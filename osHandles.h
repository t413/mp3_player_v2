#ifndef OSHANDLES_H
#define OSHANDLES_H

#include "./FreeRTOS/FreeRTOS.h"	// Includes for RTOS & Other services provided by it:
#include "./FreeRTOS/task.h"
#include "./FreeRTOS/queue.h"
#include "./FreeRTOS/semphr.h"

// A pointer to OSHANDLES is all that is needed for any file/task to access
// Semaphores, task handles, queue handles etc...
// Pointer can be passed to task as pvParameter
typedef struct
{
	struct {
		xQueueHandle playback_playlist;
		xQueueHandle mp3_control;
		xQueueHandle effect;
	}queue;

	struct {
		xTaskHandle userInterface;
		xTaskHandle ten_ms_check;
		xTaskHandle port_expander_task;
		xTaskHandle sd_card_detect;
		xTaskHandle mp3_task;
	}task;

	struct {
		xSemaphoreHandle SPI;  // spi lock
		xSemaphoreHandle I2C;  // i2c lock
	}lock;
}OSHANDLES;

#endif /* COMMONDEFS_H_ */
