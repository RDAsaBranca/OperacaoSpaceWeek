#include "QuadroReyax.h"
#include <string.h>

size_t rylrMontar(uint16_t dst, uint16_t src, const uint8_t* payload, size_t len,
                  uint8_t* out, size_t max) {
  if (len > RYLR_PAYLOAD_MAX || max < RYLR_CAB_TAM + len) return 0;
  if (payload != out + RYLR_CAB_TAM) memmove(out + RYLR_CAB_TAM, payload, len);
  out[0] = (uint8_t)(dst & 0xFF);   // little-endian, como capturado do RYLR998
  out[1] = (uint8_t)(dst >> 8);
  out[2] = (uint8_t)(src & 0xFF);
  out[3] = (uint8_t)(src >> 8);
  out[4] = (uint8_t)len;
  return RYLR_CAB_TAM + len;
}

bool rylrAbrir(const uint8_t* q, size_t len, CabecalhoRylr& cab, const uint8_t*& payload) {
  if (len < RYLR_CAB_TAM) return false;
  cab.dst = (uint16_t)(q[0] | (q[1] << 8));
  cab.src = (uint16_t)(q[2] | (q[3] << 8));
  cab.len = q[4];
  if ((size_t)cab.len != len - RYLR_CAB_TAM) return false;
  payload = q + RYLR_CAB_TAM;
  return true;
}

// ---------------- Base64 (RFC 4648, com '=') ----------------
static const char B64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

size_t base64Codificar(const uint8_t* in, size_t len, char* out, size_t max) {
  if (max < base64Tam(len)) return 0;
  size_t o = 0;
  for (size_t i = 0; i < len; i += 3) {
    uint32_t v = (uint32_t)in[i] << 16;
    if (i + 1 < len) v |= (uint32_t)in[i + 1] << 8;
    if (i + 2 < len) v |= in[i + 2];
    out[o++] = B64[(v >> 18) & 63];
    out[o++] = B64[(v >> 12) & 63];
    out[o++] = (i + 1 < len) ? B64[(v >> 6) & 63] : '=';
    out[o++] = (i + 2 < len) ? B64[v & 63] : '=';
  }
  return o;
}

static int valorB64(char c) {
  if (c >= 'A' && c <= 'Z') return c - 'A';
  if (c >= 'a' && c <= 'z') return c - 'a' + 26;
  if (c >= '0' && c <= '9') return c - '0' + 52;
  if (c == '+') return 62;
  if (c == '/') return 63;
  return -1;
}

size_t base64Decodificar(const char* in, size_t len, uint8_t* out, size_t max) {
  if (len == 0 || (len % 4) != 0) return 0;
  size_t o = 0;
  for (size_t i = 0; i < len; i += 4) {
    int v[4];
    int pad = 0;
    for (int k = 0; k < 4; k++) {
      const char c = in[i + k];
      if (c == '=' && k >= 2 && i + 4 == len) { v[k] = 0; pad++; continue; }
      if (pad) return 0;  // caractere depois de '='
      v[k] = valorB64(c);
      if (v[k] < 0) return 0;
    }
    const uint32_t x = ((uint32_t)v[0] << 18) | ((uint32_t)v[1] << 12) |
                       ((uint32_t)v[2] << 6) | (uint32_t)v[3];
    const size_t n = 3 - pad;
    if (o + n > max) return 0;
    out[o++] = (uint8_t)(x >> 16);
    if (n > 1) out[o++] = (uint8_t)(x >> 8);
    if (n > 2) out[o++] = (uint8_t)x;
  }
  return o;
}

// ---------------- Telemetria <-> payload ----------------
size_t tlmCodificar(const PacoteTelemetria& p, uint8_t* out, size_t max) {
  const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&p);
#if TLM_CODIFICACAO == TLM_COD_BINARIO
  if (max < sizeof(p)) return 0;
  memcpy(out, bytes, sizeof(p));
  return sizeof(p);
#else
  return base64Codificar(bytes, sizeof(p), reinterpret_cast<char*>(out), max);
#endif
}

bool tlmDecodificar(const uint8_t* payload, size_t len, PacoteTelemetria& p) {
  if (len == sizeof(PacoteTelemetria)) {
    return pacoteValidar(payload, len, p);
  }
  if (len == base64Tam(sizeof(PacoteTelemetria))) {
    uint8_t bin[sizeof(PacoteTelemetria)];
    const size_t n = base64Decodificar(reinterpret_cast<const char*>(payload), len, bin, sizeof(bin));
    return n == sizeof(bin) && pacoteValidar(bin, n, p);
  }
  return false;
}

size_t quadroTelemetria(const PacoteTelemetria& p, uint8_t* out, size_t max) {
  if (max < RYLR_CAB_TAM) return 0;
  const size_t n = tlmCodificar(p, out + RYLR_CAB_TAM, max - RYLR_CAB_TAM);
  if (n == 0) return 0;
  return rylrMontar(RYLR_END_DESTINO, RYLR_END_CANSAT, out + RYLR_CAB_TAM, n, out, max);
}
