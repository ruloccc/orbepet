"""Cambia el fondo ilustrado de una zona y estación en firmware/main/fondos_data.inc

Uso:   python herramientas/hacer_fondos.py [carpeta]        (por defecto: fondos_zonas/)
Archivos (cuadrados; se reducen a 466 x 466):   <zona>_<estacion>.png
   zona:     prado bosque lago colina cueva isla claro mirador  (claro y mirador: herramientas/dibujar_zonas_v98.py)
   estacion: primavera verano otono invierno
   Ej.: prado_verano.png, prado_primavera.png ...  (la hora y el clima se generan solos desde el de día)
   casita_primavera.png (también verano), casita_otono.png, casita_invierno.png: el ícono de la casita-tocón del prado (fondo transparente; se reduce a 109 x 113)
Lo que falte se queda como estaba.   Necesita: pip install pillow numpy
"""
import os, re, sys
import numpy as np
from PIL import Image
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from lzss import lz_encode, lz_decode
from hacer_pantallas import paleta_e_indices

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
INC = os.path.join(RAIZ, "firmware", "main", "fondos_data.inc")
ZONAS = ["prado", "bosque", "lago", "colina", "cueva", "isla", "claro", "mirador"]
ESTACIONES = ["primavera", "verano", "otono", "invierno"]


def empaquetar(ruta, colores=255, pixel=False):
    """pixel=True: el dibujo es de pixeles grandes (de a 4 en 1024): se agranda sin suavizar, así comprime mucho mejor"""
    im = Image.open(ruta).convert("RGB")
    w, h = im.size
    if w != h:
        m = min(w, h); im = im.crop(((w - m) // 2, (h - m) // 2, (w - m) // 2 + m, (h - m) // 2 + m))
    if im.size != (466, 466):
        if pixel:
            im = im.resize((im.size[0] // 4, im.size[1] // 4), Image.NEAREST).resize((466, 466), Image.NEAREST)
        else:
            im = im.resize((466, 466), Image.LANCZOS)
    rgb = np.asarray(im).copy()
    yy, xx = np.mgrid[0:466, 0:466]
    fuera = np.hypot(xx - 232.5, yy - 232.5) > 234
    pal, idx = paleta_e_indices(rgb, None, colores, 0.0 if pixel else 4.0)
    idx[fuera] = 0
    lz = lz_encode(idx.tobytes())
    assert lz_decode(lz, 466 * 466) == idx.tobytes()
    return bytes([len(pal)]) + pal.astype(np.uint8).tobytes() + lz


def casita(ruta, txt, nom="FD_CASITA", largo="FD_CASITA_LEN"):
    """El ícono de la casita-tocón del prado (109 x 113, fondo transparente) -> FD_CASITA (primavera y verano), FD_CASITA_O (otoño), FD_CASITA_I (invierno)"""
    W, H = 109, 113
    im = Image.open(ruta).convert("RGBA").resize((W, H), Image.LANCZOS)
    a = np.asarray(im).copy()
    opaco = a[..., 3] >= 128
    rgb = a[..., :3].copy()
    rgb[~opaco] = rgb[opaco][0]
    pal, idx = paleta_e_indices(rgb, None, 254, 3.0)
    idx = np.where(opaco, idx + 1, 0).astype(np.uint8)
    lz = lz_encode(idx.tobytes())
    assert lz_decode(lz, W * H) == idx.tobytes()
    blob = bytes([len(pal)]) + pal.astype(np.uint8).tobytes() + lz
    arr = "static const uint8_t %s[%d] = {%s};" % (nom, len(blob), ",".join(str(b) for b in blob))
    pat = r"static const uint8_t %s\[\d+\] = \{[^}]*\};" % nom
    if re.search(pat, txt):
        txt = re.sub(pat, lambda m: arr, txt, count=1)
    else:
        txt = txt.replace("#define FD_CASITA_W", arr + "\n#define FD_CASITA_W", 1)
    dfn = "#define %s %d" % (largo, len(blob))
    if re.search(r"#define %s \d+" % largo, txt):
        txt = re.sub(r"#define %s \d+" % largo, dfn, txt)
    else:
        txt = txt.replace("#define FD_CASITA_H", dfn + "\n#define FD_CASITA_H", 1)
    print("  %s -> %s (%d KB)" % (os.path.basename(ruta), nom, len(blob) // 1024))
    return txt


def main():
    carpeta = sys.argv[1] if len(sys.argv) > 1 else os.path.join(RAIZ, "fondos_zonas")
    txt = open(INC, encoding="utf-8").read()
    n = 0
    for arch, nom, largo in (("casita.png", "FD_CASITA", "FD_CASITA_LEN"), ("casita_primavera.png", "FD_CASITA", "FD_CASITA_LEN"),
                             ("casita_otono.png", "FD_CASITA_O", "FD_CASITA_LEN_O"), ("casita_invierno.png", "FD_CASITA_I", "FD_CASITA_LEN_I")):
        r = os.path.join(carpeta, arch)
        if os.path.exists(r):
            txt = casita(r, txt, nom, largo)
            n += 1
    for z, zona in enumerate(ZONAS):
        for e, est in enumerate(ESTACIONES):
            r = os.path.join(carpeta, "%s_%s.png" % (zona, est))
            if not os.path.exists(r):
                continue
            blob = empaquetar(r, 128, zona in ("claro", "mirador"))  # (v98) dibujos de pixel grande: sin suavizar
            nombre = "FD_%d_%d" % (z, e)
            arr = "static const uint8_t %s[%d] = {%s};" % (nombre, len(blob), ",".join(str(b) for b in blob))
            txt, k = re.subn(r"static const uint8_t %s\[\d+\] = \{[^}]*\};" % nombre, lambda m: arr, txt, count=1)
            if not k:
                raise SystemExit("no encontré %s en fondos_data.inc" % nombre)
            txt = re.sub(r"\{%s, \d+\}" % nombre, "{%s, %d}" % (nombre, len(blob)), txt)
            print("  %s -> %s (%d KB)" % (os.path.basename(r), nombre, len(blob) // 1024))
            n += 1
    open(INC, "w", encoding="utf-8").write(txt)
    print("Listo: %d fondos cambiados." % n)


if __name__ == "__main__":
    main()
