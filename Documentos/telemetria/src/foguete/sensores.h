// sensores.h - BMP280 (barometro), BNO055 (IMU), ATGM336H (GPS) e bateria
// Para trocar algum sensor, mude so sensores.cpp: o resto do firmware
// enxerga apenas a struct LeituraSensores.
//
// Sensor ausente nao trava nada: ele e procurado so na inicializacao e, se
// nao responder, fica de fora (o bit STATUS_* correspondente fica em 0).
// Depois de soldar/ligar um componente, aperte RESET para ele ser procurado.
#pragma once
#include <Arduino.h>

struct LeituraSensores {
  uint8_t  status = 0;               // bits STATUS_* (Protocolo.h)
  float    pressaoPa = 0;
  float    temperaturaC = 0;
  float    acelG[3] = {0, 0, 0};
  float    giroDps[3] = {0, 0, 0};
  float    magUT[3] = {0, 0, 0};     // campo magnetico [uT]
  float    quat[4] = {1, 0, 0, 0};   // w, x, y, z
  uint8_t  calibracao = 0;           // sistema<<6 | giro<<4 | acel<<2 | mag
  double   latitude = 0;             // ultima posicao conhecida (mantida sem fix)
  double   longitude = 0;
  float    altitudeGpsM = 0;
  uint8_t  satelites = 0;
  uint16_t bateriaMv = 0;
};

void  sensoresIniciar();
void  sensoresAtualizarGps();                   // chamar em todo loop()
void  sensoresLer(LeituraSensores& l);
float sensoresPressaoMedia(uint16_t amostras);  // referencia de solo (0 se sem barometro)
void  sensoresImprimirResumo(Print& saida);     // quais sensores foram encontrados
// Uma linha por componente (BMP280, BNO055, GPS, bateria) com a ultima leitura
void  sensoresImprimirDiagnostico(const LeituraSensores& l, Print& saida);
