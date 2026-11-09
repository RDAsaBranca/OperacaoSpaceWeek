// config_base.h - configuracoes da estacao base do TESTE (dois receptores)
#pragma once

// ---- Wi-Fi: a base cria a propria rede (nao precisa de internet) ----
// Nome diferente da base de voo para nao confundir as duas na bancada.
// Conecte o celular/notebook nesta rede e abra http://192.168.4.1
#define WIFI_NOME          "EstacaoBase-Teste"
#define WIFI_SENHA         "foguete123"   // minimo de 8 caracteres
#define WIFI_CANAL         6
#define WIFI_MAX_CLIENTES  4

// ---- Historico em memoria ----
// Quem abre o dashboard no meio do teste recebe os ultimos pacotes daqui.
#define HISTORICO_TAM        600   // 2 minutos a 5 Hz
#define MAX_LINHAS_RESPOSTA  60    // pacotes por requisicao ao /api/dados
#define FILA_TAM             64    // pacotes entre a tarefa do radio e o servidor (~13 s)

// ---- Fusao dos dois receptores ----
// Tempo que o primeiro receptor espera pela copia do outro antes de o
// pacote sair. Menor que TLM_PERIODO_MS. Ajuste com o teste T5 do documento.
#define JANELA_FUSAO_MS      80
