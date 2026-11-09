// saida.h - o que a base do teste envia pela USB
//
// Linhas que comecam com "TLM," sao dados em CSV (uma por pacote fundido).
// Linhas que comecam com "#" sao mensagens para leitura humana.
#pragma once
#include <stdint.h>
#include "Protocolo.h"
#include "recepcao.h"

void saidaCabecalho();
void saidaCsv(const ItemRecebido& item, uint32_t perdidos);
void saidaLegivel(const PacoteTelemetria& p, float rssi, float snr,
                  uint32_t recebidos, uint32_t perdidos);
// Uma linha por segundo comparando os dois receptores
void saidaReceptores(const EstatisticasRx& e, const ItemRecebido& ultimo);
