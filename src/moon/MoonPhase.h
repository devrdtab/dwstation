#pragma once
#include <Arduino.h>

struct MoonInfo {
  float phase;
  float illumination;
  const char* name;
};

double normalizeDegrees(double value);
MoonInfo getMoonPhase(time_t now);
void drawMoon(int cx, int cy, int r, float phase);
void drawMoonIndicator(int x, int y, float phase);