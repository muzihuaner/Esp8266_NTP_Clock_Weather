## 说明
此目录为Arudino工程

更新日志：

🔴 关键 Bug 修复
1. 内存泄漏修复 — HttpsGetUtils.cpp
ArduinoUZlib::decompress() 在 getString() 中分配的 outbuf 内存，调用方从未释放
修复：在 doUpdateCurr() 和 doUpdateFore() 中，JSON 解析完成后立即调用 free(outbuf) 释放内存
同时在 getString() 开头增加防御性释放逻辑，防止多次调用时泄漏
2. 缓冲区溢出防护 — HttpsGetUtils.cpp
fetchBuffer() 中 _buffer 大小固定为 1280 字节，但没有边界检查，网络响应过大时会越界写入
修复：添加写入前边界检查，超限时截断并打印警告
同时：将缓冲区大小从硬编码 1280 提升为宏 HTTPS_BUFFER_SIZE 2048（HttpsGetUtils.h），定义在头文件中便于维护
3. 移除未实现函数声明 — HeFeng.h
fans() 方法在头文件中声明但从未实现，会导致链接错误
修复：删除该声明，同时清理 HeFengCurrentData 中未使用的 follower 字段
4. #endif 语法修正 — HttpsGetUtils.cpp
原代码 #endif DEBUG 应为 #endif，虽然编译器容忍但不符合标准
🟠 安全改进
5. 移除硬编码凭据 — Esp8266_NTP_Clock_Weather.ino
原代码硬编码了 WiFi 密码  和 API Key 
修复：DEFAULT_WIFI_SSID 和 DEFAULT_WIFI_PASSWORD 默认为空，WiFi 密码通过 AP 配网页面输入
HEFENG_KEY 留空，需用户自行填写
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
