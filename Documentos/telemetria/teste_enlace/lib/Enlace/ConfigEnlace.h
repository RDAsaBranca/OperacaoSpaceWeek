// ConfigEnlace.h - parametros de radio do TESTE DO ENLACE
//
// Iguais nos tres radios: SX1276 do CanSat simulado, SX1276 da base e
// RYLR998 da base (docs/arquitetura_enlace_reyax.md, secao 2).
//
// NAO inclua junto com ../lib/Telemetria/ConfigLoRa.h: aquele e o do
// firmware atual (sync word 0x2B, preambulo 8), que o RYLR998 nao ouve.
#pragma once

#if defined(LORA_SYNC_WORD)
#error "ConfigLoRa.h (parametros do firmware atual) incluido junto com ConfigEnlace.h"
#endif

// Frequencia central. 915,2 MHz: os 125 kHz cabem inteiros na faixa de
// 915-928 MHz (915,0 MHz invade 907,5-915 MHz, fora da radiacao restrita).
// O RYLR998 recebe em Hz (AT+BAND), o RadioLib em MHz.
#define LORA_FREQ_HZ            915200000UL
#define LORA_FREQ_MHZ           (LORA_FREQ_HZ / 1.0e6f)

#define LORA_BW_KHZ             125.0f  // RYLR998: 125 (SF7-9), 250 (SF7-10) ou 500 kHz (SF7-11)
#define LORA_SF                 7
#define LORA_CR                 5       // 4/5. No RYLR998: <Coding Rate> = LORA_CR - 4 = 1
#define LORA_SYNC_WORD          0x12    // = AT+NETWORKID=18 (0x12 == 18). Unico valor medido
#define LORA_PREAMBULO          12      // padrao do RYLR998; unico aceito se NETWORKID != 18
#define LORA_POTENCIA_DBM       17      // CanSat: 2 a 17 dBm (PA_BOOST). A base nao transmite
#define LORA_LIMITE_CORRENTE_MA 120     // protecao de sobrecorrente do amplificador

// Intervalo entre pacotes (ms). Precisa ser maior que o tempo no ar do
// quadro: 93 bytes (cabecalho + Base64) em SF7/125 kHz = ~168 ms.
#define TLM_PERIODO_MS          200

// A base considera "sem sinal" apos esse tempo sem pacotes validos.
#define TLM_TIMEOUT_SINAL_MS    2000
