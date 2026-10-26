#include "radio_tx.h"
#include <SPI.h>
#include <RadioLib.h>
#include "ConfigLoRa.h"
#include "pinos.h"
#include "log.h"

// SX1276: NSS, DIO0 (interrupcao), RESET, DIO1 (nao usado).
// O barramento SPI e iniciado no main.cpp, porque e dividido com a flash.
static SX1276 radio = new Module(PIN_LORA_NSS, PIN_LORA_DIO0, PIN_LORA_RST, RADIOLIB_NC, SPI);

static const uint32_t TX_TIMEOUT_MS = 1000;  // seguranca caso a interrupcao se perca

static volatile bool txTerminou = false;
static bool     txEmAndamento = false;
static uint32_t tInicioTx = 0;

// Chamada pela interrupcao do DIO0 ao fim da transmissao. So marca a flag:
// nada de SPI ou Serial dentro de interrupcao.
static void IRAM_ATTR aoTerminarTx() {
  txTerminou = true;
}

bool radioIniciar() {
  int16_t st = radio.begin(LORA_FREQ_MHZ, LORA_BW_KHZ, LORA_SF, LORA_CR,
                           LORA_SYNC_WORD, LORA_POTENCIA_DBM, LORA_PREAMBULO);
  if (st != RADIOLIB_ERR_NONE) {
    LOG("[RADIO] begin() falhou, codigo %d (confira fiacao e alimentacao)\n", st);
    return false;
  }
  radio.setCurrentLimit(LORA_LIMITE_CORRENTE_MA);  // o begin() deixa em 60 mA
  radio.setCRC(true);
  radio.setPacketSentAction(aoTerminarTx);
  return true;
}

bool radioOcupado() {
  return txEmAndamento;
}

bool radioEnviar(const uint8_t* dados, size_t len) {
  if (txEmAndamento) return false;
  txTerminou = false;
  int16_t st = radio.startTransmit(dados, len);
  if (st != RADIOLIB_ERR_NONE) {
    LOG("[RADIO] startTransmit() falhou, codigo %d\n", st);
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
    if (!txTerminou) LOG("[RADIO] timeout de TX, liberando o radio\n");
    txTerminou = false;
    txEmAndamento = false;
    radio.finishTransmit();
  }
}

uint32_t radioTempoNoArUs(size_t len) {
  return (uint32_t)radio.getTimeOnAir(len);
}
