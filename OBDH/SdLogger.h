// Gravação no cartão SD: um arquivo CSV por execução do TTeC, sempre com cabeçalho.
#pragma once

#include <stdint.h>
#include <stdio.h>

#include "LineReceiver.h"

struct SdStats {
  uint32_t linesWritten = 0;
  uint32_t failures = 0;  // Erros de gravação que exigiram remontar o cartão
  uint32_t mountFailures = 0;  // Tentativas de montagem sem sucesso
  uint16_t fileIndex = 0;
  bool ready = false;
};

class SdLogger {
 public:
  // Monta o cartão. Pode ser chamado de novo depois de uma falha.
  bool begin();

  // Grava a linha. Em caso de erro retorna false e desmonta o cartão: chame begin()
  // e tente a mesma linha de novo.
  bool write(const ReceivedLine& line);

  // Força os dados para o cartão quando vence o período. Só depois disso eles
  // sobrevivem a uma queda de energia.
  void flushIfDue();

  bool ready() const { return stats_.ready; }
  const SdStats& stats() const { return stats_; }

 private:
  bool openNextFile();
  void closeFile();
  void handleFailure();

  // stdio em vez de SD.open(): File::flush() descarta o erro do fsync, e assim um
  // cartão removido passaria despercebido
  FILE* file_ = nullptr;
  SdStats stats_;
  uint32_t lastFlushMs_ = 0;
  uint32_t rowsInFile_ = 0;
  uint16_t nextIndex_ = 1;
  bool dirty_ = false;
};
