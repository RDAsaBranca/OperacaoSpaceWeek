#!/usr/bin/env python3
"""
Grava a telemetria que a ESTACAO BASE envia pela USB num arquivo CSV
(dados recebidos pelo radio durante o voo).

Uso:
    pip install pyserial
    python registrar_serial.py COM5                 (Windows)
    python registrar_serial.py /dev/ttyUSB0         (Linux)
    python registrar_serial.py /dev/ttyUSB0 -o voo1.csv

Linhas "TLM,..." vao para o CSV; linhas "#" aparecem no terminal.
Cada linha e gravada na hora (flush), entao nada se perde se o PC travar.
"""
import argparse
import csv
import datetime

import serial  # pip install pyserial

# Mantenha igual a TLM_COLUNAS (lib/Telemetria/Protocolo.cpp) + 3 colunas da base
COLUNAS = [
    "seq", "tempo_ms", "estado", "status", "alt_m", "alt_max_m", "vel_ms", "temp_c",
    "ax_g", "ay_g", "az_g", "gx_dps", "gy_dps", "gz_dps", "mx_ut", "my_ut", "mz_ut",
    "qw", "qx", "qy", "qz",
    "cal_sis", "cal_giro", "cal_acel", "cal_mag", "lat", "lon", "alt_gps_m", "sats",
    "bat_v", "rssi_dbm", "snr_db", "perdidos",
]


def abrir_porta(porta: str, baud: int) -> serial.Serial:
    ser = serial.Serial()
    ser.port = porta
    ser.baudrate = baud
    ser.timeout = 1
    # Evita que abrir a porta reinicie a ESP32 (DTR/RTS ligados ao EN/BOOT)
    ser.dtr = False
    ser.rts = False
    ser.open()
    return ser


def main() -> None:
    ap = argparse.ArgumentParser(description="Grava telemetria da estacao base em CSV")
    ap.add_argument("porta", help="porta serial da base, ex.: COM5 ou /dev/ttyUSB0")
    ap.add_argument("-b", "--baud", type=int, default=115200)
    ap.add_argument("-o", "--saida", help="arquivo CSV (padrao: voo_AAAAMMDD_HHMMSS.csv)")
    args = ap.parse_args()

    nome = args.saida or datetime.datetime.now().strftime("voo_%Y%m%d_%H%M%S.csv")
    pacotes = 0

    with abrir_porta(args.porta, args.baud) as ser, open(nome, "w", newline="") as arq:
        escritor = csv.writer(arq)
        escritor.writerow(["hora_pc"] + COLUNAS)
        print(f"Gravando em {nome}  (Ctrl+C para parar)")
        try:
            while True:
                linha = ser.readline().decode("utf-8", errors="replace").strip()
                if not linha:
                    continue
                if linha.startswith("TLM,"):
                    campos = linha.split(",")[1:]
                    if len(campos) != len(COLUNAS):
                        print(f"! linha com {len(campos)} campos (esperado {len(COLUNAS)}): {linha}")
                        continue
                    hora = datetime.datetime.now().isoformat(timespec="milliseconds")
                    escritor.writerow([hora] + campos)
                    arq.flush()
                    pacotes += 1
                else:
                    print(linha)
        except KeyboardInterrupt:
            pass

    print(f"\n{pacotes} pacotes gravados em {nome}")


if __name__ == "__main__":
    main()
