#include <PubSubClient.h>
#include <WiFi.h>
#include "esp_wifi.h"
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <Preferences.h>

const char* key_ssid = "ssid";
const char* key_pwd = "pass";

WiFiClient espClient;
PubSubClient client(espClient);
Preferences preferences;

// Add your MQTT Broker IP address, example:
const char* mqtt_server = "10.0.0.2";

char* puzzleSolved_message = "{\"method\": \"STATUS\", \"state\": \"solved\"}";

#define S1 17
#define S2 16
#define S3 5
#define S4 18
#define S5 27
#define S6 14
#define S7 13
#define S8 12
#define LED_S_SOLVED 32
#define LED_V_SOLVED 23
#define MEASUREMENT_PIN A0
#define LOWER_THRESHOLD 500
#define HIGHER_THRESHOLD 700
#define AMOUNT_CORRECT_MEAS 5
#define WIFI_TIMEOUT_MS 3000
unsigned char switch_status;
unsigned char voltage_status;
unsigned char puzzle_solved;
unsigned int adc_val;
unsigned int s1_val;
unsigned int s2_val;
unsigned int s3_val;
unsigned int s4_val;
unsigned int s5_val;
unsigned int s6_val;
unsigned int s7_val;
unsigned int s8_val;
unsigned int adc_correct_value;
void setup() {
switch_status = 0;
voltage_status = 0;
puzzle_solved = 0;
adc_val = 0;
s1_val = 0;
s2_val = 0;
s3_val = 0;
s4_val = 0;
s5_val = 0;
s6_val = 0;
s7_val = 0;
s8_val = 0;
adc_correct_value = 0;
  pinMode(A0,INPUT);
  pinMode(S1,INPUT_PULLUP);
  pinMode(S2,INPUT_PULLUP);
  pinMode(S3,INPUT_PULLUP);
  pinMode(S4,INPUT_PULLUP);
  pinMode(S5,INPUT_PULLUP);
  pinMode(S6,INPUT_PULLUP);
  pinMode(S7,INPUT_PULLUP);
  pinMode(S8,INPUT_PULLUP);
  // Register all Interrupts for the callback function
  //attachInterrupt(digitalPinToInterrupt(S1), check_switches, CHANGE);
  //attachInterrupt(digitalPinToInterrupt(S2), check_switches, CHANGE);
  //attachInterrupt(digitalPinToInterrupt(S3), check_switches, CHANGE);
  //attachInterrupt(digitalPinToInterrupt(S4), check_switches, CHANGE);
  //attachInterrupt(digitalPinToInterrupt(S5), check_switches, CHANGE);
  //attachInterrupt(digitalPinToInterrupt(S6), check_switches, CHANGE);
  //attachInterrupt(digitalPinToInterrupt(S7), check_switches, CHANGE);
  //attachInterrupt(digitalPinToInterrupt(S8), check_switches, CHANGE);
  
  pinMode(LED_S_SOLVED,OUTPUT); digitalWrite(LED_S_SOLVED,HIGH);
  pinMode(LED_V_SOLVED,OUTPUT); digitalWrite(LED_V_SOLVED, HIGH);
  Serial.begin(9600);
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

void loop() {
  
    client.loop();
    while (!client.connected()){
      Serial.println("no connection");
      client.connect("SafeActivation");
      //client.subscribe("5_safe_activate");
    }
  voltage_status = check_voltage();
  check_switches();
  delay(400);
  
    // Publish solved Message once if both parts are solved 
  if(!puzzle_solved){
    if(voltage_status  && switch_status){
      Serial.println("Both puzzles solved");
      client.publish("5/safe/activate", puzzleSolved_message, true);
      puzzle_solved = 1;
    }
    else{
      puzzle_solved = 0;
    }
  }
}



void setup_wifi() {
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting");
  int timeout_start = millis();

  char* ssid = NULL;
  char* wlan_password = NULL;
  preferences.begin("wifi", false); 
  preferences.getString(key_pwd, wlan_password, 30);
  preferences.getString(key_ssid, ssid, 30);
  preferences.end();
  WiFi.begin(ssid, wlan_password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
     if (timeout_start + WIFI_TIMEOUT_MS < millis()) {
      ESP.restart();
    }
  }
  digitalWrite(LED_S_SOLVED,LOW);
  digitalWrite(LED_V_SOLVED, LOW);
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

// returns one when sensing the correct voltage
unsigned int check_voltage(){
  adc_val = analogRead(A0);
  Serial.print("ADC VAL: "); Serial.println(adc_val);
  // Turn on led and return 1 if measured voltage is between Thresholds
  if((adc_val < HIGHER_THRESHOLD) && (adc_val > LOWER_THRESHOLD)){
    adc_correct_value = adc_correct_value + 1;
    // Low Tech "high pass" filter
    if(adc_correct_value > AMOUNT_CORRECT_MEAS){
        digitalWrite(LED_V_SOLVED,HIGH);
        Serial.println("Puzzle Voltage Solved");
        adc_correct_value = 0;
        return 1;
    }
    else{
      return 0;
    }
  }
  else{
    digitalWrite(LED_V_SOLVED,LOW);
    adc_correct_value = 0;
    return 0;
  }
  return 0;
}


// Callback by switch change
void check_switches(){
    s1_val = digitalRead(S1);
    s2_val = digitalRead(S2);
    s3_val = digitalRead(S3);
    s4_val = digitalRead(S4);
    s5_val = digitalRead(S5);
    s6_val = digitalRead(S6);
    s7_val = digitalRead(S7);
    s8_val = digitalRead(S8);
  // Only for Debugging
  /*
  Serial.print("S1: ");  Serial.println(s1_val);
  Serial.print("S2: ");  Serial.println(s2_val);
  Serial.print("S3: ");  Serial.println(s3_val);
  Serial.print("S4: ");  Serial.println(s4_val);
  Serial.print("S5: ");  Serial.println(s5_val);
  Serial.print("S6: ");  Serial.println(s6_val);
  Serial.print("S7: ");  Serial.println(s7_val);
  Serial.print("S8: ");  Serial.println(s8_val);
  */
  if(!s2_val && !s3_val && !s6_val && !s8_val && s1_val && s4_val && s5_val && s7_val){
    Serial.println("Puzzle Switches Solved");
    switch_status = 1;
    digitalWrite(LED_S_SOLVED,HIGH);
  }
  else{
    switch_status = 0;
    digitalWrite(LED_S_SOLVED,LOW);
  }
  delay(200);
} 

void callback(char* topic, byte* payload, unsigned int length) {
    Serial.print("Received message [");
    Serial.print(topic);
    Serial.print("] ");
    char msg[length+1];
    for (int i = 0; i < length; i++) {
        Serial.print((char)payload[i]);
        msg[i] = (char)payload[i];
    }
    Serial.println();
    msg[length] = '\0';
    Serial.println(msg);
}
