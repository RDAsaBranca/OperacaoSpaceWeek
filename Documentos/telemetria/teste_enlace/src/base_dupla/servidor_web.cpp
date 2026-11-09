#include "servidor_web.h"
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <sys/time.h>
#include "ConfigEnlace.h"
#include "Protocolo.h"
#include "QuadroReyax.h"
#include "config_base.h"
#include "dashboard_html.h"
#include "gravacao.h"
#include "recepcao.h"

static WebServer     servidor(80);
static QueueHandle_t fila = nullptr;

// Historico circular. Cada pacote recebe um indice crescente (1, 2, 3...);
// o dashboard pede "tudo depois do indice N" e assim nunca perde nem repete.
struct EntradaHistorico {
  uint32_t     indice;
  ItemRecebido item;
};
static EntradaHistorico historico[HISTORICO_TAM];
static uint32_t         proximoIndice = 1;

void webDrenarFila() {
  ItemRecebido item;
  while (xQueueReceive(fila, &item, 0) == pdTRUE) {
    EntradaHistorico& e = historico[proximoIndice % HISTORICO_TAM];
    e.indice = proximoIndice++;
    e.item = item;
    gravacaoAnexar(item);
  }
}

// Colunas extras, na mesma ordem de COLUNAS_BASE: melhor copia + cada receptor
// (receptor que nao entregou sai como "nan", que vira NaN no JavaScript)
static const char* const COLUNAS_BASE =
    "rssi_dbm,snr_db,receptores,rssi_sx_dbm,snr_sx_db,rssi_ry_dbm,snr_ry_db";

static int formatarLinha(const ItemRecebido& it, char* buf, size_t tam) {
  int n = pacoteParaCsv(it.pacote, buf, tam);
  if (n < 0 || (size_t)n >= tam) return 0;
  const int m = snprintf(buf + n, tam - n, ",%.1f,%.2f,%u,%.1f,%.2f,%.1f,%.2f", it.rssi, it.snr,
                         (unsigned)it.receptores,
                         it.rssiRx[RECEPTOR_SX1276], it.snrRx[RECEPTOR_SX1276],
                         it.rssiRx[RECEPTOR_RYLR998], it.snrRx[RECEPTOR_RYLR998]);
  if (m < 0 || (size_t)(n + m) >= tam) return 0;
  return n + m;
}

// Texto vindo do modulo (versao, ultimo +ERR) entre aspas, sem quebrar o JSON
static void jsonTexto(String& json, const char* s) {
  json += '"';
  for (; *s; s++) json += (*s >= 32 && *s < 127 && *s != '"' && *s != '\\') ? *s : '_';
  json += '"';
}

static void acertarRelogio() {
  if (!servidor.hasArg("epoch_ms")) return;
  const long long ms = strtoll(servidor.arg("epoch_ms").c_str(), nullptr, 10);
  if (ms < 1600000000000LL) return;
  struct timeval tv;
  tv.tv_sec = (time_t)(ms / 1000);
  tv.tv_usec = (suseconds_t)((ms % 1000) * 1000);
  settimeofday(&tv, nullptr);
}

static void jsonGravacao(String& json) {
  const StatusGravacao g = gravacaoStatus();
  char buf[220];
  snprintf(buf, sizeof(buf),
           "{\"sistema_ok\":%s,\"ativa\":%s,\"cheia\":%s,\"registros\":%lu,"
           "\"bytes\":%lu,\"livres\":%lu,\"duracao_s\":%lu}",
           g.sistemaOk ? "true" : "false", g.ativa ? "true" : "false",
           g.cheia ? "true" : "false", (unsigned long)g.registros,
           (unsigned long)g.bytesArquivo, (unsigned long)g.bytesLivres,
           (unsigned long)g.duracaoS);
  json += buf;
}

static void responderJson(int codigo, const String& json) {
  servidor.sendHeader("Cache-Control", "no-store");
  servidor.send(codigo, "application/json", json);
}

// ---------------- Rotas ----------------
static void rotaDashboard() {
  servidor.sendHeader("Content-Encoding", "gzip");
  servidor.sendHeader("Cache-Control", "no-cache");
  servidor.send_P(200, "text/html; charset=utf-8",
                  reinterpret_cast<const char*>(DASHBOARD_HTML_GZ), DASHBOARD_HTML_GZ_LEN);
}

static void rotaInfo() {
  char buf[900];
  snprintf(buf, sizeof(buf),
           "{\"colunas\":\"%s,%s\",\"versao\":%d,\"periodo_ms\":%d,"
           "\"timeout_ms\":%d,\"freq_mhz\":%.1f,\"sf\":%d,\"bw_khz\":%.0f,\"cr\":%d,"
           "\"sync\":%d,\"preambulo\":%d,\"end_cansat\":%u,\"end_base\":%u,"
           "\"janela_fusao_ms\":%d,\"simulacao\":false,\"rede\":\"%s\"}",
           TLM_COLUNAS, COLUNAS_BASE, TLM_VERSAO, TLM_PERIODO_MS, TLM_TIMEOUT_SINAL_MS,
           LORA_FREQ_MHZ, LORA_SF, LORA_BW_KHZ, LORA_CR, LORA_SYNC_WORD, LORA_PREAMBULO,
           (unsigned)RYLR_END_CANSAT, (unsigned)RYLR_END_BASE, JANELA_FUSAO_MS, WIFI_NOME);
  responderJson(200, String(buf));
}

// Estatisticas de um receptor: {"entregues":..,"exclusivos":..,"atrasados":..}
static void jsonReceptor(String& json, const EstatisticasFusao& f, int r) {
  char buf[96];
  snprintf(buf, sizeof(buf), "{\"entregues\":%lu,\"exclusivos\":%lu,\"atrasados\":%lu",
           (unsigned long)f.entregues[r], (unsigned long)f.exclusivos[r],
           (unsigned long)f.atrasados[r]);
  json += buf;
}

static void rotaDados() {
  const uint32_t desde = servidor.hasArg("desde")
                             ? (uint32_t)strtoul(servidor.arg("desde").c_str(), nullptr, 10)
                             : 0;
  const uint32_t maisAntigo = proximoIndice > HISTORICO_TAM ? proximoIndice - HISTORICO_TAM : 1;
  uint32_t ini = desde + 1;
  if (ini < maisAntigo) ini = maisAntigo;
  if (ini > proximoIndice) ini = proximoIndice;  // cliente "a frente": a base reiniciou
  uint32_t fim = ini + MAX_LINHAS_RESPOSTA;
  if (fim > proximoIndice) fim = proximoIndice;

  const EstatisticasRx e = recepcaoEstatisticas();
  const uint32_t agora = millis();
  const long semSinal = e.temPacote ? (long)(agora - e.tUltimoMs) : -1;

  String json;
  json.reserve(1100 + (fim - ini) * 300);
  char cab[520];
  snprintf(cab, sizeof(cab),
           "{\"ultimo\":%lu,\"mais\":%s,\"radio_ok\":%s,\"reyax_ok\":%s,\"recebidos\":%lu,"
           "\"perdidos\":%lu,\"erros_crc\":%lu,\"erros_radio\":%lu,\"invalidos\":%lu,"
           "\"invalidos_ry\":%lu,\"divergentes\":%lu,\"descartados\":%lu,\"sem_sinal_ms\":%ld,"
           "\"rx\":{\"sx\":",
           (unsigned long)(fim - 1), fim < proximoIndice ? "true" : "false",
           e.radioOk ? "true" : "false", e.reyaxOk ? "true" : "false",
           (unsigned long)e.recebidos, (unsigned long)e.perdidos, (unsigned long)e.errosCrc,
           (unsigned long)e.errosRadio, (unsigned long)e.invalidos,
           (unsigned long)e.invalidosReyax, (unsigned long)e.fusao.divergentes,
           (unsigned long)e.descartadosFila, semSinal);
  json += cab;
  jsonReceptor(json, e.fusao, RECEPTOR_SX1276);
  json += "},\"ry\":";
  jsonReceptor(json, e.fusao, RECEPTOR_RYLR998);
  snprintf(cab, sizeof(cab), ",\"linhas_rcv\":%lu,\"linhas_ruins\":%lu,\"erros_modulo\":%lu,"
           "\"ultimo_erro\":",
           (unsigned long)e.reyax.linhasRcv, (unsigned long)e.reyax.linhasRuins,
           (unsigned long)e.reyax.errosModulo);
  json += cab;
  jsonTexto(json, e.reyax.ultimoErro);
  json += ",\"versao\":";
  jsonTexto(json, e.reyax.versao);
  json += "}},\"gravacao\":";
  jsonGravacao(json);
  json += ",\"linhas\":[";

  char linha[400];
  bool primeira = true;
  for (uint32_t i = ini; i < fim; i++) {
    const EntradaHistorico& h = historico[i % HISTORICO_TAM];
    if (h.indice != i || !formatarLinha(h.item, linha, sizeof(linha))) continue;
    if (!primeira) json += ',';
    json += '"';
    json += linha;  // so digitos, letras, '.', '-' e ',': seguro dentro de aspas
    json += '"';
    primeira = false;
  }
  json += "]}";
  responderJson(200, json);
}

static void rotaHora() {
  acertarRelogio();
  servidor.send(204, "text/plain", "");
}

static void rotaGravacaoIniciar() {
  acertarRelogio();  // o botao manda a hora do celular junto
  if (!gravacaoComecar()) {
    const StatusGravacao g = gravacaoStatus();
    servidor.send(g.cheia ? 507 : 500, "text/plain; charset=utf-8",
                  g.cheia ? "Memoria da base cheia. Baixe e apague a gravacao."
                          : "Nao foi possivel abrir a memoria da base.");
    return;
  }
  String json;
  jsonGravacao(json);
  responderJson(200, json);
}

static void rotaGravacaoParar() {
  gravacaoParar();
  String json;
  jsonGravacao(json);
  responderJson(200, json);
}

static void rotaGravacaoApagar() {
  if (gravacaoStatus().ativa) {
    servidor.send(409, "text/plain; charset=utf-8", "Pare a gravacao antes de apagar.");
    return;
  }
  if (!gravacaoApagar()) {
    servidor.send(500, "text/plain; charset=utf-8", "Nao foi possivel apagar a gravacao.");
    return;
  }
  String json;
  jsonGravacao(json);
  responderJson(200, json);
}

// Envia o CSV em blocos (chunked), sem montar o arquivo inteiro na RAM.
// Entre um bloco e outro, continua tirando pacotes da fila para o historico.
class SaidaEmBlocos : public Print {
 public:
  explicit SaidaEmBlocos(WebServer& s) : srv(s) {}
  using Print::write;
  size_t write(uint8_t c) override {
    buf[n++] = (char)c;
    if (n == sizeof(buf)) enviar();
    return 1;
  }
  size_t write(const uint8_t* dados, size_t len) override {
    for (size_t i = 0; i < len; i++) write(dados[i]);
    return len;
  }
  void enviar() {
    if (n == 0) return;
    srv.sendContent(buf, n);
    n = 0;
    webDrenarFila();
  }

 private:
  WebServer& srv;
  char buf[1400];
  size_t n = 0;
};

static void rotaBaixarCsv() {
  if (gravacaoStatus().ativa) {
    servidor.send(409, "text/plain; charset=utf-8", "Pare a gravacao antes de baixar.");
    return;
  }
  servidor.sendHeader("Content-Disposition", "attachment; filename=\"voo_teste.csv\"");
  servidor.sendHeader("Cache-Control", "no-store");
  servidor.setContentLength(CONTENT_LENGTH_UNKNOWN);
  servidor.send(200, "text/csv; charset=utf-8", "");
  SaidaEmBlocos saida(servidor);
  gravacaoExportarCsv(saida);
  saida.enviar();
  servidor.sendContent("");  // bloco vazio = fim da resposta
}

static void rotaNaoEncontrada() {
  servidor.sendHeader("Location", "/");
  servidor.send(302, "text/plain", "");
}

void webIniciar(QueueHandle_t f) {
  fila = f;
  WiFi.mode(WIFI_AP);
  WiFi.softAP(WIFI_NOME, WIFI_SENHA, WIFI_CANAL, 0, WIFI_MAX_CLIENTES);
  Serial.printf("# [WEB] rede Wi-Fi \"%s\" | dashboard em http://%s\n", WIFI_NOME,
                WiFi.softAPIP().toString().c_str());

  servidor.on("/", HTTP_GET, rotaDashboard);
  servidor.on("/api/info", HTTP_GET, rotaInfo);
  servidor.on("/api/dados", HTTP_GET, rotaDados);
  servidor.on("/api/hora", HTTP_POST, rotaHora);
  servidor.on("/api/gravacao/iniciar", HTTP_POST, rotaGravacaoIniciar);
  servidor.on("/api/gravacao/parar", HTTP_POST, rotaGravacaoParar);
  servidor.on("/api/gravacao/apagar", HTTP_POST, rotaGravacaoApagar);
  servidor.on("/baixar.csv", HTTP_GET, rotaBaixarCsv);
  servidor.onNotFound(rotaNaoEncontrada);
  servidor.begin();
}

void webAtualizar() {
  servidor.handleClient();
  webDrenarFila();
}
