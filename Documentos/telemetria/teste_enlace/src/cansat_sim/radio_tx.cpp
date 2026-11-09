#include "radio_tx.h"
#include <SPI.h>
#include <RadioLib.h>
#include "ConfigEnlace.h"
#include "pinos.h"

static SX1276 radio = new Module(PIN_LORA_NSS, PIN_LORA_DIO0, PIN_LORA_RST, RADIOLIB_NC, SPI);

static const uint32_t TX_TIMEOUT_MS = 1000;  // seguranca caso a interrupcao se perca

static volatile bool txTerminou = false;
static bool         pronto = false;
static bool         txEmAndamento = false;
static uint32_t     tInicioTx = 0;
static int16_t      codigoBegin = 0;
static ContadoresTx cont = {};

static void IRAM_ATTR aoTerminarTx() {
  txTerminou = true;  // so a flag: nada de SPI ou Serial na interrupcao
}

bool radioIniciar() {
  codigoBegin = radio.begin(LORA_FREQ_MHZ, LORA_BW_KHZ, LORA_SF, LORA_CR,
                            LORA_SYNC_WORD, LORA_POTENCIA_DBM, LORA_PREAMBULO);
  if (codigoBegin != RADIOLIB_ERR_NONE) {
    pronto = false;
    Serial.printf("[LoRa] FALHA: begin() retornou %d (fiacao, CS no GPIO%d, alimentacao)\n",
                  codigoBegin, PIN_LORA_NSS);
    return false;
  }
  radio.setCurrentLimit(LORA_LIMITE_CORRENTE_MA);  // o begin() deixa em 60 mA
  // Camada fisica que o RYLR998 espera (secao 2 do documento). Sao os
  // padroes do RadioLib, mas ficam explicitos de proposito.
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
    cont.pulados++;
    return false;
  }
  txTerminou = false;
  // startTransmit() copia o quadro para a FIFO do SX1276 na hora:
  // o buffer pode ser reaproveitado logo depois.
  const int16_t st = radio.startTransmit(dados, len);
  if (st != RADIOLIB_ERR_NONE) {
    cont.errosTx++;
    Serial.printf("[LoRa] startTransmit() falhou, codigo %d\n", st);
    return false;
  }
  txEmAndamento = true;
  tInicioTx = millis();
  return true;
}

void radioAtualizar() {
  if (!txEmAndamento) return;
  const bool timeout = (millis() - tInicioTx) > TX_TIMEOUT_MS;
  if (txTerminou || timeout) {
    if (txTerminou) {
      cont.enviados++;
    } else {
      cont.timeouts++;
      Serial.printf("[LoRa] timeout de TX: o DIO0 (GPIO%d) nao avisou o fim\n", PIN_LORA_DIO0);
    }
    txTerminou = false;
    txEmAndamento = false;
    radio.finishTransmit();
  }
}

uint32_t radioTempoNoArUs(size_t len) {
  return pronto ? (uint32_t)radio.getTimeOnAir(len) : 0;
}

ContadoresTx radioContadores() {
  return cont;
}

void radioImprimirDiagnostico(Print& s) {
  if (!pronto) {
    s.printf("[LoRa] FALHA: SX1276 nao respondeu (codigo %d). Nova tentativa a cada 5 s\n",
             codigoBegin);
    return;
  }
  s.printf("[LoRa] ok | %lu enviados | %lu pulados (ocupado) | %lu erros | %lu timeouts\n",
           (unsigned long)cont.enviados, (unsigned long)cont.pulados,
           (unsigned long)cont.errosTx, (unsigned long)cont.timeouts);
}
