/*
Code taken from:
http://stackoverflow.com/questions/19729444/arduino-read-at-commands-from-quectel-m95-gsm-module
by David Barnes.

Modified by Cade
*/

#include <SoftwareSerial.h>

#define rxPin 10
#define txPin 11
#define STATPIN 9
#define PWKPIN 7
//

SoftwareSerial mySerial(rxPin,txPin); // RX, TX

void setup(){
  
  pinMode(rxPin, INPUT);
  pinMode(txPin, OUTPUT);
  pinMode(PWKPIN, OUTPUT);
  
  Serial.begin(9600);
  Serial.println("Arduino serial initialized!");
  delay(10);
  
  mySerial.begin(9600);
  Serial.println("Software serial initialized!");
  delay(10);
  
  //Turn the "phone" on
  Serial.println("Setting PWKPIN to HIGH");
  digitalWrite(PWKPIN, HIGH);
  Serial.println("Waiting 5000 ms");
  delay(500);
  Serial.println("Seeting PWKPIN to LOW. Then wait 25000 ms");
  digitalWrite(PWKPIN, LOW);
  delay(2500);
  Serial.println("Everything should be on.");
}

void loop(){
     //Send an AT command
    issueCommand("ATI");
    readSerial();
    delay(1000);
    
    //Set baud rate with AT command
    issueCommand("AT+IPR=9600");
    readSerial();
    delay(1000);
    
    //Store the baud rate
    issueCommand("AT&W");
    readSerial();
    delay(1000);
    
     while(true)
       readSerial();
 }
void readSerial(){
  char tempChar;
  int tempByte;
  
  while (mySerial.available())
    {
      tempByte = mySerial.read();
      tempChar = tempByte;
    Serial.print("Available Value: ");  Serial.print(mySerial.available());
    Serial.print("; Byte Value: ") ;    Serial.print(tempByte);
    Serial.print("; Char Value: ");     Serial.println(tempChar);
    
    if (mySerial.available() <= 0)
      Serial.println("Rx Msg Finished");
    }
}

void issueCommand(char* msg){
  mySerial.println(msg);
  Serial.print("Tx: ");
  Serial.println(msg);
  delay(10);
}