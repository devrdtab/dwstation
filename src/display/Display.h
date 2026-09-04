#pragma once
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

extern Adafruit_SSD1306 display;

void updateDisplay();
void tickWaveAnimation(); // вызывать в loop(), когда нет WiFi/NTP