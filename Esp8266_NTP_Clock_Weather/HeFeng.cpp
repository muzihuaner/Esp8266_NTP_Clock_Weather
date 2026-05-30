
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>  
#include <WiFiClientSecureBearSSL.h>  
#include "HeFeng.h"
#include "HttpsGetUtils.h"

HeFeng::HeFeng() {
}

// 天气代码到 Meteocon 图标的查找表
// 使用结构体数组替代长 if-else 链，更高效且易维护
struct WeatherIconMapping {
  const char* codes[6]; // 每个映射最多支持6个天气代码，末尾为空
  char icon;
};

static const WeatherIconMapping iconTable[] = {
  {{"100", "9006", ""},           'B'}, // 晴
  {{"999", ""},                   ')'}, // 未知
  {{"104", ""},                   'D'}, // 阴
  {{"500", ""},                   'E'}, // 霾
  {{"503", "504", "507", "508", ""}, 'F'}, // 浮尘/扬沙
  {{"499", "901", ""},            'G'}, // 雾
  {{"103", ""},                   'H'}, // 多云
  {{"502", "511", "512", "513", ""}, 'L'}, // 烟霾
  {{"501", "509", "510", "514", "515", ""}, 'M'}, // 雾霾
  {{"102", ""},                   'N'}, // 少云
  {{"213", ""},                   'O'}, // 沙尘暴
  {{"302", "303", ""},            'P'}, // 雷阵雨
  {{"305", "308", "309", "314", "399", ""}, 'Q'}, // 阵雨
  {{"306", "307", "310", "311", "312", "315"}, 'R'}, // 暴雨
  {{"316", "317", "318", ""},     'R'}, // 暴雨（续）
  {{"200", "201", "202", "203", "204", ""}, 'S'}, // 雷暴
  {{"205", "206", "207", "208", "209", ""}, 'S'}, // 雷暴（续）
  {{"210", "211", "212", ""},     'S'}, // 雷暴（续）
  {{"300", "301", ""},            'T'}, // 小雨
  {{"400", "408", ""},            'U'}, // 小雪
  {{"407", ""},                   'V'}, // 阵雪
  {{"401", "402", "403", ""},     'W'}, // 中雪/大雪
  {{"409", "410", ""},            'W'}, // 大雪（续）
  {{"304", "313", "404", "405", "406", ""}, 'X'}, // 冻雨/雨夹雪
  {{"101", ""},                   'Y'}, // 阴转多云
};

static const int ICON_TABLE_SIZE = sizeof(iconTable) / sizeof(iconTable[0]);

const char* HeFeng::getMeteoconIcon(const String& cond_code) {
  for (int t = 0; t < ICON_TABLE_SIZE; t++) {
    for (int c = 0; c < 6; c++) {
      if (iconTable[t].codes[c][0] == '\0') break; // 到达末尾
      if (cond_code == iconTable[t].codes[c]) {
        // 返回指向静态字符串的指针，无需额外分配
        static char iconStr[2] = {0, 0};
        iconStr[0] = iconTable[t].icon;
        return iconStr;
      }
    }
  }
  return ")"; // 默认未知图标
}

void HeFeng::doUpdateCurr(HeFengCurrentData *data, const String& key,
                          const String& location) {  // 获取天气

  std::unique_ptr<BearSSL::WiFiClientSecure> client(
      new BearSSL::WiFiClientSecure);
  client->setInsecure();

  String url = "https://devapi.qweather.com/v7/weather/now?lang=zh&location=" +
               location + "&key=" + key;
  Serial.printf("[HTTPS] doUpdateCurr begin... url:%s\n", url.c_str());

  uint8_t *outbuf = NULL;
  size_t len = 0;
  bool result = HttpsGetUtils::getString(url.c_str(), outbuf, len);
  Serial.printf("result=%d, len=%d\n", result, len);

  if (outbuf && len) {
    Serial.println("parse json");
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, (char *)outbuf, len);
    
    // 解析完成后立即释放 outbuf，防止内存泄漏
    free(outbuf);
    outbuf = NULL;

    if (error) {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.f_str());
      return;
    }
    
    const char* httpCode = doc["code"];
    Serial.println("doUpdateCurr httpCode:");
    Serial.println(httpCode);

    if (strcmp(httpCode, "200") == 0) {
      data->tmp = doc["now"]["temp"].as<String>();
      data->fl = doc["now"]["feelsLike"].as<String>();
      data->hum = doc["now"]["humidity"].as<String>();
      data->wind_sc = doc["now"]["windScale"].as<String>();
      String cond_code = doc["now"]["icon"].as<String>();
      data->cond_txt = doc["now"]["text"].as<String>();
      data->iconMeteoCon = getMeteoconIcon(cond_code);

      Serial.printf("[HTTPS] doUpdateCurr tmp:%s, fl:%s, hum:%s, wind_sc:%s\n",
                    data->tmp.c_str(), data->fl.c_str(), data->hum.c_str(), data->wind_sc.c_str());

    } else {
      Serial.printf("[HTTPS] GET failed, error code: %s\n", httpCode);
      data->tmp = "-1";
      data->fl = "-1";
      data->hum = "-1";
      data->wind_sc = "-1";
      data->cond_txt = "no network";
      data->iconMeteoCon = ")";
    }
  } else {
    Serial.printf("[HTTPS] Unable to connect\n");
    data->tmp = "-1";
    data->fl = "-1";
    data->hum = "-1";
    data->wind_sc = "-1";
    data->cond_txt = "no network";
    data->iconMeteoCon = ")";
  }
}

void HeFeng::doUpdateFore(HeFengForeData *data, const String& key,
                          const String& location) {  // 获取预报

  std::unique_ptr<BearSSL::WiFiClientSecure> client(
      new BearSSL::WiFiClientSecure);
  client->setInsecure();

  String url =
      "https://devapi.qweather.com/v7/weather/3d?lang=zh&location=" + location +
      "&key=" + key;
  Serial.printf("[HTTPS] doUpdateFore begin... url:%s\n", url.c_str());

  uint8_t *outbuf = NULL;
  size_t len = 0;
  bool result = HttpsGetUtils::getString(url.c_str(), outbuf, len);
  Serial.printf("result=%d, len=%d\n", result, len);

  if (outbuf && len) {
    Serial.println("doUpdateFore parse json");
    DynamicJsonDocument doc(3072);
    DeserializationError error = deserializeJson(doc, (char *)outbuf, len);
    
    // 解析完成后立即释放 outbuf，防止内存泄漏
    free(outbuf);
    outbuf = NULL;

    if (error) {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.f_str());
      return;
    }
    
    const char* httpCode = doc["code"];
    Serial.println("doUpdateFore httpCode:");
    Serial.println(httpCode);

    if (strcmp(httpCode, "200") == 0) {
      JsonObject root = doc.as<JsonObject>();
      for (int i = 0; i < 3; i++) {
        data[i].tmp_min = root["daily"][i]["tempMin"].as<String>();
        data[i].tmp_max = root["daily"][i]["tempMax"].as<String>();
        String datestr = root["daily"][i]["fxDate"].as<String>();
        data[i].datestr = datestr.substring(5, datestr.length());
        String cond_code = root["daily"][i]["iconDay"].as<String>();
        data[i].iconMeteoCon = getMeteoconIcon(cond_code);

        Serial.printf(
            "[HTTPS] doUpdateFore tmp_min:%s, tmp_max:%s, datestr:%s, cond_code:%s\n",
            data[i].tmp_min.c_str(), data[i].tmp_max.c_str(), data[i].datestr.c_str(),
            cond_code.c_str());
      }
    } else {
      Serial.printf("[HTTPS] GET failed, error code: %s\n", httpCode);
      for (int i = 0; i < 3; i++) {
        data[i].tmp_min = "-1";
        data[i].tmp_max = "-1";
        data[i].datestr = "N/A";
        data[i].iconMeteoCon = ")";
      }
    }
  } else {
    Serial.printf("[HTTPS] doUpdateFore Unable to connect\n");
    for (int i = 0; i < 3; i++) {
      data[i].tmp_min = "-1";
      data[i].tmp_max = "-1";
      data[i].datestr = "N/A";
      data[i].iconMeteoCon = ")";
    }
  }
}
