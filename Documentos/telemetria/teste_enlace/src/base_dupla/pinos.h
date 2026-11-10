// pinos.h - mapa de pinos da ESP32 DevKit (estacao base do teste)
// SX1276 no VSPI (igual a base de voo) e RYLR998 na UART2.
#pragma once

// ---- SX1276 (receptor 1) ----
#define PIN_LORA_SCK    18
#define PIN_LORA_MISO   19
#define PIN_LORA_MOSI   23
#define PIN_LORA_NSS    5
#define PIN_LORA_RST    14
#define PIN_LORA_DIO0   26

// ---- REYAX RYLR998 (receptor 2), UART2 a 115200 ----
// Alimentacao e niveis logicos de 3,3 V. Numa placa WROVER os GPIO16/17
// sao da PSRAM: escolha outros pinos.
#define PIN_REYAX_RX    16  // RX2 da ESP32 <- TXD do RYLR998 (pino 4)
#define PIN_REYAX_TX    17  // TX2 da ESP32 -> RXD do RYLR998 (pino 3)
// NRST do RYLR998 (pino 2) nao e usado: fica desligado da ESP32.

#define PIN_LED         2   // LED azul da maioria dos DevKits: pisca a cada pacote
