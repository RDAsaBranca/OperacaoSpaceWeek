#include "reyax_rx.h"
#include <string.h>
#include "ConfigEnlace.h"
#include "QuadroReyax.h"
#include "pinos.h"

static HardwareSerial& uart = Serial2;
static const uint32_t REYAX_BAUD    = 115200;  // padrao de fabrica (AT+IPR)
static const uint32_t AT_TIMEOUT_MS = 1500;

static EstatisticasReyax est = {};
static bool uartIniciada = false;

static void reiniciarParser();  // definida junto do parser, mais abaixo

// ------------------------------------------------------------------
// Comandos AT (so na inicializacao, bloqueante)
// ------------------------------------------------------------------

// Le uma linha ate \n (sem \r\n). Descarta o que nao couber.
static bool lerLinha(char* buf, size_t tam, uint32_t prazoMs) {
  size_t n = 0;
  while ((int32_t)(prazoMs - millis()) > 0) {
    if (!uart.available()) { delay(1); continue; }
    const char c = (char)uart.read();
    if (c == '\r') continue;
    if (c == '\n') {
      if (n == 0) continue;  // linha vazia
      buf[n] = '\0';
      return true;
    }
    if (n < tam - 1) buf[n++] = c;
  }
  return false;
}

// Envia "cmd\r\n" e devolve a primeira linha de resposta que comeca com '+'
// (+OK, +ERR=x, +ADDRESS=..., +CPIN=...). Ignora +RCV que chegue no meio.
static bool atComando(const char* cmd, char* resp, size_t tam, uint32_t timeoutMs = AT_TIMEOUT_MS) {
  uart.print(cmd);
  uart.print("\r\n");
  const uint32_t prazo = millis() + timeoutMs;
  char linha[64];
  while (lerLinha(linha, sizeof(linha), prazo)) {
    if (strncmp(linha, "+RCV=", 5) == 0) continue;
    if (linha[0] != '+') continue;
    strncpy(resp, linha, tam - 1);
    resp[tam - 1] = '\0';
    return strncmp(linha, "+ERR", 4) != 0;
  }
  resp[0] = '\0';
  return false;
}

static bool esperarLinha(const char* texto, uint32_t timeoutMs) {
  const uint32_t prazo = millis() + timeoutMs;
  char linha[64];
  while (lerLinha(linha, sizeof(linha), prazo)) {
    if (strcmp(linha, texto) == 0) return true;
  }
  return false;
}

// Consulta "AT+<nome>?" e so envia "AT+<nome>=<valor>" se for diferente:
// ADDRESS e NETWORKID sao gravados na flash do modulo a cada escrita.
static bool atGarantir(const char* nome, const char* valor) {
  char cmd[48], resp[64];
  snprintf(cmd, sizeof(cmd), "AT+%s?", nome);
  if (atComando(cmd, resp, sizeof(resp))) {
    const char* igual = strchr(resp, '=');
    if (igual && strcmp(igual + 1, valor) == 0) return true;
  }
  snprintf(cmd, sizeof(cmd), "AT+%s=%s", nome, valor);
  const bool ok = atComando(cmd, resp, sizeof(resp)) && strcmp(resp, "+OK") == 0;
  Serial.printf("# [REYAX] %s -> %s\n", cmd, ok ? "ok" : resp);
  return ok;
}

static int codigoBandaReyax(float bwKhz) {
  if (bwKhz < 200.0f) return 7;  // 125 kHz
  if (bwKhz < 400.0f) return 8;  // 250 kHz
  return 9;                      // 500 kHz
}

// NETWORKID = sync word lido como decimal (18 = 0x12). Faixa aceita: 3..15 e 18.
static_assert(LORA_SYNC_WORD == 18 || (LORA_SYNC_WORD >= 3 && LORA_SYNC_WORD <= 15),
              "o RYLR998 so aceita NETWORKID 3..15 ou 18");
static_assert(LORA_SYNC_WORD == 18 || LORA_PREAMBULO == 12,
              "com NETWORKID != 18 o RYLR998 so aceita preambulo 12");

bool reyaxIniciar() {
  if (!uartIniciada) {  // pode ser chamada de novo apos falha: a UART abre uma vez so
    uart.setRxBufferSize(1024);  // antes do begin(): folga se a tarefa atrasar
    uart.begin(REYAX_BAUD, SERIAL_8N1, PIN_REYAX_RX, PIN_REYAX_TX);
    uartIniciada = true;
  }

#ifdef PIN_REYAX_RST
  pinMode(PIN_REYAX_RST, OUTPUT);
  digitalWrite(PIN_REYAX_RST, LOW);
  delay(150);                       // datasheet: NRST em nivel baixo por >= 100 ms
  digitalWrite(PIN_REYAX_RST, HIGH);
  esperarLinha("+READY", 2000);
#endif

  char resp[64];
  if (!atComando("AT", resp, sizeof(resp))) {
    Serial.println("# [REYAX] sem resposta ao AT (fiacao TX/RX cruzada? 3,3 V?)");
    return false;
  }

  // AT+CPIN muda o que vai no ar. Precisa estar sem senha; ela some no reset.
  if (atComando("AT+CPIN?", resp, sizeof(resp)) && strstr(resp, "No Password") == nullptr) {
    Serial.printf("# [REYAX] senha ativa (%s): reiniciando o modulo para apaga-la\n", resp);
    atComando("AT+RESET", resp, sizeof(resp));
    esperarLinha("+READY", 2000);
  }

  char valor[32];
  bool ok = true;
  snprintf(valor, sizeof(valor), "%lu", (unsigned long)LORA_FREQ_HZ);
  ok &= atGarantir("BAND", valor);  // sem ",M": nao grava na flash, refeito a cada boot
  snprintf(valor, sizeof(valor), "%d,%d,%d,%d", LORA_SF, codigoBandaReyax(LORA_BW_KHZ),
           LORA_CR - 4, LORA_PREAMBULO);
  ok &= atGarantir("PARAMETER", valor);
  snprintf(valor, sizeof(valor), "%d", LORA_SYNC_WORD);
  ok &= atGarantir("NETWORKID", valor);
  snprintf(valor, sizeof(valor), "%u", (unsigned)RYLR_END_BASE);
  ok &= atGarantir("ADDRESS", valor);

  if (atComando("AT+VER?", resp, sizeof(resp))) {
    strncpy(est.versao, resp, sizeof(est.versao) - 1);
  }
  Serial.printf("# [REYAX] %s | %s\n", ok ? "configurado" : "FALHA na configuracao", est.versao);
  reiniciarParser();  // o que sobrou da configuracao nao e +RCV
  return ok;
}

// ------------------------------------------------------------------
// Parser de +RCV (nao bloqueante, byte a byte)
// ------------------------------------------------------------------
enum EstadoParser : uint8_t { PREFIXO, IGNORA_LINHA, ORIGEM, TAMANHO, DADOS, VIRGULA, RSSI, SNR };

static EstadoParser estado = PREFIXO;
static const char   PREFIXO_RCV[] = "+RCV=";
static uint8_t      idxPrefixo = 0;
static uint32_t     valor = 0;
static uint8_t      digitos = 0;
static bool         negativo = false;
static uint16_t     origemAtual = 0;
static size_t       tamAtual = 0, nDados = 0;
static int          rssiAtual = 0;
static uint8_t      dados[RYLR_PAYLOAD_MAX];
static char         outraLinha[24];
static uint8_t      nOutra = 0;

static void novoCampo() { valor = 0; digitos = 0; negativo = false; }

static void reiniciarParser() {
  estado = PREFIXO;
  idxPrefixo = 0;
  nOutra = 0;
}

static void linhaRuim() {
  est.linhasRuins++;
  nOutra = 0;
  estado = IGNORA_LINHA;
}

// Acumula um inteiro com sinal opcional. Retorna false em caractere invalido.
static bool acumular(char c, uint8_t maxDigitos) {
  if (c == '-' && digitos == 0 && !negativo) { negativo = true; return true; }
  if (c < '0' || c > '9' || digitos >= maxDigitos) return false;
  valor = valor * 10 + (uint32_t)(c - '0');
  digitos++;
  return true;
}

bool reyaxLer(uint16_t& origem, uint8_t* payload, size_t maximo, size_t& len,
              int& rssi, int& snr) {
  while (uart.available()) {
    const char c = (char)uart.read();
    switch (estado) {
      case PREFIXO:
        if (c == '\r' || c == '\n') { idxPrefixo = 0; break; }
        if (c == PREFIXO_RCV[idxPrefixo]) {
          if (++idxPrefixo == sizeof(PREFIXO_RCV) - 1) { novoCampo(); estado = ORIGEM; }
        } else {  // outra linha (+OK, +ERR=12, +READY...): guarda para o diagnostico
          memcpy(outraLinha, PREFIXO_RCV, idxPrefixo);
          nOutra = idxPrefixo;
          outraLinha[nOutra++] = c;
          estado = IGNORA_LINHA;
        }
        break;

      case IGNORA_LINHA:
        if (c == '\n' || c == '\r') {
          outraLinha[nOutra] = '\0';
          if (strncmp(outraLinha, "+ERR", 4) == 0) {
            est.errosModulo++;
            strncpy(est.ultimoErro, outraLinha, sizeof(est.ultimoErro) - 1);
          }
          nOutra = 0;
          idxPrefixo = 0;
          estado = PREFIXO;
        } else if (nOutra < sizeof(outraLinha) - 1) {
          outraLinha[nOutra++] = c;
        }
        break;

      case ORIGEM:
        if (c == ',' && digitos > 0 && valor <= 65535) {
          origemAtual = (uint16_t)valor; novoCampo(); estado = TAMANHO;
        } else if (!acumular(c, 5) || negativo) {
          linhaRuim();
        }
        break;

      case TAMANHO:
        if (c == ',' && digitos > 0 && valor >= 1 && valor <= RYLR_PAYLOAD_MAX) {
          tamAtual = valor; nDados = 0; estado = DADOS;
        } else if (!acumular(c, 3) || negativo) {
          linhaRuim();
        }
        break;

      case DADOS:  // exatamente <tam> bytes, sejam eles quais forem
        dados[nDados++] = (uint8_t)c;
        if (nDados == tamAtual) estado = VIRGULA;
        break;

      case VIRGULA:
        if (c == ',') { novoCampo(); estado = RSSI; } else { linhaRuim(); }
        break;

      case RSSI:
        if (c == ',' && digitos > 0) {
          rssiAtual = negativo ? -(int)valor : (int)valor; novoCampo(); estado = SNR;
        } else if (!acumular(c, 4)) {
          linhaRuim();
        }
        break;

      case SNR:
        if ((c == '\r' || c == '\n') && digitos > 0) {
          const int snrAtual = negativo ? -(int)valor : (int)valor;
          idxPrefixo = 0;
          estado = PREFIXO;
          if (tamAtual > maximo) { est.linhasRuins++; break; }
          memcpy(payload, dados, tamAtual);
          origem = origemAtual;
          len = tamAtual;
          rssi = rssiAtual;
          snr = snrAtual;
          est.linhasRcv++;
          return true;  // o resto do buffer fica para a proxima chamada
        }
        if (!acumular(c, 4)) linhaRuim();
        break;
    }
  }
  return false;
}

EstatisticasReyax reyaxEstatisticas() {
  return est;
}
