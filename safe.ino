/* 
 * TODO:
 * check possible overflow problem;
 * check, if this code works with the ESP32;
 * implement connection with server status changes: noPower -> locked, unlocked -> locked??,
 * reset (go back to "noPower_state");
 * switch to I"C display;
 * implement other functionality (LEDs, vibration sensor, (speaker ??), lock, sensors from
 * the prototype group;
*/
// the code for the keypad was taken from https://www.adafruit.com/product/3845 (25.11.19)
// the code for the LCD was taken from https://starthardware.org/lcd/ (4.12.19)

//#include "Arduino.h"
#include "Keypad.h"   // keypad library by Mark Stanley, ALexander Brevig
#include <LiquidCrystal.h>

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


byte rowPins[ROWS] = {12, 0, 8, 10}; //connect to the row pinouts of the keypad
byte colPins[COLS] = {11, 13, 9}; //connect to the column pinouts of the keypad

Keypad keypad = Keypad( makeKeymap(keys), rowPins, colPins, ROWS, COLS );


// ----------------------------------------------------------------------------------------------
// LCD:
const int en = 7, rs = 2, d4 = 3, d5 = 4, d6 = 5, d7 = 6;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

// ----------------------------------------------------------------------------------------------
// LED:
int LED = 0;
// password:
const int codeLength = 4;                    // <-------------------------- PASSWORD LENGTH
int password[codeLength] = {4, 2, 4, 2};     // <-------------------------- PASSWORD 
int currentTry[codeLength] = {};
// safeS status:
enum safeStatusEnum {noPower_state, locked_state, wrongPassword_state, unlocked_state};
enum safeStatusEnum safeStatus = noPower_state;

// timer:
const int messageLength = 1000;  // time while the message "wrong password" is shown
unsigned long startTime, currentTime;

void setup() {
  Serial.begin(9600);
  pinMode(LED, OUTPUT);
  digitalWrite(LED, LOW);
  lcd.begin(16, 2);
  initArray();
  printStatus();
  printPassword();
}

void loop() {
  char key = keypad.getKey();
  if (safeStatus == wrongPassword_state) {
    currentTime = millis();
    if (currentTime - startTime >= messageLength) {
      lock();
    }
    else if (currentTime < startTime) {
      // TODO: is this necessary to prevent problems with overflow (restart with 0) ??
      // just call function again:
      wrongPassword();
    }
  }
  else if (key != NO_KEY) {
    key_pressed(key);
  }
}

void reset() {
  safeStatus = noPower_state;
  printStatus();
  initArray();
  printPassword();
}

void key_pressed(char key) {
  //Serial.println(key);
  if (safeStatus == noPower_state) {
    if (key == '#' or key == '*') {
      lock();
    }
  }
  else if (safeStatus == unlocked_state) {
    if (key == '#' or key == '*') {
      lock();
    }
  }
  else if (safeStatus == locked_state) {
    append(key);
  }
}

void append(char inpChar) {
  if (inpChar == '*' or inpChar == '#') {
    // prevent those symbols to avoid bugs
    return;
  }
  int inp = inpChar - 48;    // char -> int
  int i = 0;
  while (i < codeLength) {
    if (currentTry[i] == -1) {
      currentTry[i] = inp;
      break;
    }
    i++;
  }
  if (i < codeLength - 1) {
    // not enough numbers typed in
    printPassword();
    return;
  }
  // check if Code is correct:
  checkPassword();
}

void lock() {
  // TODO: check, if safe is closed
  safeStatus = locked_state;
  printStatus();
  initArray();
  printPassword();
}

void wrongPassword() {
  printPassword();
  safeStatus = wrongPassword_state;
  printStatus();
  startTime = millis();
}

void initArray () {
  for (int i = 0; i < codeLength; i++) {
    currentTry[i] = -1;
  }
}

void printPassword() {
  for (int i = 0; i < codeLength; i++) {
    lcd.setCursor(i, 1);
    if (safeStatus == noPower_state) {
      lcd.print(" ");
    }
    else if (currentTry[i] == -1) {
      lcd.print("*");
    }
    else {
      lcd.print(currentTry[i]);
    }
  }
}

void checkPassword() {
  for (int i = codeLength - 1; i >= 0; i--) {
    if (currentTry[i] != password[i]) {
      // password not correct:
      wrongPassword();
      return;
    }
  }
  // password correct:
  safeStatus = unlocked_state;
  printStatus();
  printPassword();
  initArray();
  return;
}

void printStatus() {
  lcd.setCursor(0, 0);
  switch (safeStatus) {
    case noPower_state:
      lcd.print("*** NO POWER ***");
      break;
    case locked_state:
      lcd.print("* SAFE locked **");
      break;
    case wrongPassword_state:
      lcd.print("*PASSWORD WRONG*");
      break;
    case unlocked_state:
      lcd.print("** SAFE OPEN ***");
      break;
    default:
      lcd.print("ERR printStatus ");
  }
}
