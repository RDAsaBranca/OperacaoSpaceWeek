// gravacao.h - gravacao na memoria interna da ESP32 (LittleFS)
//
// Comeca sozinha no primeiro pacote valido depois de ligar a base
// (GRAVAR_AO_RECEBER, config_base.h) ou pelo botao do dashboard. Parar (pelo
// botao, por memoria cheia ou erro) desliga o inicio automatico ate religar.
// Os pacotes ficam num arquivo binario de 80 bytes por registro, que e
// convertido para CSV no download. Cada gravacao e anexada ao fim do arquivo.
// Com a particao no_ota.csv cabem cerca de 75 minutos a 5 Hz.
// A gravacao continua mesmo se o celular/notebook desconectar.
#pragma once
#include <Arduino.h>
#include "recepcao.h"

struct StatusGravacao {
  bool     sistemaOk;     // LittleFS montado
  bool     ativa;
  bool     automatica;    // parada, mas comeca sozinha no proximo pacote
  bool     cheia;         // parou sozinha por falta de espaco
  uint32_t registros;
  uint32_t bytesArquivo;
  uint32_t bytesLivres;
  uint32_t duracaoS;      // da gravacao em andamento
};

bool           gravacaoIniciarSistema();
bool           gravacaoComecar();
void           gravacaoParar();          // tambem desliga o inicio automatico
bool           gravacaoApagar();         // so com a gravacao parada
// Grava o pacote; se a gravacao estiver parada e o inicio automatico ligado,
// comeca a gravar antes.
void           gravacaoAnexar(const ItemRecebido& item);
StatusGravacao gravacaoStatus();
void           gravacaoExportarCsv(Print& saida);
