"""(1.0.17) Empaqueta los fondos de los juegos de a dos y de la isla (los PNG los hace herramientas/hacer_fondos_juegos.py) para la MEMORIA DE RECURSOS del orbe (no van en el programa: no
caben). Mismo formato que las pantallas (paleta + LZSS, ver hacer_pantallas.py), con una cabecera:

    'P' 'J' 1 alfa  w(u16) h(u16) x(u16) y(u16)  [cantidad de colores] [paleta RGB] [LZSS]

Escribe:
  firmware/recursos/pj0.bin .. pj3.bin   -> se copian al orbe por USB (probador) o los baja el modo Wi-Fi
  firmware/main/fondos_dos_sim.inc       -> los mismos datos para el simulador del PC (el firmware NO lo usa)

Uso:   python herramientas/hacer_fondos_juegos.py (dibuja los PNG) y luego python herramientas/hacer_fondos_dos.py
"""
import os
import struct
import sys
import zlib

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from hacer_pantallas import empaquetar  # noqa: E402

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
NOMBRES = ["13_dos_pingpong.png", "14_dos_cuerda.png", "15_dos_memorice.png", "16_dos_escondidas.png",
           "17_isla_faro.png", "18_isla_buceo.png"]  # (1.0.18) pj4 y pj5: los juegos de la isla


def main():
    sal = os.path.join(RAIZ, "firmware", "recursos")
    os.makedirs(sal, exist_ok=True)
    sim = ["/* GENERADO por herramientas/hacer_fondos_dos.py: los fondos de los juegos de a dos, SOLO para el simulador del PC.\n"
           " * En el orbe están en la memoria de recursos (/res/pjN.bin). No editar a mano. */\n"]
    filas = []
    for i, nom in enumerate(NOMBRES):
        blob, w, h, x, y, alfa = empaquetar(os.path.join(RAIZ, "fondos_pantallas", nom))
        datos = b"PJ" + bytes([1, alfa]) + struct.pack("<HHHH", w, h, x, y) + blob
        open(os.path.join(sal, "pj%d.bin" % i), "wb").write(datos)
        sim.append("static const uint8_t PJ_SIM_%d[%d] = {%s};\n" % (i, len(datos), ",".join(str(b) for b in datos)))
        filas.append("{PJ_SIM_%d, %d}" % (i, len(datos)))
        print("  pj%d.bin  %s  %d KB  crc %08x" % (i, nom, len(datos) // 1024, zlib.crc32(datos) & 0xFFFFFFFF))
    sim.append("static const struct { const uint8_t *d; uint32_t n; } PJ_SIM[%d] = {%s};\n" % (len(NOMBRES), ", ".join(filas)))
    open(os.path.join(RAIZ, "firmware", "main", "fondos_dos_sim.inc"), "w", encoding="utf-8").write("".join(sim))
    print("listo:", sal)


if __name__ == "__main__":
    main()
