/* 
 * TODO:
 * reset (go back to "noPower_state");
 * change password store location !!!
*/
// the code for the keypad was taken from https://www.adafruit.com/product/3845 (25.11.19)
// the code for the LCD was taken from https://starthardware.org/lcd/ (4.12.19)
// the code for the I2C LCD was taken from https://funduino.de/nr-19-i%C2%B2c-display (15.12.19)
// the code for the OTA update was taken from the examples: File -> Examples -> ArduinoOTA -> BasicOTA

#include "Keypad.h"              // keypad library by Mark Stanley, Alexander Brevig
#include <LiquidCrystal.h>
#include <LiquidCrystal_I2C.h>   // I2C library by Frank de Brabander
// acceleration sensor: libraries needed: Adafruit 9DOF; Adafruit ADXL343; Adafruit AHRS; Adafruit BusIO;
//                                        Adafruit Circuit Playground; Adafruit LSM303 Accel
#include <Adafruit_LSM303_Accel.h>
#include "Adafruit_Sensor.h"
#include <Wire.h>
#include <Adafruit_NeoPixel.h>  // library for LED stripe
// libraries for ESP OTA update:
#include <WiFi.h>
#include "esp_wifi.h"
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>     // library for MQTT
#include <Adafruit_NeoPixel.h>
#include <ArduinoJson.h>
#include <Preferences.h>

#define USE_ESP32
#define WLAN_enable true
#define SAFE_PW_LENGTH 4
#define USE_I2C_LCD
#define countdownStart 7
#define NUMPIXELS      32
#define MIN_BRIGHT_DEACT 30
#define MAX_BRIGHT_DEACT 120
#define MQTTport 1883
#define thisTopicName "5/safe/control"
#define ACC_SENSOR_THRESHOLD 12

const char* MQTT_BROKER = "10.0.0.2";
Preferences preferences;
long piezo_time = 0;
int count_num = 0;
bool connectWLAN = WLAN_enable;

WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;
char msg[50];
int value = 0;
// ----------------------------------------------------------------------------------------------
// keypad:
const byte ROWS = 4; //four rows
const byte COLS = 3; //three columns
char keys[ROWS][COLS] = {
  {'1', '2', '3'},
  {'4', '5', '6'},
  {'7', '8', '9'},
  {'*', '0', '#'}
};

const char* key_ssid = "ssid";
const char* key_pwd = "pass";

#ifdef USE_ESP32
  byte rowPins[ROWS] = {12, 33, 25, 27}; //connect to the row pinouts of the keypad
  byte colPins[COLS] = {14, 13, 26}; //connect to the column pinouts of the keypad
#else
  byte rowPins[ROWS] = {12, 0, 8, 10}; //connect to the row pinouts of the keypad
  byte colPins[COLS] = {11, 13, 9}; //connect to the column pinouts of the keypad
#endif


Keypad keypad = Keypad( makeKeymap(keys), rowPins, colPins, ROWS, COLS );


// ----------------------------------------------------------------------------------------------
// LCD:
#ifdef USE_I2C_LCD
#  ifdef USE_ESP32
     LiquidCrystal_I2C lcd(0x27, 16, 2);  // GPIO 21 = SDA; GPIO 22 = SCL (VCC = 5V)
#  else
     LiquidCrystal_I2C lcd(0x27, 16, 2);  // pin A4 = SDA; pin A5 = SCL (VCC = 5V)
#  endif
#else
#  ifdef USE_ESP32
     const int en = 15, rs = 2, d4 = 4, d5 = 5, d6 = 18, d7 = 19;
#  else
     const int en = 7, rs = 2, d4 = 3, d5 = 4, d6 = 5, d7 = 6;
#  endif
   LiquidCrystal lcd(rs, en, d4, d5, d6, d7);
#endif

#define LED_COLOR_WHITE 0
#define LED_COLOR_RED 1
#define LED_COLOR_GREEN 2
#define LED_COLOR_BLUE  3
#define LED_COLOR_ORANGE 4

#define LED_MODE_ON 0
#define LED_MODE_OFF 1
#define LED_MODE_PULSE 2
#define LED_MODE_BLINK 3

#define SWITCH_PIN  32
#define LOCKPIN 18
#define LED_PIN            2
#define BUZZER_PIN  4

int led_mode = 0;
uint32_t delay_led = 0;
int led_brightness = 60;
bool led_asc = 0;
uint8_t piezo_time_mqtt = 3;

StaticJsonDocument<300> mqtt_decoder;
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NUMPIXELS, LED_PIN, NEO_GRB + NEO_KHZ800);


// ----------------------------------------------------------------------------------------------
// safe code:
int safePassword[SAFE_PW_LENGTH] = {4, 2, 4, 2};     // <-------------------------- safe code
int currentTry[SAFE_PW_LENGTH] = {};
// safe status:
enum safeStatusEnum {start_state, connectingWLAN_state, noPower_state, locked_state, lockedAlarm_state, wrongSafePassword_state, openLock_state, unlocked_state};
enum safeStatusEnum safeStatus = start_state;

// timer:
const int messageLength = 1000;  // time while the message "wrong safe code" is shown
unsigned long startTime, currentTime;
unsigned long startTime_PS, currentTime_PS;

int countdown = countdownStart;
bool piezo_controlled_by_mqtt = false;
// speaker:
const int freq = 3000;
const int freq2 = 4000;
const int channel = 0;
const int resolution = 8;
// Assign a unique ID to this sensor at the same time
Adafruit_LSM303_Accel_Unified accel = Adafruit_LSM303_Accel_Unified(54321);

bool disguardACCvalues = false;
int disguardACCvalues_start;
int disguardACCvalues_time = 100;

void setColor(int colorCode, int setMode){
  uint32_t col = 0;
  switch(colorCode){
    case LED_COLOR_WHITE: // white
      col = pixels.Color(255, 255, 255);
    break;
    case LED_COLOR_RED: // red
      col = pixels.Color(255, 0, 0);
    break;
    case LED_COLOR_GREEN: // green
      col = pixels.Color(0, 255, 0);
    break;
    case LED_COLOR_BLUE: // blue
      col = pixels.Color(0, 0, 255);
    break;
    case LED_COLOR_ORANGE:
      col = pixels.Color(255, 128, 0);
    break;
  }
  for(int i=0;i<NUMPIXELS;i++){
    pixels.setPixelColor(i, col);
    pixels.show(); // This sends the updated pixel color to the hardware.
  }
  led_mode = setMode;
  switch(setMode){
    case 0:
      pixels.setBrightness(MAX_BRIGHT_DEACT);
      pixels.show();
    break;
    case 1:
      pixels.clear();
    break;
    case 2:
      led_brightness = 60;
      delay_led = 0;
    break;
    case 3:
      pixels.setBrightness(MAX_BRIGHT_DEACT);
      pixels.show();
      delay_led = 0;
    break;
  }
}


void piezoControl_for_mqtt(void) {
  if(piezo_controlled_by_mqtt) {
    long time_diff = floor((millis() - piezo_time)/100)*100;
    if(time_diff % 1000 == 0) {
      ledcWriteTone(channel, freq);
    } else if(time_diff % 500 == 0){
      ledcWriteTone(channel, freq2);
    }
    if(time_diff > piezo_time_mqtt*1000){
      ledcWrite(channel, 0);
      piezo_controlled_by_mqtt = false;
      Serial.println("piezo end");
    }
  }
}

char* createJson(char* method_s, char* state_s, char* data_s){
  StaticJsonDocument<300> doc;
  doc["method"] = method_s;
  doc["state"] = state_s;
  doc["data"] = data_s;
  static char JSON_String[300];
  serializeJson(doc, JSON_String);
  return JSON_String;
}

void setup() {
  Serial.begin(115200);
  //Serial.begin(9600);
  pinMode(SWITCH_PIN, INPUT_PULLUP);
  pinMode(LOCKPIN, OUTPUT);
  digitalWrite(LOCKPIN, LOW);
  safeStatus = start_state;
  printStatus();
  #ifdef USE_I2C_LCD
    lcd.init();
    lcd.backlight();
  #else
    lcd.begin(16, 2);
  #endif
  esp_wifi_set_ps(WIFI_PS_NONE);
  if (connectWLAN) {
    initOTA();
    client.setServer(MQTT_BROKER, MQTTport);
    client.setCallback(callback);
  }
  setup_vars();
}

void setup_vars(){
  currentTry[SAFE_PW_LENGTH] = {};
  initArray();
  safeStatus = noPower_state;
  printStatus();
  countdown = countdownStart;
  initPWM();
  initLEDstripe();
  pixels.begin();
  pixels.clear();
  setColor(LED_COLOR_RED, LED_MODE_ON);
  delay(10);
  startTime_PS = millis();
  int led_mode = 0;
  uint32_t delay_led = 0;
  int led_brightness = 60;
  bool led_asc = 0;
  uint8_t piezo_time_mqtt = 3;
  long piezo_time = 0;
  int count_num = 0;
  long lastMsg = 0;
  int value = 0;
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
    
    deserializeJson(mqtt_decoder, msg);
    const char* method_msg = mqtt_decoder["method"];
    const char* state_msg = mqtt_decoder["state"];
    const char* data_msg = mqtt_decoder["data"];
    Serial.println(method_msg);
    Serial.println(data_msg == NULL);
    Serial.println(strcmp(method_msg, "TRIGGER") == 0);
    Serial.println(strcmp(state_msg, "on") == 0);
    if(strcmp(method_msg, "TRIGGER") == 0 && strcmp(state_msg, "on") == 0 && data_msg != NULL && (unsigned)strlen(data_msg) >= 1){
      //change led
      if((unsigned)strlen(data_msg) >= 2) {
        int led_col = data_msg[0] - 48;
        int led_st = data_msg[2] - 48;
        if(led_col >= 0 && led_col <= 9 && led_st >= 0 && led_st <= 8) {
          setColor(led_col, led_st);
          Serial.println("led");
          Serial.println(data_msg);
        } else if(led_st == 9) {
          Serial.println("start piezo");
          ledcWrite(channel, 125);
          ledcWriteTone(channel, freq);
          piezo_controlled_by_mqtt = true;
          piezo_time = millis();
          piezo_time_mqtt = led_col;
        } else {
          Serial.println("invalid led/piezo control parameters");
        }
      } else {
        Serial.println("failed to parse led parameters");
      }
    } 

    if(strcmp(topic, "5/safe/control") == 0 && strcmp(method_msg, "TRIGGER") == 0 && strcmp(state_msg, "on") == 0 && ( data_msg == NULL || (unsigned)strlen(data_msg) == 0)){
      safeStatus = locked_state;
      setColor(LED_COLOR_ORANGE, LED_MODE_ON);
      printStatus();
      client.publish(thisTopicName, createJson("STATUS", "active", ""), true);
    } 
    if(strcmp(topic, "5/safe/control") == 0 && strcmp(method_msg, "TRIGGER") == 0 && strcmp(state_msg, "off") == 0){
      if(digitalRead(SWITCH_PIN) == 0){
        setup_vars();
        client.publish(thisTopicName, createJson("STATUS", "inactive", ""), true);
      } else {
        client.publish(thisTopicName, createJson("STATUS", "failed", "safe is still open, can't reset"), true);
      }
    }
}


void loop() {
  if (safeStatus != openLock_state) {    // just to be sure
    digitalWrite(LOCKPIN, LOW);
  }
  while (!client.connected() and connectWLAN) {
    Serial.println("no connection");
    client.connect("SafeDevice_M");
    client.subscribe(thisTopicName);
  }
  client.loop();
  if (connectWLAN) {
    if (WiFi.waitForConnectResult() != WL_CONNECTED) {    // TODO: better solution? problem: ESP loses the current state if the connection is lost!
      initOTA();
    }
    ArduinoOTA.handle();
  }
  action();
  printStatus();
  checkPiezo();
  piezoControl_for_mqtt();
  
  if(led_mode == LED_MODE_PULSE) {
    if(millis() - delay_led > 200){
      delay_led = millis();
      if(led_asc){
        led_brightness += 10;
      }else {
        led_brightness -= 10;
      }
      if(led_brightness > MAX_BRIGHT_DEACT){
        led_asc = false;
      } else if(led_brightness < MIN_BRIGHT_DEACT){
        led_asc = true;
      }
      pixels.setBrightness(led_brightness);
      pixels.show();
      Serial.println("Brighntess set to ");
      Serial.println(led_brightness);
    }
  } else if (led_mode == LED_MODE_BLINK) {
    if(millis() - delay_led > 500){
      delay_led = millis();
      if(led_asc){
        pixels.setBrightness(20);
        pixels.show();
        Serial.println("led blink on");
      }else {
        pixels.setBrightness(MAX_BRIGHT_DEACT);
        pixels.show();
        Serial.println("led blink off");
      }
      if(led_asc){
        led_asc = false;
      } else {
        led_asc = true;
      }
    }
  }
  currentTime_PS = millis();
  if (currentTime_PS - startTime_PS > 10000) {
	Serial.println("execute: esp_wifi_set_ps(WIFI_PS_NONE);");
	esp_wifi_set_ps(WIFI_PS_NONE);
	startTime_PS = millis();
  }
}

void action() {
  switch (safeStatus) {
    case noPower_state:
    {
      if (!connectWLAN) {
        char key2 = keypad.getKey();
        if ((key2 != NO_KEY) and (key2 == '#' or key2 == '*')) {
          lock();
        }
      }
      checkPiezo();
    }
    break;
    case lockedAlarm_state:
    {
      long time_diff = floor((millis() - piezo_time)/100)*100;
      if(time_diff % 1000 == 0) {
        ledcWriteTone(channel, freq);
      } else if(time_diff % 500 == 0){
        ledcWriteTone(channel, freq2);
      }
      if(time_diff > 3000){
        ledcWrite(channel, 0);
        safeStatus = locked_state;
        setColor(LED_COLOR_ORANGE, LED_MODE_ON);  // orange again
      }
    }
    break;
    case locked_state:
    {
      char key = keypad.getKey();
      if (key != NO_KEY) {
        append(key);
      }
      if (disguardACCvalues and (millis() - disguardACCvalues_start  > disguardACCvalues_time)) {
        disguardACCvalues = false;
      }
    }
    break;
    case wrongSafePassword_state:
    {
      currentTime = millis();
      if (currentTime - startTime >= messageLength) {
        lock();
      }
      else if (currentTime < startTime) {
        // TODO: is this necessary to prevent problems with overflow (restart with 0) ??
        // just call function again:
        wrongSafePassword();
      }
    }
    break;
    case openLock_state:
    {
      digitalWrite(LOCKPIN, HIGH);
      int switchValue = digitalRead(SWITCH_PIN);
      if (switchValue == 1) {
        digitalWrite(LOCKPIN, LOW);
        safeStatus = unlocked_state;
        digitalWrite(LOCKPIN, LOW);
        client.publish(thisTopicName, createJson("STATUS", "solved", ""), true);
        setColor(LED_COLOR_GREEN, LED_MODE_ON); // Green on
      }
      initArray();
    }
    break;
    case unlocked_state:
    {
      int switchValue2 = digitalRead(SWITCH_PIN);
      if (switchValue2 == 0) {
        lock();
      }
    }
    break;
  }
}

void append(char inpChar) {
  if (inpChar == '*' or inpChar == '#') {
    // prevent those symbols to avoid bugs
    return;
  }
  int inp = inpChar - 48;    // char -> int
  int i = 0;
  while (i < SAFE_PW_LENGTH) {
    if (currentTry[i] == -1) {
      currentTry[i] = inp;
      break;
    }
    i++;
  }
  if (i < SAFE_PW_LENGTH - 1) {
    // not enough numbers typed in
    printSafePassword();
    return;
  }
  // check if Code is correct:
  checkSafePassword();
}

void lock() {
  int switchValue3 = digitalRead(SWITCH_PIN);
  if (switchValue3 == 0) {
    initArray();
    safeStatus = locked_state;
    printStatus();
    setColor(LED_COLOR_ORANGE, LED_COLOR_ORANGE);
  }
}

void wrongSafePassword() {
  printSafePassword();
  safeStatus = wrongSafePassword_state;
  setColor(LED_COLOR_RED, LED_MODE_BLINK); //red blink
  printStatus();
  startTime = millis();
}

void initArray () {
  for (int i = 0; i < SAFE_PW_LENGTH; i++) {
    currentTry[i] = -1;
  }
}

void checkSafePassword() {
  for (int i = SAFE_PW_LENGTH - 1; i >= 0; i--) {
    if (currentTry[i] != safePassword[i]) {
      // safe code not correct:
      wrongSafePassword();
      return;
    }
  }
  // safe code correct:
  safeStatus = openLock_state;
  setColor(LED_COLOR_GREEN, LED_MODE_BLINK); //green lock
  countdown = countdownStart;
  startTime = millis();
  initArray();
  printStatus();
}

void printStatus() {
  // upper screen part:
  lcd.setCursor(0, 0);
  switch (safeStatus) {
    case start_state:
      lcd.print("STARTING ...    ");
      break;
    case connectingWLAN_state:
      lcd.print("CONNECTING WLAN ");
      break;
    case noPower_state:
      lcd.print("*** NO POWER ***");
      break;
    case locked_state:
      lcd.print("* SAFE LOCKED! *");
      break;
    case lockedAlarm_state:
      lcd.print("* DONT DO THIS *");
      break;
    case wrongSafePassword_state:
      lcd.print("** CODE WRONG **");
      break;
    case openLock_state:
      lcd.print("OPEN THE SAFE ! ");
      break;
    case unlocked_state:
      lcd.print("** SAFE OPEN ***");
      break;
    default:
      lcd.print("ERR printStatus ");
  }
  // lower screen part:
  lcd.setCursor(1, 0);
  switch (safeStatus) {
    case locked_state:
      printSafePassword();
      break;
    case wrongSafePassword_state:
      printSafePassword();
      break;
    case openLock_state:
      printCountdown();
      break;
    default:
      // print nothing:
      for (int i = 0; i < SAFE_PW_LENGTH; i++) {
        lcd.setCursor(i, 1);
        lcd.print(" ");
      }
  }
}

void printSafePassword() {
  // print password:
  for (int i = 0; i < SAFE_PW_LENGTH; i++) {
    lcd.setCursor(i, 1);
    if (currentTry[i] == -1) {
      lcd.print("*");
    }
    else {
      lcd.print(currentTry[i]);
    }
  }
}

void printCountdown() {
  currentTime = millis();
  if (currentTime < startTime) {
    // TODO: is this necessary to prevent problems with overflow (restart with 0) ??
    safeStatus = locked_state;
    digitalWrite(LOCKPIN, LOW);
    return;
  }
  countdown = trunc(startTime/1000 + countdownStart - currentTime/1000);
  if (countdown <= 0) {
    digitalWrite(LOCKPIN, LOW);
    safeStatus = locked_state;
    setColor(LED_COLOR_ORANGE, LED_MODE_ON);
    initArray();
    printStatus();
    disguardACCvalues = true;
    disguardACCvalues_start = millis();
    return;
  }
  lcd.setCursor(0, 1);
  lcd.print(countdown, DEC);
  for(int i = SAFE_PW_LENGTH; i > 0; i--) {
    // clear the digits after the number (parts from the password or old digits from the countdown can still be there)
    lcd.print(" ");
  }
}

void initPWM() {
  // init pwm
  ledcSetup(channel, freq, resolution);
  ledcAttachPin(BUZZER_PIN, channel);
  ledcWrite(channel, 0);
  /* Initialise the sensor */
  if (!accel.begin()) {
    /* There was a problem detecting the ADXL345 ... check your connections */
    Serial.println("Ooops, no LSM303 detected ... Check your wiring!");
  }
  accel.setRange(LSM303_RANGE_2G);
  Serial.print("Range set to: ");
  lsm303_accel_range_t new_range = accel.getRange();
  switch (new_range) {
  case LSM303_RANGE_2G:
    Serial.println("+- 2G");
    break;
  case LSM303_RANGE_4G:
    Serial.println("+- 4G");
    break;
  case LSM303_RANGE_8G:
    Serial.println("+- 8G");
    break;
  case LSM303_RANGE_16G:
    Serial.println("+- 16G");
    break;
  }

  accel.setMode(LSM303_MODE_NORMAL);
  Serial.print("Mode set to: ");
  lsm303_accel_mode_t new_mode = accel.getMode();
  switch (new_mode) {
  case LSM303_MODE_NORMAL:
    Serial.println("Normal");
    break;
  case LSM303_MODE_LOW_POWER:
    Serial.println("Low Power");
    break;
  case LSM303_MODE_HIGH_RESOLUTION:
    Serial.println("High Resolution");
    break;
  }
}

void checkPiezo() {
  /* Get a new sensor event */
  sensors_event_t event;
  accel.getEvent(&event);
  double vec = sqrt((event.acceleration.x)*(event.acceleration.x) + (event.acceleration.y)*(event.acceleration.y) + (event.acceleration.z)*(event.acceleration.z));
  if((safeStatus == locked_state) and (vec > ACC_SENSOR_THRESHOLD)){
    Serial.println(vec);
    if (disguardACCvalues) {
      // this is used to prevent triggering the sensor when the lock is closed
      disguardACCvalues = false;
      Serial.println("accerleration sensor value disguarded");
      return;
    }
    safeStatus = lockedAlarm_state;
    setColor(LED_COLOR_RED, LED_MODE_BLINK);
    printStatus();
    piezo_time = millis();
    ledcWrite(channel, 125);
    ledcWriteTone(channel, freq);
  }
}

void initLEDstripe() {
  pixels.begin();
  pixels.clear();
    for(int i=0;i<NUMPIXELS;i++){
    pixels.setPixelColor(i, pixels.Color(255, 200, 50));
    pixels.show(); // This sends the updated pixel color to the hardware.
  }
}

void setLEDstripe() {
 for(int i=0;i<NUMPIXELS;i++){
    pixels.setPixelColor(i, pixels.Color(255, 200, 50));
    pixels.show(); // This sends the updated pixel color to the hardware.
  }
 for(int i = MIN_BRIGHT_DEACT; i < MAX_BRIGHT_DEACT;i+= 2){
  pixels.setBrightness(i);
 pixels.show(); // This sends the updated pixel color to the hardware.
  delay(100);
 }
 for(int i = MAX_BRIGHT_DEACT; i > MIN_BRIGHT_DEACT;i-= 2){
  pixels.setBrightness(i);
  pixels.show(); // This sends the updated pixel color to the hardware.
 }
  //pixels.clear();
}

void initOTA() {
  safeStatus = connectingWLAN_state;
  printStatus();
  Serial.println("Booting");
  WiFi.mode(WIFI_STA);
  char ssid[30];
  char wlan_password[30];
  preferences.begin("wifi", false); 
  preferences.getString(key_pwd, wlan_password, 30);
  preferences.getString(key_ssid, ssid, 30);
  preferences.end();
  WiFi.begin(ssid, wlan_password);
  int x = millis();
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    char key = keypad.getKey();
    if ((key != NO_KEY) and (key == '*' or key == '#')) {
      connectWLAN = false;
      return;
    }
    if (x + 5000 < millis()) {
      ESP.restart();
    }
  }
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
