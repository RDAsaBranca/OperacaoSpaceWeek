// estado_voo.h - altitude filtrada, velocidade vertical e fase do voo
// ATENCAO: a deteccao de fase aqui e INFORMATIVA (telemetria). Nao use
// este codigo para acionar paraquedas: para isso use um altimetro de
// recuperacao dedicado e testado.
#pragma once
#include "Protocolo.h"
#include "sensores.h"

void      estadoVooIniciar(float pressaoSoloPa);
void      estadoVooAtualizar(const LeituraSensores& l, uint32_t agoraMs);
EstadoVoo estadoVooAtual();
float     estadoVooAltitudeM();   // relativa a rampa, filtrada
float     estadoVooAltMaxM();
float     estadoVooVelVertMs();
