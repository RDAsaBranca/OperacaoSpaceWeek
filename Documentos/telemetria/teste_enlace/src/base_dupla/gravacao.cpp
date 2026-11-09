#include "gravacao.h"
#include <LittleFS.h>
#include <math.h>
#include <sys/time.h>
#include <time.h>

// Nome proprio do teste (nao mistura com a gravacao da base de voo) e com a
// versao do pacote: registros de outra versao tem outro tamanho.
static const char*    ARQUIVO         = "/voo_teste_v3.bin";
static const uint32_t MARGEM_LIVRE    = 16 * 1024;  // para antes de encher de vez
static const uint32_t PERIODO_SYNC_MS = 1000;       // grava no flash 1x por segundo
static const int16_t  AUSENTE         = INT16_MIN;  // receptor nao entregou o pacote

struct __attribute__((packed)) RegistroBase {
  PacoteTelemetria pacote;                 // 64 bytes
  int64_t  epochMs;                        // hora UTC da recepcao (0 se a base nao sabia a hora)
  uint32_t tBaseMs;                        // millis() da base (primeira copia)
  int16_t  rssiDeci;                       // melhor copia: dBm x 10
  int16_t  snrCenti;                       // melhor copia: dB x 100
  uint8_t  receptores;                     // bit0 = SX1276, bit1 = RYLR998
  int16_t  rssiDeciRx[N_RECEPTORES];       // por receptor, AUSENTE se nao entregou
  int16_t  snrCentiRx[N_RECEPTORES];
};
static_assert(TLM_VERSAO == 3, "pacote mudou de versao: troque o nome em ARQUIVO");
static_assert(sizeof(RegistroBase) == 89, "RegistroBase mudou de tamanho");

static bool     sistemaOk = false;
static File     arquivo;
static bool     ativa = false, cheia = false;
static uint32_t registros = 0, tInicio = 0, tSync = 0, livres = 0;

// Hora UTC atual em ms, ou 0 se o relogio nunca foi acertado pelo dashboard
static int64_t agoraEpochMs() {
  struct timeval tv;
  gettimeofday(&tv, nullptr);
  if (tv.tv_sec < 1600000000) return 0;
  return (int64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

static void atualizarLivres() {
  const size_t total = LittleFS.totalBytes(), usado = LittleFS.usedBytes();
  livres = total > usado ? (uint32_t)(total - usado) : 0;
}

static int16_t paraInt16(float v, float escala) {
  return isfinite(v) ? (int16_t)lroundf(v * escala) : AUSENTE;
}

bool gravacaoIniciarSistema() {
  sistemaOk = LittleFS.begin(true);  // formata na primeira vez
  if (!sistemaOk) return false;
  if (LittleFS.exists(ARQUIVO)) {
    File f = LittleFS.open(ARQUIVO, FILE_READ);
    registros = f ? (uint32_t)(f.size() / sizeof(RegistroBase)) : 0;
    f.close();
  }
  atualizarLivres();
  return true;
}

bool gravacaoComecar() {
  if (!sistemaOk) return false;
  if (ativa) return true;
  atualizarLivres();
  if (livres < MARGEM_LIVRE) { cheia = true; return false; }
  arquivo = LittleFS.open(ARQUIVO, FILE_APPEND);
  if (!arquivo) return false;
  ativa = true;
  cheia = false;
  tInicio = tSync = millis();
  return true;
}

void gravacaoParar() {
  if (!ativa) return;
  arquivo.close();
  ativa = false;
  atualizarLivres();
}

bool gravacaoApagar() {
  if (!sistemaOk || ativa) return false;
  if (LittleFS.exists(ARQUIVO) && !LittleFS.remove(ARQUIVO)) return false;
  registros = 0;
  cheia = false;
  atualizarLivres();
  return true;
}

void gravacaoAnexar(const ItemRecebido& item) {
  if (!ativa) return;
  RegistroBase r;
  r.pacote     = item.pacote;
  r.tBaseMs    = item.tBaseMs;
  const int64_t agora = agoraEpochMs();
  r.epochMs    = agora ? agora - (int64_t)(millis() - item.tBaseMs) : 0;
  r.rssiDeci   = paraInt16(item.rssi, 10.0f);
  r.snrCenti   = paraInt16(item.snr, 100.0f);
  r.receptores = item.receptores;
  for (int k = 0; k < N_RECEPTORES; k++) {
    r.rssiDeciRx[k] = paraInt16(item.rssiRx[k], 10.0f);
    r.snrCentiRx[k] = paraInt16(item.snrRx[k], 100.0f);
  }

  if (arquivo.write(reinterpret_cast<const uint8_t*>(&r), sizeof(r)) != sizeof(r)) {
    gravacaoParar();  // erro de escrita: para e o dashboard mostra
    return;
  }
  registros++;

  if (millis() - tSync >= PERIODO_SYNC_MS) {
    tSync = millis();
    arquivo.flush();  // garante no flash: falta de energia perde no maximo 1 s
    atualizarLivres();
    if (livres < MARGEM_LIVRE) {
      cheia = true;
      gravacaoParar();
    }
  }
}

StatusGravacao gravacaoStatus() {
  StatusGravacao s;
  s.sistemaOk    = sistemaOk;
  s.ativa        = ativa;
  s.cheia        = cheia;
  s.registros    = registros;
  s.bytesArquivo = registros * sizeof(RegistroBase);
  s.bytesLivres  = livres;
  s.duracaoS     = ativa ? (millis() - tInicio) / 1000 : 0;
  return s;
}

static void formatarHora(int64_t epochMs, char* buf, size_t tam) {
  if (epochMs <= 0) { buf[0] = '\0'; return; }
  const time_t seg = (time_t)(epochMs / 1000);
  struct tm t;
  gmtime_r(&seg, &t);
  const size_t n = strftime(buf, tam, "%Y-%m-%dT%H:%M:%S", &t);
  snprintf(buf + n, tam - n, ".%03dZ", (int)(epochMs % 1000));
}

static double deInt16(int16_t v, double escala) {
  return v == AUSENTE ? NAN : v / escala;
}

void gravacaoExportarCsv(Print& saida) {
  saida.printf("hora_utc,t_base_ms,%s,rssi_dbm,snr_db,receptores,"
               "rssi_sx_dbm,snr_sx_db,rssi_ry_dbm,snr_ry_db\n", TLM_COLUNAS);
  if (!sistemaOk || !LittleFS.exists(ARQUIVO)) return;
  File f = LittleFS.open(ARQUIVO, FILE_READ);
  if (!f) return;
  RegistroBase r;
  char linha[340], hora[32];
  while (f.read(reinterpret_cast<uint8_t*>(&r), sizeof(r)) == sizeof(r)) {
    formatarHora(r.epochMs, hora, sizeof(hora));
    pacoteParaCsv(r.pacote, linha, sizeof(linha));
    saida.printf("%s,%lu,%s,%.1f,%.2f,%u,%.1f,%.2f,%.1f,%.2f\n", hora, (unsigned long)r.tBaseMs,
                 linha, r.rssiDeci / 10.0, r.snrCenti / 100.0, (unsigned)r.receptores,
                 deInt16(r.rssiDeciRx[RECEPTOR_SX1276], 10.0), deInt16(r.snrCentiRx[RECEPTOR_SX1276], 100.0),
                 deInt16(r.rssiDeciRx[RECEPTOR_RYLR998], 10.0), deInt16(r.snrCentiRx[RECEPTOR_RYLR998], 100.0));
  }
  f.close();
}
