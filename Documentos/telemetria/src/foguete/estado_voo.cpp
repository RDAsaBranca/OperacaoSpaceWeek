#include "estado_voo.h"
#include <math.h>
#include "log.h"

// ---- Limiares: ajuste para o seu foguete ----
static const float    LANC_ACEL_G         = 2.5f;   // |a| acima disso = motor aceso
static const uint8_t  LANC_AMOSTRAS       = 5;      // amostras seguidas (~100 ms a 50 Hz)
static const float    LANC_ALTITUDE_M     = 15.0f;  // ou subiu mais que isso
static const uint32_t APOGEU_TEMPO_MIN_MS = 1000;   // ignora "apogeu" logo apos lancar
static const float    APOGEU_QUEDA_M      = 5.0f;   // caiu isso abaixo do maximo
static const float    POUSO_JANELA_M      = 2.0f;   // variacao tolerada parado
static const uint32_t POUSO_TEMPO_MS      = 5000;   // tempo parado para "pousado"

// ---- Filtros (taxa de 50 Hz) ----
static const float ALFA_ALT  = 0.3f;    // filtro da altitude
static const float ALFA_VEL  = 0.2f;    // filtro da velocidade vertical
static const float ALFA_SOLO = 0.002f;  // acompanha a pressao na rampa (~10 s)

static EstadoVoo estado = ESTADO_RAMPA;
static float     pressaoSolo = 0;
static float     altFiltrada = 0, altMax = 0, velVert = 0;
static bool      primeiraAmostra = true;
static uint32_t  tAnterior = 0, tLancamento = 0, tRefPouso = 0;
static float     altRefPouso = 0;
static uint8_t   contAcel = 0;

static float altitudeDePressao(float p, float p0) {
  return 44330.0f * (1.0f - powf(p / p0, 0.1903f));
}

static void mudarEstado(EstadoVoo novo, uint32_t agora) {
  LOG("[VOO] %s -> %s  (t=%lu ms, alt=%.1f m)\n",
      nomeEstado(estado), nomeEstado(novo), (unsigned long)agora, altFiltrada);
  estado = novo;
}

void estadoVooIniciar(float pressaoSoloPa) {
  pressaoSolo = pressaoSoloPa;
  estado = ESTADO_RAMPA;
  primeiraAmostra = true;
  altFiltrada = altMax = velVert = 0;
  contAcel = 0;
}

void estadoVooAtualizar(const LeituraSensores& l, uint32_t agora) {
  const bool baroOk = (l.status & STATUS_BARO_OK) && pressaoSolo > 0;
  const bool imuOk  = (l.status & STATUS_IMU_OK);

  // ---- altitude e velocidade vertical ----
  if (baroOk) {
    // Na rampa, acompanha lentamente a variacao do clima para a
    // altitude continuar em ~0 m ate o lancamento.
    if (estado == ESTADO_RAMPA && !primeiraAmostra) {
      pressaoSolo += ALFA_SOLO * (l.pressaoPa - pressaoSolo);
    }
    float alt = altitudeDePressao(l.pressaoPa, pressaoSolo);
    if (primeiraAmostra) {
      altFiltrada = alt;
      primeiraAmostra = false;
    } else {
      float dt = (agora - tAnterior) / 1000.0f;
      float altAnterior = altFiltrada;
      altFiltrada += ALFA_ALT * (alt - altFiltrada);
      if (dt > 0) velVert += ALFA_VEL * ((altFiltrada - altAnterior) / dt - velVert);
    }
    tAnterior = agora;
    if (estado != ESTADO_RAMPA && altFiltrada > altMax) altMax = altFiltrada;
  }

  float acelModulo = 0;
  if (imuOk) {
    acelModulo = sqrtf(l.acelG[0] * l.acelG[0] + l.acelG[1] * l.acelG[1] +
                       l.acelG[2] * l.acelG[2]);
  }

  // ---- maquina de estados ----
  switch (estado) {
    case ESTADO_RAMPA:
      if (acelModulo > LANC_ACEL_G) { if (contAcel < 255) contAcel++; }
      else contAcel = 0;
      if (contAcel >= LANC_AMOSTRAS || (baroOk && altFiltrada > LANC_ALTITUDE_M)) {
        tLancamento = agora;
        altMax = altFiltrada;
        mudarEstado(ESTADO_SUBIDA, agora);
      }
      break;

    case ESTADO_SUBIDA:
      if (baroOk && (agora - tLancamento) > APOGEU_TEMPO_MIN_MS &&
          altFiltrada < altMax - APOGEU_QUEDA_M) {
        altRefPouso = altFiltrada;
        tRefPouso = agora;
        mudarEstado(ESTADO_DESCIDA, agora);
      }
      break;

    case ESTADO_DESCIDA:
      if (fabsf(altFiltrada - altRefPouso) > POUSO_JANELA_M) {
        altRefPouso = altFiltrada;
        tRefPouso = agora;
      } else if (agora - tRefPouso > POUSO_TEMPO_MS) {
        mudarEstado(ESTADO_POUSADO, agora);
      }
      break;

    case ESTADO_POUSADO:
      break;
  }
}

EstadoVoo estadoVooAtual()     { return estado; }
float     estadoVooAltitudeM() { return altFiltrada; }
float     estadoVooAltMaxM()   { return altMax; }
float     estadoVooVelVertMs() { return velVert; }
