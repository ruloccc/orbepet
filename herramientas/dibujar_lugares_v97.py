# (v97) El mirador de día (antes estaba dibujado de noche y a mediodía se veía con luna) y la casita de Peumo con más
# detalle y luz. Las cosas que se tocan quedan donde las espera el juego. La noche y el atardecer los pone el juego
# tiñendo la paleta (como en los demás fondos de campo), y las estrellas del mirador las pone el juego encima.
# Después: python herramientas/hacer_pantallas.py
import math, random, os, sys
from PIL import Image, ImageDraw, ImageFilter
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from dibujar_lugares_v39 import K, W, P, hx, mezcla, grad, capa, pegar, luz, hojas, pasto, flor, terminar, RAIZ

def nube(im, cx, cy, s, a=255, sombra=(176, 196, 232), cuerpo=(252, 253, 255)):
    c = capa(); dc = ImageDraw.Draw(c)
    dc.ellipse(P(cx - 50 * s, cy + 8 * s, cx + 52 * s, cy + 30 * s), fill=sombra + (a,))
    for dx, dy, r in ((-34, 8, 24), (-8, -8, 30), (22, -2, 26), (40, 10, 20), (8, 14, 24), (-20, 16, 20)):
        dc.ellipse(P(cx + (dx - r) * s, cy + (dy - r) * s, cx + (dx + r) * s, cy + (dy + r) * s), fill=cuerpo + (a,))
    pegar(im, c, .4)
    c = capa(); dc = ImageDraw.Draw(c)     # la sombra azulada de abajo, suave
    dc.ellipse(P(cx - 46 * s, cy + 12 * s, cx + 50 * s, cy + 32 * s), fill=sombra + (int(a * .8),))
    pegar(im, c, 2.2)
    c = capa(); dc = ImageDraw.Draw(c)     # el brillo de arriba
    dc.ellipse(P(cx - 26 * s, cy - 32 * s, cx + 8 * s, cy - 10 * s), fill=(255, 255, 255, 170)); pegar(im, c, 2)

# ---------------- el mirador de las nubes, de día ----------------
def mirador():
    rnd = random.Random(97)
    im = Image.new('RGBA', (W, W)); d = ImageDraw.Draw(im)
    grad(d, (0, 0, 466, 180), '#6FB4EC', '#A8D6F6')                  # el cielo alto
    grad(d, (0, 180, 466, 300), '#A8D6F6', '#E6F4FF')                # hacia el horizonte, más claro
    luz(im, 372, 70, 120, '#FFF6D0', 150)                              # el sol (el disco lo deja el brillo)
    c = capa(); dc = ImageDraw.Draw(c)
    dc.ellipse(P(350, 48, 394, 92), fill=(255, 250, 225, 255)); pegar(im, c, 1.5)
    # la cordillera lejana, con nieve
    c = capa(); dc = ImageDraw.Draw(c)
    pts = [(-10, 300)]
    x = -10
    while x < 480:
        x += rnd.randint(26, 44); pts.append((x, 238 + rnd.randint(0, 34)))
    pts += [(480, 300)]
    dc.polygon(P(*[v for q in pts for v in q]), fill=(150, 160, 204, 255))
    for (px, py) in pts[1:-1]:
        if py < 262: dc.polygon(P(px - 12, py + 12, px, py, px + 12, py + 12, px + 5, py + 9, px, py + 13, px - 5, py + 9), fill=(246, 248, 255, 255))
    pegar(im, c, .8)
    c = capa(); dc = ImageDraw.Draw(c)                                # la bruma del valle
    dc.rectangle(P(0, 262, 466, 292), fill=(232, 242, 255, 90)); pegar(im, c, 6)
    # el mundo allá abajo (de día, con bruma): el bosque, la laguna con la isla y su faro, el prado, el río
    c = capa(); dc = ImageDraw.Draw(c)
    dc.ellipse(P(-60, 280, 526, 420), fill=(108, 168, 112, 255))
    dc.ellipse(P(-40, 286, 200, 340), fill=(64, 124, 84, 255))       # el bosque (izquierda)
    for x in range(-20, 200, 12): dc.ellipse(P(x, 288 + rnd.randint(0, 8), x + 16, 304 + rnd.randint(0, 8)), fill=(52, 108, 72, 255))
    dc.ellipse(P(150, 300, 330, 346), fill=(86, 150, 210, 255))      # la laguna
    for i in range(7): dc.line(P(162 + i * 22, 324, 176 + i * 22, 324), fill=(200, 230, 255, 255), width=K)
    dc.ellipse(P(222, 314, 262, 330), fill=(122, 160, 92, 255))      # la isla
    dc.rectangle(P(244, 300, 249, 318), fill=(246, 240, 230, 255)); dc.rectangle(P(244, 306, 249, 309), fill=(220, 80, 80, 255))
    dc.polygon(P(242, 300, 246.5, 294, 251, 300), fill=(220, 80, 80, 255))
    dc.line(P(330, 322, 380, 300, 420, 296, 470, 300), fill=(120, 180, 230, 255), width=3 * K)   # el río
    dc.ellipse(P(340, 284, 420, 304), fill=(150, 206, 120, 255))      # el prado
    for x, y in ((360, 292), (380, 290), (398, 294)): dc.ellipse(P(x - 2, y - 2, x + 2, y + 2), fill=(255, 240, 200, 255))
    dc.polygon(P(70, 306, 100, 282, 130, 306), fill=(120, 116, 150, 255)); dc.ellipse(P(92, 296, 108, 306), fill=(50, 48, 64, 255))  # la cueva
    pegar(im, c, .7)
    c = capa(); dc = ImageDraw.Draw(c); dc.rectangle(P(0, 276, 466, 300), fill=(236, 246, 255, 40)); pegar(im, c, 4)   # bruma encima
    d = ImageDraw.Draw(im)
    # pajaritos lejanos
    for x, y in ((150, 70), (166, 64), (300, 120)):
        d.arc(P(x - 6, y - 3, x, y + 3), 200, 340, fill=hx('#4A5A7A'), width=K); d.arc(P(x, y - 3, x + 6, y + 3), 200, 340, fill=hx('#4A5A7A'), width=K)
    # las nubes (la que baja, la grande de la izquierda: en 112, 196) y otras de fondo
    for (cx, cy, s, a) in ((250, 108, .6, 230), (440, 250, .6, 220), (388, 150, .85, 245), (112, 196, 1.15, 255)):
        nube(im, cx, cy, s, a)
    d = ImageDraw.Draw(im)
    for i in range(3): d.ellipse(P(150 + i * 14, 236 + i * 16, 170 + i * 14, 248 + i * 16), fill=hx('#E8EEFF'), outline=hx('#B8C6EC'), width=K)   # escaloncitos de nube
    # la cima del cerro (abajo), con su borde de pasto
    c = capa(); dc = ImageDraw.Draw(c)
    dc.ellipse(P(10, 300, 456, 580), fill=(78, 150, 76, 255)); dc.ellipse(P(60, 312, 406, 520), fill=(104, 178, 88, 255))
    pegar(im, c, 1)
    c = capa(); dc = ImageDraw.Draw(c); dc.ellipse(P(120, 330, 346, 460), fill=(150, 210, 110, 140)); pegar(im, c, 8)   # luz del sol
    d = ImageDraw.Draw(im)
    d.arc(P(10, 300, 456, 580), 180, 360, fill=hx('#2E5A30'), width=4 * K)
    for x in range(30, 440, 9):                                        # matitas en el borde
        a = math.pi * (1 - (x - 10) / 446.0); y = 440 - 140 * math.sin(a) + 2
        if y < 455: d.line(P(x, y, x + rnd.uniform(-2, 2), y - rnd.uniform(5, 10)), fill=hx('#3E7A3A'), width=2 * K)
    pasto(d, 50, 420, 320, 460, '#3E8A44', '#9ACB6A', 600, 4)
    for _ in range(34): flor(d, rnd.uniform(70, 400), rnd.uniform(340, 450), rnd.choice(('#FFF4DC', '#C8A8FF', '#9FD8F0', '#FFB8D8', '#FFE27A')), rnd.uniform(1.8, 2.9))
    # la roca del cóndor (izquierda, donde él se para: 92, 322)
    d.polygon(P(52, 338, 62, 314, 84, 304, 112, 308, 128, 326, 124, 342), fill=hx('#8E8AA0'), outline=hx('#4A4660'), width=3 * K)
    d.polygon(P(66, 318, 84, 309, 106, 312, 98, 320, 74, 324), fill=hx('#B2AEC4'))
    for x, y in ((80, 332), (104, 330)): d.arc(P(x - 8, y - 3, x + 8, y + 5), 200, 340, fill=hx('#5E5A74'), width=K)
    # la piedra lisa y tibia, con el hueco gastado (en 235, 360)
    c = capa(); dc = ImageDraw.Draw(c); dc.ellipse(P(160, 372, 310, 404), fill=(30, 60, 30, 110)); pegar(im, c, 3)   # su sombra
    d = ImageDraw.Draw(im)
    d.ellipse(P(166, 328, 304, 396), fill=hx('#9A98AC'), outline=hx('#46445A'), width=3 * K)
    d.ellipse(P(180, 332, 290, 372), fill=hx('#B4B2C4'))
    d.ellipse(P(212, 344, 256, 364), fill=hx('#7E7C92'))
    d.ellipse(P(190, 334, 236, 348), fill=hx('#D4D2E2'))
    for _ in range(14): x, y = rnd.uniform(176, 296), rnd.uniform(350, 390); d.ellipse(P(x - 2, y - 1, x + 2, y + 1), fill=hx('#86849A'))
    d.ellipse(P(170, 380, 196, 394), fill=hx('#6AB85A')); d.ellipse(P(276, 382, 300, 394), fill=hx('#6AB85A'))   # musgo
    # el arbustito con la corona de hojas enganchada (en 384, 276)
    d.line(P(364, 344, 360, 270), fill=hx('#5A3A24'), width=5 * K); d.line(P(361, 296, 388, 272), fill=hx('#5A3A24'), width=3 * K)
    hojas(d, 352, 276, 26, '#5AAA5A', '#2A6A3A', 16, 2); hojas(d, 396, 290, 20, '#5AAA5A', '#2A6A3A', 12, 3)
    for a in range(0, 360, 30):
        x = 386 + 14 * math.cos(math.radians(a)); y = 276 + 7 * math.sin(math.radians(a))
        d.ellipse(P(x - 4.5, y - 3, x + 4.5, y + 3), fill=hx(('#C8A040', '#E0B850', '#A87A30')[a // 30 % 3]))
    # el sendero que baja
    for x, y in ((110, 420), (128, 404), (150, 392), (174, 384)):
        d.ellipse(P(x - 10, y - 4, x + 10, y + 4), fill=hx('#C8C0B0'), outline=hx('#7A7068'), width=K)
    return terminar(im)

# ---------------- la casita de Peumo, más tibia y con más cositas ----------------
def casita():
    rnd = random.Random(97)
    im = Image.new('RGBA', (W, W)); d = ImageDraw.Draw(im)
    # la pared de tablas, cada tabla con su tono
    for i, x in enumerate(range(0, 466, 34)):
        t = rnd.uniform(-.08, .08)
        c0 = mezcla(hx('#CC9660'), hx('#A06A3A'), .2 + t); c1 = mezcla(hx('#B07842'), hx('#8A5A30'), .4 + t)
        grad(d, (x, 0, x + 34, 300), c0, c1)
        d.line(P(x, 0, x, 300), fill=hx('#6A3E20'), width=2 * K)
        d.line(P(x + 2, 0, x + 2, 300), fill=mezcla(c0, (255, 230, 190), .25), width=K)
        for y in (40, 150, 262): d.ellipse(P(x + 15, y, x + 19, y + 4), fill=hx('#5A3018'))
        for _ in range(3):
            yy = rnd.uniform(10, 290); d.arc(P(x + 6, yy, x + 26, yy + 10), 200, 340, fill=mezcla(c1, (60, 30, 10), .35), width=K)
    d.rectangle(P(0, 0, 466, 22), fill=hx('#6A4428'))                 # la viga del techo
    d.line(P(0, 22, 466, 22), fill=hx('#3A2214'), width=2 * K)
    for x in (60, 200, 340): d.rectangle(P(x, 0, x + 16, 30), fill=hx('#5A3A22'), outline=hx('#3A2214'), width=K)
    # banderines tejidos colgando de la viga
    for i in range(12):
        x0 = 18 + i * 36; yv = 26 + 8 * math.sin(i / 11 * math.pi)
        col = ('#E8A0A8', '#FFE27A', '#9FD8C8', '#C8A8FF')[i % 4]
        d.polygon(P(x0, yv, x0 + 22, yv + 1, x0 + 11, yv + 18), fill=hx(col), outline=hx('#7A4A2A'))
    d.line(P(10, 26, 456, 26), fill=hx('#7A4A2A'), width=K)
    # el piso de tablas, con brillo
    grad(d, (0, 300, 466, 466), '#946238', '#4E301C')
    for y in range(312, 466, 20):
        d.line(P(0, y, 466, y), fill=hx('#3A2214'), width=K); d.line(P(0, y + 2, 466, y + 2), fill=hx('#A8764A'), width=K)
        for x in range(rnd.randint(0, 60), 466, 110): d.line(P(x, y, x, y + 20), fill=hx('#3A2214'), width=K)
    d.rectangle(P(0, 294, 466, 304), fill=hx('#6A4428')); d.line(P(0, 304, 466, 304), fill=hx('#3A2214'), width=K)
    # la ventana redonda con el claro afuera
    d.ellipse(P(170, 36, 296, 162), fill=hx('#F2D8A0'))
    grad(d, (176, 42, 290, 100), '#F6CC90', '#C2E2D2')
    hojas(d, 233, 132, 60, '#5DB27A', '#2E6A3A', 24, 3)
    for x, y in ((196, 120), (262, 124), (230, 108)): d.ellipse(P(x - 3, y - 3, x + 3, y + 3), fill=hx('#FFF3B0'))
    d.ellipse(P(166, 32, 300, 166), outline=hx('#5A3A22'), width=7 * K)
    d.line(P(233, 36, 233, 162), fill=hx('#5A3A22'), width=4 * K); d.line(P(170, 99, 296, 99), fill=hx('#5A3A22'), width=4 * K)
    d.rounded_rectangle(P(176, 160, 290, 172), radius=4 * K, fill=hx('#8A5A34'), outline=hx('#3A2214'), width=2 * K)   # el alféizar
    d.rounded_rectangle(P(196, 146, 216, 162), radius=4 * K, fill=hx('#C8784A'), outline=hx('#5A2A18'), width=K)      # una macetita
    hojas(d, 206, 140, 9, '#6AC86A', '#2E7A3A', 8, 9)
    for x in (166, 270): d.polygon(P(x, 40, x + 30, 40, x + 22, 170, x - 6, 170), fill=hx('#E8A0A8'))   # cortinitas
    for x in (166, 270):
        for k in range(3): d.line(P(x + 6 + k * 8, 44, x + 4 + k * 7, 166), fill=hx('#D08890'), width=K)
    # la luz tibia de la ventana sobre el piso
    c = capa(); dc = ImageDraw.Draw(c)
    dc.polygon(P(176, 120, 290, 120, 360, 466, 110, 466), fill=(255, 236, 190, 36)); pegar(im, c, 8)
    luz(im, 233, 99, 120, '#FFE8B0', 70)
    d = ImageDraw.Draw(im)
    # la repisa con frascos (cada uno con su etiqueta) y la tetera
    d.rectangle(P(30, 118, 158, 126), fill=hx('#6A4428'), outline=hx('#3A2418'), width=2 * K)
    for x in (40, 146): d.polygon(P(x, 126, x + 4, 126, x + 4, 138, x, 134), fill=hx('#5A3A22'))
    for i, c0 in enumerate(('#FF9FBC', '#9FE0FF', '#FFE27A', '#B6F0A0', '#C8A8FF')):
        x = 36 + i * 24; d.rounded_rectangle(P(x, 92, x + 16, 118), radius=4 * K, fill=hx(c0), outline=hx('#3A2418'), width=2 * K)
        d.rectangle(P(x + 3, 88, x + 13, 93), fill=hx('#A87A48'), outline=hx('#5A3A22'), width=K)
        d.rectangle(P(x + 3, 102, x + 13, 109), fill=hx('#FFF8E8'))
        d.line(P(x + 4, 96, x + 4, 112), fill=(255, 255, 255), width=K)
    # las hierbas colgando, atadas con cordel
    for i in range(5):
        x = 324 + i * 26; d.line(P(x, 22, x, 46), fill=hx('#8A6A44'), width=2 * K)
        hojas(d, x, 56, 12, ('#7AC86A', '#9AD87A', '#6AB86A', '#C8A8FF', '#7AC86A')[i], '#3E7A3A', 10, i)
        d.rectangle(P(x - 3, 44, x + 3, 48), fill=hx('#E8A0A8'))
    # el horno de barro (izquierda; su boca en 100, 256)
    c = capa(); dc = ImageDraw.Draw(c); dc.ellipse(P(16, 318, 190, 346), fill=(40, 20, 10, 110)); pegar(im, c, 3)
    d = ImageDraw.Draw(im)
    d.chord(P(26, 184, 174, 334), 180, 360, fill=hx('#D8885A'), outline=hx('#5A2A18'), width=4 * K)
    d.arc(P(36, 192, 164, 324), 200, 260, fill=hx('#F0A878'), width=3 * K)
    d.rectangle(P(26, 258, 174, 304), fill=hx('#C8784A'), outline=hx('#5A2A18'), width=4 * K)
    for y in range(266, 300, 12):
        for x in range(32, 170, 22): d.rectangle(P(x + (y // 12 % 2) * 11, y, x + 18 + (y // 12 % 2) * 11, y + 9), outline=hx('#A85A34'), width=K)
    d.chord(P(66, 222, 134, 292), 180, 360, fill=hx('#2A120A'))
    luz(im, 100, 258, 46, '#FF9A3A', 170)
    d = ImageDraw.Draw(im)
    d.ellipse(P(80, 246, 120, 270), fill=hx('#FF9A3A')); d.ellipse(P(88, 252, 112, 266), fill=hx('#FFE27A'))
    for x in (84, 100, 114): d.line(P(x, 268, x + 8, 262), fill=hx('#6A3A20'), width=2 * K)   # leñitas adentro
    d.rectangle(P(90, 150, 110, 190), fill=hx('#B8683A'), outline=hx('#5A2A18'), width=2 * K)   # la chimenea
    d.rounded_rectangle(P(52, 176, 148, 190), radius=4 * K, fill=hx('#9A9AA8'), outline=hx('#3A3A48'), width=2 * K)   # la bandeja
    for i in range(4): x = 62 + i * 20; d.ellipse(P(x, 164, x + 16, 178), fill=hx('#2A1A12'), outline=hx('#120A06'), width=K)
    for i in range(3): x = 72 + i * 22; d.line(P(x, 158, x + 5, 142), fill=hx('#C8C0B8'), width=2 * K)
    d.rounded_rectangle(P(150, 300, 176, 322), radius=4 * K, fill=hx('#8A5A34'), outline=hx('#3A2414'), width=2 * K)  # leña
    for y in (304, 312): d.ellipse(P(152, y, 160, y + 8), fill=hx('#D8A870'))
    # el gancho vacío con la silueta de la cuchara (en 326, 196)
    d.ellipse(P(320, 140, 332, 152), fill=hx('#5A5A6A'), outline=hx('#2A2A36'), width=K)
    for t in range(0, 360, 18):
        a = math.radians(t); d.ellipse(P(326 + 14 * math.cos(a) - 1, 178 + 18 * math.sin(a) - 1, 326 + 14 * math.cos(a) + 1, 178 + 18 * math.sin(a) + 1), fill=hx('#FFF2D0'))
    for y in range(196, 252, 8): d.line(P(326, y, 326, y + 4), fill=hx('#FFF2D0'), width=2 * K)
    # un cuadrito torcido con un dibujo de las criaturas
    d.polygon(P(378, 122, 440, 118, 444, 168, 382, 172), fill=hx('#F6E8C4'), outline=hx('#6A4428'))
    d.line(P(378, 122, 440, 118, 444, 168, 382, 172, 378, 122), fill=hx('#6A4428'), width=3 * K)
    for x, y, c0 in ((396, 150, '#6AB85A'), (412, 146, '#FFE27A'), (426, 152, '#9FD8F0')): d.ellipse(P(x - 5, y - 5, x + 5, y + 5), fill=hx(c0))
    d.arc(P(390, 128, 432, 150), 200, 340, fill=hx('#E86A5A'), width=K)       # un arcoíris de lápiz
    # una lamparita de aceite en la pared (luz dorada)
    d.rounded_rectangle(P(436, 214, 452, 226), radius=3 * K, fill=hx('#C8A040'), outline=hx('#6A4A1A'), width=K)
    d.ellipse(P(440, 200, 448, 214), fill=hx('#FFE27A'))
    luz(im, 444, 206, 30, '#FFD27A', 130)
    d = ImageDraw.Draw(im)
    # la camita chiquita (derecha, abajo; en 365, 318)
    c = capa(); dc = ImageDraw.Draw(c); dc.ellipse(P(290, 352, 440, 376), fill=(40, 20, 10, 110)); pegar(im, c, 3)
    d = ImageDraw.Draw(im)
    d.rounded_rectangle(P(298, 300, 432, 364), radius=10 * K, fill=hx('#9A6A3A'), outline=hx('#3A2418'), width=3 * K)
    d.rounded_rectangle(P(298, 276, 314, 364), radius=6 * K, fill=hx('#8A5A30'), outline=hx('#3A2418'), width=3 * K)
    d.ellipse(P(300, 270, 312, 282), fill=hx('#C8925A'), outline=hx('#3A2418'), width=K)
    d.rounded_rectangle(P(304, 296, 426, 336), radius=14 * K, fill=hx('#F4ECD8'), outline=hx('#B89A6A'), width=2 * K)
    for x in range(318, 420, 18): d.line(P(x, 300, x - 4, 334), fill=hx('#E2D4B4'), width=K)
    d.ellipse(P(312, 290, 354, 316), fill=hx('#FFF4DC'), outline=hx('#B89A6A'), width=2 * K)
    d.ellipse(P(326, 298, 342, 308), fill=hx('#E8D8B8'))
    for x in range(316, 420, 16): d.line(P(x, 336, x, 362), fill=hx('#7A4A28'), width=K)
    # la mesa con las tazas (en 220, 342)
    c = capa(); dc = ImageDraw.Draw(c); dc.ellipse(P(176, 384, 266, 404), fill=(40, 20, 10, 100)); pegar(im, c, 3)
    d = ImageDraw.Draw(im)
    d.ellipse(P(150, 328, 292, 356), fill=hx('#B8844A'), outline=hx('#3A2418'), width=3 * K)
    d.arc(P(160, 330, 282, 350), 200, 330, fill=hx('#D8A870'), width=K)
    d.rectangle(P(214, 352, 226, 392), fill=hx('#8A5A30'), outline=hx('#3A2418'), width=2 * K)
    d.ellipse(P(196, 386, 244, 398), fill=hx('#7A4A28'))
    for i, (c0, s) in enumerate((('#FF9FBC', 1.2), ('#9FE0FF', 1.0), ('#FFE27A', 1.1), ('#B6F0A0', .9), ('#C8A8FF', 1.0), ('#FFFFFF', .55))):
        x = 166 + i * 20
        d.rounded_rectangle(P(x, 336 - 12 * s, x + 12 * s, 340), radius=3 * K, fill=hx(c0), outline=hx('#3A2418'), width=K)
    d.polygon(P(258, 330, 282, 326, 284, 338, 260, 342), fill=hx('#F6E8C4'), outline=hx('#8A6A4A'))   # la hoja doblada
    # la alfombra redonda tejida, con flecos
    d.ellipse(P(56, 396, 236, 446), fill=hx('#C8505A'), outline=hx('#8A2A3A'), width=2 * K)
    for r, col in ((70, '#FFD8A0'), (50, '#FFF0D0'), (30, '#FFD8A0')): d.ellipse(P(146 - r, 421 - r * .28, 146 + r, 421 + r * .28), outline=hx(col), width=K)
    for t in range(0, 360, 12):
        a = math.radians(t); x = 146 + 92 * math.cos(a); y = 421 + 26 * math.sin(a)
        d.line(P(x, y, x + 4 * math.cos(a), y + 3 * math.sin(a)), fill=hx('#E8A0A8'), width=K)
    # una plantita en maceta junto a la cama
    d.rounded_rectangle(P(436, 330, 462, 360), radius=5 * K, fill=hx('#C8784A'), outline=hx('#5A2A18'), width=2 * K)
    hojas(d, 449, 318, 16, '#6AC86A', '#2E7A3A', 12, 11)
    # un poco de penumbra en las esquinas (más acogedor)
    import numpy as np
    yy, xx = np.mgrid[0:W, 0:W]
    r = np.hypot(xx - W * .5, yy - W * .48) / (W * .5)
    al = (np.clip((r - .62) / .38, 0, 1) ** 1.6 * 120).astype(np.uint8)
    v = np.zeros((W, W, 4), np.uint8); v[..., 0] = 30; v[..., 1] = 14; v[..., 2] = 6; v[..., 3] = al
    im.alpha_composite(Image.fromarray(v, 'RGBA'))
    return terminar(im)

if __name__ == '__main__':
    carpeta = os.path.join(RAIZ, 'fondos_pantallas')
    os.makedirs(os.path.join(carpeta, 'antes_v97'), exist_ok=True)
    import shutil
    for nombre, fn in (('5_casita_tilo.png', casita), ('6_mirador.png', mirador)):
        ruta = os.path.join(carpeta, nombre)
        if os.path.exists(ruta) and not os.path.exists(os.path.join(carpeta, 'antes_v97', nombre)): shutil.copy(ruta, os.path.join(carpeta, 'antes_v97', nombre))
        fn().save(ruta); print('dibujado', nombre)
