"""(1.0.19) Los fondos de los juegos de a dos y de la isla, hechos SOBRE las ilustraciones del juego (fondos_zonas/),
para que se vean como el resto del orbe. Encima se pinta lo propio de cada juego, suave y a la misma resolución
(se dibuja al doble y se achica: bordes lisos).

  13_dos_pingpong.png    el prado en primavera con una mesa de ping pong al centro
  14_dos_cuerda.png      el prado en otoño con el charco de barro al centro y dos banderitas
  15_dos_memorice.png    la laguna con un mantel de cuadros al centro (las cartas van encima)
  16_dos_escondidas.png  el bosque tal cual (los escondites son los del dibujo: tronco, arbusto, puente, piedras...)
  17_isla_faro.png       (1.0.24) pantalla propia del faro DE NOCHE: el mismo faro de la isla en su costa con el muelle, y mar abierto
  18_isla_buceo.png      bajo el agua, dibujado suave (rayos de luz, arena, algas, corales, rocas)
  19_arcoiris.png        (1.0.31) el tobogán del arcoíris: cielo alto, sol, mar de nubes y el arcoíris que baja desde una nube
                         lejana hacia la pantalla (en perspectiva: angosto arriba, ancho abajo)

Además escribe firmware/main/isla_mapa.inc: el mapa de agua de la isla (dónde pueden navegar los barquitos del faro).

Uso:   python herramientas/hacer_fondos_juegos.py   y luego   python herramientas/hacer_fondos_dos.py
Las coordenadas (pantalla de 466) que usa firmware/main/ui_dos.inc y ui_isla.inc están aquí arriba: si cambian aquí,
cambian allá."""
import math
import os

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ZONAS = os.path.join(RAIZ, "fondos_zonas")
SAL = os.path.join(RAIZ, "fondos_pantallas")
P = 466
K = 2            # se dibuja al doble
G = P * K

# --- lo que comparte con el firmware (pantalla) ---
MESA = (114, 72, 352, 398)          # ping pong: x0, y0, x1, y1 de la mesa; la red en y 235
CHARCO = (233, 250, 78, 44)         # tira y afloja: centro y radios del barro
BANDERAS = (176, 290)               # x de las dos banderitas (y 250)
MANTEL = (76, 96, 390, 392)         # memorice
ESCONDITES = [(193, 210), (92, 236), (108, 302), (343, 262), (300, 336), (300, 186)]  # tronco, arbusto, puente, piedras, arbusto, árbol
FARO_LAMPARA = (386, 128)           # la isla: la lámpara del faro
MUELLE = (38, 282)                  # la punta del muelle
ARCO_HY = 64                        # (1.0.31) el arcoíris del tobogán sale de la nube en y = ARCO_HY ...
ARCO_HW0, ARCO_HWK = 34.0, 0.24     # ... y su medio ancho en la fila y es ARCO_HW0 + (y - ARCO_HY) * ARCO_HWK (ui_arcoiris.inc)


def base(nombre, oscurecer=0.0, desenfoque=0.0):
    im = Image.open(os.path.join(ZONAS, nombre)).convert("RGB").resize((G, G), Image.LANCZOS)
    if desenfoque:
        im = im.filter(ImageFilter.GaussianBlur(desenfoque * K))
    if oscurecer:
        im = Image.blend(im, Image.new("RGB", im.size, (0, 0, 0)), oscurecer)
    return im.convert("RGBA")


def s(v):
    return int(round(v * K))


def sombra(im, forma, desplaz=(6, 8), radio=10, opa=110):
    """forma: función que dibuja en un ImageDraw (en negro) la silueta"""
    capa = Image.new("L", im.size, 0)
    forma(ImageDraw.Draw(capa))
    capa = capa.filter(ImageFilter.GaussianBlur(radio * K))
    capa = Image.eval(capa, lambda v: v * opa // 255)
    neg = Image.new("RGBA", im.size, (20, 26, 18, 0))
    neg.putalpha(capa)
    desp = Image.new("RGBA", im.size, (0, 0, 0, 0))
    desp.paste(neg, (s(desplaz[0]), s(desplaz[1])))
    return Image.alpha_composite(im, desp)


def final(im, nombre):
    out = im.convert("RGB").resize((P, P), Image.LANCZOS)
    out.save(os.path.join(SAL, nombre))
    print("listo:", nombre)
    return out


# ------------------------------------------------------------------------------------------------------- ping pong
def pingpong():
    im = base("prado_primavera.png", oscurecer=0.18, desenfoque=0.6)
    x0, y0, x1, y1 = MESA
    rr = 14
    im = sombra(im, lambda d: d.rounded_rectangle([s(x0), s(y0), s(x1), s(y1)], s(rr), fill=255), (8, 12), 9, 130)
    d = ImageDraw.Draw(im)
    # el canto de la mesa (más oscuro) y la cubierta azul con un brillo arriba a la izquierda
    d.rounded_rectangle([s(x0), s(y0), s(x1), s(y1 + 6)], s(rr), fill=(18, 52, 92, 255))
    d.rounded_rectangle([s(x0), s(y0), s(x1), s(y1)], s(rr), fill=(36, 104, 168, 255))
    brillo = Image.new("L", im.size, 0)
    ImageDraw.Draw(brillo).ellipse([s(x0 - 80), s(y0 - 90), s(x0 + 170), s(y0 + 150)], fill=70)
    brillo = brillo.filter(ImageFilter.GaussianBlur(30 * K))
    mascara = Image.new("L", im.size, 0)
    ImageDraw.Draw(mascara).rounded_rectangle([s(x0), s(y0), s(x1), s(y1)], s(rr), fill=255)
    luz = Image.new("RGBA", im.size, (255, 255, 255, 0))
    luz.putalpha(Image.fromarray(np.minimum(np.asarray(brillo), np.asarray(mascara))))
    im = Image.alpha_composite(im, luz)
    d = ImageDraw.Draw(im)
    # las líneas blancas: borde, línea del medio (a lo largo)
    w = s(3)
    d.rounded_rectangle([s(x0 + 7), s(y0 + 7), s(x1 - 7), s(y1 - 7)], s(rr - 6), outline=(244, 248, 252, 255), width=w)
    d.line([s((x0 + x1) / 2), s(y0 + 7), s((x0 + x1) / 2), s(y1 - 7)], fill=(244, 248, 252, 230), width=s(2))
    # la red: sombra, malla y la cinta blanca de arriba, con los postes afuera de la mesa
    ny = 235
    d.rectangle([s(x0 - 10), s(ny + 3), s(x1 + 10), s(ny + 9)], fill=(10, 30, 50, 120))
    d.rectangle([s(x0 - 10), s(ny - 7), s(x1 + 10), s(ny + 3)], fill=(30, 34, 40, 220))
    for xx in range(x0 - 8, x1 + 10, 5):
        d.line([s(xx), s(ny - 6), s(xx), s(ny + 2)], fill=(90, 96, 110, 255), width=1)
    d.line([s(x0 - 10), s(ny - 2), s(x1 + 10), s(ny - 2)], fill=(90, 96, 110, 255), width=1)
    d.rectangle([s(x0 - 10), s(ny - 9), s(x1 + 10), s(ny - 6)], fill=(250, 250, 252, 255))
    for px in (x0 - 13, x1 + 9):
        d.rounded_rectangle([s(px), s(ny - 12), s(px + 5), s(ny + 6)], s(2), fill=(60, 64, 72, 255))
        d.rectangle([s(px + 1), s(ny - 11), s(px + 2), s(ny + 4)], fill=(150, 156, 168, 255))
    # una pelotita de repuesto y una toalla sobre el pasto
    d.ellipse([s(372), s(380), s(382), s(390)], fill=(255, 250, 236, 255), outline=(200, 190, 160, 255))
    return final(im, "13_dos_pingpong.png")


# ---------------------------------------------------------------------------------------------------- tira y afloja
def cuerda():
    im = base("prado_otono.png", oscurecer=0.12, desenfoque=0.4)
    cx, cy, rx, ry = CHARCO
    im = sombra(im, lambda d: d.ellipse([s(cx - rx - 6), s(cy - ry - 4), s(cx + rx + 6), s(cy + ry + 6)], fill=255), (0, 4), 6, 90)
    d = ImageDraw.Draw(im)
    # el borde de tierra húmeda y el barro (más oscuro al centro), con brillos y burbujas
    d.ellipse([s(cx - rx - 8), s(cy - ry - 6), s(cx + rx + 8), s(cy + ry + 7)], fill=(120, 86, 52, 255))
    d.ellipse([s(cx - rx), s(cy - ry), s(cx + rx), s(cy + ry)], fill=(92, 62, 36, 255))
    centro = Image.new("L", im.size, 0)
    ImageDraw.Draw(centro).ellipse([s(cx - rx * 0.7), s(cy - ry * 0.65), s(cx + rx * 0.7), s(cy + ry * 0.7)], fill=150)
    centro = centro.filter(ImageFilter.GaussianBlur(12 * K))
    osc = Image.new("RGBA", im.size, (52, 34, 18, 0))
    osc.putalpha(centro)
    im = Image.alpha_composite(im, osc)
    d = ImageDraw.Draw(im)
    for (bx, by, br) in ((-40, -16, 7), (30, -22, 5), (52, 10, 6), (-18, 18, 4), (8, -2, 3)):
        d.ellipse([s(cx + bx - br), s(cy + by - br * 0.6), s(cx + bx + br), s(cy + by + br * 0.6)], outline=(140, 104, 66, 255), width=s(1))
    for (hx, hy, hw) in ((-30, -24, 26), (14, -30, 18), (40, -8, 14)):
        d.arc([s(cx + hx - hw), s(cy + hy - 6), s(cx + hx + hw), s(cy + hy + 6)], 200, 330, fill=(214, 184, 140, 220), width=s(2))
    rs = np.random.RandomState(4)
    for _ in range(26):  # salpicaduras de barro alrededor
        a = rs.uniform(0, 6.283); q = rs.uniform(1.12, 1.45)
        x, y = cx + math.cos(a) * rx * q, cy + math.sin(a) * ry * q
        r = rs.uniform(1.5, 4)
        d.ellipse([s(x - r), s(y - r * 0.7), s(x + r), s(y + r * 0.7)], fill=(98, 66, 38, 255))
    # huellas que entran al barro
    for (fx, fy) in ((cx - 120, cy + 14), (cx - 104, cy + 6), (cx + 104, cy + 8), (cx + 120, cy + 16)):
        d.ellipse([s(fx - 4), s(fy - 3), s(fx + 4), s(fy + 3)], fill=(104, 74, 44, 200))
    # las dos banderitas en estacas (la de ella amarilla a la izquierda, la de la visita azul a la derecha)
    for (bxx, col) in ((BANDERAS[0], (255, 200, 60, 255)), (BANDERAS[1], (90, 160, 230, 255))):
        d.ellipse([s(bxx - 5), s(cy + 44), s(bxx + 7), s(cy + 50)], fill=(40, 28, 16, 110))
        d.rectangle([s(bxx - 1.5), s(cy - 28), s(bxx + 1.5), s(cy + 47)], fill=(110, 76, 44, 255))
        d.rectangle([s(bxx - 1.5), s(cy - 28), s(bxx - 0.5), s(cy + 47)], fill=(160, 120, 76, 255))
        d.polygon([(s(bxx + 1.5), s(cy - 28)), (s(bxx + 26), s(cy - 20)), (s(bxx + 1.5), s(cy - 11))], fill=col)
        d.line([(s(bxx + 1.5), s(cy - 28)), (s(bxx + 26), s(cy - 20)), (s(bxx + 1.5), s(cy - 11))], fill=(60, 50, 40, 255), width=s(1))
    return final(im, "14_dos_cuerda.png")


# -------------------------------------------------------------------------------------------------------- memorice
def memorice():
    im = base("lago_verano.png", oscurecer=0.15, desenfoque=0.8)
    x0, y0, x1, y1 = MANTEL
    im = sombra(im, lambda d: d.rounded_rectangle([s(x0), s(y0), s(x1), s(y1)], s(8), fill=255), (6, 9), 8, 120)
    capa = Image.new("RGBA", im.size, (0, 0, 0, 0))
    d = ImageDraw.Draw(capa)
    d.rounded_rectangle([s(x0), s(y0), s(x1), s(y1)], s(8), fill=(250, 246, 240, 255))
    paso = 24  # el cuadrillé de picnic: franjas de media celda en las dos direcciones (más rojo donde se cruzan)
    for xx in range(x0, x1, paso):
        d.rectangle([s(xx), s(y0), s(xx + paso / 2), s(y1)], fill=(220, 70, 70, 120))
    franjas = Image.new("RGBA", im.size, (0, 0, 0, 0))
    df = ImageDraw.Draw(franjas)
    for yy in range(y0, y1, paso):
        df.rectangle([s(x0), s(yy), s(x1), s(yy + paso / 2)], fill=(220, 70, 70, 120))
    capa = Image.alpha_composite(capa, franjas)
    d = ImageDraw.Draw(capa)
    mascara = Image.new("L", im.size, 0)
    ImageDraw.Draw(mascara).rounded_rectangle([s(x0), s(y0), s(x1), s(y1)], s(8), fill=255)
    a = np.minimum(np.asarray(capa.split()[3]), np.asarray(mascara))
    capa.putalpha(Image.fromarray(a))
    im = Image.alpha_composite(im, capa)
    d = ImageDraw.Draw(im)
    # pliegues suaves y el ribete
    d.rounded_rectangle([s(x0), s(y0), s(x1), s(y1)], s(8), outline=(180, 40, 40, 255), width=s(3))
    for xx in range(x0 + 6, x1 - 4, 8):  # el borde cosido
        d.line([s(xx), s(y0 + 5), s(xx + 4), s(y0 + 5)], fill=(255, 255, 255, 200), width=s(1))
        d.line([s(xx), s(y1 - 5), s(xx + 4), s(y1 - 5)], fill=(255, 255, 255, 200), width=s(1))
    return final(im, "15_dos_memorice.png")


# ------------------------------------------------------------------------------------------------------ escondidas
def escondidas():
    im = base("bosque_verano.png", oscurecer=0.04)
    return final(im, "16_dos_escondidas.png")


# ------------------------------------------------------------------------------------------------- el faro (noche)
# (1.0.24) Pantalla propia del faro: antes era la isla entera de noche y el mar era solo un anillo angosto alrededor.
# Ahora: el MISMO faro de la isla (recortado de la ilustración, sin espejo, con su base de rocas) en la costa oeste de la
# isla con su muelle, a la derecha; a la izquierda, mar abierto con las rocas de la isla. Los barquitos llegan por la izquierda.
FARO2_LAMPARA = (391, 150)     # el vidrio de la lámpara (pantalla de 466)
FARO2_MUELLE = (206, 330)      # la punta del muelle, con el salvavidas
FARO2_VENTANAS = ((391, 203), (391, 240))
FARO2_FAROL = (232, 316)       # un farolito en el muelle


def faro():
    src = Image.open(os.path.join(ZONAS, "isla_verano.png")).convert("RGB")
    A = np.asarray(src).astype(np.float32)
    r, g, b = A[..., 0], A[..., 1], A[..., 2]
    hondo = (b > r + 60) & (b > g + 20) & (b < 235)
    hondoL = Image.fromarray((hondo * 255).astype(np.uint8)).filter(ImageFilter.MaxFilter(5)).filter(ImageFilter.MinFilter(5))
    f = P / 1024 * 1.35           # fuente (1024) -> pantalla: el faro 35 % más grande que en la isla
    F = f * K
    S = int(np.ceil(P / f))
    tierra = Image.new("L", (G, G), 0)   # dónde NO es agua (para el mapa de los barquitos)

    def capa(caja, suave=2.0):
        im_ = src.crop(caja).convert("RGBA")
        m_ = Image.eval(hondoL.crop(caja), lambda v: 255 - v).filter(ImageFilter.GaussianBlur(suave))
        im_.putalpha(m_)
        w, h = im_.size
        return im_.resize((int(w * F), int(h * F)), Image.LANCZOS)

    def pegar(im_, c, x, y):
        im_.alpha_composite(c, (x, y))
        a_ = c.split()[3].point(lambda v: 255 if v > 150 else 0)
        tierra.paste(255, (x, y), a_)
    # el mar, como el de la ilustración: azul parejo (más claro hacia la costa) con guiones de olas claros y oscuros
    rs = np.random.RandomState(11)
    yy, xx = np.mgrid[0:S, 0:S]
    t = np.clip(xx / S * 1.1, 0, 1)
    fondo = np.stack([30 + 3 * t, 76 + 18 * t, 143 + 34 * t], -1) + rs.normal(0, 1.2, (S, S, 3))
    mar = Image.fromarray(np.clip(fondo, 0, 255).astype(np.uint8))
    d = ImageDraw.Draw(mar)
    for _ in range(int(S * S / 1300)):
        x, y = rs.uniform(0, S), rs.uniform(0, S)
        l = rs.uniform(5, 13)
        if rs.rand() < 0.55:
            d.line([x, y, x + l, y], fill=(118, 176, 232), width=2)
        else:
            d.line([x, y, x + l * 0.8, y], fill=(22, 58, 116), width=3)
    im = mar.resize((G, G), Image.LANCZOS).convert("RGBA")
    # la costa oeste de la isla con su muelle (el mar de la foto se saca: queda el nuestro)
    caja = (30, 180, 720, 900)
    pegar(im, capa(caja), int(180 * K), int(60 * K))
    # rocas sueltas: las de la isla
    for c, (x, y) in (((35, 330, 130, 405), (80, 110)), ((80, 815, 185, 900), (140, 300)), ((400, 885, 470, 940), (215, 175)),
                      ((665, 905, 755, 970), (60, 230)), ((940, 710, 1022, 780), (250, 395))):
        k = capa(c)
        pegar(im, k, int(x * K - k.size[0] / 2), int(y * K - k.size[1] / 2))
    # el faro de la isla, con su base de rocas, sin el pasto ni la arena de alrededor (el techo verde se queda)
    fc = (765, 222, 942, 578)
    fi = src.crop(fc).convert("RGBA")
    fa = np.asarray(fi).astype(np.float32)
    fr, fg, fb = fa[..., 0], fa[..., 1], fa[..., 2]
    quitar = ((fg > fr + 12) & (fg > fb + 8)) | ((fr > 150) & (fg > 130) & (fb < 140) & (fr - fb > 50)) | (fb > fr + 40)
    hh, ww = quitar.shape
    jj, ii = np.mgrid[0:hh, 0:ww]
    techo = ((ii - 90) / 36.0) ** 2 + ((jj - 46) / 30.0) ** 2 < 1.0   # el techo verde y el vidrio
    quitar &= ~techo
    mI = Image.fromarray((~quitar * 255).astype(np.uint8)).filter(ImageFilter.MinFilter(3)).filter(ImageFilter.MaxFilter(3)).filter(ImageFilter.GaussianBlur(0.8))
    fi.putalpha(mI)
    fi = fi.resize((int(fi.size[0] * F), int(fi.size[1] * F)), Image.LANCZOS)
    FX, FY = int(338 * K), int(92 * K)
    som = Image.new("L", im.size, 0)
    ImageDraw.Draw(som).ellipse([FX + 10 * K, FY + 190 * K, FX + 110 * K, FY + 222 * K], fill=120)
    som = som.filter(ImageFilter.GaussianBlur(6 * K))
    sc = Image.new("RGBA", im.size, (10, 20, 10, 0)); sc.putalpha(som)
    im = Image.alpha_composite(im, sc)
    pegar(im, fi, FX, FY)
    # la noche: oscuro y azulado; el agua un poco más clara que la tierra (para leer por dónde navegar)
    a = np.asarray(im.convert("RGB")).astype(np.float32)
    agua = np.asarray(tierra) < 128
    lum = a.mean(axis=2, keepdims=True)
    n = a * 0.30 + lum * 0.08
    n[..., 0] *= 0.80; n[..., 1] *= 0.95; n[..., 2] = n[..., 2] * 1.25 + 14
    n[agua] = n[agua] * 1.15 + np.array([4, 10, 22], np.float32)
    im = Image.fromarray(np.clip(n, 0, 255).astype(np.uint8)).convert("RGBA")

    def luz(x, y, r_, col, fuerza):
        m_ = Image.new("L", im.size, 0)
        ImageDraw.Draw(m_).ellipse([s(x - r_), s(y - r_), s(x + r_), s(y + r_)], fill=int(255 * fuerza))
        m_ = m_.filter(ImageFilter.GaussianBlur(r_ * K * 0.45))
        c_ = Image.new("RGBA", im.size, col + (0,)); c_.putalpha(m_)
        return Image.alpha_composite(im, c_)
    lx, ly = FARO2_LAMPARA
    im = luz(lx, ly, 80, (255, 226, 150), 0.5)
    im = luz(lx, ly, 26, (255, 240, 190), 0.9)
    im = luz(FARO2_FAROL[0], FARO2_FAROL[1], 26, (255, 210, 120), 0.6)
    im = luz(110, 70, 60, (190, 210, 255), 0.18)   # la luna, fuera de la pantalla: un brillo suave arriba a la izquierda
    d = ImageDraw.Draw(im)
    d.rectangle([s(lx - 9), s(ly - 9), s(lx + 9), s(ly + 9)], fill=(255, 246, 200, 230))  # el vidrio encendido
    for (wx, wy) in FARO2_VENTANAS:
        d.rectangle([s(wx - 3), s(wy - 5), s(wx + 3), s(wy + 5)], fill=(255, 210, 110, 255))
    fx_, fy_ = FARO2_FAROL
    d.rectangle([s(fx_ - 1), s(fy_ - 2), s(fx_ + 1), s(fy_ + 14)], fill=(60, 44, 30, 255))
    d.ellipse([s(fx_ - 4), s(fy_ - 7), s(fx_ + 4), s(fy_ + 1)], fill=(255, 226, 140, 255))
    for _ in range(80):  # brillitos de luna en el agua
        x, y = rs.uniform(0, P), rs.uniform(0, P)
        if agua[int(y * K), int(x * K)]:
            l = rs.uniform(3, 8)
            d.line([s(x), s(y), s(x + l), s(y)], fill=(200, 220, 255, 110), width=s(1))
    out = final(im, "17_isla_faro.png")
    # el mapa de agua para el firmware: 59 x 59 celdas de 8 px; 1 = agua
    agua_p = np.asarray(Image.fromarray((agua * 255).astype(np.uint8)).resize((P, P), Image.NEAREST)) > 127
    C = 8; Nc = (P + C - 1) // C
    filas = []
    for j in range(Nc):
        bits = 0
        for i in range(Nc):
            bloque = agua_p[j * C:(j + 1) * C, i * C:(i + 1) * C]
            if bloque.size and bloque.mean() > 0.75:
                bits |= 1 << i
        filas.append(bits)
    with open(os.path.join(RAIZ, "firmware", "main", "isla_mapa.inc"), "w", encoding="utf-8") as fo:
        fo.write("/* GENERADO por herramientas/hacer_fondos_juegos.py: el agua de la pantalla del faro (466, celdas de 8 px).\n"
                 " * Bit i de la fila j = la celda (i, j) es agua (los barquitos del faro navegan por ahí). No editar a mano. */\n")
        fo.write("#define ISLA_MAPA_C %d\n#define ISLA_MAPA_N %d\n" % (C, Nc))
        fo.write("static const uint64_t ISLA_AGUA[%d] = {\n" % Nc)
        for j in range(0, Nc, 3):
            fo.write("    " + ", ".join("0x%016XULL" % b_ for b_ in filas[j:j + 3]) + ",\n")
        fo.write("};\n")
    print("listo: isla_mapa.inc")
    return out


# ------------------------------------------------------------------------------------------------------------ buceo
def buceo():
    yy, xx = np.mgrid[0:G, 0:G].astype(np.float32) / K
    t = yy / P
    arriba, abajo = np.array([92, 196, 232], np.float32), np.array([10, 52, 104], np.float32)
    a = arriba * (1 - t[..., None]) ** 1.0 + abajo * t[..., None]
    # rayos de luz (bandas diagonales suaves que se apagan hacia abajo)
    for k, x0 in enumerate((40, 120, 190, 270, 340)):
        dist = np.abs((xx - (x0 + yy * 0.38)))
        anch = 18 + 8 * (k % 2)
        a += (np.clip(1 - dist / anch, 0, 1) ** 2 * (1 - t) ** 1.5 * 46)[..., None] * np.array([0.8, 1.0, 1.0])
    im = Image.fromarray(np.clip(a, 0, 255).astype(np.uint8)).convert("RGBA")
    d = ImageDraw.Draw(im)
    # la superficie, arriba: una franja clara que ondula
    pts = [(s(x), s(34 + 3 * math.sin(x * 0.07) + 2 * math.sin(x * 0.17))) for x in range(0, P + 1, 4)]
    d.polygon([(0, 0)] + pts + [(G, 0)], fill=(150, 222, 244, 255))
    d.line(pts, fill=(230, 248, 255, 255), width=s(3))
    # arrecife lejano (siluetas desenfocadas)
    lejos = Image.new("RGBA", im.size, (0, 0, 0, 0))
    dl = ImageDraw.Draw(lejos)
    for (cx, cy, r) in ((40, 340, 60), (120, 360, 46), (300, 350, 52), (400, 336, 64), (210, 372, 40)):
        dl.ellipse([s(cx - r), s(cy - r * 0.8), s(cx + r), s(cy + r * 0.8)], fill=(16, 64, 104, 200))
    im = Image.alpha_composite(im, lejos.filter(ImageFilter.GaussianBlur(5 * K)))
    d = ImageDraw.Draw(im)
    # paredes de roca a los lados (con sombra y musgo), como un arrecife que encierra la escena
    for lado in (0, 1):
        pared = Image.new("RGBA", im.size, (0, 0, 0, 0))
        dp = ImageDraw.Draw(pared)
        for k2, (yy2, rr2) in enumerate(((150, 40), (200, 52), (258, 60), (318, 66), (372, 70), (420, 64))):
            cx2 = (14 + rr2 * 0.4) if lado == 0 else (P - 14 - rr2 * 0.4)
            dp.ellipse([s(cx2 - rr2), s(yy2 - rr2 * 0.7), s(cx2 + rr2), s(yy2 + rr2 * 0.7)], fill=(36, 66, 92, 255))
            dp.ellipse([s(cx2 - rr2 * 0.7), s(yy2 - rr2 * 0.6), s(cx2 + rr2 * 0.3), s(yy2 - rr2 * 0.05)], fill=(62, 96, 124, 255))
            dp.ellipse([s(cx2 - rr2 * 0.5), s(yy2 - rr2 * 0.68), s(cx2 + rr2 * 0.2), s(yy2 - rr2 * 0.42)], fill=(70, 140, 100, 255))
        im = Image.alpha_composite(im, pared)
    d = ImageDraw.Draw(im)
    rs2 = np.random.RandomState(12)
    for _ in range(40):  # anémonas y corales chiquitos pegados a las rocas
        lado = rs2.randint(0, 2)
        y2 = rs2.uniform(150, 420)
        x2 = rs2.uniform(30, 70) if lado == 0 else P - rs2.uniform(30, 70)
        c2 = [(240, 110, 140, 255), (255, 170, 90, 255), (214, 110, 210, 255), (250, 220, 100, 255), (110, 220, 200, 255)][rs2.randint(0, 5)]
        r2 = rs2.uniform(2, 4.5)
        d.ellipse([s(x2 - r2), s(y2 - r2), s(x2 + r2), s(y2 + r2)], fill=c2)
    # la arena con dunas y ondas
    pts = [(s(x), s(370 + 8 * math.sin(x * 0.03 + 0.6) + 4 * math.sin(x * 0.09))) for x in range(0, P + 1, 4)]
    d.polygon(pts + [(G, G), (0, G)], fill=(222, 198, 140, 255))
    d.line(pts, fill=(246, 230, 186, 255), width=s(3))
    for j in range(8):
        y0 = 392 + j * 10
        for x in range(0, P, 26):
            d.arc([s(x + (j % 2) * 13), s(y0), s(x + 22 + (j % 2) * 13), s(y0 + 6)], 200, 340, fill=(190, 164, 110, 200), width=s(1))
    # reflejos de luz que tiemblan sobre la arena (una red suave)
    luzarena = Image.new("RGBA", im.size, (0, 0, 0, 0))
    dla = ImageDraw.Draw(luzarena)
    rs3 = np.random.RandomState(21)
    for _ in range(70):
        x2, y2 = rs3.uniform(20, P - 20), rs3.uniform(384, 460)
        dla.arc([s(x2), s(y2), s(x2 + rs3.uniform(10, 24)), s(y2 + rs3.uniform(4, 8))], rs3.uniform(0, 180), rs3.uniform(200, 360), fill=(255, 250, 220, 120), width=s(1))
    im = Image.alpha_composite(im, luzarena.filter(ImageFilter.GaussianBlur(0.6 * K)))
    d = ImageDraw.Draw(im)
    # un cofre medio enterrado y un ancla vieja
    d.rounded_rectangle([s(142), s(402), s(176), s(424)], s(3), fill=(130, 84, 40, 255), outline=(70, 40, 18, 255), width=s(1))
    d.pieslice([s(142), s(392), s(176), s(414)], 180, 360, fill=(150, 98, 48, 255), outline=(70, 40, 18, 255))
    d.rectangle([s(157), s(400), s(161), s(410)], fill=(230, 196, 80, 255))
    d.polygon([(s(130), s(426)), (s(190), s(426)), (s(186), s(430)), (s(134), s(430))], fill=(222, 198, 140, 255))
    d.line([(s(330), s(372)), (s(344), s(420))], fill=(90, 98, 110, 255), width=s(4))
    d.arc([s(322), s(400), s(362), s(430)], 20, 160, fill=(90, 98, 110, 255), width=s(4))
    d.ellipse([s(325), s(364), s(335), s(374)], outline=(90, 98, 110, 255), width=s(2))
    # algas largas a los lados (curvas suaves con hojas)
    def alga(x0, alto, col, fase):
        pts = []
        for j in range(0, alto, 3):
            pts.append((x0 + 7 * math.sin(j * 0.05 + fase), 392 - j))
        for k2 in range(len(pts) - 1):
            w = 5 - 3 * k2 / len(pts)
            d.line([(s(pts[k2][0]), s(pts[k2][1])), (s(pts[k2 + 1][0]), s(pts[k2 + 1][1]))], fill=col, width=s(w))
        for k2 in range(4, len(pts), 6):
            x, y = pts[k2]
            lado = 1 if (k2 // 6) % 2 else -1
            d.ellipse([s(x + lado * 2 - (12 if lado < 0 else 0)), s(y - 4), s(x + lado * 2 + (12 if lado > 0 else 0)), s(y + 3)], fill=col)
        x, y = pts[-1]
        d.ellipse([s(x - 3), s(y - 3), s(x + 3), s(y + 3)], fill=(240, 200, 80, 255))
    for (x0, alto, col, f) in ((70, 160, (40, 140, 80, 255), 0), (88, 210, (54, 160, 92, 255), 1), (104, 130, (40, 140, 80, 255), 2),
                               (360, 190, (54, 160, 92, 255), 3), (378, 150, (40, 140, 80, 255), 4), (394, 110, (70, 176, 104, 255), 5),
                               (200, 60, (54, 160, 92, 255), 6), (280, 50, (40, 140, 80, 255), 7)):
        alga(x0, alto, col, f)
    # corales (ramitas redondeadas y uno de cerebro), rocas con sombra, conchitas y una estrella de mar
    def coral(cx, cy, col, n, sem):
        r2 = np.random.RandomState(sem)
        for _ in range(n):
            x, y, a2 = cx + r2.uniform(-6, 6), cy, -math.pi / 2 + r2.uniform(-0.8, 0.8)
            for j in range(r2.randint(10, 22)):
                nx, ny = x + math.cos(a2) * 1.6, y + math.sin(a2) * 1.6
                d.line([(s(x), s(y)), (s(nx), s(ny))], fill=col, width=s(4))
                x, y = nx, ny
                a2 += r2.uniform(-0.2, 0.2)
            d.ellipse([s(x - 3), s(y - 3), s(x + 3), s(y + 3)], fill=col)
    coral(128, 396, (240, 110, 140, 255), 5, 1); coral(156, 400, (255, 160, 90, 255), 4, 2)
    coral(312, 398, (255, 136, 110, 255), 5, 3); coral(338, 394, (214, 110, 210, 255), 4, 4)
    for (cx, cy, r, col) in ((232, 408, 14, (232, 160, 200, 255)), (180, 404, 9, (120, 210, 190, 255))):
        d.ellipse([s(cx - r), s(cy - r * 0.8), s(cx + r), s(cy + r * 0.8)], fill=col, outline=(90, 40, 80, 255), width=s(1))
        for k2 in range(3):
            d.arc([s(cx - r + 3 + k2 * 2), s(cy - r * 0.8 + 3), s(cx + r - 3 - k2 * 2), s(cy + r * 0.8 - 3)], 190, 350, fill=(130, 70, 110, 200), width=s(1))
    for (cx, cy, rx, ry) in ((206, 414, 18, 11), (280, 416, 12, 8), (60, 404, 16, 10)):
        d.ellipse([s(cx - rx + 3), s(cy - ry + 5), s(cx + rx + 3), s(cy + ry + 5)], fill=(150, 126, 80, 200))
        d.ellipse([s(cx - rx), s(cy - ry), s(cx + rx), s(cy + ry)], fill=(96, 112, 130, 255))
        d.ellipse([s(cx - rx * 0.7), s(cy - ry * 0.85), s(cx + rx * 0.2), s(cy - ry * 0.1)], fill=(140, 156, 172, 255))
        d.ellipse([s(cx - rx * 0.6), s(cy - ry * 1.0), s(cx + rx * 0.5), s(cy - ry * 0.5)], fill=(80, 150, 100, 255))
    for a2 in range(5):  # estrella de mar
        ang = math.radians(a2 * 72 - 90)
        d.line([(s(254), s(430)), (s(254 + math.cos(ang) * 10), s(430 + math.sin(ang) * 10))], fill=(240, 120, 60, 255), width=s(4))
    d.ellipse([s(250), s(426), s(258), s(434)], fill=(250, 150, 80, 255))
    # burbujas que suben (con brillo) y pececitos lejanos
    rs = np.random.RandomState(7)
    for _ in range(28):
        x, y, r = rs.uniform(30, P - 30), rs.uniform(60, 340), rs.uniform(1.5, 4.5)
        d.ellipse([s(x - r), s(y - r), s(x + r), s(y + r)], outline=(220, 246, 255, 200), width=s(1))
        d.ellipse([s(x - r * 0.5), s(y - r * 0.6), s(x - r * 0.1), s(y - r * 0.2)], fill=(255, 255, 255, 220))
    for (fx, fy, dd) in ((150, 140, 1), (166, 148, 1), (320, 170, -1), (306, 180, -1), (290, 120, -1)):
        d.ellipse([s(fx - 6), s(fy - 3), s(fx + 6), s(fy + 3)], fill=(30, 90, 140, 200))
        d.polygon([(s(fx - 6 * dd), s(fy)), (s(fx - 11 * dd), s(fy - 4)), (s(fx - 11 * dd), s(fy + 4))], fill=(30, 90, 140, 200))
    return final(im, "18_isla_buceo.png")

# ------------------------------------------------------------------------------------------------------ arcoíris
def arcoiris():
    """(1.0.31) el tobogán del arcoíris: muy alto en el cielo. Arriba un cielo azul profundo que se aclara hacia abajo,
    el sol con su halo, nubes grandes y esponjosas a los lados (con sombra lavanda), un mar de nubes abajo y el arcoíris
    que nace de una nube brillante lejana y baja hasta la pantalla, con brillo en el centro de cada color."""
    yy, xx = np.mgrid[0:G, 0:G].astype(np.float32) / K
    t = np.clip(yy / P, 0, 1)
    arriba, medio, abajo = np.array([54, 118, 214], np.float32), np.array([120, 190, 246], np.float32), np.array([214, 238, 255], np.float32)
    a = np.where((t < 0.55)[..., None], arriba + (medio - arriba) * (t / 0.55)[..., None], medio + (abajo - medio) * ((t - 0.55) / 0.45)[..., None])
    # el sol (arriba a la izquierda) con su halo y rayos muy suaves
    sx, sy = 92.0, 70.0
    dsol = np.sqrt((xx - sx) ** 2 + (yy - sy) ** 2)
    halo = np.clip(1 - dsol / 170.0, 0, 1) ** 2.2
    a += halo[..., None] * np.array([90, 70, 30], np.float32)
    ang = np.arctan2(yy - sy, xx - sx)
    rayos = (0.5 + 0.5 * np.cos(ang * 9)) ** 6 * np.clip(1 - dsol / 260.0, 0, 1) ** 1.5
    a += rayos[..., None] * np.array([40, 34, 14], np.float32)
    # un tinte cálido rosado cerca del horizonte de nubes (atardecer suave)
    a += (np.clip(1 - np.abs(yy - 400) / 90.0, 0, 1) ** 2)[..., None] * np.array([22, 4, 10], np.float32)
    im = Image.fromarray(np.clip(a, 0, 255).astype(np.uint8)).convert("RGBA")
    d = ImageDraw.Draw(im)
    d.ellipse([s(sx - 24), s(sy - 24), s(sx + 24), s(sy + 24)], fill=(255, 246, 206, 255))
    d.ellipse([s(sx - 18), s(sy - 18), s(sx + 18), s(sy + 18)], fill=(255, 252, 232, 255))

    def nube(capa, puntos, base_col=(255, 255, 255), sombra_col=(186, 196, 236), brillo=True):
        """una nube esponjosa: círculos que se juntan; arriba blanca, abajo lavanda, con una sombra suave debajo"""
        m = Image.new("L", capa.size, 0)
        dm = ImageDraw.Draw(m)
        y0, y1 = 1e9, -1e9
        for (cx, cy, r) in puntos:
            dm.ellipse([s(cx - r), s(cy - r), s(cx + r), s(cy + r)], fill=255)
            y0, y1 = min(y0, cy - r), max(y1, cy + r)
        m = m.filter(ImageFilter.GaussianBlur(0.8 * K))
        # sombra lavanda debajo
        sm = m.filter(ImageFilter.GaussianBlur(6 * K)).point(lambda v: v * 70 // 255)
        sombra_c = Image.new("RGBA", capa.size, (110, 120, 190, 0)); sombra_c.putalpha(sm)
        desp = Image.new("RGBA", capa.size, (0, 0, 0, 0)); desp.paste(sombra_c, (0, s(8)))
        capa = Image.alpha_composite(capa, desp)
        # color: degradé vertical dentro de la nube
        tt = np.clip((yy - y0) / max(1.0, (y1 - y0)), 0, 1)[..., None]
        col = np.array(base_col, np.float32) * (1 - tt ** 1.6) + np.array(sombra_col, np.float32) * tt ** 1.6
        cimg = Image.fromarray(np.clip(col, 0, 255).astype(np.uint8)).convert("RGBA")
        cimg.putalpha(m)
        capa = Image.alpha_composite(capa, cimg)
        if brillo:  # un borde de luz arriba de cada bola
            b = Image.new("L", capa.size, 0)
            db = ImageDraw.Draw(b)
            for (cx, cy, r) in puntos:
                db.ellipse([s(cx - r * 0.75), s(cy - r * 0.92), s(cx + r * 0.35), s(cy - r * 0.2)], fill=110)
            b = Image.composite(b.filter(ImageFilter.GaussianBlur(2.5 * K)), Image.new("L", capa.size, 0), m)
            w_ = Image.new("RGBA", capa.size, (255, 255, 255, 0)); w_.putalpha(b)
            capa = Image.alpha_composite(capa, w_)
        return capa

    # nubes lejanas (chicas, pálidas) y pajaritos a lo lejos
    lejos = Image.new("RGBA", im.size, (0, 0, 0, 0))
    lejos = nube(lejos, [(330, 120, 14), (346, 112, 18), (366, 118, 14), (380, 124, 10)], sombra_col=(200, 214, 244), brillo=False)
    lejos = nube(lejos, [(176, 150, 10), (190, 144, 14), (206, 150, 10)], sombra_col=(200, 214, 244), brillo=False)
    lejos = nube(lejos, [(60, 170, 12), (76, 164, 16), (94, 170, 12)], sombra_col=(200, 214, 244), brillo=False)
    lejos.putalpha(lejos.getchannel("A").point(lambda v: v * 200 // 255))
    im = Image.alpha_composite(im, lejos)
    d = ImageDraw.Draw(im)
    for (bx, by, bw) in ((300, 92, 7), (314, 100, 5), (150, 108, 6)):
        d.arc([s(bx - bw), s(by - 3), s(bx), s(by + 3)], 200, 340, fill=(70, 96, 150, 220), width=s(1.2))
        d.arc([s(bx), s(by - 3), s(bx + bw), s(by + 3)], 200, 340, fill=(70, 96, 150, 220), width=s(1.2))

    # el arcoíris en perspectiva (bandas de color, brillo al centro de cada una, borde con luz)
    C = [(240, 80, 110), (255, 138, 60), (255, 216, 74), (108, 216, 108), (69, 208, 224), (76, 136, 240), (154, 98, 224)]
    hw = ARCO_HW0 + (yy - ARCO_HY) * ARCO_HWK
    u = (xx - P / 2) / np.maximum(hw, 1)
    dentro = (np.abs(u) <= 1) & (yy >= ARCO_HY - 6)
    k = np.clip((u + 1) / 2 * 7, 0, 6.999)
    b = k.astype(np.int32)
    fr = k - b
    cols = np.array(C, np.float32)
    col = cols[b]
    sig = cols[np.clip(b + 1, 0, 6)]
    mezcla = np.clip((fr - 0.82) / 0.18, 0, 1)[..., None]
    col = col * (1 - mezcla) + sig * mezcla
    col = col + (np.clip(1 - np.abs(fr - 0.45) / 0.45, 0, 1) ** 2)[..., None] * 38  # brillo al centro de cada banda
    col = col + (np.clip(1 - np.abs(u) / 0.25, 0, 1) ** 2)[..., None] * 18            # y una luz por el medio del tobogán
    al = np.clip(1 - (np.abs(u) - 0.96) / 0.04, 0, 1) * np.clip((yy - (ARCO_HY - 6)) / 40.0, 0, 1)
    arco = np.zeros((G, G, 4), np.float32)
    arco[..., :3] = np.clip(col, 0, 255)
    arco[..., 3] = np.where(dentro, al * 255, 0)
    arco_im = Image.fromarray(arco.astype(np.uint8), "RGBA")
    # resplandor alrededor
    glow = Image.fromarray((np.where(np.abs(u) <= 1.12, 1, 0) * (yy >= ARCO_HY) * 160).astype(np.uint8), "L").filter(ImageFilter.GaussianBlur(9 * K))
    gl = Image.new("RGBA", im.size, (255, 250, 240, 0)); gl.putalpha(glow)
    im = Image.alpha_composite(im, gl)
    im = Image.alpha_composite(im, arco_im)
    # rayas de luz finas a lo largo de las bandas (el brillo del tobogán) y destellos
    luz = Image.new("RGBA", im.size, (0, 0, 0, 0))
    dl = ImageDraw.Draw(luz)
    for uu in (-0.62, -0.05, 0.48):
        pts = [(s(P / 2 + uu * (ARCO_HW0 + (y - ARCO_HY) * ARCO_HWK)), s(y)) for y in range(ARCO_HY + 20, P + 1, 6)]
        dl.line(pts, fill=(255, 255, 255, 70), width=s(2))
    rs = np.random.RandomState(31)
    for _ in range(26):
        y = rs.uniform(ARCO_HY + 30, P - 10)
        uu = rs.uniform(-0.9, 0.9)
        x = P / 2 + uu * (ARCO_HW0 + (y - ARCO_HY) * ARCO_HWK)
        r = rs.uniform(1.0, 2.2) * (0.6 + y / P)
        dl.line([(s(x - r * 2), s(y)), (s(x + r * 2), s(y))], fill=(255, 255, 255, 235), width=s(1.1))
        dl.line([(s(x), s(y - r * 2)), (s(x), s(y + r * 2))], fill=(255, 255, 255, 235), width=s(1.1))
        dl.ellipse([s(x - r * 0.6), s(y - r * 0.6), s(x + r * 0.6), s(y + r * 0.6)], fill=(255, 255, 255, 255))
    im = Image.alpha_composite(im, luz)
    # la nube brillante de donde nace (arriba, tapa la punta)
    nace = Image.new("RGBA", im.size, (0, 0, 0, 0))
    nace = nube(nace, [(200, 66, 16), (218, 56, 22), (240, 52, 24), (262, 58, 20), (280, 66, 14), (232, 70, 18)], sombra_col=(236, 222, 250))
    im = Image.alpha_composite(im, nace)
    # nubes grandes a los lados (el tobogán pasa entre ellas)
    lados = Image.new("RGBA", im.size, (0, 0, 0, 0))
    lados = nube(lados, [(8, 236, 40), (52, 220, 34), (86, 238, 28), (40, 262, 32), (108, 254, 18)])
    lados = nube(lados, [(460, 200, 40), (414, 190, 32), (386, 210, 24), (440, 230, 34), (366, 222, 14)])
    lados = nube(lados, [(20, 330, 30), (60, 318, 26), (92, 334, 22), (50, 348, 26)])
    lados = nube(lados, [(450, 318, 36), (408, 332, 26), (376, 344, 20), (430, 350, 28)])
    im = Image.alpha_composite(im, lados)
    # el mar de nubes abajo (el tobogán se pierde hacia abajo entre ellas, a los lados)
    mar = Image.new("RGBA", im.size, (0, 0, 0, 0))
    mar = nube(mar, [(10, 440, 46), (64, 424, 38), (110, 446, 34), (150, 470, 30)], sombra_col=(206, 196, 238))
    mar = nube(mar, [(456, 436, 48), (404, 424, 38), (360, 448, 32), (318, 472, 28)], sombra_col=(206, 196, 238))
    im = Image.alpha_composite(im, mar)
    # chispitas sueltas en el cielo
    d = ImageDraw.Draw(im)
    for _ in range(18):
        x, y = rs.uniform(20, P - 20), rs.uniform(20, 300)
        if abs(x - P / 2) < ARCO_HW0 + (y - ARCO_HY) * ARCO_HWK + 20:
            continue
        r = rs.uniform(1.2, 2.4)
        d.line([(s(x - r * 2), s(y)), (s(x + r * 2), s(y))], fill=(255, 255, 255, 170), width=s(0.8))
        d.line([(s(x), s(y - r * 2)), (s(x), s(y + r * 2))], fill=(255, 255, 255, 170), width=s(0.8))
    return final(im, "19_arcoiris.png")


if __name__ == "__main__":
    import sys
    if len(sys.argv) > 1:  # (1.0.31) uno solo: python herramientas/hacer_fondos_juegos.py arcoiris
        globals()[sys.argv[1]]()
    else:
        pingpong(); cuerda(); memorice(); escondidas(); faro(); buceo(); arcoiris()
