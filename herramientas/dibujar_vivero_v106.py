"""(v106) El vivero de los huevos (pantalla 0), dibujado como las zonas del mundo: un prado de noche visto desde
arriba, con pasto de verdad, arbustos, piedras, luciérnagas y OCHO NIDOS tejidos donde se posa cada huevo.

Antes era un degradado verde con una franja de cielo y ocho elipses cafés: al lado del claro del rey o del mirador
se notaba que venía de otro juego.

Uso:   python herramientas/dibujar_vivero_v106.py        ->  fondos_pantallas/0_huevos.png (466 x 466)
Luego: python herramientas/hacer_pantallas.py            (lo empaqueta en pantallas_data.inc)
"""
import os, sys
import numpy as np
from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from pixel_mundo import Lienzo, N, mezclar  # noqa: E402

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SALIDA = os.path.join(RAIZ, "fondos_pantallas", "0_huevos.png")

PANT = 466          # lo que mide la pantalla
BORDE = (N * 2 - PANT) // 2   # el dibujo (256) se agranda al doble (512) y se recorta al centro

# los huevos de la pantalla: cuatro arriba y cuatro abajo (las mismas posiciones que usa ui.c)
CX = [232 + round((i - 1.5) * 86) for i in range(4)]
CY = [52 + 84, 150 + 84]   # el nido va bajo el huevo (el huevo se dibuja centrado en 52/150 + 46)


def g(s):
    """de pixel de pantalla a pixel del dibujo"""
    return (s + BORDE) / 2.0


def nido(L, cx, cy, rx=19.0, ry=9.0, sem=0):
    """Un nido tejido visto un poco desde arriba: sombra en el pasto, ramitas cruzadas en tres cafés,
    el hueco oscuro adentro, pajitas claras en el borde y un poco de musgo."""
    rs = np.random.RandomState(1000 + sem)
    OSC, MED, CLA, PAJA = 0x4A2E18, 0x6B4423, 0x8A5A2E, 0xC19A5B
    L.sombra_elipse(cx + 1.5, cy + ry * 0.70, rx * 1.05, ry * 0.85, 0.68)
    # el cuerpo del nido: óvalo con borde oscuro
    for y in range(int(cy - ry - 2), int(cy + ry + 3)):
        for x in range(int(cx - rx - 2), int(cx + rx + 3)):
            q = ((x - cx) / rx) ** 2 + ((y - cy) / ry) ** 2
            if q <= 1.0:
                dy = (y - cy) / ry
                col = CLA if dy < -0.45 else (MED if dy < 0.45 else OSC)   # luz arriba, sombra abajo
                L.p(x, y, col)
            elif q <= 1.0 + 2.6 / max(rx, ry):
                L.p(x, y, 0x2A1A0E)                                        # contorno
    # ramitas cruzadas: trazos cortos en diagonal, alternando los dos cafés
    for k in range(int(rx * 2.6)):
        x0 = cx - rx + rs.uniform(0, 2 * rx)
        y0 = cy + rs.uniform(-ry * 0.95, ry * 0.95)
        largo = rs.randint(3, 7)
        subida = -1 if rs.uniform() < 0.5 else 1
        col = [OSC, MED, CLA, PAJA][rs.randint(0, 4)]
        for j in range(largo):
            x, y = x0 + j, y0 + (j * subida) * 0.35
            if ((x - cx) / rx) ** 2 + ((y - cy) / ry) ** 2 <= 0.96:
                L.p(x, y, col)
    # el hueco: más oscuro y hundido, con el borde de abajo iluminado
    hx, hy = rx * 0.66, ry * 0.52
    for y in range(int(cy - hy - 1), int(cy + hy + 2)):
        for x in range(int(cx - hx - 1), int(cx + hx + 2)):
            q = ((x - cx) / hx) ** 2 + ((y - cy + ry * 0.12) / hy) ** 2
            if q <= 1.0:
                L.p(x, y, 0x241608 if q < 0.72 else 0x33200E)
            elif q <= 1.35 and y > cy:
                L.p(x, y, PAJA if (x + y) % 2 else MED)
    # pajitas que asoman por el borde
    for k in range(7):
        a = rs.uniform(0.15, 2.99)
        x = cx + np.cos(a) * rx * rs.uniform(0.85, 1.15)
        y = cy - abs(np.sin(a)) * ry * rs.uniform(0.9, 1.2)
        for j in range(rs.randint(2, 5)):
            L.p(x + j * (1 if x > cx else -1), y - j * 0.6, PAJA if j % 2 else 0x9A7340)
    # musguito en el borde de arriba
    for k in range(5):
        x = cx + rs.uniform(-rx * 0.8, rx * 0.8)
        y = cy - ry * rs.uniform(0.5, 0.9)
        L.p(x, y, 0x4E7A3A)
        if rs.uniform() < 0.5:
            L.p(x + 1, y, 0x669448)


def luciernaga(L, x, y, fuerte=True):
    """una luciérnaga: su luz tiñe el pasto alrededor y el puntito encima"""
    for dy in range(-2, 3):
        for dx in range(-2, 3):
            d = dx * dx + dy * dy
            if d > 4 or (d == 4 and not fuerte):
                continue
            px, py = int(x + dx), int(y + dy)
            if 0 <= px < N and 0 <= py < N:
                t = 0.42 if d <= 1 else (0.22 if d <= 2 else 0.10)
                L.c[py, px] = np.array(hex_mezcla(int(L.c[py, px][0]) << 16 | int(L.c[py, px][1]) << 8 | int(L.c[py, px][2]), 0xFFF2A0, t))
    L.p(x, y, 0xFFF6C0)
    if fuerte:
        L.p(x, y - 1, 0xFFE87A)


def hex_mezcla(a, b, t):
    c = mezclar(a, b, t)
    return [(c >> 16) & 255, (c >> 8) & 255, c & 255]


def vivero():
    L = Lienzo(sem=11)
    # pasto de noche: verdes apagados y azulados, con florcitas pálidas que la luna alcanza
    L.pasto((0x16301F, 0x1D3E27, 0x27512F, 0x33663A), sem=5, matas=1100, treboles=70,
            flores=(130, (0xBFD6E8, 0xE8E0F4, 0xCFE4A8, 0xF2D8E8, 0xA8C8E0)))
    # la luz de la luna: una mancha clara que baja desde arriba a la izquierda
    yy, xx = L.yy, L.xx
    luna = np.clip(1.0 - np.sqrt(((xx - 76) / 200.0) ** 2 + ((yy - 44) / 180.0) ** 2), 0, 1) ** 1.6
    luna = luna * (0.82 + 0.36 * L.bay)   # tramado, para que la luz no corte en bandas
    L.c[:] = np.clip(L.c + (luna[..., None] * np.array([40, 46, 30])).astype(np.int16), 0, 255)
    # arbustos alrededor (el borde del claro donde están los nidos)
    rs = np.random.RandomState(7)
    for (bx, by, br) in ((16, 30, 13), (36, 12, 11), (228, 20, 12), (246, 54, 14), (10, 120, 12), (250, 140, 12),
                         (22, 226, 13), (60, 246, 12), (196, 244, 13), (240, 214, 14), (128, 250, 11), (248, 96, 10),
                         (6, 70, 10), (150, 6, 11), (86, 4, 10)):
        L.arbusto(bx, by, br, (0x1C3E24, 0x27522E, 0x37703C, 0x4C8C4A), contorno=0x12281A,
                  bayas=(0xE8D27A if rs.uniform() < 0.4 else None), sem=int(bx + by))
    # piedras y hongos sueltos, para que el suelo no quede vacío
    for (px, py, prx, pry) in ((40, 160, 7, 5), (214, 170, 6, 4.5), (120, 236, 8, 5), (196, 110, 5, 4)):
        L.piedra(px, py, prx, pry, cols=(0x3A3A48, 0x4E4E60, 0x646478, 0x80808E), contorno=0x24242E, musgo=0x3E6A38)
    for (mx, my) in ((30, 196), (34, 200), (222, 232), (226, 236), (168, 22), (172, 26)):
        L.p(mx, my + 1, 0xE8DCC0); L.p(mx, my + 2, 0xD6C8A8)
        for dx in (-2, -1, 0, 1, 2):
            L.p(mx + dx, my, 0xB04840 if dx else 0xC85A4E)
        L.p(mx - 1, my, 0xE8A89A)
    # el pasto pisado debajo de cada fila de nidos: una mancha de tierra clarita, nada de franjas rectas
    for cy in CY:
        for cx in CX:
            ex, ey = g(cx), g(cy) + 7
            m = ((xx - ex) / 23.0) ** 2 + ((yy - ey) / 8.0) ** 2 < 1.0
            m &= L.bay < 0.72
            L.c[m] = np.array(hex_mezcla(0x3A4A26, 0x2A3A1E, 0.35))
    # los ocho nidos
    k = 0
    for fila, cy in enumerate(CY):
        for cx in CX:
            nido(L, g(cx), g(cy), sem=k)
            k += 1
    # luciérnagas: unas cuantas, repartidas por el prado (no encima de los huevos)
    for (lx, ly, fuerte) in ((34, 108, True), (210, 118, True), (62, 182, True), (150, 190, False), (238, 184, True),
                             (18, 60, False), (196, 60, True), (112, 214, True), (56, 240, False), (176, 232, True),
                             (240, 30, False), (10, 170, True)):
        luciernaga(L, lx, ly, fuerte)
    # un poco más oscuro arriba y abajo: así se leen el título y el recuadro de la pista
    sombra = np.clip((np.abs(yy - 128) - 86) / 42.0, 0, 1) ** 1.5
    L.c[:] = np.clip(L.c * (1.0 - sombra[..., None] * 0.42), 0, 255).astype(np.int16)
    grande = np.kron(np.clip(L.c, 0, 255).astype(np.uint8), np.ones((2, 2, 1), np.uint8))
    return grande[BORDE:BORDE + PANT, BORDE:BORDE + PANT]


if __name__ == "__main__":
    Image.fromarray(vivero()).save(SALIDA)
    print("listo:", SALIDA)
