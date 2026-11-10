#include "gravacao.h"
#include <LittleFS.h>
#include <sys/time.h>
#include <time.h>
#include "config_base.h"

// O nome leva a versao do pacote: registros de outra versao tem outro tamanho
// e nao podem ser lidos com esta struct (o arquivo antigo fica intacto).
static const char*    ARQUIVO        = "/voo_base_v3.bin";
static const uint32_t MARGEM_LIVRE   = 16 * 1024;  // para antes de encher de vez
static const uint32_t PERIODO_SYNC_MS = 1000;      // grava no flash 1x por segundo

struct __attribute__((packed)) RegistroBase {
  PacoteTelemetria pacote;   // 64 bytes
  int64_t  epochMs;          // hora UTC da recepcao (0 se a base nao sabia a hora)
  uint32_t tBaseMs;          // millis() da base
  int16_t  rssiDeci;         // dBm x 10
  int16_t  snrCenti;         // dB x 100
};
static_assert(TLM_VERSAO == 3, "pacote mudou de versao: troque o nome em ARQUIVO");
static_assert(sizeof(RegistroBase) == 80, "RegistroBase mudou de tamanho");

static bool     sistemaOk = false;
static File     arquivo;
static bool     ativa = false, cheia = false;
static bool     automatica = GRAVAR_AO_RECEBER;  // ligada a cada boot
static bool     exportando = false;  // o CSV esta sendo lido: nao abre o arquivo para escrita
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

// Quem para (o operador, a memoria cheia ou um erro de escrita) tambem desliga
// o inicio automatico: senao o proximo pacote religaria a gravacao e nao daria
// para baixar o CSV com o foguete transmitindo. Volta ao religar a base.
void gravacaoParar() {
  automatica = false;
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
  if (!ativa) {
    // Pacote chegando com a gravacao parada durante o download do CSV fica de
    // fora; o primeiro depois do download comeca a gravacao.
    if (!automatica || exportando) return;
    if (!gravacaoComecar()) {
      automatica = false;  // memoria cheia ou com defeito: nao tenta a cada pacote
      Serial.println("# [GRAVACAO] nao foi possivel comecar a gravacao automatica");
      return;
    }
    Serial.printf("# [GRAVACAO] comecou sozinha no pacote seq %u\n", item.pacote.seq);
  }
  RegistroBase r;
  r.pacote   = item.pacote;
  r.tBaseMs  = item.tBaseMs;
  const int64_t agora = agoraEpochMs();
  r.epochMs  = agora ? agora - (int64_t)(millis() - item.tBaseMs) : 0;
  r.rssiDeci = (int16_t)lroundf(item.rssi * 10.0f);
  r.snrCenti = (int16_t)lroundf(item.snr * 100.0f);

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
  s.automatica   = automatica && sistemaOk;
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

void gravacaoExportarCsv(Print& saida) {
  saida.printf("hora_utc,t_base_ms,%s,rssi_dbm,snr_db\n", TLM_COLUNAS);
  if (!sistemaOk || !LittleFS.exists(ARQUIVO)) return;
  File f = LittleFS.open(ARQUIVO, FILE_READ);
  if (!f) return;
  exportando = true;  // a saida drena a fila entre os blocos (gravacaoAnexar)
  RegistroBase r;
  char linha[320], hora[32];
  while (f.read(reinterpret_cast<uint8_t*>(&r), sizeof(r)) == sizeof(r)) {
    formatarHora(r.epochMs, hora, sizeof(hora));
    pacoteParaCsv(r.pacote, linha, sizeof(linha));
    saida.printf("%s,%lu,%s,%.1f,%.2f\n", hora, (unsigned long)r.tBaseMs, linha,
                 r.rssiDeci / 10.0, r.snrCenti / 100.0);
  }
  f.close();
  exportando = false;
}
