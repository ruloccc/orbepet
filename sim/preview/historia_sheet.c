/*
 * Vista previa de la interfaz del reloj en el PC, sin hardware.
 *
 * Usa el mismo ui.c y art.c del firmware y el mismo motor; dibuja con LVGL en
 * memoria y guarda una imagen PPM por escena (466x466 con la máscara redonda).
 *
 *   ./mascota_preview [carpeta_salida]
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "lvgl.h"
#include "pet.h"
#include "ui.h"

#define W 466
#define T0 1792972800u /* 2026-10-26 00:00 UTC */

static uint32_t s_ms;
static uint32_t tick_cb(void) { return s_ms; }
static uint32_t s_fb[W * W];
static void flush_cb(lv_display_t *d, const lv_area_t *a, uint8_t *px)
{
    (void)a; (void)px;
    lv_display_flush_ready(d);
}

static int s_tx = -1, s_ty = -1;
static void touch_cb(lv_indev_t *i, lv_indev_data_t *d)
{
    (void)i;
    d->state = s_tx >= 0 ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
    if (s_tx >= 0) { d->point.x = s_tx; d->point.y = s_ty; }
}

static pet_t p;
static uint32_t t_actual;

static void on_action(int a, int arg)
{
    if (a == UI_ACCION_DECORAR) pet_place_decor(&p, (decor_t)arg, t_actual);
    else pet_action(&p, (action_t)a, arg, t_actual);
}

static void correr(uint32_t ms)
{
    for (uint32_t t = 0; t < ms; t += 10) {
        s_ms += 10;
        lv_timer_handler();
    }
}

static void eventos(void)
{
    pet_event_t e;
    while (pet_poll_event(&p, &e))
        if (e.type != EV_SONIDO) ui_show_event(&p, &e);
}

static __attribute__((unused)) void tocar(int x, int y)
{
    s_tx = x; s_ty = y;
    correr(120);
    s_tx = s_ty = -1;
    correr(150);
    eventos();
    ui_update(&p, t_actual);
    correr(300);
}

static void guardar(const char *dir, const char *nombre)
{
    lv_refr_now(NULL);
    char ruta[256];
    snprintf(ruta, sizeof ruta, "%s/%s.ppm", dir, nombre);
    FILE *f = fopen(ruta, "wb");
    if (!f) { perror(ruta); return; }
    fprintf(f, "P6\n%d %d\n255\n", W, W);
    const float r = W / 2.0f;
    for (int y = 0; y < W; y++)
        for (int x = 0; x < W; x++) {
            float dx = x + 0.5f - r, dy = y + 0.5f - r;
            uint32_t c = s_fb[y * W + x];
            unsigned char px[3] = {(unsigned char)(c >> 16), (unsigned char)(c >> 8), (unsigned char)c};
            if (dx * dx + dy * dy > r * r) px[0] = px[1] = px[2] = 40;
            fwrite(px, 1, 3, f);
        }
    fclose(f);
    printf("  %s\n", ruta);
}


static void escena(uint32_t t, uint32_t ms) { t_actual = t; eventos(); ui_update(&p, t); correr(ms); }
/* una historia de una criatura: historia_sheet salida.ppm criatura numero [ms] */
int main(int argc, char **argv)
{
    lv_init();
    lv_tick_set_cb(tick_cb);
    lv_display_t *disp = lv_display_create(W, W);
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_XRGB8888);
    lv_display_set_buffers(disp, s_fb, NULL, sizeof s_fb, LV_DISPLAY_RENDER_MODE_DIRECT);
    lv_display_set_flush_cb(disp, flush_cb);
    lv_indev_t *in = lv_indev_create();
    lv_indev_set_type(in, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(in, touch_cb);
    ui_init(disp, on_action);
    ui_boot_done();
    uint32_t t = T0 + 17 * 3600;
    pet_init(&p, "Burbu", 3, t, 12345);
    pet_set_owner_age(&p, 30);
    pet_hatch(&p, t);
    escena(t, 800);
    ui_demo_historia(argc > 2 ? atoi(argv[2]) : 0, argc > 3 ? atoi(argv[3]) : 0);
    correr(argc > 4 ? (uint32_t)atoi(argv[4]) : 3000);
    lv_refr_now(NULL);
    FILE *f = fopen(argv[1], "wb");
    fprintf(f, "P6\n%d %d\n255\n", W, W);
    for (int i = 0; i < W * W; i++) { uint32_t c = s_fb[i]; unsigned char px[3] = {(unsigned char)(c >> 16), (unsigned char)(c >> 8), (unsigned char)c}; fwrite(px, 1, 3, f); }
    fclose(f);
    return 0;
}
