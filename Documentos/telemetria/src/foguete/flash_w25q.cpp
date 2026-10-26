#include "flash_w25q.h"
#include <SPI.h>
#include "pinos.h"

static const SPISettings AJUSTE_SPI(8000000, MSBFIRST, SPI_MODE0);
static const uint32_t TAM_PAGINA = 256;

enum : uint8_t {
  CMD_WRITE_ENABLE   = 0x06,
  CMD_WRITE_STATUS1  = 0x01,
  CMD_READ_STATUS1   = 0x05,
  CMD_PAGE_PROGRAM   = 0x02,
  CMD_READ_DATA      = 0x03,
  CMD_CHIP_ERASE     = 0xC7,
  CMD_JEDEC_ID       = 0x9F,
  CMD_RELEASE_PWRDWN = 0xAB,
};

enum : uint8_t {
  SR1_BUSY     = 0x01,
  SR1_WEL      = 0x02,
  SR1_PROTECAO = 0x7C,  // BP0..BP2, TB, SEC
};

static uint32_t jedecId = 0;
static uint32_t tamanho = 0;

static inline void selecionar() {
  SPI.beginTransaction(AJUSTE_SPI);
  digitalWrite(PIN_FLASH_CS, LOW);
}

static inline void liberar() {
  digitalWrite(PIN_FLASH_CS, HIGH);
  SPI.endTransaction();
}

static void comandoSimples(uint8_t cmd) {
  selecionar();
  SPI.transfer(cmd);
  liberar();
}

static void enviarEndereco(uint32_t a) {
  SPI.transfer((uint8_t)(a >> 16));
  SPI.transfer((uint8_t)(a >> 8));
  SPI.transfer((uint8_t)a);
}

static uint8_t lerStatus() {
  selecionar();
  SPI.transfer(CMD_READ_STATUS1);
  uint8_t s = SPI.transfer(0);
  liberar();
  return s;
}

static bool esperarLivre(uint32_t timeoutMs, void (*aCadaSegundo)() = nullptr) {
  const uint32_t inicio = millis();
  uint32_t ultimoAviso = inicio;
  while (lerStatus() & SR1_BUSY) {
    const uint32_t agora = millis();
    if (agora - inicio > timeoutMs) return false;
    if (aCadaSegundo) {
      if (agora - ultimoAviso >= 1000) { ultimoAviso = agora; aCadaSegundo(); }
      delay(10);
    }
  }
  return true;
}

static bool habilitarEscrita() {
  comandoSimples(CMD_WRITE_ENABLE);
  return (lerStatus() & SR1_WEL) != 0;
}

bool flashIniciar() {
  pinMode(PIN_FLASH_CS, OUTPUT);
  digitalWrite(PIN_FLASH_CS, HIGH);

  comandoSimples(CMD_RELEASE_PWRDWN);  // caso o chip esteja em power-down
  delayMicroseconds(50);

  selecionar();
  SPI.transfer(CMD_JEDEC_ID);
  uint8_t fabricante = SPI.transfer(0);
  uint8_t tipo       = SPI.transfer(0);
  uint8_t capacidade = SPI.transfer(0);
  liberar();
  jedecId = ((uint32_t)fabricante << 16) | ((uint32_t)tipo << 8) | capacidade;

  // 0x00/0xFF = ninguem respondendo. Capacidade 0x18 = 2^24 = 16 MB, que e
  // tambem o limite do endereco de 3 bytes usado aqui.
  if (fabricante == 0x00 || fabricante == 0xFF || capacidade < 0x10 || capacidade > 0x18) {
    tamanho = 0;
    return false;
  }
  tamanho = 1UL << capacidade;

  // Remove protecao de escrita por blocos, se alguma vier ligada
  if (lerStatus() & SR1_PROTECAO) {
    if (habilitarEscrita()) {
      selecionar();
      SPI.transfer(CMD_WRITE_STATUS1);
      SPI.transfer(0x00);
      liberar();
      esperarLivre(50);
    }
  }
  return true;
}

uint32_t flashJedecId()      { return jedecId; }
uint32_t flashTamanhoBytes() { return tamanho; }

void flashLer(uint32_t endereco, uint8_t* dados, size_t len) {
  selecionar();
  SPI.transfer(CMD_READ_DATA);
  enviarEndereco(endereco);
  for (size_t i = 0; i < len; i++) dados[i] = SPI.transfer(0);
  liberar();
}

bool flashGravar(uint32_t endereco, const uint8_t* dados, size_t len) {
  if (tamanho == 0 || len == 0 || len > TAM_PAGINA) return false;
  if ((endereco % TAM_PAGINA) + len > TAM_PAGINA) return false;  // cruzaria a pagina
  if (endereco + len > tamanho) return false;
  if (!habilitarEscrita()) return false;

  selecionar();
  SPI.transfer(CMD_PAGE_PROGRAM);
  enviarEndereco(endereco);
  for (size_t i = 0; i < len; i++) SPI.transfer(dados[i]);
  liberar();

  return esperarLivre(10);  // tipico < 1 ms, maximo 3 ms
}

bool flashApagarTudo(void (*aCadaSegundo)()) {
  if (tamanho == 0 || !habilitarEscrita()) return false;
  comandoSimples(CMD_CHIP_ERASE);
  return esperarLivre(400000, aCadaSegundo);  // datasheet: ate 200 s
}
