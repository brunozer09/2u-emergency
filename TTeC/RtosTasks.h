// Tarefas FreeRTOS do firmware. Arquitetura e decisões em FREERTOS.md.
#pragma once

#include "Barometer.h"
#include "BatteryMonitor.h"
#include "Gps.h"
#include "Imu.h"
#include "Telemetry.h"

namespace rtos {

// Módulos já inicializados. Precisam existir enquanto o firmware rodar.
struct Modules {
  Barometer& barometer;
  Imu& imu;
  Gps& gps;
  BatteryMonitor& batteryMonitor;
  Telemetry& telemetry;
};

// Inicia as tarefas. A partir daqui, cada módulo pertence à sua tarefa:
// não chame nenhum método deles fora dela.
void start(Modules& modules);

}  // namespace rtos
