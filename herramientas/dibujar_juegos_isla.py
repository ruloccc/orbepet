"""(1.0.18) Los fondos de los JUEGOS DE LA ISLA, con el mismo pincel de pixel art que los de los juegos de a dos
(contorno oscuro, luz arriba a la izquierda, tramas suaves). Nada copiado de otra pantalla.

  17_isla_faro.png    el mar de noche visto desde la isla: cielo con estrellas y luna, el faro a la izquierda sobre
                      sus rocas, las rocas del mar con espuma y el muelle con su farol a la derecha
  18_isla_buceo.png   bajo el agua: rayos de luz, la superficie arriba, arena con ondas abajo, corales, algas largas,
                      rocas con musgo, un ancla y un cofre medio enterrados

Uso:   python herramientas/dibujar_juegos_isla.py   -> fondos_pantallas/17_isla_faro.png y 18_isla_buceo.png
Luego: python herramientas/hacer_fondos_dos.py      (los empaqueta: pj4.bin y pj5.bin)

Ojo: el faro (la lámpara), las rocas y el muelle están en coordenadas de PANTALLA y son los mismos que usa
firmware/main/ui_isla.inc. Si se mueven aquí, hay que moverlos allá."""
import math
import os
import sys

import numpy as np
from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from pixel_mundo import Lienzo, N, hexc, mezclar, ruido  # noqa: E402
from dibujar_juegos_dos import g, final, mezcla_px, CARPETA  # noqa: E402

# --- lo que comparte con ui_isla.inc (pantalla) ---
FARO_LAMPARA = (96, 168)          # el centro de la lámpara (de ahí sale el haz)
FARO_BASE = (96, 330)             # el pie de la torre
ROCAS = [(196, 200, 15), (300, 182, 13), (236, 290, 17), (372, 262, 13), (170, 372, 13)]   # x, y, radio
MUELLE = (344, 356)               # la punta del muelle: ahí llegan los barquitos
HORIZONTE = 122


def gradiente_vertical(L, y0, y1, c0, c1, mask=None):
    for y in range(int(y0), int(y1)):
        t = (y - y0) / max(1.0, (y1 - y0))
        col = hexc(mezclar(c0, c1, t))
        if mask is None:
            L.c[y, :] = col
        else:
            L.c[y, mask[y]] = col


def disco(L, cx, cy, r, col, borde=None, luz=None):
    for y in range(int(cy - r - 2), int(cy + r + 3)):
        for x in range(int(cx - r - 2), int(cx + r + 3)):
            d = math.hypot(x - cx, y - cy)
            if d <= r:
                c = col
                if luz is not None and (x - cx) + (y - cy) < -r * 0.5:
                    c = luz
                L.p(x, y, c)
            elif borde is not None and d <= r + 1:
                L.p(x, y, borde)


def halo(L, cx, cy, r, col, fuerza=0.5):
    for y in range(int(cy - r), int(cy + r + 1)):
        for x in range(int(cx - r), int(cx + r + 1)):
            d = math.hypot(x - cx, y - cy) / r
            if d < 1:
                mezcla_px(L, x, y, col, fuerza * (1 - d) ** 2)


def espuma(L, cx, cy, rx, ry, sem):
    rs = np.random.RandomState(sem)
    for k in range(int(rx * 7)):
        a = rs.uniform(0, 6.283)
        q = rs.uniform(1.0, 1.35)
        x, y = cx + math.cos(a) * rx * q, cy + math.sin(a) * ry * q
        mezcla_px(L, x, y, 0xE8F4FF, 0.55 if q < 1.15 else 0.3)


def faro_isla(L, lx, ly, bx, by):
    """el MISMO faro de la isla (fondos_zonas/isla_*.png), de noche: torre de piedra clara con dos anillos de ladrillo,
    puerta de madera con arco, ventanitas encendidas, cornisa de ladrillo con almenas, balcón de baranda oscura, el cuarto
    de vidrio (aquí con la lámpara encendida), la cúpula verde con su pomo y la banderita rosada y verde, y las
    plataformas de madera a los lados con sus palitos"""
    sombra = 0.82  # de noche: todo un poco más oscuro y azulado
    def c(h):
        return mezclar(mezclar(h, 0x000000, 1 - sombra), 0x1A2A50, 0.12)
    top = ly + 9            # donde termina el vidrio y empieza la cornisa
    pie = by - 4
    for y in range(int(top), int(pie)):
        t = (y - top) / max(1.0, (pie - top))
        w = 7 + 3.2 * t      # se ensancha hacia abajo
        anillo = (0.55 < t < 0.66) or t > 0.86 or t < 0.06
        fila = int((y - top) / 3)
        for x in range(int(bx - w), int(bx + w) + 1):
            d = (x - (bx - w)) / (2 * w)  # 0 izquierda (luz de la luna) .. 1 derecha (sombra)
            if anillo:
                base = 0xB8643A if d < 0.3 else (0x9A4E2C if d < 0.75 else 0x6E3218)
                if (y - top) % 3 == 0:
                    base = 0x5A2A14
            else:
                base = 0xE6DCC8 if d < 0.3 else (0xCFC3AC if d < 0.72 else 0xA89A82)
                if (y - top) % 3 == 0 or (int(x - bx + fila * 2) % 5 == 0):
                    base = 0x8A7E6A  # la junta de las piedras
            L.p(x, y, c(base))
        L.p(bx - w - 1, y, 0x14101C); L.p(bx + w + 1, y, 0x14101C)
    # la cornisa de ladrillo con almenas y el balcón con baranda
    for y in range(int(top - 1), int(top + 4)):
        for x in range(int(bx - 9), int(bx + 10)):
            col = 0x9A4E2C if y > top else 0x6E3218
            if y == int(top - 1) and (x % 3 == 0):
                col = 0x14101C
            L.p(x, y, c(col))
    for x in range(int(bx - 10), int(bx + 11)):
        L.p(x, top - 2, 0x14101C)
        if x % 2 == 0:
            L.p(x, top - 3, 0x2A2630); L.p(x, top - 4, 0x2A2630)
        L.p(x, top - 5, 0x2A2630)
    # el cuarto de vidrio: la lámpara encendida
    halo(L, lx, ly - 1, 30, 0xFFE6A0, 0.55)
    for y in range(int(ly - 6), int(top - 4)):
        for x in range(int(lx - 6), int(lx + 7)):
            col = 0xFFF6C8 if abs(x - lx) < 3 else 0xFFD27A
            if (x - int(lx - 6)) % 4 == 0:
                col = 0x2A2E3A
            L.p(x, y, col)
    # la cúpula verde (con luz a la izquierda), el pomo, el palito y la banderita rosada con verde
    for y in range(int(ly - 13), int(ly - 6)):
        q = (y - (ly - 13)) / 7.0
        w = 3 + 5 * math.sqrt(max(0.0, q))
        for x in range(int(lx - w), int(lx + w) + 1):
            d = (x - (lx - w)) / (2 * w)
            col = 0x7A9A80 if d < 0.3 else (0x5A7A62 if d < 0.7 else 0x3E5A48)
            L.p(x, y, c(col))
        L.p(lx - w - 1, y, 0x14101C); L.p(lx + w + 1, y, 0x14101C)
    for x in range(int(lx - 9), int(lx + 10)):
        L.p(x, ly - 6, 0x2A3A30)
    L.p(lx, ly - 14, c(0x5A7A62)); L.p(lx - 1, ly - 14, c(0x5A7A62)); L.p(lx, ly - 15, 0x14101C)
    for y in range(int(ly - 24), int(ly - 15)):
        L.p(lx, y, 0x3A3440)
    for y in range(int(ly - 24), int(ly - 20)):
        for x in range(int(lx + 1), int(lx + 6)):
            L.p(x, y, c(0xE07AA0) if x < lx + 3 else c(0x6ABE7A))
    # la puerta de madera con arco (abajo) y dos ventanitas encendidas
    for y in range(int(pie - 7), int(pie)):
        for x in range(int(bx - 2), int(bx + 3)):
            if y == int(pie - 7) and x in (int(bx - 2), int(bx + 2)):
                continue
            col = 0x7A4E28 if (x - int(bx)) % 2 else 0x5A3418
            L.p(x, y, c(col))
    L.p(bx + 1, pie - 3, 0xE8C84A)
    for (wy) in (top + 14, top + 34):
        for y in range(int(wy), int(wy + 4)):
            L.p(bx - 1, y, 0xFFD87A); L.p(bx, y, 0xFFC04A)
        L.p(bx - 1, wy - 1, 0x5A2A14); L.p(bx, wy - 1, 0x5A2A14)
    # las plataformas de madera a los lados (con barandita y un palito con banderín)
    for (lado, yy) in ((-1, top + 20), (1, top + 34), (-1, top + 50)):
        t = (yy - top) / max(1.0, (pie - top))
        w = 7 + 3.2 * t
        x0 = bx + lado * (w + 1)
        for k in range(7):
            x = x0 + lado * k
            L.p(x, yy, c(0xA06A3A)); L.p(x, yy + 1, c(0x6E4422)); L.p(x, yy + 2, 0x2A1A10)
            if k % 3 == 0:
                L.p(x, yy - 1, c(0x6E4422)); L.p(x, yy - 2, c(0x6E4422))
        L.p(x0 + lado * 6, yy - 3, c(0x6E4422))
        for y in range(int(yy - 9), int(yy - 2)):
            L.p(x0 + lado * 6, y, 0x3A2414)
        L.p(x0 + lado * 7, yy - 9, c(0xE07AA0)); L.p(x0 + lado * 7, yy - 8, c(0xE07AA0)); L.p(x0 + lado * 8, yy - 8, c(0xE07AA0))



# --------------------------------------------------------------------------------------------------------------- faro
def faro():
    L = Lienzo(sem=171)
    hy = g(HORIZONTE)
    # el cielo de noche (degradado con trama) y el mar
    for y in range(N):
        if y < hy:
            t = y / hy
            base = mezclar(0x081028, 0x22356A, t ** 1.5)
        else:
            t = (y - hy) / (N - hy)
            base = mezclar(0x1A3C6E, 0x0A1E3C, t ** 0.8)
        L.c[y, :] = hexc(base)
    tr = (L.bay - 0.5)
    cielo = L.yy < hy
    n1 = ruido(N, 30, 41)
    L.c[cielo & (n1 + tr * 0.1 > 0.62)] = (L.c[cielo & (n1 + tr * 0.1 > 0.62)] * 1.12).clip(0, 255).astype(np.int16)  # vía láctea suave
    rs = np.random.RandomState(7)
    for _ in range(170):  # estrellas
        x, y = rs.uniform(0, N), rs.uniform(0, hy - 3)
        c = [0xFFFFFF, 0xFFF2C0, 0xC8DCFF, 0xFFE0E8][rs.randint(0, 4)]
        mezcla_px(L, x, y, c, rs.uniform(0.35, 1.0))
    for (x, y) in ((g(150), g(40)), (g(250), g(70)), (g(205), g(26)), (g(392), g(96))):  # unas grandes, en cruz
        L.p(x, y, 0xFFFFFF)
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            mezcla_px(L, x + dx, y + dy, 0xFFF4D0, 0.6)
    # la luna (con cráteres y halo) y su reflejo en el agua
    mx, my = g(330), g(58)
    halo(L, mx, my, 22, 0xC8D8FF, 0.35)
    disco(L, mx, my, 9, 0xF4ECC8, borde=0xB8B090, luz=0xFFFBEA)
    for (dx, dy, r) in ((-3, -1, 2), (2, 3, 1.5), (3, -3, 1.2)):
        disco(L, mx + dx, my + dy, r, 0xDCD2A8)
    # nubes finitas
    for (cx, cy, w) in ((g(120), g(80), 26), (g(270), g(100), 30), (g(410), g(60), 20)):
        for k in range(int(w)):
            y = cy + math.sin(k * 0.4) * 1.2
            mezcla_px(L, cx - w / 2 + k, y, 0x3A4C7E, 0.7)
            mezcla_px(L, cx - w / 2 + k, y + 1, 0x2A3A66, 0.5)
    # la costa lejana con lucecitas (otra orilla del lago)
    for x in range(int(g(130)), int(g(290))):
        h = 2 + 2 * math.sin(x * 0.21) + 1.5 * math.sin(x * 0.07 + 1)
        for y in range(int(hy - h), int(hy)):
            L.p(x, y, 0x101C34)
    for x in (g(160), g(176), g(232), g(262)):
        L.p(x, hy - 1.5, 0xFFD87A)
    L.c[int(hy), :] = hexc(0x2C4E80)
    # el mar: olas en líneas cortas (más chicas lejos), y el camino de la luna
    rs = np.random.RandomState(11)
    for _ in range(700):
        y = rs.uniform(hy + 2, N)
        t = (y - hy) / (N - hy)
        x = rs.uniform(0, N)
        largo = 1 + int(t * 5)
        col = mezclar(0x2E5E96, 0x3E78B0, rs.uniform())
        for k in range(largo):
            mezcla_px(L, x + k, y, col, 0.55)
        if rs.uniform() < 0.3:
            mezcla_px(L, x + largo // 2, y - 1, 0x5A90C8, 0.4)
    for _ in range(160):  # el reflejo de la luna: trazos que tiemblan
        y = rs.uniform(hy + 1, g(300))
        t = (y - hy) / (g(300) - hy)
        x = mx + rs.normal(0, 2 + t * 7)
        for k in range(1 + int(t * 4)):
            mezcla_px(L, x + k, y, 0xF4ECC8, 0.65 * (1 - t * 0.7))
    # las rocas del mar, con musgo y espuma alrededor
    for k, (x, y, r) in enumerate(ROCAS):
        cx, cy, rr = g(x), g(y), r / 2.0
        espuma(L, cx, cy + rr * 0.3, rr * 1.25, rr * 0.7, 30 + k)
        L.piedra(cx, cy, rr, rr * 0.78, cols=(0x2A2E3E, 0x444A60, 0x626A84, 0x8890AA), contorno=0x101420, musgo=0x3E6A4A)
        for j in range(3):
            mezcla_px(L, cx - rr + j * rr * 0.9, cy + rr * 0.75, 0xE8F4FF, 0.7)
    # el muelle (a la derecha): tablones, pilotes y el farol encendido
    px0, py = g(MUELLE[0] - 16), g(MUELLE[1])
    for x in range(int(px0), N):
        for y in range(int(py - 4), int(py + 4)):
            yy = y - (py - 4)
            col = 0x8A5E36 if yy < 2 else (0x6E4A2A if yy < 6 else 0x4A3018)
            if (x - int(px0)) % 5 == 0:
                col = 0x3A2414
            L.p(x, y, col)
        if (x - int(px0)) % 10 == 2:
            for y in range(int(py + 4), int(py + 10)):
                L.p(x, y, 0x3A2414); L.p(x + 1, y, 0x5A3A22)
            mezcla_px(L, x, py + 10, 0xE8F4FF, 0.5); mezcla_px(L, x + 1, py + 10, 0xE8F4FF, 0.5)
    for y in range(int(py - 14), int(py - 3)):  # el farol del muelle
        L.p(px0 + 2, y, 0x2A1A10)
    halo(L, px0 + 2, py - 16, 9, 0xFFD87A, 0.55)
    disco(L, px0 + 2, py - 16, 2, 0xFFE9A0, borde=0x5A3A1A)
    # el salvavidas y una cuerda
    disco(L, px0 + 14, py - 6, 2.4, 0xE8483C, borde=0x6A1A14)
    L.p(px0 + 14, py - 6, 0x1A3C6E)
    for (dx, dy) in ((-2.4, 0), (2.4, 0), (0, -2.4), (0, 2.4)):
        L.p(px0 + 14 + dx, py - 6 + dy, 0xF4F0E8)
    # el faro: rocas de la base, torre a rayas que se angosta, balcón, la lámpara con su resplandor y el techo
    bx, by = g(FARO_BASE[0]), g(FARO_BASE[1])
    espuma(L, bx, by + 4, 18, 7, 77)
    for (dx, dy, rx, ry) in ((-10, 4, 9, 6), (9, 5, 8, 5), (0, 8, 12, 6), (-4, -2, 9, 6), (6, -1, 7, 5)):
        L.piedra(bx + dx, by + dy, rx, ry, cols=(0x2A2E3E, 0x444A60, 0x626A84, 0x8890AA), contorno=0x101420, musgo=0x3E6A4A)
    faro_isla(L, g(FARO_LAMPARA[0]), g(FARO_LAMPARA[1]), bx, by)
    # unas boyas y peces que saltan (brillitos)
    for (x, y, c) in ((g(150), g(250), 0xE8483C), (g(330), g(220), 0xF4F0E8), (g(260), g(370), 0xE8483C)):
        disco(L, x, y, 1.6, c, borde=0x101420)
        mezcla_px(L, x - 2, y + 2, 0xE8F4FF, 0.5); mezcla_px(L, x + 2, y + 2, 0xE8F4FF, 0.5)
    return final(L)


# -------------------------------------------------------------------------------------------------------------- buceo
def buceo():
    L = Lienzo(sem=181)
    # el agua: clara arriba, profunda abajo, con trama
    for y in range(N):
        t = y / N
        L.c[y, :] = hexc(mezclar(0x3CA6D4, 0x0C3466, t ** 0.9))
    n1 = ruido(N, 24, 51)
    m = n1 + (L.bay - 0.5) * 0.12 > 0.6
    L.c[m] = (L.c[m] * 1.07).clip(0, 255).astype(np.int16)
    # la superficie: banda que ondula con brillos
    sy = g(36)
    for x in range(N):
        yy = sy + 1.6 * math.sin(x * 0.19) + 0.8 * math.sin(x * 0.47 + 2)
        for y in range(0, int(yy)):
            L.p(x, y, mezclar(0x8ED8F0, 0x5EC0E4, y / max(1, yy)))
        L.p(x, yy, 0xD8F4FF)
        if x % 7 < 3:
            L.p(x, yy + 1, 0xA8E4F8)
    # rayos de luz en diagonal
    for k, x0 in enumerate((20, 60, 96, 140, 178, 214)):
        ancho = 5 + (k % 3) * 2
        for y in range(int(sy), int(g(330))):
            t = (y - sy) / (g(330) - sy)
            xc = x0 + y * 0.32
            for x in range(int(xc - ancho), int(xc + ancho)):
                f = 1 - abs(x - xc) / ancho
                mezcla_px(L, x, y, 0xC8F0FF, 0.16 * f * (1 - t))
    # el arrecife lejano (siluetas azul oscuro detrás de la arena) y las paredes de roca a los lados
    for x in range(N):
        h = 10 + 6 * math.sin(x * 0.06 + 1) + 4 * math.sin(x * 0.17) + 3 * math.sin(x * 0.41)
        for y in range(int(g(360) - h), int(g(380))):
            mezcla_px(L, x, y, 0x0E3A62, 0.55)
    def pared(lado):
        rs2 = np.random.RandomState(70 + lado)
        for y in range(int(g(150)), N):
            t = max(0.0, (y - g(150)) / (N - g(150)))
            w = 10 + 26 * t ** 0.8 + 3 * math.sin(y * 0.3) + 2 * math.sin(y * 0.11 + lado)
            for j in range(int(w) + 2):
                x = j if lado == 0 else N - 1 - j
                if j > w:
                    L.p(x, y, 0x081624)
                    continue
                d = j / max(1.0, w)
                col = 0x24405C if d > 0.75 else (0x1C3450 if d > 0.4 else 0x14283E)
                if (int(y / 4) + int(j / 4) + lado) % 3 == 0 and d > 0.3:
                    col = mezclar(col, 0x000000, 0.25)
                if d > 0.85 and (y + j) % 3 == 0:
                    col = 0x34587A
                L.p(x, y, col)
        for _ in range(45):  # corales y anémonas pegados a la pared
            y = rs2.uniform(g(190), g(420))
            t = max(0.0, (y - g(150)) / (N - g(150)))
            w = 10 + 26 * t ** 0.8
            x = w - rs2.uniform(0, 3) if lado == 0 else N - 1 - w + rs2.uniform(0, 3)
            c = [0xF06A8A, 0xFFA05A, 0xD86AD0, 0xF4D05A, 0x6AD8C0][rs2.randint(0, 5)]
            for a2 in range(0, 360, 60):
                r2 = rs2.uniform(1, 2.5)
                mezcla_px(L, x + r2 * math.cos(math.radians(a2)), y + r2 * math.sin(math.radians(a2)), c, 0.85)
            L.p(x, y, mezclar(c, 0xFFFFFF, 0.4))
    pared(0); pared(1)
    # la arena del fondo con dunas, ondas y conchitas
    arena = np.zeros((N, N), bool)
    for x in range(N):
        yb = g(372) + 4 * math.sin(x * 0.045 + 0.5) + 2 * math.sin(x * 0.13)
        arena[int(yb):, x] = True
    n2 = ruido(N, 10, 61)
    k = np.where(n2 + (L.bay - 0.5) * 0.2 < 0.42, 2, np.where(n2 < 0.56, 1, 0))
    pal = np.stack([hexc(0xE0C890), hexc(0xC8AE76), hexc(0xA88E5C)])
    L.c[arena] = pal[k][arena]
    borde = arena & ~np.roll(arena, 1, axis=0)
    L.c[borde] = hexc(0xF0DCA8)
    for y in range(0, N, 5):  # ondas en la arena
        for x in range(N):
            if arena[y, x] and (x + y * 3) % 11 < 5:
                mezcla_px(L, x, y + math.sin(x * 0.3), 0x9A7E50, 0.45)
    rs = np.random.RandomState(9)
    ys, xs = np.nonzero(arena)
    for _ in range(40):
        q = rs.randint(0, len(ys))
        c = [0xF4A0B0, 0xFFF0E0, 0xF0C080][rs.randint(0, 3)]
        L.p(xs[q], ys[q], c); L.p(xs[q] + 1, ys[q], mezclar(c, 0x000000, 0.25))
    # algas largas a los lados (ondulan)
    for (x0, alto, col, sem) in ((g(80), 74, 0x2E8A4A, 1), (g(96), 96, 0x3AA05A, 2), (g(112), 60, 0x2E8A4A, 3), (g(196), 34, 0x46B266, 7),
                                 (g(356), 88, 0x3AA05A, 4), (g(372), 66, 0x2E8A4A, 5), (g(388), 52, 0x46B266, 6), (g(276), 30, 0x3AA05A, 8)):
        base = g(392)
        for j in range(alto):
            x = x0 + 3 * math.sin(j * 0.12 + sem)
            y = base - j
            L.p(x - 1, y, mezclar(col, 0xFFFFFF, 0.2)); L.p(x, y, col); L.p(x + 1, y, mezclar(col, 0x000000, 0.35))
            if j % 7 == 3:  # hojas
                for q in range(4):
                    L.p(x + 2 + q, y - q * 0.6, col if q < 3 else mezclar(col, 0xFFFFFF, 0.3))
                    L.p(x + 2 + q, y + 1 - q * 0.6, mezclar(col, 0x000000, 0.3))
            if j % 7 == 0:
                for q in range(4):
                    L.p(x - 2 - q, y - q * 0.6, col if q < 3 else mezclar(col, 0xFFFFFF, 0.3))
                    L.p(x - 2 - q, y + 1 - q * 0.6, mezclar(col, 0x000000, 0.3))
            if j == alto - 1:
                disco(L, x, y - 1, 1.5, 0xF4D05A)  # un flotador de alga
    # corales: ramitas rosadas y naranjas, y uno de cerebro
    def coral(cx, cy, col, sem):
        r = np.random.RandomState(sem)
        for _ in range(5):
            x, y, a = cx + r.uniform(-3, 3), cy, -math.pi / 2 + r.uniform(-0.7, 0.7)
            for j in range(r.randint(8, 15)):
                x += math.cos(a) * 0.9; y += math.sin(a) * 0.9
                a += r.uniform(-0.25, 0.25)
                L.p(x, y, col); L.p(x + 1, y, mezclar(col, 0x000000, 0.3))
                if j % 5 == 4:
                    L.p(x - 1, y - 1, mezclar(col, 0xFFFFFF, 0.35))
    coral(g(120), g(390), 0xF06A8A, 3); coral(g(150), g(398), 0xFFA05A, 4)
    coral(g(318), g(394), 0xFF8A6A, 5); coral(g(342), g(388), 0xD86AD0, 6)
    disco(L, g(232), g(410), 6, 0xE8A0C8, borde=0x6A2A50, luz=0xFFC8E0)
    for a in range(0, 360, 40):
        mezcla_px(L, g(232) + 3 * math.cos(math.radians(a)), g(410) + 3 * math.sin(math.radians(a)), 0x8A3A6A, 0.6)
    def estrella_mar(cx, cy, col):
        for a2 in range(5):
            ang = math.radians(a2 * 72 - 90)
            for j in range(4):
                L.p(cx + math.cos(ang) * j, cy + math.sin(ang) * j * 0.8, col if j < 3 else mezclar(col, 0x000000, 0.3))
        L.p(cx, cy, mezclar(col, 0xFFFFFF, 0.4))
    estrella_mar(g(250), g(432), 0xF07A3A); estrella_mar(g(300), g(398), 0xE8483C); estrella_mar(g(186), g(440), 0xF4C04A)
    for (ux, uy) in ((g(260), g(404)), (g(130), g(426))):  # erizos
        for a2 in range(0, 360, 30):
            L.p(ux + 2.5 * math.cos(math.radians(a2)), uy + 2.0 * math.sin(math.radians(a2)), 0x3A1A4A)
        disco(L, ux, uy, 1.5, 0x5A2A6A)
    coral(g(176), g(396), 0xF4D05A, 7); coral(g(292), g(392), 0x6AD8C0, 8); coral(g(106), g(404), 0xD86AD0, 9)
    # rocas con musgo
    L.piedra(g(200), g(402), 9, 6, cols=(0x2A3A4E, 0x445A72, 0x627A92, 0x88A0B4), contorno=0x101A28, musgo=0x3E8A5A)
    L.piedra(g(280), g(408), 6, 4, cols=(0x2A3A4E, 0x445A72, 0x627A92, 0x88A0B4), contorno=0x101A28, musgo=0x3E8A5A)
    # el ancla (inclinada, con cadena) y el cofre medio enterrado
    ax, ay = g(384), g(392)
    for j in range(16):
        L.p(ax - j * 0.4, ay - j, 0x4A5260); L.p(ax - j * 0.4 + 1, ay - j, 0x6A7484)
    disco(L, ax - 6.5, ay - 17, 1.6, 0x101A28)
    for j in range(-6, 7):
        L.p(ax + j, ay + abs(j) * -0.4 + 1, 0x4A5260)
    for j in range(10):
        L.p(ax - 8 - j * 1.2, ay - 18 + math.sin(j) * 0.8, 0x5A6272)
    cx, cy = g(160), g(402)
    for y in range(int(cy - 5), int(cy + 2)):
        for x in range(int(cx - 7), int(cx + 8)):
            col = 0x8A5A2A if y > cy - 3 else 0xA06A32
            if x in (int(cx - 7), int(cx + 7)) or y == int(cy - 5):
                col = 0x3A2010
            if abs(x - cx) < 1:
                col = 0xE8C84A
            L.p(x, y, col)
    L.p(cx, cy - 2, 0xFFF0A0)
    # burbujas que suben y pececitos lejanos (siluetas)
    rs = np.random.RandomState(13)
    for _ in range(40):
        x, y = rs.uniform(10, N - 10), rs.uniform(sy + 4, g(360))
        r = rs.uniform(0.6, 1.8)
        for a in range(0, 360, 30):
            mezcla_px(L, x + r * math.cos(math.radians(a)), y + r * math.sin(math.radians(a)), 0xE0F8FF, 0.5)
        L.p(x - r * 0.5, y - r * 0.5, 0xFFFFFF)
    for (x, y, d) in ((g(150), g(130), 1), (g(160), g(136), 1), (g(330), g(160), -1), (g(318), g(168), -1), (g(300), g(110), -1)):
        for j in range(4):
            mezcla_px(L, x + j * d, y, 0x1C5684, 0.7)
            mezcla_px(L, x + j * d, y - 1 if j in (1, 2) else y, 0x1C5684, 0.7)
        mezcla_px(L, x - d, y - 1, 0x1C5684, 0.7); mezcla_px(L, x - d, y + 1, 0x1C5684, 0.7)
    return final(L)


if __name__ == "__main__":
    for nombre, f in (("17_isla_faro.png", faro), ("18_isla_buceo.png", buceo)):
        Image.fromarray(f()).save(os.path.join(CARPETA, nombre))
        print("listo:", nombre)
