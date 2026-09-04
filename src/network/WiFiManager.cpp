#include "WiFiManager.h"
#include "config/Secrets.h"
#include <WiFi.h>

bool wifiConnected = false;
bool wifiAttemptFinished = false;

void wifiStart() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.println();
  Serial.println("Подключение к WiFi...");
}

bool wifiIsConnected() {
  wifiConnected = (WiFi.status() == WL_CONNECTED);
  return wifiConnected;
}