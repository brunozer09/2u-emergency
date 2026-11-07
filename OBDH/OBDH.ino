// Firmware do logger OBDH para ESP32 DEVKIT V1.
// Recebe o CSV do TTeC pela UART2 e grava no cartão SD por SPI. Ver CLAUDE.md.
// As tarefas rodam em FreeRTOS, que já vem com o pacote esp32.
//
// Placa: "DOIT ESP32 DEVKIT V1" (pacote esp32 da Espressif)
//
// Bibliotecas: SD e SPI, incluídas no pacote esp32. Nada a instalar.

#include "Buzzer.h"
#include "Console.h"
#include "LineReceiver.h"
#include "ProjectConfig.h"
#include "RtosTasks.h"
#include "SdLogger.h"
#include "StatusLed.h"

namespace {

Console console(Serial);
LineReceiver receiver(Serial2);
SdLogger sdLogger;
StatusLed led;
Buzzer buzzer;
rtos::Modules modules{receiver, sdLogger, console, led, buzzer};

}  // namespace

void setup() {
  // Primeiro de tudo: define o nível dos pinos de boot o quanto antes
  led.begin();
  buzzer.begin();

  console.begin();
  console.log("inicializando");

  receiver.begin();  // Antes do SD: a montagem é lenta e os bytes já ficam no buffer

  if (sdLogger.begin()) {
    console.log("sd ok");
  } else {
    console.log("sd FALHOU: nova tentativa a cada %lu ms",
                static_cast<unsigned long>(config::kSdRetryPeriodMs));
  }

  rtos::start(modules);
}

void loop() {
  // Todo o trabalho acontece nas tarefas: encerra a tarefa do Arduino e libera sua pilha
  vTaskDelete(nullptr);
}
