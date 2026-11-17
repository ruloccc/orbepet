"""Cambia el fondo del viaje en bote (lago -> isla) en firmware/main/fondos_cruce.inc

Uso:   python herramientas/hacer_cruce.py [carpeta]        (por defecto: fondos_cruce/)
Archivos (cuadrados; se reducen a 466 x 466):  cruce_primavera / cruce_verano / cruce_otono / cruce_invierno  (.png o .jpg)
Lo de afuera del círculo se rellena con el borde (así el "cuadriculado" de transparencia no gasta colores).
"""
import os, re, sys
import numpy as np
from PIL import Image
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from lzss import lz_encode, lz_decode
from hacer_pantallas import paleta_e_indices

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
INC = os.path.join(RAIZ, "firmware", "main", "fondos_cruce.inc")
ESTACIONES = ["primavera", "verano", "otono", "invierno"]


def empaquetar(ruta):
    im = Image.open(ruta).convert("RGB")
    w, h = im.size
    if w != h:
        m = min(w, h); im = im.crop(((w - m) // 2, (h - m) // 2, (w - m) // 2 + m, (h - m) // 2 + m))
    im = im.resize((466, 466), Image.LANCZOS)
    rgb = np.asarray(im).copy()
    yy, xx = np.mgrid[0:466, 0:466]
    d = np.hypot(xx - 232.5, yy - 232.5)
    fuera = d > 230
    k = 228.0 / np.maximum(d, 1)
    sx = np.clip((232.5 + (xx - 232.5) * k).astype(int), 0, 465)
    sy = np.clip((232.5 + (yy - 232.5) * k).astype(int), 0, 465)
    rgb[fuera] = rgb[sy[fuera], sx[fuera]]
    pal, idx = paleta_e_indices(rgb, None, 255, 4.0)
    lz = lz_encode(idx.tobytes())
    assert lz_decode(lz, 466 * 466) == idx.tobytes()
    return bytes([len(pal)]) + pal.astype(np.uint8).tobytes() + lz


def main():
    carpeta = sys.argv[1] if len(sys.argv) > 1 else os.path.join(RAIZ, "fondos_cruce")
    txt = open(INC, encoding="utf-8").read()
    n = 0
    for e, est in enumerate(ESTACIONES):
        for ext in (".png", ".jpg", ".jpeg"):
            r = os.path.join(carpeta, "cruce_%s%s" % (est, ext))
            if os.path.exists(r):
                break
        else:
            continue
        blob = empaquetar(r)
        arr = "static const uint8_t FC_%d[%d] = {%s};" % (e, len(blob), ",".join(str(b) for b in blob))
        txt, k = re.subn(r"static const uint8_t FC_%d\[\d+\] = \{[^}]*\};" % e, lambda m: arr, txt, count=1)
        if not k:
            raise SystemExit("no encontré FC_%d" % e)
        print("  %s -> FC_%d (%d KB)" % (os.path.basename(r), e, len(blob) // 1024))
        n += 1
    open(INC, "w", encoding="utf-8").write(txt)
    print("Listo: %d fondos cambiados." % n)


if __name__ == "__main__":
    main()
