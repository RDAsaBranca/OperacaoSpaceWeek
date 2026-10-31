// flash_w25q.h - driver da flash SPI W25Q128FV (16 MB), conforme o datasheet
// Winbond W25Q128FV rev. M. Divide o barramento SPI com o SX1276 (cada um
// com seu CS). Usa so instrucoes SPI padrao (modo 0, endereco de 3 bytes).
#pragma once
#include <Arduino.h>

bool     flashIniciar();            // detecta o chip pelo JEDEC ID e remove protecoes
uint32_t flashJedecId();            // W25Q128FV: 0xEF4018 (modo SPI)
uint32_t flashTamanhoBytes();       // 0 se nao detectada
void     flashLerStatus(uint8_t sr[3]);  // registradores de status 1, 2 e 3
void     flashLer(uint32_t endereco, uint8_t* dados, size_t len);
// Grava ate 256 bytes SEM cruzar o limite de uma pagina e confere lendo de
// volta. A regiao precisa estar apagada (0xFF): flash NOR so faz 1 -> 0.
bool     flashGravar(uint32_t endereco, const uint8_t* dados, size_t len);
// Apaga o chip inteiro: 40 s tipico, 200 s maximo. aCadaSegundo() = progresso.
bool     flashApagarTudo(void (*aCadaSegundo)());
