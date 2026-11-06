// IMU da família MPU-92xx/65xx: acelerômetro, giroscópio e magnetômetro AK8963.
// Driver próprio sobre Wire porque as bibliotecas costumam rejeitar valores de
// WHO_AM_I diferentes do esperado, e o chip real do módulo ainda será confirmado.
#pragma once

#include <stdint.h>

struct ImuData {
  float accelG[3] = {0.0f, 0.0f, 0.0f};
  float gyroDps[3] = {0.0f, 0.0f, 0.0f};
  float magUt[3] = {0.0f, 0.0f, 0.0f};
  bool motionValid = false;  // Acelerômetro e giroscópio
  bool magValid = false;
};

class Imu {
 public:
  // Retorna true se acelerômetro e giroscópio funcionam. O magnetômetro é
  // opcional: chips como o MPU-6500 não o possuem (ver magPresent()).
  bool begin();
  void sample();

  const ImuData& data() const { return data_; }
  uint8_t address() const { return address_; }
  uint8_t whoAmI() const { return whoAmI_; }
  bool magPresent() const { return magPresent_; }

 private:
  bool configureMotion();
  bool configureMag();
  void sampleMotion();
  void sampleMag();

  ImuData data_;
  float magScaleUt_[3] = {0.0f, 0.0f, 0.0f};
  uint8_t address_ = 0;
  uint8_t whoAmI_ = 0;
  bool motionPresent_ = false;
  bool magPresent_ = false;
};
