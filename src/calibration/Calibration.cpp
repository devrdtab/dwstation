#include "Calibration.h"
#include "display/Display.h"
#include <Preferences.h>

static Preferences preferences;

float temperatureOffset = 0.0;
float humidityOffset = 0.0;
float pressureOffset = 0.0;

void loadCalibration() {
  preferences.begin("calibration", false);
  bool saved = preferences.getBool("valid", false);
  if (saved) {
    temperatureOffset = preferences.getFloat("tempOff", 0.0);
    humidityOffset = preferences.getFloat("humOff", 0.0);
    pressureOffset = preferences.getFloat("pressOff", 0.0);
    Serial.println();
    Serial.println("Калибровка загружена из Flash:");
    Serial.printf("Temperature offset: %.2f C\n", temperatureOffset);
    Serial.printf("Humidity offset: %.2f %%\n", humidityOffset);
    Serial.printf("Pressure offset: %.2f hPa\n", pressureOffset);
  } else {
    Serial.println();
    Serial.println("Калибровка во Flash не найдена.");
    Serial.println("Используются поправки 0.");
  }
  preferences.end();
}

void saveCalibration() {
  preferences.begin("calibration", false);
  preferences.putFloat("tempOff", temperatureOffset);
  preferences.putFloat("humOff", humidityOffset);
  preferences.putFloat("pressOff", pressureOffset);
  preferences.putBool("valid", true);
  preferences.end();
  Serial.println("Калибровка сохранена во Flash.");
}

void showCalibrationProgress(int sample, int total) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 5);
  display.print("CALIBRATION");
  display.setCursor(0, 20);
  display.print("Samples:");
  display.print(sample);
  display.print("/");
  display.print(total);
  int width = map(sample, 0, total, 0, 120);
  display.drawRect(4, 35, 120, 10, SSD1306_WHITE);
  if (width > 2) display.fillRect(6, 37, width - 2, 6, SSD1306_WHITE);
  display.setCursor(0, 55);
  display.print("Please wait...");
  display.display();
}

void performCalibration() {
  Serial.println("performCalibration больше не используется.");
}