#include "RemoteSetup.h"
#include "WifiConnection.h"
#include <WebServer.h>

// Порт 8080, а не 80 — на 80-м во время настройки уже сидит сам WiFiManager.
static WebServer server(8080);

static void handleWifiSetup() {
  server.send(200, "text/plain",
              "WiFi setup portal starting. Connect your phone to 'Meteo WiFi' "
              "to reconfigure.");
  wifiStartSetupPortal();
}

void remoteSetupInit() {
  server.on("/wifi-setup", HTTP_GET, handleWifiSetup);
  server.begin();
  Serial.println("Remote setup: http://<device-ip>:8080/wifi-setup");
}

void remoteSetupLoop() { server.handleClient(); }