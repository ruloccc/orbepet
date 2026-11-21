"""(1.0.65) La regadera nueva en la tabla de íconos HD (48x48 con paleta), que es la que usa el orbe: el dibujo de art.c
(case ICO_REGADERA) solo se ve si el ícono NO está en iconos_hd.inc. Mismas medidas que en art.c (12 unidades de lado).
Uso: python herramientas/iconos_nuevos/regadera48.py   (reescribe HDI_REGADERA_PAL / HDI_REGADERA_IDX en firmware/main/iconos_hd.inc)"""
import math, os, re, sys
N = 48; K = N / 12.0
img = [[None] * N for _ in range(N)]
def A(h): return (h >> 16 & 255, h >> 8 & 255, h & 255)
def pon(X, Y, c):
    if 0 <= X < N and 0 <= Y < N: img[Y][X] = c
def elipse(cx, cy, rx, ry, c):
    for Y in range(N):
        for X in range(N):
            dx = ((X + .5) / K - cx) / rx; dy = ((Y + .5) / K - cy) / ry
            if dx * dx + dy * dy <= 1: pon(X, Y, c)
def circulo(cx, cy, r, c): elipse(cx, cy, r, r, c)
def rect(x, y, w, h, c):
    for Y in range(N):
        for X in range(N):
            if x <= (X + .5) / K < x + w and y <= (Y + .5) / K < y + h: pon(X, Y, c)
def trazo(x0, y0, x1, y1, r, c):
    n = int(math.hypot(x1 - x0, y1 - y0) * 3 * K) + 1
    for i in range(n + 1):
        t = i / n; circulo(x0 + (x1 - x0) * t, y0 + (y1 - y0) * t, r, c)
def arco(cx, cy, r, a0, a1, g, c):
    for i in range(61):
        a = math.radians(a0 + (a1 - a0) * i / 60); circulo(cx + math.cos(a) * r, cy + math.sin(a) * r, g, c)

O = A(0x2B6E66); B = A(0x4FB3A6); L = A(0x6CCABD)
arco(1.55, 8.0, 1.55, 95, 265, 0.42, O)          # asa de atrás
arco(4.7, 6.0, 2.1, 195, 330, 0.42, O)           # asa de arriba
trazo(7.2, 9.4, 10.6, 4.6, 0.70, O)              # pico largo
trazo(7.2, 9.2, 10.4, 4.6, 0.48, B)
elipse(4.9, 7.9, 3.55, 2.75, O); rect(1.4, 5.7, 7, 4.0, O)   # cuerpo
elipse(4.9, 7.9, 3.3, 2.5, B); rect(1.7, 5.9, 6.4, 3.6, B)
rect(1.7, 5.9, 2.0, 3.6, L)
elipse(4.9, 5.75, 3.3, 0.72, O)                  # la boca
elipse(4.9, 5.7, 2.9, 0.45, A(0x1C4E49))
elipse(4.9, 10.15, 3.4, 0.55, O)                 # el fondo
trazo(2.2, 6.6, 2.2, 9.2, 0.3, A(0xD2F6EF))      # brillo
circulo(2.2, 6.4, 0.3, A(0xF2FFFC))
for a in range(0, 360, 72):                      # florcita pintada
    circulo(5.6 + math.cos(math.radians(a)) * 0.6, 8.0 + math.sin(math.radians(a)) * 0.6, 0.4, A(0xFF8FB8))
circulo(5.6, 8.0, 0.34, A(0xFFE27A))
elipse(10.9, 4.1, 1.0, 1.2, A(0x8F6328))         # la flor de la regadera, dorada con agujeritos
elipse(11.0, 4.0, 0.78, 0.98, A(0xE6B453))
for (dx, dy) in ((0, -0.45), (0.32, 0.05), (-0.28, 0.2), (0.05, 0.5), (0, 0)):
    circulo(11.0 + dx, 4.0 + dy, 0.11, A(0x7A5420))
# contorno oscuro de un pixel, como los otros íconos HD
CONT = A(0x173D39)
borde = [(X, Y) for Y in range(N) for X in range(N) if img[Y][X] is None and any(
    0 <= X + dx < N and 0 <= Y + dy < N and img[Y + dy][X + dx] is not None and img[Y + dy][X + dx] != CONT
    for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)))]
for X, Y in borde: img[Y][X] = CONT

pal = []
for fila in img:
    for c in fila:
        if c is not None and c not in pal: pal.append(c)
assert len(pal) < 255
idx = [0 if c is None else pal.index(c) + 1 for fila in img for c in fila]
if __name__ == "__main__":
    raiz = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..")
    ruta = os.path.join(raiz, "firmware", "main", "iconos_hd.inc")
    s = open(ruta, encoding="utf-8").read()
    pb = ",".join(f"255,{r},{g},{b}" for r, g, b in pal)
    s, n1 = re.subn(r"static const uint8_t HDI_REGADERA_PAL\[\d+\] = \{[^}]*\};",
                    f"static const uint8_t HDI_REGADERA_PAL[{len(pal) * 4}] = {{{pb}}};", s)
    s, n2 = re.subn(r"static const uint8_t HDI_REGADERA_IDX\[\d+\] = \{[^}]*\};",
                    "static const uint8_t HDI_REGADERA_IDX[2304] = {" + ",".join(map(str, idx)) + "};", s)
    s, n3 = re.subn(r"\{ICO_REGADERA, HDI_REGADERA_PAL, \d+, HDI_REGADERA_IDX\}",
                    f"{{ICO_REGADERA, HDI_REGADERA_PAL, {len(pal)}, HDI_REGADERA_IDX}}", s)
    assert n1 == n2 == n3 == 1, (n1, n2, n3)
    open(ruta, "w", encoding="utf-8").write(s)
    print(f"regadera: {len(pal)} colores escritos en iconos_hd.inc")
