#include <SPI.h>
#include <SD.h>
#include "RTClib.h"

RTC_PCF8523 rtc;

const int CS_PIN = 10;                          //Chip Select Pin
const int SIGNAL_PIN = A7 ;                     //Sound Signal to read
const int LOGCONTROL_PIN = A1 ;                 //Control for logging frequency
const int INT_PIN = 2 ;                         //Interrupt for logging times
const int INDICATOR_PIN[] = {3, 4, 5} ;      //Indicator pins for the logging frequency

String fileName = "data.log" ;

String data = "" ;
long dataRaw = 0 ;
volatile bool dataFlag = true ;
long counter = 0 ;
long sampleNum = 1 ;
long avgNum = 60 ; 

long controlTimeStart = 0 ;
long controlTimeLen = 10 ;
int logControlData = 0 ;
int logControlState = 0 ;
int logControlStateLast = 1 ;

bool buzControlState = 0 ;
bool buzControlStateLast = 1 ;

volatile bool buzStateFlag = 0 ;
bool buzState = 0 ;
bool buzStateLast = 1 ;
volatile long buzStateTimeStart = 0 ;
long buzStateTimeLen = 1000 ;
long buzTimeStart = 0 ;
long buzTimeLen = 2000 ;




void setup() {
  // Open serial communications and wait for port to open:
  Serial.begin(9600);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }

  //Initializing RTC
  Serial.print("Initializing RTC...") ;
  if (! rtc.begin()) {
    Serial.println("Couldn't find RTC");
    Serial.flush();
    while (1) delay(10);
  }
  Serial.println("RTC initiialized.") ;

  if (! rtc.initialized() || rtc.lostPower()) {
    Serial.println("RTC is NOT initialized, setting the time!");
    // When time needs to be set on a new device, or after a power loss, the
    // following line sets the RTC to the date & time this sketch was compiled
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    // This line sets the RTC with an explicit date & time, for example to set
    // January 21, 2014 at 3am you would call:
    // rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));
    //
    // Note: allow 2 seconds after inserting battery or applying external power
    // without battery before calling adjust(). This gives the PCF8523's
    // crystal oscillator time to stabilize. If you call adjust() very quickly
    // after the RTC is powered, lostPower() may still return true.
  }
  rtc.start();
  Serial.println("RTC Started") ;


  Serial.print("Initializing SD card...");
  // see if the card is present and can be initialized:
  if (!SD.begin(CS_PIN)) {
    Serial.println("Card failed, or not present");
    // don't do anything more:
    while (1);
  }
  Serial.println("card initialized.");

  //Log that the board has restarted
  data = rtc.now().timestamp() ;
  data += "\t" ;
  data += "Reboot: Lost power... Restarting Now" ;
  writeSD(data) ;

  //Attach an interrupt to the RTC timer
  pinMode(INT_PIN, INPUT_PULLUP) ;
  attachInterrupt(digitalPinToInterrupt(INT_PIN), rtcInterrupt, FALLING) ;

  logControlRead() ;
}

void loop() {

  //Only update the control circuits every so often
  if (millis() - controlTimeStart >= controlTimeLen){
    controlTimeStart = millis() ;
    logControlRead() ;
  }

  //only read data if more logging is turned on
  if (sampleNum > 0){
    //Read the data only at specified intervals
    if (dataFlag){
      dataFlag = false ;
      //Start a countdown timer until the next data logging
      rtc.enableCountdownTimer(PCF8523_FrequencySecond, 1) ;

      dataRaw += analogRead(SIGNAL_PIN) ; 
      counter ++ ;
      if (counter >= sampleNum){
        //Always Start a log line with the time stamp
        data = rtc.now().timestamp() ;
        data += "\t" ;

        //Add averaged data to the log
        data += "Sound: " ;
        data += String((float)dataRaw/counter) ;

        //Write a line to the csv file
        writeSD(data) ; 

        //Reset the data raw for the next measurement 
        dataRaw = 0 ;
        counter = 0 ;
      }
    }
  }

}

//Wrapper to write a string t othe SD card
void writeSD(String line){
  // open the file. note that only one file can be open at a time,
  // so you have to close this one before opening another.
  File dataFile = SD.open(fileName, FILE_WRITE);

  // if the file is available, write to it:
  if (dataFile) {
    dataFile.println(line);
    dataFile.close();
    // print to the serial port too:
    Serial.println(line);
  }
  // if the file isn't open, pop up an error:
  else {
    Serial.println("error opening file");
  }
}

void logControlRead(){
  logControlData = analogRead(LOGCONTROL_PIN) ;
  logControlState = logControlData >> 8 ;
  Serial.println(logControlData) ;
  
  if (logControlState != logControlStateLast){
    logControlStateLast = logControlState ;

    switch(logControlState){
      case 0:
        sampleNum = 0 ;
        digitalWrite(INDICATOR_PIN[0], 0) ;
        digitalWrite(INDICATOR_PIN[1], 0) ;
        digitalWrite(INDICATOR_PIN[2], 0) ;
      break;
      case 1:
        sampleNum = 60 * avgNum ;
        digitalWrite(INDICATOR_PIN[0], 1) ;
        digitalWrite(INDICATOR_PIN[1], 0) ;
        digitalWrite(INDICATOR_PIN[2], 0) ;
      break;
      case 2:
      sampleNum = 10 * avgNum ;
        digitalWrite(INDICATOR_PIN[0], 0) ;
        digitalWrite(INDICATOR_PIN[1], 1) ;
        digitalWrite(INDICATOR_PIN[2], 0) ;
      break;
      case 3:
        sampleNum = 1 * avgNum ;
        digitalWrite(INDICATOR_PIN[0], 0) ;
        digitalWrite(INDICATOR_PIN[1], 0) ;
        digitalWrite(INDICATOR_PIN[2], 1) ;
      break;
    }

    //Log that the sample rate has changed
    data = rtc.now().timestamp() ;
    data += "\t" ;
    data += "Rate: " ;
    data += String(sampleNum) ;
    data += " Seconds per Sample" ;
    writeSD(data) ;
  }
}

//RTC ISR to start the data logging
void rtcInterrupt(){
  dataFlag = 1 ;
}




