#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiMulti.h>
#include <WebServer.h>
#include <SPIFFS.h>
#include "ESP32_Disp_Aux.h"
#include <ArduinoJson.h>
#include <esp_wifi.h>

#define FORMAT_SPIFFS false

//bool wifiMultiRun();
String getAWifiSSID(int iNum);
String getAWifiPSWD(int iNum);
void startWebServer(void);
bool bAddWifiMulti(String sSsid, String sPwd);
int iLoadWifiMulti();
bool setWSPlatform(String sPlat); 
