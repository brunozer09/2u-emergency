// Firmware de leitura e telemetria de sensores para ESP32 DEVKIT V1.
// Lê BME/BMP280, IMU MPU e GPS e transmite CSV pela UART2. Ver CLAUDE.md.
//
// Placa: "DOIT ESP32 DEVKIT V1" (pacote esp32 da Espressif)
//
// Bibliotecas (instalar pelo Library Manager):
//   - TinyGPSPlus, de Mikal Hart
//   - Adafruit BMP280 Library
//   - Adafruit BME280 Library
//   - Adafruit Unified Sensor e Adafruit BusIO (dependências das anteriores)
//
// ATENÇÃO: desconecte o GPS do RX0 antes de gravar o firmware.

#include <Wire.h>

#include "Barometer.h"
#include "Gps.h"
#include "Imu.h"
#include "ProjectConfig.h"
#include "Telemetry.h"

namespace {

Barometer barometer;
Imu imu;
Gps gps(Serial);
Telemetry telemetry(Serial2);

uint32_t lastBarometerMs = 0;
uint32_t lastImuMs = 0;
uint32_t lastTelemetryMs = 0;
bool gpsSilenceReported = false;

bool isDue(uint32_t nowMs, uint32_t& lastMs, uint32_t periodMs) {
  if (nowMs - lastMs < periodMs) {
    return false;
  }
  lastMs = nowMs;
  return true;
}

void initBarometer() {
  if (barometer.begin()) {
    telemetry.log("barometro ok: endereco 0x%02X, chip id 0x%02X", barometer.address(),
                  barometer.chipId());
  } else {
    telemetry.log("barometro FALHOU: endereco 0x%02X, chip id 0x%02X", barometer.address(),
                  barometer.chipId());
  }
}

void initImu() {
  if (!imu.begin()) {
    telemetry.log("imu FALHOU: endereco 0x%02X, WHO_AM_I 0x%02X", imu.address(), imu.whoAmI());
    return;
  }
  telemetry.log("imu ok: endereco 0x%02X, WHO_AM_I 0x%02X, magnetometro %s", imu.address(),
                imu.whoAmI(), imu.magPresent() ? "ok" : "ausente");
}

}  // namespace

void setup() {
  telemetry.begin();
  telemetry.log("inicializando");

  Wire.begin(config::kI2cSdaPin, config::kI2cSclPin, config::kI2cClockHz);
  Wire.setTimeOut(config::kI2cTimeoutMs);

  initBarometer();
  initImu();
  gps.begin();

  telemetry.sendHeader();
}

void loop() {
  gps.poll();

  const uint32_t nowMs = millis();

  if (isDue(nowMs, lastBarometerMs, config::kBarometerPeriodMs)) {
    barometer.sample();
  }
  if (isDue(nowMs, lastImuMs, config::kImuPeriodMs)) {
    imu.sample();
  }
  if (isDue(nowMs, lastTelemetryMs, config::kTelemetryPeriodMs)) {
    gps.sample();
    telemetry.sendSample(barometer.data(), imu.data(), gps.data());
  }

  if (!gpsSilenceReported && nowMs >= config::kGpsNoDataTimeoutMs && !gps.hasReceivedData()) {
    telemetry.log("gps sem dados: verificar ligacao e baud rate");
    gpsSilenceReported = true;
  }
}
