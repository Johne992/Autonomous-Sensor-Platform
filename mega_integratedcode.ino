/* 
This is intended to be the code for the Arduino Mega for our 
project. 
Version 0.92
11/3/2015
Latest Version: Sampling Number and Period tested and work. Logic works. 1 Min Delay between Reads while at location works (and is adjustable).
*/
#include <SoftwareSerial.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Wire.h>

#define rxPin 10
#define txPin 11

#define STATPIN 9
#define PWKPIN 7
#define TEMPPIN 4

#define SAMPLING_NUMBER 2
#define SAMPLING_PERIOD 5  //Seconds between samples. 

OneWire oneWire(TEMPPIN);
DallasTemperature sensors(&oneWire);
DeviceAddress ourThermometer = {0x28, 0x09, 0x25, 0x50, 0x06, 0x00, 0x00, 0xAB };

String GETURL = "http://stumpthumpers.comuv.com/tx.php";
String POSTURL = GETURL;
String READURL = "http://stumpthumpers.comuv.com/destination.php";

//Current lat and long
//The defined values are dummy values to be used for the purpose of testing the sending of GPS coordinates.
String GPSlat_present = "31.123456";
String GPSlong_present = "-91.123456";
String GPSlat_target = "";
String GPSlong_target = "";

SoftwareSerial mySerial(rxPin,txPin); // RX, TX

long startTime;
long durationTime = 3000;
long currentTime;

long startTime_read;
long durationTime_read = 60000; //in ms

boolean flag_there = false;
boolean data_sent = false;
boolean flag_cmd = false;
//boolean flag_txUno = true;
//boolean flag_rxUno = true;
boolean flag_unoPresent = false;
boolean flag_targetChange = false;

int samplesTaken = 0;

void setup() {
  // put your setup code here, to run once:
  pinMode(rxPin, INPUT);
  pinMode(txPin, OUTPUT);
  
  Serial.begin(9600);
  //Serial.println("Arduino serial initialized!");
  delay(10);

  mySerial.begin(9600);
  //Serial.println("Software serial initialized!");
  delay(10);
  
  sensors.begin();
  sensors.setResolution(ourThermometer, 10);
  
  Wire.begin();  //Address is optional for master
}

void loop() {
  delay(500);
  
  //Set up the Temperature sensor and GSM modem.
  setupTemp();
  setupModem();
  
  //Execute a read to get the target location
  Serial.println("READ WEBSITE");
  //sendRead();
  GPSlat_target = "31.1900";
  GPSlong_target = "-91.4490";
  //Send target location to the uno
  checkSlave(8, false);
  Serial.println("Sending to Uno ... ");
  sendToUno(8);
  delay(1000);
  startTime_read = millis();
  while(true)
    {
      Serial.println("REQUESTING FROM UNO");
      checkSlave(8, true);
      requestToUno(8,23);
      //Interpret result
      //Update flag_there
      if (flag_there == true)
        {
          Serial.println("AT THE LOCATION");
          //Send Samples
          if (samplesTaken == 0)
            {
              
              for (int k=0; k<SAMPLING_NUMBER; k++)
                {
                  Serial.println("SENDING SAMPLE " + String(k));
                  //sendPost();
                  samplesTaken++;
                    if (k < SAMPLING_NUMBER-1)
                      delay(SAMPLING_PERIOD*1000);  //Wait sampling period.
                }
                Serial.println("SAMPLES SENT");
                startTime_read = millis();
            }
          
          //Serial.println("READING ... ");
          //flag_targetChange is updated by sendRead if the read gps location is new
          //set up timer so read only happens every n seconds
          if (millis() > startTime_read + durationTime_read)
            {
              //sendRead();
              startTime_read = millis();
              Serial.println("READ CMD SENT");
            }
        }
      readResponse();
      delay(500);
    }
}

String issueAndSave(const char* msg) {
   String out = "";
   char in_c;
   mySerial.println(msg);
   delay(500);
   while (mySerial.available()){
    in_c = mySerial.read();
    Serial.write(in_c);
    out = out + String(in_c);
    delay(10);
  }
  return out;
}

void issueCommand(const char* msg) {
  mySerial.println(msg);
  delay(500);
  readResponse();
  delay(500);
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
    mySerial.println(msg); 
    delay(500);
    readResponse();
    delay(500);
  }
  else
  {
    if (sentData == false)
      {
        sentData = true;
        mySerial.println(msg);
        //canGo = true;  //Don't want to just endlessly just resend the command. Give up after three attempts.
      }
    delay(500);
    while (mySerial.available()) {
       in_b = mySerial.read();
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
  while (mySerial.available()){
    if (mySerial.available() >= 64)
      {
        Serial.println("Overflooooow");
      }
    Serial.write(mySerial.read());
    delay(10);
  }
}

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
  if (tempC == -127.00){
  //Serial.print("Error getting Temperature.");
  tempC = -999;
  }
  return tempC;
}

/*
Sends an HTTP GET request to the website. The url for the form is defined by GETURL
The final url to be used will be:
GETURL?tmp=TEMPVALUE&lat=LATVALUE&long=LONGVALUE
The TEMPVALUE is obtained within the function. The current GPS Coordinates are stored in 
the global variables, GPSlat_present and GPSlong_present

result is the url to be supplied to the Quectel M95 after sending the AT+QHTTPURL=n,m command where n is the size (in bytes) of result
resultInfop is the pointer that points to the string/char array of the AT+QHTTPURL command and resultp is the pointer for result.

*/
void sendGet() {
  //Create the URL to send the HTTP GET request to.
  String result = GETURL + "?tmp=" + String(getTemperature(ourThermometer)) + "&lat=" + GPSlat_present + "&long=" + GPSlong_present;
  //Prepare the AT command that declares the amount of bytes of the url to send the request to and the max time to send (in seconds)
  String resultInfo = "AT+QHTTPURL=" + String(result.length()) + ",30";
  //Convert the two above strings to const char *, which is what the issueCommand function requires
  const char * resultp = result.c_str();
  const char * resultInfop = resultInfo.c_str();
  
  //Issue the AT commands
  issueCommand("AT+QIFGCNT=0");
  issueCommand("AT+QIDNSIP=1");
  issueCommand("AT+QICSGP=1,\"truphone.com\",\"\",\"\""); //Set the APN
  
  issueCommand(resultInfop);
  issueCommand(resultp);
  issueCommand2("AT+QHTTPGET=60","OK");
  //delay(5000);
  issueCommand("AT+QHTTPREAD=30");  //Send the read request. Not currently in use.  
  delay(2000);
  issueCommand("AT+QIDEACT");
  
}

/*
Sends an HTTP POST request to the website. The url for the form is defined by POSTURL. (Which is currently the same as GETURL
The TEMPVALUE is obtained within the function (like with sendGet(), using getTemperature)
Lat and Long are saved within the global variables.

result is the url to be supplied to AT+QHTTPURL=n,m where n is the size (in bytes) of result

*/
void sendPost() {
  String result = POSTURL;  
  String resultInfo = "AT+QHTTPURL=" + String(result.length()) + ",30";
   String dataToSend = "msg=" + String(getTemperature(ourThermometer));
  dataToSend = dataToSend + "||" + String(GPSlat_present);
  dataToSend = dataToSend + "||" + String(GPSlong_present);
  
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
  issueCommand("AT+QHTTPREAD=30"); //Send the read request. Not currently in use. 
  issueCommand("AT+QIDEACT");
  
}

void sendRead() {
  //Create the URL to send the HTTP GET request to.
  String result = READURL;
  String result2;
  //Prepare the AT command that declares the amount of bytes of the url to send the request to and the max time to send (in seconds)
  String resultInfo = "AT+QHTTPURL=" + String(result.length()) + ",30";
  //Convert the two above strings to const char *, which is what the issueCommand function requires
  const char * resultp = result.c_str();
  const char * resultInfop = resultInfo.c_str();
  
  String output;
  //char outChar[ ];
  boolean gettingLL = false;
  boolean writingLat = false;
  int leftPoint = 0;
  int rightPoint = 0;
  
  //Issue the AT commands
  issueCommand("AT+QIFGCNT=0");
  issueCommand("AT+QIDNSIP=1");
  issueCommand("AT+QICSGP=1,\"truphone.com\",\"\",\"\""); //Set the APN
  
  issueCommand(resultInfop);
  issueCommand(resultp);
  issueCommand2("AT+QHTTPGET=60","OK");
  output = issueAndSave("AT+QHTTPREAD=30");  //Send the read request.
  leftPoint = output.indexOf('[');
  rightPoint = output.indexOf(']', leftPoint + 1);
  if (leftPoint != rightPoint && rightPoint > leftPoint)
    {
    output = output.substring(leftPoint+1,rightPoint);
    leftPoint = output.indexOf(',');    // Is now a middle point
    result = output.substring(0,leftPoint);
    result2 = output.substring(leftPoint+1, output.length());
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
        flag_targetChange = true;
        samplesTaken = 0; // Reset the samples taken so we can take more at the new location.
        Serial.println("TARGET LOCATION CHANGED: samplesTaken = 0");
      }
    }
  //delay(5000);
  issueCommand2("AT+QIDEACT","DEACT OK");
   
}

void setupModem() {
 issueCommand2("AT","OK");
 while (flag_cmd)
  {
    //Modem not solid, try until it works
    issueCommand2("AT","OK");
    delay(500);
  }
  Serial.println("Modem is solid.");
}
 
void setupTemp() {
  float tempC = getTemperature(ourThermometer);
  delay(500);
  float tempC2 = getTemperature(ourThermometer);
  //85 degrees C is the value after booting up. Can also occur
  //if there's a power issue (i.e., not enough to temp)
  while (tempC2 != tempC || tempC2 == 85)
   {
     if (tempC2 == 85)
       Serial.println("85 C. Possible power issue.");
     tempC2 = getTemperature(ourThermometer);
     delay(500);
   } 
   Serial.println("Temperature sensor is online with temp " + String(tempC));
   Serial.println();
}

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
         if (flag_unoPresent == false)
          {
            flag_unoPresent = true;
            sendToUno(8);
            Serial.println("Connection restored. Resending data ... ");
          }
        }
      else
        {
        flag_unoPresent = true;
        }
      slaveUp = true;
    }else
    {
      flag_unoPresent = false;
      Serial.println("Slave not found ... Trying again");
      delay(2000);
    }
  delay(500);
  }
}

void sendToUno(int a) {
 //int lat = GPSlat_target.toFloat() * pow(2,7);
 //int lng = GPSlong_target.toFloat() * pow(2,7);
 const char * latp = GPSlat_target.c_str();
 const char * longp= GPSlong_target.c_str();
 //Serial.println(GPSlat_target.toFloat(), 6);
 //Serial.println(GPSlong_target.toFloat(), 6);
 //Serial.println("Lat: " + String(lat));
 //Serial.println("Long: " + String(lng));
 Wire.beginTransmission(a);
 Wire.write("<");
 Wire.write(latp);
 Wire.write(",");
 Wire.write(longp);
 Wire.write(">");
 Wire.endTransmission(); 
}

void requestToUno(int a, int b) {
  boolean printCheck = false;
  String output, output_piece;
  int flag1, flag2, flag3, flag4, flag5;
  Wire.requestFrom(a,b);
  while (Wire.available() > 1) {
    char c = Wire.read();
      if (c != '\n' && c != '\r' && c != '\n\r' && c != '\r\n' && c != 'ÿ' && c != 0 && c != ' ')
        {
        output = output + String(c);
        printCheck = true;
        Serial.print(c);
        }
  }
  if (printCheck == true)
    {
      Serial.println();
      //Parse Info from Uno
        //Get the Flag Data
        flag1 = (output.substring(0,1)).toInt();
        flag2 = (output.substring(1,2)).toInt();
        flag3 = (output.substring(2,3)).toInt();
        flag4 = (output.substring(3,4)).toInt();
        flag_there = (output.substring(4,5)).toInt();
        GPSlat_present = output.substring(output.indexOf(',')+1,output.lastIndexOf(','));
        GPSlong_present = output.substring(output.lastIndexOf(',')+1, output.length());
        //output_piece = output.substring(0,output.indexOf(','));
        Serial.println("Flag Received: " + String(flag1) + ", " + String(flag2) + ", " + String(flag3) + ", " + String(flag4) + ", " + String(flag_there));
    }
}