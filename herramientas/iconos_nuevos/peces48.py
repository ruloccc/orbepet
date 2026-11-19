# (1.0.32) La colección de la pesca: 30 animales del agua dibujados a 48 px (como los íconos HD), con borde oscuro.
# Uso: python3 peces48.py <salida.inc> [carpeta_vista_previa]
#   → escribe firmware/main/peces_hd.inc (paleta ARGB + índices; 0 = transparente) y una hoja de vista previa.
# El orden es el de la tabla PECES en core/peces.inc (no cambiarlo: el número es el bit guardado en la partida).
import math, os, sys
from PIL import Image, ImageDraw
S = 48
SS = 4
K = SS

def lienzo():
    im = Image.new('RGBA', (S * SS, S * SS), (0, 0, 0, 0))
    return im, ImageDraw.Draw(im)

def P(pts):
    return [(x * K, y * K) for x, y in pts]

def E(d, cx, cy, rx, ry, col):
    d.ellipse([(cx - rx) * K, (cy - ry) * K, (cx + rx) * K, (cy + ry) * K], fill=col)

def L(d, pts, col, w=1.0):
    d.line(P(pts), fill=col, width=max(1, int(w * K)), joint='curve')

def mezclar(a, b, t):
    return tuple(int(a[i] * (1 - t) + b[i] * t) for i in range(3)) + (255,)

def oscuro(c, f=0.6):
    return (int(c[0] * f), int(c[1] * f), int(c[2] * f), 255)

def claro(c, f=0.45):
    return (int(c[0] + (255 - c[0]) * f), int(c[1] + (255 - c[1]) * f), int(c[2] + (255 - c[2]) * f), 255)

def contorno(img, color):
    a = img.split()[3]
    out = Image.new('RGBA', img.size, (0, 0, 0, 0))
    px = out.load(); ap = a.load()
    W, H = img.size
    for y in range(H):
        for x in range(W):
            if ap[x, y] > 110: continue
            if any(0 <= x + i < W and 0 <= y + j < H and ap[x + i, y + j] > 110
                   for j in range(-2, 3) for i in range(-2, 3) if abs(i) + abs(j) <= 3):
                px[x, y] = color
    out.alpha_composite(img)
    return out

def acabar(im, borde=(30, 36, 60, 255)):
    im = im.resize((S, S), Image.LANCZOS)
    return contorno(im, borde)

def ojo(d, x, y, r=2.2, brillo=True):
    E(d, x, y, r, r, (255, 255, 255, 255))
    E(d, x + 0.3, y + 0.2, r * 0.62, r * 0.62, (28, 30, 44, 255))
    if brillo:
        E(d, x - 0.4, y - 0.6, r * 0.25, r * 0.25, (255, 255, 255, 255))

def pez(arriba, abajo, panza=None, largo=30, alto=16, cola='horquilla', patron=None, pcol=None, aleta=None, bigotes=False,
        boca=True, ojo_r=2.4, cx=23, cy=24, aleta_col=None, extra=None):
    """un pez de lado (mira a la izquierda): cuerpo ovalado con degradé, cola, aleta de arriba y de abajo, patrón y ojo"""
    im, d = lienzo()
    x0, x1 = cx - largo / 2, cx + largo / 2
    ac = aleta_col or oscuro(arriba, 0.8)
    # cola
    if cola == 'horquilla':
        d.polygon(P([(x1 - 3, cy), (x1 + 8, cy - alto * 0.55), (x1 + 4, cy), (x1 + 8, cy + alto * 0.55)]), fill=ac)
    elif cola == 'redonda':
        E(d, x1 + 3, cy, 5, alto * 0.42, ac)
    elif cola == 'recta':
        d.polygon(P([(x1 - 3, cy), (x1 + 7, cy - alto * 0.45), (x1 + 7, cy + alto * 0.45)]), fill=ac)
    # aletas
    d.polygon(P([(cx - largo * 0.15, cy - alto * 0.42), (cx + largo * 0.05, cy - alto * 0.85), (cx + largo * 0.28, cy - alto * 0.4)]), fill=ac)
    d.polygon(P([(cx + largo * 0.05, cy + alto * 0.4), (cx + largo * 0.18, cy + alto * 0.75), (cx + largo * 0.3, cy + alto * 0.36)]), fill=ac)
    if aleta == 'larga':
        d.polygon(P([(cx - largo * 0.3, cy - alto * 0.4), (cx, cy - alto * 1.05), (cx + largo * 0.38, cy - alto * 0.38)]), fill=ac)
    # cuerpo con degradé vertical
    cuerpo = Image.new('L', im.size, 0)
    ImageDraw.Draw(cuerpo).ellipse([x0 * K, (cy - alto / 2) * K, x1 * K, (cy + alto / 2) * K], fill=255)
    grad = Image.new('RGBA', im.size)
    gp = grad.load()
    for y in range(im.size[1]):
        t = min(1, max(0, (y / K - (cy - alto / 2)) / alto))
        c = mezclar(arriba, abajo, t)
        for x in range(im.size[0]):
            gp[x, y] = c
    im.paste(grad, (0, 0), cuerpo)
    d = ImageDraw.Draw(im)
    if panza:
        d.chord([(x0 + 3) * K, (cy - alto * 0.1) * K, (x1 - 3) * K, (cy + alto / 2 - 0.5) * K], 0, 180, fill=panza)
    # patrón (recortado al cuerpo)
    if patron:
        capa, dc = lienzo()
        c = pcol or oscuro(arriba, 0.6)
        if patron == 'franja':
            dc.rectangle([x0 * K, (cy - 1.6) * K, x1 * K, (cy + 1.6) * K], fill=c)
        if patron == 'barras':
            for i in range(5):
                xx = x0 + 6 + i * (largo - 10) / 4
                dc.rectangle([(xx - 1.4) * K, (cy - alto) * K, (xx + 1.4) * K, (cy + alto) * K], fill=c)
        if patron == 'puntos':
            for i, (px_, py_) in enumerate([(0.3, -0.2), (0.45, 0.1), (0.6, -0.25), (0.7, 0.15), (0.38, 0.28), (0.82, -0.05), (0.55, 0.32)]):
                E(dc, x0 + largo * px_, cy + alto * py_, 1.1, 1.1, c)
        if patron == 'linea':
            dc.line([((x0 + 6) * K, cy * K), ((x1 - 2) * K, cy * K)], fill=c, width=K)
        if patron == 'escamas':
            for i in range(4):
                for j in range(-1, 2):
                    xx, yy = x0 + 9 + i * 4.5, cy + j * 3.5
                    dc.arc([(xx - 2) * K, (yy - 2) * K, (xx + 2) * K, (yy + 2) * K], 90, 270, fill=c, width=K)
        recorte = Image.new('RGBA', im.size, (0, 0, 0, 0))
        recorte.paste(capa, (0, 0), Image.composite(capa.split()[3], Image.new('L', im.size, 0), cuerpo))
        im.alpha_composite(recorte)
        d = ImageDraw.Draw(im)
    # brillo arriba
    d.arc([(x0 + 4) * K, (cy - alto / 2 + 1.5) * K, (x1 - 6) * K, (cy + alto / 2) * K], 200, 260, fill=claro(arriba, 0.6), width=K)
    # agalla y boca
    d.arc([(x0 + 4) * K, (cy - alto * 0.3) * K, (x0 + 10) * K, (cy + alto * 0.3) * K], 300, 60, fill=oscuro(arriba, 0.6), width=K)
    if boca:
        L(d, [(x0 + 0.5, cy + 1.6), (x0 + 3, cy + 1.2)], oscuro(abajo, 0.5), 0.8)
    if bigotes:
        L(d, [(x0 + 1, cy + 1), (x0 - 4, cy + 6)], (60, 50, 40, 255), 0.7)
        L(d, [(x0 + 2, cy + 1.5), (x0 - 2, cy + 8)], (60, 50, 40, 255), 0.7)
        L(d, [(x0 + 1, cy - 0.5), (x0 - 5, cy - 3)], (60, 50, 40, 255), 0.7)
    ojo(d, x0 + 5, cy - alto * 0.12, ojo_r)
    if extra: extra(d)
    return im

def largo_fino(c1, c2, patron=None, boca_redonda=False, aletas=True):
    """anguila, lamprea, congrio: un cuerpo largo que ondula"""
    im, d = lienzo()
    pts = [(5 + i * 1.0, 24 + 6 * math.sin(i * 0.17)) for i in range(0, 39)]
    for i, (x, y) in enumerate(pts):
        r = 4.4 - 3.0 * (i / len(pts)) ** 2
        E(d, x, y, r, r, mezclar(c1, c2, i / len(pts)))
    if aletas:
        for i in range(8, 36, 2):
            x, y = pts[i]
            r = 4.4 - 3.0 * (i / len(pts)) ** 2
            E(d, x, y - r - 0.6, 0.9, 0.9, oscuro(c1, 0.8))
    for i in range(3, 38, 3):
        x, y = pts[i]
        if patron == 'manchas': E(d, x, y - 1, 0.9, 0.8, oscuro(c1, 0.6))
    d.arc([2 * K, 21 * K, 8 * K, 27 * K], 120, 240, fill=claro(c2, 0.4), width=K)
    if boca_redonda:
        E(d, 3.2, 25.5, 1.6, 1.6, (90, 40, 50, 255)); E(d, 3.2, 25.5, 0.8, 0.8, (210, 120, 130, 255))
    else:
        L(d, [(1.5, 25.5), (4.5, 25.5)], oscuro(c2, 0.5), 0.8)
    ojo(d, 6, 22.4, 1.7)
    return im

def cangrejo(cuerpo, patas, ojos_largos=True, pinzas=1.0, patas_largas=False, puntos=False):
    im, d = lienzo()
    lp = 19 if patas_largas else 7
    for s in (-1, 1):
        for i in range(3 if patas_largas else 4):
            x0 = 24 + s * (6 + i * 1.6)
            y0 = 28 + i * 1.2
            x1 = x0 + s * lp * 0.8
            y1 = y0 + (7 if patas_largas else 5) + i * 0.5
            L(d, [(x0, y0), ((x0 + x1) / 2 + s * 1.5, y0 - (5 if patas_largas else 1.5)), (x1, y1)], patas, 1.3 if patas_largas else 1.6)
        # pinza
        bx, by = 24 + s * 13 * (0.9 + 0.1 * pinzas), 18 - 1
        L(d, [(24 + s * 7, 25), (bx, by + 4)], patas, 2.0)
        E(d, bx, by, 4.2 * pinzas, 3.4 * pinzas, cuerpo)
        d.polygon(P([(bx + s * 1.5, by - 1), (bx + s * 5.5 * pinzas, by - 4.5 * pinzas), (bx + s * 3, by + 0.5)]), fill=(255, 255, 255, 0))
        d.pieslice([(bx - 2.5) * K, (by - 4) * K, (bx + 2.5) * K, (by + 1) * K], 250 if s < 0 else 230, 310 if s < 0 else 290, fill=(0, 0, 0, 0))
        E(d, bx - s * 1, by - 1, 1.2, 0.9, claro(cuerpo, 0.4))
    E(d, 24, 28, 11, 7.5, cuerpo)
    d.chord([14 * K, 24 * K, 34 * K, 36 * K], 0, 180, fill=oscuro(cuerpo, 0.82))
    E(d, 21, 25, 3, 1.6, claro(cuerpo, 0.4))
    if puntos:
        for (x, y) in ((19, 29), (24, 27), (29, 29), (22, 31), (27, 31)): E(d, x, y, 0.9, 0.9, claro(cuerpo, 0.6))
    if patas_largas:  # la centolla: espinitas arriba del caparazón
        for x in range(15, 34, 3): d.polygon(P([(x - 1.2, 22.5), (x, 19.2), (x + 1.2, 22.5)]), fill=oscuro(cuerpo, 0.85))
    for s in (-1, 1):
        h = 7 if ojos_largos else 4
        L(d, [(24 + s * 3, 22), (24 + s * 3.4, 22 - h + 2)], oscuro(cuerpo, 0.7), 1.1)
        ojo(d, 24 + s * 3.4, 22 - h + 1, 1.7)
    L(d, [(21.5, 30.5), (24, 31.8), (26.5, 30.5)], (60, 20, 20, 255), 0.7)
    return im

def todos():
    R = []
    # 0 TRUCHA ARCOÍRIS
    R.append(pez((104, 132, 88), (236, 230, 210), patron='franja', pcol=(240, 120, 150, 255), aleta_col=(120, 140, 96, 255),
                 extra=lambda d: [E(d, 14 + i * 4, 20 + (i % 2) * 2, 0.7, 0.7, (50, 60, 40, 255)) for i in range(6)]))
    # 1 PEJERREY
    R.append(pez((150, 176, 196), (236, 242, 248), largo=32, alto=11, patron='linea', pcol=(210, 225, 245, 255), aleta_col=(160, 180, 196, 255)))
    # 2 CARMELITA
    R.append(pez((150, 110, 70), (214, 186, 140), largo=24, alto=12, cola='redonda', patron='puntos', pcol=(110, 74, 40, 255)))
    # 3 PUYE (casi transparente: se le ven los ojos y la espina)
    def puye_extra(d):
        L(d, [(16, 24), (35, 24)], (170, 200, 220, 255), 0.6)
        for i in range(6): L(d, [(18 + i * 3, 22.5), (19 + i * 3, 25.5)], (190, 214, 230, 255), 0.4)
    R.append(pez((214, 232, 240), (236, 246, 250), largo=28, alto=8, cola='recta', aleta_col=(200, 222, 236, 255), ojo_r=2.6, extra=puye_extra))
    # 4 BAGRECITO
    R.append(pez((92, 84, 70), (180, 164, 130), largo=30, alto=13, cola='redonda', bigotes=True, patron='puntos', pcol=(70, 62, 50, 255)))
    # 5 PERCA TRUCHA
    R.append(pez((120, 150, 70), (226, 214, 150), alto=17, patron='barras', pcol=(70, 90, 40, 255), aleta='larga', aleta_col=(220, 120, 60, 255)))
    # 6 SALMÓN
    R.append(pez((120, 130, 150), (246, 170, 140), largo=33, alto=15, panza=(250, 186, 160, 255), patron='puntos', pcol=(60, 66, 84, 255)))
    # 7 LAMPREA
    R.append(largo_fino((120, 110, 100), (190, 176, 150), boca_redonda=True, aletas=False))
    # 8 PEZ LUNA
    def luna():
        im, d = lienzo()
        d.polygon(P([(26, 13), (30, 4), (34, 14)]), fill=(150, 160, 180, 255))
        d.polygon(P([(26, 35), (30, 44), (34, 34)]), fill=(150, 160, 180, 255))
        E(d, 22, 24, 15, 13, (176, 186, 204, 255))
        d.chord([7 * K, 22 * K, 37 * K, 37 * K], 0, 180, fill=(214, 220, 232, 255))
        E(d, 37, 24, 3, 11, (150, 160, 180, 255))
        for (x, y) in ((20, 18), (27, 21), (16, 27), (25, 30), (31, 26)): E(d, x, y, 1.0, 1.0, (200, 206, 222, 255))
        ojo(d, 13, 21, 2.3)
        L(d, [(8, 25), (10.5, 25)], (90, 96, 116, 255), 0.8)
        return im
    R.append(luna())
    # 9 SARDINA
    R.append(pez((60, 110, 160), (230, 238, 246), largo=30, alto=10, patron='puntos', pcol=(30, 60, 100, 255), panza=(240, 244, 250, 255)))
    # 10 JUREL
    R.append(pez((70, 120, 140), (210, 222, 220), largo=32, alto=13, patron='linea', pcol=(240, 210, 90, 255), aleta_col=(220, 190, 80, 255)))
    # 11 CONGRIO
    R.append(largo_fino((220, 130, 120), (246, 200, 180), patron='manchas'))
    # 12 CORVINA
    R.append(pez((150, 150, 160), (232, 228, 222), alto=16, patron='escamas', pcol=(120, 120, 130, 255), aleta_col=(200, 170, 80, 255)))
    # 13 REINETA (alta y plana como un plato)
    R.append(pez((170, 180, 196), (236, 238, 244), largo=26, alto=22, cola='horquilla', aleta='larga', patron='escamas', pcol=(140, 150, 170, 255)))
    # 14 PEZ GLOBO
    def globo():
        im, d = lienzo()
        for a in range(0, 360, 30):
            r = math.radians(a)
            d.polygon(P([(23 + 12 * math.cos(r - 0.12), 24 + 12 * math.sin(r - 0.12)), (23 + 17 * math.cos(r), 24 + 17 * math.sin(r)),
                         (23 + 12 * math.cos(r + 0.12), 24 + 12 * math.sin(r + 0.12))]), fill=(200, 170, 90, 255))
        E(d, 23, 24, 13, 13, (236, 206, 120, 255))
        d.chord([10 * K, 22 * K, 36 * K, 37 * K], 0, 180, fill=(250, 240, 210, 255))
        for (x, y) in ((18, 17), (25, 15), (30, 20), (21, 22), (28, 26)): E(d, x, y, 1.3, 1.3, (160, 120, 50, 255))
        d.polygon(P([(36, 24), (44, 18), (44, 30)]), fill=(214, 184, 100, 255))
        ojo(d, 15, 21, 2.8)
        E(d, 11, 27, 1.8, 1.3, (200, 100, 90, 255))
        return im
    R.append(globo())
    # 15 CABALLITO DE MAR
    def caballito():
        im, d = lienzo()
        c = (246, 160, 70, 255)
        for i, (x, y, r) in enumerate([(22, 14, 5), (24, 19, 5.4), (25, 24, 5.6), (24.5, 29, 5.0), (22.5, 33, 4.2), (20, 36.5, 3.2), (18, 38.5, 2.4),
                                        (17, 40.5, 1.8), (18.5, 42.5, 1.6), (21, 42, 1.4)]):
            E(d, x, y, r, r, mezclar(c, (250, 200, 110), i / 10))
        d.polygon(P([(18, 13), (10, 15.5), (10, 17.5), (18, 16.5)]), fill=c)
        d.polygon(P([(22, 9), (24, 5), (26, 10)]), fill=oscuro(c, 0.8))
        d.polygon(P([(28.5, 20), (33, 22), (30, 27)]), fill=(250, 210, 140, 255))
        for i in range(5): L(d, [(21, 19 + i * 3), (26, 19 + i * 3)], oscuro(c, 0.75), 0.6)
        ojo(d, 21, 13, 1.9)
        return im
    R.append(caballito())
    # 16 PULPO
    def pulpo():
        im, d = lienzo()
        c = (226, 110, 120, 255)
        for i in range(6):
            x0 = 13 + i * 4.2
            pts = [(x0 + 1.5 * math.sin(j * 0.6 + i), 28 + j * 2.2) for j in range(7)]
            for j, (x, y) in enumerate(pts): E(d, x, y, 2.3 - j * 0.22, 2.3 - j * 0.22, oscuro(c, 0.88) if i % 2 else c)
            E(d, pts[-1][0] + 1, pts[-1][1], 1.0, 1.0, c)
        E(d, 24, 19, 12, 12, c)
        E(d, 20, 14, 3.5, 2.5, claro(c, 0.4))
        for (x, y) in ((28, 13), (30, 18), (17, 21)): E(d, x, y, 1.1, 1.1, oscuro(c, 0.75))
        ojo(d, 19.5, 22, 2.4); ojo(d, 28.5, 22, 2.4)
        d.arc([21 * K, 23 * K, 27 * K, 28 * K], 20, 160, fill=(120, 40, 50, 255), width=K)
        return im
    R.append(pulpo())
    # 17 ESTRELLA DE MAR
    def estrella():
        im, d = lienzo()
        pts = []
        for i in range(10):
            a = math.radians(-90 + i * 36)
            r = 19 if i % 2 == 0 else 8
            pts.append((24 + r * math.cos(a), 25 + r * math.sin(a)))
        d.polygon(P(pts), fill=(246, 140, 80, 255))
        for i in range(5):
            a = math.radians(-90 + i * 72)
            for j in range(1, 5):
                E(d, 24 + j * 3.3 * math.cos(a), 25 + j * 3.3 * math.sin(a), 0.9, 0.9, (255, 210, 160, 255))
        E(d, 24, 25, 3, 3, (250, 170, 110, 255))
        ojo(d, 21.5, 23, 1.6); ojo(d, 26.5, 23, 1.6)
        d.arc([22 * K, 25 * K, 26 * K, 28 * K], 20, 160, fill=(130, 60, 30, 255), width=K)
        return im
    R.append(estrella())
    # 18 ERIZO DE MAR
    def erizo():
        im, d = lienzo()
        for a in range(0, 360, 15):
            r = math.radians(a)
            L(d, [(24 + 8 * math.cos(r), 26 + 7 * math.sin(r)), (24 + 18 * math.cos(r), 26 + 16 * math.sin(r))], (110, 60, 150, 255), 1.1)
        E(d, 24, 26, 11, 9.5, (140, 80, 180, 255))
        E(d, 21, 22, 4, 2.5, (186, 140, 220, 255))
        ojo(d, 20.5, 26, 1.8); ojo(d, 27.5, 26, 1.8)
        return im
    R.append(erizo())
    # 19 PEZ LINTERNA
    def linterna_extra(d):
        L(d, [(12, 18), (8, 9), (5, 10)], (90, 100, 130, 255), 0.8)
        E(d, 5, 10, 2.2, 2.2, (250, 250, 160, 255)); E(d, 5, 10, 1.0, 1.0, (255, 255, 255, 255))
        for i in range(5): E(d, 15 + i * 4, 28, 0.9, 0.9, (140, 240, 255, 255))
    R.append(pez((40, 46, 80), (70, 80, 120), largo=28, alto=17, cola='redonda', aleta_col=(56, 62, 100, 255), ojo_r=3.0, extra=linterna_extra))
    # 20 ANGUILA
    R.append(largo_fino((90, 110, 70), (200, 196, 140)))
    # 21 PEZ DE HIELO
    def hielo_extra(d):
        for (x, y) in ((16, 20), (22, 26), (28, 21), (31, 27)): E(d, x, y, 0.8, 0.8, (255, 255, 255, 255))
        L(d, [(18, 23), (34, 23)], (200, 236, 255, 255), 0.5)
    R.append(pez((170, 214, 240), (226, 244, 252), largo=30, alto=13, aleta_col=(190, 226, 246, 255), patron='linea', pcol=(150, 200, 236, 255), extra=hielo_extra))
    # 22 PANCORA (cangrejo de río)
    R.append(cangrejo((200, 80, 60, 255), (170, 70, 50, 255), pinzas=1.15))
    # 23 CAMARÓN DE RÍO
    def camaron():
        im, d = lienzo()
        c = (236, 150, 120, 255)
        for i in range(7):
            a = math.radians(200 - i * 26)
            x, y = 24 + 11 * math.cos(a), 24 - 9 * math.sin(a)
            E(d, x, y, 4.6 - i * 0.35, 4.0 - i * 0.3, mezclar(c, (246, 196, 170), i / 7))
            if i < 6: d.arc([(x - 3) * K, (y - 3) * K, (x + 3) * K, (y + 3) * K], 200, 330, fill=oscuro(c, 0.75), width=K)
        x, y = 24 + 11 * math.cos(math.radians(200 - 6 * 26)), 24 - 9 * math.sin(math.radians(200 - 6 * 26))
        d.polygon(P([(x, y), (x + 3, y + 7), (x - 4, y + 6)]), fill=oscuro(c, 0.85))
        for i in range(5): L(d, [(16 + i * 3, 29), (15 + i * 3, 35)], oscuro(c, 0.8), 0.7)
        L(d, [(13, 22), (4, 12)], oscuro(c, 0.7), 0.6); L(d, [(13, 23), (3, 18)], oscuro(c, 0.7), 0.6)
        ojo(d, 14, 22, 1.6)
        return im
    R.append(camaron())
    # 24 JAIBA
    R.append(cangrejo((90, 140, 170, 255), (70, 110, 140, 255), pinzas=1.0, puntos=True))
    # 25 CANGREJO ERMITAÑO
    def ermitano():
        im, d = lienzo()
        c = (236, 196, 150, 255)
        for i in range(5):
            r = 13 - i * 2.4
            E(d, 28 - i * 0.5, 24 - i * 1.6, r, r * 0.9, mezclar((236, 214, 190), (200, 150, 120), i / 5))
            d.arc([(28 - i * 0.5 - r) * K, (24 - i * 1.6 - r * 0.9) * K, (28 - i * 0.5 + r) * K, (24 - i * 1.6 + r * 0.9) * K], 120, 300, fill=(170, 120, 90, 255), width=K)
        for i in range(3): L(d, [(17, 31 + i), (10 - i * 2, 37 + i)], (220, 110, 80, 255), 1.4)
        E(d, 14, 28, 5, 4, (230, 120, 90, 255))
        E(d, 9, 26, 3.6, 3, (230, 120, 90, 255))
        L(d, [(15, 24), (14, 18)], (200, 100, 80, 255), 1.0); L(d, [(17, 24), (18, 18)], (200, 100, 80, 255), 1.0)
        ojo(d, 14, 17.5, 1.6); ojo(d, 18, 17.5, 1.6)
        return im
    R.append(ermitano())
    # 26 CENTOLLA
    R.append(cangrejo((196, 60, 50, 255), (176, 60, 46, 255), pinzas=0.75, patas_largas=True, puntos=True))
    # 27 PEZ DORADO
    def dorado_extra(d):
        for (x, y) in ((7, 12), (39, 10), (41, 36)):
            L(d, [(x - 2.5, y), (x + 2.5, y)], (255, 250, 200, 255), 0.8); L(d, [(x, y - 2.5), (x, y + 2.5)], (255, 250, 200, 255), 0.8)
    R.append(pez((246, 176, 40), (255, 226, 110), patron='escamas', pcol=(230, 140, 30, 255), aleta='larga', aleta_col=(255, 200, 70, 255), extra=dorado_extra))
    # 28 PEZ ARCOÍRIS
    def arcoiris():
        im = pez((240, 80, 110), (110, 120, 240), largo=31, alto=16, aleta='larga', aleta_col=(160, 100, 230, 255))
        capa, dc = lienzo()
        cols = [(240, 80, 110), (255, 150, 60), (255, 220, 80), (110, 216, 110), (70, 200, 230), (90, 130, 240)]
        for i, c in enumerate(cols):
            dc.rectangle([10 * K, (16 + i * 2.7) * K, 38 * K, (18.7 + i * 2.7) * K], fill=c + (255,))
        m = im.split()[3]
        cuerpo = Image.new('L', im.size, 0)
        ImageDraw.Draw(cuerpo).ellipse([(23 - 15.5 + 3.5) * K, (24 - 7) * K, (23 + 15.5) * K, (24 + 7) * K], fill=255)
        im.paste(capa, (0, 0), Image.composite(capa.split()[3], Image.new('L', im.size, 0), cuerpo))
        d = ImageDraw.Draw(im)
        ojo(d, 12.5, 22.1, 2.4)
        for (x, y) in ((6, 10), (40, 12)):
            L(d, [(x - 2.5, y), (x + 2.5, y)], (255, 255, 255, 255), 0.8); L(d, [(x, y - 2.5), (x, y + 2.5)], (255, 255, 255, 255), 0.8)
        return im
    R.append(arcoiris())
    # 29 PEZ DE LAS ESTRELLAS
    def estrellas_extra(d):
        for (x, y) in ((15, 20), (20, 26), (25, 19), (29, 25), (33, 21), (18, 22), (27, 28)):
            E(d, x, y, 0.8, 0.8, (255, 250, 200, 255))
        L(d, [(23, 21.5), (27, 21.5)], (255, 250, 200, 255), 0.5)
    R.append(pez((40, 50, 120), (90, 70, 160), largo=30, alto=16, aleta='larga', aleta_col=(70, 70, 150, 255), extra=estrellas_extra))
    return R

def cuantizar(img, n):
    a = img.split()[3]
    rgb = img.convert('RGB').quantize(n, method=Image.MEDIANCUT, dither=Image.NONE)
    pal = rgb.getpalette()[:n * 3]
    idx = [rgb.getpixel((x, y)) + 1 if a.getpixel((x, y)) > 110 else 0 for y in range(S) for x in range(S)]
    used = max(idx)
    palb = []
    for k in range(used): palb += [255, pal[k * 3], pal[k * 3 + 1], pal[k * 3 + 2]]
    return palb, idx, used

if __name__ == '__main__':
    salida = sys.argv[1]
    previa = sys.argv[2] if len(sys.argv) > 2 else None
    out = ['/* GENERADO por herramientas/iconos_nuevos/peces48.py: los 30 animales de la colección de la pesca a 48x48',
           ' * (paleta ARGB + índices; 0 = transparente). El orden es el de PECES en core/peces.inc. No editar a mano. */']
    tab = []
    hoja = Image.new('RGBA', (S * 3 * 10, S * 3 * 3), (40, 70, 110, 255))
    for i, im in enumerate(todos()):
        im = acabar(im)
        palb, idx, used = cuantizar(im, 24)
        out.append(f'static const uint8_t PZ_{i}_PAL[{len(palb)}] = {{' + ','.join(map(str, palb)) + '};')
        out.append(f'static const uint8_t PZ_{i}_IDX[{S * S}] = {{' + ','.join(map(str, idx)) + '};')
        tab.append(f'    {{PZ_{i}_PAL, {used}, PZ_{i}_IDX}},')
        if previa:
            pal = {0: (0, 0, 0, 0)}
            for k in range(used): pal[k + 1] = tuple(palb[k * 4 + 1:k * 4 + 4]) + (255,)
            pv = Image.new('RGBA', (S, S)); pv.putdata([pal[v] for v in idx])
            hoja.alpha_composite(pv.resize((S * 3, S * 3), Image.NEAREST), ((i % 10) * S * 3, (i // 10) * S * 3))
    out.append('#define PECES_HD_N %d' % len(tab))
    out.append('static const struct { const uint8_t *pal; int n; const uint8_t *idx; } PECES_HD[PECES_HD_N] = {')
    out += tab
    out.append('};')
    open(salida, 'w').write('\n'.join(out) + '\n')
    if previa: hoja.save(os.path.join(previa, 'peces.png'))
    print('listo', len(tab))
