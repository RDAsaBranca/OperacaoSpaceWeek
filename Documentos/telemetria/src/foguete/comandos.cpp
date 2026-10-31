#include "comandos.h"
#include <Arduino.h>
#include <ctype.h>
#include <string.h>
#include "estado_voo.h"
#include "flash_w25q.h"
#include "registro_voo.h"
#include "sensores.h"

static char    linha[48];
static uint8_t tamLinha = 0;

static void ajuda() {
  Serial.println("# comandos: info | baixar | apagar confirmo | ajuda");
}

static void info() {
  Serial.printf("# estado do voo: %s\n", nomeEstado(estadoVooAtual()));
  sensoresImprimirResumo(Serial);
  if (flashTamanhoBytes() == 0) {
    Serial.printf("# flash: NAO ENCONTRADA (JEDEC 0x%06lX)\n", (unsigned long)flashJedecId());
    return;
  }
  uint8_t sr[3];
  flashLerStatus(sr);
  Serial.printf("# flash: status SR1=0x%02X SR2=0x%02X SR3=0x%02X\n", sr[0], sr[1], sr[2]);
  const uint32_t usados = registroUsados(), cap = registroCapacidade();
  Serial.printf("# flash: JEDEC 0x%06lX, %lu de %lu registros (%.1f%%), sessao %u, %s\n",
                (unsigned long)flashJedecId(), (unsigned long)usados, (unsigned long)cap,
                cap ? 100.0f * usados / cap : 0.0f, registroSessao(),
                (registroStatus() & STATUS_FLASH_OK) ? "gravando" : "PARADA");
}

static void executar(const char* cmd) {
  if (cmd[0] == '\0') return;
  const EstadoVoo e = estadoVooAtual();
  if (e == ESTADO_SUBIDA || e == ESTADO_DESCIDA) {
    Serial.println("# comandos ignorados durante o voo");
    return;
  }
  if (strcmp(cmd, "info") == 0)                 info();
  else if (strcmp(cmd, "baixar") == 0)          registroBaixarCsv(Serial);
  else if (strcmp(cmd, "apagar confirmo") == 0) registroApagarTudo(Serial);
  else if (strcmp(cmd, "apagar") == 0)          Serial.println("# para apagar TUDO, digite: apagar confirmo");
  else if (strcmp(cmd, "ajuda") == 0)           ajuda();
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
