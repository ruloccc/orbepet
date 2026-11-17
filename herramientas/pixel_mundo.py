"""(v98) Pinceles de pixel art para dibujar zonas del mundo vistas desde arriba, con el estilo de los fondos de siempre:
contorno oscuro, copas de árbol hechas de bolitas con luz arriba a la izquierda, pasto con matitas y florcitas.
Se pinta en una grilla de 256 x 256 (cada pixel del dibujo = 4 del lienzo de 1024)."""
import math
import numpy as np
from PIL import Image

N = 256
BAYER = np.array([[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]]) / 16.0


def hexc(h):
    return np.array([(h >> 16) & 255, (h >> 8) & 255, h & 255], np.int16)


def ruido(n, escala, sem, octavas=4):
    rs = np.random.RandomState(sem)
    tot = np.zeros((n, n)); amp = 1.0; s = 0
    for _ in range(octavas):
        g = max(2, int(n / escala) + 1)
        base = rs.rand(g + 1, g + 1)
        im = Image.fromarray((base * 255).astype(np.uint8)).resize((n, n), Image.BICUBIC)
        tot += np.asarray(im) / 255.0 * amp
        s += amp; escala /= 2; amp /= 2
    return tot / s


class Lienzo:
    def __init__(self, sem=1):
        self.c = np.zeros((N, N, 3), np.int16)
        self.rng = np.random.RandomState(sem)
        yy, xx = np.mgrid[0:N, 0:N]
        self.yy, self.xx = yy, xx
        self.bay = BAYER[yy % 4, xx % 4]

    def u(self, a=0.0, b=1.0):
        return float(self.rng.uniform(a, b))

    def i(self, a, b):
        return int(self.rng.randint(a, b))

    def p(self, x, y, col):
        x, y = int(round(x)), int(round(y))
        if 0 <= x < N and 0 <= y < N:
            self.c[y, x] = hexc(col) if isinstance(col, int) else col

    def oscurecer(self, mask, f):
        self.c[mask] = (self.c[mask] * f).astype(np.int16)

    def grande(self):
        return np.clip(np.kron(self.c, np.ones((4, 4, 1), np.int16)), 0, 255).astype(np.uint8)

    # ------------------------------------------------------------ suelo
    def pasto(self, cols, sem=3, matas=900, flores=None, treboles=60):
        """cols: (oscuro, medio, claro, muy claro). Manchas grandes en tres tonos con borde tramado, matitas en V, tréboles y flores."""
        n1 = ruido(N, 48, sem) * 0.7 + ruido(N, 14, sem + 1) * 0.3
        t = n1 + (self.bay - 0.5) * 0.06
        k = np.where(t < 0.42, 0, np.where(t < 0.58, 1, 2))
        pal = np.stack([hexc(cols[0]), hexc(cols[1]), hexc(cols[2])])
        self.c[:] = pal[k]
        # matitas: un pixel oscuro abajo y dos claros arriba (o una W)
        for _ in range(matas):
            x, y = self.i(1, N - 1), self.i(1, N - 1)
            base = k[y, x]
            osc = cols[max(0, base - 1)] if base > 0 else cols[0]
            cla = cols[min(3, base + 1)]
            if base == 0:
                osc = mezclar(cols[0], 0x000000, 0.25)
            self.p(x, y, osc)
            self.p(x - 1, y - 1, cla); self.p(x + 1, y - 1, cla)
            if self.u() < 0.3:
                self.p(x, y - 2, cla)
        for _ in range(treboles):
            x, y = self.i(2, N - 2), self.i(2, N - 2)
            v = mezclar(cols[0], 0x103010, 0.3)
            for dx, dy in ((0, -1), (-1, 0), (1, 0)):
                self.p(x + dx, y + dy, v)
            self.p(x, y, cols[3])
        if flores:
            for _ in range(flores[0]):
                x, y = self.i(2, N - 2), self.i(2, N - 2)
                c = flores[1][self.i(0, len(flores[1]))]
                self.p(x, y, c)
                if self.u() < 0.5:
                    self.p(x + 1, y, c); self.p(x, y + 1, mezclar(c, 0x000000, 0.3))
        return k

    def sombra_elipse(self, cx, cy, rx, ry, f=0.72):
        m = ((self.xx - cx) / rx) ** 2 + ((self.yy - cy) / ry) ** 2 < 1
        m &= (self.bay < 0.94) | (((self.xx - cx) / rx) ** 2 + ((self.yy - cy) / ry) ** 2 < 0.8)
        self.oscurecer(m, f)

    # ------------------------------------------------------------ árboles
    def copa(self, cx, cy, r, cols, contorno, sem=0, bolitas=None, motas=None):
        """Una copa redonda hecha de bolitas (como los árboles del bosque): cada bolita tiene luz arriba a la izquierda,
        una línea oscura donde se monta sobre otra y motas. cols: (sombra, base, luz, brillo)."""
        rs = np.random.RandomState(sem)
        if bolitas is None:
            bol = [(cx, cy - r * 0.05, r * 0.55)]
            nb = max(7, int(r / 1.5))
            for k in range(nb):
                a = k / nb * 2 * math.pi + rs.uniform(-0.2, 0.2)
                rr = r * rs.uniform(0.42, 0.58)
                bol.append((cx + math.cos(a) * (r - rr) * 0.98, cy + math.sin(a) * (r - rr) * 0.9, rr * rs.uniform(0.85, 1.05)))
            for k in range(max(2, nb // 3)):
                a = rs.uniform(0, 2 * math.pi)
                rr = r * rs.uniform(0.3, 0.42)
                bol.append((cx + math.cos(a) * r * 0.3, cy + math.sin(a) * r * 0.3, rr))
        else:
            bol = bolitas
        bol.sort(key=lambda b: b[1] + b[2] * 0.3)  # las de abajo tapan a las de arriba
        x0, x1 = int(cx - r - 3), int(cx + r + 4)
        y0, y1 = int(cy - r - 3), int(cy + r + 4)
        x0, y0 = max(0, x0), max(0, y0)
        x1, y1 = min(N, x1), min(N, y1)
        if x1 <= x0 or y1 <= y0:
            return
        yy, xx = self.yy[y0:y1, x0:x1], self.xx[y0:y1, x0:x1]
        dueno = -np.ones(xx.shape, int)
        for k, (bx, by, br) in enumerate(bol):
            dueno[((xx - bx) ** 2 + (yy - by) ** 2) <= br * br] = k
        m = dueno >= 0
        out = np.zeros(xx.shape + (3,), np.int16)
        bay = self.bay[y0:y1, x0:x1]
        mot = rs.rand(*xx.shape)
        for k, (bx, by, br) in enumerate(bol):
            mk = dueno == k
            if not mk.any():
                continue
            dx, dy = (xx - bx) / br, (yy - by) / br
            luz = -(dx * 0.7 + dy * 0.8)  # 1 arriba-izquierda
            t = luz * 0.55 + 0.5 + (bay - 0.5) * 0.25
            idx = np.clip((t * 3.2).astype(int), 0, 3)
            pal = np.stack([hexc(c) for c in cols])
            col = pal[idx]
            # motas: claras en la luz, oscuras en la sombra
            col = np.where(((mot < 0.10) & (idx >= 2))[..., None], hexc(cols[3]), col)
            col = np.where(((mot > 0.92) & (idx <= 1))[..., None], hexc(cols[0]), col)
            if motas is not None:
                col = np.where(((mot > 0.5) & (mot < 0.5 + motas[1]))[..., None], hexc(motas[0]), col)
            # borde de la bolita (donde se monta sobre otra): oscuro
            d = np.sqrt(dx * dx + dy * dy)
            col = np.where(((d > 0.80) & (dy > -0.45))[..., None], hexc(cols[0]), col)
            col = np.where(((d > 0.92) & (dy > -0.1) & (dx > -0.6))[..., None], hexc(mezclar(cols[0], contorno, 0.55)), col)
            out[mk] = col[mk]
        # contorno exterior
        borde = np.zeros_like(m)
        borde[1:, :] |= m[:-1, :]; borde[:-1, :] |= m[1:, :]; borde[:, 1:] |= m[:, :-1]; borde[:, :-1] |= m[:, 1:]
        borde &= ~m
        reg = self.c[y0:y1, x0:x1]
        reg[m] = out[m]
        reg[borde] = hexc(contorno)

    def arbol(self, cx, cy, r, cols, contorno=0x16301C, tronco=(0x5A3A22, 0x7A5232, 0x3A2414), sem=0, motas=None):
        """árbol visto desde arriba: sombra en el suelo, tronco con raíces que asoma abajo y la copa"""
        self.sombra_elipse(cx + r * 0.25, cy + r * 0.75, r * 0.95, r * 0.42, 0.70)
        # tronco
        ty0, ty1 = int(cy + r * 0.45), int(cy + r * 0.95)
        w = max(2, int(r * 0.18))
        for y in range(ty0, ty1 + 1):
            ww = w + (1 if y > ty1 - 2 else 0) + (2 if y == ty1 else 0)
            for x in range(int(cx - ww), int(cx + ww + 1)):
                col = tronco[1] if (x - cx) < 0 else tronco[0]
                if (x + y) % 4 == 0:
                    col = tronco[2]
                if x in (int(cx - ww), int(cx + ww)):
                    col = tronco[2]
                self.p(x, y, col)
        self.copa(cx, cy, r, cols, contorno, sem, motas=motas)

    def pino(self, cx, cy, h, cols, contorno=0x0E2414, nieve=False):
        """pino de tres pisos (como los del prado)"""
        self.sombra_elipse(cx + h * 0.15, cy + h * 0.52, h * 0.32, h * 0.12, 0.7)
        for y in range(int(cy + h * 0.38), int(cy + h * 0.52)):
            for x in (int(cx) - 1, int(cx), int(cx) + 1):
                self.p(x, y, 0x4A2E18 if x < cx else 0x6A4428)
        pisos = 3
        for k in range(pisos):
            top = cy - h * 0.5 + k * h * 0.22
            base = top + h * 0.42
            ancho = h * (0.22 + 0.08 * k)
            for y in range(int(top), int(base) + 1):
                t = (y - top) / max(1, base - top)
                hw = ancho * t
                for x in range(int(cx - hw), int(cx + hw) + 1):
                    lado = (x - cx) / max(1, hw)
                    col = cols[2] if lado < -0.35 else (cols[1] if lado < 0.4 else cols[0])
                    if y >= base - 1:
                        col = cols[0]
                    if nieve and t < 0.55 and lado < 0.5:
                        col = 0xF4FAFF if lado < 0 else 0xD8E6F2
                    if abs(abs(x - cx) - hw) < 0.9 or y >= base:
                        col = contorno
                    self.p(x, y, col)

    def arbusto(self, cx, cy, r, cols, contorno=0x16301C, bayas=None, sem=0):
        self.sombra_elipse(cx + 1, cy + r * 0.6, r * 1.0, r * 0.45, 0.75)
        self.copa(cx, cy, r, cols, contorno, sem)
        if bayas:
            rs = np.random.RandomState(sem + 9)
            for _ in range(int(r * 1.5)):
                a = rs.uniform(0, 6.28); d = rs.uniform(0, r * 0.75)
                self.p(cx + math.cos(a) * d, cy + math.sin(a) * d * 0.9, bayas)

    # ------------------------------------------------------------ agua, piedras, caminos
    def poza(self, cx, cy, rx, ry, agua, orilla, sem=0, helada=False, nenufares=True):
        rs = np.random.RandomState(sem)
        ang = np.arctan2(self.yy - cy, self.xx - cx)
        fase = rs.uniform(0, 6.28)
        borde = 1 + 0.10 * np.sin(ang * 3 + fase) + 0.06 * np.sin(ang * 5 - fase) + 0.04 * np.sin(ang * 9)
        d = np.sqrt(((self.xx - cx) / rx) ** 2 + ((self.yy - cy) / ry) ** 2) / borde
        agua_m = d < 1.0
        playa = (d >= 1.0) & (d < 1.0 + 3.2 / rx)
        cont = (d >= 1.0 + 3.2 / rx) & (d < 1.0 + 4.4 / rx)
        self.c[cont] = hexc(orilla[2])
        self.c[playa] = hexc(orilla[0])
        pl2 = playa & ((self.bay > 0.7) | (self.yy > cy + ry * 0.2))
        self.c[pl2] = hexc(orilla[1])
        # agua: más clara en la orilla, más honda al centro; sombra de la orilla arriba
        t = np.clip(1.0 - d, 0, 1) + (self.bay - 0.5) * 0.12
        k = np.clip((t * 3.4).astype(int), 0, 3)
        pal = np.stack([hexc(c) for c in agua])
        self.c[agua_m] = pal[k][agua_m]
        sombra = agua_m & (d > 0.84) & (self.yy < cy)
        self.c[sombra] = hexc(agua[3]) if not helada else hexc(agua[0])
        if helada:
            for _ in range(int(rx)):
                x0, y0 = rs.uniform(cx - rx * 0.7, cx + rx * 0.5), rs.uniform(cy - ry * 0.6, cy + ry * 0.6)
                L = rs.randint(3, 7)
                for j in range(L):
                    self.p(x0 + j, y0 - j * 0.5, 0xFFFFFF)
        else:
            for _ in range(int(rx * 0.6)):  # brillitos del agua
                x0, y0 = rs.uniform(cx - rx * 0.7, cx + rx * 0.6), rs.uniform(cy - ry * 0.6, cy + ry * 0.6)
                if agua_m[int(y0), int(x0)]:
                    L = rs.randint(2, 5)
                    for j in range(L):
                        if agua_m[int(y0), int(x0 + j)]:
                            self.p(x0 + j, y0, 0xE6F6FF)
        return agua_m, d

    def nenufar(self, x, y, r=3, flor=0xF59AC0):
        for yy in range(int(y - r), int(y + r + 1)):
            for xx in range(int(x - r), int(x + r + 1)):
                q = (xx - x) ** 2 + ((yy - y) * 1.2) ** 2
                if q <= r * r and not (xx > x and abs(yy - y) <= 0.6):
                    self.p(xx, yy, 0x4E9A3E if (xx - x) + (yy - y) < 0 else 0x3A7A30)
                elif q <= (r + 1) ** 2 and yy >= y:
                    self.p(xx, yy, 0x1E4A22)
        if flor:
            self.p(x - 1, y - 1, flor); self.p(x, y - 2, flor); self.p(x - 2, y - 2, flor); self.p(x - 1, y - 2, 0xFFF2A0)

    def juncos(self, x, y, n=4, cols=(0x5E8A34, 0x3E6A24, 0x7A4A22), sem=0):
        rs = np.random.RandomState(sem)
        for k in range(n):
            xx = x + k * 2 + rs.randint(-1, 2)
            h = rs.randint(5, 9)
            for j in range(h):
                self.p(xx, y - j, cols[0] if j % 2 else cols[1])
            if rs.uniform() < 0.6:  # totora
                self.p(xx, y - h, cols[2]); self.p(xx, y - h - 1, cols[2]); self.p(xx, y - h - 2, 0xA06A3A)

    def piedra(self, cx, cy, rx, ry, cols=(0x5E5C6A, 0x8A8896, 0xB2B0BC, 0xD6D4DE), contorno=0x34323E, musgo=None, nieve=False):
        self.sombra_elipse(cx + 1, cy + ry * 0.6, rx * 1.05, ry * 0.6, 0.72)
        for y in range(int(cy - ry - 1), int(cy + ry + 2)):
            for x in range(int(cx - rx - 1), int(cx + rx + 2)):
                q = ((x - cx) / rx) ** 2 + ((y - cy) / ry) ** 2
                if q <= 1:
                    dx, dy = (x - cx) / rx, (y - cy) / ry
                    l = -(dx * 0.6 + dy * 0.9)
                    col = cols[3] if l > 0.65 else (cols[2] if l > 0.15 else (cols[1] if l > -0.45 else cols[0]))
                    if musgo and dy < -0.35 and ((x * 7 + y * 3) % 5 < 3):
                        col = musgo
                    if nieve and dy < -0.25:
                        col = 0xF4FAFF if dx < 0.3 else 0xD8E6F2
                    self.p(x, y, col)
                elif q <= 1.0 + 2.4 / max(rx, ry):
                    self.p(x, y, contorno)

    def camino(self, puntos, ancho, cols, sem=0, piedritas=True):
        """sendero de tierra: cols (claro, medio, oscuro, orilla)"""
        m = np.zeros((N, N), bool)
        pts = []
        for a, b in zip(puntos[:-1], puntos[1:]):
            n = int(math.hypot(b[0] - a[0], b[1] - a[1]) * 2) + 1
            for k in range(n):
                t = k / n
                pts.append((a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t))
        pts.append(puntos[-1])
        for j, (x, y) in enumerate(pts):
            w = ancho * (0.9 + 0.12 * math.sin(j * 0.21))
            m |= (self.xx - x) ** 2 + ((self.yy - y) * 1.1) ** 2 < w * w
        orl = np.zeros_like(m)
        orl[1:] |= m[:-1]; orl[:-1] |= m[1:]; orl[:, 1:] |= m[:, :-1]; orl[:, :-1] |= m[:, 1:]
        orl &= ~m
        n2 = ruido(N, 10, sem + 50)
        k = np.where(n2 + (self.bay - 0.5) * 0.15 < 0.42, 1, 0)
        pal = np.stack([hexc(cols[0]), hexc(cols[1])])
        self.c[m] = pal[k][m]
        inner = np.zeros_like(m)
        inner[1:-1, 1:-1] = m[:-2, 1:-1] & m[2:, 1:-1] & m[1:-1, :-2] & m[1:-1, 2:]
        self.c[m & ~inner & (self.yy % 2 == 0)] = hexc(cols[2])
        self.c[orl] = hexc(cols[3])
        if piedritas:
            rs = np.random.RandomState(sem)
            ys, xs = np.nonzero(inner)
            for _ in range(len(ys) // 40):
                q = rs.randint(0, len(ys))
                self.p(xs[q], ys[q], cols[2]); self.p(xs[q] + 1, ys[q], mezclar(cols[0], 0xFFFFFF, 0.4))
        return m

    def sello(self, x, y, pix, pal):
        for j, fila in enumerate(pix):
            for i, ch in enumerate(fila):
                if ch not in (".", " "):
                    self.p(x + i, y + j, pal[ch])


def mezclar(a, b, t):
    ca = [(a >> 16) & 255, (a >> 8) & 255, a & 255]
    cb = [(b >> 16) & 255, (b >> 8) & 255, b & 255]
    c = [int(ca[k] + (cb[k] - ca[k]) * t) for k in range(3)]
    return (c[0] << 16) | (c[1] << 8) | c[2]


class Sprite:
    """dibujo suelto (con transparencia) para poner encima de una zona"""
    def __init__(self, w, h):
        self.w, self.h = w, h
        self.c = np.zeros((h, w, 3), np.int16)
        self.m = np.zeros((h, w), bool)

    def p(self, x, y, col):
        x, y = int(round(x)), int(round(y))
        if 0 <= x < self.w and 0 <= y < self.h:
            self.c[y, x] = hexc(col); self.m[y, x] = True

    def contorno(self, col):
        m = self.m
        b = np.zeros_like(m)
        b[1:, :] |= m[:-1, :]; b[:-1, :] |= m[1:, :]; b[:, 1:] |= m[:, :-1]; b[:, :-1] |= m[:, 1:]
        b &= ~m
        self.c[b] = hexc(col); self.m |= b

    def poner(self, L, x, y):
        for j in range(self.h):
            Y = y + j
            if not 0 <= Y < N:
                continue
            for i in range(self.w):
                X = x + i
                if 0 <= X < N and self.m[j, i]:
                    L.c[Y, X] = self.c[j, i]


def casa_rey(est):
    """La casa del Rey Peumo: como la casita-tocón del prado, pero la de un rey viejito: un tocón de peumo ancho con
    raíces grandes, techo de musgo en bolitas con su chimenea de piedra y una coronita dorada de veleta, banderín azul,
    puerta de arco con remaches y pomo dorado, dos ventanas redondas con luz, un farol, copihues que trepan por el tronco
    y escalones de piedra. 76 x 80 pixeles de dibujo."""
    W, H = 76, 80
    s = Sprite(W, H)
    rs = np.random.RandomState(11)
    inv, oto, pri = est == "invierno", est == "otono", est == "primavera"
    yy, xx = np.mgrid[0:H, 0:W]
    # ---- tronco (de y 34 a 74), se ensancha abajo en raíces
    cx = 38
    for y in range(30, 75):
        t = (y - 30) / 44.0
        hw = 22 + 3 * math.sin(y * 0.4) * 0.3 + (t ** 3) * 12
        for x in range(int(cx - hw), int(cx + hw) + 1):
            lado = (x - cx) / hw
            surco = (int(x * 1.0 + math.sin(y * 0.25 + x) * 1.5) % 5 == 0)
            col = 0x8A5E3A if lado < -0.45 else (0x734C2E if lado < 0.35 else 0x5A3A22)
            if surco:
                col = 0x4A2E1A
            if lado < -0.7 and not surco:
                col = 0x9E7048
            if inv and lado < -0.2 and rs.rand() < 0.06:
                col = 0xD8E6F2
            s.p(x, y, col)
    # raíces que salen a los lados
    for (sx, dirx) in ((cx - 30, -1), (cx + 30, 1), (cx - 18, -1), (cx + 19, 1)):
        for k in range(9):
            x = sx + dirx * k * 0.8
            y = 70 + k * 0.45
            for j in range(-1, 2):
                s.p(x, y + j, 0x6A4428 if j < 1 else 0x4A2E1A)
    # ---- puerta de arco
    dx0, dx1, dy0, dy1 = 29, 47, 46, 72
    dcx = (dx0 + dx1) / 2
    for y in range(dy0 - 6, dy1):
        for x in range(dx0 - 2, dx1 + 3):
            arco = y < dy0 + 3
            dd = math.hypot((x - dcx) / ((dx1 - dx0) / 2 + 2), (y - (dy0 + 3)) / 9.5) if arco else 0
            if arco and dd > 1:
                continue
            marco = x < dx0 or x > dx1 or (arco and dd > 0.8)
            col = 0x4A2E1A if marco else (0x9A6A3E if (x - dx0) % 5 else 0x6E4626)
            if not marco and (x - dx0) % 5 == 2:
                col = 0xB07C48
            s.p(x, y, col)
    for y in (dy0 + 4, dy1 - 6):  # bisagras
        for x in range(dx0, dx0 + 7):
            s.p(x, y, 0x3A3A44)
        s.p(dx0 + 6, y, 0xC8C8D4)
    s.p(dx1 - 3, 60, 0xFFD24A); s.p(dx1 - 3, 61, 0xC89A2A); s.p(dx1 - 2, 60, 0xFFF0A0)  # pomo
    # coronita tallada sobre la puerta
    for (x, y) in ((dcx - 3, dy0 - 8), (dcx, dy0 - 9), (dcx + 3, dy0 - 8)):
        s.p(x, y, 0xFFD24A)
    for x in range(int(dcx - 3), int(dcx + 4)):
        s.p(x, dy0 - 7, 0xE0A830)
    # ---- ventanas redondas
    for (wx, wy) in ((17, 52), (59, 50)):
        for y in range(wy - 5, wy + 6):
            for x in range(wx - 5, wx + 6):
                d = math.hypot(x - wx, y - wy)
                if d <= 5.4:
                    col = 0x4A2E1A if d > 4.2 else (0xFFF0B0 if y < wy else 0xF8C860)
                    if d <= 4.2 and (x == wx or y == wy):
                        col = 0x6E4626
                    if inv and d <= 4.2:
                        col = 0xFFE89A if y < wy else 0xF0B850
                    s.p(x, y, col)
        for x in range(wx - 6, wx + 7):  # repisa
            s.p(x, wy + 6, 0x6E4626); s.p(x, wy + 7, 0x4A2E1A)
    # ---- copihues trepando (rojos, la flor de Chile) o hiedra de otoño / nieve
    hoja = 0x3E7A30 if not oto else 0xB8602A
    for k in range(14):
        x = 8 + k * 0.4 + math.sin(k * 0.9) * 2
        y = 72 - k * 2.6
        s.p(x, y, hoja); s.p(x + 1, y, 0x2E5A24 if not oto else 0x8A4420)
        if k % 3 == 1 and not inv:
            s.p(x + 2, y + 1, 0xD8283A); s.p(x + 2, y + 2, 0xB01828); s.p(x + 3, y + 1, 0xF04A5A)
    # ---- farol junto a la puerta
    for y in range(50, 58):
        s.p(52, y, 0x3A3A44)
    for y in range(44, 50):
        for x in range(50, 55):
            s.p(x, y, 0x3A3A44 if x in (50, 54) or y in (44, 49) else 0xFFE07A)
    s.p(52, 43, 0x3A3A44)
    # ---- techo de musgo en bolitas (de y 6 a 40), más ancho que el tronco
    musgo = {"primavera": (0x2E6A2E, 0x449A3E, 0x6CC050, 0xA8E07A), "verano": (0x2E6A2E, 0x449A3E, 0x6CC050, 0xA8E07A),
             "otono": (0x8A3A16, 0xC8641E, 0xE89A30, 0xF8CC60), "invierno": (0x9AB0C8, 0xC8D8EA, 0xEEF5FC, 0xFFFFFF)}[est]
    bol = []
    for k in range(9):  # el borde de abajo: bolitas grandes
        x = 6 + k * 8
        bol.append((x, 33 + (2 if k % 2 else 0), 7.5))
    for k in range(7):
        bol.append((10 + k * 9.3, 23 + (1 if k % 2 else 0), 8.5))
    for k in range(4):
        bol.append((20 + k * 12, 14, 8.5))
    bol.sort(key=lambda b: b[1])
    dueno = -np.ones((H, W), int)
    for k, (bx, by, br) in enumerate(bol):
        dueno[((xx - bx) ** 2 + ((yy - by) * 1.15) ** 2) <= br * br] = k
    for k, (bx, by, br) in enumerate(bol):
        mk = dueno == k
        for (y, x) in zip(*np.nonzero(mk)):
            dx, dy = (x - bx) / br, (y - by) * 1.15 / br
            luz = -(dx * 0.7 + dy * 0.8)
            t = luz * 0.55 + 0.5 + (BAYER[y % 4, x % 4] - 0.5) * 0.25
            col = musgo[min(3, max(0, int(t * 3.2)))]
            if math.hypot(dx, dy) > 0.88 and dy > -0.2:
                col = musgo[0]
            s.c[y, x] = hexc(col); s.m[y, x] = True
    # la parte de arriba del techo: pastito plano y florcitas
    for y in range(8, 30):
        for x in range(14, 62):
            q = ((x - 38) / 24.0) ** 2 + ((y - 19) / 10.0) ** 2
            if q < 1:
                col = musgo[2] if (x * 3 + y * 7) % 9 else musgo[3]
                if (x + y * 2) % 13 == 0:
                    col = musgo[1]
                s.c[y, x] = hexc(col); s.m[y, x] = True
    if not inv:
        flores = [0xFFFFFF, 0xF8F0C0] if not oto else [0xE8A23A, 0xC8641E]
        if pri:
            flores = [0xFFFFFF, 0xF59AC0, 0xFFE45A]
        for _ in range(18):
            x, y = rs.randint(16, 60), rs.randint(10, 28)
            if ((x - 38) / 24.0) ** 2 + ((y - 19) / 10.0) ** 2 < 0.9:
                c = flores[rs.randint(0, len(flores))]
                s.p(x, y, c)
    else:
        for _ in range(10):  # carámbanos bajo el alero
            x = rs.randint(4, 72)
            for j in range(rs.randint(2, 4)):
                s.p(x, 41 + j, 0xDDF0FF if j < 2 else 0xB8DCF4)
    # chimenea de piedra (a la derecha, asoma del techo) con humo
    for y in range(2, 16):
        for x in range(52, 60):
            col = [0x9A96A0, 0x7E7A86, 0xB0ACB6][((x // 3) + (y // 2)) % 3]
            if y % 3 == 0 or x in (52, 59):
                col = 0x56525E
            s.p(x, y, col)
    for x in range(51, 61):
        s.p(x, 2, 0x46424E)
    if inv:
        for x in range(52, 60):
            s.p(x, 3, 0xF4FAFF)
    # coronita dorada de veleta (arriba al centro) y banderín azul (a la izquierda)
    for y in range(0, 8):
        s.p(30, y + 2, 0x5A4A3A)
    for (x, y) in ((27, 0), (30, -1), (33, 0)):
        s.p(x, y + 1, 0xFFD24A)
    for x in range(27, 34):
        s.p(x, 2, 0xE0A830); s.p(x, 3, 0xC88A20)
    for y in range(4, 16):
        s.p(14, y, 0x5A4A3A)
    for y in range(4, 9):
        for x in range(15, 15 + (9 - y) + 3):
            s.p(x, y, 0x3A6ACB if (x + y) % 3 else 0x5A8AE0)
    s.p(17, 6, 0xFFD24A)
    # escalones de piedra
    for (y, x0, x1) in ((73, 27, 50), (75, 25, 52), (77, 24, 53)):
        for x in range(x0, x1):
            s.p(x, y, 0xB0ACB6 if (x // 4) % 2 else 0x9A96A0)
            s.p(x, y + 1, 0x6A6672)
        if inv:
            for x in range(x0, x1, 2):
                s.p(x, y, 0xF4FAFF)
    s.contorno(0x24160C)
    return s
