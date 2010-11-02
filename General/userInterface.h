
#ifndef USERINTERFACE_H_
#define USERINTERFACE_H_

#define MATCH(a,b)		(strcmp(a,b) == 0)

void getUartLine(char* uartInput);
void uartUI(void *pvParameters);

#endif /* USERINTERFACE_H_ */
