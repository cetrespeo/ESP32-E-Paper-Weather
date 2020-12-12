//vvvvvvvvvvvvvv EDIT ONLY THESE VALUES vvvvvvvvvvvvvvvvvvvv

// ########### REQUIRED TO WORK ################
#define WS4c                                                    //Choose one WS + 2,4,4c,5,7,7c or TTGOT5     <- edit for the Waveshare or Goodisplay hardware of your choice. WS+size+(c if display has color). P.e. WS2, WS$, WS4c, WS7c, WS5, WS5c...
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
