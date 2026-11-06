#include "Imu.h"

#include <Arduino.h>
#include <Wire.h>

#include "ProjectConfig.h"

namespace {

// Registradores do MPU
constexpr uint8_t kRegConfig = 0x1A;
constexpr uint8_t kRegGyroConfig = 0x1B;
constexpr uint8_t kRegAccelConfig = 0x1C;
constexpr uint8_t kRegAccelConfig2 = 0x1D;
constexpr uint8_t kRegIntPinConfig = 0x37;
constexpr uint8_t kRegAccelXoutH = 0x3B;
constexpr uint8_t kRegPowerMgmt1 = 0x6B;
constexpr uint8_t kRegWhoAmI = 0x75;

constexpr uint8_t kPowerReset = 0x80;
constexpr uint8_t kPowerAutoClock = 0x01;
constexpr uint8_t kI2cBypassEnable = 0x02;
constexpr uint8_t kDlpf41Hz = 0x03;
constexpr uint8_t kGyroRange1000Dps = 0x10;
constexpr uint8_t kAccelRange8G = 0x10;
constexpr float kAccelLsbPerG = 4096.0f;
constexpr float kGyroLsbPerDps = 32.8f;

// Valores de WHO_AM_I com o mapa de registradores usado aqui:
// MPU-6050/9150, MPU-6500, MPU-9250 e MPU-9255
constexpr uint8_t kKnownWhoAmI[] = {0x68, 0x70, 0x71, 0x73};

// Registradores do magnetômetro AK8963
constexpr uint8_t kMagAddress = 0x0C;
constexpr uint8_t kMagRegWhoAmI = 0x00;
constexpr uint8_t kMagRegStatus1 = 0x02;
constexpr uint8_t kMagRegDataStart = 0x03;
constexpr uint8_t kMagRegControl1 = 0x0A;
constexpr uint8_t kMagRegSensitivity = 0x10;

constexpr uint8_t kMagWhoAmI = 0x48;
constexpr uint8_t kMagPowerDown = 0x00;
constexpr uint8_t kMagFuseRom = 0x0F;
constexpr uint8_t kMagContinuous16Bit100Hz = 0x16;
constexpr uint8_t kMagDataReady = 0x01;
constexpr uint8_t kMagOverflow = 0x08;
constexpr float kMagUtPerLsb = 0.15f;  // Saída de 16 bits

constexpr uint8_t kMotionBytes = 14;  // Accel (6), temperatura (2), giroscópio (6)
constexpr uint8_t kMagBytes = 7;      // Eixos (6) e ST2 (1), cuja leitura libera a próxima

constexpr uint8_t kAxes = 3;
constexpr uint8_t kGyroOffsetBytes = 8;
constexpr uint8_t kMagStatus2Index = 6;

bool writeRegister(uint8_t address, uint8_t reg, uint8_t value) {
  Wire.beginTransmission(address);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

bool readRegisters(uint8_t address, uint8_t reg, uint8_t* buffer, uint8_t length) {
  Wire.beginTransmission(address);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) {
    return false;
  }
  if (Wire.requestFrom(address, length) != length) {
    return false;
  }
  for (uint8_t i = 0; i < length; ++i) {
    buffer[i] = static_cast<uint8_t>(Wire.read());
  }
  return true;
}

int16_t toInt16(uint8_t high, uint8_t low) {
  return static_cast<int16_t>((static_cast<uint16_t>(high) << 8) | low);
}

bool isKnownWhoAmI(uint8_t value) {
  for (uint8_t known : kKnownWhoAmI) {
    if (value == known) {
      return true;
    }
  }
  return false;
}

}  // namespace

bool Imu::begin() {
  for (uint8_t address : config::kImuAddresses) {
    uint8_t whoAmI = 0;
    if (!readRegisters(address, kRegWhoAmI, &whoAmI, 1)) {
      continue;
    }
    address_ = address;
    whoAmI_ = whoAmI;

    if (!isKnownWhoAmI(whoAmI) || !configureMotion()) {
      return false;
    }
    motionPresent_ = true;
    magPresent_ = configureMag();
    return true;
  }
  return false;
}

bool Imu::configureMotion() {
  if (!writeRegister(address_, kRegPowerMgmt1, kPowerReset)) {
    return false;
  }
  delay(100);  // Tempo de reset, chamado apenas no setup()

  return writeRegister(address_, kRegPowerMgmt1, kPowerAutoClock) &&
         writeRegister(address_, kRegConfig, kDlpf41Hz) &&
         writeRegister(address_, kRegGyroConfig, kGyroRange1000Dps) &&
         writeRegister(address_, kRegAccelConfig, kAccelRange8G) &&
         writeRegister(address_, kRegAccelConfig2, kDlpf41Hz);
}

bool Imu::configureMag() {
  // O bypass expõe o AK8963 diretamente no barramento I2C principal
  if (!writeRegister(address_, kRegIntPinConfig, kI2cBypassEnable)) {
    return false;
  }

  uint8_t whoAmI = 0;
  if (!readRegisters(kMagAddress, kMagRegWhoAmI, &whoAmI, 1) || whoAmI != kMagWhoAmI) {
    return false;
  }

  if (!writeRegister(kMagAddress, kMagRegControl1, kMagPowerDown)) {
    return false;
  }
  delay(10);
  if (!writeRegister(kMagAddress, kMagRegControl1, kMagFuseRom)) {
    return false;
  }
  delay(10);

  // Ajuste de sensibilidade de fábrica, conforme o datasheet do AK8963
  uint8_t sensitivity[kAxes] = {0, 0, 0};
  if (!readRegisters(kMagAddress, kMagRegSensitivity, sensitivity, kAxes)) {
    return false;
  }
  for (uint8_t axis = 0; axis < kAxes; ++axis) {
    const float adjustment = (static_cast<float>(sensitivity[axis]) - 128.0f) / 256.0f + 1.0f;
    magScaleUt_[axis] = adjustment * kMagUtPerLsb;
  }

  if (!writeRegister(kMagAddress, kMagRegControl1, kMagPowerDown)) {
    return false;
  }
  delay(10);
  return writeRegister(kMagAddress, kMagRegControl1, kMagContinuous16Bit100Hz);
}

void Imu::sample() {
  if (motionPresent_) {
    sampleMotion();
  }
  if (magPresent_) {
    sampleMag();
  }
}

void Imu::sampleMotion() {
  uint8_t raw[kMotionBytes];
  if (!readRegisters(address_, kRegAccelXoutH, raw, kMotionBytes)) {
    data_.motionValid = false;
    return;
  }

  for (uint8_t axis = 0; axis < kAxes; ++axis) {
    const uint8_t accelIndex = axis * 2;
    const uint8_t gyroIndex = kGyroOffsetBytes + axis * 2;
    data_.accelG[axis] = toInt16(raw[accelIndex], raw[accelIndex + 1]) / kAccelLsbPerG;
    data_.gyroDps[axis] = toInt16(raw[gyroIndex], raw[gyroIndex + 1]) / kGyroLsbPerDps;
  }
  data_.motionValid = true;
}

void Imu::sampleMag() {
  uint8_t status = 0;
  if (!readRegisters(kMagAddress, kMagRegStatus1, &status, 1)) {
    data_.magValid = false;
    return;
  }
  if ((status & kMagDataReady) == 0) {
    return;  // Medida nova ainda não pronta: a anterior tem menos de 10 ms
  }

  uint8_t raw[kMagBytes];
  if (!readRegisters(kMagAddress, kMagRegDataStart, raw, kMagBytes) ||
      (raw[kMagStatus2Index] & kMagOverflow) != 0) {
    data_.magValid = false;
    return;
  }

  // O AK8963 envia o byte baixo primeiro
  for (uint8_t axis = 0; axis < kAxes; ++axis) {
    const uint8_t index = axis * 2;
    data_.magUt[axis] = toInt16(raw[index + 1], raw[index]) * magScaleUt_[axis];
  }
  data_.magValid = true;
}
