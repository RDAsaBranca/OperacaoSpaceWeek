#include "registro_voo.h"
#include <math.h>
#include <string.h>
#include "flash_w25q.h"
#include "log.h"

static const uint32_t TAM_REGISTRO    = sizeof(RegistroVoo);
static const uint32_t PERIODO_SOLO_MS = 1000;  // no solo: 1 registro por segundo
static const uint8_t  TAM_PRE_DISPARO = 25;    // 0,5 s a 50 Hz

static bool     flashDetectada = false;
static bool     ativo = false;
static bool     cheio = false;
static uint32_t capacidade = 0;
static uint32_t proximo = 0;     // indice do proximo registro livre
static uint8_t  sessao = 1;
static uint32_t tUltimoSolo = 0;
static bool     primeiroSolo = true;

// Anel de pre-disparo: guarda as ultimas amostras enquanto esta na rampa,
// para nao perder o inicio da queima (a deteccao leva ~100 ms).
struct EntradaAnel {
  RegistroVoo r;
  bool gravado;
};
static EntradaAnel anel[TAM_PRE_DISPARO];
static uint8_t     anelInicio = 0, anelQtd = 0;

static uint8_t primeiroByte(uint32_t indice) {
  uint8_t b = 0xFF;
  flashLer(indice * TAM_REGISTRO, &b, 1);
  return b;
}

static bool gravar(const RegistroVoo& r) {
  if (!ativo) return false;
  if (proximo >= capacidade) {
    ativo = false;
    cheio = true;
    LOG("[FLASH] memoria cheia, gravacao encerrada\n");
    return false;
  }
  if (!flashGravar(proximo * TAM_REGISTRO, reinterpret_cast<const uint8_t*>(&r), TAM_REGISTRO)) {
    ativo = false;
    LOG("[FLASH] erro de gravacao no registro %lu, gravacao desativada\n", (unsigned long)proximo);
    return false;
  }
  proximo++;
  return true;
}

bool registroIniciar() {
  ativo = cheio = false;
  proximo = 0;
  anelInicio = anelQtd = 0;
  primeiroSolo = true;

  flashDetectada = flashIniciar();
  if (!flashDetectada) {
    LOG("[FLASH] W25Q128 nao encontrada (JEDEC 0x%06lX)\n", (unsigned long)flashJedecId());
    return false;
  }
  capacidade = flashTamanhoBytes() / TAM_REGISTRO;

  const uint8_t b0 = primeiroByte(0);
  if (b0 != 0xFF && b0 != TLM_MAGIC) {
    LOG("[FLASH] conteudo desconhecido na memoria. Apague com: apagar confirmo\n");
    return false;
  }

  // Os registros ocupam um trecho continuo desde o inicio: busca binaria
  // pelo primeiro registro livre (primeiro byte 0xFF).
  uint32_t lo = 0, hi = capacidade;
  while (lo < hi) {
    uint32_t meio = lo + (hi - lo) / 2;
    if (primeiroByte(meio) != 0xFF) lo = meio + 1;
    else hi = meio;
  }
  proximo = lo;

  if (proximo > 0) {
    RegistroVoo ultimo;
    flashLer((proximo - 1) * TAM_REGISTRO, reinterpret_cast<uint8_t*>(&ultimo), TAM_REGISTRO);
    sessao = (uint8_t)(ultimo.sessao + 1);
    if (sessao == 0) sessao = 1;
  } else {
    sessao = 1;
  }

  cheio = proximo >= capacidade;
  ativo = !cheio;
  LOG("[FLASH] JEDEC 0x%06lX | %lu de %lu registros usados (%.1f%%) | sessao %u\n",
      (unsigned long)flashJedecId(), (unsigned long)proximo, (unsigned long)capacidade,
      capacidade ? 100.0f * proximo / capacidade : 0.0f, sessao);
  return ativo;
}

uint8_t registroStatus() {
  return (ativo ? STATUS_FLASH_OK : 0) | (cheio ? STATUS_FLASH_CHEIA : 0);
}

uint8_t  registroSessao()     { return sessao; }
uint32_t registroUsados()     { return proximo; }
uint32_t registroCapacidade() { return capacidade; }

void registroAmostra(const PacoteTelemetria& p, float pressaoPa, uint32_t agoraMs) {
  if (!ativo) return;

  RegistroVoo r;
  memset(&r, 0, sizeof(r));
  r.pacote    = p;
  r.pressaoPa = (pressaoPa > 0) ? (uint32_t)lroundf(pressaoPa) : 0;
  r.sessao    = sessao;

  const bool emVoo = (p.estado == ESTADO_SUBIDA || p.estado == ESTADO_DESCIDA);
  if (emVoo) {
    // Primeiro descarrega o pre-disparo (em ordem), depois a amostra atual
    for (uint8_t i = 0; i < anelQtd; i++) {
      EntradaAnel& e = anel[(anelInicio + i) % TAM_PRE_DISPARO];
      if (!e.gravado) gravar(e.r);
    }
    anelInicio = anelQtd = 0;
    gravar(r);
    return;
  }

  // No solo: grava 1x por segundo e guarda tudo no anel de pre-disparo
  bool gravouAgora = false;
  if (primeiroSolo || agoraMs - tUltimoSolo >= PERIODO_SOLO_MS) {
    tUltimoSolo = agoraMs;
    primeiroSolo = false;
    gravouAgora = gravar(r);
  }

  EntradaAnel* e;
  if (anelQtd < TAM_PRE_DISPARO) {
    e = &anel[(anelInicio + anelQtd) % TAM_PRE_DISPARO];
    anelQtd++;
  } else {
    e = &anel[anelInicio];  // sobrescreve a mais antiga
    anelInicio = (anelInicio + 1) % TAM_PRE_DISPARO;
  }
  e->r = r;
  e->gravado = gravouAgora;
}

void registroBaixarCsv(Print& s) {
  s.printf("# colunas LOG: sessao,%s,pressao_pa,crc_ok\n", TLM_COLUNAS);
  char linha[320];
  for (uint32_t i = 0; i < proximo; i++) {
    RegistroVoo r;
    flashLer(i * TAM_REGISTRO, reinterpret_cast<uint8_t*>(&r), TAM_REGISTRO);
    PacoteTelemetria p;
    const bool crcOk = pacoteValidar(reinterpret_cast<const uint8_t*>(&r.pacote),
                                     sizeof(PacoteTelemetria), p);
    if (!crcOk) p = r.pacote;  // exporta assim mesmo, marcado com crc_ok=0
    pacoteParaCsv(p, linha, sizeof(linha));
    s.printf("LOG,%u,%s,%lu,%d\n", (unsigned)r.sessao, linha,
             (unsigned long)r.pressaoPa, crcOk ? 1 : 0);
  }
  s.printf("FIM,%lu\n", (unsigned long)proximo);
}

static Print* saidaProgresso = nullptr;
static void imprimirPonto() {
  if (saidaProgresso) saidaProgresso->print(".");
}

bool registroApagarTudo(Print& s) {
  if (!flashDetectada && !flashIniciar()) {
    s.println("# flash W25Q128 nao encontrada");
    return false;
  }
  s.println("# apagando a flash inteira (leva de 40 s a 200 s, nao desligue)");
  saidaProgresso = &s;
  const bool ok = flashApagarTudo(imprimirPonto);
  saidaProgresso = nullptr;
  s.println();
  s.println(ok ? "# flash apagada" : "# ERRO ao apagar a flash");
  registroIniciar();
  return ok;
}
