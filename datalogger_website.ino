// go to http://192.168.4.1


String updates = "";

#define DEFAULT_WAIT_TIME 1000
#define SIMULATION_MODE false

RTC_DATA_ATTR int waitTime = 10000;
RTC_DATA_ATTR long lastTime = 0;
RTC_DATA_ATTR int lastTemp = 0;
RTC_DATA_ATTR int lastHumid = 0;
RTC_DATA_ATTR long lastWeight = 0;
RTC_DATA_ATTR bool didSetup = false;
#include "FS.h"
#include "SPIFFS.h"
int numReadings = 0;
#define FORMAT_SPIFFS_IF_FAILED true


// rtc libraries
#include <SPI.h>
#include <Wire.h>
#include "RTClib.h"
RTC_DS3231 rtc;
bool editWaitTime = false;

// DHT libraries
#include "DHTesp.h"
DHTesp dht;


//HX711 constructor (dout pin, sck pin):
#include "HX711.h"
#define DOUT  25
#define CLK  26
HX711 scale;


#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiAP.h>
#include <WebServer.h>
// Set these to your desired credentials.


#include "FS.h"
#include "SPIFFS.h"
#define FORMAT_SPIFFS_IF_FAILED true

int requestChar = 0;
String ssid = "ESP32_AP";
const char *password = "WIFI12345";


WebServer server(80);


void wipe(bool wipeAll, bool reboot){
  
   Serial.println("deleting data");
   if (true) {
   String filename;
   
   File root = SPIFFS.open("/");
   File file = root.openNextFile();
    while(file){
      filename = file.name();
      Serial.println("file called: " + filename);
      if(!file.isDirectory()){
         
         if (filename != "settings.txt" or wipeAll) {
           if(SPIFFS.remove(filename)){
                Serial.println(filename + "- file deleted");
            } else {
                Serial.println(filename + "- delete failed");
            }
         }
      }
      file = root.openNextFile();
   }
   } else {
    Serial.println("not actually deleting everything, fix that problem");
   }
   Serial.println("restarting");
   if (reboot) { ESP.restart(); }
}

bool updateTime() {
  Serial.println("trying to update time");
  if (server.args()==6){
    int yr = server.arg(0).toInt();
    int mo  = server.arg(1).toInt();
    int da = server.arg(2).toInt();
    int hr = server.arg(3).toInt();
    int mi = server.arg(4).toInt();
    int se = server.arg(5).toInt();
    rtc.adjust(DateTime(yr, mo, da, hr, mi, se));
 
    Serial.println("updated time to " + String(yr) + "/" + String(mo) + "/" + String(da) + " " + String(hr) + ":" + String(mi) + ":" + String(se) + ":");
    return true;
  }
  for (int i = 0; i < server.args(); i++) {
    Serial.println(server.argName(i));
    Serial.println(server.arg(i));
  }
  Serial.println("no parameter for time. there were" + String(server.args()));
  return false;
}

void getReadings() {
    dht.setup(27, DHTesp::DHT22);
    scale.begin(DOUT, CLK);
    scale.set_scale(2);
    scale.power_down();

    DateTime now = rtc.now();
    long nowTime = now.unixtime();
    if (SIMULATION_MODE) {nowTime = lastTime + waitTime;}
    
    scale.power_up();
    long weight= int(scale.read_average(10)*100);
    scale.power_down();
    
    int temperature = int(dht.getTemperature()*10);
    int humidity = int(dht.getHumidity()*10);

    if (SIMULATION_MODE) {
      weight = random(-500000,-400000);
      temperature = random(200,300);
      humidity = random(500,800);
    }
    
    if (abs(temperature) > 1000){
      Serial.println("error, temperature too high. was " + String(temperature));
      temperature = 0;
    } if (abs(humidity) > 1000){
      Serial.println("error, humidity too high. was " + String(humidity));
      humidity = 0;
    }
    
    Serial.println("Time: " + String(nowTime));
    Serial.println("Weight: " + String(weight/10.0));
    Serial.println("Temperature: " + String(temperature/10.0));
    Serial.println("Humidity: " + String(humidity/10.0));
    
    //write to the files
    writeToFile("/time.txt", nowTime-lastTime);
    writeToFile("/weight.txt", weight-lastWeight);
    writeToFile("/temp.txt", temperature-lastTemp);
    writeToFile("/humid.txt", humidity-lastHumid);
    lastTime = nowTime;
    lastWeight  = weight;
    lastTemp = temperature;
    lastHumid = humidity;
    numReadings++;
    
    Serial.println("done writing"); 
}

bool updateWaitTime() {
  if (server.args()==1 and server.argName(0) == "waitTime"){
    int waitTime = server.arg(0).toInt();
    if (waitTime>1){
      Serial.println("adjusting wait time to " + String(waitTime));
      return true;
    }
  }
  for (int i = 0; i < server.args(); i++) {
    Serial.println(server.argName(i));
    Serial.println(server.arg(i));
  }
  Serial.println("no parameter for wait time. there were " + String(server.args()));
  return false;
}

void makeFile(){
    File settings = SPIFFS.open("/settings.txt", FILE_WRITE);
    settings.print(DEFAULT_WAIT_TIME);
    waitTime = DEFAULT_WAIT_TIME;
    
    String files[] = {"/time.txt", "/weight.txt", "/temp.txt", "/humid.txt"};
    for (int i = 0; i < 4; i++) {
      File file = SPIFFS.open(files[i], FILE_WRITE);
      if(!file){
          Serial.println("- failed to open file for writing");
          return;
      }
     
      if(file.print("")){
          Serial.println("- file written");
      } else {
          Serial.println("- write failed");
      }
    }
}

void runThroughProgram() {
  Serial.println("running through program");
  int singleCount = 0;
  String reading = "";
  char value;
  File settingfile = SPIFFS.open("/settings.txt");
  if(!settingfile || settingfile.isDirectory()){
    Serial.println("cant open setting file");
  }
  else {
    Serial.print("wait time: ");
    while (settingfile.available()){
      value = settingfile.read();
      reading += value;
      Serial.print(value);
    }
  }
  
  if (reading=="") {
    Serial.println("\nWait time was never set, using default");
    waitTime = DEFAULT_WAIT_TIME;
  } else {
    waitTime = reading.toInt();
    Serial.println("\nWait time is now " + waitTime);
  }
  reading = "";

  
  String filenames[] = {"/time.txt", "/weight.txt", "/temp.txt", "/humid.txt"};
  for (int i = 0; i < 4; i++) {
     File file = SPIFFS.open(filenames[i]);
     if(!file || file.isDirectory()){
        Serial.println("cant open files");
        wipe(true, false);
     } else {
        Serial.println("found and can open file " + filenames[i]);
        while (file.available()){
           value = file.read();
           Serial.print(value);
           if (value == ',') {
              singleCount++;
              
              switch (i) {
                 case 0:
                    Serial.println("time");
                    lastTime += reading.toInt();
                    break;
                 case 1:
                    Serial.println("weight");
                    lastWeight += reading.toInt();
                    break;
                 case 2:
                    Serial.println("temp");
                    lastTemp += reading.toInt();
                    break;
                 case 3:
                    Serial.println("humid");
                    lastHumid += reading.toInt(); 
                    break;
              }
              reading = "";
              singleCount += 1;
           } else {
              reading += value;
           }
        }
     }
     if (singleCount!=numReadings and numReadings!=0) {
        // this should never happen unless there was an error writing to the files
        Serial.println("There is an inconsistency with the file lengths.\nThe previous one was " + String(numReadings) + " and the new one had " + String(singleCount));
        wipe(true, true);
     } else if (numReadings == 0) {numReadings = singleCount;}
    Serial.println("finished with file #" + String(i));
    singleCount = 0;
     
  }
  Serial.println("last time: " + String(lastTime));
  Serial.println("last weight: " + String(lastWeight));
  Serial.println("last temp: " + String(lastTemp));
  Serial.println("last humid: " + String(lastHumid));
}

bool writeToFile(String filename, long reading){

     File file = SPIFFS.open(filename, FILE_APPEND);
     if(!file){
         Serial.println("- failed to open file for appending");
         return false;
      }
      if(file.print(String(reading) + ',')) {
        Serial.println(String(reading) + " appended to " + filename);
        Serial.println("size: " + String(file.size()));
        return true;
      } else {
          Serial.println("- append failed");
      }
   return false;
}



void setup() {
  Serial.begin(115200);

    if(!SPIFFS.begin(FORMAT_SPIFFS_IF_FAILED)){
    Serial.println("SPIFFS Mount Failed");
    return;
  }
  runThroughProgram();

  Serial.println("\nConfiguring access point...");
  // You can remove the password parameter if you want the AP to be open.
  String ssidName = ssid;
  String chipId = String((uint32_t)(ESP.getEfuseMac() >> 24), HEX);
  ssidName += '_';
  ssidName += chipId;
  Serial.println("a");
  WiFi.softAP(ssidName.c_str(), password);
  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(myIP);
//  Serial.println("AP IP address: ");
  

//  Serial.println("IP address: " + WiFi.localIP());
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  server.begin();
  
  server.on("/", [](){
    Serial.println("main requested");
    requestChar=0;
    server.send(200, "text/html", 
"<html>\n<script>   \n var fullData=[[]];\n\n  function sendCsv() {\n    let csvContent = 'data:text/csv;charset=utf-8,' + fullData.map(e => e.join(',')).join('\\n');\n   var encodedUri = encodeURI(csvContent);\n   var link = document.createElement('a');\n   link.setAttribute('href', encodedUri);    \n    link.setAttribute('download', 'my_data.csv');    \n   document.body.appendChild(link);     \n   link.click();\n }\n\n var requestNum=0;\n\n function requestData(){\n   var myxmlhttp = new XMLHttpRequest();\n    myxmlhttp.onreadystatechange = function(){  \n     if(myxmlhttp.status==200 && myxmlhttp.readyState==4){\n       var words = myxmlhttp.responseText; \n        console.log('data:', words); \n       if (requestNum%2==1 && words !=='') {\n         numStr = '';\n          lastVal = 0;\n          var num = 1;\n              for (var i = 0; i < words.length; i++) {\n                if (words[i]=='\\n'){\n                 if (fullData.length > num){\n                   lastVal += parseInt(numStr);\n                    fullData[num].push(lastVal);\n                  } else {\n                    lastVal += parseInt(numStr);\n                    fullData.push([lastVal]);\n                 }\n                 numStr = '';\n                  num+=1;\n               } else {\n              numStr += words[i];\n           }\n         }\n         } else if (words !=='') {\n           console.log('name:', words);\n            fullData[0].push(words);\n          }\n         if (words!==''){\n            requestNum+=1;\n            console.log('requesting more data');\n            requestData();\n          } else {\n            console.log('finished requesting data');\n            console.log(fullData);\n            requestNum=0; \n            sendCsv();\n          }\n       }  \n   }\n   myxmlhttp.open('GET', '/_dataPoint?fileno=' + requestNum, true);\n    myxmlhttp.send();\n   }\n\n         function sendWaitTime(){ \n                  var collectFreq = parseInt(document.getElementById('collectFreq').value);  \n                   console.log('collection freq', collectFreq);  \n                  if (isNaN(collectFreq)){    \n                   console.log('not a number');  \n                   } else {    \n                   var xhttp = new XMLHttpRequest();     \n\n            xhttp.onreadystatechange = function() {         \n              console.log(this.responseText);       \n            };    \n             xhttp.open('GET', '/_waitTime?waitTime=' + collectFreq, true);    \n             xhttp.send();   }}\n  function wipeData(){\n    var xhttp = new XMLHttpRequest();    xhttp.onreadystatechange = function() {     console.log(this.responseText);   };   var date = new Date();  xhttp.open('GET', '/_wipeData', true);\n    xhttp.send();\n }\n\nfunction sendTime(){  var xhttp = new XMLHttpRequest();    xhttp.onreadystatechange = function() {     console.log(this.responseText);   };   var date = new Date();  xhttp.open('GET', '/_currentTime?year=' + date.getFullYear() + '&?month=' + (parseInt(date.getMonth())+1) + '&?day=' + (parseInt(date.getDay())+1) + '&?hour=' + date.getHours() + '&?min=' + date.getMinutes() + '&?sec=' + date.getSeconds(), true);   xhttp.send();\n}\n</script>\n<body onload='sendTime();'>\n    <head>\n        <style>\n        body {\n            background-color: #cccccc;\n            font-family: Arial, Helvetica, Sans-Serif;\n            Color: #000088;\n        }\n        </style>\n    </head>\n    <body>\n        <h1>ESP32 Hive Scale Home</h1>\n        <center>\n            <input type='button' value='Download Data' onclick='requestData();'/>\n            <br>\n            <br>\n            <input type='button' value='Wipe data' onclick='wipeData();'/>\n            <br>\n            <br>\n            <span> Data Collection Period: </span>\n            <input type='number' id='collectFreq' style='width:80px'>\n            <span> seconds </span>\n            <input type='button' value='submit' onclick='sendWaitTime();'/>\n        </center>\n    </body>\n</html>"

); });

  server.on("/_dataPoint", []() {
    String filenames[]={"/time.txt", "/weight.txt", "/temp.txt", "/humid.txt"};
    String titles[]={"time", "weight", "temperature", "humidity"};
    Serial.print("requested data called: ");
    int requestNum = server.arg(0).toInt();
    if (requestNum/2>=sizeof(titles)) { 
      server.send(200, "text/plain", ""); 
     } else if (requestNum%2==0) {  
      server.send(200, "text/plain", titles[requestNum/2]);
    } else {
       Serial.println(filenames[requestNum/2]);
      File file = SPIFFS.open(filenames[requestNum/2], "r");
      server.streamFile(file, "txt");
      file.close();
    }

  });

  server.on("/_wipeData", []() {
    wipe(true, true);
    server.send(200, "text/plain", "wiped");
  });

  server.on("/_waitTime", []() {
    if (updateWaitTime()){
      server.send(200, "text/plain", "1");
    } else {
      server.send(200, "text/plain", "0");
    }
  });
   
  server.on("/_currentTime", []() {
    if (updateTime()){
      server.send(200, "text/plain", "1");
    } else {
      server.send(200, "text/plain", "0");
    }
  });

  server.on("/_getUpdates", []() {
    String hold = updates;
    updates = "";
    server.send(200, "text/plain", "jj");
  });
   Serial.println("Server started");
}

void loop() {
    DateTime now = rtc.now();
  long nowTime = 0;
  if (SIMULATION_MODE){ 
    nowTime = millis()/1000;
  } else {
    nowTime = now.unixtime();
  }
  
  if (nowTime > lastTime + DEFAULT_WAIT_TIME) {
    Serial.println("Getting readings");
   // lastTime = nowTime;
    getReadings();
  }
  server.handleClient();
}
