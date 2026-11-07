// Tarefas FreeRTOS do firmware. Arquitetura descrita em CLAUDE.md.
#pragma once

#include "Buzzer.h"
#include "Console.h"
#include "LineReceiver.h"
#include "SdLogger.h"
#include "StatusLed.h"

namespace rtos {

// Módulos já inicializados. Precisam existir enquanto o firmware rodar.
struct Modules {
  LineReceiver& receiver;
  SdLogger& sdLogger;
  Console& console;
  StatusLed& led;
  Buzzer& buzzer;
};

// Inicia as tarefas. A partir daqui, cada módulo pertence à sua tarefa:
// não chame nenhum método deles fora dela.
void start(Modules& modules);

}  // namespace rtos
