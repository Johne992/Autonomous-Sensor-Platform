/* 
This is the integrated code for the Mega for our sensior design
project. The Mega is interfaced with the GSM, GPS, temperature, 
compass, Bluetooth and Uno.

Version 1.37.5
12/06/2015
Latest Changes:
1.37.5
Removed some unneeded comments.
1.37.4
Added temperature sensor back to the sendGet() function.
1.37.3
Added flag_cmd func to issueCommand3.
1.37.2
issueCommand3 hotfix
Commented out setupModem before sending samples.
Full system working.
1.37.1
BatteryPin variable now in use. Multiple send reads will be sent (at the start) if the resulting dLat is 0.

1.37
The GSM2 Click failed on 12/05 and seemed unrepairable.
It's been replaced with a Seeedstudio GPRS Shield.
It sends data via get (Couldn't get HTTP POST to work quickly enough)
and is still able to receive destination coordinates from website.
It also works off of hardware serial but the baud rate is higher than the click's.
issueCommand3() was added as a new and improved issueCommand2 that
can handle the new AT commands well. It also incorporates issueAndSave into its design.

*/

//Include libraries
#include <SPI.h>
#include "Adafruit_BLE_UART.h"
#include <SoftwareSerial.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Wire.h>
#include <TinyGPS++.h>
#include <math.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_HMC5883_U.h>
#include "compass.h"
#include <NewPing.h>

//Define Pins
#define ADR_UNO 5
#define rxPin 11
#define txPin 10
#define ADAFRUITBLE_REQ 3
#define ADAFRUITBLE_RDY 2     // This should be an interrupt pin, on Uno thats #2 or #3
#define ADAFRUITBLE_RST 49

#define STATPIN 9
#define TEMPPIN 4
#define BATTERYREADPIN 4
#define Trig 22
#define Echo 23
#define Sonar_Max 200

//Object creation
Adafruit_HMC5883_Unified mag = Adafruit_HMC5883_Unified(12345);
NewPing sonar(Trig, Echo, Sonar_Max);
OneWire oneWire(TEMPPIN);
DallasTemperature sensors(&oneWire);
DeviceAddress ourThermometer = {0x28, 0x09, 0x25, 0x50, 0x06, 0x00, 0x00, 0xAB };
TinyGPSPlus gps;
SoftwareSerial ss(rxPin,txPin);
Adafruit_BLE_UART uart = Adafruit_BLE_UART(ADAFRUITBLE_REQ, ADAFRUITBLE_RDY, ADAFRUITBLE_RST);

//GSM Variables
#define SAMPLING_NUMBER 2
#define SAMPLING_PERIOD 5  //Seconds between samples. 
#define MAX_ATTEMPTS 10    //Max amount of attempts to make when issuing commands
int CMD_TRIES = 0;
String POSTPASS = "ST2015";
String POSTURL = "http://stumpthumpers.comuv.com/tx.php";
String READURL = "http://stumpthumpers.comuv.com/destination.php";
String GPSlat_target = "20.2000";
String GPSlong_target = "-90.1010";
long startTime;          //Timer to be used in issueCommand2()
long durationTime = 3000;
long currentTime;
long startTime_read;     //Timer to be used for periodically sending reads
long durationTime_read = 60000; //in ms
boolean flag_there = false;
//boolean data_sent = false;
boolean flag_cmd = false;
boolean flag_targetChange = false;
int samplesTaken = 0;

//GPS Variables
double dLat;
double dLng;
double cLat; 
double cLng;
uint32_t cCourse; 
double deltaLat = 10;
double deltaLng = 10; 

//dummy points for current 
double dummyLat = 30.1234;
double dummyLong = -70.5567;
static const uint32_t dummyCourse = 90; 

//dummy points for destination
double dummyDestLat = 31.3412;
double dummyDestLong = -91.3412;

int isVal = 0; 

boolean dummySet = false;
String tmplat;
String tmplng; 
#define MAX_DISTANCE 1000  //(m) Max distance to allow the device to travel
#define DISTANCE_ACCURACY 20   //(m) How far we want to be from the destination 
float distanceFrom = 0;     //(m) How far we are from the destination

//Bluetooth variables
String bteLat = "";
String bteLng = "";
aci_evt_opcode_t laststatus = ACI_EVT_DISCONNECTED;

//Servo Variables
float angle;
int SERVO_MIN = 65;    //Limits (in degrees)
int SERVO_MAX = 140;

//Status Variables
int STATUS_BTLE = 0;
int STATUS_GSM = 0;
int STATUS_TEMP = 0;
int STATUS_GPS = 0;
int STATUS_COMPASS = 0;
int STATUS_UNO = 0;
int STATUS_SONAR = 1;
int STATUS_OBJ = 0;

void setup() {
  //Communication with Computer through UART0
  Serial.begin(115200);
  delay(10);

  //Communication with GSM through UART2
  Serial2.begin(19200);
  delay(10);
  if (digitalRead(STATPIN) == LOW)
    digitalWrite(STATPIN, HIGH);  //Turn on Modem

  //Communication with GPS through SS
  ss.begin(9600);
  delay(10);
  
  sensors.begin();
  sensors.setResolution(ourThermometer, 10);
  
  Wire.begin();
  uart.setRXcallback(rxCallback);
  uart.setACIcallback(aciCallback);
  uart.begin();

  compass_x_gainError = 0.91;
  compass_y_gainError = 0.96;
  compass_z_gainError = 0.99;
  compass_x_offset = -274.10;
  compass_y_offset = 85.29;
  compass_z_offset = 1252.43;
}

void loop() {
  float batteryRead = analogRead(BATTERYREADPIN);
  batteryRead = batteryRead * (0.0049) * 11;
  //Set up the Temperature sensor and GSM modem.
  uart.pollACI();
  //Setup devices
  blueOut("Blvl: " + String(batteryRead));
  setupTemp();
  setupCompass();
  checkSlave(ADR_UNO, false);    //Check if Uno is connected.
  setupGPS(2, 20);
  Serial.println("Setting up modem:");
  setupModem();
  
  //Execute a read to get the target location
  Serial.println("READ WEBSITE TO GET NEW TARGET COORDINATES");
  blueOut("Getting dest. data");
  while(dLat == 0)
    {
    sendRead();
    if (dLat == 0)
      {
       uart.pollACI();
       blueOut("Error getting a dest.");
       blueOut("Trying again ..."); 
      }
     delay(500);
    }
  blueOut("Got dest. data");  
  delay(1000);
  startTime_read = millis();
  while(true)
    {
      uart.pollACI();
      //Get current location from GPS
      setPoints();
      uart.pollACI();
      updateDistance();
      if (distanceFrom <= DISTANCE_ACCURACY)
      {
        uart.pollACI();
        Serial.println("Arrived at Destination");
        blueOut("Arrived!");
        flag_there = 1; 
        sendToUno(ADR_UNO);
        while(flag_there == 1)
          {
           //Send Samples
           if (samplesTaken == 0)
             {
              blueOut("Sending Samples"); 
              for (int k=0; k<SAMPLING_NUMBER; k++)
                {
                    uart.pollACI();
                    Serial.println("SENDING SAMPLE " + String(k));
                    blueOut("Sending sample " + String(k+1));
                    sendGet();
                    samplesTaken++;
                      if (k < SAMPLING_NUMBER-1)
                        delay(SAMPLING_PERIOD*1000);  //Wait sampling period.
                }  
              uart.pollACI();
              Serial.println("SAMPLES SENT");
              blueOut("Samples sent");
              startTime_read = millis();
              }
            
            //flag_targetChange is updated by sendRead if the read gps location is new
            if (millis() > startTime_read + durationTime_read)
              {
                uart.pollACI();
                sendRead();
                if (flag_targetChange == 1)
                  {
                    flag_there = 0;
                  }
                startTime_read = millis();
                Serial.println("READ CMD SENT");
                blueOut("Read for new dest.");
              } 
          }
      }
      else{
        uart.pollACI();
        
        //Get our heading
        if (STATUS_COMPASS == 1)
          cCourse = getHeading();
        else
          cCourse = gps.course.deg();
      
        //Calculate the servo angle
        calculateServo();
      
        //Send data to the Uno
        flag_there = 0; 
        sendToUno(ADR_UNO);
      }
      readResponse();
      delay(500);
    }
}

/*
    START OF TRANSMISSION-SPECIFIC FUNCTIONS
*/

String getSignalStrength(const char* msg) {
   String out = issueAndSave(msg);
   int leftPoint = out.indexOf(":");
   int rightPoint = out.indexOf(",");
   if (rightPoint > leftPoint)
     out = out.substring(leftPoint+1,rightPoint);
   else
     out = "-1";
   return out;
}

String issueAndSave(const char* msg) {
   String out = "";
   char in_c;
   Serial2.println(msg);
   delay(500);
   while (Serial2.available()){
    in_c = Serial2.read();
    Serial.write(in_c);
    out = out + String(in_c);
    delay(10);
  }
  return out;
}

void issueCommand(const char* msg) {
  Serial2.println(msg);
  delay(500);
  readResponse();
  delay(500);
}

String issueCommand3(const char* msg, const char* resp, long t) {
  //msg is the command to send
  //resp is the desired response
  //t is the timeout (ms)
  uart.pollACI();
  Serial2.flush();        //Wait for outgoing serial data to finish
  long tB = millis() + t;
  String out = "";
  int test;
  boolean check = false;
  boolean tried = false;
  char in_c;
  //Try only a limited number of commands
  while(CMD_TRIES <= MAX_ATTEMPTS)
  {
    //Send command
    Serial2.println(msg);
    //Wait t for ideal response
  while(millis() < tB)
    {
    uart.pollACI();
    delay(500);
    while (Serial2.available()){
        in_c = Serial2.read();
        Serial.write(in_c);
        out = out + String(in_c);
        delay(10);
      }
    test = out.indexOf(resp);
    if (test >= 0)
      {
       //Found the intended response.
       check = true;
       return out;
       break; 
      }
    }
    //Waited t. Try again
    if (check == false)
     {
      delay(500);
      CMD_TRIES += 1;
      Serial.println("Try again");
      tB = tB + t;
     }
    //If correct
    if (check == true)
      {
       CMD_TRIES = 0;  //Reset CMD_TRIES
       flag_cmd = false;
       break; 
      }
  }
  //No more tries
  if (check == false)
    {
     Serial.println("Tried max attempts."); 
     flag_cmd = true;
     CMD_TRIES = 0;
    }
}
  
void issueCommand2(const char* msg, const char* resp) {
  String checkFor = resp;
  int checkForLength = checkFor.length();
  char output[checkForLength];
  String outStr;
  int i = checkForLength - 1;
  byte in_b;
  boolean canGo = false;
  boolean sentData = false;
  flag_cmd = false;
  startTime = millis();
  while (canGo == false)
  {
  if (checkForLength <= 1)
  {
    canGo = true;
    Serial2.println(msg); 
    delay(500);
    readResponse();
    delay(500);
  }
  else
  {
    if (sentData == false)
      {
        sentData = true;
        Serial2.println(msg);
        //canGo = true;  //Don't want to just endlessly just resend the command. Give up after three attempts.
      }
    delay(500);
    while (Serial2.available()) {
       in_b = Serial2.read();
       Serial.write(in_b);
       if (in_b != 0 && in_b != '\n' && in_b != '\r' && in_b !='\n\r' && in_b != '\r\n')
       {
          if (i > 1)
          {
          for(int k=1; k < i; k++)
            {
              output[k-1] = output[k];
            }
          output[i] = in_b;
          }else
          {
            output[0] = output[1];
            output[1] = in_b;
          }
       }
       delay(10); 
    }
    String test;
    for (int k=0; k < checkForLength; k++)
      {
        test = test + String(output[k]);
      }
    //Serial.println("Output: " + test);
    if (test == checkFor)
      {
       canGo = true;
      }
 
    if (millis() > startTime + durationTime)
      {
        //Time's up
        flag_cmd = true;
        canGo = true;
      }
 }
 
 }
}

void readResponse() {
  uart.pollACI();
  while (Serial2.available()){
    if (Serial2.available() >= 64)
      {
        Serial.println("Overflooooow");
      }
    Serial.write(Serial2.read());
    delay(10);
  }
}

/*
Sends an HTTP POST request to the website. The url for the form is defined by POSTURL. (Which is currently the same as GETURL
The TEMPVALUE is obtained within the function (like with sendGet(), using getTemperature)
Lat and Long are saved within the global variables.

result is the url to be supplied to AT+QHTTPURL=n,m where n is the size (in bytes) of result

*/
/*
void sendPost() {
  uart.pollACI();
  String result = POSTURL;  
  String resultInfo = "AT+QHTTPURL=" + String(result.length()) + ",30";
   String dataToSend = "msg=" + String(getTemperature(ourThermometer));
  dataToSend = dataToSend + "||" + String(cLat,6);
  dataToSend = dataToSend + "||" + String(cLng,6);
  dataToSend = dataToSend + "||" + GPSlat_target;
  dataToSend = dataToSend + "||" + GPSlong_target;
  dataToSend = dataToSend + "||" + POSTPASS;
  String postInfo = "AT+QHTTPPOST=" + String(dataToSend.length()) + ",50,10";
  
  //Convert the above strings to const char *, which is what the issueCommand function requires
  const char * resultp = result.c_str();
  const char * resultInfop = resultInfo.c_str();
  const char * dataToSendp = dataToSend.c_str();
  const char * postInfop = postInfo.c_str();

  //Issue the AT commands  
  issueCommand("AT+QIFGCNT=0");
  issueCommand("AT+QIDNSIP=1");
  issueCommand("AT+QICSGP=1,\"truphone.com\",\"\",\"\""); //Set the APN
  issueCommand(resultInfop);
  issueCommand(resultp);
  issueCommand2(postInfop,"CONNECT");
  issueCommand2(dataToSendp,"OK");
  //issueCommand("AT+QHTTPREAD=30"); //Send the read request. Not currently in use. 
  issueCommand("AT+QIDEACT");
  
}
*/
void sendGet() {
  uart.pollACI();
  //Create the URL to send the HTTP GET request to.
  String result = POSTURL;
  result = "AT+HTTPPARA=\"URL\",\"" + result + "?tmp=";
  result = result + String(getTemperature(ourThermometer));
  result = result + "&clat=" + String(cLat,6);
  result = result + "&clong=" + String(cLng,6);
  result = result + "&dlat=" + GPSlat_target;
  result = result + "&dlong=" + GPSlong_target;
  result = result + "&pw=" + POSTPASS + "\"";
  const char * resultp = result.c_str();
  issueCommand3("AT+HTTPINIT","OK",5000);
  issueCommand(resultp);
  issueCommand3("AT+HTTPACTION=0","HTTPACTION:0,200",10000);  //Send Get
  issueCommand("AT+HTTPREAD");
  issueCommand("AT+HTTPTERM");
}

void sendRead() {
  uart.pollACI();
  //Create the URL to send the HTTP GET request to.
  String result = READURL;
  String result2;
  //Prepare the AT command that declares the amount of bytes of the url to send the request to and the max time to send (in seconds)
  result = "AT+HTTPPARA=\"URL\",\"" + result + "\"";
  //Convert the above string to const char *, which is what the issueCommand function requires
  const char * resultp = result.c_str();
  
  String output;
  //char outChar[ ];
  boolean gettingLL = false;
  boolean writingLat = false;
  int leftPoint = 0;
  int rightPoint = 0;
  
  //Issue the AT commands to start HTTP
  issueCommand3("AT+HTTPINIT","OK",5000);
  issueCommand(resultp);
  delay(2000);
  issueCommand3("AT+HTTPACTION=0","HTTPACTION:0,200",10000);  //Send Get
  output = issueCommand3("AT+HTTPREAD","]",10000);  //Send the read request.
  leftPoint = output.indexOf('[');
  rightPoint = output.indexOf(']');
  if (leftPoint != rightPoint && rightPoint > leftPoint)
    {
    output = output.substring(leftPoint+1,rightPoint);
    leftPoint = output.indexOf(',');    // Is now a middle point
    result = output.substring(0,leftPoint);
    result2 = output.substring(leftPoint+1, output.length());
    //Serial.println("R: " + result);
    //Serial.println("R2: " + result2);
    if (result.equals(GPSlat_target) && result2.equals(GPSlong_target))
      {
        //No change to the target location
        flag_targetChange = false;
      }
    else 
      {
        //There's been a change to the target location
        GPSlat_target = result;
        GPSlong_target= result2;
        
        dLat = GPSlat_target.toFloat();
        dLng = GPSlong_target.toFloat();
        blueOut("dLat: " + GPSlat_target);
        blueOut("dLng: " + GPSlong_target);
        flag_targetChange = true;
        samplesTaken = 0; // Reset the samples taken so we can take more at the new location.
        Serial.println("TARGET LOCATION CHANGED: samplesTaken = 0");
      }
    }
  issueCommand("AT+HTTPTERM");
   
}

void setupModem() {
 delay(7000);
 Serial2.flush();
 Serial.flush();
 issueCommand3("AT","OK",2000);
 while (flag_cmd)
  {
    //Modem not communicating properly, try again
    issueCommand3("AT","OK",2000);
    blueOut("Check Modem wiring.");
    STATUS_GSM = 0;
    delay(500);
  }
  STATUS_GSM = 1;
  Serial.println("Modem and Mega communication established.");
  //Issue the AT commands to connect to service
  issueCommand3("AT+CSTT=\"truphone.com\",\"\",\"\"","OK",5000);
  issueCommand3("AT+SAPBR=3,1,\"APN\",\"truphone.com\"","OK",5000);
  issueCommand3("AT+SAPBR=3,1,\"USER\",\"\"","OK",5000);
  issueCommand3("AT+SAPBR=3,1,\"PWD\",\"\"","OK",5000);
  delay(3000);
  issueCommand3("AT+SAPBR=1,1","OK",5000);
  String csq = getSignalStrength("AT+CSQ");
  blueOut("Modem on, SQ: " + csq);
}

/*
    END OF TRANSMISSION-SPECIFIC FUNCTIONS
*/

/*
    START OF TEMPERATURE-SPECIFIC FUNCTIONS
*/

float getTemperature(DeviceAddress deviceAddress)
{
  
  float tempC = sensors.getTempC(deviceAddress);
  delay(100);
  float tempC2 = sensors.getTempC(deviceAddress);
  if (tempC2 != tempC)
    {
      delay(300);
      tempC = sensors.getTempC(deviceAddress);
    }
  return tempC;
}
 
void setupTemp() {
  delay(2000);
  float tempC = getTemperature(ourThermometer);
  delay(500);
  float tempC2 = getTemperature(ourThermometer);
  //85 degrees C is the value after booting up. Can also occur
  //if there's a power issue (i.e., not enough to temp)
  while (tempC2 == 85 || tempC2 == -127)
   {
     if (tempC2 == 85 || -127)
       {
       Serial.println(String(tempC2) + ". Possible power issue.");
       blueOut("Tmp Error: " + String(tempC2));
       }
        // report parasite power requirements
        Serial.print("Parasite power is: "); 
        if (sensors.isParasitePowerMode()) Serial.println("ON");
        else Serial.println("OFF");
        
        Serial.print(sensors.getResolution(ourThermometer), DEC); 
        
        Serial.print("Requesting temperatures...");
        sensors.requestTemperatures(); // Send the command to get temperatures
        Serial.println("DONE");
        
           tempC2 = getTemperature(ourThermometer);
           delay(500);
   } 
   Serial.println("Temperature sensor is online with temp " + String(tempC));
   Serial.println();
   blueOut("Temperature Working.");
   STATUS_TEMP = 1;
}

/*
    END OF TEMPERATURE-SPECIFIC FUNCTIONS
*/

/*
    START OF I2C-SPECIFIC FUNCTIONS
*/

void checkSlave(int a, boolean b) {
  boolean slaveUp = false;
  boolean check = b;
  while (slaveUp == false)
  {
  Wire.beginTransmission(a);
  if (Wire.endTransmission() == 0)
    {
      if (check == true)
        {
         if (STATUS_UNO == 0)
          {
            STATUS_UNO = true;
            sendToUno(ADR_UNO);
            Serial.println("Connection restored. Resending data ... ");
          }
        }
      else
        {
        STATUS_UNO = true;
        }
      slaveUp = true;
    }else
    {
      STATUS_UNO = false;
      Serial.println("Slave not found ... Trying again");
      blueOut("Uno not found ...");
      delay(2000);
    }
  delay(500);
  }
}

void sendToUno(int a) {
 uart.pollACI();
 String angle_out = String(angle);
 String fthere_out = String(flag_there);
 
 const char * outp = angle_out.c_str();
 const char * ftherep = fthere_out.c_str();
 
 Wire.beginTransmission(a);
 Wire.write("<");
 Wire.write(outp);
 Wire.write(",");
 Wire.write(ftherep);
 Wire.write(">");
 if (Wire.endTransmission() == 0)
   {
    //Successfully sent
    STATUS_UNO = 1; 
   }
 else
   {
    //Failure to successfully send
    STATUS_UNO = 0;
    blueOut("Modem lost, recon..."); 
    checkSlave(ADR_UNO,true);
   }
}

/*
    END OF I2C-SPECIFIC FUNCTIONS
*/

/*
    START OF GPS-SPECIFIC FUNCTIONS
*/

void setDummys(){
  String tmpout = "";
  int tmpLeftPoint, tmpRightPoint;
  char c;
  while (tmplat.equals(""))
  {
  while (Serial.available() > 0) 
    {  
    c = Serial.read(); 
    if (c != '\n' && c != '\r' && c != '\n\r' && c != '\r\n' && c != 0)
      {
        tmpout = tmpout + String(c);
      }
     }
   
   if (!tmpout.equals(""))
       {
       tmpLeftPoint = tmpout.indexOf('<'); 
       tmpRightPoint = tmpout.indexOf('>');
       if (tmpRightPoint > tmpLeftPoint)
         {
         tmpout = tmpout.substring(tmpLeftPoint+1,tmpRightPoint); 
         tmpLeftPoint = tmpout.indexOf(',');
           if (tmpLeftPoint > 0)
             {
             tmplat = tmpout.substring(0,tmpLeftPoint);
             tmplng = tmpout.substring(tmpLeftPoint+1, tmpout.length());
             dummyLat = tmplat.toFloat();
             dummyLong = tmplng.toFloat();
                 
             tmpout = "";
             dummySet = true;
             }
         }
       }
  }

}
void setPoints(){
  // Set current Lat and Lng and print 
  if(gps.location.isValid()){
    STATUS_GPS = 1;
    cLat = gps.location.lat();
    cLng = gps.location.lng();
    Serial.println("Receiving Vaild GPS Coordinates..."); 
    isVal = 1; 
  }
  else{
  Serial.println("Not Receiving vaild GPS Coordinates. Enter Dummy Variables via Bluetooth:");
   STATUS_GPS = 0;
   bteLat = "";
   blueOut("Enter cGPS");
   while(bteLat.equals(""))
      {
        uart.pollACI();
      }
    //Serial.println("String to format: " + String(bteLat));
    formatBteResponse(bteLat);
    Serial.println("Lat to use: " + String(cLat,6) + "\nLng to us: " + String(cLng,6));
    delay(100);
    //Serial.println("Lat: " + String(gps.location.lat(),6));
    bteLat = "";
    bteLng = "";
  }
 
deltaLat=dLat-cLat;
deltaLng=dLng-cLng; 
}

void updateDistance() {
 if (STATUS_GPS == 1)
   {
     cLat = gps.location.lat();
     cLng = gps.location.lng();
   }
 distanceFrom = gps.distanceBetween(cLat, cLng, dLat, dLng);  
}

void calculateServo(){
  uart.pollACI();
  float pAngle = angle;
  
  collisionCheck();
  
 //initial caclculations
 double rad=3.14/180;
 int theta=round(atan(deltaLng/deltaLat)/rad);

 //figure out if theta needs to be negative or positive
 if(cLat>dLat)
 {
 theta=theta-180;
 } 
 theta=theta-cCourse;
 theta=(theta+360)%360; 
 
 if(theta>180){
 theta=theta-360;
 } 
 else if(theta < -180){
 theta=360+theta; 
 }
 
 if(theta >90){
 angle=180;
 }
 else if(theta < -90){
 angle=0;
 }
 else{
 angle=90+theta;
 }

//Have to invert the angle
angle = 180 - angle;

if (angle > SERVO_MAX)
  angle = SERVO_MAX;
if (angle < SERVO_MIN)
  angle = SERVO_MIN;
Serial.println("Angle is " + String(angle));
updateDistance();
Serial.println("cLat is " + String(cLat));
Serial.println("cLng is " + String(cLng));
Serial.println("Theta is " + String(theta));
Serial.println("cLat: " + String(cLat,6) + ", cLng: " + String(cLng,6));
if (gps.location.isValid())
  {
    Serial.print("cLat: "); printFloat(cLat,gps.location.isValid(), 12, 6);
    Serial.println();
    Serial.print("cLng: "); printFloat(cLng,gps.location.isValid(), 12, 6);
    Serial.println();
  }
else
  {
    Serial.println("cLat: " + String(cLat,6)); 
    Serial.println("cLng: " + String(cLng,6));
  }
Serial.println("dLat: " + String(dLat,6) + ", dLng: " + String(dLng,6));
Serial.println("Distance Left: " + String(distanceFrom) + " m");


uart.pollACI();
if (angle != pAngle)
  {
   Serial.println("Angle: " + String(angle,4));
   blueOut("Angle: " + String(angle,4));
  }
}

void setupGPS(long t, int tInt)  {
  Serial.println("Setup GPS");
  long timerStart = millis(); //ms
  long timerDelay = 1L;  //Minutes to give GPS to find satellites
  timerDelay = timerDelay * t * 1000 * 60;  //ms
  int printInterval = tInt;
  int i = 0;
  long currentTime;
  long remainingTime;
  
  while (!gps.location.isValid())
    {
      uart.pollACI();
      //Get new lat and long value to update isValid;
      printInt(gps.satellites.value(), gps.satellites.isValid(), 5);
      printInt(gps.hdop.value(), gps.hdop.isValid(), 5);
      printFloat(gps.location.lat(), gps.location.isValid(), 11, 6);
      printFloat(gps.location.lng(), gps.location.isValid(), 12, 6);
      printInt(gps.location.age(), gps.location.isValid(), 5);
      printDateTime(gps.date, gps.time);
      printFloat(gps.altitude.meters(), gps.altitude.isValid(), 7, 2);
      printFloat(gps.course.deg(), gps.course.isValid(), 7, 2);
      printFloat(gps.speed.kmph(), gps.speed.isValid(), 6, 2);
      printStr(gps.course.isValid() ? TinyGPSPlus::cardinal(gps.course.value()) : "*** ", 6);
      Serial.println();
      
      //Check connection issue
      if (millis() > 5000 && gps.charsProcessed() < 10)
        {
          Serial.println(F("No GPS data received: check wiring"));
          blueOut("Check GPS wiring.");
        }
      //Check if we've waited the desired amount of minutes before proceeding.
      if (millis() > (timerStart + timerDelay))
        {
          //Waited timerDelay minutes, time to quit trying.
          Serial.println("Waited " + String(timerDelay/(60L*1000L)) + " minutes. Switching to Bluetooth/Test Data from Serial.");
          blueOut("Waiting timed out.");
          break;
        }
        
      //Check if it's time to post another message to say we aren't receiving anything.
      if (millis() >= (timerStart+i*(timerDelay/printInterval)))
        {
          remainingTime = (timerStart + timerDelay) - (timerStart+i*(timerDelay/printInterval));
          i++;
          Serial.print("GPS still not receiving coordinates. "); 
          Serial.println(String(remainingTime /1000) + " seconds remaining. " + String(remainingTime/(1000L*60L)) + " minutes remaining.");
          blueOut("GPS Not Rx " + String(remainingTime /1000) + "s left");
        }
    }
  if (gps.location.isValid())
    {
    Serial.println("GPS is online. Lat: " + String(gps.location.lat(),6) + ", Long: " + String(gps.location.lng(),6));
    blueOut("GPS online");
    STATUS_GPS = 1;
    }
}

// This custom version of delay() ensures that the gps object
// is being "fed".
static void smartDelay(unsigned long ms)
{
  unsigned long start = millis();
  do 
  {
    while (ss.available())
      gps.encode(ss.read());
  } while (millis() - start < ms);
}

static void printFloat(float val, bool valid, int len, int prec)
{
  if (!valid)
  {
    while (len-- > 1)
      Serial.print('*');
    Serial.print(' ');
  }
  else
  {
    Serial.print(val, prec);
    int vi = abs((int)val);
    int flen = prec + (val < 0.0 ? 2 : 1); // . and -
    flen += vi >= 1000 ? 4 : vi >= 100 ? 3 : vi >= 10 ? 2 : 1;
    for (int i=flen; i<len; ++i)
      Serial.print(' ');
  }
  smartDelay(0);
}

static void printInt(unsigned long val, bool valid, int len)
{
  char sz[32] = "*****************";
  if (valid)
    sprintf(sz, "%ld", val);
  sz[len] = 0;
  for (int i=strlen(sz); i<len; ++i)
    sz[i] = ' ';
  if (len > 0) 
    sz[len-1] = ' ';
  Serial.print(sz);
  smartDelay(0);
}

static void printDateTime(TinyGPSDate &d, TinyGPSTime &t)
{
  if (!d.isValid())
  {
    Serial.print(F("********** "));
  }
  else
  {
    char sz[32];
    sprintf(sz, "%02d/%02d/%02d ", d.month(), d.day(), d.year());
    Serial.print(sz);
  }
  
  if (!t.isValid())
  {
    Serial.print(F("******** "));
  }
  else
  {
    char sz[32];
    sprintf(sz, "%02d:%02d:%02d ", t.hour(), t.minute(), t.second());
    Serial.print(sz);
  }

  printInt(d.age(), d.isValid(), 5);
  smartDelay(0);
}

static void printStr(const char *str, int len)
{
  int slen = strlen(str);
  for (int i=0; i<len; ++i)
    Serial.print(i<slen ? str[i] : ' ');
  smartDelay(0);
}

/*
    END OF GPS-SPECIFIC FUNCTIONS
*/

/*
    START OF COMPASS-SPECIFIC FUNCTIONS
*/

float getHeading() {
  sensors_event_t event;
  mag.getEvent(&event);
  float heading = atan2(event.magnetic.y, event.magnetic.x);
  // Once you have your heading, you must then add your 'Declination Angle', which is the 'Error' of the magnetic field in your location.
  // Find yours here: http://www.magnetic-declination.com/
  // Ours is 0° 5' or 0.08333 degrees or (pi/2160) rad
  float declinationAngle = (1.454)/1000;
  heading += declinationAngle;
  
  // Correct for when signs are reversed.
  if(heading < 0)
    heading += 2*PI;
    
  // Check for wrap due to addition of declination.
  if(heading > 2*PI)
    heading -= 2*PI;
   
  // Convert radians to degrees for readability.
  float headingDegrees = heading * 180/M_PI;
  
  //Calibrated result
  compass_heading();
  float bearing2 = bearing - 180;
  if (bearing2 < 0)
    bearing2 = bearing2 + 360;
  return bearing2 + 0.0833;

}

void setupCompass() {
   if(!mag.begin())
      {
        STATUS_COMPASS = 0;
        /* There was a problem detecting the HMC5883 ... check your connections */
        Serial.println("Ooops, no HMC5883 detected ... Check your wiring!");
        blueOut("Compass Not Found");
        blueOut("Using GPS Instead");
      }
   STATUS_COMPASS = 1;
   Serial.println("Compass Found");
   blueOut("Compass Found.");
   delay(500); 
}

/*
    END OF COMPASS-SPECIFIC FUNCTIONS
*/
/*
    END OF BLUETOOTH-SPECIFIC FUNCTIONS
*/
void aciCallback(aci_evt_opcode_t event)
{
  switch(event)
  {
    case ACI_EVT_DEVICE_STARTED:
      Serial.println(F("Advertising started"));
      break;
    case ACI_EVT_CONNECTED:
      Serial.println(F("Connected!"));
      break;
    case ACI_EVT_DISCONNECTED:
      Serial.println(F("Disconnected or advertising timed out"));
      break;
    default:
      break;
  }
}

/**************************************************************************/
/*!
    This function is called whenever data arrives on the RX channel
*/
/**************************************************************************/
void rxCallback(uint8_t *buffer, uint8_t len)
{
  //Serial.print(F("Received "));
  //Serial.print(len);
  //Serial.print(F(" bytes: "));
  for(int i=0; i<len; i++)
    {
     bteLat += String((char)buffer[i]);
     //Serial.print((char)buffer[i]); 
    }

  //Serial.print(F(" ["));

  for(int i=0; i<len; i++)
  {
    //Serial.print(" 0x"); Serial.print((char)buffer[i], HEX); 
  }
  //Serial.println(F(" ]"));

  /* Echo the same data back! */
  uart.write(buffer, len);
}

//Interpret data from bluetooth
//Should receive lat, lng in the form <lat,lng>
void formatBteResponse(String s) {
   String out = s;
   //Serial.println("Received: " + s);
   int leftPoint = 0;
   int rightPoint = 0;
   leftPoint = s.indexOf('<');
   rightPoint = s.indexOf('>');
   if (rightPoint > leftPoint)
     {
     s = s.substring(leftPoint+1,rightPoint);
     //Serial.println("Arrows gone: " + s);
     rightPoint = s.indexOf(',');
     if (rightPoint > leftPoint)
       {
         bteLat = s.substring(0,rightPoint);
         bteLng = s.substring(rightPoint+1,s.length());
         cLat = bteLat.toFloat();
         cLng = bteLng.toFloat();
       }
      else
       {
         bteLat = "";
         bteLng = "";
       }
     }
   else
     {
       bteLat = "";
       bteLng = "";
     } 
}

void blueOut(String s) {
  uart.pollACI();
  uart.print(s);   
}

/*
    END OF BLUETOOTH-Specific FUNCTIONS
*/
/*
    START OF SONAR-Specific FUNCTIONS
*/

int sendSonar() {
 delay(50);
 int cm = sonar.ping_cm(); //actually in cm
 Serial.println("Cm: " + String(cm));
 return cm;
}

boolean collisionCheck() {
 int cm = sendSonar();
 if (cm > 0)
  {
    if (STATUS_OBJ == 0)
      {
        STATUS_OBJ = 1;
        blueOut("Object ahead!" + String(cm) + " cm!");
      }
     else
      {
        STATUS_OBJ = 1; 
      }
  }
  else
  {
    if (STATUS_OBJ == 1)
      {
        STATUS_OBJ = 0;
        blueOut("Object moved!"); 
      }
     else
      {
       STATUS_OBJ = 0; 
      }
  }
}