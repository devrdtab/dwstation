#pragma once
#include <Arduino.h>

enum ButtonEvent {
  BUTTON_EVENT_NONE,
  BUTTON_EVENT_PRESSED,   // кнопку нажали (один раз за нажатие)
  BUTTON_EVENT_LONG_PRESS // кнопку держат дольше LONG_PRESS_MS (один раз за
                          // нажатие)
};

void buttonInit();
ButtonEvent buttonUpdate(); // вызывать в loop() как можно чаще
void buttonLedUpdate(
    bool screenOn); // мигание в ожидании, выключен при включённом экране