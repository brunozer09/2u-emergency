#include "Buzzer.h"

#include <Arduino.h>

namespace {
constexpr uint8_t kLedcResolutionBits = 10;
}  // namespace

void Buzzer::begin() {
  if (config::kBuzzerIsPassive) {
    ledcAttach(config::kBuzzerPin, config::kBuzzerToneHz, kLedcResolutionBits);
    ledcWriteTone(config::kBuzzerPin, 0);
  } else {
    digitalWrite(config::kBuzzerPin, LOW);
    pinMode(config::kBuzzerPin, OUTPUT);
  }
}

void Buzzer::requestBeep() {
  requested_ = true;
}

void Buzzer::update() {
  const uint32_t nowMs = millis();

  if (isSounding_) {
    if (nowMs - lastBeepMs_ >= config::kBuzzerBeepMs) {
      setSound(false);
    }
    return;
  }

  if (requested_ && nowMs - lastBeepMs_ >= config::kBuzzerMinIntervalMs) {
    requested_ = false;
    lastBeepMs_ = nowMs;
    setSound(true);
  }
}

void Buzzer::setSound(bool on) {
  if (config::kBuzzerIsPassive) {
    ledcWriteTone(config::kBuzzerPin, on ? config::kBuzzerToneHz : 0);
  } else {
    digitalWrite(config::kBuzzerPin, on ? HIGH : LOW);
  }
  isSounding_ = on;
}
