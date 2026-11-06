// Barômetro BMP280/BME280: detecta o chip pelo Chip ID e guarda a última leitura.
#pragma once

#include <Adafruit_BME280.h>
#include <Adafruit_BMP280.h>
#include <stdint.h>

struct BarometerData {
  float temperatureC = 0.0f;
  float pressureHpa = 0.0f;
  float humidityPct = 0.0f;
  bool temperatureValid = false;
  bool pressureValid = false;
  bool humidityValid = false;  // Sempre falso no BMP280, que não mede umidade
};

class Barometer {
 public:
  bool begin();
  void sample();

  const BarometerData& data() const { return data_; }
  uint8_t address() const { return address_; }
  uint8_t chipId() const { return chipId_; }

 private:
  enum class Chip : uint8_t { kNone, kBmp280, kBme280 };

  static bool readChipId(uint8_t address, uint8_t& chipId);

  Adafruit_BMP280 bmp_;
  Adafruit_BME280 bme_;
  BarometerData data_;
  Chip chip_ = Chip::kNone;
  uint8_t address_ = 0;
  uint8_t chipId_ = 0;
};
