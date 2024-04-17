/******************************************************************************
  Copyright 2018. Used external Libraries (many thanks to the authors for their great job):
  Credit to included Arduino libraries; ArduinoJson (Benoit Blanchon 5.13.x max), OneWire, DallasTemperature, Adafruit Gfx (1.11.3), u8glib (Oliver Kraus), GxEPD (Jean-Marc Zingg), FirebaseESP32 (Mobitz)
  As libraries are getting bigger, a bigger app partition is recommended. Also recommended avoiding esp32 1.0.5 library.
  *****************************************************************************/
#define CONFIG_ESP32_WIFI_NVS_ENABLED 0 //trying to prevent NVS + OTA corruptions
#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <SPI.h>
#include <rom/rtc.h>
#include <esp_wifi.h>
#include <esp_sleep.h>
#include <driver/adc.h>
#include <SPIFFS.h>
#include <nvs_flash.h>
#include <ArduinoJson.h>              // Max 5.13.x as 6.x won't fit on RAM while deserializing Weather
#include <FirebaseESP32.h>            // avoid >3.8.8.
#include <GxEPD.h>                    // 3.1.3 works, but avoid indirect install of Adafruit BusIO >1.11.0
#include <GxIO/GxIO_SPI/GxIO_SPI.h>
#include <GxIO/GxIO.h>
#include "additions/U8G2_FONTS_GFX.h" // Install u8G2_for_Adafruit_GFX 1.8.0, and Adafruit_GFX 1.11.3
#include "WeatherIcons.h"
#include "ESP32_Disp_Aux.h"
#include "WDWebServer.h"              // Comment if you don't use the internal web server option and need extra space
#include <DallasTemperature.h>        // Comment if you don't use an internal DS18B20 temp sensor and need extra space
#include "Gsender.h"                  // by Boris Shobat! (comment if you don't want to receive event notifications via email) and need extra space

static const char REVISION[] = "2.49";

//SELECT DISPLAY
#define WS4c      //WS + 2,4,4c,5,7,7c or TTGOT5     <- edit for the Waveshare or Goodisplay hardware of your choice

//SELECT BOARD
#define LOLIN32  //BOARD LOLIN32, TTGOT7 or TTGOT5

#include "config.h"                   // Don't forget to edit this file in order to work!.
/////////////////////////////////////////////////////////////////////
//Conditions
#define ANALYZEHOURS 48
#define PERIODBETWEENVTGS 300
#define VTGMAXIMALLOWED 4095
#define MINBATTFACTOR 0.8
#define FORMAT_SPIFFS_IF_FAILED true
#define LED_LIGHT_LEVEL 100
const char compile_date[] = __DATE__; //" " __TIME__;
float aTempH[ANALYZEHOURS], aFeelT[ANALYZEHOURS], aPrecip[ANALYZEHOURS], aPrecipProb[ANALYZEHOURS], aCloudCover[ANALYZEHOURS], aWindSpd[ANALYZEHOURS], aWindBrn[ANALYZEHOURS], fCurrTemp, fInsideTemp = -100, fTempInOffset = 0, aDTMin[8], aDTMax[8];
String sWifiSsid = "", sWifiPassword = "", sMACADDR, sRESETREASON, aIcon[ANALYZEHOURS], dIcon[10], sSummaryDay, sSummaryWeek, sCustomText, sDevID, sLastWe = "", sWifiIP = "", sWeatherURL =  "https://api.darksky.net/forecast/", sWeatherFIO =  "api.darksky.net", sBattMsg = "";
int32_t  aHour[ANALYZEHOURS], aHumid[ANALYZEHOURS], tSunrise, tSunset, tTimeLastVtgMax, tTimeLastVtgChg, tTimeLastVtgDown, tLastBoot, iVtgMax, iVtgChg, iVtgMin, iVtgPeriodMax, iVtgPeriodDrop, iVtgStableMax, iVtgStableMin, iDailyDisplay, iLastVtgNotWritten = 0, iBattStatus = 0, iRefreshPeriod = 60, iLogMaxSize = 128, iLogFlag = 2, iScreenXMax, iScreenYMax, iSPIFFSWifiSSIDs = -1, iLedLevel = 0, iWifiRSSI,  timeUploaded, iJDevSize = 0;
bool bGettingRawVoltage = false, bClk = false, bResetBtnPressed = false, bInsideTempSensor = false, bHasBattery = false, bRed, bWeHaveWifi = false, bSPIFFSExists, bFBDownloaded = false, bEraseSPIFFJson = false, bFBAvailable = false, bFBDevAvail = false, bFBLoadedFromSPIFFS = false ;
const char* sBattStatus[5] = {"----", "CHGN", "FULL", "DSCH", "EMPT"};
unsigned long iStartMillis;

GxIO_Class io(SPI, io_CS, io_DC, io_RST);
GxEPD_Class display(io, io_RST, io_BUSY);
U8G2_FONTS_GFX u8g2Fonts(display);

FirebaseData firebaseData;
FirebaseJson jVars, jFunc, jLog, jMisc, jVtg, jDefLog;
bool bUVars = false, bUFunc = false, bULog = false, bUMisc = false, bUVtg = false, bUDev = false, bUDefLog = false;
bool bDVars = false, bDFunc = false, bDLog = false, bDMisc = false, bDVtg = false, bDDev = false;

RTC_DATA_ATTR bool bFBModified;
RTC_DATA_ATTR int32_t iBootWOWifi, tNow, tFirstBoot, tLastFBUpdate, tLastSPIFFSWeather, lSecsOn, lBoots, iVtgVal[VTGMEASSURES], tVtgTime[VTGMEASSURES], iShortboots;

String sTimeLocal(time_t local, bool bZeros = true);
bool bFBGetJson(FirebaseJson & jJson, String sPath, bool bClear = true);
bool bFBGetStr(String & sData, String sPath, bool bCreate = true);
bool bFBGetInt(int * iData, String sPath, bool bCreate = true);
bool bFBGetFlt(float * fData, String sPath, bool bCreate = true);
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
//////////////////////////loop ////////////////////////////////////
void loop() {
#ifdef ForceClock
  //testIni ();
  int i1, i2, i3, iTemp;
  static int tLastWifi = tNow;
  static int tLastNigthReboot = tNow;
  String sAux;
  if (((tNow - tLastNigthReboot) > 86000) && (hour(tNow) >= 4) && (hour(tNow) < 6)) { //Clean one minute all nights fron 04:00 to 05:00
    //    if ((io_LED) && (iLedLevel)) ledcWrite(0, 0);
    delay(10000);
    display.fillScreen(GxEPD_WHITE);
    display.update();
    delay(10000);
    display.fillScreen(GxEPD_BLACK);
    display.update();
    delay(10000);
    display.fillScreen(GxEPD_WHITE);
    display.update();
    delay(10000);
    tLastNigthReboot = tNow;
    //    SendToSleep(1);
  }
  if (!bClk) SendToSleep(1);
  if ((ESP.getFreeHeap() / 1024) < 100) SendToRestart(); //something is wrong if heap < 100kB
  tNow = time(nullptr) ;
  Serial.println(" -- CLOCK refresh " + (String)(ESP.getFreeHeap() / 1024) + "kB " + sGetDateTimeStr(tNow) + " --");
  if (iLedLevel) CheckLed();
  iTemp = round(fGetTempTime(tNow));
  if ((-40 < iTemp) && (iTemp < 50))  fCurrTemp = iTemp;
  bCheckInternalTemp();
  if (iRefreshPeriod) iTemp = iRefreshPeriod; else iTemp = 60;
  i1 = (int)((tNow - tLastFBUpdate) / iTemp - 1) ;
  i2 = round(iTemp / 3 );
  i3 = i1 % i2;
  if (i3 == 0) {
    Serial.println(" -- Clock FIREBASE data refresh --");
    if (WiFi.status() != WL_CONNECTED) {
      if (!StartWiFi(30)) LogDef("No Wifi @3 ", 2);
    }
    if (bWeHaveWifi) {
      tLastWifi = tNow;
      Serial.print("_Wifi_");
      /////////////////////////////////////
      jMisc.clear();
      iSetJsonMisc();
      if (bHasBattery) {
        jVtg.clear();
        dGetVoltagePerc();
        bSetJsonVtg();
        bUVtg = true;
      }
      bDVars = bFBGetJson(jVars, "/dev/" + sMACADDR + "/vars");
      bGetJsonVars();
      bDFunc = bFBGetJson(jFunc, "/dev/" + sMACADDR + "/Functions");
      iGetJsonFunctions();

      sAux = sGetJsonString(jFunc, "OTAUpdate", "[]");
      Firebase.deleteNode(firebaseData, "/dev/" + sMACADDR + "/Functions");
      jFunc.clear();
      jFunc.set("OTAUpdate", sAux);
      iGetJsonFunctions();
      bUFunc = true;

      FBCheckLogLength();
      bFBCheckUpdateJsons();
      /////////////////////////////////////
    } /*else {
      if ((tNow - tLastWifi) > 144000) {
        SendToRestart();
      }
    }*/
    bGetWeatherForecast();
    WiFi.disconnect();
  }
  if (bHasBattery) {
    dGetVoltagePerc();
    bSetJsonVtg();
  }
  bNewPage();
  display.fillScreen(GxEPD_WHITE);
  DisplayForecast();
  delay(1000);
  int iHour = hour(tNow);
  if ((iHour > 1) && (iHour < 5)) {
    int iMinute = 5 - (minute(tNow) % 5);
    if (iMinute < 1) iMinute = 5;
    delay(2000);
    bRefreshPage();     //From 2 to 4 perform a full update
    Serial.println(" & full updated...");
    delay(60000 * iMinute);
  }  else {
    if (bWS29) {
      if (minute(tNow) > 0) {
        delay(1000);
        bPartialDisplayUpdate();
      } else  {
        delay(1000);
        bRefreshPage();
      }
    }
    Serial.println(" & partial updated...");
    tNow = time(nullptr);
    delay(1000 * (65 - second(tNow)));
  }
#endif
}
//////////////////////////FUNCTIONS ////////////////////////////////////
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
  /*
    esp_bluedroid_disable();
    esp_bt_controller_disable();
  */
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);
  pinMode (5, OUTPUT);
  digitalWrite (5, HIGH); //Switch off LOLIN32 Blue LED
  if (io_VOLTAGE) adcAttachPin(io_VOLTAGE);
  if (io_VOLTAGE) bHasBattery = (analogRead(io_VOLTAGE) > 200);
  else bHasBattery = false;
#ifdef ForceClock
  if ((bClk) && (io_LED)) {
    ledcSetup(0, 2000, 8);
    ledcAttachPin(io_LED, 0);
    ledcWriteTone(0, 2000);
    ledcWrite(0, 0);
  }
#endif
  Serial.begin(115200);
  sMACADDR = getMacAddress();
  if (sDevID == "") sDevID = sMACADDR ;
  sRESETREASON = sGetResetReason();
  bResetBtnPressed = (sRESETREASON == "PON") || (sRESETREASON == "RTC");
  String sPlatf = sPlatform();
  bInsideTempSensor = bCheckInternalTemp();
  sAux1 = (tNow ? sGetDateTimeStr(tNow) : "NO TIME");
  Serial.println("\n\n\n-------------------------------------------------------");
  Serial.printf( "   2Day_Forecast %s of %s boot %s @%s\n", REVISION, __DATE__ , sRESETREASON.c_str(), sAux1.c_str()  );
  Serial.println("   MAC=" + sMACADDR + " is " + sPlatf + (String)(bClk ? " CLOCK" : " NO-CLOCK"));
  Serial.printf( "   Heap T/F/A=%d/%d/%dkB PSRAM T/F/A=%d/%d/%dkB  \n", ESP.getHeapSize() / 1024, ESP.getFreeHeap() / 1024, ESP.getMaxAllocHeap() / 1024, ESP.getPsramSize() / 1024 , ESP.getFreePsram() / 1024, ESP.getMaxAllocPsram() / 1024);//, esp_himem_get_phys_size() / 1024, esp_himem_get_free_size() / 1024);
  Serial.println("   cpu:" + (String)(temperatureRead()) + "ºC Hall:" + (String)(hallRead()) + (bInsideTempSensor ? " In:" + (String)(fInsideTemp + fTempInOffset) + "ºC" : " NO-TEMP ") + " Vtg:" + (String)(bHasBattery ? analogRead(io_VOLTAGE) : 0));
  Serial.println("-------------------------------------------------------");
  if ((sTimeFirst == "7.00") && (sMACADDR.length() > 0)) {
    int iMac = hexToDec(sMACADDR.substring(sMACADDR.length() - 2, sMACADDR.length())) % 60;
    sTimeFirst = "7." + int2str2dig(iMac);
  }
  if (!SPIFFS.begin(FORMAT_SPIFFS_IF_FAILED)) {
    Serial.println("SPIFFS Mount Failed");
    bSPIFFSExists = false;
  } else bSPIFFSExists = true;
  listSPIFFSDir("/", 2, true);
  Serial.printf("\n  Total/Used/Free:   %d/%d/%d kB\n", SPIFFS.totalBytes() / 1024, SPIFFS.usedBytes() / 1024, (SPIFFS.totalBytes() - SPIFFS.usedBytes()) / 1024);
  Serial.println("--------------------------------------------" );
  if (tNow > 0) {
    setenv("TZ", sTimeZone.c_str(), 1);
    struct timeval tVal = { .tv_sec = tNow };
    settimeofday(&tVal, NULL);
    tLastBoot = tNow;
  }
  ///////////////////////////////////////////////////
  if (bSPIFFSExists) {
    if ((tNow == 0) && (SPIFFS.exists("/lasttime.txt"))) {
      sAux1 = "";
      sAux1 = readSPIFFSFile("/lasttime.txt");
      tNow = atol(sAux1.c_str());
      if (tNow > 0) {
        setenv("TZ", sTimeZone.c_str(), 1);
        struct timeval tVal = { .tv_sec = tNow };
        settimeofday(&tVal, NULL);
      }
      sAux1 = sGetDateTimeStr(tNow);
      Serial.printf( "   Last time booted on %s\n", sAux1.c_str());
    }
    if (SPIFFS.exists("/jVars.txt")) {
      sAux1 = "";
      sAux1 = readSPIFFSFile("/jVars.txt");
      if (sAux1.length() > 0) {
        jVars.setJsonData(sAux1);
        if (iSizeJson(jVars) > 3) {
          bGetJsonVars();
          bDVars = true;
          Serial.printf("~LVars_SPFS %dB~", sAux1.length());
        }
      }
    }
    if (bHasBattery) {
      if (SPIFFS.exists("/jVtg.txt")) {
        sAux1 = "";
        sAux1 = readSPIFFSFile("/jVtg.txt");
        if (sAux1.length() > 0) {
          jVtg.setJsonData(sAux1);
          if (iSizeJson(jVtg) > 3) {
            bGetJsonVtg();
            iGetJsonVtgSlopes(iVtgVal, tVtgTime, VTGMEASSURES);
            Serial.printf("~LVtg_SPFS %dB~", sAux1.length());
            bDVtg = true;
          }
        }
      }
    }
    if (SPIFFS.exists("/jDefLog.txt")) {
      sAux1 = "";
      sAux1 = readSPIFFSFile("/jDefLog.txt");
      if (sAux1.length() > 0) {
        jDefLog.setJsonData(sAux1);
        if (iSizeJson(jDefLog) > 3) {
          Serial.printf("~LDefLog_SPFS %dB~", sAux1.length());
          bUDefLog = true;
        }
      }
    }
    jFunc.set("OTAUpdate", "");
    jMisc.set("Status", "");
    //iSetJsonMisc();
  }
  if (SPIFFS.exists("/otaupdat.txt")) {
    deleteSPIFFSFile("/otaupdat.txt");
    EraseAllNVS("otaupdat.txt");
    SendToRestart();
  }
  if (sRESETREASON == "RST") { //To prevent NVS bootloop
    sAux1 = "";
    sAux1 = readSPIFFSFile("/rstwoboo.txt"); //Resets without boot
    if (sAux1 == "") iAux = 0;
    iAux = sAux1.toInt() + 1;
    delay(1000);
    writeSPIFFSFile("/rstwoboo.txt", String(iAux).c_str());
    delay(1000);
    if (iAux > 1) Serial.println("\n" + sAux1 + " resets in a row");
    if (iAux > 9) {
      delay(100);
      EraseAllNVS("RST>9");
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
    Serial.print(" loading Wifi defaults...");
    iSPIFFSWifiSSIDs = iLoadWifiMulti();
    writeSPIFFSFile("/resets.txt", "0");
    LogDef("Default Wifi.txt Added", 2);
    delay(500);
    SendToRestart();
  }
  if (!StartWiFi((bResetBtnPressed ? 5 : 20))) {
    // No Wifi
    if (bDVars) {
      Serial.print("\n Working with SPIFFS...");
    } else {
      if (!bResetBtnPressed) {
        LogDef("No Wifi, No DevOld @1", 2);
        bUVars = false;
        bUVtg = false;
        bUFunc = false;
        bUMisc = false;
        SendToSleep(5); //Nothing to do
      }
    }
    LogDef("No Wifi, Yes DevOld @1", 2);
  } else bCheckWifiSPIFFSFile();
  iAux = hallRead();
  if (abs(iAux) > 70) {
    if (abs(iAux) > 150) LogDef("Hall=" + (String)(iAux), 2);
    else LogDef("Hall=" + (String)(iAux), 1);
  }
#ifdef ForceClock
  CheckLed();
#endif
  Serial.printf("\n  SPIFFS: %d WifiSsids, Vtg{%d>%d>%d>%d}", iSPIFFSWifiSSIDs, iVtgMax, iVtgChg, (io_VOLTAGE ? analogRead(io_VOLTAGE) : 0), iVtgMin);
  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);
  Firebase.reconnectWiFi(true);
  Firebase.setReadTimeout(firebaseData, 1000 * 40);
  Firebase.setwriteSizeLimit(firebaseData, "medium");
  Firebase.setMaxRetry(firebaseData, 3);
  Firebase.setMaxErrorQueue(firebaseData, 30);
  delay(500);
  if ((tNow > tLastBoot) && ((tNow - tLastBoot) < 60)) {
    iShortboots++;
    if (iShortboots > 10) {
      //There is a loop
      Serial.print("\nDetected LOOP #" + (String)(iShortboots));
      LogAlert("LOOP after WIFI detected", 2);
      iShortboots = 0;
      SendToSleep(30);
    }
  } else iShortboots = 0;
  if (bWeHaveWifi) {
    bFBAvailable = bCheckFBAvailable();
    if (bFBAvailable)  {
      bFBDevAvail = bCheckFBDevAvailable(true);
    } else bFBDevAvail = false;
  } else { //No Wifi
    bFBAvailable = false;
    bFBDevAvail = false;
  }
  if (bFBAvailable)  {
    sAux2 = "/dev/" + sMACADDR + "/";
    //Functions
    bDFunc = bFBGetJson(jFunc, sAux2 + "Functions");
    if (!bDFunc) {
      sAux1 = sAux2 + "Functions/OTAUpdate";
      delay(100);
      Firebase.setString(firebaseData, sAux1, "[]");
      bUFunc = true;
      bDFunc = bFBGetJson(jFunc, sAux2 + "Functions");
    }
    //Vars
    bDVars = bFBGetJson(jVars, sAux2 + "vars");
    if (!bDVars) {
      sAux1 = sAux2 + "vars/RefreshPeriod";
      delay(100);
      Firebase.setInt(firebaseData, sAux1, 60);
      bUVars = true;
      bDVars = bFBGetJson(jVars, sAux2 + "vars");
    }
    //Vtg
    if (bHasBattery) {
      bDVtg = bFBGetJson(jVtg, sAux2 + "Vtg");
      if (!bDVtg) {
        sAux1 = sAux2 + "Vtg/VHist/VtgVal0";
        delay(100);
        Firebase.setInt(firebaseData, sAux1, 0);
        bUVtg = true;
        bDVtg = bFBGetJson(jVtg, sAux2 + "Vtg");
      }
    }
    //Log
    bDLog = bFBGetJson(jLog, sAux2 + "Log");
    if (!bDLog) LogAlert("Log ini", 2);
  }
  Serial.print("  Wifi:" + (String)(bWeHaveWifi) + ",Firebase:" + (String)(bFBAvailable) + ",Dev:" + (String)(bFBDevAvail) + " ");
  bGetJsonVars();
  if (bHasBattery) {
    bGetJsonVtg();
    iAux2 = iGetJsonVtgSlopes(iVtgVal, tVtgTime, VTGMEASSURES);
    if (tVtgTime[0] == 0 ) FillEmptyVtgVal();
  }
  FBCheckLogLength();
  FlushDefLog();
  lBoots += 1;
  bInsideTempSensor = bCheckInternalTemp();
  iGetJsonFunctions();
  //  iSetJsonMisc();
  bUMisc = false;
  bFBCheckUpdateJsons();
  Serial.printf("\n  Updated %d times, on core %d since %s.", lBoots, xPortGetCoreID(), (tFirstBoot ? sInt32TimetoStr(tFirstBoot).c_str() : "never"));
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
      delay(4000);
      display.fillScreen(GxEPD_BLACK);
      delay(4000);
      bRefreshPage();
      delay(4000);
      bNewPage();
      delay(4000);
      display.fillScreen(GxEPD_WHITE);
      delay(4000);
      bRefreshPage();
      delay(4000);
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
    sAux1 = " Weather Station " + (String)(REVISION) + "/" +  (String)(sPlatform()) + "-" + (String)(compile_date);
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
    sAux1 = "   Hall sensor=" + (String)(hallRead()) + " CpuTemp=" + (String)(temperatureRead()) + "ºC";
    if (bInsideTempSensor) sAux1 = sAux1 + " Inside:" + (String)(fInsideTemp + fTempInOffset) + "ºC";
    DisplayU8Text(1 , 140, sAux1, fU8g2_XS);
    if (iAux2 > 0) sAux2 = String(iAux2) + " Vtgtimes 0.";
    else sAux2 = String((tVtgTime[VTGMEASSURES - 1] - tVtgTime[0]) / 3600) + "h of data.";
    sAux1 = "   Slope: 1=" + String(fLinearfit(iVtgVal, tVtgTime, VTGMEASSURES, 1)) + ",4=" + String(fLinearfit(iVtgVal, tVtgTime, VTGMEASSURES, 4)) + ",24=" + String(fLinearfit(iVtgVal, tVtgTime, VTGMEASSURES, 24)) + " & " + sAux2;
    DisplayU8Text(1 , 160, sAux1, fU8g2_XS);
    sAux1 = "   Version=" + (String)(REVISION) + " of " + (String)(__DATE__);
    DisplayU8Text(1 , 180, sAux1, fU8g2_XS);
    sAux1 = "   Firebase:" + (String)(bFBAvailable ? "ON" : "OFF") + " DEV:" + (String)(bFBDevAvail ? "ON" : "OFF");
    DisplayU8Text(1 , 200, sAux1, fU8g2_XS);
    sAux1 = "  Wait 3 secs to enter {" + String(sBootModes[iAux]) + "} Mode..." ;
    DisplayU8Text(1 , 240, sAux1, fU8g2_S);
    sAux1 = "  Reset again to enter {" + String(sBootModes[iAux + 1]) + "} Mode. ";
    DisplayU8Text(1 , 280, sAux1, fU8g2_S);
    bRefreshPage();
    if (bHasBattery) AddVtg(iGetMeanAnalogValueFast(io_VOLTAGE, 1000));
    bNewPage();
    display.fillScreen(GxEPD_WHITE);
    display.setTextColor(GxEPD_BLACK);
    writeSPIFFSFile("/resets.txt", "0");
    Serial.print("\nEntering Bootmode = " + String(sBootModes[iAux]) + ".");
    switch (iAux ) {
      case 2:
#ifdef WD_WEBSERVER
        LogAlert("Loading Web Server", 3);
        setWSPlatform(sPlatform());
        StartWifiAP(true);
        startWebServer();
        WiFi.disconnect();
#endif
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
        EraseAllNVS("CASE 5");
        SendToRestart();
        break;
      case 6:
        OTADefault();
        SendToRestart();
        break;
      default:
        break;
    }
  } else if (bHasBattery) AddVtg(iGetMeanAnalogValueFast(io_VOLTAGE, 1000)); //MENU ENDED
  return true;
}//////////////////////////////////////////////////////////////////////////////
bool bEndSetup() { //With standard
  if (WiFi.status() != WL_CONNECTED) {
    if (!StartWiFi(20)) {
      iBootWOWifi++;
      LogDef("No Wifi @2 x " + String(iBootWOWifi) + " times", 2);
    }
  } else   iBootWOWifi = 0;
  if (!bGetWeatherForecast()) {
    bWeHaveWifi = (WiFi.status() == WL_CONNECTED);
    bUVars = false;
    bUMisc = false;
    bUVtg = false;
    bUFunc = false;
    LogAlert("NO forecast, WF:" + (String)(bWeHaveWifi) + ",FB:" + (String)(bFBAvailable) + ",DV:" + (String)(bFBDevAvail) + " _", 2);
    if ((lBoots < 2) && (bResetBtnPressed)) {
      DisplayU8Text(1 , 60, " Forecast download unable", fU8g2_L);
      bRefreshPage();
    }
    if (bClk) SendToSleep(5);
    int iSleepPeriod = iMintoNextWake(tNow);
    SendToSleep(iSleepPeriod);
  }
  if (bHasBattery) {
    dGetVoltagePerc();
    bSetJsonVtg();
  }
  iSetJsonMisc();
  DisplayForecast();
  if (!bClk) {
    int iSleepPeriod = iMintoNextWake(tNow);
    Serial.print(" Display Update ");
    bRefreshPage();
    bCheckAWSSPIFFSFiles();
    SendToSleep(iSleepPeriod);
  } else {
    Serial.print(" Display Update for CLOCK");
    iStartMillis = 0;
    bCheckAWSSPIFFSFiles();
    if (WiFi.status() == WL_CONNECTED) {
      bFBCheckUpdateJsons();
    }
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
  const int iArrOx = 47, iArrOy = 8, iArrOs = 24;
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
      yH = iScreenYMax;      yL = iScreenYMax;
    } else {
      yH = 0;      yL = iScreenYMax - 12;
    }
  }
  if (tNow == 0) tNow = time(nullptr);
  iTemp = round(fGetTempTime(tNow));
  if ((-40 < iTemp) && (iTemp < 50))  fCurrTemp = iTemp;
  if (abs(fInsideTemp + fTempInOffset) > 50) bInsideTempSensor = false;
  if (fCurrTemp == 0) fCurrTemp = aTempH[0];
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
      DisplayU8TextAlignBorder(iScreenXMax * (.71 - bWS75 * .01), iScreenYMax / 10 + 1, sDateWeekDayName(sWeatherLNG, tNow), fU8g2_XL, -1, 0, bRed ? GxEPD_RED : GxEPD_BLACK);
      DisplayU8TextAlignBorder(iScreenXMax * (.775 - bWS75 * .01), iScreenYMax / 10 + 1, (String)(day(tNow)), fU8g2_XL, 0, 0);
      DisplayU8TextAlignBorder(iScreenXMax * (.84 - bWS75 * .01), iScreenYMax / 10 + 1, sDateMonthName(sWeatherLNG, tNow), fU8g2_XL, 1, 0, bRed ? GxEPD_RED : GxEPD_BLACK);
      if (sSummaryDay.length() > (iMaxCharsPerLine)) {
        DisplayU8Text(iScreenXMax * .55, iScreenYMax * (.149 - bWS75 * .019), sSummaryDay.substring(0, iMaxCharsPerLine), fU8g2_XS);
        DisplayU8Text(iScreenXMax * .55, iScreenYMax * (.185 - bWS75 * .020), sSummaryDay.substring(iMaxCharsPerLine, sSummaryDay.length()), fU8g2_XS);
      } else {
        if (sSummaryDay.length() > (iMaxCharsPerLine * .6)) {
          DisplayU8TextAlignBorder(iScreenXMax * .775, iScreenYMax * .155, sSummaryDay, fU8g2_XS, 0, 0);
        } else {
          DisplayU8TextAlignBorder(iScreenXMax * .775, iScreenYMax * .155, sSummaryDay, fU8g2_S, 0, 0);
        }
      }
      DisplayU8Text(1 + bWS75 * 3, yH - 5 + bWS75 * 2, sSummaryWeek, fU8g2_XS);
      if (sWeatherAPI.indexOf("darksky") >= 0) {
        DisplayWXicon(iScreenXMax * (xC + .314 + ((float)(sAux2.length()) - 2) * 0.01), iScreenYMax * (.026 - bWS42 * 0.02), sMeanWeatherIcon(0, 1));
      } else  { //Openweathermap
        DisplayWXicon(iScreenXMax * (xC + .314 + ((float)(sAux2.length()) - 2) * 0.01), iScreenYMax * (.026 - bWS42 * 0.02), aIcon[0]);
      }
      drawArrow(iScreenXMax * (xC + .314 + ((float)(sAux2.length()) - 2) * 0.01) + iArrOx - 2, iScreenYMax * (.026 - bWS42 * 0.02) + iArrOy, iArrOs , (int)(round(dGetWindSpdTime(tNow))), round(dGetWindBrnTime(tNow)), /*(dGetWindSpdTime(tNow) > 10)*/bRed ? GxEPD_RED : GxEPD_BLACK);
    }
  } else { //bClk
#ifdef ForceClock
    if (hour(tNow + 30) > 9) DisplayU8TextAlignBorder(iScreenXMax * .35, yH - 46,  String(hour(tNow + 30)), fU8g2_XXL, -1, 0, GxEPD_BLACK);
    else DisplayU8TextAlignBorder(iScreenXMax * .3, yH - 46,  String(hour(tNow + 30)), fU8g2_XXL, -1, 0, GxEPD_BLACK);
    DisplayU8TextAlignBorder(iScreenXMax * .395, yH - 46, ":", fU8g2_XXL, 0, 0, GxEPD_BLACK);
    DisplayU8TextAlignBorder(iScreenXMax * .44, yH - 46, int2str2dig(minute(tNow + 30)), fU8g2_XXL, 1, 0, GxEPD_BLACK);
    DisplayU8TextAlignBorder(iScreenXMax * .91, 22, sDateWeekDayName(sWeatherLNG, tNow), fU8g2_L, 0, 0, GxEPD_BLACK);
    DisplayU8TextAlignBorder(iScreenXMax * .91, 57,  (String)(day(tNow)), fU8g2_XL, 0, 0, GxEPD_BLACK);
    DisplayU8TextAlignBorder(iScreenXMax * .91, 80, sDateMonthName(sWeatherLNG, tNow), fU8g2_L, 0, 0, GxEPD_BLACK);
    DisplayWXicon(iScreenXMax * .84, yH - 46, sMeanWeatherIcon(0, 1));
    if ((-40 < fCurrTemp) && (fCurrTemp < 50)) sAux1 = String(int(abs(round(fCurrTemp)))) + char(176);
    else sAux1 = "-";
    DisplayWXicon(iScreenXMax * .76 - 70, yH - 46, "ood");
    if (fCurrTemp != 0) DisplayU8TextAlignBorder(iScreenXMax * .76, yH - 5, sAux1, fU8g2_XL, 0, 0, GxEPD_BLACK);
    if (bInsideTempSensor) {
      sAux1  = String(int(round(fInsideTemp + fTempInOffset))) + char(176);
      DisplayWXicon(-5, yH - 46, "hhd");
      DisplayU8TextAlignBorder(iScreenXMax * .22, yH - 5, sAux1, fU8g2_XL, 0, 0, GxEPD_BLACK);
    }
#endif
  }
  if (dIcon[0].length() < 1) {
    Serial.print("\n >> iDailyDisplay " + (String)(iDailyDisplay) + ">" + dIcon[1] + ":" + dIcon[1].length() + "\n");
    iDailyDisplay = 0;
  }
  if (iDailyDisplay > 2) yL = yL - 8;
  //HOURS & ICONS
  float fWindAccuX , fWindAccuY , fMeanBrn, fMeanSpd, fDirDeg;
  int iXIcon , iYIcon;
  if (!bWS29) {
    if (iDailyDisplay > 2) { //Daily icons
      for ( i = 1; i < 8; i++) { //ICON
        iXIcon  = (iScreenXMax / 8 - bWS42 * 20 + bWS75 * 32) / 7 + (iScreenXMax * (i - 1)) / 7 ;
        iYIcon = yL + 22 - 24 * bWS29 - bWS42 * 3;
        sAux1 = sDateWeekDayName(sWeatherLNG, tNow + (i * 86400));
        sAux2 = sAux1 + String((day(tNow + (i * 86400)) )); //sAux1.substring(0, 2);
        DisplayU8TextAlignBorder( iXIcon + (iScreenXMax / 16), iScreenYMax - 1, sAux2 ,  fU8g2_S, 0, 0); //, bRed ? GxEPD_RED : GxEPD_BLACK);
        DisplayWXicon(iXIcon, iYIcon, dIcon[i]);
        if ((aDTMin[i] != 0) || (aDTMax[i] != 0)) {
          sAux2 = (String)((int)(aDTMin[i])) + "/" + (String)((int)(aDTMax[i]));
          DisplayU8TextAlignBorder( iXIcon + (iScreenXMax / 16) - 1, iYIcon + 34 , sAux2 ,  fU8g2_S, 0, 1, bRed ? GxEPD_RED : GxEPD_BLACK);
        }
      }
      for ( i = 0; i < iDailyDisplay; i++) { //Wind
        fWindAccuX = 0;
        fWindAccuY = 0;
        iXIcon  = iScreenXMax / 64 - bWS42 * 5 + bWS75 * 4 + iScreenXMax / iDailyDisplay * i;
        iYIcon = yL + 26 - 24 * bWS29 - bWS42 * 3;
        for (j = i * ANALYZEHOURS / iDailyDisplay; j < ((i + 1)* ANALYZEHOURS / iDailyDisplay) ; j++) {
          fWindAccuX = fWindAccuX + (aWindSpd[j] * cos(aWindBrn[j] * 1000 / 57296));
          fWindAccuY = fWindAccuY + (aWindSpd[j] * sin(aWindBrn[j] * 1000 / 57296));
        }
        fMeanSpd = sqrt(pow(fWindAccuX * iDailyDisplay / ANALYZEHOURS , 2) + pow(fWindAccuY * iDailyDisplay / ANALYZEHOURS , 2));
        fMeanBrn = atan2 (fWindAccuY , fWindAccuX ) * 57296 / 1000;
        if (fMeanBrn >= 360) fMeanBrn -= 360;
        if (fMeanBrn < 0) fMeanBrn += 360;
        //        drawArrowBorder(iXIcon + iArrOx, /*yH + 5*/ iYIcon + iArrOy, iArrOs, (int)(round(fMeanSpd)), round(fMeanBrn), /*(fMeanSpd > 10)*/bRed ? GxEPD_RED : GxEPD_BLACK);
        drawArrow(iXIcon + iArrOx, iYIcon + iArrOy, iArrOs, (int)(round(fMeanSpd)), round(fMeanBrn), bRed ? GxEPD_RED : GxEPD_BLACK);
      }
    } else { //hourly icons
      if (sWeatherAPI.indexOf("darksky") >= 0) {
        for ( i = 0; i < 8; i++) { //ICON
          iXIcon  = iScreenXMax / 64 - bWS42 * 5 + bWS75 * 4 + iScreenXMax / 8 * i;
          iYIcon = yL + 26 - 24 * bWS29 - bWS42 * 3;
          fWindAccuX = 0;
          fWindAccuY = 0;
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
          DisplayWXicon(iXIcon, iYIcon, sMeanWeatherIcon(i * ANALYZEHOURS / 8, (i + 1)* ANALYZEHOURS / 8 - 1)); //, bRed?GxEPD_RED:GxEPD_BLACK);
          drawArrow(iXIcon + iArrOx, iYIcon + iArrOy, iArrOs, (int)(round(fMeanSpd)), round(fMeanBrn), /*(fMeanSpd > 10)*/ bRed ? GxEPD_RED : GxEPD_BLACK);
        }
      }  else { //OpenWeatherMap
        for ( i = 0; i < 8; i++) { //ICON
          iXIcon  = iScreenXMax / 64 - bWS42 * 5 + bWS75 * 4 + iScreenXMax / 8 * i;
          iYIcon = yL + 26 - 24 * bWS29 - bWS42 * 3;
          fWindAccuX = 0;
          fWindAccuY = 0;
          if (i > 3) drawBar(iXIcon, iYIcon , iXIcon + 60, iYIcon + 60, 2 , GxEPD_WHITE);
          //Wind
          for (j = i * ANALYZEHOURS / 8; j < ((i + 1)* ANALYZEHOURS / 8) ; j++) {
            fWindAccuX = fWindAccuX + (aWindSpd[j] * cos(aWindBrn[j] * 1000 / 57296));
            fWindAccuY = fWindAccuY + (aWindSpd[j] * sin(aWindBrn[j] * 1000 / 57296));
          }
          fMeanSpd = sqrt(pow(fWindAccuX * 8 / ANALYZEHOURS , 2) + pow(fWindAccuY * 8 / ANALYZEHOURS , 2));
          fMeanBrn = atan2 (fWindAccuY , fWindAccuX ) * 57296 / 1000;
          if (fMeanBrn >= 360) fMeanBrn -= 360;
          if (fMeanBrn < 0) fMeanBrn += 360;
          DisplayWXicon(iXIcon, iYIcon, aIcon[i * ANALYZEHOURS / 8]);
          drawArrow(iXIcon + iArrOx, iYIcon + iArrOy, iArrOs, (int)(round(fMeanSpd)), round(fMeanBrn), /*(fMeanSpd > 10)*/ bRed ? GxEPD_RED : GxEPD_BLACK);
        }
      }
    }
  }
  //Graph
  if (yH != yL)  DisplayForecastGraph(0, yH, iScreenXMax, yL - yH, ANALYZEHOURS, iOffsetH, fU8g2_S, fU8g2_M);
  //Hours
  if (!bWS29) DisplayU8Text(iScreenXMax / 128, yL + 22 - bWS42 * 3, sTimeLocal(tNow), fU8g2_M, bRed ? GxEPD_RED : GxEPD_BLACK);
  else if (!bClk) DisplayU8Text(iScreenXMax / 128, yL + 12, sTimeLocal(tNow), fU8g2_S, bRed ? GxEPD_RED : GxEPD_BLACK);
  for ( i = 0; i < 8; i++) {
    if (i > 0) {
      if (!bWS29) DisplayU8TextAlignBorder( t2x(tNow + (iOffsetH + (6 * i)) * 3600, iScreenXMax), yL + 22 - bWS42 * 3, String((hour(tNow) + iOffsetH - 1 + (6 * (i))) % 24) + "h",  fU8g2_M, 0, 0);
      else if (!bClk) DisplayU8Text(t2x(tNow + (iOffsetH + (6 * i)) * 3600, iScreenXMax), yL + 12, String((hour(tNow) + iOffsetH - 2 + (6 * (i))) % 24) + "h",  fU8g2_S);
    }
  }
  Serial.print("  Forecast Display redone...");
}//////////////////////////////////////////////////////////////////////////////
#define PRECIP_LVL_2 4
#define MAXMINNUM 8
#define TEMPGAP 5

void DisplayForecastGraph(int x, int y, int wx, int wy, int iAnalyzePeriod, int iOffsetH, const uint8_t *fontS, const uint8_t *fontL) {
  int iTmp1, iTmp2, iNightIni, i, j, iWDay, xHourA, xHourB, xPrecF, iOffsetX,  iHourMin[MAXMINNUM], iHourMax[MAXMINNUM], jMin, jMax, jLastMin = -3, jLastMax = -3, iTempY , iGrMin[MAXMINNUM], iGrMax[MAXMINNUM];
  float dTempMax = -100, dTempMin = 100, dPrecipMax = 0, dTempMaxReal, dTempMinReal;
  bool bIsNight = false, bPrecipText = false, bJumpMin, bJumpMax;
  i = 12 - hour(tNow);
  iOffsetX = ((/*iOffsetH +*/  2) * wy) / iAnalyzePeriod ;
  DisplayU8TextAlignBorder(iOffsetX + ((i *  wx) / iAnalyzePeriod), y + wy / 2 - 5, sWeekDayNames(sWeatherLNG, (iWeekdayToday()) % 7) ,  bWS29 ? fU8g2_L : fU8g2_XL, 0, 0, bRed ? GxEPD_RED : GxEPD_BLACK);
  i += 24;
  DisplayU8TextAlignBorder(iOffsetX + ((i *  wx) / iAnalyzePeriod), y + wy / 2 - 5, sWeekDayNames(sWeatherLNG, (iWeekdayToday() + 1) % 7) ,  bWS29 ? fU8g2_L : fU8g2_XL, 0, 0, bRed ? GxEPD_RED : GxEPD_BLACK);
  i += 24;
  DisplayU8TextAlignBorder(iOffsetX + ((i *  wx) / iAnalyzePeriod), y + wy / 2 - 5, sWeekDayNames(sWeatherLNG, (iWeekdayToday() + 2) % 7) ,  bWS29 ? fU8g2_L : fU8g2_XL, 0, 0, bRed ? GxEPD_RED : GxEPD_BLACK);
  drawLine(x, y - 1, x + wx, y - 1, 1, 2);
  /*    drawLine(x + (0.07 * wx), y - 1 + wy / 4, x + (0.93 * wx), y - 1 + wy / 4, 1, 3);
      drawLine(x + (0.07 * wx), y - 1 + wy / 2, x + (0.93 * wx), y - 1 + wy / 2, 1, 2);
      drawLine(x + (0.07 * wx), y - 1 + wy * 3 / 4, x + (0.93 * wx), y - 1 + wy * 3 / 4, 1, 3);*/
  drawLine(x, y + wy + 1, x + wx, y + wy + 1, 1, 1);
  if ((tSunset > 0) && (tSunrise > 0)) {
    long int tSA, tSB;
    tSA = tSunset - 86400; //the one from yesterday
    tSB = tSunrise ;
    //Serial.printf("\nHOURS: A%d->B%d,Now:%d\n", tSA, tSB, tNow);
    do { //Bars
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
  }
  //Max,Min
  for (i = 0; i < MAXMINNUM; i++) {
    iHourMin[i] = -1;
    iHourMax[i] = -1;
    iGrMin[i] = 100;
    iGrMax[i] = -100;
  }
  jMin = 0;
  jMax = 0;
  bool bSkipMin, bSkipMax;
  for ( i = 0; i < iAnalyzePeriod; i++) {
    bSkipMin = false;
    bSkipMax = false;
    if ( aTempH[i] > dTempMax) dTempMax = aTempH[i];
    if ( aTempH[i] < dTempMin) dTempMin = aTempH[i];
    if ((aFeelT[i] != 0) && ( aFeelT[i] > dTempMax)) dTempMax = aFeelT[i];
    if ((aFeelT[i] != 0) && ( aFeelT[i] < dTempMin)) dTempMin = aFeelT[i];
    if ( aPrecip[i] > dPrecipMax) dPrecipMax = aPrecip[i];
    if ((i > 0) && (i < (iAnalyzePeriod - 2))) {
      if (jMin > 0) {
        if (round(aTempH[i]) == iGrMin[jMin - 1]) {
          bSkipMin = true;
          bSkipMax = true;
        }
      } else {
        if (jMax) {
          if ((round(aTempH[i]) == iGrMax[jMax - 1])) {
            bSkipMin = true;
            bSkipMax = true;
          }
        }
      }
      if (jMax > 0) {
        if (round(aTempH[i]) == iGrMax[jMax - 1]) {
          bSkipMin = true;
          bSkipMax = true;
        }
      } else {
        if (jMin) {
          if ((round(aTempH[i]) == iGrMin[jMin - 1])) {
            bSkipMin = true;
            bSkipMax = true;
          }
        }
      }
      if ((!bSkipMin) && (i >= (jLastMin + TEMPGAP)) && (bIsTempMin(i))) {
        iGrMin[jMin] = round(aTempH[i]);
        iHourMin[jMin] = aHour[i];
        jMin++;
        jLastMin = i;
        if (jMin >= (MAXMINNUM - 1)) jMin = MAXMINNUM - 1;
        i += TEMPGAP - 1;
      }
      if ((!bSkipMax) && (i >= (jLastMax + TEMPGAP)) && (bIsTempMax(i))) {
        iGrMax[jMax] = round(aTempH[i]);
        iHourMax[jMax] = aHour[i];
        jMax++;
        jLastMax = i;
        if (jMax >= (MAXMINNUM - 1)) jMax = MAXMINNUM - 1;
        i += TEMPGAP - 1;
      }
    }
  }
  dPrecipMax = dPrecipMax * 1.02;
  dTempMaxReal = dTempMax;
  dTempMinReal = dTempMin;
  dTempMax = dTempMax + abs(dTempMax) * 0.02;
  dTempMin = dTempMin - abs(dTempMin) * 0.2;
  if ((dTempMax - dTempMin) < 10) {
    float iDiff = 10 - (dTempMax - dTempMin);
    if (dTempMin > 0) {
      dTempMax = dTempMax + (iDiff * dTempMax ) / (dTempMax + dTempMin);
      dTempMin = dTempMin - (iDiff * dTempMin ) / (dTempMax + dTempMin);
    } else {
      dTempMax = dTempMin + 10;
    }
  }
  j = (int)(dTempMax / 2) * 2;
  do {
    iTmp1 = y - 1 + wy - (float)(wy) * ((float)(j) - dTempMin) / (dTempMax - dTempMin);
    if (iTmp1 < (y + wy))drawLine(x + (0.02 * wx), iTmp1 , x + (0.98 * wx), iTmp1, 1, 2);
    j -= 2;
  } while (j > dTempMin);
  if (dPrecipMax > PRECIP_LVL_2)  dPrecipMax = round(dPrecipMax + 0.5); //Show the values progressively
  else  dPrecipMax = round((-0.0102 * dPrecipMax * dPrecipMax * dPrecipMax) - (0.0289 * dPrecipMax * dPrecipMax) + (0.9987 * dPrecipMax) + 1.9841);
  if (dPrecipMax < 1) dPrecipMax = 1;
  iOffsetX = ((/*iOffsetH +*/  5) * wy) / iAnalyzePeriod ;
  //Graph lines
  for ( i = 0; i < (iAnalyzePeriod - 1); i++) {
    if ((aTempH[i] != 0) && (aTempH[i + 1] != 0)) {
      xHourA = x + iOffsetX + t2x(aHour[i], wx );
      xHourB = x + iOffsetX + t2x(aHour[i + 1], wx );
      if ((xHourA <= (x + wx)) && (xHourB >= x) && (xHourB > xHourA) && (xHourB < (xHourA + (2 * wx / iAnalyzePeriod)))) {
        if (xHourA < x) xHourA = x;
        if (xHourB > (x + wx)) xHourB = x + wx;
        xPrecF = (xHourB - xHourA) * (1 - ((aPrecipProb[i] < 0.5) ? (aPrecipProb[i] * 1.5) : (aPrecipProb[i] * 0.5 + 0.5)));
        //Clouds
        drawLine(xHourA, y + wy - wy * aCloudCover[i], xHourB, y + wy - wy * aCloudCover[i + 1], 3, 3, bRed ? GxEPD_RED : GxEPD_BLACK);
        //Feel
        if (aFeelT[i] > dTempMin) drawLine(xHourA, y + wy - ((float)(wy) * (aFeelT[i] - dTempMin) / (dTempMax - dTempMin)), xHourB, y + wy - ((float)(wy) * (aFeelT[i + 1] - dTempMin) / (dTempMax - dTempMin)), 3, 3); //, bRed ? GxEPD_RED : GxEPD_BLACK);
        //Rain
        drawLine(xHourA, y + wy - wy * aPrecip[i] / dPrecipMax, xHourB, y + wy - wy * aPrecip[i + 1] / dPrecipMax, 2, 2, bRed ? GxEPD_RED : GxEPD_BLACK);
        drawBar (xHourA + xPrecF - ((xHourB - xHourA) / 2), (int)(y + wy - wy * aPrecip[i] / dPrecipMax), xHourB - xPrecF - ((xHourB - xHourA) / 2), y + wy, 1, GxEPD_WHITE);
        drawBar (xHourA + xPrecF - ((xHourB - xHourA) / 2), (int)(y + wy - wy * aPrecip[i] / dPrecipMax), xHourB - xPrecF - ((xHourB - xHourA) / 2), y + wy, 2, bRed ? GxEPD_RED : GxEPD_BLACK);
        //Temp
        drawLine(xHourA, y + wy - ((float)(wy) * (aTempH[i] - dTempMin) / (dTempMax - dTempMin)), xHourB, y + wy - ((float)(wy) * (aTempH[i + 1] - dTempMin) / (dTempMax - dTempMin)), 4, 1);
      }
    }
  }
  // 0ºC
  if ((dTempMax > 0) && (dTempMin < 0)) {
    int iPos0 = (int)((dTempMax * wy)  / (dTempMax - dTempMin));
    if (iPos0 < wy) {
      drawLine(x + (0.01 * wx), y + iPos0 + 1 , x + (0.99 * wx), y + iPos0 + 1, 3, 2, bRed ? GxEPD_RED : GxEPD_BLACK);
      drawLine(x + (0.01 * wx), y  + iPos0, x + (0.99 * wx), y + iPos0, 1, 2, GxEPD_BLACK);
      DisplayU8TextAlignBorder(x + 2, y + iPos0 - 3, float2string(0, 0) + char(176),  fontL, 1, 2);
    }
  }
  if (bPrecipText)  {
    DisplayU8TextAlignBorder(x + wx - 16, y + 25 - bWS29 * 4 + ((dPrecipMax >= PRECIP_LVL_2) ? 12 : 0), float2string(dPrecipMax, 0), ((dPrecipMax >= PRECIP_LVL_2) ? fU8g2_XL : fU8g2_L), -1, 2, bRed ? GxEPD_RED : GxEPD_BLACK);
    DisplayU8TextAlignBorder(x + wx - 14, y + 15 - bWS29 * 4, "mm",  fU8g2_XS, 1, 2, bRed ? GxEPD_RED : GxEPD_BLACK);
  }
  //Border Temps
  /*
    DisplayU8TextAlignBorder(x, y + 15, float2string(dTempMax, 0) + char(176),  fontL, 1, 2);
    DisplayU8TextAlignBorder(x, y + 4 + wy / 2, float2string(dTempMax - (dTempMax - dTempMin) / 2, 0) + char(176), fontS, 1, 2);
    DisplayU8TextAlignBorder(x, y + wy - 2, float2string(dTempMin, 0) + char(176), fontL, 1, 2);
    if (!bClk) {
    DisplayU8TextAlignBorder(x, y + 4 + wy / 4, float2string(dTempMax - (dTempMax - dTempMin) / 4, 0) + char(176), fU8g2_XS, 1, 2);
    DisplayU8TextAlignBorder(x, y + 4 + wy * 3 / 4, float2string(dTempMax - (dTempMax - dTempMin) * 3 / 4, 0) + char(176),  fU8g2_XS, 1, 2);
    }
  */
  //MinMax
  for (j = 0; j < MAXMINNUM; j++) {
    if (iHourMin[j] > 0) {
      xHourA = x + iOffsetX + t2x(iHourMin[j], wx );
      iTempY = y - 8 + wy - wy * (iGrMin[j] - dTempMin) / (dTempMax - dTempMin);
      //      Serial.printf("\n MIN(%d)= [%d->%d->%d]", j, iTempY, y + wy - iTempY, y + wy);
      if ((y + wy - iTempY) > 42) iTempY += 26;
      DisplayU8TextAlignBorder(xHourA - 3, iTempY, float2string(iGrMin[j], 0) + char(176), fontL, 1, 2);
    }
    if (iHourMax[j] > 0) {
      xHourA = x + iOffsetX + t2x(iHourMax[j], wx );
      iTempY = y +  24  + wy - wy * (iGrMax[j] - dTempMin) / (dTempMax - dTempMin);
      //      Serial.printf("\n MAX(%d)= [%d->%d->%d]", j, y, iTempY - y, iTempY);
      if ((iTempY - y) > 42) iTempY -= 30;
      DisplayU8TextAlignBorder(xHourA - 11, iTempY  , float2string(iGrMax[j], 0) + char(176), fontL, 1, 2);
    }
  }
  String sAux2 = sGetJsonString(jVars, "CustomText", sCustomText);
  if (sAux2.length() > 0)  sCustomText = sAux2;
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
  IconName.replace(" ", "");
  if (!IconName.length()) return;
  if      (IconName == "01d" || IconName == "clear" || IconName == "clear-day") display.drawBitmap(x, y, gImage_01d, 48, 48, color);
  else if (IconName == "01n" || IconName == "clear-night") display.drawBitmap(x, y, gImage_01n, 48, 48, color);
  else if (IconName == "02d" || IconName == "partly-cloudy" || IconName == "partly-cloudy-day" || IconName == "partly - cloudy") display.drawBitmap(x, y, gImage_02d, 48, 48, color);
  else if (IconName == "02n" || IconName == "partly-cloudy-night") display.drawBitmap(x, y, gImage_02n, 48, 48, color);
  else if (IconName == "03d" || IconName == "cloudy" || IconName == "cloudy-day" || IconName == "cloudy") display.drawBitmap(x, y, gImage_03d, 48, 48, color);
  else if (IconName == "03n" || IconName == "cloudy-night") display.drawBitmap(x, y, gImage_03n, 48, 48, color);
  else if (IconName == "04d") display.drawBitmap(x, y, gImage_04d, 48, 48, color); //dark cloud
  else if (IconName == "04n") display.drawBitmap(x, y, gImage_04n, 48, 48, color);
  else if (IconName == "09d" || IconName == "rain-day" || IconName == "rain") display.drawBitmap(x, y, gImage_09d, 48, 48, color);
  else if (IconName == "09n" || IconName == "rain-night") display.drawBitmap(x, y, gImage_09n, 48, 48, color);
  else if (IconName == "10d") display.drawBitmap(x, y, gImage_10d, 48, 48, color); //rain and sun
  else if (IconName == "10n") display.drawBitmap(x, y, gImage_10n, 48, 48, color);
  else if (IconName == "11d" || IconName == "storm-day" || IconName == "storm") display.drawBitmap(x, y, gImage_11d, 48, 48, color);
  else if (IconName == "11n" || IconName == "storm-night") display.drawBitmap(x, y, gImage_11n, 48, 48, color);
  else if (IconName == "13d" || IconName == "snow-day" || IconName == "snow") display.drawBitmap(x, y, gImage_13d, 48, 48, color);
  else if (IconName == "13n" || IconName == "snow-night") display.drawBitmap(x, y, gImage_13n, 48, 48, color);
  else if (IconName == "50d" || IconName == "50n"  || IconName == "fog") display.drawBitmap(x, y, gImage_50n, 48, 48, color);
  else if (IconName == "hhd") display.drawBitmap(x, y, gImage_hhd, 48, 48, color);
  else if (IconName == "ood") display.drawBitmap(x, y, gImage_ood, 48, 48, color);
  else if (IconName == "wind") display.drawBitmap(x, y, gImage_wind, 48, 48, color);
  else    {
    display.drawBitmap(x, y, gImage_nodata, 48, 48, color);
    LogAlert("NO ICON [" + IconName + "]", 3);
  }
}//////////////////////////////////////////////////////////////////////////////
bool bIsTempMin(int i) {
  int iIni = i - TEMPGAP, iEnd = i + TEMPGAP, j;
  if (iIni < 0) iIni = 0;
  if (iEnd > ANALYZEHOURS) iEnd = ANALYZEHOURS;
  for (j = iIni; j < iEnd; j++) {
    if (aTempH[j] < aTempH[i]) return false;
  }
  return true;
}
bool bIsTempMax(int i) {
  int iIni = i - TEMPGAP, iEnd = i + TEMPGAP, j;
  if (iIni < 0) iIni = 0;
  if (iEnd > ANALYZEHOURS) iEnd = ANALYZEHOURS;
  for (j = iIni; j < iEnd; j++) {
    if (aTempH[j] > aTempH[i]) return false;
  }
  return true;
}
//////////////////////////////////////////////////////////////////////////////
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
    //Raining
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
void drawArrowBorder(int x, int y, int iMaxSize, int iSpd, float fDir, uint16_t iColor) {
  drawArrowS(x + 1, y + 1, iMaxSize, iSpd, fDir, GxEPD_WHITE);
  drawArrowS(x + 1, y - 1, iMaxSize, iSpd, fDir, GxEPD_WHITE);
  drawArrowS(x - 1, y + 1, iMaxSize, iSpd, fDir, GxEPD_WHITE);
  drawArrowS(x - 1, y - 1, iMaxSize, iSpd, fDir, GxEPD_WHITE);
  drawArrowS(x, y + 1, iMaxSize, iSpd, fDir, GxEPD_WHITE);
  drawArrowS(x, y - 1, iMaxSize, iSpd, fDir, GxEPD_WHITE);
  drawArrowS(x + 1, y, iMaxSize, iSpd, fDir, GxEPD_WHITE);
  drawArrowS(x - 1, y, iMaxSize, iSpd, fDir, GxEPD_WHITE);
  drawArrowS(x, y, iMaxSize, iSpd, fDir, iColor);
}//////////////////////////////////////////////////////////////////////////////
void drawArrow(int x, int y, int iMaxSize, int iSpd, float fDir, uint16_t iColor) {
  if (3 > iSpd) return;
  float fRad1 = fDir * DEG_TO_RAD;
  float fLength = (float)(iMaxSize) * (0.6 + ((float)(iSpd) / 60));
  int dX1 = (cos(fRad1) * fLength * .1), dY1 = (sin(fRad1) * fLength * .1);
  drawArrowS( x,  y,  iMaxSize,  iSpd,  fDir,  iColor);
  drawArrowS( x + dX1,  y + dY1,  iMaxSize,  iSpd,  fDir,  iColor);
  drawArrowS( x + dX1,  y - dY1,  iMaxSize,  iSpd,  fDir,  iColor);
  drawArrowS( x - dX1,  y - dY1,  iMaxSize,  iSpd,  fDir,  iColor);
  drawArrowS( x - dX1,  y + dY1,  iMaxSize,  iSpd,  fDir,  iColor);

}//////////////////////////////////////////////////////////////////////////////
void drawArrowS(int x, int y, int iMaxSize, int iSpd, float fDir, uint16_t iColor) {
  float flW = .8;
  if (3 > iSpd) return;
  fDir = (round(fDir / 20) * 20) +  180;
  if (fDir >= 360) fDir -= 360;
  float fRad1 = fDir * DEG_TO_RAD, fRad2 = (fDir + 135) * DEG_TO_RAD, fRad3 = (fDir - 135) * DEG_TO_RAD;;
  float fLength = (float)(iMaxSize) * (0.6 + ((float)(iSpd) / 60));
  float x1 = x + (sin(fRad1) * fLength * .5), y1 = y - (cos(fRad1) * fLength * .5);
  float x2 = x - (sin(fRad1) * fLength * .5), y2 = y + (cos(fRad1) * fLength * .5);
  float x3 = x1 + (sin(fRad2) * fLength * .5 * flW), y3 = y1 - (cos(fRad2) * fLength * .5 * flW);
  float x4 = x1 + (sin(fRad3) * fLength * .5 * flW), y4 = y1 - (cos(fRad3) * fLength * .5 * flW);
  drawLine(x1, y1, x2 , y2 , 1, 1, iColor); // Main
  drawLine(x1 , y1 , x3 , y3 , 1, 1, iColor); //Right
  drawLine(x1 , y1 , x4 , y4 , 1, 1, iColor); //Left
}//////////////////////////////////////////////////////////////////////////////
void PaintBatt(int BattPerc) {
  if (BattPerc > 0)  {
    String sAux = sBattStatus[iBattStatus];
    if (bWS42 || bWS75) {
      display.fillRect(0, 0, 22 + bWS75 * 5, 60 + bWS75 * 8, GxEPD_WHITE);
      int iVChgEnd;
      //Serial.println("\n"+(String)(iBattStatus));
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
    //iSetJsonBatt(iVtgVal[VTGMEASSURES - 1], sAux );
    sBattMsg = sAux;
  } else {
    drawLine(1, 1, 22 + bWS75 * 5, 60 + bWS75 * 8, 3, 1, bRed ? GxEPD_RED : GxEPD_BLACK);
    drawLine(22 + bWS75 * 5, 1, 1, 60 + bWS75 * 8, 3, 1, bRed ? GxEPD_RED : GxEPD_BLACK);
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
  String jsonFioString = "", jsonHoursString;
  bool bFromSPIFFS = false, bExistsSPIFFS = false, bFioFailed = false;
  Serial.print(" GetWF:" + (String)(ESP.getFreeHeap() / 1024) + "kB free,");
  sLastWe = "--";
  //if ((!bWeHaveWifi) || ((tNow - tLastSPIFFSWeather) < (iRefreshPeriod * 35))) jsonFioString = readSPIFFSFile("/weather.txt");
  jsonFioString = readSPIFFSFile("/weather.txt");
  if (jsonFioString.length()) {
    DynamicJsonBuffer jsonBuffer(1024);
    JsonObject& root = jsonBuffer.parseObject(jsonFioString);
    if (root.success()) {
      bExistsSPIFFS = true;
      tLastSPIFFSWeather = root["currently"]["time"];
      Serial.print(", JW_SPIFFS Ok " + (String)(ESP.getFreeHeap() / 1024) + "kB free,");
      if ((!bWeHaveWifi) || ((tNow - tLastSPIFFSWeather) < (iRefreshPeriod * 45 ))) {
        Serial.print(" SPIFFS");
        sLastWe = "SPIFFS";
      }
      else jsonFioString = "";
    } else {
      jsonFioString = "";
      Serial.print(", JW_SPIFFS NOk ");
    }
  }
  if (bWeHaveWifi) {
    if (!jsonFioString.length()) {
      FB_GetWeatherJson(&jsonFioString, true);
      if (jsonFioString.length()) {
        Serial.print(", JW_FIREBASE " + (String)(ESP.getFreeHeap() / 1024) + "kB free,");
        DynamicJsonBuffer jsonBuffer(1024);
        JsonObject& root = jsonBuffer.parseObject(jsonFioString);
        if (!root.success()) {
          jsonFioString = "";
          Serial.print(" NOT OK!");
        } else {
          Serial.print(" OK");
          sLastWe = "FB";
        }
      } else Serial.print(" No FB WJ!, ");
    } else bFromSPIFFS = true;
    if (!jsonFioString.length()) { //load from URL
      if (sWeatherAPI.indexOf("darksky") >= 0) {
        String jsonHoursString = "{}", sWeatherJSONSource;
        sWeatherURL =  "https://api.darksky.net/forecast/";
        sWeatherFIO =  "api.darksky.net";
        //Hours
        sWeatherJSONSource = sWeatherURL + sWeatherKEY + "/" + sWeatherLOC + "?units=si&lang=" + sWeatherLNG + "&exclude=[alerts,flags,daily]";
        Serial.print(" <Download darksky " + sWeatherLOC + " > ");
        if (!bGetJSONString(sWeatherFIO, sWeatherJSONSource, &jsonFioString)) {
          sLastWe = "None";
        } else {
          bSummarizeDKSJsonWHours(&jsonFioString);
          jsonHoursString = jsonFioString;
          sLastWe = "DSHours";
        }
        jsonFioString = "";
        delay(5000);
        // Days
        sWeatherJSONSource = sWeatherURL + sWeatherKEY + "/" + sWeatherLOC + "?units=si&lang=" + sWeatherLNG + "&exclude=[alerts,flags,current,hourly]";
        if (!bGetJSONString(sWeatherFIO, sWeatherJSONSource, &jsonFioString)) {
          jsonFioString = "";
          if (jsonHoursString.length() > 0)
            jsonFioString = jsonHoursString;
          else  sLastWe = "None";
        } else {
          bSummarizeDKSJsonWDays(&jsonFioString, &jsonHoursString);
          jsonHoursString = "";
          sLastWe = "DSHoursDays";
        }
        jsonHoursString = "";
      }  else { //OWM
        sWeatherURL =  "http://api.openweathermap.org/data/3.0/onecall";
        sWeatherFIO =  "api.openweathermap.org";
        String sWeatherJSONSource;
        float fLon, fLat;
        int iPos = sWeatherLOC.indexOf(",");
        fLat = atof(sWeatherLOC.substring(0, iPos).c_str());
        fLon = atof(sWeatherLOC.substring(iPos + 1, sWeatherLOC.length()).c_str());
        String sLatLong = "?lat=" + (String)(fLat) + "&lon=" + (String)(fLon);
        sWeatherJSONSource = sWeatherURL + sLatLong + "&appid=" + sWeatherKEY + "&lang=" + sWeatherLNG + "&units=metric&exclude=minutely,daily";
        if (!bGetJSONString(sWeatherFIO, sWeatherJSONSource, &jsonFioString)) {
          sLastWe = "None";
        } else {
          if (!bSummarizeOWMJsonWHours(&jsonFioString)) {
            jsonFioString = "";
          } else {
            jsonHoursString = jsonFioString;
            jsonFioString = "";
            sLastWe = "OWMH";
          }
        }
        sWeatherJSONSource = sWeatherURL + sLatLong + "&appid=" + sWeatherKEY + "&lang=" + sWeatherLNG + "&units=metric&exclude=minutely,hourly";
        if (!bGetJSONString(sWeatherFIO, sWeatherJSONSource, &jsonFioString)) {
          jsonFioString = "";
          if (jsonHoursString.length() > 0)
            jsonFioString = jsonHoursString;
          else  sLastWe = "None";
        } else {
          if (!bSummarizeOWMJsonWDays(&jsonFioString, &jsonHoursString)) {
            jsonFioString = "";
          } else sLastWe = "OWMHD";
        }
      } //openweathermap
      if (jsonFioString.length() > 0) { // Save to FB
        if (bCheckSJson(&jsonFioString)) {
          FB_SetWeatherJson(jsonFioString);
          writeSPIFFSFile("/weather.txt", jsonFioString.c_str());
          Serial.print(", Loaded " + (String)(ESP.getFreeHeap() / 1024) + "kB free,");
        } else {
          jsonFioString = "";
        }
      }
    }
  }
  if (!jsonFioString.length() && bExistsSPIFFS) {   //Last chance with old data
    jsonFioString = readSPIFFSFile("/weather.txt");
    if (jsonFioString.length()) {
      Serial.print(", SPIFFS WJSON Loaded " +  (String)(jsonFioString.length()) + " chrs,");
      bFromSPIFFS = true;
      bFioFailed = false;
      sLastWe = "SPIFFS_OLD";
    }
  }
  if (!jsonFioString.length() ) {   //try old FB
    FB_GetWeatherJson(&jsonFioString, false);
    sLastWe = "FB_OLD";
    bFromSPIFFS = false;
    bFioFailed = false;
  }
  if (!jsonFioString.length()) {   //Now Show
    LogAlert("No weather data.", 2);
    bFioFailed = true;
  }  else   {
    Serial.print(", WJSON Loaded " +  (String)(jsonFioString.length()) + " chrs," + (String)(ESP.getFreeHeap() / 1024) + "kB free,");
    if (!showWeather_conditionsFIO(&jsonFioString ))         {
      Serial.print(" showWeather_conditionsFIO FAILED!,");
      LogAlert("Failed to get Conditions Data", 1);
      bFioFailed = true;
      if ((sLastWe == "OWMHD") || (sLastWe == "DSHours")) { //remove failing data from FB
        bFBSetInt(1, "/Weather/" + sWeatherLOC + "/Time");
      }
    }
  }
  if ((!bFromSPIFFS) && (!bFioFailed) && (jsonFioString.length())) writeSPIFFSFile("/weather.txt", jsonFioString.c_str());
  jsonFioString = "";
  Serial.print("#" + sLastWe + "#");
  return (!bFioFailed);
}////////////////////////////////////////////////////////////////////////////////
bool showWeather_conditionsFIO(String * jsonFioString ) {
  String sAux;
  time_t tLocal , tAux;
  int tzOffset;
  float fTempTot = 0;
  Serial.print("\n  ->showWeather_conditionsFIO," );
  DynamicJsonBuffer jsonBuffer(1024);
  Serial.print("Parsing " + (String)(jsonFioString->length()) + "B,");
  JsonObject& root = jsonBuffer.parseObject(*jsonFioString);
  Serial.print("done.");
  if (!root.success()) {
    LogAlert(F("\nERROR: jsonBuffer.parseObject() failed @showWeather_conditionsFIO\n"), 1);
    return false;
  }
  Serial.print("->Vars," );
  fCurrTemp = root["currently"]["temperature"];
  tzOffset = root["offset"];
  tzOffset *= 3600;
  tSunrise = root["daily"]["sunriseTime"];
  tSunset = root["daily"]["sunsetTime"];
  String stmp1 = root["hourly"]["summary"];
  sSummaryDay = stmp1;//sUtf8ascii(stmp1);
  String stmp2 = root["daily"]["summary"];
  sSummaryWeek = stmp2;//sUtf8ascii(stmp2);
  tLastSPIFFSWeather = root["currently"]["time"];
  for (int i = 0; i < ANALYZEHOURS; i++) {
    aHour[i] = tLocal;//+ tzOffset;
    if (sWeatherAPI.indexOf("darksky") >= 0) {
      if ((hour(aHour[i]) < hour(tSunrise)) || (hour(aHour[i]) > hour(tSunset))) {
        sAux = "-night";
      } else {
        sAux = "-day";
      }
    }
    tLocal =          root["hourly"]["time" + (String)(i)];
    aTempH[i] =       root["hourly"]["temp" + (String)(i)];
    aFeelT[i] =       root["hourly"]["feel" + (String)(i)];
    aHumid[i] =       root["hourly"]["humi" + (String)(i)];
    aPrecip[i] =      root["hourly"]["preI" + (String)(i)];
    aPrecipProb[i] =  root["hourly"]["preP" + (String)(i)];
    aWindSpd[i] =     root["hourly"]["winS" + (String)(i)];
    aWindBrn[i] =     root["hourly"]["winB" + (String)(i)];
    aCloudCover[i] =  root["hourly"]["cloC" + (String)(i)];
    String sTemp =    root["hourly"]["icon" + (String)(i)];
    fTempTot += aTempH[i];
    if (sTemp.length() < 8) sTemp = sTemp + sAux;
    sTemp.replace(" ", "");
    aIcon[i] = sTemp;
  }
  for (int i = 0; i < 8; i++) {
    String sTemp = root["daily"]["icon" + (String)(i)];
    sTemp.replace(" ", "");
    dIcon[i] = sTemp;
    aDTMin[i] = root["daily"]["tMin" + (String)(i)];
    aDTMax[i] = root["daily"]["tMax" + (String)(i)];
  }
  tAux = time(nullptr);
  if (tAux > 0)  tNow = tAux;
  if (fTempTot == 0) {
    Serial.println("\n ERROR; total Temp = 0 on showWeather_conditionsFIO");
    return false;
  }
  if (!tFirstBoot) tFirstBoot = tNow;
  Serial.print("Done." );
  return true;
}//////////////////////////////////////////////////////////////////////////////
bool StartWiFi(int iRetries) {
  if (!iSPIFFSWifiSSIDs) {
    Serial.println("\nDefine WIFI params!." );
#ifdef WD_WEBSERVER
    bWeHaveWifi = false;
    StartWifiAP(true);
    setWSPlatform(sPlatform());
    startWebServer();
    WiFi.disconnect();
#endif
    SendToRestart();
    return false;
  }
  int i = 0, j = 0;
  if (WiFi.status() == WL_CONNECTED)  WiFi.disconnect();
  Serial.print(" |WIFI");
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
    bWeHaveWifi = false;
    String sAux = "";
    for (i = 0; i < iSPIFFSWifiSSIDs; i++) sAux = sAux + "," + getAWifiSSID(i);
    LogAlert("No SSID found within" + sAux, 2);
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
  sWifiIP = WiFi.localIP().toString();
  iWifiRSSI = WiFi.RSSI();
  Serial.print("(pwd:" + String(sWifiPassword.length()) + " chrs long)-> @" + sWifiIP + " RSSI:" + (String)(iWifiRSSI) + "dBm ");
  NtpConnect();
  Serial.print("|");
  return true;
}//////////////////////////////////////////////////////////////////////////////
void NtpConnect() {
  struct tm tmLocal;
  int i = 0;
  configTime( 0, 0, "pool.ntp.org", "time.nist.gov");
  setenv("TZ", sTimeZone.c_str(), 1);
  Serial.print(",NTP=");
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
  delay(1000); //if removed ntp is not refreshed
  tNow = time(nullptr);
  char buff[30];
  sprintf(buff, "%04d/%02d/%02d_%02d:%02d:%02d", year(tNow), month(tNow), day(tNow), hour(tNow), minute(tNow), second(tNow));
  Serial.printf(" %s Ok.", buff);
  if (bSPIFFSExists) {
    String sAux = (String)(tNow);
    writeSPIFFSFile("/lasttime.txt", sAux.c_str());
  }
}//////////////////////////////////////////////////////////////////////////////
void SendToSleep(int mins) {
  String sAux = "";
  int iJsonSize;
  display.powerDown();
  Serial.print("\n[-> To sleep... " + (String)mins + " mins");
  lSecsOn += millis() / 1000;
  long lSecsCycle = ((lBoots) ? (0.8 * lSecsOn / lBoots) : 30);
  if (lBoots > 0) Serial.print(". @" + (String)(millis() / 1000) + " secs/" + (String)(lSecsCycle) + " mean ");
  uint64_t sleep_time_us = (uint64_t)(mins) * 60000000ULL;
  if (mins > 20) sleep_time_us -= (uint64_t)(lSecsCycle) * 1000000ULL; //give time for preprocessing as mean cycle is 1 min
  if (!mins) sleep_time_us = 2000000ULL;
  if (abs(fInsideTemp + fTempInOffset) < 50)  sAux = String(fInsideTemp + fTempInOffset) + "ºC ";
  sAux = sAux + sRESETREASON + ">SLP" + uintToStr(sleep_time_us / 60000000ULL) + "@" + (String)(int)(millis() / 1000) + "/" + (String)(lSecsCycle) + "s/c";
  LogAlert( sAux, 1);
  delay(100);
  if (WiFi.status() == WL_CONNECTED) {
    bFBCheckUpdateJsons();
    WiFi.disconnect();
  }
  delay(100);
  //END
  if (WiFi.status() == WL_CONNECTED)  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);
  bWeHaveWifi = false;
  tNow = tNow + (mins * 60);
  Serial.println("]. Zzz\n-------------------------------- -");
  delay(100);
  SPIFFS.end();
  iShortboots = 0;
  delay(2000);
  esp_sleep_enable_timer_wakeup(sleep_time_us);
  esp_deep_sleep_start();
}////////////////////////////////////////////////////////////////////////////////////
void SendToRestart() {
  Serial.print("\n[-> To reset... ");
  //lSecsOn += millis()/1000;
  if (lBoots > 2) lBoots--;
  String sAux = "";
  if (abs(fInsideTemp + fTempInOffset) < 50)  sAux = String(fInsideTemp + fTempInOffset) + "ºC ";
  long lSecsCycle = ((lBoots) ? (0.8 * lSecsOn / lBoots) : 30);
  sAux = sAux + sRESETREASON + " > RST @" + (String)(int)(millis() / 1000) + "/" + (String)(lSecsCycle) + "s/c";
  LogAlert( sAux, 1);
  if (WiFi.status() == WL_CONNECTED) {
    bFBCheckUpdateJsons();
    WiFi.disconnect();
  }
  WiFi.mode(WIFI_OFF);
  bWeHaveWifi = false;
  Serial.println("].\n-------------------------------- -");
  delay(100);
  SPIFFS.end();
  iShortboots = 0;
  delay(2000);
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
    AddVtg(iGetMeanAnalogValueFast(io_VOLTAGE, 1000));//iGetMeanAnalogValue(io_VOLTAGE, 5, 5));
  }
  Vtg = iVtgVal[VTGMEASSURES - 1];
  int BattSlope1 = fLinearfit(iVtgVal, tVtgTime, VTGMEASSURES, 1);
  int BattSlope4 = fLinearfit(iVtgVal, tVtgTime, VTGMEASSURES, 4);
  int BattSlope12 = fLinearfit(iVtgVal, tVtgTime, VTGMEASSURES, 12);
  int BattSlopeChg = fLinearfit(iVtgVal, tVtgTime, VTGMEASSURES, (tNow - tTimeLastVtgChg) / 3600);
  dCurrVoltage = 4.15 * Vtg / iVtgMax;
  float fVaux = iVtgMin;
  if (fVaux > ((float)iVtgMax * .8)) fVaux = (float)iVtgMax * .8;
  BattPerc = 100 * (Vtg - fVaux) / ((float)(iVtgMax) - fVaux);
  iBattStatus = -1;
  for (i = 0; i < VTGMEASSURES; i++) if (iVtgVal[i] > 0) break;
  ////////////////////////////////////////////////////////////////////////
  if ((tVtgTime[VTGMEASSURES - 1] - tVtgTime[i]) < PERIODBETWEENVTGS)     iBattStatus = 0;//NO INFO
  if ((iBattStatus < 0 ) && (BattSlope1 > (BATTTHRESHOLD * 2))) iBattStatus = 1;         //CHARGING.
  if ((iBattStatus < 0 ) && (BattPerc > 98)) iBattStatus = 2;             						    //FULL.
  if ((iBattStatus < 0 ) && (BattSlope4 > (BATTTHRESHOLD / 2))) iBattStatus = 1;       //CHARGING.
  if (iBattStatus < 0 ) iBattStatus = 3; //If not all previous should be DISCHARGING as slope is really low
  ////////////////////////////////////////////////////////////////////////
  if ((iBattStatus > 2) && (BattSlope4 > (-1 * BATTTHRESHOLD / 100))) { //Stable Discharge
    if ((Vtg > iVtgStableMax) && ((iVtgMax * 0.985) > Vtg)) iVtgStableMax = Vtg;
    if (iVtgStableMin > Vtg ) iVtgStableMin = Vtg;
  }
  if ((tVtgTime[18] > 0) && (((100 * (iVtgVal[23] - iVtgVal[0])) / iVtgVal[23]) > 10 ) && bFBDevAvail && (iBattStatus < 3) && ((BattSlope12 > (BATTTHRESHOLD / 2)) || (tTimeLastVtgChg == 0))) { //Charging
    bool bFull = false;
    if (abs(iVtgChg - Vtg) > (Vtg * .005)) bFull = true;
    if ((!bFull) && (BattSlope4 > 0) && ((BATTTHRESHOLD / 5) > BattSlope4) ) {
      iVtgMax = iVtgMax * .98;
      iVtgStableMax = iVtgStableMax * .98;
      bFull = true;
    }
    if (bFull) {
      if ((tNow - tTimeLastVtgChg) > 86400) {
        LogAlert("New CHG!", 3);
      }
      iBattStatus = 2;
      Serial.print("[‚¨]");
      if ((iVtgMax * .98) > Vtg)      iVtgChg = Vtg ;
      else iVtgChg = iVtgMax * .98;
      tTimeLastVtgChg = tNow;
    }
  }
  if (5 > BattPerc) iBattStatus = 4;
  if (iBattStatus == -1) iBattStatus = 0;
  if ((dCurrVoltage > 0) && (dCurrVoltage < 2.9) ) { //Battery drained -> CUT
    DisplayU8Text(5, iScreenYMax * .6, " LOW BATTERY -> OFF  ", fU8g2_XL, bRed ? GxEPD_RED : GxEPD_BLACK);
    bRefreshPage();
    String sLowVolt = (String)(Vtg);
    writeSPIFFSFile("/lowvolt.txt", sLowVolt.c_str());
    LogAlert("Batt low " + (String)(dCurrVoltage) + "V", 3);
    delay(2000);
    SendToSleep(60);
  }
  PaintBatt(BattPerc);
  return BattPerc;
}//////////////////////////////////////////////////////////////////////////////
#define MAXPERCDROPPERMEASSURE 0.01
void AddVtg(int iVtg) {
  if (!io_VOLTAGE) return;
  int iAux = 0, i;
  if ((iVtg > iVtgMax) && (iVtg < VTGMAXIMALLOWED)) {
    iVtgMax = iVtg;
    LogAlert(" [New VtgMax = " + (String)(iVtgMax) + "]", 1);
    tTimeLastVtgMax = tNow;
    if (iVtgMin > (iVtgMax * .8)) iVtgMin = iVtgMax * .8;
  }
  if ((iVtg < iVtgMin) && (iVtg > 0)) {
    iVtgMin = iVtg;
    Serial.print(" [New VtgMin = " + (String)(iVtgMin) + "]");
  }
  if (tNow > 10000) {
    iAux = abs( tNow - tTimeLastVtgChg) / 3600; //Hours
    if ((tTimeLastVtgChg > 0) && (iAux > (iVtgPeriodMax)) && (abs( iVtgChg - iVtg) > 50)) {
      iVtgPeriodMax = iAux;
      if (iVtg > iVtgStableMin)  iVtgPeriodDrop = iVtgChg - iVtg;
      else  iVtgPeriodDrop = iVtgChg - iVtgStableMin;
      if (iVtgPeriodDrop < 1) iVtgPeriodDrop = 1;
      if ((iVtgPeriodMax / iVtgPeriodDrop) > 20) iVtgPeriodMax = iVtgPeriodDrop * 20;
      Serial.print(" [New VtgDrop = " + (String)(tNow) + ", " + (String)(tTimeLastVtgChg) + "->" + (String)(iVtgPeriodMax) + "/" +  (String)(iVtgPeriodDrop)  + "]");
    }
  } else       Serial.print(" [tNow = " + (String)(tNow) + "]!!");

  if ((iVtgVal[VTGMEASSURES - 1] - iVtg) > ((float)iVtgVal[VTGMEASSURES - 1] * (float)MAXPERCDROPPERMEASSURE)) { //prevent incorrect drops
    LogAlert("Cointained Vtg drop " + (String)iVtgVal[VTGMEASSURES - 1] + "->" + (String)iVtg + " ? -> " + (String)((float)iVtgVal[VTGMEASSURES - 1] * ((float)1 - (float)MAXPERCDROPPERMEASSURE)), 1);
    iVtg = (float)iVtgVal[VTGMEASSURES - 1] * 0.98;
  }
  if (tNow > 100000) { //time available
    bool bCh1, bCh2, bCh3;
    int iStep1, iStep2, iStep3;
    iStep1 = (int)(VTGMEASSURES * 1 / 3);
    iStep2 = (int)(VTGMEASSURES * 2 / 3);
    iStep3 = (int)(VTGMEASSURES );
    bCh1 = (tVtgTime[iStep1 ] - tVtgTime[iStep1 - 1]) > (24 * 3600);
    bCh2 = (tVtgTime[iStep2 ] - tVtgTime[iStep2 - 1]) > (4 * 3600);
    bCh3 = ((tNow - tVtgTime[VTGMEASSURES - 1]) > PERIODBETWEENVTGS);
    if (bCh3) {
      //Divide in 3 steps 1h,4h,24h
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
      iAux = iSetJsonVtgSlopes(iVtgVal, tVtgTime, VTGMEASSURES);
      Serial.print(" {$" + String(bCh1 ? "1" : "") + String(bCh2 ? "2" : "") + String(bCh3 ? "3" : "") + " : " + String(iAux) + "}" );
    } else Serial.print(" {E}");
    iLastVtgNotWritten = 0;
  } else {
    Serial.print(" {@}");
    iLastVtgNotWritten = iVtg;
  }
}/////////////////////////////////////////////////////////////////////////////////////////
bool StartWifiAP(bool bUpdateDisplay) {
  const char *APssid = "ESP32";
  WiFi.softAP(APssid);
  //IPAddress ip(192, 168, 1, 1);
  Serial.print("Started AP ESP32 with IP address : ");
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
    DisplayU8Text(1, 80, " Stored ssid [" + sWifiSsid + "], pass : " + sWifiPassword.length() + " chars", fU8g2_S, GxEPD_BLACK);
    DisplayU8Text(1, 100, " Please connect to WIFI ssid 'ESP32'", fU8g2_S, GxEPD_BLACK);
    DisplayU8Text(1, 120, " open http : //192.168.4.1", fU8g2_S, GxEPD_BLACK);
    DisplayU8Text(1, 140, " enter params, and reboot when done..", fU8g2_S, GxEPD_BLACK);
    DisplayU8Text(1, 180, " If you use a phone, first connect airplane", fU8g2_S, GxEPD_BLACK);
    DisplayU8Text(1, 200, " mode , and then enable the wifi, so you", fU8g2_S, GxEPD_BLACK);
    DisplayU8Text(1, 220, " avoid 3G data connection interference..", fU8g2_S, GxEPD_BLACK);
    bRefreshPage();
    display.fillScreen(GxEPD_WHITE);
  }
  return true;
}//////////////////////////////////////////////////////////////////////////////
void tGetRawVoltage(void * pvParameters) {
  if (!io_VOLTAGE) return;
  bGettingRawVoltage = true;
  AddVtg(iGetMeanAnalogValueFast(io_VOLTAGE, 1000));
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
  iSetJsonVtgSlopes(iVtgVal, tVtgTime, VTGMEASSURES);
}//////////////////////////////////////////////////////////////////////////////
bool bGetJSONString(String sHost, String sJSONSource, String * jsonString) {
  int httpPort = 443, iTries = 2;
  bool bConn = false;
  unsigned long timeout ;
  WiFiClientSecure  SecureClientJson;
  Serial.print("  Connecting to {" + String(sHost) + "} ");
  do {
    if (SecureClientJson.connect(const_cast<char*>(sHost.c_str()), httpPort)) bConn = true;
    else     delay(5000);
    iTries--;
  } while ((iTries) && (!bConn));
  if (!bConn) {
    LogAlert("\n ERROR **Connection failed for JSON {" + sJSONSource + "} **\n", 1);
    return false;
  }
  SecureClientJson.print(String("GET ") + sJSONSource + " HTTP/1.1\r\n" + "Host: " + sHost + "\r\n" + "Connection: close\r\n\r\n");
  timeout = millis();
  while (SecureClientJson.available() == 0) {
    if (millis() - timeout > 10000) {
      LogAlert("\n ERROR Client Connection Timeout 10 seg... Stopping.", 1);
      SecureClientJson.stop();
      return false;
    }
  }
  Serial.print(" download ");
  while (SecureClientJson.available()) {
    String sTemp = SecureClientJson.readStringUntil('\r');
    sTemp.replace("\n", "");
    sTemp.replace("\r", "");
    sTemp.trim();
    if (sTemp.length() > 2) {
      if ((sTemp.substring(0, 1) == "{") && (sTemp.length() > jsonString->length())) {
        //        Serial.print("\n" + String(sTemp.length()) + "B,[" + sTemp + "] ");
        *jsonString = sTemp;
      }
    }
  }
  SecureClientJson.stop();
  if (jsonString->length() < 100) {
    Serial.print("\n json downloaded: [" + *jsonString + "]");
  }
  if (jsonString->length() > 2) {
    //Check End
    if (jsonString->substring(jsonString->length() - 1) != "}") {
      Serial.print("\nERROR: json not finished! [" + jsonString->substring(jsonString->length() - 5, jsonString->length()) + "]");
      *jsonString = *jsonString + "}";
    }
  }
  return true;
}//////////////////////////////////////////////////////////////////////////////////////////////////
String sGetGPSLocality(String sGPS, String sAPI) {
  String sJsonData = "";
  String sJsonHost = "maps.googleapis.com";
  String sJsonSource = "https://maps.googleapis.com/maps/api/geocode/json?latlng=" + sGPS + "&result_type=locality&sensor=false&key=" + sAPI;
  if (bGetJSONString(sJsonHost, sJsonSource, &sJsonData)) {
    sJsonData.replace("\n", "");
    sJsonData.replace("\r", "");
    Serial.println("\n LOAD LOCALITY: {" + sJsonSource + "} [" + sJsonData + "]");
    DynamicJsonBuffer jsonBuffer(256);
    JsonObject& root = jsonBuffer.parseObject(sJsonData);
    if (!root.success()) {
      Serial.print(" ERROR No root on sGetGPSLocality " );
      return "";
    }
    String sResGPS;
    sResGPS = root["plus_code"]["compound_code"].as<String>(); //root["results"][0]["address_components"][0]["short_name"].as<String>();
    if (sResGPS.length() > 4) {
      int iB, iC;
      iB = sResGPS.indexOf(" ");
      iC = sResGPS.indexOf(",");
      if ((iB < 1) || (iC < iB)) return "";
      else sResGPS = sResGPS.substring(iB + 1, iC);
      sResGPS.trim();
      return  sResGPS;
    } else     return  "";
  } else {
    Serial.print("\n ERROR No json String on sGetGPSLocality ");
    return "";
  }
}//////////////////////////////////////////////////////////////////////////////
bool bCheckFBAvailable() {
  String sAux = "";
  int i = 0;
  do {
    sAux = "";
    if (i > 0) delay(5000);
    else delay(100);
    bFBGetStr(sAux, "Available", false);
    i++;
  } while ((sAux.length() < 1) && (i < 3));
  if (i > 1) Serial.print("~bCheckFBAvailable + " + (String)(i) + " tries~");
  return (sAux.length() > 0);
}//////////////////////////////////////////////////////////////////////////////////////////////////
#define MAXALLOWEDJDEV 6144
int iCheckJDevSize() {
  if (iJDevSize > 0) return iJDevSize;
  if (sMACADDR.length() < 10) return 0;
  if (!bWeHaveWifi) return 0;
  Serial.print("|cDS");
  int i = 0;
  iJDevSize = 0;
  FirebaseJson jDev;
  do {
    if (i > 0) delay(5000);
    else delay(100);
    bFBGetJson(jDev, "/dev/" + sMACADDR, true);
    i++;
    iJDevSize = iSizeJson(jDev);
  } while ((iJDevSize < 4) && (i < 3));
  if (iJDevSize > MAXALLOWEDJDEV) { //There is some error with the data
    delay(1000);
    Firebase.deleteNode(firebaseData, "/dev/" + sMACADDR);
    delay(5000);
    bFBSetStr(sDevID, "/dev/" + sMACADDR + "/Id");
    delay(1000);
    bULog = false;
    bUMisc = false;
    bUVtg = false;
    bUVars = false;
    LogDef("jDev=" + (String)(iJDevSize), 2);
    delay(1000);
    SendToSleep(0);
  }
  Serial.print("=" + (String)(iJDevSize) + "B");
  jDev.clear();
  Serial.print("|");
  return iJDevSize;
}//////////////////////////////////////////////////////////////////////////////////////////////////
bool bCheckFBDevAvailable(bool bWriteJson) {
  if (sMACADDR.length() < 10) return false;
  String sAux1, sAux2;
  sDevID = sMACADDR;
  sAux2 = sGetJsonString(jVars, "Id", sDevID);
  if  (!bFBGetStr(sAux1, "/dev/" + sMACADDR + "/Id")) {
    sDevID = sAux2;
    bFBSetStr(sDevID, "/dev/" + sMACADDR + "/Id");
    return false;
  }
  if ((sAux1.length() > 0) && (sAux1 != sMACADDR)) sDevID = sAux1;
  else {
    if (sAux2 != sMACADDR) sDevID = sAux2;
    else sDevID = sMACADDR;
    bFBSetStr(sDevID, "/dev/" + sMACADDR + "/Id");
  }
  return true;
}//////////////////////////////////////////////////////////////////////////////////////////////////
bool iSetJsonMisc() {
  char buff[32];
  String sAux;
  Serial.print("|sM");
  if ((bResetBtnPressed) || (hour(tNow) < 2)) { //once a day
    jMisc.clear();
    sAux = (String)(REVISION) + " " + sPlatform();
    jMisc.set("Soft_Rev", sAux);
    sAux = (String)(compile_date) + " _";
    jMisc.set("Soft_Wrtn", sAux);
    sAux = listPartitions(false);
    jMisc.set("Partitions", sAux);
    sAux = (String)((SPIFFS.totalBytes() - SPIFFS.usedBytes()) / 1024) + "kB Free";
    sAux = sAux + listSPIFFSDir("/", 2, false);
    jMisc.set("SPIFFS", sAux);
    //    iCheckJDevSize();
    //    if (iJDevSize)  jMisc.set("DevSize", iJDevSize);
    if (tNow > (tFirstBoot + 86400)) {
      float fMeanOn = lSecsOn * 86400 / (tNow - tFirstBoot);
      float fMeanBoots = lBoots * 86400 / (tNow - tFirstBoot);
      sAux = (String)(int)(fMeanOn) + "s/" + (String)(int)(fMeanBoots) + "b = " + (String)(int)(fMeanOn / fMeanBoots) + "sec/boot _";
      jMisc.set("DailyUse", sAux);
    }
  }
  sprintf(buff, "%04d/%02d/%02d_%02d:%02d:%02d_", year(tNow), month(tNow), day(tNow), hour(tNow), minute(tNow), second(tNow));
  sAux = (String)(buff);
  jMisc.set("TimeLastUpdate", sAux);
  sprintf(buff, "%04d/%02d/%02d_%02d:%02d:%02d_", year(tLastSPIFFSWeather), month(tLastSPIFFSWeather), day(tLastSPIFFSWeather), hour(tLastSPIFFSWeather), minute(tLastSPIFFSWeather), second(tLastSPIFFSWeather));
  sAux = sLastWe + "#" + (String)(buff);
  jMisc.set("TimeLastWeather", sAux);
  sAux = (String)(iWifiRSSI) + "dBm, " + sWifiSsid + "@" + sWifiIP + " _";
  jMisc.set("Ssid", sAux);
  if (bInsideTempSensor)   sAux = String(fInsideTemp + fTempInOffset);
  else sAux = " - ";
  jMisc.set("TempIn", sAux);
#ifdef ForceClock
  if (bClk) {
    String sLed = CheckLed();
    jMisc.set("Led", sLed);
  }
#endif
  sAux = (String)(iBattPercentage(iVtgVal[VTGMEASSURES - 1])) + "%l," + (String)(iBattPercentageCurve(iVtgVal[VTGMEASSURES - 1])) +  "%c,CHGN,1h" + (String)(int)(fLinearfit(iVtgVal, tVtgTime, VTGMEASSURES, 1)) + ",4h" + (String)(int)(fLinearfit(iVtgVal, tVtgTime, VTGMEASSURES, 4)) + ",12h" + (String)(int)(fLinearfit(iVtgVal, tVtgTime, VTGMEASSURES, 12)) + "_";
  jMisc.set("Slope", sAux);
  jMisc.set("Status", sBattMsg);
  bUMisc = true;
  Serial.print("|");
  return true;
}//////////////////////////////////////////////////////////////////////////////
#define BLOCKSIZE 9999
bool FB_SetWeatherJson(String jsonW) {
  Serial.print("|sWJ");
  if (jsonW.length() < 5000) {
    Serial.printf("\n FBSetWJ: jsonW too small %dB", jsonW.length());
    return false;
  }
  String sAux1 = "/Weather/" + sWeatherLOC + "/", sAux2;
  int iTimes = 0, iLength, tFirst = 0 ;
  sAux1.replace(".", "'");
  Serial.print("\n FBSetWJ:");
  bFBGetInt(&timeUploaded, sAux1 + "Time");
  if (timeUploaded < 1) { //Why corrupted?
    timeUploaded = tNow;
    bFBSetInt(timeUploaded, sAux1 + "Time");
  }
  if (tNow > 0) {
    delay(100);
    //    if ((tNow - timeUploaded) < 600) return false; //It is too early to update
    bFBSetInt(tNow, sAux1 + "Time");
  } else return false;
  delay(100);
  jsonW.replace("\"", "\\\"");
  iLength = jsonW.length();
  bFBSetInt(iLength, sAux1 + "Size");
  delay(100);
  bFBSetStr(sDevID, sAux1 + "Uploader");
  Serial.print((String)(iLength));
  iTimes = (int)(1 + (iLength / BLOCKSIZE));
  if (iTimes > 1) delay(500);
  else delay(200);
  if (iTimes == 1) {
    bFBSetStr(jsonW, sAux1 + "jsonW");
  } else {
    delay(100);
    Firebase.deleteNode(firebaseData, sAux1 + "json/" );
    delay(100);
    if (iTimes > 3) iTimes = 3;
    int iTicksIn = millis();
    for (int i = 0; i < iTimes; i++ ) {
      sAux2 = jsonW.substring((i * BLOCKSIZE), (i + 1) * BLOCKSIZE);
      if (iTimes < 5) bFBSetStr(sAux2, sAux1 + "json/" + (String)(i));
      if (iTimes > 1) delay(2000);
      else delay(200);
    }
  }
  ///Set, now play with your toys
  delay(100);
  String sLocality = "";
  bFBGetStr(sLocality , sAux1 + "Locality");
  if (sLocality.length() < 1) {
    delay(100);
    sLocality = sGetGPSLocality(sWeatherLOC, sGeocodeAPIKey);
    if (sLocality.length() > 0) {
      delay(100);
      bFBSetStr(sLocality, sAux1 + "Locality");
      LogAlert("[New Locality='" + sLocality + "']", 2);
    }
  }
  delay(100);
  bFBGetInt(&iTimes , sAux1 + "nDownloads");
  iTimes += 1;
  delay(100);
  bFBGetInt(&tFirst , sAux1 + "tFirstDownload");
  delay(100);
  if ((tNow) && (!tFirst)) bFBSetInt(tNow, sAux1 + "tFirstDownload");
  delay(100);
  bFBSetInt(iTimes, sAux1 + "nDownloads");
  delay(100);
  if (tNow > tFirst) bFBSetInt((iTimes * 86400 / (tNow - tFirst)), sAux1 + "MeanDownDay");
  delay(100);
  Serial.print("Ok.");
  Serial.print("|");
  return true;
}//////////////////////////////////////////////////////////////////////////////
bool FB_GetWeatherJson(String * jsonW, bool bRejectOld) {
  String sAux1 = "/Weather/" + sWeatherLOC + "/", sAux2, sAux3;
  int iLength = 0, iTimes;
  sAux1.replace(".", "'");
  delay(100);
  Serial.print("|FBGWJ");
  if (!bFBGetInt(&timeUploaded , sAux1 + "Time")) {
    Serial.print(", getting json_Time failed : ");
    return false;
  }
  if (timeUploaded < 1) {
    bFBGetStr(sAux3, sAux1 + "Uploader", false);
    LogAlert("Weather Upload Time -!:" + (String)(timeUploaded ) + " by " + sAux3, 2);
    bFBSetInt(tNow, sAux1 + "Time");
  } else {
    if (bRejectOld && ((tNow - timeUploaded) > (iRefreshPeriod * 60))) {
      Serial.print(", [getting json too old. Rejected.]");
      return false; //Too old
    }
  }
  delay(100);
  if (!bFBGetInt(&iLength, sAux1 + "Size")) {
    Serial.print("getting json_length failed : ");
    return false;
  }
  delay(100);
  sAux2 = "";
  iTimes = (int)(1 + (iLength / BLOCKSIZE));
  if (iTimes == 1) {
    if (!bFBGetStr(sAux2, sAux1 + "jsonW", false)) {
      *jsonW = "";
      return false;
    }
  } else {
    if (iTimes > 3) iTimes = 3;
    Serial.printf(" acceptable %d blocks", iTimes);
    for (int i = 0; i < iTimes; i++ ) {
      sAux3 = "";
      if (bFBGetStr(sAux3, sAux1 + "json/" + (String)(i), false)) {
        //      Serial.printf(",json %d=%dB + ", i, sAux3.length());
        sAux2 = sAux2 + sAux3;
      }
      delay(100);
    }
  }
  sAux2.trim();
  String s1 = "\\" + (String)(char(34));
  String s2 = (String)(char(34));
  sAux2.replace(s1, s2);
  sAux2.replace("\\n", "");
  *jsonW = sAux2;
  ///Get, now play with your toys
  delay(100);
  String sLocality;
  bFBGetStr(sLocality, sAux1 + "Locality");
  if ((sLocality.length() > 2) && (sCustomText.length() < 2)) {
    sCustomText = sLocality;
    jVars.set("CustomText", sCustomText);
    bUVars = true;
  }
  delay(100);
  Serial.printf("=%dB OK. ", jsonW->length());
  Serial.print("|");
  return true;
}//////////////////////////////////////////////////////////////////////////////////////////////////
bool bSummarizeOWMJsonWHours(String * sJWD) {
  float fAux;
  Serial.print("\n SUMMARY OWN WJSON from " + (String)(sJWD->length()) + " ");
  //Reduce
  sJWD->replace("ozone", "o1");
  sJWD->replace("pressure", "p1");
  sJWD->replace("visibility", "v1");
  sJWD->replace("humidity", "h1");
  sJWD->replace("description", "d1");
  sJWD->replace("dew_point", "d2");
  sJWD->replace("wind_speed", "w1");
  sJWD->replace("wind_deg", "w2");
  sJWD->replace("feels_like", "f1");
  Serial.print(" reduced to " + (String)(sJWD->length()) + " ");
  DynamicJsonBuffer jsonBufferO(1024);
  JsonObject& rootO = jsonBufferO.parseObject(*sJWD);
  if (!rootO.success()) {
    LogAlert("\nERROR SUMMARIZING, no rootO " + sJWD->substring(0, 100) + "\n" + sJWD->substring(sJWD->length() - 100, sJWD->length()) + "\n", 1);
    jsonBufferO.clear();
    return false;
  }
  DynamicJsonBuffer jsonBufferN(1024);
  JsonObject& rootN = jsonBufferN.parseObject(" {}");
  if (!rootN.success()) {
    Serial.print("\nERROR SUMMARIZING, no rootN\n");
    jsonBufferO.clear();
    jsonBufferN.clear();
    return false;
  }
  JsonObject& NSubPath1 = rootN.createNestedObject("currently");
  JsonObject& NSubPath2 = rootN.createNestedObject("daily");
  JsonObject& NSubPath3 = rootN.createNestedObject("hourly");
  rootN["daily"]["sunriseTime"] = (int)(rootO["current"]["sunrise"]);
  rootN["daily"]["sunsetTime"] = (int)(rootO["current"]["sunset"]);
  rootN["offset"] = (int)(rootO["timezone_offset"]);
  for (int i = 0; i < ANALYZEHOURS; i++) {
    rootN["hourly"]["time" + (String)(i)] = (int)(rootO["hourly"][i]["dt"]);
    rootN["hourly"]["temp" + (String)(i)] = (float)(rootO["hourly"][i]["temp"]);
    rootN["hourly"]["feel" + (String)(i)] = (float)(rootO["hourly"][i]["f1"]);
    rootN["hourly"]["humi" + (String)(i)] = (float)(rootO["hourly"][i]["h1"]);
    fAux = (float)(rootO["hourly"][i]["rain"]["1h"]);
    rootN["hourly"]["preI" + (String)(i)] = fAux;
    rootN["hourly"]["preP" + (String)(i)] = 1;
    rootN["hourly"]["winS" + (String)(i)] = (float)(rootO["hourly"][i]["w1"]);
    rootN["hourly"]["winB" + (String)(i)] = (int)(rootO["hourly"][i]["w2"]);
    rootN["hourly"]["cloC" + (String)(i)] = (float)(rootO["hourly"][i]["clouds"]) / 100;
    String stmp0 = rootO["hourly"][i]["weather"][0]["icon"];
    rootN["hourly"]["icon" + (String)(i)] = stmp0;
  }
  fAux = rootO["currently"]["temp"];
  if (fAux == 0) fAux = rootN["hourly"]["temp0"];
  rootN["currently"]["temperature"] = fAux;
  int iAux = (int)(rootO["current"]["dt"]);
  if (iAux == 0) fAux = (int)(rootN["hourly"]["time0"]);
  rootN["currently"]["time"] = (int)(iAux);
  String stmp2 = rootO["hourly"][0]["weather"][0]["d1"];
  if (stmp2.length() > 2) {
    String sCap = stmp2.substring(0, 1);
    sCap.toUpperCase();
    stmp2 = sCap + stmp2.substring(1, stmp2.length());
  }
  rootN["hourly"]["summary"] = stmp2;
  *sJWD = "";
  rootN.printTo(*sJWD);
  Serial.print(" to " + (String)(sJWD->length()) + ".");
  jsonBufferO.clear();
  jsonBufferN.clear();
  return true;
}//////////////////////////////////////////////////////////////////////////////////////////////////
bool bSummarizeOWMJsonWDays(String * sJWD, String * sJWH) {
  //Reduce
  /*
    sJWD->replace("ozone", "o1");
    sJWD->replace("pressure", "p1");
    sJWD->replace("visibility", "v1");
    sJWD->replace("humidity", "h1");
    sJWD->replace("description", "d1");
    sJWD->replace("dew_point", "d2");
    sJWD->replace("wind_speed", "w1");
    sJWD->replace("wind_deg", "w2");
    sJWD->replace("feels_like", "f1");*/
  Serial.print(" reduced to " + (String)(sJWD->length()) + " + <" + (String)(sJWH->length()) + "> = ");
  DynamicJsonBuffer jsonBufferO(1024);
  JsonObject& rootO = jsonBufferO.parseObject(*sJWD);
  if (!rootO.success()) {
    LogAlert("\nERROR bSummarizeOWMJsonWDays, no rootO " + sJWD->substring(0, 100) + "\n" + sJWD->substring(sJWD->length() - 100, sJWD->length()) + "\n", 1);
    jsonBufferO.clear();
    return false;
  }
  DynamicJsonBuffer jsonBufferN(1024);
  JsonObject& rootN = jsonBufferN.parseObject(*sJWH);
  if (!rootN.success()) {
    Serial.print("\nERROR bSummarizeOWMJsonWDays HOURS + DAYS, no rootN\n");
    jsonBufferO.clear();
    jsonBufferN.clear();
    return false;
  }
  for (int i = 0; i < 8; i++) { //icons
    String stmp1 = rootO["daily"][i]["weather"][0]["icon"];
    rootN["daily"]["icon" + (String)(i)] = stmp1;
    rootN["daily"]["tMin" + (String)(i)] = rootO["daily"][i]["temp"]["min"];
    rootN["daily"]["tMax" + (String)(i)] = rootO["daily"][i]["temp"]["max"];
  }
  String stmp3 = rootO["alerts"][0]["d1"];
  rootN["daily"]["summary"] = stmp3;
  *sJWD = "";
  *sJWH = "";
  rootN.printTo(*sJWD);
  Serial.print(" to " + (String)(sJWD->length()) + ".");
  jsonBufferN.clear();
  jsonBufferO.clear();
  return true;
}//////////////////////////////////////////////////////////////////////////////////////////////////
//lowers weather json size from 28k to 5k
bool bSummarizeDKSJsonWDays(String * sJWD, String * sJWPrev) {
  Serial.print("\n SUMMARY WJSON Days from " + (String)(sJWD->length()) + " ");
  //Reduce
  //Daily
  sJWD->replace("precipIntensityMaxTime", "d3");
  sJWD->replace("precipIntensityMax", "d2");
  sJWD->replace("apparentTemperatureHighTime", "d10");
  sJWD->replace("apparentTemperatureHigh", "d9");
  sJWD->replace("apparentTemperatureLow", "d11");
  sJWD->replace("apparentTemperatureMinTime", "d20");
  sJWD->replace("apparentTemperatureMin", "d19");
  sJWD->replace("apparentTemperatureMaxTime", "d22");
  sJWD->replace("apparentTemperatureMax", "d21");
  sJWD->replace("temperatureHighTime", "d6");
  sJWD->replace("temperatureHigh", "d5");
  sJWD->replace("temperatureLowTime", "d8");
  sJWD->replace("temperatureLow", "d7");
  sJWD->replace("temperatureMinTime", "d16");
  sJWD->replace("temperatureMin", "d15");
  sJWD->replace("temperatureMaxTime", "d18");
  sJWD->replace("temperatureMax", "d17");
  sJWD->replace("precipType", "d4");
  sJWD->replace("windGustTime", "d13");
  sJWD->replace("uvIndexTime", "d14");
  sJWD->replace("moonPhase", "d1");
  //hourly
  sJWD->replace("dewPoint", "dP1");
  sJWD->replace("windGust", "wG1");
  sJWD->replace("pressure", "p1");
  sJWD->replace("uvIndex", "uv1");
  sJWD->replace("visibility", "v1");
  sJWD->replace("apparentTemperature", "aT1");
  sJWD->replace("precipIntensity", "preI");
  sJWD->replace("precipProbability", "preP");
  sJWD->replace("humidity", "humI");
  sJWD->replace("windSpeed", "winS");
  sJWD->replace("windBearing", "winB");
  sJWD->replace("cloudCover", "cloC");
  sJWD->replace("temperature", "temP");
  sJWD->replace("ozone", "o1");
  Serial.print(" reduced to " + (String)(sJWD->length()) + " ");
  DynamicJsonBuffer jsonBufferO(1024);
  JsonObject& rootO = jsonBufferO.parseObject(*sJWD);
  if (!rootO.success()) {
    LogAlert("\nERROR SUMMARIZING, no rootO " + sJWD->substring(0, 100) + "\n" + sJWD->substring(sJWD->length() - 100, sJWD->length()) + "\n", 1);
    jsonBufferO.clear();
    return false;
  }
  DynamicJsonBuffer jsonBufferN(256);
  JsonObject& rootN = jsonBufferN.parseObject(*sJWPrev);
  if (!rootN.success()) {
    Serial.print("\nERROR SUMMARIZING, no rootHourly\n");
    jsonBufferO.clear();
    jsonBufferN.clear();
    return false;
  }
  JsonObject& NSubPath2 = rootN.createNestedObject("daily");
  rootN["daily"]["sunriseTime"] = rootO["daily"]["data"][0]["sunriseTime"];
  rootN["daily"]["sunsetTime"] = rootO["daily"]["data"][0]["sunsetTime"];
  for (int i = 0; i < 8; i++) {
    String stmp1 = rootO["daily"]["data"][i]["icon"];
    rootN["daily"]["icon" + (String)(i)] = stmp1;
    rootN["daily"]["tMin" + (String)(i)] = rootO["daily"]["data"][i]["apparentTemperatureMin"];
    rootN["daily"]["tMax" + (String)(i)] = rootO["daily"]["data"][i]["apparentTemperatureMax"];
  }
  String stmp3 = rootO["daily"]["summary"];
  rootN["daily"]["summary"] = stmp3;
  *sJWD = "";
  *sJWPrev = "";
  rootN.printTo(*sJWD);
  Serial.print("to Hours and Days " + (String)(sJWD->length()) + ".");
  jsonBufferO.clear();
  jsonBufferN.clear();
  return true;
}//////////////////////////////////////////////////////////////////////////////////////////////////
//lowers weather json size from 28k to 5k
bool bSummarizeDKSJsonWHours(String * sJWH) {
  int iAux;
  float fAux;
  Serial.print("\n SUMMARY WJSON Hours from " + (String)(sJWH->length()) + " ");
  //Reduce
  //hourly
  sJWH->replace("dewPoint", "dP1");
  sJWH->replace("windGust", "wG1");
  sJWH->replace("pressure", "p1");
  sJWH->replace("uvIndex", "uv1");
  sJWH->replace("visibility", "v1");
  sJWH->replace("apparentTemperature", "aT1");
  sJWH->replace("precipIntensity", "preI");
  sJWH->replace("precipProbability", "preP");
  sJWH->replace("humidity", "humI");
  sJWH->replace("windSpeed", "winS");
  sJWH->replace("windBearing", "winB");
  sJWH->replace("cloudCover", "cloC");
  sJWH->replace("temperature", "temP");
  sJWH->replace("ozone", "o1");
  Serial.print(" reduced to " + (String)(sJWH->length()) + " ");
  DynamicJsonBuffer jsonBufferO(1024);
  JsonObject& rootO = jsonBufferO.parseObject(*sJWH);
  if (!rootO.success()) {
    LogAlert("\nERROR SUMMARIZING, no rootO " + sJWH->substring(0, 100) + "\n" + sJWH->substring(sJWH->length() - 100, sJWH->length()) + "\n", 1);
    jsonBufferO.clear();
    return false;
  }
  DynamicJsonBuffer jsonBufferN(256);
  JsonObject& rootN = jsonBufferN.parseObject(" {}");
  if (!rootN.success()) {
    Serial.print("\nERROR SUMMARIZING, no rootN\n");
    jsonBufferO.clear();
    jsonBufferN.clear();
    return false;
  }
  JsonObject& NSubPath1 = rootN.createNestedObject("currently");
  JsonObject& NSubPath3 = rootN.createNestedObject("hourly");
  rootN["offset"] = rootO["offset"];
  for (int i = 0; i < ANALYZEHOURS; i++) {
    rootN["hourly"]["time" + (String)(i)] = rootO["hourly"]["data"][i]["time"];
    rootN["hourly"]["temp" + (String)(i)] = rootO["hourly"]["data"][i]["temP"];
    rootN["hourly"]["humi" + (String)(i)] = rootO["hourly"]["data"][i]["humI"];
    rootN["hourly"]["preI" + (String)(i)] = rootO["hourly"]["data"][i]["preI"];
    rootN["hourly"]["preP" + (String)(i)] = rootO["hourly"]["data"][i]["preP"];
    rootN["hourly"]["winS" + (String)(i)] = rootO["hourly"]["data"][i]["winS"];
    rootN["hourly"]["winB" + (String)(i)] = rootO["hourly"]["data"][i]["winB"];
    rootN["hourly"]["cloC" + (String)(i)] = rootO["hourly"]["data"][i]["cloC"];
    String stmp0 = rootO["hourly"]["data"][i]["icon"];
    rootN["hourly"]["icon" + (String)(i)] = stmp0;
  }
  fAux = rootO["currently"]["temP"];
  if (fAux == 0) fAux = rootN["hourly"]["temp0"];
  rootN["currently"]["temperature"] = fAux;
  iAux = rootO["currently"]["time"];
  if (iAux == 0) fAux = rootN["hourly"]["time0"];
  rootN["currently"]["time"] = iAux;
  String stmp2 = rootO["hourly"]["summary"];
  rootN["hourly"]["summary"] = stmp2;
  *sJWH = "";
  rootN.printTo(*sJWH);
  Serial.print("to Hours " + (String)(sJWH->length()) + ".");
  jsonBufferO.clear();
  jsonBufferN.clear();
  return true;
}//////////////////////////////////////////////////////////////////////////////////////////////////
bool iGetJsonFunctions() {
  Serial.print("|gF");
  String sAux;
  if (bGetJsonBool(jFunc, "Reboot", false)) {
    Serial.println("\nFunction REBOOT");
    jFunc.set("Reboot", false);
    bUFunc = true;
    SendToRestart();
  }
  if (bGetJsonBool(jFunc, "ToSleep", false)) {
    Serial.println("\nFunction ToSleep");
    jFunc.set("ToSleep", false);
    bUFunc = true;
    SendToSleep(0);
  }
  if (bGetJsonBool(jFunc, "EraseVMaxMin", false)) {
    Serial.println("\nFunction EraseVMaxMin");
    jFunc.set("EraseVMaxMin", false);
    bUFunc = true;
    EraseVMaxVmin();
    SendToSleep(0);
  }
  if (bGetJsonBool(jFunc, "FormatSPIFFS", false)) {
    Serial.println("\nFunction FormatSPIFFS");
    jFunc.set("FormatSPIFFS", false);
    bUFunc = true;
    FormatSPIFFS();
    SendToSleep(0);
  }
  if (bGetJsonBool(jFunc, "EraseSPIFFJson", false)) {
    Serial.println("\nFunction EraseSPIFFJson");
    bEraseSPIFFJson = true;
    deleteSPIFFSFile("/jFunc.txt");
    sAux = sGetJsonString(jFunc, "OTAUpdate", "[]");
    Firebase.deleteNode(firebaseData, "/dev/" + sMACADDR + "/Functions");
    jFunc.clear();
    jFunc.set("OTAUpdate", sAux);
    bUFunc = true;
    deleteSPIFFSFile("/jMisc.txt");
    jMisc.clear();
    iSetJsonMisc();
    bUMisc = true;
    deleteSPIFFSFile("/jVtg.txt");
    jVtg.clear();
    if (bHasBattery) {
      dGetVoltagePerc();
      bSetJsonVtg();
      bUVtg = true;
    }
    jLog.clear();
    deleteSPIFFSFile("/jDefLog.txt");
    bULog = false;
    deleteSPIFFSFile("/jVars.txt");
    bUVars = false;
    SendToSleep(0);
  } else       {
    //    jFunc.set("EraseSPIFFJson", false);
    //    bUFunc = true;
    bEraseSPIFFJson = false;
  }
  if (bGetJsonBool(jFunc, "EraseAllNVS", false)) {
    Serial.println("\nFunction EraseAllNVS");
    jFunc.set("EraseAllNVS", false);
    bUFunc = true;
    bEraseSPIFFJson = true;
    EraseAllNVS("Function");
    SendToSleep(0);
  } else       {
    //    jFunc.set("EraseAllNVS", false);
    //    bUFunc = true;
    bEraseSPIFFJson = false;
  }
  if (bGetJsonBool(jFunc, "EraseAllFB", false)) {
    Serial.println("\nFunction EraseAllFB");
    jFunc.set("EraseAllFB", false);
    bUFunc = true;
    delay(100);
    Firebase.deleteNode(firebaseData, "/dev/" + sMACADDR);
    delay(100);
    SendToSleep(0);
  }
  String sOtaBin ;
  sOtaBin = sGetJsonString(jFunc, "OTAUpdate", "[]");
  if (sOtaBin.length() > 5) {
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
        DisplayU8Text(1, 100, sTimeLocal(0) + " " + sDateLocal(sWeatherLNG, tNow), fU8g2_S);
        DisplayU8Text(1, 120, "Reset in 5 mins...", fU8g2_S);
        bRefreshPage();
        sSpecialCase += " ";
      }
      //NVSClose();
      if (execOTA(sOtaBin)) {
        sAux = "[" + sSpecialCase + "OK] " + sOtaBin ;
        EraseAllNVS("OTAUpdate");
        writeSPIFFSFile("/otaupdat.txt", sOtaBin.c_str());
      } else {
        sAux = "[" + sSpecialCase + "NOK] " + sOtaBin ;
      }
      jFunc.set("OTAUpdate", sAux);
      bUFunc = true;
      LogAlert((String)(REVISION) + "-> ~OTAUpdate DONE " + sGetJsonString(jFunc, "OTAUpdate", "") + " ~" , 3);
      SendToRestart();
    }
    sOtaBin = "";
  }
  Serial.print("|");
  return true;
}//////////////////////////////////////////////////////////////////////////////////////////////////
void EraseVMaxVmin() {
  LogAlert("~EraseVMaxMin~", 3);
  tTimeLastVtgMax = 0;
  tTimeLastVtgChg = 0;
  iVtgMax = 0;
  iVtgChg = 0;
  iVtgMin = 10000;
  bSetJsonVtg();
  delay(100);
  Firebase.deleteNode(firebaseData, "/dev/" + sMACADDR + "/Vtg");
  delay(10);
}//////////////////////////////////////////////////////////////////////////////////////////////////
void FormatSPIFFS() {
  LogAlert("~Format SPIFFS~", 3);
  listSPIFFSDir("/", 2, true);
  String sWifiTxt = readSPIFFSFile("/wifi.txt");
  Serial.println("---- - FORMATING ------");
  bool bRes = SPIFFS.format();
  Serial.println("---- - FORMATED " + (String)(bRes ? "true" : "false") + "------");
  if (sWifiTxt.length() > 3) writeSPIFFSFile("/wifi.txt", sWifiTxt.c_str());
  listSPIFFSDir("/", 2, true);
}//////////////////////////////////////////////////////////////////////////////////////////////////
void EraseAllNVS(String sMsg) {
  LogAlert("~EraseAllNVS [" + sDevID + "] " + sMsg + " ~", 3);
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
  //Serial.printf("\n dGetTempTime%d >%d >%d (%f >%f >%f) ", t, aHour[0], aHour[ANALYZEHOURS - 1], fCurrTemp, aTempH[0], aTempH[ANALYZEHOURS - 1]);
  if (aHour[0] > t) return aTempH[0];
  if ((aHour[ANALYZEHOURS - 1] > 0) && (t > aHour[ANALYZEHOURS - 1])) return aTempH[ANALYZEHOURS - 1];
  for (int i = 0; i < (ANALYZEHOURS - 1); i++) {
    if (t >= aHour[i]) {
      iIni = i;
      break;
    }
  }
  if (iIni > -1) {
    float val;
    val = aTempH[iIni] + ((t - aHour[iIni]) * (aTempH[iIni + 1] - aTempH[iIni])) / (aHour[iIni + 1] - aHour[iIni]);
    //Serial.printf(" ->%f \n", val);
    return val;
  } else return -100;
}//////////////////////////////////////////////////////////////////////////////////////////////////
float dGetWindSpdTime(long int t) {
  int iIni = -1;
  if (aHour[0] > t) return aWindSpd[0];
  if ((aHour[ANALYZEHOURS - 1] > 0) && (t > aHour[ANALYZEHOURS - 1])) return aWindSpd[ANALYZEHOURS - 1];
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
  if ((aHour[ANALYZEHOURS - 1] > 0) && (t > aHour[ANALYZEHOURS - 1])) return aWindBrn[ANALYZEHOURS - 1];
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
  if (6 > hour(tN)) {
    if (iDiff > (2 * 60 * iRefreshPeriod))       ret += iRefreshPeriod;
    else     if (iDiff > (90 * iRefreshPeriod))         ret = round(iDiff / 120);
    else         ret = round(iDiff / 60);
  }
  return (ret + 1);
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
  DisplayU8Text(1, 100, sTimeLocal(0) + " " + sDateLocal(sWeatherLNG, tNow), fU8g2_S);
  DisplayU8Text(1, 120, "Reset in 5 mins...", fU8g2_S);
  bRefreshPage();
  LogAlert("OTA Default " + sOTAName, 3);
  bool res = execOTA(sOTAName);
  EraseAllNVS("OTADefault");
  writeSPIFFSFile("/otaupdat.txt", sOTAName.c_str());
  return res;
}//////////////////////////////////////////////////////////////////////////////////////////////////
bool LogDef(String sText, int iLevel) {
  time_t tAlert = time(nullptr);
  if (iLevel >= iLogFlag ) {
    char buff[30];
    sprintf(buff, "%04d%02d%02d_%02d%02d%02d", year(tAlert), month(tAlert), day(tAlert), hour(tAlert), minute(tAlert), second(tAlert));
    String sAux = buff;
    jDefLog.set("L" + (String)(iLevel) + "_" + (String)(buff) + "D", sText);
    bUDefLog = true;
    delay(1100);
  }
  return true;
}//////////////////////////////////////////////////////////////////////////////////////////////////
bool LogAlert(String sText, int iLevel) {
  if (iLevel > 1)  Serial.println("\n" + sText);
  else Serial.print(", " + sText);
  time_t tAlert = time(nullptr);
  if (bWeHaveWifi) WriteLog(tAlert, sText, iLevel);
  else LogDef( sText,  iLevel);
  return true;
}//////////////////////////////////////////////////////////////////////////////////////////////////
bool WriteLog(time_t tAlert, String sText, int iLevel) {
  if (iLevel >= iLogFlag ) {
    char buff[30];
    sprintf(buff, "%04d%02d%02d_%02d%02d%02d", year(tAlert), month(tAlert), day(tAlert), hour(tAlert), minute(tAlert), second(tAlert));
    String sAux = buff;
    jLog.set("L" + (String)(iLevel) + "_" + (String)(buff), sText);
    bULog = true;
    delay(1100);
  }
  if ((iLevel > 2) && (bWeHaveWifi)) {
    String sAux;
    jVars.toString(sAux);
    jMisc.toString(sAux);
    jLog.toString(sAux);
    SendEmail("WESP32 > " + sDevID + " [" + String(iLevel) + ":" +  sText + "]" , sAux);
  }
  return true;
}//////////////////////////////////////////////////////////////////////////////////////////////////
bool FlushDefLog() {
  if (!bWeHaveWifi) return false;
  size_t count = jDefLog.iteratorBegin();
  String key, value;
  int type = 0;
  for (size_t i = 0; i < count; i++) {
    jDefLog.iteratorGet(i, type, key, value);
    jLog.set(key, value);
    jDefLog.remove(key);
  }
  return true;
}//////////////////////////////////////////////////////////////////////////////////////////////////
bool FBCheckLogLength() {
  //TODO
  static int iCycles = 0;
  int iSize;
  String sLog;
  jLog.toString(sLog, false);
  iSize = sLog.length();
  if (iSize > iLogMaxSize) {
    FirebaseJsonData jsonParseResult;
    size_t count = jLog.iteratorBegin();
    String sNum, key, value;
    int type = 0;
    jLog.iteratorGet(0, type, key, value);
    jLog.remove(key);
    sLog = "";
    jLog.toString(sLog, false);
    iSize = sLog.length();
    bULog = true;
    //   Serial.printf("->%d", iSize);
    if (iSize < iLogMaxSize) {
      //     Serial.print(". Done. \n OUT:" + sLog + "\n");
      return true;
    } else {
      iCycles++;
      FBCheckLogLength();
    }
  }
  iCycles = 0;
  return false;
}//////////////////////////////////////////////////////////////////////////////
bool bGetJsonVars() {
  String sAux1;
  Serial.print("|gV");
  sAux1 = sGetJsonString(jVars, "Id", sDevID);
  if (sAux1.length() == 0) sAux1 = sMACADDR;
  else {
    if (sAux1 == sMACADDR) {
      String sAux2;
      bFBGetStr(sAux2, "/dev/" + sMACADDR + "/Id");
      if (sAux2.length() == 0) {
        sAux2 = sMACADDR;
        bFBSetStr(sAux2, "/dev/" + sMACADDR + "/Id");
      } else {
        if (sAux2 != sMACADDR) sAux1 = sAux2;
      }
    }
  }
  sDevID = sAux1;
  sCustomText = sGetJsonString(jVars, "CustomText", sCustomText);
  sTimeFirst = sGetJsonString(jVars, "TimeFirst", sTimeFirst);
  sTimeFirst.replace("-", "");
  sTimeFirst.replace("+", "");
  sWeatherLOC = sGetJsonString(jVars, "WeatherLOC", sWeatherLOC);
  sWeatherAPI = sGetJsonString(jVars, "WeatherAPI", sWeatherAPI);
  sWeatherKEY = sGetJsonString(jVars, "WeatherKEY", sWeatherKEY);
  sWeatherLNG = sGetJsonString(jVars, "WeatherLNG", sWeatherLNG);
  sTimeZone = sGetJsonString(jVars, "WeatherTimeZone", sTimeZone);
  iRefreshPeriod = iGetJsonInt(jVars, "RefreshPeriod", iRefreshPeriod);
  iDailyDisplay = iGetJsonInt(jVars, "ShowDaily", iDailyDisplay);
  iLogMaxSize = iGetJsonInt(jVars, "LogMaxSize", iLogMaxSize);
  iLogFlag = iGetJsonInt(jVars, "LogFlag", iLogFlag);
  fTempInOffset = fGetJsonFlt(jVars, "TempInOffset", fTempInOffset);
  if (io_LED)      iLedLevel = iGetJsonInt(jVars, "LedLevel", iLedLevel);
  String sAux = sFormatGPSString(sWeatherLOC);
  if (sAux.length() > 5) {
    if (sAux != sWeatherLOC) {
      sWeatherLOC = sAux;
    }
  }
  Serial.print("|");
  return true;
}//////////////////////////////////////////////////////////////////////////////
bool  bGetJsonVtg() {
  Serial.print("|gT");
  int iAux = iGetJsonInt(jVtg, "VMax", iVtgMax);
  if (iAux > 0) iVtgMax = iAux;
  if (iVtgMax > VTGMAXIMALLOWED) iVtgMax = 0;
  iAux = iGetJsonInt(jVtg, "VChg", iVtgChg);
  if (iAux > 0) iVtgChg = iAux;
  if (iVtgChg > VTGMAXIMALLOWED) iVtgChg = 0;
  iAux = iGetJsonInt(jVtg, "VMin", iVtgMin);
  if (iAux > 0) iVtgMin = iAux;
  else     iVtgMin = 10000;
  tTimeLastVtgMax = iGetJsonInt(jVtg, "tLstMax", tTimeLastVtgMax);
  tTimeLastVtgChg = iGetJsonInt(jVtg, "tLstChg", tTimeLastVtgChg);
  tTimeLastVtgDown = iGetJsonInt(jVtg, "tLstDown", tTimeLastVtgDown);
  iVtgPeriodMax = iGetJsonInt(jVtg, "PeriodMax", iVtgPeriodMax);
  iVtgPeriodDrop = iGetJsonInt(jVtg, "PeriodDrop", iVtgPeriodDrop);
  if ((iVtgPeriodMax < 1) && (iVtgPeriodDrop < 1) ) {
    iVtgPeriodMax = 1000;
    iVtgPeriodDrop = 100;
  }
  if (iVtgPeriodMax < 1) iVtgPeriodMax = 1;
  if (iVtgPeriodDrop < 1) iVtgPeriodDrop = 1;
  iVtgStableMax = iGetJsonInt(jVtg, "StableMax", iVtgStableMax);
  if (1 > iVtgStableMax ) iVtgStableMax = iVtgMax * .9;
  iVtgStableMin = iGetJsonInt(jVtg, "StableMin", iVtgStableMin);
  if (1 > iVtgStableMin) {
    if ((iVtgMin > 1) && (iVtgMax > 1)) iVtgStableMin = (iVtgMax + iVtgMin) / 2;
    else     iVtgStableMin = 3000;
  }
  Serial.print("|");
  return true;
}//////////////////////////////////////////////////////////////////////////////
bool  bSetJsonVtg() {
  Serial.print("|sT");
  jVtg.clear();
  jVtg.set("VMax", iVtgMax);
  jVtg.set("VChg", iVtgChg);
  jVtg.set("VMin", iVtgMin);
  jVtg.set("tLstMax", tTimeLastVtgMax);
  jVtg.set("tLstChg", tTimeLastVtgChg);
  jVtg.set("tLstDown", tTimeLastVtgDown);
  jVtg.set("PeriodMax", iVtgPeriodMax);
  jVtg.set("PeriodDrop", iVtgPeriodDrop);
  jVtg.set("StableMax", iVtgStableMax);
  jVtg.set("StableMin", iVtgStableMin);
  iSetJsonVtgSlopes(iVtgVal, tVtgTime, VTGMEASSURES);
  bUVtg = true;
  Serial.print("|");
  return true;
}//////////////////////////////////////////////////////////////////////////////
int iSetJsonVtgSlopes(int32_t* iVtgVal, int32_t* tVtgTime, int iVTGMEASSURES) {
  String sName;
  int zeros = 0;
  for (int i = 0; i < iVTGMEASSURES; i++) {
    sName = "VtgVal" + (String)(int)(i);
    jVtg.set("VHist/" + sName, iVtgVal[i]);
    sName = "VtgTime" + (String)(int)(i);
    jVtg.set("VHist/" + sName, tVtgTime[i]);
    if (tVtgTime[i] == 0)       zeros++;
  }
  bUVtg = true;
  return zeros;
}//////////////////////////////////////////////////////////////////////////////
int iGetJsonVtgSlopes(int32_t* iVtgVal, int32_t* tVtgTime, int iVTGMEASSURES) {
  String sName;
  int zeros = 0;
  FirebaseJsonData jsonData;
  for (int i = 0; i < iVTGMEASSURES; i++) {
    sName = "VtgVal" + (String)(int)(i);
    jVtg.get(jsonData, "VHist/" + sName) ;
    iVtgVal[i] = jsonData.intValue;
    sName = "VtgTime" + (String)(int)(i);
    jVtg.get(jsonData, "VHist/" + sName) ;
    tVtgTime[i] = jsonData.intValue;
    if (tVtgTime[i] == 0)       zeros++;
  }
  return zeros;
}//////////////////////////////////////////////////////////////////////////////
bool bCheckWifiSPIFFSFile() {
  if (!SPIFFS.exists("/wifi.txt"))  {
    if (!writeSPIFFSFile("/wifi.txt", sWifiDefaultJson.c_str())) return false;
  }
  return true;
}
//////////////////////////////////////////////////////////////////////////////
bool bCheckAWSSPIFFSFiles() {
  /*
    if (!SPIFFS.exists("/edit.htm"))  {
    if (!DowloadFromAWSToSpiffs("data/edit.htm", "/edit.htm")) return false;
    }
    if (!SPIFFS.exists("/ace.js"))    {
    if (!DowloadFromAWSToSpiffs("data/ace.js", "/ace.js")) return false;
    }
    if (!SPIFFS.exists("/mode-html.js"))  {
    if (!DowloadFromAWSToSpiffs("data/mode-html.js", "/mode-html.js")) return false;
    }*/
  if (!SPIFFS.exists("/edit.htm"))  DowloadFromAWSToSpiffs("data/edit.htm", "/edit.htm");
  if (!SPIFFS.exists("/ace.js"))    DowloadFromAWSToSpiffs("data/ace.js", "/ace.js");
  if (!SPIFFS.exists("/mode-html.js")) DowloadFromAWSToSpiffs("data/mode-html.js", "/mode-html.js");
  return true;
}//////////////////////////////////////////////////////////////////////////////
bool bCheckInternalTemp() {
  int iSensorType = -1;
  float fFactor = 1;
  //Serial.print(" ~ Internal Temp: ");
  delay(10);
  bInsideTempSensor = false;
#ifdef DallasTemperature_h
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
#endif
  if (!bInsideTempSensor && io_TMP36) {
    int iAux = 0;
    for (int i = 0; i < 10; i++) {
      delay(20);
      iAux = iAux + analogRead(io_TMP36);
    }
    delay(20);
    iAux = iAux / 10;
    fInsideTemp = ((float(iAux) * 3.3 / 1024.0) - 0.5) * 10;
    //FACTOR
    //Serial.printf("[%d/% d]", iVtgVal[VTGMEASSURES - 1], iVtgMax);
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
  //Serial.print((String)((iSensorType != -1) ? ((String)((iSensorType == 1) ? "DS18B20 = " : "TMP36 = ") + (String)(fInsideTemp)) : "NO SENSOR ") + "ºC ~ [vFACTOR:" + (String)(fFactor) + "]\n");
  if (abs(fInsideTemp) > 50) bInsideTempSensor = false;
  return bInsideTempSensor;
}//////////////////////////////////////////////////////////////////////////////////////////////////
#ifdef ForceClock
String CheckLed() {
  if (!bClk) return " - ";
  if ((!tNow) || (!tSunset) || (!tSunrise)) return " - ";
  static int iLastLevel = 0;
  int iLevel = 0, iNow, iSet, iRise;
  bool bOn = false;
  String sMsg = "LED [" + (String)((tNow - tSunrise) / 60) + "_" + (String)((tNow - tSunset) / 60) + ", "; //{" + sTimeLocal(tSunrise) + "->" + sTimeLocal(tSunset) + "} ";
  if ((io_LED) && (tNow) && (iLedLevel)) {
    if ((tNow - tSunrise) > 86400) tSunrise += 86400;
    if ((tNow - tSunset) > 86400) tSunset += 86400;
    iNow = iMinFrom000(tNow);
    iSet = iMinFrom000(tSunset);
    iRise = iMinFrom000(tSunrise);
    float fMinA[] = {0, 120, 180, 240, 300, 360, iRise - 30, iRise + 30, iSet - 30, iSet + 30, 1440};
    float fLvlA[] = {8, 6  , 4  , 0  , 0  , 4  , 10   , 0         , 0        , 10  , 8   };
    int i;
    for (i = 0; i < 10; i++) {
      if (fMinA[i + 1] >= iNow) break;
    }
    float fFactor = (iNow - fMinA[i]) / (fMinA[i + 1] - fMinA[i]) * (fLvlA[i + 1] - fLvlA[i]) + fLvlA[i];
    iLevel = iLedLevel * fFactor / 10;
    if (iLevel != iLastLevel) {
      ledcWrite(0, iLevel);
      sMsg = sMsg + ",CHG: " + (String)(iLastLevel) + "->" + (String)(iLevel) + " ";
      iLastLevel = iLevel;
    } else  sMsg = sMsg + ",NO_CHG ";
  } else   sMsg = sMsg + "NO LED]";
  sMsg = sMsg + ", io_LED=" + (String)(io_LED) + ",iLedLevel=" + (String)(iLevel);
  Serial.println(sMsg);
  return sMsg;
}
#endif
//////////////////////////////////////////////////////////////////////////////////////////////////
int iBattPercentage(int iVtg) {
  int iAuxVStableMax, iAuxVStableMin, iBattPerc;
  if (10 > iVtgStableMax) iAuxVStableMax = iVtgMax * .97;
  else iAuxVStableMax = iVtgStableMax;
  if (10 > iVtgStableMin) iAuxVStableMin = iVtgMax * .88;
  else iAuxVStableMin = iVtgStableMin;
  if (iAuxVStableMax > iAuxVStableMin)  iBattPerc = 100 * (iVtg - iAuxVStableMin) / (iAuxVStableMax - iAuxVStableMin) ; //100% @ 96%Vtg and 0% @ 88%Vtg
  else iBattPerc = 0;
  if (iBattPerc > 100) iBattPerc = 100;
  if (iBattPerc < 0) iBattPerc = 0;
  return iBattPerc;
}//////////////////////////////////////////////////////////////////////////////////////////////////
int iBattPercentageCurve(int iVtg) { //Polynomial from David Bird
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
  //iAuxVPeriodDrop = iVtgPeriodDrop - (iVtgStableMin - iVtgMin);
  float fChgMax;
  if (iVtgPeriodDrop) fChgMax = (float)(iVtgChg - iAuxVStableMin) / (float)(iVtgPeriodDrop); //last 15% is the drop
  else fChgMax = 0;
  float fEnd = tTimeLastVtgChg + (fChgMax * iVtgPeriodMax * 3600);
  float fRem = fEnd - tNow;
  float fPerc ;
  if (fEnd > tTimeLastVtgChg) fPerc = fRem / (fEnd - tTimeLastVtgChg);
  else fPerc = 0;
  if (fRem < 0) fRem = 0;
  //if (fRem>100) fRem=99;
  //Serial.printf("\n iRemDaysTime: iBatStat=%d, (%d,%d) iVSMax=%d,iVSMin=%d iAuxPeriodDrop=%d fChgMax=%f, fEnd=%f, fRem=%f, fPerc=%f\n",iBattStatus,iVtgStableMax,iVtgStableMin,iAuxVStableMax,iAuxVStableMin,iAuxVPeriodDrop,fChgMax,fEnd,fRem,fPerc);
  return (int)(fRem / 86400);
}//////////////////////////////////////////////////////////////////////////////
void SendEmail(String sSubject, String sText) {
#ifdef G_SENDER
  if (strlen(sFROMEMAIL) > 0) {
    Gsender *gsender = Gsender::Instance();    //Getting pointer to class instance
    gsender->SetParams(sEMAILBASE64_LOGIN, sEMAILBASE64_PASSWORD, sFROMEMAIL );
    if (gsender->Subject(sSubject)->Send(sEmailDest, sText)) {
      Serial.println("Message send.");
    } else {
      Serial.print("Error sending message: ");
      Serial.println(gsender->getError());
    }
  }
#endif
}//////////////////////////////////////////////////////////////////////////////
String sFormatGPSString(String sGPS) {
  int iLat = 0, iLon = 0;
  int iPos = sGPS.indexOf(",");
  if (!iPos) return "0,0";
  iLat = sGPS.substring(0, iPos).toFloat() * 100;
  iLon = sGPS.substring(iPos + 1).toFloat() * 100;
  String sAux;
  sAux = (String)(iLat / 100) + "." + (String)(abs(iLat % 100)) + "," + (String)(iLon / 100) + "." + (String)(abs(iLon % 100));
  //Serial.println("\n GPS: " + sGPS + " -> " + sAux + "\n");
  if (!iLat && !iLon) {
    Serial.print("\n\n ERROR: sWeatherLOC not valid!!\n\n");
  }
  return sAux;
}//////////////////////////////////////////////////////////////////////////////
bool bCheckLowVoltage () {
  String sLowVolt = readSPIFFSFile("/lowvolt.txt");
  int iLowVolt = sLowVolt.toInt();
  if (iLowVolt > 1) {
    int iDiff = (60 * 100 * (iLowVolt - analogRead(io_VOLTAGE))) / iLowVolt;
    if (iDiff < 60) iDiff = 60; //Increase one hour each 1% of difference
    if (analogRead(io_VOLTAGE) > (iLowVolt * 1.2)) {
      deleteSPIFFSFile("/lowvolt.txt");
      SendToRestart();
    } else {
      SendToSleep(iDiff);
    }
  }
  return true;
}
//////////////////////////////////////////////////////////////////////////////
bool bCheckSJson(String * sJson) {
  Serial.print(", bCheckSJson " + (String)(sJson->length()) + " ");
  DynamicJsonBuffer jsonBufferO(256);
  JsonObject& rootO = jsonBufferO.parseObject(*sJson);
  if (!rootO.success()) {
    Serial.print(" NOk! \n");
    return false;
  } else {
    Serial.print(" Ok ");
    return true;
  }
}//////////////////////////////////////////////////////////////////////////////
float fGetJsonFlt(FirebaseJson & jJson, String sPath, float fDef) {
  FirebaseJsonData jsonObj;
  jJson.get(jsonObj, sPath);
  if (jsonObj.typeNum == 0) {
    jJson.set(sPath, fDef);
    return fDef;
  } else return jsonObj.floatValue;
}//////////////////////////////////////////////////////////////////////////////
double dGetJsonDbl(FirebaseJson & jJson, String sPath, double dDef) {
  FirebaseJsonData jsonObj;
  jJson.get(jsonObj, sPath);
  if (jsonObj.typeNum == 0) {
    jJson.set(sPath, dDef);
    return dDef;
  } else return jsonObj.doubleValue;
}//////////////////////////////////////////////////////////////////////////////
int iGetJsonInt(FirebaseJson & jJson, String sPath, int iDef) {
  FirebaseJsonData jsonObj;
  jJson.get(jsonObj, sPath);
  if (jsonObj.typeNum == 0) {
    jJson.set(sPath, iDef);
    return iDef;
  } else return jsonObj.intValue;
}//////////////////////////////////////////////////////////////////////////////
String sGetJsonString(FirebaseJson & jJson, String sPath, String sDef) {
  FirebaseJsonData jsonObj;
  jJson.get(jsonObj, sPath);
  if (jsonObj.typeNum == 0) {
    jJson.set(sPath, sDef);
    return sDef;
  } else return jsonObj.stringValue;
}//////////////////////////////////////////////////////////////////////////////
bool bGetJsonBool(FirebaseJson & jJson, String sPath, bool bDef) {
  FirebaseJsonData jsonObj;
  jJson.get(jsonObj, sPath);
  if (jsonObj.typeNum == 0) {
    jJson.set(sPath, bDef);
    return bDef;
  } else return jsonObj.boolValue;
}//////////////////////////////////////////////////////////////////////////////
int iSizeJson(FirebaseJson & jJson) {
  String jsonStr;
  jJson.toString(jsonStr, true);
  return (jsonStr.length());
}//////////////////////////////////////////////////////////////////////////////
bool bFBCheckUpdateJsons() {
  if (!bWeHaveWifi) return false;
  if (bUVars) {
    bFBSetjVars();
    bUVars = false;
  }
  if (bUFunc) {
    bFBSetjFunc();
    bUFunc = false;
  }
  if (bUVtg) {
    bFBSetjVtg();
    bUVtg = false;
  }
  if (bUDefLog) {
    FlushDefLog();
    bSPFSSetjDefLog();
    bUDefLog = false;
  }
  if (bULog) {
    bFBSetjLog();
    bULog = false;
  }
  if (bUMisc) {
    bFBSetjMisc();
    bUMisc = false;
  }
  if (!SPIFFS.exists("/jVars.txt")) {
    String sAux = "";
    if (iSizeJson(jVars) > 3) {
      jVars.toString(sAux, true);
      writeSPIFFSFile("/jVars.txt", sAux.c_str());
    }
  }
  return true;
}//////////////////////////////////////////////////////////////////////////////
bool bFBSetjVars() {
  Serial.print("|fV");
  delay(100);
  if (bWeHaveWifi) Firebase.updateNodeSilent(firebaseData, "/dev/" + sMACADDR + "/vars", jVars);
  delay(100);
  String sAux = "";
  if (iSizeJson(jVars) > 3) {
    jVars.toString(sAux, true);
    writeSPIFFSFile("/jVars.txt", sAux.c_str());
  }
  bUVars = false;
  Serial.print("|");
  return true;
}//////////////////////////////////////////////////////////////////////////////
bool bFBSetjMisc() {
  Serial.print("|fM");
  delay(100);
  if (bWeHaveWifi) {
    if ((bResetBtnPressed) || (hour(tNow) < 2)) { //once a day
      //    if ((hour(tNow) < 2)) {
      Firebase.setJSON(firebaseData, "/dev/" + sMACADDR + "/Misc", jMisc);
    } else {
      Firebase.updateNodeSilent(firebaseData, "/dev/" + sMACADDR + "/Misc", jMisc);
    }
  }
  delay(100);
  bUMisc = false;
  Serial.print("|");
  return true;
}//////////////////////////////////////////////////////////////////////////////
bool bFBSetjVtg() {
  Serial.print("|fT");
  delay(100);
  if (bWeHaveWifi) Firebase.setJSON(firebaseData, "/dev/" + sMACADDR + "/Vtg", jVtg);
  delay(100);
  String sAux = "";
  if (iSizeJson(jVtg) > 3) {
    jVtg.toString(sAux, true);
    writeSPIFFSFile("/jVtg.txt", sAux.c_str());
  }
  bUVtg = false;
  Serial.print("|");
  return true;
}//////////////////////////////////////////////////////////////////////////////
bool bFBSetjLog() {
  Serial.print("|fL");
  FBCheckLogLength();
  delay(100);
  if (bWeHaveWifi) {
    Firebase.updateNodeSilent(firebaseData, "/dev/" + sMACADDR + "/Log", jLog);
    jLog.clear();
  }
  delay(100);
  /*
    String sAux = "";
    if (iSizeJson(jLog) > 3) {
      jLog.toString(sAux, true);
      writeSPIFFSFile("/jLog.txt", sAux.c_str());
    }*/
  bULog = false;
  Serial.print("|");
  return true;
}//////////////////////////////////////////////////////////////////////////////
bool bSPFSSetjDefLog() {
  delay(500);
  String sAux = "";
  if (iSizeJson(jDefLog) > 3) {
    jDefLog.toString(sAux, true);
    writeSPIFFSFile("/jDefLog.txt", sAux.c_str());
  } else deleteSPIFFSFile("/jDefLog.txt");
  bUDefLog = false;
  return true;
}//////////////////////////////////////////////////////////////////////////////
bool bFBSetjFunc() {
  Serial.print("|fF");
  delay(100);
  if (bWeHaveWifi) Firebase.setJSON(firebaseData, "/dev/" + sMACADDR + "/Functions", jFunc);
  bUFunc = false;
  Serial.print("|");
  return true;
}
//////////////////////////////////////////////////////////////////////////////
bool bFBGetJson(FirebaseJson & jJson, String sPath, bool bClear) {
  delay(100);
  Firebase.get(firebaseData, sPath); //ERROR HERE when FB Json is wrong
  delay(100);
  if (firebaseData.dataType() == "json") {
    delay(100);
    String jsonStr = firebaseData.jsonString();
    delay(100);
    if (jsonStr.length() > 3) {
      if (bClear) jJson.clear();
      jJson.setJsonData(jsonStr);
    }
    return true;
  } else {
    Serial.print("\n ERROR bFBGetJson:'" + sPath + "' type=" + (String)(firebaseData.dataType()) );
    return false;
  }
}//////////////////////////////////////////////////////////////
bool bWriteSPIFFSVarsVtg() {
  String sAux = "";
  if (iSizeJson(jVars) > 3) {
    jVars.toString(sAux, true);
    writeSPIFFSFile("/jVars.txt", sAux.c_str());
  }
  if (iSizeJson(jVtg) > 3) {
    sAux = "";
    jVtg.toString(sAux, true);
    writeSPIFFSFile("/jVtg.txt", sAux.c_str());
  }
  return true;
}/////////////////////////////////////////////////////////////
bool bFBGetStr(String & sData, String sPath, bool bCreate) {
  delay(100);
  Firebase.get(firebaseData,  sPath);
  delay(100);
  if (firebaseData.dataType() == "string") {
    delay(100);
    sData = firebaseData.stringData();
    return true;
  } else {
    Serial.print("\n ERROR bFBGetStr:'" + sPath + "' type=" + (String)(firebaseData.dataType()) + " <" + firebaseData.stringData() + ">");
    delay(100);
    if (bCreate) Firebase.setString(firebaseData, sPath, sData);
    return false;
  }
}/////////////////////////////////////////////////////////////
bool bFBGetInt(int *iData, String sPath, bool bCreate) {
  delay(100);
  Firebase.get(firebaseData,  sPath);
  delay(100);
  if (firebaseData.dataType() == "int") {
    delay(100);
    *iData = firebaseData.intData();
    delay(100);
    return true;
  } else {
    Serial.print("\n ERROR bFBGetInt:'" + sPath + "' type=" + (String)(firebaseData.dataType()) );
    delay(100);
    if (bCreate) Firebase.setInt(firebaseData, sPath, *iData);
    return false;
  }
}/////////////////////////////////////////////////////////////
bool bFBGetFlt(float * fData, String sPath, bool bCreate) {
  delay(100);
  Firebase.get(firebaseData, sPath);
  delay(100);
  if (firebaseData.dataType() == "float") {
    delay(100);
    *fData = firebaseData.floatData();
    delay(100);
    return true;
  } else {
    Serial.print("\n ERROR bFBGetFlt:'" + sPath + "' type=" + (String)(firebaseData.dataType()) );
    delay(100);
    if (bCreate) Firebase.setFloat(firebaseData, sPath, *fData);
    return false;
  }
}//////////////////////////////////////////////////////////////////////////////////////////////////
bool bFBSetJson(FirebaseJson & jJson, String sPath) {
  delay(100);
  Firebase.updateNode(firebaseData, sPath, jJson);
  return true;
}//////////////////////////////////////////////////////////////////////////////
bool bFBSetFlt(float fData, String sPath) {
  delay(100);
  return Firebase.setFloat(firebaseData, sPath, fData);
}//////////////////////////////////////////////////////////////////////////////
bool bFBSetInt(int iData, String sPath) {
  delay(100);
  return Firebase.setInt(firebaseData,  sPath, iData);
}//////////////////////////////////////////////////////////////////////////////
bool bFBSetStr(String sData, String sPath) {
  delay(100);
  return Firebase.setString(firebaseData, sPath, sData);
}//////////////////////////////////////////////////////////////////////////////
void testWifi() {

}//////////////////////////////////////////////////////////////////////////////
void testIni() {
  /*
  int tMidnight = tNow - iSecFrom000(tNow);
  String str;
  for (int i = 0; i < 120; i++) {
    tNow = (i * 12 * 60) + tMidnight;
    str = sInt32TimetoStr(tNow);
    CheckLed();
    delay(100);
    //    display.fillScreen(GxEPD_WHITE);
    //    DisplayU8TextAlignBorder(40, 40 , str, fU8g2_XL, 0, 0, GxEPD_BLACK);
    //    delay(1000);
    //    display.update();
    //    bPartialDisplayUpdate();
    */
}//////////////////////////////////////////////////////////////////////////////
