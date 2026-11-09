// radio_tx.h - transmissao LoRa NAO bloqueante (mesma logica do foguete)
//
// Igual a ../../src/foguete/radio_tx.*, com duas diferencas: os parametros
// vem de ConfigEnlace.h (compativeis com o RYLR998) e a camada fisica que o
// RYLR998 espera fica explicita (header explicito, CRC, IQ normal, LDRO).
#pragma once
#include <Arduino.h>

bool     radioIniciar();                                // pode ser chamada de novo apos falha
bool     radioPronto();
bool     radioEnviar(const uint8_t* dados, size_t len); // false se sem radio, ocupado ou erro
void     radioAtualizar();                              // chamar em todo loop()
uint32_t radioTempoNoArUs(size_t len);
void     radioImprimirDiagnostico(Print& saida);

struct ContadoresTx {
  uint32_t enviados;   // transmissoes concluidas (DIO0 avisou o fim)
  uint32_t pulados;    // pacote descartado porque o radio ainda transmitia
  uint32_t errosTx;    // startTransmit() recusou
  uint32_t timeouts;   // DIO0 nao avisou o fim a tempo
};
ContadoresTx radioContadores();
