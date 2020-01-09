#include <Adafruit_LSM303_Accel.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>

int freq = 3000;
int freq2 = 4000;
int channel = 0;
int resolution = 8;
/* Assign a unique ID to this sensor at the same time */
Adafruit_LSM303_Accel_Unified accel = Adafruit_LSM303_Accel_Unified(54321);

void setup(void) {
#ifndef ESP8266
  while (!Serial)
    ; // will pause Zero, Leonardo, etc until serial console opens
#endif
  Serial.begin(9600);
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

long piezo_time = 0;
bool piezo_started = 0;

void loop(void) {
  /* Get a new sensor event */
  sensors_event_t event;
  accel.getEvent(&event);
  double vec = sqrt((event.acceleration.x)*(event.acceleration.x) + (event.acceleration.y)*(event.acceleration.y) + (event.acceleration.z)*(event.acceleration.z));
  Serial.println(vec);
  if(vec > 20){
     piezo_started = 1;
     piezo_time = millis();
     ledcWrite(channel, 125);
     ledcWriteTone(channel, freq);
  }
  if(piezo_started){
    long time_diff = floor((millis() - piezo_time)/100)*100;
    if(time_diff % 1000 == 0) {
      ledcWriteTone(channel, freq);
    } else if(time_diff % 500 == 0){
      ledcWriteTone(channel, freq2);
    }
    if(time_diff > 3000){
      ledcWrite(channel, 0);
      piezo_started = 0;
    }
  }

}
