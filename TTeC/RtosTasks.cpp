#include "RtosTasks.h"

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include "Mailbox.h"
#include "ProjectConfig.h"

namespace rtos {
namespace {

// Uma leitura precisa poder ser republicada antes de ser considerada antiga
static_assert(config::kBarometerPeriodMs < config::kSampleMaxAgeMs, "Período do barômetro");
static_assert(config::kImuPeriodMs < config::kSampleMaxAgeMs, "Período da IMU");
static_assert(config::kGpsPollPeriodMs < config::kSampleMaxAgeMs, "Período do GPS");

// O buffer da UART precisa aguentar os bytes que chegam entre duas leituras (10 bits por byte)
static_assert(config::kGpsBaud / 10 * config::kGpsPollPeriodMs / 1000 < config::kGpsRxBufferBytes,
              "Buffer do GPS insuficiente para o período de leitura");

template <uint32_t StackBytes>
struct TaskSlot {
  StaticTask_t control;
  alignas(16) StackType_t stack[StackBytes / sizeof(StackType_t)];
  TaskHandle_t handle;

  void start(const char* name, TaskFunction_t function, void* argument, uint32_t priority) {
    handle = xTaskCreateStaticPinnedToCore(function, name, StackBytes / sizeof(StackType_t),
                                           argument, priority, stack, &control,
                                           config::kTaskCore);
  }

  // Menor folga de pilha já registrada, em bytes
  unsigned int stackFreeBytes() const {
    return uxTaskGetStackHighWaterMark(handle) * sizeof(StackType_t);
  }
};

TaskSlot<config::kGpsTaskStackBytes> gpsSlot;
TaskSlot<config::kImuTaskStackBytes> imuSlot;
TaskSlot<config::kBarometerTaskStackBytes> barometerSlot;
TaskSlot<config::kTelemetryTaskStackBytes> telemetrySlot;

Mailbox<GpsData> gpsMailbox;
Mailbox<ImuData> imuMailbox;
Mailbox<BarometerData> barometerMailbox;

// Wire protege cada transação, mas uma leitura de sensor é uma sequência delas
SemaphoreHandle_t i2cMutex = nullptr;
StaticSemaphore_t i2cMutexControl;

// Se o barramento não for obtido, nada é publicado. A leitura anterior envelhece
// e a telemetria passa a enviar campos vazios, sem tratamento especial aqui.
template <typename Sensor, typename Data>
[[noreturn]] void runI2cSensor(Sensor& sensor, Mailbox<Data>& mailbox, uint32_t periodMs) {
  const TickType_t period = pdMS_TO_TICKS(periodMs);
  const TickType_t mutexTimeout = pdMS_TO_TICKS(config::kI2cMutexTimeoutMs);
  TickType_t lastWake = xTaskGetTickCount();

  for (;;) {
    if (xSemaphoreTake(i2cMutex, mutexTimeout) == pdTRUE) {
      sensor.sample();
      xSemaphoreGive(i2cMutex);
      mailbox.publish(sensor.data());
    }
    xTaskDelayUntil(&lastWake, period);
  }
}

void barometerTask(void* argument) {
  Modules& modules = *static_cast<Modules*>(argument);
  runI2cSensor(modules.barometer, barometerMailbox, config::kBarometerPeriodMs);
}

void imuTask(void* argument) {
  Modules& modules = *static_cast<Modules*>(argument);
  runI2cSensor(modules.imu, imuMailbox, config::kImuPeriodMs);
}

void gpsTask(void* argument) {
  Gps& gps = static_cast<Modules*>(argument)->gps;
  const TickType_t period = pdMS_TO_TICKS(config::kGpsPollPeriodMs);
  TickType_t lastWake = xTaskGetTickCount();

  for (;;) {
    gps.poll();
    gps.sample();
    gpsMailbox.publish(gps.data());
    xTaskDelayUntil(&lastWake, period);
  }
}

// Única tarefa que usa Serial2 depois do setup(), o que evita linhas misturadas
void telemetryTask(void* argument) {
  Telemetry& telemetry = static_cast<Modules*>(argument)->telemetry;
  const TickType_t period = pdMS_TO_TICKS(config::kTelemetryPeriodMs);
  const TickType_t maxAge = pdMS_TO_TICKS(config::kSampleMaxAgeMs);
  const TickType_t stackReportPeriod = pdMS_TO_TICKS(config::kStackReportPeriodMs);
  TickType_t lastWake = xTaskGetTickCount();
  TickType_t lastStackReport = lastWake;
  bool gpsSilenceReported = false;

  for (;;) {
    xTaskDelayUntil(&lastWake, period);

    const GpsData gps = gpsMailbox.readFresh(maxAge);
    telemetry.sendSample(barometerMailbox.readFresh(maxAge), imuMailbox.readFresh(maxAge), gps);

    if (!gpsSilenceReported && !gps.dataReceived && millis() >= config::kGpsNoDataTimeoutMs) {
      telemetry.log("gps sem dados: verificar ligacao e baud rate");
      gpsSilenceReported = true;
    }

    if (lastWake - lastStackReport >= stackReportPeriod) {
      lastStackReport = lastWake;
      telemetry.log("pilha livre (bytes): gps=%u imu=%u baro=%u telemetria=%u",
                    gpsSlot.stackFreeBytes(), imuSlot.stackFreeBytes(),
                    barometerSlot.stackFreeBytes(), telemetrySlot.stackFreeBytes());
    }
  }
}

}  // namespace

void start(Modules& modules) {
  gpsMailbox.begin();
  imuMailbox.begin();
  barometerMailbox.begin();
  i2cMutex = xSemaphoreCreateMutexStatic(&i2cMutexControl);

  gpsSlot.start("gps", gpsTask, &modules, config::kGpsTaskPriority);
  imuSlot.start("imu", imuTask, &modules, config::kImuTaskPriority);
  barometerSlot.start("baro", barometerTask, &modules, config::kBarometerTaskPriority);
  telemetrySlot.start("telemetry", telemetryTask, &modules, config::kTelemetryTaskPriority);
}

}  // namespace rtos
