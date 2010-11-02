/*
 * mp3.c
 *
 *  Created on: Oct 26, 2010
 *      Author: timo
 */
#include "../FreeRTOS/FreeRTOS.h"	// Includes for RTOS & Other services provided by it:
#include "../FreeRTOS/task.h"
#include "../FreeRTOS/queue.h"
#include "../FreeRTOS/semphr.h"
#include "../osHandles.h"
#include "../fat/ff.h"				// FAT File System Library
#include "../fat/diskio.h" 			// Disk IO Initialize SPI Mutex
#include "../drivers/uart/uart0.h"  // uart0GetChar()
#include "../System/rprintf.h"      // rprintf
#include "./read_id3.h"
#include <string.h>
#include "mp3.h"


#define MP3_DATAREQ_IS_HIGH (IOPIN0 & (1<<15))
#define min(a,b) ((a>b)?(b):(a))


void mp3_task(void *pvParameters) {
	OSHANDLES *osHandles = (OSHANDLES*)pvParameters; // Cast the void pointer to pointer of OSHANDLES
		
	player_status.has_song = 0;
	player_status.playing = 0;
	
	for(;;)
	{
		char file_name[128];
		if (xQueueReceive(osHandles->queue.play_this_MP3, &file_name[0],999999999)) {
			file_name[128-1] = 0; //just to be sure there's a null terminator.
			//rprintf("queue got: [%s]",file_name);
			
			/* ---- got a filename, let's open the file. ---- */
			FIL file;
			FRESULT rstat = f_open(&file, file_name, (FA_READ | FA_OPEN_EXISTING));
			if (rstat != FR_OK) {
				rprintf("open error #%i (0x%02X).\n",rstat,rstat);
			}
			// TODO: add error checking -> check for .mp3 extention
			else {
				/* ---- file opened successfully ---- */
				//FRESULT returnCode = f_readdir(&Dir, &Finfo);
				//iprintf("reading %13s (%10lu kBytes)\n", &(Finfo.fname[0]), Finfo.fsize/1000 );
				player_status.has_song = 1;
				player_status.playing = 1;
				//player_status.legnth = Finfo.fsize;
				char tag[50];
				read_ID3_info(TITLE_ID3,tag,sizeof(tag),&file);
				rprintf("Playing: %s",tag);
				read_ID3_info(ARTIST_ID3,tag,sizeof(tag),&file);
				rprintf(" by %s\n",tag);
				
				while (1){
					/* ---- check about the control queue ---- */
					unsigned char cntl = 0;
					xQueueReceive(osHandles->queue.mp3_control, &cntl,0);
					if ((cntl == PAUSE) || (cntl == PLAY_PAUSE)) {
						player_status.playing = 0;
						while (1){
							if (xQueueReceive(osHandles->queue.mp3_control, &cntl,1000*60*5)){
								if ((cntl == RESUME) || (cntl == PLAY_PAUSE) || (cntl == STOP)) break;
							}
							else {cntl = STOP; break;} //stop if past timeout (5 mins)
						}
					}
					if (cntl == STOP) break;
					
					/* ---- read a chunk of data to the buffer ---- */
					player_status.playing = 1;
					char buff[4096];
					unsigned int bytesRead = 0;
					unsigned int start_ms = xTaskGetTickCount();
					f_read(&file, buff, sizeof(buff), &bytesRead);
					player_status.read_speed = bytesRead/(xTaskGetTickCount()-start_ms);

					//rprintf("read %i bytes\n",bytesRead);
					
					/* ---- send chunked data to the mp3 decoder ---- */
					int i = 0;
					while( i < bytesRead) {
						if (MP3_DATAREQ_IS_HIGH) {
							if (xSemaphoreTake( osHandles->lock.SPI, 100)){
								IOSET1 = (1<<29);  //MP3-decoder CS => active high
								rxTxByteSSPSPI(buff[i++]);
								IOCLR1 = (1<<29);  //MP3-decoder CS
								xSemaphoreGive( osHandles->lock.SPI );
							}
						}
					}
					// TODO: change 4094 to 4095 or whatever is _correct_.
					if(4094 > bytesRead){ // Last Chunk in file
						player_status.has_song = 0;
						player_status.playing = 0;
						break; // Break outer loop, song ended
					}
				}
				f_close(&file);
			}
		}
	}
}


