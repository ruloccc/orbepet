/*
 * Datos de fábrica para el editor de Windows: los píxeles de cada ícono, cada sonido y cada
 * ambiente tal como suenan en el reloj, y las celebraciones. Escribe editor/datos.js.
 *
 *   ./mascota_editor_datos ../editor
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "art.h"
#include "lvgl.h"
#include "pet.h"
#include "sfx.h"

#define HZ 16000

static void js_str(FILE *f, const char *s)
{
    fputc('"', f);
    for (; *s; s++) {
        if (*s == '"' || *s == '\\') fputc('\\', f);
        if (*s == '\n') { fputs("\\n", f); continue; }
        fputc(*s, f);
    }
    fputc('"', f);
}

static void js_b64(FILE *f, const uint8_t *d, int n)
{
    int cap = 4 * ((n + 2) / 3) + 1;
    char *b = malloc((size_t)cap);
    pet_base64_encode(d, n, b, cap);
    fprintf(f, "\"%s\"", b);
    free(b);
}

static void rgba(const uint32_t *px, uint8_t *o)
{
    for (int i = 0; i < ART_ICONO_LADO * ART_ICONO_LADO; i++) {
        uint32_t c = px[i];
        o[i * 4] = (uint8_t)(c >> 16); o[i * 4 + 1] = (uint8_t)(c >> 8); o[i * 4 + 2] = (uint8_t)c; o[i * 4 + 3] = (uint8_t)(c >> 24);
    }
}

int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : ".";
    char ruta[512];
    snprintf(ruta, sizeof ruta, "%s/datos.js", dir);
    FILE *f = fopen(ruta, "w");
    if (!f) { perror(ruta); return 1; }
    lv_init();
    fprintf(f, "/* generado por mascota_editor_datos: no editar a mano */\nconst DATOS = {\n");
    fprintf(f, "  version_partida: %d, hz: %d, max_seg: %.1f, amb_seg: %.1f, icono_lado: %d,\n", PET_SAVE_VERSION, HZ,
            SFX_MAX_SEGUNDOS, SFX_AMB_SEGUNDOS, ART_ICONO_LADO);

    static const char *const GI[][2] = {{"boton", "Botones"}, {"objeto", "Comida y hallazgos"}, {"decor", "Decoraciones"},
        {"clima", "Clima"}, {"emocion", "Emociones"}, {"causa", "Causas"}, {"efecto", "Efectos"}, {"fauna", "Animales"},
        {"jardin", "Jardín"}, {"zona", "Zonas"}, {"fiesta", "Fiesta"}, {"lago", "El lago"}, {"juego", "Juegos y trucos"},
        {"recuerdo", "Recuerdos y metas"}, {"tesoro", "Tesoros"}, {"raro", "Cosas raras"}, {"fecha", "Fechas especiales"},
        {"ajustes", "Ajustes"}, {"ropa", "Ropa"}, {"juguete", "Juguetes"},
        {"obj_tierra", "Objetos: tesoros de la tierra"}, {"obj_flora", "Objetos: flora mística"}, {"obj_espiritu", "Objetos: animales y espíritus"},
        {"obj_antiguo", "Objetos: antigüedades"}, {"obj_artefacto", "Objetos: artefactos místicos"}};
    fprintf(f, "  grupos_iconos: [");
    for (unsigned g = 0; g < sizeof GI / sizeof GI[0]; g++) { fprintf(f, "%s[", g ? "," : ""); js_str(f, GI[g][0]); fputc(',', f); js_str(f, GI[g][1]); fputc(']', f); }
    fprintf(f, "],\n  iconos: [\n");
    uint32_t px[ART_ICONO_LADO * ART_ICONO_LADO];
    uint8_t b[ART_ICONO_LADO * ART_ICONO_LADO * 4];
    for (int i = 0; i < ICO_COUNT; i++) {
        const art_icono_info_t *in = art_icono_info((art_icono_t)i);
        art_icono_fabrica((art_icono_t)i, px);
        rgba(px, b);
        fprintf(f, "    {n: %d, id: ", i); js_str(f, in->id);
        fprintf(f, ", grupo: "); js_str(f, in->grupo);
        fprintf(f, ", nombre: "); js_str(f, in->nombre);
        fprintf(f, ", significado: "); js_str(f, in->significado);
        fprintf(f, ", uso: "); js_str(f, in->uso);
        fprintf(f, ", px: "); js_b64(f, b, (int)sizeof b);
        fprintf(f, "},\n");
    }
    static const char *const GS[][2] = {{"animo", "Su voz: cómo se siente"}, {"cuerpo", "Su cuerpo"}, {"accion", "Lo que hace"},
        {"juego", "Juegos y trucos"}, {"mundo", "El mundo"}, {"musica", "Música"}};
    fprintf(f, "  ],\n  grupos_sonidos: [");
    for (unsigned g = 0; g < sizeof GS / sizeof GS[0]; g++) { fprintf(f, "%s[", g ? "," : ""); js_str(f, GS[g][0]); fputc(',', f); js_str(f, GS[g][1]); fputc(']', f); }
    fprintf(f, "],\n  sonidos: [\n");
    static int16_t pcm[147200]; /* 9,2 s a 16 kHz */
    for (int s = 1; s < SND_COUNT; s++) {
        const pet_sonido_info_t *in = pet_sound_info((sound_t)s);
        int n = sfx_render((sound_t)s, pcm, (int)(sizeof pcm / 2), HZ);
        fprintf(f, "    {n: %d, id: ", s); js_str(f, in->id);
        fprintf(f, ", grupo: "); js_str(f, in->grupo);
        fprintf(f, ", nombre: "); js_str(f, in->nombre);
        fprintf(f, ", significado: "); js_str(f, in->significado);
        fprintf(f, ", cuando: "); js_str(f, in->cuando);
        fprintf(f, ", pcm: "); js_b64(f, (const uint8_t *)pcm, n * 2);
        fprintf(f, "},\n");
    }
    static const struct { int a; const char *id, *nombre, *cuando; } AMB[] = {
        {AMB_PRADO, "AMB_PRADO", "Prado", "De día en el prado"}, {AMB_BOSQUE, "AMB_BOSQUE", "Bosque", "De día en el bosque"},
        {AMB_LAGO, "AMB_LAGO", "Lago", "De día en el lago"}, {AMB_CUEVA, "AMB_CUEVA", "Cueva", "En la cueva"},
        {AMB_NOCHE, "AMB_NOCHE", "Grillos", "De noche (no en invierno)"}, {AMB_LLUVIA, "AMB_LLUVIA", "Lluvia", "Cuando llueve"},
        {AMB_TORMENTA, "AMB_TORMENTA", "Tormenta", "Con tormenta"}, {AMB_VIENTO, "AMB_VIENTO", "Viento", "Con viento o frío"}, {AMB_MAR, "AMB_MAR", "Mar", "De día en la isla"}};
    fprintf(f, "  ],\n  ambientes: [\n");
    for (unsigned k = 0; k < sizeof AMB / sizeof AMB[0]; k++) {
        int n = sfx_ambient((ambiente_t)AMB[k].a, pcm, (int)(sizeof pcm / 2), HZ);
        fprintf(f, "    {n: %d, id: ", AMB[k].a); js_str(f, AMB[k].id);
        fprintf(f, ", nombre: "); js_str(f, AMB[k].nombre);
        fprintf(f, ", cuando: "); js_str(f, AMB[k].cuando);
        fprintf(f, ", pcm: "); js_b64(f, (const uint8_t *)pcm, n * 2);
        fprintf(f, "},\n");
    }
    fprintf(f, "  ],\n  decoraciones: [");
    static const struct { int d; const char *n; int ico; } DEC[] = {{FECHA_HALLOWEEN, "Calabazas", ICO_CALABAZA},
        {FECHA_NAVIDAD, "Arbolito con regalo", ICO_ARBOL_NAVIDAD}, {FECHA_ANIO_NUEVO, "Fuegos artificiales", ICO_FUEGOS},
        {FECHA_SAN_VALENTIN, "Corazones", ICO_CORAZON}, {FECHA_PRIMAVERA, "Flores", ICO_FLOR}, {FECHA_FIESTAS_PATRIAS, "Banderines", ICO_BANDERINES}};
    for (unsigned k = 0; k < sizeof DEC / sizeof DEC[0]; k++) { fprintf(f, "%s{id: %d, nombre: ", k ? ", " : "", DEC[k].d); js_str(f, DEC[k].n); fprintf(f, ", icono: %d}", DEC[k].ico); }
    fprintf(f, "],\n  celebraciones: [\n");
    pet_celebrations_default();
    for (int i = 0; i < pet_celebration_count(); i++) {
        const celebracion_t *c = pet_celebration_get(i);
        fprintf(f, "    {regla: %d, mes: %d, dia: %d, semana: %d, dias: %d, deco: %d, nombre: ", c->regla, c->mes, c->dia, c->semana, c->dias, c->deco);
        js_str(f, c->nombre); fprintf(f, ", saludo: "); js_str(f, c->saludo); fprintf(f, "},\n");
    }
    fprintf(f, "  ],\n  max_celebraciones: %d,\n", PET_CELEBRACIONES);
    /* las criaturas de fábrica: el cuerpo de cada especie en cada etapa (sin cara, patitas ni contorno) */
    fprintf(f, "  criatura_lado: %d, cuerpo_bytes: %d,\n  criaturas: [\n", ART_CRIATURA_LADO_C, ART_CUERPO_BYTES);
    for (int e = 0; e < pet_species_count(); e++) {
        fprintf(f, "    {e: %d, nombre: ", e); js_str(f, pet_species_factory_name((uint8_t)e)); fprintf(f, ", etapas: [");
        for (int et = 1; et <= 3; et++) {
            static art_cuerpo_t c;
            static uint8_t o[ART_CRIATURA_LADO_C * ART_CRIATURA_LADO_C * 4]; /* 30x30 */
            art_criatura_fabrica(e, et, &c);
            for (int i = 0; i < ART_CRIATURA_LADO_C * ART_CRIATURA_LADO_C; i++) {
                uint32_t v = c.px[i];
                o[i * 4] = (uint8_t)(v >> 16); o[i * 4 + 1] = (uint8_t)(v >> 8); o[i * 4 + 2] = (uint8_t)v; o[i * 4 + 3] = (uint8_t)(v >> 24);
            }
            fprintf(f, "%s{ojo_x: %d, ojo_y: %d, sep: %d, patitas: %d, px: ", et > 1 ? ", " : "", c.ojo_x, c.ojo_y, c.ojos_sep, c.patitas);
            js_b64(f, o, sizeof o);
            fprintf(f, "}");
        }
        fprintf(f, "]},\n");
    }
    fprintf(f, "  ],\n};\n");
    fclose(f);
    printf("%d íconos, %d sonidos y %d ambientes -> %s\n", ICO_COUNT, SND_COUNT - 1, (int)(sizeof AMB / sizeof AMB[0]), ruta);
    return 0;
}
