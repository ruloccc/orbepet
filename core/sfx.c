/* Síntesis de sonidos: tonos con barrido de frecuencia y ruido filtrado. */
#include "sfx.h"

#include <math.h>
#include <stdbool.h>
#include <string.h>

typedef enum { ONDA_SENO, ONDA_TRIANGULO, ONDA_CUADRADA, ONDA_SIERRA } onda_t;

typedef struct {
    int16_t *buf;
    int n;   /* capacidad */
    int fin; /* última muestra usada */
    int fs;
    uint32_t semilla;
} mezcla_t;

static float onda(onda_t o, float fase)
{
    float x = fase - floorf(fase);
    switch (o) {
    case ONDA_TRIANGULO: return 4.0f * fabsf(x - 0.5f) - 1.0f;
    case ONDA_CUADRADA: return x < 0.5f ? 1.0f : -1.0f;
    case ONDA_SIERRA: return 2.0f * x - 1.0f;
    default: return sinf(6.2831853f * x);
    }
}

/* suma con saturación directamente en el búfer de salida (sin memoria extra) */
static void mezclar(mezcla_t *m, int i, float v)
{
    int x = m->buf[i] + (int)(v * 30000.0f);
    m->buf[i] = (int16_t)(x > 32767 ? 32767 : (x < -32768 ? -32768 : x));
}

/* envolvente: ataque corto y caída exponencial suave */
static float env(float t, float dur)
{
    const float ataque = 0.015f;
    if (t < ataque) return t / ataque;
    float r = (t - ataque) / (dur - ataque);
    return r >= 1.0f ? 0.0f : (1.0f - r) * (1.0f - r);
}

static void tono(mezcla_t *m, float f0, float f1, float dur, onda_t o, float vol, float inicio)
{
    int a = (int)(inicio * (float)m->fs), n = (int)(dur * (float)m->fs);
    float fase = 0;
    for (int i = 0; i < n && a + i < m->n; i++) {
        float t = (float)i / (float)m->fs;
        float f = f0 * powf(f1 / f0, t / dur); /* barrido exponencial */
        fase += f / (float)m->fs;
        mezclar(m, a + i, onda(o, fase) * env(t, dur) * vol);
    }
    if (a + n > m->fin) m->fin = a + n < m->n ? a + n : m->n;
}

static void ruido(mezcla_t *m, float dur, float frec, float vol, float inicio)
{
    int a = (int)(inicio * (float)m->fs), n = (int)(dur * (float)m->fs);
    /* filtro pasa-banda simple (biquad) centrado en frec */
    float w = 6.2831853f * frec / (float)m->fs, q = 1.2f;
    float alfa = sinf(w) / (2 * q), c = cosf(w);
    float b0 = alfa, b2 = -alfa, a0 = 1 + alfa, a1 = -2 * c, a2 = 1 - alfa;
    float x1 = 0, x2 = 0, y1 = 0, y2 = 0;
    for (int i = 0; i < n && a + i < m->n; i++) {
        m->semilla = m->semilla * 1664525u + 1013904223u;
        float x = (float)(m->semilla >> 8) / 8388608.0f - 1.0f;
        float y = (b0 * x + b2 * x2 - a1 * y1 - a2 * y2) / a0;
        x2 = x1; x1 = x; y2 = y1; y1 = y;
        float t = (float)i / (float)m->fs;
        mezclar(m, a + i, y * 3.0f * (1.0f - t / dur) * vol);
    }
    if (a + n > m->fin) m->fin = a + n < m->n ? a + n : m->n;
}

/* nota sostenida (bocinas, silbatos, la canción de cuna): sube rápido, se mantiene y se apaga suave */
static void nota_larga(mezcla_t *m, float f, float dur, onda_t o, float vol, float inicio)
{
    int a = (int)(inicio * (float)m->fs), n = (int)(dur * (float)m->fs);
    float fase = 0;
    for (int i = 0; i < n && a + i < m->n; i++) {
        float t = (float)i / (float)m->fs;
        float e = t < 0.03f ? t / 0.03f : (t > dur - 0.12f ? (dur - t) / 0.12f : 1.0f);
        fase += f * (1.0f + 0.004f * sinf(t * 31.0f)) / (float)m->fs;
        mezclar(m, a + i, onda(o, fase) * e * vol);
    }
    if (a + n > m->fin) m->fin = a + n < m->n ? a + n : m->n;
}

/* ---- la voz del animalito: "píos" suaves y redonditos (seno con un poco de su octava), con un
 * vibrato chiquito y notas de la escala, para que suene tierno y nunca áspero. Vive entre 300 Hz y
 * 2 kHz, que es lo que el parlante chico del reloj da bien. */
static void pio(mezcla_t *m, float f0, float f1, float dur, float vibrato, float vol, float inicio)
{
    int ini = (int)(inicio * (float)m->fs), n = (int)(dur * (float)m->fs);
    float fase = 0;
    for (int i = 0; i < n && ini + i < m->n; i++) {
        float t = (float)i / (float)m->fs, k = t / dur;
        /* la curva de tono, suavizada (empieza y termina despacio) */
        float kk = k * k * (3.0f - 2.0f * k);
        float f = f0 * powf(f1 / f0, kk) * (1.0f + vibrato * sinf(6.2831853f * 5.5f * t));
        fase += f / (float)m->fs;
        float x = sinf(6.2831853f * fase) + 0.22f * sinf(2.0f * 6.2831853f * fase) + 0.06f * sinf(3.0f * 6.2831853f * fase);
        /* envolvente redondita: sube rápido y se apaga como una campanita */
        float e = t < 0.012f ? t / 0.012f : 1.0f;
        float r = 0.5f + 0.5f * cosf(3.14159f * k);
        e *= 0.35f + 0.65f * r;
        if (t > dur - 0.02f) e *= (dur - t) / 0.02f;
        mezclar(m, ini + i, x * e * vol * 0.8f);
    }
    if (ini + n > m->fin) m->fin = ini + n < m->n ? ini + n : m->n;
}

/* ronroneo tierno: un "mmm" tibio que tiembla suavecito */
static void ronroneo(mezcla_t *m, float dur, float vol, float inicio)
{
    int ini = (int)(inicio * (float)m->fs), n = (int)(dur * (float)m->fs);
    float f1 = 0, f2 = 0;
    for (int i = 0; i < n && ini + i < m->n; i++) {
        float t = (float)i / (float)m->fs;
        f1 += 330.0f / (float)m->fs;
        f2 += 495.0f / (float)m->fs;
        float x = sinf(6.2831853f * f1) + 0.45f * sinf(6.2831853f * f2);
        float trem = 0.65f + 0.35f * sinf(6.2831853f * 14.0f * t);
        float e = t < 0.1f ? t / 0.1f : (t > dur - 0.15f ? (dur - t) / 0.15f : 1.0f);
        mezclar(m, ini + i, x * trem * e * vol * 0.6f);
    }
    if (ini + n > m->fin) m->fin = ini + n < m->n ? ini + n : m->n;
}

/* burbujas: gotitas que suben de tono */
static void burbujas(mezcla_t *m, int cuantas, float dur, float vol, float inicio)
{
    for (int k = 0; k < cuantas; k++) {
        m->semilla = m->semilla * 1664525u + 1013904223u;
        float t0 = inicio + dur * (float)(m->semilla >> 8) / 16777216.0f;
        float f = 700.0f + 900.0f * (float)((m->semilla >> 4) & 255) / 255.0f;
        tono(m, f, f * 1.9f, .05f, ONDA_SENO, vol, t0);
    }
}

static void voz_frase(mezcla_t *m, int id, bool tarareo);
static void voz_la(mezcla_t *m, int k);
static bool voz_patron(mezcla_t *m, sound_t s);

int sfx_render(sound_t s, int16_t *out, int max_muestras, int fs)
{
    int cap = (int)(SFX_MAX_SEGUNDOS * (float)fs);
    if (cap > max_muestras) cap = max_muestras;
    memset(out, 0, (size_t)cap * sizeof(int16_t));
    mezcla_t m = {out, cap, 0, fs, 12345u + (uint32_t)s};
    if (voz_patron(&m, s)) return m.fin; /* los sonidos de la mascota son su voz */

    switch (s) {
    case SND_ALEGRE: pio(&m, 784, 988, .1f, .01f, .5f, 0); pio(&m, 988, 1318, .18f, .015f, .5f, .11f); break; /* "pi-pii!" */
    case SND_RISA: for (int i = 0; i < 4; i++) pio(&m, 1318.0f - (float)i * 110, 1480.0f - (float)i * 110, .07f, 0, .42f, (float)i * .1f); break; /* "ji-ji-ji" */
    case SND_RONRONEO: ronroneo(&m, 1.1f, .5f, 0); break;
    case SND_QUEJA: pio(&m, 880, 659, .32f, .03f, .45f, 0); break; /* "uuu..." */
    case SND_LLANTO: for (int i = 0; i < 3; i++) pio(&m, 988.0f - (float)i * 50, 740.0f - (float)i * 40, .26f, .04f, .38f, (float)i * .3f); break; /* gimoteo bajito */
    case SND_GRUNIDO: pio(&m, 440, 349, .16f, .05f, .42f, 0); pio(&m, 415, 330, .2f, .05f, .38f, .17f); break; /* "hmpf, hmpf" */
    case SND_BOSTEZO: pio(&m, 659, 392, .55f, .01f, .4f, 0); pio(&m, 392, 349, .25f, .01f, .3f, .52f); break; /* "aaa-uum" */
    case SND_RONQUIDO: pio(&m, 523, 659, .45f, 0, .18f, 0); pio(&m, 523, 440, .5f, .02f, .18f, .6f); break; /* silbidito al respirar */
    case SND_ESTORNUDO: pio(&m, 659, 784, .1f, 0, .35f, 0); pio(&m, 784, 988, .1f, 0, .38f, .14f); ruido(&m, .06f, 3200, .3f, .3f); pio(&m, 1568, 1318, .08f, 0, .35f, .3f); break; /* "a-a-¡chú!" */
    case SND_TOS: for (int i = 0; i < 2; i++) { pio(&m, 587, 494, .08f, 0, .32f, (float)i * .18f); ruido(&m, .04f, 1600, .12f, (float)i * .18f); } break;
    case SND_COMER: for (int i = 0; i < 3; i++) { pio(&m, 659, 587, .07f, 0, .32f, (float)i * .15f); ruido(&m, .035f, 2400, .15f, (float)i * .15f + .02f); } break; /* "ñam ñam" */
    case SND_CHAPOTEO: for (int i = 0; i < 4; i++) ruido(&m, .08f, 2200.0f + (float)i * 300, .28f, (float)i * .1f); burbujas(&m, 3, .4f, .2f, .05f); break;
    case SND_SORPRESA: pio(&m, 523, 1046, .18f, 0, .5f, 0); break; /* "¿uh?" */
    case SND_SUSPIRO_AMOR: pio(&m, 659, 880, .25f, .03f, .42f, 0); pio(&m, 880, 784, .38f, .04f, .4f, .24f); break; /* "aaww" */
    case SND_PELEA: for (int i = 0; i < 3; i++) { pio(&m, 784, 587, .08f, .04f, .35f, (float)i * .12f); ruido(&m, .05f, 2000, .12f, (float)i * .12f + .03f); } break; /* "¡ey, ey!" */
    case SND_LLAMADO: pio(&m, 1046, 1175, .12f, .01f, .48f, 0); pio(&m, 1318, 1568, .18f, .015f, .48f, .16f); break; /* "mii-mii!" */
    case SND_TRUENO: ruido(&m, .25f, 400, .7f, 0); ruido(&m, 1.0f, 90, .8f, .15f); break;
    /* fuegos artificiales: cada cohete son tres partes (silbido al subir, estallido hondo, crepitar final) */
    case SND_COHETE_SILBA: tono(&m, 700, 3000, .62f, ONDA_SENO, .2f, 0); tono(&m, 1400, 5200, .62f, ONDA_SENO, .06f, 0); ruido(&m, .6f, 3800, .05f, 0); break;
    case SND_COHETE_BUM_P: ruido(&m, .09f, 2200, .5f, 0); tono(&m, 180, 70, .28f, ONDA_SENO, .55f, 0); ruido(&m, .22f, 500, .35f, .01f); break;
    case SND_COHETE_BUM_M: ruido(&m, .1f, 1800, .55f, 0); tono(&m, 130, 45, .5f, ONDA_SENO, .7f, 0); ruido(&m, .45f, 320, .5f, .01f); break;
    case SND_COHETE_BUM_G: ruido(&m, .12f, 1400, .6f, 0); tono(&m, 95, 32, .85f, ONDA_SENO, .85f, 0); tono(&m, 60, 28, 1.0f, ONDA_SENO, .45f, .05f); ruido(&m, .8f, 220, .6f, .01f); break;
    case SND_COHETE_CHISPAS:
        for (int i = 0; i < 26; i++) {
            m.semilla = m.semilla * 1664525u + 1013904223u;
            float azar = (float)(m.semilla >> 8) / 16777216.0f;
            ruido(&m, .018f, 2500.0f + azar * 3500.0f, .24f * (1.0f - (float)i / 34.0f), (float)i * .034f + azar * .03f);
        }
        break;
    case SND_COHETE_LEJOS: ruido(&m, .15f, 500, .22f, 0); tono(&m, 80, 40, .5f, ONDA_SENO, .28f, .02f); ruido(&m, .4f, 160, .2f, .03f); break;
    case SND_OLFATEO: for (int i = 0; i < 3; i++) ruido(&m, .05f, 3500, .2f, (float)i * .11f); break;
    case SND_CAVAR: for (int i = 0; i < 4; i++) ruido(&m, .07f, 900.0f + (float)(i % 2) * 500, .3f, (float)i * .13f); break;
    case SND_SUSPIRO: pio(&m, 587, 440, .45f, .01f, .3f, 0); ruido(&m, .3f, 1500, .06f, 0); break;
    case SND_TOQUE: pio(&m, 1046, 1318, .07f, 0, .45f, 0); break; /* "pip" */
    case SND_ABRAZO: pio(&m, 523, 659, .3f, .02f, .4f, 0); pio(&m, 659, 784, .3f, .02f, .4f, .28f); ronroneo(&m, .6f, .3f, .55f); break; /* "mmm" y ronronea */
    case SND_MAREO: pio(&m, 784, 523, .8f, .07f, .4f, 0); break; /* "uuuooo" que da vueltas */
    case SND_HOLA_AMIGO: pio(&m, 523, 523, .14f, 0, .42f, 0); pio(&m, 659, 659, .14f, 0, .42f, .15f); pio(&m, 784, 1046, .25f, .02f, .42f, .3f); break;
    case SND_PLOP: tono(&m, 900, 180, .14f, ONDA_SENO, .5f, 0); ruido(&m, .12f, 1500, .3f, .06f); break;
    case SND_PESCO: {
        static const float N[] = {523.25f, 659.25f, 783.99f, 1046.5f};
        for (int i = 0; i < 4; i++) tono(&m, N[i], N[i], .12f, ONDA_TRIANGULO, .3f, (float)i * .09f);
        break;
    }
    case SND_BIENVENIDA: {
        static const float N[] = {523.25f, 659.25f, 783.99f, 1046.5f, 1318.5f};
        for (int i = 0; i < 5; i++) tono(&m, N[i], N[i], .22f, ONDA_TRIANGULO, .4f, (float)i * .13f);
        tono(&m, 1046.5f, 1046.5f, .8f, ONDA_SENO, .3f, .7f); tono(&m, 1568.0f, 1568.0f, .8f, ONDA_SENO, .2f, .7f);
        tono(&m, 2093.0f, 2637.0f, .25f, ONDA_SENO, .15f, 1.2f);
        break;
    }
    case SND_LANZAR: ruido(&m, .25f, 2500, .35f, 0); tono(&m, 500, 1400, .2f, ONDA_SENO, .2f, 0); break;
    case SND_REBOTE: tono(&m, 180, 420, .12f, ONDA_SENO, .45f, 0); break;
    case SND_ATRAPA: tono(&m, 900, 1300, .06f, ONDA_TRIANGULO, .3f, 0); tono(&m, 1300, 1760, .1f, ONDA_SENO, .35f, .06f); break;
    case SND_TRUCO: {
        static const float N[] = {523.25f, 659.25f, 783.99f, 1046.5f};
        static const float D[] = {.1f, .1f, .1f, .35f};
        float t0 = 0;
        for (int i = 0; i < 4; i++) { tono(&m, N[i], N[i], D[i], ONDA_TRIANGULO, .38f, t0); t0 += D[i] * .9f; }
        break;
    }
    case SND_CONTAR: for (int i = 0; i < 3; i++) tono(&m, i == 2 ? 1046.5f : 784.0f, i == 2 ? 1046.5f : 784.0f, .12f, ONDA_TRIANGULO, .3f, (float)i * .45f); break;
    case SND_ENCONTRADA: tono(&m, 659.25f, 659.25f, .1f, ONDA_TRIANGULO, .4f, 0); tono(&m, 987.77f, 987.77f, .35f, ONDA_TRIANGULO, .4f, .1f); break;
    case SND_DESLIZA: tono(&m, 1200, 500, .45f, ONDA_SENO, .35f, 0); break;
    case SND_BANO: /* agua, burbujas y un brillito al final: ¡limpia! */
        ruido(&m, .35f, 2600, .35f, 0); burbujas(&m, 9, .9f, .22f, .1f);
        tono(&m, 1568, 1568, .12f, ONDA_SENO, .2f, 1.05f); tono(&m, 2093, 2093, .25f, ONDA_SENO, .2f, 1.15f);
        break;
    case SND_CEPILLO: for (int i = 0; i < 4; i++) ruido(&m, .14f, 4200, .22f, (float)i * .18f); ronroneo(&m, .6f, .3f, .55f); break;
    case SND_LIMPIO: tono(&m, 1318.5f, 1318.5f, .08f, ONDA_SENO, .25f, 0); tono(&m, 1760, 1760, .08f, ONDA_SENO, .25f, .07f); tono(&m, 2637, 2637, .2f, ONDA_SENO, .2f, .14f); break;
    case SND_ROPA: ruido(&m, .12f, 3500, .35f, 0); ruido(&m, .14f, 2800, .3f, .14f); break;
    case SND_NUEVO: tono(&m, 300, 500, .08f, ONDA_TRIANGULO, .3f, 0); tono(&m, 784, 784, .1f, ONDA_SENO, .35f, .1f); tono(&m, 1175, 1175, .25f, ONDA_SENO, .35f, .2f); break;
    case SND_PELUCHE: tono(&m, 1400, 1700, .08f, ONDA_SENO, .35f, 0); tono(&m, 1600, 1300, .1f, ONDA_SENO, .3f, .1f); break;
    case SND_TAMBOR: for (int i = 0; i < 3; i++) { tono(&m, 220, 140, .12f, ONDA_SENO, .55f, (float)i * .22f); ruido(&m, .04f, 1500, .3f, (float)i * .22f); } break;
    case SND_COLUMPIO: tono(&m, 1500, 1800, .25f, ONDA_TRIANGULO, .12f, 0); tono(&m, 1800, 1450, .25f, ONDA_TRIANGULO, .12f, .55f); break;
    case SND_SALPICON: /* plic: una gotita que cae y salpica un poquito */
        ruido(&m, .06f, 3000, .32f, 0); tono(&m, 900, 1500, .06f, ONDA_SENO, .25f, .01f); burbujas(&m, 2, .18f, .14f, .05f); break;
    case SND_SEMILLA: tono(&m, 784, 784, .09f, ONDA_TRIANGULO, .3f, 0); tono(&m, 988, 988, .09f, ONDA_TRIANGULO, .3f, .1f);
        tono(&m, 1319, 1319, .25f, ONDA_SENO, .32f, .2f); break;
    case SND_FOTO: ruido(&m, .03f, 4000, .6f, 0); ruido(&m, .05f, 2500, .5f, .08f); break;
    case SND_CHAPUZON: /* se tira al agua: plaf y burbujas */
        ruido(&m, .18f, 700, .6f, 0); ruido(&m, .3f, 2400, .25f, .03f); tono(&m, 600, 200, .15f, ONDA_SENO, .25f, 0);
        burbujas(&m, 5, .5f, .2f, .15f); break;
    case SND_CAIDA: tono(&m, 220, 90, .16f, ONDA_SENO, .55f, 0); ruido(&m, .08f, 300, .4f, 0); break; /* ¡pum! suave */
    case SND_TESORO: { /* campanitas */
        static const float N[] = {1318.5f, 1568.0f, 2093.0f, 2637.0f};
        for (int i = 0; i < 4; i++) { tono(&m, N[i], N[i], .35f, ONDA_SENO, .25f, (float)i * .09f); tono(&m, N[i] * 2, N[i] * 2, .2f, ONDA_SENO, .06f, (float)i * .09f); }
        break;
    }
    case SND_HUELE: /* olfateo y un pitido curioso que sube */
        for (int i = 0; i < 3; i++) ruido(&m, .05f, 3500, .2f, (float)i * .11f);
        pio(&m, 880, 1320, .18f, .02f, .35f, .38f); break;
    case SND_RARO: /* brillitos que suben */
        for (int i = 0; i < 7; i++) tono(&m, 1046.5f * powf(1.12f, (float)i), 1046.5f * powf(1.12f, (float)i), .22f, ONDA_SENO, .2f, (float)i * .07f);
        break;
    case SND_META: tono(&m, 1568, 1568, .12f, ONDA_TRIANGULO, .3f, 0); tono(&m, 2093, 2093, .5f, ONDA_SENO, .32f, .1f); break; /* ding */
    case SND_FIESTA: { /* cornetita */
        static const float N[] = {523.25f, 659.25f, 783.99f, 1046.5f, 783.99f, 1046.5f};
        static const float D[] = {.1f, .1f, .1f, .25f, .1f, .45f};
        float t0 = 0;
        for (int i = 0; i < 6; i++) { nota_larga(&m, N[i], D[i] * .9f, ONDA_CUADRADA, .1f, t0); nota_larga(&m, N[i], D[i] * .9f, ONDA_TRIANGULO, .2f, t0); t0 += D[i]; }
        break;
    }
    case SND_DESPERTADOR: { /* melodía alegre de campanitas, dos veces (la pantalla la repite) */
        static const float N[] = {783.99f, 1046.5f, 1318.5f, 1046.5f, 1567.98f, 1318.5f, 1046.5f, 1174.66f};
        for (int r = 0; r < 2; r++)
            for (int i = 0; i < 8; i++) {
                float t0 = (float)r * 1.25f + (float)i * .14f + (i == 7 ? .1f : 0);
                tono(&m, N[i], N[i], i == 7 ? .4f : .16f, ONDA_TRIANGULO, .34f, t0);
                tono(&m, N[i] * 2, N[i] * 2, .1f, ONDA_SENO, .1f, t0);
            }
        break;
    }
    case SND_A_DORMIR: { /* canción de cuna original, lenta y suave */
        static const float N[] = {659.25f, 523.25f, 587.33f, 493.88f, 523.25f, 440.0f, 493.88f, 392.0f, 523.25f};
        static const float D[] = {.5f, .5f, .5f, .5f, .75f, .25f, .5f, .5f, 1.2f};
        float t0 = 0;
        for (int i = 0; i < 9; i++) {
            nota_larga(&m, N[i], D[i] * .95f, ONDA_SENO, .3f, t0);
            nota_larga(&m, N[i] / 2, D[i] * .95f, ONDA_TRIANGULO, .08f, t0);
            t0 += D[i];
        }
        break;
    }
    case SND_AVION: /* viento mágico: un soplo y campanitas que suben */
        ruido(&m, 1.6f, 700, .25f, 0); ruido(&m, 1.2f, 1800, .1f, .2f);
        for (int i = 0; i < 6; i++) tono(&m, 880.0f * powf(1.19f, (float)i), 880.0f * powf(1.19f, (float)i), .45f, ONDA_SENO, .2f, .3f + (float)i * .12f);
        break;
    case SND_BOCINA: /* dos campanadas suaves */
        for (int k = 0; k < 2; k++) {
            float t0 = (float)k * .9f, f = k ? 523.25f : 659.25f;
            tono(&m, f, f, 1.2f, ONDA_SENO, .35f, t0); tono(&m, f * 2.76f, f * 2.76f, .5f, ONDA_SENO, .08f, t0); tono(&m, f * 5.4f, f * 5.4f, .2f, ONDA_SENO, .04f, t0);
        }
        break;
    case SND_SILBATO: /* chu-chuu de cristal y chispitas */
        for (int k = 0; k < 2; k++) {
            float t0 = (float)k * .6f, d = k ? .8f : .4f;
            nota_larga(&m, 1174.66f, d, ONDA_SENO, .14f, t0); nota_larga(&m, 1479.98f, d, ONDA_SENO, .12f, t0); nota_larga(&m, 1760.0f, d, ONDA_SENO, .1f, t0);
        }
        for (int i = 0; i < 8; i++) tono(&m, 2093.0f + (float)(i % 3) * 400.0f, 2093.0f + (float)(i % 3) * 400.0f, .12f, ONDA_SENO, .12f, 1.5f + (float)i * .14f);
        break;
    case SND_GLOBO: /* soplido de diente de león y semillas que brillan */
        ruido(&m, .9f, 1200, .3f, 0); ruido(&m, .7f, 500, .2f, .05f);
        for (int i = 0; i < 6; i++) tono(&m, 1568.0f + (float)(i * 197 % 600), 1568.0f + (float)(i * 197 % 600), .2f, ONDA_SENO, .14f, .6f + (float)i * .15f);
        break;
    case SND_CAJA_MUSICA: { /* una melodía propia, de caja de música: 16 notas a medio segundo (el pulso del baile) */
        static const float NOTA[16] = {659.25f, 783.99f, 880.0f, 783.99f, 659.25f, 587.33f, 523.25f, 587.33f,
                                       659.25f, 783.99f, 1046.5f, 987.77f, 880.0f, 783.99f, 659.25f, 523.25f};
        for (int i = 0; i < 16; i++) {
            float t0 = (float)i * .5f;
            tono(&m, NOTA[i], NOTA[i], .45f, ONDA_SENO, .30f, t0);              /* el dientecito del peine */
            tono(&m, NOTA[i] * 2, NOTA[i] * 2, .18f, ONDA_SENO, .10f, t0);      /* su brillo */
            if (i % 4 == 0) tono(&m, NOTA[i] / 2, NOTA[i] / 2, .4f, ONDA_TRIANGULO, .12f, t0); /* el bajo, cada cuatro */
        }
        break;
    }
    case SND_CRISTAL_AZUL: case SND_CRISTAL_VERDE: case SND_CRISTAL_ROSA: case SND_CRISTAL_ORO: case SND_CRISTAL_LILA: case SND_CRISTAL_BLANCO: {
        static const float F[6] = {523.25f, 659.25f, 783.99f, 1046.5f, 587.33f, 880.0f};
        float f = F[s - SND_CRISTAL_AZUL];
        tono(&m, f, f, .9f, ONDA_SENO, .30f, 0);
        tono(&m, f * 2.0f, f * 2.0f, .5f, ONDA_SENO, .10f, 0);
        tono(&m, f * 3.01f, f * 3.01f, .25f, ONDA_SENO, .05f, 0);
        break;
    }
    case SND_PRIMAVERA: { /* una melodía original con los seis cristales de la cueva (do, re, mi, sol, la, do alto), de 16 notas */
        static const float F[6] = {523.25f, 659.25f, 783.99f, 1046.5f, 587.33f, 880.0f}; /* azul, verde, rosado, dorado, violeta, blanco */
        static const uint8_t M[16] = {1, 2, 5, 2, 1, 4, 0, 4, 1, 2, 3, 5, 2, 1, 4, 0};
        for (int i = 0; i < 16; i++) {
            const float f = F[M[i]], t0 = (float)i * .44f + (i >= 12 ? .12f : 0.0f);
            tono(&m, f, f, .85f, ONDA_SENO, .28f, t0);
            tono(&m, f * 2.0f, f * 2.0f, .45f, ONDA_SENO, .09f, t0);
            tono(&m, f * 3.01f, f * 3.01f, .22f, ONDA_SENO, .04f, t0);
        }
        tono(&m, F[0] / 2, F[0] / 2, 1.2f, ONDA_TRIANGULO, .10f, 0.0f);   /* un fondo suave al empezar */
        tono(&m, F[0], F[0], 1.4f, ONDA_SENO, .16f, 15 * .44f + .12f);    /* y la última nota se queda sonando */
        break;
    }
    case SND_HADAS: /* campanitas que giran y suben */
        for (int i = 0; i < 10; i++) {
            float f = 1318.5f * powf(1.0595f, (float)((i * 5) % 12));
            tono(&m, f, f, .35f, ONDA_SENO, .14f, (float)i * .08f);
        }
        tono(&m, 2093.0f, 2093.0f, .7f, ONDA_SENO, .10f, .85f);
        break;
    case SND_ALETEO: /* alas que se acercan: soplos cortos y un piar */
        for (int i = 0; i < 6; i++) ruido(&m, .07f, 900.0f + (float)i * 120.0f, .10f + (float)i * .02f, (float)i * .14f);
        tono(&m, 1800.0f, 2400.0f, .12f, ONDA_SENO, .12f, .9f);
        break;
    case SND_CUMPLEANOS: {
        /* melodía tradicional del cumpleaños (dominio público), en compás de 3/4 */
        static const float NOTA[] = {392.0f, 392.0f, 440.0f, 392.0f, 523.25f, 493.88f,
                                     392.0f, 392.0f, 440.0f, 392.0f, 587.33f, 523.25f,
                                     392.0f, 392.0f, 783.99f, 659.25f, 523.25f, 493.88f, 440.0f,
                                     698.46f, 698.46f, 659.25f, 523.25f, 587.33f, 523.25f};
        static const float TIEMPO[] = {.75f, .25f, 1, 1, 1, 2, .75f, .25f, 1, 1, 1, 2, .75f, .25f, 1, 1, 1, 1, 2,
                                       .75f, .25f, 1, 1, 1, 3};
        const float pulso = .3f;
        float t0 = 0;
        for (int i = 0; i < (int)(sizeof NOTA / sizeof NOTA[0]); i++) {
            float d = TIEMPO[i] * pulso;
            tono(&m, NOTA[i], NOTA[i], d * .95f, ONDA_TRIANGULO, .38f, t0);
            tono(&m, NOTA[i] * 2, NOTA[i] * 2, d * .6f, ONDA_SENO, .1f, t0);
            t0 += d;
        }
        break;
    }
    default:
        if (s >= SND_VOZ_0 && s <= SND_VOZ_23) { voz_frase(&m, (int)(s - SND_VOZ_0), false); break; } /* canta con sílabas inventadas */
        if (s >= SND_LA_0 && s <= SND_LA_5) { voz_la(&m, (int)(s - SND_LA_0)); break; } /* (v38) un «la» cantado en la nota de un cristal */
        if (s == SND_CRISTAL_CELESTE) { /* (v39) el cristal de la orilla izquierda: un do grave, redondo como un bajo */
            const float f = 261.63f;
            tono(&m, f, f, 1.1f, ONDA_SENO, .34f, 0); tono(&m, f * 2.0f, f * 2.0f, .55f, ONDA_SENO, .08f, 0);
            tono(&m, f * 0.5f, f * 0.5f, .7f, ONDA_TRIANGULO, .10f, 0);
            break;
        }
        if (s == SND_CRISTAL_MORADO) { /* (v39) el de la orilla derecha: un re alto, con brillo de campanita */
            const float f = 1174.66f;
            tono(&m, f, f, .8f, ONDA_SENO, .22f, 0); tono(&m, f * 2.76f, f * 2.76f, .35f, ONDA_SENO, .06f, 0);
            tono(&m, f * 5.4f, f * 5.4f, .15f, ONDA_SENO, .025f, 0);
            break;
        }
        if (s == SND_CHUCAO) { /* (v40) «¡chu-cao!»: dos silbidos fuertes y redondos, el segundo más grave, repetidos */
            for (int r = 0; r < 2; r++) {
                const float t0 = (float)r * .55f;
                pio(&m, 1650.0f, 1500.0f, .11f, .02f, .30f, t0);
                pio(&m, 1180.0f, 980.0f, .20f, .03f, .32f, t0 + .15f);
            }
            break;
        }
        if (s == SND_CONDOR) { /* (v40) el viento bajo las alas grandes y un graznido ronco, bajito */
            ruido(&m, 1.3f, 420, .14f, 0); ruido(&m, .9f, 700, .07f, .3f);
            tono(&m, 180.0f, 150.0f, .35f, ONDA_TRIANGULO, .10f, .9f); ruido(&m, .3f, 1200, .04f, .9f);
            break;
        }
        if (s == SND_DRAGON_TARAREA) { /* (v38) el dragón: un zumbido grave, redondo, que sube y baja */
            tono(&m, 98.0f, 110.0f, 1.1f, ONDA_TRIANGULO, .34f, 0); tono(&m, 196.0f, 220.0f, 1.1f, ONDA_SENO, .14f, 0);
            tono(&m, 110.0f, 92.0f, .9f, ONDA_TRIANGULO, .28f, 1.0f); tono(&m, 330.0f, 330.0f, .5f, ONDA_SENO, .05f, .3f);
            ruido(&m, 1.6f, 260, .05f, 0);
            break;
        }
        if (s >= SND_TARAREO_0 && s <= SND_TARAREO_7) { voz_frase(&m, (int)(s - SND_TARAREO_0), true); break; } /* tararea con la boca cerrada */
        return 0;
    }
    return m.fin;
}

/* ======================================================================
 * Sonido ambiente: un bucle que empalma sin cortes (el final se funde con el comienzo).
 * Pensado para un parlante chico: nada de graves profundos; la lluvia, los grillos y
 * el viento viven en los medios y agudos, que es lo que ese parlante sí da.
 * ==================================================================== */
typedef struct { float b0, b1, b2, a1, a2, x1, x2, y1, y2; } bq_t;

static void bq_banda(bq_t *f, float frec, float q, int fs)
{
    float w = 6.2831853f * frec / (float)fs, alfa = sinf(w) / (2 * q), c = cosf(w), a0 = 1 + alfa;
    f->b0 = alfa / a0; f->b1 = 0; f->b2 = -alfa / a0; f->a1 = -2 * c / a0; f->a2 = (1 - alfa) / a0;
    f->x1 = f->x2 = f->y1 = f->y2 = 0;
}
static void bq_altos(bq_t *f, float frec, int fs)
{
    float w = 6.2831853f * frec / (float)fs, alfa = sinf(w) / (2 * 0.707f), c = cosf(w), a0 = 1 + alfa;
    f->b0 = (1 + c) / 2 / a0; f->b1 = -(1 + c) / a0; f->b2 = (1 + c) / 2 / a0; f->a1 = -2 * c / a0; f->a2 = (1 - alfa) / a0;
    f->x1 = f->x2 = f->y1 = f->y2 = 0;
}
static float bq(bq_t *f, float x)
{
    float y = f->b0 * x + f->b1 * f->x1 + f->b2 * f->x2 - f->a1 * f->y1 - f->a2 * f->y2;
    f->x2 = f->x1; f->x1 = x; f->y2 = f->y1; f->y1 = y;
    return y;
}

static float blanco(uint32_t *s)
{
    *s = *s * 1664525u + 1013904223u;
    return (float)(*s >> 8) / 8388608.0f - 1.0f;
}

static int16_t sat16(float v) { return (int16_t)(v > 32767.0f ? 32767 : (v < -32768.0f ? -32768 : v)); }
static void pone(int16_t *b, int i, float v) { b[i] = sat16(v * 32767.0f); }
static void suma(int16_t *b, int i, float v) { b[i] = sat16((float)b[i] + v * 32767.0f); }

/* ======================================================================
 * La voz de la mascota (y de los demás personajes): sílabas inventadas («la», «ba», «mu», «ni»...) y tarareos.
 * Una fuente de pulsos de voz (con vibrato, un resbalón de tono al empezar cada sílaba y un poco de aire) pasa por tres
 * resonancias (formantes) que dan la vocal; las resonancias van algo más arriba, como en una boquita chica y tierna.
 * Cada sonido es una frase de sílabas: consonante, vocal, nota (semitonos sobre el tono del personaje), duración y gesto.
 * ==================================================================== */
enum { V_A, V_O, V_U, V_E, V_I, V_M };
enum { G_NADA = 0, G_SUBE = 1, G_BAJA = 2, G_TEMBLOR = 4, G_AIRE = 8 }; /* cómo se mueve el tono y qué tan «soplada» es */
typedef struct { char c; uint8_t v; int8_t st; uint8_t cs; uint8_t g; } sil_t;
static const float FORM[6][3] = {{800, 1250, 2800}, {500, 900, 2600}, {340, 800, 2400}, {520, 1800, 2500}, {300, 2200, 2900}, {280, 1000, 2300}};
static const float FBASE[8] = {523.3f, 587.3f, 466.2f, 659.3f, 554.4f, 622.3f, 493.9f, 698.5f}; /* el tono de cada especie */

static uint32_t vz_az(uint32_t *s) { *s = *s * 1664525u + 1013904223u; return *s >> 8; }

/* v89: la consonante ya no es solo un golpe de ruido: cada una tiene sus propias resonancias al empezar y la boca "viaja"
 * hacia la vocal (así el oído oye "la", "mi", "yu", "wa" y no un pitido que se enciende). La m y la n tienen un murmullo nasal
 * antes de abrir; la r, un toquecito. Además la voz tiembla apenas (cada vibración de las cuerdas es un poquito distinta),
 * que es lo que la hace sonar viva y no electrónica. */
typedef struct { char c; float f1, f2, f3, ms, amp, nasal_ms; } cons_t;
static const cons_t CONS_F[] = {
    {'l', 360, 1150, 2600, 55, 0.55f, 0}, {'m', 280, 950, 2300, 45, 0.6f, 55}, {'n', 280, 1500, 2500, 45, 0.6f, 50},
    {'y', 280, 2250, 3000, 50, 0.6f, 0},  {'w', 310, 680, 2250, 60, 0.5f, 0},  {'r', 420, 1350, 1700, 30, 0.55f, 0},
    {'b', 300, 850, 2200, 30, 0.35f, 0},  {'p', 300, 850, 2200, 25, 0.25f, 0}, {'d', 320, 1750, 2700, 30, 0.35f, 0},
    {'t', 320, 1800, 2800, 25, 0.25f, 0}, {'g', 300, 2000, 2400, 32, 0.35f, 0}, {'h', 0, 0, 0, 0, 1.0f, 0},
};
static void bq_banda_coef(bq_t *f, float frec, float q, int fs) /* cambia la resonancia sin borrar lo que venía sonando */
{
    float w = 6.2831853f * frec / (float)fs, alfa = sinf(w) / (2 * q), c = cosf(w), a0 = 1 + alfa;
    f->b0 = alfa / a0; f->b1 = 0; f->b2 = -alfa / a0; f->a1 = -2 * c / a0; f->a2 = (1 - alfa) / a0;
}
static void voz_sil(mezcla_t *m, float *fase, float t0, const sil_t *sl, float base, float chico, float vol, bool ultima)
{
    if (sl->c == '_') return; /* un silencio */
    const int fs = m->fs;
    const float f0 = base * powf(2.0f, (float)sl->st / 12.0f), dur = (float)sl->cs * 0.01f;
    const float *F = FORM[sl->v];
    bq_t r[3];
    const float Q[3] = {7.0f, 9.0f, 11.0f}, G[3] = {1.0f, 0.7f, 0.22f}; /* el tercer formante bajito: voz redonda, no zumbona */
    float lp = 0.0f;
    const char c = sl->c;
    const cons_t *cn = NULL;
    for (unsigned k = 0; k < sizeof CONS_F / sizeof CONS_F[0]; k++) if (CONS_F[k].c == c) cn = &CONS_F[k];
    const bool viaja = cn && cn->ms > 0;
    float tn = cn ? cn->nasal_ms * 0.001f : 0.0f; /* murmullo nasal: va DENTRO de la duración de la sílaba (no corre el ritmo) */
    if (tn > dur * 0.4f) tn = dur * 0.4f;
    const float dv = dur - tn;                     /* lo que dura la parte con la boca abierta */
    float tv = viaja ? cn->ms * 0.001f : 0.0f;     /* el viaje de la consonante a la vocal */
    if (tv > dv * 0.6f) tv = dv * 0.6f;
    const float D0[3] = {viaja ? cn->f1 : F[0], viaja ? cn->f2 : F[1], viaja ? cn->f3 : F[2]};
    for (int j = 0; j < 3; j++) bq_banda(&r[j], D0[j] * chico, Q[j], fs);
    const bool blanda = c == 'l' || c == 'm' || c == 'n' || c == 'y' || c == 'w' || c == 'r';
    const float suave = blanda ? 0.035f : (c == 'h' ? 0.035f : (c ? 0.015f : 0.03f));
    const bool aire = (sl->g & G_AIRE) || c == 'h';
    const int a = (int)(t0 * (float)fs), n = (int)(dur * (float)fs);
    float jit = 0.0f, jit_obj = 0.0f, fr_ant = 0.0f; /* temblor natural: cambia un poquito en cada vibración */
    for (int i = 0; i < n && a + i < m->n; i++) {
        const float tt = (float)i / (float)fs;          /* desde el comienzo (con el murmullo) */
        const bool murmullo = tt < tn;                  /* la m / n: boca cerrada, suena por la nariz */
        const float t = murmullo ? 0.0f : tt - tn, kk = t / dv;
        if ((i & 31) == 0 && (viaja || murmullo)) {     /* la boca viaja de la consonante a la vocal */
            float u = murmullo ? 0.0f : (tv > 0 ? t / tv : 1.0f);
            if (u > 1.0f) u = 1.0f;
            u = u * u * (3.0f - 2.0f * u);
            const float N[3] = {260.0f, c == 'm' ? 900.0f : 1450.0f, 2300.0f};
            for (int j = 0; j < 3; j++) {
                const float de = murmullo ? N[j] : D0[j];
                bq_banda_coef(&r[j], (de + (F[j] - de) * u) * chico, Q[j], fs);
            }
        }
        const float entra = 1.0f - 0.07f * (ultima ? 0.0f : 1.0f) * expf(-t * 38.0f);
        float cae = 1.0f;
        if (sl->g & G_SUBE) cae = powf(2.0f, 5.0f * kk / 12.0f);
        else if (sl->g & G_BAJA) cae = powf(2.0f, -5.0f * kk / 12.0f);
        else cae = ultima ? 1.0f - 0.05f * kk * kk : 1.0f + 0.012f * kk;
        const float vp = (sl->g & G_TEMBLOR) ? 0.035f : 0.018f;
        const float vib = vp * sinf(6.2831853f * ((sl->g & G_TEMBLOR) ? 8.0f : 6.0f) * t) * (kk > 0.2f ? 1.0f : kk * 5.0f);
        jit += (jit_obj - jit) * 0.02f;
        const float f = f0 * entra * cae * (1.0f + vib + jit) * (murmullo ? 0.97f : 1.0f);
        *fase += f / (float)fs;
        const float fr = *fase - floorf(*fase);
        if (fr < fr_ant) { /* empezó una vibración nueva: un temblor distinto (±0,6 %) */
            m->semilla = m->semilla * 1664525u + 1013904223u;
            jit_obj = ((float)(m->semilla >> 8) / 8388608.0f - 1.0f) * 0.006f;
        }
        fr_ant = fr;
        float src = (1.0f - fr) * (1.0f - fr) * 2.0f - 0.55f;
        m->semilla = m->semilla * 1664525u + 1013904223u;
        const float ruido_b = (float)(m->semilla >> 8) / 8388608.0f - 1.0f;
        src = aire ? src * 0.45f + 0.55f * ruido_b : src + 0.05f * ruido_b; /* (un poquito más de aire: voz menos «de máquina») */
        lp += (murmullo ? 0.3f : 0.55f) * (src - lp); src = lp; /* suaviza los bordes del pulso (más en el murmullo) */
        float y = 0.0f;
        for (int j = 0; j < 3; j++) y += G[j] * bq(&r[j], src);
        float e;
        if (murmullo) e = (tt < 0.012f ? tt / 0.012f : 1.0f) * 0.45f;            /* mmm: más bajito que la vocal */
        else {
            e = t < suave ? t / suave : 1.0f;
            if (viaja && t < tv) e *= cn->amp + (1.0f - cn->amp) * (t / tv);       /* la consonante suena más suave que la vocal */
            if (c == 'r' && t > 0.008f && t < 0.022f) e *= 0.35f;                    /* el toquecito de la r */
            if (tn > 0 && t < 0.02f) e = e < 0.45f ? 0.45f : e;                       /* sin hueco entre el murmullo y la vocal */
        }
        const float cola = dv > 0.2f ? 0.07f : 0.03f;
        if (!murmullo && t > dv - cola) e *= (dv - t) / cola;
        if (e < 0) e = 0;
        mezclar(m, a + i, y * e * vol * 0.42f * (aire ? 1.8f : 1.0f));
    }
    const float tb = t0 + tn;
    if (c == 'b' || c == 'd' || c == 'p' || c == 't' || c == 'g') ruido(m, c == 'p' || c == 't' ? 0.014f : 0.008f, c == 'b' ? 900.0f : (c == 'd' ? 2200.0f : (c == 'g' ? 1500.0f : (c == 'p' ? 1200.0f : 2600.0f))), c == 'p' || c == 't' ? 0.022f : 0.016f, tb);
    if (a + n > m->fin) m->fin = a + n < m->n ? a + n : m->n;
}

/* una frase de sílabas con sus tonos (el tono base es el del personaje) */
static void voz_dice(mezcla_t *m, const sil_t *sl, int ns, float base, float chico, float vol)
{
    float t0 = 0.0f, fase = 0.0f;
    for (int k = 0; k < ns; k++) {
        voz_sil(m, &fase, t0, &sl[k], base, chico, vol, k == ns - 1);
        t0 += (float)sl[k].cs * 0.01f * 0.93f;
    }
}

/* deja el sonido a un volumen parejo (que ninguna sílaba suene mucho más fuerte que otra) */
static void voz_nivelar(mezcla_t *m, int objetivo)
{
    /* v89: se mide el volumen sin los picos sueltos (el 0,5 % más alto: el golpecito de una p, un arranque) y esos picos se redondean;
     * antes un solo pico al comienzo bajaba toda la sílaba a la mitad */
    static uint16_t hist[256];
    memset(hist, 0, sizeof hist);
    int total = 0;
    for (int i = 0; i < m->fin && i < m->n; i++) { int v = m->buf[i] < 0 ? -m->buf[i] : m->buf[i]; hist[v >> 7 > 255 ? 255 : v >> 7]++; total++; }
    int acum = 0, pico = 1;
    for (int k = 255; k >= 0; k--) { acum += hist[k]; if (acum * 200 >= total) { pico = (k + 1) << 7; break; } }
    const float g = (float)objetivo / (float)pico, lim = (float)objetivo * 1.1f;
    for (int i = 0; i < m->fin && i < m->n; i++) {
        float v = (float)m->buf[i] * g;
        const float av = v < 0 ? -v : v;
        if (av > lim) v = (v < 0 ? -1.0f : 1.0f) * (lim + (float)objetivo * 0.25f * tanhf((av - lim) / ((float)objetivo * 0.25f)));
        m->buf[i] = (int16_t)v;
    }
}

/* canto: cada especie tiene su tempo y su ritmo (el mismo en todas sus frases, 4 tiempos = 8 corcheas); las tres tonaditas
 * comparten el comienzo y cambian el final (como A A' B); el tarareo canta la tonada A con la boca cerrada, al mismo compás */
static const uint8_t CANTO_BPM[8] = {96, 104, 88, 112, 100, 108, 92, 116};
uint32_t sfx_frase_ms(int esp) { return 240000u / CANTO_BPM[esp & 7]; }
/* (v38) un «la» cantado en la nota de un cristal (una octava abajo: su voz) */
static void voz_la(mezcla_t *m, int k)
{
    static const float F[6] = {523.25f, 659.25f, 783.99f, 1046.5f, 587.33f, 880.0f};
    const sil_t la = {'l', V_A, 0, 34, G_NADA};
    voz_dice(m, &la, 1, F[k % 6] * 0.5f, 1.15f, 1.0f);
}
static void voz_frase(mezcla_t *m, int id, bool tarareo)
{
    static const uint8_t RIT[8][7] = {{4, 2, 2, 2, 2}, {4, 3, 1, 2, 2}, {3, 2, 2, 4}, {6, 1, 1, 2, 1, 1, 2}, {4, 3, 1, 3, 1}, {5, 2, 1, 1, 2, 2}, {3, 4, 2, 2}, {6, 1, 1, 1, 1, 2, 2}};
    static const int8_t PENTA[5] = {0, 2, 4, 7, 9};
    static const char CONS[6] = {'l', 'm', 'n', 'l', 'y', 'n'};
    static const uint8_t VOC[6] = {V_A, V_O, V_U, V_A, V_I, V_O};
    const int esp = tarareo ? id & 7 : (id / 3) & 7, var = tarareo ? 0 : id % 3;
    const int ns = RIT[esp][0];
    const float corch = 30.0f / (float)CANTO_BPM[esp] * 100.0f; /* centésimas por corchea */
    uint32_t sa = 7919u * (uint32_t)(esp + 1) + 15485863u;      /* primera mitad: igual en las tres tonaditas */
    uint32_t sb = sa * 31u + 977u * (uint32_t)(var + 1);       /* segunda mitad: cambia con la variante */
    int grado = (int)(vz_az(&sa) % 2u) * 2;
    sil_t sl[8];
    int cum = 0;
    for (int k = 0; k < ns; k++) {
        const bool ultima = k == ns - 1;
        uint32_t *q = k < (ns + 1) / 2 ? &sa : &sb;
        const uint32_t r = vz_az(q) % 10u;
        grado += r < 4 ? 1 : (r < 7 ? -1 : (r < 8 ? 1 : (r < 9 ? -1 : 0))); /* de a pasitos, a veces se repite */
        if (grado < -1) grado = -1;
        if (grado > 5) grado = 5;
        if (ultima) grado = var == 1 ? 3 : (var == 2 ? 1 : 0);
        const int oct = grado >= 5 ? 12 : (grado < 0 ? -12 : 0), gg = ((grado % 5) + 5) % 5;
        const int e0 = (int)((float)cum * corch + 0.5f);
        cum += RIT[esp][k + 1];
        const int e1 = (int)((float)cum * corch + 0.5f);
        const uint8_t cs = (uint8_t)((float)(e1 - e0) / 0.93f);
        const uint8_t vocal = tarareo ? V_M : VOC[vz_az(q) % 6u];
        char c = tarareo ? 0 : (k == 0 || vz_az(q) % 3u ? CONS[vz_az(q) % 6u] : 0);
        sl[k] = (sil_t){c, vocal, (int8_t)(PENTA[gg] + oct), cs, G_NADA};
    }
    voz_dice(m, sl, ns, FBASE[esp] * (tarareo ? 0.82f : 1.0f), 1.3f, 1.0f);
    voz_nivelar(m, tarareo ? 6000 : 7000);
}

/* los sonidos de la mascota: cada emoción y cada gesto, dicho con su voz */
#define S(c, v, st, cs, g) {c, v, st, cs, g}
static const sil_t P_ALEGRE[] = {S('p', V_A, 0, 12, 0), S('p', V_I, 7, 24, G_SUBE)};                                   /* pa-pii↑ */
static const sil_t P_RISA[] = {S('h', V_A, 4, 9, 0), S('h', V_A, 2, 9, 0), S('h', V_A, 5, 9, 0), S('h', V_A, 1, 12, 0)}; /* ja-ja-ja-ja */
static const sil_t P_RONRONEO[] = {S(0, V_M, -5, 55, G_TEMBLOR), S(0, V_M, -6, 55, G_TEMBLOR)};                          /* mmmm */
static const sil_t P_QUEJA[] = {S(0, V_U, 3, 14, 0), S(0, V_U, -2, 34, G_BAJA)};                                         /* uu-uuu↓ */
static const sil_t P_LLANTO[] = {S(0, V_U, 3, 18, G_BAJA | G_TEMBLOR), S(0, V_U, 3, 14, G_BAJA | G_TEMBLOR), S(0, V_U, 1, 28, G_BAJA | G_TEMBLOR)};
static const sil_t P_GRUNIDO[] = {S('d', V_U, -9, 16, 0), S('g', V_U, -11, 24, G_TEMBLOR)};                              /* dum-gru */
static const sil_t P_BOSTEZO[] = {S(0, V_A, 5, 32, G_BAJA), S(0, V_A, -1, 26, G_BAJA), S('m', V_U, -6, 22, G_BAJA)};     /* aah-aah-mu */
static const sil_t P_RONQUIDO[] = {S(0, V_U, -12, 48, G_AIRE | G_TEMBLOR), S(0, V_U, -14, 44, G_AIRE | G_TEMBLOR)};      /* zzz */
static const sil_t P_ESTORNUDO[] = {S(0, V_A, 2, 10, 0), S(0, V_A, 4, 10, 0), S('t', V_U, 10, 12, G_SUBE | G_AIRE)};     /* a-a-chú */
static const sil_t P_TOS[] = {S('h', V_E, -3, 9, G_AIRE), S('h', V_E, -5, 10, G_AIRE)};                                  /* je-je */
static const sil_t P_COMER[] = {S('n', V_A, -2, 10, 0), S('m', V_A, -4, 10, 0), S('n', V_A, -2, 10, 0), S('m', V_A, -4, 12, 0)}; /* ñam ñam */
static const sil_t P_SORPRESA[] = {S(0, V_U, 0, 10, 0), S(0, V_A, 9, 22, G_SUBE)};                                       /* ¿u-ah? */
static const sil_t P_SUSPIRO_AMOR[] = {S(0, V_A, 4, 18, 0), S(0, V_O, 7, 40, G_BAJA | G_AIRE)};                          /* ah-ohh~ */
static const sil_t P_PELEA[] = {S('g', V_E, 3, 10, 0), S('g', V_E, 3, 10, 0), S('g', V_A, 0, 14, G_BAJA)};               /* ge-ge-ga */
static const sil_t P_LLAMADO[] = {S('m', V_I, 7, 12, 0), S('m', V_I, 9, 24, G_SUBE)};                                    /* mi-mii↑ */
static const sil_t P_SUSPIRO[] = {S('h', V_A, 2, 44, G_BAJA | G_AIRE)};                                                   /* haaah */
static const sil_t P_ABRAZO[] = {S('m', V_A, 0, 30, 0), S(0, V_O, 4, 56, G_TEMBLOR)};                                    /* ma-ooo */
static const sil_t P_MAREO[] = {S('w', V_O, 0, 22, G_SUBE), S('w', V_O, 3, 22, G_BAJA), S('w', V_O, -2, 26, G_SUBE), S('w', V_O, 2, 30, G_BAJA)};
static const sil_t P_OLFATEO[] = {S('h', V_I, 5, 8, G_AIRE), S('h', V_I, 6, 8, G_AIRE), S('h', V_I, 7, 8, G_AIRE)};      /* ji-ji-ji (olfateo) */
static const sil_t P_HUELE[] = {S('h', V_I, 5, 8, G_AIRE), S(0, V_I, 10, 22, G_SUBE)};                                   /* ji-iii↑ */
static const sil_t P_DESLIZA[] = {S('w', V_I, 8, 26, G_BAJA), S('w', V_A, 3, 22, G_BAJA), S(0, V_I, 9, 16, G_SUBE)};     /* wiii-wa-i */
static const sil_t P_CAIDA[] = {S(0, V_U, 0, 8, 0), S('f', V_U, -4, 16, G_BAJA | G_AIRE)};                                /* u-uf */
static const sil_t P_PELUCHE[] = {S(0, V_I, 12, 16, G_SUBE), S(0, V_I, 14, 10, 0)};                                      /* iii! */
static const sil_t P_TOQUE[] = {S('b', V_U, 2, 12, 0)};                                                                    /* bu */
static const sil_t P_TRUCO[] = {S('t', V_A, 0, 10, 0), S('t', V_A, 4, 10, 0), S('r', V_A, 9, 30, G_SUBE)};               /* ta-ta-ráaa */
static const sil_t P_ENCONTRADA[] = {S('t', V_A, 0, 14, 0), S('d', V_A, 7, 32, 0)};                                       /* ta-dá */
static const sil_t P_ATRAPA[] = {S('y', V_E, 4, 10, 0), S(0, V_I, 9, 20, G_SUBE)};                                       /* ye-ii */
static const sil_t P_META[] = {S('y', V_I, 7, 14, 0), S('p', V_I, 12, 26, G_SUBE)};                                      /* yi-pii */
static const sil_t P_CHAPUZON[] = {S('w', V_U, 5, 14, G_SUBE), S('b', V_U, -2, 14, G_BAJA)};                              /* wu-bu */
static const sil_t P_CONTAR[] = {S('d', V_O, 0, 14, 0), S('d', V_O, 0, 14, 0), S('d', V_O, 4, 14, 0)};                   /* do-do-do */
/* Pochi, el chihuahua: voz muy aguda, ladridos cortos y secos */
static const sil_t P_POCHI_LADRA[] = {S('g', V_A, 5, 9, G_BAJA | G_AIRE), S('_', V_A, 0, 5, 0), S('g', V_A, 5, 9, G_BAJA | G_AIRE), S('_', V_A, 0, 5, 0), S('g', V_O, 3, 14, G_BAJA | G_AIRE)}; /* guau, guau, guaau: ladrido seco y áspero */
static const sil_t P_POCHI_PIDE[] = {S(0, V_I, 2, 22, G_SUBE | G_TEMBLOR), S('y', V_A, 5, 9, 0), S('y', V_A, 5, 18, G_BAJA)};  /* iii-ya-yaa */
static const sil_t P_POCHI_COME[] = {S('n', V_A, 0, 8, 0), S('m', V_A, -2, 8, 0), S('n', V_A, 0, 8, 0), S('m', V_A, -2, 8, 0), S('y', V_A, 7, 8, 0)};
static const sil_t P_POCHI_FELIZ[] = {S('y', V_A, 0, 6, 0), S('y', V_A, 3, 6, 0), S('y', V_A, 5, 6, 0), S('y', V_A, 7, 6, 0), S('y', V_I, 12, 18, G_SUBE)};
static const sil_t P_POCHI_ADIOS[] = {S(0, V_I, 7, 22, G_BAJA | G_TEMBLOR), S(0, V_U, 3, 36, G_BAJA | G_TEMBLOR)};              /* iiu-uu */
static const sil_t P_HOLA[] = {S('h', V_O, 4, 14, 0), S('l', V_A, 9, 26, G_SUBE)};                                        /* ho-la↑ */

static const struct { sound_t id; const sil_t *sl; uint8_t n; float base, chico; } PATRONES[] = {
#define PAT(id, arr, base, chico) {id, arr, (uint8_t)(sizeof arr / sizeof arr[0]), base, chico}
    PAT(SND_ALEGRE, P_ALEGRE, 620, 1.22f), PAT(SND_RISA, P_RISA, 640, 1.22f), PAT(SND_RONRONEO, P_RONRONEO, 560, 1.15f),
    PAT(SND_QUEJA, P_QUEJA, 600, 1.2f), PAT(SND_LLANTO, P_LLANTO, 640, 1.22f), PAT(SND_GRUNIDO, P_GRUNIDO, 600, 1.1f),
    PAT(SND_BOSTEZO, P_BOSTEZO, 600, 1.2f), PAT(SND_RONQUIDO, P_RONQUIDO, 600, 1.1f), PAT(SND_ESTORNUDO, P_ESTORNUDO, 600, 1.2f),
    PAT(SND_TOS, P_TOS, 600, 1.15f), PAT(SND_COMER, P_COMER, 600, 1.2f), PAT(SND_SORPRESA, P_SORPRESA, 600, 1.22f),
    PAT(SND_SUSPIRO_AMOR, P_SUSPIRO_AMOR, 600, 1.2f), PAT(SND_PELEA, P_PELEA, 600, 1.15f), PAT(SND_LLAMADO, P_LLAMADO, 620, 1.22f),
    PAT(SND_SUSPIRO, P_SUSPIRO, 600, 1.2f), PAT(SND_ABRAZO, P_ABRAZO, 580, 1.2f), PAT(SND_MAREO, P_MAREO, 620, 1.22f),
    PAT(SND_OLFATEO, P_OLFATEO, 620, 1.2f), PAT(SND_HUELE, P_HUELE, 620, 1.2f), PAT(SND_DESLIZA, P_DESLIZA, 620, 1.22f),
    PAT(SND_CAIDA, P_CAIDA, 580, 1.15f), PAT(SND_PELUCHE, P_PELUCHE, 620, 1.25f), PAT(SND_TOQUE, P_TOQUE, 620, 1.2f),
    PAT(SND_TRUCO, P_TRUCO, 600, 1.2f), PAT(SND_ENCONTRADA, P_ENCONTRADA, 600, 1.2f), PAT(SND_ATRAPA, P_ATRAPA, 620, 1.22f),
    PAT(SND_META, P_META, 620, 1.22f), PAT(SND_CHAPUZON, P_CHAPUZON, 600, 1.2f), PAT(SND_CONTAR, P_CONTAR, 600, 1.15f),
    PAT(SND_HOLA_AMIGO, P_HOLA, 520, 1.1f),
    PAT(SND_POCHI_LADRA, P_POCHI_LADRA, 980, 1.38f), PAT(SND_POCHI_PIDE, P_POCHI_PIDE, 940, 1.38f), PAT(SND_POCHI_COME, P_POCHI_COME, 900, 1.35f),
    PAT(SND_POCHI_FELIZ, P_POCHI_FELIZ, 960, 1.38f), PAT(SND_POCHI_ADIOS, P_POCHI_ADIOS, 900, 1.35f),
#undef PAT
};

static bool voz_patron(mezcla_t *m, sound_t s)
{
    for (size_t k = 0; k < sizeof PATRONES / sizeof PATRONES[0]; k++)
        if (PATRONES[k].id == s) {
            voz_dice(m, PATRONES[k].sl, PATRONES[k].n, PATRONES[k].base, PATRONES[k].chico, 1.0f);
            voz_nivelar(m, s == SND_RONQUIDO || s == SND_SUSPIRO || s == SND_TOS || s == SND_OLFATEO ? 5000 : 8500); /* los soplados, más suaves */
            return true;
        }
    return false;
}
#undef S

/* un pajarito: dos o tres notas que suben y bajan */
static void pajarito(int16_t *b, int n, int fs, int ini, float f0, float vol, uint32_t *sem)
{
    int notas = 2 + (int)((*sem >> 9) % 3u);
    for (int k = 0; k < notas; k++) {
        int a = ini + k * fs / 9, d = fs / 14;
        float fa = f0 * (1.0f + 0.15f * (float)(k % 2)), fb = fa * 1.25f, fase = 0;
        for (int i = 0; i < d && a + i < n; i++) {
            float t = (float)i / (float)d;
            fase += (fa + (fb - fa) * t) / (float)fs;
            suma(b, a + i, sinf(6.2831853f * fase) * sinf(3.14159f * t) * vol);
        }
    }
    blanco(sem);
}

/* una gota: un "plic" que cae de tono */
static void gota(int16_t *b, int n, int fs, int ini, float f, float vol)
{
    int d = fs / 12;
    float fase = 0;
    for (int i = 0; i < d && ini + i < n; i++) {
        float t = (float)i / (float)d;
        fase += f * (1.0f - 0.35f * t) / (float)fs;
        suma(b, ini + i, sinf(6.2831853f * fase) * expf(-7.0f * t) * vol);
    }
}

int sfx_ambient(ambiente_t a, int16_t *out, int max_muestras, int fs)
{
    if (a == AMB_NADA || a >= AMB_COUNT || !out) return 0;
    const int L = (int)(SFX_AMB_SEGUNDOS * (float)fs), F = fs / 2; /* F: lo que se funde al empalmar */
    if (max_muestras < L + F) return 0;
    /* se genera L + F muestras directo en la salida (sin memoria extra) */
    int16_t *b = out;
    int n = L + F;
    memset(b, 0, (size_t)n * sizeof(int16_t));
    uint32_t sem = 777u + (uint32_t)a * 7919u;
    bq_t f1 = {0}, f2 = {0};
    switch (a) {
    case AMB_PRADO: /* brisa suave y pajaritos */
        bq_banda(&f1, 900, 0.6f, fs);
        for (int i = 0; i < n; i++) pone(b, i, bq(&f1, blanco(&sem)) * (0.05f + 0.03f * sinf(6.2831853f * (float)i / (float)n * 2)));
        for (int k = 0; k < 4; k++) pajarito(b, n, fs, F + (int)((float)(L - F) * ((float)k + 0.2f + 0.5f * (float)((sem >> (k * 3)) & 7u) / 8.0f) / 4.0f), 2600.0f + 500.0f * (float)(k % 3), 0.10f, &sem);
        break;
    case AMB_BOSQUE: /* hojas que se mueven y un par de pájaros más lejos */
        bq_banda(&f1, 2200, 0.8f, fs);
        for (int i = 0; i < n; i++) {
            float m = 0.5f + 0.5f * sinf(6.2831853f * (float)i / (float)fs * 0.35f) * sinf(6.2831853f * (float)i / (float)fs * 0.13f);
            pone(b, i, bq(&f1, blanco(&sem)) * 0.06f * (0.4f + m));
        }
        pajarito(b, n, fs, F + L / 5, 1900.0f, 0.08f, &sem);
        pajarito(b, n, fs, F + L * 3 / 5, 2300.0f, 0.06f, &sem);
        break;
    case AMB_LAGO: /* el agua que va y viene contra la orilla */
        bq_banda(&f1, 700, 0.7f, fs); bq_banda(&f2, 2400, 1.0f, fs);
        for (int i = 0; i < n; i++) {
            float ola = 0.35f + 0.65f * (0.5f + 0.5f * sinf(6.2831853f * (float)i / (float)fs * 0.25f));
            float w = blanco(&sem);
            pone(b, i, bq(&f1, w) * 0.11f * ola + bq(&f2, w) * 0.04f * ola * ola);
        }
        gota(b, n, fs, F + L / 3, 900.0f, 0.12f);
        break;
    case AMB_CUEVA: /* gotas con eco y un rumor apagado */
        bq_banda(&f1, 350, 0.9f, fs);
        for (int i = 0; i < n; i++) pone(b, i, bq(&f1, blanco(&sem)) * 0.03f);
        for (int k = 0; k < 6; k++) {
            int ini = F + (int)((float)(L - F) * ((float)k + 0.3f) / 6.0f) + (int)((sem >> (k * 4)) % 2000u);
            float f = 1300.0f + 250.0f * (float)((sem >> (k * 2)) & 3u);
            gota(b, n, fs, ini, f, 0.16f);
            gota(b, n, fs, ini + fs / 4, f, 0.06f); /* eco */
            gota(b, n, fs, ini + fs / 2, f, 0.025f);
        }
        break;
    case AMB_NOCHE: { /* un coro de grillos medido de una grabación real (varios tonos a la vez, cantos de pulsos rápidos, unas 2 veces
                       * por segundo, siseo de fondo), pero con lo imperfecto de la vida real: cada pulso un poco distinto */
#define RND() (blanco(&sem) * 0.5f + 0.5f)
        static const float FC[6] = {4350.0f, 3620.0f, 3000.0f, 2240.0f, 4550.0f, 3300.0f}; /* el tono de cada grillo */
        static const float PER[6] = {0.47f, 0.52f, 0.45f, 0.50f, 0.62f, 0.58f};          /* cada cuánto canta */
        static const float SEP[6] = {0.021f, 0.022f, 0.032f, 0.023f, 0.020f, 0.028f};    /* tiempo entre pulsos */
        static const int NP[6] = {9, 7, 5, 4, 8, 5};                                      /* pulsos por canto (±1) */
        static const float VOLG[6] = {0.040f, 0.040f, 0.034f, 0.040f, 0.020f, 0.022f};
        const float dur = (float)n / (float)fs;
        bq_banda(&f1, 3600, 0.6f, fs);
        for (int i = 0; i < n; i++) {
            const float t = (float)i / (float)fs;
            suma(b, i, bq(&f1, blanco(&sem)) * 0.008f * (0.7f + 0.3f * sinf(6.2831853f * t * 0.31f)));
        }
        for (int g = 0; g < 6; g++) {
            float t0 = 0.09f * (float)g + 0.25f * RND();
            float deriva = 0.0f; /* el ritmo de cada grillo se acelera y se frena un poco con el tiempo (la temperatura, el ánimo) */
            while (t0 < dur - 0.3f) {
                const int pulsos = NP[g] - 2 + (int)(RND() * 4.0f);
                const float sep0 = SEP[g] * (0.9f + 0.2f * RND());
                const float fcg = FC[g] * (1.0f + 0.03f * (RND() - 0.5f));
                const float amp = VOLG[g] * (0.5f + 0.5f * RND());
                float tp = t0;
                for (int s2 = 0; s2 < pulsos; s2++) {
                    const int a0 = (int)(tp * (float)fs), d = (int)(0.022f * (float)fs);
                    const float cre = (0.55f + 0.45f * (float)s2 / (float)pulsos) * (0.75f + 0.25f * RND()); /* cada pulso, distinto */
                    const float fp = fcg * (1.0f + 0.012f * (RND() - 0.5f));
                    float fase = 0.0f;
                    for (int i = 0; i < d && a0 + i < n; i++) {
                        const float t = (float)i / (float)fs;
                        const float e2 = (1.0f - expf(-t * 1000.0f)) * expf(-t * 130.0f);
                        fase += fp * (1.0f - 0.04f * t * 40.0f) / (float)fs; /* el tono baja un pelo en cada pulso */
                        suma(b, a0 + i, (sinf(6.2831853f * fase) + 0.12f * sinf(12.5663706f * fase)) * e2 * amp * cre);
                    }
                    tp += sep0 * (0.88f + 0.24f * RND()); /* el golpecito del ala no es de reloj */
                }
                deriva += 0.04f * (RND() - 0.5f);
                if (deriva > 0.12f) deriva = 0.12f;
                if (deriva < -0.12f) deriva = -0.12f;
                t0 += PER[g] * (1.0f + deriva) * (0.85f + 0.3f * RND());
                if (RND() < 0.08f) t0 += 0.4f + 0.7f * RND();
            }
        }
#undef RND
        break;
    }
    case AMB_LLUVIA: case AMB_TORMENTA: { /* lluvia pareja y gotas sueltas; la tormenta, más fuerte y con viento */
        bool fuerte = a == AMB_TORMENTA;
        bq_altos(&f1, 1200, fs); bq_banda(&f2, 3200, 0.5f, fs);
        for (int i = 0; i < n; i++) {
            float w = blanco(&sem);
            pone(b, i, bq(&f1, w) * (fuerte ? 0.10f : 0.065f) + bq(&f2, w) * 0.03f);
        }
        for (int k = 0; k < (fuerte ? 40 : 24); k++) {
            int ini = (int)(blanco(&sem) * 0.5f * (float)n + 0.5f * (float)n);
            if (ini < 0) ini = 0;
            gota(b, n, fs, ini, 2500.0f + 1500.0f * (blanco(&sem) * 0.5f + 0.5f), 0.05f);
        }
        if (fuerte) { /* y viento: los truenos de verdad los pone el clima, no el bucle */
            bq_banda(&f1, 700, 1.5f, fs);
            for (int i = 0; i < n; i++) {
                float t = (float)i / (float)fs, m = 0.5f + 0.5f * sinf(6.2831853f * t * 0.25f);
                suma(b, i, bq(&f1, blanco(&sem)) * 0.06f * m);
            }
        }
        break;
    }
    case AMB_VIENTO: { /* ráfagas que suben y bajan de tono */
        float fase = 0;
        for (int i = 0; i < n; i++) {
            float t = (float)i / (float)fs;
            float m = 0.5f + 0.5f * sinf(6.2831853f * t * 0.18f) * sinf(6.2831853f * t * 0.07f + 1.0f);
            if ((i & 63) == 0) { bq_banda(&f1, 500.0f + 900.0f * m, 2.5f, fs); f1.x1 = f1.x2 = 0; }
            fase += 1;
            pone(b, i, bq(&f1, blanco(&sem)) * 0.16f * (0.25f + m));
        }
        (void)fase;
        break;
    }
    case AMB_MAR: { /* la isla: olas grandes que llegan y se retiran, con su espuma, y una gaviota lejos */
        bq_banda(&f1, 420, 0.6f, fs); bq_banda(&f2, 3200, 0.9f, fs);
        for (int i = 0; i < n; i++) {
            float u = (float)(i % L) / (float)L * 3.0f; /* tres olas por vuelta: empalma justo */
            float fr = u - floorf(u);
            float ola = fr < 0.35f ? fr / 0.35f : expf(-(fr - 0.35f) * 4.5f); /* sube y rompe despacio */
            float w = blanco(&sem);
            pone(b, i, bq(&f1, w) * (0.05f + 0.13f * ola) + bq(&f2, w) * 0.05f * ola * ola);
        }
        pajarito(b, n, fs, F + L / 2, 1500.0f, 0.05f, &sem);
        break;
    }
    default: break;
    }
    /* empalme: el último medio segundo se funde con el primero */
    for (int i = 0; i < F && L + i < n; i++) {
        float k = (float)i / (float)F;
        b[i] = sat16((float)b[i] * k + (float)b[L + i] * (1.0f - k));
    }
    return L;
}
