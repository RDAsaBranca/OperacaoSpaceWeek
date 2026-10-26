// ============================================================
//  FOGUETE - ESP32-C3 SuperMini
//  BMP280 + BNO055 + ATGM336H + W25Q128 + LoRa SX1276
//
//  Le os sensores a 50 Hz, grava o voo na flash e transmite
//  telemetria a cada TLM_PERIODO_MS.
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
#include "registro_voo.h"
#include "comandos.h"

static const uint32_t PERIODO_SENSORES_MS = 20;    // 50 Hz
static const uint32_t PERIODO_LOG_MS      = 1000;  // diagnostico na USB

static LeituraSensores  leitura;
static PacoteTelemetria pacoteTx, pacoteFlash;
static uint16_t seqTx = 0;       // sequencia dos pacotes de radio
static uint16_t amostra = 0;     // contador das amostras gravadas na flash
static uint32_t tSensores = 0, tTx = 0, tLog = 0;
static uint32_t txPulados = 0;   // vezes em que o radio ainda estava ocupado

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
  p.status     = leitura.status | registroStatus();
  p.altitudeCm = (int32_t)lroundf(estadoVooAltitudeM() * 100.0f);
  p.altMaxCm   = (int32_t)lroundf(estadoVooAltMaxM() * 100.0f);
  p.velVertDms = saturar16(estadoVooVelVertMs() * 10.0f);
  p.tempC100   = saturar16(leitura.temperaturaC * 100.0f);
  for (int i = 0; i < 3; i++) {
    p.acelMg[i]    = saturar16(leitura.acelG[i] * 1000.0f);
    p.giroDps10[i] = saturar16(leitura.giroDps[i] * 10.0f);
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

// LoRa e flash dividem o SPI. Os dois CS precisam estar em nivel alto
// ANTES de qualquer transacao, senao os dois chips respondem juntos no MISO.
static void prepararSpi() {
  pinMode(PIN_LORA_NSS, OUTPUT);
  digitalWrite(PIN_LORA_NSS, HIGH);
  pinMode(PIN_FLASH_CS, OUTPUT);
  digitalWrite(PIN_FLASH_CS, HIGH);
  SPI.begin(PIN_SPI_SCK, PIN_SPI_MISO, PIN_SPI_MOSI);
}

void setup() {
  Serial.begin(115200);
  delay(1500);  // tempo para o USB-CDC da C3 enumerar no PC
  LOG("\n=== FOGUETE: telemetria LoRa SX1276 ===\n");

  prepararSpi();
  sensoresIniciar();

  float p0 = sensoresPressaoMedia(50);
  LOG("[VOO] pressao de referencia na rampa: %.1f Pa\n", p0);
  estadoVooIniciar(p0);

  registroIniciar();  // sem flash o foguete continua: so nao grava

  while (!radioIniciar()) {
    LOG("[RADIO] tentando de novo em 2 s...\n");
    delay(2000);
  }
  uint32_t toaUs = radioTempoNoArUs(sizeof(PacoteTelemetria));
  LOG("[RADIO] ok | %.1f MHz SF%d BW%.0f | pacote %u bytes | tempo no ar %.1f ms | periodo %d ms\n",
      LORA_FREQ_MHZ, LORA_SF, LORA_BW_KHZ, (unsigned)sizeof(PacoteTelemetria),
      toaUs / 1000.0f, TLM_PERIODO_MS);
  if (toaUs / 1000 >= TLM_PERIODO_MS) {
    LOG("[RADIO] AVISO: tempo no ar >= periodo. Aumente TLM_PERIODO_MS ou reduza o SF.\n");
  }
  LOG("# comandos pela USB: info | baixar | apagar confirmo | ajuda\n");
}

void loop() {
  const uint32_t agora = millis();

  sensoresAtualizarGps();
  radioAtualizar();
  comandosAtualizar();

  if (agora - tSensores >= PERIODO_SENSORES_MS) {
    tSensores = agora;
    sensoresLer(leitura);
    estadoVooAtualizar(leitura, agora);
    montarPacote(pacoteFlash, amostra++);
    registroAmostra(pacoteFlash, leitura.pressaoPa, agora);
  }

  if (agora - tTx >= TLM_PERIODO_MS) {
    tTx = agora;
    montarPacote(pacoteTx, seqTx);
    if (radioEnviar(reinterpret_cast<const uint8_t*>(&pacoteTx), sizeof(pacoteTx))) {
      seqTx++;  // so conta o que realmente foi ao ar
    } else {
      txPulados++;
    }
  }

  if (agora - tLog >= PERIODO_LOG_MS) {
    tLog = agora;
    const uint8_t c = leitura.calibracao;
    LOG("[TLM] seq=%u %s alt=%.1f m max=%.1f m v=%.1f m/s | baro=%d imu=%d orient=%d cal=S%uG%uA%uM%u"
        " | gps=%d fix=%d sats=%u | bat=%u mV | flash=%lu reg | pulados=%lu\n",
        seqTx, nomeEstado(estadoVooAtual()), estadoVooAltitudeM(), estadoVooAltMaxM(),
        estadoVooVelVertMs(),
        !!(leitura.status & STATUS_BARO_OK), !!(leitura.status & STATUS_IMU_OK),
        !!(leitura.status & STATUS_ORIENT_OK),
        (c >> 6) & 3u, (c >> 4) & 3u, (c >> 2) & 3u, c & 3u,
        !!(leitura.status & STATUS_GPS_OK), !!(leitura.status & STATUS_GPS_FIX),
        leitura.satelites, leitura.bateriaMv, (unsigned long)registroUsados(),
        (unsigned long)txPulados);
  }
}
