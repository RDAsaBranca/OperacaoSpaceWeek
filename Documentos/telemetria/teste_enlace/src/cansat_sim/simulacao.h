// simulacao.h - dados FICTICIOS no lugar dos sensores do CanSat
//
// Espelha o que o firmware do foguete junta antes de montar o pacote:
// LeituraSensores (sensores.h) + altitude/velocidade/fase (estado_voo.h).
// Nenhum sensor e lido: os valores saem de um voo inventado.
//
// Perfil (repete a cada 200 s): 20 s na rampa, 2 s de motor, subida ate
// ~850 m, descida de paraquedas a 7 m/s com vento, pouso.
#pragma once
#include <stdint.h>

struct DadosFicticios {
  // estado_voo
  uint8_t  estado;           // EstadoVoo
  float    altitudeM;        // relativa a rampa
  float    altMaxM;
  float    velVertMs;
  // LeituraSensores
  uint8_t  status;           // bits STATUS_*
  float    temperaturaC;
  float    acelG[3];
  float    giroDps[3];
  float    magUT[3];         // campo magnetico no referencial do sensor [uT]
  float    quat[4];          // w, x, y, z
  uint8_t  calibracao;       // sistema<<6 | giro<<4 | acel<<2 | mag
  double   latitude;
  double   longitude;
  float    altitudeGpsM;
  uint8_t  satelites;
  uint16_t bateriaMv;
};

void simulacaoReiniciar(uint32_t agoraMs);  // volta o voo para o inicio da rampa
void simulacaoGerar(uint32_t agoraMs, DadosFicticios& d);
