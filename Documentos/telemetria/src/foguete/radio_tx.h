// radio_tx.h - transmissao LoRa NAO bloqueante (o loop continua lendo sensores)
//
// Se o SX1276 nao responder, nada trava: radioPronto() fica false, os
// pacotes simplesmente nao sao enviados e o main.cpp tenta iniciar o radio
// de novo de tempos em tempos.
#pragma once
#include <Arduino.h>

bool     radioIniciar();                               // pode ser chamada de novo apos falha
bool     radioPronto();                                // true depois de um radioIniciar() bem-sucedido
bool     radioEnviar(const uint8_t* dados, size_t len); // false se sem radio, ocupado ou erro
void     radioAtualizar();                             // chamar em todo loop()
uint32_t radioTempoNoArUs(size_t len);
void     radioImprimirDiagnostico(Print& saida);       // uma linha [LoRa] para o monitor serial
