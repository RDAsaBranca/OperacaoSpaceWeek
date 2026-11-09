// ============================================================
//  CODIGO 1 - CANSAT SIMULADO (transmissor do teste do enlace)
//
//  Monta o MESMO PacoteTelemetria do CanSat real (../lib/Telemetria/
//  Protocolo.h: 64 bytes, versao 3, com magnetometro), so que com dados
//  ficticios no lugar dos sensores, e o transmite pelo SX1276 dentro do
//  quadro que o RYLR998 aceita:
//
//     [dst_lo dst_hi src_lo src_hi len] + Base64(PacoteTelemetria)
//      00     00     C5     00     58      88 caracteres  -> 93 bytes
//
//  Uma unica transmissao a cada TLM_PERIODO_MS, recebida ao mesmo tempo
//  pelo SX1276 e pelo RYLR998 da base (Codigo 2).
//
//  Comandos pela USB (115200), uma letra:
//    p  pausa / retoma a transmissao
//    x  imprime em hex o proximo quadro transmitido
//    r  reinicia o voo ficticio (volta para a rampa)
//    i  parametros do enlace e tamanho do quadro
// ============================================================
#include <Arduino.h>
#include <SPI.h>
#include <math.h>
#include <string.h>
#include "ConfigEnlace.h"
#include "Protocolo.h"
#include "QuadroReyax.h"
#include "pinos.h"
#include "radio_tx.h"
#include "simulacao.h"

static const uint32_t PERIODO_LOG_MS            = 1000;
static const uint32_t PERIODO_RETENTAR_RADIO_MS = 5000;
static const uint32_t LED_ACESO_MS              = 30;

static DadosFicticios   dados;
static PacoteTelemetria pacoteTx;
static uint8_t          quadroTx[RYLR_QUADRO_MAX];  // cabecalho Reyax + telemetria codificada
static uint16_t         seqTx = 0;                  // sequencia dos pacotes de radio
static uint32_t         tTx = 0, tLog = 0, tRadio = 0, tLed = 0;
static bool             pausado = false, imprimirProximo = false, ledAceso = false;

static int16_t saturar16(float v) {
  if (v > 32767.0f)  return 32767;
  if (v < -32768.0f) return -32768;
  return (int16_t)lroundf(v);
}

// Mesma conversao de montarPacote() em ../../src/foguete/main.cpp.
// So muda a origem dos valores: DadosFicticios em vez de sensores.
static void montarPacote(PacoteTelemetria& p, uint16_t seq, const DadosFicticios& d) {
  memset(&p, 0, sizeof(p));
  p.seq        = seq;
  p.tempoMs    = millis();
  p.estado     = d.estado;
  p.status     = d.status;
  p.altitudeCm = (int32_t)lroundf(d.altitudeM * 100.0f);
  p.altMaxCm   = (int32_t)lroundf(d.altMaxM * 100.0f);
  p.velVertDms = saturar16(d.velVertMs * 10.0f);
  p.tempC100   = saturar16(d.temperaturaC * 100.0f);
  for (int i = 0; i < 3; i++) {
    p.acelMg[i]    = saturar16(d.acelG[i] * 1000.0f);
    p.giroDps10[i] = saturar16(d.giroDps[i] * 10.0f);
    p.magUT10[i]   = saturar16(d.magUT[i] * 10.0f);
  }
  for (int i = 0; i < 4; i++) {
    p.quat[i] = saturar16(d.quat[i] * QUAT_ESCALA);
  }
  p.calibracao = d.calibracao;
  p.latE7      = (int32_t)llround(d.latitude * 1e7);
  p.lonE7      = (int32_t)llround(d.longitude * 1e7);
  p.altGpsM    = saturar16(d.altitudeGpsM);
  p.satelites  = d.satelites;
  p.bateriaMv  = d.bateriaMv;
  pacoteFinalizar(p);  // magic, versao e CRC16
}

static void imprimirQuadro(const uint8_t* q, size_t n) {
  Serial.printf("# quadro seq %u (%u B):\n#   cabecalho: ", (unsigned)pacoteTx.seq, (unsigned)n);
  for (size_t i = 0; i < RYLR_CAB_TAM && i < n; i++) Serial.printf("%02X ", q[i]);
  Serial.print("\n#   payload:   ");
  for (size_t i = RYLR_CAB_TAM; i < n; i++) {
#if TLM_CODIFICACAO == TLM_COD_BINARIO
    Serial.printf("%02X ", q[i]);
#else
    Serial.print((char)q[i]);  // Base64: texto imprimivel
#endif
  }
  Serial.printf("\n#   no RYLR998 deve aparecer: +RCV=%u,%u,<payload>,<RSSI>,<SNR>\n",
                (unsigned)RYLR_END_CANSAT, (unsigned)(n - RYLR_CAB_TAM));
}

static void imprimirInfo() {
  Serial.printf("# enlace: %.1f MHz | SF%d | BW %.0f kHz | CR 4/%d | sync 0x%02X (AT+NETWORKID=%d) |"
                " preambulo %d | header explicito, CRC, IQ normal\n",
                LORA_FREQ_MHZ, LORA_SF, LORA_BW_KHZ, LORA_CR, LORA_SYNC_WORD, LORA_SYNC_WORD,
                LORA_PREAMBULO);
  Serial.printf("# quadro: %u B = cabecalho Reyax %u B (dst %u, src %u) + %s %u B"
                " (PacoteTelemetria v%d, %u B)\n",
                (unsigned)TLM_QUADRO_TAM, (unsigned)RYLR_CAB_TAM, (unsigned)RYLR_END_DESTINO,
                (unsigned)RYLR_END_CANSAT,
                TLM_CODIFICACAO == TLM_COD_BINARIO ? "binario" : "Base64",
                (unsigned)TLM_PAYLOAD_TAM, TLM_VERSAO, (unsigned)sizeof(PacoteTelemetria));
  const uint32_t toaUs = radioTempoNoArUs(TLM_QUADRO_TAM);
  Serial.printf("# tempo no ar %.1f ms | periodo %d ms%s\n", toaUs / 1000.0f, TLM_PERIODO_MS,
                toaUs / 1000 >= TLM_PERIODO_MS ? " | AVISO: tempo no ar >= periodo" : "");
}

static void iniciarRadio() {
  if (radioIniciar()) imprimirInfo();
}

static void comandos() {
  while (Serial.available()) {
    switch (Serial.read()) {
      case 'p':
        pausado = !pausado;
        Serial.println(pausado ? "# transmissao PAUSADA" : "# transmissao retomada");
        break;
      case 'x':
        imprimirProximo = true;
        break;
      case 'r':
        simulacaoReiniciar(millis());
        Serial.println("# voo ficticio reiniciado (rampa)");
        break;
      case 'i':
        imprimirInfo();
        radioImprimirDiagnostico(Serial);
        break;
      default:
        break;
    }
  }
}

static void imprimirResumo() {
  const PacoteTelemetria& p = pacoteTx;  // exatamente o que foi ao ar
  Serial.printf("[TX] seq %u | %s | alt %.1f m (max %.1f) | v %+.1f m/s | acel z %+.2f g |"
                " mag %+.1f %+.1f %+.1f uT | bat %.2f V%s\n",
                (unsigned)p.seq, nomeEstado(p.estado), p.altitudeCm / 100.0f, p.altMaxCm / 100.0f,
                p.velVertDms / 10.0f, p.acelMg[2] / 1000.0f, p.magUT10[0] / 10.0f,
                p.magUT10[1] / 10.0f, p.magUT10[2] / 10.0f, p.bateriaMv / 1000.0f,
                pausado ? " | PAUSADO" : "");
  radioImprimirDiagnostico(Serial);
}

void setup() {
  Serial.begin(115200);
#if !PLACA_DEVKIT
  delay(1500);  // tempo para o USB-CDC da C3 enumerar no PC
#endif
#ifdef PIN_LED
  pinMode(PIN_LED, OUTPUT);
#endif
  // O CS do radio precisa estar em nivel alto antes da primeira transacao SPI
  pinMode(PIN_LORA_NSS, OUTPUT);
  digitalWrite(PIN_LORA_NSS, HIGH);
  SPI.begin(PIN_SPI_SCK, PIN_SPI_MISO, PIN_SPI_MOSI);

  Serial.println("\n=== CODIGO 1: CanSat simulado (dados ficticios, pacote real) ===");
  simulacaoReiniciar(millis());
  iniciarRadio();  // se falhar, o loop() tenta de novo a cada 5 s
  tRadio = millis();
  Serial.println("# comandos: p (pausa) | x (hex do proximo quadro) | r (reinicia o voo) | i (info)");
}

void loop() {
  const uint32_t agora = millis();

  radioAtualizar();
  comandos();

  if (!radioPronto() && agora - tRadio >= PERIODO_RETENTAR_RADIO_MS) {
    tRadio = agora;
    iniciarRadio();
  }

  if (radioPronto() && !pausado && agora - tTx >= TLM_PERIODO_MS) {
    tTx = agora;
    simulacaoGerar(agora, dados);
    montarPacote(pacoteTx, seqTx, dados);
    const size_t n = quadroTelemetria(pacoteTx, quadroTx, sizeof(quadroTx));
    if (n > 0 && radioEnviar(quadroTx, n)) {
      if (imprimirProximo) {
        imprimirQuadro(quadroTx, n);
        imprimirProximo = false;
      }
      seqTx++;  // so conta o que realmente foi ao ar
#ifdef PIN_LED
      digitalWrite(PIN_LED, HIGH);
      ledAceso = true;
      tLed = agora;
#endif
    }
  }

#ifdef PIN_LED
  if (ledAceso && agora - tLed >= LED_ACESO_MS) {
    digitalWrite(PIN_LED, LOW);
    ledAceso = false;
  }
#endif

  if (agora - tLog >= PERIODO_LOG_MS) {
    tLog = agora;
    imprimirResumo();
  }
}
