#pragma once
#include <Arduino.h>

void batteryInit();
void batteryUpdate();   // вызывать в loop(), частоту ограничивает сама
int batteryPercent();   // 0-100
float batteryVoltage(); // напряжение самой батареи, В — для отладки в Serial