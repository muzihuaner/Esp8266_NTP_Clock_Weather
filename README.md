# Esp8266_NTP_Clock_Weather
Esp8266 NTP时钟和气象站(使用和风天气API) 使用NTP网络时间 不用对时永远准确  
看不到后边的图片的同学请自行爬梯 或看Pictures里的图  
![screen1.jpg](https://i.loli.net/2020/05/08/stXPQ8oKS5dcevk.jpg)

![screen2.jpg](https://i.loli.net/2020/05/08/zhkfZmyJbT7N4vi.jpg)

![screen3.jpg](https://i.loli.net/2020/05/08/I43O2ELxYrWvAoD.jpg)

## 视频教程
B站 Monster阿原  除了此项目还有其他有意思的视频分享   
https://space.bilibili.com/402654671

## 材料
![list.jpg](https://i.loli.net/2020/05/08/1uaDPfY3cjU784m.jpg)

Esp8266 mini D1  
1.3英寸Oled屏幕(SH1106/SSD1306) IIC 4线版(本程序仅适用于IIC 4线版本 SPI 7线版本需要改程序)  
DS18B20 / 4.7k 上拉电阻  
3D打印外壳  
## 程序
根据ESP8266 weather station demo修改而成， 修改了部分显示样式，主要使用了和风天气API，方便国内用户使用，并且可预报未来三天的最低/高气温，原版的openweathermap免费版API是不行的，增加了DS18B20温度传感器右下角显示室内实时温度。  
## Arduino 库
1.自行设置安装ESP8266开发板  
进入首选项（Preferences），找到附加开发板管理器地址（Additional Board Manager URLs），并在其后添加如下信息：  
http://arduino.esp8266.com/stable/package_esp8266com_index.json  
![pic2.png](https://i.loli.net/2020/05/08/tlLTqHzFaPwCYUu.png)
点击工具 - 开发板 - 开发板管理器  
搜索ESP8266并安装。  
![pic1.png](https://i.loli.net/2020/05/08/mNBRfaV4S8sb37I.png)  

2.Library Manager中安装  
ArduinoJson库  用于解析和风天气API response的JSON  
DS18B20库      用于读取DS18B20的温度,检测房间内实时温度  
ESP8266 and ESP32 OLED driver for SSD1306 displays库   用于驱动OLED屏幕显示内容  
OneWire库      用于连接DS18B20

## 焊接/连线  
我的接线方法:(你也可以按你的喜好来,注意修改程序中的接口信息)  
![wire.png](https://i.loli.net/2020/05/08/OC8XKxFJwPenI3a.png)

## 3D打印外壳 
模型是别人做的 作者Qrome 
一个是0.96英寸版本的 https://www.thingiverse.com/thing:2884823  
一个是1.3英寸版本的 https://www.thingiverse.com/thing:2934049  
他的项目是用于显示3D打印机打印状态的https://github.com/Qrome/printer-monitor  

## 程序  
需要修改的地方：  

```
// Default WiFi Configuration (set to empty to skip default WiFi)
#define DEFAULT_WIFI_SSID "WIFINAME"
#define DEFAULT_WIFI_PASSWORD "WIFIPASSWORD"
```

如果连接不上，进行HTML配网 连接ESP8266建立的wifi 访问http://192.168.4.1 即可配网  

DS18B20 数据端(中间角) 连接的位置  

```
DS18B20 ds(D7);  
```

时区以及夏令时设置  

```
#define TZ              +8       // (utc+) TZ in hours  
#define DST_MN          0      // use 60mn for summer time in some countries  
```

第二屏实时天气 第三屏预报的更新频率  

```
const int UPDATE_INTERVAL_SECS = 20 * 60; // Update every 20 minutes  online weather 
```


DS18B20更新频率  

```
const int UPDATE_CURR_INTERVAL_SECS = 10; // Update every 10 secs DS18B20  

```

OLED地址以及管脚信息  

```
const int I2C_DISPLAY_ADDRESS = 0x3c;  
#if defined(ESP8266)  
const int SDA_PIN = D2;  
const int SDC_PIN = D5;  
#endif  
```

和风天气KEY 城市ID 自行去https://dev.heweather.com/ 注册  

城市代码查询https://github.com/qwd/LocationList   

```
const char* HEFENG_KEY="";   
const char* HEFENG_LOCATION="";  
```

OLED屏幕一般有两种驱动SSD1306和SH1106  
根据你买的屏幕 更改如下两行的信息 用错了会花屏  

```
#include "SH1106Wire.h"//or #include "SSD1306Wire.h"  
SH1106Wire     display(I2C_DISPLAY_ADDRESS, SDA_PIN, SDC_PIN);   // or SSD1306Wire  display(I2C_DISPLAY_ADDRESS, SDA_PIN, SDC_PIN);  
```

## 其他
和风天气目前免费版API提供了实时天气 天气预报 市级的空气质量等 可用的信息很多 如果你想显示其他信息 请查看API https://dev.heweather.com/docs/api/ 并更改HeFeng.app HeFeng.h中的结构体以及JSON解析信息  
如果你想用其他网站的API 请自行修改HeFeng.app HeFeng.h

## 改进

2025 更新日志：
- 更改代码为和风天气V7版本 修复bug  
- 接口默认返回的数据是Gzip压缩的，直接解析返回结果会乱码  
- 加入Gzip处理  
- 支持配置默认WIFI
- 
2026 更新日志：

🔴 关键 Bug 修复
1. 内存泄漏修复 — HttpsGetUtils.cpp
ArduinoUZlib::decompress() 在 getString() 中分配的 outbuf 内存，调用方从未释放
修复：在 doUpdateCurr() 和 doUpdateFore() 中，JSON 解析完成后立即调用 free(outbuf) 释放内存
同时在 getString() 开头增加防御性释放逻辑，防止多次调用时泄漏
1. 缓冲区溢出防护 — HttpsGetUtils.cpp
fetchBuffer() 中 _buffer 大小固定为 1280 字节，但没有边界检查，网络响应过大时会越界写入
修复：添加写入前边界检查，超限时截断并打印警告
同时：将缓冲区大小从硬编码 1280 提升为宏 HTTPS_BUFFER_SIZE 2048（HttpsGetUtils.h），定义在头文件中便于维护
1. 移除未实现函数声明 — HeFeng.h
fans() 方法在头文件中声明但从未实现，会导致链接错误
修复：删除该声明，同时清理 HeFengCurrentData 中未使用的 follower 字段
1. #endif 语法修正 — HttpsGetUtils.cpp
原代码 #endif DEBUG 应为 #endif，虽然编译器容忍但不符合标准

🟡 内存管理优化
1. HTML 存储到 Flash — Esp8266_NTP_Clock_Weather.ino
原代码 6 个 HTML 字符串使用 String 类存储在 RAM 中，占用约 2KB
修复：改为 const char[] PROGMEM + FPSTR() 宏，从 Flash 读取，节省宝贵 RAM
1. WiFi 名称改为 PROGMEM — Esp8266_NTP_Clock_Weather.ino
WDAY_NAMES 和 MONTH_NAMES 从 String[] 改为 const char* const PROGMEM[]
1. String 碎片化减少 — Esp8266_NTP_Clock_Weather.ino
drawCurrentWeather()、drawForecastDetails()、drawHeaderOverlay() 中的 String 拼接改为 snprintf() 栈上缓冲区操作
handleRoot() 和 HandleScanWifi() 中使用 str.reserve() 预分配内存
1. HeFeng.cpp String 优化 — HeFeng.cpp
getMeteoconIcon() 返回类型从 String 改为 const char*，避免每次调用创建临时 String 对象
函数参数改为 const String& 避免不必要的拷贝
🟢 健壮性改进
1.  WiFi 自动重连 — Esp8266_NTP_Clock_Weather.ino
新增 checkAndReconnectWifi() 函数，每 5 分钟检查 WiFi 状态
断线时自动调用 WiFi.reconnect() 并设置 10 秒超时
在 loop() 中定期调用
天气更新前检查 WiFi 状态，避免无效请求
1.  重试间隔 — Esp8266_NTP_Clock_Weather.ino
天气更新重试循环中增加 delay(1000) 间隔，避免连续快速请求
1.  缓冲区大小可配置化 — HttpsGetUtils.h
通过 #define HTTPS_BUFFER_SIZE 宏统一管理，便于根据实际 API 响应大小调整
🔵 代码质量改进
1.  引脚命名修正 — Esp8266_NTP_Clock_Weather.ino
SDC_PIN → SCL_PIN（I2C 时钟线标准命名为 SCL）
1.  天气图标查找表优化 — HeFeng.cpp
getMeteoconIcon() 从 30+ 行 if-else 链改为结构体查找表 iconTable[]，更高效且易于维护
1.  清理未使用代码
移除 HttpsGetUtils.h 中未使用的 _host 和 httpsPort 成员
移除 Esp8266_NTP_Clock_Weather.ino 中未使用的 date 变量
清理注释中的调试代码残留
