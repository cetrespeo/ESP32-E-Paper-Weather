#include "ESP32_Disp_Aux.h"
String aWifiSSIDs[10];
String aWifiPSWDs[10];
//////////////////////////////////////////////////////////////////////////////
String sWeekDayNames(String sLang, int iDay) {
  if (sLang == (String)("es")) {
    const char* WeekDayNames[7] = {"Dom", "Lun", "Mar", "Mie", "Jue", "Vie", "Sab"};
    return WeekDayNames[iDay];
  }
  if (sLang == (String)("fr")) {
    const char* WeekDayNames[7] = {"Dim", "Lun", "Mar", "Mer", "Jeu", "Ven", "Sam"};
    return WeekDayNames[iDay];
  }
  if (sLang == (String)("de")) {
    const char* WeekDayNames[7] = {"Son", "Mon", "Die", "Mit", "Don", "Fre", "Sam"};
    return WeekDayNames[iDay];
  }
  if (sLang == (String)("cz")) {
    const char* WeekDayNames[7] = {"Ne", "Po", "Út", "St", "Čt", "Pá", "So"} ;
    return WeekDayNames[iDay];
  }
  const char* WeekDayNames[7] = {"Sun", "Mon", "Tue", "Wed", "Thr", "Fri", "Sat"};
  return WeekDayNames[iDay];
}
//////////////////////////////////////////////////////////////////////////////
String sDateMonthName( String sLang, time_t local) {
  if (!local) local = time(nullptr);
  if (sLang == (String)("es")) {
    const char* MonthNames[12] = {"Ene", "Feb", "Mar", "Abr", "May", "Jun", "Jul", "Ago", "Sep", "Oct", "Nov", "Dic"};
    return (MonthNames[month(local) - 1]);
  }
  if (sLang == (String)("fr")) {
    const char* MonthNames[12] = {"Jan", "Fev", "Mar", "Avr", "Mai", "Jun", "Jul", "Aou", "Sep", "Oct", "Nov", "Dec"};
    return (MonthNames[month(local) - 1]);
  }
  if (sLang == (String)("de")) {
    const char* MonthNames[12] = {"Jan", "Feb", "Mar", "Apr", "Mai", "Jun", "Jul", "Aou", "Sep", "Okt", "Nov", "Dez"};
    return (MonthNames[month(local) - 1]);
  }
  if (sLang == (String)("cz")) {
    const char* MonthNames[12] =    {"Led", "Úno", "Bře", "Dub", "Kvě", "Čer", "Čec", "Srp", "Zář", "Říj", "Lis", "Pro"};
    return (MonthNames[month(local) - 1]);
  }
  const char* MonthNames[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
  return (MonthNames[month(local) - 1]);
}
//////////////////////////////////////////////////////////////////////////////
String sDateWeekDayName(String sLang, time_t local) {
  if (!local) local = time(nullptr);
  return (sWeekDayNames(sLang, weekday(local) - 1));
}
//////////////////////////////////////////////////////////////////////////////
String sDateMonthDay(time_t local) {
  if (!local) local = time(nullptr);
  return (int2str2dig(day(local)));
}

int hour(time_t t) { // the minute for the given time
  struct tm * timeinfo;
  timeinfo = localtime(&t);
  return timeinfo->tm_hour;
}
int minute(time_t t) { // the minute for the given time
  struct tm * timeinfo;
  timeinfo = localtime(&t);
  return timeinfo->tm_min;
}

int second(time_t t) {  // the second for the given time
  struct tm * timeinfo;
  timeinfo = localtime(&t);
  return timeinfo->tm_sec;
}

int iSecFrom000(time_t t) {  // number of seconds from 00:00
  struct tm * timeinfo;
  timeinfo = localtime(&t);
  return (timeinfo->tm_hour * 3600 + timeinfo->tm_min * 60 + timeinfo->tm_sec);
}

int iMinFrom000(time_t t) {  // number of minutes from 00:00
  struct tm * timeinfo;
  timeinfo = localtime(&t);
  return (timeinfo->tm_hour * 60 + timeinfo->tm_min );
}

int day(time_t t) { // the day for the given time (0-6)
  struct tm * timeinfo;
  timeinfo = localtime(&t);
  return timeinfo->tm_mday;
}

int weekday(time_t t) {
  struct tm * timeinfo;
  timeinfo = localtime(&t);
  return (timeinfo->tm_wday + 1);
}

int month(time_t t) {  // the month for the given time
  struct tm * timeinfo;
  timeinfo = localtime(&t);
  return (timeinfo->tm_mon + 1);
}

int year(time_t t) { // the year for the given time
  struct tm * timeinfo;
  timeinfo = localtime(&t);
  if (timeinfo->tm_year < 2000)
    return (1900 + timeinfo->tm_year);
  else
    return (timeinfo->tm_year);
}

//////////////////////////////////////////////////////////////////////////////
String sInt32TimetoStr(int32_t tTime) {
  time_t tAux = tTime;
  return (String)(asctime(localtime(&tAux)));
}
//////////////////////////////////////////////////////////////////////////////
bool readSPIFFSBin(const char * path, uint8_t * buff, int len) {
  Serial.printf("{RB:'%s'", path);
  delay(50);
  File file = SPIFFS.open(path);
  delay(50);
  if (!file || file.isDirectory()) {
    Serial.println(" failed!}");
    return false;
  }
  if (file.size() < len) {
    Serial.printf("\n ERROR readSPIFFSBin file lenght! %d<>%d\n", file.size(), len);
    return false;
  }
  file.read(buff, len);
  Serial.printf(">%dB}", sizeof(buff));
  return true;
}
//////////////////////////////////////////////////////////////////////////////
bool writeSPIFFSBin(const char * path, uint8_t * buff, int len) {
  Serial.printf(" {WB'%s'", path);
  delay(50);
  File file = SPIFFS.open(path, FILE_WRITE);
  delay(50);
  if (!file) {
    Serial.println("- failed to open file for writing}");
    return false;
  }
  if (file.write(buff, len)) {
    Serial.printf(">%dB}", len);
    delay(50);
    return true;
  } else {
    Serial.println(" failed!}");
    delay(50);
    return false;
  }
}
//////////////////////////////////////////////////////////////////////////////
String readFSFile(fs::FS &fs, const char * path) {
  Serial.printf("{R'%s'", path);
  delay(50);
  File file = fs.open(path);
  delay(50);
  if (!file || file.isDirectory()) {
    Serial.println(" failed!}");
    return "";
  }
  String sAux = "";
  int iFileLength = file.size();
  while (file.available() && (sAux.length() < iFileLength)) {
    sAux = sAux + (char)(file.read());
  }
  //  Serial.printf("\n<%s>\n", sAux.c_str());
  Serial.printf("%dB}", sAux.length());
  return sAux;
}
//////////////////////////////////////////////////////////////////////////////
bool writeFSFile(fs::FS &fs, const char * path, const char * message) {
  Serial.printf(" {W'%s'", path);
  delay(50);
  File file = fs.open(path, FILE_WRITE);
  delay(50);
  if (!file) {
    Serial.println("- failed to open file for writing}");
    return false;
  }
  if (file.print(message)) {
    Serial.print(">" + (String)(strlen(message)) + "B}");
    delay(50);
    return true;
  } else {
    Serial.println(" failed!}");
    delay(50);
    return false;
  }
}
//////////////////////////////////////////////////////////////////////////////
int sizeFSFile(const char * path) {
  Serial.printf(" {S'%s'", path);
  delay(50);
  File file = SPIFFS.open(path);
  delay(50);
  if (!file) {
    Serial.println("- failed to size}");
    return false;
  }
  Serial.print(">" + (String)(file.size()) + "B}");
  delay(50);
  return file.size();
}
//////////////////////////////////////////////////////////////////////////////
bool  deleteFSFile(fs::FS &fs, const char * path) {
  delay(10);
  if (!fs.exists(path)) {
    return true;
  }
  Serial.printf(" {D'%s'", path);
  delay(10);
  if (fs.remove(path)) {
    Serial.println("}");
    delay(50);
    return true;
  } else {
    Serial.println("- failed}");
    delay(50);
    return false;
  }
}
//////////////////////////////////////////////////////////////////////////////
String listDir(fs::FS &fs, const char * dirname, uint8_t levels, bool bSerialPrint) {
  static int iFileNum=0;
  String sListRet = "";
  if (bSerialPrint) Serial.printf("Listing directory: %s\r\n", dirname);
  File root = fs.open(dirname);
  if (!root) {
    if (bSerialPrint) Serial.println("- failed to open directory");
    return "[FAILED]";
  }
  if (!root.isDirectory()) {
    if (bSerialPrint) Serial.println(" - not a directory");
    return "[NO DIR]";
  }

  File file = root.openNextFile();
  while (file) {
    if (file.isDirectory()) {
      if (bSerialPrint) Serial.print(" DIR: ");
      if (bSerialPrint) Serial.print(file.name());
      if (levels) {
        sListRet = sListRet + listDir(fs, file.name(), levels - 1, bSerialPrint);
      }
    } else {
      iFileNum++;
      if (iFileNum>5) {
        iFileNum=0;
        if (bSerialPrint) Serial.print("\n");
      }
      if (bSerialPrint) Serial.print(file.name());
      String sAux = file.name();
//      if ((bSerialPrint) && (sAux.length() < 8)) Serial.print("\t");
      if (bSerialPrint) Serial.print(":");
      if (bSerialPrint) Serial.print(file.size());
      if (bSerialPrint) Serial.print("B,");
    }
    sListRet = sListRet + "," + (String)(file.name()) + ":" + (String)(file.size()) + "B";
    file = root.openNextFile();
  }
  return sListRet;
}
//////////////////////////////////////////////////////////////////////////////
String listSPIFFSDir(const char * dirname, uint8_t levels, bool bSerialPrint) {
  return listDir(SPIFFS, dirname, levels, bSerialPrint);
}
//////////////////////////////////////////////////////////////////////////////
String readSPIFFSFile(const char * path) {
  return readFSFile(SPIFFS, path);
}
//////////////////////////////////////////////////////////////////////////////
bool writeSPIFFSFile(const char * path, const char * message) {
  return writeFSFile(SPIFFS, path, message);
}
//////////////////////////////////////////////////////////////////////////////
bool deleteSPIFFSFile(const char * path) {
  return deleteFSFile(SPIFFS, path);
}
//////////////////////////////////////////////////////////////////////////////
String uintToStr( const uint64_t num)
{
  char str[21];
  uint8_t i = 0;
  uint64_t n = num;
  do
    i++;
  while ( n /= 10 );
  str[i] = '\0';
  n = num;
  do
    str[--i] = ( n % 10 ) + '0';
  while ( n /= 10 );
  return (String)(str);
}
//////////////////////////////////////////////////////////////////////////////
String getMacAddress() {
  uint8_t baseMac[6];
  // Get MAC address for WiFi station
  esp_read_mac(baseMac, ESP_MAC_WIFI_STA);
  char baseMacChr[18] = {0};
  sprintf(baseMacChr, "%02X:%02X:%02X:%02X:%02X:%02X", baseMac[0], baseMac[1], baseMac[2], baseMac[3], baseMac[4], baseMac[5]);
  return String(baseMacChr);
}
//////////////////////////////////////////////////////////////////////////////
uint64_t getUUID() {
  uint8_t baseMac[6];
  esp_read_mac(baseMac, ESP_MAC_WIFI_STA);
  uint64_t uuid = baseMac[0] + (baseMac[1] << 8) + (baseMac[2] << 16) + (baseMac[3] << 24) + (baseMac[4] << 32) + (baseMac[5] << 38);
  return uuid;
}
//////////////////////////////////////////////////////////////////////////////
uint32_t getUUID32() {
  uint8_t baseMac[6];
  esp_read_mac(baseMac, ESP_MAC_WIFI_STA);
  uint32_t uuid = baseMac[0] + (baseMac[1] << 8) + (baseMac[2] << 16) + (baseMac[3] << 24);
  return uuid;
}
//////////////////////////////////////////////////////////////////////////////
uint32_t getUUID24() {
  uint8_t baseMac[6];
  esp_read_mac(baseMac, ESP_MAC_WIFI_STA);
  uint32_t uuid = baseMac[0] + (baseMac[1] << 8) + (baseMac[2] << 16) ;
  return uuid;
}
//////////////////////////////////////////////////////////////////////////////
uint16_t getUUID16() {
  uint8_t baseMac[6];
  esp_read_mac(baseMac, ESP_MAC_WIFI_STA);
  uint16_t uuid = baseMac[0] + (baseMac[1] << 8);
  return uuid;
}
//////////////////////////////////////////////////////////////////////////////
String sGetDateTimeStr(time_t t) {
  if (!t) return "";
  char buff[30];
  sprintf(buff, "%04d/%02d/%02d_%02d:%02d:%02d", year(t), month(t), day(t), hour(t), minute(t), second(t));
  return (String)(buff);
}
//////////////////////////////////////////////////////////////////////////////
time_t tGetLocalTime() {
  return (time(nullptr));
}
//////////////////////////////////////////////////////////////////////////////
int iWeekdayToday () {
  time_t local = time(nullptr) ;
  return (weekday(local) - 1);
}
//////////////////////////////////////////////////////////////////////////////
String sTimeLocal(time_t local, bool bZeros) {
  if (!local) local = time(nullptr);
  if (bZeros)  return (int2str2dig(hour(local)) + ":" + int2str2dig(minute(local)));
  else return ((String)(hour(local)) + ":" + int2str2dig(minute(local)));
}
//////////////////////////////////////////////////////////////////////////////
String sDateLocal( String sLang, time_t local) {
  if (!local) local = time(nullptr);
  String sAux = sDateWeekDayName(sLang, local);
  sAux += sDateMonthDay(local) + "/" + sDateMonthName(sLang, local);
  return (sAux);
}
//////////////////////////////////////////////////////////////////////////////
String float2string(float n, int ndec) {
  if (ndec == 0) {
    return (String)(int)(n + 0.5);
  }
  String r = "";
  int v = n;
  r += v;     // whole number part
  r += '.';   // decimal point
  int i;
  for (i = 0; i < ndec; i++) {
    // iterate through each decimal digit for 0..ndec
    n -= v;
    n *= 10;
    v = n;
    r += v;
  }
  return r;
}
//////////////////////////////////////////////////////////////////////////////
String int2str2dig(int i) {
  String sTmp = "";
  if (i < 10) sTmp = "0";
  sTmp += (String)i;
  return sTmp;
}
//////////////////////////////////////////////////////////////////////////////
#define LINEARFITFACTOR 604800 // need to test 604800 (1 week)
float fLinearfit(int32_t* iVtgVal, int32_t* tVtgTime, int n, int iMaxHours) {
  int i, iMin, iGoodValue = 0;
  float sum_x = 0, sum_y = 0, sum_x2 = 0, sum_y2 = 0, sum_xy = 0, m, b;
  uint32_t timeIni = 0;
  //  Serial.print("[LinearFit: Diffs");
  iMin = 0;
  if (iMaxHours > 0) {
    for (i = n - 2; i > 0; i--) {
      //    Serial.print("," + String(i) + "=" + String((tVtgTime[n - 1] - tVtgTime[i])));
      if ((tVtgTime[n - 1] - tVtgTime[i]) > (iMaxHours * 3600)) {
        break;
      }
    }
    iMin = i;
    if (iMaxHours > 24) iMin ++;
  } else iMin = 0;
  timeIni = tVtgTime[iMin];
  //  Serial.print("->min:" + String(iMin)+ "("+String((tVtgTime[n - 1] - tVtgTime[iMin])/3600)+"h)");
  for (i = iMin; i < n; i++)  {
    if (tVtgTime[i] > 0) {
      if (timeIni == 0) timeIni = tVtgTime[i] - 10;
      //     Serial.print("(" + (String)(tVtgTime[i] - timeIni) + "," + (String)(iVtgVal[i]) + ")");
      sum_x += (tVtgTime[i] - timeIni);
      sum_y += iVtgVal[i];
      sum_x2 += (tVtgTime[i] - timeIni) * (tVtgTime[i] - timeIni);
      sum_y2 += iVtgVal[i] * iVtgVal[i];
      sum_xy = sum_xy + (tVtgTime[i] - timeIni) * iVtgVal[i];
      iGoodValue++;
    } else {
      //      Serial.print("0");
    }
  }
  m = ((iGoodValue) * sum_xy - sum_x * sum_y) / ((iGoodValue) * sum_x2 - sum_x * sum_x) * LINEARFITFACTOR;
  b = (sum_x2 * sum_y - sum_x * sum_xy) / ((iGoodValue) * sum_x2 - sum_x * sum_x);
  //  Serial.println(" => m=" + (String)(int)(m) + " " + iGoodValue + " values]");
  if (m > LINEARFITFACTOR) m = LINEARFITFACTOR;
  if (m < -LINEARFITFACTOR) m = -LINEARFITFACTOR;
  return (float)m;
}
//////////////////////////////////////////////////////////////////////////////
int iGetMeanAnalogValueFast(int iPin, int iArrSize) {
  int i1, iMean = 0, iMillis;
  iMillis = millis();
  for (i1 = 0; i1 < iArrSize; i1++) {
    iMean = iMean + analogRead(iPin);
  }
  iMean = iMean / iArrSize;
  Serial.print(" [FST mVtg:" + String((int)(iMean)) + "@" + (String)(millis() - iMillis) + "ms]" );
  return iMean;
}
//////////////////////////////////////////////////////////////////////////////
int iGetMeanAnalogValue(int iPin, int iArrSize, int iDelay) {
  int i1, i2, i3, i4, iMean, iA, iV1[iArrSize], iV2[iArrSize], iV3[iArrSize], iV4[iArrSize], iMillis;
  iMillis = millis();
  for (i1 = 0; i1 < iArrSize; i1++) {
    for (i2 = 0; i2 < iArrSize; i2++) {
      for (i3 = 0; i3 < iArrSize; i3++) {
        for (i4 = 0; i4 < iArrSize; i4++) {
          delay(iDelay);
          iV4[i4] = analogRead(iPin);
        }
        iMean = 0;
        for (iA = 0; iA < iArrSize; iA++) iMean += iV4[iA];
        iV3[i3] = iMean / iArrSize;
      }
      iMean = 0;
      for (iA = 0; iA < iArrSize; iA++) iMean += iV3[iA];
      iV2[i2] = iMean / iArrSize;
    }
    iMean = 0;
    for (iA = 0; iA < iArrSize; iA++) iMean += iV2[iA];
    iV1[i1] = iMean / iArrSize;
  }
  iMean = 0;
  for (iA = 0; iA < iArrSize; iA++) iMean += iV1[iA];
  iMean = iMean / iArrSize;
  Serial.print(" [mVtg:" + String((int)(iMean)) + "@" + (String)(millis() - iMillis) + "ms]" );
  return iMean;
}
//////////////////////////////////////////////////////////////////////////////////////////////////
// Utility to extract header value from headers
String getHeaderValue(String header, String headerName) {
  return header.substring(strlen(headerName.c_str()));
}
//////////////////////////////////////////////////////////////////////////////////////////////////
// OTA Logic
bool execOTA(String sOtaBinName) {
  //AWS S3 OTA Variables, Update to yours
  const String sOtaHost = "s3.eu-west-3.amazonaws.com";
  const String sOtaBucket = "/otabin-weather/";
  const int iOtaPort = 443;
  int contentLength = 0;
  bool isValidContentType = false;
  bool bResult = false;
  WiFiClientSecure  SecureClient;
  Serial.println("Connecting to: " + String(sOtaHost));
  if (SecureClient.connect(sOtaHost.c_str(), iOtaPort)) {
    SecureClient.print(String("GET ") + sOtaBucket + sOtaBinName +  " HTTP/1.1\r\n" + "Host: " + sOtaHost + "\r\n" + "Cache-Control: no-cache\r\n" + "Connection: close\r\n\r\n");
    unsigned long timeout = millis();
    while (SecureClient.available() == 0) {
      if (millis() - timeout > 5000) {
        Serial.println("Client Timeout !");
        SecureClient.stop();
        return false;
      }
    }
    while (SecureClient.available()) {
      String line = SecureClient.readStringUntil('\n');
      line.trim();
      if (!line.length())         break;
      if (line.startsWith("HTTP/1.1")) {
        if (line.indexOf("200") < 0) {
          Serial.println("Got a non 200 status code from server. Exiting OTA Update.");
          break;
        }
      }
      if (line.startsWith("Content-Length: ")) {
        contentLength = atoi((getHeaderValue(line, "Content-Length: ")).c_str()); //atoi(line.substring(strlen("Content-Length: ")));
        Serial.println("Got " + String(contentLength) + " bytes from server");
      }
      if (line.startsWith("Content-Type: ")) {
        String contentType = getHeaderValue(line, "Content-Type: ");
        Serial.println("Got " + contentType + " payload.");
        if (contentType == "application/octet-stream") {
          isValidContentType = true;
        }
      }
    }
  } else     Serial.println("Connection to " + String(sOtaHost) + " failed. Please check your setup");
  Serial.println("contentLength : " + String(contentLength) + ", isValidContentType : " + String(isValidContentType));
  if (contentLength && isValidContentType) {
    bool canBegin = Update.begin(contentLength);
    if (canBegin) {
      Serial.println("Begin OTA. This may take 2 - 5 mins to complete. Things might be quite for a while.. Patience!");
      size_t written = Update.writeStream(SecureClient);
      if (written == contentLength) {
        Serial.println("Written : " + String(written) + " successfully");
        bResult = true;
      } else Serial.println("Written only : " + String(written) + "/" + String(contentLength) + ". Retry?" );
      if (Update.end()) {
        Serial.println("OTA done!");
        if (Update.isFinished()) {
          Serial.println("Update successfully completed. Rebooting.");
          return true;
        } else   Serial.println("Update not finished? Something went wrong!");
      } else         Serial.println("Error Occurred. Error #: " + String(Update.getError()));
    } else {
      Serial.println("Not enough space to begin OTA");
      SecureClient.flush();
    }
  } else {
    Serial.println("There was no content in the response");
    SecureClient.flush();
    return bResult;
  }
}
//////////////////////////////////////////////////////////////////////////////
bool DowloadFromAWSToSpiffs(String sBinFileName, String sFileName) {
  Serial.println("\nCopying from " + sBinFileName + " to " + sFileName);
  File f = SPIFFS.open(sFileName, FILE_WRITE);
  if (f) {
    const String sOtaHost = "s3.eu-west-3.amazonaws.com";
    const String sOtaBucket = "/otabin-weather/";
    const int iOtaPort = 443;
    int contentLength = 0;
    bool isValidContentType = false;
    bool bResult = false;
    WiFiClientSecure  AwsClient;
    Serial.println("Connecting to: " + String(sOtaHost));
    if (AwsClient.connect(sOtaHost.c_str(), iOtaPort)) {
      AwsClient.print(String("GET ") + sOtaBucket + sBinFileName +  " HTTP/1.1\r\n" + "Host: " + sOtaHost + "\r\n" + "Cache-Control: no-cache\r\n" + "Connection: close\r\n\r\n");
      unsigned long timeout = millis();
      while (AwsClient.available() == 0) {
        if (millis() - timeout > 5000) {
          Serial.println("Client Timeout !");
          AwsClient.stop();
          return false;
        }
      }
    }
    while (AwsClient.connected()) {
      String line = AwsClient.readStringUntil('\n');
      if (line == "\r") {
        Serial.println("headers received");
        break;
      }
    }
    int iLength = 0;
    while (AwsClient.connected()) {
      byte c = AwsClient.read();
      if (c != 255) {
        f.print(char(c));
        iLength++;
      }
    }
    f.close();
    Serial.println(String(iLength) + " bytes downloaded.");
    AwsClient.stop();
    if (!iLength) {
      deleteFSFile(SPIFFS, sFileName.c_str());
    }
  }
}
/*
static byte c1;  // Last character buffer
byte utf8ascii(byte ascii) {
  if ( ascii < 128 ) // Standard ASCII-set 0..0x7F handling
  { c1 = 0;
    return ( ascii );
  }
  // get previous input
  byte last = c1;   // get last char
  c1 = ascii;       // remember actual character
  switch (last)     // conversion depending on first UTF8-character
  { case 0xC2: return  (ascii);  break;
    case 0xC3: return  (ascii | 0xC0);  break;
    case 0x82: if (ascii == 0xAC) return (0x80);   // special case Euro-symbol
  }
  return  (0);                                     // otherwise: return zero, if character has to be ignored
}
////////////////////////////////////
String sUtf8ascii(String s)
{
  String r = "";
  char c;
  for (int i = 0; i < s.length(); i++)
  {
    c = utf8ascii(s.charAt(i));
    if (c != 0) r += c;
  }
  return r;
}*/
////////////////////////////////
unsigned int hexToDec(String hexString) {

  unsigned int decValue = 0;
  int nextInt;

  for (int i = 0; i < hexString.length(); i++) {

    nextInt = int(hexString.charAt(i));
    if (nextInt >= 48 && nextInt <= 57) nextInt = map(nextInt, 48, 57, 0, 9);
    if (nextInt >= 65 && nextInt <= 70) nextInt = map(nextInt, 65, 70, 10, 15);
    if (nextInt >= 97 && nextInt <= 102) nextInt = map(nextInt, 97, 102, 10, 15);
    nextInt = constrain(nextInt, 0, 15);

    decValue = (decValue * 16) + nextInt;
  }

  return decValue;
}////////////////////////////////
String listPartitions(bool bSerialPrint) {
  String sPartitionInfo = "kB";
  String sBootPart = "";
#ifdef _OTA_OPS_H
  const esp_partition_t *boot_partition = esp_ota_get_running_partition();
  sBootPart = boot_partition->label;
#endif

#ifdef __ESP_PARTITION_H__
  size_t ul;
  esp_partition_iterator_t _mypartiterator;
  const esp_partition_t *_mypart;
  ul = spi_flash_get_chip_size();
  if (bSerialPrint)  Serial.print("Flash chip size: ");
  if (bSerialPrint)  Serial.println(ul);
  if (bSerialPrint)  Serial.printf("Partiton table: (running on %s)\n", sBootPart.c_str());
  _mypartiterator = esp_partition_find(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, NULL);
  if (_mypartiterator)
  {
    do
    {
      _mypart = esp_partition_get(_mypartiterator);
      if (bSerialPrint)      printf("%x - %02x - %06x - %06x - %s - %i\r\n", _mypart->type, _mypart->subtype, _mypart->address, _mypart->size, _mypart->label, _mypart->encrypted);
      sPartitionInfo = sPartitionInfo + "," + (String)(_mypart->label);
      if (sBootPart == (String)(_mypart->label)) sPartitionInfo = sPartitionInfo + "*";
      sPartitionInfo = sPartitionInfo + ":" + (String)(_mypart->size / 1024) ;
    } while ((_mypartiterator = esp_partition_next(_mypartiterator)));
  }
  esp_partition_iterator_release(_mypartiterator);
  _mypartiterator = esp_partition_find(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, NULL);
  if (_mypartiterator)
  {
    do
    {
      _mypart = esp_partition_get(_mypartiterator);
      if (bSerialPrint)      printf("%x - %02x - %06x - %06x - %s - %i\r\n", _mypart->type, _mypart->subtype, _mypart->address, _mypart->size, _mypart->label, _mypart->encrypted);
      sPartitionInfo = sPartitionInfo + "," + (String)(_mypart->label);
      if (sBootPart == (String)(_mypart->label)) sPartitionInfo = sPartitionInfo + "*";
      sPartitionInfo = sPartitionInfo + ":" + (String)(_mypart->size / 1024) ;
    } while ((_mypartiterator = esp_partition_next(_mypartiterator)));
  }
  esp_partition_iterator_release(_mypartiterator);
  if (bSerialPrint)  Serial.println("-----------------------------------");
#endif
  return sPartitionInfo;
}//////////////////////////////////////////////////////////////////////////////

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
  String sAuxSSID, sAuxPSWD, sFSSsid, sFSPswd = "";
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
  int i = 0;
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
