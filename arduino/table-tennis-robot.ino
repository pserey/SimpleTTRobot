#include "Arduino.h"
#include <SoftwareSerial.h>

const byte rxPin = 7;
const byte txPin = 3;
SoftwareSerial BTSerial(rxPin, txPin);

// define motor control pins (PWA)
const int back = 10;
const int top = 11; 

// frequency pins (PWA)
const int AFreq = 5;
const int BFreq = 6; 

int topVel = 0;
int backVel = 0;
const int step = 5;
const int minVel = 20;
const int maxVel = 35;
const int freqVelocity = 50;

// variables
int freqState = 0;
// at first implementation, i've decided to use string messages
// ending at ';' to communicate via HC-05
String messageBuffer = "";
String message = "";

int increaseVelocity(int velocity) {
  if (velocity < minVel) {
    return minVel;
  } else {

    if (velocity + step >= maxVel) {
      return maxVel;
    } else {
      return velocity + step;
    }
  }
}

int decreaseVelocity(int velocity) {
  if (velocity - step < minVel) {
    return 0;
  } else {
    return velocity - step;
  }
}

void startFrequency() {
  freqState = 1;
  analogWrite(AFreq, 70);
  delay(100);
  analogWrite(AFreq, freqVelocity);
}

void stopFrequency() {
  freqState = 0;
  analogWrite(AFreq, 0);
}

void unclogFrequency() {
  analogWrite(AFreq, 0);
  analogWrite(BFreq, 50);
  delay(70);
  analogWrite(BFreq, 0);
  startFrequency();
}

void setup() {
  pinMode(top, OUTPUT);
  pinMode(back, OUTPUT);
  pinMode(AFreq, OUTPUT);
  pinMode(BFreq, OUTPUT);
  pinMode(txPin, OUTPUT);
  pinMode(rxPin, INPUT);
  BTSerial.begin(9600);
  Serial.begin(9600);
}

void loop() {

  while (BTSerial.available() > 0) {
    char data = (char) BTSerial.read();
    messageBuffer += data;

    // other spin of frequency always turned off
    analogWrite(BFreq, 0);

    if (data == ';') {
      message = messageBuffer;
      messageBuffer = "";

      if (message == "TOPSPIN+1;") {

        if (!topVel) {
          analogWrite(top, 50);
          delay(100);
        }

        topVel = increaseVelocity(topVel);

        analogWrite(top, topVel);
        Serial.println(topVel);
      } else if (message == "BACKSPIN+1;") {

        if (!backVel) {
          analogWrite(back, 50);
          delay(100);
        }

        backVel = increaseVelocity(backVel);

        analogWrite(back, backVel);
        Serial.println(backVel);
      } else if (message == "STOP_ALL;") {
        stopFrequency();

        backVel = 0;
        topVel = 0;

        analogWrite(top, backVel);
        analogWrite(back, topVel);
        analogWrite(BFreq, 0);
      } else if (message == "BACKSPIN-1;") {

        backVel = decreaseVelocity(backVel);

        analogWrite(back, backVel);
        Serial.println(backVel);
      } else if (message == "TOPSPIN-1;") {

        topVel = decreaseVelocity(topVel);

        analogWrite(top, topVel);
        Serial.println(topVel);
      } else if (message == "FREQ;") {

        if (!freqState) {
          startFrequency();
        } else {
          stopFrequency();
        }
      } else if (message == "UNC;") {
        unclogFrequency();
      }
    }

  }
}

