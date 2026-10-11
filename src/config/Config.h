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

// ОБНОВЛЕНИЕ ЭКРАНА
const unsigned long UPDATE_INTERVAL = 1000;

// ПЕРЕКЛЮЧЕНИЕ СТРАНИЦ (показания / качество воздуха)
const unsigned long SCREEN_PAGE_INTERVAL = 5000;

// ПОВТОРНЫЙ ПОИСК ДАТЧИКОВ, ПОКА КАКОЙ-ТО НЕ НАЙДЕН
const unsigned long SENSOR_RETRY_INTERVAL = 10000;

// КАЛИБРОВКА
const float REFERENCE_TEMPERATURE = 25.0;
const float REFERENCE_HUMIDITY = 48.0;
const float REFERENCE_PRESSURE = 1016.0;

const bool RESET_CALIBRATION = false;

const int CALIBRATION_SAMPLES = 10;
const unsigned long CALIBRATION_INTERVAL = 500;
const unsigned long CALIBRATION_WARMUP = 3000;