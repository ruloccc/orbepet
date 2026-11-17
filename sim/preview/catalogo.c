/*
 * Catálogo de íconos: genera la hoja de muestra y la lista de qué significa cada uno.
 *
 *   ./mascota_iconos carpeta/   ->  iconos.ppm (hoja), iconos.md (tabla) e iconos.json
 *
 * La lista sale de ART_ICONOS en art.h, la misma que usa el firmware. El editor de
 * personalizaciones leerá iconos.json para saber qué representa cada ícono y su tamaño.
 */
#include <stdio.h>
#include <string.h>

#include "art.h"
#include "lvgl.h"

#define SW 1240
#define CELDA_W 155
#define CELDA_H 126
#define POR_FILA 8

static uint32_t *hoja;
static int SH;

static void pegar(const lv_image_dsc_t *d, int x0, int y0)
{
    const uint32_t *p = (const uint32_t *)d->data;
    for (uint32_t y = 0; y < d->header.h; y++)
        for (uint32_t x = 0; x < d->header.w; x++) {
            uint32_t c = p[y * d->header.w + x];
            int X = x0 + (int)x, Y = y0 + (int)y;
            if ((c >> 24) && X >= 0 && X < SW && Y >= 0 && Y < SH) hoja[Y * SW + X] = c & 0xFFFFFF;
        }
}

static void json_txt(FILE *f, const char *s)
{
    fputc('"', f);
    for (; *s; s++) {
        if (*s == '"' || *s == '\\') fputc('\\', f);
        fputc(*s, f);
    }
    fputc('"', f);
}

int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : ".";
    lv_init();
    static const char *const GRUPOS[] = {"boton", "objeto", "decor", "clima", "emocion", "causa", "efecto", "fauna", "jardin", "zona", "fiesta", "lago", "juego", "recuerdo", "tesoro", "raro", "fecha", "ajustes", "ropa", "juguete",
                                         "obj_tierra", "obj_flora", "obj_espiritu", "obj_antiguo", "obj_artefacto",
                                         "animal", "magia", "mensajero"};
    static const char *const TITULOS[] = {"Botones", "Comida y hallazgos", "Decoraciones", "Clima",
                                          "Emociones", "Causas (por qué siente algo)", "Efectos",
                                          "Animales", "Jardín, charcos y hojas", "Zonas del mundo",
                                          "Fiesta (tu cumpleaños)", "El lago (pesca)", "Juegos y trucos",
                                          "Recuerdos y metas", "Tesoros y colecciones", "Cosas raras", "Fechas especiales", "Ajustes, hora y respaldo", "Ropa y accesorios", "Juguetes",
                                          "Objetos mágicos: tesoros de la tierra", "Objetos mágicos: flora mística", "Objetos mágicos: restos de animales y espíritus",
                                          "Objetos mágicos: objetos perdidos y antigüedades", "Objetos mágicos: artefactos místicos",
                                          "Animales del bosque", "Criaturas mágicas y secretos", "Mensajeros de las cartas"};
    const int NG = 28;

    /* alto de la hoja: un título y las filas de cada grupo */
    SH = 20;
    for (int g = 0; g < NG; g++) {
        int n = 0;
        for (int i = 0; i < ICO_COUNT; i++) n += !strcmp(art_icono_info((art_icono_t)i)->grupo, GRUPOS[g]);
        SH += 40 + ((n + POR_FILA - 1) / POR_FILA) * CELDA_H;
    }
    static uint32_t buf[SW * 10000];
    if (SH > 10000) SH = 10000;
    hoja = buf;
    for (int i = 0; i < SW * SH; i++) hoja[i] = 0xF4F1EC;

    char ruta[512];
    snprintf(ruta, sizeof ruta, "%s/iconos.md", dir);
    FILE *md = fopen(ruta, "w");
    snprintf(ruta, sizeof ruta, "%s/iconos.json", dir);
    FILE *js = fopen(ruta, "w");
    if (!md || !js) { perror(ruta); return 1; }
    fprintf(md, "# Catálogo de íconos\n\nCada ícono se diseña en 12×12 y se pinta con más detalle: a tamaño de botón (escala 4) "
                "sale de 24×24 pixeles, cada uno de 2×2 en pantalla. En el editor se dibujan en 24×24. Se generan desde "
                "`ART_ICONOS` en `firmware/main/art.h`; esta lista y `iconos.json` las usa el editor.\n\n");
    fprintf(js, "{\n  \"lado\": %d,\n  \"iconos\": [\n", ART_ICONO_LADO);

    art_img_t img = {0};
    int y = 20, total = 0;
    for (int g = 0; g < NG; g++) {
        art_texto(&img, TITULOS[g], 0x2A2238, 3, 0);
        pegar(&img.dsc, 20, y);
        y += 40;
        fprintf(md, "## %s\n\n| # | Ícono | Nombre | Qué significa | Dónde aparece |\n|---|---|---|---|---|\n", TITULOS[g]);
        int k = 0;
        for (int i = 0; i < ICO_COUNT; i++) {
            const art_icono_info_t *in = art_icono_info((art_icono_t)i);
            if (strcmp(in->grupo, GRUPOS[g])) continue;
            int x = 20 + (k % POR_FILA) * CELDA_W, yy = y + (k / POR_FILA) * CELDA_H;
            pegar(art_icono((art_icono_t)i, 4), x + (CELDA_W - 48) / 2 - 10, yy);
            char n[8];
            snprintf(n, sizeof n, "%d", i);
            art_texto(&img, n, 0x8A8FA3, 2, 0);
            pegar(&img.dsc, x, yy);
            art_texto(&img, in->nombre, 0x2A2238, 2, CELDA_W - 10);
            pegar(&img.dsc, x, yy + 54);
            fprintf(md, "| %d | `%s` | %s | %s | %s |\n", i, in->id, in->nombre, in->significado, in->uso);
            fprintf(js, "%s    {\"numero\": %d, \"id\": ", total ? ",\n" : "", i);
            json_txt(js, in->id);
            fprintf(js, ", \"grupo\": ");
            json_txt(js, in->grupo);
            fprintf(js, ", \"nombre\": ");
            json_txt(js, in->nombre);
            fprintf(js, ", \"significado\": ");
            json_txt(js, in->significado);
            fprintf(js, ", \"uso\": ");
            json_txt(js, in->uso);
            fprintf(js, "}");
            k++;
            total++;
        }
        fprintf(md, "\n");
        y += ((k + POR_FILA - 1) / POR_FILA) * CELDA_H;
    }
    fprintf(js, "\n  ]\n}\n");
    fclose(md);
    fclose(js);

    snprintf(ruta, sizeof ruta, "%s/iconos.ppm", dir);
    FILE *f = fopen(ruta, "wb");
    fprintf(f, "P6\n%d %d\n255\n", SW, SH);
    for (int i = 0; i < SW * SH; i++) {
        unsigned char px[3] = {(unsigned char)(hoja[i] >> 16), (unsigned char)(hoja[i] >> 8), (unsigned char)hoja[i]};
        fwrite(px, 1, 3, f);
    }
    fclose(f);
    printf("%d íconos\n", total);
    return 0;
}
