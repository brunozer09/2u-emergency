// Monitor de bateria com INA219: tensão, corrente, potência e estado de carga.
#pragma once

#include <Adafruit_INA219.h>
#include <stdint.h>

#include "ProjectConfig.h"

enum class BatteryState : uint8_t { kUnknown, kOk, kLow, kCritical };

struct BatteryData {
  float voltageV = 0.0f;
  float currentMa = 0.0f;  // Positiva na descarga, se ligado como em CLAUDE.md
  float powerMw = 0.0f;
  bool voltageValid = false;
  bool currentValid = false;
  bool powerValid = false;
  BatteryState state = BatteryState::kUnknown;
};

class BatteryMonitor {
 public:
  bool begin();
  void sample();

  const BatteryData& data() const { return data_; }

 private:
  Adafruit_INA219 ina219_{config::kBatteryMonitorAddress};
  BatteryData data_;
};
