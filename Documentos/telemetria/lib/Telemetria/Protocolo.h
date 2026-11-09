// Protocolo.h
// Formato binario do pacote de telemetria, usado pelos dois firmwares.
//
// O pacote e uma struct "packed" (sem bytes de preenchimento). Funciona
// direto entre ESP32 e ESP32-C3 porque os dois sao little-endian.
// Se mudar qualquer campo, incremente TLM_VERSAO e grave os DOIS lados.
#pragma once
#include <stdint.h>
#include <stddef.h>

#define TLM_MAGIC   0xA5   // primeiro byte de todo pacote valido
#define TLM_VERSAO  3      // v3: magnetometro do BNO055 (v2: quaternion + calibracao)

#define QUAT_ESCALA 16384.0f  // quaternion: 1,0 = 16384 (formato nativo do BNO055)

// Fase do voo, estimada a bordo (apenas informativa)
enum EstadoVoo : uint8_t {
  ESTADO_RAMPA   = 0,  // parado, esperando o lancamento
  ESTADO_SUBIDA  = 1,  // motor + coast ate o apogeu
  ESTADO_DESCIDA = 2,  // apos o apogeu
  ESTADO_POUSADO = 3,  // altitude estavel no solo
};

// Bits do campo "status"
enum : uint8_t {
  STATUS_BARO_OK     = 1 << 0,  // BMP280 respondendo e com leitura plausivel
  STATUS_IMU_OK      = 1 << 1,  // BNO055 respondendo
  STATUS_GPS_OK      = 1 << 2,  // ATGM336H enviando dados
  STATUS_GPS_FIX     = 1 << 3,  // GPS com posicao recente (< 3 s)
  STATUS_ORIENT_OK   = 1 << 4,  // quaternion valido (BNO055 em modo fusao)
  // Bits 5 e 6 eram da flash W25Q128, que saiu do projeto. Ficam livres
  // (sempre 0); o formato do pacote nao mudou.
};

struct __attribute__((packed)) PacoteTelemetria {
  uint8_t  magic;         // TLM_MAGIC
  uint8_t  versao;        // TLM_VERSAO
  uint16_t seq;           // sequencia dos pacotes de radio
  uint32_t tempoMs;       // millis() do foguete
  uint8_t  estado;        // EstadoVoo
  uint8_t  status;        // bits STATUS_*
  int32_t  altitudeCm;    // altitude barometrica relativa a rampa [cm]
  int32_t  altMaxCm;      // altitude maxima registrada [cm]
  int16_t  velVertDms;    // velocidade vertical [dm/s]
  int16_t  tempC100;      // temperatura do BMP280 [0,01 C]
  int16_t  acelMg[3];     // aceleracao X, Y, Z [mg]
  int16_t  giroDps10[3];  // velocidade angular X, Y, Z [0,1 graus/s]
  int16_t  magUT10[3];    // campo magnetico X, Y, Z [0,1 uT] (BNO055: +-1300 uT em X/Y, +-2500 em Z)
  int16_t  quat[4];       // orientacao w, x, y, z [x 16384]
  uint8_t  calibracao;    // BNO055: sistema<<6 | giro<<4 | acel<<2 | mag (cada 0..3)
  int32_t  latE7;         // latitude  [graus * 1e7]
  int32_t  lonE7;         // longitude [graus * 1e7]
  int16_t  altGpsM;       // altitude GPS [m]
  uint8_t  satelites;     // satelites em uso
  uint16_t bateriaMv;     // tensao da bateria 18650 [mV]
  uint16_t crc;           // CRC16-CCITT de todos os bytes anteriores
};

static_assert(sizeof(PacoteTelemetria) == 64, "Tamanho do pacote mudou: revise os dois lados");

// Preenche magic, versao e CRC. Chame por ultimo, antes de transmitir.
void pacoteFinalizar(PacoteTelemetria& p);

// Confere tamanho, magic, versao e CRC. Copia para "saida" se valido.
bool pacoteValidar(const uint8_t* dados, size_t len, PacoteTelemetria& saida);

uint16_t crc16Ccitt(const uint8_t* dados, size_t len);

const char* nomeEstado(uint8_t estado);

// Nomes das colunas geradas por pacoteParaCsv (mesma ordem).
extern const char* const TLM_COLUNAS;

// Converte o pacote em uma linha CSV, em unidades fisicas (sem '\n').
// Usado pela base (copia pela USB e gravacao).
int pacoteParaCsv(const PacoteTelemetria& p, char* buf, size_t tamanho);
