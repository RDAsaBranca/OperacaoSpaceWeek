#include "fusao.h"
#include <math.h>
#include <string.h>

static const int PENDENTES = 8;   // a 5 Hz com janela de 80 ms, no maximo 1 ou 2 em uso
static const int RECENTES  = 32;  // ~6 s de chaves ja emitidas

struct Pendente {
  bool        usado;
  uint32_t    prazoMs;
  ItemFundido item;
};
struct Chave {
  bool     valida;
  uint16_t seq;
  uint32_t tempoMs;
};

static Pendente          pend[PENDENTES];
static Chave             recentes[RECENTES];
static int               idxRecentes = 0;
static uint32_t          janela = 80;
static EstatisticasFusao est;

static bool mesmaChave(const PacoteTelemetria& a, const PacoteTelemetria& b) {
  return a.seq == b.seq && a.tempoMs == b.tempoMs;
}

static bool jaEmitido(const PacoteTelemetria& p) {
  for (const Chave& k : recentes) {
    if (k.valida && k.seq == p.seq && k.tempoMs == p.tempoMs) return true;
  }
  return false;
}

void fusaoIniciar(uint32_t janelaMs) {
  janela = janelaMs;
  memset(pend, 0, sizeof(pend));
  memset(recentes, 0, sizeof(recentes));
  memset(&est, 0, sizeof(est));
}

void fusaoEntrada(Receptor r, const PacoteTelemetria& p, float rssi, float snr, uint32_t agora) {
  est.entregues[r]++;
  if (jaEmitido(p)) {
    est.atrasados[r]++;
    return;
  }

  Pendente* livre = nullptr;
  Pendente* maisAntigo = nullptr;
  for (Pendente& s : pend) {
    if (s.usado && mesmaChave(s.item.pacote, p)) {  // segunda copia do mesmo pacote
      if (memcmp(&s.item.pacote, &p, sizeof(p)) != 0) est.divergentes++;
      s.item.mascara |= (uint8_t)(1u << r);
      s.item.rx[r] = {rssi, snr, agora};
      if (s.item.mascara == MASCARA_TODOS) s.prazoMs = agora;  // completo: sai ja
      return;
    }
    if (!s.usado && !livre) livre = &s;
    if (s.usado && (!maisAntigo || (int32_t)(s.prazoMs - maisAntigo->prazoMs) < 0)) maisAntigo = &s;
  }

  if (!livre) {  // so acontece se fusaoSaida() deixar de ser chamada
    est.sobrescritos++;
    livre = maisAntigo;
  }
  livre->usado = true;
  livre->prazoMs = agora + janela;
  livre->item.pacote = p;
  livre->item.mascara = (uint8_t)(1u << r);
  for (int k = 0; k < N_RECEPTORES; k++) livre->item.rx[k] = {NAN, NAN, 0};
  livre->item.rx[r] = {rssi, snr, agora};
}

bool fusaoSaida(uint32_t agora, ItemFundido& saida) {
  Pendente* pronto = nullptr;
  for (Pendente& s : pend) {
    if (!s.usado || (int32_t)(agora - s.prazoMs) < 0) continue;
    if (!pronto || (int32_t)(s.prazoMs - pronto->prazoMs) < 0) pronto = &s;
  }
  if (!pronto) return false;

  saida = pronto->item;
  pronto->usado = false;
  recentes[idxRecentes] = {true, saida.pacote.seq, saida.pacote.tempoMs};
  idxRecentes = (idxRecentes + 1) % RECENTES;
  est.fundidos++;
  for (int r = 0; r < N_RECEPTORES; r++) {
    if (saida.mascara == (1u << r)) est.exclusivos[r]++;
  }
  return true;
}

EstatisticasFusao fusaoEstatisticas() {
  return est;
}
