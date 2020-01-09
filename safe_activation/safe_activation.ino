#include <PubSubClient.h>
#include <WiFi.h>
#include "esp_wifi.h"
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
const char* ssid = "Darknet";
const char* password = "2556732986301168";

WiFiClient espClient;
PubSubClient client(espClient);

// Add your MQTT Broker IP address, example:
const char* mqtt_server = "192.168.178.21";

char* puzzleSolved_message = "{\"method\": \"STATUS\", \"state\": \"solved\"}";

#define S1 17
#define S2 16
#define S3 5
#define S4 18
#define S5 27
#define S6 14
#define S7 13
#define S8 12
#define LED_S 32
#define MEASUREMENT_PIN A0
#define LOWER_THRESHOLD 500
#define HIGHER_THRESHOLD 800
unsigned char switch_status = 0;
unsigned char voltage_status = 0;
unsigned char puzzle_solved = 0;
unsigned int adc_val = 0;
unsigned int s1_val = 0;
unsigned int s2_val = 0;
unsigned int s3_val = 0;
unsigned int s4_val = 0;
unsigned int s5_val = 0;
unsigned int s6_val = 0;
unsigned int s7_val = 0;
unsigned int s8_val = 0;
void setup() {
  pinMode(A0,INPUT);
  pinMode(S1,INPUT_PULLUP);
  pinMode(S2,INPUT_PULLUP);
  pinMode(S3,INPUT_PULLUP);
  pinMode(S4,INPUT_PULLUP);
  pinMode(S5,INPUT_PULLUP);
  pinMode(S6,INPUT_PULLUP);
  pinMode(S7,INPUT_PULLUP);
  pinMode(S8,INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(S1), check_switches, CHANGE);
  attachInterrupt(digitalPinToInterrupt(S2), check_switches, CHANGE);
  attachInterrupt(digitalPinToInterrupt(S3), check_switches, CHANGE);
  attachInterrupt(digitalPinToInterrupt(S4), check_switches, CHANGE);
  attachInterrupt(digitalPinToInterrupt(S5), check_switches, CHANGE);
  attachInterrupt(digitalPinToInterrupt(S6), check_switches, CHANGE);
  attachInterrupt(digitalPinToInterrupt(S7), check_switches, CHANGE);
  attachInterrupt(digitalPinToInterrupt(S8), check_switches, CHANGE);
  
  pinMode(LED_S,OUTPUT);
  digitalWrite(LED_S,LOW);
  Serial.begin(9600);
  
  setup_wifi();
  client.setServer(mqtt_server, 1884);
  client.setCallback(callback);
}

void loop() {
  
    client.loop();
    while (!client.connected()){
      Serial.println("no connection");
      client.connect("SafeActivation");
      //client.subscribe("5_safe_activate");
    }
  
    Serial.println("connected");

  voltage_status = check_voltage();
  if(voltage_status  && switch_status){
    Serial.println("Both puzzles solved");
    client.publish("mqtt_5_safe_activate", puzzleSolved_message);
    puzzle_solved = 1;
  }
  else{
    puzzle_solved = 0;
  }
  delay(100);
}



void setup_wifi() {
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

unsigned int check_voltage(){
  adc_val = analogRead(A0);
  Serial.print("ADC VAL: "); Serial.println(adc_val);
  if((adc_val < HIGHER_THRESHOLD) && (adc_val > LOWER_THRESHOLD)){
    Serial.println("Puzzle Voltage Solved");
    return 1;
  }
  else{
    return 0;
  }
}


// Callback by switch change
void check_switches(){
  Serial.print("S1: "); s1_val = digitalRead(S1); Serial.println(s1_val);
  Serial.print("S2: "); s2_val = digitalRead(S2); Serial.println(s2_val);
  Serial.print("S3: "); s3_val = digitalRead(S3); Serial.println(s3_val);
  Serial.print("S4: "); s4_val = digitalRead(S4); Serial.println(s4_val);
  Serial.print("S5: "); s5_val = digitalRead(S5); Serial.println(s5_val);
  Serial.print("S6: "); s6_val = digitalRead(S6); Serial.println(s6_val);
  Serial.print("S7: "); s7_val = digitalRead(S7); Serial.println(s7_val);
  Serial.print("S8: "); s8_val = digitalRead(S8); Serial.println(s8_val);
  if(!s2_val && !s3_val && !s6_val && !s8_val && s1_val && s4_val && s5_val && s7_val){
    Serial.println("Puzzle Switches Solved");
    switch_status = 1;
    digitalWrite(LED_S,HIGH);
  }
  else{
    switch_status = 0;
    digitalWrite(LED_S,LOW);
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
