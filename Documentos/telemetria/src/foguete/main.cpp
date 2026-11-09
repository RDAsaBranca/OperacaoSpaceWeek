// ============================================================
//  FOGUETE - ESP32-C3 SuperMini
//  BMP280 + BNO055 + ATGM336H + LoRa SX1276 + bateria (ADC)
//
//  Le os sensores a 50 Hz e transmite telemetria a cada
//  TLM_PERIODO_MS. Nao ha armazenamento a bordo: o CanSat cai no
//  mar e nao e recuperado, entao os dados vao todos pelo radio.
//
//  Nenhuma falha de componente trava o firmware. O que nao responder
//  fica marcado no status do pacote e no diagnostico, e o resto segue.
// ============================================================
#include <Arduino.h>
#include <SPI.h>
#include <math.h>
#include <string.h>
#include "ConfigLoRa.h"
#include "Protocolo.h"
#include "pinos.h"
#include "log.h"
#include "radio_tx.h"
#include "sensores.h"
#include "estado_voo.h"
#include "comandos.h"

static const uint32_t PERIODO_SENSORES_MS      = 20;    // 50 Hz
static const uint32_t PERIODO_LOG_MS           = 1000;  // diagnostico na USB
static const uint32_t PERIODO_RETENTAR_RADIO_MS = 5000; // se o SX1276 nao respondeu

static LeituraSensores  leitura;
static PacoteTelemetria pacoteTx;
static uint16_t seqTx = 0;       // sequencia dos pacotes de radio
static uint32_t tSensores = 0, tTx = 0, tLog = 0, tRadio = 0;

static int16_t saturar16(float v) {
  if (v > 32767.0f)  return 32767;
  if (v < -32768.0f) return -32768;
  return (int16_t)lroundf(v);
}

static void montarPacote(PacoteTelemetria& p, uint16_t seq) {
  memset(&p, 0, sizeof(p));
  p.seq        = seq;
  p.tempoMs    = millis();
  p.estado     = estadoVooAtual();
  p.status     = leitura.status;
  p.altitudeCm = (int32_t)lroundf(estadoVooAltitudeM() * 100.0f);
  p.altMaxCm   = (int32_t)lroundf(estadoVooAltMaxM() * 100.0f);
  p.velVertDms = saturar16(estadoVooVelVertMs() * 10.0f);
  p.tempC100   = saturar16(leitura.temperaturaC * 100.0f);
  for (int i = 0; i < 3; i++) {
    p.acelMg[i]    = saturar16(leitura.acelG[i] * 1000.0f);
    p.giroDps10[i] = saturar16(leitura.giroDps[i] * 10.0f);
    p.magUT10[i]   = saturar16(leitura.magUT[i] * 10.0f);
  }
  for (int i = 0; i < 4; i++) {
    p.quat[i] = saturar16(leitura.quat[i] * QUAT_ESCALA);
  }
  p.calibracao = leitura.calibracao;
  p.latE7      = (int32_t)llround(leitura.latitude * 1e7);
  p.lonE7      = (int32_t)llround(leitura.longitude * 1e7);
  p.altGpsM    = saturar16(leitura.altitudeGpsM);
  p.satelites  = leitura.satelites;
  p.bateriaMv  = leitura.bateriaMv;
  pacoteFinalizar(p);
}

// O CS do radio precisa estar em nivel alto antes da primeira transacao SPI.
static void prepararSpi() {
  pinMode(PIN_LORA_NSS, OUTPUT);
  digitalWrite(PIN_LORA_NSS, HIGH);
  SPI.begin(PIN_SPI_SCK, PIN_SPI_MISO, PIN_SPI_MOSI);
}

static void iniciarRadio() {
  if (!radioIniciar()) return;
  const uint32_t toaUs = radioTempoNoArUs(sizeof(PacoteTelemetria));
  LOG("[LoRa] ok: %.1f MHz SF%d BW%.0f | pacote %u bytes | tempo no ar %.1f ms | periodo %d ms\n",
      LORA_FREQ_MHZ, LORA_SF, LORA_BW_KHZ, (unsigned)sizeof(PacoteTelemetria),
      toaUs / 1000.0f, TLM_PERIODO_MS);
  if (toaUs / 1000 >= TLM_PERIODO_MS) {
    LOG("[LoRa] AVISO: tempo no ar >= periodo. Aumente TLM_PERIODO_MS ou reduza o SF.\n");
  }
}

// Bloco de diagnostico, 1x por segundo: uma linha por componente.
// Com DIAGNOSTICO_USB=0 (ambiente foguete_voo) nada disso e compilado.
static void imprimirDiagnostico() {
#if DIAGNOSTICO_USB
  Serial.printf("\n---- t=%.1f s | %s | alt %.1f m (max %.1f) | v %+.1f m/s | pacote seq %u ----\n",
                millis() / 1000.0f, nomeEstado(estadoVooAtual()), estadoVooAltitudeM(),
                estadoVooAltMaxM(), estadoVooVelVertMs(), seqTx);
  sensoresImprimirDiagnostico(leitura, Serial);
  radioImprimirDiagnostico(Serial);
#endif
}

void setup() {
  Serial.begin(115200);
#if DIAGNOSTICO_USB
  delay(1500);  // tempo para o USB-CDC da C3 enumerar no PC
#endif
  LOG("\n=== FOGUETE: telemetria LoRa SX1276 ===\n");

  prepararSpi();
  sensoresIniciar();

  float p0 = sensoresPressaoMedia(50);
  LOG("[VOO] pressao de referencia na rampa: %.1f Pa%s\n", p0,
      p0 > 0 ? "" : " (sem barometro: altitude fica em 0)");
  estadoVooIniciar(p0);

  iniciarRadio();  // se falhar, o loop() tenta de novo a cada 5 s
  tRadio = millis();

  LOG("# comandos pela USB: info | ajuda\n");
}

void loop() {
  const uint32_t agora = millis();

  sensoresAtualizarGps();
  radioAtualizar();
  comandosAtualizar();

  if (!radioPronto() && agora - tRadio >= PERIODO_RETENTAR_RADIO_MS) {
    tRadio = agora;
    iniciarRadio();
  }

  if (agora - tSensores >= PERIODO_SENSORES_MS) {
    tSensores = agora;
    sensoresLer(leitura);
    estadoVooAtualizar(leitura, agora);
  }

  if (radioPronto() && agora - tTx >= TLM_PERIODO_MS) {
    tTx = agora;
    montarPacote(pacoteTx, seqTx);
    if (radioEnviar(reinterpret_cast<const uint8_t*>(&pacoteTx), sizeof(pacoteTx))) {
      seqTx++;  // so conta o que realmente foi ao ar
    }
  }

  if (agora - tLog >= PERIODO_LOG_MS) {
    tLog = agora;
    imprimirDiagnostico();
  }
}
