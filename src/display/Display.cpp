#include "Display.h"
#include "config/Config.h"
#include "sensors/Sensors.h"
#include "calibration/Calibration.h"
#include "moon/MoonPhase.h"
#include "pressure/PressureHistory.h"
#include "network/WiFiManager.h"
#include "time/TimeManager.h"
#include <WiFi.h>
#include <time.h>

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

static int lineProgress = 0;
static float wavePhase = 0.0;
static unsigned long lastWaveUpdate = 0;
static const unsigned long WAVE_INTERVAL = 40;
static const float WAVE_STEP = 0.35;

static void drawAnimatedLine() {
  for (int x = 0; x < SCREEN_WIDTH; x += 4) {
    display.drawFastHLine(x, 10, 2, SSD1306_WHITE);
  }
  if (lineProgress > 0) display.drawFastHLine(0, 10, lineProgress, SSD1306_WHITE);
}

static void drawWaveGrid() {
  for (int y = 1; y < 16; y += 5) {
    for (int x = 0; x < SCREEN_WIDTH; x += 6) {
      display.drawFastHLine(x, y, 3, SSD1306_WHITE);
    }
  }
  for (int x = 8; x < SCREEN_WIDTH; x += 16) {
    for (int y = 0; y < 16; y += 4) {
      display.drawFastVLine(x, y, 2, SSD1306_WHITE);
    }
  }
}

static void drawAnimatedWave() {
  const float CENTER_Y = 8.0;
  const float AMPLITUDE = 6.0;
  const float FREQUENCY = 0.19635;

  drawWaveGrid();

  int previousY = (int)(CENTER_Y + AMPLITUDE * sin(wavePhase));
  for (int x = 1; x < SCREEN_WIDTH; x++) {
    int y = (int)(CENTER_Y + AMPLITUDE * sin(wavePhase + x * FREQUENCY));
    display.drawLine(x - 1, previousY, x, y, SSD1306_WHITE);
    previousY = y;
  }
}

void tickWaveAnimation() {
  unsigned long now = millis();
  if (now - lastWaveUpdate >= WAVE_INTERVAL) {
    lastWaveUpdate = now;
    wavePhase += WAVE_STEP;
    if (wavePhase >= TWO_PI) wavePhase -= TWO_PI;
    updateDisplay();
  }
}

void updateDisplay() {
  float temperature = NAN, humidity = NAN, pressure = NAN;
  sensorsRead(temperature, humidity, pressure);

  if (!isnan(temperature)) temperature += temperatureOffset;
  if (!isnan(humidity)) humidity += humidityOffset;
  if (!isnan(pressure)) pressure += pressureOffset;

  updatePressureHistory(pressure);

  bool currentWifi = WiFi.status() == WL_CONNECTED;
  wifiConnected = currentWifi;

  char dateStr[9] = "";
  char dayStr[4] = "";
  char timeStr[6] = "";
  struct tm timeinfo;
  bool timeValid = false;

  if (ntpTimeValid) {
    timeValid = getLocalTime(&timeinfo, 0);
  } else if (currentWifi) {
    timeValid = getLocalTime(&timeinfo, 0);
    if (timeValid) ntpTimeValid = true;
  }

  if (timeValid) {
    strftime(dateStr, sizeof(dateStr), "%d-%m-%y", &timeinfo);
    strftime(dayStr, sizeof(dayStr), "%a", &timeinfo);
    strftime(timeStr, sizeof(timeStr), "%H:%M", &timeinfo);
  }

  time_t now = time(nullptr);
  MoonInfo moon = getMoonPhase(now);

  Serial.printf(
    "%s %s %s  T=%.1fC H=%.1f%% P=%.1fhPa Moon=%s %.0f%% WiFi=%s NTP=%s Graph=%d/12 Trend=%c\n",
    dateStr, dayStr, timeStr, temperature, humidity, pressure,
    moon.name, moon.illumination,
    currentWifi ? "OK" : "OFF",
    ntpTimeValid ? "OK" : "WAIT",
    getPressureHistoryCount(),
    getPressureTrend()
  );

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  if (currentWifi && timeValid) {
    display.setCursor(0, 0);
    display.print(dateStr);
    display.setCursor(55, 0);
    display.print(dayStr);
    display.setCursor(98, 0);
    display.print(timeStr);
    drawAnimatedLine();
  } else if (currentWifi && !timeValid) {
    display.setCursor(0, 0);
    display.print("Waiting NTP");
    int dots = (millis() / 400) % 4;
    for (int i = 0; i < dots; i++) display.print(".");
    drawAnimatedLine();
  } else if (!currentWifi && wifiAttemptFinished && !ntpTimeValid) {
    drawAnimatedWave();
  }

  display.drawFastVLine(64, 16, 48, SSD1306_WHITE);
  display.drawFastHLine(0, 37, 64, SSD1306_WHITE);
  display.drawFastHLine(65, 37, 63, SSD1306_WHITE);

  display.setCursor(4, 17);
  display.print("Temp:");
  display.setCursor(2, 28);
  if (!isnan(temperature)) display.printf("%.1fC", temperature);
  else display.print("--.-C");

  display.setCursor(67, 17);
  display.print("P:");
  if (!isnan(pressure)) display.printf("%.0fhPa", pressure);
  else display.print("--hPa");

  display.setCursor(67, 28);
  display.print("H:");
  if (!isnan(humidity)) display.printf("%.0f%%", humidity);
  else display.print("--%");

  drawMoon(10, 51, 7, moon.phase);

  display.setCursor(22, 45);
  display.print(ntpTimeValid ? moon.name : "DATA");

  display.setCursor(22, 55);
  if (ntpTimeValid) display.printf("%.0f%%", moon.illumination);
  else display.print("N/A");

  if (ntpTimeValid) drawMoonIndicator(47, 53, moon.phase);

  drawPressureGraph();

  lineProgress += 4;
  if (lineProgress >= SCREEN_WIDTH) lineProgress = 0;

  display.display();
}