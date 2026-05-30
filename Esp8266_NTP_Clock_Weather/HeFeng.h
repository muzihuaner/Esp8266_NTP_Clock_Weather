
#pragma once
#include <ArduinoJson.h>

typedef struct HeFengCurrentData {
  String cond_txt;
  String fl;
  String tmp;
  String hum;
  String wind_sc;
  String iconMeteoCon;
} HeFengCurrentData;

typedef struct HeFengForeData {
  String datestr;
  String tmp_min;
  String tmp_max;
  String iconMeteoCon;
} HeFengForeData;

class HeFeng {
  private:
    static const char* getMeteoconIcon(const String& cond_code);
  public:
    HeFeng();
    void doUpdateCurr(HeFengCurrentData *data, const String& key, const String& location);
    void doUpdateFore(HeFengForeData *data, const String& key, const String& location);
};
