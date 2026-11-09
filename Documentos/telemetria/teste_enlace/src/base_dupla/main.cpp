// ============================================================
//  CODIGO 2 - ESTACAO BASE DO TESTE: SX1276 + RYLR998
//
//  Faz o mesmo que a estacao base de voo (../../src/base), com duas
//  diferencas:
//   - dois receptores ouvindo o MESMO quadro do CanSat (Codigo 1):
//       SX1276 (SPI)   recebe o quadro inteiro e tira o cabecalho Reyax;
//       RYLR998 (UART) o proprio modulo tira o cabecalho e entrega +RCV;
//   - fusao: cada pacote sai uma vez so, com RSSI/SNR de cada receptor.
//
//  Duas tarefas em paralelo:
//   - recepcao (prioridade alta): le os dois radios, valida, funde, conta perdas
//   - loop() (prioridade normal): Wi-Fi proprio, dashboard, historico, gravacao
//
//  Conecte na rede "EstacaoBase-Teste" e abra http://192.168.4.1
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

  webIniciar(fila);        // o dashboard abre mesmo se os radios falharem
  recepcaoIniciar(fila);   // a tarefa inicia os dois radios e tenta de novo se precisar
}

void loop() {
  webAtualizar();
  delay(2);
}
