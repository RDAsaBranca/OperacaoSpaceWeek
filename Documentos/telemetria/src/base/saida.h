// saida.h - formatacao do que a base envia pela USB
//
// Linhas que comecam com "TLM," sao dados em CSV (o script do PC grava).
// Linhas que comecam com "#" sao mensagens para leitura humana.
#pragma once
#include <stdint.h>
#include "Protocolo.h"

void saidaCabecalho();
void saidaCsv(const PacoteTelemetria& p, float rssi, float snr, uint32_t perdidos);
void saidaLegivel(const PacoteTelemetria& p, float rssi, float snr,
                  uint32_t recebidos, uint32_t perdidos);
