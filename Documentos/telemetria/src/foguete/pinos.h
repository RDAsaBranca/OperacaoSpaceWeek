// pinos.h - mapa de pinos e opcoes de hardware da ESP32-C3 SuperMini (foguete)
#pragma once

// ---- Barramento SPI compartilhado: LoRa SX1276 + flash W25Q128 ----
#define PIN_SPI_SCK     4
#define PIN_SPI_MISO    5
#define PIN_SPI_MOSI    6

// LoRa SX1276
#define PIN_LORA_NSS    7
#define PIN_LORA_RST    10
#define PIN_LORA_DIO0   3

// Flash W25Q128 (16 MB). /WP e /HOLD do chip devem ir em 3,3 V.
#define PIN_FLASH_CS    0

// ---- I2C: barometro BMP280 + IMU BNO055 ----
#define PIN_I2C_SDA     8   // tambem e o LED azul da placa (ele pisca com o I2C, normal)
#define PIN_I2C_SCL     9   // tambem e o botao BOOT (nao aperte com o foguete ligado)
#define I2C_FREQ_HZ     100000  // o BNO055 usa clock stretching: 100 kHz e o mais seguro

// ---- GPS ATGM336H-5N-31 (UART1, NMEA a 9600 baud por padrao) ----
#define PIN_GPS_RX      20  // ligar no TX do GPS
#define PIN_GPS_TX      21  // ligar no RX do GPS
#define GPS_BAUD        9600

// ---- Bateria NCR18650B (Li-ion 1S: 4,2 V cheia, ~3,0 V vazia) ----
// Divisor 100k/100k entre o B+ da celula e o GND: 4,2 V vira 2,1 V no pino.
#define PIN_VBAT            1
#define VBAT_FATOR_DIVISOR  2.0f

// ---- Modo do BNO055 ----
// 0 = fusao NDOF: envia orientacao (quaternion), mas o BNO055 trava o
//     acelerometro em +-4 g (satura durante a queima do motor).
// 1 = modo AMG cru: acelerometro em +-16 g, sem orientacao.
// Pode ser trocado no platformio.ini (-DIMU_MODO_ALTO_G=1).
#ifndef IMU_MODO_ALTO_G
#define IMU_MODO_ALTO_G 0
#endif

// GPIO2 esta livre. Ele e pino de "strapping": nao ligue nada que o puxe
// para GND durante o boot.
