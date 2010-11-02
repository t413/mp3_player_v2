#include "pcm1774.h"

// TODO: 4a.  Include the I2C Driver File here

#ifndef FALSE
#define FALSE 0
#endif

#ifndef TRUE
#define TRUE 1
#endif

//! Low level function to send a byte to the PCM1774
//! @param address is the 8-bit address of register to write to
//! @param data is the data to write to register
void pcm1774_sendByte(unsigned char address, unsigned char data)
{
	// TODO:4b.  Fill in I2C Driver write function for I2C interfaced DAC
	// Write PCM_1774_ADDRESS at "address" with "data"
	//i2cWriteDeviceRegister(PCM_1774_ADDRESS, address, data) ???
}

//! Low level function to read a byte from the PCM1774
//! @param address is the 8-bit address of register to read from
//! @returns the byte read from the register
unsigned char pcm1774_readByte(unsigned char address)
{
	// TODO: 4c.  Fill in I2C Driver read function for I2C Interfaced DAC
	// Read PCM_1774_ADDRESS from "address"
	return 0;
	//return i2cReadDeviceRegister(PCM_1774_ADDRESS, address) ???
}

//! @param	address Address to write to
//!	@param   andOr If 1, then byte will be ORed by value of byte, otherwise byte will be ANDed with value of byte
//! @param	byte The value to AND or OR the register value
//! This is used to retain existing data contents and and/or just specific bits
void pcm1774_bitMask(unsigned char address, unsigned char byte, char andOr)
{
	unsigned char data;

	if(andOr)
		data = pcm1774_readByte(address) | byte;
	else
		data = pcm1774_readByte(address) & (char)(~byte);

	pcm1774_sendByte(address, data);
}



//! @name PCM1774 Register names & values
//@{
#define PCM1774_HPA_L							0x40
#define PCM1774_HPA_R							0x41
#define PCM1774_DAC_AND_SOFT_MUTE_L				0x44
#define PCM1774_DAC_AND_SOFT_MUTE_R				0x45

#define PCM1774_DE_EMPHASIS_AUDIO_INTERFACE		0x46
#define PCM1774_ANALOG_MIXER_POWER_UP_DOWN		0x48
#define PCM1774_DAC_HPA_POWER_UP_DOWN			0x49
#define PCM1774_ANALOG_OUTPUT_CONFIG_SELECT		0x4A

#define PCM1774_HPA_INSERTION_DELETION			0x4B
#define PCM1774_SHUTDOWN_STATUS_READBACK		0x4D
#define PCM1774_PG_1_2_5_6_POWER_UP_DOWN		0x52
#define PCM1774_MASTER_MODE						0x54

#define PCM1774_SYSTEM_RESET_SAMPLING_CONTROL	0x55
#define PCM1774_BCK_CONFIG_SAMPLE_RATE_CONTROL	0x56
#define PCM1774_ANALOG_INPUT_SELECT				0x57
#define PCM1774_ANALOG_MIXER_SWITCH				0x58

#define PCM1774_ANALOG_TO_ANALOG_PATH_GAIN		0x59
#define PCM1774_MICROPHONE_BOOST				0x5A
#define PCM1774_BASS_BOOST						0x5C
#define PCM1774_MIDDLE_BOOST					0x5D
#define PCM1774_TREBLE_BOOST					0x5E

#define PCM1774_SOUND_EFFECT_SOURCE_3D_SOUND	0x5F
#define PCM1774_DIGITAL_MONAURAL_MIXING			0x60
#define PCM1774_PG1_PG2_ADDITIONAL_GAIN			0x7C
#define PCM1774_POWER_UP_DOWN_TIME_CONTROL		0x7D
//@}

char initialize_pcm1774(void)
{
	// Default powerup registers recommended by PCM1774 at page 13/49
	pcm1774_sendByte(0x40, 0x27);	// L-Channel analog output volume
	pcm1774_sendByte(0x41, 0x27);	// R-Channel analog output volume

	pcm1774_sendByte(0x44, 0x27);	// L-Channel digital output volume
	pcm1774_sendByte(0x45, 0x27);	// R-Channel digital output volume

	pcm1774_sendByte(0x46, 0x20);	// DAC Audio Interface
	pcm1774_sendByte(0x49, 0xE0);	// DAC and analog BIAS power-up

	pcm1774_sendByte(0x56, 0x01);	// Zero-cross detection enable
	pcm1774_sendByte(0x48, 0x03);	// Analog Mixer Power-up

	pcm1774_sendByte(0x58, 0x11);	// Analog mixer input SW2, SW5 select to feed Digital DAC signals to Audio Jack output
	pcm1774_sendByte(0x49, 0xEC);	// Headphone Amplifier Power-up

	pcm1774_sendByte(0x4A, 0x01);	// Vcom power up
	pcm1774_sendByte(0x52, 0x30);	// Analog front-end D2S, MCB, PG1, 2, 5, 6 power-up

	//pcm1774_sendByte(0x57, 0x11);	// Analog input select -- (For FM Radio)
	pcm1774_sendByte(0x57, 0x00);	// Analog input select -- We don't want FM Radio's Analog Input by default.

	pcm1774_DigitalVolume(90, 90);	// Digital Volume
	pcm1774_OutputVolume(50, 50);	// 20% overall Output Volume
	return 1;
}

void pcm1774_Reset(void)
{
	volatile int i = 0;
	pcm1774_sendByte(PCM1774_SYSTEM_RESET_SAMPLING_CONTROL, (1<<7));
	for(i=0; i<5000; i++);
	initialize_pcm1774();
	pcm1774_LoadDefaultDigitalConfiguration();
}

void pcm1774_LoadDefaultDigitalConfiguration(void)
{
	initialize_pcm1774();

	pcm1774_PowerUpMixer(TRUE, TRUE);
	pcm1774_PowerUpAnalog(TRUE, TRUE);
	pcm1774_PowerUpGainAmplifierAnalog(TRUE, TRUE);
	pcm1774_selectAnalogInput(FALSE, FALSE);
	
	// Direct left input to left output
	pcm1774_redirectAnalogInputToLeftOutput(FALSE, FALSE);
	// Direct right input to right output
	pcm1774_redirectAnalogInputToRightOutput(FALSE, FALSE);

	pcm1774_gainAmplifierAnalogInput(4, 4);
	pcm1774_ConnectDigitalOutputs(1, 1);
	pcm1774_DigitalVolume(40, 40);	// Digital Volume at 80%
	pcm1774_OutputVolume(50, 50);	// 20% Output Volume
}

void pcm1774_OutputVolume(int volumeLeft, int volumeRight)
{
	// Scale 0-100 to 0-63
	volumeLeft = (190*volumeLeft)/300;
	volumeRight= (190*volumeRight)/300;

	pcm1774_sendByte(PCM1774_HPA_L, (0x3f &  volumeLeft));
	pcm1774_sendByte(PCM1774_HPA_R, (0x3f & volumeRight));
}

void pcm1774_ConnectDigitalOutputs(char leftChannel, char rightChannel)
{
	pcm1774_bitMask(PCM1774_ANALOG_MIXER_SWITCH, (1<<0), leftChannel);
	pcm1774_bitMask(PCM1774_ANALOG_MIXER_SWITCH, (1<<4), rightChannel);
}


void pcm1774_DigitalVolume(int volumeLeft, int volumeRight)
{
	// Scale 0-100 to 0-63
	volumeLeft = (190*volumeLeft)/300;
	volumeRight= (190*volumeRight)/300;

	pcm1774_sendByte(PCM1774_DAC_AND_SOFT_MUTE_L, (0x3f &  volumeLeft));
	pcm1774_sendByte(PCM1774_DAC_AND_SOFT_MUTE_R, (0x3f & volumeRight));
}

void pcm1774_DigitalDeEmphasisFilter(char filter)
{
	// Filter should be:
	// 00:	OFF
	// 01:	32Khz
	// 10:	44.1Khz
	// 11:	48Khz

	// Clear old filter first
	pcm1774_bitMask(PCM1774_DE_EMPHASIS_AUDIO_INTERFACE, 0xC0, 0);
	// set the bits defined by filter to 1
	pcm1774_bitMask(PCM1774_DE_EMPHASIS_AUDIO_INTERFACE, (filter<<6), 1);
}

void pcm1774_DigitalInterfaceSelection(char selection)
{
	// selection should be:
	// 00: I2S Format
	// 01: Right-justified format
	// 10: left-justified format
	// 11: DSP Format

	// Clear old filter first
	pcm1774_bitMask(PCM1774_DE_EMPHASIS_AUDIO_INTERFACE, 0x30, 0);
	// set the bits defined by filter to 1
	pcm1774_bitMask(PCM1774_DE_EMPHASIS_AUDIO_INTERFACE, ((selection&0x03)<<4), 1);
}

void pcm1774_DigitalGainControl(char control)
{
	// control should be:
	// 00: 0db
	// 01: 6db
	// 10: 12db
	// 11: 18db

	// Clear old filter first
	pcm1774_bitMask(PCM1774_DE_EMPHASIS_AUDIO_INTERFACE, 0x0C, 0);
	// set the bits defined by filter to 1
	pcm1774_bitMask(PCM1774_DE_EMPHASIS_AUDIO_INTERFACE, ((control&0x03)<<2), 1);
}

void pcm1774_DigitalOversamplingControl(char control)
{
	pcm1774_bitMask(PCM1774_DE_EMPHASIS_AUDIO_INTERFACE, 0x01, control);
}

void pcm1774_PowerUpMixer(char powerLeft, char powerRight)
{
	pcm1774_bitMask(PCM1774_ANALOG_MIXER_POWER_UP_DOWN, (1<<0), powerLeft);
	pcm1774_bitMask(PCM1774_ANALOG_MIXER_POWER_UP_DOWN, (1<<1), powerRight);
}

void pcm1774_PowerUpAnalogBiasControl(char power)
{
	pcm1774_bitMask(PCM1774_DAC_HPA_POWER_UP_DOWN, (1<<7), power);
}

void pcm1774_PowerUpDAC(char powerLeft, char powerRight)
{
	pcm1774_bitMask(PCM1774_DAC_HPA_POWER_UP_DOWN, (1<<5), powerLeft);
	pcm1774_bitMask(PCM1774_DAC_HPA_POWER_UP_DOWN, (1<<6), powerRight);
}

void pcm1774_PowerUpAnalog(char powerLeft, char powerRight)
{
	pcm1774_bitMask(PCM1774_DAC_HPA_POWER_UP_DOWN, (1<<2), powerLeft);
	pcm1774_bitMask(PCM1774_DAC_HPA_POWER_UP_DOWN, (1<<3), powerRight);
}

void pcm1774_LineOutputConfiguration(char config)
{
	char stereo = 0x00;
	char monaural = 0x04;
	char differential = 0x08;

	pcm1774_bitMask(PCM1774_ANALOG_OUTPUT_CONFIG_SELECT, 0x0C, 0);
	switch(config)
	{
		case 1:	pcm1774_bitMask(PCM1774_ANALOG_OUTPUT_CONFIG_SELECT, 0xC0, monaural);	break;
		case 2:	pcm1774_bitMask(PCM1774_ANALOG_OUTPUT_CONFIG_SELECT, 0xC0, differential); break;

		case 0:
		default:
			pcm1774_bitMask(PCM1774_ANALOG_OUTPUT_CONFIG_SELECT, 0xC0, stereo);
	}
}

void pcm1774_PowerUpVcom(char power)
{
	pcm1774_bitMask(PCM1774_ANALOG_OUTPUT_CONFIG_SELECT, 0x01, power);
}

void pcm1774_setShortCircuitProtectionLeft(char protection)
{
	// 0 is to enable protection
	if(protection)	pcm1774_bitMask(PCM1774_ANALOG_OUTPUT_CONFIG_SELECT, 0x04, 0);
	else			pcm1774_bitMask(PCM1774_ANALOG_OUTPUT_CONFIG_SELECT, 0x04, 1);
}

void pcm1774_setShortCircuitProtectionRight(char protection)
{
	// 0 is to enable protection
	if(protection)	pcm1774_bitMask(PCM1774_ANALOG_OUTPUT_CONFIG_SELECT, 0x08, 0);
	else			pcm1774_bitMask(PCM1774_ANALOG_OUTPUT_CONFIG_SELECT, 0x08, 1);
}

char pcm1774_readShortCircuitStatusLeft(void)
{
	// 0 means short circuit detected
	return (!(pcm1774_readByte(PCM1774_SHUTDOWN_STATUS_READBACK) & 0x04));
}

char pcm1774_readShortCircuitStatusRight(void)
{
	// 0 means short circuit detected
	return (!(pcm1774_readByte(PCM1774_SHUTDOWN_STATUS_READBACK) & 0x08));
}

void pcm1774_PowerUpGainAmplifierAnalog(char powerLeft, char powerRight)
{
	pcm1774_bitMask(PCM1774_PG_1_2_5_6_POWER_UP_DOWN, (1<<4), powerLeft);
	pcm1774_bitMask(PCM1774_PG_1_2_5_6_POWER_UP_DOWN, (1<<5), powerRight);
}

void pcm1774_selectAnalogInput(char selectLeft, char selectRight)
{
	pcm1774_bitMask(PCM1774_ANALOG_INPUT_SELECT, (1<<0), selectLeft);	
	pcm1774_bitMask(PCM1774_ANALOG_INPUT_SELECT, (1<<4), selectRight);
}

void pcm1774_gainAmplifierAnalogInput(char leftGain, char rightGain)
{
	/* leftGain and rightGain are 3-bit values as follows:
	0 –21 dB (default)
	1 –18 dB
	2 –15 dB
	3 –12 dB
	4 –9 dB
	5 –6 dB
	6 –3 dB
	7 –0 dB
	*/

	leftGain &= 0x07;
	rightGain &= 0x07;
	pcm1774_bitMask(PCM1774_ANALOG_TO_ANALOG_PATH_GAIN, 0x77, 0);
	pcm1774_bitMask(PCM1774_ANALOG_TO_ANALOG_PATH_GAIN, ((rightGain<<4)|leftGain), 1);
}

void pcm1774_redirectAnalogInputToLeftOutput(char left, char right)
{
	char bitmask;

	bitmask = (1 << 1) | (1 << 2);
	pcm1774_bitMask(PCM1774_ANALOG_MIXER_SWITCH, bitmask, 0);

	bitmask = (left << 1) | (right << 2);
	pcm1774_bitMask(PCM1774_ANALOG_MIXER_SWITCH, bitmask, 1);
}

void pcm1774_redirectAnalogInputToRightOutput(char left, char right)
{
	char bitmask;

	bitmask = (1 << 6) | (1 << 5);
	pcm1774_bitMask(PCM1774_ANALOG_MIXER_SWITCH, bitmask, 0);

	bitmask = (left << 6) | (right << 5);
	pcm1774_bitMask(PCM1774_ANALOG_MIXER_SWITCH, bitmask, 1);
}

static char pcm1774_getMappedLevel(int level)
{
					// -12,  -11,  -10,   -9,   -8,   -7,   -6,   -5,   -4,   -3,   -2,   -1
	char table[] = { 0x9B, 0x9A, 0x99, 0x98, 0x97, 0x96, 0x95, 0x94, 0x93, 0x92, 0x91, 0x90,
						0x80, // 0
				   //   1,    2,    3,    4,    5,    6,    7,    8,    9,   10,   11,   12
					 0x8E, 0x8D, 0x8C, 0x8B, 0x8A, 0x89, 0x88, 0x87, 0x86, 0x85, 0x84, 0x83
					};

	if(level < -12 || level > 12)
		level = 0;
	level += 12;	// map level to the table
	return table[level];
}

void pcm1774_bassBoost(signed char level)
{
	pcm1774_sendByte(PCM1774_BASS_BOOST, pcm1774_getMappedLevel(level));
}

void pcm1774_middleBoost(signed char level)
{
	pcm1774_sendByte(PCM1774_MIDDLE_BOOST, pcm1774_getMappedLevel(level));
}

void pcm1774_trebleBoost(signed char level)
{
	pcm1774_sendByte(PCM1774_TREBLE_BOOST, pcm1774_getMappedLevel(level));
}

void pcm1774_3dSoundEffect(char narrow_or_wide, char level)
{
	// If not narrow, then set the bit for wide
	char wide = 0;
	if(!narrow_or_wide) wide = 0x10;
	if(level > 10) level = 10;

	if(level==0)	pcm1774_sendByte(PCM1774_SOUND_EFFECT_SOURCE_3D_SOUND, 0);
	else			pcm1774_bitMask(PCM1774_SOUND_EFFECT_SOURCE_3D_SOUND, (level|0x40|wide), 1);
}

