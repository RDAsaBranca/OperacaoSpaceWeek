#include "Protocolo.h"
#include <stdio.h>
#include <string.h>

static const size_t BYTES_COM_CRC = offsetof(PacoteTelemetria, crc);

const char* const TLM_COLUNAS =
    "seq,tempo_ms,estado,status,alt_m,alt_max_m,vel_ms,temp_c,"
    "ax_g,ay_g,az_g,gx_dps,gy_dps,gz_dps,qw,qx,qy,qz,"
    "cal_sis,cal_giro,cal_acel,cal_mag,lat,lon,alt_gps_m,sats,bat_v";

uint16_t crc16Ccitt(const uint8_t* dados, size_t len) {
  uint16_t crc = 0xFFFF;
  while (len--) {
    crc ^= (uint16_t)(*dados++) << 8;
    for (uint8_t i = 0; i < 8; i++) {
      crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021) : (uint16_t)(crc << 1);
    }
  }
  return crc;
}

void pacoteFinalizar(PacoteTelemetria& p) {
  p.magic  = TLM_MAGIC;
  p.versao = TLM_VERSAO;
  p.crc    = crc16Ccitt(reinterpret_cast<const uint8_t*>(&p), BYTES_COM_CRC);
}

bool pacoteValidar(const uint8_t* dados, size_t len, PacoteTelemetria& saida) {
  if (len != sizeof(PacoteTelemetria)) return false;
  memcpy(&saida, dados, sizeof(saida));
  if (saida.magic != TLM_MAGIC || saida.versao != TLM_VERSAO) return false;
  return saida.crc == crc16Ccitt(dados, BYTES_COM_CRC);
}

const char* nomeEstado(uint8_t estado) {
  switch (estado) {
    case ESTADO_RAMPA:   return "RAMPA";
    case ESTADO_SUBIDA:  return "SUBIDA";
    case ESTADO_DESCIDA: return "DESCIDA";
    case ESTADO_POUSADO: return "POUSADO";
    default:             return "?";
  }
}

int pacoteParaCsv(const PacoteTelemetria& p, char* buf, size_t tamanho) {
  const unsigned c = p.calibracao;
  return snprintf(buf, tamanho,
      "%u,%lu,%s,%u,%.2f,%.2f,%.1f,%.2f,"
      "%.3f,%.3f,%.3f,%.1f,%.1f,%.1f,%.4f,%.4f,%.4f,%.4f,"
      "%u,%u,%u,%u,%.7f,%.7f,%d,%u,%.2f",
      (unsigned)p.seq, (unsigned long)p.tempoMs, nomeEstado(p.estado), (unsigned)p.status,
      p.altitudeCm / 100.0, p.altMaxCm / 100.0, p.velVertDms / 10.0, p.tempC100 / 100.0,
      p.acelMg[0] / 1000.0, p.acelMg[1] / 1000.0, p.acelMg[2] / 1000.0,
      p.giroDps10[0] / 10.0, p.giroDps10[1] / 10.0, p.giroDps10[2] / 10.0,
      p.quat[0] / (double)QUAT_ESCALA, p.quat[1] / (double)QUAT_ESCALA,
      p.quat[2] / (double)QUAT_ESCALA, p.quat[3] / (double)QUAT_ESCALA,
      (c >> 6) & 3u, (c >> 4) & 3u, (c >> 2) & 3u, c & 3u,
      p.latE7 / 1e7, p.lonE7 / 1e7, (int)p.altGpsM, (unsigned)p.satelites,
      p.bateriaMv / 1000.0);
}
