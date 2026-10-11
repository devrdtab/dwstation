#include "TimeManager.h"
#include <RTClib.h>
#include <esp_sntp.h>
#include <sys/time.h>
#include <time.h>

static RTC_DS3231 rtc;
static bool rtcOk = false;
static bool clockValid = false;
static bool ntpSynced = false;
static volatile bool ntpSyncPending = false;

// Вызывается ядром ESP-IDF, когда SNTP получил время
static void onNtpSync(struct timeval *tv) { ntpSyncPending = true; }

void timeInit() {
  // Часовой пояс + первый запуск SNTP. Сети ещё может не быть,
  // поэтому после подключения к WiFi вызывается timeStartNtp().
  sntp_set_time_sync_notification_cb(onNtpSync);
  configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);

  rtcOk = rtc.begin();
  if (!rtcOk) {
    Serial.println("DS3231 не найден!");
    return;
  }

  if (rtc.lostPower()) {
    Serial.println("DS3231: батарейка села, время недостоверно. Нужен NTP.");
    clockValid = false;
    return;
  }

  // В RTC храним UTC, часовой пояс применяет уже getLocalTime()
  DateTime nowUtc = rtc.now();
  struct timeval tv;
  tv.tv_sec = (time_t)nowUtc.unixtime();
  tv.tv_usec = 0;
  settimeofday(&tv, nullptr);
  clockValid = true;

  Serial.printf("Время из RTC: %04d-%02d-%02d %02d:%02d:%02d UTC\n",
                nowUtc.year(), nowUtc.month(), nowUtc.day(), nowUtc.hour(),
                nowUtc.minute(), nowUtc.second());
}

void timeStartNtp() {
  // Повторный configTime перезапускает SNTP и сразу делает новый запрос
  configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
  Serial.println("SNTP перезапущен после подключения к WiFi.");
}

bool timeUpdate() {
  if (!ntpSyncPending)
    return false;
  ntpSyncPending = false;

  ntpSynced = true;
  clockValid = true;

  if (rtcOk) {
    time_t nowUtc = time(nullptr);
    rtc.adjust(DateTime((uint32_t)nowUtc)); // подводим RTC по интернету
    Serial.println("NTP получен, RTC синхронизирован.");
  } else {
    Serial.println("NTP получен (RTC отсутствует).");
  }
  return true;
}

bool timeIsValid() { return clockValid; }
bool timeRtcOk() { return rtcOk; }
bool timeNtpSynced() { return ntpSynced; }