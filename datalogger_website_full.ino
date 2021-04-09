// go to http://192.168.4.1 if ap

#define TESTMODE false
int useAP = 1;
int useSI = 0;

String ssid_name = "SSID";
String ssid_pwd = "password";

RTC_DATA_ATTR long waitTime = -1;
RTC_DATA_ATTR long lastVals[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
RTC_DATA_ATTR bool didSetup = false;
long liveVals[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
int numberSensors = 0;
int okayToReadTime = 0;
long lastTime = 0;
bool rtcWorks = false;
long countDown = 0;
int dataRequests = 0;
long lastMillis = 0;
long lastAccurateTime = 0;

extern int getReadings(long datas[], bool saveData);


#include "time.h"

#include "FS.h"
#include "SPIFFS.h"

#define FORMAT_SPIFFS_IF_FAILED true

// rtc libraries
#include <SPI.h>
#include <Wire.h>
#include "RTClib.h"
RTC_DS3231 rtc;

// dht library
#include "DHTesp.h"


// bme library
#include "Adafruit_BME280.h"
Adafruit_BME280 bme;
#define BME280_ADD 0x76

// hx711 library
#include "HX711.h"

// wifi libraries
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiAP.h>
#include <WebServer.h>
#include <HTTPClient.h>

#include <ESPmDNS.h> // not needed for ap

#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>



AsyncWebServer server(80);


String parsePart(String message, int *index, char delimiter) {
  String part = "";
  while (message[*index] != delimiter && *index < message.length()) {
    part += message[(*index)++];
  }
  (*index)++;
  return part;
}


void setup() {
  Serial.begin(115200);
  
  if (beginFS() == false){
    Serial.println("Unable to set up. Shutting down");
    esp_sleep_enable_timer_wakeup(10000000000);
  }
  
  
  setupSensors();

  if (useAP) {
    setupAP();
  } else {
    connectToWIFI();
  }


  setupPages();

  server.begin();
  Serial.println("Server started");
}

long lastPrintVal = 0;
void loop() {
  long nowTime = getTime();

  // is it possible to get readings
  if (okayToReadTime<nowTime) {
    
    // get readings to record
    if (nowTime >= lastTime + waitTime && waitTime > 0) {
      Serial.println("Getting readings");
      long datas[10];
      getReadings(datas, true);
      lastTime = getTime();

      countDown = 0;
      
    } else if (dataRequests>0 && okayToReadTime<=nowTime) {
      // if someone has been requesting live data, get it
      Serial.println("getting live data");
      dataRequests=0;
      long datas[10];
      getReadings(datas, false);
    }
  }
  if (waitTime - (nowTime-lastTime) != lastPrintVal){
       getTime();
       Serial.println(String(nowTime-lastTime) + '/' + String(waitTime) + " (" + String(nowTime) + ") ");
       lastPrintVal = waitTime - (nowTime-lastTime);
  }
  
}
