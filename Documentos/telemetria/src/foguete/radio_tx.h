// radio_tx.h - transmissao LoRa NAO bloqueante (o loop continua lendo sensores)
//
// Se o SX1276 nao responder, nada trava: radioPronto() fica false, os
// pacotes simplesmente nao sao enviados e o main.cpp tenta iniciar o radio
// de novo de tempos em tempos.
//
// Parametros e camada fisica compativeis com o REYAX RYLR998 (ConfigLoRa.h):
// header explicito, CRC, IQ normal. O que vai no ar e o quadro montado por
// quadroTelemetria() (QuadroReyax.h), nao a struct crua.
#pragma once
#include <Arduino.h>

bool     radioIniciar();                               // pode ser chamada de novo apos falha
bool     radioPronto();                                // true depois de um radioIniciar() bem-sucedido
bool     radioEnviar(const uint8_t* dados, size_t len); // false se sem radio, ocupado ou erro
void     radioAtualizar();                             // chamar em todo loop()
uint32_t radioTempoNoArUs(size_t len);
void     radioImprimirParametros(Print& saida);        // enlace, tamanho do quadro e tempo no ar
void     radioImprimirDiagnostico(Print& saida);       // uma linha [LoRa] para o monitor serial
