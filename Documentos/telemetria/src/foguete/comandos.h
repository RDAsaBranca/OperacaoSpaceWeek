// comandos.h - comandos pela USB, usados em solo antes/depois do voo
//   info             estado dos sensores e da flash
//   baixar           exporta todos os registros da flash em CSV
//   apagar confirmo  apaga a flash inteira
//   ajuda            lista os comandos
// Durante SUBIDA e DESCIDA os comandos sao ignorados.
#pragma once

void comandosAtualizar();  // chamar em todo loop()
