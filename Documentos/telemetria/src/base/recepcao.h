// recepcao.h - tarefa dedicada ao receptor LoRa (SX1276 ou RYLR998)
//
// Roda numa tarefa propria do FreeRTOS, com prioridade maior que o
// servidor web. Assim, mesmo durante um download longo no dashboard,
// nenhum pacote do foguete deixa de ser lido. Le o receptor escolhido em
// config_base.h, tira o cabecalho Reyax, decodifica e valida a telemetria.
// Os pacotes validos vao para uma fila, de onde o servidor web os retira.
#pragma once
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include "Protocolo.h"
#include "reyax_rx.h"

struct ItemRecebido {
  PacoteTelemetria pacote;
  float    rssi;     // dBm
  float    snr;      // dB
  uint32_t tBaseMs;  // millis() da base quando o pacote chegou
};

struct EstatisticasRx {
  bool     radioOk;          // receptor iniciado (ou modo simulacao)
  bool     temPacote;        // ja chegou algum pacote valido
  uint32_t recebidos;
  uint32_t perdidos;         // pelos saltos na sequencia
  uint32_t errosCrc;         // SX1276: CRC do LoRa falhou (o RYLR998 descarta sozinho)
  uint32_t errosRadio;       // SX1276: outro erro (ex.: quadro sem CRC no header)
  uint32_t invalidos;        // cabecalho Reyax, origem, versao ou CRC proprio invalidos
  uint32_t descartadosFila;  // fila cheia (servidor travado por muito tempo)
  uint32_t tUltimoMs;        // millis() do ultimo pacote valido
  EstatisticasReyax reyax;   // RYLR998: linhas +RCV, linhas ruins, +ERR, versao (zero no SX1276)
};

bool           recepcaoIniciar(QueueHandle_t fila);
EstatisticasRx recepcaoEstatisticas();
