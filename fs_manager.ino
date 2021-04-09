String charsList = "!#%$&()*+./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[]^_`abcdefghijklmnopqrstuvwxyz{|}";
char posDelim = ',';
char negDelim = '-';
//" !#%$&()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[]^_`abcdefghijklmnopqrstuvwxyz{|}";


bool beginFS() {
  if (!SPIFFS.begin(FORMAT_SPIFFS_IF_FAILED)) {
    Serial.println("SPIFFS Mount Failed");
    return false;
  }

  File file = SPIFFS.open("/settings.txt");
  if (!file || file.isDirectory()) {
    file = SPIFFS.open("/settings.txt", FILE_WRITE);
    Serial.println("making settings file");

    String msg = String(waitTime) + ',' + String(useAP) + ',' + ssid_name + ',' + ssid_pwd + ',' + String(useSI);
    file.print(msg);
    file.close();
    // waitTime, is AP(0,1), network name, network password, SI units(0,1)
  }

  runThroughProgram();
  return true;
}


String longToString(long x) {
  int p = charsList.length();
  String res = "";
  while (x > 0) {
    res = charsList[x % p] + res;
    x /= p;
  }
  return res;
}

long stringToLong(String res) {
  int p = charsList.length();
  long x = 0;
  int i = 0;
  while (i < res.length()) {
    x *= p;
    char b = res[i++];
    int j = 0;
    while (j < p) {
      if (charsList[j++] == b) {
        x += j - 1;
      }
    }
  }
  return x;
}

String readFile(String filename) {
  String fileText = "";
  File file = SPIFFS.open("/" + filename + ".txt");
  if (!file || file.isDirectory()) {
    Serial.println("- failed to open file for reading");
    return "";
  }

  Serial.println("- read from file:");
  while (file.available()) {
    char c = file.read();
    fileText += c;
  }

  return fileText;
}

void listDir(fs::FS &fs, const char * dirname, uint8_t levels) {
  Serial.printf("Listing directory: %s\r\n", dirname);

  File root = fs.open(dirname);
  if (!root) {
    Serial.println("- failed to open directory");
    return;
  }
  if (!root.isDirectory()) {
    Serial.println(" - not a directory");
    return;
  }

  File file = root.openNextFile();
  while (file) {
    if (file.isDirectory()) {
      Serial.print("  DIR : ");
      Serial.println(file.name());
      if (levels) {
        listDir(fs, file.name(), levels - 1);
      }
    } else {
      Serial.print("  FILE: ");
      Serial.print(file.name());
      Serial.print("\tSIZE: ");
      Serial.println(file.size());
    }
    file = root.openNextFile();
  }
}

int getFilenames(String filenames[]) {
  Serial.println("getting file names");
  String message = readFile("sensors");
  int i = 0;
  int n = 0;

  String sensorMsg = parsePart(message, &i, '$');

  while (sensorMsg != "") {
    int j = 0;
    filenames[n] = parsePart(sensorMsg, &j , ',');
    Serial.println("file  name: " + filenames[n]);
    sensorMsg = parsePart(message, &i, '$');
    n++;
  }
  return n;
}

bool writeToFile(String filename, long reading) {
  char delimiter = posDelim;
  if (reading < 0) {
    delimiter = negDelim;
    reading *= -1;
  }

  Serial.println("reading: " + String(reading));
  String msg = longToString(reading) + delimiter;

  if (reading == 0) {
    msg = ",";
  }

  File file = SPIFFS.open("/" + filename + ".txt", FILE_APPEND);
  if (file  && file.print(msg)) {
    Serial.println(msg + " appended to " + filename);
    Serial.println("size: " + String(file.size()));
    return true;
  } else {
    Serial.println("- append failed");
  }
  return false;
}

bool replaceFile(String filename, String message) {
  Serial.println("removing");
  SPIFFS.remove("/" + filename + ".txt");
  File file = SPIFFS.open("/" + filename + ".txt", FILE_WRITE);
  Serial.println("deleted file");
  file.print(message);
  return true;
}

void wipe(String fileToDelete) {
  Serial.println("deleting data");
  if (fileToDelete == "all" || fileToDelete == "data") {
    String filename;
    File root = SPIFFS.open("/");
    File file = root.openNextFile();
    while (file) {
      filename = file.name();
      if (fileToDelete == "all" || (filename != "/settings.txt" && filename != "/sensors.txt")) {
        SPIFFS.remove(filename);
      }
      file = root.openNextFile();
    }
  } else {
    SPIFFS.remove(fileToDelete);
  }
  ESP.restart();
}

void runThroughProgram() {
  listDir(SPIFFS, "/", 0);
  Serial.println("running through program");
  String reading = "";
  char charVal =  '0';

  // get settings
  // waitTime\n is AP(0,1)\n network name\n network password\n SI units(0,1)
  // 1000\n1\nnetworkname\nwifi12345\n1
  String settingsNames[] = {"", "", "", "", ""};

  String settingsTxt = readFile("settings");
  Serial.println("got settings: " + settingsTxt);

  int i = 0;
  int j = 0;
  while (i < settingsTxt.length() - 1 && j < sizeof(settingsNames)) {
    if (settingsTxt[i] == ',') {
      Serial.println(settingsNames[j]);
      j++;
    } else {
      settingsNames[j] += settingsTxt[i];
    }
    i++;
  }

  waitTime = settingsNames[0].toInt();
  useAP = settingsNames[1].toInt();
  ssid_name = settingsNames[2];
  ssid_pwd = settingsNames[3];
  useSI = settingsNames[3].toInt();


  // get filenames
  String filenames[10];
  int numFiles = getFilenames(filenames);
  Serial.print("number of files:");
  Serial.println(numFiles);


  reading = "";
  for (int i = 0; i < numFiles; i++) {
    int numPosReadings = 0;
    int numNegReadings = 0;
    Serial.println("opening: " + filenames[i]);
    File file = SPIFFS.open("/" + filenames[i] + ".txt");
    if (!file || file.isDirectory()) {
      Serial.println("- failed to open file for reading");
    }
    while (file.available()) {
      charVal = file.read();

      if (charVal == posDelim) {
        numPosReadings++;
        //        Serial.print("reading " + reading);
        long numReading = stringToLong(reading);
        //        Serial.print(" number ");
        //        Serial.println(numReading);
        lastVals[i] += numReading;
        //        Serial.println(stringToLong(reading) + " (positive)");
        reading = "";

      } else if (charVal == negDelim) {
        numNegReadings++;
        //  Serial.println(reading);
        lastVals[i] -= stringToLong(reading);

        reading = "";
      } else {
        reading += charVal;

      }
      //     Serial.print(charVal);
    }
    Serial.print("\n\n\nnumber of readings: ");
    Serial.println(String(numPosReadings) + "/ " + String(numNegReadings) + "\n\n");
    Serial.print("last value for " + filenames[i] + ": ");
    Serial.println(lastVals[i]);
    file.close();
  }
}
