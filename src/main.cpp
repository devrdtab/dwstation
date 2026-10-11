#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
#include <esp_system.h>

#include "boot/BootManager.h"
#include "config/Config.h"
#include "display/Display.h"
#include "network/RemoteSetup.h"
#include "network/WifiConnection.h"
#include "pressure/PressureHistory.h"
#include "sensors/Sensors.h"
#include "time/TimeManager.h"

unsigned long lastUpdate = 0;
unsigned long lastTimeCheck = 0;

static const char *resetReasonName(esp_reset_reason_t reason) {
  switch (reason) {
  case ESP_RST_POWERON:
    return "POWERON (включение питания)";
  case ESP_RST_EXT:
    return "EXT (внешний сброс)";
  case ESP_RST_SW:
    return "SW (программный перезапуск)";
  case ESP_RST_PANIC:
    return "PANIC (сбой в программе)";
  case ESP_RST_INT_WDT:
    return "INT_WDT (сторожевой таймер)";
  case ESP_RST_TASK_WDT:
    return "TASK_WDT (сторожевой таймер задачи)";
  case ESP_RST_WDT:
    return "WDT (сторожевой таймер)";
  case ESP_RST_DEEPSLEEP:
    return "DEEPSLEEP";
  case ESP_RST_BROWNOUT:
    return "BROWNOUT (просадка питания!)";
  default:
    return "другая причина";
  }
}

// Сканирует шину, печатает найденные адреса
// и возвращает адрес дисплея (0x3C или 0x3D), либо 0, если его нет
static uint8_t findOledAddress() {
  uint8_t oledAddr = 0;
  int found = 0;
  Serial.println("I2C scan:");
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.printf("  0x%02X\n", addr);
      found++;
      if (addr == 0x3C || addr == 0x3D)
        oledAddr = addr;
    }
  }
  if (found == 0) {
    Serial.println("  ничего не найдено (шина полностью молчит)");
  }
  return oledAddr;
}

void setup() {
  Serial.begin(115200);

  // Нативный USB на C3 переподключается при ресете — ждём монитор до 3 секунд
  unsigned long serialWaitStart = millis();
  while (!Serial && millis() - serialWaitStart < 3000) {
    delay(10);
  }
  delay(200);

  esp_reset_reason_t reason = esp_reset_reason();
  Serial.printf("Причина перезапуска: %s (код %d)\n", resetReasonName(reason),
                (int)reason);

  Wire.begin(I2C_SDA, I2C_SCL);

  uint8_t oledAddr = findOledAddress();
  while (oledAddr == 0) {
    Serial.println("Дисплей не найден ни на 0x3C, ни на 0x3D. Проверьте "
                   "провода, повтор через 2 с...");
    delay(2000);
    oledAddr = findOledAddress();
  }
  Serial.printf("Дисплей найден: 0x%02X\n", oledAddr);

  if (!display.begin(oledAddr, true)) {
    Serial.println("SH1106: begin() не удался");
    while (true)
      delay(1000);
  }
  display.clearDisplay();
  display.setTextColor(SH110X_WHITE);
  display.setTextSize(1);
  display.display();

  bootAnimation();

  remoteSetupInit();

  updateDisplay();
  lastUpdate = millis();
  lastTimeCheck = millis();
}

void loop() {
  unsigned long now = millis();

  wifiLoop();
  remoteSetupLoop();
  sensorsRetryMissing();

  if (now - lastTimeCheck >= NTP_CHECK_INTERVAL) {
    lastTimeCheck = now;
    timeUpdate();
  }

  tickPressureBlink();

  if (now - lastUpdate >= UPDATE_INTERVAL) {
    lastUpdate = now;
    updateDisplay();
  }
}