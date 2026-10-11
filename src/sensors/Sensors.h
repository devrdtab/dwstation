#pragma once
#include <Arduino.h>

const uint8_t ENS_STATUS_NORMAL = 0;
const uint8_t ENS_STATUS_WARMUP = 1;
const uint8_t ENS_STATUS_STARTUP = 2;

struct AirQuality {
  uint16_t eco2;
  uint16_t tvoc;
  uint8_t aqi;
  uint8_t status;
  bool valid;
};

void sensorsInit();
void sensorsRead(float &temperature, float &humidity, float &pressure);
AirQuality sensorsReadAirQuality(float temperature, float humidity);

bool sensorsAllOk();
void sensorsRetryMissing(); // вызывать из loop(), частоту ограничивает сама