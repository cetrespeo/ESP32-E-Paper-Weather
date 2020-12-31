# ESP32 E-Paper Weather forecast

New:
 - OpenWeatherMaps API as well as DarkSky API
 - Languages available; en,fr,de,es

 ![alt text](https://i.imgur.com/dsHalkO.jpg)

 ![alt text](https://cdn.thingiverse.com/renders/75/d6/a7/d1/de/2b9d35bfe5607057e232901f3cf53377_preview_featured.jpg)
  
 ![alt text](https://cdn.thingiverse.com/renders/0d/a7/f2/06/e1/801294e4b230b225e4222f602e4dcfab_preview_featured.jpg)

Arduino based ESP32 standalone device that connects via wifi and reports weather every hour.
Provides a rather technical but detailed info for a 48 hour forecast, loaded from https://api.darksky.net/ (must register to get api key). Also valid with OpenWeatherMap. 
Should remain alive for about 4 months without need to recharge.

Works with;
- 1x Waveshare / Good Display 2.9 (BW), 4.2 (BW or BWR), 5 (BW), 7.5 (BW or BWR) or TTGO T5.  
- 1x ESP32 (preferable Lolin32)
- 1x Li-ion 3,7V battery (preferable 4000-5000mAh)
- (Optional) 2x1MOhm resistors for battery voltage measurement 
- (Optional) 3D printed frame (https://www.thingiverse.com/thing:3135733)
- (Optional) temp sensor for internal Temp meassure

Setup guide at the end of this document.

Hardware

Higly recomendable https://www.e-paper-display.com/products_detail/productId=403.html for communicating with Waveshare screens.

Lolin32 connections from board to DESPI-C02 (or similar);
- SPI
  - pin 13 ORANGE - CS 
  - pin 21 GREEN -  DC 
  - pin 14 WHITE -  RST 
  - pin 22 VIOLET - BUSY 
- POWER
  - any +3.3V RED
  - any GND BLACK
- VOLTAGE SENSOR (optional)
  - pin 35 VOLTAGE (to the middle point of the 1MOhm voltage divider between Bat- and Bat+)
- (optional in GoodDisplay some boards) NOT USED IN APP
  - pin 18 YELLOW - SCK 
  - pin 23 BLUE   - DIN 

Customize in hardcode (must edit config.h file with your values)
- darksky api key (and OpenWeatherMap) 
- Weather report gps location ( "1.23,4.56" format). You can take the gps coordinates from the url on google maps when pointing at your location.
- in case you need Cloud management, Firebase integration api key should be configured. This option is really interesting whenever you have multiple devices as you can download weather data to firebase and use that source as an alternative to consume your Darksky download count limit.
- Once you have many devices (I have around 10 for my family) management via Firebase and OTA Updates is really needed. OTA updates requires a AWS S3 account and Firebase integration for remote command.

# Change Log:

(18/11/11 Update) Added GxEPD driver v1 option as I suspect v2 drivers drain more battery. When using 4,2" screen, driver v1 cycle is 37secs and v2 cycle is 42secs. You can define G1 or G2 to choose version in code.

(18/11/29 Update) Added AWS S3 based OTA upload capabilities. You will need to have a S3 bucket with the bin file uploaded, and then define in the Firebase parameter "OTAUpdate" the name of the bin. The OTA update will be automatically done next time the system reboots.

(18/12/13 Update) Also available Geocoding api to locate the GPS position's Locality name.

(20/03/16 Update) 7 day forecast included as optional. 

(20/10/30) Upgraded to FirebaseESP32

(20/12/11) Added support for OpenWeatherMaps (DarkSky also valid). Now you can choose the API in sWeatherAPI. Don't forget to add a valid key for your API on sWeatherKEY

# IMPORTANT: Resize App Size Partition
As FirebaseESP32 is bigger, you may need to increase partition for apps. 

On \packages\esp32\hardware\esp32\1.0.4\boards.txt file add the following lines:
 lolin32.menu.PartitionScheme.med_spiffs=Medium SPIFFS (Large APPS with OTA)  
 lolin32.menu.PartitionScheme.med_spiffs.build.partitions=med_spiffs  
 lolin32.menu.PartitionScheme.med_spiffs.upload.maximum_size=1572864   
to the file (you may change "lolin32" with the board of your choice).

Then create a new file called "med_spiffs.csv" file on folder \packages\esp32\hardware\esp32\1.0.4\tools\partitions with the following columns and values:

#Name,    Type, Subtype, Offset,  Size, Flags       
 nvs,      data, nvs,     0x9000,  0x5000,  
 otadata,  data, ota,     0xe000,  0x2000,  
 app0,     app,  ota_0,   0x10000, 0x180000,  
 app1,     app,  ota_1,   0x190000,0x180000,     
 spiffs,   data, spiffs,  0x310000,0xF0000,       

Both files are shared on this github root folder.

--------------------------------------------------------------------------------------------------

Front of 4.2" wall version

 ![alt text](https://cdn.thingiverse.com/renders/2c/d5/12/57/fd/295a45b2040e32851cf271885126ecfc_preview_featured.jpg)


Back of 4.2" wall version

 ![alt text](https://cdn.thingiverse.com/renders/24/4b/11/8f/45/ce7e6c3ffa647b41647d48f9c0e5ab42_preview_featured.jpg)

2.9" desk clock version
 
 ![alt text](https://cdn.thingiverse.com/renders/f1/64/9d/ed/0b/6ec0a79d41489d84fe7ac521ba54bc00_preview_featured.jpg)
 
--------------------------------------------------------------------------------------------------

# Graph Explanation
 
  ![alt text](https://cdn.thingiverse.com/renders/4b/1e/79/44/a6/c7bf283a9247fa44293110c41f7d9472_preview_featured.jpg)
 
 - 1  Remaining days of battery expected
 - 2  battery graph
 - 3  battery historical graph
 - 4  Current temp
 - 5  Current weather status
 - 6  Current Date (you can change language in sketch "WeekDayNames")
 - 7  Today's forecast (you can change language in sketch "sWeatherLNG" )
 - 8  This week's forecast
 - 9  Bold Black graph TEMPERATURE
 - 10  Dotted black graph CLOUD %
 - 11  Red filled line PRECIPITATION in mm. Probability is detailed in the width of the dotted bars below red line.
 - 12  Night, defined by sunset and sunrise lines.
 - 13  IP Location or "CustomText" variable in Firebase.
 - 14  Time of last update
 - 15  Weather icons each 6 hours for a 48 hour forecast.

--------------------------------------------------------------------------------------------------

# Setup guide

 Customize hardcoded variables at your glance in the config.h file. Choose your hardware there (WS2,WS4c, etc..) and insert your keys (darksy, wifi and so). You can also add those later.
 
 Wifi setup can also be done in the json file "wifi.txt" added in the data folder. Please note that this folder files must be uploaded to the ESP32's SPIFFS with the arduino app.

Add room for app as described in "Resize App Size Partition". 

You can later change those values through the Web interface in the Menu operation.

# Menu operation
 As there no button is needed for normal operation, configuration is done only with reset button. When you push the reset button, you start a boot cycle of all boot modes; "Standard", "Web Server Setup", "Show Values", "ERASE BATT_LEVEL", "ERASE ALL", "OTA DEFAULT". Whenever a boot page is loaded, if you press reset again in the first 3 seconds after the display is refreshed, then you pass to next boot mode.
  - Standard: this is the base mode and leads to the weather graph
  - Web Server Setup: the device starts a WIFI AP named ESP32 where you can config all functional values
  ![alt text](https://cdn.thingiverse.com/renders/7c/04/ed/43/20/6e7861f0293c0accbaed000fc4c5679e_preview_featured.jpg)
   
   Once you are connected to the ESP32 Wifi AP, open 192.168.4.1 in a browser, and modify the params of choice. You can "Send" the values to the flash config data, or erase all values or return to defaults... Whatever.
  - Show values: shows internal operational parameters
  - ERASE BATT_LEVEL, deletes all values learned from battery behaviour
  - ERASE ALL, deletes all NVS values
  - OTA DEFAULT, loads your S3 default bin and writes it down to the OTA partition

Normal operation does not require any button to be pressed and the only display shown is the weather forecast display.

--------------------------------------------------------------------------------------------------
# Data management

1. Default values are hardcoded and can be updated via a Wifi server included in the app.
2. When updated, the new values are stored in SPIFFs as a backup.
3. Then Firebase is checked and compared with stored values. User can interact with gadget values through Firebase and change gadget behaviour. Any update on the later, will be written as backup to SPIFFs and that data will be used in case of no Wifi (p.e.).

Weather:
1. Weather data is downloaded from API. 
2. When successfully downloaded it is uploaded to Firebase. That helps you in case you have several gadgets in the same location. I have some for my family members and I only consume 20 updates per day of darksky. Firebase information is only valid if one hour old. If older, then it is downloaded again from API.
3. All succesfull downloads are stored in SPIFFs for backup in case of no Wifi.

--------------------------------------------------------------------------------------------------

P.D. I know the code is really long and messy, but it is my sandbox for many projects and I didn't try to make it beautiful. I apologize if you try to understand all the coded processes :-)
