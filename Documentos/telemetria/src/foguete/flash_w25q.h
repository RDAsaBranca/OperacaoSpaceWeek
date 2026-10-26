// flash_w25q.h - driver minimo da flash SPI W25Q128 (16 MB)
// Divide o barramento SPI com o SX1276 (cada um com seu CS).
#pragma once
#include <Arduino.h>

bool     flashIniciar();            // detecta o chip pelo JEDEC ID
uint32_t flashJedecId();            // W25Q128: 0xEF4018 (ou 0xEF7018)
uint32_t flashTamanhoBytes();       // 0 se nao detectada
void     flashLer(uint32_t endereco, uint8_t* dados, size_t len);
// Grava ate 256 bytes SEM cruzar o limite de uma pagina. A regiao precisa
// estar apagada (0xFF): flash NOR so transforma bits 1 em 0.
bool     flashGravar(uint32_t endereco, const uint8_t* dados, size_t len);
// Apaga o chip inteiro: 40 s a 200 s. Chama aCadaSegundo() para progresso.
bool     flashApagarTudo(void (*aCadaSegundo)());
