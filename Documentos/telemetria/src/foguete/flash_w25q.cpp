#include "flash_w25q.h"
#include <SPI.h>
#include <string.h>
#include "pinos.h"

// Datasheet: Read Data (03h) ate 50 MHz; modos SPI 0 e 3. Usamos 8 MHz,
// folgado para fios de alguns centimetros dividindo o barramento com o radio.
static const SPISettings AJUSTE_SPI(8000000, MSBFIRST, SPI_MODE0);
static const uint32_t TAM_PAGINA = 256;

// Instrucoes (secao 8.1 do datasheet)
enum : uint8_t {
  CMD_WRITE_ENABLE   = 0x06,
  CMD_READ_STATUS1   = 0x05,
  CMD_READ_STATUS2   = 0x35,
  CMD_READ_STATUS3   = 0x15,
  CMD_WRITE_STATUS1  = 0x01,
  CMD_WRITE_STATUS2  = 0x31,
  CMD_WRITE_STATUS3  = 0x11,
  CMD_READ_DATA      = 0x03,
  CMD_PAGE_PROGRAM   = 0x02,
  CMD_CHIP_ERASE     = 0xC7,
  CMD_JEDEC_ID       = 0x9F,
  CMD_RELEASE_PWRDWN = 0xAB,
};

// Bits dos registradores de status (secao 7.1)
enum : uint8_t {
  SR1_BUSY     = 0x01,  // S0
  SR1_WEL      = 0x02,  // S1
  SR1_PROTECAO = 0x7C,  // S2..S6: BP0, BP1, BP2, TB, SEC
  SR2_CMP      = 0x40,  // S14: inverte a protecao dos bits BP
  SR3_WPS      = 0x04,  // S18: liga as travas individuais por bloco
};

// Tempos maximos (secao 9.6): tPP 3 ms, tW 15 ms, tCE 200 s, tRES1 3 us
static const uint32_t TIMEOUT_PROGRAMA_MS = 10;
static const uint32_t TIMEOUT_STATUS_MS   = 50;
static const uint32_t TIMEOUT_APAGAR_MS   = 400000;

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

static uint8_t lerRegistro(uint8_t cmd) {
  selecionar();
  SPI.transfer(cmd);
  uint8_t s = SPI.transfer(0);
  liberar();
  return s;
}

static bool esperarLivre(uint32_t timeoutMs, void (*aCadaSegundo)() = nullptr) {
  const uint32_t inicio = millis();
  uint32_t ultimoAviso = inicio;
  while (lerRegistro(CMD_READ_STATUS1) & SR1_BUSY) {
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
  return (lerRegistro(CMD_READ_STATUS1) & SR1_WEL) != 0;
}

static bool escreverRegistro(uint8_t cmd, uint8_t valor) {
  if (!habilitarEscrita()) return false;
  selecionar();
  SPI.transfer(cmd);
  SPI.transfer(valor);
  liberar();
  return esperarLivre(TIMEOUT_STATUS_MS);
}

// Deixa a memoria inteira gravavel. De fabrica ja vem assim, mas um chip
// usado antes pode ter protecoes ligadas, e o chip IGNORA em silencio
// gravacoes em area protegida.
static void removerProtecoes() {
  // CMP=1 inverte a protecao: com BP=0 a memoria INTEIRA fica travada
  uint8_t sr2 = lerRegistro(CMD_READ_STATUS2);
  if (sr2 & SR2_CMP) escreverRegistro(CMD_WRITE_STATUS2, sr2 & (uint8_t)~SR2_CMP);

  // WPS=1 usa travas por bloco, que voltam todas para 1 a cada energizacao
  uint8_t sr3 = lerRegistro(CMD_READ_STATUS3);
  if (sr3 & SR3_WPS) escreverRegistro(CMD_WRITE_STATUS3, sr3 & (uint8_t)~SR3_WPS);

  // BP0..BP2, TB, SEC em zero = nenhuma regiao protegida (com CMP=0)
  uint8_t sr1 = lerRegistro(CMD_READ_STATUS1);
  if (sr1 & SR1_PROTECAO) escreverRegistro(CMD_WRITE_STATUS1, sr1 & (uint8_t)~SR1_PROTECAO);
}

bool flashIniciar() {
  pinMode(PIN_FLASH_CS, OUTPUT);
  digitalWrite(PIN_FLASH_CS, HIGH);

  comandoSimples(CMD_RELEASE_PWRDWN);  // caso o chip esteja em power-down
  delayMicroseconds(50);               // tRES1 = 3 us

  selecionar();
  SPI.transfer(CMD_JEDEC_ID);
  uint8_t fabricante = SPI.transfer(0);  // Winbond = 0xEF
  uint8_t tipo       = SPI.transfer(0);  // 0x40 em modo SPI
  uint8_t capacidade = SPI.transfer(0);  // 0x18 = 2^24 = 16 MB
  liberar();
  jedecId = ((uint32_t)fabricante << 16) | ((uint32_t)tipo << 8) | capacidade;

  // 0x00/0xFF = ninguem respondendo. Ate 16 MB, o limite do endereco de 3 bytes.
  if (fabricante == 0x00 || fabricante == 0xFF || capacidade < 0x10 || capacidade > 0x18) {
    tamanho = 0;
    return false;
  }
  tamanho = 1UL << capacidade;

  removerProtecoes();
  return true;
}

uint32_t flashJedecId()      { return jedecId; }
uint32_t flashTamanhoBytes() { return tamanho; }

void flashLerStatus(uint8_t sr[3]) {
  sr[0] = lerRegistro(CMD_READ_STATUS1);
  sr[1] = lerRegistro(CMD_READ_STATUS2);
  sr[2] = lerRegistro(CMD_READ_STATUS3);
}

void flashLer(uint32_t endereco, uint8_t* dados, size_t len) {
  selecionar();
  SPI.transfer(CMD_READ_DATA);
  enviarEndereco(endereco);
  for (size_t i = 0; i < len; i++) dados[i] = SPI.transfer(0);
  liberar();
}

bool flashGravar(uint32_t endereco, const uint8_t* dados, size_t len) {
  if (tamanho == 0 || len == 0 || len > TAM_PAGINA) return false;
  // Passar do fim da pagina faria o chip "dar a volta" e sobrescrever o
  // inicio da mesma pagina (secao 8.2.15)
  if ((endereco % TAM_PAGINA) + len > TAM_PAGINA) return false;
  if (endereco + len > tamanho) return false;
  if (!habilitarEscrita()) return false;

  selecionar();
  SPI.transfer(CMD_PAGE_PROGRAM);
  enviarEndereco(endereco);
  for (size_t i = 0; i < len; i++) SPI.transfer(dados[i]);
  liberar();

  if (!esperarLivre(TIMEOUT_PROGRAMA_MS)) return false;

  // Confere: area protegida ou fio solto fazem a gravacao falhar sem aviso
  uint8_t lido[TAM_PAGINA];
  flashLer(endereco, lido, len);
  return memcmp(lido, dados, len) == 0;
}

bool flashApagarTudo(void (*aCadaSegundo)()) {
  if (tamanho == 0 || !habilitarEscrita()) return false;
  comandoSimples(CMD_CHIP_ERASE);
  return esperarLivre(TIMEOUT_APAGAR_MS, aCadaSegundo);
}
