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

#include "./mp3.h"
#include "./read_id3.h"


/* ---- private declarations ---- */
void getUartLine(char* uartInput);
void dir_ls(char * dirPath, unsigned char show_hidden, unsigned char show_all);
void open_file(char * file_path);



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
	
	rprintf("init sta013\n");
	initialize_sta013();

	rprintf("pcm1774\n");
	initialize_pcm1774();

	vTaskDelay(100);
	scan_root();  //scan/populate database of artist & their albums.

	artist_list->tracks = bubblesort(artist_list->tracks);
	
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
		else if(MATCH("effect", command)) {
			char * effect_str = strtok(NULL, "");
			unsigned char effect_int = atoi(effect_str);
			rprintf("sending effect 0x%02X\n",effect_int);
			if(xQueueSend(osHandles->queue.effect, &effect_int, 100))
				rprintf(".. sent!\n");
		}
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
			/*unsigned int num_queued = uxQueueMessagesWaiting(osHandles->queue.play_this_MP3);
			rprintf(" %i queued, removing each.\n",num_queued);
			while (num_queued--) {
				char file_name[128];
				xQueueReceive(osHandles->queue.play_this_MP3, &file_name[0],0);
			}*/
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




