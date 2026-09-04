#pragma once
#include <Arduino.h>

const int PRESSURE_HISTORY_SIZE = 13;
const unsigned long PRESSURE_SAMPLE_INTERVAL = 3600000UL;
const float PRESSURE_TREND_THRESHOLD = 0.5;

void updatePressureHistory(float pressure);
char getPressureTrend();
int getPressureHistoryCount();
void drawPressureTrendIndicator(int x, int y, char trend);
void drawPressureGraph();
void tickPressureBlink(); // вызывать каждый loop()