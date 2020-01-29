/* call once to save credentials in flash memory */
#include <Preferences.h>

Preferences preferences;
const char* key_ssid = "ssid";
const char* ssid = "...";
const char* key_WifiPassword = "pass";
const char* wifiPassword = "...";
const char* key_safeCode = "safeCode";
const char* safeCode = "...";


void setup() {
  preferences.begin("wifi", false); 
  preferences.putString(key_WifiPassword, wifiPassword);
  preferences.putString(key_ssid, ssid);
  preferences.putString(key_safeCode, safeCode);
  preferences.end();
}

void loop() {
  // put your main code here, to run repeatedly:
  delay(500);
}
