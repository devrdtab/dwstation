#include <Arduino.h>
#include <WiFi.h>
#include <time.h>
#include <Wire.h>
#include <math.h>
#include <Adafruit_AHTX0.h>
#include <Adafruit_BMP280.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// =====================================================
// WIFI
// =====================================================

const char* WIFI_SSID = "YOUR_WIFI_SSID";
const char* WIFI_PASS = "YOUR_WIFI_PASSWORD";

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

Adafruit_SSD1306 display(
  SCREEN_WIDTH,
  SCREEN_HEIGHT,
  &Wire,
  -1
);

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

// Длина заполненной части анимированной линии
int lineProgress = 0;


// =====================================================
// СТРУКТУРА ФАЗЫ ЛУНЫ
// =====================================================

struct MoonInfo {
  float phase;
  float illumination;
  const char* name;
};


// =====================================================
// ПРОТОТИПЫ
// =====================================================

void bootAnimation();
void updateDisplay();
MoonInfo getMoonPhase(time_t now);
void drawMoon(int cx, int cy, int r, float phase);
void drawMoonIndicator(int x, int y, float phase);
void drawAnimatedLine();


// =====================================================
// SETUP
// =====================================================

void setup() {
  Serial.begin(115200);
  delay(200);

  // ===================================================
  // I2C
  // ===================================================

  Wire.begin(
    I2C_SDA,
    I2C_SCL
  );

  // ===================================================
  // OLED
  // ===================================================

  if (!display.begin(
        SSD1306_SWITCHCAPVCC,
        OLED_ADDR
      )) {

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
  // СТАРТОВАЯ АНИМАЦИЯ
  // ===================================================

  bootAnimation();

  // ===================================================
  // NTP
  // ===================================================

  configTime(
    GMT_OFFSET_SEC,
    DAYLIGHT_OFFSET_SEC,
    NTP_SERVER
  );

  // ===================================================
  // AHT20
  // ===================================================

  ahtOk = aht.begin();

  if (!ahtOk) {
    Serial.println("AHT20 не найден!");
  } else {
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

  // ===================================================
  // ПЕРВЫЙ ВЫВОД
  // ===================================================

  updateDisplay();

  lastUpdate = millis();
}


// =====================================================
// LOOP
// =====================================================

void loop() {

  if (
    millis() - lastUpdate >=
    UPDATE_INTERVAL
  ) {

    lastUpdate = millis();

    updateDisplay();
  }
}


// =====================================================
// СТАРТОВАЯ АНИМАЦИЯ
// =====================================================

void bootAnimation() {

  const int totalSteps = 100;

  // Скорость обычных этапов
  const int animationDelay = 20;

  // Максимальное время подключения WiFi
  const unsigned long WIFI_TIMEOUT = 10000;

  bool wifiStarted = false;
  bool wifiFinished = false;

  unsigned long wifiStartTime = 0;

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  // ===================================================
  // АНИМАЦИЯ 0-100%
  // ===================================================

  for (
    int percent = 0;
    percent <= totalSteps;
    percent++
  ) {

    // =================================================
    // ЗАПУСК WIFI НА 50%
    // =================================================

    if (
      percent >= 50 &&
      !wifiStarted
    ) {

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

    // =================================================
    // ПРОВЕРКА WIFI
    // =================================================

    if (
      wifiStarted &&
      !wifiFinished
    ) {

      if (
        WiFi.status() ==
        WL_CONNECTED
      ) {

        wifiFinished = true;

        Serial.println(
          "WiFi подключен: " +
          WiFi.localIP().toString()
        );

      } else if (
        millis() - wifiStartTime >=
        WIFI_TIMEOUT
      ) {

        wifiFinished = true;

        Serial.println(
          "WiFi: таймаут 10 секунд"
        );
      }
    }

    // =================================================
    // ШИРИНА ПРОГРЕСС-БАРА
    // =================================================

    int barWidth = map(
      percent,
      0,
      totalSteps,
      0,
      SCREEN_WIDTH - 8
    );

    // =================================================
    // ОЧИСТКА
    // =================================================

    display.clearDisplay();

    // =================================================
    // ЗАГОЛОВОК
    // =================================================

    display.setCursor(
      4,
      2
    );

// display.setTextSize(1);
// display.setCursor(19, 2);
// display.print("DESKTOP WEATHER");

display.setTextSize(1);

const char* title = "DESKTOP WEATHER";

int16_t x1, y1;
uint16_t w, h;

display.getTextBounds(
  title,
  0,
  2,
  &x1,
  &y1,
  &w,
  &h
);

int16_t x = (SCREEN_WIDTH - w) / 2;

display.setCursor(
  x,
  2
);

display.print(title);


    // =================================================
    // ПРОЦЕНТ
    // =================================================

    display.setCursor(
      0,
      24
    );

    display.print(
      "Loading:"
    );

    if (percent < 10) {
      display.print("  ");
    } else if (percent < 100) {
      display.print(" ");
    }

    display.print(
      percent
    );

    display.print(
      "%"
    );

    // =================================================
    // РАМКА ПРОГРЕСС-БАРА
    // =================================================

    display.drawRect(
      4,
      36,
      SCREEN_WIDTH - 8,
      10,
      SSD1306_WHITE
    );

    // =================================================
    // ЗАПОЛНЕНИЕ ПРОГРЕСС-БАРА
    // =================================================

    if (barWidth > 0) {

      display.fillRect(
        5,
        37,
        barWidth - 1,
        8,
        SSD1306_WHITE
      );
    }

    // =================================================
    // БЕГУЩАЯ ТОЧКА
    // =================================================

    int dotX =
      4 +
      (percent * (SCREEN_WIDTH - 8)) /
      100;

    if (dotX > 123) {
      dotX = 123;
    }

    display.fillCircle(
      dotX,
      52,
      0.5,
      SSD1306_WHITE
    );

    // =================================================
    // ТЕКСТ ЭТАПА
    // =================================================

    display.setCursor(
      0,
      57
    );

    if (percent < 25) {

      display.print(
        "Starting.."
      );

    } else if (percent < 50) {

      display.print(
        "Get sensors..."
      );

    } else if (percent < 70) {

      display.print(
        "Connecting WiFi"
      );

      if (
        WiFi.status() ==
        WL_CONNECTED
      ) {

        display.print(
          " OK"
        );

      } else {

        int dots =
          (millis() / 300) % 4;

        for (
          int i = 0;
          i < dots;
          i++
        ) {
          display.print(".");
        }
      }

    } else if (percent < 90) {


      display.print(
        "Initializing...."
      );

    } else if (percent < 100) {

      display.print(
        "Almost done......"
      );

    } else {

      display.print(
        "READY"
      );
    }

    // =================================================
    // WIFI СТАТУС
    // =================================================


    display.display();

    // =================================================
    // ЕСЛИ WIFI ПОДКЛЮЧАЕТСЯ
    // =================================================

    if (
      percent >= 50 &&
      percent < 70 &&
      !wifiFinished
    ) {

      // Пока WiFi не подключен,
      // остаёмся на текущем проценте.

      percent--;

      delay(100);

      continue;
    }

    // =================================================
    // ОБЫЧНАЯ СКОРОСТЬ АНИМАЦИИ
    // =================================================

    delay(
      animationDelay
    );
  }

  // ===================================================
  // READY
  // ===================================================

  delay(500);
}


// =====================================================
// РАСЧЁТ ФАЗЫ ЛУНЫ
// =====================================================

MoonInfo getMoonPhase(
  time_t now
) {

  const double SYNODIC_MONTH =
    29.530588853;

  const double KNOWN_NEW_MOON =
    2451550.1;

  double jd =
    ((double)now / 86400.0) +
    2440587.5;

  double days =
    jd - KNOWN_NEW_MOON;

  double moonAge =
    fmod(
      days,
      SYNODIC_MONTH
    );

  if (moonAge < 0) {
    moonAge += SYNODIC_MONTH;
  }

  float phase =
    moonAge /
    SYNODIC_MONTH;

  // ===================================================
  // ОСВЕЩЁННОСТЬ
  // ===================================================

  float illumination =
    (1.0 -
     cos(
       2.0 *
       PI *
       phase
     )) *
    50.0;

  const char* name;

  // ===================================================
  // НАЗВАНИЕ ФАЗЫ
  // ===================================================

  if (
    phase < 0.0625 ||
    phase >= 0.9375
  ) {

    name = "NEW";

  } else if (
    phase < 0.1875
  ) {

    name = "CRES";

  } else if (
    phase < 0.3125
  ) {

    name = "1/4";

  } else if (
    phase < 0.4375
  ) {

    name = "GIB";

  } else if (
    phase < 0.5625
  ) {

    name = "FULL";

  } else if (
    phase < 0.6875
  ) {

    name = "GIB";

  } else if (
    phase < 0.8125
  ) {

    name = "3/4";

  } else {

    name = "CRES";
  }

  MoonInfo result;

  result.phase =
    phase;

  result.illumination =
    illumination;

  result.name =
    name;

  return result;
}


// =====================================================
// РИСОВАНИЕ ЛУНЫ
// =====================================================

void drawMoon(
  int cx,
  int cy,
  int r,
  float phase
) {

  // ===================================================
  // НОВОЛУНИЕ
  // ===================================================

  if (
    phase < 0.03 ||
    phase > 0.97
  ) {

    display.drawCircle(
      cx,
      cy,
      r,
      SSD1306_WHITE
    );

    return;
  }

  // ===================================================
  // ПОЛНОЛУНИЕ
  // ===================================================

  if (
    phase > 0.47 &&
    phase < 0.53
  ) {

    display.fillCircle(
      cx,
      cy,
      r,
      SSD1306_WHITE
    );

    return;
  }

  // ===================================================
  // ПЕРВАЯ ЧЕТВЕРТЬ
  // ===================================================

  if (
    phase >= 0.22 &&
    phase < 0.28
  ) {

    display.fillCircle(
      cx,
      cy,
      r,
      SSD1306_WHITE
    );

    display.fillRect(
      cx - r,
      cy - r,
      r,
      r * 2 + 1,
      SSD1306_BLACK
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
  // ПОСЛЕДНЯЯ ЧЕТВЕРТЬ
  // ===================================================

  if (
    phase >= 0.72 &&
    phase < 0.78
  ) {

    display.fillCircle(
      cx,
      cy,
      r,
      SSD1306_WHITE
    );

    display.fillRect(
      cx,
      cy - r,
      r + 1,
      r * 2 + 1,
      SSD1306_BLACK
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
  // РАСТУЩАЯ ЛУНА
  // ===================================================

  if (
    phase < 0.5
  ) {

    display.fillCircle(
      cx,
      cy,
      r,
      SSD1306_WHITE
    );

    display.fillCircle(
      cx - 5,
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

    return;
  }

  // ===================================================
  // УБЫВАЮЩАЯ ЛУНА
  // ===================================================

  display.fillCircle(
    cx,
    cy,
    r,
    SSD1306_WHITE
  );

  display.fillCircle(
    cx + 5,
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
  // НОВОЛУНИЕ — ПУСТАЯ ТОЧКА
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
  // ПОЛНОЛУНИЕ — ЖИРНАЯ ТОЧКА
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
  // РАСТУЩАЯ — СТРЕЛКА ВВЕРХ
  // ===================================================

  if (
    phase < 0.5
  ) {

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
  // УБЫВАЮЩАЯ — СТРЕЛКА ВНИЗ
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

  // ===================================================
  // ПУНКТИР ПО ВСЕЙ ШИРИНЕ
  // ===================================================

  // Формат:
  //
  // ██  ██  ██  ██  ██  ██
  //
  // 2 пикселя линия
  // 2 пикселя пробел

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

  // ===================================================
  // ЗАПОЛНЕННАЯ ЧАСТЬ
  // ===================================================

  if (
    lineProgress > 0
  ) {

    display.drawFastHLine(
      0,
      10,
      lineProgress,
      SSD1306_WHITE
    );
  }
}


// =====================================================
// ОБНОВЛЕНИЕ ДИСПЛЕЯ
// =====================================================

void updateDisplay() {

  // ===================================================
  // ПОКАЗАНИЯ ДАТЧИКОВ
  // ===================================================

  float temperature = NAN;
  float humidity = NAN;
  float pressure = NAN;

  // ===================================================
  // AHT20
  // ===================================================

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

  // ===================================================
  // BMP280
  // ===================================================

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
  // ДАТА / ДЕНЬ / ВРЕМЯ
  // ===================================================

  char dateStr[9] =
    "-- -- --";

  char dayStr[4] =
    "---";

  char timeStr[6] =
    "--:--";

  struct tm timeinfo;

  if (
    getLocalTime(
      &timeinfo,
      1000
    )
  ) {

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
    getMoonPhase(
      now
    );

  // ===================================================
  // SERIAL MONITOR
  // ===================================================

  Serial.printf(
    "%s %s %s  T=%.1fC H=%.1f%% P=%.1fhPa Moon=%s %.0f%%\n",
    dateStr,
    dayStr,
    timeStr,
    temperature,
    humidity,
    pressure,
    moon.name,
    moon.illumination
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
  // ВЕРХНЯЯ ОБЛАСТЬ
  // ===================================================

  // Дата слева
  display.setCursor(
    0,
    0
  );

  display.print(
    dateStr
  );

  // День недели по центру
  display.setCursor(
    55,
    0
  );

  display.print(
    dayStr
  );

  // Время справа
  display.setCursor(
    98,
    0
  );

  display.print(
    timeStr
  );

  // ===================================================
  // ПУНКТИРНАЯ АНИМИРОВАННАЯ ЛИНИЯ
  // ===================================================

  drawAnimatedLine();

  // ===================================================
  // ВЕРТИКАЛЬНАЯ ЛИНИЯ
  // ===================================================

  display.drawFastVLine(
    64,
    16,
    48,
    SSD1306_WHITE
  );

  // ===================================================
  // ГОРИЗОНТАЛЬНЫЕ ЛИНИИ
  // ===================================================

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
  // ЛЕВАЯ ВЕРХНЯЯ ЗОНА — ТЕМПЕРАТУРА
  // ===================================================

  display.setTextSize(1);

  display.setCursor(
    4,
    17
  );

  display.print(
    "Temp:"
  );

  display.setCursor(
    2,
    28
  );

  if (
    !isnan(temperature)
  ) {

    display.printf(
      "%.1fC",
      temperature
    );

  } else {

    display.print(
      "--.-C"
    );
  }

  // ===================================================
  // ПРАВАЯ ВЕРХНЯЯ ЗОНА — ДАВЛЕНИЕ
  // ===================================================

  display.setCursor(
    68,
    16
  );

  display.print(
    "Pressure:"
  );

  display.setCursor(
    68,
    27
  );

  if (
    !isnan(pressure)
  ) {

    display.printf(
      "%.0f hPa",
      pressure
    );

  } else {

    display.print(
      "-- hPa"
    );
  }

  // ===================================================
  // ЛЕВАЯ НИЖНЯЯ ЗОНА — ФАЗА ЛУНЫ
  // ===================================================

  drawMoon(
    10,
    51,
    7,
    moon.phase
  );

  // Название фазы
  display.setCursor(
    22,
    45
  );

  display.print(
    moon.name
  );

  // Освещённость
  display.setCursor(
    22,
    55
  );

  display.printf(
    "%.0f%%",
    moon.illumination
  );

  // Стрелка / точка
  drawMoonIndicator(
    47,
    53,
    moon.phase
  );

  // ===================================================
  // ПРАВАЯ НИЖНЯЯ ЗОНА — ВЛАЖНОСТЬ
  // ===================================================

  display.setCursor(
    68,
    42
  );

  display.print(
    "Humidity:"
  );

  display.setCursor(
    68,
    53
  );

  if (
    !isnan(humidity)
  ) {

    display.printf(
      "%.0f%%",
      humidity
    );

  } else {

    display.print(
      "--%"
    );
  }

  // ===================================================
  // ОБНОВЛЕНИЕ АНИМИРОВАННОЙ ЛИНИИ
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