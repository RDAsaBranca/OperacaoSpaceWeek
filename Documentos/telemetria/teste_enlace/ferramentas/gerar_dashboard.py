"""
Converte web/dashboard.html em src/base_dupla/dashboard_html.h (comprimido com
gzip), para a ESP32 servir a pagina direto da memoria de programa.

O PlatformIO roda este script sozinho antes de compilar o ambiente base_dupla
(extra_scripts no platformio.ini). Tambem da para rodar a mao:
    python ferramentas/gerar_dashboard.py
"""
import gzip
import os

try:
    Import("env")  # noqa: F821  (definido pelo PlatformIO/SCons)
    PROJETO = env.subst("$PROJECT_DIR")  # noqa: F821
except NameError:
    PROJETO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

ORIGEM = os.path.join(PROJETO, "web", "dashboard.html")
DESTINO = os.path.join(PROJETO, "src", "base_dupla", "dashboard_html.h")


def gerar() -> None:
    if os.path.exists(DESTINO) and os.path.getmtime(DESTINO) >= os.path.getmtime(ORIGEM):
        return  # ja esta atualizado
    with open(ORIGEM, "rb") as f:
        html = f.read()
    comprimido = gzip.compress(html, compresslevel=9, mtime=0)
    linhas = []
    for i in range(0, len(comprimido), 20):
        linhas.append("  " + ", ".join(f"0x{b:02X}" for b in comprimido[i:i + 20]) + ",")
    with open(DESTINO, "w", newline="\n") as f:
        f.write("// GERADO AUTOMATICAMENTE a partir de web/dashboard.html\n")
        f.write("// Nao edite aqui: edite o HTML e compile de novo.\n")
        f.write("#pragma once\n#include <Arduino.h>\n\n")
        f.write(f"// {len(html)} bytes de HTML -> {len(comprimido)} bytes com gzip\n")
        f.write(f"static const size_t DASHBOARD_HTML_GZ_LEN = {len(comprimido)};\n")
        f.write("static const uint8_t DASHBOARD_HTML_GZ[] PROGMEM = {\n")
        f.write("\n".join(linhas))
        f.write("\n};\n")
    print(f"dashboard: {len(html)} bytes -> {len(comprimido)} bytes (gzip) em {DESTINO}")


gerar()
