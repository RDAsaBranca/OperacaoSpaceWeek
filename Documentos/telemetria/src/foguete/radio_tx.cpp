#include "radio_tx.h"
#include <SPI.h>
#include <RadioLib.h>
#include "ConfigLoRa.h"
#include "Protocolo.h"
#include "QuadroReyax.h"
#include "pinos.h"
#include "log.h"

// SX1276: NSS, DIO0 (interrupcao), RESET, DIO1 (nao usado).
// O barramento SPI e iniciado no main.cpp.
static SX1276 radio = new Module(PIN_LORA_NSS, PIN_LORA_DIO0, PIN_LORA_RST, RADIOLIB_NC, SPI);

static const uint32_t TX_TIMEOUT_MS = 1000;  // seguranca caso a interrupcao se perca

static volatile bool txTerminou = false;
static bool     pronto = false;
static bool     txEmAndamento = false;
static uint32_t tInicioTx = 0;
static int16_t  codigoBegin = 0;

// Contadores para o diagnostico
static uint32_t enviados = 0;   // transmissoes concluidas (DIO0 avisou o fim)
static uint32_t pulados = 0;    // pacotes descartados porque o radio ainda estava transmitindo
static uint32_t errosTx = 0;    // startTransmit() recusou
static uint32_t timeouts = 0;   // o DIO0 nao avisou o fim a tempo

// Chamada pela interrupcao do DIO0 ao fim da transmissao. So marca a flag:
// nada de SPI ou Serial dentro de interrupcao.
static void IRAM_ATTR aoTerminarTx() {
  txTerminou = true;
}

bool radioIniciar() {
  codigoBegin = radio.begin(LORA_FREQ_MHZ, LORA_BW_KHZ, LORA_SF, LORA_CR,
                            LORA_SYNC_WORD, LORA_POTENCIA_DBM, LORA_PREAMBULO);
  if (codigoBegin != RADIOLIB_ERR_NONE) {
    pronto = false;
    LOG("[LoRa] FALHA: begin() retornou %d (confira fiacao, CS no GPIO%d e alimentacao)\n",
        codigoBegin, PIN_LORA_NSS);
    return false;
  }
  radio.setCurrentLimit(LORA_LIMITE_CORRENTE_MA);  // o begin() deixa em 60 mA
  // Camada fisica que o RYLR998 espera (docs/arquitetura_enlace_reyax.md,
  // secao 2). Sao os padroes do RadioLib, mas ficam explicitos de proposito.
  radio.explicitHeader();   // header explicito: len, CR e flag de CRC vao no ar
  radio.setCRC(true);       // CRC16 de hardware no fim do payload
  radio.invertIQ(false);    // IQ normal
  radio.autoLDRO();         // SF7/125 kHz: simbolo de 1,024 ms -> LDRO desligado
  radio.setPacketSentAction(aoTerminarTx);
  txEmAndamento = false;
  txTerminou = false;
  pronto = true;
  return true;
}

bool radioPronto() {
  return pronto;
}

bool radioEnviar(const uint8_t* dados, size_t len) {
  if (!pronto) return false;
  if (txEmAndamento) {
    pulados++;
    return false;
  }
  txTerminou = false;
  int16_t st = radio.startTransmit(dados, len);
  if (st != RADIOLIB_ERR_NONE) {
    errosTx++;
    LOG("[LoRa] startTransmit() falhou, codigo %d\n", st);
    return false;
  }
  txEmAndamento = true;
  tInicioTx = millis();
  return true;
}

void radioAtualizar() {
  if (!txEmAndamento) return;
  bool timeout = (millis() - tInicioTx) > TX_TIMEOUT_MS;
  if (txTerminou || timeout) {
    if (txTerminou) {
      enviados++;
    } else {
      timeouts++;
      LOG("[LoRa] timeout de TX: o DIO0 (GPIO%d) nao avisou o fim, liberando o radio\n",
          PIN_LORA_DIO0);
    }
    txTerminou = false;
    txEmAndamento = false;
    radio.finishTransmit();
  }
}

uint32_t radioTempoNoArUs(size_t len) {
  return pronto ? (uint32_t)radio.getTimeOnAir(len) : 0;
}

void radioImprimirParametros(Print& s) {
  s.printf("[LoRa]   enlace %.1f MHz | SF%d | BW %.0f kHz | CR 4/%d | sync 0x%02X (AT+NETWORKID=%d)"
           " | preambulo %d | header explicito, CRC, IQ normal\n",
           LORA_FREQ_MHZ, LORA_SF, LORA_BW_KHZ, LORA_CR, LORA_SYNC_WORD, LORA_SYNC_WORD,
           LORA_PREAMBULO);
  s.printf("[LoRa]   quadro %u B = cabecalho Reyax %u B (dst %u, src %u) + %s %u B"
           " (PacoteTelemetria v%d, %u B)\n",
           (unsigned)TLM_QUADRO_TAM, (unsigned)RYLR_CAB_TAM, (unsigned)RYLR_END_DESTINO,
           (unsigned)RYLR_END_CANSAT, TLM_CODIFICACAO == TLM_COD_BINARIO ? "binario" : "Base64",
           (unsigned)TLM_PAYLOAD_TAM, TLM_VERSAO, (unsigned)sizeof(PacoteTelemetria));
  if (!pronto) return;  // o tempo no ar so sai com o SX1276 configurado
  const uint32_t toaUs = radioTempoNoArUs(TLM_QUADRO_TAM);
  s.printf("[LoRa]   tempo no ar %.1f ms | periodo %d ms%s\n", toaUs / 1000.0f, TLM_PERIODO_MS,
           toaUs / 1000 >= TLM_PERIODO_MS
               ? " | AVISO: tempo no ar >= periodo. Aumente TLM_PERIODO_MS ou reduza o SF"
               : "");
}

void radioImprimirDiagnostico(Print& s) {
  if (!pronto) {
    s.printf("[LoRa]   FALHA: SX1276 nao respondeu (codigo %d). Nova tentativa a cada 5 s\n",
             codigoBegin);
    return;
  }
  s.printf("[LoRa]   ok    | %lu enviados | %lu pulados (ocupado) | %lu erros | %lu timeouts",
           (unsigned long)enviados, (unsigned long)pulados, (unsigned long)errosTx,
           (unsigned long)timeouts);
  if (timeouts > 0 && enviados == 0) s.printf(" | DIO0 (GPIO%d) nao chega?", PIN_LORA_DIO0);
  s.println();
}
