#include "Display.h"
#include "calibration/Calibration.h"
#include "config/Config.h"
#include "moon/MoonPhase.h"
#include "network/WifiConnection.h"
#include "pressure/PressureHistory.h"
#include "sensors/Sensors.h"
#include <WiFi.h>
#include <Wire.h>
#include <time.h>

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

static int lineProgress = 0;

static void drawAnimatedLine() {
  for (int x = 0; x < SCREEN_WIDTH; x += 4) {
    display.drawFastHLine(x, 10, 2, SSD1306_WHITE);
  }
  if (lineProgress > 0) {
    display.drawFastHLine(0, 10, lineProgress, SSD1306_WHITE);
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

  bool currentWifi = (WiFi.status() == WL_CONNECTED);
  wifiConnected = currentWifi;

  char dateStr[9] = "";
  char dayStr[4] = "";
  char timeStr[6] = "";
  struct tm timeinfo;
  // Часы не зависят от текущего наличия WiFi — системное время продолжает идти
  // само
  bool timeValid = getLocalTime(&timeinfo, 0);

  if (timeValid) {
    strftime(dateStr, sizeof(dateStr), "%d-%m-%y", &timeinfo);
    strftime(dayStr, sizeof(dayStr), "%a", &timeinfo);
    strftime(timeStr, sizeof(timeStr), "%H:%M", &timeinfo);
  }

  time_t now = time(nullptr);
  MoonInfo moon = getMoonPhase(now);

  Serial.printf("%s %s %s  T=%.1fC H=%.1f%% P=%.1fhPa Moon=%s %.0f%% WiFi=%s "
                "Graph=%d/12 Trend=%c\n",
                dateStr, dayStr, timeStr, temperature, humidity, pressure,
                moon.name, moon.illumination, currentWifi ? "OK" : "OFF",
                getPressureHistoryCount(), getPressureTrend());

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  // ВЕРХНЯЯ СТРОКА: дата/день/время
  if (timeValid) {
    display.setCursor(0, 0);
    display.print(dateStr);
    display.setCursor(55, 0);
    display.print(dayStr);
    display.setCursor(98, 0);
    display.print(timeStr);
  } else {
    display.setCursor(0, 0);
    display.print("Waiting NTP");
  }

  // ДЕКОРАТИВНАЯ СТРОКА — она же индикатор WiFi
  if (wifiIsPortalActive()) {
    display.setCursor(0, 10);
    display.print("Setup: Meteo WiFi");
  } else if (currentWifi) {
    drawAnimatedLine();
  } else {
    display.setCursor(0, 10);
    display.print("No WiFi");
  }

  // ОСНОВНАЯ ОБЛАСТЬ
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

  drawMoon(10, 51, 7, moon.phase);

  display.setCursor(22, 45);
  display.print(timeValid ? moon.name : "DATA");

  display.setCursor(22, 55);
  if (timeValid)
    display.printf("%.0f%%", moon.illumination);
  else
    display.print("N/A");

  if (timeValid)
    drawMoonIndicator(47, 53, moon.phase);

  drawPressureGraph();

  lineProgress += 4;
  if (lineProgress >= SCREEN_WIDTH)
    lineProgress = 0;

  display.display();
}