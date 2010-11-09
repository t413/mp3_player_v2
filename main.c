
#include "./osHandles.h"                // Includes all OS Files
#include "./System/cpu.h"               // Initialize CPU Hardware

#include "./System/crash.h"             // Detect exception (Reset)
#include "./System/watchdog.h"
#include "./drivers/uart/uart0.h"       // Initialize UART
#include "./System/rprintf.h"			// Reduced printf.  STDIO uses lot of FLASH space & Stack space
#include "./drivers/i2c.h"
#include "./fat/diskio.h"

//task files
#include "./general/userInterface.h"	// User interface functions to interact through UART
#include "./General/mp3.h"
#include "./General/port_expander.h"

void ten_ms_check(void *pvParameters);

/* INTERRUPT VECTORS:
 * 0:    OS Timer Tick
 * 1:    Not Used
 * 2:    UART Interrupt
 * 3:    Not Used
 * 4:    Not Used
 * 5:    i2c interrupt
 * 6-16: Not Used
 */

/* Resources Used
 * 1.  TIMER0 FOR OS Interrupt
 * 2.  TIMER1 FOR Run-time Stats (can be disabled at FreeRTOSConfig.h)
 * 3.  Watchdog for timed delay functions (No CPU Reset or timer)
 */

int main (void)
{
	OSHANDLES System;            // Should contain all OS Handles

	cpuSetupHardware();          // Setup PLL, enable MAM etc.
	watchdogDelayUs(2000*1000);  // Some startup delay
	uart0Init(38400, 256);       // 256 is size of Rx/Tx FIFO

	// Use polling version of uart0 to do printf/rprintf before starting FreeRTOS
	rprintf_devopen(uart0PutCharPolling);
	if(didSystemCrash())
	{
		printCrashInfo();
		clearCrashInfo();
	}
	cpuPrintMemoryInfo();

	// Open interrupt-driven version of UART0 Rx/Tx
	rprintf_devopen(uart0PutChar);

	System.lock.SPI = xQueueCreateMutex();
	System.lock.I2C = xQueueCreateMutex();

	System.queue.playback_playlist = xQueueCreate(1,sizeof(Track *)); //send a playlist to play.
	System.queue.mp3_control = xQueueCreate(1, sizeof(unsigned char));
	System.queue.effect = xQueueCreate(3, sizeof(unsigned char));

	i2c_init(400);
	initialize_SSPSPI();
	diskio_initializeSPIMutex(&(System.lock.SPI));
	initialize_SdCardSignals();

	xTaskCreate( sd_card_detect, (signed char*)"sd_card_detect", STACK_BYTES(1024), &System, PRIORITY_LOW, &System.task.sd_card_detect );
	xTaskCreate( uartUI, (signed char*)"userInterface", STACK_BYTES(1024*4), &System, PRIORITY_MEDIUM,  &System.task.userInterface );
	xTaskCreate( ten_ms_check, (signed char*)"10ms", STACK_BYTES(256), &System, PRIORITY_MEDIUM, &System.task.ten_ms_check );
	xTaskCreate( port_expander_task, (signed char*)"port-expander", STACK_BYTES(512), &System, PRIORITY_LOW, &System.task.port_expander_task );
	xTaskCreate( mp3_task, (signed char*)"mp3 task", STACK_BYTES(1024*8), &System, PRIORITY_LOW, &System.task.mp3_task );

	rprintf("\n-- Starting FreeRTOS --\n");
	vTaskStartScheduler();	// This function will not return.

	rprintf_devopen(uart0PutCharPolling);
	rprintf("ERROR: Unexpected OS Exit!\n");
	return 0;
}

void ten_ms_check(void *pvParameters) {
	for(;;) {
		disk_timerproc();
		vTaskDelay(10);
	}
}
