#include <Arduino.h>
#include <SdFat.h>      // Sketch > Include library > Manage libraries > search: SdFat, by Bill Greiman
#include <LiquidCrystal_I2C.h>
#include "pcmConfig.h"
#include "TMRpcm.h"

#define SD0_MO          50     // SD MOSI
#define SD0_MI          51     // SD MISO
#define SD0_CK          52     // SD SCK

#define DIR_DEPTH       8
#define SFN_DEPTH       13
#define LFN_DEPTH       100

#define SCROLL_SPEED  250    // Text scroll delay
#define SCROLL_WAIT   3000   // Delay before scrolling starts

TMRpcm tmrpcm;   // create an object for use in this sketch
LiquidCrystal_I2C lcd(0x27, 16, 2);

char        entry_type;
SdFat       sd;
SdFile      entry;
SdFile      dir[DIR_DEPTH];
char        sfn[SFN_DEPTH];
char        lfn[LFN_DEPTH + 1];
bool        sd_ready = false;
int16_t     entry_index = 0;
int8_t      dir_depth = -1;
int16_t     dir_index[DIR_DEPTH] = {};
bool        canceled = false;
bool        level = LOW;
bool        loading = false;
bool        record = false;
char        cdir[13];
char        ldir[13];

byte          scroll_pos = 0;
unsigned long scroll_time = millis() + SCROLL_WAIT;

byte ifolder[8] = {
  0b00000,
  0b11000,
  0b11111,
  0b11111,
  0b11111,
  0b11111,
  0b00000,
  0b00000
};
byte ifile[8] = {
  0b00000,
  0b00111,
  0b01101,
  0b11101,
  0b10001,
  0b10001,
  0b11111,
  0b00000
};

/// This function displays a text on the first line with a horizontal scrolling if necessary.
void scrollText(char* text){

    if (scroll_pos < 0){ scroll_pos = 0; }
    char outtext[17];
    
    if (entry.isDir()){
          outtext[0] = 1;
          lcd.write(1);
    }
    else {
      outtext[0] = 2;
      lcd.write(2);
    }

    for (byte i = 1; i < 16; ++i){
        int8_t p = i + scroll_pos - 1;
        if (p < strlen(text)){
            outtext[i] = text[p];
        }
        else{
            outtext[i] = '\0';
        }
    }
    outtext[16] = '\0';

    lcd.setCursor(0, 0);
    lcd.print(F("                    "));
    lcd.setCursor(0, 0);
    lcd.print(outtext);
    lcd.setCursor(0, 1);
    lcd.print(F("                    "));
    
}

unsigned int readButton(unsigned int pin) {     // Taste einlesen
  if(analogRead(pin) < 150) { // Analog Eingang abfragen
    delay(150);            
    if(analogRead(pin) < 150){
      //Serial.println(analogRead(pin));
      //Serial.println(pin);
      return 1;               // war gedrückt
    }
  }
  return 0;                   // war nicht gedrückt
}

bool checkForWAV(char *filename){
  auto ext = strlwr(filename + (strlen(filename) - 4));
  return
    !!strstr(ext, ".wav");
}

bool checkForTAP(char *filename){
    auto ext = strlwr(filename + (strlen(filename) - 4));
    return
        !!strstr(ext, ".tap");
}

void setupDisplay(){ 
    lcd.init();
    lcd.backlight();
  
    lcd.createChar(2, ifile);
    lcd.createChar(1, ifolder);
  
    lcd.clear();
    lcd.print(F("ROBOTON SD"));
    delay(600);
}

inline void displayEntryNameMessage(bool exists){ 
    scroll_time = millis() + SCROLL_WAIT;
    scroll_pos = 0;
    scrollText(lfn);
}

inline void displayScrollingMessage(){
    if ((millis() >= scroll_time) && (strlen(lfn) > 15)){
        scroll_time = millis() + SCROLL_SPEED;
        scrollText(lfn);
        ++scroll_pos;
        if (scroll_pos > strlen(lfn)){
            scroll_pos = 0;
            scroll_time = millis() + SCROLL_WAIT;
            scrollText(lfn);
        }
    }
}

inline void displayPlay(){
    lcd.clear();
    scroll_pos = 0;
    scrollText(lfn);
    lcd.setCursor(0, 1);
    lcd.print(F("Play"));
}

inline void displayStop(){
    lcd.clear();
    scroll_pos = 0;
    scrollText(lfn);
    lcd.setCursor(0, 1);
    lcd.print(F("Stop")); 
}

void fetchEntry(int16_t new_index){
    bool    found = true;
    int16_t index;

    entry.close();

    if (new_index < 0){
        new_index = 0;
    }

    do{
        dir[dir_depth].rewind();
        index = 0;
        while (index <= new_index){
            found = entry.openNext(&dir[dir_depth], O_READ);
            if (found){
                if (!entry.isHidden() && !entry.isSystem()){
                    if (index == new_index){
                        break;
                    }
                    ++index;
                }
                entry.close();
            }
            else{
                break;
            }
        }

        if (!found){
            new_index = entry_index;
        }
    }
    while (!found && index > 0);

    if (found){
        entry.getSFN(sfn);
        entry.getName(lfn, LFN_DEPTH);

        if (entry.isDir()){
          entry_type = 0;
        }
        else if(checkForWAV(lfn)){
          entry_type = 1;
        } else if(checkForTAP(lfn)){
          entry_type = 2;
        }
        displayEntryNameMessage(true);
        entry_index = new_index;
    }
    else{
        memset(sfn, 0, SFN_DEPTH);
        memset(lfn, 0, LFN_DEPTH + 1);
        strcpy(lfn, "<no file>");
        displayEntryNameMessage(false);
    }
}

/// This function enters a directory.
void enterDir(){
    if (dir_depth < DIR_DEPTH - 2){
        if (dir_depth < 0){
            if (dir[0].openRoot(&sd)){
                ++dir_depth;
                sd.chdir( &sd );
            }
        }
        else if (entry.isOpen()){
            ++dir_depth;
            dir[dir_depth] = entry;
            dir_index[dir_depth] = entry_index;
        }
        fetchEntry(0);
    }
}

/// This function leaves a subdirectory.
void leaveDir(){
    // leave only subdirectory
    if (dir_depth > 0){
        dir[dir_depth].close();
        entry_index = dir_index[dir_depth];
        --dir_depth;
        fetchEntry(entry_index);
        sd.chdir( dir_index[dir_depth] );
        sd.vwd()->rewind();
        
    } else {
      sd.chdir( &sd );
      sd.vwd()->rewind();
    }
}

/// This function handles the case where LEFT button is pressed.
void leftPressed(){
        leaveDir();
}

/// This function handles the case where UP button is pressed.
void upPressed(){
        fetchEntry(entry_index + 1);
}

/// This function handles the case where DOWN button is pressed.
void downPressed(){
        fetchEntry(entry_index - 1);
}
/// This function plays a WAV file.
void playWAV(){
  if(tmrpcm.isPlaying()){
    displayStop();
    tmrpcm.stopPlayback();   
    delay(800);       
  }else {
    displayPlay();
    tmrpcm.play(lfn);
    delay(10);        
  }
}

void playTAP(){
  digitalWrite(3, HIGH);

  digitalWrite(3, LOW);
}

/// This function handles the case where SELECT button is pressed.
void selectPressed(){
    switch (entry_type){
      case 0:
          sd.chdir( lfn );
          enterDir();       
          break;
      case 1:
        playWAV();
        break;
      case 2:
        playTAP();
        break;
      default:
        break;
    }
}

void recording(){
  loading=false;
  tmrpcm.stopPlayback();
  
  if(record==false){
     record=true;
     tmrpcm.startRecording("record.wav",16000,A5);
     lcd.clear();
     lcd.setCursor(0,0);
     lcd.print(F("record.wav"));
     lcd.setCursor(0,1);
     lcd.print(F("Record"));
     digitalWrite(3, HIGH);//loading LED ON
  } else {
    record=false;
    tmrpcm.stopRecording("record.wav");
    tmrpcm.disable();
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print(F("record.wav"));
    lcd.setCursor(0,1);
    lcd.print(F("Stop"));
    digitalWrite(3, LOW);//loading LED OFF
  }
}


void writeToSD(String line) {
   File dataFile = sd.open("print.txt", FILE_WRITE);
   if (dataFile) {
     dataFile.println(line); // Auf die SD-Karte schreiben
     dataFile.close();
     Serial.println(line); // Zusätzlich auf serielle Schnittstelle schreiben zum Debuggen
   }
   else {
     Serial.println("Error opening datafile");
   }
}


void setup(){
    Serial.begin(9600);
  
    setupDisplay();
    delay(500);
    pinMode(2, OUTPUT);         // LED 1 Device Ready
    pinMode(3, OUTPUT);         // LED 2 Device Loading

    pinMode(A0,INPUT);          // Taste 0 (up)
    digitalWrite(A0,HIGH);      // interner pullup Widerstand
    pinMode(A1,INPUT);          // Taste 1 (down)
    digitalWrite(A1,HIGH);      // interner pullup Widerstand
    pinMode(A2,INPUT);          // Taste 2 (select)
    digitalWrite(A2,HIGH);      // interner pullup Widerstand
    pinMode(A3,INPUT);          // Taste 3 (dir back)
    digitalWrite(A3,HIGH);      // interner pullup Widerstand
    pinMode(A4,INPUT);          // Taste 4 (record)
    digitalWrite(A4,HIGH);      // interner pullup Widerstand
    pinMode(A5, INPUT);         // Microphone
    digitalWrite(A5,HIGH); 
    
    tmrpcm.setVolume(2);
    tmrpcm.speakerPin = 11;
    
    pinMode(CSPin, OUTPUT);
    sd_ready = sd.begin(CSPin, SPI_FULL_SPEED);

    if (!sd_ready){
        sd.initErrorHalt();
        lcd.clear();
        lcd.print(F("No SD card"));
    } else {
      digitalWrite(2, HIGH);
      enterDir();
    }
}

void loop(){
    while (!Serial.available()){

      if(tmrpcm.isPlaying()&&!record) {      
          digitalWrite(3, HIGH);//loading LED ON
          loading=true;  
      }else{
          if(loading){   
            lcd.setCursor(0,1);
            lcd.print(F("               "));
            digitalWrite(3, LOW);
            loading=false;//loading LED OFF
          }
        }
     
        if(readButton(0)) {
          if(!tmrpcm.isPlaying()&&!record) {  
            fetchEntry(entry_index - 1);
          }                      
        }
    
        if(readButton(1)) {  
          if(!tmrpcm.isPlaying()&&!record) {                         
            fetchEntry(entry_index + 1);
          }                         
        }
        
        if(readButton(2)) {  
            if(!record) { 
              selectPressed();
            }   
        }
         
        if(readButton(3)) {
          if(!tmrpcm.isPlaying()&&!record) {   
            leaveDir();
          }
        }

        if(readButton(4)) {
          if(!tmrpcm.isPlaying()) { 
            recording();
          }
        }
     
        displayScrollingMessage();
    }

    if (Serial.available()) {
      String s = Serial.readString();
      String line = s + String("\n");
      writeToSD(line);
    }

    
}
