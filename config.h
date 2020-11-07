//vvvvvvvvvvvvvv EDIT ONLY THESE VALUES START vvvvvvvvvvvvvvvvvvvv

#define WS      //WS + 2,4,4c,5,7,7c or TTGOT5     <- edit for the Waveshare or Goodisplay hardware of your choice

//Default Params (you can add other through web server param edit page)
String sWifiDefaultJson = "{\"YourSSID\":\"YourPassword\"}";  //customize YourSSID and YourPassword with those of your wifi. Allows multiple SSID and PASS in json format
String sWeatherAPI =  "xxxxxxxxxxxxxxxxx";                  //Add your darsk sky api account key
String sWeatherLOC =     "xx.xx,xx.xx";               //Add your GPS location as in "43.25,-2.94" (two decimals are enough;
String sWeatherLNG =  "en";                       //read https://darksky.net/dev/docs for other languages as en,fr,de (screen values should also be updated in "ESP32_Disp_Aux.cpp")
const String sGeocodeAPIKey = "xxxxxxxxxxxxxxxxxxxxxxxx";   //Add your Google API Geocode key (optional)
#ifdef G_SENDER
  char* sEMAILBASE64_LOGIN = "base64loginkey";                  //for sending events to your email account
  char* sEMAILBASE64_PASSWORD = "base64password";               //for sending events to your email account
  char* sFROMEMAIL = "youremail@gmail.com";                     //for sending events to your email account
  String sEmailDest = "youremail@gmail.com";                    //for sending events to your email account
#endif
const String sTimeZone = "CET-1CEST,M3.5.0,M10.5.0/3";  //for CET. Update your Time Zone with instructions on https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv
String sTimeFirst = "7.00";                       //Add the first refresh hour in the morning
//###### OPTIONAL EDIT ONLY THESE VALUES START ################
//FIREBASE API PARAMS, Update to yours
#define FIREBASE_HOST "weatheresp32.firebaseio.com"
#define FIREBASE_AUTH "htFuCm2OfTXtP4g5xrQBpSaE0IHYEOrSLoVmSiBF"
//Weather
const String sWeatherURL =  "https://api.darksky.net/forecast/";
const String sWeatherFIO =  "api.darksky.net";
//^^^^^^^^^^^^^^^^^ EDIT ONLY THESE VALUES END ^^^^^^^^^^^^^^^^^
