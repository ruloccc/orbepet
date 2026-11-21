/* (1.0.64) El registro y los eventos raros en un ANILLO sobre una partición cruda de la flash (sin SPIFFS).
 *
 * Por qué: el informe #11 mostró que guardar el registro en SPIFFS tardaba SIEMPRE 1,6-1,9 s, fueran 185 bytes o 12 KB,
 * con la partición usada al 12 %: el costo no era escribir, era SPIFFS (6 MB, 1536 bloques) buscando el archivo y lugar
 * libre; mientras tanto la caché de la flash está apagada y se detiene todo. Aquí escribir es copiar los bytes nuevos al
 * final (unos ms) y, cada 4 KB de registro, borrar UN sector (~45 ms).
 *
 * Formato: la zona se divide en sectores de 4 KB. Cada sector empieza con {AN_MAGIA, seq} (8 bytes) y sigue con texto.
 * Lo no escrito queda en 0xFF (la flash borrada); el texto nunca lleva 0xFF (no existe en UTF-8; si llega, se cambia por
 * '?'), así el largo de un sector es hasta el último byte que no es 0xFF. El sector vigente es el de seq mayor; al
 * llenarse se borra el siguiente y se le pone seq + 1. Al leer valen los sectores con seq en (vigente − n, vigente]:
 * borrar todo es solo saltar seq en n (un solo sector borrado).
 *
 * Quien lo incluye define AN_LEER(dir, buf, n), AN_ESCRIBIR(dir, buf, n) y AN_BORRAR(dir) (direcciones dentro de la
 * partición; devuelven 0 si salió bien). Se prueba en el PC con una flash de mentira (tests/test_anillo.c). */
#define AN_SECTOR 4096u
#define AN_CAB 8u
#define AN_MAGIA 0x31414752u /* "RGA1" */

typedef struct {
    uint32_t base, n;       /* primer sector (en bytes) y cuántos */
    uint32_t cur, off, seq; /* el vigente, dónde sigue y su número */
    bool ok;
} anillo_t;

static uint32_t an_dir(const anillo_t *a, uint32_t s) { return a->base + s * AN_SECTOR; }

/* hasta dónde está escrito el sector s (≥ AN_CAB): busca desde el final el último byte que no es 0xFF */
static uint32_t an_largo(const anillo_t *a, uint32_t s)
{
    uint8_t b[256];
    for (uint32_t fin = AN_SECTOR; fin > AN_CAB;) {
        const uint32_t ini = fin >= AN_CAB + sizeof b ? fin - (uint32_t)sizeof b : AN_CAB;
        if (AN_LEER(an_dir(a, s) + ini, b, fin - ini) != 0) return AN_CAB;
        for (uint32_t i = fin - ini; i > 0; i--)
            if (b[i - 1] != 0xFF) return ini + i;
        fin = ini;
    }
    return AN_CAB;
}

static bool an_cab(const anillo_t *a, uint32_t s, uint32_t *seq)
{
    uint32_t c[2];
    if (AN_LEER(an_dir(a, s), c, sizeof c) != 0 || c[0] != AN_MAGIA || c[1] == 0xFFFFFFFFu) return false;
    *seq = c[1];
    return true;
}

/* empieza el sector s con el número seq (lo borra antes) */
static bool an_empezar(anillo_t *a, uint32_t s, uint32_t seq)
{
    const uint32_t c[2] = {AN_MAGIA, seq};
    if (AN_BORRAR(an_dir(a, s)) != 0) return false;
    if (AN_ESCRIBIR(an_dir(a, s), c, sizeof c) != 0) return false;
    a->cur = s; a->off = AN_CAB; a->seq = seq;
    return true;
}

static bool an_abrir(anillo_t *a, uint32_t base, uint32_t n)
{
    a->base = base; a->n = n; a->ok = false;
    if (n < 2) return false;
    bool hay = false;
    uint32_t mejor = 0, sm = 0;
    for (uint32_t s = 0; s < n; s++) {
        uint32_t q;
        if (an_cab(a, s, &q) && (!hay || q > mejor)) { hay = true; mejor = q; sm = s; }
    }
    if (!hay) { a->ok = an_empezar(a, 0, 1); return a->ok; }
    a->cur = sm; a->seq = mejor; a->off = an_largo(a, sm);
    a->ok = true;
    return true;
}

static bool an_avanzar(anillo_t *a) { return an_empezar(a, (a->cur + 1) % a->n, a->seq + 1); }

static bool an_agregar(anillo_t *a, const char *t, uint32_t n)
{
    if (!a->ok) return false;
    uint8_t b[256]; /* en la RAM interna (pila): la flash escribe sin pasar por la PSRAM */
    while (n > 0) {
        if (a->off >= AN_SECTOR && !an_avanzar(a)) { a->ok = false; return false; }
        uint32_t k = AN_SECTOR - a->off;
        if (k > n) k = n;
        if (k > sizeof b) k = (uint32_t)sizeof b;
        memcpy(b, t, k);
        for (uint32_t i = 0; i < k; i++)
            if (b[i] == 0xFF) b[i] = '?';
        if (AN_ESCRIBIR(an_dir(a, a->cur) + a->off, b, k) != 0) { a->off = AN_SECTOR; return false; } /* (el próximo va a otro sector) */
        a->off += k; t += k; n -= k;
    }
    return true;
}

/* lo más nuevo que quepa en cap bytes, de lo viejo a lo nuevo, empezando en una línea entera si hubo que cortar */
static int an_leer_todo(const anillo_t *a, char *buf, int cap)
{
    if (!a->ok || cap <= 0) return 0;
    uint32_t total = 0, validos = 0;
    for (uint32_t i = 0; i < a->n; i++) { /* i = 0: el más viejo que todavía vale */
        const uint32_t q = a->seq - (a->n - 1) + i, s = (a->cur + 1 + i) % a->n;
        uint32_t qs;
        if (q > a->seq || !an_cab(a, s, &qs) || qs != q) continue;
        total += (s == a->cur ? a->off : an_largo(a, s)) - AN_CAB;
        validos++;
    }
    uint32_t saltar = total > (uint32_t)cap ? total - (uint32_t)cap : 0;
    /* con todos los sectores en uso, el más viejo puede empezar a media línea (la otra mitad ya se borró) */
    const bool cortado = saltar > 0 || validos == a->n;
    int v = 0;
    for (uint32_t i = 0; i < a->n && v < cap; i++) {
        const uint32_t q = a->seq - (a->n - 1) + i, s = (a->cur + 1 + i) % a->n;
        uint32_t qs;
        if (q > a->seq || !an_cab(a, s, &qs) || qs != q) continue;
        const uint32_t fin = s == a->cur ? a->off : an_largo(a, s);
        uint32_t ini = AN_CAB;
        if (saltar) {
            const uint32_t hay = fin - AN_CAB;
            if (saltar >= hay) { saltar -= hay; continue; }
            ini += saltar; saltar = 0;
        }
        uint32_t k = fin - ini;
        if (k > (uint32_t)(cap - v)) k = (uint32_t)(cap - v);
        /* de a 256 por un búfer en la pila: buf suele estar en la PSRAM y leer directo ahí haría que ESP-IDF pida un
         * búfer intermedio de hasta 16 KB en la RAM interna (que anda por los 25 KB libres) */
        bool mal = false;
        for (uint32_t h = 0; h < k && !mal;) {
            uint8_t t[256];
            const uint32_t c = k - h > sizeof t ? (uint32_t)sizeof t : k - h;
            if (AN_LEER(an_dir(a, s) + ini + h, t, c) != 0) mal = true;
            else { memcpy(buf + v + h, t, c); h += c; }
        }
        if (mal) break;
        v += (int)k;
    }
    if (cortado) { /* que no empiece con media línea */
        int d = 0;
        while (d < v && buf[d] != '\n') d++;
        if (d < v) { d++; memmove(buf, buf + d, (size_t)(v - d)); v -= d; }
    }
    return v;
}

/* deja el anillo vacío: lo viejo queda fuera de la ventana de números (un solo sector borrado) */
static bool an_vaciar(anillo_t *a)
{
    if (!a->ok) return false;
    a->ok = an_empezar(a, (a->cur + 1) % a->n, a->seq + a->n);
    return a->ok;
}
