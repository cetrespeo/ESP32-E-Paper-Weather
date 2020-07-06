#include <Arduino.h>
#include <WString.h>
#include <WiFiClientSecure.h>
#include <esp_system.h>
#include "FS.h"
#include "SPIFFS.h"
#include <Update.h>
#include <HTTPClient.h>

#define VTGMEASSURES 24


int hour(time_t t);
int minute(time_t t);
int second(time_t t);
int day(time_t t);
int weekday(time_t t);
int month(time_t t);
int year(time_t t);
int iSecFrom000(time_t t);

String float2string(float n, int ndec);
String int2str2dig(int i);
float fLinearfit(int32_t* iVtgVal, int32_t* tVtgTime, int n, int iMaxHours);
int iGetMeanAnalogValue(int iPin, int iArraySize, int iDelay);
int iGetMeanAnalogValueFast(int iPin, int iArrSize);

String sInt32TimetoStr(int32_t tTime);
String sGetDateTimeStr(time_t t);
time_t tGetLocalTime();
int iWeekdayToday();
String sTimeLocal(time_t local);
String sDateLocal( String sLang, time_t local);
String sWeekDayNames(String sLang, int iDay);
String sDateWeekDayName( String sLang, time_t local);
String sDateMonthDay(time_t local);
String sDateMonthName( String sLang, time_t local);
String getMacAddress();
uint64_t getUUID();
uint32_t getUUID32();
uint32_t getUUID24();
uint16_t getUUID16();
String uintToStr( const uint64_t num);
String sUtf8ascii(String s);
unsigned int hexToDec(String hexString);

void listSPIFFSDir(const char * dirname, uint8_t levels);
String readSPIFFSFile(const char * path);
bool writeSPIFFSFile(const char * path, const char * message);
bool deleteSPIFFSFile(const char * path);
int sizeFSFile(const char * path);

bool execOTA(String sOtaBinName);
bool DowloadFromAWSToSpiffs(String sBinFileName,String sFileName);
