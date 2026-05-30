#include "HttpsGetUtils.h"
#include "ArduinoUZlib.h" // gzip库

HttpsGetUtils::HttpsGetUtils() {
}

void heap(){
  Serial.print("FREE_HEAP[");
  Serial.print(ESP.getFreeHeap());
  Serial.print("]\n");
}

void log(const char *str) {
  Serial.println(str);
}

// 尝试获取信息，成功返回true，失败返回false
String HttpsGetUtils::get(String url) {
  // https请求
    WiFiClient client;
    HTTPClient http;

  #ifdef DEBUG
  Serial.print("[HTTP] begin...\n");
  #endif

  if (http.begin(client, url)) {  // HTTP连接成功
    #ifdef DEBUG
    Serial.print("[HTTP] GET...\n");
    #endif
    int httpCode = http.GET(); // 请求

    if (httpCode > 0) { // 错误返回负值
      #ifdef DEBUG
      Serial.printf("[HTTP] GET... code: %d\n", httpCode);
      #endif
      if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) { // 服务器响应
        String payload = http.getString();
        #ifdef DEBUG
        Serial.printf("payload size=%d\n", payload.length());
        #endif
        http.end();
        return payload;
      }
    } else { // 错误返回负值
      #ifdef DEBUG
      Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
      #endif
    }
    http.end();
  } else { // HTTP连接失败
    #ifdef DEBUG
    Serial.printf("[HTTP] Unable to connect\n");
    #endif
  }
  return ""; // 确保所有路径都有返回值
}

// 配置请求信息，私钥、位置、单位、语言
bool HttpsGetUtils::fetchBuffer(const char *url) {
    _bufferSize=0;
    std::unique_ptr<BearSSL::WiFiClientSecure> client(new BearSSL::WiFiClientSecure);
    client->setInsecure();
    Serial.print("[HTTPS] begin...\n");
    HTTPClient https;
    if (https.begin(*client, url)) {
      https.addHeader("Accept-Encoding", "gzip");
      Serial.print("[HTTPS] GET...\n");
      // start connection and send HTTP header
      int httpCode = https.GET();
      if (httpCode > 0) {
        // HTTP header has been send and Server response header has been handled
        Serial.printf("[HTTPS] GET... code: %d\n", httpCode);

        // file found at server
        if (httpCode == HTTP_CODE_OK) {

          // get length of document (is -1 when Server sends no Content-Length header)
          int len = https.getSize();

          // create buffer for read
          static uint8_t buff[128] = { 0 };

          // read all data from server
          int offset=0;
          while (https.connected() && (len > 0 || len == -1)) {
            // get available data size
            size_t size = client->available();
            
            if (size) {
              // read up to 128 bytes
              int c = client->readBytes(buff, ((size > sizeof(buff)) ? sizeof(buff) : size));
              // 防止缓冲区溢出：检查写入后是否超出 _buffer 容量
              if (offset + (int)c > (int)sizeof(_buffer)) {
                Serial.printf("[HTTPS] WARNING: buffer overflow, truncating data! offset=%d, c=%d, bufSize=%d\n", offset, c, (int)sizeof(_buffer));
                c = (int)sizeof(_buffer) - offset;
                if (c <= 0) break;
              }
              memcpy(_buffer+offset, buff, sizeof(uint8_t)*c);
              offset+=c;
              if (len > 0) {
                len -= c;
              } 
            }
            delay(1);
          }
          _bufferSize=offset;
          Serial.printf("offset=%d\n", offset);
          Serial.print("[HTTPS] connection closed or file end.\n");
          
        }
      } else {
        Serial.printf("[HTTPS] GET... failed, error: %s\n", https.errorToString(httpCode).c_str());
      }
      https.end();

    } else {
      Serial.printf("Unable to connect\n");
    }
    return _bufferSize>0;
}

uint8_t HttpsGetUtils::_buffer[HTTPS_BUFFER_SIZE];
const char* HttpsGetUtils::host = "https://devapi.qweather.com"; // 代理服务器地址
size_t HttpsGetUtils::_bufferSize=0;

bool HttpsGetUtils::getString(const char* url, uint8_t *& outbuf, size_t &outlen) {
  // 确保调用前释放上次分配的内存，防止内存泄漏
  if (outbuf != NULL) {
    free(outbuf);
    outbuf = NULL;
    outlen = 0;
  }

  fetchBuffer(url);
  Serial.printf("\nAfter fetch, buffer size=%d\n", _bufferSize);
  if(_bufferSize) {
    // write it to Serial 
    Serial.write(_buffer,_bufferSize);
    ArduinoUZlib::decompress(_buffer, _bufferSize, outbuf, outlen);
    Serial.printf("outsize=%d\n", outlen);
    
    _bufferSize=0;
    return true;
  } else {
    Serial.println("no available size!");
  }
  return false;
}
