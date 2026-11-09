// comandos.h - comandos pela USB, usados em solo (monitor serial a 115200)
//   info    estado do voo, dos sensores e do radio
//   ajuda   lista os comandos
// Durante SUBIDA e DESCIDA os comandos sao ignorados.
#pragma once

void comandosAtualizar();  // chamar em todo loop()
