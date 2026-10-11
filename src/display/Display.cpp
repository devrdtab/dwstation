#include "Display.h"
#include "calibration/Calibration.h"
#include "config/Config.h"
#include "moon/MoonPhase.h"
#include "network/WifiConnection.h"
#include "pressure/PressureHistory.h"
#include "sensors/Sensors.h"
#include "time/TimeManager.h"
#include <WiFi.h>
#include <Wire.h>
#include <time.h>

Adafruit_SH1106G display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

static int lineProgress = 0;
static int screenPage = 0;
static unsigned long lastPageSwitch = 0;

static void drawAnimatedLine() {
  for (int x = 0; x < SCREEN_WIDTH; x += 4) {
    display.drawFastHLine(x, 10, 2, SSD1306_WHITE);
  }
  if (lineProgress > 0) {
    display.drawFastHLine(0, 10, lineProgress, SSD1306_WHITE);
  }
}

static const char *aqiLabel(uint8_t aqi) {
  switch (aqi) {
  case 1:
    return "EXCELLENT";
  case 2:
    return "GOOD";
  case 3:
    return "MODERATE";
  case 4:
    return "POOR";
  case 5:
    return "UNHEALTHY";
  default:
    return "--";
  }
}

static void drawMainPage(float temperature, float humidity, float pressure,
                         const MoonInfo &moon) {
  display.drawFastVLine(64, 16, 48, SSD1306_WHITE);
  display.drawFastHLine(0, 37, 64, SSD1306_WHITE);
  display.drawFastHLine(65, 37, 63, SSD1306_WHITE);

  display.setCursor(4, 17);
  display.print("Temp:");
  display.setCursor(2, 28);
  if (!isnan(temperature))
    display.printf("%.1fC", temperature);
  else
    display.print("--.-C");

  display.setCursor(67, 17);
  display.print("P:");
  if (!isnan(pressure))
    display.printf("%.0fhPa", pressure);
  else
    display.print("--hPa");

  display.setCursor(67, 28);
  display.print("H:");
  if (!isnan(humidity))
    display.printf("%.0f%%", humidity);
  else
    display.print("--%");

  bool clockOk = timeIsValid();
  drawMoon(10, 51, 7, moon.phase);

  display.setCursor(22, 45);
  display.print(clockOk ? moon.name : "DATA");

  display.setCursor(22, 55);
  if (clockOk)
    display.printf("%.0f%%", moon.illumination);
  else
    display.print("N/A");

  if (clockOk)
    drawMoonIndicator(47, 53, moon.phase);

  drawPressureGraph();
}

static void drawAirPage(const AirQuality &air) {
  display.setCursor(0, 18);
  display.print("AIR QUALITY");
  display.drawFastHLine(0, 28, SCREEN_WIDTH, SSD1306_WHITE);

  display.setCursor(0, 32);
  display.print("CO2 :");
  if (air.valid)
    display.printf("%u ppm", air.eco2);
  else
    display.print("---- ppm");

  display.setCursor(0, 42);
  display.print("TVOC:");
  if (air.valid)
    display.printf("%u ppb", air.tvoc);
  else
    display.print("---- ppb");

  display.setCursor(0, 52);
  if (air.valid) {
    display.print("AQI :");
    display.print(air.aqi);
    display.print(" ");
    display.print(aqiLabel(air.aqi));
  } else if (air.status == ENS_STATUS_WARMUP) {
    display.print("Sensor warming up");
  } else if (air.status == ENS_STATUS_STARTUP) {
    display.print("Initial start-up");
  } else {
    display.print("No sensor data");
  }
}

void updateDisplay() {
  float temperature = NAN, humidity = NAN, pressure = NAN;
  sensorsRead(temperature, humidity, pressure);

  if (!isnan(temperature))
    temperature += temperatureOffset;
  if (!isnan(humidity))
    humidity += humidityOffset;
  if (!isnan(pressure))
    pressure += pressureOffset;

  updatePressureHistory(pressure);

  AirQuality air = sensorsReadAirQuality(temperature, humidity);

  bool currentWifi = (WiFi.status() == WL_CONNECTED);
  wifiConnected = currentWifi;

  char dateStr[9] = "";
  char dayStr[4] = "";
  char timeStr[6] = "";
  struct tm timeinfo;
  bool clockOk = timeIsValid() && getLocalTime(&timeinfo, 0);

  if (clockOk) {
    strftime(dateStr, sizeof(dateStr), "%d-%m-%y", &timeinfo);
    strftime(dayStr, sizeof(dayStr), "%a", &timeinfo);
    strftime(timeStr, sizeof(timeStr), "%H:%M", &timeinfo);
  }

  time_t now = time(nullptr);
  MoonInfo moon = getMoonPhase(now);

  Serial.printf("%s %s %s  T=%.1fC H=%.1f%% P=%.1fhPa CO2=%u TVOC=%u AQI=%u "
                "Moon=%s %.0f%% WiFi=%s RTC=%s\n",
                dateStr, dayStr, timeStr, temperature, humidity, pressure,
                air.eco2, air.tvoc, air.aqi, moon.name, moon.illumination,
                currentWifi ? "OK" : "OFF", timeRtcOk() ? "OK" : "OFF");

  // ПЕРЕКЛЮЧЕНИЕ СТРАНИЦ
  if (millis() - lastPageSwitch >= SCREEN_PAGE_INTERVAL) {
    lastPageSwitch = millis();
    screenPage = (screenPage + 1) % 2;
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  // ВЕРХНЯЯ СТРОКА — одинакова на обеих страницах
  if (clockOk) {
    display.setCursor(0, 0);
    display.print(dateStr);
    display.setCursor(55, 0);
    display.print(dayStr);
    display.setCursor(98, 0);
    display.print(timeStr);
  } else {
    display.setCursor(0, 0);
    display.print("No time");
  }

  if (wifiIsPortalActive()) {
    display.setCursor(0, 10);
    display.print("Setup: Meteo WiFi");
  } else if (currentWifi) {
    drawAnimatedLine();
  } else {
    display.setCursor(0, 10);
    display.print("No WiFi");
  }

  if (screenPage == 0)
    drawMainPage(temperature, humidity, pressure, moon);
  else
    drawAirPage(air);

  lineProgress += 4;
  if (lineProgress >= SCREEN_WIDTH)
    lineProgress = 0;

  display.display();
}