#ifndef STA013_H
#define STA013_H
#ifdef __cplusplus
extern "C" {
#endif

#include "../System/lpc214x.h"


// TODO: 3a. (Done) Finish the # Defines at STA013.h header file.
#define STA013_CS_PIN		(1<<29)					// Select the PIN# for Decoder's CS Signal
#define STA013_RESET_PIN	(1<<13)					// Select the PIN# for Decoder's Reset Signal
#define STA013_DATREQ_PIN	(1<<15)					// Select the PIN# for Decoder's DATAREQ Signal
#define CONFIG_STA_PINS()	PINSEL0 &= ~(3 << 26);		\
							PINSEL0 &= ~(3 << 30);		\
							PINSEL2 &= ~(3 << 26);		\
							IOCLR1  = STA013_CS_PIN;	\
							IODIR1 |= STA013_CS_PIN;	\
							IOSET0  = STA013_RESET_PIN; \
							IODIR0 |= STA013_RESET_PIN;	\
							IODIR0 &= ~STA013_DATREQ_PIN

/// These # defines should work.
#define STA013_RESET(yes)   if(yes) IOCLR0 = STA013_RESET_PIN; else IOSET0 = STA013_RESET_PIN
#define SELECT_MP3_CS()		IOSET1 = STA013_CS_PIN
#define DESELECT_MP3_CS()	IOCLR1 = STA013_CS_PIN
#define STA013_NEEDS_DATA()	(IOPIN0 & STA013_DATREQ_PIN)

/// Initializes the MP3 Decoder
/// Performs Reset and downloads the software update.
unsigned char initialize_sta013(void);

void sta013StartDecoder(void);	///< Starts the MP3 Decoding
void sta013StopDecoder(void);	///< Stops the MP3 Decoding
void sta013PauseDecoder(void);	///< Pauses the MP3 Decoder
void sta013ResumeDecoder(void);	///< Resumes the paused MP3 Decoder.

/// Gets the bitrate, sampling frequency and mode for the mp3 data being streamed
void sta013GetMP3Info(unsigned short *bitrate, unsigned char *sampFreq, unsigned char *mode);

/// @returns the average bit-rate of the mp3 data being streamed
unsigned short sta013GetAverageBitrate(void);

/// Sets the Volume 0-100 and the balance of left/right audio channels
void sta013SetVolume(unsigned char volume, char balance);

/// Sets the bass and treble of the Decoder
void sta013SetTone(char bassEnh, unsigned short bassFreq, char trebleEnh, unsigned short trebleFreq);


#ifdef __cplusplus
}
#endif
#endif
