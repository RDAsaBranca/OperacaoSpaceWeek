// pinos.h - mapa de pinos e opcoes de hardware do foguete
//
// Padrao: a placa de voo, ESP32-C3 SuperMini, seguindo o esquematico
// hardware/backup_projetos/Projeto_V1_7.epro2 (PCB V1.7). O nome entre
// parenteses e o nome da net no esquematico.
//
// Com -DPLACA_DEVKIT=1 (ambiente foguete_devkit): ESP32 DevKit V1, so para
// bancada. O SX1276 fica no VSPI, ligado como na estacao base.
#pragma once

#if PLACA_DEVKIT
// ================= ESP32 DevKit V1 (bancada) =================
// Os pinos da C3 nao servem aqui: na ESP32 classica os GPIO6-11 sao da flash
// interna (travam a placa), o GPIO1 e o TX do Serial e o GPIO0 decide o boot.

// ---- SPI (VSPI): LoRa SX1276 ----
#define PIN_SPI_SCK     18
#define PIN_SPI_MISO    19
#define PIN_SPI_MOSI    23
#define PIN_LORA_NSS    5   // strapping, mas tem pull-up interno no reset e o CS fica alto: ok
#define PIN_LORA_RST    14
#define PIN_LORA_DIO0   26  // interrupcao de fim de transmissao

// ---- I2C: barometro BMP280 + IMU BNO055 (pinos padrao da ESP32) ----
#define PIN_I2C_SDA     21
#define PIN_I2C_SCL     22
#define I2C_FREQ_HZ     100000  // o BNO055 usa clock stretching: 100 kHz e o mais seguro

// ---- GPS ATGM336H-5N-31 (UART1, NMEA a 9600 baud por padrao) ----
#define PIN_GPS_RX      16  // RX2 da DevKit, ligado no TX do GPS
#define PIN_GPS_TX      17  // TX2 da DevKit, ligado no RX do GPS
#define GPS_BAUD        9600

// ---- Bateria (opcional na bancada) ----
// Divisor 100k/100k no GPIO34 (ADC1, so entrada). Sem divisor, a leitura e lixo.
#define PIN_VBAT            34
#define VBAT_FATOR_DIVISOR  2.0f

#else
// ================= ESP32-C3 SuperMini (placa de voo, PCB V1.7) =================

// ---- SPI: LoRa SX1276 ----
#define PIN_SPI_SCK     4   // (SCK)
#define PIN_SPI_MISO    5   // (MISO)
#define PIN_SPI_MOSI    6   // (MOSI)

// LoRa SX1276
#define PIN_LORA_NSS    7   // (CS1)   CS do radio
#define PIN_LORA_RST    2   // (RESET) GPIO2 e pino de strapping, ver nota no fim (sem problema)
#define PIN_LORA_DIO0   0   // (INTER) interrupcao de fim de transmissao

// ---- I2C: barometro BMP280 + IMU BNO055 ----
#define PIN_I2C_SDA     8   // (SDA) tambem e o LED azul da placa (ele pisca com o I2C, normal)
#define PIN_I2C_SCL     9   // (SCL) tambem e o botao BOOT (nao aperte com o foguete ligado)
#define I2C_FREQ_HZ     100000  // o BNO055 usa clock stretching: 100 kHz e o mais seguro

// ---- GPS ATGM336H-5N-31 (UART1, NMEA a 9600 baud por padrao) ----
#define PIN_GPS_RX      20  // (ESP_RX) ligado no TX do GPS
#define PIN_GPS_TX      21  // (ESP_TX) ligado no RX do GPS
#define GPS_BAUD        9600

// ---- Bateria NCR18650B (Li-ion 1S: 4,2 V cheia, ~3,0 V vazia) ----
// Divisor 100k/100k entre o B+ da celula e o GND: 4,2 V vira 2,1 V no pino.
#define PIN_VBAT            1   // (V_Bat) ADC1 canal 1
#define VBAT_FATOR_DIVISOR  2.0f

// GPIO2, GPIO8 e GPIO9 sao pinos de "strapping", lidos no instante do reset.
// GPIO9 em nivel alto = boot normal; o pull-up do I2C garante isso (e o
// botao BOOT o puxa para GND). GPIO2 no RESET do SX1276 nao atrapalha: pelo
// datasheet da C3 (v2.4, tabela 3-3, nota 2) ele nao define o modo de boot,
// a Espressif so recomenda um pull-up contra ruido.
#endif

// ---- Modo do BNO055 ----
// 0 = fusao NDOF: envia orientacao (quaternion), mas o BNO055 trava o
//     acelerometro em +-4 g (satura durante a queima do motor).
// 1 = modo AMG cru: acelerometro em +-16 g, sem orientacao.
// Pode ser trocado no platformio.ini (-DIMU_MODO_ALTO_G=1).
#ifndef IMU_MODO_ALTO_G
#define IMU_MODO_ALTO_G 0
#endif
