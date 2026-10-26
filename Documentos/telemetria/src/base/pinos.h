// pinos.h - mapa de pinos da ESP32 DevKit (estacao base), barramento VSPI
// Se usar uma placa com LoRa integrado (ex.: TTGO LoRa32), troque pelos
// pinos dela.
#pragma once

#define PIN_LORA_SCK    18
#define PIN_LORA_MISO   19
#define PIN_LORA_MOSI   23
#define PIN_LORA_NSS    5
#define PIN_LORA_RST    14
#define PIN_LORA_DIO0   26

#define PIN_LED         2   // LED azul da maioria dos DevKits: pisca a cada pacote
