#include "TimeManager.h"
#include <time.h>

bool ntpTimeValid = false;

void ntpStart() {
  configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
  Serial.println();
  Serial.println("================================");
  Serial.println("ЗАПУСК NTP");
  Serial.println("================================");
  Serial.println("Ожидание получения времени...");
}

bool ntpUpdateStatus() {
  struct tm timeinfo;
  bool result = getLocalTime(&timeinfo, 100);
  if (result) {
    if (!ntpTimeValid) {
      Serial.println();
      Serial.println("================================");
      Serial.println("NTP ВРЕМЯ ПОЛУЧЕНО");
      Serial.println("================================");
      Serial.printf("Time: %02d:%02d:%02d\n", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    }
    ntpTimeValid = true;
    return true;
  }
  ntpTimeValid = false;
  return false;
}