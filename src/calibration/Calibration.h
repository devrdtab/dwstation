#pragma once
#include <Arduino.h>

extern float temperatureOffset;
extern float humidityOffset;
extern float pressureOffset;

void loadCalibration();
void saveCalibration();
void showCalibrationProgress(
    int sample, int total); // не используется, оставлено как в оригинале
void performCalibration();  // совместимость, как в оригинале