# BeeData

## About:
BeeData is an open-source project to log environmental data over multiple months. This project is specifically directed toward weighing bee hives, but it can be used to log any other type of data. Compared to other data loggers fro the ESP32/ESP8266, it can store data locally or online, and it hosts a website with multiple useful features.

## Setup:
### 1. Set up hardware
You can use any type of sensors you would like, but the suggested sensors and pin connections are:
DS3231 (rtc):
Vcc: 3.3v
GND: GND
SCK: D26
DT: D25

HX711 50kg load cells
GND: GND
VCC: 3.3v
SDA: D21
SCL: D22

DHT22 (temperature + humidity sensor)
1: 3.3v
2: D27
3: GND

Currently, it can log with any combination of the DS3231, HX711, DHT11/22, BMP180, BME280 sensors

### 2. Installing Arduino IDE

Go to https://www.arduino.cc/en/software and download the arduino IDE for your device
file > preferences
enter https://dl.espressif.com/dl/package_esp32_index.json in the “Additional Board Manager URLs” and press okay
Tools > Board > Boards Manager…
search for “ESP32 by Espressif Systems“ and install the board.
It will come with many libraries for the ESP32
tools > Board > ESP32 Arduino > ESP32 dev model
Open the file in this repository titled "datalogger_website_full.ino"

### 3. Installing necessary libraries
tools >  manage libararies
search for and press install for
rtclib, DHTesp.h, HX711.h, try compiling. If this does not work, look at the error messages and try to find the appropriate message.

### 4. Upload sketch
Connect the ESP32 to your computer and upload the sketch. This will take a few seconds to compile and upload.
Go to tools > Serial monitor. Make sure the baud rate is 115200. If you do not readable messages press the boot button on the ESP32.

### 5. Setup within the website
From the Serial Monitor, you should see the a URL, most likely 192.168.4.1. Using the wifi on your computer, connect to the new network that is called something like "ESP32AP_HXo3" and enter the URL in the browser.
You should see a page with multiple tabs. Go to settings and enter the information appropriate to your hardware and needs.
Once complete, you can go to the calibration tab and calibrate the scale using known weights. The ESP32 will continuously send readings to the website.
When you are ready to download the data collected, under the data tab press "download data" to download the data as a csv. 
You can see the amount of availible memory left in the storage tab. It is suggested to download your data and clear the memory before the ESP32 runs out of memory. It will simply stop recording data when it runs out of memory.

## Hardware:
### ESP32:
The ESP32 is similar to an Arduino Uno, but has more built-in features at a lower price. It is the newer version of the ESP8266. It has built-in WIFI (2.4 GHz) and Bluetooth. The ESP32 has 48 pins although only 39 are usually availible. It has 2 cores with a clock speed of 240 MHz, but it can be reduced to conserve power.

The ESP32 series devkit has 4mb of flash. Approximately 1/3 is for the sketch, 1/3 is for the backup sketch, and 1/3 is for SPIFFS. SPIFFS is a file system with wear-leveling and support for several file types, including .txt files. Once set up, the ESP32 is able to log the data.

### HX711
Datasheets:

20 kg bending https://www.robotshop.com/media/files/pdf/datasheet-3135.pdf
50 kg https://www.makerfabs.com/desfile/files/Load%20Cell-50kg.pdf

## Accuracy:
### Weight
The accuracy of the HX711 varies heavily on multiple environmental and setup factors. The accuracy can vary from +/- 0.5 kg over months, all the way to +/- 2 kg over a day. There are several things to consider when determining accuracy:
##### Temperature/humidity
Changes in temperature and humidity change the accuracy fo the scale. The most accurate continuous readings were performed indoors.
##### Wiring
Exposed wires are particularly heavily affected by external factors, particularly humidity. It is suggested that the thin wire should be soldered to a heavier gauge wire before going to the HX711.
    
### temperature/humidity
Temperature and humidity heavily depend on the sensor used. The sensors in order from least accurate to most accurate:
DHT11
BMP180
DHT22
BME280
The BME280 is very accurate, but it produces heat so the actual temperature is usually 2 degrees celcius lower than its reading.

### Time
The DS3231 has very good accuracy. It only varies by a second or two each month. Everytime a device connects to the ESP32's website, it will automatically adjust the time. 
     
As more testing is performed, better information on the accuracy of the devices can be provided and corrections can be made.

## Further Information

### Power:
The ESP32 devkit uses too much power to be powered by a battery alone, even in deep sleep mode. Consult the following link for more information on how to create your own ESP32 module that uses much less power.
https://www.reddit.com/r/esp32/comments/jor1gq/i_made_this_custom_esp32_board_and_its_easy_4ua/

### To contribute
Feel free to rcontribute to this website

### References
Purdue, SURF, Dr. John Evans, Nathan Sprague
