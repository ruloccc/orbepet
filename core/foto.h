/*
 * Fotos de la mascota: la pantalla (RGB565) comprimida en formato QOI ("Quite OK Image",
 * de dominio público, https://qoiformat.org). Sin pérdida, rápido y simple: el editor de
 * Windows (o cualquier programa que lea QOI) la convierte a PNG.
 */
#ifndef FOTO_H
#define FOTO_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* el peor caso (todo distinto): para reservar el búfer de salida */
int foto_qoi_max(int w, int h);
/* comprime w x h pixeles RGB565 (stride en pixeles). Devuelve los bytes escritos o -1. */
int foto_qoi_encode_565(const uint16_t *px, int w, int h, int stride, uint8_t *out, int cap);
/* ancho y alto de un QOI (false si no lo es) */
bool foto_qoi_info(const uint8_t *q, int n, int *w, int *h);
/* descomprime a RGB565 tomando un pixel de cada "paso" (1 = tamaño real, 2 = mitad, 4 = cuarto):
 * out debe tener (w / paso) x (h / paso) pixeles. */
bool foto_qoi_decode_565(const uint8_t *q, int n, uint16_t *out, int paso);

#ifdef __cplusplus
}
#endif
#endif
