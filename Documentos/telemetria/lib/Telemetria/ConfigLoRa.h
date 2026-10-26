// ConfigLoRa.h
// Parametros de radio COMPARTILHADOS entre foguete e base.
// Os dois lados precisam usar exatamente os mesmos valores,
// senao um nao "ouve" o outro. Por isso ficam num lugar so.
#pragma once

// Frequencia central em MHz. Use um valor compativel com o SEU modulo:
//  - RFM95W / modulos SX1276 de 868/915 MHz -> 915.0 (faixa usual no Brasil)
//  - modulos de 433 MHz (ex.: Ra-02, que usa SX1278) -> 433.0
#define LORA_FREQ_MHZ           915.0f

#define LORA_BW_KHZ             125.0f  // largura de banda: 125, 250 ou 500
#define LORA_SF                 7       // spreading factor: 7 (rapido) ... 12 (mais alcance)
#define LORA_CR                 5       // coding rate 4/5
#define LORA_SYNC_WORD          0x2B    // "nome da rede" (evite 0x34, que e do LoRaWAN)
#define LORA_POTENCIA_DBM       17      // 2 a 17 dBm (PA_BOOST). 20 e possivel, com cuidado
#define LORA_PREAMBULO          8
#define LORA_LIMITE_CORRENTE_MA 120     // protecao de sobrecorrente do amplificador

// Intervalo entre pacotes de telemetria (ms). Precisa ser MAIOR que o
// tempo no ar do pacote (o foguete imprime esse valor ao ligar).
#define TLM_PERIODO_MS          200

// A base considera "sem sinal" apos esse tempo sem pacotes validos.
#define TLM_TIMEOUT_SINAL_MS    2000
