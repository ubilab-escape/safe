/* 
 * TODO:
 * implement connection with server status changes: noPower -> locked, unlocked -> locked??,
 * reset (go back to "noPower_state");
 * implement other functionality (LEDs, vibration sensor, (speaker ??), lock, sensors from
 * the prototype group;
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
#include <Adafruit_Sensor.h>
#include <Wire.h>
#include <Adafruit_NeoPixel.h>  // library for LED stripe
// libraries for ESP OTA update:
#include <WiFi.h>
#include "esp_wifi.h"
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>     // library for MQTT

#define USE_ESP32
#define WLAN_enable true
#define CODE_LENGTH 4                    // <-------------------------- safe code LENGTH
#define USE_I2C_LCD
#define countdownStart 7
#define NUMPIXELS      32
#define MIN_BRIGHT_DEACT 30
#define MAX_BRIGHT_DEACT 120
#define MQTTport 1883
#define thisTopicName "mqtt_5_safe_control"
#define activateTopicName "mqtt_5_safe_activate"

const char* ssid = "Darknet";
const char* wlan_password = "123456789";
const char* MQTT_BROKER = "........";

bool connectWLAN = WLAN_enable;

WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;
char msg[50];
int value = 0;

// messages:
char* puzzleSolved_message = "{\"method\": \"STATUS\", \"state\": \"solved\"}";

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

const int lockPin = 18;

// ----------------------------------------------------------------------------------------------
// safe code:
int safePassword[CODE_LENGTH] = {4, 2, 4, 2};     // <-------------------------- safe code
int currentTry[CODE_LENGTH] = {};
// safe status:
enum safeStatusEnum {start_state, connectingWLAN_state, noPower_state, locked_state, lockedAlarm_state, wrongSafePassword_state, openLock_state, unlocked_state};
enum safeStatusEnum safeStatus = start_state;

// timer:
const int messageLength = 1000;  // time while the message "wrong safe code" is shown
unsigned long startTime, currentTime;

int countdown = countdownStart;

// speaker:
const int freq = 3000;
const int freq2 = 4000;
const int channel = 0;
const int resolution = 8;
// Assign a unique ID to this sensor at the same time
Adafruit_LSM303_Accel_Unified accel = Adafruit_LSM303_Accel_Unified(54321);

// LED stripe:
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NUMPIXELS, 2, NEO_GRB + NEO_KHZ800);

void setup() {
  Serial.begin(115200);
  //Serial.begin(9600);
  pinMode(lockPin, OUTPUT);
  digitalWrite(lockPin, LOW);
  pinMode(15, OUTPUT);
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
  initArray();
  safeStatus = noPower_state;
  printStatus();
  countdown = countdownStart;
  initPWM();
  //initLEDstripe();
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
    safeStatus = locked_state;
    printStatus();
}

long piezo_time = 0;
bool piezo_started = 0;
int count_num = 0;

void loop() {
  esp_wifi_set_ps(WIFI_PS_NONE);          // TODO: only every 10s or so ...
  while (!client.connected()){
      Serial.println("no connection");
      client.connect("SafeDevice_M");
      client.subscribe(activateTopicName);
    }
  client.loop();
  
  if (safeStatus == wrongSafePassword_state) {
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
  else if(safeStatus == openLock_state) {
    // TODO: check here if safe was opened:
    char key = keypad.getKey();       // TODO: remove
    if (key != NO_KEY) {
      if (key == '*' or key == '#') {
        safeStatus = unlocked_state;
        client.publish(thisTopicName, puzzleSolved_message);
      }
    }
    initArray();
    printStatus();
  }
  else if (safeStatus == locked_state) {
    char key = keypad.getKey();
    if (key != NO_KEY) {
      append(key);
    }
  }
  else if (safeStatus == noPower_state) {
    if (!WLAN_enable) {
      char key = keypad.getKey();       // TODO: remove and implement MQTT here
      if (key != NO_KEY) {
        if (key == '#' or key == '*') {
          lock();
        }
      }
    }
  }
  if (safeStatus == unlocked_state) {  // TODO: remove if not needed anymore
    char key = keypad.getKey();
    if (key == '#' or key == '*') {
      lock();
    }
  }
  if (safeStatus == openLock_state) {
    digitalWrite(lockPin, HIGH);
  }
  else {
    digitalWrite(lockPin, LOW);
  }
  if (connectWLAN) {
    if (WiFi.waitForConnectResult() != WL_CONNECTED) {    // TODO: better solution? problem: ESP loses the current state if the connection is lost!
      initOTA();
    }
    ArduinoOTA.handle();
  }
  sensorEvent();
}

void reset() {
  safeStatus = noPower_state;
  initArray();
  printStatus();
}

void append(char inpChar) {
  if (inpChar == '*' or inpChar == '#') {
    // prevent those symbols to avoid bugs
    return;
  }
  int inp = inpChar - 48;    // char -> int
  int i = 0;
  while (i < CODE_LENGTH) {
    if (currentTry[i] == -1) {
      currentTry[i] = inp;
      break;
    }
    i++;
  }
  if (i < CODE_LENGTH - 1) {
    // not enough numbers typed in
    printSafePassword();
    return;
  }
  // check if Code is correct:
  checkSafePassword();
}

void lock() {
  // TODO: check, if safe is closed
  safeStatus = locked_state;
  initArray();
  printStatus();
}

void wrongSafePassword() {
  printSafePassword();
  safeStatus = wrongSafePassword_state;
  printStatus();
  startTime = millis();
}

void initArray () {
  for (int i = 0; i < CODE_LENGTH; i++) {
    currentTry[i] = -1;
  }
}

void checkSafePassword() {
  for (int i = CODE_LENGTH - 1; i >= 0; i--) {
    if (currentTry[i] != safePassword[i]) {
      // safe code not correct:
      wrongSafePassword();
      return;
    }
  }
  // safe code correct:
  safeStatus = openLock_state;
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
      for (int i = 0; i < CODE_LENGTH; i++) {
        lcd.setCursor(i, 1);
        lcd.print(" ");
    }
  }
}

void printSafePassword() {
  // print password:
  for (int i = 0; i < CODE_LENGTH; i++) {
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
    // TODO: switch lock off!!!
    return;
  }
  countdown = trunc(startTime/1000 + countdownStart - currentTime/1000);
  if (countdown <= 0) {
    safeStatus = locked_state;
    initArray();
    printStatus();
    // TODO: switch lock off!!!
    return;
  }
  lcd.setCursor(0, 1);
  lcd.print(countdown, DEC);
  for(int i = CODE_LENGTH; i > 0; i--) {
    // clear the digits after the number (parts from the password or old digits from the countdown can still be there)
    lcd.print(" ");
  }
}

void initPWM() {
  // init pwm
  ledcSetup(channel, freq, resolution);
  ledcAttachPin(4, channel);
  ledcWrite(channel, 0);
  /* Initialise the sensor */
  if (!accel.begin()) {
    /* There was a problem detecting the ADXL345 ... check your connections */
    Serial.println("Ooops, no LSM303 detected ... Check your wiring!");
    while (1)
      ;
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

void sensorEvent() {
  /* Get a new sensor event */
  sensors_event_t event;
  accel.getEvent(&event);
  double vec = sqrt((event.acceleration.x)*(event.acceleration.x) + (event.acceleration.y)*(event.acceleration.y) + (event.acceleration.z)*(event.acceleration.z));
  Serial.println(vec);
  //if(vec > 20){
  if((safeStatus == locked_state) and (vec > 20)){
     // piezo_started = 1;
     safeStatus = lockedAlarm_state;
     printStatus();
     piezo_time = millis();
     ledcWrite(channel, 125);
     ledcWriteTone(channel, freq);
  }
  //if(piezo_started){
  if(safeStatus == lockedAlarm_state){
    long time_diff = floor((millis() - piezo_time)/100)*100;
    if(time_diff % 1000 == 0) {
      ledcWriteTone(channel, freq);
    } else if(time_diff % 500 == 0){
      ledcWriteTone(channel, freq2);
    }
    if(time_diff > 3000){
      ledcWrite(channel, 0);
      //piezo_started = 0;
      safeStatus = locked_state;
      printStatus();
    }
  }
}

void initLEDstripe() {
  pixels.begin();
  pixels.clear();
    for(int i=0;i<NUMPIXELS;i++){
    pixels.setPixelColor(i, pixels.Color(255, 200, 50));
    pixels.show(); // This sends the updated pixel color to the hardware.
  }
  //delay(100);         // TODO: remove?
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
  //delay(100);       // TODO: remove?
 }
  //pixels.clear();
}

void initOTA() {
  safeStatus = connectingWLAN_state;
  printStatus();
  Serial.println("Booting");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, wlan_password);
  int x = millis();
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    char key = keypad.getKey();
    if (key != NO_KEY) {
      if (key == '*' or key == '#') {
        connectWLAN = false;
        return;
      }
    }
    if (x + 5000 < millis()) {
      ESP.restart();
    }
  }

  // Port defaults to 3232
  // ArduinoOTA.setPort(3232);

  // Hostname defaults to esp3232-[MAC]
  // ArduinoOTA.setHostname("myesp32");

  // No authentication by default
  // ArduinoOTA.setPassword("admin");

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

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
