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

static void onNtpSync(struct timeval *tv) { ntpSyncPending = true; }

void timeInit() {
  sntp_set_time_sync_notification_cb(onNtpSync);
  configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);

  rtcOk = rtc.begin();
  if (!rtcOk) {
    Serial.println("DS3231 не найден!");
    return;
  }

  if (rtc.lostPower()) {
    Serial.println("DS3231: время недостоверно (питание терялось). Ждём NTP.");
    clockValid = false;
    return;
  }

  DateTime nowUtc = rtc.now();

  // Защита от сбоя чтения по I2C: если год совсем неправдоподобный,
  // это битые данные, а не реальное время — не доверяем им
  if (nowUtc.year() < 2024 || nowUtc.year() > 2099) {
    Serial.printf("DS3231: прочитано неправдоподобное время (%04d-%02d-%02d), "
                  "похоже на сбой чтения. Ждём NTP.\n",
                  nowUtc.year(), nowUtc.month(), nowUtc.day());
    clockValid = false;
    return;
  }

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
    rtc.adjust(DateTime((uint32_t)nowUtc));
    Serial.println("NTP получен, RTC синхронизирован.");
  } else {
    Serial.println("NTP получен (RTC отсутствует).");
  }
  return true;
}

bool timeIsValid() { return clockValid; }
bool timeRtcOk() { return rtcOk; }
bool timeNtpSynced() { return ntpSynced; }