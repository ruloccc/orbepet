"""Convierte los dibujos de fondo de pantallas (PNG) al formato del firmware (paleta + LZSS) -> firmware/main/pantallas_data.inc

Uso:   python herramientas/hacer_pantallas.py [carpeta_con_los_png]      (por defecto: fondos_pantallas/)
Archivos (466 x 466):   0_huevos.png   1_fruta.png   2_picnic.png   3_picnic_fondo.png (fondo completo del picnic, sin transparencia)
  - 0 y 1: imagen completa sin transparencia.
  - 2: capa con transparencia (solo tapete y cesta): se recorta sola al rectángulo útil.
Si falta un archivo, esa pantalla sigue pintándose con formas como antes.
Necesita: pip install pillow numpy
"""
import os, sys
import numpy as np
from PIL import Image
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from lzss import lz_encode, lz_decode

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SALIDA = os.path.join(RAIZ, "firmware", "main", "pantallas_data.inc")
NOMBRES = ["0_huevos.png", "1_fruta.png", "2_picnic.png", "3_picnic_fondo.png", "4_casa.png", "5_casita_tilo.png", "6_mirador.png", "7_claro.png",
           "8_juego_nubes.png", "9_juego_viento.png", "10_juego_piedra.png", "11_juego_horno.png", "12_juego_cama.png"]  # (v97) los juegos de los lugares secretos
BAY = np.array([[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]]) / 16.0


def paleta_e_indices(rgb, mascara, n, spread):
    h, w = rgb.shape[:2]
    im = Image.fromarray(rgb)
    q = im.quantize(colors=n, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE)
    pal = np.array(q.getpalette()[: n * 3], dtype=np.uint8).reshape(-1, 3)
    ruido = np.tile(BAY, (h // 4 + 1, w // 4 + 1))[:h, :w]
    pert = np.clip(rgb.astype(np.float32) + ((ruido - 0.5) * spread)[..., None], 0, 255).astype(np.uint8)
    idx = np.array(Image.fromarray(pert).quantize(palette=q, dither=Image.Dither.NONE), dtype=np.uint8)
    return pal[: int(idx.max()) + 1], idx


def empaquetar(ruta):
    im = Image.open(ruta).convert("RGBA")
    a = np.asarray(im)
    alfa = bool((a[..., 3] < 255).any())
    x0 = y0 = 0
    if alfa:
        ys, xs = np.where(a[..., 3] >= 128)
        if len(xs) == 0:
            return None
        x0, x1, y0, y1 = int(xs.min()), int(xs.max()) + 1, int(ys.min()), int(ys.max()) + 1
        a = a[y0:y1, x0:x1]
    rgb = a[..., :3].copy()
    h, w = rgb.shape[:2]
    if alfa:
        opaco = a[..., 3] >= 128
        rgb[~opaco] = rgb[opaco][0] if opaco.any() else 0
        pal, idx = paleta_e_indices(rgb, opaco, 255, 3.0)
        idx = np.where(opaco, idx + 1, 0).astype(np.uint8)
        pal = np.vstack([[0, 0, 0], pal]).astype(np.uint8)
    else:
        if (w, h) != (466, 466):
            raise SystemExit(f"{ruta}: debe medir 466 x 466 (mide {w} x {h})")
        yy, xx = np.mgrid[0:h, 0:w]
        fuera = np.hypot(xx - 232.5, yy - 232.5) > 234  # las esquinas no se ven: se rellenan para que comprima mejor
        pal, idx = paleta_e_indices(rgb, None, 255, 4.0)
        idx[fuera] = 0
    lz = lz_encode(idx.tobytes())
    assert lz_decode(lz, w * h) == idx.tobytes()
    blob = bytes([len(pal)]) + pal.astype(np.uint8).tobytes() + lz
    return blob, w, h, x0, y0, int(alfa)


def main():
    carpeta = sys.argv[1] if len(sys.argv) > 1 else os.path.join(RAIZ, "fondos_pantallas")
    txt = ["/* GENERADO por herramientas/hacer_pantallas.py: fondos de pantallas (se cargan solo mientras se usan). No editar a mano. */\n"]
    filas, total = [], 0
    for i, nom in enumerate(NOMBRES):
        r = os.path.join(carpeta, nom)
        res = empaquetar(r) if os.path.exists(r) else None
        if res is None:
            print("  (sin", nom + ": se pinta con formas)")
            filas.append("{0, 0, 0, 0, 0, 0, 0}")
            continue
        blob, w, h, x, y, alfa = res
        txt.append("static const uint8_t PA_%d[%d] = {%s};\n" % (i, len(blob), ",".join(str(b) for b in blob)))
        filas.append("{PA_%d, %d, %d, %d, %d, %d, %d}" % (i, len(blob), w, h, x, y, alfa))
        total += len(blob)
        print("  %s: %d x %d%s -> %d KB" % (nom, w, h, " (con transparencia, x=%d y=%d)" % (x, y) if alfa else "", len(blob) // 1024))
    txt.append("#define PA_N %d\n" % len(NOMBRES))
    txt.append("static const struct { const uint8_t *d; uint32_t n; int w, h, x, y, alfa; } PA_TABLA[PA_N] = {\n  %s\n};\n" % ",\n  ".join(filas))
    open(SALIDA, "w", encoding="utf-8").write("".join(txt))
    print("Escrito", SALIDA, "(%d KB en la flash)" % (total // 1024))


if __name__ == "__main__":
    main()
