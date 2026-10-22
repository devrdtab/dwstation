#include "Battery.h"
#include "config/Config.h"

static int percent = 0;
static float voltage = 0.0;
static unsigned long lastUpdate = 0;

void batteryInit() {
  // ничего не нужно: analogRead не требует pinMode
}

void batteryUpdate() {
  if (millis() - lastUpdate < BATTERY_UPDATE_INTERVAL)
    return;
  lastUpdate = millis();

  uint32_t mv = analogReadMilliVolts(BATTERY_VOLTAGE_PIN);
  voltage = (mv / 1000.0f) * BATTERY_DIVIDER_RATIO;

  float clamped = voltage;
  if (clamped < BATTERY_EMPTY_V)
    clamped = BATTERY_EMPTY_V;
  if (clamped > BATTERY_FULL_V)
    clamped = BATTERY_FULL_V;

  percent = (int)((clamped - BATTERY_EMPTY_V) /
                      (BATTERY_FULL_V - BATTERY_EMPTY_V) * 100.0f +
                  0.5f);
}

int batteryPercent() { return percent; }
float batteryVoltage() { return voltage; }