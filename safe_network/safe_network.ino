/* this file connects to a network via wifi, it reads the passwort from flash memory*/
#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
// use preferences to load/safe wifi credentials
#include <Preferences.h>

Preferences preferences;
const char* ssid = "Network";
char password[12];
const char* key = "pass";
const char* passwordToSafe = "hallohallo12";
String pw;
void setup() {
  preferences.begin("wifi", false); 
  pw = preferences.getString("pass", "default");
  preferences.end();
  
  Serial.begin(115200);
  Serial.println(password);
  Serial.println(pw);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.println("");

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

}

void loop() {
  // put your main code here, to run repeatedly:
  delay(500);
  printWifiStatus();
}

void printWifiStatus() {
  // print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print your WiFi shield's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
}
