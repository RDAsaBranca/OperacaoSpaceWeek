// ============================================================
//  ESTACAO BASE - ESP32 + SX1276
//  Recebe, valida e envia a telemetria pela USB (CSV + resumo legivel).
// ============================================================
#include <Arduino.h>
#include "ConfigLoRa.h"
#include "Protocolo.h"
#include "pinos.h"
#include "radio_rx.h"
#include "saida.h"

static const uint32_t PERIODO_LEGIVEL_MS = 1000;
static const uint32_t PERIODO_ESPERA_MS  = 5000;
static const uint32_t LED_ACESO_MS       = 30;

static uint32_t recebidos = 0, perdidos = 0, errosCrc = 0, invalidos = 0;
static bool     temSeq = false;
static uint16_t ultimoSeq = 0;

static PacoteTelemetria ultimo;          // ultimo pacote valido (posicao para resgate)
static bool     temUltimo = false;
static float    ultimoRssi = 0, ultimoSnr = 0;
static uint32_t tUltimoPacote = 0, tLegivel = 0, tEspera = 0, tLed = 0;
static bool     ledAceso = false;

static void contarSequencia(uint16_t seq) {
  if (temSeq) {
    uint16_t salto = (uint16_t)(seq - ultimoSeq);
    if (salto >= 1 && salto < 1000) {
      perdidos += salto - 1;
    } else if (salto != 0) {
      Serial.println("# [BASE] sequencia reiniciou (o foguete reiniciou?)");
    }
  }
  ultimoSeq = seq;
  temSeq = true;
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_LED, OUTPUT);
  delay(200);
  saidaCabecalho();

  while (!radioIniciar()) {
    Serial.println("# [RADIO] tentando de novo em 2 s...");
    delay(2000);
  }
  Serial.printf("# [RADIO] escutando em %.1f MHz SF%d BW%.0f kHz\n",
                LORA_FREQ_MHZ, LORA_SF, LORA_BW_KHZ);
}

void loop() {
  const uint32_t agora = millis();

  uint8_t buf[64];
  size_t  len = 0;
  float   rssi = 0, snr = 0;

  switch (radioLer(buf, sizeof(buf), len, rssi, snr)) {
    case RX_OK: {
      PacoteTelemetria p;
      if (pacoteValidar(buf, len, p)) {
        recebidos++;
        contarSequencia(p.seq);
        ultimo = p;
        temUltimo = true;
        ultimoRssi = rssi;
        ultimoSnr = snr;
        tUltimoPacote = agora;
        saidaCsv(p, rssi, snr, perdidos);
        digitalWrite(PIN_LED, HIGH);
        ledAceso = true;
        tLed = agora;
      } else {
        invalidos++;
        Serial.printf("# [BASE] pacote invalido ou de outra rede (%u bytes, RSSI %.0f dBm)\n",
                      (unsigned)len, rssi);
      }
      break;
    }
    case RX_ERRO_CRC:
      errosCrc++;
      break;
    case RX_ERRO:
      Serial.println("# [BASE] erro de leitura no radio");
      break;
    case RX_NADA:
      break;
  }

  if (ledAceso && agora - tLed >= LED_ACESO_MS) {
    digitalWrite(PIN_LED, LOW);
    ledAceso = false;
  }

  // Resumo legivel 1x por segundo, ou alerta de perda de sinal
  if (temUltimo && agora - tLegivel >= PERIODO_LEGIVEL_MS) {
    tLegivel = agora;
    if (agora - tUltimoPacote < TLM_TIMEOUT_SINAL_MS) {
      saidaLegivel(ultimo, ultimoRssi, ultimoSnr, recebidos, perdidos);
    } else {
      Serial.printf("# [BASE] SEM SINAL ha %.1f s | ultima posicao %.7f,%.7f | alt max %.1f m | erros CRC %lu\n",
                    (agora - tUltimoPacote) / 1000.0f, ultimo.latE7 / 1e7, ultimo.lonE7 / 1e7,
                    ultimo.altMaxCm / 100.0f, (unsigned long)errosCrc);
    }
  }

  if (!temUltimo && agora - tEspera >= PERIODO_ESPERA_MS) {
    tEspera = agora;
    Serial.printf("# [BASE] aguardando o foguete... (erros CRC %lu, invalidos %lu)\n",
                  (unsigned long)errosCrc, (unsigned long)invalidos);
  }
}
