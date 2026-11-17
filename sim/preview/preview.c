#include <stdlib.h>
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

static void tocar(int x, int y)
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

static void s_dummy_comida(void) { pet_event_t e = {EV_VECINO, VEC_COMER, -1, 0, 0}; ui_show_event(&p, &e); }

static void escena(uint32_t t, uint32_t ms)
{
    t_actual = t;
    eventos();
    ui_update(&p, t);
    correr(ms);
}

int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : ".";
    lv_init();
    lv_tick_set_cb(tick_cb);
    lv_display_t *disp = lv_display_create(W, W);
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_XRGB8888);
    lv_display_set_buffers(disp, s_fb, NULL, sizeof s_fb, LV_DISPLAY_RENDER_MODE_DIRECT);
    lv_display_set_flush_cb(disp, flush_cb);
    lv_indev_t *in = lv_indev_create();
    lv_indev_set_type(in, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(in, touch_cb);
    if (getenv("MASCOTA_TAMANO")) art_tamano_set(atoi(getenv("MASCOTA_TAMANO"))); /* 100..140 % */
    ui_init(disp, on_action);
    ui_boot_done();

    uint32_t t = T0 + 17 * 3600; /* 14:00 en Chile */
    pet_init(&p, "Burbu", 3, t, 12345);
    pet_set_owner_age(&p, 30);
    pet_hatch(&p, t);
    p.pais = 1; p.aspecto_ok = 2; p.ritmo_visto = 1; p.libro = 0xFF; /* (sin los paneles de bienvenida) */
    p.vinculo = 60;
    pet_set_timezone(&p, -3 * 3600);
    pet_event_t ev = {0};

    for (int i = 0; i < NEED_COUNT; i++) p.necesidad[i] = 92;
    p.emocion = MOOD_FELIZ;
    escena(t, 1500);
    guardar(dir, "1_feliz");

    p.necesidad[NEED_SACIEDAD] = 18;
    p.emocion = MOOD_HAMBRIENTA;
    ev.type = EV_PIDE; ev.a = WANT_COMER;
    ui_show_event(&p, &ev);
    escena(t + 600, 900);
    guardar(dir, "2_hambrienta");

    tocar(233 - 111, 369); /* botón Comer */
    guardar(dir, "3_comer");
    tocar(233 - 100, 300); /* elegir comida */
    correr(900);
    guardar(dir, "4_comiendo");

    p.inventario[ITEM_SEMILLA] = 6; p.inventario[ITEM_PIEDRA] = 5;
    p.necesidad[NEED_HIGIENE] = 30;
    escena(t + 900, 400);
    tocar(233 + 111, 369); /* botón Más */
    guardar(dir, "5_estado");
    tocar(233 + 38, 233 + 180 - 4 - 16 - 32); /* Tienda */
    guardar(dir, "6_tienda");
    tocar(233 + 39 + 2 - 170 + 14, 233 - 150 + 58 + 114 + 52); /* comprar flores */
    tocar(20, 20);

    /* adulta radiante, con visita y decoración, al atardecer */
    p.etapa = STAGE_ADULTA; p.forma = FORMA_RADIANTE;
    p.decoraciones = 0x1F;
    for (int i = 0; i < NEED_COUNT; i++) p.necesidad[i] = 85;
    p.emocion = MOOD_ENAMORADA;
    p.visitantes[0].conocido = 1; p.visitantes[0].especie = 1;
    p.visita_actual = 0;
    ev.type = EV_VISITA_LLEGA; ev.a = 0; ui_show_event(&p, &ev);
    ev.type = EV_ENCUENTRO; ev.b = ENC_SE_ENAMORARON; ui_show_event(&p, &ev);
    escena(t + 4 * 3600 + 1800, 3000);
    guardar(dir, "7_visita_atardecer");

    /* enferma */
    p.visita_actual = -1;
    ev.type = EV_VISITA_SE_VA; ui_show_event(&p, &ev);
    p.enferma = 1; p.necesidad[NEED_SALUD] = 30; p.emocion = MOOD_ENFERMA;
    ev.type = EV_ENFERMO; ui_show_event(&p, &ev);
    escena(t + 3 * 3600, 2500);
    guardar(dir, "8_enferma");

    /* de noche, dormida */
    p.enferma = 0; p.dormida = 1; p.luz_apagada = 1; p.emocion = MOOD_DORMIDA;
    escena(t + 9 * 3600, 2000);
    guardar(dir, "9_noche");

    /* fase 2: el jardín con flores, mariposa, abeja, charcos y hojas */
    p.dormida = 0; p.luz_apagada = 0; p.emocion = MOOD_FELIZ; p.decoraciones = 0x10;
    p.etapa = STAGE_ADULTA; p.forma = 0;
    p.escenas_abiertas = 0x1F;
    uint32_t td = t + 24 * 3600;
    static const uint8_t TIPO[PET_PARCELAS] = {1, 1, 1, 1, 2, 0}, ETA[PET_PARCELAS] = {2, 2, 1, 2, 3, 0};
    for (int i = 0; i < PET_PARCELAS; i++) {
        p.jardin[i].tipo = TIPO[i]; p.jardin[i].etapa = ETA[i]; p.jardin[i].color = (uint8_t)i; p.jardin[i].agua = 80;
    }
    p.jardin[3].mustia = 1;
    p.charcos_hasta = td + 3600;
    p.hojas = 2;
    p.fauna_hasta[FAUNA_MARIPOSA] = p.fauna_hasta[FAUNA_ABEJA] = td + 3600;
    for (int k = 0; k < 6; k++) p.senda[k] = 200;
    escena(td, 3000);
    guardar(dir, "10_jardin");

    /* el bosque, con el zorro y el pájaro */
    for (int f = 0; f < FAUNA_COUNT; f++) p.fauna_hasta[f] = 0;
    p.escena = ESC_BOSQUE; p.lugar = LUGAR_CLARO; p.hojas = 0; p.charcos_hasta = 0;
    p.fauna_hasta[FAUNA_ZORRO] = p.fauna_hasta[FAUNA_PAJARO] = td + 7200;
    p.zorro_roba = td + 600;
    escena(td + 60, 5000);
    guardar(dir, "11_bosque");

    p.escena = ESC_LAGO;
    for (int f = 0; f < FAUNA_COUNT; f++) p.fauna_hasta[f] = 0;
    p.fauna_hasta[FAUNA_RANA] = td + 7200;
    escena(td + 120, 6000);
    escena(td + 3000, 4000);
    guardar(dir, "12_lago");

    p.escena = ESC_COLINA; p.fauna_hasta[FAUNA_RANA] = 0;
    escena(td + 3600, 6000);
    escena(td + 7000, 4000);
    guardar(dir, "13_colina");

    p.escena = ESC_CUEVA;
    escena(td + 7200, 6000);
    escena(td + 10000, 4000);
    guardar(dir, "14_cueva");

    /* fase 3: pescando en el lago */
    p.escena = ESC_LAGO; p.lugar = LUGAR_ESTANQUE;
    pet_force_weather(&p, CLIMA_DESPEJADO, td + 10000, 4 * 3600);
    escena(td + 10100, 3000);
    p.actividad = ACTV_PESCAR; p.actividad_hasta = td + 99999;
    escena(td + 10200, 12000);
    guardar(dir, "15_pesca");

    /* su amigo del barrio: juegan a la pelota */
    p.actividad = ACTV_PASEAR; p.escena = ESC_PRADO; p.lugar = LUGAR_CLARO;
    escena(td + 10300, 8000);
    p.vecino.presente = 1; p.vecino.hasta = td + 99999; p.vecino.actividad = VEC_JUGAR;
    p.actividad = ACTV_JUNTOS; p.actividad_hasta = td + 99999;
    escena(td + 10400, 4000);
    guardar(dir, "16_amigo_juegan");
    p.vecino.actividad = VEC_COMER; s_dummy_comida();
    escena(td + 10500, 3000);
    guardar(dir, "17_amigo_comen");
    p.vecino.presente = 0; p.actividad = ACTV_PASEAR;

    tocar(20, 20); /* cierra el mensaje del día si apareció */
    /* tu cumpleaños: pastel y globo */
    pet_set_owner_birthdate(&p, 0, 0, 0, td); /* inválida: no cambia nada */
    {
        int a, m, d;
        pet_local_date(&p, td + 10600, &a, &m, &d);
        pet_set_owner_birthdate(&p, d, m, a - 30, td + 10600);
    }
    p.escena = ESC_PRADO; p.lugar = LUGAR_CLARO;
    escena(td + 10600, 300);
    tocar(20, 20);
    ev.type = EV_CUMPLE; ev.a = 30; ui_show_event(&p, &ev);
    correr(2500);
    guardar(dir, "18_cumple");

    /* la casita por dentro, de noche */
    p.decoraciones |= 1u << DECOR_CASITA;
    p.dormida = 1; p.luz_apagada = 1; p.lugar = LUGAR_CASITA; p.escena = ESC_PRADO;
    escena(td + 20 * 3600, 3000);
    tocar(20, 20);
    p.escena = ESC_PRADO;
    escena(td + 20 * 3600 + 5, 1500);
    guardar(dir, "19_casita_noche");
    p.dormida = 0; p.luz_apagada = 0;
    escena(td + 26 * 3600, 400);
    /* visita a la casita (despierta, de día): se toca la casita desde el prado */
    p.escena = ESC_PRADO; p.lugar = LUGAR_CLARO;
    escena(td + 31 * 3600, 2500);
    tocar(20, 20); /* cierra lo que haya abierto */
    escena(td + 31 * 3600 + 5, 800);
    tocar(310, 150);
    escena(td + 31 * 3600 + 10, 2500);
    guardar(dir, "19b_casita_visita");

    /* fase 4: arcoíris, Halloween, nadar, trepar, su cumpleaños, diario, álbum y colecciones */
    p.dormida = 0; p.luz_apagada = 0; p.escena = ESC_PRADO; p.lugar = LUGAR_CLARO; p.edad_dueno = 30;
    uint32_t t4 = 1793404800u + 15 * 3600; /* 31/10/2026 12:00 en Chile: Halloween */
    pet_force_weather(&p, CLIMA_DESPEJADO, t4, 4 * 3600);
    p.raro = RARO_ARCOIRIS; p.raro_hasta = t4 + 3600; p.ultimo_raro = t4;
    escena(t4, 2500);
    tocar(20, 20);
    escena(t4 + 5, 1500);
    guardar(dir, "22_arcoiris_halloween");
    p.raro = 0xFF;
    p.escena = ESC_LAGO; p.actividad = ACTV_NADAR; p.actividad_hasta = t4 + 99999; p.habilidad[HAB_NADAR] = 60;
    escena(t4 + 60, 6000);
    guardar(dir, "23_nada");
    p.escena = ESC_BOSQUE; p.actividad = ACTV_TREPAR; p.habilidad[HAB_TREPAR] = 60;
    escena(t4 + 120, 9000);
    guardar(dir, "24_trepa");
    p.actividad = ACTV_PASEAR; p.escena = ESC_PRADO;
    p.nacimiento = t4 - 7 * 86400; /* hoy cumple una semana */
    escena(t4 + 180, 3000);
    guardar(dir, "25_gorro");
    p.coleccion[COL_PIEDRAS] = 0x1F; p.colecciones_completas = 1; p.coleccion[COL_CONCHAS] = 0x5; p.coleccion[COL_PLUMAS] = 0x2;
    p.coleccion[COL_HOJAS] = 0x4;
    p.meta = META_PELOTA; p.meta_objetivo = 3; p.meta_progreso = 2; p.meta_dia = 1; p.estampitas = 4;
    escena(t4 + 240, 500);
    tocar(20, 20);
    tocar(233 + 111, 369); /* Más */
    tocar(233 + 2 * 45, 373); /* Diario */
    guardar(dir, "26_diario");
    tocar(233, 77);
    guardar(dir, "27_album");
    tocar(233 + 78, 77);
    guardar(dir, "28_colecciones");
    tocar(20, 20);

    /* bienvenida y fecha de nacimiento */
    p.edad_dueno = 0; p.dueno_mes = 0;
    escena(td + 27 * 3600, 2000);
    guardar(dir, "20_bienvenida");
    escena(td + 27 * 3600 + 10, 5000);
    guardar(dir, "21_fecha");
    return 0;
}
