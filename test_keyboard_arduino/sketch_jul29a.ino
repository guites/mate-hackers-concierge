/*  PS2Keyboard library example
  
  PS2Keyboard now requries both pins specified for begin()

  keyboard.begin(data_pin, irq_pin);
  
  Valid irq pins:
     Arduino Uno:  2, 3
     Arduino Due:  All pins, except 13 (LED)
     Arduino Mega: 2, 3, 18, 19, 20, 21
     Teensy 3.0:   All pins, except 13 (LED)
     Teensy 2.0:   5, 6, 7, 8
     Teensy 1.0:   0, 1, 2, 3, 4, 6, 7, 16
     Teensy++ 2.0: 0, 1, 2, 3, 18, 19, 36, 37
     Teensy++ 1.0: 0, 1, 2, 3, 18, 19, 36, 37
     Sanguino:     2, 10, 11
  
  for more information you can read the original wiki in arduino.cc
  at http://www.arduino.cc/playground/Main/PS2Keyboard
  or http://www.pjrc.com/teensy/td_libs_PS2Keyboard.html
  
  Like the Original library and example this is under LGPL license.
  
  Modified by Cuninganreset@gmail.com on 2010-03-22
  Modified by Paul Stoffregen <paul@pjrc.com> June 2010
*/
   
#include <PS2Keyboard.h>

const int DataPin = 8;
const int IRQpin =  2;

PS2Keyboard keyboard;

// Maps the led color to the arduino digital port
const int blueLedPin = 4;
const int greenLedPin = 5;
const int redLedPin = 6;

// saves the current identification step: "idle", "id", "password"
String inputStep;
// saves the currently typed id
String inputId;
// saves the currently typed password
String inputPassword;

enum LedColor {
  RED,
  GREEN,
  BLUE
};

void setLedState(LedColor color, bool state) {
  int ledPin;
  switch (color) {
    case RED:
      ledPin = redLedPin;
      break;
    case GREEN:
      ledPin = greenLedPin;
      break;
    case BLUE:
      ledPin = blueLedPin;
      break;
    default:
      return; // Invalid color
  }
  digitalWrite(ledPin, state ? LOW : HIGH);
}

void initializeLeds() {
  pinMode(redLedPin, OUTPUT);
  setLedState(RED, false);
  pinMode(greenLedPin, OUTPUT);
  setLedState(GREEN, false);
  pinMode(blueLedPin, OUTPUT);
  setLedState(BLUE, false);
}

void setup() {
  delay(1000);
  keyboard.begin(DataPin, IRQpin);
  Serial.begin(9600);
  Serial.println("Keyboard Test:");
  initializeLeds();

  inputStep = "idle";
  inputId = "";
  inputPassword = "";
}

void feedbackError() {
  Serial.println("BUZZ!!");
  setLedState(RED, true);
  delay(250);
  setLedState(RED, false);
  inputStep = "idle";
}

void feedbackSuccess(int numBlinks) {
  for (int i = 0; i < numBlinks; i++) {
    delay(150);
    setLedState(GREEN, true);
    delay(250);
    setLedState(GREEN, false);
  }
}

void loop() {
  if (keyboard.available()) {

    // read the next key
    char c = keyboard.read();

    if (inputStep == "idle") {
      if (c != PS2_ENTER) {
        feedbackError();
        return;
      }
      Serial.print("[Enter] -- Start Login");
      setLedState(BLUE, true);
      inputStep = "id";
      return;
    }

    if (inputStep == "id") {
      if (c == PS2_ENTER) {
        Serial.print("[Enter]");
        setLedState(BLUE, false);
        if (inputId == "") {
          feedbackError();
          return;
        }
        Serial.print("Received Id: ");
        Serial.println(inputId);
        inputId = "";
        inputStep = "password";
        setLedState(RED, true);
        setLedState(GREEN, true);
        return;
      }
      inputId += c;
    }

    if (inputStep == "password") {
      if (c == PS2_ENTER) {
        Serial.print("[Enter]");
        setLedState(RED, false);
        setLedState(GREEN, false);
        if (inputPassword == "") {
          feedbackError();
          return;
        }
        Serial.print("Received Password: ");
        Serial.println(inputPassword);
        inputPassword = "";
        inputStep = "idle";
        setLedState(RED, false);
        setLedState(GREEN, false);

        feedbackSuccess(8);
        return;
      }
      inputPassword += c;
    }
    

    // else if (c == PS2_ESC) {
    //   Serial.print("[ESC]");
    //   setLedState(BLUE, true);
    // }
    // else {
    //   // otherwise, just print all normal characters
    //   Serial.print(c);
    //   setLedState(GREEN, true);
    // }
    Serial.print("Current state: ");
    Serial.println(inputStep);
  }
}
