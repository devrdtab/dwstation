#include "RemoteSetup.h"
#include "WifiConnection.h"
#include <ESPmDNS.h>
#include <WebServer.h>

static WebServer server(8080);

static void handleWifiSetup() {
  server.send(200, "text/plain",
              "WiFi setup portal starting. Connect your phone to 'Meteo WiFi' "
              "to reconfigure.");
  wifiStartSetupPortal();
}

void remoteSetupInit() {
  if (MDNS.begin("mini-weather")) {
    Serial.println("mDNS: http://mini-weather.local:8080/wifi-setup");
  } else {
    Serial.println("mDNS: не удалось запустить");
  }

  server.on("/wifi-setup", HTTP_GET, handleWifiSetup);
  server.begin();
  MDNS.addService("http", "tcp", 8080);
}

void remoteSetupLoop() { server.handleClient(); }