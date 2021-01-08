// go to http://192.168.4.1 if ap

#define TESTMODE false
#define USE_AP false

RTC_DATA_ATTR long waitTime = -1;
RTC_DATA_ATTR long lastVals[] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
RTC_DATA_ATTR bool didSetup = false;
long liveVals[] = {0,0,0,0,0,0,0,0,0,0,0}; 
int numberSensors = 0;
long lastDHTTime = 0;
int DHTwaitTime = 0;
long lastTime = 0;
bool rtcWorks = false;
long countDown = 0;
int dataRequests = 0;


#include "FS.h"
#include "SPIFFS.h"

#define FORMAT_SPIFFS_IF_FAILED true

// rtc libraries
#include <SPI.h>
#include <Wire.h>
#include "RTClib.h"
RTC_DS3231 rtc;

#include "DHTesp.h"

#include <Adafruit_BMP085.h>
Adafruit_BMP085 bmp;

#include "HX711.h"

#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiAP.h>
#include <WebServer.h>

#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>


#include <ESPmDNS.h> // not needed for ap


// Set these to your desired credentials.
String ssid = "ESP32_AP";
const char *password = "WIFI12345";

AsyncWebServer server(86);

void notFound(AsyncWebServerRequest *request) {
    request->send(404, "text/plain", "Not found");
}

bool writeToFile(String filename, long reading){
     File file = SPIFFS.open("/" + filename + ".txt", FILE_APPEND);
     if(file  && file.print(String(reading) + ',')) {
        Serial.println(String(reading) + " appended to " + filename);
        Serial.println("size: " + String(file.size()));
        return true;
      } else {
          Serial.println("- append failed");
      }
   return false;
}

bool replaceFile(String filename, String message){
      Serial.println("removing");
      SPIFFS.remove("/" + filename + ".txt");
      File file = SPIFFS.open("/" + filename + ".txt", FILE_WRITE);
      Serial.println("deleted settings  file");
      file.print(message);
      return true;
}

String parsePart(String message, int *index, char delimiter){
  String part = "";
  while (message[*index] != delimiter && *index<message.length()){
    part+=message[(*index)++]; 
  }
  (*index)++;
  return part;
}



String readFile(String filename){
    String fileText = "";
    File file = SPIFFS.open("/" + filename + ".txt");
    if(!file || file.isDirectory()){
        Serial.println("- failed to open file for reading");
        return "";
    }
    
    Serial.println("- read from file:");
    while(file.available()){
        char c = file.read();
        fileText += c;
    }
        
    return fileText;
}

int getFilenames(String filenames[]){
    Serial.println("getting file names");
    String message = readFile("sensors");
    int i = 0;
    int n = 0;

    String sensorMsg = parsePart(message, &i, '$');

    while (sensorMsg != ""){  
        int j = 0;
        filenames[n] = parsePart(sensorMsg, &j ,',');
        Serial.println("file  name: " + filenames[n]);
        sensorMsg = parsePart(message, &i, '$');
        n++;
    }
    return n;
}


int getReadings(long datas[], bool saveData) {
      int i = 0;
      String message = readFile("sensors");
      numberSensors = 0;
      
      bool setupDHT = false;
      DHTesp dht;
      
      int numSensors = 0;
    
      while (i<message.length()){
         String sensorMsg = parsePart(message, &i, '$');
         
         int j = 0;
         String fileName = parsePart(sensorMsg, &j, ',');
         String sensorType = parsePart(sensorMsg, &j, ',');
         int pinNums = parsePart(sensorMsg, &j, ',').toInt();
         int precision = parsePart(sensorMsg, &j, ',').toInt();

         Serial.println(sensorType);

         if (TESTMODE){ 
            datas[numSensors] = random(1, 100);
         } else {
            
           if (sensorType == "ds3231"){
              Serial.print("rtc: ");
               DateTime now = rtc.now();
               datas[numSensors] = now.unixtime();
               
            } else if (sensorType== "hx711"){
                Serial.print("scale");
                HX711 scale;
                Serial.print("pin nums");
                Serial.println(pinNums);
                scale.begin(pinNums/100, pinNums%100); //26, 25
                scale.set_scale(2);
                scale.power_down();
                scale.power_up();
                datas[numSensors] = int(scale.read_average(10) * pow(10,precision));
                
                scale.power_down();
                
            } else if (sensorType == "dht22_temp"){
              Serial.println("dht temp ");
                if (!setupDHT){
                  dht.setup(pinNums, DHTesp::DHT22);
                  setupDHT = true;
                }
                datas[numSensors] = int(dht.getTemperature()* pow(10,precision));
                DHTwaitTime = 500;
              
              
            } else if (sensorType == "dht22_humid"){
              Serial.print("dht humid ");
              if (!setupDHT){
                  dht.setup(pinNums, DHTesp::DHT22);
                  setupDHT = true;
              }
              datas[numSensors] = int(dht.getHumidity()* pow(10,precision));

              
            } else if (sensorType == "bmp180_temp"){
              Serial.print("bmp temp ");
                datas[numSensors] = int(bmp.readTemperature() * pow(10,precision));
               
            } else if (sensorType == "bmp180_press"){
              Serial.print("bmp press ");
                   
              datas[numSensors] = int(bmp.readPressure() * pow(10,precision));
              
            } else if (sensorType == "hall "){
              Serial.print("hall");
                datas[numSensors] = hallRead();
            }
            Serial.print("value: ");
            Serial.println(datas[numSensors]);
            liveVals[numSensors] = datas[numSensors];
         }
         if (saveData){
            int tBytes = SPIFFS.totalBytes();
            int uBytes = SPIFFS.usedBytes();
            if (uBytes+100<tBytes) {
              writeToFile(fileName, datas[numSensors]-lastVals[numSensors]);
              lastVals[numSensors] = datas[numSensors];
            }
         }
         numSensors++;
      }
      numberSensors = numSensors;
      return numSensors;
}



void wipe(String fileToDelete){
   Serial.println("deleting data");
   if (fileToDelete=="all" || fileToDelete=="data"){
     String filename;
     File root = SPIFFS.open("/");
     File file = root.openNextFile();
     while(file){
        filename = file.name();
        if (fileToDelete=="all" || (filename!="/settings.txt" && filename!="/sensors.txt")){
          SPIFFS.remove(filename);
        }
        file = root.openNextFile(); 
     } 
   } else{
    SPIFFS.remove(fileToDelete);
   }
   ESP.restart();
}

long updateTime() {
  Serial.println("trying to update time");
//  if (server.args()==6){
//    int yr = server.arg(0).toInt();
//    int mo = server.arg(1).toInt();
//    int da = server.arg(2).toInt();
//    int hr = server.arg(3).toInt();
//    int mi = server.arg(4).toInt();
//    int se = server.arg(5).toInt();
//    rtc.adjust(DateTime(yr, mo, da, hr, mi, se));
//    Serial.println("updated time to " + String(yr) + "/" + String(mo) + "/" + String(da) + " " + String(hr) + ":" + String(mi) + ":" + String(se) + ":");
//    return 1;
//  }
  return 0;
}



void runThroughProgram() {  
  Serial.println("running through program");
  String reading = "";
  char value =  '0';
  File file = SPIFFS.open("/settings.txt");
  while (file.available() && value!=','){
    reading+=value;
    value = file.read();
  }
  
  if (reading.length()>0){
    waitTime = reading.toInt();
    Serial.print("wait time:  ");
    Serial.println(waitTime);
  } else {
    Serial.println("wait time not found, using default");
  }
  file.close();

  
  String filenames[10];
  int numFiles = getFilenames(filenames);
  Serial.print("number of files:"); 
  Serial.println(numFiles);

    
  reading = "";
  for (int i = 0; i < numFiles; i++) {
    Serial.println("opening: " + filenames[i]);
     File file = SPIFFS.open("/" + filenames[i] + ".txt");
     if(!file || file.isDirectory()){
        Serial.println("- failed to open file for reading");
     }
      while (file.available()){
         value = file.read();
         if (value == ',') {
            lastVals[i] += reading.toInt();
            reading = "";
         } else {
            reading += value;
         }
      }   
      Serial.print("last value for " + filenames[i] + ": ");
      Serial.println(lastVals[i]);
      file.close();
  }
}



void setup() {
  Serial.begin(115200);

  if(!SPIFFS.begin(FORMAT_SPIFFS_IF_FAILED)){
    Serial.println("SPIFFS Mount Failed");
    return;
  }
  
  File file = SPIFFS.open("/settings.txt");
  if (!file || file.isDirectory()){
    file = SPIFFS.open("/settings.txt", FILE_WRITE);
    Serial.println("making settings file");
    file.print("1000,1,networkname,wifi12345,0"); 
    file.close();
    // waitTime, is AP(0,1), network name, network password, SI units(0,1)
   }
  
  runThroughProgram();
  rtcWorks = true;
  if (!rtc.begin()) {
      Serial.println("Couldn't find RTC");
      rtcWorks = false;
  }
  if (!bmp.begin()){
    Serial.println("couldn't find bmp");
  }
  Serial.println("finished setting up devices");
  
  

  if (USE_AP){
    Serial.println("\nConfiguring access point...");
    // You can remove the password parameter if you want the AP to be open.
    String ssidName = ssid;
    String chipId = String((uint32_t)(ESP.getEfuseMac() >> 24), HEX);
    ssidName += '_';
    ssidName += chipId;
    WiFi.softAP(ssidName.c_str());
    IPAddress myIP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(myIP);
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    server.begin();
  } else {
      WiFi.mode(WIFI_STA);
      WiFi.begin("", "");
      while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
      }
    
      Serial.println("");
      Serial.print("Connected to ");
      Serial.println("SpragueNetX");
      Serial.print("IP address: ");
      Serial.println(WiFi.localIP());
      if (MDNS.begin("esp32")) {
        Serial.println("MDNS responder started");
      }
  }


  server.on("/", [](AsyncWebServerRequest *request){
      Serial.println("main requested");
      request->send(200, "text/html", 
"<!DOCTYPE html>\n<script>\n\n var requestInt = setInterval(requestLiveData, 3000);\n  clearInterval(requestInt);\n  var setupFunctions = [getSettings, getFileNames, sendTime, requestData];\n  var functionToRun = 0;\n  function setup(){\n   // requestData();\n\n   setupFunctions[functionToRun]();\n\n    document.getElementById('data').click();\n  }\n\n var fileNames = [];\n var settings = [1000,1,'networkname','wifi12345',0]//[0,1,2,3,4,5,6,7,8,9,10,11,12,13]; \n  var settingChangeWarning = 0;\n\n\n\n //  in file: waitTime, is AP(0,1), network name, network password, SI units(0,1)\n\n  // added later: MAC address, deviceID, bytes used, total bytes\n  //waitTime (0), is AP (1), network name (2), network password (3), is SI (4), MAC address (5), deviceID (6), bytes used (7), total bytes (8)\n  function getSettings(){\n   //updateSettings();\n   console.log('getting settings');\n    var xhttp = new XMLHttpRequest();\n   xhttp.onreadystatechange = function(){  \n      if(xhttp.status==200 && xhttp.readyState==4){ \n          settings = stringListConvert(xhttp.responseText);\n         console.log('settings: ', settings);\n          console.log(xhttp.responseText);\n          updateSettings();\n         functionToRun+=1;\n         setupFunctions[functionToRun]();\n        \n      } };\n    var date = new Date();  \n    xhttp.open('GET', '/_info', true);   \n   xhttp.send();\n\n }\n\n function getFileNames() {\n   console.log('getting files');\n   var xhttp = new XMLHttpRequest(); \n    xhttp.onreadystatechange = function(){  \n      if(xhttp.status==200 && xhttp.readyState==4){ \n        console.log(xhttp.responseText);\n        var fileNames = stringListConvert(xhttp.responseText);\n\n        console.log(fileNames);\n       var x = document.getElementById('fileBrowser');\n       var i=0;\n        while (i<fileNames.length){\n         \n        var option = document.createElement('option');\n        option.text = fileNames[i];\n       x.add(option);\n        i+=1;\n     }\n     functionToRun+=1;\n       setupFunctions[functionToRun]();\n      \n      } };\n\n    var date = new Date();  \n    xhttp.open('GET', '/_fileNames', true);   \n    xhttp.send();\n }\n\n\n function sendTime(){  \n    var xhttp = new XMLHttpRequest(); \n    xhttp.onreadystatechange = function() {\n     if(xhttp.status==200 && xhttp.readyState==4){\n         console.log('time updated',  this.responseText);\n          functionToRun+=1;\n         setupFunctions[functionToRun]();\n\n    } };\n    var date = new Date();  \n    xhttp.open('GET', '/_currentTime?year=' + date.getFullYear() + '&?month=' + (parseInt(date.getMonth())+1) + '&?day=' + (parseInt(date.getDay())+1) + '&?hour=' + date.getHours() + '&?min=' + date.getMinutes() + '&?sec=' + date.getSeconds(), true);   \n   xhttp.send();\n }\n\n function requestData(){\n   //getRandomData();\n    //makePlot();\n   //return;\n   var xhttp = new XMLHttpRequest(); \n    xhttp.onreadystatechange = function(){\n      if(xhttp.status==200 && xhttp.readyState==4){ \n        console.log(xhttp.responseText);\n\n        var sensorInfo = stringListConvert(this.responseText); \n       setupSensorInfo(sensorInfo);\n          sensorsOriginal = Object.assign({}, sensorsNew);\n\n\n          var fileNames = Object.keys(sensorsOriginal);\n       console.log('filenames: ', fileNames);\n        if (fileNames.length>0){\n          downloadFile(0, fileNames);\n       } \n      }\n   }\n   xhttp.open('GET', '/_streamFile?filename=sensors', true);   \n    xhttp.send();\n }\n\n function downloadFile(fileNum, fileNames){\n    var fileName = fileNames[fileNum];\n    var xhttp = new XMLHttpRequest();    \n   xhttp.onreadystatechange = function() {\n     if(xhttp.status==200 && xhttp.readyState==4){ \n          var words = xhttp.responseText; \n         // console.log('file contents: ', words);\n        words = stringListConvert(words, true);\n       wordsCalib = [...words];\n        var i = 1;\n        var regress = sensorsOriginal[fileNames[fileNum]]['calibration'];\n       var precision = Math.pow(10, sensorsOriginal[fileNames[fileNum]]['precision']);\n\n       words[0] /= precision;\n        wordsCalib[0] = words[0]*regress[0] + regress[1];\n\n       console.log('regression: ', regress);\n       while (i<words.length){\n         words[i] = words[i-1] + words[i]/precision;\n         wordsCalib[i] = words[i]*regress[0] + regress[1];\n         i+=1;\n       }\n\n       fileData[fileName + ' (raw)'] = words;\n        fileData[fileName] = wordsCalib\n       fileNum+=1;\n       if (fileNum==fileNames.length){\n         //makePlot();\n       } else {\n          downloadFile(fileNum, fileNames);\n\n       }\n     }\n   };   \n   console.log('requesting to view: ' + fileName);\n   xhttp.open('GET', '_streamFile?filename=' + fileName , true);\n   xhttp.send();\n }\n\n function stringListConvert(strList, allNumbers, delimiter = ','){\n   var word = '';\n    var newList = [];\n   var fullList = [];\n    for (let i = 0; i < strList.length; i++) {\n        if (strList[i]==delimiter){\n         if (allNumbers==true){word =parseFloat(word);} \n         newList.push(word);\n         word = '';\n        } else if (strList[i] == '$') {\n         newList.push(word);\n         word = '';\n          fullList.push(newList);\n         newList = [];\n       } else {\n          word += strList[i];\n       }\n   }\n   if (word !== ''){\n     if (allNumbers==true){word = parseFloat(word);} \n      newList.push(word);\n   }\n   if (fullList.length == 0){\n      return newList;\n   } else {\n      if (newList.length > 0){\n        fullList.push(newList);\n     }\n     return fullList\n   }\n }\n\n\n function openTab(elmnt) {\n   console.log(elmnt.id);\n    clearInterval(requestInt);\n    var i, tabcontent, tablinks;\n\n    tabcontent = document.getElementsByClassName('tabcontent');\n   for (i = 0; i < tabcontent.length; i++) {\n   tabcontent[i].style.display = 'none';\n   }\n   tablinks = document.getElementsByClassName('tablink');\n    for (i = 0; i < tablinks.length; i++) {\n   tablinks[i].style.backgroundColor = '';\n   }\n   document.getElementById(elmnt.id + '_div').style.display = 'block';\n   elmnt.style.backgroundColor = 'white' ;\n   document.body.style.backgroundColor = 'white';\n    if (elmnt.id == 'data' || elmnt.id == 'calibration'){\n     requestInt = setInterval(requestLiveData, 3000);\n    } if (elmnt.id == 'calibration'){\n     stopCalibration();\n    }\n }\n var settingDict = {};\n function updateSettings(){\n    settingDict = {'waitTime': settings[0], 'isAP': settings[1], 'networkName': settings[2], 'networkPwd': settings[3], 'SI': settings[4], 'MAC': settings[5], 'deviceID': settings[6], 'bytesUsed': settings[7], 'totalBytes': settings[8]};\n   document.getElementById('macAddress').textContent=settingDict['MAC'];\n     document.getElementById('chipID').textContent=settingDict['deviceID'];\n      document.getElementById('totalBytes').textContent=settingDict['totalBytes'];\n      document.getElementById('usedBytes').textContent=settingDict['bytesUsed'];\n      document.getElementById('percentUsed').textContent=Math.floor(100*settingDict['bytesUsed']/settingDict['totalBytes']);\n      document.getElementById('sampleRate').textContent=settingDict['waitTime'];\n      document.getElementById('numRecordDays').textContent=Math.floor((settingDict['totalBytes']-settingDict['bytesUsed'])*settingDict['waitTime']/(86400*16));\n     document.getElementById('sampleRate2').value = settingDict['waitTime'];\n     if (settings['SI'] == '0'){\n       document.getElementById('scales').checked=false;\n      }\n }\n\n function sendSettings(){\n    console.log('sending settings');\n    var xhttp = new XMLHttpRequest();\n   xhttp.onreadystatechange = function(){  \n      if(xhttp.status==200 && xhttp.readyState==4){ \n          alert('successfully updated settings');\n         xhttp.open('GET', '/_restart', true);   \n        xhttp.send();\n\n         \n      } };\n    var date = new Date();  \n    settings[0] = document.getElementById('sampleRate2').value;\n   if (document.getElementById('scales').checked){settings[9]='1';} else {settings[9]='0';}\n\n    var message = settings.slice(0, 5);\n   console.log('setting message to send: ',  message.toString());\n\n    xhttp.open('GET', '/_replaceFile?filename=settings&?message=' + message.toString(), true);   \n   xhttp.send();\n }\n\n\n\n function requestLiveData(){\n\n   console.log('getting live data');\n   \n\n    var xhttp = new XMLHttpRequest();\n   xhttp.onreadystatechange = function(){  \n      if(xhttp.status==200 && xhttp.readyState==4){\n       var liveVals = stringListConvert(this.responseText);\n\n        var liveSpans = ['liveData'];\n       var n = 0;\n        while (n<liveSpans.length){\n         var liveList = document.getElementById(liveSpans[n]);\n\n         \n          while (liveList.options.length > 0){\n            liveList.remove(0);\n         }\n         var i = 0;\n          while (i<liveVals.length){\n            sensorName = Object.keys(sensorsOriginal)[i]\n            var a = sensorsOriginal[sensorName]['calibration'][0]*liveVals[i] + sensorsOriginal[sensorName]['calibration'][1];\n            message =  sensorName + ': ' + parseFloat(a).toFixed(2) + ' (' + liveVals[i] + ')';\n           var option = document.createElement('option');\n            option.text = message;\n            liveList.add(option);\n           i+=1;\n         }\n         n+=1;\n         }\n       document.getElementById('rawVal').textContent = liveVals[Object.keys(sensorsOriginal).indexOf(document.getElementById('calibFor').textContent)];\n        document.getElementById('estVal').textContent = reg[0]*document.getElementById('rawVal').textContent + reg[1];\n      }\n   }\n   xhttp.open('GET', '/_currentReadings', true);\n   xhttp.send();\n }\n\n\n var fileData = {};\n\n  function sendCsv() {\n    console.log('Sending csv');\n   keyys = Object.keys(fileData);\n    console.log('names: ' + keyys);\n   var csvData = [];\n   i=0;\n    while (i<keyys.length){\n     csvData.push(fileData[keyys[i]]);\n     i+=1;\n   }\n   csvData = csvData[0].map((_, colIndex) => csvData.map(row => row[colIndex]));\n   csvData.splice(0, 0, keyys);\n\n    let csvContent = 'data:text/csv;charset=utf-8,' + csvData.map(e => e.join(',')).join('\\n');\n    var encodedUri = encodeURI(csvContent);\n   var link = document.createElement('a');\n   link.setAttribute('href', encodedUri);    \n    link.setAttribute('download', 'beeScale_' + settingDict['deviceID'] + '.csv');    \n    document.body.appendChild(link);\n    link.click();\n }\n\n\n\n\n\n function makePlot(){\n    var element = document.getElementById('downloadButton');\n\n        element.style.left = '100px';\n        element.style.top = '100px';\n\n   if (fileData == {}){return;}\n\n    var elem = (document.compatMode === 'CSS1Compat') ? document.documentElement : document.body;\n     height = elem.clientHeight;\n     width = elem.clientWidth;\n\n     var c = document.getElementById('myCanvas');\n      c.height = height;\n      c.width = width;\n      var ctx = c.getContext('2d');\n     ctx.clearRect(0, 0, width, width);\n      ctx.font = '20px calibri MS';\n   ctx.textAlign = 'center';\n   \n    xVals = fileData[Object.keys(fileData)[0]];\n   yVals = fileData[Object.keys(fileData)[1]].concat(fileData[Object.keys(fileData)[2]]);\n    yVals = yVals.concat(fileData[Object.keys(fileData)[3]]);\n   var minX = getLimits(xVals)[0];\n   var maxX = getLimits(xVals)[1];\n   var xRange = getLimits(xVals)[2];\n\n   var minY = getLimits(yVals)[0];\n   var maxY = getLimits(yVals)[1];\n   var yRange = getLimits(yVals)[2];\n\n   ctx.beginPath();\n    \n\n    let chartHeight = 200;\n    let chartWidth = 300;\n   let chartX=300;\n   let chartY=300;\n   ctx.font = '20px Arial';\n    ctx.fillText('Title', chartX+chartWidth/2, chartY-chartHeight*1.1);\n   ctx.font = '12px Arial';\n\n    ctx.strokeStyle='rgb(0,0,0)';\n   startX = makeAxis(ctx, 1, minX, maxX, chartWidth, chartHeight, chartX, chartY)\n    startY = makeAxis(ctx, 0, minY, maxY, chartWidth, chartHeight, chartX, chartY)\n    ctx.stroke()\n\n    // make plot\n    var i = 0;\n    x=(xVals[i]-startX)/(xRange+(minX-startX))*chartWidth+chartX;\n   y=-(yVals[i]-startY)/(yRange+(minY-startY))*chartHeight+chartY;\n   console.log('plotting');\n    ctx.moveTo(x,y);\n  //  console.log('x,y', xVals, yVals);\n\n   ctx.save()\n    //ctx.setLineDash([]);\n    // ctx.strokeStyle='rgb(255,0,0)';\n    colors = ['red','green', 'blue', 'brown', 'orange', 'yellow', 'purple']\n   console.log('min', minY);\n   ctx.beginPath();\n    while (i<yVals.length){\n     ctx.strokeStyle=colors[Math.floor(i/xVals.length)];\n     x=(xVals[i%xVals.length]-startX)/(xRange+(minX-startX))*chartWidth+chartX;\n      y=-(yVals[i]-startY)/(yRange+(minY-startY))*chartHeight+chartY;\n     \n      ctx.lineTo(x,y);\n      i+=1;\n     if (i%xVals.length==0){\n       ctx.stroke();   \n        ctx.restore()\n       ctx.beginPath();\n        //change color\n      }\n   } \n  }\n\n function translatePlot(val, min, max, chartMin, chartSize){\n   return (val-min)/(max-min)*chartSize+chartMin;\n  }\n\n\n function makeAxis(ctx, dir, min, max, chartWidth, chartHeight, chartX, chartY){\n   var order = Math.floor(Math.log((max-min)/10) / Math.LN10+ 0.000000001);\n    console.log('order', order);\n    console.log('min: '+ min%10**order)\n   startTick = Math.floor(min-(min%10**(order+1)))\n   increment = 10**(order+1);\n    numTicks = Math.floor((max-min)/increment)+1;\n\n   console.log('num ticks:', numTicks);\n    console.log('start at: ' + startTick + ', increment by: ' + 10**(order+1));\n   if (dir==1) {var x = chartX+chartWidth;}\n    else {var x = chartX;}\n    var y = chartY;//+chartHeight/40;\n   ctx.moveTo(x,y);\n    x=chartX;\n   if (dir==0) {y=chartY-chartHeight;}\n   ctx.lineTo(x,y);\n    ctx.stroke();\n   ctx.textAlign = 'right';\n\n\n    var i = 0;\n    while (i<numTicks+1){\n     if (dir==1){x = (i*increment)/(max-min)*chartWidth+chartX;}\n     else {y = -((i*increment)/(max-min))*chartHeight+chartY; console.log('y', startTick+i*increment, y);}\n     if (x>chartX+chartWidth || y<chartY-chartHeight){\n       console.log('over bounds');\n       break;\n      }\n     ctx.moveTo(x,y);\n      ctx.lineTo(x+chartHeight/40*(dir-1), y+chartHeight/40*dir);\n     ctx.stroke();\n     \n      ctx.save();\n     ctx.translate(x+chartHeight/40*(dir-1),y+chartHeight/40*dir);\n     if (dir==1){ctx.rotate(-Math.PI/4);}\n      ctx.fillText(startTick+i*increment, 0, 0);\n      ctx.restore();\n      i+=1;\n\n   }\n\n   return startTick;\n\n }\n\n\n function getLimits(myArray){\n\n    var max = myArray.reduce(function(a, b) {\n       return Math.max(a, b);\n    });\n\n   var min = myArray.reduce(function(a, b) {\n       return Math.min(a, b);\n    });\n   return [min, max, max-min];\n }\n function clickEvent(){\n  }\n function goBack() {\n     window.history.back();\n  }\n\nfunction beginVideo(){\n var video = document.querySelector('#videoElement');\n\n  if (navigator.mediaDevices.getUserMedia) {\n    navigator.mediaDevices.getUserMedia({ video: true })\n      .then(function (stream) {\n       video.srcObject = stream;\n     })\n      .catch(function (err0r) {\n       console.log('Something went wrong!');\n     });\n   }\n }\n\n\n function wipeData(all){\n   var amount = 'data';\n    var r = false;\n    if (all){\n     amount='all'; \n      r = confirm('Do you really want to delete all files?');\n   } else {\n      r = confirm('Do you really want to delete data?');\n    }\n    \n     if (r == true) {\n      var xhttp = new XMLHttpRequest();    \n     xhttp.onreadystatechange = function() {\n          if (this.responseText==1) {\n\n            alert('deleted');\n          }\n        };   \n      var date = new Date();\n      xhttp.open('GET', '/_wipeData?filename=' + amount, true);\n      xhttp.send();\n   }\n }\n\n function deleteFile(){\n    var fileName = document.getElementById('fileBrowser').value;\n      if (fileName.length>0){\n     var r = confirm('do you really want to delete ' + fileName)\n     if (r==true){\n         var xhttp = new XMLHttpRequest();    \n         xhttp.onreadystatechange = function() {\n              if (this.responseText==1) {\n\n                alert('deleted');\n              }\n            };   \n          xhttp.open('GET', '/_wipeData?fileName=' + fileName, true);\n          xhttp.send();\n     }\n   }\n }\n \n  function viewFile(){\n    var fileName = document.getElementById('fileBrowser').value;\n    var xhttp = new XMLHttpRequest();    \n   xhttp.onreadystatechange = function() {\n     if(xhttp.status==200 && xhttp.readyState==4){ \n       console.log(this.responseText);\n         document.getElementById('rawinfo').textContent = this.responseText;  \n      }\n   };   \n   var date = new Date();  \n    console.log('requesting to view: ' + fileName)\n    fileName = fileName.slice(0, fileName.length-4);\n    console.log('Requesting to view ',  fileName);\n    xhttp.open('GET', '_streamfile?filename=' + fileName, true);\n    xhttp.send();\n }\n\n\n var sensorsOriginal = {};\n var sensorsNew = {};\n  function setupSensorInfo(rawSensorInfo){\n    var i = 0;\n    console.log('setting up sensor info');\n    console.log(rawSensorInfo);\n   //file name, sensor type, pin numbers, precision, calibration,\n    while (i<rawSensorInfo.length){\n     var sensor = rawSensorInfo[i];\n      console.log(sensor);\n      var sensorName = sensor[0]\n      sensorsOriginal[sensorName] = {};\n     sensorsOriginal[sensorName]['type'] = sensor[1];\n      sensorsOriginal[sensorName]['pinNums'] = sensor[2];\n     sensorsOriginal[sensorName]['precision'] = sensor[3];\n     sensorsOriginal[sensorName]['calibration'] = stringListConvert(sensor[4], true, ';');\n     i+=1;\n   }\n   sensorsNew = sensorsOriginal;\n   refreshSensorList();\n  }\n\n function refreshSensorList(){\n   var sensors = document.getElementById('sensors');\n\n    while (sensors.options.length>0){\n        sensors.remove(0);\n      }\n\n     i = 0;\n      while (i<Object.keys(sensorsNew).length){\n       var sensor = sensorsNew[Object.keys(sensorsNew)[i]];\n        console.log(sensor);\n      \n        var message = 'Name: ' + Object.keys(sensorsNew)[i] + ' -> ' + sensor['type'] + ', pins:';\n        var j = sensor['pinNums'];\n        while (j>.9){\n         message += Math.floor(j%100) + ',';\n         j/=100;\n       }\n       message += ' precision: ' + sensor['precision'];\n\n        var option = document.createElement('option');\n      option.text = message;\n      sensors.add(option);\n        i+=1;\n   }\n\n }\n\n     // sensorsOriginal = {'Time': {'type':'ds3231','pinNums':2122, 'precision': 0, 'calibration': [1,0]}, 'Weight': {'type':'hx711','pinNums':2625, 'precision': 0, 'calibration': [1,0]}, 'Temperature': {'type':'dht22_temp','pinNums':27, 'precision': 1, 'calibration': [1,0]}, 'Humidity': {'type':'dht22_humid','pinNums':27, 'precision': 1, 'calibration': [1,0]}};\n function useDefaultSensors(){\n   sensorsNew = {'Time': {'type':'ds3231','pinNums':2122, 'precision': 0, 'calibration': [1,0]}, 'Weight': {'type':'hx711','pinNums':2625, 'precision': 0, 'calibration': [1,0]}, 'Temperature': {'type':'dht22_temp','pinNums':27, 'precision': 1, 'calibration': [1,0]}, 'Humidity': {'type':'dht22_humid','pinNums':27, 'precision': 1, 'calibration': [1,0]}};\n   refreshSensorList();\n  }\n\n function sendSensorInfo(){\n    var i=0;\n    var warning = false;\n    console.log('og dict', sensorsOriginal);\n    console.log('og sensors', Object.keys(sensorsOriginal));\n    console.log('new sensors', Object.keys(sensorsNew));\n    if (Object.keys(sensorsOriginal).length > 0){\n     while (i<Object.keys(sensorsNew).length){\n       if (Object.keys(sensorsOriginal).includes(Object.keys(sensorsNew)[i])){\n       } else{\n         console.log('missing ', Object.keys(sensorsNew)[i]);\n          warning = true;\n       }\n       i+=1;\n     }\n   }\n   if (warning){\n     var r = confirm('Changing the sensors used will result in all previous data being deleted. Are you sure you would like to change sensors?');\n        if (r == false) {\n         return;\n       }\n   }\n   i = 0;\n    var message = '';\n   var sensorNames = Object.keys(sensorsNew);\n    while (i<sensorNames.length){\n     s = sensorsNew[sensorNames[i]];\n\n     message += sensorNames[i] + ',' + s['type'] + ','  +  s['pinNums'] + ',' + s['precision'] + ',' + s['calibration'].toString().replace(/,/g, ';') + '$';\n\n     i+=1;\n   }\n   console.log(message);\n   var xhttp = new XMLHttpRequest();\n   xhttp.onreadystatechange = function(){  \n    if(xhttp.status==200 && xhttp.readyState==4){ \n      settings = stringListConvert(xhttp.responseText);\n     console.log('success');\n     sensorsOriginal = Object.assign({}, sensorsNew);\n\n      } };\n    xhttp.open('GET', '/_replaceFile?filename=sensors&?message=' + message, true);   \n   xhttp.send();\n\n   \n  \n    \n  }\n\n function deleteSensor() {\n   var sensor = document.getElementById('sensors');\n    var s = sensor.selectedIndex;\n   if (s<0){return;}//s=Object.keys(sensorsNew).length+s;}\n   \n    console.log('sensors:', Object.keys(sensorsNew));\n   console.log('deleting:', Object.keys(sensorsNew)[s]);\n   \n    sensor.remove(sensor.selectedIndex);\n    delete sensorsNew[Object.keys(sensorsNew)[s]];\n  }\n \n  var pinOptions = {'ds3231': 2, 'hx711':2, 'dht22_temp': 1, 'dht22_humid':1, 'bmp180_temp':2, 'bmp180_press':2,  'hall': 0};\n function updatePinBoxes(){\n    var t = document.getElementById('sensors');\n   console.log(sensorTypes.value);\n   document.getElementById('sensorTitle').value = sensorTypes.value;\n   console.log('update pin Boxes');\n    var numPins = pinOptions[document.getElementById('sensorTypes').value];\n   console.log(numPins);\n   var elementNames = ['pin1', 'pin0'];\n    var i = 0;\n    while (i<elementNames.length){\n      var x = document.getElementById(elementNames[i]);\n     if (i<numPins){\n       x.style.visibility = 'visible';\n     } else {\n        x.style.visibility = 'hidden';\n      }\n     i+=1;\n   }\n }\n function addSensor(){\n\n   var sensorName = document.getElementById('sensorTitle').value;\n\n\n    var x = document.getElementById('sensors');\n   var option = document.createElement('option');\n    var numPins = pinOptions[document.getElementById('sensorTypes').value];\n   var sensorType = document.getElementById('sensorTypes').value;\n\n    var message = 'Name: ' + sensorName + ' -> ' + sensorType + ', pins:' ;\n       var elementNames = ['pin1','pin0'];\n\n   var i = 0;\n\n    var pinNums = 0;\n    while (i<elementNames.length){\n      var t = document.getElementById(elementNames[i]);\n     if (i<numPins){\n       console.log('t val', t.value);\n        message += t.value + ',';\n\n       pinNums += t.value*Math.pow(10,i*2);\n        if (t.value==''){console.log('t value empty'); return;}\n     } else {\n      }\n     i+=1;\n   }\n var sp = document.getElementById('sensorPrecision').value;\n  message += ' precision: ' + sp;\n if (sp==''){console.log('sp value empty'); return;}\n if (sensorName in sensorsNew || sensorName == ''){alert('invalid sensor name'); return;}\n\n  sensorsNew[sensorName] = {'type':sensorType, 'pinNums':pinNums, 'precision':sp, 'calibration': [1,0]};\n  console.log(sensorsNew);\n  option.text = message;\n  \n  x.add(option);\n  }\n\n\n\n\n  function findLineByLeastSquares(values_x, values_y) {\n      console.log('x:', values_x);\n      console.log('y: ', values_y)\n      var sum_x = 0;\n      var sum_y = 0;\n      var sum_xy = 0;\n      var sum_xx = 0;\n      var count = 0;\n      var x = 0;\n      var y = 0;\n      var values_length = values_x.length;\n\n      if (values_length != values_y.length) {\n          throw new Error('The parameters values_x and values_y need to have same size!');\n      }\n      if (values_length === 0) {\n          return [ [], [] ];\n      }\n      var v=0;\n      while (v<values_length){\n          x = values_x[v];\n          y = values_y[v];\n          sum_x += x;\n          sum_y += y;\n          sum_xx += x*x;\n          sum_xy += x*y;\n          count++;\n          v+=1;\n      }\n      var m = (count*sum_xy - sum_x*sum_y) / (count*sum_xx - sum_x*sum_x);\n      var b = (sum_y/count) - (m*sum_x)/count;\n\n      var predict = (x) => { return (m * x) + b };\n      var rPrediction = [];\n\n      var SStot = 0; \n      var SSres = 0;\n      var rSquared = 0;\n      for (var n in values_y) { meanValue += values_y[n]; }\n      var meanValue = (meanValue / values_y.length);\n      \n      for (var n in values_y) { \n          SStot += Math.pow(values_y[n] - meanValue, 2); \n          rPrediction.push(predict(n));\n          SSres += Math.pow(rPrediction[n] - values_y[n], 2);\n      }\n      rSquared = 1 - (SSres / SStot);\n      return [m, b, rSquared];\n  }\n\n\n    function clearCalibValues(){\n     var sensors = document.getElementById('calibVals');\n     while (sensors.options.length>0){\n       sensors.remove(0);\n      }\n     calibRawVals = [];\n      calibGivenVals = []\n   }\n   var  calibGivenVals = []\n    var calibRawVals = [];\n\n  function addCalibrationValue() {\n    given = parseFloat(document.getElementById('calibWeight').value);\n   raw = parseFloat(document.getElementById('rawVal').textContent);\n    // raw = Math.random()*3;\n   if (isNaN(given) || given == '') {\n      console.log('given is not a number');\n     console.log('given: ', given);\n      return;\n   } if (isNaN(raw)){\n      console.log('raw is not a number');\n     return;\n   }\n\n     var x = document.getElementById('calibVals');\n     var option = document.createElement('option');\n      option.text = 'actual: ' + given + '..... Raw: ' + raw;\n     x.add(option);\n      calibGivenVals.push(given);\n     calibRawVals.push(raw);\n\n     if (calibRawVals.length>2){\n       console.log('performing regression');\n       res = findLineByLeastSquares(calibRawVals, calibGivenVals);\n       document.getElementById('calibM').textContent = res[0];\n       document.getElementById('calibB').textContent = res[1];\n       document.getElementById('rSquare').textContent = res[2];\n        reg = [res[0], res[1]]\n      }\n   }\n\n function beginCalibration() {\n       \n    x = document.getElementById('liveData2').value;\n   document.getElementById('calibFor').textContent = x\n   if (x!==''){\n      document.getElementById('calibration_div').style.display = 'none';\n      document.getElementById('calibration_div2').style.display = 'block';\n      \n      document.getElementById('calibM').textContent = sensorsOriginal[x]['calibration'][0];\n     document.getElementById('calibB').textContent = sensorsOriginal[x]['calibration'][1];\n     reg = sensorsOriginal[x]['calibration'];\n    }\n }\n reg = [1,0]\n function stopCalibration() {\n    clearCalibValues();\n   document.getElementById('rSquare').textContent = '__';\n    document.getElementById('calibM').textContent = '__';\n   document.getElementById('calibB').textContent = '__';\n\n   var liveList = document.getElementById('liveData2');\n    while (liveList.options.length>0){\n      liveList.remove(0);\n   }\n   reg = [1,0];\n    var i = 0;\n    while (i<Object.keys(sensorsOriginal).length){\n      var message = Object.keys(sensorsOriginal)[i];\n      var option = document.createElement('option');\n      option.text = message;\n      liveList.add(option);\n     i+=1;\n   }\n \n    document.getElementById('calibration_div2').style.display = 'none';\n   document.getElementById('calibration_div').style.display = 'block';\n }\n\n function saveCalibration(){\n   var name = document.getElementById('calibFor').textContent;\n   if (Object.keys(sensorsNew).includes(name)){\n      sensorsNew[name]['calibration'] = reg;\n      sendSensorInfo();\n   }  else {\n     alert('that sensor somehow doesnt exist')\n   }\n }\n\n\n\n</script>\n\n<html>\n<head>\n<meta name='viewport' content='width=device-width, initial-scale=1'>\n<style>\nbody {font-family: 'Lato', sans-serif;}\n\n.tablink {\n  background-color: Aquamarine;\n  color: black;\n  float: left;\n  border: none;\n  outline: none;\n  cursor: pointer;\n  padding: 14px 20px;\n  font-size: 17px;\n  width: 14%;\n}\n\n.button {\ncursor: pointer;\npadding: 15px 15px; text-align: center; background-color: #4CAF50; border: none;\n}\n.tablink:hover {\n  background-color: #777;\n}\n\n/* Style the tab content */\n.tabcontent {\n  color: black;\n  display: none;\n  padding: 15%;\n  text-align: center;\n}\n\n#container {\n  margin: 0px auto;\n  width: 500px;\n  height: 375px;\n  border: 10px #333 solid;\n}\n#videoElement {\n  width: 500px;\n  height: 375px;\n  background-color: #666;\n}\n\n#Data {background-color:white;}\n#About {background-color:white;}\n#Camera {background-color:white;}\n#Calibration {background-color:white;}\n#Storage {background-color:white;}\n#Settings {background-color:white;}\n#Power {background-color:white;}\n\n* {\n  box-sizing: border-box;\n}\n\n\n\n</style>\n</head>\n<body>\n\n<button class='tablink' onclick='openTab(this)' id='data'>Data</button>\n<button class='tablink' onclick='openTab(this)' id='about'>About</button>\n<button class='tablink' onclick='openTab(this)' id='camera'>Camera</button>\n<button class='tablink' onclick='openTab(this)' id='calibration'>Calibration</button>\n<button class='tablink' onclick='openTab(this)' id='storage'>Storage</button>\n<button class='tablink' onclick='openTab(this)' id='settings'>Settings</button>\n<button class='tablink' onclick='openTab(this)' id='power'>Power</button>\n\n\n\n<div id='about_div' class='tabcontent'>\n  <h1>About</h1>\n  <p>This is a scale website.<br><br>MAC Address: <span id='macAddress'>___</span><br>Chip ID: <span id='chipID'>___</span><br>Software version 2 (1/6/20)<br>FS format 2 (csv)<br>Settings format 2<br></p>\n</div>\n\n<div id='camera_div' class='tabcontent' style='text-align: left; padding:5%'>\n   <h1>Coming soon:</h1>\n   <p>Need to use an ESP32cam for outside the hive video or Https for client side bee counting</p>\n   <input id='startVideo' type='button'class='button' value='Begin Video' onclick='beginVideo();'/>\n  <div id='container'>\n    <video autoplay='true' id='videoElement'>\n   \n    </video>\n  </div>\n\n</div>\n<div id='calibration_div' class='tabcontent' style='text-align: left; padding:5%'>\n  Calibrate for:<br>\n  <select id='liveData2' size=8 style='width:200px;'></select>\n  <button type='button' onclick='beginCalibration();'>begin calibration</button>\n</div>\n<div id='calibration_div2' class='tabcontent' style='text-align: left; padding:5%'>\n <h1>Calibrating for: <span id='calibFor'>___</span></h1>\n <button type='button' onclick='stopCalibration();'>back</button>\n  <p> Current Raw: <span id='rawVal'>__</span><br>\n    Current Estimated: <span id='estVal'>___</span> <br>\n    Equation: y=<span id='calibM'>_</span>*x+<span id='calibB'>_</span> R-square: <span id='rSquare'>____</span><br>\n  <button type='button' onclick='saveCalibration();'>Save Calibration</button>\n  </p>\n\n  <p> \n  Entered values:<br>\n  <select id='calibVals' size=20 style='width:300px'></select><br>\n  <input type='number' id='calibWeight' style='width:80px'>\n  <button type='button' onclick='addCalibrationValue();'>Add weight</button>\n  <button type='button' onclick='clearCalibValues();'>clear</button>\n\n\n</p>\n</div>\n<div id='storage_div' class='tabcontent'>\n  <p>File system: <span id='fs'>SPIFFS</span><br><span id='usedBytes'>__</span> bytes used out of <span id='totalBytes'>_____</span> bytes (<span id='percentUsed'>__</span> percent)<br>At a sample rate of <span id='sampleRate'>__</span>, it can record for <span id='numRecordDays'> ___</span> more days</p>\n  <input type='button' value='Wipe all files' onclick='wipeData(true);'/>\n  <input type='button' value='Wipe just data' onclick='wipeData(false);'/>\n</div>\n<div id='settings_div' class='tabcontent'>\n   <input type='button' class='button' value='save' onclick='sendSettings();'/>\n\n  <h2>Data</h2>\n <input type='checkbox' id='scales' name='scales' checked> SI units\n    <p> Data Collection Period: <input type='number' id='sampleRate2' style='width:80px'> seconds<br><br><br>\n    <input type='button' value='use default sensors' onclick='useDefaultSensors();'><br>\n   <select id='sensors' size=8 style='width:500px;'></select>\n    <br>Sensor name: <input type='text' id='sensorTitle'>\n   <input type='button' value='delete sensor' onclick='deleteSensor();'>\n   <form onchange='updatePinBoxes();'>\n   <select name='sensor' id='sensorTypes'>\n     <option value='ds3231'>ds3231 (rtc)</option>\n      <option value='hx711'>HX711 (scale)</option>\n      <option value='dht22_temp'>DHT22 (temperature)</option>\n     <option value='dht22_humid'>DHT22 (humidity)</option>\n     <option value='bmp180_temp'>BMP180 (temperature)</option>\n     <option value='bmp180_press'>BMP180 (pressure)</option>\n     <option value='hall'>hall (magnetism)</option>\n    </select>\n\n </form>\n Pin Numbers: <input type='number' style='width:60px;' id='pin1'> <input type='number' style='width:60px;' id='pin0'>\n  Precision: <input type='number' style='width:100px;' id='sensorPrecision' value='0'> decimal places<br>\n     <input type='button' value='add sensor' onclick='addSensor();'><br><br>\n     <input type='button' value='Save sensor info' onclick='sendSensorInfo();'>\n\n\n  <h2>Network</h2>\n  <p>\n <input type='checkbox' id='useAP' name='ap'> create an access point<br><br>\n Network name: <input type='text' id='networkName'><br><br>\n    Password: <input type='text' id='networkPwd'> <br><br>*Credentials are saved on ESP32, which offers no encryption</p>\n\n    <h2><br>Files</h2>\n    <h3>Upload file:</h3>\n    <input type='file' name='update'>\n    <input type='submit' value='Upload'>\n    <h3>Browse files:</h3>\n    <select id='fileBrowser' size=10 style='width:100px'></select>\n     <button type='button' onclick='deleteFile()'>Delete File</button>\n     <button type='button' onclick='viewFile()'>View Raw File</button><br>\n     <span id='rawinfo'>____</span>\n</div>\n\n<div id='power_div' class='tabcontent'>\n <h1>Power down:</h1>\n    <p>Power off the WIFI of this device to save power.<br>The ESP32 will continue to record data.<br>Restart the power source to turn on the WIFI sooner.<br><br>Power off for\n   <input type='number' id='powerPeriod' style='width:80px'> days<br><br></p>\n  <input id='powerButton' class='button' value='Power Off' onclick='shutdown();'/>\n</div>\n\n\n<div id='data_div' class='tabcontent'>\n<canvas id='myCanvas' style='position: absolute; left: 1%; top: 8%; ' onclick='clickEvent(event);' >\nYour browser does not support the HTML5 canvas tag.</canvas>\n<input id='downloadButton' type='button' style='position:absolute; left:10px' value='download csv' onclick='sendCsv();'/>\n<p style='position:absolute; font-size:30px; left:60%;'>\nCurrent readings:<br>\n<select id='liveData' size=8 style='width:200px;'></select>\n<body onload='setup();'>\n\n\n</body>\n</html> \n"

); });

  server.on("/_wipeData", [](AsyncWebServerRequest *request) {
    Serial.println("wipe data");
    wipe(request->getParam("filename")->value());
    request->send(200, "text/plain", "wiped");
  });

  server.on("/_streamFile", [](AsyncWebServerRequest *request) {
    Serial.println("streaming file: " + request->getParam("filename")->value());
    File file = SPIFFS.open("/" + request->getParam("filename")->value() + ".txt");
    if(!file || file.isDirectory()){
       request->send(200, "text/plain", "");
    } else {
     
//      request->send(200, "text/plain", "");
      request->send(SPIFFS, "/" + request->getParam("filename")->value() + ".txt", "text/plain"); 
    }
    file.close();
  } );

  server.on("/_replaceFile", [](AsyncWebServerRequest *request) {
    Serial.println("replace file");
    if (request->hasParam("filename") && request->hasParam("message")){
     
      replaceFile(request->getParam("filename")->value(), request->getParam("message")->value());
      
      request->send(200, "text/plain", "1");
    } else {
      request->send(200, "text/plain", "0");
    }
  } );

  server.on("/_currentTime", [](AsyncWebServerRequest *request) {
    Serial.println("update time");
    //long error = updateTime();
    request->send(200, "text/plain", "1");
  });


  server.on("/_currentReadings", [](AsyncWebServerRequest *request){
    Serial.println("live data requested");
    dataRequests = 3;
    String res = "";
    int i = 0;
    while (i < numberSensors && i < 10) {
        res += String(liveVals[i]);
        res += ",";
        i++;
      }
      Serial.println("sending: " + res); 
    request->send(200, "text/plain", res);
  });

  server.on("/_powerOff", [](AsyncWebServerRequest *request) {
    Serial.println("turn off");
    request->send(200, "text/plain", "1");
   // int powerOffTime = server.arg(0).toInt();
  });

  server.on("/_fileNames", [](AsyncWebServerRequest *request) {
    Serial.print("file names requested: ");
    String names = "";
    File root = SPIFFS.open("/");
    File file = root.openNextFile();
    while(file){
      names += file.name();
      names += ",";
      file = root.openNextFile();
    }
    names.remove(names.length()-1);
    Serial.println(names);
    request->send(200, "text/plain", names);
  });
  
  server.on("/_restart", [](AsyncWebServerRequest *request) {
    ESP.restart();
    request->send(200, "text/plain", "1");
  });
  
  server.on("/_info", [](AsyncWebServerRequest *request) {
    Serial.print("info requested: ");
    String vars[] = {WiFi.macAddress(), String((uint32_t)(ESP.getEfuseMac() >> 24), HEX), String(SPIFFS.usedBytes()), String(SPIFFS.totalBytes())};
    String message = readFile("settings");
    Serial.println("message: " + message);
    for (int i = 0; i < 4; i++) {
      message += ",";
      message += vars[i];
    }
    Serial.print("message sending: ");
    Serial.println(message);
    request->send(200, "text/plain", message);
  });
  
  server.begin();
  Serial.println("Server started"); 
}


void loop() {
  long nowTime;
  if (rtcWorks) {
    DateTime now = rtc.now();
    nowTime = now.unixtime();
  } else {
    nowTime = millis()/1000;
  }
  if (nowTime-lastTime>countDown && (waitTime-countDown)!=0){
    countDown++;
    if (countDown%2==0 && dataRequests>0){
      long datas[10];
      getReadings(datas, false);
      dataRequests = 0;
      
    }
    Serial.println(waitTime-countDown);
  }

  
  if (nowTime > lastTime + waitTime && waitTime > 0) {
    Serial.println("Getting readings");
    long datas[10];
    getReadings(datas, !TESTMODE);
    
    if (!rtcWorks){
      lastTime = millis()/1000;
    } else {
      DateTime now = rtc.now();
      lastTime = now.unixtime();
    }
    countDown = 0;
    
  }
}
