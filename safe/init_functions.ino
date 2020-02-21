/*
 * 
 */

void setup_vars(){
  flag_finished = 0;
  led_asc = 0;
  piezo_time_mqtt = 3;
  piezo_time = 0;
  lastMsg = 0;
  safeStatus = noPower_state;
  printStatus();
  countdown = countdownStart;
  initPWM();
  // initLEDstripe();
  led_mode = 0;
  delay_led = 0;
  led_brightness = 60;
  setRgbColor(LED_COLOR_RED, LED_MODE_ON);
  delay(10);
  startTime_PS = millis();
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

void initLEDstripe() {
  pixels.Begin();
  for(int i=0;i<NUMPIXELS;i++){
    HtmlColor col = HtmlColor(RgbColor(255, 200, 50));
    pixels.SetPixelColor(i, col);
    pixels.Show(); // This sends the updated pixel RgbColor to the hardware.
  }
}

void initWifi() {
  safeStatus = connectingWLAN_state;
  printStatus();
  Serial.println("Booting");
  WiFi.mode(WIFI_STA);
  // get ssid and password for wifi connection:
  char ssid[30];
  char wlan_password[30];
  preferences.begin("wifi", false);
  preferences.getString(key_pwd, wlan_password, 30);
  preferences.getString(key_ssid, ssid, 30);
  preferences.end();
  // connecting with wifi:
  WiFi.begin(ssid, wlan_password);
  int startTimeWifi = millis();
  bool retry = true;
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    // wait for wifi connection
    char key = keypad.getKey();
    if ((key != NO_KEY) and (key == '*' or key == '#')) {
      // entering debug mode -> no wifi connection
      Serial.println("entering debug mode!");
      debugMode = true;
      return;
    }
    if ((startTimeWifi + 3500 < millis()) and retry) {
      // for some reason the first try to connect to the wifi does not work
      // -> try again after 3.5 seconds
      WiFi.begin(ssid, wlan_password);
      retry = false;
    }
    if (startTimeWifi + 5000 < millis()) {
      // failed to connect to wifi within 5 seconds -> restart ESP
      ESP.restart();
    }
  }
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void initOTA() {
  // this function is implementing the 'update over the air' functionality
  ArduinoOTA.setHostname("5/safe_control");
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

char readSafeCode() {
  char tempCode[30];
  preferences.begin("wifi", false);
  preferences.getString("safeCode", tempCode, 30);
  preferences.end();
  safeCodeLength = strlen(tempCode);
  memcpy(safeCode, tempCode, safeCodeLength);
  initArray();
}

void initArray() {
  for (int i = 0; i < safeCodeLength; i++) {
    currentTry[i] = -1;
  }
}
