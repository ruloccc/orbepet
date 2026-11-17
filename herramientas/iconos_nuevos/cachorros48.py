# (fw 102) Los cuatro cachorros de Pochi, a partir de sus cuadros de 48 px (de pie, cola, trota 1 y 2, sentado/a).
#  - Chapi: del color de Pochi, collar verde liso.      - Pasita: mucho más blanca, collar rosado liso.
#  - Morita: un poco más clara que Pochi, collar morado liso.  - Rony: del color de Pochi, sin collar.
# El collar de perlas de Pochi se cambia por uno liso de 3 tonos (índices HDI_CACH_COLLAR*) o por pelaje (HDI_CACH_SIN*).
# Agrega todo a iconos_hd.inc (se puede correr de nuevo: reemplaza lo anterior). En pantalla van más chicos que Pochi.
# Uso: python3 cachorros48.py ruta/a/iconos_hd.inc [carpeta_vista_previa]
import re, sys
p = sys.argv[1]
s = open(p, encoding='utf-8').read()
s = re.sub(r'/\* \(fw 102\) cachorros: INICIO \*/.*?/\* \(fw 102\) cachorros: FIN \*/\n', '', s, flags=re.S)
s = re.sub(r'    \{ICO_CACH_[A-Z_]+, HDI_CACH_[A-Z_]+_PAL, \d+, HDI_\w+_IDX\},\n', '', s)
BLANCO = (255, 255, 255)
CACH = [  # nombre, cuánto se aclara el pelaje hacia el blanco (0 = como Pochi), collar (None = sin collar)
    ('CHAPI', 0.0, (0x4C, 0xB8, 0x5A)),
    ('PASITA', 0.72, (0xF2, 0x7A, 0xB0)),
    ('MORITA', 0.30, (0x9A, 0x5A, 0xD8)),
    ('RONY', 0.0, None),
]
CUADROS = [('', 'POCHI'), ('_B', 'POCHI_B'), ('_C', 'POCHI_C'), ('_D', 'POCHI_D'), ('_S', 'POCHI_SENT')]
def arr(n, k): return list(map(int, re.search(r'HDI_%s_%s\[\d+\] = \{([^}]*)\}' % (n, k), s).group(1).split(',')))
def lum(c): return (c[0] * 3 + c[1] * 6 + c[2]) / 10
def tipo(c):
    r, g, b = c
    if lum(c) < 70: return 'borde'
    if c == BLANCO: return 'blanco'
    if b >= r - 6 and b >= g: return 'collar'
    if r > b + 10 and b > g - 18 and r > 180: return 'rosado'
    return 'pelo'
def mezcla(a, b, t): return tuple(int(round(a[i] + (b[i] - a[i]) * t)) for i in range(3))
def es_lengua(c): return c is not None and c[0] > 180 and c[1] < 165 and c[0] - c[1] > 85
def cerrar_boca(idx, cols):
    # los cachorros, con la boca cerrada: se quitan la lengua y la boca abierta, y el hocico baja en diagonal hasta el pecho
    oscuro = lambda v: v != 0 and lum(cols[v]) < 70
    leng = [i for i, v in enumerate(idx) if v and es_lengua(cols[v])]
    if not leng: return idx
    out = list(idx)
    ty0 = min(i // 48 for i in leng); ty1 = max(i // 48 for i in leng); tx0 = min(i % 48 for i in leng)
    x0, x1, y0, y1 = tx0 - 14, 47, ty0 - 3, ty1 + 3
    caja = lambda i: x0 <= i % 48 <= x1 and y0 <= i // 48 <= y1
    vec8 = lambda i: [i + dy * 48 + dx for dy in (-1, 0, 1) for dx in (-1, 0, 1) if (dx or dy) and 0 <= i % 48 + dx < 48 and 0 <= i + dy * 48 + dx < 2304]
    pila = [j for i in leng for j in vec8(i) if oscuro(out[j]) and caja(j)]
    boca = set(pila)
    while pila:
        i = pila.pop()
        for j in vec8(i):
            if j not in boca and oscuro(out[j]) and caja(j) and j // 48 >= ty0 - 1: boca.add(j); pila.append(j)
    for i in leng: out[i] = 0
    for i in boca: out[i] = 0
    borde = min((v for v in set(idx) if v and oscuro(v)), key=lambda v: lum(cols[v]))
    cuerpo = lambda v: v != 0 and not oscuro(v)
    xn = max(x for y in range(ty0 - 3, ty0) for x in range(48) if out[y * 48 + x])  # la punta de la nariz
    for y in range(ty0 - 1, ty1 + 2):
        xs = [x for x in range(x0, 48) if cuerpo(out[y * 48 + x])]
        if not xs: continue
        xc = max(xs)
        e = max(xn - (y - ty0 + 2), xc + 1) if y <= ty0 + 4 else xc + 1   # el borde: diagonal desde la nariz, después el pecho
        e = min(e, 47)
        def pelo(x, y):  # el pelo más cercano (a la izquierda o arriba): nunca el collar ni los ojos
            for d in range(1, 20):
                for (xx, yy) in ((x - d, y), (x, y - d), (x - d, y - d)):
                    if 0 <= xx < 48 and 0 <= yy < 48:
                        v = idx[yy * 48 + xx]
                        if v and tipo(cols[v]) == 'pelo' and not es_lengua(cols[v]): return v
            return out[y * 48 + xc]
        for x in range(xc + 1, e):  # se rellena con pelo
            if out[y * 48 + x] == 0 or oscuro(out[y * 48 + x]): out[y * 48 + x] = pelo(x, y)
        for x in range(x0, e):  # lo que tocó el collar de perlas bajo el hocico también es pelo (el collar se rehace después)
            v = out[y * 48 + x]
            if v and v != idx[y * 48 + x] and tipo(cols[v]) != 'pelo': out[y * 48 + x] = pelo(x, y)
        out[y * 48 + e] = borde
        for x in range(e + 1, 48):
            if caja(y * 48 + x): out[y * 48 + x] = 0
    # el mentón: contorno abajo donde el hocico queda al aire
    for y in range(ty0 - 1, ty1 + 2):
        for x in range(x0, 48):
            i = y * 48 + x
            if cuerpo(out[i]) and i + 48 < 2304 and out[i + 48] == 0: out[i + 48] = borde
    # contorno suelto (que no toca nada del cuerpo): fuera
    for _ in range(2):
        for i in range(2304):
            if caja(i) and oscuro(out[i]) and not any(cuerpo(out[j]) for j in vec8(i)): out[i] = 0
    # agujeros (transparente encerrado dentro de la cara): pelo
    fuera, pila = set(), [i for i in range(2304) if (i % 48 in (0, 47) or i // 48 in (0, 47)) and out[i] == 0]
    fuera.update(pila)
    while pila:
        i = pila.pop()
        for j in ((i - 1) if i % 48 else -1, (i + 1) if i % 48 < 47 else -1, i - 48, i + 48):
            if 0 <= j < 2304 and out[j] == 0 and j not in fuera: fuera.add(j); pila.append(j)
    for i in range(2304):
        if out[i] == 0 and i not in fuera:
            x, y = i % 48, i // 48
            v = None
            for d in range(1, 12):
                for (xx, yy) in ((x - d, y), (x, y - d), (x + d, y), (x, y + d)):
                    if 0 <= xx < 48 and 0 <= yy < 48 and cuerpo(out[yy * 48 + xx]) and tipo(cols[out[yy * 48 + xx]]) == 'pelo': v = out[yy * 48 + xx]; break
                if v: break
            out[i] = v or borde
    return out
bloque = ['/* (fw 102) cachorros: INICIO */']
filas = []
idx_con, idx_sin = {}, {}
for suf, base in CUADROS:
    pal = arr(base, 'PAL'); idx = arr(base, 'IDX')
    cols = [None] + [tuple(pal[i * 4 + 1:i * 4 + 4]) for i in range(len(pal) // 4)]
    n = len(cols) - 1
    tp = [None if v == 0 else tipo(cols[v]) for v in idx]
    # el collar: los pixeles «collar» y los blancos pegados a ellos (los blancos de los ojos no lo tocan: tienen borde)
    col = set(i for i, t in enumerate(tp) if t == 'collar')
    pila = list(col)
    while pila:
        i = pila.pop(); x, y = i % 48, i // 48
        for j in ((i - 1) if x > 0 else -1, (i + 1) if x < 47 else -1, i - 48, i + 48):
            if 0 <= j < 2304 and j not in col and tp[j] in ('collar', 'blanco'): col.add(j); pila.append(j)
    # (los cachorros sacan la lengua solo de vez en cuando: en el cuadro «cola»; los demás, con la boca cerrada)
    if suf != '_B': idx = cerrar_boca(idx, cols)
    tp = [None if v == 0 else tipo(cols[v]) for v in idx]
    col = set(i for i in col if idx[i] and tp[i] in ('collar', 'blanco'))  # (lo que el hocico cerrado tapó ya no es collar)
    # pedacitos sueltos de «collar» (de menos de 4 pixeles, p. ej. en la mejilla): no son collar, son pelo
    def pelo_de(i):
        x, y = i % 48, i // 48
        for d in range(1, 20):
            for (xx, yy) in ((x - d, y), (x, y - d), (x + d, y), (x, y + d)):
                if 0 <= xx < 48 and 0 <= yy < 48 and idx[yy * 48 + xx] and tp[yy * 48 + xx] == 'pelo': return idx[yy * 48 + xx]
        return idx[i]
    vistos = set()
    for i0 in list(col):
        if i0 in vistos: continue
        comp, pila = {i0}, [i0]
        while pila:
            i = pila.pop()
            for j in (i - 1, i + 1, i - 48, i + 48):
                if j in col and j not in comp: comp.add(j); pila.append(j)
        vistos |= comp
        if len(comp) < 4:
            for i in comp: col.discard(i); idx[i] = pelo_de(i)
    con = list(idx); sin = list(idx)
    for i in col:
        arriba, abajo = i - 48 in col, i + 48 in col
        con[i] = n + 3 if not arriba else (n + 1 if not abajo else n + 2)  # claro arriba, oscuro abajo, medio
        x, y = i % 48, i // 48
        v = None
        for d in range(1, 12):  # sin collar: el pelo de al lado
            for xx in (x - d, x + d):
                if 0 <= xx < 48 and (y * 48 + xx) not in col and tp[y * 48 + xx] == 'pelo': v = idx[y * 48 + xx]; break
            if v: break
        sin[i] = v or idx[i]
    idx_con[base], idx_sin[base] = con, sin
    bloque.append('static const uint8_t HDI_CACH_COLLAR%s_IDX[2304] = {%s};' % (suf, ','.join(map(str, con))))
    bloque.append('static const uint8_t HDI_CACH_SIN%s_IDX[2304] = {%s};' % (suf, ','.join(map(str, sin))))
for nom, aclara, collar in CACH:
    for suf, base in CUADROS:
        pal = arr(base, 'PAL')
        cols = [tuple(pal[i * 4 + 1:i * 4 + 4]) for i in range(len(pal) // 4)]
        out = []
        for c in cols:
            if tipo(c) == 'pelo' and aclara: c = mezcla(c, (255, 252, 246), aclara)
            out += [255, c[0], c[1], c[2]]
        if collar:
            for t in (mezcla(collar, (0, 0, 0), 0.38), collar, mezcla(collar, BLANCO, 0.35)):  # oscuro, medio, claro
                out += [255, t[0], t[1], t[2]]
        bloque.append('static const uint8_t HDI_CACH_%s%s_PAL[%d] = {%s};' % (nom, suf, len(out), ','.join(map(str, out))))
        filas.append('    {ICO_CACH_%s%s, HDI_CACH_%s%s_PAL, %d, HDI_CACH_%s%s_IDX},' % (nom, suf, nom, suf, len(out) // 4, 'COLLAR' if collar else 'SIN', suf))
bloque.append('/* (fw 102) cachorros: FIN */')
i = s.index('static const struct { int ico; const uint8_t *pal; int n; const uint8_t *idx; } HDI[] = {')
s = s[:i] + '\n'.join(bloque) + '\n' + s[i:]
j = s.index('\n};', s.index('} HDI[] = {'))
s = s[:j] + '\n' + '\n'.join(filas) + s[j:]
open(p, 'w', encoding='utf-8').write(s)
print('cachorros:', len(filas), 'cuadros')
if len(sys.argv) > 2:
    from PIL import Image
    W = Image.new('RGBA', (5 * 110, 5 * 110), (96, 150, 90, 255))
    for fila, (nom, _a, collar) in enumerate([('POCHI', 0, 0)] + CACH):
        for k, (suf, base) in enumerate(CUADROS):
            if nom == 'POCHI': pal, idx, esc = arr(base, 'PAL'), arr(base, 'IDX'), 2
            else: pal, idx, esc = arr('CACH_%s%s' % (nom, suf), 'PAL'), (idx_con if collar else idx_sin)[base], 1.25
            P = [(0, 0, 0, 0)] + [tuple(pal[i * 4 + 1:i * 4 + 4]) + (255,) for i in range(len(pal) // 4)]
            im = Image.new('RGBA', (48, 48)); im.putdata([P[v] for v in idx])
            L = int(48 * esc); im = im.resize((L, L), Image.NEAREST); W.paste(im, (k * 110 + (96 - L) // 2, fila * 110 + 96 - L), im)
    W.save(sys.argv[2] + '/cachorros.png')
