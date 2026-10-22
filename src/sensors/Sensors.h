#pragma once
#include <Arduino.h>

const uint8_t ENS_STATUS_NORMAL = 0;
const uint8_t ENS_STATUS_WARMUP = 1;
const uint8_t ENS_STATUS_STARTUP = 2;
const uint8_t ENS_STATUS_INVALID = 3;

struct AirQuality {
  uint16_t eco2;
  uint16_t tvoc;
  uint8_t aqi;
  uint8_t status; // флаг достоверности: 0 норма, 1 прогрев, 2 первый запуск, 3
                  // нет данных
  bool valid;     // данные правдоподобны (eCO2 >= 400, AQI 1..5)
  bool commOk;    // датчик отвечает по I2C
};

void sensorsInit();
void sensorsRead(float &temperature, float &humidity, float &pressure);
AirQuality sensorsReadAirQuality(float temperature, float humidity);

bool sensorsAllOk();
void sensorsRetryMissing();

const char *ensStatusName(uint8_t status);