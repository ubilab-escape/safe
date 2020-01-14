#include <Adafruit_NeoPixel.h>
#include <ArduinoJson.h>

#define PIN            23
#define NUMPIXELS      32
#define MIN_BRIGHT_DEACT 30
#define MAX_BRIGHT_DEACT 120

#define LED_COLOR_WHITE 0
#define LED_COLOR_RED 1
#define LED_COLOR_GREEN 2
#define LED_COLOR_BLUE  3

#define LED_MODE_ON 0
#define LED_MODE_OFF 1
#define LED_MODE_PULSE 2

int led_mode = 0;
uint32_t delay_led = 0;
int led_brightness = 60;
bool led_asc = 0;

StaticJsonDocument<300> mqtt_decoder;

Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

String createJson(String method_s, String state_s, String data_s){
  StaticJsonDocument<300> doc;
  doc["method"] = method_s;
  doc["state"] = state_s;
  doc["data"] = data_s;
  String JSON_String = "";
  serializeJson(doc, JSON_String);
  return JSON_String;
}

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
      pixels.show();
    break;
  }
}

void setup() {
  Serial.begin(9600);
  Serial.println("Started");
  Serial.print(createJson("START", "DU", "PENNER"));
  while(true);
  pixels.begin();
  pixels.clear();
  setColor(LED_COLOR_RED, LED_MODE_ON);
  delay(3000);
  setColor(3, 2);
}

void loop() {
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
  }
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
    
    deserializeJson(mqtt_decoder, msg);
    const char* method_msg = mqtt_decoder["method"];
    const char* state_msg = mqtt_decoder["state"];
    int data_msg = mqtt_decoder["data"];
    if(method_msg == "TRIGGER" && state_msg == "on"){
      //change led
      int led_col = data_msg & 0x0F;
      int led_st = (data_msg & 0xF0) >> 4;
      setColor(led_col, led_st);
    }
    
}
