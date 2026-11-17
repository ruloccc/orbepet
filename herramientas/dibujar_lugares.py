# Dibuja los fondos de los lugares secretos (466x466): fondos_pantallas/5_casita_tilo.png, 6_mirador.png (versión 92) y 7_claro.png (v39).
# Solo dibuja los que faltan (si ya los retocaste a mano, no los pisa).
# Son una base para retocar a mano; después: python herramientas/hacer_pantallas.py
import math, random, os
RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
from PIL import Image, ImageDraw, ImageFilter
K = 2; W = 466 * K
def P(*v): return [x * K for x in v]
def grad(d, box, c0, c1):
    x0, y0, x1, y1 = box
    for y in range(y0 * K, y1 * K):
        t = (y - y0 * K) / max(1, (y1 - y0) * K)
        c = tuple(int(c0[i] * (1 - t) + c1[i] * t) for i in range(3))
        d.line([(x0 * K, y), (x1 * K, y)], fill=c)
def hx(s): return tuple(int(s[i:i + 2], 16) for i in (1, 3, 5))
def contorno(d, kind, box, fill, out='#3A2418', w=3):
    getattr(d, kind)(P(*box), fill=hx(fill) if isinstance(fill, str) else fill, outline=hx(out), width=w * K)

def casita():
    im = Image.new('RGB', (W, W)); d = ImageDraw.Draw(im)
    grad(d, (0, 0, 466, 300), hx('#C8925A'), hx('#A8703E'))           # pared de tablas
    for x in range(0, 466, 38): d.line(P(x, 0, x, 300), fill=hx('#8A5A30'), width=2 * K)
    for x in range(19, 466, 38):
        for y in (60, 170, 260): d.ellipse(P(x - 2, y - 2, x + 2, y + 2), fill=hx('#7A4A28'))
    grad(d, (0, 300, 466, 466), hx('#8A5A34'), hx('#5A3A22'))          # piso
    for y in range(310, 466, 22): d.line(P(0, y, 466, y), fill=hx('#4A2E1A'), width=K)
    d.rectangle(P(0, 296, 466, 304), fill=hx('#6A4428'))
    # ventana redonda con el bosque de día
    contorno(d, 'ellipse', (173, 40, 293, 160), '#9FD8F0', w=5)
    d.ellipse(P(178, 100, 290, 170), fill=hx('#5DB27A'))
    for cx, cy, r in ((200, 105, 22), (240, 95, 26), (275, 108, 20)): d.ellipse(P(cx - r, cy - r, cx + r, cy + r), fill=hx('#3E8A54'))
    d.line(P(233, 42, 233, 158), fill=hx('#5A3A22'), width=4 * K); d.line(P(175, 100, 291, 100), fill=hx('#5A3A22'), width=4 * K)
    contorno(d, 'ellipse', (173, 40, 293, 160), None, w=6)
    # hierbas colgando y repisa con frascos
    d.rectangle(P(40, 120, 150, 128), fill=hx('#6A4428'), outline=hx('#3A2418'), width=2 * K)
    for i, c in enumerate(('#FF9FBC', '#9FE0FF', '#FFE27A', '#B6F0A0')):
        x = 50 + i * 26; contorno(d, 'rounded_rectangle', (x, 96, x + 18, 120), c, w=2)
    for i in range(4):
        x = 330 + i * 30; d.line(P(x, 0, x, 40), fill=hx('#5A3A22'), width=2 * K)
        d.ellipse(P(x - 9, 36, x + 9, 60), fill=hx('#6AB85A'), outline=hx('#2E5A2A'), width=2 * K)
    # el horno de barro (izquierda)
    contorno(d, 'pieslice', (30, 180, 170, 330), '#D8885A', w=4) if False else None
    d.chord(P(30, 190, 170, 330), 180, 360, fill=hx('#D8885A'), outline=hx('#5A2A18'), width=4 * K)
    d.rectangle(P(30, 258, 170, 300), fill=hx('#C8784A'), outline=hx('#5A2A18'), width=4 * K)
    d.chord(P(68, 224, 132, 290), 180, 360, fill=hx('#3A1A10'))
    d.ellipse(P(80, 244, 120, 268), fill=hx('#FF9A3A'))
    d.ellipse(P(88, 250, 112, 264), fill=hx('#FFE27A'))
    # bandeja de galletas negras sobre el horno
    d.rounded_rectangle(P(56, 178, 146, 190), radius=4 * K, fill=hx('#9A9AA8'), outline=hx('#3A3A48'), width=2 * K)
    for i in range(4): x = 66 + i * 20; d.ellipse(P(x, 166, x + 16, 180), fill=hx('#2A1A12'), outline=hx('#120a06'), width=K)
    for i in range(3): x = 76 + i * 22; d.line(P(x, 160, x + 4, 146), fill=hx('#B8B0A8'), width=2 * K)  # humito
    # el gancho con la silueta de la cuchara (pared derecha)
    d.ellipse(P(320, 140, 332, 152), fill=hx('#5A5A6A'))
    for t in range(0, 360, 20):
        a = math.radians(t); d.point([(K * (326 + 14 * math.cos(a)), K * (178 + 18 * math.sin(a)))], fill=hx('#FFF2D0'))
    for y in range(196, 252, 8): d.line(P(326, y, 326, y + 4), fill=hx('#FFF2D0'), width=2 * K)
    # la camita chiquita (derecha, abajo)
    d.rounded_rectangle(P(300, 300, 430, 362), radius=10 * K, fill=hx('#9A6A3A'), outline=hx('#3A2418'), width=3 * K)
    d.rounded_rectangle(P(306, 296, 424, 336), radius=14 * K, fill=hx('#6AB85A'), outline=hx('#2E5A2A'), width=3 * K)
    d.ellipse(P(312, 292, 354, 318), fill=hx('#FFF4DC'), outline=hx('#B89A6A'), width=2 * K)
    d.ellipse(P(326, 300, 342, 310), fill=hx('#E8D8B8'))  # la forma de una cabecita
    for x in (300, 424): d.rectangle(P(x - 3, 280, x + 5, 368), fill=hx('#7A4A28'), outline=hx('#3A2418'), width=2 * K)
    # la mesita con tazas de todos los tamaños (centro)
    d.ellipse(P(150, 330, 290, 372), fill=hx('#B8784A'), outline=hx('#3A2418'), width=3 * K)
    d.rectangle(P(214, 360, 226, 420), fill=hx('#8A5A30'), outline=hx('#3A2418'), width=2 * K)
    for (x, s, c) in ((168, 20, '#FF9FBC'), (196, 16, '#9FE0FF'), (222, 24, '#FFE27A'), (252, 14, '#B48AF0'), (272, 8, '#FFFFFF')):
        d.rounded_rectangle(P(x, 344 - s, x + s * .9, 346), radius=3 * K, fill=hx(c), outline=hx('#3A2418'), width=2 * K)
    d.polygon(P(236, 352, 262, 348, 266, 360, 240, 364), fill=hx('#FBF0D2'), outline=hx('#8A6A4A'))  # la hoja doblada
    # alfombra
    d.ellipse(P(120, 400, 346, 456), fill=hx('#C84A5A'), outline=hx('#7A2A3A'), width=3 * K)
    d.ellipse(P(150, 410, 316, 446), outline=hx('#FFD76A'), width=2 * K)
    im = im.filter(ImageFilter.SMOOTH).resize((466, 466), Image.LANCZOS)
    return im

def mirador():
    random.seed(37)
    im = Image.new('RGB', (W, W)); d = ImageDraw.Draw(im)
    grad(d, (0, 0, 466, 300), hx('#0A0E2A'), hx('#3A2F6A'))
    grad(d, (0, 300, 466, 466), hx('#3A2F6A'), hx('#1A2048'))
    for _ in range(140):
        x, y = random.randint(0, 465), random.randint(0, 300); r = random.choice((1, 1, 1, 2))
        d.ellipse(P(x - r, y - r, x + r, y + r), fill=(255, 246, 208))
    d.ellipse(P(330, 40, 390, 100), fill=hx('#FFF3C8')); d.ellipse(P(346, 34, 404, 92), fill=hx('#1A1A44'))  # luna
    # el mundo allá abajo: bosque, lago con brillo, la isla con su faro
    d.ellipse(P(-40, 300, 520, 420), fill=hx('#1E3A3A'))
    for x in range(-10, 480, 22): d.polygon(P(x, 330, x + 11, 300 + random.randint(0, 14), x + 22, 330), fill=hx('#173030'))
    d.ellipse(P(150, 330, 330, 372), fill=hx('#2A4A7A'))
    d.ellipse(P(220, 342, 262, 356), fill=hx('#3A2E20'))
    d.rectangle(P(244, 330, 247, 344), fill=hx('#C8C0B0')); d.ellipse(P(240, 324, 252, 334), fill=hx('#FFE27A'))
    for i in range(6): d.line(P(160 + i * 26, 360, 172 + i * 26, 360), fill=hx('#6A8AD8'), width=K)
    # nubes grandes y suaves (la nubecita que baja, a la izquierda)
    for (cx, cy, s) in ((110, 190, 1.0), (380, 150, .8), (250, 110, .6)):
        for dx, dy, r in ((-30, 6, 22), (0, -6, 28), (30, 4, 22), (12, 14, 20), (-14, 14, 20)):
            d.ellipse(P(cx + (dx - r) * s, cy + (dy - r) * s, cx + (dx + r) * s, cy + (dy + r) * s), fill=hx('#C8D2F0'))
        d.ellipse(P(cx - 40 * s, cy + 6 * s, cx + 40 * s, cy + 26 * s), fill=hx('#9AA6D8'))
    # la cima de la colina (abajo)
    d.ellipse(P(20, 300, 446, 560), fill=hx('#2E5A3A'), outline=hx('#1A3020'), width=4 * K)
    for _ in range(60):
        x = random.randint(60, 410); y = random.randint(330, 450)
        d.line(P(x, y, x + random.randint(-3, 3), y - 7), fill=hx('#4A8A54'), width=2 * K)
    # la piedra lisa con el hueco gastado (centro)
    d.ellipse(P(170, 330, 300, 392), fill=hx('#8A8A9A'), outline=hx('#3A3A4A'), width=3 * K)
    d.ellipse(P(214, 344, 254, 362), fill=hx('#6A6A7A'))
    d.ellipse(P(186, 334, 236, 350), fill=hx('#A8A8B8'))
    # el arbustito con la corona de hojas enganchada (derecha)
    d.line(P(360, 340, 356, 270), fill=hx('#4A3020'), width=5 * K); d.line(P(357, 296, 384, 274), fill=hx('#4A3020'), width=3 * K)
    for cx, cy, r in ((340, 280, 16), (372, 266, 18), (392, 290, 14)): d.ellipse(P(cx - r, cy - r, cx + r, cy + r), fill=hx('#2E6A3A'), outline=hx('#1A3020'), width=2 * K)
    for a in range(0, 360, 40):
        x = 384 + 12 * math.cos(math.radians(a)); y = 276 + 6 * math.sin(math.radians(a))
        d.ellipse(P(x - 4, y - 3, x + 4, y + 3), fill=hx('#C8A040'))
    im = im.filter(ImageFilter.SMOOTH).resize((466, 466), Image.LANCZOS)
    return im

def claro():
    """(v39) el claro de las luciérnagas: un claro escondido del bosque, con el árbol viejo y la puertita redonda de la casita de Tilo"""
    random.seed(39)
    im = Image.new('RGB', (W, W)); d = ImageDraw.Draw(im)
    grad(d, (0, 0, 466, 240), hx('#7AB0C8'), hx('#B8D8C0'))            # el cielo, entre las copas
    for x in range(-20, 500, 34):                                       # el bosque de atrás
        h = random.randint(120, 190)
        d.ellipse(P(x - 40, 240 - h, x + 40, 240 - h + 150), fill=hx('#2E5A3A'))
    for x in range(-30, 500, 46):
        h = random.randint(90, 150)
        d.ellipse(P(x - 46, 250 - h, x + 46, 250 - h + 160), fill=hx('#24482E'))
    grad(d, (0, 230, 466, 466), hx('#5EA05A'), hx('#3A7A44'))           # el pasto del claro
    d.ellipse(P(40, 250, 426, 440), fill=hx('#6AB060'))
    d.ellipse(P(90, 280, 376, 420), fill=hx('#78BC6A'))
    for _ in range(90):
        x = random.randint(40, 426); y = random.randint(250, 450)
        d.line(P(x, y, x + random.randint(-3, 3), y - 7), fill=hx('#4A8A44'), width=2 * K)
    for x0, y0 in ((-30, 160), (440, 150)):                              # dos árboles que lo esconden, a los lados
        d.rectangle(P(x0 + 20, y0, x0 + 56, y0 + 180), fill=hx('#5A3A22'), outline=hx('#3A2414'), width=3 * K)
        d.ellipse(P(x0 - 40, y0 - 120, x0 + 120, y0 + 40), fill=hx('#2E6A3A'), outline=hx('#1A3020'), width=3 * K)
    # el árbol viejo (derecha) con la puertita redonda de la casita de Tilo
    d.polygon(P(300, 330, 312, 150, 396, 150, 410, 330), fill=hx('#7A5232'), outline=hx('#3A2414'))
    d.line(P(300, 330, 312, 150), fill=hx('#3A2414'), width=3 * K); d.line(P(410, 330, 396, 150), fill=hx('#3A2414'), width=3 * K)
    for yy in range(170, 320, 26): d.arc(P(318, yy, 360, yy + 22), 200, 340, fill=hx('#5A3A22'), width=2 * K)
    d.ellipse(P(250, 40, 460, 200), fill=hx('#2E6A3A'), outline=hx('#1A3020'), width=3 * K)
    d.ellipse(P(280, 20, 420, 130), fill=hx('#3A7A44'))
    d.chord(P(334, 262, 378, 336), 180, 360, fill=hx('#C8925A'), outline=hx('#3A2414'), width=3 * K)   # la puertita
    d.rectangle(P(334, 298, 378, 330), fill=hx('#C8925A'), outline=hx('#3A2414'), width=3 * K)
    d.line(P(356, 266, 356, 330), fill=hx('#8A5A30'), width=2 * K)
    d.ellipse(P(366, 302, 372, 308), fill=hx('#FFE27A'))
    d.ellipse(P(330, 326, 384, 338), fill=hx('#5A3A22'))
    for cx, cy in ((318, 322), (398, 326)):                              # raíces
        d.ellipse(P(cx - 14, cy - 6, cx + 14, cy + 8), fill=hx('#6A4428'), outline=hx('#3A2414'), width=2 * K)
    # hongos y florcitas
    for cx, cy, c in ((110, 330, '#E86A5A'), (150, 360, '#FFE27A'), (230, 400, '#E86A5A'), (290, 360, '#C8A8FF')):
        d.rectangle(P(cx - 3, cy - 10, cx + 3, cy), fill=hx('#F4EAD8'))
        d.chord(P(cx - 12, cy - 22, cx + 12, cy - 2), 180, 360, fill=hx(c), outline=hx('#3A2418'), width=2 * K)
    for _ in range(26):
        x = random.randint(70, 400); y = random.randint(290, 430)
        d.ellipse(P(x - 3, y - 3, x + 3, y + 3), fill=hx(random.choice(('#FFF4DC', '#FFB8D8', '#FFE27A'))))
    # un tronco caído para sentarse (izquierda)
    d.rounded_rectangle(P(60, 380, 170, 406), radius=12 * K, fill=hx('#8A5A34'), outline=hx('#3A2414'), width=3 * K)
    d.ellipse(P(150, 380, 176, 406), fill=hx('#C8925A'), outline=hx('#3A2414'), width=2 * K)
    im = im.filter(ImageFilter.SMOOTH).resize((466, 466), Image.LANCZOS)
    return im
for nombre, fn in (('5_casita_tilo.png', casita), ('6_mirador.png', mirador), ('7_claro.png', claro)):
    ruta = os.path.join(RAIZ, 'fondos_pantallas', nombre)
    if not os.path.exists(ruta): fn().save(ruta); print('dibujado', nombre)
print('listo')
