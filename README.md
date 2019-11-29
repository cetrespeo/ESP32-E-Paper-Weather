# ESP32 E-Paper Weather forecast

New: Languages available; en,fr,de,es

 ![alt text](https://cdn.thingiverse.com/renders/75/d6/a7/d1/de/2b9d35bfe5607057e232901f3cf53377_preview_featured.jpg)
  
 ![alt text](https://cdn.thingiverse.com/renders/0d/a7/f2/06/e1/801294e4b230b225e4222f602e4dcfab_preview_featured.jpg)

Arduino based ESP32 standalone device that connects via wifi and reports weather every hour. 
Provides a rather technical but detailed info for a 48 hour forecast, loaded from https://api.darksky.net/ (must register to get api key).
Should remain alive for about 4 months without need to recharge.

Works with;
- 1x Waveshare / Good Display 2.9 (BW) or 4.2 (BW or BWR) or 7.5 (BW) or TTGO T5
- 1x ESP32 (preferable Lolin32)
- 1x Li-ion 3,7V battery (preferable 4000-5000mAh)
- (Optional) 2x1MOhm resistors for battery voltage measurement 
- (Optional) 3D printed frame (https://www.thingiverse.com/thing:3135733)

Although version is fully operative, as code is still really mesy, will be continously improving. 

Setup guide at the end of this document.

Lolin32 connections from board to display;
  - pin 13 ORANGE - CS
  - pin 21 GREEN -  DC
  - pin 14 WHITE -  RST
  - pin 22 VIOLET - BUSY
  - any +3.3V RED
  - any GND BLACK
  - pin 35 VOLTAGE (to the middle point of the 1MOhm voltage divider between Bat- and Bat+)
  - (optional in GoodDisplay some boards) pin 18 YELLOW - SCK
  - (optional in GoodDisplay some boards) pin 23 BLUE   - DIN

Customize in hardcode;
- darksky api key (can use other sources as OpenWeatherMap but then source should be updated) 
- Weather report gps location ( "1.23,4.56" format). You can take the gps coordinates from the url on google maps when pointing at your location.
- in case you need Cloud management, Firebase integration api key should be configured.
- OTA Update requires a AWS S3 account and Firebase integration for remote command.

Also available, cloud Firebase management (requires api key) in order to monitor remotely.

(18/11/11 Update) Added GxEPD driver v1 option as I suspect v2 drivers drain more battery. When using 4,2" screen, driver v1 cycle is 37secs and v2 cycle is 42secs. You can define G1 or G2 to choose version in code.

(18/11/29 Update) Added AWS S3 based OTA upload capabilities. You will need to have a S3 bucket with the bin file uploaded, and then define in the Firebase parameter "OTAUpdate" the name of the bin. The OTA update will be automatically done next time the system reboots.

(18/12/13 Update) Also available Geocoding api to locate the GPS position's Locality name.

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

 As there no button is needed for normal operation, configuration is done only with reset button. When you push the reset button, you start a boot cycle of all boot modes; "Standard", "Web Server Setup", "Show Values", "ERASE BATT_LEVEL", "ERASE ALL", "OTA DEFAULT". Whenever a boot page is loaded, if you press reset again in the first 3 seconds after the display is refreshed, then you pass to next boot mode.
  - Standard: this is the base mode and leads to the weather graph
  - Web Server Setup: the device starts a WIFI AP named ESP32 where you can config all functional values
  ![alt text](https://cdn.thingiverse.com/renders/7c/04/ed/43/20/6e7861f0293c0accbaed000fc4c5679e_preview_featured.jpg)
   
   Once you are connected to the ESP32 Wifi AP, open 192.168.4.1 in a browser, and modify the params of choice. You can "Send" the values to the flash config data, or erase all values or return to defaults... Whatever.
  - Show values: shows internal operational parameters
  - ERASE BATT_LEVEL, deletes all values learned from battery behaviour
  - ERASE ALL, deletes all NVS values
  - OTA DEFAULT, loads your S3 default bin and writes it down to the OTA partition

Normal operation does not require any button to be pressed and the only display shown is the weather forecast display
