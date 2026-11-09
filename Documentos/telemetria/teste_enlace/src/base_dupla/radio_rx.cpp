#include "radio_rx.h"
#include <SPI.h>
#include <RadioLib.h>
#include "ConfigEnlace.h"
#include "pinos.h"

static SX1276 radio = new Module(PIN_LORA_NSS, PIN_LORA_DIO0, PIN_LORA_RST, RADIOLIB_NC, SPI);

static volatile bool pacoteChegou = false;

// Interrupcao do DIO0: so marca a flag, a leitura e feita na tarefa
static void IRAM_ATTR aoReceber() {
  pacoteChegou = true;
}

bool radioIniciar() {
  SPI.begin(PIN_LORA_SCK, PIN_LORA_MISO, PIN_LORA_MOSI, PIN_LORA_NSS);

  int16_t st = radio.begin(LORA_FREQ_MHZ, LORA_BW_KHZ, LORA_SF, LORA_CR,
                           LORA_SYNC_WORD, LORA_POTENCIA_DBM, LORA_PREAMBULO);
  if (st != RADIOLIB_ERR_NONE) {
    Serial.printf("# [SX1276] begin() falhou, codigo %d (confira fiacao e alimentacao)\n", st);
    return false;
  }
  radio.setCurrentLimit(LORA_LIMITE_CORRENTE_MA);
  // Mesma camada fisica do transmissor e do RYLR998 (secao 2 do documento)
  radio.explicitHeader();
  radio.setCRC(true);     // quadro sem CRC no header LoRa -> RADIOLIB_ERR_LORA_HEADER_DAMAGED
  radio.invertIQ(false);
  radio.autoLDRO();
  radio.setPacketReceivedAction(aoReceber);

  st = radio.startReceive();
  if (st != RADIOLIB_ERR_NONE) {
    Serial.printf("# [SX1276] startReceive() falhou, codigo %d\n", st);
    return false;
  }
  return true;
}

ResultadoRx radioLer(uint8_t* buf, size_t maximo, size_t& len, float& rssi, float& snr) {
  if (!pacoteChegou) return RX_NADA;
  pacoteChegou = false;

  len = radio.getPacketLength();
  if (len > maximo) len = maximo;
  const int16_t st = radio.readData(buf, len);
  rssi = radio.getRSSI();
  snr  = radio.getSNR();

  radio.startReceive();  // volta a escutar imediatamente

  if (st == RADIOLIB_ERR_NONE)         return RX_OK;
  if (st == RADIOLIB_ERR_CRC_MISMATCH) return RX_ERRO_CRC;
  return RX_ERRO;
}
