// ============================================================
//  ESTACAO BASE - ESP32 + SX1276
//
//  Duas tarefas em paralelo:
//   - recepcao (prioridade alta): le o radio, valida, conta perdas
//   - loop() (prioridade normal): rede Wi-Fi propria, dashboard,
//     historico e gravacao sob demanda na memoria interna
//
//  Conecte na rede "EstacaoBase-Foguete" e abra http://192.168.4.1
// ============================================================
#include <Arduino.h>
#include "config_base.h"
#include "gravacao.h"
#include "recepcao.h"
#include "saida.h"
#include "servidor_web.h"

static QueueHandle_t fila = nullptr;

void setup() {
  Serial.begin(115200);
  delay(200);
  saidaCabecalho();

  if (!gravacaoIniciarSistema()) {
    Serial.println("# [GRAVACAO] falha ao montar a memoria interna (LittleFS)");
  }

  fila = xQueueCreate(FILA_TAM, sizeof(ItemRecebido));

#if SIMULAR_FOGUETE
  Serial.println("# [BASE] MODO SIMULACAO: voo ficticio, radio desligado");
#endif
  webIniciar(fila);        // o dashboard abre mesmo se o radio falhar
  recepcaoIniciar(fila);   // a tarefa inicia o radio e tenta de novo se precisar
}

void loop() {
  webAtualizar();
  delay(2);
}
