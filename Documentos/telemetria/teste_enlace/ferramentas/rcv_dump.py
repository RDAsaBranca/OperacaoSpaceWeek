# rcv_dump.py - le o RYLR998 pela USB-serial e mostra cada +RCV em hex (teste T3)
# uso: python rcv_dump.py COM5        (pip install pyserial)
import sys

ESPERADO = {bytes(range(0, 240)), bytes(range(240, 256))}  # comando "b" da bancada


def tentar(buf):
    """Procura um +RCV completo. Retorna (registro, bytes_consumidos).
    registro = None se faltam bytes; ("RUIM", linha) se mal formado."""
    k = buf.find(b"+RCV=")
    if k < 0:
        return None, max(0, len(buf) - 4)
    i = k + 5
    try:
        campos = []
        for _ in range(2):  # origem, tamanho
            j = buf.find(b",", i)
            if j < 0:
                return None, k  # incompleto
            campos.append(int(buf[i:j]))
            i = j + 1
        origem, tam = campos
        if len(buf) < i + tam + 1:
            return None, k
        dados = bytes(buf[i:i + tam])  # le pelo tamanho: aceita , \r \n 0x00 nos dados
        i += tam
        fim = buf.find(b"\n", i)
        if fim < 0:
            return None, k
        if buf[i:i + 1] != b",":
            return ("RUIM", bytes(buf[k:fim + 1])), fim + 1
        rssi, snr = (int(x) for x in bytes(buf[i + 1:fim]).strip().split(b","))
        return (origem, tam, dados, rssi, snr), fim + 1
    except ValueError:
        return ("RUIM", bytes(buf[k:k + 40])), k + 5


def main():
    import serial
    porta = serial.Serial(sys.argv[1], 115200, timeout=0.1)
    buf = bytearray()
    while True:
        buf += porta.read(512)
        while True:
            reg, n = tentar(buf)
            del buf[:n]
            if reg is None:
                break
            if reg[0] == "RUIM":
                print("LINHA RUIM:", reg[1])
                continue
            origem, tam, dados, rssi, snr = reg
            veredito = "IDENTICO" if dados in ESPERADO else "(nao e o teste b)"
            print(f"+RCV origem={origem} tam={tam} rssi={rssi} snr={snr} {veredito}")
            print("   ", dados.hex(" "))


if __name__ == "__main__":
    main()
