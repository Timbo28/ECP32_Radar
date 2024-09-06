#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "LittleFS.h"
#include <Arduino_JSON.h>
#include <Wire.h>

char ssid[50] = "PC-5864";
char password[50] = "ghshdjkjss";

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

// Create an Event Source on /events
AsyncEventSource events("/events");


// Json Variable to Hold Sensor Readings
JSONVar readings;

// Buffer for speed variable
char i2c_buf[5];


// Timer variables
unsigned long lastTime = 0;  
unsigned long timerDelay = 100;


void initWiFi();

void Write_ssid(uint8_t funktion, const char* data){
  Wire.beginTransmission(0x50);
  Wire.write(funktion);
  Wire.write(data);
  Wire.endTransmission();
}

void getWifiData(uint8_t acction,char* data){
  Write_ssid(acction,"");
  memset(data,0,strlen(data));
  Wire.requestFrom(0x50, 49);    // request 6 bytes from peripheral device #8
  uint8_t index = 0;
  char c;
  while (Wire.available()) { // peripheral may send less than requested
    c = Wire.read();    // receive a byte as character
    data[index] = c;
    ++index;
  }
  Serial.println(data);
}


int32_t getSpeed(){

  Wire.requestFrom(0x50, 6);    // request 6 bytes from peripheral device #8
  uint8_t index = 0;
  char c;
  while (Wire.available()) { // peripheral may send less than requested
    c = Wire.read();    // receive a byte as character
    i2c_buf[index] = c;
    ++index;
  }
  int32_t speed = atoi(i2c_buf);
  if(speed ==170){
    Write_ssid(1,"");
  }else if((speed == 171) && (WiFi.status() == WL_CONNECTED)){
    Write_ssid(3,ssid);
    char localIp[13];
    WiFi.localIP().toString().toCharArray(localIp,sizeof(localIp));
    Write_ssid(4,localIp);
    Write_ssid(2,"");
  }else if((speed == 172)){
    char old_ssid[50];
    strcpy(old_ssid,ssid);
    getWifiData(8,ssid);
    getWifiData(9,password);
    Write_ssid(1,"");
    if(strcmp(old_ssid,ssid)!=0){
      Write_ssid(5,"");
      delay(50);
      initWiFi();
    }
    
  }

  return speed;
}





// Get Sensor Readings and return JSON object
String getSensorReadings(){
  readings["speed"] = String(getSpeed());
  String jsonString = JSON.stringify(readings);
  return jsonString;
}

// Initialize LittleFS
void initFS() {
  if (!LittleFS.begin()) {
    Serial.println("An error has occurred while mounting LittleFS");
  }
  Serial.println("LittleFS mounted successfully");
}

void waitForSTM32(){
  while(getSpeed() != 170){
    Serial.println("wait to connect   ");
    delay(1000);
  }
}

// Initialize WiFi
void initWiFi() {
  waitForSTM32();
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    Write_ssid(3,ssid);
    Write_ssid(1,"");
    getSpeed();
    delay(1500);
  }
  Serial.println(WiFi.localIP());
    char localIp[17];
  WiFi.localIP().toString().toCharArray(localIp,sizeof(localIp));
  Write_ssid(4,localIp);
  Write_ssid(2,"");
}



void setup() {
  Wire.begin();       // join i2c bus
  Serial.begin(115200);
  initWiFi();
  initFS();
  
  
  // Web Server Root URL
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(LittleFS, "/index.html", "text/html");
  });

  server.serveStatic("/", LittleFS, "/");

  // Request for the latest sensor readings
  server.on("/readings", HTTP_GET, [](AsyncWebServerRequest *request){
    String json = getSensorReadings();
    request->send(200, "application/json", json);
    json = String();
  });

   events.onConnect([](AsyncEventSourceClient *client){
    if(client->lastId()){
      Serial.printf("Client reconnected! Last message ID that it got is: %u\n", client->lastId());
    }
    // send event with message "hello!", id current millis
    // and set reconnect delay to 1 second
    client->send("hello!", NULL, millis(), 10000);
  });


  server.addHandler(&events);

  // Start server
  server.begin();

}

void loop() {
  if ((millis() - lastTime) > timerDelay) {
    // Send Events to the client with the Sensor Readings Every 30 seconds
    events.send("ping",NULL,millis());
    events.send(getSensorReadings().c_str(),"new_readings" ,millis());
    lastTime = millis();
 }
}