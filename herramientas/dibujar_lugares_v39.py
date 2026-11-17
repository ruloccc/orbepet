# (v39) Fondos con más detalle para las zonas nuevas: el claro de las luciérnagas, la casita de Tilo y el mirador de las nubes.
# Dibuja fondos_pantallas/5_casita_tilo.png, 6_mirador.png y 7_claro.png (466x466). Antes guarda los anteriores en
# fondos_pantallas/antes_v39/ (por si los habías retocado). Las cosas que se tocan quedan donde las espera el juego.
# Después: python herramientas/hacer_pantallas.py
import math, random, os, shutil
from PIL import Image, ImageDraw, ImageFilter
RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
K = 3; W = 466 * K
def P(*v): return [x * K for x in v]
def hx(s): return tuple(int(s[i:i + 2], 16) for i in (1, 3, 5))
def mezcla(a, b, t): return tuple(int(a[i] * (1 - t) + b[i] * t) for i in range(3))
def grad(d, box, c0, c1):
    x0, y0, x1, y1 = box
    c0 = hx(c0) if isinstance(c0, str) else c0; c1 = hx(c1) if isinstance(c1, str) else c1
    for y in range(y0 * K, y1 * K):
        d.line([(x0 * K, y), (x1 * K, y)], fill=mezcla(c0, c1, (y - y0 * K) / max(1, (y1 - y0) * K)))
def capa(): return Image.new('RGBA', (W, W), (0, 0, 0, 0))
def pegar(im, c, blur=0):
    if blur: c = c.filter(ImageFilter.GaussianBlur(blur * K))
    im.alpha_composite(c)
def luz(im, cx, cy, r, col, a=110):
    c = capa(); d = ImageDraw.Draw(c)
    for k in range(12, 0, -1):
        rr = r * k / 12
        d.ellipse(P(cx - rr, cy - rr, cx + rr, cy + rr), fill=hx(col) + (int(a * (1 - k / 13) / 3),))
    pegar(im, c, 2)
def hojas(d, cx, cy, r, c0, c1, n=26, seed=0):
    rnd = random.Random(seed)
    for _ in range(n):
        a = rnd.random() * 6.283; rr = r * math.sqrt(rnd.random())
        x, y = cx + math.cos(a) * rr, cy + math.sin(a) * rr * 0.8
        s = rnd.uniform(.18, .34) * r
        d.ellipse(P(x - s, y - s, x + s, y + s), fill=mezcla(hx(c0), hx(c1), (y - cy + r) / (2 * r)))
def pasto(d, x0, x1, y0, y1, c0, c1, n, seed):
    rnd = random.Random(seed)
    for _ in range(n):
        x = rnd.uniform(x0, x1); y = rnd.uniform(y0, y1); h = rnd.uniform(4, 10)
        d.line(P(x, y, x + rnd.uniform(-3, 3), y - h), fill=mezcla(hx(c0), hx(c1), rnd.random()), width=max(1, int(1.4 * K)))
def flor(d, x, y, col, r=4):
    for a in range(0, 360, 72):
        px, py = x + math.cos(math.radians(a)) * r, y + math.sin(math.radians(a)) * r
        d.ellipse(P(px - r * .7, py - r * .7, px + r * .7, py + r * .7), fill=hx(col))
    d.ellipse(P(x - r * .5, y - r * .5, x + r * .5, y + r * .5), fill=hx('#FFE27A'))
def hongo(d, x, y, col, s=1.0):
    d.rounded_rectangle(P(x - 3 * s, y - 11 * s, x + 3 * s, y), radius=2 * K, fill=hx('#F4EAD8'), outline=hx('#5A3A28'), width=K)
    d.chord(P(x - 13 * s, y - 24 * s, x + 13 * s, y - 4 * s), 180, 360, fill=hx(col), outline=hx('#3A2418'), width=2 * K)
    for dx, dy in ((-6, -15), (2, -18), (7, -13)): d.ellipse(P(x + dx * s - 2, y + dy * s - 2, x + dx * s + 2, y + dy * s + 2), fill=(255, 246, 230))
def terminar(im):
    return im.convert('RGB').filter(ImageFilter.SMOOTH).resize((466, 466), Image.LANCZOS)

# ---------------- el claro de las luciérnagas ----------------
def claro():
    rnd = random.Random(39)
    im = Image.new('RGBA', (W, W)); d = ImageDraw.Draw(im)
    grad(d, (0, 0, 466, 250), '#F2C890', '#B8D8C8')                 # el cielo del atardecer, entre las copas
    luz(im, 233, 150, 150, '#FFE8A8', 160)
    c = capa(); dc = ImageDraw.Draw(c)                              # el bosque lejano, azulado
    for x in range(-30, 500, 30):
        h = rnd.randint(120, 180); dc.ellipse(P(x - 36, 250 - h, x + 36, 250 - h + 140), fill=(70, 110, 96, 255))
    pegar(im, c, 1.2)
    c = capa(); dc = ImageDraw.Draw(c)
    for x in range(-40, 520, 44):
        h = rnd.randint(90, 140); hojas(dc, x, 250 - h + 60, 56, '#2E6A4A', '#1E4A36', 30, x)
    pegar(im, c)
    for x in range(10, 470, 70):                                     # rayos de luz que se cuelan
        c = capa(); dc = ImageDraw.Draw(c)
        dc.polygon(P(x, 0, x + 26, 0, x + 120, 300, x + 70, 300), fill=(255, 240, 190, 34))
        pegar(im, c, 3)
    grad(d, (0, 236, 466, 466), '#6AAE5E', '#2E6A3A')               # el pasto
    c = capa(); dc = ImageDraw.Draw(c)
    dc.ellipse(P(30, 246, 436, 446), fill=(130, 196, 110, 255)); dc.ellipse(P(80, 276, 386, 426), fill=(150, 210, 120, 255))
    pegar(im, c, 4)
    pasto(d, 20, 446, 250, 460, '#3E8A44', '#8ACB6A', 900, 1)
    # los árboles grandes de los lados (lo esconden)
    for x0, y0, esp in ((-40, 150, False), (436, 140, True)):
        d.rectangle(P(x0 + 16, y0, x0 + 62, y0 + 200), fill=hx('#5A3A22'))
        for yy in range(y0, y0 + 200, 14): d.arc(P(x0 + 18, yy, x0 + 60, yy + 12), 200, 340, fill=hx('#3A2414'), width=K)
        c = capa(); dc = ImageDraw.Draw(c); hojas(dc, x0 + 40, y0 - 30, 100, '#3E8A4A', '#1E4A30', 60, x0); pegar(im, c)
    # el árbol viejo, con la puertita redonda de la casita de Tilo
    d.polygon(P(292, 336, 306, 140, 404, 140, 418, 336), fill=hx('#7A5232'))
    for xx in range(312, 400, 9): d.line(P(xx, 150, xx + rnd.uniform(-5, 5), 330), fill=hx('#5E3E24'), width=K)
    d.line(P(292, 336, 306, 140), fill=hx('#3A2414'), width=3 * K); d.line(P(418, 336, 404, 140), fill=hx('#3A2414'), width=3 * K)
    for cx, cy in ((300, 330), (412, 332), (284, 340), (424, 342)):    # raíces
        d.ellipse(P(cx - 16, cy - 6, cx + 16, cy + 9), fill=hx('#6A4428'), outline=hx('#3A2414'), width=2 * K)
    c = capa(); dc = ImageDraw.Draw(c)
    hojas(dc, 355, 80, 120, '#4EA05A', '#245A36', 80, 7)
    pegar(im, c)
    d.ellipse(P(326, 208, 346, 230), fill=hx('#3A2414'))              # un hueco con un búho dormido... o no
    d.chord(P(338, 258, 386, 336), 180, 360, fill=hx('#C8925A'), outline=hx('#3A2414'), width=3 * K)   # la puertita
    d.rectangle(P(338, 296, 386, 330), fill=hx('#C8925A'), outline=hx('#3A2414'), width=3 * K)
    for xx in (350, 362, 374): d.line(P(xx, 264, xx, 330), fill=hx('#9A6A3A'), width=K)
    d.ellipse(P(372, 300, 379, 307), fill=hx('#FFE27A'))
    d.rounded_rectangle(P(352, 272, 372, 286), radius=6 * K, fill=hx('#FFE9A0'), outline=hx('#3A2414'), width=2 * K)  # ventanita con luz
    d.line(P(362, 272, 362, 286), fill=hx('#3A2414'), width=K)
    luz(im, 362, 280, 30, '#FFE27A', 120)
    d = ImageDraw.Draw(im)
    d.ellipse(P(332, 328, 392, 340), fill=hx('#5A3A22'))              # el escalón
    for i, (x, y) in enumerate(((330, 338), (322, 352), (310, 366), (292, 380), (268, 392))):   # piedritas del caminito
        d.ellipse(P(x - 9, y - 4, x + 9, y + 4), fill=hx('#C8C0B0'), outline=hx('#8A8070'), width=K)
    d.line(P(372, 250, 372, 256), fill=hx('#3A2414'), width=K)       # el letrero chiquito (sin nombre)
    d.rounded_rectangle(P(360, 238, 386, 250), radius=3 * K, fill=hx('#D8B07A'), outline=hx('#3A2414'), width=K)
    for cx in (366, 372, 378): d.ellipse(P(cx - 1.5, 243, cx + 1.5, 246), fill=hx('#7A4A24'))
    # hongos, flores, helechos
    for x, y, col, s in ((96, 330, '#E86A5A', 1.0), (140, 356, '#FFD45C', .8), (226, 404, '#E86A5A', 1.1), (284, 352, '#C8A8FF', .8), (190, 300, '#FF9FBC', .6), (60, 360, '#E86A5A', .7)):
        hongo(d, x, y, col, s)
    for _ in range(40):
        x, y = rnd.uniform(60, 410), rnd.uniform(280, 440)
        flor(d, x, y, rnd.choice(('#FFF4DC', '#FFB8D8', '#C8A8FF', '#9FD8F0')), rnd.uniform(2, 3.5))
    for x, y in ((40, 300), (440, 310), (250, 290)):
        for a in range(-60, 61, 20):
            d.line(P(x, y, x + math.sin(math.radians(a)) * 26, y - math.cos(math.radians(a)) * 30), fill=hx('#2E7A44'), width=2 * K)
    # el tronco caído para sentarse
    d.rounded_rectangle(P(56, 378, 172, 408), radius=14 * K, fill=hx('#8A5A34'), outline=hx('#3A2414'), width=3 * K)
    for xx in range(70, 150, 14): d.arc(P(xx, 380, xx + 20, 406), 250, 290, fill=hx('#6A4428'), width=K)
    d.ellipse(P(150, 378, 180, 408), fill=hx('#D8A870'), outline=hx('#3A2414'), width=2 * K)
    for r in (4, 8, 12): d.ellipse(P(165 - r, 393 - r, 165 + r, 393 + r), outline=hx('#A87A48'), width=K)
    d.ellipse(P(80, 372, 100, 382), fill=hx('#6AB85A'))              # musgo encima
    # luciérnagas pintadas (las que se mueven las pone el juego)
    c = capa(); dc = ImageDraw.Draw(c)
    for _ in range(26):
        x, y = rnd.uniform(40, 430), rnd.uniform(120, 360); r = rnd.uniform(3, 6)
        dc.ellipse(P(x - r, y - r, x + r, y + r), fill=(255, 240, 140, 90)); dc.ellipse(P(x - 1.2, y - 1.2, x + 1.2, y + 1.2), fill=(255, 250, 200, 255))
    pegar(im, c, .6)
    return terminar(im)

# ---------------- la casita de Tilo ----------------
def casita():
    rnd = random.Random(5)
    im = Image.new('RGBA', (W, W)); d = ImageDraw.Draw(im)
    grad(d, (0, 0, 466, 300), '#C8925A', '#A06A3A')                 # la pared de tablas
    for x in range(0, 466, 34):
        d.line(P(x, 0, x, 300), fill=hx('#7A4A28'), width=2 * K)
        for y in (40, 150, 262): d.ellipse(P(x + 15, y, x + 19, y + 4), fill=hx('#6A3A20'))
        for _ in range(3):
            yy = rnd.uniform(10, 290); d.arc(P(x + 6, yy, x + 26, yy + 10), 200, 340, fill=hx('#9A6A3A'), width=K)
    d.rectangle(P(0, 0, 466, 22), fill=hx('#6A4428'))                 # la viga del techo
    for x in (60, 200, 340): d.rectangle(P(x, 0, x + 16, 30), fill=hx('#5A3A22'))
    grad(d, (0, 300, 466, 466), '#8A5A34', '#4A2E1A')               # el piso
    for y in range(312, 466, 20): d.line(P(0, y, 466, y), fill=hx('#3A2214'), width=K)
    for y in range(312, 466, 20):
        for x in range(rnd.randint(0, 60), 466, 110): d.line(P(x, y, x, y + 20), fill=hx('#3A2214'), width=K)
    d.rectangle(P(0, 294, 466, 304), fill=hx('#6A4428'))
    # la ventana redonda con el claro afuera y una luz tibia
    d.ellipse(P(170, 36, 296, 162), fill=hx('#F2D8A0'))
    grad(d, (176, 42, 290, 100), '#F2C890', '#B8D8C8')
    hojas(d, 233, 132, 60, '#5DB27A', '#2E6A3A', 24, 3)
    for x, y in ((196, 120), (262, 124)): d.ellipse(P(x - 3, y - 3, x + 3, y + 3), fill=hx('#FFF3B0'))
    c = capa(); dc = ImageDraw.Draw(c); dc.rectangle(P(0, 0, 466, 466), fill=(0, 0, 0, 0)); pegar(im, c)
    d.ellipse(P(166, 32, 300, 166), outline=hx('#5A3A22'), width=7 * K)
    d.line(P(233, 36, 233, 162), fill=hx('#5A3A22'), width=4 * K); d.line(P(170, 99, 296, 99), fill=hx('#5A3A22'), width=4 * K)
    for x in (166, 270): d.polygon(P(x, 40, x + 30, 40, x + 22, 170, x - 6, 170), fill=hx('#E8A0A8'))   # cortinitas
    luz(im, 233, 99, 120, '#FFE8B0', 70)
    d = ImageDraw.Draw(im)
    # la repisa con frascos y la tetera
    d.rectangle(P(30, 118, 158, 126), fill=hx('#6A4428'), outline=hx('#3A2418'), width=2 * K)
    for i, c0 in enumerate(('#FF9FBC', '#9FE0FF', '#FFE27A', '#B6F0A0', '#C8A8FF')):
        x = 36 + i * 24; d.rounded_rectangle(P(x, 92, x + 16, 118), radius=4 * K, fill=hx(c0), outline=hx('#3A2418'), width=2 * K)
        d.rectangle(P(x + 3, 88, x + 13, 93), fill=hx('#A87A48'))
    # las hierbas colgando
    for i in range(5):
        x = 324 + i * 26; d.line(P(x, 22, x, 46), fill=hx('#5A3A22'), width=2 * K)
        hojas(d, x, 56, 12, '#7AC86A', '#3E7A3A', 10, i)
    # el horno de barro (izquierda; su boca en 100, 256)
    d.chord(P(26, 184, 174, 334), 180, 360, fill=hx('#D8885A'), outline=hx('#5A2A18'), width=4 * K)
    d.rectangle(P(26, 258, 174, 304), fill=hx('#C8784A'), outline=hx('#5A2A18'), width=4 * K)
    for y in range(266, 300, 12):
        for x in range(32, 170, 22): d.rectangle(P(x + (y // 12 % 2) * 11, y, x + 18 + (y // 12 % 2) * 11, y + 9), outline=hx('#A85A34'), width=K)
    d.chord(P(66, 222, 134, 292), 180, 360, fill=hx('#2A120A'))
    luz(im, 100, 258, 40, '#FF9A3A', 160)
    d = ImageDraw.Draw(im)
    d.ellipse(P(80, 246, 120, 270), fill=hx('#FF9A3A')); d.ellipse(P(88, 252, 112, 266), fill=hx('#FFE27A'))
    d.rectangle(P(90, 150, 110, 190), fill=hx('#B8683A'), outline=hx('#5A2A18'), width=2 * K)   # la chimenea
    d.rounded_rectangle(P(52, 176, 148, 190), radius=4 * K, fill=hx('#9A9AA8'), outline=hx('#3A3A48'), width=2 * K)   # la bandeja
    for i in range(4): x = 62 + i * 20; d.ellipse(P(x, 164, x + 16, 178), fill=hx('#2A1A12'), outline=hx('#120A06'), width=K)
    for i in range(3): x = 72 + i * 22; d.line(P(x, 158, x + 5, 142), fill=hx('#C8C0B8'), width=2 * K)
    d.rounded_rectangle(P(150, 300, 176, 322), radius=4 * K, fill=hx('#8A5A34'), outline=hx('#3A2414'), width=2 * K)  # leña
    for y in (304, 312): d.ellipse(P(152, y, 160, y + 8), fill=hx('#D8A870'))
    # el gancho vacío con la silueta de la cuchara (en 326, 196)
    d.ellipse(P(320, 140, 332, 152), fill=hx('#5A5A6A'))
    for t in range(0, 360, 18):
        a = math.radians(t); d.ellipse(P(326 + 14 * math.cos(a) - 1, 178 + 18 * math.sin(a) - 1, 326 + 14 * math.cos(a) + 1, 178 + 18 * math.sin(a) + 1), fill=hx('#FFF2D0'))
    for y in range(196, 252, 8): d.line(P(326, y, 326, y + 4), fill=hx('#FFF2D0'), width=2 * K)
    # un cuadrito torcido con un dibujo de las criaturas
    d.rounded_rectangle(P(380, 120, 440, 170), radius=4 * K, fill=hx('#F6E8C4'), outline=hx('#6A4428'), width=3 * K)
    for x, y, c0 in ((396, 150, '#6AB85A'), (412, 146, '#FFE27A'), (426, 152, '#9FD8F0')): d.ellipse(P(x - 5, y - 5, x + 5, y + 5), fill=hx(c0))
    # la camita chiquita (derecha, abajo; en 365, 318)
    d.rounded_rectangle(P(298, 300, 432, 364), radius=10 * K, fill=hx('#9A6A3A'), outline=hx('#3A2418'), width=3 * K)
    d.rounded_rectangle(P(298, 276, 314, 364), radius=6 * K, fill=hx('#8A5A30'), outline=hx('#3A2418'), width=3 * K)
    d.rounded_rectangle(P(304, 296, 426, 336), radius=14 * K, fill=hx('#F4ECD8'), outline=hx('#B89A6A'), width=2 * K)
    d.ellipse(P(312, 290, 354, 316), fill=hx('#FFF4DC'), outline=hx('#B89A6A'), width=2 * K)
    d.ellipse(P(326, 298, 342, 308), fill=hx('#E8D8B8'))
    for x in range(316, 420, 16): d.line(P(x, 336, x, 362), fill=hx('#7A4A28'), width=K)
    # la mesa con las tazas (en 220, 342)
    d.ellipse(P(150, 328, 292, 356), fill=hx('#B8844A'), outline=hx('#3A2418'), width=3 * K)
    d.rectangle(P(214, 352, 226, 392), fill=hx('#8A5A30'), outline=hx('#3A2418'), width=2 * K)
    d.ellipse(P(196, 386, 244, 398), fill=hx('#7A4A28'))
    for i, (c0, s) in enumerate((('#FF9FBC', 1.2), ('#9FE0FF', 1.0), ('#FFE27A', 1.1), ('#B6F0A0', .9), ('#C8A8FF', 1.0), ('#FFFFFF', .55))):
        x = 166 + i * 20
        d.rounded_rectangle(P(x, 336 - 12 * s, x + 12 * s, 340), radius=3 * K, fill=hx(c0), outline=hx('#3A2418'), width=K)
    d.polygon(P(258, 330, 282, 326, 284, 338, 260, 342), fill=hx('#F6E8C4'), outline=hx('#8A6A4A'))   # la hoja doblada
    # la alfombra redonda tejida
    d.ellipse(P(56, 396, 236, 446), fill=hx('#C8505A'), outline=hx('#8A2A3A'), width=2 * K)
    for r in (70, 50, 30): d.ellipse(P(146 - r, 421 - r * .28, 146 + r, 421 + r * .28), outline=hx('#FFD8A0'), width=K)
    return terminar(im)

# ---------------- el mirador de las nubes ----------------
def mirador():
    rnd = random.Random(37)
    im = Image.new('RGBA', (W, W)); d = ImageDraw.Draw(im)
    grad(d, (0, 0, 466, 260), '#0A0E2A', '#3A2F6A')
    grad(d, (0, 260, 466, 466), '#3A2F6A', '#1A2048')
    c = capa(); dc = ImageDraw.Draw(c)                               # la vía láctea
    for _ in range(500):
        t = rnd.random(); x = t * 466; y = 40 + t * 120 + rnd.gauss(0, 18)
        r = rnd.choice((.6, .8, 1.0)); dc.ellipse(P(x - r, y - r, x + r, y + r), fill=(220, 210, 255, rnd.randint(60, 160)))
    pegar(im, c, .4)
    d = ImageDraw.Draw(im)
    for _ in range(180):
        x, y = rnd.uniform(0, 466), rnd.uniform(0, 300); r = rnd.choice((.8, 1, 1, 1.4, 2))
        d.ellipse(P(x - r, y - r, x + r, y + r), fill=(255, 246, 208))
    for x, y in ((60, 60), (150, 30), (410, 200), (300, 40)):
        d.line(P(x - 6, y, x + 6, y), fill=(255, 250, 220), width=K); d.line(P(x, y - 6, x, y + 6), fill=(255, 250, 220), width=K)
    luz(im, 362, 66, 60, '#FFF3C8', 120)                              # la luna
    d = ImageDraw.Draw(im)
    d.ellipse(P(334, 38, 392, 96), fill=hx('#FFF3C8')); d.ellipse(P(350, 32, 406, 88), fill=hx('#141A44'))
    # el mundo allá abajo: el bosque, el lago con su brillo, la isla y su faro, la cueva
    c = capa(); dc = ImageDraw.Draw(c)
    dc.ellipse(P(-60, 288, 526, 420), fill=(30, 58, 62, 255))
    for x in range(-10, 480, 16): dc.polygon(P(x, 322, x + 8, 296 + rnd.randint(0, 14), x + 16, 322), fill=(22, 46, 48, 255))
    dc.ellipse(P(140, 318, 330, 364), fill=(46, 76, 128, 255))
    for i in range(7): dc.line(P(150 + i * 26, 346, 164 + i * 26, 346), fill=(120, 150, 220, 255), width=K)
    dc.ellipse(P(216, 330, 262, 346), fill=(60, 48, 34, 255))
    dc.rectangle(P(244, 316, 248, 334), fill=(210, 200, 186, 255)); dc.ellipse(P(240, 310, 252, 320), fill=(255, 226, 122, 255))
    dc.polygon(P(60, 318, 92, 290, 124, 318), fill=(54, 50, 74, 255)); dc.ellipse(P(84, 306, 100, 318), fill=(20, 20, 30, 255))
    for x, y in ((180, 300), (300, 296), (360, 304), (120, 296)):
        dc.ellipse(P(x - 2, y - 2, x + 2, y + 2), fill=(255, 226, 122, 220))   # lucecitas de las casas y faroles
    pegar(im, c, .5)
    luz(im, 246, 314, 22, '#FFE27A', 140)
    d = ImageDraw.Draw(im)
    # nubes grandes y suaves (la nubecita que baja, a la izquierda: en 112, 196)
    for (cx, cy, s, a) in ((112, 196, 1.15, 255), (388, 150, .85, 235), (250, 108, .6, 210), (440, 250, .6, 200)):
        c = capa(); dc = ImageDraw.Draw(c)
        for dx, dy, r in ((-34, 8, 24), (-8, -8, 30), (22, -2, 26), (40, 10, 20), (8, 14, 24), (-20, 16, 20)):
            dc.ellipse(P(cx + (dx - r) * s, cy + (dy - r) * s, cx + (dx + r) * s, cy + (dy + r) * s), fill=(212, 220, 248, a))
        dc.ellipse(P(cx - 50 * s, cy + 8 * s, cx + 52 * s, cy + 30 * s), fill=(150, 162, 216, a))
        pegar(im, c, .5)
        c = capa(); dc = ImageDraw.Draw(c)
        dc.ellipse(P(cx - 24 * s, cy - 30 * s, cx + 10 * s, cy - 8 * s), fill=(255, 255, 255, 120)); pegar(im, c, 2)
    d = ImageDraw.Draw(im)
    for i in range(3): d.ellipse(P(150 + i * 14, 236 + i * 16, 170 + i * 14, 248 + i * 16), fill=hx('#B8C2EC'))   # escaloncitos de nube
    # la cima de la colina (abajo)
    c = capa(); dc = ImageDraw.Draw(c)
    dc.ellipse(P(10, 300, 456, 580), fill=(46, 90, 58, 255)); dc.ellipse(P(60, 312, 406, 520), fill=(58, 108, 66, 255))
    pegar(im, c, 1)
    d = ImageDraw.Draw(im)
    d.arc(P(10, 300, 456, 580), 180, 360, fill=hx('#1A3020'), width=4 * K)
    pasto(d, 50, 420, 320, 460, '#2E6A3A', '#7AB86A', 500, 4)
    for _ in range(26): flor(d, rnd.uniform(70, 400), rnd.uniform(340, 450), rnd.choice(('#FFF4DC', '#C8A8FF', '#9FD8F0')), rnd.uniform(1.8, 2.8))
    # la piedra lisa con el hueco gastado (en 235, 360)
    d.ellipse(P(166, 328, 304, 396), fill=hx('#8A8A9A'), outline=hx('#3A3A4A'), width=3 * K)
    d.ellipse(P(180, 332, 290, 372), fill=hx('#A2A2B2'))
    d.ellipse(P(212, 344, 256, 364), fill=hx('#6E6E80'))
    d.ellipse(P(190, 334, 236, 348), fill=hx('#C2C2D0'))
    for _ in range(14): x, y = rnd.uniform(176, 296), rnd.uniform(350, 390); d.ellipse(P(x - 2, y - 1, x + 2, y + 1), fill=hx('#7A7A8A'))
    d.ellipse(P(170, 380, 196, 394), fill=hx('#6AB85A')); d.ellipse(P(276, 382, 300, 394), fill=hx('#6AB85A'))   # musgo
    # el arbustito con la corona de hojas enganchada (en 384, 276)
    d.line(P(364, 344, 360, 270), fill=hx('#4A3020'), width=5 * K); d.line(P(361, 296, 388, 272), fill=hx('#4A3020'), width=3 * K)
    hojas(d, 352, 276, 26, '#3E8A4A', '#1E5030', 16, 2); hojas(d, 396, 290, 20, '#3E8A4A', '#1E5030', 12, 3)
    for a in range(0, 360, 30):
        x = 386 + 14 * math.cos(math.radians(a)); y = 276 + 7 * math.sin(math.radians(a))
        d.ellipse(P(x - 4.5, y - 3, x + 4.5, y + 3), fill=hx(('#C8A040', '#E0B850', '#A87A30')[a // 30 % 3]))
    # el sendero que baja
    for i, (x, y) in enumerate(((110, 420), (128, 404), (150, 392), (174, 384))):
        d.ellipse(P(x - 10, y - 4, x + 10, y + 4), fill=hx('#B8B0A0'), outline=hx('#6A6058'), width=K)
    return terminar(im)

if __name__ == '__main__':
    carpeta = os.path.join(RAIZ, 'fondos_pantallas')
    os.makedirs(os.path.join(carpeta, 'antes_v39'), exist_ok=True)
    for nombre, fn in (('5_casita_tilo.png', casita), ('6_mirador.png', mirador), ('7_claro.png', claro)):
        ruta = os.path.join(carpeta, nombre)
        if os.path.exists(ruta) and not os.path.exists(os.path.join(carpeta, 'antes_v39', nombre)): shutil.copy(ruta, os.path.join(carpeta, 'antes_v39', nombre))
        fn().save(ruta); print('dibujado', nombre)
