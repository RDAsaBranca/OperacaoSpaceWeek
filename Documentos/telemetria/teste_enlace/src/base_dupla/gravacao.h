// gravacao.h - gravacao sob demanda na memoria interna da ESP32 (LittleFS)
//
// Igual a da base de voo, mas cada registro guarda tambem o RSSI/SNR de
// cada receptor (89 bytes por registro, arquivo /voo_teste_v3.bin).
// O dashboard pede para iniciar/parar; o CSV e montado no download.
// Com a particao no_ota.csv cabem cerca de 70 minutos a 5 Hz.
#pragma once
#include <Arduino.h>
#include "recepcao.h"

struct StatusGravacao {
  bool     sistemaOk;     // LittleFS montado
  bool     ativa;
  bool     cheia;         // parou sozinha por falta de espaco
  uint32_t registros;
  uint32_t bytesArquivo;
  uint32_t bytesLivres;
  uint32_t duracaoS;      // da gravacao em andamento
};

bool           gravacaoIniciarSistema();
bool           gravacaoComecar();
void           gravacaoParar();
bool           gravacaoApagar();         // so com a gravacao parada
void           gravacaoAnexar(const ItemRecebido& item);
StatusGravacao gravacaoStatus();
void           gravacaoExportarCsv(Print& saida);
