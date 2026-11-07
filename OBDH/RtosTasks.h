// Tarefas FreeRTOS do firmware. Arquitetura descrita em CLAUDE.md.
#pragma once

#include "Console.h"
#include "LineReceiver.h"
#include "SdLogger.h"

namespace rtos {

// Módulos já inicializados. Precisam existir enquanto o firmware rodar.
struct Modules {
  LineReceiver& receiver;
  SdLogger& sdLogger;
  Console& console;
};

// Inicia as tarefas. A partir daqui, cada módulo pertence à sua tarefa:
// não chame nenhum método deles fora dela.
void start(Modules& modules);

}  // namespace rtos
