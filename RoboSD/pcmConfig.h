
/*
This library was intended to be a simple and user friendly wav audio player using standard
Arduino libraries, playing bare-bones standard format WAV files.

Many of the extra features have been added due to user request, and are enabled
optionally only to preserve the out of the box simplicity and performance initially
intended.

Code/Updates: https://github.com/TMRh20/TMRpcm
Wiki: https://github.com/TMRh20/TMRpcm/wiki
Blog: https://tmrh20.blogspot.com/

*/


#ifndef pcmConfig_h   // if x.h hasn't been included yet...
#define pcmConfig_h   //   #define this so the compiler knows it has been included

#include <Arduino.h>

#define  CSPin  53

/****************** GENERAL USER DEFINES *********************************
 See https://github.com/TMRh20/TMRpcm/wiki for info on usage

   Override the default size of the buffers (MAX 254). There are 2 buffers, so memory usage will be double this number
   Defaults to 64bytes for Uno etc. 254 for Mega etc. note: In multi mode there are 4 buffers*/
#define buffSize 254  //must be an even number

  /* Uncomment to run the SD card at full speed (half speed is default for standard SD lib)*/
#define SD_FULLSPEED

//#define debug
/****************** ADVANCED USER DEFINES ********************************
   See https://github.com/TMRh20/TMRpcm/wiki for info on usage

   /* Use the SDFAT library from http://code.google.com/p/sdfatlib/            */
#define SDFAT

   /* MULTI Track mode currently allows playback of 2 tracks at once          */
//#define ENABLE_MULTI  //Using separate pins on a single 16-bit timer

   /* The library uses two different ramping methods to prevent popping sounds
      when PWM is enabled or disabled. This option is autodetected unless defined here*/
//#define rampMega

   /* Initial implementation for recording of WAV files to SD card via a microphone or input connected to an analog pin
   SdFat library is recommended
   Requires a class 4 card minimum, buffSize may need to be increased to 254 if audio is skipping etc.
   Depending on the card, can take a few seconds for recording to start
   																									*/
#define ENABLE_RECORDING
	// Amount of space to pre-allocate for recording
//#define BLOCK_COUNT 2000UL  // 10000 = 500MB   2000 = 100MB
#define BLOCK_COUNT 10000

#endif
