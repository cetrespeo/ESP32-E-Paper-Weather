/******************************************************************************
   Copyright 2018. Used external Libraries (many thanks to the authors for their great job):
  Included in Arduino libraries; ArduinoJson (Benoit Blanchon 5.13.1 max), OneWire, DallasTemperature, Adafruit Gfx, u8glib (Oliver Kraus), GxEPD (Jean-Marc Zingg)
  Not in Arduino library;  https://github.com/ioxhop/IOXhop_FirebaseESP32 from ioxhop
 *****************************************************************************/
#define CONFIG_ESP32_WIFI_NVS_ENABLED 0 // trying to prevent NVS + OTA corruptions
#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <SPI.h>
#include <rom/rtc.h>
#include <esp_wifi.h>
#include <esp_bt.h>
#include <esp_bt_main.h>
#include <esp_system.h>
#include <esp_sleep.h>
#include <driver/adc.h>
#include <FS.h>
#include <SPIFFS.h>
#include <nvs.h>
#include <nvs_flash.h>
#include <ArduinoJson.h>              // Max 5.13.1
#include <IOXhop_FirebaseESP32.h>     // Not running with ESP32 board 1.0.3
#include <OneWire.h>
#include <DallasTemperature.h>
#include "Gsender.h"                  // by Boris Shobat!
#include "WeatherIcons.h"
#include "ESP32_Disp_Aux.h"
#include "WDWebServer.h"
#include "GxEPD.h"
#include "GxIO/GxIO_SPI/GxIO_SPI.cpp"
#include "GxIO/GxIO.cpp"
#include "additions/U8G2_FONTS_GFX.h"

static const char REVISION[] = "1.54";

//################ EDIT ONLY THESE VALUES START ################

#define WS4c //WS 2,4,4c,5,5c,7,7c or TTGOT5     <- edit for the Waveshare or Goodisplay hardware of your choice

//Default Params (you can add other through web server param edit page)

  String sWifiDefaultJson = "{\"YourSSID\":\"YourPassword\"}";	// customize YourSSID and YourPassword with those of your wifi. Allows multiple in json format
  String sWeatherAPI =  "xxxxxxxxxxxxxxxxx";                 	// Add your darsk sky api account key
  const String sGeocodeAPIKey = "xxxxxxxxxxxxxxxxxxxxxxxx";  	// Add your Google API Geocode key (optional)
  String sWeatherLOC =     "xx.xxx,xx.xxx";		 		            // Add your GPS location as in "43.258,-2.946";
  String sWeatherLNG =  "en";     							              // read https://darksky.net/dev/docs for other languages as en,fr,de
  char* sEMAILBASE64_LOGIN = "base64loginkey";                // for sending events to your email account
  char* sEMAILBASE64_PASSWORD = "base64password";             // for sending events to your email account
  char* sFROMEMAIL = "youremail@gmail.com";                   // for sending events to your email account

String sTimeFirst = "7.00";						            // Add the first refresh hour in the morning
//###### OPTIONAL EDIT ONLY THESE VALUES START ################
#ifdef TTGOT5
#define WS2
#endif
//FIREBASE API PARAMS, Update to yours
#define FIREBASE_HOST "weatheresp32.firebaseio.com"
#define FIREBASE_AUTH "htFuCm2OfTXtP4g5xrQBpSaE0IHYEOrSLoVmSiBF"
//Weather
const String sWeatherURL =  "https://api.darksky.net/forecast/";
const String sWeatherFIO =  "api.darksky.net";
//################ EDIT ONLY THESE VALUES END ################
#ifdef TTGOT5
static const uint8_t io_CS        = 5;    // CS
static const uint8_t io_DC        = 17;   // DC
static const uint8_t io_RST       = 16;   // RST
static const uint8_t io_BUSY      = 4;    // BUSY
static const uint8_t io_DS18B20   = 0;
static const uint8_t io_TMP36     = 0;
static const uint8_t io_LED       = 0;
static const uint8_t io_VOLTAGE   = 0;
#else
//LOLIN32 based io connectors
static const uint8_t io_CS        = 13;   // ORANGE -  CS
static const uint8_t io_DC        = 21;   // GREEN  -  DC
static const uint8_t io_RST       = 14;   // WHITE  -  RST
static const uint8_t io_BUSY      = 22;   // VIOLET -  BUSY
//static const uint8_t io_CLK     = 18;   // YELLOW -  SCK - optional
//static const uint8_t io_MOSI    = 23;   // BLUE   -  DIN - optional
static const uint8_t io_DS18B20   = 15;
static const uint8_t io_TMP36     = 34;
static const uint8_t io_LED       = 5;
static const uint8_t io_VOLTAGE   = 35;
#endif

//GOODDISPLAY 7 BW//////////////////
#if defined(WS7)
#include "GxGDEW075T8/GxGDEW075T8.cpp"
#endif
//GOODDISPLAY 7 3C//////////////////
#if defined(WS7c)
#include "GxGDEW075Z09/GxGDEW075Z09.cpp"
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
#if defined(WS5)
#include "GxGDEW0583T7/GxGDEW0583T7.cpp"
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
#include "GxGDEW042Z15/GxGDEW042Z15.cpp"
#endif
//GOODDISPLAY 4 BW//////////////////
#if defined(WS4) || defined(WS4k)
#include "GxGDEW042T2/GxGDEW042T2.cpp"
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
#include "GxGDEH029A1/GxGDEH029A1.cpp"
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

/////////////////////////////////////////////////////////////////////
//WIFI PARAMS
String sWifiSsid     = "", sWifiPassword = "";
String sMACADDR, sRESETREASON;
// Conditions
#define ANALYZEHOURS 48
#define PERIODBETWEENVTGS 300
#define VTGMAXIMALLOWED 4095
#define MINBATTFACTOR 0.8
#define FORMAT_SPIFFS_IF_FAILED true
#define LED_LIGHT_LEVEL 100
#define CONFIG_ESP32_WIFI_NVS_ENABLED 0 // trying to prevent NVS + OTA corruptions
const char compile_date[] = __DATE__; // " " __TIME__;
// Weather API
int32_t aHour[ANALYZEHOURS], tSunrise, tSunset;
int aHumid[ANALYZEHOURS];
float aTempH[ANALYZEHOURS], aPrecip[ANALYZEHOURS], aPrecipProb[ANALYZEHOURS], aCloudCover[ANALYZEHOURS], aWindSpd[ANALYZEHOURS], aWindBrn[ANALYZEHOURS];
String aIcon[ANALYZEHOURS], sSummaryDay, sSummaryWeek, sCustomText, sDevID;
float fCurrTemp, fInsideTemp = -100, fTempInOffset = 0;
int32_t  tTimeLastVtgMax, tTimeLastVtgChg, tTimeLastVtgDown, iVtgMax, iVtgChg, iVtgMin, iVtgHPeriodMax, iVtgPeriodDrop, iVtgStableMax, iVtgStableMin;
bool bGettingRawVoltage = false;
int iLastVtgNotWritten = 0, iBattStatus = 0, iRefreshPeriod = 60;
int iLogMaxSize = 512, iLogFlag = 2;
const char* sBattStatus[5] = {"----", "CHGN", "FULL", "DSCH", "EMPT"};
unsigned long iStartMillis;
bool bClk = false, bResetBtnPressed = false, bInsideTempSensor = false, bHasBattery = false, bRed, bWeHaveWifi = false ,  bLogUpdateNeeded = false;
int  iScreenXMax, iScreenYMax;
int iSPIFFSWifiSSIDs = -1, iLedLevel = 0;;
bool bSPIFFSExists, bFBDownloaded = false, bEraseSPIFFJson = false, bFBAvailable = false, bFBDevAvail = false, bFBLoadedFromSPIFFS = false ;
String sDefLog , sJsonDev, sJsonDevOld , sEmailDest = "ubernedo@gmail.com";

GxIO_Class io(SPI, io_CS, io_DC, io_RST);
GxEPD_Class display(io, io_RST, io_BUSY);
U8G2_FONTS_GFX u8g2Fonts(display);

RTC_DATA_ATTR bool bFBModified;
RTC_DATA_ATTR int32_t iBootWOWifi, tNow, tFirstBoot, tLastFBUpdate, tLastSPIFFSWeather, lSecsOn, lBoots, iVtgVal[VTGMEASSURES], tVtgTime[VTGMEASSURES];

void DisplayU8Text(int x, int y, String text, const uint8_t *font, uint16_t color = GxEPD_BLACK );
void DisplayU8TextAlignBorder(int x, int y, String text, const uint8_t *font, int iAlignMode, int iBorderSize, uint16_t color = GxEPD_BLACK );
void drawLine(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t w, uint16_t trama, uint16_t color = GxEPD_BLACK);
void drawBatt(uint16_t x, uint16_t y, uint16_t sx, uint16_t sy, int BattPerc, uint16_t color = GxEPD_BLACK);
void drawBar(int x1, int y1, int x2, int y2, int trama, uint16_t color = GxEPD_BLACK);
void DisplayWXicon(int x, int y, String IconName, uint16_t color = GxEPD_BLACK);
bool LogAlert(String sText, int iLevel = 1) ;
void tGetRawVoltage(void * pvParameters);
void tLoopWebServer(void * pvParameters);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void setup() {
  bInitFrame();
  bLoopModes();
  bEndSetup();
}
////////////////////////// loop ////////////////////////////////////
void loop() {
  int i1, i2, i3, iTemp;
  if (!bClk) SendToSleep(1);
  tNow = time(nullptr) ;
  if (iLedLevel) CheckLed();
  iTemp = round(fGetTempTime(tNow));
  if ((-40 < iTemp) && (iTemp < 50))  fCurrTemp = iTemp;
  bCheckInternalTemp();

  i1 = (int)((tNow - tLastFBUpdate) / 60 - 1) ;
  i2 = round(iRefreshPeriod / 3 );
  i3 = i1 % i2;
  if (i3 == 0) {
    Serial.println(" -- CLOCK data refresh --");
    if (WiFi.status() != WL_CONNECTED) if (!StartWiFi(20)) LogDef("No Wifi @3", 2);
    if (bWeHaveWifi) {
      FB_SetMisc();
      bFlushPartialJsonDev("Misc");
      if (bLogUpdateNeeded) {
        bFlushPartialJsonDev("Log1");
        bFlushPartialJsonDev("Log2");
        bFlushPartialJsonDev("Log3");
      }
      FBCheckLogLength();
      bFBModified = false;
      bFBDownloaded = false;
      bGetPartialJsonDev("Functions");
      bFBModified = false;
      bFBDownloaded = false;
      bGetPartialJsonDev("vars");
      FB_ApplyFunctions();
      bGetFBVars();
      if (bHasBattery) {
        dGetVoltagePerc();
        bFlushPartialJsonDev("Vtg");
      }
      writeSPIFFSFile("/json.txt", sJsonDev.c_str());
    }
    bGetWeatherForecast();
    WiFi.disconnect();
  }
  bNewPage();
  display.fillScreen(GxEPD_WHITE);
  if (bHasBattery) dGetVoltagePerc();
  DisplayForecast();
  delay(5000);
  int iHour = hour(tNow);
  if ((iHour > 1) && (iHour < 5)) {
    int iMinute = 5 - (minute(tNow) % 5);
    if (iMinute < 1) iMinute = 5;
    bRefreshPage();     //From 2 to 4 perform a full update
    delay(60000 * iMinute);
  }  else {
    bPartialDisplayUpdate();
    delay(50000);
  }
  Serial.println("  Display updated...");
}
////////////////////////// FUNCTIONS ////////////////////////////////////
bool bInitFrame() {
#ifndef ForceClock
  bClk = bWS29;
#else
  bClk = true;
#endif
#ifdef DisableClock
  bClk = false;
#endif
  String sAux1, sAux2;
  int i, iAux, iAux2;
  esp_bluedroid_disable();
  esp_bt_controller_disable();
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);
  pinMode (5, OUTPUT);
  digitalWrite (5, HIGH); // Switch off LOLIN32 Blue LED
  if (io_VOLTAGE) adcAttachPin(io_VOLTAGE);
  if (io_VOLTAGE) bHasBattery = (analogRead(io_VOLTAGE) > 200);
  else bHasBattery = false;
  if ((bClk) && (io_LED)) {
    ledcSetup(0, 2000, 8);
    ledcAttachPin(io_LED, 0);
    ledcWriteTone(0, 2000);
  }
  Serial.begin(115200);
  sMACADDR = getMacAddress();
  if (sDevID == "") sDevID = sMACADDR ;
  sRESETREASON = sGetResetReason();
  bResetBtnPressed = (sRESETREASON == "PON") || (sRESETREASON == "RTC");
  String sPlatf = sPlatform();
  Serial.println("\n--------------------------------------------");
  bInsideTempSensor = bCheckInternalTemp();
  Serial.printf( "   2Day_Forecast %s of %s\n", REVISION, __DATE__ );
  Serial.println("   boot " + sRESETREASON + " @ " + (tNow ? sGetDateTimeStr(tNow) : "NO TIME") + " cpu:" + (String)(temperatureRead()) + "ºC Hall:" + (String)(hallRead()) + (bInsideTempSensor ? " In:" + (String)(fInsideTemp + fTempInOffset) + "ºC" : " NO-TEMP "));
  Serial.printf( "   Heap=%d Vtg=%d %dsecs\n", ESP.getFreeHeap(), (bHasBattery ? analogRead(io_VOLTAGE) : 0), (lBoots > 0 ? (lSecsOn / lBoots) : 0));
  Serial.println("   MAC=" + sMACADDR + " is " + sPlatf + (String)(bClk ? " CLOCK" : " NO-CLOCK"));
  Serial.println("--------------------------------------------" );
  if (!SPIFFS.begin(FORMAT_SPIFFS_IF_FAILED)) {
    Serial.println("SPIFFS Mount Failed");
    bSPIFFSExists = false;
  } else bSPIFFSExists = true;
  listSPIFFSDir("/", 2);
  Serial.println("--------------------------------------------" );
  ///////////////////////////////////////////////////
  if (bSPIFFSExists) {
    sJsonDevOld = readSPIFFSFile("/json.txt");
    DynamicJsonBuffer jsonBuffer(256);
    JsonObject& root = jsonBuffer.parseObject(sJsonDevOld);
    if (!root.success()) {
      sJsonDevOld = "";
      Serial.print(" sJsonDevOld NOK ");
      deleteSPIFFSFile("/json.txt");
    } else {
      Serial.print(" sJsonDevOld OK ");
      if ((!tNow) && (sJsonDevOld.length() > 3)) {
        tNow = root["Vtg"]["VHist"]["VtgTime23"];
        Serial.print(" [SPIFFS_Clock:" + sGetDateTimeStr(tNow) + "] ");
      }
    }
  } else  sJsonDevOld = "";
  if (bSPIFFSExists) LoadDefLog(); // No Deffered logs before this point
  if (SPIFFS.exists("/otaupdat.txt")) {
    deleteSPIFFSFile("/otaupdat.txt");
    EraseAllNVS();
    SendToRestart();
  }
  if (sRESETREASON == "RST") { //To prevent NVS bootloop
    sAux1 = "";
    sAux1 = readSPIFFSFile("/rstwoboo.txt"); // Resets without boot
    if (sAux1 == "") iAux = 0;
    iAux = sAux1.toInt() + 1;
    delay(1000);
    writeSPIFFSFile("/rstwoboo.txt", String(iAux).c_str());
    delay(1000);
    if (iAux > 1) Serial.println("\n" + sAux1 + " resets in a row");
    if (iAux > 9) {
      delay(100);
      EraseAllNVS();
      delay(100);
      if (SPIFFS.exists("/rstwoboo.txt")) deleteSPIFFSFile("/rstwoboo.txt");
      delay(100);
      LogDef(" WIFI BOOTLOOP DETECTED", 3);
      delay(100);
      SendToRestart();
    }
  }
  if (SPIFFS.exists("/lowvolt.txt")) bCheckLowVoltage();
  Serial.print("\nLoading Wifi.txt...");
  iSPIFFSWifiSSIDs =  iLoadWifiMulti();
  if ((sWifiDefaultJson != "") && (iSPIFFSWifiSSIDs < 1)) {
    writeSPIFFSFile("/wifi.txt", sWifiDefaultJson.c_str());
    iSPIFFSWifiSSIDs = iLoadWifiMulti();
    writeSPIFFSFile("/resets.txt", "0");
    LogDef("Default Wifi.txt Added", 2);
    delay(500);
    SendToRestart();
  }
  if (!StartWiFi((bResetBtnPressed ? 5 : 20))) {
    if (sJsonDevOld.length() > 0) {
      sJsonDev = sJsonDevOld;
      bFBDownloaded = (sJsonDev.length() > 4);
      bFBLoadedFromSPIFFS = true;
    } else {
      if (!bResetBtnPressed) {
        LogDef("No Wifi, No DevOld @1", 2);
        SendToSleep(5); // Nothing to do
      }
    }
    LogDef("No Wifi, Yes DevOld @1", 2);
  } else bCheckAWSSPIFFSFiles();
  // WE SHOULD HAVE WIFI ////////////////////////////////
  //
  //
  iAux = hallRead();
  if (abs(iAux) > 70) {
    if (abs(iAux) > 150) LogDef("Hall=" + (String)(iAux), 2);
    else LogDef("Hall=" + (String)(iAux), 1);
  }
  CheckLed();
  Serial.printf("  SPIFFS: %d WifiSsids, Vtg{%d>%d>%d>%d}", iSPIFFSWifiSSIDs, iVtgMax, iVtgChg, (io_VOLTAGE ? analogRead(io_VOLTAGE) : 0), iVtgMin);
  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);
  bFBAvailable = bCheckFBAvailable();
  bFBDevAvail = bCheckFBDevAvailable(true);
  if (!bFBDevAvail) {
    if (sJsonDevOld.length() > 0) {
      sJsonDev = sJsonDevOld;
      bFBDownloaded = (sJsonDev.length() > 4);
      bFBLoadedFromSPIFFS = true;
    }
  }
  Serial.print("  Wifi:" + (String)(bWeHaveWifi) + ",Firebase:" + (String)(bFBAvailable) + ",Dev:" + (String)(bFBDevAvail) + " ");
  bCheckJsonDevStruct();
  bGetVtgValues(sJsonDev);
  bGetFBVars();
  iAux2 = iLoadSlopesSPIFFS(iVtgVal, tVtgTime, VTGMEASSURES);
  FBCheckLogLength();
  FlushDefLog();
  //LogAlert("Initial EMAIL test", 3);
  if (tVtgTime[0] == 0 ) FillEmptyVtgVal();
  lBoots += 1;
  Serial.printf("\n  Updated %d times, on core %d since %s.", lBoots, xPortGetCoreID(), (tFirstBoot ? sInt32TimetoStr(tFirstBoot).c_str() : "never"));
  bInsideTempSensor = bCheckInternalTemp();
  //Initiate display
  display.init();
  display.setRotation(bWS29 ? 1 : 0);
  display.setTextColor(GxEPD_BLACK);
#if defined(WS7c) || defined(WS4c)
  bRed = true;
#else
  bRed = false;
#endif
  iScreenXMax = display.width();
  iScreenYMax = display.height();
  u8g2Fonts.begin(display);
  u8g2Fonts.setFontMode(1);
  u8g2Fonts.setFontDirection(0);
  u8g2Fonts.setForegroundColor(GxEPD_BLACK);
  u8g2Fonts.setBackgroundColor(GxEPD_WHITE);
  u8g2Fonts.setFont(fU8g2_M);
  FB_ApplyFunctions();
  bNewPage();
  display.fillScreen(GxEPD_WHITE);
  return true;
} //////////////////////////////////////////////////////////////////////////////
bool  bLoopModes() {
  String sAux1, sAux2;
  int i, iAux, iAux2;
  const char* sBootModes[] = {"---", "Standard", "Web Server Setup", "Show Values", "ERASE BATT_LEVEL", "ERASE ALL", "OTA DEFAULT", "Standard"};
  //BootMode MENU
  if ((lBoots < 2) && (bResetBtnPressed)) {
    if (!bRed) {
      delay(100);
      display.fillScreen(GxEPD_BLACK);
      delay(100);
      bRefreshPage();
      delay(100);
      bNewPage();
      delay(100);
      display.fillScreen(GxEPD_WHITE);
      delay(100);
      bRefreshPage();
      delay(100);
      bNewPage();
    }
    sAux1 = readSPIFFSFile("/resets.txt");
    iAux = sAux1.toInt() + 1;
    sAux1 = sBootModes[iAux];
    Serial.printf("**MENU:%d reboots-> %s ", iAux,  sAux1.c_str());
    if (iAux > (sizeof(sBootModes) / sizeof(sBootModes[0]) - 2)) iAux = 1;
    sAux1 = (String)(iAux);
    writeSPIFFSFile("/resets.txt", sAux1.c_str());
    if (SPIFFS.exists("/rstwoboo.txt")) deleteSPIFFSFile("/rstwoboo.txt");
    sAux1 = " Weather Station " + (String)(REVISION) + "/" +  (String)(sPlatform()) + "   -   " + (String)(compile_date);
    DisplayU8Text(1 , 20, sAux1, fU8g2_M);
    sAux1 = "   WiFi SSID=" + sWifiSsid + " @IP:" + WiFi.localIP().toString().c_str() + " " + sMACADDR;
    DisplayU8Text(1 , 60, sAux1, fU8g2_XS);
    sAux1 = "   Weather API = " + sWeatherAPI;
    DisplayU8Text(1 , 80, sAux1, fU8g2_XS);
    sAux1 = "   Weather LOC = " + sWeatherLOC;
    DisplayU8Text(1 , 100, sAux1, fU8g2_XS);
    sAux1 = "   Vtg{" + (String)(iVtgMax) + " > " + (String)(iVtgChg) + " > " + (String)(io_VOLTAGE ? analogRead(io_VOLTAGE) : 0) + " > " + (String)(iVtgMin) + "}";
    if (tTimeLastVtgChg > 0) sAux1 += " (Last Time Chg) = " + String((time(nullptr) - tTimeLastVtgChg) / 3600) + "h ago.";
    DisplayU8Text(1 , 120, sAux1, fU8g2_XS);
    //    sAux1 = "   IP City:" + stWD.sCity + " GPS:" + stWD.sGPS + " " ;
    sAux1 = "   Hall sensor=" + (String)(hallRead()) + " CpuTemp=" + (String)(temperatureRead()) + "ºC";
    if (bInsideTempSensor) sAux1 = sAux1 + " Inside:" + (String)(fInsideTemp + fTempInOffset) + "ºC";
    DisplayU8Text(1 , 140, sAux1, fU8g2_XS);
    if (iAux2 > 0) sAux2 = String(iAux2) + " Vtgtimes 0.";
    else sAux2 = String((tVtgTime[VTGMEASSURES - 1] - tVtgTime[0]) / 3600) + "h of data.";
    sAux1 = "   Slope: 1=" + String(fLinearfit(iVtgVal, tVtgTime, VTGMEASSURES, 1)) + ",4=" + String(fLinearfit(iVtgVal, tVtgTime, VTGMEASSURES, 4)) + ",24=" + String(fLinearfit(iVtgVal, tVtgTime, VTGMEASSURES, 24)) + " & " + sAux2;
    DisplayU8Text(1 , 160, sAux1, fU8g2_XS);
    sAux1 = "  Wait 3 secs to enter {" + String(sBootModes[iAux]) + "} Mode..." ;
    DisplayU8Text(1 , 240, sAux1, fU8g2_S);
    sAux1 = "  Reset again to enter {" + String(sBootModes[iAux + 1]) + "} Mode. ";
    DisplayU8Text(1 , 280, sAux1, fU8g2_S);
    bRefreshPage();
    if (bHasBattery) AddVtg(iGetMeanAnalogValue(io_VOLTAGE, 5, 5));
    bNewPage();
    display.fillScreen(GxEPD_WHITE);
    display.setTextColor(GxEPD_BLACK);
    writeSPIFFSFile("/resets.txt", "0");
    Serial.println("Entering Bootmode = " + String(sBootModes[iAux]));
    switch (iAux ) {
      case 2:
        LogAlert("Loading Web Server", 3);
        setWSPlatform(sPlatform());
        StartWifiAP(true);
        startWebServer();
        WiFi.disconnect();
        SendToRestart();
        break;
      case 3:
        LogAlert("Loading Show Value Screen", 3);
        ShowValueScreen();
        SendToSleep(0);
        break;
      case 4:
        EraseVMaxVmin();
        SendToRestart();
        break;
      case 5:
        EraseAllNVS();
        SendToRestart();
        break;
      case 6:
        OTADefault();
        SendToRestart();
        break;
      default:
        break;
    }
  } else if (bHasBattery) AddVtg(iGetMeanAnalogValue(io_VOLTAGE, 5, 5)); //MENU ENDED
  return true;
}//////////////////////////////////////////////////////////////////////////////
bool bEndSetup() { //With standard
  if (WiFi.status() != WL_CONNECTED) {
    if (!StartWiFi(20)) {
      iBootWOWifi++;
      LogDef("No Wifi @2" + String(iBootWOWifi) + " times", 2);
    }
  } else   iBootWOWifi = 0;
  if (!bGetWeatherForecast()) {
    LogAlert("NO forecast", 2);
    if ((lBoots < 2) && (bResetBtnPressed)) {
      DisplayU8Text(1 , 60, " Unable to download forecast ", fU8g2_L);
      bRefreshPage();
    }
    SendToSleep(10);
  }
  if (bHasBattery) dGetVoltagePerc();
  if (bWeHaveWifi) FB_SetMisc();
  DisplayForecast();
  if (!bClk) {
    int iSleepPeriod = iMintoNextWake(tNow);
    Serial.print(" Display Update ");
    bRefreshPage();
    SendToSleep(iSleepPeriod);
  } else {
    Serial.print(" Display Update for CLOCK");
    iStartMillis = 0;
    FlushDefLog();
    if ((bFBModified) && (WiFi.status() == WL_CONNECTED)) bFlushJsonDev(false);
    writeSPIFFSFile("/json.txt", sJsonDev.c_str());
    tLastFBUpdate = tNow;
    delay(40000);
  }
  return true;
}////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void DisplayForecast() {
  float yH = 70 * iScreenYMax / 300;
  float yL = 227 * iScreenYMax / 300;
  float xC = .1, yC = .18;
  int iTemp, iTmp1, iTmp2, iAux, iMaxCharsPerLine = 30, i, j, yVal, iColor, iOffsetH = 3;
  const int iArrOx = 41, iArrOy = 8, iArrOs = 16;
  String sAux1, sAux2;
  if (bWS75) {
    yH = 80;    yL = 308;    xC = .1; yC -= .01; iMaxCharsPerLine += 12;
  }
  if (bWS58) {
    yH = 130;    yL = 370;    xC = .1; yC += .04; iMaxCharsPerLine += 10;
    if (bClk)  {
      xC = .3; yC = .25; yH = 150;
    }
  }
  if (bWS42) {
    yH = 73;    yL = 228;
    if (bClk) {
      yH = 140;    yL = 228;
    }
  }
  if (bWS29) {
    xC = 0; yC = .11;
    if (bClk) {
      //yH = iScreenYMax / 2;      yL = iScreenYMax;
      yH = iScreenYMax;      yL = iScreenYMax;
    } else {
      yH = 0;      yL = iScreenYMax - 12;
    }
  }
  if (tNow == 0) tNow = time(nullptr);
  iTemp = round(fGetTempTime(tNow));
  if ((-40 < iTemp) && (iTemp < 50))  fCurrTemp = iTemp;

  Serial.printf("\n DF: %f ", fCurrTemp);

  if (abs(fInsideTemp + fTempInOffset) > 50) bInsideTempSensor = false;
  //      fCurrTemp = -6;
  //      fInsideTemp=21;
  if (!bClk) {
    if (bWS42 || bWS58 || bWS75)  {
      if ((-40 < fCurrTemp) && (fCurrTemp < 50)) sAux2 = String(int(abs(round(fCurrTemp))));
      else sAux2 = "-";
      if (round(fCurrTemp) > 0) xC -= 8 / iScreenXMax;
      DisplayU8TextAlignBorder((xC + .09)*iScreenXMax , yC * iScreenYMax, sAux2 , fU8g2_XXL, 0, 0, (bRed ? GxEPD_RED : GxEPD_BLACK));
      if (round(fCurrTemp) < 0) DisplayU8TextAlignBorder((xC + .03 - ((float)(sAux2.length()) - 1) * 0.02)*iScreenXMax , (yC - .049)*iScreenYMax , "-", fU8g2_XL, -1, 0, (bRed ? GxEPD_RED : GxEPD_BLACK));
      sAux1 = String(char(176));
      if (bInsideTempSensor) {
        DisplayU8TextAlignBorder(iScreenXMax * (xC + .20 + ((float)(sAux2.length()) - 2) * 0.04), iScreenYMax * (.074 - bWS75 * .017), sAux1, fU8g2_L, 1, 0, (bRed) ? GxEPD_RED : GxEPD_BLACK);
        sAux1 = "C";
        DisplayU8TextAlignBorder(iScreenXMax * (xC + .225 + ((float)(sAux2.length()) - 2) * 0.04), iScreenYMax * (.064 - bWS75 * .017), sAux1, fU8g2_L, 1, 0, (bRed) ? GxEPD_RED : GxEPD_BLACK);
        sAux1 = String(int(abs(round(fInsideTemp + fTempInOffset))));
        DisplayU8TextAlignBorder(iScreenXMax * (xC + .245 + ((float)(sAux2.length()) - 2) * 0.04), iScreenYMax * (.179 - bWS75 * .01), sAux1, fU8g2_XL, 0, 0, GxEPD_BLACK);
      } else {
        DisplayU8TextAlignBorder(iScreenXMax * (xC + .20 + ((float)(sAux2.length()) - 2) * 0.04), iScreenYMax * (.104 - bWS75 * .017), sAux1, fU8g2_L, 1, 0, (bRed) ? GxEPD_RED : GxEPD_BLACK);
        sAux1 = "C";
        DisplayU8TextAlignBorder(iScreenXMax * (xC + .225 + ((float)(sAux2.length()) - 2) * 0.04), iScreenYMax * (.094 - bWS75 * .017), sAux1, fU8g2_L, 1, 0, (bRed) ? GxEPD_RED : GxEPD_BLACK);
      }
      DisplayWXicon(iScreenXMax * (xC + .314 + ((float)(sAux2.length()) - 2) * 0.01), iScreenYMax * (.026 - bWS42 * 0.02), sMeanWeatherIcon(0, 1));
      drawArrowBorder(iScreenXMax * (xC + .314 + ((float)(sAux2.length()) - 2) * 0.01) + iArrOx, iScreenYMax * (.026 - bWS42 * 0.02) + iArrOy, iArrOs, (int)(round(dGetWindSpdTime(tNow))), round(dGetWindBrnTime(tNow)));
      DisplayU8TextAlignBorder(iScreenXMax * (.71 - bWS75 * .01), iScreenYMax / 10 + 1, sDateWeekDayName(sWeatherLNG), fU8g2_XL, -1, 0, bRed ? GxEPD_RED : GxEPD_BLACK);
      DisplayU8TextAlignBorder(iScreenXMax * (.775 - bWS75 * .01), iScreenYMax / 10 + 1, (String)(day(tNow)), fU8g2_XL, 0, 0); //, bRed?GxEPD_RED:GxEPD_BLACK);
      DisplayU8TextAlignBorder(iScreenXMax * (.84 - bWS75 * .01), iScreenYMax / 10 + 1, sDateMonthName(sWeatherLNG), fU8g2_XL, 1, 0, bRed ? GxEPD_RED : GxEPD_BLACK);
      if (sSummaryDay.length() > (iMaxCharsPerLine)) {
        DisplayU8Text(iScreenXMax * .55, iScreenYMax * (.149 - bWS75 * .019), sSummaryDay.substring(0, iMaxCharsPerLine), fU8g2_XS);
        DisplayU8Text(iScreenXMax * .55, iScreenYMax * (.185 - bWS75 * .020), sSummaryDay.substring(iMaxCharsPerLine, sSummaryDay.length()), fU8g2_XS);
      } else {
        DisplayU8Text(iScreenXMax * .55, iScreenYMax * .155, sSummaryDay, fU8g2_XS);
      }
      DisplayU8Text(1 + bWS75 * 3, yH - 5 + bWS75 * 2, sSummaryWeek, fU8g2_XS);
    }
  } else { //bClk
    if (bWS42 || bWS58 || bWS75) {
      DisplayU8TextAlignBorder(iScreenXMax * xC, yC * iScreenYMax, sTimeLocal(tNow), fU8g2_XXL, 0, 0, GxEPD_BLACK);
      DisplayU8TextAlignBorder(iScreenXMax * .875, .09 * iScreenYMax, sDateWeekDayName(sWeatherLNG), fU8g2_L, 0, 0, GxEPD_BLACK);
      DisplayU8TextAlignBorder(iScreenXMax * .875, .22 * iScreenYMax, sDateMonthDay(), fU8g2_XL, 0, 0, bRed ? GxEPD_RED : GxEPD_BLACK);
      DisplayU8TextAlignBorder(iScreenXMax * .875, .30 * iScreenYMax, sDateMonthName(sWeatherLNG), fU8g2_L, 0, 0, GxEPD_BLACK);
      DisplayWXicon(iScreenXMax * .72, yH * .1, sMeanWeatherIcon(0, 1));
      sAux1 = float2string(round(fCurrTemp), 0);
      DisplayU8Text(iScreenXMax * .59, yH * .65 , sAux1 + char(176), fU8g2_XL, bRed ? GxEPD_RED : GxEPD_BLACK);
    } else { //bClk+bWS29
      DisplayU8Text(5, yH - 46, sTimeLocal(tNow + 30), fU8g2_XXL);
      DisplayU8TextAlignBorder(iScreenXMax * .91, 22, sDateWeekDayName(sWeatherLNG), fU8g2_L, 0, 0, GxEPD_BLACK);
      DisplayU8TextAlignBorder(iScreenXMax * .91, 57,  (String)(day(tNow)), fU8g2_XL, 0, 0, GxEPD_BLACK);
      DisplayU8TextAlignBorder(iScreenXMax * .91, 80, sDateMonthName(sWeatherLNG), fU8g2_L, 0, 0, GxEPD_BLACK);
      DisplayWXicon(iScreenXMax * .84, yH - 46, sMeanWeatherIcon(0, 1));
      if ((-40 < fCurrTemp) && (fCurrTemp < 50)) sAux1 = String(int(abs(round(fCurrTemp))))+ char(176);
      else sAux1 = "-";     
      DisplayWXicon(iScreenXMax * .76 - 70, yH - 46, "ood");
      DisplayU8TextAlignBorder(iScreenXMax * .76, yH - 5, sAux1, fU8g2_XL, 0, 0, GxEPD_BLACK);
      if (bInsideTempSensor) {
        sAux1  = String(int(round(fInsideTemp + fTempInOffset))) + char(176);
        DisplayWXicon(-5, yH - 46, "hhd");
        DisplayU8TextAlignBorder(iScreenXMax * .22, yH - 5, sAux1, fU8g2_XL, 0, 0, GxEPD_BLACK);
      }
    }
  }
  if (yH != yL)  DisplayForecastGraph(0, yH, iScreenXMax, yL - yH, ANALYZEHOURS, iOffsetH, fU8g2_S, fU8g2_M);
  if (!bWS29) DisplayU8Text(iScreenXMax / 128, yL + 22 - bWS42 * 3, sTimeLocal(tNow), fU8g2_M, bRed ? GxEPD_RED : GxEPD_BLACK);
  else if (!bClk) DisplayU8Text(iScreenXMax / 128, yL + 12, sTimeLocal(tNow), fU8g2_S, bRed ? GxEPD_RED : GxEPD_BLACK);
  //HOURS & ICONS
  float fWindAccuX , fWindAccuY , fMeanBrn, fMeanSpd, fDirDeg;
  int iXIcon , iYIcon;
  for ( i = 0; i < 8; i++) {
    if (i > 0) {
      if (!bWS29) DisplayU8TextAlignBorder( t2x(tNow + (iOffsetH + (6 * i)) * 3600, iScreenXMax), yL + 22 - bWS42 * 3, String((hour(tNow) + iOffsetH - 1 + (6 * (i))) % 24) + "h",  fU8g2_M, 0, 0);
      else if (!bClk) DisplayU8Text(t2x(tNow + (iOffsetH + (6 * i)) * 3600, iScreenXMax), yL + 12, String((hour(tNow) + iOffsetH - 2 + (6 * (i))) % 24) + "h",  fU8g2_S);
    }
    if (!bWS29) {
      iXIcon  = iScreenXMax / 64 - bWS42 * 5 + bWS75 * 4 + iScreenXMax / 8 * i;
      iYIcon = yL + 26 - 24 * bWS29 - bWS42 * 3;
      fWindAccuX = 0;
      fWindAccuY = 0;
      DisplayWXicon(iXIcon, iYIcon, sMeanWeatherIcon(i * ANALYZEHOURS / 8, (i + 1)* ANALYZEHOURS / 8 - 1)); //, bRed?GxEPD_RED:GxEPD_BLACK);
      if (i > 3)        drawBar(iXIcon, iYIcon , iXIcon + 60, iYIcon + 60, 2 , GxEPD_WHITE);
      //Wind
      for (j = i * ANALYZEHOURS / 8; j < ((i + 1)* ANALYZEHOURS / 8) ; j++) {
        fWindAccuX = fWindAccuX + (aWindSpd[j] * cos(aWindBrn[j] * 1000 / 57296));
        fWindAccuY = fWindAccuY + (aWindSpd[j] * sin(aWindBrn[j] * 1000 / 57296));
      }
      fMeanSpd = sqrt(pow(fWindAccuX * 8 / ANALYZEHOURS , 2) + pow(fWindAccuY * 8 / ANALYZEHOURS , 2));
      fMeanBrn = atan2 (fWindAccuY , fWindAccuX ) * 57296 / 1000;
      if (fMeanBrn >= 360) fMeanBrn -= 360;
      if (fMeanBrn < 0) fMeanBrn += 360;
      drawArrowBorder(iXIcon + iArrOx, iYIcon + iArrOy, iArrOs, (int)(round(fMeanSpd)), round(fMeanBrn));
    }
  }
  Serial.print("  Forecast Display redone...");
}//////////////////////////////////////////////////////////////////////////////
#define PRECIP_LVL_2 4
void DisplayForecastGraph(int x, int y, int wx, int wy, int iAnalyzePeriod, int iOffsetH, const uint8_t *fontS, const uint8_t *fontL) {
  int iTmp1, iTmp2, iNightIni, i, iWDay, xHourA, xHourB, xPrecF, iOffsetX;
  long int tSA, tSB;
  float dTempMax = -100, dTempMin = 100, dPrecipMax = 0;
  bool bIsNight = false, bPrecipText = false;
  i = 12 - hour(tNow);
  iOffsetX = ((iOffsetH + 2) * wy) / iAnalyzePeriod ;
  DisplayU8TextAlignBorder(iOffsetX + ((i *  wx) / iAnalyzePeriod), y + wy / 2 - 5, sWeekDayNames(sWeatherLNG, (iWeekdayToday()) % 7) ,  bWS29 ? fU8g2_L : fU8g2_XL, 0, 0, bRed ? GxEPD_RED : GxEPD_BLACK);
  i += 24;
  DisplayU8TextAlignBorder(iOffsetX + ((i *  wx) / iAnalyzePeriod), y + wy / 2 - 5, sWeekDayNames(sWeatherLNG, (iWeekdayToday() + 1) % 7) ,  bWS29 ? fU8g2_L : fU8g2_XL, 0, 0, bRed ? GxEPD_RED : GxEPD_BLACK);
  i += 24;
  DisplayU8TextAlignBorder(iOffsetX + ((i *  wx) / iAnalyzePeriod), y + wy / 2 - 5, sWeekDayNames(sWeatherLNG, (iWeekdayToday() + 2) % 7) ,  bWS29 ? fU8g2_L : fU8g2_XL, 0, 0, bRed ? GxEPD_RED : GxEPD_BLACK);
  drawLine(x, y - 1, x + wx, y - 1, 1, 2);
  drawLine(x + (0.07 * wx), y - 1 + wy / 4, x + (0.93 * wx), y - 1 + wy / 4, 1, 3);
  drawLine(x + (0.07 * wx), y - 1 + wy / 2, x + (0.93 * wx), y - 1 + wy / 2, 1, 2);
  drawLine(x + (0.07 * wx), y - 1 + wy * 3 / 4, x + (0.93 * wx), y - 1 + wy * 3 / 4, 1, 3);
  drawLine(x, y + wy + 1, x + wx, y + wy + 1, 1, 1);
  tSA = tSunset - 86400; //the one from yesterday
  tSB = tSunrise ;
  // Serial.printf("\nHOURS: A%d->B%d,Now:%d\n", tSA, tSB, tNow);
  do {
    xHourA = x + iOffsetX + t2x(tSA, wx);
    xHourB = x + iOffsetX + t2x(tSB, wx);
    if ((xHourB > (x)) && (xHourA < (x + wx))) {
      if (xHourA < (x )) xHourA = x ;
      if (xHourB > (x + wx)) xHourB = x + wx;
      drawBar( xHourA, y + 1 , xHourB , y + wy - 1, 6);
      if (xHourA > 0) drawLine(xHourA, y , xHourA, y + wy , 1, 2);
      if (xHourB < (x + wx)) drawLine(xHourB , y , xHourB, y + wy , 1, 2);
    }
    tSA += 86400;
    tSB += 86400;
  } while (tSA < (tNow + (iAnalyzePeriod * 3600)));

  for ( i = 0; i < iAnalyzePeriod; i++) {
    if ( aTempH[i] > dTempMax) dTempMax = aTempH[i];
    if ( aTempH[i] < dTempMin) dTempMin = aTempH[i];
    if ( aPrecip[i] > dPrecipMax) dPrecipMax = aPrecip[i];
  }
  dPrecipMax = dPrecipMax * 1.02;
  dTempMax = dTempMax + abs(dTempMax) * 0.02;
  dTempMin = dTempMin - abs(dTempMin) * 0.02;
  //  Serial.printf("\n Precip ini %f (%f)", dPrecipMax, (0.0778 * dPrecipMax * dPrecipMax * dPrecipMax) - (1.131 * dPrecipMax * dPrecipMax) + (5.5698 * dPrecipMax) - 4.45);
  // y = -0.0102x^3-0.0289x^2+.9987x+1.9841
  if (dPrecipMax > PRECIP_LVL_2)  dPrecipMax = round(dPrecipMax + 0.5); //Show the values progressively
  else  dPrecipMax = round((-0.0102 * dPrecipMax * dPrecipMax * dPrecipMax) - (0.0289 * dPrecipMax * dPrecipMax) + (0.9987 * dPrecipMax) + 1.9841);
  if (dPrecipMax < 1) dPrecipMax = 1;
  iOffsetX = ((iOffsetH + 5) * wy) / iAnalyzePeriod ;

  for ( i = 0; i < (iAnalyzePeriod - 1); i++) {
    xHourA = x + iOffsetX + t2x(aHour[i], wx );
    xHourB = x + iOffsetX + t2x(aHour[i + 1], wx );
    if ((xHourA <= (x + wx)) && (xHourB >= x)) {
      if (xHourA < x) xHourA = x;
      if (xHourB > (x + wx)) xHourB = x + wx;
      xPrecF = (xHourB - xHourA) * (1 - ((aPrecipProb[i] < 0.5) ? (aPrecipProb[i] * 1.5) : (aPrecipProb[i] * 0.5 + 0.5)));
      //Clouds
      drawLine(xHourA, y + wy - wy * aCloudCover[i], xHourB, y + wy - wy * aCloudCover[i + 1], 3, 3);
      //Temp
      drawLine(xHourA, y + wy - wy * (aTempH[i] - dTempMin) / (dTempMax - dTempMin), xHourB, y + wy - wy * (aTempH[i + 1] - dTempMin) / (dTempMax - dTempMin), 4, 1);
      // Rain
      drawLine(xHourA, y + wy - wy * aPrecip[i] / dPrecipMax, xHourB, y + wy - wy * aPrecip[i + 1] / dPrecipMax, 2, 1, bRed ? GxEPD_RED : GxEPD_BLACK);
      // Serial.printf("\nForescast%d Graph;%f/%f, %d",i, aPrecip[i],dPrecipMax,(int)(y + wy - wy * aPrecip[i] / dPrecipMax));
      drawBar (xHourA + xPrecF - ((xHourB - xHourA) / 2), (int)(y + wy - wy * aPrecip[i] / dPrecipMax), xHourB - xPrecF - ((xHourB - xHourA) / 2), y + wy, 2, bRed ? GxEPD_RED : GxEPD_BLACK);
    }
  }
  if (bPrecipText)  {
    DisplayU8TextAlignBorder(x + wx - 16, y + 25 - bWS29 * 4 + ((dPrecipMax >= PRECIP_LVL_2) ? 12 : 0), float2string(dPrecipMax, 0), ((dPrecipMax >= PRECIP_LVL_2) ? fU8g2_XL : fU8g2_L), -1, 2, bRed ? GxEPD_RED : GxEPD_BLACK);
    DisplayU8TextAlignBorder(x + wx - 14, y + 15 - bWS29 * 4, "mm",  fU8g2_XS, 1, 2, bRed ? GxEPD_RED : GxEPD_BLACK);
  }
  DisplayU8TextAlignBorder(x, y + 15, float2string(dTempMax, 0) + char(176),  fontL, 1, 2);
  DisplayU8TextAlignBorder(x, y + 4 + wy / 2, float2string(dTempMax - (dTempMax - dTempMin) / 2, 0) + char(176), fontS, 1, 2);
  DisplayU8TextAlignBorder(x, y + wy - 2, float2string(dTempMin, 0) + char(176), fontL, 1, 2);
  if (!bClk) {
    DisplayU8TextAlignBorder(x, y + 4 + wy / 4, float2string(dTempMax - (dTempMax - dTempMin) / 4, 0) + char(176), fU8g2_XS, 1, 2);
    DisplayU8TextAlignBorder(x, y + 4 + wy * 3 / 4, float2string(dTempMax - (dTempMax - dTempMin) * 3 / 4, 0) + char(176),  fU8g2_XS, 1, 2);
  }
  if (sJsonDev.length() > 4)     {
    DynamicJsonBuffer jsonBuffer(256);
    JsonObject& root = jsonBuffer.parseObject(sJsonDev);
    if (root.success()) {
      String sAux = root["vars"]["CustomText"];
      if (sAux.length() > 0)  sCustomText = sAux;
    }
  }
  if (sCustomText.length() > 0)  DisplayU8TextAlignBorder(x + wx - 3, y + wy - 2, sCustomText,  fontL, -1, 2, bRed ? GxEPD_RED : GxEPD_BLACK);
}////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void DisplayU8Text(int x, int y, String text, const uint8_t *font, uint16_t color  ) {
  u8g2Fonts.setFont(font);
  u8g2Fonts.setForegroundColor(color);
  u8g2Fonts.setCursor(x, y);
  u8g2Fonts.println(text);
}//////////////////////////////////////////////////////////////////////////////
void DisplayU8TextAlignBorder(int x, int y, String text, const uint8_t *font, int iAlignMode, int iBorderSize, uint16_t color ) {
  u8g2Fonts.setFont(font);
  int iTxtWdth = u8g2Fonts.getUTF8Width(text.c_str());
  int xf = x - iTxtWdth * (0.5 - (float)iAlignMode / 2);
  u8g2Fonts.setForegroundColor(GxEPD_WHITE);
  if (iBorderSize > 0) {
    u8g2Fonts.setCursor(xf - iBorderSize, y - iBorderSize);
    u8g2Fonts.println(text);
    u8g2Fonts.setCursor(xf + iBorderSize, y - iBorderSize);
    u8g2Fonts.println(text);
    u8g2Fonts.setCursor(xf - iBorderSize, y + iBorderSize);
    u8g2Fonts.println(text);
    u8g2Fonts.setCursor(xf + iBorderSize, y + iBorderSize);
    u8g2Fonts.println(text);
  }
  u8g2Fonts.setForegroundColor(color);
  u8g2Fonts.setCursor(xf, y);
  u8g2Fonts.println(text);
}//////////////////////////////////////////////////////////////////////////////
void DisplayU8TextAlign2Border(int x, int y, String text, const uint8_t *font, int iAlignMode, int iBorderSize, uint16_t colorBorder, uint16_t colorIn ) {
  u8g2Fonts.setFont(font);
  int iTxtWdth = u8g2Fonts.getUTF8Width(text.c_str());
  int xf = x - iTxtWdth * (0.5 - (float)iAlignMode / 2);
  if (iBorderSize > 0) {
    u8g2Fonts.setForegroundColor(GxEPD_WHITE);
    u8g2Fonts.setCursor(xf - 2 * iBorderSize, y - 2 * iBorderSize);
    u8g2Fonts.println(text);
    u8g2Fonts.setCursor(xf + 2 * iBorderSize, y - 2 * iBorderSize);
    u8g2Fonts.println(text);
    u8g2Fonts.setCursor(xf - 2 * iBorderSize, y + 2 * iBorderSize);
    u8g2Fonts.println(text);
    u8g2Fonts.setCursor(xf + 2 * iBorderSize, y + 2 * iBorderSize);
    u8g2Fonts.println(text);
    u8g2Fonts.setForegroundColor(colorBorder);
    u8g2Fonts.setCursor(xf - iBorderSize, y - iBorderSize);
    u8g2Fonts.println(text);
    u8g2Fonts.setCursor(xf + iBorderSize, y - iBorderSize);
    u8g2Fonts.println(text);
    u8g2Fonts.setCursor(xf - iBorderSize, y + iBorderSize);
    u8g2Fonts.println(text);
    u8g2Fonts.setCursor(xf + iBorderSize, y + iBorderSize);
    u8g2Fonts.println(text);
  }
  u8g2Fonts.setForegroundColor(colorIn);
  u8g2Fonts.setCursor(xf, y);
  u8g2Fonts.println(text);
}//////////////////////////////////////////////////////////////////////////////
void DisplayWXicon(int x, int y, String IconName, uint16_t color) {
  if      (IconName == "01d" || IconName == "clear - day") display.drawBitmap(x, y, gImage_01d, 48, 48, color);
  else if (IconName == "01n" || IconName == "clear - night") display.drawBitmap(x, y, gImage_01n, 48, 48, color);
  else if (IconName == "02d" || IconName == "partly - cloudy - day") display.drawBitmap(x, y, gImage_02d, 48, 48, color);
  else if (IconName == "02n" || IconName == "partly - cloudy - night") display.drawBitmap(x, y, gImage_02n, 48, 48, color);
  else if (IconName == "03d" || IconName == "cloudy - day") display.drawBitmap(x, y, gImage_03d, 48, 48, color);
  else if (IconName == "03n" || IconName == "cloudy - night") display.drawBitmap(x, y, gImage_03n, 48, 48, color);
  else if (IconName == "04d") display.drawBitmap(x, y, gImage_04d, 48, 48, color); // dark cloud
  else if (IconName == "04n") display.drawBitmap(x, y, gImage_04n, 48, 48, color);
  else if (IconName == "09d" || IconName == "rain - day") display.drawBitmap(x, y, gImage_09d, 48, 48, color);
  else if (IconName == "09n" || IconName == "rain - night") display.drawBitmap(x, y, gImage_09n, 48, 48, color);
  else if (IconName == "10d") display.drawBitmap(x, y, gImage_10d, 48, 48, color); //rain and sun
  else if (IconName == "10n") display.drawBitmap(x, y, gImage_10n, 48, 48, color);
  else if (IconName == "11d" || IconName == "storm - day") display.drawBitmap(x, y, gImage_11d, 48, 48, color);
  else if (IconName == "11n" || IconName == "storm - night") display.drawBitmap(x, y, gImage_11n, 48, 48, color);
  else if (IconName == "13d" || IconName == "snow - day") display.drawBitmap(x, y, gImage_13d, 48, 48, color);
  else if (IconName == "13n" || IconName == "snow - night") display.drawBitmap(x, y, gImage_13n, 48, 48, color);
  else if (IconName == "50d" || IconName == "50n"  || IconName == "fog") display.drawBitmap(x, y, gImage_50n, 48, 48, color);
  else if (IconName == "hhd") display.drawBitmap(x, y, gImage_hhd, 48, 48, color);
  else if (IconName == "ood") display.drawBitmap(x, y, gImage_ood, 48, 48, color);
  else     display.drawBitmap(x, y, gImage_nodata, 48, 48, color);
}//////////////////////////////////////////////////////////////////////////////
#define MINRAINICON 0.015
String sMeanWeatherIcon(int iS, int iF) {
  int i;
  float dNight = 0, dMeanRain = 0, dMeanCloud = 0, dMeanTemp = 0;
  String str;
  bool bNight = false, bFog = false, bStorm = false;
  for (i = iS; i < iF; i++) {
    dNight += ((hour(aHour[i]) < hour(tSunrise)) || (hour(aHour[i]) > hour(tSunset)));
    dMeanRain += (aPrecip[i] * aPrecipProb[i] ) / (iF - iS);
    dMeanCloud += aCloudCover[i] / (iF - iS);
    dMeanTemp += aTempH[i] / (iF - iS);
    if (aIcon[i] == "storm") bStorm = true;
    if (aIcon[i] == "fog") bFog = true;
  }
  if ((dNight / (iF - iS)) >= 0.5)     bNight = true;
  if (dMeanRain > MINRAINICON) {
    // Raining
    if (dMeanCloud < 0.5) {
      if (bNight) str = "10n";
      else str = "10d";
    } else {
      if (bNight) str = "09n";
      else str = "09d";
    }
    if (dMeanTemp < 4) {
      //snow
      if (bNight) str = "13n";
      else str = "13d";
    }
  } else {
    if (dMeanCloud < 0.3) {
      if (bNight) str = "01n";
      else str = "01d";
    } else if (dMeanCloud < 0.5) {
      if (bNight) str = "02n";
      else str = "02d";
    } else if (dMeanCloud < 0.8) {
      if (bNight) str = "03n";
      else str = "03d";
    } else {
      if (bNight) str = "04n";
      else str = "04d";
    }
  }
  if (bStorm) {
    if (bNight) str = "11n";
    else str = "11d";
  }
  if (bFog) {
    if (bNight) str = "50n";
    else str = "50d";
  }
  return str;
}//////////////////////////////////////////////////////////////////////////////
void drawLine(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t w, uint16_t trama, uint16_t color ) {
  uint16_t fcolor;
  int tmp, i, j;
  if (x1 < 0) x1 = 0; if (x2 < 0) x2 = 0; if (y1 < 0) y1 = 0; if (y2 < 0) y2 = 0;
  if (x1 > iScreenXMax) x1 = iScreenXMax; if (x2 > iScreenXMax) x2 = iScreenXMax; if (y1 > iScreenYMax) y1 = iScreenYMax; if (y2 > iScreenYMax) y2 = iScreenYMax;
  if (abs(x2 - x1) > abs(y2 - y1)) {
    if (x1 > x2) {
      tmp = x1; x1 = x2; x2 = tmp;
      tmp = y1; y1 = y2; y2 = tmp;
    }
    for (i = x1; i < x2 ; i++) {
      tmp = y1 + (i - x1) * (y2 - y1) / (x2 - x1);
      for (j = (int)(-w / 2); j < ((int)(-w / 2) + w); j++) {
        if (((tmp + i + j) % trama) == 0) {
          fcolor = color;
        } else {
          fcolor = GxEPD_WHITE;
        }
        display.drawPixel(i, tmp + j, fcolor);
      }
    }
  } else {
    if (y1 > y2) {
      tmp = y1; y1 = y2; y2 = tmp;
      tmp = x1; x1 = x2; x2 = tmp;
    }
    for (i = y1; i < y2 ; i++) {
      tmp = x1 + (i - y1) * (x2 - x1) / (y2 - y1);
      for (j = (int)(-w / 2); j < ((int)(-w / 2) + w); j++) {
        if (((tmp + i + j) % trama) == 0) {
          fcolor = color;
        } else {
          fcolor = GxEPD_WHITE;
        }
        display.drawPixel(tmp + j, i , fcolor);
      }
    }
  }
}//////////////////////////////////////////////////////////////////////////////
void drawArrowBorder(int x, int y, int iMaxSize, int iSpd, float fDir) {
  drawArrow(x + 3, y + 3, iMaxSize, iSpd, fDir, GxEPD_WHITE);
  drawArrow(x + 3, y - 3, iMaxSize, iSpd, fDir, GxEPD_WHITE);
  drawArrow(x - 3, y + 3, iMaxSize, iSpd, fDir, GxEPD_WHITE);
  drawArrow(x - 3, y - 3, iMaxSize, iSpd, fDir, GxEPD_WHITE);
  drawArrow(x, y + 3, iMaxSize, iSpd, fDir, GxEPD_WHITE);
  drawArrow(x, y - 3, iMaxSize, iSpd, fDir, GxEPD_WHITE);
  drawArrow(x + 3, y, iMaxSize, iSpd, fDir, GxEPD_WHITE);
  drawArrow(x - 3, y, iMaxSize, iSpd, fDir, GxEPD_WHITE);
  drawArrow(x, y, iMaxSize, iSpd, fDir, bRed ? GxEPD_RED : GxEPD_BLACK);
}//////////////////////////////////////////////////////////////////////////////
void drawArrow(int x, int y, int iMaxSize, int iSpd, float fDir, uint16_t iColor) {
  float flW = .8;
  if (5 > iSpd) return;
  fDir = (round(fDir / 45) * 45) +  180;
  if (fDir >= 360) fDir -= 360;
  float fRad1 = fDir * DEG_TO_RAD, fRad2 = (fDir + 135) * DEG_TO_RAD, fRad3 = (fDir - 135) * DEG_TO_RAD;;
  float fLength = (float)(iMaxSize) * (0.6 + ((float)(iSpd) / 60));
  float x1 = x + (sin(fRad1) * fLength * .5), y1 = y - (cos(fRad1) * fLength * .5);
  float x2 = x - (sin(fRad1) * fLength * .5), y2 = y + (cos(fRad1) * fLength * .5);
  float x3 = x1 + (sin(fRad2) * fLength * .5 * flW), y3 = y1 - (cos(fRad2) * fLength * .5 * flW);
  float x4 = x1 + (sin(fRad3) * fLength * .5 * flW), y4 = y1 - (cos(fRad3) * fLength * .5 * flW);
  int dX1 = (cos(fRad1) * fLength * .05), dY1 = (sin(fRad1) * fLength * .05);
  drawLine(x1 + dX1, y1 + dY1, x2 + dX1, y2 + dY1, 3, 1, iColor);
  drawLine(x1 - dX1, y1 - dY1, x2 - dX1, y2 - dY1, 3, 1, iColor);
  drawLine(x1 + dX1, y1 + dY1, x3 + dX1, y3 + dY1, 2, 1, iColor);
  drawLine(x1 - dX1, y1 - dY1, x4 - dX1, y4 - dY1, 2, 1, iColor);
}//////////////////////////////////////////////////////////////////////////////
void PaintBatt(int BattPerc) {
  if (BattPerc > 0)  {
    String sAux = sBattStatus[iBattStatus];
    if (bWS42 || bWS75) {
      display.fillRect(0, 0, 22 + bWS75 * 5, 60 + bWS75 * 8, GxEPD_WHITE);
      int iVChgEnd;
      //      Serial.println("\n"+(String)(iBattStatus));
      if ((iBattStatus == 3) && (iVtgPeriodDrop > 0) && (tNow > 0)) {
        float fRemDays = iRemDaysTime();
        if (fRemDays > 180) fRemDays = 180;
        sAux = (String)(int)(fRemDays) + "d";
        if (1 > fRemDays) sAux = "-";
      }
      DisplayU8TextAlignBorder(5 + bWS75 * 1 + 9, 10 + bWS75 * 2, sAux, fU8g2_XS, 0, 0);
      drawBatt(5 + bWS75 * 1, 12 + bWS75 * 2, 16, 32, BattPerc);
      int i, tWidth = (tVtgTime[VTGMEASSURES - 1] - tVtgTime[0]), tIni = tVtgTime[0], iVmin = 1000, iVmax = 0;
      float x1, y1, x2, y2;
      for (i = 0; i < VTGMEASSURES; i++) {
        if (iVtgVal[i] > iVmax) iVmax = iVtgVal[i];
        if (iVtgVal[i] < iVmin) iVmin = iVtgVal[i];
      }
      if ((iVmax - iVmin) < ((iVtgMax - iVtgMin)*.001)) iVmin = iVmax - (iVtgMax - iVtgMin) * .001;
      //Draw Vtg Graph
      x1 = 1 + bWS75 * 3 + ((24 * (tVtgTime[0] - tIni)) / tWidth);
      y1 = 56 + bWS75 * 5 - (((10 + bWS75 * 3) * (iVtgVal[0] - iVmin)) / (iVmax - iVmin));
      for (i = 1; i < VTGMEASSURES; i++) {
        x2 = 1 + bWS75 * 3 + ((24 * (tVtgTime[i] - tIni)) / tWidth);
        y2 = 56 + bWS75 * 5 - (((10 + bWS75 * 3) * (iVtgVal[i] - iVmin)) / (iVmax - iVmin));
        display.drawLine(x1, y1, x2, y2, GxEPD_BLACK);
        x1 = x2;
        y1 = y2;
      }
    } else {
      if (!bClk) drawBatt(iScreenXMax - 6, iScreenYMax - 10, 5, 9, BattPerc);
    }
    if (BattPerc < 1)  {
      DisplayU8Text(iScreenXMax * .25, iScreenYMax * .3, " LOW BATTERY " , fU8g2_L, bRed ? GxEPD_RED : GxEPD_BLACK);
      LogAlert("LOW BATTERY " + (String)(BattPerc) + "%", 1);
    }
    FB_SetBatt(iVtgVal[VTGMEASSURES - 1], sAux );
  }
}//////////////////////////////////////////////////////////////////////////////
void drawBatt(uint16_t x, uint16_t y, uint16_t sx, uint16_t sy, int BattPerc, uint16_t color) {
  int f = sx / 4;
  if (BattPerc > 0) {
    int yV = y + sy - (sy - f) * BattPerc / 100;
    drawBar(x , yV , x + sx , y + sy , 2);
    drawLine(x, y + f, x + f, y + f, 1, 1, color);
    drawLine(x + f, y + f, x + f, y, 1, 1, color);
    drawLine(x + f, y, x + sx - f, y, 1, 1, color);
    drawLine(x + sx - f, y, x + sx - f, y + f, 1, 1, color);
    drawLine(x + sx - f, y + f, x + sx, y + f, 1, 1, color);
    drawLine(x + sx, y + f, x + sx, y + sy, 1, 1, color);
    drawLine(x + sx, y + sy, x, y + sy, 1, 1, color);
    drawLine(x, y + sy, x, y + f, 1, 1, color);
  }
}//////////////////////////////////////////////////////////////////////////////
void drawBar(int x1, int y1, int x2, int y2, int trama, uint16_t color ) {
  uint16_t i, j, tmp;
  if (x1 < 0) x1 = 0; if (x2 < 0) x2 = 0; if (y1 < 0) y1 = 0; if (y2 < 0) y2 = 0;
  if (x1 > iScreenXMax) x1 = iScreenXMax; if (x2 > iScreenXMax) x2 = iScreenXMax; if (y1 > iScreenYMax) y1 = iScreenYMax; if (y2 > iScreenYMax) y2 = iScreenYMax;
  if (x1 > x2) {
    tmp = x1; x1 = x2; x2 = tmp;
  }
  if (y1 > y2) {
    tmp = y1; y1 = y2; y2 = tmp;
  }
  for (i = x1; i < x2 ; i++) {
    for (j = y1; j < y2 ; j++) {
      if (((i % trama) == 0) && ((j % trama) == 0)) display.drawPixel(i, j, color);
    }
  }
}////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int t2x(long int t, int xMax) {
  int x = (int)((t - tNow) * xMax / (ANALYZEHOURS * 3600));
  return x;
}////////////////////////////////////////////////////////////////////////////////
bool bGetWeatherForecast() {
  String jsonFioString = "";
  bool bFromSPIFFS = false, bFioFailed = false;
  if ((!bWeHaveWifi) || ((tNow - tLastSPIFFSWeather) < (iRefreshPeriod * 35))) jsonFioString = readSPIFFSFile("/weather.txt");
  if (jsonFioString.length()) {
    DynamicJsonBuffer jsonBuffer(1024);
    JsonObject& root = jsonBuffer.parseObject(jsonFioString);
    if (root.success()) {
      tLastSPIFFSWeather = root["currently"]["time"];
      tLastSPIFFSWeather = tLastSPIFFSWeather;
      Serial.print(" JW_SPIFFS Ok ");
      if ((!bWeHaveWifi) || ((tNow - tLastSPIFFSWeather) < (iRefreshPeriod * 45))) Serial.print(" SPIFFS");
      else jsonFioString = "";
    } else {
      jsonFioString = "";
      Serial.print(" JW_SPIFFS NOk ");
    }
  }
  if (bWeHaveWifi) {
    if (!jsonFioString.length()) {
      FB_GetWeatherJson(&jsonFioString);
      if (jsonFioString.length()) {
        Serial.print(" JW_FIREBASE");
        DynamicJsonBuffer jsonBuffer(1024);
        JsonObject& root = jsonBuffer.parseObject(jsonFioString);
        if (!root.success()) {
          jsonFioString = "";
          Serial.print(" NOT OK!");
        } else Serial.print(" OK");
      }
    } else bFromSPIFFS = true;

    if (!jsonFioString.length()) {
      String sWeatherJSONSource = sWeatherURL + sWeatherAPI + "/" + sWeatherLOC + "?units=si&lang=" + sWeatherLNG;
      if (!bGetJSONString(sWeatherFIO, sWeatherJSONSource, &jsonFioString)) return false;
      if (jsonFioString.length() > 0) {
        FB_SetWeatherJson(jsonFioString);
        Serial.print(" DARKSKY");
      }
    }
  }
  Serial.print(" WJSON Loaded " +  (String)(jsonFioString.length()) + " chrs,");
  if (!jsonFioString.length()) {
    LogAlert("JSON EMPTY", 1);
    return false;
  }  else   {
    if (!showWeather_conditionsFIO(jsonFioString ))         {
      LogAlert("Failed to get Conditions Data", 1);
      bFioFailed = true;
    }
  }
  if ((!bFromSPIFFS) && (!bFioFailed)) writeSPIFFSFile("/weather.txt", jsonFioString.c_str());
  jsonFioString = "";
  return (!bFioFailed);
}////////////////////////////////////////////////////////////////////////////////
bool showWeather_conditionsFIO(String jsonFioString ) {
  String sAux;
  time_t tLocal;
  int tzOffset;
  bool bSummarized = (jsonFioString.length() < 9999);
  Serial.print("  Creating object," );
  DynamicJsonBuffer jsonBuffer(1024);
  Serial.print("Parsing,");
  JsonObject& root = jsonBuffer.parseObject(jsonFioString);
  Serial.print("done.");
  if (!root.success()) {
    LogAlert(F("jsonBuffer.parseObject() failed"), 1);
    return false;
  }
  Serial.print("->Vars," );
  fCurrTemp = root["currently"]["temperature"];
  tzOffset = root["offset"];
  tzOffset *= 3600;
  if (bSummarized) {
    tSunrise = root["daily"]["sunriseTime"];
    tSunset = root["daily"]["sunsetTime"];
  } else {
    tSunrise = root["daily"]["data"][0]["sunriseTime"];
    tSunset = root["daily"]["data"][0]["sunsetTime"];
  }
//  tSunrise += tzOffset;
//  tSunset += tzOffset;
  String stmp1 = root["hourly"]["summary"];
  sSummaryDay = stmp1;//sUtf8ascii(stmp1);
  String stmp2 = root["daily"]["summary"];
  sSummaryWeek = stmp2;//sUtf8ascii(stmp2);
  tLastSPIFFSWeather = root["currently"]["time"];
//  tLastSPIFFSWeather = tLastSPIFFSWeather + tzOffset;
  for (int i = 0; i < ANALYZEHOURS; i++) {
    if (bSummarized) {
      tLocal =          root["hourly"]["time" + (String)(i)];
      aTempH[i] =       root["hourly"]["temp" + (String)(i)];
      aHumid[i] =       root["hourly"]["humi" + (String)(i)];
      aPrecip[i] =      root["hourly"]["preI" + (String)(i)];
      aPrecipProb[i] =  root["hourly"]["preP" + (String)(i)];
      aWindSpd[i] =     root["hourly"]["winS" + (String)(i)];
      aWindBrn[i] =     root["hourly"]["winB" + (String)(i)];
      aCloudCover[i] =  root["hourly"]["cloC" + (String)(i)];
      String sTemp =    root["hourly"]["icon" + (String)(i)];
      if (sTemp.length() < 8) sTemp = sTemp + sAux;
      aIcon[i] = sTemp;
    } else {
      tLocal = root["hourly"]["data"][i]["time"];
      aTempH[i] = root["hourly"]["data"][i]["temperature"];
      aHumid[i] = root["hourly"]["data"][i]["humidity"];
      aPrecip[i] = root["hourly"]["data"][i]["precipIntensity"];
      aPrecipProb[i] = root["hourly"]["data"][i]["precipProbability"];
      aWindSpd[i] = root["hourly"]["data"][i]["windSpeed"];
      aWindBrn[i] = root["hourly"]["data"][i]["windBearing"];
      aCloudCover[i] = root["hourly"]["data"][i]["cloudCover"];
      String sTemp = root["hourly"]["data"][i]["icon"];
      if (sTemp.length() < 8) sTemp = sTemp + sAux;
      aIcon[i] = sTemp;
    }
    aHour[i] = tLocal;// + tzOffset;
    if ((hour(aHour[i]) < hour(tSunrise)) || (hour(aHour[i]) > hour(tSunset))) {
      sAux = "-night";
    } else {
      sAux = "-day";
    }
  }
  tNow = time(nullptr);
  if (!tFirstBoot) tFirstBoot = tNow;
  Serial.println("Done." );
  return true;
}//////////////////////////////////////////////////////////////////////////////
bool StartWiFi(int iRetries) {
  if (!iSPIFFSWifiSSIDs) {
    bWeHaveWifi = false;
    StartWifiAP(true);
    setWSPlatform(sPlatform());
    startWebServer();
    WiFi.disconnect();
    SendToRestart();
    return false;
  }
  int i = 0, j = 0;
  if (WiFi.status() == WL_CONNECTED)  WiFi.disconnect();
  Serial.print("  Connecting to WIFI");
  int iLastWifiNum = -1;
  int  iMaxSSIDs = WiFi.scanNetworks();
  if (!iMaxSSIDs) {
    bWeHaveWifi = false;
    LogAlert("No SSID available", 2);
    SendToSleep(5);
    return false;
  }
  for (i = 0; i < iSPIFFSWifiSSIDs; i++) {
    for (j = 0; j < iMaxSSIDs; j++) {
      if (getAWifiSSID(i) == WiFi.SSID(j)) {
        Serial.print("," + getAWifiSSID(i));
        iLastWifiNum = i;
        break;
      }
    }
    if (iLastWifiNum > -1) break;
  }
  if (iLastWifiNum < 0) {
    String sAux = "";
    for (i = 0; i < iSPIFFSWifiSSIDs; i++) sAux = sAux + "," + getAWifiSSID(i);
    LogAlert("No SSID found within" + sAux, 2);
    bWeHaveWifi = false;
    return false;
  }
  WiFi.begin(getAWifiSSID(iLastWifiNum).c_str(), getAWifiPSWD(iLastWifiNum).c_str());
  while (WiFi.status() != WL_CONNECTED ) {
    delay(500); Serial.print(".");
    if (j > iRetries) {
      Serial.print(" FAILED!");
      WiFi.disconnect();
      bWeHaveWifi = false;
      return false;
      break;
    }
    j++;
  }
  bWeHaveWifi = true;
  wifi_config_t conf;
  esp_wifi_get_config(WIFI_IF_STA, &conf);
  sWifiSsid = String(reinterpret_cast<const char*>(conf.sta.ssid));
  sWifiPassword = String(reinterpret_cast<const char*>(conf.sta.password));
  Serial.print(" (pwd:" + String(sWifiPassword.length()) + " chrs long)");
  Serial.print("-> @");
  Serial.print(WiFi.localIP());
  NtpConnect();
  return true;
}//////////////////////////////////////////////////////////////////////////////
void NtpConnect() {
  struct tm tmLocal;
  int i = 0;
  configTime( 3600, 0, "pool.ntp.org", "time.nist.gov"); //CET
  Serial.print("  NTP ");
  while (!getLocalTime(&tmLocal) && (i < 10)) {
    i++;
    Serial.print(".");
    delay(500);
  }
  if (i == 10) {
    LogAlert("NO NTP", 2);
    SendToSleep(5);
    return;
  }
  if (bIsDst()) {
    configTime( 3600, 3600, "pool.ntp.org", "time.nist.gov"); //CET
    Serial.print("  DST ");
    while (!getLocalTime(&tmLocal) && (i < 10)) {
      i++;
      Serial.print(".");
      delay(500);
    }
  }
  tNow = time(nullptr) ;
  Serial.println("Ok.");
}//////////////////////////////////////////////////////////////////////////////
void SendToSleep(int mins) {
  display.powerDown();
  Serial.print("\n[-> To sleep... " + (String)mins + " mins");
  lSecsOn += millis() / 1000;
  long lSecsCycle = ((lBoots) ? (0.8 * lSecsOn / lBoots) : 30);
  if (lBoots > 0) Serial.print(". @" + (String)(millis() / 1000) + " secs/" + (String)(lSecsCycle) + " mean ");
  uint64_t sleep_time_us = (uint64_t)(mins) * 60000000ULL;
  if (mins > 20) sleep_time_us -= (uint64_t)(lSecsCycle) * 1000000ULL; //give time for preprocessing as mean cycle is 1 min
  if (!mins) sleep_time_us = 2000000ULL;
  String sAux = "";
  if (abs(fInsideTemp + fTempInOffset) < 50)  sAux = String(fInsideTemp + fTempInOffset) + "ºC ";
  sAux = sAux + sRESETREASON + ">SLP" + uintToStr(sleep_time_us / 60000000ULL) + "@" + (String)(int)(millis() / 1000) + "/" + (String)(lSecsCycle) + "s/c";
  LogAlert( sAux, 1);
  writeSPIFFSFile("/json.txt", sJsonDev.c_str());
  FlushDefLog();
  if ((bFBModified) && (WiFi.status() == WL_CONNECTED)) bFlushJsonDev(false);
  delay(100);
  if (WiFi.status() == WL_CONNECTED)  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);
  bWeHaveWifi = false;
  tNow = tNow + (mins * 60);
  Serial.println("]. Zzz");
  delay(100);
  esp_sleep_enable_timer_wakeup(sleep_time_us);
  esp_deep_sleep_start();
}////////////////////////////////////////////////////////////////////////////////////
void SendToRestart() {
  Serial.print("\n[-> To reset... ");
  //  lSecsOn += millis()/1000;
  if (lBoots > 2) lBoots--;
  String sAux = "";
  if (abs(fInsideTemp + fTempInOffset) < 50)  sAux = String(fInsideTemp + fTempInOffset) + "ºC ";
  long lSecsCycle = ((lBoots) ? (0.8 * lSecsOn / lBoots) : 30);
  sAux = sAux + sRESETREASON + ">RST @" + (String)(int)(millis() / 1000) + "/" + (String)(lSecsCycle) + "s/c";
  LogAlert( sAux, 1);
  writeSPIFFSFile("/json.txt", sJsonDev.c_str());
  FlushDefLog();
  if ((bFBModified) && (WiFi.status() == WL_CONNECTED)) bFlushJsonDev(false);
  if (WiFi.status() == WL_CONNECTED)  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);
  bWeHaveWifi = false;
  Serial.println("].");
  delay(500);
  ESP.restart();
}////////////////////////////////////////////////////////////////////////////////////
#define BATTTHRESHOLD 2000
int dGetVoltagePerc() {
  if (!io_VOLTAGE) return -1;
  float Vtg, dCurrVoltage;
  int BattPerc, i, iAux;
  while (bGettingRawVoltage) {
    delay(500);
    Serial.print("~");
  }
  if (10000 > time(nullptr)) {
    LogAlert(" NO TIME LOADED on dGetVoltagePerc!", 1);
    return -1;
  }
  if (iLastVtgNotWritten > 0)      AddVtg(iLastVtgNotWritten);
  if ((time(nullptr) - tVtgTime[VTGMEASSURES - 1]) > PERIODBETWEENVTGS) {  ////300
    Serial.print(" [New VTG Meassure needed]");
    AddVtg(iGetMeanAnalogValue(io_VOLTAGE, 5, 5));
  }
  Vtg = iVtgVal[VTGMEASSURES - 1];
  int BattSlope1 = fLinearfit(iVtgVal, tVtgTime, VTGMEASSURES, 1);
  int BattSlope4 = fLinearfit(iVtgVal, tVtgTime, VTGMEASSURES, 4);
  int BattSlope24 = fLinearfit(iVtgVal, tVtgTime, VTGMEASSURES, 24);
  int BattSlopeChg = fLinearfit(iVtgVal, tVtgTime, VTGMEASSURES, (tNow - tTimeLastVtgChg) / 3600);
  dCurrVoltage = 4.15 * Vtg / iVtgMax;
  float fVaux = iVtgMin;
  if (fVaux > ((float)iVtgMax * .8)) fVaux = (float)iVtgMax * .8;
  //  BattPerc = (int)(((float)(100) / (float)(1 - MINBATTFACTOR)) * ((float)(Vtg) / (float)(iVtgMax) - MINBATTFACTOR));
  BattPerc = 100 * (Vtg - fVaux) / ((float)(iVtgMax) - fVaux);
  // BattSlope1=0; BattSlope4=0;  BattSlope24=0;
  iBattStatus = -1;
  for (i = 0; i < VTGMEASSURES; i++) if (iVtgVal[i] > 0) break;
  ////////////////////////////////////////////////////////////////////////
  if ((tVtgTime[VTGMEASSURES - 1] - tVtgTime[i]) < PERIODBETWEENVTGS)     iBattStatus = 0;//NO INFO
  if ((iBattStatus < 0 ) && (BattSlope1 > (BATTTHRESHOLD * 2))) iBattStatus = 1;         //CHARGING.
  if ((iBattStatus < 0 ) && (BattPerc > 98)) iBattStatus = 2;             						    //FULL.
  if ((iBattStatus < 0 ) && (BattSlope4 > (BATTTHRESHOLD  / 2))) iBattStatus = 1;         //CHARGING.
  if (iBattStatus < 0 ) iBattStatus = 3; //If not all previous should be DISCHARGING as slope is really low
  ////////////////////////////////////////////////////////////////////////
  if ((iBattStatus > 2) && (BattSlope4 > (-1 * BATTTHRESHOLD / 100))) { //Stable Discharge
    if ((Vtg > iVtgStableMax) && ((iVtgMax * 0.985) > Vtg)) iVtgStableMax = Vtg;
    if (iVtgStableMin > Vtg ) iVtgStableMin = Vtg;
  }
  if ((tVtgTime[18] > 0) && bFBDevAvail && (iBattStatus < 3) && ((BattSlope4 > (BATTTHRESHOLD / 2)) || (tTimeLastVtgChg == 0))) { //Charging
    if (abs(iVtgChg - Vtg) > (Vtg * .005)) {
      if ((tNow - tTimeLastVtgChg) > 604800) {
        LogAlert("New CHG!", 3);
      }
      Serial.print("[‚¨]");
      if ((iVtgMax * .98) > Vtg)      iVtgChg = Vtg ;
      else iVtgChg = iVtgMax * .98;
      tTimeLastVtgChg = tNow;
    }
  }
  if (5 > BattPerc) iBattStatus = 4;
  if (iBattStatus == -1) iBattStatus = 0;
  if ((dCurrVoltage > 0) && (dCurrVoltage < 2.9) ) { //CUT
    DisplayU8Text(5, iScreenYMax * .6, " LOW BATTERY -> OFF  ", fU8g2_XL, bRed ? GxEPD_RED : GxEPD_BLACK);
    bRefreshPage();
    String sLowVolt = (String)(Vtg);
    writeSPIFFSFile("/lowvolt.txt", sLowVolt.c_str());
    LogAlert("Batt low " + (String)(dCurrVoltage) + "V", 3);
    delay(2000);
    SendToSleep(60);
  }
  PaintBatt(BattPerc);
  bSetVtgValues();
  return BattPerc;
}//////////////////////////////////////////////////////////////////////////////
#define MAXPERCDROPPERMEASSURE 0.01
void AddVtg(int iVtg) {
  if (!io_VOLTAGE) return;
  int iAux = 0, i;
  if ((iVtg > iVtgMax) && (iVtg < VTGMAXIMALLOWED)) {
    iVtgMax = iVtg;
    LogAlert(" [New VtgMax=" + (String)(iVtgMax) + "]", 1);
    tTimeLastVtgMax = tNow;
    if (iVtgMin > (iVtgMax * .8)) iVtgMin = iVtgMax * .8;
  }
  if ((iVtg < iVtgMin) && (iVtg > 0)) {
    iVtgMin = iVtg;
    Serial.print(" [New VtgMin=" + (String)(iVtgMin) + "]");
  }
  if (tNow > 10000) {
    iAux = abs( tNow - tTimeLastVtgChg) / 3600; //Hours
    if ((tTimeLastVtgChg > 0) && (iAux > (iVtgHPeriodMax)) && (abs( iVtgChg - iVtg) > 50)) {
      iVtgHPeriodMax = iAux;
      if (iVtg > iVtgStableMin) iVtgPeriodDrop = iVtgChg - iVtg;
      else iVtgPeriodDrop = iVtgChg - iVtgStableMin;
      Serial.print(" [New VtgDrop=" + (String)(tNow) + "," + (String)(tTimeLastVtgChg) + "->" + (String)(iVtgHPeriodMax) + "/" +  (String)(iVtgPeriodDrop)  + "]");
    }
  } else       Serial.print(" [tNow=" + (String)(tNow) + "]!!");

  if ((iVtgVal[VTGMEASSURES - 1] - iVtg) > ((float)iVtgVal[VTGMEASSURES - 1] * (float)MAXPERCDROPPERMEASSURE)) { // prevent incorrect drops
    LogAlert("Cointained Vtg drop " + (String)iVtgVal[VTGMEASSURES - 1] + "->" + (String)iVtg + "? -> " + (String)((float)iVtgVal[VTGMEASSURES - 1] * ((float)1 - (float)MAXPERCDROPPERMEASSURE)), 1);
    iVtg = (float)iVtgVal[VTGMEASSURES - 1] * 0.98;
  }
  if (tNow > 100000) { // time available
    bool bCh1, bCh2, bCh3;
    int iStep1, iStep2, iStep3;
    iStep1 = (int)(VTGMEASSURES * 1 / 3);
    iStep2 = (int)(VTGMEASSURES * 2 / 3);
    iStep3 = (int)(VTGMEASSURES );
    bCh1 = (tVtgTime[iStep1 ] - tVtgTime[iStep1 - 1]) > (24 * 3600);
    bCh2 = (tVtgTime[iStep2 ] - tVtgTime[iStep2 - 1]) > (4 * 3600);
    bCh3 = ((tNow - tVtgTime[VTGMEASSURES - 1]) > PERIODBETWEENVTGS);
    if (bCh3) {
      // Divide in 3 steps 1h,4h,24h
      if (bCh2 ) {
        //Rolling down 2/3rds
        if (bCh1) {
          for ( i = 0; i < iStep1; i++) {
            iVtgVal[i] = iVtgVal[i + 1];
            tVtgTime[i] = tVtgTime[i + 1];
          }
        }
        for ( i = iStep1; i < iStep2; i++) {
          iVtgVal[i] = iVtgVal[i + 1];
          tVtgTime[i] = tVtgTime[i + 1];
        }
      }
      for ( i = iStep2; i < VTGMEASSURES; i++) {
        iVtgVal[i] = iVtgVal[i + 1];
        tVtgTime[i] = tVtgTime[i + 1];
      }
      iVtgVal[VTGMEASSURES - 1] = iVtg;
      tVtgTime[VTGMEASSURES - 1] = tNow;
      iAux = iWriteSlopesSPIFFS(iVtgVal, tVtgTime, VTGMEASSURES);
      Serial.print(" {$" + String(bCh1 ? "1" : "") + String(bCh2 ? "2" : "") + String(bCh3 ? "3" : "") + ":" + String(iAux) + "}" );
    } else Serial.print(" {E}");
    iLastVtgNotWritten = 0;
  } else {
    Serial.print(" {@}");
    iLastVtgNotWritten = iVtg;
  }
}/////////////////////////////////////////////////////////////////////////////////////////
int StartWifiAP(bool bUpdateDisplay) {
  const char *APssid = "ESP32";
  WiFi.softAP(APssid);
  //  IPAddress ip(192, 168, 1, 1);
  Serial.print("Started AP ESP32 with IP address: ");
  Serial.println(WiFi.softAPIP());
  wifi_config_t conf;
  esp_wifi_get_config(WIFI_IF_STA, &conf);
  sWifiSsid = String(reinterpret_cast<const char*>(conf.sta.ssid));
  sWifiPassword = String(reinterpret_cast<const char*>(conf.sta.password));
  if (bUpdateDisplay) {
    bNewPage();
    display.fillScreen(GxEPD_WHITE);
    display.setTextColor(GxEPD_BLACK);
    DisplayU8Text(1, 20, "Web server Setup MODE entered", fU8g2_M, GxEPD_BLACK);
    DisplayU8Text(1, 80, " Stored ssid [" + sWifiSsid + "], pass: " + sWifiPassword.length() + " chars", fU8g2_S, GxEPD_BLACK);
    DisplayU8Text(1, 100, " Please connect to WIFI ssid 'ESP32'", fU8g2_S, GxEPD_BLACK);
    DisplayU8Text(1, 120, " open http://192.168.4.1", fU8g2_S, GxEPD_BLACK);
    DisplayU8Text(1, 140, " enter params, and reboot when done..", fU8g2_S, GxEPD_BLACK);
    DisplayU8Text(1, 180, " If you use a phone, first connect airplane", fU8g2_S, GxEPD_BLACK);
    DisplayU8Text(1, 200, " mode , and then enable the wifi, so you", fU8g2_S, GxEPD_BLACK);
    DisplayU8Text(1, 220, " avoid 3G data connection interference..", fU8g2_S, GxEPD_BLACK);
    bRefreshPage();
    display.fillScreen(GxEPD_WHITE);
  }
}//////////////////////////////////////////////////////////////////////////////
void tGetRawVoltage(void * pvParameters) {
  if (!io_VOLTAGE) return;
  bGettingRawVoltage = true;
  AddVtg(iGetMeanAnalogValue(io_VOLTAGE, 5, 5));
  bGettingRawVoltage = false;
  vTaskDelete( NULL );
}//////////////////////////////////////////////////////////////////////////////
void ShowValueScreen() {
  String sAux = "", sName;
  int i = 0, iAux = 0;
  time_t tAux;
  bNewPage();
  display.fillScreen(GxEPD_WHITE);
  DisplayU8Text(1 , 15, "STORED VALUES", fU8g2_S);
  display.drawLine(1, 16, 120, 16, GxEPD_BLACK);
  DisplayU8Text(1 , 45, "API=" + sWeatherAPI, fU8g2_XS);
  DisplayU8Text(1 , 60, "LOC=" + sWeatherLOC, fU8g2_XS);
  DisplayU8Text(1 , 75, "iRefreshPeriod=" + (String)(iRefreshPeriod), fU8g2_XS);
  sAux = "Vtg (Max\\Min)= (" + (String)(iVtgMax) + "\\" + (String)(iVtgMin) + ")";
  DisplayU8Text(1 , 95, sAux, fU8g2_XS);
  sAux = "LastTimeFull=" + sInt32TimetoStr(tTimeLastVtgMax);
  DisplayU8Text(1 , 110, sAux, fU8g2_XS);
  DisplayU8Text(5, 134 , "SSID", fU8g2_XS);
  DisplayU8Text(100, 134, "PASS length", fU8g2_XS);
  display.drawLine(5, 135, 170, 135, GxEPD_BLACK);
  String sWifiJson = readSPIFFSFile("/wifi.txt");
  if (sWifiJson.length()) {
    DynamicJsonBuffer jsonBuffer(256);
    JsonObject& root = jsonBuffer.parseObject(sWifiJson);
    if (root.success()) {
      String sFSSsid, sFSPswd;
      for (i = 0 ; i < 10; i++) {
        sAux = "WIFISSID" + String(i);
        sFSSsid = root[sAux].as<String>();
        if (sFSSsid.length() > 0) {
          sAux = "WIFIPSWD" + String(i);
          sFSPswd = root[sAux].as<String>();
          DisplayU8Text(5, 150 + i * 15 , sFSSsid, fU8g2_XS);
          iAux = sFSPswd.length();
          DisplayU8Text(100, 150 + i * 15 ,  (String)(iAux), fU8g2_XS);
        } else break;
      }
    }
  }
  DisplayU8Text(iScreenXMax * .6 , 11, "VtgVal       VtgTime", fU8g2_XS);
  display.drawLine(iScreenXMax * .6, 12, iScreenXMax, 12, GxEPD_BLACK);
  display.drawLine(iScreenXMax * .6 - 2, 1, iScreenXMax * .6 - 2, iScreenYMax, GxEPD_BLACK) ;
  for (i = 0; i < VTGMEASSURES; i++) {
    iAux = iVtgVal[i];
    DisplayU8Text(iScreenXMax * .6 , 11 + (iScreenYMax / VTGMEASSURES) * (i + 1), (String)(iAux), fU8g2_XS);
    tAux = tVtgTime[i];
    sAux = asctime(localtime(&tAux ) );
    DisplayU8Text(iScreenXMax * .68 , 11 + (iScreenYMax / VTGMEASSURES) * (i + 1), sAux, fU8g2_XS);
  }
  bRefreshPage();
  delay(60000);
}//////////////////////////////////////////////////////////////////////////////
void FillEmptyVtgVal() {
  int i, j;
  for (i = 0; i < VTGMEASSURES; i++) {
    if (tVtgTime[i] != 0) break;
  }
  if (i > (VTGMEASSURES - 2)) return; //NO DATA
  for (j = i; j > -1; j--) {
    tVtgTime[j] = tVtgTime[j + 1] - 30;
    iVtgVal[j] = iVtgVal[j + 1];
  }
  iWriteSlopesSPIFFS(iVtgVal, tVtgTime, VTGMEASSURES);
}//////////////////////////////////////////////////////////////////////////////
bool bGetJSONString(String sHost, String sJSONSource, String * jsonString) {
  int httpPort = 443;
  unsigned long timeout ;
  WiFiClientSecure  SecureClientJson;
  Serial.print("  Connecting to " + String(sHost) );
  if (!SecureClientJson.connect(const_cast<char*>(sHost.c_str()), httpPort)) {
    LogAlert(" **Connection failed for JSON " + sJSONSource + "**", 1);
    return false;
  }
  SecureClientJson.print(String("GET ") + sJSONSource + " HTTP/1.1\r\n" + "Host: " + sHost + "\r\n" + "Connection: close\r\n\r\n");
  timeout = millis();
  while (SecureClientJson.available() == 0) {
    if (millis() - timeout > 10000) {
      LogAlert(">>> Client Connection Timeout...Stopping", 1);
      SecureClientJson.stop();
      return false;
    }
  }
  Serial.print(" done. Get json,");
  while (SecureClientJson.available()) {
    *jsonString = SecureClientJson.readStringUntil('\r');
  }
  Serial.print("done," + String(jsonString->length()) + " bytes long.");
  SecureClientJson.stop();
  return true;
}//////////////////////////////////////////////////////////////////////////////////////////////////
String sGetGPSLocality(String sGPS, String sAPI) {
  String sJsonData = "";
  String sJsonHost = "maps.googleapis.com";
  String sJsonSource = "https://maps.googleapis.com/maps/api/geocode/json?latlng=" + sGPS + "&result_type=locality&sensor=false&key=" + sAPI;
  if (bGetJSONString(sJsonHost, sJsonSource, &sJsonData)) {
    DynamicJsonBuffer jsonBuffer(256);
    JsonObject& root = jsonBuffer.parseObject(sJsonData);
    if (!root.success()) {
      Serial.print("No root on sGetGPSLocality");
      return "";
    }
    return root["results"][0]["address_components"][0]["short_name"].as<String>();
  } else {
    Serial.print("No json on sGetGPSLocality");
    return "";
  }
}//////////////////////////////////////////////////////////////////////////////
bool bGetJsonDev() {
  if (sMACADDR.length() < 10) return false;
  if ((bFBModified) && (sJsonDev.length() > 4)) {
    bFBDownloaded = true;      //avoid writting over changes
    Serial.print("¥");
    return true;
  }
  if ((sJsonDev.length() < 4) ||  (!bFBDownloaded))     {
    delay(10);
    sJsonDev = "{" + Firebase.getString("/dev/" + sMACADDR) + "}";
    delay(10);
    Serial.print("~D(" + (String)(sJsonDev.length()) + ")~");
  }
  if (sJsonDev.length() < 4) {
    Serial.print("ª");
    bFBDownloaded = false;
  } else {
    Serial.print("º");
    bFBDownloaded = true;
  }
  return (sJsonDev.length() > 4);
}//////////////////////////////////////////////////////////////////////////////////////////////////
bool bGetPartialJsonDev(String sPartial) { //Under development
  if (sMACADDR.length() < 10) return false;
  if ((bFBModified) && (sJsonDev.length() > 4)) {
    bFBDownloaded = true;      //avoid writting over changes
    Serial.print("¥");
    return true;
  }
  String sJsonDevPartial, sAux;
  if ((sJsonDev.length() < 4) ||  (!bFBDownloaded))     {
    DynamicJsonBuffer jsonBuffer(256);
    JsonObject& root = jsonBuffer.parseObject(sJsonDev);
    JsonObject& rootS = root[sPartial];
    if (!rootS.success()) return false;
    //    Serial.print("1-" + sJsonDev);
    delay(10);
    sJsonDevPartial = "{" + Firebase.getString("/dev/" + sMACADDR + "/" + sPartial) + "}";
    delay(10);
    DynamicJsonBuffer jsonBufferP(256);
    JsonObject& rootP = jsonBufferP.parseObject(sJsonDevPartial);
    for (auto kvp : rootP) {
      rootS[kvp.key] = kvp.value;
    }
    sAux = "";
    root.printTo(sAux);
    if (sAux.length()) sJsonDev = sAux;
    //    Serial.println("\n3-" + sJsonDev);
    Serial.print("~PD [" + sPartial + "] (" + (String)(sJsonDev.length()) + ")~");
  }
  if (sJsonDev.length() < 4) {
    Serial.print("ª");
    bFBDownloaded = false;
  } else {
    Serial.print("º");
    bFBDownloaded = true;
  }
  return (sJsonDev.length() > 4);
}//////////////////////////////////////////////////////////////////////////////////////////////////
bool bFlushJsonDev(bool bForce) {
  if (sMACADDR.length() < 10) return false;
  if (!bFBAvailable) return false;
  if ((sJsonDev.length() > 4) && (bFBModified))     {
    DynamicJsonBuffer jsonBuffer(256);
    JsonObject& root = jsonBuffer.parseObject(sJsonDev);
    if (root.success()) {
      delay(50);
      Firebase.set("/dev/" + sMACADDR, root);
      delay(50);
      if (Firebase.failed()) {
        Serial.print("\n !! Setting json_Time failed :");
        LogDef("Firebase update ERROR (" + (String)(sJsonDev.length()) + "B)", 2);
        return false;
      }
      if (!bForce) tLastFBUpdate = tNow;
      bFBModified = false;
      bFBDownloaded = true;
      Serial.print("~U_Ok(" + (String)(sJsonDev.length()) + ")~");
      return true;
    } else  {
      Serial.print("~U_Nok(" + (String)(sJsonDev.length()) + ")~");
      LogDef("bFlushJsonDev root ERROR (" + (String)(sJsonDev.length()) + "B)", 2);
      return false;
    }
  }
  return false;
}//////////////////////////////////////////////////////////////////////////////
bool bFlushPartialJsonDev(String sPartial) {
  if (sMACADDR.length() < 10) return false;
  if (!bFBAvailable) return false;
  if ((sJsonDev.length() > 4) && (bFBModified) && (sPartial.length()))     {
    DynamicJsonBuffer jsonBuffer(256);
    JsonObject& root = jsonBuffer.parseObject(sJsonDev);
    JsonObject& rootS = root[sPartial];
    if (rootS.success()) {
      delay(50);
      Firebase.set("/dev/" + sMACADDR + "/" + sPartial, rootS);
      delay(50);
      if (Firebase.failed()) {
        Serial.print("\n !! Setting json_Time failed :");
        LogDef("Firebase partial update ERROR @ " + sPartial  + " (" + (String)(sJsonDev.length()) + "B)", 2);
        return false;
      }
      Serial.print("~PU_Ok(" + (String)(sJsonDev.length()) + ")~");
      return true;
    } else  {
      Serial.print("~PU_Nok(" + (String)(sJsonDev.length()) + ")~");
      return false;
    }
  }
  return false;
}//////////////////////////////////////////////////////////////////////////////////////////////////
bool bUpdateRootJsonDev(JsonObject & root) {
  if (!root.success()) return false;
  String sAux = "";
  root.printTo(sAux);
  if (sAux.length() > 4) {
    sJsonDev = sAux;
    Serial.print("~M~");
    bFBModified = true;
  }
  return true;
}//////////////////////////////////////////////////////////////////////////////////////////////////
bool bCheckJsonDevStruct() {
  DynamicJsonBuffer jsonBuffer(256);
  JsonObject& root = jsonBuffer.parseObject(sJsonDev);
  if (root.success()) {
    FBrootCheckSubPath(root, "Functions");
    FBrootCheckSubPath(root, "vars");
    FBrootCheckSubPath(root, "Misc");
    FBrootCheckSubPath(root, "Vtg");
    FBrootCheckSub2Path(root, "Vtg", "VHist");
    return true;
  } else return false;
}//////////////////////////////////////////////////////////////////////////////////////////////////
bool bCheckFBAvailable() {
  String sAux = Firebase.getString("/Available");
  //  Serial.println("\n[FBAVailability=" + sAux + "]");
  return (sAux.length() > 0);
}//////////////////////////////////////////////////////////////////////////////////////////////////
bool bCheckFBDevAvailable(bool bWriteJson) {
  if (sMACADDR.length() < 10) return false;
  String sAux = "{" + Firebase.getString("/dev/" + sMACADDR) + "}";
  if (sAux.length() > 4) {
    if (bWriteJson) sJsonDev = sAux;
    return true;
  } else return false;
}//////////////////////////////////////////////////////////////////////////////////////////////////
bool FBCheckroot2ContainsFlt(JsonObject & root,  String sSubPath, String sVarName, float fVar, bool bCreate) {
  if (!FBrootCheckSubPath(root, sSubPath)) return false;
  JsonObject& rootS = root[sSubPath];
  if (rootS.success()) {
    if (!rootS.containsKey(sVarName)) {
      if (bCreate) {
        rootS[sVarName] = fVar;
        bUpdateRootJsonDev(root);
        return true;
      } else {
        //        LogAlert(",NO Key:" + sVarName + " in " + sSubPath + ".", 1);
        return false;
      }
    } else return true;
  } else {
    LogAlert(",NO Path:" + sSubPath, 1);
    return false;
  }
}//////////////////////////////////////////////////////////////////////////////////////////////////
bool FBCheckroot2ContainsStr(JsonObject & root,  String sSubPath, String sVarName, String sVar, bool bCreate) {
  if (!FBrootCheckSubPath(root, sSubPath)) return false;
  JsonObject& rootS = root[sSubPath];
  if (rootS.success()) {
    if (!rootS.containsKey(sVarName)) {
      if (bCreate) {
        rootS[sVarName] = sVar;
        bUpdateRootJsonDev(root);
        return true;
      } else {
        //        LogAlert(",NO Key:" + sVarName + " in " + sSubPath + ".", 1);
        return false;
      }
    } else return true;
  } else {
    LogAlert(",NO Path:" + sSubPath, 1);
    return false;
  }
}//////////////////////////////////////////////////////////////////////////////
bool FBrootCheckSubPath(JsonObject & root, String sSubPath) {
  if (!root.success()) {
    Serial.print(",No valid root on FBrootCheckSubPath(" + sSubPath + ")");
    return false;
  }
  JsonObject& rootS = root[sSubPath];
  if (!rootS.success()) {
    JsonObject& NSubPath = root.createNestedObject(sSubPath);
    Serial.print(",Path:'" + sSubPath + "' created.");
    bUpdateRootJsonDev(root);
    JsonObject& rootChk = root[sSubPath];
    return rootChk.success();
  } else return true;
}//////////////////////////////////////////////////////////////////////////////
bool FBrootCheckSub2Path(JsonObject & root, String sSubPath, String sSubSubPath) {
  if (!FBrootCheckSubPath(root, sSubPath)) return false;
  JsonObject& rootS = root[sSubPath];
  JsonObject& rootSS = rootS[sSubSubPath];
  if (!rootSS.success()) {
    JsonObject& NSubPath = rootS.createNestedObject(sSubSubPath);
    Serial.print(",Path:'" + sSubSubPath + "' created.");
    bUpdateRootJsonDev(root);
    JsonObject& rootChk = rootS[sSubSubPath];
    return rootChk.success();
  } else return true;
}/////////////////////////////////////////////////////////////////////////////
bool FBUpdate1rootFlt(JsonObject & root,  String sName, float fVal ) {
  bool bUpdt = false;
  if (!root.containsKey(sName)) bUpdt = true;
  else if ((float)(root[sName]) != fVal) bUpdt = true;
  if (bUpdt) {
    root[sName] = fVal;
    bUpdateRootJsonDev(root);
    return true;
  } else return false;
}//////////////////////////////////////////////////////////////////////////////
bool FBUpdate2rootFlt(JsonObject & root,  String sSubPath, String sName, float fVal ) {
  bool bUpdt = false;
  FBrootCheckSubPath(root, sSubPath);
  JsonObject& rootS = root[sSubPath];
  if (!rootS.containsKey(sName)) bUpdt = true;
  if ((float)(rootS[sName]) != fVal) bUpdt = true;
  if (bUpdt) {
    rootS[sName] = fVal;
    bUpdateRootJsonDev(root);
    return true;
  } else return false;
}//////////////////////////////////////////////////////////////////////////////
bool FBUpdate1rootStr(JsonObject & root, String sName, String sVal ) {
  bool bUpdt = false;
  if (!root.containsKey(sName)) bUpdt = true;
  String sAux = root[sName];
  if (sAux != sVal) bUpdt = true;
  if (bUpdt) {
    root[sName] = sVal;
    bUpdateRootJsonDev(root);
    return true;
  }
}//////////////////////////////////////////////////////////////////////////////
bool FBUpdate2rootStr(JsonObject & root,  String sSubPath, String sName, String sVal ) {
  bool bUpdt = false;
  FBrootCheckSubPath(root, sSubPath);
  JsonObject& rootS = root[sSubPath];
  if (!rootS.containsKey(sName)) bUpdt = true;
  String sAux = rootS[sName];
  if (sAux != sVal) bUpdt = true;
  if (bUpdt) {
    rootS[sName] = sVal;
    bUpdateRootJsonDev(root);
    return true;
  }
}//////////////////////////////////////////////////////////////////////////////
bool FB_SetBatt(int iVtg, String sBattMsg) {
  if (!bGetJsonDev()) return false;
  DynamicJsonBuffer jsonBuffer(256);
  JsonObject& root = jsonBuffer.parseObject(sJsonDev);
  if (!root.success()) return false;
  FBUpdate2rootFlt(root,  "Vtg", "VMax", iVtgMax);
  FBUpdate2rootFlt(root,  "Vtg", "VChg", iVtgChg);
  FBUpdate2rootFlt(root,  "Vtg", "VMin", iVtgMin);
  FBUpdate2rootFlt(root,  "Vtg", "VNow", iVtg);
  FBUpdate2rootFlt(root,  "Vtg", "tLstChg",  tTimeLastVtgChg);
  FBUpdate2rootFlt(root,  "Vtg", "tLstMax",  tTimeLastVtgMax);
  FBUpdate2rootFlt(root,  "Vtg", "tLstDown", tTimeLastVtgDown);
  FBUpdate2rootFlt(root,  "Vtg", "PeriodDrop", iVtgPeriodDrop  );
  FBUpdate2rootFlt(root,  "Vtg", "PeriodMax", iVtgHPeriodMax  );
  FBUpdate2rootFlt(root,  "Vtg", "StableMax", iVtgStableMax  );
  FBUpdate2rootFlt(root,  "Vtg", "StableMin", iVtgStableMin  );
  FBUpdate2rootStr(root,  "Misc", "Status", sBattMsg);
  String sText;
  float fVaux;
  if (iVtgPeriodDrop == 0) iVtgPeriodDrop = 1;
  if (iVtgChg > (iVtg * .99)) sText = (String)(iBattPercentage(iVtg)) + "%l," + (String)(iBattPercentageCurve(iVtg)) +  "%c,(" + (String)((tNow - tTimeLastVtgChg) / 3600) + "h)" + (String)(int)(fLinearfit(iVtgVal, tVtgTime, VTGMEASSURES, (tNow - tTimeLastVtgChg) / 3600)) + ",Sc" + (String)((float)(tNow - tTimeLastVtgChg) / ((float)iVtgChg - (float)iVtg) / 864) + ",St" + (String)((float)(iVtgHPeriodMax * 3600) / ((float)iVtgPeriodDrop * 6048)) + ",1h" + (String)(int)(fLinearfit(iVtgVal, tVtgTime, VTGMEASSURES, 1)) + ",4h" + (String)(int)(fLinearfit(iVtgVal, tVtgTime, VTGMEASSURES, 4));
  else sText = (String)(iBattPercentage(iVtg)) + "%l," + (String)(iBattPercentageCurve(iVtg)) +  "%c,CHGN,1h" + (String)(int)(fLinearfit(iVtgVal, tVtgTime, VTGMEASSURES, 1)) + ",4h" + (String)(int)(fLinearfit(iVtgVal, tVtgTime, VTGMEASSURES, 4));
  FBUpdate2rootStr(root,  "Misc" , "Slope", sText);
  return true;
}//////////////////////////////////////////////////////////////////////////////
bool FB_SetMisc() {
  if (!bGetJsonDev()) return false;
  DynamicJsonBuffer jsonBuffer(256);
  JsonObject& root = jsonBuffer.parseObject(sJsonDev);
  if (!root.success()) return false;
  char buff[30];
  sprintf(buff, "%04d/%02d/%02d %02d:%02d:%02d", year(tNow), month(tNow), day(tNow), hour(tNow), minute(tNow), second(tNow));
  FBUpdate2rootStr(root,  "Misc", "TimeLastUpdate", (String)(buff));
  sprintf(buff, "%04d/%02d/%02d %02d:%02d:%02d", year(tLastSPIFFSWeather), month(tLastSPIFFSWeather), day(tLastSPIFFSWeather), hour(tLastSPIFFSWeather), minute(tLastSPIFFSWeather), second(tLastSPIFFSWeather));
  FBUpdate2rootStr(root,  "Misc", "TimeLastWeather", (String)(buff));
  if (tNow > tFirstBoot) {
    float fMeanOn = lSecsOn * 86400 / (tNow - tFirstBoot);
    float fMeanBoots = lBoots * 86400 / (tNow - tFirstBoot);
    FBUpdate2rootStr(root, "Misc", "DailyUse", (String)(int)(fMeanOn) + "s/" + (String)(int)(fMeanBoots) + "b=" + (String)(int)(fMeanOn / fMeanBoots) + "sec/boot");
  }
  FBUpdate2rootStr(root,   "Misc", "Ssid", sWifiSsid);
  if (bInsideTempSensor) FBUpdate2rootStr(root,   "Misc", "TempIn", String(fInsideTemp + fTempInOffset));
  else FBUpdate2rootStr(root, "Misc", "TempIn", "-");
  FBUpdate2rootStr(root,  "Misc", "Soft_Rev", (String)(REVISION) + " " + sPlatform());
  FBUpdate2rootStr(root,  "Misc", "Soft_Wrtn", (String)(compile_date));
  //  FBUpdate2rootStr(root,  "Misc", "Id", sDevID);
  return true;
}//////////////////////////////////////////////////////////////////////////////
bool FB_SetWeatherJson(String jsonW) {
  if (!jsonW.length()) return false;
  jsonW = bSummarizeJsonW(jsonW);
  if (jsonW.length() < 100) return false;
  String sAux1 = "/Weather/" + sWeatherLOC + "/";
  int iTimes, iLength, iBlockSize = 9999;
  long int tFirst = 0;
  sAux1.replace(".", "'");
  if (tNow) {
    Firebase.setFloat(sAux1 + "Time", (float)(tNow));
    delay(10);
  } else return false;
  Firebase.setFloat(sAux1 + "Size", (float)(jsonW.length()));
  delay(10);
  Firebase.remove(sAux1 + "json/" );
  iLength = jsonW.length();
  for (int i = 0; i < (int)(1 + (iLength / iBlockSize)); i++ ) {
    Firebase.setString(sAux1 + "json/" + (String)(i), jsonW.substring((i * iBlockSize), (i + 1)*iBlockSize));
    delay(10);
  }
  /// Set, now play with your toys
  String sLocality = Firebase.getString(sAux1 + "Locality");
  if (sLocality.length() < 2) {
    sLocality = sGetGPSLocality(sWeatherLOC, sGeocodeAPIKey);
    Firebase.setString(sAux1 + "Locality", sLocality);
    LogAlert("[New Locality='" + sLocality + "']", 2);
  }
  delay(10);
  iTimes = Firebase.getFloat(sAux1 + "nDownloads");
  iTimes += 1;
  delay(10);
  tFirst = Firebase.getFloat(sAux1 + "tFirstDownload");
  delay(10);
  if ((tNow) && (!tFirst)) Firebase.setFloat(sAux1 + "tFirstDownload", (float)(tNow));
  delay(10);
  Firebase.setFloat(sAux1 + "nDownloads", (float)(iTimes));
  delay(10);
  if (tNow > tFirst) Firebase.setFloat(sAux1 + "MeanDownDay", (float)(iTimes * 86400 / (tNow - tFirst)));
  delay(10);
  return true;
}//////////////////////////////////////////////////////////////////////////////
bool FB_GetWeatherJson(String * jsonW) {
  String sAux1 = "/Weather/" + sWeatherLOC + "/", sAux2;
  int iLength, iBlockSize = 9999;
  sAux1.replace(".", "'");
  float timeUploaded = Firebase.getFloat(sAux1 + "Time");
  if (Firebase.failed()) {
    Serial.print("getting json_Time failed : ");
    Serial.println(Firebase.error());
    return false;
  }
  if ((tNow - timeUploaded) > (iRefreshPeriod * 60)) {
    Serial.print("[getting json too old. Rejected.]");
    return false; //Too old
  }
  delay(10);
  iLength = Firebase.getFloat(sAux1 + "Size");
  if (Firebase.failed()) {
    Serial.print("getting json_length failed : ");
    Serial.println(Firebase.error());
    return false;
  }
  delay(10);
  sAux2 = "";
  for (int i = 0; i < (int)(1 + (iLength / iBlockSize)); i++ ) {
    sAux2 = sAux2 + Firebase.getString(sAux1 + "json/" + (String)(i));
    if (Firebase.failed()) {
      Serial.print("setting json failed: ");
      Serial.println(Firebase.error());
      return false;
    }
    delay(10);
  }
  sAux2.trim();
  String s1 = "\\" + (String)(char(34));
  String s2 = (String)(char(34));
  sAux2.replace(s1, s2);
  sAux2.replace("\\n", "");
  *jsonW = sAux2;
  /// Get, now play with your toys
  delay(10);
  String sLocality = Firebase.getString(sAux1 + "Locality");
  if ((sLocality.length() > 2) && (sCustomText.length() < 2)) {
    sCustomText = sLocality;
    DynamicJsonBuffer jsonBuffer(256);
    JsonObject& root = jsonBuffer.parseObject(sJsonDev);
    if (root.success()) {
      FBUpdate2rootStr(root, "vars", "CustomText", sCustomText);
    }
  }
  delay(10);
  return true;
}//////////////////////////////////////////////////////////////////////////////////////////////////
String bSummarizeJsonW(String sJWO) {
  DynamicJsonBuffer jsonBufferO(1024);
  JsonObject& rootO = jsonBufferO.parseObject(sJWO);
  if (!rootO.success()) {
    Serial.print("\nERROR SUMMARIZED no rootO " + sJWO.substring(0, 100) + "\n" + sJWO.substring(sJWO.length() - 100, sJWO.length()) + "\n");
    return "{\"kk\":\"no root ORIGINAL Ok\"}";
  }
  DynamicJsonBuffer jsonBufferN(256);
  JsonObject& rootN = jsonBufferN.parseObject("{}");
  if (!rootN.success()) {
    Serial.print("\nERROR SUMMARIZED no rootN\n");
    return "{\"kk\":\"no root NEW Ok\"}";
  }
  JsonObject& NSubPath1 = rootN.createNestedObject("currently");
  JsonObject& NSubPath2 = rootN.createNestedObject("daily");
  JsonObject& NSubPath3 = rootN.createNestedObject("hourly");
  rootN["offset"] = rootO["offset"];
  rootN["currently"]["temperature"] = rootO["currently"]["temperature"];
  rootN["currently"]["time"] = rootO["currently"]["time"];
  rootN["daily"]["sunriseTime"] = rootO["daily"]["data"][0]["sunriseTime"];
  rootN["daily"]["sunsetTime"] = rootO["daily"]["data"][0]["sunsetTime"];
  for (int i = 0; i < ANALYZEHOURS; i++) {
    rootN["hourly"]["time" + (String)(i)] = rootO["hourly"]["data"][i]["time"];
    rootN["hourly"]["temp" + (String)(i)] = rootO["hourly"]["data"][i]["temperature"];
    rootN["hourly"]["humi" + (String)(i)] = rootO["hourly"]["data"][i]["humidity"];
    rootN["hourly"]["preI" + (String)(i)] = rootO["hourly"]["data"][i]["precipIntensity"];
    rootN["hourly"]["preP" + (String)(i)] = rootO["hourly"]["data"][i]["precipProbability"];
    rootN["hourly"]["winS" + (String)(i)] = rootO["hourly"]["data"][i]["windSpeed"];
    rootN["hourly"]["winB" + (String)(i)] = rootO["hourly"]["data"][i]["windBearing"];
    rootN["hourly"]["cloC" + (String)(i)] = rootO["hourly"]["data"][i]["cloudCover"];
    String stmp1 = rootO["hourly"]["data"][i]["icon"];
    rootN["hourly"]["icon" + (String)(i)] = stmp1;
  }
  String stmp2 = rootO["hourly"]["summary"];
  rootN["hourly"]["summary"] = stmp2;
  String stmp3 = rootO["daily"]["summary"];
  rootN["daily"]["summary"] = stmp3;
  String sAux = "";
  rootN.printTo(sAux);
  //  Serial.print("\nSUMMARIZED\n" + sAux + "\n");
  return sAux;
}//////////////////////////////////////////////////////////////////////////////////////////////////
bool FB_ApplyFunctions() {
  if (!bGetJsonDev()) return false;
  DynamicJsonBuffer jsonBuffer(256);
  JsonObject& root = jsonBuffer.parseObject(sJsonDev);
  if (!root.success()) {
    Serial.print(", FB_ApplyFunctions NO ROOT");
    return false;
  }
  JsonObject& rootS = root["Functions"];
  if (rootS.success()) {
    if (rootS.containsKey("Reboot")) {
      if (root["Functions"]["Reboot"]) {
        root["Functions"]["Reboot"] = "false";
        bUpdateRootJsonDev(root);
        delay(500);
        SendToRestart();
      }
    } else  {
      root["Functions"]["Reboot"] = "false";
      bUpdateRootJsonDev(root);
    }
    if (rootS.containsKey("ToSleep")) {
      if (root["Functions"]["ToSleep"]) {
        root["Functions"]["ToSleep"] = "false";
        bUpdateRootJsonDev(root);
        SendToSleep(0);
      }
    } else {
      root["Functions"]["ToSleep"] = "false";
      bUpdateRootJsonDev(root);
    }
    if (rootS.containsKey("EraseVMaxMin")) {
      if (root["Functions"]["EraseVMaxMin"]) {
        root["Functions"]["EraseVMaxMin"] = "false";
        bUpdateRootJsonDev(root);
        EraseVMaxVmin();
        SendToSleep(0);
      }
    } else       {
      root["Functions"]["EraseVMaxMin"] = "false";
      bUpdateRootJsonDev(root);
    }
    if (rootS.containsKey("FormatSPIFFS")) {
      if (root["Functions"]["FormatSPIFFS"]) {
        root["Functions"]["FormatSPIFFS"] = "false";
        bUpdateRootJsonDev(root);
        FormatSPIFFS();
        SendToSleep(0);
      }
    } else       {
      root["Functions"]["FormatSPIFFS"] = "false";
      bUpdateRootJsonDev(root);
    }
    if (rootS.containsKey("EraseSPIFFJson")) {
      if (root["Functions"]["EraseSPIFFJson"]) {
        root["Functions"]["EraseSPIFFJson"] = "false";
        bUpdateRootJsonDev(root);
        bEraseSPIFFJson = true;
        SendToSleep(0);
      }
    } else       {
      root["Functions"]["EraseSPIFFJson"] = "false";
      bEraseSPIFFJson = false;
      bUpdateRootJsonDev(root);
    }
    if (rootS.containsKey("EraseAllNVS")) {
      if (root["Functions"]["EraseAllNVS"]) {
        root["Functions"]["EraseAllNVS"] = "false";
        bUpdateRootJsonDev(root);
        bEraseSPIFFJson = true;
        SendToSleep(0);
      }
    } else       {
      root["Functions"]["EraseAllNVS"] = "false";
      bUpdateRootJsonDev(root);
      EraseAllNVS();
      SendToRestart();
    }
    if (rootS.containsKey("EraseAllFB")) {
      if (root["Functions"]["EraseAllFB"]) {
        root["Functions"]["EraseAllFB"] = "false";
        bUpdateRootJsonDev(root);
        delay(10);
        Firebase.remove("/dev/" + sMACADDR);
        delay(10);
        SendToSleep(0);
      }
    } else       {
      root["Functions"]["EraseAllFB"] = "false";
      bUpdateRootJsonDev(root);
    }
    if (rootS.containsKey("OTAUpdate")) {
      String sOtaBin = root["Functions"]["OTAUpdate"].as<String>();
      if (sOtaBin.length() > 3) {
        bool bOTAUpdate = true;
        String sSpecialCase = "";
        if (sOtaBin.substring(0, 1) == "[") bOTAUpdate = false;
        else if (sOtaBin.substring(0, 1) == "(") {
          sSpecialCase = sOtaBin.substring(1, 2);
          sOtaBin = sOtaBin.substring(3, sOtaBin.length());
        }
        sOtaBin.trim();
        if (bOTAUpdate) {
          Serial.println("\n  ~OTAUpdate~ " + sOtaBin);
          if (sSpecialCase == "S") {
            bNewPage();
            display.fillScreen(GxEPD_WHITE);
            display.setTextColor(GxEPD_BLACK);
            DisplayU8Text(1, 40, "OTA Update", fU8g2_M);
            DisplayU8Text(1, 80, "[" + sOtaBin + "]", fU8g2_S);
            DisplayU8Text(1, 100, sTimeLocal(0) + " " + sDateLocal(sWeatherLNG), fU8g2_S);
            DisplayU8Text(1, 120, "Reset in 5 mins...", fU8g2_S);
            bRefreshPage();
            sSpecialCase += " ";
          }
          //          NVSClose();
          if (execOTA(sOtaBin)) {
            root["Functions"]["OTAUpdate"] = "[" + sSpecialCase + "OK] " + sOtaBin;
            EraseAllNVS();
            writeSPIFFSFile("/otaupdat.txt", sOtaBin.c_str());
          } else {
            root["Functions"]["OTAUpdate"] = "[" + sSpecialCase + "NOK] " + sOtaBin;
          }
          bUpdateRootJsonDev(root);
          LogAlert("~OTAUpdate DONE " + root["Functions"]["OTAUpdate"].as<String>() + "~ " , 3);
          SendToRestart();
        }
      }
      sOtaBin = "";
    } else {
      root["Functions"]["OTAUpdate"] = "";
      bUpdateRootJsonDev(root);
    }
  } else LogAlert("\n NO root Functions on FB_ApplyFunctions() " , 1);
  return true;
}//////////////////////////////////////////////////////////////////////////////////////////////////
void EraseVMaxVmin() {
  LogAlert("~EraseVMaxMin~", 3);
  tTimeLastVtgMax = 0;
  tTimeLastVtgChg = 0;
  iVtgMax = 0;
  iVtgChg = 0;
  iVtgMin = 10000;
  bSetVtgValues();
  delay(10);
  Firebase.remove("/dev/" + sMACADDR + "/Vtg");
  delay(10);
}//////////////////////////////////////////////////////////////////////////////////////////////////
void FormatSPIFFS() {
  LogAlert("~Format SPIFFS~", 3);
  listSPIFFSDir("/", 2);
  String sWifiTxt = readSPIFFSFile("/wifi.txt");
  Serial.println("----- FORMATING ------");
  bool bRes = SPIFFS.format();
  Serial.println("----- FORMATED " + (String)(bRes ? "true" : "false") + "------");
  if (sWifiTxt.length() > 3) writeSPIFFSFile("/wifi.txt", sWifiTxt.c_str());
  listSPIFFSDir("/", 2);
}//////////////////////////////////////////////////////////////////////////////////////////////////
void EraseAllNVS() {
  LogAlert("~EraseAllNVS~", 2);
  nvs_handle my_nvs_handle;
  delay(100);
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    err = nvs_flash_init();
  }
  ESP_ERROR_CHECK( err );
  err = nvs_open("storage", NVS_READWRITE, &my_nvs_handle);
  nvs_erase_all(my_nvs_handle);
  nvs_commit(my_nvs_handle);
  nvs_close(my_nvs_handle);
  delay(100);
  err = nvs_flash_deinit();
  delay(100);
}//////////////////////////////////////////////////////////////////////////////////////////////////
bool LoadVars_FB_SPIFFS_Mem_Str(String sVarName, String * sVar ) {
  String sAux = "", sValOld = "";
  bool bVarOnRoot = false;
  DynamicJsonBuffer jsonBufferOld(256);
  JsonObject& rootOld = jsonBufferOld.parseObject(sJsonDevOld);
  if (!rootOld.success()) {
    LogAlert(",NoOldRootStr:" + sVarName + ",", 1);
  } else {
    sValOld = rootOld["vars"][sVarName].as<String>();
  }
  bGetJsonDev();
  DynamicJsonBuffer jsonBuffer(256);
  JsonObject& root = jsonBuffer.parseObject(sJsonDev);
  if (!root.success()) {
    LogAlert(", LoadVars_FB_SPIFFS_Mem_Str NO NEW ROOT on var " + sVarName, 2);
    if (sValOld.length() > 0) *sVar = sValOld;
    return (sVar->length() > 0);
  }
  bVarOnRoot = FBCheckroot2ContainsStr(root,  "vars", sVarName, *sVar, false);
  if (bVarOnRoot) {
    sAux = root["vars"][sVarName].as<String>();
    if (sAux != "-") { // FB = "-" to return to defaults
      if (sAux.length() > 0) *sVar = sAux;
      else if (sValOld.length() > 0) *sVar = sValOld;
    }
  } else {
    if (sValOld.length() > 0) *sVar = sValOld;
  }
  return FBUpdate2rootStr(root, "vars", sVarName, *sVar);
}//////////////////////////////////////////////////////////////////////////////////////////////////
bool LoadVars_FB_SPIFFS_Mem_Int(String sVarName, int* iVar ) {
  int iAux = -1, iValOld = -1000;
  bool bVarOnRoot = false;
  DynamicJsonBuffer jsonBufferOld(256);
  JsonObject& rootOld = jsonBufferOld.parseObject(sJsonDevOld);
  if (!rootOld.success()) {
    LogAlert(",NoOldRootInt:" + sVarName + ",", 1);
  } else {
    iValOld = rootOld["vars"][sVarName];
  }
  bGetJsonDev();
  DynamicJsonBuffer jsonBuffer(256);
  JsonObject& root = jsonBuffer.parseObject(sJsonDev);
  if (!root.success()) {
    LogAlert(", LoadVars_FB_SPIFFS_Mem_Int NO NEW ROOT on var " + sVarName, 2);
    if (iValOld > -1000) *iVar = iValOld;
    return (*iVar > -1000);
  }
  bVarOnRoot = FBCheckroot2ContainsFlt(root,  "vars", sVarName, *iVar, false);
  if (bVarOnRoot) {
    iAux = root["vars"][sVarName];
    if (iAux > -1000) *iVar = iAux; // FB = "-1" to return to defaults
  } else {
    if (iValOld > -1000) *iVar = iValOld;
  }
  return FBUpdate2rootFlt(root, "vars", sVarName, *iVar);
}//////////////////////////////////////////////////////////////////////////////////////////////////
bool LoadVars_FB_SPIFFS_Mem_Flt(String sVarName, float * fVar ) {
  int fAux = -1000, fValOld = -1000;
  bool bVarOnRoot = false;
  DynamicJsonBuffer jsonBufferOld(256);
  JsonObject& rootOld = jsonBufferOld.parseObject(sJsonDevOld);
  if (!rootOld.success()) {
    LogAlert(",NoOldRootFlt:" + sVarName + ",", 1);
  } else {
    fValOld = rootOld["vars"][sVarName];
  }
  bGetJsonDev();
  DynamicJsonBuffer jsonBuffer(256);
  JsonObject& root = jsonBuffer.parseObject(sJsonDev);
  if (!root.success()) {
    LogAlert(", LoadVars_FB_SPIFFS_Mem_Int NO NEW ROOT on var " + sVarName, 2);
    if (fValOld > -1000) *fVar = fValOld;
    return (*fVar > -1000);
  }
  bVarOnRoot = FBCheckroot2ContainsFlt(root,  "vars", sVarName, *fVar, false);
  if (bVarOnRoot) {
    fAux = root["vars"][sVarName];
    if (fAux > -1000) *fVar = fAux; // FB = "-1000" to return to defaults
  } else {
    if (fValOld > -1000) *fVar = fValOld;
  }
  return FBUpdate2rootFlt(root, "vars", sVarName, *fVar);
}//////////////////////////////////////////////////////////////////////////////////////////////////
time_t tToNextTime(String sTimeFirst) {
  int iHourF, iMinF, iHourN, iMinN;
  iHourF = sTimeFirst.toInt();
  iMinF = ((sTimeFirst.toFloat() * 100) - (iHourF * 100));
  time_t tDest;
  iHourN = hour(tNow);
  iMinN = minute(tNow);
  tDest = tNow - (iHourN - iHourF) * 3600 - (iMinN - iMinF) * 60 + (60 - second(tNow));
  if (tDest < tNow) tDest = tDest + 86400;
  return tDest;
}//////////////////////////////////////////////////////////////////////////////////////////////////
float fGetTempTime(long int t) {
  int iIni = -1;
  Serial.printf("\n dGetTempTime %d>%d>%d (%f>%f>%f) ", t, aHour[0], aHour[ANALYZEHOURS - 1], fCurrTemp, aTempH[0], aTempH[ANALYZEHOURS - 1]);
  if (aHour[0] > t) return aTempH[0];
  if (t > aHour[ANALYZEHOURS]) return aTempH[ANALYZEHOURS - 1];
  for (int i = 0; i < (ANALYZEHOURS - 1); i++) {
    if (t >= aHour[i]) {
      iIni = i;
      break;
    }
  }
  if (iIni > -1) {
    float val;
    val = aTempH[iIni] + ((t - aHour[iIni]) * (aTempH[iIni + 1] - aTempH[iIni])) / (aHour[iIni + 1] - aHour[iIni]);
    return val;
  } else return -100;
}//////////////////////////////////////////////////////////////////////////////////////////////////
float dGetWindSpdTime(long int t) {
  int iIni = -1;
  if (aHour[0] > t) return aWindSpd[0];
  if (t > aHour[ANALYZEHOURS]) return aWindSpd[ANALYZEHOURS - 1];
  for (int i = 0; i < (ANALYZEHOURS - 1); i++) {
    if (t >= aHour[i]) {
      iIni = i;
      break;
    }
  }
  if (iIni > -1) {
    float val;
    val = aWindSpd[iIni] + ((t - aHour[iIni]) * (aWindSpd[iIni + 1] - aWindSpd[iIni])) / (aHour[iIni + 1] - aHour[iIni]);
    return val;
  } else return -1;
}//////////////////////////////////////////////////////////////////////////////////////////////////
float dGetWindBrnTime(long int t) {
  int iIni = -1;
  if (aHour[0] > t) return aWindBrn[0];
  if (t > aHour[ANALYZEHOURS]) return aWindBrn[ANALYZEHOURS - 1];
  for (int i = 0; i < (ANALYZEHOURS - 1); i++) {
    if (t >= aHour[i]) {
      iIni = i;
      break;
    }
  }
  if (iIni > -1) {
    float val;
    val = aWindBrn[iIni] + ((t - aHour[iIni]) * (aWindBrn[iIni + 1] - aWindBrn[iIni])) / (aHour[iIni + 1] - aHour[iIni]);
    return val;
  } else return -1;
}//////////////////////////////////////////////////////////////////////////////////////////////////
bool bNewPage() {
  return true;
}//////////////////////////////////////////////////////////////////////////////////////////////////
bool bRefreshPage() {
  display.update();
  return true;
}/////////////////////////////////////////////////////////////////////////////////////////////////
bool bPartialDisplayUpdate() {
  display.updateWindow(0, 0, GxEPD_WIDTH, GxEPD_HEIGHT, false);
  return true;
}//////////////////////////////////////////////////////////////////////////////////////////////////
String sPlatform() {
  String sPlatf = "";
  if (bWS29) sPlatf = "2";
  if (bWS42) sPlatf = "4";
  if (bWS58) sPlatf = "5";
  if (bWS75) sPlatf = "7";
#if defined(TTGOT5)
  sPlatf = "TTG";
#endif
#if defined(WS7c) || defined(WS4c)
  bRed = true;
#else
  bRed = false;
#endif
  if (bRed) sPlatf += "C";
  else sPlatf += "B";
  sPlatf += "1";
  if (bClk)  sPlatf += "_CLK";
  return sPlatf;
}//////////////////////////////////////////////////////////////////////////////////////////////////
bool OTADefault() {
  String sOTAName = "WD_default_" + sPlatform() + ".bin";
  bNewPage();
  display.fillScreen(GxEPD_WHITE);
  display.setTextColor(GxEPD_BLACK);
  DisplayU8Text(1, 40, "OTA Update to DEFAULTS", fU8g2_M);
  DisplayU8Text(1, 80, "[" + sOTAName + "]", fU8g2_S);
  DisplayU8Text(1, 100, sTimeLocal(0) + " " + sDateLocal(sWeatherLNG), fU8g2_S);
  DisplayU8Text(1, 120, "Reset in 5 mins...", fU8g2_S);
  bRefreshPage();
  LogAlert("OTA Default " + sOTAName, 3);
  bool res = execOTA(sOTAName);
  EraseAllNVS();
  writeSPIFFSFile("/otaupdat.txt", sOTAName.c_str());
  return res;
}//////////////////////////////////////////////////////////////////////////////////////////////////
bool LogAlert(String sText, int iLevel) {
  if (iLevel > 1)  Serial.println("\n" + sText);
  else Serial.print(", " + sText);
  time_t tAlert = time(nullptr);
  bLogUpdateNeeded = true;
  if (bWeHaveWifi) return WriteLog(tAlert, sText, iLevel);
  else return LogDef(sText, iLevel);
}//////////////////////////////////////////////////////////////////////////////////////////////////
bool LogDef(String sText, int iLevel) {
  if (iLevel > 1)  Serial.println("\n" + sText);
  else Serial.print(", " + sText);
  time_t tAlert;
  if (time(nullptr) > 60) tAlert = time(nullptr);
  else tAlert = tNow + (millis() / 1000);
  String sAux = "[" + (String)(tAlert) + "]" + (String)(iLevel) + sText + (String)(char(239));
  if ((sDefLog.length() + sAux.length()) < 4000)  sDefLog += sAux;
  bLogUpdateNeeded = true;
  return true;
}//////////////////////////////////////////////////////////////////////////////////////////////////
bool LoadDefLog() {
  if (SPIFFS.exists("/deflog.txt")) {
    sDefLog = readSPIFFSFile("/deflog.txt");
    Serial.print("DL(" + (String)(sDefLog.length()) + ")");
    return true;
  }
  return false;
}//////////////////////////////////////////////////////////////////////////////////////////////////
bool FlushDefLog() {
  if (sDefLog.length()) {
    if (bGetJsonDev()) {
      String sLim = (String)(char(239)), sAux, sText;
      int iPosL, iPosT, iLevel;
      time_t tLog;
      iPosL = sDefLog.indexOf(sLim);
      while (iPosL > 0) {
        sAux = sDefLog.substring(0, iPosL);
        //        Serial.print("\nDEFFERED LOG RECEIVED{" + sAux + "}");
        iPosT = sDefLog.indexOf(']');
        sAux = sDefLog.substring(1, iPosT);
        tLog = atol(sAux.c_str());
        //        Serial.print(", LOGTIME{" + sAux + "}");
        sAux = sDefLog.substring(iPosT + 1, iPosT + 2);
        //        Serial.print(", LOGLVL{" + sAux + "}");
        iLevel = atoi(sAux.c_str());
        sText = sDefLog.substring(iPosT + 2, iPosL);
        //        Serial.print(", LOGTXT{" + sText + "}");
        WriteLog( tLog,  "*" + sText,  iLevel);
        sDefLog = sDefLog.substring(iPosL + 1, sDefLog.length());
        iPosL = sDefLog.indexOf(sLim);
      }
      sDefLog = "";
    }
    Serial.print(", DEFUL(" + (String)(sDefLog.length()) + ")");
    writeSPIFFSFile("/deflog.txt", sDefLog.c_str());
  }
  bLogUpdateNeeded = false;
  return true;
}//////////////////////////////////////////////////////////////////////////////////////////////////
bool WriteLog(time_t tAlert, String sText, int iLevel) {
  if (bGetJsonDev()) {
    if (iLevel >= iLogFlag ) {
      DynamicJsonBuffer jsonBuffer(256);
      JsonObject& root = jsonBuffer.parseObject(sJsonDev);
      FBrootCheckSubPath(root, "Log" + (String)(iLevel));
      char buff[30];
      sprintf(buff, "%04d%02d%02d_%02d%02d%02d", year(tAlert), month(tAlert), day(tAlert), hour(tAlert), minute(tAlert), second(tAlert));
      String sAux = buff;
      while (FBCheckroot2ContainsStr(root, "Log" + (String)(iLevel), sAux, "", false)) {
        tAlert++;
        sprintf(buff, "%04d%02d%02d_%02d%02d%02d", year(tAlert), month(tAlert), day(tAlert), hour(tAlert), minute(tAlert), second(tAlert));
        sAux = buff;
        Serial.print("_WLJ_");
      }
      root["Log" + (String)(iLevel)][(String)(buff)] =  sText;
      bUpdateRootJsonDev(root);
    }
    if ((iLevel > 2) && (bWeHaveWifi)) {
      SendEmail("WESP32> " + sDevID + " [" + String(iLevel) + ":" +  sText + "]" , sJsonDev);
    }
  }
  bLogUpdateNeeded = false;
  return true;
}//////////////////////////////////////////////////////////////////////////////////////////////////
bool FBCheckLogLength() {
  DynamicJsonBuffer jsonBuffer(256);
  JsonObject& root = jsonBuffer.parseObject(sJsonDev);
  if (!root.success()) return false;
  JsonObject& rootLog1 = root["Log1"];
  JsonObject& rootLog2 = root["Log2"];
  JsonObject& rootLog3 = root["Log3"];
  int iLength, iLengthini;
  iLength = rootLog1.measureLength() + rootLog2.measureLength() + rootLog3.measureLength();
  if (iLength < iLogMaxSize) return true;
  iLengthini = iLength;
  for (const JsonPair& pair : rootLog1) {
    rootLog1.remove(pair.key);
    iLength = rootLog1.measureLength() + rootLog2.measureLength() + rootLog3.measureLength();
    if (iLength < iLogMaxSize) break;
  }
  if (iLength > iLogMaxSize) {
    for (const JsonPair& pair : rootLog2) {
      rootLog2.remove(pair.key);
      iLength = rootLog1.measureLength() + rootLog2.measureLength() + rootLog3.measureLength();
      if (iLength < iLogMaxSize) break;
    }
  }
  if (iLength > iLogMaxSize) {
    for (const JsonPair& pair : rootLog3) {
      rootLog3.remove(pair.key);
      iLength = rootLog1.measureLength() + rootLog2.measureLength() + rootLog3.measureLength();
      if (iLength < iLogMaxSize) break;
    }
  }
  bUpdateRootJsonDev(root);
  //  LogAlert("Log reduced " + (String)(iLengthini) + "->" + (String)(iLength), 2);
  return true;
}//////////////////////////////////////////////////////////////////////////////
bool bGetFBVars() {
  LoadVars_FB_SPIFFS_Mem_Str("Id", &sDevID);
  LoadVars_FB_SPIFFS_Mem_Str("CustomText", &sCustomText);
  LoadVars_FB_SPIFFS_Mem_Str("TimeFirst", &sTimeFirst);
  LoadVars_FB_SPIFFS_Mem_Str("WeatherLOC", &sWeatherLOC);
  LoadVars_FB_SPIFFS_Mem_Str("WeatherAPI", &sWeatherAPI);
  LoadVars_FB_SPIFFS_Mem_Str("WeatherLNG", &sWeatherLNG);
  LoadVars_FB_SPIFFS_Mem_Int("RefreshPeriod", &iRefreshPeriod);
  LoadVars_FB_SPIFFS_Mem_Int("LogMaxSize", &iLogMaxSize);
  LoadVars_FB_SPIFFS_Mem_Int("LogFlag", &iLogFlag);
  LoadVars_FB_SPIFFS_Mem_Flt("TempInOffset", &fTempInOffset);
  if (io_LED)  LoadVars_FB_SPIFFS_Mem_Int("LedLevel", &iLedLevel);
  return true;
}//////////////////////////////////////////////////////////////////////////////
String sGetResetReason() {
  String ret = "";
  switch (rtc_get_reset_reason(0)) {
    case 1   : ret = "PON"; break;
    case 5   : ret = "SLP"; break;
    case 7   : ret = "WD0"; break;
    case 8   : ret = "WD1"; break;
    case 12  : ret = "RST"; break;
    case 13  : ret = "BUT"; break;
    case 16  : ret = "RTC"; break;
    default:    ret = (String)(rtc_get_reset_reason(0));
  }
  return ret;
}//////////////////////////////////////////////////////////////////////////////
int iMintoNextWake(time_t tN) {
  time_t tFirst = tToNextTime(sTimeFirst);
  if (!tN) return -1;
  int ret = 0;
  int iDiff = tFirst - tN;
  int iWakes = iDiff / (60 * iRefreshPeriod);
  int iRemainMins = round((iDiff - (iWakes * 60 * iRefreshPeriod)) / 60);
  if (iRemainMins > 15)     ret = iRemainMins;
  else     ret = iRemainMins + iRefreshPeriod;
  if (((hour(tN) == 23) || (6 > hour(tN)))) {
    if (iDiff > (2 * 60 * iRefreshPeriod))       ret += iRefreshPeriod;
    else     if (iDiff > (90 * iRefreshPeriod))         ret = round(iDiff / 120);
    else         ret = round(iDiff / 60);
  }
  return (ret + 1);
}//////////////////////////////////////////////////////////////////////////////
bool  bGetVtgValues(String sJson) {
  if (!sJson.length()) {
    if (bGetJsonDev()) sJson = sJsonDev;
    else {
      LogAlert("No Json on bGetVtgValues", 2);
      return false;
    }
  }
  int iAux;
  DynamicJsonBuffer jsonBuffer(256);
  JsonObject& root = jsonBuffer.parseObject(sJson);
  iAux = root["Vtg"]["VMax"];
  if (iAux > 0) iVtgMax = iAux;
  if (iVtgMax > VTGMAXIMALLOWED) iVtgMax = 0;
  iAux = root["Vtg"]["VChg"];
  if (iAux > 0) iVtgChg = iAux;
  if (iVtgChg > VTGMAXIMALLOWED) iVtgChg = 0;
  iAux = root["Vtg"]["VMin"];
  if (iAux > 0) iVtgMin = iAux;
  else     iVtgMin = 10000;
  tTimeLastVtgMax = root["Vtg"]["tLstMax"];
  tTimeLastVtgChg = root["Vtg"]["tLstChg"];
  tTimeLastVtgDown = root["Vtg"]["tLstDown"];
  iVtgHPeriodMax = root["Vtg"]["PeriodMax"];
  if (iVtgHPeriodMax < 1) iVtgHPeriodMax = 1;
  iVtgPeriodDrop = root["Vtg"]["PeriodDrop"];
  if (iVtgPeriodDrop < 1) iVtgPeriodDrop = 1;
  iVtgStableMax = root["Vtg"]["StableMax"];
  if (1 > iVtgStableMax ) iVtgStableMax = iVtgMax * .9;
  iVtgStableMin = root["Vtg"]["StableMin"];
  if (1 > iVtgStableMin) {
    if ((iVtgMin > 1) && (iVtgMax > 1)) iVtgStableMin = (iVtgMax + iVtgMin) / 2;
    else     iVtgStableMin = 3000;
  }
  return true;
}//////////////////////////////////////////////////////////////////////////////
bool  bSetVtgValues() {
  if (bGetJsonDev()) {
    DynamicJsonBuffer jsonBuffer(256);
    JsonObject& root = jsonBuffer.parseObject(sJsonDev);
    root["Vtg"]["VMax"] = iVtgMax;
    root["Vtg"]["VChg"] = iVtgChg ;
    root["Vtg"]["VMin"] = iVtgMin;
    root["Vtg"]["tLstMax"] = tTimeLastVtgMax ;
    root["Vtg"]["tLstChg"] = tTimeLastVtgChg ;
    root["Vtg"]["tLstDown"] = tTimeLastVtgDown ;
    root["Vtg"]["PeriodMax"] = iVtgHPeriodMax;
    root["Vtg"]["PeriodDrop"] = iVtgPeriodDrop;
    root["Vtg"]["StableMax"] = iVtgStableMax ;
    root["Vtg"]["StableMin"] = iVtgStableMin ;
    bUpdateRootJsonDev(root);
    return true;
  } else {
    LogAlert("No Json on bSetVtgValues", 2);
    return false;
  }
}//////////////////////////////////////////////////////////////////////////////
int iWriteSlopesSPIFFS(int32_t* iVtgVal, int32_t* tVtgTime, int iVTGMEASSURES) {
  String sName;
  int zeros = 0;
  DynamicJsonBuffer jsonBuffer(256);
  JsonObject& root = jsonBuffer.parseObject(sJsonDev);
  for (int i = 0; i < iVTGMEASSURES; i++) {
    sName = "VtgVal" + (String)(int)(i);
    root["Vtg"]["VHist"][sName] = iVtgVal[i];
    sName = "VtgTime" + (String)(int)(i);
    root["Vtg"]["VHist"][sName] = tVtgTime[i];
    if (tVtgTime[i] == 0)       zeros++;
  }
  bUpdateRootJsonDev(root);
  return zeros;
}//////////////////////////////////////////////////////////////////////////////
int iLoadSlopesSPIFFS(int32_t* iVtgVal, int32_t* tVtgTime, int iVTGMEASSURES) {
  String sName;
  int zeros = 0;
  DynamicJsonBuffer jsonBuffer(256);
  JsonObject& root = jsonBuffer.parseObject(sJsonDev);
  for (int i = 0; i < iVTGMEASSURES; i++) {
    sName = "VtgVal" + (String)(int)(i);
    iVtgVal[i] = root["Vtg"]["VHist"][sName];
    sName = "VtgTime" + (String)(int)(i);
    tVtgTime[i] = root["Vtg"]["VHist"][sName];
    if (tVtgTime[i] == 0)       zeros++;
  }
  return zeros;
}//////////////////////////////////////////////////////////////////////////////
bool bCheckAWSSPIFFSFiles() {
  if (!SPIFFS.exists("/wifi.txt"))  writeSPIFFSFile("/wifi.txt", sWifiDefaultJson.c_str());
  if (!SPIFFS.exists("/edit.htm"))  DowloadFromAWSToSpiffs("data/edit.htm", "/edit.htm");
  if (!SPIFFS.exists("/ace.js"))    DowloadFromAWSToSpiffs("data/ace.js", "/ace.js");
  if (!SPIFFS.exists("/mode-html.js"))  DowloadFromAWSToSpiffs("data/mode-html.js", "/mode-html.js");
}//////////////////////////////////////////////////////////////////////////////
bool bCheckInternalTemp() {
  int iSensorType = -1;
  float fFactor = 1;
  Serial.print(" ~ Internal Temp:" );
  delay(10);
  bInsideTempSensor = false;
  if (io_DS18B20) {
    OneWire oneWireObject(io_DS18B20);
    DallasTemperature sensorDS18B20(&oneWireObject);
    sensorDS18B20.begin();
    sensorDS18B20.requestTemperatures();
    fInsideTemp = sensorDS18B20.getTempCByIndex(0);
    if (fInsideTemp > -99) {
      iSensorType = 1;
      bInsideTempSensor = true;
    }
  }
  if (!bInsideTempSensor && io_TMP36) {
    int iAux = 0;
    for (int i = 0; i < 10; i++) {
      delay(20);
      iAux = iAux + analogRead(io_TMP36);
    }
    delay(20);
    iAux = iAux / 10;
    fInsideTemp = ((float(iAux) * 3.3 / 1024.0) - 0.5) * 10;
    // FACTOR
    Serial.printf("[%d/%d]", iVtgVal[VTGMEASSURES - 1], iVtgMax);
    delay(10);
    if (fInsideTemp != -5.00) {
      iSensorType = 2;
      bInsideTempSensor = true;
      if ((iVtgVal[VTGMEASSURES - 1] > 0) && (iVtgMax > 0)) {
        fFactor = (float)(iVtgMax) / (float)(iVtgVal[VTGMEASSURES - 1]);
        if (fFactor > 1)      fInsideTemp = fInsideTemp * fFactor;
      }
    }
  }
  Serial.print((String)((iSensorType != -1) ? ((String)((iSensorType == 1) ? "DS18B20=" : "TMP36=") + (String)(fInsideTemp)) : "NO SENSOR ") + "ºC ~ [vFACTOR:" + (String)(fFactor) + "]\n");
  if (abs(fInsideTemp) > 50) bInsideTempSensor = false;
  return bInsideTempSensor;
}//////////////////////////////////////////////////////////////////////////////////////////////////
void CheckLed() {
  bool bOn = false;
  static bool bLastOn;
  if ((bClk) && (io_LED) && (tNow) && (iLedLevel)) {
    Serial.print("-LED(" + (String)(bLastOn ? "T" : "F") + "->");
    bOn = ((tNow < (tSunrise + 600)) || (tNow > (tSunset - 600)));
    Serial.print((String)(bOn ? "T" : "F") + ")");
    if (bOn != bLastOn) {
      if (bOn) ledcWrite(0, iLedLevel);
      else ledcWrite(0, 0);
      Serial.print(",CHG->" + (String)(bOn ? "T" : "F") + "-");
    } else       Serial.print(",NO_CHG-");
    bLastOn = bOn;
  }
}//////////////////////////////////////////////////////////////////////////////////////////////////
int iBattPercentage(int iVtg) {
  int iAuxVStableMax, iAuxVStableMin, iBattPerc;
  if (10 > iVtgStableMax) iAuxVStableMax = iVtgMax * .97;
  else iAuxVStableMax = iVtgStableMax;
  if (10 > iVtgStableMin) iAuxVStableMin = iVtgMax * .88;
  else iAuxVStableMin = iVtgStableMin;
  if (iAuxVStableMax > iAuxVStableMin)  iBattPerc = 100 * (iVtg - iAuxVStableMin) / (iAuxVStableMax - iAuxVStableMin) ; // 100% @ 96%Vtg and 0% @ 88%Vtg
  else iBattPerc = 0;
  if (iBattPerc > 100) iBattPerc = 100;
  if (iBattPerc < 0) iBattPerc = 0;
  return iBattPerc;
}//////////////////////////////////////////////////////////////////////////////////////////////////
int iBattPercentageCurve(int iVtg) { // Polynomial from David Bird
  uint8_t iBattPerc = 100;
  float voltage = 4.20 * iVtg / iVtgMax;
  iBattPerc = 2808.3808 * pow(voltage, 4) - 43560.9157 * pow(voltage, 3) + 252848.5888 * pow(voltage, 2) - 650767.4615 * voltage + 626532.5703;
  if (iBattPerc > 100) iBattPerc = 100;
  if (iBattPerc < 0) iBattPerc = 0;
  return iBattPerc;
}//////////////////////////////////////////////////////////////////////////////////////////////////
int iRemDaysTime() {
  int iAuxVStableMax, iAuxVStableMin;//, iAuxVPeriodDrop;
  if (iVtgPeriodDrop < 1) iVtgPeriodDrop = 1;
  if (10 > iVtgStableMax) iAuxVStableMax = iVtgMax * .96;
  else iAuxVStableMax = iVtgStableMax;
  if (10 > iVtgStableMin) iAuxVStableMin = iVtgMax * .88;
  else iAuxVStableMin = iVtgStableMin;
  //  iAuxVPeriodDrop = iVtgPeriodDrop - (iVtgStableMin - iVtgMin);
  float fChgMax;
  if (iVtgPeriodDrop) fChgMax = (float)(iVtgChg - iAuxVStableMin) / (float)(iVtgPeriodDrop); // last 15% is the drop
  else fChgMax = 0;
  float fEnd = tTimeLastVtgChg + (fChgMax * iVtgHPeriodMax * 3600);
  float fRem = fEnd - tNow;
  float fPerc ;
  if (fEnd > tTimeLastVtgChg) fPerc = fRem / (fEnd - tTimeLastVtgChg);
  else fPerc = 0;
  if (fRem < 0) fRem = 0;
  //  if (fRem>100) fRem=99;
  //  Serial.printf("\n iRemDaysTime: iBatStat=%d, (%d,%d) iVSMax=%d,iVSMin=%d iAuxPeriodDrop=%d fChgMax=%f, fEnd=%f, fRem=%f, fPerc=%f\n",iBattStatus,iVtgStableMax,iVtgStableMin,iAuxVStableMax,iAuxVStableMin,iAuxVPeriodDrop,fChgMax,fEnd,fRem,fPerc);
  return (int)(fRem / 86400);
}//////////////////////////////////////////////////////////////////////////////
void test() {

}
//////////////////////////////////////////////////////////////////////////////
void SendEmail(String sSubject, String sText) {
#ifdef G_SENDER
  if (strlen(sFROMEMAIL) > 0) {
    Gsender *gsender = Gsender::Instance();    // Getting pointer to class instance
    gsender->SetParams(sEMAILBASE64_LOGIN, sEMAILBASE64_PASSWORD, sFROMEMAIL );
    if (gsender->Subject(sSubject)->Send(sEmailDest, sText)) {
      Serial.println("Message send.");
    } else {
      Serial.print("Error sending message: ");
      Serial.println(gsender->getError());
    }
  }
#endif
}
//////////////////////////////////////////////////////////////////////////////
bool bCheckLowVoltage () {
  String sLowVolt = readSPIFFSFile("/lowvol.txt");
  int iLowVolt = sLowVolt.toInt();
  if (iLowVolt > 1) {
    if (analogRead(io_VOLTAGE) > (iLowVolt * 1.2)) {
      deleteSPIFFSFile("/lowvol.txt");
      SendToRestart();
    } else {
      SendToSleep(60);
    }
  }
  return true;
}
