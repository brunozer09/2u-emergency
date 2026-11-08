# Firmware OBDH: logger em cartão SD

Firmware para ESP32 DEVKIT V1, desenvolvido na Arduino IDE. Ele recebe pela UART2 o CSV transmitido pelo firmware TTeC (`../TTeC`), que roda em outra placa, e grava os dados em um cartão SD.

## Hardware

| Conexão | Interface | Pinos ESP32 (OBDH) | Observações |
|---|---|---|---|
| Enlace com o TTeC | UART2 (`Serial2`) | RX2 = GPIO16, TX2 = GPIO17 | 115200 baud. Ligação cruzada: TX2 do TTeC no RX2 do OBDH, e vice-versa |
| Módulo de cartão SD | SPI (VSPI) | CS 5, SCK 18, MISO 19, MOSI 23 | Cartão FAT32. Alimentar conforme o módulo: 5 V se tiver regulador, 3,3 V se não tiver |
| Log de estado | UART0 (`Serial`) | Conector USB da placa | 115200 baud |
| LED de atividade | GPIO | GPIO2 | Mesmo pino do LED da placa. LED externo: resistor em série, com o cátodo no GND |
| Buzzer de erro | GPIO | GPIO15 | Ativo por padrão. Para buzzer passivo, use `kBuzzerIsPassive = true` |

## Restrições do hardware

- **GND comum.** As duas placas precisam compartilhar o terra, ou o enlace não funciona.
- **O enlace não tem checksum.** Um erro que preserve o formato da linha não é detectado.
- **O OBDH ainda não transmite ao TTeC.** O TX2 fica reservado.
- **O cartão pode demorar.** Gravações podem levar centenas de milissegundos, por isso recepção e gravação são tarefas separadas.
- **GPIO2 e GPIO15 são pinos de boot.** O GPIO2 precisa ficar em nível baixo ou solto durante a gravação do firmware, então nada externo pode puxá-lo para 3,3 V. O GPIO15 pode gerar um som breve ao ligar, antes do `setup()`.
- **O GPIO não alimenta o buzzer diretamente, salvo se ele consumir menos de 12 mA.** Buzzers maiores precisam de um módulo ou transistor.

## Objetivos

1. Receber as linhas do TTeC sem perder dados enquanto o cartão estiver ocupado.
2. Gravar as linhas no cartão SD, sem corromper as que já foram salvas se o sistema falhar.
3. Sinalizar o funcionamento: o LED pisca a cada linha válida recebida e o buzzer bipa quando ocorre um erro.

## Formato dos arquivos

- **Nome:** `LOG0001.CSV`, `LOG0002.CSV` e assim por diante, sempre um número ainda não usado.
- **Criação:** o arquivo só é criado quando chega a primeira linha, e começa sempre pelo cabeçalho, mesmo que o OBDH tenha ligado depois do TTeC.
- **Novo arquivo:** o cabeçalho recebido do TTeC depois de já haver dados indica que ele reiniciou. Cada arquivo tem, portanto, uma única sequência de ids.
- **Conteúdo:** linhas de dados e logs `#` do TTeC, exatamente como recebidos, terminados em `\n`. Para ler com pandas, use `comment='#'`.
- **Linhas inválidas:** corrompidas ou longas demais são descartadas e contadas, nunca gravadas. A definição do formato em `ProjectConfig.h` precisa acompanhar `TTeC/Telemetry.cpp`.
- **Início da recepção:** tudo antes do primeiro `\n` é descartado, pois a linha pode estar cortada. Se as placas ligarem juntas, perde-se apenas o primeiro log do TTeC.
- **Reinício do TTeC:** os logs de boot chegam antes do cabeçalho e ficam no fim do arquivo anterior.
- **Queda de energia:** o flush ocorre a cada `kSdFlushPeriodMs`, então até 1 s de dados recentes pode ser perdido, e a última linha do arquivo pode ficar incompleta.

## Arquitetura

| Tarefa | Prioridade | Função |
|---|---|---|
| `receiver` | 4 | Monta e valida as linhas da UART e as coloca na fila. Nunca espera pelo SD |
| `indicators` | 3 | Observa as estatísticas e controla o LED e o buzzer |
| `sd` | 2 | Retira linhas da fila, grava, faz o flush e remonta o cartão após falhas |
| `status` | 1 | Envia estatísticas pela USB |

Falhas do cartão não interrompem a recepção. A linha que falhou é regravada em um novo arquivo depois de remontar o cartão. Se a fila encher, linhas inteiras são perdidas e contadas, e aparecem como salto no id.

## Sinalização

- **LED:** pulso de `kLedBlinkMs` a cada linha válida (dados, logs ou cabeçalho). Linhas descartadas não piscam o LED. Linhas que chegam juntas, na mesma leitura de `kReceiverPollPeriodMs`, geram uma só piscada.
- **Buzzer:** um bipe de `kBuzzerBeepMs` quando aumenta qualquer contador de erro: linha inválida, linha longa demais, linha perdida por fila cheia, falha de gravação ou falha de montagem.
- **Intervalo mínimo:** no máximo um bipe a cada `kBuzzerMinIntervalMs`. Erros nesse intervalo geram um único bipe ao final dele.
- **Sem cartão:** cada tentativa de montagem falha e gera um bipe, portanto o alarme se repete a cada `kSdRetryPeriodMs` até o cartão voltar.
- **Novos erros:** some o contador novo ao total em `indicatorTask`, em `RtosTasks.cpp`.

## Estrutura do código

- O projeto precisa compilar na Arduino IDE. O arquivo `.ino` deve ter o mesmo nome da pasta do sketch.
- Cada módulo tem uma classe própria, com arquivos `.h` e `.cpp` separados.
- O `.ino` apenas orquestra: inicializa os módulos e inicia as tarefas FreeRTOS, definidas em `RtosTasks.h/.cpp`. Os diagramas do sistema estão em `../ARQUITETURA.md`.
- Pinos, baud rates, períodos, prioridades e pilhas ficam centralizados em `ProjectConfig.h`.
- `Mailbox.h` é uma cópia do arquivo de mesmo nome do TTeC. Mantenha os dois iguais.

## Política de código embarcado

- **Memória:** sem alocação dinâmica no nosso código depois do `setup()` e sem a classe `String`. Use buffers de tamanho fixo e `snprintf`. Não use `Serial.printf`, que aloca memória em mensagens longas. O sistema de arquivos aloca internamente ao abrir arquivos.
- **Temporização:** sem `delay()` nas tarefas. Use `xTaskDelayUntil` para manter um período fixo.
- **Concorrência:** cada módulo pertence a uma única tarefa. Linhas passam pela fila, estatísticas passam por `Mailbox` e só a tarefa `status` usa `Serial` depois do `setup()`.
- **FreeRTOS:** tarefas, filas e mutexes usam alocação estática.
- **Falhas:** a inicialização retorna `bool`. A falta do cartão não interrompe o firmware, que tenta remontar periodicamente.
- **Erros de gravação:** use `fwrite` e `fsync` sobre `FILE*`, e confira os retornos. `File::flush()` descarta o erro do `fsync`.
- **Validade dos dados:** nada corrompido é gravado como se fosse dado real.
- **Tipos:** use tipos de largura fixa (`uint8_t`, `int16_t`) e `constexpr` em vez de `#define` para constantes.
- **Bibliotecas:** prefira as do pacote esp32 e bibliotecas consolidadas do Library Manager. Liste as usadas em comentário no `.ino`.

## Verificação

Sempre compile após alterar o código, se o `arduino-cli` estiver disponível:

```bash
arduino-cli compile --fqbn esp32:esp32:esp32doit-devkit-v1 .
```

Compilar não valida o comportamento. Deixe claro o que depende de teste no hardware físico.
