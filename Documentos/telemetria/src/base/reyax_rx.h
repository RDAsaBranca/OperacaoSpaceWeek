// reyax_rx.h - receptor REYAX RYLR998 pela UART2 (comandos AT)
// So compilado com RECEPTOR_BASE == RECEPTOR_RYLR998 (ambiente "base_reyax").
// Copia de teste_enlace/src/base_dupla/reyax_rx.*, com o ConfigLoRa.h.
//
// O modulo so recebe (a base nunca usa AT+SEND). Cada quadro valido que ele
// aceita chega como uma linha:
//   +RCV=<origem>,<tam>,<dados>,<RSSI>,<SNR>\r\n
// O parser le <dados> pelo <tam>, byte a byte, entao virgulas, \r ou \n
// dentro do payload nao confundem a leitura.
#pragma once
#include <Arduino.h>

// Configura o modulo (frequencia, SF/BW/CR/preambulo, NETWORKID, endereco)
// e confere que AT+CPIN esta desligado. Bloqueia ~1 s. false = sem resposta.
bool reyaxIniciar();

// Nao bloqueante. true quando um +RCV completo foi lido.
bool reyaxLer(uint16_t& origem, uint8_t* payload, size_t maximo, size_t& len,
              int& rssi, int& snr);

struct EstatisticasReyax {
  uint32_t linhasRcv;     // +RCV completos
  uint32_t linhasRuins;   // +RCV mal formados (ex.: payload binario truncado pelo modulo)
  uint32_t errosModulo;   // linhas +ERR=... espontaneas
  char     ultimoErro[16];
  char     versao[32];    // AT+VER?
};
EstatisticasReyax reyaxEstatisticas();
