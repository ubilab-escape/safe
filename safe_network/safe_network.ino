/* 
 * call once to save credentials in flash memory
 */
#include <Preferences.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

Preferences preferences;
const char* key_ssid = "ssid";
const char* ssid = "...";              // <-------- enter the ssid of your network

const char* key_WifiPassword = "pass";
const char* wifiPassword = "...";     // <--------- enter the password for your network

const char* key_safeCode = "safeCode";
const char* safeCode = "...";         // <---------- enter the safe code you want


void setup() {
  Serial.begin(115200);
  Serial.print("writing credentials into eeprom ");
  preferences.begin("wifi", false); 
  Serial.print(".");
  preferences.putString(key_WifiPassword, wifiPassword);
  Serial.print(".");
  preferences.putString(key_ssid, ssid);
  Serial.print(".");
  preferences.putString(key_safeCode, safeCode);
  Serial.print(".");
  preferences.end();
  Serial.println(" Done");
  Serial.println("Trying to connect to wifi...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, wifiPassword);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }
  Serial.println("Initializing update over the air...");
  ArduinoOTA.setHostname("5/safe_updateCredentials");
  ArduinoOTA
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else // U_SPIFFS
        type = "filesystem";
      Serial.println("Start updating " + type);
    })
    .onEnd([]() {
      Serial.println("\nEnd");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });
  ArduinoOTA.begin();
  Serial.println("Update ota started.");
}

void loop() {
  // esp_wifi_set_ps(WIFI_PS_NONE); // disable power saving features for better ping; probably not needed here
  ArduinoOTA.handle();
}
