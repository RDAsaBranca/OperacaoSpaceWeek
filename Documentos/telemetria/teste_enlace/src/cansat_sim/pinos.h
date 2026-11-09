// pinos.h - SX1276 do CanSat simulado
//
// Padrao: a placa do proprio CanSat (ESP32-C3 SuperMini, PCB V1.7), os
// mesmos pinos de ../../src/foguete/pinos.h. Assim o teste usa o radio e a
// antena que vao voar.
// Com -DPLACA_DEVKIT=1 (ambiente cansat_sim_devkit): ESP32 DevKit com o
// SX1276 ligado como na estacao base (VSPI).
#pragma once

#if PLACA_DEVKIT
#define PIN_SPI_SCK     18
#define PIN_SPI_MISO    19
#define PIN_SPI_MOSI    23
#define PIN_LORA_NSS    5
#define PIN_LORA_RST    14
#define PIN_LORA_DIO0   26
#define PIN_LED         2    // LED azul do DevKit: pisca a cada transmissao
#else
#define PIN_SPI_SCK     4    // (SCK)
#define PIN_SPI_MISO    5    // (MISO)
#define PIN_SPI_MOSI    6    // (MOSI)
#define PIN_LORA_NSS    3    // (CS2)
#define PIN_LORA_RST    2    // (RESET)
#define PIN_LORA_DIO0   0    // (INTER) fim de transmissao
// Sem LED: na SuperMini o LED azul (GPIO8) e o SDA do I2C dos sensores.
#endif
