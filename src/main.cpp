#include <Arduino.h>
#include <WiFi.h>
#include <time.h>
#include <Wire.h>
#include <math.h>
#include <Preferences.h>
#include <Adafruit_AHTX0.h>
#include <Adafruit_BMP280.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
// =====================================================
// WIFI
// =====================================================
const char* WIFI_SSID = " ";
const char* WIFI_PASS = " ";
// =====================================================
// NTP / ЧАСОВОЙ ПОЯС
// =====================================================
const char* NTP_SERVER = "pool.ntp.org";
const long GMT_OFFSET_SEC = 3 * 3600;
const int DAYLIGHT_OFFSET_SEC = 0;
// =====================================================
// I2C ПИНЫ ESP32-S3-DevKitC-1
// =====================================================
#define I2C_SDA 8
#define I2C_SCL 9
// =====================================================
// OLED
// =====================================================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDR 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
// =====================================================
// ДАТЧИКИ
// =====================================================
Adafruit_AHTX0 aht;
Adafruit_BMP280 bmp;
bool ahtOk = false;
bool bmpOk = false;
// =====================================================
// ОБНОВЛЕНИЕ ЭКРАНА
// =====================================================
const unsigned long UPDATE_INTERVAL = 1000;
unsigned long lastUpdate = 0;
// =====================================================
// ПРОВЕРКА NTP
// =====================================================
const unsigned long NTP_CHECK_INTERVAL = 1000;
unsigned long lastNtpCheck = 0;
// =====================================================
// ТАЙМАУТ ПОЛУЧЕНИЯ NTP ВО ВРЕМЯ ЗАГРУЗКИ
// =====================================================
//
// После подключения WiFi устройство ждёт NTP
// максимум это время.
//
// Если NTP получен раньше — загрузка продолжается сразу.
//
// Если NTP не получен — загрузка продолжается
// после таймаута, чтобы устройство не зависло.
//
const unsigned long NTP_BOOT_TIMEOUT = 10000;
// =====================================================
// АНИМИРОВАННАЯ ЛИНИЯ
// =====================================================
int lineProgress = 0;
// =====================================================
// АНИМАЦИЯ ВОЛНЫ ПРИ ОТСУТСТВИИ WIFI / ВРЕМЕНИ
// =====================================================
float wavePhase = 0.0;
unsigned long lastWaveUpdate = 0;
const unsigned long WAVE_INTERVAL = 40;
const float WAVE_STEP = 0.35;
// =====================================================
// ЭТАЛОННЫЕ ЗНАЧЕНИЯ КАЛИБРОВКИ
// =====================================================
const float REFERENCE_TEMPERATURE = 25.0;
const float REFERENCE_HUMIDITY = 48.0;
const float REFERENCE_PRESSURE = 1016.0;
// =====================================================
// СБРОС / ПОВТОРНАЯ КАЛИБРОВКА
// =====================================================
//
// true  = выполнить калибровку при загрузке
// false = использовать сохранённую калибровку
//
const bool RESET_CALIBRATION = false;
// =====================================================
// ПАРАМЕТРЫ КАЛИБРОВКИ
// =====================================================
const int CALIBRATION_SAMPLES = 10;
const unsigned long CALIBRATION_INTERVAL = 500;
const unsigned long CALIBRATION_WARMUP = 3000;
// =====================================================
// ПОПРАВКИ КАЛИБРОВКИ
// =====================================================
float temperatureOffset = 0.0;
float humidityOffset = 0.0;
float pressureOffset = 0.0;
// =====================================================
// FLASH / NVS
// =====================================================
Preferences preferences;
// =====================================================
// ГРАФИК ДАВЛЕНИЯ ЗА 12 ЧАСОВ
// =====================================================
//
// 0 = первая точка сразу после запуска
// 1 = через 1 час
// ...
// 12 = через 12 часов
//
// Всего необходимо 13 точек.
//
const int PRESSURE_HISTORY_SIZE = 13;
const unsigned long PRESSURE_SAMPLE_INTERVAL = 3600000UL;
const float PRESSURE_TREND_THRESHOLD = 0.5;
float pressureHistory[PRESSURE_HISTORY_SIZE];
int pressureHistoryCount = 0;
unsigned long lastPressureSample = 0;
// =====================================================
// СЧЁТЧИК ОТОБРАЖАЕМОГО 12-ЧАСОВОГО ЦИКЛА
// =====================================================
// Сразу после запуска показывается:
// P 1/12h
// После каждого полного часа номер увеличивается.
// После 12 часов:
// P 12/12h
// После 13 часов:
// P 1/12h
// Этот счётчик влияет ТОЛЬКО на отображение
// количества часов и не изменяет сам график.
unsigned long pressureHourCounter = 1;
bool pressureHistoryStarted = false;
bool pressureBlinkState = true;
unsigned long lastPressureBlink = 0;
// =====================================================
// СТРУКТУРА ФАЗЫ ЛУНЫ
// =====================================================
struct MoonInfo {
  float phase;
  float illumination;
  const char* name;
};
// =====================================================
// СОСТОЯНИЕ WIFI / NTP
// =====================================================
bool wifiConnected = false;
bool ntpTimeValid = false;
bool wifiAttemptFinished = false;
// =====================================================
// ПРОТОТИПЫ
// =====================================================
void bootAnimation();
void updateDisplay();
void loadCalibration();
void saveCalibration();
void performCalibration();
void showCalibrationProgress(int sample, int total);
void showBootScreen(int percent, const char* status, bool wifiOk);
MoonInfo getMoonPhase(time_t now);
void drawMoon(int cx, int cy, int r, float phase);
void drawMoonIndicator(int x, int y, float phase);
void drawAnimatedLine();
void drawWaveGrid();
void drawAnimatedWave();
bool updateNTPStatus();
double normalizeDegrees(double value);
void updatePressureHistory(float pressure);
void drawPressureGraph();
char getPressureTrend();
void drawPressureTrendIndicator(int x, int y, char trend);
// =====================================================
// SETUP
// =====================================================
void setup() {
  Serial.begin(115200);
  delay(200);
  Wire.begin(I2C_SDA, I2C_SCL);
  // ===================================================
  // OLED
  // ===================================================
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("SSD1306 не найден!");
    while (true) {
      delay(1000);
    }
  }
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.display();
  // ===================================================
  // AHT20
  // ===================================================
  ahtOk = aht.begin();
  if (!ahtOk) {
    Serial.println("AHT20 не найден!");
  }
  else {
    Serial.println("AHT20 OK");
  }
  // ===================================================
  // BMP280
  // ===================================================
  bmpOk = bmp.begin(0x76);
  if (!bmpOk) {
    bmpOk = bmp.begin(0x77);
  }
  if (!bmpOk) {
    Serial.println("BMP280 не найден!");
  }
  else {
    Serial.println("BMP280 OK");
    bmp.setSampling(
      Adafruit_BMP280::MODE_NORMAL,
      Adafruit_BMP280::SAMPLING_X2,
      Adafruit_BMP280::SAMPLING_X16,
      Adafruit_BMP280::FILTER_X16,
      Adafruit_BMP280::STANDBY_MS_500
    );
  }
  // ===================================================
  // КАЛИБРОВКА
  // ===================================================
  loadCalibration();
  // ===================================================
  // ЗАГРУЗОЧНАЯ АНИМАЦИЯ
  // ===================================================
  //
  // NTP здесь специально НЕ запускаем.
  //
  // Сначала необходимо подключиться к WiFi.
  // После получения WiFi bootAnimation() запускает NTP
  // и ждёт получения времени.
  //
  bootAnimation();
  // ===================================================
  // После bootAnimation NTP уже:
  //
  // 1. либо успешно получен;
  // 2. либо истёк таймаут.
  //
  // Поэтому здесь дополнительного ожидания NTP нет.
  // ===================================================
  updateNTPStatus();
  // ===================================================
  // Первый экран
  // ===================================================
  updateDisplay();
  lastUpdate = millis();
  lastNtpCheck = millis();
}
// =====================================================
// LOOP
// =====================================================
void loop() {
  unsigned long now = millis();
  // ===================================================
  // WIFI
  // ===================================================
  bool currentWifi = WiFi.status() == WL_CONNECTED;
  if (currentWifi) {
    wifiConnected = true;
  }
  else {
    wifiConnected = false;
    ntpTimeValid = false;
  }
  // ===================================================
  // NTP
  // Проверяем только раз в секунду
  // ===================================================
  if (currentWifi) {
    if (now - lastNtpCheck >= NTP_CHECK_INTERVAL) {
      lastNtpCheck = now;
      updateNTPStatus();
    }
  }
  // ===================================================
  // МИГАНИЕ ПОСЛЕДНЕЙ ТОЧКИ ГРАФИКА
  // ===================================================
  if (now - lastPressureBlink >= 1000) {
    lastPressureBlink = now;
    pressureBlinkState = !pressureBlinkState;
  }
  // ===================================================
  // ОБНОВЛЕНИЕ ОСНОВНОГО ЭКРАНА
  // ===================================================
  if (now - lastUpdate >= UPDATE_INTERVAL) {
    lastUpdate = now;
    updateDisplay();
  }
  // ===================================================
  // АНИМАЦИЯ ВОЛНЫ
  //
  // Только если WiFi отсутствует
  // и NTP не был получен
  // ===================================================
  if (!wifiConnected && wifiAttemptFinished && !ntpTimeValid) {
    if (now - lastWaveUpdate >= WAVE_INTERVAL) {
      lastWaveUpdate = now;
      wavePhase += WAVE_STEP;
      if (wavePhase >= TWO_PI) {
        wavePhase -= TWO_PI;
      }
      updateDisplay();
    }
  }
}
// =====================================================
// ПРОВЕРКА NTP
// =====================================================
bool updateNTPStatus() {
  struct tm timeinfo;
  bool result = getLocalTime(&timeinfo, 100);
  if (result) {
    if (!ntpTimeValid) {
      Serial.println();
      Serial.println("================================");
      Serial.println("NTP ВРЕМЯ ПОЛУЧЕНО");
      Serial.println("================================");
      Serial.printf(
        "Time: %02d:%02d:%02d\n",
        timeinfo.tm_hour,
        timeinfo.tm_min,
        timeinfo.tm_sec
      );
    }
    ntpTimeValid = true;
    return true;
  }
  ntpTimeValid = false;
  return false;
}
// =====================================================
// ЭКРАН ЗАГРУЗКИ
// =====================================================
void showBootScreen(
  int percent,
  const char* status,
  bool wifiOk
) {
  int barWidth =
    map(
      percent,
      0,
      100,
      0,
      SCREEN_WIDTH - 8
    );
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  const char* title = "MINI WEATHER";
  int16_t x1;
  int16_t y1;
  uint16_t w;
  uint16_t h;
  display.getTextBounds(
    title,
    0,
    2,
    &x1,
    &y1,
    &w,
    &h
  );
  int16_t x =
    (SCREEN_WIDTH - w) / 2;
  display.setCursor(x, 2);
  display.print(title);
  display.setCursor(0, 24);
  display.print("Loading:");
  if (percent < 10) {
    display.print("  ");
  }
  else if (percent < 100) {
    display.print(" ");
  }
  display.print(percent);
  display.print("%");
  display.drawRect(
    4,
    36,
    SCREEN_WIDTH - 8,
    10,
    SSD1306_WHITE
  );
  if (barWidth > 0) {
    display.fillRect(
      5,
      37,
      barWidth - 1,
      8,
      SSD1306_WHITE
    );
  }
  display.setCursor(0, 57);
  display.print(status);
  if (strcmp(status, "Connecting WiFi") == 0) {
    if (wifiOk) {
      display.print(" OK");
    }
    else {
      int dots =
        (millis() / 300) % 4;
      for (int i = 0; i < dots; i++) {
        display.print(".");
      }
    }
  }
  if (strcmp(status, "Get NTP") == 0) {
    int dots =
      (millis() / 350) % 4;
    for (int i = 0; i < dots; i++) {
      display.print(".");
    }
  }
  display.display();
}
// =====================================================
// СТАРТОВАЯ АНИМАЦИЯ
// =====================================================
void bootAnimation() {
  const int totalSteps = 100;
  const int animationDelay = 20;
  const unsigned long WIFI_TIMEOUT = 10000;
  bool wifiStarted = false;
  bool wifiFinished = false;
  unsigned long wifiStartTime = 0;
  bool ntpStarted = false;
  bool ntpFinished = false;
  unsigned long ntpStartTime = 0;
  bool calibrationStarted = false;
  bool calibrationFinished = false;
  int calibrationSample = 0;
  float tempSum = 0.0;
  float humiditySum = 0.0;
  float pressureSum = 0.0;
  int tempCount = 0;
  int humidityCount = 0;
  int pressureCount = 0;
  unsigned long calibrationStartTime = 0;
  unsigned long lastCalibrationSample = 0;
  for (int percent = 0;
       percent <= totalSteps;
       percent++) {
    // =================================================
    // WIFI
    // =================================================
    if (percent >= 50 && !wifiStarted) {
      wifiStarted = true;
      wifiStartTime = millis();
      WiFi.mode(WIFI_STA);
      WiFi.begin(
        WIFI_SSID,
        WIFI_PASS
      );
      Serial.println();
      Serial.println("Подключение к WiFi...");
    }
    if (wifiStarted && !wifiFinished) {
      if (WiFi.status() == WL_CONNECTED) {
        wifiFinished = true;
        wifiConnected = true;
        Serial.println(
          "WiFi подключен: " +
          WiFi.localIP().toString()
        );
        // =================================================
        // NTP
        //
        // ВАЖНО:
        // configTime() запускается ТОЛЬКО после подключения
        // к WiFi.
        // =================================================
        if (!ntpStarted) {
          ntpStarted = true;
          ntpStartTime = millis();
          configTime(
            GMT_OFFSET_SEC,
            DAYLIGHT_OFFSET_SEC,
            NTP_SERVER
          );
          Serial.println();
          Serial.println("================================");
          Serial.println("ЗАПУСК NTP");
          Serial.println("================================");
          Serial.println(
            "Ожидание получения времени..."
          );
        }
      }
      else if (
        millis() - wifiStartTime >= WIFI_TIMEOUT
      ) {
        wifiFinished = true;
        wifiConnected = false;
        Serial.println(
          "WiFi: таймаут 10 секунд"
        );
      }
    }
    // =================================================
    // ОЖИДАНИЕ NTP
    // =================================================
    //
    // После WiFi остаёмся на этапе Get NTP.
    //
    // Максимальное ожидание:
    // NTP_BOOT_TIMEOUT
    //
    // Если время получено раньше —
    // продолжаем загрузку сразу.
    //
    // Если время не получено —
    // продолжаем после таймаута.
    // =================================================
    if (
      wifiFinished &&
      wifiConnected &&
      ntpStarted &&
      !ntpFinished
    ) {
      struct tm ntpTimeInfo;
      if (getLocalTime(&ntpTimeInfo, 100)) {
        ntpTimeValid = true;
        ntpFinished = true;
        Serial.println();
        Serial.println("================================");
        Serial.println("NTP УСПЕШНО ПОЛУЧЕН");
        Serial.println("================================");
        Serial.printf(
          "Time: %02d:%02d:%02d\n",
          ntpTimeInfo.tm_hour,
          ntpTimeInfo.tm_min,
          ntpTimeInfo.tm_sec
        );
      }
      else if (
        millis() - ntpStartTime >= NTP_BOOT_TIMEOUT
      ) {
        ntpTimeValid = false;
        ntpFinished = true;
        Serial.println();
        Serial.println("================================");
        Serial.println("NTP TIMEOUT");
        Serial.println("================================");
        Serial.println(
          "Загрузка продолжается без времени."
        );
      }
    }
    // =================================================
    // КАЛИБРОВКА
    // =================================================
    if (percent >= 70 && !calibrationStarted) {
      calibrationStarted = true;
      calibrationStartTime = millis();
      lastCalibrationSample = millis();
      if (RESET_CALIBRATION) {
        Serial.println();
        Serial.println("================================");
        Serial.println("ЗАПУСК КАЛИБРОВКИ");
        Serial.println("================================");
        Serial.printf(
          "Эталон T: %.2f C\n",
          REFERENCE_TEMPERATURE
        );
        Serial.printf(
          "Эталон H: %.2f %%\n",
          REFERENCE_HUMIDITY
        );
        Serial.printf(
          "Эталон P: %.2f hPa\n",
          REFERENCE_PRESSURE
        );
        Serial.println(
          "Стабилизация датчиков..."
        );
      }
      else {
        calibrationFinished = true;
      }
    }
    // =================================================
    // ПРОЦЕСС КАЛИБРОВКИ
    // =================================================
    if (
      calibrationStarted &&
      RESET_CALIBRATION &&
      !calibrationFinished
    ) {
      unsigned long elapsed =
        millis() - calibrationStartTime;
      if (
        elapsed >= CALIBRATION_WARMUP &&
        calibrationSample < CALIBRATION_SAMPLES
      ) {
        if (
          calibrationSample == 0 ||
          millis() - lastCalibrationSample >=
          CALIBRATION_INTERVAL
        ) {
          float temperature = NAN;
          float humidity = NAN;
          float pressure = NAN;
          if (ahtOk) {
            sensors_event_t humEvent;
            sensors_event_t tempEvent;
            aht.getEvent(
              &humEvent,
              &tempEvent
            );
            temperature =
              tempEvent.temperature;
            humidity =
              humEvent.relative_humidity;
          }
          if (bmpOk) {
            pressure =
              bmp.readPressure() / 100.0F;
            if (!ahtOk) {
              temperature =
                bmp.readTemperature();
            }
          }
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
          calibrationSample++;
          lastCalibrationSample =
            millis();
          Serial.printf(
            "Calibration %d/%d: ",
            calibrationSample,
            CALIBRATION_SAMPLES
          );
          if (!isnan(temperature)) {
            Serial.printf(
              "T=%.2f C  ",
              temperature
            );
          }
          if (!isnan(humidity)) {
            Serial.printf(
              "H=%.2f %%  ",
              humidity
            );
          }
          if (!isnan(pressure)) {
            Serial.printf(
              "P=%.2f hPa",
              pressure
            );
          }
          Serial.println();
        }
      }
      // =================================================
      // ЗАВЕРШЕНИЕ КАЛИБРОВКИ
      // =================================================
      if (
        calibrationSample >=
        CALIBRATION_SAMPLES
      ) {
        float measuredTemperature = NAN;
        float measuredHumidity = NAN;
        float measuredPressure = NAN;
        if (tempCount > 0) {
          measuredTemperature =
            tempSum / tempCount;
          temperatureOffset =
            REFERENCE_TEMPERATURE -
            measuredTemperature;
        }
        if (humidityCount > 0) {
          measuredHumidity =
            humiditySum / humidityCount;
          humidityOffset =
            REFERENCE_HUMIDITY -
            measuredHumidity;
        }
        if (pressureCount > 0) {
          measuredPressure =
            pressureSum / pressureCount;
          pressureOffset =
            REFERENCE_PRESSURE -
            measuredPressure;
        }
        saveCalibration();
        calibrationFinished = true;
        Serial.println();
        Serial.println("================================");
        Serial.println("КАЛИБРОВКА ЗАВЕРШЕНА");
        Serial.println("================================");
        Serial.printf(
          "Средняя T: %.2f C | Offset: %.2f C\n",
          measuredTemperature,
          temperatureOffset
        );
        Serial.printf(
          "Средняя H: %.2f %% | Offset: %.2f %%\n",
          measuredHumidity,
          humidityOffset
        );
        Serial.printf(
          "Среднее P: %.2f hPa | Offset: %.2f hPa\n",
          measuredPressure,
          pressureOffset
        );
      }
    }
    // =================================================
    // ТЕКСТ СТАТУСА
    // =================================================
    const char* status;
    if (percent < 25) {
      status = "Starting..";
    }
    else if (percent < 50) {
      status = "Get sensors...";
    }
    else if (percent < 70) {
      status = "Connecting WiFi";
    }
    else if (percent < 90) {
      if (
        wifiConnected &&
        ntpStarted &&
        !ntpFinished
      ) {
        status = "Get NTP";
      }
      else {
        status =
          RESET_CALIBRATION
          ? "Calibration..."
          : "Initialization...";
      }
    }
    else if (percent < 100) {
      status = "Almost ready......";
    }
    else {
      status = "DONE";
    }
    showBootScreen(
      percent,
      status,
      WiFi.status() == WL_CONNECTED
    );
    // =================================================
    // ОЖИДАНИЕ WIFI
    // =================================================
    if (
      percent >= 50 &&
      percent < 70 &&
      !wifiFinished
    ) {
      percent--;
      delay(100);
      continue;
    }
    // =================================================
    // ОЖИДАНИЕ NTP
    //
    // Загрузка НЕ переходит дальше,
    // пока NTP не получен или не закончился таймаут.
    // =================================================
    if (
      percent >= 70 &&
      percent < 90 &&
      wifiConnected &&
      ntpStarted &&
      !ntpFinished
    ) {
      percent--;
      delay(100);
      continue;
    }
    // =================================================
    // ОЖИДАНИЕ КАЛИБРОВКИ
    // =================================================
    if (
      percent >= 70 &&
      percent < 90 &&
      RESET_CALIBRATION &&
      !calibrationFinished
    ) {
      percent--;
      delay(100);
      continue;
    }
    delay(animationDelay);
  }
  // ===================================================
  // BOOT FINISHED
  // ===================================================
  wifiAttemptFinished = true;
  Serial.println();
  Serial.println(
    "Boot animation завершена."
  );
  if (wifiConnected) {
    Serial.println("WiFi подключен.");
    if (ntpTimeValid) {
      Serial.println(
        "NTP время уже получено."
      );
    }
    else {
      Serial.println(
        "NTP время не получено."
      );
    }
  }
  else {
    Serial.println(
      "WiFi отсутствует."
    );
  }
  delay(100);
}
// =====================================================
// ЗАГРУЗКА КАЛИБРОВКИ ИЗ FLASH
// =====================================================
void loadCalibration() {
  preferences.begin(
    "calibration",
    false
  );
  bool saved =
    preferences.getBool(
      "valid",
      false
    );
  if (saved) {
    temperatureOffset =
      preferences.getFloat(
        "tempOff",
        0.0
      );
    humidityOffset =
      preferences.getFloat(
        "humOff",
        0.0
      );
    pressureOffset =
      preferences.getFloat(
        "pressOff",
        0.0
      );
    Serial.println();
    Serial.println(
      "Калибровка загружена из Flash:"
    );
    Serial.printf(
      "Temperature offset: %.2f C\n",
      temperatureOffset
    );
    Serial.printf(
      "Humidity offset: %.2f %%\n",
      humidityOffset
    );
    Serial.printf(
      "Pressure offset: %.2f hPa\n",
      pressureOffset
    );
  }
  else {
    Serial.println();
    Serial.println(
      "Калибровка во Flash не найдена."
    );
    Serial.println(
      "Используются поправки 0."
    );
  }
  preferences.end();
}
// =====================================================
// СОХРАНЕНИЕ КАЛИБРОВКИ В FLASH
// =====================================================
void saveCalibration() {
  preferences.begin(
    "calibration",
    false
  );
  preferences.putFloat(
    "tempOff",
    temperatureOffset
  );
  preferences.putFloat(
    "humOff",
    humidityOffset
  );
  preferences.putFloat(
    "pressOff",
    pressureOffset
  );
  preferences.putBool(
    "valid",
    true
  );
  preferences.end();
  Serial.println(
    "Калибровка сохранена во Flash."
  );
}
// =====================================================
// ЭКРАН ПРОГРЕССА КАЛИБРОВКИ
// =====================================================
void showCalibrationProgress(
  int sample,
  int total
) {
  display.clearDisplay();
  display.setTextColor(
    SSD1306_WHITE
  );
  display.setTextSize(1);
  display.setCursor(0, 5);
  display.print("CALIBRATION");
  display.setCursor(0, 20);
  display.print("Samples:");
  display.print(sample);
  display.print("/");
  display.print(total);
  int width =
    map(
      sample,
      0,
      total,
      0,
      120
    );
  display.drawRect(
    4,
    35,
    120,
    10,
    SSD1306_WHITE
  );
  if (width > 2) {
    display.fillRect(
      6,
      37,
      width - 2,
      6,
      SSD1306_WHITE
    );
  }
  display.setCursor(0, 55);
  display.print("Please wait...");
  display.display();
}
// =====================================================
// СОВМЕСТИМОСТЬ
// =====================================================
void performCalibration() {
  Serial.println(
    "performCalibration больше не используется."
  );
}
// =====================================================
// НОРМАЛИЗАЦИЯ УГЛА
// =====================================================
double normalizeDegrees(
  double value
) {
  value =
    fmod(
      value,
      360.0
    );
  if (value < 0.0) {
    value += 360.0;
  }
  return value;
}
// =====================================================
// РАСЧЁТ ФАЗЫ ЛУНЫ
// =====================================================
MoonInfo getMoonPhase(time_t now) {
  const double MOON_DEG_TO_RAD =
    PI / 180.0;
  const double MOON_RAD_TO_DEG =
    180.0 / PI;
  double jd =
    ((double)now / 86400.0) +
    2440587.5;
  double d =
    jd - 2451543.5;
  // ===================================================
  // СОЛНЦЕ
  // ===================================================
  double ws =
    (282.9404 +
     0.0000470935 * d)
    * MOON_DEG_TO_RAD;
  double es =
    0.016709 -
    0.000000001151 * d;
  double Ms =
    (356.0470 +
     0.9856002585 * d)
    * MOON_DEG_TO_RAD;
  double Es =
    Ms +
    es * sin(Ms) *
    (1.0 + es * cos(Ms));
  double xs =
    cos(Es) - es;
  double ys =
    sqrt(1.0 - es * es) *
    sin(Es);
  double vs =
    atan2(ys, xs);
  double sunLon =
    normalizeDegrees(
      (vs + ws) *
      MOON_RAD_TO_DEG
    );
  // ===================================================
  // ЛУНА
  // ===================================================
  double Nm =
    (125.1228 -
     0.0529538083 * d)
    * MOON_DEG_TO_RAD;
  double im =
    5.1454 *
    MOON_DEG_TO_RAD;
  double wm =
    (318.0634 +
     0.1643573223 * d)
    * MOON_DEG_TO_RAD;
  double am = 60.2666;
  double em = 0.054900;
  double Mm =
    (115.3654 +
     13.0649929509 * d)
    * MOON_DEG_TO_RAD;
  double Em =
    Mm +
    em * sin(Mm) *
    (1.0 + em * cos(Mm));
  double xv =
    am * (cos(Em) - em);
  double yv =
    am *
    (sqrt(1.0 - em * em) *
     sin(Em));
  double vm =
    atan2(yv, xv);
  double rm =
    sqrt(
      xv * xv +
      yv * yv
    );
  double xh =
    rm *
    (
      cos(Nm) *
      cos(vm + wm) -
      sin(Nm) *
      sin(vm + wm) *
      cos(im)
    );
  double yh =
    rm *
    (
      sin(Nm) *
      cos(vm + wm) +
      cos(Nm) *
      sin(vm + wm) *
      cos(im)
    );
  double zh =
    rm *
    sin(vm + wm) *
    sin(im);
  double moonLon =
    atan2(yh, xh);
  // ===================================================
  // ОСНОВНЫЕ ЛУННЫЕ АРГУМЕНТЫ
  // ===================================================
  double Ls =
    Ms + ws;
  double Lm =
    Mm + wm + Nm;
  double D =
    Lm - Ls;
  double F =
    Lm - Nm;
  // ===================================================
  // ПОПРАВКИ ПОЛОЖЕНИЯ ЛУНЫ
  // ===================================================
  double dlon = 0.0;
  dlon +=
    -1.274 *
    sin(Mm - 2.0 * D);
  dlon +=
    0.658 *
    sin(2.0 * D);
  dlon +=
    -0.186 *
    sin(Ms);
  dlon +=
    -0.059 *
    sin(
      2.0 * Mm -
      2.0 * D
    );
  dlon +=
    -0.057 *
    sin(
      Mm -
      2.0 * D +
      Ms
    );
  dlon +=
    0.053 *
    sin(
      Mm +
      2.0 * D
    );
  dlon +=
    0.046 *
    sin(
      2.0 * D -
      Ms
    );
  dlon +=
    0.041 *
    sin(
      Mm -
      Ms
    );
  dlon +=
    -0.035 *
    sin(D);
  dlon +=
    -0.031 *
    sin(
      Mm +
      Ms
    );
  dlon +=
    -0.015 *
    sin(
      2.0 * F -
      2.0 * D
    );
  dlon +=
    0.011 *
    sin(
      Mm -
      4.0 * D
    );
  moonLon =
    normalizeDegrees(
      moonLon *
      MOON_RAD_TO_DEG +
      dlon
    );
  // ===================================================
  // ЭЛОНГАЦИЯ ЛУНЫ ОТ СОЛНЦА
  //
  // 0°   = NEW
  // 90°  = FIRST QUARTER
  // 180° = FULL
  // 270° = LAST QUARTER
  // ===================================================
  double elongation =
    normalizeDegrees(
      moonLon -
      sunLon
    );
  float phase =
    (float)(
      elongation /
      360.0
    );
  // ===================================================
  // ОСВЕЩЁННОСТЬ
  // ===================================================
  float illumination =
    (float)(
      (1.0 -
       cos(
         elongation *
         MOON_DEG_TO_RAD
       )
      ) *
      50.0
    );
  if (illumination < 0.0) {
    illumination = 0.0;
  }
  if (illumination > 100.0) {
    illumination = 100.0;
  }
  // ===================================================
  // НАЗВАНИЕ ФАЗЫ
  // ===================================================
  const char* name;
  if (
    phase < 0.0625 ||
    phase >= 0.9375
  ) {
    name = "NEW";
  }
  else if (phase < 0.1875) {
    name = "CRES";
  }
  else if (phase < 0.3125) {
    name = "1/4";
  }
  else if (phase < 0.4375) {
    name = "GIB";
  }
  else if (phase < 0.5625) {
    name = "FULL";
  }
  else if (phase < 0.6875) {
    name = "GIB";
  }
  else if (phase < 0.8125) {
    name = "3/4";
  }
  else {
    name = "CRES";
  }
  MoonInfo result;
  result.phase = phase;
  result.illumination = illumination;
  result.name = name;
  return result;
}
// =====================================================
// РИСОВАНИЕ ЛУНЫ
// Реальный эллиптический терминатор.
// =====================================================
void drawMoon(
  int cx,
  int cy,
  int r,
  float phase
) {
  // ===================================================
  // Рамка тёмного диска
  // ===================================================
  display.fillCircle(
    cx,
    cy,
    r,
    SSD1306_BLACK
  );
  display.drawCircle(
    cx,
    cy,
    r,
    SSD1306_WHITE
  );
  // ===================================================
  // Новолуние
  // ===================================================
  if (
    phase < 0.01 ||
    phase > 0.99
  ) {
    return;
  }
  // ===================================================
  // Полнолуние
  // ===================================================
  if (
    phase > 0.49 &&
    phase < 0.51
  ) {
    display.fillCircle(
      cx,
      cy,
      r,
      SSD1306_WHITE
    );
    display.drawCircle(
      cx,
      cy,
      r,
      SSD1306_WHITE
    );
    return;
  }
  // ===================================================
  // Рисуем освещённую часть
  // ===================================================
  double terminator =
    cos(
      2.0 *
      PI *
      phase
    );
  for (
    int y = -r;
    y <= r;
    y++
  ) {
    double dy =
      (double)y;
    double inside =
      (double)r *
      (double)r -
      dy * dy;
    if (inside < 0.0) {
      continue;
    }
    int halfWidth =
      (int)sqrt(inside);
    int xLeft;
    int xRight;
    int terminatorX =
      (int)(
        terminator *
        halfWidth
      );
    if (phase < 0.5) {
      // Растущая:
      // освещена правая сторона
      xLeft = terminatorX;
      xRight = halfWidth;
    }
    else {
      // Убывающая:
      // освещена левая сторона
      xLeft = -halfWidth;
      xRight = -terminatorX;
    }
    if (xRight >= xLeft) {
      display.drawFastHLine(
        cx + xLeft,
        cy + y,
        xRight - xLeft + 1,
        SSD1306_WHITE
      );
    }
  }
  // ===================================================
  // Контур диска
  // ===================================================
  display.drawCircle(
    cx,
    cy,
    r,
    SSD1306_WHITE
  );
}
// =====================================================
// СТРЕЛКА / ТОЧКА ФАЗЫ ЛУНЫ
// =====================================================
void drawMoonIndicator(
  int x,
  int y,
  float phase
) {
  // ===================================================
  // НОВОЛУНИЕ
  // ===================================================
  if (
    phase < 0.0625 ||
    phase >= 0.9375
  ) {
    display.drawCircle(
      x + 3,
      y + 3,
      3,
      SSD1306_WHITE
    );
    return;
  }
  // ===================================================
  // ПОЛНОЛУНИЕ
  // ===================================================
  if (
    phase >= 0.4375 &&
    phase < 0.5625
  ) {
    display.fillCircle(
      x + 3,
      y + 3,
      3,
      SSD1306_WHITE
    );
    return;
  }
  // ===================================================
  // РАСТУЩАЯ
  // ===================================================
  if (phase < 0.5) {
    display.drawLine(
      x + 3,
      y + 7,
      x + 3,
      y,
      SSD1306_WHITE
    );
    display.drawLine(
      x + 3,
      y,
      x,
      y + 3,
      SSD1306_WHITE
    );
    display.drawLine(
      x + 3,
      y,
      x + 6,
      y + 3,
      SSD1306_WHITE
    );
    return;
  }
  // ===================================================
  // УБЫВАЮЩАЯ
  // ===================================================
  display.drawLine(
    x + 3,
    y,
    x + 3,
    y + 7,
    SSD1306_WHITE
  );
  display.drawLine(
    x + 3,
    y + 7,
    x,
    y + 4,
    SSD1306_WHITE
  );
  display.drawLine(
    x + 3,
    y + 7,
    x + 6,
    y + 4,
    SSD1306_WHITE
  );
}
// =====================================================
// АНИМИРОВАННАЯ ПУНКТИРНАЯ ЛИНИЯ
// =====================================================
void drawAnimatedLine() {
  for (
    int x = 0;
    x < SCREEN_WIDTH;
    x += 4
  ) {
    display.drawFastHLine(
      x,
      10,
      2,
      SSD1306_WHITE
    );
  }
  if (lineProgress > 0) {
    display.drawFastHLine(
      0,
      10,
      lineProgress,
      SSD1306_WHITE
    );
  }
}
// =====================================================
// СЕТКА ДЛЯ ВОЛНЫ
// =====================================================
void drawWaveGrid() {
  for (
    int y = 1;
    y < 16;
    y += 5
  ) {
    for (
      int x = 0;
      x < SCREEN_WIDTH;
      x += 6
    ) {
      display.drawFastHLine(
        x,
        y,
        3,
        SSD1306_WHITE
      );
    }
  }
  for (
    int x = 8;
    x < SCREEN_WIDTH;
    x += 16
  ) {
    for (
      int y = 0;
      y < 16;
      y += 4
    ) {
      display.drawFastVLine(
        x,
        y,
        2,
        SSD1306_WHITE
      );
    }
  }
}
// =====================================================
// АНИМИРОВАННАЯ ВОЛНА
// =====================================================
void drawAnimatedWave() {
  const float CENTER_Y = 8.0;
  const float AMPLITUDE = 6.0;
  const float FREQUENCY = 0.19635;
  drawWaveGrid();
  int previousY =
    (int)(
      CENTER_Y +
      AMPLITUDE *
      sin(wavePhase)
    );
  for (
    int x = 1;
    x < SCREEN_WIDTH;
    x++
  ) {
    int y =
      (int)(
        CENTER_Y +
        AMPLITUDE *
        sin(
          wavePhase +
          x * FREQUENCY
        )
      );
    display.drawLine(
      x - 1,
      previousY,
      x,
      y,
      SSD1306_WHITE
    );
    previousY = y;
  }
}
// =====================================================
// ОБНОВЛЕНИЕ ИСТОРИИ ДАВЛЕНИЯ
// =====================================================
void updatePressureHistory(
  float pressure
) {
  if (isnan(pressure)) {
    return;
  }
  unsigned long now =
    millis();
  // ===================================================
  // ПЕРВАЯ ТОЧКА
  //
  // Сразу после запуска:
  // P 1/12h
  // ===================================================
  if (!pressureHistoryStarted) {
    pressureHistory[0] =
      pressure;
    pressureHistoryCount = 1;
    pressureHistoryStarted = true;
    lastPressureSample = now;
    Serial.printf(
      "Pressure graph started: %.1f hPa\n",
      pressure
    );
    return;
  }
  // ===================================================
  // НОВАЯ ТОЧКА КАЖДЫЙ ЧАС
  // ===================================================
  if (
    now - lastPressureSample >=
    PRESSURE_SAMPLE_INTERVAL
  ) {
    // =================================================
    // Увеличиваем ТОЛЬКО счётчик отображения часов.
    // Сам график работает по старой логике.
    // =================================================
    pressureHourCounter++;
    if (
      pressureHistoryCount <
      PRESSURE_HISTORY_SIZE
    ) {
      pressureHistory[
        pressureHistoryCount
      ] = pressure;
      pressureHistoryCount++;
    }
    else {
      // =================================================
      // После 12 часов двигаем окно.
      // =================================================
      for (
        int i = 0;
        i < PRESSURE_HISTORY_SIZE - 1;
        i++
      ) {
        pressureHistory[i] =
          pressureHistory[i + 1];
      }
      pressureHistory[
        PRESSURE_HISTORY_SIZE - 1
      ] = pressure;
    }
    lastPressureSample = now;
    Serial.printf(
      "Pressure graph sample %d/%d: %.1f hPa\n",
      pressureHistoryCount,
      PRESSURE_HISTORY_SIZE,
      pressure
    );
  }
}
// =====================================================
// ОПРЕДЕЛЕНИЕ ТРЕНДА ДАВЛЕНИЯ
// =====================================================
char getPressureTrend() {
  // ===================================================
  // Пока есть только одна точка,
  // направление определить нельзя.
  // ===================================================
  if (pressureHistoryCount < 2) {
    return '-';
  }
  float first =
    pressureHistory[0];
  float last =
    pressureHistory[
      pressureHistoryCount - 1
    ];
  float difference =
    last - first;
  if (
    difference >
    PRESSURE_TREND_THRESHOLD
  ) {
    return '^';
  }
  if (
    difference <
    -PRESSURE_TREND_THRESHOLD
  ) {
    return 'v';
  }
  return '-';
}
// =====================================================
// ГРАФИЧЕСКАЯ СТРЕЛКА ТРЕНДА ДАВЛЕНИЯ
//
// Используется та же графика,
// что и у стрелки Луны.
// =====================================================
void drawPressureTrendIndicator(
  int x,
  int y,
  char trend
) {
  // ===================================================
  // ВВЕРХ
  // ===================================================
  if (trend == '^') {
    display.drawLine(
      x + 3,
      y + 7,
      x + 3,
      y,
      SSD1306_WHITE
    );
    display.drawLine(
      x + 3,
      y,
      x,
      y + 3,
      SSD1306_WHITE
    );
    display.drawLine(
      x + 3,
      y,
      x + 6,
      y + 3,
      SSD1306_WHITE
    );
  }
  // ===================================================
  // ВНИЗ
  // ===================================================
  else if (trend == 'v') {
    display.drawLine(
      x + 3,
      y,
      x + 3,
      y + 7,
      SSD1306_WHITE
    );
    display.drawLine(
      x + 3,
      y + 7,
      x,
      y + 4,
      SSD1306_WHITE
    );
    display.drawLine(
      x + 3,
      y + 7,
      x + 6,
      y + 4,
      SSD1306_WHITE
    );
  }
  // ===================================================
  // БЕЗ ИЗМЕНЕНИЯ
  // ===================================================
  else {
    display.drawFastHLine(
      x,
      y + 3,
      7,
      SSD1306_WHITE
    );
  }
}
// =====================================================
// ГРАФИК ДАВЛЕНИЯ ЗА 12 ЧАСОВ
// =====================================================
void drawPressureGraph() {
  const int GRAPH_LEFT = 66;
  const int GRAPH_RIGHT = 127;
  const int GRAPH_TOP = 48;
  const int GRAPH_BOTTOM = 63;
  const int GRAPH_WIDTH =
    GRAPH_RIGHT - GRAPH_LEFT;
  const int GRAPH_HEIGHT =
    GRAPH_BOTTOM - GRAPH_TOP;
  // ===================================================
  // ЗАГОЛОВОК
  // ===================================================
  display.setTextSize(1);
  display.setCursor(
    67,
    39
  );
  // ===================================================
  // ОТОБРАЖЕНИЕ ТЕКУЩЕГО ЧАСА 12-ЧАСОВОГО ЦИКЛА
  //
  // Сразу после запуска:
  // 1/12h
  //
  // Затем:
  // 2/12h ... 12/12h
  //
  // После 12 часов снова:
  // 1/12h
  //
  // Сам массив истории при этом НЕ изменяется.
  // ===================================================
  int elapsedHours =
    ((pressureHourCounter - 1) % 12) + 1;
  display.print("P ");
  display.print(elapsedHours);
  display.print("/12h");
  // ===================================================
  // ГРАФИЧЕСКАЯ СТРЕЛКА
  // ===================================================
  char trend =
    getPressureTrend();
  drawPressureTrendIndicator(
    118,
    38,
    trend
  );
  // ===================================================
  // НЕТ ДАННЫХ
  // ===================================================
  if (pressureHistoryCount <= 0) {
    return;
  }
  // ===================================================
  // MIN / MAX
  // ===================================================
  float minPressure =
    pressureHistory[0];
  float maxPressure =
    pressureHistory[0];
  for (
    int i = 1;
    i < pressureHistoryCount;
    i++
  ) {
    if (
      pressureHistory[i] <
      minPressure
    ) {
      minPressure =
        pressureHistory[i];
    }
    if (
      pressureHistory[i] >
      maxPressure
    ) {
      maxPressure =
        pressureHistory[i];
    }
  }
  // ===================================================
  // НЕ ДАЁМ ГРАФИКУ СТАТЬ ПЛОСКИМ
  // ===================================================
  if (
    maxPressure - minPressure <
    1.0
  ) {
    float center =
      (
        maxPressure +
        minPressure
      ) * 0.5;
    minPressure =
      center - 0.5;
    maxPressure =
      center + 0.5;
  }
  else {
    float padding =
      (
        maxPressure -
        minPressure
      ) * 0.15;
    minPressure -= padding;
    maxPressure += padding;
  }
  // ===================================================
  // ФУНКЦИЯ Y
  // ===================================================
  auto pressureToY =
    [&](float p) {
      int y =
        GRAPH_BOTTOM -
        (int)(
          (
            (p - minPressure) /
            (maxPressure - minPressure)
          ) *
          GRAPH_HEIGHT
        );
      if (y < GRAPH_TOP) {
        y = GRAPH_TOP;
      }
      if (y > GRAPH_BOTTOM) {
        y = GRAPH_BOTTOM;
      }
      return y;
    };
  // ===================================================
  // ПЕРВАЯ ТОЧКА НЕМНОГО ПРАВЕЕ
  // ВЕРТИКАЛЬНОЙ ЛИНИИ
  // ===================================================
  const int FIRST_POINT_OFFSET = 2;
  const float X_STEP =
    (float)(
      GRAPH_WIDTH -
      FIRST_POINT_OFFSET
    ) /
    (PRESSURE_HISTORY_SIZE - 1);
  // ===================================================
  // ПЕРВАЯ ТОЧКА
  // ===================================================
  int previousX =
    GRAPH_LEFT +
    FIRST_POINT_OFFSET;
  int previousY =
    pressureToY(
      pressureHistory[0]
    );
  // ===================================================
  // ОДНА ТОЧКА
  // ===================================================
  if (
    pressureHistoryCount == 1
  ) {
    if (pressureBlinkState) {
      display.fillRect(
        previousX - 1,
        previousY - 1,
        3,
        3,
        SSD1306_WHITE
      );
    }
    return;
  }
  // ===================================================
  // СОЕДИНЯЕМ ТОЧКИ
  // ===================================================
  for (
    int i = 1;
    i < pressureHistoryCount;
    i++
  ) {
    int x =
      GRAPH_LEFT +
      FIRST_POINT_OFFSET +
      (int)(
        i * X_STEP
      );
    int y =
      pressureToY(
        pressureHistory[i]
      );
    display.drawLine(
      previousX,
      previousY,
      x,
      y,
      SSD1306_WHITE
    );
    previousX = x;
    previousY = y;
  }
  // ===================================================
  // МИГАЮЩАЯ ПОСЛЕДНЯЯ ТОЧКА
  // ===================================================
  if (pressureBlinkState) {
    display.fillRect(
      previousX - 1,
      previousY - 1,
      3,
      3,
      SSD1306_WHITE
    );
  }
}
// =====================================================
// ОБНОВЛЕНИЕ ДИСПЛЕЯ
// =====================================================
void updateDisplay() {
  // ===================================================
  // СЧИТЫВАНИЕ ДАТЧИКОВ
  // ===================================================
  float temperature = NAN;
  float humidity = NAN;
  float pressure = NAN;
  if (ahtOk) {
    sensors_event_t humEvent;
    sensors_event_t tempEvent;
    aht.getEvent(
      &humEvent,
      &tempEvent
    );
    temperature =
      tempEvent.temperature;
    humidity =
      humEvent.relative_humidity;
  }
  if (bmpOk) {
    pressure =
      bmp.readPressure() /
      100.0F;
    if (!ahtOk) {
      temperature =
        bmp.readTemperature();
    }
  }
  // ===================================================
  // КАЛИБРОВОЧНЫЕ ПОПРАВКИ
  // ===================================================
  if (!isnan(temperature)) {
    temperature +=
      temperatureOffset;
  }
  if (!isnan(humidity)) {
    humidity +=
      humidityOffset;
  }
  if (!isnan(pressure)) {
    pressure +=
      pressureOffset;
  }
  // ===================================================
  // ГРАФИК ДАВЛЕНИЯ
  // ===================================================
  updatePressureHistory(
    pressure
  );
  // ===================================================
  // WIFI
  // ===================================================
  bool currentWifi =
    WiFi.status() ==
    WL_CONNECTED;
  wifiConnected =
    currentWifi;
  // ===================================================
  // ВРЕМЯ
  // ===================================================
  char dateStr[9] = "";
  char dayStr[4] = "";
  char timeStr[6] = "";
  struct tm timeinfo;
  bool timeValid = false;
  // ===================================================
  // Если NTP уже подтверждён,
  // getLocalTime() выполняется без ожидания.
  // ===================================================
  if (ntpTimeValid) {
    timeValid =
      getLocalTime(
        &timeinfo,
        0
      );
  }
  else if (currentWifi) {
    timeValid =
      getLocalTime(
        &timeinfo,
        0
      );
    if (timeValid) {
      ntpTimeValid = true;
    }
  }
  // ===================================================
  // ФОРМАТИРОВАНИЕ ДАТЫ / ВРЕМЕНИ
  // ===================================================
  if (timeValid) {
    strftime(
      dateStr,
      sizeof(dateStr),
      "%d-%m-%y",
      &timeinfo
    );
    strftime(
      dayStr,
      sizeof(dayStr),
      "%a",
      &timeinfo
    );
    strftime(
      timeStr,
      sizeof(timeStr),
      "%H:%M",
      &timeinfo
    );
  }
  // ===================================================
  // ФАЗА ЛУНЫ
  // ===================================================
  time_t now =
    time(nullptr);
  MoonInfo moon =
    getMoonPhase(now);
  // ===================================================
  // SERIAL
  // ===================================================
  Serial.printf(
    "%s %s %s  T=%.1fC H=%.1f%% P=%.1fhPa Moon=%s %.0f%% WiFi=%s NTP=%s Graph=%d/12 Trend=%c\n",
    dateStr,
    dayStr,
    timeStr,
    temperature,
    humidity,
    pressure,
    moon.name,
    moon.illumination,
    currentWifi ? "OK" : "OFF",
    ntpTimeValid ? "OK" : "WAIT",
    pressureHistoryCount,
    getPressureTrend()
  );
  // ===================================================
  // ОЧИСТКА ЭКРАНА
  // ===================================================
  display.clearDisplay();
  display.setTextColor(
    SSD1306_WHITE
  );
  display.setTextSize(1);
  // ===================================================
  // ВЕРХНИЕ 16 ПИКСЕЛЕЙ
  // ===================================================
  if (
    currentWifi &&
    timeValid
  ) {
    display.setCursor(0, 0);
    display.print(dateStr);
    display.setCursor(55, 0);
    display.print(dayStr);
    display.setCursor(98, 0);
    display.print(timeStr);
    drawAnimatedLine();
  }
  else if (
    currentWifi &&
    !timeValid
  ) {
    // =================================================
    // NTP не получен даже после загрузочного таймаута.
    //
    // Здесь больше НЕТ "Get timezone".
    //
    // Если NTP появится позже, следующий updateDisplay()
    // автоматически покажет настоящее время.
    // =================================================
    display.setCursor(0, 0);
    display.print("Waiting NTP");
    int dots =
      (millis() / 400) % 4;
    for (
      int i = 0;
      i < dots;
      i++
    ) {
      display.print(".");
    }
    drawAnimatedLine();
  }
  else if (
    !currentWifi &&
    wifiAttemptFinished &&
    !ntpTimeValid
  ) {
    drawAnimatedWave();
  }
  // ===================================================
  // ОСНОВНАЯ ОБЛАСТЬ
  // ===================================================
  display.drawFastVLine(
    64,
    16,
    48,
    SSD1306_WHITE
  );
  display.drawFastHLine(
    0,
    37,
    64,
    SSD1306_WHITE
  );
  display.drawFastHLine(
    65,
    37,
    63,
    SSD1306_WHITE
  );
  // ===================================================
  // TEMPERATURE
  // ===================================================
  display.setTextSize(1);
  display.setCursor(4, 17);
  display.print("Temp:");
  display.setCursor(2, 28);
  if (!isnan(temperature)) {
    display.printf(
      "%.1fC",
      temperature
    );
  }
  else {
    display.print(
      "--.-C"
    );
  }
  // ===================================================
  // PRESSURE + HUMIDITY
  // ===================================================
  display.setTextSize(1);
  display.setCursor(67, 17);
  display.print("P:");
  if (!isnan(pressure)) {
    display.printf(
      "%.0fhPa",
      pressure
    );
  }
  else {
    display.print(
      "--hPa"
    );
  }
  display.setCursor(67, 28);
  display.print("H:");
  if (!isnan(humidity)) {
    display.printf(
      "%.0f%%",
      humidity
    );
  }
  else {
    display.print(
      "--%"
    );
  }
  // ===================================================
  // MOON
  // ===================================================
  drawMoon(
    10,
    51,
    7,
    moon.phase
  );
  // ===================================================
  // НАЗВАНИЕ ФАЗЫ
  //
  // Пока NTP нет:
  // get
  //
  // После NTP:
  // реальная фаза
  // ===================================================
  display.setCursor(
    22,
    45
  );
  if (!ntpTimeValid) {
    display.print("DATA");
  }
  else {
    display.print(
      moon.name
    );
  }
  // ===================================================
  // ОСВЕЩЁННОСТЬ
  //
  // Пока NTP нет:
  // time
  //
  // После NTP:
  // реальный %
  // ===================================================
  display.setCursor(
    22,
    55
  );
  if (!ntpTimeValid) {
    display.print("N/A");
  }
  else {
    display.printf(
      "%.0f%%",
      moon.illumination
    );
  }
  // ===================================================
  // СТРЕЛКА ЛУНЫ
  //
  // Пока NTP нет — НЕ РИСУЕМ.
  // После NTP — реальная стрелка.
  // ===================================================
  if (ntpTimeValid) {
    drawMoonIndicator(
      47,
      53,
      moon.phase
    );
  }
  // ===================================================
  // ГРАФИК ДАВЛЕНИЯ
  // ===================================================
  drawPressureGraph();
  // ===================================================
  // АНИМАЦИЯ ЛИНИИ
  // ===================================================
  lineProgress += 4;
  if (
    lineProgress >= SCREEN_WIDTH
  ) {
    lineProgress = 0;
  }
  // ===================================================
  // ВЫВОД НА OLED
  // ===================================================
  display.display();
}
