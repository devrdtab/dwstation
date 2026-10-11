#pragma once
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>

// Дисплей SH1106 (1.3", 128x64, I2C).
// Старые имена цветов оставлены, чтобы не править остальные файлы проекта.
#ifndef SSD1306_WHITE
#define SSD1306_WHITE SH110X_WHITE
#endif
#ifndef SSD1306_BLACK
#define SSD1306_BLACK SH110X_BLACK
#endif

extern Adafruit_SH1106G display;

void updateDisplay();