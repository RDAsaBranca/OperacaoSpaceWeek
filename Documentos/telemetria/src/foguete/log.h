// log.h - mensagens de diagnostico pela USB, desligaveis para o voo
#pragma once
#include <Arduino.h>

#ifndef DIAGNOSTICO_USB
#define DIAGNOSTICO_USB 1
#endif

#if DIAGNOSTICO_USB
  #define LOG(...) Serial.printf(__VA_ARGS__)
#else
  // Desligado: o compilador descarta a chamada, mas continua conferindo o formato
  #define LOG(...) do { if (0) Serial.printf(__VA_ARGS__); } while (0)
#endif
