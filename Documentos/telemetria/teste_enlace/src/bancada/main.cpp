// ============================================================
//  BANCADA - SX1276 da estacao base como farejador e gerador
//
//  RX: imprime em hex todo pacote LoRa recebido, com o que o header
//      LoRa diz (CRC presente? CR?), RSSI, SNR, erro de frequencia e
//      a leitura do cabecalho Reyax.
//  TX: comandos de uma letra pela USB (115200):
//      h  quadro "HELLO" para o broadcast (dst 0)       -> testes T4
//      u  "HELLO" SEM cabecalho Reyax                    -> T4 (deve ser ignorado)
//      d  quadro "HELLO" para o endereco 3 (nao e o do Reyax) -> T4 (filtragem)
//      e  quadro com len errado (diz 4, leva 5)          -> T4 (comportamento)
//      b  transparencia binaria: 0x00..0xEF e 0xF0..0xFF -> T3
//      t  telemetria ficticia em base64                  -> T5
//      T  telemetria ficticia em binario                 -> T3/T5
//  Hardware: o mesmo da base do teste (ESP32 DevKit + SX1276, pinos de
//  base_dupla/pinos.h). O RYLR998 fica de fora, ligado ao PC.
// ============================================================
#include <Arduino.h>
#include <SPI.h>
#include <RadioLib.h>
#include <string.h>
#include "ConfigEnlace.h"
#include "Protocolo.h"
#include "QuadroReyax.h"
#include "../base_dupla/pinos.h"

// getMod() e protegido no RadioLib 7.x: a subclasse so expoe a leitura de registrador
class SX1276Farejador : public SX1276 {
 public:
  using SX1276::SX1276;
  uint8_t registrador(uint8_t reg, uint8_t msb = 7, uint8_t lsb = 0) {
    return (uint8_t)getMod()->SPIgetRegValue(reg, msb, lsb);
  }
};
static SX1276Farejador radio(new Module(PIN_LORA_NSS, PIN_LORA_DIO0, PIN_LORA_RST, RADIOLIB_NC, SPI));
static volatile bool chegou = false;
static uint16_t seqTeste = 0;

static void IRAM_ATTR aoReceber() { chegou = true; }

static void imprimirHex(const uint8_t* d, size_t n) {
  for (size_t i = 0; i < n; i++) Serial.printf("%02X ", d[i]);
}

static void imprimirPacote() {
  uint8_t buf[256];
  const size_t len = radio.getPacketLength();
  const int16_t st = radio.readData(buf, len);
  // Registradores lidos logo apos o RX, antes de voltar a escutar
  const bool crcNoHeader = radio.registrador(RADIOLIB_SX127X_REG_HOP_CHANNEL, 6, 6) != 0;
  const int  crHeader    = radio.registrador(RADIOLIB_SX127X_REG_MODEM_STAT, 7, 5) >> 5;
  const float rssi = radio.getRSSI();
  const float snr  = radio.getSNR();
  const float df   = radio.getFrequencyError();

  Serial.printf("RX %3u B | st %d | CRC no header %s | CR 4/%d | RSSI %.0f dBm | SNR %.1f dB | dF %.0f Hz\n",
                (unsigned)len, st, crcNoHeader ? "SIM" : "NAO", crHeader + 4, rssi, snr, df);
  Serial.print("   hex: ");
  imprimirHex(buf, len);
  Serial.print("\n   txt: ");
  for (size_t i = 0; i < len; i++) Serial.print((buf[i] >= 32 && buf[i] < 127) ? (char)buf[i] : '.');
  Serial.println();

  CabecalhoRylr cab;
  const uint8_t* payload = nullptr;
  if (rylrAbrir(buf, len, cab, payload)) {
    PacoteTelemetria p;
    const bool tlm = tlmDecodificar(payload, cab.len, p);
    Serial.printf("   Reyax: dst %u src %u len %u -> OK%s\n", cab.dst, cab.src, cab.len,
                  tlm ? " | telemetria valida" : "");
  } else if (len >= RYLR_CAB_TAM) {
    Serial.printf("   Reyax: len no byte 4 = %u, mas vieram %u bytes depois do cabecalho\n",
                  buf[4], (unsigned)(len - RYLR_CAB_TAM));
  }
}

static void transmitir(const uint8_t* q, size_t n, const char* nome) {
  const int16_t st = radio.transmit(q, n);
  Serial.printf("TX %-28s %3u B | st %d | hex: ", nome, (unsigned)n, st);
  imprimirHex(q, n);
  Serial.println();
  chegou = false;  // o DIO0 tambem sobe no fim do TX
  radio.startReceive();
}

static void telemetriaFicticia(PacoteTelemetria& p) {
  memset(&p, 0, sizeof(p));
  p.seq = seqTeste++;
  p.tempoMs = millis();
  p.altitudeCm = 12345;
  p.bateriaMv = 4000;
  pacoteFinalizar(p);
}

static void comando(char c) {
  uint8_t q[RYLR_QUADRO_MAX];
  size_t n = 0;
  switch (c) {
    case 'h':
      n = rylrMontar(RYLR_END_BROADCAST, RYLR_END_CANSAT, (const uint8_t*)"HELLO", 5, q, sizeof(q));
      transmitir(q, n, "HELLO -> broadcast");
      break;
    case 'u':
      transmitir((const uint8_t*)"HELLO", 5, "HELLO sem cabecalho");
      break;
    case 'd':
      n = rylrMontar(3, RYLR_END_CANSAT, (const uint8_t*)"HELLO", 5, q, sizeof(q));
      transmitir(q, n, "HELLO -> endereco 3");
      break;
    case 'e':
      n = rylrMontar(RYLR_END_BROADCAST, RYLR_END_CANSAT, (const uint8_t*)"HELLO", 5, q, sizeof(q));
      q[4] = 4;  // mente o tamanho
      transmitir(q, n, "len errado (4 vs 5)");
      break;
    case 'b': {
      uint8_t todos[240];
      for (int i = 0; i < 240; i++) todos[i] = (uint8_t)i;
      n = rylrMontar(RYLR_END_BROADCAST, RYLR_END_CANSAT, todos, 240, q, sizeof(q));
      transmitir(q, n, "binario 0x00..0xEF");
      delay(50);
      for (int i = 0; i < 16; i++) todos[i] = (uint8_t)(0xF0 + i);
      n = rylrMontar(RYLR_END_BROADCAST, RYLR_END_CANSAT, todos, 16, q, sizeof(q));
      transmitir(q, n, "binario 0xF0..0xFF");
      break;
    }
    case 't': {
      PacoteTelemetria p;
      telemetriaFicticia(p);
      char b64[base64Tam(sizeof(p))];
      const size_t m = base64Codificar((const uint8_t*)&p, sizeof(p), b64, sizeof(b64));
      n = rylrMontar(RYLR_END_DESTINO, RYLR_END_CANSAT, (const uint8_t*)b64, m, q, sizeof(q));
      transmitir(q, n, "telemetria base64");
      break;
    }
    case 'T': {
      PacoteTelemetria p;
      telemetriaFicticia(p);
      n = rylrMontar(RYLR_END_DESTINO, RYLR_END_CANSAT, (const uint8_t*)&p, sizeof(p), q, sizeof(q));
      transmitir(q, n, "telemetria binaria");
      break;
    }
    default:
      break;
  }
}

void setup() {
  Serial.begin(115200);
  delay(300);
  SPI.begin(PIN_LORA_SCK, PIN_LORA_MISO, PIN_LORA_MOSI, PIN_LORA_NSS);
  // 2 dBm: na bancada os radios ficam a ~1 m; potencia alta satura o receptor
  int16_t st = radio.begin(LORA_FREQ_MHZ, LORA_BW_KHZ, LORA_SF, LORA_CR, LORA_SYNC_WORD, 2,
                           LORA_PREAMBULO);
  radio.explicitHeader();
  radio.setCRC(false);    // farejador: aceita quadro com ou sem CRC e mostra o que o header diz
  radio.invertIQ(false);
  radio.autoLDRO();
  radio.setPacketReceivedAction(aoReceber);
  radio.startReceive();
  Serial.printf("# BANCADA: begin %d | %.3f MHz SF%d BW%.0f CR4/%d sync 0x%02X preambulo %d\n",
                st, LORA_FREQ_MHZ, LORA_SF, LORA_BW_KHZ, LORA_CR, LORA_SYNC_WORD, LORA_PREAMBULO);
  Serial.println("# comandos: h u d e b t T");
}

void loop() {
  if (chegou) {
    chegou = false;
    imprimirPacote();
    radio.startReceive();
  }
  if (Serial.available()) comando((char)Serial.read());
}
