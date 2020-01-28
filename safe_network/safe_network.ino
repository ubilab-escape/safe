/* call once to save credentials in flash memory */
#include <Preferences.h>

Preferences preferences;
const char* key_ssid = "ssid";
const char* ssid = "ubilab_wifi";
const char* key = "pass";
const char* passwordToSafe = "ohg4xah3oufohreiPe7e";


void setup() {
  preferences.begin("wifi", false); 
  preferences.putString(key, passwordToSafe);
  preferences.putString(key_ssid, ssid);
  preferences.end();
}

void loop() {
  // put your main code here, to run repeatedly:
  delay(500);
}
