// Configuração central do firmware: pinos, endereços, baud rates, períodos e tarefas.
#pragma once

#include <stdint.h>

namespace config {

// Barramento I2C (BME/BMP280, MPU e INA219)
constexpr int kI2cSdaPin = 21;
constexpr int kI2cSclPin = 22;
constexpr uint32_t kI2cClockHz = 400000;
constexpr uint16_t kI2cTimeoutMs = 20;
constexpr uint32_t kI2cMutexTimeoutMs = 50;  // Espera máxima de uma tarefa pelo barramento

// Endereços I2C possíveis de cada sensor
constexpr uint8_t kBarometerAddresses[] = {0x76, 0x77};
constexpr uint8_t kImuAddresses[] = {0x68, 0x69};
constexpr uint8_t kBatteryMonitorAddress = 0x40;  // Padrão do INA219, com A0 e A1 abertos

// Limites de saúde da bateria. Valores para 1 célula de Li-ion/LiPo: ajustar à bateria real.
constexpr float kBatteryLowV = 3.5f;
constexpr float kBatteryCriticalV = 3.3f;
constexpr float kBatteryHysteresisV = 0.1f;

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
constexpr uint32_t kBatteryPeriodMs = 200;
constexpr uint32_t kGpsPollPeriodMs = 50;
constexpr uint32_t kTelemetryPeriodMs = 100;
constexpr uint32_t kSampleMaxAgeMs = 500;  // Sem leitura nova por este tempo, os campos saem vazios
constexpr uint32_t kStackReportPeriodMs = 60000;

// Tarefas FreeRTOS. Sem Wi-Fi ou Bluetooth, todas ficam no núcleo do Arduino.
constexpr int kTaskCore = 1;

// Maior número = mais urgente. Prioridades por período: quanto menor, mais alta.
constexpr uint32_t kGpsTaskPriority = 5;
constexpr uint32_t kImuTaskPriority = 4;
constexpr uint32_t kBarometerTaskPriority = 3;
constexpr uint32_t kBatteryTaskPriority = 2;
constexpr uint32_t kTelemetryTaskPriority = 1;

// Pilhas com folga: conferir no hardware com o log "pilha livre" antes de reduzir
constexpr uint32_t kGpsTaskStackBytes = 3072;
constexpr uint32_t kImuTaskStackBytes = 3072;
constexpr uint32_t kBarometerTaskStackBytes = 3072;
constexpr uint32_t kBatteryTaskStackBytes = 3072;
constexpr uint32_t kTelemetryTaskStackBytes = 4096;

}  // namespace config
