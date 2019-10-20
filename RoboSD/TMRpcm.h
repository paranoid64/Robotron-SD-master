/*Library by TMRh20 2012-2014

Contributors:

  ftp27 (GitHub) - setVolume(); function and code
  muessigb (GitHub) - metatata (ID3) support

*/

#ifndef TMRpcm_h   // if x.h hasn't been included yet...
#define TMRpcm_h   //   #define this so the compiler knows it has been included

#include <Arduino.h>
#include "pcmConfig.h"
#include <SdFat.h>

class TMRpcm
{
 public:
 	//TMRpcm();
 	//*** General Playback Functions and Vars ***
	void play(char* filename);
	void stopPlayback();
	void volume(char vol);
	void setVolume(char vol);
	void disable();
	void quality(boolean q);
	void loop(boolean set);
	byte speakerPin;
	boolean isPlaying();

	//*** Public vars used by RF library also ***
	boolean wavInfo(char* filename);

	unsigned int SAMPLE_RATE;

	void play(char* filename, unsigned long seekPoint);

	//*** Recording WAV files ***
	void createWavTemplate(char* filename,unsigned int sampleRate);
	void finalizeWavTemplate(char* filename);

	void startRecording(char* fileName, unsigned int SAMPLE_RATE, byte pin);
	void startRecording(char *fileName, unsigned int SAMPLE_RATE, byte pin, byte passThrough);
	void stopRecording(char *fileName);

 private:

 	void setPin();
	void timerSt();
	unsigned long fPosition();
	unsigned int resolution;
	byte lastSpeakPin;
	boolean seek(unsigned long pos);
	boolean ifOpen();
};

#endif
