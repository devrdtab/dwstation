#include "BootManager.h"
#include "calibration/Calibration.h"
#include "config/Config.h"
#include "display/Display.h"
#include "network/WifiConnection.h"
#include "sensors/Sensors.h"
#include "time/TimeManager.h"
#include <math.h>

static void showBootScreen(int percent, const char *status, bool animateDots) {
  int barWidth = map(percent, 0, 100, 0, SCREEN_WIDTH - 8);
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  const char *title = "MINI WEATHER";
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(title, 0, 2, &x1, &y1, &w, &h);
  display.setCursor((SCREEN_WIDTH - w) / 2, 2);
  display.print(title);

  display.setCursor(0, 24);
  display.print("Loading:");
  if (percent < 10)
    display.print("  ");
  else if (percent < 100)
    display.print(" ");
  display.print(percent);
  display.print("%");

  display.drawRect(4, 36, SCREEN_WIDTH - 8, 10, SSD1306_WHITE);
  if (barWidth > 0)
    display.fillRect(5, 37, barWidth - 1, 8, SSD1306_WHITE);

  display.setCursor(0, 57);
  display.print(status);
  if (animateDots) {
    int dots = (millis() / 350) % 4;
    for (int i = 0; i < dots; i++)
      display.print(".");
  }

  display.display();
}

static void waitForWifiSetup() {
  // Портал уже поднят в неблокирующем режиме
  while (!wifiConnected) {
    wifiLoop(); // обязателен .process() для captive portal
    int pseudoPercent =
        20 + (int)((millis() / 30) %
                   60); // "дышащий" бар — реальный % тут не имеет смысла
    showBootScreen(pseudoPercent, "Please Setup Wifi", true);
    delay(30);
  }
}

static void waitForNtp() {
  unsigned long start = millis();
  bool synced = false;
  while (!synced && millis() - start < NTP_BOOT_TIMEOUT) {
    synced = ntpUpdateStatus();
    int percent = 70 + (int)((millis() - start) * 20UL / NTP_BOOT_TIMEOUT);
    showBootScreen(percent, "Get NTP", true);
    delay(50);
  }
}

static void runCalibrationIfNeeded() {
  if (!RESET_CALIBRATION)
    return;

  showBootScreen(92, "Calibration...", false);
  delay(CALIBRATION_WARMUP);

  float tempSum = 0, humiditySum = 0, pressureSum = 0;
  int tempCount = 0, humidityCount = 0, pressureCount = 0;

  for (int i = 0; i < CALIBRATION_SAMPLES; i++) {
    float temperature, humidity, pressure;
    sensorsRead(temperature, humidity, pressure);

    if (!isnan(temperature)) {
      tempSum += temperature;
      tempCount++;
    }
    if (!isnan(humidity)) {
      humiditySum += humidity;
      humidityCount++;
    }
    if (!isnan(pressure)) {
      pressureSum += pressure;
      pressureCount++;
    }

    Serial.printf("Calibration %d/%d: ", i + 1, CALIBRATION_SAMPLES);
    if (!isnan(temperature))
      Serial.printf("T=%.2f C  ", temperature);
    if (!isnan(humidity))
      Serial.printf("H=%.2f %%  ", humidity);
    if (!isnan(pressure))
      Serial.printf("P=%.2f hPa", pressure);
    Serial.println();

    delay(CALIBRATION_INTERVAL);
  }

  if (tempCount > 0)
    temperatureOffset = REFERENCE_TEMPERATURE - (tempSum / tempCount);
  if (humidityCount > 0)
    humidityOffset = REFERENCE_HUMIDITY - (humiditySum / humidityCount);
  if (pressureCount > 0)
    pressureOffset = REFERENCE_PRESSURE - (pressureSum / pressureCount);

  saveCalibration();
  Serial.println("КАЛИБРОВКА ЗАВЕРШЕНА");
}

void bootAnimation() {
  showBootScreen(5, "Starting..", false);
  delay(500);

  showBootScreen(15, "Get sensors...", false);
  sensorsInit();
  loadCalibration();
  delay(500);
  wifiInit();

  bool alreadySaved = wifiWasSaved();
  const char *connectStatus =
      alreadySaved ? "Connecting to WiFi..." : "Please Setup Wifi";
  showBootScreen(20, connectStatus, false);

  bool connected = wifiConnectBlocking(); // до 15 сек на сохранённую сеть,
                                          // иначе сразу открывает портал

  if (!connected) {
    waitForWifiSetup(); // загрузка не идёт дальше, пока WiFi не настроят через
                        // портал
  }
  ntpStart(); // ← вот эта строка была потеряна — запускает SNTP-клиент
              // (configTime)
  showBootScreen(70, "Get NTP", false);
  waitForNtp();

  runCalibrationIfNeeded();

  showBootScreen(95, "Almost ready....", false);
  delay(300);
  showBootScreen(100, "DONE", false);
  delay(300);
}