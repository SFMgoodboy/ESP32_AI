#ifndef Web_Scr_h
#define Web_Scr_h

// 与AP模式和Web服务器有关的库
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Preferences.h>
#include <Arduino.h>
#include "1.8TFT.h"


#define wifi_led 14

// AP模式的SSID和密码
extern const char *ap_ssid;
extern const char *ap_password;
// Web服务器和Preferences对象
extern AsyncWebServer server;
extern Preferences preferences;

extern  bool ledstatus,WIFI_cont_sign;
extern  int WIFI_sign,WIFI_con_time,WIFI_tinme,wifi_num,wifi_choose_num;

void openWeb();
void handleRoot(AsyncWebServerRequest *request);
void handleWifiManagement(AsyncWebServerRequest *request);
void handleSave(AsyncWebServerRequest *request);
void handleDelete(AsyncWebServerRequest *request);
void handleList(AsyncWebServerRequest *request);

int wifiConnect();

#endif