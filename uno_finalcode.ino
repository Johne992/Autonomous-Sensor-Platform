/*
Code for the Uno, controlling the servo and esc. It also communicates
via I2C with the Mega, though it doesn't need to actually send
data to it.

Version 1.0
Date: 11/22/15
Latest Version: Works in the almost-full test, where everything
but the sonar module was tested on lab.
*/

#include <Servo.h>
#include <Wire.h>
#define ADR_UNO 5

Servo servo; 
Servo servoEsc; 

float angle; 
int flag_there;
int PIN_SERVO = 9;
int PIN_ESC = 5;

void setup() 
{
  // put your setup code here, to run once:
Serial.begin(115200);  
Wire.begin(ADR_UNO);
Wire.onReceive(receiveEvent);


angle = 90; 
flag_there = 0; 
servo.attach(PIN_SERVO); 
servoEsc.attach(PIN_ESC);
servoEsc.writeMicroseconds(1500);
delay(1000);

}

void loop() {
 

}
void receiveEvent(int howMany) {
  int leftPoint;
  int rightPoint; 
  String out ="";
  while (1 <= Wire.available()) { 
    char c = Wire.read(); 
    Serial.write(c);
    out = out + String(c);  
   }
   leftPoint = out.indexOf('<'); 
   rightPoint = out.indexOf('>',leftPoint +1);
   
   if(leftPoint != rightPoint && rightPoint > leftPoint)
   {
    out = out.substring(leftPoint+1,rightPoint); 
    leftPoint = out.indexOf(',');
    angle = (out.substring(0,leftPoint)).toFloat();
    flag_there = (out.substring(leftPoint+1, out.length())).toInt();
    Serial.println(flag_there);
    Serial.println("/");
    moveServo();
    Serial.println(angle);
   }
}

void moveServo(){
 servo.write(angle);
 startEsc(); 
}

void startEsc(){
  if(flag_there == 0){
    servoEsc.writeMicroseconds(1550);
  }
  else{
    servoEsc.writeMicroseconds(1500); 
  }
}