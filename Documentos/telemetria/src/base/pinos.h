// pinos.h - mapa de pinos da estacao base
// So os pinos do receptor escolhido (RECEPTOR_BASE, em config_base.h) sao
// usados; os do outro ficam livres.
//
// A placa vem do "board" do ambiente no platformio.ini:
//   base, base_reyax  -> ESP32 DevKit V1 com o modulo ligado por fios
//                        (mesma fiacao da base_dupla do teste_enlace)
//   base_heltec       -> Heltec WiFi LoRa 32 V2, com o SX1276 na propria placa
//                        (o board define ARDUINO_HELTEC_WIFI_LORA_32_V2)
#pragma once
#include "config_base.h"

#if defined(ARDUINO_HELTEC_WIFI_LORA_32_V2)
// ================= Heltec WiFi LoRa 32 V2 / V2.1 =================
// Conferido no esquematico da Heltec (WiFi_LoRa_32 V2.1, versao 868-915).
// O SX1276 e soldado na placa: nao ligue nada nos GPIO 5, 14, 18, 19, 26 e 27.
#define PLACA_NOME      "Heltec WiFi LoRa 32 V2"

#if RECEPTOR_BASE != RECEPTOR_SX1276
#error "A Heltec WiFi LoRa 32 V2 ja tem o SX1276 na placa: use RECEPTOR_BASE=RECEPTOR_SX1276"
#endif

// ---- SX1276 da placa (VSPI) ----
#define PIN_LORA_SCK    5
#define PIN_LORA_MISO   19
#define PIN_LORA_MOSI   27
#define PIN_LORA_NSS    18
#define PIN_LORA_RST    14
#define PIN_LORA_DIO0   26  // fim de recepcao (RxDone)
// DIO1 = GPIO35 e DIO2 = GPIO34 tambem estao ligados, mas a recepcao continua
// so precisa do DIO0. A chave de antena e comandada pelo proprio SX1276
// (pino RXTX), entao nao ha GPIO de antena para controlar.

// Outros recursos da placa que a base NAO usa (ficam como o reset deixa):
//   OLED SSD1306 128x64: SDA GPIO4, SCL GPIO15, RST GPIO16. Alimentado pelo
//     Vext, que fica desligado: a tela fica apagada.
//   Vext: GPIO21 em nivel BAIXO liga os 3,3 V do Vext (OLED e pinos "Vext").
//     Na V2.1 o mesmo GPIO21 liga o divisor da bateria (220k/100k no GPIO37).
//   Cristal de 32,768 kHz nos GPIO32 e GPIO33. Botao PRG no GPIO0.

#define PIN_LED         25  // LED branco "LED" da placa (ativo em alto): pisca a cada pacote

#else
// ================= ESP32 DevKit V1 + modulo externo =================
#define PLACA_NOME      "ESP32 DevKit"

// ---- SX1276 no VSPI (ambiente "base") ----
#define PIN_LORA_SCK    18
#define PIN_LORA_MISO   19
#define PIN_LORA_MOSI   23
#define PIN_LORA_NSS    5
#define PIN_LORA_RST    14
#define PIN_LORA_DIO0   26

// ---- REYAX RYLR998 na UART2 a 115200 (ambiente "base_reyax") ----
// Alimentacao e niveis logicos de 3,3 V. Numa placa WROVER os GPIO16/17
// sao da PSRAM: escolha outros pinos.
#define PIN_REYAX_RX    16  // RX2 da ESP32 <- TXD do RYLR998 (pino 4)
#define PIN_REYAX_TX    17  // TX2 da ESP32 -> RXD do RYLR998 (pino 3)
// NRST do RYLR998 (pino 2) nao e usado: fica desligado da ESP32.

#define PIN_LED         2   // LED azul da maioria dos DevKits: pisca a cada pacote
#endif
