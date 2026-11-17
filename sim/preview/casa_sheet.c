/* La casita por dentro (de día y de noche, con luz y sin luz) en un PPM: para ver la cama. */
#include <stdio.h>
#include "art.h"
#include "lvgl.h"
int main(int argc, char **argv)
{
    lv_init();
    static uint32_t hoja[2 * 466 * 466];
    const art_periodo_t per[2] = {PERIODO_DIA, PERIODO_NOCHE};
    for (int k = 0; k < 2; k++) {
        const lv_image_dsc_t *d = art_fondo(per[k], 0, ESTACION_ART_VERANO, 0, ART_ESCENA_CASA, 0);
        const uint16_t *p = (const uint16_t *)d->data;
        for (int y = 0; y < 466; y++)
            for (int x = 0; x < 466; x++) {
                int sx = x * (int)d->header.w / 466, sy = y * (int)d->header.h / 466;
                uint16_t v = p[sy * d->header.w + sx];
                hoja[y * 932 + k * 466 + x] = ((uint32_t)(v >> 11) << 19) | ((uint32_t)((v >> 5) & 63) << 10) | ((uint32_t)(v & 31) << 3);
            }
    }
    FILE *f = fopen(argc > 1 ? argv[1] : "casa.ppm", "wb");
    fprintf(f, "P6\n932 466\n255\n");
    for (int y = 0; y < 466; y++)
        for (int x = 0; x < 932; x++) {
            uint32_t c = hoja[y * 932 + x];
            unsigned char px[3] = {(unsigned char)(c >> 16), (unsigned char)(c >> 8), (unsigned char)c};
            fwrite(px, 1, 3, f);
        }
    fclose(f);
    return 0;
}
