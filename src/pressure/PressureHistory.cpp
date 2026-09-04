#include "PressureHistory.h"
#include "display/Display.h"

static float pressureHistory[PRESSURE_HISTORY_SIZE];
static int pressureHistoryCount = 0;
static unsigned long lastPressureSample = 0;
static unsigned long pressureHourCounter = 1;
static bool pressureHistoryStarted = false;
static bool pressureBlinkState = true;
static unsigned long lastPressureBlink = 0;

int getPressureHistoryCount() {
  return pressureHistoryCount;
}

void tickPressureBlink() {
  unsigned long now = millis();
  if (now - lastPressureBlink >= 1000) {
    lastPressureBlink = now;
    pressureBlinkState = !pressureBlinkState;
  }
}

void updatePressureHistory(float pressure) {
  if (isnan(pressure)) return;
  unsigned long now = millis();

  if (!pressureHistoryStarted) {
    pressureHistory[0] = pressure;
    pressureHistoryCount = 1;
    pressureHistoryStarted = true;
    lastPressureSample = now;
    Serial.printf("Pressure graph started: %.1f hPa\n", pressure);
    return;
  }

  if (now - lastPressureSample >= PRESSURE_SAMPLE_INTERVAL) {
    pressureHourCounter++;
    if (pressureHistoryCount < PRESSURE_HISTORY_SIZE) {
      pressureHistory[pressureHistoryCount] = pressure;
      pressureHistoryCount++;
    } else {
      for (int i = 0; i < PRESSURE_HISTORY_SIZE - 1; i++) {
        pressureHistory[i] = pressureHistory[i + 1];
      }
      pressureHistory[PRESSURE_HISTORY_SIZE - 1] = pressure;
    }
    lastPressureSample = now;
    Serial.printf("Pressure graph sample %d/%d: %.1f hPa\n", pressureHistoryCount, PRESSURE_HISTORY_SIZE, pressure);
  }
}

char getPressureTrend() {
  if (pressureHistoryCount < 2) return '-';
  float difference = pressureHistory[pressureHistoryCount - 1] - pressureHistory[0];
  if (difference > PRESSURE_TREND_THRESHOLD) return '^';
  if (difference < -PRESSURE_TREND_THRESHOLD) return 'v';
  return '-';
}

void drawPressureTrendIndicator(int x, int y, char trend) {
  if (trend == '^') {
    display.drawLine(x + 3, y + 7, x + 3, y, SSD1306_WHITE);
    display.drawLine(x + 3, y, x, y + 3, SSD1306_WHITE);
    display.drawLine(x + 3, y, x + 6, y + 3, SSD1306_WHITE);
  } else if (trend == 'v') {
    display.drawLine(x + 3, y, x + 3, y + 7, SSD1306_WHITE);
    display.drawLine(x + 3, y + 7, x, y + 4, SSD1306_WHITE);
    display.drawLine(x + 3, y + 7, x + 6, y + 4, SSD1306_WHITE);
  } else {
    display.drawFastHLine(x, y + 3, 7, SSD1306_WHITE);
  }
}

void drawPressureGraph() {
  const int GRAPH_LEFT = 66;
  const int GRAPH_RIGHT = 127;
  const int GRAPH_TOP = 48;
  const int GRAPH_BOTTOM = 63;
  const int GRAPH_WIDTH = GRAPH_RIGHT - GRAPH_LEFT;
  const int GRAPH_HEIGHT = GRAPH_BOTTOM - GRAPH_TOP;

  display.setTextSize(1);
  display.setCursor(67, 39);
  int elapsedHours = ((pressureHourCounter - 1) % 12) + 1;
  display.print("P ");
  display.print(elapsedHours);
  display.print("/12h");

  char trend = getPressureTrend();
  drawPressureTrendIndicator(118, 38, trend);

  if (pressureHistoryCount <= 0) return;

  float minPressure = pressureHistory[0];
  float maxPressure = pressureHistory[0];
  for (int i = 1; i < pressureHistoryCount; i++) {
    if (pressureHistory[i] < minPressure) minPressure = pressureHistory[i];
    if (pressureHistory[i] > maxPressure) maxPressure = pressureHistory[i];
  }

  if (maxPressure - minPressure < 1.0) {
    float center = (maxPressure + minPressure) * 0.5;
    minPressure = center - 0.5;
    maxPressure = center + 0.5;
  } else {
    float padding = (maxPressure - minPressure) * 0.15;
    minPressure -= padding;
    maxPressure += padding;
  }

  auto pressureToY = [&](float p) {
    int y = GRAPH_BOTTOM - (int)(((p - minPressure) / (maxPressure - minPressure)) * GRAPH_HEIGHT);
    if (y < GRAPH_TOP) y = GRAPH_TOP;
    if (y > GRAPH_BOTTOM) y = GRAPH_BOTTOM;
    return y;
  };

  const int FIRST_POINT_OFFSET = 2;
  const float X_STEP = (float)(GRAPH_WIDTH - FIRST_POINT_OFFSET) / (PRESSURE_HISTORY_SIZE - 1);

  int previousX = GRAPH_LEFT + FIRST_POINT_OFFSET;
  int previousY = pressureToY(pressureHistory[0]);

  if (pressureHistoryCount == 1) {
    if (pressureBlinkState) display.fillRect(previousX - 1, previousY - 1, 3, 3, SSD1306_WHITE);
    return;
  }

  for (int i = 1; i < pressureHistoryCount; i++) {
    int x = GRAPH_LEFT + FIRST_POINT_OFFSET + (int)(i * X_STEP);
    int y = pressureToY(pressureHistory[i]);
    display.drawLine(previousX, previousY, x, y, SSD1306_WHITE);
    previousX = x;
    previousY = y;
  }

  if (pressureBlinkState) display.fillRect(previousX - 1, previousY - 1, 3, 3, SSD1306_WHITE);
}