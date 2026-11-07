#include "RtosTasks.h"

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include "Mailbox.h"
#include "ProjectConfig.h"

namespace rtos {
namespace {

// O buffer da UART precisa aguentar os bytes que chegam entre duas leituras (10 bits por byte)
static_assert(config::kLinkBaud / 10 * config::kReceiverPollPeriodMs / 1000 <
                  config::kLinkRxBufferBytes,
              "Buffer da UART insuficiente para o período de leitura");

// A espera por linhas limita quanto tempo as estatísticas do SD ficam sem atualizar
static_assert(config::kSdIdleWaitMs < config::kStatsMaxAgeMs, "Espera do SD");
static_assert(config::kSdRetryPeriodMs < config::kStatsMaxAgeMs, "Espera entre remontagens");
static_assert(config::kReceiverPollPeriodMs < config::kStatsMaxAgeMs, "Período da recepção");

// Os pulsos só têm a duração configurada se forem mais longos que o período da tarefa
static_assert(config::kIndicatorPeriodMs < config::kLedBlinkMs, "Pulso do LED");
static_assert(config::kIndicatorPeriodMs < config::kBuzzerBeepMs, "Duração do bipe");
static_assert(config::kBuzzerBeepMs < config::kBuzzerMinIntervalMs, "Intervalo entre bipes");

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

TaskSlot<config::kReceiverTaskStackBytes> receiverSlot;
TaskSlot<config::kIndicatorTaskStackBytes> indicatorSlot;
TaskSlot<config::kSdTaskStackBytes> sdSlot;
TaskSlot<config::kStatusTaskStackBytes> statusSlot;

Mailbox<ReceiverStats> receiverStatsMailbox;
Mailbox<SdStats> sdStatsMailbox;

// Linhas inteiras, copiadas: se a fila encher, a perda é de linhas completas,
// que aparecem como salto no id, e nunca de pedaços de linha
QueueHandle_t lineQueue = nullptr;
StaticQueue_t lineQueueControl;
uint8_t lineQueueStorage[config::kLineQueueDepth * sizeof(ReceivedLine)];

// Nunca espera pelo SD: toda a demora de gravação fica na tarefa de baixo
void receiverTask(void* argument) {
  LineReceiver& receiver = static_cast<Modules*>(argument)->receiver;
  const TickType_t period = pdMS_TO_TICKS(config::kReceiverPollPeriodMs);
  TickType_t lastWake = xTaskGetTickCount();
  ReceivedLine line;

  for (;;) {
    while (receiver.readLine(line)) {
      if (xQueueSend(lineQueue, &line, 0) != pdTRUE) {
        receiver.countDropped();
      }
    }
    receiverStatsMailbox.publish(receiver.stats());
    xTaskDelayUntil(&lastWake, period);
  }
}

// A linha só deixa de ser pendente depois de gravada, para sobreviver a uma falha do cartão
void sdTask(void* argument) {
  SdLogger& sdLogger = static_cast<Modules*>(argument)->sdLogger;
  const TickType_t idleWait = pdMS_TO_TICKS(config::kSdIdleWaitMs);
  const TickType_t retryPeriod = pdMS_TO_TICKS(config::kSdRetryPeriodMs);
  ReceivedLine line;
  bool pending = false;

  for (;;) {
    sdStatsMailbox.publish(sdLogger.stats());

    if (!sdLogger.ready()) {
      vTaskDelay(retryPeriod);
      sdLogger.begin();
      continue;
    }

    if (!pending) {
      pending = xQueueReceive(lineQueue, &line, idleWait) == pdTRUE;
    }
    if (pending && sdLogger.write(line)) {
      pending = false;
    }
    sdLogger.flushIfDue();
  }
}

// Observa as estatísticas e não interfere nas outras tarefas: LED e buzzer nunca
// atrasam a recepção ou a gravação.
void indicatorTask(void* argument) {
  Modules& modules = *static_cast<Modules*>(argument);
  StatusLed& led = modules.led;
  Buzzer& buzzer = modules.buzzer;
  const TickType_t period = pdMS_TO_TICKS(config::kIndicatorPeriodMs);
  TickType_t lastWake = xTaskGetTickCount();
  uint32_t lastAccepted = 0;
  uint32_t lastErrors = 0;

  for (;;) {
    // Sem limite de idade: os contadores só podem ser comparados com o valor real,
    // e uma leitura zerada por idade pareceria um novo evento ao voltar
    const ReceiverStats receiver = receiverStatsMailbox.readFresh(portMAX_DELAY);
    const SdStats sd = sdStatsMailbox.readFresh(portMAX_DELAY);

    if (receiver.accepted != lastAccepted) {
      lastAccepted = receiver.accepted;
      led.blink();
    }

    const uint32_t errors = receiver.malformed + receiver.tooLong + receiver.dropped +
                            sd.failures + sd.mountFailures;
    if (errors != lastErrors) {
      lastErrors = errors;
      buzzer.requestBeep();
    }

    led.update();
    buzzer.update();
    xTaskDelayUntil(&lastWake, period);
  }
}

// Única tarefa que usa Serial depois do setup()
void statusTask(void* argument) {
  Console& console = static_cast<Modules*>(argument)->console;
  const TickType_t period = pdMS_TO_TICKS(config::kStatusPeriodMs);
  const TickType_t maxAge = pdMS_TO_TICKS(config::kStatsMaxAgeMs);
  TickType_t lastWake = xTaskGetTickCount();

  for (;;) {
    xTaskDelayUntil(&lastWake, period);

    const ReceiverStats receiver = receiverStatsMailbox.readFresh(maxAge);
    const SdStats sd = sdStatsMailbox.readFresh(maxAge);

    console.log("rx: aceitas=%lu invalidas=%lu longas=%lu perdidas=%lu",
                static_cast<unsigned long>(receiver.accepted),
                static_cast<unsigned long>(receiver.malformed),
                static_cast<unsigned long>(receiver.tooLong),
                static_cast<unsigned long>(receiver.dropped));
    console.log("sd: %s, arquivo=%u, gravadas=%lu, falhas=%lu, montagens_falhas=%lu",
                sd.ready ? "ok" : "FALHA", static_cast<unsigned int>(sd.fileIndex),
                static_cast<unsigned long>(sd.linesWritten),
                static_cast<unsigned long>(sd.failures),
                static_cast<unsigned long>(sd.mountFailures));
    console.log("pilha livre (bytes): rx=%u indicadores=%u sd=%u status=%u",
                receiverSlot.stackFreeBytes(), indicatorSlot.stackFreeBytes(),
                sdSlot.stackFreeBytes(), statusSlot.stackFreeBytes());
  }
}

}  // namespace

void start(Modules& modules) {
  receiverStatsMailbox.begin();
  sdStatsMailbox.begin();
  lineQueue = xQueueCreateStatic(config::kLineQueueDepth, sizeof(ReceivedLine), lineQueueStorage,
                                 &lineQueueControl);

  receiverSlot.start("receiver", receiverTask, &modules, config::kReceiverTaskPriority);
  indicatorSlot.start("indicators", indicatorTask, &modules, config::kIndicatorTaskPriority);
  sdSlot.start("sd", sdTask, &modules, config::kSdTaskPriority);
  statusSlot.start("status", statusTask, &modules, config::kStatusTaskPriority);
}

}  // namespace rtos
