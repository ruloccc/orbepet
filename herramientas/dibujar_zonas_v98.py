"""(v98) Dibuja los fondos de las dos zonas nuevas, vistas desde arriba como el resto del mundo:
   - claro_<estación>.png: el claro de las luciérnagas con la casita de Tilo (sale del prado y del bosque: misma mano)
   - mirador_<estación>.png: la cima del cerro, justo sobre la cueva (rocas del cerro, el mar de nubes alrededor)
Escribe en fondos_zonas/ (1024 x 1024). Después: python herramientas/hacer_fondos.py
Uso: python herramientas/dibujar_zonas_v98.py [claro|mirador]   (sin nada: las dos)
"""
import os, sys, math
import numpy as np
from PIL import Image
from collections import deque

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
FZ = os.path.join(RAIZ, "fondos_zonas")
ESTACIONES = ["primavera", "verano", "otono", "invierno"]
PX = 4  # un pixel del dibujo = 4 pixeles del lienzo de 1024 (como los fondos de siempre)


def cargar(n):
    return np.asarray(Image.open(os.path.join(FZ, n + ".png")).convert("RGB")).astype(np.int16)


def hexc(h):
    return np.array([(h >> 16) & 255, (h >> 8) & 255, h & 255], np.int16)


class Rng:
    def __init__(self, s):
        self.r = np.random.RandomState(s)

    def u(self, a=0.0, b=1.0):
        return float(self.r.uniform(a, b))

    def i(self, a, b):
        return int(self.r.randint(a, b))


# ---------------------------------------------------------------- recortes del mundo
def es_fondo(c):
    r, g, b = c[..., 0], c[..., 1], c[..., 2]
    return (r * 3 + g * 6 + b) / 10 > 95


def recortar(img, x0, y0, x1, y1, umbral=95):
    """recorta un objeto con contorno oscuro (árbol, arbusto): el fondo claro conectado al borde queda afuera"""
    c = img[y0:y1, x0:x1].copy()
    h, w = c.shape[:2]
    r, g, b = c[..., 0], c[..., 1], c[..., 2]
    fondo = (r * 3 + g * 6 + b) / 10 > umbral
    vis = np.zeros((h, w), bool)
    q = deque()
    for x in range(w):
        for y in (0, h - 1):
            if fondo[y, x] and not vis[y, x]:
                vis[y, x] = 1; q.append((y, x))
    for y in range(h):
        for x in (0, w - 1):
            if fondo[y, x] and not vis[y, x]:
                vis[y, x] = 1; q.append((y, x))
    while q:
        y, x = q.popleft()
        for dy, dx in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            yy, xx = y + dy, x + dx
            if 0 <= yy < h and 0 <= xx < w and not vis[yy, xx] and fondo[yy, xx]:
                vis[yy, xx] = 1; q.append((yy, xx))
    m = ~vis
    # solo la mancha más grande (sin migas sueltas)
    lab = np.zeros((h, w), np.int32); n = 0; tam = [0]
    for y in range(h):
        for x in range(w):
            if m[y, x] and not lab[y, x]:
                n += 1; k = 0; lab[y, x] = n; q.append((y, x))
                while q:
                    yy, xx = q.popleft(); k += 1
                    for dy, dx in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                        a, bb = yy + dy, xx + dx
                        if 0 <= a < h and 0 <= bb < w and m[a, bb] and not lab[a, bb]:
                            lab[a, bb] = n; q.append((a, bb))
                tam.append(k)
    if n:
        mejor = int(np.argmax(tam))
        m = lab == mejor
    return c, m


def pegar(lien, spr, mask, x, y, sombra=0):
    h, w = mask.shape
    if sombra:
        # sombra suave hacia abajo a la derecha
        sm = np.zeros_like(mask)
        dy, dx = sombra, sombra // 2
        sm[dy:, dx:] = mask[:h - dy, :w - dx]
        sm &= ~mask
        reg = lien[max(0, y):y + h, max(0, x):x + w]
        oy, ox = max(0, -y), max(0, -x)
        s2 = sm[oy:oy + reg.shape[0], ox:ox + reg.shape[1]]
        reg[s2] = (reg[s2] * 0.72).astype(np.int16)
    for yy in range(h):
        Y = y + yy
        if not 0 <= Y < lien.shape[0]:
            continue
        x0 = max(0, -x); x1 = min(w, lien.shape[1] - x)
        if x1 <= x0:
            continue
        fila = mask[yy, x0:x1]
        lien[Y, x + x0:x + x1][fila] = spr[yy, x0:x1][fila]


def rellenar_pasto(lien, mascara, fuentes, rng, bloque=40):
    """tapa lo que esté en la máscara copiando trocitos de pasto (fuentes: rectángulos sin nada encima)"""
    H, W = mascara.shape
    ys, xs = np.nonzero(mascara)
    if not len(ys):
        return
    y0, y1, x0, x1 = ys.min(), ys.max() + 1, xs.min(), xs.max() + 1
    orig = lien.copy()
    for by in range(y0 - bloque // 2, y1, bloque // 2):
        for bx in range(x0 - bloque // 2, x1, bloque // 2):
            yy0, xx0 = max(0, by), max(0, bx)
            yy1, xx1 = min(H, by + bloque), min(W, bx + bloque)
            if yy1 <= yy0 or xx1 <= xx0:
                continue
            sub = mascara[yy0:yy1, xx0:xx1]
            if not sub.any():
                continue
            fx0, fy0, fx1, fy1 = fuentes[rng.i(0, len(fuentes))]
            hh, ww = yy1 - yy0, xx1 - xx0
            sx = rng.i(fx0, max(fx0 + 1, fx1 - ww)); sy = rng.i(fy0, max(fy0 + 1, fy1 - hh))
            src = orig[sy:sy + hh, sx:sx + ww]
            # borde irregular (pixelado de a 4) para que no se vean cuadrados
            nb = np.random.RandomState(rng.i(0, 1 << 30)).rand((hh + 3) // 4, (ww + 3) // 4)
            nb = np.kron(nb, np.ones((4, 4)))[:hh, :ww]
            cy, cx = np.mgrid[0:hh, 0:ww]
            d = np.hypot((cy - hh / 2) / (hh / 2), (cx - ww / 2) / (ww / 2))
            sel = sub & ((d + (nb - 0.5) * 0.5) < 1.0)
            lien[yy0:yy1, xx0:xx1][sel] = src[sel]


def igualar_tono(lien, mascara, radio=90):
    """corrige el tono de lo rellenado para que siga al de alrededor (sin manchas más claras u oscuras)"""
    from PIL import ImageFilter
    H, W = mascara.shape
    def borroso(a):
        im = Image.fromarray(np.clip(a, 0, 255).astype(np.uint8))
        return np.asarray(im.filter(ImageFilter.GaussianBlur(radio))).astype(np.float32)
    afuera = (~mascara).astype(np.float32)
    pesos = np.maximum(borroso(np.repeat((afuera * 255)[..., None], 3, 2))[..., 0] / 255.0, 0.03)
    obj = borroso(lien * afuera[..., None]) / pesos[..., None]
    # el tono propio del relleno
    dentro = mascara.astype(np.float32)
    pd = np.maximum(borroso(np.repeat((dentro * 255)[..., None], 3, 2))[..., 0] / 255.0, 0.03)
    prop = borroso(lien * dentro[..., None]) / pd[..., None]
    corr = lien + (obj - prop) * 0.9
    lien[mascara] = np.clip(corr[mascara], 0, 255).astype(np.int16)


# ---------------------------------------------------------------- dibujo de pixeles (de a PX)
class Spr:
    def __init__(self, w, h):
        self.w, self.h = w, h
        self.c = np.zeros((h, w, 3), np.int16)
        self.m = np.zeros((h, w), bool)

    def p(self, x, y, col):
        x, y = int(round(x)), int(round(y))
        if 0 <= x < self.w and 0 <= y < self.h:
            self.c[y, x] = col if isinstance(col, np.ndarray) else hexc(col)
            self.m[y, x] = True

    def rect(self, x0, y0, x1, y1, col):
        for y in range(int(y0), int(y1)):
            for x in range(int(x0), int(x1)):
                self.p(x, y, col)

    def elipse(self, cx, cy, rx, ry, col):
        for y in range(int(cy - ry - 1), int(cy + ry + 2)):
            for x in range(int(cx - rx - 1), int(cx + rx + 2)):
                if ((x - cx) / rx) ** 2 + ((y - cy) / ry) ** 2 <= 1.0:
                    self.p(x, y, col)

    def contorno(self, col):
        """pinta de col los pixeles vacíos que tocan algo (el contorno oscuro del pixel art)"""
        m = self.m.copy()
        borde = np.zeros_like(m)
        borde[1:, :] |= m[:-1, :]; borde[:-1, :] |= m[1:, :]; borde[:, 1:] |= m[:, :-1]; borde[:, :-1] |= m[:, 1:]
        borde &= ~m
        self.c[borde] = hexc(col); self.m |= borde

    def grande(self):
        return np.kron(self.c, np.ones((PX, PX, 1), np.int16)), np.kron(self.m, np.ones((PX, PX), bool))


def mezcla(a, b, t):
    a, b = hexc(a), hexc(b)
    return (a + (b - a) * t).astype(np.int16)


def casita_tilo(est):
    """La casita de Tilo vista desde arriba (tres cuartos, como el árbol-casa del prado): techo de tejuelas de alerce
    con musgo, muros de piedra y madera, puerta redonda, ventanas con luz, chimenea. 60 x 58 pixeles."""
    W, H = 62, 60
    s = Spr(W, H)
    rng = Rng(7)
    inv = est == "invierno"
    # muro del frente (abajo): piedra abajo, madera arriba
    wx0, wx1, wy0, wy1 = 9, 53, 34, 54
    for y in range(wy0, wy1):
        for x in range(wx0, wx1):
            if y >= wy1 - 6:  # zócalo de piedra
                k = ((x + (y // 3) * 2) // 4 + y // 3) % 3
                col = [0x8D8A86, 0x7A7672, 0x9C9893][k]
                if (x + (y // 3) * 2) % 4 == 0 or y % 3 == 0:
                    col = 0x5E5A57
                s.p(x, y, col)
            else:  # tablas de madera verticales
                k = (x - wx0) % 4
                col = [0xB98A5A, 0xA9784A, 0xC69766, 0x8E6038][k]
                if k == 3:
                    col = 0x7A5030
                if rng.u() < 0.06:
                    col = 0x9A6A40
                s.p(x, y, col)
    # vigas
    for x in range(wx0, wx1):
        s.p(x, wy0, 0x6A4428); s.p(x, wy0 + 1, 0x6A4428)
    for x in (wx0, wx1 - 1):
        for y in range(wy0, wy1):
            s.p(x, y, 0x5E3C22)
    # puerta redonda verde (la de Tilo es chiquita)
    dcx, dtop = 31, 39
    for y in range(dtop, wy1):
        for x in range(dcx - 6, dcx + 7):
            dy = y - (dtop + 6)
            if dy < 0 and (x - dcx) ** 2 + dy * dy > 42:
                continue
            borde = (x - dcx) ** 2 + (dy if dy < 0 else 0) ** 2 > 28 or x in (dcx - 6, dcx + 6)
            col = 0x3D2A18 if borde else (0x4E8A4A if (x - dcx) % 3 else 0x3F7440)
            s.p(x, y, col)
    s.p(dcx + 3, dtop + 9, 0xF2D25A); s.p(dcx + 3, dtop + 10, 0xC9A23A)  # pomo
    s.rect(dcx - 7, wy1, dcx + 8, wy1 + 2, 0x9C9893)  # escalón
    for x in range(dcx - 7, dcx + 8):
        s.p(x, wy1 + 1, 0x6E6A66)
    # ventanas redondeadas con luz cálida
    for vx in (16, 42):
        for y in range(40, 47):
            for x in range(vx - 3, vx + 4):
                marco = x in (vx - 3, vx + 3) or y in (40, 46)
                col = 0x5E3C22 if marco else (0xFFE9A0 if (y < 43) else 0xF5C55A)
                if not marco and x == vx:
                    col = 0x7A5030
                if not marco and y == 43:
                    col = 0x7A5030
                s.p(x, y, col)
        # jardinera
        flor = {"primavera": [0xF59AC0, 0xFFFFFF], "verano": [0xFFD04A, 0xF56A4A], "otono": [0xE08A3A, 0xB5523B], "invierno": [0xE8F4FF, 0xC8DCEE]}[est]
        for x in range(vx - 4, vx + 5):
            s.p(x, 47, 0x6A4428); s.p(x, 48, 0x553620)
            if (x + vx) % 2 == 0:
                s.p(x, 46, flor[(x // 2) % 2] if not inv else 0xF4FAFF)
    # techo: tejuelas en filas (vistas desde arriba y adelante), con alero sobre el muro
    rx0, rx1, ry0, ry1 = 3, 59, 4, 38
    teja = [0xB5523B, 0x9E4430, 0xC8664A, 0x84382A]
    for y in range(ry0, ry1):
        # el techo se angosta arriba (la cumbrera) y se abre abajo (el alero)
        t = (y - ry0) / (ry1 - ry0)
        mx = int(round(10 * (1 - t) ** 1.6))
        for x in range(rx0 + mx, rx1 - mx):
            fila = (y - ry0) // 4
            off = (fila % 2) * 3
            k = ((x + off) // 6) % 4
            col = teja[k % 3]
            if (y - ry0) % 4 == 3:
                col = 0x6E2E22  # sombra bajo cada fila
            elif (x + off) % 6 == 0:
                col = 0x84382A
            elif (y - ry0) % 4 == 0:
                col = 0xD9805E  # luz arriba de la tejuela
            # musgo en manchas
            nm = math.sin(x * 0.35 + y * 0.2) + math.sin(x * 0.13 - y * 0.41) * 0.8
            if nm > 1.25 and not inv:
                col = 0x6E9A3E if (x + y) % 3 else 0x587E30
            s.p(x, y, col)
    for x in range(rx0 + 10, rx1 - 10):  # cumbrera
        s.p(x, ry0, 0x5E2A1E); s.p(x, ry0 + 1, 0x7A3424)
    for x in range(rx0, rx1):  # borde del alero
        s.p(x, ry1 - 1, 0x4E2418); s.p(x, ry1, 0x3A1A12)
    # nieve y carámbanos en invierno; hojas en otoño
    if inv:
        for y in range(ry0, ry1 - 3):
            t = (y - ry0) / (ry1 - ry0)
            mx = int(round(10 * (1 - t) ** 1.6))
            for x in range(rx0 + mx, rx1 - mx):
                n = math.sin(x * 0.5) * 1.5 + math.sin(x * 0.17 + 1) * 2
                if y < ry1 - 6 + n:
                    col = 0xF4FAFF if (y - ry0) % 4 != 3 else 0xD6E6F4
                    if x < rx0 + mx + 2 or x > rx1 - mx - 3:
                        col = 0xC8DCEE
                    s.p(x, y, col)
        for x in range(rx0 + 2, rx1 - 2, 5):
            for k in range(rng.i(2, 4)):
                s.p(x, ry1 + 1 + k, 0xE8F6FF if k < 2 else 0xBFE0F4)
    elif est == "otono":
        for _ in range(26):
            x, y = rng.i(rx0 + 6, rx1 - 6), rng.i(ry0 + 4, ry1 - 4)
            col = [0xE08A3A, 0xF2B23E, 0xC85A2A][rng.i(0, 3)]
            s.p(x, y, col); s.p(x + 1, y, col)
    elif est == "primavera":
        for _ in range(10):
            x, y = rng.i(rx0 + 8, rx1 - 8), rng.i(ry0 + 6, ry1 - 6)
            s.p(x, y, 0xFFFFFF); s.p(x + 1, y, 0xF59AC0)
    # chimenea de piedra a la derecha, con humito
    for y in range(0, 14):
        for x in range(43, 50):
            col = [0x8D8A86, 0x7A7672, 0x9C9893][((x // 2) + (y // 2)) % 3]
            if y % 3 == 0 or x in (43, 49):
                col = 0x5E5A57
            s.p(x, y, col)
    for x in range(42, 51):
        s.p(x, 0, 0x4A4744)
    if inv:
        for x in range(43, 50):
            s.p(x, 1, 0xF4FAFF)
    s.contorno(0x2A1A10)
    return s


def camino(lien, puntos, ancho, rng, cols):
    """un sendero de tierra (colores del camino del prado) que pasa por los puntos"""
    H, W = lien.shape[:2]
    mask = np.zeros((H, W), bool)
    yy, xx = np.mgrid[0:H, 0:W]
    pts = []
    for (a, b) in zip(puntos[:-1], puntos[1:]):
        n = int(math.hypot(b[0] - a[0], b[1] - a[1]) / 6) + 1
        for k in range(n):
            t = k / n
            pts.append((a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t))
    pts.append(puntos[-1])
    borde = np.zeros((H, W), bool)
    for i, (x, y) in enumerate(pts):
        w = ancho * (0.85 + 0.15 * math.sin(i * 0.37))
        x0, x1 = int(max(0, x - w - 12)), int(min(W, x + w + 12))
        y0, y1 = int(max(0, y - w - 12)), int(min(H, y + w + 12))
        d = np.hypot(xx[y0:y1, x0:x1] - x, yy[y0:y1, x0:x1] - y)
        mask[y0:y1, x0:x1] |= d < w
        borde[y0:y1, x0:x1] |= d < w + 8
    # pixelado de a 4
    def px4(m):
        return np.kron(m[::4, ::4], np.ones((4, 4), bool))[:H, :W]
    mask, borde = px4(mask), px4(borde)
    arena, arena2, oscuro, orilla = cols
    lien[borde & ~mask] = orilla
    noise = np.kron(np.random.RandomState(3).rand(H // 4 + 1, W // 4 + 1), np.ones((4, 4)))[:H, :W]
    lien[mask] = arena
    lien[mask & (noise < 0.18)] = arena2
    lien[mask & (noise > 0.93)] = oscuro
    # borde de adentro un poquito más oscuro
    interior = np.zeros_like(mask)
    interior[4:-4, 4:-4] = mask[:-8, 4:-4] & mask[8:, 4:-4] & mask[4:-4, :-8] & mask[4:-4, 8:]
    lien[mask & ~interior] = arena2
    return mask | borde


def sello(lien, x, y, pix, pal):
    """pinta un dibujito chico (lista de strings, cada carácter un color de pal; '.' vacío) de a PX"""
    for j, fila in enumerate(pix):
        for i, ch in enumerate(fila):
            if ch == "." or ch == " ":
                continue
            X, Y = x + i * PX, y + j * PX
            if 0 <= X < 1024 - PX and 0 <= Y < 1024 - PX:
                lien[Y:Y + PX, X:X + PX] = hexc(pal[ch])


HONGO = ["..rrr..", ".rrwrr.", "rwrrrwr", "kkkkkkk", "..eee..", "..eee..", "..kkk.."]
HONGO_PAL = {"r": 0xD8443A, "w": 0xFFF4E8, "k": 0x3A1A12, "e": 0xF0E2C8}
LENA = ["..kkkkkkkk..", ".kbbcbbcbbk.", "kcbdbcbdbbck", "kbbcbbcbbcbk", ".kkkkkkkkkk.", ".kbcbbcbbck.", "kbbdbbcbdbbk", ".kkkkkkkkkk."]
LENA_PAL = {"k": 0x3A2414, "b": 0xA9784A, "c": 0xC69766, "d": 0xE0C29A}
BUZON = ["kkkkk", "krrrk", "krwrk", "krrrk", "kkkkk", "..k..", "..k..", "..k.."]
BUZON_PAL = {"k": 0x3A2414, "r": 0xC85A3A, "w": 0xFFF4E8}
PIEDRA_PASO = [".kkkk.", "kgggfk", "kggffk", ".kkkk."]
PIEDRA_PAL = {"k": 0x4E4A48, "g": 0x9C9893, "f": 0x7A7672}


def claro(est):
    """El claro del Rey Peumo, dibujado entero (nada copiado de otra zona): un claro redondo en medio del bosque, con la
    casa del rey (un tocón de peumo, como la casita del prado pero suya) arriba al centro, la poza a la derecha con
    juncos y nenúfares, el sendero de las luciérnagas que entra por el este, el anillo de hongos, el huerto del rey,
    un tronco caído y piedras. Se pinta de a pixel de dibujo (256) y se agranda x4."""
    import pixel_mundo as pm
    inv, oto, pri = est == "invierno", est == "otono", est == "primavera"
    L = pm.Lienzo(60 + ESTACIONES.index(est))
    if inv:
        pasto = (0xBCCEE2, 0xD4E2F0, 0xE8F1FA, 0xFFFFFF); flores = None
        hojas = [(0x24463A, 0x3A6450, 0xDCEAF6, 0xFFFFFF)]; cont = 0x14302A
    elif oto:
        pasto = (0x86862E, 0x9E9838, 0xB6AC48, 0xD2C266); flores = (90, [0xE89A3A, 0xC8641E, 0xF8CC60])
        hojas = [(0x8A3A16, 0xC8641E, 0xE89A30, 0xF8CC60), (0x6E1A14, 0xA8302A, 0xD4543A, 0xF08A5A), (0x7A5A14, 0xB8902A, 0xDCB83E, 0xF4DC78)]
        cont = 0x3A1A0C
    else:
        pasto = (0x3A8236, 0x4A9A40, 0x5EAE4C, 0x84C862)
        flores = (160 if pri else 110, [0xFFFFFF, 0xFFE45A, 0xF59AC0, 0xB9A0FF] if pri else [0xFFFFFF, 0xFFE45A, 0xF5B06A])
        hojas = [(0x1A5428, 0x2A7232, 0x40923C, 0x76BE58), (0x1E5A2E, 0x307A3A, 0x4C9C46, 0x86C866)]
        cont = 0x10301A
    L.pasto(pasto, sem=7, matas=1100, flores=flores, treboles=0 if inv else 70)
    # ---- el sendero de las luciérnagas (del este, desde el bosque, hasta la puerta)
    ccam = (0xDCC08A, 0xC8A672, 0xA8865A, 0x7A5A3A) if not inv else (0xE8E4DC, 0xD6CEC2, 0xBCB2A4, 0x9C9286)
    L.camino([(262, 170), (230, 168), (196, 160), (168, 148), (140, 126), (122, 110), (113, 101)], 5.2, ccam, sem=3)
    # piedras de paso hacia la poza
    for (x, y) in ((150, 128), (157, 120)):
        L.piedra(x, y, 3, 2, nieve=inv)
    # ---- la poza (a la derecha)
    agua = (0x2E6AB0, 0x3E82C4, 0x5A9ED6, 0x7AB8E6) if not inv else (0xA8C8E2, 0xC0D8EE, 0xD8E8F6, 0x8EB4D4)
    orilla = (0xD8C48E, 0xBCA672, 0x4A3A22) if not inv else (0xE8EEF4, 0xCCD8E4, 0x6A7A8A)
    L.poza(186, 112, 25, 16, agua, orilla, sem=5, helada=inv)
    if not inv:
        for (x, y, f) in ((176, 108, 0xF59AC0), (194, 116, 0), (188, 104, 0xFFFFFF if pri else 0), (200, 108, 0)):
            L.nenufar(x, y, 3, f if not oto else 0)
    L.juncos(206, 104, 4, sem=2) if not inv else L.juncos(206, 104, 3, cols=(0xA89A7A, 0x8A7A5A, 0x7A5A3A), sem=2)
    L.juncos(164, 124, 3, sem=4) if not inv else None
    # ---- el bosque alrededor (deja abierta la entrada del este)
    fuera = [(-160, 30), (-130, 32), (-100, 30), (-70, 30), (-40, 32), (-12, 30), (60, 30), (90, 32), (120, 30), (150, 32), (180, 30), (205, 30)]
    dentro = [(-150, 20), (-118, 22), (-88, 19), (-58, 21), (-28, 19), (52, 20), (78, 22), (104, 20), (132, 22), (160, 20), (188, 21), (214, 19)]
    k = 0
    for lista, dist in ((fuera, 140), (dentro, 110)):
        for (ang_g, r) in lista:
            a = math.radians(ang_g)
            x, y = 128 + dist * math.cos(a), 128 + dist * math.sin(a) * 0.98
            L.arbol(x, y, r, hojas[k % len(hojas)], cont, sem=k + 1,
                    motas=(0xF8C0D8, 0.07) if pri and k % 3 == 0 else None)
            k += 1
    # pinos y arbustos en el borde del claro
    pinc = (0x0E3A24, 0x1E5A34, 0x3A7A44) if not inv else (0x1E3A30, 0x2E5A44, 0x4A7A5E)
    L.pino(52, 70, 30, pinc, nieve=inv)
    L.pino(214, 196, 26, pinc, nieve=inv)
    bcol = hojas[0]
    L.arbusto(44, 122, 8, bcol, cont, bayas=None if inv else (0xD83A3A if not oto else 0x6A2A6A), sem=21)
    L.arbusto(222, 76, 7, bcol, cont, bayas=None if inv else 0x3A2A8A, sem=22)
    L.arbusto(166, 64, 6, bcol, cont, sem=23)
    # ---- el huerto del rey (a la izquierda de la casa): surcos con brotes
    for f in range(4):
        y = 98 + f * 5
        for x in range(54, 76):
            L.p(x, y, 0x6A4428 if not inv else 0xC8C0B4); L.p(x, y + 1, 0x4A2E1A if not inv else 0xA89E92)
            if x % 3 == 0 and not inv:
                c = (0x5EB44A, 0xE8402A, 0xFFE45A, 0x5EB44A)[f] if not oto else 0xC89A3A
                L.p(x, y - 1, c)
        if inv:
            for x in range(54, 76, 2):
                L.p(x, y - 1, 0xFFFFFF)
    for x in range(52, 78, 3):  # cerquito
        for y in range(94, 116):
            if x in (52, 76) or y in (94, 115):
                L.p(x, y, 0x8A5E3A if (x + y) % 2 else 0x5A3A22)
    # ---- el anillo de hongos (abajo a la izquierda), donde bailan las luciérnagas
    for k in range(8):
        a = k / 8 * 2 * math.pi
        x, y = 86 + 15 * math.cos(a), 176 + 9 * math.sin(a)
        if inv:
            L.sello(int(x) - 2, int(y) - 2, [".ww.", "wwww", "kkkk", ".ee."], {"w": 0xF4FAFF, "k": 0x8AA0B4, "e": 0xE0E8F0})
        else:
            col = 0xD8443A if k % 2 == 0 else 0xE8C88A
            L.sello(int(x) - 2, int(y) - 2, [".rr.", "rwrr", "kkkk", ".ee."], {"r": col, "w": 0xFFF4E8, "k": 0x3A1A12, "e": 0xF0E2C8})
    # ---- el tronco caído y piedras. (1.0.36) Más arriba y un poco a la derecha (antes: x 116, y 206, abajo al centro):
    # ahí quedaba encima del botón de jugar y al tocar el tronco se tocaba el botón. TX/TY = esquina de arriba a la izquierda.
    TX, TY = 126, 178
    for y in range(TY, TY + 7):
        for x in range(TX, TX + 34):
            col = 0x8A5E3A if y < TY + 2 else (0x6A4428 if y < TY + 5 else 0x4A2E1A)
            if (x - TX) % 7 == 0:
                col = 0x4A2E1A
            if inv and y < TY + 2:
                col = 0xF4FAFF
            L.p(x, y, col)
    for y in range(TY - 1, TY + 8):
        for x in range(TX + 30, TX + 37):
            d = math.hypot(x - (TX + 33), (y - (TY + 3.5)) * 0.9)
            if d < 4.2:
                L.p(x, y, 0x3A2414 if d > 3.2 else (0xC8A070 if int(d) % 2 else 0xA87E50))
    if not inv:
        for x in (TX + 4, TX + 12, TX + 23):
            L.p(x, TY - 1, 0x6CC050); L.p(x + 1, TY - 1, 0x4A9A3E)
    # florcitas en manchones y pasto alto
    if not inv:
        for (fx, fy, fc) in ((150, 190, 0xFFE45A), (104, 140, 0xFFFFFF), (176, 160, 0xF59AC0 if not oto else 0xE89A3A), (70, 200, 0xB9A0FF if not oto else 0xC8641E)):
            for _ in range(22):
                a = L.u(0, 6.28); d = L.u(0, 7)
                x, y = fx + d * math.cos(a), fy + d * math.sin(a) * 0.7
                L.p(x, y, fc); L.p(x, y + 1, 0x2E6A2E if not oto else 0x6A5A1E)
    L.arbusto(118, 160, 5, hojas[0], cont, bayas=None if inv else 0xF59AC0, sem=31)
    L.arbusto(210, 150, 6, hojas[-1], cont, sem=32)
    L.piedra(60, 146, 6, 4, musgo=None if inv else 0x5E9A3E, nieve=inv)
    L.piedra(66, 150, 3, 2, nieve=inv)
    L.piedra(200, 196, 5, 3, nieve=inv)
    L.piedra(150, 82, 3, 2, nieve=inv)
    # ---- la casa del rey, arriba al centro (como la casita del prado: se entra tocándola)
    casa = pm.casa_rey(est)
    cxs, cys = 74, 20
    L.sombra_elipse(cxs + 42, cys + 76, 36, 7, 0.68)
    casa.poner(L, cxs, cys)
    # buzón y leña al lado de la casa
    L.sello(cxs + 74, cys + 62, ["kkkkk", "krrrk", "krwrk", "kkkkk", "..k..", "..k..", "..k.."],
            {"k": 0x3A2414, "r": 0x3A6ACB, "w": 0xFFF4E8})
    L.sello(cxs - 8, cys + 66, ["..kkkkkk..", ".kbcbbcbk.", "kcbdbcbdbk", ".kkkkkkkk.", "kbbdbbcbdk", ".kkkkkkkk."],
            {"k": 0x3A2414, "b": 0xA9784A, "c": 0xC69766, "d": 0xE0C29A if not inv else 0xFFFFFF})
    return L.grande()


# ---------------------------------------------------------------- el mirador
def ruido(n, escala, sem):
    """ruido suave (suma de senos y grilla interpolada) de n x n"""
    rs = np.random.RandomState(sem)
    tot = np.zeros((n, n))
    amp = 1.0
    for k in range(4):
        g = max(2, int(n / escala) + 1)
        base = rs.rand(g + 1, g + 1)
        im = Image.fromarray((base * 255).astype(np.uint8)).resize((n, n), Image.BICUBIC)
        tot += np.asarray(im) / 255.0 * amp
        escala /= 2; amp /= 2
    return tot / 1.875


BAYER = np.array([[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]]) / 16.0


def mirador(est):
    """La cima del cerro vista desde arriba, como un mapa en relieve: la meseta de la cumbre (pasto de altura, llaretas,
    la apacheta, piedras en el borde) con acantilados de roca en capas al norte, este y oeste, y al sur el lomo por donde
    sube el sendero desde el cerro. Abajo, muy lejos, el valle: arbolitos, praderas, la laguna y el río, con neblina;
    alrededor, otras cumbres con nieve; unas nubes redondas pasan bajo la cima, con su sombra.
    Se dibuja de a pixel de dibujo (256) y se agranda x4."""
    N = 256
    rng = Rng(400 + ESTACIONES.index(est))
    inv, oto, pri, ver = est == "invierno", est == "otono", est == "primavera", est == "verano"
    yy, xx = np.mgrid[0:N, 0:N].astype(np.float64)
    iy, ix = np.mgrid[0:N, 0:N]
    bay = BAYER[iy % 4, ix % 4]
    cx, cy = 128.0, 118.0
    ang = np.arctan2(yy - cy, xx - cx)
    d = np.hypot(xx - cx, (yy - cy) * 1.05)
    n1, n2, n3 = ruido(N, 40, 11), ruido(N, 20, 12), ruido(N, 64, 13)
    # ---------------- el relieve: un cerro con su cumbre redondeada y tres aristas que bajan (la del sur trae el sendero)
    cono = 1.0 - np.clip(d / 138.0, 0, 1) ** 2.0
    arista = np.zeros((N, N))
    for (th, ancho, fuerza) in ((math.pi / 2, 0.30, 0.20), (-0.55, 0.22, 0.15), (math.pi + 0.45, 0.22, 0.15), (-2.2, 0.18, 0.10)):
        dif = np.angle(np.exp(1j * (ang - th)))
        arista = np.maximum(arista, fuerza * np.exp(-(dif / ancho) ** 2) * np.clip(d / 60.0, 0, 1))
    cana = 0.5 + 0.5 * np.sin(ang * 15 + n1 * 7)
    surcos = (np.clip(d / 70.0, 0, 1) * 0.05 * (cana - 0.5))
    h = cono + arista + surcos + 0.06 * (n2 - 0.5)
    tope = 0.86 + 0.03 * n2                       # la cumbre: redondeada y casi plana donde se camina
    h = np.where(h > tope, tope + (h - tope) * 0.25, h)
    h = np.maximum(h, 0.06 + 0.10 * n3)
    borde = np.full((N, N), 1e9)                   # (sin filo: la cumbre baja de a poco hacia las laderas)
    vecinas = np.zeros((N, N)); cumbre_v = np.zeros((N, N), bool)
    for (px, py, r, alto) in ((0, 10, 70, 0.70), (262, 22, 64, 0.66)):
        dp = np.hypot(xx - px, yy - py)
        ap = np.arctan2(yy - py, xx - px)
        filo = 0.68 + 0.32 * np.abs(np.sin(ap * 2.5 + px))
        hp = alto * np.clip(1 - dp / (r * filo), 0, 1) ** 1.1
        cumbre_v |= hp > alto * 0.6
        vecinas = np.maximum(vecinas, hp)
    h = np.maximum(h, vecinas)
    es_vecina = (vecinas >= h - 1e-6) & (vecinas > 0.2)
    cima = h > 0.78
    xr = 128 + 6 * np.sin(yy / 30.0)
    w = 22 + 4 * n1
    lomo = np.zeros((N, N))
    en_lomo = np.zeros((N, N), bool)
    # ---------------- luz
    gy, gx = np.gradient(h * 70.0)
    nz = 1.0 / np.sqrt(gx * gx + gy * gy + 1)
    luz = np.clip(((gx * 0.62 + gy * 0.62) * nz + 0.5 * nz + 0.12) / 1.05, 0, 1)
    pend = np.sqrt(gx * gx + gy * gy)

    def ramp(cols, t, dither=True):
        L = len(cols)
        k = np.floor(t * L + ((bay - 0.5) * 0.9 if dither else 0)).astype(int)
        k = np.clip(k, 0, L - 1)
        return np.stack([hexc(c) for c in cols])[k]

    img = np.zeros((N, N, 3), np.int16)
    # ---------------- el valle (muy abajo)
    if inv:
        prad = [0xA9BED0, 0xBFD0DE, 0xD4E2EE, 0xE8F1F8]
        arb = (0x2E4A50, 0x4A6A70, 0x7A9AA4, 0xE8F2FA)
    elif oto:
        prad = [0x8A7438, 0xA08A46, 0xB8A25A, 0xCCB86E]
        arb = (0x5A2E14, 0xA0521E, 0xD88A30, 0xF2C060)
    elif pri:
        prad = [0x5A8A3A, 0x709E46, 0x88B456, 0x9ECA68]
        arb = (0x1A3E22, 0x2E6232, 0x4A8A42, 0x78B85A)
    else:
        prad = [0x6A8E3C, 0x80A44A, 0x98B85A, 0xB0CC6C]
        arb = (0x1A3E22, 0x2C5E30, 0x447E3C, 0x6EA850)
    img[:] = ramp(prad, np.clip(luz * 0.8 + 0.1, 0, 1))
    valle = h < 0.30
    bosque = (n2 > 0.47) | (n3 > 0.6)
    # arbolitos: copas de 3x3 en una grilla corrida
    for gy0 in range(0, N, 4):
        for gx0 in range(0, N, 4):
            x = gx0 + (2 if (gy0 // 4) % 2 else 0) + rng.i(-1, 2)
            y = gy0 + rng.i(-1, 2)
            if not (0 < x < N - 2 and 0 < y < N - 2) or not valle[y, x] or not bosque[y, x]:
                continue
            o, m, l, hi = (hexc(c) for c in arb)
            img[y + 1, x - 1:x + 2] = o
            img[y, x - 1:x + 2] = m
            img[y - 1, x] = l
            img[y, x - 1] = l
            if inv:
                img[y - 1, x] = hi
            img[min(N - 1, y + 2), x] = (img[min(N - 1, y + 2), x] * 0.7).astype(np.int16)
    # florcitas en las praderas
    if not inv:
        fl = (np.random.RandomState(8).rand(N, N) < 0.01) & valle & ~bosque
        img[fl] = hexc(0xFFE45A if not oto else 0xE89A3A)
    # la laguna de los cisnes y el río que baja
    lag = ((xx - 44) / 28) ** 2 + ((yy - 200) / 16) ** 2 < 1 + 0.25 * (n2 - 0.5)
    rio = (np.abs(xx - (58 + 70 * (yy / 256) + 12 * np.sin(yy / 14.0))) < 1.8) & (yy > 128) & (yy < 192) & valle
    agua_c = [0x2E6AA8, 0x4A86C2, 0x6EA6DA, 0x9CC8EE] if not inv else [0x9ABCD6, 0xB8D2E6, 0xD6E6F4, 0xF0F7FC]
    agua = ramp(agua_c, np.clip(0.25 + 0.6 * n2 + 0.25 * (luz - 0.5), 0, 1))
    m = lag | rio
    img[m] = agua[m]
    orl = np.zeros_like(m); orl[1:] |= m[:-1]; orl[:-1] |= m[1:]; orl[:, 1:] |= m[:, :-1]; orl[:, :-1] |= m[:, 1:]
    img[orl & ~m] = hexc(0xC8B486 if not inv else 0xDCE4EC)
    # ---------------- roca en capas (laderas, lomo y otras cumbres)
    roca_c = [0x55576F, 0x63657E, 0x71738C, 0x80829A, 0x8F91A8, 0x9FA1B6, 0xB0B2C5, 0xC4C6D6]
    q = h * 13 + n1 * 1.2 + np.sin(ang * 7) * 0.25
    fr = q - np.floor(q)
    tr = np.clip(luz * 0.85 + 0.10 * (cana - 0.5) + 0.08, 0, 1)
    k = np.clip(np.floor(tr * 8 + (bay - 0.5) * 0.9).astype(int), 0, 7)
    k = np.where(fr < 0.22, k - 3, np.where((fr < 0.40) & (luz > 0.30), k + 2, k))  # capas: línea oscura y ceja clara
    grieta = (np.abs(np.sin(ang * 31 + n2 * 9)) < 0.05) & (fr > 0.3)              # grietas que bajan
    k = np.where(grieta, k - 2, k)
    v_roc = np.stack([hexc(c) for c in roca_c])[np.clip(k, 0, 7)]
    roca = (h >= 0.30) & ~cima & ~((n2 > 0.6) & (h > 0.62) & (pend < 1.6))  # roca, con algún manchón de pasto arriba
    img[roca] = v_roc[roca]
    # piedras sueltas en las laderas
    for _ in range(260):
        x, y = rng.i(2, N - 3), rng.i(2, N - 3)
        if roca[y, x] and roca[y + 1, x + 1]:
            img[y, x] = hexc(0xC4C6D6); img[y, x + 1] = hexc(0x9FA1B6); img[y + 1, x] = hexc(0x80829A); img[y + 1, x + 1] = hexc(0x45475E)
    # el lomo: pasto de altura con piedras sueltas
    pasto = {"primavera": (0x92BC52, 0x7EA846, 0xA8CC66, 0x6A9440), "verano": (0xB4BC5A, 0x9EA64A, 0xC8D070, 0x86903E),
             "otono": (0xC4A458, 0xAC8C46, 0xD6B86C, 0x96783C), "invierno": (0xF2F8FF, 0xDDE9F5, 0xFFFFFF, 0xC6D6E6)}[est]
    v_lom = ramp([pasto[3], pasto[1], pasto[0], pasto[2]], np.clip(luz * 0.9 + 0.05, 0, 1))
    lm = (h >= 0.30) & ~cima & ~roca
    img[lm] = v_lom[lm]
    # nieve: gorros en las otras cumbres (en invierno, en todo lo alto)
    nieve_c = [0x9AB0C8, 0xB6C8DE, 0xD2E0F0, 0xEAF2FA, 0xFFFFFF]
    v_nie = ramp(nieve_c, np.clip(luz * 1.05, 0, 1))
    um = {"invierno": -1.0, "otono": 0.42, "primavera": 0.48, "verano": 0.58}[est]
    nev = cumbre_v & es_vecina & ((n2 + 0.35 * cana) > um)
    if inv:  # en invierno: nieve en las cejas de cada capa de roca y en las lomas suaves
        nev |= roca & (fr >= 0.22) & (fr < 0.55)
        nev |= (h > 0.3) & ~cima & ~roca & ~lm
    elif not ver:
        nev |= roca & (h > 0.62) & (cana > 0.88) & (luz < 0.35)
    img[nev] = v_nie[nev]
    # neblina sobre lo que está muy abajo
    cielo = hexc(0xCFE6F4) if not inv else hexc(0xE4EEF6)
    neb = (0.18 + np.clip((0.30 - h) / 0.30, 0, 1) * 0.32)[..., None] * (h < 0.3)[..., None]
    img = (img * (1 - neb) + cielo * neb).astype(np.int16)
    # ---------------- nubes redondas bajo la cumbre (con sombra)
    nubes = []
    for _ in range(40):
        if len(nubes) >= 6:
            break
        x0, y0 = rng.u(10, 246), rng.u(10, 246)
        if h[int(y0), int(x0)] > 0.5 or np.hypot(x0 - 128, y0 - 128) > 124 or h[int(y0), int(x0)] > 0.42:
            continue
        nubes.append([(x0 + rng.u(-12, 12), y0 + rng.u(-4, 4), rng.u(5, 10)) for _ in range(rng.i(3, 6))])
    cm = np.zeros((N, N), bool); cs = np.zeros((N, N), bool); clo = np.zeros((N, N), bool)
    for nube in nubes:
        for (x0, y0, r) in nube:
            e = (xx - x0) ** 2 + ((yy - y0) * 1.25) ** 2 < r * r
            cm |= e
            cs |= (xx - x0 - 9) ** 2 + ((yy - y0 - 7) * 1.25) ** 2 < r * r
            clo |= (xx - x0) ** 2 + ((yy - y0 + r * 0.35) * 1.25) ** 2 < (r * 0.8) ** 2
    som = cs & ~cm
    img[som] = (img[som] * 0.78).astype(np.int16)
    img[cm] = hexc(0xDCEAF6)
    img[cm & clo] = hexc(0xFFFFFF)
    ob = np.zeros_like(cm); ob[:-1] |= cm[1:]; ob[1:] |= cm[:-1]; ob[:, 1:] |= cm[:, :-1]; ob[:, :-1] |= cm[:, 1:]
    img[ob & ~cm] = hexc(0xA8C4DC)
    # ---------------- la meseta de la cumbre
    nm = ruido(N, 16, 31)
    nf = np.random.RandomState(5).rand(N, N)
    pc = np.where((nm > 0.55)[..., None], hexc(pasto[1]), hexc(pasto[0]))
    pc = np.where((nm < 0.36)[..., None], hexc(pasto[2]), pc)
    domo = np.clip(1.0 + 0.09 * (-(xx - cx) - (yy - cy)) / 68.0, 0.88, 1.10)
    pc = (pc * domo[..., None]).astype(np.int16)
    img[cima] = pc[cima]
    # matitas en V
    for _ in range(420):
        a = rng.u(0, 6.28); rq = rng.u(0, 68)
        x, y = int(cx + rq * math.cos(a)), int(cy + rq * math.sin(a) * 0.95)
        if 1 <= x < N - 1 and 1 <= y < N and cima[y, x]:
            img[y, x] = hexc(pasto[3]); img[y - 1, x - 1] = hexc(pasto[1]); img[y - 1, x + 1] = hexc(pasto[1])
    # el sendero que sube por el lomo hasta la cumbre
    sx = 128 + 6 * np.sin(yy / 22.0) + 2 * np.sin(yy / 7.0)
    send = (np.abs(xx - sx) < 2.6) & (yy > cy + 30)
    send |= (((xx - 128) ** 2 + ((yy - (cy + 30)) * 1.3) ** 2) < 30)
    cs_ = [0xD6C49A, 0xC2AE82, 0xA8946A] if not inv else [0xE8E4DA, 0xD8D0C2, 0xC0B6A6]
    img[send] = hexc(cs_[0])
    img[send & ((ix + iy * 3) % 7 == 0)] = hexc(cs_[1])
    orill = np.zeros_like(send); orill[:, 1:] |= send[:, :-1]; orill[:, :-1] |= send[:, 1:]; orill[1:] |= send[:-1]
    img[orill & ~send] = hexc(cs_[2])
    # el filo de la cumbre: contorno oscuro y piedras que asoman en el borde
    orilla = cima & (h < 0.80)
    img[orilla] = (img[orilla] * 0.92).astype(np.int16)
    s = Spr(N, N)
    s.c[:] = img; s.m[:] = True

    def piedra(px, py, rx, ry, base=0x9EA0B6, luz_=0xC2C4D4, osc=0x6A6C84):
        for y in range(int(py - ry - 2), int(py + ry + 3)):
            for x in range(int(px - rx - 2), int(px + rx + 3)):
                q_ = ((x - px) / rx) ** 2 + ((y - py) / ry) ** 2
                if q_ <= 1:
                    col = base
                    if (y - py) < -ry * 0.35 and (x - px) < rx * 0.3:
                        col = luz_
                    if (y - py) > ry * 0.45:
                        col = osc
                    s.p(x, y, col)
                elif q_ <= 1.4 and y > py:
                    s.p(x, y, 0x34364A)
        if inv:
            for x in range(int(px - rx + 1), int(px + rx)):
                s.p(x, int(py - ry * 0.55), 0xF4FAFF)

    # piedras sueltas sobre la cumbre, en grupitos
    for k in range(9):
        a = rng.u(0, 2 * math.pi)
        rb = rng.u(30, 58)
        for j in range(rng.i(1, 4)):
            px, py = cx + (rb + rng.u(-3, 3)) * math.cos(a + rng.u(-0.08, 0.08)), cy + (rb + rng.u(-3, 3)) * math.sin(a) / 1.05
            if abs(px - 128) < 8 and py > cy + 20:
                continue  # no sobre el sendero
            r_ = rng.u(1.6, 3.8)
            piedra(px, py, r_ * 1.4, r_)
    # la piedra tibia donde se sentaba Tilo (plana y grande)
    piedra(90, 122, 14, 8, 0xA89A88, 0xCCBEA8, 0x7A6C5A)
    for x in range(81, 99):
        s.p(x, 119, 0xD8CAB2)
    # la apacheta (piedras apiladas de los viajeros)
    for (px, py, rx, ry) in ((162, 90, 9, 5), (162, 84, 7, 4), (161, 79, 5, 3), (162, 75, 3, 2)):
        piedra(px, py, rx, ry)
    # la roca del cóndor (al borde, arriba)
    piedra(122, 58, 11, 6, 0x8C8EA6, 0xB2B4C6, 0x5E6078)
    for x in range(115, 130, 3):
        s.p(x, 54, 0xFFFFFF)
    # llaretas
    for (px, py, r) in ((148, 132, 6), (104, 88, 5), (160, 110, 4), (104, 146, 5), (76, 104, 4), (178, 120, 4), (146, 70, 4)):
        for y in range(py - r - 1, py + r + 2):
            for x in range(px - r - 1, px + r + 2):
                q_ = (x - px) ** 2 + ((y - py) * 1.2) ** 2
                if q_ <= r * r:
                    col = 0x5E8A34 if not inv else 0xE6F0F8
                    if (x + y) % 2 == 0:
                        col = 0x6E9C3E if not inv else 0xF8FCFF
                    if y < py - r * 0.4:
                        col = 0x8CBC52 if not inv else 0xFFFFFF
                    if oto and (x * 7 + y * 3) % 11 == 0:
                        col = 0xC89A3A
                    s.p(x, y, col)
                elif q_ <= (r + 1.2) ** 2 and y >= py:
                    s.p(x, y, 0x2E4A1E if not inv else 0x9AB0C4)
    # el arbusto de la corona
    for y in range(136, 158):
        for x in range(158, 184):
            q_ = ((x - 171) / 12.0) ** 2 + ((y - 147) / 9.0) ** 2
            if q_ <= 1:
                hoja = [0x4E8A3A, 0x5E9E44, 0x3E7430] if not oto else [0xD8822E, 0xE8A23A, 0xB8602A]
                if inv:
                    hoja = [0x3E6A30, 0x4E7A3A, 0x30562A]
                col = hoja[(x * 7 + y * 13 + (x * y) % 5) % 3]
                if y < 143 and (x + y) % 2:
                    col = 0xF4FAFF if inv else (0x8CC45A if not oto else 0xF4C25A)
                s.p(x, y, col)
            elif q_ <= 1.25:
                s.p(x, y, 0x1E3A16 if not oto else 0x6A3A16)
    if pri:
        for (x, y) in ((164, 142), (176, 144), (170, 152), (161, 149)):
            s.p(x, y, 0xF59AC0)
    # la grieta de la cueva (la cueva está justo debajo): brilla el cristal entre las piedras
    for y in range(146, 158):
        wq = 2.6 - abs(y - 152) / 2.6
        xc = 76 + (y - 152) * 0.5
        for x in range(int(xc - wq), int(xc + wq + 1)):
            s.p(x, y, 0x2A2440 if abs(x - xc) >= wq - 1 else (0xB48CFF if (y % 3) else 0xE0CCFF))
    # florcitas del cerro / escarcha
    for _ in range(90 if not inv else 40):
        a = rng.u(0, 6.28); rq = rng.u(0, 58)
        x, y = int(cx + rq * math.cos(a)), int(cy + rq * math.sin(a) * 0.9)
        if 0 <= x < N and 0 <= y < N and cima[y, x]:
            if inv:
                s.p(x, y, 0xC8E4FF)
            else:
                s.p(x, y, [0xFFE45A, 0xFFFFFF, 0xF59AC0, 0xB9A0FF][rng.i(0, 4)] if not oto else 0xE8A23A)
    big = np.kron(s.c, np.ones((PX, PX, 1), np.int16))
    return np.clip(big, 0, 255).astype(np.uint8)


def main():
    cuales = sys.argv[1:] or ["claro", "mirador"]
    for est in ESTACIONES:
        if "claro" in cuales:
            Image.fromarray(claro(est)).save(os.path.join(FZ, "claro_%s.png" % est))
            print("  claro_%s.png" % est)
        if "mirador" in cuales:
            Image.fromarray(mirador(est)).save(os.path.join(FZ, "mirador_%s.png" % est))
            print("  mirador_%s.png" % est)


if __name__ == "__main__":
    main()
