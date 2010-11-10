#include "userInterface.h"

#include "../osHandles.h"
#include "../drivers/uart/uart0.h"  // uart0GetChar()
#include "../System/rprintf.h"      // rprintf
#include "../System/crash.h"        // To crash the system :)

#include <stdlib.h>                 // atoi()
#include <stdio.h>                  // printf() or iprintf()
#include <string.h>                 // strtok()

#include "../fat/ff.h"				// FAT File System Library
#include "../fat/diskio.h" 			// Disk IO Initialize SPI Mutex
#include "../drivers/sta013.h"		// STA013 Mp3 Decoder
#include "../drivers/pcm1774.h"		// PCM 1774 DAC

#include "../drivers/i2c.h"
#include "../drivers/ssp_spi.h"

#include "../System/lpc214x.h"

#include "./mp3.h"
#include "./read_id3.h"


/* ---- private declarations ---- */
void getUartLine(char* uartInput);
void dir_ls(char * dirPath, unsigned char show_hidden, unsigned char show_all);
void open_file(char * file_path);
unsigned char waitForReady(unsigned int timeoutMs);
void writeEnable();
void globalProtectionOff();
void play_spi_mp3(unsigned long, unsigned int, OSHANDLES *);

#define SELECT_FLASH_MEM IOCLR0=(1<<12)
#define DE_SELECT_FLASH_MEM IOSET0=(1<<12)
#define MP3_DATAREQ_IS_HIGH (IOPIN0 & (1<<15))



/* ---- functions ---- */
void sd_card_detect(void *pvParameters){
	unsigned char last_sd = 0;
	for(;;) {
		if (IS_SD_CARD_HERE && !last_sd) {  //if sd card is present for first time
			FATFS SDCard;        // Takes ~550 Bytes
			rprintf("mounting card..");
			FRESULT res = f_mount(0, &SDCard); // Mount the Card on the File System
			rprintf(" done w/%s..\n", (res)? "failure":"success");
			
		}
		else if (!IS_SD_CARD_HERE && last_sd) {  //if sd card is gone for first time
			rprintf("un-mounting card..");
			FRESULT res = f_mount(0, NULL); // UN-mount the Card from the File System
			rprintf(" done w/%s..\n", (res)? "failure":"success");
		}
		last_sd = IS_SD_CARD_HERE;
		vTaskDelay(100);
	}
}


void uartUI(void *pvParameters)
{
	OSHANDLES *osHandles = (OSHANDLES*)pvParameters;
	char uartInput[128];
	char current_dir[128] = "0:";

	artist_list = NULL;

	//i2c_init(400); <- moved to main();	
	
	rprintf("init sta013, ");
	initialize_sta013();

	rprintf("pcm1774\n");
	initialize_pcm1774();
	
	PINSEL0 &= ~(3<<24);  // GPIO P0.12 -> Flash Memmory
	IODIR0 |= (1<<12);    // GPIO P0.12 -> Flash Memmory
	DE_SELECT_FLASH_MEM;
	
	vTaskDelay(100);
	play_spi_mp3(0,42*1024,osHandles);
	
	for (;;)
	{
		rprintf("tim: "); //print prompt
		memset(uartInput, 0, sizeof(uartInput));  //clear buffer
		getUartLine(uartInput);  //waits for return
		if (strlen(uartInput) <= 0) continue; //be sure there's a command here.

		char *command = strtok(uartInput, " ");

		if (MATCH(command, "echo")){
			char *echoBack = strtok(NULL, "");
			rprintf("Echo: %s\n", echoBack);
		}

		/*---------miscellaneous opperations--------*/
		else if(MATCH("list", command)) {
			display_track_list(artist_list);
		}
		else if(MATCH("clear", command)) {
			clear_track_list();
		}
		

		/*---------music commands--------*/
		else if(MATCH("speed", command)) {
			rprintf("decoding speed: %i bytes/ms\n",player_status.read_speed);
		}
		else if(MATCH("pp", command)) {
			unsigned char cntl = PLAY_PAUSE;
			if(xQueueSend(osHandles->queue.mp3_control, &cntl, 100))
				rprintf(".. sent!\n");
		}
		else if(MATCH("stop", command)) {
			unsigned char cntl = STOP;
			xQueueSend(osHandles->queue.mp3_control, &cntl, 100);
		}
		else if (MATCH("next", command)) { unsigned char cntl = STOP; xQueueSend(osHandles->queue.mp3_control, &cntl, 100); }
		/*else if(MATCH("open", command)) {
			char * file_path = strtok(NULL, "");
			if (file_path[1] == ':') { //absolute URL
				printf("absolute [%s]\n", file_path);
			}
			else if (strlen(file_path) > 0){  //relative URL
				unsigned int current_dir_end = strlen(current_dir);
				current_dir[current_dir_end] = '/';
				strcpy ( &(current_dir[current_dir_end+1]) ,file_path);

				// now send the string over the queue.
				printf("sending to queue: %s\n", current_dir);
				if(xQueueSend(osHandles->queue.play_this_MP3, &(current_dir[0]), 100))
					printf("sent!\n");
				else printf("FAILED TO SEND ITEM (Timeout)!\n");
				//done sending

				current_dir[current_dir_end] = 0;  //restore current_dir
			}
		}*/

		/*---------Filesystem opperations--------*/
		else if(MATCH("cd", command)) {
			char * file_path = strtok(NULL, "");

			if (file_path[1] == ':') { //absolute URL
				strcpy( current_dir, file_path);
				printf("(abs) ");
			}
			else if (file_path == NULL){ //no path.. go home (root)
				strcpy( current_dir, "0:");
				printf("(~) ");
			}
			else {  //relative URL
				unsigned int current_dir_end = strlen(current_dir);
				current_dir[current_dir_end] = '/';
				strcpy ( &(current_dir[current_dir_end+1]) ,file_path);
				printf("(rel) ");
			}
			printf("current_dir is now: [%s]\n", current_dir);
		}
		else if(MATCH("pwd", command)) { rprintf("%s\n",current_dir); }
		else if(MATCH("scan", command)) {
			scan_root();
		}
		else if(MATCH("ls", command)) {
			char * dir_path = strtok(NULL, "");
			dir_ls( (dir_path == NULL)? current_dir : dir_path,FALSE,FALSE);
		}
		else if(MATCH("ls-l", command)) { char * dir_path = strtok(NULL, ""); dir_ls( (dir_path == NULL)? current_dir : dir_path,FALSE,TRUE); }
		else if(MATCH(command, "read")) {
			char * file_path = strtok(NULL, "");
			if (file_path[1] == ':') { //absolute URL
				open_file(file_path);
			}
			else if (strlen(file_path) > 0){  //relative URL
				unsigned int current_dir_end = strlen(current_dir);
				current_dir[current_dir_end] = '/';
				strcpy ( &(current_dir[current_dir_end+1]) ,file_path);
				open_file(current_dir);
				current_dir[current_dir_end] = 0;  //restore current_dir
			}
		}

		/*---------cpu info opperations--------*/
		else if (MATCH(command, "CRASH")) {
			char *crashType = strtok(NULL, "");

			if(MATCH(crashType, "UNDEF"))		performUndefinedInstructionCrash();
			else if(MATCH(crashType, "PABORT"))	performPABORTCrash();
			else if(MATCH(crashType, "DABORT"))	performDABORTCrash();
			else rprintf("Define the crash type as either UNDEF, PABORT, or DABORT\n");
		}
		#if configGENERATE_RUN_TIME_STATS
		else if(MATCH(command, "CPU")) {
			 vTaskGetRunTimeStats(uartInput, 0);
			rprintf("%s", uartInput);
		}
		else if(MATCH(command, "CPUr")) {
			vTaskGetRunTimeStats(uartInput, 1);
			rprintf("%s", uartInput);
		}
		else if(MATCH(command, "CPUn")) {
			int delayTimeMs = atoi(strtok(NULL, ""));
			if(delayTimeMs > 0) {
				rprintf("Delaying for %ims.  CPU Usage will be reported afterwards...\n", delayTimeMs);
				vTaskGetRunTimeStats(uartInput, 1);
				vTaskDelay(OS_MS(delayTimeMs));
				vTaskGetRunTimeStats(uartInput, 1);
				rprintf("%s", uartInput);
			}
			else {
				rprintf("You must define a delay time in milliseconds as parameter.\n");
			}
		}
		#endif
		#if INCLUDE_uxTaskGetStackHighWaterMark
		else if(MATCH(command, "watermark")) {
			rprintf("High watermarks (closer to zero = bad)\n");
			rprintf("userInterface : % 5i\n", 4*uxTaskGetStackHighWaterMark(osHandles->task.userInterface));
			rprintf("ten_ms_check : % 5i\n", 4*uxTaskGetStackHighWaterMark(osHandles->task.ten_ms_check));
			rprintf("port_expander_task : % 5i\n", 4*uxTaskGetStackHighWaterMark(osHandles->task.port_expander_task));
			rprintf("sd_card_detect : % 5i\n", 4*uxTaskGetStackHighWaterMark(osHandles->task.sd_card_detect));
			rprintf("mp3_task : % 5i\n", 4*uxTaskGetStackHighWaterMark(osHandles->task.mp3_task));
		}
		#endif
		else if (MATCH("HELP", command)) {
			rprintf("Command list:\n fix me!");
		}
		/*---------spi flash memory--------*/
		else if (MATCH("flash-id", command)) {
			if (xSemaphoreTake( osHandles->lock.SPI, 100)){
				SELECT_FLASH_MEM; //select flash (active low)
				rxTxByteSSPSPI(0x9f);
				unsigned char id = rxTxByteSSPSPI(0x00);
				unsigned char part1 = rxTxByteSSPSPI(0xFF);
				unsigned char part2 = rxTxByteSSPSPI(0xFF);
				DE_SELECT_FLASH_MEM; //deselect flash (active low)
				rprintf("Flash module manufacturer ID=%02X, DeviceID.1=%02X, DeviceID.2=%02X\n",id, part1,part2);
				xSemaphoreGive( osHandles->lock.SPI );
			}
		}
		else if (MATCH("flash-erase", command)) {
			if (xSemaphoreTake( osHandles->lock.SPI, 100)){
				if (waitForReady(500)){ //wait for the memory to be ready
					unsigned long address = 0x00;
					globalProtectionOff();
					writeEnable();
					SELECT_FLASH_MEM; //select flash (active low)
					rxTxByteSSPSPI(0xD8);
					rxTxByteSSPSPI( (address >> 16) & 0xFF);
					rxTxByteSSPSPI( (address >>  8) & 0xFF);
					rxTxByteSSPSPI( (address >>  0) & 0xFF);
					DE_SELECT_FLASH_MEM; //deselect flash (active low)
					unsigned char resu = waitForReady(2000); //wait for the memory to be ready
					rprintf(resu?"erased successfully.\n":"error erasing.\n");
				} else rprintf("timeout error\n");
				xSemaphoreGive( osHandles->lock.SPI );
			}
		}
		else if (MATCH("flash-read", command)) {
			unsigned long address = atoi(strtok(NULL, "")); //first space seperated argument
			if (xSemaphoreTake( osHandles->lock.SPI, 100)){
				if (waitForReady(500)){ //wait for the memory to be ready
					SELECT_FLASH_MEM; //select flash (active low)
					rxTxByteSSPSPI(0x0B);
					rxTxByteSSPSPI( (address >> 16) & 0xFF);
					rxTxByteSSPSPI( (address >>  8) & 0xFF);
					rxTxByteSSPSPI( (address >>  0) & 0xFF);
					rxTxByteSSPSPI(0x00);
					unsigned int i = 0;
					while (1){
						if (i > 1000) break;
						char tmp = rxTxByteSSPSPI(0x00);
						//if ((tmp == 0x00)||(tmp == 0xFF)) break;
						rprintf("%02X ",tmp);
						if (!((i++)%6)) rprintf("\n");
					}
					
					DE_SELECT_FLASH_MEM; //deselect flash (active low)
				} else rprintf("timeout error\n");
				xSemaphoreGive( osHandles->lock.SPI );
			}
		}
		else if (MATCH("flash-play", command)) {
			unsigned long address = atoi(strtok(NULL, "")); //first space seperated argument
			address &= ~(0xFF);  //be sure address has even 256-byte page boundary
			play_spi_mp3(address,42*1024,osHandles);
		}

		else if (MATCH("flash-write", command)) {
			char *write_str = strtok(NULL, "");
			if (xSemaphoreTake( osHandles->lock.SPI, 100)){
				if (waitForReady(500)){ //wait for the memory to be ready
					unsigned long address = 0x00;
					globalProtectionOff();
					writeEnable();
					SELECT_FLASH_MEM;
					rxTxByteSSPSPI(0x02);
					rxTxByteSSPSPI( (address >> 16) & 0xFF);
					rxTxByteSSPSPI( (address >>  8) & 0xFF);
					rxTxByteSSPSPI( (address >>  0) & 0xFF);
					unsigned int i = 0;
					do { rxTxByteSSPSPI( write_str[i] ); } 
					while (write_str[i++] != 0x00);
					DE_SELECT_FLASH_MEM;
					waitForReady(1000);
				} else rprintf("timeout error\n");
				xSemaphoreGive( osHandles->lock.SPI );
			}
		}
		else if (MATCH("flash-load", command)) {
			unsigned long address = atoi(strtok(NULL, " ")); //first space seperated argument
			address &= ~(0xFF);  //be sure address has even 256-byte page boundary
			char *file_path = strtok(NULL, ""); //file path.
			if (strlen(file_path) > 0){  //relative URL
				rprintf(" (using starting address 0x%02x-%02x-%02x)\n",(address>>16)&0xFF,(address>>8)&0xFF,address&0xFF);
				rprintf(" given file: [%s]\n",file_path);
				
				unsigned int current_dir_end = strlen(current_dir);
				current_dir[current_dir_end] = '/';
				strcpy ( &(current_dir[current_dir_end+1]) ,file_path);
				rprintf(" using path: [%s]\n",current_dir);
			
				if (xSemaphoreTake( osHandles->lock.SPI, 100) && waitForReady(500)){
					globalProtectionOff();
					writeEnable();
					xSemaphoreGive( osHandles->lock.SPI );

					FIL file;
					FRESULT rstat = f_open(&file, current_dir, (FA_READ | FA_OPEN_EXISTING));
					if (rstat == FR_OK) {
						char buff[256];
						unsigned int bytesRead = 0;
						unsigned int total_bytes_read = 0;
						while (1) {
							f_read(&file, buff, sizeof(buff), &bytesRead);
							//rprintf("read %i bytes, writing to address 0x%02x-%02x-%02x)\n",bytesRead,(address>>16)&0xFF,(address>>8)&0xFF,address&0xFF);
							if (xSemaphoreTake( osHandles->lock.SPI, 100) && waitForReady(100)){
								writeEnable();
								SELECT_FLASH_MEM;
								rxTxByteSSPSPI(0x02);
								rxTxByteSSPSPI( (address >> 16) & 0xFF);
								rxTxByteSSPSPI( (address >>  8) & 0xFF);
								rxTxByteSSPSPI( (address >>  0) & 0xFF);
								unsigned int i = 0;
								while (i < bytesRead){
									rxTxByteSSPSPI( buff[i++] );
								}
								DE_SELECT_FLASH_MEM;
								address += bytesRead;
								xSemaphoreGive( osHandles->lock.SPI );
								total_bytes_read += bytesRead;
							}
							if (bytesRead < sizeof(buff)) break; //stops at end of file
						}
						rprintf("transfered %i KBs\n",total_bytes_read/1000);
						f_close(&file);
					}
				}
				current_dir[current_dir_end] = 0;  //restore current_dir
			} else rprintf("error, try: [flash-load 00 hi.mp3]\n");
		}
		else if (MATCH("flash-scan", command)) {
			if (xSemaphoreTake( osHandles->lock.SPI, 100)){
				unsigned long startAddress = 0x00, finishAddress = 0x07FFFF;
				SELECT_FLASH_MEM; //select flash (active low)
				rxTxByteSSPSPI(0x0B);
				//starting address location (3 bytes) of first byte to read
				rxTxByteSSPSPI( (startAddress >> 16) & 0xFF);
				rxTxByteSSPSPI( (startAddress >>  8) & 0xFF);
				rxTxByteSSPSPI( (startAddress >>  0) & 0xFF);
				rxTxByteSSPSPI(0x00); //when using read opcode 0Bh, 1 don't care byte is required
				
				int i = 0, lastFind = 0, numFound = 0;
				while ( (startAddress+i) < finishAddress ){
					unsigned char readByte = rxTxByteSSPSPI(0x00);
					
					if (readByte != 0xFF){
						if (!lastFind) { //leading edge triggered
							rprintf("at %i (0x%02x-%02x-%02x)",(startAddress+i),((startAddress+i)>>16)&0xFF,((startAddress+i)>>8)&0xFF,(startAddress+i)&0xFF);
							numFound = 0;
						}
						++numFound;
					}
					else if (lastFind) { // falling edge triggered
						rprintf(" found %i bytes.\n",numFound);
						numFound = 0;
					}
					lastFind = (readByte != 0xFF);
					++i;
				}
				DE_SELECT_FLASH_MEM; //deselect flash (active low)
				
				rprintf("\ndone.\n");
				xSemaphoreGive( osHandles->lock.SPI );
			}
		}
		else {
			rprintf("Command not recognized.\n");
		}
	}
}

void getUartLine(char* uartInput)
{
	char data;
	unsigned int uartInputPtr = 0;

	while (1)
	{
		while(!uart0GetChar(&data, portMAX_DELAY));

		if (data == '\n'){
			uartInput[uartInputPtr] = 0;
			uart0PutChar('\n', 100);
			break;
		}
		else if (data == '\r'){
			// Ignore it
		}
		else if (data == '\b')
		{
			if (uartInputPtr > 0){
				uartInputPtr--;
				rprintf("\b \b");
			}
			else {					// Nothing to backspace
				uart0PutChar(7, 0);	// ASCII Char 7 is for bell
			}
		}
		/*
		// If 1st char is +/- then call it quit
		else if(uartInputPtr == 0 && (data == '+' || data == '-')) {
			uartInput[0] = data;
			uartInput[1] = '\0';
			break;
		}
		*/
		else{
			uart0PutChar(data, 100);
			uartInput[uartInputPtr++] = data;
		}
	}
}

void open_file(char * filename)
{
	rprintf("File to read: |%s|\n", filename);

	FIL file;
	FRESULT rstat = f_open(&file, filename, (FA_READ | FA_OPEN_EXISTING));
	if (rstat != FR_OK) {
		rprintf("open error #%i (0x%02X).\n",rstat,rstat);
		return;
	}
	char buff[513];
	const unsigned int bytesToRead = sizeof(buff);
	rprintf("bytesToRead == %i",bytesToRead);

	unsigned int bytesRead = 0;
	do {
		f_read(&file, buff, bytesToRead, &bytesRead);
		buff[bytesRead]=0;
		rprintf("%s",buff);
	}while(bytesRead == bytesToRead);
	f_close(&file);
	rprintf("<EOF>\n");
}

void dir_ls(char * dirPath, unsigned char show_hidden, unsigned char show_all)
{
	DIR Dir;
	FILINFO Finfo;
	//FATFS *fs;
	FRESULT returnCode = FR_OK;

	unsigned int fileBytesTotal, numFiles, numDirs;
	fileBytesTotal = numFiles = numDirs = 0;
	#if _USE_LFN
	char Lfname[60];
	#endif

	//char dirPath[] = "0:";
	if (RES_OK != (returnCode = f_opendir(&Dir, dirPath))) {
		rprintf("Invalid directory: |%s| code %i\n", dirPath,returnCode);
		return;
	}
	for (;;) {
		#if _USE_LFN
		Finfo.lfname = Lfname;
		Finfo.lfsize = sizeof(Lfname);
		#endif

		returnCode = f_readdir(&Dir, &Finfo);
		if ( (FR_OK != returnCode) || !Finfo.fname[0])
			break;
		if ( show_hidden || (!(Finfo.fattrib & AM_HID) && (Finfo.fname[0] != '.'))) {
			if (Finfo.fattrib & AM_DIR) { numDirs++; }
			else{ numFiles++; fileBytesTotal += Finfo.fsize; }
			if (show_all) {
				iprintf("%c%c%c%c%c %u/%2u/%2u %2u:%2u %10lu %13s",
						(Finfo.fattrib & AM_DIR) ? 'D' : '-',
						(Finfo.fattrib & AM_RDO) ? 'R' : '-',
						(Finfo.fattrib & AM_HID) ? 'H' : '-',
						(Finfo.fattrib & AM_SYS) ? 'S' : '-',
						(Finfo.fattrib & AM_ARC) ? 'A' : '-',
						(Finfo.fdate >> 9) + 1980, (Finfo.fdate >> 5) & 15, Finfo.fdate & 31,
						(Finfo.ftime >> 11), (Finfo.ftime >> 5) & 63, Finfo.fsize, &(Finfo.fname[0]));
				#if _USE_LFN
				iprintf(" -- %s", Lfname);
				#endif
				iprintf("\n");
			}
			else {
				rprintf("%s \t", &(Finfo.fname[0]) );
				rprintf( ((numDirs+numFiles)%2)? "\t":"\n" );
			}
		}
	}
	if (show_all) iprintf("\n%4u File(s), %10u bytes total\n%4u Dir(s)", numFiles, fileBytesTotal, numDirs);
	else rprintf("\n");
	//if (f_getfree(uartInput, (DWORD*) &fileBytesTotal, &fs) == FR_OK)
	//{
	//	iprintf(", %10uK bytes free\n", fileBytesTotal * fs->csize / 2);
	//}
}

void writeEnable(){
	//TURN OFF GLOBAL PROTECTION!!!
	//The WEL bit must be set before a program, erase, Protect Sector, 
	// Unprotect Sector, or Write Status Register command can be executed.
	SELECT_FLASH_MEM; //select flash (active low)
	rxTxByteSSPSPI(0x06);
	DE_SELECT_FLASH_MEM; //deselect flash (active low)
	DE_SELECT_FLASH_MEM; //deselect flash (active low)
}

void globalProtectionOff(){
	writeEnable();
	SELECT_FLASH_MEM; //select flash (active low)
	rxTxByteSSPSPI(0x01);
	rxTxByteSSPSPI(0x00); //Global Unprotect – all Sector Protection Registers reset to 0
	DE_SELECT_FLASH_MEM; //deselect flash (active low)
	DE_SELECT_FLASH_MEM; //deselect flash (active low)
}


unsigned char waitForReady(unsigned int timeoutMs){
	//now wait for the opperation to complete
	SELECT_FLASH_MEM; //select flash (active low)
	rxTxByteSSPSPI(0x05);//send read status register
	while( rxTxByteSSPSPI(0x00)&0x01 ){ //wait for not busy
		if (timeoutMs-- <= 0) return 0; //return timed-out.
		vTaskDelay(1);  //delay 1 ms..
	}
	DE_SELECT_FLASH_MEM; //select flash (active low)
	return 1; //success
}

void play_spi_mp3(unsigned long address, unsigned int length, OSHANDLES * osHandles){	
	unsigned char buff[128];
	unsigned int chunks_to_read = length/sizeof(buff);
	while (chunks_to_read--){
		unsigned int bytesRead = 0;
		if (! xSemaphoreTake( osHandles->lock.SPI, 100)) return;
		if (! waitForReady(500)) return;
		SELECT_FLASH_MEM; //select flash (active low)
		rxTxByteSSPSPI(0x0B);
		rxTxByteSSPSPI( (address >> 16) & 0xFF);
		rxTxByteSSPSPI( (address >>  8) & 0xFF);
		rxTxByteSSPSPI( (address >>  0) & 0xFF);
		rxTxByteSSPSPI(0x00);
		while (bytesRead < sizeof(buff)){
			buff[bytesRead++] = rxTxByteSSPSPI(0x00);
			buff[bytesRead++] = rxTxByteSSPSPI(0x00);
			buff[bytesRead++] = rxTxByteSSPSPI(0x00);
			buff[bytesRead++] = rxTxByteSSPSPI(0x00);
			//if ((buff[bytesRead-4] == 0xFF)&&(buff[bytesRead-3] == 0xFF)&&(buff[bytesRead-2] == 0xFF)&&(buff[bytesRead-1] == 0xFF)) break;
		}
		DE_SELECT_FLASH_MEM; //deselect flash (active low)
		address += bytesRead;
		xSemaphoreGive( osHandles->lock.SPI );
		//rprintf("at[%i], read %i bytes\n",chunks_to_read,bytesRead);
		int bytes_played = 0;
		while( bytes_played < bytesRead) {
			if (MP3_DATAREQ_IS_HIGH) {
				if (xSemaphoreTake( osHandles->lock.SPI, 100)){
					IOSET1 = (1<<29);  //MP3-decoder CS => active high
					rxTxByteSSPSPI(buff[bytes_played++]);
					IOCLR1 = (1<<29);  //MP3-decoder CS
					xSemaphoreGive( osHandles->lock.SPI );
				}
			}
		}
		if(sizeof(buff) > bytesRead){ // Last Chunk in file
			rprintf("hit > break\n");
			break; // Break outer loop, song ended
		}
		
	}
}


