# (fw 104) Dos íconos nuevos en 48 px: el trocito de hielo (ICO_HIELO) y el papelito doblado del bosque (ICO_PAPELITO).
# Uso: python3 hielo_papelito48.py <carpeta_salida>  → escribe hielo.inc.txt / papelito.inc.txt (paleta + índices para
# firmware/main/iconos_hd.inc) y una vista previa .png de cada uno.
import math, sys, os
from PIL import Image, ImageDraw
S = 48
SS = 4  # supersampling

def cuantizar(img, n):
    """img RGBA de S x S -> (paleta ARGB plana, índices) con 0 = transparente"""
    a = img.split()[3]
    rgb = img.convert('RGB').quantize(n, method=Image.MEDIANCUT, dither=Image.NONE)
    pal = rgb.getpalette()[:n * 3]
    idx = []
    for y in range(S):
        for x in range(S):
            idx.append(rgb.getpixel((x, y)) + 1 if a.getpixel((x, y)) > 110 else 0)
    used = max(idx)
    palb = []
    for k in range(used):
        palb += [255, pal[k * 3], pal[k * 3 + 1], pal[k * 3 + 2]]
    return palb, idx, used

def contorno(img, color):
    """borde oscuro de 2 px alrededor de lo pintado (como el resto de los íconos)"""
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

def hielo():
    """un cubito irregular, celeste translúcido, con caras de distinto tono, brillos y una gotita"""
    W = S * SS
    im = Image.new('RGBA', (W, W), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    k = SS
    # caras del cubito (vista 3/4): arriba, izquierda, derecha
    arriba = [(12, 15), (27, 9), (39, 15), (24, 21)]
    izq = [(12, 15), (24, 21), (24, 38), (11, 31)]
    der = [(24, 21), (39, 15), (38, 31), (24, 38)]
    d.polygon([(x * k, y * k) for x, y in arriba], fill=(232, 250, 255, 255))
    d.polygon([(x * k, y * k) for x, y in izq], fill=(150, 214, 245, 255))
    d.polygon([(x * k, y * k) for x, y in der], fill=(104, 180, 230, 255))
    # vetas de aire congelado (lo hace parecer hielo de verdad)
    d.line([(15 * k, 22 * k), (19 * k, 29 * k)], fill=(205, 240, 255, 255), width=k)
    d.line([(30 * k, 24 * k), (33 * k, 21 * k)], fill=(170, 220, 248, 255), width=k)
    d.ellipse([28 * k, 27 * k, 31 * k, 30 * k], fill=(185, 228, 250, 255))
    # brillos
    d.line([(15 * k, 15 * k), (22 * k, 12 * k)], fill=(255, 255, 255, 255), width=2 * k)
    d.ellipse([33 * k, 16 * k, 35 * k, 18 * k], fill=(255, 255, 255, 255))
    # aristas claras
    d.line([(24 * k, 21 * k), (24 * k, 37 * k)], fill=(214, 244, 255, 255), width=k)
    # gotita que se escurre (se derrite un poquito)
    d.ellipse([33 * k, 33 * k, 37 * k, 38 * k], fill=(150, 214, 245, 255))
    d.polygon([(35 * k, 30 * k), (33 * k, 34 * k), (37 * k, 34 * k)], fill=(150, 214, 245, 255))
    d.ellipse([34 * k, 34 * k, 35 * k, 35 * k], fill=(255, 255, 255, 255))
    im = im.resize((S, S), Image.LANCZOS)
    return contorno(im, (22, 52, 92, 255))

def papelito():
    """un papel doblado en tres, color crema, con letra antigua y una esquina doblada; una hojita de peumo encima"""
    W = S * SS
    im = Image.new('RGBA', (W, W), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    k = SS
    # el papel, un poco girado (tres paneles por los dobleces)
    p1 = [(9, 14), (21, 11), (23, 37), (11, 40)]
    p2 = [(21, 11), (32, 12), (33, 38), (23, 37)]
    p3 = [(32, 12), (41, 15), (40, 36), (33, 38)]
    d.polygon([(x * k, y * k) for x, y in p1], fill=(250, 238, 205, 255))
    d.polygon([(x * k, y * k) for x, y in p2], fill=(236, 220, 182, 255))
    d.polygon([(x * k, y * k) for x, y in p3], fill=(248, 232, 196, 255))
    # esquina doblada
    d.polygon([(36 * k, 36.5 * k), (40 * k, 36 * k), (39.5 * k, 31 * k)], fill=(214, 194, 150, 255))
    # renglones de letra antigua (rayitas café)
    for i, (x0, y0, x1) in enumerate([(12, 18, 19), (12, 22, 20), (12, 26, 18), (24, 17, 30), (24, 21, 31), (24, 25, 29), (24, 29, 30), (34, 19, 38), (34, 23, 38)]):
        d.line([(x0 * k, y0 * k), (x1 * k, (y0 + 0.4) * k)], fill=(110, 70, 36, 255), width=k + 1)
    # hojita de peumo (verde) sujetándolo
    hoja = [(12, 34), (15, 29.5), (20, 28), (23, 29), (20, 33), (15, 35.5)]
    d.polygon([(x * k, y * k) for x, y in hoja], fill=(96, 176, 84, 255))
    d.line([(12.5 * k, 34 * k), (22 * k, 29 * k)], fill=(40, 108, 52, 255), width=k)
    d.line([(10.5 * k, 35.5 * k), (13 * k, 34 * k)], fill=(40, 108, 52, 255), width=k)
    im = im.resize((S, S), Image.LANCZOS)
    return contorno(im, (70, 46, 22, 255))

def escribir(nombre, img, carpeta, n):
    palb, idx, used = cuantizar(img, n)
    base = nombre.upper()
    txt = (f'static const uint8_t HDI_{base}_PAL[{len(palb)}] = {{' + ','.join(map(str, palb)) + '};\n' +
           f'static const uint8_t HDI_{base}_IDX[{S * S}] = {{' + ','.join(map(str, idx)) + '};\n')
    open(os.path.join(carpeta, nombre + '.inc.txt'), 'w').write(txt)
    pal = {0: (0, 0, 0, 0)}
    for k in range(used): pal[k + 1] = tuple(palb[k * 4 + 1:k * 4 + 4]) + (255,)
    prev = Image.new('RGBA', (S, S)); prev.putdata([pal[v] for v in idx])
    fondo = Image.new('RGBA', (S * 5, S * 5), (40, 52, 40, 255))
    fondo.alpha_composite(prev.resize((S * 5, S * 5), Image.NEAREST))
    fondo.save(os.path.join(carpeta, nombre + '.png'))
    print(nombre, 'colores', used)

if __name__ == '__main__':
    carpeta = sys.argv[1] if len(sys.argv) > 1 else '.'
    escribir('hielo', hielo(), carpeta, 14)
    escribir('papelito', papelito(), carpeta, 20)
