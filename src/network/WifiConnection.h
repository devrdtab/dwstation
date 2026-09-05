#pragma once
#include <Arduino.h>

constexpr char AP_SSID[] = "Meteo WiFi";
constexpr char AP_PASS[] = "meteo123";
constexpr unsigned long WIFI_CONNECT_TIMEOUT_SEC = 15;

extern bool wifiConnected;

void wifiInit();
bool wifiWasSaved();
bool wifiConnectBlocking();
void wifiStartSetupPortal();
bool wifiIsPortalActive();
void wifiLoop();