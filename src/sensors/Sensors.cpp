/*TVOC (ppb) — суммарная оценка концентрации летучих органических соединений.
eCO₂(ppm) — расчётный эквивалент концентрации CO₂, а не прямое измерение CO₂.
AQI(1–5) — индекс качества воздуха, рассчитанный самим датчиком на основании его
алгоритма обработки газовых показаний.*/
#include "Sensors.h"
#include "config/Config.h"

#include <Adafruit_AHTX0.h>
#include <Adafruit_BMP280.h>
#include <Wire.h>
#include <math.h>

static Adafruit_AHTX0 aht;
static Adafruit_BMP280 bmp;

static bool ahtOk = false;
static bool bmpOk = false;
static bool ensOk = false;

static bool mainBusSeen[128] = {false};

// ENS160 registers
static constexpr uint8_t ENS_REG_PART_ID = 0x00;
static constexpr uint8_t ENS_REG_OPMODE = 0x10;
static constexpr uint8_t ENS_REG_TEMP_IN = 0x13;
static constexpr uint8_t ENS_REG_DEVICE_STATUS = 0x20;

static constexpr uint8_t ENS_OPMODE_IDLE = 0x01;
static constexpr uint8_t ENS_OPMODE_STANDARD = 0x02;
static constexpr uint8_t ENS_OPMODE_RESET = 0xF0;

static constexpr uint8_t ENS_STATUS_RUNNING = 0x80;
static constexpr uint8_t ENS_STATUS_ERROR = 0x40;
static constexpr uint8_t ENS_STATUS_NEWDAT = 0x02;

static constexpr bool ENS_DEBUG_LOG = true;

static uint8_t ensCommFailures = 0;
static uint8_t ensIdleReads = 0;

static float ensLastTemp = NAN;
static float ensLastHum = NAN;

static unsigned long ensLastCompMs = 0;
static unsigned long ensLastRestartMs = 0;
static unsigned long ensLastGoodMs = 0;

static bool selectMuxChannel(uint8_t channel) {
  Wire.beginTransmission(TCA9548A_ADDR);
  Wire.write((uint8_t)(1U << channel));
  return Wire.endTransmission() == 0;
}

static void disableMuxChannels() {
  Wire.beginTransmission(TCA9548A_ADDR);
  Wire.write((uint8_t)0x00);
  Wire.endTransmission();
}

static bool ensWrite(uint8_t reg, const uint8_t *data, uint8_t len) {
  Wire.beginTransmission((uint8_t)ENS160_ADDR);
  Wire.write(reg);

  for (uint8_t i = 0; i < len; ++i) {
    Wire.write(data[i]);
  }

  return Wire.endTransmission() == 0;
}

static bool ensRead(uint8_t reg, uint8_t *buf, uint8_t len) {
  Wire.beginTransmission((uint8_t)ENS160_ADDR);
  Wire.write(reg);

  if (Wire.endTransmission(false) != 0) {
    return false;
  }

  uint8_t received = Wire.requestFrom((uint8_t)ENS160_ADDR, len);

  if (received != len) {
    while (Wire.available()) {
      Wire.read();
    }
    return false;
  }

  for (uint8_t i = 0; i < len; ++i) {
    buf[i] = (uint8_t)Wire.read();
  }

  return true;
}

static bool ensSetMode(uint8_t mode) {
  return ensWrite(ENS_REG_OPMODE, &mode, 1);
}

static bool ensReadByte(uint8_t reg, uint8_t &value) {
  return ensRead(reg, &value, 1);
}

static void ensDiagnosticDump() {
  const uint8_t regs[] = {0x00, 0x01, 0x10, 0x11, 0x12, 0x20,
                          0x21, 0x22, 0x23, 0x24, 0x25};

  Serial.println("=== ENS160 REGISTER DUMP ===");

  for (uint8_t reg : regs) {
    uint8_t value = 0xFF;

    if (ensReadByte(reg, value)) {
      Serial.printf("REG 0x%02X = 0x%02X (%u)\n", reg, value, value);
    } else {
      Serial.printf("REG 0x%02X READ ERROR\n", reg);
    }

    delay(5);
  }

  Serial.println("============================");
}

static bool ensPartIdOk(uint16_t *idOut = nullptr) {
  uint8_t id[2] = {0, 0};

  if (!ensRead(ENS_REG_PART_ID, id, 2)) {
    if (idOut) {
      *idOut = 0;
    }
    return false;
  }

  uint16_t value = (uint16_t)id[0] | ((uint16_t)id[1] << 8);

  if (idOut) {
    *idOut = value;
  }

  return value == 0x0160;
}

static bool ensWaitForRunning(uint32_t timeoutMs) {
  const unsigned long started = millis();

  while (millis() - started < timeoutMs) {
    uint8_t opmode = 0xFF;
    uint8_t status = 0;

    if (ensReadByte(ENS_REG_OPMODE, opmode) &&
        ensReadByte(ENS_REG_DEVICE_STATUS, status)) {

      if (opmode == ENS_OPMODE_STANDARD && (status & ENS_STATUS_RUNNING) &&
          !(status & ENS_STATUS_ERROR)) {
        return true;
      }
    }

    delay(50);
  }

  return false;
}

static bool ensRestartMeasurement() {
  ensLastRestartMs = millis();
  ensLastTemp = NAN;
  ensLastHum = NAN;
  ensLastCompMs = 0;
  ensLastGoodMs = 0;
  ensIdleReads = 0;

  Serial.println("ENS160: выполняется перезапуск");

  if (!ensSetMode(ENS_OPMODE_RESET)) {
    Serial.println("ENS160: ошибка записи RESET");
    return false;
  }

  delay(100);

  bool found = false;

  for (uint8_t i = 0; i < 20; ++i) {
    if (ensPartIdOk()) {
      found = true;
      break;
    }
    delay(50);
  }

  if (!found) {
    Serial.println("ENS160: PART_ID недоступен после RESET");
    return false;
  }

  if (!ensSetMode(ENS_OPMODE_IDLE)) {
    Serial.println("ENS160: ошибка перехода в IDLE");
    return false;
  }

  delay(20);

  if (!ensSetMode(ENS_OPMODE_STANDARD)) {
    Serial.println("ENS160: ошибка записи STANDARD");
    return false;
  }

  // Датчик должен подтвердить, что режим действительно запущен.
  if (!ensWaitForRunning(2000)) {
    uint8_t opmode = 0xFF;
    uint8_t status = 0;

    ensReadByte(ENS_REG_OPMODE, opmode);
    ensReadByte(ENS_REG_DEVICE_STATUS, status);

    Serial.printf("ENS160: STANDARD не подтверждён: "
                  "opmode=0x%02X status=0x%02X\n",
                  opmode, status);

    return false;
  }

  uint8_t opmode = 0xFF;
  uint8_t status = 0;

  ensReadByte(ENS_REG_OPMODE, opmode);
  ensReadByte(ENS_REG_DEVICE_STATUS, status);

  Serial.printf("ENS160: режим запущен, opmode=0x%02X status=0x%02X\n", opmode,
                status);

  return true;
}

// TEMP_IN = (temperature C + 273.15) * 64
// RH_IN = relative humidity * 512
static bool ensWriteCompensation(float temperature, float humidity) {
  if (!isfinite(temperature) || !isfinite(humidity)) {
    return false;
  }

  temperature = constrain(temperature, -40.0f, 85.0f);
  humidity = constrain(humidity, 0.0f, 100.0f);

  uint16_t t = (uint16_t)((temperature + 273.15f) * 64.0f);
  uint16_t h = (uint16_t)(humidity * 512.0f);

  uint8_t data[4] = {(uint8_t)(t & 0xFF), (uint8_t)(t >> 8),
                     (uint8_t)(h & 0xFF), (uint8_t)(h >> 8)};

  return ensWrite(ENS_REG_TEMP_IN, data, sizeof(data));
}

static void scanI2CBus() {
  Serial.println();
  Serial.println("========== I2C SCAN ==========");

  memset(mainBusSeen, 0, sizeof(mainBusSeen));

  Serial.println("Главная шина:");
  int found = 0;

  disableMuxChannels();

  for (uint8_t addr = 1; addr < 127; ++addr) {
    Wire.beginTransmission(addr);

    if (Wire.endTransmission() == 0) {
      mainBusSeen[addr] = true;
      Serial.printf("  0x%02X\n", addr);
      ++found;
    }
  }

  if (found == 0) {
    Serial.println("  Устройства не найдены");
  }

  Serial.println("Устройства за TCA9548A, канал 0:");

  if (selectMuxChannel(TCA9548A_CHANNEL_ENS160)) {
    int behind = 0;

    for (uint8_t addr = 1; addr < 127; ++addr) {
      if (mainBusSeen[addr]) {
        continue;
      }

      Wire.beginTransmission(addr);

      if (Wire.endTransmission() == 0) {
        Serial.printf("  0x%02X\n", addr);
        ++behind;
      }
    }

    if (behind == 0) {
      Serial.println("  Новых адресов не найдено");
    }
  } else {
    Serial.println("  TCA9548A не отвечает");
  }

  disableMuxChannels();
  Serial.println("==============================");
}

static void initAht() {
  ahtOk = aht.begin();

  Serial.println(ahtOk ? "AHT20 OK" : "AHT20 не найден");
}

static void initBmp() {
  bmpOk = bmp.begin(BMP280_ADDR_PRIMARY);

  if (!bmpOk) {
    bmpOk = bmp.begin(BMP280_ADDR_SECONDARY);
  }

  if (!bmpOk) {
    Serial.println("BMP280 не найден");
    return;
  }

  bmp.setSampling(Adafruit_BMP280::MODE_NORMAL, Adafruit_BMP280::SAMPLING_X2,
                  Adafruit_BMP280::SAMPLING_X16, Adafruit_BMP280::FILTER_X16,
                  Adafruit_BMP280::STANDBY_MS_500);

  Serial.println("BMP280 OK");
}

static void initEns() {
  ensOk = false;
  ensCommFailures = 0;
  ensIdleReads = 0;

  if (!selectMuxChannel(TCA9548A_CHANNEL_ENS160)) {
    disableMuxChannels();
    Serial.println("ENS160: TCA9548A не отвечает");
    return;
  }

  uint16_t id = 0;

  if (!ensPartIdOk(&id)) {
    Serial.printf("ENS160: неверный PART_ID 0x%04X, ожидался 0x0160\n", id);
    disableMuxChannels();
    return;
  }

  uint8_t opmode = 0xFF;
  uint8_t status = 0;

  bool readOk = ensReadByte(ENS_REG_OPMODE, opmode) &&
                ensReadByte(ENS_REG_DEVICE_STATUS, status);

  if (readOk && opmode == ENS_OPMODE_STANDARD &&
      (status & ENS_STATUS_RUNNING) && !(status & ENS_STATUS_ERROR)) {

    ensOk = true;
    Serial.printf("ENS160 OK: уже работает, opmode=0x%02X status=0x%02X\n",
                  opmode, status);
  } else {
    Serial.printf("ENS160: требуется запуск, opmode=0x%02X status=0x%02X\n",
                  opmode, status);

    ensOk = ensRestartMeasurement();

    if (!ensOk) {
      Serial.println("ENS160: запуск не подтверждён");
    }
  }

  disableMuxChannels();
}

void sensorsInit() {
  disableMuxChannels();
  scanI2CBus();
  initAht();
  initBmp();
  initEns();
  ensDiagnosticDump();
}

bool sensorsAllOk() { return ahtOk && bmpOk && ensOk; }

void sensorsRetryMissing() {
  if (sensorsAllOk()) {
    return;
  }

  static unsigned long lastTry = 0;

  if (millis() - lastTry < SENSOR_RETRY_INTERVAL) {
    return;
  }

  lastTry = millis();

  Serial.println();
  Serial.println("--- Повторная инициализация датчиков ---");

  if (!ahtOk) {
    initAht();
  }

  if (!bmpOk) {
    initBmp();
  }

  if (!ensOk) {
    initEns();
  }
}

void sensorsRead(float &temperature, float &humidity, float &pressure) {
  temperature = NAN;
  humidity = NAN;
  pressure = NAN;

  if (ahtOk) {
    sensors_event_t humEvent;
    sensors_event_t tempEvent;

    aht.getEvent(&humEvent, &tempEvent);

    if (isfinite(tempEvent.temperature)) {
      temperature = tempEvent.temperature;
    }

    if (isfinite(humEvent.relative_humidity)) {
      humidity = humEvent.relative_humidity;
    }
  }

  if (bmpOk) {
    float p = bmp.readPressure() / 100.0f;

    if (isfinite(p) && p > 0.0f) {
      pressure = p;
    }

    if (!ahtOk) {
      float t = bmp.readTemperature();

      if (isfinite(t)) {
        temperature = t;
      }
    }
  }
}

static void readEns(float temperature, float humidity, AirQuality &r) {
  if (!ensOk) {
    return;
  }

  if (!selectMuxChannel(TCA9548A_CHANNEL_ENS160)) {
    Serial.println("ENS160: ошибка выбора канала TCA9548A");
    ensOk = false;
    return;
  }

  uint16_t id = 0;

  if (!ensPartIdOk(&id)) {
    ++ensCommFailures;

    Serial.printf("ENS160: нет связи, PART_ID=0x%04X, сбой=%u\n", id,
                  ensCommFailures);

    if (ensCommFailures >= 5) {
      ensOk = false;
    }

    disableMuxChannels();
    return;
  }

  ensCommFailures = 0;
  r.commOk = true;

  uint8_t opmode = 0xFF;
  uint8_t status = 0;

  if (!ensReadByte(ENS_REG_OPMODE, opmode) ||
      !ensReadByte(ENS_REG_DEVICE_STATUS, status)) {
    Serial.println("ENS160: не удалось прочитать состояние");
    disableMuxChannels();
    return;
  }

  if (opmode != ENS_OPMODE_STANDARD || !(status & ENS_STATUS_RUNNING) ||
      (status & ENS_STATUS_ERROR)) {

    Serial.printf("ENS160: неисправное состояние opmode=0x%02X status=0x%02X; "
                  "перезапуск\n",
                  opmode, status);

    bool restarted = ensRestartMeasurement();
    ensOk = restarted;

    disableMuxChannels();
    return;
  }

  uint8_t validity = (status >> 2) & 0x03;

  // Передаём температурную и влажностную компенсацию,
  // когда показания заметно изменились или раз в минуту.
  bool needComp = isfinite(temperature) && isfinite(humidity) &&
                  (isnan(ensLastTemp) || isnan(ensLastHum) ||
                   fabsf(temperature - ensLastTemp) >= 0.5f ||
                   fabsf(humidity - ensLastHum) >= 3.0f ||
                   millis() - ensLastCompMs >= 60000UL);

  if (needComp && ensWriteCompensation(temperature, humidity)) {
    ensLastTemp = temperature;
    ensLastHum = humidity;
    ensLastCompMs = millis();
  }

  // Читаем регистры AQI, TVOC и eCO2.
  // DEVICE_STATUS был прочитан выше, чтобы проверить состояние
  // до чтения результатов измерений.
  uint8_t data[5] = {0};

  if (!ensRead(0x21, data, sizeof(data))) {
    Serial.println("ENS160: ошибка чтения результатов");
    disableMuxChannels();
    return;
  }

  uint8_t aqi = data[0] & 0x07;

  uint16_t tvoc = (uint16_t)data[1] | ((uint16_t)data[2] << 8);

  uint16_t eco2 = (uint16_t)data[3] | ((uint16_t)data[4] << 8);

  r.status = validity;
  r.aqi = aqi;
  r.tvoc = tvoc;
  r.eco2 = eco2;

  // validity=0 означает нормальный статус, но дополнительно
  // проверяем правдоподобность значений.

  r.valid =
      validity != ENS_STATUS_INVALID && eco2 >= 400 && aqi >= 1 && aqi <= 5;

  if (r.valid) {
    ensLastGoodMs = millis();
    ensIdleReads = 0;
  } else {
    ++ensIdleReads;
  }

  if (ENS_DEBUG_LOG) {
    Serial.printf("ENS op=0x%02X dev=0x%02X running=%u error=%u "
                  "newdat=%u validity=%u aqi=%u tvoc=%u eco2=%u valid=%u\n",
                  opmode, status, (status & ENS_STATUS_RUNNING) ? 1 : 0,
                  (status & ENS_STATUS_ERROR) ? 1 : 0,
                  (status & ENS_STATUS_NEWDAT) ? 1 : 0, validity, aqi, tvoc,
                  eco2, r.valid ? 1 : 0);
  }

  // Если датчик долго не выдаёт корректных результатов,
  // пробуем перезапустить его, не делая этого каждую секунду.
  if (ensIdleReads >= 10 && millis() - ensLastRestartMs >= 60000UL) {

    Serial.println("ENS160: десять некорректных чтений, перезапуск");

    ensOk = ensRestartMeasurement();
    ensIdleReads = 0;
  }

  disableMuxChannels();
}

AirQuality sensorsReadAirQuality(float temperature, float humidity) {
  static AirQuality lastGood = {0, 0, 0, ENS_STATUS_STARTUP, false, false};

  AirQuality r = {0, 0, 0, ENS_STATUS_STARTUP, false, false};

  readEns(temperature, humidity, r);

  if (r.valid) {
    lastGood = r;
    return r;
  }

  // Не выдаём старые измерения за текущие.
  // При отсутствии корректных данных отображаем состояние датчика.
  return r;
}

const char *ensStatusName(uint8_t status) {
  switch (status) {
  case ENS_STATUS_NORMAL:
    return "Normal";
  case ENS_STATUS_WARMUP:
    return "WarmUp";
  case ENS_STATUS_STARTUP:
    return "Startup";
  case ENS_STATUS_INVALID:
  default:
    return "Invalid";
  }
}
