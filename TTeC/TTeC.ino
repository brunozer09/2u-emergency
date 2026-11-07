// Firmware de leitura e telemetria de sensores para ESP32 DEVKIT V1.
// Lê BME/BMP280, IMU MPU, GPS e INA219 e transmite CSV pela UART2. Ver CLAUDE.md.
// As leituras rodam em tarefas FreeRTOS, que já vem com o pacote esp32. Ver FREERTOS.md.
//
// Placa: "DOIT ESP32 DEVKIT V1" (pacote esp32 da Espressif)
//
// Bibliotecas (instalar pelo Library Manager):
//   - TinyGPSPlus, de Mikal Hart
//   - Adafruit BMP280 Library
//   - Adafruit BME280 Library
//   - Adafruit INA219
//   - Adafruit Unified Sensor e Adafruit BusIO (dependências das anteriores)
//
// ATENÇÃO: desconecte o GPS do RX0 antes de gravar o firmware.

#include <Wire.h>

#include "Barometer.h"
#include "BatteryMonitor.h"
#include "Gps.h"
#include "Imu.h"
#include "ProjectConfig.h"
#include "RtosTasks.h"
#include "Telemetry.h"

namespace {

Barometer barometer;
Imu imu;
Gps gps(Serial);
BatteryMonitor batteryMonitor;
Telemetry telemetry(Serial2);
rtos::Modules modules{barometer, imu, gps, batteryMonitor, telemetry};

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

void initBatteryMonitor() {
  telemetry.log("bateria (INA219) %s: endereco 0x%02X",
                batteryMonitor.begin() ? "ok" : "FALHOU", config::kBatteryMonitorAddress);
}

}  // namespace

void setup() {
  telemetry.begin();
  telemetry.log("inicializando");

  Wire.begin(config::kI2cSdaPin, config::kI2cSclPin, config::kI2cClockHz);
  Wire.setTimeOut(config::kI2cTimeoutMs);

  initBarometer();
  initImu();
  initBatteryMonitor();
  gps.begin();

  telemetry.sendHeader();
  rtos::start(modules);
}

void loop() {
  // Todo o trabalho acontece nas tarefas: encerra a tarefa do Arduino e libera sua pilha
  vTaskDelete(nullptr);
}
