"""(v106) Los ocho huevos de siempre, con el MISMO dibujo, pero al doble de detalle (92 x 90) y con volumen.

El diseño no cambia: la cáscara crema, el brote de hojas, el rayo, el solcito, el agua con burbujas, el gorro de
bellota, la luna con estrellas, la mariposa y la niebla con los ojitos son los mismos, en sus colores de siempre.
Lo que cambia es la calidad: antes eran 46 x 45 y en la pantalla de elegir huevo se veían a bloques y planos.

Qué se hace, en orden:
  1. se parte del dibujo original (herramientas/iconos_nuevos/huevos_base.png, 8 huevos x 4 grietas);
  2. se agranda al doble con EPX/Scale2x, que redondea las diagonales en vez de dejar escalones;
  3. se le pone luz de verdad: rampa suave arriba a la izquierda con tramado, brillo, luz rebotada abajo a la
     derecha y sombra en la base, respetando el color de cada parte (el dibujo no se repinta: se ilumina);
  4. se repasa el contorno con su propio color, un tono más oscuro.

Uso:   python herramientas/iconos_nuevos/huevos92.py          -> firmware/main/huevos_data.inc
       python herramientas/iconos_nuevos/huevos92.py muestra  -> además /tmp/huevos92.png para mirarlos
"""
import os, sys

import numpy as np
from PIL import Image

AQUI = os.path.dirname(os.path.abspath(__file__))
RAIZ = os.path.dirname(os.path.dirname(AQUI))
sys.path.insert(0, os.path.join(RAIZ, "herramientas"))
from lzss import lz_encode  # noqa: E402

BASE = os.path.join(AQUI, "huevos_base.png")
W0, H0 = 46, 45
W, H = W0 * 2, H0 * 2
BAYER = np.array([[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]]) / 16.0


def epx(a):
    """EPX/Scale2x: cada pixel pasa a 2 x 2 y las esquinas se redondean cuando los vecinos coinciden,
    así las diagonales del dibujo original quedan suaves en vez de escalonadas."""
    h, w = a.shape[:2]
    out = np.repeat(np.repeat(a, 2, 0), 2, 1)
    ig = np.array_equal
    for y in range(h):
        for x in range(w):
            P = a[y, x]
            A = a[y - 1, x] if y > 0 else P
            B = a[y, x + 1] if x < w - 1 else P
            C = a[y, x - 1] if x > 0 else P
            D = a[y + 1, x] if y < h - 1 else P
            if ig(C, A) and not ig(C, D) and not ig(A, B):
                out[y * 2, x * 2] = A
            if ig(A, B) and not ig(A, C) and not ig(B, D):
                out[y * 2, x * 2 + 1] = B
            if ig(D, C) and not ig(D, B) and not ig(C, A):
                out[y * 2 + 1, x * 2] = C
            if ig(B, D) and not ig(B, A) and not ig(D, C):
                out[y * 2 + 1, x * 2 + 1] = D
    return out


def cuerpo(mask):
    """el huevo propiamente tal (la mancha grande del centro), sin los adornos sueltos de alrededor"""
    h, w = mask.shape
    vis = np.zeros_like(mask)
    ys, xs = np.where(mask)
    semilla = (h // 2, w // 2) if mask[h // 2, w // 2] else (int(ys.mean()), int(xs.mean()))
    pila = [semilla]
    while pila:
        y, x = pila.pop()
        if y < 0 or x < 0 or y >= h or x >= w or vis[y, x] or not mask[y, x]:
            continue
        vis[y, x] = True
        pila += [(y + 1, x), (y - 1, x), (y, x + 1), (y, x - 1)]
    return vis


def iluminar(a):
    """le da volumen al huevo sin cambiarle el dibujo: cada pixel conserva su color y se aclara u oscurece
    según dónde está (la luz viene de arriba a la izquierda), con tramado para que no se vean bandas"""
    rgb = a[..., :3].astype(np.float32)
    cu = cuerpo(a[..., 3] > 128)
    ys, xs = np.where(cu)
    y0, y1, x0, x1 = ys.min(), ys.max(), xs.min(), xs.max()
    cx, cy = (x0 + x1) / 2.0, (y0 + y1) / 2.0
    rx, ry = (x1 - x0) / 2.0 + 0.5, (y1 - y0) / 2.0 + 0.5
    yy, xx = np.mgrid[0:H, 0:W]
    dx, dy = (xx - cx) / rx, (yy - cy) / ry
    d = np.clip(dx * dx + dy * dy, 0, 1.6)
    luz = -(dx * 0.60 + dy * 0.80)
    f = 1.0 + luz * 0.15                                                   # la rampa principal, suave
    f += np.clip(dy - 0.45, 0, 1) * np.clip(dx - 0.10, 0, 1) * 0.40        # luz rebotada del suelo
    f -= np.clip(d - 0.78, 0, 1) * 0.60                                    # el borde, más oscuro
    f -= np.clip(dy - 0.74, 0, 1) * 0.30                                   # sombra en la base
    f += (BAYER[yy % 4, xx % 4] - 0.5) * 0.05                              # tramado
    out = np.clip(rgb * np.clip(f, 0.55, 1.30)[..., None], 0, 255)
    gx, gy = cx - rx * 0.36, cy - ry * 0.40                                # el brillo: una gota clara
    g = np.clip(1.0 - (((xx - gx) / (rx * 0.30)) ** 2 + ((yy - gy) / (ry * 0.22)) ** 2), 0, 1) ** 0.8
    g = g * (0.55 + 0.45 * (BAYER[yy % 4, xx % 4] > 0.3))
    out = np.clip(out + (255.0 - out) * (g * 0.50)[..., None], 0, 255)
    res = a.copy()
    res[..., :3] = np.where(cu[..., None], out.astype(np.uint8), a[..., :3])
    return res


def contornear(a):
    """repasa el borde del huevo con su propio color, un tono más oscuro (nunca negro)"""
    cu = cuerpo(a[..., 3] > 128)
    borde = np.zeros_like(cu)
    borde[1:, :] |= ~cu[:-1, :]; borde[:-1, :] |= ~cu[1:, :]
    borde[:, 1:] |= ~cu[:, :-1]; borde[:, :-1] |= ~cu[:, 1:]
    borde &= cu
    res = a.copy()
    ys, xs = np.where(borde)
    for y, x in zip(ys, xs):
        res[y, x, :3] = (res[y, x, :3].astype(np.float32) * 0.66).astype(np.uint8)
    return res


def huevos():
    hoja = np.asarray(Image.open(BASE).convert("RGBA"))
    return {(e, g): contornear(iluminar(epx(hoja[g * H0:(g + 1) * H0, e * W0:(e + 1) * W0].copy())))
            for e in range(8) for g in range(4)}


def main():
    imgs = huevos()
    colores = {}
    for px in imgs.values():
        for (r, g_, b, a) in px.reshape(-1, 4):
            if a > 128:
                colores[(int(r), int(g_), int(b))] = colores.get((int(r), int(g_), int(b)), 0) + 1
    # la paleta: 96 colores bastan (el dibujo ya viene tramado) y así el archivo no se dispara en la flash
    CUANTOS = 96
    pal = sorted(colores)
    if len(pal) > CUANTOS:
        muestra = np.array([c for c, n in colores.items() for _ in range(min(n, 400))], np.uint8)
        q = Image.fromarray(muestra.reshape(-1, 1, 3)).quantize(colors=CUANTOS, method=Image.Quantize.MEDIANCUT)
        pq = np.array(q.getpalette()[: CUANTOS * 3], np.uint8).reshape(-1, 3)
        pal = sorted({tuple(int(v) for v in c) for c in pq})
    arr = np.array(pal, np.int32)
    cache = {}

    def cerca(c):
        if c not in cache:
            cache[c] = int(np.argmin(((arr - np.array(c)) ** 2).sum(1))) + 1
        return cache[c]

    partes = ["/* GENERADO por herramientas/iconos_nuevos/huevos92.py: los 8 huevos de siempre a 92 x 90 (el mismo dibujo, con más detalle y volumen). No editar. */"]
    partes.append("static const uint8_t HU_PAL[%d] = {%s};" % (1 + len(pal) * 3, ",".join([str(len(pal))] + [str(v) for c in pal for v in c])))
    for e in range(8):
        for g in range(4):
            datos = bytes(0 if a <= 128 else cerca((int(r), int(gg), int(b))) for (r, gg, b, a) in imgs[(e, g)].reshape(-1, 4))
            comp = lz_encode(datos)
            partes.append("static const uint8_t HU_%d_%d[%d] = {%s};" % (e, g, len(comp), ",".join(str(v) for v in comp)))
    partes.append("static const struct { const uint8_t *d; uint16_t n; } HU_SPR[8][4] = {" +
                  ",".join("{" + ",".join("{HU_%d_%d,sizeof(HU_%d_%d)}" % (e, g, e, g) for g in range(4)) + "}" for e in range(8)) + "};")
    partes.append("#define HU_W %d" % W)
    partes.append("#define HU_H %d" % H)
    destino = os.path.join(RAIZ, "firmware", "main", "huevos_data.inc")
    open(destino, "w").write("\n".join(partes) + "\n")
    print("escrito %s (%d colores, %d KB)" % (destino, len(pal), os.path.getsize(destino) // 1024))
    if len(sys.argv) > 1 and sys.argv[1] == "muestra":
        hoja = Image.new("RGBA", (W * 8, H * 4), (28, 28, 38, 255))
        for e in range(8):
            for g in range(4):
                im = Image.fromarray(imgs[(e, g)], "RGBA")
                hoja.paste(im, (e * W, g * H), im)
        hoja.save("/tmp/huevos92.png")
        print("muestra: /tmp/huevos92.png")


if __name__ == "__main__":
    main()
