#include "Display.h"
#include "battery/Battery.h"
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

static float lastTemperature = NAN;
static float lastHumidity = NAN;
static float lastPressure = NAN;
static AirQuality lastAir = {0, 0, 0, ENS_STATUS_STARTUP, false};

static bool displayOn = true;
static int screenPage = 0;
static unsigned long screenTimerStart = 0;

static int lineProgress = 0;

static void drawAnimatedLine() {
  for (int x = 0; x < SCREEN_WIDTH; x += 4) {
    display.drawFastHLine(x, 10, 2, SH110X_WHITE);
  }
  if (lineProgress > 0) {
    display.drawFastHLine(0, 10, lineProgress, SH110X_WHITE);
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

static void drawBatteryIndicator() {
  display.setCursor(104, 0);
  display.printf("%3d%%", batteryPercent());
}

static void drawMainPage(float temperature, float humidity, float pressure,
                         const MoonInfo &moon) {
  display.drawFastVLine(64, 16, 48, SH110X_WHITE);
  display.drawFastHLine(0, 37, 64, SH110X_WHITE);
  display.drawFastHLine(65, 37, 63, SH110X_WHITE);

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
  display.drawFastHLine(0, 28, SCREEN_WIDTH, SH110X_WHITE);

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

static void setPanelPower(bool on) {
  display.oled_command(on ? SH110X_DISPLAYON : SH110X_DISPLAYOFF);
}

void updateMeasurements() {
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

  lastTemperature = temperature;
  lastHumidity = humidity;
  lastPressure = pressure;
  lastAir = air;

  char dateStr[9] = "";
  char dayStr[4] = "";
  char timeStr[6] = "";
  struct tm timeinfo;
  bool clockOk = timeIsValid() && getLocalTime(&timeinfo, 0);

  if (clockOk) {
    strftime(dateStr, sizeof(dateStr), "%d.%m.%y", &timeinfo);
    strftime(dayStr, sizeof(dayStr), "%a", &timeinfo);
    strftime(timeStr, sizeof(timeStr), "%H:%M", &timeinfo);
  }

  MoonInfo moon = getMoonPhase(time(nullptr));
  bool currentWifi = (WiFi.status() == WL_CONNECTED);

  Serial.printf("%s %s %s  T=%.1fC H=%.1f%% P=%.1fhPa CO2=%u TVOC=%u AQI=%u "
                "Moon=%s %.0f%% WiFi=%s RTC=%s Batt=%d%%(%.2fV) Screen=%s\n",
                dateStr, dayStr, timeStr, temperature, humidity, pressure,
                air.eco2, air.tvoc, air.aqi, moon.name, moon.illumination,
                currentWifi ? "OK" : "OFF", timeRtcOk() ? "OK" : "OFF",
                batteryPercent(), batteryVoltage(),
                displayOn ? (screenPage == 0 ? "ON-1" : "ON-2") : "OFF");
}

void renderScreen() {
  if (!displayOn)
    return;

  bool currentWifi = (WiFi.status() == WL_CONNECTED);

  char dateStr[9] = "";
  char dayStr[4] = "";
  char timeStr[6] = "";
  struct tm timeinfo;
  bool clockOk = timeIsValid() && getLocalTime(&timeinfo, 0);

  if (clockOk) {
    strftime(dateStr, sizeof(dateStr), "%d.%m%y", &timeinfo);
    strftime(dayStr, sizeof(dayStr), "%a", &timeinfo);
    strftime(timeStr, sizeof(timeStr), "%H:%M", &timeinfo);
  }

  MoonInfo moon = getMoonPhase(time(nullptr));

  display.clearDisplay();
  display.setTextColor(SH110X_WHITE);
  display.setTextSize(1);

  // ВЕРХНЯЯ СТРОКА: дата, день, время, заряд батареи
  // if (clockOk) {
  //   display.setCursor(0, 0);
  //   display.print(dateStr);
  //   display.setCursor(50, 0);
  //   display.print(dayStr);
  //   display.setCursor(70, 0);
  //   display.print(timeStr);
  // } else {
  //   display.setCursor(0, 0);
  //   display.print("No time");
  // }
  // ВТОРАЯ СТРОКА — только предупреждение о портале, иначе просто линия
  if (wifiIsPortalActive()) {
    display.setCursor(0, 10);
    display.print("Setup: Meteo WiFi");
  } else {
    drawAnimatedLine();
  }
  drawBatteryIndicator();

  // ВТОРАЯ СТРОКА — статус WiFi/портала
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
    drawMainPage(lastTemperature, lastHumidity, lastPressure, moon);
  else
    drawAirPage(lastAir);

  lineProgress += 4;
  if (lineProgress >= SCREEN_WIDTH)
    lineProgress = 0;

  display.display();
}

void displayWake() {
  screenPage = 0;
  screenTimerStart = millis();
  if (!displayOn) {
    displayOn = true;
    renderScreen();
    setPanelPower(true);
  } else {
    renderScreen();
  }
}

void displayToggleScreenPage() {
  if (!displayOn)
    return;
  screenPage = (screenPage + 1) % 2;
  screenTimerStart = millis();
  renderScreen();
}

void displayExtendTimer() {
  if (!displayOn)
    return;
  screenTimerStart = millis();
}

void displayTick() {
  if (displayOn && millis() - screenTimerStart >= SCREEN_ON_TIME_MS) {
    displayOn = false;
    display.clearDisplay();
    display.display();
    setPanelPower(false);
  }
}

bool displayIsOn() { return displayOn; }