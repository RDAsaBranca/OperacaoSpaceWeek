#!/usr/bin/env python3
"""
Baixa o voo gravado na flash W25Q128 do FOGUETE e salva num arquivo CSV.

Conecte o foguete (ESP32-C3) pela USB, ja no solo, e rode:
    pip install pyserial
    python baixar_flash.py COM6                 (Windows)
    python baixar_flash.py /dev/ttyACM0         (Linux)
    python baixar_flash.py /dev/ttyACM0 -o voo1_flash.csv

O script envia o comando "baixar", grava as linhas "LOG,..." e para ao
receber "FIM". Os nomes das colunas vem do proprio firmware.
A coluna "sessao" separa cada vez que o foguete foi ligado.
"""
import argparse
import csv
import datetime
import time

import serial  # pip install pyserial


def abrir_porta(porta: str, baud: int) -> serial.Serial:
    ser = serial.Serial()
    ser.port = porta
    ser.baudrate = baud
    ser.timeout = 1
    ser.dtr = False  # evita reiniciar a placa ao abrir a porta
    ser.rts = False
    ser.open()
    return ser


def main() -> None:
    ap = argparse.ArgumentParser(description="Baixa o registro de voo da flash do foguete")
    ap.add_argument("porta", help="porta USB do foguete, ex.: COM6 ou /dev/ttyACM0")
    ap.add_argument("-b", "--baud", type=int, default=115200)
    ap.add_argument("-o", "--saida", help="arquivo CSV (padrao: flash_AAAAMMDD_HHMMSS.csv)")
    ap.add_argument("--espera", type=float, default=15.0,
                    help="segundos sem resposta ate desistir (padrao 15)")
    args = ap.parse_args()

    nome = args.saida or datetime.datetime.now().strftime("flash_%Y%m%d_%H%M%S.csv")

    with abrir_porta(args.porta, args.baud) as ser:
        time.sleep(0.5)
        ser.reset_input_buffer()
        ser.write(b"baixar\n")
        print("Comando enviado, aguardando dados...")

        cabecalho = None
        linhas = []
        total_firmware = None
        ultimo = time.time()

        while time.time() - ultimo < args.espera:
            linha = ser.readline().decode("utf-8", errors="replace").strip()
            if not linha:
                continue
            ultimo = time.time()
            if linha.startswith("# colunas LOG:"):
                cabecalho = [c.strip() for c in linha.split(":", 1)[1].split(",")]
            elif linha.startswith("LOG,"):
                linhas.append(linha.split(",")[1:])
                if len(linhas) % 2000 == 0:
                    print(f"  {len(linhas)} registros...")
            elif linha.startswith("FIM"):
                partes = linha.split(",")
                total_firmware = int(partes[1]) if len(partes) > 1 else None
                break
            else:
                print(linha)  # mensagens do firmware

    if cabecalho is None:
        print("Nao recebi o cabecalho. O foguete esta no solo e com o firmware atual?")
        return

    ruins = sum(1 for l in linhas if len(l) != len(cabecalho))
    with open(nome, "w", newline="") as arq:
        escritor = csv.writer(arq)
        escritor.writerow(cabecalho)
        escritor.writerows(l for l in linhas if len(l) == len(cabecalho))

    print(f"\n{len(linhas) - ruins} registros salvos em {nome}")
    if total_firmware is not None and total_firmware != len(linhas):
        print(f"ATENCAO: o firmware informou {total_firmware} registros, chegaram {len(linhas)}")
    if ruins:
        print(f"ATENCAO: {ruins} linhas com numero de colunas errado foram descartadas")
    crc_ruim = sum(1 for l in linhas if len(l) == len(cabecalho) and l[-1] == "0")
    if crc_ruim:
        print(f"{crc_ruim} registros com crc_ok=0 (gravacao interrompida ou versao antiga)")


if __name__ == "__main__":
    main()
