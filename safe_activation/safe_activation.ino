#include <PubSubClient.h>
#include <WiFi.h>
#include "esp_wifi.h"
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <Preferences.h>
#include <ArduinoJson.h>

// Placeholder for ssid and password read from ram
const char* key_ssid = "ssid";
const char* key_pwd = "pass";

WiFiClient espClient;
PubSubClient client(espClient);
Preferences preferences;

// Add your MQTT Broker IP address, example:
const char* mqtt_server = "10.0.0.2";

// Status messages
char* puzzleSolved_message = "{\"method\": \"status\", \"state\": \"solved\"}";
char* puzzleActive_message = "{\"method\": \"status\", \"state\": \"active\"}";
char* puzzleInactive_message = "{\"method\": \"status\", \"state\": \"inactive\"}";
StaticJsonDocument<300> mqtt_decoder;

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

typedef enum
{
  STATE_INACTIVE = 0,
  STATE_ACTIVE = 1,
  STATE_SOLVED = 2,
  } puzzle_states;

typedef enum
{
  VOLT_SOLVED = 1,
  VOLT_NOT_SOLVED = 0,
  } voltage_states;

typedef enum
{
  SWITCH_SOLVED = 1,
  SWITCH_NOT_SOLVED = 0,
  } switch_states;
  
unsigned char switch_status;
unsigned char voltage_status;
unsigned char puzzle_status;
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
  switch_status = SWITCH_NOT_SOLVED;
  voltage_status = VOLT_NOT_SOLVED;
  puzzle_status = STATE_ACTIVE;
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
  // Init ADC GPIO
  pinMode(A0,INPUT);
  // Init GPIO for checking the switches
  pinMode(S1,INPUT_PULLUP);
  pinMode(S2,INPUT_PULLUP);
  pinMode(S3,INPUT_PULLUP);
  pinMode(S4,INPUT_PULLUP);
  pinMode(S5,INPUT_PULLUP);
  pinMode(S6,INPUT_PULLUP);
  pinMode(S7,INPUT_PULLUP);
  pinMode(S8,INPUT_PULLUP);
  pinMode(LED_S_SOLVED,OUTPUT); digitalWrite(LED_S_SOLVED,HIGH);
  pinMode(LED_V_SOLVED,OUTPUT); digitalWrite(LED_V_SOLVED, HIGH);
  Serial.begin(9600);
  setup_wifi();
  initOTA();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

void loop() {
  // Debug output current state
  //Serial.print("Current State:"); Serial.println(puzzle_status); 
  // Always connect to the mqqt server
  connect_mqqt();
  if(puzzle_status == STATE_ACTIVE){  
    // Check wether puzzle are solved
    voltage_status = check_voltage();
    switch_status = check_switches();
    // Publish solved Message once if both parts are solved 
    if(!(puzzle_status == STATE_SOLVED)){
      if(voltage_status  && switch_status){
        Serial.println("Both puzzles solved");
        client.publish("5/safe/activate", puzzleSolved_message, true);
        puzzle_status = STATE_SOLVED;
      }
    }
  }
  ArduinoOTA.handle();
}

// Function returning 1 when switches are in the correct position
unsigned int check_switches(){
    s1_val = digitalRead(S1);
    s2_val = digitalRead(S2);
    s3_val = digitalRead(S3);
    s4_val = digitalRead(S4);
    s5_val = digitalRead(S5);
    s6_val = digitalRead(S6);
    s7_val = digitalRead(S7);
    s8_val = digitalRead(S8);
  /*
  // Debug output
  Serial.print("S1: ");  Serial.println(s1_val);
  Serial.print("S2: ");  Serial.println(s2_val);
  Serial.print("S3: ");  Serial.println(s3_val);
  Serial.print("S4: ");  Serial.println(s4_val);
  Serial.print("S5: ");  Serial.println(s5_val);
  Serial.print("S6: ");  Serial.println(s6_val);
  Serial.print("S7: ");  Serial.println(s7_val);
  Serial.print("S8: ");  Serial.println(s8_val);
  */
  // If switches are in the correct position set status LED to High
  if(!s2_val && !s3_val && !s6_val && !s8_val && s1_val && s4_val && s5_val && s7_val){
    Serial.println("Puzzle Switches Solved");
    delay(200);
    digitalWrite(LED_S_SOLVED,HIGH);
    return SWITCH_SOLVED;
  }
  else{
    delay(200);
    digitalWrite(LED_S_SOLVED,LOW);
    return SWITCH_NOT_SOLVED;
  }
}

// returns 1 when sensing the correct voltage
unsigned int check_voltage(){
  adc_val = analogRead(A0);
  // Debug output
  //Serial.print("ADC VAL: "); Serial.println(adc_val);
  // Turn on led and return 1 if measured voltage is between Thresholds
  if((adc_val < HIGHER_THRESHOLD) && (adc_val > LOWER_THRESHOLD)){
    adc_correct_value = adc_correct_value + 1;
    // Only return solved when there are a least 5 consecutive measurements in the right voltage range
    if(adc_correct_value > AMOUNT_CORRECT_MEAS){
        digitalWrite(LED_V_SOLVED,HIGH);
        Serial.println("Puzzle Voltage Solved");
        adc_correct_value = 0;
        return VOLT_SOLVED;
    }
    else{
      return VOLT_NOT_SOLVED;
    }
  }
  else{
    digitalWrite(LED_V_SOLVED,LOW);
    adc_correct_value = 0;
    return VOLT_NOT_SOLVED;
  }
  return VOLT_NOT_SOLVED;
}


void setup_wifi() {
  delay(10);
  // Connect to selected WiFi Network
  Serial.println();
  Serial.print("Connecting");
  int timeout_start = millis();
  char ssid[30];
  char wlan_password[30];
  preferences.begin("wifi", false); 
  // Get password and ssid from ram
  preferences.getString(key_pwd, wlan_password, 30);
  preferences.getString(key_ssid, ssid, 30);
  preferences.end();
  WiFi.begin(ssid, wlan_password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
      // Restart ESP when no connection to the Network is established
     if (timeout_start + WIFI_TIMEOUT_MS < millis()) {
      ESP.restart();
    }
  }
  // Turn LED off to signalise that the device is connected to the Network
  digitalWrite(LED_S_SOLVED,LOW);
  digitalWrite(LED_V_SOLVED, LOW);
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void initOTA() {
  ArduinoOTA.setHostname("5/safe_activation");
  ArduinoOTA
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else // U_SPIFFS
        type = "filesystem";

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
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
}

// Mqqt callback function
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
    // Deserialzie messafe
    deserializeJson(mqtt_decoder, msg);
    const char* method_msg = mqtt_decoder["method"];
    const char* state_msg = mqtt_decoder["state"];
    const char* data_msg = mqtt_decoder["data"];
    Serial.println(method_msg);
    Serial.println(state_msg);
    Serial.println(strcmp(method_msg, "trigger") == 0);
    Serial.println(strcmp(state_msg, "on") == 0);
    // If trigger on message was sended
    if(strcmp(topic, "5/safe/activate") == 0 && strcmp(method_msg, "trigger") == 0 && strcmp(state_msg, "on") == 0){
      puzzle_status = STATE_ACTIVE;
      client.publish("5/safe/activate", puzzleActive_message, true);
    }
   // If trigger off message was sended
    if(strcmp(topic, "5/safe/activate") == 0 && strcmp(method_msg, "trigger") == 0 && strcmp(state_msg, "off") == 0){
      puzzle_status = STATE_INACTIVE;
      client.publish("5/safe/activate", puzzleInactive_message, true);
    }  
}

// Function connecting to the mqqt server and suscribing the topic
void connect_mqqt(){
	client.loop();
  while (!client.connected()){
    Serial.println("no connection");
    client.connect("SafeActivation");
    client.subscribe("5/safe/activate");
  }
}
