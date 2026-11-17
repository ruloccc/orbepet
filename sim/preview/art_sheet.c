/* Hoja de muestra del arte: criaturas, íconos, fuente y fondos, en un PPM. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "art.h"
#include "lvgl.h"

#define SW 1000
#define SH 2200
static uint32_t hoja[SW * SH];

static void pegar_argb(const lv_image_dsc_t *d, int x0, int y0)
{
    const uint32_t *p = (const uint32_t *)d->data;
    for (uint32_t y = 0; y < d->header.h; y++)
        for (uint32_t x = 0; x < d->header.w; x++) {
            uint32_t c = p[y * d->header.w + x];
            int X = x0 + (int)x, Y = y0 + (int)y;
            if ((c >> 24) && X < SW && Y < SH) hoja[Y * SW + X] = c;
        }
}

static void pegar_565(const lv_image_dsc_t *d, int x0, int y0, int div)
{
    const uint16_t *p = (const uint16_t *)d->data;
    for (uint32_t y = 0; y < d->header.h; y += div)
        for (uint32_t x = 0; x < d->header.w; x += div) {
            uint16_t v = p[y * d->header.w + x];
            uint32_t c = ((uint32_t)(v >> 11) << 19) | ((uint32_t)((v >> 5) & 63) << 10) | ((uint32_t)(v & 31) << 3);
            int X = x0 + (int)(x / div), Y = y0 + (int)(y / div);
            if (X < SW && Y < SH) hoja[Y * SW + X] = c;
        }
}

int main(int argc, char **argv)
{
    lv_init();
    for (int i = 0; i < SW * SH; i++) hoja[i] = 0xF4F1EC;
    art_img_t img = {0};
    int y = 10;
    /* 8 especies: huevo, bebé, cría, adulta normal, radiante, gruñona, enferma, recuerdo (escala 4) */
    for (int e = 0; e < 8; e++, y += 86) {
        art_huevo(&img, e, 4, e % 4, 0);
        pegar_argb(&img.dsc, 10, y);
        for (int k = 0; k < 6; k++) {
            int etapa = k < 3 ? k + 1 : 3, forma = k < 3 ? 0 : k - 2;
            if (k == 5) art_criatura(&img, e, 3, 0, 4, EXP_MAREADO, false, 0, false, true);
            else art_criatura(&img, e, etapa, forma, 4, k == 3 ? EXP_FELIZ : EXP_ABIERTOS, false, 0, false, false);
            if (k == 4) art_criatura(&img, e, 3, 2, 4, EXP_ENOJADO, false, 0, false, false);
            if (k == 3) art_criatura(&img, e, 3, 1, 4, EXP_FELIZ, true, 0, false, false);
            pegar_argb(&img.dsc, 100 + k * 86, y);
        }
        art_criatura(&img, e, 3, 0, 4, EXP_ABIERTOS, false, 1, true, false);
        pegar_argb(&img.dsc, 100 + 6 * 86, y);
        art_recuerdo(&img, e, 4);
        pegar_argb(&img.dsc, 100 + 7 * 86, y);
    }
    /* expresiones de una adulta */
    for (int ex = 0; ex < EXP_COUNT; ex++) {
        art_criatura(&img, 3, 3, 0, 3, (art_expresion_t)ex, ex == 2, ex & 1, false, ex == EXP_MAREADO);
        pegar_argb(&img.dsc, 800 + (ex % 3) * 66, 10 + (ex / 3) * 66);
    }
    /* lenguaje corporal: cada especie triste (orejas y cola abajo, encogida), orgullosa y echada */
    for (int e = 0; e < 8; e++) {
        art_pose_t t = {e, 3, 0, 3, EXP_TRISTE, false, 0, false, false, -1, -1, -1, {0, 0, 0}, 0, 0};
        art_criatura_pose(&img, &t); pegar_argb(&img.dsc, 800 + (e % 2) * 100, 290 + (e / 2) * 66);
        art_pose_t o = {e, 3, 0, 3, EXP_ORGULLO, false, 1, false, false, 1, 1, 1, {0, 0, 0}, 0, 0};
        art_criatura_pose(&img, &o); pegar_argb(&img.dsc, 800 + (e % 2) * 100 + 50, 290 + (e / 2) * 66);
    }
    /* íconos */
    for (int i = 0; i < ICO_COUNT; i++) pegar_argb(art_icono((art_icono_t)i, 3), 10 + (i % 18) * 44, y + (i / 18) * 44);
    y += ((ICO_COUNT + 17) / 18) * 44 + 10;
    art_texto(&img, "¡HOLA! ¿JUGAMOS? Ñuño 12:30 +5 ÁÉÍÓÚ", 0x2A2238, 3, 900);
    pegar_argb(&img.dsc, 10, y);
    y += img.dsc.header.h + 10;
    /* fondos: las 4 estaciones (a media escala), atardecer nublado y noche */
    for (int e = 0; e < 4; e++) pegar_565(art_fondo(PERIODO_DIA, e == 1 ? 0x1F : 0, (art_estacion_t)e, 0, 0, e == 1 ? 0x7FFF : 0), 10 + e * 240, y, 2);
    pegar_565(art_fondo(PERIODO_ATARDECER, 0x1F, ESTACION_ART_OTONO, 1, 0, 0), 10, y + 240, 2);
    pegar_565(art_fondo(PERIODO_NOCHE, 0x1F, ESTACION_ART_INVIERNO, 2, 0, 0), 250, y + 240, 2);
    /* las otras zonas: bosque, lago, colina, cueva (verano) y bosque en otoño, lago helado, colina en primavera, cueva de noche */
    for (int z = 1; z < 5; z++) pegar_565(art_fondo(PERIODO_DIA, 0, ESTACION_ART_VERANO, 0, z, 0), 10 + (z - 1) * 240, y + 480, 2);
    pegar_565(art_fondo(PERIODO_DIA, 0, ESTACION_ART_OTONO, 0, 1, 0), 10, y + 720, 2);
    pegar_565(art_fondo(PERIODO_ATARDECER, 0, ESTACION_ART_INVIERNO, 2, 2, 0), 250, y + 720, 2);
    pegar_565(art_fondo(PERIODO_DIA, 0, ESTACION_ART_PRIMAVERA, 0, 3, 0), 490, y + 720, 2);
    pegar_565(art_fondo(PERIODO_NOCHE, 0, ESTACION_ART_VERANO, 0, 4, 0), 730, y + 720, 2);

    FILE *f = fopen(argc > 1 ? argv[1] : "arte.ppm", "wb");
    fprintf(f, "P6\n%d %d\n255\n", SW, SH);
    for (int i = 0; i < SW * SH; i++) {
        unsigned char px[3] = {(unsigned char)(hoja[i] >> 16), (unsigned char)(hoja[i] >> 8), (unsigned char)hoja[i]};
        fwrite(px, 1, 3, f);
    }
    fclose(f);
    return 0;
}
