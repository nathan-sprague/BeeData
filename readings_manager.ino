
long getTime() {
  long nowTime;
  if ((millis() / 1000) - lastMillis < 60 && lastAccurateTime != 0) {
    nowTime = (lastAccurateTime - lastMillis) + (millis() / 1000);
  } else {
    if (useAP == false) {
      Serial.println("\n\n\n\nSetting accurate time");

      String link = "http://yolohashtag.com/time";
      String timeStr = clientRequest(link);
      if (timeStr.toInt() > 2) {
        Serial.println("got time from server");
        nowTime = timeStr.toInt();
      } else {
        Serial.println("couldnt get time from server");
        nowTime = (lastAccurateTime - lastMillis) + (millis() / 1000);
      }
      lastAccurateTime = nowTime;
      Serial.println(lastAccurateTime);
    }

    if (rtcWorks) {
      DateTime now = rtc.now();
      nowTime = now.unixtime();
      lastAccurateTime = nowTime;
    } else {
      nowTime = (lastAccurateTime - lastMillis) + millis() / 1000;
      lastAccurateTime = nowTime;
    }
    lastMillis = millis() / 1000;
  }
  return nowTime;
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


bool setupSensors() {
  rtcWorks = true;
  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC");
    rtcWorks = false;
  }
  if (!bme.begin(BME280_ADD)) {
    Serial.println("couldn't find bmp");
  }
  Serial.println("finished setting up devices");

  return true;
}

int getDHT(int DHTnum, int pin, bool getTemp, int precision) {

  return 0;
}

long getHX711(int pin1, int pin2, int precision) {
  Serial.print("scale");
  HX711 scale;
  scale.begin(pin1, pin2); //26, 25
  scale.set_scale(2);
  scale.power_down();
  scale.power_up();
  long res = long(scale.read_average(10) * pow(10, precision));
  scale.power_down();
  return res;
}


int getReadings(long datas[], bool saveData) {
  okayToReadTime = getTime() + 1;

  String message = readFile("sensors");
  numberSensors = 0;

  bool setupDHT11 = false;
  bool setupDHT22 = false;
  DHTesp dht;

  String readingMsg = "";

  int numSensors = 0;

  int i = 0;
  while (i < message.length()) {
    String sensorMsg = parsePart(message, &i, '$');

    int j = 0;
    String fileName = parsePart(sensorMsg, &j, ',');
    String sensorType = parsePart(sensorMsg, &j, ',');
    int pinNums = parsePart(sensorMsg, &j, ',').toInt();
    int precision = parsePart(sensorMsg, &j, ',').toInt();

    Serial.println(sensorType);

    if (TESTMODE) {
      datas[numSensors] = random(1, 100);
    } else {

      if (sensorType == "ds3231") {
        datas[numSensors] = getTime();

      } else if (sensorType == "hx711") {
        HX711 scale;
        scale.begin(pinNums / 100, pinNums % 100); //25, 26
        scale.set_scale(2);
        scale.power_down();
        scale.power_up();
        datas[numSensors] = int(scale.read_average(10) * precision);
        scale.power_down();

      } else if (sensorType == "dht22_temp") {
        Serial.println("dht temp ");
        if (!setupDHT22) {
          dht.setup(pinNums, DHTesp::DHT22);
          setupDHT22 = true;
        }
        datas[numSensors] = int(dht.getTemperature() * pow(10, precision));
        okayToReadTime = getTime() + 1;


      } else if (sensorType == "dht22_humid") {
        Serial.print("dht humid ");
        if (!setupDHT22) {
          dht.setup(pinNums, DHTesp::DHT22);
          setupDHT22 = true;
        }
        datas[numSensors] = int(dht.getHumidity() * pow(10, precision));
        okayToReadTime = getTime() + 1;

      } else if (sensorType == "dht11_temp") {
        Serial.println("dht temp ");
        if (!setupDHT11) {
          dht.setup(pinNums, DHTesp::DHT11);
          setupDHT11 = true;
        }
        datas[numSensors] = int(dht.getTemperature() * pow(10, precision));
        okayToReadTime = getTime() + 2;
        //          datas[numSensors] = random(1,100);
      } else if (sensorType == "dht11_humid") {
        Serial.print("dht humid ");
        if (!setupDHT11) {
          dht.setup(pinNums, DHTesp::DHT11);
          setupDHT11 = true;
        }
        datas[numSensors] = int(dht.getHumidity() * pow(10, precision));
        okayToReadTime = getTime() + 2;

      } else if (sensorType == "bmp180_temp") {
        Serial.print("bmp temp ");
        datas[numSensors] = int(bme.readTemperature() * pow(10, precision));

      } else if (sensorType == "bmp180_press") {
        Serial.print("bmp press ");

        datas[numSensors] = int(bme.readPressure() * pow(10, precision));
      } else if (sensorType == "bmp180_humid") {
        Serial.print("bmp humid ");

        datas[numSensors] = int(bme.readHumidity() * pow(10, precision));

      } else if (sensorType == "hall ") {
        Serial.print("hall");
        datas[numSensors] = hallRead();
      }
      Serial.print("raw value: ");
      Serial.println(datas[numSensors]);
      liveVals[numSensors] = datas[numSensors];
    }

    if (saveData) {
      readingMsg += ',' + String(datas[numSensors]);
      int tBytes = SPIFFS.totalBytes();
      int uBytes = SPIFFS.usedBytes();
      if (uBytes + 100 < tBytes) {
        long writeVal = datas[numSensors] - lastVals[numSensors];
        writeToFile(fileName, writeVal);
        lastVals[numSensors] = datas[numSensors];
      }
    }

    numSensors++;
  }
  if (saveData) {
    String chipId = String((uint32_t)(ESP.getEfuseMac() >> 24), HEX);
    String link = "http://yolohashtag.com/uploadData?username=" + chipId + "&data=" + readingMsg;
    Serial.println(link);
    clientRequest(link);
  }
  numberSensors = numSensors;
  return numSensors;
}
