#include "recepcao.h"
#include <math.h>
#include <string.h>
#include "ConfigLoRa.h"
#include "QuadroReyax.h"
#include "config_base.h"
#include "pinos.h"
#include "radio_rx.h"
#include "saida.h"

static const uint32_t PERIODO_LEGIVEL_MS   = 1000;
static const uint32_t PERIODO_ESPERA_MS    = 5000;
static const uint32_t LED_ACESO_MS         = 30;
static const uint32_t RETENTAR_RECEPTOR_MS = 2000;

static QueueHandle_t   filaSaida = nullptr;
static portMUX_TYPE    trava = portMUX_INITIALIZER_UNLOCKED;
static EstatisticasRx  est = {};

static bool             temSeq = false;
static uint16_t         ultimoSeq = 0;
static PacoteTelemetria ultimo;
static float            ultimoRssi = 0, ultimoSnr = 0;
static uint32_t         tLegivel = 0, tEspera = 0, tLed = 0;
static bool             ledAceso = false;

EstatisticasRx recepcaoEstatisticas() {
  portENTER_CRITICAL(&trava);
  EstatisticasRx copia = est;
  portEXIT_CRITICAL(&trava);
  return copia;
}

#define CONTAR(campo) do { portENTER_CRITICAL(&trava); est.campo++; portEXIT_CRITICAL(&trava); } while (0)

// Conta perdas pelos saltos na sequencia. Retorna true se o foguete reiniciou.
static bool contarSequencia(uint16_t seq) {
  bool reiniciou = false;
  portENTER_CRITICAL(&trava);
  if (temSeq) {
    uint16_t salto = (uint16_t)(seq - ultimoSeq);
    if (salto >= 1 && salto < 1000) est.perdidos += salto - 1;
    else if (salto != 0) reiniciou = true;
  }
  ultimoSeq = seq;
  temSeq = true;
  portEXIT_CRITICAL(&trava);
  return reiniciou;
}

static void processarValido(const PacoteTelemetria& p, float rssi, float snr) {
  const uint32_t agora = millis();
  const bool reiniciou = contarSequencia(p.seq);

  portENTER_CRITICAL(&trava);
  est.recebidos++;
  est.tUltimoMs = agora;
  est.temPacote = true;
  const uint32_t perdidos = est.perdidos;
  portEXIT_CRITICAL(&trava);

  if (reiniciou) Serial.println("# [BASE] sequencia reiniciou (o foguete reiniciou?)");

  ItemRecebido item;
  item.pacote  = p;
  item.rssi    = rssi;
  item.snr     = snr;
  item.tBaseMs = agora;
  if (xQueueSend(filaSaida, &item, 0) != pdTRUE) {
    portENTER_CRITICAL(&trava);
    est.descartadosFila++;
    portEXIT_CRITICAL(&trava);
  }

  ultimo = p;
  ultimoRssi = rssi;
  ultimoSnr = snr;
  saidaCsv(p, rssi, snr, perdidos);  // copia pela USB, como antes
  digitalWrite(PIN_LED, HIGH);
  ledAceso = true;
  tLed = agora;
}

#if SIMULAR_FOGUETE
// ------------------------------------------------------------------
// Voo ficticio: 20 s na rampa, 2 s de motor, subida ate ~850 m,
// descida de paraquedas a 7 m/s com vento, pouso. Repete a cada 200 s.
// ------------------------------------------------------------------
static int16_t sat16(float v) {
  if (v > 32767.0f) return 32767;
  if (v < -32768.0f) return -32768;
  return (int16_t)lroundf(v);
}

static void simularPacote(PacoteTelemetria& p, uint16_t seq, float& rssi, float& snr) {
  static const float LAT0 = -5.9238f, LON0 = -35.1647f;  // coordenadas ficticias
  const float t = (millis() % 200000UL) / 1000.0f;
  const float tau = t - 20.0f;  // tempo desde o lancamento

  float alt = 0, vel = 0, acel = 1.0f, leste = 0, norte = 0, rol = 0, arf = 2, gui = 40;
  uint8_t estado = ESTADO_RAMPA;
  static float altMax = 0;
  if (tau < 0) {
    altMax = 0;
  } else if (tau < 2.0f) {
    alt = 30.0f * tau * tau; vel = 60.0f * tau; acel = 7.1f; estado = ESTADO_SUBIDA;
    rol = 90.0f * tau;
  } else if (tau < 14.2f) {
    const float tc = tau - 2.0f;
    alt = 120.0f + 120.0f * tc - 4.905f * tc * tc; vel = 120.0f - 9.81f * tc; acel = 0.05f;
    estado = ESTADO_SUBIDA; rol = 180.0f + 45.0f * tc; arf = 2.0f + tc;
  } else {
    const float td = tau - 14.2f;
    alt = 853.9f - 7.0f * td; vel = -7.0f; acel = 1.0f; estado = ESTADO_DESCIDA;
    leste = 3.0f * td; norte = 1.2f * td; rol = 10.0f * sinf(td); arf = 20.0f + 5.0f * sinf(td / 3);
    if (alt <= 0) { alt = 0; vel = 0; estado = (td > 127.0f) ? ESTADO_POUSADO : ESTADO_DESCIDA;
                    leste = 3.0f * 122.0f; norte = 1.2f * 122.0f; arf = 85.0f; }
  }
  if (alt > altMax) altMax = alt;

  // Euler (graus) -> quaternion
  const float r = rol * 0.0087266f, pi = arf * 0.0087266f, y = gui * 0.0087266f;
  const float cr = cosf(r), sr = sinf(r), cp = cosf(pi), sp = sinf(pi), cy = cosf(y), sy = sinf(y);
  const float qw = cr * cp * cy + sr * sp * sy, qx = sr * cp * cy - cr * sp * sy;
  const float qy = cr * sp * cy + sr * cp * sy, qz = cr * cp * sy - sr * sp * cy;

  memset(&p, 0, sizeof(p));
  p.seq = seq;
  p.tempoMs = millis();
  p.estado = estado;
  p.status = STATUS_BARO_OK | STATUS_IMU_OK | STATUS_GPS_OK | STATUS_GPS_FIX |
             STATUS_ORIENT_OK;
  p.altitudeCm = (int32_t)lroundf(alt * 100.0f);
  p.altMaxCm   = (int32_t)lroundf(altMax * 100.0f);
  p.velVertDms = sat16(vel * 10.0f);
  p.tempC100   = sat16((28.0f - alt * 0.0065f) * 100.0f);
  p.acelMg[0]  = sat16(20.0f * sinf(t));
  p.acelMg[1]  = sat16(15.0f * cosf(t));
  p.acelMg[2]  = sat16(acel * 1000.0f);
  p.giroDps10[2] = sat16((estado == ESTADO_SUBIDA ? 90.0f : 3.0f) * 10.0f);
  p.quat[0] = sat16(qw * QUAT_ESCALA); p.quat[1] = sat16(qx * QUAT_ESCALA);
  p.quat[2] = sat16(qy * QUAT_ESCALA); p.quat[3] = sat16(qz * QUAT_ESCALA);
  p.calibracao = (3 << 6) | (3 << 4) | (3 << 2) | 2;
  p.latE7 = (int32_t)lround((LAT0 + norte / 111320.0f) * 1e7);
  p.lonE7 = (int32_t)lround((LON0 + leste / (111320.0f * cosf(LAT0 * 0.0174533f))) * 1e7);
  p.altGpsM = sat16(alt + 52.0f);
  p.satelites = 9;
  p.bateriaMv = (uint16_t)(4100 - (millis() / 60000UL) * 2);
  pacoteFinalizar(p);

  const float dist = sqrtf(alt * alt + leste * leste + norte * norte) + 30.0f;
  rssi = -45.0f - 20.0f * log10f(dist);
  snr  = 9.5f - (dist / 120.0f);
}

#elif RECEPTOR_BASE == RECEPTOR_SX1276
// ------------------------------------------------------------------
// SX1276: recebe o quadro INTEIRO do foguete e tira o cabecalho Reyax aqui.
// ------------------------------------------------------------------
static bool iniciarReceptor() {
  return radioIniciar();
}

static void lerReceptor() {
  uint8_t buf[RYLR_QUADRO_MAX];  // o quadro Base64 tem 93 bytes (binario: 69)
  size_t  len = 0;
  float   rssi = 0, snr = 0;
  switch (radioLer(buf, sizeof(buf), len, rssi, snr)) {
    case RX_OK: {
      CabecalhoRylr    cab;
      const uint8_t*   payload = nullptr;
      PacoteTelemetria p;
      // 1) cabecalho: len >= 5 e byte 4 == len - 5   2) origem = foguete
      // 3) bytes 5..: Base64 (ou binario) -> PacoteTelemetria -> magic, versao, CRC16
      if (rylrAbrir(buf, len, cab, payload) && cab.src == RYLR_END_CANSAT &&
          tlmDecodificar(payload, cab.len, p)) {
        processarValido(p, rssi, snr);
      } else {
        CONTAR(invalidos);
        Serial.printf("# [SX1276] quadro invalido ou de outra rede (%u bytes, RSSI %.0f dBm)\n",
                      (unsigned)len, rssi);
      }
      break;
    }
    case RX_ERRO_CRC: CONTAR(errosCrc);   break;
    case RX_ERRO:     CONTAR(errosRadio); break;
    case RX_NADA:     break;
  }
}

#else  // RECEPTOR_BASE == RECEPTOR_RYLR998
// ------------------------------------------------------------------
// RYLR998: o proprio modulo confere e tira o cabecalho, e entrega +RCV.
// ------------------------------------------------------------------
static bool iniciarReceptor() {
  return reyaxIniciar();
}

static void lerReceptor() {
  uint16_t origem;
  uint8_t  payload[RYLR_PAYLOAD_MAX];
  size_t   len;
  int      rssi, snr;
  while (reyaxLer(origem, payload, sizeof(payload), len, rssi, snr)) {
    PacoteTelemetria p;
    if (origem == RYLR_END_CANSAT && tlmDecodificar(payload, len, p)) {
      processarValido(p, (float)rssi, (float)snr);
    } else {
      CONTAR(invalidos);
      Serial.printf("# [REYAX] +RCV invalido (origem %u, %u bytes, RSSI %d dBm)\n",
                    (unsigned)origem, (unsigned)len, rssi);
    }
  }
  const EstatisticasReyax er = reyaxEstatisticas();
  portENTER_CRITICAL(&trava);
  est.reyax = er;
  portEXIT_CRITICAL(&trava);
}
#endif

static void tarefaRecepcao(void*) {
#if SIMULAR_FOGUETE
  portENTER_CRITICAL(&trava);
  est.radioOk = true;
  portEXIT_CRITICAL(&trava);
  uint32_t tSim = 0;
  uint16_t seqSim = 0;
#else
  while (!iniciarReceptor()) {
    Serial.println("# [" RECEPTOR_NOME "] tentando de novo em 2 s...");
    vTaskDelay(pdMS_TO_TICKS(RETENTAR_RECEPTOR_MS));
  }
  portENTER_CRITICAL(&trava);
  est.radioOk = true;
  portEXIT_CRITICAL(&trava);
  Serial.printf("# [" RECEPTOR_NOME "] escutando em %.1f MHz SF%d BW%.0f kHz sync 0x%02X preambulo %d\n",
                LORA_FREQ_MHZ, LORA_SF, LORA_BW_KHZ, LORA_SYNC_WORD, LORA_PREAMBULO);
#endif

  for (;;) {
    const uint32_t agora = millis();

#if SIMULAR_FOGUETE
    if (agora - tSim >= TLM_PERIODO_MS) {
      tSim = agora;
      PacoteTelemetria p;
      float rssi, snr;
      simularPacote(p, seqSim++, rssi, snr);
      if (random(100) >= 3) processarValido(p, rssi, snr);  // ~3% de perda ficticia
    }
#else
    lerReceptor();
#endif

    if (ledAceso && agora - tLed >= LED_ACESO_MS) {
      digitalWrite(PIN_LED, LOW);
      ledAceso = false;
    }

    // Resumo legivel pela USB 1x por segundo, ou alerta de perda de sinal
    const EstatisticasRx e = recepcaoEstatisticas();
    if (e.temPacote && agora - tLegivel >= PERIODO_LEGIVEL_MS) {
      tLegivel = agora;
      if (agora - e.tUltimoMs < TLM_TIMEOUT_SINAL_MS) {
        saidaLegivel(ultimo, ultimoRssi, ultimoSnr, e.recebidos, e.perdidos);
      } else {
        Serial.printf("# [BASE] SEM SINAL ha %.1f s | ultima posicao %.7f,%.7f | alt max %.1f m\n",
                      (agora - e.tUltimoMs) / 1000.0f, ultimo.latE7 / 1e7, ultimo.lonE7 / 1e7,
                      ultimo.altMaxCm / 100.0f);
      }
    }
    if (!e.temPacote && agora - tEspera >= PERIODO_ESPERA_MS) {
      tEspera = agora;
#if RECEPTOR_BASE == RECEPTOR_SX1276
      Serial.printf("# [BASE] aguardando o foguete... SX1276 (erros CRC %lu, outros erros %lu,"
                    " invalidos %lu)\n", (unsigned long)e.errosCrc, (unsigned long)e.errosRadio,
                    (unsigned long)e.invalidos);
#else
      Serial.printf("# [BASE] aguardando o foguete... RYLR998 (+RCV %lu, invalidos %lu,"
                    " linhas ruins %lu, +ERR %lu %s)\n", (unsigned long)e.reyax.linhasRcv,
                    (unsigned long)e.invalidos, (unsigned long)e.reyax.linhasRuins,
                    (unsigned long)e.reyax.errosModulo, e.reyax.ultimoErro);
#endif
    }

    vTaskDelay(pdMS_TO_TICKS(2));  // libera a CPU; o SX1276 e a UART guardam os dados
  }
}

bool recepcaoIniciar(QueueHandle_t fila) {
  filaSaida = fila;
  pinMode(PIN_LED, OUTPUT);
  // Nucleo 1, prioridade 3: acima do loop() do Arduino (prioridade 1)
  return xTaskCreatePinnedToCore(tarefaRecepcao, "recepcao", 8192, nullptr, 3, nullptr, 1) == pdPASS;
}
