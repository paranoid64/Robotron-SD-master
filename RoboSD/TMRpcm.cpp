/*Library by TMRh20 2012-2014*/

#define RESOLUTION_BASE ((F_CPU) / 10)
    
#include "pcmConfig.h"
#include <SdFat.h>
#include "TMRpcm.h"

	const byte togByte = _BV(ICIE1); //Get the value for toggling the buffer interrupt on/off

	#if !defined (buffSize)
		#define buffSize 254
	#endif


		volatile byte *TIMSK[] = {&TIMSK1,&TIMSK3,&TIMSK4,&TIMSK5};
		volatile byte *TCCRnA[] = {&TCCR1A,&TCCR3A,&TCCR4A,&TCCR5A};
		volatile byte *TCCRnB[] = {&TCCR1B, &TCCR3B,&TCCR4B,&TCCR5B};
		volatile unsigned int *OCRnA[] = {&OCR1A,&OCR3A,&OCR4A,&OCR5A};
		volatile unsigned int *ICRn[] = {&ICR1, &ICR3,&ICR4,&ICR5};
		volatile unsigned int *TCNT[] = {&TCNT1,&TCNT3,&TCNT4,&TCNT5};


		 	volatile unsigned int one, two, three, four;
			volatile unsigned int *OCRnB[] ={&one,&two,&three,&four};

	//*** These aliases can be commented out to enable full use of alternate timers for other things
		ISR(TIMER3_OVF_vect, ISR_ALIASOF(TIMER1_OVF_vect));
		ISR(TIMER3_CAPT_vect, ISR_ALIASOF(TIMER1_CAPT_vect));

		ISR(TIMER4_OVF_vect, ISR_ALIASOF(TIMER1_OVF_vect));
		ISR(TIMER4_CAPT_vect, ISR_ALIASOF(TIMER1_CAPT_vect));

		ISR(TIMER5_OVF_vect, ISR_ALIASOF(TIMER1_OVF_vect));
		ISR(TIMER5_CAPT_vect, ISR_ALIASOF(TIMER1_CAPT_vect));

		#if defined (ENABLE_RECORDING)
		ISR(TIMER3_COMPA_vect, ISR_ALIASOF(TIMER1_COMPA_vect));
		ISR(TIMER3_COMPB_vect, ISR_ALIASOF(TIMER1_COMPB_vect));

		ISR(TIMER4_COMPA_vect, ISR_ALIASOF(TIMER1_COMPA_vect));
		ISR(TIMER4_COMPB_vect, ISR_ALIASOF(TIMER1_COMPB_vect));

		ISR(TIMER5_COMPA_vect, ISR_ALIASOF(TIMER1_COMPA_vect));
		ISR(TIMER5_COMPB_vect, ISR_ALIASOF(TIMER1_COMPB_vect));
		#endif
//*********** Standard Global Variables ***************
volatile unsigned int dataEnd;
volatile boolean buffEmpty[2] = {true,true}, whichBuff = false, playing = 0, a, b;

//*** Options/Indicators from MSb to LSb: paused, qual, rampUp, 2-byte samples, loop, loop2nd track, 16-bit ***
byte optionByte = B01100000;

volatile byte buffer[2][buffSize], buffCount = 0;
char volMod=0;
byte tt;

volatile byte loadCounter = 0, SR = 3;

SdFile sFile;

#if defined (ENABLE_RECORDING)
	Sd2Card card1;
	byte recording = 0;
#endif

//**************************************************************
//********** Core Playback Functions used in all modes *********
void TMRpcm::timerSt(){
	*ICRn[tt] = resolution;
	*TCCRnA[tt] = _BV(WGM11) | _BV(COM1A1); //WGM11,12,13 all set to 1 = fast PWM/w ICR TOP
	*TCCRnB[tt] = _BV(WGM13) | _BV(WGM12) | _BV(CS10);
}

void TMRpcm::setPin(){
	disable();
	pinMode(speakerPin,OUTPUT);

		switch(speakerPin){
			case 5: tt=1; break; //use TIMER3
			case 6: tt=2; break;//use TIMER4
			case 46:tt=3; break;//use TIMER5
			default:tt=0; break; //useTIMER1 as default
		}

	#if defined (SD_FULLSPEED)
		SPSR |= (1 << SPI2X);
	  SPCR &= ~((1 <<SPR1) | (1 << SPR0));
	#endif
}

boolean TMRpcm::wavInfo(char* filename){

  //check for the string WAVE starting at 8th bit in header of file
  sFile.open(filename);
  
  if( !ifOpen() ){ return 0; }
  seek(8);
  char wavStr[] = {'W','A','V','E'};
  for (byte i =0; i<4; i++){
	  if(sFile.read() != wavStr[i]){
		  #if defined (debug)
		  	Serial.println(F("WAV ERROR"));
		  #endif
		  break; }
  }
	    seek(24);
    SAMPLE_RATE = sFile.read();
    SAMPLE_RATE = sFile.read() << 8 | SAMPLE_RATE;
	seek(44); dataEnd = buffSize;
	return 1;
}

//*************** General Playback Functions *****************

void TMRpcm::quality(boolean q){
	if(!playing){	bitWrite(optionByte,6,q); } //qual = q; }
}

void TMRpcm::stopPlayback(){
	playing = 0;
	*TIMSK[tt] &= ~(togByte | _BV(TOIE1));
	if(ifOpen()){ sFile.close(); }
}

void TMRpcm::loop(boolean set){
	bitWrite(optionByte,3,set);
}
/**************************************************
This section used for translation of functions between
 SDFAT library and regular SD library
Prevents a whole lot more #if defined statements */
	boolean TMRpcm::seek(unsigned long pos ){
		return sFile.seekSet(pos);
	}

	unsigned long TMRpcm::fPosition(){
		return sFile.curPosition();
	}

	boolean TMRpcm::ifOpen(){
		return sFile.isOpen();
	}



//***************************************************************************************
//********************** Functions for single track playback ****************************
void TMRpcm::play(char* filename){
	play(filename,0);
}

void TMRpcm::play(char* filename, unsigned long seekPoint){

  if(speakerPin != lastSpeakPin){
	  	setPin();
	  lastSpeakPin=speakerPin;
   }
  stopPlayback();
  if(!wavInfo(filename)){
  	#if defined (debug)
  		Serial.println("WAV ERROR");
  	#endif
  return;
  }//verify its a valid wav file


  		if(seekPoint > 0){ seekPoint = (SAMPLE_RATE*seekPoint) + fPosition();
  		seek(seekPoint); //skip the header info

  }
	playing = 1; bitClear(optionByte,7); //paused = 0;

	if(SAMPLE_RATE > 45050 ){ SAMPLE_RATE = 24000;
	#if defined (debug)
  	  	Serial.print("SAMPLE RATE TOO HIGH: ");
  	  	Serial.println(SAMPLE_RATE);
  	#endif
  	}

    if(bitRead(optionByte,6)){resolution = 10 * ((RESOLUTION_BASE/2)/SAMPLE_RATE);}
    else{ resolution = 10 * (RESOLUTION_BASE/SAMPLE_RATE);
	}
    byte tmp = (sFile.read() + sFile.peek()) / 2;


	//rampUp = 0;
	unsigned int mod;
	if(volMod > 0){ mod = *OCRnA[tt] >> volMod; }else{ mod = *OCRnA[tt] << (volMod*-1); }
	if(tmp > mod){
		for(unsigned int i=0; i<buffSize; i++){ mod = constrain(mod+1,mod, tmp); buffer[0][i] = mod; }
		for(unsigned int i=0; i<buffSize; i++){ mod = constrain(mod+1,mod, tmp); buffer[1][i] = mod; }
	}else{
		for(unsigned int i=0; i<buffSize; i++){ mod = constrain(mod-1,tmp ,mod); buffer[0][i] = mod; }
		for(unsigned int i=0; i<buffSize; i++){ mod = constrain(mod-1,tmp, mod); buffer[1][i] = mod; }
	}

    whichBuff = 0; buffEmpty[0] = 0; buffEmpty[1] = 0; buffCount = 0;
    noInterrupts();
	timerSt();

    *TIMSK[tt] = ( togByte | _BV(TOIE1) );

    interrupts();

}

void TMRpcm::volume(char upDown){

  if(upDown){
	  volMod++;
  }else{
	  volMod--;
  }
}

void TMRpcm::setVolume(char vol) {
    volMod = vol - 4 ;
}

#if defined (ENABLE_RECORDING)

  ISR(TIMER1_COMPA_vect){
		if(buffEmpty[!whichBuff] == 0){
	 		a = !whichBuff;
   			*TIMSK[tt] &= ~(_BV(OCIE1A));
   			sei();
   			sFile.write((byte*)buffer[a], buffSize );
   			buffEmpty[a] = 1;
   			*TIMSK[tt] |= _BV(OCIE1A);
		}
  }
#endif

ISR(TIMER1_CAPT_vect){

	if(buffEmpty[!whichBuff]){

		a = !whichBuff;
		*TIMSK[tt] &= ~togByte;
		sei();

		if( sFile.available() <= dataEnd){
				if(bitRead(optionByte,3)){ sFile.seekSet(44); *TIMSK[tt] |= togByte; return;}
				*TIMSK[tt] &= ~( togByte | _BV(TOIE1) );
				if(sFile.isOpen()){ sFile.close();}
			playing = 0;
			return;
	  	}
	  	sFile.read((byte*)buffer[a],buffSize);
			buffEmpty[a] = 0;
			*TIMSK[tt] |= togByte;
   	}
}



#if defined (ENABLE_RECORDING)
  ISR(TIMER1_COMPB_vect){

    buffer[whichBuff][buffCount] = ADCH;
	if(recording > 1){
		if(volMod < 0 ){  *OCRnA[tt] = ADCH >> (volMod*-1);
	  	}else{  		  *OCRnA[tt] = ADCH << volMod;
	  	}
	}
 		buffCount++;
 		if(buffCount >= buffSize){
    		buffCount = 0;
    		buffEmpty[!whichBuff] = 0;
  			whichBuff = !whichBuff;
  		}
  }

#endif

ISR(TIMER1_OVF_vect){


    if(bitRead(optionByte,6)){loadCounter = !loadCounter;if(loadCounter){ return; } }


		if(volMod < 0 ){  *OCRnA[tt] = *OCRnB[tt] = buffer[whichBuff][buffCount] >> (volMod*-1);
	  	}else{  		  *OCRnA[tt] = *OCRnB[tt] = buffer[whichBuff][buffCount] << volMod;
	  	}
	  	++buffCount;

  	if(buffCount >= buffSize){
	  buffCount = 0;
      buffEmpty[whichBuff] = true;
      whichBuff = !whichBuff;
  	}
}


void TMRpcm::disable(){
	playing = 0;
	*TIMSK[tt] &= ~( togByte | _BV(TOIE1) );
	if(ifOpen()){ sFile.close();}
	if(bitRead(*TCCRnA[tt],7) > 0){
		int current = *OCRnA[tt];
		for(int i=0; i < resolution; i++){

				*OCRnB[tt] = constrain((current - i),0,resolution);
				*OCRnA[tt] = constrain((current - i),0,resolution);

			for(int i=0; i<10; i++){ while(*TCNT[tt] < resolution-50){} }
		}
	}
    bitSet(optionByte,5);
    //rampUp = 1;
    *TCCRnA[tt] = *TCCRnB[tt] = 0;
}



boolean TMRpcm::isPlaying(){
	return playing;
}

/*********************************************************************************
********************** DIY Digital Audio Generation ******************************/


void TMRpcm::finalizeWavTemplate(char* filename){
	disable();

	unsigned long fSize = 0;
  
		sFile.open(filename,O_WRITE );

    if(!sFile.isOpen()){
  		#if defined (debug)
  			Serial.println("failed to finalize");
  		#endif
  		return;
		}
    fSize = sFile.fileSize()-8;

	seek(4); byte data[4] = {lowByte(fSize),highByte(fSize), fSize >> 16,fSize >> 24};
	sFile.write(data,4);
	byte tmp;
	seek(40);
	fSize = fSize - 36;
	data[0] = lowByte(fSize); data[1]=highByte(fSize);data[2]=fSize >> 16;data[3]=fSize >> 24;
	sFile.write((byte*)data,4);
	sFile.close();

	#if defined (debug)
	sFile.open(filename);
	if(ifOpen()){
		    while (fPosition() < 44) {
		      Serial.print(sFile.read(),HEX);
		      Serial.print(":");
		   	}
		   	Serial.println("");
	   	//Serial.println(sFile.size());
    	sFile.close();
	}
	#endif
}



void TMRpcm::createWavTemplate(char* filename, unsigned int sampleRate){
	disable();

#if defined (ENABLE_RECORDING)
   SdFat  vol;
	 SdFile rut;

	uint32_t bgnBlock = 0;
  uint32_t endBlock = 0;

	bool sd_ready = vol.begin(CSPin, SPI_FULL_SPEED);

    if (!sd_ready){
        vol.initErrorHalt();
		    return;
    }
	
	if (!rut.openRoot(&vol)) {
    #if defined (debug)
	    Serial.println(F("openRoot failed"));
    #endif
    return;
	}

   vol.remove(filename);

   //Serial.println(filename);

  if (!rut.createContiguous(filename, 512UL*BLOCK_COUNT)) {
      #if defined (debug)
	      Serial.println(F("createContiguous failed"));
      #endif
	  }
	  // get the location of the file's blocks
   if (!rut.contiguousRange(&bgnBlock, &endBlock)) {
      #if defined (debug)
	      Serial.println(F("contiguousRange failed"));
      #endif
	  }
  if (!vol.card()->writeStart(bgnBlock, BLOCK_COUNT)){
      #if defined (debug)
        Serial.println(F("Block Start failed!"));
      #endif
  }
  if (!vol.card()->writeStop()){
    #if defined (debug)
      Serial.println(F("Block Stop failed!"));
    #endif
  }
  
	rut.close();
#endif

   	sFile.open(filename,O_CREAT | O_WRITE);
   	if(sFile.fileSize() > 44 ){ sFile.truncate(44);}
	if(!sFile.isOpen()){
		return;
	}else{

  		//Serial.print("Sr: ");
  		//Serial.println(sampleRate);
  		seek(0);
		byte data[] = {16,0,0,0,1,0,1,0,lowByte(sampleRate),highByte(sampleRate)};
		sFile.write((byte*)"RIFF    WAVEfmt ",16);
		sFile.write((byte*)data,10);
		//unsigned int byteRate = (sampleRate/8)*monoStereo*8;
		//byte blockAlign = monoStereo * (bps/8); //monoStereo*(bps/8)
		data[0] = 0; data[1] = 0; data[2] = lowByte(sampleRate); data[3] =highByte(sampleRate);//Should be byteRate
		data[4]=0;data[5]=0;data[6]=1; //BlockAlign
		data[7]=0;data[8]=8;data[9]=0;
		sFile.write((byte*)data,10);
		sFile.write((byte*)"data    ",8);
		//Serial.print("siz");
		//Serial.println(sFile.size());
		sFile.close();

	}
}

#if defined (ENABLE_RECORDING)

void TMRpcm::startRecording(char *fileName, unsigned int SAMPLE_RATE, byte pin){
	startRecording(fileName,SAMPLE_RATE,pin,0);
}

void TMRpcm::startRecording(char *fileName, unsigned int SAMPLE_RATE, byte pin, byte passThrough){

	recording = passThrough + 1;
	setPin();
	if(recording < 3){
		//*** Creates a blank WAV template file. Data can be written starting at the 45th byte ***
		createWavTemplate(fileName, SAMPLE_RATE);

		//*** Open the file and seek to the 44th byte ***
    	sFile.open(fileName,O_WRITE );
    	if(!sFile.isOpen()){
			#if defined (debug)
				Serial.println("fail");
			#endif
			return;
		}
	seek(44);
 	}
	buffCount = 0; buffEmpty[0] = 1; buffEmpty[1] = 1;


	/*** This section taken from wiring_analog.c to translate between pins and channel numbers ***/
	#if defined(analogPinToChannel)
	#if defined(__AVR_ATmega32U4__)
		if (pin >= 18) pin -= 18; // allow for channel or pin numbers
	#endif
		pin = analogPinToChannel(pin);
	#elif defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)
		if (pin >= 54) pin -= 54; // allow for channel or pin numbers
	#elif defined(__AVR_ATmega32U4__)
		if (pin >= 18) pin -= 18; // allow for channel or pin numbers
	#elif defined(__AVR_ATmega1284__) || defined(__AVR_ATmega1284P__) || defined(__AVR_ATmega644__) || defined(__AVR_ATmega644A__) || defined(__AVR_ATmega644P__) || defined(__AVR_ATmega644PA__)
		if (pin >= 24) pin -= 24; // allow for channel or pin numbers
	#else
		if (pin >= 14) pin -= 14; // allow for channel or pin numbers
	#endif

	#if defined(ADCSRB) && defined(MUX5)
		// the MUX5 bit of ADCSRB selects whether we're reading from channels
		// 0 to 7 (MUX5 low) or 8 to 15 (MUX5 high).
		ADCSRB = (ADCSRB & ~(1 << MUX5)) | (((pin >> 3) & 0x01) << MUX5);
	#endif

	#if defined(ADMUX)
		ADMUX = (pin & 0x07);
	#endif

	//Set up the timer
	if(recording > 1){

		*TCCRnA[tt] = _BV(COM1A1); //Enable the timer port/pin as output for passthrough

	}
    *ICRn[tt] = 10 * (RESOLUTION_BASE/SAMPLE_RATE);//Timer will count up to this value from 0;
	*TCCRnA[tt] |= _BV(WGM11); //WGM11,12,13 all set to 1 = fast PWM/w ICR TOP
	*TCCRnB[tt] = _BV(WGM13) | _BV(WGM12) | _BV(CS10); //CS10 = no prescaling

	if(recording < 3){ //Normal Recording
		*TIMSK[tt] |=  _BV(OCIE1B)| _BV(OCIE1A); //Enable the TIMER1 COMPA and COMPB interrupts
	}else{
		*TIMSK[tt] |=  _BV(OCIE1B); //Direct pass through to speaker, COMPB only
	}


	ADMUX |= _BV(REFS0) | _BV(ADLAR);// Analog 5v reference, left-shift result so only high byte needs to be read
	ADCSRB |= _BV(ADTS0) | _BV(ADTS2);  //Attach ADC start to TIMER1 Compare Match B flag
	byte prescaleByte = 0;

	if(      SAMPLE_RATE < 18000){ prescaleByte = B00000110;} //ADC division factor 64 (16MHz / 64 / 13clock cycles = 19230 Hz Max Sample Rate )
	else if( SAMPLE_RATE < 27000){ prescaleByte = B00000101;} //32  (38461Hz Max)
	else{	                       prescaleByte = B00000100;} //16  (76923Hz Max)
	ADCSRA = prescaleByte; //Adjust sampling rate of ADC depending on sample rate
	ADCSRA |= _BV(ADEN) | _BV(ADATE); //ADC Enable, Auto-trigger enable


}

void TMRpcm::stopRecording(char *fileName){

/*
	*TIMSK[tt] &= ~(_BV(OCIE1B) | _BV(OCIE1A));
	ADCSRA = 0;
    ADCSRB = 0;
*/

    TIMSK1 &= ~_BV(OCIE1B) | _BV(OCIE1A);   // Disable the transmit interrupts
 //   ADCSRA = 0; ADCSRB = 0;                   // Disable Analog to Digital Converter (ADC)    

	if(recording == 1 || recording == 2){
		recording = 0;
		unsigned long position = fPosition();
		#if defined (SDFAT)
			sFile.truncate(position);
		#endif
		sFile.close();
		finalizeWavTemplate(fileName);
	}
}


#endif
