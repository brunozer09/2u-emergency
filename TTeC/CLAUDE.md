# Firmware ESP32: leitura e telemetria de sensores

Firmware para ESP32 DEVKIT V1, desenvolvido na Arduino IDE. Ele lê os sensores abaixo e transmite os dados pela UART2 (RX2/TX2).

## Hardware

| Componente | Interface | Pinos ESP32 | Observações |
|---|---|---|---|
| BME/BMP280 (barômetro) | I2C | SDA 21, SCL 22 | Endereço 0x76 ou 0x77. Chip ID: BMP280 = 0x58, BME280 = 0x60 (só o BME mede umidade) |
| MPU 9265 (IMU) | I2C | SDA 21, SCL 22 | Endereço 0x68 ou 0x69. Confirmar o chip real lendo o `WHO_AM_I` |
| GY-GPS6MV2 (GPS) | UART0 (`Serial`) | RX0 = GPIO3, TX0 = GPIO1 | Baud padrão 9600, envia NMEA a 1 Hz |
| Saída de telemetria | UART2 (`Serial2`) | RX2 = GPIO16, TX2 = GPIO17 | Para ler no PC, usar um adaptador USB-UART de 3,3 V |

## Restrições do hardware

- **`Serial` é exclusiva do GPS.** Nunca use `Serial.print` para depuração: o TX0 vai para o GPS. Todo log e toda telemetria saem por `Serial2`, e logs começam com `#` para não se misturarem ao CSV.
- **O conector USB da placa não mostra a telemetria.** Ele está ligado à UART0, não à UART2.
- **Desconecte o GPS do RX0 ao gravar o firmware.** A gravação usa a mesma UART0.
- **Sem bloqueio na leitura do GPS.** Esvazie o buffer da UART a cada iteração do `loop()` para não perder sentenças NMEA.

## Objetivos

1. Ler todos os sensores.
2. Transmitir os dados por `Serial2` em CSV, conforme o formato abaixo.

## Formato de saída (CSV)

Cada linha contém as leituras atuais dos sensores, precedidas por um id sequencial para depuração. O cabeçalho é enviado uma vez, após a inicialização:

```csv
id,temp_c,press_hpa,hum_pct,ax_g,ay_g,az_g,gx_dps,gy_dps,gz_dps,mx_ut,my_ut,mz_ut,lat_deg,lon_deg,alt_m,sats
```

- **`id`:** `uint32_t` que começa em 0 e aumenta 1 a cada linha transmitida. Saltos indicam linhas perdidas, e a volta a 0 indica reinício do firmware.
- **Dados inválidos:** campo vazio, mantendo todas as vírgulas (por exemplo, `hum_pct` no BMP280 ou a posição sem fix do GPS). Nunca use `0` ou o último valor válido.
- **Números:** ponto como separador decimal e casas decimais fixas por campo.
- **Linha:** termina em `\n` e é montada por inteiro em um buffer fixo antes do envio.

## Estrutura do código

- O projeto precisa compilar na Arduino IDE. O arquivo `.ino` deve ter o mesmo nome da pasta do sketch.
- Cada sensor tem uma classe própria, com arquivos `.h` e `.cpp` separados, responsável pela leitura e pelo armazenamento dos seus dados.
- O `.ino` apenas orquestra: inicializa os módulos, agenda as leituras e chama a transmissão.
- Pinos, endereços, baud rates e períodos de amostragem ficam centralizados em um único arquivo de configuração.

## Política de código embarcado

- **Memória:** sem alocação dinâmica depois do `setup()` e sem a classe `String`. Use buffers de tamanho fixo e `snprintf`.
- **Temporização:** sem `delay()` no `loop()`. Agende as tarefas com `millis()`.
- **Falhas:** a inicialização retorna `bool`. Um sensor ausente ou com falha é sinalizado na saída e não interrompe os demais.
- **Validade dos dados:** cada módulo informa se a leitura atual é válida. Dados inválidos, como GPS sem fix, nunca são transmitidos como se fossem reais.
- **Tipos:** use tipos de largura fixa (`uint8_t`, `int16_t`) e `constexpr` em vez de `#define` para constantes.
- **Bibliotecas:** prefira bibliotecas consolidadas e disponíveis no Library Manager. Liste as usadas em comentário no `.ino`.

## Verificação

Sempre compile após alterar o código, se o `arduino-cli` estiver disponível:

```bash
arduino-cli compile --fqbn esp32:esp32:esp32doit-devkit-v1 .
```

Compilar não valida o comportamento. Deixe claro o que depende de teste no hardware físico.
