#include "Sensors.h"
#include <Adafruit_AHTX0.h>
#include <Adafruit_BMP280.h>

static Adafruit_AHTX0 aht;
static Adafruit_BMP280 bmp;
static bool ahtOk = false;
static bool bmpOk = false;

void sensorsInit() {
  ahtOk = aht.begin();
  Serial.println(ahtOk ? "AHT20 OK" : "AHT20 не найден!");

  bmpOk = bmp.begin(0x76);
  if (!bmpOk) bmpOk = bmp.begin(0x77);
  if (!bmpOk) {
    Serial.println("BMP280 не найден!");
  } else {
    Serial.println("BMP280 OK");
    bmp.setSampling(
      Adafruit_BMP280::MODE_NORMAL,
      Adafruit_BMP280::SAMPLING_X2,
      Adafruit_BMP280::SAMPLING_X16,
      Adafruit_BMP280::FILTER_X16,
      Adafruit_BMP280::STANDBY_MS_500
    );
  }
}

void sensorsRead(float &temperature, float &humidity, float &pressure) {
  temperature = NAN;
  humidity = NAN;
  pressure = NAN;

  if (ahtOk) {
    sensors_event_t humEvent, tempEvent;
    aht.getEvent(&humEvent, &tempEvent);
    temperature = tempEvent.temperature;
    humidity = humEvent.relative_humidity;
  }
  if (bmpOk) {
    pressure = bmp.readPressure() / 100.0F;
    if (!ahtOk) temperature = bmp.readTemperature();
  }
}