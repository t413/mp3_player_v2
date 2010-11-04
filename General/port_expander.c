/*
 * port_expander.c
 *
 *  Created on: Oct 26, 2010
 *      Author: timo
 */

#include "port_expander.h"

#include "../osHandles.h"
#include "../drivers/i2c.h"
#include "../fat/ff.h"				// FAT File System Library
#include "../fat/diskio.h" 			// Disk IO Initialize SPI Mutex
#include "../drivers/sta013.h"		// STA013 Mp3 Decoder
#include "../drivers/pcm1774.h"		// PCM 1774 DAC
#include "./mp3.h"

#include "../drivers/uart/uart0.h"  // uart0GetChar()
#include "../System/rprintf.h"      // rprintf

/* ---- defines ---- */
#define PORT_EXPANDER_ADDRESS 0x40
#define PE_READWRITE_TIMEOUT 50

#define BUTTON_0 (1<<0)
#define BUTTON_1 (1<<1)
#define BUTTON_2 (1<<2)
#define BUTTON_3 (1<<3)
#define BUTTON_4 (1<<4)
#define BUTTON_5 (1<<5)
#define BUTTON_6 (1<<6)
#define BUTTON_7 (1<<7)

#define min(a,b) ((a>b)?(b):(a))
#define max(a,b) ((a>b)?(a):(b))

/* ---- private functions ---- */
unsigned char readButtons(OSHANDLES *osHandles);
void writeLEDs(unsigned char effect, OSHANDLES *osHandles);


/* ---- public functions ---- */

void port_expander_task(void *pvParameters) {
	OSHANDLES *osHandles = (OSHANDLES*)pvParameters; // Cast the void pointer to pointer of OSHANDLES
	
	if(xSemaphoreTake( osHandles->lock.I2C, 50)){  //take i2c lock.
		//setup the Port Expander
		i2c_send_byte(0x40,0x02,0x00); //turn off leds initially
		i2c_send_byte(0x40,0x06,0x00); //set leds as output.
		xSemaphoreGive( osHandles->lock.I2C );
	}
	
	//unsigned int period = 100;
	unsigned char eButt = 0;
	unsigned int i = 0;
	unsigned char last_buttons = 0, out_volume = 20;
	for(;;i++)
	{
		unsigned char innertask_comm;
		if(xQueueReceive(osHandles->queue.effect, &innertask_comm, 2)){
			eButt = innertask_comm;
		}
		
		if (xSemaphoreTake( osHandles->lock.I2C, PE_READWRITE_TIMEOUT)) {  //take i2c lock.
			unsigned char readButtons = i2c_receive_byte(PORT_EXPANDER_ADDRESS,0x01);
			xSemaphoreGive( osHandles->lock.I2C );
		
			/* ---- leading edge triggered ---- */
			// PLAY-PAUSE
			if ((readButtons & BUTTON_0) && (last_buttons==0)) {
				unsigned char cntl = PLAY_PAUSE;
				xQueueSend(osHandles->queue.mp3_control, &cntl, 1000);
			}
			// STOP
			else if ((readButtons & BUTTON_1) && (last_buttons==0)) {
				unsigned int num_queued = uxQueueMessagesWaiting(osHandles->queue.play_this_MP3);
				while (num_queued--) {
					char file_name[128];
					xQueueReceive(osHandles->queue.play_this_MP3, &file_name[0],0);
				}
				unsigned char cntl = STOP;
				xQueueSend(osHandles->queue.mp3_control, &cntl, 1000);
			}
			// NEXT
			if ((readButtons & BUTTON_2) && (last_buttons==0)) {
				unsigned char cntl = STOP;
				xQueueSend(osHandles->queue.mp3_control, &cntl, 1000);
			}
			// VOLUME UP
			if ((readButtons & BUTTON_3) && (last_buttons==0)) {
				out_volume = min(100, out_volume+10);  // Scale 0-100
				pcm1774_OutputVolume(out_volume, out_volume);
			}
			// VOLUME DOWN
			if ((readButtons & BUTTON_4) && (last_buttons==0)) {
				out_volume = max(0, out_volume-10);  // Scale 0-100
				pcm1774_OutputVolume(out_volume, out_volume);
			}
			// Seek Foreward
			if ((readButtons & BUTTON_5) && (readButtons & BUTTON_3) && (last_buttons==0)) {
				unsigned char cntl = SEEK_F_8X;
				xQueueSend(osHandles->queue.mp3_control, &cntl, 50);
			}
			// Seek back
			if ((readButtons & BUTTON_5) && (readButtons & BUTTON_4) && (last_buttons==0)) {
				unsigned char cntl = SEEK_R_8X;
				xQueueSend(osHandles->queue.mp3_control, &cntl, 50);
			}
			if ((last_buttons & BUTTON_5) && (readButtons==0)) {
				unsigned char cntl = RESUME;
				xQueueSend(osHandles->queue.mp3_control, &cntl, 50);
			}
			
			/* ---- trailing edge triggered ---- */
			// load a song
			if ((last_buttons & BUTTON_7) && (readButtons==0)) {
				char songnum = (last_buttons & ~BUTTON_7) + 0x30;
				unsigned char name[] = "0:/Music/mike/_.mp3";
				name[14] = songnum;
				rprintf("but sending %s\n",name);
				xQueueSend(osHandles->queue.play_this_MP3, &(name[0]), 100);
			}
			if ((last_buttons & BUTTON_6) && (readButtons==0)) {
				char songnum = (last_buttons & ~BUTTON_6) + 0x30;
				unsigned char name[] = "0:/Music/us/_.mp3";
				name[12] = songnum;
				rprintf("but sending %s\n",name);
				xQueueSend(osHandles->queue.play_this_MP3, &(name[0]), 100);
			}
			
			//write to the LEDS
			unsigned char effect = readButtons;
			if ((player_status.playing) && (last_buttons == 0) && (readButtons == 0)){
				if ((i/8)%2) effect = 1<<((i)%8);
				else effect = 1<<(6-(i)%8+1);
			}
			
			if(xSemaphoreTake( osHandles->lock.I2C, PE_READWRITE_TIMEOUT)){  //take i2c lock.
				i2c_send_byte(PORT_EXPANDER_ADDRESS,0x02,effect);
				xSemaphoreGive( osHandles->lock.I2C );
			}
		
			last_buttons = readButtons;
		}
		/*
		{
			// create effects based on last button imput that wasn't 0
			if (readButtons != 0) eButt = readButtons;
			unsigned char effect;
			if ((!eButt) || (eButt & 1)){   // button: (-------#)
				effect = 0; period = 100;
			}
			if (eButt & (7<<1)){   // buttons: (----###-)
				period = (eButt&(1<<1))?150:(eButt&(1<<2))?70:20;
				if ((i/8)%2) effect = 1<<((i)%8);
				else effect = 1<<(6-(i)%8+1);
			}
			else if (eButt & (7<<4)){   // buttons: (-###----)
				period = (eButt&(1<<4))?150:(eButt&(1<<5))?70:20;
				if ((i/8)%2) effect = (1<<((i)%8+1))-1;
				else effect = (1<<(5-(i)%8+2))-1;
			}
			else if (eButt & (1<<7)){   // button: (#-------)
				effect = 24; period = 100;
			}
			else effect = 0;
			
			if (effect || (eButt & 1)) i2c_send_byte(0x40,0x02,effect);
			
			xSemaphoreGive( osHandles->lock.I2C );
		}
		 */
		//vTaskDelay(period);
		vTaskDelay(100);

	}
}

