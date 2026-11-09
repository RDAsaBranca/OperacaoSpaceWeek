// recepcao.h - tarefa dedicada aos DOIS receptores
//
// Roda numa tarefa propria do FreeRTOS, com prioridade maior que o servidor
// web. Le o SX1276 (SPI) e o RYLR998 (UART), tira o cabecalho Reyax,
// decodifica e valida a telemetria, junta as duas copias de cada pacote
// (fusao.h) e poe um registro por pacote na fila do servidor web.
#pragma once
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include "Protocolo.h"
#include "fusao.h"
#include "reyax_rx.h"

struct ItemRecebido {
  PacoteTelemetria pacote;
  float    rssi;                   // dBm, da melhor copia (maior SNR)
  float    snr;                    // dB,  da melhor copia
  uint32_t tBaseMs;                // millis() da base quando chegou a primeira copia
  uint8_t  receptores;             // bit0 = SX1276, bit1 = RYLR998
  float    rssiRx[N_RECEPTORES];   // NAN se aquele receptor nao entregou
  float    snrRx[N_RECEPTORES];
};

struct EstatisticasRx {
  bool     radioOk;          // SX1276 iniciado
  bool     reyaxOk;          // RYLR998 respondeu e foi configurado
  bool     temPacote;        // ja chegou algum pacote valido
  uint32_t recebidos;        // pacotes fundidos (cada pacote conta uma vez)
  uint32_t perdidos;         // pelos saltos na sequencia, depois da fusao
  uint32_t errosCrc;         // SX1276: CRC do LoRa falhou
  uint32_t errosRadio;       // SX1276: outro erro (ex.: quadro sem CRC no header)
  uint32_t invalidos;        // SX1276: cabecalho Reyax, origem ou telemetria invalidos
  uint32_t invalidosReyax;   // RYLR998: +RCV de outra origem ou telemetria invalida
  uint32_t descartadosFila;  // fila cheia (servidor travado por muito tempo)
  uint32_t tUltimoMs;        // millis() do ultimo pacote valido
  EstatisticasFusao fusao;   // por receptor: entregues, exclusivos, atrasados
  EstatisticasReyax reyax;   // linhas +RCV, linhas ruins, +ERR, versao do firmware
};

bool           recepcaoIniciar(QueueHandle_t fila);
EstatisticasRx recepcaoEstatisticas();
