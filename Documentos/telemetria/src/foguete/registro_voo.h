// registro_voo.h - gravacao do voo na W25Q128 ("caixa-preta")
//
// Cada registro tem 64 bytes (4 por pagina de 256 B), so e acrescentado
// (nunca sobrescreve) e sobrevive a reinicios: ao ligar, o firmware acha
// o fim dos dados e abre uma nova "sessao".
//
// Taxa de gravacao:
//   RAMPA / POUSADO -> 1 registro por segundo
//   SUBIDA / DESCIDA -> todas as amostras (50 Hz), mais os 0,5 s
//                      anteriores a deteccao do lancamento (pre-disparo)
// 16 MB = 262.144 registros = ~87 min a 50 Hz, ou dias a 1 Hz.
#pragma once
#include <Arduino.h>
#include "Protocolo.h"

struct __attribute__((packed)) RegistroVoo {
  PacoteTelemetria pacote;   // 58 bytes, com CRC proprio
  uint32_t pressaoPa;        // pressao bruta do BMP280, para pos-processamento
  uint8_t  sessao;           // incrementa cada vez que o foguete liga
  uint8_t  reservado;
};
static_assert(sizeof(RegistroVoo) == 64, "RegistroVoo deve ter 64 bytes (4 por pagina)");

bool     registroIniciar();
uint8_t  registroStatus();      // STATUS_FLASH_OK / STATUS_FLASH_CHEIA para o pacote
uint8_t  registroSessao();
uint32_t registroUsados();
uint32_t registroCapacidade();
void     registroAmostra(const PacoteTelemetria& p, float pressaoPa, uint32_t agoraMs);
void     registroBaixarCsv(Print& saida);
bool     registroApagarTudo(Print& saida);
