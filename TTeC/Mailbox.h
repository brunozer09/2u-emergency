// Caixa de correio FreeRTOS: guarda a leitura mais recente de um sensor, por cópia.
// Uma tarefa publica e outra lê sem compartilhar memória, então a leitura nunca
// chega pela metade. A memória é estática, definida em tempo de compilação.
#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <stdint.h>

#include <type_traits>

template <typename T>
class Mailbox {
  static_assert(std::is_trivially_copyable<T>::value, "A fila copia T byte a byte");

 public:
  // Chamar antes de iniciar as tarefas
  void begin() {
    queue_ = xQueueCreateStatic(1, sizeof(Entry), storage_, &control_);
  }

  // Substitui a leitura anterior. Nunca bloqueia.
  void publish(const T& value) {
    const Entry entry{value, xTaskGetTickCount()};
    xQueueOverwrite(queue_, &entry);
  }

  // Retorna a leitura atual, ou T{} (com todas as flags de validade em falso) se
  // nada foi publicado nos últimos maxAgeTicks. Nunca bloqueia.
  T readFresh(TickType_t maxAgeTicks) const {
    Entry entry;
    if (xQueuePeek(queue_, &entry, 0) != pdTRUE ||
        xTaskGetTickCount() - entry.publishedAt > maxAgeTicks) {
      return T{};
    }
    return entry.value;
  }

 private:
  struct Entry {
    T value;
    TickType_t publishedAt;
  };

  QueueHandle_t queue_ = nullptr;
  StaticQueue_t control_;
  uint8_t storage_[sizeof(Entry)];
};
