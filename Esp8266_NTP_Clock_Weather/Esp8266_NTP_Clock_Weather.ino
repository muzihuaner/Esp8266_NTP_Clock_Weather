/**The MIT License (MIT)

Copyright (c) 2018 by Daniel Eichhorn - ThingPulse

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

See more at https://thingpulse.com
*/

//Monsteryuan forked from Daniel Eichhorn/ThingPulse ESP8266 Weather Station

#include <DS18B20.h>
#include <Arduino.h>

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
// time
#include <time.h>                       // time() ctime()
#include <sys/time.h>                   // struct timeval
#include <coredecls.h>                  // settimeofday_cb()
#include <cstring>


#include "SH1106Wire.h"//or #include "SSD1306Wire.h"
#include "OLEDDisplayUi.h"
#include "Wire.h"
#include "HeFeng.h"

#include "WeatherStationFonts.h"
#include "WeatherStationImages.h"


/***************************
 * Begin Settings
 **************************/

DS18B20 ds(D7);

#define TZ              +8       // (utc+) TZ in hours
#define DST_MN          0      // use 60mn for summer time in some countries

/***************************
 * WiFi 配置
 * 优先使用默认配置，若为空则尝试已保存的 WiFi，最后进入 AP 配网模式
 * 建议将密码放在 AP 配网页面中输入，避免硬编码在源码中
 **************************/
// 默认 WiFi（留空则跳过，直接尝试已保存的 WiFi）
#define DEFAULT_WIFI_SSID     "WIFI名称"
#define DEFAULT_WIFI_PASSWORD "WIFI密码"

// 和风天气 API 配置

const char* HEFENG_KEY="你的key";
const char* HEFENG_LOCATION="你的地区id"; //例如: "101020100"为上海

// 更新间隔
const int UPDATE_INTERVAL_SECS = 20 * 60;       // 天气更新间隔：20 分钟
const int UPDATE_CURR_INTERVAL_SECS = 10;        // DS18B20 温度更新间隔：10 秒
const int WIFI_CHECK_INTERVAL_SECS = 5 * 60;     // WiFi 连接检查间隔：5 分钟
const int WIFI_RECONNECT_TIMEOUT_MS = 10000;     // WiFi 重连超时：10 秒

// Display Settings
const int I2C_DISPLAY_ADDRESS = 0x3c;
#if defined(ESP8266)
const int SDA_PIN = D2;
const int SCL_PIN = D5;  // 修正：原 SDC_PIN 命名错误
#endif

const char* const WDAY_NAMES[] PROGMEM = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
const char* const MONTH_NAMES[] PROGMEM = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

/***************************
 * End Settings
 **************************/

// Initialize the oled display for address 0x3c
SH1106Wire     display(I2C_DISPLAY_ADDRESS, SDA_PIN, SCL_PIN);   // or SSD1306Wire  display(I2C_DISPLAY_ADDRESS, SDA_PIN, SCL_PIN);
OLEDDisplayUi   ui( &display );

HeFengCurrentData currentWeather;
HeFengForeData foreWeather[3];
HeFeng HeFengClient;

#define TZ_MN           ((TZ)*60)
#define TZ_SEC          ((TZ)*3600)
#define DST_SEC         ((DST_MN)*60)

time_t now;

// flag changed in the ticker function every 10 minutes
bool readyForWeatherUpdate = false;

String lastUpdate = "--";

long timeSinceLastWUpdate = 0;
long timeSinceLastCurrUpdate = 0;
long timeSinceLastWifiCheck = 0;

String currTemp = "-1.0";

//declaring prototypes
void drawProgress(OLEDDisplay *display, int percentage, const char* label);
void updateData(OLEDDisplay *display);
void drawDateTime(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);
void drawCurrentWeather(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);
void drawForecast(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);
void drawForecastDetails(OLEDDisplay *display, int x, int y, int dayIndex);
void drawHeaderOverlay(OLEDDisplay *display, OLEDDisplayUiState* state);
void setReadyForWeatherUpdate();


// Add frames
// this array keeps function pointers to all frames
// frames are the single views that slide from right to left
FrameCallback frames[] = { drawDateTime, drawCurrentWeather, drawForecast };
int numberOfFrames = 3;

OverlayCallback overlays[] = { drawHeaderOverlay };
int numberOfOverlays = 1;



bool autoConfig()
{
  WiFi.mode(WIFI_STA);
  
  bool connected = false;
  
  if (strlen(DEFAULT_WIFI_SSID) > 0) {
    Serial.println("Trying default WiFi configuration...");
    WiFi.begin(DEFAULT_WIFI_SSID, DEFAULT_WIFI_PASSWORD);
    for (int i = 0; i < 20; i++)
    {
      if (WiFi.status() == WL_CONNECTED)
      {
        Serial.println("Default WiFi Connected");
        Serial.printf("SSID:%s\r\n", WiFi.SSID().c_str());
        WiFi.printDiag(Serial);
        connected = true;
        break;
      }
      delay(500);
      Serial.print(".");
      display.clear();
      display.drawString(64, 10, "Connecting to default WiFi");
      display.drawXbm(46, 30, 8, 8, i % 3 == 0 ? activeSymbole : inactiveSymbole);
      display.drawXbm(60, 30, 8, 8, i % 3 == 1 ? activeSymbole : inactiveSymbole);
      display.drawXbm(74, 30, 8, 8, i % 3 == 2 ? activeSymbole : inactiveSymbole);
      display.display();
    }
    if (connected) {
      return true;
    }
  }
  
  Serial.println("Trying saved WiFi configuration...");
  WiFi.begin();
  for (int i = 0; i < 20; i++)
  {
    if (WiFi.status() == WL_CONNECTED)
    {
      Serial.println("Saved WiFi Connected");
      Serial.printf("SSID:%s\r\n", WiFi.SSID().c_str());
      WiFi.printDiag(Serial);
      return true;
    }
    else
    {
        delay(500);
        Serial.print(".");
        display.clear();
        display.drawString(64, 10, "Connecting to WiFi");
        display.drawXbm(46, 30, 8, 8, i % 3 == 0 ? activeSymbole : inactiveSymbole);
        display.drawXbm(60, 30, 8, 8, i % 3 == 1 ? activeSymbole : inactiveSymbole);
        display.drawXbm(74, 30, 8, 8, i % 3 == 2 ? activeSymbole : inactiveSymbole);
        display.display(); 
    }
  }
  Serial.println("AutoConfig Failed!" );
  return false;
}

ESP8266WebServer server(80);

// 使用 F() 宏将 HTML 字符串存储在 Flash 中，减少 RAM 占用
// ESP8266 RAM 仅约 80KB，大字符串应尽量放在 Flash
const char HTML_TITLE[] PROGMEM = "<!DOCTYPE html><html><head><meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\"><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\"><meta http-equiv=\"X-UA-Compatible\" content=\"ie=edge\"><title>ESP8266网页配网</title>";
const char HTML_SCRIPT_ONE[] PROGMEM = "<script type=\"text/javascript\">function wifi(){var ssid = s.value;var password = p.value;var xmlhttp=new XMLHttpRequest();xmlhttp.open(\"GET\",\"/HandleWifi?ssid=\"+ssid+\"&password=\"+password,true);xmlhttp.send();xmlhttp.onload = function(e){alert(this.responseText);}}</script>";
const char HTML_SCRIPT_TWO[] PROGMEM = "<script>function c(l){document.getElementById('s').value=l.innerText||l.textContent;document.getElementById('p').focus();}</script>";
const char HTML_HEAD_BODY_BEGIN[] PROGMEM = "</head><body>请输入wifi信息进行配网:";
const char HTML_FORM_ONE[] PROGMEM = "<form>WiFi名称：<input id='s' name='s' type=\"text\" placeholder=\"请输入您WiFi的名称\"><br>WiFi密码：<input id='p' name='p' type=\"text\" placeholder=\"请输入您WiFi的密码\"><br><input type=\"button\" value=\"扫描\" onclick=\"window.location.href = '/HandleScanWifi'\"><input type=\"button\" value=\"连接\" onclick=\"wifi()\"></form>";
const char HTML_BODY_HTML_END[] PROGMEM = "</body></html>";

void handleRoot() {
    Serial.println("root page");
    String str;
    str.reserve(1024); // 预分配内存，减少多次拼接导致的碎片化
    str += FPSTR(HTML_TITLE);
    str += FPSTR(HTML_SCRIPT_ONE);
    str += FPSTR(HTML_SCRIPT_TWO);
    str += FPSTR(HTML_HEAD_BODY_BEGIN);
    str += FPSTR(HTML_FORM_ONE);
    str += FPSTR(HTML_BODY_HTML_END);
    server.send(200, "text/html", str);
}

void HandleScanWifi() {
    Serial.println("scan start");

    // WiFi.scanNetworks will return the number of networks found
    int n = WiFi.scanNetworks();
    Serial.println("scan done");
    
    String scanstr;
    scanstr.reserve(2048); // 预分配内存
    scanstr += FPSTR(HTML_TITLE);
    scanstr += FPSTR(HTML_SCRIPT_ONE);
    scanstr += FPSTR(HTML_SCRIPT_TWO);
    scanstr += FPSTR(HTML_HEAD_BODY_BEGIN);
    scanstr += FPSTR(HTML_FORM_ONE);
    
    if (n == 0) {
        Serial.println("no networks found");
        scanstr += "NO WIFI !!!";
    }
    else {
        Serial.print(n);
        Serial.println(" networks found");
        scanstr += "<table><head><tr><th>序号</th><th>名称</th><th>强度</th></tr></head><body>";
        for (int i = 0; i < n; ++i) {
            // Print SSID and RSSI for each network found
            Serial.print(i + 1);
            Serial.print(": ");
            Serial.print(WiFi.SSID(i));
            Serial.print(" (");
            Serial.print(WiFi.RSSI(i));
            Serial.print(")");
            Serial.println((WiFi.encryptionType(i) == ENC_TYPE_NONE) ? " " : "*");
            delay(10);
            scanstr += "<tr><td align=\"center\">";
            scanstr += String(i+1);
            scanstr += "</td><td align=\"center\"><a href='#p' onclick='c(this)'>";
            scanstr += WiFi.SSID(i);
            scanstr += "</a></td><td align=\"center\">";
            scanstr += String(WiFi.RSSI(i));
            scanstr += "</td></tr>";
        }
        scanstr += "</body></table>";
    }
    Serial.println("");

    scanstr += FPSTR(HTML_BODY_HTML_END);
    server.send(200, "text/html", scanstr);
}

void HandleWifi()
{
    String wifis = server.arg("ssid"); //从JavaScript发送的数据中找ssid的值
    String wifip = server.arg("password"); //从JavaScript发送的数据中找password的值
    Serial.println("received:"+wifis);
    server.send(200, "text/html", "连接中..");
    WiFi.begin(wifis, wifip);
}

void handleNotFound() { 
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}

void htmlConfig()
{
    WiFi.mode(WIFI_AP_STA);//设置模式为AP+STA
    WiFi.softAP("wifi_clock");

    IPAddress myIP = WiFi.softAPIP();
  
    if (MDNS.begin("clock")) {
      Serial.println("MDNS responder started");
    }
    
    server.on("/", handleRoot);
    server.on("/HandleWifi", HTTP_GET, HandleWifi);
    server.on("/HandleScanWifi", HandleScanWifi);
    server.onNotFound(handleNotFound);//请求失败回调函数
    MDNS.addService("http", "tcp", 80);
    server.begin();//开启服务器
    Serial.println("HTTP server started");
    int counter = 0;
    while(1)
    {
        server.handleClient();
        MDNS.update();  
        delay(500);
        display.clear();
        display.drawString(64, 5, "WIFI AP:wifi_clock");
        display.drawString(64, 20, "192.168.4.1");
        display.drawString(64, 35, "waiting for config wifi.");
        display.drawXbm(46, 50, 8, 8, counter % 3 == 0 ? activeSymbole : inactiveSymbole);
        display.drawXbm(60, 50, 8, 8, counter % 3 == 1 ? activeSymbole : inactiveSymbole);
        display.drawXbm(74, 50, 8, 8, counter % 3 == 2 ? activeSymbole : inactiveSymbole);
        display.display();  
        counter++;
        if (WiFi.status() == WL_CONNECTED)
        {
            Serial.println("HtmlConfig Success");
            Serial.printf("SSID:%s\r\n", WiFi.SSID().c_str());
            Serial.println("HTML连接成功");
            break;
        }
    }
    server.close();  
    WiFi.mode(WIFI_STA);
}

// WiFi 连接检查与自动重连
void checkAndReconnectWifi() {
  if (WiFi.status() == WL_CONNECTED) {
    return; // 已连接，无需处理
  }
  
  Serial.println("WiFi disconnected, attempting reconnection...");
  display.clear();
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.drawString(64, 20, "WiFi disconnected");
  display.drawString(64, 32, "Reconnecting...");
  display.display();
  
  WiFi.reconnect();
  
  unsigned long startAttemptTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < WIFI_RECONNECT_TIMEOUT_MS) {
    delay(500);
    Serial.print(".");
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi reconnected!");
    Serial.printf("SSID:%s, RSSI:%d\n", WiFi.SSID().c_str(), WiFi.RSSI());
  } else {
    Serial.println("\nWiFi reconnection failed. Will retry later.");
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println();

  // initialize display
  display.init();
  display.clear();
  display.display();

  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setContrast(255);

  bool wifiConfig = autoConfig();
  if(wifiConfig == false){
      htmlConfig();//HTML配网
  }

  ui.setTargetFPS(30);

  ui.setActiveSymbol(activeSymbole);
  ui.setInactiveSymbol(inactiveSymbole);

  // You can change this to
  // TOP, LEFT, BOTTOM, RIGHT
  ui.setIndicatorPosition(BOTTOM);

  // Defines where the first frame is located in the bar.
  ui.setIndicatorDirection(LEFT_RIGHT);

  // You can change the transition that is used
  // SLIDE_LEFT, SLIDE_RIGHT, SLIDE_TOP, SLIDE_DOWN
  ui.setFrameAnimation(SLIDE_LEFT);

  ui.setFrames(frames, numberOfFrames);
  ui.setTimePerFrame(7500);
  ui.setOverlays(overlays, numberOfOverlays);

  // Inital UI takes care of initalising the display too.
  ui.init();

  Serial.println("");
  configTime(TZ_SEC, DST_SEC, "ntp.ntsc.ac.cn", "ntp.aliyun.com", "ntp.tuna.tsinghua.edu.cn");
  updateData(&display);
}

void loop() {

  // 定期检查 WiFi 连接状态并自动重连
  if (millis() - timeSinceLastWifiCheck > (1000L * WIFI_CHECK_INTERVAL_SECS)) {
    checkAndReconnectWifi();
    timeSinceLastWifiCheck = millis();
  }

  if (millis() - timeSinceLastWUpdate > (1000L * UPDATE_INTERVAL_SECS)) {
    // 仅在 WiFi 已连接时才尝试更新天气
    if (WiFi.status() == WL_CONNECTED) {
      setReadyForWeatherUpdate();
    }
    timeSinceLastWUpdate = millis();
  }
  
  if (millis() - timeSinceLastCurrUpdate > (1000L * UPDATE_CURR_INTERVAL_SECS)) {
    if( ui.getUiState()->frameState == FIXED)
    {
      currTemp = String(ds.getTempC(), 1);
      timeSinceLastCurrUpdate = millis();
    }
  }

  if (readyForWeatherUpdate && ui.getUiState()->frameState == FIXED) {
    updateData(&display);
  }

  int remainingTimeBudget = ui.update();

  if (remainingTimeBudget > 0) {
    // You can do some work here
    // Don't do stuff if you are below your
    // time budget.
    delay(remainingTimeBudget);
  }
}

void drawProgress(OLEDDisplay *display, int percentage, const char* label) {
  display->clear();
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_10);
  display->drawString(64, 10, label);
  display->drawProgressBar(2, 28, 124, 10, percentage);
  display->display();
}


void updateData(OLEDDisplay *display) {
  drawProgress(display, 30, "Updating weather...");

  for(int i = 0; i < 5; i++){
    HeFengClient.doUpdateCurr(&currentWeather, HEFENG_KEY, HEFENG_LOCATION);
    if(currentWeather.cond_txt != "no network"){
      break;
    }
    Serial.printf("Weather update attempt %d failed, retrying...\n", i + 1);
    delay(1000);
  }
  
  drawProgress(display, 50, "Updating forecasts...");
  
  for(int i = 0; i < 5; i++){
    HeFengClient.doUpdateFore(foreWeather, HEFENG_KEY, HEFENG_LOCATION);
    if(foreWeather[0].datestr != "N/A"){
      break;
    }
    Serial.printf("Forecast update attempt %d failed, retrying...\n", i + 1);
    delay(1000);
  }
  
  readyForWeatherUpdate = false;
  drawProgress(display, 100, "Done...");
  delay(1000);
}



void drawDateTime(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  now = time(nullptr);
  struct tm* timeInfo;
  timeInfo = localtime(&now);
  char buff[24];

  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_16);

  // 使用 PROGMEM 指针 + strncpy 替代 String 数组访问
  char wday[4];
  strncpy_P(wday, WDAY_NAMES[timeInfo->tm_wday], sizeof(wday));
 
  sprintf_P(buff, PSTR("%04d-%02d-%02d, %s"), timeInfo->tm_year + 1900, timeInfo->tm_mon+1, timeInfo->tm_mday, wday);
  display->drawString(64 + x, 5 + y, String(buff));
  display->setFont(ArialMT_Plain_24);

  sprintf_P(buff, PSTR("%02d:%02d:%02d"), timeInfo->tm_hour, timeInfo->tm_min, timeInfo->tm_sec);
  display->drawString(64 + x, 22 + y, String(buff));
  display->setTextAlignment(TEXT_ALIGN_LEFT);
}

void drawCurrentWeather(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  display->setFont(ArialMT_Plain_10);
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  
  // 使用 char[] 缓冲区替代 String 拼接，减少堆碎片化
  char infoBuf[64];
  snprintf(infoBuf, sizeof(infoBuf), "%s | Wind: %s", currentWeather.cond_txt.c_str(), currentWeather.wind_sc.c_str());
  display->drawString(64 + x, 38 + y, infoBuf);

  display->setFont(ArialMT_Plain_24);
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  snprintf(infoBuf, sizeof(infoBuf), "%s°C", currentWeather.tmp.c_str());
  display->drawString(60 + x, 3 + y, infoBuf);
  
  display->setFont(ArialMT_Plain_10);
  snprintf(infoBuf, sizeof(infoBuf), "%s°C | %s%%", currentWeather.fl.c_str(), currentWeather.hum.c_str());
  display->drawString(70 + x, 26 + y, infoBuf);
  
  display->setFont(Meteocons_Plain_36);
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->drawString(32 + x, 0 + y, currentWeather.iconMeteoCon);
}


void drawForecast(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  drawForecastDetails(display, x, y, 0);
  drawForecastDetails(display, x + 44, y, 1);
  drawForecastDetails(display, x + 88, y, 2);
}

void drawForecastDetails(OLEDDisplay *display, int x, int y, int dayIndex) {

  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_10);
  display->drawString(x + 20, y, foreWeather[dayIndex].datestr);
  display->setFont(Meteocons_Plain_21);
  display->drawString(x + 20, y + 12, foreWeather[dayIndex].iconMeteoCon);

  // 使用 snprintf 替代 String 拼接
  char tempBuf[32];
  snprintf(tempBuf, sizeof(tempBuf), "%s | %s", foreWeather[dayIndex].tmp_min.c_str(), foreWeather[dayIndex].tmp_max.c_str());
  display->setFont(ArialMT_Plain_10);
  display->drawString(x + 20, y + 34, tempBuf);
  display->setTextAlignment(TEXT_ALIGN_LEFT);
}

void drawHeaderOverlay(OLEDDisplay *display, OLEDDisplayUiState* state) {
  now = time(nullptr);
  struct tm* timeInfo;
  timeInfo = localtime(&now);
  char buff[14];
  sprintf_P(buff, PSTR("%02d:%02d"), timeInfo->tm_hour, timeInfo->tm_min);

  display->setColor(WHITE);
  display->setFont(ArialMT_Plain_10);
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->drawString(6, 54, String(buff));
  display->setTextAlignment(TEXT_ALIGN_RIGHT);
  
  // 使用 snprintf 替代 String 拼接
  char tempBuf[16];
  snprintf(tempBuf, sizeof(tempBuf), "%s°C", currTemp.c_str());
  display->drawString(128, 54, tempBuf);
  display->drawHorizontalLine(0, 52, 128);
}

void setReadyForWeatherUpdate() {
  Serial.println("Setting readyForUpdate to true");
  readyForWeatherUpdate = true;
}
