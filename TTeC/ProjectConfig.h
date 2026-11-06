// Configuração central do firmware: pinos, endereços, baud rates e períodos.
#pragma once

#include <stdint.h>

namespace config {

// Barramento I2C (BME/BMP280 e MPU)
constexpr int kI2cSdaPin = 21;
constexpr int kI2cSclPin = 22;
constexpr uint32_t kI2cClockHz = 400000;
constexpr uint16_t kI2cTimeoutMs = 20;

// Endereços I2C possíveis de cada sensor
constexpr uint8_t kBarometerAddresses[] = {0x76, 0x77};
constexpr uint8_t kImuAddresses[] = {0x68, 0x69};

// GPS na UART0 (Serial)
constexpr int kGpsRxPin = 3;
constexpr int kGpsTxPin = 1;
constexpr uint32_t kGpsBaud = 9600;
constexpr uint16_t kGpsRxBufferBytes = 512;
constexpr uint32_t kGpsMaxDataAgeMs = 2000;     // Idade máxima para um dado do GPS ser válido
constexpr uint32_t kGpsNoDataTimeoutMs = 5000;  // Sem bytes após este tempo, avisa no log

// Telemetria na UART2 (Serial2)
constexpr int kTelemetryRxPin = 16;
constexpr int kTelemetryTxPin = 17;
constexpr uint32_t kTelemetryBaud = 115200;
constexpr uint16_t kTelemetryTxBufferBytes = 512;

// Períodos de amostragem e transmissão
constexpr uint32_t kBarometerPeriodMs = 100;
constexpr uint32_t kImuPeriodMs = 100;
constexpr uint32_t kTelemetryPeriodMs = 100;

}  // namespace config
