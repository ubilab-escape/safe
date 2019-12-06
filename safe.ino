// TODO: avoid the 'delay' function

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

enum safeStatusEnum {noPower, locked, wrongPW, opened};
enum safeStatusEnum safeStatus = noPower;

void setup() {
  Serial.begin(9600);
  pinMode(LED, OUTPUT);
  digitalWrite(LED, LOW);
  lcd.begin(16, 2);
  initArray();
  printStatus();
  printPW();
}

void loop() {
  char key = keypad.getKey();
  if (key != NO_KEY) {
    //Serial.println(key);
    if (safeStatus == noPower) {
      if (key == '#' or key == '*') {
        safeStatus = locked;
        printStatus();
      }
    }
    else if (append(key) == true) {
      safeStatus = opened;
      printStatus();
      while (true) {
        char key = keypad.getKey();
        if (key != NO_KEY) {
          if (key == '#' or key == '*') {
            safeStatus = locked;
            printStatus();
            printPW();
            break;
          }
        }
      }
    }
  }
}

bool append(char inpChar) {
  if (inpChar == '*' or inpChar == '#') {
    // prevent those symbols to avoid bugs
    return false;
  }
  int inp = inpChar - 48;
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
    printPW();
    return false;
  }
  // check if Code is correct:
  for (int i = codeLength - 1; i >= 0; i--) {
    if (currentTry[i] != password[i]) {
      // password not correct:
      printPW();
      safeStatus = wrongPW;
      printStatus();
      delay(1000);
      safeStatus = locked;
      printStatus();
      initArray();
      printPW();
      return false;
    }
  }
  printPW();
  initArray();
  return true;
}

void initArray () {
  for (int i = 0; i < codeLength; i++) {
    currentTry[i] = -1;
  }
}

void printPW() {
  for (int i = 0; i < codeLength; i++) {
    lcd.setCursor(i, 1);
    if (safeStatus == noPower) {
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

void printStatus() {
  lcd.setCursor(0, 0);
  switch (safeStatus) {
    case noPower:
      lcd.print("*** NO POWER ***");
      break;
    case locked:
      lcd.print("* SAFE LOCKED **");
      break;
    case wrongPW:
      lcd.print("*PASSWORD WRONG*");
      break;
    case opened:
      lcd.print("** SAFE OPEN ***");
      break;
    default:
      lcd.print("ERR printStatus ");
  }
}
