#include "BootManager.h"
#include "config/Config.h"
#include "display/Display.h"
#include "network/WiFiManager.h"
#include "time/TimeManager.h"
#include "sensors/Sensors.h"
#include "calibration/Calibration.h"
#include <WiFi.h>
#include <time.h>

static void showBootScreen(int percent, const char* status, bool wifiOk) {
  int barWidth = map(percent, 0, 100, 0, SCREEN_WIDTH - 8);
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  const char* title = "MINI WEATHER";
  int16_t x1, y1; uint16_t w, h;
  display.getTextBounds(title, 0, 2, &x1, &y1, &w, &h);
  display.setCursor((SCREEN_WIDTH - w) / 2, 2);
  display.print(title);
  display.setCursor(0, 24);
  display.print("Loading:");
  if (percent < 10) display.print("  ");
  else if (percent < 100) display.print(" ");
  display.print(percent);
  display.print("%");
  display.drawRect(4, 36, SCREEN_WIDTH - 8, 10, SSD1306_WHITE);
  if (barWidth > 0) display.fillRect(5, 37, barWidth - 1, 8, SSD1306_WHITE);
  display.setCursor(0, 57);
  display.print(status);
  if (strcmp(status, "Connecting WiFi") == 0) {
    if (wifiOk) {
      display.print(" OK");
    } else {
      int dots = (millis() / 300) % 4;
      for (int i = 0; i < dots; i++) display.print(".");
    }
  }
  if (strcmp(status, "Get NTP") == 0) {
    int dots = (millis() / 350) % 4;
    for (int i = 0; i < dots; i++) display.print(".");
  }
  display.display();
}

void bootAnimation() {
  const int totalSteps = 100;
  const int animationDelay = 20;
  const unsigned long WIFI_TIMEOUT = 10000;

  bool wifiStarted = false, wifiFinished = false;
  unsigned long wifiStartTime = 0;

  bool ntpStarted = false, ntpFinished = false;
  unsigned long ntpStartTime = 0;

  bool calibrationStarted = false, calibrationFinished = false;
  int calibrationSample = 0;
  float tempSum = 0.0, humiditySum = 0.0, pressureSum = 0.0;
  int tempCount = 0, humidityCount = 0, pressureCount = 0;
  unsigned long calibrationStartTime = 0, lastCalibrationSample = 0;

  for (int percent = 0; percent <= totalSteps; percent++) {
    // WIFI
    if (percent >= 50 && !wifiStarted) {
      wifiStarted = true;
      wifiStartTime = millis();
      wifiStart();
    }
    if (wifiStarted && !wifiFinished) {
      if (wifiIsConnected()) {
        wifiFinished = true;
        Serial.println("WiFi подключен: " + WiFi.localIP().toString());
        if (!ntpStarted) {
          ntpStarted = true;
          ntpStartTime = millis();
          ntpStart();
        }
      } else if (millis() - wifiStartTime >= WIFI_TIMEOUT) {
        wifiFinished = true;
        Serial.println("WiFi: таймаут 10 секунд");
      }
    }

    // ОЖИДАНИЕ NTP
    if (wifiFinished && wifiConnected && ntpStarted && !ntpFinished) {
      struct tm ntpTimeInfo;
      if (getLocalTime(&ntpTimeInfo, 100)) {
        ntpTimeValid = true;
        ntpFinished = true;
        Serial.println();
        Serial.println("================================");
        Serial.println("NTP УСПЕШНО ПОЛУЧЕН");
        Serial.println("================================");
        Serial.printf("Time: %02d:%02d:%02d\n", ntpTimeInfo.tm_hour, ntpTimeInfo.tm_min, ntpTimeInfo.tm_sec);
      } else if (millis() - ntpStartTime >= NTP_BOOT_TIMEOUT) {
        ntpTimeValid = false;
        ntpFinished = true;
        Serial.println();
        Serial.println("================================");
        Serial.println("NTP TIMEOUT");
        Serial.println("================================");
        Serial.println("Загрузка продолжается без времени.");
      }
    }

    // КАЛИБРОВКА
    if (percent >= 70 && !calibrationStarted) {
      calibrationStarted = true;
      calibrationStartTime = millis();
      lastCalibrationSample = millis();
      if (RESET_CALIBRATION) {
        Serial.println();
        Serial.println("================================");
        Serial.println("ЗАПУСК КАЛИБРОВКИ");
        Serial.println("================================");
        Serial.printf("Эталон T: %.2f C\n", REFERENCE_TEMPERATURE);
        Serial.printf("Эталон H: %.2f %%\n", REFERENCE_HUMIDITY);
        Serial.printf("Эталон P: %.2f hPa\n", REFERENCE_PRESSURE);
        Serial.println("Стабилизация датчиков...");
      } else {
        calibrationFinished = true;
      }
    }

    if (calibrationStarted && RESET_CALIBRATION && !calibrationFinished) {
      unsigned long elapsed = millis() - calibrationStartTime;
      if (elapsed >= CALIBRATION_WARMUP && calibrationSample < CALIBRATION_SAMPLES) {
        if (calibrationSample == 0 || millis() - lastCalibrationSample >= CALIBRATION_INTERVAL) {
          float temperature = NAN, humidity = NAN, pressure = NAN;
          sensorsRead(temperature, humidity, pressure);

          if (!isnan(temperature)) { tempSum += temperature; tempCount++; }
          if (!isnan(humidity)) { humiditySum += humidity; humidityCount++; }
          if (!isnan(pressure)) { pressureSum += pressure; pressureCount++; }

          calibrationSample++;
          lastCalibrationSample = millis();

          Serial.printf("Calibration %d/%d: ", calibrationSample, CALIBRATION_SAMPLES);
          if (!isnan(temperature)) Serial.printf("T=%.2f C  ", temperature);
          if (!isnan(humidity)) Serial.printf("H=%.2f %%  ", humidity);
          if (!isnan(pressure)) Serial.printf("P=%.2f hPa", pressure);
          Serial.println();
        }
      }

      if (calibrationSample >= CALIBRATION_SAMPLES) {
        float measuredTemperature = NAN, measuredHumidity = NAN, measuredPressure = NAN;
        if (tempCount > 0) {
          measuredTemperature = tempSum / tempCount;
          temperatureOffset = REFERENCE_TEMPERATURE - measuredTemperature;
        }
        if (humidityCount > 0) {
          measuredHumidity = humiditySum / humidityCount;
          humidityOffset = REFERENCE_HUMIDITY - measuredHumidity;
        }
        if (pressureCount > 0) {
          measuredPressure = pressureSum / pressureCount;
          pressureOffset = REFERENCE_PRESSURE - measuredPressure;
        }
        saveCalibration();
        calibrationFinished = true;

        Serial.println();
        Serial.println("================================");
        Serial.println("КАЛИБРОВКА ЗАВЕРШЕНА");
        Serial.println("================================");
        Serial.printf("Средняя T: %.2f C | Offset: %.2f C\n", measuredTemperature, temperatureOffset);
        Serial.printf("Средняя H: %.2f %% | Offset: %.2f %%\n", measuredHumidity, humidityOffset);
        Serial.printf("Среднее P: %.2f hPa | Offset: %.2f hPa\n", measuredPressure, pressureOffset);
      }
    }

    const char* status;
    if (percent < 25) status = "Starting..";
    else if (percent < 50) status = "Get sensors...";
    else if (percent < 70) status = "Connecting WiFi";
    else if (percent < 90) {
      status = (wifiConnected && ntpStarted && !ntpFinished)
        ? "Get NTP"
        : (RESET_CALIBRATION ? "Calibration..." : "Initialization...");
    } else if (percent < 100) status = "Almost ready......";
    else status = "DONE";

    showBootScreen(percent, status, WiFi.status() == WL_CONNECTED);

    if (percent >= 50 && percent < 70 && !wifiFinished) { percent--; delay(100); continue; }
    if (percent >= 70 && percent < 90 && wifiConnected && ntpStarted && !ntpFinished) { percent--; delay(100); continue; }
    if (percent >= 70 && percent < 90 && RESET_CALIBRATION && !calibrationFinished) { percent--; delay(100); continue; }

    delay(animationDelay);
  }

  wifiAttemptFinished = true;
  Serial.println();
  Serial.println("Boot animation завершена.");
  if (wifiConnected) {
    Serial.println("WiFi подключен.");
    Serial.println(ntpTimeValid ? "NTP время уже получено." : "NTP время не получено.");
  } else {
    Serial.println("WiFi отсутствует.");
  }
  delay(100);
}