# Suporte a FreeRTOS

**Sim, é possível, e não exige nenhuma biblioteca nova.** O pacote esp32 para Arduino já roda sobre o FreeRTOS: `setup()` e `loop()` executam dentro de uma tarefa chamada `loopTask`. Esta mudança substitui o agendamento manual com `millis()` por tarefas independentes.

Compilado com o pacote esp32 3.3.11. O formato do CSV e a ligação elétrica não mudaram.

## Arquitetura

```text
 Tarefa gps  ──────► gpsMailbox ─────────┐
 (UART0)                                 │
                                         ├──► Tarefa telemetry ──► Serial2 (CSV)
 Tarefa imu  ──┬───► imuMailbox ─────────┤
               │                         │
          mutex I2C                      │
               │                         │
 Tarefa baro ──┼───► barometerMailbox ───┤
               │                         │
 Tarefa bat  ──┴───► batteryMailbox ─────┘
```

| Tarefa | Prioridade | Período | Pilha | Função |
|---|---|---|---|---|
| `gps` | 5 | 50 ms | 3072 bytes | Esvazia a UART, decodifica NMEA e publica |
| `imu` | 4 | 100 ms | 3072 bytes | Lê a IMU com o mutex I2C e publica |
| `baro` | 3 | 100 ms | 3072 bytes | Lê o barômetro com o mutex I2C e publica |
| `bat` | 2 | 200 ms | 3072 bytes | Lê o INA219 com o mutex I2C e publica |
| `telemetry` | 1 | 100 ms | 4096 bytes | Monta e envia o CSV e os logs |

Todos esses valores estão em `ProjectConfig.h`.

## Arquivos alterados

| Arquivo | Mudança |
|---|---|
| `RtosTasks.h/.cpp` | **Novo.** Tarefas, mutex I2C e início do sistema |
| `Mailbox.h` | **Novo.** Troca de dados entre tarefas |
| `ProjectConfig.h` | Prioridades, pilhas, núcleo, novos períodos e limites |
| `TTeC.ino` | O `setup()` inicia as tarefas. O `loop()` apenas encerra a `loopTask` |
| `Gps.h/.cpp` | `hasReceivedData()` virou o campo `GpsData::dataReceived`, porque a telemetria não pode consultar o decodificador de outra tarefa |
| `CLAUDE.md` | Regras de temporização e concorrência atualizadas, pois a regra anterior mandava usar `millis()` |

`Barometer`, `Imu` e `Telemetry` não mudaram: nenhuma dessas classes sabe que existe um RTOS.

## Decisões de projeto

- **Dados passam por `Mailbox`, não por variáveis compartilhadas.** A leitura de um sensor tem vários campos, então uma tarefa poderia ler uma estrutura escrita pela metade. A `Mailbox` é uma fila de uma posição com `xQueueOverwrite`: guarda sempre a leitura mais recente, por cópia, e nunca bloqueia.
- **Mutex próprio no I2C.** A biblioteca `Wire` protege cada transação, mas uma leitura de sensor é uma sequência delas. O mutex cobre o `sample()` inteiro e, por ser um mutex do FreeRTOS, tem herança de prioridade.
- **Leitura antiga vira campo vazio.** Toda publicação guarda o instante em que ocorreu. Sem leitura nova por `kSampleMaxAgeMs` (500 ms), a telemetria envia campos vazios. Um único mecanismo cobre barramento ocupado e tarefa travada, e mantém a regra de nunca transmitir o último valor válido.
- **Só a tarefa `telemetry` usa `Serial2`.** Por isso as linhas não se misturam e nenhum mutex é necessário nessa porta. O `setup()` escreve nela apenas antes de iniciar as tarefas.
- **Alocação estática.** Tarefas, filas e mutex têm memória definida em tempo de compilação. Não há caminho de erro por falta de heap, e o consumo aparece no relatório do compilador.
- **Tudo no núcleo 1.** É o núcleo do Arduino, e o projeto não usa Wi-Fi nem Bluetooth, que motivariam a separação. As tarefas são fixadas a um núcleo porque usam ponto flutuante.
- **Prioridades por período.** Quanto menor o período, maior a prioridade. A telemetria é a mais baixa, para que formatar texto nunca atrase uma leitura.
- **`loopTask` encerrada.** Ela não tem mais função, e encerrá-la devolve 8 KB ao heap.

## O que muda no comportamento

- **Período mais estável.** `xTaskDelayUntil` mantém o período fixo, sem acumular atraso.
- **Nova linha de log.** A cada 60 s sai `# pilha livre (bytes): gps=... imu=... baro=... telemetria=...`. Quem processa o CSV já deve ignorar linhas com `#`.
- **Atraso de até dois períodos.** Depois que um sensor volta a responder, os dados podem levar até duas linhas para reaparecer, porque leitura e telemetria têm períodos independentes.

## Consumo de recursos

| Recurso | Antes | Depois |
|---|---|---|
| Flash | 311.296 bytes | 312.984 bytes (+1.688) |
| RAM estática | 24.300 bytes | 39.580 bytes (+15.280) |

Os 15 KB de RAM estática são compensados em parte pelos 8 KB da `loopTask` devolvidos ao heap. As pilhas foram definidas com folga e podem diminuir depois de medidas no hardware.

## Verificação

**Compilação:** o firmware compila para `esp32:esp32:esp32doit-devkit-v1`.

**Simulação no PC:** as fontes reais do firmware foram compiladas com FreeRTOS, `Wire` e UART simulados por threads e executadas com ThreadSanitizer. Os cenários cobriram:

- Todos os sensores saudáveis, com NMEA real decodificado pelo TinyGPSPlus e conversões conferidas: nenhum erro de id, coluna ou valor.
- Barômetro desconectado e reconectado com o firmware em execução.
- Mutex I2C indisponível por 1 s: os campos de I2C saem vazios e o GPS continua.
- GPS parando de enviar: os campos do GPS saem vazios.
- BME280 em 0x77 com MPU-6500 sem magnetômetro, e chips com ID desconhecido.

Nenhuma condição de corrida foi encontrada. Três defeitos inseridos de propósito foram detectados: retirar o mutex, retirar a verificação de idade e ler um módulo fora da `Mailbox`.

**Não verificado:** a simulação usa threads comuns, sem prioridades. O escalonador real do FreeRTOS, os asserts de criação de tarefas estáticas e os tempos de execução não foram exercitados, e o firmware ainda não rodou em um ESP32.

### Checklist no hardware

1. O firmware inicia, envia o cabeçalho e não reinicia sozinho.
2. Os ids sobem de um em um, a 10 linhas por segundo.
3. Após 60 s, o log `pilha livre` mostra pelo menos 512 bytes de folga em cada tarefa. Só reduza as pilhas depois de deixar o firmware rodando com todos os sensores ativos e GPS com fix.

Se uma tarefa estourar a pilha, o FreeRTOS detecta pelo canário e reinicia o ESP32. A mensagem de erro sai pelo TX0, visível no conector USB da placa. Como a UART0 está configurada em 9600 baud para o GPS, tente 9600 no monitor serial se o texto vier ilegível a 115200.

## Regras ao alterar o código

- Depois de `rtos::start()`, chame métodos de um módulo apenas pela tarefa dona dele.
- Novo sensor I2C: use `runI2cSensor`, que já aplica o mutex.
- Estruturas enviadas por `Mailbox` precisam ser copiáveis byte a byte, e seu valor padrão precisa indicar dado inválido.
- Não chame `enableLoopWDT()`, pois a `loopTask` não existe mais.
- Mudou um período? Os `static_assert` em `RtosTasks.cpp` conferem a idade máxima e o buffer do GPS na compilação.

## Limitações conhecidas

- **Sem watchdog nas tarefas.** Uma tarefa travada aparece como campos vazios, mas não reinicia o sistema. Adicionar `esp_task_wdt` seria o próximo passo natural.
- **Sem reconexão de sensores que falharam no início.** Um sensor ausente no `setup()` só volta após reiniciar. Um sensor que cai depois disso volta sozinho.
