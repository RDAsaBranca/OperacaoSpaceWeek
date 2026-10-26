// radio_rx.h - recepcao LoRa por interrupcao (sem perder pacotes no loop)
#pragma once
#include <Arduino.h>

enum ResultadoRx {
  RX_NADA = 0,   // nenhum pacote novo
  RX_OK,         // pacote recebido (ainda precisa de pacoteValidar)
  RX_ERRO_CRC,   // chegou corrompido (sinal fraco/interferencia)
  RX_ERRO,       // outro erro do radio
};

bool        radioIniciar();
ResultadoRx radioLer(uint8_t* buf, size_t maximo, size_t& len, float& rssi, float& snr);
