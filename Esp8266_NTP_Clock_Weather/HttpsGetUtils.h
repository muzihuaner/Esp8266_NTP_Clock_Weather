#ifndef _HTTPS_GET_UTILS_H_
#define _HTTPS_GET_UTILS_H_

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>

#define DEBUG // 调试用

// gzip 响应最大缓冲区大小，和风天气 API 典型响应 < 1KB，预留 2KB
#define HTTPS_BUFFER_SIZE 2048

class HttpsGetUtils {

  public:
    HttpsGetUtils();

    static bool getString(const char* url, uint8_t*& outbuf, size_t &len);
    static String get(String str);
    static const char  *host;

  private:
    static bool fetchBuffer(const char* url);
    static uint8_t _buffer[HTTPS_BUFFER_SIZE];
    static size_t _bufferSize;

};

#endif
