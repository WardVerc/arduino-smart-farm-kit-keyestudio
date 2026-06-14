#include <Arduino.h>
#ifdef ESP32
  #include <WiFi.h>
#elif defined(ESP8266)
  #include <ESP8266WiFi.h>
#endif

#include <dht11.h>
#include <analogWrite.h>
#include <ESP32_Servo.h>
#include <LiquidCrystal_I2C.h>
#include "BuzzerMusic.h"

//To be displayed
#define DHT11PIN        17  //Temperature and humidity sensor pin
#define RAINWATERPIN    35  //Steam sensor pin
#define LIGHTPIN        34  //Photoresistor pin
#define WATERLEVELPIN   33  //Water level sensor pin
#define SOILHUMIDITYPIN 32  //Soil humidity sensor pin
//To be controlled
#define LEDPIN          27  //LED pin
#define RELAYPIN        25  //Relay pin (to control water pump)
#define SERVOPIN        26  //Servo pin
#define FANPIN1         19  //Fan IN+ pin
#define FANPIN2         18  //Fan IN- pin
#define BUZZERPIN       16  //Buzzer pin
#define BUTTONPIN       5   //Button pin
#define TRIGPIN         12  //
#define ECHOPIN         13  // Distance sensor

const char* ssid = "your wifi";
const char* pwd = "your pasw";

//Initialize LCD1602, 0x27 is I2C address
LiquidCrystal_I2C lcd(0x27,16,2);
WiFiServer server(80);  //Initialize wifi server
dht11 DHT11;            //Initialize temperature and humidity sensor
Servo myservo;          // create servo object to control a servo
                // 16 servo objects can be created on the ESP32

//Define variable as detected values
String request;
String dataBuffer;
int Temperature;   //Temperature
int Humidity;      //Humidity
int SoilHumidity;  //Soil humidity
int Light;         //Brightness
int WaterLevel;    //Water level
int Rainwater;     //Rainfall

bool fanOn = false;//Fan

void setup() {
  Serial.begin(9600);
  //Connect to wifi
  WiFi.begin(ssid, pwd);
  //Determine whether connected
  Serial.println("Connecting to WiFi...");
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 5) {
    delay(1000);
    Serial.print(".");
    attempts++;
  }
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi failed, running offline");
    lcd.clear();
    lcd.setCursor(0, 0); lcd.print("WiFi failed!");
    lcd.setCursor(0, 1); lcd.print("Offline mode");
  } else {
  Serial.println("Connected to WiFi");
  Serial.print("WiFi NAME:"); Serial.println(ssid);
  Serial.print("IP:"); Serial.println(WiFi.localIP());
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("IP:");
  lcd.setCursor(0, 1); lcd.print(WiFi.localIP());
  server.begin();
}

  delay(1000);
  //Serial monitor prints wifi name and IP address
  Serial.println("Connected to WiFi");
  Serial.print("WiFi NAME:");
  Serial.println(ssid);
  Serial.print("IP:");
  Serial.println(WiFi.localIP());

  //Initialize LCD
  lcd.init();
  // Turn the (optional) backlight off/on
  lcd.backlight();
  //lcd.noBacklight();
  lcd.clear();
  //Set the position of cursor
  lcd.setCursor(0, 0);
  //LCD prints
  lcd.print("IP:");
  //Set the position of cursor
  lcd.setCursor(0, 1);
  //LCD prints
  lcd.print(WiFi.localIP());

  //set pins mode
  pinMode(LEDPIN,OUTPUT);
  pinMode(RAINWATERPIN,INPUT);
  pinMode(LIGHTPIN,INPUT);
  pinMode(SOILHUMIDITYPIN,INPUT);
  pinMode(WATERLEVELPIN,INPUT);
  pinMode(RELAYPIN,OUTPUT);
  pinMode(FANPIN1,OUTPUT);
  pinMode(FANPIN2,OUTPUT);
  pinMode(BUZZERPIN,OUTPUT);
  pinMode(BUTTONPIN, INPUT);
  pinMode(TRIGPIN, OUTPUT);
  pinMode(ECHOPIN, INPUT);
  delay(1000);

  // attaches the servo on pin 26 to the servo object
  myservo.attach(SERVOPIN);

  //Start server
  server.begin();
}

void loop() {
  //Check whether a client is connected to the web server
  //When the client is connected to server, "server.available()" returns a WiFiClient object for communication at client-side.
  WiFiClient client = server.available();
  if (client) {
    Serial.println("New client connected");
    while (client.connected()) {
      //Determine whether the server sends data
      if (client.available()) {
        request = client.readStringUntil('s');
        Serial.print("Received message: ");
        Serial.println(request);
      }
        //Acquire all senser data
        getSensorsData();
        //put all data into "dataBuffer"
        dataBuffer = "";
        dataBuffer += String(Temperature,HEX);
        dataBuffer += String(Humidity,HEX);
        dataBuffer += dataHandle(SoilHumidity);
        dataBuffer += dataHandlePercent(Light);
        dataBuffer += dataHandle(WaterLevel);
        dataBuffer += dataHandlePercent(Rainwater);
        //Send data to server, transmit to APP
        client.print(dataBuffer);
        delay(500);

      //LED
      if(request == "a")
      {
        digitalWrite(LEDPIN,HIGH);
      }
      else if(request == "A")
      {
        digitalWrite(LEDPIN,LOW);
      }
      //Irrigation
      else if(request == "b")
      {
        digitalWrite(RELAYPIN,HIGH);
        delay(400);//Irrigation delay
        digitalWrite(RELAYPIN,LOW);
        delay(650);
      }
      //Fan
      else if(request == "c")
      {
        delay(800);
        fanOn = true;
        digitalWrite(FANPIN1, HIGH);
        digitalWrite(FANPIN2, LOW);
        delay(200);
      }
      else if(request == "C")
      {
        fanOn = false;
        digitalWrite(FANPIN1, LOW);
        digitalWrite(FANPIN2, LOW);
      }
      //Feeding box
      else if(request == "d")
      {
        //Servo rotates to 180°, open feeding box
        myservo.write(80);
        delay(500);
      }
      else if(request == "D")
      {
        //Servo rotates to 80°, close feeding box
        myservo.write(180);
      }
      //Music
      else if(request == "e")
      {
        Music();
      }
      request = "";
    }
    Serial.println("Client disconnected");
  }
  getSensorsData();
  updateLCD();
  autoLED();
  checkButton();
  autoServo();
  checkSteamMusic();
  //autoIrrigate();
}

void getSensorsData() {
  int chk = DHT11.read(DHT11PIN);
  if (chk == DHTLIB_OK) {
    Temperature = DHT11.temperature;
    Humidity = DHT11.humidity;
  }
  // else: keep previous values, or set to -1 to indicate error

  Light     = (analogRead(LIGHTPIN) / 4095.0) * 100;
  Rainwater = (analogRead(RAINWATERPIN) / 4095.0) * 100;
  SoilHumidity = analogRead(SOILHUMIDITYPIN) * 2.3;
  WaterLevel   = analogRead(WATERLEVELPIN) * 2.5;
}

void Music() {
  // iterate over the notes of the melody:
  for (int thisNote = 0; thisNote < 41; thisNote++) {
  
    // to calculate the note duration, take one second
    // divided by the note type.
    //e.g. quarter note = 1000 / 4, eighth note = 1000/8, etc.
    int noteDuration = 700/noteDurations2[thisNote];
    tone(BUZZERPIN, melody2[thisNote],noteDuration);
    
    // to distinguish the notes, set a minimum time between them.
    // the note's duration + 30% seems to work well:
    int pauseBetweenNotes = noteDuration * 1.30;
    delay(pauseBetweenNotes);
    // stop the tone playing:
    noTone(BUZZERPIN);
  }
}

//Convert data into percentage
String dataHandle(int data){
  // Convert analog values into percentage
  int percentage = (data / 4095.0) * 100;
  // If the converted percentage is greater than 100, output 100. 
  percentage = percentage > 100 ? 100 : percentage;
  // Six characters store hexadecimal strings, one character is as terminators
  char hexString[3];
  // Convert hexadecimal values to 6-digit hexadecimal strings, add leading zeros: 0 is 00, 1 is 01...
  sprintf(hexString, "%02X", percentage);

  return hexString;
}

// For values already in percentage (0-100)
String dataHandlePercent(int data){
  data = data > 100 ? 100 : data;
  char hexString[3];
  sprintf(hexString, "%02X", data);
  return hexString;
}

void updateLCD() {
  static int screen = 0;
  static unsigned long lastSwitch = 0;
  if (millis() - lastSwitch < 3000) return;
  lastSwitch = millis();
  lcd.clear();
  switch (screen) {
    case 0:
      lcd.setCursor(0, 0); lcd.print("Temp: " + String(Temperature) + "C");
      lcd.setCursor(0, 1); lcd.print("Hum:  " + String(Humidity) + "%");
      break;
    case 1:
      lcd.setCursor(0, 0); lcd.print("Soil: " + String(SoilHumidity) + "%");
      lcd.setCursor(0, 1); lcd.print("Water:" + String(WaterLevel) + "%");
      break;
    case 2:
      lcd.setCursor(0, 0); lcd.print("Light:" + String(Light) + "%");
      lcd.setCursor(0, 1); lcd.print("Rain: " + String(Rainwater) + "%");
      break;
    case 3:
      lcd.setCursor(0, 0); lcd.print("IP:");
      lcd.setCursor(0, 1); lcd.print(WiFi.localIP());
      break;
  }
  screen = (screen + 1) % 4;
}

void autoLED() {
  if (Light < 50) {
    digitalWrite(LEDPIN, HIGH);
  } else {
    digitalWrite(LEDPIN, LOW);
  }
}

void checkButton() {
  int ReadValue = digitalRead(BUTTONPIN);
  if (ReadValue == 0) {
    delay(10);
    if (ReadValue == 0) {
      fanOn = !fanOn;
      if (fanOn) {
        digitalWrite(FANPIN1, HIGH);
        digitalWrite(FANPIN2, LOW);
      } else {
        digitalWrite(FANPIN1, LOW);
        digitalWrite(FANPIN2, LOW);
      }
      while (digitalRead(BUTTONPIN) == 0);
    }
  }
}

float getDistance() {
  digitalWrite(TRIGPIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIGPIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIGPIN, LOW);
  int duration = pulseIn(ECHOPIN, HIGH, 20000);
  return duration / 58.0;
}

void autoServo() {
  static bool isOpen = false;
  float distance = getDistance();

  if (distance >= 2 && distance <= 7 && !isOpen) {
    isOpen = true;
    myservo.write(80);
    delay(500); // give servo time to reach position
  } else if ((distance < 2 || distance > 7) && isOpen) {
    isOpen = false;
    myservo.write(180);
    delay(500);
  }
}

void checkSteamMusic() {
  static int baseline = -1;
  if (baseline == -1) baseline = Rainwater;

  Rainwater = (analogRead(RAINWATERPIN) / 4095.0) * 100;

  if (Rainwater >= baseline + 10) {
    int randomFreq = random(200, 1000);
    tone(BUZZERPIN, randomFreq, 100);
  } else {
    noTone(BUZZERPIN);
  }
}

void autoIrrigate() {
  if (SoilHumidity < 30) {
    digitalWrite(RELAYPIN, HIGH);
    delay(400);
    digitalWrite(RELAYPIN, LOW);
  }
}
