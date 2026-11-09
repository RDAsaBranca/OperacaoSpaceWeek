# Teste do enlace: SX1276 → SX1276 + RYLR998

Projeto PlatformIO separado para validar, em bancada, o enlace descrito em `../docs/arquitetura_enlace_reyax.md`. Um único quadro transmitido pelo SX1276 do CanSat é recebido ao mesmo tempo por dois rádios na base: um SX1276 e um REYAX RYLR998.

Os firmwares de voo (`../src/foguete` e `../src/base`) não são usados nem alterados aqui. O formato do pacote, porém, é o mesmo do CanSat real: o projeto compila o `../lib/Telemetria/Protocolo.h` (64 bytes, versão 3, com magnetômetro).

| Ambiente | Código | Placa | O que faz |
|---|---|---|---|
| `cansat_sim` | **Código 1** | placa do CanSat (ESP32-C3 SuperMini) | Gera um voo fictício, monta o `PacoteTelemetria` real e transmite o quadro compatível com o Reyax a 5 Hz |
| `cansat_sim_devkit` | Código 1 | ESP32 DevKit + SX1276 (pinos da base) | O mesmo, para quem não quer usar a placa do CanSat |
| `base_dupla` | **Código 2** | ESP32 DevKit + SX1276 + RYLR998 | Recebe pelos dois rádios, junta as cópias, mostra no dashboard, grava e copia pela USB |
| `bancada` | opcional | o hardware da base, sem o RYLR998 | Farejador e gerador de quadros de teste (seção 8 do documento) |

## O quadro

```
[00 00] [C5 00] [58] [ 88 caracteres Base64 do PacoteTelemetria ]   = 93 bytes, ~168 ms no ar
  dst     src    len
 (todos) (197)  (88)
```

- **SX1276 da base:** recebe os 93 bytes, confere o cabeçalho (len e origem 197), descarta os 5 primeiros e decodifica o resto.
- **RYLR998:** o próprio módulo confere e retira o cabeçalho e entrega `+RCV=197,88,<Base64>,<RSSI>,<SNR>`.
- **Os dois:** Base64 → `PacoteTelemetria` → magic, versão e CRC16. A fusão junta as duas cópias pela chave (seq, tempo) e emite um registro por pacote.

Parâmetros de rádio, iguais nos três rádios (`lib/Enlace/ConfigEnlace.h`): 915,2 MHz, SF7, 125 kHz, CR 4/5, sync word 0x12 (`AT+NETWORKID=18`), preâmbulo 12, header explícito, CRC ligado, IQ normal.

**Atenção:** esses parâmetros são diferentes dos firmwares de voo atuais (915,0 MHz, sync 0x2B, preâmbulo 8). O CanSat real não fala com a `base_dupla`, e o `cansat_sim` não fala com a base de voo, até a integração ser feita.

## Ligações da base (`base_dupla`)

| Rádio | Sinal | GPIO da ESP32 DevKit |
|---|---|---|
| SX1276 | SCK / MISO / MOSI / NSS | 18 / 19 / 23 / 5 |
| SX1276 | RESET / DIO0 | 14 / 26 |
| RYLR998 | TXD (pino 4) | 16 (RX2) |
| RYLR998 | RXD (pino 3) | 17 (TX2) |
| RYLR998 | NRST (pino 2) | 4 (opcional) |
| RYLR998 | VDD / GND | 3V3 / GND |

O RYLR998 é de 3,3 V (alimentação e níveis lógicos). Numa ESP32 WROVER os GPIO16/17 são da PSRAM: troque em `src/base_dupla/pinos.h`.

O Código 1 na placa do CanSat usa os mesmos pinos do firmware de voo (`src/cansat_sim/pinos.h`).

## Como rodar

```bash
cd teste_enlace
pio run -e cansat_sim -t upload
pio run -e base_dupla -t upload
pio device monitor -e base_dupla
```

1. **Código 1**, no monitor do `cansat_sim`: ao ligar, ele imprime os parâmetros, o tamanho do quadro e o tempo no ar. Depois, uma linha por segundo:
   ```
   [TX] seq 312 | SUBIDA | alt 640.2 m (max 640.2) | v +62.4 m/s | acel z +0.05 g | mag +4.9 -24.3 -4.4 uT | bat 4.10 V
   [LoRa] ok | 312 enviados | 0 pulados (ocupado) | 0 erros | 0 timeouts
   ```
   Comandos de uma letra: `p` pausa/retoma, `x` imprime o próximo quadro (cabeçalho em hex + Base64 + a linha `+RCV` esperada), `r` reinicia o voo, `i` mostra os parâmetros.
2. **Código 2**, no monitor da `base_dupla`: `# [REYAX] configurado | +VER=...` e `# [SX1276] escutando ...`. Para cada pacote sai uma linha `TLM,...` com RSSI/SNR de cada receptor (`nan` = aquele receptor não pegou). A cada segundo sai um resumo:
   ```
   #    SX1276 ok: 1520 (so ele 12, perda 0.8%) RSSI -48 SNR 9.8 | RYLR998 ok: 1511 (so ele 3, perda 1.4%) RSSI -52 SNR 8.0 | ...
   ```
3. **Dashboard:** conecte na rede `EstacaoBase-Teste` (senha `foguete123`) e abra http://192.168.4.1. É o dashboard da base de voo, com um painel "Receptores da base" (estado, RSSI, SNR, recebidos, perda, só este recebeu, atrasados, inválidos), o gráfico "Sinal" com uma linha por receptor e o campo magnético na atitude. A gravação baixa `voo_teste.csv`, com as colunas por receptor.

Se o RYLR998 não responder, a base segue só com o SX1276, e vice-versa. O dashboard avisa qual rádio falhou.

## Ordem sugerida dos testes (seção 8 do documento)

1. **T1:** com o `bancada` gravado na base e o RYLR998 ligado ao PC, mande `AT+SEND=0,5,HELLO` e confira o cabeçalho de 5 bytes no farejador. Isso valida os campos marcados ⚠️ no documento.
2. **T3:** comando `b` no `bancada` e `python ferramentas/rcv_dump.py COM5` no RYLR998. Se os 256 valores de byte chegarem idênticos, dá para usar `-DTLM_CODIFICACAO=0` (binário, 69 bytes, ~132 ms no ar) no Código 1.
3. **T4:** comandos `h`, `u`, `d`, `e` e `t` no `bancada`, olhando o que o RYLR998 imprime.
4. **T5:** Código 1 + Código 2 por 10 minutos. Critérios: cada receptor com ≥ 99 % dos pacotes, inválidos = 0, atrasados = 0, cada `seq` uma única vez no CSV.

## Arquivos

```
teste_enlace/
├── platformio.ini
├── lib/Enlace/
│   ├── ConfigEnlace.h      parâmetros de rádio compatíveis com o RYLR998
│   └── QuadroReyax.*       cabeçalho Reyax, Base64, codifica/decodifica a telemetria
├── src/cansat_sim/         Código 1: main, simulacao (voo fictício), radio_tx, pinos
├── src/base_dupla/         Código 2: recepcao (dois rádios), reyax_rx (AT + parser +RCV),
│                           fusao, radio_rx, servidor_web, gravacao, saida, pinos
├── src/bancada/            farejador/gerador de bancada
├── web/dashboard.html      dashboard do Código 2
└── ferramentas/
    ├── gerar_dashboard.py  embute o dashboard no firmware (roda sozinho)
    └── rcv_dump.py         teste T3: lê o RYLR998 em binário e confere byte a byte
```
