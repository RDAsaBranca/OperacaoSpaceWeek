// servidor_web.h - rede Wi-Fi propria + dashboard + API
//
// Rotas:
//   GET  /                        dashboard (pagina unica, guardada comprimida)
//   GET  /api/info                colunas, parametros do radio, modo simulacao
//   GET  /api/dados?desde=N       pacotes novos depois do indice N + estatisticas
//   POST /api/hora?epoch_ms=T     acerta o relogio da base (o dashboard faz sozinho)
//   POST /api/gravacao/iniciar    comeca a gravar na memoria da base
//   POST /api/gravacao/parar
//   POST /api/gravacao/apagar
//   GET  /baixar.csv              gravacao em CSV (com a gravacao parada)
#pragma once
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

void webIniciar(QueueHandle_t fila);
void webAtualizar();   // chamar em todo loop()
void webDrenarFila();  // move os pacotes da fila para o historico e a gravacao
