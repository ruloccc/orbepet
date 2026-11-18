/*
 * Arte en pixel art generado por código. Diseños originales de este proyecto.
 *
 * Técnica: se diseña en una grilla chica (20x20 las criaturas, 12x12 los íconos,
 * 156x156 el mundo) con formas simples, pero se pinta con más detalle: cada pixel
 * del diseño son 1,5 o 2 pixeles reales (lienzo_t.k), y en pantalla cada pixel real
 * se ve de 2x2. Las formas redondas, el sombreado y el contorno salen en pixeles
 * reales (más finos); los detalles puestos a mano siguen la grilla del diseño.
 */
#ifdef ESP_PLATFORM
#include "esp_heap_caps.h"
#include "esp_attr.h"
#endif
#ifndef EXT_RAM_BSS_ATTR
#define EXT_RAM_BSS_ATTR
#endif
#include "art.h"
#include "objetos.h"
#include "src/misc/cache/instance/lv_image_cache.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ======================================================================
 * Utilidades de color y lienzo de baja resolución
 * ==================================================================== */

#define TRANSP 0x00000000u
#define A(c) (0xFF000000u | (c))
/* estilo del mundo y de los personajes: 0 el de antes (pastel), 1 el nuevo (verdes profundos, más tonos, sombras) */
static int s_estilo = 1;

typedef struct {
    int w, h;
    uint32_t *p;
    float k; /* pixeles reales por pixel del dibujo (0 = 1) */
} lienzo_t;
#define KL(l) ((l)->k > 0.0f ? (l)->k : 1.0f)
static void lz_real(lienzo_t *l, int X, int Y, uint32_t c) { if (X >= 0 && Y >= 0 && X < l->w && Y < l->h) l->p[Y * l->w + X] = c; }
static uint32_t lz_rget(const lienzo_t *l, int X, int Y) { return (X >= 0 && Y >= 0 && X < l->w && Y < l->h) ? l->p[Y * l->w + X] : 0; }
/* un pixel fino (real) en coordenadas lógicas con decimales */
static void lz_fino(lienzo_t *l, float x, float y, uint32_t c) { float k = KL(l); lz_real(l, (int)floorf(x * k), (int)floorf(y * k), c); }
static uint32_t mezclar(uint32_t a, uint32_t b, float t);
/* mezcla con un color cada pixel real de un pixel del dibujo: conserva el detalle fino que tenga debajo */
static void lz_mezclar(lienzo_t *l, int x, int y, uint32_t c, float t)
{
    float k = KL(l);
    for (int Y = (int)floorf(y * k); Y < (int)floorf((y + 1) * k); Y++)
        for (int X = (int)floorf(x * k); X < (int)floorf((x + 1) * k); X++)
            if (X >= 0 && Y >= 0 && X < l->w && Y < l->h) l->p[Y * l->w + X] = mezclar(l->p[Y * l->w + X], c, t);
}
/* Detalle: cada pixel del dibujo se pinta con k pixeles reales, y cada pixel real se ve de 2x2 en la
 * pantalla. Así una criatura a escala 3 se dibuja en 30x30 (en vez de 20x20) y un ícono a escala 4 en
 * 24x24: las formas salen más redondas y el contorno más fino, sin perder el aspecto de pixel art. */
static float k_de_escala(int escala) { return escala >= 2 ? (float)escala * 0.5f : 1.0f; }
/* búfer de trabajo para criaturas, huevos y recuerdos (se dibujan de a uno) */
static uint32_t *buf_trabajo(int n)
{
    static uint32_t *b;
    static int cap;
    if (n > cap) {
        free(b);
        b = malloc((size_t)n * 4);
        cap = b ? n : 0;
    }
    if (b) memset(b, 0, (size_t)n * 4);
    return b;
}

static uint32_t mezclar(uint32_t a, uint32_t b, float t)
{
    int ar = (a >> 16) & 255, ag = (a >> 8) & 255, ab = a & 255;
    int br = (b >> 16) & 255, bg = (b >> 8) & 255, bb = b & 255;
    int r = (int)(ar + (br - ar) * t), g = (int)(ag + (bg - ag) * t), bl = (int)(ab + (bb - ab) * t);
    return A(((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)bl);
}

static uint32_t oscurecer(uint32_t c, float f) { return mezclar(c, A(0x1A1626), f); }
static uint32_t aclarar(uint32_t c, float f) { return mezclar(c, A(0xFFFFFF), f); }

static uint32_t hash(int x, int y, uint32_t s)
{
    uint32_t h = (uint32_t)x * 374761393u + (uint32_t)y * 668265263u + s * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

static void lz_set(lienzo_t *l, int x, int y, uint32_t c)
{
    float k = KL(l);
    int x0 = (int)floorf(x * k), x1 = (int)floorf((x + 1) * k), y0 = (int)floorf(y * k), y1 = (int)floorf((y + 1) * k);
    for (int Y = y0; Y < y1; Y++) for (int X = x0; X < x1; X++) lz_real(l, X, Y, c);
}
static uint32_t lz_get(const lienzo_t *l, int x, int y)
{
    float k = KL(l);
    return lz_rget(l, (int)floorf((x + 0.5f) * k), (int)floorf((y + 0.5f) * k));
}

static void elipse(lienzo_t *l, float cx, float cy, float rx, float ry, uint32_t c)
{
    float k = KL(l);
    for (int Y = (int)floorf((cy - ry) * k); Y <= (int)ceilf((cy + ry) * k); Y++)
        for (int X = (int)floorf((cx - rx) * k); X <= (int)ceilf((cx + rx) * k); X++) {
            float dx = ((X + 0.5f) / k - cx) / rx, dy = ((Y + 0.5f) / k - cy) / ry;
            if (dx * dx + dy * dy <= 1.0f) lz_real(l, X, Y, c);
        }
}
static void circulo(lienzo_t *l, float cx, float cy, float r, uint32_t c) { elipse(l, cx, cy, r, r, c); }

static void rect(lienzo_t *l, int x, int y, int w, int h, uint32_t c)
{
    float k = KL(l);
    for (int Y = (int)floorf(y * k); Y < (int)floorf((y + h) * k); Y++)
        for (int X = (int)floorf(x * k); X < (int)floorf((x + w) * k); X++) lz_real(l, X, Y, c);
}

static float lado(float ax, float ay, float bx, float by, float px, float py)
{
    return (bx - ax) * (py - ay) - (by - ay) * (px - ax);
}
static void triangulo(lienzo_t *l, float x0, float y0, float x1, float y1, float x2, float y2, uint32_t c)
{
    float k = KL(l);
    int minx = (int)floorf(fminf(x0, fminf(x1, x2)) * k), maxx = (int)ceilf(fmaxf(x0, fmaxf(x1, x2)) * k);
    int miny = (int)floorf(fminf(y0, fminf(y1, y2)) * k), maxy = (int)ceilf(fmaxf(y0, fmaxf(y1, y2)) * k);
    for (int y = miny; y <= maxy; y++)
        for (int x = minx; x <= maxx; x++) {
            float px = (x + 0.5f) / k, py = (y + 0.5f) / k;
            float a = lado(x0, y0, x1, y1, px, py), b = lado(x1, y1, x2, y2, px, py), d = lado(x2, y2, x0, y0, px, py);
            if ((a >= 0 && b >= 0 && d >= 0) || (a <= 0 && b <= 0 && d <= 0)) lz_real(l, x, y, c);
        }
}

/* trazo grueso de puntos circulares a lo largo de un segmento */
static void trazo(lienzo_t *l, float x0, float y0, float x1, float y1, float r, uint32_t c)
{
    float d = hypotf(x1 - x0, y1 - y0);
    int n = (int)(d * 2 * KL(l)) + 1;
    for (int i = 0; i <= n; i++) {
        float t = (float)i / n;
        circulo(l, x0 + (x1 - x0) * t, y0 + (y1 - y0) * t, r, c);
    }
}

/* contorno automático: todo pixel vacío junto a uno lleno se pinta con "borde" */
static void contornear(lienzo_t *l, uint32_t borde)
{
    uint32_t *copia = malloc((size_t)l->w * l->h * 4);
    if (!copia) return;
    memcpy(copia, l->p, (size_t)l->w * l->h * 4);
    lienzo_t c = {l->w, l->h, copia, 1};
    for (int y = 0; y < l->h; y++)
        for (int x = 0; x < l->w; x++) {
            if (lz_rget(&c, x, y)) continue;
            if (lz_rget(&c, x - 1, y) || lz_rget(&c, x + 1, y) || lz_rget(&c, x, y - 1) || lz_rget(&c, x, y + 1))
                lz_real(l, x, y, borde);
        }
    free(copia);
}

/* ======================================================================
 * Imágenes LVGL
 * ==================================================================== */

uint32_t g_art_bytes; /* (v97) bytes de imágenes vivas (para medir qué panel se come la memoria) */
void art_img_free(art_img_t *img)
{
    if (img->px) {
        lv_image_cache_drop(&img->dsc);
        free(img->px);
        g_art_bytes -= img->capacidad * 4u;
    }
    memset(img, 0, sizeof(*img));
}

static void img_preparar(art_img_t *img, int w, int h)
{
    uint32_t n = (uint32_t)(w * h);
    if (n > img->capacidad) {
        if (img->px) { lv_image_cache_drop(&img->dsc); g_art_bytes -= img->capacidad * 4u; }
        free(img->px);
        img->px = malloc(n * 4);
        img->capacidad = img->px ? n : 0;
        g_art_bytes += img->capacidad * 4u;
    }
    if (!img->px) return;
    memset(img->px, 0, n * 4);
    lv_image_cache_drop(&img->dsc);
    memset(&img->dsc, 0, sizeof(img->dsc));
    img->dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
    img->dsc.header.cf = LV_COLOR_FORMAT_ARGB8888;
    img->dsc.header.w = (uint32_t)w;
    img->dsc.header.h = (uint32_t)h;
    img->dsc.header.stride = (uint32_t)(w * 4);
    img->dsc.data_size = n * 4;
    img->dsc.data = (const uint8_t *)img->px;
}

/* amplía un lienzo a una imagen ARGB8888 (con espejo opcional) */
static void ampliar(art_img_t *dst, const lienzo_t *src, int escala, bool espejo)
{
    img_preparar(dst, src->w * escala, src->h * escala);
    if (!dst->px) return;
    int W = src->w * escala;
    for (int y = 0; y < src->h; y++)
        for (int x = 0; x < src->w; x++) {
            uint32_t c = src->p[y * src->w + (espejo ? src->w - 1 - x : x)];
            if (!c) continue;
            for (int j = 0; j < escala; j++)
                for (int i = 0; i < escala; i++) dst->px[(y * escala + j) * W + x * escala + i] = c;
        }
}

/* ======================================================================
 * Criaturas: 8 especies elementales originales, cada una en 3 etapas
 * (bebé, cría y adulta) y 3 formas de adulta (normal, radiante, gruñona).
 * ==================================================================== */

typedef struct {
    uint32_t cuerpo, claro, oscuro, acento, panza, nariz, acento2;
} paleta_t;

static const paleta_t PALETA[8] = {
    /* Brotito (planta)   */ {0x86D16A, 0xB5EA8E, 0x5AA54E, 0x3E9E4C, 0xF4F1C4, 0xFF8FB1, 0xFFD45C},
    /* Chispa (eléctrico) */ {0xFFD84A, 0xFFF09A, 0xE0A92A, 0x4FC3F7, 0xFFF6D0, 0x3A2A2A, 0x2E8FD6},
    /* Ascua (fuego)      */ {0xF26B3A, 0xFF9A62, 0xC24A26, 0xFFC23D, 0xFFE3B0, 0x3A2A2A, 0xFF8A2E},
    /* Burbu (agua)       */ {0x8FD3F4, 0xC2EBFF, 0x5AA8D8, 0xFF9EC4, 0xE9F8FF, 0xFF7AAE, 0x4A90D9},
    /* Roco (roca)        */ {0xB9A58C, 0xD8C9B0, 0x8C7A63, 0x7FE0D6, 0xE8DCC8, 0x5C4F40, 0x6E6253},
    /* Nubi (nube)        */ {0xEEF3FF, 0xFFFFFF, 0xBCC8E6, 0x8CCBFF, 0xFFFFFF, 0xFF9EC4, 0xFFE066},
    /* Lumi (luz)         */ {0xFFEFAE, 0xFFFBE0, 0xE8C36A, 0xFFC93D, 0xFFFFFF, 0xFF9EC4, 0x9ED8FF},
    /* Brumo (fantasma)   */ {0xB9A6E8, 0xD9CCFF, 0x8C78C4, 0x6B5AA8, 0xE7DEFF, 0x7FE0C9, 0x7FE0C9},
};

/* Colores que se eligen al empezar: ahora se GIRA el tono (conservando luz y sombras), así cada especie cambia de verdad
 * aunque su color de fábrica sea parecido al tinte (antes se mezclaban y en algunas casi no se notaba). */
static void rgb_hsl(uint32_t c, float *h, float *sa, float *li)
{
    float r = (float)((c >> 16) & 255) / 255.0f, g = (float)((c >> 8) & 255) / 255.0f, b = (float)(c & 255) / 255.0f;
    float mx = fmaxf(r, fmaxf(g, b)), mn = fminf(r, fminf(g, b)), d = mx - mn;
    *li = (mx + mn) * 0.5f;
    *sa = d < 1e-4f ? 0.0f : d / (1.0f - fabsf(2.0f * *li - 1.0f));
    float hh = 0;
    if (d > 1e-4f) {
        if (mx == r) hh = fmodf((g - b) / d, 6.0f); else if (mx == g) hh = (b - r) / d + 2.0f; else hh = (r - g) / d + 4.0f;
        hh *= 60.0f; if (hh < 0) hh += 360.0f;
    }
    *h = hh;
}
static uint32_t hsl_rgb(float h, float sa, float li)
{
    float c = (1.0f - fabsf(2.0f * li - 1.0f)) * sa, hp = fmodf(h, 360.0f) / 60.0f, x = c * (1.0f - fabsf(fmodf(hp, 2.0f) - 1.0f)), r = 0, g = 0, b = 0;
    if (hp < 1) { r = c; g = x; } else if (hp < 2) { r = x; g = c; } else if (hp < 3) { g = c; b = x; } else if (hp < 4) { g = x; b = c; } else if (hp < 5) { r = x; b = c; } else { r = c; b = x; }
    float m = li - c * 0.5f;
    int R = (int)((r + m) * 255.0f + 0.5f), G = (int)((g + m) * 255.0f + 0.5f), B = (int)((b + m) * 255.0f + 0.5f);
    R = R < 0 ? 0 : R > 255 ? 255 : R; G = G < 0 ? 0 : G > 255 ? 255 : G; B = B < 0 ? 0 : B > 255 ? 255 : B;
    return A(((uint32_t)R << 16) | ((uint32_t)G << 8) | (uint32_t)B);
}
static const float TONO_H[8] = {0, 335.0f, 205.0f, 150.0f, 270.0f, 22.0f, 50.0f, 265.0f}; /* rosa, celeste, menta, lila, durazno, dorado, noche */
/* ref_h: el tono propio del cuerpo de la especie (si el tinte cae cerca del suyo, se usa el opuesto para que se note) */
static uint32_t girar_tono(uint32_t v, int tono, float ref_h, float fuerza)
{
    tono &= 7;
    if (!tono) return v;
    float h, sa, li;
    rgb_hsl(v, &h, &sa, &li);
    if (tono == 7) { /* noche: oscuro y violáceo */
        li *= 0.55f; sa = fmaxf(sa * 0.6f, 0.22f);
        return mezclar(v, hsl_rgb(TONO_H[7], sa, li), fuerza + 0.25f);
    }
    float th = TONO_H[tono];
    if (ref_h >= 0) {
        float dh = fabsf(th - ref_h); if (dh > 180.0f) dh = 360.0f - dh;
        if (dh < 40.0f) th = fmodf(th + 160.0f, 360.0f);
    }
    float ns = fmaxf(sa, 0.50f); /* los casi blancos y grises también toman color */
    float nl = li < 0.35f ? 0.35f : (li > 0.84f ? 0.84f : li); /* los muy claros (Nubi, Lumi) bajan un poco para que el color se vea */
    return mezclar(v, hsl_rgb(th, ns, nl), fuerza);
}
static float tono_ref(int especie) { float h, s, l; rgb_hsl(A(PALETA[(unsigned)especie % 8].cuerpo), &h, &s, &l); return s < 0.18f ? -1.0f : h; }

/* colores que se eligen al empezar: tiñen el cuerpo, la panza un poco */
__attribute__((unused)) static const uint32_t TINTE[8] = {0, 0xFF8FC8, 0x7CC8FF, 0x7EE0B0, 0xB99CF0, 0xFFB07A, 0xFFD45C, 0x4A4470};
static uint32_t color_cuerpo_nuevo(int especie, int tono); /* (108) más abajo: el color de verdad del cuerpo */
uint32_t art_color_especie(int e)
{
    int tono = (e >> 7) & 7;
    /* (fw 108) el botón de la paleta tiene que mostrar EL MISMO color con el que se pinta el cuerpo. Antes usaba
     * la paleta vieja (PALETA[].cuerpo) mientras que los personajes nuevos se pintan con BEBE_PAL: el color del
     * botón no se parecía en nada al de la mascota. */
    if (s_estilo) return color_cuerpo_nuevo(e & 7, tono);
    uint32_t c = PALETA[(unsigned)(e & 7)].cuerpo;
    if (!tono) return c;
    return girar_tono(A(c), tono, tono_ref(e & 7), 0.92f) & 0xFFFFFFu;
}

#define C_OJO A(0x2A2238)
#define C_BRILLO A(0xFFFFFF)
#define C_MEJILLA A(0xFF8FA3)
#define C_LAGRIMA A(0x7CC8FF)
#define C_FLOR A(0xFF8FC8)
#define C_FLOR_C A(0xFFE066)
#define C_DESTELLO A(0xFFF6A8)


static paleta_t paleta_forma(int especie, int forma, bool enferma, int tono)
{
    paleta_t p = PALETA[(unsigned)especie % 8];
    uint32_t *c = &p.cuerpo;
    tono &= 7;
    for (int i = 0; i < 7; i++) {
        uint32_t v = A(c[i]);
        if (tono && (i <= 2 || i == 4)) v = girar_tono(v, tono, tono_ref(especie), i == 4 ? 0.35f : 0.92f);
        if (forma == 1) v = aclarar(v, 0.10f);                     /* radiante */
        if (forma == 2) v = mezclar(v, A(0x7A7088), 0.35f);         /* gruñona: apagada */
        if (enferma) v = mezclar(v, A(0xB8DCA0), 0.33f);            /* un tercio de verde pálido */
        c[i] = v;
    }
    return p;
}

/* sombreado del cuerpo: luz arriba a la izquierda, sombra abajo a la derecha */
static void cuerpo(lienzo_t *l, float cx, float cy, float rx, float ry, const paleta_t *p)
{
    float k = KL(l);
    for (int Y = (int)floorf((cy - ry) * k); Y <= (int)ceilf((cy + ry) * k); Y++)
        for (int X = (int)floorf((cx - rx) * k); X <= (int)ceilf((cx + rx) * k); X++) {
            float nx = ((X + 0.5f) / k - cx) / rx, ny = ((Y + 0.5f) / k - cy) / ry, d = nx * nx + ny * ny;
            if (d > 1.0f) continue;
            float s = nx * 0.6f + ny * 0.8f, luz = -nx * 0.7f - ny * 0.7f;
            uint32_t c = p->cuerpo;
            if (s > 0.62f) c = p->oscuro;
            else if (s > 0.45f) c = mezclar(p->cuerpo, p->oscuro, 0.5f); /* un tono intermedio */
            else if (luz > 0.45f && d > 0.4f) c = p->claro;
            else if (luz > 0.3f && d > 0.5f) c = mezclar(p->cuerpo, p->claro, 0.5f);
            lz_real(l, X, Y, c);
        }
}

static void ojo(lienzo_t *l, int x, int y, art_expresion_t e, bool grande)
{
    if (KL(l) > 1.0f) { /* personajes nuevos, dibujados con más detalle: ojos grandes y brillantes */
        const float ox = (float)x + 1.0f, oy = (float)y + (grande ? 1.2f : 0.9f);
        if (e == EXP_FELIZ) { /* ojos sonrientes: arcos gruesos con pestaña */
            trazo(l, ox - 1.5f, oy + 0.9f, ox - 0.7f, oy - 0.2f, 0.42f, C_OJO);
            trazo(l, ox - 0.7f, oy - 0.2f, ox + 0.7f, oy - 0.2f, 0.42f, C_OJO);
            trazo(l, ox + 0.7f, oy - 0.2f, ox + 1.5f, oy + 0.9f, 0.42f, C_OJO);
            return;
        }
        if (e == EXP_CERRADOS) {
            trazo(l, ox - 1.4f, oy + 0.2f, ox - 0.6f, oy + 0.9f, 0.4f, C_OJO);
            trazo(l, ox - 0.6f, oy + 0.9f, ox + 0.6f, oy + 0.9f, 0.4f, C_OJO);
            trazo(l, ox + 0.6f, oy + 0.9f, ox + 1.4f, oy + 0.2f, 0.4f, C_OJO);
            return;
        }
        { /* el resto de las expresiones, con el mismo trazo fino */
            const float sg2 = x < 10 ? -1.0f : 1.0f;
            if (e == EXP_MAREADO) { /* ojos en espiral / con cruz */
                trazo(l, ox - 0.9f, oy - 0.8f, ox + 0.9f, oy + 0.9f, 0.36f, C_OJO);
                trazo(l, ox + 0.9f, oy - 0.8f, ox - 0.9f, oy + 0.9f, 0.36f, C_OJO);
                return;
            }
            if (e == EXP_MIEDO) { /* ojo blanco con la pupila chiquita */
                elipse(l, ox, oy, 1.05f, 1.45f, A(0x2A2050));
                elipse(l, ox, oy, 0.85f, 1.25f, C_BRILLO);
                elipse(l, ox, oy + 0.1f, 0.32f, 0.42f, C_OJO);
                return;
            }
            if (e == EXP_CELOS) { /* mira de reojo */
                elipse(l, ox, oy, 1.05f, 1.35f, A(0x2A2050));
                elipse(l, ox, oy, 0.85f, 1.15f, C_BRILLO);
                elipse(l, ox + sg2 * 0.45f, oy + 0.1f, 0.5f, 0.75f, C_OJO);
                lz_fino(l, ox + sg2 * 0.3f, oy - 0.2f, C_BRILLO);
                return;
            }
            if (e == EXP_ORGULLO) { /* ojos satisfechos: arco suave hacia arriba */
                trazo(l, ox - 1.3f, oy + 0.7f, ox - 0.5f, oy + 0.1f, 0.38f, C_OJO);
                trazo(l, ox - 0.5f, oy + 0.1f, ox + 0.5f, oy + 0.1f, 0.38f, C_OJO);
                trazo(l, ox + 0.5f, oy + 0.1f, ox + 1.3f, oy + 0.7f, 0.38f, C_OJO);
                lz_fino(l, ox + sg2 * 1.9f, oy - 0.8f, C_BRILLO);
                return;
            }
            if (e == EXP_ABURRIDO) { /* párpado caído */
                trazo(l, ox - 1.2f, oy + 0.1f, ox + 1.2f, oy + 0.1f, 0.4f, C_OJO);
                elipse(l, ox, oy + 0.75f, 0.7f, 0.5f, C_OJO);
                return;
            }
            if (e == EXP_ENFERMA || e == EXP_ENFERMA_LEVE) { /* cerrados de cansancio */
                trazo(l, ox - 1.2f, oy + 0.7f, ox + 1.2f, oy + 0.7f, 0.38f, C_OJO);
                return;
            }
        }
        if (e == EXP_ABIERTOS || e == EXP_CURIOSO || e == EXP_TRISTE || e == EXP_ENOJADO) {
            /* ojo ovalado y vertical, con un iris grande y claro (mirada dulce, no de pez ni enojada) y dos brillos */
            const float lado = (x < 10 ? 1.0f : -1.0f) * 0.2f;
            const float rx = grande ? 1.15f : 1.0f, ry = grande ? 1.5f : 1.3f, ex = ox + lado, yy = (e == EXP_CURIOSO ? oy - 0.4f : oy) + 0.2f;
            const float sg = x < 10 ? -1.0f : 1.0f;
            elipse(l, ex, yy, rx, ry, A(0x2A2050));
            elipse(l, ex, yy + ry * 0.04f, rx * 0.86f, ry * 0.9f, A(0x5A4AB0));       /* el iris */
            elipse(l, ex, yy + ry * 0.38f, rx * 0.7f, ry * 0.5f, A(0x8C7CE8));        /* más claro abajo */
            elipse(l, ex, yy - ry * 0.02f, rx * 0.42f, ry * 0.5f, A(0x1E1636));       /* la pupila */
            elipse(l, ex - rx * 0.34f, yy - ry * 0.42f, rx * 0.34f, rx * 0.4f, C_BRILLO); /* brillo grande */
            lz_fino(l, ex + rx * 0.38f, yy + ry * 0.5f, C_BRILLO);                     /* brillo chiquito */
            (void)sg;
            if (e == EXP_TRISTE) trazo(l, ex - rx, yy - ry * 1.05f, ex + rx * 0.8f, yy - ry * 1.35f, 0.3f, C_OJO);
            if (e == EXP_ENOJADO) trazo(l, ex - sg * rx, yy - ry * 1.3f, ex + sg * rx, yy - ry * 0.9f, 0.3f, C_OJO);
            return;
        }
    }
    if (KL(l) > 1.0f && (e == EXP_ABIERTOS || e == EXP_CURIOSO)) {
        float cx = x + 1.0f, cy = y + (grande ? 1.4f : 1.0f), r = grande ? 1.25f : 1.05f;
        elipse(l, cx, cy, r, r * (grande ? 1.2f : 1.1f), C_OJO);
        lz_fino(l, cx - r * 0.35f, cy - r * 0.55f, C_BRILLO);
        lz_fino(l, cx + r * 0.35f, cy + r * 0.25f, A(0x6A5A8A));
        return;
    }
    switch (e) {
    case EXP_CERRADOS: lz_set(l, x, y + 1, C_OJO); lz_set(l, x + 1, y + 1, C_OJO); break;
    case EXP_FELIZ: lz_set(l, x, y, C_OJO); lz_set(l, x + 1, y, C_OJO); lz_set(l, x - 1, y + 1, C_OJO); lz_set(l, x + 2, y + 1, C_OJO); break;
    case EXP_MAREADO: lz_set(l, x, y, C_OJO); lz_set(l, x + 1, y + 1, C_OJO); lz_set(l, x + 1, y, C_OJO); break;
    case EXP_ENFERMA: case EXP_ENFERMA_LEVE: /* — — : cerrados de cansancio */
        lz_set(l, x - 1, y + 1, C_OJO); lz_set(l, x, y + 1, C_OJO); lz_set(l, x + 1, y + 1, C_OJO); break;
    case EXP_CURIOSO: /* ojos bien abiertos, mirando arriba */
        lz_set(l, x, y - 1, C_OJO); lz_set(l, x + 1, y - 1, C_OJO);
        lz_set(l, x, y, C_BRILLO); lz_set(l, x + 1, y, C_OJO);
        lz_set(l, x, y + 1, C_OJO); lz_set(l, x + 1, y + 1, C_OJO);
        break;
    case EXP_MIEDO: /* pupila chiquita en un ojo blanco */
        lz_set(l, x, y, C_BRILLO); lz_set(l, x + 1, y, C_BRILLO);
        lz_set(l, x, y + 1, C_BRILLO); lz_set(l, x + 1, y + 1, C_OJO);
        break;
    case EXP_ORGULLO: /* párpados a media asta, satisfecha */
        lz_set(l, x - 1, y, C_OJO); lz_set(l, x, y, C_OJO); lz_set(l, x + 1, y, C_OJO);
        lz_set(l, x, y + 1, C_OJO); lz_set(l, x + 1, y + 1, C_OJO);
        break;
    case EXP_CELOS: /* mira de reojo */
        lz_set(l, x, y, C_BRILLO); lz_set(l, x + 1, y, C_OJO);
        lz_set(l, x, y + 1, C_BRILLO); lz_set(l, x + 1, y + 1, C_OJO);
        break;
    case EXP_ABURRIDO: /* párpado caído */
        lz_set(l, x, y, C_OJO); lz_set(l, x + 1, y, C_OJO);
        lz_set(l, x, y + 1, C_OJO);
        break;
    default:
        lz_set(l, x, y, C_BRILLO); lz_set(l, x + 1, y, C_OJO);
        lz_set(l, x, y + 1, C_OJO); lz_set(l, x + 1, y + 1, C_OJO);
        if (grande) { lz_set(l, x, y + 2, C_OJO); lz_set(l, x + 1, y + 2, C_OJO); lz_set(l, x + 1, y + 1, C_BRILLO); }
        break;
    }
}

/* rayo en zigzag de (x,y) hacia arriba */
static void zigzag(lienzo_t *l, float x, float y, float alto, float ancho, uint32_t c)
{
    float paso = alto / 3.0f;
    trazo(l, x, y, x + ancho, y - paso, 0.55f, c);
    trazo(l, x + ancho, y - paso, x - ancho * 0.3f, y - 2 * paso, 0.55f, c);
    trazo(l, x - ancho * 0.3f, y - 2 * paso, x + ancho * 0.8f, y - alto, 0.55f, c);
}

/* llama de tres capas con la base en (x,y) */
static void llama(lienzo_t *l, float x, float y, float alto, float ancho, const paleta_t *P)
{
    triangulo(l, x - ancho, y, x + ancho, y, x + ancho * 0.2f, y - alto, P->oscuro);
    triangulo(l, x - ancho * 1.1f, y, x - ancho * 0.1f, y, x - ancho * 0.9f, y - alto * 0.7f, P->oscuro);
    triangulo(l, x - ancho * 0.6f, y, x + ancho * 0.7f, y, x + ancho * 0.1f, y - alto * 0.75f, A(P->acento2));
    triangulo(l, x - ancho * 0.3f, y, x + ancho * 0.35f, y, x, y - alto * 0.45f, A(P->acento));
}

static void estrellita(lienzo_t *l, int x, int y, uint32_t c)
{
    lz_set(l, x, y, c); lz_set(l, x - 1, y, c); lz_set(l, x + 1, y, c); lz_set(l, x, y - 1, c); lz_set(l, x, y + 1, c);
}

static int s_cara_frame; /* el cuadro de la animación (para lo que tiembla o se infla en la cara) */
static void pintar_cara(lienzo_t *lp, art_expresion_t e, int ex0, int ex1, int ey, int mx, int my, bool ojos_grandes, int forma,
                        bool mejillas_siempre, uint32_t mejilla, bool boca_abierta, int etapa)
{
    lienzo_t l = *lp;
    ojo(&l, ex0, ey, e, ojos_grandes);
    ojo(&l, ex1, ey, e, ojos_grandes);
    if (e == EXP_ENOJADO || e == EXP_CELOS || forma == 2) { /* cejas */
        if (KL(&l) > 1.0f) {
            trazo(&l, (float)ex0 - 0.6f, (float)ey - 2.2f, (float)ex0 + 1.8f, (float)ey - 1.3f, 0.32f, C_OJO);
            trazo(&l, (float)ex1 + 2.6f, (float)ey - 2.2f, (float)ex1 + 0.2f, (float)ey - 1.3f, 0.32f, C_OJO);
        } else {
        lz_set(&l, ex0 - 1, ey - 2, C_OJO); lz_set(&l, ex0, ey - 1, C_OJO);
        lz_set(&l, ex1 + 2, ey - 2, C_OJO); lz_set(&l, ex1 + 1, ey - 1, C_OJO);
        }
    }
    int dy_ojo = ojos_grandes ? 1 : 0;
    if (e == EXP_TRISTE) { lz_set(&l, ex0, ey + 2 + dy_ojo, C_LAGRIMA); lz_set(&l, ex0, ey + 3 + dy_ojo, C_LAGRIMA); }
    if (e == EXP_MIEDO) { lz_set(&l, ex1 + 3, ey - 1, C_LAGRIMA); lz_set(&l, ex1 + 3, ey, C_LAGRIMA); } /* gotita de sudor */
    const bool enf = e == EXP_ENFERMA || e == EXP_ENFERMA_LEVE;
    if (enf) { /* mejillas rojas de fiebre */
        uint32_t fiebre = A(0xFF4A5A);
        lz_set(&l, ex0 - 1, ey + 2 + dy_ojo, fiebre); lz_set(&l, ex0, ey + 2 + dy_ojo, fiebre);
        lz_set(&l, ex1 + 1, ey + 2 + dy_ojo, fiebre); lz_set(&l, ex1 + 2, ey + 2 + dy_ojo, fiebre);
    } else if (e == EXP_FELIZ || forma == 1 || mejillas_siempre) {
        if (KL(&l) > 1.0f) { /* mejillas redondas y suaves, con su brillito */
            for (int q = 0; q < 2; q++) {
                const float mx2 = q ? (float)ex1 + 2.6f : (float)ex0 - 0.6f, my2 = (float)ey + 2.9f + dy_ojo;
                elipse(&l, mx2, my2, 1.25f, 0.8f, mejilla);
                elipse(&l, mx2 - 0.25f, my2 - 0.2f, 0.6f, 0.35f, aclarar(mejilla, 0.45f));
            }
        } else {
            lz_set(&l, ex0 - 1, ey + 2 + dy_ojo, mejilla);
            lz_set(&l, ex1 + 2, ey + 2 + dy_ojo, mejilla);
        }
    }
    if (e == EXP_ENFERMA) { /* una gota de sudor que cae y una burbuja de mocos que se infla y se desinfla */
        lz_set(&l, ex1 + 3, ey - 1 + s_cara_frame * 2, C_LAGRIMA);
        int bx = (ex0 + ex1) / 2 + 2, by = ey + 2 + dy_ojo;
        if (s_cara_frame) { circulo(&l, (float)bx + 1.2f, (float)by + 0.6f, 1.4f, A(0xCFEFFF)); lz_set(&l, bx + 1, by, A(0xFFFFFF)); }
        else lz_set(&l, bx, by + 1, A(0xCFEFFF));
    }
    my += dy_ojo;
    if (KL(&l) > 1.0f && (boca_abierta || e == EXP_CURIOSO || e == EXP_MIEDO || e == EXP_ORGULLO || e == EXP_ABURRIDO || e == EXP_CELOS || enf || e == EXP_TRISTE || e == EXP_ENOJADO || e == EXP_MAREADO)) {
        const float fx = (float)mx, fy = (float)my;
        if (boca_abierta) { elipse(&l, fx + 0.9f, fy + 0.7f, 1.0f, 0.85f, C_OJO); elipse(&l, fx + 0.9f, fy + 1.1f, 0.65f, 0.4f, C_MEJILLA); }
        else if (e == EXP_CURIOSO) elipse(&l, fx + 0.9f, fy + 0.6f, 0.5f, 0.65f, C_OJO);
        else if (e == EXP_MIEDO) {
            trazo(&l, fx - 0.4f, fy + 0.9f, fx + 0.5f, fy + 0.2f, 0.28f, C_OJO); trazo(&l, fx + 0.5f, fy + 0.2f, fx + 1.3f, fy + 0.9f, 0.28f, C_OJO); trazo(&l, fx + 1.3f, fy + 0.9f, fx + 2.2f, fy + 0.2f, 0.28f, C_OJO);
        } else if (e == EXP_ORGULLO) {
            trazo(&l, fx - 0.2f, fy + 0.9f, fx + 1.4f, fy + 0.9f, 0.28f, C_OJO); trazo(&l, fx + 1.4f, fy + 0.9f, fx + 2.3f, fy + 0.1f, 0.28f, C_OJO);
        } else if (e == EXP_ABURRIDO || e == EXP_CELOS) trazo(&l, fx - 0.1f, fy + 0.6f, fx + 2.0f, fy + 0.6f, 0.28f, C_OJO);
        else if (enf) {
            const float t = (float)s_cara_frame * 0.5f;
            trazo(&l, fx - 0.4f, fy + 0.9f, fx + 0.5f, fy + 0.2f + t, 0.28f, C_OJO); trazo(&l, fx + 0.5f, fy + 0.2f + t, fx + 1.4f, fy + 0.9f - t, 0.28f, C_OJO); trazo(&l, fx + 1.4f, fy + 0.9f - t, fx + 2.3f, fy + 0.2f, 0.28f, C_OJO);
        } else { /* triste, enojada, mareada: boquita hacia abajo */
            trazo(&l, fx - 0.3f, fy + 0.95f, fx + 0.6f, fy + 0.25f, 0.28f, C_OJO); trazo(&l, fx + 0.6f, fy + 0.25f, fx + 1.4f, fy + 0.25f, 0.28f, C_OJO); trazo(&l, fx + 1.4f, fy + 0.25f, fx + 2.3f, fy + 0.95f, 0.28f, C_OJO);
        }
    } else if (boca_abierta) {
        lz_set(&l, mx, my, C_OJO); lz_set(&l, mx + 1, my, C_OJO);
        lz_set(&l, mx, my + 1, C_MEJILLA); lz_set(&l, mx + 1, my + 1, C_MEJILLA);
    } else if (e == EXP_CURIOSO) {
        lz_set(&l, mx, my, C_OJO); /* boquita en "o" */
    } else if (e == EXP_MIEDO) {
        lz_set(&l, mx - 1, my + 1, C_OJO); lz_set(&l, mx, my, C_OJO); lz_set(&l, mx + 1, my + 1, C_OJO); lz_set(&l, mx + 2, my, C_OJO);
    } else if (e == EXP_ORGULLO) {
        lz_set(&l, mx, my + 1, C_OJO); lz_set(&l, mx + 1, my + 1, C_OJO); lz_set(&l, mx + 2, my, C_OJO);
    } else if (e == EXP_ABURRIDO || e == EXP_CELOS) {
        lz_set(&l, mx, my, C_OJO); lz_set(&l, mx + 1, my, C_OJO);
    } else if (enf) { /* ⌒ que tiembla */
        int t = s_cara_frame;
        lz_set(&l, mx - 1, my + 1, C_OJO); lz_set(&l, mx, my + t, C_OJO); lz_set(&l, mx + 1, my + 1 - t, C_OJO); lz_set(&l, mx + 2, my + 1, C_OJO);
        lz_set(&l, mx, my + 1 - t, C_OJO);
    } else if (e == EXP_TRISTE || e == EXP_ENOJADO || e == EXP_MAREADO) {
        lz_set(&l, mx, my, C_OJO); lz_set(&l, mx + 1, my, C_OJO);
        lz_set(&l, mx - 1, my + 1, C_OJO); lz_set(&l, mx + 2, my + 1, C_OJO);
    } else if (etapa == 1) {
        if (KL(&l) > 1.0f) { trazo(&l, (float)mx + 0.2f, (float)my + 0.3f, (float)mx + 0.9f, (float)my + 0.8f, 0.28f, C_OJO); trazo(&l, (float)mx + 0.9f, (float)my + 0.8f, (float)mx + 1.7f, (float)my + 0.3f, 0.28f, C_OJO); }
        else { lz_set(&l, mx, my, C_OJO); lz_set(&l, mx + 1, my, C_OJO); } /* boquita de bebé */
    } else {
        if (KL(&l) > 1.0f) { /* sonrisa fina */
            trazo(&l, (float)mx - 0.3f, (float)my + 0.2f, (float)mx + 0.4f, (float)my + 0.95f, 0.28f, C_OJO);
            trazo(&l, (float)mx + 0.4f, (float)my + 0.95f, (float)mx + 1.6f, (float)my + 0.95f, 0.28f, C_OJO);
            trazo(&l, (float)mx + 1.6f, (float)my + 0.95f, (float)mx + 2.3f, (float)my + 0.2f, 0.28f, C_OJO);
        } else {
            lz_set(&l, mx - 1, my, C_OJO); lz_set(&l, mx, my + 1, C_OJO); lz_set(&l, mx + 1, my + 1, C_OJO);
            lz_set(&l, mx + 2, my, C_OJO);
        }
    }

}

/* el ambiente para la ropa mágica (lo pone la pantalla): la estación, si es de noche y si hay sol */
static int s_amb_estacion, s_amb_noche, s_amb_sol;
void art_set_ambiente(int estacion, bool noche, bool sol) { s_amb_estacion = estacion & 3; s_amb_noche = noche; s_amb_sol = sol && !noche; }
uint32_t art_ambiente_clave(void) { return (uint32_t)s_amb_estacion | (uint32_t)s_amb_noche << 2 | (uint32_t)s_amb_sol << 3; }

/* la ropa de siempre, con un toque mágico */
static void pintar_ropa(lienzo_t *lp, const int ropa[4], float cx, float rx, float cy, float ry, float top, int ex0, int ex1, int ey)
{
    lienzo_t l = *lp;
    const int f = s_cara_frame;
    const int cab = ropa[0], cara = ropa[1], cue = 0; /* ropa_t + 1 (el cuello ya no se viste: la ropa tapaba mucho el cuerpo; solo sombreros y accesorios) */
    (void)ropa[2];
    const float neck = cy + ry * 0.38f, tp = top;
    if (cue == 2) { /* pañuelo de hojas: verde tierno, verde, naranjo o escarchado según la estación */
        static const uint32_t HOJA[4][2] = {{0x8FE08A, 0x5DBB63}, {0x4FB85A, 0x2E8A4A}, {0xF2A33A, 0xC8642A}, {0xCFEAF2, 0x8ABCD0}};
        const uint32_t *c = HOJA[s_amb_estacion];
        triangulo(&l, cx - rx * 0.7f, neck, cx + rx * 0.7f, neck, cx + 0.3f, neck + 3.4f, A(c[0]));
        trazo(&l, cx + 0.3f, neck + 0.2f, cx + 0.3f, neck + 3.0f, 0.25f, A(c[1]));           /* la nervadura */
        lz_set(&l, (int)(cx - rx * 0.35f), (int)neck, A(c[1])); lz_set(&l, (int)(cx + rx * 0.4f), (int)neck, A(c[1]));
        if (s_amb_estacion == 3) lz_set(&l, (int)(cx - 1), (int)neck + 1, A(0xFFFFFF));        /* escarcha */
    } else if (cue == 4) { /* bufanda de seda de araña: blanca plateada, con un brillo que corre */
        rect(&l, (int)(cx - rx * 0.85f), (int)neck - 1, (int)(rx * 1.7f) + 1, 2, A(0xF0F0FF));
        for (int x = (int)(cx - rx * 0.6f); x < (int)(cx + rx * 0.85f); x += 3) lz_set(&l, x, (int)neck, A(0xB8C8E8));
        rect(&l, (int)(cx + rx * 0.35f), (int)neck + 1, 2, 3, A(0xF0F0FF)); lz_set(&l, (int)(cx + rx * 0.35f), (int)neck + 3, A(0xB8C8E8));
        lz_set(&l, (int)(cx - rx * 0.5f + f * rx * 0.6f), (int)neck - 1, A(0xFFFFFF));
    } else if (cue == 8) { /* collar de conchas con una perla que brilla */
        for (int k = 0; k < 5; k++) {
            if (k == 2) continue;
            float x = cx - rx * 0.6f + k * rx * 0.3f, y = neck - 0.5f + ((k == 1 || k == 3) ? 0.6f : 0.0f);
            lz_set(&l, (int)x, (int)y, k % 2 ? A(0xFFF1C9) : A(0xFFC2D6));
        }
        circulo(&l, cx, neck + 0.8f, 1.0f, A(0xF4F0FF)); lz_set(&l, (int)cx, (int)neck, A(0xFFFFFF));
        if (f) { lz_set(&l, (int)cx - 2, (int)neck + 1, A(0xE8E0FF)); lz_set(&l, (int)cx + 2, (int)neck + 1, A(0xE8E0FF)); } /* su luz */
    }
    if (cara == 6) { /* lentes de cristal de luna: lunas celestes con aro de plata y un destello */
        rect(&l, ex0 - 1, ey, 3, 2, A(0xB8C0D8)); rect(&l, ex1, ey, 3, 2, A(0xB8C0D8));
        lz_set(&l, ex0, ey, A(0xCFE0FF)); lz_set(&l, ex0, ey + 1, A(0x9AB4F0)); lz_set(&l, ex1 + 1, ey, A(0xCFE0FF)); lz_set(&l, ex1 + 1, ey + 1, A(0x9AB4F0));
        for (int x = ex0 + 2; x < ex1; x++) lz_set(&l, x, ey, A(0xB8C0D8));
        lz_set(&l, f ? ex0 - 1 : ex1 + 1, ey, A(0xFFFFFF));
    }
    if (cab == 1) { /* moño de pétalos que suelta brillitos */
        float bx = cx + rx * 0.45f, by = tp + 1.2f;
        triangulo(&l, bx, by, bx - 2.6f, by - 1.8f, bx - 2.2f, by + 1.6f, A(0xFF8FC0));
        triangulo(&l, bx, by, bx + 2.6f, by - 1.8f, bx + 2.2f, by + 1.6f, A(0xFF8FC0));
        lz_set(&l, (int)(bx - 2), (int)(by - 1), A(0xFFD6E8)); lz_set(&l, (int)(bx + 2), (int)(by - 1), A(0xFFD6E8));
        lz_set(&l, (int)bx, (int)by, A(0xFFE066));
        lz_set(&l, (int)(bx + (f ? 3.5f : -3.0f)), (int)(by - (f ? 2.5f : 1.5f)), A(0xFFF6B0)); /* brillito */
    } else if (cab == 3) { /* gorro de lana azul noche con estrellitas que titilan */
        for (int y = (int)(tp - 2.0f); y <= (int)(tp + 2.0f); y++)
            for (int x = (int)(cx - rx * 0.85f); x <= (int)(cx + rx * 0.85f); x++) {
                float nx = (x + 0.5f - cx) / (rx * 0.85f), ny = (y + 0.5f - (tp + 2.0f)) / 4.0f;
                if (nx * nx + ny * ny <= 1.0f) lz_set(&l, x, y, (y >= (int)(tp + 1.0f)) ? A(0xE8E4FF) : ((x + y) % 3 ? A(0x4A3FB8) : A(0x5E52D0)));
            }
        circulo(&l, cx, tp - 2.6f, 1.2f, A(0xFFFFFF));
        lz_set(&l, (int)(cx - rx * 0.4f), (int)tp - 1 + f, A(f ? 0xFFE066 : 0xFFF6C0));
        lz_set(&l, (int)(cx + rx * 0.3f), (int)tp - f, A(f ? 0xFFF6C0 : 0xFFE066));
        if (f) lz_set(&l, (int)cx, (int)tp, A(0xFFFFFF));
    } else if (cab == 5) { /* sombrero de paja con una flor que se abre al sol */
        rect(&l, (int)(cx - rx - 1.0f), (int)(tp + 1.0f), (int)(2.0f * rx + 3.0f), 1, A(0xE8C36A));
        rect(&l, (int)(cx - 2.5f), (int)(tp - 2.0f), 6, 3, A(0xF0D488));
        rect(&l, (int)(cx - 2.5f), (int)tp, 6, 1, A(0x5DBB63));
        float fx = cx + 2.6f, fy = tp - 0.6f;
        if (s_amb_sol) { /* abierta */
            lz_set(&l, (int)fx - 1, (int)fy, A(0xFF8FC0)); lz_set(&l, (int)fx + 1, (int)fy, A(0xFF8FC0));
            lz_set(&l, (int)fx, (int)fy - 1, A(0xFF8FC0)); lz_set(&l, (int)fx, (int)fy + 1, A(0xFF8FC0));
            lz_set(&l, (int)fx, (int)fy, A(0xFFE066));
        } else { lz_set(&l, (int)fx, (int)fy, A(0xFF8FC0)); lz_set(&l, (int)fx, (int)fy + 1, A(0x5DBB63)); } /* un capullo */
    } else if (cab == 7) { /* corona de flores que de noche brillan */
        for (int k = 0; k < 5; k++) {
            int x = (int)(cx - rx * 0.7f + k * rx * 0.35f), y = (int)(tp + 0.6f) + (k == 0 || k == 4 ? 1 : 0);
            uint32_t c = k % 2 ? (s_amb_noche ? 0xFFF6A0 : 0xFFE066) : (s_amb_noche ? 0xFFC8F0 : 0xFF8FC8);
            lz_set(&l, x, y, A(c));
            if (s_amb_noche && (k + f) % 2 == 0) lz_set(&l, x, y - 1, A(0xFFFDE0)); /* su luz */
            if (k < 4) lz_set(&l, x + 1, y, A(0x5DBB63));
        }
    } else if (cab == 9) { /* nenúfar: una hoja redonda sobre la cabeza con su florcita rosa y una gotita */
        for (int y = (int)(tp - 0.5f); y <= (int)(tp + 1.6f); y++)
            for (int x = (int)(cx - rx * 0.8f); x <= (int)(cx + rx * 0.8f); x++) {
                float nx = (x + 0.5f - cx) / (rx * 0.8f), ny = (y + 0.5f - (tp + 0.6f)) / 1.7f;
                if (nx * nx + ny * ny <= 1.0f && !(x == (int)cx + 1 && y > (int)tp)) lz_set(&l, x, y, ((x + y) & 1) ? A(0x6FD27E) : A(0x4FB866)); /* (con su cortecito) */
            }
        lz_set(&l, (int)cx, (int)tp - 1, A(0xFF9FC8)); lz_set(&l, (int)cx - 1, (int)tp, A(0xFFC2DE)); lz_set(&l, (int)cx + 1, (int)tp, A(0xFFC2DE));
        lz_set(&l, (int)cx, (int)tp, A(0xFFE066)); lz_set(&l, (int)cx, (int)tp - 2, A(0xFFC2DE));
        lz_set(&l, (int)(cx - rx * 0.5f), (int)tp, A(f ? 0xFFFFFF : 0xCFF0FF)); /* la gotita */
    }
    /* lo que lleva en la mano: ropa_t + 1 (el paraguas, sobre la cabeza; lo demás, a un costado) */
    const int mano = ropa[3];
    const float hx = cx + rx * 1.0f, hy = cy + ry * 0.15f;
    if (mano == 10) { /* paraguas de hoja: lo sostiene abierto sobre su cabeza */
        const float ox = cx + rx * 0.25f;
        for (int k = 0; k < 4; k++) {
            const float semi = rx * (0.55f + 0.30f * k) + 0.6f;
            const int y = (int)(tp - 5.0f + k);
            for (int x = (int)(ox - semi); x <= (int)(ox + semi); x++) lz_set(&l, x, y, (x + y) % 4 == 0 ? A(0x3E9A56) : (k == 0 ? A(0x8FE39A) : A(0x5DBB63)));
        }
        const int by = (int)(tp - 1.0f);
        for (int x = (int)(ox - rx * 1.45f); x <= (int)(ox + rx * 1.45f); x += 2) lz_set(&l, x, by, A(0x3E9A56)); /* el borde festoneado */
        lz_set(&l, (int)(ox - rx * 0.6f), (int)(tp - 4.0f), A(0xE8FFE8));
        trazo(&l, hx, tp - 1.0f, hx, hy, 0.35f, A(0x8A5A32));                                  /* el palito */
        lz_set(&l, (int)hx, (int)hy, A(0xFFE0B0));
    } else if (mano == 11) { /* molinillo de viento: las aspas giran */
        const float mx = hx + 1.0f, my = hy - 3.0f;
        trazo(&l, mx, my, mx, hy + 3.0f, 0.35f, A(0xB98A5A));
        if (f) { /* en equis */
            lz_set(&l, (int)mx - 1, (int)my - 1, A(0xFF8FC0)); lz_set(&l, (int)mx + 1, (int)my - 1, A(0xFFE066));
            lz_set(&l, (int)mx - 1, (int)my + 1, A(0x7FD8FF)); lz_set(&l, (int)mx + 1, (int)my + 1, A(0xFFFFFF));
            lz_set(&l, (int)mx - 2, (int)my - 2, A(0xFFC2DE)); lz_set(&l, (int)mx + 2, (int)my - 2, A(0xFFF0A0));
            lz_set(&l, (int)mx - 2, (int)my + 2, A(0xBFEAFF)); lz_set(&l, (int)mx + 2, (int)my + 2, A(0xF4F4FF));
        } else { /* en cruz */
            lz_set(&l, (int)mx, (int)my - 1, A(0xFF8FC0)); lz_set(&l, (int)mx + 1, (int)my, A(0xFFE066));
            lz_set(&l, (int)mx, (int)my + 1, A(0x7FD8FF)); lz_set(&l, (int)mx - 1, (int)my, A(0xFFFFFF));
            lz_set(&l, (int)mx, (int)my - 2, A(0xFFC2DE)); lz_set(&l, (int)mx + 2, (int)my, A(0xFFF0A0));
            lz_set(&l, (int)mx, (int)my + 2, A(0xBFEAFF)); lz_set(&l, (int)mx - 2, (int)my, A(0xF4F4FF));
        }
        lz_set(&l, (int)mx, (int)my, A(0xE8465E));
    } else if (mano == 12) { /* farolito de papel: de noche brilla */
        const float lx = hx + 1.0f, ly = hy + 1.0f;
        trazo(&l, lx, ly - 3.0f, lx, ly - 1.0f, 0.3f, A(0x8A5A32));
        rect(&l, (int)lx - 1, (int)ly - 1, 3, 4, s_amb_noche ? A(0xFFB04A) : A(0xF07A4A));
        lz_set(&l, (int)lx, (int)ly, s_amb_noche ? A(0xFFF6B0) : A(0xFFD08A));
        rect(&l, (int)lx - 1, (int)ly - 1, 3, 1, A(0xC8482E)); rect(&l, (int)lx - 1, (int)ly + 2, 3, 1, A(0xC8482E));
        if (s_amb_noche) { lz_set(&l, (int)lx - 2, (int)ly, A(0xFFE9A0)); lz_set(&l, (int)lx + 2, (int)ly, A(0xFFE9A0)); if (f) lz_set(&l, (int)lx, (int)ly - 2, A(0xFFF6C0)); }
    }
}

/* ---------------------------------------------------------------------------
 * Criaturas propias (del editor)
 * ------------------------------------------------------------------------- */
static art_cuerpo_t *s_cuerpo[8][3];
static uint32_t s_cuerpos_version = 1;
/* para sacar el cuerpo de fábrica: art_criatura_pose se detiene antes de la cara */
static bool s_solo_cuerpo;
static int s_cap_ex0, s_cap_ex1, s_cap_ey, s_cap_flota;

uint32_t art_criaturas_version(void) { return s_cuerpos_version; }
bool art_criatura_es_propia(int especie, int etapa)
{
    return especie >= 0 && especie < 8 && etapa >= 1 && etapa <= 3 && s_cuerpo[especie][etapa - 1];
}
void art_criatura_propia(int especie, int etapa, const art_cuerpo_t *c)
{
    if (especie < 0 || especie >= 8 || etapa < 1 || etapa > 3) return;
    art_cuerpo_t **d = &s_cuerpo[especie][etapa - 1];
    if (c) {
        if (!*d) *d = malloc(sizeof(art_cuerpo_t));
        if (!*d) return;
        **d = *c;
        if ((*d)->lado != ART_CRIATURA_LADO && (*d)->lado != ART_CRIATURA_LADO_C) (*d)->lado = ART_CRIATURA_LADO;
        const int lo = (*d)->lado * 3 / 20, hi = (*d)->lado * 10 / 20;
        if ((*d)->ojos_sep < lo) (*d)->ojos_sep = (int8_t)lo;
        if ((*d)->ojos_sep > hi) (*d)->ojos_sep = (int8_t)hi;
    } else {
        free(*d);
        *d = NULL;
    }
    s_cuerpos_version++;
}

/* tiñe un color del dibujo como se tiñe el cuerpo de fábrica: color elegido, forma y enferma */
static float s_tinte_ref = -1.0f; /* el tono propio del cuerpo que se está tiñendo (para no "teñir" del mismo color) */
static float hue_ref(uint32_t c) { float h, sa, li; rgb_hsl(c, &h, &sa, &li); return sa < 0.18f ? -1.0f : h; }
static uint32_t tenir(uint32_t v, int tono, int forma, bool enferma)
{
    if (tono) {
        v = girar_tono(v, tono, s_tinte_ref, 0.9f);
    }
    if (forma == 1) v = aclarar(v, 0.10f);
    if (forma == 2) v = mezclar(v, A(0x7A7088), 0.35f);
    if (enferma) v = mezclar(v, A(0xB8DCA0), 0.33f);
    return v;
}

/* dibuja una criatura propia con la misma cara, ropa, postura y saltito que las de fábrica.
 * El cuerpo puede venir en 30x30 (editor actual) o 20x20 (editor antiguo); el lienzo trabaja en las
 * coordenadas de siempre (20x20) y cada pixel real busca de qué pixel del dibujo viene. */
static void pintar_propia(lienzo_t *l, const art_pose_t *ps, const art_cuerpo_t *cu, int forma, int tono)
{
    const int B = cu->lado;
    const float s = (float)B / ART_CRIATURA_LADO, k = KL(l); /* pixeles del dibujo y reales por pixel lógico */
    int x0 = B, x1 = -1, y0 = B, y1 = -1;
    uint32_t sr = 0, sg = 0, sb = 0, n = 0;
    for (int y = 0; y < B; y++)
        for (int x = 0; x < B; x++) {
            uint32_t v = cu->px[y * B + x];
            if ((v >> 24) < 128) continue;
            if (x < x0) x0 = x;
            if (x > x1) x1 = x;
            if (y < y0) y0 = y;
            if (y > y1) y1 = y;
            sr += (v >> 16) & 255; sg += (v >> 8) & 255; sb += v & 255; n++;
        }
    if (!n) return;
    const int frame = ps->frame, etapa = ps->etapa < 1 ? 1 : (ps->etapa > 3 ? 3 : ps->etapa);
    uint32_t medio = 0xFF000000u | (sr / n) << 16 | (sg / n) << 8 | (sb / n);
    s_tinte_ref = hue_ref(medio);
    medio = tenir(medio, tono, forma, ps->enferma);
    /* postura: se estira o se aplasta desde los pies, como el cuerpo de fábrica */
    float esy = 1.0f, esx = 1.0f;
    if (ps->postura == 1) { esy = 1.08f; esx = 0.96f; }
    if (ps->postura == -1) { esy = 0.86f; esx = 1.07f; }
    if (ps->postura == -2) { esy = 0.72f; esx = 1.16f; }
    /* el cuerpo en coordenadas lógicas (20x20) */
    const float lx0 = (float)x0 / s, lx1 = (float)(x1 + 1) / s, ly0 = (float)y0 / s, ly1 = (float)(y1 + 1) / s;
    const float cxf = (lx0 + lx1) * 0.5f, base = ly1;
    const int dy = frame ? 1 : 0; /* el saltito al caminar: baja un pixel */
    const int L = ART_CRIATURA_LADO;
    /* patitas (debajo del cuerpo) */
    if (cu->patitas && ps->postura != -2) {
        int pa = frame ? 1 : 0, sep = (int)((lx1 - lx0) * 0.3f + 0.5f);
        if (sep < 2) sep = 2;
        uint32_t pc = oscurecer(medio, 0.35f);
        int yu = (int)ceilf(base) - 1, fy = yu > L - 3 ? L - 2 : yu;
        rect(l, (int)cxf - sep - 1, fy + pa, 2, 1, pc);
        rect(l, (int)cxf + sep - 1, fy + (1 - pa), 2, 1, pc);
    }
    for (int Y = 0; Y < l->h; Y++)
        for (int X = 0; X < l->w; X++) {
            /* de dónde viene este pixel */
            float x = (X + 0.5f) / k, y = (Y + 0.5f) / k;
            float sx = cxf + (x - cxf) / esx, sy = base - (base - (y - (float)dy)) / esy;
            /* + 0,001: en el saltito el pixel cae justo en el borde; así C y el editor redondean igual */
            int ix = (int)floorf(sx * s + 0.001f), iy = (int)floorf(sy * s + 0.001f);
            if (ix < 0 || iy < 0 || ix >= B || iy >= B) continue;
            uint32_t v = cu->px[iy * B + ix];
            if ((v >> 24) < 128) continue;
            lz_real(l, X, Y, tenir(v | 0xFF000000u, tono, forma, ps->enferma));
        }
    /* cara: donde la pusiste, movida con la postura y el saltito */
    float fx0 = ((float)cu->ojo_x + 0.5f) / s, fx1 = ((float)(cu->ojo_x + cu->ojos_sep) + 0.5f) / s, fy = ((float)cu->ojo_y + 0.5f) / s;
    int ey = (int)floorf(base - (base - fy) * esy + 0.001f) + dy;
    int ex0 = (int)floorf(cxf + (fx0 - cxf) * esx + 0.001f);
    int ex1 = (int)floorf(cxf + (fx1 - cxf) * esx + 0.001f);
    int mx = (ex0 + ex1 + 2) / 2 - 1, my = ey + 3;
    pintar_cara(l, ps->expr, ex0, ex1, ey, mx, my, etapa == 1, forma, false, C_MEJILLA, ps->boca, etapa);
    float rx = (lx1 - lx0) * 0.5f * esx, ry = (ly1 - ly0) * 0.5f * esy;
    float top = base - (ly1 - ly0) * esy + (float)dy;
    pintar_ropa(l, ps->ropa, cxf, rx, top + ry, ry, top, ex0, ex1, ey);
    contornear(l, oscurecer(medio, 0.6f));
    if ((forma == 1 && etapa == 3) || ps->expr == EXP_ORGULLO) {
        int ioy = frame ? 1 : 0;
        lz_set(l, 2, 5 + ioy, C_DESTELLO); lz_set(l, 17, 3 + ioy, C_DESTELLO); lz_set(l, 18, 9 - ioy, C_DESTELLO);
    }
}

bool art_cuerpo_desde_bytes(const uint8_t *b, int n, art_cuerpo_t *c)
{
    if (!b || n < 12 || memcmp(b, "CUER", 4)) return false;
    int lado;
    if (b[4] == 1 && n == ART_CUERPO_BYTES_V1) lado = ART_CRIATURA_LADO;
    else if (b[4] == 2 && n == ART_CUERPO_BYTES) lado = ART_CRIATURA_LADO_C;
    else return false;
    memset(c, 0, sizeof *c);
    c->lado = (uint8_t)lado;
    c->ojo_x = (int8_t)b[5]; c->ojo_y = (int8_t)b[6]; c->ojos_sep = (int8_t)b[7]; c->patitas = b[8] ? 1 : 0;
    /* los mismos márgenes que en 20x20, en la grilla que venga */
    if (c->ojo_x < 1 || c->ojo_x > lado * 16 / 20 || c->ojo_y < 1 || c->ojo_y > lado * 17 / 20 || c->ojos_sep < lado * 3 / 20 ||
        c->ojos_sep > lado * 10 / 20) return false;
    int pintados = 0;
    for (int i = 0; i < lado * lado; i++) {
        const uint8_t *q = b + 12 + i * 4;
        c->px[i] = q[3] < 128 ? 0 : (0xFF000000u | (uint32_t)q[0] << 16 | (uint32_t)q[1] << 8 | q[2]);
        pintados += q[3] >= 128;
    }
    return pintados >= 6; /* algo tiene que haber dibujado */
}

void art_criatura_fabrica(int especie, int etapa, art_cuerpo_t *c)
{
    memset(c, 0, sizeof *c);
    if (etapa < 1) etapa = 1;
    if (etapa > 3) etapa = 3;
    art_img_t img = {0};
    art_pose_t ps = {0};
    ps.especie = especie & 7; ps.etapa = etapa; ps.escala = 1; ps.expr = EXP_ABIERTOS;
    s_solo_cuerpo = true;
    art_criatura_pose(&img, &ps);
    s_solo_cuerpo = false;
    if (img.px) memcpy(c->px, img.px, sizeof c->px);
    art_img_free(&img);
    const float s = (float)ART_CRIATURA_LADO_C / ART_CRIATURA_LADO;
    c->lado = ART_CRIATURA_LADO_C;
    c->ojo_x = (int8_t)lroundf(s_cap_ex0 * s);
    c->ojo_y = (int8_t)lroundf(s_cap_ey * s);
    c->ojos_sep = (int8_t)lroundf((s_cap_ex1 - s_cap_ex0) * s);
    c->patitas = (uint8_t)!s_cap_flota;
}

void art_criatura(art_img_t *dst, int especie, int etapa, int forma, int escala, art_expresion_t e, bool boca_abierta,
                  int frame, bool espejo, bool enferma)
{
    art_pose_t ps = {especie, etapa, forma, escala, e, boca_abierta, frame, espejo, enferma, 0, 0, 0, {0, 0, 0}, 0, 0};
    art_criatura_pose(dst, &ps);
}

static void criatura_nueva(lienzo_t *l, const art_pose_t *ps, int col, int especie, int tono);

/* la suciedad que se ve: manchas de tierra en la mitad de abajo del cuerpo (nunca en la cara),
 * más y más grandes según cuán sucia está; muy sucia, además se ve opaca */
static void caja_cuerpo(const lienzo_t *l, int *x0, int *y0, int *x1, int *y1)
{
    *x0 = l->w; *y0 = l->h; *x1 = -1; *y1 = -1;
    for (int Y = 0; Y < l->h; Y++)
        for (int X = 0; X < l->w; X++)
            if (l->p[Y * l->w + X] >> 24) { if (X < *x0) *x0 = X; if (X > *x1) *x1 = X; if (Y < *y0) *y0 = Y; *y1 = Y; }
}
static bool opaco(const lienzo_t *l, int X, int Y) { return X >= 0 && Y >= 0 && X < l->w && Y < l->h && (l->p[Y * l->w + X] >> 24); }

/* la suciedad que se ve: manchas de barro redondas (con borde más oscuro y un brillito) en la mitad de abajo,
 * y las patitas embarradas; nunca en la cara. Más sucia, más manchas; muy sucia, se ve opaca */
static void ensuciar(lienzo_t *l, int nivel)
{
    if (nivel <= 0) return;
    int x0, y0, x1, y1;
    caja_cuerpo(l, &x0, &y0, &x1, &y1);
    if (y1 < 0) return;
    const float k = KL(l);
    const int desde = y0 + (y1 - y0) * (nivel >= 3 ? 66 : 72) / 100; /* de la pancita para abajo: la cara queda limpia */
    if (nivel >= 3)
        for (int Y = desde; Y <= y1; Y++)
            for (int X = x0; X <= x1; X++) if (opaco(l, X, Y)) l->p[Y * l->w + X] = mezclar(l->p[Y * l->w + X], A(0x8A7A60), 0.14f);
    /* las manchas: sobre el cuerpo, lejos del borde (para que no parezca pelo) */
    const int n = nivel * 2 + 2, borde = (int)(0.7f * k + 0.5f);
    for (int m = 0, intentos = 0; m < n && intentos < 60; intentos++) {
        uint32_t h = hash(intentos, nivel, 911);
        int cx = x0 + (int)(h % (uint32_t)(x1 - x0 + 1)), cy = desde + (int)((h >> 10) % (uint32_t)(y1 - desde + 1));
        /* la cara va al centro: las manchas solo en los costados, o bien abajo (la pancita y las patas) */
        int medio = (x0 + x1) / 2, ancho = x1 - x0 + 1;
        if (abs(cx - medio) < ancho * 20 / 100 && cy < y1 - (y1 - y0) * 12 / 100) continue;
        bool dentro = true;
        for (int dy = -borde; dy <= borde && dentro; dy++)
            for (int dx = -borde; dx <= borde && dentro; dx++) dentro = opaco(l, cx + dx, cy + dy);
        if (!dentro) continue;
        m++;
        float r = (0.8f + 0.25f * (float)((h >> 20) % 3)) * k;
        for (int Y = (int)(cy - r - 1); Y <= (int)(cy + r + 1); Y++)
            for (int X = (int)(cx - r - 1); X <= (int)(cx + r + 1); X++) {
                if (!opaco(l, X, Y)) continue;
                float d = hypotf((float)(X - cx) * 0.9f, (float)(Y - cy) * 1.1f) / r;
                if (d > 1.0f) continue;
                l->p[Y * l->w + X] = d > 0.72f ? A(0x5A3E26) : A(0x7E5C3A);
            }
        if (r > 1.5f) l->p[(int)(cy - r * 0.35f) * l->w + (int)(cx - r * 0.35f)] = A(0xB08A5C); /* el brillito del barro húmedo */
        int sx = cx + (int)(r + 1.5f * k) * (((h >> 24) & 1) ? 1 : -1), sy = cy - (int)(0.8f * k);
        if (opaco(l, sx, sy) && opaco(l, sx + 1, sy)) l->p[sy * l->w + sx] = A(0x6E5234); /* una salpicadura chiquita */
    }
    /* las patitas embarradas: solo la parte de más abajo */
    const int pie = (int)(1.4f * k + 0.5f) + (nivel >= 2 ? 1 : 0);
    for (int Y = y1 - pie; Y <= y1; Y++)
        for (int X = x0; X <= x1; X++)
            if (opaco(l, X, Y) && !opaco(l, X, Y + pie + 1)) l->p[Y * l->w + X] = ((X + Y) & 1) ? A(0x6E5234) : A(0x7E5C3A);
}

/* la capa blanca élfica de la Guardiana: cae por detrás desde los hombros, se abre abajo, con ribete dorado,
 * y se cierra adelante con un broche verde (el Corazón del Mundo) */
static bool s_capa_fina; /* cuerpos largos: la capa se ve solo como un borde claro pegado al cuerpo */
static void poner_capa(lienzo_t *l)
{
    int x0, y0, x1, y1;
    caja_cuerpo(l, &x0, &y0, &x1, &y1);
    if (y1 < 0) return;
    const float k = KL(l);
    const int alto = y1 - y0 + 1, abajo = y1 + (s_capa_fina ? 0 : (int)(1.3f * k + 0.5f)), cuello = y0 + alto * 52 / 100;
    const int capucha = y0 + alto * 18 / 100; /* por detrás sube como un cuello alto, detrás de la cabeza */
    uint32_t *o = malloc((size_t)l->w * l->h * 4);
    if (!o) return;
    memcpy(o, l->p, (size_t)l->w * l->h * 4);
    /* tela blanca azulada con tres luces, el forro verde, el ribete dorado y un borde verde oscuro que la recorta del pasto */
    const uint32_t LUZ = A(0xFFFFFF), BL = A(0xF3F6FF), MED = A(0xDCE4F6), SOMB = A(0xB4C0E0), HOND = A(0x8C9AC4);
    const uint32_t RIB = A(0xF0C850), RIB2 = A(0xB8862A), BORDE = A(0x3C5A4C), FOR = A(0x7CCB94);
    const int per = (int)(2.4f * k + 0.5f) < 2 ? 2 : (int)(2.4f * k + 0.5f); /* ancho de cada pliegue */
    const int borde_h = (int)(0.9f * k + 0.5f) < 1 ? 1 : (int)(0.9f * k + 0.5f);
    for (int Y = capucha; Y <= abajo && Y < l->h; Y++) {
        int fy = Y > y1 ? y1 : Y, izq = -1, der = -1;
        for (int X = 0; X < l->w; X++) if (o[fy * l->w + X] >> 24) { if (izq < 0) izq = X; der = X; }
        if (izq < 0) continue;
        const float t = (float)(Y - capucha) / (float)(abajo - capucha + 1);
        float vf = 1.5f + t * 2.6f; /* angosta arriba, se abre hacia abajo como campana */
        if (Y > y1) { const float q = (float)(Y - y1) / (float)(abajo - y1 + 1); vf *= sqrtf(1.0f - q * q); } /* esquinas redondeadas abajo */
        int vuelo = (int)(vf * k + 0.5f); if (vuelo < 1) vuelo = 1;
        if (s_capa_fina && vuelo > (int)(0.9f * k) + 1) vuelo = (int)(0.9f * k) + 1;
        const int ctr = (izq + der) / 2;
        for (int X = izq - vuelo; X <= der + vuelo; X++) {
            if (X < 0 || X >= l->w || (o[Y * l->w + X] >> 24)) continue; /* por detrás: solo donde no hay cuerpo */
            bool cerca = false; /* y pegada al cuerpo: no rellena los huecos entre patas ni colas */
            for (int q = X - vuelo; q <= X + vuelo && !cerca; q++) if (q >= 0 && q < l->w && (o[fy * l->w + q] >> 24)) cerca = true;
            if (!cerca) continue;
            const int lado = X < ctr ? -1 : 1;
            const int ext = lado < 0 ? (izq - X) : (X - der);             /* cuánto sobresale del cuerpo */
            const bool borde_lado = ext >= vuelo - borde_h + 1;           /* el borde exterior */
            /* el dobladillo: festón de pico y valle (no en los cuerpos largos) */
            const int fest = s_capa_fina ? 0 : (int)(0.9f * k + 0.5f);
            if (fest > 0 && Y > abajo - fest - 1 && ((X / (per * 2)) & 1) && Y > abajo - fest) continue;
            const bool hem = Y >= abajo - borde_h - (fest > 0 && ((X / (per * 2)) & 1) ? 0 : 0);
            /* tela: luz a la izquierda, pliegues que bajan abiertos en abanico */
            int f = ((X - ctr) * 1000 / (int)(per * 1000 / 1)) % 3; if (f < 0) f += 3;
            uint32_t c = lado < 0 ? (f == 0 ? LUZ : (f == 1 ? BL : MED)) : (f == 0 ? MED : (f == 1 ? SOMB : MED));
            if (ext <= (int)(0.8f * k) + 1 && t < 0.7f) c = lado < 0 ? SOMB : HOND;   /* pegada al cuerpo: sombra de contacto */
            if (t < 0.18f && ext <= vuelo / 2 + 1) c = FOR;                           /* el forro verde junto al cuello */
            if (!s_capa_fina && ext >= vuelo - borde_h * 2 + 1 && ((Y / (per)) & 1) == 0 && !borde_lado) c = RIB; /* bordado dorado en la orilla */
            if (hem) c = ((X / per) & 1) ? RIB : RIB2;                                  /* el ribete dorado abajo */
            if (borde_lado || Y >= abajo) c = BORDE;
            if (Y == capucha) c = BORDE;
            l->p[Y * l->w + X] = c;
        }
    }
    if (!s_capa_fina) {
        const int ctrx = (x0 + x1) / 2;
        /* el broche: el Corazón del Mundo, una gema verde engarzada en oro, abajo en el centro del cuerpo */
        const int bx = ctrx, by = y1 - (int)(1.5f * k);
        const int r = (int)(1.7f * k + 0.5f) < 2 ? 2 : (int)(1.7f * k + 0.5f);
        for (int Y = by - r - 1; Y <= by + r + 1; Y++)
            for (int X = bx - r - 1; X <= bx + r + 1; X++) {
                const float d = hypotf((float)(X - bx), (float)(Y - by) * 1.05f);
                if (d > (float)r + 0.6f) continue;
                uint32_t c = d > (float)r - 0.4f ? RIB2 : (d > (float)r * 0.62f ? RIB : A(0x2FC77A)); /* oro y la gema */
                if (d <= (float)r * 0.62f && X < bx && Y < by) c = A(0xB8FFD6);                      /* el brillito */
                else if (d <= (float)r * 0.62f && X >= bx && Y >= by) c = A(0x178A52);
                lz_real(l, X, Y, c);
            }
    }
    free(o);
}

/* ======================================================================
 * Brotito dibujado a mano (pixel art de verdad): el cuerpo sin cara en tres etapas, con las hojas caídas, normales o
 * alertas, y la cara aparte para cada expresión (con la boca cerrada o abierta). Se arma en el tamaño del dibujo, se
 * tiñe con los siete colores (solo lo verde) y se pasa al lienzo con la postura, el saltito, la ropa, la suciedad y la capa
 * ==================================================================== */
/* 1 = usa los personajes dibujados a mano de brotito_data.inc y personajes_data.inc; 0 = los diseños originales (pausados por ahora) */
#define PERSONAJES_PINTADOS 0
#include "brotito_data.inc"
static bool fd_lz(const uint8_t *src, const uint8_t *fin, uint8_t *dst, uint32_t total);
typedef struct { const uint8_t *pal; const uint8_t *bd[3]; uint16_t bn[3]; const uint8_t *od; uint16_t on; int ox, oy, ow, oh, W, H; const float *g; int lado; bool izq, hi; } pint_t;
#include "personajes_data.inc"
static void criatura_pintada(art_img_t *dst, const art_pose_t *ps, int etapa, int forma, int tono, int escala, bool espejo, const pint_t *V);
static void criatura_brotito(art_img_t *dst, const art_pose_t *ps, int etapa, int forma, int tono, int escala, bool espejo)
{
    pint_t V;
    V.pal = BR_ST(etapa);
    const int ex = ps->expr < 0 || ps->expr >= EXP_COUNT ? 0 : ps->expr, oi = ex * 2 + (ps->boca ? 1 : 0);
    for (int i = 0; i < 3; i++) {
        switch (etapa) { case 1: V.bd[i] = BR1_BODY[i].d; V.bn[i] = BR1_BODY[i].n; break; case 2: V.bd[i] = BR2_BODY[i].d; V.bn[i] = BR2_BODY[i].n; break; default: V.bd[i] = BR3_BODY[i].d; V.bn[i] = BR3_BODY[i].n; }
    }
    switch (etapa) {
    case 1: V.od = BR1_OV[oi].d; V.on = BR1_OV[oi].n; V.ox = BR1_OV[oi].x; V.oy = BR1_OV[oi].y; V.ow = BR1_OV[oi].w; V.oh = BR1_OV[oi].h; break;
    case 2: V.od = BR2_OV[oi].d; V.on = BR2_OV[oi].n; V.ox = BR2_OV[oi].x; V.oy = BR2_OV[oi].y; V.ow = BR2_OV[oi].w; V.oh = BR2_OV[oi].h; break;
    default: V.od = BR3_OV[oi].d; V.on = BR3_OV[oi].n; V.ox = BR3_OV[oi].x; V.oy = BR3_OV[oi].y; V.ow = BR3_OV[oi].w; V.oh = BR3_OV[oi].h; break;
    }
    V.W = BR_GEO[etapa - 1].w; V.H = BR_GEO[etapa - 1].h; V.g = &BR_GEO[etapa - 1].cx; V.lado = ((const int[4]){0, 104, 124, 136})[etapa]; V.izq = false; V.hi = false;
    criatura_pintada(dst, ps, etapa, forma, tono, escala, espejo, &V);
}
/* Chispa y Burbu (y las que sigan): la misma maquinaria, con los datos de personajes_data.inc */
static bool criatura_pd(art_img_t *dst, const art_pose_t *ps, int especie, int etapa, int forma, int tono, int escala, bool espejo)
{
    if (especie < 0 || especie > 7 || !PD[especie][etapa - 1].pal) return false;
    const pd_etapa_t *E = &PD[especie][etapa - 1];
    pint_t V;
    V.pal = E->pal;
    for (int i = 0; i < 3; i++) { V.bd[i] = E->body[i].d; V.bn[i] = E->body[i].n; }
    const int ex = ps->expr < 0 || ps->expr >= EXP_COUNT ? 0 : ps->expr, oi = ex * 2 + (ps->boca ? 1 : 0);
    V.od = E->ov[oi].d; V.on = E->ov[oi].n; V.ox = E->ov[oi].x; V.oy = E->ov[oi].y; V.ow = E->ov[oi].w; V.oh = E->ov[oi].h;
    V.W = E->w; V.H = E->h; V.g = E->geo; V.lado = E->lado; V.izq = E->mira_izq; V.hi = E->hi;
    criatura_pintada(dst, ps, etapa, forma, tono, escala, espejo, &V);
    return true;
}
static void criatura_pintada(art_img_t *dst, const art_pose_t *ps, int etapa, int forma, int tono, int escala, bool espejo, const pint_t *V)
{
    const int ppx = (escala * 2 + 2) / 5 < 1 ? 1 : (escala * 2 + 2) / 5;
    const int S = V->lado * ppx / 2;
    const float pp = V->hi ? (float)ppx * 0.5f : (float)ppx; /* tamaño de cada pixel del dibujo en pantalla (los de doble resolución, la mitad) */
    uint32_t *buf = buf_trabajo(S * S);
    if (!buf) return;
    lienzo_t l = {S, S, buf, (float)S / 20.0f};
    const int W = V->W, H = V->H;
    const uint8_t *pal = V->pal;
    uint32_t col[256];
    col[0] = 0;
    { /* el tono propio: el promedio de lo que se tiñe */
        uint32_t qr = 0, qg = 0, qb = 0, qn = 0;
        for (int i = 0; i < pal[0]; i++) { const uint8_t *q = pal + 1 + i * 4; if (q[3]) { qr += q[0]; qg += q[1]; qb += q[2]; qn++; } }
        s_tinte_ref = qn ? hue_ref(A((qr / qn) << 16 | (qg / qn) << 8 | (qb / qn))) : -1.0f;
    }
    for (int i = 0; i < pal[0]; i++) {
        const uint8_t *q = pal + 1 + i * 4;
        uint32_t v = A((uint32_t)q[0] << 16 | (uint32_t)q[1] << 8 | q[2]);
        col[i + 1] = q[3] ? tenir(v, tono, forma, ps->enferma) : v;
    }
    uint8_t *ix = malloc((size_t)W * H);
    uint32_t *arte = calloc((size_t)W * H, 4);
    if (!ix || !arte) { free(ix); free(arte); return; }
    const int dz = (ps->orejas < 0 || (etapa == 3 && forma == 2 && V->izq == false)) ? 0 : (ps->orejas > 0 ? 2 : 1);
    if (fd_lz(V->bd[dz], V->bd[dz] + V->bn[dz], ix, (uint32_t)W * H))
        for (int i = 0; i < W * H; i++) arte[i] = col[ix[i]];
    const int ow = V->ow, oh = V->oh, ox0 = V->ox, oy0 = V->oy;
    if (ow > 0 && fd_lz(V->od, V->od + V->on, ix, (uint32_t)ow * oh))
        for (int y = 0; y < oh; y++)
            for (int x = 0; x < ow; x++)
                if (ix[y * ow + x] && ox0 + x < W && oy0 + y < H) arte[(oy0 + y) * W + ox0 + x] = col[ix[y * ow + x]];
    /* postura: se estira o se aplasta desde los pies; saltito al caminar */
    float esy = 1.0f, esx = 1.0f;
    if (ps->postura == 1) { esy = 1.08f; esx = 0.96f; }
    if (ps->postura == -1) { esy = 0.86f; esx = 1.07f; }
    if (ps->postura == -2) { esy = 0.72f; esx = 1.16f; }
    const int dyp = ps->frame ? ppx : 0;
    const float cxf = S * 0.5f, ox = ((float)S - (float)W * pp) * 0.5f, oy = (float)S - (float)H * pp - 3.0f * pp, base = oy + (float)H * pp;
    for (int Y = 0; Y < S; Y++)
        for (int X = 0; X < S; X++) {
            float sx = cxf + ((float)X + 0.5f - cxf) / esx, sy = base - (base - ((float)Y + 0.5f - (float)dyp)) / esy;
            int ax = (int)floorf((sx - ox) / pp), ay = (int)floorf((sy - oy) / pp);
            if (ax < 0 || ay < 0 || ax >= W || ay >= H) continue;
            uint32_t v = arte[ay * W + ax];
            if (v) l.p[Y * S + X] = v;
        }
    free(ix); free(arte);
    /* la ropa, sobre la cabeza (en las coordenadas de siempre, 20x20) */
    {
        const float K = l.k, g_ppx = pp;
        const float *g = V->g;
#define BX(a) ((cxf + (ox + (a) * g_ppx - cxf) * esx) / K)
#define BY(a) ((base - (base - (oy + (a) * g_ppx)) * esy + (float)dyp) / K)
        s_cara_frame = ps->frame & 1;
        pintar_ropa(&l, ps->ropa, BX(g[0]), g[2] * g_ppx * esx / K, BY(g[1] + g[3] * 0.62f), g[3] * g_ppx * esy / K, BY(g[4]), (int)BX(g[5]), (int)BX(g[6]), (int)BY(g[7]));
#undef BX
#undef BY
    }
    if ((forma == 1 && etapa == 3) || ps->expr == EXP_ORGULLO) {
        int ioy = ps->frame ? 1 : 0;
        lz_set(&l, 2, 5 + ioy, C_DESTELLO); lz_set(&l, 17, 3 + ioy, C_DESTELLO); lz_set(&l, 18, 9 - ioy, C_DESTELLO);
    }
    if (ps->capa && !V->izq) poner_capa(&l); /* los cuerpos largos (Chispa, Burbu) van sin capa */
    ensuciar(&l, ps->capa && !V->izq ? 0 : ps->sucio);
    ampliar(dst, &l, 1, espejo != V->izq);
}

/* La pantalla de bienvenida: fondo nocturno, el borde del nido (va por delante del huevo) y el huevo del misterio */
#include "inicio_data.inc"
#include "vivero_data.inc"
static bool in_capa(art_img_t *dst, const uint8_t *pal, const uint8_t *lz, uint32_t nlz, int w, int h, int esc)
{
    uint8_t *ix = malloc((size_t)w * h);
    if (!ix) return false;
    if (!fd_lz(lz, lz + nlz, ix, (uint32_t)w * h)) { free(ix); return false; }
    img_preparar(dst, w * esc, h * esc);
    if (!dst->px) { free(ix); return false; }
    uint32_t col[256];
    col[0] = 0;
    for (int i = 0; i < pal[0]; i++) col[i + 1] = A((uint32_t)pal[1 + i * 3] << 16 | (uint32_t)pal[2 + i * 3] << 8 | pal[3 + i * 3]);
    const int W = w * esc;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            const uint32_t v = col[ix[y * w + x]];
            for (int j = 0; j < esc; j++)
                for (int i = 0; i < esc; i++) dst->px[(y * esc + j) * W + x * esc + i] = v;
        }
    free(ix);
    return true;
}
bool art_noche(art_img_t *fondo)
{
    return in_capa(fondo, VV_NOCHE_PAL, VV_NOCHE_LZ, sizeof VV_NOCHE_LZ, VV_NOCHE_W, VV_NOCHE_H, 2);
}
bool art_vivero(art_img_t *fondo, art_img_t *frente)
{
    return in_capa(fondo, VV_FONDO_PAL, VV_FONDO_LZ, sizeof VV_FONDO_LZ, VV_FONDO_W, VV_FONDO_H, 2) &&
           in_capa(frente, VV_FRENTE_PAL, VV_FRENTE_LZ, sizeof VV_FRENTE_LZ, VV_FRENTE_W, VV_FRENTE_H, 2);
}
#include "cristales_data.inc"
#include "pantallas_data.inc"
/* la cueva del juego de los cristales y el dragón pintado (3 cuadros: dormido, despierto, alas abiertas) */
void art_cristales_fondo(art_img_t *dst) { in_capa(dst, CJ_FONDO_PAL, CJ_FONDO_LZ, sizeof CJ_FONDO_LZ, CJ_FONDO_W, CJ_FONDO_H, 2); }
static lv_image_dsc_t s_cj_dsc;
static uint16_t *s_cj_px;
/* la cueva como imagen opaca RGB565 de 466x466 (la mitad de memoria que ARGB y se pinta mucho más rápido) */
const lv_image_dsc_t *art_cristales_fondo565(void)
{
    if (s_cj_px) return &s_cj_dsc;
    const int w = CJ_FONDO_W, h = CJ_FONDO_H;
    uint8_t *ix = malloc((size_t)w * h);
    if (!ix) return NULL;
    if (!fd_lz(CJ_FONDO_LZ, CJ_FONDO_LZ + sizeof CJ_FONDO_LZ, ix, (uint32_t)w * h)) { free(ix); return NULL; }
    uint16_t *px = malloc((size_t)w * 2 * h * 2 * 2);
    if (!px) { free(ix); return NULL; }
    uint16_t col[257];
    col[0] = 0x10A3;
    for (int i = 0; i < CJ_FONDO_PAL[0]; i++)
        col[i + 1] = (uint16_t)(((CJ_FONDO_PAL[1 + i * 3] & 0xF8) << 8) | ((CJ_FONDO_PAL[2 + i * 3] & 0xFC) << 3) | (CJ_FONDO_PAL[3 + i * 3] >> 3));
    const int W = w * 2;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            const uint16_t v = col[ix[y * w + x]];
            uint16_t *o = px + (size_t)(y * 2) * W + x * 2;
            o[0] = o[1] = o[W] = o[W + 1] = v;
        }
    free(ix);
    memset(&s_cj_dsc, 0, sizeof s_cj_dsc);
    s_cj_dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
    s_cj_dsc.header.cf = LV_COLOR_FORMAT_RGB565;
    s_cj_dsc.header.w = (uint32_t)W;
    s_cj_dsc.header.h = (uint32_t)(h * 2);
    s_cj_dsc.header.stride = (uint32_t)(W * 2);
    s_cj_dsc.data_size = (uint32_t)(W * h * 2 * 2);
    s_cj_dsc.data = (const uint8_t *)px;
    s_cj_px = px;
    return &s_cj_dsc;
}
void art_cristales_fondo565_liberar(void)
{
    if (!s_cj_px) return;
    lv_image_cache_drop(&s_cj_dsc);
    free(s_cj_px);
    s_cj_px = NULL;
}

void art_dragon_pintado(art_img_t *dst, int fase, int esc)
{
    switch (fase) {
    case 0: in_capa(dst, CJ_DRAG0_PAL, CJ_DRAG0_LZ, sizeof CJ_DRAG0_LZ, CJ_DRAG0_W, CJ_DRAG0_H, esc); break;
    case 1: in_capa(dst, CJ_DRAG1_PAL, CJ_DRAG1_LZ, sizeof CJ_DRAG1_LZ, CJ_DRAG1_W, CJ_DRAG1_H, esc); break;
    default: in_capa(dst, CJ_DRAG2_PAL, CJ_DRAG2_LZ, sizeof CJ_DRAG2_LZ, CJ_DRAG2_W, CJ_DRAG2_H, esc); break;
    }
}
bool art_bienvenida(art_img_t *fondo, art_img_t *frente, art_img_t *huevo)
{
    return in_capa(fondo, IN_FONDO_PAL, IN_FONDO_LZ, sizeof IN_FONDO_LZ, IN_FONDO_W, IN_FONDO_H, 2) &&
           in_capa(frente, IN_FRENTE_PAL, IN_FRENTE_LZ, sizeof IN_FRENTE_LZ, IN_FRENTE_W, IN_FRENTE_H, 2) &&
           in_capa(huevo, IN_HUEVO_PAL, IN_HUEVO_LZ, sizeof IN_HUEVO_LZ, IN_HUEVO_W, IN_HUEVO_H, 2);
}

/* Los ocho huevos dibujados a mano (crema, con una pista de quién es): 4 grados de grieta; el bamboleo los inclina */
#include "huevos_data.inc"
static void huevo_pintado(art_img_t *dst, int especie, int escala, int grietas, int inclinacion)
{
    const int ppx = (escala * 2 + 2) / 5 < 1 ? 1 : (escala * 2 + 2) / 5, S = 104 * ppx / 2;
    uint32_t *buf = buf_trabajo(S * S);
    if (!buf) return;
    lienzo_t l = {S, S, buf, (float)S / 20.0f};
    uint8_t *ix = malloc((size_t)HU_W * HU_H);
    if (!ix) return;
    const int g = grietas < 0 ? 0 : (grietas > 3 ? 3 : grietas), e = especie & 7;
    uint32_t col[256];
    col[0] = 0;
    for (int i = 0; i < HU_PAL[0]; i++) col[i + 1] = A((uint32_t)HU_PAL[1 + i * 3] << 16 | (uint32_t)HU_PAL[2 + i * 3] << 8 | HU_PAL[3 + i * 3]);
    if (fd_lz(HU_SPR[e][g].d, HU_SPR[e][g].d + HU_SPR[e][g].n, ix, (uint32_t)HU_W * HU_H)) {
        /* (106) el dibujo es del doble de fino (92 x 90): ocupa lo mismo en pantalla que antes, con más detalle.
         * A escala 5 y 6 (la alarma y la pantalla de elegir huevo) sale pixel a pixel, sin agrandar nada. */
        const int bw = HU_W * ppx / 2, bh = HU_H * ppx / 2;
        const int ox = (S - bw) / 2, oy = S - bh - 3 * ppx;
        for (int Y = 0; Y < bh; Y++) {
            /* inclinado: arriba se corre más que abajo, como un huevo que se bambolea */
            const int sh = inclinacion * ppx * 3 * (bh - Y) / bh;
            const uint8_t *fila = ix + (size_t)(Y * HU_H / bh) * HU_W;
            for (int X = 0; X < bw; X++) {
                const uint8_t c = fila[X * HU_W / bw];
                if (!c) continue;
                lz_real(&l, ox + sh + X, oy + Y, col[c]);
            }
        }
    }
    free(ix);
    ampliar(dst, &l, 1, false);
}

static void art_criatura_pose_base(art_img_t *dst, const art_pose_t *ps)
{
    int especie = ps->especie, etapa = ps->etapa, forma = ps->forma, escala = ps->escala, frame = ps->frame;
    art_expresion_t e = ps->expr;
    const bool boca_abierta = ps->boca, espejo = ps->espejo, enferma = ps->enferma;
    s_cara_frame = ps->frame & 1;
    const int orejas = ps->orejas, cola = ps->cola, postura = ps->postura;
    /* el aspecto viene en un número: especie | mezcla << 3 | color << 7 (ver pet_look) */
    const int mez = (especie >> 3) & 15, tono = (especie >> 7) & 7;
    especie = (int)((unsigned)especie % 8);
    const art_cuerpo_t *propio = s_solo_cuerpo ? NULL : s_cuerpo[especie][(etapa < 1 ? 1 : (etapa > 3 ? 3 : etapa)) - 1];
    float kd = s_solo_cuerpo ? (float)ART_CRIATURA_LADO_C / ART_CRIATURA_LADO : k_de_escala(escala);
    if (propio && propio->lado <= ART_CRIATURA_LADO) kd = 1.0f; /* dibujada en la grilla antigua: se ve como antes */
    if (!propio && !s_solo_cuerpo && s_estilo && !PERSONAJES_PINTADOS && escala >= 2) kd = (float)escala; /* los personajes nuevos se dibujan al doble de detalle (píxeles de 1 en vez de 2) */
    const int LR = (int)lroundf(ART_CRIATURA_LADO * kd), AMP = s_solo_cuerpo ? 1 : (int)lroundf((float)escala / kd);
    uint32_t *buf = buf_trabajo(LR * LR);
    if (!buf) return;
    lienzo_t l = {LR, LR, buf, kd};
    const int rasgo = mez ? (mez - 1) % 8 : especie; /* de quién son las orejas, la cola y las crestas */
    if (etapa < 1) etapa = 1;
    if (etapa > 3) etapa = 3;
    if (etapa < 3) forma = 0; /* la forma se nota recién de adulta */
    paleta_t P = paleta_forma(especie, forma, enferma, tono);
    if (mez) { /* los detalles de la otra especie, con sus colores */
        paleta_t Q = paleta_forma(rasgo, forma, enferma, 0);
        P.acento = Q.acento; P.acento2 = Q.acento2;
    }
    if (PERSONAJES_PINTADOS && !propio && !s_solo_cuerpo && especie == 0) { /* Brotito: el dibujo hecho a mano */
        criatura_brotito(dst, ps, etapa, forma, tono, escala, espejo);
        return;
    }
    if (PERSONAJES_PINTADOS && !propio && !s_solo_cuerpo && criatura_pd(dst, ps, especie, etapa, forma, tono, escala, espejo)) return;
    if (propio) { /* la dibujaste tú en el editor */
        pintar_propia(&l, ps, propio, forma, tono);
        if (ps->capa) poner_capa(&l);
        ensuciar(&l, ps->capa ? 0 : ps->sucio);
        ampliar(dst, &l, AMP, espejo);
        return;
    }
    if (s_estilo && !s_solo_cuerpo) { /* los personajes nuevos */
        art_pose_t q = *ps;
        q.etapa = etapa;
        criatura_nueva(&l, &q, especie, mez && etapa == 3 ? (mez - 1) & 7 : especie, tono); /* de adulta, la forma de su linaje */
        if (ps->capa) poner_capa(&l);
        ensuciar(&l, ps->capa ? 0 : ps->sucio);
        ampliar(dst, &l, AMP, espejo);
        return;
    }
    const bool flota = especie == 5 || especie == 7; /* nube y fantasma no tienen pies */
    const float oy = frame ? 1.0f : 0.0f;
    const int ioy = (int)oy;

    /* tamaño por etapa: la bebé es una bolita, la adulta llena el cuadro */
    static const float RX[4] = {0, 4.6f, 5.6f, 6.6f}, RY[4] = {0, 4.3f, 5.2f, 6.2f}, CY[4] = {0, 14.4f, 13.5f, 12.6f};
    float cx = 10.0f, rx = RX[etapa], ry = RY[etapa], cy = CY[etapa] + oy - (flota ? 1.0f : 0.0f);
    /* postura: erguida (orgullo), encogida (miedo), echada (siesta, sombra) */
    if (postura == 1) { float a0 = ry; ry *= 1.08f; rx *= 0.96f; cy -= (ry - a0); }
    if (postura == -1) { float a0 = ry; ry *= 0.86f; rx *= 1.07f; cy += (a0 - ry); }
    if (postura == -2) { float a0 = ry; ry *= 0.72f; rx *= 1.16f; cy += (a0 - ry) + 1.0f; }
    /* orejas, crestas, hojas y antenas: caídas (triste, cansada) o alertas (curiosa) */
    const float alza = orejas < 0 ? 0.55f : (orejas > 0 ? 1.25f : 1.0f);
    /* cola: quieta, moviéndose (contenta) o baja (triste) */
    const float colay = cola < 0 ? 2.2f : (cola > 0 ? (frame ? -1.2f : 0.6f) : 0.0f);
    const float top = cy - ry; /* coronilla */
    int ex0 = (int)(cx - rx * 0.55f), ex1 = (int)(cx + rx * 0.55f) - 1;
    int ey = (int)(cy - ry * 0.25f);
    int mx = 9, my = ey + 3;
    bool panza = true, ojos_grandes = etapa == 1;
    uint32_t mejilla = C_MEJILLA;

    /* --- pies --- */
    if (!flota && postura != -2 && !s_solo_cuerpo) {
        int pa = frame ? 1 : 0, sep = etapa == 1 ? 2 : (etapa == 2 ? 3 : 4);
        rect(&l, 10 - sep - 1, 18 + pa, 2, 1, P.oscuro);
        rect(&l, 10 + sep - 1, 18 + (1 - pa), 2, 1, P.oscuro);
    }

    /* --- detrás del cuerpo --- */
    switch (rasgo) {
    case 0: /* Brotito: hojas en la cabeza */
        if (etapa == 1) {
            trazo(&l, 10, top + 0.5f, 10, top - 1.5f * alza, 0.4f, P.acento);
            elipse(&l, 11.6f, top - 2.0f * alza + (orejas < 0 ? 0.8f : 0), 1.4f, 0.8f, A(P.acento));
        } else if (etapa == 2) {
            trazo(&l, 10, top + 0.5f, 10, top - 1.8f * alza, 0.4f, A(P.acento));
            elipse(&l, orejas < 0 ? 7.2f : 8.0f, top - 2.0f * alza + (orejas < 0 ? 1.0f : 0), 2.0f, 1.0f, A(P.acento));
            elipse(&l, orejas < 0 ? 12.8f : 12.0f, top - 2.0f * alza + (orejas < 0 ? 1.0f : 0), 2.0f, 1.0f, A(P.acento));
        } else {
            bool mustia = forma == 2 || orejas < 0;
            triangulo(&l, 10, top + 1, 4.0f, top - (mustia ? 0.5f : 3.5f), 7.5f, top + 1.5f, A(P.acento));
            triangulo(&l, 10, top + 1, 16.0f, top - (mustia ? 0.5f : 3.5f), 12.5f, top + 1.5f, A(P.acento));
            trazo(&l, 10, top + 0.5f, 10, top - 3.0f, 0.5f, A(P.acento));
            /* hojitas como brazos */
            elipse(&l, cx - rx - 0.5f, cy + 1.0f, 1.6f, 0.9f, A(P.acento));
            elipse(&l, cx + rx + 0.5f, cy + 1.0f, 1.6f, 0.9f, A(P.acento));
        }
        break;
    case 1: /* Chispa: cresta de rayos y cola en espiral */
        if (etapa == 1) {
            zigzag(&l, 10.0f, top + 0.5f, 3.5f * alza, 1.2f, P.oscuro);
            lz_set(&l, 11, (int)(top - 3.2f * alza), A(P.acento));
        } else {
            float alto = (etapa == 2 ? 4.0f : 5.5f) * alza;
            zigzag(&l, 9.0f, top + 0.8f, alto, 1.4f, P.oscuro);
            if (etapa == 3) { zigzag(&l, 6.3f, top + 1.8f, alto - 1.5f, 1.1f, P.oscuro); zigzag(&l, 12.2f, top + 1.6f, alto - 1.2f, 1.1f, P.oscuro); }
            /* cola enrollada que termina en chispa */
            float tx = cx + rx - 0.5f, ty = cy + 2.5f;
            trazo(&l, tx, ty, tx + 2.5f, ty - 1.0f + colay * 0.4f, 0.6f, P.cuerpo);
            trazo(&l, tx + 2.5f, ty - 1.0f + colay * 0.4f, tx + 2.8f, ty - 3.5f + colay, 0.6f, P.cuerpo);
            estrellita(&l, (int)(tx + 2.8f), (int)(ty - 4.8f + colay), A(P.acento));
        }
        mejilla = A(P.acento);
        break;
    case 2: /* Ascua: llama en la cabeza y cola con brasa */
        if (etapa == 1) llama(&l, 10.0f, top + 1.5f, 3.5f * alza, 1.6f, &P);
        else if (etapa == 2) llama(&l, 10.0f, top + 1.8f, 5.0f * alza, 2.4f, &P);
        else {
            llama(&l, 7.5f, top + 2.5f, 4.5f * alza, 2.2f, &P);
            llama(&l, 12.5f, top + 2.5f, 4.5f * alza, 2.2f, &P);
            llama(&l, 10.0f, top + 1.8f, 6.5f * alza, 2.6f, &P);
        }
        if (etapa >= 2) {
            float tx = cx + rx - 1.0f, ty = cy + 3.0f;
            trazo(&l, tx, ty, tx + 2.5f, ty - 2.0f + colay, 0.7f, P.cuerpo);
            llama(&l, tx + 2.8f, ty - 2.0f + colay, (etapa == 3 ? 4.5f : 3.0f) * (cola < 0 ? 0.6f : 1.0f), 1.4f, &P);
        }
        break;
    case 3: /* Burbu: branquias rosadas a los lados de la cabeza y aleta */
    {
        int n = etapa; /* 1, 2 o 3 frondas por lado */
        for (int k = 0; k < n; k++) {
            float a = -0.6f + k * 0.55f + (orejas < 0 ? 0.9f : (orejas > 0 ? -0.3f : 0.0f)), largo = 2.2f + etapa * 0.6f;
            float bx = cx - rx + 0.8f, by = cy - ry * 0.35f + k * 1.4f;
            trazo(&l, bx, by, bx - largo * cosf(a), by - largo * sinf(a) - 1.0f, 0.65f, A(P.acento));
            bx = cx + rx - 0.8f;
            trazo(&l, bx, by, bx + largo * cosf(a), by - largo * sinf(a) - 1.0f, 0.65f, A(P.acento));
        }
        if (etapa >= 2) triangulo(&l, cx + rx - 1, cy + 1, 19.5f, cy - 1.5f + colay, 19.0f, cy + 3.5f + colay * 0.5f, A(P.acento2));
        if (etapa == 3) { /* cresta ondulada */
            for (int x = 7; x <= 13; x++) lz_set(&l, x, (int)top - ((x & 1) ? 1 : 0), A(P.acento2));
        }
        mejilla = A(P.acento);
        break;
    }
    case 4: /* Roco: placas de roca en la espalda */
        if (etapa >= 2) {
            int n = etapa == 2 ? 2 : 3;
            for (int k = 0; k < n; k++) {
                float bx = 6.0f + k * (8.0f / (n - 1 + 0.001f)), by = top + 1.8f;
                if (n == 2) bx = 7.5f + k * 5.0f;
                triangulo(&l, bx - 2.0f, by + 1.0f, bx + 2.0f, by + 1.0f, bx + 0.4f, by - (etapa == 3 ? 4.0f : 2.8f) * alza, A(P.acento2));
            }
        }
        if (etapa == 3) { /* cola de piedra */
            elipse(&l, cx + rx + 0.4f, cy + 3.0f + colay * 0.5f, 2.2f, 1.6f, A(P.acento2));
        }
        break;
    case 5: /* Nubi: copos de nube alrededor */
        circulo(&l, cx - rx * 0.75f, cy + ry * 0.45f, ry * 0.55f, P.cuerpo);
        circulo(&l, cx + rx * 0.75f, cy + ry * 0.45f, ry * 0.55f, P.oscuro);
        circulo(&l, cx - rx * 0.35f, top + 1.0f, ry * 0.45f, P.claro);
        if (etapa >= 2) circulo(&l, cx + rx * 0.3f, top + 0.6f, ry * 0.5f, P.claro);
        if (etapa == 3) {
            /* gotitas que caen como colita */
            lz_set(&l, (int)(cx + rx + 1), (int)(cy + 3) + ((frame) ? 1 : 0), A(P.acento));
            lz_set(&l, (int)(cx + rx + 2), (int)(cy + 5) - ((frame) ? 1 : 0), A(P.acento));
        }
        panza = false;
        break;
    case 6: /* Lumi: antenas con estrella y alitas */
        if (etapa == 1) {
            estrellita(&l, 10, (int)(top - 1.5f), A(P.acento));
        } else {
            float ax = orejas < 0 ? 2.5f : 1.5f, ay = orejas < 0 ? 0.0f : 2.0f * alza;
            trazo(&l, 8.0f, top + 1.0f, 8.0f - ax, top - ay, 0.4f, P.oscuro);
            trazo(&l, 12.0f, top + 1.0f, 12.0f + ax, top - ay, 0.4f, P.oscuro);
            estrellita(&l, (int)(8.0f - ax - 0.5f), (int)(top - ay - 0.5f), A(P.acento));
            estrellita(&l, (int)(12.0f + ax + 0.5f), (int)(top - ay - 0.5f), A(P.acento));
        }
        if (etapa == 3) {
            elipse(&l, cx - rx - 0.6f, cy - 1.5f + (frame ? -0.5f : 0), 1.6f, 2.8f, A(P.acento2));
            elipse(&l, cx + rx + 0.6f, cy - 1.5f + (frame ? -0.5f : 0), 1.6f, 2.8f, A(P.acento2));
        }
        break;
    case 7: /* Brumo: punta de capucha y estela */
        if (etapa >= 2) triangulo(&l, cx - 2.5f, top + 2.0f, cx + 2.5f, top + 1.5f, cx + 3.5f + (frame ? 0.5f : 0) + (orejas < 0 ? 1.5f : 0), top - (etapa == 3 ? 4.0f : 2.5f) * alza, P.cuerpo);
        if (etapa == 3) {
            elipse(&l, cx - rx - 0.3f, cy + 0.5f, 1.2f, 1.6f, P.cuerpo);
            elipse(&l, cx + rx + 0.3f, cy + 0.5f, 1.2f, 1.6f, P.oscuro);
        }
        panza = false;
        break;
    default: break;
    }

    cuerpo(&l, cx, cy, rx, ry, &P);

    /* --- delante del cuerpo --- */
    switch (rasgo) {
    case 0:
        if (etapa == 3) {
            int fx = 10, fy = (int)(top - 3.5f);
            if (forma == 1) { /* radiante: flor abierta */
                lz_set(&l, fx, fy - 1, C_FLOR); lz_set(&l, fx - 1, fy, C_FLOR); lz_set(&l, fx + 1, fy, C_FLOR);
                lz_set(&l, fx, fy + 1, C_FLOR); lz_set(&l, fx, fy, C_FLOR_C);
            } else if (forma == 0) { lz_set(&l, fx, fy, A(P.nariz)); lz_set(&l, fx, fy - 1, A(P.nariz)); }
        }
        break;
    case 1: /* barriga con un rayito */
        if (etapa >= 2) { lz_set(&l, 10, (int)(cy + 2), P.oscuro); lz_set(&l, 9, (int)(cy + 3), P.oscuro); lz_set(&l, 10, (int)(cy + 3), P.oscuro); lz_set(&l, 9, (int)(cy + 4), P.oscuro); }
        break;
    case 4: /* manchas de piedra y cristal en la frente */
        lz_set(&l, (int)(cx + rx * 0.5f), (int)(cy + ry * 0.2f), P.oscuro);
        lz_set(&l, (int)(cx - rx * 0.6f), (int)(cy + ry * 0.5f), P.oscuro);
        if (etapa == 3) {
            uint32_t cr = forma == 1 ? A(0xFF9EC4) : A(P.acento);
            lz_set(&l, 10, (int)top + 1, cr); lz_set(&l, 9, (int)top + 2, cr); lz_set(&l, 10, (int)top + 2, aclarar(cr, 0.5f)); lz_set(&l, 11, (int)top + 2, cr);
        }
        break;
    case 6:
        if (etapa >= 2) estrellita(&l, 10, (int)(top + 2), A(P.acento));
        break;
    case 7: /* borde ondulado del fantasma */
        for (int x = (int)(cx - rx); x <= (int)(cx + rx); x++) {
            int yb = (int)(cy + ry) + (((x + frame) % 3 == 0) ? 1 : 0);
            lz_set(&l, x, yb, P.oscuro);
            lz_set(&l, x, yb + 1, ((x + frame) % 3 == 0) ? P.oscuro : TRANSP);
        }
        break;
    default: break;
    }
    if (panza) elipse(&l, 10.0f, cy + ry * 0.55f, rx * 0.58f, ry * 0.38f, A(P.panza));
    if (s_solo_cuerpo) { /* para el editor: el cuerpo solo, y dónde van los ojos */
        s_cap_ex0 = ex0; s_cap_ex1 = ex1; s_cap_ey = ey; s_cap_flota = flota;
        ampliar(dst, &l, 1, false);
        return;
    }

    /* --- cara --- */
    pintar_cara(&l, e, ex0, ex1, ey, mx, my, ojos_grandes, forma, rasgo == 1 || rasgo == 3, mejilla, boca_abierta, etapa);
    /* --- ropa y accesorios --- */
    pintar_ropa(&l, ps->ropa, cx, rx, cy, ry, top, ex0, ex1, ey);

    contornear(&l, oscurecer(P.oscuro, 0.55f));
    /* radiante: destellos fuera del contorno */
    if ((forma == 1 && etapa == 3) || e == EXP_ORGULLO) {
        lz_set(&l, 2, 5 + ioy, C_DESTELLO); lz_set(&l, 17, 3 + ioy, C_DESTELLO); lz_set(&l, 18, 9 - ioy, C_DESTELLO);
    }
    if (ps->capa) poner_capa(&l);
    ensuciar(&l, ps->capa ? 0 : ps->sucio);
    ampliar(dst, &l, AMP, espejo);
}

/* huevo con manchas del color de la especie; grietas 0..3 */
/* --- tamaño de las criaturas: se agranda UNA vez al dibujar la pose (interpolación bilineal con el alfa pesado, sin halos);
 * después la imagen queda en caché como cualquier otra. El zoom en vivo de LVGL sería lento. --- */
static int s_tam = 100;
void art_tamano_set(int pct) { s_tam = (pct == 120 || pct == 130 || pct == 140) ? pct : 100; }
int art_tamano_get(void) { return s_tam; }

static void img_agrandar(art_img_t *dst, int pct)
{
    const int w = (int)dst->dsc.header.w, h = (int)dst->dsc.header.h;
    if (!dst->px || w < 2 || h < 2) return;
    const int nw = (w * pct + 50) / 100, nh = (h * pct + 50) / 100;
    uint32_t *src = buf_trabajo(w * h);
    if (!src) return;
    memcpy(src, dst->px, (size_t)w * (size_t)h * 4);
    img_preparar(dst, nw, nh);
    if (!dst->px) return;
    const float k = 100.0f / (float)pct;
    for (int y = 0; y < nh; y++) {
        float fy = ((float)y + 0.5f) * k - 0.5f;
        int y0 = (int)floorf(fy);
        float ty = fy - (float)y0;
        int y1 = y0 + 1;
        if (y0 < 0) y0 = 0;
        if (y1 > h - 1) y1 = h - 1;
        if (y0 > h - 1) y0 = h - 1;
        for (int x = 0; x < nw; x++) {
            float fx = ((float)x + 0.5f) * k - 0.5f;
            int x0 = (int)floorf(fx);
            float tx = fx - (float)x0;
            int x1 = x0 + 1;
            if (x0 < 0) x0 = 0;
            if (x1 > w - 1) x1 = w - 1;
            if (x0 > w - 1) x0 = w - 1;
            const uint32_t c4[4] = {src[y0 * w + x0], src[y0 * w + x1], src[y1 * w + x0], src[y1 * w + x1]};
            const float wt[4] = {(1 - tx) * (1 - ty), tx * (1 - ty), (1 - tx) * ty, tx * ty};
            float A = 0, R = 0, G = 0, B = 0;
            for (int i = 0; i < 4; i++) {
                float a = (float)(c4[i] >> 24) * wt[i];
                A += a;
                R += a * (float)((c4[i] >> 16) & 255);
                G += a * (float)((c4[i] >> 8) & 255);
                B += a * (float)(c4[i] & 255);
            }
            if (A < 1.0f) continue; /* transparente */
            dst->px[y * nw + x] = ((uint32_t)(A > 255.0f ? 255 : (int)(A + 0.5f)) << 24) | ((uint32_t)(R / A + 0.5f) << 16) | ((uint32_t)(G / A + 0.5f) << 8) | (uint32_t)(B / A + 0.5f);
        }
    }
}

void art_criatura_pose(art_img_t *dst, const art_pose_t *ps)
{
    art_criatura_pose_base(dst, ps);
    if (s_tam > 100 && !s_solo_cuerpo) img_agrandar(dst, s_tam);
}

static void huevo_nuevo(art_img_t *dst, int especie, int escala, int grietas, int inclinacion);
static void huevo_pintado(art_img_t *dst, int especie, int escala, int grietas, int inclinacion);
void art_huevo(art_img_t *dst, int especie, int escala, int grietas, int inclinacion)
{
    if (s_estilo) { huevo_pintado(dst, especie & 7, escala, grietas, inclinacion); return; }
    const float kd = k_de_escala(escala);
    const int LR = (int)lroundf(ART_CRIATURA_LADO * kd), AMP = (int)lroundf((float)escala / kd);
    uint32_t *buf = buf_trabajo(LR * LR);
    if (!buf) return;
    lienzo_t l = {LR, LR, buf, kd};
    const int mez = (especie >> 3) & 15, tono = (especie >> 7) & 7;
    paleta_t PE = paleta_forma(especie & 7, 0, false, tono), PM = mez ? paleta_forma((mez - 1) % 8, 0, false, 0) : PE;
    const paleta_t *P = &PE;
    float cx = 10.0f + inclinacion * 0.6f, cy = 11.6f;
    uint32_t base = A(0xFFF6E2), luz = A(0xFFFFFF), sombra_c = A(0xE6D6B8);
    for (int Y = 0; Y < l.h; Y++) /* en pixeles reales: la cáscara sale bien redonda */
        for (int X = 0; X < l.w; X++) {
            float x = (X + 0.5f) / kd, y = (Y + 0.5f) / kd;
            if (y < 3.0f || y >= 19.0f || x < 2.0f || x >= 18.0f) continue;
            float dy = (y - cy) / 7.2f, k = dy < 0 ? 0.78f : 1.0f; /* más angosto arriba */
            float dx = (x - cx - inclinacion * dy * -0.8f) / (5.8f * k);
            float d = dx * dx + dy * dy;
            if (d > 1.0f) continue;
            uint32_t c = base;
            if (dx * 0.6f + dy * 0.8f > 0.55f) c = sombra_c;
            else if (dx * 0.6f + dy * 0.8f > 0.42f) c = mezclar(base, sombra_c, 0.5f);
            else if (-dx * 0.7f - dy * 0.7f > 0.5f && d > 0.35f) c = luz;
            lz_real(&l, X, Y, c);
        }
    /* manchas */
    static const int8_t MANCHAS[][3] = {{7, 9, 2}, {12, 7, 1}, {13, 13, 2}, {8, 15, 1}, {10, 11, 1}};
    for (int i = 0; i < 5; i++) {
        float mxp = MANCHAS[i][0] + inclinacion * 0.5f + 0.5f, myp = MANCHAS[i][1] + 0.5f;
        uint32_t mc = mez && i >= 3 ? A(PM.acento) : (i & 1 ? A(P->acento) : A(P->oscuro)); /* mezclado: manchas de las dos */
        for (int Y = (int)((myp - 3) * kd); Y <= (int)((myp + 3) * kd); Y++)
            for (int X = (int)((mxp - 3) * kd); X <= (int)((mxp + 3) * kd); X++)
                if (lz_rget(&l, X, Y) && hypotf((X + 0.5f) / kd - mxp, (Y + 0.5f) / kd - myp) <= MANCHAS[i][2] * 0.9f + 0.3f)
                    lz_real(&l, X, Y, mc);
    }
    /* grietas */
    uint32_t g = A(0x6A5A48);
    if (grietas >= 1) { lz_set(&l, 8, 7, g); lz_set(&l, 9, 8, g); lz_set(&l, 8, 9, g); }
    if (grietas >= 2) { lz_set(&l, 10, 8, g); lz_set(&l, 11, 7, g); lz_set(&l, 12, 8, g); lz_set(&l, 13, 9, g); }
    if (grietas >= 3) { lz_set(&l, 7, 10, g); lz_set(&l, 6, 11, g); lz_set(&l, 14, 10, g); lz_set(&l, 13, 11, g); }
    contornear(&l, A(0x4A3F5A));
    ampliar(dst, &l, AMP, false);
}

/* recuerdo: piedrita con una estrella y flores */
void art_recuerdo(art_img_t *dst, int especie, int escala)
{
    const float kd = k_de_escala(escala);
    const int LR = (int)lroundf(ART_CRIATURA_LADO * kd), AMP = (int)lroundf((float)escala / kd);
    uint32_t *buf = buf_trabajo(LR * LR);
    if (!buf) return;
    lienzo_t l = {LR, LR, buf, kd};
    elipse(&l, 10, 11, 5.5f, 7.0f, A(0xC9C3DA));
    rect(&l, 4, 11, 12, 7, A(0xC9C3DA));
    elipse(&l, 9, 8, 3.5f, 4.0f, A(0xDCD8EA));
    rect(&l, 4, 16, 12, 2, A(0x9D96B4));
    estrellita(&l, 10, 9, A(PALETA[(unsigned)especie % 8].cuerpo));
    lz_set(&l, 10, 9, A(0xFFF6A8));
    rect(&l, 2, 18, 16, 1, A(0x5FA84F));
    lz_set(&l, 3, 17, C_FLOR); lz_set(&l, 16, 17, C_FLOR_C); lz_set(&l, 15, 17, C_FLOR);
    contornear(&l, A(0x4A3F5A));
    ampliar(dst, &l, AMP, false);
}

/* ======================================================================
 * Íconos (12x12, dibujados con formas y contorno automático)
 * ==================================================================== */

#define ICO_LADO 12
#define K A(0x2A2238)

/* ---- los objetos mágicos: una forma por familia, con los dos colores de cada uno y una variación chica ---- */
_Static_assert(ART_OBJETOS == PET_OBJETOS, "un ícono por objeto");
static void obj_dibujar(lienzo_t *l, int id)
{
    const objeto_info_t *o = objeto_info(id);
    if (!o) return;
    const uint32_t c1 = A(o->c1), c2 = A(o->c2), os = oscurecer(c1, 0.35f), cl = aclarar(c1, 0.45f), W = A(0xFFFFFF);
    const int v = (int)(hash(id, 7, 99) % 3); /* variación */
    switch (o->familia) {
    case OFA_PIEDRA:
        elipse(l, 6.0f, 7.2f, 4.6f - 0.4f * v, 3.6f, c1); elipse(l, 7.0f, 8.2f, 3.6f, 2.4f, os);
        elipse(l, 5.6f, 6.6f, 3.4f, 2.6f, c1); lz_set(l, 4, 5, cl);
        lz_set(l, 6 + v, 7, c2); lz_set(l, 7 + v, 8, c2);
        break;
    case OFA_GEMA:
        triangulo(l, 2.0f, 5.0f, 10.0f, 5.0f, 6.0f, 11.0f, c1); rect(l, 3, 3, 6, 2, cl); triangulo(l, 2.0f, 5.0f, 3.0f, 3.0f, 3.0f, 5.0f, cl);
        triangulo(l, 10.0f, 5.0f, 9.0f, 3.0f, 9.0f, 5.0f, cl); triangulo(l, 6.0f, 5.0f, 10.0f, 5.0f, 6.0f, 11.0f, os);
        lz_set(l, 4, 4, W); lz_set(l, 9, 1, c2); lz_set(l, 10, 2, c2);
        break;
    case OFA_CRISTAL:
        triangulo(l, 3.0f, 11.0f, 5.0f, 2.0f, 7.0f, 11.0f, c1); triangulo(l, 6.0f, 11.0f, 8.4f, 4.0f, 10.0f, 11.0f, cl);
        triangulo(l, 1.5f, 11.0f, 2.6f, 6.5f, 4.0f, 11.0f, os); lz_set(l, 4, 5, c2); lz_set(l, 8, 7, W);
        break;
    case OFA_POLVO:
        elipse(l, 6.0f, 9.0f, 4.6f, 2.0f, c1); elipse(l, 6.0f, 8.4f, 3.0f, 1.4f, cl);
        estrellita(l, 3, 3 + v, c2); estrellita(l, 9, 2, c2); lz_set(l, 6, 5, W); lz_set(l, 1, 6, c2); lz_set(l, 10, 6, W);
        break;
    case OFA_FLOR:
        rect(l, 6, 7, 1, 5, A(0x4FA86A)); elipse(l, 8.0f, 9.6f, 1.6f, 0.8f, A(0x5DBB63));
        for (int k = 0; k < 5; k++) {
            float a = (float)k * 1.2566f + 0.3f * v;
            circulo(l, 6.5f + 2.4f * cosf(a), 4.6f + 2.4f * sinf(a), 1.7f, c1);
        }
        circulo(l, 6.5f, 4.6f, 1.3f, c2);
        break;
    case OFA_HONGO:
        rect(l, 5, 6, 3, 5, A(0xF6EEDC)); elipse(l, 6.5f, 5.2f, 5.0f, 3.4f, c1); rect(l, 0, 6, 12, 2, TRANSP);
        rect(l, 5, 6, 3, 5, A(0xF6EEDC)); elipse(l, 6.5f, 6.0f, 5.0f, 0.8f, os);
        lz_set(l, 4, 3, c2); lz_set(l, 8, 3, c2); lz_set(l, 6, 2, c2); if (v) lz_set(l, 3, 5, c2);
        break;
    case OFA_SEMILLA:
        elipse(l, 6.0f, 7.0f, 3.2f, 4.2f, c1); elipse(l, 7.0f, 8.0f, 2.0f, 3.0f, os); elipse(l, 5.4f, 6.4f, 1.8f, 2.6f, c1);
        lz_set(l, 5, 5, cl); rect(l, 6, 1, 1, 2, A(0x5DBB63)); lz_set(l, 7, 1, c2); lz_set(l, 8, 1, c2);
        break;
    case OFA_BAYA:
        circulo(l, 4.4f, 7.6f, 2.6f, c1); circulo(l, 8.0f, 7.0f, 2.6f, c1); circulo(l, 6.2f, 4.6f, 2.4f, c1);
        lz_set(l, 3, 7, c2); lz_set(l, 7, 6, c2); lz_set(l, 5, 4, c2);
        trazo(l, 6.2f, 2.4f, 8.6f, 0.8f, 0.3f, A(0x4FA86A)); elipse(l, 9.4f, 1.6f, 1.4f, 0.8f, A(0x5DBB63));
        break;
    case OFA_HOJA:
        triangulo(l, 6.0f, 0.8f, 1.2f, 7.0f, 6.0f, 11.0f, c1); triangulo(l, 6.0f, 0.8f, 10.8f, 7.0f, 6.0f, 11.0f, c1);
        circulo(l, 3.0f, 5.0f, 1.6f, c1); circulo(l, 9.0f, 5.0f, 1.6f, c1);
        trazo(l, 6.0f, 2.0f, 6.0f, 11.5f, 0.25f, c2); trazo(l, 6.0f, 6.0f, 3.0f, 4.4f, 0.2f, c2); trazo(l, 6.0f, 6.0f, 9.0f, 4.4f, 0.2f, c2);
        break;
    case OFA_PLANTA:
        trazo(l, 6.0f, 11.0f, 6.0f, 3.0f, 0.35f, os);
        elipse(l, 3.6f, 6.0f - v * 0.5f, 2.4f, 1.2f, c1); elipse(l, 8.4f, 4.6f, 2.4f, 1.2f, c1); elipse(l, 4.4f, 2.6f, 1.8f, 1.0f, c1);
        elipse(l, 8.4f, 8.4f, 2.0f, 1.0f, c1); lz_set(l, 6, 2, c2); lz_set(l, 2, 6, c2); lz_set(l, 9, 4, c2);
        break;
    case OFA_PLUMA:
        trazo(l, 2.0f, 11.0f, 9.4f, 1.6f, 0.3f, os);
        elipse(l, 6.6f, 5.0f, 2.2f, 4.2f, c1); elipse(l, 7.2f, 4.0f, 1.0f, 2.4f, cl);
        trazo(l, 3.4f, 9.0f, 9.4f, 1.6f, 0.25f, c2); lz_set(l, 5, 7, c2);
        break;
    case OFA_ESCAMA: /* escamas superpuestas */
        for (int f = 0; f < 3; f++)
            for (int k = 0; k < 3 - (f & 1); k++) {
                float x = 3.0f + 3.0f * k + 1.5f * (f & 1), y = 3.4f + 2.8f * f;
                circulo(l, x, y, 1.9f, f == 1 ? cl : c1);
                elipse(l, x, y + 1.1f, 1.5f, 0.7f, os);
            }
        lz_set(l, 4, 3, W); lz_set(l, 7, 6, c2); lz_set(l, 5, 9, c2);
        break;
    case OFA_HUEVO:
        elipse(l, 6.0f, 6.6f, 3.8f, 4.8f, c1); elipse(l, 7.2f, 8.0f, 2.4f, 3.0f, os); elipse(l, 5.6f, 6.0f, 3.0f, 4.0f, c1);
        lz_set(l, 4, 4, W); lz_set(l, 5, 8, c2); lz_set(l, 8, 6, c2); lz_set(l, 7, 3, c2);
        break;
    case OFA_HUESO:
        if (v == 2) { triangulo(l, 3.0f, 2.0f, 9.0f, 2.0f, 6.0f, 11.0f, c1); trazo(l, 6.0f, 3.0f, 6.0f, 9.0f, 0.3f, cl); lz_set(l, 5, 3, c2); }
        else {
            trazo(l, 3.0f, 9.0f, 9.0f, 3.0f, 0.9f, c1); circulo(l, 2.2f, 8.6f, 1.3f, c1); circulo(l, 3.4f, 9.8f, 1.3f, c1);
            circulo(l, 8.6f, 2.2f, 1.3f, c1); circulo(l, 9.8f, 3.4f, 1.3f, c1); lz_set(l, 6, 6, c2); lz_set(l, 7, 5, c2);
        }
        break;
    case OFA_CONCHA:
        elipse(l, 6.0f, 6.6f, 5.0f, 4.0f, c1); rect(l, 0, 7, 12, 5, TRANSP); rect(l, 2, 7, 8, 2, c1); rect(l, 4, 9, 4, 2, os);
        for (int k = -2; k <= 2; k++) trazo(l, 6.0f, 9.0f, 6.0f + 2.2f * k, 3.4f, 0.2f, os);
        lz_set(l, 6, 7, c2);
        break;
    case OFA_FRASCO:
        rect(l, 5, 1, 3, 2, A(0xA8744A)); rect(l, 4, 3, 5, 1, cl);
        circulo(l, 6.5f, 7.4f, 3.8f, A(0xBFD8E8)); circulo(l, 6.5f, 7.8f, 3.0f, c1); circulo(l, 6.5f, 7.8f, 1.6f, c2);
        lz_set(l, 4, 6, W);
        break;
    case OFA_POCION:
        rect(l, 5, 0, 3, 2, A(0xA8744A)); rect(l, 5, 2, 3, 3, A(0xD8E8F0));
        triangulo(l, 6.5f, 3.0f, 1.4f, 11.2f, 11.6f, 11.2f, A(0xD8E8F0)); triangulo(l, 6.5f, 5.0f, 2.6f, 11.0f, 10.4f, 11.0f, c1);
        rect(l, 3, 9, 7, 2, os); lz_set(l, 5, 8, c2); lz_set(l, 7, 7, c2); lz_set(l, 4, 6, W);
        break;
    case OFA_DISCO:
        circulo(l, 6.0f, 6.4f, 4.4f, c1); circulo(l, 6.0f, 6.4f, 3.2f, c2); circulo(l, 6.0f, 6.4f, 2.0f, cl);
        rect(l, 5, 1, 2, 1, c1); lz_set(l, 6, 6, os); lz_set(l, 6, 5 - (v == 1), os); lz_set(l, 7, 6, os);
        break;
    case OFA_LLAVE:
        if (v == 2) { /* herradura, imán, candado... */
            elipse(l, 6.0f, 5.2f, 4.2f, 4.0f, c1); elipse(l, 6.0f, 5.2f, 2.0f, 2.0f, TRANSP); rect(l, 4, 5, 4, 7, TRANSP);
            rect(l, 2, 5, 2, 6, c1); rect(l, 8, 5, 2, 6, c1); rect(l, 2, 10, 2, 1, c2); rect(l, 8, 10, 2, 1, c2);
        } else {
            circulo(l, 3.6f, 3.6f, 2.6f, c1); circulo(l, 3.6f, 3.6f, 1.0f, TRANSP); trazo(l, 5.0f, 5.0f, 10.0f, 10.0f, 0.55f, c1);
            trazo(l, 8.0f, 8.0f, 9.6f, 6.4f, 0.4f, c1); trazo(l, 9.4f, 9.4f, 11.0f, 7.8f, 0.4f, c1); lz_set(l, 2, 2, c2);
        }
        break;
    case OFA_LIBRO:
        rect(l, 2, 2, 8, 9, c1); rect(l, 4, 2, 6, 8, A(0xF6F2EA)); rect(l, 2, 2, 2, 9, os);
        rect(l, 5, 4, 4, 1, A(0x9A8A9A)); rect(l, 5, 6, 3, 1, A(0x9A8A9A)); rect(l, 5, 8, 4, 1, A(0x9A8A9A)); lz_set(l, 2, 5, c2);
        break;
    case OFA_PALO:
        trazo(l, 2.0f, 11.0f, 9.0f, 3.0f, 0.55f, c1); trazo(l, 2.6f, 10.6f, 8.6f, 3.6f, 0.2f, cl);
        circulo(l, 9.6f, 2.4f, 1.6f, c2); lz_set(l, 9, 1, W); if (v == 1) estrellita(l, 3, 3, c2);
        break;
    case OFA_RUNA:
        elipse(l, 6.0f, 6.4f, 4.8f, 4.6f, c1); elipse(l, 7.0f, 7.4f, 3.6f, 3.4f, os); elipse(l, 5.6f, 6.0f, 4.0f, 3.8f, c1);
        trazo(l, 5.0f, 3.2f, 5.0f, 9.0f, 0.4f, c2);
        if (v == 0) trazo(l, 5.0f, 4.8f, 8.0f, 3.4f, 0.4f, c2); else trazo(l, 5.0f, 6.0f, 8.0f, 8.6f, 0.4f, c2);
        if (v == 2) trazo(l, 5.0f, 6.0f, 2.6f, 4.0f, 0.4f, c2);
        lz_set(l, 3, 3, cl);
        break;
    case OFA_AMULETO:
        trazo(l, 2.0f, 1.0f, 6.0f, 5.0f, 0.2f, A(0xC8B89A)); trazo(l, 10.0f, 1.0f, 6.0f, 5.0f, 0.2f, A(0xC8B89A));
        circulo(l, 6.0f, 7.6f, 3.4f, c1); circulo(l, 6.0f, 7.6f, 1.8f, c2); lz_set(l, 5, 6, W);
        break;
    case OFA_CAJA:
        rect(l, 1, 5, 10, 6, c1); rect(l, 1, 3, 10, 2, cl); rect(l, 1, 8, 10, 1, os); rect(l, 5, 5, 2, 2, c2);
        if (v) { lz_set(l, 4, 1, c2); lz_set(l, 7, 0, c2); }
        break;
    case OFA_BOTA:
        rect(l, 3, 1, 4, 7, c1); rect(l, 3, 7, 8, 3, c1); elipse(l, 9.4f, 8.4f, 1.8f, 1.6f, c1);
        rect(l, 2, 10, 10, 1, os); rect(l, 3, 1, 4, 1, c2); lz_set(l, 5, 4, os); lz_set(l, 5, 6, os);
        break;
    case OFA_TAZA:
        rect(l, 2, 4, 7, 6, c1); elipse(l, 5.5f, 10.0f, 3.5f, 1.0f, c1); rect(l, 2, 4, 7, 1, cl);
        circulo(l, 10.0f, 6.6f, 1.8f, c1); circulo(l, 10.0f, 6.6f, 0.8f, TRANSP); lz_set(l, 4, 7, c2); lz_set(l, 6, 6, c2);
        if (v == 1) { rect(l, 7, 7, 2, 1, TRANSP); }
        break;
    case OFA_LAMPARA:
        rect(l, 5, 0, 2, 1, os); rect(l, 3, 1, 6, 1, c1); rect(l, 3, 2, 6, 6, A(0x3A3448)); rect(l, 4, 3, 4, 4, c2);
        lz_set(l, 5, 4, W); rect(l, 2, 8, 8, 2, c1); rect(l, 4, 10, 4, 1, os);
        break;
    case OFA_ORBE:
        circulo(l, 6.0f, 6.0f, 4.6f, c1); circulo(l, 6.8f, 6.8f, 3.4f, os); circulo(l, 5.6f, 5.6f, 3.4f, c1);
        circulo(l, 6.0f, 6.0f, 1.6f + 0.3f * v, c2); lz_set(l, 4, 3, W); lz_set(l, 3, 4, W);
        break;
    case OFA_HILO: /* un ovillo de hebras */
        circulo(l, 5.6f, 6.4f, 3.8f, c1);
        trazo(l, 2.6f, 5.0f, 8.4f, 9.0f, 0.22f, cl); trazo(l, 3.0f, 8.4f, 7.6f, 3.0f, 0.22f, cl); trazo(l, 5.0f, 2.8f, 9.2f, 6.4f, 0.22f, cl);
        trazo(l, 9.0f, 8.6f, 11.0f, 11.0f, 0.25f, c1); lz_set(l, 4, 4, c2); lz_set(l, 6, 8, c2);
        break;
    default: circulo(l, 6.0f, 6.0f, 4.0f, c1); break;
    }
}

static void ico_dibujar(lienzo_t *l, art_icono_t i)
{
    if (i >= ICO_OBJ0) { obj_dibujar(l, i - ICO_OBJ0); return; }
    switch (i) {
    case ICO_CORAZON:
        circulo(l, 4.0f, 4.5f, 2.6f, A(0xF0506E)); circulo(l, 8.0f, 4.5f, 2.6f, A(0xF0506E));
        triangulo(l, 1.4f, 5.2f, 10.6f, 5.2f, 6.0f, 10.4f, A(0xF0506E));
        lz_set(l, 3, 3, A(0xFFB3C1)); lz_set(l, 3, 4, A(0xFFB3C1));
        break;
    case ICO_COMIDA: /* bolita de arroz con alga */
        triangulo(l, 6.0f, 1.0f, 1.0f, 10.0f, 11.0f, 10.0f, A(0xFFFFFF));
        circulo(l, 6.0f, 7.0f, 3.6f, A(0xFFFFFF));
        rect(l, 3, 7, 6, 4, A(0x2F4A3A));
        lz_set(l, 5, 3, A(0xE8E4F0));
        break;
    case ICO_FRUTA: /* manzana */
        circulo(l, 6.0f, 7.0f, 4.2f, A(0xF0506E));
        rect(l, 6, 1, 1, 3, A(0x8B5E3C));
        elipse(l, 8.3f, 2.4f, 1.6f, 0.9f, A(0x5DBB63));
        lz_set(l, 4, 5, A(0xFFB3C1)); lz_set(l, 4, 6, A(0xFFB3C1));
        break;
    case ICO_GOTA: /* mancha de barro: está sucia */
        circulo(l, 6.0f, 7.0f, 3.6f, A(0x8B5E3C)); circulo(l, 2.6f, 9.4f, 1.3f, A(0x8B5E3C)); circulo(l, 9.6f, 4.0f, 1.2f, A(0x8B5E3C));
        circulo(l, 9.8f, 9.6f, 0.9f, A(0x8B5E3C)); lz_set(l, 5, 6, A(0xA87650)); lz_set(l, 2, 3, K); lz_set(l, 3, 2, K);
        break;
    case ICO_SUDOR:
        circulo(l, 6.0f, 7.4f, 3.4f, A(0x9AD8FF));
        triangulo(l, 6.0f, 0.8f, 3.0f, 6.4f, 9.0f, 6.4f, A(0x9AD8FF));
        lz_set(l, 4, 7, A(0xFFFFFF)); lz_set(l, 4, 8, A(0xFFFFFF));
        break;
    case ICO_PELOTA: /* pelota de playa a gajos */
        circulo(l, 6.0f, 6.0f, 4.8f, A(0xFFFFFF));
        for (int Y = 0; Y < l->h; Y++)
            for (int X = 0; X < l->w; X++) {
                if (!lz_rget(l, X, Y)) continue;
                float x = (X + 0.5f) / KL(l) - 0.5f, y = (Y + 0.5f) / KL(l) - 0.5f;
                int g = (int)floorf((x * 3 + y) / 8);
                static const uint32_t G[] = {0xF0506E, 0xFFFFFF, 0xFFD45C, 0xFFFFFF, 0x4DA6FF, 0xFFFFFF};
                lz_real(l, X, Y, A(G[(g + 6) % 6]));
            }
        lz_set(l, 4, 3, A(0xFFFFFF)); lz_set(l, 3, 4, A(0xFFFFFF));
        break;
    case ICO_ESTRELLA:
        triangulo(l, 6.0f, 0.6f, 3.6f, 8.0f, 8.4f, 8.0f, A(0xFFD45C));
        triangulo(l, 0.6f, 4.4f, 11.4f, 4.4f, 6.0f, 8.0f, A(0xFFD45C));
        triangulo(l, 6.0f, 6.8f, 2.0f, 11.2f, 4.2f, 5.0f, A(0xFFD45C));
        triangulo(l, 6.0f, 6.8f, 10.0f, 11.2f, 7.8f, 5.0f, A(0xFFD45C));
        break;
    case ICO_ZETA:
        rect(l, 2, 2, 8, 2, A(0xFFFFFF)); rect(l, 2, 8, 8, 2, A(0xFFFFFF));
        trazo(l, 8.5f, 4.0f, 3.5f, 8.0f, 0.9f, A(0xFFFFFF));
        break;
    case ICO_NOTA:
        circulo(l, 4.0f, 9.0f, 2.0f, A(0xB48CFF)); rect(l, 5, 2, 1, 7, A(0xB48CFF));
        rect(l, 6, 2, 3, 1, A(0xB48CFF)); rect(l, 8, 3, 1, 2, A(0xB48CFF));
        break;
    case ICO_HIELO: /* (fw 104) trocito de hielo: un cubito celeste con brillo */
        rect(l, 2, 4, 8, 6, A(0x96D6F5)); rect(l, 6, 4, 4, 6, A(0x68B4E6)); rect(l, 3, 2, 7, 2, A(0xE8FAFF));
        lz_set(l, 3, 3, A(0xFFFFFF)); lz_set(l, 4, 3, A(0xFFFFFF)); lz_set(l, 9, 10, A(0x96D6F5));
        break;
    case ICO_PAPELITO: /* (fw 104) papelito doblado con letra */
        rect(l, 2, 2, 8, 8, A(0xFAEECD)); rect(l, 5, 2, 3, 8, A(0xECDCB6));
        rect(l, 3, 4, 2, 1, A(0x6E4624)); rect(l, 3, 6, 2, 1, A(0x6E4624)); rect(l, 6, 4, 1, 1, A(0x6E4624)); rect(l, 6, 6, 1, 1, A(0x6E4624));
        rect(l, 3, 8, 2, 1, A(0x60B054));
        break;
    case ICO_ENOJO: /* marca de enojo */
        trazo(l, 2.5f, 2.5f, 4.5f, 4.5f, 0.8f, A(0xF0506E)); trazo(l, 9.5f, 2.5f, 7.5f, 4.5f, 0.8f, A(0xF0506E));
        trazo(l, 2.5f, 9.5f, 4.5f, 7.5f, 0.8f, A(0xF0506E)); trazo(l, 9.5f, 9.5f, 7.5f, 7.5f, 0.8f, A(0xF0506E));
        break;
    case ICO_LUNA:
        circulo(l, 6.0f, 6.0f, 4.6f, A(0xFFE27A));
        circulo(l, 8.2f, 4.4f, 3.8f, TRANSP);
        break;
    case ICO_SOL:
        circulo(l, 6.0f, 6.0f, 3.0f, A(0xFFC93C));
        for (int k = 0; k < 8; k++) {
            float a = k * 0.785f;
            lz_set(l, (int)(6 + cosf(a) * 4.9f), (int)(6 + sinf(a) * 4.9f), A(0xFFC93C));
        }
        break;
    case ICO_CACA:
        elipse(l, 6.0f, 9.0f, 4.8f, 2.0f, A(0x8B5E3C));
        elipse(l, 6.0f, 6.4f, 3.4f, 1.8f, A(0x9C6A44));
        elipse(l, 6.2f, 4.0f, 2.0f, 1.5f, A(0xA87650));
        lz_set(l, 4, 8, A(0xC79A72));
        break;
    case ICO_BURBUJA:
        circulo(l, 6.0f, 6.0f, 4.4f, A(0xCFF0FF));
        circulo(l, 6.0f, 6.0f, 3.2f, A(0xA8DBFF));
        lz_set(l, 4, 4, A(0xFFFFFF)); lz_set(l, 4, 5, A(0xFFFFFF));
        break;
    case ICO_FLOR:
        circulo(l, 6.0f, 3.0f, 1.8f, A(0xFF8FC8)); circulo(l, 3.4f, 5.6f, 1.8f, A(0xFF8FC8));
        circulo(l, 8.6f, 5.6f, 1.8f, A(0xFF8FC8)); circulo(l, 6.0f, 7.4f, 1.8f, A(0xFF8FC8));
        circulo(l, 6.0f, 5.4f, 1.2f, A(0xFFE066)); rect(l, 6, 9, 1, 3, A(0x5DBB63));
        break;
    case ICO_HONGO:
        elipse(l, 6.0f, 5.0f, 5.0f, 3.6f, A(0xF0506E)); rect(l, 0, 6, 12, 3, TRANSP);
        elipse(l, 6.0f, 5.6f, 5.0f, 1.0f, A(0xF0506E));
        rect(l, 4, 6, 4, 5, A(0xFFF1D6));
        lz_set(l, 4, 3, A(0xFFFFFF)); lz_set(l, 8, 3, A(0xFFFFFF)); lz_set(l, 6, 2, A(0xFFFFFF));
        break;
    case ICO_FAROL:
        rect(l, 5, 4, 2, 8, A(0x4A4A5E)); rect(l, 3, 1, 6, 4, A(0xFFE27A)); rect(l, 3, 0, 6, 1, A(0x4A4A5E));
        break;
    case ICO_BANCO:
        rect(l, 1, 4, 10, 2, A(0xC08A5A)); rect(l, 1, 7, 10, 2, A(0xC08A5A));
        rect(l, 2, 9, 1, 3, A(0x6A442B)); rect(l, 9, 9, 1, 3, A(0x6A442B));
        break;
    case ICO_CASITA: /* (v38) una casita de ramitas y musgo: paredes de ramitas atadas, techo de musgo con hongos, puerta redonda, ventanita con luz y chimenea */
        rect(l, 8, 0, 2, 3, A(0x8A84A0)); lz_set(l, 8, 0, A(0xB0AAC4));                  /* la chimenea de piedra */
        rect(l, 2, 4, 8, 7, A(0xB07A44));                                               /* las paredes */
        for (int y = 5; y < 11; y += 2) { rect(l, 1, y, 10, 1, A(0x7A5028)); lz_set(l, 1, y, A(0x5A3A1E)); lz_set(l, 10, y, A(0x5A3A1E)); } /* las ramitas, una sobre otra (asoman las puntas) */
        rect(l, 2, 4, 1, 7, A(0xC89A5E));                                               /* luz a la izquierda */
        lz_set(l, 2, 6, A(0x5DBB63)); lz_set(l, 9, 8, A(0x5DBB63));                     /* musgo entre las ramitas */
        rect(l, 1, 10, 10, 1, A(0x5A8A3A)); lz_set(l, 0, 10, A(0x4FA85A)); lz_set(l, 11, 10, A(0x4FA85A)); /* musgo al pie */
        elipse(l, 6.0f, 4.0f, 5.8f, 2.4f, A(0x4FA85A));                                 /* el techo de musgo */
        lz_set(l, 3, 3, A(0x8FE08A)); lz_set(l, 6, 2, A(0x8FE08A)); lz_set(l, 8, 4, A(0x8FE08A));
        circulo(l, 3.0f, 1.8f, 1.1f, A(0xE5484D)); lz_set(l, 3, 1, A(0xFFFFFF)); lz_set(l, 3, 3, A(0xFFF1D6)); /* un hongo */
        lz_set(l, 10, 3, A(0xF2A33A));                                                  /* y otro chiquito */
        circulo(l, 7.0f, 8.8f, 1.7f, A(0x5A3A1E)); rect(l, 5, 9, 4, 2, A(0x5A3A1E));   /* la puerta redonda */
        lz_set(l, 8, 9, A(0xFFD27A));                                                   /* su manilla */
        rect(l, 3, 6, 2, 2, A(0xFFD27A)); lz_set(l, 3, 6, A(0xFFF0B0));                 /* la ventanita con luz cálida */
        break;
    case ICO_NUBE:
    case ICO_LLUVIA:
        circulo(l, 4.0f, 6.0f, 2.6f, A(0xEEF3FF)); circulo(l, 7.4f, 5.0f, 3.2f, A(0xEEF3FF));
        rect(l, 2, 6, 8, 3, A(0xEEF3FF)); rect(l, 2, 8, 9, 1, A(0xC6D0EA));
        if (i == ICO_LLUVIA) { lz_set(l, 3, 10, A(0x4DA6FF)); lz_set(l, 6, 11, A(0x4DA6FF)); lz_set(l, 9, 10, A(0x4DA6FF)); }
        break;
    case ICO_COPO:
        trazo(l, 6.0f, 0.8f, 6.0f, 11.2f, 0.6f, A(0x9ED8FF)); trazo(l, 1.5f, 3.4f, 10.5f, 8.6f, 0.6f, A(0x9ED8FF));
        trazo(l, 1.5f, 8.6f, 10.5f, 3.4f, 0.6f, A(0x9ED8FF)); circulo(l, 6.0f, 6.0f, 1.2f, A(0xFFFFFF));
        break;
    case ICO_VIENTO: /* ráfagas en espiral */
        trazo(l, 0.5f, 3.5f, 7.0f, 3.5f, 0.6f, A(0x9ED8FF)); circulo(l, 8.0f, 2.6f, 1.4f, A(0x9ED8FF)); circulo(l, 8.0f, 2.6f, 0.5f, TRANSP);
        trazo(l, 0.5f, 6.5f, 9.5f, 6.5f, 0.6f, A(0x9ED8FF));
        trazo(l, 2.5f, 9.5f, 7.0f, 9.5f, 0.6f, A(0x9ED8FF)); circulo(l, 8.0f, 10.0f, 1.2f, A(0x9ED8FF));
        break;
    case ICO_AMIGOS: /* dos bichitos con su brote, juntos */
        elipse(l, 3.6f, 7.2f, 3.4f, 3.2f, A(0xFFD86B)); elipse(l, 8.6f, 7.8f, 3.2f, 3.0f, A(0x9FD3F7));
        rect(l, 3, 3, 1, 1, A(0x3E8A4F)); rect(l, 4, 2, 2, 1, A(0x86D16A)); rect(l, 8, 4, 1, 1, A(0x3E8A4F)); rect(l, 9, 3, 2, 1, A(0x86D16A));
        lz_set(l, 2, 7, K); lz_set(l, 4, 7, K); lz_set(l, 7, 8, K); lz_set(l, 9, 8, K);
        lz_set(l, 3, 9, A(0xFF9DB0)); lz_set(l, 9, 10, A(0xFF9DB0));
        lz_set(l, 6, 1, A(0xF0506E)); lz_set(l, 7, 1, A(0xF0506E)); lz_set(l, 6, 2, A(0xF0506E)); lz_set(l, 7, 2, A(0xF0506E)); /* un corazoncito entre ambos */
        break;
    case ICO_HUEVO:
        elipse(l, 6.0f, 6.6f, 3.8f, 4.8f, A(0xFFF6E2)); circulo(l, 4.6f, 5.6f, 1.0f, A(0x86D16A)); circulo(l, 7.6f, 8.6f, 1.0f, A(0x8FD3F4));
        break;
    case ICO_SI:
        trazo(l, 2.0f, 6.0f, 5.0f, 9.0f, 0.9f, A(0x5DBB63)); trazo(l, 5.0f, 9.0f, 10.0f, 2.5f, 0.9f, A(0x5DBB63));
        break;
    case ICO_NO:
        trazo(l, 2.5f, 2.5f, 9.5f, 9.5f, 0.9f, A(0xF0506E)); trazo(l, 9.5f, 2.5f, 2.5f, 9.5f, 0.9f, A(0xF0506E));
        break;
    case ICO_PLATO: /* tazón con comida */
        elipse(l, 6.0f, 5.6f, 4.2f, 2.4f, A(0xE8A04A));
        lz_set(l, 4, 5, A(0xFFD08A)); lz_set(l, 7, 4, A(0xFFD08A)); lz_set(l, 8, 6, A(0xC27A2E));
        for (int y = 6; y < 11; y++)
            for (int x = 0; x < 12; x++) {
                float dx = (x + 0.5f - 6.0f) / 5.6f, dy = (y + 0.5f - 6.0f) / 4.6f;
                if (dx * dx + dy * dy <= 1.0f) lz_set(l, x, y, y == 6 ? A(0x4DA6FF) : A(0xFFFFFF));
            }
        break;
    case ICO_DUCHA: /* bañera con grifo y burbujas */
        trazo(l, 1.0f, 4.5f, 1.0f, 1.0f, 0.4f, A(0xB6B1C6)); trazo(l, 1.0f, 1.0f, 3.0f, 1.0f, 0.4f, A(0xB6B1C6));
        lz_set(l, 3, 2, A(0x4DA6FF));
        circulo(l, 5.5f, 3.6f, 1.4f, A(0xCFF0FF)); circulo(l, 8.6f, 3.2f, 1.6f, A(0xCFF0FF)); lz_set(l, 8, 2, A(0xFFFFFF));
        rect(l, 0, 5, 12, 1, A(0xB6B1C6)); rect(l, 1, 6, 10, 3, A(0xFFFFFF)); rect(l, 2, 9, 8, 1, A(0xFFFFFF));
        lz_set(l, 2, 10, A(0x8A84A0)); lz_set(l, 9, 10, A(0x8A84A0));
        break;
    case ICO_ESTADO: /* "más": un signo + en un círculo */
        circulo(l, 6.0f, 6.0f, 5.0f, A(0x6A4FA0));
        rect(l, 5, 3, 2, 6, A(0xFFFFFF)); rect(l, 3, 5, 6, 2, A(0xFFFFFF));
        break;
    case ICO_BOMBILLA:
        circulo(l, 6.0f, 4.6f, 3.6f, A(0xFFE27A)); lz_set(l, 4, 3, A(0xFFFFFF)); lz_set(l, 4, 4, A(0xFFFFFF));
        rect(l, 4, 8, 4, 1, A(0xB6B1C6)); rect(l, 4, 9, 4, 1, A(0x8A84A0)); rect(l, 5, 10, 2, 1, A(0xB6B1C6));
        break;
    case ICO_JERINGA:
        trazo(l, 3.5f, 8.5f, 7.5f, 4.5f, 1.5f, A(0xEAF6FF));   /* cuerpo */
        trazo(l, 4.3f, 7.7f, 6.0f, 6.0f, 0.8f, A(0x7CC8FF));   /* líquido */
        trazo(l, 7.8f, 4.2f, 10.0f, 2.0f, 0.45f, A(0x8A84A0)); /* émbolo */
        trazo(l, 9.0f, 1.2f, 10.8f, 3.0f, 0.5f, A(0x8A84A0));
        trazo(l, 2.6f, 9.4f, 0.8f, 11.2f, 0.3f, A(0xB6B1C6));  /* aguja */
        break;
    case ICO_MACETA: /* maceta con flor */
        rect(l, 2, 7, 8, 1, A(0xD9773F)); rect(l, 3, 8, 6, 3, A(0xC0602E));
        trazo(l, 6.0f, 6.5f, 6.0f, 3.5f, 0.3f, A(0x5DBB63)); elipse(l, 7.8f, 5.0f, 1.2f, 0.6f, A(0x5DBB63));
        circulo(l, 6.0f, 2.4f, 1.9f, A(0xFF8FC8)); lz_set(l, 6, 2, A(0xFFE066));
        break;
    case ICO_PERSONA: /* tú: una carita amable con pelo y hombros */
        elipse(l, 6.0f, 11.0f, 4.8f, 3.4f, A(0x8A6FD0)); rect(l, 0, 11, 12, 1, TRANSP);
        circulo(l, 6.0f, 4.6f, 3.0f, A(0xFFD9B8));
        elipse(l, 6.0f, 2.6f, 3.2f, 1.7f, A(0x6B4A32)); rect(l, 3, 3, 1, 2, A(0x6B4A32)); rect(l, 8, 3, 1, 2, A(0x6B4A32));
        lz_set(l, 5, 5, K); lz_set(l, 7, 5, K); lz_set(l, 5, 7, K); lz_set(l, 6, 7, K); lz_set(l, 4, 6, A(0xFF9DB0)); lz_set(l, 8, 6, A(0xFF9DB0));
        break;
    /* --- comidas de temporada --- */
    case ICO_CE_FRUTILLA:
        circulo(l, 6.0f, 6.6f, 3.9f, A(0xE8344E)); triangulo(l, 2.2f, 7.0f, 9.8f, 7.0f, 6.0f, 11.6f, A(0xE8344E));
        elipse(l, 6.0f, 2.4f, 2.8f, 1.0f, A(0x4DAF5A)); rect(l, 5, 1, 2, 1, A(0x3E8A4A));
        lz_set(l, 4, 6, A(0xFFE27A)); lz_set(l, 7, 6, A(0xFFE27A)); lz_set(l, 5, 8, A(0xFFE27A)); lz_set(l, 7, 8, A(0xFFE27A)); lz_set(l, 6, 10, A(0xFFE27A));
        lz_set(l, 4, 5, A(0xFF8FA0));
        break;
    case ICO_CE_BROTES:
        rect(l, 0, 10, 12, 2, A(0x7A5230)); rect(l, 6, 4, 1, 6, A(0x5DBB63));
        elipse(l, 3.8f, 4.0f, 2.4f, 1.2f, A(0x7FD36B)); elipse(l, 8.4f, 3.0f, 2.4f, 1.2f, A(0x5DBB63));
        elipse(l, 6.0f, 1.8f, 1.1f, 1.5f, A(0x9BE88A));
        lz_set(l, 2, 9, A(0x5DBB63)); lz_set(l, 9, 9, A(0x5DBB63));
        break;
    case ICO_CE_PETALOS:
        for (int k = 0; k < 5; k++) { float an = 6.2832f * (float)k / 5.0f - 1.5708f; circulo(l, 6.0f + cosf(an) * 3.0f, 6.0f + sinf(an) * 3.0f, 2.0f, A(0xFF9FC8)); }
        circulo(l, 6.0f, 6.0f, 1.7f, A(0xFFD45C)); lz_set(l, 5, 5, A(0xFFF0A0));
        break;
    case ICO_CE_BERROS:
        trazo(l, 6.0f, 11.0f, 6.0f, 6.0f, 0.5f, A(0x3E8A4A)); trazo(l, 6.0f, 9.0f, 3.0f, 6.5f, 0.45f, A(0x3E8A4A)); trazo(l, 6.0f, 9.0f, 9.0f, 6.5f, 0.45f, A(0x3E8A4A));
        circulo(l, 6.0f, 4.0f, 3.2f, A(0x5DBB63)); circulo(l, 2.8f, 5.6f, 2.2f, A(0x7FD36B)); circulo(l, 9.2f, 5.6f, 2.2f, A(0x7FD36B));
        lz_set(l, 5, 3, A(0xB6F0A8)); lz_set(l, 2, 5, A(0xB6F0A8)); lz_set(l, 9, 5, A(0xB6F0A8));
        break;
    case ICO_CE_SANDIA:
        circulo(l, 6.0f, 3.6f, 5.6f, A(0x3E8A4A)); circulo(l, 6.0f, 3.6f, 4.8f, A(0xDDF0D0)); circulo(l, 6.0f, 3.6f, 4.1f, A(0xF0506E));
        rect(l, 0, 0, 12, 3, TRANSP);
        lz_set(l, 4, 5, K); lz_set(l, 6, 6, K); lz_set(l, 8, 5, K); lz_set(l, 5, 7, K); lz_set(l, 7, 7, K);
        break;
    case ICO_CE_DURAZNO:
        circulo(l, 6.0f, 7.0f, 4.4f, A(0xFFA27A)); circulo(l, 8.0f, 8.0f, 2.4f, A(0xFF7F6E)); lz_set(l, 4, 5, A(0xFFD0B0)); lz_set(l, 4, 6, A(0xFFD0B0));
        rect(l, 6, 1, 1, 3, A(0x8B5E3C)); elipse(l, 8.4f, 2.2f, 2.0f, 1.0f, A(0x5DBB63));
        trazo(l, 6.0f, 3.5f, 6.0f, 6.5f, 0.2f, A(0xE0705A));
        break;
    case ICO_CE_ARANDANOS:
        circulo(l, 4.0f, 7.4f, 3.0f, A(0x4B5FC4)); circulo(l, 8.4f, 7.0f, 3.0f, A(0x5B70D8)); circulo(l, 6.2f, 3.8f, 2.8f, A(0x3E4FB0));
        lz_set(l, 3, 6, A(0xB6C4FF)); lz_set(l, 7, 5, A(0xB6C4FF)); lz_set(l, 5, 2, A(0xB6C4FF));
        lz_set(l, 4, 7, A(0x2A2F6A)); lz_set(l, 8, 7, A(0x2A2F6A)); lz_set(l, 6, 4, A(0x2A2F6A));
        elipse(l, 6.0f, 1.0f, 1.6f, 0.8f, A(0x5DBB63));
        break;
    case ICO_CE_CHOCLO:
        elipse(l, 6.0f, 5.0f, 2.9f, 4.7f, A(0xFFD84A));
        for (int y = 1; y < 9; y += 2) for (int x = 4; x < 9; x += 2) lz_set(l, x, y, A(0xE8A810));
        for (int y = 2; y < 9; y += 2) for (int x = 5; x < 9; x += 2) lz_set(l, x, y, A(0xFFEE90));
        triangulo(l, 6.0f, 11.8f, 0.8f, 4.5f, 4.8f, 9.2f, A(0x4DAF5A)); triangulo(l, 6.0f, 11.8f, 11.2f, 4.5f, 7.2f, 9.2f, A(0x6CCB6A));
        rect(l, 5, 10, 2, 2, A(0x3E8A4A));
        break;
    case ICO_CE_CALABAZA2:
        elipse(l, 3.8f, 7.2f, 3.0f, 3.8f, A(0xE8721C)); elipse(l, 8.2f, 7.2f, 3.0f, 3.8f, A(0xE8721C)); elipse(l, 6.0f, 7.2f, 2.8f, 4.2f, A(0xFF9A3C));
        rect(l, 5, 1, 2, 3, A(0x5E8A3A)); lz_set(l, 7, 1, A(0x5E8A3A)); lz_set(l, 4, 6, A(0xFFC88A));
        break;
    case ICO_CE_CASTANAS:
        circulo(l, 4.0f, 7.4f, 3.4f, A(0x8B4A2B)); triangulo(l, 1.5f, 6.0f, 6.5f, 6.0f, 4.0f, 2.6f, A(0xC9A06A));
        circulo(l, 8.4f, 7.8f, 3.0f, A(0xA0583A)); triangulo(l, 6.2f, 7.0f, 10.6f, 7.0f, 8.4f, 4.0f, A(0xD8B880));
        lz_set(l, 3, 8, A(0xC47A52)); lz_set(l, 7, 9, A(0xD0906A));
        break;
    case ICO_CE_HONGOS:
        rect(l, 5, 6, 3, 5, A(0xF2E6C8)); rect(l, 5, 6, 1, 5, A(0xE0D0A8));
        elipse(l, 6.0f, 5.0f, 5.4f, 3.6f, A(0xC8553D)); rect(l, 0, 5, 12, 2, TRANSP); rect(l, 1, 6, 10, 1, A(0xC8553D));
        lz_set(l, 3, 3, A(0xFFFFFF)); lz_set(l, 6, 2, A(0xFFFFFF)); lz_set(l, 8, 4, A(0xFFFFFF)); lz_set(l, 5, 4, A(0xFFFFFF));
        break;
    case ICO_CE_HIGOS:
        circulo(l, 6.0f, 7.6f, 3.9f, A(0x7A3E8E)); triangulo(l, 3.4f, 6.6f, 8.6f, 6.6f, 6.0f, 2.2f, A(0x7A3E8E));
        rect(l, 5, 0, 2, 2, A(0x5E8A3A)); lz_set(l, 4, 8, A(0x9A5CB0)); lz_set(l, 4, 9, A(0x9A5CB0)); lz_set(l, 7, 10, A(0xC890D8));
        break;
    case ICO_CE_PINONES:
        elipse(l, 3.4f, 5.0f, 1.8f, 3.6f, A(0xD9B77C)); elipse(l, 8.6f, 5.2f, 1.8f, 3.6f, A(0xC9A060)); elipse(l, 6.0f, 7.6f, 1.8f, 3.4f, A(0xE6C98E));
        lz_set(l, 3, 8, A(0x5C3A22)); lz_set(l, 8, 8, A(0x5C3A22)); lz_set(l, 6, 11, A(0x5C3A22));
        lz_set(l, 3, 3, A(0xFFF0C8)); lz_set(l, 5, 6, A(0xFFF0C8));
        break;
    case ICO_CE_CAMOTE:
        circulo(l, 4.2f, 8.2f, 3.2f, A(0x9A3E5C)); circulo(l, 6.8f, 7.0f, 3.5f, A(0xB04A68)); circulo(l, 8.6f, 5.4f, 2.8f, A(0x9A3E5C));
        trazo(l, 1.2f, 10.8f, 2.6f, 9.8f, 0.4f, A(0x6A2A40));                          /* la colita */
        circulo(l, 8.4f, 5.0f, 2.0f, A(0xFF9A2E)); circulo(l, 8.4f, 5.0f, 1.1f, A(0xFFD060)); /* abierto: se ve lo naranja y calentito */
        lz_set(l, 5, 8, A(0x6A2A40)); lz_set(l, 4, 7, A(0xD87A98)); lz_set(l, 6, 6, A(0xD87A98));
        lz_set(l, 7, 1, A(0xFFFFFF)); lz_set(l, 7, 0, A(0xE8E4F0)); lz_set(l, 9, 2, A(0xFFFFFF)); lz_set(l, 10, 1, A(0xE8E4F0)); /* vapor */
        break;
    case ICO_CE_BAYAS:
        circulo(l, 4.2f, 7.0f, 2.9f, A(0x9FD8FF)); circulo(l, 8.4f, 6.4f, 2.9f, A(0xB6E2FF)); circulo(l, 6.2f, 3.6f, 2.6f, A(0x8CC8F0));
        lz_set(l, 3, 6, A(0xFFFFFF)); lz_set(l, 7, 5, A(0xFFFFFF)); lz_set(l, 5, 2, A(0xFFFFFF));
        lz_set(l, 1, 2, A(0xFFFFFF)); lz_set(l, 10, 2, A(0xFFFFFF)); lz_set(l, 6, 10, A(0xE8F6FF)); /* escarcha */
        break;
    case ICO_CE_MIEL_CRIST:
        triangulo(l, 6.0f, 0.6f, 1.2f, 6.0f, 10.8f, 6.0f, A(0xFFB830)); triangulo(l, 1.2f, 6.0f, 10.8f, 6.0f, 6.0f, 11.4f, A(0xE89A10));
        triangulo(l, 6.0f, 0.6f, 6.0f, 6.0f, 1.2f, 6.0f, A(0xFFD870)); lz_set(l, 4, 3, A(0xFFFFFF)); lz_set(l, 3, 5, A(0xFFF0B0));
        break;
    case ICO_SACUDIDA: /* el orbe que se sacude (v89: antes era un reloj): esfera de vidrio con su brillo, sobre su base */
        rect(l, 4, 10, 4, 2, A(0x6B5A8A)); circulo(l, 6.0f, 5.6f, 4.0f, A(0x5A6FD0)); circulo(l, 6.0f, 5.6f, 3.2f, A(0x8FA8FF));
        circulo(l, 6.4f, 6.2f, 2.0f, A(0xB48CFF)); lz_set(l, 4, 3, A(0xFFFFFF)); lz_set(l, 5, 3, A(0xE8F0FF)); lz_set(l, 4, 4, A(0xE8F0FF));
        trazo(l, 0.5f, 4.0f, 1.5f, 5.0f, 0.3f, A(0xFFE27A)); trazo(l, 0.5f, 7.0f, 1.5f, 8.0f, 0.3f, A(0xFFE27A));
        trazo(l, 11.5f, 4.0f, 10.5f, 5.0f, 0.3f, A(0xFFE27A)); trazo(l, 11.5f, 7.0f, 10.5f, 8.0f, 0.3f, A(0xFFE27A));
        break;
    case ICO_TERMOMETRO:
        rect(l, 5, 1, 2, 8, A(0xFFFFFF)); rect(l, 5, 5, 2, 4, A(0xF0506E)); circulo(l, 6.0f, 9.6f, 2.1f, A(0xF0506E));
        lz_set(l, 8, 2, A(0xB6B1C6)); lz_set(l, 8, 4, A(0xB6B1C6)); lz_set(l, 8, 6, A(0xB6B1C6));
        break;
    case ICO_DESPERTADOR:
        circulo(l, 2.5f, 2.5f, 1.6f, A(0xF0506E)); circulo(l, 9.5f, 2.5f, 1.6f, A(0xF0506E));
        circulo(l, 6.0f, 6.8f, 4.2f, A(0xFFFFFF));
        rect(l, 6, 4, 1, 3, K); rect(l, 6, 6, 2, 1, K);
        lz_set(l, 3, 11, K); lz_set(l, 8, 11, K);
        break;
    case ICO_ALERTA: /* triángulo de cuidado */
        triangulo(l, 6.0f, 0.5f, 0.5f, 11.0f, 11.5f, 11.0f, A(0xFFD45C));
        rect(l, 5, 4, 2, 4, K); rect(l, 5, 9, 2, 1, K);
        break;
    case ICO_PUNTOS:
        for (int k = 0; k < 3; k++) rect(l, 1 + k * 4, 5, 2, 2, A(0x8C84B0));
        break;
    case ICO_MANO_NO:
        rect(l, 2, 5, 6, 5, A(0xFFD9B8));
        for (int k = 0; k < 4; k++) rect(l, 2 + k + k / 2, 1 + (k == 0 || k == 3), 1, 5, A(0xFFD9B8));
        trazo(l, 8.0f, 6.0f, 11.0f, 9.0f, 0.5f, A(0xF0506E)); trazo(l, 11.0f, 6.0f, 8.0f, 9.0f, 0.5f, A(0xF0506E));
        break;
    case ICO_CARA_FELIZ: case ICO_CARA_TRISTE: case ICO_CARA_ENOJADA: case ICO_CARA_MIEDO: case ICO_CARA_CURIOSA:
    case ICO_CARA_ABURRIDA: case ICO_CARA_CANSADA: case ICO_CARA_ORGULLOSA: case ICO_CARA_CELOSA: case ICO_CARA_CARINO: {
        /* bichitos del juego: cuerpo blando, brote en la cabeza, ojos grandes y mejillas; cada ánimo cambia ojos, cejas y boca */
        static const uint32_t COL[10] = {0xFFD86B, 0x9FD3F7, 0xFF8A5C, 0xD9CCFF, 0xFFE58A, 0xD8D0C0, 0xC9B8F0, 0xFFC93C, 0xA8E58A, 0xFFB0CC};
        const uint32_t W_ = A(0xFFFFFF), CH = A(0xFF9DB0), AZ = A(0x7CC8FF);
        int k = (int)i - (int)ICO_CARA_FELIZ;
        elipse(l, 6.0f, 6.9f, 5.3f, 4.6f, A(COL[k]));
        rect(l, 6, 1, 1, 2, A(0x3E8A4F)); rect(l, 7, 0, 2, 2, A(0x86D16A)); lz_set(l, 7, 1, A(0x5DBB63));
        switch (i) {
        case ICO_CARA_FELIZ: /* ojos que sonríen y boca abierta */
            lz_set(l, 3, 6, K); lz_set(l, 4, 5, K); lz_set(l, 5, 6, K); lz_set(l, 7, 6, K); lz_set(l, 8, 5, K); lz_set(l, 9, 6, K);
            lz_set(l, 4, 8, K); rect(l, 5, 9, 2, 1, K); lz_set(l, 7, 8, K); lz_set(l, 2, 8, CH); lz_set(l, 10, 8, CH); break;
        case ICO_CARA_TRISTE: /* cejas caídas, boca hacia abajo y una lagrimita */
            rect(l, 3, 6, 2, 2, K); rect(l, 8, 6, 2, 2, K); lz_set(l, 3, 6, W_); lz_set(l, 8, 6, W_);
            lz_set(l, 3, 4, K); lz_set(l, 4, 3, K); lz_set(l, 8, 3, K); lz_set(l, 9, 4, K);
            lz_set(l, 4, 10, K); rect(l, 5, 9, 2, 1, K); lz_set(l, 7, 10, K); lz_set(l, 2, 8, AZ); lz_set(l, 2, 9, AZ); break;
        case ICO_CARA_ENOJADA: /* cejas bajas hacia el centro, dientes apretados */
            rect(l, 3, 6, 2, 2, K); rect(l, 8, 6, 2, 2, K);
            lz_set(l, 2, 4, K); lz_set(l, 3, 5, K); lz_set(l, 4, 5, K); lz_set(l, 9, 4, K); lz_set(l, 8, 5, K); lz_set(l, 7, 5, K);
            rect(l, 4, 9, 4, 1, K); lz_set(l, 5, 10, W_); lz_set(l, 7, 10, W_); lz_set(l, 11, 1, A(0xF0506E)); lz_set(l, 10, 2, A(0xF0506E)); break;
        case ICO_CARA_MIEDO: /* ojos enormes con pupilas chicas, boca temblorosa y gota de sudor */
            rect(l, 3, 4, 3, 4, W_); rect(l, 7, 4, 3, 4, W_); lz_set(l, 4, 6, K); lz_set(l, 8, 6, K);
            lz_set(l, 4, 10, K); lz_set(l, 5, 9, K); lz_set(l, 6, 10, K); lz_set(l, 7, 9, K); lz_set(l, 11, 2, AZ); lz_set(l, 11, 3, AZ); break;
        case ICO_CARA_CURIOSA: /* ojos grandes con brillo, una ceja arriba, boquita redonda */
            rect(l, 3, 5, 2, 3, K); rect(l, 8, 5, 2, 3, K); lz_set(l, 3, 5, W_); lz_set(l, 8, 5, W_);
            rect(l, 8, 3, 2, 1, K); lz_set(l, 9, 2, K); rect(l, 5, 9, 2, 2, K); lz_set(l, 2, 8, CH); lz_set(l, 10, 8, CH); break;
        case ICO_CARA_ABURRIDA: /* párpados a medias y boca recta */
            rect(l, 3, 6, 3, 1, K); rect(l, 7, 6, 3, 1, K); rect(l, 3, 5, 3, 1, A(0xB9B2A2)); rect(l, 7, 5, 3, 1, A(0xB9B2A2));
            rect(l, 5, 9, 3, 1, K); break;
        case ICO_CARA_CANSADA: /* ojos cerrados, un bostezo y sus zzz */
            rect(l, 3, 7, 3, 1, K); lz_set(l, 3, 6, K); lz_set(l, 5, 6, K); rect(l, 7, 7, 3, 1, K); lz_set(l, 7, 6, K); lz_set(l, 9, 6, K);
            circulo(l, 6.0f, 9.6f, 1.1f, K); rect(l, 9, 0, 2, 1, AZ); lz_set(l, 10, 1, AZ); rect(l, 9, 2, 2, 1, AZ); break;
        case ICO_CARA_ORGULLOSA: /* ojos contentos, sonrisa ancha y destellos */
            lz_set(l, 3, 6, K); lz_set(l, 4, 5, K); lz_set(l, 5, 6, K); lz_set(l, 7, 6, K); lz_set(l, 8, 5, K); lz_set(l, 9, 6, K);
            rect(l, 4, 9, 4, 1, K); lz_set(l, 3, 8, K); lz_set(l, 8, 8, K); lz_set(l, 1, 2, W_); lz_set(l, 0, 3, W_); lz_set(l, 1, 4, W_); lz_set(l, 10, 3, W_); break;
        case ICO_CARA_CELOSA: /* ojos de lado y cejas picudas */
            rect(l, 3, 6, 3, 2, W_); rect(l, 7, 6, 3, 2, W_); rect(l, 5, 6, 1, 2, K); rect(l, 9, 6, 1, 2, K);
            lz_set(l, 2, 4, K); lz_set(l, 3, 5, K); lz_set(l, 4, 5, K); lz_set(l, 9, 4, K); lz_set(l, 8, 5, K); lz_set(l, 7, 5, K);
            lz_set(l, 4, 9, K); rect(l, 5, 10, 3, 1, K); break;
        default: /* cariño: ojos de corazón y mejillas rosadas */
            rect(l, 3, 5, 2, 1, A(0xF0506E)); lz_set(l, 3, 6, A(0xF0506E)); lz_set(l, 4, 6, A(0xF0506E)); lz_set(l, 4, 7, A(0xF0506E));
            rect(l, 8, 5, 2, 1, A(0xF0506E)); lz_set(l, 8, 6, A(0xF0506E)); lz_set(l, 9, 6, A(0xF0506E)); lz_set(l, 8, 7, A(0xF0506E));
            lz_set(l, 4, 9, K); rect(l, 5, 10, 2, 1, K); lz_set(l, 7, 9, K); lz_set(l, 2, 8, CH); lz_set(l, 10, 8, CH); lz_set(l, 11, 1, A(0xF0506E)); break;
        }
        break;
    }
    case ICO_TAZA: /* taza de chocolate caliente con vapor */
        rect(l, 2, 5, 7, 6, A(0xF0506E)); rect(l, 3, 5, 5, 1, A(0x7A4A2A)); rect(l, 2, 10, 7, 1, A(0xC0384E));
        rect(l, 9, 6, 2, 1, A(0xF0506E)); rect(l, 10, 6, 1, 3, A(0xF0506E)); rect(l, 9, 8, 2, 1, A(0xF0506E));
        lz_set(l, 4, 3, A(0xFFFFFF)); lz_set(l, 5, 2, A(0xFFFFFF)); lz_set(l, 4, 1, A(0xFFFFFF));
        lz_set(l, 7, 3, A(0xFFFFFF)); lz_set(l, 6, 2, A(0xFFFFFF)); lz_set(l, 7, 1, A(0xFFFFFF));
        lz_set(l, 4, 7, A(0xFF8FA3));
        break;
    case ICO_VASO: /* vaso de jugo con hielo y bombilla */
        rect(l, 3, 3, 6, 8, A(0xFFC14D)); rect(l, 3, 3, 6, 1, A(0xFFE27A));
        rect(l, 4, 4, 2, 2, A(0xE8F4FF)); rect(l, 6, 6, 2, 2, A(0xE8F4FF));
        trazo(l, 7.5f, 3.0f, 9.5f, 0.5f, 0.35f, A(0xF0506E));
        lz_set(l, 3, 10, A(0xE09A2A)); lz_set(l, 8, 10, A(0xE09A2A));
        break;
    case ICO_CALOR: /* sol rojo con ondas de calor */
        circulo(l, 6.0f, 4.6f, 3.0f, A(0xFF7A3D)); circulo(l, 6.0f, 4.6f, 1.8f, A(0xFFC14D));
        for (int k = 0; k < 3; k++) {
            int x = 2 + k * 3;
            lz_set(l, x, 9, A(0xF0506E)); lz_set(l, x + 1, 10, A(0xF0506E)); lz_set(l, x, 11, A(0xF0506E));
        }
        break;
    case ICO_FRIO: /* termómetro azul y un copito */
        rect(l, 4, 1, 2, 8, A(0xFFFFFF)); rect(l, 4, 7, 2, 2, A(0x4DA6FF)); circulo(l, 5.0f, 9.6f, 2.1f, A(0x4DA6FF));
        trazo(l, 9.0f, 1.0f, 9.0f, 6.0f, 0.4f, A(0x9ED8FF)); trazo(l, 7.0f, 2.2f, 11.0f, 4.8f, 0.4f, A(0x9ED8FF));
        trazo(l, 7.0f, 4.8f, 11.0f, 2.2f, 0.4f, A(0x9ED8FF));
        break;
    case ICO_RAYO:
        triangulo(l, 7.5f, 0.5f, 2.5f, 6.5f, 6.5f, 6.5f, A(0xFFD84A));
        triangulo(l, 5.0f, 5.5f, 9.5f, 5.5f, 4.0f, 11.5f, A(0xFFD84A));
        break;
    case ICO_MANO: /* mano abierta (dónde la tocaste) */
        rect(l, 3, 5, 6, 5, A(0xFFD9B8));
        for (int k = 0; k < 4; k++) rect(l, 3 + k + k / 2, 1 + (k == 0 || k == 3), 1, 5, A(0xFFD9B8));
        rect(l, 9, 6, 2, 1, A(0xFFD9B8));
        break;
    case ICO_LUPA:
        circulo(l, 5.0f, 5.0f, 3.6f, A(0x5C6A88)); circulo(l, 5.0f, 5.0f, 2.4f, A(0xCFF0FF));
        trazo(l, 7.5f, 7.5f, 10.5f, 10.5f, 0.8f, A(0x8B5E3C));
        lz_set(l, 4, 4, A(0xFFFFFF));
        break;
    case ICO_PIEDRA: /* piedra brillante */
        triangulo(l, 6.0f, 1.0f, 1.5f, 6.0f, 10.5f, 6.0f, A(0x9EE6F0));
        triangulo(l, 1.5f, 6.0f, 10.5f, 6.0f, 6.0f, 11.0f, A(0x5FB8D8));
        lz_set(l, 4, 4, A(0xFFFFFF)); lz_set(l, 5, 3, A(0xFFFFFF));
        break;
    case ICO_SEMILLA:
        elipse(l, 6.0f, 7.0f, 3.0f, 3.8f, A(0xB07A3E)); elipse(l, 5.2f, 6.0f, 1.0f, 1.6f, A(0xD9A562));
        trazo(l, 6.0f, 3.0f, 8.0f, 1.0f, 0.5f, A(0x5DBB63)); elipse(l, 8.8f, 1.6f, 1.4f, 0.8f, A(0x5DBB63));
        break;
    case ICO_MIEL: /* tarrito de miel */
        rect(l, 2, 4, 8, 7, A(0xFFB020)); rect(l, 3, 2, 6, 2, A(0xC08A5A));
        rect(l, 3, 5, 2, 3, A(0xFFD45C)); rect(l, 2, 3, 8, 1, A(0x8B5E3C));
        break;
    case ICO_HIERBA: /* hojita medicinal con cruz */
        elipse(l, 6.0f, 6.0f, 3.2f, 4.8f, A(0x5DBB63)); trazo(l, 6.0f, 2.0f, 6.0f, 11.0f, 0.3f, A(0x3E8A4A));
        rect(l, 8, 1, 3, 1, A(0xF0506E)); rect(l, 9, 0, 1, 3, A(0xF0506E));
        break;
    case ICO_RAMITA:
        trazo(l, 1.5f, 10.0f, 10.0f, 2.0f, 0.6f, A(0x8B5E3C)); trazo(l, 5.0f, 7.0f, 4.0f, 3.0f, 0.4f, A(0x8B5E3C));
        elipse(l, 4.0f, 2.5f, 1.2f, 0.8f, A(0x5DBB63));
        break;
    case ICO_PREGUNTA:
        rect(l, 3, 1, 6, 2, A(0x8C6AD8)); rect(l, 8, 2, 2, 3, A(0x8C6AD8)); rect(l, 5, 5, 4, 2, A(0x8C6AD8));
        rect(l, 5, 6, 2, 2, A(0x8C6AD8)); rect(l, 5, 9, 2, 2, A(0x8C6AD8)); rect(l, 2, 2, 2, 2, A(0x8C6AD8));
        break;
    case ICO_CEPILLO:
        rect(l, 1, 3, 9, 4, A(0xC08A5A)); for (int k = 0; k < 5; k++) rect(l, 2 + k * 2, 7, 1, 3, A(0xFFF1D6));
        rect(l, 9, 4, 3, 2, A(0x8B5E3C));
        break;
    case ICO_LAGRIMA:
        circulo(l, 6.0f, 7.6f, 2.8f, A(0x7CC8FF)); triangulo(l, 6.0f, 1.5f, 3.4f, 7.0f, 8.6f, 7.0f, A(0x7CC8FF));
        lz_set(l, 5, 7, A(0xFFFFFF));
        break;
    case ICO_AJUSTES: /* engranaje */
        circulo(l, 6.0f, 6.0f, 3.6f, A(0xB6B1C6));
        for (int k = 0; k < 8; k++) { float a = k * 0.785f; circulo(l, 6 + cosf(a) * 4.2f, 6 + sinf(a) * 4.2f, 1.1f, A(0xB6B1C6)); }
        circulo(l, 6.0f, 6.0f, 1.5f, TRANSP);
        break;
    case ICO_MARIPOSA:
    case ICO_MARIPOSA_B: { /* alas violeta arriba y rosadas abajo, con lunares; cuerpo fino y antenas. B: alas casi cerradas */
        bool b = i == ICO_MARIPOSA_B;
        const uint32_t v1 = A(0xA98BF5), v2 = A(0x7A5CD8), r1 = A(0xFF86BA), r2 = A(0xE0529A), am = A(0xFFE27A);
        if (!b) {
            elipse(l, 3.0f, 3.6f, 3.0f, 2.6f, v1); elipse(l, 9.0f, 3.6f, 3.0f, 2.6f, v1);
            elipse(l, 3.6f, 8.2f, 2.3f, 2.1f, r1); elipse(l, 8.4f, 8.2f, 2.3f, 2.1f, r1);
            rect(l, 1, 4, 2, 1, v2); rect(l, 9, 4, 2, 1, v2); rect(l, 2, 9, 2, 1, r2); rect(l, 8, 9, 2, 1, r2);
            lz_set(l, 2, 2, A(0xFFFFFF)); lz_set(l, 9, 2, A(0xFFFFFF)); lz_set(l, 3, 8, am); lz_set(l, 8, 8, am);
        } else {
            elipse(l, 4.7f, 4.0f, 1.5f, 3.0f, v1); elipse(l, 7.3f, 4.0f, 1.5f, 3.0f, v1);
            elipse(l, 4.9f, 8.2f, 1.2f, 1.9f, r1); elipse(l, 7.1f, 8.2f, 1.2f, 1.9f, r1);
            lz_set(l, 4, 2, A(0xFFFFFF)); lz_set(l, 7, 2, A(0xFFFFFF)); lz_set(l, 4, 8, am); lz_set(l, 7, 8, am);
        }
        rect(l, 6, 3, 1, 7, K); circulo(l, 6.5f, 2.6f, 0.9f, K);
        lz_set(l, 5, 1, K); lz_set(l, 4, 0, K); lz_set(l, 7, 1, K); lz_set(l, 8, 0, K); /* antenas */
        break;
    }
    case ICO_ABEJA:
    case ICO_ABEJA_B:
        if (i == ICO_ABEJA) { elipse(l, 4.0f, 3.0f, 2.2f, 1.8f, A(0xE4F4FF)); elipse(l, 8.0f, 3.0f, 2.2f, 1.8f, A(0xE4F4FF)); }
        else { elipse(l, 4.6f, 3.6f, 1.4f, 2.4f, A(0xE4F4FF)); elipse(l, 7.4f, 3.6f, 1.4f, 2.4f, A(0xE4F4FF)); }
        elipse(l, 6.0f, 7.4f, 4.6f, 3.2f, A(0xFFC93C));
        rect(l, 4, 5, 1, 6, K); rect(l, 7, 5, 1, 6, K);
        lz_set(l, 10, 8, K); lz_set(l, 11, 8, K); lz_set(l, 2, 6, K);
        break;
    case ICO_PAJARO:
    case ICO_PAJARO_B:
        elipse(l, 6.0f, 7.0f, 4.0f, 3.2f, A(0x6FB7FF)); circulo(l, 8.6f, 4.4f, 2.4f, A(0x6FB7FF));
        elipse(l, 6.0f, 8.4f, 2.6f, 1.6f, A(0xFFF1D6));
        triangulo(l, 10.6f, 4.0f, 10.6f, 5.6f, 12.0f, 4.8f, A(0xFFB020));
        lz_set(l, 9, 4, K);
        if (i == ICO_PAJARO) triangulo(l, 2.0f, 5.0f, 6.6f, 6.4f, 4.0f, 1.0f, A(0x3E8AD8));
        else triangulo(l, 2.0f, 6.6f, 6.6f, 6.6f, 3.6f, 10.6f, A(0x3E8AD8));
        triangulo(l, 0.4f, 6.0f, 2.4f, 6.6f, 2.0f, 9.0f, A(0x3E8AD8));
        break;
    case ICO_ZORRO:
        elipse(l, 5.0f, 8.0f, 3.6f, 2.6f, A(0xF07A2A)); /* cuerpo */
        elipse(l, 1.6f, 7.4f, 1.8f, 2.6f, A(0xF07A2A)); lz_set(l, 1, 5, A(0xFFFFFF)); lz_set(l, 0, 5, A(0xFFFFFF)); /* cola */
        circulo(l, 8.6f, 5.2f, 2.6f, A(0xF07A2A)); /* cabeza */
        triangulo(l, 6.4f, 4.0f, 8.0f, 3.6f, 6.8f, 0.6f, A(0xF07A2A)); triangulo(l, 9.4f, 3.6f, 11.0f, 4.0f, 10.6f, 0.6f, A(0xF07A2A));
        triangulo(l, 8.0f, 6.0f, 12.0f, 6.0f, 11.6f, 7.6f, A(0xFFFFFF)); lz_set(l, 11, 6, K);
        lz_set(l, 8, 5, K); lz_set(l, 10, 5, K);
        rect(l, 3, 10, 1, 2, A(0x5A3A2A)); rect(l, 6, 10, 1, 2, A(0x5A3A2A));
        break;
    case ICO_RANA:
        elipse(l, 6.0f, 7.6f, 4.8f, 3.2f, A(0x6CC24A));
        circulo(l, 3.6f, 4.0f, 1.8f, A(0x6CC24A)); circulo(l, 8.4f, 4.0f, 1.8f, A(0x6CC24A));
        lz_set(l, 3, 4, K); lz_set(l, 8, 4, K); lz_set(l, 3, 3, A(0xFFFFFF)); lz_set(l, 8, 3, A(0xFFFFFF));
        rect(l, 4, 8, 4, 1, A(0x3E8A3A)); elipse(l, 6.0f, 9.8f, 2.6f, 1.0f, A(0xC9EFA8));
        rect(l, 1, 10, 2, 1, A(0x6CC24A)); rect(l, 9, 10, 2, 1, A(0x6CC24A));
        break;
    case ICO_BROTE:
        elipse(l, 6.0f, 10.4f, 4.6f, 1.4f, A(0x8B5E3C));
        rect(l, 6, 5, 1, 5, A(0x5DBB63));
        elipse(l, 4.2f, 5.0f, 2.0f, 1.1f, A(0x7FD36B)); elipse(l, 7.8f, 4.0f, 2.0f, 1.1f, A(0x5DBB63));
        break;
    case ICO_FLOR_MUSTIA:
        elipse(l, 6.0f, 10.6f, 4.6f, 1.2f, A(0x8B5E3C));
        trazo(l, 5.0f, 10.0f, 5.6f, 5.0f, 0.4f, A(0x9DA05A)); trazo(l, 5.6f, 5.0f, 8.4f, 4.2f, 0.4f, A(0x9DA05A));
        circulo(l, 9.0f, 5.6f, 1.6f, A(0xC79AB0)); circulo(l, 7.8f, 6.4f, 1.3f, A(0xC79AB0));
        lz_set(l, 9, 6, A(0xD8C27A));
        elipse(l, 3.8f, 7.8f, 1.6f, 0.8f, A(0x9DA05A));
        lz_set(l, 2, 2, A(0x7CC8FF)); lz_set(l, 2, 3, A(0x7CC8FF)); /* gotita: quiere agua */
        break;
    case ICO_ARBOLITO:
    case ICO_ARBOL_FRUTAL:
        rect(l, 5, 7, 2, 5, A(0x8B5E3C));
        circulo(l, 6.0f, 4.6f, 4.4f, A(0x4FAE5A)); circulo(l, 4.4f, 3.4f, 1.8f, A(0x7FD36B));
        if (i == ICO_ARBOL_FRUTAL) {
            circulo(l, 3.4f, 5.6f, 1.0f, A(0xF0506E)); circulo(l, 8.4f, 3.2f, 1.0f, A(0xF0506E)); circulo(l, 7.8f, 6.6f, 1.0f, A(0xF0506E));
        }
        break;
    case ICO_REGADERA:
        rect(l, 3, 5, 6, 5, A(0x6FB7FF)); rect(l, 3, 5, 6, 1, A(0x9ED8FF));
        trazo(l, 8.5f, 7.5f, 11.0f, 3.5f, 0.6f, A(0x6FB7FF)); rect(l, 10, 2, 2, 2, A(0x3E8AD8));
        trazo(l, 3.0f, 5.5f, 1.0f, 3.0f, 0.4f, A(0x3E8AD8)); trazo(l, 1.0f, 3.0f, 4.5f, 2.5f, 0.4f, A(0x3E8AD8));
        lz_set(l, 11, 5, A(0x9ED8FF)); lz_set(l, 11, 7, A(0x9ED8FF));
        break;
    case ICO_HOJAS:
        elipse(l, 6.0f, 8.4f, 5.6f, 3.2f, A(0xC1502E));
        elipse(l, 4.2f, 7.0f, 2.6f, 1.6f, A(0xE86A3A)); elipse(l, 8.0f, 7.4f, 2.6f, 1.6f, A(0xF2A33A));
        elipse(l, 6.0f, 5.6f, 2.2f, 1.4f, A(0xFFC14D)); elipse(l, 3.0f, 9.6f, 1.6f, 1.0f, A(0xF2A33A));
        lz_set(l, 6, 4, A(0x8B5E3C));
        break;
    case ICO_CHARCO:
        elipse(l, 6.0f, 7.6f, 5.6f, 2.6f, A(0x6FA8E0)); elipse(l, 5.6f, 7.2f, 4.0f, 1.6f, A(0x9ED8FF));
        rect(l, 3, 7, 2, 1, A(0xFFFFFF));
        lz_set(l, 2, 3, A(0x9ED8FF)); lz_set(l, 9, 2, A(0x9ED8FF)); lz_set(l, 10, 4, A(0x9ED8FF));
        break;
    case ICO_ZONA_PRADO:
        rect(l, 0, 8, 12, 4, A(0x7FD36B)); rect(l, 0, 8, 12, 1, A(0x5DBB63));
        rect(l, 3, 4, 6, 4, A(0xFFF1D6)); triangulo(l, 1.6f, 4.6f, 10.4f, 4.6f, 6.0f, 0.6f, A(0xF0506E)); rect(l, 5, 5, 2, 3, A(0x8B5E3C));
        lz_set(l, 1, 10, A(0xFF8FC8)); lz_set(l, 10, 10, A(0xFFE066));
        break;
    case ICO_ZONA_BOSQUE:
        triangulo(l, 4.0f, 0.6f, 0.4f, 8.6f, 7.6f, 8.6f, A(0x2E8B57)); triangulo(l, 8.4f, 2.6f, 5.2f, 9.6f, 11.6f, 9.6f, A(0x3FA86A));
        rect(l, 3, 9, 2, 3, A(0x8B5E3C)); rect(l, 8, 10, 1, 2, A(0x8B5E3C));
        break;
    case ICO_ZONA_LAGO:
        elipse(l, 6.0f, 7.0f, 5.8f, 4.0f, A(0x4DA6FF));
        trazo(l, 2.5f, 6.0f, 5.0f, 6.0f, 0.4f, A(0xCFF0FF)); trazo(l, 6.5f, 8.5f, 9.5f, 8.5f, 0.4f, A(0xCFF0FF));
        rect(l, 10, 1, 1, 5, A(0x5FA84F)); rect(l, 10, 0, 1, 2, A(0x8B5E3C));
        break;
    case ICO_ZONA_COLINA:
        elipse(l, 6.0f, 12.0f, 7.0f, 6.4f, A(0x7FD36B)); elipse(l, 6.0f, 12.0f, 7.0f, 1.0f, A(0x5DBB63));
        rect(l, 6, 3, 1, 3, A(0x8B5E3C)); circulo(l, 6.5f, 2.6f, 2.2f, A(0x3FA86A));
        circulo(l, 1.8f, 1.8f, 1.4f, A(0xFFC93C));
        break;
    case ICO_ZONA_ISLA:
        elipse(l, 6.0f, 9.5f, 6.0f, 2.4f, A(0x4DA6FF));
        elipse(l, 5.6f, 8.8f, 4.2f, 1.6f, A(0xF2D8A0));
        elipse(l, 5.6f, 8.4f, 3.2f, 1.1f, A(0x7FD36B));
        rect(l, 5, 4, 1, 4, A(0x8B5E3C)); circulo(l, 5.4f, 3.4f, 2.4f, A(0x3FA86A)); circulo(l, 4.4f, 2.6f, 1.0f, A(0x7FD36B));
        rect(l, 9, 3, 2, 5, A(0xFFFFFF)); rect(l, 9, 5, 2, 1, A(0xF0506E)); rect(l, 9, 2, 2, 1, A(0xFFE27A)); /* el faro */
        break;
    case ICO_CARTA: /* un sobre con su sello de cera */
        rect(l, 1, 3, 10, 7, A(0xF4E6C4)); rect(l, 1, 3, 10, 1, A(0xFFF6DE));
        triangulo(l, 1.0f, 3.0f, 11.0f, 3.0f, 6.0f, 7.5f, A(0xE2CFA4));
        trazo(l, 1.5f, 9.5f, 4.5f, 7.0f, 0.3f, A(0xD8C296)); trazo(l, 10.5f, 9.5f, 7.5f, 7.0f, 0.3f, A(0xD8C296));
        circulo(l, 6.0f, 7.0f, 1.8f, A(0xC8323C)); lz_set(l, 5, 6, A(0xF0646C));
        break;
    case ICO_VIRUS: /* un bichito de resfrío: bolita verde con pinchos y carita (8x8, simpático) */
        for (int k = 0; k < 8; k++) {
            float a = (float)k * 0.785f;
            trazo(l, 6.0f + cosf(a) * 2.6f, 6.0f + sinf(a) * 2.6f, 6.0f + cosf(a) * 3.8f, 6.0f + sinf(a) * 3.8f, 0.35f, A(0x7AC85A));
            circulo(l, 6.0f + cosf(a) * 4.0f, 6.0f + sinf(a) * 4.0f, 0.6f, A(0xB8F08A));
        }
        circulo(l, 6.0f, 6.0f, 2.8f, A(0x8ADC6A)); lz_set(l, 5, 5, A(0xC8FFA8));
        lz_set(l, 5, 6, A(0x2A2238)); lz_set(l, 7, 6, A(0x2A2238)); lz_set(l, 6, 7, A(0x2A6A3A));
        break;
    case ICO_MALESTAR: /* tres líneas onduladas moradas */
        for (int k = 0; k < 3; k++)
            for (int y = 2; y < 11; y++) lz_set(l, 2 + k * 4 + ((y / 2) % 2), y, A(0x9A6AD8));
        break;
    case ICO_ALGA: /* un alga verde que ondula */
        for (int k = 0; k < 3; k++)
            for (int y = 2 + k; y < 12; y++) lz_set(l, 3 + k * 3 + ((y + k) / 2) % 2, y, k == 1 ? A(0x3E9A4E) : A(0x5DBB63));
        lz_set(l, 4, 3, A(0x8FE08A)); lz_set(l, 10, 5, A(0x8FE08A));
        break;
    case ICO_CANGREJO: /* un cangrejito rojo con sus tenazas */
        elipse(l, 6.0f, 7.4f, 3.6f, 2.4f, A(0xE5484D)); elipse(l, 5.4f, 6.8f, 1.6f, 0.8f, A(0xFF7A7E));
        circulo(l, 1.6f, 4.4f, 1.4f, A(0xE5484D)); circulo(l, 10.4f, 4.4f, 1.4f, A(0xE5484D)); lz_set(l, 1, 3, TRANSP); lz_set(l, 10, 3, TRANSP);
        trazo(l, 2.6f, 5.4f, 3.6f, 6.6f, 0.35f, A(0xC8323C)); trazo(l, 9.4f, 5.4f, 8.4f, 6.6f, 0.35f, A(0xC8323C));
        for (int k = 0; k < 3; k++) { lz_set(l, 3 + k, 10, A(0xC8323C)); lz_set(l, 7 + k, 10, A(0xC8323C)); }
        lz_set(l, 5, 5, A(0x2A2238)); lz_set(l, 7, 5, A(0x2A2238)); lz_set(l, 5, 4, A(0xFFFFFF)); lz_set(l, 7, 4, A(0xFFFFFF));
        break;
    case ICO_NENUFAR: /* una hoja de nenúfar redonda con su muesca y una florcita */
        elipse(l, 6.0f, 7.0f, 5.4f, 3.6f, A(0x4FB85A)); triangulo(l, 6.0f, 7.0f, 8.0f, 3.2f, 10.6f, 4.6f, TRANSP);
        trazo(l, 6.0f, 7.0f, 2.0f, 6.0f, 0.25f, A(0x2E8A4A)); trazo(l, 6.0f, 7.0f, 5.0f, 10.4f, 0.25f, A(0x2E8A4A)); trazo(l, 6.0f, 7.0f, 10.0f, 9.0f, 0.25f, A(0x2E8A4A));
        circulo(l, 3.6f, 4.4f, 1.2f, A(0xFF8FC0)); lz_set(l, 3, 4, A(0xFFE066));
        break;
    case ICO_PEZ_DORADO: /* un pez dorado que brilla */
        elipse(l, 6.5f, 6.0f, 4.0f, 2.6f, A(0xFFC93C)); triangulo(l, 0.4f, 3.2f, 0.4f, 8.8f, 3.4f, 6.0f, A(0xFFB020));
        elipse(l, 6.8f, 7.0f, 2.6f, 1.0f, A(0xFFF0A0)); lz_set(l, 9, 5, K); trazo(l, 5.0f, 3.6f, 5.6f, 8.4f, 0.25f, A(0xE09A1A));
        lz_set(l, 11, 1, A(0xFFFFFF)); lz_set(l, 10, 2, A(0xFFF6B0)); lz_set(l, 2, 10, A(0xFFF6B0));
        break;
    case ICO_ERIZO: /* un erizo café con su cara clara, de perfil */
        for (int k = 0; k < 7; k++) triangulo(l, 2.0f + k * 1.2f, 7.0f, 3.6f + k * 1.2f, 7.0f, 2.2f + k * 1.3f - 0.6f, 2.6f + (k % 2) * 0.8f, A(0x6A4A30));
        elipse(l, 5.6f, 8.0f, 4.6f, 2.8f, A(0x8A6440)); elipse(l, 9.4f, 8.6f, 2.0f, 1.8f, A(0xE8C8A0));
        lz_set(l, 11, 9, A(0x2A2238)); lz_set(l, 9, 8, A(0x2A2238)); lz_set(l, 4, 11, A(0x5A3A20)); lz_set(l, 8, 11, A(0x5A3A20));
        break;
    case ICO_ERIZO_BOLA: /* hecho bolita: todo pinchos */
        circulo(l, 6.0f, 7.0f, 4.2f, A(0x8A6440));
        for (int k = 0; k < 10; k++) { float a = (float)k * 0.628f; trazo(l, 6.0f + cosf(a) * 3.4f, 7.0f + sinf(a) * 3.4f, 6.0f + cosf(a) * 5.4f, 7.0f + sinf(a) * 5.0f, 0.4f, A(0x6A4A30)); }
        lz_set(l, 5, 6, A(0xA88460));
        break;
    case ICO_ARDILLA: /* una ardilla con su cola peluda enroscada */
        elipse(l, 3.6f, 5.0f, 2.8f, 4.2f, A(0xD87A3A)); elipse(l, 3.8f, 4.2f, 1.4f, 2.6f, A(0xF2A35A)); /* la cola */
        elipse(l, 7.4f, 8.2f, 2.6f, 2.8f, A(0xC8642A)); elipse(l, 7.8f, 9.0f, 1.4f, 1.6f, A(0xF6D8B0));
        circulo(l, 8.6f, 4.8f, 2.0f, A(0xC8642A)); lz_set(l, 9, 2, A(0xC8642A)); lz_set(l, 9, 4, A(0x2A2238)); lz_set(l, 10, 5, A(0x5A3A20));
        break;
    case ICO_CONEJO: /* un conejito gris claro con orejas largas */
        elipse(l, 6.0f, 8.6f, 3.6f, 2.8f, A(0xD8D4E0)); circulo(l, 8.2f, 6.2f, 2.2f, A(0xE6E2EE));
        elipse(l, 7.4f, 2.6f, 0.8f, 2.4f, A(0xE6E2EE)); elipse(l, 9.0f, 2.8f, 0.8f, 2.4f, A(0xE6E2EE)); lz_set(l, 7, 2, A(0xFFB0C8)); lz_set(l, 9, 3, A(0xFFB0C8));
        circulo(l, 2.6f, 8.6f, 1.2f, A(0xFFFFFF)); lz_set(l, 9, 6, A(0x2A2238)); lz_set(l, 10, 7, A(0xFF8FB0));
        break;
    case ICO_CARACOL: /* un caracol: concha en espiral y su cuerpito con antenas */
        elipse(l, 6.0f, 9.4f, 5.0f, 1.4f, A(0xA8C890)); trazo(l, 9.6f, 8.6f, 10.6f, 5.6f, 0.3f, A(0xA8C890)); trazo(l, 10.2f, 8.6f, 11.4f, 6.2f, 0.3f, A(0xA8C890));
        circulo(l, 5.4f, 6.4f, 3.2f, A(0xE8A05A)); circulo(l, 5.6f, 6.2f, 2.0f, A(0xC87A3A)); circulo(l, 5.8f, 6.0f, 0.9f, A(0xE8A05A));
        lz_set(l, 4, 4, A(0xFFD8A0));
        break;
    case ICO_TORTUGA: /* una tortuga sabia: caparazón verde con dibujo */
        elipse(l, 5.6f, 7.6f, 4.6f, 3.0f, A(0x4F8A4A)); elipse(l, 5.6f, 7.0f, 3.4f, 2.0f, A(0x6CAA5A));
        lz_set(l, 4, 7, A(0x3E6E3A)); lz_set(l, 6, 6, A(0x3E6E3A)); lz_set(l, 7, 8, A(0x3E6E3A));
        circulo(l, 10.4f, 8.4f, 1.4f, A(0xA8C890)); lz_set(l, 11, 8, A(0x2A2238));
        lz_set(l, 3, 10, A(0xA8C890)); lz_set(l, 8, 10, A(0xA8C890));
        break;
    case ICO_PATO: /* un pato blanco con pico naranjo */
        elipse(l, 5.6f, 8.0f, 4.4f, 2.4f, A(0xF4F4F8)); elipse(l, 4.6f, 7.4f, 2.0f, 1.0f, A(0xDCDCE6));
        circulo(l, 8.8f, 4.6f, 1.9f, A(0xF4F4F8)); rect(l, 10, 5, 2, 1, A(0xF2A33A)); lz_set(l, 9, 4, A(0x2A2238));
        rect(l, 2, 10, 8, 1, A(0x9ED8FF));
        break;
    case ICO_BUHO: /* un búho de ojos grandes */
        elipse(l, 6.0f, 7.0f, 4.2f, 4.6f, A(0x8A6440)); elipse(l, 6.0f, 8.6f, 2.6f, 2.4f, A(0xD8B88A));
        circulo(l, 4.4f, 5.4f, 1.6f, A(0xFFF6D0)); circulo(l, 7.6f, 5.4f, 1.6f, A(0xFFF6D0)); lz_set(l, 4, 5, A(0x2A2238)); lz_set(l, 7, 5, A(0x2A2238));
        lz_set(l, 6, 7, A(0xF2A33A)); lz_set(l, 2, 2, A(0x8A6440)); lz_set(l, 9, 2, A(0x8A6440));
        break;
    case ICO_TOPO: /* un topo gruñón asomado en su montoncito de tierra */
        elipse(l, 6.0f, 10.0f, 5.4f, 1.8f, A(0x8A6440));
        elipse(l, 6.0f, 6.6f, 3.2f, 3.4f, A(0x4A4458)); circulo(l, 6.0f, 8.0f, 1.0f, A(0xFF9AB4));
        trazo(l, 3.6f, 4.6f, 5.2f, 5.4f, 0.3f, A(0x2A2238)); trazo(l, 8.4f, 4.6f, 6.8f, 5.4f, 0.3f, A(0x2A2238)); /* cejas de gruñón */
        lz_set(l, 4, 6, A(0x1A1626)); lz_set(l, 7, 6, A(0x1A1626));
        break;
    case ICO_TOPO_HOYO:
        elipse(l, 6.0f, 9.4f, 5.4f, 2.4f, A(0x8A6440)); elipse(l, 6.0f, 9.0f, 2.2f, 0.9f, A(0x3A2A1E)); lz_set(l, 3, 8, A(0xA88460));
        break;
    case ICO_SOPLA: /* una boquita que sopla, con ondas de aire */
        circulo(l, 3.5f, 6.0f, 3.2f, A(0xFFD8B0)); circulo(l, 2.6f, 5.0f, 0.6f, A(0x2A2238));
        elipse(l, 6.2f, 6.6f, 1.2f, 1.0f, A(0xE8607A)); lz_set(l, 6, 6, A(0x7A2038));
        trazo(l, 8.0f, 4.0f, 11.0f, 3.2f, 0.35f, A(0xBFE6FF)); trazo(l, 8.4f, 6.6f, 11.6f, 6.6f, 0.35f, A(0xBFE6FF)); trazo(l, 8.0f, 9.0f, 11.0f, 9.8f, 0.35f, A(0xBFE6FF));
        break;
    case ICO_ZONA_CUEVA:
        elipse(l, 6.0f, 8.0f, 6.0f, 6.0f, A(0x8A84A8)); rect(l, 0, 11, 12, 1, A(0x6E6890));
        elipse(l, 6.0f, 9.4f, 3.4f, 4.0f, A(0x2A2440));
        triangulo(l, 4.6f, 11.0f, 6.0f, 11.0f, 5.2f, 7.4f, A(0x7FE0D6)); lz_set(l, 5, 9, A(0xFFFFFF));
        break;
    case ICO_FLECHA:
        triangulo(l, 3.0f, 1.0f, 3.0f, 11.0f, 10.0f, 6.0f, A(0xFFFFFF));
        break;
    /* --- versión 23: treinta animales más (de perfil, mirando a la derecha) --- */
    case ICO_COLIBRI: /* chiquitito, verde brillante, pico largo y alas que zumban */
        elipse(l, 5.4f, 6.8f, 2.6f, 1.6f, A(0x3FBF8A)); circulo(l, 7.8f, 5.6f, 1.4f, A(0x3FBF8A)); elipse(l, 5.4f, 7.6f, 1.6f, 0.8f, A(0xE8607A));
        trazo(l, 9.0f, 5.4f, 11.8f, 5.0f, 0.25f, A(0x2A2238)); lz_set(l, 8, 5, K);
        elipse(l, 4.4f, 3.6f, 2.4f, 1.0f, A(0xBFE6FF)); triangulo(l, 3.0f, 7.0f, 0.4f, 6.0f, 0.8f, 8.6f, A(0x2E9A6E));
        break;
    case ICO_CARPINTERO: /* negro y blanco con la cabeza roja, agarrado al tronco */
        rect(l, 0, 0, 3, 12, A(0x8A6440)); lz_set(l, 1, 3, A(0x6A4A30)); lz_set(l, 1, 8, A(0x6A4A30));
        elipse(l, 5.6f, 7.4f, 2.4f, 3.6f, A(0x2A2238)); elipse(l, 6.4f, 8.0f, 1.2f, 2.6f, A(0xF4F4F8));
        circulo(l, 6.8f, 3.6f, 2.0f, A(0xF0506E)); rect(l, 8, 3, 3, 1, A(0x6A6A80)); lz_set(l, 7, 3, A(0xFFFFFF));
        lz_set(l, 4, 11, A(0x2A2238));
        break;
    case ICO_MARTIN: /* azul brillante con la panza naranja y el pico como lanza */
        elipse(l, 5.4f, 7.0f, 3.4f, 2.4f, A(0x2E8AE6)); circulo(l, 7.6f, 4.8f, 2.0f, A(0x2E8AE6)); elipse(l, 5.6f, 8.2f, 2.4f, 1.2f, A(0xF2A33A));
        triangulo(l, 9.0f, 4.2f, 9.0f, 5.6f, 12.0f, 5.0f, A(0x2A2238)); lz_set(l, 8, 4, A(0xFFFFFF)); lz_set(l, 7, 5, A(0xF2C890));
        triangulo(l, 2.4f, 7.0f, 0.2f, 9.0f, 2.8f, 9.4f, A(0x1E6AB8));
        break;
    case ICO_GOLONDRINA: /* volando: alas largas en punta y la cola de tijera */
        elipse(l, 6.0f, 6.4f, 2.4f, 1.4f, A(0x2A3A6A)); circulo(l, 8.4f, 5.8f, 1.2f, A(0x2A3A6A)); lz_set(l, 9, 6, A(0xE8607A));
        elipse(l, 6.0f, 7.0f, 1.6f, 0.6f, A(0xF4F1FF));
        triangulo(l, 5.0f, 5.6f, 7.0f, 5.6f, 3.0f, 0.6f, A(0x34467A)); triangulo(l, 5.0f, 7.0f, 7.0f, 7.0f, 3.4f, 11.2f, A(0x34467A));
        triangulo(l, 3.8f, 6.0f, 3.8f, 7.0f, 0.2f, 4.6f, A(0x2A3A6A)); triangulo(l, 3.8f, 6.4f, 3.8f, 7.2f, 0.4f, 8.6f, A(0x2A3A6A));
        lz_set(l, 9, 5, A(0xFFFFFF));
        break;
    case ICO_CISNE: /* blanco, con el cuello curvo y el pico naranja */
        elipse(l, 5.4f, 8.6f, 4.4f, 2.2f, A(0xF8F8FF)); elipse(l, 4.4f, 7.6f, 2.6f, 1.4f, A(0xE6E6F2));
        trazo(l, 8.4f, 8.0f, 8.6f, 4.0f, 0.7f, A(0xF8F8FF)); circulo(l, 8.0f, 3.2f, 1.4f, A(0xF8F8FF));
        rect(l, 9, 3, 2, 1, A(0xF2A33A)); lz_set(l, 8, 3, K); rect(l, 1, 11, 10, 1, A(0x9ED8FF));
        break;
    case ICO_URRACA: /* blanca y negra con la cola larga */
        elipse(l, 6.0f, 6.8f, 2.8f, 2.2f, A(0x2A2238)); elipse(l, 6.4f, 7.4f, 1.6f, 1.4f, A(0xF8F8FF));
        circulo(l, 8.6f, 4.6f, 1.7f, A(0x2A2238)); triangulo(l, 10.0f, 4.2f, 10.0f, 5.2f, 11.6f, 4.8f, A(0x4A4458)); lz_set(l, 9, 4, A(0xFFFFFF));
        triangulo(l, 3.6f, 6.0f, 3.6f, 8.0f, 0.0f, 10.4f, A(0x3A4A8A)); lz_set(l, 5, 6, A(0x4A6AC0));
        rect(l, 6, 9, 1, 2, A(0x4A4458));
        break;
    case ICO_CUERVO: /* negro brillante, con un reflejo azul */
        elipse(l, 5.6f, 7.2f, 3.4f, 2.6f, A(0x24202E)); circulo(l, 8.6f, 4.8f, 2.0f, A(0x24202E));
        triangulo(l, 10.2f, 4.2f, 10.2f, 5.8f, 12.0f, 5.2f, A(0x3A3448)); lz_set(l, 9, 4, A(0xFFFFFF));
        elipse(l, 4.8f, 6.4f, 2.0f, 0.8f, A(0x3A4A7A)); triangulo(l, 2.6f, 6.6f, 2.6f, 8.6f, 0.0f, 8.4f, A(0x24202E));
        rect(l, 5, 10, 1, 2, A(0x4A4458)); rect(l, 7, 10, 1, 2, A(0x4A4458));
        break;
    case ICO_GARZA: /* gris celeste, en una pata, de cuello largo */
        elipse(l, 4.8f, 5.6f, 2.8f, 1.8f, A(0xC8D4E8)); trazo(l, 6.8f, 5.0f, 8.0f, 2.2f, 0.5f, A(0xC8D4E8)); circulo(l, 8.2f, 1.8f, 1.1f, A(0xE6ECF6));
        trazo(l, 9.0f, 1.8f, 11.8f, 2.4f, 0.3f, A(0xF2C33A)); lz_set(l, 8, 1, K); trazo(l, 7.6f, 1.2f, 5.6f, 0.6f, 0.2f, A(0x4A4458));
        trazo(l, 4.8f, 7.2f, 4.8f, 11.0f, 0.25f, A(0xF2C33A)); rect(l, 1, 11, 10, 1, A(0x9ED8FF));
        break;
    case ICO_MAPACHE: /* gris con antifaz negro y cola a rayas */
        elipse(l, 5.6f, 8.0f, 3.6f, 2.6f, A(0x8A8A9A)); circulo(l, 8.6f, 5.6f, 2.4f, A(0xA8A8B8));
        lz_set(l, 7, 3, A(0x6A6A7A)); lz_set(l, 10, 3, A(0x6A6A7A)); rect(l, 7, 5, 4, 1, A(0x2A2238)); lz_set(l, 9, 5, A(0xFFFFFF));
        lz_set(l, 11, 6, K); elipse(l, 9.4f, 7.0f, 1.2f, 0.6f, A(0xE6E6EE));
        trazo(l, 2.4f, 8.0f, 0.4f, 5.0f, 0.9f, A(0x8A8A9A)); lz_set(l, 1, 6, A(0x3A3448)); lz_set(l, 0, 5, A(0x3A3448)); lz_set(l, 2, 7, A(0x3A3448));
        break;
    case ICO_MURCIELAGO: /* alas abiertas, orejitas y ojos chiquitos */
        circulo(l, 6.0f, 6.0f, 1.8f, A(0x5A4A70)); lz_set(l, 5, 3, A(0x5A4A70)); lz_set(l, 7, 3, A(0x5A4A70));
        triangulo(l, 4.6f, 5.2f, 0.0f, 3.2f, 1.6f, 8.6f, A(0x6A5A84)); triangulo(l, 7.4f, 5.2f, 12.0f, 3.2f, 10.4f, 8.6f, A(0x6A5A84));
        triangulo(l, 1.6f, 8.6f, 3.0f, 6.0f, 4.2f, 8.0f, TRANSP); triangulo(l, 10.4f, 8.6f, 9.0f, 6.0f, 7.8f, 8.0f, TRANSP);
        lz_set(l, 5, 6, A(0xFFE066)); lz_set(l, 6, 6, A(0xFFE066));
        break;
    case ICO_NUTRIA: /* flotando de espaldas, con las patitas sobre la panza */
        elipse(l, 6.0f, 7.6f, 5.0f, 2.0f, A(0x8A5A3A)); elipse(l, 6.0f, 7.0f, 3.2f, 1.2f, A(0xC89A70));
        circulo(l, 10.2f, 6.4f, 1.6f, A(0x9A6A4A)); lz_set(l, 10, 6, K); lz_set(l, 11, 7, A(0x4A3428));
        lz_set(l, 6, 6, A(0x6A4A30)); lz_set(l, 7, 6, A(0x6A4A30)); rect(l, 0, 9, 12, 1, A(0x9ED8FF)); lz_set(l, 3, 10, A(0x9ED8FF));
        break;
    case ICO_PINGUINO: /* de frac: espalda negra, panza blanca, pico y patas naranjas */
        elipse(l, 6.0f, 6.8f, 3.2f, 4.2f, A(0x2A3248)); elipse(l, 6.4f, 7.4f, 2.0f, 3.0f, A(0xF8F8FF));
        circulo(l, 6.0f, 2.8f, 2.0f, A(0x2A3248)); rect(l, 7, 3, 3, 1, A(0xF2A33A)); lz_set(l, 6, 2, A(0xFFFFFF)); lz_set(l, 7, 2, K);
        triangulo(l, 3.0f, 5.0f, 3.0f, 8.6f, 1.0f, 8.4f, A(0x3A4260)); triangulo(l, 9.0f, 5.0f, 9.0f, 8.6f, 11.0f, 8.4f, A(0x3A4260));
        rect(l, 4, 11, 2, 1, A(0xF2A33A)); rect(l, 7, 11, 2, 1, A(0xF2A33A));
        break;
    case ICO_FOCA: /* gris plateada, tendida, de bigotes largos */
        elipse(l, 5.4f, 8.0f, 4.8f, 2.4f, A(0x9AA4B8)); elipse(l, 5.4f, 8.8f, 3.4f, 1.2f, A(0xD8DEEA));
        circulo(l, 9.8f, 6.0f, 1.9f, A(0xAEB8CC)); lz_set(l, 10, 5, K); lz_set(l, 11, 6, A(0x2A2238));
        trazo(l, 10.6f, 6.6f, 12.0f, 6.2f, 0.15f, A(0xF4F8FF)); trazo(l, 10.6f, 6.9f, 12.0f, 7.4f, 0.15f, A(0xF4F8FF));
        triangulo(l, 1.0f, 7.0f, 0.0f, 10.0f, 2.4f, 9.4f, A(0x7A869E));
        break;
    case ICO_OSO_POLAR: /* oso blanco de perfil: cuerpo largo y bajo, cabeza con hocico largo y negro, orejitas chicas */
        elipse(l, 4.8f, 7.4f, 4.6f, 2.8f, A(0xF4F7FC)); elipse(l, 4.8f, 8.8f, 4.0f, 1.2f, A(0xCBD8EA)); /* cuerpo y sombra */
        rect(l, 1, 9, 2, 3, A(0xE6EDF8)); rect(l, 5, 9, 2, 3, A(0xE6EDF8)); rect(l, 1, 11, 2, 1, A(0x7C8FAE)); rect(l, 5, 11, 2, 1, A(0x7C8FAE)); /* patas */
        circulo(l, 8.4f, 5.2f, 2.2f, A(0xFFFFFF)); /* cabeza */
        elipse(l, 10.4f, 6.2f, 1.6f, 1.2f, A(0xEEF2FA)); /* hocico largo */
        lz_set(l, 11, 5, A(0x24202E)); lz_set(l, 11, 6, A(0x24202E)); lz_set(l, 10, 7, A(0x6A7488)); /* nariz y boca */
        lz_set(l, 9, 4, A(0x24202E)); /* ojo */
        lz_set(l, 7, 2, A(0xDDE6F2)); lz_set(l, 8, 2, A(0xDDE6F2)); lz_set(l, 10, 2, A(0xDDE6F2)); /* orejas */
        lz_set(l, 3, 5, A(0xFFFFFF)); lz_set(l, 4, 5, A(0xFFFFFF)); lz_set(l, 5, 5, A(0xFFFFFF)); /* lomo brillante */
        break;
    case ICO_CASTOR: /* café, con dientes grandes y la cola de paleta */
        elipse(l, 5.6f, 8.0f, 3.4f, 2.6f, A(0x8A5A30)); circulo(l, 8.4f, 5.4f, 2.2f, A(0x9A6A3A));
        lz_set(l, 9, 4, K); rect(l, 10, 6, 1, 2, A(0xFFFFFF)); lz_set(l, 10, 5, A(0x4A3428)); lz_set(l, 7, 3, A(0x7A4A28));
        elipse(l, 1.6f, 9.6f, 1.6f, 1.0f, A(0x5A4A3A)); lz_set(l, 1, 9, A(0x7A6A5A));
        break;
    case ICO_RATON: /* chiquito, de orejas redondas y bigotes */
        elipse(l, 5.6f, 8.4f, 3.0f, 2.0f, A(0xB0A090)); circulo(l, 8.4f, 7.0f, 1.8f, A(0xC0B0A0));
        circulo(l, 7.6f, 4.8f, 1.2f, A(0xE6B8C0)); lz_set(l, 9, 7, K); lz_set(l, 10, 7, A(0xFF9AB4));
        trazo(l, 10.0f, 7.6f, 11.8f, 7.0f, 0.15f, A(0x6A6A80)); trazo(l, 10.0f, 8.0f, 11.8f, 8.6f, 0.15f, A(0x6A6A80));
        trazo(l, 2.8f, 9.0f, 0.4f, 10.6f, 0.2f, A(0xE6B8C0));
        break;
    case ICO_OSO: /* grande, café, de orejas redondas */
        elipse(l, 5.6f, 7.8f, 4.6f, 3.4f, A(0x7A4A2A)); circulo(l, 8.6f, 4.4f, 2.8f, A(0x8A5A34));
        circulo(l, 7.0f, 1.8f, 0.9f, A(0x8A5A34)); circulo(l, 10.0f, 2.0f, 0.9f, A(0x8A5A34));
        elipse(l, 10.2f, 5.2f, 1.4f, 1.0f, A(0xC89A70)); lz_set(l, 11, 5, K); lz_set(l, 9, 3, K);
        rect(l, 2, 10, 2, 2, A(0x6A3A1E)); rect(l, 7, 10, 2, 2, A(0x6A3A1E));
        break;
    case ICO_CIERVO_CAFE: /* un ciervo café con manchitas y cuernos de rama */
        elipse(l, 5.0f, 7.0f, 3.6f, 2.0f, A(0xB07840)); circulo(l, 9.0f, 4.6f, 1.8f, A(0xB07840)); lz_set(l, 4, 6, A(0xF2D8B0)); lz_set(l, 6, 7, A(0xF2D8B0));
        rect(l, 2, 8, 1, 4, A(0x8A5A30)); rect(l, 7, 8, 1, 4, A(0x8A5A30)); rect(l, 4, 9, 1, 3, A(0x8A5A30));
        trazo(l, 8.4f, 3.0f, 7.0f, 0.4f, 0.3f, A(0x6A4A30)); trazo(l, 9.8f, 3.0f, 11.0f, 0.4f, 0.3f, A(0x6A4A30)); lz_set(l, 7, 1, A(0x6A4A30));
        lz_set(l, 10, 4, K); lz_set(l, 11, 5, A(0x2A2238));
        break;
    case ICO_TEJON: /* gris con la cara de rayas blancas y negras */
        elipse(l, 5.6f, 8.2f, 4.0f, 2.4f, A(0x7A7486)); circulo(l, 9.2f, 7.0f, 2.0f, A(0xF4F4F8));
        trazo(l, 8.0f, 6.0f, 11.0f, 7.4f, 0.5f, A(0x2A2238)); lz_set(l, 11, 8, K); lz_set(l, 10, 6, A(0x2A2238));
        rect(l, 3, 10, 1, 2, A(0x4A4458)); rect(l, 7, 10, 1, 2, A(0x4A4458));
        break;
    case ICO_ZORRILLO: /* negro con la raya blanca y la cola esponjosa levantada */
        elipse(l, 6.4f, 8.6f, 3.2f, 2.0f, A(0x24202E)); circulo(l, 9.6f, 8.0f, 1.6f, A(0x24202E));
        elipse(l, 3.0f, 4.8f, 2.6f, 3.8f, A(0x24202E)); trazo(l, 2.6f, 2.0f, 8.0f, 7.8f, 0.6f, A(0xF8F8FF)); lz_set(l, 10, 8, A(0xFFFFFF));
        lz_set(l, 11, 8, A(0xFF9AB4));
        break;
    case ICO_GATO_MONTES: /* un gato salvaje gris con rayas y orejas en punta */
        elipse(l, 5.4f, 8.0f, 3.6f, 2.2f, A(0x9A8A7A)); circulo(l, 8.8f, 5.6f, 2.2f, A(0xA89888));
        triangulo(l, 7.0f, 4.2f, 8.4f, 4.0f, 7.4f, 1.8f, A(0xA89888)); triangulo(l, 9.4f, 4.0f, 10.8f, 4.4f, 10.4f, 2.0f, A(0xA89888));
        lz_set(l, 8, 5, A(0xC8E060)); lz_set(l, 10, 5, A(0xC8E060)); lz_set(l, 9, 7, A(0xFF9AB4));
        lz_set(l, 4, 7, A(0x6A5A4A)); lz_set(l, 6, 7, A(0x6A5A4A)); trazo(l, 2.0f, 8.0f, 0.4f, 5.0f, 0.5f, A(0x9A8A7A));
        break;
    case ICO_LINCE: /* dorado con manchitas, orejas de pincel y patillas */
        elipse(l, 5.4f, 8.0f, 3.6f, 2.2f, A(0xD8A860)); circulo(l, 8.8f, 5.6f, 2.3f, A(0xE0B470));
        triangulo(l, 7.0f, 4.2f, 8.4f, 4.0f, 7.2f, 1.6f, A(0xE0B470)); triangulo(l, 9.4f, 4.0f, 10.8f, 4.4f, 10.6f, 1.6f, A(0xE0B470));
        lz_set(l, 7, 1, K); lz_set(l, 10, 1, K); lz_set(l, 8, 5, A(0x8A6A1E)); lz_set(l, 10, 5, A(0x8A6A1E));
        triangulo(l, 7.0f, 6.6f, 6.0f, 8.4f, 8.0f, 7.4f, A(0xF6E8D0)); lz_set(l, 4, 7, A(0x8A6440)); lz_set(l, 6, 9, A(0x8A6440));
        rect(l, 1, 7, 2, 1, A(0x2A2238));
        break;
    case ICO_PUMA: /* grande, color arena, de cola larga */
        elipse(l, 5.6f, 7.6f, 4.0f, 2.2f, A(0xC89A60)); circulo(l, 9.4f, 5.4f, 2.0f, A(0xD0A468)); lz_set(l, 8, 3, A(0xD0A468)); lz_set(l, 10, 3, A(0xD0A468));
        elipse(l, 10.4f, 6.4f, 1.2f, 0.8f, A(0xF6E8D0)); lz_set(l, 10, 5, A(0x5A4A20)); lz_set(l, 11, 6, A(0x2A2238));
        rect(l, 3, 9, 1, 3, A(0xB08850)); rect(l, 7, 9, 1, 3, A(0xB08850)); trazo(l, 1.8f, 7.6f, 0.4f, 11.0f, 0.4f, A(0xC89A60)); lz_set(l, 0, 11, A(0x6A4A30));
        break;
    case ICO_LAGARTIJA: /* verde café, flaquita, con la cola larga, tomando sol */
        elipse(l, 6.0f, 7.4f, 3.0f, 1.2f, A(0x8AA04A)); circulo(l, 9.2f, 7.0f, 1.2f, A(0x8AA04A)); lz_set(l, 9, 6, K);
        trazo(l, 3.2f, 7.6f, 0.2f, 9.6f, 0.4f, A(0x8AA04A)); lz_set(l, 4, 9, A(0x6A803A)); lz_set(l, 7, 9, A(0x6A803A)); lz_set(l, 5, 5, A(0x6A803A)); lz_set(l, 8, 5, A(0x6A803A));
        elipse(l, 6.0f, 10.4f, 4.6f, 1.2f, A(0xA8A0B8));
        break;
    case ICO_SALAMANDRA: /* naranja con puntitos */
        elipse(l, 6.0f, 7.6f, 3.4f, 1.4f, A(0xF2843A)); circulo(l, 9.6f, 7.2f, 1.4f, A(0xF2843A)); lz_set(l, 10, 6, K);
        trazo(l, 2.8f, 7.8f, 0.2f, 6.4f, 0.5f, A(0xF2843A)); lz_set(l, 4, 9, A(0xE06A2A)); lz_set(l, 8, 9, A(0xE06A2A)); lz_set(l, 4, 6, A(0xE06A2A)); lz_set(l, 8, 6, A(0xE06A2A));
        lz_set(l, 5, 7, A(0xFFE066)); lz_set(l, 7, 8, A(0xFFE066)); lz_set(l, 3, 7, A(0xFFE066));
        break;
    case ICO_CAMALEON: /* verde, con la cola enroscada y el ojo grande */
        elipse(l, 6.0f, 6.6f, 3.4f, 2.4f, A(0x5AC060)); triangulo(l, 8.4f, 5.0f, 8.4f, 8.0f, 11.4f, 6.6f, A(0x5AC060));
        circulo(l, 8.6f, 5.6f, 1.0f, A(0xC8F0A0)); lz_set(l, 8, 5, K);
        circulo(l, 2.6f, 9.0f, 1.6f, A(0x5AC060)); circulo(l, 2.6f, 9.0f, 0.7f, TRANSP);
        rect(l, 0, 10, 12, 1, A(0x8A6440)); lz_set(l, 5, 9, A(0x4AA050)); lz_set(l, 7, 9, A(0x4AA050)); lz_set(l, 5, 5, A(0xFFE066)); lz_set(l, 6, 7, A(0xFFE066));
        break;
    case ICO_CULEBRA: /* una culebrita verde, ondulada, de cara simpática */
        for (int k = 0; k < 8; k++) circulo(l, 1.4f + k * 1.2f, 8.0f + sinf((float)k * 1.0f) * 1.6f, 1.0f, A(0x6ACC5A));
        circulo(l, 10.4f, 6.4f, 1.4f, A(0x7ADA6A)); lz_set(l, 10, 6, K); trazo(l, 11.4f, 7.0f, 12.0f, 7.4f, 0.15f, A(0xF0506E));
        break;
    case ICO_MARIQUITA: /* roja con puntitos negros */
        circulo(l, 6.0f, 7.0f, 3.8f, A(0xF0403E)); circulo(l, 6.0f, 3.2f, 1.6f, A(0x2A2238)); rect(l, 6, 3, 1, 8, A(0x2A2238));
        lz_set(l, 4, 6, A(0x2A2238)); lz_set(l, 8, 6, A(0x2A2238)); lz_set(l, 4, 9, A(0x2A2238)); lz_set(l, 8, 9, A(0x2A2238));
        lz_set(l, 5, 2, A(0xFFFFFF)); lz_set(l, 4, 5, A(0xFF8A8A));
        break;
    case ICO_LIBELULA: /* cuerpo largo azul y cuatro alas de vidrio */
        elipse(l, 4.0f, 4.0f, 3.2f, 1.0f, A(0xD6F0FF)); elipse(l, 8.0f, 4.0f, 3.2f, 1.0f, A(0xD6F0FF));
        elipse(l, 4.2f, 6.4f, 2.8f, 0.9f, A(0xC6E6FA)); elipse(l, 7.8f, 6.4f, 2.8f, 0.9f, A(0xC6E6FA));
        rect(l, 6, 3, 1, 9, A(0x3A8AE0)); circulo(l, 6.0f, 2.4f, 1.0f, A(0x4AA0F0)); lz_set(l, 5, 2, A(0x2A2238)); lz_set(l, 7, 2, A(0x2A2238));
        break;
    case ICO_GRILLO: /* café verdoso, patas largas y antenas */
        elipse(l, 5.6f, 7.6f, 3.4f, 1.6f, A(0x7A8A3A)); circulo(l, 8.8f, 7.0f, 1.4f, A(0x8A9A4A)); lz_set(l, 9, 6, K);
        trazo(l, 9.4f, 5.8f, 11.8f, 2.0f, 0.15f, A(0x5A6A2A)); trazo(l, 9.0f, 5.8f, 10.0f, 1.6f, 0.15f, A(0x5A6A2A));
        trazo(l, 4.0f, 7.0f, 2.0f, 4.4f, 0.3f, A(0x6A7A30)); trazo(l, 2.0f, 4.4f, 1.2f, 10.0f, 0.3f, A(0x6A7A30)); lz_set(l, 6, 9, A(0x5A6A2A));
        break;
    case ICO_HORMIGAS: /* tres hormiguitas en fila, la primera con una miga */
        for (int k = 0; k < 3; k++) {
            float x = 1.6f + k * 3.6f;
            circulo(l, x, 8.0f, 0.9f, A(0x5A2A2A)); circulo(l, x + 1.4f, 7.8f, 0.7f, A(0x6A3A3A)); circulo(l, x + 2.4f, 7.6f, 0.7f, A(0x5A2A2A));
            lz_set(l, (int)x, 9, A(0x5A2A2A)); lz_set(l, (int)(x + 2), 9, A(0x5A2A2A));
        }
        elipse(l, 10.4f, 5.4f, 1.6f, 1.2f, A(0xF2D8A0)); lz_set(l, 10, 5, A(0xE0B870));
        break;
    case ICO_ESCARABAJO: /* verde tornasol con la armadura brillante */
        elipse(l, 6.0f, 7.0f, 3.4f, 3.6f, A(0x2A9A6A)); rect(l, 6, 4, 1, 7, A(0x1A6A4A)); elipse(l, 6.0f, 3.0f, 1.6f, 1.2f, A(0x2A2238));
        lz_set(l, 4, 5, A(0x8AF0C8)); lz_set(l, 5, 6, A(0x6AD0E8)); lz_set(l, 2, 7, A(0x2A2238)); lz_set(l, 9, 7, A(0x2A2238)); lz_set(l, 2, 9, A(0x2A2238)); lz_set(l, 9, 9, A(0x2A2238));
        trazo(l, 5.4f, 2.0f, 4.0f, 0.6f, 0.15f, A(0x2A2238)); trazo(l, 6.6f, 2.0f, 8.0f, 0.6f, 0.15f, A(0x2A2238));
        break;
    case ICO_POLILLA: /* alas suaves color crema con ojitos dibujados */
        elipse(l, 3.6f, 5.6f, 3.0f, 3.2f, A(0xE8DCC0)); elipse(l, 8.4f, 5.6f, 3.0f, 3.2f, A(0xE8DCC0));
        circulo(l, 3.4f, 5.4f, 1.0f, A(0xA08AC0)); circulo(l, 8.6f, 5.4f, 1.0f, A(0xA08AC0)); lz_set(l, 3, 5, A(0xFFF6D0)); lz_set(l, 8, 5, A(0xFFF6D0));
        rect(l, 6, 3, 1, 6, A(0x8A7A60)); trazo(l, 6.0f, 3.0f, 4.0f, 0.6f, 0.2f, A(0x8A7A60)); trazo(l, 6.4f, 3.0f, 8.4f, 0.6f, 0.2f, A(0x8A7A60));
        break;
    /* --- las criaturas mágicas --- */
    case ICO_MUSGUITO: /* un duendecito redondo de musgo, con un hongo de sombrero */
        circulo(l, 6.0f, 8.0f, 3.6f, A(0x6AB04A)); circulo(l, 4.6f, 7.0f, 1.0f, A(0x8ACC60)); lz_set(l, 7, 9, A(0x5A9A3A)); lz_set(l, 3, 9, A(0x5A9A3A));
        elipse(l, 6.0f, 4.0f, 4.0f, 2.0f, A(0xF0506E)); rect(l, 2, 4, 8, 1, A(0xD83A58)); lz_set(l, 4, 3, A(0xFFFFFF)); lz_set(l, 8, 3, A(0xFFFFFF));
        lz_set(l, 5, 7, K); lz_set(l, 7, 7, K); lz_set(l, 6, 9, A(0x3A6A2A)); lz_set(l, 4, 8, A(0xFF9AB4)); lz_set(l, 8, 8, A(0xFF9AB4));
        break;
    case ICO_LUMBRE: /* un hada de luz: una gotita dorada con alitas y un halo */
        elipse(l, 3.0f, 4.4f, 2.4f, 1.6f, A(0xD8F4FF)); elipse(l, 9.0f, 4.4f, 2.4f, 1.6f, A(0xD8F4FF));
        circulo(l, 6.0f, 6.4f, 2.4f, A(0xFFE27A)); circulo(l, 6.0f, 6.4f, 1.4f, A(0xFFF6D0));
        lz_set(l, 5, 6, K); lz_set(l, 7, 6, K); lz_set(l, 6, 7, A(0xF2843A)); lz_set(l, 6, 10, A(0xFFE27A)); lz_set(l, 6, 11, A(0xFFF6D0));
        break;
    case ICO_ONDINA: /* un espíritu de agua: una gota grande, transparente, con una ola de pelo */
        circulo(l, 6.0f, 7.4f, 3.8f, A(0x8FD8FF)); triangulo(l, 2.6f, 6.0f, 9.4f, 6.0f, 6.0f, 0.6f, A(0x8FD8FF));
        elipse(l, 4.6f, 5.6f, 1.0f, 1.6f, A(0xD8F4FF)); trazo(l, 3.0f, 3.6f, 1.0f, 5.6f, 0.5f, A(0x4FB0F0)); trazo(l, 9.0f, 3.6f, 11.0f, 5.6f, 0.5f, A(0x4FB0F0));
        lz_set(l, 5, 8, A(0x1A4A7A)); lz_set(l, 7, 8, A(0x1A4A7A)); lz_set(l, 6, 9, A(0xFF8FB0)); lz_set(l, 4, 10, A(0xFFFFFF));
        break;
    case ICO_VENTOLIN: /* un dragoncito celeste con alitas y un remolino de aire */
        elipse(l, 5.6f, 7.4f, 3.0f, 2.4f, A(0x9ED0F0)); circulo(l, 8.6f, 4.8f, 2.0f, A(0xAEDCF8)); elipse(l, 5.6f, 8.2f, 1.8f, 1.2f, A(0xE6F6FF));
        triangulo(l, 4.0f, 5.6f, 6.4f, 5.4f, 3.0f, 1.6f, A(0xC8B0F0)); lz_set(l, 9, 4, K); lz_set(l, 8, 2, A(0xFFE27A)); lz_set(l, 10, 3, A(0xFFE27A));
        trazo(l, 2.8f, 8.0f, 0.4f, 9.6f, 0.5f, A(0x9ED0F0)); trazo(l, 10.6f, 5.6f, 11.8f, 7.0f, 0.2f, A(0xFFFFFF)); trazo(l, 11.0f, 4.6f, 12.0f, 4.6f, 0.2f, A(0xFFFFFF));
        break;
    case ICO_CRISTALINO: /* un gnomo de cristal: el gorro es un cristal que brilla */
        circulo(l, 6.0f, 8.4f, 3.0f, A(0xB8C8E8)); elipse(l, 6.0f, 10.2f, 2.0f, 1.2f, A(0xF4F4F8));
        triangulo(l, 6.0f, 0.4f, 2.8f, 6.0f, 9.2f, 6.0f, A(0xA080F0)); triangulo(l, 6.0f, 0.4f, 6.0f, 6.0f, 9.2f, 6.0f, A(0x7FE0D6)); lz_set(l, 5, 3, A(0xFFFFFF));
        lz_set(l, 5, 8, K); lz_set(l, 7, 8, K); lz_set(l, 6, 9, A(0xFF9AB4));
        break;
    case ICO_MAREA: /* una sirenita con cola de pez y una estrellita en el pelo */
        circulo(l, 5.0f, 3.6f, 2.2f, A(0xFFD8B0)); elipse(l, 4.0f, 3.0f, 2.6f, 2.0f, A(0xF2843A)); circulo(l, 5.6f, 4.0f, 1.4f, A(0xFFD8B0));
        lz_set(l, 6, 4, K); lz_set(l, 2, 2, A(0xFFE066));
        elipse(l, 5.4f, 7.4f, 1.6f, 2.0f, A(0x4FC0B6)); triangulo(l, 5.0f, 8.6f, 7.4f, 8.6f, 9.4f, 11.2f, A(0x3FA8A0)); triangulo(l, 9.4f, 11.2f, 11.8f, 9.6f, 11.4f, 12.0f, A(0x3FA8A0));
        lz_set(l, 5, 7, A(0xA0F0E8));
        break;
    /* --- mensajeros y cosas de las citas --- */
    case ICO_PALOMA:
    case ICO_PALOMA_B: /* blanca, con un sobrecito en la patita */
        elipse(l, 6.0f, 7.0f, 3.4f, 2.2f, A(0xF4F4F8)); circulo(l, 9.0f, 5.0f, 1.8f, A(0xF4F4F8)); lz_set(l, 9, 4, K);
        triangulo(l, 10.4f, 4.6f, 10.4f, 5.6f, 11.8f, 5.2f, A(0xF2A33A)); triangulo(l, 2.8f, 6.4f, 2.8f, 8.0f, 0.2f, 7.6f, A(0xDCDCE6));
        if (i == ICO_PALOMA) triangulo(l, 4.0f, 6.0f, 8.0f, 6.2f, 4.6f, 0.6f, A(0xE6E6F2));
        else triangulo(l, 4.0f, 7.0f, 8.0f, 7.0f, 4.8f, 11.0f, A(0xE6E6F2));
        rect(l, 7, 9, 3, 2, A(0xFFF1D6)); lz_set(l, 8, 10, A(0xD83A3A));
        break;
    case ICO_GRULLA: /* una grulla de papel doblado */
        triangulo(l, 1.0f, 8.0f, 11.0f, 8.0f, 6.0f, 5.0f, A(0xF6E2C8)); triangulo(l, 6.0f, 5.0f, 2.0f, 1.4f, 6.4f, 7.6f, A(0xFFF6E6));
        triangulo(l, 6.0f, 5.0f, 10.6f, 2.0f, 6.0f, 7.6f, A(0xEAD2B0)); triangulo(l, 10.4f, 8.0f, 11.8f, 3.6f, 9.2f, 7.4f, A(0xF6E2C8));
        lz_set(l, 11, 3, A(0xEAD2B0)); trazo(l, 1.0f, 8.0f, 11.0f, 8.0f, 0.15f, A(0xC8A880));
        break;
    case ICO_GRULLA_ABRE: /* la grulla se desdobla: ya casi es un papel */
        triangulo(l, 1.0f, 9.0f, 11.0f, 9.0f, 6.0f, 2.0f, A(0xFFF6E6)); triangulo(l, 1.0f, 9.0f, 6.0f, 2.0f, 1.4f, 3.4f, A(0xF6E2C8));
        triangulo(l, 11.0f, 9.0f, 6.0f, 2.0f, 10.6f, 3.4f, A(0xEAD2B0));
        trazo(l, 6.0f, 2.0f, 6.0f, 9.0f, 0.15f, A(0xC8A880)); trazo(l, 3.0f, 6.0f, 9.0f, 6.0f, 0.15f, A(0xD8C0A0));
        break;
    case ICO_MANTEL: /* un mantel a cuadros con una canastita */
        for (int y = 0; y < 3; y++) for (int x = 0; x < 6; x++) rect(l, x * 2, 7 + y * 2 - (x + y) % 2 * 0, 2, 2, (x + y) % 2 ? A(0xF4F4F8) : A(0xF0506E));
        rect(l, 3, 4, 6, 3, A(0xC8964A)); trazo(l, 3.6f, 4.0f, 6.0f, 1.6f, 0.3f, A(0xA8762A)); trazo(l, 6.0f, 1.6f, 8.4f, 4.0f, 0.3f, A(0xA8762A));
        lz_set(l, 4, 4, A(0xF0403E)); lz_set(l, 7, 4, A(0x6ACC5A));
        break;
    case ICO_BARQUITO_HOJA: /* una hoja que navega con su velita de pétalo */
        elipse(l, 6.0f, 9.0f, 5.4f, 1.8f, A(0x5AB04A)); rect(l, 0, 7, 12, 2, TRANSP); elipse(l, 6.0f, 8.8f, 5.0f, 0.6f, A(0x4A9A3A));
        trazo(l, 1.2f, 8.6f, 10.8f, 8.6f, 0.15f, A(0x3A7A2A)); rect(l, 6, 2, 1, 7, A(0x8A6440));
        triangulo(l, 6.8f, 2.0f, 6.8f, 7.4f, 10.4f, 6.4f, A(0xFFB3C8)); rect(l, 0, 11, 12, 1, A(0x9ED8FF));
        break;
    case ICO_FARO: /* un faro blanco con rayas rojas y la luz arriba */
        triangulo(l, 3.6f, 11.6f, 8.4f, 11.6f, 6.0f, 2.0f, TRANSP); rect(l, 4, 4, 4, 8, A(0xF4F4F8)); rect(l, 4, 6, 4, 2, A(0xF0403E)); rect(l, 4, 10, 4, 2, A(0xF0403E));
        rect(l, 4, 2, 4, 2, A(0xFFE066)); rect(l, 3, 1, 6, 1, A(0x4A4458)); triangulo(l, 3.4f, 1.0f, 8.6f, 1.0f, 6.0f, -0.4f, A(0x4A4458));
        lz_set(l, 5, 2, A(0xFFFFFF));
        break;
    case ICO_COMETA: /* una cometa en rombo con su cola de lacitos */
        triangulo(l, 6.0f, 0.4f, 1.6f, 4.6f, 6.0f, 4.6f, A(0xF0506E)); triangulo(l, 6.0f, 0.4f, 10.4f, 4.6f, 6.0f, 4.6f, A(0xFFD45C));
        triangulo(l, 1.6f, 4.6f, 6.0f, 9.0f, 6.0f, 4.6f, A(0x5AB0F0)); triangulo(l, 10.4f, 4.6f, 6.0f, 9.0f, 6.0f, 4.6f, A(0x6ACC5A));
        trazo(l, 6.0f, 9.0f, 4.0f, 11.8f, 0.15f, A(0x6A6A80)); lz_set(l, 5, 10, A(0xF0506E)); lz_set(l, 4, 11, A(0xFFD45C));
        break;
    case ICO_HONGO_LUZ: /* un hongo azulado que brilla de noche */
        elipse(l, 6.0f, 5.0f, 4.4f, 3.0f, A(0x8FE8FF)); rect(l, 0, 6, 12, 3, TRANSP); elipse(l, 6.0f, 5.6f, 4.4f, 0.9f, A(0x6AD0F0));
        rect(l, 5, 6, 2, 5, A(0xE6FAFF)); lz_set(l, 4, 3, A(0xFFFFFF)); lz_set(l, 7, 4, A(0xFFFFFF));
        break;
    case ICO_HADA: /* un hada chiquitita: un brillito con alas */
        elipse(l, 3.6f, 5.0f, 2.6f, 1.8f, A(0xE6D8FF)); elipse(l, 8.4f, 5.0f, 2.6f, 1.8f, A(0xE6D8FF));
        circulo(l, 6.0f, 6.0f, 1.8f, A(0xFFF6D0)); circulo(l, 6.0f, 3.4f, 1.2f, A(0xFFE0C0));
        lz_set(l, 6, 9, A(0xFFE27A)); lz_set(l, 5, 10, A(0xFFF6D0)); lz_set(l, 7, 11, A(0xFFF6D0)); lz_set(l, 2, 2, A(0xFFFFFF)); lz_set(l, 10, 8, A(0xFFFFFF));
        break;
    case ICO_EMPANADA: /* una empanada dorada con su repulgue */
        elipse(l, 6.0f, 7.4f, 5.4f, 3.4f, A(0xE8A84A)); rect(l, 0, 7, 12, 5, TRANSP); elipse(l, 6.0f, 7.4f, 5.4f, 1.6f, A(0xE8A84A));
        elipse(l, 6.0f, 6.4f, 4.6f, 2.4f, A(0xF2C46A));
        for (int k = 0; k < 6; k++) circulo(l, 1.6f + (float)k * 1.75f, 8.4f - sinf((float)k * 0.6f) * 0.3f, 0.9f, A(0xC8862A));
        lz_set(l, 4, 5, A(0xFFE6A0)); lz_set(l, 7, 4, A(0xFFE6A0));
        break;
    case ICO_VOLANTIN: /* un volantín chileno: rombo rojo, blanco y azul, con su cola */
        triangulo(l, 6.0f, 0.4f, 1.6f, 4.8f, 10.4f, 4.8f, A(0xD52B1E));
        triangulo(l, 1.6f, 4.8f, 10.4f, 4.8f, 6.0f, 9.2f, A(0x0039A6));
        circulo(l, 6.0f, 4.8f, 1.4f, A(0xFFFFFF));
        trazo(l, 6.0f, 0.4f, 6.0f, 9.2f, 0.12f, A(0x2A2238)); trazo(l, 1.6f, 4.8f, 10.4f, 4.8f, 0.12f, A(0x2A2238));
        trazo(l, 6.0f, 9.2f, 4.2f, 11.8f, 0.15f, A(0x6A6A80)); lz_set(l, 5, 10, A(0xD52B1E)); lz_set(l, 4, 11, A(0xFFFFFF));
        break;
    case ICO_GORRO_BRUJA: /* un sombrero de bruja con estrellita (disfraz de Halloween) */
        elipse(l, 6.0f, 9.6f, 5.6f, 1.6f, A(0x4A3A7A)); triangulo(l, 2.6f, 9.4f, 9.4f, 9.4f, 7.6f, 0.6f, A(0x5A4A90));
        rect(l, 3, 7, 6, 1, A(0xF2A33A)); lz_set(l, 6, 4, A(0xFFE066));
        break;
    case ICO_FANTASMA: /* un fantasmita de sábana, sonriente (disfraz de Halloween) */
        elipse(l, 6.0f, 5.4f, 4.2f, 4.6f, A(0xF8F8FF)); rect(l, 2, 5, 9, 5, A(0xF8F8FF));
        for (int k = 0; k < 3; k++) circulo(l, 3.0f + (float)k * 3.0f, 10.0f, 1.4f, A(0xF8F8FF));
        lz_set(l, 4, 5, K); lz_set(l, 7, 5, K); lz_set(l, 5, 7, A(0xFF9AB4));
        break;
    case ICO_RAMO_AMARILLO: /* un ramito de flores amarillas con su cinta */
        trazo(l, 6.0f, 11.6f, 4.6f, 6.0f, 0.4f, A(0x4FA84A)); trazo(l, 6.0f, 11.6f, 6.0f, 5.6f, 0.4f, A(0x4FA84A)); trazo(l, 6.0f, 11.6f, 7.6f, 6.0f, 0.4f, A(0x4FA84A));
        for (int k = 0; k < 5; k++) {
            static const float FX[5] = {3.6f, 6.0f, 8.4f, 4.6f, 7.4f}, FY[5] = {4.8f, 3.6f, 4.8f, 2.2f, 2.2f};
            for (int q = 0; q < 5; q++) { float a = (float)q * 1.2566f; circulo(l, FX[k] + cosf(a) * 1.2f, FY[k] + sinf(a) * 1.2f, 0.8f, A(0xFFD43C)); }
            circulo(l, FX[k], FY[k], 0.6f, A(0xF29A2A));
        }
        triangulo(l, 4.6f, 8.4f, 7.4f, 8.4f, 6.0f, 9.8f, A(0xF0506E)); lz_set(l, 6, 9, A(0xFF8FB0));
        break;
    case ICO_GUIRNALDA: /* una guirnalda navideña con lucecitas */
        for (int x = 0; x < 12; x++) { int y = 3 + (int)(sinf((float)x * 0.52f) * 1.6f); rect(l, x, y, 1, 2, A(0x3E8A4A)); lz_set(l, x, y, A(0x5AB060)); }
        circulo(l, 1.5f, 6.4f, 1.0f, A(0xF0403E)); circulo(l, 4.5f, 7.2f, 1.0f, A(0xFFD45C)); circulo(l, 7.5f, 7.2f, 1.0f, A(0x6FB7FF)); circulo(l, 10.5f, 6.4f, 1.0f, A(0xF0403E));
        break;
    case ICO_PASTEL_VELAS: /* el pastel con tres velas apagadas: las llamas van aparte */
        rect(l, 1, 6, 10, 5, A(0xFFB3C8)); rect(l, 1, 10, 10, 1, A(0xE87A9A));
        rect(l, 1, 6, 10, 2, A(0xFFFFFF)); lz_set(l, 2, 8, A(0xFFFFFF)); lz_set(l, 5, 8, A(0xFFFFFF)); lz_set(l, 9, 8, A(0xFFFFFF));
        lz_set(l, 3, 9, A(0xF0506E)); lz_set(l, 7, 9, A(0xF0506E));
        for (int k = 0; k < 3; k++) rect(l, 3 + k * 3, 3, 1, 3, k == 1 ? A(0x8FD0FF) : A(0xFFE066));
        break;
    case ICO_PASTEL:
        rect(l, 1, 6, 10, 5, A(0xFFB3C8)); rect(l, 1, 10, 10, 1, A(0xE87A9A));
        rect(l, 1, 6, 10, 2, A(0xFFFFFF)); lz_set(l, 2, 8, A(0xFFFFFF)); lz_set(l, 5, 8, A(0xFFFFFF)); lz_set(l, 9, 8, A(0xFFFFFF));
        lz_set(l, 3, 9, A(0xF0506E)); lz_set(l, 7, 9, A(0xF0506E)); /* frutillas */
        for (int k = 0; k < 3; k++) { rect(l, 3 + k * 3, 3, 1, 3, k == 1 ? A(0x8FD0FF) : A(0xFFE066)); lz_set(l, 3 + k * 3, 1, A(0xFFB020)); lz_set(l, 3 + k * 3, 2, A(0xFFE27A)); }
        break;
    case ICO_REGALO:
        rect(l, 2, 5, 8, 6, A(0xF0506E)); rect(l, 1, 4, 10, 2, A(0xFF7A90));
        rect(l, 5, 4, 2, 7, A(0xFFD45C));
        elipse(l, 4.0f, 2.6f, 1.8f, 1.3f, A(0xFFD45C)); elipse(l, 8.0f, 2.6f, 1.8f, 1.3f, A(0xFFD45C));
        lz_set(l, 4, 2, A(0xF0506E)); lz_set(l, 8, 2, A(0xF0506E));
        break;
    case ICO_CALENDARIO:
        rect(l, 1, 2, 10, 9, A(0xFFFFFF)); rect(l, 1, 2, 10, 3, A(0xF0506E));
        rect(l, 3, 1, 1, 2, A(0x6A6A80)); rect(l, 8, 1, 1, 2, A(0x6A6A80));
        for (int y = 6; y < 10; y += 2) for (int x = 2; x < 10; x += 2) lz_set(l, x, y, A(0x9A96B0));
        circulo(l, 7.0f, 8.0f, 1.3f, A(0xFFB020));
        break;
    case ICO_BOTE:
        rect(l, 0, 6, 12, 1, A(0xE0A060));
        for (int y = 7; y < 11; y++) rect(l, y - 7, y, 12 - 2 * (y - 7), 1, y == 8 ? A(0xF0F0F0) : A(0xB06A3A));
        rect(l, 5, 5, 2, 1, A(0x8B5E3C));
        trazo(l, 9.0f, 3.0f, 11.5f, 8.5f, 0.35f, A(0x8B5E3C)); /* un remo */
        break;
    case ICO_PEZ:
        elipse(l, 6.5f, 6.0f, 4.0f, 2.6f, A(0xFF9A3C)); triangulo(l, 0.4f, 3.2f, 0.4f, 8.8f, 3.4f, 6.0f, A(0xFF9A3C));
        elipse(l, 6.8f, 7.0f, 2.6f, 1.0f, A(0xFFD48A)); lz_set(l, 9, 5, K); trazo(l, 5.0f, 3.6f, 5.6f, 8.4f, 0.25f, A(0xE06A1A));
        break;
    case ICO_BOTA:
        rect(l, 3, 1, 4, 8, A(0x7A5A44)); rect(l, 3, 7, 8, 3, A(0x7A5A44)); rect(l, 3, 10, 8, 1, A(0x3E2A20));
        rect(l, 3, 1, 4, 1, A(0x9A7A5C)); lz_set(l, 5, 4, A(0x5A4030)); lz_set(l, 9, 8, A(0x5A4030));
        lz_set(l, 1, 4, A(0x7CC8FF)); lz_set(l, 1, 6, A(0x7CC8FF)); lz_set(l, 11, 5, A(0x7CC8FF)); /* gotea */
        break;
    case ICO_CANA:
        trazo(l, 1.0f, 11.0f, 10.0f, 1.0f, 0.45f, A(0x8B5E3C)); rect(l, 2, 8, 2, 2, A(0x5A5A6E));
        trazo(l, 10.0f, 1.0f, 10.5f, 9.0f, 0.15f, A(0xE0E0F0)); circulo(l, 10.5f, 9.5f, 1.2f, A(0xF0506E)); lz_set(l, 10, 10, A(0xFFFFFF));
        break;
    case ICO_GALLETA:
        circulo(l, 6.0f, 6.0f, 4.8f, A(0xD9A562)); circulo(l, 5.5f, 5.5f, 3.6f, A(0xE8BC7A));
        lz_set(l, 4, 4, A(0x5A3A2A)); lz_set(l, 7, 5, A(0x5A3A2A)); lz_set(l, 5, 8, A(0x5A3A2A)); lz_set(l, 8, 8, A(0x5A3A2A));
        break;
    case ICO_MAREO: { /* espiral */
        float a = 0, r = 0.4f;
        for (int k = 0; k < 40; k++, a += 0.45f, r += 0.12f) lz_set(l, (int)(6 + cosf(a) * r), (int)(6 + sinf(a) * r), A(0xB48CFF));
        lz_set(l, 6, 6, A(0xB48CFF));
        break;
    }
    case ICO_CANASTA:
        rect(l, 1, 6, 10, 5, A(0xC08A5A)); rect(l, 0, 5, 12, 2, A(0xD9A562));
        for (int x = 2; x < 11; x += 3) rect(l, x, 7, 1, 4, A(0x8B5E3C));
        rect(l, 1, 9, 10, 1, A(0x8B5E3C));
        trazo(l, 2.0f, 5.0f, 6.0f, 1.0f, 0.4f, A(0x8B5E3C)); trazo(l, 6.0f, 1.0f, 10.0f, 5.0f, 0.4f, A(0x8B5E3C));
        circulo(l, 4.5f, 4.6f, 1.4f, A(0xF0506E)); circulo(l, 7.5f, 4.4f, 1.3f, A(0xFFB020));
        break;
    case ICO_ESCONDIDAS: /* un arbusto con ojitos que espían */
        circulo(l, 4.0f, 7.0f, 3.4f, A(0x4FAE5A)); circulo(l, 8.0f, 7.0f, 3.4f, A(0x4FAE5A)); circulo(l, 6.0f, 4.6f, 3.4f, A(0x5DBB63));
        rect(l, 1, 8, 10, 3, A(0x4FAE5A));
        rect(l, 4, 6, 1, 1, A(0xFFFFFF)); rect(l, 7, 6, 1, 1, A(0xFFFFFF)); lz_set(l, 4, 7, K); lz_set(l, 7, 7, K);
        break;
    case ICO_PATITA: /* huellita */
        elipse(l, 6.0f, 8.6f, 2.8f, 2.0f, A(0xF0A0B8));
        rect(l, 1, 4, 2, 2, A(0xF0A0B8)); rect(l, 4, 1, 2, 2, A(0xF0A0B8));
        rect(l, 7, 1, 2, 2, A(0xF0A0B8)); rect(l, 10, 4, 2, 2, A(0xF0A0B8));
        break;
    case ICO_SENTAR: /* cojín */
        elipse(l, 6.0f, 8.0f, 5.4f, 2.6f, A(0xB48CFF)); elipse(l, 6.0f, 7.2f, 4.6f, 1.6f, A(0xCDB2FF));
        lz_set(l, 6, 8, A(0x8C6AD8)); trazo(l, 6.0f, 1.0f, 6.0f, 4.0f, 0.4f, A(0xFFFFFF)); triangulo(l, 4.0f, 3.6f, 8.0f, 3.6f, 6.0f, 6.0f, A(0xFFFFFF));
        break;
    case ICO_VUELTA: { /* flecha en círculo */
        for (int k = 0; k < 26; k++) { float a = 0.5f + (float)k * 0.21f; circulo(l, 6 + cosf(a) * 4.0f, 6 + sinf(a) * 4.0f, 0.75f, A(0x4DA6FF)); }
        triangulo(l, 8.0f, 0.6f, 8.0f, 5.0f, 11.4f, 2.4f, A(0x4DA6FF));
        break;
    }
    case ICO_SALUDO: /* manito que saluda con rayitas */
        circulo(l, 6.0f, 7.0f, 3.2f, A(0xFFC8A0));
        for (int k = 0; k < 4; k++) rect(l, 3 + k * 2, 1 + (k == 0 || k == 3 ? 2 : 0), 1, 4, A(0xFFC8A0));
        trazo(l, 0.6f, 4.0f, 1.6f, 2.0f, 0.3f, A(0xFFD45C)); trazo(l, 11.4f, 4.0f, 10.4f, 2.0f, 0.3f, A(0xFFD45C));
        break;
    case ICO_LIBRO:
        rect(l, 1, 2, 10, 9, A(0xB06A3A)); rect(l, 2, 3, 8, 7, A(0xFFF1D6)); rect(l, 6, 3, 1, 7, A(0xB06A3A));
        for (int y = 4; y < 9; y += 2) { rect(l, 3, y, 2, 1, A(0xB6B1C6)); rect(l, 8, y, 2, 1, A(0xB6B1C6)); }
        rect(l, 9, 1, 1, 4, A(0xF0506E));
        break;
    case ICO_FOTO:
        rect(l, 1, 3, 10, 7, A(0x4A4A5E)); rect(l, 3, 2, 3, 1, A(0x4A4A5E));
        circulo(l, 6.0f, 6.5f, 2.4f, A(0x9ED8FF)); circulo(l, 6.0f, 6.5f, 1.2f, A(0x2A2238)); lz_set(l, 5, 5, A(0xFFFFFF));
        lz_set(l, 9, 4, A(0xFFE27A));
        break;
    case ICO_COFRE:
        rect(l, 1, 5, 10, 6, A(0xB06A3A)); elipse(l, 6.0f, 5.0f, 5.0f, 2.6f, A(0xC98B5A)); rect(l, 1, 5, 10, 1, A(0xFFD45C));
        rect(l, 5, 6, 2, 2, A(0xFFD45C)); lz_set(l, 3, 3, A(0xFFF6A8)); lz_set(l, 8, 2, A(0xFFF6A8));
        break;
    case ICO_META: /* banderita en un montecito */
        elipse(l, 6.0f, 10.4f, 4.6f, 1.4f, A(0x7FD36B)); rect(l, 4, 1, 1, 10, A(0x6A6A80));
        triangulo(l, 5.0f, 1.0f, 5.0f, 6.0f, 11.0f, 3.5f, A(0xF0506E));
        break;
    case ICO_ESTAMPITA:
        circulo(l, 6.0f, 6.0f, 5.0f, A(0xFFD45C)); circulo(l, 6.0f, 6.0f, 3.8f, A(0xFFE88A));
        triangulo(l, 6.0f, 2.6f, 4.4f, 7.2f, 7.6f, 7.2f, A(0xF0A020)); triangulo(l, 3.0f, 5.0f, 9.0f, 5.0f, 6.0f, 8.6f, A(0xF0A020));
        break;
    case ICO_GORRO:
        triangulo(l, 2.0f, 11.0f, 10.0f, 11.0f, 6.0f, 1.6f, A(0x8C6AD8));
        trazo(l, 3.6f, 8.0f, 7.4f, 4.6f, 0.4f, A(0xFFD45C)); trazo(l, 4.4f, 10.4f, 9.0f, 7.0f, 0.4f, A(0xFF8FC8));
        circulo(l, 6.0f, 1.4f, 1.3f, A(0xFFD45C));
        break;
    case ICO_NARIZ: /* nariz que olfatea con un ¡! */
        circulo(l, 4.0f, 7.0f, 3.0f, A(0xF0A0B8)); lz_set(l, 3, 7, K); lz_set(l, 5, 7, K);
        rect(l, 9, 1, 2, 6, A(0xFFB020)); rect(l, 9, 8, 2, 2, A(0xFFB020));
        break;
    case ICO_CONCHA:
        for (int k = 0; k < 5; k++) trazo(l, 6.0f, 10.0f, 1.5f + (float)k * 2.25f, 3.0f + (k == 2 ? -1.0f : 0.0f), 1.0f, A(0xFFB3C8));
        elipse(l, 6.0f, 6.0f, 4.6f, 3.6f, A(0xFFB3C8));
        for (int k = 0; k < 5; k++) trazo(l, 6.0f, 9.6f, 1.8f + (float)k * 2.1f, 3.4f, 0.2f, A(0xE87A9A));
        rect(l, 5, 9, 3, 2, A(0xFFB3C8));
        break;
    case ICO_PLUMA:
        trazo(l, 2.0f, 11.0f, 9.0f, 2.0f, 0.3f, A(0xFFFFFF));
        elipse(l, 6.6f, 5.0f, 2.2f, 4.0f, A(0x6FB7FF)); /* se tiñe con el color de cada pluma */
        trazo(l, 3.4f, 9.0f, 9.0f, 2.0f, 0.3f, A(0x3E8AD8));
        break;
    case ICO_HOJA:
        elipse(l, 6.0f, 5.6f, 3.4f, 4.6f, A(0x5DBB63)); trazo(l, 6.0f, 2.0f, 6.0f, 11.0f, 0.3f, A(0x3E8A4A));
        trazo(l, 6.0f, 5.0f, 8.0f, 3.6f, 0.2f, A(0x3E8A4A)); trazo(l, 6.0f, 7.0f, 4.0f, 5.6f, 0.2f, A(0x3E8A4A));
        break;
    case ICO_CRISTAL:
        triangulo(l, 6.0f, 0.6f, 2.4f, 5.0f, 9.6f, 5.0f, A(0x7FE0D6)); triangulo(l, 2.4f, 5.0f, 9.6f, 5.0f, 6.0f, 11.4f, A(0x4FC0B6));
        trazo(l, 6.0f, 1.0f, 6.0f, 11.0f, 0.2f, A(0xCFF8F4)); lz_set(l, 4, 4, A(0xFFFFFF));
        break;
    case ICO_CHAPITA:
        circulo(l, 6.0f, 6.0f, 4.6f, A(0xF0506E)); circulo(l, 6.0f, 6.0f, 3.2f, A(0xFF8A9A));
        for (int k = 0; k < 10; k++) { float a = (float)k * 0.628f; lz_set(l, (int)(6 + cosf(a) * 5.0f), (int)(6 + sinf(a) * 5.0f), A(0xC03A50)); }
        lz_set(l, 5, 5, A(0xFFFFFF));
        break;
    case ICO_HOJA_SECA:
        elipse(l, 6.0f, 6.0f, 3.0f, 4.6f, A(0xC08A5A)); trazo(l, 6.0f, 2.0f, 6.0f, 11.0f, 0.3f, A(0x8B5E3C));
        lz_set(l, 4, 4, A(0x8B5E3C)); lz_set(l, 8, 7, A(0x8B5E3C)); rect(l, 3, 8, 1, 1, TRANSP);
        break;
    case ICO_ARCOIRIS: {
        static const uint32_t C[5] = {0xF0506E, 0xFFB020, 0xFFE066, 0x7FD36B, 0x6FB7FF};
        for (int k = 0; k < 5; k++)
            for (int i = 0; i <= 24; i++) { float a = 3.1416f * (float)i / 24.0f; lz_set(l, (int)(6 - cosf(a) * (5.6f - (float)k)), (int)(10 - sinf(a) * (5.6f - (float)k)), A(C[k])); }
        break;
    }
    case ICO_ESTRELLA_FUGAZ:
        trazo(l, 1.0f, 11.0f, 6.0f, 6.0f, 0.4f, A(0xCFE4FF)); trazo(l, 2.0f, 9.0f, 6.5f, 5.5f, 0.3f, A(0x9ED8FF));
        triangulo(l, 8.5f, 0.8f, 6.0f, 6.0f, 11.4f, 4.0f, A(0xFFE066)); triangulo(l, 5.6f, 3.0f, 11.0f, 7.0f, 9.0f, 2.0f, A(0xFFE066));
        break;
    case ICO_CIERVO:
        elipse(l, 5.0f, 7.0f, 3.6f, 2.0f, A(0xF4F1FF)); circulo(l, 9.0f, 4.6f, 1.8f, A(0xF4F1FF));
        rect(l, 2, 8, 1, 4, A(0xE0DCF0)); rect(l, 7, 8, 1, 4, A(0xE0DCF0)); rect(l, 4, 9, 1, 3, A(0xE0DCF0));
        trazo(l, 8.4f, 3.0f, 7.0f, 0.4f, 0.3f, A(0xC8A880)); trazo(l, 9.8f, 3.0f, 11.0f, 0.4f, 0.3f, A(0xC8A880));
        lz_set(l, 10, 4, K);
        break;
    case ICO_GLOBO_AIRE: /* ballena de nubes: blanca, con la panza celeste y un chorrito de estrellas */
        elipse(l, 5.6f, 6.4f, 4.8f, 3.0f, A(0xF6F8FF)); circulo(l, 3.6f, 4.8f, 2.0f, A(0xF6F8FF)); circulo(l, 6.4f, 4.2f, 2.2f, A(0xF6F8FF));
        elipse(l, 5.6f, 8.0f, 4.2f, 1.4f, A(0xBFE6FF));
        triangulo(l, 9.6f, 6.0f, 12.0f, 3.6f, 12.0f, 8.4f, A(0xF6F8FF));
        lz_set(l, 2, 6, A(0x2A2238)); lz_set(l, 3, 8, A(0x9CC8F0));
        lz_set(l, 4, 1, A(0xFFE27A)); lz_set(l, 3, 0, A(0xFFF6C8)); lz_set(l, 5, 0, A(0xFFF6C8));
        break;
    case ICO_CALABAZA:
        elipse(l, 6.0f, 7.0f, 5.4f, 4.0f, A(0xF07A2A)); trazo(l, 4.0f, 3.6f, 4.0f, 10.4f, 0.3f, A(0xC85A1A)); trazo(l, 8.0f, 3.6f, 8.0f, 10.4f, 0.3f, A(0xC85A1A));
        rect(l, 5, 1, 2, 3, A(0x5DBB63));
        triangulo(l, 3.0f, 5.6f, 5.0f, 5.6f, 4.0f, 7.0f, A(0x2A2238)); triangulo(l, 7.0f, 5.6f, 9.0f, 5.6f, 8.0f, 7.0f, A(0x2A2238));
        rect(l, 4, 8, 4, 1, A(0x2A2238));
        break;
    case ICO_ARBOL_NAVIDAD:
        triangulo(l, 6.0f, 0.6f, 2.4f, 5.0f, 9.6f, 5.0f, A(0x2E8B57)); triangulo(l, 6.0f, 3.0f, 1.0f, 9.0f, 11.0f, 9.0f, A(0x2E8B57));
        rect(l, 5, 9, 2, 3, A(0x8B5E3C)); lz_set(l, 6, 0, A(0xFFE066));
        lz_set(l, 4, 6, A(0xF0506E)); lz_set(l, 8, 7, A(0x6FB7FF)); lz_set(l, 6, 4, A(0xFFE066)); lz_set(l, 3, 8, A(0xFFE066));
        break;
    case ICO_FUEGOS: {
        static const uint32_t C[3] = {0xF0506E, 0xFFE066, 0x6FB7FF};
        for (int k = 0; k < 8; k++) { float a = (float)k * 0.785f; trazo(l, 6 + cosf(a) * 1.6f, 6 + sinf(a) * 1.6f, 6 + cosf(a) * 4.8f, 6 + sinf(a) * 4.8f, 0.3f, A(C[k % 3])); }
        lz_set(l, 6, 6, A(0xFFFFFF));
        break;
    }
    case ICO_BANDERINES:
        trazo(l, 0.0f, 2.0f, 12.0f, 2.0f, 0.3f, A(0x6A6A80));
        /* los colores de la bandera de Chile, siempre en orden: rojo, blanco y azul */
        triangulo(l, 0.5f, 2.4f, 3.5f, 2.4f, 2.0f, 7.0f, A(0xD52B1E)); triangulo(l, 4.5f, 2.4f, 7.5f, 2.4f, 6.0f, 7.0f, A(0xFFFFFF));
        triangulo(l, 8.5f, 2.4f, 11.5f, 2.4f, 10.0f, 7.0f, A(0x0039A6));
        break;
    case ICO_NADAR:
        circulo(l, 6.0f, 4.0f, 2.6f, A(0xFFC8A0)); lz_set(l, 5, 4, K); lz_set(l, 7, 4, K);
        for (int x = 0; x < 12; x++) { int y = 7 + (int)(sinf((float)x * 1.2f) * 1.2f); rect(l, x, y, 1, 12 - y, A(0x6FB7FF)); lz_set(l, x, y, A(0xCFF0FF)); }
        break;
    case ICO_TREPAR:
        rect(l, 4, 3, 3, 9, A(0x8B5E3C)); circulo(l, 5.5f, 2.6f, 3.0f, A(0x5DBB63));
        triangulo(l, 9.0f, 1.0f, 7.4f, 4.0f, 10.6f, 4.0f, A(0xFFD45C)); rect(l, 8, 4, 2, 6, A(0xFFD45C));
        break;
    case ICO_RELOJ:
        circulo(l, 6.0f, 6.0f, 5.4f, A(0x4D7FE0)); circulo(l, 6.0f, 6.0f, 4.3f, A(0xFFFFFF));
        rect(l, 6, 3, 1, 4, K); rect(l, 6, 6, 3, 1, K);
        lz_set(l, 6, 2, A(0x9A96B0)); lz_set(l, 10, 6, A(0x9A96B0)); lz_set(l, 6, 10, A(0x9A96B0)); lz_set(l, 2, 6, A(0x9A96B0));
        break;
    case ICO_DADO:
        rect(l, 1, 1, 10, 10, A(0xFFFFFF)); rect(l, 1, 10, 10, 1, A(0xC8C4D8)); rect(l, 10, 1, 1, 10, A(0xC8C4D8));
        lz_set(l, 3, 3, A(0xF0506E)); lz_set(l, 5, 5, A(0xF0506E)); lz_set(l, 7, 7, A(0xF0506E));
        lz_set(l, 7, 3, A(0xF0506E)); lz_set(l, 3, 7, A(0xF0506E));
        break;
    case ICO_PERCHA: /* una percha con un vestidito */
        trazo(l, 6, 0.5f, 6, 2.0f, 0.5f, A(0x9A96B0));
        trazo(l, 1.0f, 4.0f, 6.0f, 2.0f, 0.5f, A(0x9A96B0)); trazo(l, 6.0f, 2.0f, 11.0f, 4.0f, 0.5f, A(0x9A96B0));
        triangulo(l, 3.0f, 4.0f, 9.0f, 4.0f, 6.0f, 6.0f, A(0xFF8FC8));
        triangulo(l, 3.5f, 5.5f, 8.5f, 5.5f, 10.0f, 11.0f, A(0xFF8FC8)); triangulo(l, 3.5f, 5.5f, 2.0f, 11.0f, 10.0f, 11.0f, A(0xFF8FC8));
        rect(l, 3, 7, 6, 1, A(0xFFFFFF));
        break;
    case ICO_MONO: /* moño de pétalos con brillitos */
        triangulo(l, 6.0f, 6.0f, 1.0f, 2.5f, 1.6f, 9.5f, A(0xFF8FC0)); triangulo(l, 6.0f, 6.0f, 11.0f, 2.5f, 10.4f, 9.5f, A(0xFF8FC0));
        lz_set(l, 2, 4, A(0xFFD6E8)); lz_set(l, 9, 4, A(0xFFD6E8));
        circulo(l, 6.0f, 6.0f, 1.4f, A(0xFFE066));
        lz_set(l, 11, 1, A(0xFFF6B0)); lz_set(l, 1, 11, A(0xFFF6B0));
        break;
    case ICO_PANUELO: /* pañuelo de hojas */
        triangulo(l, 0.5f, 2.0f, 11.5f, 2.0f, 6.0f, 10.5f, A(0x6CC870));
        trazo(l, 6.0f, 2.5f, 6.0f, 9.5f, 0.3f, A(0x3E9A4E)); trazo(l, 6.0f, 4.5f, 3.0f, 3.0f, 0.25f, A(0x3E9A4E)); trazo(l, 6.0f, 4.5f, 9.0f, 3.0f, 0.25f, A(0x3E9A4E));
        lz_set(l, 2, 3, A(0xF2A33A)); lz_set(l, 9, 3, A(0xF2A33A));
        break;
    case ICO_GORRO_LANA: /* gorro de lana azul noche con estrellitas */
        elipse(l, 6.0f, 8.0f, 5.0f, 4.5f, A(0x4A3FB8));
        rect(l, 0, 8, 12, 4, TRANSP);
        rect(l, 1, 8, 10, 2, A(0xE8E4FF)); for (int x = 1; x < 11; x += 2) lz_set(l, x, 9, A(0xC8C2F0));
        estrellita(l, 4, 6, A(0xFFE066)); lz_set(l, 8, 5, A(0xFFF6C0));
        circulo(l, 6.0f, 2.2f, 1.6f, A(0xFFFFFF));
        break;
    case ICO_BUFANDA: /* bufanda de seda de araña: blanca plateada, con brillo */
        rect(l, 1, 3, 10, 3, A(0xF0F0FF)); rect(l, 7, 5, 3, 6, A(0xF0F0FF));
        for (int x = 2; x < 11; x += 3) rect(l, x, 4, 1, 1, A(0xB8C8E8));
        rect(l, 7, 8, 3, 1, A(0xB8C8E8)); lz_set(l, 3, 3, A(0xFFFFFF)); lz_set(l, 8, 10, A(0xB8C8E8));
        break;
    case ICO_SOMBRERO: /* sombrero de paja con una flor */
        elipse(l, 6.0f, 8.0f, 5.6f, 1.6f, A(0xE8C36A));
        rect(l, 3, 3, 6, 5, A(0xF0D488)); rect(l, 3, 6, 6, 1, A(0x5DBB63));
        lz_set(l, 4, 4, A(0xFFF0B8)); lz_set(l, 7, 8, A(0xC9A24E)); lz_set(l, 2, 8, A(0xC9A24E));
        lz_set(l, 9, 5, A(0xFF8FC0)); lz_set(l, 10, 6, A(0xFF8FC0)); lz_set(l, 9, 7, A(0xFF8FC0)); lz_set(l, 8, 6, A(0xFF8FC0)); lz_set(l, 9, 6, A(0xFFE066));
        break;
    case ICO_LENTES: /* lentes de cristal de luna */
        rect(l, 0, 4, 5, 4, A(0xB8C0D8)); rect(l, 7, 4, 5, 4, A(0xB8C0D8)); rect(l, 5, 5, 2, 1, A(0xB8C0D8));
        rect(l, 1, 5, 3, 2, A(0xCFE0FF)); rect(l, 8, 5, 3, 2, A(0xCFE0FF));
        lz_set(l, 1, 5, A(0xFFFFFF)); lz_set(l, 8, 5, A(0xFFFFFF)); lz_set(l, 3, 6, A(0x9AB4F0)); lz_set(l, 10, 6, A(0x9AB4F0));
        break;
    case ICO_CORONA: /* corona de flores (de noche brillan) */
        trazo(l, 1.0f, 7.0f, 11.0f, 7.0f, 0.6f, A(0x5DBB63));
        for (int k = 0; k < 4; k++) {
            float x = 2.0f + k * 2.7f;
            uint32_t c = k % 2 ? A(0xFFE066) : A(0xFF8FC8);
            circulo(l, x, 5.5f, 1.4f, c); lz_set(l, (int)x, 5, A(0xFFFFFF));
        }
        break;
    case ICO_COLLAR: /* collar de conchas con una perla luminosa */
        for (int k = 0; k < 7; k++) {
            float a = 3.14159f * (0.15f + 0.7f * (float)k / 6.0f);
            float x = 6.0f - 5.0f * cosf(a), y = 2.0f + 6.0f * sinf(a);
            circulo(l, x, y, 1.1f, k % 2 ? A(0xFFF1C9) : A(0xFFC2D6));
        }
        circulo(l, 6.0f, 9.4f, 1.8f, A(0xF4F0FF)); lz_set(l, 5, 9, A(0xFFFFFF)); /* la perla luminosa */
        lz_set(l, 3, 10, A(0xE8E0FF)); lz_set(l, 9, 10, A(0xE8E0FF));
        break;
    case ICO_PELUCHE: /* osito de peluche */
        circulo(l, 3.0f, 2.6f, 1.6f, A(0xB07A4E)); circulo(l, 9.0f, 2.6f, 1.6f, A(0xB07A4E));
        circulo(l, 6.0f, 5.0f, 3.6f, A(0xC8925E)); elipse(l, 6.0f, 9.5f, 3.6f, 2.4f, A(0xB07A4E));
        elipse(l, 6.0f, 6.3f, 1.5f, 1.0f, A(0xF0D2A8)); lz_set(l, 6, 6, K); lz_set(l, 4, 4, K); lz_set(l, 7, 4, K);
        lz_set(l, 3, 2, A(0xF0A0A0)); lz_set(l, 9, 2, A(0xF0A0A0));
        break;
    case ICO_PELOTON: /* pelota grande a rayas */
        circulo(l, 6.0f, 6.0f, 5.5f, A(0x4DA6FF));
        rect(l, 0, 4, 12, 2, A(0xFFFFFF)); rect(l, 0, 8, 12, 1, A(0xFFD45C));
        for (int y = 0; y < 12; y++) for (int x = 0; x < 12; x++) {
            float dx = x + 0.5f - 6.0f, dy = y + 0.5f - 6.0f;
            if (dx * dx + dy * dy > 30.25f) lz_set(l, x, y, TRANSP);
        }
        lz_set(l, 3, 2, A(0xFFFFFF));
        break;
    case ICO_CAJA: /* caja de cartón abierta */
        rect(l, 1, 4, 10, 7, A(0xC9945C)); rect(l, 1, 4, 10, 1, A(0x9C6B3C));
        triangulo(l, 1.0f, 4.0f, 0.0f, 1.0f, 4.0f, 4.0f, A(0xDDB07A)); triangulo(l, 11.0f, 4.0f, 12.0f, 1.0f, 8.0f, 4.0f, A(0xDDB07A));
        rect(l, 5, 6, 2, 3, A(0xE8C99A));
        break;
    case ICO_COLUMPIO: /* columpio: marco y asiento */
        trazo(l, 1.0f, 11.0f, 3.0f, 1.0f, 0.5f, A(0x8B5E3C)); trazo(l, 11.0f, 11.0f, 9.0f, 1.0f, 0.5f, A(0x8B5E3C));
        rect(l, 2, 1, 8, 1, A(0x8B5E3C));
        trazo(l, 4.5f, 1.5f, 4.5f, 8.0f, 0.3f, A(0x9A96B0)); trazo(l, 7.5f, 1.5f, 7.5f, 8.0f, 0.3f, A(0x9A96B0));
        rect(l, 3, 8, 6, 1, A(0xF0506E));
        break;
    case ICO_TAMBOR: /* tambor con palillos */
        elipse(l, 6.0f, 8.0f, 4.6f, 3.2f, A(0xE5484D)); rect(l, 1, 5, 10, 4, A(0xE5484D));
        elipse(l, 6.0f, 5.0f, 4.6f, 1.5f, A(0xFFF6E0));
        for (int x = 2; x < 11; x += 2) lz_set(l, x, 7 + (x % 4 == 0), A(0xFFD45C));
        trazo(l, 2.0f, 0.5f, 5.0f, 4.0f, 0.4f, A(0xC9945C)); trazo(l, 10.0f, 0.5f, 7.0f, 4.0f, 0.4f, A(0xC9945C));
        break;
    case ICO_MOLINETE: /* molinete de papel */
        trazo(l, 6.0f, 6.0f, 6.0f, 11.5f, 0.4f, A(0x8B5E3C));
        triangulo(l, 6.0f, 6.0f, 6.0f, 1.0f, 9.0f, 3.0f, A(0xF0506E));
        triangulo(l, 6.0f, 6.0f, 11.0f, 6.0f, 9.0f, 9.0f, A(0xFFD45C));
        triangulo(l, 6.0f, 6.0f, 6.0f, 11.0f, 3.0f, 9.0f, A(0x4DA6FF));
        triangulo(l, 6.0f, 6.0f, 1.0f, 6.0f, 3.0f, 3.0f, A(0x5DBB63));
        lz_set(l, 6, 6, A(0xFFFFFF));
        break;
    case ICO_GIRASOL: /* girasol */
        trazo(l, 6.0f, 6.0f, 6.0f, 11.5f, 0.5f, A(0x3E9E4C)); elipse(l, 8.0f, 9.0f, 1.6f, 0.8f, A(0x5DBB63));
        for (int k = 0; k < 8; k++) {
            float a = k * 0.785f;
            circulo(l, 6.0f + 3.0f * cosf(a), 4.5f + 3.0f * sinf(a), 1.3f, A(0xFFD02E));
        }
        circulo(l, 6.0f, 4.5f, 1.9f, A(0x8B5E3C)); lz_set(l, 5, 4, A(0xB07A4E));
        break;
    case ICO_FRUTILLA: /* matita con frutillas */
        elipse(l, 6.0f, 6.0f, 5.0f, 3.0f, A(0x5DBB63)); elipse(l, 4.0f, 5.0f, 2.0f, 1.2f, A(0x7ED37A));
        for (int k = 0; k < 3; k++) {
            float x = 3.0f + k * 3.0f, y = 9.0f - (k == 1 ? 1.0f : 0.0f);
            triangulo(l, x - 1.6f, y - 1.2f, x + 1.6f, y - 1.2f, x, y + 1.8f, A(0xE5384D));
            lz_set(l, (int)x, (int)y, A(0xFFE066));
        }
        break;
    case ICO_CAMARA: /* cámara de fotos */
        rect(l, 1, 3, 10, 7, A(0x4A4458)); rect(l, 3, 2, 3, 1, A(0x4A4458)); rect(l, 1, 4, 10, 1, A(0x6A6480));
        circulo(l, 6.0f, 6.5f, 2.4f, A(0x2A2238)); circulo(l, 6.0f, 6.5f, 1.4f, A(0x7CC8FF)); lz_set(l, 5, 6, A(0xFFFFFF));
        lz_set(l, 9, 4, A(0xFFD45C));
        break;
    case ICO_PECERA: /* pecera redonda con un pececito */
        circulo(l, 6.0f, 6.5f, 5.0f, A(0xC2EBFF)); rect(l, 0, 0, 12, 3, TRANSP);
        rect(l, 2, 2, 8, 1, A(0x8CCBFF));
        for (int Y = (int)(5 * KL(l)); Y < l->h; Y++) for (int X = 0; X < l->w; X++) if (lz_rget(l, X, Y)) lz_real(l, X, Y, A(0x7CC8FF));
        elipse(l, 6.0f, 7.5f, 1.8f, 1.1f, A(0xFF9A3C)); triangulo(l, 7.5f, 7.5f, 9.3f, 6.3f, 9.3f, 8.7f, A(0xFF9A3C));
        lz_set(l, 5, 7, K); lz_set(l, 3, 9, A(0x5DBB63)); lz_set(l, 3, 10, A(0x5DBB63));
        break;
    case ICO_PARLANTE: /* parlante con ondas */
        rect(l, 1, 4, 3, 4, A(0x6A6480)); triangulo(l, 3.0f, 6.0f, 7.0f, 1.0f, 7.0f, 11.0f, A(0x6A6480));
        trazo(l, 9.0f, 4.0f, 9.5f, 6.0f, 0.4f, A(0x7CC8FF)); trazo(l, 9.5f, 6.0f, 9.0f, 8.0f, 0.4f, A(0x7CC8FF));
        trazo(l, 10.8f, 2.5f, 11.5f, 6.0f, 0.4f, A(0x7CC8FF)); trazo(l, 11.5f, 6.0f, 10.8f, 9.5f, 0.4f, A(0x7CC8FF));
        break;
    case ICO_PALETA: /* paleta de pintor */
        elipse(l, 6.0f, 6.5f, 5.5f, 4.5f, A(0xF0D2A8)); circulo(l, 8.0f, 8.5f, 1.2f, TRANSP);
        lz_set(l, 3, 5, A(0xF0506E)); lz_set(l, 5, 3, A(0xFFD45C)); lz_set(l, 8, 4, A(0x4DA6FF)); lz_set(l, 3, 8, A(0x5DBB63));
        lz_set(l, 4, 5, A(0xF0506E)); lz_set(l, 6, 3, A(0xFFD45C)); lz_set(l, 9, 4, A(0x4DA6FF)); lz_set(l, 4, 8, A(0x5DBB63));
        break;
    case ICO_PLANTA_SECA: /* tallo café y doblado, hojas secas en el suelo */
        elipse(l, 6.0f, 10.6f, 4.6f, 1.2f, A(0x8B5E3C));
        trazo(l, 5.4f, 10.0f, 5.8f, 5.6f, 0.45f, A(0xA07A48)); trazo(l, 5.8f, 5.6f, 8.6f, 6.4f, 0.4f, A(0xA07A48));
        circulo(l, 9.0f, 7.0f, 1.3f, A(0x9C7B5A)); lz_set(l, 9, 7, A(0x7A5A3A));
        elipse(l, 3.6f, 9.6f, 1.4f, 0.6f, A(0xB58E5A)); elipse(l, 8.6f, 10.0f, 1.2f, 0.5f, A(0xC49A62));
        break;
    case ICO_MAPA: /* un mapa doblado con un camino y una cruz */
        triangulo(l, 1.0f, 2.0f, 4.0f, 1.0f, 4.0f, 10.0f, A(0xF4E3B8)); triangulo(l, 1.0f, 2.0f, 4.0f, 10.0f, 1.0f, 11.0f, A(0xF4E3B8));
        rect(l, 4, 1, 4, 10, A(0xE8D29C));
        triangulo(l, 8.0f, 1.0f, 11.0f, 2.0f, 11.0f, 11.0f, A(0xF4E3B8)); triangulo(l, 8.0f, 1.0f, 11.0f, 11.0f, 8.0f, 10.0f, A(0xF4E3B8));
        lz_set(l, 2, 8, A(0x5DBB63)); lz_set(l, 3, 7, A(0x5DBB63)); lz_set(l, 5, 6, A(0x7A5234)); lz_set(l, 6, 5, A(0x7A5234)); lz_set(l, 8, 5, A(0x7A5234));
        lz_set(l, 9, 3, A(0xF0506E)); lz_set(l, 10, 4, A(0xF0506E)); lz_set(l, 10, 3, A(0xF0506E)); lz_set(l, 9, 4, A(0xF0506E));
        lz_set(l, 2, 3, A(0x4DA6FF)); lz_set(l, 3, 4, A(0x4DA6FF));
        break;
    case ICO_MALETA: /* maleta de viaje con etiqueta */
        rect(l, 4, 1, 4, 1, A(0x6A4A2A)); rect(l, 4, 1, 1, 3, A(0x6A4A2A)); rect(l, 7, 1, 1, 3, A(0x6A4A2A));
        rect(l, 1, 3, 10, 8, A(0xE08A3C)); rect(l, 1, 9, 10, 2, A(0xC0702E));
        rect(l, 3, 3, 1, 8, A(0x8A5428)); rect(l, 8, 3, 1, 8, A(0x8A5428));
        rect(l, 5, 5, 2, 2, A(0xFFF2D0)); lz_set(l, 5, 7, A(0xF0506E));
        lz_set(l, 2, 11, K); lz_set(l, 9, 11, K);
        break;
    case ICO_PINA: /* piña de pino: escamas en diagonal */
        elipse(l, 6.0f, 6.6f, 3.6f, 4.8f, A(0x9A5A2A));
        for (int f = 0; f < 4; f++) for (int k = 0; k < 3; k++) lz_set(l, 4 + k + (f & 1), 3 + f * 2, A(0x6A3A1A));
        lz_set(l, 5, 2, A(0xC88A4A)); lz_set(l, 7, 4, A(0xC88A4A)); lz_set(l, 4, 6, A(0xC88A4A));
        rect(l, 6, 0, 1, 2, A(0x5DBB63));
        break;
    case ICO_SAMARA: /* semilla de arce con su ala */
        circulo(l, 3.4f, 8.4f, 1.8f, A(0x8A5A2A));
        triangulo(l, 3.0f, 7.0f, 11.0f, 1.0f, 5.0f, 9.4f, A(0xC8B060)); trazo(l, 3.6f, 7.8f, 10.0f, 2.0f, 0.25f, A(0x9A7A3A));
        lz_set(l, 8, 3, A(0xE8D890));
        break;
    case ICO_HUELLA: /* una patita de barro */
        elipse(l, 6.0f, 8.0f, 2.6f, 2.2f, A(0x6A4A2A));
        circulo(l, 3.2f, 4.6f, 1.1f, A(0x6A4A2A)); circulo(l, 5.2f, 3.4f, 1.1f, A(0x6A4A2A));
        circulo(l, 7.2f, 3.4f, 1.1f, A(0x6A4A2A)); circulo(l, 9.0f, 4.6f, 1.1f, A(0x6A4A2A));
        break;
    case ICO_LIBRO_MAGICO: /* libro violeta con una estrella dorada */
        rect(l, 2, 1, 8, 10, A(0x6A4FA0)); rect(l, 3, 1, 7, 9, A(0x8A6AC8)); rect(l, 2, 1, 2, 10, A(0x4A3478));
        rect(l, 3, 10, 7, 1, A(0xF6F2EA));
        estrellita(l, 7, 5, A(0xFFE27A)); lz_set(l, 7, 5, A(0xFFFFFF));
        break;
    case ICO_ESPONJA: /* esponja amarilla con burbujas */
        rect(l, 1, 5, 9, 6, A(0xFFD45C)); rect(l, 1, 4, 9, 1, A(0xFFE89A)); rect(l, 1, 9, 9, 2, A(0xE0A92A));
        lz_set(l, 3, 6, A(0xE0A92A)); lz_set(l, 6, 7, A(0xE0A92A)); lz_set(l, 8, 6, A(0xE0A92A)); lz_set(l, 4, 9, A(0xC89020));
        circulo(l, 8.6f, 2.4f, 1.6f, A(0xBFE6FF)); circulo(l, 4.6f, 1.8f, 1.1f, A(0xBFE6FF)); lz_set(l, 8, 2, A(0xFFFFFF));
        break;
    case ICO_BOLSA: /* un morral de tela: boca fruncida arriba, cordón claro con su nudo, panza redonda y una estrellita */
        triangulo(l, 2.2f, 1.2f, 9.8f, 1.2f, 6.0f, 5.0f, A(0xDDA66C));                  /* la boca fruncida, abierta como abanico */
        lz_set(l, 4, 1, A(0xB57A44)); lz_set(l, 6, 1, A(0xB57A44)); lz_set(l, 8, 1, A(0xB57A44)); /* sus pliegues */
        rect(l, 4, 4, 4, 2, A(0xC8894C));                                               /* el cuello */
        elipse(l, 6.0f, 8.4f, 5.2f, 3.4f, A(0xC8894C));                                 /* la panza */
        elipse(l, 6.8f, 10.0f, 4.0f, 1.4f, A(0xA06A3A));                                /* su sombra */
        elipse(l, 4.2f, 7.4f, 1.4f, 0.9f, A(0xE6B47A));                                 /* brillo */
        rect(l, 3, 4, 6, 1, A(0xF6EAC8));                                               /* el cordón */
        circulo(l, 9.2f, 4.6f, 0.9f, A(0xF6EAC8)); trazo(l, 9.4f, 5.2f, 10.4f, 7.2f, 0.3f, A(0xF6EAC8)); /* el nudo y la punta */
        estrellita(l, 6, 8, A(0xFFE27A)); lz_set(l, 6, 8, A(0xFFFFFF));
        break;
    case ICO_BOSQUE_DEV: /* un arbolito con una flecha que vuelve */
        circulo(l, 6.0f, 4.6f, 3.6f, A(0x5DBB63)); circulo(l, 4.4f, 6.0f, 2.4f, A(0x4FA86A)); circulo(l, 7.8f, 6.0f, 2.4f, A(0x4FA86A));
        rect(l, 5, 8, 2, 4, A(0x8B5E3C)); lz_set(l, 4, 3, A(0x8FE08A));
        triangulo(l, 9.0f, 9.0f, 12.0f, 9.0f, 10.5f, 11.4f, A(0xFFE27A)); trazo(l, 10.5f, 9.0f, 10.5f, 7.0f, 0.4f, A(0xFFE27A));
        break;
    case ICO_MANO_FROTA:
    case ICO_MANO_TIENE:
    case ICO_MANO_DESLIZA: /* un dedo y lo que hace */
        elipse(l, 6.0f, 8.0f, 2.6f, 2.8f, A(0xFFD7B0)); rect(l, 5, 2, 2, 6, A(0xFFD7B0)); lz_set(l, 5, 2, A(0xFFE8D0));
        if (i == ICO_MANO_FROTA) { trazo(l, 1.0f, 3.0f, 3.0f, 1.6f, 0.25f, A(0xFFE27A)); trazo(l, 9.0f, 1.6f, 11.0f, 3.0f, 0.25f, A(0xFFE27A)); trazo(l, 1.0f, 5.0f, 3.0f, 3.6f, 0.25f, A(0xFFE27A)); trazo(l, 9.0f, 3.6f, 11.0f, 5.0f, 0.25f, A(0xFFE27A)); }
        else if (i == ICO_MANO_TIENE) { circulo(l, 6.0f, 1.6f, 1.4f, A(0xFFE27A)); lz_set(l, 6, 1, A(0xFFFFFF)); }
        else { trazo(l, 0.6f, 3.0f, 3.6f, 3.0f, 0.3f, A(0xFFE27A)); triangulo(l, 9.0f, 1.6f, 9.0f, 4.4f, 11.6f, 3.0f, A(0xFFE27A)); trazo(l, 8.0f, 3.0f, 9.4f, 3.0f, 0.3f, A(0xFFE27A)); }
        break;
    case ICO_CANDADO:
        trazo(l, 4.0f, 5.5f, 4.0f, 3.5f, 0.6f, A(0x9A96B0)); trazo(l, 8.0f, 5.5f, 8.0f, 3.5f, 0.6f, A(0x9A96B0));
        trazo(l, 4.0f, 3.4f, 6.0f, 1.6f, 0.6f, A(0x9A96B0)); trazo(l, 8.0f, 3.4f, 6.0f, 1.6f, 0.6f, A(0x9A96B0));
        rect(l, 2, 5, 8, 6, A(0xFFC93D)); rect(l, 2, 9, 8, 2, A(0xE0A92A));
        lz_set(l, 6, 7, A(0x6A4A1A)); lz_set(l, 6, 8, A(0x6A4A1A));
        break;
    case ICO_MEZCLA: /* dos gotas que se juntan */
        circulo(l, 4.0f, 6.5f, 3.4f, A(0x86D16A)); circulo(l, 8.0f, 6.5f, 3.4f, A(0xF26B3A));
        elipse(l, 6.0f, 6.5f, 1.4f, 3.0f, A(0xC2A64E));
        lz_set(l, 3, 5, A(0xFFFFFF)); lz_set(l, 7, 5, A(0xFFFFFF));
        break
;
    case ICO_RESPALDO: /* una pantallita de computador con una flecha que entra y sale */
        rect(l, 0, 1, 12, 8, A(0x6A6A80)); rect(l, 1, 2, 10, 6, A(0x7CC8FF));
        rect(l, 5, 9, 2, 1, A(0x6A6A80)); rect(l, 3, 10, 6, 1, A(0x6A6A80));
        rect(l, 5, 3, 2, 3, A(0xFFFFFF)); triangulo(l, 3.0f, 5.5f, 9.0f, 5.5f, 6.0f, 8.0f, A(0xFFFFFF));
        break;
    case ICO_ABRAZO: /* dos bracitos rodeando un corazón */
        circulo(l, 4.6f, 5.0f, 1.9f, A(0xF0506E)); circulo(l, 7.4f, 5.0f, 1.9f, A(0xF0506E));
        triangulo(l, 2.6f, 5.6f, 9.4f, 5.6f, 6.0f, 9.4f, A(0xF0506E));
        trazo(l, 1.0f, 3.0f, 2.0f, 9.0f, 0.6f, A(0xFFC8A0)); trazo(l, 2.0f, 9.0f, 5.0f, 11.0f, 0.6f, A(0xFFC8A0));
        trazo(l, 11.0f, 3.0f, 10.0f, 9.0f, 0.6f, A(0xFFC8A0)); trazo(l, 10.0f, 9.0f, 7.0f, 11.0f, 0.6f, A(0xFFC8A0));
        break;
    default: break;
    }
}

#define ART_ICONO_INFO(id, g, n, s, u) {#id, g, n, s, u},
static const art_icono_info_t ICONOS_INFO[ICO_OBJ0] = {ART_ICONOS(ART_ICONO_INFO)};
const art_icono_info_t *art_icono_info(art_icono_t ico)
{
    if ((unsigned)ico < ICO_OBJ0) return &ICONOS_INFO[ico];
    if ((unsigned)ico >= ICO_COUNT) return NULL;
    /* los objetos: su ficha sale del catálogo */
    static EXT_RAM_BSS_ATTR art_icono_info_t obj[ART_OBJETOS];
    static EXT_RAM_BSS_ATTR char ids[ART_OBJETOS][12]; /* (1.0.14) en la PSRAM */
    static const char *const GRUPO[OCAT_COUNT] = {"obj_tierra", "obj_flora", "obj_espiritu", "obj_antiguo", "obj_artefacto"};
    int id = ico - ICO_OBJ0;
    if (!obj[id].id) {
        const objeto_info_t *o = objeto_info(id);
        snprintf(ids[id], sizeof ids[id], "ICO_OBJ_%03d", id + 1);
        obj[id] = (art_icono_info_t){ids[id], GRUPO[o->cat % OCAT_COUNT], o->nombre, o->leyenda, "La Bolsa"};
    }
    return &obj[id];
}

typedef struct {
    art_img_t img;
    uint8_t escala, espejo;
} ico_cache_t;
#define ICO_CACHE 5
static EXT_RAM_BSS_ATTR ico_cache_t s_iconos[ICO_OBJ0][ICO_CACHE];
/* los objetos comparten un caché chico (se ven pocos a la vez) */
#define OBJ_CACHE 28
static EXT_RAM_BSS_ATTR struct { ico_cache_t c; uint16_t ico; } s_obj_cache[OBJ_CACHE];
static int s_obj_turno;

/* íconos que cambiaste en el editor (ya con su contorno): 24x24, o 12x12 si vienen del editor antiguo */
static EXT_RAM_BSS_ATTR uint32_t *s_propio[ICO_COUNT];
static uint8_t s_propio_lado[ICO_COUNT];

/* el de fábrica con k pixeles reales por pixel del dibujo (de 12*k de lado), con su contorno fino */
static void ico_fabrica_k(art_icono_t ico, uint32_t *px, float k)
{
    int L = (int)lroundf(ICO_LADO * k);
    memset(px, 0, (size_t)L * L * 4);
    lienzo_t l = {L, L, px, k};
    ico_dibujar(&l, ico);
    contornear(&l, K);
}

void art_icono_pixeles(art_icono_t ico, uint32_t px[ART_ICONO_LADO * ART_ICONO_LADO])
{
    if ((unsigned)ico >= ICO_COUNT) return;
    if (s_propio[ico]) {
        const int n = s_propio_lado[ico];
        for (int y = 0; y < ART_ICONO_LADO; y++)
            for (int x = 0; x < ART_ICONO_LADO; x++) px[y * ART_ICONO_LADO + x] = s_propio[ico][(y * n / ART_ICONO_LADO) * n + x * n / ART_ICONO_LADO];
        return;
    }
    ico_fabrica_k(ico, px, (float)ART_ICONO_LADO / ICO_LADO);
}

void art_icono_fabrica(art_icono_t ico, uint32_t px[ART_ICONO_LADO * ART_ICONO_LADO])
{
    if ((unsigned)ico >= ICO_COUNT) return;
    ico_fabrica_k(ico, px, (float)ART_ICONO_LADO / ICO_LADO);
}

bool art_icono_es_propio(art_icono_t ico) { return (unsigned)ico < ICO_COUNT && s_propio[ico]; }

int art_icono_lado_de_bytes(int n)
{
    if (n == ART_ICONO_LADO * ART_ICONO_LADO * 4) return ART_ICONO_LADO;
    if (n == ART_ICONO_LADO_V1 * ART_ICONO_LADO_V1 * 4) return ART_ICONO_LADO_V1;
    return 0;
}

/* el ícono listo para la pantalla: 12*escala pixeles de lado */

/* estilo nuevo en los íconos: el contorno toma el color de lo que encierra (más oscuro, no negro),
 * y cada parte tiene luz arriba a la izquierda y sombra abajo a la derecha, tramadas */
static int luma(uint32_t c) { return (int)((((c >> 16) & 255) * 3 + ((c >> 8) & 255) * 6 + (c & 255)) / 10); }
static void ico_pulir(uint32_t *px, int L, float k)
{
    uint32_t *o = malloc((size_t)L * L * 4);
    if (!o) return;
    memcpy(o, px, (size_t)L * L * 4);
    const int G = k >= 1.4f ? 2 : 1; /* grosor del bisel, en pixeles reales */
#define OP(x, y) ((x) >= 0 && (y) >= 0 && (x) < L && (y) < L && (o[(y) * L + (x)] >> 24) > 0x80)
#define OSC(x, y) (OP(x, y) && luma(o[(y) * L + (x)]) < 64)
    for (int y = 0; y < L; y++)
        for (int x = 0; x < L; x++) {
            uint32_t c = o[y * L + x];
            if ((c >> 24) <= 0x80) continue;
            bool borde = !OP(x - 1, y) || !OP(x + 1, y) || !OP(x, y - 1) || !OP(x, y + 1);
            if (luma(c) < 64) {
                if (!borde) continue;
                /* contorno de afuera: el color vecino de adentro, oscurecido */
                uint32_t v = 0; int n = 0, r = 0, g = 0, b = 0;
                for (int dy = -2; dy <= 2; dy++)
                    for (int dx = -2; dx <= 2; dx++)
                        if (OP(x + dx, y + dy) && !OSC(x + dx, y + dy)) { v = o[(y + dy) * L + x + dx]; r += (v >> 16) & 255; g += (v >> 8) & 255; b += v & 255; n++; }
                if (n) {
                    uint32_t m = A(((uint32_t)(r / n) << 16) | ((uint32_t)(g / n) << 8) | (uint32_t)(b / n));
                    px[y * L + x] = (c & 0xFF000000u) | (mezclar(m, A(0x1A1626), 0.62f) & 0xFFFFFF);
                }
                continue;
            }
            if (luma(c) > 235) continue; /* el blanco y los brillos quedan como están */
            /* el bisel: cuánto falta para el borde (o una línea oscura) hacia arriba-izquierda y abajo-derecha */
            int luz = 0, som = 0;
            for (int d = 1; d <= G; d++) {
                if (!luz && (!OP(x - d, y) || OSC(x - d, y) || !OP(x, y - d) || OSC(x, y - d))) luz = d;
                if (!som && (!OP(x + d, y) || OSC(x + d, y) || !OP(x, y + d) || OSC(x, y + d))) som = d;
            }
            uint32_t r = c;
            bool t = ((x + y) & 1) != 0;
            if (luz == 1) r = mezclar(c, A(0xFFFFFF), 0.28f);
            else if (luz == 2 && t) r = mezclar(c, A(0xFFFFFF), 0.16f);
            else if (som == 1) r = mezclar(c, A(0x1A1626), 0.22f);
            else if (som == 2 && t) r = mezclar(c, A(0x1A1626), 0.12f);
            px[y * L + x] = (c & 0xFF000000u) | (r & 0xFFFFFF);
        }
#undef OP
#undef OSC
    free(o);
}

#include "iconos_hd.inc"
#include "iconos_hd2.inc"
/* íconos pintados a 48x48: se reducen (o amplían) al tamaño pedido promediando, para que se vean nítidos a cualquier escala */
static void ico_muestrear(art_img_t *img, const uint32_t *pal, const uint8_t *idx, int escala, bool espejo, int n)
{
    const int W = ICO_LADO * escala;
    img_preparar(img, W, W);
    if (!img->px) return;
    for (int Y = 0; Y < W; Y++)
        for (int X = 0; X < W; X++) {
            int sa = 0, sr = 0, sg = 0, sb = 0;
            for (int j = 0; j < 3; j++)
                for (int i = 0; i < 3; i++) {
                    int px = (X * 6 + i * 2 + 1) * n / (W * 6), py = (Y * 6 + j * 2 + 1) * n / (W * 6);
                    if (px >= n) px = n - 1;
                    if (py >= n) py = n - 1;
                    if (espejo) px = n - 1 - px;
                    const uint32_t p = pal[idx[py * n + px]];
                    const int al = (int)(p >> 24);
                    if (!al) continue;
                    sa += al; sr += (int)((p >> 16) & 255) * al; sg += (int)((p >> 8) & 255) * al; sb += (int)(p & 255) * al;
                }
            uint32_t v = 0;
            if (sa) v = ((uint32_t)(sa / 9) << 24) | ((uint32_t)(sr / sa) << 16) | ((uint32_t)(sg / sa) << 8) | (uint32_t)(sb / sa);
            img->px[Y * W + X] = v;
        }
}
#include "objetos_hd.inc"
static bool ico_hd(art_img_t *img, art_icono_t ico, int escala, bool espejo)
{
    uint32_t pal[256];
    if (ico >= ICO_OBJ0) { /* objeto mágico: la forma de su familia, con sus dos colores */
        const int id = (int)ico - ICO_OBJ0;
        const objeto_info_t *o = objeto_info(id);
        if (!o || o->familia >= OFA_COUNT) return false;
        int v = (int)(hash(id, 7, 99) % 3);
        if (o->familia == OFA_DISCO) { /* moneda, anillo de sello o placa de reloj: cada uno con su forma */
            if (strstr(o->nombre, "Moneda")) v = 0; else if (strstr(o->nombre, "Anillo") || strstr(o->nombre, "sello")) v = 1; else v = 2;
        }
        const uint32_t c1 = A(o->c1), c2 = A(o->c2);
        memset(pal, 0, sizeof pal);
        pal[1] = oscurecer(c1, 0.55f); pal[2] = oscurecer(c1, 0.3f); pal[3] = c1; pal[4] = aclarar(c1, 0.3f); pal[5] = aclarar(c1, 0.6f);
        pal[6] = oscurecer(c2, 0.35f); pal[7] = c2; pal[8] = aclarar(c2, 0.5f); pal[9] = oscurecer(c1, 0.75f);
        const uint8_t *fx = OBJHD[o->familia][v].fix;
        for (int k = 0; k < OBJHD[o->familia][v].nfix && 10 + k < 256; k++) pal[10 + k] = A((uint32_t)fx[k * 3] << 16 | (uint32_t)fx[k * 3 + 1] << 8 | fx[k * 3 + 2]);
        ico_muestrear(img, pal, OBJHD[o->familia][v].idx, escala, espejo, HDI_LADO);
        return true;
    }
    for (unsigned t = 0; t < sizeof HD2 / sizeof HD2[0]; t++) { /* los íconos tiernos a 96x96 van primero */
        if (HD2[t].ico != (int)ico) continue;
        memset(pal, 0, sizeof pal);
        for (int k = 0; k < HD2[t].n; k++) {
            const uint8_t *p = HD2[t].pal + k * 4;
            pal[k + 1] = ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
        }
        ico_muestrear(img, pal, HD2[t].idx, escala, espejo, HD2_LADO);
        return true;
    }
    for (unsigned t = 0; t < sizeof HDI / sizeof HDI[0]; t++) {
        if (HDI[t].ico != (int)ico) continue;
        memset(pal, 0, sizeof pal);
        for (int k = 0; k < HDI[t].n; k++) {
            const uint8_t *p = HDI[t].pal + k * 4;
            pal[k + 1] = ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
        }
        ico_muestrear(img, pal, HDI[t].idx, escala, espejo, HDI_LADO);
        return true;
    }
    return false;
}
static void ico_render(art_img_t *img, art_icono_t ico, int escala, bool espejo)
{
    const int W = ICO_LADO * escala;
    if (s_propio[ico]) { /* dibujado a mano: se amplía tal cual */
        const int n = s_propio_lado[ico];
        img_preparar(img, W, W);
        if (!img->px) return;
        for (int Y = 0; Y < W; Y++)
            for (int X = 0; X < W; X++) {
                int sx = X * n / W, sy = Y * n / W;
                img->px[Y * W + X] = s_propio[ico][sy * n + (espejo ? n - 1 - sx : sx)];
            }
        return;
    }
    if (ico_hd(img, ico, escala, espejo)) return;
    const float k = k_de_escala(escala);
    const int L = (int)lroundf(ICO_LADO * k);
    uint32_t *buf = malloc((size_t)L * L * 4);
    if (!buf) return;
    ico_fabrica_k(ico, buf, k);
    if (s_estilo) ico_pulir(buf, L, k);
    lienzo_t l = {L, L, buf, k};
    ampliar(img, &l, (int)lroundf((float)escala / k), espejo);
    free(buf);
}

void art_icono_propio(art_icono_t ico, const uint32_t *px, int lado)
{
    if ((unsigned)ico >= ICO_COUNT) return;
    if (px) {
        if (lado != ART_ICONO_LADO && lado != ART_ICONO_LADO_V1) return;
        if (!s_propio[ico]) s_propio[ico] = malloc(ART_ICONO_LADO * ART_ICONO_LADO * 4);
        if (!s_propio[ico]) return;
        memcpy(s_propio[ico], px, (size_t)lado * lado * 4);
        s_propio_lado[ico] = (uint8_t)lado;
    } else {
        free(s_propio[ico]);
        s_propio[ico] = NULL;
    }
    /* lo que ya estaba en pantalla se redibuja en el mismo lugar de memoria (las imágenes lo siguen usando) */
    if (ico >= ICO_OBJ0) {
        for (int i = 0; i < OBJ_CACHE; i++)
            if (s_obj_cache[i].ico == ico && s_obj_cache[i].c.img.px) ico_render(&s_obj_cache[i].c.img, ico, s_obj_cache[i].c.escala, s_obj_cache[i].c.espejo);
        return;
    }
    for (int i = 0; i < ICO_CACHE; i++) {
        ico_cache_t *c = &s_iconos[ico][i];
        if (c->img.px) ico_render(&c->img, ico, c->escala, c->espejo);
    }
}

static const lv_image_dsc_t *icono_gen(art_icono_t ico, int escala, bool espejo)
{
    if ((unsigned)ico >= ICO_COUNT) return NULL;
    if (ico >= ICO_OBJ0) {
        for (int i = 0; i < OBJ_CACHE; i++)
            if (s_obj_cache[i].ico == ico && s_obj_cache[i].c.escala == escala && s_obj_cache[i].c.espejo == espejo && s_obj_cache[i].c.img.px)
                return &s_obj_cache[i].c.img.dsc;
        int libre = -1;
        for (int i = 0; i < OBJ_CACHE && libre < 0; i++) if (!s_obj_cache[i].c.img.px) libre = i;
        if (libre < 0) { libre = s_obj_turno; s_obj_turno = (s_obj_turno + 1) % OBJ_CACHE; }
        ico_cache_t *c = &s_obj_cache[libre].c;
        ico_render(&c->img, ico, escala, espejo);
        c->escala = (uint8_t)escala;
        c->espejo = espejo;
        s_obj_cache[libre].ico = (uint16_t)ico;
        return &c->img.dsc;
    }
    for (int i = 0; i < ICO_CACHE; i++)
        if (s_iconos[ico][i].escala == escala && s_iconos[ico][i].espejo == espejo && s_iconos[ico][i].img.px)
            return &s_iconos[ico][i].img.dsc;
    int libre = ICO_CACHE - 1; /* si se llenó, se reusa el último lugar */
    for (int i = 0; i < ICO_CACHE; i++)
        if (!s_iconos[ico][i].img.px) { libre = i; break; }
    ico_cache_t *c = &s_iconos[ico][libre];
    ico_render(&c->img, ico, escala, espejo);
    c->escala = (uint8_t)escala;
    c->espejo = espejo;
    return &c->img.dsc;
}

/* (v96) suelta los íconos guardados que ninguna imagen de la pantalla está usando (en_uso lo dice). Se usa cuando falta memoria:
 * el caché de íconos nunca se vaciaba y, con un rato de juego (todos los animales, sus pasos, mirando a cada lado, los objetos),
 * se comía la PSRAM; después malloc seguía en la RAM interna y la placa se colgaba. Lo que se suelta se vuelve a dibujar solo
 * la próxima vez que se pida. */
uint32_t art_iconos_soltar(bool (*en_uso)(const void *src))
{
    uint32_t libres = 0;
    for (int ico = 0; ico < ICO_OBJ0; ico++)
        for (int i = 0; i < ICO_CACHE; i++) {
            ico_cache_t *c = &s_iconos[ico][i];
            if (!c->img.px || (en_uso && en_uso(&c->img.dsc))) continue;
            libres += c->img.capacidad * 4u;
            art_img_free(&c->img);
            c->escala = 0; c->espejo = 0;
        }
    for (int i = 0; i < OBJ_CACHE; i++) {
        ico_cache_t *c = &s_obj_cache[i].c;
        if (!c->img.px || (en_uso && en_uso(&c->img.dsc))) continue;
        libres += c->img.capacidad * 4u;
        art_img_free(&c->img);
        c->escala = 0; c->espejo = 0;
    }
    return libres;
}
const lv_image_dsc_t *art_icono(art_icono_t ico, int escala) { return icono_gen(ico, escala, false); }
void art_icono_en(art_img_t *img, art_icono_t ico, int escala) { if ((unsigned)ico < ICO_COUNT) ico_render(img, ico, escala, false); }
const lv_image_dsc_t *art_icono_espejo(art_icono_t ico, int escala) { return icono_gen(ico, escala, true); }

/* ======================================================================
 * Fuente pixel 5x7 con tildes
 * ==================================================================== */

typedef struct {
    uint32_t cp;
    const char *filas; /* 7 filas de 5, separadas por espacio */
} glifo_t;

static const glifo_t FUENTE[] = {
    {0xAB, "..... ..... .#.#. #.#.. .#.#. ..... ....."}, {0xBB, "..... ..... #.#.. .#.#. #.#.. ..... ....."}, /* (v96) « y » de los diálogos */
    {'A', ".###. #...# #...# ##### #...# #...# #...#"}, {'B', "####. #...# #...# ####. #...# #...# ####."},
    {'C', ".###. #...# #.... #.... #.... #...# .###."}, {'D', "####. #...# #...# #...# #...# #...# ####."},
    {'E', "##### #.... #.... ####. #.... #.... #####"}, {'F', "##### #.... #.... ####. #.... #.... #...."},
    {'G', ".###. #...# #.... #.### #...# #...# .####"}, {'H', "#...# #...# #...# ##### #...# #...# #...#"},
    {'I', "###.. .#... .#... .#... .#... .#... ###.."}, {'J', "..### ...#. ...#. ...#. ...#. #..#. .##.."},
    {'K', "#...# #..#. #.#.. ##... #.#.. #..#. #...#"}, {'L', "#.... #.... #.... #.... #.... #.... #####"},
    {'M', "#...# ##.## #.#.# #.#.# #...# #...# #...#"}, {'N', "#...# ##..# #.#.# #..## #...# #...# #...#"},
    {'O', ".###. #...# #...# #...# #...# #...# .###."}, {'P', "####. #...# #...# ####. #.... #.... #...."},
    {'Q', ".###. #...# #...# #...# #.#.# #..#. .##.#"}, {'R', "####. #...# #...# ####. #.#.. #..#. #...#"},
    {'S', ".#### #.... #.... .###. ....# ....# ####."}, {'T', "##### ..#.. ..#.. ..#.. ..#.. ..#.. ..#.."},
    {'U', "#...# #...# #...# #...# #...# #...# .###."}, {'V', "#...# #...# #...# #...# #...# .#.#. ..#.."},
    {'W', "#...# #...# #...# #.#.# #.#.# #.#.# .#.#."}, {'X', "#...# #...# .#.#. ..#.. .#.#. #...# #...#"},
    {'Y', "#...# #...# .#.#. ..#.. ..#.. ..#.. ..#.."}, {'Z', "##### ....# ...#. ..#.. .#... #.... #####"},
    {'0', ".###. #...# #..## #.#.# ##..# #...# .###."}, {'1', ".#... ##... .#... .#... .#... .#... ###.."},
    {'2', ".###. #...# ....# ...#. ..#.. .#... #####"}, {'3', "####. ....# ....# .###. ....# ....# ####."},
    {'4', "...#. ..##. .#.#. #..#. ##### ...#. ...#."}, {'5', "##### #.... ####. ....# ....# #...# .###."},
    {'6', ".###. #.... #.... ####. #...# #...# .###."}, {'7', "##### ....# ...#. ..#.. .#... .#... .#..."},
    {'8', ".###. #...# #...# .###. #...# #...# .###."}, {'9', ".###. #...# #...# .#### ....# ....# .###."},
    {'!', "#.... #.... #.... #.... #.... ..... #...."}, {0xA1, "#.... ..... #.... #.... #.... #.... #...."},
    {'?', ".###. #...# ....# ...#. ..#.. ..... ..#.."}, {0xBF, "..#.. ..... ..#.. .#... #.... #...# .###."},
    {'_', "..... ..... ..... ..... ..... ..... #####"}, {'.', "..... ..... ..... ..... ..... ..... #...."}, {',', "..... ..... ..... ..... ..... .#... #...."},
    {':', "..... #.... ..... ..... ..... #.... ....."}, {'-', "..... ..... ..... ###.. ..... ..... ....."},
    {'+', "..... ..#.. ..#.. ##### ..#.. ..#.. ....."}, {'=', "..... ..... ####. ..... ####. ..... ....."}, /* (v97) */ {'/', "....# ...#. ...#. ..#.. .#... .#... #...."},
    {'%', "##..# ##.#. ...#. ..#.. .#... .#.## #..##"}, {'\'', "#.... #.... ..... ..... ..... ..... ....."},
    {'(', ".#... #.... #.... #.... #.... #.... .#..."}, {')', "#.... .#... .#... .#... .#... .#... #...."},
    {'<', "...#. ..#.. .#... #.... .#... ..#.. ...#."}, {'>', "#.... .#... ..#.. ...#. ..#.. .#... #...."},
    {'x', "..... ..... #...# .#.#. ..#.. .#.#. #...#"}, {'*', "..... #.#.# .###. ##### .###. #.#.# ....."},
    /* (1.0.13) símbolos para escribir claves de Wi-Fi */
    {'@', ".###. #...# #.### #.#.# #.##. #.... .####"},
    {'#', ".#.#. .#.#. ##### .#.#. ##### .#.#. .#.#."},
    {'$', "..#.. .#### #.#.. .###. ..#.# ####. ..#.."},
    {'&', ".##.. #..#. #.#.. .#... #.#.# #..#. .##.#"},
    {';', "..... .#... ..... ..... ..... .#... #...."},
    {'"', "#.#.. #.#.. ..... ..... ..... ..... ....."},
    {'[', "##... #.... #.... #.... #.... #.... ##..."},
    {']', "##... .#... .#... .#... .#... .#... ##..."},
    {'{', "..#.. .#... .#... #.... .#... .#... ..#.."},
    {'}', "#.... .#... .#... ..#.. .#... .#... #...."},
    {'~', "..... ..... .#..# #.##. ..... ..... ....."},
    {'^', "..#.. .#.#. #...# ..... ..... ..... ....."},
    {'|', "#.... #.... #.... #.... #.... #.... #...."},
    {'\\', "#.... .#... .#... ..#.. ...#. ...#. ....#"},
    {'`', "#.... .#... ..... ..... ..... ..... ....."},
    /* (1.0.13) minúsculas: solo se usan con art_texto_minusculas(true) (claves y nombres de redes); el resto del juego sigue en mayúsculas */
    {0x10000u + 'a', "..... ..... .###. ....# .#### #...# .####"},
    {0x10000u + 'b', "#.... #.... ####. #...# #...# #...# ####."},
    {0x10000u + 'c', "..... ..... .###. #.... #.... #...# .###."},
    {0x10000u + 'd', "....# ....# .#### #...# #...# #...# .####"},
    {0x10000u + 'e', "..... ..... .###. #...# ##### #.... .###."},
    {0x10000u + 'f', "..##. .#... ###.. .#... .#... .#... .#..."},
    {0x10000u + 'g', "..... .#### #...# #...# .#### ....# .###."},
    {0x10000u + 'h', "#.... #.... ####. #...# #...# #...# #...#"},
    {0x10000u + 'i', ".#... ..... ##... .#... .#... .#... ###.."},
    {0x10000u + 'j', "...#. ..... ..##. ...#. ...#. #..#. .##.."},
    {0x10000u + 'k', "#.... #.... #..#. #.#.. ##... #.#.. #..#."},
    {0x10000u + 'l', "##... .#... .#... .#... .#... .#... ###.."},
    {0x10000u + 'm', "..... ..... ##.#. #.#.# #.#.# #.#.# #.#.#"},
    {0x10000u + 'n', "..... ..... ####. #...# #...# #...# #...#"},
    {0x10000u + 'o', "..... ..... .###. #...# #...# #...# .###."},
    {0x10000u + 'p', "..... ####. #...# #...# ####. #.... #...."},
    {0x10000u + 'q', "..... .#### #...# #...# .#### ....# ....#"},
    {0x10000u + 'r', "..... ..... #.##. ##..# #.... #.... #...."},
    {0x10000u + 's', "..... ..... .#### #.... .###. ....# ####."},
    {0x10000u + 't', ".#... .#... ###.. .#... .#... .#..# ..##."},
    {0x10000u + 'u', "..... ..... #...# #...# #...# #..## .##.#"},
    {0x10000u + 'v', "..... ..... #...# #...# #...# .#.#. ..#.."},
    {0x10000u + 'w', "..... ..... #...# #...# #.#.# #.#.# .#.#."},
    {0x10000u + 'x', "..... ..... #...# .#.#. ..#.. .#.#. #...#"},
    {0x10000u + 'y', "..... #...# #...# #...# .#### ....# .###."},
    {0x10000u + 'z', "..... ..... ##### ...#. ..#.. .#... #####"},
};
#define N_GLIFOS (sizeof(FUENTE) / sizeof(FUENTE[0]))

enum { ACENTO_NADA, ACENTO_AGUDO, ACENTO_TILDE, ACENTO_DIERESIS };
static bool s_minusculas; /* (1.0.13) las minúsculas se ven como minúsculas (solo el Wi-Fi: claves y nombres de redes) */
void art_texto_minusculas(bool si) { s_minusculas = si; }

/* lee un carácter UTF-8 y lo normaliza a mayúscula + marca de acento */
static const char *leer_cp(const char *s, uint32_t *cp, int *acento)
{
    const unsigned char *u = (const unsigned char *)s;
    *acento = ACENTO_NADA;
    uint32_t c;
    if (u[0] < 0x80) { c = u[0]; s += 1; }
    else if ((u[0] & 0xE0) == 0xC0 && u[1]) { c = ((uint32_t)(u[0] & 0x1F) << 6) | (u[1] & 0x3F); s += 2; }
    else if ((u[0] & 0xF0) == 0xE0 && u[1] && u[2]) { c = ((uint32_t)(u[0] & 0x0F) << 12) | ((uint32_t)(u[1] & 0x3F) << 6) | (u[2] & 0x3F); s += 3; }
    else { c = '?'; s += 1; }
    if (c >= 'a' && c <= 'z') { if (s_minusculas) { *cp = 0x10000u + c; return s; } c -= 32; }
    switch (c) {
    case 0xE1: case 0xC1: c = 'A'; *acento = ACENTO_AGUDO; break;
    case 0xE9: case 0xC9: c = 'E'; *acento = ACENTO_AGUDO; break;
    case 0xED: case 0xCD: c = 'I'; *acento = ACENTO_AGUDO; break;
    case 0xF3: case 0xD3: c = 'O'; *acento = ACENTO_AGUDO; break;
    case 0xFA: case 0xDA: c = 'U'; *acento = ACENTO_AGUDO; break;
    case 0xFC: case 0xDC: c = 'U'; *acento = ACENTO_DIERESIS; break;
    case 0xF1: case 0xD1: c = 'N'; *acento = ACENTO_TILDE; break;
    case 0xD7: c = 'x'; break; /* signo por */
    default: break;
    }
    *cp = c;
    return s;
}

static const glifo_t *glifo(uint32_t cp)
{
    for (size_t i = 0; i < N_GLIFOS; i++)
        if (FUENTE[i].cp == cp) return &FUENTE[i];
    return NULL;
}

/* ancho real del glifo (sin columnas vacías a la derecha) */
static int ancho_glifo(const glifo_t *g)
{
    int w = 1;
    for (int f = 0; f < 7; f++)
        for (int c = 0; c < 5; c++)
            if (g->filas[f * 6 + c] == '#' && c + 1 > w) w = c + 1;
    return w;
}

#define FUENTE_ALTO 9   /* 2 filas de acento + 7 */
#define FUENTE_LINEA 11 /* con espacio entre líneas */

static int ancho_palabra(const char *s, const char **fin)
{
    int w = 0;
    while (*s && *s != ' ' && *s != '\n') {
        uint32_t cp; int ac;
        s = leer_cp(s, &cp, &ac);
        const glifo_t *g = glifo(cp);
        w += (g ? ancho_glifo(g) : 3) + 1;
    }
    if (fin) *fin = s;
    return w;
}

/* (v96) la letra un 5 % más grande: el texto se pinta a escala × ART_TEXTO_FACTOR con un filtro de caja (cada pixel de la
 * letra cubre 2,1 pixeles de pantalla; los bordes quedan suaves). En la pantalla de 466 px en 1,75" casi no se nota el suavizado. */
#define ART_TEXTO_FACTOR 1.05f
static float s_texto_factor = ART_TEXTO_FACTOR;
void art_texto_factor(float f) { s_texto_factor = f > 0.5f ? f : 1.0f; }
/* (v97) la letra de escala 1 era de 9 pixeles: ilegible en la placa («APAGADO» bajo los botones). Ahora se pinta a 1,55
 * (1,3 si no cabe y se vuelve a pintar sin el 5 %), con bordes suaves */
static float esc_ef(int escala) { return escala == 1 ? (s_texto_factor == 1.0f ? 1.3f : 1.55f) : (float)escala * s_texto_factor; }
static int texto_px(int n, int escala) { return (int)ceilf((float)n * esc_ef(escala) - 0.01f); }
static void ampliar_suave(art_img_t *dst, const lienzo_t *src, float f, uint32_t color)
{
    const int W = (int)ceilf((float)src->w * f - 0.01f), H = (int)ceilf((float)src->h * f - 0.01f);
    img_preparar(dst, W, H);
    if (!dst->px) return;
    const float inv = 1.0f / f;
    for (int y = 0; y < H; y++) {
        const float y0 = (float)y * inv, y1 = (float)(y + 1) * inv;
        const int sy0 = (int)y0, sy1 = (int)ceilf(y1) - 1;
        for (int x = 0; x < W; x++) {
            const float x0 = (float)x * inv, x1 = (float)(x + 1) * inv;
            const int sx0 = (int)x0, sx1 = (int)ceilf(x1) - 1;
            float cub = 0.0f;
            for (int sy = sy0; sy <= sy1 && sy < src->h; sy++) {
                const float hy = fminf(y1, (float)(sy + 1)) - fmaxf(y0, (float)sy);
                if (hy <= 0) continue;
                for (int sx = sx0; sx <= sx1 && sx < src->w; sx++) {
                    if (!src->p[sy * src->w + sx]) continue;
                    const float hx = fminf(x1, (float)(sx + 1)) - fmaxf(x0, (float)sx);
                    if (hx > 0) cub += hx * hy;
                }
            }
            cub *= f * f; /* fracción del pixel de pantalla que cubre la letra */
            if (cub < 0.06f) continue;
            const uint32_t al = cub >= 0.94f ? 255u : (uint32_t)(cub * 255.0f);
            dst->px[y * W + x] = (al << 24) | (color & 0xFFFFFFu);
        }
    }
}
/* (v97) textos de los paneles en A8 (un byte por pixel: solo la transparencia; el color lo pone la imagen con image_recolor).
 * Ocupan la cuarta parte: el diario con sus 60 renglones o una historia del Libro pedían cerca de 1 MB de PSRAM en ARGB y,
 * con la memoria justa, la placa se quedaba sin RAM. */
static bool s_texto_a8;
void art_texto_a8(bool si) { s_texto_a8 = si; }
static void img_preparar_a8(art_img_t *img, int w, int h)
{
    const uint32_t n = (uint32_t)(w * h), u32 = (n + 3u) / 4u; /* capacidad contada en grupos de 4 bytes, como las ARGB */
    if (u32 > img->capacidad) {
        if (img->px) { lv_image_cache_drop(&img->dsc); g_art_bytes -= img->capacidad * 4u; }
        free(img->px);
        img->px = malloc((size_t)u32 * 4u);
        img->capacidad = img->px ? u32 : 0;
        g_art_bytes += img->capacidad * 4u;
    }
    if (!img->px) return;
    memset(img->px, 0, n);
    lv_image_cache_drop(&img->dsc);
    memset(&img->dsc, 0, sizeof(img->dsc));
    img->dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
    img->dsc.header.cf = LV_COLOR_FORMAT_A8;
    img->dsc.header.w = (uint32_t)w;
    img->dsc.header.h = (uint32_t)h;
    img->dsc.header.stride = (uint32_t)w;
    img->dsc.data_size = n;
    img->dsc.data = (const uint8_t *)img->px;
}
static void texto_a8(art_img_t *dst, const lienzo_t *src, float f)
{
    const int fi = (int)lroundf(f);
    const bool entero = fabsf(f - (float)fi) < 0.001f && fi >= 1;
    const int W = entero ? src->w * fi : (int)ceilf((float)src->w * f - 0.01f), H = entero ? src->h * fi : (int)ceilf((float)src->h * f - 0.01f);
    img_preparar_a8(dst, W, H);
    if (!dst->px) return;
    uint8_t *o = (uint8_t *)dst->px;
    if (entero) { /* pixel exacto */
        for (int y = 0; y < H; y++)
            for (int x = 0; x < W; x++) o[y * W + x] = src->p[(y / fi) * src->w + x / fi] ? 255u : 0u;
        return;
    }
    const float inv = 1.0f / f;
    for (int y = 0; y < H; y++) {
        const float y0 = (float)y * inv, y1 = (float)(y + 1) * inv;
        const int sy0 = (int)y0, sy1 = (int)ceilf(y1) - 1;
        for (int x = 0; x < W; x++) {
            const float x0 = (float)x * inv, x1 = (float)(x + 1) * inv;
            const int sx0 = (int)x0, sx1 = (int)ceilf(x1) - 1;
            float cub = 0.0f;
            for (int sy = sy0; sy <= sy1 && sy < src->h; sy++) {
                const float hy = fminf(y1, (float)(sy + 1)) - fmaxf(y0, (float)sy);
                if (hy <= 0) continue;
                for (int sx = sx0; sx <= sx1 && sx < src->w; sx++) {
                    if (!src->p[sy * src->w + sx]) continue;
                    const float hx = fminf(x1, (float)(sx + 1)) - fmaxf(x0, (float)sx);
                    if (hx > 0) cub += hx * hy;
                }
            }
            cub *= f * f;
            if (cub < 0.06f) continue;
            o[y * W + x] = cub >= 0.94f ? 255u : (uint8_t)(cub * 255.0f);
        }
    }
}
int art_texto_ancho(const char *s, int escala)
{
    int w = 0, max = 0;
    while (*s) {
        if (*s == '\n') { if (w > max) max = w; w = 0; s++; continue; }
        if (*s == ' ') { w += 3; s++; continue; }
        const char *fin;
        w += ancho_palabra(s, &fin);
        s = fin;
    }
    if (w > max) max = w;
    return max > 0 ? texto_px(max - 1, escala) : 0;
}

static bool s_texto_centrado; /* cada renglón centrado (ver art_texto_centrado) */
void art_texto(art_img_t *dst, const char *utf8, uint32_t color, int escala, int ancho_max_px)
{
    /* primera pasada: cortar en líneas y medir */
    int max_px = ancho_max_px > 0 ? (int)((float)ancho_max_px / esc_ef(escala)) : 100000;
    int lineas = 1, x = 0, ancho = 0, lw[64] = {0};
    for (const char *s = utf8; *s;) {
        if (*s == '\n') { lineas++; x = 0; s++; continue; }
        if (*s == ' ') { x += 3; s++; continue; }
        const char *fin;
        int w = ancho_palabra(s, &fin);
        if (x > 0 && x + w - 1 > max_px) { lineas++; x = 0; }
        x += w;
        if (x - 1 > ancho) ancho = x - 1;
        if (lineas <= 64) lw[lineas - 1] = x - 1;
        s = fin;
    }
    if (ancho < 1) ancho = 1;
    int alto = lineas * FUENTE_LINEA - (FUENTE_LINEA - FUENTE_ALTO);

    uint32_t *buf = calloc((size_t)ancho * alto, 4);
    if (!buf) return;
    lienzo_t l = {ancho, alto, buf, 1};
    uint32_t c = A(color);
    int fila = 0;
    x = 0;
    int off = s_texto_centrado ? (ancho - lw[0]) / 2 : 0; /* corrimiento del renglón (centrado) */
    for (const char *s = utf8; *s;) {
        if (*s == '\n') { fila++; x = 0; off = s_texto_centrado && fila < 64 ? (ancho - lw[fila]) / 2 : 0; s++; continue; }
        if (*s == ' ') { x += 3; s++; continue; }
        const char *fin;
        int w = ancho_palabra(s, &fin);
        if (x > 0 && x + w - 1 > max_px) { fila++; x = 0; off = s_texto_centrado && fila < 64 ? (ancho - lw[fila]) / 2 : 0; }
        while (s < fin) {
            uint32_t cp; int ac;
            s = leer_cp(s, &cp, &ac);
            const glifo_t *g = glifo(cp);
            int oy = fila * FUENTE_LINEA;
            if (g) {
                int gw = ancho_glifo(g);
                for (int f = 0; f < 7; f++)
                    for (int k = 0; k < 5; k++)
                        if (g->filas[f * 6 + k] == '#') lz_set(&l, off + x + k, oy + 2 + f, c);
                int mid = off + x + gw / 2;
                if (ac == ACENTO_AGUDO) { lz_set(&l, mid + 1, oy, c); lz_set(&l, mid, oy + 1, c); }
                if (ac == ACENTO_TILDE) {
                    lz_set(&l, mid - 1, oy + 1, c); lz_set(&l, mid, oy, c); lz_set(&l, mid + 1, oy + 1, c); lz_set(&l, mid + 2, oy, c);
                }
                if (ac == ACENTO_DIERESIS) { lz_set(&l, mid - 1, oy + 1, c); lz_set(&l, mid + 1, oy + 1, c); }
                x += gw + 1;
            } else {
                x += 4;
            }
        }
    }
    const float ef = esc_ef(escala);
    if (s_texto_a8) texto_a8(dst, &l, ef);
    else if (fabsf(ef - (float)escala) < 0.001f) ampliar(dst, &l, escala, false);
    else ampliar_suave(dst, &l, ef, color);
    free(buf);
}
void art_texto_centrado(art_img_t *dst, const char *utf8, uint32_t color, int escala, int ancho_max_px)
{
    s_texto_centrado = true;
    art_texto(dst, utf8, color, escala, ancho_max_px);
    s_texto_centrado = false;
}

/* ======================================================================
 * El mundo: pradera con estanque, árboles, sendero y decoraciones
 * ==================================================================== */

#define M ART_MUNDO
#define K_MUNDO 1.5f /* el mundo se dibuja en 234x234 pixeles reales y se ve a 2x2: más fino que los 156x156 de antes */

/* paleta del mundo: cambia con la estación */
typedef struct {
    uint32_t pasto, pasto_o, pasto_c, mata, senda, senda_o, piedrita, agua, agua_o, agua_c, orilla;
    uint32_t copa, copa_c, copa_o, borde_verde, tronco, tronco_o, roca, roca_c, roca_o, roca_b, flor_extra;
    int flores;
    bool hojas_caidas, nieve, hielo;
} paleta_mundo_t;

static paleta_mundo_t W;
/* estilo del mundo: 0 el de siempre (pastel), 1 el nuevo (más sobrio: verdes profundos, más tonos, sombras) */

#define PASTO W.pasto
#define PASTO_O W.pasto_o
#define PASTO_C W.pasto_c
#define PASTO_MATA W.mata
#define SENDA W.senda
#define SENDA_O W.senda_o
#define PIEDRITA W.piedrita
#define AGUA W.agua
#define AGUA_O W.agua_o
#define AGUA_C W.agua_c
#define ORILLA W.orilla
#define COPA W.copa
#define COPA_C W.copa_c
#define COPA_O W.copa_o
#define BORDE_VERDE W.borde_verde
#define TRONCO W.tronco
#define TRONCO_O W.tronco_o
#define ROCA W.roca
#define ROCA_C W.roca_c
#define ROCA_O W.roca_o
#define ROCA_B W.roca_b

static void art_fondo_invalidar(void);
static void aplicar_estacion(art_estacion_t e)
{
    /* base: verano */
    W = (paleta_mundo_t){
        A(0x86CF6B), A(0x72BC5C), A(0xA6E184), A(0x5FA84F), A(0xEAD29A), A(0xCDB079), A(0xB79C6B),
        A(0x5CB7EA), A(0x3D93D6), A(0xA9E2FF), A(0xF2E3B3),
        A(0x5BAE5B), A(0x86CC6B), A(0x3F8A4A), A(0x2D5E3C), A(0x8B5E3C), A(0x6A442B),
        A(0xB6B1C6), A(0xD8D4E4), A(0x8A84A0), A(0x5C5670), A(0xFFD45C), 200, false, false, false,
    };
    switch (e) {
    case ESTACION_ART_PRIMAVERA:
        W.pasto = A(0x96D977); W.pasto_o = A(0x7FC765); W.pasto_c = A(0xB8EA96);
        W.copa = A(0x6CC06A); W.copa_c = A(0xFFB8D8); W.copa_o = A(0x4C9A52); /* árboles en flor */
        W.flor_extra = A(0xFF8FC8);
        W.flores = 380;
        break;
    case ESTACION_ART_VERANO:
        W.pasto = A(0x7CCB5A); W.pasto_o = A(0x66B54A); W.pasto_c = A(0xA3DE73);
        W.copa = A(0x4FA552); W.copa_c = A(0x79C462); W.copa_o = A(0x357D41);
        W.flores = 160;
        break;
    case ESTACION_ART_OTONO:
        W.pasto = A(0xB7C46A); W.pasto_o = A(0xA0AE57); W.pasto_c = A(0xCFD685); W.mata = A(0x8C9A48);
        W.copa = A(0xE88A3A); W.copa_c = A(0xFFC14D); W.copa_o = A(0xC1502E); W.borde_verde = A(0x6E3A26);
        W.flor_extra = A(0xE86A3A);
        W.flores = 40;
        W.hojas_caidas = true;
        break;
    case ESTACION_ART_INVIERNO:
        W.pasto = A(0xEEF4FB); W.pasto_o = A(0xD6E2F0); W.pasto_c = A(0xFFFFFF); W.mata = A(0xBFD0E3);
        W.senda = A(0xD9D2C6); W.senda_o = A(0xB9AFA0); W.piedrita = A(0x9D927F);
        W.agua = A(0xCBE8F7); W.agua_o = A(0x9FCBE6); W.agua_c = A(0xFFFFFF); W.orilla = A(0xE4ECF5);
        W.copa = A(0x3E7A5A); W.copa_c = A(0xFFFFFF); W.copa_o = A(0x2C5B45); W.borde_verde = A(0x24443A);
        W.flores = 0;
        W.nieve = true;
        W.hielo = true;
        break;
    default: break;
    }
    if (s_estilo && e != ESTACION_ART_INVIERNO) { /* estilo nuevo: verdes más profundos y menos pastel */
        if (e == ESTACION_ART_OTONO) {
            W.pasto = A(0x8E9A4C); W.pasto_o = A(0x76833E); W.pasto_c = A(0xA6AE5E); W.mata = A(0x5E6A30);
            W.copa = A(0xC8702E); W.copa_c = A(0xE8A040); W.copa_o = A(0x9A4A24); W.borde_verde = A(0x4A2618);
        } else {
            W.pasto = A(0x62A44A); W.pasto_o = A(0x52923F); W.pasto_c = A(0x78B856); W.mata = A(0x3C7232);
            W.copa = A(0x3F8A40); W.copa_c = A(0x5CA84C); W.copa_o = A(0x2C6634); W.borde_verde = A(0x1C3A26);
            if (e == ESTACION_ART_PRIMAVERA) { W.copa_c = A(0x6CB85A); W.flor_extra = A(0xF4A6C6); }
        }
        W.tronco = A(0x7A4A2A); W.tronco_o = A(0x4A2C1A);
        W.senda = A(0xC4A472); W.senda_o = A(0x9C7E50); W.piedrita = A(0x8A7656);
        W.agua = A(0x4FA6D8); W.agua_o = A(0x2E78B4); W.agua_c = A(0x9AD4F2); W.orilla = A(0x8C7A52);
        W.roca = A(0x9A98A8); W.roca_c = A(0xC2C0CE); W.roca_o = A(0x6E6C80); W.roca_b = A(0x3C3A4A);
        W.flores = W.flores * 2 / 5;
    }
}
void art_set_estilo(int estilo)
{
    if (estilo == s_estilo) return;
    s_estilo = estilo;
    art_fondo_invalidar();
}

static void sombra(lienzo_t *l, float cx, float cy, float rx, float ry)
{
    float k = KL(l);
    for (int Y = (int)floorf((cy - ry) * k); Y <= (int)floorf((cy + ry) * k); Y++)
        for (int X = (int)floorf((cx - rx) * k); X <= (int)floorf((cx + rx) * k); X++) {
            float dx = ((X + 0.5f) / k - cx) / rx, dy = ((Y + 0.5f) / k - cy) / ry;
            if (dx * dx + dy * dy <= 1.0f) lz_real(l, X, Y, oscurecer(lz_rget(l, X, Y), 0.18f));
        }
}

static void arbol2(lienzo_t *l, float cx, float cy, float r, uint32_t semilla);
static void arbol(lienzo_t *l, float cx, float cy, float r, uint32_t semilla)
{
    if (s_estilo) { arbol2(l, cx, cy, r, semilla); return; }
    sombra(l, cx + 2, cy + r * 1.4f, r * 0.9f, r * 0.3f);
    rect(l, (int)cx - 2, (int)(cy + r * 0.5f), 5, (int)(r * 0.95f) + 1, TRONCO_O);
    rect(l, (int)cx - 1, (int)(cy + r * 0.5f), 3, (int)(r * 0.95f), TRONCO);
    circulo(l, cx, cy, r + 1, BORDE_VERDE);
    circulo(l, cx - r * 0.45f, cy + r * 0.25f, r * 0.62f + 1, BORDE_VERDE);
    circulo(l, cx + r * 0.45f, cy + r * 0.25f, r * 0.62f + 1, BORDE_VERDE);
    circulo(l, cx, cy, r, COPA);
    circulo(l, cx - r * 0.45f, cy + r * 0.25f, r * 0.62f, COPA);
    circulo(l, cx + r * 0.45f, cy + r * 0.25f, r * 0.62f, COPA);
    /* sombra y luz con tramado */
    float k = KL(l);
    for (int y = (int)((cy - r - 1) * k); y <= (int)((cy + r + 2) * k); y++)
        for (int x = (int)((cx - r - 2) * k); x <= (int)((cx + r + 2) * k); x++) {
            if (lz_rget(l, x, y) != COPA) continue;
            float dx = ((x + 0.5f) / k - cx) / r, dy = ((y + 0.5f) / k - cy) / r;
            float luz = -dx * 0.6f - dy * 0.8f;
            if (W.nieve) {
                if (dy < -0.35f + 0.25f * sinf(dx * 5.0f + semilla)) lz_real(l, x, y, COPA_C);
                else if (luz < -0.4f) lz_real(l, x, y, COPA_O);
                continue;
            }
            uint32_t medio_c = mezclar(COPA, COPA_C, 0.5f), medio_o = mezclar(COPA, COPA_O, 0.5f);
            if (luz > 0.5f || (luz > 0.38f && ((x + y) & 1))) lz_real(l, x, y, COPA_C);
            else if (luz > 0.22f) lz_real(l, x, y, ((x + y) & 1) ? medio_c : COPA);
            else if (luz < -0.6f || (luz < -0.48f && ((x + y) & 1))) lz_real(l, x, y, COPA_O);
            else if (luz < -0.3f) lz_real(l, x, y, ((x + y) & 1) ? medio_o : COPA);
            else if ((hash(x, y, semilla) & 31) == 0) lz_real(l, x, y, COPA_C);
            else if ((hash(x / 2, y / 2, semilla + 3) & 15) == 0) lz_real(l, x, y, medio_c);
            if (W.hojas_caidas && (hash(x, y, semilla + 9) & 15) == 0) lz_real(l, x, y, A(0xD9452E));
        }
}


/* ---- estilo nuevo: copas grumosas con muchos tonos, sombra que pesa, pinos, pasto con matitas ---- */
static void copa_grumosa(lienzo_t *l, float cx, float cy, float r, uint32_t semilla, uint32_t base, uint32_t luz, uint32_t osc)
{
    static const float B[7][3] = {{0, 0.05f, 0.72f}, {-0.52f, 0.22f, 0.58f}, {0.52f, 0.22f, 0.58f}, {-0.28f, -0.42f, 0.54f},
                                  {0.32f, -0.40f, 0.54f}, {0.0f, 0.48f, 0.56f}, {0.0f, -0.62f, 0.4f}};
    for (int b = 0; b < 7; b++) circulo(l, cx + B[b][0] * r, cy + B[b][1] * r, B[b][2] * r + 1.0f, BORDE_VERDE);
    for (int b = 0; b < 7; b++) circulo(l, cx + B[b][0] * r, cy + B[b][1] * r, B[b][2] * r, base);
    const float k = KL(l);
    const uint32_t medio_c = mezclar(base, luz, 0.5f), medio_o = mezclar(base, osc, 0.5f), sombra_p = oscurecer(osc, 0.25f);
    for (int Y = (int)((cy - r * 1.1f) * k); Y <= (int)((cy + r * 1.1f) * k); Y++)
        for (int X = (int)((cx - r * 1.2f) * k); X <= (int)((cx + r * 1.2f) * k); X++) {
            if (lz_rget(l, X, Y) != base) continue;
            float px = (X + 0.5f) / k, py = (Y + 0.5f) / k;
            /* el grumo que manda aquí: el más profundo */
            int mejor = 0; float prof = -9;
            for (int b = 0; b < 7; b++) {
                float rb = B[b][2] * r, d = hypotf(px - (cx + B[b][0] * r), py - (cy + B[b][1] * r)) / rb;
                if (1 - d > prof) { prof = 1 - d; mejor = b; }
            }
            float rb = B[mejor][2] * r, dx = (px - (cx + B[mejor][0] * r)) / rb, dy = (py - (cy + B[mejor][1] * r)) / rb;
            float lz = -dx * 0.55f - dy * 0.85f, glob = (py - cy) / r; /* luz de arriba a la izquierda */
            bool d2 = ((X + Y) & 1) != 0;
            uint32_t c = base;
            if (lz > 0.62f) c = luz;
            else if (lz > 0.42f) c = d2 ? luz : medio_c;
            else if (lz > 0.22f) c = d2 ? medio_c : base;
            else if (lz < -0.62f || prof < 0.12f) c = osc;
            else if (lz < -0.38f) c = d2 ? osc : medio_o;
            if (glob > 0.55f && c != luz) c = d2 ? sombra_p : osc;             /* abajo, la copa se oscurece */
            if ((hash(X, Y, semilla) & 63) == 0 && c != osc) c = luz;          /* hojitas sueltas */
            if (W.hojas_caidas && (hash(X, Y, semilla + 9) & 31) == 0) c = A(0xD9452E);
            if (W.nieve && dy < -0.3f) c = COPA_C;
            lz_real(l, X, Y, c);
        }
}
static void arbol2(lienzo_t *l, float cx, float cy, float r, uint32_t semilla)
{
    sombra(l, cx + 3, cy + r * 1.35f, r * 1.15f, r * 0.42f);
    sombra(l, cx + 2, cy + r * 1.35f, r * 0.7f, r * 0.25f);
    /* tronco con raíces */
    rect(l, (int)cx - 2, (int)(cy + r * 0.5f), 5, (int)(r * 0.9f) + 1, TRONCO_O);
    rect(l, (int)cx - 1, (int)(cy + r * 0.5f), 2, (int)(r * 0.9f), TRONCO);
    triangulo(l, cx - 5, cy + r * 1.42f, cx - 1, cy + r * 1.42f, cx - 1, cy + r * 1.1f, TRONCO_O);
    triangulo(l, cx + 6, cy + r * 1.42f, cx + 2, cy + r * 1.42f, cx + 2, cy + r * 1.1f, TRONCO_O);
    copa_grumosa(l, cx, cy, r, semilla, COPA, COPA_C, COPA_O);
}
static void arbusto2(lienzo_t *l, float cx, float cy, float r)
{
    sombra(l, cx + 1.5f, cy + r * 0.75f, r * 1.15f, r * 0.42f);
    copa_grumosa(l, cx, cy - r * 0.1f, r * 0.95f, (uint32_t)(cx * 7 + cy), COPA, COPA_C, COPA_O);
    if (!W.nieve && !W.hojas_caidas) { lz_set(l, (int)(cx + r * 0.4f), (int)(cy - r * 0.2f), A(0xE86A8A)); lz_set(l, (int)(cx - r * 0.5f), (int)cy, A(0xE86A8A)); }
}
static void pino(lienzo_t *l, float cx, float by, float h)
{
    /* los pinos no cambian de color en otoño */
    const bool verde = !W.hojas_caidas;
    const uint32_t base = verde ? mezclar(COPA, COPA_O, 0.45f) : A(0x2F6A3C), luz = verde ? COPA : A(0x3F8448), osc = verde ? oscurecer(COPA_O, 0.15f) : A(0x1F4A2C);
    sombra(l, cx + 2, by + 0.5f, h * 0.36f, h * 0.12f);
    rect(l, (int)cx - 1, (int)(by - h * 0.18f), 3, (int)(h * 0.2f), TRONCO_O);
    for (int t = 0; t < 3; t++) { /* tres pisos, de abajo hacia arriba */
        float y0 = by - h * 0.14f - t * h * 0.24f, w = h * (0.34f - t * 0.08f), alto = h * 0.42f;
        triangulo(l, cx - w - 1, y0 + 1, cx + w + 1, y0 + 1, cx, y0 - alto - 1, BORDE_VERDE);
    }
    for (int t = 0; t < 3; t++) {
        float y0 = by - h * 0.14f - t * h * 0.24f, w = h * (0.34f - t * 0.08f), alto = h * 0.42f;
        triangulo(l, cx - w, y0, cx + w, y0, cx, y0 - alto, base);
    }
    const float k = KL(l);
    for (int Y = (int)((by - h) * k); Y <= (int)(by * k); Y++)
        for (int X = (int)((cx - h * 0.4f) * k); X <= (int)((cx + h * 0.4f) * k); X++) {
            if (lz_rget(l, X, Y) != base) continue;
            float dx = ((X + 0.5f) / k - cx) / (h * 0.34f);
            bool d2 = ((X + Y) & 1) != 0;
            uint32_t c = base;
            if (dx < -0.35f) c = d2 ? luz : base;
            else if (dx > 0.3f) c = d2 ? osc : base;
            if (W.nieve && (hash(X, Y, 5) & 3) == 0) c = COPA_C;
            lz_real(l, X, Y, c);
        }
}
static void barro_suelo(lienzo_t *l, float cx, float cy, float rx, float ry, uint32_t semilla)
{
    if (W.nieve) return;
    const float k = KL(l);
    const uint32_t c0 = A(0xA0784A), c1 = A(0x84603A), c2 = A(0xB89262);
    for (int Y = (int)((cy - ry - 2) * k); Y <= (int)((cy + ry + 2) * k); Y++)
        for (int X = (int)((cx - rx - 2) * k); X <= (int)((cx + rx + 2) * k); X++) {
            float dx = ((X + 0.5f) / k - cx) / rx, dy = ((Y + 0.5f) / k - cy) / ry;
            float d = dx * dx + dy * dy + ((float)(hash(X / 3, Y / 3, semilla) & 15) / 15.0f - 0.5f) * 0.5f;
            if (d > 1.0f) continue;
            uint32_t c = d > 0.75f ? c1 : ((hash(X, Y, semilla + 1) & 7) == 0 ? c2 : c0);
            lz_real(l, X, Y, c);
        }
}
static void pasto2(lienzo_t *l, uint32_t semilla, int matas)
{
    const int T = (int)(2 * KL(l)) < 1 ? 1 : (int)(2 * KL(l));
    for (int y = 0; y < l->h; y++)
        for (int x = 0; x < l->w; x++) {
            /* manchones suaves (ruido de valores) con el borde tramado al azar, y alguna brizna suelta */
            const int G = T * 6;
            float fx = (float)(x % G) / G, fy = (float)(y % G) / G;
            int gx = x / G, gy = y / G;
            float a = (float)(hash(gx, gy, semilla + 2) & 255), b = (float)(hash(gx + 1, gy, semilla + 2) & 255);
            float c0 = (float)(hash(gx, gy + 1, semilla + 2) & 255), d = (float)(hash(gx + 1, gy + 1, semilla + 2) & 255);
            fx = fx * fx * (3 - 2 * fx); fy = fy * fy * (3 - 2 * fy);
            float v = (a + (b - a) * fx) * (1 - fy) + (c0 + (d - c0) * fx) * fy + (float)(hash(x, y, semilla + 5) & 31) - 16;
            uint32_t c = v < 70 ? PASTO_O : (v > 190 ? PASTO_C : PASTO);
            uint32_t h = hash(x / T, y / T, semilla);
            if ((h & 31) == 0) c = PASTO_O;
            lz_real(l, x, y, c);
        }
    const int n = matas * 3;
    for (int i = 0; i < n; i++) { /* matitas en V, dos tonos */
        int x = (int)(hash(i, 1, semilla + 10) % (unsigned)l->w), y = (int)(hash(i, 2, semilla + 10) % (unsigned)l->h);
        uint32_t o = PASTO_MATA, cl = PASTO_C;
        lz_real(l, x, y, o); lz_real(l, x - 1, y - 1, o); lz_real(l, x + 1, y - 1, o); lz_real(l, x - 2, y - 2, cl); lz_real(l, x + 2, y - 2, cl);
        if (hash(i, 4, semilla + 10) & 1) { lz_real(l, x, y - 1, o); lz_real(l, x, y - 2, cl); }
    }
}

static void arbusto(lienzo_t *l, float cx, float cy, float r)
{
    if (s_estilo) { arbusto2(l, cx, cy, r); return; }
    sombra(l, cx + 1, cy + r * 0.8f, r * 1.1f, r * 0.4f);
    circulo(l, cx - r * 0.5f, cy, r * 0.75f + 1, BORDE_VERDE);
    circulo(l, cx + r * 0.5f, cy, r * 0.75f + 1, BORDE_VERDE);
    circulo(l, cx, cy - r * 0.35f, r * 0.8f + 1, BORDE_VERDE);
    circulo(l, cx - r * 0.5f, cy, r * 0.75f, COPA);
    circulo(l, cx + r * 0.5f, cy, r * 0.75f, COPA);
    circulo(l, cx, cy - r * 0.35f, r * 0.8f, COPA);
    circulo(l, cx - r * 0.3f, cy - r * 0.6f, r * 0.3f, COPA_C);
    if (W.nieve) { elipse(l, cx, cy - r * 0.8f, r * 0.8f, r * 0.35f, COPA_C); elipse(l, cx - r * 0.6f, cy - r * 0.35f, r * 0.4f, r * 0.2f, COPA_C); }
    uint32_t baya = W.nieve ? A(0xE0405A) : A(0xFF8FB1);
    lz_set(l, (int)(cx + r * 0.4f), (int)(cy - r * 0.2f), baya); /* bayitas */
    lz_set(l, (int)(cx - r * 0.6f), (int)(cy + r * 0.1f), baya);
}

static void roca(lienzo_t *l, float cx, float cy, float r)
{
    elipse(l, cx, cy, r + 1, r * 0.75f + 1, ROCA_B);
    elipse(l, cx, cy, r, r * 0.75f, ROCA);
    elipse(l, cx + r * 0.25f, cy + r * 0.25f, r * 0.7f, r * 0.45f, ROCA_O);
    elipse(l, cx - r * 0.25f, cy - r * 0.2f, r * 0.45f, r * 0.3f, ROCA_C);
}

static void estanque(lienzo_t *l, float cx, float cy, float rx, float ry)
{
    elipse(l, cx, cy, rx + 2, ry + 2, ORILLA);
    elipse(l, cx, cy, rx, ry, AGUA_O);
    elipse(l, cx - 1, cy - 1, rx - 1.5f, ry - 1.5f, AGUA);
    for (int k = 0; k < 5; k++) { /* brillos */
        int x = (int)(cx - rx * 0.5f + (float)(hash(k, 1, 7) % (unsigned)(rx))), y = (int)(cy - ry * 0.4f + (float)(hash(k, 2, 7) % (unsigned)(ry)));
        rect(l, x, y, 3, 1, AGUA_C);
    }
    if (W.hielo) { /* estanque congelado: grietas en el hielo */
        trazo(l, cx - rx * 0.6f, cy - 2, cx - 2, cy + 1, 0.3f, AGUA_C);
        trazo(l, cx - 2, cy + 1, cx + rx * 0.5f, cy - 1, 0.3f, AGUA_C);
        trazo(l, cx - 2, cy + 1, cx, cy + ry * 0.6f, 0.3f, AGUA_C);
        return;
    }
    /* hoja de nenúfar con flor */
    elipse(l, cx + rx * 0.35f, cy + ry * 0.2f, 2.6f, 1.6f, A(0x4F9A55));
    lz_set(l, (int)(cx + rx * 0.35f), (int)(cy + ry * 0.2f) - 1, A(0xFF9ECF));
}

static void senda(lienzo_t *l, const float (*pts)[2], int n, float r)
{
    for (int i = 0; i + 1 < n; i++) trazo(l, pts[i][0], pts[i][1], pts[i + 1][0], pts[i + 1][1], r + 1, SENDA_O);
    for (int i = 0; i + 1 < n; i++) trazo(l, pts[i][0], pts[i][1], pts[i + 1][0], pts[i + 1][1], r, SENDA);
}

static void florcitas(lienzo_t *l, uint32_t semilla, int n)
{
    const uint32_t COL[] = {A(0xFF8FB1), A(0xFFD45C), A(0xFFFFFF), W.flor_extra};
    for (int i = 0; i < n; i++) {
        int x = (int)(hash(i, 3, semilla) % M), y = (int)(hash(i, 4, semilla) % M);
        uint32_t c = lz_get(l, x, y);
        if (c != PASTO && c != PASTO_O && c != PASTO_C) continue;
        uint32_t f = COL[hash(i, 5, semilla) % 4];
        float k = KL(l);
        int X = (int)(x * k), Y = (int)(y * k);
        if (hash(i, 6, semilla) & 1) { /* florcita de cuatro pétalos con centro */
            lz_real(l, X - 1, Y, f); lz_real(l, X + 1, Y, f); lz_real(l, X, Y - 1, f); lz_real(l, X, Y + 1, f);
            lz_real(l, X, Y, f == A(0xFFD45C) ? A(0xFFFFFF) : A(0xFFD45C)); lz_real(l, X, Y + 2, PASTO_MATA);
        } else { lz_real(l, X, Y, f); lz_real(l, X, Y + 1, PASTO_MATA); }
    }
}

/* decoraciones compradas (se dibujan con los mismos íconos, más grandes) */
/* un montoncito de nieve: lo que la nieve cubre (flores, hongos) */
static void monton_nieve(lienzo_t *l, int x, int y)
{
    elipse(l, x + 0.5f, y + 1.0f, 4.2f, 2.0f, A(0xC4D4E8));
    elipse(l, x + 0.5f, y + 0.2f, 3.6f, 2.4f, A(0xF4F8FF));
    lz_set(l, x - 1, y - 1, A(0xFFFFFF));
}
/* un ícono tierno (96x96) reducido a LxL por promedio de cada bloque (para pintarlo en un fondo) */
static bool hd2_a_pixeles(art_icono_t ico, uint32_t *buf, int L)
{
    for (unsigned t = 0; t < sizeof HD2 / sizeof HD2[0]; t++) {
        if (HD2[t].ico != (int)ico) continue;
        for (int j = 0; j < L; j++)
            for (int i = 0; i < L; i++) {
                const int x0 = i * HD2_LADO / L, x1 = (i + 1) * HD2_LADO / L, y0 = j * HD2_LADO / L, y1 = (j + 1) * HD2_LADO / L;
                int n = 0, tot = 0, r = 0, g = 0, b = 0;
                for (int y = y0; y < (y1 > y0 ? y1 : y0 + 1) && y < HD2_LADO; y++)
                    for (int x = x0; x < (x1 > x0 ? x1 : x0 + 1) && x < HD2_LADO; x++) {
                        tot++;
                        const int ix = HD2[t].idx[y * HD2_LADO + x];
                        if (!ix) continue;
                        const uint8_t *p = HD2[t].pal + (ix - 1) * 4;
                        r += p[1]; g += p[2]; b += p[3]; n++;
                    }
                buf[j * L + i] = n * 2 >= tot && n ? (0xFFu << 24) | ((uint32_t)(r / n) << 16) | ((uint32_t)(g / n) << 8) | (uint32_t)(b / n) : 0;
            }
        return true;
    }
    return false;
}

static void poner_icono(lienzo_t *l, art_icono_t ico, int x, int y, int escala)
{
    const float kw = KL(l), k = (float)escala * kw;
    const int L = (int)lroundf(ICO_LADO * k);
    uint32_t *buf = malloc((size_t)L * L * 4);
    if (!buf) return;
    if (ico == ICO_BANCO && hd2_a_pixeles(ico, buf, L)) { /* el banco nuevo, con su respaldo */ }
    else ico_fabrica_k(ico, buf, k);
    sombra(l, x + ICO_LADO * escala / 2.0f, y + ICO_LADO * escala - 1.0f, ICO_LADO * escala * 0.45f, 2.0f);
    const int X0 = (int)floorf((float)x * kw), Y0 = (int)floorf((float)y * kw);
    for (int j = 0; j < L; j++)
        for (int i = 0; i < L; i++)
            if (buf[j * L + i]) lz_real(l, X0 + i, Y0 + j, buf[j * L + i]);
    free(buf);
}

static void pintar_senderos(lienzo_t *l, uint32_t mascara);
static void pasto(lienzo_t *l, uint32_t semilla, int matas);
static void pintar_mundo(lienzo_t *l, uint32_t decor, uint32_t senderos)
{
    /* pasto con matas y variación */
    pasto(l, 11, 260);
    if (s_estilo) { /* el claro: un borde de pasto más alto alrededor, como un anillo */
        const float k = KL(l);
        for (int Y = 0; Y < l->h; Y++)
            for (int X = 0; X < l->w; X++) {
                float dx = (X / k - 78.0f) / 56.0f, dy = (Y / k - 90.0f) / 40.0f, d = sqrtf(dx * dx + dy * dy);
                d += ((float)(hash(X / 4, Y / 4, 3) & 15) / 15.0f - 0.5f) * 0.08f;
                if (d < 0.9f) lz_real(l, X, Y, aclarar(lz_rget(l, X, Y), 0.035f));
                else if (d < 1.0f && (hash(X, Y, 8) & 3) == 0) lz_real(l, X, Y, PASTO_MATA);
                else if (d >= 1.0f) lz_real(l, X, Y, oscurecer(lz_rget(l, X, Y), 0.06f));
            }
        barro_suelo(l, 10, 74, 7, 4, 21);
        barro_suelo(l, 122, 146, 8, 4, 22);
    }
    /* claro central un poco más luminoso, donde camina la mascota */
    for (int Y = 0; Y < l->h; Y++)
        for (int X = 0; X < l->w; X++) {
            float dx = (X / KL(l) - 78.0f) / 40.0f, dy = (Y / KL(l) - 86.0f) / 26.0f;
            if (dx * dx + dy * dy < 1.0f && ((X + Y) % 5 == 0)) lz_real(l, X, Y, aclarar(lz_rget(l, X, Y), 0.08f));
        }

    static const float SENDA_PTS[][2] = {{80, 160}, {82, 140}, {76, 124}, {78, 108}, {86, 100}};
    senda(l, SENDA_PTS, 5, 5.0f);
    static const float RAMAL[][2] = {{80, 128}, {100, 126}, {124, 132}, {160, 134}};
    senda(l, RAMAL, 4, 3.5f);
    for (int i = 0; i < 30; i++) { /* piedritas en el sendero */
        int x = (int)(hash(i, 7, 31) % M), y = (int)(hash(i, 8, 31) % M);
        if (lz_get(l, x, y) == SENDA) lz_set(l, x, y, PIEDRITA);
    }

    pintar_senderos(l, senderos);
    estanque(l, 40, 120, 19, 11);
    florcitas(l, 41, W.flores);
    if (W.hojas_caidas) /* hojas caídas en el pasto y el sendero */
        for (int i = 0; i < 220; i++) {
            int x = (int)(hash(i, 12, 51) % M), y = (int)(hash(i, 13, 51) % M);
            uint32_t c = lz_get(l, x, y);
            if (c == AGUA || c == AGUA_O) continue;
            static const uint32_t HOJAS[] = {A(0xE86A3A), A(0xF2A33A), A(0xC1502E), A(0xFFC14D)};
            uint32_t h = HOJAS[hash(i, 14, 51) & 3];
            lz_set(l, x, y, h);
            if (hash(i, 15, 51) & 1) lz_set(l, x + 1, y, h);
        }
    if (W.nieve) /* montoncitos de nieve con sombra azulada */
        for (int i = 0; i < 90; i++) {
            int x = (int)(hash(i, 16, 61) % M), y = (int)(hash(i, 17, 61) % M);
            if (lz_get(l, x, y) == PASTO) { rect(l, x, y, 3, 1, PASTO_O); lz_set(l, x + 1, y - 1, PASTO_C); }
        }

    roca(l, 116, 70, 4.5f);
    roca(l, 123, 74, 2.5f);
    roca(l, 118, 118, 3.0f);
    arbusto(l, 108, 142, 6);
    arbusto(l, 24, 76, 5);
    arbusto(l, 140, 84, 5);

    /* decoraciones compradas, antes de los árboles para que queden detrás */
    if (decor & (1u << 4)) poner_icono(l, ICO_CASITA, 88, 34, 2);   /* casita */
    if (decor & (1u << 0)) { if (W.nieve) { monton_nieve(l, 52, 92); monton_nieve(l, 60, 96); monton_nieve(l, 104, 98); } else { poner_icono(l, ICO_FLOR, 52, 92, 1); poner_icono(l, ICO_FLOR, 60, 96, 1); poner_icono(l, ICO_FLOR, 104, 98, 1); } }
    if (decor & (1u << 1)) { if (W.nieve) { monton_nieve(l, 62, 112); monton_nieve(l, 69, 116); } else { poner_icono(l, ICO_HONGO, 62, 112, 1); poner_icono(l, ICO_HONGO, 69, 116, 1); } }
    if (decor & (1u << 2)) poner_icono(l, ICO_FAROL, 112, 88, 2);   /* farol */
    if (decor & (1u << 3)) poner_icono(l, ICO_BANCO, 40, 66, 2);    /* banco */

    if (s_estilo) { /* pinos en el borde, detrás de los árboles grandes */
        pino(l, 58, 30, 18);
        pino(l, 112, 30, 16);
        pino(l, 150, 82, 15);
        pino(l, 6, 120, 16);
        pino(l, 34, 150, 14);
    }
    arbol(l, 26, 46, 15, 1);
    arbol(l, 133, 50, 14, 2);
    arbol(l, 146, 108, 11, 3);
    arbol(l, 10, 96, 9, 4);
}

/* ---------------------------------------------------------------------------
 * Lugares de cada zona (pies de la mascota, en coordenadas de pantalla).
 * El orden es el de lugar_t: claro, agua, árbol, rocas, sendero, refugio.
 * ------------------------------------------------------------------------- */
const int16_t ART_LUGAR_XY[ART_ESCENAS][6][2] = {
    {{233, 255}, {176, 302}, {104, 228}, {318, 232}, {256, 305}, {300, 190}}, /* prado  */
    {{233, 255}, {126, 250}, {296, 222}, {330, 262}, {240, 310}, {165, 200}}, /* bosque */
    {{233, 268}, {233, 214}, {122, 278}, {330, 266}, {262, 312}, {300, 214}}, /* lago   */
    {{233, 255}, {150, 282}, {150, 226}, {322, 222}, {262, 310}, {356, 204}}, /* colina */
    {{233, 250}, {150, 292}, {132, 212}, {330, 218}, {250, 314}, {300, 182}}, /* cueva  */
    {{236, 336}, {322, 318}, {296, 172}, {214, 256}, {124, 268}, {352, 262}}, /* isla: orilla, árbol viejo, círculo de piedras, muelle, faro */
    {{262, 258}, {182, 300}, {132, 250}, {326, 236}, {256, 318}, {207, 178}}, /* (v98) claro de Tilo: la poza, el bosque, las piedras, el sendero, la puerta de la casita */
    {{236, 236}, {150, 278}, {206, 152}, {170, 226}, {236, 322}, {290, 166}}, /* (v98) mirador: la grieta, la roca del cóndor, la piedra tibia, el lomo, la apacheta */
    {{220, 330}, {150, 330}, {130, 320}, {330, 330}, {233, 340}, {300, 282}}, /* casita por dentro: refugio = la cama */
};

static float wx(int esc, int l) { return (ART_LUGAR_XY[esc][l][0] + 1) / 3.0f; } /* pantalla -> mundo */
static float wy(int esc, int l) { return (ART_LUGAR_XY[esc][l][1] + 1) / 3.0f; }

static float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }

/* pasto en pixeles reales: manchones de tono y briznas finas de dos o tres hojas */
static void pasto(lienzo_t *l, uint32_t semilla, int matas)
{
    if (s_estilo) { pasto2(l, semilla, matas); return; }
    const int T = (int)(3 * KL(l)) < 1 ? 1 : (int)(3 * KL(l));
    const uint32_t medio = mezclar(PASTO, PASTO_C, 0.5f);
    for (int y = 0; y < l->h; y++)
        for (int x = 0; x < l->w; x++) {
            uint32_t h = hash(x / T, y / T, semilla);
            uint32_t c = (h & 7) == 0 ? PASTO_O : ((h & 7) == 1 ? PASTO_C : PASTO);
            if ((h & 7) > 1 && (hash(x, y, semilla + 1) & 31) == 0) c = medio;
            lz_real(l, x, y, c);
        }
    const int n = matas * 27 / 10;
    for (int i = 0; i < n; i++) {
        int x = (int)(hash(i, 1, semilla + 10) % (unsigned)l->w), y = (int)(hash(i, 2, semilla + 10) % (unsigned)l->h);
        uint32_t c = (hash(i, 3, semilla + 10) & 3) ? PASTO_MATA : mezclar(PASTO_MATA, PASTO_C, 0.35f);
        lz_real(l, x, y, c); lz_real(l, x - 1, y - 1, c); lz_real(l, x + 1, y - 1, c); lz_real(l, x, y - 2, c);
        if (hash(i, 4, semilla + 10) & 1) lz_real(l, x + 2, y - 2, c);
    }
}

static void hojas_caidas(lienzo_t *l, int n, uint32_t semilla)
{
    static const uint32_t HOJAS[] = {A(0xE86A3A), A(0xF2A33A), A(0xC1502E), A(0xFFC14D)};
    for (int i = 0; i < n; i++) {
        int x = (int)(hash(i, 12, semilla) % M), y = (int)(hash(i, 13, semilla) % M);
        uint32_t c = lz_get(l, x, y);
        if (c == AGUA || c == AGUA_O || c == AGUA_C) continue;
        uint32_t h = HOJAS[hash(i, 14, semilla) & 3];
        lz_set(l, x, y, h);
        if (hash(i, 15, semilla) & 1) lz_set(l, x + 1, y, h);
    }
}

static void nieve_suelo(lienzo_t *l, uint32_t semilla)
{
    for (int i = 0; i < 90; i++) {
        int x = (int)(hash(i, 16, semilla) % M), y = (int)(hash(i, 17, semilla) % M);
        if (lz_get(l, x, y) == PASTO) { rect(l, x, y, 3, 1, PASTO_O); lz_set(l, x + 1, y - 1, PASTO_C); }
    }
}

static void hongo(lienzo_t *l, int x, int y, uint32_t c)
{
    if (W.nieve) { /* en invierno la nieve lo cubre */
        elipse(l, x + 0.5f, y + 1.0f, 2.8f, 1.5f, A(0xC4D4E8));
        elipse(l, x + 0.5f, y + 0.4f, 2.4f, 1.6f, A(0xF4F8FF));
        return;
    }
    elipse(l, x + 0.5f, y + 0.5f, 1.8f, 1.1f, c);
    lz_set(l, x, y - 1, A(0xFFFFFF));
    rect(l, x, y + 1, 1, 2, A(0xFFF1D6));
}

/* bosque: árboles tupidos, un arroyo, troncos, hongos y helechos */
static void pintar_bosque(lienzo_t *l)
{
    uint32_t p0 = W.pasto, p1 = W.pasto_o, p2 = W.pasto_c;
    W.pasto = mezclar(W.pasto, A(0x2E6B3E), W.nieve ? 0.0f : 0.35f);
    W.pasto_o = mezclar(W.pasto_o, A(0x2E6B3E), W.nieve ? 0.0f : 0.35f);
    W.pasto_c = mezclar(W.pasto_c, A(0x2E6B3E), W.nieve ? 0.0f : 0.3f);
    pasto(l, 71, 360);
    if (!W.nieve) hojas_caidas(l, W.hojas_caidas ? 320 : 90, 73);
    else nieve_suelo(l, 74);
    /* arroyo por la izquierda */
    static const float ARROYO[][2] = {{54, 0}, {46, 34}, {38, 66}, {32, 96}, {16, 130}, {4, 158}};
    for (int i = 0; i < 5; i++) trazo(l, ARROYO[i][0], ARROYO[i][1], ARROYO[i + 1][0], ARROYO[i + 1][1], 4.6f, ORILLA);
    for (int i = 0; i < 5; i++) trazo(l, ARROYO[i][0], ARROYO[i][1], ARROYO[i + 1][0], ARROYO[i + 1][1], 3.2f, AGUA_O);
    for (int i = 0; i < 5; i++) trazo(l, ARROYO[i][0] - 0.6f, ARROYO[i][1], ARROYO[i + 1][0] - 0.6f, ARROYO[i + 1][1], 2.0f, AGUA);
    for (int k = 0; k < 14; k++) { int y = (int)(hash(k, 3, 75) % 150); int x = (int)(54 - y * 0.32f); lz_set(l, x, y, AGUA_C); lz_set(l, x + 1, y, AGUA_C); }
    static const float SENDA_PTS[][2] = {{82, 160}, {80, 132}, {78, 110}, {80, 96}};
    senda(l, SENDA_PTS, 4, 3.5f);
    /* tronco hueco (madriguera) y rocas con musgo */
    float tx = wx(1, 5), ty = wy(1, 5) - 4;
    elipse(l, tx, ty, 9.0f, 3.0f, TRONCO_O); elipse(l, tx, ty - 0.5f, 8.2f, 2.3f, TRONCO);
    elipse(l, tx - 7.6f, ty, 2.2f, 2.6f, A(0x3A2A1E)); trazo(l, tx - 4, ty - 1.5f, tx + 6, ty - 1.5f, 0.3f, TRONCO_O);
    roca(l, wx(1, 3) + 4, wy(1, 3) - 4, 4.5f); roca(l, wx(1, 3) - 3, wy(1, 3) - 2, 2.6f);
    for (int k = 0; k < 6; k++) lz_set(l, (int)wx(1, 3) + 1 + k, (int)wy(1, 3) - 7, W.nieve ? A(0xFFFFFF) : A(0x6FBF5A));
    /* hongos */
    static const int8_t HONGOS[][2] = {{66, 70}, {70, 73}, {98, 104}, {120, 110}, {58, 118}, {104, 40}};
    for (int k = 0; k < 6; k++) hongo(l, HONGOS[k][0], HONGOS[k][1], k & 1 ? A(0xE85D4A) : A(0xC98B5A));
    /* árboles: un gran árbol con hueco y muchos alrededor */
    if (s_estilo) { pino(l, 44, 26, 18); pino(l, 102, 22, 17); pino(l, 158, 90, 15); pino(l, 28, 150, 15); pino(l, 140, 142, 14); pino(l, 4, 92, 14); }
    arbol(l, 22, 22, 13, 11); arbol(l, 76, 12, 15, 12); arbol(l, 124, 16, 14, 13); arbol(l, 150, 60, 12, 14);
    arbol(l, 148, 112, 12, 15); arbol(l, 118, 150, 13, 16); arbol(l, 10, 64, 9, 17); arbol(l, 6, 112, 8, 18);
    arbol(l, wx(1, 2) + 2, wy(1, 2) - 18, 16, 19);
    elipse(l, wx(1, 2) + 2, wy(1, 2) - 5, 2.0f, 2.6f, A(0x3A2A1E)); /* hueco del tronco */
    arbusto(l, 46, 140, 6); arbusto(l, 132, 88, 5);
    W.pasto = p0; W.pasto_o = p1; W.pasto_c = p2;
}

/* lago: agua grande arriba, orilla de arena, juncos, nenúfares, un muelle y un sauce */
static void pintar_lago(lienzo_t *l)
{
    pasto(l, 81, 220);
    if (W.hojas_caidas) hojas_caidas(l, 120, 83);
    if (W.nieve) nieve_suelo(l, 84);
    float cx = 78, cy = 26, rx = 86, ry = 44;
    elipse(l, cx, cy, rx + 6, ry + 7, ORILLA);
    elipse(l, cx, cy, rx, ry, AGUA_O);
    elipse(l, cx - 2, cy - 2, rx - 4, ry - 4, AGUA);
    for (int k = 0; k < 18; k++) { /* brillos u ondas */
        int x = (int)(hash(k, 1, 85) % 140) + 8, y = (int)(hash(k, 2, 85) % 50) + 6;
        if (lz_get(l, x, y) == AGUA) rect(l, x, y, 3, 1, AGUA_C);
    }
    if (W.hielo) { /* lago congelado */
        trazo(l, 30, 30, 70, 48, 0.3f, AGUA_C); trazo(l, 70, 48, 120, 36, 0.3f, AGUA_C); trazo(l, 70, 48, 78, 64, 0.3f, AGUA_C);
    } else {
        static const int8_t NEN[][2] = {{40, 50}, {58, 58}, {120, 52}};
        for (int k = 0; k < 3; k++) { elipse(l, NEN[k][0], NEN[k][1], 2.6f, 1.6f, A(0x4F9A55)); lz_set(l, NEN[k][0], NEN[k][1] - 1, A(0xFF9ECF)); }
    }
    /* juncos */
    static const int8_t JUNCOS[][2] = {{14, 68}, {18, 70}, {138, 66}, {142, 64}, {146, 68}};
    for (int k = 0; k < 5; k++) {
        rect(l, JUNCOS[k][0], JUNCOS[k][1] - 6, 1, 7, A(0x5FA84F));
        rect(l, JUNCOS[k][0], JUNCOS[k][1] - 8, 1, 3, A(0x8B5E3C));
    }
    /* muelle de madera */
    float mx = wx(2, 5);
    rect(l, (int)mx - 3, 36, 7, 36, TRONCO_O);
    for (int y = 37; y < 71; y += 3) rect(l, (int)mx - 2, y, 5, 2, TRONCO);
    rect(l, (int)mx - 4, 70, 1, 3, TRONCO_O); rect(l, (int)mx + 4, 70, 1, 3, TRONCO_O);
    static const float SENDA_PTS[][2] = {{86, 160}, {84, 130}, {80, 104}, {78, 82}};
    senda(l, SENDA_PTS, 4, 3.5f);
    roca(l, wx(2, 3) + 4, wy(2, 3) - 4, 4.0f); roca(l, wx(2, 3) - 2, wy(2, 3) - 1, 2.2f);
    /* sauce con ramas que cuelgan */
    float sx = wx(2, 2) - 4, sy = wy(2, 2) - 20;
    if (s_estilo) { pino(l, 8, 128, 15); pino(l, 152, 130, 14); pino(l, 140, 150, 12); }
    arbol(l, sx, sy, 14, 21);
    for (int k = 0; k < 9; k++) {
        float x = sx - 12 + k * 3.0f;
        trazo(l, x, sy + 2, x + 0.5f, sy + 14 + (float)(hash(k, 9, 22) % 4), 0.4f, W.nieve ? COPA_C : COPA_O);
    }
    arbusto(l, 140, 112, 6); arbusto(l, 24, 132, 5);
    florcitas(l, 87, W.flores / 2);
}

/* colina: pasto con relieve, un árbol solitario, manantial, mirador de piedras y flores silvestres */
static void pintar_colina(lienzo_t *l)
{
    pasto(l, 91, 260);
    for (int Y = 0; Y < l->h; Y++) /* relieve: más claro en la cima, más oscuro en las faldas */
        for (int X = 0; X < l->w; X++) {
            float x = X / KL(l), y = Y / KL(l);
            float d = hypotf((x - 70.0f) / 80.0f, (y - 50.0f) / 70.0f);
            uint32_t c = lz_rget(l, X, Y);
            if (d < 0.45f) c = aclarar(c, (0.45f - d) * 0.35f);
            else if (d > 0.85f) c = oscurecer(c, (d - 0.85f) * 0.35f);
            if (((int)(d * 40 * KL(l)) % 6 == 0) && ((X + Y) & 1)) c = oscurecer(c, 0.06f); /* curvas de nivel */
            lz_real(l, X, Y, c);
        }
    if (W.hojas_caidas) hojas_caidas(l, 100, 93);
    if (W.nieve) nieve_suelo(l, 94);
    for (int k = 0; k < 40; k++) { /* pasto doblado por el viento */
        int x = (int)(hash(k, 5, 95) % M), y = (int)(hash(k, 6, 95) % M);
        lz_set(l, x, y, PASTO_C); lz_set(l, x + 1, y, PASTO_C); lz_set(l, x + 2, y - 1, PASTO_C);
    }
    florcitas(l, 97, W.flores + 120);
    static const float SENDA_PTS[][2] = {{90, 160}, {94, 140}, {82, 120}, {88, 104}, {78, 86}, {70, 66}};
    senda(l, SENDA_PTS, 6, 3.0f);
    /* manantial */
    estanque(l, wx(3, 1) - 2, wy(3, 1) - 2, 7, 4);
    /* mirador: círculo de piedras */
    float mx = wx(3, 5), my = wy(3, 5) - 6;
    for (int k = 0; k < 7; k++) { float a = k * 0.9f; roca(l, mx + cosf(a) * 7, my + sinf(a) * 4, 1.6f); }
    roca(l, wx(3, 3) + 4, wy(3, 3) - 4, 5.0f); roca(l, wx(3, 3) - 4, wy(3, 3), 2.5f);
    if (s_estilo) { pino(l, 18, 44, 14); pino(l, 146, 36, 15); pino(l, 132, 150, 13); }
    arbol(l, wx(3, 2) - 2, wy(3, 2) - 20, 17, 31); /* el árbol solitario de la cima */
    arbusto(l, 20, 110, 5); arbusto(l, 140, 120, 5);
}

/* cueva: piso de piedra, paredes, cristales que brillan, estalagmitas y una poza */

/* ---- estilo nuevo: la cueva y la casita con luz, sombra y muchos tonos ---- */
static void masa(lienzo_t *l, float cx, float cy, float rx, float ry, uint32_t base, uint32_t luz, uint32_t osc);
/* ruido de valores suave (0..255) en pixeles reales, con celdas de G pixeles */
static float vruido(int x, int y, int G, uint32_t sem)
{
    float fx = (float)(x % G) / G, fy = (float)(y % G) / G;
    int gx = x / G, gy = y / G;
    float a = (float)(hash(gx, gy, sem) & 255), b = (float)(hash(gx + 1, gy, sem) & 255);
    float c = (float)(hash(gx, gy + 1, sem) & 255), d = (float)(hash(gx + 1, gy + 1, sem) & 255);
    fx = fx * fx * (3 - 2 * fx); fy = fy * fy * (3 - 2 * fy);
    return (a + (b - a) * fx) * (1 - fy) + (c + (d - c) * fx) * fy;
}
/* un resplandor redondo y suave (con el borde tramado) */
static void resplandor(lienzo_t *l, float cx, float cy, float r, uint32_t c, float fuerza)
{
    const float k = KL(l);
    /* (105) floorf, no (int): con el fondo por franjas las coordenadas pueden ser negativas y (int) corta hacia el cero */
    for (int Y = (int)floorf((cy - r) * k); Y <= (int)floorf((cy + r) * k); Y++)
        for (int X = (int)floorf((cx - r) * k); X <= (int)floorf((cx + r) * k); X++) {
            float d = hypotf((X + 0.5f) / k - cx, (Y + 0.5f) / k - cy) / r;
            if (d >= 1.0f || X < 0 || Y < 0 || X >= l->w || Y >= l->h) continue;
            float t = (1.0f - d) * (1.0f - d) * fuerza;
            if (d > 0.55f && ((X + Y) & 1)) t *= 0.55f;
            lz_real(l, X, Y, mezclar(lz_rget(l, X, Y), c, t));
        }
}
/* una punta de piedra (estalagmita, o estalactita si cuelga): cara con luz a la izquierda y sombra a la derecha */
static void punta_piedra(lienzo_t *l, float x, float y, float w, float h, bool cuelga)
{
    const uint32_t O = A(0x2E2846), B = A(0x5A527E), C = A(0x857CAE), CC = A(0xB0A8D4);
    float s = cuelga ? 1.0f : -1.0f; /* hacia dónde crece */
    if (!cuelga) sombra(l, x + 1.5f, y, w * 0.9f, w * 0.3f);
    triangulo(l, x - w - 0.6f, y, x + w + 0.6f, y, x, y + s * (h + 1), O);
    triangulo(l, x - w, y, x + w, y, x, y + s * h, B);
    triangulo(l, x - w, y, x - w * 0.15f, y, x, y + s * h, C);
    trazo(l, x - w * 0.55f, y + s * 1.0f, x - w * 0.08f, y + s * h * 0.8f, 0.25f, CC);
    if (!cuelga) for (int i = 0; i < 3; i++) lz_set(l, (int)(x + w * 0.4f), (int)(y - 2 - i * 3), O); /* anillos */
}
/* un racimo de cristales: tres agujas facetadas que brillan */
static void cristales2(lienzo_t *l, float x, float y, uint32_t c, uint32_t sem, float tam)
{
    resplandor(l, x, y - 3 * tam, 15 * tam, c, 0.6f);
    sombra(l, x + 1, y + 0.5f, 5 * tam, 1.6f * tam);
    static const float P[3][3] = {{-2.6f, 6.0f, -0.35f}, {2.4f, 5.0f, 0.4f}, {0.0f, 9.5f, 0.0f}};
    const uint32_t O = oscurecer(c, 0.5f), Lz = aclarar(c, 0.45f), M2 = mezclar(c, O, 0.35f);
    for (int i = 0; i < 3; i++) {
        float bx = x + P[i][0] * tam, h = P[i][1] * tam * (0.85f + (float)(hash(i, 3, sem) & 7) / 30.0f), inc = P[i][2] * h;
        float w = 2.0f * tam, tx = bx + inc, ty = y - h;
        triangulo(l, bx - w - 0.5f, y + 0.5f, bx + w + 0.5f, y + 0.5f, tx, ty - 0.8f, oscurecer(c, 0.7f));
        triangulo(l, bx - w, y, bx + w, y, tx, ty, M2);                 /* cara en sombra */
        triangulo(l, bx - w, y, bx - w * 0.1f, y, tx, ty, c);           /* cara iluminada */
        triangulo(l, bx - w * 0.75f, y - 0.5f, bx - w * 0.35f, y - 0.5f, tx - 0.2f, ty + 1, Lz); /* brillo */
        lz_set(l, (int)tx, (int)ty + 1, A(0xFFFFFF));
    }
}
/* hongos que brillan solos */
static void hongo_luz(lienzo_t *l, float x, float y, uint32_t c)
{
    resplandor(l, x, y - 2, 7, c, 0.35f);
    rect(l, (int)x, (int)y - 2, 1, 3, A(0xE8E2F4));
    elipse(l, x + 0.5f, y - 2.5f, 2.2f, 1.3f, c);
    lz_set(l, (int)x - 1, (int)y - 3, aclarar(c, 0.6f));
}

static void pintar_cueva2(lienzo_t *l)
{
    const float k = KL(l);
    const uint32_t P0 = A(0x3A3354), P1 = A(0x4A4268), P2 = A(0x585078), P3 = A(0x6A6290), GR = A(0x2E2944);
    const uint32_t PA0 = A(0x1C1830), PA1 = A(0x2A243F), BORDE = A(0x7A71A8);
    for (int Y = 0; Y < l->h; Y++)
        for (int X = 0; X < l->w; X++) {
            const float x = (X + 0.5f) / k, y = (Y + 0.5f) / k;
            /* el piso: manchones suaves de piedra con grano fino */
            float v = vruido(X, Y, 18, 301) * 0.75f + vruido(X, Y, 6, 302) * 0.25f + (float)(hash(X, Y, 303) & 15) - 8;
            uint32_t c = v < 100 ? P0 : (v < 150 ? P1 : (v < 190 ? P2 : P3));
            if ((hash(X, Y, 304) & 63) == 0) c = GR;
            /* las paredes: borde apenas irregular; la boca de la cueva queda abajo, abierta */
            float ond = (vruido(X, Y, 20, 305) - 128) / 1400.0f;
            float d = hypotf((x - 78.0f) / 70.0f, (y - 84.0f) / 62.0f) + ond;
            /* la boca de la cueva: a la izquierda, por donde llega y se va (la flecha a la colina) */
            d -= clampf((20.0f - x) / 22.0f, 0, 1) * clampf(1.0f - fabsf(y - 80.0f) / 26.0f, 0, 1) * 0.45f;
            if (d > 1.0f) c = ((hash(X / 3, Y / 3, 307) & 7) == 0) ? PA1 : PA0;
            else if (d > 0.88f) c = oscurecer(c, (d - 0.88f) * 3.5f);    /* sombra al pie de la pared */
            if (d <= 1.0f) {
                /* la luz que entra por la boca (abajo), cálida y tramada en el borde */
                float luz = clampf(1.0f - hypotf((x + 4.0f) / 62.0f, (y - 80.0f) / 40.0f), 0, 1);
                if (luz > 0 && (luz > 0.2f || ((X + Y) & 1))) c = mezclar(c, A(0xE8D2B0), luz * 0.42f);
                c = oscurecer(c, clampf((x - 70.0f) / 160.0f, 0, 0.25f)); /* hacia el fondo (la derecha), más oscuro */
            }
            lz_real(l, X, Y, c);
        }
    /* la pared: rocas redondas una al lado de otra, con luz arriba (lo que mira hacia adentro) */
    for (int i = 0; i < 46; i++) {
        float a = (float)i / 46.0f * 6.2832f;
        float ry = 84 + sinf(a) * 64.5f;
        float bx = 78 + cosf(a) * 72.5f;
        if (bx < 24 && fabsf(ry - 80) < 24) continue; /* la boca queda abierta */
        float j = (float)(hash(i, 3, 310) % 5) - 2.0f;
        float rx = 78 + cosf(a) * (72.5f + j);
        float r = 6.0f + (float)(hash(i, 1, 310) % 7);
        masa(l, rx, ry + j * 0.5f, r, r * 0.8f, A(0x332D4C), A(0x5A527E), PA0);
    }
    for (int i = 0; i < 30; i++) { /* una segunda fila, más adentro y más chica, arriba y a la derecha */
        float a = 4.10f + (float)i / 30.0f * 3.30f;
        float rx = 78 + cosf(a) * 66, ry = 84 + sinf(a) * 58;
        float r = 4.0f + (float)(hash(i, 2, 310) % 3);
        masa(l, rx, ry, r, r * 0.75f, A(0x4A4268), BORDE, PA1);
    }
    /* estalactitas colgando del borde de arriba */
    static const float COLG[][3] = {{44, 30, 9}, {60, 25, 7}, {92, 24, 11}, {110, 29, 8}, {126, 40, 6}, {30, 44, 6}};
    for (int i = 0; i < 6; i++) punta_piedra(l, COLG[i][0], COLG[i][1], 2.6f, COLG[i][2], true);
    /* la poza: piedras alrededor y agua profunda que refleja los cristales */
    {
        float px = wx(4, 1) - 4, py = wy(4, 1) - 2;
        sombra(l, px + 1, py + 2, 14, 7.5f);
        for (int i = 0; i < 10; i++) {
            float a = (float)i / 10 * 6.283f;
            float rx = px + cosf(a) * 12.5f, ry = py + sinf(a) * 6.8f;
            masa(l, rx, ry, 2.6f, 1.9f, A(0x5E5682), A(0x8A82B0), A(0x332D4A));
        }
        for (int Y = (int)((py - 7) * k); Y <= (int)((py + 7) * k); Y++)
            for (int X = (int)((px - 12) * k); X <= (int)((px + 12) * k); X++) {
                float dx = ((X + 0.5f) / k - px) / 11.0f, dy = ((Y + 0.5f) / k - py) / 5.8f, dd = dx * dx + dy * dy;
                if (dd > 1) continue;
                uint32_t c = mezclar(A(0x3A6AB0), A(0x16244A), clampf(dd * 1.1f + dy * 0.3f, 0, 1));
                if (dd > 0.8f && dy < 0 && ((X + Y) & 1)) c = A(0x14203E);
                lz_real(l, X, Y, c);
            }
        trazo(l, px - 7, py - 2, px - 2, py - 2, 0.3f, A(0x9ED8FF));
        trazo(l, px + 2, py + 1, px + 6, py + 1, 0.3f, A(0x6FB8E8));
        lz_set(l, (int)px + 4, (int)py - 3, A(0x7FE0D6)); /* reflejo de un cristal */
        lz_set(l, (int)px - 4, (int)py + 2, A(0xB48CFF));
    }
    /* la estalagmita grande y otras más chicas */
    punta_piedra(l, wx(4, 2) + 2, wy(4, 2) - 2, 6.5f, 24, false);
    punta_piedra(l, wx(4, 2) - 6, wy(4, 2) - 1, 3.2f, 11, false);
    static const float EST[][3] = {{100, 46, 8}, {62, 38, 6}, {122, 104, 7}, {28, 100, 6}};
    for (int i = 0; i < 4; i++) punta_piedra(l, EST[i][0], EST[i][1], 2.8f, EST[i][2], false);
    /* cristales: racimos turquesa, violeta y rosa */
    cristales2(l, 114, 74, A(0x6FE6D8), 1, 1.35f);
    cristales2(l, 36, 58, A(0xB48CFF), 2, 1.2f);
    cristales2(l, 122, 46, A(0x6FE6D8), 3, 0.9f);
    cristales2(l, 74, 34, A(0xFF9AD6), 4, 1.0f);
    cristales2(l, 24, 114, A(0xB48CFF), 5, 0.8f);
    /* hongos que brillan */
    hongo_luz(l, 48, 116, A(0x7AF0FF)); hongo_luz(l, 51, 118, A(0x7AF0FF)); hongo_luz(l, 136, 92, A(0xB48CFF));
    hongo_luz(l, 66, 58, A(0x7AF0FF)); hongo_luz(l, 106, 118, A(0xFF9AD6));
    /* piedritas sueltas */
    for (int i = 0; i < 14; i++) {
        float x = 20 + (float)(hash(i, 1, 309) % 116), y = 40 + (float)(hash(i, 2, 309) % 90);
        float d = hypotf((x - 78.0f) / 70.0f, (y - 82.0f) / 62.0f);
        if (d > 0.9f) continue;
        masa(l, x, y, 1.6f, 1.1f, A(0x6A6290), A(0x958CBC), A(0x3A3354));
    }
    /* el refugio: un colchón de musgo con florcitas que brillan */
    {
        float mx = wx(4, 5), my = wy(4, 5) - 3;
        sombra(l, mx + 1, my + 2.5f, 10, 3.5f);
        masa(l, mx, my, 9, 4.2f, A(0x3E7A4A), A(0x6AAA5A), A(0x264A30));
        masa(l, mx - 3, my - 1.5f, 5, 2.4f, A(0x4E8C52), A(0x86C46A), A(0x3E7A4A));
        lz_set(l, (int)mx + 3, (int)my - 2, A(0xFFF6C8)); lz_set(l, (int)mx - 5, (int)my, A(0x7AF0FF)); lz_set(l, (int)mx + 6, (int)my + 1, A(0xFFF6C8));
    }
    roca(l, wx(4, 3) + 6, wy(4, 3) - 2, 3.5f);
}


/* ---- la isla del lago: agua alrededor, playa, un árbol muy viejo, un círculo de piedras, un faro y el muelle ---- */
static void pintar_isla(lienzo_t *l, uint32_t decor)
{
    const float k = KL(l);
    const uint32_t AG_P = A(0x2A6AA8), AG_M = A(0x3C86C0), AG_C = A(0x58A8D8), AG_B = A(0x7CCBE8), ESP = A(0xE8F6FF);
    const uint32_t ARE = A(0xF0D8A0), ARE_C = A(0xFAE8BC), ARE_O = A(0xD8B880), ARE_M = A(0xC8A870);
    for (int Y = 0; Y < l->h; Y++)
        for (int X = 0; X < l->w; X++) {
            const float x = (X + 0.5f) / k, y = (Y + 0.5f) / k;
            float ond = (vruido(X, Y, 16, 501) - 128) / 1500.0f;
            float d = hypotf((x - 80.0f) / 62.0f, (y - 86.0f) / 47.0f) + ond;
            uint32_t c;
            if (d > 1.0f) { /* el agua: más clara cerca de la orilla, con ondas */
                float v = vruido(X, Y * 3, 9, 502);
                c = d < 1.10f ? AG_B : (d < 1.24f ? AG_C : (d < 1.5f ? AG_M : AG_P));
                if (d < 1.03f && ((X + Y) & 1)) c = ESP;                 /* la espuma */
                else if (d < 1.06f && (hash(X, Y, 503) & 3) == 0) c = ESP;
                else if (v > 205 && (hash(X / 3, Y, 504) & 1)) c = aclarar(c, 0.35f); /* ondas */
                else if (v < 40) c = oscurecer(c, 0.12f);
            } else if (d > 0.84f) { /* la playa */
                float v = vruido(X, Y, 7, 505);
                c = v < 80 ? ARE_O : (v > 190 ? ARE_C : ARE);
                if (d > 0.97f) c = ARE_M;                                  /* arena mojada */
                if ((hash(X, Y, 506) & 63) == 0) c = ARE_O;
            } else { /* el pasto, con manchones */
                float v = vruido(X, Y, 12, 507) + (float)(hash(X, Y, 508) & 15) - 8;
                c = v < 80 ? PASTO_O : (v > 185 ? PASTO_C : PASTO);
                if (d > 0.80f && ((X + Y) & 1)) c = ARE;                   /* borde tramado con la arena */
            }
            lz_real(l, X, Y, c);
        }
    /* el muelle a la izquierda (ahí amarra el bote) */
    sombra(l, 16, 92, 14, 3);
    for (int x = 4; x < 28; x += 3) { rect(l, x, 84, 3, 7, A(0xB57E4E)); rect(l, x, 84, 3, 1, A(0xD8A070)); rect(l, x + 2, 84, 1, 7, A(0x8B5A34)); }
    for (int i = 0; i < 3; i++) { rect(l, 6 + i * 9, 90, 2, 5, A(0x6E4428)); rect(l, 6 + i * 9, 83, 2, 2, A(0x6E4428)); }
    /* el círculo de piedras antiguas, con runas que brillan apenas */
    {
        float cx = 80, cy = 88; /* el círculo en el centro de la isla, a la altura del muelle */
        elipse(l, cx, cy + 1, 15, 7, oscurecer(PASTO, 0.12f));
        for (int i = 0; i < 7; i++) {
            float a = (float)i / 7.0f * 6.2832f - 1.57f;
            float px = cx + cosf(a) * 13, py = cy + sinf(a) * 6.5f;
            float h = 6.0f + (float)(hash(i, 1, 509) % 3);
            sombra(l, px + 1.5f, py + 0.5f, 3, 1.2f);
            rect(l, (int)px - 2, (int)(py - h), 4, (int)h + 1, A(0x8C8AA0));
            rect(l, (int)px - 2, (int)(py - h), 1, (int)h + 1, A(0xB4B2C8));
            rect(l, (int)px + 1, (int)(py - h), 1, (int)h + 1, A(0x5E5C74));
            masa(l, px, py - h, 2.0f, 1.2f, A(0x8C8AA0), A(0xC8C6DA), A(0x5E5C74));
            lz_set(l, (int)px, (int)(py - h * 0.5f), A(0xB48CFF));                /* una runa */
        }
        masa(l, cx, cy, 5, 2.4f, A(0x9C9AB0), A(0xD0CEE0), A(0x6A687E));   /* la piedra del centro */
        if (decor & 1u) { /* la Guardiana dejó ahí el Corazón del Mundo: late verde y dorado */
            resplandor(l, cx, cy - 3, 16, A(0x7FFF7A), 0.5f);
            masa(l, cx, cy - 3.5f, 2.8f, 2.6f, A(0x5ADF6A), A(0xC8FFB0), A(0x2A8A3A));
            lz_set(l, (int)cx - 1, (int)cy - 5, A(0xFFFFFF));
            for (int i = 0; i < 7; i++) { /* las runas encendidas */
                float a = (float)i / 7.0f * 6.2832f - 1.57f;
                resplandor(l, cx + cosf(a) * 13, cy + sinf(a) * 6.5f - 4, 4, A(0xFFE27A), 0.5f);
            }
        } else resplandor(l, cx, cy - 1, 9, A(0xB48CFF), 0.25f);
        lz_set(l, (int)cx, (int)cy - 1, A(0xE8DCFF));
    }
    /* el árbol muy viejo: tronco ancho con raíces y una copa enorme */
    {
        float cx = 100, cy = 29, r = 17; /* el árbol viejo: más arriba y un poco más chico */
        sombra(l, cx + 4, cy + r * 1.15f, r * 1.2f, r * 0.36f);
        for (int i = -2; i <= 2; i++) trazo(l, cx + i * 1.5f, cy + r * 0.9f, cx + i * 5.5f, cy + r * 1.12f, 0.9f, TRONCO_O); /* raíces */
        rect(l, (int)cx - 4, (int)(cy + r * 0.35f), 9, (int)(r * 0.75f), TRONCO_O);
        rect(l, (int)cx - 3, (int)(cy + r * 0.35f), 6, (int)(r * 0.72f), TRONCO);
        rect(l, (int)cx - 3, (int)(cy + r * 0.35f), 2, (int)(r * 0.72f), aclarar(TRONCO, 0.2f));
        elipse(l, cx + 1, cy + r * 0.8f, 1.6f, 2.2f, A(0x3A2618)); /* un hueco */
        copa_grumosa(l, cx, cy, r, 511, A(0x2E7A4A), A(0x58A85E), A(0x1C5236));
        for (int i = 0; i < 7; i++) { /* frutos de luz que cuelgan */
            float fx = cx - r * 0.7f + (float)(hash(i, 1, 512) % (unsigned)(r * 1.4f)), fy = cy + (float)(hash(i, 2, 512) % 14);
            lz_set(l, (int)fx, (int)fy, A(0xFFE27A)); lz_set(l, (int)fx, (int)fy + 1, A(0xFFB020));
        }
    }
    /* el faro chiquito, a la derecha */
    {
        float fx = 134, fy = 82;
        sombra(l, fx + 3, fy + 1, 7, 2);
        rect(l, (int)fx - 6, (int)fy - 2, 12, 3, A(0x8C8AA0)); /* base de piedra */
        for (int y = (int)fy - 26; y < (int)fy - 2; y++) {
            int w = 4 + ((int)fy - y) / 9;
            uint32_t c = ((y - ((int)fy - 26)) / 5) % 2 ? A(0xF0506E) : A(0xFFFFFF);
            rect(l, (int)fx - w, y, w * 2, 1, c);
            lz_set(l, (int)fx - w, y, aclarar(c, 0.3f)); lz_set(l, (int)fx + w - 1, y, oscurecer(c, 0.25f));
        }
        rect(l, (int)fx - 4, (int)fy - 32, 8, 6, A(0x3A3A58));
        rect(l, (int)fx - 3, (int)fy - 31, 6, 4, A(0xFFE27A));
        triangulo(l, fx - 5, fy - 32, fx + 5, fy - 32, fx, fy - 37, A(0xC84A64));
    }
    /* conchas y piedritas en la playa, matas de pasto y unas flores */
    for (int i = 0; i < 12; i++) {
        float a = (float)(hash(i, 1, 513) % 628) / 100.0f;
        float px = 80 + cosf(a) * 56, py = 86 + sinf(a) * 42;
        if (px < 30 && py > 78 && py < 96) continue; /* el muelle */
        if (i % 3 == 0) masa(l, px, py, 1.5f, 1.0f, A(0x9C9AB0), A(0xD0CEE0), A(0x6A687E));
        else { lz_set(l, (int)px, (int)py, A(0xFFC8D8)); lz_set(l, (int)px + 1, (int)py, A(0xF0A0B8)); }
    }
    arbusto(l, 64, 104, 5); arbusto(l, 112, 100, 4); arbusto(l, 36, 52, 4);
    for (int i = 0; i < 10; i++) {
        float x = 40 + (float)(hash(i, 3, 514) % 80), y = 60 + (float)(hash(i, 4, 514) % 46);
        if (hypotf((x - 80) / 18, (y - 88) / 10) < 1 || hypotf((x - 100) / 18, (y - 40) / 22) < 1) continue;
        lz_set(l, (int)x, (int)y, (i & 1) ? A(0xFFE066) : A(0xFF8FC8)); lz_set(l, (int)x, (int)y + 1, PASTO_O);
    }
}

static void nube_viaje(lienzo_t *l, float x, float y, float tam);
static void estrellas_cielo(lienzo_t *l, int n, int hasta, uint32_t semilla);
/* ---- el cruce: el lago de lado, la orilla con árboles a la izquierda y la isla a lo lejos a la derecha ---- */
static bool s_cruce_hielo;
void art_cruce_hielo(bool on) { s_cruce_hielo = on; }
static int s_cruce_est = 1;
void art_cruce_estacion(int e) { s_cruce_est = e; s_cruce_hielo = e == 3; }
__attribute__((unused)) static void art_cruce_fondo_viejo(art_img_t *dst, int periodo)
{
    const int D = 156;
    const float k = 1.5f;
    const int LR = (int)(D * k), H0 = ART_CRUCE_AGUA_Y / 3;
    lienzo_t l = {LR, LR, malloc((size_t)LR * LR * 4), k};
    if (!l.p) return;
    static const uint32_t ARRIBA[3] = {A(0x6EB8F0), A(0x6A5AA8), A(0x0E1638)}, ABAJO[3] = {A(0xCDEBFF), A(0xFFB48A), A(0x3A3A78)};
    periodo = periodo < 0 || periodo > 2 ? 0 : periodo;
    for (int Y = 0; Y < LR; Y++)
        for (int X = 0; X < LR; X++) {
            float t = (float)Y / (H0 * k) + ((((X + Y) & 1) ? 0.012f : -0.012f));
            l.p[Y * LR + X] = mezclar(ARRIBA[periodo], ABAJO[periodo], t < 0 ? 0 : (t > 1 ? 1 : t));
        }
    if (periodo == 2) { estrellas_cielo(&l, 60, H0 - 6, 541); circulo(&l, 40, 26, 7, A(0xFFF6D0)); }
    else if (periodo == 1) { circulo(&l, 70, H0 - 4, 12, A(0xFFD98A)); circulo(&l, 70, H0 - 4, 9, A(0xFFF0B0)); }
    else { circulo(&l, 36, 24, 8, A(0xFFF6C8)); nube_viaje(&l, 70, 30, 4); nube_viaje(&l, 128, 18, 3); }
    const uint32_t LEJOS = periodo == 2 ? A(0x1A2050) : (periodo == 1 ? A(0x5A3A6A) : A(0x5E9A78));
    const uint32_t CERCA = periodo == 2 ? A(0x141A40) : (periodo == 1 ? A(0x4A2E5A) : A(0x3E7A4A));
    /* la orilla del lago, con árboles, a la izquierda */
    elipse(&l, 6, H0, 28, 5, CERCA);
    for (int i = 0; i < 5; i++) { float x = -2 + i * 7.0f; triangulo(&l, x - 5, H0 - 1, x + 5, H0 - 1, x, H0 - 16 - (i % 2) * 5, CERCA); }
    /* la isla a lo lejos: el árbol viejo y el faro */
    elipse(&l, 128, H0, 26, 4, LEJOS);
    circulo(&l, 122, H0 - 12, 9, LEJOS); circulo(&l, 115, H0 - 8, 6, LEJOS); circulo(&l, 130, H0 - 8, 6, LEJOS);
    rect(&l, 121, H0 - 6, 3, 6, LEJOS);
    rect(&l, 143, H0 - 14, 4, 14, periodo == 2 ? A(0x2A2E60) : A(0xF2F2F2));
    rect(&l, 143, H0 - 10, 4, 2, A(0xF0506E));
    rect(&l, 143, H0 - 17, 4, 3, A(0xFFE27A));
    if (periodo == 2) for (int i = 0; i < 24; i++) lz_mezclar(&l, 145 - i, H0 - 16 + i / 8, A(0xFFE27A), 0.5f - i * 0.02f); /* el haz del faro */
    /* el agua */
    const uint32_t AG0 = s_cruce_hielo ? (periodo == 2 ? A(0x3A4A7A) : (periodo == 1 ? A(0xB8B8E0) : A(0xE4F6FC))) : periodo == 2 ? A(0x23306A) : (periodo == 1 ? A(0x6A5A9A) : A(0x5AA8D8));
    const uint32_t AG1 = s_cruce_hielo ? (periodo == 2 ? A(0x222E56) : (periodo == 1 ? A(0x9496C4) : A(0xB4DCEE))) : periodo == 2 ? A(0x101838) : (periodo == 1 ? A(0x3A3A70) : A(0x2A6AA8));
    for (int Y = (int)(H0 * k); Y < LR; Y++)
        for (int X = 0; X < LR; X++) {
            float t = (float)(Y - H0 * k) / (LR - H0 * k);
            uint32_t c = mezclar(AG0, AG1, t);
            float v = vruido(X, Y * 4, 10, 542);
            if (v > 200 && (hash(X / 4, Y / 2, 543) & 1)) c = mezclar(c, A(0xFFFFFF), 0.25f);
            if (s_cruce_hielo && v > 200 && (hash(X / 3, Y, 544) % 7) == 0) c = mezclar(c, A(0xFFFFFF), 0.5f); /* grietas claras */
            if (v < 40) c = oscurecer(c, 0.1f);
            l.p[Y * LR + X] = c;
        }
    ampliar(dst, &l, 2, false);
    free(l.p);
}


/* ---- el paseo por el firmamento ---- */
static void chispa(lienzo_t *l, float x, float y, uint32_t c);
__attribute__((unused)) static void art_firmamento_fondo_viejo(art_img_t *dst)
{
    const int D = 156;
    const float k = 1.5f;
    const int LR = (int)(D * k);
    lienzo_t l = {LR, LR, malloc((size_t)LR * LR * 4), k};
    if (!l.p) return;
    for (int Y = 0; Y < LR; Y++)
        for (int X = 0; X < LR; X++) {
            float t = (float)Y / LR + ((((X + Y) & 1) ? 0.01f : -0.01f));
            uint32_t c = mezclar(A(0x0A0A2A), A(0x2A1A5A), t < 0 ? 0 : (t > 1 ? 1 : t));
            /* nebulosas: manchones suaves de color */
            float v = vruido(X, Y, 40, 701), w = vruido(X + 300, Y, 30, 702);
            if (v > 150) c = mezclar(c, A(0x6A3AA8), (v - 150) / 105.0f * 0.45f);
            if (w > 170) c = mezclar(c, A(0x2A6AA8), (w - 170) / 85.0f * 0.4f);
            l.p[Y * LR + X] = c;
        }
    estrellas_cielo(&l, 160, D, 703);
    for (int i = 0; i < 14; i++) chispa(&l, (float)(hash(i, 1, 704) % D), (float)(hash(i, 2, 704) % D), (i & 1) ? A(0xFFE27A) : A(0x9CF0FF));
    /* la Vía Láctea: una banda tenue en diagonal */
    for (int Y = 0; Y < LR; Y++)
        for (int X = 0; X < LR; X++) {
            float d = fabsf((float)(X - Y) * 0.7071f - 10.0f * k) / (26.0f * k);
            if (d < 1.0f && (hash(X, Y, 705) & 7) < (uint32_t)((1.0f - d) * 6)) l.p[Y * LR + X] = mezclar(l.p[Y * LR + X], A(0xE8E0FF), 0.35f * (1.0f - d));
        }
    ampliar(dst, &l, 2, false);
    free(l.p);
}
/* tu cumpleaños: un claro del bosque de noche, con pinos alrededor y un círculo de musgo al centro */
void art_claro_fondo(art_img_t *dst)
{
    const int D = 156;
    const float k = 1.5f;
    const int LR = (int)(D * k);
    lienzo_t l = {LR, LR, malloc((size_t)LR * LR * 4), k};
    if (!l.p) return;
    for (int Y = 0; Y < LR; Y++)
        for (int X = 0; X < LR; X++) {
            float y = Y / k;
            uint32_t c;
            if (y < 70) c = mezclar(A(0x0E0E30), A(0x2A2A5E), y / 70.0f);           /* el cielo */
            else {
                float t = (y - 70) / 86.0f;
                c = mezclar(A(0x1E3A2E), A(0x2E5A3A), t);                          /* el pasto de noche */
                if ((hash(X / 3, Y / 3, 811) & 7) == 0) c = mezclar(c, A(0x3E6E44), 0.5f);
                float d = hypotf((X / k - 78) / 46.0f, (y - 112) / 22.0f);
                if (d < 1.0f) c = mezclar(c, A(0x4E8A50), (1.0f - d) * 0.6f);       /* el claro de musgo */
                float luz = 1.0f - hypotf((X / k - 78) / 60.0f, (y - 104) / 40.0f); /* la luz tibia del pastel */
                if (luz > 0) c = mezclar(c, A(0xFFD27A), luz * 0.25f);
            }
            l.p[Y * LR + X] = c;
        }
    estrellas_cielo(&l, 60, D, 812);
    for (int i = 0; i < 8; i++) { /* una fila de pinos al fondo, en sombra */
        float x = 4.0f + (float)i * 21.0f + (float)(hash(i, 0, 813) % 8);
        pino(&l, x, 78.0f + (float)(hash(i, 1, 813) % 6), 24.0f + (float)(hash(i, 2, 813) % 10));
    }
    for (int Y = (int)(50 * k); Y < (int)(84 * k); Y++) /* los pinos del fondo, más oscuros (lejos) */
        for (int X = 0; X < LR; X++) l.p[Y * LR + X] = mezclar(l.p[Y * LR + X], A(0x0E1A2A), 0.45f);
    pino(&l, 10.0f, 150.0f, 50.0f); pino(&l, 146.0f, 152.0f, 54.0f); /* y dos cerca, a los lados */
    ampliar(dst, &l, 2, false);
    free(l.p);
}

/* el Árbol de los años: cada cumpleaños le sale una flor nueva (la última brilla) */
void art_arbol_anios(art_img_t *dst, int flores, int escala)
{
    const float kd = k_de_escala(escala);
    const int L = (int)lroundf(48 * kd), AMP = (int)lroundf((float)escala / kd);
    uint32_t *buf = buf_trabajo(L * L);
    if (!buf) return;
    lienzo_t l = {L, L, buf, kd};
    memset(buf, 0, (size_t)L * L * 4);
    rect(&l, 21, 30, 6, 16, A(0x8B5E3C)); rect(&l, 21, 30, 2, 16, A(0xA8764A));            /* el tronco */
    trazo(&l, 24, 34, 15, 26, 1.2f, A(0x8B5E3C)); trazo(&l, 24, 32, 33, 25, 1.2f, A(0x8B5E3C)); /* ramas */
    rect(&l, 15, 45, 18, 2, A(0x6A4428));                                                 /* raíces */
    masa(&l, 24, 20, 17, 13, A(0x4FA85A), A(0x8FE08A), A(0x2E7A3E));                     /* la copa */
    masa(&l, 13, 24, 8, 7, A(0x4FA85A), A(0x8FE08A), A(0x2E7A3E));
    masa(&l, 35, 24, 8, 7, A(0x4FA85A), A(0x8FE08A), A(0x2E7A3E));
    static const uint32_t C[] = {0xFF8FC0, 0xFFE066, 0xFFFFFF, 0xB48CFF, 0xFF9A3C, 0x8FD0FF};
    int n = flores < 0 ? 0 : (flores > 40 ? 40 : flores);
    for (int i = 0; i < n; i++) { /* repartidas por la copa, siempre en el mismo lugar */
        float a = (float)i * 2.39996f, r = 3.0f + 11.0f * sqrtf((float)(i + 1) / 41.0f);
        float x = 24 + cosf(a) * r * 1.35f, y = 21 + sinf(a) * r * 0.85f;
        uint32_t c = C[i % 6];
        if (i == n - 1) { circulo(&l, x, y, 2.0f, A(0xFFF6B0)); estrellita(&l, (int)x, (int)y, A(0xFFFFFF)); } /* la de este año */
        else { lz_set(&l, (int)x, (int)y, A(c)); lz_set(&l, (int)x + 1, (int)y, A(c)); lz_set(&l, (int)x, (int)y - 1, A(0xFFF6D0)); }
    }
    if (s_estilo) ico_pulir(buf, L, kd);
    ampliar(dst, &l, AMP, false);
}

void art_astro(art_img_t *dst, int tipo, int escala)
{
    const float kd = k_de_escala(escala);
    const int L = (int)lroundf(40 * kd), AMP = (int)lroundf((float)escala / kd);
    uint32_t *buf = buf_trabajo(L * L);
    if (!buf) return;
    lienzo_t l = {L, L, buf, kd};
    memset(buf, 0, (size_t)L * L * 4);
    switch (tipo) {
    case 0: /* planeta con anillos */
        elipse(&l, 20, 21, 19, 5.5f, A(0xC8A870)); elipse(&l, 20, 21, 15, 3.5f, A(0x2A1A5A)); /* el anillo de atrás */
        masa(&l, 20, 20, 11, 11, A(0xE8A86A), A(0xFFD8A0), A(0xA8603A));
        for (int y = 12; y < 30; y += 4) trazo(&l, 11, (float)y, 29, (float)y + 1, 0.4f, A(0xD08850)); /* bandas */
        for (int X = 0; X < L; X++) /* el anillo de adelante: pasa por encima del planeta abajo */
            for (int Y = 0; Y < L; Y++) {
                float x = (X + 0.5f) / kd - 20, y = (Y + 0.5f) / kd - 21;
                float d = (x / 19) * (x / 19) + (y / 5.5f) * (y / 5.5f), d2 = (x / 15) * (x / 15) + (y / 3.5f) * (y / 3.5f);
                if (d <= 1 && d2 > 1 && y > 0) buf[Y * L + X] = (x < 0) ? A(0xF0D090) : A(0xC8A870);
            }
        break;
    case 1: /* la luna, con cráteres */
        masa(&l, 20, 20, 16, 16, A(0xE8E4D8), A(0xFFFFF4), A(0xB0AC9C));
        circulo(&l, 14, 15, 3, A(0xC8C4B4)); circulo(&l, 25, 24, 4, A(0xC8C4B4)); circulo(&l, 22, 12, 2, A(0xD4D0C0)); circulo(&l, 12, 26, 2.5f, A(0xD4D0C0));
        lz_set(&l, 13, 14, A(0xB0AC9C)); lz_set(&l, 24, 23, A(0xB0AC9C));
        break;
    case 2: /* un cometa: la cabeza a la izquierda y la cola larga */
        for (int i = 0; i < 26; i++) {
            float x = 12 + (float)i, a = 1.0f - (float)i / 26.0f;
            trazo(&l, x, 20 - a * 0.5f, x + 1, 20 + (float)i * 0.25f, 0.6f + a * 3.5f, mezclar(A(0x7AF0FF), A(0x2A3A8A), 1 - a));
        }
        circulo(&l, 11, 20, 4, A(0xE8FFFF)); circulo(&l, 10, 19, 2, A(0xFFFFFF));
        break;
    case 3: /* un planetita rojo con un satélite */
        masa(&l, 18, 22, 9, 9, A(0xD8504A), A(0xFF8A70), A(0x8A2A2A));
        circulo(&l, 15, 20, 2, A(0xB03A3A)); circulo(&l, 21, 25, 1.5f, A(0xB03A3A));
        masa(&l, 32, 10, 3, 3, A(0xC8C4D8), A(0xF4F0FF), A(0x8A86A0));
        break;
    case 5: /* el gigante rayado: bandas y una mancha */
        masa(&l, 20, 20, 17, 17, A(0xE8C89A), A(0xFFEBC8), A(0x9A6A4A));
        for (int y = 8; y < 34; y += 5) trazo(&l, 4, (float)y, 36, (float)y + 1, 0.9f, (y / 5) % 2 ? A(0xC8805A) : A(0xF4DCB4));
        elipse(&l, 26, 24, 4, 2.5f, A(0xC8402A));
        break;
    case 6: /* la luciérnaga dormida: un puntito dorado con alitas */
        circulo(&l, 20, 20, 9, A(0x5A4A2A)); circulo(&l, 20, 20, 6, A(0xFFE27A)); circulo(&l, 19, 19, 3, A(0xFFFFFF));
        elipse(&l, 12, 14, 5, 2.5f, A(0xC8F0FF)); elipse(&l, 28, 14, 5, 2.5f, A(0xC8F0FF));
        break;
    default: /* una nube grande y mullida */
        masa(&l, 12, 24, 10, 7, A(0xFFFFFF), A(0xFFFFFF), A(0xD8E0F4));
        masa(&l, 24, 20, 12, 9, A(0xFFFFFF), A(0xFFFFFF), A(0xD8E0F4));
        masa(&l, 32, 26, 7, 5, A(0xF4F6FF), A(0xFFFFFF), A(0xD8E0F4));
        rect(&l, 4, 27, 34, 4, A(0xE4EAF8));
        break;
    }
    if (s_estilo && tipo != 4) ico_pulir(buf, L, kd);
    ampliar(dst, &l, AMP, false);
}

/* el bote con su velita, de lado */
void art_bote_vela_asiento(int escala, int *x, int *y) { *x = 16 * escala; *y = 22 * escala; }
__attribute__((unused)) static void art_bote_vela_viejo(art_img_t *dst, int inflada, int escala)
{
    const float kd = k_de_escala(escala);
    const int W = 40, Hh = 30;
    const int LW = (int)lroundf(W * kd), LH = (int)lroundf(Hh * kd), AMP = (int)lroundf((float)escala / kd);
    uint32_t *buf = buf_trabajo(LW * LH);
    if (!buf) return;
    lienzo_t l = {LW, LH, buf, kd};
    memset(buf, 0, (size_t)LW * LH * 4);
    /* el mástil y la vela */
    rect(&l, 24, 2, 1, 21, A(0x6E4428));
    float b = inflada ? 7.0f : 1.5f; /* cuánto se hincha */
    for (int y = 3; y < 20; y++) {
        float t = (float)(y - 3) / 17.0f;
        float ancho = 2 + t * 11;                         /* triángulo: angosto arriba */
        float panza = b * sinf(t * 3.1416f);
        for (int x = 0; x < (int)(ancho + panza); x++) {
            uint32_t c = x < 2 ? A(0xD8CCB4) : (x > ancho + panza - 2 ? A(0xFFFFFF) : A(0xF6EEDC));
            if (inflada && x > 2 && x < 5) c = A(0xE8DCC4);
            lz_set(&l, 25 + x, y, c);
        }
    }
    for (int y = 6; y < 18; y += 5) lz_set(&l, 28, y, A(0xF0506E)); /* puntadas */
    trazo(&l, 24, 20, 37, 20, 0.35f, A(0x8B5A34)); /* la botavara */
    rect(&l, 24, 0, 4, 2, A(0xF0506E)); /* banderita */
    /* el casco */
    for (int y = 21; y < 28; y++) {
        int s = (y - 21) / 2;
        uint32_t c = y == 21 ? A(0xE0A060) : (y == 22 ? A(0xD8905A) : (y == 24 ? A(0xF6EEDC) : (y > 25 ? A(0x8B5A34) : A(0xB06A3A))));
        rect(&l, 3 + s, y, 34 - 2 * s, 1, c);
    }
    rect(&l, 2, 20, 3, 2, A(0xB06A3A)); rect(&l, 35, 20, 3, 2, A(0xB06A3A)); /* proa y popa */
    for (int y = 0; y < LH; y++) /* contorno oscuro: se marca primero (0x01) y se pinta después, para que no se desparrame */
        for (int x = 0; x < LW; x++) {
            if (buf[y * LW + x] >> 24) continue;
            bool v = false;
            for (int d = 0; d < 4 && !v; d++) {
                int nx = x + (d == 0) - (d == 1), ny = y + (d == 2) - (d == 3);
                if (nx >= 0 && ny >= 0 && nx < LW && ny < LH && (buf[ny * LW + nx] >> 24)) v = true;
            }
            if (v) buf[y * LW + x] = 1;
        }
    for (int i = 0; i < LW * LH; i++) if (buf[i] == 1) buf[i] = A(0x2A2238);
    if (s_estilo) ico_pulir(buf, LW, kd);
    ampliar(dst, &l, AMP, false);
}

/* la carta de un amigo: un sobre crema con su sello de cera rojo. fase 0 cerrada, 1 el sello se parte,
 * 2 la solapa abierta, 3..6 la hoja va saliendo */
/* el color del sello y del sobre según quién la escribe: 0 amigo (rojo), 1 familia (dorado), 2 viajero (azul), 3 criatura (verde) */
static int s_carta_tipo;
void art_carta_tipo(int t) { s_carta_tipo = t < 0 || t > 3 ? 0 : t; }
static void sello_cera(lienzo_t *l, float cx, float cy, float r, int partido)
{
    static const uint32_t C[4][4] = {{0xB02A34, 0xC8323C, 0xF0646C, 0x8A1A24}, {0xB8801C, 0xD9A12A, 0xFFD968, 0x8A5A10},
                                     {0x2C5CA8, 0x3A74C8, 0x7AAAF0, 0x1E3E80}, {0x2E8450, 0x3DA264, 0x86DCA0, 0x1E5E38}};
    const uint32_t *c = C[s_carta_tipo];
    for (int k = 0; k < 8; k++) { /* el borde ondulado de la cera derretida */
        float a = (float)k * 0.785f;
        circulo(l, cx + cosf(a) * (r - 0.6f), cy + sinf(a) * (r - 0.6f), r * 0.42f, A(c[0]));
    }
    masa(l, cx, cy, r, r, A(c[1]), A(c[2]), A(c[3]));
    circulo(l, cx, cy, r * 0.62f, mezclar(A(c[0]), A(c[3]), 0.4f)); /* el relieve: un círculo hundido con una huella */
    trazo(l, cx - r * 0.3f, cy + r * 0.3f, cx + r * 0.3f, cy - r * 0.3f, 0.5f, A(c[2]));
    circulo(l, cx + r * 0.08f, cy - r * 0.08f, r * 0.2f, A(c[2]));
    lz_set(l, (int)(cx - r * 0.5f), (int)(cy - r * 0.55f), mezclar(A(c[2]), A(0xFFFFFF), 0.5f)); /* brillito */
    if (partido) { /* una grieta en zigzag */
        float y = cy - r;
        for (int i = 0; i < 4; i++) {
            float y2 = y + r * 0.5f;
            trazo(l, cx + (i & 1 ? 1.2f : -1.2f), y, cx + (i & 1 ? -1.2f : 1.2f), y2, 0.45f, A(0x2A0A10));
            y = y2;
        }
    }
}
void art_carta(art_img_t *dst, int fase, int escala)
{
    const float kd = k_de_escala(escala);
    const int W = 60, Hh = 60;
    const int LW = (int)lroundf(W * kd), LH = (int)lroundf(Hh * kd), AMP = (int)lroundf((float)escala / kd);
    uint32_t *buf = buf_trabajo(LW * LH);
    if (!buf) return;
    lienzo_t l = {LW, LH, buf, kd};
    memset(buf, 0, (size_t)LW * LH * 4);
    /* el sobre es de noche encantada: índigo con filo dorado y estrellitas; la hoja de adentro es pergamino */
    const uint32_t PAPEL = A(0x4C4A98), PAPEL_L = A(0x8A86DA), PAPEL_O = A(0x2E2C68), ADENTRO = A(0x1E1C4A), ORO = A(0xF4CC5A), ORO_L = A(0xFFF0A8);
    if (fase < 2) {
        rect(&l, 4, 22, 52, 34, PAPEL);
        rect(&l, 4, 22, 52, 2, PAPEL_L);
        trazo(&l, 5, 55, 30, 40, 0.5f, PAPEL_O); trazo(&l, 55, 55, 30, 40, 0.5f, PAPEL_O); /* los pliegues de abajo */
        triangulo(&l, 4, 22, 56, 22, 30, 43, A(0x3E3C84));                              /* la solapa cerrada */
        trazo(&l, 4.5f, 22.5f, 30, 42.5f, 0.55f, PAPEL_O); trazo(&l, 55.5f, 22.5f, 30, 42.5f, 0.55f, PAPEL_O);
        trazo(&l, 8, 23.5f, 30, 40, 0.35f, PAPEL_L);
        rect(&l, 4, 22, 52, 1, ORO); rect(&l, 4, 54, 52, 1, ORO);                       /* el filo dorado */
        trazo(&l, 4.5f, 22.5f, 30, 42.5f, 0.35f, ORO); trazo(&l, 55.5f, 22.5f, 30, 42.5f, 0.35f, ORO);
        static const uint8_t ES[7][2] = {{9, 30}, {50, 31}, {13, 47}, {46, 48}, {22, 27}, {38, 27}, {30, 51}}; /* estrellitas */
        for (int k = 0; k < 7; k++) {
            int ex = ES[k][0], ey = ES[k][1];
            rect(&l, ex, ey, 1, 1, ORO_L);
            if (k < 4) { rect(&l, ex - 1, ey, 3, 1, ORO); rect(&l, ex, ey - 1, 1, 3, ORO); }
        }
        sello_cera(&l, 30, 41, 6.5f, fase == 1);
    } else {
        triangulo(&l, 4, 22, 56, 22, 30, 3, A(0x5A58B0)); /* la solapa abierta, hacia arriba */
        triangulo(&l, 9, 22, 51, 22, 30, 8, A(0x2A2864));  /* su cara de adentro */
        sello_cera(&l, 30, 9, 4.0f, 1);                    /* medio sello quedó en la punta */
        rect(&l, 5, 22, 50, 33, ADENTRO);                  /* el fondo del sobre */
        int sube = fase >= 3 ? (fase - 3) * 6 : 0;         /* la hoja va saliendo */
        if (fase >= 3) {
            rect(&l, 10, 20 - sube, 40, 34, A(0xFFF4D2));
            rect(&l, 10, 20 - sube, 40, 2, A(0xFFFBE8));
            rect(&l, 10, 20 - sube, 40, 1, ORO); rect(&l, 10, 53 - sube, 40, 1, ORO);
            for (int y = 26 - sube; y < 50; y += 4) rect(&l, 14, y, (y / 4) % 3 ? 30 : 22, 1, A(0xCDB68A)); /* renglones */
            rect(&l, 43, 23 - sube, 5, 1, ORO); rect(&l, 45, 21 - sube, 1, 5, ORO); rect(&l, 44, 22 - sube, 3, 3, ORO_L); /* una estrella en vez del corazón */
        }
        for (int Y = 0; Y < LH; Y++) /* el bolsillo de adelante: debajo de la V de los pliegues */
            for (int X = 0; X < LW; X++) {
                float x = (X + 0.5f) / kd, y = (Y + 0.5f) / kd;
                if (x < 4 || x >= 56 || y >= 56) continue;
                float v = 30 + 14 * (1 - fabsf(x - 30) / 26);
                if (y >= v) buf[Y * LW + X] = y < v + 1.2f ? PAPEL_L : (x < 30 ? PAPEL : mezclar(PAPEL, PAPEL_O, 0.35f));
            }
        trazo(&l, 5, 55, 30, 44, 0.4f, PAPEL_O); trazo(&l, 55, 55, 30, 44, 0.4f, PAPEL_O);
    }
    rect(&l, 4, 54, 52, 2, PAPEL_O); /* la sombra de abajo */
    for (int y = 0; y < LH; y++) /* contorno: se marca primero y se pinta después */
        for (int x = 0; x < LW; x++) {
            if (buf[y * LW + x] >> 24) continue;
            bool v = false;
            for (int d = 0; d < 4 && !v; d++) {
                int nx = x + (d == 0) - (d == 1), ny = y + (d == 2) - (d == 3);
                if (nx >= 0 && ny >= 0 && nx < LW && ny < LH && (buf[ny * LW + nx] >> 24)) v = true;
            }
            if (v) buf[y * LW + x] = 1;
        }
    for (int i = 0; i < LW * LH; i++) if (buf[i] == 1) buf[i] = A(0x2A2238);
    if (s_estilo) ico_pulir(buf, LW, kd);
    ampliar(dst, &l, AMP, false);
}

/* el dragón de cristal del fondo de la cueva (el secreto de la cueva): fase 0 dormido, 1 despierto, 2 con las alas
 * abiertas. Hecho de facetas lila y turquesa, con un corazón de cristal que brilla en el pecho */
void art_dragon_cristal(art_img_t *dst, int fase, int escala)
{
    const float kd = k_de_escala(escala);
    const int W = 44, Hh = 40;
    const int LW = (int)lroundf(W * kd), LH = (int)lroundf(Hh * kd), AMP = (int)lroundf((float)escala / kd);
    uint32_t *buf = buf_trabajo(LW * LH);
    if (!buf) return;
    lienzo_t l = {LW, LH, buf, kd};
    memset(buf, 0, (size_t)LW * LH * 4);
    const uint32_t L1 = A(0xC8B8F8), L2 = A(0xA890F0), L3 = A(0x7A68C8), T1 = A(0xB8F4EC), T2 = A(0x7FE0D6), T3 = A(0x4FB0B0), BR = A(0xFFFFFF);
    /* las alas: puntas de cristal (abiertas o recogidas) */
    if (fase == 2) {
        triangulo(&l, 14, 18, 2, 2, 10, 20, T2); triangulo(&l, 14, 18, 6, 0, 18, 14, T1); triangulo(&l, 12, 20, 0, 12, 8, 22, T3);
        triangulo(&l, 30, 18, 42, 2, 34, 20, T2); triangulo(&l, 30, 18, 38, 0, 26, 14, T1); triangulo(&l, 32, 20, 44, 12, 36, 22, T3);
    } else {
        triangulo(&l, 14, 20, 6, 10, 16, 14, T2); triangulo(&l, 30, 20, 38, 10, 28, 14, T2);
        triangulo(&l, 12, 22, 4, 16, 12, 18, T3); triangulo(&l, 32, 22, 40, 16, 32, 18, T3);
    }
    /* la cola enroscada, el cuerpo y las patas */
    for (int i = 0; i < 14; i++) { float a = (float)i * 0.32f; circulo(&l, 34 + cosf(a) * (6 - i * 0.25f), 33 - sinf(a) * 4, 3.2f - (float)i * 0.15f, i % 2 ? L2 : L1); }
    triangulo(&l, 40, 26, 43, 22, 41, 29, T1);
    elipse(&l, 22, 28, 10, 9, L1);
    triangulo(&l, 12, 28, 22, 19, 22, 37, L2); triangulo(&l, 32, 28, 22, 19, 22, 37, L3); /* facetas */
    elipse(&l, 22, 31, 6, 6, A(0xE8E0FF));
    rect(&l, 13, 34, 5, 5, L2); rect(&l, 26, 34, 5, 5, L3); rect(&l, 13, 38, 6, 1, L3); rect(&l, 26, 38, 6, 1, A(0x5A4A98));
    /* el cuello y la cabeza, con cuernitos de cristal */
    elipse(&l, 22, 17, 6, 5, L1);
    elipse(&l, 22, 12, 8, 6.5f, L1); triangulo(&l, 14, 12, 22, 6, 22, 18, L2); triangulo(&l, 30, 12, 22, 6, 22, 18, L3);
    elipse(&l, 22, 15.5f, 5, 3, A(0xE8E0FF));
    triangulo(&l, 16, 7, 18, 7, 14, 0, T2); triangulo(&l, 26, 7, 28, 7, 30, 0, T2); lz_set(&l, 15, 2, BR); lz_set(&l, 29, 2, BR);
    triangulo(&l, 21, 6, 23, 6, 22, 2, T1); /* la cresta */
    lz_set(&l, 20, 16, L3); lz_set(&l, 24, 16, L3); /* la nariz */
    if (fase == 0) { trazo(&l, 16.5f, 11.5f, 19.5f, 12.2f, 0.45f, A(0x3A2A6A)); trazo(&l, 27.5f, 11.5f, 24.5f, 12.2f, 0.45f, A(0x3A2A6A)); } /* dormido */
    else {
        circulo(&l, 18, 11.5f, 2.2f, BR); circulo(&l, 26, 11.5f, 2.2f, BR);
        circulo(&l, 18.4f, 11.8f, 1.4f, A(0x2A2238)); circulo(&l, 26.4f, 11.8f, 1.4f, A(0x2A2238));
        lz_set(&l, 18, 11, BR); lz_set(&l, 26, 11, BR);
        trazo(&l, 19.5f, 18, 24.5f, 18, 0.35f, A(0x5A4A98)); /* sonríe */
        lz_set(&l, 16, 15, A(0xFFB3C8)); lz_set(&l, 28, 15, A(0xFFB3C8));
    }
    /* el corazón de cristal en el pecho */
    triangulo(&l, 19, 26, 25, 26, 22, 31, A(0xFF6FA8)); circulo(&l, 20.4f, 25.6f, 1.7f, A(0xFF6FA8)); circulo(&l, 23.6f, 25.6f, 1.7f, A(0xFF6FA8));
    lz_set(&l, 20, 25, BR);
    if (fase > 0) resplandor(&l, 22, 27, 8, A(0xFFB3D8), 0.45f);
    /* brillitos en las facetas */
    lz_set(&l, 16, 25, BR); lz_set(&l, 28, 22, BR); lz_set(&l, 10, 12, BR); lz_set(&l, 34, 12, BR);
    for (int y = 0; y < LH; y++)
        for (int x = 0; x < LW; x++) {
            if (buf[y * LW + x] >> 24) continue;
            bool v = false;
            for (int d = 0; d < 4 && !v; d++) {
                int nx = x + (d == 0) - (d == 1), ny = y + (d == 2) - (d == 3);
                if (nx >= 0 && ny >= 0 && nx < LW && ny < LH && (buf[ny * LW + nx] >> 24)) v = true;
            }
            if (v) buf[y * LW + x] = 1;
        }
    for (int i = 0; i < LW * LH; i++) if (buf[i] == 1) buf[i] = A(0x3A2A6A);
    if (s_estilo) ico_pulir(buf, LW, kd);
    ampliar(dst, &l, AMP, false);
}

static void pintar_cueva(lienzo_t *l)
{
    if (s_estilo) { pintar_cueva2(l); return; }
    const float k = KL(l);
    const int T = (int)(2 * k) < 1 ? 1 : (int)(2 * k); /* manchas del piso, en pixeles reales */
    for (int Y = 0; Y < l->h; Y++)
        for (int X = 0; X < l->w; X++) {
            const float x = X / k, y = Y / k;
            uint32_t h = hash(X / T, Y / T, 101);
            uint32_t c = (h & 7) == 0 ? A(0x544C6C) : ((h & 7) == 1 ? A(0x766E92) : A(0x655D80));
            if ((h & 7) > 1 && (hash(X, Y, 102) & 31) == 0) c = A(0x5C5476); /* granitos */
            /* luz que entra por abajo (la entrada) */
            float luz = clampf(1.0f - hypotf((x - 82.0f) / 50.0f, (y - 160.0f) / 70.0f), 0, 1);
            c = mezclar(c, A(0xB0A6C4), luz * 0.5f);
            /* paredes alrededor */
            float d = hypotf((x - 78.0f) / 76.0f, (y - 86.0f) / 70.0f) + (float)(hash(X / (2 * T), Y / (2 * T), 103) & 7) / 60.0f;
            if (d > 1.0f && y < 150) c = A(0x2A2440);
            else if (d > 0.93f && y < 150) c = A(0x3E3756);
            lz_real(l, X, Y, c);
        }
    /* poza subterránea */
    elipse(l, wx(4, 1) - 4, wy(4, 1) - 2, 11, 6, A(0x2C3E6A)); elipse(l, wx(4, 1) - 5, wy(4, 1) - 3, 9, 4.5f, A(0x3D5A8E));
    rect(l, (int)wx(4, 1) - 8, (int)wy(4, 1) - 4, 3, 1, A(0x9ED8FF));
    /* estalagmita grande y otras chicas */
    float ex = wx(4, 2) + 2, ey = wy(4, 2) - 2;
    triangulo(l, ex - 6, ey, ex + 6, ey, ex, ey - 22, A(0x6E6890)); triangulo(l, ex - 3, ey, ex + 1, ey, ex - 1, ey - 18, A(0x8A84A8));
    static const int8_t EST[][3] = {{100, 46, 8}, {60, 40, 6}, {120, 104, 7}, {30, 100, 6}};
    for (int k = 0; k < 4; k++) triangulo(l, EST[k][0] - 3, EST[k][1], EST[k][0] + 3, EST[k][1], EST[k][0], EST[k][1] - EST[k][2], A(0x6E6890));
    /* cristales: turquesa y violeta, con brillo */
    static const int8_t CRIS[][2] = {{110, 72}, {116, 70}, {113, 66}, {34, 52}, {38, 56}, {124, 40}, {72, 30}};
    for (int k = 0; k < 7; k++) {
        uint32_t c = k % 3 == 1 ? A(0xB48CFF) : A(0x7FE0D6);
        int x = CRIS[k][0], y = CRIS[k][1];
        triangulo(l, x - 3, y + 1, x + 3, y + 1, x, y - 8, c);
        triangulo(l, x - 1, y + 1, x + 1, y + 1, x, y - 6, aclarar(c, 0.4f));
        lz_set(l, x, y - 4, A(0xFFFFFF));
        for (int dy = -8; dy <= 8; dy++) /* halo de luz */
            for (int dx = -8; dx <= 8; dx++) {
                float d = hypotf((float)dx, (float)dy);
                if (d < 8 && d > 3 && !((dx + dy) & 1)) lz_mezclar(l, x + dx, y + dy, c, 0.3f * (1 - d / 8));
            }
    }
    /* rincón con musgo (refugio) */
    elipse(l, wx(4, 5), wy(4, 5) - 3, 8, 4, A(0x3E6B4A)); elipse(l, wx(4, 5) - 1, wy(4, 5) - 4, 6, 2.5f, A(0x5A8C5A));
    roca(l, wx(4, 3) + 6, wy(4, 3) - 2, 3.5f);
}


/* la casita por dentro, estilo nuevo: paredes de troncos, piso de tablas con veta, luz de la ventana */
static void casa2_cuarto(lienzo_t *l)
{
    const float k = KL(l);
    for (int Y = 0; Y < l->h; Y++)
        for (int X = 0; X < l->w; X++) {
            const float x = (X + 0.5f) / k, y = (Y + 0.5f) / k;
            uint32_t c;
            if (y < 76) { /* troncos horizontales, redondeados con luz arriba y sombra abajo */
                float t = fmodf(y, 9.5f) / 9.5f;
                int fila = (int)(y / 9.5f);
                uint32_t base = (fila & 1) ? A(0xC98E5A) : A(0xC4885A);
                c = base;
                if (t < 0.18f) c = A(0xE0AA74);
                else if (t < 0.3f) c = ((X + Y) & 1) ? A(0xE0AA74) : base;
                else if (t > 0.86f) c = A(0x7A4A2C);
                else if (t > 0.74f) c = ((X + Y) & 1) ? A(0x9A6238) : base;
                float veta = vruido(X + fila * 37, Y, 9, 401);
                if (t > 0.3f && t < 0.74f && veta > 200 && ((X / 2 + fila) & 1)) c = oscurecer(c, 0.12f);
                /* los extremos de los troncos en las esquinas */
                if ((x < 4 || x > M - 4) && t > 0.1f && t < 0.9f) c = ((X + Y) & 1) ? A(0xE8C08A) : A(0xD8AA74);
            } else if (y < 80) c = y < 77 ? A(0x5A3820) : A(0x8A5A34); /* zócalo */
            else { /* tablas del piso con veta y junturas */
                float yy = y - 80;
                int fila = (int)(yy / 8.0f), tabla = (int)((x + (float)(fila % 3) * 17.0f) / 30.0f);
                float t = fmodf(yy, 8.0f) / 8.0f;
                uint32_t base = (tabla + fila) % 3 == 0 ? A(0xB57A48) : ((tabla + fila) % 3 == 1 ? A(0xBE8450) : A(0xAE7444));
                c = base;
                float veta = vruido(X * 1 + tabla * 51, Y * 4, 7, 402);
                if (veta > 190) c = oscurecer(base, 0.10f);
                else if (veta < 50) c = aclarar(base, 0.08f);
                if (t > 0.88f) c = A(0x6E4428);
                else if (t < 0.12f) c = aclarar(base, 0.12f);
                float xx = fmodf(x + (float)(fila % 3) * 17.0f, 30.0f);
                if (xx < 0.7f) c = A(0x6E4428);
                if ((hash(X, Y, 403) & 127) == 0) c = oscurecer(c, 0.25f); /* clavitos y nudos */
                /* las paredes de los costados hacen sombra en el piso */
                c = oscurecer(c, clampf((90.0f - y) / 30.0f, 0, 0.25f));
            }
            /* esquinas del cuarto más oscuras */
            float vig = hypotf((x - 78.0f) / 95.0f, (y - 82.0f) / 95.0f);
            if (vig > 0.6f) c = oscurecer(c, (vig - 0.6f) * 0.6f);
            lz_real(l, X, Y, c);
        }
    /* la alfombra tejida: anillos de colores con puntadas */
    sombra(l, 75, 130, 35, 12);
    static const uint32_t ANI[5] = {A(0x4E7EA8), A(0x9ED8E8), A(0xE8B44A), A(0x6FA8C8), A(0xF07A8C)};
    for (int r = 0; r < 5; r++) elipse(l, 74, 128, 34 - r * 6.0f, 12 - r * 2.1f, ANI[r]);
    for (int i = 0; i < 64; i++) { /* puntadas en el borde */
        float a = (float)i / 64 * 6.2832f;
        lz_set(l, (int)(74 + cosf(a) * 32.5f), (int)(128 + sinf(a) * 11), A(0x2E5A80));
    }
}
static void casa2_muebles(lienzo_t *l, int wx0, int wy0, int ww, int wh, bool noche)
{
    const uint32_t MAD = A(0x8B5A34), MAD_C = A(0xB57E4E), MAD_O = A(0x5A3820), BLANCO = A(0xFFF6E0);
    /* marco de la ventana, de madera, con alféizar */
    rect(l, wx0 - 3, wy0 - 3, ww + 6, 3, MAD_C); rect(l, wx0 - 3, wy0 + wh, ww + 6, 2, MAD);
    rect(l, wx0 - 3, wy0, 3, wh, MAD_C); rect(l, wx0 + ww, wy0, 3, wh, MAD);
    rect(l, wx0 + ww / 2 - 1, wy0, 2, wh, MAD); rect(l, wx0, wy0 + wh / 2 - 1, ww, 2, MAD);
    rect(l, wx0 - 6, wy0 + wh + 2, ww + 12, 3, MAD_C); rect(l, wx0 - 6, wy0 + wh + 5, ww + 12, 1, MAD_O);
    /* una maceta en el alféizar */
    rect(l, wx0 + 4, wy0 + wh - 3, 6, 5, A(0xC8603A)); rect(l, wx0 + 4, wy0 + wh - 3, 6, 1, A(0xE0805A));
    masa(l, wx0 + 7, wy0 + wh - 6, 4, 3, A(0x5DBB63), A(0x8FE08A), A(0x2E7A44));
    lz_set(l, wx0 + 6, wy0 + wh - 8, A(0xFF8FB1));
    /* cortinas con pliegues */
    for (int y = wy0 - 3; y < wy0 + wh + 6; y++) {
        int a = 7 - (y - wy0) / 7; if (a < 3) a = 3;
        for (int k = 0; k < a; k++) {
            uint32_t c = (k % 3 == 0) ? A(0xC84A64) : ((k % 3 == 1) ? A(0xF07A8C) : A(0xFF9AAC));
            lz_set(l, wx0 - 8 + k, y, c);
            lz_set(l, wx0 + ww + 7 - k, y, (k % 3 == 0) ? A(0xC84A64) : ((k % 3 == 1) ? A(0xE06A7C) : A(0xF07A8C)));
        }
    }
    rect(l, wx0 - 10, wy0 - 6, ww + 20, 2, MAD_O); circulo(l, wx0 - 10, wy0 - 5, 1.5f, MAD_C); circulo(l, wx0 + ww + 10, wy0 - 5, 1.5f, MAD_C);
    /* la luz que entra por la ventana, en el piso (de día) */
    if (!noche) {
        const float k = KL(l);
        for (int Y = (int)(80 * k); Y < (int)(124 * k); Y++)
            for (int X = 0; X < l->w; X++) {
                float x = (X + 0.5f) / k, y = (Y + 0.5f) / k, u = (y - 80) * 0.5f;
                if (x > wx0 + u && x < wx0 + ww + u && (y < 116 || ((X + Y) & 1)))
                    lz_real(l, X, Y, mezclar(lz_rget(l, X, Y), A(0xFFF0C8), 0.16f));
            }
    }
    /* repisa con libros, un frasco que brilla y un cuadro */
    sombra(l, 121, 37, 18, 1.5f);
    rect(l, 103, 33, 36, 3, MAD_C); rect(l, 103, 36, 36, 1, MAD_O);
    static const uint32_t LIB[5] = {A(0x6A5ACD), A(0xE0805A), A(0x5DBB63), A(0xE8B44A), A(0x4E7EA8)};
    for (int i = 0; i < 5; i++) { int h = 8 + (int)(hash(i, 1, 404) % 3); rect(l, 105 + i * 3, 33 - h, 3, h, LIB[i]); rect(l, 105 + i * 3, 33 - h, 1, h, aclarar(LIB[i], 0.3f)); }
    elipse(l, 125, 29, 3, 4, A(0x9ED8E8)); rect(l, 124, 24, 3, 2, MAD); lz_set(l, 124, 28, A(0xFFFFFF));
    resplandor(l, 125, 29, 7, A(0x7AF0FF), 0.3f); lz_set(l, 126, 30, A(0x7AF0FF)); lz_set(l, 125, 31, A(0xFFF6C8));
    rect(l, 130, 18, 9, 11, MAD); rect(l, 131, 19, 7, 9, A(0xBFE6FF)); rect(l, 131, 25, 7, 3, A(0x7FC86A)); circulo(l, 134, 21, 1.5f, A(0xFFE27A));
    /* la camita: cabecera tallada, colchón, edredón de retazos y almohada mullida */
    float bx = 96, by = 96;
    sombra(l, bx + 3, by + 11, 34, 4);
    rect(l, (int)bx - 31, (int)by - 26, 7, 34, MAD_O); rect(l, (int)bx - 30, (int)by - 26, 5, 33, MAD);
    rect(l, (int)bx - 30, (int)by - 26, 2, 33, MAD_C);
    circulo(l, bx - 27.5f, by - 28, 4.5f, MAD_O); circulo(l, bx - 27.5f, by - 28, 3.5f, MAD); circulo(l, bx - 28.5f, by - 29, 1.5f, MAD_C);
    rect(l, (int)bx - 26, (int)by - 4, 58, 12, MAD_O); rect(l, (int)bx - 26, (int)by - 4, 58, 3, MAD_C);
    rect(l, (int)bx - 25, (int)by - 10, 56, 7, BLANCO); rect(l, (int)bx - 25, (int)by - 4, 56, 1, A(0xD8CCB4));
    static const uint32_t RET[4] = {A(0x7E8CE0), A(0xF0B04A), A(0x7FC8A0), A(0xF07A8C)};
    for (int y = (int)by - 10; y < (int)by + 6; y++)
        for (int x = (int)bx - 5; x < (int)bx + 32; x++) {
            int cx = (x - ((int)bx - 5)) / 6, cy = (y - ((int)by - 10)) / 5;
            uint32_t c = RET[(cx + cy * 3) & 3];
            if (y == (int)by - 10) c = aclarar(c, 0.35f);
            else if (y >= (int)by + 4) c = oscurecer(c, 0.25f);
            if ((x - ((int)bx - 5)) % 6 == 0 && ((y & 1) == 0)) c = BLANCO; /* costuras */
            lz_set(l, x, y, c);
        }
    masa(l, bx - 15, by - 11, 8.5f, 3.8f, BLANCO, A(0xFFFFFF), A(0xD8CCB4));
    rect(l, (int)bx - 24, (int)by + 8, 3, 4, MAD_O); rect(l, (int)bx + 28, (int)by + 8, 3, 4, MAD_O);
    /* mesita con lámpara que da luz cálida */
    sombra(l, 141, 106, 10, 2);
    rect(l, 132, 87, 17, 3, MAD_C); rect(l, 132, 90, 17, 10, MAD); rect(l, 133, 92, 15, 1, MAD_O);
    circulo(l, 140.5f, 95, 1, A(0xE8B44A));
    rect(l, 133, 100, 2, 6, MAD_O); rect(l, 146, 100, 2, 6, MAD_O);
    rect(l, 139, 79, 3, 8, MAD_O);
    triangulo(l, 132, 81, 149, 81, 140.5f, 69, A(0xF0B04A)); triangulo(l, 133, 80, 140, 80, 140.5f, 70, A(0xFFD98A));
    resplandor(l, 140.5f, 78, noche ? 24 : 14, A(0xFFD98A), noche ? 0.55f : 0.25f);
    /* pantuflas */
    masa(l, 51, 106, 4, 2, A(0xF07A8C), A(0xFFB0BC), A(0xC84A64)); masa(l, 60, 107.5f, 4, 2, A(0xF07A8C), A(0xFFB0BC), A(0xC84A64));
    lz_set(l, 49, 105, A(0xFFFFFF)); lz_set(l, 58, 106, A(0xFFFFFF));
}

/* adentro de la casita: su camita y una ventana que muestra el cielo de afuera
 * (de noche, estrellas y la luna; con la luz apagada el cuarto queda en penumbra) */
/* la casita por dentro (versión 23): el interior del tocón. Paredes de madera curvas con los anillos del
 * árbol, puerta y ventana redondas con marco de corteza, cama de musgo con manta de hojas tejidas, lámpara
 * de hongo, chimenea de piedra con fuego (la del humito de afuera), repisas de raíces y raíces que cuelgan */
#define TRONCO_VX 106 /* la ventana redonda (centro y radio) */
#define TRONCO_VY 38
#define TRONCO_VR 19

/* un píxel real (fino) en coordenadas del mundo: para los detalles chiquitos de la casita */
static void fino(lienzo_t *l, float x, float y, uint32_t c) { lz_real(l, (int)(x * KL(l)), (int)(y * KL(l)), c); }
static void hilo(lienzo_t *l, float x0, float y0, float x1, float y1, uint32_t c)
{
    const float k = KL(l); int n = (int)(hypotf(x1 - x0, y1 - y0) * k) + 1;
    for (int i = 0; i <= n; i++) fino(l, x0 + (x1 - x0) * i / n, y0 + (y1 - y0) * i / n, c);
}
/* lo que cambia adentro: la luz (apagada mientras duerme) y el cuadro de la pared (según lo que le gusta) */
static bool s_casa_luz = true;
static int s_cuadro_tipo, s_cuadro_ico, s_casa_clave = 1000000;
void art_set_casa(bool luz, int cuadro, int icono)
{
    s_casa_luz = luz; s_cuadro_tipo = cuadro; s_cuadro_ico = icono;
    s_casa_clave = (luz ? 1000000 : 0) + cuadro * 1000 + (icono & 255);
}
#define CASA_LUCES 9
static float casa_luz_x(int i) { return 52.0f + (float)i * 9.5f; }
static float casa_luz_y(int i) { float u = (float)i / (CASA_LUCES - 1); return 11.0f + u * 4.0f + sinf(u * 3.1416f) * 7.0f; }
/* el cuadro de la pared: cambia según lo que le gusta (un paisaje de sus zonas, un animal amigo, la ballena de nubes) */
static void casa_cuadro(lienzo_t *l, float x, float y)
{
    const int px = (int)x - 8, py = (int)y - 9; /* la tela: 16 x 18 */
    const int t = s_cuadro_tipo;
    /* cielo en franjas (casi todos los cuadros) */
    static const uint32_t DIA[5] = {A(0x8AC8FF), A(0xA0D4FF), A(0xB8E0FF), A(0xCDEBFF), A(0xE0F4FF)};
    static const uint32_t TARDE[5] = {A(0x6E86E0), A(0x9A96E8), A(0xD0A0DC), A(0xF4B4C8), A(0xFFD2B8)};
    if (t == 6) for (int i = 0; i < 5; i++) rect(l, px, py + i * 4, 16, 4, TARDE[i]);
    else if (t == 3 || t == 0 || t == 2 || t == 5 || t == 7) for (int i = 0; i < 5; i++) rect(l, px, py + i * 4, 16, 4, DIA[i]);
    switch (t) {
    default:
    case 0: /* el prado: sol, lomas, un arbolito y flores */
        circulo(l, px + 12.0f, py + 4.0f, 2.4f, A(0xFFE27A)); circulo(l, px + 11.6f, py + 3.6f, 1.2f, A(0xFFF6C0));
        elipse(l, px + 4.0f, py + 12.0f, 9.0f, 4.0f, A(0x78C866)); elipse(l, px + 13.0f, py + 13.0f, 8.0f, 3.5f, A(0x5FB454));
        rect(l, px, py + 14, 16, 4, A(0x4FA444));
        rect(l, px + 3, py + 8, 1, 5, A(0x7A4A2C)); circulo(l, px + 3.5f, py + 7.0f, 2.6f, A(0x3E9A3E)); fino(l, px + 2.4f, py + 6.0f, A(0x7ADC70));
        for (int i = 0; i < 6; i++) fino(l, (float)(px + 6 + i * 2), (float)(py + 15 + (i & 1) * 2), i % 3 == 0 ? A(0xFF8AA8) : (i % 3 == 1 ? A(0xFFE27A) : A(0xFFFFFF)));
        trazo(l, px + 8.0f, py + 11.0f, px + 12.0f, py + 17.0f, 0.5f, A(0xE8C890)); /* el camino */
        break;
    case 1: /* el bosque: pinos en la bruma con luciérnagas */
        for (int i = 0; i < 5; i++) rect(l, px, py + i * 4, 16, 4, i < 2 ? A(0x2A4A5A) : (i < 4 ? A(0x356A5A) : A(0x3E7A50)));
        elipse(l, px + 8.0f, py + 12.0f, 9.0f, 2.2f, A(0x8ACFB0));
        for (int k = 0; k < 4; k++) { const float tx = px + 2.5f + (float)k * 3.8f, th = 9.0f + (float)(k & 1) * 3.0f;
            rect(l, (int)tx, py + 13, 1, 3, A(0x4A3020));
            triangulo(l, tx - 3.0f, py + 14.0f, tx + 3.5f, py + 14.0f, tx + 0.3f, py + 14.0f - th, k & 1 ? A(0x1E5A3A) : A(0x2A7A48));
            triangulo(l, tx - 2.0f, py + 11.0f, tx + 2.6f, py + 11.0f, tx + 0.3f, py + 14.0f - th + 2.0f, A(0x3E9A5E)); }
        rect(l, px, py + 15, 16, 3, A(0x2A5A38));
        rect(l, px + 11, py + 14, 1, 2, A(0xF6EAD0)); elipse(l, px + 11.5f, py + 14.0f, 2.0f, 1.2f, A(0xF0506E)); fino(l, px + 11.0f, py + 13.6f, A(0xFFFFFF));
        for (int i = 0; i < 4; i++) fino(l, (float)(px + 2 + i * 4 + (i & 1)), (float)(py + 4 + i * 2), A(0x7AF0FF));
        break;
    case 2: /* el lago: montañas, agua con reflejos, un barquito */
        triangulo(l, px - 2.0f, py + 10.0f, px + 7.0f, py + 10.0f, px + 2.5f, py + 3.0f, A(0x7A8CB8)); triangulo(l, px + 5.0f, py + 10.0f, px + 17.0f, py + 10.0f, px + 11.0f, py + 4.0f, A(0x6A7CA8));
        triangulo(l, px + 1.2f, py + 5.2f, px + 3.8f, py + 5.2f, px + 2.5f, py + 3.0f, A(0xFFFFFF));
        circulo(l, px + 12.5f, py + 3.0f, 1.8f, A(0xFFE27A));
        rect(l, px, py + 10, 16, 8, A(0x3E8AD8)); rect(l, px, py + 10, 16, 2, A(0x5AA6F0));
        for (int i = 0; i < 5; i++) trazo(l, (float)(px + 2 + (i * 5) % 12), (float)(py + 13 + i % 4), (float)(px + 5 + (i * 5) % 12), (float)(py + 13 + i % 4), 0.35f, A(0x9AD0FF));
        trazo(l, px + 12.5f, py + 12.0f, px + 12.5f, py + 17.0f, 0.4f, A(0xFFE9A0));
        rect(l, px + 5, py + 12, 1, 3, A(0x5A3820)); triangulo(l, px + 6.0f, py + 11.5f, px + 6.0f, py + 14.5f, px + 9.0f, py + 14.5f, A(0xFFFFFF)); rect(l, px + 4, py + 15, 6, 1, A(0x8A5A34));
        circulo(l, px + 2.0f, py + 16.0f, 1.2f, A(0x4FB85A)); fino(l, px + 2.0f, py + 15.4f, A(0xFF8AA8));
        break;
    case 3: /* la colina: viento y una cometa roja */
        elipse(l, px + 6.0f, py + 17.0f, 12.0f, 7.0f, A(0x5FB454)); elipse(l, px + 14.0f, py + 18.0f, 8.0f, 5.0f, A(0x78C866));
        triangulo(l, px + 10.0f, py + 3.0f, px + 12.5f, py + 6.0f, px + 10.0f, py + 9.0f, A(0xF0506E)); triangulo(l, px + 10.0f, py + 3.0f, px + 7.5f, py + 6.0f, px + 10.0f, py + 9.0f, A(0xFF8A3C));
        trazo(l, px + 10.0f, py + 3.0f, px + 10.0f, py + 9.0f, 0.3f, A(0xFFE27A));
        for (int i = 0; i < 4; i++) fino(l, px + 10.0f - (float)i * 0.8f, py + 10.0f + (float)i * 1.3f, i & 1 ? A(0xFFE27A) : A(0x6FB7FF));
        trazo(l, px + 10.0f, py + 9.0f, px + 5.0f, py + 14.0f, 0.2f, A(0xF6EAD0));
        rect(l, px + 4, py + 13, 1, 3, A(0x4A8A9A)); circulo(l, px + 4.5f, py + 12.4f, 1.1f, A(0xF6C8A0)); /* alguien mirando */
        for (int i = 0; i < 3; i++) trazo(l, px + 1.0f + (float)i, py + 5.0f + (float)i * 1.2f, px + 4.0f + (float)i, py + 5.0f + (float)i * 1.2f, 0.25f, A(0xFFFFFF));
        break;
    case 4: /* la cueva: cristales que brillan */
        for (int i = 0; i < 5; i++) rect(l, px, py + i * 4, 16, 4, i < 3 ? A(0x1E1838) : A(0x2A2250));
        elipse(l, px + 8.0f, py + 16.0f, 7.0f, 2.2f, A(0x3AB8C8));
        triangulo(l, px + 2.0f, py + 16.0f, px + 6.0f, py + 16.0f, px + 4.0f, py + 6.0f, A(0xB48CFF)); triangulo(l, px + 3.0f, py + 16.0f, px + 4.0f, py + 16.0f, px + 4.0f, py + 6.0f, A(0xE0D0FF));
        triangulo(l, px + 7.0f, py + 16.0f, px + 12.0f, py + 16.0f, px + 9.5f, py + 3.0f, A(0x5AA8FF)); triangulo(l, px + 8.0f, py + 16.0f, px + 9.5f, py + 16.0f, px + 9.5f, py + 3.0f, A(0xC0E0FF));
        triangulo(l, px + 11.0f, py + 16.0f, px + 15.0f, py + 16.0f, px + 13.0f, py + 8.0f, A(0xFF8FC0));
        fino(l, px + 3.6f, py + 8.0f, A(0xFFFFFF)); fino(l, px + 9.0f, py + 5.0f, A(0xFFFFFF)); fino(l, px + 12.6f, py + 10.0f, A(0xFFFFFF));
        for (int i = 0; i < 4; i++) fino(l, (float)(px + 1 + i * 4), (float)(py + 2 + (i * 3) % 5), A(0xFFE27A));
        break;
    case 5: /* la isla: palmera en el mar */
        rect(l, px, py + 11, 16, 7, A(0x2E88D8)); rect(l, px, py + 11, 16, 1, A(0x7AC0F8));
        elipse(l, px + 8.0f, py + 12.0f, 6.0f, 2.4f, A(0xF0D898)); elipse(l, px + 8.0f, py + 11.5f, 4.0f, 1.4f, A(0xFFF0C0));
        trazo(l, px + 8.0f, py + 12.0f, px + 9.2f, py + 5.0f, 0.5f, A(0x7A4A2C));
        for (int a = 0; a < 5; a++) { const float an = -2.6f + (float)a * 0.55f; trazo(l, px + 9.2f, py + 5.0f, px + 9.2f + cosf(an) * 4.5f, py + 5.0f + sinf(an) * 2.8f + 1.5f, 0.6f, a & 1 ? A(0x3E9A3E) : A(0x58B84A)); }
        circulo(l, px + 3.0f, py + 3.0f, 1.8f, A(0xFFE27A)); fino(l, px + 11.0f, py + 14.5f, A(0xB8E0FF)); fino(l, px + 4.0f, py + 15.5f, A(0xB8E0FF));
        break;
    case 6: /* la ballena de nubes en un cielo rosado */
        elipse(l, px + 8.0f, py + 9.0f, 6.2f, 3.4f, A(0xFFFFFF)); elipse(l, px + 7.0f, py + 10.0f, 5.2f, 2.2f, A(0xD8E0F6));
        trazo(l, px + 13.0f, py + 8.0f, px + 14.6f, py + 5.6f, 0.8f, A(0xFFFFFF)); trazo(l, px + 14.6f, py + 5.6f, px + 15.6f, py + 4.6f, 0.9f, A(0xFFFFFF)); trazo(l, px + 14.6f, py + 5.6f, px + 13.4f, py + 4.8f, 0.9f, A(0xFFFFFF));
        fino(l, px + 4.2f, py + 8.2f, A(0x3A3A6A)); fino(l, px + 4.2f, py + 8.8f, A(0x3A3A6A));
        trazo(l, px + 5.0f, py + 10.6f, px + 8.0f, py + 11.0f, 0.3f, A(0xF0A0B8));
        for (int i = 0; i < 3; i++) fino(l, px + 7.0f + (float)(i & 1), py + 5.6f - (float)i * 1.2f, A(0xE8F4FF)); /* el chorro */
        elipse(l, px + 3.0f, py + 15.0f, 4.0f, 1.6f, A(0xFFFFFF)); elipse(l, px + 12.0f, py + 16.0f, 4.6f, 1.8f, A(0xF4E8F8));
        fino(l, px + 2.0f, py + 2.0f, A(0xFFFFFF)); fino(l, px + 13.0f, py + 1.4f, A(0xFFFFFF)); fino(l, px + 9.0f, py + 3.0f, A(0xFFF6C8));
        break;
    case 7: /* un animal amigo en el prado */
        elipse(l, px + 4.0f, py + 13.0f, 9.0f, 4.0f, A(0x78C866)); rect(l, px, py + 14, 16, 4, A(0x5FB454));
        circulo(l, px + 13.0f, py + 3.5f, 2.0f, A(0xFFE27A));
        if (s_cuadro_ico > 0) poner_icono(l, (art_icono_t)s_cuadro_ico, px + 2, py + 5, 1);
        for (int i = 0; i < 4; i++) fino(l, (float)(px + 2 + i * 4), (float)(py + 16 + (i & 1)), i & 1 ? A(0xFF8AA8) : A(0xFFE27A));
        break;
    }
    /* marco de madera con su brillo */
    rect(l, (int)x - 10, (int)y - 11, 20, 2, A(0x8A5A34)); rect(l, (int)x - 10, (int)y + 9, 20, 2, A(0x5A3820));
    rect(l, (int)x - 10, (int)y - 11, 2, 22, A(0x7A4A2C)); rect(l, (int)x + 8, (int)y - 11, 2, 22, A(0x5A3820));
    rect(l, (int)x - 10, (int)y - 11, 20, 1, A(0xB07842)); rect(l, (int)x - 10, (int)y - 11, 1, 22, A(0xB07842));
    rect(l, (int)x - 8, (int)y - 9, 16, 1, A(0x3A2414)); rect(l, (int)x - 8, (int)y - 9, 1, 18, A(0x3A2414)); /* sombra interior */
    fino(l, x - 9.0f, y - 10.0f, A(0xE8C890));
}
/* los detalles de la casita por dentro: luces, hierbas, atrapasueños, mesita con té, canasta de lana, hongos... */
static void casa3_detalles(lienzo_t *l, bool noche)
{
    static const uint32_t LUZ[4] = {A(0xFFD45C), A(0xFF8AA8), A(0x8AD8FF), A(0xA8F08A)};
    /* guirnalda de luces: el cable cuelga entre dos raíces */
    for (int i = 0; i < CASA_LUCES - 1; i++) hilo(l, casa_luz_x(i), casa_luz_y(i), casa_luz_x(i + 1), casa_luz_y(i + 1), A(0x2E2018));
    for (int i = 0; i < CASA_LUCES; i++) {
        const float x = casa_luz_x(i), y = casa_luz_y(i);
        rect(l, (int)x, (int)y + 1, 1, 1, A(0x2E2018));
        elipse(l, x + 0.5f, y + 3.2f, 1.5f, 2.0f, LUZ[i & 3]);
        fino(l, x - 0.2f, y + 2.4f, aclarar(LUZ[i & 3], 0.7f));
    }
    /* hierbas y ajos colgando de las raíces */
    for (int b = 0; b < 2; b++) {
        const float x = b ? 134.0f : 72.0f, y0 = 0.0f, y1 = b ? 16.0f : 20.0f;
        hilo(l, x, y0, x, y1, A(0x6E4428));
        for (int h = 0; h < 5; h++) {
            const float a = ((float)h - 2.0f) * 0.32f;
            trazo(l, x, y1, x + sinf(a) * 9.0f, y1 + cosf(a) * 9.0f, 0.9f, b ? A(0x7A5AB0) : A(0x4FA84A));
            fino(l, x + sinf(a) * 7.0f, y1 + cosf(a) * 7.0f, b ? A(0xC8B0F0) : A(0x9ADC70));
        }
        rect(l, (int)x - 1, (int)y1 - 1, 3, 2, A(0xE8C87A)); /* el lazo */
    }
    /* un atrapasueños entre la chimenea y la ventana */
    {
        const float x = 82.0f, y = 70.0f;
        hilo(l, x, 50.0f, x, y - 7.0f, A(0x6E4428));
        for (int i = 0; i < 40; i++) { float a = (float)i / 40 * 6.2832f; fino(l, x + cosf(a) * 7.0f, y + sinf(a) * 7.0f, A(0x4FB85A)); fino(l, x + cosf(a) * 6.0f, y + sinf(a) * 6.0f, A(0x2E7A3A)); }
        for (int r = 1; r < 4; r++) for (int i = 0; i < 12; i++) { float a = (float)i / 12 * 6.2832f + (float)r; fino(l, x + cosf(a) * (float)r * 1.7f, y + sinf(a) * (float)r * 1.7f, A(0xF6EAD0)); }
        static const uint32_t PL[3] = {A(0xF07A9A), A(0x8AD8FF), A(0xFFD45C)};
        for (int p = 0; p < 3; p++) { const float px = x - 4.0f + (float)p * 4.0f; hilo(l, px, y + 7.0f, px, y + 12.0f + (float)(p & 1) * 3.0f, A(0xF6EAD0)); elipse(l, px, y + 13.0f + (float)(p & 1) * 3.0f, 1.2f, 2.4f, PL[p]); }
    }
    casa_cuadro(l, 140.0f, 62.0f);
    /* mesita redonda con tetera y dos tacitas */
    {
        const float x = 22.0f, y = 112.0f;
        sombra(l, x + 1, y + 9, 12, 3.5f);
        rect(l, (int)x - 7, (int)y + 1, 2, 8, A(0x6E4428)); rect(l, (int)x + 5, (int)y + 1, 2, 8, A(0x6E4428)); rect(l, (int)x - 1, (int)y + 2, 2, 7, A(0x5A3820));
        elipse(l, x, y, 11, 3.6f, A(0x5A3820)); elipse(l, x, y - 0.8f, 11, 3.2f, A(0xB07842)); elipse(l, x, y - 0.8f, 8, 2.2f, A(0xF6EAD0)); /* con un mantelito */
        for (int i = 0; i < 12; i++) { float a = (float)i / 12 * 6.2832f; fino(l, x + cosf(a) * 8.0f, y - 0.8f + sinf(a) * 2.2f, A(0xE07A9A)); }
        /* tetera */
        elipse(l, x - 2, y - 4.5f, 4.0f, 3.2f, A(0xE0805A)); rect(l, (int)x - 6, (int)y - 5, 8, 2, A(0xE0805A));
        rect(l, (int)x - 3, (int)y - 8, 2, 1, A(0x8A3A22)); fino(l, x - 3.5f, y - 6.2f, A(0xFFC8A8)); fino(l, x - 3.0f, y - 5.6f, A(0xFFC8A8));
        trazo(l, x + 2.0f, y - 5.5f, x + 4.6f, y - 6.6f, 0.6f, A(0xE0805A)); /* pico */
        trazo(l, x - 6.2f, y - 5.0f, x - 7.6f, y - 3.4f, 0.5f, A(0x8A3A22)); /* asa */
        for (int c2 = 0; c2 < 2; c2++) { const float cx = x + 3.0f + (float)c2 * 4.0f, cy = y - 0.5f; rect(l, (int)cx, (int)cy - 2, 3, 2, A(0xF6EAD0)); fino(l, cx + 1.0f, cy - 1.8f, A(0x8A5A34)); }
        for (int i = 0; i < 4; i++) fino(l, x + 3.5f + sinf((float)i) * 0.8f, y - 6.0f - (float)i * 1.4f, A(0xFFF6E8)); /* el vapor */
    }
    /* una canasta con ovillos de lana */
    {
        const float x = 128.0f, y = 118.0f;
        sombra(l, x, y + 4, 11, 3);
        elipse(l, x, y, 10, 5, A(0x7A4A2C));
        for (int j = 0; j < 4; j++) for (int i = 0; i < 18; i++) if (((i + j) & 1) == 0) fino(l, x - 8.5f + (float)i, y - 2.0f + (float)j * 1.4f, A(0xB07842));
        rect(l, (int)x - 11, (int)y - 5, 22, 2, A(0x5A3820));
        circulo(l, x - 4, y - 5, 4, A(0xF07A9A)); circulo(l, x + 3, y - 6, 3.4f, A(0x8AD8FF));
        for (int i = 0; i < 4; i++) { hilo(l, x - 7.0f + (float)i, y - 6.0f - (float)i * 0.4f, x - 1.0f + (float)i, y - 3.0f + (float)i * 0.3f, A(0xFFC8DC)); }
        fino(l, x - 5.5f, y - 6.5f, A(0xFFE8F0)); fino(l, x + 2.0f, y - 7.5f, A(0xD8F2FF));
        hilo(l, x + 3.0f, y - 6.0f, x + 12.0f, y - 11.0f, A(0xC8A060)); /* una aguja */
    }
    /* honguitos que brillan en el piso */
    {
        static const float HX[5] = {118.0f, 124.0f, 38.0f, 32.0f, 56.0f}, HY[5] = {134.0f, 130.0f, 134.0f, 138.0f, 142.0f};
        static const float HR[5] = {4.0f, 3.0f, 3.5f, 2.6f, 3.0f};
        for (int i = 0; i < 5; i++) {
            const float x = HX[i], y = HY[i], r = HR[i];
            rect(l, (int)x - 1, (int)y - 3, 2, 4, A(0xF6EAD0));
            elipse(l, x, y - 4, r, r * 0.65f, i < 2 ? A(0xF0506E) : A(0xB48CFF)); rect(l, (int)x - (int)r, (int)y - 4, (int)(2 * r), 1, i < 2 ? A(0xF0506E) : A(0xB48CFF));
            fino(l, x - r * 0.4f, y - 5.0f, A(0xFFFFFF)); fino(l, x + r * 0.3f, y - 4.2f, A(0xFFFFFF));
        }
    }
    /* pantuflas junto a la cama y un par de hojitas sueltas */
    elipse(l, 66, 104, 3.2f, 1.6f, A(0xF07A8C)); elipse(l, 72, 105, 3.2f, 1.6f, A(0xF07A8C)); fino(l, 65, 103.4f, A(0xFFC8D4)); fino(l, 71, 104.4f, A(0xFFC8D4));
    for (int i = 0; i < 6; i++) { float x = 40.0f + (float)(hash(i, 9, 520) % 80), y = 100.0f + (float)(hash(i, 10, 520) % 40); if (x > 60 && x < 110 && y < 140 && y > 114) continue; trazo(l, x, y, x + 2.4f, y - 0.8f, 0.6f, i & 1 ? A(0xE8943A) : A(0xC86A4A)); }
}
static void casa3_tronco(lienzo_t *l, art_periodo_t periodo)
{
    const bool noche = periodo == PERIODO_NOCHE, tarde = periodo == PERIODO_ATARDECER;
    const float k = KL(l);
    /* la pared: los anillos del árbol, curvos, alrededor de un centro arriba al medio; el piso de tierra con musgo */
    for (int Y = 0; Y < l->h; Y++)
        for (int X = 0; X < l->w; X++) {
            const float x = (X + 0.5f) / k, y = (Y + 0.5f) / k;
            uint32_t c;
            float piso = 78.0f + 6.0f * (1.0f - cosf((x - 78.0f) / 78.0f * 1.5708f)); /* el piso sube en los bordes: es redondo */
            if (y < piso) {
                float r = hypotf((x - 78.0f) * 0.9f, (y + 30.0f) * 1.1f);
                r += vruido(X, Y, 12, 501) / 255.0f * 2.2f; /* los anillos no son perfectos */
                float t = fmodf(r, 6.5f) / 6.5f;
                int anillo = (int)(r / 6.5f);
                uint32_t base = (anillo & 1) ? A(0xD9A26A) : A(0xCF955C);
                c = base;
                if (t < 0.12f) c = A(0x9A6236);                                   /* la línea del anillo */
                else if (t < 0.22f) c = ((X + Y) & 1) ? A(0xB07842) : base;
                else if (t > 0.8f) c = aclarar(base, 0.08f);
                if (vruido(X * 3, Y, 6, 502) > 225) c = oscurecer(c, 0.08f);       /* vetas */
                c = oscurecer(c, clampf((y - (piso - 14)) / 30.0f, 0, 0.18f)); /* abajo, la sombra del piso */
                if (y > piso - 2.5f) c = ((X + Y) & 1) ? A(0x6E4428) : A(0x7A4A2C); /* el borde con el piso */
            } else {
                uint32_t base = A(0x8A6440);
                float v = vruido(X, Y, 10, 503);
                c = v > 150 ? A(0x93704A) : base;
                if (v < 70) c = A(0x7A5636);
                float m = vruido(X + 300, Y, 16, 504); /* manchones de musgo */
                if (m > 170) c = m > 200 ? A(0x6A9A4A) : (((X + Y) & 1) ? A(0x5A8A3E) : c);
                if ((hash(X, Y, 505) & 63) == 0) c = A(0xB08A5A); /* piedritas */
            }
            /* el borde del tocón: más oscuro, como corteza por dentro */
            float vig = hypotf((x - 78.0f) / 80.0f, (y - 70.0f) / 86.0f);
            if (vig > 0.78f) c = oscurecer(c, clampf((vig - 0.78f) * 1.6f, 0, 0.55f));
            lz_real(l, X, Y, c);
        }
    /* la alfombra tejida, redonda */
    sombra(l, 75, 130, 35, 12);
    static const uint32_t ANI[5] = {A(0x6A9A4A), A(0xE8C87A), A(0xC86A4A), A(0x8ACC60), A(0xF0B04A)};
    for (int r = 0; r < 5; r++) elipse(l, 74, 126, 32 - r * 6.0f, 11 - r * 2.0f, ANI[r]);
    for (int i = 0; i < 56; i++) { float a = (float)i / 56 * 6.2832f; lz_set(l, (int)(74 + cosf(a) * 31), (int)(126 + sinf(a) * 10.5f), A(0x4A7A3A)); }
    /* raíces que cuelgan del techo, con honguitos */
    const uint32_t RAIZ = A(0x6E4428), RAIZ_C = A(0x9A6A40);
    for (int i = 0; i < 9; i++) {
        float x0 = 22.0f + (float)i * 14.0f + (float)(hash(i, 1, 506) % 7);
        float largo = 8.0f + (float)(hash(i, 2, 506) % 12);
        for (int j = 0; j < (int)(largo * 2); j++) {
            float y = (float)j * 0.5f, x = x0 + sinf(y * 0.5f + (float)i) * 1.6f;
            lz_set(l, (int)x, (int)y, RAIZ); if (j % 3 == 0) lz_set(l, (int)x + 1, (int)y, RAIZ_C);
        }
        if (i % 3 == 1) { elipse(l, x0 + sinf(largo * 0.5f + (float)i) * 1.6f, largo + 1, 2.5f, 1.4f, A(0xF0506E)); lz_set(l, (int)x0, (int)largo, A(0xFFFFFF)); }
    }
    /* la ventana redonda con marco de corteza: se ve el cielo */
    {
        const float vx = TRONCO_VX, vy = TRONCO_VY, vr = TRONCO_VR;
        for (int Y = (int)((vy - vr - 5) * k); Y <= (int)((vy + vr + 5) * k); Y++)
            for (int X = (int)((vx - vr - 5) * k); X <= (int)((vx + vr + 5) * k); X++) {
                float x = (X + 0.5f) / k, y = (Y + 0.5f) / k, d = hypotf(x - vx, y - vy);
                if (d > vr + 4.5f) continue;
                uint32_t c;
                if (d < vr) {
                    float t = (y - (vy - vr)) / (2 * vr);
                    c = noche ? mezclar(A(0x1A2050), A(0x2E3A78), t) : (tarde ? mezclar(A(0xFF9E6B), A(0xFFD38A), t) : mezclar(A(0x7EC8F2), A(0xC6EBFF), t));
                    if (!noche && y > vy + vr * 0.55f) c = A(0x7FC86A); /* el prado allá afuera */
                } else { /* corteza: café con grietas */
                    float g = vruido(X * 2, Y * 2, 5, 507);
                    c = g > 160 ? A(0x5A3820) : (g > 90 ? A(0x7A5234) : A(0x6A4428));
                    if (d < vr + 1.2f) c = A(0x3E2614);
                }
                lz_real(l, X, Y, c);
            }
        if (noche) {
            for (int i = 0; i < 9; i++) lz_set(l, (int)(vx - 9 + hash(i, 3, 508) % 18), (int)(vy - 9 + hash(i, 4, 508) % 14), A(0xFFF6C8));
            circulo(l, vx + 5, vy - 5, 3.5f, A(0xFFF1B0)); circulo(l, vx + 6.5f, vy - 6, 3.0f, A(0x1E2658));
        } else {
            elipse(l, vx - 4, vy - 4, 4, 1.8f, A(0xFFFFFF)); elipse(l, vx, vy - 5, 3, 2, A(0xFFFFFF));
            circulo(l, vx + 6, tarde ? vy + 3 : vy - 6, 2.5f, A(0xFFE27A));
        }
        rect(l, (int)(vx - vr - 3), (int)(vy + vr + 3), (int)(2 * vr + 6), 2, A(0x8A5A34)); /* el alféizar */
        rect(l, (int)(vx - vr - 3), (int)(vy + vr + 5), (int)(2 * vr + 6), 1, A(0x5A3820));
    }
    /* la puerta redonda, a la izquierda, de tablas con bisagras y una perilla */
    {
        const float dx = 26, dy = 66, dr = 15;
        for (int Y = (int)((dy - dr - 3) * k); Y <= (int)(84 * k); Y++)
            for (int X = (int)((dx - dr - 3) * k); X <= (int)((dx + dr + 3) * k); X++) {
                float x = (X + 0.5f) / k, y = (Y + 0.5f) / k, d = hypotf(x - dx, y - dy);
                if (y > 82 || d > dr + 2.5f) continue;
                uint32_t c;
                if (d < dr) {
                    int tabla = (int)((x - (dx - dr)) / 5.0f);
                    c = (tabla & 1) ? A(0xA8743E) : A(0x9A6838);
                    if (fmodf(x - (dx - dr), 5.0f) < 0.7f) c = A(0x5A3820);
                } else c = ((X + Y) & 1) ? A(0x5A3820) : A(0x6E4428); /* marco de corteza */
                lz_real(l, X, Y, c);
            }
        rect(l, (int)(dx - dr + 2), (int)(dy - 6), 7, 2, A(0x3A3448)); rect(l, (int)(dx - dr + 2), (int)(dy + 6), 7, 2, A(0x3A3448)); /* bisagras */
        circulo(l, dx + dr - 5, dy + 2, 1.5f, A(0xFFD45C)); lz_set(l, (int)(dx + dr - 6), (int)(dy + 1), A(0xFFFFFF));
    }
    /* repisa de raíz arriba de la puerta: un frasco con luciérnagas, libros y una piña */
    {
        for (int x = 8; x < 46; x++) { int y = 36 + (int)(sinf((float)x * 0.35f) * 0.8f); lz_set(l, x, y, A(0x6E4428)); lz_set(l, x, y + 1, A(0x8A5A34)); lz_set(l, x, y + 2, A(0x5A3820)); }
        trazo(l, 10, 38, 8, 44, 0.6f, A(0x6E4428)); trazo(l, 44, 38, 47, 45, 0.6f, A(0x6E4428)); /* las raíces que la sostienen */
        elipse(l, 15, 31, 3.5f, 4.5f, A(0x9ED8E8)); rect(l, 13, 25, 5, 2, A(0x8A5A34));
        lz_set(l, 14, 30, A(0xFFF06A)); lz_set(l, 16, 32, A(0xFFF06A)); lz_set(l, 15, 28, A(0xFFF6C8));
        resplandor(l, 15, 31, noche ? 12 : 7, A(0xFFF06A), noche ? 0.5f : 0.25f);
        static const uint32_t LIB[3] = {A(0x6A5ACD), A(0xE0805A), A(0x5DBB63)};
        for (int i = 0; i < 3; i++) { int h = 7 + (int)(hash(i, 5, 509) % 3); rect(l, 23 + i * 3, 36 - h, 3, h, LIB[i]); rect(l, 23 + i * 3, 36 - h, 1, h, aclarar(LIB[i], 0.3f)); }
        elipse(l, 39, 32, 3, 4, A(0x9A6236)); for (int i = 0; i < 4; i++) rect(l, 37, 29 + i * 2, 5, 1, A(0x6E4428)); /* la piña */
    }
    /* la chimenea de piedra con su fuego (la del humito de afuera) */
    {
        const int cx = 58, base = 80;
        for (int y = 0; y < 50; y++) for (int x = cx - 5; x < cx + 5; x++) { /* el tubo de piedra sube hasta el techo */
            int fila = y / 4, col = (x - (cx - 5) + (fila & 1) * 2) / 4;
            uint32_t c = (hash(col, fila, 510) & 1) ? A(0x9A92A8) : A(0x8A8298);
            if (y % 4 == 0 || (x - (cx - 5) + (fila & 1) * 2) % 4 == 0) c = A(0x5A5468);
            lz_set(l, x, y, c);
        }
        for (int y = 48; y < base; y++) for (int x = cx - 15; x < cx + 15; x++) { /* la boca de piedras */
            int fila = (y - 48) / 5, col = (x - (cx - 15) + (fila & 1) * 3) / 6;
            uint32_t c = (hash(col, fila, 511) % 3 == 0) ? A(0xA8A0B8) : ((hash(col, fila, 511) % 3 == 1) ? A(0x948CA4) : A(0x847C94));
            if ((y - 48) % 5 == 0 || (x - (cx - 15) + (fila & 1) * 3) % 6 == 0) c = A(0x5A5468);
            lz_set(l, x, y, c);
        }
        for (int y = 58; y < base; y++) for (int x = cx - 9; x < cx + 9; x++) { /* el hueco del fuego, en arco */
            float d = hypotf((float)(x - cx) / 9.0f, (float)(y - 66) / 8.0f);
            if (y >= 66 || d < 1.0f) lz_set(l, x, y, A(0x2A1A16));
        }
        rect(l, cx - 17, 47, 34, 3, A(0x6E4428)); rect(l, cx - 17, 50, 34, 1, A(0x4A2E1A)); /* repisa de madera */
        elipse(l, cx - 10, 44, 2, 3, A(0xFFF6E0)); rect(l, cx - 11, 40, 2, 2, A(0xFFD45C)); /* una velita */
        /* troncos y llamas */
        rect(l, cx - 7, base - 4, 14, 3, A(0x7A4A2C)); rect(l, cx - 5, base - 6, 10, 2, A(0x8A5A34));
        triangulo(l, cx - 6, base - 5, cx - 1, base - 5, cx - 3, base - 16, A(0xF2843A));
        triangulo(l, cx - 3, base - 5, cx + 4, base - 5, cx + 1, base - 19, A(0xFFB020));
        triangulo(l, cx + 1, base - 5, cx + 6, base - 5, cx + 4, base - 14, A(0xF2843A));
        triangulo(l, cx - 1, base - 5, cx + 2, base - 5, cx + 1, base - 12, A(0xFFF06A));
        resplandor(l, (float)cx, (float)base - 8, noche ? 30 : 18, A(0xFFB050), noche ? 0.5f : 0.3f);
    }
    /* la cama: marco de madera con cabecera, colchón claro, manta a cuadros de colores vivos y almohada blanca (se ve bien sobre el piso verde) */
    {
        const int bx = 100, by = 96;
        sombra(l, bx + 2, by + 14, 38, 5);
        /* patas y marco */
        rect(l, bx - 33, by + 8, 4, 8, A(0x5A3A22)); rect(l, bx + 29, by + 8, 4, 8, A(0x5A3A22));
        rect(l, bx - 34, by + 3, 68, 8, A(0x8A5A34)); rect(l, bx - 34, by + 3, 68, 2, A(0xB5804E)); rect(l, bx - 34, by + 10, 68, 1, A(0x4A2E1A));
        /* cabecera (a la izquierda) y pie (a la derecha) */
        rect(l, bx - 37, by - 15, 6, 28, A(0x8A5A34)); rect(l, bx - 37, by - 15, 6, 2, A(0xB5804E)); rect(l, bx - 37, by - 15, 1, 28, A(0xB5804E)); rect(l, bx - 32, by - 15, 1, 28, A(0x4A2E1A));
        elipse(l, bx - 34, by - 15, 3, 2, A(0xB5804E));
        rect(l, bx + 32, by - 5, 4, 16, A(0x8A5A34)); rect(l, bx + 32, by - 5, 4, 2, A(0xB5804E)); rect(l, bx + 35, by - 5, 1, 16, A(0x4A2E1A));
        /* colchón */
        rect(l, bx - 30, by - 6, 62, 11, A(0xF4EBD6)); rect(l, bx - 30, by - 6, 62, 2, A(0xFFFFFF)); rect(l, bx - 30, by + 3, 62, 2, A(0xD8C9A8));
        /* manta a cuadros */
        for (int y = by - 7; y < by + 8; y++)
            for (int x = bx - 12; x < bx + 33; x++) {
                int u = (x - (bx - 12)) / 5, v = (y - (by - 7)) / 4;
                uint32_t c = ((u + v) & 1) ? A(0xE5546B) : A(0xFFC857);
                if (u % 3 == 2 && (v & 1)) c = A(0x4FA3E0);
                if (y == by - 7) c = A(0xFFE6A0);              /* el doblez de arriba */
                else if (y >= by + 5) c = oscurecer(c, 0.25f);  /* el borde que cuelga */
                if (x == bx - 12 || x == bx + 32) c = oscurecer(c, 0.3f);
                lz_set(l, x, y, c);
            }
        /* almohada blanca con un moño rosado */
        masa(l, bx - 21, by - 9, 10, 4.5f, A(0xFFFFFF), A(0xFFFFFF), A(0xD8D0E8));
        rect(l, bx - 26, by - 10, 3, 2, A(0xFF8FB0)); rect(l, bx - 19, by - 10, 3, 2, A(0xFF8FB0)); lz_set(l, bx - 22, by - 10, A(0xE0507A));
    }
    /* la lámpara de hongo: un hongo alto que brilla */
    {
        const float lx = 140, ly = 96;
        sombra(l, lx, ly + 2, 8, 2);
        rect(l, (int)lx - 2, (int)ly - 18, 4, 20, A(0xF6EAD0)); rect(l, (int)lx - 2, (int)ly - 18, 1, 20, A(0xFFFFFF));
        elipse(l, lx, ly - 20, 10, 6, A(0xF0B04A)); rect(l, (int)lx - 11, (int)ly - 19, 22, 6, TRANSP);
        elipse(l, lx, ly - 20, 10, 6, A(0xF0B04A)); rect(l, (int)lx - 11, (int)ly - 18, 22, 3, A(0xF6EAD0));
        elipse(l, lx, ly - 21, 10, 5, A(0xFFD45C)); lz_set(l, (int)lx - 4, (int)ly - 23, A(0xFFF6C8)); lz_set(l, (int)lx + 3, (int)ly - 24, A(0xFFF6C8));
    }
    /* cortinas de la ventana redonda: dos paños de tela con pliegues, recogidos con una cinta */
    for (int lado = 0; lado < 2; lado++) {
        const float x0 = lado ? 130.0f : 80.0f; /* el borde de la tela junto a la pared */
        const int dir = lado ? -1 : 1;
        for (int y = 12; y < 56; y++) {
            const float ancho = 7.0f - (y > 38 ? (float)(y - 38) * 0.22f : 0.0f) + sinf((float)y * 0.15f) * 0.6f; /* se recoge abajo */
            for (int i = 0; i < (int)ancho; i++) {
                const int x = (int)(x0 + (float)(dir * i)) - (lado ? 0 : 0);
                uint32_t c = (i % 3 == 0) ? A(0xE88AA6) : ((i % 3 == 1) ? A(0xF6B0C4) : A(0xC96A8A)); /* pliegues */
                if (y < 15) c = oscurecer(c, 0.2f);
                lz_set(l, x, y, c);
            }
        }
        rect(l, (int)x0 - (lado ? 6 : 0), 37, 8, 2, A(0xFFE6A0)); /* la cinta que los recoge */
        fino(l, x0 + (float)dir * 7.5f, 39.5f, A(0xFFE6A0)); fino(l, x0 + (float)dir * 8.5f, 41.0f, A(0xFFD070)); /* borla */
    }
    rect(l, 77, 10, 63, 2, A(0x6E4428)); rect(l, 77, 10, 63, 1, A(0xB07842)); /* la barra de las cortinas */
    circulo(l, 76, 11, 1.8f, A(0xF0B04A)); circulo(l, 141, 11, 1.8f, A(0xF0B04A));
    /* helecho en maceta, junto a la chimenea */
    {
        const float x = 40.0f, y = 112.0f;
        sombra(l, x, y + 4, 8, 2.5f);
        rect(l, (int)x - 6, (int)y - 8, 12, 10, A(0xC8683A)); rect(l, (int)x - 7, (int)y - 9, 14, 3, A(0xE0805A)); rect(l, (int)x - 6, (int)y + 0, 12, 2, A(0x8A4426));
        for (int i = 0; i < 9; i++) { /* frondas en abanico */
            const float a = -2.5f + (float)i * 0.62f;
            const float lx = x + sinf(a) * 14.0f, ly = y - 9 - cosf(a) * 12.0f;
            trazo(l, x, y - 9, lx, ly, 0.9f, i & 1 ? A(0x3E9A4A) : A(0x5CC060));
            for (int k = 1; k < 5; k++) { const float u = (float)k / 5.0f; fino(l, x + (lx - x) * u + 1.2f, y - 9 + (ly - y + 9) * u, A(0x8AE080)); fino(l, x + (lx - x) * u - 1.2f, y - 9 + (ly - y + 9) * u, A(0x2E7A3A)); }
        }
        fino(l, x - 3, y - 6, A(0xFFC8A8)); fino(l, x - 2, y - 5, A(0xFFC8A8));
    }
    /* un cofre de madera a los pies de la cama, con un osito de peluche encima */
    {
        const float x = 146.0f, y = 112.0f;
        sombra(l, x, y + 3, 11, 3);
        rect(l, (int)x - 9, (int)y - 8, 18, 10, A(0x8A5A34)); rect(l, (int)x - 9, (int)y - 8, 18, 2, A(0xB5804E)); rect(l, (int)x - 9, (int)y + 1, 18, 1, A(0x4A2E1A));
        elipse(l, x, y - 9, 9, 3, A(0xA06C3E)); rect(l, (int)x - 9, (int)y - 10, 18, 2, A(0xA06C3E)); fino(l, x - 6, y - 11, A(0xD09A60)); /* tapa abombada */
        rect(l, (int)x - 6, (int)y - 11, 2, 12, A(0xC0C8D8)); rect(l, (int)x + 4, (int)y - 11, 2, 12, A(0xC0C8D8)); /* flejes de metal */
        rect(l, (int)x - 1, (int)y - 8, 3, 3, A(0xF0C040)); fino(l, x + 0.5f, y - 7.0f, A(0x6E4A10)); /* la cerradura */
        for (int i = 0; i < 3; i++) { fino(l, x - 8.0f, y - 6.0f + (float)i * 3.0f, A(0x4A2E1A)); fino(l, x + 8.0f, y - 6.0f + (float)i * 3.0f, A(0x4A2E1A)); }
        /* osito */
        const float ox = x + 1.0f, oy = y - 12.0f;
        circulo(l, ox, oy - 3, 3.2f, A(0xC8884A)); circulo(l, ox - 2.5f, oy - 5.5f, 1.3f, A(0xC8884A)); circulo(l, ox + 2.5f, oy - 5.5f, 1.3f, A(0xC8884A));
        elipse(l, ox, oy - 0.5f, 3.2f, 2.4f, A(0xB07838)); circulo(l, ox, oy - 2.4f, 1.2f, A(0xF0D0A0));
        fino(l, ox - 1.2f, oy - 3.6f, A(0x2A1A10)); fino(l, ox + 1.2f, oy - 3.6f, A(0x2A1A10)); fino(l, ox, oy - 2.6f, A(0x2A1A10));
        rect(l, (int)ox - 1, (int)oy + 1, 3, 1, A(0xE0507A)); /* un moñito */
    }
    /* una pila de libros con una velita sobre un banquito */
    {
        const float x = 60.0f, y = 112.0f;
        sombra(l, x, y + 3, 7, 2);
        rect(l, (int)x - 5, (int)y - 5, 10, 7, A(0x8A5A34)); rect(l, (int)x - 6, (int)y - 6, 12, 2, A(0xB5804E));
        rect(l, (int)x - 4, (int)y - 9, 9, 3, A(0x5A8AD8)); rect(l, (int)x - 4, (int)y - 9, 9, 1, A(0xA8C8FF));
        rect(l, (int)x - 3, (int)y - 12, 8, 3, A(0xE0505A)); rect(l, (int)x - 3, (int)y - 12, 8, 1, A(0xFFA0A8));
        rect(l, (int)x - 1, (int)y - 17, 3, 5, A(0xFFF6E0)); rect(l, (int)x, (int)y - 19, 1, 2, A(0xFFC030)); /* la vela */
        resplandor(l, x + 0.5f, y - 20.0f, noche ? 12 : 5, A(0xFFC050), noche ? 0.55f : 0.2f);
    }
    casa3_detalles(l, noche);
    /* de noche, la penumbra; las luces (el fuego, la lámpara, las luciérnagas y la luna) se ven encima */
    if (!s_casa_luz) { /* luz apagada (duerme): todo en penumbra, solo brasas en la chimenea y lo que entra por la ventana */
        for (int Y = 0; Y < l->h; Y++)
            for (int X = 0; X < l->w; X++) {
                float x = (X + 0.5f) / k, y = (Y + 0.5f) / k;
                if (hypotf(x - TRONCO_VX, y - TRONCO_VY) < TRONCO_VR) continue;
                lz_real(l, X, Y, mezclar(lz_rget(l, X, Y), A(0x12112A), 0.74f));
            }
        resplandor(l, 58, 76, 14, A(0xFF6A20), 0.28f); /* brasas */
        if (noche) { resplandor(l, 100, 90, 30, A(0xBFC8FF), 0.12f); }
    } else if (noche || tarde) {
        for (int Y = 0; Y < l->h; Y++)
            for (int X = 0; X < l->w; X++) {
                float x = (X + 0.5f) / k, y = (Y + 0.5f) / k;
                if (hypotf(x - TRONCO_VX, y - TRONCO_VY) < TRONCO_VR) continue;
                lz_real(l, X, Y, mezclar(lz_rget(l, X, Y), noche ? A(0x1A1838) : A(0xFF9E6B), noche ? 0.55f : 0.12f));
            }
        if (noche) {
            resplandor(l, 58, 72, 34, A(0xFFB050), 0.55f);  /* el fuego */
            resplandor(l, 140, 78, 26, A(0xFFD98A), 0.5f);  /* la lámpara de hongo */
            resplandor(l, 15, 31, 12, A(0xFFF06A), 0.45f);  /* las luciérnagas del frasco */
            elipse(l, 140, 76, 10, 5, A(0xFFE27A));
            for (int i = 0; i < CASA_LUCES; i++) resplandor(l, casa_luz_x(i) + 0.5f, casa_luz_y(i) + 3.0f, 7, i % 4 == 0 ? A(0xFFD45C) : (i % 4 == 1 ? A(0xFF8AA8) : (i % 4 == 2 ? A(0x8AD8FF) : A(0xA8F08A))), 0.6f);
            resplandor(l, 22, 106, 14, A(0xFFC878), 0.25f);
        }
    }
}

static void pintar_casa(lienzo_t *l, art_periodo_t periodo)
{
    if (s_estilo) { casa3_tronco(l, periodo); return; }
    const bool noche = periodo == PERIODO_NOCHE, tarde = periodo == PERIODO_ATARDECER;
    if (s_estilo) casa2_cuarto(l);
    else {
    /* pared con papel a rayas y zócalo */
    for (int y = 0; y < 78; y++)
        for (int x = 0; x < M; x++) lz_set(l, x, y, (x / 6) % 2 ? A(0xF2D9B8) : A(0xEBCFAA));
    for (int x = 0; x < M; x++) for (int k = 0; k < 6; k += 3) lz_set(l, x, 14 + k * 12 + (x % 12 == 0), A(0xE3C49C));
    rect(l, 0, 74, M, 4, A(0xA8744A));
    /* piso de tablas */
    for (int y = 78; y < M; y++)
        for (int x = 0; x < M; x++) {
            int tabla = (x + ((y - 78) / 9 % 2) * 13) / 26;
            uint32_t c = (y - 78) % 9 == 0 ? A(0x9A6436) : ((tabla & 1) ? A(0xC98E58) : A(0xC08452));
            if ((hash(x, y, 201) & 31) == 0) c = oscurecer(c, 0.1f);
            lz_set(l, x, y, c);
        }
    /* alfombra redonda */
    elipse(l, 74, 128, 34, 12, A(0x6FA8C8)); elipse(l, 74, 128, 28, 9, A(0x9ED8E8)); elipse(l, 74, 128, 20, 6, A(0x6FA8C8));
    }
    /* ventana con cortinas */
    int wx0 = 26, wy0 = 18, ww = 40, wh = 36;
    for (int y = wy0; y < wy0 + wh; y++)
        for (int x = wx0; x < wx0 + ww; x++) {
            float k = (float)(y - wy0) / (float)wh;
            uint32_t c = noche ? mezclar(A(0x1A2050), A(0x2E3A78), k)
                               : (tarde ? mezclar(A(0xFF9E6B), A(0xFFD38A), k) : mezclar(A(0x7EC8F2), A(0xC6EBFF), k));
            lz_set(l, x, y, c);
        }
    if (noche) {
        for (int i = 0; i < 16; i++) {
            int x = wx0 + 2 + (int)(hash(i, 1, 202) % (uint32_t)(ww - 4)), y = wy0 + 2 + (int)(hash(i, 2, 202) % (uint32_t)(wh - 8));
            lz_set(l, x, y, A(0xFFF6C8));
            if (i % 5 == 0) { lz_set(l, x - 1, y, A(0xBFC8FF)); lz_set(l, x + 1, y, A(0xBFC8FF)); lz_set(l, x, y - 1, A(0xBFC8FF)); lz_set(l, x, y + 1, A(0xBFC8FF)); }
        }
        circulo(l, wx0 + 29, wy0 + 9, 4.5f, A(0xFFF1B0)); circulo(l, wx0 + 31, wy0 + 7.5f, 4.0f, mezclar(A(0x1A2050), A(0x2E3A78), 0.2f));
    } else {
        elipse(l, wx0 + 12, wy0 + 12, 6, 2.5f, A(0xFFFFFF)); elipse(l, wx0 + 17, wy0 + 10, 5, 3, A(0xFFFFFF));
        if (tarde) circulo(l, wx0 + 30, wy0 + 26, 5, A(0xFFE27A));
        else { circulo(l, wx0 + 30, wy0 + 9, 4, A(0xFFE27A)); }
        rect(l, wx0, wy0 + wh - 6, ww, 6, A(0x7FC86A)); /* el prado allá afuera */
    }
    if (s_estilo) casa2_muebles(l, wx0, wy0, ww, wh, noche);
    else {
    rect(l, wx0 - 2, wy0 - 2, ww + 4, 2, A(0xFFFFFF)); rect(l, wx0 - 2, wy0 + wh, ww + 4, 3, A(0xFFFFFF));
    rect(l, wx0 - 2, wy0, 2, wh, A(0xFFFFFF)); rect(l, wx0 + ww, wy0, 2, wh, A(0xFFFFFF));
    rect(l, wx0 + ww / 2 - 1, wy0, 2, wh, A(0xFFFFFF)); rect(l, wx0, wy0 + wh / 2 - 1, ww, 2, A(0xFFFFFF));
    rect(l, wx0 - 6, wy0 - 5, ww + 12, 3, A(0x8B5E3C)); /* barra */
    for (int y = wy0 - 2; y < wy0 + wh + 4; y++) {
        int a = 6 - (y - wy0) / 8; if (a < 3) a = 3;
        rect(l, wx0 - 7, y, a, 1, (y % 4) ? A(0xF07A8C) : A(0xD8586E));
        rect(l, wx0 + ww + 7 - a, y, a, 1, (y % 4) ? A(0xF07A8C) : A(0xD8586E));
    }
    /* repisa con un cuadro y una planta */
    rect(l, 104, 34, 34, 3, A(0x8B5E3C));
    rect(l, 108, 18, 14, 14, A(0x8B5E3C)); rect(l, 110, 20, 10, 10, A(0xBFE6FF)); circulo(l, 115, 26, 2.5f, A(0x7FC86A)); /* paisaje */
    rect(l, 127, 26, 7, 8, A(0xE07A4A)); circulo(l, 130, 22, 4, A(0x5DBB63)); circulo(l, 127, 20, 2.5f, A(0x7FD36B));
    /* la camita: cabecera, colchón, frazada y almohada */
    float bx = 96, by = 96; /* centro del colchón */
    rect(l, (int)bx - 30, (int)by - 26, 6, 32, A(0x9A6436)); circulo(l, bx - 27, by - 27, 4, A(0x9A6436)); /* cabecera */
    rect(l, (int)bx - 26, (int)by - 6, 56, 14, A(0x8B5E3C));                                          /* base */
    rect(l, (int)bx - 25, (int)by - 9, 54, 6, A(0xFFFFFF));                                           /* colchón */
    rect(l, (int)bx - 4, (int)by - 9, 34, 15, A(0x7E8CE0));                                           /* frazada */
    for (int y = (int)by - 8; y < (int)by + 6; y += 4) for (int x = (int)bx - 2; x < (int)bx + 30; x += 4) lz_set(l, x, y, A(0xA8B4F0));
    rect(l, (int)bx - 4, (int)by - 9, 34, 2, A(0x5E6CC0));
    elipse(l, bx - 15, by - 10, 8, 3.5f, A(0xFFF6E0));                                                /* almohada */
    rect(l, (int)bx - 24, (int)by + 8, 3, 4, A(0x6A442B)); rect(l, (int)bx + 26, (int)by + 8, 3, 4, A(0x6A442B));
    /* mesita con lámpara */
    rect(l, 132, 88, 16, 12, A(0xA8744A)); rect(l, 133, 100, 2, 6, A(0x6A442B)); rect(l, 145, 100, 2, 6, A(0x6A442B));
    rect(l, 139, 80, 2, 8, A(0x6A442B)); triangulo(l, 133, 81, 147, 81, 140, 71, A(0xFFD38A));
    /* pantuflas */
    elipse(l, 52, 106, 4, 2, A(0xF07A8C)); elipse(l, 60, 107, 4, 2, A(0xF07A8C));
    }
    if (noche) { /* de noche: el cuarto en penumbra, la ventana se ve y entra luz de luna */
        for (int Y = 0; Y < l->h; Y++)
            for (int X = 0; X < l->w; X++) {
                int x = (int)(X / KL(l)), y = (int)(Y / KL(l));
                bool ventana = x >= wx0 && x < wx0 + ww && y >= wy0 && y < wy0 + wh;
                if (ventana) continue;
                uint32_t c = mezclar(lz_rget(l, X, Y), A(0x1A1838), 0.6f);
                /* rayo de luna en el piso */
                float u = (float)(y - 78) * 0.55f;
                if (y > 78 && x > wx0 + u && x < wx0 + ww + u + 6 && y < 140) c = mezclar(c, A(0xBFC8FF), 0.22f);
                lz_real(l, X, Y, c);
            }
    } else if (tarde) {
        for (int i = 0; i < l->w * l->h; i++) {
            int x = (int)((i % l->w) / KL(l)), y = (int)((i / l->w) / KL(l));
            if (!(x >= wx0 && x < wx0 + ww && y >= wy0 && y < wy0 + wh)) l->p[i] = mezclar(l->p[i], A(0xFF9E6B), 0.12f);
        }
    }
}

/* (105) ox/oy: el lienzo es un trozo del mundo que empieza en ese pixel real (el fondo armado por franjas) */
static void pintar_senderos_off(lienzo_t *l, uint32_t mascara, int ox, int oy)
{
    /* pasto gastado: una franja suave de tierra clara donde más pasea (se funde con el dibujo, sin puntitos sueltos) */
    const float k = KL(l);
    for (int a = 0, q = 0; a < 6; a++)
        for (int b = a + 1; b < 6; b++, q++) {
            if (!((mascara >> q) & 1u)) continue;
            const float x0 = wx(0, a), y0 = wy(0, a) - 1, x1 = wx(0, b), y1 = wy(0, b) - 1;
            const float vx = x1 - x0, vy = y1 - y0, L2 = vx * vx + vy * vy + 1e-6f;
            const float R = 4.2f; /* ancho de la franja, en unidades del mundo */
            const int X0 = (int)((fminf(x0, x1) - R) * k) - ox, X1 = (int)((fmaxf(x0, x1) + R) * k) - ox;
            const int Y0 = (int)((fminf(y0, y1) - R) * k) - oy, Y1 = (int)((fmaxf(y0, y1) + R) * k) - oy;
            for (int Y = Y0 < 0 ? 0 : Y0; Y <= Y1 && Y < l->h; Y++)
                for (int X = X0 < 0 ? 0 : X0; X <= X1 && X < l->w; X++) {
                    const float px = (float)(X + ox) / k, py = (float)(Y + oy) / k;
                    float t = ((px - x0) * vx + (py - y0) * vy) / L2;
                    t = t < 0 ? 0 : (t > 1 ? 1 : t);
                    const float d = hypotf(px - (x0 + vx * t), py - (y0 + vy * t)) / R; /* 0 al centro, 1 al borde */
                    if (d >= 1.0f) continue;
                    float w = 1.0f - d; w = w * w * (3.0f - 2.0f * w);                    /* borde suave */
                    w *= 0.30f + 0.12f * (float)(hash((X + ox) >> 1, (Y + oy) >> 1, 111) & 3) / 3.0f; /* un poco de irregularidad, sin ruido duro */
                    const uint32_t c = lz_rget(l, X, Y);
                    if (c == AGUA || c == AGUA_O || c == AGUA_C) continue;
                    lz_real(l, X, Y, mezclar(c, SENDA, w));
                }
        }
}
static void pintar_senderos(lienzo_t *l, uint32_t mascara) { pintar_senderos_off(l, mascara, 0, 0); }

static uint32_t tinte(uint32_t c, art_periodo_t p)
{
    if (p == PERIODO_DIA) return c;
    float r = (float)((c >> 16) & 255), g = (float)((c >> 8) & 255), b = (float)(c & 255);
    if (p == PERIODO_ATARDECER) { r = r * 1.02f + 10; g *= 0.86f; b *= 0.78f; }
    else { r = r * 0.32f + 6; g = g * 0.38f + 10; b = b * 0.62f + 34; }
    int R = r > 255 ? 255 : (int)r, G = g > 255 ? 255 : (int)g, B = b > 255 ? 255 : (int)b;
    return A(((uint32_t)R << 16) | ((uint32_t)G << 8) | (uint32_t)B);
}

/* (105) ox/oy: el lienzo es un trozo del mundo que empieza en ese pixel real (el fondo armado por franjas). Las cuentas
 * van con la coordenada de verdad, para que el borde del resplandor caiga en el mismo pixel que en el fondo entero. */
static void luz_nocturna_off(lienzo_t *l, float cx, float cy, float r, int ox, int oy)
{
    const float k = KL(l);
    for (int Y = (int)floorf((cy - r) * k) - oy; Y <= (int)floorf((cy + r) * k) - oy; Y++)
        for (int X = (int)floorf((cx - r) * k) - ox; X <= (int)floorf((cx + r) * k) - ox; X++) {
            float d = hypotf((float)(X + ox) / k - cx, (float)(Y + oy) / k - cy) / r;
            if (d >= 1.0f || X < 0 || Y < 0 || X >= l->w || Y >= l->h) continue;
            float t = (1.0f - d) * 0.55f;
            if (((X + ox + Y + oy) & 1) && d > 0.6f) t *= 0.6f; /* borde tramado */
            lz_real(l, X, Y, mezclar(lz_rget(l, X, Y), A(0xFFD98A), t));
        }
}
static void luz_nocturna(lienzo_t *l, float cx, float cy, float r) { luz_nocturna_off(l, cx, cy, r, 0, 0); }


/* ======================================================================
 * Fondos ilustrados: cada zona y estación es una imagen de 466x466 hecha a mano (pixel art),
 * guardada con paleta (hasta 255 colores) y comprimida con LZSS. El motor solo la descomprime,
 * le pone encima las decoraciones compradas y la gradúa según la hora y el clima.
 * ==================================================================== */
#include "fondos_data.inc"
#include "fondos_cruce.inc"
#define FD_LADO 466
#define FD_LIENZO 468 /* el lienzo del mundo mide 468 (3x156); la pantalla es su recorte central de 466 */

/* descomprime LZSS: bandera de 8 elementos; 1 = copia (12 bits de distancia, 4 de largo-3), 0 = byte literal */
static bool fd_lz(const uint8_t *src, const uint8_t *fin, uint8_t *dst, uint32_t total)
{
    uint32_t o = 0;
    while (o < total && src < fin) {
        const uint8_t flag = *src++;
        for (int b = 0; b < 8 && o < total; b++) {
            if (flag & (1u << b)) {
                if (src + 1 >= fin) return false;
                const unsigned lo = src[0], hi = src[1];
                src += 2;
                const unsigned off = lo | ((hi >> 4) << 8);
                unsigned len = (hi & 15u) + 3u;
                if (off == 0 || off > o) return false;
                while (len-- && o < total) { dst[o] = dst[o - off]; o++; }
            } else {
                if (src >= fin) return false;
                dst[o++] = *src++;
            }
        }
    }
    return o == total;
}

/* pone en el lienzo (468x468) el fondo de una zona y estación, sin decoraciones */
static bool fd_blob(lienzo_t *l, const uint8_t *d, uint32_t n);
/* (v101) el índice de 466×466 (217 KB) que usan los fondos se pide UNA vez y se guarda: antes se pedía y soltaba en cada
 * cambio de zona y, con la PSRAM fragmentada tras horas de uso, a veces no había un bloque libre de ese tamaño: el fondo nuevo
 * no se armaba y seguía viéndose el de la zona anterior (13-nov: en el lago, saliera por donde saliera, «llegaba» al lago) */
static uint8_t *s_fd_idx;
static uint8_t *fd_idx_buf(void)
{
    if (!s_fd_idx) s_fd_idx = malloc((size_t)FD_LADO * FD_LADO);
    return s_fd_idx;
}
static bool fd_base(lienzo_t *l, int escena, int estacion)
{
    if ((unsigned)escena >= 8 || (unsigned)estacion >= 4) return false;
    return fd_blob(l, FD_TABLA[escena][estacion].d, FD_TABLA[escena][estacion].n);
}
static bool fd_blob(lienzo_t *l, const uint8_t *d, uint32_t n)
{
    const unsigned np = d[0];
    uint8_t *idx = fd_idx_buf();
    if (!idx) return false;
    if (!fd_lz(d + 1 + np * 3, d + n, idx, (uint32_t)FD_LADO * FD_LADO)) return false;
    uint32_t pal[256];
    for (unsigned i = 0; i < np; i++) pal[i] = A(((uint32_t)d[1 + i * 3] << 16) | ((uint32_t)d[2 + i * 3] << 8) | d[3 + i * 3]);
    for (int Y = 0; Y < FD_LIENZO; Y++) {
        const int sy = Y == 0 ? 0 : (Y > FD_LADO ? FD_LADO - 1 : Y - 1);
        for (int X = 0; X < FD_LIENZO; X++) {
            const int sx = X == 0 ? 0 : (X > FD_LADO ? FD_LADO - 1 : X - 1);
            l->p[Y * FD_LIENZO + X] = pal[idx[sy * FD_LADO + sx]];
        }
    }
    return true;
}

/* la casita-tocón (decoración del prado): sprite con transparencia, esquina superior izquierda en pantalla */
static void fd_casita(lienzo_t *l, int sx, int sy, int estacion)
{
    const uint8_t *d = estacion == ESTACION_ART_OTONO ? FD_CASITA_O : estacion == ESTACION_ART_INVIERNO ? FD_CASITA_I : FD_CASITA;
    const unsigned largo = estacion == ESTACION_ART_OTONO ? FD_CASITA_LEN_O : estacion == ESTACION_ART_INVIERNO ? FD_CASITA_LEN_I : FD_CASITA_LEN;
    const unsigned np = d[0];
    uint8_t *idx = malloc((size_t)FD_CASITA_W * FD_CASITA_H);
    if (!idx) return;
    if (fd_lz(d + 1 + np * 3, d + largo, idx, (uint32_t)FD_CASITA_W * FD_CASITA_H))
        for (int j = 0; j < FD_CASITA_H; j++)
            for (int i = 0; i < FD_CASITA_W; i++) {
                const unsigned v = idx[j * FD_CASITA_W + i];
                if (v) lz_real(l, sx + i + 1, sy + j + 1, A(((uint32_t)d[1 + (v - 1) * 3] << 16) | ((uint32_t)d[2 + (v - 1) * 3] << 8) | d[3 + (v - 1) * 3]));
            }
    free(idx);
}

/* el Corazón del Mundo en la piedra del centro del círculo (si la Guardiana lo dejó) y la luz de las runas
 * (dx/dy: corrimiento en unidades del mundo, para cuando el fondo se arma por franjas) */
static void fd_corazon_off(lienzo_t *l, bool latido, float dx_, float dy_)
{
    const float cx = 78 - dx_, cy = 89 - dy_; /* el centro del círculo de piedras (en el centro de la isla, a la altura del muelle) */
    if (latido) {
        resplandor(l, cx, cy - 3, 16, A(0x7FFF7A), 0.5f);
        masa(l, cx, cy - 3.5f, 2.8f, 2.6f, A(0x5ADF6A), A(0xC8FFB0), A(0x2A8A3A));
        lz_set(l, (int)cx - 1, (int)cy - 5, A(0xFFFFFF));
        for (int i = 0; i < 7; i++) {
            const float a = (float)i / 7.0f * 6.2832f - 1.57f + 0.35f;
            resplandor(l, cx + cosf(a) * 14.7f, cy + sinf(a) * 8.0f - 4, 4, A(0xFFE27A), 0.5f);
        }
    } else resplandor(l, cx, cy - 1, 9, A(0xB48CFF), 0.25f);
}
static void fd_corazon(lienzo_t *l, bool latido) { fd_corazon_off(l, latido, 0.0f, 0.0f); }

/* gradúa el día pintado a la tarde y a la noche: sombras violetas y luces cálidas al atardecer; de noche, azul profundo
 * y desaturado, pero lo muy claro (agua, flores, ventanas) conserva un poco de luz propia */
/* un pixel del fondo graduado a la hora del día (x,y: su lugar de verdad en el lienzo, para el resplandor del atardecer) */
static uint32_t fd_grad_px(uint32_t c, art_periodo_t per, int x, int y)
{
    if (per == PERIODO_DIA) return c;
    static uint8_t lut[256];
    static bool listo;
    if (!listo) { for (int i = 0; i < 256; i++) lut[i] = (uint8_t)(powf((float)i / 255.0f, 0.9f) * 255.0f + 0.5f); listo = true; }
    const float r = (float)((c >> 16) & 255) / 255.0f, g = (float)((c >> 8) & 255) / 255.0f, b = (float)(c & 255) / 255.0f;
    const float L = r * 0.30f + g * 0.59f + b * 0.11f;
    float R, G, B;
    if (per == PERIODO_ATARDECER) {
        const float t = (float)lut[(int)(L * 255.0f)] / 255.0f;
        R = r * (0.60f * (1 - t) + 1.04f * t) + 0.03f * (1 - t);
        G = g * (0.50f * (1 - t) + 0.84f * t);
        B = b * (0.82f * (1 - t) + 0.62f * t) + 0.05f * (1 - t);
        const float dx = (float)(x - 1) - 60.0f, dy = (float)(y - 1) - 180.0f;
        float s = 1.0f - sqrtf(dx * dx + dy * dy) / 420.0f;
        if (s > 0) { s *= s; R += s * 0.16f; G += s * 0.07f; }
    } else {
        float em = L - 0.72f;
        em = em > 0 ? em * 2.2f * 0.55f : 0;
        R = (r * 0.65f + L * 0.35f) * 0.30f + 0.015f + r * em;
        G = (g * 0.65f + L * 0.35f) * 0.40f + 0.03f + g * em;
        B = (b * 0.65f + L * 0.35f) * 0.80f + 0.10f + b * em;
    }
    const int ri = R < 0 ? 0 : (R > 1 ? 255 : (int)(R * 255.0f)), gi = G < 0 ? 0 : (G > 1 ? 255 : (int)(G * 255.0f)),
              bi = B < 0 ? 0 : (B > 1 ? 255 : (int)(B * 255.0f));
    return A(((uint32_t)ri << 16) | ((uint32_t)gi << 8) | (uint32_t)bi);
}
static void fd_graduar(uint32_t *px, int MR, art_periodo_t per)
{
    if (per == PERIODO_DIA) return;
    for (int y = 0; y < MR; y++)
        for (int x = 0; x < MR; x++) px[y * MR + x] = fd_grad_px(px[y * MR + x], per, x, y);
}

typedef struct {
    lv_image_dsc_t dsc;
    uint16_t *px;
    uint32_t decor;
    int8_t estacion, nublado, escena;
    uint32_t senderos;
    int32_t clave; /* adentro de la casita: luz y cuadro */
    bool listo;
    bool parcial; /* (101b) la zona sin sus adornos (no hubo memoria para el lienzo): se reintenta */
} fondo_t;
static EXT_RAM_BSS_ATTR fondo_t s_fondos[PERIODO_COUNT];

static void art_fondo_invalidar(void) { for (int i = 0; i < PERIODO_COUNT; i++) s_fondos[i].listo = false; }
int art_estilo(void) { return s_estilo; }
static int s_fondo_rapido = 1; /* 1: el fondo se gradúa por paleta (rápido); 0: pixel por pixel (para comparar) */
void art_fondo_rapido(int on) { s_fondo_rapido = on; art_fondo_invalidar(); }

/* el color de la paleta del día pasado a la tarde / noche (lo mismo que fd_graduar, pero de un color; sin el resplandor del atardecer, que depende de dónde está el pixel) */
static uint32_t fd_grad_color(uint32_t c, art_periodo_t per)
{
    if (per == PERIODO_DIA) return c;
    static uint8_t lut[256];
    static bool listo;
    if (!listo) { for (int i = 0; i < 256; i++) lut[i] = (uint8_t)(powf((float)i / 255.0f, 0.9f) * 255.0f + 0.5f); listo = true; }
    const float r = (float)((c >> 16) & 255) / 255.0f, g = (float)((c >> 8) & 255) / 255.0f, b = (float)(c & 255) / 255.0f;
    const float L = r * 0.30f + g * 0.59f + b * 0.11f;
    float R, G, B;
    if (per == PERIODO_ATARDECER) {
        const float t = (float)lut[(int)(L * 255.0f)] / 255.0f;
        const float rs = L + (r - L) * 1.25f, gs = L + (g - L) * 1.25f, bs = L + (b - L) * 1.2f; /* colores más vivos: antes quedaban pálidos */
        R = rs * (0.62f * (1 - t) + 1.05f * t) + 0.02f * (1 - t);
        G = gs * (0.46f * (1 - t) + 0.82f * t);
        B = bs * (0.78f * (1 - t) + 0.58f * t) + 0.03f * (1 - t);
    } else {
        float em = L - 0.72f;
        em = em > 0 ? em * 2.2f * 0.55f : 0;
        const float rs = L + (r - L) * 1.2f, gs = L + (g - L) * 1.2f, bs = L + (b - L) * 1.1f; /* la noche conserva sus colores, solo azulados */
        R = (rs * 0.88f + L * 0.12f) * 0.36f + 0.010f + r * em;
        G = (gs * 0.88f + L * 0.12f) * 0.47f + 0.022f + g * em;
        B = (bs * 0.88f + L * 0.12f) * 0.92f + 0.085f + b * em;
    }
    const int ri = R < 0 ? 0 : (R > 1 ? 255 : (int)(R * 255.0f)), gi = G < 0 ? 0 : (G > 1 ? 255 : (int)(G * 255.0f)),
              bi = B < 0 ? 0 : (B > 1 ? 255 : (int)(B * 255.0f));
    return A(((uint32_t)ri << 16) | ((uint32_t)gi << 8) | (uint32_t)bi);
}
/* gris (nublado, lluvia, nieve) o helada (2), de un color */
static uint32_t fd_tinte_color(uint32_t c, int nublado)
{
    if (nublado == 2) return mezclar(c, A(0xCFE4FF), 0.28f);
    if (!nublado) return c;
    const int r = (c >> 16) & 255, g = (c >> 8) & 255, b = c & 255, y = (r * 3 + g * 6 + b) / 10;
    c = mezclar(c, A(((uint32_t)y << 16) | ((uint32_t)y << 8) | (uint32_t)y), 0.35f);
    return mezclar(c, A(0x4A5470), 0.12f);
}
/* el fondo de una zona sin decoraciones encima: se descomprime, se gradúa la PALETA (unos 200 colores, no 217 mil pixeles) y se pasa a RGB565 */
static bool fd_rapido(uint16_t *dst, int escena, int estacion, int nublado, art_periodo_t per)
{
    const uint8_t *d = FD_TABLA[escena][estacion].d;
    const uint32_t n = FD_TABLA[escena][estacion].n;
    const unsigned np = d[0];
    if (np == 0 || np > 255) return false;
    uint8_t *idx = fd_idx_buf();
    if (!idx) return false;
    if (!fd_lz(d + 1 + np * 3, d + n, idx, (uint32_t)FD_LADO * FD_LADO)) return false;
    uint32_t pal[256], palg[256], palt[256];
    for (unsigned i = 0; i < np; i++) {
        uint32_t c = A(((uint32_t)d[1 + i * 3] << 16) | ((uint32_t)d[2 + i * 3] << 8) | d[3 + i * 3]);
        if (escena == 1 && W.nieve) { /* los hongos del bosque quedan cubiertos de nieve */
            const int r = (c >> 16) & 255, g = (c >> 8) & 255, bl = c & 255;
            if (r >= 170 && g <= 120 && bl <= 120 && r > g + 70) c = mezclar(c, A(0xF4F8FF), 0.88f);
            else if (r >= 180 && g >= 110 && g <= 160 && bl >= 60 && bl <= 110 && r > g + 40) c = mezclar(c, A(0xEEF3FB), 0.85f);
        }
        pal[i] = c;
        palg[i] = escena == 4 ? c : fd_grad_color(c, per); /* la cueva es igual de día y de noche */
        palt[i] = fd_tinte_color(palg[i], nublado);
    }
    const bool brillo = per == PERIODO_ATARDECER && escena != 4; /* el resplandor cálido de la esquina izquierda */
    static EXT_RAM_BSS_ATTR uint8_t gr_r[2800], gr_g[2800];
    static bool gr_listo;
    if (brillo && !gr_listo) {
        for (int k = 0; k < 2800; k++) {
            float sv = 1.0f - sqrtf((float)(k * 64)) / 420.0f;
            if (sv > 0) { sv *= sv; gr_r[k] = (uint8_t)(sv * 0.16f * 255.0f + 0.5f); gr_g[k] = (uint8_t)(sv * 0.07f * 255.0f + 0.5f); } else { gr_r[k] = 0; gr_g[k] = 0; }
        }
        gr_listo = true;
    }
    for (int y = 0; y < FD_LADO; y++) {
        const uint8_t *fila = idx + (size_t)y * FD_LADO;
        uint16_t *o = dst + (size_t)y * FD_LADO;
        const int dy = y - 180;
        for (int x = 0; x < FD_LADO; x++) {
            uint32_t c;
            if (brillo) {
                c = palg[fila[x]];
                const int dx = x - 60, k = (dx * dx + dy * dy) >> 6;
                if (k < 2800 && (gr_r[k] | gr_g[k])) {
                    int r = (int)((c >> 16) & 255) + gr_r[k], g = (int)((c >> 8) & 255) + gr_g[k];
                    c = A(((uint32_t)(r > 255 ? 255 : r) << 16) | ((uint32_t)(g > 255 ? 255 : g) << 8) | (c & 255));
                }
                if (nublado) c = fd_tinte_color(c, nublado);
            } else c = palt[fila[x]];
            if (nublado == 2 && (hash(x + 1, y + 1, 77) & 63) == 0) c = mezclar(c, A(0xFFFFFF), 0.55f); /* escarcha */
            o[x] = (uint16_t)((((c >> 16) & 0xF8) << 8) | (((c >> 8) & 0xFC) << 3) | ((c & 0xFF) >> 3));
        }
    }
    (void)pal;
    return true;
}

/* ---- (105) el fondo CON sus adornos armado por franjas, sin el lienzo entero de 876 KB ----
 * El lienzo del mundo (468×468 en 32 bits) son 876 KB de un tirón. Tras horas de uso la PSRAM queda picada y a veces
 * no hay un bloque libre tan grande: antes la zona se dibujaba pelada (la casita del prado, el farol, el banco, las
 * flores, los senderos y el Corazón de la isla no aparecían hasta reiniciar). Ahora el fondo se arma en franjas
 * horizontales de pocas filas (el alto que quepa) y cada adorno se dibuja en las franjas que toca, corrido lo que
 * haga falta: el resultado es el mismo pixel a pixel, pero el bloque de memoria que pide es muchísimo menor. */
static bool fd_franjas(uint16_t *dst, int escena, int estacion, int nublado, art_periodo_t per, uint32_t decor,
                       uint32_t senderos, int alto)
{
    if ((unsigned)escena >= 8 || (unsigned)estacion >= 4) return false;
    if (alto < 6 || alto % 6) return false; /* múltiplo de 6: el corrimiento es exacto en unidades del mundo (3) y no cambia el tramado par/impar */
    const uint8_t *d = FD_TABLA[escena][estacion].d;
    const uint32_t n = FD_TABLA[escena][estacion].n;
    if (!d) return false;
    const unsigned np = d[0];
    if (np == 0 || np > 255) return false;
    uint8_t *idx = fd_idx_buf();
    if (!idx) return false;
    if (!fd_lz(d + 1 + np * 3, d + n, idx, (uint32_t)FD_LADO * FD_LADO)) return false;
    aplicar_estacion((art_estacion_t)estacion);
    uint32_t pal[256];
    for (unsigned i = 0; i < np; i++) {
        uint32_t c = A(((uint32_t)d[1 + i * 3] << 16) | ((uint32_t)d[2 + i * 3] << 8) | d[3 + i * 3]);
        if (escena == 1 && W.nieve) { /* los hongos del bosque, cubiertos de nieve (igual que en fd_rapido) */
            const int r = (c >> 16) & 255, g = (c >> 8) & 255, bl = c & 255;
            if (r >= 170 && g <= 120 && bl <= 120 && r > g + 70) c = mezclar(c, A(0xF4F8FF), 0.88f);
            else if (r >= 180 && g >= 110 && g <= 160 && bl >= 60 && bl <= 110 && r > g + 40) c = mezclar(c, A(0xEEF3FB), 0.85f);
        }
        pal[i] = c;
    }
    uint32_t *buf = malloc((size_t)FD_LIENZO * alto * 4);
    if (!buf) return false;
    for (int y0 = 0; y0 < FD_LIENZO; y0 += alto) {
        const int h = (y0 + alto > FD_LIENZO) ? FD_LIENZO - y0 : alto;
        lienzo_t l = {FD_LIENZO, h, buf, 3.0f};
        const int wo = y0 / 3; /* el corrimiento de la franja, en unidades del mundo */
        /* el dibujo de la zona (el lienzo de 468 es el recorte de 466 corrido un pixel) */
        for (int Y = 0; Y < h; Y++) {
            const int ly = y0 + Y, sy = ly == 0 ? 0 : (ly > FD_LADO ? FD_LADO - 1 : ly - 1);
            uint32_t *o = buf + (size_t)Y * FD_LIENZO;
            const uint8_t *fila = idx + (size_t)sy * FD_LADO;
            o[0] = pal[fila[0]];
            for (int X = 1; X < FD_LIENZO; X++) o[X] = pal[fila[X > FD_LADO ? FD_LADO - 1 : X - 1]];
        }
        if (escena == 0) { /* el prado: el pasto gastado y las cosas que compraste */
            pintar_senderos_off(&l, senderos, 0, y0);
            if (decor & (1u << 0)) {
                if (W.nieve) { monton_nieve(&l, 60, 92 - wo); monton_nieve(&l, 68, 96 - wo); monton_nieve(&l, 104, 98 - wo); }
                else { poner_icono(&l, ICO_FLOR, 60, 92 - wo, 1); poner_icono(&l, ICO_FLOR, 68, 96 - wo, 1); poner_icono(&l, ICO_FLOR, 104, 98 - wo, 1); }
            }
            if (decor & (1u << 1)) {
                if (W.nieve) { monton_nieve(&l, 62, 112 - wo); monton_nieve(&l, 69, 116 - wo); }
                else { poner_icono(&l, ICO_HONGO, 62, 112 - wo, 1); poner_icono(&l, ICO_HONGO, 69, 116 - wo, 1); }
            }
            if (decor & (1u << 2)) poner_icono(&l, ICO_FAROL, 112, 88 - wo, 2);   /* farol */
            if (decor & (1u << 3)) poner_icono(&l, ICO_BANCO, 40, 66 - wo, 2);    /* banco */
            if (decor & (1u << 4)) fd_casita(&l, 244, 71 - y0, (int)estacion);    /* la casita-tocón */
        }
        if (escena == 5) fd_corazon_off(&l, (decor & 1u) != 0, 0.0f, (float)wo);
        /* la hora del día y el clima, con la fila de verdad (el resplandor del atardecer depende del lugar) */
        for (int Y = 0; Y < h; Y++)
            for (int X = 0; X < FD_LIENZO; X++) {
                uint32_t c = buf[(size_t)Y * FD_LIENZO + X];
                if (escena != 4) c = fd_grad_px(c, per, X, y0 + Y);
                if (nublado == 2) { /* helada: todo azulado, con escarcha en el pasto */
                    c = mezclar(c, A(0xCFE4FF), 0.28f);
                    if ((hash(X, y0 + Y, 77) & 63) == 0) c = mezclar(c, A(0xFFFFFF), 0.55f);
                } else if (nublado) { /* día gris: menos saturado y un poco más oscuro */
                    const int r = (c >> 16) & 255, g = (c >> 8) & 255, b = c & 255, y = (r * 3 + g * 6 + b) / 10;
                    c = mezclar(c, A(((uint32_t)y << 16) | ((uint32_t)y << 8) | (uint32_t)y), 0.35f);
                    c = mezclar(c, A(0x4A5470), 0.12f);
                }
                buf[(size_t)Y * FD_LIENZO + X] = c;
            }
        if (per == PERIODO_NOCHE) { /* las luces, encima de todo (como en el fondo entero) */
            if (escena == 0 && (decor & (1u << 2))) luz_nocturna_off(&l, 124, 92, 22, 0, y0);
            if (escena == 0 && (decor & (1u << 4))) luz_nocturna_off(&l, 106, 49, 10, 0, y0);
            if (escena == 5) { luz_nocturna_off(&l, 129, 47, 16, 0, y0); luz_nocturna_off(&l, 50, 70, 9, 0, y0); }
        }
        /* a la pantalla: el recorte central de 466 */
        for (int Y = 0; Y < h; Y++) {
            const int oy = y0 + Y - 1;
            if (oy < 0 || oy >= FD_LADO) continue;
            const uint32_t *o = buf + (size_t)Y * FD_LIENZO;
            uint16_t *q = dst + (size_t)oy * FD_LADO;
            for (int x = 0; x < FD_LADO; x++) {
                const uint32_t c = o[x + 1];
                q[x] = (uint16_t)((((c >> 16) & 0xF8) << 8) | (((c >> 8) & 0xFC) << 3) | ((c & 0xFF) >> 3));
            }
        }
    }
    free(buf);
    return true;
}
/* (105) de prueba: hace como si nunca hubiera memoria para el lienzo entero (para ver los fondos por franjas).
 * Con un valor de 6 o más, además obliga a usar franjas de ese alto (para probar las más chicas). */
static int s_sin_lienzo;
void art_fondo_sin_lienzo(int on) { s_sin_lienzo = on > 0 ? on : 0; art_fondo_invalidar(); }
void art_fondo_rehacer(void) { art_fondo_invalidar(); } /* (105) rehace el fondo de ahora (sin cambiar nada más) */
int art_fondo_sin_lienzo_ver(void) { return s_sin_lienzo; }

static const lv_image_dsc_t *fondo_listo(fondo_t *f, uint32_t decor, art_estacion_t estacion, int nublado, int escena, uint32_t senderos)
{
    const int Wd = ART_PANTALLA;
    lv_image_cache_drop(&f->dsc);
    memset(&f->dsc, 0, sizeof(f->dsc));
    f->dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
    f->dsc.header.cf = LV_COLOR_FORMAT_RGB565;
    f->dsc.header.w = Wd;
    f->dsc.header.h = Wd;
    f->dsc.header.stride = Wd * 2;
    f->dsc.data_size = (uint32_t)(Wd * Wd * 2);
    f->dsc.data = (const uint8_t *)f->px;
    f->decor = decor;
    f->estacion = (int8_t)estacion;
    f->nublado = (int8_t)nublado;
    f->escena = (int8_t)escena;
    f->senderos = senderos;
    f->clave = s_casa_clave;
    f->listo = true;
    f->parcial = false;
    return &f->dsc;
}
/* sin memoria para el fondo: suelta los fondos de las otras horas (se rehacen cuando haga falta) */
static int s_fondo_ult, s_fondo_prev = -1; /* los dos últimos fondos pedidos: pueden estar en pantalla y NO se sueltan */
static void fondo_soltar_otros(art_periodo_t periodo)
{
    /* v88: antes se soltaba todo menos el último pedido. Si una imagen de LVGL seguía mostrando otro (el de la escena anterior,
     * una transición), quedaba apuntando a memoria liberada y reusada: así aparecía el huerto nevado en el lago o desaparecía la casita. */
    for (int i = 0; i < PERIODO_COUNT; i++) {
        if (i == (int)periodo || i == s_fondo_ult || i == s_fondo_prev || !s_fondos[i].px) continue;
        lv_image_cache_drop(&s_fondos[i].dsc);
        free(s_fondos[i].px);
        s_fondos[i].px = NULL;
        s_fondos[i].listo = false;
    }
}

/* (s_fondo_ult: el fondo que se pidió por última vez, el que se está viendo; s_fondo_prev: el anterior distinto) */
/* poca PSRAM: cada hora del día guarda su fondo de 466×466 (≈ 434 KB cada una); se sueltan las de las otras horas */
void art_fondos_soltar_otros(void) { fondo_soltar_otros((art_periodo_t)s_fondo_ult); }
/* (101b) el fondo que se ve quedó sin adornos por falta de memoria: true una vez, y queda pedido rehacerlo en la próxima llamada a art_fondo */
bool art_fondo_parcial(void)
{
    if (s_fondo_ult < 0 || s_fondo_ult >= PERIODO_COUNT) return false;
    fondo_t *f = &s_fondos[s_fondo_ult];
    if (!f->listo || !f->parcial) return false;
    f->listo = false;
    return true;
}
const lv_image_dsc_t *art_fondo(art_periodo_t periodo, uint32_t decor, art_estacion_t estacion, int nublado, int escena,
                                uint32_t senderos)
{
    if ((unsigned)escena >= ART_ESCENAS) escena = 0;
    if (escena != 0) senderos = 0;
    if (escena == 4) nublado = 0; /* adentro de la cueva no hiela: se ve igual con cualquier clima */
    if (escena == 5) senderos = 0; /* adentro de la cueva no se ve el cielo */
    if (escena == ART_ESCENA_CASA) { nublado = 0; decor = 0; estacion = ESTACION_ART_VERANO; }
    if ((unsigned)periodo >= PERIODO_COUNT) periodo = PERIODO_DIA;
    if ((unsigned)estacion >= ESTACION_ART_COUNT) estacion = ESTACION_ART_VERANO;
    if ((int)periodo != s_fondo_ult) { s_fondo_prev = s_fondo_ult; s_fondo_ult = (int)periodo; }
    fondo_t *f = &s_fondos[periodo];
    if (f->listo && f->decor == decor && f->estacion == (int8_t)estacion && f->nublado == (int8_t)nublado &&
        f->escena == (int8_t)escena && f->senderos == senderos && (escena != ART_ESCENA_CASA || f->clave == s_casa_clave)) return &f->dsc;

    if (s_fondo_rapido && ((escena >= 1 && escena <= 4) || escena == 6 || escena == 7 || (escena == 0 && decor == 0 && senderos == 0))) {
        aplicar_estacion(estacion);
        if (!f->px) f->px = malloc((size_t)ART_PANTALLA * ART_PANTALLA * 2);
        if (!f->px) { fondo_soltar_otros(periodo); f->px = malloc((size_t)ART_PANTALLA * ART_PANTALLA * 2); } /* (v101) */
        if (f->px && fd_rapido(f->px, escena, (int)estacion, nublado, periodo)) return fondo_listo(f, decor, estacion, nublado, escena, senderos);
    }

    if (escena == ART_ESCENA_CASA && PA_TABLA[4].d) { /* el interior ilustrado (pantalla 4): se gradúa con la hora y se copia al fondo */
        int px0, py0;
        const lv_image_dsc_t *im = art_pantalla_clima(4, (int)periodo, 0, 0, &px0, &py0);
        if (im && im->header.w == ART_PANTALLA && im->header.h == ART_PANTALLA) {
            if (!f->px) f->px = malloc((size_t)ART_PANTALLA * ART_PANTALLA * 2);
            if (f->px) {
                memcpy(f->px, im->data, (size_t)ART_PANTALLA * ART_PANTALLA * 2);
                if (periodo != PERIODO_DIA) { /* la chimenea ilumina su rincón: resplandor cálido que se apaga con la distancia */
                    const int gx = 175, gy = 210, gr = 120;
                    const float fuerza = periodo == PERIODO_NOCHE ? 1.0f : 0.6f;
                    uint16_t *q = (uint16_t *)f->px;
                    for (int y = gy - gr; y <= gy + gr; y++) {
                        if (y < 0 || y >= ART_PANTALLA) continue;
                        for (int x = gx - gr; x <= gx + gr; x++) {
                            if (x < 0 || x >= ART_PANTALLA) continue;
                            const float dx = (float)(x - gx), dy = (float)(y - gy) * 1.15f;
                            const float d = sqrtf(dx * dx + dy * dy) / (float)gr;
                            if (d >= 1.0f) continue;
                            const bool hueco = x >= 148 && x <= 203 && y >= 178 && y <= 232; /* el hogar: conserva el dibujo del fuego */
                            uint16_t c = q[y * ART_PANTALLA + x];
                            if (hueco) { /* las llamas (pixeles cálidos y claros) brillan más; la leña y el fondo no */
                                float r = (float)((c >> 11) & 31) * 8.0f, g = (float)((c >> 5) & 63) * 4.0f, b = (float)(c & 31) * 8.0f;
                                if (r > 150.0f && r > b + 50.0f) {
                                    const float m = 0.45f * fuerza;
                                    r += (255.0f - r) * m; g += (235.0f - g) * m; b += (150.0f - b) * m * 0.5f;
                                    q[y * ART_PANTALLA + x] = (uint16_t)((((int)r >> 3) << 11) | (((int)g >> 2) << 5) | ((int)b >> 3));
                                }
                                continue;
                            }
                            const float k = (1.0f - d) * (1.0f - d) * 0.42f * fuerza;

                            float r = (float)((c >> 11) & 31) * 8.0f, g = (float)((c >> 5) & 63) * 4.0f, b = (float)(c & 31) * 8.0f;
                            r += (255.0f - r) * k; g += (190.0f - g) * k * 0.9f; b += (90.0f - b) * k * 0.6f;
                            q[y * ART_PANTALLA + x] = (uint16_t)((((int)r >> 3) << 11) | (((int)g >> 2) << 5) | ((int)b >> 3));
                        }
                    }
                }
                art_pantalla_liberar(4);
                lv_image_cache_drop(&f->dsc);
                memset(&f->dsc, 0, sizeof(f->dsc));
                f->dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
                f->dsc.header.cf = LV_COLOR_FORMAT_RGB565;
                f->dsc.header.w = ART_PANTALLA;
                f->dsc.header.h = ART_PANTALLA;
                f->dsc.header.stride = ART_PANTALLA * 2;
                f->dsc.data_size = (uint32_t)(ART_PANTALLA * ART_PANTALLA * 2);
                f->dsc.data = (const uint8_t *)f->px;
                f->decor = decor; f->estacion = (int8_t)estacion; f->nublado = (int8_t)nublado;
                f->escena = (int8_t)escena; f->senderos = senderos; f->clave = s_casa_clave; f->listo = true;
                return &f->dsc;
            }
        } else if (im) art_pantalla_liberar(4);
    }
    const bool nuevo = escena != ART_ESCENA_CASA; /* las zonas son imágenes ilustradas; la casita por dentro sigue pintándose por código */
    const int MR = (nuevo || escena == ART_ESCENA_CASA) ? FD_LIENZO : (int)(M * K_MUNDO); /* la casita por dentro, al doble de detalle */
    lienzo_t l = {MR, MR, (nuevo && escena != ART_ESCENA_CASA && s_sin_lienzo) ? NULL : malloc((size_t)MR * MR * 4),
                  (nuevo || escena == ART_ESCENA_CASA) ? 3.0f : K_MUNDO};
    if (!l.p && !s_sin_lienzo) { fondo_soltar_otros(periodo); l.p = malloc((size_t)MR * MR * 4); } /* reintento con más lugar */
    if (!l.p && nuevo && escena != ART_ESCENA_CASA) {
        /* (105) sin los 876 KB de un tirón: el fondo CON sus adornos, armado por franjas (el alto que quepa).
         * Antes se dibujaba la zona pelada y la casita del prado no aparecía hasta reiniciar. */
        if (!f->px) f->px = malloc((size_t)ART_PANTALLA * ART_PANTALLA * 2);
        if (!f->px) { fondo_soltar_otros(periodo); f->px = malloc((size_t)ART_PANTALLA * ART_PANTALLA * 2); }
        if (f->px) {
            static const int ALTOS[] = {156, 78, 36, 12, 6};
            const int forzado = s_sin_lienzo >= 6 ? s_sin_lienzo : 0; /* de prueba */
            for (unsigned t = 0; t < sizeof ALTOS / sizeof ALTOS[0]; t++)
                if (fd_franjas(f->px, escena, (int)estacion, nublado, periodo, decor, senderos, forzado ? forzado : ALTOS[t])) {
#ifdef ESP_PLATFORM
                    if (!s_sin_lienzo) {
                        extern void hal_evento(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
                        hal_evento("fondo por franjas de %d filas (sin bloque para el lienzo): escena %d, adornos 0x%X; PSRAM %u KB, bloque mayor %u KB",
                                   ALTOS[t], escena, (unsigned)decor, (unsigned)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024),
                                   (unsigned)(heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM) / 1024));
                    }
#endif
                    return fondo_listo(f, decor, estacion, nublado, escena, senderos); /* con adornos: nada de «parcial» */
                }
            aplicar_estacion(estacion);
            if (s_fondo_rapido && fd_rapido(f->px, escena, (int)estacion, nublado, periodo)) { /* (v101) ni por franjas: la zona pelada antes que la anterior */
                const lv_image_dsc_t *d = fondo_listo(f, decor, estacion, nublado, escena, senderos);
                /* (101b) sin adornos: queda marcado y la interfaz lo reintenta */
                f->parcial = (escena == 0 && (decor != 0 || senderos != 0)) || (escena == 5 && decor != 0); /* (prado: casita, farol...; isla: el Corazón) */
                return d;
            }
        }
    }
    if (!l.p) return NULL;
    aplicar_estacion(estacion);
    if (nuevo) {
        if (!fd_base(&l, escena, (int)estacion)) { fondo_soltar_otros(periodo); if (!fd_base(&l, escena, (int)estacion)) { free(l.p); return NULL; } }
        if (escena == 1 && W.nieve) { /* los hongos rojos del bosque quedan cubiertos de nieve (solo asoma lo de abajo) */
            for (int i = 0; i < MR * MR; i++) {
                uint32_t c = l.p[i];
                int r = (c >> 16) & 255, g = (c >> 8) & 255, bl = c & 255;
                if (r >= 170 && g <= 120 && bl <= 120 && r > g + 70) l.p[i] = mezclar(c, A(0xF4F8FF), 0.88f);
                else if (r >= 180 && g >= 110 && g <= 160 && bl >= 60 && bl <= 110 && r > g + 40) l.p[i] = mezclar(c, A(0xEEF3FB), 0.85f); /* los de color arena */
            }
        }
        if (escena == 0) { /* el prado: pasto gastado por donde pasea y las cosas que compraste */
            pintar_senderos(&l, senderos);
            if (decor & (1u << 0)) { if (W.nieve) { monton_nieve(&l, 60, 92); monton_nieve(&l, 68, 96); monton_nieve(&l, 104, 98); } else { poner_icono(&l, ICO_FLOR, 60, 92, 1); poner_icono(&l, ICO_FLOR, 68, 96, 1); poner_icono(&l, ICO_FLOR, 104, 98, 1); } }
            if (decor & (1u << 1)) { if (W.nieve) { monton_nieve(&l, 62, 112); monton_nieve(&l, 69, 116); } else { poner_icono(&l, ICO_HONGO, 62, 112, 1); poner_icono(&l, ICO_HONGO, 69, 116, 1); } }
            if (decor & (1u << 2)) poner_icono(&l, ICO_FAROL, 112, 88, 2);   /* farol */
            if (decor & (1u << 3)) poner_icono(&l, ICO_BANCO, 40, 66, 2);    /* banco */
            if (decor & (1u << 4)) fd_casita(&l, 244, 71, (int)estacion);                    /* la casita-tocón */
        }
        if (escena == 5) fd_corazon(&l, (decor & 1u) != 0);
        if (escena != 4) fd_graduar(l.p, MR, periodo); /* la cueva es igual de día y de noche */
    } else pintar_casa(&l, periodo);
    for (int i = 0; i < MR * MR; i++) {
        uint32_t c = l.p[i];
        if (!nuevo) { l.p[i] = c; continue; } /* adentro no se nubla ni hiela */
        if (nublado == 2) { /* helada: todo azulado, con escarcha en el pasto */
            c = mezclar(c, A(0xCFE4FF), 0.28f);
            if ((hash(i % MR, i / MR, 77) & 63) == 0) c = mezclar(c, A(0xFFFFFF), 0.55f);
        } else if (nublado) { /* día gris: menos saturado y un poco más oscuro */
            int r = (c >> 16) & 255, g = (c >> 8) & 255, b = c & 255, y = (r * 3 + g * 6 + b) / 10;
            c = mezclar(c, A(((uint32_t)y << 16) | ((uint32_t)y << 8) | (uint32_t)y), 0.35f);
            c = mezclar(c, A(0x4A5470), 0.12f);
        }
        l.p[i] = c;
    }
    if (escena == 0 && periodo == PERIODO_NOCHE && (decor & (1u << 2))) luz_nocturna(&l, 124, 92, 22);
    if (escena == 0 && periodo == PERIODO_NOCHE && (decor & (1u << 4))) luz_nocturna(&l, 106, 49, 10);
    if (escena == 5 && periodo == PERIODO_NOCHE) { luz_nocturna(&l, 129, 47, 16); luz_nocturna(&l, 50, 70, 9); } /* el faro y las runas */

    const int Wd = ART_PANTALLA, S = (int)lroundf(ART_ESCALA_MUNDO / l.k), off = (MR * S - Wd) / 2;
    if (!f->px) f->px = malloc((size_t)Wd * Wd * 2);
    if (!f->px) { fondo_soltar_otros(periodo); f->px = malloc((size_t)Wd * Wd * 2); }
    if (!f->px) { free(l.p); return NULL; }
    for (int y = 0; y < Wd; y++)
        for (int x = 0; x < Wd; x++) {
            uint32_t c = l.p[((y + off) / S) * MR + (x + off) / S];
            f->px[y * Wd + x] = (uint16_t)((((c >> 16) & 0xF8) << 8) | (((c >> 8) & 0xFC) << 3) | ((c & 0xFF) >> 3));
        }
    free(l.p);
    lv_image_cache_drop(&f->dsc);
    memset(&f->dsc, 0, sizeof(f->dsc));
    f->dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
    f->dsc.header.cf = LV_COLOR_FORMAT_RGB565;
    f->dsc.header.w = Wd;
    f->dsc.header.h = Wd;
    f->dsc.header.stride = Wd * 2;
    f->dsc.data_size = (uint32_t)(Wd * Wd * 2);
    f->dsc.data = (const uint8_t *)f->px;
    f->decor = decor;
    f->estacion = (int8_t)estacion;
    f->nublado = (int8_t)nublado;
    f->escena = (int8_t)escena;
    f->senderos = senderos;
    f->clave = s_casa_clave;
    f->listo = true;
    f->parcial = false;
    return &f->dsc;
}


/* ---- fondos de pantalla como foto: paleta + LZSS en la flash; se descomprimen al abrir la pantalla y se sueltan al cerrarla ---- */
#define PANT_MAX 20 /* (v97) 8 → 16: las pantallas de los juegos de los lugares secretos; (1.0.17) 20: + los 4 de los juegos de a dos */
static EXT_RAM_BSS_ATTR struct { lv_image_dsc_t dsc; void *px; } s_pant[PANT_MAX];
static uint32_t s_pant_clave[PANT_MAX];
/* (v97) la interfaz dice qué pantalla está en uso (la del lugar secreto o del juego de ahora): esa no se suelta para hacer lugar */
static bool (*s_pant_en_uso)(int id);
void art_pantalla_en_uso_fn(bool (*fn)(int id)) { s_pant_en_uso = fn; }
static bool art_pantalla_en_uso(int id) { return s_pant_en_uso && s_pant_en_uso(id); }
/* la estación sobre los colores de una pantalla de campo: otoño lleva los verdes al ocre, invierno los cubre de nieve */
static uint32_t pant_estacion_color(uint32_t c, int est)
{
    if (est != ESTACION_ART_OTONO && est != ESTACION_ART_INVIERNO) return c;
    const int r = (c >> 16) & 255, g = (c >> 8) & 255, b = c & 255;
    if (!(g > r + 6 && g > b + 14)) { /* lo que no es verde: el cielo y los objetos, casi igual */
        if (est == ESTACION_ART_INVIERNO && b > r + 30 && b > g) return mezclar(c, A(0xDDE8F5), 0.25f); /* cielo invernal, más pálido */
        return c;
    }
    const float v = (float)g / 255.0f; /* claro (pasto) u oscuro (follaje) */
    if (est == ESTACION_ART_OTONO) return mezclar(c, v > 0.62f ? A(0xD8A23A) : A(0xB8581E), 0.72f);
    return mezclar(c, v > 0.55f ? A(0xF4F8FF) : A(0xCBDCE8), v > 0.55f ? 0.9f : 0.7f);
}
static const lv_image_dsc_t *pant_cargar(int id, int *x, int *y, int periodo, int estacion, int tinte);
const lv_image_dsc_t *art_pantalla(int id, int *x, int *y) { return pant_cargar(id, x, y, -1, 0, 0); }
/* una pantalla de campo con la hora (periodo 0 día, 1 atardecer, 2 noche), la estación y el clima (tinte 0, 1 gris, 2 helada) aplicados a su paleta */
const lv_image_dsc_t *art_pantalla_clima(int id, int periodo, int estacion, int tinte, int *x, int *y) { return pant_cargar(id, x, y, periodo, estacion, tinte); }
static const lv_image_dsc_t *pant_cargar(int id, int *x, int *y, int periodo, int estacion, int tinte)
{
    if (id < 0 || id >= PA_N || id >= PANT_MAX) return NULL;
    const uint32_t clave = periodo < 0 ? 0u : (1u << 16) | ((uint32_t)periodo << 8) | ((uint32_t)estacion << 4) | (uint32_t)tinte;
    if (s_pant[id].px && s_pant_clave[id] != clave) art_pantalla_liberar(id); /* otra hora o clima: se vuelve a graduar */
    s_pant_clave[id] = clave;
    if (s_pant[id].px) { if (x) *x = PA_TABLA[id].x; if (y) *y = PA_TABLA[id].y; return &s_pant[id].dsc; }
#ifdef ESP_PLATFORM
    /* poca PSRAM: las fotos de otras pantallas ya no se ven, se sueltan antes de pedir más (si no, malloc sigue en la RAM interna y la deja en 0: la placa se caía) */
    if (heap_caps_get_free_size(MALLOC_CAP_SPIRAM) < 1500u * 1024u)
        for (int k = 0; k < PANT_MAX; k++) if (k != id && !art_pantalla_en_uso(k)) art_pantalla_liberar(k);
#endif
    const uint8_t *d = PA_TABLA[id].d;
    const int w = PA_TABLA[id].w, h = PA_TABLA[id].h, alfa = PA_TABLA[id].alfa;
    const unsigned np = d[0];
    uint8_t *idx = malloc((size_t)w * h);
    if (!idx) return NULL;
    if (!fd_lz(d + 1 + np * 3, d + PA_TABLA[id].n, idx, (uint32_t)w * h)) { free(idx); return NULL; }
    void *px = malloc((size_t)w * h * (alfa ? 4 : 2));
    if (!px) { free(idx); return NULL; }
    if (alfa) { /* el índice 0 es transparente */
        uint32_t pal[256];
        pal[0] = 0;
        for (unsigned i = 1; i < np && i < 256; i++) pal[i] = 0xFF000000u | ((uint32_t)d[1 + i * 3] << 16) | ((uint32_t)d[2 + i * 3] << 8) | d[3 + i * 3];
        uint32_t *o = px;
        for (int i = 0; i < w * h; i++) o[i] = pal[idx[i]];
    } else {
        uint16_t pal[256];
        for (unsigned i = 0; i < np && i < 256; i++) {
            uint32_t c = A(((uint32_t)d[1 + i * 3] << 16) | ((uint32_t)d[2 + i * 3] << 8) | d[3 + i * 3]);
            if (periodo >= 0) {
                c = pant_estacion_color(c, estacion);
                if (id == 4 && periodo != PERIODO_DIA) c = mezclar(c, fd_grad_color(c, (art_periodo_t)periodo), 0.5f); /* el cuarto de noche: la mitad de oscuro (se veía negro) */
                else c = fd_grad_color(c, (art_periodo_t)periodo);
                c = fd_tinte_color(c, tinte);
            }
            pal[i] = (uint16_t)(((c >> 8) & 0xF800) | ((c >> 5) & 0x07E0) | ((c >> 3) & 0x1F));
        }
        uint16_t *o = px;
        for (int i = 0; i < w * h; i++) o[i] = pal[idx[i]];
    }
    free(idx);
    memset(&s_pant[id].dsc, 0, sizeof(lv_image_dsc_t));
    s_pant[id].dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
    s_pant[id].dsc.header.cf = alfa ? LV_COLOR_FORMAT_ARGB8888 : LV_COLOR_FORMAT_RGB565;
    s_pant[id].dsc.header.w = (uint32_t)w;
    s_pant[id].dsc.header.h = (uint32_t)h;
    s_pant[id].dsc.header.stride = (uint32_t)(w * (alfa ? 4 : 2));
    s_pant[id].dsc.data_size = (uint32_t)(w * h * (alfa ? 4 : 2));
    s_pant[id].dsc.data = px;
    s_pant[id].px = px;
    if (x) *x = PA_TABLA[id].x;
    if (y) *y = PA_TABLA[id].y;
    return &s_pant[id].dsc;
}
/* ---- (1.0.17) los fondos de los juegos de a dos: archivos en la memoria de recursos ---- */
static int (*s_lec_leer)(const char *, void *, int);
static int (*s_lec_tam)(const char *);
void art_lector(int (*leer)(const char *nombre, void *buf, int cap), int (*tam)(const char *nombre)) { s_lec_leer = leer; s_lec_tam = tam; }
#ifndef ESP_PLATFORM
#include "fondos_dos_sim.inc" /* (el simulador no tiene memoria de recursos: los trae adentro) */
#endif
/* lee pjN.bin entero (en la PSRAM). *n = su largo. NULL si no está o no se puede leer */
static uint8_t *dos_leer(int n, uint32_t *largo)
{
#ifndef ESP_PLATFORM
    if (!s_lec_leer && n >= 0 && n < (int)(sizeof PJ_SIM / sizeof PJ_SIM[0])) {
        uint8_t *b = malloc(PJ_SIM[n].n);
        if (b) { memcpy(b, PJ_SIM[n].d, PJ_SIM[n].n); *largo = PJ_SIM[n].n; }
        return b;
    }
#endif
    if (!s_lec_leer || !s_lec_tam) return NULL;
    char nom[16];
    snprintf(nom, sizeof nom, "pj%d.bin", n);
    const int t = s_lec_tam(nom);
    if (t < 16 || t > 600 * 1024) return NULL;
#ifdef ESP_PLATFORM
    uint8_t *b = heap_caps_malloc((size_t)t, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#else
    uint8_t *b = malloc((size_t)t);
#endif
    if (!b) return NULL;
    if (s_lec_leer(nom, b, t) != t) { free(b); return NULL; }
    *largo = (uint32_t)t;
    return b;
}
bool art_fondo_dos_hay(int n)
{
#ifndef ESP_PLATFORM
    if (!s_lec_leer) return n >= 0 && n < (int)(sizeof PJ_SIM / sizeof PJ_SIM[0]);
#endif
    if (!s_lec_tam) return false;
    char nom[16];
    snprintf(nom, sizeof nom, "pj%d.bin", n);
    return s_lec_tam(nom) >= 16;
}
const lv_image_dsc_t *art_fondo_dos(int n, int *x, int *y)
{
    const int id = ART_PANT_DOS + n;
    if (n < 0 || id >= PANT_MAX) return NULL;
    static int16_t pos[PANT_MAX - ART_PANT_DOS][2];
    if (s_pant[id].px) { if (x) *x = pos[n][0]; if (y) *y = pos[n][1]; return &s_pant[id].dsc; }
#ifdef ESP_PLATFORM
    if (heap_caps_get_free_size(MALLOC_CAP_SPIRAM) < 1500u * 1024u) /* (igual que art_pantalla: poca PSRAM, se sueltan las otras) */
        for (int k = 0; k < PANT_MAX; k++) if (k != id && !art_pantalla_en_uso(k)) art_pantalla_liberar(k);
#endif
    uint32_t largo = 0;
    uint8_t *f = dos_leer(n, &largo);
    if (!f) return NULL;
    const lv_image_dsc_t *res = NULL;
    if (largo > 12 && f[0] == 'P' && f[1] == 'J' && f[2] == 1) {
        const int alfa = f[3], w = f[4] | f[5] << 8, h = f[6] | f[7] << 8;
        pos[n][0] = (int16_t)(f[8] | f[9] << 8); pos[n][1] = (int16_t)(f[10] | f[11] << 8);
        const uint8_t *d = f + 12;
        const unsigned np = d[0];
        uint8_t *idx = (w > 0 && h > 0 && w <= 466 && h <= 466) ? malloc((size_t)w * h) : NULL;
        if (idx && 1 + np * 3 < largo - 12 && fd_lz(d + 1 + np * 3, f + largo, idx, (uint32_t)w * h)) {
            void *px = malloc((size_t)w * h * (alfa ? 4 : 2));
            if (px) {
                if (alfa) {
                    uint32_t pal[256];
                    pal[0] = 0;
                    for (unsigned i = 1; i < np && i < 256; i++) pal[i] = 0xFF000000u | ((uint32_t)d[1 + i * 3] << 16) | ((uint32_t)d[2 + i * 3] << 8) | d[3 + i * 3];
                    uint32_t *o = px;
                    for (int i = 0; i < w * h; i++) o[i] = pal[idx[i]];
                } else {
                    uint16_t pal[256] = {0};
                    for (unsigned i = 0; i < np && i < 256; i++) {
                        const uint32_t c = A(((uint32_t)d[1 + i * 3] << 16) | ((uint32_t)d[2 + i * 3] << 8) | d[3 + i * 3]);
                        pal[i] = (uint16_t)(((c >> 8) & 0xF800) | ((c >> 5) & 0x07E0) | ((c >> 3) & 0x1F));
                    }
                    uint16_t *o = px;
                    for (int i = 0; i < w * h; i++) o[i] = pal[idx[i]];
                }
                memset(&s_pant[id].dsc, 0, sizeof(lv_image_dsc_t));
                s_pant[id].dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
                s_pant[id].dsc.header.cf = alfa ? LV_COLOR_FORMAT_ARGB8888 : LV_COLOR_FORMAT_RGB565;
                s_pant[id].dsc.header.w = (uint32_t)w;
                s_pant[id].dsc.header.h = (uint32_t)h;
                s_pant[id].dsc.header.stride = (uint32_t)(w * (alfa ? 4 : 2));
                s_pant[id].dsc.data_size = (uint32_t)(w * h * (alfa ? 4 : 2));
                s_pant[id].dsc.data = px;
                s_pant[id].px = px;
                res = &s_pant[id].dsc;
            }
        }
        free(idx);
    }
    free(f);
    if (res) { if (x) *x = pos[n][0]; if (y) *y = pos[n][1]; }
    return res;
}

void art_pantalla_liberar(int id)
{
    if (id < 0 || id >= PANT_MAX || !s_pant[id].px) return;
    lv_image_cache_drop(&s_pant[id].dsc);
    free(s_pant[id].px);
    s_pant[id].px = NULL;
}

/* ======================================================================
 * El viaje de despedida: vehículos y paisajes (diseños originales)
 * ==================================================================== */
/* versión mágica: una pluma gigante que vuela, un barquito de hoja con vela de pétalo sobre un lago de
 * estrellas, un tren de luciérnagas por un puente de arcoíris y un diente de león gigante */
static const int VIAJE_TAM[VIAJE_COUNT][2] = {{62, 30}, {62, 46}, {86, 40}, {50, 76}};
static const int VIAJE_ASIENTO[VIAJE_COUNT][2] = {{30, 11}, {22, 34}, {21, 23}, {25, 88}}; /* pies de la mascota */

void art_vehiculo_asiento(art_viaje_t v, int escala, int *x, int *y)
{
    v = (art_viaje_t)((unsigned)v % VIAJE_COUNT);
    *x = VIAJE_ASIENTO[v][0] * escala;
    *y = VIAJE_ASIENTO[v][1] * escala;
}

static void rueda(lienzo_t *l, float cx, float cy, float r, int frame)
{
    circulo(l, cx, cy, r, A(0x2A2250));
    circulo(l, cx, cy, r * 0.62f, A(0x7FE7FF)); /* llanta de luz */
    float a = frame ? 0.785f : 0.0f;
    for (int k = 0; k < 2; k++) {
        float b = a + k * 1.5708f;
        trazo(l, cx - cosf(b) * r * 0.6f, cy - sinf(b) * r * 0.6f, cx + cosf(b) * r * 0.6f, cy + sinf(b) * r * 0.6f, 0.35f, A(0x2A2250));
    }
    circulo(l, cx, cy, r * 0.22f, A(0xFFFFFF));
}

/* chispitas de magia (una cruz de luz) */
static void chispa(lienzo_t *l, float x, float y, uint32_t c)
{
    lz_fino(l, x, y, A(0xFFFFFF));
    lz_fino(l, x - 0.7f, y, c); lz_fino(l, x + 0.7f, y, c); lz_fino(l, x, y - 0.7f, c); lz_fino(l, x, y + 0.7f, c);
}

void art_vehiculo(art_img_t *dst, art_viaje_t v, int frame, int escala)
{
    v = (art_viaje_t)((unsigned)v % VIAJE_COUNT);
    const float kd = k_de_escala(escala);
    const int W = VIAJE_TAM[v][0], Hh = VIAJE_TAM[v][1];
    const int LW = (int)lroundf(W * kd), LH = (int)lroundf(Hh * kd), AMP = (int)lroundf((float)escala / kd);
    uint32_t *buf = buf_trabajo(LW * LH);
    if (!buf) return;
    lienzo_t l = {LW, LH, buf, kd};
    bool sin_borde = false;
    switch (v) {
    case VIAJE_AVION: { /* pluma gigante que vuela como alfombra */
        const uint32_t AZ = A(0x5E8CFF), AZ_C = A(0x9CC8FF), CI = A(0xC8F4FF);
        elipse(&l, 32, 16, 28, 7.5f, AZ);
        elipse(&l, 32, 15, 25, 5.0f, AZ_C);
        triangulo(&l, 4, 16, 14, 10, 14, 22, AZ); /* la punta de atrás, deshilachada */
        for (int k = 0; k < 9; k++) { /* barbas de la pluma */
            float x = 10 + k * 5.5f;
            trazo(&l, x, 15.5f, x - 3, 9.5f + (k % 2), 0.35f, CI);
            trazo(&l, x, 15.5f, x - 3, 21.5f - (k % 2), 0.35f, A(0x7FA8F0));
        }
        trazo(&l, 2, 18, 60, 14, 0.55f, A(0xFFFFFF)); /* el cañón */
        circulo(&l, 59, 14, 1.4f, A(0xFFE27A));
        sin_borde = false;
        break;
    }
    case VIAJE_BARCO: { /* barquito de hoja con vela de pétalo */
        const uint32_t H = A(0x4FB86A), H_O = A(0x2E8A4A), H_C = A(0x8AE08E), P = A(0xFF9FC8), P_C = A(0xFFD0E4);
        trazo(&l, 36, 6, 36, 35, 0.55f, A(0x7A5234));            /* mástil (una ramita) */
        elipse(&l, 43, 19, 8.5f, 13, P);                          /* vela: un pétalo */
        elipse(&l, 41, 17, 5, 9, P_C);
        circulo(&l, 36, 5, 1.6f, A(0xFFE27A));                    /* lucecita en la punta */
        elipse(&l, 31, 38, 29, 7.5f, H);                          /* la hoja */
        elipse(&l, 31, 36.5f, 26, 3.5f, H_C);
        triangulo(&l, 1, 33, 7, 35, 4, 39, H);                    /* puntas curvas */
        triangulo(&l, 61, 33, 55, 35, 58, 39, H);
        trazo(&l, 6, 38, 56, 38, 0.4f, H_O);                      /* la nervadura */
        for (int k = 0; k < 5; k++) { float x = 12 + k * 9.0f; trazo(&l, x, 38, x + 4, 41.5f, 0.3f, H_O); }
        circulo(&l, 8, 31, 2.2f, A(0xFFE27A));                    /* farolito */
        if (frame) chispa(&l, 8, 27, A(0xFFF2A0));
        break;
    }
    case VIAJE_TREN: { /* tren de luciérnagas: faroles de vidrio que brillan */
        const uint32_t VI = A(0x7A5AC8), VI_C = A(0xA88CE8), AZ = A(0x4A6FD8), LUZ = A(0xFFE27A), LUZ_C = A(0xFFF6C8);
        rect(&l, 3, 21, 34, 11, LUZ);                            /* vagón: un farol grande */
        rect(&l, 5, 23, 30, 7, LUZ_C);
        for (int x = 3; x < 38; x += 8) rect(&l, x, 21, 1, 11, A(0x6A4A2A));
        rect(&l, 2, 20, 36, 1, A(0x6A4A2A)); rect(&l, 2, 31, 36, 1, A(0x6A4A2A));
        rueda(&l, 11, 34, 3.4f, frame); rueda(&l, 29, 34, 3.4f, frame);
        rect(&l, 37, 30, 8, 2, A(0x2A2250));
        rect(&l, 44, 9, 15, 23, VI);                             /* cabina */
        rect(&l, 47, 12, 9, 7, LUZ_C);
        rect(&l, 42, 7, 19, 3, A(0x2A2250));
        rect(&l, 58, 18, 18, 12, AZ);                            /* caldera */
        circulo(&l, 76, 24, 6, AZ);
        rect(&l, 58, 18, 18, 3, VI_C);
        circulo(&l, 81, 24, 2.4f, LUZ);                          /* farol */
        rect(&l, 68, 9, 5, 9, A(0x2A2250)); rect(&l, 66, 7, 9, 3, A(0x2A2250));
        for (int k = 0; k < 3; k++) chispa(&l, 70 + k * 3.5f + (frame ? 2 : 0), 4 - k * 1.2f, k == 1 ? A(0xFFB6F0) : A(0x9CF0FF)); /* humo de estrellas */
        triangulo(&l, 78, 30, 85, 37, 76, 37, VI_C);
        rect(&l, 44, 30, 36, 2, A(0x2A2250));
        rueda(&l, 51, 34, 3.8f, frame); rueda(&l, 63, 34, 3.8f, frame); rueda(&l, 73, 35, 2.8f, frame);
        trazo(&l, 51, 34, 63, 34, 0.45f, A(0xFFFFFF));
        break;
    }
    default: { /* diente de león gigante: ella va colgando del tallo */
        trazo(&l, 25, 34, 25, 68, 0.7f, A(0x5DB27A));
        trazo(&l, 25, 50, 31, 46, 0.5f, A(0x5DB27A)); elipse(&l, 33, 45, 3, 1.4f, A(0x7FD38A)); /* hojita */
        circulo(&l, 25, 32, 2.5f, A(0x9A8A5A));
        for (int k = 0; k < 28; k++) { /* las semillas con sus paraguitas */
            float a = k * 6.2832f / 28 + (frame ? 0.11f : 0.0f), r = 17 + (k % 3);
            float x = 25 + cosf(a) * r, y = 20 + sinf(a) * r * 0.95f;
            trazo(&l, 25, 32 + (y - 32) * 0.25f, x, y, 0.25f, A(0xE0DCEC));
            circulo(&l, x, y, 1.3f, A(0xFFFFFF));
        }
        circulo(&l, 25, 22, 7, A(0xF6F4FF));
        chispa(&l, frame ? 6 : 44, frame ? 6 : 10, A(0xFFE27A));
        sin_borde = true;
        break;
    }
    }
    if (!sin_borde) contornear(&l, A(0x2A2238));
    ampliar(dst, &l, AMP, false);
}

/* nube de pixel art: varias bolitas con la base plana */
static void nube_viaje(lienzo_t *l, float x, float y, float tam)
{
    uint32_t B = A(0xFFFFFF), S = A(0xDCEBF7);
    elipse(l, x, y + tam * 0.4f, tam * 2.0f, tam * 0.6f, S);
    circulo(l, x - tam, y, tam * 0.8f, B); circulo(l, x, y - tam * 0.4f, tam, B); circulo(l, x + tam, y, tam * 0.75f, B);
    rect(l, (int)(x - tam * 1.7f), (int)y, (int)(tam * 3.4f), (int)(tam * 0.6f) + 1, B);
}

static void estrellas_cielo(lienzo_t *l, int n, int hasta, uint32_t semilla)
{
    for (int i = 0; i < n; i++) {
        float x = (float)(hash(i, 1, semilla) % 156), y = (float)(hash(i, 2, semilla) % (unsigned)hasta);
        if (hash(i, 3, semilla) % 5 == 0) chispa(l, x, y, A(0xBFD8FF));
        else lz_fino(l, x, y, (hash(i, 4, semilla) & 1) ? A(0xFFFFFF) : A(0xFFF2A0));
    }
}

/* resplandor redondo: color que se apaga con el cuadrado de la distancia */
static void halo_luz(lienzo_t *l, float cx, float cy, float r0, float r1, uint32_t c, float amax)
{
    const float k = KL(l);
    for (int Y = (int)((cy - r1) * k); Y <= (int)((cy + r1) * k); Y++)
        for (int X = (int)((cx - r1) * k); X <= (int)((cx + r1) * k); X++) {
            if (X < 0 || Y < 0 || X >= l->w || Y >= l->h) continue;
            const float d = hypotf((float)X / k - cx, (float)Y / k - cy);
            if (d >= r1) continue;
            const float t = d <= r0 ? 1.0f : 1.0f - (d - r0) / (r1 - r0);
            l->p[Y * l->w + X] = mezclar(l->p[Y * l->w + X], c, amax * t * t);
        }
}
/* rayos de sol: abanico de cuñas claras que se apagan con la distancia */
static void rayos_sol(lienzo_t *l, float cx, float cy, int n, float largo, uint32_t c, float amax)
{
    const float k = KL(l);
    for (int Y = 0; Y < l->h; Y++)
        for (int X = 0; X < l->w; X++) {
            const float dx = (float)X / k - cx, dy = (float)Y / k - cy, d = hypotf(dx, dy);
            if (d < 4.0f || d > largo) continue;
            const float v = sinf(atan2f(dy, dx) * (float)n);
            if (v < 0.35f) continue;
            l->p[Y * l->w + X] = mezclar(l->p[Y * l->w + X], c, amax * (1.0f - d / largo) * (v - 0.35f) / 0.65f);
        }
}
/* una cinta de aurora que ondula */
static void aurora_banda(lienzo_t *l, float y0, float alto, uint32_t c, float amax, float fase)
{
    const float k = KL(l);
    for (int X = 0; X < l->w; X++) {
        const float x = (float)X / k, yy = y0 + 6.0f * sinf(x * 0.06f + fase) + 2.0f * sinf(x * 0.17f + fase * 2.0f);
        for (int Y = (int)(yy * k); Y < (int)((yy + alto) * k); Y++) {
            if (Y < 0 || Y >= l->h) continue;
            const float t = ((float)Y / k - yy) / alto;
            l->p[Y * l->w + X] = mezclar(l->p[Y * l->w + X], c, amax * (1.0f - fabsf(t * 2.0f - 1.0f)) * (0.7f + 0.3f * sinf(x * 0.5f + fase)));
        }
    }
}
void art_viaje_fondo(art_img_t *dst, art_viaje_t v)
{
    v = (art_viaje_t)((unsigned)v % VIAJE_COUNT);
    const int D = 156;
    const float k = 1.5f;
    const int LR = (int)(D * k);
    lienzo_t l = {LR, LR, malloc((size_t)LR * LR * 4), k};
    if (!l.p) return;
    static const uint32_t ARRIBA[VIAJE_COUNT] = {A(0x2A1E5C), A(0x0E1638), A(0x141A44), A(0xF5A3B8)};
    static const uint32_t ABAJO[VIAJE_COUNT] = {A(0xF0A0C8), A(0x3A3A78), A(0x5A4A98), A(0xFFE2B0)};
    for (int Y = 0; Y < LR; Y++)
        for (int X = 0; X < LR; X++) {
            float t = (float)Y / LR + ((((X + Y) & 1) ? 0.012f : -0.012f));
            l.p[Y * LR + X] = mezclar(ARRIBA[v], ABAJO[v], t < 0 ? 0 : (t > 1 ? 1 : t));
        }
    switch (v) {
    case VIAJE_AVION: /* atardecer con aurora y colinas con hongos que brillan */
        estrellas_cielo(&l, 50, 70, 601);
        for (int b = 0; b < 2; b++) /* la aurora: dos bandas que ondulan */
            for (int X = 0; X < LR; X++) {
                float x = X / k, y0 = 26 + b * 14 + 6 * sinf(x * 0.06f + b * 2.0f);
                for (int Y = (int)(y0 * k); Y < (int)((y0 + 9) * k); Y++) {
                    float t = (Y / k - y0) / 9.0f;
                    uint32_t c = b ? A(0x7AF0C8) : A(0x9C7CFF);
                    if (Y >= 0 && Y < LR) l.p[Y * LR + X] = mezclar(l.p[Y * LR + X], c, 0.45f * (1 - fabsf(t * 2 - 1)));
                }
            }
        halo_luz(&l, 124, 38, 9, 36, A(0xFFF2C0), 0.34f); /* la luna grande con su resplandor */
        circulo(&l, 124, 38, 9, A(0xFFF4CC)); circulo(&l, 121, 36, 2.2f, A(0xF0E0AA)); circulo(&l, 127, 41, 1.6f, A(0xF0E0AA)); circulo(&l, 126, 34, 1.2f, A(0xF0E0AA));
        for (int x = 0; x < D; x++) { /* la sierra lejana, pálida de bruma */
            float h = 104 + 6 * sinf(x * 0.035f + 1.0f) + 3 * sinf(x * 0.11f);
            rect(&l, x, (int)h, 1, D - (int)h, mezclar(A(0x6A5CA2), A(0xE096C0), 0.35f));
        }
        for (int i = 0; i < 2; i++) {
            uint32_t c = i ? A(0x2E2A5A) : A(0x45407A);
            for (int x = 0; x < D; x++) {
                float h = 120 + i * 12 + 5 * sinf(x * 0.05f + i * 2.1f) + 3 * sinf(x * 0.13f + i);
                rect(&l, x, (int)h, 1, D - (int)h, c);
                if (i == 0 && (x & 1) == 0) lz_fino(&l, x, h, A(0x7C74B4)); /* el filo con luz de luna */
            }
        }
        for (int x = 3; x < D; x += 6 + (int)(hash(x, 11, 605) % 4)) { /* pinos oscuros en la loma */
            float h = 120 + 5 * sinf(x * 0.05f) + 3 * sinf(x * 0.13f);
            float a = 8 + (float)(hash(x, 12, 605) % 6);
            triangulo(&l, x - 2.6f, h + 1, x + 2.6f, h + 1, (float)x, h - a, A(0x1E1A44));
            triangulo(&l, x - 2.0f, h - a * 0.45f, x + 2.0f, h - a * 0.45f, (float)x, h - a - 1.5f, A(0x1E1A44));
        }
        for (int i = 0; i < 18; i++) { /* hongos que brillan */
            int x = (int)(hash(i, 5, 603) % D), y = 136 + (int)(hash(i, 6, 603) % 16);
            uint32_t c = (i & 1) ? A(0x7AF0FF) : A(0xFF8FE0);
            lz_fino(&l, x, y, c); lz_fino(&l, x + 0.7f, y, c); lz_fino(&l, x, y + 0.7f, A(0xD8D0F0));
        }
        break;
    case VIAJE_BARCO: { /* lago de estrellas con la luna */
        const int H0 = ART_VIAJE_AGUA_Y / 3 - 12;
        estrellas_cielo(&l, 70, H0, 611);
        aurora_banda(&l, 20, 12, A(0x7AF0C8), 0.30f, 0.6f); aurora_banda(&l, 34, 10, A(0x9C7CFF), 0.22f, 2.4f);
        halo_luz(&l, 112, 30, 11, 34, A(0xFFF6D0), 0.30f);
        circulo(&l, 112, 30, 11, A(0xFFF6D0)); circulo(&l, 116, 27, 9, mezclar(ARRIBA[v], ABAJO[v], 27.0f / 156.0f)); /* luna creciente */
        for (int x = 0; x < D; x++) { /* colinas lejanas al fondo y un faro con su luz */
            float h = (float)H0 - 7 + 4 * sinf(x * 0.05f + 0.6f) + 2 * sinf(x * 0.15f);
            rect(&l, x, (int)h, 1, H0 - (int)h + 1, A(0x1C2656));
        }
        rect(&l, 136, H0 - 17, 5, 17, A(0xE8E2F4)); rect(&l, 136, H0 - 12, 5, 3, A(0xC8506E)); rect(&l, 135, H0 - 19, 7, 2, A(0x2A2250)); rect(&l, 137, H0 - 21, 3, 2, A(0xFFF6A0));
        halo_luz(&l, 138.5f, H0 - 20, 1.5f, 22, A(0xFFF0A0), 0.5f);
        elipse(&l, 26, H0, 22, 4, A(0x1A2050));                 /* orilla con árboles en sombra */
        for (int i = 0; i < 4; i++) triangulo(&l, 10 + i * 9.0f, H0, 18 + i * 9.0f, H0, 14 + i * 9.0f, H0 - 12 - (i % 2) * 4, A(0x1A2050));
        for (int Y = (int)(H0 * k); Y < LR; Y++)
            for (int X = 0; X < LR; X++) {
                float t = (float)(Y - H0 * k) / (LR - H0 * k);
                uint32_t c = mezclar(A(0x23306A), A(0x101838), t);
                uint32_t h = hash(X / 5, Y / 2, 777);
                if ((h & 31) == 0) c = mezclar(c, A(0xBFD8FF), 0.5f); /* estrellas en el agua */
                if (abs(X - (int)(112 * k)) < 6 + (int)(t * 6) && ((Y / 3) & 1) && (hash(X / 3, Y / 3, 778) & 1)) c = mezclar(c, A(0xFFF6D0), 0.25f); /* la luna reflejada */
                l.p[Y * LR + X] = c;
            }
        for (int i = 0; i < 16; i++) chispa(&l, (float)(hash(i, 7, 613) % 60), (float)(H0 - 20 + (int)(hash(i, 8, 613) % 24)), A(0xFFE27A)); /* luciérnagas */
        break;
    }
    case VIAJE_TREN: { /* puente de arcoíris sobre un mar de nubes, de noche */
        const int RY = ART_VIAJE_RIEL_Y / 3;
        estrellas_cielo(&l, 80, 100, 621);
        aurora_banda(&l, 34, 11, A(0xB48CFF), 0.20f, 1.1f);
        halo_luz(&l, 32, 30, 8, 30, A(0xFFF6D0), 0.30f);
        circulo(&l, 32, 30, 8, A(0xFFF6D0)); circulo(&l, 29, 28, 1.7f, A(0xEBDDB0)); circulo(&l, 35, 33, 1.2f, A(0xEBDDB0));
        for (int i = 0; i < 6; i++) { /* picos lejanos que asoman del mar de nubes */
            const float cx = 8.0f + i * 28.0f + (float)(hash(i, 14, 622) % 8), al = 14.0f + (float)(hash(i, 15, 622) % 12);
            triangulo(&l, cx - al * 0.9f, 132, cx + al * 0.9f, 132, cx, 132 - al, A(0x4A4284));
            triangulo(&l, cx - al * 0.22f, 132 - al * 0.76f, cx + al * 0.22f, 132 - al * 0.76f, cx, 132 - al, A(0xDCD6F4)); /* la nieve */
        }
        for (int i = 0; i < 9; i++) { /* mar de nubes */
            float x = (float)(i * 20 - 6), y = 128 + (float)(hash(i, 9, 623) % 10);
            elipse(&l, x, y, 16, 9, A(0xC8C0F0)); elipse(&l, x + 4, y - 3, 11, 6, A(0xE8E2FF));
        }
        rect(&l, 0, 132, D, D - 132, A(0xC8C0F0));
        static const uint32_t ARCO[6] = {A(0xFF6B6B), A(0xFFB04A), A(0xFFE27A), A(0x7FD38A), A(0x6FB7FF), A(0xB48CFF)};
        for (int i = 0; i < 6; i++) rect(&l, 0, RY - 3 + i, D, 1, ARCO[i]);  /* el puente */
        for (int x = 2; x < D; x += 9) chispa(&l, (float)x, (float)(RY + 4), A(0xFFFFFF));
        for (int x = 14; x < D; x += 28) { /* faroles junto al puente */
            rect(&l, x, RY - 12, 1, 9, A(0x2A2250)); circulo(&l, (float)x + 0.5f, (float)(RY - 13), 1.6f, A(0xFFF0A0));
            halo_luz(&l, (float)x + 0.5f, (float)(RY - 13), 1.0f, 9, A(0xFFE27A), 0.45f);
        }
        break;
    }
    default: /* amanecer en un prado mágico con luces que flotan */
        rayos_sol(&l, 120, 108, 9, 130, A(0xFFF6D0), 0.34f);
        halo_luz(&l, 120, 108, 14, 52, A(0xFFE9B0), 0.42f);
        circulo(&l, 120, 110, 16, A(0xFFF0B0)); circulo(&l, 120, 110, 13, A(0xFFD98A));
        nube_viaje(&l, 36, 40, 5); nube_viaje(&l, 118, 60, 4); nube_viaje(&l, 78, 26, 3);
        for (int i = 0; i < 2; i++) { /* lomas lejanas, cada vez más verdes */
            for (int x = 0; x < D; x++) {
                float h = 106 + i * 6 + 4 * sinf(x * 0.045f + i * 1.7f) + 2 * sinf(x * 0.13f + i);
                rect(&l, x, (int)h, 1, 120 - (int)h, i ? A(0x9FD08C) : A(0xC0D8A8));
            }
        }
        for (int i = 0; i < 14; i++) { /* arbolitos a lo lejos */
            const float x = (float)(hash(i, 16, 634) % D), h = 112 + 4 * sinf(x * 0.045f + 1.7f);
            circulo(&l, x, h - 2, 2.4f, A(0x6FAE62)); rect(&l, (int)x, (int)h - 1, 1, 2, A(0x6A5238));
        }
        rect(&l, 0, 118, D, D - 118, A(0x86C778));
        for (int i = 0; i < 160; i++) {
            int x = (int)(hash(i, 3, 633) % D), y = 118 + (int)(hash(i, 4, 633) % (D - 118));
            lz_set(&l, x, y, (hash(i, 5, 633) & 3) ? A(0x5FB05A) : ((i & 1) ? A(0xFF8FC8) : A(0xFFFFFF)));
        }
        for (int i = 0; i < 6; i++) { /* un círculo de hongos */
            float a = i * 1.047f, x = 40 + cosf(a) * 16, y = 136 + sinf(a) * 5;
            elipse(&l, x, y, 3, 1.8f, (i & 1) ? A(0xF0506E) : A(0xFF9FC8)); lz_set(&l, (int)x, (int)y + 2, A(0xF6EEDC));
        }
        for (int i = 0; i < 22; i++) chispa(&l, (float)(hash(i, 6, 635) % D), (float)(20 + hash(i, 7, 635) % 96), (i & 1) ? A(0xFFE27A) : A(0x9CF0FF));
        break;
    }
    ampliar(dst, &l, 2, false);
    free(l.p);
}

/* ======================================================================
 * Personajes nuevos (en prueba): los ocho bebés y sus huevos con pista.
 * 0 Espíritu de las Hojas · 1 Espíritu del Rayo · 2 Ciervo Solar · 3 Guardián del Río ·
 * 4 Guardián de las Raíces · 5 Ave Astral · 6 Insecto Lumínico · 7 Oculto de las Sombras
 * ==================================================================== */
/* una masa redonda con luz arriba a la izquierda y sombra tramada abajo (el estilo nuevo) */
static void masa(lienzo_t *l, float cx, float cy, float rx, float ry, uint32_t base, uint32_t luz, uint32_t osc)
{
    const float k = KL(l);
    for (int Y = (int)floorf((cy - ry - 1) * k); Y <= (int)floorf((cy + ry + 1) * k); Y++)
        for (int X = (int)floorf((cx - rx - 1) * k); X <= (int)floorf((cx + rx + 1) * k); X++) {
            float dx = ((X + 0.5f) / k - cx) / rx, dy = ((Y + 0.5f) / k - cy) / ry, d = dx * dx + dy * dy;
            if (d > 1.0f) continue;
            float lz = -dx * 0.55f - dy * 0.8f;
            bool t = ((X + Y) & 1) != 0;
            uint32_t c = base;
            if (lz > 0.55f && d > 0.15f) c = luz;
            else if (lz > 0.38f) c = t ? luz : base;
            else if (lz < -0.55f || d > 0.86f) c = (lz < -0.2f || t) ? osc : base;
            else if (lz < -0.35f) c = t ? osc : base;
            lz_real(l, X, Y, c);
        }
}
typedef struct { uint32_t base, luz, osc, acento, acento2, panza; bool flota, oscura; } bebe_pal_t;
static const bebe_pal_t BEBE_PAL[8] = {
    {A(0x6FBF5A), A(0x9CDD7A), A(0x3F8A44), A(0x3E9A4A), A(0xB8E890), A(0xD6F2B4), false, false}, /* hojas */
    {A(0xFFD94A), A(0xFFF2A0), A(0xE0A42A), A(0x5ED0FF), A(0xFFFFFF), A(0xFFF6C8), true, false},  /* rayo */
    {A(0xD8A26A), A(0xEEC490), A(0xA8743E), A(0xFFE27A), A(0x6A4428), A(0xF6E6CC), false, false}, /* ciervo */
    {A(0x5FA8E8), A(0x9AD4F2), A(0x2E70B4), A(0xFF6BB8), A(0x3E86C8), A(0xC8E8FF), true, false},  /* río */
    {A(0xB07A48), A(0xD8A878), A(0x7A4E2A), A(0x6A4428), A(0x8A5A30), A(0xF2D8B4), false, false}, /* bellotas */
    {A(0x5A44B4), A(0x8E7AE0), A(0x34267E), A(0xFFE27A), A(0xFFF4B0), A(0xD8CCF4), false, false}, /* ave de la noche */
    {A(0x5DC8A0), A(0x9AE8C8), A(0x2E8A70), A(0xFFE27A), A(0x2A2238), A(0xC8F4E4), false, false}, /* insecto */
    {A(0x2A2238), A(0x463C5A), A(0x15111E), A(0x7AF0FF), A(0xB48CFF), A(0x2A2238), true, true},   /* sombras */
};
void art_bebe_nuevo(art_img_t *dst, int especie, int escala, art_expresion_t e, int frame)
{
    especie &= 7;
    const float kd = k_de_escala(escala);
    const int LR = (int)lroundf(ART_CRIATURA_LADO * kd), AMP = (int)lroundf((float)escala / kd);
    uint32_t *buf = buf_trabajo(LR * LR);
    if (!buf) return;
    lienzo_t l = {LR, LR, buf, kd};
    const bebe_pal_t *P = &BEBE_PAL[especie];
    const float oy = frame ? 1.0f : 0.0f;
    float cx = 10.0f, cy = 13.6f + oy - (P->flota ? 1.2f : 0.0f), rx = 4.8f, ry = 4.4f;
    int ex0 = 7, ex1 = 12, ey = (int)(cy - 0.8f), mx = 9, my = ey + 3;
    /* detrás del cuerpo */
    switch (especie) {
    case 0: /* un brote con dos hojitas */
        trazo(&l, 10, cy - ry + 0.5f, 10.3f, cy - ry - 2.2f, 0.45f, P->acento);
        masa(&l, 8.2f, cy - ry - 2.6f, 1.9f, 1.0f, P->acento, P->acento2, P->osc);
        masa(&l, 12.3f, cy - ry - 3.0f, 1.9f, 1.0f, P->acento, P->acento2, P->osc);
        break;
    case 1: /* chispas que lo rodean */
        zigzag(&l, 10.0f, cy - ry + 0.6f, 3.0f, 1.1f, P->acento);
        trazo(&l, cx - rx - 0.4f, cy, cx - rx - 2.0f, cy - 1.2f, 0.35f, P->acento);
        trazo(&l, cx + rx + 0.4f, cy + 0.5f, cx + rx + 2.0f, cy - 0.6f, 0.35f, P->acento);
        break;
    case 2: /* orejitas de cervatillo */
        /* orejas largas hacia los lados, como hojas, y dos cuernitos que recién asoman */
        masa(&l, 4.6f, cy - ry + 1.4f, 2.4f, 1.0f, P->base, P->luz, P->osc);
        masa(&l, 15.4f, cy - ry + 1.4f, 2.4f, 1.0f, P->base, P->luz, P->osc);
        lz_set(&l, 4, (int)(cy - ry + 1.4f), A(0xF4B8A8)); lz_set(&l, 15, (int)(cy - ry + 1.4f), A(0xF4B8A8));
        rect(&l, 8, (int)(cy - ry - 0.6f), 1, 2, P->acento2); rect(&l, 11, (int)(cy - ry - 0.6f), 1, 2, P->acento2);
        break;
    case 3: /* cola de renacuajo y branquias de neón */
        trazo(&l, cx + rx - 1.0f, cy + 2.0f, cx + rx + 2.6f, cy + 3.2f - oy * 1.5f, 0.9f, P->osc);
        masa(&l, cx + rx + 2.8f, cy + 3.0f - oy * 1.5f, 1.2f, 0.8f, P->base, P->luz, P->osc);
        for (int s = -1; s <= 1; s += 2)
            for (int k = 0; k < 3; k++) trazo(&l, cx + s * (rx - 0.6f), cy - 1.6f + k * 1.2f, cx + s * (rx + 1.6f), cy - 2.6f + k * 1.5f, 0.35f, P->acento);
        break;
    case 4: /* la caperuza de bellota */
        break;
    case 5: /* penachos de búho */
        triangulo(&l, 5.4f, cy - ry + 1.4f, 7.6f, cy - ry + 0.4f, 5.2f, cy - ry - 1.8f, P->osc);
        triangulo(&l, 14.6f, cy - ry + 1.4f, 12.4f, cy - ry + 0.4f, 14.8f, cy - ry - 1.8f, P->osc);
        break;
    case 6: /* oruga: dos segmentos detrás */
        masa(&l, 14.6f, cy + 1.4f, 2.6f, 2.4f, P->base, P->luz, P->osc);
        masa(&l, 17.4f, cy + 2.0f - oy * 0.8f, 1.9f, 1.8f, P->base, P->luz, P->osc);
        lz_set(&l, 15, (int)(cy + 0.4f), P->acento); lz_set(&l, 17, (int)(cy + 1.2f), P->acento);
        break;
    case 7: /* bordes de neblina */
        for (int k = 0; k < 9; k++) {
            float a = (float)k * 0.7f + oy * 0.3f;
            circulo(&l, cx + cosf(a) * (rx + 0.6f), cy + sinf(a) * (ry + 0.4f) + 0.6f, 1.1f, P->base);
        }
        break;
    default: break;
    }
    /* el cuerpo */
    if (especie == 6) { rx = 3.8f; ry = 3.8f; cx = 8.6f; ex0 = 6; ex1 = 10; mx = 8; }
    masa(&l, cx, cy, rx, ry, P->base, P->luz, P->osc);
    if (especie != 7 && especie != 1) masa(&l, cx, cy + ry * 0.45f, rx * 0.6f, ry * 0.4f, P->panza, aclarar(P->panza, 0.4f), P->panza); /* pancita */
    /* encima del cuerpo */
    switch (especie) {
    case 1: masa(&l, cx, cy, 2.0f, 1.8f, P->panza, A(0xFFFFFF), P->base); break; /* el corazón de luz */
    case 2: /* motitas de sol en el lomo */
        lz_set(&l, 6, (int)cy, A(0xFFF4C8)); lz_set(&l, 14, (int)cy - 1, A(0xFFF4C8)); lz_set(&l, 13, (int)cy + 2, A(0xFFF4C8)); lz_set(&l, 7, (int)cy - 2, A(0xFFF4C8)); lz_set(&l, 12, (int)cy - 3, A(0xFFF4C8));
        break;
    case 4: /* la caperuza */
        elipse(&l, cx, cy - ry + 1.2f, rx * 0.95f, 1.9f, P->osc);
        elipse(&l, cx, cy - ry + 0.8f, rx * 0.85f, 1.5f, P->acento);
        for (int k = 0; k < 4; k++) lz_set(&l, 7 + k * 2, (int)(cy - ry + 0.8f), P->acento2);
        rect(&l, 10, (int)(cy - ry - 1.4f), 1, 2, P->acento);
        ey += 1; my += 1;
        break;
    case 5: /* el ojo místico en la frente y alitas */
        circulo(&l, cx, cy - ry + 1.6f, 0.9f, P->acento); lz_set(&l, 10, (int)(cy - ry + 1.4f), A(0xFFFFFF));
        masa(&l, cx - rx + 0.2f, cy + 1.2f, 1.2f, 2.0f, P->osc, P->base, P->osc);
        masa(&l, cx + rx - 0.2f, cy + 1.2f, 1.2f, 2.0f, P->osc, P->base, P->osc);
        break;
    case 6: /* antenas con luz */
        trazo(&l, 7.4f, cy - ry + 0.4f, 6.4f, cy - ry - 2.0f, 0.3f, P->acento2); trazo(&l, 9.8f, cy - ry + 0.4f, 10.8f, cy - ry - 2.0f, 0.3f, P->acento2);
        lz_set(&l, 6, (int)(cy - ry - 2.2f), P->acento); lz_set(&l, 10, (int)(cy - ry - 2.2f), P->acento);
        break;
    case 7: /* ojos grandes y claros: la cara se pinta encima */
        circulo(&l, (float)ex0 + 0.5f, (float)ey + 0.5f, 1.6f, A(0xEAF4FF)); circulo(&l, (float)ex1 + 0.5f, (float)ey + 0.5f, 1.6f, A(0xEAF4FF));
        break;
    default: break;
    }
    /* patitas */
    if (!P->flota && especie != 4) {
        int pa = frame ? 1 : 0;
        rect(&l, (int)cx - 3, 18 + pa, 2, 1, P->osc);
        rect(&l, (int)cx + 1, 18 + (1 - pa), 2, 1, P->osc);
    }
    pintar_cara(&l, e, ex0, ex1, ey, mx, my, true, 0, especie != 7, especie == 7 ? A(0xB48CFF) : C_MEJILLA, false, 1);
    contornear(&l, especie == 7 ? A(0x0A0812) : oscurecer(P->osc, 0.45f));
    if (especie == 1 || especie == 7) { /* destellos alrededor */
        uint32_t c = especie == 1 ? P->acento : P->acento2;
        lz_set(&l, 2, (int)(8 + oy), c); lz_set(&l, 17, (int)(6 - oy), c); lz_set(&l, 18, (int)(12 + oy), c);
    }
    ampliar(dst, &l, AMP, false);
}

/* los huevos: todos grises, con una pista chiquita de lo que hay adentro */
void art_huevo_nuevo(art_img_t *dst, int especie, int escala) { huevo_pintado(dst, especie, escala, 0, 0); }
static void huevo_nuevo(art_img_t *dst, int especie, int escala, int grietas, int inclinacion)
{
    especie &= 7;
    const float kd = k_de_escala(escala);
    const int LR = (int)lroundf(ART_CRIATURA_LADO * kd), AMP = (int)lroundf((float)escala / kd);
    uint32_t *buf = buf_trabajo(LR * LR);
    if (!buf) return;
    lienzo_t l = {LR, LR, buf, kd};
    const float cx = 10.0f + inclinacion * 0.6f, cy = 11.6f;
    const uint32_t base = especie == 5 ? A(0x8C889E) : A(0xB4B0BE), luz = A(0xDAD6E2), osc = especie == 7 ? A(0x5A5468) : A(0x86829A);
    for (int Y = 0; Y < l.h; Y++)
        for (int X = 0; X < l.w; X++) {
            float x = (X + 0.5f) / kd, y = (Y + 0.5f) / kd, dy = (y - cy) / 7.2f, k = dy < 0 ? 0.78f : 1.0f, dx = (x - cx) / (5.6f * k);
            float d = dx * dx + dy * dy;
            if (d > 1.0f) continue;
            float lz = -dx * 0.6f - dy * 0.7f;
            bool t = ((X + Y) & 1) != 0;
            uint32_t c = base;
            if (lz > 0.5f) c = t ? luz : base;
            else if (lz < -0.5f || d > 0.85f) c = t ? osc : base;
            if ((hash(X / 2, Y / 2, 11 + (uint32_t)especie) & 15) == 0) c = mezclar(c, osc, 0.4f); /* textura sutil */
            lz_real(&l, X, Y, c);
        }
    switch (especie) { /* la pista: grande y en color, que se vea de lejos */
    case 0: /* le brota una hoja por arriba y tiene musgo */
        trazo(&l, cx, 4.8f, cx + 0.4f, 2.4f, 0.4f, A(0x3F8A44));
        masa(&l, cx + 2.2f, 2.4f, 2.2f, 1.1f, A(0x6FBF5A), A(0x9CDD7A), A(0x3F8A44));
        masa(&l, cx - 1.6f, 3.2f, 1.4f, 0.8f, A(0x6FBF5A), A(0x9CDD7A), A(0x3F8A44));
        for (int k = 0; k < 5; k++) circulo(&l, 6.4f + k * 1.8f, 16.4f + (k & 1) * 0.8f, 0.9f, A(0x7FCB62));
        break;
    case 1: /* un rayo que le cruza la cáscara, con chispas */
        zigzag(&l, cx - 0.6f, 16.4f, 9.6f, 2.0f, A(0xFFE27A));
        lz_set(&l, 5, 8, A(0x5ED0FF)); lz_set(&l, 15, 13, A(0x5ED0FF)); lz_set(&l, 6, 15, A(0xFFF4A0));
        break;
    case 2: /* manchitas de cervato, tibias y doradas */
        elipse(&l, cx, 13.4f, 4.6f, 4.4f, A(0xD8B07A));
        for (int k = 0; k < 6; k++) circulo(&l, 7.2f + (k % 3) * 2.8f, 10.0f + (k / 3) * 3.4f + (k & 1) * 0.6f, 0.75f, A(0xFFF4DC));
        break;
    case 3: /* olas de agua y burbujas */
        for (int k = 0; k < 2; k++) for (int s = 0; s < 4; s++)
            trazo(&l, 5.4f + s * 2.4f, 12.4f + k * 2.6f, 6.6f + s * 2.4f, 11.4f + k * 2.6f, 0.45f, A(0x5EB8FF));
        circulo(&l, 8.0f, 7.8f, 0.9f, A(0xBFE6FF)); circulo(&l, 12.0f, 6.6f, 0.6f, A(0xBFE6FF));
        break;
    case 4: /* gorrito de bellota */
        elipse(&l, cx, 6.2f, 4.6f, 2.0f, A(0x7A4E2A));
        for (int k = 0; k < 4; k++) lz_set(&l, 7 + k * 2, 6, A(0x5A3820));
        rect(&l, 10, 3, 1, 2, A(0x6A4428));
        break;
    case 5: /* un pedacito de cielo de noche: luna y estrellas */
        elipse(&l, cx, 11.6f, 5.0f, 6.6f, A(0x3A4A8A));
        circulo(&l, 8.6f, 9.4f, 1.8f, A(0xFFF2A0)); circulo(&l, 9.4f, 8.8f, 1.6f, A(0x3A4A8A));
        lz_set(&l, 12, 8, A(0xFFFFFF)); lz_set(&l, 13, 12, A(0xFFFFFF)); lz_set(&l, 8, 14, A(0xFFFFFF)); lz_set(&l, 11, 16, A(0xE8E0FF));
        break;
    case 6: /* brilla solito: una luz grande con rayitos */
        circulo(&l, cx, 12.0f, 2.6f, A(0xD8F27A));
        circulo(&l, cx, 12.0f, 1.4f, A(0xFFFFE0));
        for (int k = 0; k < 4; k++) { float a = k * 1.5708f + 0.785f; trazo(&l, cx + cosf(a) * 3.2f, 12.0f + sinf(a) * 3.2f, cx + cosf(a) * 4.2f, 12.0f + sinf(a) * 4.2f, 0.3f, A(0xE8FF9A)); }
        break;
    case 7: /* niebla oscura y dos ojitos que miran desde adentro */
        elipse(&l, cx, 12.6f, 5.2f, 5.0f, A(0x4A4258));
        for (int k = 0; k < 4; k++) circulo(&l, 5.6f + k * 2.9f, 15.6f - (k & 1) * 1.0f, 1.3f, A(0x6E6684));
        rect(&l, 8, 11, 1, 2, A(0xFFE27A)); rect(&l, 11, 11, 1, 2, A(0xFFE27A));
        break;
    default: break;
    }
    uint32_t g = A(0x3A3448); /* grietas cuando está por nacer */
    if (grietas >= 1) { lz_set(&l, 8, 7, g); lz_set(&l, 9, 8, g); lz_set(&l, 8, 9, g); }
    if (grietas >= 2) { lz_set(&l, 10, 8, g); lz_set(&l, 11, 7, g); lz_set(&l, 12, 8, g); lz_set(&l, 13, 9, g); }
    if (grietas >= 3) { lz_set(&l, 7, 10, g); lz_set(&l, 6, 11, g); lz_set(&l, 14, 10, g); lz_set(&l, 13, 11, g); }
    contornear(&l, A(0x4A4458));
    ampliar(dst, &l, AMP, false);
}

/* ---- las tres etapas de los personajes nuevos, con pose (cara, postura, cola, ropa, color elegido) ---- */
/* acabado de las criaturas: luz arriba a la izquierda (borde claro), sombra abajo a la derecha (tono violeta, como las sombras de los fondos),
 * con tramado de puntos (el mismo estilo de los fondos) para que se vean con volumen y no planas */
#if 1 // LINDO
#define LINDO_GRANDE 1
#else
#define LINDO_GRANDE 0
#endif
static void pulir_criatura(lienzo_t *l)
{
    const int W = l->w, H = l->h;
    uint32_t *src = malloc((size_t)W * H * 4);
    if (!src) return;
    memcpy(src, l->p, (size_t)W * H * 4);
    const float k = KL(l);
    int R = (int)(1.7f * k + 0.5f);
    if (R < 2) R = 2;
    static const uint8_t B4[16] = {0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5};
#define OP(X, Y) ((X) >= 0 && (Y) >= 0 && (X) < W && (Y) < H && (src[(Y) * W + (X)] >> 24))
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            uint32_t c = src[y * W + x];
            if (!(c >> 24)) continue;
            int ds = R + 1, dl = R + 1;
            for (int j = 1; j <= R; j++) {
                if (ds > R && (!OP(x, y + j) || !OP(x + j, y + j) || !OP(x + j, y))) ds = j;
                if (dl > R && (!OP(x, y - j) || !OP(x - j, y - j) || !OP(x - j, y))) dl = j;
                if (ds <= R && dl <= R) break;
            }
            const float thr = ((float)B4[(y & 3) * 4 + (x & 3)] + 0.5f) / 16.0f;
            const float ts = ds <= R ? (float)(R + 1 - ds) / (float)R : 0.0f;
            const float tl = dl <= R ? (float)(R + 1 - dl) / (float)R : 0.0f;
            const float ls = ts * 1.25f - thr * 0.5f, ll = tl * 1.1f - thr * 0.55f;
            if (ls > 0.62f) c = mezclar(c, A(0x2B2060), 0.34f);
            else if (ls > 0.30f) c = mezclar(c, A(0x2B2060), 0.18f);
            else if (ll > 0.55f) c = mezclar(c, A(0xFFF4C8), 0.30f);
            else if (ll > 0.28f) c = mezclar(c, A(0xFFF4C8), 0.15f);
            else if ((hash(x, y, 77) % 31u) == 0u && ds > 2 && dl > 2) c = mezclar(c, A(0xFFF4C8), 0.16f); /* motitas de luz: el pelaje / las escamas */
            else if ((hash(x, y, 91) % 41u) == 0u && ds > 2) c = mezclar(c, A(0x2B2060), 0.12f);
            l->p[y * W + x] = c;
        }
#undef OP
    free(src);
}

/* contorno "selectivo": cada borde toma el color del cuerpo que toca, bien oscurecido (y más claro hacia la luz), no un negro parejo */
static void contornear_fino(lienzo_t *l, uint32_t base)
{
    const int W = l->w, H = l->h;
    uint32_t *src = malloc((size_t)W * H * 4);
    if (!src) return;
    memcpy(src, l->p, (size_t)W * H * 4);
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            if (src[y * W + x] >> 24) continue;
            uint32_t v[4]; int n = 0;
            if (x > 0 && (src[y * W + x - 1] >> 24)) v[n++] = src[y * W + x - 1];
            if (x < W - 1 && (src[y * W + x + 1] >> 24)) v[n++] = src[y * W + x + 1];
            if (y > 0 && (src[(y - 1) * W + x] >> 24)) v[n++] = src[(y - 1) * W + x];
            if (y < H - 1 && (src[(y + 1) * W + x] >> 24)) v[n++] = src[(y + 1) * W + x];
            if (!n) continue;
            int r = 0, g = 0, b = 0;
            for (int i = 0; i < n; i++) { r += (v[i] >> 16) & 255; g += (v[i] >> 8) & 255; b += v[i] & 255; }
            uint32_t m = A(((uint32_t)(r / n) << 16) | ((uint32_t)(g / n) << 8) | (uint32_t)(b / n));
            const bool luz = (x < W - 1 && (src[y * W + x + 1] >> 24)) || (y < H - 1 && (src[(y + 1) * W + x] >> 24)); /* toca el cuerpo por la derecha o abajo: borde de la cara iluminada */
            l->p[y * W + x] = mezclar(m, base, luz ? 0.66f : 0.82f);
        }
    free(src);
}

/* (fw 108) el color del cuerpo de una criatura nueva, tal cual queda pintado (para el botón de la paleta) */
static uint32_t color_cuerpo_nuevo(int especie, int tono)
{
    const uint32_t base = BEBE_PAL[especie & 7].base, luz = BEBE_PAL[especie & 7].luz;
    const float ref = s_tinte_ref;
    s_tinte_ref = hue_ref(base);
    const uint32_t b2 = tono ? tenir(base, tono, 0, false) : A(base), l2 = tono ? tenir(luz, tono, 0, false) : A(luz);
    s_tinte_ref = ref;
    /* el cuerpo se ve un poco más claro que su color base (tiene luz arriba): el botón lo imita */
    return mezclar(b2, l2, 0.34f) & 0xFFFFFFu;
}

static void criatura_nueva(lienzo_t *l, const art_pose_t *ps, int col, int especie, int tono)
{
    /* col: de qué especie son sus colores; especie: la forma de su cuerpo (al crecer puede tomar la de otra) */
    const int etapa = ps->etapa < 1 ? 1 : (ps->etapa > 3 ? 3 : ps->etapa), frame = ps->frame;
    bebe_pal_t P = BEBE_PAL[col & 7];
    const bool propia = (col & 7) == (especie & 7);
    if (!propia) { P.acento = BEBE_PAL[especie & 7].acento; P.acento2 = BEBE_PAL[especie & 7].acento2; P.flota = BEBE_PAL[especie & 7].flota; }
    /* algunos cambian de color al crecer */
    if (propia && especie == 2 && etapa == 3) { P.base = A(0xF4D47A); P.luz = A(0xFFEEB0); P.osc = A(0xC8923A); P.panza = A(0xFFFAE6); } /* el Gran Ciervo Dorado */
    if (propia && especie == 5 && etapa == 2) { P.base = A(0x4A3AA4); P.luz = A(0x8470DA); P.osc = A(0x2A1E70); }                           /* ave índigo */
    if (propia && especie == 5 && etapa == 3) { P.base = A(0x3A2C96); P.luz = A(0x7A66D4); P.osc = A(0x1E1660); P.acento = A(0xFFE27A); }  /* ave del firmamento: índigo con estrellas */
    if (propia && especie == 7 && etapa == 3) { P.base = A(0x4E3E7C); P.luz = A(0x8878BE); P.osc = A(0x2C2250); P.panza = A(0xB4A8E0); } /* gato de niebla nocturna */
    if (propia && especie == 7 && etapa == 1) { P.base = A(0x4A3E6E); P.luz = A(0x7A6AA4); P.osc = A(0x2E2548); } /* bruma */
    if (propia && especie == 7 && etapa == 2) { P.base = A(0x6A4A8A); P.luz = A(0x8A6AAE); P.osc = A(0x45305E); }                           /* mapache morado */
    if (especie == 3 && etapa == 3) { P.acento = A(0x7AF0E8); }                                                                     /* aletas cian */
    uint32_t *cs[6] = {&P.base, &P.luz, &P.osc, &P.acento, &P.acento2, &P.panza};
    s_tinte_ref = hue_ref(P.base);
    for (int i = 0; i < 6; i++) *cs[i] = tenir(*cs[i], i < 3 || i == 5 ? tono : 0, 0, ps->enferma);
    bool flota = P.flota;
    if (especie == 1) flota = etapa != 2;  /* la chispa y el felino sobre su nube flotan; el erizo camina */
    if (especie == 3) flota = etapa == 1;  /* el renacuajo flota */
    if (especie == 5 && etapa == 3) flota = true;
    if (especie == 6 && etapa == 3) flota = true;
    const float oy = frame ? 1.0f : 0.0f, s = etapa == 1 ? 1.0f : (etapa == 2 ? 1.17f : 1.34f);
#if 1 // LINDO
    float cx = 10.0f, rx = 5.3f * s, ry = 4.6f * s, cy = (etapa == 1 ? 13.6f : (etapa == 2 ? 13.2f : 12.6f)) + oy - (flota ? 1.2f : 0.0f);
#else
    float cx = 10.0f, rx = 4.8f * s, ry = 4.4f * s, cy = (etapa == 1 ? 13.6f : (etapa == 2 ? 13.2f : 12.6f)) + oy - (flota ? 1.2f : 0.0f);
#endif
    if (especie == 6 && etapa < 3) { rx = 3.8f * s; ry = 3.8f * s; cx = 8.4f - (etapa - 1) * 0.6f; }
    if (ps->postura == 1) { float a = ry; ry *= 1.08f; rx *= 0.96f; cy -= ry - a; }
    if (ps->postura == -1) { float a = ry; ry *= 0.86f; rx *= 1.07f; cy += a - ry; }
    if (ps->postura == -2) { float a = ry; ry *= 0.72f; rx *= 1.16f; cy += a - ry + 1.0f; }
    const float alza = ps->orejas < 0 ? 0.6f : (ps->orejas > 0 ? 1.2f : 1.0f);
    const float colay = ps->cola < 0 ? 1.6f : (ps->cola > 0 ? (frame ? -1.0f : 0.6f) : 0.0f);
    const float top = cy - ry;
#if 1 // LINDO
    int ex0 = (int)(cx - rx * 0.5f), ex1 = (int)(cx + rx * 0.5f) - 1, ey = (int)(cy - ry * 0.08f);
#else
    int ex0 = (int)(cx - rx * 0.47f), ex1 = (int)(cx + rx * 0.47f) - 1, ey = (int)(cy - ry * 0.18f);
#endif
    int mx = (ex0 + ex1) / 2, my = ey + 3;
    /* sombra a sus pies (o más abajo si flota) */
    /* --- detrás del cuerpo --- */
    switch (especie) {
    case 0:
        if (etapa == 3) { /* capa de hojas otoñales doradas */
            for (int k = 0; k < 5; k++) masa(l, cx - rx - 0.6f + k * (rx * 2 + 1.2f) / 4.0f, cy + ry * 0.55f - (k == 0 || k == 4 ? 1.0f : 0.0f), 1.8f, 2.6f, A(0xE8A040), A(0xFFD45C), A(0xB86A28));
        }
        if (etapa >= 2) { /* cola de enredadera */
            trazo(l, cx + rx - 1, cy + ry * 0.5f, cx + rx + 2.4f, cy + ry * 0.2f + colay, 0.45f, P.acento);
            masa(l, cx + rx + 2.6f, cy + ry * 0.1f + colay, 1.2f, 0.7f, P.acento, P.acento2, P.osc);
        }
        { /* hojas en la cabeza: dos, tres o una corona */
            int n = etapa == 1 ? 2 : 3;
            trazo(l, cx, top + 0.6f, cx + 0.3f, top - 2.0f * alza, 0.45f, P.acento);
            if (etapa == 3) { /* una flor grande en la cabeza, entre dos hojas */
                const float fy = top - 3.4f * alza;
                masa(l, cx - 3.2f * s, top - 1.4f * alza, 1.8f * s, 0.9f, P.acento, P.acento2, P.osc); masa(l, cx + 3.2f * s, top - 1.4f * alza, 1.8f * s, 0.9f, P.acento, P.acento2, P.osc);
                for (int k = 0; k < 6; k++) { float a = (float)k * 1.0472f + 0.3f; circulo(l, cx + cosf(a) * 1.7f, fy + sinf(a) * 1.5f, 1.25f, k & 1 ? A(0xFF8ABC) : A(0xFFB8D8)); }
                circulo(l, cx, fy, 1.2f, A(0xFFE27A)); lz_set(l, (int)cx, (int)fy, A(0xFFFFFF));
            } else
            for (int k = 0; k < n; k++) {
                float a = (k - (n - 1) / 2.0f) * 1.9f;
                masa(l, cx + a * s, top - (etapa == 1 ? 2.4f : 2.2f) * alza - (k == 1 && n == 3 ? 0.8f : 0), 1.8f * s, 1.0f * s, etapa == 3 ? A(0xE8A040) : P.acento, etapa == 3 ? A(0xFFD45C) : P.acento2, P.osc);
            }
        }
        break;
    case 1:
        if (etapa == 1) {
            zigzag(l, 10.0f, top + 0.6f, 3.0f * alza, 1.1f, P.acento);
            trazo(l, cx - rx - 0.4f, cy, cx - rx - 2.0f, cy - 1.2f, 0.35f, P.acento);
            trazo(l, cx + rx + 0.4f, cy + 0.5f, cx + rx + 2.0f, cy - 0.6f, 0.35f, P.acento);
        } else if (etapa == 2) { /* espinas de energía en el lomo y cola de rayo */
            zigzag(l, cx + rx + 0.8f, cy + 0.2f + colay, 3.6f, 1.0f, P.acento);
            for (int k = 0; k < 5; k++) {
                float a = 3.14f + 0.35f + k * 0.6f;
                triangulo(l, cx + cosf(a) * rx * 0.8f, cy + sinf(a) * ry * 0.8f - 0.6f, cx + cosf(a + 0.3f) * rx * 0.8f, cy + sinf(a + 0.3f) * ry * 0.8f,
                          cx + cosf(a + 0.15f) * (rx + 2.4f * alza), cy + sinf(a + 0.15f) * (ry + 2.4f * alza), k & 1 ? P.acento : P.osc);
            }
        } else { /* felino de la tormenta: orejas y cola de rayo, sobre una nube */
            masa(l, cx, 18.4f, 6.4f, 1.8f, A(0xE8ECF8), A(0xFFFFFF), A(0xA8B4CC));
            triangulo(l, cx - rx * 0.75f, top + 2.0f, cx - rx * 0.2f, top + 0.6f, cx - rx * 0.62f, top - 2.6f * alza, P.base);
            triangulo(l, cx + rx * 0.75f, top + 2.0f, cx + rx * 0.2f, top + 0.6f, cx + rx * 0.62f, top - 2.6f * alza, P.base);
            zigzag(l, cx + rx + 1.0f, cy - 0.4f + colay, 4.4f, 1.2f, P.acento);
        }
        break;
    case 2:
        masa(l, cx - rx - 0.2f, top + 1.6f, 2.2f * s, 1.0f, P.base, P.luz, P.osc);
        masa(l, cx + rx + 0.2f, top + 1.6f, 2.2f * s, 1.0f, P.base, P.luz, P.osc);
        if (etapa == 1) { rect(l, 8, (int)(top - 0.6f), 1, 2, P.acento2); rect(l, 11, (int)(top - 0.6f), 1, 2, P.acento2); }
        else { /* astas: con flores (cría) o rúnicas con hojas (adulto) */
            uint32_t asta = etapa == 3 ? A(0xD8B878) : P.acento2;
            float h = (etapa == 2 ? 3.2f : 4.4f) * alza;
            for (int sgn = -1; sgn <= 1; sgn += 2) {
                trazo(l, cx + sgn * 1.4f, top + 0.6f, cx + sgn * 2.6f, top - h, 0.4f, asta);
                trazo(l, cx + sgn * 2.2f, top - h * 0.5f, cx + sgn * 4.0f, top - h * 0.75f, 0.35f, asta);
                if (etapa == 2) lz_set(l, (int)(cx + sgn * 2.6f), (int)(top - h), A(0xFF9ECF));
                else { masa(l, cx + sgn * 4.2f, top - h * 0.8f, 1.0f, 0.6f, A(0x6FBF5A), A(0x9CDD7A), A(0x3F8A44)); lz_set(l, (int)(cx + sgn * 2.4f), (int)(top - h * 0.3f), A(0x7AF0FF)); }
            }
            if (etapa == 3) { /* brillitos dorados alrededor de las astas */
                static const float BR[5][2] = {{-5.6f, -0.4f}, {5.8f, -1.6f}, {-2.4f, -1.9f}, {3.0f, -0.6f}, {0.2f, -2.6f}};
                for (int k = 0; k < 5; k++) {
                    const int bx = (int)(cx + BR[k][0]), by = (int)(top - h * 0.55f + BR[k][1] * 1.4f);
                    lz_set(l, bx, by, A(0xFFFFFF));
                    if (k < 3) { lz_set(l, bx - 1, by, A(0xFFE27A)); lz_set(l, bx + 1, by, A(0xFFE27A)); lz_set(l, bx, by - 1, A(0xFFE27A)); lz_set(l, bx, by + 1, A(0xFFE27A)); }
                }
            }
        }
        break;
    case 3:
        if (etapa == 1) {
            trazo(l, cx + rx - 1.0f, cy + 2.0f, cx + rx + 2.6f, cy + 3.2f - oy * 1.5f, 0.9f, P.osc);
            masa(l, cx + rx + 2.8f, cy + 3.0f - oy * 1.5f, 1.2f, 0.8f, P.base, P.luz, P.osc);
        } else { /* cola de ajolote o de dragón */
            trazo(l, cx + rx - 1.0f, cy + ry * 0.5f, cx + rx + 3.0f, cy + ry * 0.2f + colay, etapa == 3 ? 1.0f : 0.8f, P.base);
            if (etapa == 3) triangulo(l, cx + rx + 2.0f, cy + colay, cx + rx + 4.6f, cy - 1.4f + colay, cx + rx + 4.0f, cy + 1.6f + colay, P.acento);
        }
        for (int sgn = -1; sgn <= 1; sgn += 2) /* branquias de neón o aletas */
            for (int k = 0; k < 3; k++)
                trazo(l, cx + sgn * (rx - 0.6f), top + ry * 0.6f + k * 1.2f, cx + sgn * (rx + 1.6f * s * alza), top + ry * 0.35f + k * 1.5f, 0.35f + 0.1f * etapa, P.acento);
        if (etapa == 3) for (int k = 0; k < 3; k++) triangulo(l, cx - 2.0f + k * 2.0f, top + 0.8f, cx - 0.6f + k * 2.0f, top + 0.8f, cx - 1.4f + k * 2.0f, top - 1.6f * alza, P.acento);
        if (especie == 3 && etapa == 3) { /* dragón de río: cuernos dorados hacia atrás y bigotes largos */
            for (int sgn = -1; sgn <= 1; sgn += 2) {
                triangulo(l, cx + sgn * 1.2f, top + 1.2f, cx + sgn * 2.8f, top + 1.0f, cx + sgn * 4.2f, top - 2.6f * alza, A(0xFFC84A));
                trazo(l, cx + sgn * (rx - 0.4f), cy + ry * 0.25f, cx + sgn * (rx + 2.2f), cy + ry * 0.2f, 0.3f, P.acento);
                trazo(l, cx + sgn * (rx + 2.2f), cy + ry * 0.2f, cx + sgn * (rx + 3.0f), cy - ry * 0.25f, 0.3f, P.acento);
            }
        }
        break;
    case 4:
        if (etapa >= 2) { /* colita esponjosa (de zorrito en el adulto) */
            float tr = etapa == 3 ? 2.8f : 2.2f;
            masa(l, cx + rx + 0.6f, cy - 0.4f + colay, tr * 0.8f, tr, P.base, P.luz, P.osc);
            if (etapa == 3) masa(l, cx + rx + 1.2f, cy - tr + 0.4f + colay, 1.2f, 1.0f, A(0xFFF4E0), A(0xFFFFFF), A(0xE0C8A8));
        }
        if (etapa == 2) masa(l, cx - rx - 0.4f, cy + 0.6f, 1.8f, 2.4f, A(0xC89A5A), A(0xE8C08A), A(0x8A5A2A)); /* mochila de nuez */
        if (etapa == 2) { circulo(l, cx - rx - 0.4f, cy + 0.6f, 0.9f, A(0x5EE89A)); lz_set(l, (int)(cx - rx - 0.4f), (int)(cy + 0.5f), A(0xE4FFF0)); } /* la runa de la nuez */
        if (etapa == 3) { /* orejas de zorrito */
            triangulo(l, cx - rx * 0.8f, top + 2.0f, cx - rx * 0.2f, top + 0.8f, cx - rx * 0.6f, top - 2.4f * alza, P.base);
            triangulo(l, cx + rx * 0.8f, top + 2.0f, cx + rx * 0.2f, top + 0.8f, cx + rx * 0.6f, top - 2.4f * alza, P.base);
        }
        break;
    case 5:
        triangulo(l, cx - rx * 0.85f, top + 1.4f, cx - rx * 0.4f, top + 0.4f, cx - rx * 0.88f, top - 1.8f * alza, P.osc);
        triangulo(l, cx + rx * 0.85f, top + 1.4f, cx + rx * 0.4f, top + 0.4f, cx + rx * 0.88f, top - 1.8f * alza, P.osc);
        if (etapa >= 2) { /* cola larga; en el fénix, de fuego azul */
            for (int k = 0; k < 3; k++) trazo(l, cx + rx * 0.6f, cy + ry * 0.5f, cx + rx + 2.6f + k * 0.6f, cy + ry * 0.9f - k * 1.4f + colay, 0.5f, etapa == 3 ? P.acento : P.osc);
        }
        if (etapa == 3) for (int k = 0; k < 3; k++) trazo(l, cx - 1.0f + k, top + 1.0f, cx - 1.6f + k * 1.6f, top - 3.0f * alza - (k == 1), 0.4f, P.acento); /* cresta de llama */
        break;
    case 6:
        if (etapa < 3) {
            masa(l, cx + rx + 1.0f, cy + 1.2f, 1.8f * s, 1.7f * s, P.base, P.luz, P.osc);
            circulo(l, cx + rx + 2.9f * s, cy + 1.7f - oy * 0.8f, 1.8f * s, A(0xFFF6C0)); /* el farolito de luz de la colita, que brilla */
            masa(l, cx + rx + 2.9f * s, cy + 1.7f - oy * 0.8f, 1.1f * s, 1.0f * s, A(0xFFE27A), A(0xFFFBD0), A(0xE0B040));
            lz_set(l, (int)(cx + rx + 1.0f), (int)(cy + 0.4f), P.acento);
            if (etapa == 2) { /* brotes de alas que brillan */
                masa(l, cx + rx + 1.6f, cy - 1.6f, 1.8f, 1.1f, A(0xE8FFF4), A(0xFFFFFF), A(0x9AE8C8));
                masa(l, cx + rx + 3.4f, cy - 1.2f, 1.4f, 0.9f, A(0xE8FFF4), A(0xFFFFFF), A(0x9AE8C8));
            }
        } else { /* polilla lunar: alas grandes detrás */
            float b = frame ? 0.85f : 1.0f;
            masa(l, cx - 5.2f * b, cy - 2.6f, 4.0f * b, 4.4f, A(0xD8F4E8), A(0xFFFFFF), A(0x7AC8A8));
            masa(l, cx + 5.2f * b, cy - 2.6f, 4.0f * b, 4.4f, A(0xD8F4E8), A(0xFFFFFF), A(0x7AC8A8));
            masa(l, cx - 3.8f * b, cy + 2.8f, 2.6f * b, 2.6f, A(0xC8EADC), A(0xFFFFFF), A(0x7AC8A8));
            masa(l, cx + 3.8f * b, cy + 2.8f, 2.6f * b, 2.6f, A(0xC8EADC), A(0xFFFFFF), A(0x7AC8A8));
            circulo(l, cx - 5.4f * b, cy - 2.8f, 1.2f, P.acento); circulo(l, cx + 5.4f * b, cy - 2.8f, 1.2f, P.acento);
        }
        break;
    case 7:
        if (etapa == 1) {
            /* bolita de niebla: bordes suaves (muchas nubecitas que se juntan), orejitas de humo y una colita que se deshace */
            for (int k = 0; k < 14; k++) {
                float a = (float)k * 0.449f + oy * 0.2f, rr = 2.0f + 0.5f * sinf((float)k * 2.1f);
                circulo(l, cx + cosf(a) * (rx - 0.6f), cy + sinf(a) * (ry - 0.6f) + 0.6f, rr, P.base);
            }
            trazo(l, cx - rx * 0.6f, top + 1.0f, cx - rx * 0.9f, top - 2.0f * alza, 0.8f, P.base); trazo(l, cx + rx * 0.6f, top + 1.0f, cx + rx * 0.9f, top - 2.0f * alza, 0.8f, P.base);
            circulo(l, cx + rx + 1.6f, cy + ry * 0.6f, 1.3f, P.luz); circulo(l, cx + rx + 3.2f, cy + ry * 0.2f - oy, 0.8f, P.luz);
        } else { /* orejas de mapache o de pantera, y cola */
            float ore = etapa == 2 ? 1.6f : 3.0f;
            triangulo(l, cx - rx * 0.8f, top + 1.8f, cx - rx * 0.2f, top + 0.8f, cx - rx * 0.62f, top - ore * alza, P.base);
            triangulo(l, cx + rx * 0.8f, top + 1.8f, cx + rx * 0.2f, top + 0.8f, cx + rx * 0.62f, top - ore * alza, P.base);
            if (etapa == 2) trazo(l, cx + rx - 0.6f, cy + ry * 0.4f, cx + rx + 3.0f, cy - 1.0f + colay, 1.1f, P.base);
            else { /* cola de humo en voluta que se deshace en un brillo */
                trazo(l, cx + rx - 0.6f, cy + ry * 0.4f, cx + rx + 2.4f, cy + ry * 0.1f + colay, 1.2f, P.base);
                trazo(l, cx + rx + 2.4f, cy + ry * 0.1f + colay, cx + rx + 3.4f, cy - 1.8f + colay, 0.9f, P.base);
                trazo(l, cx + rx + 3.4f, cy - 1.8f + colay, cx + rx + 2.0f, cy - 3.2f + colay, 0.6f, P.luz);
                circulo(l, cx + rx + 1.6f, cy - 3.8f + colay, 0.7f, P.acento2); lz_set(l, (int)(cx + rx + 4.4f), (int)(cy - 3.0f + colay), P.acento);
                triangulo(l, cx - rx * 0.62f, top + 0.6f, cx - rx * 0.4f, top + 1.0f, cx - rx * 0.62f, top - 1.8f * alza, P.luz); /* adentro de las orejas */
                triangulo(l, cx + rx * 0.62f, top + 0.6f, cx + rx * 0.4f, top + 1.0f, cx + rx * 0.62f, top - 1.8f * alza, P.luz);
                for (int k = -3; k <= 3; k++) circulo(l, cx + k * rx * 0.3f, cy + ry - 0.2f + (k & 1 ? 0.6f : 0.0f), 1.6f, P.osc); /* borde de niebla ondulado */
            }
            if (etapa == 2) for (int k = 0; k < 2; k++) trazo(l, cx + rx + 0.8f + k * 1.3f, cy + 0.4f - k * 0.5f + colay * 0.5f, cx + rx + 1.2f + k * 1.3f, cy - 0.8f - k * 0.5f + colay * 0.5f, 0.35f, P.osc);
        }
        break;
    default: break;
    }
    /* --- el cuerpo --- */
    masa(l, cx, cy, rx, ry, P.base, P.luz, P.osc);
    if ((especie != 7 || etapa == 3) && !(especie == 1 && etapa != 2)) masa(l, cx, cy + ry * 0.45f, rx * 0.6f, ry * 0.4f, P.panza, aclarar(P.panza, 0.4f), P.panza);
    /* --- encima del cuerpo --- */
    switch (especie) {
    case 1:
        if (etapa != 2) masa(l, cx, cy, 2.0f * s, 1.8f * s, P.panza, A(0xFFFFFF), P.base);
        if (etapa == 3) { lz_set(l, (int)(cx - rx * 0.6f), (int)(cy - 1), P.acento); lz_set(l, (int)(cx + rx * 0.6f), (int)cy, P.acento); }
        break;
    case 2:
        lz_set(l, (int)(cx - rx * 0.7f), (int)cy, A(0xFFF4C8)); lz_set(l, (int)(cx + rx * 0.8f), (int)cy - 1, A(0xFFF4C8));
        lz_set(l, (int)(cx + rx * 0.6f), (int)cy + 2, A(0xFFF4C8)); lz_set(l, (int)(cx - rx * 0.5f), (int)cy - 2, A(0xFFF4C8));
        break;
    case 4:
        if (etapa == 1) {
            elipse(l, cx, top + 1.2f, rx * 0.95f, 1.9f, P.osc);
            elipse(l, cx, top + 0.8f, rx * 0.85f, 1.5f, P.acento);
            for (int k = 0; k < 4; k++) lz_set(l, 7 + k * 2, (int)(top + 0.8f), P.acento2);
            rect(l, 10, (int)(top - 1.4f), 1, 2, P.acento);
            ey += 1; my += 1;
        } else if (etapa == 2) { /* rayitas de ardilla */
            trazo(l, cx - 1.2f, top + 0.6f, cx - 1.6f, top + 2.6f, 0.35f, P.acento); trazo(l, cx + 1.2f, top + 0.6f, cx + 1.6f, top + 2.6f, 0.35f, P.acento);
        } else { /* marcas rúnicas que brillan suavemente */
            trazo(l, cx - rx * 0.75f, cy + 1.0f, cx - rx * 0.55f, cy + 2.6f, 0.6f, A(0x2AB870)); trazo(l, cx + rx * 0.75f, cy + 1.0f, cx + rx * 0.55f, cy + 2.6f, 0.6f, A(0x2AB870));
            trazo(l, cx - rx * 0.75f, cy + 1.0f, cx - rx * 0.55f, cy + 2.6f, 0.3f, A(0xA8FFCC)); trazo(l, cx + rx * 0.75f, cy + 1.0f, cx + rx * 0.55f, cy + 2.6f, 0.3f, A(0xA8FFCC));
            circulo(l, cx, top + 1.5f, 1.1f, A(0x2AB870)); circulo(l, cx, top + 1.5f, 0.6f, A(0xA8FFCC)); lz_set(l, (int)cx, (int)(top + 1.4f), A(0xE4FFF0));
        }
        break;
    case 5:
        circulo(l, cx, top + 1.6f, 0.9f, etapa == 3 ? A(0xFFFFFF) : P.acento); lz_set(l, 10, (int)(top + 1.4f), A(0xFFFFFF));
        for (int sgn = -1; sgn <= 1; sgn += 2) { /* alitas de membrana, puntiagudas */
            triangulo(l, cx + sgn * (rx - 0.4f), cy - 0.4f, cx + sgn * (rx + 3.6f * s * (etapa == 1 ? 0.6f : 1.0f)), cy - 2.2f * s + (frame ? 0.8f : 0), cx + sgn * (rx + 0.4f), cy + 3.2f * s, P.osc);
            lz_set(l, (int)(cx + sgn * (rx + 1.0f)), (int)(cy + 0.4f), P.luz);
        }
        if (etapa >= 2) circulo(l, cx + rx + 3.2f, cy + ry * 0.9f - 2.6f + colay, 0.9f, P.acento); /* estrella en la punta de la cola */
        if (etapa >= 2) { lz_set(l, (int)(cx - rx - 1.0f), (int)(cy + 3.0f), A(0xFFFFFF)); lz_set(l, (int)(cx + rx + 1.0f), (int)(cy + 2.0f), A(0xE8E0FF)); } /* polvo cósmico */
        break;
    case 6:
        trazo(l, cx - 1.2f, top + 0.4f, cx - 2.2f, top - 2.0f * alza, 0.3f, P.acento2); trazo(l, cx + 1.2f, top + 0.4f, cx + 2.2f, top - 2.0f * alza, 0.3f, P.acento2);
        lz_set(l, (int)(cx - 2.2f), (int)(top - 2.2f * alza), P.acento); lz_set(l, (int)(cx + 2.2f), (int)(top - 2.2f * alza), P.acento);
        break;
    case 7:
        if (etapa == 2) { /* antifaz de mapache */
            elipse(l, cx, (float)ey + 0.5f, rx * 0.8f, 1.6f, P.osc);
        } else if (etapa == 3) { /* luna creciente en la frente */
            circulo(l, cx, top + 2.0f, 1.4f, A(0xF4E8B0)); circulo(l, cx + 0.8f, top + 1.7f, 1.2f, P.base);
        }
        circulo(l, (float)ex0 + 0.5f, (float)ey + 0.5f, 1.6f, A(0xEAF4FF)); circulo(l, (float)ex1 + 0.5f, (float)ey + 0.5f, 1.6f, A(0xEAF4FF));
        break;
    default: break;
    }
    /* patitas (las que caminan) */
    if (!flota && !(especie == 4 && etapa == 1) && ps->postura != -2) {
        int pa = frame ? 1 : 0, sep = etapa == 1 ? 2 : (etapa == 2 ? 3 : 4);
#if 1 // LINDO
        elipse(l, cx - sep, 18.4f + pa, 1.35f, 0.8f, P.osc);
        elipse(l, cx + sep, 18.4f + (1 - pa), 1.35f, 0.8f, P.osc);
#else
        rect(l, (int)cx - sep - 1, 18 + pa, 2, 1, P.osc);
        rect(l, (int)cx + sep - 1, 18 + (1 - pa), 2, 1, P.osc);
#endif
    }
#if 1 // LINDO
    if (!(especie == 7 && etapa == 1) && !(especie == 1 && etapa != 2) && ps->postura != -2) { /* bracitos redondos a los lados */
        const float ay = cy + ry * 0.32f + (frame ? 0.3f : 0.0f);
        masa(l, cx - rx + 0.15f, ay, 1.15f, 1.0f, P.base, P.luz, P.osc);
        masa(l, cx + rx - 0.15f, ay + (frame ? -0.3f : 0.0f), 1.15f, 1.0f, P.base, P.luz, P.osc);
    }
    { /* brillo suave en la frente (dos manchitas claras) */
        const float k = KL(l);
        const float gx = cx - rx * 0.42f, gy = top + ry * 0.42f;
        for (int Y = (int)((gy - 1.3f) * k); Y <= (int)((gy + 1.3f) * k); Y++)
            for (int X = (int)((gx - 1.8f) * k); X <= (int)((gx + 1.8f) * k); X++) {
                if (!opaco(l, X, Y)) continue;
                const float dx = ((float)X / k - gx) / 1.8f, dy = ((float)Y / k - gy) / 1.0f, d = dx * dx + dy * dy;
                if (d < 1.0f) l->p[Y * l->w + X] = mezclar(l->p[Y * l->w + X], A(0xFFFFFF), 0.22f * (1.0f - d));
            }
    }
#endif
    pulir_criatura(l);
    pintar_cara(l, ps->expr, ex0, ex1, ey, mx, my, LINDO_GRANDE || etapa == 1, 0, especie != 7, especie == 7 ? A(0xB48CFF) : C_MEJILLA, ps->boca, etapa);
    pintar_ropa(l, ps->ropa, cx, rx, cy, ry, top, ex0, ex1, ey);
    contornear_fino(l, especie == 7 ? A(0x0A0812) : A(0x1E1636));
    if (especie == 1 || especie == 7 || (especie == 5 && etapa == 3)) { /* destellos alrededor */
        uint32_t c = especie == 1 ? P.acento : (especie == 7 ? P.acento2 : P.acento);
        lz_set(l, 2, (int)(8 + oy), c); lz_set(l, 17, (int)(6 - oy), c); lz_set(l, 18, (int)(12 + oy), c);
    }
}


/* ---- el viaje a la isla: fondo ilustrado de la estación (la orilla con muelle, la montaña, la isla con su faro) ---- */
void art_cruce_fondo(art_img_t *dst, int periodo)
{
    /* v88: se arma directo en RGB565 (468x468 = 428 KB) graduando la PALETA, como los fondos de las zonas.
     * Antes hacían falta dos lienzos ARGB8888 a la vez (lienzo de 856 KB + imagen de 856 KB + índices de 212 KB ≈ 1,9 MB):
     * con ~1,1 MB de PSRAM libre no se cargaba y el cruce quedaba en celeste. Ahora el pico es ~640 KB y no suelta otros fondos. */
    const int MR = FD_LIENZO;
    const int e = (unsigned)s_cruce_est < 4 ? s_cruce_est : 1;
    const uint8_t *d = FC_TABLA[e].d;
    const uint32_t n = FC_TABLA[e].n;
    const unsigned np = d[0];
    const art_periodo_t per = (unsigned)periodo < PERIODO_COUNT ? (art_periodo_t)periodo : PERIODO_DIA;
    if (np == 0 || np > 255) return;
    /* el búfer de la imagen (se reusa el de antes si alcanza: capacidad contada en pixeles de 4 bytes) */
    const uint32_t pix = (uint32_t)(MR * MR), u32 = (pix + 1) / 2;
    if (u32 > dst->capacidad) {
        if (dst->px) { lv_image_cache_drop(&dst->dsc); g_art_bytes -= dst->capacidad * 4u; }
        free(dst->px);
        dst->px = malloc((size_t)u32 * 4);
        dst->capacidad = dst->px ? u32 : 0;
        g_art_bytes += dst->capacidad * 4u;
    }
    if (!dst->px) return;
    uint8_t *idx = malloc((size_t)FD_LADO * FD_LADO);
    if (!idx) { art_img_free(dst); return; }
    if (!fd_lz(d + 1 + np * 3, d + n, idx, (uint32_t)FD_LADO * FD_LADO)) { free(idx); art_img_free(dst); return; }
    uint16_t pal[256];
    for (unsigned i = 0; i < np; i++) {
        const uint32_t c = fd_grad_color(A(((uint32_t)d[1 + i * 3] << 16) | ((uint32_t)d[2 + i * 3] << 8) | d[3 + i * 3]), per);
        pal[i] = (uint16_t)((((c >> 16) & 0xF8) << 8) | (((c >> 8) & 0xFC) << 3) | ((c & 0xFF) >> 3));
    }
    uint16_t *o = (uint16_t *)dst->px;
    for (int Y = 0; Y < MR; Y++) {
        const int sy = Y == 0 ? 0 : (Y > FD_LADO ? FD_LADO - 1 : Y - 1);
        const uint8_t *fila = idx + (size_t)sy * FD_LADO;
        for (int X = 0; X < MR; X++) {
            const int sx = X == 0 ? 0 : (X > FD_LADO ? FD_LADO - 1 : X - 1);
            o[Y * MR + X] = pal[fila[sx]];
        }
    }
    free(idx);
    lv_image_cache_drop(&dst->dsc);
    memset(&dst->dsc, 0, sizeof(dst->dsc));
    dst->dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
    dst->dsc.header.cf = LV_COLOR_FORMAT_RGB565;
    dst->dsc.header.w = (uint32_t)MR;
    dst->dsc.header.h = (uint32_t)MR;
    dst->dsc.header.stride = (uint32_t)(MR * 2);
    dst->dsc.data_size = pix * 2;
    dst->dsc.data = (const uint8_t *)dst->px;
}

/* el barquito: una hoja grande con ramita de mástil, vela de hoja clara y unas piedritas de carga */
void art_bote_vela(art_img_t *dst, int inflada, int escala)
{
    const uint8_t *d = FC_BOTES[inflada ? 1 : 0].d;
    const unsigned np = d[0];
    img_preparar(dst, FC_BOTE_W, FC_BOTE_H);
    if (!dst->px) return;
    memset(dst->px, 0, (size_t)FC_BOTE_W * FC_BOTE_H * 4);
    uint8_t *idx = malloc((size_t)FC_BOTE_W * FC_BOTE_H);
    if (!idx) return;
    if (fd_lz(d + 1 + np * 3, d + FC_BOTES[inflada ? 1 : 0].n, idx, (uint32_t)FC_BOTE_W * FC_BOTE_H))
        for (int i = 0; i < FC_BOTE_W * FC_BOTE_H; i++) {
            const unsigned v = idx[i];
            if (v) dst->px[i] = A(((uint32_t)d[1 + (v - 1) * 3] << 16) | ((uint32_t)d[2 + (v - 1) * 3] << 8) | d[3 + (v - 1) * 3]);
        }
    free(idx);
    (void)escala;
}


/* ---- el paseo por el firmamento: cielo ilustrado y la ballena de nubes ---- */
void art_firmamento_fondo(art_img_t *dst)
{
    lienzo_t l = {FD_LIENZO, FD_LIENZO, malloc((size_t)FD_LIENZO * FD_LIENZO * 4), 3.0f};
    if (!l.p) return;
    if (!fd_blob(&l, FC_CIELO, sizeof(FC_CIELO))) { free(l.p); art_firmamento_fondo_viejo(dst); return; }
    ampliar(dst, &l, 1, false);
    free(l.p);
}
/* la ballena: 2 cuadros (la cola sube y baja); la espalda queda en (ART_BALLENA_ASIENTO_X, ART_BALLENA_ASIENTO_Y) */
void art_ballena(art_img_t *dst, int cuadro)
{
    const uint8_t *d = FC_BALLENAS[cuadro & 1].d;
    const unsigned np = d[0];
    img_preparar(dst, FC_BALLENA_W, FC_BALLENA_H);
    if (!dst->px) return;
    memset(dst->px, 0, (size_t)FC_BALLENA_W * FC_BALLENA_H * 4);
    uint8_t *idx = malloc((size_t)FC_BALLENA_W * FC_BALLENA_H);
    if (!idx) return;
    if (fd_lz(d + 1 + np * 3, d + FC_BALLENAS[cuadro & 1].n, idx, (uint32_t)FC_BALLENA_W * FC_BALLENA_H))
        for (int i = 0; i < FC_BALLENA_W * FC_BALLENA_H; i++) {
            const unsigned v = idx[i];
            if (v) dst->px[i] = A(((uint32_t)d[1 + (v - 1) * 3] << 16) | ((uint32_t)d[2 + (v - 1) * 3] << 8) | d[3 + (v - 1) * 3]);
        }
    free(idx);
}
