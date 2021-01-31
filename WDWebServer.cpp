#include "WDWebServer.h"

WebServer server(80);
File fsUploadFile;
bool bExit = false;
String sHtmlCode = "";
String aWifiSSIDs[10];
String aWifiPSWDs[10];
String sWSPlatform = "";

//////////////////////////////////////////////////////////////////////////////
bool setWSPlatform(String sPlat) {
  sWSPlatform = sPlat;
  return true;
}
//////////////////////////////////////////////////////////////////////////////
String getAWifiSSID(int iNum) {
  if ((iNum > -1) &&  (iNum < 10))  return aWifiSSIDs[iNum];
  else return "";
}
//////////////////////////////////////////////////////////////////////////////
String getAWifiPSWD(int iNum) {
  if ((iNum > -1) &&  (iNum < 10))  return aWifiPSWDs[iNum];
  else return "";
}
//////////////////////////////////////////////////////////////////////////////
bool bAddWifiMulti(String sSsid, String sPwd) {
  String sAuxSSID, sAuxPSWD, sFSSsid,sFSPswd="";
  int i ;
  if ((sSsid.length() < 1) || (sPwd.length() < 1)) return false;
  String sWifiJson = readSPIFFSFile("/wifi.txt");
  if (!sWifiJson.length()) sWifiJson = "{}";
  DynamicJsonBuffer jsonBuffer(256);
  JsonObject& root = jsonBuffer.parseObject(sWifiJson);
  if (!root.success()) {
//    LogAlert(" No ROOT on bAddWifiMulti", 2);
    return false;
  }
  root[sSsid] = sPwd;
  sWifiJson = "";
  root.printTo(sWifiJson);
  Serial.println("\n" + sWifiJson);
  writeSPIFFSFile("/wifi.txt", sWifiJson.c_str());
  return true;
}
//////////////////////////////////////////////////////////////////////////////
int iLoadWifiMulti() {
  String sAux, sFSSsid, sFSPswd;
  int i=0;
  String sWifiJson = readSPIFFSFile("/wifi.txt");
  if (!sWifiJson.length()) return 0;
  if (sWifiJson.startsWith("{\"WIFISSID0")) return 0;
  DynamicJsonBuffer jsonBuffer(256);
  JsonObject& root = jsonBuffer.parseObject(sWifiJson);
  if (!root.success()) {
    //    LogAlert(" No ROOT on iLoadWifiMulti", 2);
    return false;
  }
  for (JsonObject::iterator it = root.begin(); it != root.end(); ++it) {
    sFSSsid = it->key;
    if (sFSSsid.length() > 0) {
      sFSPswd = (String)(it->value.as<char*>());
      aWifiSSIDs[i] = sFSSsid;
      aWifiPSWDs[i] = sFSPswd;
//      Serial.println(" Wifi added; SSID:[" + (String)(it->key) + "], PWD:["+(String)(it->value.as<char*>())+"]");
      i++;
    }
  }
  return i;
}
////////////////////////////////////////////////////////////////////////////
//format bytes
String formatBytes(size_t bytes) {
  if (bytes < 1024) {
    return String(bytes) + "B";
  } else if (bytes < (1024 * 1024)) {
    return String(bytes / 1024.0) + "KB";
  } else if (bytes < (1024 * 1024 * 1024)) {
    return String(bytes / 1024.0 / 1024.0) + "MB";
  } else {
    return String(bytes / 1024.0 / 1024.0 / 1024.0) + "GB";
  }
}
////////////////////////////////////////////////////////////////////////////
String getContentType(String filename) {
  if (server.hasArg("download")) {
    return "application/octet-stream";
  } else if (filename.endsWith(".htm")) {
    return "text/html";
  } else if (filename.endsWith(".html")) {
    return "text/html";
  } else if (filename.endsWith(".css")) {
    return "text/css";
  } else if (filename.endsWith(".js")) {
    return "application/javascript";
  } else if (filename.endsWith(".png")) {
    return "image/png";
  } else if (filename.endsWith(".gif")) {
    return "image/gif";
  } else if (filename.endsWith(".jpg")) {
    return "image/jpeg";
  } else if (filename.endsWith(".ico")) {
    return "image/x-icon";
  } else if (filename.endsWith(".xml")) {
    return "text/xml";
  } else if (filename.endsWith(".pdf")) {
    return "application/x-pdf";
  } else if (filename.endsWith(".zip")) {
    return "application/x-zip";
  } else if (filename.endsWith(".gz")) {
    return "application/x-gzip";
  }
  return "text/plain";
}
////////////////////////////////////////////////////////////////////////////
bool exists(String path) {
  bool yes = false;
  File file = SPIFFS.open(path, "r");
  if (!file.isDirectory()) {
    yes = true;
  }
  file.close();
  return yes;
}
////////////////////////////////////////////////////////////////////////////
bool handleFileRead(String path) {
  Serial.println("handleFileRead: " + path);
  //  if (path.endsWith("/")) {
  //    path += "index.htm";
  //  }
  if (path == "/index.htm") return true;
  String contentType = getContentType(path);
  String pathWithGz = path + ".gz";
  if (exists(pathWithGz) || exists(path)) {
    if (exists(pathWithGz)) {
      path += ".gz";
    }
    File file = SPIFFS.open(path, "r");
    server.streamFile(file, contentType);
    file.close();
    return true;
  }
  return false;
}
////////////////////////////////////////////////////////////////////////////
void handleFileUpload() {
  if (server.uri() != "/edit") {
    return;
  }
  HTTPUpload& upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    String filename = upload.filename;
    if (!filename.startsWith("/")) {
      filename = "/" + filename;
    }
    Serial.print("handleFileUpload Name: "); Serial.println(filename);
    fsUploadFile = SPIFFS.open(filename, "w");
    filename = String();
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    //Serial.print("handleFileUpload Data: "); Serial.println(upload.currentSize);
    if (fsUploadFile) {
      fsUploadFile.write(upload.buf, upload.currentSize);
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (fsUploadFile) {
      fsUploadFile.close();
    }
    Serial.print("handleFileUpload Size: "); Serial.println(upload.totalSize);
  }
}
////////////////////////////////////////////////////////////////////////////
void handleFileDelete() {
  if (server.args() == 0) {
    return server.send(500, "text/plain", "BAD ARGS");
  }
  String path = server.arg(0);
  Serial.println("handleFileDelete: " + path);
  if (path == "/") {
    return server.send(500, "text/plain", "BAD PATH");
  }
  if (!exists(path)) {
    return server.send(404, "text/plain", "FileNotFound");
  }
  if ((path != "/edit.htm") && (path != "/index.htm") && (path != "/ace.js") && (path != "/mode-html.js")) { // Cannot remove main files
    SPIFFS.remove(path);
  }
  server.send(200, "text/plain", "");
  path = String();
}
////////////////////////////////////////////////////////////////////////////
void handleFileCreate() {
  if (server.args() == 0) {
    return server.send(500, "text/plain", "BAD ARGS");
  }
  String path = server.arg(0);
  Serial.println("handleFileCreate: " + path);
  if (path == "/") {
    return server.send(500, "text/plain", "BAD PATH");
  }
  if (exists(path)) {
    return server.send(500, "text/plain", "FILE EXISTS");
  }
  File file = SPIFFS.open(path, "w");
  if (file) {
    file.close();
  } else {
    return server.send(500, "text/plain", "CREATE FAILED");
  }
  server.send(200, "text/plain", "");
  path = String();
}
////////////////////////////////////////////////////////////////////////////
void handleFileList() {
  if (!server.hasArg("dir")) {
    server.send(500, "text/plain", "BAD ARGS");
    return;
  }
  String path = server.arg("dir");
  Serial.println("handleFileList: " + path);
  File root = SPIFFS.open(path);
  path = String();
  String output = "[";
  if (root.isDirectory()) {
    File file = root.openNextFile();
    while (file) {
      if (output != "[") {
        output += ',';
      }
      output += "{\"type\":\"";
      output += (file.isDirectory()) ? "dir" : "file";
      output += "\",\"name\":\"";
      output += String(file.name()).substring(1);
      output += "\"}";
      file = root.openNextFile();
    }
  }
  output += "]";
  server.send(200, "text/json", output);
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void startWebServer(void) {
  bExit = false;
  Serial.begin(115200);
  Serial.print("\n");
  Serial.setDebugOutput(true);
  if (FORMAT_SPIFFS) SPIFFS.format();
  SPIFFS.begin();
  {
    File root = SPIFFS.open("/");
    File file = root.openNextFile();
    while (file) {
      String fileName = file.name();
      size_t fileSize = file.size();
      Serial.printf("FS File: %s, size: %s\n", fileName.c_str(), formatBytes(fileSize).c_str());
      file = root.openNextFile();
    }
    Serial.printf("\n");
  }
  //SERVER INIT
  //list directory
  server.on("/list", HTTP_GET, handleFileList);
  //load editor
  server.on("/edit", HTTP_GET, []() {
    if (!handleFileRead("/edit.htm")) {
      server.send(404, "text/plain", "FileNotFound");
    }
  });
  //create file
  server.on("/edit", HTTP_PUT, handleFileCreate);
  //delete file
  server.on("/edit", HTTP_DELETE, handleFileDelete);
  //first callback is called after the request has ended with all parsed arguments
  //second callback handles file uploads at that location
  server.on("/edit", HTTP_POST, []() {
    server.send(200, "text/plain", "");
  }, handleFileUpload);
  //called when the url is not defined here
  //use it to load content from SPIFFS
  server.onNotFound([]() {
    if (!handleFileRead(server.uri())) {
      server.send(404, "text/plain", "FileNotFound");
    }
  });
  server.on("/", HTTP_GET, []() {
    Serial.printf("'/' with %d args\n", server.args());
    if (server.args() != 0) {
      if (server.hasArg("ButtonReboot")) {
        Serial.println("ButtonReboot");
        sHtmlCode = "<!DOCTYPE html><html>";
        sHtmlCode += "<head><title></title>";
        sHtmlCode += "  <meta content=\"\\&quot;width=device-width,\" initial-scale=\"1\\&quot;\" name=\"\\&quot;viewport\\&quot;\" />";
        sHtmlCode += "  <link href=\"\\&quot;data:,\\&quot;\" rel=\"\\&quot;icon\\&quot;\" /></head><body>";
        sHtmlCode += "<h1 style=\"text-align: center;\"><span style=\"font-family:tahoma,geneva,sans-serif;\">Rebooting</span></h1>";
        sHtmlCode += "</body></html>\r\n";
        server.send(200, "text/html", sHtmlCode);
        delay(100);
        sHtmlCode = "";
        bExit = true;
      }
      if (server.hasArg("ws") && server.hasArg("wp")) {
        if (server.arg("ws").length() && server.arg("ws").length()) {
          Serial.println("ssid:" + server.arg("ws") + " pass:" + server.arg("wp"));
          bAddWifiMulti( server.arg("ws"), server.arg("wp"));
        } else Serial.println("Empty values");
      }
      if (server.hasArg("FlashOTADefault") && (sWSPlatform != "")) {
        Serial.println("FlashOTADefault");
        sHtmlCode = "<!DOCTYPE html><html>";
        sHtmlCode += "<head><title></title>";
        sHtmlCode += "  <meta content=\"\\&quot;width=device-width,\" initial-scale=\"1\\&quot;\" name=\"\\&quot;viewport\\&quot;\" />";
        sHtmlCode += "  <link href=\"\\&quot;data:,\\&quot;\" rel=\"\\&quot;icon\\&quot;\" /></head><body>";
        sHtmlCode += "<h1 style=\"text-align: center;\"><span style=\"font-family:tahoma,geneva,sans-serif;\">Flashing OTA Default</span></h1>";
        sHtmlCode += "<p style=\"text-align: center;\"><span style=\"font-family:tahoma,geneva,sans-serif;\"> Will restart in two minutes...<br>";
        sHtmlCode += "</body></html>\r\n";
        server.send(200, "text/html", sHtmlCode);
        String sOTAName = "WD_default_" + sWSPlatform + ".bin";
        Serial.println("FlashOTADefault " + sOTAName);
        delay(100);
        execOTA(sOTAName);
        delay(100);
        sHtmlCode = "";
        bExit = true;
      }
    }
    wifi_config_t conf;
    esp_wifi_get_config(WIFI_IF_STA, &conf);
    sHtmlCode = "<!DOCTYPE html><html>";
    sHtmlCode += "<head><title></title>";
    sHtmlCode += "  <meta content=\"\\&quot;width=device-width,\" initial-scale=\"1\\&quot;\" name=\"\\&quot;viewport\\&quot;\" />";
    sHtmlCode += "  <link href=\"\\&quot;data:,\\&quot;\" rel=\"\\&quot;icon\\&quot;\" /></head><body>";
    sHtmlCode += "<h1 style=\"text-align: center;\"><span style=\"font-family:tahoma,geneva,sans-serif;\">ESP32 Web Server</span></h1>";
    sHtmlCode += "<p style=\"text-align: center;\"><span style=\"font-family:tahoma,geneva,sans-serif;\"> [" + (String)(millis() / 1000) + " secs on platform " + sWSPlatform + " ]<br>";
    sHtmlCode += "<form method=GET><hr /><p style=\"text-align: center;\"><span style=\"font-family:tahoma,geneva,sans-serif;\">Please input changes and  <input name=\"ButtonReboot\" type=\"submit\" value=\"REBOOT\" />&nbsp;</p></form>";
    sHtmlCode += "<hr />";
    sHtmlCode += "<form method=GET>";
    sHtmlCode += "<p style=\"text-align: center;\"><span style=\"font-family:tahoma,geneva,sans-serif;\">WIFI SSID [" + (String)(reinterpret_cast<const char*>(conf.sta.ssid)) + "]: <input type=text name=ws><br>";
    sHtmlCode += "<p style=\"text-align: center;\"><span style=\"font-family:tahoma,geneva,sans-serif;\">WIFI PASS (" + (String)(strlen(reinterpret_cast<const char*>(conf.sta.password))) + " chars long)  : <input type=text name=wp><br>";
    sHtmlCode += "<span style=\"font-family:tahoma,geneva,sans-serif;\">&nbsp;<input type=\"submit\" />&nbsp;</span></p></form>";
    sHtmlCode += "<hr />";
    sHtmlCode += "<form action=\"edit\"><p style=\"text-align: center;\"><span style=\"font-family:tahoma,geneva,sans-serif;\"><input type=\"submit\" value=\"Edit files\" /></form>";
    sHtmlCode += "<hr />";
    sHtmlCode += "<form method=GET><p style=\"text-align: center;\"><span style=\"font-family:tahoma,geneva,sans-serif;\"><input name=\"FlashOTADefault\" type=\"submit\" value=\"FLASH_OTA_DEFAULT\" />&nbsp;</p></form>";
    sHtmlCode += "</body></html>\r\n";
    server.send(200, "text/html", sHtmlCode);
    sHtmlCode = String();
  });

  server.begin();
  Serial.println("HTTP server started");
  do {
    server.handleClient();
  }  while (!bExit);
}
