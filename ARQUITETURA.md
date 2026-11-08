# Arquitetura do sistema

Visão geral das placas e da estrutura de software de cada firmware. Os diagramas usam Mermaid, que o GitHub desenha diretamente.

## Visão geral: as quatro placas

```mermaid
flowchart LR
    EPS["<b>EPS</b><br/>energia<br/><i>sem firmware no repositório</i>"]
    TTeC["<b>TTeC</b><br/>sensores e telemetria<br/>pasta TTeC/"]
    OBDH["<b>OBDH</b><br/>registro em cartão SD<br/>pasta OBDH/"]
    PAYLOAD["<b>PAYLOAD</b><br/>carga útil<br/><i>sem firmware no repositório</i>"]

    TTeC == "UART2 · CSV a 10 Hz" ==> OBDH
    PAYLOAD -. "dados: interface a definir" .-> OBDH
    EPS -. "alimentação: a definir" .-> TTeC
    EPS -. "alimentação: a definir" .-> OBDH
    EPS -. "alimentação: a definir" .-> PAYLOAD
```

- **Linha grossa:** ligação implementada. **Tracejada:** ainda não definida no código.
- **Bateria:** o INA219 que mede a bateria está ligado ao I2C do TTeC, e os dados chegam ao OBDH nas colunas `bat_v`, `bat_ma` e `bat_mw`.
- **Portas livres no OBDH:** a UART2 recebe o TTeC e a UART0 é o console USB. Sobram a UART1 e o barramento SPI, que já é usado pelo cartão SD com CS no GPIO5. Definir as interfaces do EPS e do PAYLOAD exige escolher entre essas opções.

## TTeC: fluxo de dados

```mermaid
flowchart LR
    subgraph HW["Hardware"]
        gpsHw["GY-GPS6MV2<br/>UART0 · 9600 bd"]
        imuHw["MPU 9265<br/>I2C 0x68 / 0x69"]
        baroHw["BME/BMP280<br/>I2C 0x76 / 0x77"]
        inaHw["INA219<br/>I2C 0x40"]
    end

    subgraph TASKS["Tarefas no núcleo 1"]
        gpsTask["<b>gps</b> · Gps<br/>P5 · 50 ms"]
        imuTask["<b>imu</b> · Imu<br/>P4 · 100 ms"]
        baroTask["<b>baro</b> · Barometer<br/>P3 · 100 ms"]
        batTask["<b>bat</b> · BatteryMonitor<br/>P2 · 200 ms"]
    end

    subgraph MAIL["Mailbox, 1 posição"]
        gpsMb(["GpsData"])
        imuMb(["ImuData"])
        baroMb(["BarometerData"])
        batMb(["BatteryData"])
    end

    telemetry["<b>telemetry</b> · Telemetry<br/>P1 · 100 ms"]
    serialOut["Serial2 · 115200 bd<br/>para o OBDH"]

    gpsHw --> gpsTask
    imuHw -- "I2C + mutex" --> imuTask
    baroHw -- "I2C + mutex" --> baroTask
    inaHw -- "I2C + mutex" --> batTask

    gpsTask --> gpsMb
    imuTask --> imuMb
    baroTask --> baroMb
    batTask --> batMb

    gpsMb --> telemetry
    imuMb --> telemetry
    baroMb --> telemetry
    batMb -- "readFresh: mais de 500 ms vira campo vazio" --> telemetry

    telemetry -- "CSV e logs" --> serialOut
```

- As tarefas de leitura nunca esperam a telemetria: cada uma sobrescreve a sua `Mailbox`.
- O mutex I2C cobre o `sample()` inteiro de cada sensor, aplicado por `runI2cSensor`.
- Só a tarefa `telemetry` escreve na `Serial2`. Detalhes em `TTeC/FREERTOS.md`.

## OBDH: fluxo de dados

```mermaid
flowchart LR
    linkIn["Serial2 · 115200 bd<br/>vindo do TTeC"]
    receiver["<b>receiver</b> · LineReceiver<br/>P4 · 10 ms"]
    queue[["lineQueue<br/>64 linhas inteiras"]]
    sd["<b>sd</b> · SdLogger<br/>P2 · a cada linha"]
    card["microSD · SPI<br/>LOGnnnn.CSV"]

    rxStats(["ReceiverStats"])
    sdStats(["SdStats"])

    indicators["<b>indicators</b> · StatusLed, Buzzer<br/>P3 · 10 ms"]
    status["<b>status</b> · Console<br/>P1 · 10 s"]

    led["LED · GPIO 2<br/>pisca a cada linha"]
    buzzer["Buzzer · GPIO 15<br/>bipe em erro"]
    usb["USB Serial<br/>contadores e pilhas"]

    linkIn --> receiver
    receiver -- "fila cheia: linha descartada" --> queue
    queue --> sd
    sd -- "fsync a cada 1 s" --> card

    receiver --> rxStats
    sd --> sdStats
    rxStats --> indicators
    sdStats --> indicators
    rxStats --> status
    sdStats --> status

    indicators --> led
    indicators --> buzzer
    status --> usb
```

- A recepção nunca espera o cartão: a demora de gravação fica na tarefa `sd`.
- Uma fila cheia descarta linhas inteiras, que aparecem como salto no `id`.
- LED, buzzer e console só leem as estatísticas e não atrasam a recepção nem a gravação.

## Camadas de código

Cada camada chama apenas a de baixo. Só `RtosTasks.cpp` e `Mailbox.h` incluem cabeçalhos do FreeRTOS, então as classes dos módulos não sabem que existem tarefas.

| Camada | TTeC | OBDH |
|---|---|---|
| Orquestração | `TTeC.ino` | `OBDH.ino` |
| Tarefas e troca de dados | `RtosTasks.h/.cpp`, `Mailbox.h` | `RtosTasks.h/.cpp`, `Mailbox.h` (mesmo arquivo) |
| Módulos | `Gps`, `Imu`, `Barometer`, `BatteryMonitor`, `Telemetry` | `LineReceiver`, `SdLogger`, `StatusLed`, `Buzzer`, `Console` |
| Bibliotecas | TinyGPSPlus, Adafruit BMP280, BME280 e INA219, `Wire` | `SD`, `SPI`, LEDC, todas do pacote esp32 |
| Configuração | `ProjectConfig.h` | `ProjectConfig.h` |

## Contrato entre TTeC e OBDH

O formato da linha é a única dependência entre os dois projetos. O TTeC monta a linha em `Telemetry.cpp`, e o OBDH confere cada linha com `kCsvHeader` e `kCsvColumns` no seu `ProjectConfig.h`:

```csv
id,temp_c,press_hpa,hum_pct,ax_g,ay_g,az_g,gx_dps,gy_dps,gz_dps,mx_ut,my_ut,mz_ut,lat_deg,lon_deg,alt_m,sats,bat_v,bat_ma,bat_mw
```

Ao mudar uma coluna, altere os dois lados e grave as duas placas.
