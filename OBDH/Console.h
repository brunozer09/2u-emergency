// Mensagens de estado pela USB (Serial), com o mesmo prefixo '#' usado pelo TTeC.
#pragma once

#include <Arduino.h>

class Console {
 public:
  explicit Console(HardwareSerial& serial) : serial_(serial) {}

  void begin();
  void log(const char* format, ...) __attribute__((format(printf, 2, 3)));

 private:
  static constexpr size_t kLogBytes = 128;

  HardwareSerial& serial_;
};
