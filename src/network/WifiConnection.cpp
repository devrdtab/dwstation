#include "WifiConnection.h"
#include <WiFiManager.h>

WiFiManager wifiManager;

bool wifiConnected = false;

static void onConfigPortalStarted(WiFiManager *wm) {
  Serial.println();
  Serial.println("================================");
  Serial.println("WIFI SETUP PORTAL STARTED");
  Serial.println("================================");
  Serial.print("AP: ");
  Serial.println(wm->getConfigPortalSSID());
  Serial.print("Portal IP: ");
  Serial.println(WiFi.softAPIP());
}

static void onWifiConfigSaved() {
  Serial.println("Новые данные WiFi сохранены.");
}

void wifiInit() {
  wifiManager.setConnectTimeout(WIFI_CONNECT_TIMEOUT_SEC);
  wifiManager.setConfigPortalBlocking(false);
  wifiManager.setConfigPortalTimeout(0);
  wifiManager.setAPCallback(onConfigPortalStarted);
  wifiManager.setSaveConfigCallback(onWifiConfigSaved);
  WiFi.setAutoReconnect(true); // тихий авто-реконнект к той же сети при обрыве
}

bool wifiWasSaved() { return wifiManager.getWiFiIsSaved(); }

bool wifiConnectBlocking() {
  bool connected = wifiManager.autoConnect(AP_SSID, AP_PASS);
  wifiConnected = connected;
  if (connected) {
    Serial.print("WiFi connected, IP: ");
    Serial.println(WiFi.localIP());
  }
  return connected;
}

void wifiStartSetupPortal() {
  if (wifiManager.getConfigPortalActive()) {
    wifiManager.stopConfigPortal();
  }
  wifiManager.startConfigPortal(AP_SSID, AP_PASS);
}

bool wifiIsPortalActive() { return wifiManager.getConfigPortalActive(); }

void wifiLoop() {
  wifiManager.process();

  bool nowConnected = (WiFi.status() == WL_CONNECTED);
  if (nowConnected && !wifiConnected) {
    Serial.print("WiFi connected, IP: ");
    Serial.println(WiFi.localIP());
  }
  wifiConnected = nowConnected;
}