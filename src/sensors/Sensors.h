#pragma once
#include <Arduino.h>

void sensorsInit();
void sensorsRead(float &temperature, float &humidity, float &pressure);