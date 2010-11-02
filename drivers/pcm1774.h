/*! \file pcm1774driver.h 
*	\brief PCM1774 DAC Driver using I2C BUS
*	\author:	SJValley Engineering - Contact:	support@sjvalley.com
*	\ingroup 	DRIVERS
*/
#ifndef PCM1774_DRIVER_H
#define PCM1774_DRIVER_H
#ifdef __cplusplus
extern "C" {
#endif

// Two PCM chips can be connected at a time to I2C BUS
//! PCM1774 Address Bit which is hardware configured, set to 0 or 1
#define PCM_1774_ADDRESS_BIT	(0)

//! PCM1774 Address based on the PCM_1667_ADR_BIT
#define PCM_1774_ADDRESS	((0b1000110 << 1) | PCM_1774_ADDRESS_BIT)


//! Call this before anything using the PCM1774 DAC
char initialize_pcm1774(void);

//! \post Restores default configuration of the DAC
void pcm1774_LoadDefaultDigitalConfiguration(void);

//! \post Performs Software Reset
void pcm1774_Reset(void);

//! @param volumeLeft 0-100 volume of Left Channel
//! @param volumeRight 0-100 volume of Right Channel
void pcm1774_OutputVolume(int volumeLeft, int volumeRight);

//! Mixer only has affect for Digital and Analog volume mixes
void pcm1774_PowerUpMixer(char powerLeft, char powerRight);

//! Redirects analog input to left output
/// @param left Redirects left channel to left output
/// @param right Redirects right channel to left output
void pcm1774_redirectAnalogInputToLeftOutput(char left, char right);

//! Redirects analog input to right output
/// @param left Redirects left channel to right output
/// @param right Redirects right channel to right output
void pcm1774_redirectAnalogInputToRightOutput(char left, char right);

//! \post Analog Bias Control is Powered Up
void pcm1774_PowerUpAnalogBiasControl(char power);

//! @param powerLeft If 1, Left Analog section is powered up
//! @param powerRight If 1, Right Analog section is powered up
void pcm1774_PowerUpAnalog(char powerLeft, char powerRight);

//! @param config 0=Stereo, 1=Monaural, 2=differential
/// \post Configures the Line Output of Analog
void pcm1774_LineOutputConfiguration(char config);

//! @param power If 1, powers up VCOM
void pcm1774_PowerUpVcom(char power);

//! param protection If 1, turns on short circuit protection of left channel
/// note Protection is ON by default
void pcm1774_setShortCircuitProtectionLeft(char protection);

//! param protection If 1, turns on short circuit protection of right channel
/// note Protection is ON by default
void pcm1774_setShortCircuitProtectionRight(char protection);

//! @returns true if short circuit is detected on left channel
char pcm1774_readShortCircuitStatusLeft(void);

//! @returns true if short circuit is detected on left channel
char pcm1774_readShortCircuitStatusRight(void);

//! @param powerLeft If 1, powers up Left channel gain amplifier
//! @param powerRight If 1, powers up Right channel gain amplifier
void pcm1774_PowerUpGainAmplifierAnalog(char powerLeft, char powerRight);

//! @param selectLeft If 1, selects left channel line input
//! @param selectRight If 1, selects right channel line input
void pcm1774_selectAnalogInput(char selectLeft, char selectRight);

//! @param leftGain 3-bit value: 0–21dB (default) 1–18dB, 2–15dB, 3–12dB 4–9 dB, 5–6dB, 6–3dB, 7–0dB
//! @param rightGain 3-bit value: 0–21dB (default) 1–18dB, 2–15dB, 3–12dB 4–9 dB, 5–6dB, 6–3dB, 7–0dB
void pcm1774_gainAmplifierAnalogInput(char leftGain, char rightGain);







/***************************************************/
// Next functions only work for DIGITAL AUDIO ******/
/***************************************************/

//! Connects left and right digital input (ie: I2S) to Mixer
//! @param leftChannel If 1, connects left channel to mixer's Left channel
//! @param rightChannel If 1, connects right channel to mixer's right channel
void pcm1774_ConnectDigitalOutputs(char leftChannel, char rightChannel);

//! Controls the volume of digital input and disables digital soft mute automatically
//! note The output volume takes control of final amplification.
//! @param volumeLeft 0-100 Volume Level of left channel
//! @param volumeRight 0-100 Volume Level of right channel
void pcm1774_DigitalVolume(int volumeLeft, int volumeRight);

//! Powers up the DAC input
void pcm1774_PowerUpDAC(char powerLeft, char powerRight);

//! Sets the Bass Control
//! @param level Range is from -15 to +15
//! note This does not affect Analog Input Audio
void pcm1774_bassBoost(signed char level);

//! Sets the MID Control
//! @param level Range is from -15 to +15
//! note This does not affect Analog Input Audio
void pcm1774_middleBoost(signed char level);

//! Sets the Treble Control
//! @param level Range is from -15 to +15
//! note This does not affect Analog Input Audio
void pcm1774_trebleBoost(signed char level);

//! Sets the 3D Sound Control
///	@param narrow_or_wide  Use 1 to set to narrow 3D, or 0 for Wide
/// @param level The level of 3D Sound effect, should be 0-10
void pcm1774_3dSoundEffect(char narrow_or_wide, char level);








//! Read datasheet for more info
void pcm1774_DigitalDeEmphasisFilter(char filter);
//! Read datasheet for more info
void pcm1774_DigitalInterfaceSelection(char selection);
//! Read datasheet for more info
void pcm1774_DigitalGainControl(char control);
//! Read datasheet for more info
void pcm1774_DigitalOversamplingControl(char control);



#ifdef __cplusplus
}
#endif
#endif
