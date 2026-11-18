"""(1.0.16) Los fondos de los JUEGOS DE A DOS (cuando hay visita), dibujados como el vivero y las zonas: pasto con
matitas, árboles de bolitas con luz arriba a la izquierda, contorno oscuro, piedras, flores... Nada copiado de otra
pantalla.

  13_dos_pingpong.png   la cancha de tierra del claro: líneas de tiza, la red de cuerda y totora, bancos y árboles
  14_dos_cuerda.png     el barro junto a la laguna: el charco al medio, huellas, juncos, el agua arriba
  15_dos_memorice.png   el mantel del picnic sobre el pasto: cuadrillé, canasta, tacitas, mermelada, pétalos
  16_dos_escondidas.png el prado de las escondidas: el árbol grande, los arbustos, el tronco hueco, el pastizal y la roca

Uso:   python herramientas/dibujar_juegos_dos.py        -> fondos_pantallas/13..16_dos_*.png (466 x 466)
Luego: python herramientas/hacer_fondos_dos.py          (los empaqueta para la memoria de recursos: firmware/recursos/)

Ojo: los lugares importantes (la cancha, el charco, el mantel, los escondites) están en coordenadas de PANTALLA y son
los mismos que usa firmware/main/ui_dos.inc. Si se mueven aquí, hay que moverlos allá."""
import math
import os
import sys

import numpy as np
from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from pixel_mundo import Lienzo, N, hexc, mezclar, ruido  # noqa: E402

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CARPETA = os.path.join(RAIZ, "fondos_pantallas")
PANT = 466
BORDE = (N * 2 - PANT) // 2

PASTO = (0x3A8236, 0x4A9A40, 0x5EAE4C, 0x84C862)
FLORES = (140, [0xFFFFFF, 0xFFE45A, 0xF59AC0, 0xB9A0FF, 0xF5B06A])
HOJAS = [(0x1A5428, 0x2A7232, 0x40923C, 0x76BE58), (0x1E5A2E, 0x307A3A, 0x4C9C46, 0x86C866), (0x245A24, 0x387A2C, 0x52983A, 0x8CC45A)]
CONT = 0x10301A
TIERRA = (0xC8A672, 0xB89462, 0x9C7A4C, 0x6E5232)


def g(s):
    """de pixel de pantalla a pixel del dibujo"""
    return (s + BORDE) / 2.0


def final(L, oscurecer_bordes=True):
    if oscurecer_bordes:  # un poco más oscuro hacia el borde redondo: el centro (donde se juega) se lee mejor
        d = np.sqrt((L.xx - 128.0) ** 2 + (L.yy - 128.0) ** 2)
        f = np.clip((d - 92) / 40.0, 0, 1) ** 1.4
        L.c[:] = np.clip(L.c * (1.0 - f[..., None] * 0.32), 0, 255).astype(np.int16)
    grande = np.kron(np.clip(L.c, 0, 255).astype(np.uint8), np.ones((2, 2, 1), np.uint8))
    return grande[BORDE:BORDE + PANT, BORDE:BORDE + PANT]


def mezcla_px(L, x, y, col, t):
    x, y = int(round(x)), int(round(y))
    if 0 <= x < N and 0 <= y < N:
        c = L.c[y, x]
        a = (int(c[0]) << 16) | (int(c[1]) << 8) | int(c[2])
        L.p(x, y, mezclar(a, col, t))


def manchon_flores(L, fx, fy, col, n=18, r=6):
    for _ in range(n):
        a = L.u(0, 6.28); d = L.u(0, r)
        x, y = fx + d * math.cos(a), fy + d * math.sin(a) * 0.7
        L.p(x, y, col); L.p(x, y + 1, 0x2E6A2E)


def hongos(L, x, y, col=0xD8443A):
    L.sello(int(x) - 2, int(y) - 2, [".rr.", "rwrr", "kkkk", ".ee."], {"r": col, "w": 0xFFF4E8, "k": 0x3A1A12, "e": 0xF0E2C8})


def bosque_borde(L, sem, dejar=()):
    """árboles grandes alrededor (se ven a medias en el borde redondo)"""
    k = 0
    for ang in range(0, 360, 26):
        if any(abs(((ang - a + 180) % 360) - 180) < w for a, w in dejar):
            continue
        a = math.radians(ang + sem * 7)
        dist = 132 + 8 * math.sin(ang * 0.7 + sem)
        x, y = 128 + dist * math.cos(a), 128 + dist * math.sin(a)
        L.arbol(x, y, 22 + (k * 5) % 7, HOJAS[k % len(HOJAS)], CONT, sem=sem * 31 + k)
        k += 1


# ---------------------------------------------------------------------------------------------------------- ping pong
def pingpong():
    L = Lienzo(sem=161)
    L.pasto(PASTO, sem=21, matas=1200, flores=FLORES, treboles=80)
    bosque_borde(L, 3)
    # la cancha: tierra apisonada (pantalla x 96..370, y 66..404)
    x0, x1, y0, y1 = g(96), g(370), g(66), g(404)
    n2 = ruido(N, 4, 77) * 0.6 + ruido(N, 16, 78) * 0.4
    m = (L.xx >= x0) & (L.xx <= x1) & (L.yy >= y0) & (L.yy <= y1)
    # esquinas un poco redondeadas
    for (cx, cy) in ((x0, y0), (x1, y0), (x0, y1), (x1, y1)):
        m &= ~((np.abs(L.xx - cx) < 3) & (np.abs(L.yy - cy) < 3) & (((L.xx - cx) ** 2 + (L.yy - cy) ** 2) > 4))
    t = n2 + (L.bay - 0.5) * 0.18
    k = np.where(t < 0.36, 2, np.where(t < 0.50, 1, 0))
    pal = np.stack([hexc(0xD2B07C), hexc(0xC4A06C), hexc(0xB08C5A)])
    L.c[m] = pal[k][m]
    borde = np.zeros_like(m)
    borde[1:, :] |= m[:-1, :]; borde[:-1, :] |= m[1:, :]; borde[:, 1:] |= m[:, :-1]; borde[:, :-1] |= m[:, 1:]
    borde &= ~m
    L.c[borde] = hexc(0x2E5A22)
    # pasto que se mete en la orilla de la cancha
    rs = np.random.RandomState(5)
    for _ in range(140):
        side = rs.randint(0, 4)
        if side == 0:
            x, y = rs.uniform(x0, x1), y0 + rs.uniform(0, 2)
        elif side == 1:
            x, y = rs.uniform(x0, x1), y1 - rs.uniform(0, 2)
        elif side == 2:
            x, y = x0 + rs.uniform(0, 2), rs.uniform(y0, y1)
        else:
            x, y = x1 - rs.uniform(0, 2), rs.uniform(y0, y1)
        L.p(x, y, PASTO[1]); L.p(x, y - 1, PASTO[2])
    # piedritas y pisadas en la tierra
    for _ in range(90):
        x, y = rs.uniform(x0 + 3, x1 - 3), rs.uniform(y0 + 3, y1 - 3)
        L.p(x, y, TIERRA[3] if rs.uniform() < 0.5 else mezclar(TIERRA[0], 0xFFFFFF, 0.35))
    for (px, py, esp) in ((g(150), g(330), 1), (g(160), g(318), -1), (g(300), g(150), 1), (g(312), g(138), -1)):
        for dy in (0, 1):
            L.p(px, py + dy, TIERRA[3]); L.p(px + esp, py + dy, TIERRA[3])
    # líneas de tiza (un poco gastadas): el borde, la línea del medio y las de saque
    def tiza(xa, ya, xb, yb):
        n = int(max(abs(xb - xa), abs(yb - ya)) * 2) + 1
        for j in range(n):
            x = xa + (xb - xa) * j / n; y = ya + (yb - ya) * j / n
            if rs.uniform() < 0.88:
                L.p(x, y, 0xF4F0E4 if rs.uniform() < 0.8 else 0xD8D2C2)
    i0, i1, j0, j1 = x0 + 4, x1 - 4, y0 + 4, y1 - 4
    tiza(i0, j0, i1, j0); tiza(i0, j1, i1, j1); tiza(i0, j0, i0, j1); tiza(i1, j0, i1, j1)
    cy = g(235)
    tiza((i0 + i1) / 2, j0, (i0 + i1) / 2, cy - 18); tiza((i0 + i1) / 2, cy + 18, (i0 + i1) / 2, j1)
    tiza(i0, j0 + 26, i1, j0 + 26); tiza(i0, j1 - 26, i1, j1 - 26)
    # la red: postes de madera a los lados y una cuerda con totora tejida
    for px in (x0 - 4, x1 + 4):
        L.sombra_elipse(px + 2, cy + 3, 3, 1.6, 0.6)
        for y in range(int(cy - 6), int(cy + 3)):
            L.p(px, y, 0x7A5232); L.p(px + 1, y, 0x5A3A22)
        L.p(px, cy - 7, 0xA87E50); L.p(px + 1, cy - 7, 0x8A5E3A)
    for x in range(int(x0 - 3), int(x1 + 4)):
        L.p(x, cy - 2, 0xD6B47A if x % 3 else 0xA8864E)          # la cuerda
        if x0 <= x <= x1:
            L.p(x, cy - 1, 0x6A8A3A if x % 2 else 0x4E6E2A)     # la totora tejida colgando
            L.p(x, cy, 0x4E6E2A if x % 2 else 0x3A5A22)
            if x % 4 == 0:
                L.p(x, cy + 1, 0x3A5A22)
            mezcla_px(L, x + 1, cy + 2, 0x000000, 0.25)          # su sombra
    # bancos de tronco a los lados (afuera de la cancha) y una canastita con pelotas de repuesto
    for (bx, by) in ((g(64), g(170)), (g(402), g(300))):
        L.sombra_elipse(bx + 1, by + 4, 5, 2.5, 0.66)
        for y in range(int(by - 3), int(by + 3)):
            for x in range(int(bx - 3), int(bx + 4)):
                d = math.hypot(x - bx, (y - by) * 1.2)
                if d < 3.6:
                    L.p(x, y, 0x3A2414 if d > 2.8 else (0xC8A070 if int(d * 1.5) % 2 else 0xA87E50))
    cx, cyb = g(66), g(318)
    L.sombra_elipse(cx + 1, cyb + 4, 6, 2.4, 0.66)
    for y in range(int(cyb - 3), int(cyb + 4)):
        for x in range(int(cx - 5), int(cx + 6)):
            if abs(x - cx) <= 5 - (1 if y > cyb + 1 else 0):
                L.p(x, y, 0x9A6A3A if (x + y) % 2 else 0x7A5230)
    for (px, py, c) in ((cx - 2, cyb - 4, 0xFFFFFF), (cx + 1, cyb - 4, 0xFFD45C), (cx + 3, cyb - 3, 0xFF8A5A)):
        L.p(px, py, c); L.p(px + 1, py, c); L.p(px, py - 1, mezclar(c, 0xFFFFFF, 0.5))
    # arbustos, piedras, hongos y flores alrededor
    L.arbusto(g(52), g(240), 7, HOJAS[1], CONT, bayas=0xD83A3A, sem=41)
    L.arbusto(g(414), g(196), 7, HOJAS[0], CONT, bayas=0x3A2A8A, sem=42)
    L.arbusto(g(410), g(392), 6, HOJAS[2], CONT, sem=43)
    L.piedra(g(70), g(110), 5, 3.5, musgo=0x5E9A3E)
    L.piedra(g(400), g(110), 4, 3)
    L.piedra(g(56), g(380), 4, 3)
    for (hx, hy) in ((g(420), g(250)), (g(426), g(256)), (g(44), g(150))):
        hongos(L, hx, hy)
    manchon_flores(L, g(60), g(200), 0xFFE45A)
    manchon_flores(L, g(412), g(340), 0xF59AC0)
    manchon_flores(L, g(232), g(440), 0xFFFFFF, n=14)
    manchon_flores(L, g(232), g(30), 0xB9A0FF, n=14)
    return final(L)


# ------------------------------------------------------------------------------------------------------ tira y afloja
def cuerda():
    L = Lienzo(sem=171)
    L.pasto(PASTO, sem=33, matas=1200, flores=FLORES, treboles=80)
    # la laguna arriba (se ve la orilla)
    agua = (0x2E6AB0, 0x3E82C4, 0x5A9ED6, 0x7AB8E6)
    L.poza(128, 18, 120, 36, agua, (0xD8C48E, 0xBCA672, 0x4A3A22), sem=9)
    for (x, y, f) in ((80, 26, 0xF59AC0), (150, 22, 0), (176, 30, 0xFFFFFF), (110, 14, 0)):
        L.nenufar(x, y, 3, f)
    for (x, sem) in ((30, 1), (60, 2), (186, 3), (212, 4), (126, 5)):
        L.juncos(x, 58 + (x % 7) * 0.4, 4, sem=sem)
    # el charco de barro al medio (pantalla 160..306 x 200..300)
    cx, cy, rx, ry = g(233), g(252), 40, 24
    ang = np.arctan2(L.yy - cy, L.xx - cx)
    bor = 1 + 0.09 * np.sin(ang * 3 + 1.2) + 0.05 * np.sin(ang * 7)
    d = np.sqrt(((L.xx - cx) / rx) ** 2 + ((L.yy - cy) / ry) ** 2) / bor
    barro = d < 1.0
    orilla = (d >= 1.0) & (d < 1.14)
    cont = (d >= 1.14) & (d < 1.20)
    L.c[cont] = hexc(0x3A2414)
    L.c[orilla] = hexc(0x8A6A42)
    L.c[orilla & (L.bay > 0.6)] = hexc(0x7A5A36)
    tt = np.clip(1.0 - d, 0, 1) + (L.bay - 0.5) * 0.14 + (ruido(N, 8, 4) - 0.5) * 0.25
    kk = np.clip((tt * 3.6).astype(int), 0, 3)
    pal = np.stack([hexc(0x6A4A2A), hexc(0x5A3C22), hexc(0x4A301A), hexc(0x3E2814)])
    L.c[barro] = pal[kk][barro]
    rs = np.random.RandomState(12)
    for _ in range(26):  # brillos de barro mojado
        x, y = rs.uniform(cx - rx * 0.7, cx + rx * 0.7), rs.uniform(cy - ry * 0.6, cy + ry * 0.6)
        if barro[int(y), int(x)]:
            L.p(x, y, 0x9A7A52); L.p(x + 1, y, 0x8A6A44)
    for (bx, by, r) in ((cx - 14, cy - 4, 2), (cx + 10, cy + 6, 1.5), (cx + 22, cy - 8, 1.5), (cx - 4, cy + 10, 1.2)):  # burbujas
        for a in range(0, 360, 30):
            L.p(bx + r * math.cos(math.radians(a)), by + r * math.sin(math.radians(a)), 0x8A6A44)
        L.p(bx - r * 0.4, by - r * 0.4, 0xB89A6E)
    # salpicaduras de barro en el pasto y huellas
    for _ in range(40):
        a = rs.uniform(0, 6.28); dd = rs.uniform(1.25, 1.7)
        x, y = cx + math.cos(a) * rx * dd, cy + math.sin(a) * ry * dd
        L.p(x, y, 0x5A3C22)
        if rs.uniform() < 0.4:
            L.p(x + 1, y, 0x6A4A2A)
    def huella(x, y, sem):
        for (dx, dy) in ((0, 0), (1, 0), (0, 1), (1, 1), (-1, -2), (2, -2), (0, -3), (1, -3)):
            L.p(x + dx, y + dy, 0x5A3C22)
    for k, (x, y) in enumerate(((g(120), g(286)), (g(134), g(272)), (g(330), g(230)), (g(344), g(246)), (g(300), g(318)), (g(180), g(190)))):
        huella(x, y, k)
    # los lugares donde se paran (tierra pisada) a cada lado
    for sx in (g(108), g(358)):
        m = ((L.xx - sx) / 15.0) ** 2 + ((L.yy - g(268)) / 7.0) ** 2 < 1
        m &= (L.bay < 0.8) | (((L.xx - sx) / 15.0) ** 2 + ((L.yy - g(268)) / 7.0) ** 2 < 0.7)
        L.c[m] = np.where((L.bay[m] > 0.5)[..., None], hexc(0xA88A5E), hexc(0x96784E))
    # estacas de las banderitas que marcan los lados
    for (sx, col) in ((g(176), 0xE84A4A), (g(290), 0x4A8AE8)):
        for y in range(int(g(214)), int(g(226))):
            L.p(sx, y, 0x6A4428)
        for k2 in range(5):
            for j in range(4 - k2 // 2):
                L.p(sx + 1 + j, g(214) + k2, col if j < 3 else mezclar(col, 0x000000, 0.3))
    # abajo: tronco, piedras, arbustos, flores, hongos
    for y in range(int(g(410)), int(g(424))):
        for x in range(int(g(150)), int(g(300))):
            yy = y - g(410)
            col = 0x8A5E3A if yy < 4 else (0x6A4428 if yy < 9 else 0x4A2E1A)
            if int(x - g(150)) % 9 == 0:
                col = 0x4A2E1A
            L.p(x, y, col)
    L.arbol(g(40), g(380), 26, HOJAS[0], CONT, sem=81)
    L.arbol(g(432), g(388), 24, HOJAS[1], CONT, sem=82)
    L.arbol(g(30), g(160), 22, HOJAS[2], CONT, sem=83)
    L.arbol(g(440), g(150), 22, HOJAS[0], CONT, sem=84)
    L.arbusto(g(80), g(330), 6, HOJAS[1], CONT, bayas=0xF59AC0, sem=85)
    L.arbusto(g(392), g(336), 6, HOJAS[2], CONT, bayas=0xD83A3A, sem=86)
    L.piedra(g(70), g(222), 5, 3.5, musgo=0x5E9A3E)
    L.piedra(g(398), g(226), 4.5, 3)
    L.piedra(g(260), g(372), 3.5, 2.5)
    hongos(L, g(110), g(372)); hongos(L, g(116), g(378), 0xE8C88A)
    manchon_flores(L, g(200), g(360), 0xFFE45A)
    manchon_flores(L, g(330), g(380), 0xFFFFFF)
    manchon_flores(L, g(150), g(150), 0xB9A0FF, n=12)
    return final(L)


# ---------------------------------------------------------------------------------------------------------- memorice
def memorice():
    L = Lienzo(sem=181)
    L.pasto(PASTO, sem=45, matas=1300, flores=FLORES, treboles=90)
    bosque_borde(L, 5)
    # el mantel: cuadrillé celeste y blanco (pantalla 66..400 x 86..392), con sombra, bordes con flecos y arrugas
    x0, x1, y0, y1 = g(66), g(400), g(86), g(392)
    L.sombra_elipse((x0 + x1) / 2 + 3, (y0 + y1) / 2 + 4, (x1 - x0) * 0.56, (y1 - y0) * 0.56, 0.78)
    arr = ruido(N, 20, 91)
    for y in range(int(y0), int(y1) + 1):
        for x in range(int(x0), int(x1) + 1):
            cu = 7
            a = (int((x - x0) // cu) % 2) == 0
            b = (int((y - y0) // cu) % 2) == 0
            if a and b:
                col = 0x7AB0E0
            elif a or b:
                col = 0xB8D6F2
            else:
                col = 0xF6F8FC
            sh = (arr[y, x] - 0.5) * 0.35   # arrugas suaves
            if sh > 0.06:
                col = mezclar(col, 0xFFFFFF, min(0.35, sh))
            elif sh < -0.06:
                col = mezclar(col, 0x2A4A7A, min(0.25, -sh))
            if x in (int(x0), int(x1)) or y in (int(y0), int(y1)):
                col = 0x3A5A8A
            L.p(x, y, col)
    for x in range(int(x0) + 1, int(x1), 2):   # flecos arriba y abajo
        L.p(x, y0 - 1, 0xE8F0FA); L.p(x, y1 + 1, 0xE8F0FA)
        if x % 4 == 1:
            L.p(x, y0 - 2, 0xC8D8EE); L.p(x, y1 + 2, 0xC8D8EE)
    # la canasta (abajo a la izquierda, sobre el mantel y el pasto)
    bx, by = g(70), g(398)
    L.sombra_elipse(bx + 2, by + 5, 14, 5, 0.66)
    for y in range(int(by - 8), int(by + 6)):
        for x in range(int(bx - 13), int(bx + 14)):
            if abs(x - bx) <= 13 - max(0, y - by - 2):
                col = 0xB07A3E if ((x + y) // 2) % 2 else 0x8A5A2A
                if y == int(by - 8):
                    col = 0x6A4220
                L.p(x, y, col)
    for a in range(0, 181, 6):   # el asa
        t = math.radians(a)
        L.p(bx + 11 * math.cos(t), by - 8 - 9 * math.sin(t), 0x6A4220)
        L.p(bx + 11 * math.cos(t), by - 9 - 9 * math.sin(t), 0x9A6A34)
    for (px, py, c) in ((bx - 6, by - 10, 0xE8402A), (bx - 2, by - 11, 0xFFE45A), (bx + 3, by - 10, 0x6ABE4A), (bx + 7, by - 9, 0xE8402A)):
        for dy in range(2):
            for dx in range(3):
                L.p(px + dx, py + dy, c)
        L.p(px, py, mezclar(c, 0xFFFFFF, 0.5))
    # tacitas y tetera (arriba a la derecha)
    def taza(x, y, col):
        L.sombra_elipse(x + 1, y + 2, 4.5, 2.2, 0.7)
        for yy in range(int(y - 3), int(y + 3)):
            for xx in range(int(x - 4), int(x + 5)):
                dd = ((xx - x) / 4.2) ** 2 + ((yy - y) / 3.0) ** 2
                if dd <= 1:
                    L.p(xx, yy, 0x8A4A2A if dd < 0.45 else col)
                elif dd < 1.4:
                    L.p(xx, yy, mezclar(col, 0x000000, 0.4))
        L.p(x + 5, y, col); L.p(x + 6, y, col); L.p(x + 6, y + 1, col)
    taza(g(380), g(110), 0xF4F0E8)
    taza(g(352), g(96), 0xF5C0D0)
    tx, ty = g(404), g(150)
    L.sombra_elipse(tx + 1, ty + 4, 8, 3, 0.7)
    for yy in range(int(ty - 6), int(ty + 5)):
        for xx in range(int(tx - 7), int(tx + 8)):
            dd = ((xx - tx) / 7.0) ** 2 + ((yy - ty) / 5.5) ** 2
            if dd <= 1:
                L.p(xx, yy, 0xF8F0E0 if (xx - tx) + (yy - ty) < -2 else (0xE8D8C0 if dd < 0.8 else 0xC8B49A))
    L.p(tx - 8, ty - 1, 0xE8D8C0); L.p(tx - 9, ty - 2, 0xE8D8C0); L.p(tx, ty - 6, 0x8A5A3A)
    # el frasco de mermelada (de murta) y una cuchara
    jx, jy = g(392), g(370)
    L.sombra_elipse(jx + 1, jy + 4, 4.5, 2, 0.7)
    for yy in range(int(jy - 5), int(jy + 4)):
        for xx in range(int(jx - 3), int(jx + 4)):
            L.p(xx, yy, 0xB02A3A if yy > jy - 3 else 0xF4E8D8)
    for xx in range(int(jx - 3), int(jx + 4)):
        L.p(xx, jy - 6, 0xE84A4A if xx % 2 else 0xFFFFFF)
    for k in range(9):
        L.p(jx + 6 + k, jy + 2 - k * 0.3, 0xC8C8D4)
    L.p(jx + 15, jy - 1, 0xA8A8B4); L.p(jx + 16, jy - 1, 0xC8C8D4)
    # pétalos y migas sobre el mantel (pocos: ahí van las cartas), una chinita en la orilla
    rs = np.random.RandomState(4)
    for _ in range(14):
        x, y = rs.uniform(x0 + 2, x1 - 2), rs.choice([rs.uniform(y0 + 1, y0 + 8), rs.uniform(y1 - 8, y1 - 1)])
        L.p(x, y, [0xF59AC0, 0xFFFFFF, 0xFFE45A][rs.randint(0, 3)])
    for _ in range(10):
        x, y = rs.uniform(x0 + 2, x1 - 2), rs.choice([rs.uniform(y0 + 1, y0 + 8), rs.uniform(y1 - 8, y1 - 1)])
        L.p(x, y, 0xD8A86A)
    chx, chy = g(110), g(92)
    for yy in range(int(chy - 2), int(chy + 2)):
        for xx in range(int(chx - 2), int(chx + 3)):
            if ((xx - chx) / 2.4) ** 2 + ((yy - chy) / 1.8) ** 2 <= 1:
                L.p(xx, yy, 0xE8302A)
    L.p(chx - 2, chy, 0x1A1A1A); L.p(chx, chy - 1, 0x1A1A1A); L.p(chx + 1, chy + 1, 0x1A1A1A); L.p(chx - 3, chy, 0x1A1A1A)
    manchon_flores(L, g(30), g(240), 0xFFE45A, n=12)
    manchon_flores(L, g(436), g(250), 0xF59AC0, n=12)
    manchon_flores(L, g(233), g(434), 0xFFFFFF, n=12)
    return final(L)


# --------------------------------------------------------------------------------------------------------- escondidas
# los escondites, en coordenadas de pantalla (los mismos que ui_dos.inc: DOS_ESC_XY)
ESCONDITES = {"arbol": (118, 150), "arbustos": (356, 168), "roca": (238, 108), "tronco": (132, 330), "pastizal": (338, 334)}


def escondidas():
    L = Lienzo(sem=191)
    L.pasto(PASTO, sem=57, matas=1300, flores=FLORES, treboles=90)
    bosque_borde(L, 8, dejar=((240, 20),))
    # un senderito que cruza el prado
    L.camino([(-4, 196), (40, 182), (96, 176), (150, 170), (200, 164), (262, 158)], 4.0, (0xDCC08A, 0xC8A672, 0xA8865A, 0x7A5A3A), sem=8)
    # el árbol grande (con huequito en el tronco)
    ax, ay = g(ESCONDITES["arbol"][0]), g(ESCONDITES["arbol"][1])
    L.arbol(ax, ay - 6, 26, HOJAS[0], CONT, sem=91, motas=(0xF8C0D8, 0.05))
    # los arbustos (tres juntos)
    bx, by = g(ESCONDITES["arbustos"][0]), g(ESCONDITES["arbustos"][1])
    L.arbusto(bx - 9, by + 2, 9, HOJAS[1], CONT, bayas=0xD83A3A, sem=92)
    L.arbusto(bx + 9, by + 3, 9, HOJAS[2], CONT, bayas=0xD83A3A, sem=93)
    L.arbusto(bx, by - 4, 10, HOJAS[0], CONT, bayas=0xD83A3A, sem=94)
    # la roca grande con musgo
    rx, ry = g(ESCONDITES["roca"][0]), g(ESCONDITES["roca"][1])
    L.piedra(rx, ry, 14, 9, musgo=0x5E9A3E)
    L.piedra(rx + 14, ry + 6, 4, 3)
    # el tronco hueco (acostado), con el hueco oscuro mirando a la derecha
    tx, ty = g(ESCONDITES["tronco"][0]), g(ESCONDITES["tronco"][1])
    L.sombra_elipse(tx + 2, ty + 6, 22, 4, 0.66)
    for y in range(int(ty - 6), int(ty + 6)):
        for x in range(int(tx - 20), int(tx + 18)):
            yy = y - (ty - 6)
            col = 0x9A6A40 if yy < 3 else (0x7A5232 if yy < 7 else (0x5A3A22 if yy < 10 else 0x3A2414))
            if (int(x - tx) % 6 == 0 and yy > 1) or yy == 0:
                col = 0x4A2E1A if yy else 0x3A2414
            L.p(x, y, col)
    for y in range(int(ty - 7), int(ty + 7)):   # la boca del tronco
        for x in range(int(tx + 14), int(tx + 22)):
            d = math.hypot((x - (tx + 18)) * 1.4, y - ty)
            if d < 7:
                L.p(x, y, 0x1A0E06 if d < 4.4 else (0xC8A070 if int(d) % 2 else 0xA87E50))
            elif d < 8:
                L.p(x, y, 0x3A2414)
    for (mx, my) in ((tx - 12, ty - 7), (tx - 4, ty - 7), (tx + 6, ty - 7)):   # musgo y un brote
        L.p(mx, my, 0x6CC050); L.p(mx + 1, my, 0x4A9A3E)
    hongos(L, tx - 18, ty + 7, 0xE8C88A)
    # el pastizal alto (matas largas en tres verdes, con espigas)
    px, py = g(ESCONDITES["pastizal"][0]), g(ESCONDITES["pastizal"][1])
    rs = np.random.RandomState(31)
    L.sombra_elipse(px + 1, py + 5, 18, 6, 0.72)
    for _ in range(320):
        x = px + rs.uniform(-20, 20); base = py + rs.uniform(-4, 9)
        if ((x - px) / 21.0) ** 2 + ((base - py - 2) / 10.0) ** 2 > 1:
            continue
        h = rs.randint(8, 17)
        inc = rs.uniform(-0.35, 0.35)
        col = [0x1E5A28, 0x2A7232, 0x40923C, 0x5EA848][rs.randint(0, 4)]
        for j in range(h):
            L.p(x + inc * j, base - j, col if j < h - 2 else mezclar(col, 0xFFFFFF, 0.25))
        if rs.uniform() < 0.18:
            L.p(x + inc * h, base - h, 0xD8C07A); L.p(x + inc * h, base - h - 1, 0xC8A85A)
    # flores, piedritas, hongos sueltos
    L.piedra(g(70), g(250), 4, 3)
    L.piedra(g(398), g(262), 4, 3, musgo=0x5E9A3E)
    hongos(L, g(232), g(392)); hongos(L, g(240), g(398), 0xE8C88A)
    manchon_flores(L, g(232), g(250), 0xFFE45A, n=10, r=5)
    manchon_flores(L, g(176), g(240), 0xF59AC0, n=12)
    manchon_flores(L, g(300), g(240), 0xFFFFFF, n=12)
    manchon_flores(L, g(232), g(330), 0xB9A0FF, n=10)
    return final(L)


if __name__ == "__main__":
    for nombre, f in (("13_dos_pingpong.png", pingpong), ("14_dos_cuerda.png", cuerda), ("15_dos_memorice.png", memorice),
                      ("16_dos_escondidas.png", escondidas)):
        Image.fromarray(f()).save(os.path.join(CARPETA, nombre))
        print("listo:", nombre)
