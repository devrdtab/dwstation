#pragma once
#include <Arduino.h>

constexpr char NTP_SERVER[] = "pool.ntp.org";
constexpr long GMT_OFFSET_SEC = 3 * 3600;
constexpr int DAYLIGHT_OFFSET_SEC = 0;
constexpr unsigned long NTP_CHECK_INTERVAL = 1000;
constexpr unsigned long NTP_BOOT_TIMEOUT = 10000;

extern bool ntpTimeValid;

void ntpStart();          // configTime(...)
bool ntpUpdateStatus();   // проверка getLocalTime, обновляет ntpTimeValid