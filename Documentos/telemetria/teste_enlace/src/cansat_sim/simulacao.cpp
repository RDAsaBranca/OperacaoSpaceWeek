#include "simulacao.h"
#include <math.h>
#include <string.h>
#include "Protocolo.h"

static const float  CICLO_S   = 200.0f;      // o voo se repete
static const float  RAMPA_S   = 20.0f;
static const double LAT0      = -5.9238;     // coordenadas ficticias
static const double LON0      = -35.1647;
static const float  GRAU      = 0.01745329f;

// Campo geomagnetico ficticio no referencial do solo (norte, leste, baixo)
// [uT]: ~25 uT na horizontal, declinacao -21 graus, inclinacao -10 graus.
static const float CAMPO_NED[3] = {23.3f, -9.0f, -4.4f};

static uint32_t t0Ms = 0;
static float    altMax = 0;

// Ruido pequeno e reproduzivel (gerador congruencial), em [-1, 1]
static float ruido() {
  static uint32_t s = 12345;
  s = s * 1664525u + 1013904223u;
  return ((s >> 8) & 0xFFFF) / 32767.5f - 1.0f;
}

// Angulos de Euler (graus) -> quaternion w, x, y, z
static void eulerParaQuat(float rol, float arf, float gui, float q[4]) {
  const float cr = cosf(rol * GRAU / 2), sr = sinf(rol * GRAU / 2);
  const float cp = cosf(arf * GRAU / 2), sp = sinf(arf * GRAU / 2);
  const float cy = cosf(gui * GRAU / 2), sy = sinf(gui * GRAU / 2);
  q[0] = cr * cp * cy + sr * sp * sy;
  q[1] = sr * cp * cy - cr * sp * sy;
  q[2] = cr * sp * cy + sr * cp * sy;
  q[3] = cr * cp * sy - sr * sp * cy;
}

// Vetor do referencial do solo para o do sensor: v_sensor = R(q)^T * v_solo
static void soloParaSensor(const float q[4], const float v[3], float out[3]) {
  const float w = q[0], x = q[1], y = q[2], z = q[3];
  const float r[3][3] = {
      {1 - 2 * (y * y + z * z), 2 * (x * y - w * z),     2 * (x * z + w * y)},
      {2 * (x * y + w * z),     1 - 2 * (x * x + z * z), 2 * (y * z - w * x)},
      {2 * (x * z - w * y),     2 * (y * z + w * x),     1 - 2 * (x * x + y * y)},
  };
  for (int i = 0; i < 3; i++) out[i] = r[0][i] * v[0] + r[1][i] * v[1] + r[2][i] * v[2];
}

void simulacaoReiniciar(uint32_t agoraMs) {
  t0Ms = agoraMs;
  altMax = 0;
}

void simulacaoGerar(uint32_t agoraMs, DadosFicticios& d) {
  memset(&d, 0, sizeof(d));
  const float t   = fmodf((agoraMs - t0Ms) / 1000.0f, CICLO_S);
  const float tau = t - RAMPA_S;  // tempo desde o lancamento

  float alt = 0, vel = 0, acel = 1.0f, leste = 0, norte = 0;
  float rol = 0, arf = 2, gui = 40, giroZ = 0;
  uint8_t estado = ESTADO_RAMPA;

  if (tau < 0) {                  // rampa
    altMax = 0;
  } else if (tau < 2.0f) {        // motor: ~6 g
    alt = 30.0f * tau * tau; vel = 60.0f * tau; acel = 7.1f; estado = ESTADO_SUBIDA;
    rol = 90.0f * tau; giroZ = 90.0f;
  } else if (tau < 14.2f) {       // coast ate o apogeu
    const float tc = tau - 2.0f;
    alt = 120.0f + 120.0f * tc - 4.905f * tc * tc; vel = 120.0f - 9.81f * tc; acel = 0.05f;
    estado = ESTADO_SUBIDA; rol = 180.0f + 45.0f * tc; arf = 2.0f + tc; giroZ = 45.0f;
  } else {                        // paraquedas, 7 m/s, vento de ~3 m/s
    const float td = tau - 14.2f;
    alt = 853.9f - 7.0f * td; vel = -7.0f; estado = ESTADO_DESCIDA;
    leste = 3.0f * td; norte = 1.2f * td;
    rol = 10.0f * sinf(td); arf = 20.0f + 5.0f * sinf(td / 3); giroZ = 3.0f;
    if (alt <= 0) {
      alt = 0; vel = 0; leste = 3.0f * 122.0f; norte = 1.2f * 122.0f; arf = 85.0f; giroZ = 0;
      estado = (td > 127.0f) ? ESTADO_POUSADO : ESTADO_DESCIDA;
    }
  }
  if (alt > altMax) altMax = alt;

  d.estado    = estado;
  d.altitudeM = alt + 0.3f * ruido();
  d.altMaxM   = altMax;
  d.velVertMs = vel + 0.2f * ruido();

  d.status = STATUS_BARO_OK | STATUS_IMU_OK | STATUS_GPS_OK | STATUS_GPS_FIX | STATUS_ORIENT_OK;
  d.temperaturaC = 28.0f - alt * 0.0065f + 0.05f * ruido();

  d.acelG[0] = 0.02f * ruido();
  d.acelG[1] = 0.02f * ruido();
  d.acelG[2] = acel + 0.02f * ruido();
  d.giroDps[0] = 0.5f * ruido();
  d.giroDps[1] = 0.5f * ruido();
  d.giroDps[2] = giroZ + 0.5f * ruido();

  eulerParaQuat(rol, arf, gui, d.quat);
  soloParaSensor(d.quat, CAMPO_NED, d.magUT);
  for (int i = 0; i < 3; i++) d.magUT[i] += 0.3f * ruido();
  d.calibracao = (3 << 6) | (3 << 4) | (3 << 2) | 2;

  d.latitude     = LAT0 + norte / 111320.0;
  d.longitude    = LON0 + leste / (111320.0 * cos(LAT0 * 0.0174533));
  d.altitudeGpsM = alt + 52.0f;
  d.satelites    = 9;
  d.bateriaMv    = (uint16_t)(4100 - ((agoraMs - t0Ms) / 60000UL) * 2);  // -2 mV/min
}
