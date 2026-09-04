#pragma once
#include <Arduino.h>

// I2C ПИНЫ ESP32-S3-DevKitC-1
#define I2C_SDA 8
#define I2C_SCL 9

// OLED
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDR 0x3C

// ОБНОВЛЕНИЕ ЭКРАНА
const unsigned long UPDATE_INTERVAL = 1000;

// КАЛИБРОВКА
const float REFERENCE_TEMPERATURE = 25.0;
const float REFERENCE_HUMIDITY = 48.0;
const float REFERENCE_PRESSURE = 1016.0;

const bool RESET_CALIBRATION = false;

const int CALIBRATION_SAMPLES = 10;
const unsigned long CALIBRATION_INTERVAL = 500;
const unsigned long CALIBRATION_WARMUP = 3000;