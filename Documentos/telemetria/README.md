# Telemetria LoRa SX1276 para foguete

Um único projeto PlatformIO com dois firmwares e duas ferramentas para o PC:

- **foguete** (ESP32-C3 SuperMini): lê BMP280, BNO055, GPS ATGM336H e bateria a 50 Hz, estima a fase do voo, grava tudo na flash W25Q128 e transmite telemetria pelo SX1276 a 5 Hz.
- **base** (ESP32): recebe por interrupção, valida o pacote (CRC do LoRa + CRC próprio), conta perdas e envia pela USB em CSV, com RSSI, SNR e alertas.
- **ferramentas/registrar_serial.py**: grava no PC o que a base recebe durante o voo.
- **ferramentas/baixar_flash.py**: depois do voo, baixa o registro completo da flash do foguete.

## Componentes da missão

| Subsistema | Função | Componente |
|---|---|---|
| OBDH | Microcontrolador | ESP32-C3 SuperMini |
| OBDH | IMU | BNO055 |
| OBDH | Barômetro | BMP280 (módulo 5 V, 4 pinos) |
| OBDH | GPS | ATGM336H-5N-31 |
| OBDH | Armazenamento | W25Q128 (16 MB) |
| EPS | Bateria | Panasonic NCR18650B |
| EPS | Regulador | 134N3P (HT4928S) ou SX1308 |
| EPS | Carregador | 134N3P (HT4928S) |
| TT&C | Rádio | SX1276 SMD |
| Base | Microcontrolador | ESP32 DevKit + SX1276 |

## Estrutura

```
telemetria-foguete/
├── platformio.ini            ambientes [env:foguete] e [env:base]
├── lib/Telemetria/           CÓDIGO COMPARTILHADO (compilado nos dois)
│   ├── ConfigLoRa.h          frequência, SF, BW, potência, período
│   ├── Protocolo.h           pacote de 58 bytes, estados, bits de status
│   └── Protocolo.cpp         CRC16, validação, conversão para CSV
├── src/foguete/              ESP32-C3 SuperMini
│   ├── main.cpp              agendador: sensores 50 Hz, flash, TX a cada 200 ms
│   ├── pinos.h               mapa de pinos e modo do BNO055
│   ├── log.h                 diagnóstico USB (desligável para o voo)
│   ├── radio_tx.*            transmissão LoRa não bloqueante
│   ├── sensores.*            BMP280, BNO055, ATGM336H, bateria
│   ├── estado_voo.*          altitude filtrada, velocidade, fase do voo
│   ├── flash_w25q.*          driver da W25Q128
│   ├── registro_voo.*        caixa-preta: grava o voo na flash
│   └── comandos.*            comandos pela USB (info, baixar, apagar)
├── src/base/                 ESP32
│   ├── main.cpp              recepção, contagem de perdas, alerta sem sinal
│   ├── pinos.h
│   ├── radio_rx.*            recepção LoRa por interrupção
│   └── saida.*               CSV + resumo legível com orientação e alertas
└── ferramentas/
    ├── registrar_serial.py   grava a telemetria recebida pela base
    └── baixar_flash.py       baixa a flash do foguete após o voo
```

Tudo que precisa ser igual nos dois lados (rádio e formato do pacote) fica em `lib/Telemetria`.

## Ligações do foguete (ESP32-C3 SuperMini)

O SX1276 e a W25Q128 dividem o mesmo barramento SPI, cada um com seu CS. O BMP280 e o BNO055 dividem o I2C.

| Função | Componente / pino | GPIO da C3 |
|---|---|---|
| SPI SCK | SX1276 SCK + W25Q128 CLK | 4 |
| SPI MISO | SX1276 MISO + W25Q128 DO | 5 |
| SPI MOSI | SX1276 MOSI + W25Q128 DI | 6 |
| CS do rádio | SX1276 NSS | 7 |
| Reset do rádio | SX1276 RESET | 10 |
| Interrupção do rádio | SX1276 DIO0 | 3 |
| CS da flash | W25Q128 /CS | 0 |
| I2C SDA | BMP280 SDA + BNO055 SDA | 8 |
| I2C SCL | BMP280 SCL + BNO055 SCL | 9 |
| GPS TX → ESP RX | ATGM336H TX | 20 |
| GPS RX ← ESP TX | ATGM336H RX | 21 |
| Tensão da bateria | divisor 100k/100k a partir do B+ | 1 |

Detalhes que costumam dar problema:

- **W25Q128:** os pinos /WP e /HOLD do chip precisam ir em 3,3 V (vários módulos já fazem isso; no chip solto, não). Um resistor de 10 kΩ do /CS para 3,3 V mantém a flash desligada do barramento durante o boot.
- **BMP280 de 5 V (4 pinos):** alimente o VIN com **3,3 V**. Esse módulo tem regulador próprio e funciona assim, e isso garante que os resistores de pull-up do I2C fiquem em 3,3 V. Se ele for ligado em 5 V, meça SDA e SCL em repouso: acima de 3,6 V pode danificar a ESP32-C3.
- **BNO055:** pelo mesmo motivo, alimente com 3,3 V. O firmware procura o sensor em 0x28 e 0x29 (as placas GY-BNO055 costumam vir em 0x29).
- **ATGM336H:** alimente com 3,3 V. Ele sai de fábrica em NMEA a 9600 baud, 1 Hz, que é o que o firmware espera.
- **SX1276 SMD:** 3,3 V apenas, com um capacitor de 100 µF perto do módulo. A antena vai soldada no pad ou no conector U.FL.

## Alimentação (EPS)

```
NCR18650B ─► 134N3P / HT4928S (carga + boost) ─► 5 V ─► pino 5V da SuperMini
     │                                                    └─► regulador 3,3 V da placa
     └─► divisor 100k/100k ─► GPIO1                          ─► SX1276, BNO055, BMP280,
                                                                ATGM336H, W25Q128
```

O consumo de pico em 3,3 V fica em torno de 220 mA (rádio transmitindo a 17 dBm + GPS + sensores + gravação na flash). Confira se o regulador da sua SuperMini aguenta isso com folga. Com a NCR18650B (3.400 mAh), a autonomia esperada é da ordem de 10 horas ou mais.

- **SX1308:** a saída desses módulos é ajustável por trimpot. Ajuste para 5,0 V com um multímetro **antes** de ligar a placa.
- **Módulos de power bank (134N3P):** alguns desligam sozinhos quando a corrente de carga é baixa. Teste o foguete ligado por pelo menos uma hora no consumo real antes da missão.
- Evite ligar o USB do computador e o boost no pino 5V ao mesmo tempo, a não ser que confirme que a sua placa tem diodo de proteção.

## BNO055: escolha do modo

O BNO055 tem uma limitação importante para foguetes: **nos modos de fusão (que calculam a orientação), o próprio chip trava o acelerômetro em ±4 g**. Durante a queima do motor a aceleração costuma passar disso. O modo é escolhido no `platformio.ini`:

| `IMU_MODO_ALTO_G` | Orientação (quaternion) | Acelerômetro |
|---|---|---|
| 0 (padrão) | sim, fusão NDOF | satura em ±4 g |
| 1 | não | até ±16 g |

A detecção de lançamento funciona nos dois modos (ela usa 2,5 g). A calibração do BNO055 (0 a 3 para sistema, giroscópio, acelerômetro e magnetômetro) vai na telemetria. No modo NDOF, o magnetômetro só calibra com movimento, e motor ou partes metálicas por perto atrapalham a bússola.

A orientação mostrada na base (rolagem, arfagem e guinada) depende de como o sensor está montado. Se o eixo longo do foguete for o eixo X ou Y do BNO055, a arfagem fica perto de ±90° com o foguete na vertical, onde os ângulos de Euler perdem precisão. Para análise, use o quaternion do CSV.

## Registro na flash (caixa-preta)

O LoRa sempre perde alguns pacotes, então o foguete grava tudo na W25Q128:

- Na rampa e depois do pouso: 1 registro por segundo.
- Em voo (subida e descida): todas as amostras, a 50 Hz.
- Pré-disparo: os 0,5 s anteriores à detecção do lançamento também são gravados, para não perder o início da queima.
- Cada registro tem 64 bytes: o pacote completo, a pressão bruta e o número da sessão. A sessão aumenta cada vez que o foguete liga, então um reinício no ar não apaga nada.
- A gravação só acrescenta, nunca sobrescreve. Cabem cerca de 87 minutos a 50 Hz.

Comandos pela USB do foguete (monitor serial a 115200, só funcionam no solo):

```
info              estado dos sensores e da flash
baixar            exporta os registros em CSV
apagar confirmo   apaga a flash inteira (40 s a 200 s)
ajuda             lista os comandos
```

Antes da missão, apague a flash com `apagar confirmo`. Depois do voo, conecte o foguete e rode:

```
python ferramentas/baixar_flash.py COM6          (Windows)
python ferramentas/baixar_flash.py /dev/ttyACM0  (Linux)
```

## Compilar e gravar

Com o VS Code + extensão PlatformIO, abra a pasta do projeto e use:

```
pio run -e foguete -t upload     grava o foguete
pio run -e base -t upload        grava a base
pio device monitor -e base       vê a telemetria chegando
```

Na primeira gravação da C3 SuperMini, se ela não for reconhecida: segure o botão BOOT, conecte o USB (ou aperte RESET) e solte o BOOT.

Para gravar a telemetria recebida pela base durante o voo:

```
pip install pyserial
python ferramentas/registrar_serial.py COM5          (Windows)
python ferramentas/registrar_serial.py /dev/ttyUSB0  (Linux)
```

## Rádio: alcance x taxa

Os parâmetros ficam em `lib/Telemetria/ConfigLoRa.h`. Cada passo de SF dá cerca de 2,5 dB a mais de sensibilidade (mais alcance), mas quase dobra o tempo no ar. Valores para o pacote de 58 bytes com BW 125 kHz e CR 4/5:

| SF | Tempo no ar | `TLM_PERIODO_MS` sugerido | Taxa |
|---|---|---|---|
| 7 | 113 ms | 200 | 5 Hz |
| 8 | 195 ms | 300 | ~3 Hz |
| 9 | 370 ms | 500 | 2 Hz |
| 10 | 657 ms | 1000 | 1 Hz |

O foguete imprime o tempo no ar real ao ligar e avisa se o período estiver curto demais.

Verifique a faixa do seu módulo SX1276 SMD: o firmware vem em 915 MHz (faixa usual no Brasil). Se o módulo for de 433 MHz, mude `LORA_FREQ_MHZ` para 433.0.

## Formato da saída da base

Linhas `TLM,...` são dados (uma por pacote). Linhas `#` são para leitura humana: um resumo por segundo, orientação e calibração do BNO055, e alertas (sensor com falha, flash parada, bateria abaixo de 3,5 V, perda de sinal com a última posição GPS).

```
# [SUBIDA] alt 523.4 m (max 523.4) v +85.2 m/s | GPS fix 8 sats -23.5505123,-46.6333456 | bat 3.92 V | RSSI -87 dBm SNR 9.2 dB | perda 1.0%
#    orientacao rol +2.1 arf -1.4 gui 87.5 graus | calibracao BNO055 S3 G3 A2 M1 (0..3)
```

## Antes de voar

1. Com `info` no monitor serial do foguete, confirme que BMP280, BNO055, GPS e flash aparecem como ok.
2. Apague a flash (`apagar confirmo`) e confira que a base não mostra o alerta "NAO esta gravando na flash".
3. Teste de bancada com os dois lados ligados: a perda deve ficar perto de 0%.
4. Teste de alcance no solo com o foguete montado e a antena na posição de voo. O SX1276 em SF7/125 kHz recebe até cerca de −123 dBm; procure ter uns 20 dB de margem na distância esperada.
5. Levante o foguete 1 a 2 m e veja a altitude responder na base.
6. Deixe o foguete ligado pelo tempo de espera previsto na rampa, para testar a bateria e o módulo de alimentação.
7. Ajuste os limiares de `estado_voo.cpp` para o seu motor.
8. Grave o foguete com `-DDEBUG_SERIAL=0` no `platformio.ini` para o voo.

## Cuidados

- **Nunca transmita sem antena conectada**: isso pode queimar o amplificador do SX1276.
- Fuselagem de fibra de carbono ou alumínio bloqueia o sinal de rádio e de GPS. Fibra de vidro, papelão e PLA deixam passar.
- Antena de 1/4 de onda em 915 MHz: cerca de 8,2 cm. Na base, uma antena direcional (Yagi) aumenta muito o alcance.
- Se a ESP32-C3 reiniciar no ar, ela recalibra o "zero" de altitude onde estiver. Os dados anteriores ficam salvos na flash (sessão anterior), mas capriche na alimentação e na fixação dos conectores.
- A detecção de fase do voo é apenas **informativa**. Não use este código para acionar paraquedas: use um altímetro de recuperação dedicado e testado.
