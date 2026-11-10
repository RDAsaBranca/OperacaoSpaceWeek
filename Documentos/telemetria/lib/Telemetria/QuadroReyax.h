// QuadroReyax.h - quadro over-the-air que um REYAX RYLR998 aceita.
//
// O firmware do RYLR998 nao transmite so o <Data> do AT+SEND: ele poe na
// frente um cabecalho de 5 bytes e, na recepcao, descarta em silencio
// qualquer pacote LoRa sem esse cabecalho (engenharia reversa, ver docs/).
//
//   [dst_lo, dst_hi, src_lo, src_hi, len][payload; len]
//
// dst/src = AT+ADDRESS (uint16 little-endian), len = bytes do payload.
// O SX1276 do CanSat monta exatamente isso; o SX1276 da base retira.
#pragma once
#include <stdint.h>
#include <stddef.h>
#include "Protocolo.h"

static constexpr size_t   RYLR_CAB_TAM      = 5;    // A VALIDAR em bancada (teste T1)
static constexpr size_t   RYLR_PAYLOAD_MAX  = 240;  // limite do AT+SEND (guia AT oficial)
static constexpr size_t   RYLR_QUADRO_MAX   = RYLR_CAB_TAM + RYLR_PAYLOAD_MAX;
static constexpr uint16_t RYLR_END_BROADCAST = 0;   // AT+SEND=0: "todos os enderecos"

// Enderecos do enlace (AT+ADDRESS vai de 0 a 65535).
static constexpr uint16_t RYLR_END_CANSAT  = 197;   // origem: e o <Address> do +RCV na base
static constexpr uint16_t RYLR_END_BASE    = 2;     // AT+ADDRESS do RYLR998 da base
static constexpr uint16_t RYLR_END_DESTINO = RYLR_END_BROADCAST;

// Codificacao do payload de aplicacao dentro do quadro.
//  BINARIO: 64 bytes crus  -> quadro de 69 bytes. So depois do teste T3 passar.
//  BASE64 : 88 caracteres  -> quadro de 93 bytes. Seguro para o "ASCII Format" do Reyax.
// So o transmissor usa esta opcao; a base aceita as duas (pelo tamanho).
#define TLM_COD_BINARIO 0
#define TLM_COD_BASE64  1
#ifndef TLM_CODIFICACAO
#define TLM_CODIFICACAO TLM_COD_BASE64
#endif

static constexpr size_t base64Tam(size_t n) { return 4 * ((n + 2) / 3); }

#if TLM_CODIFICACAO == TLM_COD_BINARIO
static constexpr size_t TLM_PAYLOAD_TAM = sizeof(PacoteTelemetria);
#else
static constexpr size_t TLM_PAYLOAD_TAM = base64Tam(sizeof(PacoteTelemetria));
#endif
static constexpr size_t TLM_QUADRO_TAM = RYLR_CAB_TAM + TLM_PAYLOAD_TAM;
static_assert(TLM_PAYLOAD_TAM <= RYLR_PAYLOAD_MAX, "payload maior que o RYLR998 aceita");

struct CabecalhoRylr {
  uint16_t dst;
  uint16_t src;
  uint8_t  len;
};

// Monta o quadro em "out". "payload" pode ja estar em out + RYLR_CAB_TAM.
// Retorna o tamanho total, ou 0 se nao couber.
size_t rylrMontar(uint16_t dst, uint16_t src, const uint8_t* payload, size_t len,
                  uint8_t* out, size_t max);

// Confere e separa o cabecalho. Estrito: len tem que bater com o que veio.
bool rylrAbrir(const uint8_t* quadro, size_t len, CabecalhoRylr& cab, const uint8_t*& payload);

// Telemetria -> payload (conforme TLM_CODIFICACAO). Retorna bytes escritos ou 0.
size_t tlmCodificar(const PacoteTelemetria& p, uint8_t* out, size_t max);

// Payload -> telemetria validada (magic, versao, CRC16). Aceita binario (64) e base64 (88).
bool tlmDecodificar(const uint8_t* payload, size_t len, PacoteTelemetria& p);

// Quadro completo do CanSat: cabecalho Reyax + telemetria codificada.
size_t quadroTelemetria(const PacoteTelemetria& p, uint8_t* out, size_t max);

size_t base64Codificar(const uint8_t* in, size_t len, char* out, size_t max);
size_t base64Decodificar(const char* in, size_t len, uint8_t* out, size_t max);
