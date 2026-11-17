/*
 * Catálogo de sonidos: genera docs/sonidos.md, docs/sonidos.json y un .wav de
 * cada sonido (docs/sonidos/), a partir de PET_SONIDOS en core/pet.h.
 * Es la lista que usará el editor de Windows para reemplazar sonidos.
 *
 *   ./mascota_sonidos docs
 */
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "pet.h"
#include "sfx.h"

#define HZ 22050
static int16_t pcm[200000]; /* más que SFX_MAX_SEGUNDOS a 22 050 Hz */

static void u32(FILE *f, uint32_t v) { fputc(v & 255, f); fputc(v >> 8 & 255, f); fputc(v >> 16 & 255, f); fputc(v >> 24 & 255, f); }
static void u16(FILE *f, uint16_t v) { fputc(v & 255, f); fputc(v >> 8 & 255, f); }

static void wav_hz(const char *ruta, const int16_t *s, int n, int hz)
{
    FILE *f = fopen(ruta, "wb");
    if (!f) { perror(ruta); return; }
    fwrite("RIFF", 1, 4, f); u32(f, 36 + (uint32_t)n * 2); fwrite("WAVEfmt ", 1, 8, f);
    u32(f, 16); u16(f, 1); u16(f, 1); u32(f, (uint32_t)hz); u32(f, (uint32_t)hz * 2); u16(f, 2); u16(f, 16);
    fwrite("data", 1, 4, f); u32(f, (uint32_t)n * 2);
    for (int i = 0; i < n; i++) u16(f, (uint16_t)s[i]);
    fclose(f);
}

static void wav(const char *ruta, const int16_t *s, int n) { wav_hz(ruta, s, n, HZ); }

static void json_str(FILE *f, const char *s)
{
    fputc('"', f);
    for (; *s; s++) {
        if (*s == '"' || *s == '\\') fputc('\\', f);
        fputc(*s, f);
    }
    fputc('"', f);
}

static const char *minus(const char *id, char *buf, size_t n)
{
    /* SND_HOLA_AMIGO -> hola_amigo */
    const char *s = strncmp(id, "SND_", 4) ? id : id + 4;
    size_t i = 0;
    for (; s[i] && i + 1 < n; i++) buf[i] = (char)(s[i] >= 'A' && s[i] <= 'Z' ? s[i] + 32 : s[i]);
    buf[i] = 0;
    return buf;
}

int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : ".";
    char ruta[512], nombre[64];
    snprintf(ruta, sizeof ruta, "%s/sonidos", dir);
    mkdir(ruta, 0755);
    snprintf(ruta, sizeof ruta, "%s/sonidos.md", dir);
    FILE *md = fopen(ruta, "w");
    snprintf(ruta, sizeof ruta, "%s/sonidos.json", dir);
    FILE *js = fopen(ruta, "w");
    if (!md || !js) { perror(ruta); return 1; }
    fprintf(md, "# Catálogo de sonidos\n\nTodos los sonidos se sintetizan en el reloj (`core/sfx.c`), sin archivos de audio. "
                "Se generan desde `PET_SONIDOS` en `core/pet.h`; esta lista y `sonidos.json` son la referencia para el "
                "editor de Windows, que podrá reemplazar cada uno por un sonido propio (mono, 16 bits, hasta %.1f s). "
                "En `docs/sonidos/` hay un `.wav` de cada uno tal como suena hoy.\n",
            SFX_MAX_SEGUNDOS);
    fprintf(js, "{\n  \"version\": 1,\n  \"frecuencia_hz\": %d,\n  \"max_segundos\": %.1f,\n  \"sonidos\": [\n", HZ, SFX_MAX_SEGUNDOS);
    static const char *const GRUPOS[] = {"animo", "cuerpo", "accion", "juego", "mundo", "musica", "alarma", "viaje"};
    static const char *const TITULOS[] = {"Ánimo (cómo se siente)", "Cuerpo", "Lo que hace", "Juegos y trucos", "El mundo", "Música", "Alarmas", "El viaje de despedida"};
    int total = 0, primero = 1;
    for (int g = 0; g < 8; g++) {
        fprintf(md, "\n## %s\n\n| # | Sonido | Nombre | Qué significa | Cuándo suena | Duración | Archivo |\n|---|---|---|---|---|---|---|\n", TITULOS[g]);
        for (int s = 1; s < SND_COUNT; s++) {
            const pet_sonido_info_t *i = pet_sound_info((sound_t)s);
            if (strcmp(i->grupo, GRUPOS[g])) continue;
            int n = sfx_render((sound_t)s, pcm, (int)(sizeof pcm / 2), HZ);
            minus(i->id, nombre, sizeof nombre);
            snprintf(ruta, sizeof ruta, "%s/sonidos/%s.wav", dir, nombre);
            wav(ruta, pcm, n);
            fprintf(md, "| %d | `%s` | %s | %s | %s | %.2f s | `sonidos/%s.wav` |\n", s, i->id, i->nombre, i->significado, i->cuando,
                    (double)n / HZ, nombre);
            fprintf(js, "%s    {\"n\": %d, \"id\": ", primero ? "" : ",\n", s);
            json_str(js, i->id);
            fprintf(js, ", \"grupo\": ");
            json_str(js, i->grupo);
            fprintf(js, ", \"nombre\": ");
            json_str(js, i->nombre);
            fprintf(js, ", \"significado\": ");
            json_str(js, i->significado);
            fprintf(js, ", \"cuando\": ");
            json_str(js, i->cuando);
            fprintf(js, ", \"segundos\": %.2f, \"archivo\": \"sonidos/%s.wav\"}", (double)n / HZ, nombre);
            primero = 0;
            total++;
        }
    }
    /* sonido ambiente: bucles que se repiten de fondo */
    static const struct { ambiente_t a; const char *id, *nombre, *cuando; } AMB[] = {
        {AMB_PRADO, "AMB_PRADO", "Prado", "De día en el prado: brisa y pajaritos"},
        {AMB_BOSQUE, "AMB_BOSQUE", "Bosque", "De día en el bosque: hojas y pájaros lejanos"},
        {AMB_LAGO, "AMB_LAGO", "Lago", "De día en el lago: el agua contra la orilla"},
        {AMB_CUEVA, "AMB_CUEVA", "Cueva", "Siempre en la cueva: gotas con eco"},
        {AMB_NOCHE, "AMB_NOCHE", "Grillos", "De noche (salvo en invierno o con frío)"},
        {AMB_LLUVIA, "AMB_LLUVIA", "Lluvia", "Cuando llueve"},
        {AMB_TORMENTA, "AMB_TORMENTA", "Tormenta", "Con tormenta: lluvia fuerte y viento (los truenos suenan aparte)"},
        {AMB_VIENTO, "AMB_VIENTO", "Viento", "Con viento, y de día con frío o nieve"},
        {AMB_MAR, "AMB_MAR", "Mar", "De día en la isla: olas que llegan y se retiran, y una gaviota"},
    };
    static int16_t amb[8 * 16000 + 9600];
    fprintf(md, "\n## Sonido ambiente (bucles de fondo)\n\nSuenan bajito de fondo mientras la pantalla está encendida (en Ajustes: "
                "apagado, bajito o normal). Cada uno es un bucle de %.0f s que empalma sin cortes. El parlante del reloj es chico: "
                "se oyen bien la lluvia, los grillos y el viento, pero sin graves. Para reemplazarlos: mono, 16 bits, 16 000 Hz, "
                "y que el final empalme con el comienzo.\n\n| # | Sonido | Nombre | Cuándo suena | Archivo |\n|---|---|---|---|---|\n",
            SFX_AMB_SEGUNDOS);
    fprintf(js, "\n  ],\n  \"ambiente_hz\": 16000,\n  \"ambiente_segundos\": %.1f,\n  \"ambientes\": [\n", SFX_AMB_SEGUNDOS);
    for (unsigned k = 0; k < sizeof AMB / sizeof AMB[0]; k++) {
        int n = sfx_ambient(AMB[k].a, amb, (int)(sizeof amb / 2), 16000);
        minus(AMB[k].id, nombre, sizeof nombre);
        snprintf(ruta, sizeof ruta, "%s/sonidos/%s.wav", dir, nombre);
        wav_hz(ruta, amb, n, 16000);
        fprintf(md, "| %d | `%s` | %s | %s | `sonidos/%s.wav` |\n", (int)AMB[k].a, AMB[k].id, AMB[k].nombre, AMB[k].cuando, nombre);
        fprintf(js, "%s    {\"n\": %d, \"id\": \"%s\", \"nombre\": ", k ? ",\n" : "", (int)AMB[k].a, AMB[k].id);
        json_str(js, AMB[k].nombre);
        fprintf(js, ", \"cuando\": ");
        json_str(js, AMB[k].cuando);
        fprintf(js, ", \"archivo\": \"sonidos/%s.wav\"}", nombre);
    }
    fprintf(js, "\n  ]\n}\n");
    fclose(md);
    fclose(js);
    printf("%d sonidos y 8 ambientes\n", total);
    return 0;
}
