// Configuração central do firmware: pinos, enlace, cartão SD, períodos e tarefas.
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace config {

// Enlace com o TTeC pela UART2 (Serial2)
constexpr int kLinkRxPin = 16;
constexpr int kLinkTxPin = 17;  // Reservado: o OBDH ainda não envia nada ao TTeC
constexpr uint32_t kLinkBaud = 115200;
constexpr uint16_t kLinkRxBufferBytes = 2048;

// Formato de linha do TTeC. Precisa ser igual ao definido em TTeC/Telemetry.cpp.
constexpr char kCsvHeader[] =
    "id,temp_c,press_hpa,hum_pct,ax_g,ay_g,az_g,gx_dps,gy_dps,gz_dps,"
    "mx_ut,my_ut,mz_ut,lat_deg,lon_deg,alt_m,sats,bat_v,bat_ma,bat_mw";
constexpr uint8_t kCsvColumns = 20;
constexpr size_t kLineBytes = 192;  // Mesmo limite do TTeC, com o terminador

// Cartão SD no barramento VSPI
constexpr int kSdCsPin = 5;
constexpr int kSdSckPin = 18;
constexpr int kSdMisoPin = 19;
constexpr int kSdMosiPin = 23;
constexpr uint32_t kSdClockHz = 4000000;  // Baixo de propósito: tolera fios longos
constexpr char kSdMountPoint[] = "/sd";
constexpr uint8_t kSdMaxOpenFiles = 1;
constexpr uint16_t kSdMaxFileIndex = 9999;  // Arquivos LOG0001.CSV a LOG9999.CSV

// Gravação no SD
constexpr uint32_t kSdFlushPeriodMs = 1000;  // Perda máxima em queda de energia
constexpr uint32_t kSdRetryPeriodMs = 2000;  // Espera antes de remontar o cartão
constexpr uint32_t kSdIdleWaitMs = 100;      // Espera máxima por linhas antes de checar o flush

// Linhas à espera de gravação: a 10 Hz, 64 linhas cobrem 6,4 s de SD ocupado
constexpr size_t kLineQueueDepth = 64;

// Log de estado pela USB (Serial)
constexpr uint32_t kConsoleBaud = 115200;
constexpr uint32_t kStatusPeriodMs = 10000;
constexpr uint32_t kStatsMaxAgeMs = 5000;  // Estatística mais antiga que isso aparece zerada
constexpr uint32_t kReceiverPollPeriodMs = 10;

// Interface humana. GPIO2 e GPIO15 são pinos de boot: ver as restrições em CLAUDE.md.
constexpr int kLedPin = 2;  // Mesmo pino do LED da placa
constexpr uint32_t kLedBlinkMs = 30;  // Curto o bastante para separar linhas a 10 Hz
constexpr int kBuzzerPin = 15;
constexpr bool kBuzzerIsPassive = false;  // true para buzzer sem oscilador interno
constexpr uint32_t kBuzzerToneHz = 2000;  // Usado apenas pelo buzzer passivo
constexpr uint32_t kBuzzerBeepMs = 100;
constexpr uint32_t kBuzzerMinIntervalMs = 1000;
constexpr uint32_t kIndicatorPeriodMs = 10;

// Tarefas FreeRTOS. Sem Wi-Fi ou Bluetooth, todas ficam no núcleo do Arduino.
constexpr int kTaskCore = 1;

// Maior número = mais urgente. A recepção vem primeiro para o SD nunca atrasá-la, e os
// indicadores ficam acima do SD para um cartão lento não alongar o bipe.
constexpr uint32_t kReceiverTaskPriority = 4;
constexpr uint32_t kIndicatorTaskPriority = 3;
constexpr uint32_t kSdTaskPriority = 2;
constexpr uint32_t kStatusTaskPriority = 1;

// Pilhas com folga: conferir no hardware com o log "pilha livre" antes de reduzir
constexpr uint32_t kReceiverTaskStackBytes = 3072;
constexpr uint32_t kIndicatorTaskStackBytes = 3072;
constexpr uint32_t kSdTaskStackBytes = 6144;
constexpr uint32_t kStatusTaskStackBytes = 3072;

}  // namespace config
