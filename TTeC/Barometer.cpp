#include "Barometer.h"

#include <Wire.h>
#include <math.h>

#include "ProjectConfig.h"

namespace {
constexpr uint8_t kRegChipId = 0xD0;
constexpr uint8_t kChipIdBmp280 = 0x58;
constexpr uint8_t kChipIdBme280 = 0x60;
constexpr float kPaPerHpa = 100.0f;

// Faixas de operação do datasheet
constexpr float kMinTemperatureC = -40.0f;
constexpr float kMaxTemperatureC = 85.0f;
constexpr float kMinPressureHpa = 300.0f;
constexpr float kMaxPressureHpa = 1100.0f;
constexpr float kMinHumidityPct = 0.0f;
constexpr float kMaxHumidityPct = 100.0f;

// Falso também para NaN
bool isInRange(float value, float minimum, float maximum) {
  return value >= minimum && value <= maximum;
}
}  // namespace

bool Barometer::readChipId(uint8_t address, uint8_t& chipId) {
  Wire.beginTransmission(address);
  Wire.write(kRegChipId);
  if (Wire.endTransmission(false) != 0) {
    return false;
  }
  if (Wire.requestFrom(address, static_cast<uint8_t>(1)) != 1) {
    return false;
  }
  chipId = static_cast<uint8_t>(Wire.read());
  return true;
}

bool Barometer::begin() {
  for (uint8_t address : config::kBarometerAddresses) {
    uint8_t chipId = 0;
    if (!readChipId(address, chipId)) {
      continue;
    }
    address_ = address;
    chipId_ = chipId;

    if (chipId == kChipIdBmp280 && bmp_.begin(address, chipId)) {
      chip_ = Chip::kBmp280;
      return true;
    }
    if (chipId == kChipIdBme280 && bme_.begin(address, &Wire)) {
      chip_ = Chip::kBme280;
      return true;
    }
    return false;  // Respondeu, mas o Chip ID é desconhecido ou a biblioteca falhou
  }
  return false;
}

void Barometer::sample() {
  // As bibliotecas ignoram erros de I2C e calculam valores a partir de lixo, então
  // a presença do sensor é conferida antes e a faixa do datasheet, depois
  uint8_t chipId = 0;
  if (chip_ == Chip::kNone || !readChipId(address_, chipId) || chipId != chipId_) {
    data_.temperatureValid = false;
    data_.pressureValid = false;
    data_.humidityValid = false;
    return;
  }

  float temperature = NAN;
  float pressurePa = NAN;
  float humidity = NAN;

  if (chip_ == Chip::kBmp280) {
    temperature = bmp_.readTemperature();
    pressurePa = bmp_.readPressure();
  } else {
    temperature = bme_.readTemperature();
    pressurePa = bme_.readPressure();
    humidity = bme_.readHumidity();
  }

  data_.temperatureC = temperature;
  data_.pressureHpa = pressurePa / kPaPerHpa;
  data_.humidityPct = humidity;
  data_.temperatureValid = isInRange(temperature, kMinTemperatureC, kMaxTemperatureC);
  data_.pressureValid = isInRange(data_.pressureHpa, kMinPressureHpa, kMaxPressureHpa);
  data_.humidityValid = isInRange(humidity, kMinHumidityPct, kMaxHumidityPct);
}
