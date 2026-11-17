/* Fotos en formato QOI (ver foto.h). Implementación propia y mínima del formato público. */
#include "foto.h"

#include <string.h>

#define QOI_OP_INDEX 0x00
#define QOI_OP_DIFF 0x40
#define QOI_OP_LUMA 0x80
#define QOI_OP_RUN 0xC0
#define QOI_OP_RGB 0xFE
#define QOI_OP_RGBA 0xFF
#define QOI_MASK_2 0xC0

typedef struct { uint8_t r, g, b, a; } rgba_t;

static int hash(rgba_t c) { return (c.r * 3 + c.g * 5 + c.b * 7 + c.a * 11) % 64; }

static void put32(uint8_t *o, uint32_t v)
{
    o[0] = (uint8_t)(v >> 24); o[1] = (uint8_t)(v >> 16); o[2] = (uint8_t)(v >> 8); o[3] = (uint8_t)v;
}
static uint32_t get32(const uint8_t *i) { return (uint32_t)i[0] << 24 | (uint32_t)i[1] << 16 | (uint32_t)i[2] << 8 | i[3]; }

static rgba_t de565(uint16_t v)
{
    rgba_t c;
    uint8_t r = (uint8_t)(v >> 11), g = (uint8_t)((v >> 5) & 63), b = (uint8_t)(v & 31);
    c.r = (uint8_t)(r << 3 | r >> 2);
    c.g = (uint8_t)(g << 2 | g >> 4);
    c.b = (uint8_t)(b << 3 | b >> 2);
    c.a = 255;
    return c;
}
static uint16_t a565(rgba_t c) { return (uint16_t)((c.r >> 3) << 11 | (c.g >> 2) << 5 | (c.b >> 3)); }

int foto_qoi_max(int w, int h) { return 14 + w * h * 4 + 8; }

int foto_qoi_encode_565(const uint16_t *px, int w, int h, int stride, uint8_t *out, int cap)
{
    if (!px || !out || w <= 0 || h <= 0 || cap < 22) return -1;
    int k = 0;
    memcpy(out, "qoif", 4);
    put32(out + 4, (uint32_t)w);
    put32(out + 8, (uint32_t)h);
    out[12] = 3; /* RGB */
    out[13] = 0; /* sRGB */
    k = 14;
    rgba_t idx[64];
    memset(idx, 0, sizeof idx);
    rgba_t prev = {0, 0, 0, 255};
    int run = 0;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            if (k + 6 > cap) return -1;
            rgba_t c = de565(px[y * stride + x]);
            bool ultimo = y == h - 1 && x == w - 1;
            if (c.r == prev.r && c.g == prev.g && c.b == prev.b) {
                run++;
                if (run == 62 || ultimo) { out[k++] = (uint8_t)(QOI_OP_RUN | (run - 1)); run = 0; }
                continue;
            }
            if (run) { out[k++] = (uint8_t)(QOI_OP_RUN | (run - 1)); run = 0; }
            int h6 = hash(c);
            if (idx[h6].r == c.r && idx[h6].g == c.g && idx[h6].b == c.b && idx[h6].a == c.a) {
                out[k++] = (uint8_t)(QOI_OP_INDEX | h6);
            } else {
                idx[h6] = c;
                int vr = (int8_t)(c.r - prev.r), vg = (int8_t)(c.g - prev.g), vb = (int8_t)(c.b - prev.b);
                int vgr = vr - vg, vgb = vb - vg;
                if (vr > -3 && vr < 2 && vg > -3 && vg < 2 && vb > -3 && vb < 2)
                    out[k++] = (uint8_t)(QOI_OP_DIFF | (vr + 2) << 4 | (vg + 2) << 2 | (vb + 2));
                else if (vgr > -9 && vgr < 8 && vg > -33 && vg < 32 && vgb > -9 && vgb < 8) {
                    out[k++] = (uint8_t)(QOI_OP_LUMA | (vg + 32));
                    out[k++] = (uint8_t)((vgr + 8) << 4 | (vgb + 8));
                } else {
                    out[k++] = QOI_OP_RGB;
                    out[k++] = c.r; out[k++] = c.g; out[k++] = c.b;
                }
            }
            prev = c;
        }
    if (k + 8 > cap) return -1;
    static const uint8_t FIN[8] = {0, 0, 0, 0, 0, 0, 0, 1};
    memcpy(out + k, FIN, 8);
    return k + 8;
}

bool foto_qoi_info(const uint8_t *q, int n, int *w, int *h)
{
    if (!q || n < 22 || memcmp(q, "qoif", 4)) return false;
    uint32_t W = get32(q + 4), H = get32(q + 8);
    if (!W || !H || W > 4096 || H > 4096) return false;
    if (w) *w = (int)W;
    if (h) *h = (int)H;
    return true;
}

bool foto_qoi_decode_565(const uint8_t *q, int n, uint16_t *out, int paso)
{
    int w, h;
    if (!foto_qoi_info(q, n, &w, &h) || !out || paso < 1) return false;
    int ow = w / paso;
    rgba_t idx[64];
    memset(idx, 0, sizeof idx);
    rgba_t c = {0, 0, 0, 255};
    int k = 14, run = 0, fin = n - 8;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            if (run > 0) run--;
            else if (k < fin) {
                uint8_t b1 = q[k++];
                if (b1 == QOI_OP_RGB) { c.r = q[k]; c.g = q[k + 1]; c.b = q[k + 2]; k += 3; }
                else if (b1 == QOI_OP_RGBA) { c.r = q[k]; c.g = q[k + 1]; c.b = q[k + 2]; c.a = q[k + 3]; k += 4; }
                else if ((b1 & QOI_MASK_2) == QOI_OP_INDEX) c = idx[b1];
                else if ((b1 & QOI_MASK_2) == QOI_OP_DIFF) {
                    c.r = (uint8_t)(c.r + ((b1 >> 4) & 3) - 2);
                    c.g = (uint8_t)(c.g + ((b1 >> 2) & 3) - 2);
                    c.b = (uint8_t)(c.b + (b1 & 3) - 2);
                } else if ((b1 & QOI_MASK_2) == QOI_OP_LUMA) {
                    uint8_t b2 = q[k++];
                    int vg = (b1 & 0x3F) - 32;
                    c.r = (uint8_t)(c.r + vg - 8 + ((b2 >> 4) & 0x0F));
                    c.g = (uint8_t)(c.g + vg);
                    c.b = (uint8_t)(c.b + vg - 8 + (b2 & 0x0F));
                } else if ((b1 & QOI_MASK_2) == QOI_OP_RUN) run = b1 & 0x3F;
                idx[hash(c)] = c;
            } else return false; /* se cortó */
            if (paso == 1) out[y * w + x] = a565(c);
            else if (y % paso == 0 && x % paso == 0 && x / paso < ow && y / paso < h / paso) out[(y / paso) * ow + x / paso] = a565(c);
        }
    return true;
}
