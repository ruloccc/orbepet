/* (1.0.13) Vista previa de las pantallas del modo Wi-Fi (firmware/main/ota_pantalla.inc) sin la placa:
 * ./mascota_ota carpeta  → ota_lista.ppm, ota_clave.ppm, ota_clave123.ppm, ota_aviso.ppm, ota_bajar.ppm, ota_msg.ppm */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "art.h"
#include "lvgl.h"
#include "version.h"

#define ANCHO_P 466
static uint32_t s_fb[ANCHO_P * ANCHO_P];
static uint32_t s_t;
static uint32_t tick_cb(void) { return s_t; }
static void flush_cb(lv_display_t *d, const lv_area_t *a, uint8_t *px) { (void)a; (void)px; lv_display_flush_ready(d); }

/* lo que ota.c le presta a las pantallas */
typedef struct { char ssid[33]; char clave[65]; } ota_wifi_t;
typedef struct { char ssid[33]; int8_t rssi; bool abierta; } red_t;
#define N_REDES 12
static ota_wifi_t s_prueba;
static red_t s_redes[N_REDES];
static int s_nredes;
static volatile bool s_cancelar, s_ya, s_buscar_otra, s_conectar_pedido;
static volatile int s_elegida = -1;
static char s_msg1[96], s_msg2[160];
static char s_nueva[33], s_nueva_nombre[64];
static lv_display_t *s_disp;
static void uso(void) {}
static bool hal_display_lock(void) { return true; }
static void hal_display_unlock(void) {}

#include "ota_pantalla.inc"

static void guardar(const char *dir, const char *n)
{
    for (int i = 0; i < 20; i++) { s_t += 10; lv_timer_handler(); }
    lv_refr_now(NULL);
    char r[256];
    snprintf(r, sizeof r, "%s/%s.ppm", dir, n);
    FILE *f = fopen(r, "wb");
    if (!f) return;
    fprintf(f, "P6\n%d %d\n255\n", ANCHO_P, ANCHO_P);
    const float R = ANCHO_P / 2.0f;
    for (int y = 0; y < ANCHO_P; y++)
        for (int x = 0; x < ANCHO_P; x++) {
            float dx = x + 0.5f - R, dy = y + 0.5f - R;
            uint32_t c = s_fb[y * ANCHO_P + x];
            unsigned char px[3] = {(unsigned char)(c >> 16), (unsigned char)(c >> 8), (unsigned char)c};
            if (dx * dx + dy * dy > R * R) px[0] = px[1] = px[2] = 40;
            fwrite(px, 1, 3, f);
        }
    fclose(f);
}

int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : ".";
    lv_init();
    lv_tick_set_cb(tick_cb);
    s_disp = lv_display_create(ANCHO_P, ANCHO_P);
    lv_display_set_color_format(s_disp, LV_COLOR_FORMAT_XRGB8888);
    lv_display_set_buffers(s_disp, s_fb, NULL, sizeof s_fb, LV_DISPLAY_RENDER_MODE_DIRECT);
    lv_display_set_flush_cb(s_disp, flush_cb);
    const char *R[] = {"Casa Rulo", "VTR-1234567", "Familia_Pérez 5G", "invitados", "Starlink", "TP-Link_9F2A", "HUAWEI-B310"};
    for (int i = 0; i < 7; i++) { snprintf(s_redes[i].ssid, 33, "%s", R[i]); s_redes[i].rssi = (int8_t)(-45 - i * 7); }
    s_nredes = 7;
    pantalla_lista(); guardar(dir, "ota_lista");
    snprintf(s_prueba.ssid, 33, "%s", "Casa Rulo"); snprintf(s_prueba.clave, 65, "%s", "Mi#Clave_2026");
    s_ver_clave = true; s_modo_tec = 0; pantalla_clave(NULL); guardar(dir, "ota_clave");
    s_modo_tec = 2; pantalla_clave("NO PUDE ENTRAR: REVISA LA CLAVE"); guardar(dir, "ota_clave123");
    s_modo_tec = 1; s_ver_clave = false; pantalla_clave(NULL); guardar(dir, "ota_claveABC");
    s_modo_tec = 3; pantalla_clave(NULL); guardar(dir, "ota_clave_signos");
    snprintf(s_nueva, 33, "1.0.14"); snprintf(s_nueva_nombre, 64, "%s", "LA LUNA LLENA");
    pantalla_aviso(4); guardar(dir, "ota_aviso");
    pantalla_bajar(); bajar_progreso(42); guardar(dir, "ota_bajar");
    pantalla_msg("CONECTANDO...", "Casa Rulo", true, true); guardar(dir, "ota_msg");
    return 0;
}
