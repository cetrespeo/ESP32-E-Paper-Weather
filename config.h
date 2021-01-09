//vvvvvvvvvvvvvv EDIT ONLY THESE VALUES vvvvvvvvvvvvvvvvvvvv

// ########### REQUIRED TO WORK ################
// Board and display should be selected on .ino file
String sWifiDefaultJson = "{\"YourSSID\":\"YourPassword\"}";    //customize YourSSID and YourPassword with those of your wifi. Allows multiple SSID and PASS in json format
String sWeatherAPI =  "darksky";                                //choose "darksky" or "openweathermap" API
String sWeatherKEY =  "xxxxxxxxxxxxxxxxx";                      //Add your API account key
String sWeatherLOC =  "xx.xx,xx.xx";                            //Add your GPS location as in "43.25,-2.94" (two decimals are more than enough)
String sWeatherLNG =  "en";                                     //read https://darksky.net/dev/docs for other languages as en,fr,de (screen values should also be updated in "ESP32_Disp_Aux.cpp")
const String sTimeZone = "CET-1CEST,M3.5.0,M10.5.0/3";          //for CET. Update your Time Zone with instructions on https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv
String sTimeFirst = "7.00";                                     //Customize the first refresh hour in the morning you need

//###### OPTIONAL VALUES ################
//FIREBASE API PARAMS, Update to yours
#define FIREBASE_HOST "weatheresp32.firebaseio.com"
#define FIREBASE_AUTH "htFuCm2OfTXtP4g5xrQBpSaE0IHYEOrSLoVmSiBF"
//GOOGLE LOCATION
const String sGeocodeAPIKey = "";       //Add your Google API Geocode key (optional)
//GMAIL
#ifdef G_SENDER
  char* sEMAILBASE64_LOGIN = "base64loginkey";                  //for sending events to your email account
  char* sEMAILBASE64_PASSWORD = "base64password";               //for sending events to your email account
  char* sFROMEMAIL = "youremail@gmail.com";                     //for sending events to your email account
  String sEmailDest = "youremail@gmail.com";                    //for sending events to your email account
#endif


////////////////////////////////////////////////////////////////////////////////////////////////////
#ifdef TTGOT5   //TTGOT5 based io connectors
#define WS2
static const uint8_t io_CS        = 5;    //CS
static const uint8_t io_DC        = 17;   //DC
static const uint8_t io_RST       = 16;   //RST
static const uint8_t io_BUSY      = 4;    //BUSY
static const uint8_t io_DS18B20   = 0;
static const uint8_t io_TMP36     = 0;
static const uint8_t io_LED       = 0;
#endif

#ifdef TTGOT7   //TTGOT7 based io connectors
static const uint8_t io_CS        = 5;    //ORANGE -  CS  (usually 5)
static const uint8_t io_RST       = 22;   //WHITE  -  RST (usually 16)
static const uint8_t io_DC        = 21;   //GREEN  -  DC  (usually 17)
static const uint8_t io_BUSY      = 4;    //VIOLET -  BUSY(usually 4)
//static const uint8_t io_CLK     = 18;   //YELLOW -  SCK - Not used in app, but connection is needed
//static const uint8_t io_MOSI    = 23;   //BLUE   -  DIN - Not used in app, but connection is needed
static const uint8_t io_DS18B20   = 0;    //DS18B20 Temp Sensor OPTIONAL
static const uint8_t io_TMP36     = 0;    //TMP36 Temp Sensor OPTIONAL
static const uint8_t io_LED       = 0;    //BOARD LIGHT LED
#endif

#ifdef LOLIN32  //LOLIN32 based io connectors
static const uint8_t io_CS        = 13;   //ORANGE -  CS  (usually 5)
static const uint8_t io_RST       = 14;   //WHITE  -  RST (usually 16)
static const uint8_t io_DC        = 21;   //GREEN  -  DC  (usually 17)
static const uint8_t io_BUSY      = 22;   //VIOLET -  BUSY(usually 4)
//static const uint8_t io_CLK     = 18;   //YELLOW -  SCK - Not used in app, but connection is needed
//static const uint8_t io_MOSI    = 23;   //BLUE   -  DIN - Not used in app, but connection is needed
static const uint8_t io_DS18B20   = 15;   //DS18B20 Temp Sensor OPTIONAL
static const uint8_t io_TMP36     = 34;   //TMP36 Temp Sensor OPTIONAL
static const uint8_t io_LED       = 5;    //BOARD LIGHT LED
#endif

#ifdef WS2
static const uint8_t io_VOLTAGE   = 0;
#else
static const uint8_t io_VOLTAGE   = 35; // PIN WHERE A VOLTAGE DIVIDER FOR BATT VOLTAGE IS CONNECTED
#endif

//GOODDISPLAY 7 BW//////////////////
#if defined(WS7)
#include <GxGDEW075T8/GxGDEW075T8.h>
#endif
//GOODDISPLAY 7 3C//////////////////
#if defined(WS7c)
#include <GxGDEW075Z09/GxGDEW075Z09.h>
#endif
#if defined(WS7) || defined(WS7c)
#define DisableClock
const bool bWS75 = true, bWS58 = false, bWS42 = false, bWS29 = false;
const uint8_t* fU8g2_XS = u8g2_font_7x14_tf;
const uint8_t* fU8g2_S = u8g2_font_helvB12_tf;
const uint8_t* fU8g2_M = u8g2_font_helvB14_tf;
const uint8_t* fU8g2_L = u8g2_font_logisoso18_tf;
const uint8_t* fU8g2_XL = u8g2_font_logisoso38_tf;
const uint8_t* fU8g2_XXL = u8g2_font_logisoso62_tn;
#endif
//GOODDISPLAY 58 BW//////////////////
#if defined(WS5) || defined(WS5c)
#include <GxGDEW0583T7/GxGDEW0583T7.h>
//#define ForceClock
#endif
#if defined(WS5) || defined(WS5c)
#define DisableClock
const bool bWS75 = false, bWS58 = true, bWS42 = false, bWS29 = false;
const uint8_t* fU8g2_XS = u8g2_font_7x14_tf;
const uint8_t* fU8g2_S = u8g2_font_helvB12_tf;
const uint8_t* fU8g2_M = u8g2_font_helvB14_tf;
const uint8_t* fU8g2_L = u8g2_font_logisoso18_tf;
const uint8_t* fU8g2_XL = u8g2_font_logisoso38_tf;
const uint8_t* fU8g2_XXL = u8g2_font_logisoso92_tn ;
#endif//GOODDISPLAY 4 3C//////////////////
#if defined(WS4c)
#include <GxGDEW042Z15/GxGDEW042Z15.h>
#endif
//GOODDISPLAY 4 BW//////////////////
#if defined(WS4) || defined(WS4k)
#include <GxGDEW042T2/GxGDEW042T2.h>
#endif
#if defined(WS4) || defined(WS4c)
#define DisableClock
const bool bWS75 = false, bWS58 = false, bWS42 = true, bWS29 = false;
const uint8_t* fU8g2_XS = u8g2_font_6x13_tf;
const uint8_t* fU8g2_S = u8g2_font_helvB12_tf;
const uint8_t* fU8g2_M = u8g2_font_helvB14_tf;
const uint8_t* fU8g2_L = u8g2_font_logisoso18_tf;
const uint8_t* fU8g2_XL = u8g2_font_logisoso30_tf;
const uint8_t* fU8g2_XXL = u8g2_font_logisoso54_tf;
#endif
#if defined(WS4k)
#define ForceClock
const bool bWS75 = false, bWS58 = false, bWS42 = true, bWS29 = false;
const uint8_t* fU8g2_XS = u8g2_font_6x13_tf;
const uint8_t* fU8g2_S = u8g2_font_helvB12_tf;
const uint8_t* fU8g2_M = u8g2_font_helvB14_tf;
const uint8_t* fU8g2_L = u8g2_font_logisoso18_tf;
const uint8_t* fU8g2_XL = u8g2_font_logisoso30_tf;
const uint8_t* fU8g2_XXL = u8g2_font_logisoso78_tn;
#endif
//WAVESHARE 2.9 BW//////////////////
#if defined(WS2)
#include <GxGDEH029A1/GxGDEH029A1.h>
#endif
#ifdef WS2
#define ForceClock
const bool bWS75 = false, bWS58 = false, bWS42 = false, bWS29 = true;
const uint8_t* fU8g2_XS = u8g2_font_6x13_tf;
const uint8_t* fU8g2_S = u8g2_font_6x13_tf;
const uint8_t* fU8g2_M = u8g2_font_helvB12_tf;
const uint8_t* fU8g2_L = u8g2_font_logisoso20_tf;
const uint8_t* fU8g2_XL = u8g2_font_logisoso32_tf;
const uint8_t* fU8g2_XXL = u8g2_font_logisoso78_tn ;//u8g2_font_logisoso54_tf;
#endif
