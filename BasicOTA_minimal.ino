#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

const char* ssid = "...";
const char* password = "...";

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  int x = millis();
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
   if (x + 5000 < millis()) {
      ESP.restart();
    }
  }
  ArduinoOTA
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else
        type = "filesystem";
    });
  ArduinoOTA.begin();
}

void loop() {
  esp_wifi_set_ps(WIFI_PS_NONE); // disable power saving features for better ping
  ArduinoOTA.handle();
}
