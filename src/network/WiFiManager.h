#pragma once
#include <Arduino.h>

extern bool wifiConnected;
extern bool wifiAttemptFinished;

void wifiStart();          // WiFi.mode + WiFi.begin
bool wifiIsConnected();    // обновляет wifiConnected и возвращает его