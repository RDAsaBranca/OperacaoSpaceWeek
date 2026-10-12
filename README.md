# CanSat Áries

![Status](https://img.shields.io/badge/Status-Em_Desenvolvimento-orange)

Projeto desenvolvido para a spaceweek.

## Requisitos
- **Massa**: 800g
- **Diâmetro esperado**: 66mm (máximo de 66.3mm)
- **Altura**: 230mm (CanSat 2U) ou 115mm (2 CanSat 1U)
- **Mecânica**: Componentes e estrutura com resistência a vibrações durante o lançamento no foguete.
- **Telemetria**: Transmitir com sucesso os [dados](#formato-do-pacote-de-telemetria) para a estação base via LoRa em 915MHz.
- **Missão**: Avaliação das anomalias magnéticas no Atlântico Sul.

## Etapas a serem feitas
- [ ] Definição dos requisitos
- [ ] Pesquisa e definição dos componentes
- [ ] Desenvolvimento do firmware
- [ ] Desenvolvimento das PCBs
- [ ] Desenvolvimento da estrutura mecânica
- [ ] Integração dos subsistemas e testes

Preencher com x o que for concluído.

## Testes que devem ser feitos
- **Teste de Vibração**
- **Telemetria**

## Componentes
| Subsistema | Categoria / Função | Componente / Modelo |
| :--- | :--- | :--- |
| **OBDH** | Microcontrolador | [ESP32 C3 supermini](./Documentos/OBDH/ESP32C3_chip_Datasheet.pdf) |
| **OBDH** | Sensor IMU | [BNO055](./Documentos/OBDH/BNO055_chip_Datasheet.pdf) |
| **OBDH** | Barômetro | [BMP280 5V](./Documentos/OBDH/BMP280_chip_Datasheet.pdf) (versão de 4 pinos) |
| **OBDH** | GPS | [ATGM336H-5N-31](./Documentos/OBDH/ATGM336H_chip_Datasheet.pdf) |
| **OBDH** | Armazenamento de dados | [W25Q128](./Documentos/OBDH/W25Q128FV_chip_Datasheet.pdf) |
| **EPS** | Bateria | [NCR18650B](./Documentos/EPS/NCR18650_Datasheet_1.pdf) (Panasonic) |
| **EPS** | Regulador de tensão | [134N3P](./Documentos/EPS/134N3P_Datasheet.pdf) ([HT4928S](./Documentos/EPS/HT4928S_Datasheet.pdf)) ou [SX1308](./Documentos/EPS/SX1308_Datasheet.pdf) |
| **EPS** | Carregador | [134N3P](./Documentos/EPS/134N3P_Datasheet.pdf) ([HT4928S](./Documentos/EPS/HT4928S_Datasheet.pdf)) |
| **TT&C** | Módulo LoRa | [SX1276](./Documentos/TT&C/SX1276_chip_Datasheet.pdf) [SMD](./Documentos/TT&C/SX1276_SMD_Datasheet.pdf) |

O datasheet de cada componente pode ser encontrado na pasta de Documentos/{subsistema}/{componente} ou clicando em seu nome na tabela acima.
Mudanças feitas em relação ao CubeSat 1U Galeoerdo:
- GPS [NEO-6M](./Documentos/OBDH/NEO6M_chip_Datasheet.pdf) $\rightarrow$ [ATGM336H-5N-31](./Documentos/OBDH/ATGM336H_chip_Datasheet.pdf)
- Módulo leitor de cartão microSD genérico $\rightarrow$ [W25Q128](./Documentos/OBDH/W25Q128FV_chip_Datasheet.pdf)
- Regulador de tensão [MT3608](./Documentos/EPS/MT3608_chip_Datasheet.pdf) $\rightarrow$ [134N3P](./Documentos/EPS/134N3P_Datasheet.pdf) [HT4928S](./Documentos/EPS/HT4928S_Datasheet.pdf)
- Carregador [TP4056](./Documentos/EPS/TP4056_chip_Datasheet.pdf) $\rightarrow$ [134N3P](./Documentos/EPS/134N3P_Datasheet.pdf) [HT4928S](./Documentos/EPS/HT4928S_Datasheet.pdf)
- Adição do módulo LoRa [SX1276](./Documentos/TT&C/SX1276_chip_Datasheet.pdf) [SMD](./Documentos/TT&C/SX1276_SMD_Datasheet.pdf)

## Formato do Pacote de Telemetria
`ainda a ser decidido`
