// radio_tx.h - transmissao LoRa NAO bloqueante (o loop continua lendo sensores)
#pragma once
#include <Arduino.h>

bool     radioIniciar();
bool     radioOcupado();                              // true durante uma transmissao
bool     radioEnviar(const uint8_t* dados, size_t len); // false se ocupado ou erro
void     radioAtualizar();                            // chamar em todo loop()
uint32_t radioTempoNoArUs(size_t len);
