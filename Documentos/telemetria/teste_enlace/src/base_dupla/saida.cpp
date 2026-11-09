#include "saida.h"
#include <Arduino.h>
#include <math.h>
#include "ConfigEnlace.h"
#include "QuadroReyax.h"

static const float BATERIA_BAIXA_V = 3.5f;  // NCR18650B: ~3,6 V nominal, 4,2 V cheia

void saidaCabecalho() {
  Serial.println("# === ESTACAO BASE DO TESTE: SX1276 + RYLR998 ===");
  Serial.printf("# enlace: %.1f MHz SF%d BW%.0f CR4/%d sync 0x%02X preambulo %d | CanSat = endereco %u\n",
                LORA_FREQ_MHZ, LORA_SF, LORA_BW_KHZ, LORA_CR, LORA_SYNC_WORD, LORA_PREAMBULO,
                (unsigned)RYLR_END_CANSAT);
  Serial.printf("# colunas TLM: %s,rssi_dbm,snr_db,perdidos,"
                "receptores,rssi_sx_dbm,snr_sx_db,rssi_ry_dbm,snr_ry_db\n", TLM_COLUNAS);
}

void saidaCsv(const ItemRecebido& it, uint32_t perdidos) {
  char linha[340];
  pacoteParaCsv(it.pacote, linha, sizeof(linha));
  // receptor ausente sai como "nan" (o pandas le como NaN)
  Serial.printf("TLM,%s,%.1f,%.2f,%lu,%u,%.1f,%.2f,%.1f,%.2f\n", linha, it.rssi, it.snr,
                (unsigned long)perdidos, (unsigned)it.receptores,
                it.rssiRx[RECEPTOR_SX1276], it.snrRx[RECEPTOR_SX1276],
                it.rssiRx[RECEPTOR_RYLR998], it.snrRx[RECEPTOR_RYLR998]);
}

// Quaternion -> angulos de Euler (rolagem, arfagem, guinada) em graus
static void quaternionParaEuler(const PacoteTelemetria& p, float& rol, float& arf, float& gui) {
  const float w = p.quat[0] / QUAT_ESCALA, x = p.quat[1] / QUAT_ESCALA;
  const float y = p.quat[2] / QUAT_ESCALA, z = p.quat[3] / QUAT_ESCALA;
  const float RAD = 57.2957795f;
  rol = atan2f(2.0f * (w * x + y * z), 1.0f - 2.0f * (x * x + y * y)) * RAD;
  float s = 2.0f * (w * y - z * x);
  if (s > 1.0f) s = 1.0f;
  if (s < -1.0f) s = -1.0f;
  arf = asinf(s) * RAD;
  gui = atan2f(2.0f * (w * z + x * y), 1.0f - 2.0f * (y * y + z * z)) * RAD;
  if (gui < 0) gui += 360.0f;
}

void saidaLegivel(const PacoteTelemetria& p, float rssi, float snr,
                  uint32_t recebidos, uint32_t perdidos) {
  const uint32_t total = recebidos + perdidos;
  const float perda = total ? 100.0f * perdidos / total : 0.0f;
  const bool fix = p.status & STATUS_GPS_FIX;
  const float bateria = p.bateriaMv / 1000.0f;

  Serial.printf(
      "# [%s] seq %u | alt %.1f m (max %.1f) v %+.1f m/s | GPS %s %u sats %.7f,%.7f | "
      "bat %.2f V | melhor RSSI %.0f dBm SNR %.1f dB | perda %.1f%%\n",
      nomeEstado(p.estado), (unsigned)p.seq, p.altitudeCm / 100.0f, p.altMaxCm / 100.0f,
      p.velVertDms / 10.0f, fix ? "fix" : "SEM FIX", p.satelites,
      p.latE7 / 1e7, p.lonE7 / 1e7, bateria, rssi, snr, perda);

  const float mx = p.magUT10[0] / 10.0f, my = p.magUT10[1] / 10.0f, mz = p.magUT10[2] / 10.0f;
  const unsigned c = p.calibracao;
  if (p.status & STATUS_ORIENT_OK) {
    float rol, arf, gui;
    quaternionParaEuler(p, rol, arf, gui);
    Serial.printf("#    orientacao rol %+.1f arf %+.1f gui %.1f graus | mag %+.1f %+.1f %+.1f uT"
                  " (|B| %.1f) | calibracao S%u G%u A%u M%u\n",
                  rol, arf, gui, mx, my, mz, sqrtf(mx * mx + my * my + mz * mz),
                  (c >> 6) & 3u, (c >> 4) & 3u, (c >> 2) & 3u, c & 3u);
  } else {
    Serial.printf("#    mag %+.1f %+.1f %+.1f uT (|B| %.1f) | sem orientacao\n",
                  mx, my, mz, sqrtf(mx * mx + my * my + mz * mz));
  }

  if (!(p.status & STATUS_BARO_OK)) Serial.println("# [ALERTA] barometro BMP280 com falha");
  if (!(p.status & STATUS_IMU_OK))  Serial.println("# [ALERTA] IMU BNO055 com falha");
  if (bateria < BATERIA_BAIXA_V) Serial.printf("# [ALERTA] bateria baixa: %.2f V\n", bateria);
}

void saidaReceptores(const EstatisticasRx& e, const ItemRecebido& u) {
  const EstatisticasFusao& f = e.fusao;
  const uint32_t esperados = e.recebidos + e.perdidos;
  // Perdidos por um receptor = pacotes esperados - os que ele entregou a tempo
  auto perdaRx = [&](int r) -> float {
    const uint32_t ok = f.entregues[r] - f.atrasados[r];
    return esperados ? 100.0f * (float)(esperados - (ok < esperados ? ok : esperados)) / esperados : 0;
  };
  Serial.printf("#    SX1276 %s: %lu (so ele %lu, perda %.1f%%) RSSI %.0f SNR %.1f | "
                "RYLR998 %s: %lu (so ele %lu, perda %.1f%%) RSSI %.0f SNR %.1f | "
                "atrasados %lu/%lu | invalidos %lu/%lu | +ERR %lu | linhas ruins %lu\n",
                e.radioOk ? "ok" : "FALHA", (unsigned long)f.entregues[RECEPTOR_SX1276],
                (unsigned long)f.exclusivos[RECEPTOR_SX1276], perdaRx(RECEPTOR_SX1276),
                u.rssiRx[RECEPTOR_SX1276], u.snrRx[RECEPTOR_SX1276],
                e.reyaxOk ? "ok" : "FALHA", (unsigned long)f.entregues[RECEPTOR_RYLR998],
                (unsigned long)f.exclusivos[RECEPTOR_RYLR998], perdaRx(RECEPTOR_RYLR998),
                u.rssiRx[RECEPTOR_RYLR998], u.snrRx[RECEPTOR_RYLR998],
                (unsigned long)f.atrasados[RECEPTOR_SX1276], (unsigned long)f.atrasados[RECEPTOR_RYLR998],
                (unsigned long)e.invalidos, (unsigned long)e.invalidosReyax,
                (unsigned long)e.reyax.errosModulo, (unsigned long)e.reyax.linhasRuins);
  if (f.divergentes) {
    Serial.printf("# [ALERTA] %lu pacotes com conteudo diferente nos dois receptores\n",
                  (unsigned long)f.divergentes);
  }
}
