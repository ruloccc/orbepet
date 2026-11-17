/* Un fondo del mundo en PPM: fondo_sheet salida.ppm escena periodo estacion nublado (466x466) */
#include <stdio.h>
#include <stdlib.h>
#include "art.h"
#include "lvgl.h"
int main(int argc, char **argv)
{
    lv_init();
    const int esc = argc > 2 ? atoi(argv[2]) : 0, per = argc > 3 ? atoi(argv[3]) : 0, est = argc > 4 ? atoi(argv[4]) : 1, nub = argc > 5 ? atoi(argv[5]) : 0, dec = argc > 6 ? atoi(argv[6]) : 0;
    if (esc == 99) { /* el fondo del viaje en bote (estación = est) */
        art_img_t im = {0};
        art_cruce_estacion(est);
        art_cruce_fondo(&im, per);
        FILE *g = fopen(argv[1], "wb");
        fprintf(g, "P6\n%d %d\n255\n", (int)im.dsc.header.w, (int)im.dsc.header.h);
        for (uint32_t i = 0; i < im.dsc.header.w * im.dsc.header.h; i++) { unsigned char q[3] = {(unsigned char)(im.px[i] >> 16), (unsigned char)(im.px[i] >> 8), (unsigned char)im.px[i]}; fwrite(q, 1, 3, g); }
        fclose(g);
        return 0;
    }
    const lv_image_dsc_t *d = art_fondo((art_periodo_t)per, (uint32_t)dec, (art_estacion_t)est, nub, esc, 0);
    const uint16_t *p = (const uint16_t *)d->data;
    FILE *f = fopen(argv[1], "wb");
    fprintf(f, "P6\n466 466\n255\n");
    for (int y = 0; y < 466; y++)
        for (int x = 0; x < 466; x++) {
            uint16_t v = p[(y * (int)d->header.h / 466) * d->header.w + x * (int)d->header.w / 466];
            unsigned char px[3] = {(unsigned char)((v >> 11) << 3), (unsigned char)(((v >> 5) & 63) << 2), (unsigned char)((v & 31) << 3)};
            fwrite(px, 1, 3, f);
        }
    fclose(f);
    return 0;
}
