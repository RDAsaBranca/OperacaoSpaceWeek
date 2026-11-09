// radio_rx.h - receptor 1: SX1276 por interrupcao (sem perder pacotes no loop)
#pragma once
#include <Arduino.h>

enum ResultadoRx {
  RX_NADA = 0,   // nenhum pacote novo
  RX_OK,         // quadro recebido (ainda precisa tirar o cabecalho e validar)
  RX_ERRO_CRC,   // chegou corrompido (sinal fraco/interferencia)
  RX_ERRO,       // outro erro (ex.: quadro sem CRC no header LoRa)
};

bool        radioIniciar();
ResultadoRx radioLer(uint8_t* buf, size_t maximo, size_t& len, float& rssi, float& snr);
