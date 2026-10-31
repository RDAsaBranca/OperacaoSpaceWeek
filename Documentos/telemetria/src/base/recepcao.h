// recepcao.h - tarefa dedicada ao radio LoRa
//
// Roda numa tarefa propria do FreeRTOS, com prioridade maior que o
// servidor web. Assim, mesmo durante um download longo no dashboard,
// nenhum pacote do foguete deixa de ser lido. Os pacotes validos vao para
// uma fila, de onde o servidor web os retira.
#pragma once
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include "Protocolo.h"

struct ItemRecebido {
  PacoteTelemetria pacote;
  float    rssi;     // dBm
  float    snr;      // dB
  uint32_t tBaseMs;  // millis() da base quando o pacote chegou
};

struct EstatisticasRx {
  bool     radioOk;          // SX1276 iniciado (ou modo simulacao)
  bool     temPacote;        // ja chegou algum pacote valido
  uint32_t recebidos;
  uint32_t perdidos;         // pelos saltos na sequencia
  uint32_t errosCrc;
  uint32_t invalidos;        // de outra rede ou versao diferente
  uint32_t descartadosFila;  // fila cheia (servidor travado por muito tempo)
  uint32_t tUltimoMs;        // millis() do ultimo pacote valido
};

bool           recepcaoIniciar(QueueHandle_t fila);
EstatisticasRx recepcaoEstatisticas();
