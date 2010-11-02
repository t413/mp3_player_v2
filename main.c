
#include "./osHandles.h"                // Includes all OS Files
#include "./System/cpu.h"               // Initialize CPU Hardware

#include "./System/crash.h"             // Detect exception (Reset)
#include "./System/watchdog.h"
#include "./drivers/uart/uart0.h"       // Initialize UART
#include "./System/rprintf.h"			// Reduced printf.  STDIO uses lot of FLASH space & Stack space
#include "./general/userInterface.h"	// User interface functions to interact through UART



/* INTERRUPT VECTORS:
 * 0:    OS Timer Tick
 * 1:    Not Used
 * 2:    UART Interrupt
 * 3:    Not Used
 * 4-16: Not Used
 */

/* Resources Used
 * 1.  TIMER0 FOR OS Interrupt
 * 2.  TIMER1 FOR Run-time Stats (can be disabled at FreeRTOSConfig.h)
 * 3.  Watchdog for timed delay functions (No CPU Reset or timer)
 */
#include "./fat/diskio.h"
void diskTimer()
{
	for(;;)
	{
		vTaskDelay(10);
		disk_timerproc();
	}
}

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

	// TODO: 1a. (Done) Create SPI Mutex Here:
	System.lock.SPI = xSemaphoreCreateMutex();

	// TODO: 1b. (Done) Create a function that calls disk_timerproc() every 10ms:
	// This is nothing but a task function with forever loop, with disk_timerproc() and vTaskDelay(10);

	// Use the WATERMARK command to determine optimal Stack size of each task (set to high, then slowly decrease)
	// Priorities should be set according to response required
	if (
		// User Interaction set to lowest priority.
		pdPASS != xTaskCreate( uartUI, (signed char*)"Uart UI", STACK_BYTES(1024*6), &System, PRIORITY_LOW,  &System.task.userInterface )
		||
		// diskTimer should always run, and it is a short function, so assign CRITIAL priority.
		pdPASS != xTaskCreate( diskTimer, (signed char*)"Dtimer", STACK_BYTES(256), &System, PRIORITY_CRITICAL,  &System.task.diskTimer )
	){
		rprintf("ERROR:  OUT OF MEMORY: Check OS Stack Size or task stack size.\n");
	}

	rprintf("\n-- Starting FreeRTOS --\n");
	vTaskStartScheduler();	// This function will not return.

	rprintf_devopen(uart0PutCharPolling);
	rprintf("ERROR: Unexpected OS Exit!\n");
	return 0;
}

