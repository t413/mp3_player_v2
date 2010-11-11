#include "i2c.h"

#include "../System/lpc214x.h"
#include "../System/sysConfig.h"


//set (I2CONSET)
#define AA   0x04 /* Assert acknowledge flag. */
#define SI   0x08 /* I2C interrupt flag. */
#define STO  0x10 /* STOP flag. */
#define STA  0x20 /* START flag, enables i2c */
#define I2EN 0x40 /* I2C interface enable. */
//clear (I2CONCLR)
#define AAC   0x04 /* I2CONCLR I2C interrupt Clear bit*/
#define SIC   0x08 /* I2CONCLR I2C interrupt Clear bit*/
#define STAC  0x20 /* I2CONCLR START flag Clear bit.*/
#define I2ENC 0x40 /* I2CONCLR START flag Clear bit.*/


/*--------global interrupt variables-------*/
volatile unsigned char I2C_OP_COMPLETED;
volatile unsigned char I2C_SLAVE_ADDRESS;
volatile unsigned char I2C_SLAVE_REGISTER;
volatile unsigned char * I2C_DATA_ARRAY;
volatile unsigned int I2C_DATA_ARRAY_LEGNTH;


void i2c_init(unsigned int freq){
	//  Set pinouts as scl and sda
	PINSEL0  &= ~(0x0F<<4); // reset config for PINSEL0 for P0.2 & P0.3
	PINSEL0  |= (5<<4); // setup P0.2 for SCL0 and P0.3 for SDA0
	I2CONCLR = AAC | SIC | STAC | I2ENC; //clear everything
	I2SCLL = PCLK/(freq*1000)/2;
	I2SCLH = PCLK/(freq*1000)/2;
	I2CONSET = I2EN;  //Active Master Mode on I2C bus (I2C interface enable)
	/*------enable i2c interrupts------*/
	VICIntEnable |= (1<<9);			 //enable hardware i2c interrupt
	VICVectCntl5  = (1<<5) | 9;		     //enable vector 2 for i2c0 (see talbe 57)
	VICVectAddr5  = (volatile unsigned long *)(&i2c_isr);  //write vector address.
}

void i2c_isr(){
	i2c_stateMachine();     //run the state machine code.
	VICVectAddr = 0;	//acknoledge interrupt
}

void i2c_stateMachine()
{
	switch (I2STAT) {
		case 0x08: //Start transmitted. next send slave address, then ACK will be received.
			I2DAT = I2C_SLAVE_ADDRESS & ~(1); //send _even_ transmit address no matter what.
			break;
		case 0x10: //Repeat Start transmitted.
			I2DAT = I2C_SLAVE_ADDRESS; // (this time the read address is sent)
			break;
		case 0x18:  // 0x18 (SUCCESS) = SLA+W transmitted, ACK received
			I2CONCLR = STA; //clear the START flag.
			I2DAT = I2C_SLAVE_REGISTER;
			break;
		case 0x28: //Data has been transmitted, ACK has been received.
			if (I2C_SLAVE_ADDRESS & 1){  //this is a read address.. send the repeat start.
				I2CONSET = STA; //set the start bit again.
			}
			else { //else we're writing data, even address
				if (I2C_DATA_ARRAY_LEGNTH > 0){
					I2DAT = I2C_DATA_ARRAY[0];
					I2C_DATA_ARRAY = I2C_DATA_ARRAY + 1; //increment array position
					--I2C_DATA_ARRAY_LEGNTH; //decrement how many more bytes to read
				}
				else {
					I2CONSET = STO;
					I2CONCLR = SIC; //clear the SI flag
					while (I2CONSET & STO); //wait for stop bit to clear
					I2C_OP_COMPLETED = 1;  //mark globally that we're done.
					return;
				}
			}
			break;
		case 0x40:  // 0x40 (SUCCESS) = SLA+R transmitted, ACK received
			I2CONCLR = STA; //clear the START flag.
			if (I2C_DATA_ARRAY_LEGNTH >= 2) I2CONSET = AA; //have 2 or more bytes to read
			else I2CONCLR = AAC; // only 1 byte to read.
			break;
		case 0x50:
			I2C_DATA_ARRAY[0] = I2DAT;
			I2C_DATA_ARRAY = I2C_DATA_ARRAY + 1;    //increment array position
			--I2C_DATA_ARRAY_LEGNTH;				//decrement how many more bytes to read
			if (I2C_DATA_ARRAY_LEGNTH >= 2){ //if we've got more than 2 chars left
				I2CONSET = AA; // "Assert acknowledge flag" --> I want more
			}
			else { //haven't read enough.. send more.
				I2CONCLR = AAC; //clear "Assert acknowledge flag" and "I2C interrupt"
			}
			break;
		case 0x58: //data read, nACKED --> last byte to read.
			I2C_DATA_ARRAY[0] = I2DAT;
			I2C_DATA_ARRAY = I2C_DATA_ARRAY + 1;    //increment array position
			--I2C_DATA_ARRAY_LEGNTH;				//decrement how many more bytes to read
			I2CONCLR = SIC;
			I2CONSET = STO | AA;
			while (I2CONSET & STO); //wait for stop bit to clear
			I2C_OP_COMPLETED = 1;   //mark globally that we're done.
			return;
			break;
		default: //stop.
			I2CONSET = STO;
			while (I2CONSET & STO); //wait for stop bit to clear
	}
	I2CONCLR = SIC; //clear SI flag
	I2C_OP_COMPLETED = 0;
	return;
}

void i2c_send(unsigned char slaveAddr, unsigned char slaveReg, unsigned char * data, unsigned int legnth) {
	I2C_OP_COMPLETED = 0;
	I2C_SLAVE_ADDRESS = (slaveAddr & ~(1)); //make sure the slaveAddr is _even_ (like 0x40)
	I2C_SLAVE_REGISTER = slaveReg;
	I2C_DATA_ARRAY = data;
	I2C_DATA_ARRAY_LEGNTH = legnth;
	I2CONSET = STA; //set the start bit.
	// (now the hardware does the send opperation)
while (I2C_OP_COMPLETED != 1);
}

void i2c_revieve(unsigned char slaveAddr, unsigned char slaveReg, unsigned char * data, unsigned int legnth) {
	I2C_OP_COMPLETED = 0;
	I2C_SLAVE_ADDRESS = (slaveAddr | 1); //make sure the slaveAddr is _odd_ (like 0x41)
	I2C_SLAVE_REGISTER = slaveReg;
	I2C_DATA_ARRAY = data;
	I2C_DATA_ARRAY_LEGNTH = legnth;
	I2CONSET = STA; //set the start bit.
	while (I2C_OP_COMPLETED != 1);
}

void i2c_send_byte(unsigned char slaveAddr, unsigned char slaveReg, unsigned char data) {
	volatile unsigned char dataArray[1] = {data};
	i2c_send(slaveAddr,slaveReg,dataArray,1);
}

unsigned char i2c_receive_byte(unsigned char slaveAddr, unsigned char slaveReg) {
	volatile unsigned char dataArray[1];
	i2c_revieve(slaveAddr,slaveReg,dataArray,1);
	return dataArray[0];
}
