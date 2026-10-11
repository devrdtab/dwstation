#include "Sensors.h"
#include "config/Config.h"
#include <Adafruit_AHTX0.h>
#include <Adafruit_BMP280.h>
#include <DFRobot_ENS160.h>
#include <Wire.h>

static Adafruit_AHTX0 aht;                            // AHT20 — главная шина
static Adafruit_BMP280 bmp;                           // главная шина
static DFRobot_ENS160_I2C ens160(&Wire, ENS160_ADDR); // за мультиплексором

static bool ahtOk = false;
static bool bmpOk = false;
static bool ensOk = false;

static bool mainBusSeen[128];

static void selectMuxChannel(uint8_t channel) {
  Wire.beginTransmission(TCA9548A_ADDR);
  Wire.write(1 << channel);
  Wire.endTransmission();
}

static void disableMuxChannels() {
  Wire.beginTransmission(TCA9548A_ADDR);
  Wire.write(0x00);
  Wire.endTransmission();
}

static void scanI2CBus() {
  Serial.println();
  Serial.println("================================");
  Serial.println("I2C SCAN");
  Serial.println("================================");

  Serial.println("Главная шина:");
  int found = 0;
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    bool present = (Wire.endTransmission() == 0);
    mainBusSeen[addr] = present;
    if (present) {
      Serial.printf("  0x%02X\n", addr);
      found++;
    }
  }
  if (found == 0) {
    Serial.println("  ничего не найдено — проверьте SDA/SCL и питание");
  }

  // Открываем канал и показываем только адреса, которых не было на главной шине
  Serial.println("За мультиплексором (канал 0), новые адреса:");
  int behind = 0;
  selectMuxChannel(TCA9548A_CHANNEL_ENS160);
  for (uint8_t addr = 1; addr < 127; addr++) {
    if (mainBusSeen[addr])
      continue;
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.printf("  0x%02X\n", addr);
      behind++;
    }
  }
  disableMuxChannels();
  if (behind == 0) {
    Serial.println("  ничего не найдено");
  }
  Serial.println("================================");
}

static void initAht() {
  ahtOk = aht.begin();
  Serial.println(ahtOk ? "AHT20 OK" : "AHT20 не найден!");
}

static void initBmp() {
  bmpOk = bmp.begin(BMP280_ADDR_PRIMARY);
  if (!bmpOk)
    bmpOk = bmp.begin(BMP280_ADDR_SECONDARY);
  if (!bmpOk) {
    Serial.println("BMP280 не найден!");
    return;
  }
  Serial.println("BMP280 OK");
  bmp.setSampling(Adafruit_BMP280::MODE_NORMAL, Adafruit_BMP280::SAMPLING_X2,
                  Adafruit_BMP280::SAMPLING_X16, Adafruit_BMP280::FILTER_X16,
                  Adafruit_BMP280::STANDBY_MS_500);
}

static void initEns() {
  selectMuxChannel(TCA9548A_CHANNEL_ENS160);
  ensOk = (ens160.begin() == NO_ERR);
  if (!ensOk) {
    Serial.println("ENS160 не найден! (проверьте TCA9548A и SD0/SC0)");
  } else {
    ens160.setPWRMode(ENS160_STANDARD_MODE);
    Serial.println("ENS160 OK (нужен прогрев ~3 мин)");
  }
  disableMuxChannels();
}

void sensorsInit() {
  disableMuxChannels();
  scanI2CBus();
  initAht();
  initBmp();
  initEns();
}

bool sensorsAllOk() { return ahtOk && bmpOk && ensOk; }

void sensorsRetryMissing() {
  if (sensorsAllOk())
    return;

  static unsigned long lastTry = 0;
  if (millis() - lastTry < SENSOR_RETRY_INTERVAL)
    return;
  lastTry = millis();

  Serial.println();
  Serial.println("--- Повторный поиск датчиков ---");
  scanI2CBus();
  if (!ahtOk)
    initAht();
  if (!bmpOk)
    initBmp();
  if (!ensOk)
    initEns();
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
    if (!ahtOk)
      temperature = bmp.readTemperature();
  }
}

AirQuality sensorsReadAirQuality(float temperature, float humidity) {
  AirQuality result = {0, 0, 0, ENS_STATUS_STARTUP, false};
  if (!ensOk)
    return result;

  selectMuxChannel(TCA9548A_CHANNEL_ENS160);

  if (!isnan(temperature) && !isnan(humidity)) {
    ens160.setTempAndHum(temperature, humidity);
  }

  result.status = ens160.getENS160Status();
  result.eco2 = ens160.getECO2();
  result.tvoc = ens160.getTVOC();
  result.aqi = ens160.getAQI();
  result.valid = (result.status == ENS_STATUS_NORMAL);

  disableMuxChannels();
  return result;
}