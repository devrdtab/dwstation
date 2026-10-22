#pragma once
#include <Arduino.h>

// I2C ПИНЫ ESP32-C3 MINI
#define I2C_SDA 4
#define I2C_SCL 5

// АДРЕСА I2C
#define OLED_ADDR 0x3C
#define BMP280_ADDR_PRIMARY 0x76
#define BMP280_ADDR_SECONDARY 0x77
#define ENS160_ADDR 0x53
// AHT20 (0x38, главная шина) и DS3231 (0x68) — фиксированные адреса своих
// библиотек

// I2C-МУЛЬТИПЛЕКСОР TCA9548A
#define TCA9548A_ADDR 0x70
#define TCA9548A_CHANNEL_ENS160 0 // за этим каналом — плата ENS160+AHT21

// OLED
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

// ОБНОВЛЕНИЕ ДАННЫХ (датчики, лог, перерисовка)
const unsigned long UPDATE_INTERVAL = 1000;

// ===== КНОПКА И ЭКРАН =====
#define BUTTON_PIN 3 // контакт кнопки → GPIO3, второй контакт → GND
#define LED_PIN                                                                \
  6 // светодиод кнопки: «+» через резистор 330 Ом → GPIO6, «−» → GND

const unsigned long SCREEN_ON_TIME_MS =
    10000; // сколько экран остаётся включённым
const unsigned long LONG_PRESS_MS =
    500; // длительное нажатие переключает экраны
const unsigned long BUTTON_DEBOUNCE_MS = 30; // защита от дребезга контактов

const unsigned long LED_BLINK_PERIOD_MS =
    20000; // период мигания светодиода в ожидании
const unsigned long LED_BLINK_ON_MS = 20; // длительность одной вспышки

// ПОВТОРНЫЙ ПОИСК ДАТЧИКОВ, ПОКА КАКОЙ-ТО НЕ НАЙДЕН
const unsigned long SENSOR_RETRY_INTERVAL = 10000;

// КАЛИБРОВКА
const float REFERENCE_TEMPERATURE = 25.0;
const float REFERENCE_HUMIDITY = 70.0;
const float REFERENCE_PRESSURE = 1016.0;

const bool RESET_CALIBRATION =
    false; // true — сбросить калибровку при старте (для тестов)

const int CALIBRATION_SAMPLES = 10;
const unsigned long CALIBRATION_INTERVAL = 500;
const unsigned long CALIBRATION_WARMUP = 3000;
// ЭКОНОМИЯ ПИТАНИЯ: WiFi включается только на время синхронизации времени
const unsigned long WIFI_RESYNC_INTERVAL_MS =
    7UL * 24UL * 3600UL * 1000UL; // раз в неделю
const unsigned long WIFI_SYNC_CONNECT_TIMEOUT_MS = 15000;

// БАТАРЕЯ И ПИТАНИЕ (нужна плата GREATZT + 2 делителя напряжения 100к/100к)
#define BATTERY_VOLTAGE_PIN 1 // ADC: напряжение батареи через делитель
#define BATTERY_CHARGE_SENSE_PIN                                               \
  7 // цифровой: HIGH, если на входе GREATZT есть сеть

const float BATTERY_DIVIDER_RATIO =
    2.0; // делитель 100к/100к занижает напряжение вдвое
const float BATTERY_EMPTY_V = 3.3; // напряжение батареи, которое считаем 0%
const float BATTERY_FULL_V = 4.2;  // напряжение батареи, которое считаем 100%
const unsigned long BATTERY_UPDATE_INTERVAL = 2000; // как часто опрашивать