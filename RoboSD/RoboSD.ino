#include <Arduino.h>
#include <SdFat.h>      // Sketch > Include library > Manage libraries > search: SdFat, by Bill Greiman
#include <LiquidCrystal_I2C.h>
#include <TMRpcm.h>

#define SD0_MO          50     // SD MOSI
#define SD0_MI          51     // SD MISO
#define SD0_CK          52     // SD SCK
#define SD0_SS          53     // SS SS

#define DIR_DEPTH       3
#define SFN_DEPTH       13
#define LFN_DEPTH       100

#define SCROLL_SPEED  250    // Text scroll delay
#define SCROLL_WAIT   3000   // Delay before scrolling starts

TMRpcm tmrpcm;   // create an object for use in this sketch
LiquidCrystal_I2C lcd(0x27, 16, 2);

char        entry_type = 0;
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
char        cdir[13];
char        ldir[13];

byte          scroll_pos = 0;
unsigned long scroll_time = millis() + SCROLL_WAIT;

byte ifolder[8] = {
  B00000,
  B11000,
  B11111,
  B11111,
  B11111,
  B11111,
  B00000,
  B00000
};
byte ifile[8] = {
  B00111,
  B01011,
  B10011,
  B11111,
  B11111,
  B11111,
  B11111,
  B11111
};

/// This function displays a text on the first line with a horizontal scrolling if necessary.
void scrollText(char* text){
    if (scroll_pos < 0){ scroll_pos = 0; }
    char outtext[17];

    outtext[0] = entry_type ? (entry_type + 1) : lcd.write(byte(0));

    for (int i = 1; i < 16; ++i){
        int p = i + scroll_pos - 1;
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
  if(analogRead(pin) < 500) { // Analog Eingang abfragen
    delay(150);            
    if(analogRead(pin) < 500)
      return 1;               // war gedrückt
  }
  return 0;                   // war nicht gedrückt
}

bool checkForWAV(char *filename){
  auto ext = strlwr(filename + (strlen(filename) - 4));
  return
    !!strstr(ext, ".wav");
}

void setupDisplay(){ 
    lcd.init();
    lcd.backlight();
  
    lcd.createChar(0, ifile);
    lcd.createChar(1, ifolder);
  
    lcd.clear();
    lcd.print(F("ROBOTON SD"));
    delay(100);
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
        //sd.chdir(dir_index[dir_depth]);
        //sd.vwd()->rewind();
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
                fetchEntry(0);

                Serial.print("EnterDir Root:");
                Serial.println( "/");
                sd.chdir( &sd );
                sd.vwd()->rewind();
            }
        }
        else if (entry.isOpen()){
            ++dir_depth;
            dir[dir_depth] = entry;
            dir_index[dir_depth] = entry_index;
            fetchEntry(0);
        }
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

        //Serial.print("LeaveDir:");
        //Serial.println(dir_index[dir_depth]);
        sd.chdir( dir_index[dir_depth] );
        sd.vwd()->rewind();
        
    } else {    
      //Serial.print("Leave Root:");
      //Serial.println( "/");
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

/// This function handles the case where SELECT button is pressed.
void selectPressed(){
    switch (entry_type){
      case 0:
          //Serial.print("Dir:");
          //Serial.println(lfn);
          
          sd.chdir( lfn );
          sd.vwd()->rewind();
          enterDir();
          
          break;
      case 1:
        
        if(entry_type==1){playWAV();}
        break;
      default:
        break;
    }
}

void setup(){
    //Serial.begin(9600);
  
    setupDisplay();
    delay(500);
    pinMode(2, OUTPUT);         // LED 1 Device Ready
    pinMode(3, OUTPUT);         // LED 2 Device Loading

    pinMode(A0,INPUT);          // Taste 0 
    digitalWrite(A0,HIGH);      // interner pullup Widerstand
    pinMode(A1,INPUT);          // Taste 1
    digitalWrite(A1,HIGH); 
    pinMode(A2,INPUT);          // Taste 2
    digitalWrite(A2,HIGH); 
    pinMode(A3,INPUT);          // Taste 3
    digitalWrite(A3,HIGH);

    tmrpcm.setVolume(2);
    tmrpcm.speakerPin = 11;
    
    pinMode(SD0_SS, OUTPUT);
    sd_ready = sd.begin(SD0_SS, SPI_FULL_SPEED);

    if (!sd_ready){
        sd.initErrorHalt();
        lcd.clear();
        lcd.print(F("No SD card"));
    } else {
      digitalWrite(2, HIGH);
      enterDir();
    }
}

bool loading = false;

void loop(){
    while (!Serial.available()){

      if(tmrpcm.isPlaying()) {      
          //LED loading
          digitalWrite(3, HIGH);
          loading=true;  
      }else{
          if(loading){   
            lcd.setCursor(0,1);
            lcd.print(F("               "));
            digitalWrite(3, LOW);
            loading=false;
          }
        }
        
        if(readButton(0)) {
          if(!tmrpcm.isPlaying()) {  
            fetchEntry(entry_index - 1);
          }                      
        }
    
        if(readButton(1)) {  
          if(!tmrpcm.isPlaying()) {                         
            fetchEntry(entry_index + 1);
          }                         
        }
        
        if(readButton(2)) {  
            selectPressed();    
        }
         
        if(readButton(3)) {
          if(!tmrpcm.isPlaying()) {   
            leaveDir();
          }
        }
     
        displayScrollingMessage();
    }
}
