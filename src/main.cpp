#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>

#include "config/Config.h"
#include "display/Display.h"
#include "sensors/Sensors.h"
#include "calibration/Calibration.h"
#include "network/WiFiManager.h"
#include "time/TimeManager.h"
#include "pressure/PressureHistory.h"
#include "boot/BootManager.h"

unsigned long lastUpdate = 0;
unsigned long lastNtpCheck = 0;

void setup() {
  Serial.begin(115200);
  delay(200);
  Wire.begin(I2C_SDA, I2C_SCL);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("SSD1306 не найден!");
    while (true) delay(1000);
  }
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.display();

  sensorsInit();
  loadCalibration();

  bootAnimation();
  ntpUpdateStatus();

  updateDisplay();
  lastUpdate = millis();
  lastNtpCheck = millis();
}

void loop() {
  unsigned long now = millis();

  bool currentWifi = WiFi.status() == WL_CONNECTED;
  if (currentWifi) {
    wifiConnected = true;
  } else {
    wifiConnected = false;
    ntpTimeValid = false;
  }

  if (currentWifi && now - lastNtpCheck >= NTP_CHECK_INTERVAL) {
    lastNtpCheck = now;
    ntpUpdateStatus();
  }

  tickPressureBlink();

  if (now - lastUpdate >= UPDATE_INTERVAL) {
    lastUpdate = now;
    updateDisplay();
  }

  if (!wifiConnected && wifiAttemptFinished && !ntpTimeValid) {
    tickWaveAnimation();
  }
}