// Recepção do enlace com o TTeC: monta linhas completas e descarta as corrompidas.
#pragma once

#include <Arduino.h>
#include <stdint.h>

#include "ProjectConfig.h"

enum class LineType : uint8_t { kLog, kHeader, kData };

struct ReceivedLine {
  LineType type = LineType::kLog;
  uint16_t length = 0;
  char text[config::kLineBytes] = {};  // Sem o fim de linha
};

struct ReceiverStats {
  uint32_t accepted = 0;   // Linhas válidas entregues
  uint32_t malformed = 0;  // Descartadas por conteúdo inválido
  uint32_t tooLong = 0;    // Descartadas por excederem o buffer
  uint32_t dropped = 0;    // Válidas, mas perdidas por falta de espaço na fila
};

class LineReceiver {
 public:
  explicit LineReceiver(HardwareSerial& serial) : serial_(serial) {}

  void begin();

  // Consome a UART até completar uma linha válida. Retorna false se os bytes acabarem
  // antes disso; a linha parcial fica guardada para a próxima chamada.
  bool readLine(ReceivedLine& line);

  void countDropped() { ++stats_.dropped; }
  const ReceiverStats& stats() const { return stats_; }

 private:
  bool finishLine(ReceivedLine& line);

  HardwareSerial& serial_;
  ReceiverStats stats_;
  char buffer_[config::kLineBytes];
  uint16_t length_ = 0;
  bool overflowed_ = false;
  bool synchronized_ = false;  // Falso até o primeiro '\n', pois a linha inicial pode estar cortada
};
