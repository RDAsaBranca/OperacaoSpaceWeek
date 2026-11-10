// ConfigLoRa.h
// Parametros de radio COMPARTILHADOS entre foguete e base.
// Os dois lados precisam usar exatamente os mesmos valores,
// senao um nao "ouve" o outro. Por isso ficam num lugar so.
//
// Valores compativeis com o REYAX RYLR998 (docs/arquitetura_enlace_reyax.md,
// secao 2), validados em bancada com teste_enlace/ (cansat_sim -> base_dupla).
// Sao os mesmos de teste_enlace/lib/Enlace/ConfigEnlace.h: o foguete fala
// com a base (SX1276 ou RYLR998) e com a base_dupla. Mudou algo aqui? Mude
// la tambem. O AT+ do RYLR998 sai destes valores (src/base/reyax_rx.cpp).
#pragma once

// Frequencia central. 915,2 MHz: os 125 kHz cabem inteiros na faixa de
// 915-928 MHz (915,0 MHz invade 907,5-915 MHz, fora da radiacao restrita).
// O RYLR998 recebe em Hz (AT+BAND), o RadioLib em MHz.
// Modulo de 433 MHz (ex.: Ra-02, que usa SX1278)? Troque aqui e no RYLR998.
#define LORA_FREQ_HZ            915200000UL
#define LORA_FREQ_MHZ           (LORA_FREQ_HZ / 1.0e6f)

#define LORA_BW_KHZ             125.0f  // RYLR998: 125 (SF7-9), 250 (SF7-10) ou 500 kHz (SF7-11)
#define LORA_SF                 7       // spreading factor: 7 (rapido) ... 9 (mais alcance a 125 kHz)
#define LORA_CR                 5       // coding rate 4/5. No RYLR998: <Coding Rate> = LORA_CR - 4 = 1
#define LORA_SYNC_WORD          0x12    // = AT+NETWORKID=18 (0x12 == 18). Unico valor medido
#define LORA_PREAMBULO          12      // padrao do RYLR998; unico aceito se NETWORKID != 18
#define LORA_POTENCIA_DBM       17      // 2 a 17 dBm (PA_BOOST). 20 e possivel, com cuidado
#define LORA_LIMITE_CORRENTE_MA 120     // protecao de sobrecorrente do amplificador

// Intervalo entre pacotes de telemetria (ms). Precisa ser MAIOR que o
// tempo no ar do quadro (o foguete imprime esse valor ao ligar):
// 93 bytes (cabecalho Reyax + Base64) em SF7/125 kHz = ~168 ms.
#define TLM_PERIODO_MS          200

// A base considera "sem sinal" apos esse tempo sem pacotes validos.
#define TLM_TIMEOUT_SINAL_MS    2000
