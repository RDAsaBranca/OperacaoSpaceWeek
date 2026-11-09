// fusao.h - junta as copias do mesmo pacote vindas dos dois receptores
//
// Chave do pacote: (seq, tempoMs). As duas copias tem o mesmo conteudo
// (o CanSat transmitiu uma vez so); tempoMs evita confundir pacotes de
// antes e depois de um reinicio do CanSat, quando seq volta a 0.
//
// Cada pacote novo espera ate "janelaMs" pela copia do outro receptor e
// sai uma unica vez, com RSSI/SNR de quem recebeu. Copia que chega depois
// disso so entra nas estatisticas ("atrasados"). Chamar so da tarefa de recepcao.
#pragma once
#include <stdint.h>
#include "Protocolo.h"

enum Receptor : uint8_t {
  RECEPTOR_SX1276  = 0,
  RECEPTOR_RYLR998 = 1,
  N_RECEPTORES     = 2,
};
static constexpr uint8_t MASCARA_TODOS = (1u << N_RECEPTORES) - 1;

struct CopiaRx {
  float    rssi;  // dBm
  float    snr;   // dB
  uint32_t tMs;   // millis() da base na chegada
};

struct ItemFundido {
  PacoteTelemetria pacote;
  uint8_t mascara;              // bit r = o receptor r entregou este pacote
  CopiaRx rx[N_RECEPTORES];     // so vale onde o bit de "mascara" esta ligado
};

struct EstatisticasFusao {
  uint32_t entregues[N_RECEPTORES];   // pacotes validos entregues por receptor
  uint32_t exclusivos[N_RECEPTORES];  // so este receptor pegou (ganho da diversidade)
  uint32_t atrasados[N_RECEPTORES];   // chegou depois de o registro ter saido
  uint32_t divergentes;               // mesma chave, conteudo diferente (nao deveria ocorrer)
  uint32_t sobrescritos;              // tabela de pendentes cheia
  uint32_t fundidos;                  // registros emitidos
};

void              fusaoIniciar(uint32_t janelaMs);
void              fusaoEntrada(Receptor r, const PacoteTelemetria& p, float rssi, float snr,
                               uint32_t agoraMs);
bool              fusaoSaida(uint32_t agoraMs, ItemFundido& saida);  // um por chamada
EstatisticasFusao fusaoEstatisticas();
