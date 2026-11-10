#include "comandos.h"
#include <Arduino.h>
#include <ctype.h>
#include <string.h>
#include "estado_voo.h"
#include "radio_tx.h"
#include "sensores.h"

static char    linha[48];
static uint8_t tamLinha = 0;

static void ajuda() {
  Serial.println("# comandos: info | ajuda");
}

static void info() {
  Serial.printf("# estado do voo: %s\n", nomeEstado(estadoVooAtual()));
  sensoresImprimirResumo(Serial);
  Serial.print("# ");
  radioImprimirDiagnostico(Serial);
  radioImprimirParametros(Serial);
}

static void executar(const char* cmd) {
  if (cmd[0] == '\0') return;
  const EstadoVoo e = estadoVooAtual();
  if (e == ESTADO_SUBIDA || e == ESTADO_DESCIDA) {
    Serial.println("# comandos ignorados durante o voo");
    return;
  }
  if (strcmp(cmd, "info") == 0)       info();
  else if (strcmp(cmd, "ajuda") == 0) ajuda();
  else {
    Serial.printf("# comando desconhecido: %s\n", cmd);
    ajuda();
  }
}

void comandosAtualizar() {
  while (Serial.available()) {
    const char c = (char)Serial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      linha[tamLinha] = '\0';
      executar(linha);
      tamLinha = 0;
    } else if (tamLinha < sizeof(linha) - 1) {
      linha[tamLinha++] = (char)tolower((unsigned char)c);
    }
  }
}
