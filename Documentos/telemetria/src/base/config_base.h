// config_base.h - configuracoes da estacao base
#pragma once

// ---- Receptor: a base tem UM modulo LoRa, escolhido na compilacao ----
// O ambiente do platformio.ini define RECEPTOR_BASE:
//   pio run -e base        -> SX1276 no SPI (tira o cabecalho Reyax no firmware)
//   pio run -e base_reyax  -> REYAX RYLR998 na UART (o modulo tira o cabecalho)
//   pio run -e base_heltec -> SX1276 da Heltec WiFi LoRa 32 V2 (pinos em pinos.h)
// Os dois recebem o mesmo quadro do foguete (lib/Telemetria/QuadroReyax.h).
// Para ouvir com os dois ao mesmo tempo, use a base_dupla do teste_enlace/.
#define RECEPTOR_SX1276   1
#define RECEPTOR_RYLR998  2
#ifndef RECEPTOR_BASE
#define RECEPTOR_BASE RECEPTOR_SX1276
#endif
#if RECEPTOR_BASE == RECEPTOR_SX1276
#define RECEPTOR_NOME "SX1276"
#elif RECEPTOR_BASE == RECEPTOR_RYLR998
#define RECEPTOR_NOME "RYLR998"
#else
#error "RECEPTOR_BASE precisa ser RECEPTOR_SX1276 ou RECEPTOR_RYLR998"
#endif

// ---- Wi-Fi: a base cria a propria rede (nao precisa de internet) ----
// Conecte o celular/notebook nesta rede e abra http://192.168.4.1
#define WIFI_NOME          "EstacaoBase-Foguete"
#define WIFI_SENHA         "foguete123"   // minimo de 8 caracteres. TROQUE antes da missao.
#define WIFI_CANAL         6
#define WIFI_MAX_CLIENTES  4

// ---- Historico em memoria ----
// Quem abre o dashboard no meio do voo recebe os ultimos pacotes daqui.
#define HISTORICO_TAM        600   // 2 minutos a 5 Hz
#define MAX_LINHAS_RESPOSTA  60    // pacotes por requisicao ao /api/dados
#define FILA_TAM             64    // pacotes entre a tarefa do radio e o servidor (~13 s)

// ---- Simulacao ----
// 1 = gera um voo ficticio em vez de usar o radio. Serve para testar o
// dashboard so com a ESP32, sem foguete e sem modulo LoRa.
#ifndef SIMULAR_FOGUETE
#define SIMULAR_FOGUETE 0
#endif

// ---- Gravacao automatica ----
// 1 = a gravacao na memoria da base comeca sozinha no primeiro pacote valido
// que o receptor entrega depois de ligar a base (pacote com CRC certo e
// vindo do CanSat; ruido e quadros de outros nao contam). O botao "Parar
// gravacao" desliga o inicio automatico ate a base ser religada.
// Na simulacao fica desligada, para o voo ficticio nao se misturar com os
// dados reais na memoria (o botao "Iniciar gravacao" continua funcionando).
#ifndef GRAVAR_AO_RECEBER
#define GRAVAR_AO_RECEBER (!SIMULAR_FOGUETE)
#endif
