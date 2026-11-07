// LED de atividade: pisca sem bloquear a tarefa que o controla.
#pragma once

#include <stdint.h>

class StatusLed {
 public:
  void begin();
  void blink();   // Acende e inicia a contagem do pulso
  void update();  // Apaga ao fim do pulso: chamar periodicamente

 private:
  uint32_t onSinceMs_ = 0;
  bool isOn_ = false;
};
