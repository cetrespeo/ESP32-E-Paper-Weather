#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiMulti.h>
#include <WebServer.h>
#include <SPIFFS.h>
#include "ESP32_Disp_Aux.h"
#include <esp_wifi.h>

#define WD_WEBSERVER
#define FORMAT_SPIFFS false

bool setWSPlatform(String sPlat); 

void startWebServer(void);
