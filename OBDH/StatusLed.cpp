#include "StatusLed.h"

#include <Arduino.h>

#include "ProjectConfig.h"

void StatusLed::begin() {
  digitalWrite(config::kLedPin, LOW);
  pinMode(config::kLedPin, OUTPUT);
}

void StatusLed::blink() {
  digitalWrite(config::kLedPin, HIGH);
  onSinceMs_ = millis();
  isOn_ = true;
}

void StatusLed::update() {
  if (isOn_ && millis() - onSinceMs_ >= config::kLedBlinkMs) {
    digitalWrite(config::kLedPin, LOW);
    isOn_ = false;
  }
}
