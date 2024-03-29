void Evas_Man()
 {
    // put your main code here, to run repeatedly:
    refHeading = cHeading;
  
    Left_Avoid();
  
    if (block == 1) 
     {
      Rigth_Avoid();
     }

    elif (block == 2)
     {
      Serial.print(Help!);
     }

 }


///////////////////////////////////////////////////////
//Function to maneuver left and check for obstructions

void Left_Avoid()
 {
    dHeading = cHeading - 45;
    
    if (dHeading < 0)	        //Fixes heading if less than 0°
      {        
      dHeading += 360;         
      }
  
    while (cHeading != dHeading)	//run until desired heading matches current heading
     {        
      angle -= 1;
      servo.write(angle);
      servoEsc.writeMicroseconds(1600);
     }
  
    servoEsc.writeMicroseconds(1500);

    sonar();                //See if anything's in the way

    if (in>=120)
     {
      block = 2;
     }

    else
     {
      block = 0;
     }

    else 
     {   
     }
 }


///////////////////////////////////////////////////////
//Function to maneuver right and check for obstructions

void Right_Avoid()
 {
   dHeading = cHeading + 45;
  
    if (dHeading > 360)		//fixes heading if greater than 360°
     {        
      dHeading -= 360;         
     }
  
    while (cHeading != dHeading)	//run until desired heading matches current heading
     {        
      angle += 1;
      servo.write(angle);
      servoEsc.writeMicroseconds(1400);
     }
  
    servoEsc.writeMicroseconds(1500);
  
    dHeading = cHeading + 45;
    
    if (dHeading > 360)		//fixes heading if greater than 360°
     {       
      dHeading += 360;          
     }
  
    while (cHeading != dHeading)	//run until desired heading matches current heading
     {        
      servoEsc.writeMicroseconds(1600);
     }
  
    servoEsc.writeMicroseconds(1500);

    sonar();                //See if anything's in the way
  
    if (in<=120)
     {
      block = 2;
      //Serial.print(Help!);
     }

    else 
     {
      block = 0;
     }

    while (block = 1)
     {
      servoEsc.writeMicroseconds(1500);
     }
 }

/////////////////////////////////////////////
//Actual SONAR code

void sonar()
 {

 //Initializes digital output pin VCC(2) for use. The GND(5) pin 
 //..doesn't have to be declared as pins default to LOW
 //digitalWrite(VCC, HIGH);
 
 //From the HC-SR04 info sheet:
 // The PING is triggered by a HIGH pulse of 2 or more microseconds.
 // Give a short LOW pulse beforehand to ensure a clean HIGH pulse:

 pinMode(Trig, OUTPUT); //digital output
 digitalWrite(Trig, LOW);
 delayMicroseconds(2);  //clean-up the pulse
 digitalWrite(Trig, HIGH);
 delayMicroseconds(5);  // actual PING
 digitalWrite(Trig, LOW);

 // The same pin is used to read the signal from the PING: a HIGH
 // pulse whose duration is the time (in microseconds) from the sending
 // of the ping to the reception of its echo off of an object.

 pinMode(Echo,INPUT);
 duration = pulseIn(Echo, HIGH); //get the echo/return of sound

 //Used to convert the return time of the echo into a distance
 in = microsecondsToInches(duration);

 delay(1000);
 }