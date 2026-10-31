// config_base.h - configuracoes da estacao base
#pragma once

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
