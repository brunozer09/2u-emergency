// Buzzer de alerta: bipa sem bloquear e limita a frequência dos bipes, para que uma
// sequência de erros não vire um som contínuo.
#pragma once

#include <stdint.h>

#include "ProjectConfig.h"

class Buzzer {
 public:
  void begin();
  void requestBeep();  // Pedidos feitos durante o intervalo mínimo viram um único bipe
  void update();       // Liga e desliga o som: chamar periodicamente

 private:
  void setSound(bool on);

  // Valor inicial no passado, para o primeiro bipe não esperar o intervalo mínimo
  uint32_t lastBeepMs_ = 0u - config::kBuzzerMinIntervalMs;
  bool requested_ = false;
  bool isSounding_ = false;
};
