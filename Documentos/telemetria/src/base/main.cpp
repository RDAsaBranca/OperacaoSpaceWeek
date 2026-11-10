// ============================================================
//  ESTACAO BASE - ESP32 + um receptor LoRa: SX1276 OU REYAX RYLR998
//
//  O receptor e escolhido pelo ambiente (config_base.h, RECEPTOR_BASE):
//    pio run -e base        -> SX1276 (SPI): recebe o quadro inteiro e
//                              tira o cabecalho Reyax no firmware
//    pio run -e base_reyax  -> RYLR998 (UART): o modulo tira o cabecalho
//                              e entrega +RCV
//  Os dois saem com o mesmo formato (USB, dashboard, gravacao). Para ouvir
//  com os dois ao mesmo tempo, use a base_dupla do teste_enlace/.
//
//  Duas tarefas em paralelo:
//   - recepcao (prioridade alta): le o receptor, valida, conta perdas
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
  Serial.println("# [BASE] MODO SIMULACAO: voo ficticio, receptor desligado");
#endif
  webIniciar(fila);        // o dashboard abre mesmo se o receptor falhar
  recepcaoIniciar(fila);   // a tarefa inicia o receptor e tenta de novo se precisar
}

void loop() {
  webAtualizar();
  delay(2);
}
