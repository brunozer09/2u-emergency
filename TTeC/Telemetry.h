// Saída serial: monta e transmite as linhas CSV e as mensagens de log (prefixo '#').
#pragma once

#include <Arduino.h>

#include "Barometer.h"
#include "BatteryMonitor.h"
#include "Gps.h"
#include "Imu.h"

class Telemetry {
 public:
  explicit Telemetry(HardwareSerial& serial) : serial_(serial) {}

  void begin();
  void sendHeader();
  void sendSample(const BarometerData& barometer, const ImuData& imu, const GpsData& gps,
                  const BatteryData& battery);
  void log(const char* format, ...) __attribute__((format(printf, 2, 3)));

 private:
  // Maior linha possível: cerca de 170 caracteres com todos os campos preenchidos.
  // O receptor OBDH usa o mesmo limite.
  static constexpr size_t kLineBytes = 192;
  static constexpr size_t kLogBytes = 96;

  void appendUint(uint32_t value, bool valid);
  void appendFloat(double value, uint8_t decimals, bool valid);
  void appendSeparator();
  void appendFormatted(const char* format, ...) __attribute__((format(printf, 2, 3)));

  HardwareSerial& serial_;
  char line_[kLineBytes];
  size_t lineLength_ = 0;
  bool lineOverflow_ = false;
  uint32_t nextId_ = 0;
};
