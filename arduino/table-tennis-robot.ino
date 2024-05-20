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

// encoder velocity
const int encPin = 2;
volatile int pulseCount = 0;
unsigned long previousMillis = 0;
const unsigned long interval = 500;
volatile int lastState = 0;

// PID
int globalRPM = 0;
const int targetRPM = 60;

// Kp, Ki, Kd
const float Kp = 0.75;
const float Ki = 0.2;
const float Kd = 0;

float previousError = 0;
float integral = 0;
float controlOutput = 0;

int topVel = 0;
int backVel = 0;
const int step = 5;
const int minVel = 25;
const int maxVel = 40;
int freqVelocity = 40;

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

  analogWrite(AFreq, 40);
  delay(50);
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

void calculateRPM() {
  int pulses = pulseCount;
  pulseCount = 0;
  // pulses * 60k (pulses in one minute)
  // 23 * interval (23 holes * interval inspected)
  globalRPM = (pulses * 60000) / (23 * interval);
}

void countPulse() {
  int currentState = digitalRead(encPin);
  if (currentState && !lastState) {
    pulseCount++;
  }
  lastState = currentState;
}

void makePID() {
  int error = targetRPM - globalRPM;
  integral += error * (interval / 1000.0); // integral term
  float derivative = (error - previousError) / (interval / 1000.0); // derivative term

  float rawOutput = (Kp * error) + (Ki * integral) + (Kd * derivative); // PID formula
  previousError = error; // Update previous error

  // Smooth the control output with a low-pass filter
  const float alpha = 0.1; // Smoothing factor (0 < alpha <= 1)
  controlOutput = (alpha * rawOutput) + ((1 - alpha) * controlOutput);

  // Limit the rate of change of the frequency velocity
  const float maxChangeRate = 10; // Maximum change rate per interval
  float newFreqVelocity = freqVelocity + constrain(controlOutput, -maxChangeRate, maxChangeRate);

  // Constrain the new frequency velocity to be within allowed limits
  freqVelocity = constrain(newFreqVelocity, 40, 60);

  // Update motor speed
  analogWrite(AFreq, freqVelocity);

  // Print debug information
  Serial.print("Target RPM: ");
  Serial.print(targetRPM);
  Serial.print(" | Current RPM: ");
  Serial.print(globalRPM);
  Serial.print(" | PID Output: ");
  Serial.print(controlOutput);
  Serial.print(" | Frequency Velocity: ");
  Serial.println(freqVelocity);
}

void setup() {
  pinMode(top, OUTPUT);
  pinMode(back, OUTPUT);
  pinMode(AFreq, OUTPUT);
  pinMode(BFreq, OUTPUT);
  pinMode(txPin, OUTPUT);
  pinMode(rxPin, INPUT);
  pinMode(encPin, INPUT);
  BTSerial.begin(9600);
  Serial.begin(9600);
  attachInterrupt(digitalPinToInterrupt(encPin), countPulse, RISING);
}

void loop() {


  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    calculateRPM();
    if (freqState) {
      makePID();
    }
  }

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

