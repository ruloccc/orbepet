/* Las criaturas de fábrica dibujadas como si fueran del editor (art_criatura_propia), con varias caras,
 * a escala 3 y 4: la prueba editor/pruebas/prueba_criaturas.js compara la vista previa del editor contra esto.
 * Uso: mascota_criaturas_ref <archivo> */
#include "art.h"
#include <stdio.h>
#include <string.h>
/* para cada especie/etapa: cuerpo de fábrica como propio, render a escala 3 y 4, varias caras */
int main(int argc, char **argv)
{
    lv_init();
    FILE *f = fopen(argc > 1 ? argv[1] : "criaturas_ref.bin", "wb");
    if (!f) return 1;
    static art_cuerpo_t c;
    const art_expresion_t EX[] = {EXP_ABIERTOS, EXP_FELIZ, EXP_TRISTE, EXP_ORGULLO, EXP_CERRADOS};
    for (int e = 0; e < 8; e++)
        for (int et = 1; et <= 3; et++) {
            art_criatura_fabrica(e, et, &c);
            fwrite(&c.ojo_x, 1, 4, f);
            fwrite(c.px, 4, 900, f);
            art_criatura_propia(e, et, &c);
            for (int x = 0; x < 5; x++)
                for (int esc = 3; esc <= 4; esc++)
                    for (int fr = 0; fr < 2; fr++) {
                        art_img_t im = {0};
                        art_pose_t ps = {0};
                        ps.especie = e; ps.etapa = et; ps.escala = esc; ps.expr = EX[x]; ps.frame = fr; ps.postura = x == 4 ? -2 : (x == 3 ? 1 : 0);
                        art_criatura_pose(&im, &ps);
                        int W = 20 * esc;
                        for (int y = 0; y < W; y += 2)
                            for (int xx = 0; xx < W; xx += 2) { uint32_t v = im.px[y * W + xx]; fwrite(&v, 4, 1, f); }
                        art_img_free(&im);
                    }
            art_criatura_propia(e, et, NULL);
        }
    fclose(f);
    return 0;
}
