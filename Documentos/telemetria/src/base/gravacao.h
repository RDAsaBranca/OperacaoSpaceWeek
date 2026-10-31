// gravacao.h - gravacao sob demanda na memoria interna da ESP32 (LittleFS)
//
// O dashboard pede para iniciar/parar. Os pacotes ficam num arquivo
// binario de 74 bytes por registro, que e convertido para CSV no download.
// Com a particao no_ota.csv cabem cerca de 80 minutos a 5 Hz.
// A gravacao continua mesmo se o celular/notebook desconectar.
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
