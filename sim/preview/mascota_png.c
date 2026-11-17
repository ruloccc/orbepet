/* Una criatura adulta en ARGB crudo: mascota_png salida.raw especie escala */
#include <stdio.h>
#include <stdlib.h>
#include "art.h"
#include "lvgl.h"
int main(int argc, char **argv)
{
    lv_init();
    if (argc > 4) art_tamano_set(atoi(argv[4]));
    art_img_t img = {0};
    art_criatura(&img, atoi(argv[2]), 3, 0, atoi(argv[3]), EXP_FELIZ, false, 0, false, false);
    FILE *f = fopen(argv[1], "wb");
    fprintf(f, "%u %u\n", (unsigned)img.dsc.header.w, (unsigned)img.dsc.header.h);
    fwrite(img.dsc.data, 4, img.dsc.header.w * img.dsc.header.h, f);
    fclose(f);
    return 0;
}
