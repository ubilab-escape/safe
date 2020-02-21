/*
 * This code implements the control for a safe inside an escape room which includes:
 *   - reading the credentials from the EEPROM
 *   - connecting to wifi
 *   - using update over the air (OTA) -> code can be changed over wifi
 *   - implementing MQTT to send and receive messages (using JSON)
 *   - reading values from a keaypad (keys: 1, ..., 9, *, #)
 *   - printing messages to a LCD screen (using I2C)
 *   - reading values from an acceleration sensor using I2C
 *   - reading values from a switch
 *   - control a lock
 *   - control a LED stripe
 *   - control a speaker
 *    
 * The code for the keypad was taken from https://www.adafruit.com/product/3845 (25.11.19)
 * The code for the LCD was taken from https://starthardware.org/lcd/ (4.12.19)
 * The code for the I2C LCD was taken from https://funduino.de/nr-19-i%C2%B2c-display (15.12.19)
 * The code for the OTA update was taken from the examples: File -> Examples -> ArduinoOTA -> BasicOTA
 * 
 * All functions used in 'setup' are in the 'init_functions.ino' file.
 * 
 * The credentials for the network as well as the password for the safe have to be stored in
 * the EEPROM. This can be accomplished easily by entering the credentials in 'safe_network.ino'
 * and uploading the code once to the ESP. 
 */

 
#include "Keypad.h"              // keypad library by Mark Stanley, Alexander Brevig
#include <LiquidCrystal.h>
#include <LiquidCrystal_I2C.h>   // I2C library by Frank de Brabander (does result in a warning while compiling but works fine)
// acceleration sensor: libraries needed: Adafruit 9DOF; Adafruit ADXL343; Adafruit AHRS; Adafruit BusIO;
//                                        Adafruit Circuit Playground; Adafruit LSM303 Accel
#include <Adafruit_LSM303_Accel.h>
#include "Adafruit_Sensor.h"
#include <Wire.h>
#include <NeoPixelBrightnessBus.h>  // "NeoPixelBus by Makuna" by Micheal C. Miller
// libraries for ESP OTA update:
#include <WiFi.h>
#include "esp_wifi.h"
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>     // library for MQTT
#include <ArduinoJson.h>
#include <Preferences.h>

#define WLAN_enable true
#define USE_I2C_LCD
#define countdownStart 6
#define WRONG_CODE_DISPLAY_TIME 1000  // time in ms while the message "wrong safe code" is shown
#define reopenDelay    5         // delay in s
#define NUMPIXELS      32
#define MIN_BRIGHT_DEACT 30
#define MAX_BRIGHT_DEACT 120
#define MQTTport 1883
#define thisTopicName "5/safe/control"
#define ACC_SENSOR_THRESHOLD 12

#define LED_COLOR_WHITE 0
#define LED_COLOR_RED 1
#define LED_COLOR_GREEN 2
#define LED_COLOR_BLUE 3
#define LED_COLOR_ORANGE 4

#define LED_MODE_ON 0
#define LED_MODE_OFF 1
#define LED_MODE_PULSE 2
#define LED_MODE_BLINK 3

#define SWITCH_PIN 32
#define LOCKPIN 18
#define LED_PIN 2
#define BUZZER_PIN 4

bool debugMode = !WLAN_enable;
bool flag_finished = 0;

// Wifi, MQTT:
Preferences preferences;
const char* key_ssid = "ssid";
const char* key_pwd = "pass";

const char* MQTT_BROKER = "10.0.0.2";

WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;
char msg[50];

// safe code:
const char* key_safeCode = "safeCode";
char safeCode[30];
int currentTry[30];
int safeCodeLength;

// keypad:
const byte ROWS = 4; //four rows
const byte COLS = 3; //three columns

char keys[ROWS][COLS] = {
  {'1', '2', '3'},
  {'4', '5', '6'},
  {'7', '8', '9'},
  {'*', '0', '#'}
};

byte rowPins[ROWS] = {12, 33, 25, 27}; //connect to the row pinouts of the keypad
byte colPins[COLS] = {14, 13, 26}; //connect to the column pinouts of the keypad
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// LCD:
#ifdef USE_I2C_LCD
  LiquidCrystal_I2C lcd(0x27, 16, 2);  // GPIO 21 = SDA; GPIO 22 = SCL (VCC = 5V)
#else
  const int en = 15, rs = 2, d4 = 4, d5 = 5, d6 = 18, d7 = 19;
  LiquidCrystal lcd(rs, en, d4, d5, d6, d7);
#endif

// LED stripe:
int led_mode = 0;
uint32_t delay_led = 0;
int led_brightness = 60;
bool led_asc = 0;
uint8_t piezo_time_mqtt = 3;
StaticJsonDocument<300> mqtt_decoder;
NeoPixelBrightnessBus<NeoGrbFeature, Neo800KbpsMethod> pixels(NUMPIXELS, LED_PIN);

// safe status:
enum safeStatusEnum {start_state, connectingWLAN_state, noPower_state, locked_state, lockedAlarm_state,
                     wrongSafePassword_state, openLock_state, unlocked_state, skippedLocked_state, reopenDelay_state};
enum safeStatusEnum safeStatus = start_state;

// timer:
unsigned long startTime, currentTime;
unsigned long startTime_PS;

int countdown = countdownStart;
bool piezo_controlled_by_mqtt = false;

// speaker:
const int freq = 3000;
const int freq2 = 4000;
const int channel = 0;
const int resolution = 8;
long piezo_time = 0;

// acceleration sensor:
Adafruit_LSM303_Accel_Unified accel = Adafruit_LSM303_Accel_Unified(54321);   // Assign a unique ID to the acceleration sensor

bool disguardACCvalues = false;
int disguardACCvalues_start;
int disguardACCvalues_time = 100;

void setup() {
  Serial.begin(115200);
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
  if (!debugMode) {
    initWifi();
    initOTA();
    client.setServer(MQTT_BROKER, MQTTport);
    client.setCallback(callback);
  }
  readSafeCode();
  pixels.Begin();
  setup_vars();
}

void loop() {
  if (safeStatus != openLock_state) {
    // just to be sure that the lock is not open whenever it is not supposed to to prevent self destruction
    digitalWrite(LOCKPIN, LOW);
  }
  while (!client.connected() and !debugMode) {
    // in case of a disconnect try to reconnect
    Serial.println("no connection");
    client.connect("SafeDevice_M");
    client.subscribe(thisTopicName);
  }
  client.loop();                                          // TODO: dont do this in debugMode?
  if (!debugMode) {
    if (WiFi.waitForConnectResult() != WL_CONNECTED) {    // TODO: better solution? problem: ESP loses the current state if the connection is lost!
      initWifi();
      initOTA();
    }
    ArduinoOTA.handle();
  }
  action();     // check state and execute corresponding actions
  printStatus();
  checkPiezo();
  piezoControl_for_mqtt();
  updateLED();
  
  if (millis() - startTime_PS > 10000) {
    // reset power state every 10 seconds to prevent power saving and improve the ping of the ESP significantly
    // Serial.println(MDNS.begin("ESP WIFI"));    // returns "1" if Wifi is working; good for debugging
    esp_wifi_set_ps(WIFI_PS_NONE);
    startTime_PS = millis();
  }
}

void updateLED() {
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
      pixels.SetBrightness(led_brightness);
      pixels.Show();
      Serial.print("Brighntess set to ");
      Serial.println(led_brightness);
    }
  } else if (led_mode == LED_MODE_BLINK) {
    if(millis() - delay_led > 500){
      delay_led = millis();
      if(led_asc){
        pixels.SetBrightness(20);
        pixels.Show();
        Serial.println("led blink on");
      }else {
        pixels.SetBrightness(MAX_BRIGHT_DEACT);
        pixels.Show();
        Serial.println("led blink off");
      }
      if(led_asc){
        led_asc = false;
      } else {
        led_asc = true;
      }
    }
  }
}

void setRgbColor(int RgbColorCode, int setMode){
  HtmlColor col;
  switch(RgbColorCode){
    case LED_COLOR_WHITE: // white
      col = HtmlColor(RgbColor(255, 255, 255));
    break;
    case LED_COLOR_RED: // red
      col = HtmlColor(RgbColor(255, 0, 0));
    break;
    case LED_COLOR_GREEN: // green
      col = HtmlColor(RgbColor(0, 255, 0));
    break;
    case LED_COLOR_BLUE: // blue
      col = HtmlColor(RgbColor(0, 0, 255));
    break;
    case LED_COLOR_ORANGE:
      col = HtmlColor(RgbColor(255, 128, 0));
    break;
  }
  for(int i=0;i<NUMPIXELS;i++){
    pixels.SetPixelColor(i, col);
    pixels.Show(); // This sends the updated pixel RgbColor to the hardware.
  }
  led_mode = setMode;
  switch(setMode){
    case 0:
      pixels.SetBrightness(MAX_BRIGHT_DEACT);
      pixels.Show();
    break;
    case 1:
      col = HtmlColor(RgbColor(0, 0, 0));
        for(int i=0;i<NUMPIXELS;i++){
          pixels.SetPixelColor(i, col);
          pixels.Show(); // This sends the updated pixel RgbColor to the hardware.
        }
    break;
    case 2:
      led_brightness = 60;
      delay_led = 0;
    break;
    case 3:
      pixels.SetBrightness(MAX_BRIGHT_DEACT);
      pixels.Show();
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
    if(strcmp(method_msg, "trigger") == 0 && strcmp(state_msg, "on") == 0 && data_msg != NULL && (unsigned)strlen(data_msg) >= 1){
      //change led
      if((unsigned)strlen(data_msg) >= 2) {
        int led_col = data_msg[0] - 48;
        int led_st = data_msg[2] - 48;
        if(led_col >= 0 && led_col <= 9 && led_st >= 0 && led_st <= 8) {
          setRgbColor(led_col, led_st);
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

    if(strcmp(topic, "5/safe/control") == 0 && strcmp(method_msg, "trigger") == 0 && strcmp(state_msg, "on") == 0 && ( data_msg == NULL || (unsigned)strlen(data_msg) == 0)){
      lock();
      setRgbColor(LED_COLOR_ORANGE, LED_MODE_ON);
      printStatus();
      client.publish(thisTopicName, createJson("status", "active", ""), true);
    } 
    if(strcmp(topic, "5/safe/control") == 0 && strcmp(method_msg, "trigger") == 0 && strcmp(state_msg, "off") == 0 && ( data_msg == NULL || (unsigned)strlen(data_msg) == 0)){
       if(digitalRead(SWITCH_PIN) == 0){
        setup_vars();
        client.publish(thisTopicName, createJson("status", "inactive", ""), true);
      } else {
        client.publish(thisTopicName, createJson("status", "failed", "safe is still open, can't reset"), true);
      }
    } else if(strcmp(topic, "5/safe/control") == 0 && strcmp(method_msg, "trigger") == 0 && strcmp(state_msg, "off") == 0 && strcmp(data_msg, "skipped") == 0){
      safeStatus = openLock_state;
      setRgbColor(LED_COLOR_GREEN, LED_MODE_BLINK); //green lock
      countdown = countdownStart;
      startTime = millis();
      initArray();
      printStatus();
    }
}

void action() {
  switch (safeStatus) {
    case noPower_state:
    {
      if (debugMode) {
        char key2 = keypad.getKey();
        if ((key2 != NO_KEY) and (key2 == '#' or key2 == '*')) {
          Serial.println("going into no WLAN state");
          lock();
        }
      }
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
        if(!flag_finished){
          setRgbColor(LED_COLOR_ORANGE, LED_MODE_ON);  // orange again
        }
      }
    }
    break;
    case locked_state:
    {
      char key = keypad.getKey();
      if (key != NO_KEY) {
        append(key);
      }
    }
    break;
    case wrongSafePassword_state:
    {
      currentTime = millis();
      if (currentTime - startTime >= WRONG_CODE_DISPLAY_TIME) {
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
        startTime = millis();
        digitalWrite(LOCKPIN, LOW);
        client.publish(thisTopicName, createJson("status", "solved", ""), true);
        setRgbColor(LED_COLOR_GREEN, LED_MODE_ON); // Green on
        flag_finished = 1;
      }
      initArray();
    }
    break;
    case unlocked_state:
    {
      int switchValue2 = digitalRead(SWITCH_PIN);
      currentTime = millis();
      if (switchValue2 == 0) {
        if (currentTime - startTime < reopenDelay * 1000) {
          safeStatus = reopenDelay_state;
        } else {
          safeStatus = skippedLocked_state;
        }
      }
    }
    break;
    case skippedLocked_state:
    {
      char key = keypad.getKey();
      if (key != NO_KEY) {
        safeStatus = openLock_state;
        countdown = countdownStart;
        startTime = millis();
        initArray();
        printStatus();
      }
      if (disguardACCvalues and (millis() - disguardACCvalues_start  > disguardACCvalues_time)) {
        disguardACCvalues = false;
      }
    }
    break;
    case reopenDelay_state:
      currentTime = millis();
      if (currentTime - startTime > reopenDelay * 1000) {
        safeStatus = skippedLocked_state;
      }
      countdown = trunc(startTime/1000 + reopenDelay - currentTime/1000);
      lcd.setCursor(0, 1);
      lcd.print(countdown, DEC);
      for(int i = 16; i > 0; i--) {
        // clear the digits after the number
        lcd.print(" ");
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
  while (i < safeCodeLength) {
    if (currentTry[i] == -1) {
      currentTry[i] = inp;
      break;
    }
    i++;
  }
  if (i < safeCodeLength - 1) {
    // not enough numbers typed in
    printSafePassword();
    return;
  }
  // check if Code is correct:
  checkSafePassword();
}

void lock() {
  int switchValue3 = digitalRead(SWITCH_PIN);
  if (switchValue3 == 0 or debugMode) {
    initArray();
    safeStatus = locked_state;
    printStatus();
    if(!flag_finished) {
      setRgbColor(LED_COLOR_ORANGE, LED_COLOR_ORANGE);
    }
  }
}

void wrongSafePassword() {
  printSafePassword();
  safeStatus = wrongSafePassword_state;
  setRgbColor(LED_COLOR_RED, LED_MODE_BLINK); //red blink
  printStatus();
  startTime = millis();
}

void checkSafePassword() {
  for (int i = safeCodeLength - 1; i >= 0; i--) {
    if (currentTry[i] != (safeCode[i] - 48)) {
      // safe code not correct:
      wrongSafePassword();
      return;
    }
  }
  // safe code correct:
  safeStatus = openLock_state;
  setRgbColor(LED_COLOR_GREEN, LED_MODE_BLINK); //green lock
  countdown = countdownStart;
  startTime = millis();
  initArray();
  printStatus();
}

void printStatus() {
  // upper screen part:
  // clear display and sets cursor to 0, 0
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
    case skippedLocked_state:
      lcd.print("UNLOCKED: PRESS ");
      break;
    case reopenDelay_state:
      lcd.print("ONE MOMENT ...  ");
      break;
    default:
      lcd.print("ERR printStatus ");
  }
  // lower screen part:
  lcd.setCursor(0, 1);
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
    case skippedLocked_state:
      lcd.print("ANY KEY TO OPEN!");
      break;
    case reopenDelay_state:
      break;
    default:
      // print nothing:
      for (int i = 0; i < 16; i++) {
        lcd.setCursor(i, 1);
        lcd.print(" ");
      }
  }
}

void printSafePassword() {
  // print password:
  for (int i = 0; i < safeCodeLength; i++) {
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
    startTime = millis();
    safeStatus = reopenDelay_state;
    initArray();
    printStatus();
    disguardACCvalues = true;
    disguardACCvalues_start = millis();
    return;
  }
  lcd.setCursor(0, 1);
  lcd.print(countdown, DEC);
  for(int i = 16; i > 0; i--) {
    // clear the digits after the number (parts from the password or old digits from the countdown can still be there)
    lcd.print(" ");
  }
}

void checkPiezo() {
  /* Get a new sensor event */
  sensors_event_t event;
  accel.getEvent(&event);
  double vec = sqrt((event.acceleration.x)*(event.acceleration.x) + (event.acceleration.y)*(event.acceleration.y) + (event.acceleration.z)*(event.acceleration.z));
  if(safeStatus == locked_state and (vec > ACC_SENSOR_THRESHOLD)){
    Serial.println(vec);
    if (disguardACCvalues) {
      // this is used to prevent triggering the sensor when the lock is closed
      disguardACCvalues = false;
      Serial.println("accerleration sensor value disguarded");
      return;
    }
    safeStatus = lockedAlarm_state;
    if(!flag_finished) {
      setRgbColor(LED_COLOR_RED, LED_MODE_BLINK);
    }
    printStatus();
    piezo_time = millis();
    ledcWrite(channel, 125);
    ledcWriteTone(channel, freq);
  }
}
