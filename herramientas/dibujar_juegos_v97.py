# (v97) Fondos de los juegos de los lugares secretos, cada uno en su pantalla aparte (466x466):
#   8  las nubes (subir de nube en nube)     9  el viento (atrapar las hojas de la corona)
#   10 la piedra tibia (frío o caliente)       11 el horno (avivar el fuego)       12 la camita (tenderla)
# Lo que se mueve (nubes, hojas, fuego, mantita, almohada) lo pone el juego encima. Después: python herramientas/hacer_pantallas.py
import math, random, os, sys
from PIL import Image, ImageDraw, ImageFilter
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from dibujar_lugares_v39 import K, W, P, hx, mezcla, grad, capa, pegar, luz, hojas, pasto, flor, terminar, RAIZ
from dibujar_lugares_v97 import nube

def nubes():
    rnd = random.Random(8)
    im = Image.new('RGBA', (W, W)); d = ImageDraw.Draw(im)
    grad(d, (0, 0, 466, 240), '#3E7EDA', '#7DBAF0')
    grad(d, (0, 240, 466, 466), '#7DBAF0', '#D8EEFF')
    luz(im, 92, 76, 130, '#FFF6D0', 150)
    c = capa(); dc = ImageDraw.Draw(c); dc.ellipse(P(70, 54, 114, 98), fill=(255, 250, 225, 255)); pegar(im, c, 1.5)
    # capas de nubes lejanas, suaves (dan altura)
    c = capa(); dc = ImageDraw.Draw(c)                                # bancos de bruma lejanos (muy suaves: las nubes del juego van encima)
    for y0 in (150, 260, 370):
        for k in range(5):
            x = rnd.uniform(-40, 506); w = rnd.uniform(90, 160)
            dc.ellipse(P(x - w, y0 - 14, x + w, y0 + 14), fill=(255, 255, 255, 60))
    pegar(im, c, 8)
    # abajo, la cima del mirador desde donde se empieza
    c = capa(); dc = ImageDraw.Draw(c)
    dc.ellipse(P(-40, 430, 506, 640), fill=(78, 150, 76, 255)); dc.ellipse(P(30, 438, 436, 600), fill=(104, 178, 88, 255))
    pegar(im, c, 1)
    d = ImageDraw.Draw(im)
    d.arc(P(-40, 430, 506, 640), 190, 350, fill=hx('#2E5A30'), width=4 * K)
    for _ in range(14): flor(d, rnd.uniform(110, 360), rnd.uniform(446, 462), rnd.choice(('#FFF4DC', '#FFB8D8', '#FFE27A')), 2)
    return terminar(im)

def viento():
    rnd = random.Random(9)
    im = Image.new('RGBA', (W, W)); d = ImageDraw.Draw(im)
    grad(d, (0, 0, 466, 260), '#7EC0F0', '#D6EEFF')
    luz(im, 380, 70, 100, '#FFF6D0', 120)
    for (cx, cy, s, a) in ((120, 90, .7, 220), (330, 150, .55, 200)): nube(im, cx, cy, s, a)
    # la cordillera
    c = capa(); dc = ImageDraw.Draw(c)
    pts = [(-10, 300)]; x = -10
    while x < 480: x += rnd.randint(30, 50); pts.append((x, 220 + rnd.randint(0, 30)))
    pts.append((480, 300)); dc.polygon(P(*[v for q in pts for v in q]), fill=(160, 170, 210, 255)); pegar(im, c, .8)
    # el pasto del cerro, de cerca, doblado por el viento
    grad(d, (0, 250, 466, 466), '#7CC46A', '#3E8A44')
    c = capa(); dc = ImageDraw.Draw(c); dc.ellipse(P(-60, 236, 526, 330), fill=(124, 196, 106, 255)); pegar(im, c, 3)
    d = ImageDraw.Draw(im)
    for _ in range(1400):
        x, y = rnd.uniform(-10, 476), rnd.uniform(262, 470); h = rnd.uniform(6, 16) * (0.6 + (y - 260) / 260)
        d.line(P(x, y, x - h * .55, y - h), fill=mezcla(hx('#2E7A3A'), hx('#A8DA7A'), rnd.random()), width=max(1, int(1.3 * K)))
    for _ in range(26): flor(d, rnd.uniform(20, 446), rnd.uniform(300, 450), rnd.choice(('#FFF4DC', '#C8A8FF', '#FFB8D8', '#FFE27A')), rnd.uniform(2.2, 3.4))
    # el arbustito de la corona, a la izquierda, sacudido por el viento
    d.line(P(70, 470, 76, 330), fill=hx('#5A3A24'), width=7 * K); d.line(P(74, 380, 30, 340), fill=hx('#5A3A24'), width=4 * K); d.line(P(75, 360, 118, 318), fill=hx('#5A3A24'), width=4 * K)
    c = capa(); dc = ImageDraw.Draw(c)
    hojas(dc, 70, 310, 54, '#5AAA5A', '#2A6A3A', 30, 5); hojas(dc, 120, 300, 30, '#6ABA5A', '#2A6A3A', 16, 6); hojas(dc, 26, 330, 26, '#5AAA5A', '#2A6A3A', 14, 7)
    pegar(im, c)
    # líneas de viento
    c = capa(); dc = ImageDraw.Draw(c)
    for k in range(6):
        y = 40 + k * 34 + rnd.uniform(-6, 6); x0 = rnd.uniform(100, 300)
        dc.arc(P(x0, y, x0 + 140, y + 30), 200, 340, fill=(255, 255, 255, 200), width=3 * K)
        dc.arc(P(x0 + 120, y - 10, x0 + 150, y + 16), 270, 90, fill=(255, 255, 255, 170), width=3 * K)
    pegar(im, c, .5)
    return terminar(im)

def piedra():
    rnd = random.Random(10)
    im = Image.new('RGBA', (W, W)); d = ImageDraw.Draw(im)
    grad(d, (0, 0, 466, 466), '#7CC46A', '#4E9A50')                 # el pasto alrededor (visto desde arriba)
    for _ in range(2200):
        x, y = rnd.uniform(0, 466), rnd.uniform(0, 466); h = rnd.uniform(4, 9)
        d.line(P(x, y, x + rnd.uniform(-3, 3), y - h), fill=mezcla(hx('#2E7A3A'), hx('#B2E07A'), rnd.random()), width=max(1, int(1.2 * K)))
    for _ in range(40): flor(d, rnd.uniform(10, 456), rnd.uniform(10, 456), rnd.choice(('#FFF4DC', '#C8A8FF', '#9FD8F0', '#FFB8D8')), rnd.uniform(2.2, 3.6))
    # la sombra y la piedra grande
    c = capa(); dc = ImageDraw.Draw(c); dc.ellipse(P(56, 128, 420, 404), fill=(20, 50, 20, 130)); pegar(im, c, 6)
    d = ImageDraw.Draw(im)
    d.ellipse(P(46, 100, 420, 384), fill=hx('#9A98AC'), outline=hx('#46445A'), width=4 * K)
    c = capa(); dc = ImageDraw.Draw(c)
    dc.ellipse(P(66, 110, 400, 350), fill=(184, 182, 198, 255)); dc.ellipse(P(96, 118, 300, 220), fill=(214, 212, 228, 255))
    pegar(im, c, 6)
    d = ImageDraw.Draw(im)
    for _ in range(180):                                               # granito
        x, y = rnd.uniform(70, 396), rnd.uniform(120, 366)
        if ((x - 233) / 180) ** 2 + ((y - 242) / 136) ** 2 < 1:
            r = rnd.uniform(1, 2.6); d.ellipse(P(x - r, y - r * .7, x + r, y + r * .7), fill=rnd.choice((hx('#86849A'), hx('#A8A6BA'), hx('#CFCDE0'))))
    for x, y, a0 in ((120, 300, 200), (300, 160, 20), (330, 290, 120)):  # grietitas
        d.arc(P(x - 30, y - 10, x + 30, y + 14), a0, a0 + 70, fill=hx('#6E6C82'), width=K)
    c = capa(); dc = ImageDraw.Draw(c); dc.ellipse(P(176, 210, 290, 270), fill=(110, 108, 130, 160)); pegar(im, c, 5)   # el hueco gastado
    d = ImageDraw.Draw(im)
    for x, y, r in ((92, 300, 22), (352, 186, 16), (310, 336, 18), (150, 150, 12)):   # musgo y líquenes
        hojas(d, x, y, r, '#7ACC5A', '#3A8A3A', 10, int(x))
    for x, y in ((210, 320), (260, 140), (130, 220)): d.ellipse(P(x - 7, y - 5, x + 7, y + 5), fill=hx('#E8D27A'))
    return terminar(im)

def horno():
    rnd = random.Random(11)
    im = Image.new('RGBA', (W, W)); d = ImageDraw.Draw(im)
    # la pared de tablas detrás, en penumbra cálida
    for x in range(0, 466, 34):
        grad(d, (x, 0, x + 34, 466), mezcla(hx('#9A6A40'), hx('#7A4E2C'), rnd.uniform(0, .4)), hx('#4E301C'))
        d.line(P(x, 0, x, 466), fill=hx('#3A2214'), width=2 * K)
    # el piso
    grad(d, (0, 400, 466, 466), '#6A4426', '#3A2414')
    # el horno de barro, grande
    c = capa(); dc = ImageDraw.Draw(c); dc.ellipse(P(20, 380, 446, 440), fill=(20, 10, 4, 160)); pegar(im, c, 6)
    d = ImageDraw.Draw(im)
    d.chord(P(26, 50, 440, 560), 180, 360, fill=hx('#D8885A'), outline=hx('#5A2A18'), width=6 * K)
    d.rectangle(P(26, 300, 440, 410), fill=hx('#C8784A'), outline=hx('#5A2A18'), width=6 * K)
    c = capa(); dc = ImageDraw.Draw(c); dc.ellipse(P(60, 60, 260, 220), fill=(255, 196, 140, 120)); pegar(im, c, 14)   # brillo del barro
    d = ImageDraw.Draw(im)
    for y in range(316, 404, 18):
        for x in range(34, 436, 36): d.rounded_rectangle(P(x + (y // 18 % 2) * 18, y, x + 32 + (y // 18 % 2) * 18, y + 14), radius=3 * K, outline=hx('#A85A34'), width=K)
    for _ in range(60):
        x, y = rnd.uniform(60, 400), rnd.uniform(80, 290); d.ellipse(P(x - 1.5, y - 1, x + 1.5, y + 1), fill=hx('#B86A44'))
    # la boca del horno (oscura: el fuego lo pone el juego)
    d.chord(P(113, 170, 353, 410), 180, 360, fill=hx('#3A1A0E'), outline=hx('#5A2A18'), width=5 * K)
    d.rectangle(P(113, 290, 353, 330), fill=hx('#2A120A'))
    c = capa(); dc = ImageDraw.Draw(c); dc.chord(P(128, 186, 338, 396), 180, 360, fill=(14, 6, 2, 220)); pegar(im, c, 4)
    d = ImageDraw.Draw(im)
    for x in (150, 190, 236, 280): d.rounded_rectangle(P(x, 314, x + 40, 326), radius=5 * K, fill=hx('#6A3A20'), outline=hx('#3A1A0E'), width=K)   # la leña
    d.arc(P(113, 170, 353, 410), 180, 360, fill=hx('#E8A070'), width=3 * K)
    for a in range(190, 351, 16):                                      # el arco de ladrillos de la boca
        r1, r2 = 120, 140
        x1, y1 = 233 + r1 * math.cos(math.radians(a)), 290 + r1 * math.sin(math.radians(a))
        x2, y2 = 233 + r2 * math.cos(math.radians(a)), 290 + r2 * math.sin(math.radians(a))
        d.line(P(x1, y1, x2, y2), fill=hx('#8A3A20'), width=2 * K)
    d.rounded_rectangle(P(100, 330, 366, 352), radius=6 * K, fill=hx('#B8683A'), outline=hx('#5A2A18'), width=3 * K)   # el umbral
    # la chimenea arriba y la pala de madera apoyada
    d.rectangle(P(206, 0, 260, 66), fill=hx('#B8683A'), outline=hx('#5A2A18'), width=3 * K)
    d.line(P(410, 440, 452, 200), fill=hx('#C8925A'), width=7 * K); d.ellipse(P(430, 160, 466, 216), fill=hx('#D8A870'), outline=hx('#6A4428'), width=2 * K)
    return terminar(im)

def cama():
    rnd = random.Random(12)
    im = Image.new('RGBA', (W, W)); d = ImageDraw.Draw(im)
    for x in range(0, 466, 34):                                         # la pared
        grad(d, (x, 0, x + 34, 250), mezcla(hx('#CC9660'), hx('#A06A3A'), rnd.uniform(.1, .4)), hx('#8A5A30'))
        d.line(P(x, 0, x, 250), fill=hx('#6A3E20'), width=2 * K)
    grad(d, (0, 250, 466, 466), '#946238', '#4E301C')                 # el piso
    for y in range(262, 466, 22): d.line(P(0, y, 466, y), fill=hx('#3A2214'), width=K)
    d.rectangle(P(0, 244, 466, 254), fill=hx('#6A4428'))
    # una ventanita con la luna y estrellas (es hora de dormir)
    d.rounded_rectangle(P(300, 40, 420, 150), radius=16 * K, fill=hx('#2A2A5A'), outline=hx('#5A3A22'), width=6 * K)
    for x, y in ((320, 70), (350, 100), (396, 64), (380, 126)): d.ellipse(P(x - 2, y - 2, x + 2, y + 2), fill=hx('#FFF3B0'))
    d.ellipse(P(360, 56, 396, 92), fill=hx('#FFF3C8')); d.ellipse(P(372, 50, 404, 84), fill=hx('#2A2A5A'))
    d.line(P(360, 40, 360, 150), fill=hx('#5A3A22'), width=4 * K)
    luz(im, 360, 95, 80, '#B8C8FF', 60)
    d = ImageDraw.Draw(im)
    # la cama grande (la cabecera a la izquierda)
    c = capa(); dc = ImageDraw.Draw(c); dc.ellipse(P(30, 390, 440, 430), fill=(30, 14, 6, 140)); pegar(im, c, 5)
    d = ImageDraw.Draw(im)
    d.rounded_rectangle(P(36, 150, 82, 410), radius=14 * K, fill=hx('#8A5A30'), outline=hx('#3A2418'), width=4 * K)   # la cabecera
    d.ellipse(P(42, 134, 76, 168), fill=hx('#C8925A'), outline=hx('#3A2418'), width=3 * K)
    for y in range(190, 400, 30): d.line(P(48, y, 70, y), fill=hx('#6A3E20'), width=2 * K)
    d.rounded_rectangle(P(60, 290, 434, 400), radius=18 * K, fill=hx('#9A6A3A'), outline=hx('#3A2418'), width=4 * K)  # el marco
    for x in range(90, 420, 24): d.line(P(x, 350, x, 396), fill=hx('#7A4A28'), width=K)
    d.rounded_rectangle(P(410, 260, 440, 400), radius=10 * K, fill=hx('#8A5A30'), outline=hx('#3A2418'), width=4 * K)  # los pies
    d.rounded_rectangle(P(74, 270, 420, 352), radius=26 * K, fill=hx('#F4ECD8'), outline=hx('#B89A6A'), width=3 * K)   # el colchón
    for x in range(110, 400, 34): d.line(P(x, 276, x - 8, 346), fill=hx('#E2D4B4'), width=K)
    luz(im, 250, 300, 160, '#FFF0C8', 40)
    return terminar(im)

if __name__ == '__main__':
    carpeta = os.path.join(RAIZ, 'fondos_pantallas')
    for nombre, fn in (('8_juego_nubes.png', nubes), ('9_juego_viento.png', viento), ('10_juego_piedra.png', piedra), ('11_juego_horno.png', horno), ('12_juego_cama.png', cama)):
        fn().save(os.path.join(carpeta, nombre)); print('dibujado', nombre)
