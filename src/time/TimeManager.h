#pragma once
#include <Arduino.h>

constexpr char NTP_SERVER[] = "pool.ntp.org";
constexpr long GMT_OFFSET_SEC = 3 * 3600;
constexpr int DAYLIGHT_OFFSET_SEC = 0;
constexpr unsigned long NTP_CHECK_INTERVAL = 1000;
constexpr unsigned long NTP_BOOT_TIMEOUT = 10000;

void timeInit();      // RTC → системные часы, настройка часового пояса и SNTP
bool timeUpdate();    // в loop(): переносит свежее NTP-время в RTC
bool timeIsValid();   // можно ли доверять текущему времени
bool timeRtcOk();     // найден ли DS3231
bool timeNtpSynced(); // было ли хоть одно успешное обращение к NTP