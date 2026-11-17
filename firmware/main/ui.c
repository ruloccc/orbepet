/*
 * Interfaz del reloj (LVGL 9): un mundo en pixel art donde vive la mascota.
 *
 *   - sin números en pantalla: el estado se lee mirándola (cara, orejas, cola,
 *     postura, forma de caminar) y en globos de pensamiento con la causa
 *   - la mascota decide sola qué hacer y adónde ir (el motor elige; aquí se anima)
 *   - se acaricia en distintas partes del cuerpo; también a las visitas (¡celos!)
 *   - estaciones, clima con partículas y truenos
 *   - modo niño (sin textos) y modo adulto (con detalles y estadísticas)
 *   - paneles: comer, estado, rincón (decorar con lo encontrado), amigos, ajustes
 *
 * Todo el arte sale de art.c; aquí solo se arma la escena y se anima.
 */
#ifdef ESP_PLATFORM
#include "esp_attr.h"
#include "esp_heap_caps.h"
#include "src/misc/cache/instance/lv_image_cache.h"
#endif
#ifndef EXT_RAM_BSS_ATTR
#define EXT_RAM_BSS_ATTR
#endif
#include "ui.h"
#include "version.h" /* (1.0.09) FW_VERSION y FW_NOMBRE: Ajustes → Versión */
#include <time.h>
#include "sfx.h"
#include "foto.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "art.h"
#include "src/misc/cache/instance/lv_image_cache.h" /* (v96) lv_image_cache_drop */

#define W 466
#define CX 233
#define TICK_MS 80

#define C_TINTA 0x2A2238
#define C_CREMA 0xFFF8E8
#define C_BOTON 0x2E3448
#define C_BOTON_P 0x56608A
#define C_BLANCO 0xFFFFFF
#define C_GRIS 0x4F5270     /* gris oscuro: se lee sobre el vidrio claro */
#define C_GRIS_CLARO 0x9AA0B8 /* gris claro: para lo bloqueado sobre botones oscuros */
#define C_ROJO 0xE0485E
#define C_VIOLETA 0x6A4FA0
#define C_VERDE 0x3E8A5A

static ui_action_cb_t s_on_action;
static const pet_t *s_pet;
/* niña o niño: cambia las marcas {a} {la} {lo} {ella} {guardiana} de un texto (ver pet_gender_text) */
static const char *gtxt(const char *s)
{
    static char buf[4][700];
    static int k;
    if (!s || !strchr(s, '{')) return s;
    k = (k + 1) & 3;
    return pet_gender_text(s_pet, s, buf[k], (int)sizeof buf[k]);
}
static uint32_t s_ms;
static uint32_t s_rng = 0x1234567u;
static uint32_t s_ultimo_t; /* hora UTC de la última actualización */
static bool s_nino;         /* modo infante: sin textos */
static bool s_suave;        /* modo niño o infante: juegos más fáciles y todo más marcado */
static uint32_t s_hora_pidio; /* cuándo se pidió por última vez la fecha (no insistir) */
static bool s_hora_dudosa;  /* el reloj perdió la hora (se quedó sin batería): hay que preguntarla */
#define ICO_MUSGO ICO_OBJETO(37) /* (v38) un puñado de musgo: el dibujo del musgo de terciopelo */
static bool s_pez_on, s_pez_pedido; /* v89: la pesca con minijuego está abierta / la pediste tú (tocando el bote) */
static uint32_t s_pez_espera; /* v90: la pesca empieza cuando ella ya caminó al bote y subió (no aparece adentro) */
static void pez_empezar(const pet_t *m);
static bool pez_resultado(int que, int comio);
static void animar_pesca_juego(void);
static bool pez_quiere_soplo(void);
static void pez_soplo(void);

static uint32_t azar(void)
{
    s_rng ^= s_rng << 13; s_rng ^= s_rng >> 17; s_rng ^= s_rng << 5;
    return s_rng;
}
static float azar01(void) { return (float)(azar() >> 8) / 16777216.0f; }

/* v89: LVGL repinta un objeto CADA vez que se le pone un estilo, aunque sea el mismo valor que ya tenía. Las animaciones de la
 * interfaz ponen estilos en cada latido (opacidad, color, posición...), así que se repintaban cosas quietas todo el tiempo.
 * El enlazador desvía lv_obj_set_local_style_prop hacia aquí (-Wl,--wrap): si el valor no cambia, no se hace nada. */
void __real_lv_obj_set_local_style_prop(lv_obj_t *obj, lv_style_prop_t prop, lv_style_value_t value, lv_style_selector_t selector);
void __wrap_lv_obj_set_local_style_prop(lv_obj_t *obj, lv_style_prop_t prop, lv_style_value_t value, lv_style_selector_t selector)
{
    lv_style_value_t antes;
    if (obj && lv_obj_get_local_style_prop(obj, prop, &antes, selector) == LV_STYLE_RES_FOUND && memcmp(&antes, &value, sizeof value) == 0) {
        /* (y lo que se ve también es ese valor: si hay una transición en curso, se deja que el valor la corte como antes) */
        const lv_style_value_t ahora = lv_obj_get_style_prop(obj, lv_obj_style_get_selector_part(selector), prop);
        if (memcmp(&ahora, &value, sizeof value) == 0 && lv_obj_style_get_selector_state(selector) == LV_STATE_DEFAULT) return;
    }
    __real_lv_obj_set_local_style_prop(obj, prop, value, selector);
}
/* v89: cambiar el orden de un objeto (ponerlo delante o detrás de otro) hacía que LVGL repintara a su PADRE entero. Casi todo
 * vive en s_mundo (la pantalla completa), y de noche algo se reordena en cada latido: se repintaban los 217 mil pixeles en cada
 * cuadro (en la placa, ~230 ms por cuadro: por eso de noche todo iba a tirones). Al cambiar el orden solo cambia lo que el objeto
 * tapa o deja ver, así que basta con repintar el objeto mismo. */
void __real_lv_obj_move_to_index(lv_obj_t *obj, int32_t index);
void __wrap_lv_obj_move_to_index(lv_obj_t *obj, int32_t index)
{
    lv_display_t *d = obj ? lv_obj_get_display(obj) : NULL;
    if (!d || !lv_obj_get_parent(obj)) { __real_lv_obj_move_to_index(obj, index); return; }
    const int32_t antes = lv_obj_get_index(obj);
    const bool inv = lv_display_is_invalidation_enabled(d);
    if (inv) lv_display_enable_invalidation(d, false);
    __real_lv_obj_move_to_index(obj, index);
    if (inv) lv_display_enable_invalidation(d, true);
    if (inv && lv_obj_get_index(obj) != antes) lv_obj_invalidate(obj);
}
static void ocultar(lv_obj_t *o, bool si)
{
    if (si) lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_remove_flag(o, LV_OBJ_FLAG_HIDDEN);
}

static lv_obj_t *caja(lv_obj_t *padre)
{
    lv_obj_t *o = lv_obj_create(padre);
    lv_obj_remove_style_all(o);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE);
    return o;
}

/* una forma redonda de adorno para los fondos pintados */
static lv_obj_t *adorno(lv_obj_t *padre, int x, int y, int w, int h, uint32_t col, int opa, bool redondo)
{
    lv_obj_t *o = caja(padre);
    lv_obj_set_size(o, w, h); lv_obj_set_pos(o, x, y);
    lv_obj_set_style_radius(o, redondo ? LV_RADIUS_CIRCLE : 2, 0);
    lv_obj_set_style_bg_color(o, lv_color_hex(col), 0);
    lv_obj_set_style_bg_opa(o, (lv_opa_t)opa, 0);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_user_data(o, (void *)(uintptr_t)0xAD0); /* adorno de fondo: el vidrio redondo lo recorta */
    return o;
}
static void flor_adorno(lv_obj_t *padre, int x, int y, uint32_t petalo)
{
    adorno(padre, x + 3, y + 8, 2, 10, 0x2F7A33, LV_OPA_COVER, false);
    adorno(padre, x, y + 3, 4, 4, petalo, LV_OPA_COVER, true);
    adorno(padre, x + 6, y + 3, 4, 4, petalo, LV_OPA_COVER, true);
    adorno(padre, x + 3, y, 4, 4, petalo, LV_OPA_COVER, true);
    adorno(padre, x + 3, y + 6, 4, 4, petalo, LV_OPA_COVER, true);
    adorno(padre, x + 3, y + 4, 4, 4, 0xFFE15A, LV_OPA_COVER, true);
}
static void arbol_adorno(lv_obj_t *padre, int x, int base, int alto, uint32_t hoja, uint32_t claro)
{
    adorno(padre, x - alto / 12, base - alto / 2, alto / 6 + 2, alto / 2, 0x6B4423, LV_OPA_COVER, false);
    adorno(padre, x - alto / 3, base - alto * 3 / 4, alto * 2 / 3, alto / 2, hoja, LV_OPA_COVER, true);
    adorno(padre, x - alto / 5, base - alto, alto * 2 / 5 + 8, alto * 2 / 5, hoja, LV_OPA_COVER, true);
    adorno(padre, x - alto / 4, base - alto * 7 / 10, alto / 4, alto / 5, claro, LV_OPA_70, true);
}

static void poner_src(lv_obj_t *img, const lv_image_dsc_t *d)
{
    lv_image_set_src(img, NULL);
    lv_image_set_src(img, d);
}

/* un cartel (fondo redondeado) con un texto adentro: la imagen va como hija, así no se recorta el borde */
static lv_obj_t *cartel_de(lv_obj_t *padre, lv_obj_t **img, uint32_t color, lv_opa_t opa)
{
    lv_obj_t *o = caja(padre);
    lv_obj_set_style_bg_color(o, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(o, opa, 0);
    lv_obj_set_style_radius(o, 12, 0);
    *img = lv_image_create(o);
    return o;
}
static lv_obj_t *cartel(lv_obj_t *padre, lv_obj_t **img) { return cartel_de(padre, img, 0xFFFFFF, LV_OPA_80); }
static void cartel_poner(lv_obj_t *o, lv_obj_t *img, const art_img_t *a)
{
    poner_src(img, &a->dsc);
    lv_obj_set_size(o, (int)a->dsc.header.w + 20, (int)a->dsc.header.h + 12);
    lv_obj_center(img);
}

static void accion(int a, int arg)
{
    if (s_on_action) s_on_action(a, arg);
}

/* ======================================================================
 * Lugares del prado (donde están los pies de la mascota)
 * ==================================================================== */

/* cada zona tiene sus propios lugares (art.c los dibuja ahí): ver ART_LUGAR_XY */
static int s_escena = -1;     /* la zona que se ve (durante el viaje puede no ser la del motor) */
static bool s_interior;       /* adentro de la casita, durmiendo en su camita */
#define LXY(l) ART_LUGAR_XY[s_interior ? ART_ESCENA_CASA : (s_escena > 0 ? s_escena : 0)][(l)]
#define Y_MIN 160
#define Y_MAX 318

/* terreno de cada zona: la montaña de la Colina, la orilla del Lago y la costa de la Isla no se pisan */
static const int16_t COLINA_YMIN[59] = {156,158,156,153,150,150,152,153,155,159,164,169,172,174,175,174,174,174,175,180,188,196,202,205,208,211,213,211,209,208,208,210,215,219,219,213,205,198,193,188,184,182,180,178,177,177,175,169,161,155,151,150,150,152,155,156,156,157,157}; /* y mínima de los pies cada 8 px de x (la base de la montaña) */
static bool lago_congelado(const pet_t *p);
static int terreno_ymin(float x)
{
    if (s_interior) return Y_MIN;
    if (s_escena == ESC_LAGO && s_pet && lago_congelado(s_pet)) return 80; /* el lago helado se camina: ya no hay agua que no se pise */
    int ym = Y_MIN;
    if (s_escena == ESC_COLINA) {
        float fx = x < 0 ? 0 : x / 8.0f; int i = (int)fx; if (i > 58) i = 58;
        int j = i < 58 ? i + 1 : i; float t = fx - (float)i;
        ym = (int)((float)COLINA_YMIN[i] * (1.0f - t) + (float)COLINA_YMIN[j] * t);
    } else if (s_escena == ESC_LAGO) { /* la orilla: bahía al centro y un promontorio a la derecha */
        float v = 208.0f - 58.0f * ((x - 230.0f) / 250.0f) * ((x - 230.0f) / 250.0f) - 34.0f * expf(-((x - 300.0f) / 38.0f) * ((x - 300.0f) / 38.0f));
        ym = (int)(v + 6.0f);
        if (x > 150.0f && x < 245.0f) ym = 214;
    }
    return ym > Y_MIN ? ym : Y_MIN;
}

#define MUELLE_X 355.0f /* el muelle del lago: de la orilla (y 192) hasta donde amarra el bote (y 118) */
#define MUELLE_Y0 96
#define MUELLE_PUNTA_Y 118.0f
#define MUELLE_BASE_Y 194.0f
static bool s_picnic_on, s_picnic_tapa;
static void limitar(float *x, float *y)
{
    if (s_picnic_on) { /* comiendo en el picnic: solo se mueve DENTRO del mantel */
        if (*x < 160.0f) *x = 160.0f;
        if (*x > 306.0f) *x = 306.0f;
        if (*y < 338.0f) *y = 338.0f;
        if (*y > 372.0f) *y = 372.0f;
        return;
    }
    int ym = terreno_ymin(*x);
    if (s_escena == ESC_LAGO && !s_interior && *x > 343.0f && *x < 369.0f && ym > MUELLE_Y0) ym = MUELLE_Y0; /* el muelle del lago se pisa de la orilla al bote */
    if (*y < ym) *y = (float)ym;
    if (*y > Y_MAX) *y = Y_MAX;
    if (s_escena == ESC_ISLA && !s_interior) { /* la costa del oeste: solo se pisa el muelle (de la orilla al bote) */
        float yo = *y < 160 ? 160 : *y;
        float xw = yo < 190 ? 100.0f - (yo - 160.0f) * 0.4f : (yo < 226 ? 88.0f + (yo - 190.0f) * 0.11f : (yo < 266 ? 92.0f + (yo - 226.0f) * 0.35f : 106.0f + (yo - 266.0f) * 0.18f));
        xw += 8.0f;
        if (*x < xw) {
            float yp = 266.0f + (114.0f - *x) * 0.375f;
            if (*x > 36.0f && fabsf(*y - yp) < 9.0f) { /* en el muelle */ }
            else *x = xw;
        }
    }
    /* las copas de los árboles grandes: no se camina sobre ellas (a menos que los escale, y eso es otro cuento) */
    if (!s_interior) {
        static const int16_t COPAS[ESC_COUNT][4] = {{0}, {0}, {120, 205, 58, 50}, {98, 165, 52, 48}, {0}, {292, 105, 80, 70}, {0}, {0}};
        const int16_t *cp = COPAS[s_escena < ESC_COUNT ? s_escena : 0];
        if (cp[2]) {
            const float ex = (*x - cp[0]) / cp[2];
            if (fabsf(ex) < 1.0f) {
                const float fondo = cp[1] + cp[3] * sqrtf(1.0f - ex * ex), tope = cp[1] - cp[3] * sqrtf(1.0f - ex * ex);
                if (*y > tope && *y < fondo) *y = fondo; /* baja hasta el pie del árbol */
            }
        }
    }
    if (!s_interior && s_escena == ESC_CLARO) { /* (v98) la casa del rey y la poza no se pisan (se baja hasta el frente) */
        static const int16_t OBST[2][4] = {{204, 128, 74, 56}, {339, 204, 50, 32}};
        for (int k = 0; k < 2; k++) {
            const float ex = (*x - OBST[k][0]) / OBST[k][2];
            if (fabsf(ex) >= 1.0f) continue;
            const float h = OBST[k][3] * sqrtf(1.0f - ex * ex), fondo = OBST[k][1] + h, tope = OBST[k][1] - h;
            if (*y > tope && *y < fondo) *y = fondo;
        }
    }
    if (!s_interior && s_escena == ESC_MIRADOR) { /* (v98) solo la cima: más allá, las laderas caen a pique */
        const float mx = *x - 233.0f, my = (*y - 222.0f) * 1.05f, md = sqrtf(mx * mx + my * my);
        if (md > 112.0f) { *x = 233.0f + mx / md * 112.0f; *y = 222.0f + my / md * 112.0f / 1.05f; }
    }
    float dx = *x - CX, dy = *y - CX, d = sqrtf(dx * dx + dy * dy);
    if (d > 175.0f && !(s_escena == ESC_ISLA && *x < 120.0f)) { *x = CX + dx / d * 175.0f; *y = CX + dy / d * 175.0f; }
}
/* ¿ese punto del suelo es agua? (no se puede cavar ahí) */
static bool es_agua(float x, float y)
{
    if (s_interior) return false;
    if (s_escena == ESC_LAGO && y < (float)terreno_ymin(x) + 6.0f) return true;
    if (s_escena == ESC_PRADO) { float ex = (x - 106.0f) / 78.0f, ey = (y - 300.0f) / 48.0f; return ex * ex + ey * ey < 1.0f; }
    if (s_escena == ESC_CLARO) { float ex = (x - 339.0f) / 50.0f, ey = (y - 204.0f) / 32.0f; return ex * ex + ey * ey < 1.0f; } /* (v98) la poza del rey */
    if (s_escena == ESC_ISLA) { float lx = x, ly = y; limitar(&lx, &ly); return fabsf(lx - x) > 3.0f; }
    return false;
}

/* ======================================================================
 * Criaturas en el mundo
 * ==================================================================== */

typedef enum { M_LIBRE, M_COME, M_JUEGA, M_BANO, M_SALTA, M_NIEGA, M_TRAE, M_SALE, M_CAMINA, M_QUIETA, M_VA, M_ZONA_SALE, M_ZONA_ENTRA, M_MAREADA, M_ABRAZO, M_TRUCO, M_DUDA, M_CHARCO, M_OFRENDA, M_CRISTAL, M_REVERENCIA, M_BAILA, M_ROBOT } modo_t;
/* el baile con la caja de música: medio segundo por pulso, 16 pulsos */
#define BAILE_PULSO_MS 500u
#define BAILE_MS (16u * BAILE_PULSO_MS)
static bool s_baile_on;
static uint32_t s_baile_desde;
static int s_baile_aciertos, s_baile_ult_pulso = -1, s_baile_nota = -1;

typedef struct {
    lv_obj_t *sombra, *img;
    art_img_t sprite;
    float x, y, tx, ty;
    art_pose_t pose;
    bool visible;
    uint32_t clave;
    int grietas;     /* huevo: 0..3 */
    modo_t modo;
    uint32_t modo_hasta, proximo_paseo;
    int encuentro;   /* encounter_t pendiente de mostrar, -1 = ninguno */
    int amigo;       /* visita de un amigo: índice + 1 */
    uint32_t se_va;
} criatura_t;

static criatura_t s_mascota, s_visita;
static void objx_uso(int param, int cod, int id, int hx, int hy);
static void objx_pose(criatura_t *c, const pet_t *p);
static void objx_robot(criatura_t *c, art_pose_t *ps, float *sx, float *sy);
static void animar_objx(void);
static lv_obj_t *s_mundo;
static void mascota_modo(modo_t m, uint32_t ms);
static int s_vec_comida = -1, s_vec_regalo; /* lo que comen juntos / lo que le regaló su amigo */
static lv_obj_t *s_vec_mantel; /* el mantel de picnic donde comen juntos */
static bool s_pais_enviado;     /* ya eligió el país (esperando que el motor lo guarde) */
/* juegos (ver "Interacción directa") */
typedef enum { JG_NADA = 0, JG_PELOTA, JG_ESCONDIDAS, JG_FRUTAS, JG_HONGOS } jg_t;
static int s_juego = JG_NADA, s_juego_pedido = -1;
static void empezar_comida(int f);
static void picnic_abrir(void);
static void panel_temporada(void);
static art_icono_t ico_comida(int f)
{
    static const art_icono_t C[FOOD_COUNT] = {ICO_COMIDA, ICO_FRUTA, ICO_MIEL, ICO_TAZA, ICO_VASO};
    if (f >= PET_COMIDA_EST_BASE && f < PET_COMIDA_EST_BASE + PET_COMIDAS_EST) return (art_icono_t)(ICO_CE_FRUTILLA + (f - PET_COMIDA_EST_BASE));
    return C[(f < 0 ? 0 : f) % FOOD_COUNT];
}
static void panel_jugar(void);
static bool esco_toque(int x, int y);

static void crear_criatura(criatura_t *c)
{
    memset(c, 0, sizeof(*c));
    c->sombra = caja(s_mundo);
    lv_obj_set_style_bg_color(c->sombra, lv_color_hex(0x10301A), 0);
    lv_obj_set_style_bg_opa(c->sombra, LV_OPA_30, 0);
    lv_obj_set_style_radius(c->sombra, LV_RADIUS_CIRCLE, 0);
    c->img = lv_image_create(s_mundo);
    c->x = LXY(0)[0]; c->y = LXY(0)[1];
    c->tx = c->x; c->ty = c->y;
    c->pose.escala = 3;
    c->pose.etapa = STAGE_ADULTA;
    c->encuentro = -1;
    c->clave = 0xFFFFFFFFu;
}

static int lado(const criatura_t *c) { return c->sprite.dsc.header.w > 0 && c->pose.etapa > 0 ? (int)c->sprite.dsc.header.w : ART_CRIATURA_LADO * c->pose.escala; }

/* (declaradas arriba) tapa: hay fondo completo, el mundo de atrás se oculta */
static lv_obj_t *s_picnic_pintura, *s_picnic_foto; /* dando de comer en el picnic: la mascota se ve al doble */
static void colocar(criatura_t *c, float sx, float sy)
{
    int l = lado(c);
    lv_obj_set_pos(c->img, (int)(c->x - l / 2.0f + sx), (int)(c->y - l + c->pose.escala + sy));
    int sw = l * 6 / 10, sh = c->pose.escala * 3;
    lv_obj_set_size(c->sombra, sw, sh);
    lv_obj_set_pos(c->sombra, (int)(c->x - sw / 2.0f), (int)(c->y - sh / 2.0f));
}

static void dibujar(criatura_t *c)
{
    const art_pose_t *p = &c->pose;
    uint32_t clave = (uint32_t)p->especie | (uint32_t)p->forma << 3 | (uint32_t)p->escala << 5 | (uint32_t)p->expr << 9 |
                     (uint32_t)p->boca << 13 | (uint32_t)p->frame << 14 | (uint32_t)p->espejo << 15 |
                     (uint32_t)p->enferma << 16 | (uint32_t)p->etapa << 17 | (uint32_t)c->grietas << 20 |
                     (uint32_t)(p->orejas + 1) << 22 | (uint32_t)(p->cola + 1) << 24 | (uint32_t)(p->postura + 2) << 26;
    clave ^= (uint32_t)(p->especie >> 3) * 0x9E3779B1u ^ (uint32_t)(p->ropa[0] | p->ropa[1] << 4 | p->ropa[2] << 8 | p->ropa[3] << 12) * 0x85EBCA77u;
    clave ^= art_criaturas_version() * 0xC2B2AE35u; /* cambiaron su dibujo en el editor */
    clave ^= (uint32_t)p->sucio * 0x27D4EB2Fu ^ (uint32_t)p->capa * 0x165667B1u;
    if (p->ropa[0] | p->ropa[1] | p->ropa[2] | p->ropa[3]) clave ^= art_ambiente_clave() * 0x61C88647u; /* la ropa mágica cambia con el ambiente */
    const bool grande = c == &s_mascota && s_picnic_on; /* en el picnic se dibuja ya 1,5 veces más grande (el zoom en vivo era lo lento) */
    if (grande) clave ^= 0x7A3C91E5u;
    clave ^= (uint32_t)art_tamano_get() * 0x2545F491u; /* cambió el tamaño de las criaturas */
    if (clave == c->clave) return;
    c->clave = clave;
    art_pose_t pg = *p;
    if (grande) pg.escala = p->escala * 3 / 2;
    if (p->etapa == 0) art_huevo(&c->sprite, p->especie, pg.escala, c->grietas, p->frame ? (p->espejo ? -1 : 1) : 0);
    else art_criatura_pose(&c->sprite, &pg);
    poner_src(c->img, &c->sprite.dsc);
}

/* muy sucia: dos moscas chiquitas le dan vueltas a la cabeza */
static lv_obj_t *s_mosca[2];
static void animar_moscas(const criatura_t *c)
{
    for (int i = 0; i < 2; i++) {
        if (!s_mosca[i]) {
            s_mosca[i] = lv_obj_create(lv_obj_get_parent(c->img));
            lv_obj_remove_style_all(s_mosca[i]);
            lv_obj_set_size(s_mosca[i], 5, 5);
            lv_obj_set_style_radius(s_mosca[i], 2, 0);
            lv_obj_set_style_bg_color(s_mosca[i], lv_color_hex(0x2A2238), 0);
            lv_obj_set_style_bg_opa(s_mosca[i], LV_OPA_COVER, 0);
            lv_obj_set_style_border_width(s_mosca[i], 1, 0);
            lv_obj_set_style_border_color(s_mosca[i], lv_color_hex(0xDDE6FF), 0);
            lv_obj_set_style_border_opa(s_mosca[i], LV_OPA_50, 0);
            lv_obj_remove_flag(s_mosca[i], LV_OBJ_FLAG_CLICKABLE);
        }
        bool ver = c->visible && c->pose.sucio >= 3;
        if (lv_obj_has_flag(s_mosca[i], LV_OBJ_FLAG_HIDDEN) == ver) { if (ver) lv_obj_remove_flag(s_mosca[i], LV_OBJ_FLAG_HIDDEN); else lv_obj_add_flag(s_mosca[i], LV_OBJ_FLAG_HIDDEN); }
        if (!ver) continue;
        float a = (float)s_ms / (i ? 260.0f : 340.0f) + i * 2.4f;
        int r = c->pose.escala * 7;
        lv_obj_set_pos(s_mosca[i], (int)(c->x + cosf(a) * r), (int)(c->y - lado(c) + c->pose.escala * 3 + sinf(a * 1.7f) * r * 0.45f));
        lv_obj_move_foreground(s_mosca[i]);
    }
}

/* la Guardiana: una corona de luz sobre la cabeza (un aro dorado con una gema verde que respira), cinco luciérnagas
 * que le dan vueltas despacio y, de vez en cuando, una hojita de luz que cae (para siempre) */
static void chispa(int x, int y, uint32_t color, float vx, float vy); /* ui_magia.inc */
static void onda(int x, int y, uint32_t color, int rmax, bool plano, uint32_t retraso_ms); /* ui_magia.inc */
#define N_AURA_G 5
static bool s_rito_on; /* la ceremonia de la Guardiana está en curso (más abajo) */
static uint32_t s_rito_desde;
#define RITO_FLASH 7200 /* ms en que se revela la capa */
static lv_obj_t *s_aura_g[N_AURA_G], *s_halo_g, *s_halo_glow, *s_halo_gema;
static void animar_guardiana(const criatura_t *c, bool guardiana)
{
    lv_obj_t *padre = lv_obj_get_parent(c->img);
    if (!s_halo_g) {
        s_halo_glow = lv_obj_create(padre); /* el resplandor suave detrás de la cabeza */
        lv_obj_remove_style_all(s_halo_glow);
        lv_obj_set_style_radius(s_halo_glow, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(s_halo_glow, lv_color_hex(0xFFF0A8), 0);
        lv_obj_set_style_bg_opa(s_halo_glow, 46, 0);
        lv_obj_set_style_shadow_width(s_halo_glow, 28, 0);
        lv_obj_set_style_shadow_color(s_halo_glow, lv_color_hex(0xFFE27A), 0);
        lv_obj_set_style_shadow_opa(s_halo_glow, 120, 0);
        lv_obj_remove_flag(s_halo_glow, LV_OBJ_FLAG_CLICKABLE);
        s_halo_g = lv_obj_create(padre); /* el aro */
        lv_obj_remove_style_all(s_halo_g);
        lv_obj_set_style_radius(s_halo_g, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(s_halo_g, 3, 0);
        lv_obj_set_style_border_color(s_halo_g, lv_color_hex(0xF4CE5A), 0);
        lv_obj_set_style_border_opa(s_halo_g, LV_OPA_COVER, 0);
        lv_obj_set_style_shadow_width(s_halo_g, 10, 0);
        lv_obj_set_style_shadow_color(s_halo_g, lv_color_hex(0xFFF6C8), 0);
        lv_obj_remove_flag(s_halo_g, LV_OBJ_FLAG_CLICKABLE);
        s_halo_gema = lv_obj_create(padre); /* la gema verde del frente */
        lv_obj_remove_style_all(s_halo_gema);
        lv_obj_set_size(s_halo_gema, 9, 9);
        lv_obj_set_style_radius(s_halo_gema, 4, 0);
        lv_obj_set_style_bg_color(s_halo_gema, lv_color_hex(0x39E08C), 0);
        lv_obj_set_style_bg_opa(s_halo_gema, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(s_halo_gema, 2, 0);
        lv_obj_set_style_border_color(s_halo_gema, lv_color_hex(0xF4CE5A), 0);
        lv_obj_set_style_shadow_width(s_halo_gema, 10, 0);
        lv_obj_set_style_shadow_color(s_halo_gema, lv_color_hex(0xB8FF8A), 0);
        lv_obj_remove_flag(s_halo_gema, LV_OBJ_FLAG_CLICKABLE);
    }
    lv_obj_t *const halo[3] = {s_halo_glow, s_halo_g, s_halo_gema};
    const bool ver = guardiana && c->visible && c->pose.etapa != STAGE_HUEVO && !(s_rito_on && s_ms - s_rito_desde < RITO_FLASH);
    for (int i = 0; i < 3; i++) ocultar(halo[i], !ver);
    for (int i = 0; i < N_AURA_G; i++) {
        if (!s_aura_g[i]) {
            s_aura_g[i] = lv_obj_create(padre);
            lv_obj_remove_style_all(s_aura_g[i]);
            lv_obj_set_size(s_aura_g[i], i & 1 ? 5 : 7, i & 1 ? 5 : 7);
            lv_obj_set_style_radius(s_aura_g[i], 4, 0);
            lv_obj_set_style_bg_color(s_aura_g[i], lv_color_hex(i & 1 ? 0xB8FF8A : 0xFFE27A), 0);
            lv_obj_set_style_bg_opa(s_aura_g[i], LV_OPA_COVER, 0);
            lv_obj_set_style_shadow_width(s_aura_g[i], 12, 0);
            lv_obj_set_style_shadow_color(s_aura_g[i], lv_color_hex(i & 1 ? 0x7AFFB0 : 0xFFD24A), 0);
            lv_obj_remove_flag(s_aura_g[i], LV_OBJ_FLAG_CLICKABLE);
        }
        ocultar(s_aura_g[i], !ver);
    }
    if (!ver) return;
    const int esc = c->pose.escala;
    const int cab_y = (int)(c->y - lado(c) + esc * 3) + 2 - (int)(sinf((float)s_ms / 700.0f) * 2); /* sobre la cabeza, flota */
    const int hw = esc * 8 + 4, hh = hw / 4 + 3;
    const float pul = 0.5f + 0.5f * sinf((float)s_ms / 520.0f);
    lv_obj_set_size(s_halo_glow, hw + 26, hh + 22);
    lv_obj_set_pos(s_halo_glow, (int)c->x - (hw + 26) / 2, cab_y - (hh + 22) / 2);
    lv_obj_set_style_bg_opa(s_halo_glow, (lv_opa_t)(34 + 30 * pul), 0);
    lv_obj_set_size(s_halo_g, hw, hh);
    lv_obj_set_pos(s_halo_g, (int)c->x - hw / 2, cab_y - hh / 2);
    lv_obj_set_style_border_opa(s_halo_g, (lv_opa_t)(200 + 55 * pul), 0);
    lv_obj_set_pos(s_halo_gema, (int)c->x - 5, cab_y + hh / 2 - 6);
    lv_obj_set_style_shadow_opa(s_halo_gema, (lv_opa_t)(110 + 120 * pul), 0);
    lv_obj_move_foreground(s_halo_glow);
    lv_obj_move_foreground(s_halo_g);
    lv_obj_move_foreground(s_halo_gema);
    for (int i = 0; i < N_AURA_G; i++) {
        const float a = (float)s_ms / (1500.0f + (float)i * 190.0f) + (float)i * 6.2832f / N_AURA_G;
        const int r = esc * (10 + (i % 3) * 3);
        const int y = (int)(c->y - lado(c) / 2 + sinf(a) * r * 0.32f - (float)((i * 11) % 14));
        lv_obj_set_pos(s_aura_g[i], (int)(c->x + cosf(a) * r) - 3, y - 3);
        lv_obj_set_style_opa(s_aura_g[i], (lv_opa_t)(170 + (int)(sinf((float)s_ms / 300.0f + (float)i) * 80)), 0);
        if (sinf(a) > 0) lv_obj_move_foreground(s_aura_g[i]); /* pasa por delante y por detrás */
    }
    if (s_ms % 900 < TICK_MS) /* una hojita de luz que cae desde la corona */
        chispa((int)c->x - hw / 2 + (int)(azar() % (uint32_t)hw), cab_y, (azar() & 1) ? 0xB8FF8A : 0xFFF6C8, (azar01() - 0.5f) * 0.6f, 1.1f);
}

static void flotar(int ico, const char *txt, uint32_t color, int x, int y, uint32_t retraso);
static void evento_animal(const pet_event_t *e, int hx, int hy);
static void evento_magia(const pet_event_t *e, int hx, int hy); /* ui_magia.inc */
static void animar_mundo23(void);
static void magia_update(const pet_t *p);
static void mensajero_nueva(void);
static void botella_nada(int f); /* ui_magia.inc */
static void regadera_empezar(int i, int px, int py); /* ui_magia.inc */
static void panel_historia(int c, int k);
static void panel_libro_hist(void); /* ui_magia.inc */
static int hist_total(void);
static void tesoros_magia(lv_obj_t *ar);
/* Tesoros: se cuentan todas las filas, pero solo se crean las de la página (con todo encontrado, la lista entera se comía la RAM interna) */
static int s_ts_idx, s_ts_lo, s_ts_hi;
static bool ts_toca(void) { int i = s_ts_idx++; return i >= s_ts_lo && i < s_ts_hi; }
static bool s_pantalla_visible; /* ui_magia.inc */
static lv_timer_t *s_tick_timer;  /* el reloj de las animaciones: con la pantalla apagada late más despacio */
static int s_fie_tipo;
static bool s_ny_gorro; /* ui_ny.inc */
static uint32_t s_sacude_desde; /* sacudiste el reloj: el mundo se estremece un momento */
/* (fw 103) tocar animales: el motor decide y aquí se ve (más abajo) */
static int s_toq_res;
/* los toques seguidos a un animal: la ventana es larga (8 s), porque entre uno y otro alcanzas a leer lo que dice */
static int toques_animal(int clave)
{
    static int cl = -1, n;
    static uint32_t ult;
    if (clave == cl && s_ms - ult < 8000u) n++;
    else { n = 1; cl = clave; }
    ult = s_ms;
    return n;
}
static void toque_mostrar(int res, float ax, float ay);
void ui_demo_toque(int res);
static void olor_actualizar(void);
static void animar_nochevieja(void);
static int s_cum_fase; /* tu cumpleaños en su pantalla (ver cumple_empezar) */
static void cumple_soplar(void);
static void globo(int ico, const char *txt, uint32_t ms);
/* una frase al azar de varias (no repite la misma dos veces seguidas): para los avisos que salen todos los días */
static const char *una_de(const char *const *v, int n)
{
    static const char *ultima[8]; static int k;
    const char *r = v[(int)(azar() % (uint32_t)n)];
    for (int i = 0; i < 8; i++) if (ultima[i] == r && n > 1) { r = v[(int)(azar() % (uint32_t)n)]; break; }
    ultima[k++ & 7] = r;
    return r;
}
#define UNA_DE(...) ({ static const char *const v_[] = {__VA_ARGS__}; una_de(v_, (int)(sizeof v_ / sizeof v_[0])); })
static void mascota_modo(modo_t m, uint32_t ms);
/* tocar durante el baile: si fue al pulso (cerca de la nota), cuenta y salen corazones */
static void baile_toque(int x, int y)
{
    uint32_t tb = s_ms - s_baile_desde;
    int pulso = (int)((tb + BAILE_PULSO_MS / 2) / BAILE_PULSO_MS);
    uint32_t fase = tb % BAILE_PULSO_MS;
    bool al_ritmo = fase < 140 || fase > BAILE_PULSO_MS - 140;
    if (al_ritmo && pulso != s_baile_ult_pulso) {
        s_baile_ult_pulso = pulso;
        s_baile_aciertos++;
        flotar(ICO_CORAZON, NULL, 0, x, y - 20, 0);
    } else flotar(ICO_NOTA, NULL, 0, x, y - 20, 0);
}
/* tocar una serie de cristales de la cueva (su melodía o un ensayo): una nota cada cierto rato, con una notita flotando */
static uint8_t s_mel_notas[PET_MELODIA_NOTAS];
static int s_mel_k, s_mel_len, s_mel_paso = 420;
static uint32_t s_mel_prox;
static bool s_mel_on;
static void tocar_cristales(const uint8_t *n, int len, int paso_ms)
{
    if (len > PET_MELODIA_NOTAS) len = PET_MELODIA_NOTAS;
    memcpy(s_mel_notas, n, (size_t)len);
    s_mel_len = len; s_mel_k = 0; s_mel_prox = s_ms; s_mel_paso = paso_ms; s_mel_on = len > 0;
}
static int cabeza_y(const criatura_t *c);
static void animar_notas_cristal(void)
{
    if (!s_mel_on || (int32_t)(s_ms - s_mel_prox) < 0) return;
    if (s_mel_k >= s_mel_len || !s_pet) { s_mel_on = false; return; }
    const int idx = s_mel_notas[s_mel_k] % 6;
    accion(UI_ACCION_SONAR, SND_CRISTAL_AZUL + idx);

    flotar(ICO_NOTA, NULL, 0, (int)s_mascota.x - 36 + (s_mel_k % 4) * 24, cabeza_y(&s_mascota) - 6 - (s_mel_k % 2) * 12, 0);

    s_mel_k++;
    s_mel_prox += (uint32_t)s_mel_paso;
}
static void animar_baile(void)
{
    if (!s_baile_on) return;
    if (s_ms - s_baile_desde < BAILE_MS + 300 && s_mascota.modo == M_BAILA) return;
    s_baile_on = false;
    accion(UI_ACCION_BAILE, s_baile_aciertos);
    if (s_baile_aciertos >= 4) { globo(ICO_CORAZON, s_nino ? NULL : "¡BAILAMOS JUNTOS!", 3000); mascota_modo(M_SALTA, 1200); }
}

static void mostrar_criatura(criatura_t *c, bool si)
{
    c->visible = si;
    ocultar(c->img, !si);
    ocultar(c->sombra, !si);
}

static int cabeza_y(const criatura_t *c) { return (int)(c->y - lado(c) + c->pose.escala * 5); }

/* ======================================================================
 * Paneles (declaración temprana: los íconos flotantes no los tapan)
 * ==================================================================== */

typedef enum {
    PANEL_NINGUNO, PANEL_COMER, PANEL_MAS, PANEL_RINCON, PANEL_AMIGOS, PANEL_INVITACION, PANEL_FRASE,
    PANEL_NOMBRE, PANEL_FECHA, PANEL_AJUSTES, PANEL_RESUMEN, PANEL_JUGAR, PANEL_TRUCOS, PANEL_DIARIO,
    PANEL_HORA, PANEL_RESTAURAR, PANEL_RESPALDO, PANEL_ASPECTO, PANEL_SEMILLA, PANEL_FOTO, PANEL_MAPA, PANEL_MASCOTAS,
    PANEL_ALARMAS, PANEL_ALARMA, PANEL_DORMIR, PANEL_SONIDO, PANEL_PANTALLA, PANEL_HORA_MENU, PANEL_TEMPORIZADOR, PANEL_MAS_AJ, PANEL_MODO,
    PANEL_BOLSA, PANEL_OBJETO, PANEL_RAPIDO, PANEL_COLOR, PANEL_BOTE, PANEL_FAMILIA, PANEL_VIAJE_PIDE, PANEL_CARTA, PANEL_CARTAS, PANEL_GENERO, PANEL_HISTORIA, PANEL_PAIS, PANEL_CRISTALES, PANEL_EVOLUCION, PANEL_RITMO, PANEL_FINAL, PANEL_TEMPORADA, PANEL_LIBHIST, PANEL_CINE, PANEL_COFRE, PANEL_PAGINA, PANEL_LUGAR, PANEL_CRISMENU, PANEL_CANCIONES, PANEL_VERSION
} panel_t;
static bool s_fin_repetida; /* se está viendo otra vez desde la Biblioteca: se puede cerrar y no da premios */
static void final_revisar(void);
static void biblioteca_cb(lv_event_t *e);
static void constelacion_actualizar(bool ver);
static void panel_final(int cual, int pag);
static void rito_historia(void); /* ui_magia.inc */
static art_icono_t icono_narrador(int c, int k); /* ui_magia.inc (v37) */
static void animar_historia(void); /* ui_historia.inc (v37) */
static void panel_pagina(int k);
static void panel_lugar(int l);
static void panel_lectura(art_icono_t ico, const char *de, const char *titulo, const char *cuerpo);
static void evento_historia(const pet_event_t *e);
static panel_t s_panel = PANEL_NINGUNO;
/* (fw 104) un papelito del bosque esperando a abrirse (después del globo y si no hay otro panel encima) */
static int s_papel_k = -1;
static bool s_papel_trajo; /* el que se abre ahora te lo trajo ella (sin leerlo) */
static uint32_t s_papel_desde;
static void papelito_pendiente(int k, uint32_t espera) { s_papel_k = k; s_papel_desde = s_ms + espera; }
static int s_mic_objeto = -1; /* el objeto que espera un soplido (en su ficha) */
static bool s_cru_activo; /* va en el bote a la isla (o de vuelta) */
static bool s_po_tele; /* Pochi la lleva por un portal: llega directo, sin bote ni caminata */
static bool s_barro_pochi; /* el barro de ella viene de Pochi: sus patas se ven solo por donde ella caminó (no las huellas fijas hasta el baño) */
static void cielo_empezar(const pet_t *m);
static void cruce_rafaga(void);
static void animar_rito(void);
static void animar_ceremonias(void);
static void panel_libro_indice(void); /* ui_magia.inc */
static int s_lib_narr;
static void animar_zona(void);
static void animar_entradas_zonas(void);
static bool s_exc_oculta; /* (v40) se fue sola de excursión al claro o al mirador: no se ve en el mundo */
static bool s_lw_directo; /* (v98) llega a la zona nueva de una vez (en la nube al mirador, o encontrándola de excursión) */
static bool lw_nube_desde_colina(bool mirando);
static bool zj_quiere_mic(void);
static void zj_soplido(void);
static bool zona_activa(void);
static void zona_desde_mapa(int z);
static void rep_tocar(const uint8_t *notas, int n, int modo); /* ui_musica.inc */
static bool rep_activo(void);
static void cer_empezar(int cual, bool repetida);
static void panel_pide_viaje(void);
static void actualizar_huevo_fam(const pet_t *p);
static bool lago_congelado(const pet_t *p);
static uint32_t s_pajaro_t; /* cuándo se vio por última vez un pajarito (0: nunca) */
static void pajaro_llama(void); /* hace que cruce un pajarito */
static bool zona_fria(const pet_t *p); /* invierno, nieve o frío: la nieve cubre lo que crece */
/* fase 5 (ver "creatividad") */
static bool s_cargando;
static int s_colocando = -1; /* juguete que estás poniendo en el prado */
static int s_foto_pag;
static void panel_aspecto(void);
static void panel_genero(void);
static void aspecto_volver_cb(lv_event_t *e);
static void panel_color(void);
static uint32_t s_color_pend;
static void libro_pedir(int n); /* ui_final.inc */
static void libro_revisar(void);
static void cine_revisar(void); /* ui_escenas.inc */
static void libro_inicio(void);
static bool s_libro_inicio;
/* el inicio, en este orden: Libro → huevo → niña/niño → fecha, hora y país → el huevo se abre → nombre → color → modo → hola.
 * s_asp_listo: ya eligió huevo y género (se envían cuando el reloj ya está puesto); s_inicio_cfg: 1 nombre, 2 color, 3 modo */
static bool s_asp_listo;
static void inicio_huevo(void);
static int s_asp_gen, s_inicio_cfg;
/* (107) el primer huevo se abre A LA VISTA: antes se llamaba a ECLOSIONAR en el mismo instante en que se ponía
 * el reloj, así que el huevo nunca llegaba a verse en el prado. Ahora llega, se mece, se le abren las grietas y
 * recién entonces nace; después viene el nombre.
 * (1.0.10) Como se acordó al comienzo, tarda 5 MINUTOS (PET_HUEVO_SEGUNDOS, la cuenta es del motor: pet_egg_start
 * al llegar al prado) y no 9 segundos. Cada caricia le da calor y lo adelanta (PET_HUEVO_CALOR_S), como a todos. */
static uint32_t s_nace_desde, s_nace_fin;
static int s_nace_paso;
static void panel_rincon(void);
static void panel_semilla(int parcela);
static void panel_foto(void);
static void panel_bolsa(void);
static int s_bolsa_pag, s_bolsa_libro;
static void objeto_al_mundo(int id, int x0, int y0, bool solo);
static void fotos_tab(void);
static void entrar_pecera(bool solo);
static bool colocar_toque(int x, int y);
static void arrastrar_fantasma(int x, int y);
static const art_icono_t ICO_JUG[JUG_COUNT] = {ICO_PELUCHE, ICO_PELOTON, ICO_CAJA, ICO_COLUMPIO, ICO_TAMBOR, ICO_MOLINETE};
static const art_icono_t ICO_ROPA[ROPA_COUNT] = {ICO_MONO, ICO_PANUELO, ICO_GORRO_LANA, ICO_BUFANDA, ICO_SOMBRERO, ICO_LENTES, ICO_CORONA, ICO_COLLAR, ICO_NENUFAR, ICO_PARAGUAS, ICO_MOLINILLO, ICO_FAROL_PAPEL};

/* ======================================================================
 * Íconos flotantes
 * ==================================================================== */

#define N_FLOTANTES 12
typedef struct {
    lv_obj_t *img;
    art_img_t texto;
    bool usado;
} flotante_t;
static flotante_t s_flot[N_FLOTANTES];

static void anim_y(void *o, int32_t v) { lv_obj_set_style_translate_y((lv_obj_t *)o, v, 0); }
static void anim_opa(void *o, int32_t v) { lv_obj_set_style_opa((lv_obj_t *)o, (lv_opa_t)v, 0); }
static void flot_fin(lv_anim_t *a)
{
    lv_obj_t *o = (lv_obj_t *)a->var;
    ocultar(o, true);
    for (int i = 0; i < N_FLOTANTES; i++)
        if (s_flot[i].img == o) s_flot[i].usado = false;
}

/* hace subir un ícono (o un texto si ico < 0) desde (x, y) y desvanecerse */
static void flotar(int ico, const char *texto, uint32_t color, int x, int y, uint32_t retraso)
{
    if (s_panel != PANEL_NINGUNO) return;
    flotante_t *f = NULL;
    for (int i = 0; i < N_FLOTANTES; i++)
        if (!s_flot[i].usado) { f = &s_flot[i]; break; }
    if (!f) return;
    f->usado = true;
    const lv_image_dsc_t *d;
    if (ico >= 0) d = art_icono((art_icono_t)ico, 3);
    else { art_texto(&f->texto, texto, color, 3, 0); d = &f->texto.dsc; }
    poner_src(f->img, d);
    lv_obj_set_pos(f->img, x - (int)d->header.w / 2, y - (int)d->header.h / 2);
    lv_obj_set_style_translate_y(f->img, 0, 0);
    lv_obj_set_style_opa(f->img, LV_OPA_COVER, 0);
    ocultar(f->img, false);
    lv_obj_move_foreground(f->img);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, f->img);
    lv_anim_set_exec_cb(&a, anim_y);
    lv_anim_set_values(&a, 0, -56);
    lv_anim_set_duration(&a, 1300);
    lv_anim_set_delay(&a, retraso);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);
    lv_anim_init(&a);
    lv_anim_set_var(&a, f->img);
    lv_anim_set_exec_cb(&a, anim_opa);
    lv_anim_set_values(&a, LV_OPA_COVER, LV_OPA_TRANSP);
    lv_anim_set_duration(&a, 600);
    lv_anim_set_delay(&a, retraso + 700);
    lv_anim_set_completed_cb(&a, flot_fin);
    lv_anim_start(&a);
}

/* ======================================================================
 * Globo de diálogo (texto solo en modo adulto) y globo de pensamiento
 * ==================================================================== */

static bool s_pecera; /* modo pecera (ver más abajo) */
static lv_obj_t *s_globo, *s_globo_ico, *s_globo_txt, *s_globo_cola, *s_globo_area;
/* mientras lo lees (o lo deslizas), el globo no se va */
static uint32_t s_globo_hasta;
static void globo_toque_cb(lv_event_t *e) { (void)e; s_globo_hasta = s_ms + 4000; }
static bool globo_cerrar(void); /* (v39) un toque sobre el globo o doble toque en cualquier parte: se cierra y se calla */
static void globo_clic_cb(lv_event_t *e) { (void)e; globo_cerrar(); }
static art_img_t s_globo_art;

static bool po_globo_visible(void); /* ui_pochi.inc */
static bool po_activo(void);        /* ui_pochi.inc: Pochi está de visita */
static bool s_po_bypass;            /* una respuesta a Pochi: esa sí se dice */
typedef struct { int ico; char t[220]; uint32_t ms; } globo_pend_t;
static globo_pend_t s_gpend[4];
static int s_gpend_n;
static void globo(int ico, const char *texto, uint32_t ms)
{
    texto = gtxt(texto);
    if (!s_po_bypass && texto && texto[0] && po_activo()) return; /* mientras Pochi habla o pasea, ella no suelta frases sueltas: solo le responde a él */
    static char cur[220];
    const bool ocupado = po_globo_visible() || (s_globo && !lv_obj_has_flag(s_globo, LV_OBJ_FLAG_HIDDEN) && (int32_t)(s_ms - s_globo_hasta) <= 0);
    if (ocupado) { /* ya se está diciendo algo (ella o Pochi): espera su turno, así no se pierde ningún mensaje */
        if (!((texto && texto[0]) || ico >= 0)) return;
        if (texto && !strcmp(texto, cur) && !po_globo_visible()) return; /* es lo mismo que está diciendo */
        for (int i = 0; i < s_gpend_n; i++) if (s_gpend[i].ico == ico && !strcmp(s_gpend[i].t, texto ? texto : "")) return;
        if (s_gpend_n >= 4) { for (int i = 1; i < 4; i++) s_gpend[i - 1] = s_gpend[i]; s_gpend_n = 3; }
        s_gpend[s_gpend_n].ico = ico; s_gpend[s_gpend_n].ms = ms;
        snprintf(s_gpend[s_gpend_n].t, sizeof s_gpend[0].t, "%s", texto ? texto : "");
        s_gpend_n++;
        return;
    }
    snprintf(cur, sizeof cur, "%s", texto ? texto : "");
    bool con_texto = texto && texto[0] && !s_nino;
    if (ico >= 0) {
        poner_src(s_globo_ico, art_icono((art_icono_t)ico, con_texto ? 3 : 5));
        ocultar(s_globo_ico, false);
    } else {
        ocultar(s_globo_ico, true);
    }
    if (con_texto) {
        art_texto(&s_globo_art, texto, 0xFFF3C8, 2, 250); /* letra crema y más pequeña, sobre la noche del globo */
        poner_src(s_globo_txt, &s_globo_art.dsc);
        lv_obj_scroll_to_y(s_globo_area, 0, LV_ANIM_OFF);
        lv_obj_update_layout(s_globo);
        bool desliza = lv_obj_get_scroll_bottom(s_globo_area) > 2;
        lv_obj_set_scrollbar_mode(s_globo_area, LV_SCROLLBAR_MODE_OFF); /* sin barra: si hay más, se desliza */
        { /* tiempo de lectura: mínimo 4 s y ~90 ms por letra (más si se desliza) */
            uint32_t min_ms = 4000u + (uint32_t)strlen(texto) * 90u;
            if (desliza) min_ms += 2500u;
            if (ms < min_ms) ms = min_ms;
        }
    }
    ocultar(s_globo_area, !con_texto);
    if (!con_texto && ico < 0) return;
    ocultar(s_globo, false);
    ocultar(s_globo_cola, false);
    s_globo_hasta = s_ms + ms;
}

static void po_globo_cerrar(void); /* ui_pochi.inc */
static bool globo_cerrar(void)
{
    const bool suyo = s_globo && !lv_obj_has_flag(s_globo, LV_OBJ_FLAG_HIDDEN);
    const bool de_pochi = po_globo_visible();
    if (!suyo && !de_pochi) return false;
    if (suyo) { ocultar(s_globo, true); ocultar(s_globo_cola, true); s_globo_hasta = s_ms; }
    if (de_pochi) po_globo_cerrar();
    accion(UI_ACCION_CALLAR, 0); /* se calla: se corta lo que estaba diciendo */
    return true;
}

/* pensamiento: nube con la emoción y su causa, sin palabras */
static lv_obj_t *s_pens, *s_pens_emo, *s_pens_causa, *s_pens_b1, *s_pens_b2;
static uint32_t s_pens_hasta;

/* Los globos y las nubes siempre salen de ella: arriba, a un lado o abajo, con la colita (o las burbujitas) apuntando a ella, y nunca tapándola */
typedef enum { BD_ARRIBA, BD_ABAJO, BD_DER, BD_IZQ } bdir_t;
static bool burbuja_cabe(int x, int y, int w, int h)
{
    const int R = 224, dx[2] = {x, x + w}, dy[2] = {y, y + h};
    if (y + h > 336) return false; /* sin tapar la barra de botones */
    if (s_pecera && (y < 178 || y + h > 318)) return false; /* sin tapar la hora ni los botones de abajo */
    for (int a = 0; a < 2; a++)
        for (int b = 0; b < 2; b++) {
            int ex = dx[a] - CX, ey = dy[b] - CX;
            if (ex * ex + ey * ey > R * R) return false;
        }
    return true;
}
static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }
/* elige dónde poner una burbuja de w x h: devuelve la dirección y la esquina; sep = hueco entre ella y la burbuja (para la colita) */
static bdir_t ubicar_burbuja(const criatura_t *c, int w, int h, int sep, int *px, int *py, int *ax, int *ay)
{
    const int l = lado(c);
    const int sx0 = (int)c->x - l * 38 / 100, sx1 = (int)c->x + l * 38 / 100;
    const int sy0 = cabeza_y(c) - 2, sy1 = (int)c->y + 6, sym = (sy0 + sy1) / 2;
    const bool der_primero = c->x < CX;
    const bdir_t orden[4] = {BD_ARRIBA, der_primero ? BD_DER : BD_IZQ, der_primero ? BD_IZQ : BD_DER, BD_ABAJO};
    int mejor = -1, mx = 0, my = 0;
    bdir_t md = BD_ARRIBA;
    for (int k = 0; k < 4; k++) {
        int x, y;
        switch (orden[k]) {
        case BD_ARRIBA: x = clampi((int)c->x - w / 2, 40, W - 40 - w); y = sy0 - h - sep; break;
        case BD_ABAJO: x = clampi((int)c->x - w / 2, 40, W - 40 - w); y = sy1 + sep; break;
        case BD_DER: x = sx1 + sep; y = sym - h / 2; break;
        default: x = sx0 - sep - w; y = sym - h / 2; break;
        }
        if (orden[k] == BD_DER || orden[k] == BD_IZQ) y = clampi(y, 60, 330 - h);
        const bool tapa = x < sx1 && x + w > sx0 && y < sy1 && y + h > sy0; /* ¿le cae encima? */
        if (!tapa && burbuja_cabe(x, y, w, h)) { *px = x; *py = y; md = orden[k]; goto listo; }
        if (!tapa && mejor < 0) { mejor = k; mx = x; my = y; md = orden[k]; }
    }
    if (mejor >= 0) { *px = mx; *py = my; goto listo; } /* no cabe del todo en el círculo, pero no la tapa */
    *px = clampi((int)c->x - w / 2, 40, W - 40 - w); *py = clampi(sy0 - h - sep, 56, 330); md = BD_ARRIBA;
listo:
    switch (md) { /* de dónde sale: el punto de ella al que apunta */
    case BD_ARRIBA: *ax = (int)c->x; *ay = sy0; break;
    case BD_ABAJO: *ax = (int)c->x; *ay = sy1; break;
    case BD_DER: *ax = sx1; *ay = sym; break;
    default: *ax = sx0; *ay = sym; break;
    }
    return md;
}

static void seguir_globos(void)
{
    criatura_t *c = &s_mascota;
    if (lv_obj_has_flag(s_globo, LV_OBJ_FLAG_HIDDEN) && s_gpend_n > 0 && !po_globo_visible()) { /* le toca a ella */
        globo_pend_t g = s_gpend[0];
        for (int i = 1; i < s_gpend_n; i++) s_gpend[i - 1] = s_gpend[i];
        s_gpend_n--;
        globo(g.ico, g.t, g.ms);
    }
    if (!lv_obj_has_flag(s_globo, LV_OBJ_FLAG_HIDDEN)) {
        if ((int32_t)(s_ms - s_globo_hasta) > 0 || zona_activa()) { /* (v39) en el claro o el mirador no hay globos del mundo */
            ocultar(s_globo, true);
            ocultar(s_globo_cola, true);
        } else {
            lv_obj_update_layout(s_globo);
            const int w = lv_obj_get_width(s_globo), h = lv_obj_get_height(s_globo);
            /* v90: el globo se queda QUIETO donde apareció durante todo lo que dura (antes se volvía a poner cuando ella se alejaba
             * 70 px, y parecía que decía lo mismo en otro lugar o dos veces). Si ella camina, solo la colita la sigue por el borde;
             * si se aleja mucho, la colita se esconde y el globo sigue en su lugar hasta terminar. */
            static int clave_ant = -1, gx, gy;
            static bdir_t gd;
            static uint32_t hasta_ant;
            static const void *obj_ant;
            const int clave = (w * 17 + h * 29) ^ (s_picnic_on ? 0x40000 : 0);
            if (hasta_ant != s_globo_hasta || clave != clave_ant || obj_ant != s_globo_txt) {
                clave_ant = clave; obj_ant = s_globo_txt; hasta_ant = s_globo_hasta;
                int ax0, ay0;
                gd = ubicar_burbuja(c, w, h, 22, &gx, &gy, &ax0, &ay0);
                lv_obj_set_pos(s_globo, gx, gy);
                lv_obj_set_style_transform_pivot_x(s_globo_cola, 6, 0);
                lv_obj_set_style_transform_pivot_y(s_globo_cola, 6, 0);
                lv_obj_set_style_transform_rotation(s_globo_cola, gd == BD_ARRIBA ? 0 : gd == BD_ABAJO ? 1800 : gd == BD_DER ? 900 : 2700, 0);
            }
            const int ax = (int)c->x, ay = cabeza_y(c) + 10; /* hacia dónde está ella ahora */
            int cx, cy;
            switch (gd) {
            case BD_ARRIBA: cx = clampi(ax, gx + 16, gx + w - 16) - 6; cy = gy + h - 3; break;
            case BD_ABAJO: cx = clampi(ax, gx + 16, gx + w - 16) - 6; cy = gy - 9; break;
            case BD_DER: cx = gx - 9; cy = clampi(ay, gy + 16, gy + h - 16) - 6; break;
            default: cx = gx + w - 3; cy = clampi(ay, gy + 16, gy + h - 16) - 6; break;
            }
            const int ddx = ax - cx, ddy = ay - cy;
            const bool cerca = ddx * ddx + ddy * ddy < 130 * 130;
            ocultar(s_globo_cola, !cerca || !c->visible);
            if (cerca) lv_obj_set_pos(s_globo_cola, cx, cy);
        }
    }

    if (!lv_obj_has_flag(s_pens, LV_OBJ_FLAG_HIDDEN)) {
        if ((int32_t)(s_ms - s_pens_hasta) > 0 || !lv_obj_has_flag(s_globo, LV_OBJ_FLAG_HIDDEN) || po_globo_visible()) {
            ocultar(s_pens, true); ocultar(s_pens_b1, true); ocultar(s_pens_b2, true);
        } else {
            lv_obj_update_layout(s_pens);
            const int w = lv_obj_get_width(s_pens), h = lv_obj_get_height(s_pens);
            /* v90: la nube de lo que piensa también se queda quieta donde apareció; solo las burbujitas la unen con ella */
            static int px0, py0, pw0, ph0; static bdir_t pd0; static uint32_t phasta;
            int ax0, ay0;
            if (phasta != s_pens_hasta || pw0 != w || ph0 != h) {
                phasta = s_pens_hasta; pw0 = w; ph0 = h;
                pd0 = ubicar_burbuja(c, w, h, 38, &px0, &py0, &ax0, &ay0);
                lv_obj_set_pos(s_pens, px0, py0);
            }
            const int x = px0, y = py0, ax = (int)c->x, ay = cabeza_y(c);
            const bdir_t d = pd0;
            { const int dx = ax - x - w / 2, dy = ay - y - h / 2; const bool cerca = dx * dx + dy * dy < 160 * 160; ocultar(s_pens_b1, !cerca); ocultar(s_pens_b2, !cerca); }
            /* las burbujitas van de ella a la nube, en línea */
            int tx, ty;
            switch (d) {
            case BD_ARRIBA: tx = clampi(ax, x + 16, x + w - 16); ty = y + h; break;
            case BD_ABAJO: tx = clampi(ax, x + 16, x + w - 16); ty = y; break;
            case BD_DER: tx = x; ty = clampi(ay, y + 16, y + h - 16); break;
            default: tx = x + w; ty = clampi(ay, y + 16, y + h - 16); break;
            }
            const int w1 = lv_obj_get_width(s_pens_b1), w2 = lv_obj_get_width(s_pens_b2);
            lv_obj_set_pos(s_pens_b1, ax + (tx - ax) * 22 / 100 - w1 / 2, ay + (ty - ay) * 22 / 100 - w1 / 2);
            lv_obj_set_pos(s_pens_b2, ax + (tx - ax) * 62 / 100 - w2 / 2, ay + (ty - ay) * 62 / 100 - w2 / 2);
        }
    }
}

static art_icono_t icono_emocion(emo_t e)
{
    static const art_icono_t T[EMO_COUNT] = {ICO_CARA_FELIZ, ICO_CARA_TRISTE, ICO_CARA_ENOJADA, ICO_CARA_MIEDO,
                                              ICO_CARA_CURIOSA, ICO_CARA_ABURRIDA, ICO_CARA_CANSADA,
                                              ICO_CARA_ORGULLOSA, ICO_CARA_CELOSA, ICO_CARA_CARINO};
    return T[(unsigned)e % EMO_COUNT];
}

static const art_icono_t ICO_FAUNA[FAUNA_COUNT] = {ICO_MARIPOSA, ICO_ABEJA, ICO_PAJARO, ICO_ZORRO, ICO_RANA};
static const art_icono_t ICO_ZONA[ESC_COUNT] = {ICO_ZONA_PRADO, ICO_ZONA_BOSQUE, ICO_ZONA_LAGO, ICO_ZONA_COLINA, ICO_ZONA_CUEVA, ICO_ZONA_ISLA, ICO_ZONA_CLARO, ICO_ZONA_MIRADOR};

static int icono_causa(causa_t c)
{
    switch (c) {
    case CAUSA_JUGAMOS: case CAUSA_NO_JUGASTE: return ICO_PELOTA;
    case CAUSA_CARICIAS: case CAUSA_FALTA_CARINO: return ICO_MANO;
    case CAUSA_TOQUE_FEO: return ICO_MANO_NO;
    case CAUSA_TE_EXTRANA: return ICO_RELOJ; /* pasó mucho rato sin ti */
    case CAUSA_VOLVISTE: return ICO_CASITA; /* volviste a casa */
    case CAUSA_SACUDIDA: return ICO_SACUDIDA;
    case CAUSA_HAMBRE: case CAUSA_COMIDA_FEA: return ICO_PLATO;
    case CAUSA_COMIDA_RICA: return ICO_FRUTA;
    case CAUSA_SUENO: return ICO_LUNA;
    case CAUSA_LA_DESPERTASTE: return ICO_DESPERTADOR;
    case CAUSA_SUCIA: return ICO_GOTA;
    case CAUSA_BANO: return ICO_BURBUJA;
    case CAUSA_ENFERMA: return ICO_TERMOMETRO;
    case CAUSA_LLUVIA: return ICO_LLUVIA;
    case CAUSA_SOL: return ICO_SOL;
    case CAUSA_NIEVE: return ICO_COPO;
    case CAUSA_VIENTO: return ICO_VIENTO;
    case CAUSA_TORMENTA: return ICO_RAYO;
    case CAUSA_NUBLADO: return ICO_NUBE;
    case CAUSA_VISITA: case CAUSA_AMIGO: case CAUSA_MIMASTE_OTRO: return ICO_AMIGOS;
    case CAUSA_HALLAZGO: return ICO_PIEDRA;
    case CAUSA_CRECIO: return ICO_ESTRELLA;
    case CAUSA_NOVEDAD: case CAUSA_EXPLORAR: return ICO_LUPA;
    case CAUSA_NADA_QUE_HACER: return ICO_PUNTOS;
    case CAUSA_LUGAR_FEO: return ICO_ALERTA;
    case CAUSA_FRIO: return ICO_FRIO;
    case CAUSA_CALOR: return ICO_CALOR;
    case CAUSA_BEBIDA: return s_pet && s_pet->bebida_tipo == FOOD_BEBIDA_FRIA ? ICO_VASO : ICO_TAZA;
    case CAUSA_MARIPOSA: return ICO_MARIPOSA;
    case CAUSA_ABEJA: return ICO_ABEJA;
    case CAUSA_PAJARO: return ICO_PAJARO;
    case CAUSA_ZORRO: case CAUSA_ESPANTASTE: return ICO_ZORRO;
    case CAUSA_JARDIN: return ICO_FLOR;
    case CAUSA_LUGAR: return s_pet && s_pet->escena < ESC_COUNT ? (int)ICO_ZONA[s_pet->escena] : -1;
    case CAUSA_CHARCO: return ICO_CHARCO;
    case CAUSA_HOJAS: return ICO_HOJAS;
    case CAUSA_VECINO: return ICO_AMIGOS;
    case CAUSA_PESCA: return ICO_CANA;
    case CAUSA_CUMPLE: return ICO_PASTEL;
    case CAUSA_CARTA: return ICO_CARTA;
    default: return -1;
    }
}

static void estilo_nube(bool sueno)
{
    lv_obj_t *o[3] = {s_pens, s_pens_b1, s_pens_b2};
    for (int i = 0; i < 3; i++) {
        lv_obj_set_style_bg_color(o[i], lv_color_hex(sueno ? 0x2E2A5A : 0xF4F1FF), 0);
        lv_obj_set_style_border_color(o[i], lv_color_hex(sueno ? 0xB48CFF : 0x8C84B0), 0);
        /* la nube de un sueño es translúcida: se ve el mundo detrás */
        lv_obj_set_style_bg_opa(o[i], sueno ? LV_OPA_30 : LV_OPA_COVER, 0);
        lv_obj_set_style_border_opa(o[i], sueno ? LV_OPA_60 : LV_OPA_COVER, 0);
    }
}

/* en el modo adulto lo que piensa se dice con palabras: "ESTOY FELIZ PORQUE JUGAMOS" (en el modo niño sigue siendo la nube con dibujitos) */
static const char *texto_pensado(emo_t e, causa_t c, char *buf, int n)
{
    static const char *const EM[EMO_COUNT] = {"ESTOY FELIZ", "ESTOY TRISTE", "ESTOY ENOJAD{O}", "TENGO MIEDO", "ESTOY CURIOS{O}",
                                              "ME ABURRO", "ESTOY CANSAD{O}", "ESTOY ORGULLOS{O}", "ME DIERON CELOS", "TE QUIERO MUCHO"};
    static const char *const CA[CAUSA_COUNT] = {
        "", "PORQUE JUGAMOS", "PORQUE NO JUGAMOS", "POR LAS CARICIAS", "PORQUE TE EXTRAÑO", "POR EL HAMBRE", "POR LA COMIDA RICA", "POR LA COMIDA FEA",
        "POR EL SUEÑO", "PORQUE ESTOY SUCI{O}", "PORQUE ME SIENTO MAL", "POR LA LLUVIA", "POR EL SOL", "POR LA NIEVE", "POR EL VIENTO", "POR LA TORMENTA", "POR LAS NUBES",
        "POR LA VISITA", "POR MI AMIGO", "POR ESE TOQUE FEO", "POR LA SACUDIDA", "POR LO QUE ENCONTRÉ", "PORQUE CRECÍ", "PORQUE MIMASTE A OTRO",
        "POR TANTAS NOVEDADES", "POR ESTE LUGAR", "PORQUE VOLVISTE", "PORQUE NO HAY NADA QUE HACER", "PORQUE ME DESPERTASTE", "POR EL BAÑO", "POR EXPLORAR",
        "POR EL FRÍO", "POR EL CALOR", "POR LA BEBIDA", "POR LA MARIPOSA", "POR LA ABEJA", "POR EL PÁJARO", "POR EL ZORRO", "PORQUE ESPANTASTE AL ZORRO",
        "POR MI JARDÍN", "POR ESTA ZONA", "POR LOS CHARCOS", "POR LAS HOJAS", "POR MI AMIGO DEL BARRIO", "POR LA PESCA", "POR TU CUMPLEAÑOS",
        "POR MI TRUCO", "POR HABERME DESLIZADO", "POR MI TESORO", "POR ALGO RARO Y LINDO", "POR LA META DEL DÍA", "POR MI CUMPLEAÑOS", "POR ESTA FECHA ESPECIAL",
        "POR LO QUE APRENDÍ", "POR LA ROPA", "POR MI JUGUETE", "PORQUE QUIERO MIMOS", "POR LA CARTA", "POR LA CRIATURA MÁGICA", "POR EL SECRETO"};
    const char *em = EM[(unsigned)e % EMO_COUNT];
    const char *ca = (unsigned)c < CAUSA_COUNT ? CA[c] : "";
    snprintf(buf, (size_t)n, ca[0] ? "%s %s" : "%s", em, ca);
    return buf;
}

static void pensar(emo_t e, causa_t c)
{
    if (!s_nino) { /* adulto: con palabras */
        char t[120];
        texto_pensado(e, c, t, sizeof t);
        globo(icono_emocion(e), t, 4500);
        return;
    }
    estilo_nube(false);
    art_icono_t ie = icono_emocion(e);
    int ic = icono_causa(c);
    poner_src(s_pens_emo, art_icono(ie, 4));
    ocultar(s_pens_causa, ic < 0 || ic == (int)ie);
    if (ic >= 0) poner_src(s_pens_causa, art_icono((art_icono_t)ic, 4));
    ocultar(s_pens, false); ocultar(s_pens_b1, false); ocultar(s_pens_b2, false);
    s_pens_hasta = s_ms + 4500;
}

/* ======================================================================
 * Escena: fondo, cacas, luciérnagas, objetos, partículas, flash
 * ==================================================================== */

static lv_obj_t *s_fondo;
static int s_fondo_periodo = -1, s_fondo_estacion = -1, s_fondo_clima = -1, s_fondo_escena = -1;
static uint32_t s_fondo_decor = 0xFFFFFFFFu, s_fondo_sendas = 0xFFFFFFFFu;

#define N_CACAS 3
static lv_obj_t *s_cacas[N_CACAS];
/* dónde caen las cacas: sitios de tierra de cada zona (nunca sobre el agua; en el lago solo si está congelado) y cambian cada vez */
static const int16_t CACA_SITIOS[ESC_COUNT][8][2] = {
    {{330, 270}, {190, 330}, {300, 200}, {120, 200}, {250, 300}, {380, 320}, {200, 380}, {330, 370}},   /* prado */
    {{250, 330}, {330, 280}, {300, 200}, {200, 300}, {360, 350}, {260, 380}, {180, 220}, {340, 230}},   /* bosque */
    {{150, 300}, {320, 290}, {240, 330}, {100, 340}, {360, 350}, {200, 380}, {300, 380}, {260, 270}},   /* lago (orilla) */
    {{150, 300}, {330, 300}, {240, 260}, {120, 340}, {350, 350}, {200, 380}, {300, 220}, {180, 210}},   /* colina */
    {{150, 300}, {330, 300}, {240, 260}, {120, 340}, {350, 350}, {200, 380}, {300, 220}, {180, 210}},   /* cueva */
    {{150, 300}, {330, 300}, {240, 260}, {120, 340}, {350, 350}, {200, 380}, {300, 220}, {180, 210}},   /* isla (no hay) */
    {{300, 300}, {240, 380}, {330, 360}, {220, 230}, {360, 270}, {180, 390}, {290, 200}, {130, 220}},   /* (v98) claro (no en la poza ni en la casita) */
    {{280, 250}, {190, 270}, {250, 300}, {300, 210}, {170, 200}, {220, 180}, {320, 280}, {210, 240}}};  /* (v98) mirador (sobre la cumbre) */
static const int16_t CACA_HIELO[2][2] = {{150, 170}, {310, 180}}; /* sobre el hielo del lago */
static uint32_t s_caca_sem = 1;
static int s_caca_x[N_CACAS], s_caca_y[N_CACAS];
static void caca_sitio(int i, bool hielo)
{
    int e = s_escena < 0 || s_escena >= ESC_COUNT ? 0 : s_escena;
    uint32_t h = (s_caca_sem * 2654435761u) ^ ((uint32_t)i * 40503u) ^ ((uint32_t)e * 977u);
    h ^= h >> 13; h *= 3266489917u; h ^= h >> 16;
    int k = (int)((h + (uint32_t)i * 3u) % (hielo && e == ESC_LAGO ? 10u : 8u));
    if (k >= 8) { s_caca_x[i] = CACA_HIELO[k - 8][0]; s_caca_y[i] = CACA_HIELO[k - 8][1]; }
    else { s_caca_x[i] = CACA_SITIOS[e][k][0]; s_caca_y[i] = CACA_SITIOS[e][k][1]; }
}

#define N_LUCES 7
static lv_obj_t *s_luces[N_LUCES];
static float s_luz_x[N_LUCES], s_luz_y[N_LUCES], s_luz_f[N_LUCES];

static lv_obj_t *s_pelota, *s_objeto; /* objeto: comida o el regalo que trae */
static int s_ultima_comida = FOOD_COMIDA;

#define N_TIERRA 32
static lv_obj_t *s_tierra[N_TIERRA];
static float s_tx_[N_TIERRA], s_ty_[N_TIERRA], s_tvx[N_TIERRA], s_tvy[N_TIERRA], s_tsz[N_TIERRA];
static uint8_t s_ttipo[N_TIERRA], s_tvida[N_TIERRA], s_tmax[N_TIERRA]; /* tipo 0: terrón o gota que cae; 1: gotita de agua; 2: nube de polvo que crece y se desvanece */
#define N_ONDITA 6
static lv_obj_t *s_ondita[N_ONDITA]; /* anillos que se abren en el agua */
static float s_ondita_x[N_ONDITA], s_ondita_y[N_ONDITA];
static uint8_t s_ondita_t[N_ONDITA]; /* 0 libre; si no, cuántos cuadros lleva */
static bool s_squash; /* chapoteo y cavar estiran y aplastan a la criatura */
static void salpicar(int x, int y, uint32_t color); /* gotitas, tierra u hojas que saltan */
/* quitar cosas del mapa con tres toques (ver más abajo) */
enum { QUITAR_NADA = 0, QUITAR_JUGUETE, QUITAR_PLANTA, QUITAR_DECOR };
static int toques_seguidos(int clave);
static int decor_en(int x, int y);
static void mostrar_quitar(int tipo, int cual, int x, int y);
static void tierra(int x, int y);
static void tierra_atras(int x, int y, float dir);
static bool s_cavando; /* este cuadro está cavando (para el hoyo) */
static void crear_hoyo(void);

#define N_PART 26
static lv_obj_t *s_part[N_PART], *s_clima_ico, *s_flash, *s_luna_grande;
static float s_part_x[N_PART], s_part_y[N_PART], s_part_v[N_PART];

static lv_obj_t *s_bat, *s_bat_lleno;
static int s_bat_pct = -1;

static bool s_carga_pend;
static uint32_t s_carga_en;
static void bateria_refrescar(void);
void ui_set_battery(int pct, bool cargando)
{
    if (!s_bat) return;
    /* la batería es parte de ella: con poca carga le da sueño; al enchufarla se alegra */
    static bool s_aviso_bajo;
    if (s_pet && s_pet->etapa != STAGE_HUEVO && s_pet->edad_dueno && pct >= 0) {
        if (cargando && !s_cargando && s_bat_pct >= 0) {
            s_carga_pend = true; s_carga_en = 0; /* el mensaje de ella sale unos segundos DESPUÉS de que se cierre el aviso de carga */
        }
        if (!cargando && pct <= 15 && !s_aviso_bajo) {
            s_aviso_bajo = true;
            globo(ICO_LUNA, s_nino ? NULL : UNA_DE("TENGO SUEÑITO... ¿ME CARGAS?", "ME ESTOY QUEDANDO SIN FUERZAS...", "¿PODEMOS ENCHUFARNOS UN RATITO?", "ME QUEDA POCA ENERGÍA... ¿ME CARGAS?", "ESTOY CANSADITA... ¿ME ENCHUFAS?", "NECESITO UN POCO DE ENERGÍA"), 4000);
            accion(UI_ACCION_SONAR, SND_BOSTEZO);
        }
        if (cargando || pct > 25) s_aviso_bajo = false;
    }
    s_bat_pct = pct;
    s_cargando = cargando;
    ocultar(s_bat, true);
    bateria_refrescar(); /* el dibujo de los ajustes se actualiza solo (porcentaje y rayo) */
    if (pct < 0) return;
    lv_obj_set_width(s_bat_lleno, 2 + pct * 20 / 100);
    lv_obj_set_style_bg_color(s_bat_lleno, lv_color_hex(cargando ? 0x6BD38A : (pct < 20 ? C_ROJO : C_BLANCO)), 0);
}

/* --- aviso de batería: al enchufar el cargador (unos segundos) y cuando la carga termina; se ve sin tocar nada --- */
static lv_obj_t *icono(lv_obj_t *padre, art_icono_t ico, int escala);
static lv_obj_t *s_bt, *s_bt_fill, *s_bt_rayo, *s_bt_txt, *s_bt_sub;
static art_img_t s_bt_art, s_bt_sub_art;
static uint32_t s_bt_hasta;
void ui_battery_aviso(int pct, int modo, uint32_t ms) /* modo 0: conectaron el cargador; 1: carga completa */
{
    if (!s_mundo) return;
    if (!s_bt) {
        s_bt = caja(lv_layer_top());
        lv_obj_set_size(s_bt, 280, 150);
        lv_obj_align(s_bt, LV_ALIGN_CENTER, 0, -20);
        lv_obj_set_style_bg_color(s_bt, lv_color_hex(0x16203A), 0);
        lv_obj_set_style_bg_opa(s_bt, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(s_bt, 26, 0);
        lv_obj_set_style_border_width(s_bt, 3, 0);
        lv_obj_set_style_border_color(s_bt, lv_color_hex(0xFFF3C8), 0);
        lv_obj_t *m = caja(s_bt); /* el marco de la pila */
        lv_obj_set_size(m, 120, 46);
        lv_obj_align(m, LV_ALIGN_TOP_MID, -6, 16);
        lv_obj_set_style_border_width(m, 4, 0);
        lv_obj_set_style_border_color(m, lv_color_hex(0xFFF3C8), 0);
        lv_obj_set_style_radius(m, 9, 0);
        lv_obj_t *p = caja(s_bt);
        lv_obj_set_size(p, 8, 20);
        lv_obj_align(p, LV_ALIGN_TOP_MID, 60, 29);
        lv_obj_set_style_bg_color(p, lv_color_hex(0xFFF3C8), 0);
        lv_obj_set_style_bg_opa(p, LV_OPA_COVER, 0);
        s_bt_fill = caja(m);
        lv_obj_set_height(s_bt_fill, 30);
        lv_obj_set_pos(s_bt_fill, 4, 4);
        lv_obj_set_style_radius(s_bt_fill, 5, 0);
        lv_obj_set_style_bg_opa(s_bt_fill, LV_OPA_COVER, 0);
        s_bt_rayo = icono(s_bt, ICO_RAYO, 3);
        lv_obj_align(s_bt_rayo, LV_ALIGN_TOP_MID, -90, 24);
        s_bt_txt = lv_image_create(s_bt);
        s_bt_sub = lv_image_create(s_bt);
        lv_obj_remove_flag(s_bt_txt, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_remove_flag(s_bt_sub, LV_OBJ_FLAG_CLICKABLE);
    }
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    lv_obj_set_width(s_bt_fill, 4 + pct * 104 / 100);
    lv_obj_set_style_bg_color(s_bt_fill, lv_color_hex(modo ? 0x6BD38A : (pct < 20 ? C_ROJO : 0xFFE27A)), 0);
    char b[16];
    snprintf(b, sizeof b, "%d%%", pct);
    art_texto(&s_bt_art, b, 0xFFFFFF, 5, 0);
    lv_image_set_src(s_bt_txt, &s_bt_art.dsc);
    lv_obj_align(s_bt_txt, LV_ALIGN_TOP_MID, 0, 64);
    art_texto(&s_bt_sub_art, modo ? "CARGA COMPLETA" : "CARGANDO", modo ? 0x9CF0B4 : 0xFFE27A, 2, 0);
    lv_image_set_src(s_bt_sub, &s_bt_sub_art.dsc);
    lv_obj_align(s_bt_sub, LV_ALIGN_BOTTOM_MID, 0, -8);
    ocultar(s_bt, false);
    lv_obj_move_foreground(s_bt);
    s_bt_hasta = s_ms + ms;
}
bool ui_battery_aviso_activo(void) { return s_bt && !lv_obj_has_flag(s_bt, LV_OBJ_FLAG_HIDDEN); }
static void animar_bateria_aviso(void)
{
    if (s_bt && !lv_obj_has_flag(s_bt, LV_OBJ_FLAG_HIDDEN) && (int32_t)(s_ms - s_bt_hasta) >= 0) ocultar(s_bt, true);
    if (s_carga_pend && !ui_battery_aviso_activo()) {
        if (!s_carga_en) s_carga_en = s_ms + 2000;
        else if ((int32_t)(s_ms - s_carga_en) >= 0 && s_panel == PANEL_NINGUNO && s_cargando) {
            s_carga_pend = false;
            globo(ICO_RAYO, s_nino ? NULL : UNA_DE("¡QUÉ RICO! ME ESTÁS CARGANDO", "¡ENERGÍA NUEVA! ¡GRACIAS!", "¡AAH, QUÉ BIEN SE SIENTE!", "¡CARGANDO! ¡ME SIENTO COMO NUEVA!", "¡MMM, QUÉ CALENTITO!", "¡ME ESTOY LLENANDO DE ENERGÍA!"), 3000);
            accion(UI_ACCION_SONAR, SND_ALEGRE);
        } else if (!s_cargando) s_carga_pend = false;
    }
}

/* ======================================================================
 * Paneles
 * ==================================================================== */

static lv_obj_t *s_capa, *s_tarjeta, *s_volver;
#define N_PTXT 100
static EXT_RAM_BSS_ATTR art_img_t s_ptxt[N_PTXT];
static int s_ptxt_n;
static EXT_RAM_BSS_ATTR art_img_t s_mini[12];
/* íconos de objetos de las listas largas (la Bolsa): imágenes propias, se liberan al cerrar */
#define N_POBJ 140
static EXT_RAM_BSS_ATTR art_img_t s_pobj[N_POBJ];
static int s_pobj_n; /* criaturas chicas de los paneles (amigos, fotos del álbum) */

static lv_obj_t *s_disco;
static bool s_portal; /* el panel abierto va sobre el disco translúcido de noche (menús de comer, jugar y configuración) */
/* "Texto grande" (Ajustes > Pantalla): solo crecen los textos de lectura que se pueden deslizar (historias, notas, cartas); botones, etiquetas y
 * páginas de espacio justo quedan igual. Se pide con texto_l(). */
static bool s_fuente_grande, s_texto_l;
static int fuente_escala(int escala) { return s_fuente_grande && s_texto_l && escala == 2 ? 3 : escala; }
/* (v96) la letra va un 5 % más grande. Después de armar un panel se revisa cada texto: si así se sale de la cajita o del
 * botón donde va (o del panel), se vuelve a pintar del tamaño de antes (pixel exacto). Por eso se guarda con qué se pintó. */
static struct { char *s; uint32_t color; int16_t ancho; int8_t escala; bool centrado; lv_obj_t *img; } s_ptxt_info[N_PTXT];
static bool s_txt_revisar;
static void texto_pintar(int k, lv_obj_t *img, const char *s, uint32_t color, int escala, int ancho, bool centrado)
{
    art_img_t *t = &s_ptxt[k];
    art_texto_a8(true); /* (v97) en A8: la cuarta parte de memoria; el color va en la imagen */
    if (centrado) art_texto_centrado(t, s, color, escala, ancho); else art_texto(t, s, color, escala, ancho);
    art_texto_a8(false);
    if (img) lv_obj_set_style_image_recolor(img, lv_color_hex(color), 0);
    free(s_ptxt_info[k].s);
    s_ptxt_info[k].s = strdup(s);
    s_ptxt_info[k].color = color; s_ptxt_info[k].ancho = (int16_t)ancho; s_ptxt_info[k].escala = (int8_t)escala;
    s_ptxt_info[k].centrado = centrado; s_ptxt_info[k].img = img;
    s_txt_revisar = true;
}
static void textos_revisar(void)
{
    if (!s_txt_revisar || s_panel == PANEL_NINGUNO || !s_tarjeta) return;
    s_txt_revisar = false;
    lv_obj_update_layout(s_tarjeta);
    bool cambio = false;
    for (int k = 0; k < s_ptxt_n; k++) {
        lv_obj_t *img = s_ptxt_info[k].img;
        if (!img || !s_ptxt_info[k].s || !lv_obj_is_valid(img) || lv_image_get_src(img) != &s_ptxt[k].dsc) continue;
        lv_obj_t *pa = lv_obj_get_parent(img);
        if (!pa) continue;
        lv_area_t a, c;
        lv_obj_get_coords(img, &a);
        if (pa == s_tarjeta || pa == lv_screen_active()) lv_obj_get_coords(pa, &c); else lv_obj_get_content_coords(pa, &c);
        if (a.x1 >= c.x1 - 1 && a.x2 <= c.x2 + 1) continue;
        art_texto_factor(1.0f);
        art_texto_a8(true);
        if (s_ptxt_info[k].centrado) art_texto_centrado(&s_ptxt[k], s_ptxt_info[k].s, s_ptxt_info[k].color, s_ptxt_info[k].escala, s_ptxt_info[k].ancho);
        else art_texto(&s_ptxt[k], s_ptxt_info[k].s, s_ptxt_info[k].color, s_ptxt_info[k].escala, s_ptxt_info[k].ancho);
        art_texto_a8(false);
        art_texto_factor(1.05f);
        lv_image_set_src(img, &s_ptxt[k].dsc);
        cambio = true;
    }
    if (cambio) lv_obj_invalidate(s_tarjeta);
}
/* sobre el disco de noche (los menús), las letras oscuras pasan a tonos claros */
static uint32_t color_portal(uint32_t color)
{
    if (!s_portal) return color;
    if (color == C_TINTA) return 0xFFF3C8;
    if (color == C_VIOLETA) return 0xD8C8FF;
    if (color == C_GRIS) return 0xC8C4E0;
    if ((((color >> 16) & 255) + ((color >> 8) & 255) + (color & 255)) / 3 < 125) return 0xFFF3C8; /* cualquier otro tono oscuro */
    return color;
}
static lv_obj_t *texto(lv_obj_t *padre, const char *s, uint32_t color, int escala, int ancho)
{
    s = gtxt(s);
    escala = fuente_escala(escala);
    color = color_portal(color);
    lv_obj_t *img = lv_image_create(padre);
    if (s_ptxt_n < N_PTXT) {
        const int k = s_ptxt_n++;
        texto_pintar(k, img, s, color, escala, ancho, false);
        lv_image_set_src(img, &s_ptxt[k].dsc);
    }
    return img;
}

/* un texto con cada renglón centrado (para leyendas y frases en medio de un panel) */
static lv_obj_t *texto_c(lv_obj_t *padre, const char *s, uint32_t color, int escala, int ancho)
{
    s = gtxt(s);
    escala = fuente_escala(escala);
    color = color_portal(color); /* (v97) también aquí: en los menús de noche, un gris oscuro casi no se veía */
    lv_obj_t *img = lv_image_create(padre);
    if (s_ptxt_n < N_PTXT) {
        const int k = s_ptxt_n++;
        texto_pintar(k, img, s, color, escala, ancho, true);
        lv_image_set_src(img, &s_ptxt[k].dsc);
    }
    return img;
}
/* un texto de lectura: crece con "Texto grande" */
static lv_obj_t *texto_l(lv_obj_t *padre, const char *s, uint32_t color, int escala, int ancho)
{
    s_texto_l = true;
    lv_obj_t *o = texto(padre, s, color, escala, ancho);
    s_texto_l = false;
    return o;
}
static lv_obj_t *icono(lv_obj_t *padre, art_icono_t ico, int escala);
/* a MAYÚSCULAS (también las letras con tilde, la ñ y la ü) */
static char *mayusculas(char *s)
{
    for (unsigned char *q = (unsigned char *)s; *q; q++) {
        if (*q >= 'a' && *q <= 'z') *q = (unsigned char)(*q - 32);
        else if (*q == 0xC3 && q[1] >= 0xA0 && q[1] <= 0xBF) { q[1] = (unsigned char)(q[1] - 0x20); q++; }
    }
    return s;
}
static lv_obj_t *icono_obj(lv_obj_t *padre, int id, int escala)
{
    if (s_pobj_n >= N_POBJ) return icono(padre, ICO_OBJETO(id), escala);
    art_img_t *im = &s_pobj[s_pobj_n++];
    art_icono_en(im, ICO_OBJETO(id), escala);
    lv_obj_t *img = lv_image_create(padre);
    lv_image_set_src(img, &im->dsc);
    return img;
}
/* (v96) los íconos grandes de un panel (escala 4 o más) se dibujan en imágenes del panel, que se sueltan al cerrarlo. Antes iban
 * todos al caché de íconos, que nunca se vacía: con el Libro (un dibujo por historia, a escala 5) y los demás paneles, la PSRAM se
 * llenaba en una hora de pruebas y la placa se quedaba sin memoria y se colgaba. */
static bool es_del_panel(lv_obj_t *o)
{
    if (s_panel == PANEL_NINGUNO || !s_tarjeta) return false;
    for (int i = 0; o && i < 16; i++, o = lv_obj_get_parent(o)) if (o == s_tarjeta) return true;
    return false;
}
static lv_obj_t *icono(lv_obj_t *padre, art_icono_t ico, int escala)
{
    lv_obj_t *img = lv_image_create(padre);
    if (escala >= 4 && s_pobj_n < N_POBJ && es_del_panel(padre)) {
        art_img_t *im = &s_pobj[s_pobj_n++];
        art_icono_en(im, ico, escala);
        if (im->px) { lv_image_set_src(img, &im->dsc); return img; }
    }
    lv_image_set_src(img, art_icono(ico, escala));
    return img;
}

static art_img_t s_noche_art;
static bool s_pag_mundo; /* el panel de Ella se abrió deslizando desde el mundo (primero la afinidad, luego las barras) */
static lv_obj_t *s_fin_num[16]; /* (el contador de objetos del final; se declara aquí para poder soltarlo al cerrar) */
static void zona_tras_panel(void);
static void zona_bajo_panel(void);
static bool zona_usa_pantalla(int id);
static bool zona_activa(void);
static void zona_salir(void);
static void cerrar_panel_dentro(void)
{
    if (s_panel == PANEL_NINGUNO) return;
    lv_anim_delete(s_tarjeta, NULL); /* las animaciones del panel (p. ej. el contador del final) siguen vivas aunque se borren sus objetos: tocaban memoria ya liberada y colgaban la placa */
    memset(s_fin_num, 0, sizeof s_fin_num);
    lv_obj_clean(s_tarjeta);
    art_pantalla_liberar(0); /* la foto del vivero */
    /* (v37) los lugares secretos (v97: salvo el que está de fondo en la zona de ahora: soltarlo dejaba la zona dibujando
     * memoria ya liberada -se veía basura arriba- cuando se cerraba una carta o un menú encima de la casita o el mirador) */
    if (!zona_usa_pantalla(5)) art_pantalla_liberar(5);
    if (!zona_usa_pantalla(6)) art_pantalla_liberar(6);
    art_img_free(&s_noche_art); s_noche_art.px = NULL;
    for (int i = 0; i < s_ptxt_n; i++) { art_img_free(&s_ptxt[i]); free(s_ptxt_info[i].s); s_ptxt_info[i].s = NULL; s_ptxt_info[i].img = NULL; }
    s_ptxt_n = 0;
    for (int i = 0; i < s_pobj_n; i++) art_img_free(&s_pobj[i]);
    s_pobj_n = 0;
    ocultar(s_capa, true);
    if (s_volver) ocultar(s_volver, true);
    s_panel = PANEL_NINGUNO;
    s_portal = false;
    s_mic_objeto = -1;
}
static void cerrar_panel(void) { s_pag_mundo = false; cerrar_panel_dentro(); zona_tras_panel(); } /* (v97) la zona de debajo vuelve a verse al tiro: un toque en ese instante caía en los botones del mundo */

static lv_obj_t *s_cerrar_x;
static void cerrar_x_cb(lv_event_t *e) { (void)e; cerrar_panel(); }
/* abajo, para salir y volver: la X al centro y la flecha a su izquierda, lejos del borde del círculo */
#define BOTON_ABAJO 42
#define BOTON_ABAJO_Y 396 /* y de arriba de los dos botones */
#define VOLVER_DX 58      /* la flecha, a la izquierda de la X */
#define ZONA_ABAJO_Y (BOTON_ABAJO_Y - 6) /* lo de los paneles termina más arriba que esto (salvo a los lados) */
static lv_event_cb_t s_volver_fn;
/* la flecha de volver de este panel: llama a cb con id (como cualquier botón) */
static void volver(lv_event_cb_t cb, intptr_t id)
{
    if (!s_volver) return;
    if (s_volver_fn) lv_obj_remove_event_cb(s_volver, s_volver_fn);
    s_volver_fn = cb;
    lv_obj_add_event_cb(s_volver, cb, LV_EVENT_CLICKED, (void *)id);
    /* con «volver» los dos botones van como pareja, centrados en la pantalla */
    if (s_cerrar_x) lv_obj_set_x(s_cerrar_x, CX - BOTON_ABAJO / 2 + VOLVER_DX / 2);
    lv_obj_set_x(s_volver, CX - BOTON_ABAJO / 2 - VOLVER_DX / 2);
    ocultar(s_volver, false);
    lv_obj_move_foreground(s_volver);
}
/* un botón de la tarjeta, abajo a la derecha de la X (simétrico a volver) */
static void abajo_derecha(lv_obj_t *b)
{
    lv_obj_update_layout(s_tarjeta);
    lv_area_t c;
    lv_obj_get_coords(s_tarjeta, &c);
    lv_obj_set_size(b, BOTON_ABAJO + 4, BOTON_ABAJO);
    lv_obj_set_pos(b, CX + VOLVER_DX - (BOTON_ABAJO + 4) / 2 - c.x1, BOTON_ABAJO_Y - c.y1);
}
/* zona segura de la pantalla redonda: el ancho que se ve a la altura y (de la pantalla), con margen hacia adentro */
static int zona_ancho(int y)
{
    int dy = y - CX, r = 226 - 18;
    if (dy < -r || dy > r) return 0;
    return 2 * (int)sqrtf((float)(r * r - dy * dy));
}
static void capa_cb(lv_event_t *e)
{
    if (s_panel == PANEL_CINE) return; /* la escena se salta tocándola, no por el fondo */
    if (s_panel == PANEL_NOMBRE || s_panel == PANEL_FECHA || s_panel == PANEL_HORA || s_panel == PANEL_RESTAURAR || s_panel == PANEL_ASPECTO || s_panel == PANEL_COLOR || s_panel == PANEL_GENERO || (s_panel == PANEL_PAIS && s_pet && !s_pet->pais) || (s_panel == PANEL_FINAL && !s_fin_repetida)) return; /* hay que completarlos */
    if (s_cerrar_x && !lv_obj_has_flag(s_cerrar_x, LV_OBJ_FLAG_HIDDEN)) return; /* los menús se cierran solo con la X (un toque o un deslizar afuera no los cierra) */
    if (lv_event_get_target(e) == lv_event_get_current_target(e)) cerrar_panel();
}

/* los botones de aceptar de todas las pantallas de inicio quedan en el mismo lugar de la pantalla, sin importar el tamaño de la tarjeta */
#define OK_Y_PANTALLA 84
static void ok_alinear(lv_obj_t *ok, int dx) { lv_obj_align_to(ok, lv_screen_active(), LV_ALIGN_BOTTOM_MID, dx, -OK_Y_PANTALLA); }
static int s_pr_x, s_pr_y;
static void filtro_toque_cb(lv_event_t *e) /* un dedo que venía deslizando no aprieta botones: así no se pisan las flechas ni los botones al ver sus estados o el libro */
{
    lv_indev_t *in = lv_indev_active();
    if (!in) return;
    lv_point_t p;
    lv_indev_get_point(in, &p);
    if (lv_event_get_code(e) == LV_EVENT_PRESSED) { s_pr_x = p.x; s_pr_y = p.y; return; }
    if (abs(p.x - s_pr_x) > 30 || abs(p.y - s_pr_y) > 30) lv_event_stop_processing(e);
}
static void filtro_toque(lv_obj_t *o)
{
    lv_obj_add_event_cb(o, filtro_toque_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(o, filtro_toque_cb, LV_EVENT_CLICKED, NULL);
}
static lv_obj_t *boton(lv_obj_t *padre, int w, int h, lv_event_cb_t cb, intptr_t dato)
{
    lv_obj_t *b = lv_button_create(padre);
    lv_obj_set_size(b, w, h);
    /* los anchos son píldoras y los cuadrados de un ícono, redondos: combinan con la pantalla */
    lv_obj_set_style_radius(b, w >= h * 7 / 5 || (w == h && w <= 60) ? LV_RADIUS_CIRCLE : 18, 0);
    lv_obj_set_style_bg_color(b, lv_color_hex(C_BOTON), 0);
    lv_obj_set_style_bg_color(b, lv_color_hex(C_BOTON_P), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(b, 3, 0);
    lv_obj_set_style_border_color(b, lv_color_hex(0x15121F), 0);
    lv_obj_set_style_shadow_width(b, 0, 0);
    lv_obj_set_style_pad_all(b, 0, 0);
    filtro_toque(b);
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, (void *)dato);
    if (h <= 64) lv_obj_set_ext_click_area(b, 10); /* los botones chicos se aciertan mejor con el dedo */
    return b;
}

/* (v99) un panel con varias páginas: abajo van los puntitos con una flecha a cada lado, y también se pasa deslizando.
 * Cada panel guarda su página en s_pag y se vuelve a dibujar con «fn». */
static int s_pag, s_pag_n;
static void (*s_pag_fn)(void);
static void pag_ir(int d)
{
    const int n = s_pag + d;
    if (n < 0 || n >= s_pag_n || !s_pag_fn) return;
    s_pag = n;
    s_pag_fn();
}
static void pag_cb(lv_event_t *e) { pag_ir((int)(intptr_t)lv_event_get_user_data(e)); }
static void pag_gesto_cb(lv_event_t *e)
{
    (void)e;
    lv_indev_t *in = lv_indev_active();
    if (!in) return;
    const lv_dir_t d = lv_indev_get_gesture_dir(in);
    if (d == LV_DIR_LEFT) pag_ir(1);
    else if (d == LV_DIR_RIGHT) pag_ir(-1);
}
static void paginas(int n, void (*fn)(void), int y)
{
    s_pag_n = n; s_pag_fn = fn;
    if (s_pag >= n) s_pag = 0;
    lv_obj_add_event_cb(s_tarjeta, pag_gesto_cb, LV_EVENT_GESTURE, NULL);
    lv_obj_remove_flag(s_tarjeta, LV_OBJ_FLAG_GESTURE_BUBBLE);
    for (int k = 0; k < n; k++) {
        lv_obj_t *p = lv_obj_create(s_tarjeta);
        lv_obj_remove_style_all(p);
        const int r = k == s_pag ? 12 : 8;
        lv_obj_set_size(p, r, r);
        lv_obj_set_style_radius(p, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(p, lv_color_hex(k == s_pag ? 0xFFD76A : 0x8A8EA0), 0);
        lv_obj_set_style_bg_opa(p, LV_OPA_COVER, 0);
        lv_obj_align(p, LV_ALIGN_TOP_MID, (k * 2 - (n - 1)) * 10, y + 20 - r / 2);
        lv_obj_remove_flag(p, LV_OBJ_FLAG_CLICKABLE);
    }
    for (int k = 0; k < 2; k++) {
        if ((k == 0 && s_pag == 0) || (k == 1 && s_pag == n - 1)) continue;
        lv_obj_t *b = boton(s_tarjeta, 40, 40, pag_cb, k ? 1 : -1);
        lv_obj_align(b, LV_ALIGN_TOP_MID, (k ? 1 : -1) * (n * 10 + 34), y);
        lv_obj_t *fl = icono(b, ICO_FLECHA, 2);
        lv_obj_update_layout(fl);
        lv_image_set_pivot(fl, lv_obj_get_width(fl) / 2, lv_obj_get_height(fl) / 2);
        lv_image_set_rotation(fl, k ? 0 : 1800);
        lv_obj_center(fl);
    }
}

/* lo irreversible (despedir, borrar una foto, pisar con un respaldo) pide mantener apretado */
static void mantener(lv_obj_t *b, lv_event_cb_t cb, intptr_t dato)
{
    lv_obj_remove_event_cb(b, cb);
    lv_obj_add_event_cb(b, cb, LV_EVENT_LONG_PRESSED, (void *)dato);
}


/* --- el vidrio de los paneles: el bosque de ese momento, empañado, con un círculo claro encima.
 * Se calcula una sola vez al abrir (no hay transparencias que mezclar en cada cuadro): es una imagen
 * opaca y por debajo no se dibuja nada. */
static uint16_t *s_vidrio_px;
static lv_image_dsc_t s_vidrio_dsc;
static lv_obj_t *s_vidrio;
#define VID_R 226 /* radio del vidrio */
/* la chimenea de la casita del prado, en la pantalla (el ícono va en 88,34 del mundo, a escala 2, y el mundo se ve x3) */
#define CASITA_CHIMENEA_X ((88 + 9 * 2) * 3)
#define CASITA_CHIMENEA_Y ((34 + 0) * 3)
static bool s_vid_osc; /* el vidrio de ahora es el oscuro de los menús */
static void preparar_vidrio(bool osc)
{
    s_vid_osc = osc;
    if (!s_vidrio_px) {
        s_vidrio_px = malloc((size_t)W * W * 2);
        if (!s_vidrio_px) return;
        s_vidrio_dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
        s_vidrio_dsc.header.cf = LV_COLOR_FORMAT_RGB565;
        s_vidrio_dsc.header.w = W;
        s_vidrio_dsc.header.h = W;
        s_vidrio_dsc.header.stride = W * 2;
        s_vidrio_dsc.data = (const uint8_t *)s_vidrio_px;
        s_vidrio_dsc.data_size = (uint32_t)W * W * 2;
    }
    /* 1) el fondo en bloques de 8x8 (promedio): 59x59 colores */
    enum { B = 8, G = (W + B - 1) / B };
    static EXT_RAM_BSS_ATTR uint8_t gr[G][G][3];
    const lv_image_dsc_t *f = (const lv_image_dsc_t *)lv_image_get_src(s_fondo);
    const uint16_t *fp = f && f->header.cf == LV_COLOR_FORMAT_RGB565 && f->header.w == W ? (const uint16_t *)f->data : NULL;
    for (int gy = 0; gy < G; gy++)
        for (int gx = 0; gx < G; gx++) {
            int r = 0, g = 0, b = 0, n = 0;
            for (int y = gy * B; y < gy * B + B && y < W; y += 2)
                for (int x = gx * B; x < gx * B + B && x < W; x += 2) {
                    uint16_t c = fp ? fp[y * W + x] : 0x3C67;
                    r += (c >> 11) << 3; g += ((c >> 5) & 63) << 2; b += (c & 31) << 3; n++;
                }
            gr[gy][gx][0] = (uint8_t)(r / n); gr[gy][gx][1] = (uint8_t)(g / n); gr[gy][gx][2] = (uint8_t)(b / n);
        }
    /* 2) se suaviza dos veces */
    static EXT_RAM_BSS_ATTR uint8_t tmp[G][G][3];
    for (int pasada = 0; pasada < 2; pasada++) {
        for (int gy = 0; gy < G; gy++)
            for (int gx = 0; gx < G; gx++)
                for (int k = 0; k < 3; k++) {
                    int sum = 0, n = 0;
                    for (int dy = -1; dy <= 1; dy++)
                        for (int dx = -1; dx <= 1; dx++) {
                            int yy = gy + dy, xx = gx + dx;
                            if (yy < 0 || xx < 0 || yy >= G || xx >= G) continue;
                            sum += gr[yy][xx][k]; n++;
                        }
                    tmp[gy][gx][k] = (uint8_t)(sum / n);
                }
        memcpy(gr, tmp, sizeof gr);
    }
    /* 3) se amplía suave, se aclara hacia crema (vidrio lechoso) y se le pone el borde */
    const int CR = osc ? 0x14 : 0xFF, CG = osc ? 0x1A : 0xF8, CB = osc ? 0x46 : 0xE8; /* crema, o azul noche en los menús */
    for (int y = 0; y < W; y++) {
        int fy = y * 256 / B - 128; if (fy < 0) fy = 0;
        int y0 = fy >> 8, y1 = y0 + 1 < G ? y0 + 1 : G - 1, ty = fy & 255;
        int dy = y - CX;
        for (int x = 0; x < W; x++) {
            int dx = x - CX, d2 = dx * dx + dy * dy;
            uint16_t px;
            if (d2 > (VID_R + 4) * (VID_R + 4)) px = 0;
            else if (d2 > VID_R * VID_R) px = 0x2908; /* el aro (tinta) */
            else {
                int fx = x * 256 / B - 128; if (fx < 0) fx = 0;
                int x0 = fx >> 8, x1 = x0 + 1 < G ? x0 + 1 : G - 1, tx = fx & 255;
                int c[3];
                for (int k = 0; k < 3; k++) {
                    int a0 = gr[y0][x0][k] * (256 - tx) + gr[y0][x1][k] * tx, a1 = gr[y1][x0][k] * (256 - tx) + gr[y1][x1][k] * tx;
                    c[k] = (a0 * (256 - ty) + a1 * ty) >> 16;
                }
                int mezcla = osc ? 176 : 158; /* de 256: cuánto crema (o noche) */
                if (d2 > (VID_R - 5) * (VID_R - 5)) mezcla = 140;      /* un borde interior más verde: se nota el vidrio */
                if (!osc && (((x >> 1) + (y >> 1)) & 1)) mezcla += 8;           /* tramado fino 2x2, como el pixel art */
                int r = (c[0] * (256 - mezcla) + CR * mezcla) >> 8, g = (c[1] * (256 - mezcla) + CG * mezcla) >> 8, b = (c[2] * (256 - mezcla) + CB * mezcla) >> 8;
                
                px = (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
            }
            s_vidrio_px[y * W + x] = px;
        }
    }
    lv_image_set_src(s_vidrio, &s_vidrio_dsc);
    lv_obj_invalidate(s_vidrio);
}

static void portal_fondo_r(int radio, int w, int h);
static bool s_hora_portal; /* la fecha y hora abierta desde Configuración (no la del inicio): va con el disco de los menús */
static bool panel_es_portal(panel_t p)
{
    if (p == PANEL_HORA) return s_hora_portal;
    switch (p) {
    case PANEL_COMER: case PANEL_TEMPORADA: case PANEL_JUGAR: case PANEL_TRUCOS: case PANEL_OBJETO: case PANEL_RITMO:
    case PANEL_MAS: case PANEL_AJUSTES: case PANEL_SONIDO: case PANEL_PANTALLA: case PANEL_HORA_MENU: case PANEL_ALARMAS: case PANEL_ALARMA:
    case PANEL_COFRE: case PANEL_DORMIR: case PANEL_TEMPORIZADOR: case PANEL_MASCOTAS: case PANEL_RESPALDO: case PANEL_MAS_AJ: case PANEL_MODO:
    case PANEL_MAPA: case PANEL_AMIGOS: case PANEL_CARTAS: case PANEL_RINCON: case PANEL_BOLSA:
    case PANEL_CRISMENU: case PANEL_CANCIONES: case PANEL_VERSION: return true; /* (v97) sus letras son claras: sobre el vidrio claro no se leían */
    default: return false;
    }
}
static void abrir_tarjeta(panel_t p, int w, int h)
{
#ifdef ESP_PLATFORM
    /* poca PSRAM: se sueltan las imágenes guardadas por LVGL (se vuelven a cargar solas);
     * si no, malloc sigue en la RAM interna, la deja en 0 y la placa se reinicia (pasaba al abrir la bolsa o el diario) */
    if (heap_caps_get_free_size(MALLOC_CAP_SPIRAM) < 1200u * 1024u) {
        lv_image_cache_drop(NULL); /* (v97: los íconos guardados ya no se sueltan aquí, solo en ui_update: aquí podía haber uno recién pedido) */
    }
#endif
    { const bool osc = panel_es_portal(p); if (s_panel == PANEL_NINGUNO || osc != s_vid_osc) preparar_vidrio(osc); }
    cerrar_panel_dentro();
    zona_bajo_panel(); /* (v97) */
    s_panel = p;
    lv_obj_remove_style_all(s_tarjeta); /* (cada panel parte limpio: sin el estilo del anterior) */
    s_portal = panel_es_portal(p);
    if ((p == PANEL_FINAL || p == PANEL_CINE) && s_globo) { ocultar(s_globo, true); ocultar(s_globo_cola, true); } /* nada debajo que se repinte */
    if (s_portal && s_globo) { ocultar(s_globo, true); ocultar(s_globo_cola, true); } /* (el menú es translúcido: el globo no debe verse a través) */
    if (s_disco) ocultar(s_disco, true);
    lv_obj_set_size(s_tarjeta, w, h);
    lv_obj_align(s_tarjeta, LV_ALIGN_CENTER, 0, h > 330 ? -12 : -6); /* un poco arriba: abajo va la X */
    lv_obj_set_style_bg_opa(s_capa, LV_OPA_COVER, 0);
    lv_obj_set_style_opa(s_vidrio, LV_OPA_COVER, 0);
    if (s_portal) portal_fondo_r(w == h && w < 460 ? LV_RADIUS_CIRCLE : (p == PANEL_COMER || p == PANEL_TEMPORADA ? 70 : -1), w, h); /* -1: placa alta o ancha → disco grande que entra en la pantalla redonda */
    ocultar(s_capa, false);
    lv_obj_move_foreground(s_capa);
    /* para salir: una X redonda abajo, siempre en el mismo lugar (menos en lo que hay que completar) */
    bool obligatorio = p == PANEL_NOMBRE || p == PANEL_FECHA || p == PANEL_HORA || p == PANEL_RESTAURAR || p == PANEL_ASPECTO || p == PANEL_COLOR || p == PANEL_GENERO ||
                       (p == PANEL_PAIS && s_pet && !s_pet->pais && !s_pais_enviado) || (p == PANEL_FINAL && !s_fin_repetida);
    if (s_cerrar_x) { ocultar(s_cerrar_x, obligatorio); lv_obj_move_foreground(s_cerrar_x); }
    if (s_volver) ocultar(s_volver, true);
    if (s_cerrar_x) lv_obj_set_x(s_cerrar_x, CX - BOTON_ABAJO / 2); /* sola: en el centro */
    s_volver_fn = NULL;
}

static lv_obj_t *mini_criatura(lv_obj_t *padre, int slot, int especie, int etapa, int forma, int escala, art_expresion_t ex)
{
    art_criatura(&s_mini[slot], especie, etapa < 1 ? 1 : etapa, forma, escala, ex, false, 0, false, false);
    lv_obj_t *img = lv_image_create(padre);
    lv_image_set_src(img, &s_mini[slot].dsc);
    return img;
}

static void cerrar_cb(lv_event_t *e) { (void)e; cerrar_panel(); }

static lv_obj_t *boton_cerrar(int w, const char *txt)
{
    lv_obj_t *b = boton(s_tarjeta, w + 20, 58, cerrar_cb, 0); /* más grande: antes costaba acertarle */
    lv_obj_set_ext_click_area(b, 18);
    lv_obj_align(b, LV_ALIGN_BOTTOM_MID, 0, -10);
    if (s_nino || !txt) { lv_obj_t *i = icono(b, ICO_SI, 3); lv_obj_center(i); }
    else { lv_obj_t *t = texto(b, txt, C_BLANCO, 2, 0); lv_obj_center(t); }
    return b;
}

/* --- Comer --- */
static void portal_fondo_r(int radio, int w, int h);
static void ficha_magica(lv_obj_t *b, uint32_t arriba, uint32_t abajo, int radio);
static void temporada_cb(lv_event_t *e) { (void)e; panel_temporada(); }
static void panel_comer(void);
static void comida_cb(lv_event_t *e)
{
    int f = (int)(intptr_t)lv_event_get_user_data(e);
    if ((f == FOOD_FRUTA || f == FOOD_MIEL) && s_pet && s_pet->inventario[f == FOOD_FRUTA ? ITEM_FRUTA : ITEM_MIEL] == 0) {
        cerrar_panel();
        globo(f == FOOD_FRUTA ? ICO_FRUTA : ICO_MIEL, "NO QUEDA. ¡HAY QUE BUSCAR MÁS!", 3500);
        return;
    }
    s_ultima_comida = f;
    cerrar_panel();
    empezar_comida(f); /* aparece la comida: llévasela a la boca */
}

static void panel_comer(void)
{
    abrir_tarjeta(PANEL_COMER, 340, 344);
    if (!s_nino) { lv_obj_t *t = texto(s_tarjeta, "¿QUÉ LE DAMOS?", 0xFFE9A0, 2, 0); lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 18); }
    /* arriba la comida; abajo las bebidas (la que necesita, resaltada) */
    static const art_icono_t ICO[FOOD_COUNT] = {ICO_COMIDA, ICO_FRUTA, ICO_MIEL, ICO_TAZA, ICO_VASO};
    int sens = s_pet ? pet_temperature_feel(s_pet, s_ultimo_t) : 0;
    for (int i = 0; i < FOOD_COUNT; i++) {
        bool bebida = i >= FOOD_BEBIDA_CALIENTE;
        lv_obj_t *b = boton(s_tarjeta, 92, 118, comida_cb, i);
        { static const uint32_t FC[FOOD_COUNT][2] = {{0xFFB45C, 0xD8702A}, {0xFF7A7A, 0xC83C48}, {0xFFD45C, 0xD8962A}, {0xC88A5A, 0x8A5A3A}, {0x6ADC8A, 0x2E9A52}}; ficha_magica(b, FC[i][0], FC[i][1], 30); }
        if (!bebida) lv_obj_align(b, LV_ALIGN_TOP_MID, (i - 1) * 100, 52);
        else lv_obj_align(b, LV_ALIGN_TOP_MID, (i == FOOD_BEBIDA_CALIENTE ? -1 : 1) * 52, 182);
        lv_obj_t *ic = icono(b, ICO[i], 5);
        lv_obj_align(ic, LV_ALIGN_TOP_MID, 0, s_nino ? 12 : 6);
        if (!s_nino) { /* el nombre, para que se entienda qué es */
            static const char *const NOM[FOOD_COUNT] = {"COMIDA", "FRUTA", "MIEL", "CHOCO", "JUGO"};
            lv_obj_t *nm = texto(b, NOM[i], 0xFFF3C8, 2, 0);
            lv_obj_align(nm, LV_ALIGN_TOP_MID, 0, 72);
        }
        if (i == FOOD_FRUTA || i == FOOD_MIEL) {
            int n = s_pet ? s_pet->inventario[i == FOOD_FRUTA ? ITEM_FRUTA : ITEM_MIEL] : 0;
            char cant[12];
            snprintf(cant, sizeof cant, "x%d", n);
            lv_obj_t *c = texto(b, cant, 0xFFE27A, s_nino ? 3 : 2, 0);
            lv_obj_align(c, LV_ALIGN_BOTTOM_MID, 0, s_nino ? -10 : -6);
            if (n == 0) lv_obj_set_style_opa(b, LV_OPA_50, 0);
        }
        if ((i == FOOD_BEBIDA_CALIENTE && sens < 0) || (i == FOOD_BEBIDA_FRIA && sens > 0)) { /* lo que le haría bien */
            lv_obj_set_style_border_color(b, lv_color_hex(0xFFD45C), 0);
            lv_obj_t *h = icono(b, ICO_CORAZON, 2);
            lv_obj_align(h, LV_ALIGN_BOTTOM_MID, 0, -6);
        }
    }
    /* y la comida de la estación (lo que ha ido recogiendo según por dónde pasea) */
    lv_obj_t *bt = boton(s_tarjeta, s_nino ? 70 : 230, 40, temporada_cb, 0);
    ficha_magica(bt, 0xFFB45C, 0xB8602A, 20);
    lv_obj_align(bt, LV_ALIGN_TOP_MID, 0, 302);
    lv_obj_t *ih = icono(bt, ICO_HOJA, 2);
    if (s_nino) lv_obj_center(ih);
    else {
        lv_obj_align(ih, LV_ALIGN_LEFT_MID, 14, 0);
        lv_obj_t *tt = texto(bt, "DE TEMPORADA", C_BLANCO, 2, 0);
        lv_obj_align(tt, LV_ALIGN_CENTER, 14, 0);
    }
}

/* --- Comida de temporada: 4 de la estación (aunque falten) y las que guardó de otras --- */
static int s_temp_aviso_sel = -1;
static void comer_volver_cb(lv_event_t *e) { (void)e; if (s_picnic_on) cerrar_panel(); else panel_comer(); }
static void temp_cb(lv_event_t *e)
{
    int i = (int)(intptr_t)lv_event_get_user_data(e);
    if (!s_pet) return;
    if (pet_seasonal_food_count(s_pet, i) <= 0) { /* no tiene: dice dónde se encuentra */
        const comida_est_info_t *c = pet_seasonal_food_info(i);
        char t[120], z[64] = "";
        for (int k = 0; k < ESC_COUNT; k++)
            if ((c->zonas >> k) & 1u) { if (z[0]) strncat(z, " / ", sizeof z - strlen(z) - 1); strncat(z, pet_scene_name((escena_t)k), sizeof z - strlen(z) - 1); }
        snprintf(t, sizeof t, "%s: %s", c->corto, z);
        static lv_obj_t *aviso;
        if (aviso && lv_obj_is_valid(aviso)) lv_obj_delete(aviso);
        aviso = s_nino ? NULL : texto(s_tarjeta, t, 0xFFE27A, 2, 300);
        if (aviso) lv_obj_align(aviso, LV_ALIGN_TOP_MID, 0, 300);
        lv_obj_set_style_translate_x(lv_event_get_current_target(e), 6, 0);
        return;
    }
    s_ultima_comida = PET_COMIDA_EST_BASE + i;
    cerrar_panel();
    empezar_comida(PET_COMIDA_EST_BASE + i);
}
static void panel_temporada(void)
{
    abrir_tarjeta(PANEL_TEMPORADA, 340, 344);
    volver(comer_volver_cb, 0);
    estacion_t es = s_pet ? pet_season(s_pet, s_ultimo_t) : ESTACION_PRIMAVERA;
    if (!s_nino) {
        lv_obj_t *tt = texto(s_tarjeta, "DE TEMPORADA", 0xFFE9A0, 2, 0);
        lv_obj_align(tt, LV_ALIGN_TOP_MID, 0, 18);
    }
    int x = 0, y2 = 0, otras = 0;
    for (int i = 0; i < PET_COMIDAS_EST; i++)
        if (pet_seasonal_food_info(i)->estacion != (uint8_t)es && s_pet && pet_seasonal_food_count(s_pet, i) > 0) otras++;
    for (int i = 0; i < PET_COMIDAS_EST; i++) {
        const comida_est_info_t *c = pet_seasonal_food_info(i);
        int n = s_pet ? pet_seasonal_food_count(s_pet, i) : 0;
        bool grande = c->estacion == (uint8_t)es;
        if (!grande && n <= 0) continue;
        lv_obj_t *b;
        char cant[12];
        snprintf(cant, sizeof cant, "x%d", n);
        if (grande) {
            b = boton(s_tarjeta, 72, 120, temp_cb, i); /* (v97) más alta: el nombre (ahora legible) y la cantidad no se tapan */
            ficha_magica(b, 0xFFB45C, 0xB8602A, 22);
            int k = c->estacion == (uint8_t)es ? i - (int)(i / 4) * 4 : 0;
            lv_obj_align(b, LV_ALIGN_TOP_MID, (k - 2) * 76 + 38, 52);
            lv_obj_t *ic = icono(b, (art_icono_t)(ICO_CE_FRUTILLA + i), 4);
            lv_obj_align(ic, LV_ALIGN_TOP_MID, 0, 4);
            if (!s_nino) { char nn[24]; snprintf(nn, sizeof nn, "%s", c->corto); mayusculas(nn); lv_obj_t *nm = texto(b, nn, 0xFFF3C8, 1, 68); lv_obj_align(nm, LV_ALIGN_TOP_MID, 0, 54); }
            lv_obj_t *q = texto(b, cant, 0xFFE27A, 2, 0);
            lv_obj_align(q, LV_ALIGN_BOTTOM_MID, 0, -4);
            if (n == 0) lv_obj_set_style_opa(b, LV_OPA_40, 0);
            else if (s_pet && pet_seasonal_food_taste(s_pet, i) == 2) { lv_obj_t *h = icono(b, ICO_CORAZON, 1); lv_obj_align(h, LV_ALIGN_TOP_RIGHT, -4, 4); }
        } else {
            b = boton(s_tarjeta, 48, 62, temp_cb, i);
            ficha_magica(b, 0xB48AF0, 0x6A4AB8, 16);
            int en_fila = otras - (x / 6) * 6; if (en_fila > 6) en_fila = 6;
            lv_obj_align(b, LV_ALIGN_TOP_MID, (x % 6) * 52 - (en_fila - 1) * 26, 200 + (x / 6) * 64);
            lv_obj_t *ic = icono(b, (art_icono_t)(ICO_CE_FRUTILLA + i), 3);
            lv_obj_align(ic, LV_ALIGN_TOP_MID, 0, 4);
            lv_obj_t *q = texto(b, cant, 0xFFE27A, 1, 0);
            lv_obj_align(q, LV_ALIGN_BOTTOM_MID, 0, -4);
            x++;
        }
        (void)y2;
    }
    if (!s_nino) {
        lv_obj_t *g = texto(s_tarjeta, x ? "GUARDADAS DE OTRAS ESTACIONES" : "PASEA POR EL MAPA PARA ENCONTRAR MÁS", 0xA09CB8, 1, 300);
        lv_obj_align(g, LV_ALIGN_TOP_MID, 0, x ? 178 : 180);
    }
    (void)s_temp_aviso_sel;
}

/* --- Rincón: decorar con lo que encontró (el panel está en "creatividad") --- */
static void decor_cb(lv_event_t *e)
{
    int d = (int)(intptr_t)lv_event_get_user_data(e);
    if (!s_pet || pet_has_decor(s_pet, (decor_t)d)) return;
    int sem, pie;
    pet_decor_cost((decor_t)d, &sem, &pie);
    if (d == DECOR_CASITA && !pet_materials(s_pet, NULL, NULL)) { /* (v38) la casita es de ramitas y musgo */
        lv_obj_t *b = lv_event_get_current_target(e);
        lv_obj_set_style_translate_x(b, 6, 0);
        static lv_obj_t *aviso_c;
        if (aviso_c && lv_obj_is_valid(aviso_c)) lv_obj_delete(aviso_c);
        char t[112];
        const int fr = PET_CASITA_RAMITAS - s_pet->ramitas, fm = PET_CASITA_MUSGO - s_pet->musgo;
        if (fr > 0 && fm > 0) snprintf(t, sizeof t, "TE FALTAN %d RAMITAS Y %d DE MUSGO. BÚSCALOS EN EL BOSQUE", fr, fm);
        else if (fr > 0) snprintf(t, sizeof t, "TE FALTAN %d RAMITAS. BÚSCALAS EN EL BOSQUE", fr);
        else snprintf(t, sizeof t, "TE FALTAN %d DE MUSGO. BÚSCALO EN EL BOSQUE O LA LAGUNA", fm);
        aviso_c = s_nino ? NULL : texto(s_tarjeta, t, 0xFFB0A0, 2, 300);
        if (aviso_c) lv_obj_align(aviso_c, LV_ALIGN_TOP_MID, 0, 294);
        return;
    }
    if (s_pet->inventario[ITEM_SEMILLA] < sem || s_pet->inventario[ITEM_PIEDRA] < pie) {
        lv_obj_t *b = lv_event_get_current_target(e);
        lv_obj_set_style_translate_x(b, 6, 0); /* "no alcanza": tiembla un poco */
        /* y dice qué le falta, para que se entienda */
        static lv_obj_t *aviso;
        if (aviso && lv_obj_is_valid(aviso)) lv_obj_delete(aviso);
        char t[96];
        const int fs = sem - s_pet->inventario[ITEM_SEMILLA], fp = pie - s_pet->inventario[ITEM_PIEDRA];
        if (fs > 0 && fp > 0) snprintf(t, sizeof t, "TE FALTAN %d SEMILLAS Y %d PIEDRAS BRILLANTES", fs, fp);
        else if (fs > 0) snprintf(t, sizeof t, "TE FALTAN %d SEMILLAS", fs);
        else snprintf(t, sizeof t, "TE FALTAN %d PIEDRAS BRILLANTES", fp);
        aviso = s_nino ? NULL : texto(s_tarjeta, t, 0xFFB0A0, 2, 300);
        if (aviso) lv_obj_align(aviso, LV_ALIGN_TOP_MID, 0, 294);
        return;
    }
    accion(UI_ACCION_DECORAR, d);
}

static const art_icono_t ICO_DECOR[DECOR_COUNT] = {ICO_FLOR, ICO_HONGO, ICO_FAROL, ICO_BANCO, ICO_CASITA};

/* --- Amigos cercanos --- */
#if MASCOTA_BLUETOOTH
static void invitar_cb(lv_event_t *e)
{
    int i = (int)(intptr_t)lv_event_get_user_data(e);
    accion(UI_ACCION_INVITAR, i);
    lv_obj_set_style_bg_color(lv_event_get_current_target(e), lv_color_hex(0x4A4458), 0);
}
#endif

static void vecino_invitar_cb(lv_event_t *e)
{
    (void)e;
    if (!s_pet || s_pet->vecino.presente) return;
    cerrar_panel();
    accion(ACT_INVITAR_VECINO, 0);
}

static void panel_amigos(void)
{
    abrir_tarjeta(PANEL_AMIGOS, 340, 340);
    if (!s_pet) return;
    lv_obj_t *t = s_nino ? icono(s_tarjeta, ICO_AMIGOS, 4) : texto(s_tarjeta, "AMIGOS", C_TINTA, 3, 0);
    lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 14);
    /* primero, su amigo del barrio (vive en este reloj) */
    const vecino_t *v = &s_pet->vecino;
    if (v->nombre[0]) {
        lv_obj_t *m = mini_criatura(s_tarjeta, PET_MAX_AMIGOS + 1, v->especie, s_pet->etapa, 0, 2, EXP_FELIZ);
        lv_obj_set_pos(m, 26, 60);
        lv_obj_t *n = texto(s_tarjeta, v->nombre, C_TINTA, 3, 0);
        lv_obj_set_pos(n, 76, 60);
        int corazones = ((int)v->amistad + 19) / 20;
        for (int k = 0; k < corazones && k < 5; k++) {
            lv_obj_t *h = icono(s_tarjeta, ICO_CORAZON, 2);
            lv_obj_set_pos(h, 78 + k * 14, 92);
        }
        lv_obj_t *b = boton(s_tarjeta, 92, 46, vecino_invitar_cb, 0);
        lv_obj_set_pos(b, 222, 60);
        lv_obj_t *bi = icono(b, v->presente ? ICO_CORAZON : ICO_CASITA, 3);
        lv_obj_center(bi);
        if (v->presente) lv_obj_set_style_bg_color(b, lv_color_hex(0x4A4458), 0);
    }
    int filas = v->nombre[0] ? 1 : 0;
#if MASCOTA_BLUETOOTH
    for (int i = 0; i < PET_MAX_AMIGOS && filas < 4; i++) {
        const amigo_t *a = &s_pet->amigos[i];
        if (!a->id) continue;
        int y = 56 + filas * 62;
        filas++;
        lv_obj_t *m = mini_criatura(s_tarjeta, i, a->especie, a->etapa, a->forma, 2, EXP_FELIZ);
        lv_obj_set_pos(m, 26, y + 4);
        lv_obj_t *n = texto(s_tarjeta, a->nombre, C_TINTA, 3, 0);
        lv_obj_set_pos(n, 76, y + 4);
        int corazones = (a->amistad + 19) / 20;
        for (int k = 0; k < corazones && k < 5; k++) {
            lv_obj_t *h = icono(s_tarjeta, ICO_CORAZON, 2);
            lv_obj_set_pos(h, 78 + k * 14, y + 36);
        }
        if (pet_friend_nearby(s_pet, i, s_ultimo_t)) {
            bool invitando = s_pet->invitando_a == a->id && s_ultimo_t < s_pet->envio_hasta;
            lv_obj_t *b = boton(s_tarjeta, 92, 46, invitar_cb, i);
            lv_obj_set_pos(b, 222, y + 4);
            lv_obj_t *bi = icono(b, invitando ? ICO_NOTA : ICO_PELOTA, 3);
            lv_obj_center(bi);
            if (invitando) lv_obj_set_style_bg_color(b, lv_color_hex(0x4A4458), 0);
        }
    }
    if (filas <= 1) {
        if (!s_nino) {
            lv_obj_t *n = texto(s_tarjeta, "ACERCA OTRO ORBE CON UNA MASCOTA Y APARECERÁ AQUÍ.", C_GRIS, 2, 270);
            lv_obj_align(n, LV_ALIGN_CENTER, 0, 96);
        }
        lv_obj_t *ic = icono(s_tarjeta, ICO_LUPA, 4);
        lv_obj_align(ic, LV_ALIGN_CENTER, 0, 46);
    }
#else
    if (!filas) { /* todavía no la visita nadie del barrio */
        lv_obj_t *ic = icono(s_tarjeta, ICO_CASITA, 4);
        lv_obj_align(ic, LV_ALIGN_CENTER, 0, 10);
        if (!s_nino) {
            lv_obj_t *n = texto(s_tarjeta, "PRONTO VENDRÁ ALGUIEN DEL BARRIO A CONOCERL{A}.", C_GRIS, 2, 260);
            lv_obj_align(n, LV_ALIGN_CENTER, 0, 70);
        }
    }
#endif
}

/* --- Invitación de un amigo --- */
static void respuesta_cb(lv_event_t *e)
{
    int si = (int)(intptr_t)lv_event_get_user_data(e);
    cerrar_panel();
    accion(UI_ACCION_RESPONDER, si);
}

static void panel_invitacion(int i)
{
    if (!s_pet || i < 0 || i >= PET_MAX_AMIGOS) return;
    abrir_tarjeta(PANEL_INVITACION, 320, 300);
    const amigo_t *a = &s_pet->amigos[i];
    lv_obj_t *m = mini_criatura(s_tarjeta, PET_MAX_AMIGOS, a->especie, a->etapa, a->forma, 4, EXP_FELIZ);
    lv_obj_align(m, LV_ALIGN_TOP_MID, s_nino ? -40 : 0, 14);
    if (s_nino) { lv_obj_t *p = icono(s_tarjeta, ICO_PELOTA, 5); lv_obj_align(p, LV_ALIGN_TOP_MID, 60, 40); }
    else {
        char t[64];
        snprintf(t, sizeof t, "¡%s TE INVITA A JUGAR!", a->nombre);
        lv_obj_t *tx = texto(s_tarjeta, t, C_TINTA, 3, 280);
        lv_obj_align(tx, LV_ALIGN_TOP_MID, 0, 104);
    }
    for (int k = 0; k < 2; k++) {
        lv_obj_t *b = boton(s_tarjeta, 110, 70, respuesta_cb, k == 0 ? 1 : 0);
        lv_obj_align(b, LV_ALIGN_BOTTOM_MID, k == 0 ? -62 : 62, -18);
        lv_obj_t *ic = icono(b, k == 0 ? ICO_SI : ICO_NO, 4);
        lv_obj_center(ic);
    }
}

/* --- Mensaje del día --- */
/* --- texto largo: un área que se desliza con el dedo, con una flechita mientras quede más abajo --- */
static void area_scroll_cb(lv_event_t *e)
{
    lv_obj_t *c = lv_event_get_target(e), *fl = (lv_obj_t *)lv_event_get_user_data(e);
    ocultar(fl, lv_obj_get_scroll_bottom(c) <= 2);
}
static lv_obj_t *area_texto(lv_obj_t *padre, int x, int y, int w, int h)
{
    lv_obj_t *c = lv_obj_create(padre);
    lv_obj_remove_style_all(c);
    lv_obj_set_pos(c, x, y);
    lv_obj_set_size(c, w, h);
    lv_obj_set_flex_flow(c, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(c, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(c, 12, 0);
    lv_obj_set_style_pad_bottom(c, 8, 0);
    lv_obj_set_scroll_dir(c, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(c, LV_SCROLLBAR_MODE_OFF); /* sin barra ni flechita: si hay más, se desliza */
    lv_obj_set_style_bg_color(c, lv_color_hex(C_VIOLETA), LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_opa(c, LV_OPA_60, LV_PART_SCROLLBAR);
    lv_obj_set_style_width(c, 4, LV_PART_SCROLLBAR);
    lv_obj_add_flag(c, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(c, LV_OBJ_FLAG_GESTURE_BUBBLE);
    return c;
}
/* después de llenarla: si no cabe todo, la barra y la flechita avisaban que hay más. Ahora no se muestran
 * (si hay más, se desliza); para volver a mostrarlas, AVISO_DESLIZA en 1 */
#define AVISO_DESLIZA 0
static void area_lista(lv_obj_t *c)
{
    lv_obj_update_layout(c);
    if (!AVISO_DESLIZA || lv_obj_get_scroll_bottom(c) <= 2) return;
    lv_obj_set_scrollbar_mode(c, LV_SCROLLBAR_MODE_ON);
    lv_obj_t *fl = icono(lv_obj_get_parent(c), ICO_FLECHA, 2);
    lv_image_set_pivot(fl, 12, 12);
    lv_image_set_rotation(fl, 900);
    lv_obj_set_pos(fl, lv_obj_get_x(c) + lv_obj_get_width(c) / 2 - 12, lv_obj_get_y(c) + lv_obj_get_height(c) - 14);
    lv_obj_set_style_opa(fl, LV_OPA_70, 0);
    lv_obj_add_event_cb(c, area_scroll_cb, LV_EVENT_SCROLL, fl);
}

/* responder el mensaje del día: el botón cambia según el mensaje; la reacción, según el modo */
static lv_obj_t *s_aura;
static uint32_t s_resp_toque_hasta;
static void reaccion_respuesta(void)
{
    if (!s_pet) return;
    int hx = (int)s_mascota.x, hy = cabeza_y(&s_mascota);
    if (s_pet->modo_nino == MODO_INFANTE) { /* salto, ojitos felices y tres corazones */
        mascota_modo(M_SALTA, 1800);
        for (int i = 0; i < 3; i++) flotar(ICO_CORAZON, NULL, 0, hx - 24 + i * 24, hy - 10, (uint32_t)i * 200);
        accion(UI_ACCION_SONAR, SND_ALEGRE);
    } else if (s_pet->modo_nino == MODO_NINO) { /* corre al vidrio; si la tocas al tiro, más corazones */
        mascota_modo(M_CRISTAL, 4200);
        s_resp_toque_hasta = s_ms + 5000;
        accion(UI_ACCION_SONAR, SND_ALEGRE);
    } else { /* una reverencia de agradecimiento, un halo dorado y chispas en las flores */
        if (!s_aura) {
            s_aura = caja(s_mundo);
            lv_obj_set_style_radius(s_aura, LV_RADIUS_CIRCLE, 0);
            lv_obj_set_style_bg_color(s_aura, lv_color_hex(0xFFE27A), 0);
            lv_obj_set_style_bg_opa(s_aura, LV_OPA_20, 0);
            lv_obj_set_style_shadow_color(s_aura, lv_color_hex(0xFFD45C), 0);
            lv_obj_set_style_shadow_spread(s_aura, 4, 0);
        }
        lv_obj_move_to_index(s_aura, lv_obj_get_index(s_mascota.img));
        mascota_modo(M_REVERENCIA, 2400);
        for (int i = 0; i < 5; i++) flotar(ICO_ESTRELLA, NULL, 0, 90 + (int)(azar() % 290), 200 + (int)(azar() % 110), 300 + (uint32_t)i * 220);
        accion(UI_ACCION_SONAR, SND_RONRONEO);
    }
}
static void frase_resp_cb(lv_event_t *e)
{
    (void)e;
    cerrar_panel();
    accion(UI_ACCION_RESPUESTA, 0);
    reaccion_respuesta();
}
static void boton_respuesta(int frase)
{
    if (s_nino) { /* un corazón, sin palabras */
        lv_obj_t *b = boton(s_tarjeta, 130, 52, frase_resp_cb, 0);
        lv_obj_align(b, LV_ALIGN_BOTTOM_MID, 0, -14);
        lv_obj_t *i = icono(b, ICO_CORAZON, 3);
        lv_obj_center(i);
        return;
    }
    lv_obj_t *b = boton(s_tarjeta, 190, 50, frase_resp_cb, 0);
    lv_obj_align(b, LV_ALIGN_BOTTOM_MID, 0, -14);
    lv_obj_t *t = texto(b, pet_reply_text(pet_phrase_reply(frase)), C_BLANCO, 2, 0);
    lv_obj_center(t);
}
static bool s_frase_botella; /* el mensaje llegó en una botella (no lo dice ella) */
static void panel_frase(int frase)
{
    if (!s_pet) return;
    bool botella = s_frase_botella;
    s_frase_botella = false;
    abrir_tarjeta(PANEL_FRASE, 340, 372);
    emo_t a, b;
    pet_top_emotions(s_pet, &a, &b);
    if (s_nino) { /* sin texto: la mascota te manda un corazón */
        lv_obj_t *m = mini_criatura(s_tarjeta, PET_MAX_AMIGOS + 1, pet_look(s_pet), s_pet->etapa, s_pet->forma, 6, EXP_FELIZ);
        lv_obj_align(m, LV_ALIGN_CENTER, 0, -20);
        lv_obj_t *h = icono(s_tarjeta, ICO_CORAZON, 4);
        lv_obj_align(h, LV_ALIGN_TOP_MID, 70, 30);
        lv_obj_t *e1 = icono(s_tarjeta, icono_emocion(a), 3);
        lv_obj_align(e1, LV_ALIGN_TOP_MID, -70, 40);
        boton_respuesta(frase);
        return;
    }
    if (botella) { /* el mensaje de la botella, en su papelito enrollado: se lee como las cartas */
        lv_obj_t *papel = caja(s_tarjeta);
        lv_obj_set_size(papel, 296, 246);
        lv_obj_align(papel, LV_ALIGN_TOP_MID, 0, 50);
        lv_obj_set_style_bg_color(papel, lv_color_hex(0xF6E8C6), 0);
        lv_obj_set_style_bg_opa(papel, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(papel, 14, 0);
        lv_obj_set_style_border_width(papel, 3, 0);
        lv_obj_set_style_border_color(papel, lv_color_hex(0xB8925A), 0);
        lv_obj_remove_flag(papel, LV_OBJ_FLAG_CLICKABLE);
    }
    lv_obj_t *ic = icono(s_tarjeta, botella ? ICO_OBJETO(OBJ_BOTELLA) : ICO_CORAZON, 3);
    lv_obj_align(ic, LV_ALIGN_TOP_MID, 0, 16);
    char t[48];
    if (botella) snprintf(t, sizeof t, "UNA BOTELLA TRAJO ESTE MENSAJE:");
    else snprintf(t, sizeof t, "%s TE DICE:", s_pet->nombre);
    lv_obj_t *ti = texto(s_tarjeta, t, C_GRIS, 2, 280);
    lv_obj_align(ti, LV_ALIGN_TOP_MID, 0, 56);
    const lv_image_dsc_t *tid = lv_image_get_src(ti);
    const int ty_fin = 56 + (tid ? (int)tid->header.h : 16) + 10; /* el título puede ocupar dos renglones: la frase empieza debajo */
    /* la frase y cómo se siente, una debajo de la otra; si no cabe, se desliza */
    const char *fz = pet_phrase(frase);
    lv_obj_t *ar = area_texto(s_tarjeta, 20, ty_fin, 300, 288 - ty_fin);
    texto(ar, fz, C_TINTA, strlen(fz) > 48 ? 2 : 3, 280);
    if (!botella) {
        char sent[128];
        pet_feeling(s_pet, sent, sizeof sent);
        texto(ar, sent, C_VIOLETA, 2, 270);
    }
    area_lista(ar);
    boton_respuesta(frase);
}

/* --- Cartas de sus amigos: llega un sobre grande con su sello de cera; al tocarlo se rompe el sello, se abre la
 * solapa, sale la hoja y se lee (si es larga, se desliza). Guardadas en Recuerdos → Cartas. --- */
static art_img_t s_carta_img;
static lv_obj_t *s_carta_sobre, *s_carta_pista, *s_carta_aura, *s_carta_chis[5];
static int s_carta_i, s_carta_fase = -1;   /* -1: no se está abriendo */
static uint32_t s_carta_t, s_carta_vista_ms;
static void panel_cartas(void);
static void boton_volver(lv_event_cb_t cb, int id, int x, int y_abajo);
static void carta_volver_cb(lv_event_t *e) { (void)e; panel_cartas(); }
static void carta_escala_cb(void *o, int32_t v) { lv_obj_set_style_transform_scale((lv_obj_t *)o, v, 0); }
static void carta_leer(int i, bool animada)
{
    if (!s_pet) return;
    char de[24], txt[640];
    if (!pet_letter_get(s_pet, i, de, sizeof de, txt, sizeof txt)) { cerrar_panel(); return; }
    abrir_tarjeta(PANEL_CARTA, 360, 384);
    s_carta_fase = -1;
    s_carta_sobre = NULL; s_carta_aura = NULL; for (int k = 0; k < 5; k++) s_carta_chis[k] = NULL;
    lv_obj_t *papel = lv_obj_create(s_tarjeta);
    lv_obj_remove_style_all(papel);
    int pw = zona_ancho(80) + 40; /* la hoja: tan ancha como deja el círculo arriba (las esquinas son redondas) */
    lv_obj_set_size(papel, pw > 280 ? 280 : pw, 300);
    lv_obj_align(papel, LV_ALIGN_TOP_MID, 0, 34);
    /* el papel de cada quien: rosado de amigo, crema de familia, celeste de viajero, verdoso de criatura */
    static const uint32_t PAPEL[4] = {0xFFF0EA, 0xFBF2DC, 0xEAF3FC, 0xEBF7E6}, BORDE[4] = {0xD4707C, 0xB8925A, 0x5E8EC4, 0x5EA070};
    static const uint32_t CINTA[4] = {0xF2A0A8, 0xE8C26A, 0x9CC4F0, 0x9CD8A8};
    int tipo = (int)pet_letter_from(s_pet, i);
    if (tipo < 0 || tipo > 3) tipo = 0;
    lv_obj_set_style_bg_color(papel, lv_color_hex(PAPEL[tipo]), 0);
    lv_obj_set_style_bg_opa(papel, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(papel, 16, 0);
    lv_obj_set_style_border_width(papel, 3, 0);
    lv_obj_set_style_border_color(papel, lv_color_hex(BORDE[tipo]), 0);
    lv_obj_set_style_shadow_width(papel, 16, 0);
    lv_obj_set_style_shadow_color(papel, lv_color_hex(BORDE[tipo]), 0);
    lv_obj_set_style_shadow_opa(papel, LV_OPA_30, 0);
    lv_obj_remove_flag(papel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *marco = caja(papel); /* el marco dorado de adentro, como los pergaminos encantados */
    lv_obj_set_size(marco, pw > 280 ? 262 : pw - 18, 282);
    lv_obj_align(marco, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_opa(marco, LV_OPA_TRANSP, 0);
    lv_obj_set_style_radius(marco, 11, 0);
    lv_obj_set_style_border_width(marco, 2, 0);
    lv_obj_set_style_border_color(marco, lv_color_hex(0xE0B84A), 0);
    lv_obj_set_style_border_opa(marco, LV_OPA_80, 0);
    lv_obj_remove_flag(marco, LV_OBJ_FLAG_CLICKABLE);
    for (int k = 0; k < 4; k++) { /* un rombito dorado en cada esquina del marco */
        lv_obj_t *r = caja(papel);
        lv_obj_set_size(r, 9, 9);
        lv_obj_set_style_bg_color(r, lv_color_hex(0xF4CC5A), 0);
        lv_obj_set_style_bg_opa(r, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(r, 1, 0);
        lv_obj_set_style_border_color(r, lv_color_hex(0x9A6E10), 0);
        lv_obj_set_style_radius(r, 1, 0);
        lv_obj_set_style_transform_rotation(r, 450, 0);
        lv_obj_align(r, k & 1 ? LV_ALIGN_TOP_RIGHT : LV_ALIGN_TOP_LEFT, k & 1 ? -4 : 4, k & 2 ? 280 : 6);
        if (k & 2) lv_obj_align(r, k & 1 ? LV_ALIGN_BOTTOM_RIGHT : LV_ALIGN_BOTTOM_LEFT, k & 1 ? -4 : 4, -6);
        lv_obj_remove_flag(r, LV_OBJ_FLAG_CLICKABLE);
    }
    lv_obj_t *cinta = caja(papel); /* una cinta de color arriba, como papel de carta */
    lv_obj_set_size(cinta, 220, 6);
    lv_obj_set_style_bg_color(cinta, lv_color_hex(CINTA[tipo]), 0);
    lv_obj_set_style_bg_opa(cinta, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(cinta, 3, 0);
    lv_obj_align(cinta, LV_ALIGN_TOP_MID, 0, 8);
    char para[48];
    snprintf(para, sizeof para, "PARA %s", s_pet->nombre);
    lv_obj_t *ph = texto(papel, para, BORDE[tipo], 2, 180);
    lv_obj_align(ph, LV_ALIGN_TOP_LEFT, 18, 22);
    lv_obj_t *se = icono(papel, ICO_CARTA, 2); /* el sobre, de adorno, en la esquina */
    lv_obj_align(se, LV_ALIGN_TOP_RIGHT, -12, 18);
    lv_obj_t *rayita = caja(papel); /* una raya con una estrellita en medio, bajo el "PARA" */
    lv_obj_set_size(rayita, 200, 1);
    lv_obj_set_style_bg_color(rayita, lv_color_hex(0xE0B84A), 0);
    lv_obj_set_style_bg_opa(rayita, LV_OPA_COVER, 0);
    lv_obj_align(rayita, LV_ALIGN_TOP_MID, 0, 46);
    lv_obj_t *est = icono(papel, ICO_ESTRELLA, 1);
    lv_obj_align(est, LV_ALIGN_TOP_MID, 0, 38);
    lv_obj_t *ar = area_texto(papel, 16, 54, 248, 222);
    lv_obj_set_flex_align(ar, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(ar, 10, 0);
    texto_l(ar, txt, C_TINTA, 2, 226);
    area_lista(ar);
    if (animada) { /* la hoja se despliega */
        lv_obj_set_style_transform_pivot_x(papel, 150, 0);
        lv_obj_set_style_transform_pivot_y(papel, 280, 0);
        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, papel);
        lv_anim_set_values(&a, 90, 256);
        lv_anim_set_time(&a, 320);
        lv_anim_set_path_cb(&a, lv_anim_path_overshoot);
        lv_anim_set_exec_cb(&a, carta_escala_cb);
        lv_anim_start(&a);
    }
    if (!animada) boton_volver(carta_volver_cb, 0, -128, -20); /* volver a la lista de cartas */
}

static void carta_abrir_cb(lv_event_t *e)
{
    (void)e;
    if (s_carta_fase != 0 || !s_pet) return;
    s_carta_fase = 1;
    s_carta_t = s_ms;
    art_carta(&s_carta_img, 1, 4);
    poner_src(s_carta_sobre, &s_carta_img.dsc);
    if (s_carta_pista) ocultar(s_carta_pista, true);
    accion(UI_ACCION_SONAR, SND_PLOP);
    accion(UI_ACCION_CARTA, 0);
}
/* la carta que acaba de llegar: el sobre cerrado, grande, que se mece suave */
static void panel_carta_llega(void)
{
    if (!s_pet || !pet_letter_count(s_pet)) return;
    char de[24];
    pet_letter_get(s_pet, 0, de, sizeof de, NULL, 0);
    abrir_tarjeta(PANEL_CARTA, 360, 384);
    s_carta_i = 0;
    s_carta_fase = 0;
    s_carta_t = s_ms;
    s_carta_vista_ms = s_ms;
    art_carta_tipo((int)pet_letter_from(s_pet, 0));
    char t[64];
    snprintf(t, sizeof t, "¡UNA CARTA PARA %s!", s_pet->nombre);
    lv_obj_t *ti = texto(s_tarjeta, t, C_TINTA, 2, 300);
    lv_obj_align(ti, LV_ALIGN_TOP_MID, 0, 20);
    static const uint32_t AURA[4] = {0xF2A0A8, 0xE8C26A, 0x9CC4F0, 0x9CD8A8};
    int ctipo = (int)pet_letter_from(s_pet, 0);
    s_carta_aura = caja(s_tarjeta); /* un resplandor suave detrás del sobre */
    lv_obj_set_size(s_carta_aura, 150, 90);
    lv_obj_align(s_carta_aura, LV_ALIGN_TOP_MID, 0, 120);
    lv_obj_set_style_bg_color(s_carta_aura, lv_color_hex(AURA[ctipo < 0 || ctipo > 3 ? 0 : ctipo]), 0);
    lv_obj_set_style_bg_opa(s_carta_aura, LV_OPA_30, 0);
    lv_obj_set_style_radius(s_carta_aura, LV_RADIUS_CIRCLE, 0);
    /* (sin sombra difuminada: recalcularla cada cuadro con el sobre meciéndose bajaba la animación a 16 cuadros en 3 s) */
    lv_obj_set_size(s_carta_aura, 230, 150);
    lv_obj_align(s_carta_aura, LV_ALIGN_TOP_MID, 0, 90);
    lv_obj_set_style_bg_opa(s_carta_aura, LV_OPA_20, 0);
    lv_obj_remove_flag(s_carta_aura, LV_OBJ_FLAG_CLICKABLE);
    static const int16_t CH[5][2] = {{-110, 90}, {112, 100}, {-136, 170}, {130, 196}, {-96, 262}};
    for (int k = 0; k < 5; k++) { /* chispas que titilan alrededor */
        s_carta_chis[k] = icono(s_tarjeta, ICO_ESTRELLA, k % 2 ? 1 : 2);
        lv_obj_align(s_carta_chis[k], LV_ALIGN_TOP_MID, CH[k][0] * 3 / 4, CH[k][1]);
        lv_obj_remove_flag(s_carta_chis[k], LV_OBJ_FLAG_CLICKABLE);
    }
    art_carta(&s_carta_img, 0, 4);
    s_carta_sobre = lv_image_create(s_tarjeta);
    lv_image_set_src(s_carta_sobre, &s_carta_img.dsc);
    lv_obj_align(s_carta_sobre, LV_ALIGN_TOP_MID, 0, 44);
    lv_obj_add_flag(s_carta_sobre, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_carta_sobre, carta_abrir_cb, LV_EVENT_CLICKED, NULL);
    snprintf(t, sizeof t, "DE %s", de);
    s_carta_pista = lv_obj_create(s_tarjeta);
    lv_obj_remove_style_all(s_carta_pista);
    lv_obj_set_size(s_carta_pista, 300, 56);
    lv_obj_align(s_carta_pista, LV_ALIGN_TOP_MID, 0, 290);
    lv_obj_t *d = texto(s_carta_pista, t, C_VIOLETA, 2, 290);
    lv_obj_align(d, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_t *pi = texto(s_carta_pista, "TOCA EL SELLO PARA ABRIRLA", C_GRIS, 2, 290);
    lv_obj_align(pi, LV_ALIGN_TOP_MID, 0, 24);
    accion(UI_ACCION_SONAR, SND_NUEVO);
}
/* cada cuadro: el sobre se mece mientras espera y, al tocarlo, se abre por fases */
static void animar_carta(void)
{
    if (s_panel != PANEL_CARTA || s_carta_fase < 0 || !s_carta_sobre) return;
    uint32_t dt = s_ms - s_carta_t;
    if (s_carta_fase == 0) { /* se mece: invita a tocarlo */
        int dy = (int)lroundf(sinf((float)dt * 0.004f) * 4.0f);
        lv_obj_align(s_carta_sobre, LV_ALIGN_TOP_MID, 0, 44 + dy);
        for (int k = 0; k < 5; k++) /* las chispas titilan, cada una a su ritmo */
            if (s_carta_chis[k]) lv_obj_set_style_opa(s_carta_chis[k], (lv_opa_t)(110 + 145 * (0.5f + 0.5f * sinf((float)dt * 0.005f + (float)k * 1.7f))), 0);
        if (s_carta_aura) lv_obj_set_style_shadow_opa(s_carta_aura, (lv_opa_t)(110 + 70 * sinf((float)dt * 0.003f)), 0);
        return;
    }
    static const uint16_t T[7] = {0, 0, 260, 420, 520, 620, 720}; /* cuándo empieza cada fase */
    int f = s_carta_fase;
    if (f < 6 && dt >= T[f + 1]) {
        f++;
        s_carta_fase = f;
        art_carta(&s_carta_img, f, 4);
        poner_src(s_carta_sobre, &s_carta_img.dsc);
        if (f == 2) accion(UI_ACCION_SONAR, SND_DESLIZA);
        lv_obj_align(s_carta_sobre, LV_ALIGN_TOP_MID, 0, 44);
    } else if (f == 6 && dt >= 980) {
        carta_leer(s_carta_i, true);
    }
}

/* Recuerdos → Cartas: las que le escribieron, la más nueva arriba */
static void cartas_cb(lv_event_t *e)
{
    int i = (int)(intptr_t)lv_event_get_user_data(e);
    if (i == 0 && s_pet && pet_letter_pending(s_pet)) { panel_carta_llega(); return; }
    carta_leer(i, false);
}
static void panel_cartas(void)
{
    if (!s_pet) return;
    abrir_tarjeta(PANEL_CARTAS, 360, 384);
    lv_obj_t *ic = icono(s_tarjeta, ICO_CARTA, 3);
    lv_obj_align(ic, LV_ALIGN_TOP_MID, -70, 18);
    lv_obj_t *ti = texto(s_tarjeta, "CARTAS", C_TINTA, 3, 0);
    lv_obj_align(ti, LV_ALIGN_TOP_MID, 14, 26);
    lv_obj_t *ar = area_texto(s_tarjeta, 30, 70, 300, 270);
    lv_obj_set_style_pad_row(ar, 6, 0);
    for (int i = 0; i < pet_letter_count(s_pet); i++) {
        char de[24], t[40];
        pet_letter_get(s_pet, i, de, sizeof de, NULL, 0);
        bool cerrada = i == 0 && pet_letter_pending(s_pet);
        lv_obj_t *b = boton(ar, 280, 50, cartas_cb, i);
        lv_obj_set_style_radius(b, 18, 0);
        lv_obj_add_flag(b, LV_OBJ_FLAG_SCROLL_CHAIN_VER);
        if (cerrada) lv_obj_set_style_bg_color(b, lv_color_hex(C_VIOLETA), 0);
        lv_obj_t *bi = icono(b, ICO_CARTA, 2);
        lv_obj_align(bi, LV_ALIGN_LEFT_MID, 12, 0);
        snprintf(t, sizeof t, cerrada ? "DE %s (NUEVA)" : "DE %s", de);
        lv_obj_t *bt = texto(b, t, C_BLANCO, 2, 210);
        lv_obj_align(bt, LV_ALIGN_LEFT_MID, 50, 0);
    }
    area_lista(ar);
}

/* --- Fecha de nacimiento del dueño (calcula la edad, el modo y el día de tu cumpleaños) --- */
static int s_fn_dia = 1, s_fn_mes = 1, s_fn_anio = 0;
static lv_obj_t *s_fn_val[3];
static art_img_t s_fn_art[3];
static const int16_t FN_X[3] = {-112, 0, 112};

static int fn_dias_mes(int m, int a)
{
    static const uint8_t D[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    bool bis = (a % 4 == 0 && a % 100 != 0) || a % 400 == 0;
    return m == 2 && bis ? 29 : D[(m - 1) % 12];
}

static int fn_hoy_anio(void)
{
    int a = 2026;
    if (s_pet && s_ultimo_t) pet_local_date(s_pet, s_ultimo_t, &a, NULL, NULL);
    return a;
}

/* fondo nocturno compartido por la fecha y la hora; el contenido baja NY para caer dentro del panel oscuro */
#define NY 50
static void abrir_noche(panel_t p)
{
    abrir_tarjeta(p, 466, 466);
    lv_obj_align(s_tarjeta, LV_ALIGN_CENTER, 0, 0);
    if (art_noche(&s_noche_art)) { lv_obj_t *fo = lv_image_create(s_tarjeta); lv_image_set_src(fo, &s_noche_art.dsc); lv_obj_align(fo, LV_ALIGN_TOP_LEFT, 0, 0); }
}
static void flecha_noche(lv_obj_t *b) { lv_obj_set_style_border_color(b, lv_color_hex(0xF0C870), 0); lv_obj_set_style_border_width(b, 2, 0); }
static void fecha_refrescar(void)
{
    static const char *const MES[12] = {"ENE", "FEB", "MAR", "ABR", "MAY", "JUN", "JUL", "AGO", "SEP", "OCT", "NOV", "DIC"};
    int dm = fn_dias_mes(s_fn_mes, s_fn_anio);
    if (s_fn_dia > dm) s_fn_dia = dm;
    char t[3][16];
    snprintf(t[0], sizeof t[0], "%d", s_fn_dia);
    snprintf(t[1], sizeof t[1], "%s", MES[(s_fn_mes - 1) % 12]);
    snprintf(t[2], sizeof t[2], "%d", s_fn_anio);
    for (int i = 0; i < 3; i++) {
        art_texto(&s_fn_art[i], t[i], 0xFFEFB8, i == 2 ? 3 : 4, 0);
        poner_src(s_fn_val[i], &s_fn_art[i].dsc);
        lv_obj_align(s_fn_val[i], LV_ALIGN_TOP_MID, FN_X[i], NY + (i == 2 ? 158 : 152));
    }
}

static void fecha_cb(lv_event_t *e)
{
    int k = (int)(intptr_t)lv_event_get_user_data(e);
    if (k == 6) {
        cerrar_panel();
        accion(UI_ACCION_NACIMIENTO, s_fn_anio * 10000 + s_fn_mes * 100 + s_fn_dia);
        return;
    }
    int col = k / 2, d = (k % 2) ? -1 : 1;
    if (col == 0) {
        int dm = fn_dias_mes(s_fn_mes, s_fn_anio);
        s_fn_dia += d;
        if (s_fn_dia < 1) s_fn_dia = dm;
        if (s_fn_dia > dm) s_fn_dia = 1;
    } else if (col == 1) {
        s_fn_mes += d;
        if (s_fn_mes < 1) s_fn_mes = 12;
        if (s_fn_mes > 12) s_fn_mes = 1;
    } else {
        int hoy = fn_hoy_anio();
        s_fn_anio += d;
        if (s_fn_anio > hoy) s_fn_anio = hoy;
        if (s_fn_anio < hoy - 100) s_fn_anio = hoy - 100;
    }
    fecha_refrescar();
}

static void panel_fecha(void)
{
    abrir_noche(PANEL_FECHA);
    if (!s_fn_anio) s_fn_anio = fn_hoy_anio() - 10;
    lv_obj_t *ic = icono(s_tarjeta, ICO_CALENDARIO, 3);
    lv_obj_align(ic, LV_ALIGN_TOP_MID, 0, NY + 12);
    lv_obj_t *t = texto(s_tarjeta, "¿CUÁNDO NACISTE?", 0xFFEFB8, 3, 0);
    lv_obj_align(t, LV_ALIGN_TOP_MID, 0, NY + 54);
    static const char *const ROT[3] = {"DÍA", "MES", "AÑO"};
    for (int c = 0; c < 3; c++) {
        lv_obj_t *r = texto(s_tarjeta, ROT[c], 0xB8A8E8, 2, 0);
        lv_obj_align(r, LV_ALIGN_TOP_MID, FN_X[c], NY + 86);
        for (int k = 0; k < 2; k++) {
            lv_obj_t *b = boton(s_tarjeta, 84, 40, fecha_cb, c * 2 + k);
            /* un toque mueve de a uno; dejarlo apretado avanza rápido (para el año) */
            lv_obj_remove_event_cb(b, fecha_cb);
            lv_obj_add_event_cb(b, fecha_cb, LV_EVENT_SHORT_CLICKED, (void *)(intptr_t)(c * 2 + k));
            lv_obj_add_event_cb(b, fecha_cb, LV_EVENT_LONG_PRESSED_REPEAT, (void *)(intptr_t)(c * 2 + k));
            lv_obj_align(b, LV_ALIGN_TOP_MID, FN_X[c], NY + (k == 0 ? 106 : 196));
            flecha_noche(b);
            lv_obj_t *fl = icono(b, ICO_FLECHA, 2);
            lv_image_set_pivot(fl, 12, 12);
            lv_image_set_rotation(fl, k == 0 ? 2700 : 900);
            lv_obj_center(fl);
        }
        s_fn_val[c] = lv_image_create(s_tarjeta);
    }
    fecha_refrescar();
    lv_obj_t *ok = boton(s_tarjeta, 150, 50, fecha_cb, 6);
    ok_alinear(ok, 0);
    lv_obj_set_style_bg_color(ok, lv_color_hex(C_VERDE), 0);
    lv_obj_t *ot = texto(ok, "ACEPTAR", C_BLANCO, 3, 0);
    lv_obj_center(ot);
}

/* --- La hora de hoy: al empezar y si el reloj la perdió (se quedó sin batería) --- */
static int s_hr_paso;                 /* 0: el día de hoy; 1: la hora */
static int s_hr[5] = {1, 1, 2026, 12, 0}; /* día, mes, año, hora, minuto */
static bool s_hr_error;
static lv_obj_t *s_hr_val[3];
static art_img_t s_hr_art[3];
static void panel_hora(void);

static int64_t ui_dias_civil(int y, int m, int d)
{
    y -= m <= 2;
    int64_t era = (y >= 0 ? y : y - 399) / 400, yoe = y - era * 400;
    int64_t doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    return era * 146097 + yoe * 365 + yoe / 4 - yoe / 100 + doy - 719468;
}

static int64_t ui_dia_local(uint32_t t) { int64_t l = (int64_t)t + (s_pet ? s_pet->zona_horaria : 0); return l >= 0 ? l / 86400 : (l - 86399) / 86400; }
static int ui_dia_semana(int64_t d) { return (int)(((d + 3) % 7 + 7) % 7); } /* 0 lunes */
static const char *const DIA_NOMBRE[7] = {"LUNES", "MARTES", "MIÉRCOLES", "JUEVES", "VIERNES", "SÁBADO", "DOMINGO"};
static const char *const DIA_CORTO[7] = {"LU", "MA", "MI", "JU", "VI", "SA", "DO"};

static void hora_desde(uint32_t utc) /* rellena con una hora conocida */
{
    if (!s_pet) return;
    pet_local_date(s_pet, utc, &s_hr[2], &s_hr[1], &s_hr[0]);
    int64_t l = (int64_t)utc + s_pet->zona_horaria;
    int seg = (int)(((l % 86400) + 86400) % 86400);
    s_hr[3] = seg / 3600;
    s_hr[4] = seg / 60 % 60;
}

static void hora_refrescar(void)
{
    static const char *const MES[12] = {"ENE", "FEB", "MAR", "ABR", "MAY", "JUN", "JUL", "AGO", "SEP", "OCT", "NOV", "DIC"};
    char t[3][16];
    int n = s_hr_paso == 0 ? 3 : 2;
    if (s_hr_paso == 0) {
        int dm = fn_dias_mes(s_hr[1], s_hr[2]);
        if (s_hr[0] > dm) s_hr[0] = dm;
        snprintf(t[0], sizeof t[0], "%d", s_hr[0]);
        snprintf(t[1], sizeof t[1], "%s", MES[(s_hr[1] - 1) % 12]);
        snprintf(t[2], sizeof t[2], "%d", s_hr[2]);
    } else {
        snprintf(t[0], sizeof t[0], "%02d", s_hr[3]);
        snprintf(t[1], sizeof t[1], "%02d", s_hr[4]);
    }
    for (int i = 0; i < n; i++) {
        art_texto(&s_hr_art[i], t[i], 0xFFEFB8, i == 2 ? 3 : 4, 0);
        poner_src(s_hr_val[i], &s_hr_art[i].dsc);
        if (n == 3) lv_obj_align(s_hr_val[i], LV_ALIGN_TOP_MID, FN_X[i], NY + (i == 2 ? 158 : 152));
        else lv_obj_align(s_hr_val[i], LV_ALIGN_TOP_MID, 0, NY + (i ? 186 : 116)); /* la hora arriba y los minutos abajo */
    }
}

static void hora_cb(lv_event_t *e)
{
    int k = (int)(intptr_t)lv_event_get_user_data(e);
    if (k == 6) {
        if (s_hr_paso == 0) { s_hr_paso = 1; s_hr_error = false; panel_hora(); return; }
        int64_t local = ui_dias_civil(s_hr[2], s_hr[1], s_hr[0]) * 86400 + s_hr[3] * 3600 + s_hr[4] * 60;
        int64_t utc = local - (s_pet ? s_pet->zona_horaria : 0);
        bool primera = !s_pet || !s_pet->edad_dueno;
        const bool al_pasado = !primera && !s_hora_dudosa && utc + 120 < (int64_t)s_pet->ultima_actualizacion;
        cerrar_panel();
        s_hr_paso = 0;
        if (al_pasado) { /* otra fecha, anterior: solo para este perfil, la hora del reloj no cambia */
            accion(UI_ACCION_FECHA_OTRA, (int)local);
            globo(ICO_CALENDARIO, s_nino ? NULL : "OTRA FECHA, SOLO PARA ESTE PERFIL. LA HORA DEL ORBE NO CAMBIÓ", 4500);
            return;
        }
        s_hora_dudosa = false;
        accion(UI_ACCION_HORA, (int)local);
        if (primera) panel_fecha();
        return;
    }
    if (k == 7) { s_hr_paso = 0; panel_hora(); return; }
    if (k == 8) { /* volver a la hora real del reloj */
        cerrar_panel();
        s_hr_paso = 0;
        accion(UI_ACCION_FECHA_OTRA, (int)((int64_t)s_ultimo_t - (s_pet ? s_pet->desfase_s : 0) + (s_pet ? s_pet->zona_horaria : 0)));
        globo(ICO_RELOJ, s_nino ? NULL : "DE VUELTA EN LA HORA REAL", 3000);
        return;
    }
    int col = k / 2, d = (k % 2) ? -1 : 1;
    if (s_hr_paso == 0) {
        if (col == 0) {
            int dm = fn_dias_mes(s_hr[1], s_hr[2]);
            s_hr[0] += d;
            if (s_hr[0] < 1) s_hr[0] = dm;
            if (s_hr[0] > dm) s_hr[0] = 1;
        } else if (col == 1) {
            s_hr[1] += d;
            if (s_hr[1] < 1) s_hr[1] = 12;
            if (s_hr[1] > 12) s_hr[1] = 1;
        } else {
            s_hr[2] += d;
            if (s_hr[2] < 2024) s_hr[2] = 2024;
            if (s_hr[2] > 2099) s_hr[2] = 2099;
        }
    } else if (col == 0) s_hr[3] = (s_hr[3] + d + 24) % 24;
    else s_hr[4] = (s_hr[4] + d + 60) % 60;
    hora_refrescar();
}

static void panel_hora(void)
{
    bool primera = !s_pet || !s_pet->edad_dueno;
    if (s_panel != PANEL_HORA) { /* recién abierto: la mejor hora que se conoce */
        s_hr_paso = 0;
        uint32_t base = s_ultimo_t;
        if (s_pet && s_pet->ultima_actualizacion > base) base = s_pet->ultima_actualizacion;
        hora_desde(base);
    }
    s_hora_portal = !primera;
    if (primera) abrir_noche(PANEL_HORA);
    else { abrir_tarjeta(PANEL_HORA, 466, 466); lv_obj_align(s_tarjeta, LV_ALIGN_CENTER, 0, 0); } /* (desde Configuración: el mismo disco que el resto del menú) */
    lv_obj_t *ic = icono(s_tarjeta, s_hr_paso == 0 ? ICO_CALENDARIO : ICO_RELOJ, 3);
    lv_obj_align(ic, LV_ALIGN_TOP_MID, 0, NY + 12);
    const char *tit = s_hr_paso == 1 ? "¿QUÉ HORA ES?"
                      : s_hr_error  ? "ESA HORA YA PASÓ. ¿QUÉ DÍA ES HOY?"
                      : primera     ? "¿QUÉ DÍA ES HOY?"
                      : s_pet->desfase_s ? "OTRA FECHA, SOLO ACÁ"
                      : !s_hora_dudosa ? "¿QUÉ FECHA QUIERES?"
                                    : "PERDÍ LA HORA. ¿QUÉ DÍA ES HOY?";
    bool largo = s_hr_paso == 0 && (s_hr_error || !primera);
    lv_obj_t *t = texto(s_tarjeta, tit, s_hr_error ? 0xC0405A : 0xFFEFB8, largo ? 2 : 3, largo ? 250 : 320);
    lv_obj_align(t, LV_ALIGN_TOP_MID, 0, NY + (largo ? 50 : 54));
    int n = s_hr_paso == 0 ? 3 : 2;
    static const char *const ROT[2][3] = {{"DÍA", "MES", "AÑO"}, {"HORA", "MINUTOS", ""}};
    for (int c = 0; c < n; c++) {
        int x = n == 3 ? FN_X[c] : 0;
        lv_obj_t *r = texto(s_tarjeta, ROT[s_hr_paso][c], 0xB8A8E8, 2, 0);
        lv_obj_align(r, LV_ALIGN_TOP_MID, x, n == 3 ? NY + 86 : NY + (c ? 160 : 90));
        for (int k = 0; k < 2; k++) {
            lv_obj_t *b = boton(s_tarjeta, 84, 40, hora_cb, c * 2 + k);
            lv_obj_remove_event_cb(b, hora_cb);
            lv_obj_add_event_cb(b, hora_cb, LV_EVENT_SHORT_CLICKED, (void *)(intptr_t)(c * 2 + k));
            lv_obj_add_event_cb(b, hora_cb, LV_EVENT_LONG_PRESSED_REPEAT, (void *)(intptr_t)(c * 2 + k));
            if (n == 3) lv_obj_align(b, LV_ALIGN_TOP_MID, x, NY + (k == 0 ? 106 : 196));
            else lv_obj_align(b, LV_ALIGN_TOP_MID, k == 0 ? 112 : -112, NY + (c ? 178 : 108)); /* a cada lado: derecha sube, izquierda baja */
            flecha_noche(b);
            lv_obj_t *fl = icono(b, ICO_FLECHA, 2);
            lv_image_set_pivot(fl, 12, 12);
            lv_image_set_rotation(fl, n == 3 ? (k == 0 ? 2700 : 900) : (k == 0 ? 0 : 1800));
            lv_obj_center(fl);
        }
        s_hr_val[c] = lv_image_create(s_tarjeta);
    }
    hora_refrescar();
    lv_obj_t *ok = boton(s_tarjeta, 150, 50, hora_cb, 6);
    ok_alinear(ok, 0);
    lv_obj_set_style_bg_color(ok, lv_color_hex(C_VERDE), 0);
    lv_obj_t *ot = texto(ok, s_hr_paso == 0 ? "SIGUE" : "ACEPTAR", C_BLANCO, 3, 0);
    lv_obj_center(ot);
    if (s_hr_paso) { /* volver al día */
        volver(hora_cb, 7);
    }
    if (!primera && s_pet && s_pet->desfase_s != 0 && s_hr_paso == 0) { /* está en otra fecha: aviso y vuelta a la real */
        lv_obj_t *rb = boton(s_tarjeta, 150, 34, hora_cb, 8);
        lv_obj_align(rb, LV_ALIGN_TOP_MID, 0, NY + 250);
        lv_obj_t *rt = texto(rb, "HORA REAL", C_BLANCO, 2, 0);
        lv_obj_center(rt);
    }
}

void ui_set_clock_doubtful(bool dudosa) { s_hora_dudosa = dudosa; }
void ui_close_panel(void) { cerrar_panel(); }

/* --- Bienvenida: la primera vez (y con cada mascota nueva) --- */
#define N_BIENV_EST 16
static lv_obj_t *s_bienv, *s_bienv_fondo, *s_bienv_frente, *s_bienv_pill, *s_bienv_huevo, *s_bienv_t1, *s_bienv_t2, *s_bienv_t2b, *s_bienv_t2s, *s_bienv_est[N_BIENV_EST], *s_bienv_brillo[3];
static art_img_t s_bienv_fondo_art, s_bienv_frente_art, s_bienv_huevo_art, s_bienv_t1_art, s_bienv_t2_art, s_bienv_t2s_art;
static uint32_t s_bienv_desde;
static bool s_bienv_activa;

static void bienvenida_fin(void)
{
    if (!s_bienv_activa) return;
    s_bienv_activa = false;
    ocultar(s_bienv, true);
    art_img_free(&s_bienv_fondo_art); art_img_free(&s_bienv_frente_art); art_img_free(&s_bienv_huevo_art); /* ~1.7 MB que ya no hacen falta */
    libro_inicio(); /* el Libro se presenta; al seguir: la fecha y la hora de hoy, tu nacimiento y el nombre */
}

static void bienvenida_cb(lv_event_t *e)
{
    (void)e;
    if (s_ms - s_bienv_desde > 1500) bienvenida_fin(); /* tocar salta la animación */
}

static void crear_bienvenida(lv_obj_t *scr)
{
    s_bienv = caja(scr);
    lv_obj_set_size(s_bienv, W, W);
    lv_obj_add_flag(s_bienv, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_bienv, bienvenida_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_set_style_bg_color(s_bienv, lv_color_hex(0x161A44), 0);
    lv_obj_set_style_bg_grad_color(s_bienv, lv_color_hex(0x6A4FA0), 0);
    lv_obj_set_style_bg_grad_dir(s_bienv, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_bg_opa(s_bienv, LV_OPA_COVER, 0);
    s_bienv_fondo = lv_image_create(s_bienv);
    lv_obj_set_pos(s_bienv_fondo, 0, 0);
    for (int i = 0; i < N_BIENV_EST; i++) { /* luciérnagas */
        lv_obj_t *s = caja(s_bienv);
        int tam = 3 + (int)(azar() % 2);
        lv_obj_set_size(s, tam, tam);
        lv_obj_set_style_bg_color(s, lv_color_hex(i % 3 ? 0xF0FFA8 : 0xFFE9A0), 0);
        lv_obj_set_style_bg_opa(s, LV_OPA_COVER, 0);
        s_bienv_est[i] = s;
    }
    s_bienv_huevo = lv_image_create(s_bienv);
    for (int i = 0; i < 3; i++) { s_bienv_brillo[i] = lv_image_create(s_bienv); lv_image_set_src(s_bienv_brillo[i], art_icono(ICO_ESTRELLA, 2)); }
    s_bienv_frente = lv_image_create(s_bienv);
    lv_obj_set_pos(s_bienv_frente, 0, 0);
    s_bienv_pill = caja(s_bienv);
    lv_obj_set_style_radius(s_bienv_pill, 22, 0);
    lv_obj_set_style_bg_color(s_bienv_pill, lv_color_hex(0x0E0F2E), 0);
    lv_obj_set_style_bg_opa(s_bienv_pill, LV_OPA_COVER, 0);
    s_bienv_t1 = lv_image_create(s_bienv);
    s_bienv_t2s = lv_image_create(s_bienv); /* sombra */
    s_bienv_t2b = lv_image_create(s_bienv); /* el mismo texto corrido 1 px: letra más gruesa */
    s_bienv_t2 = lv_image_create(s_bienv);
    ocultar(s_bienv, true);
}

static void bienvenida(void)
{
    if (s_bienv_activa) return;
    cerrar_panel();
    s_bienv_activa = true;
    s_bienv_desde = s_ms;
    if (art_bienvenida(&s_bienv_fondo_art, &s_bienv_frente_art, &s_bienv_huevo_art)) {
        poner_src(s_bienv_fondo, &s_bienv_fondo_art.dsc);
        poner_src(s_bienv_frente, &s_bienv_frente_art.dsc);
        poner_src(s_bienv_huevo, &s_bienv_huevo_art.dsc);
    }
    art_texto(&s_bienv_t1_art, "¡HOLA!", 0xFFFFFF, 6, 0);
    poner_src(s_bienv_t1, &s_bienv_t1_art.dsc);
    lv_obj_align(s_bienv_t1, LV_ALIGN_TOP_MID, 0, 62);
    art_texto(&s_bienv_t2_art, "ALGUIEN TE ESTABA\n       ESPERANDO", 0xFFEFB8, 3, 0);
    art_texto(&s_bienv_t2s_art, "ALGUIEN TE ESTABA\n       ESPERANDO", 0x1A0E2A, 3, 0);
    poner_src(s_bienv_t2, &s_bienv_t2_art.dsc);
    poner_src(s_bienv_t2b, &s_bienv_t2_art.dsc);
    poner_src(s_bienv_t2s, &s_bienv_t2s_art.dsc);
    {
        const int tw = (int)s_bienv_t2_art.dsc.header.w, th = (int)s_bienv_t2_art.dsc.header.h, ty = 366;
        lv_obj_set_size(s_bienv_pill, tw + 34, th + 18);
        lv_obj_set_pos(s_bienv_pill, CX - (tw + 34) / 2, ty - 9);
        lv_obj_set_pos(s_bienv_t2, CX - tw / 2, ty);
        lv_obj_set_pos(s_bienv_t2b, CX - tw / 2 + 1, ty);
        lv_obj_set_pos(s_bienv_t2s, CX - tw / 2 + 3, ty + 3);
    }
    lv_obj_set_style_opa(s_bienv_t1, LV_OPA_TRANSP, 0);
    lv_obj_set_style_opa(s_bienv_t2, LV_OPA_TRANSP, 0);
    lv_obj_set_style_opa(s_bienv_t2b, LV_OPA_TRANSP, 0);
    lv_obj_set_style_opa(s_bienv_t2s, LV_OPA_TRANSP, 0);
    lv_obj_set_style_opa(s_bienv_pill, LV_OPA_TRANSP, 0);
    ocultar(s_bienv, false);
    lv_obj_move_foreground(s_bienv);
    accion(UI_ACCION_SONAR, SND_BIENVENIDA);
}

static void animar_bienvenida(void)
{
    if (!s_bienv_activa) return;
    uint32_t t = s_ms - s_bienv_desde;
    /* el huevo baja del cielo hasta su nido, rebota y se mece; las luciérnagas flotan */
    int hw = (int)s_bienv_huevo_art.dsc.header.w, hh = (int)s_bienv_huevo_art.dsc.header.h;
    const float suelo = 350.0f; /* donde se apoya: dentro del nido */
    float y;
    if (t < 900) { float k = (float)t / 900.0f; y = -150 + (suelo + 150) * k * k; }
    else if (t < 1300) { float k = (float)(t - 900) / 400.0f; y = suelo - sinf(k * 3.1416f) * 36; }
    else y = suelo - fabsf(sinf((float)(t - 1300) / 260.0f)) * 5 * (t < 3200 ? 1.0f : 0.0f);
    lv_obj_set_pos(s_bienv_huevo, CX - hw / 2, (int)y - hh);
    lv_image_set_pivot(s_bienv_huevo, hw / 2, hh);
    lv_image_set_rotation(s_bienv_huevo, t > 1300 ? (int32_t)(sinf((float)t / 300.0f) * 60) : 0);
    for (int i = 0; i < N_BIENV_EST; i++) {
        float a = (float)i * 2.4f, f = (float)t / 1400.0f;
        float px = 233.0f + cosf(a) * (110 + 40 * sinf(a * 3)) + sinf(f + a) * 14;
        float py = 300.0f + sinf(a * 1.7f) * 70 + cosf(f * 1.3f + a) * 12;
        lv_obj_set_pos(s_bienv_est[i], (int)px, (int)py);
        lv_obj_set_style_bg_opa(s_bienv_est[i], (lv_opa_t)(60 + 195 * (0.5f + 0.5f * sinf((float)t / 300.0f + (float)i * 1.9f))), 0);
    }
    for (int i = 0; i < 3; i++) {
        float a = (float)t / 500.0f + (float)i * 2.094f;
        lv_obj_set_pos(s_bienv_brillo[i], (int)(CX + cosf(a) * 85) - 12, (int)(y - hh / 2 + sinf(a) * 40) - 12);
        ocultar(s_bienv_brillo[i], t < 1100);
    }
    int o1 = t < 1100 ? 0 : (t > 1600 ? 255 : (int)(t - 1100) * 255 / 500);
    int o2 = t < 1900 ? 0 : (t > 2400 ? 255 : (int)(t - 1900) * 255 / 500);
    lv_obj_set_style_opa(s_bienv_t1, (lv_opa_t)o1, 0);
    lv_obj_set_style_opa(s_bienv_t2, (lv_opa_t)o2, 0);
    lv_obj_set_style_opa(s_bienv_t2b, (lv_opa_t)o2, 0);
    lv_obj_set_style_opa(s_bienv_t2s, (lv_opa_t)(o2 * 190 / 255), 0);
    lv_obj_set_style_opa(s_bienv_pill, (lv_opa_t)(o2 * 170 / 255), 0);
    if (t > 5200) bienvenida_fin();
}

/* --- Elegir nombre --- */
static char s_nombre[PET_NAME_LEN];
static lv_obj_t *s_nombre_img;
static art_img_t s_nombre_art;

static int letras_nombre(void)
{
    int n = 0;
    for (const unsigned char *u = (const unsigned char *)s_nombre; *u; u++) n += (*u & 0xC0) != 0x80;
    return n;
}

static void nombre_refrescar(void)
{
    char t[PET_NAME_LEN + 2];
    snprintf(t, sizeof t, "%s_", s_nombre);
    art_texto(&s_nombre_art, t, C_TINTA, 4, 0);
    poner_src(s_nombre_img, &s_nombre_art.dsc);
    lv_obj_align(s_nombre_img, LV_ALIGN_TOP_MID, 0, 64);
}

static void tecla_cb(lv_event_t *e)
{
    int k = (int)(intptr_t)lv_event_get_user_data(e);
    size_t n = strlen(s_nombre);
    if (k == 27) {
        if (n == 0) return;
        n--;
        while (n > 0 && ((unsigned char)s_nombre[n] & 0xC0) == 0x80) n--;
        s_nombre[n] = 0;
    } else if (k == 29) { /* un nombre inventado al azar (miles distintos; no repite los últimos 20) */
        static uint32_t recientes[20];
        static int ri;
        char t[PET_NAME_LEN];
        for (int intento = 0; intento < 30; intento++) {
            uint32_t sem = azar();
            pet_make_name(sem, t, sizeof t);
            uint32_t h = 2166136261u;
            for (const char *q = t; *q; q++) h = (h ^ (uint8_t)*q) * 16777619u;
            bool visto = false;
            for (int r = 0; r < 20; r++) visto |= recientes[r] == h;
            int letras = 0;
            for (const unsigned char *u = (const unsigned char *)t; *u; u++) letras += (*u & 0xC0) != 0x80;
            if (visto || letras > 8) continue;
            recientes[ri++ % 20] = h;
            break;
        }
        snprintf(s_nombre, sizeof s_nombre, "%s", t);
    } else if (k == 28) {
        if (n == 0) return;
        cerrar_panel();
        accion(UI_ACCION_NOMBRE, 0);
        if (s_inicio_cfg == 1) { s_inicio_cfg = 2; panel_color(); }
        return;
    } else {
        if (letras_nombre() >= 8 || n + 3 >= sizeof s_nombre) return;
        if (k == 26) { s_nombre[n] = (char)0xC3; s_nombre[n + 1] = (char)(n == 0 ? 0x91 : 0xB1); s_nombre[n + 2] = 0; } /* Ñ / ñ */
        else { s_nombre[n] = (char)((n == 0 ? 'A' : 'a') + k); s_nombre[n + 1] = 0; }
    }
    nombre_refrescar();
}

static void panel_nombre(void)
{
    abrir_tarjeta(PANEL_NOMBRE, 440, 440); /* grande: teclas de 49 px (antes 40) */
    lv_obj_t *t = texto(s_tarjeta, "¿CÓMO SE LLAMARÁ?", C_TINTA, 2, 0);
    lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 34);
    s_nombre_img = lv_image_create(s_tarjeta);
    nombre_refrescar();
    lv_obj_t *dado = boton(s_tarjeta, 46, 46, tecla_cb, 29);
    lv_obj_set_pos(dado, 306, 334); /* a la derecha de ACEPTAR */
    lv_obj_t *di = icono(dado, ICO_DADO, 2);
    lv_obj_center(di);
    static const char *const LETRAS = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    for (int k = 0; k < 28; k++) {
        int fila = k / 7, col = k % 7;
        lv_obj_t *b = boton(s_tarjeta, 49, 49, tecla_cb, k);
        lv_obj_set_pos(b, 37 + col * 53, 108 + fila * 53);
        lv_obj_set_style_radius(b, 8, 0);
        char l[4] = {0};
        if (k < 26) l[0] = LETRAS[k];
        else if (k == 26) { l[0] = (char)0xC3; l[1] = (char)0x91; }
        else l[0] = '<';
        lv_obj_t *lt = texto(b, l, C_BLANCO, 4, 0);
        lv_obj_center(lt);
        if (k == 27) lv_obj_set_style_bg_color(b, lv_color_hex(0x7A4458), 0);
    }
    lv_obj_t *ok = boton(s_tarjeta, 150, 50, tecla_cb, 28);
    ok_alinear(ok, 0);
    lv_obj_set_style_bg_color(ok, lv_color_hex(C_VERDE), 0);
    lv_obj_t *ot = texto(ok, "ACEPTAR", C_BLANCO, 3, 0);
    lv_obj_center(ot);
}

const char *ui_pending_name(void) { return s_nombre; }

/* --- Ajustes: hemisferio y mascota nueva --- */
static int s_confirmar_nueva;
static int s_masc_sel = -1, s_masc_conf; /* mis mascotas: la elegida y qué se está confirmando */
static void panel_mascotas(void);
static void po_fuera(void); /* (fw 108) ui_pochi.inc: sacar a Pochi al cambiar de mascota */
static void panel_ajustes(void);
static void panel_respaldo(void);
static void panel_modo(void);
static void panel_modo_inicio(void);
static void viaje_empezar(const pet_t *m);
static int elegir_vehiculo(void); /* el viaje sale en este vehículo (se lo cuenta al motor: desde ahí te escribe) */
static const pet_device_t *s_disp_aj;  /* volumen, brillo y cuándo se apaga (de main) */
static pet_device_t s_disp_vista;
void ui_set_device(const pet_device_t *d) { s_disp_aj = d; if (d) s_fuente_grande = d->fuente != 0; }
static void panel_sonido(void);
static void panel_pantalla(void);
static void panel_mas_ajustes(void);
static void panel_version(void); /* (1.0.09) qué versión tiene y actualizar por Wi-Fi */
static int s_hemi_vista, s_modo_vista = -1, s_desp_vista;
static void panel_pais(bool desde_ajustes);
static void panel_hora_menu(void);
static void panel_alarmas(void);
static void panel_dormir(void);
static void panel_hora(void);

/* la batería dibujada: marco, carga de color, el porcentaje adentro y un rayo si está cargando.
 * Se actualiza sola (ui_set_battery → bateria_refrescar): al enchufar el cargador cambia sin salir del panel. */
#define BD_W 76 /* la pila: ancha, para que el porcentaje quepa adentro */
#define BD_H 28
static lv_obj_t *s_bd_marco, *s_bd_fill, *s_bd_txt, *s_bd_rayo;
static art_img_t s_bd_art;
static void bd_borrado(lv_event_t *e)
{
    lv_obj_t *o = lv_event_get_target(e);
    if (o == s_bd_marco) { s_bd_marco = s_bd_fill = s_bd_txt = NULL; art_img_free(&s_bd_art); s_bd_art.px = NULL; }
    if (o == s_bd_rayo) s_bd_rayo = NULL;
}
static void bateria_refrescar(void)
{
    if (!s_bd_marco || !s_bd_fill) return;
    const int pct = s_bat_pct < 0 ? 100 : s_bat_pct;
    lv_obj_set_width(s_bd_fill, 6 + pct * (BD_W - 6 - 14) / 100);
    lv_obj_set_style_bg_color(s_bd_fill, lv_color_hex(s_cargando ? 0x2FA866 : (pct < 20 ? 0xC0392B : (pct < 50 ? 0xC98A12 : 0x2E9E50))), 0);
    if (s_bd_txt) {
        static int antes = -2;
        if (antes != s_bat_pct || !s_bd_art.px) {
            antes = s_bat_pct;
            char b[16];
            if (s_bat_pct < 0) snprintf(b, sizeof b, "--");
            else snprintf(b, sizeof b, "%d%%", s_bat_pct);
            art_img_free(&s_bd_art);
            art_texto(&s_bd_art, b, C_BLANCO, 2, 0);
            lv_image_set_src(s_bd_txt, &s_bd_art.dsc);
            lv_obj_align(s_bd_txt, LV_ALIGN_CENTER, 0, 0);
        }
    }
    if (s_bd_rayo) { if (s_cargando) lv_obj_remove_flag(s_bd_rayo, LV_OBJ_FLAG_HIDDEN); else lv_obj_add_flag(s_bd_rayo, LV_OBJ_FLAG_HIDDEN); }
}
static void bateria_dibujo(lv_obj_t *padre, int x, int y, bool numero)
{
    /* una pila: cuerpo oscuro con borde claro, la carga de color adentro, el porcentaje al centro y el rayo si carga */
    lv_obj_t *m = caja(padre);
    lv_obj_set_size(m, BD_W, BD_H);
    lv_obj_align(m, LV_ALIGN_TOP_MID, x, y);
    lv_obj_set_style_border_width(m, 3, 0);
    lv_obj_set_style_border_color(m, lv_color_hex(0xFFF3C8), 0);
    lv_obj_set_style_radius(m, 8, 0);
    lv_obj_set_style_bg_color(m, lv_color_hex(0x1E2238), 0);
    lv_obj_set_style_bg_opa(m, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(m, 0, 0);
    lv_obj_t *p = caja(padre); /* el polo de la pila */
    lv_obj_set_size(p, 6, 11);
    lv_obj_align(p, LV_ALIGN_TOP_MID, x + BD_W / 2 + 7, y + (BD_H - 11) / 2);
    lv_obj_set_style_bg_color(p, lv_color_hex(0xFFF3C8), 0);
    lv_obj_set_style_bg_opa(p, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(p, 4, 0);
    lv_obj_t *f = caja(m);
    lv_obj_set_height(f, BD_H - 6 - 6);
    lv_obj_set_pos(f, 3, 3);
    lv_obj_set_style_radius(f, 5, 0);
    lv_obj_set_style_bg_opa(f, LV_OPA_COVER, 0);
    lv_obj_t *r = icono(m, ICO_RAYO, 1);
    lv_obj_align(r, LV_ALIGN_LEFT_MID, 3, 0);
    art_img_free(&s_bd_art); s_bd_art.px = NULL;
    s_bd_marco = m; s_bd_fill = f; s_bd_rayo = r; s_bd_txt = numero ? lv_image_create(m) : NULL;
    lv_obj_add_event_cb(m, bd_borrado, LV_EVENT_DELETE, NULL);
    lv_obj_add_event_cb(r, bd_borrado, LV_EVENT_DELETE, NULL);
    bateria_refrescar();
    if (s_bd_txt) lv_obj_move_foreground(s_bd_txt);
    lv_obj_move_foreground(r);
}

static void ajustes_cb(lv_event_t *e)
{
    int q = (int)(intptr_t)lv_event_get_user_data(e);
    switch (q) {
    case 0: panel_sonido(); break;
    case 1: panel_pantalla(); break;
    case 2: panel_hora_menu(); break;
    case 3: s_confirmar_nueva = 0; s_masc_sel = -1; s_masc_conf = 0; panel_mascotas(); break;
    case 4: panel_modo(); break; /* el selector de modos, en el menú de ajustes */
    case 5: panel_mas_ajustes(); break;
    case 6: panel_version(); break;
    case 9: panel_ajustes(); break;
    default: break;
    }
}

/* Ajustes: una cuadrícula de íconos grandes que abren pantallas cortas */
static void panel_mas(void);
static int s_mas_tab;
static void panel_ajustes(void) { s_mas_tab = 2; panel_mas(); }

/* --- Más ajustes: estaciones (hemisferio) y modo de juego --- */
static void panel_modo(void);
static void panel_ritmo(void);
static void panel_evolucion(bool desde_ajustes);
static void mas_aj_cb(lv_event_t *e)
{
    int q = (int)(intptr_t)lv_event_get_user_data(e);
    if (q == 0 && s_pet) panel_pais(true); /* el país (decide las estaciones) */
    else if (q == 1) panel_respaldo(); /* el respaldo, en Más */
    else if (q == 3 && s_pet) panel_evolucion(true); /* crecer: ya le tocaba y dijiste «todavía no» */
    else if (q == 2 && s_pet) { s_desp_vista = !s_desp_vista; accion(UI_ACCION_DESPEDIDA_NAT, s_desp_vista); panel_mas_ajustes(); }
    else panel_ajustes();
}
static void panel_mas_ajustes(void)
{
    if (s_panel != PANEL_MAS_AJ && s_pet) { s_hemi_vista = s_pet->hemisferio_sur; s_modo_vista = s_pet->modo_manual ? s_pet->modo_manual - 1 : -1; s_desp_vista = s_pet->despedida_natural; }
    abrir_tarjeta(PANEL_MAS_AJ, 330, s_suave ? 270 : 330);
    const bool puede_crecer = s_pet && pet_evo_pending(s_pet) > 0;
    lv_obj_t *t = icono(s_tarjeta, ICO_AJUSTES, 3);
    lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 10);
    for (int k = 0; k < 2; k++) {
        lv_obj_t *b = boton(s_tarjeta, 270, 54, mas_aj_cb, k);
        lv_obj_align(b, LV_ALIGN_TOP_MID, 0, 54 + k * 62);
        lv_obj_t *ic = icono(b, k ? ICO_RESPALDO : ICO_SOL, 3);
        lv_obj_align(ic, s_nino ? LV_ALIGN_CENTER : LV_ALIGN_LEFT_MID, s_nino ? 0 : 12, 0);
        if (!s_nino) {
            char pt[48];
            snprintf(pt, sizeof pt, "PAÍS: %s", s_pet ? pet_country_name(s_pet->pais) : "");
            mayusculas(pt);
            lv_obj_t *tt = texto(b, k ? "RESPALDO" : pt, C_BLANCO, 2, 200);
            lv_obj_align(tt, LV_ALIGN_LEFT_MID, 58, 0);
        }
    }
    if (!s_suave) { /* la despedida natural: solo el adulto la ve (apagada al empezar) */
        lv_obj_t *b = boton(s_tarjeta, 270, 54, mas_aj_cb, 2);
        lv_obj_align(b, LV_ALIGN_TOP_MID, 0, 178);
        lv_obj_set_style_bg_color(b, lv_color_hex(s_desp_vista ? C_VIOLETA : C_BOTON), 0);
        lv_obj_t *ic = icono(b, ICO_MALETA, 3);
        lv_obj_align(ic, LV_ALIGN_LEFT_MID, 12, 0);
        lv_obj_t *tt = texto(b, s_desp_vista ? "UN DÍA PODRÁ PEDIR IRSE DE VIAJE" : "NUNCA PIDE IRSE DE VIAJE", C_BLANCO, 2, 200);
        lv_obj_align(tt, LV_ALIGN_LEFT_MID, 58, 0);
    }
    if (puede_crecer) { /* ya le toca crecer: aquí se decide cuando dijiste «todavía no» */
        lv_obj_t *b = boton(s_tarjeta, 270, 54, mas_aj_cb, 3);
        lv_obj_align(b, LV_ALIGN_TOP_MID, 0, s_suave ? 178 : 240);
        lv_obj_set_style_bg_color(b, lv_color_hex(C_VIOLETA), 0);
        lv_obj_t *ic = icono(b, ICO_ESTRELLA, 3);
        lv_obj_align(ic, s_nino ? LV_ALIGN_CENTER : LV_ALIGN_LEFT_MID, s_nino ? 0 : 12, 0);
        if (!s_nino) { lv_obj_t *tt = texto(b, "LISTA PARA CRECER", C_BLANCO, 2, 200); lv_obj_align(tt, LV_ALIGN_LEFT_MID, 58, 0); }
    }
    volver(mas_aj_cb, 9);
}

/* --- (1.0.09) la versión: número, nombre y fecha; desde aquí se pide actualizar por Wi-Fi --- */
static void version_cb(lv_event_t *e)
{
    const int q = (int)(intptr_t)lv_event_get_user_data(e);
    if (q == 1) { cerrar_panel(); ui_splash("ACTUALIZAR", "REINICIANDO EN MODO WI-FI...", false, 8000); accion(UI_ACCION_ACTUALIZAR, 0); }
    else panel_ajustes();
}
static char s_red_ota[24] = "ORBEPET";
void ui_set_red_ota(const char *red) { if (red) snprintf(s_red_ota, sizeof s_red_ota, "%s", red); }
static void panel_version(void)
{
    abrir_tarjeta(PANEL_VERSION, 360, 360);
    lv_obj_t *t = texto(s_tarjeta, "VERSIÓN " FW_VERSION, C_TINTA, 3, 0);
    lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 22);
    lv_obj_t *n = texto_c(s_tarjeta, FW_NOMBRE, C_VIOLETA, 2, 300);
    lv_obj_align(n, LV_ALIGN_TOP_MID, 0, 64);
    char f[48] = "";
#ifdef BUILD_UNIX_TIME
    { static const char *const MES[12] = {"ENE", "FEB", "MAR", "ABR", "MAY", "JUN", "JUL", "AGO", "SEP", "OCT", "NOV", "DIC"};
      const time_t bt = (time_t)BUILD_UNIX_TIME; struct tm tm; gmtime_r(&bt, &tm);
      snprintf(f, sizeof f, "COMPILADA EL %d-%s-%d", tm.tm_mday, MES[tm.tm_mon % 12], tm.tm_year + 1900); }
#endif
    if (f[0]) { lv_obj_t *fe = texto(s_tarjeta, f, C_TINTA, 2, 0); lv_obj_align(fe, LV_ALIGN_TOP_MID, 0, 100); }
    char ex_t[200];
    snprintf(ex_t, sizeof ex_t, "SE REINICIA EN MODO WI-FI (LA MASCOTA QUEDA GUARDADA). EN EL CELULAR: RED %s, CLAVE ORBEPET1 Y ABRE 192.168.4.1", s_red_ota);
    lv_obj_t *ex = texto_c(s_tarjeta, ex_t, C_TINTA, 2, 300);
    lv_obj_align(ex, LV_ALIGN_TOP_MID, 0, 132);
    lv_obj_t *b = boton(s_tarjeta, 230, 52, version_cb, 1);
    lv_obj_align(b, LV_ALIGN_TOP_MID, 0, 262);
    lv_obj_set_style_bg_color(b, lv_color_hex(C_VERDE), 0);
    lv_obj_t *bt = texto(b, "ACTUALIZAR", C_BLANCO, 3, 230);
    lv_obj_center(bt);
    volver(version_cb, 0);
}

/* --- el país: se escribe (con sugerencias). Si no está en la lista (o es inventado), pregunta en qué estación
 * estamos. Al empezar lo pregunta solo en modo adulto; en niño e infante queda en Chile y se cambia en Más ajustes --- */
static bool s_pais_desde_ajustes;
static char s_pais_txt[24];
static int s_pais_sug[2], s_pais_nsug;
static lv_obj_t *s_pais_img, *s_pais_b[2], *s_pais_bt[2];
static art_img_t s_pais_art, s_pais_bt_art[2];
static void panel_pais(bool desde_ajustes);
static void panel_estacion_pais(void);
static void pais_listo(int arg)
{
    s_pais_enviado = true;
    accion(UI_ACCION_PAIS, arg);
    if (s_pais_desde_ajustes) panel_mas_ajustes();
    else cerrar_panel();
}
static void pais_refrescar(void)
{
    char t[32];
    snprintf(t, sizeof t, "%s_", s_pais_txt);
    art_texto(&s_pais_art, t, C_TINTA, 3, 0);
    poner_src(s_pais_img, &s_pais_art.dsc);
    lv_obj_align(s_pais_img, LV_ALIGN_TOP_MID, 0, 72);
    s_pais_nsug = pet_country_find(s_pais_txt, s_pais_sug, 2);
    for (int k = 0; k < 2; k++) {
        bool ver = k < s_pais_nsug || (k == 0 && strlen(s_pais_txt) >= 3); /* sin coincidencias: "no está" */
        ocultar(s_pais_b[k], !ver);
        if (!ver) continue;
        char nm[48];
        snprintf(nm, sizeof nm, "%s", k < s_pais_nsug ? pet_country_name(s_pais_sug[k]) : "NO ESTÁ: OTRO PAÍS");
        mayusculas(nm);
        const bool sola = s_pais_nsug <= 1; /* una sola opción: el botón ocupa todo el ancho */
        lv_obj_set_size(s_pais_b[k], sola ? 304 : 148, 40);
        art_texto(&s_pais_bt_art[k], nm, C_BLANCO, 2, sola ? 288 : 132);
        poner_src(s_pais_bt[k], &s_pais_bt_art[k].dsc);
        lv_obj_center(s_pais_bt[k]);
        lv_obj_set_style_bg_color(s_pais_b[k], lv_color_hex(k < s_pais_nsug ? C_VIOLETA : 0x7A4458), 0);
    }
}
static void pais_tecla_cb(lv_event_t *e)
{
    int k = (int)(intptr_t)lv_event_get_user_data(e);
    size_t n = strlen(s_pais_txt);
    if (k == 99) { panel_mas_ajustes(); return; }
    if (k == 40 || k == 41) { /* una sugerencia (o "otro país") */
        int i = k - 40;
        if (i < s_pais_nsug) pais_listo(s_pais_sug[i]);
        else panel_estacion_pais();
        return;
    }
    if (k == 27) { /* borrar */
        if (!n) return;
        n--;
        while (n > 0 && ((unsigned char)s_pais_txt[n] & 0xC0) == 0x80) n--;
        s_pais_txt[n] = 0;
    } else if (k == 28) { if (n && n + 2 < sizeof s_pais_txt && s_pais_txt[n - 1] != ' ') { s_pais_txt[n] = ' '; s_pais_txt[n + 1] = 0; } } /* espacio */
    else {
        if (n + 3 >= sizeof s_pais_txt) return;
        if (k == 26) { s_pais_txt[n] = (char)0xC3; s_pais_txt[n + 1] = (char)0x91; s_pais_txt[n + 2] = 0; }
        else { s_pais_txt[n] = (char)('A' + k); s_pais_txt[n + 1] = 0; }
    }
    pais_refrescar();
}
static void panel_pais(bool desde_ajustes)
{
    s_pais_desde_ajustes = desde_ajustes;
    s_pais_txt[0] = 0;
    abrir_tarjeta(PANEL_PAIS, 440, 440); /* grande: teclas de 45 px (antes 37) */
    lv_obj_t *t = texto(s_tarjeta, "¿EN QUÉ PAÍS VIVES?", C_TINTA, 2, 280);
    lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 38);
    s_pais_img = lv_image_create(s_tarjeta);
    for (int k = 0; k < 2; k++) { /* las sugerencias, a medida que escribes */
        s_pais_b[k] = boton(s_tarjeta, 148, 40, pais_tecla_cb, 40 + k);
        lv_obj_set_style_radius(s_pais_b[k], 14, 0);
        lv_obj_set_pos(s_pais_b[k], 68 + k * 156, 108);
        s_pais_bt[k] = lv_image_create(s_pais_b[k]);
        ocultar(s_pais_b[k], true);
    }
    static const char *const LETRAS = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    for (int k = 0; k < 29; k++) { /* A..Z, Ñ, borrar y espacio */
        int fila = k / 8, col = k % 8;
        int x = 22 + col * 49 + (fila == 3 ? 98 : 0);
        lv_obj_t *b = boton(s_tarjeta, 45, 45, pais_tecla_cb, k);
        lv_obj_set_pos(b, x, 158 + fila * 49);
        lv_obj_set_style_radius(b, 8, 0);
        char l[8] = {0};
        if (k < 26) l[0] = LETRAS[k];
        else if (k == 26) { l[0] = (char)0xC3; l[1] = (char)0x91; }
        else if (k == 27) l[0] = '<';
        else l[0] = '_';
        lv_obj_t *lt = texto(b, l, C_BLANCO, 3, 0);
        lv_obj_center(lt);
        if (k == 27) lv_obj_set_style_bg_color(b, lv_color_hex(0x7A4458), 0);
    }
    pais_refrescar();
    if (desde_ajustes) volver(pais_tecla_cb, 99);
}
/* el país no está (o lo inventaron): ¿en qué estación estamos? */
static void estacion_pais_cb(lv_event_t *e)
{
    int q = (int)(intptr_t)lv_event_get_user_data(e);
    if (q == 99) { panel_pais(s_pais_desde_ajustes); return; }
    pais_listo(0x100 | q);
}
static void panel_estacion_pais(void)
{
    abrir_tarjeta(PANEL_PAIS, 340, 340);
    lv_obj_t *t = texto(s_tarjeta, "¿EN QUÉ ESTACIÓN ESTAMOS?", C_TINTA, 2, 280);
    lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 24);
    static const art_icono_t IC[4] = {ICO_FLOR, ICO_SOL, ICO_HOJA_SECA, ICO_COPO};
    static const char *const NOM[4] = {"PRIMAVERA", "VERANO", "OTOÑO", "INVIERNO"};
    for (int k = 0; k < 4; k++) {
        lv_obj_t *b = boton(s_tarjeta, 128, 108, estacion_pais_cb, k);
        lv_obj_align(b, LV_ALIGN_TOP_MID, (k % 2 ? 1 : -1) * 68, 58 + (k / 2) * 116);
        lv_obj_t *ic = icono(b, IC[k], 4);
        lv_obj_align(ic, LV_ALIGN_TOP_MID, 0, 10);
        lv_obj_t *nt = texto(b, NOM[k], C_BLANCO, 2, 0);
        lv_obj_align(nt, LV_ALIGN_BOTTOM_MID, 0, -10);
    }
    volver(estacion_pais_cb, 99);
}

/* el modo de juego, a mano: con una explicación corta. Ir a uno con menos protección pide mantener apretado */
/* fin de la configuración del inicio: ahora sí, ella sale a saludar */
static void inicio_terminar(void)
{
    s_inicio_cfg = 0;
    if (s_pet) {
        char t[40];
        snprintf(t, sizeof t, "¡HOLA! SOY %s", s_pet->nombre[0] ? s_pet->nombre : "YO");
        globo(ICO_CORAZON, t, 5000);
        for (int i = 0; i < 6; i++) flotar(ICO_ESTRELLA, NULL, 0, 233 - 60 + i * 24, 250, (uint32_t)i * 120);
    }
    mascota_modo(M_SALTA, 1500);
    libro_pedir(1);
}

static void modo_cb(lv_event_t *e)
{
    int q = (int)(intptr_t)lv_event_get_user_data(e); /* 0 según edad, 1 adulto, 2 niño, 3 infante; 9 volver */
    if (q == 9) { panel_ajustes(); return; }
    s_modo_vista = q - 1;
    accion(UI_ACCION_MODO, q - 1);
    cerrar_panel();
    if (s_inicio_cfg == 3) { inicio_terminar(); return; }
    globo(ICO_PERSONA, NULL, 1500);
}

/* Al empezar, el modo es ADULTO. Solo si la fecha de nacimiento es de un niño o un infante se pregunta si quien lo configura es un adulto
 * (que puede elegir o mantener el modo adulto); si no lo es, queda el modo que le toca por su edad. */
static void adulto_cb(lv_event_t *e)
{
    const int q = (int)(intptr_t)lv_event_get_user_data(e); /* 1 soy adulto, 0 no */
    if (q == 1) { s_modo_vista = 0; panel_modo(); return; }
    accion(UI_ACCION_MODO, -1); /* según la edad */
    cerrar_panel();
    inicio_terminar();
}
static void panel_adulto_pregunta(void)
{
    abrir_tarjeta(PANEL_MODO, 372, 400);
    lv_obj_t *ic = icono(s_tarjeta, ICO_PERSONA, 3);
    lv_obj_align(ic, LV_ALIGN_TOP_MID, 0, 14);
    const bool infante = s_pet && s_pet->edad_dueno < PET_EDAD_INFANTE;
    lv_obj_t *t1 = texto(s_tarjeta, infante ? "POR ESA FECHA SERÍA MODO INFANTE" : "POR ESA FECHA SERÍA MODO NIÑO", C_TINTA, 2, 320);
    lv_obj_align(t1, LV_ALIGN_TOP_MID, 0, 70);
    lv_obj_t *t2 = texto(s_tarjeta, "¿LO ESTÁ CONFIGURANDO UN ADULTO?", 0x8A4F00, 2, 320);
    lv_obj_align(t2, LV_ALIGN_TOP_MID, 0, 104);
    lv_obj_t *a = boton(s_tarjeta, 300, 70, adulto_cb, 1);
    lv_obj_set_style_radius(a, 22, 0);
    lv_obj_align(a, LV_ALIGN_TOP_MID, 0, 160);
    lv_obj_set_style_bg_color(a, lv_color_hex(0x3E6A50), 0);
    mantener(a, adulto_cb, 1); /* mantener apretado: así un niño no lo toca sin querer */
    lv_obj_t *ai = icono(a, ICO_SI, 3);
    lv_obj_align(ai, LV_ALIGN_LEFT_MID, 12, 0);
    lv_obj_t *at = texto(a, "SOY ADULTO (MANTÉN)", C_BLANCO, 2, 0);
    lv_obj_align(at, LV_ALIGN_LEFT_MID, 58, 0);
    lv_obj_t *n = boton(s_tarjeta, 300, 70, adulto_cb, 0);
    lv_obj_set_style_radius(n, 22, 0);
    lv_obj_align(n, LV_ALIGN_TOP_MID, 0, 242);
    lv_obj_t *ni = icono(n, ICO_NO, 3);
    lv_obj_align(ni, LV_ALIGN_LEFT_MID, 12, 0);
    lv_obj_t *nt = texto(n, "NO, SOY YO", C_BLANCO, 2, 0);
    lv_obj_align(nt, LV_ALIGN_LEFT_MID, 58, 0);
}
/* el último paso del inicio: lo normal es seguir en modo adulto sin preguntar nada */
static void panel_modo_inicio(void)
{
    if (s_pet && s_pet->edad_dueno && s_pet->edad_dueno < PET_EDAD_NINO) { panel_adulto_pregunta(); return; }
    cerrar_panel();
    inicio_terminar();
}

/* --- crecer: te avisa, ves cómo se vería y decides (después de crecer no hay vuelta atrás) --- */
static bool s_cie_activo, s_via_activo; /* (se definen más abajo) */
static art_img_t s_evo_art[2];
static bool s_evo_abrir;
static void evo_cb(lv_event_t *e)
{
    int q = (int)(intptr_t)lv_event_get_user_data(e);
    if (q == 1 || q == 0) accion(UI_ACCION_EVOLUCION, q);
    cerrar_panel();
}
static void panel_evolucion(bool desde_ajustes)
{
    (void)desde_ajustes;
    if (!s_pet || !pet_evo_pending(s_pet)) return;
    abrir_tarjeta(PANEL_EVOLUCION, 372, 400);
    int et, forma, mez;
    pet_evo_preview(s_pet, s_ultimo_t, &et, &forma, &mez);
    const int look = (pet_look(s_pet) & ~(15 << 3)) | (mez << 3);
    art_pose_t ps[2] = {
        {pet_look(s_pet), s_pet->etapa, s_pet->etapa == STAGE_ADULTA ? s_pet->forma : 0, 5, EXP_FELIZ, false, 0, false, false, 0, 0, 0, {0, 0, 0}, 0, s_pet->guardiana},
        {look, et, et == STAGE_ADULTA ? forma : 0, 5, EXP_FELIZ, true, 1, false, false, 1, 1, 0, {0, 0, 0}, 0, s_pet->guardiana}};
    for (int k = 0; k < 2; k++) {
        for (int r = 0; r < RANURA_COUNT; r++) ps[k].ropa[r] = pet_worn_raw(s_pet, r);
        art_criatura_pose(&s_evo_art[k], &ps[k]);
    }
    lv_obj_t *t = icono(s_tarjeta, ICO_ESTRELLA, 3);
    lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 14);
    if (!s_nino) {
        lv_obj_t *tt = texto(s_tarjeta, s_pet->genero ? "¡ESTÁ LISTO PARA CRECER!" : "¡ESTÁ LISTA PARA CRECER!", 0x8A4F00, 2, 320);
        lv_obj_align(tt, LV_ALIGN_TOP_MID, 0, 50);
    }
    lv_obj_t *a = lv_image_create(s_tarjeta);
    lv_image_set_src(a, &s_evo_art[0].dsc);
    lv_obj_align(a, LV_ALIGN_TOP_MID, -104, 80);
    lv_obj_t *fl = icono(s_tarjeta, ICO_FLECHA, 3);
    lv_obj_align(fl, LV_ALIGN_TOP_MID, 0, 142);
    lv_obj_t *b = lv_image_create(s_tarjeta);
    lv_image_set_src(b, &s_evo_art[1].dsc);
    lv_obj_align(b, LV_ALIGN_TOP_MID, 104, 80);
    if (!s_nino) {
        lv_obj_t *s1 = texto(s_tarjeta, s_suave ? "ASÍ SE VERÍA" : "ASÍ SE VERÍA AHORA", C_TINTA, 2, 300);
        lv_obj_align(s1, LV_ALIGN_TOP_MID, 0, 228);
    }
    lv_obj_t *g = boton(s_tarjeta, 150, 62, evo_cb, 1);
    lv_obj_align(g, LV_ALIGN_BOTTOM_MID, -82, -66);
    lv_obj_set_style_bg_color(g, lv_color_hex(0x3E6A50), 0);
    lv_obj_t *gi = icono(g, ICO_SI, 3);
    lv_obj_align(gi, s_nino ? LV_ALIGN_CENTER : LV_ALIGN_LEFT_MID, s_nino ? 0 : 12, 0);
    if (!s_nino) { lv_obj_t *gt = texto(g, "CRECER", C_BLANCO, 2, 0); lv_obj_align(gt, LV_ALIGN_LEFT_MID, 52, 0); }
    lv_obj_t *n = boton(s_tarjeta, 150, 62, evo_cb, 0);
    lv_obj_align(n, LV_ALIGN_BOTTOM_MID, 82, -66);
    lv_obj_t *ni = icono(n, ICO_NO, 3);
    lv_obj_align(ni, s_nino ? LV_ALIGN_CENTER : LV_ALIGN_LEFT_MID, s_nino ? 0 : 12, 0);
    if (!s_nino) { lv_obj_t *nt = texto(n, "AÚN NO", C_BLANCO, 2, 0); lv_obj_align(nt, LV_ALIGN_LEFT_MID, 52, 0); }
}
/* cuando le toca crecer, aparece la pregunta apenas no haya otro panel abierto */
static void evo_revisar(void)
{
    if (!s_pet || s_evo_abrir == false) {
        if (s_pet && pet_evo_pending(s_pet) && !s_pet->evo_decidido) s_evo_abrir = true;
        else return;
    }
    if (s_panel != PANEL_NINGUNO || s_pet->dormida || s_interior || s_cie_activo || s_via_activo || s_cru_activo || s_escena < 0 || s_juego != JG_NADA) return;
    s_evo_abrir = false;
    if (pet_evo_pending(s_pet) && !s_pet->evo_decidido) { accion(UI_ACCION_EVOLUCION, 0); panel_evolucion(false); } /* aviso mostrado: no se repite */
}

static void panel_modo(void)
{
    abrir_tarjeta(PANEL_MODO, 372, 400);
    lv_obj_t *t = icono(s_tarjeta, ICO_PERSONA, 2);
    lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 12);
    static const char *const NOM[4] = {"SEGÚN TU EDAD", "ADULTO", "NIÑO", "INFANTE"};
    static const char *const EXP[4] = {"LO ELIGE TU EDAD", "TEXTOS E HISTORIAS",
                                       "DIBUJOS, POCO TEXTO", "SOLO TOCAR Y JUGAR"};
    static const art_icono_t IC[4] = {ICO_CALENDARIO, ICO_LIBRO, ICO_ESTRELLA, ICO_HUEVO};
    for (int k = 0; k < 4; k++) { /* las cuatro caben enteras dentro del círculo */
        lv_obj_t *b = boton(s_tarjeta, 280, 64, modo_cb, k);
        lv_obj_set_style_radius(b, 22, 0);
        lv_obj_align(b, LV_ALIGN_TOP_MID, 0, 46 + k * 68);
        if (k == s_modo_vista + 1) lv_obj_set_style_bg_color(b, lv_color_hex(C_VIOLETA), 0);
        /* menos protección (adulto, o según la edad si eso da adulto) desde un modo protegido: mantener */
        bool afloja = s_suave && (k == 1 || (k == 0 && s_pet && s_pet->edad_dueno >= PET_EDAD_NINO));
        if (afloja) mantener(b, modo_cb, k);
        lv_obj_t *ic = icono(b, IC[k], 3);
        lv_obj_align(ic, LV_ALIGN_LEFT_MID, 10, 0);
        lv_obj_t *n = texto(b, afloja ? (k == 1 ? "ADULTO (MANTÉN)" : "SEGÚN EDAD (MANTÉN)") : NOM[k], 0xFFE27A, 2, 0);
        lv_obj_align(n, LV_ALIGN_TOP_LEFT, 56, 10);
        lv_obj_t *x = texto(b, EXP[k], C_BLANCO, 2, 0);
        lv_obj_align(x, LV_ALIGN_TOP_LEFT, 56, 33);
    }
    if (s_inicio_cfg != 3) volver(modo_cb, 9); /* (en el inicio no hay vuelta: se elige uno) */
}

/* --- el ritmo de su tiempo: tu reloj no cambia, pero su día interno corre más rápido --- */
static void ritmo_cb(lv_event_t *e)
{
    int q = (int)(intptr_t)lv_event_get_user_data(e); /* 0 tranquilo, 1 equilibrado, 2 rápido; 9 volver */
    if (q == 9) { if (s_pecera) cerrar_panel(); else panel_pantalla(); return; }
    accion(UI_ACCION_RITMO, q);
    cerrar_panel();
    globo(ICO_RELOJ, NULL, 1500);
}
static void panel_ritmo(void)
{
    abrir_tarjeta(PANEL_RITMO, 372, 420);
    lv_obj_t *t = icono(s_tarjeta, ICO_RELOJ, 3);
    lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 10);
    static const char *const NOM[3] = {"TRANQUILO", "EQUILIBRADO", "RÁPIDO"};
    static const char *const EXP[3] = {"SU DÍA DURA 24 HORAS", "SU DÍA DURA 16 HORAS", "SU DÍA DURA 8 HORAS"};
    static const char *const EXP2[3] = {"1 DÍA REAL = 1 SUYO", "1 DÍA REAL = 1,5 SUYOS", "1 DÍA REAL = 3 SUYOS"}; /* cuánto tarda en realidad */
    static const art_icono_t IC[3] = {ICO_LUNA, ICO_SOL, ICO_RAYO};
    const int actual = s_pet ? pet_rhythm(s_pet) : 0;
    for (int k = 0; k < 3; k++) {
        lv_obj_t *b = boton(s_tarjeta, 290, 76, ritmo_cb, k); /* (v97) más ancho: «24 HORAS» y «SUYOS» se cortaban */
        lv_obj_set_style_radius(b, 22, 0);
        lv_obj_align(b, LV_ALIGN_TOP_MID, 0, 56 + k * 80);
        if (k == actual) lv_obj_set_style_bg_color(b, lv_color_hex(C_VIOLETA), 0);
        lv_obj_t *ic = icono(b, IC[k], 3);
        lv_obj_align(ic, LV_ALIGN_LEFT_MID, 6, 0);
        lv_obj_t *n = texto(b, NOM[k], 0xFFE27A, 2, 0);
        lv_obj_align(n, LV_ALIGN_TOP_LEFT, 46, 6);
        lv_obj_t *x = texto(b, EXP[k], C_BLANCO, 2, 0);
        lv_obj_align(x, LV_ALIGN_TOP_LEFT, 46, 28);
        lv_obj_t *x2 = texto(b, EXP2[k], 0xFFEFB8, 2, 0);
        lv_obj_align(x2, LV_ALIGN_TOP_LEFT, 46, 49);
    }
    if (!s_nino) {
        lv_obj_t *x = texto(s_tarjeta, "SUS CUMPLEAÑOS SE CUENTAN EN TIEMPO REAL: UN AÑO TARDA UN AÑO. TU ORBE NO CAMBIA", C_GRIS, 2, 316);
        lv_obj_align(x, LV_ALIGN_TOP_MID, 0, 300);
    }
    volver(ritmo_cb, 9);
}
/* al empezar, una sola vez, cuando ya no hay otra pantalla abierta */
static void ritmo_revisar(void)
{
    if (!s_pet || s_pet->ritmo_visto || !s_pet->edad_dueno || s_panel != PANEL_NINGUNO || s_pet->dormida || s_pet->etapa == STAGE_HUEVO ||
        s_interior || s_cie_activo || s_via_activo || s_cru_activo || s_escena < 0 || s_juego != JG_NADA || s_bienv_activa ||
        s_inicio_cfg || s_nace_desde || !s_pet->nombre[0])   /* (107) primero el nombre, el color y el modo: esto se colaba antes */
        return;
    panel_ritmo();
}

/* una barra gruesa para el dedo, con un ícono a cada lado */
static const char *s_barra_txt; /* lo que hace la barra, escrito adentro (se usa una vez) */
static lv_obj_t *barra(lv_obj_t *padre, int y, art_icono_t izq, art_icono_t der, int min, int max, int valor, lv_event_cb_t cb, int id)
{
    const char *etiqueta = s_nino ? NULL : s_barra_txt;
    s_barra_txt = NULL;
    lv_obj_t *s = lv_slider_create(padre);
    lv_obj_remove_flag(s, LV_OBJ_FLAG_GESTURE_BUBBLE); /* (v99) arrastrar la barra no pasa de página */
    lv_obj_set_size(s, 190, etiqueta ? 32 : 26);
    lv_obj_align(s, LV_ALIGN_TOP_MID, 0, y);
    lv_slider_set_range(s, min, max);
    lv_slider_set_value(s, valor, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(s, lv_color_hex(etiqueta ? 0x3A3558 : 0xD8D2C4), LV_PART_MAIN); /* (con letra adentro: fondo oscuro y letra blanca, que se lee en toda la barra) */
    lv_obj_set_style_bg_opa(s, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(s, 16, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s, lv_color_hex(etiqueta ? 0x6A55C8 : C_VIOLETA), LV_PART_INDICATOR);
    lv_obj_set_style_radius(s, 16, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(s, lv_color_hex(C_BLANCO), LV_PART_KNOB);
    lv_obj_set_style_border_width(s, 4, LV_PART_KNOB);
    lv_obj_set_style_border_color(s, lv_color_hex(C_TINTA), LV_PART_KNOB);
    /* sin perilla: se desliza el dedo y la barra se llena o se vacía (el principio es donde empieza el color) */
    lv_obj_set_style_bg_opa(s, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_set_style_border_width(s, 0, LV_PART_KNOB);
    lv_obj_set_style_shadow_width(s, 0, LV_PART_KNOB);
    lv_obj_set_style_pad_all(s, 0, LV_PART_KNOB);
    lv_obj_set_ext_click_area(s, 18);
    lv_obj_add_event_cb(s, cb, LV_EVENT_VALUE_CHANGED, (void *)(intptr_t)id);
    lv_obj_add_event_cb(s, cb, LV_EVENT_RELEASED, (void *)(intptr_t)(id + 100));
    if (etiqueta) {
        lv_obj_t *t = texto(s, etiqueta, C_BLANCO, 2, 170);
        lv_obj_remove_flag(t, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_center(t);
    }
    lv_obj_t *a = icono(padre, izq, 2);
    lv_obj_align(a, LV_ALIGN_TOP_MID, -134, y);
    lv_obj_t *b = icono(padre, der, 3);
    lv_obj_align(b, LV_ALIGN_TOP_MID, 136, y - 4);
    return s;
}

static void sonido_cb(lv_event_t *e)
{
    int id = (int)(intptr_t)lv_event_get_user_data(e);
    int v = (int)lv_slider_get_value(lv_event_get_target(e));
    if (id == 0 || id == 100) { s_disp_vista.volumen = (uint8_t)v; accion(UI_ACCION_VOLUMEN, v | (id == 100 ? UI_SOLTADO : 0)); }
    if (id == 1 || id == 101) { s_disp_vista.ambiente = (uint8_t)v; accion(UI_ACCION_AMBIENTE, v | (id == 101 ? UI_SOLTADO : 0)); }
    if (id == 2 || id == 102) { s_disp_vista.efectos = (uint8_t)v; accion(UI_ACCION_EFECTOS, v | (id == 102 ? UI_SOLTADO : 0)); }
    if (id == 4 || id == 104) { s_disp_vista.alarmas = (uint8_t)v; accion(UI_ACCION_ALARMAS, v | (id == 104 ? UI_SOLTADO : 0)); }
    if (id == 3 || id == 103) { /* la barra rápida: sube o baja lo de ella y los efectos juntos */
        /* volumen general: ella, efectos, ambiente y arrullo suben y bajan juntos, cada uno a su proporción (las alarmas no: el despertador debe sonar siempre) */
        const int antes = s_disp_vista.volumen;
        int ef = v, am, ar;
        if (antes > 0) { ef = (int)s_disp_vista.efectos * v / antes; am = (int)s_disp_vista.ambiente * v / antes; ar = (int)s_disp_vista.arrullo_vol * v / antes; }
        else { am = v; ar = v; }
        if (ef > 100) { ef = 100; }
        if (am > 100) { am = 100; }
        if (ar > 100) { ar = 100; }
        s_disp_vista.volumen = (uint8_t)v; s_disp_vista.efectos = (uint8_t)ef; s_disp_vista.ambiente = (uint8_t)am; s_disp_vista.arrullo_vol = (uint8_t)ar;
        accion(UI_ACCION_VOLUMEN, v | (id == 103 ? UI_SOLTADO : 0));
        accion(UI_ACCION_EFECTOS, ef);
        accion(UI_ACCION_AMBIENTE, am);
        accion(UI_ACCION_ARRULLO_VOL, ar);
    }
    if (id == 5 || id == 105) { s_disp_vista.arrullo_vol = (uint8_t)v; accion(UI_ACCION_ARRULLO_VOL, v | (id == 105 ? UI_SOLTADO : 0)); }
    if (id == 9) panel_ajustes();
}

static void arrullo_cb(lv_event_t *e)
{
    (void)e;
    s_disp_vista.arrullo = s_disp_vista.arrullo ? 0 : 3; /* sin arrullo o grillos */
    accion(UI_ACCION_ARRULLO, s_disp_vista.arrullo);
    panel_sonido();
}
static void arrullo_dur_cb(lv_event_t *e)
{
    (void)e;
    s_disp_vista.arrullo_dur = (uint8_t)((s_disp_vista.arrullo_dur + 1) % 3); /* 30 min, 1 hora, 2 horas */
    accion(UI_ACCION_ARRULLO_DUR, s_disp_vista.arrullo_dur);
    panel_sonido();
}
static void musica_cb(lv_event_t *e)
{
    (void)e;
    s_disp_vista.musica = !s_disp_vista.musica;
    accion(UI_ACCION_MUSICA, s_disp_vista.musica);
    panel_sonido();
}

static void panel_sonido(void)
{
    if (s_disp_aj && s_panel != PANEL_SONIDO) s_disp_vista = *s_disp_aj;
    abrir_tarjeta(PANEL_SONIDO, 340, 340);
    if (!s_pet) return;
    /* todos los sonidos en una lista que se desliza hacia abajo; cada barra lleva adentro lo que hace (sin renglón aparte) */
    lv_obj_t *sc = caja(s_tarjeta);
    lv_obj_set_size(sc, 320, 258);
    lv_obj_align(sc, LV_ALIGN_TOP_MID, 0, 40);
    lv_obj_add_flag(sc, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(sc, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(sc, LV_SCROLLBAR_MODE_AUTO);
    s_barra_txt = "ELLA";
    barra(sc, 4, ICO_NO, ICO_PARLANTE, 0, 100, s_disp_vista.volumen, sonido_cb, 0);
    s_barra_txt = "EFECTOS";
    barra(sc, 50, ICO_NO, ICO_FUEGOS, 0, 100, s_disp_vista.efectos, sonido_cb, 2);
    s_barra_txt = "AMBIENTE";
    barra(sc, 96, ICO_NO, ICO_PAJARO, 0, 100, s_disp_vista.ambiente, sonido_cb, 1);
    s_barra_txt = "ALARMAS";
    barra(sc, 142, ICO_LUNA, ICO_DESPERTADOR, PET_ALARMA_VOLUMEN_MIN, 100, s_disp_vista.alarmas ? s_disp_vista.alarmas : PET_ALARMA_VOLUMEN, sonido_cb, 4);
    if (!s_nino) { lv_obj_t *r = texto(sc, "SUENAN AUNQUE LOS DEMÁS ESTÉN EN SILENCIO", C_GRIS, 2, 290); lv_obj_align(r, LV_ALIGN_TOP_MID, 0, 182); }
    /* escuchar música: usa el micrófono con la pantalla prendida, no graba nada; apagado de fábrica */
    lv_obj_t *mb = boton(sc, 280, 44, musica_cb, 0);
    lv_obj_align(mb, LV_ALIGN_TOP_MID, 0, 226);
    lv_obj_set_style_bg_color(mb, lv_color_hex(s_disp_vista.musica ? C_VERDE : 0x6A6E80), 0);
    lv_obj_t *mi = icono(mb, ICO_NOTA, 2);
    lv_obj_align(mi, s_nino ? LV_ALIGN_CENTER : LV_ALIGN_LEFT_MID, s_nino ? 0 : 14, 0);
    if (!s_nino) { lv_obj_t *mt = texto(mb, s_disp_vista.musica ? "ESCUCHA MÚSICA" : "NO ESCUCHA MÚSICA", C_BLANCO, 2, 0); lv_obj_align(mt, LV_ALIGN_LEFT_MID, 44, 0); }
    if (!s_nino) { lv_obj_t *r = texto(sc, "USA EL MICRÓFONO CON LA PANTALLA PRENDIDA. NO GRABA NADA", C_GRIS, 2, 290); lv_obj_align(r, LV_ALIGN_TOP_MID, 0, 274); }
    /* arrullo: el canto de los grillos mientras ella duerme, con su propio volumen; apagado de fábrica */
    lv_obj_t *ab = boton(sc, 280, 44, arrullo_cb, 0);
    lv_obj_align(ab, LV_ALIGN_TOP_MID, 0, 328);
    lv_obj_set_style_bg_color(ab, lv_color_hex(s_disp_vista.arrullo ? C_VERDE : 0x6A6E80), 0);
    lv_obj_t *ai = icono(ab, s_disp_vista.arrullo ? ICO_GRILLO : ICO_LUNA, 2);
    lv_obj_align(ai, s_nino ? LV_ALIGN_CENTER : LV_ALIGN_LEFT_MID, s_nino ? 0 : 14, 0);
    if (!s_nino) { lv_obj_t *at = texto(ab, s_disp_vista.arrullo ? "ARRULLO: GRILLOS" : "SIN ARRULLO", C_BLANCO, 2, 0); lv_obj_align(at, LV_ALIGN_LEFT_MID, 44, 0); }
    s_barra_txt = "VOLUMEN DEL ARRULLO";
    barra(sc, 382, ICO_NO, ICO_GRILLO, 0, 100, s_disp_vista.arrullo_vol, sonido_cb, 5);
    static const char *const TD[3] = {"DURA 30 MINUTOS", "DURA 1 HORA", "DURA 2 HORAS"};
    lv_obj_t *db = boton(sc, 280, 40, arrullo_dur_cb, 0);
    lv_obj_align(db, LV_ALIGN_TOP_MID, 0, 428);
    lv_obj_set_style_bg_color(db, lv_color_hex(0x6A6E80), 0);
    lv_obj_t *di = icono(db, ICO_RELOJ, 2);
    lv_obj_align(di, s_nino ? LV_ALIGN_CENTER : LV_ALIGN_LEFT_MID, s_nino ? 0 : 14, 0);
    if (!s_nino) { lv_obj_t *dt = texto(db, TD[s_disp_vista.arrullo_dur % 3], C_BLANCO, 2, 0); lv_obj_align(dt, LV_ALIGN_LEFT_MID, 44, 0); }
    if (!s_nino) { lv_obj_t *r = texto(sc, "SUENA CUANDO ELLA SE DUERME", C_GRIS, 2, 290); lv_obj_align(r, LV_ALIGN_TOP_MID, 0, 474); }
    lv_obj_t *fin = caja(sc); /* un poco de aire al final de la lista */
    lv_obj_set_size(fin, 10, 10);
    lv_obj_align(fin, LV_ALIGN_TOP_MID, 0, 510);
    area_lista(sc);
    volver(ajustes_cb, 9);
}

static void pantalla_cb(lv_event_t *e)
{
    int id = (int)(intptr_t)lv_event_get_user_data(e);
    if (id == 0 || id == 100) { /* brillo: cambia en vivo mientras arrastras */
        int v = (int)lv_slider_get_value(lv_event_get_target(e));
        s_disp_vista.brillo = (uint8_t)v;
        accion(UI_ACCION_BRILLO, v | (id == 100 ? UI_SOLTADO : 0));
        return;
    }
    if (id >= 10 && id < 15) { s_disp_vista.apagado = (uint8_t)(id - 10); accion(UI_ACCION_APAGADO, id - 10); panel_pantalla(); return; }
    if (id >= 30 && id < 34) { s_disp_vista.pecera = (uint8_t)(id - 30); accion(UI_ACCION_PECERA_DUR, id - 30); panel_pantalla(); return; }
    if (id == 20) { cerrar_panel(); entrar_pecera(false); return; }
    if (id == 22) { panel_ritmo(); return; }
    if (id == 23) { s_disp_vista.fuente = !s_disp_vista.fuente; s_fuente_grande = s_disp_vista.fuente != 0; accion(UI_ACCION_FUENTE, s_disp_vista.fuente); panel_pantalla(); return; }
    if (id == 21) { s_disp_vista.girar = !s_disp_vista.girar; accion(UI_ACCION_GIRAR, s_disp_vista.girar); panel_pantalla(); return; }
    if (id == 9) panel_ajustes();
}

static void panel_pantalla(void)
{
    if (s_panel != PANEL_PANTALLA) { s_pag = 0; if (s_disp_aj) s_disp_vista = *s_disp_aj; }
    abrir_tarjeta(PANEL_PANTALLA, 352, 400);
    if (s_pag == 0) { /* página 1: el brillo y cuánto dura encendida */
        s_barra_txt = "BRILLO";
        barra(s_tarjeta, 56, ICO_LUNA, ICO_SOL, PET_BRILLO_MIN, 100, s_disp_vista.brillo, pantalla_cb, 0);
        if (!s_nino) { lv_obj_t *r = texto(s_tarjeta, "SE APAGA SOLA A LOS", C_GRIS, 2, 0); lv_obj_align(r, LV_ALIGN_TOP_MID, 0, 100); }
        static const char *const AP[5] = {"10 S", "20 S", "30 S", "1 MIN", "2 MIN"};
        for (int k = 0; k < 5; k++) {
            lv_obj_t *b = boton(s_tarjeta, 68, 46, pantalla_cb, 10 + k);
            lv_obj_align(b, LV_ALIGN_TOP_MID, (k - 2) * 71, 122);
            if (k == s_disp_vista.apagado) lv_obj_set_style_bg_color(b, lv_color_hex(C_VIOLETA), 0);
            lv_obj_t *l = texto(b, AP[k], C_BLANCO, 2, 0);
            lv_obj_center(l);
        }
        /* (v99) la pecera es para mirarla: dura lo que se elija (con el cargador puesto no se apaga nunca) */
        lv_obj_t *pi = icono(s_tarjeta, ICO_PECERA, 2);
        lv_obj_align(pi, LV_ALIGN_TOP_MID, s_nino ? 0 : -128, 180);
        if (!s_nino) { lv_obj_t *r = texto(s_tarjeta, "LA PECERA SE APAGA A", C_GRIS, 2, 0); lv_obj_align(r, LV_ALIGN_TOP_MID, 14, 184); }
        static const char *const PE[4] = {"10 MIN", "30 MIN", "1 HORA", "NUNCA"};
        for (int k = 0; k < 4; k++) {
            lv_obj_t *b = boton(s_tarjeta, 82, 46, pantalla_cb, 30 + k);
            lv_obj_align(b, LV_ALIGN_TOP_MID, (k * 2 - 3) * 43, 208);
            if (k == s_disp_vista.pecera) lv_obj_set_style_bg_color(b, lv_color_hex(C_VIOLETA), 0);
            lv_obj_t *l = texto(b, PE[k], C_BLANCO, 2, 0);
            lv_obj_center(l);
        }
    } else { /* página 2: lo demás, con botones grandes */
        /* se endereza sola: si das vuelta el reloj, la pantalla también */
        lv_obj_t *gb = boton(s_tarjeta, 290, 58, pantalla_cb, 21);
        lv_obj_align(gb, LV_ALIGN_TOP_MID, 0, 64);
        lv_obj_set_style_bg_color(gb, lv_color_hex(s_disp_vista.girar ? C_VERDE : 0x6A6E80), 0);
        lv_obj_t *gi = icono(gb, ICO_FLECHA, 3);
        lv_obj_update_layout(gi);
        lv_image_set_pivot(gi, lv_obj_get_width(gi) / 2, lv_obj_get_height(gi) / 2);
        lv_image_set_rotation(gi, s_disp_vista.girar ? 2700 : 900);
        lv_obj_align(gi, s_nino ? LV_ALIGN_CENTER : LV_ALIGN_LEFT_MID, s_nino ? 0 : 16, 0);
        if (!s_nino) { lv_obj_t *gt = texto(gb, s_disp_vista.girar ? "SE ENDEREZA" : "NO SE ENDEREZA", C_BLANCO, 2, 0); lv_obj_align(gt, LV_ALIGN_LEFT_MID, 62, 0); }
        lv_obj_t *rb = boton(s_tarjeta, 290, 58, pantalla_cb, 22);
        lv_obj_align(rb, LV_ALIGN_TOP_MID, 0, 130);
        lv_obj_t *ri = icono(rb, ICO_RELOJ, 3);
        lv_obj_align(ri, s_nino ? LV_ALIGN_CENTER : LV_ALIGN_LEFT_MID, s_nino ? 0 : 16, 0);
        if (!s_nino) { lv_obj_t *rt = texto(rb, "RITMO DE SU TIEMPO", C_BLANCO, 2, 0); lv_obj_align(rt, LV_ALIGN_LEFT_MID, 62, 0); }
        /* tamaño de las letras de lectura */
        lv_obj_t *fb = boton(s_tarjeta, 290, 58, pantalla_cb, 23);
        lv_obj_align(fb, LV_ALIGN_TOP_MID, 0, 196);
        lv_obj_set_style_bg_color(fb, lv_color_hex(s_disp_vista.fuente ? C_VERDE : 0x6A6E80), 0);
        lv_obj_t *fi = icono(fb, ICO_LIBRO, 3);
        lv_obj_align(fi, s_nino ? LV_ALIGN_CENTER : LV_ALIGN_LEFT_MID, s_nino ? 0 : 16, 0);
        if (!s_nino) { lv_obj_t *ft = texto(fb, s_disp_vista.fuente ? "TEXTO GRANDE" : "TEXTO NORMAL", C_BLANCO, 2, 0); lv_obj_align(ft, LV_ALIGN_LEFT_MID, 62, 0); }
    }
    paginas(2, panel_pantalla, 268);
    volver(ajustes_cb, 9);
}

static void flechas(lv_obj_t *padre, lv_event_cb_t cb, int id_arriba, int x, int y_arriba, int y_abajo);
static void flecha_lado(lv_obj_t *padre, lv_event_cb_t cb, int id, int x, int y, bool derecha);
static void panel_temporizador(void);
static void hora_menu_cb(lv_event_t *e)
{
    int id = (int)(intptr_t)lv_event_get_user_data(e);
    if (id == 0) panel_hora();
    else if (id == 1) panel_dormir();
    else if (id == 2) panel_alarmas();
    else if (id == 3) panel_temporizador();
    else panel_ajustes();
}

/* --- temporizador: unos minutos y suena (con el motor de las alarmas) --- */
static int s_tm_min = 10;
static lv_obj_t *s_tm_val;
static art_img_t s_tm_art;
static void tm_refrescar(void)
{
    char t[16];
    snprintf(t, sizeof t, "%d MIN", s_tm_min);
    art_texto(&s_tm_art, t, s_portal ? 0xFFF3C8 : C_TINTA, 4, 0);
    poner_src(s_tm_val, &s_tm_art.dsc);
    lv_obj_update_layout(s_tm_val);
    lv_obj_align(s_tm_val, LV_ALIGN_TOP_MID, 0, 112 + (54 - lv_obj_get_height(s_tm_val)) / 2); /* (v99) centrado con las flechas */
}
static void tm_cb(lv_event_t *e)
{
    int id = (int)(intptr_t)lv_event_get_user_data(e);
    bool rapido = lv_event_get_code(e) == LV_EVENT_LONG_PRESSED_REPEAT;
    if (id == 0 || id == 1) {
        s_tm_min += (id ? -1 : 1) * (rapido ? 5 : 1);
        if (s_tm_min < 1) s_tm_min = 1;
        if (s_tm_min > 180) s_tm_min = 180;
        tm_refrescar();
        return;
    }
    static const int PRE[4] = {1, 5, 10, 30};
    if (id >= 10 && id < 14) { s_tm_min = PRE[id - 10]; tm_refrescar(); return; }
    if (id == 20) { accion(UI_ACCION_TEMPORIZADOR, s_tm_min * 60); globo(ICO_RELOJ, s_nino ? NULL : "¡YA EMPEZÓ! TE AVISO", 2500); cerrar_panel(); return; }
    if (id == 21) { accion(UI_ACCION_TEMPORIZADOR, 0); cerrar_panel(); return; }
    panel_hora_menu();
}
static void panel_temporizador(void)
{
    abrir_tarjeta(PANEL_TEMPORIZADOR, 420, 420);
    uint32_t queda = s_pet ? pet_timer_left(s_pet, s_ultimo_t) : 0;
    if (queda) { /* ya está corriendo: cuánto falta y cancelar */
        char q[48];
        snprintf(q, sizeof q, "FALTAN %u MIN", (unsigned)((queda + 59) / 60));
        lv_obj_t *l = texto(s_tarjeta, q, C_TINTA, 3, 0);
        lv_obj_align(l, LV_ALIGN_TOP_MID, 0, 130);
        lv_obj_t *c = boton(s_tarjeta, 170, 48, tm_cb, 21);
        lv_obj_align(c, LV_ALIGN_TOP_MID, 0, 210);
        lv_obj_set_style_bg_color(c, lv_color_hex(0x7A4458), 0);
        lv_obj_t *ct = s_nino ? icono(c, ICO_NO, 2) : texto(c, "CANCELAR", C_BLANCO, 2, 0);
        lv_obj_center(ct);
    } else {
        flecha_lado(s_tarjeta, tm_cb, 1, -116, 112, false); /* menos a la izquierda, más a la derecha */
        flecha_lado(s_tarjeta, tm_cb, 0, 116, 112, true);
        s_tm_val = lv_image_create(s_tarjeta);
        static const char *const P[4] = {"1", "5", "10", "30"};
        for (int k = 0; k < 4; k++) {
            lv_obj_t *b = boton(s_tarjeta, 58, 36, tm_cb, 10 + k);
            lv_obj_align(b, LV_ALIGN_TOP_MID, (k * 2 - 3) * 34, 196);
            lv_obj_t *l = texto(b, P[k], C_BLANCO, 2, 0);
            lv_obj_center(l);
        }
        lv_obj_t *ok = boton(s_tarjeta, 150, 46, tm_cb, 20);
        lv_obj_align(ok, LV_ALIGN_TOP_MID, 0, 256);
        lv_obj_set_style_bg_color(ok, lv_color_hex(C_VERDE), 0);
        lv_obj_t *ot = s_nino ? icono(ok, ICO_SI, 3) : texto(ok, "¡EMPEZAR!", C_BLANCO, 2, 0);
        lv_obj_center(ot);
        tm_refrescar();
    }
    volver(tm_cb, 9);
}

static void panel_hora_menu(void)
{
    abrir_tarjeta(PANEL_HORA_MENU, 420, 420);
    static const art_icono_t I[4] = {ICO_CALENDARIO, ICO_LUNA, ICO_DESPERTADOR, ICO_RELOJ};
    static const char *const T[4] = {"FECHA Y HORA", "DORMIR", "ALARMAS", "TEMPORIZADOR"};
    /* cuatro mosaicos grandes (2 x 2): el dibujo arriba y el nombre debajo; caben justo dentro del círculo */
    for (int k = 0; k < 4; k++) {
        lv_obj_t *b = boton(s_tarjeta, 164, 134, hora_menu_cb, k);
        lv_obj_align(b, LV_ALIGN_TOP_MID, (k % 2 ? 1 : -1) * 86, 66 + (k / 2) * 144);
        lv_obj_set_style_radius(b, 26, 0);
        lv_obj_t *ic = icono(b, I[k], s_nino ? 9 : 7);
        if (s_nino) lv_obj_center(ic);
        else {
            lv_obj_align(ic, LV_ALIGN_TOP_MID, 0, 8);
            lv_obj_t *tt = texto(b, T[k], C_BLANCO, 2, 0);
            lv_obj_align(tt, LV_ALIGN_BOTTOM_MID, 0, -10);
        }
    }
    volver(hora_menu_cb, 9);
}

/* --- Mis mascotas: varias en el mismo reloj. La que no juega se queda descansando (no pasa el tiempo
 * para ella) y se puede volver a ella cuando quieras. --- */
static const ui_mascotas_t *s_mascotas;
void ui_set_pet_store(const ui_mascotas_t *m) { s_mascotas = m; }
static EXT_RAM_BSS_ATTR pet_t s_masc_tmp;

static bool masc_leer(int i, pet_t *out) { return s_mascotas && s_mascotas->leer && s_mascotas->leer(i, out); }

static int8_t s_vive_ov[PET_GUARDADAS] = {-1, -1, -1}; /* lo elegido, mientras main lo guarda */
static void mascotas_cb(lv_event_t *e)
{
    int q = (int)(intptr_t)lv_event_get_user_data(e);
    if (q >= 10 && q <= 12) memset(s_vive_ov, -1, sizeof s_vive_ov); /* cambian los lugares */
    if (q >= 0 && q < PET_GUARDADAS) { /* tocaste un lugar */
        if (masc_leer(q, &s_masc_tmp)) { s_masc_sel = s_masc_sel == q ? -1 : q; s_masc_conf = 0; }
        else { s_masc_sel = -1; s_masc_conf = 1; } /* uno libre: mascota nueva */
        panel_mascotas();
        return;
    }
    if (q == 10) { /* sí, mascota nueva */
        cerrar_panel();
        po_fuera();   /* (fw 108) Pochi no se pasa a la mascota nueva */
        s_nombre[0] = 0;
        s_masc_conf = 0;
        accion(UI_ACCION_NUEVA, 0);
    } else if (q == 11 && s_masc_sel >= 0) { /* jugar con la elegida */
        cerrar_panel();
        po_fuera();   /* (fw 108) la visita de Pochi es de la otra mascota: no sigue en este mundo */
        accion(UI_ACCION_CAMBIAR, s_masc_sel);
        s_masc_sel = -1;
    } else if (q == 12 && s_masc_sel >= 0) { /* se va de viaje: se confirma */
        if (s_masc_conf != 2) { s_masc_conf = 2; panel_mascotas(); return; }
        static EXT_RAM_BSS_ATTR pet_t viajera;
        bool hay = masc_leer(s_masc_sel, &viajera);
        accion(UI_ACCION_SOLTAR, s_masc_sel | (elegir_vehiculo() + 1) << 8);
        s_masc_sel = -1; s_masc_conf = 0;
        cerrar_panel();
        if (hay) viaje_empezar(&viajera);
    } else if (q == 13) { s_masc_conf = 0; s_masc_sel = -1; panel_mascotas(); }
    else if (q == 15 && s_masc_sel >= 0 && masc_leer(s_masc_sel, &s_masc_tmp)) { /* que venga de visita */
        char t[64];
        snprintf(t, sizeof t, "¡%s VIENE DE VISITA!", s_masc_tmp.nombre[0] ? s_masc_tmp.nombre : "TU OTRA MASCOTA");
        accion(UI_ACCION_VISITA_PERFIL, s_masc_sel);
        s_masc_sel = -1;
        cerrar_panel();
        globo(ICO_AMIGOS, s_nino ? NULL : t, 3000);
    }
    else if (q == 14 && s_masc_sel >= 0 && masc_leer(s_masc_sel, &s_masc_tmp)) { /* sigue viviendo o descansa */
        bool vive = !(s_vive_ov[s_masc_sel] >= 0 ? s_vive_ov[s_masc_sel] : s_masc_tmp.perfil_vive);
        s_vive_ov[s_masc_sel] = (int8_t)vive;
        accion(UI_ACCION_PERFIL_VIVE, s_masc_sel | (vive ? 16 : 0));
        panel_mascotas();
    }
}

static void panel_mascotas(void)
{
    abrir_tarjeta(PANEL_MASCOTAS, 352, 372);
    if (!s_pet) return;
    if (!s_nino) { lv_obj_t *t = texto(s_tarjeta, "PERFILES", C_TINTA, 3, 0); lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 12); }
    /* la de ahora */
    lv_obj_t *yo = caja(s_tarjeta);
    lv_obj_set_size(yo, 300, 74);
    lv_obj_align(yo, LV_ALIGN_TOP_MID, 0, 44);
    lv_obj_set_style_bg_color(yo, lv_color_hex(C_VIOLETA), 0);
    lv_obj_set_style_bg_opa(yo, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(yo, 14, 0);
    lv_obj_t *m0 = mini_criatura(yo, 0, pet_look(s_pet), s_pet->etapa, s_pet->forma, 3, EXP_FELIZ);
    lv_obj_align(m0, LV_ALIGN_LEFT_MID, 12, 0);
    char t[112];
    snprintf(t, sizeof t, "%s", s_pet->nombre[0] ? s_pet->nombre : "?");
    lv_obj_t *n0 = texto(yo, t, C_BLANCO, 3, 200);
    lv_obj_align(n0, LV_ALIGN_LEFT_MID, 88, s_nino ? 0 : -10);
    if (!s_nino) { lv_obj_t *a0 = texto(yo, "AHORA", 0xFFE27A, 2, 0); lv_obj_align(a0, LV_ALIGN_LEFT_MID, 88, 16); }
    /* las que descansan, y los lugares libres */
    int libres = 0;
    for (int i = 0; i < PET_GUARDADAS; i++) {
        bool hay = masc_leer(i, &s_masc_tmp);
        libres += !hay;
        lv_obj_t *b = boton(s_tarjeta, 96, 104, mascotas_cb, i);
        lv_obj_align(b, LV_ALIGN_TOP_MID, (i - 1) * 104, 128);
        if (hay) {
            if (i == s_masc_sel) lv_obj_set_style_bg_color(b, lv_color_hex(C_VIOLETA), 0);
            lv_obj_t *mc = mini_criatura(b, 1 + i, pet_look(&s_masc_tmp), s_masc_tmp.etapa, s_masc_tmp.forma, 2, s_masc_tmp.etapa ? EXP_CERRADOS : EXP_ABIERTOS);
            lv_obj_align(mc, LV_ALIGN_TOP_MID, 0, 8);
            if (!s_nino) { lv_obj_t *nt = texto(b, s_masc_tmp.nombre[0] ? s_masc_tmp.nombre : "?", C_BLANCO, 2, 90); lv_obj_align(nt, LV_ALIGN_BOTTOM_MID, 0, -8); }
        } else {
            lv_obj_set_style_bg_opa(b, LV_OPA_50, 0);
            lv_obj_t *h = icono(b, ICO_HUEVO, 3);
            lv_obj_align(h, LV_ALIGN_TOP_MID, 0, 14);
            if (!s_nino) { lv_obj_t *nt = texto(b, "NUEVA", C_BLANCO, 2, 0); lv_obj_align(nt, LV_ALIGN_BOTTOM_MID, 0, -8); }
        }
    }
    /* abajo: lo que se puede hacer */
    if (s_masc_sel >= 0 && masc_leer(s_masc_sel, &s_masc_tmp)) {
        if (s_masc_conf == 2) {
            snprintf(t, sizeof t, "¿DESPEDIR A %s? SE VA DE VIAJE A EXPLORAR EL MUNDO Y NO VUELVE", s_masc_tmp.nombre[0] ? s_masc_tmp.nombre : "{ELLA}");
            if (!s_nino) { lv_obj_t *q = texto(s_tarjeta, t, 0xC0405A, 2, 300); lv_obj_align(q, LV_ALIGN_TOP_MID, 0, 244); }
            lv_obj_t *si = boton(s_tarjeta, 120, 46, mascotas_cb, 12);
            mantener(si, mascotas_cb, 12);
            lv_obj_align(si, LV_ALIGN_BOTTOM_MID, -66, -22);
            lv_obj_set_style_bg_color(si, lv_color_hex(0x7A4458), 0);
            lv_obj_t *st = texto(si, "MANTÉN: SÍ", C_BLANCO, 2, 110); lv_obj_center(st);
            lv_obj_t *no = boton(s_tarjeta, 120, 46, mascotas_cb, 13);
            lv_obj_align(no, LV_ALIGN_BOTTOM_MID, 66, -22);
            lv_obj_t *nt = s_nino ? icono(no, ICO_NO, 2) : texto(no, "NO", C_BLANCO, 2, 0); lv_obj_center(nt);
        } else {
            lv_obj_t *ju = boton(s_tarjeta, 170, 50, mascotas_cb, 11);
            lv_obj_align(ju, LV_ALIGN_BOTTOM_MID, -48, -28);
            lv_obj_set_style_bg_color(ju, lv_color_hex(C_VERDE), 0);
            snprintf(t, sizeof t, "TRAER A %s", s_masc_tmp.nombre[0] ? s_masc_tmp.nombre : "{ELLA}");
            lv_obj_t *jt = s_nino ? icono(ju, ICO_SI, 3) : texto(ju, t, C_BLANCO, 2, 160); lv_obj_center(jt);
            if (!s_suave) { /* despedirla: solo el adulto (los niños no ven esta opción) */
                lv_obj_t *sv = boton(s_tarjeta, 80, 50, mascotas_cb, 12);
                lv_obj_align(sv, LV_ALIGN_BOTTOM_MID, 92, -28);
                lv_obj_t *svt = texto(sv, "DE VIAJE", C_BLANCO, 2, 70); lv_obj_center(svt);
            } else lv_obj_align(ju, LV_ALIGN_BOTTOM_MID, 0, -28);
            /* mientras no es la activa: sigue viviendo o descansa */
            bool vive = s_vive_ov[s_masc_sel] >= 0 ? s_vive_ov[s_masc_sel] : s_masc_tmp.perfil_vive;
            lv_obj_t *vb = boton(s_tarjeta, 196, 36, mascotas_cb, 14);
            lv_obj_align(vb, LV_ALIGN_TOP_MID, -38, 240);
            if (s_pet->etapa != STAGE_HUEVO && s_masc_tmp.etapa != STAGE_HUEVO) { /* que venga de visita */
                lv_obj_t *iv = boton(s_tarjeta, 70, 36, mascotas_cb, 15);
                lv_obj_align(iv, LV_ALIGN_TOP_MID, 104, 240);
                lv_obj_t *ivi = icono(iv, ICO_AMIGOS, 2); lv_obj_center(ivi);
            }
            lv_obj_set_style_bg_color(vb, lv_color_hex(vive ? C_VIOLETA : 0x6A6E80), 0);
            lv_obj_t *vt = s_nino ? icono(vb, vive ? ICO_SOL : ICO_ZETA, 2)
                                  : texto(vb, vive ? "SIN TI: VIVE" : "SIN TI: DESCANSA", C_BLANCO, 2, 186);
            lv_obj_center(vt);
        }
    } else if (s_masc_conf == 1) {
        snprintf(t, sizeof t, "¿UN HUEVO NUEVO? %s SE QUEDA DESCANSANDO", s_pet->nombre[0] ? s_pet->nombre : "LA DE AHORA");
        if (!s_nino) { lv_obj_t *q = texto(s_tarjeta, t, C_TINTA, 2, 300); lv_obj_align(q, LV_ALIGN_TOP_MID, 0, 240); }
        lv_obj_t *si = boton(s_tarjeta, 170, 46, mascotas_cb, 10);
        lv_obj_align(si, LV_ALIGN_BOTTOM_MID, 0, -20);
        lv_obj_set_style_bg_color(si, lv_color_hex(C_VERDE), 0);
        lv_obj_t *st = s_nino ? icono(si, ICO_HUEVO, 3) : texto(si, "SÍ, UNO NUEVO", C_BLANCO, 2, 0); lv_obj_center(st);
    } else if (!s_nino) {
        const char *ay = libres ? "TOCA UNA PARA TRAERLA, O UN HUEVO PARA EMPEZAR OTRA" : "PARA UNA NUEVA, PRIMERO DEJA IR A UNA";
        lv_obj_t *q = texto(s_tarjeta, ay, C_GRIS, 2, 300);
        lv_obj_align(q, LV_ALIGN_TOP_MID, 0, 248);
    }
}

/* --- Respaldo: cómo guardarla en el computador o traerla de otro reloj --- */
static void panel_respaldo(void)
{
    abrir_tarjeta(PANEL_RESPALDO, 340, 330);
    lv_obj_t *ic = icono(s_tarjeta, ICO_RESPALDO, 4);
    lv_obj_align(ic, LV_ALIGN_TOP_MID, 0, 16);
    char t[200];
    snprintf(t, sizeof t, "PARA GUARDAR A %s EN EL COMPUTADOR O PASARL{A} A OTRO ORBE, CONECTA EL ORBE CON EL CABLE Y "
                          "USA EL EDITOR. SE GUARDA TODO: SUS RECUERDOS, EL DIARIO Y EL ÁLBUM.",
             s_pet && s_pet->nombre[0] ? s_pet->nombre : "TU MASCOTA");
    for (char *q = t; *q; q++) if (*q >= 'a' && *q <= 'z') *q = (char)(*q - 32);
    lv_obj_t *tx = texto(s_tarjeta, t, C_TINTA, 2, 280);
    lv_obj_align(tx, LV_ALIGN_TOP_MID, 0, 80);
    boton_cerrar(150, "ACEPTAR");
}

/* --- Traer una mascota de un respaldo: siempre se confirma en el reloj --- */
static EXT_RAM_BSS_ATTR pet_t s_rest;
static void restaurar_cb(lv_event_t *e)
{
    int si = (int)(intptr_t)lv_event_get_user_data(e);
    cerrar_panel();
    accion(UI_ACCION_RESTAURAR, si);
}

static void panel_restaurar(void)
{
    if (s_bienv_activa) { s_bienv_activa = false; ocultar(s_bienv, true); }
    abrir_tarjeta(PANEL_RESTAURAR, 340, 340);
    lv_obj_t *ic = icono(s_tarjeta, ICO_RESPALDO, 3);
    lv_obj_align(ic, LV_ALIGN_TOP_MID, 0, 12);
    char t[96];
    snprintf(t, sizeof t, "¿TRAER A %s?", s_rest.nombre[0] ? s_rest.nombre : "ESTA MASCOTA");
    for (char *q = t; *q; q++) if (*q >= 'a' && *q <= 'z') *q = (char)(*q - 32);
    lv_obj_t *tt = texto(s_tarjeta, t, C_TINTA, 3, 300);
    lv_obj_align(tt, LV_ALIGN_TOP_MID, 0, 56);
    lv_obj_t *yo = s_rest.etapa == STAGE_HUEVO ? icono(s_tarjeta, ICO_HUEVO, 6)
                                              : mini_criatura(s_tarjeta, 0, pet_look(&s_rest), s_rest.etapa, s_rest.forma, 4, EXP_FELIZ);
    lv_obj_align(yo, LV_ALIGN_TOP_MID, 0, 96);
    if (s_pet && s_pet->nombre[0] && s_pet->etapa != STAGE_HUEVO) {
        snprintf(t, sizeof t, "REEMPLAZA A %s (MANTÉN APRETADO EL SÍ)", s_pet->nombre);
        for (char *q = t; *q; q++) if (*q >= 'a' && *q <= 'z') *q = (char)(*q - 32);
        lv_obj_t *r = texto(s_tarjeta, t, 0xC0405A, 2, 280);
        lv_obj_align(r, LV_ALIGN_TOP_MID, 0, 196);
    }
    for (int i = 0; i < 2; i++) {
        lv_obj_t *b = boton(s_tarjeta, 120, 56, restaurar_cb, i ? 1 : 0);
        if (i && s_pet && s_pet->nombre[0] && s_pet->etapa != STAGE_HUEVO) mantener(b, restaurar_cb, 1); /* pisa a la de ahora */
        lv_obj_align(b, LV_ALIGN_BOTTOM_MID, i ? 70 : -70, -24);
        lv_obj_set_style_bg_color(b, lv_color_hex(i ? C_VERDE : 0x7A4458), 0);
        lv_obj_t *bi = icono(b, i ? ICO_SI : ICO_NO, 3);
        lv_obj_center(bi);
    }
}

void ui_ask_restore(const pet_t *nueva)
{
    if (!nueva) return;
    s_rest = *nueva;
    cerrar_panel();
    panel_restaurar();
}

/* --- Resumen: lo que pasó mientras no estabas --- */
static uint8_t s_resumen[HALL_COUNT];
static int s_resumen_n;
#define N_RES 5
static art_icono_t s_res_ico[N_RES];
static char s_res_txt[N_RES][56];
static int s_res_n;

static void resumen_linea(art_icono_t ico, const char *txt)
{
    for (int i = 0; i < s_res_n; i++) if (!strcmp(s_res_txt[i], txt)) return; /* sin repetir */
    if (s_res_n >= N_RES) return;
    s_res_ico[s_res_n] = ico;
    snprintf(s_res_txt[s_res_n], sizeof s_res_txt[0], "%s", txt);
    for (char *q = s_res_txt[s_res_n]; *q; q++) if (*q >= 'a' && *q <= 'z') *q = (char)(*q - 32);
    s_res_n++;
}

static void panel_resumen(void)
{
    abrir_tarjeta(PANEL_RESUMEN, 344, s_res_n ? 360 : 280);
    if (!s_nino) {
        lv_obj_t *t = texto(s_tarjeta, "MIENTRAS NO ESTABAS:", C_TINTA, 2, 290);
        lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 20);
    } else {
        lv_obj_t *t = icono(s_tarjeta, ICO_LUPA, 3);
        lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 10);
    }
    static const art_icono_t I[HALL_COUNT] = {ICO_FRUTA, ICO_MIEL, ICO_HIERBA, ICO_SEMILLA, ICO_PIEDRA, ICO_RAMITA, ICO_CHAPITA, ICO_HOJA_SECA, ICO_MUSGO};
    int k = 0, total = 0;
    for (int i = 0; i < HALL_COUNT; i++) total += s_resumen[i] > 0;
    int yh = s_res_n ? 70 : 110;
    for (int i = 0; i < HALL_COUNT; i++) { /* lo que encontró, con cuántos */
        if (!s_resumen[i] || k >= 5) continue;
        int x = (int)((k - ((total > 5 ? 5 : total) - 1) / 2.0f) * 58);
        lv_obj_t *ic = icono(s_tarjeta, I[i], 3);
        lv_obj_align(ic, LV_ALIGN_TOP_MID, x, yh);
        char n[6];
        snprintf(n, sizeof n, "x%d", s_resumen[i]);
        lv_obj_t *t = texto(s_tarjeta, n, C_TINTA, 2, 0);
        lv_obj_align(t, LV_ALIGN_TOP_MID, x, yh + 40);
        k++;
    }
    int y = total ? yh + 66 : 64;
    if (s_res_n) { /* y lo que vivió: si es mucho, se desliza */
        lv_obj_t *ar = area_texto(s_tarjeta, 16, y, 312, 360 - 74 - y);
        lv_obj_set_flex_align(ar, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
        for (int i = 0; i < s_res_n; i++) {
            lv_obj_t *fila = caja(ar);
            lv_obj_set_size(fila, 296, LV_SIZE_CONTENT);
            lv_obj_t *ic = icono(fila, s_res_ico[i], 2);
            lv_obj_set_pos(ic, 8, 0);
            if (!s_nino) { lv_obj_t *t = texto(fila, s_res_txt[i], C_TINTA, 2, 240); lv_obj_set_pos(t, 46, 2); }
        }
        area_lista(ar);
    }
    memset(s_resumen, 0, sizeof s_resumen);
    s_resumen_n = 0;
    s_res_n = 0;
    boton_cerrar(130, "ACEPTAR");
}

/* --- Más: cómo está, gustos y acciones --- */
static int s_diario_tab, s_diario_pag;
static void panel_diario(void);
static void panel_cofre(void);
static int s_cofre_vista, s_cofre_sub; /* vista: 0 el cofre, 1 tesoros, 2 enciclopedia, 3 artefactos místicos (cuadrícula de familias) */
static bool s_obj_cofre; static int s_obj_cofre_sub = -2; /* el objeto se abrió desde una lista de Tesoros: volver regresa a ella */
static bool s_bolsa_desde_cofre; /* la lista de una familia se abrió desde la cuadrícula de artefactos: volver regresa a ella */

/* el libro de historias: se abre deslizando de izquierda a derecha (y desde Más) */
#define LIBRO_HIST_ACTIVO 1
static void panel_mapa(void);
static void mas_cb(lv_event_t *e)
{
    int q = (int)(intptr_t)lv_event_get_user_data(e);
    switch (q) {
    case 0: cerrar_panel(); accion(s_pet && s_pet->luz_apagada ? ACT_DESPERTAR : ACT_DORMIR, 0); break;
    case 1:
        cerrar_panel();
        if (s_pet && !s_pet->enferma) { globo(ICO_CORAZON, "¡ME SIENTO BIEN!", 2500); return; }
        accion(ACT_CURAR, 0);
        break;
    case 2: cerrar_panel(); accion(ACT_CEPILLAR, 0); break;
    case 3: panel_rincon(); break;
    case 4: panel_amigos(); break;
    case 6: s_diario_tab = 0; s_diario_pag = 0; panel_diario(); break;
    case 7: panel_mapa(); break;
    case 8: s_cofre_vista = 1; s_cofre_sub = -1; panel_cofre(); break; /* la bolsa vive ahora en Tesoros */
    case 10: break; /* el libro mágico se quitó por ahora */
    case 11: s_diario_tab = 1; s_diario_pag = 0; panel_diario(); break;
    case 12: s_diario_tab = 2; s_diario_pag = 0; panel_diario(); break;
    case 13: s_diario_tab = 3; s_diario_pag = 0; panel_diario(); break;
    case 14: panel_cartas(); break;
    case 15: s_cofre_vista = 0; panel_cofre(); break;
    case 16: if (LIBRO_HIST_ACTIVO) panel_libro_hist(); break;
    default: s_confirmar_nueva = 0; panel_ajustes(); break;
    }
}

/* el nombre de la especie (el de fábrica o el que le pusiste en el editor); la fuente lo escribe en mayúsculas */
static const char *nombre_especie(int e) { return pet_species_name((uint8_t)((unsigned)e % 8)); }

/* --- el mapa: las zonas abiertas, adónde ir, y las que faltan por descubrir --- */
static void mapa_cb(lv_event_t *e)
{
    int z = (int)(intptr_t)lv_event_get_user_data(e);
    if (!s_pet || !pet_scene_open(s_pet, (escena_t)z) || s_pet->dormida) { lv_obj_set_style_translate_x(lv_event_get_current_target(e), 6, 0); return; }
    cerrar_panel();
    if (z != (int)s_pet->escena) accion(ACT_IR, z);
}

static void panel_mapa(void)
{
    /* solo los lugares que ya conoce, en cuadrícula y con su nombre: los demás no aparecen hasta descubrirlos */
    abrir_tarjeta(PANEL_MAPA, 400, 400);
    if (!s_pet) return;
    static const char *const NOM[ESC_COUNT] = {"PRADO", "BOSQUE", "LAGUNA", "CERRO", "CUEVA", "ISLA", "CLARO", "MIRADOR"};
    int abiertas[ESC_COUNT], n = 0;
    for (int z = 0; z < ESC_COUNT; z++) if (pet_scene_open(s_pet, (escena_t)z)) abiertas[n++] = z;
    const int filas = (n + 2) / 3, TW = 100, TH = filas > 2 ? 90 : 106, GAP = 8;
    const int y0 = (400 - (filas * TH + (filas - 1) * GAP)) / 2 - 10;
    if (!s_nino) { /* (v40) arriba, el nombre propio del lugar donde está */
        char tt[64];
        snprintf(tt, sizeof tt, "%s", pet_scene_proper((escena_t)s_pet->escena));
        mayusculas(tt);
        lv_obj_t *ti = texto_c(s_tarjeta, tt, 0xFFE27A, 2, 260);
        lv_obj_align(ti, LV_ALIGN_TOP_MID, 0, y0 > 46 ? y0 - 34 : 12);
    }
    for (int i = 0; i < n; i++) {
        const int z = abiertas[i], fila = i / 3, en_fila = (n - fila * 3) < 3 ? n - fila * 3 : 3, col = i % 3;
        lv_obj_t *b = boton(s_tarjeta, TW, TH, mapa_cb, z);
        lv_obj_align(b, LV_ALIGN_TOP_MID, (int)((col - (en_fila - 1) / 2.0f) * (TW + GAP)), y0 + fila * (TH + GAP));
        lv_obj_set_style_radius(b, 22, 0);
        if (z == (int)s_pet->escena) lv_obj_set_style_bg_color(b, lv_color_hex(C_VIOLETA), 0);
        lv_obj_t *ic = icono(b, ICO_ZONA[z], 5);
        lv_obj_align(ic, LV_ALIGN_TOP_MID, 0, 10);
        if (!s_nino) { lv_obj_t *nt = texto(b, NOM[z], 0xFFE27A, 2, 0); lv_obj_align(nt, LV_ALIGN_BOTTOM_MID, 0, -8); }
        if (!pet_scene_seen(s_pet, (escena_t)z)) { lv_obj_t *st = icono(b, ICO_ESTRELLA, 2); lv_obj_align(st, LV_ALIGN_TOP_RIGHT, -4, 4); }
        else if (z == ESC_LAGO) { lv_obj_t *bt = icono(b, lago_congelado(s_pet) ? ICO_COPO : ICO_BOTE, 2); lv_obj_align(bt, LV_ALIGN_TOP_RIGHT, -4, 4); }
    }
}

static void nota_mostrar(art_icono_t ico, uint32_t tinte, const char *titulo, const char *cuerpo);
static void gusto_icono_cb(lv_event_t *e) /* tocar un gusto o un miedo: dice qué es */
{
    const int q = (int)(intptr_t)lv_event_get_user_data(e);
    const art_icono_t ic = (art_icono_t)(q >> 1);
    const art_icono_info_t *in = art_icono_info(ic);
    nota_mostrar(ic, 0, in && in->nombre ? in->nombre : "?", (q & 1) ? "NO LE GUSTA O LE DA MIEDO" : "LE ENCANTA");
}
static lv_obj_t *gusto_icono(lv_obj_t *padre, art_icono_t ic, int odio, int x, int y)
{
    lv_obj_t *o = icono(padre, ic, 4); /* medianos (unos 38 px): se tocan con comodidad sin ocupar tanto */
    lv_obj_set_pos(o, x, y);
    lv_obj_add_flag(o, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(o, 4);
    filtro_toque(o);
    lv_obj_add_event_cb(o, gusto_icono_cb, LV_EVENT_CLICKED, (void *)(intptr_t)(((int)ic << 1) | (odio ? 1 : 0)));
    return o;
}
static void gustos_calcular(int ico[2][5], int nn[2]);
static void gustos(int y)
{
    int ico[2][5], nn[2];
    gustos_calcular(ico, nn);
    /* lo que se descubrió observándola: ❤ lo que le encanta, ✗ lo que no le gusta */
    (void)0;
    goto dibujar;
dibujar:;
    {
        /* centrado en la tarjeta: cada grupo es un corazón (o ✗) más sus iconos */
        int total = 0, grupos = 0;
        for (int g = 0; g < 2; g++) if (nn[g]) { total += 30 * (nn[g] + 1); grupos++; }
        if (!grupos) return;
        total += 14 * (grupos - 1) - 6;
        int ancho = (int)lv_obj_get_width(s_tarjeta); if (ancho <= 0) ancho = 466;
        int x = (ancho - total) / 2;
        for (int odio = 0; odio < 2; odio++) {
            const int n = nn[odio];
            if (!n) continue;
            lv_obj_t *m = icono(s_tarjeta, odio ? ICO_NO : ICO_CORAZON, 2);
            lv_obj_set_pos(m, x, y + 4);
            x += 30;
            for (int i = 0; i < n; i++) { gusto_icono(s_tarjeta, (art_icono_t)ico[odio][i], odio, x, y); x += 30; }
            x += 14;
        }
    }
}
static void gustos_calcular(int ico[2][5], int nn[2])
{
    /* lo que se descubrió observándola: ❤ lo que le encanta, ✗ lo que no le gusta */
    static const art_icono_t COMIDA[FOOD_COUNT] = {ICO_COMIDA, ICO_FRUTA, ICO_MIEL};
    static const art_icono_t CLIMA[CLIMA_COUNT] = {ICO_SOL, ICO_NUBE, ICO_LLUVIA, ICO_VIENTO, ICO_COPO, ICO_RAYO, ICO_FRIO};
    nn[0] = nn[1] = 0;
    for (int odio = 0; odio < 2; odio++) {
        int *iconos = ico[odio], n = 0;
        { /* lo que aprendió: su actividad favorita (❤) o de la que se aburrió (✗) */
            static const struct { uint8_t a; art_icono_t ico; } AI[] = {
                {ACTV_BAILAR, ICO_ESTRELLA}, {ACTV_CANTAR, ICO_NOTA}, {ACTV_AVES, ICO_PAJARO}, {ACTV_PELOTA, ICO_PELOTA}, {ACTV_PESCAR, ICO_PEZ},
                {ACTV_CHAPOTEAR, ICO_LLUVIA}, {ACTV_PERSEGUIR, ICO_MARIPOSA}, {ACTV_TREPAR, ICO_ARBOLITO}, {ACTV_BUSCAR, ICO_HOJA}, {ACTV_EXPLORAR, ICO_LUPA}, {ACTV_PROBAR, ICO_SOMBRERO},
                {ACTV_CAVAR, ICO_SEMILLA}, {ACTV_OLFATEAR, ICO_LUPA}, {ACTV_PASEAR, ICO_HOJAS}, {ACTV_CHARLAR, ICO_CORAZON}, {ACTV_NADAR, ICO_PEZ}};
            const int top = odio ? pet_taste_low(s_pet) : pet_taste_top(s_pet);
            for (unsigned k = 0; k < sizeof AI / sizeof AI[0]; k++) if (AI[k].a == top) iconos[n++] = AI[k].ico;
        }
        { /* su estación favorita (❤) o la que menos le gusta (✗), cuando ya la vivió */
            static const art_icono_t IE[4] = {ICO_FLOR, ICO_SOL, ICO_HOJA, ICO_COPO};
            const int es = pet_season_fav(s_pet, odio != 0);
            if (es >= 0 && n < 4) iconos[n++] = IE[es];
        }
        int f = pet_favorite(s_pet, GUSTO_COMIDA, odio), c = pet_favorite(s_pet, GUSTO_CLIMA, odio), z = pet_favorite(s_pet, GUSTO_GESTO, odio);
        if (f >= 0) iconos[n++] = COMIDA[f];
        if (c >= 0) iconos[n++] = CLIMA[c];
        static const art_icono_t GESTO_ICO[GESTO_COUNT] = {ICO_MANO, ICO_MANO, ICO_MAREO, ICO_ABRAZO};
        if (z >= 0) iconos[n++] = GESTO_ICO[z % GESTO_COUNT];
        nn[odio] = n;
    }
}


/* --- accesos rápidos: deslizar el dedo hacia abajo desde el borde de arriba --- */
static lv_obj_t *s_tira;
static int s_tira_y0;
static bool s_tira_ok;
static void panel_rapido(void);
static void tira_cb(lv_event_t *e)
{
    lv_event_code_t c = lv_event_get_code(e);
    lv_indev_t *in = lv_indev_active();
    lv_point_t pt = {0, 0};
    if (in) lv_indev_get_point(in, &pt);
    if (c == LV_EVENT_PRESSED) { s_tira_y0 = pt.y; s_tira_ok = true; return; }
    if (c == LV_EVENT_PRESSING && s_tira_ok && pt.y - s_tira_y0 > 50) {
        s_tira_ok = false;
        if (in) lv_indev_wait_release(in);
        if (s_panel == PANEL_NINGUNO && s_pet && s_pet->edad_dueno && s_juego == JG_NADA) panel_rapido();
    }
}
static void rapido_cb(lv_event_t *e)
{
    int q = (int)(intptr_t)lv_event_get_user_data(e);
    if (q == 0) { cerrar_panel(); entrar_pecera(false); }
    else if (q == 1) panel_alarmas();
    else if (q == 3) { /* arrullo: no, lluvia, mar, grillos */
        if (s_disp_aj) s_disp_vista = *s_disp_aj;
        s_disp_vista.arrullo = s_disp_vista.arrullo ? 0 : 3;
        accion(UI_ACCION_ARRULLO, s_disp_vista.arrullo);
        panel_rapido();
    }
    else if (q == 2) { /* escuchar música (apagado de fábrica) */
        if (s_disp_aj) s_disp_vista = *s_disp_aj;
        s_disp_vista.musica = !s_disp_vista.musica;
        accion(UI_ACCION_MUSICA, s_disp_vista.musica);
        panel_rapido();
    }
}
static void panel_rapido(void)
{
    if (s_disp_aj) s_disp_vista = *s_disp_aj;
    abrir_tarjeta(PANEL_RAPIDO, 400, 400);
    bateria_dibujo(s_tarjeta, -3, 26, true);
    barra(s_tarjeta, 84, ICO_LUNA, ICO_SOL, PET_BRILLO_MIN, 100, s_disp_vista.brillo, pantalla_cb, 0);
    barra(s_tarjeta, 140, ICO_NO, ICO_PARLANTE, 0, 100, s_disp_vista.volumen, sonido_cb, 3); /* volumen general */
    static const art_icono_t I[4] = {ICO_PECERA, ICO_DESPERTADOR, ICO_NOTA, ICO_LUNA};
    static const char *const T[4] = {"PECERA", "ALARMAS", "MÚSICA", "ARRULLO"};
    /* la alarma más próxima (o apagado) */
    char alarma[16] = "APAGADO";
    if (s_pet) {
        uint32_t mejor = 0; int mi = -1;
        for (int k = 0; k < PET_ALARMAS; k++) {
            if (!s_pet->alarma[k].usada || !s_pet->alarma[k].activa) continue;
            const uint32_t n = pet_alarm_next(s_pet, k, s_ultimo_t);
            if (n && (!mejor || n < mejor)) { mejor = n; mi = k; }
        }
        if (mi >= 0) snprintf(alarma, sizeof alarma, "%02d:%02d", s_pet->alarma[mi].hora, s_pet->alarma[mi].minuto);
    }
    const char *estado[4] = {NULL, alarma, s_disp_vista.musica ? "SONANDO" : "APAGADA", s_disp_vista.arrullo ? "GRILLOS" : "APAGADO"};
    const bool activo[4] = {false, strcmp(alarma, "APAGADO") != 0, s_disp_vista.musica != 0, s_disp_vista.arrullo != 0};
    for (int i = 0; i < 4; i++) {
        lv_obj_t *b = boton(s_tarjeta, 164, 76, rapido_cb, i);
        lv_obj_align(b, LV_ALIGN_TOP_MID, (i % 2 ? 1 : -1) * 86, 194 + (i / 2) * 86);
        lv_obj_set_style_radius(b, 18, 0);
        if (activo[i]) lv_obj_set_style_bg_color(b, lv_color_hex(0x3E7F4F), 0);
        lv_obj_t *ic = icono(b, I[i], 3);
        lv_obj_align(ic, LV_ALIGN_LEFT_MID, 10, 0);
        if (s_nino) continue;
        lv_obj_t *t = texto(b, T[i], C_BLANCO, 2, 106);
        lv_obj_align(t, LV_ALIGN_LEFT_MID, 54, estado[i] ? -12 : 0);
        if (estado[i]) { lv_obj_t *s = texto(b, estado[i], activo[i] ? 0xBFF5C8 : 0xC8C4E0, 2, 0); lv_obj_align(s, LV_ALIGN_LEFT_MID, 54, 12); } /* (v97) del mismo tamaño que el resto: chiquito no se leía */
    }
}

/* las habilidades: el dibujo de cada una (especie x cría/adulta) */
static const art_icono_t ICO_HABILIDAD[8][2] = {
    {ICO_MACETA, ICO_FLOR}, {ICO_RAYO, ICO_ESTRELLA}, {ICO_DUCHA, ICO_SOL}, {ICO_PEZ, ICO_LLUVIA},
    {ICO_COFRE, ICO_SEMILLA}, {ICO_ESTRELLA, ICO_GLOBO_AIRE}, {ICO_LUPA, ICO_MARIPOSA}, {ICO_ZORRO, ICO_ESCONDIDAS}};
static void habilidad_cb(lv_event_t *e)
{
    int k = (int)(intptr_t)lv_event_get_user_data(e);
    if (!s_pet || !pet_ability_ready(s_pet, k, s_ultimo_t)) {
        char t[64] = "";
        if (s_pet) { snprintf(t, sizeof t, "YA USASTE %s HOY. VUELVE MAÑANA", pet_ability_name(s_pet->especie, k)); mayusculas(t); }
        cerrar_panel(); globo(ICO_LUNA, s_nino ? NULL : t, 3000); return;
    }
    if (s_pet->dormida) { cerrar_panel(); globo(ICO_LUNA, s_nino ? NULL : "ESTÁ DURMIENDO", 3000); return; }
    int bl = pet_ability_block(s_pet, k, s_ultimo_t);
    if (bl) { cerrar_panel(); globo(ICO_LUNA, s_nino ? NULL : pet_ability_block_text(bl), 3500); return; }
    cerrar_panel();
    accion(UI_ACCION_HABILIDAD, k);
}
/* Más: un vidrio redondo con tres pestañas arriba (ella, la bolsa y los recuerdos, los ajustes) */
/* "Ella", contada desde abajo: los botones, encima las habilidades, encima los corazones, encima las barras */
#define ELLA_BOTONES_Y 316
#define ELLA_HAB_Y (ELLA_BOTONES_Y - 50)
#define ELLA_CORAZON_Y (ELLA_HAB_Y - 26)
#define ELLA_BARRAS_Y (ELLA_CORAZON_Y - 96)

static int s_ella_pag; /* en "Ella": 0 todo sobre ella, 1 sus barras (se desliza hacia un lado) */
/* (v96) su figura en la primera página: chica y debajo del texto (antes, a escala 6 y a media altura, tapaba lo que decía) */
#define ELLA_FIGURA_H 64
static int ella_figura_base(void) { return s_pet && pet_ability_has(s_pet, 0) ? ELLA_HAB_Y - 4 : ELLA_BOTONES_Y - 8; } /* sus pies */
static int ella_texto_fin(void) /* hasta dónde llega el texto de la primera página */
{
    if (!s_pet || s_pet->etapa == STAGE_HUEVO || s_ella_pag != 0) return ELLA_CORAZON_Y - 10;
    const int f = ella_figura_base() - ELLA_FIGURA_H - 6;
    return f < ELLA_CORAZON_Y - 10 ? f : ELLA_CORAZON_Y - 10;
}
static void mas_tab_cb(lv_event_t *e) { const int k = (int)(intptr_t)lv_event_get_user_data(e); s_mas_tab = k == s_mas_tab ? 0 : k; s_ella_pag = 0; s_pag_mundo = false; panel_mas(); } /* tocar de nuevo la pestaña abierta vuelve a "Ella" */
static int s_ella_x0, s_ella_y0;
static void panel_mas_async(void *u) { (void)u; if (s_panel == PANEL_MAS) panel_mas(); }
static void barra_cierra(void);
static void cerrar_panel_async(void *u) { (void)u; if (s_panel == PANEL_MAS) cerrar_panel(); }
/* el orden de las páginas al deslizar a la izquierda: desde el mundo, la afinidad (2) y las barras (1); desde el botón Más, ella (0), la afinidad (2) y las barras (1) */
static int ella_orden(int *o)
{
    if (s_pag_mundo) { if (s_nino) { o[0] = 1; return 1; } o[0] = 2; o[1] = 1; return 2; }
    if (s_nino) { o[0] = 0; o[1] = 1; return 2; }
    o[0] = 0; o[1] = 2; o[2] = 1; return 3;
}
static void ella_pag_cb(lv_event_t *e) /* deslizar hacia un lado: cambia de página (a la izquierda, las barras) */
{
    lv_indev_t *in = lv_indev_active();
    if (!in || s_mas_tab != 0 || s_panel != PANEL_MAS) return;
    lv_point_t p;
    lv_indev_get_point(in, &p);
    if (lv_event_get_code(e) == LV_EVENT_PRESSED) { s_ella_x0 = p.x; s_ella_y0 = p.y; return; }
    const int dx = p.x - s_ella_x0, dy = p.y - s_ella_y0;
    if (abs(dx) < 50 || abs(dx) < abs(dy) * 3 / 2) return;
    int ord[3];
    const int n = ella_orden(ord);
    int idx = 0;
    for (int k = 0; k < n; k++) if (ord[k] == s_ella_pag) idx = k;
    const int ni = idx + (dx < 0 ? 1 : -1);
    if (ni < 0) { barra_cierra(); lv_async_call(cerrar_panel_async, NULL); return; } /* ya en la primera: otro deslizar a la derecha la cierra (no abre el libro) */
    if (ni >= n) return; /* en la última: a la izquierda no pasa nada */
    const int nueva = ord[ni];
    s_ella_pag = nueva;
    barra_cierra();
    lv_async_call(panel_mas_async, NULL); /* (reconstruir el panel DENTRO del evento de uno de sus propios objetos dejaba al programa usando memoria ya liberada) */
}
static lv_obj_t *s_barra_aviso, *s_barra_capa;
static void barra_borrada_cb(lv_event_t *e) /* si el panel se reconstruye, los punteros no quedan colgando (reusar esa memoria rompía el programa) */
{
    lv_obj_t *o = lv_event_get_target(e);
    if (o == s_barra_aviso) s_barra_aviso = NULL;
    if (o == s_barra_capa) s_barra_capa = NULL;
}
static void barra_cierra(void)
{
    if (s_barra_aviso && lv_obj_is_valid(s_barra_aviso)) lv_obj_delete(s_barra_aviso);
    if (s_barra_capa && lv_obj_is_valid(s_barra_capa)) lv_obj_delete(s_barra_capa);
    s_barra_aviso = s_barra_capa = NULL;
}
static void barra_cierra_cb(lv_event_t *e) { (void)e; barra_cierra(); }
static void barra_info_cb(lv_event_t *e) /* tocar una barrita: dice a qué se refiere y cómo está */
{
    static const char *const T[NEED_COUNT] = {
        "COMIDA: SI ESTÁ BAJA, TIENE HAMBRE. DALE DE COMER",
        "SUEÑO: SI ESTÁ BAJA, ESTÁ CANSAD{O}. DÉJAL{O} DORMIR",
        "LIMPIEZA: SI ESTÁ BAJA, ESTÁ SUCI{O}. BÁÑAL{O}",
        "JUEGO: SI ESTÁ BAJA, SE ABURRE. JUEGA CON {ELLA}",
        "CARIÑO: SI ESTÁ BAJA, SE SIENTE SOL{O}. ACARÍCIAL{O}",
        "SALUD: SI ESTÁ BAJA, ESTÁ ENFERM{O}. CUÍDAL{O}",
        "CURIOSIDAD: SI ESTÁ BAJA, QUIERE NOVEDADES. PASEA",
        "SEGURIDAD: SI ESTÁ BAJA, TIENE MIEDO. DALE UNA CASITA Y CALMA"};
    int i = (int)(intptr_t)lv_event_get_user_data(e);
    if (i < 0 || i >= NEED_COUNT || !s_pet) return;
    barra_cierra();
    char t[200];
    snprintf(t, sizeof t, "%s", gtxt(T[i]));
    if (s_nino) return;
    /* un cartelito sobre las barras; tocarlo, o tocar fuera de él, lo cierra */
    lv_obj_t *capa = lv_obj_create(s_tarjeta);
    lv_obj_remove_style_all(capa);
    lv_obj_set_size(capa, lv_obj_get_width(s_tarjeta), lv_obj_get_height(s_tarjeta));
    lv_obj_set_pos(capa, 0, 0);
    lv_obj_add_flag(capa, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_FLOATING);
    lv_obj_add_event_cb(capa, barra_cierra_cb, LV_EVENT_CLICKED, NULL);
    s_barra_capa = capa;
    lv_obj_add_event_cb(capa, barra_borrada_cb, LV_EVENT_DELETE, NULL);
    lv_obj_t *c = lv_obj_create(s_tarjeta);
    lv_obj_remove_style_all(c);
    lv_obj_set_size(c, 310, 118);
    lv_obj_align(c, LV_ALIGN_TOP_MID, 0, 168);
    lv_obj_set_style_bg_color(c, lv_color_hex(0x161C4A), 0); /* el cartelito, con el estilo de los menús */
    lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(c, 18, 0);
    lv_obj_set_style_border_width(c, 2, 0);
    lv_obj_set_style_border_color(c, lv_color_hex(0xD0B969), 0);
    lv_obj_add_flag(c, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(c, barra_cierra_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *ic = icono(c, ICO_PLATO, 3);
    static const art_icono_t ICN[NEED_COUNT] = {ICO_PLATO, ICO_LUNA, ICO_DUCHA, ICO_PELOTA, ICO_CORAZON, ICO_TERMOMETRO, ICO_LUPA, ICO_CASITA};
    lv_image_set_src(ic, art_icono(ICN[i], 3));
    lv_obj_align(ic, LV_ALIGN_TOP_LEFT, 12, 12);
    lv_obj_t *tx = texto(c, t, C_TINTA, 2, 230);
    lv_obj_align(tx, LV_ALIGN_TOP_LEFT, 64, 12);
    lv_obj_add_event_cb(c, barra_borrada_cb, LV_EVENT_DELETE, NULL);
    s_barra_aviso = c;
}
static void ella_pag_toque_cb(lv_event_t *e) { (void)e; if (s_nino) return; int o[3]; const int n = ella_orden(o); int idx = 0; for (int q = 0; q < n; q++) if (o[q] == s_ella_pag) idx = q; s_ella_pag = o[(idx + 1) % n]; barra_cierra(); lv_async_call(panel_mas_async, NULL); }
static void mas_boton(int q, art_icono_t ico, const char *txt, int x, int y, int w, int h)
{
    lv_obj_t *bt = boton(s_tarjeta, w, h, mas_cb, q);
    lv_obj_align(bt, LV_ALIGN_TOP_MID, x, y);
    if (h >= 80) lv_obj_set_style_radius(bt, 18, 0);
    lv_obj_t *ic = icono(bt, ico, h >= 80 ? 4 : (h >= 64 ? 5 : (h >= 54 ? 4 : 3)));
    lv_obj_align(ic, LV_ALIGN_CENTER, 0, txt && !s_nino && h >= 80 ? -12 : 0);
    if (txt && !s_nino && h >= 80) { lv_obj_t *t = texto(bt, txt, C_BLANCO, 2, w - 8); lv_obj_align(t, LV_ALIGN_BOTTOM_MID, 0, -8); }
    if (q == 1) { /* la hierba: cuántas quedan, en puntitos */
        for (int d = 0; d < s_pet->inventario[ITEM_HIERBA] && d < 4; d++) {
            lv_obj_t *pt = caja(bt);
            lv_obj_set_size(pt, 4, 4);
            lv_obj_set_style_radius(pt, 2, 0);
            lv_obj_set_style_bg_color(pt, lv_color_hex(0x8FE08A), 0);
            lv_obj_set_style_bg_opa(pt, LV_OPA_COVER, 0);
            lv_obj_align(pt, LV_ALIGN_BOTTOM_MID, (d - 1) * 6 - 3, -4);
        }
    }
}
static void puntos_pagina(void)
{
    if (s_nino) return;
    int o[3];
    const int n = ella_orden(o);
    lv_obj_t *pg = caja(s_tarjeta);
    lv_obj_set_size(pg, 20 * n + 12, 28);
    lv_obj_set_style_bg_opa(pg, 0, 0);
    lv_obj_add_flag(pg, LV_OBJ_FLAG_CLICKABLE);
    if (s_ella_pag == 2) lv_obj_align(pg, LV_ALIGN_TOP_MID, 100, 20); else lv_obj_align(pg, LV_ALIGN_TOP_MID, 112, 72);
    lv_obj_add_event_cb(pg, ella_pag_toque_cb, LV_EVENT_CLICKED, NULL);
    for (int q = 0; q < n; q++) {
        lv_obj_t *d = caja(pg);
        lv_obj_set_size(d, 10, 10);
        lv_obj_set_style_radius(d, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(d, lv_color_hex(o[q] == s_ella_pag ? C_VIOLETA : 0xB8AEC8), 0);
        lv_obj_set_style_bg_opa(d, LV_OPA_COVER, 0);
        lv_obj_align(d, LV_ALIGN_CENTER, (int)((q - (n - 1) / 2.0f) * 14), 0);
        lv_obj_remove_flag(d, LV_OBJ_FLAG_CLICKABLE);
    }
}

/* tercera página (solo deslizando): cuánto te quiere, qué le gusta y qué teme, cómo se siente; sin nombre grande, libro, engranaje ni botones (solo la X) */
static void pagina_afinidad(void)
{
    char tt[80];
    snprintf(tt, sizeof tt, "AFINIDAD CON %s", s_pet->nombre);
    lv_obj_t *ti = texto(s_tarjeta, tt, 0xFFD5E0, 2, 300);
    lv_obj_align(ti, LV_ALIGN_TOP_MID, 0, 66);
    lv_obj_t *bar = lv_bar_create(s_tarjeta);
    lv_obj_set_size(bar, 346, 32);
    lv_obj_align(bar, LV_ALIGN_TOP_MID, 0, 90);
    lv_obj_set_style_pad_all(bar, 2, 0);
    lv_bar_set_range(bar, 0, 100);
    int v = (int)s_pet->vinculo; if (v < 0) v = 0; if (v > 100) v = 100;
    lv_bar_set_value(bar, v, LV_ANIM_OFF);
    lv_obj_set_style_radius(bar, 16, 0);
    lv_obj_set_style_radius(bar, 14, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0xE6DCC6), 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(bar, 2, 0);
    lv_obj_set_style_border_color(bar, lv_color_hex(C_TINTA), 0);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0xE8465E), LV_PART_INDICATOR);
    lv_obj_set_style_bg_grad_color(bar, lv_color_hex(0xE6DCC6), 0);
    { const bool sp = s_portal; s_portal = false;
      lv_obj_t *nm = texto(bar, pet_bond_name(pet_bond_level(s_pet)), 0xFFFFFF, 2, 0);
      s_portal = sp; lv_obj_center(nm);
      lv_obj_t *co = icono(bar, ICO_CORAZON, 2); lv_obj_align_to(co, nm, LV_ALIGN_OUT_LEFT_MID, -6, 0); }
    /* lo que le gusta (verde) y lo que teme (rojo): tocar un ícono dice qué es */
    int ico[2][5], nn[2];
    gustos_calcular(ico, nn);
    for (int g = 0; g < 2; g++) {
        const int cx = g ? 316 : 106; /* el centro de cada columna: el título y los íconos quedan alineados debajo de él */
        lv_obj_t *rot = texto_c(s_tarjeta, g ? "LE TEME" : "LE GUSTA", g ? 0xFF9F9F : 0x9BE3A0, 2, 150);
        lv_obj_update_layout(rot); lv_obj_set_pos(rot, cx - lv_obj_get_width(rot) / 2, 140); /* centrado por su ancho real, no por el pedido */
        if (!nn[g]) { lv_obj_t *na = texto_c(s_tarjeta, "AÚN NO LO SÉ", 0xCFE6D8, 2, 190); lv_obj_update_layout(na); lv_obj_set_pos(na, cx - lv_obj_get_width(na) / 2, 176); }
        const int cuantos = nn[g] < 3 ? nn[g] : 3;
        for (int i = 0; i < cuantos; i++) { /* el objeto del ícono es más grande que lo que se ve (se dibuja reducido al centro): se centra por su centro */
            lv_obj_t *gi = gusto_icono(s_tarjeta, (art_icono_t)ico[g][i], g, 0, 0);
            lv_obj_update_layout(gi);
            lv_obj_set_pos(gi, cx - cuantos * 46 / 2 + 23 + i * 46 - lv_obj_get_width(gi) / 2, 183 - lv_obj_get_height(gi) / 2);
        }
    }
    char sent[128];
    pet_feeling_short(s_pet, sent, sizeof sent);
    lv_obj_t *fe = texto(s_tarjeta, sent, 0xFFE9A8, 2, 330);
    lv_obj_align(fe, LV_ALIGN_TOP_MID, 0, 232);
    static const char *const ETAPA[] = {"HUEVO", "BEBÉ", "CRÍA", "ADULT{A}"};
    char sub[80];
    snprintf(sub, sizeof sub, "%s %s, %d DÍAS", s_pet->nombre, ETAPA[s_pet->etapa % 4], (int)pet_age_days(s_pet, s_ultimo_t));
    lv_obj_t *su = texto(s_tarjeta, sub, 0xCFE6D8, 2, 320);
    lv_obj_align(su, LV_ALIGN_TOP_MID, 0, 322);
    puntos_pagina();
}

static void panel_mas(void)
{
    if (s_panel != PANEL_MAS && !s_pag_mundo) s_ella_pag = 0;
    abrir_tarjeta(PANEL_MAS, 420, 420);
    if (!s_pet) return;
    lv_obj_add_flag(s_tarjeta, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(s_tarjeta, LV_OBJ_FLAG_SCROLLABLE);
    static const art_icono_t TI[3] = {ICO_CORAZON, ICO_LIBRO, ICO_AJUSTES};
    const bool solo_aj = s_mas_tab == 2; /* en ajustes: sin pestañas, nombre ni texto; solo la batería y las opciones */
    for (int i = 2; i < 3 && !solo_aj && !(s_mas_tab == 0 && s_ella_pag >= 1 && !s_nino); i++) { /* (solo el engranaje: los recuerdos viven en el cofre) */ /* pestañas: dos botoncitos redondos (Ella es la página de inicio) */
        lv_obj_t *b = boton(s_tarjeta, 52, 52, mas_tab_cb, i);
        lv_obj_set_style_radius(b, LV_RADIUS_CIRCLE, 0);
        lv_obj_align(b, LV_ALIGN_TOP_MID, 0, 24);
        if (i == s_mas_tab) { lv_obj_set_style_bg_color(b, lv_color_hex(C_VIOLETA), 0); lv_obj_set_style_border_color(b, lv_color_hex(0xFFE27A), 0); }
        lv_obj_t *ic = icono(b, TI[i], 3);
        lv_obj_center(ic);
    }
    emo_t a, b;
    int n = pet_top_emotions(s_pet, &a, &b);
    /* arriba, siempre: cómo está */
    int y = 78;
    if (solo_aj) {
        y = 56;
    } else if (!s_nino && s_mas_tab == 0 && s_ella_pag == 2) {
        y = 60; /* tercera página: sin nombre grande (lo dibuja pagina_afinidad) */
    } else if (s_nino && s_mas_tab == 0 && s_ella_pag == 1) {
        y = 70; /* los chicos: en la página de barras, solo las barras */
    } else if (s_nino) {
        int ia = icono_emocion(a), ca = icono_causa(pet_emotion_cause(s_pet, a));
        lv_obj_t *e1 = icono(s_tarjeta, n ? (art_icono_t)ia : ICO_CARA_FELIZ, s_mas_tab == 0 ? 5 : 3);
        lv_obj_align(e1, LV_ALIGN_TOP_MID, ca >= 0 ? -40 : 0, y);
        if (ca >= 0 && n) { lv_obj_t *e2 = icono(s_tarjeta, (art_icono_t)ca, s_mas_tab == 0 ? 5 : 3); lv_obj_align(e2, LV_ALIGN_TOP_MID, 40, y); }
        y += s_mas_tab == 0 ? 70 : 44;
    } else {
        lv_obj_t *t = texto(s_tarjeta, s_pet->nombre, C_TINTA, 3, 0);
        lv_obj_align(t, LV_ALIGN_TOP_MID, 0, y);
        if (s_mas_tab == 0 && s_ultimo_t && pet_nights_to_full_moon(s_ultimo_t) == 0) { /* esta noche hay luna llena: una lunita brillante */
            const lv_image_dsc_t *nd = lv_image_get_src(t);
            lv_obj_t *lu = icono(s_tarjeta, ICO_LUNA, 2);
            lv_obj_align(lu, LV_ALIGN_TOP_MID, (nd ? (int)nd->header.w / 2 : 40) + 18, y);
            lv_obj_set_style_shadow_color(lu, lv_color_hex(0xFFF2A0), 0);
            lv_obj_set_style_shadow_width(lu, 12, 0);
            lv_obj_set_style_shadow_opa(lu, LV_OPA_70, 0);
        }
        char sent[128];
        pet_feeling_short(s_pet, sent, sizeof sent);
        if (s_mas_tab == 0 && s_ella_pag == 1) {
            y = 112; /* las barras, en su propia página */
        } else if (s_mas_tab == 0) {
            /* en "Ella" todo tiene su lugar fijo, contado desde abajo: lo que dice, quién es y hacia dónde
             * va su magia caben en dos renglones; si hay más, se desliza con el dedo (nunca tapa los botones) */
            /* el texto llega justo hasta las barras (o los corazones con niños): nunca las tapa */
            lv_obj_t *ar = area_texto(s_tarjeta, 60, y + 30, 300, ella_texto_fin() - (y + 30));
            lv_obj_set_style_pad_row(ar, 2, 0);
            lv_obj_set_style_pad_bottom(ar, 0, 0);
            texto(ar, sent, C_VIOLETA, 2, 290);
            static const char *const ETAPA[] = {"HUEVO", "BEBÉ", "CRÍA", "ADULT{A}"};
            char sub[80];
            snprintf(sub, sizeof sub, "%s %s, %d DÍAS", nombre_especie(s_pet->especie), ETAPA[s_pet->etapa % 4], (int)pet_age_days(s_pet, s_ultimo_t));
            texto(ar, sub, C_GRIS, 2, 290);
            if (s_pet->guardiana) texto(ar, "{GUARDIANA} DEL MUNDO", 0x3E9A4A, 2, 290);
            if (s_pet->padre[0]) { char pa[48]; snprintf(pa, sizeof pa, "DE LA FAMILIA DE %s", s_pet->padre); texto(ar, pa, C_GRIS, 2, 290); }
            if (s_pet->etapa == STAGE_CRIA) { /* hacia dónde la está llevando lo que vive */
                static const char *const HACIA[8] = {"LAS HOJAS", "EL RAYO", "EL SOL", "EL AGUA", "LA TIERRA", "EL CIELO", "LA LUZ", "LAS SOMBRAS"};
                int lin = pet_lineage_lead(s_pet);
                if (lin != (s_pet->especie & 7)) {
                    char tt[64];
                    snprintf(tt, sizeof tt, "SU MAGIA SE INCLINA HACIA %s", HACIA[lin & 7]);
                    texto(ar, tt, C_VIOLETA, 2, 290);
                }
            }
            lv_obj_update_layout(ar);
            if (AVISO_DESLIZA && lv_obj_get_scroll_bottom(ar) > 2) lv_obj_set_scrollbar_mode(ar, LV_SCROLLBAR_MODE_ON);
            y = ELLA_CORAZON_Y;
        } else {
            lv_obj_t *fe = texto(s_tarjeta, s_mas_tab == 1 ? "LO QUE HAN VIVIDO JUNTOS" : sent, C_VIOLETA, 2, 300);
            lv_obj_align(fe, LV_ALIGN_TOP_MID, 0, y + 30);
            const lv_image_dsc_t *fd = lv_image_get_src(fe);
            y += 30 + (fd ? (int)fd->header.h : 20) + 10;
        }
    }
    if (s_mas_tab == 0 && s_ella_pag == 2 && !s_nino) {
        pagina_afinidad();
    } else if (s_mas_tab == 0) { /* ella: cómo está, cuánto te quiere, y lo que se hace con ella */
        if (s_ella_pag == 1) {
            static const art_icono_t ICO[NEED_COUNT] = {ICO_PLATO, ICO_LUNA, ICO_DUCHA, ICO_PELOTA, ICO_CORAZON, ICO_TERMOMETRO, ICO_LUPA, ICO_CASITA};
            static const char *const NOM[NEED_COUNT] = {"COMIDA", "SUEÑO", "LIMPIEZA", "JUEGO", "CARIÑO", "SALUD", "CURIOSIDAD", "SEGURIDAD"};
            static const uint32_t COL[NEED_COUNT] = {0xFFB020, 0x4AA8FF, 0x34D1BF, 0xFF6FB5, 0xFF5A5A, 0x5CC97A, 0xB48CFF, 0xC08A5A};
            if (s_nino) y = 84; /* los chicos: solo el dibujo y la barra, más grandes */
            for (int i = 0; i < NEED_COUNT; i++) {
                int x = i < 4 ? 44 : 214, yy = y + (i % 4) * (s_nino ? 58 : 50);
                lv_obj_t *ic = icono(s_tarjeta, ICO[i], 3);
                lv_obj_set_pos(ic, x, yy + (s_nino ? 12 : 9));
                static const char *const NOM_NINO[NEED_COUNT] = {"COMIDA", "SUEÑO", "BAÑO", "JUEGO", "CARIÑO", "SALUD", "PASEO", "CALMA"}; /* los chicos: nombres cortos */
                if (s_nino) { lv_obj_t *nm = texto(s_tarjeta, NOM_NINO[i], C_TINTA, 2, 0); lv_obj_set_pos(nm, x + 44, yy + 4); }
                lv_obj_t *bar = lv_bar_create(s_tarjeta);
                lv_obj_set_size(bar, 124, s_nino ? 18 : 28);
                lv_obj_set_pos(bar, x + 44, s_nino ? yy + 26 : yy + 9);
                lv_obj_set_style_pad_all(bar, 2, 0); /* el color queda dentro del borde negro */
                lv_bar_set_range(bar, 0, 100);
                lv_bar_set_value(bar, (int32_t)pet_need(s_pet, (need_t)i), LV_ANIM_OFF);
                lv_obj_set_style_radius(bar, 9, 0);
                lv_obj_set_style_radius(bar, 7, LV_PART_INDICATOR);
                lv_obj_set_style_bg_color(bar, lv_color_hex(0xE6DCC6), 0);
                lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
                lv_obj_set_style_border_width(bar, 2, 0);
                lv_obj_set_style_border_color(bar, lv_color_hex(C_TINTA), 0);
                lv_obj_set_style_bg_color(bar, lv_color_hex(COL[i]), LV_PART_INDICATOR);
                if (!s_nino) { /* el nombre va DENTRO de la barra (oscuro sobre el fondo claro, aunque el panel sea de noche) */
                    const bool sp = s_portal; s_portal = false;
                    lv_obj_t *nm = texto(bar, NOM[i], C_TINTA, 2, 0);
                    s_portal = sp;
                    lv_obj_center(nm);
                    lv_obj_remove_flag(nm, LV_OBJ_FLAG_CLICKABLE);
                }
                lv_obj_t *zona = lv_obj_create(s_tarjeta); /* toda la fila se puede tocar */
                lv_obj_remove_style_all(zona);
                lv_obj_set_size(zona, 168, 46);
                lv_obj_set_pos(zona, x - 2, yy);
                lv_obj_add_flag(zona, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_EVENT_BUBBLE);
                filtro_toque(zona);
                if (!s_nino) lv_obj_add_event_cb(zona, barra_info_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i); /* (los chicos: sin textos) */
            }
        }
        /* desde abajo, cada fila en su lugar fijo: botones, habilidades, corazones (nada se encima) */
        y = ELLA_CORAZON_Y;
        if (s_ella_pag == 0) {
        if (s_pet->etapa != STAGE_HUEVO) { /* en el centro, ella: con la cara de cómo se siente */
            static const art_expresion_t EXM[MOOD_COUNT] = {EXP_FELIZ, EXP_ABIERTOS, EXP_TRISTE, EXP_CERRADOS, EXP_TRISTE, EXP_ABURRIDO, EXP_TRISTE,
                                                            EXP_ENOJADO, EXP_MAREADO, EXP_FELIZ, EXP_CERRADOS, EXP_CURIOSO, EXP_MIEDO, EXP_ORGULLO, EXP_CELOS};
            const int mo = (int)pet_mood(s_pet);
            const art_expresion_t ex = s_pet->dormida ? EXP_CERRADOS : (s_pet->enferma ? EXP_ENFERMA : EXM[(mo >= 0 && mo < MOOD_COUNT) ? mo : 0]);
            lv_obj_t *yo = NULL;
            for (int esc = 6; esc >= 2; esc--) { /* la escala más grande que quepa en su lugar (una adulta grande baja a 3 o 4) */
                if (yo) lv_obj_delete(yo);
                yo = mini_criatura(s_tarjeta, 11, pet_look(s_pet), s_pet->etapa, s_pet->forma, esc, ex);
                if ((int)s_mini[11].dsc.header.h <= ELLA_FIGURA_H + 8) break;
            }
            lv_obj_remove_flag(yo, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_align(yo, LV_ALIGN_TOP_MID, 0, ella_figura_base() - (int)s_mini[11].dsc.header.h);
        }
        y = ELLA_HAB_Y;
        if (pet_ability_has(s_pet, 0)) { /* su magia: una habilidad de cría y otra de adulta, una vez al día cada una */
            int n = pet_ability_has(s_pet, 1) ? 2 : 1;
            for (int k = 0; k < n; k++) {
                int esp = k == 0 ? (s_pet->especie & 7) : pet_lineage_shape(s_pet);
                bool lista = pet_ability_ready(s_pet, k, s_ultimo_t);
                int w = s_nino ? 64 : 150;
                lv_obj_t *b = boton(s_tarjeta, w, 44, habilidad_cb, k);
                lv_obj_align(b, LV_ALIGN_TOP_MID, n == 1 ? 0 : (k * 2 - 1) * (w / 2 + 6), y);
                lv_obj_set_style_bg_color(b, lv_color_hex(lista ? 0x4A3A78 : C_BOTON), 0);
                lv_obj_t *ic = icono(b, lista ? ICO_HABILIDAD[esp][k] : ICO_LUNA, 2);
                lv_obj_align(ic, s_nino ? LV_ALIGN_CENTER : LV_ALIGN_LEFT_MID, s_nino ? 0 : 12, 0);
                if (!lista) lv_obj_set_style_opa(b, LV_OPA_60, 0);
                if (!s_nino) {
                    char nm[24];
                    snprintf(nm, sizeof nm, "%s", pet_ability_name(esp, k));
                    lv_obj_t *t = texto(b, nm, 0xFFE27A, 2, 104);
                    lv_obj_align(t, LV_ALIGN_LEFT_MID, 44, 0);
                }
            }
        }
        }
        /* los puntitos de página (ella / sus barras): tocarlos o deslizar cambia */
        puntos_pagina();
        /* abajo: la luz, la jeringa (solo si está enferma), el libro mágico, la bolsa, el mapa y los amigos */
        art_icono_t bi[6];
        int bq[6], nb = 0;
        bi[nb] = ICO_BOMBILLA; bq[nb++] = 0;
        if (s_pet->enferma) { bi[nb] = ICO_JERINGA; bq[nb++] = 1; }
        bi[nb] = ICO_COFRE; bq[nb++] = 15; /* el cofre: accesorios, tesoros, enciclopedia, fotos, cámara y cartas */
        int zonas_abiertas = 0;
        for (int z = 0; z < ESC_COUNT; z++) zonas_abiertas += pet_scene_open(s_pet, (escena_t)z) ? 1 : 0;
        /* (el libro de historias no está en este menú: se abre solo deslizando a la derecha) */ /* las historias de los personajes, apenas aparece la primera */
        if (zonas_abiertas >= 2) { bi[nb] = ICO_MAPA; bq[nb++] = 7; } /* el mapa, cuando hay adónde ir */
        bi[nb] = ICO_AMIGOS; bq[nb++] = 4;
        bool zona_nueva = false;
        for (int z = 0; z < ESC_COUNT; z++) zona_nueva |= pet_scene_open(s_pet, (escena_t)z) && !pet_scene_seen(s_pet, (escena_t)z);
        const int bw = nb > 5 ? 54 : (nb == 5 ? 62 : 68); /* botones grandes: más fáciles de acertar */
        int gap = nb > 5 ? 4 : 6;
        int x = -(nb * bw + (nb - 1) * gap) / 2; /* borde izquierdo */
        for (int i = 0; i < nb; i++) {
            mas_boton(bq[i], bi[i], NULL, x + bw / 2, ELLA_BOTONES_Y, bw, bw);
            if ((bq[i] == 15 && s_pet->bolsa_nueva) || (bq[i] == 7 && zona_nueva)) {
                lv_obj_t *st = icono(s_tarjeta, ICO_ESTRELLA, 2);
                lv_obj_align(st, LV_ALIGN_TOP_MID, x + bw - 6, ELLA_BOTONES_Y);
            }
            x += bw + gap;
        }
    } else if (s_mas_tab == 1) { /* los recuerdos */
        /* lo que han vivido juntos: cada cosa con su nombre y una línea que dice qué guarda */
        static const art_icono_t BI[6] = {ICO_LIBRO, ICO_FOTO, ICO_CAMARA, ICO_COFRE, ICO_PERCHA, ICO_CARTA};
        static const char *const BT[6] = {"DIARIO", "FOTOS", "CÁMARA", "TESOROS", "RINCÓN", "CARTAS"};
        static const char *const BD[6] = {"SU DÍA A DÍA", "MOMENTOS GUARDADOS", "SÁCALE UNA FOTO",
                                          "LO QUE COLECCIONA", "ROPA Y JUGUETES", "LO QUE LE ESCRIBIERON"};
        static const int BQ[6] = {6, 11, 12, 13, 3, 14};
        /* cada cosa aparece cuando hay algo que guardar en ella */
        bool hay_tes = false, hay_rincon = false, conoce_obj = false;
        for (int c = 0; c < COL_COUNT; c++) hay_tes |= s_pet->coleccion[c] != 0;
        for (int i = 0; i < PET_OBJETOS && !conoce_obj; i++) conoce_obj = s_pet->bolsa[i] || pet_object_known(s_pet, i);
        hay_tes |= conoce_obj; /* (los animales y criaturas que conoció también van en Tesoros) */
        for (int r = 0; r < ROPA_COUNT; r++) hay_rincon |= pet_has_clothes(s_pet, (ropa_t)r);
        for (int j = 0; j < JUG_COUNT; j++) hay_rincon |= pet_has_toy(s_pet, (juguete_t)j);
        const bool vis[6] = {pet_diary_count(s_pet) > 0, pet_album_count(s_pet) > 0, true, hay_tes, hay_rincon, !s_nino && pet_letter_count(s_pet) > 0};
        int idx[6], nr = 0;
        for (int i = 0; i < 6; i++) if (vis[i]) idx[nr++] = i;
        int y0 = y < 140 ? 140 : y;
        if (s_nino) { /* los chicos: dibujos grandes */
            for (int k = 0; k < nr && k < 5; k++) {
                int i = idx[k], fila = k / 3, en_fila = (nr < 5 ? nr : 5) - fila * 3 < 3 ? (nr < 5 ? nr : 5) - fila * 3 : 3, col = k % 3;
                mas_boton(BQ[i], BI[i], NULL, (int)((col - (en_fila - 1) / 2.0f) * 104), y0 + fila * 92, 98, 84);
            }
        } else {
            lv_obj_t *ar = area_texto(s_tarjeta, 50, y0 - 6, 320, 366 - y0);
            lv_obj_set_style_pad_row(ar, 6, 0);
            for (int k = 0; k < nr; k++) {
                int i = idx[k];
                lv_obj_t *b = boton(ar, 300, 64, mas_cb, BQ[i]);
                lv_obj_set_style_radius(b, 18, 0);
                lv_obj_add_flag(b, LV_OBJ_FLAG_SCROLL_CHAIN_VER);
                lv_obj_t *ic = icono(b, BI[i], 3);
                lv_obj_align(ic, LV_ALIGN_LEFT_MID, 10, 0);
                lv_obj_t *t = texto(b, BT[i], 0xFFE27A, 2, 220);
                lv_obj_align(t, LV_ALIGN_TOP_LEFT, 56, 12);
                lv_obj_t *d = texto(b, BD[i], C_BLANCO, 2, 236);
                lv_obj_align(d, LV_ALIGN_TOP_LEFT, 56, 34);
            }
            area_lista(ar);
        }
    } else { /* ajustes */
        bateria_dibujo(s_tarjeta, 0, 30, true);
        static const art_icono_t AI[6] = {ICO_PARLANTE, ICO_BOMBILLA, ICO_RELOJ, ICO_AMIGOS, ICO_PERSONA, ICO_AJUSTES};
        static const char *const TXT[6] = {"SONIDO", "PANTALLA", "HORA", "PERFILES", "MODO", "MÁS"};
        const int y0 = 86; /* con el espacio que ganamos: botones más grandes */
        for (int i = 0; i < 6; i++) {
            lv_obj_t *bt = boton(s_tarjeta, 110, 112, ajustes_cb, i);
            lv_obj_align(bt, LV_ALIGN_TOP_MID, (i % 3 - 1) * 116, y0 + (i / 3) * 118);
            lv_obj_set_style_radius(bt, 20, 0);
            lv_obj_t *ic = icono(bt, AI[i], 5);
            lv_obj_align(ic, LV_ALIGN_CENTER, 0, s_nino ? 0 : -14);
            if (!s_nino) { lv_obj_t *tt = texto(bt, TXT[i], C_BLANCO, 2, 100); lv_obj_align(tt, LV_ALIGN_BOTTOM_MID, 0, -8); }
        }
        { /* (1.0.09) abajo: la versión que tiene; al tocarla, la pantalla de actualizar */
            lv_obj_t *bv = boton(s_tarjeta, 236, 44, ajustes_cb, 6);
            lv_obj_align(bv, LV_ALIGN_TOP_MID, 0, y0 + 2 * 118 + 4);
            lv_obj_set_style_radius(bv, 16, 0);
            lv_obj_t *tv = texto(bv, "VERSIÓN " FW_VERSION, 0xFFE27A, 2, 220);
            lv_obj_center(tv);
        }
    }
}

/* ======================================================================
 * Barra de botones y toques
 * ==================================================================== */

static lv_obj_t *s_dock[4];

/* --- la esponja: al tocar Bañar aparece; se frota sobre ella hasta que queda limpia --- */
static lv_obj_t *s_esp_capa, *s_esponja;
static bool s_esponja_on;
static int s_esp_frote, s_esp_burbuja;
static uint32_t s_esp_ultimo;
static lv_point_t s_esp_pt;
static void esponja_fin(void) { s_esponja_on = false; ocultar(s_esp_capa, true); ocultar(s_esponja, true); }
static void esp_cb(lv_event_t *e)
{
    lv_event_code_t c = lv_event_get_code(e);
    if (!s_esponja_on) return;
    lv_indev_t *in = lv_indev_active();
    lv_point_t pt = {0, 0};
    if (in) lv_indev_get_point(in, &pt);
    s_esp_ultimo = s_ms;
    if (c == LV_EVENT_PRESSED) { s_esp_pt = pt; }
    if (c == LV_EVENT_PRESSED || c == LV_EVENT_PRESSING) {
        lv_obj_set_pos(s_esponja, pt.x - 24, pt.y - 30);
        int d = abs(pt.x - s_esp_pt.x) + abs(pt.y - s_esp_pt.y);
        s_esp_pt = pt;
        int mx = (int)s_mascota.x, my = (int)s_mascota.y - lado(&s_mascota) / 2;
        if (abs(pt.x - mx) < lado(&s_mascota) / 2 + 20 && abs(pt.y - my) < lado(&s_mascota) / 2 + 20) { /* sobre ella */
            s_esp_frote += d;
            s_esp_burbuja += d;
            if (s_esp_burbuja > 90) { s_esp_burbuja = 0; flotar(ICO_BURBUJA, NULL, 0, pt.x + (int)(azar() % 40) - 20, pt.y - 10, 0); }
            if (s_esp_frote > 900) { esponja_fin(); if (in) lv_indev_wait_release(in); accion(ACT_BANAR, 0); return; }
        }
    }
    if (c == LV_EVENT_CLICKED && s_esp_frote < 60) { /* un toque lejos de ella: la guarda */
        int mx = (int)s_mascota.x;
        if (abs(pt.x - mx) > 90) esponja_fin();
    }
}
static void esponja_empezar(void)
{
    if (!s_esp_capa) {
        lv_obj_t *scr = lv_screen_active();
        s_esp_capa = caja(scr);
        lv_obj_set_size(s_esp_capa, W, W);
        lv_obj_add_flag(s_esp_capa, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(s_esp_capa, esp_cb, LV_EVENT_PRESSED, NULL);
        lv_obj_add_event_cb(s_esp_capa, esp_cb, LV_EVENT_PRESSING, NULL);
        lv_obj_add_event_cb(s_esp_capa, esp_cb, LV_EVENT_CLICKED, NULL);
        s_esponja = lv_image_create(scr);
        lv_image_set_src(s_esponja, art_icono(ICO_ESPONJA, 4));
    }
    s_esponja_on = true;
    s_esp_frote = 0;
    s_esp_ultimo = s_ms;
    ocultar(s_esp_capa, false);
    ocultar(s_esponja, false);
    lv_obj_move_foreground(s_esp_capa);
    lv_obj_move_foreground(s_esponja);
    lv_obj_set_pos(s_esponja, (int)s_mascota.x + 40, (int)s_mascota.y - 60);
    globo(ICO_ESPONJA, "FRÓTAME CON LA ESPONJA", 2500);
}
static void animar_esponja(void)
{
    if (!s_esponja_on) return;
    if (s_ms - s_esp_ultimo > 10000) { esponja_fin(); return; } /* se olvidó: se guarda sola */
    if (lv_indev_get_state(lv_indev_get_next(NULL)) != LV_INDEV_STATE_PRESSED) /* se mece esperando */
        lv_obj_set_style_translate_y(s_esponja, (int)(4 * sinf((float)s_ms / 200.0f)), 0);
    else lv_obj_set_style_translate_y(s_esponja, 0, 0);
}

static void dock_cb(lv_event_t *e)
{
    int i = (int)(intptr_t)lv_event_get_user_data(e);
    switch (i) {
    case 0: picnic_abrir(); break; /* se abre al tiro la pantalla del picnic; la comida se elige ahí abajo */
    case 1: panel_jugar(); break;
    case 2: esponja_empezar(); break;
    default: s_mas_tab = 0; s_pag_mundo = false; panel_mas(); break;
    }
}

/* ======================================================================
 * Objetos mágicos: la Bolsa, el cielo, el acompañante y lo que se pone
 * ==================================================================== */
#define N_MAGIA 18
static lv_obj_t *s_magia[N_MAGIA], *s_velo, *s_compa_o, *s_compa_ojo[2], *s_cabeza_obj, *s_obj_deco[PET_OBJ_PUESTOS];
#define PET_OBJ_CARA_PCT 28  /* a qué altura de su dibujo van las gafas (% desde arriba) */
#define PET_OBJ_PECHO_PCT 50 /* y los broches y medallas */
static lv_obj_t *s_vuela, *s_bolsa_aviso;
static float s_mg_x[N_MAGIA], s_mg_y[N_MAGIA], s_mg_v[N_MAGIA];
static int s_cielo_ui = -1, s_compa_ui = -1;
static float s_compa_x = CX, s_compa_y = 200;
static int s_obj_sel = -1;
static uint32_t s_vuela_hasta;
/* dónde quedan los objetos que decoran el prado (los pies del objeto) */
static const int16_t POS_OBJ_DECO[PET_OBJ_PUESTOS][2] = {{92, 286}, {378, 300}, {170, 240}, {392, 248}};

static int obj_de(int a, int b) { return (a & 0x7F) | (b & 1) << 7; }

static void crear_magia(lv_obj_t *scr)
{
    s_velo = caja(s_mundo); /* oscurece el cielo (eclipse, constelaciones) */
    lv_obj_set_size(s_velo, W, 260);
    lv_obj_set_style_bg_color(s_velo, lv_color_hex(0x0A0820), 0);
    lv_obj_set_style_bg_grad_color(s_velo, lv_color_hex(0x0A0820), 0);
    lv_obj_set_style_bg_grad_dir(s_velo, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_bg_main_opa(s_velo, 200, 0);
    lv_obj_set_style_bg_grad_opa(s_velo, 0, 0);
    lv_obj_set_style_bg_opa(s_velo, LV_OPA_COVER, 0);
    lv_obj_move_to_index(s_velo, 0);
    ocultar(s_velo, true);
    for (int i = 0; i < N_MAGIA; i++) {
        s_magia[i] = caja(s_mundo);
        lv_obj_set_style_bg_opa(s_magia[i], LV_OPA_COVER, 0);
        lv_obj_move_to_index(s_magia[i], 1);
        ocultar(s_magia[i], true);
    }
    for (int i = 0; i < PET_OBJ_PUESTOS; i++) {
        s_obj_deco[i] = lv_image_create(s_mundo);
        lv_obj_move_to_index(s_obj_deco[i], N_MAGIA + 1);
        ocultar(s_obj_deco[i], true);
    }
    s_cabeza_obj = lv_image_create(s_mundo);
    ocultar(s_cabeza_obj, true);
    s_compa_o = caja(scr);
    lv_obj_set_size(s_compa_o, 16, 16);
    lv_obj_set_style_radius(s_compa_o, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(s_compa_o, LV_OPA_COVER, 0);
    lv_obj_set_style_shadow_width(s_compa_o, 22, 0);
    lv_obj_set_style_shadow_spread(s_compa_o, 3, 0);
    for (int k = 0; k < 2; k++) {
        s_compa_ojo[k] = caja(s_compa_o);
        lv_obj_set_size(s_compa_ojo[k], 3, 4);
        lv_obj_set_pos(s_compa_ojo[k], 4 + k * 6, 5);
        lv_obj_set_style_bg_color(s_compa_ojo[k], lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_bg_opa(s_compa_ojo[k], LV_OPA_COVER, 0);
    }
    ocultar(s_compa_o, true);
    s_bolsa_aviso = lv_image_create(s_dock[3]); /* algo nuevo en la Bolsa */
    lv_image_set_src(s_bolsa_aviso, art_icono(ICO_ESTRELLA, 2));
    lv_obj_align(s_bolsa_aviso, LV_ALIGN_TOP_RIGHT, -3, 3);
    ocultar(s_bolsa_aviso, true);
    s_vuela = lv_image_create(scr);
    ocultar(s_vuela, true);
}

/* un objeto que vuela de un lugar a otro (a la Bolsa cuando lo encuentra, a ella cuando lo usas) */
static void anim_x(void *o, int32_t v) { lv_obj_set_x((lv_obj_t *)o, v); }
static void anim_y_pos(void *o, int32_t v) { lv_obj_set_y((lv_obj_t *)o, v); }
static void anim_escala(void *o, int32_t v) { lv_image_set_scale((lv_obj_t *)o, (uint32_t)v); }
static void anim_rot(void *o, int32_t v) { lv_image_set_rotation((lv_obj_t *)o, v); }
static void volar(int id, int x0, int y0, int x1, int y1, uint32_t retraso, bool achicar)
{
    lv_image_set_src(s_vuela, art_icono(ICO_OBJETO(id), 3));
    lv_obj_move_to_index(s_vuela, lv_obj_get_index(s_globo)); /* por debajo del globo de diálogo */
    lv_image_set_pivot(s_vuela, 18, 18);
    lv_obj_set_pos(s_vuela, x0 - 18, y0 - 18);
    lv_image_set_scale(s_vuela, 256);
    lv_image_set_rotation(s_vuela, 0);
    ocultar(s_vuela, false);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, s_vuela);
    lv_anim_set_duration(&a, 900);
    lv_anim_set_delay(&a, retraso);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_set_exec_cb(&a, anim_x);
    lv_anim_set_values(&a, x0 - 18, x1 - 18);
    lv_anim_start(&a);
    lv_anim_set_exec_cb(&a, anim_y_pos);
    lv_anim_set_path_cb(&a, lv_anim_path_overshoot);
    lv_anim_set_values(&a, y0 - 18, y1 - 18);
    lv_anim_start(&a);
    lv_anim_set_exec_cb(&a, anim_escala);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in);
    lv_anim_set_values(&a, 256, achicar ? 90 : 160);
    lv_anim_start(&a);
    s_vuela_hasta = s_ms + retraso + 950;
}

static void cielo_preparar(int c)
{
    s_cielo_ui = c;
    ocultar(s_velo, !(c == 2 || c == 8)); /* eclipse y constelaciones oscurecen el cielo */
    static const uint32_t COL[11][2] = {{0, 0}, {0x7AF0C8, 0xB48CFF}, {0xFFE27A, 0xFFFFFF}, {0x7AF0FF, 0xFFFFFF}, {0xFFFFFF, 0xFFF2A0},
                                       {0xFF6B6B, 0x7AF0C8}, {0xFFB6F0, 0xFFE27A}, {0xFFF4A0, 0xC8FF7A}, {0xFFFFFF, 0xBFE6FF},
                                       {0xBFFFE0, 0xFFE8A8}, {0xFFF4A0, 0xC8FF7A}};
    for (int i = 0; i < N_MAGIA; i++) {
        bool usa = c > 0;
        if (c == 2) usa = i < 1; /* eclipse: un anillo de luz */
        if (c == 9) usa = i < 7;  /* motitas del ambiente */
        if (c == 10) usa = i < 10; /* luciérnagas de las noches tibias */
        ocultar(s_magia[i], !usa);
        if (!usa) continue;
        lv_obj_t *o = s_magia[i];
        int w = 4, h = 4;
        lv_obj_set_style_radius(o, 2, 0);
        lv_obj_set_style_shadow_width(o, 0, 0);
        lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(o, 0, 0);
        switch (c) {
        case 1: w = 70 + (int)(azar01() * 60); h = 10; lv_obj_set_style_radius(o, 5, 0); break; /* aurora: bandas */
        case 2: w = h = 74; lv_obj_set_style_radius(o, LV_RADIUS_CIRCLE, 0); lv_obj_set_style_bg_color(o, lv_color_hex(0x0A0820), 0);
            lv_obj_set_style_border_width(o, 5, 0); lv_obj_set_style_border_color(o, lv_color_hex(0xFFE27A), 0);
            lv_obj_set_style_shadow_width(o, 30, 0); lv_obj_set_style_shadow_color(o, lv_color_hex(0xFFD45C), 0); break;
        case 4: w = 26; h = 4; lv_obj_set_style_shadow_width(o, 12, 0); lv_obj_set_style_shadow_color(o, lv_color_hex(0xFFF2A0), 0); break; /* fugaces */
        case 10: w = h = 5 + (i & 1); lv_obj_set_style_radius(o, 3, 0); lv_obj_set_style_shadow_width(o, 14, 0);
            lv_obj_set_style_shadow_color(o, lv_color_hex(COL[10][i & 1]), 0); break; /* luciérnagas del mundo */
        case 6: w = h = 7; lv_obj_set_style_radius(o, 3, 0); lv_obj_set_style_shadow_width(o, 12, 0); /* (v96) la purpurina: más grande y con brillo (antes 4 px: casi no se veía) */
            lv_obj_set_style_shadow_color(o, lv_color_hex(COL[6][i & 1]), 0); break;
        case 7: case 8: case 5: w = h = c == 5 ? 9 : 7; lv_obj_set_style_radius(o, 3, 0); lv_obj_set_style_shadow_width(o, 12, 0);
            lv_obj_set_style_shadow_color(o, lv_color_hex(COL[c][i & 1]), 0); break; /* luciérnagas, estrellas, fuegos */
        case 9: w = h = 3 + (i & 1); lv_obj_set_style_radius(o, 2, 0); lv_obj_set_style_shadow_width(o, 8, 0);
            lv_obj_set_style_shadow_color(o, lv_color_hex(COL[9][i & 1]), 0); break;
        default: w = h = 4 + (i % 3); break;
        }
        lv_obj_set_size(o, w, h);
        lv_obj_set_style_bg_color(o, lv_color_hex(c == 2 ? 0x0A0820 : COL[c][i & 1]), 0);
        s_mg_x[i] = 60 + azar01() * 340;
        s_mg_y[i] = c == 1 ? 40 + (float)(i % 6) * 14 : (c == 9 ? 120 + azar01() * 220 : c == 10 ? 150 + azar01() * 170 : 50 + azar01() * 180);
        s_mg_v[i] = azar01() * 6.28f;
        if (c == 2) { s_mg_x[i] = CX - 37; s_mg_y[i] = 52; }
        if (c == 8) { /* constelaciones: tres dibujitos fijos */
            static const int8_t CONS[N_MAGIA][2] = {{0, 0}, {14, -8}, {28, -4}, {40, 6}, {52, 2}, {0, 30}, {-10, 44}, {8, 52},
                                                    {22, 44}, {80, 20}, {96, 10}, {110, 24}, {98, 38}, {84, 44}, {-40, 10}, {-54, 22}, {-36, 34}, {-60, 0}};
            s_mg_x[i] = 170 + CONS[i][0] * 1.5f;
            s_mg_y[i] = 70 + CONS[i][1] * 1.3f;
        }
        lv_obj_set_pos(o, (int)s_mg_x[i], (int)s_mg_y[i]);
    }
}

/* el árbol hueco del prado: ahí lleva las ofrendas */
#define HUECO_X 68
#define HUECO_Y 177
static lv_obj_t *s_hueco, *s_ofr_luz[3];
static bool s_ofrenda_llego;
static void animar_magia(void)
{
    /* tras el final secreto, el primer día de primavera el árbol viejo florece amarillo (el sueño del Cuidador) */
    if (s_pet && s_escena == ESC_PRADO && !s_interior && s_panel == PANEL_NINGUNO && pet_final_seen(s_pet, PET_FINAL_SECRETO) &&
        pet_yellow_flowers_day(s_pet, s_ultimo_t) && s_ms % 800 < TICK_MS)
        flotar(ICO_FLOR, NULL, 0, HUECO_X - 44 + (int)(azar() % 120), 70 + (int)(azar() % 100), 0);
    if (!s_hueco) { /* el árbol viejo (grande, con raíces) ya está pintado en el fondo del prado; aquí solo lo que se deja en las raíces */
        s_hueco = caja(s_mundo);
        lv_obj_set_size(s_hueco, 1, 1);
        static const int16_t RX[3][2] = {{33, 210}, {103, 210}, {68, 215}};
        for (int i = 0; i < 3; i++) { /* lo que dejó ahí, brillando en las raíces hasta que el bosque lo agradece */
            s_ofr_luz[i] = caja(s_mundo);
            lv_obj_set_size(s_ofr_luz[i], 8, 8);
            lv_obj_set_pos(s_ofr_luz[i], RX[i][0] - 4, RX[i][1] - 4);
            lv_obj_set_style_radius(s_ofr_luz[i], LV_RADIUS_CIRCLE, 0);
            lv_obj_set_style_bg_color(s_ofr_luz[i], lv_color_hex(0xFFE27A), 0);
            lv_obj_set_style_bg_opa(s_ofr_luz[i], LV_OPA_COVER, 0);
            lv_obj_set_style_shadow_width(s_ofr_luz[i], 14, 0);
            lv_obj_set_style_shadow_color(s_ofr_luz[i], lv_color_hex(0xFFE27A), 0);
            ocultar(s_ofr_luz[i], true);
        }
    }
    {
        const bool aqui = s_escena == ESC_PRADO && !s_interior;
        const bool espera = s_pet && s_pet->dia_ofrenda != 0;
        for (int i = 0; i < 3; i++) {
            ocultar(s_ofr_luz[i], !(aqui && espera));
            lv_obj_set_style_opa(s_ofr_luz[i], (lv_opa_t)(150 + 100 * sinf((float)s_ms / 500.0f + (float)i * 2.0f)), 0);
        }
    }
    const pet_t *p = s_pet;
    if (!p) return;
    bool afuera = !s_interior && s_escena != ESC_CUEVA;
    int c = p->cielo && !s_interior && s_panel == PANEL_NINGUNO ? p->cielo : 0; /* (v96) también en la cueva; en la casa espera a que salgan */
    /* sin magia del dueño: motitas de luz que suben en el bosque y en las noches (el mundo también es mágico) */
    /* y en las noches que no son frías ni de lluvia, luciérnagas en el prado, el bosque y el lago (más que nada con el calor) */
    if (!c && afuera && s_panel == PANEL_NINGUNO && s_fondo_periodo == PERIODO_NOCHE && (s_escena == ESC_PRADO || s_escena == ESC_BOSQUE || s_escena == ESC_LAGO) &&
        pet_season(p, s_ultimo_t) != ESTACION_INVIERNO && p->clima != CLIMA_LLUVIA && p->clima != CLIMA_TORMENTA && p->clima != CLIMA_NIEVE) c = 10;
    if (!c && afuera && s_panel == PANEL_NINGUNO && (s_escena == ESC_BOSQUE || s_fondo_periodo == PERIODO_NOCHE || pet_final_seen(p, PET_FINAL_SECRETO))) c = 9; /* tras el final secreto, también de día */
    constelacion_actualizar(afuera && s_panel == PANEL_NINGUNO && s_fondo_periodo == PERIODO_NOCHE && pet_final_seen(p, PET_FINAL_SECRETO) &&
                            p->clima != CLIMA_LLUVIA && p->clima != CLIMA_TORMENTA && p->clima != CLIMA_NIEVE && p->clima != CLIMA_NUBLADO); /* (v39) la cuchara de Tilo, las noches despejadas */
    if (c != s_cielo_ui) cielo_preparar(c);
    float t = (float)s_ms;
    if (c) for (int i = 0; i < N_MAGIA; i++) {
        lv_obj_t *o = s_magia[i];
        if (lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN)) continue;
        float *x = &s_mg_x[i], *y = &s_mg_y[i], f = s_mg_v[i];
        lv_opa_t opa = LV_OPA_COVER;
        switch (c) {
        case 1: *x += sinf(t / 900.0f + i) * 0.8f; opa = (lv_opa_t)(90 + 80 * sinf(t / 700.0f + i * 0.7f)); break;
        case 2: opa = LV_OPA_COVER; break;
        case 4: *x += 7 + (i % 3); *y += 3; if (*x > W || *y > 300) { *x = -30 - azar01() * 200; *y = 20 + azar01() * 140; } break;
        case 5: { /* fuegos: chispas que salen de un punto y caen */
            float fase = fmodf(t / 1400.0f + (i / 6) * 0.33f, 1.0f), ang = (float)(i % 6) * 1.047f + (i / 6);
            float cx0 = 120 + (i / 6) * 110, cy0 = 90 + (i / 6) * 20;
            *x = cx0 + cosf(ang) * 50 * fase; *y = cy0 + sinf(ang) * 50 * fase + 30 * fase * fase;
            opa = (lv_opa_t)(fase < 0.6f ? 255 : 255 * (1 - fase) / 0.4f);
            break;
        }
        case 6: *y += 1.2f; *x += sinf(t / 400.0f + i) * 0.8f; if (*y > 330) *y = 30; opa = (lv_opa_t)((s_ms / 120 + i) % 3 ? 255 : 80); break;
        case 9: *y -= 0.35f; *x += sinf(t / 900.0f + f) * 0.5f; if (*y < 60) { *y = 340; *x = 70 + azar01() * 320; }
            opa = (lv_opa_t)(50 + 70 * fabsf(sinf(t / 700.0f + f))); break;
        case 10: /* vagan despacito y se prenden y apagan cada una a su ritmo */
            *x += sinf(t / 1500.0f + f * 3) * 0.55f; *y += cosf(t / 1900.0f + f * 2) * 0.4f;
            if (*x < 40) *x = 40;
            if (*x > 410) *x = 410;
            if (*y < 120) *y = 120;
            if (*y > 330) *y = 330;
            { float b = sinf(t / (900.0f + 140.0f * (float)(i % 5)) + f * 5); opa = (lv_opa_t)(b > 0.2f ? 90 + 165 * b : 12); }
            break;
        case 7: *x += sinf(t / 600.0f + f) * 1.2f; *y += cosf(t / 800.0f + f * 2) * 0.8f; opa = (lv_opa_t)(120 + 120 * sinf(t / 300.0f + f)); break;
        default: opa = (lv_opa_t)(110 + 140 * fabsf(sinf(t / 500.0f + f))); break; /* chispas, constelaciones */
        }
        lv_obj_set_pos(o, (int)*x, (int)*y);
        lv_obj_set_style_opa(o, opa, 0);
    }

    /* el acompañante: sigue tu dedo; si no, revolotea cerca de ella */
    int cp = p->compa && s_panel == PANEL_NINGUNO && !s_interior ? p->compa : 0;
    if (cp != s_compa_ui) {
        s_compa_ui = cp;
        static const uint32_t CC[5] = {0, 0x5ED0FF, 0xFFE27A, 0xFF9FC8, 0x2A2238};
        ocultar(s_compa_o, !cp);
        if (cp) {
            lv_obj_set_style_bg_color(s_compa_o, lv_color_hex(CC[cp % 5]), 0);
            lv_obj_set_style_shadow_color(s_compa_o, lv_color_hex(cp == 4 ? 0x6A4FA0 : CC[cp % 5]), 0);
            for (int k = 0; k < 2; k++) ocultar(s_compa_ojo[k], cp != 4 && cp != 1);
            lv_obj_set_style_bg_color(s_compa_ojo[0], lv_color_hex(cp == 4 ? 0xFFFFFF : 0x1A3A5A), 0);
            lv_obj_set_style_bg_color(s_compa_ojo[1], lv_color_hex(cp == 4 ? 0xFFFFFF : 0x1A3A5A), 0);
        }
    }
    if (cp) {
        float tx = s_mascota.x + 46 * sinf(t / 1100.0f), ty = cabeza_y(&s_mascota) - 20 + 12 * sinf(t / 700.0f);
        lv_indev_t *in = lv_indev_get_next(NULL);
        if (in && lv_indev_get_state(in) == LV_INDEV_STATE_PRESSED) {
            lv_point_t pt;
            lv_indev_get_point(in, &pt);
            tx = (float)pt.x;
            ty = (float)pt.y - 34; /* un poco arriba del dedo, para verlo */
        }
        s_compa_x += (tx - s_compa_x) * 0.12f;
        s_compa_y += (ty - s_compa_y) * 0.12f;
        lv_obj_set_pos(s_compa_o, (int)s_compa_x - 8, (int)(s_compa_y - 8 + 3 * sinf(t / 180.0f)));
        lv_obj_set_style_opa(s_compa_o, (lv_opa_t)(200 + 55 * sinf(t / 250.0f)), 0);
    }

    /* lo que lleva en la cabeza */
    int cab = p->obj_cabeza && s_mascota.visible && p->etapa != STAGE_HUEVO ? p->obj_cabeza - 1 : -1;
    bool hoja = cab < 0 && s_mascota.visible && p->etapa != STAGE_HUEVO && s_ultimo_t < p->hoja_hasta && !p->dormida; /* la hoja gigante de sombrero */
    ocultar(s_cabeza_obj, cab < 0 && !hoja);
    if (cab >= 0 || hoja) {
        poner_src(s_cabeza_obj, hoja ? art_icono(p->hoja_nenufar ? ICO_NENUFAR : ICO_HOJA, 3) : art_icono(ICO_OBJETO(cab), 2));
        int cx = lv_obj_get_x(s_mascota.img) + lv_obj_get_width(s_mascota.img) / 2;
        int cy = lv_obj_get_y(s_mascota.img) + s_mascota.pose.escala * 7;
        int dy = hoja ? 28 : 14;
        if (!hoja) { /* cada cosa va donde corresponde: gorros y plumas en la cabeza, las gafas en la cara, medallas y broches en el pecho */
            const objeto_info_t *oi = objeto_info(cab);
            const int al = lv_obj_get_height(s_mascota.img);
            if (oi && oi->familia == OFA_DISCO) dy = -al * PET_OBJ_CARA_PCT / 100 + 12;
            else if (oi && oi->familia == OFA_AMULETO) dy = -al * PET_OBJ_PECHO_PCT / 100 + 12;
        }
        lv_obj_set_pos(s_cabeza_obj, cx - (hoja ? 18 : 12), cy - dy);
        lv_obj_move_foreground(s_cabeza_obj);
    }

    /* los que decoran el prado */
    for (int i = 0; i < PET_OBJ_PUESTOS; i++) {
        int id = p->obj_puesto[i] - 1;
        bool ver = id >= 0 && s_escena == ESC_PRADO && !s_interior;
        ocultar(s_obj_deco[i], !ver);
        if (!ver) continue;
        poner_src(s_obj_deco[i], art_icono(ICO_OBJETO(id), 3));
        int brillo = id == 90 ? (int)(30 * sinf(t / 400.0f + i)) : 0; /* la casita de hadas titila */
        lv_obj_set_pos(s_obj_deco[i], POS_OBJ_DECO[i][0] - 18, POS_OBJ_DECO[i][1] - 34 + (brillo > 20 ? -1 : 0));
    }
    if ((int32_t)(s_ms - s_vuela_hasta) > 0 && !lv_obj_has_flag(s_vuela, LV_OBJ_FLAG_HIDDEN)) ocultar(s_vuela, true);
    ocultar(s_bolsa_aviso, !p->bolsa_nueva);
}

/* --- la Bolsa --- */
static void panel_bolsa(void);
static void panel_objeto(int id);
static void panel_historia(int c, int k);
#define HIST_FAM OCAT_COUNT /* en el Libro, una familia más: las historias que le han contado */
#define HIST_ID 1000
static const art_icono_t ICO_CRIATURA[CRI_COUNT];

static uint32_t firma_bolsa(const pet_t *p)
{
    uint32_t f = p->obj_cabeza;
    for (int i = 0; i < PET_OBJETOS; i++) f = f * 31u + p->bolsa[i] + (uint32_t)p->obj_dia[i] * 7u;
    for (int k = 0; k < PET_OBJ_PUESTOS; k++) f = f * 31u + p->obj_puesto[k];
    return f;
}

static void bolsa_cb(lv_event_t *e)
{
    int q = (int)(intptr_t)lv_event_get_user_data(e);
    if (q >= HIST_ID) { panel_historia((q - HIST_ID) / 5, (q - HIST_ID) % 5); return; } /* releer una historia */
    if (q <= -10) s_bolsa_pag = -10 - q; /* una familia */
    else if (q == -6) {
        if (s_bolsa_desde_cofre) { s_bolsa_desde_cofre = false; s_cofre_vista = 1; s_cofre_sub = -1; panel_cofre(); return; } /* volver a la cuadrícula de tesoros */
        s_mas_tab = 0; s_pag_mundo = false; panel_mas(); return; /* volver a "Ella" */
    }
    else if (q >= 0) {
        if (s_bolsa_libro && !pet_object_known(s_pet, q)) return;
        if (s_nino && !s_bolsa_libro && s_pet->bolsa[q]) { /* los más chicos: lo deja en el prado y ella va a usarlo */
            if (!pet_object_ready(s_pet, q, s_ultimo_t)) { cerrar_panel(); globo(ICO_LUNA, NULL, 2000); return; }
            lv_area_t a;
            lv_obj_get_coords(lv_event_get_target(e), &a);
            cerrar_panel();
            objeto_al_mundo(q, (a.x1 + a.x2) / 2, (a.y1 + a.y2) / 2, true);
            return;
        }
        panel_objeto(q);
        return;
    }
    panel_bolsa();
}

static void panel_bolsa(void)
{
    abrir_tarjeta(PANEL_BOLSA, 420, 420);
    if (!s_pet) return;
    s_obj_sel = -1;
    if (s_bolsa_libro < 0) s_bolsa_libro = 0;
    if (s_pet->bolsa_nueva && !s_bolsa_libro) accion(UI_ACCION_OBJETO, 4 << 8);
    /* lo de cada lado: la bolsa muestra lo que lleva; el libro, lo que conoce */
    int hay[OCAT_COUNT + 1] = {0};
    for (int i = 0; i < PET_OBJETOS; i++) {
        bool puesto = false;
        for (int k = 0; k < PET_OBJ_PUESTOS; k++) puesto |= s_pet->obj_puesto[k] == i + 1;
        bool entra = s_bolsa_libro ? pet_object_known(s_pet, i) : (s_pet->bolsa[i] || puesto);
        if (entra) hay[objeto_info(i)->cat]++;
    }
    /* siempre una sola familia a la vez; al abrir, la de lo último que trajo (o la primera que tenga algo) */
    if (s_bolsa_pag < 1 || s_bolsa_pag > OCAT_COUNT + 1) {
        int nv = s_pet->bolsa_nueva ? s_pet->bolsa_nueva - 1 : -1;
        s_bolsa_pag = 1;
        if (nv >= 0 && hay[objeto_info(nv)->cat]) s_bolsa_pag = 1 + objeto_info(nv)->cat;
        else for (int c = OCAT_COUNT; c >= 0; c--) if (hay[c]) s_bolsa_pag = 1 + c;
    }
    const int filtro = s_bolsa_pag;
    /* arriba: de qué lado estás */
    lv_obj_t *ti = icono(s_tarjeta, ICO_BOLSA, 3);
    lv_obj_align(ti, LV_ALIGN_TOP_MID, 30, 26);
    if (!s_nino) { /* con palabras: el título solo (el dibujo no cabe al lado) */
        ocultar(ti, true);
        const bool dorado = s_bolsa_libro && pet_final_seen(s_pet, PET_FINAL_SECRETO);
        lv_obj_t *t = texto(s_tarjeta, dorado ? "LA BIBLIOTECA" : (s_bolsa_libro ? "LIBRO MÁGICO" : "LA BOLSA"), dorado ? 0x8A5A00 : C_TINTA, 3, 0);
        lv_obj_align(t, LV_ALIGN_TOP_MID, 0, dorado ? 50 : 38);
        if (dorado) { /* ya está completo: tocar el título vuelve a abrir la última página */
            lv_obj_t *pz = boton(s_tarjeta, 180, 34, biblioteca_cb, 0);
            lv_obj_align(pz, LV_ALIGN_TOP_MID, 0, 44);
            lv_obj_set_style_bg_color(pz, lv_color_hex(0xFFE9A0), 0);
            lv_obj_set_style_border_width(pz, 3, 0);
            lv_obj_set_style_border_color(pz, lv_color_hex(0xD0A030), 0);
            lv_obj_set_style_radius(pz, 20, 0);
            lv_obj_t *es = icono(pz, ICO_ESTRELLA, 2);
            lv_obj_align(es, LV_ALIGN_RIGHT_MID, -6, 0);
            lv_obj_t *es2 = icono(pz, ICO_ESTRELLA, 2);
            lv_obj_align(es2, LV_ALIGN_LEFT_MID, 6, 0);
            lv_obj_move_foreground(t);
        }
    }
    volver(bolsa_cb, -6);
    /* las familias: se ve solo una (así no te pierdes) */
    static const art_icono_t FAM[OCAT_COUNT + 1] = {ICO_PIEDRA, ICO_FLOR, ICO_PLUMA, ICO_RELOJ, ICO_ESTRELLA, ICO_LIBRO};
    int nuevo_cat = s_pet->bolsa_nueva && !s_bolsa_libro ? objeto_info(s_pet->bolsa_nueva - 1)->cat : -1;
    int nfam = 0, ordfam[OCAT_COUNT + 1];
    for (int c = 0; c <= OCAT_COUNT; c++) if (hay[c]) ordfam[c] = nfam++; /* solo se ven las familias que ya tienen algo */
    for (int c = 0; c <= OCAT_COUNT; c++) {
        if (!hay[c] || nfam < 2) continue; /* con una sola familia no hacen falta pestañas */
        lv_obj_t *b = boton(s_tarjeta, 46, 46, bolsa_cb, -11 - c);
        lv_obj_align(b, LV_ALIGN_TOP_MID, (int)((ordfam[c] - (nfam - 1) / 2.0f) * 54), 80);
        if (c + 1 == filtro) { lv_obj_set_style_bg_color(b, lv_color_hex(C_VIOLETA), 0); lv_obj_set_style_border_color(b, lv_color_hex(0xFFE27A), 0); }
        lv_obj_t *ic = icono(b, FAM[c], 2);
        lv_obj_center(ic);
        if (c == nuevo_cat) { lv_obj_t *st = icono(b, ICO_ESTRELLA, 2); lv_obj_align(st, LV_ALIGN_TOP_RIGHT, -2, 2); }
    }
    if (!s_nino && filtro && nfam >= 1) {
        lv_obj_t *ct = texto(s_tarjeta, filtro - 1 == HIST_FAM ? "HISTORIAS" : objeto_cat_nombre(filtro - 1), C_VIOLETA, 2, 380);
        lv_obj_align(ct, LV_ALIGN_TOP_MID, 0, 128);
    }
    if (!hay[filtro - 1]) {
        lv_obj_t *v = icono(s_tarjeta, ICO_HOJA, 6);
        lv_obj_align(v, LV_ALIGN_CENTER, 0, 10);
        lv_obj_set_style_opa(v, LV_OPA_40, 0);
        if (!s_nino) {
            lv_obj_t *t = texto(s_tarjeta, s_bolsa_libro ? "EL LIBRO SE VA LLENANDO CON LO QUE ENCUENTRE." :
                                "TODAVÍA NO HAY NADA AQUÍ. EXPLORANDO, PESCANDO Y PASEANDO CONTIGO ENCUENTRA COSAS MÁGICAS.", C_GRIS, 2, 280);
            lv_obj_align(t, LV_ALIGN_CENTER, 0, 80);
        }
        return;
    }
    /* la lista se desliza con el dedo: sin páginas. Lo último que trajo va primero */
    lv_obj_t *ar = area_texto(s_tarjeta, 50, s_nino ? 136 : 152, 320, s_nino ? 234 : 218);
    lv_obj_set_style_pad_row(ar, 6, 0);
    if (s_nino) { lv_obj_set_flex_flow(ar, LV_FLEX_FLOW_ROW_WRAP); lv_obj_set_style_pad_column(ar, 8, 0); lv_obj_set_flex_align(ar, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START); }
    if (filtro - 1 == HIST_FAM) { /* las historias que le han contado: tocar una la vuelve a abrir */
        for (int c = 0; c < CRI_COUNT; c++) for (int k = 0; k < 5; k++) {
            if (!((s_pet->historias[c] >> k) & 1u)) continue;
            lv_obj_t *b;
            if (s_nino) {
                b = boton(ar, 70, 64, bolsa_cb, HIST_ID + c * 5 + k);
                lv_obj_set_style_radius(b, 18, 0);
                lv_obj_center(icono(b, ICO_CRIATURA[c], 3));
            } else {
                b = boton(ar, 300, 48, bolsa_cb, HIST_ID + c * 5 + k);
                lv_obj_set_style_radius(b, 18, 0);
                lv_obj_align(icono(b, ICO_CRIATURA[c], 2), LV_ALIGN_LEFT_MID, 8, 0);
                char t[96];
                snprintf(t, sizeof t, "%s", pet_story_title((magica_t)c, k));
                mayusculas(t);
                lv_obj_t *tx = texto(b, t, C_BLANCO, 2, 224);
                lv_obj_align(tx, LV_ALIGN_LEFT_MID, 54, 0);
                const lv_image_dsc_t *td = lv_image_get_src(tx);
                if (td && (int)td->header.h + 18 > 48) lv_obj_set_height(b, (int)td->header.h + 18);
            }
        }
        area_lista(ar);
        return;
    }
    int orden[PET_OBJETOS], n = 0, nuevo = s_pet->bolsa_nueva ? s_pet->bolsa_nueva - 1 : -1;
    if (nuevo >= 0) orden[n++] = nuevo;
    for (int i = 0; i < PET_OBJETOS; i++) if (i != nuevo) orden[n++] = i;
    int filas = 0;
    for (int k = 0; k < n && filas < 70; k++) {
        int id = orden[k];
        const objeto_info_t *o = objeto_info(id);
        bool puesto = false;
        for (int q = 0; q < PET_OBJ_PUESTOS; q++) puesto |= s_pet->obj_puesto[q] == id + 1;
        bool entra = s_bolsa_libro ? pet_object_known(s_pet, id) : (s_pet->bolsa[id] || puesto);
        if (!entra || o->cat != filtro - 1) continue;
        filas++;
        bool tiene = s_pet->bolsa[id] > 0, listo = pet_object_ready(s_pet, id, s_ultimo_t);
        lv_obj_t *b;
        if (s_nino) { /* los chicos: solo dibujos grandes */
            b = boton(ar, 70, 64, bolsa_cb, id);
            lv_obj_set_style_radius(b, 18, 0);
            lv_obj_t *ic = icono_obj(b, id, 4);
            lv_obj_center(ic);
            if (!listo || !tiene) lv_obj_set_style_opa(ic, LV_OPA_50, 0);
        } else { /* los grandes: el dibujo y el nombre, uno por fila */
            b = boton(ar, 300, 48, bolsa_cb, id);
            lv_obj_set_style_radius(b, 18, 0);
            lv_obj_t *ic = icono_obj(b, id, 3);
            lv_obj_align(ic, LV_ALIGN_LEFT_MID, 8, 0);
            if (!listo || !tiene) lv_obj_set_style_opa(ic, LV_OPA_50, 0);
            lv_obj_t *t = texto(b, o->nombre, tiene ? C_BLANCO : C_GRIS_CLARO, 2, 214);
            lv_obj_align(t, LV_ALIGN_LEFT_MID, 54, 0);
            const lv_image_dsc_t *td = lv_image_get_src(t); /* un nombre largo usa dos líneas: la fila crece */
            if (td && (int)td->header.h + 18 > 48) lv_obj_set_height(b, (int)td->header.h + 18);
        }
        if (id == nuevo) { lv_obj_t *st = icono(b, ICO_ESTRELLA, 2); lv_obj_align(st, LV_ALIGN_TOP_RIGHT, -6, 4); }
        if (id == pet_guarded_object(s_pet)) { lv_obj_t *cd = icono(b, ICO_CANDADO, 2); lv_obj_align(cd, LV_ALIGN_BOTTOM_RIGHT, -4, -2); } /* lo cuida el candado rúnico */
        if (pet_gold_has(s_pet, id)) { /* juego+: ya tiene la versión dorada */
            lv_obj_t *ic0 = lv_obj_get_child(b, 0);
            if (ic0) { lv_obj_set_style_image_recolor(ic0, lv_color_hex(0xFFC83C), 0); lv_obj_set_style_image_recolor_opa(ic0, LV_OPA_40, 0); }
            lv_obj_set_style_border_color(b, lv_color_hex(0xE8B83C), 0);
        }
        if (!s_bolsa_libro) { /* cuántos tiene: puntitos, sin números */
            int cuantos = s_pet->bolsa[id];
            for (int d = 0; d < cuantos && d < 5; d++) {
                lv_obj_t *pt = caja(b);
                lv_obj_set_size(pt, 5, 5);
                lv_obj_set_style_radius(pt, 3, 0);
                lv_obj_set_style_bg_color(pt, lv_color_hex(0xFFE27A), 0);
                lv_obj_set_style_bg_opa(pt, LV_OPA_COVER, 0);
                if (s_nino) lv_obj_align(pt, LV_ALIGN_BOTTOM_MID, (int)((d - (cuantos < 5 ? cuantos : 5) / 2.0f + 0.5f) * 8), -3);
                else lv_obj_align(pt, LV_ALIGN_RIGHT_MID, -12 - d * 8, 8);
            }
            if (puesto || s_pet->obj_cabeza == id + 1) { lv_obj_t *m = icono(b, puesto ? ICO_CASITA : ICO_GORRO, 2); lv_obj_align(m, s_nino ? LV_ALIGN_TOP_LEFT : LV_ALIGN_RIGHT_MID, s_nino ? 4 : -12, s_nino ? 4 : -8); }
            if (tiene && !listo) { lv_obj_t *m = icono(b, ICO_LUNA, 2); lv_obj_align(m, s_nino ? LV_ALIGN_TOP_LEFT : LV_ALIGN_RIGHT_MID, s_nino ? 4 : -30, s_nino ? 4 : -8); }
        }
    }
    area_lista(ar);
}

/* --- un objeto: para los niños, qué hace y dos botones; para los grandes, su leyenda y el gesto --- */
static art_icono_t icono_efecto(int ef)
{
    static const art_icono_t I[OEF_COUNT] = {ICO_LUPA, ICO_PLATO, ICO_CARA_FELIZ, ICO_RAYO, ICO_LUNA, ICO_CORAZON, ICO_JERINGA, ICO_GORRO,
                                             ICO_CASITA, ICO_MACETA, ICO_ESTRELLA, ICO_BOMBILLA, ICO_ESTRELLA_FUGAZ, ICO_PEZ, ICO_ZORRO, ICO_NUBE};
    return (unsigned)ef < OEF_COUNT ? I[ef] : ICO_PREGUNTA;
}
static lv_obj_t *s_obj_grande;

/* --- el objeto en el mundo: lo sacas de la bolsa, queda en el pasto, haces su gesto ahí y ella va, lo toca
 * y hace su magia (con niños va sola). Si no lo usas en un minuto, vuelve a la bolsa. --- */
static lv_obj_t *s_om;
static int s_om_id = -1, s_om_fase, s_om_x, s_om_y; /* fase: 1 espera tu gesto, 2 ella va, 3 lo toca */
static uint32_t s_om_hasta, s_om_aparece;
static int s_om_dist, s_om_x0;
static lv_point_t s_om_ult;
static uint32_t s_om_t0;
static void om_listo(void) /* hiciste el gesto: ella va */
{
    s_om_fase = 2;
    s_mic_objeto = -1;
    lv_obj_set_style_image_recolor_opa(s_om, 0, 0);
    if (s_pet && s_pet->dormida) { s_om_fase = 3; s_om_hasta = s_ms + 300; return; } /* durmiendo: pasa ahí mismo */
    int lado_x = s_om_x < CX ? 34 : -34;
    s_mascota.tx = (float)(s_om_x + lado_x);
    s_mascota.ty = (float)(s_om_y + 4);
    limitar(&s_mascota.tx, &s_mascota.ty);
    mascota_modo(M_VA, 7000);
    s_om_hasta = s_ms + 7000;
}
static void om_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    const objeto_info_t *o = objeto_info(s_om_id);
    if (!o || s_om_fase != 1) return;
    lv_indev_t *in = lv_indev_active();
    lv_point_t pt = {0, 0};
    if (in) lv_indev_get_point(in, &pt);
    if (code == LV_EVENT_PRESSED) { s_om_dist = 0; s_om_x0 = pt.x; s_om_ult = pt; s_om_t0 = s_ms; return; }
    if (code == LV_EVENT_PRESSING) {
        s_om_dist += abs(pt.x - s_om_ult.x) + abs(pt.y - s_om_ult.y);
        s_om_ult = pt;
        int brillo = o->gesto == OGE_FROTAR ? s_om_dist * 255 / 500 : (o->gesto == OGE_MANTENER ? (int)(s_ms - s_om_t0) * 255 / 1500 : 0);
        if (brillo > 255) brillo = 255;
        lv_obj_set_style_image_recolor(s_om, lv_color_hex(0xFFF6C8), 0);
        lv_obj_set_style_image_recolor_opa(s_om, (lv_opa_t)(brillo / 2), 0);
        if ((o->gesto == OGE_FROTAR && s_om_dist > 500) || (o->gesto == OGE_MANTENER && s_ms - s_om_t0 > 1500) ||
            (o->gesto == OGE_DESLIZAR && s_om_dist > 70)) { if (in) lv_indev_wait_release(in); om_listo(); } /* (v96) deslizar: basta con mover el dedo encima, en cualquier dirección */
        return;
    }
    if (code == LV_EVENT_RELEASED) {
        lv_obj_set_style_image_recolor_opa(s_om, 0, 0);
        int dx = pt.x - s_om_x0;
        if ((o->gesto == OGE_DESLIZAR || o->gesto == OGE_SOPLAR) && (abs(dx) > 50 || s_om_dist > 60) && s_ms - s_om_t0 < 2000) om_listo();
        else if (o->gesto == OGE_TOCAR && s_om_dist < 30) om_listo();
    }
}
static void objeto_al_mundo(int id, int x0, int y0, bool solo)
{
    if (!s_om) {
        s_om = lv_image_create(s_mundo);
        lv_obj_add_flag(s_om, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_ext_click_area(s_om, 22);
        lv_obj_remove_flag(s_om, LV_OBJ_FLAG_GESTURE_BUBBLE);
        lv_obj_add_event_cb(s_om, om_cb, LV_EVENT_PRESSED, NULL);
        lv_obj_add_event_cb(s_om, om_cb, LV_EVENT_PRESSING, NULL);
        lv_obj_add_event_cb(s_om, om_cb, LV_EVENT_RELEASED, NULL);
    }
    float x = s_mascota.x + (s_mascota.x < CX ? 80 : -80), y = s_mascota.y;
    limitar(&x, &y);
    s_om_x = (int)x; s_om_y = (int)y;
    s_om_id = id;
    lv_image_set_src(s_om, art_icono(ICO_OBJETO(id), 4));
    lv_obj_set_pos(s_om, s_om_x - 24, s_om_y - 44);
    ocultar(s_om, true);
    volar(id, x0, y0, s_om_x, s_om_y - 20, 0, false);
    s_om_aparece = s_ms + 950;
    s_om_fase = 1;
    s_om_hasta = s_ms + 60000;
    const objeto_info_t *o = objeto_info(id);
    if (solo) { om_listo(); s_om_hasta += 0; return; }
    s_mic_objeto = o && o->gesto == OGE_SOPLAR ? id : -1;
    static const char *const GESTO[OGE_COUNT] = {"TÓCALO", "FRÓTALO CON EL DEDO", "MANTÉN EL DEDO ENCIMA", "DESLIZA EL DEDO SOBRE ÉL", "SÓPLALE AL ORBE"};
    if (o) globo(ICO_OBJETO(id), GESTO[o->gesto % OGE_COUNT], 3500);
}
static void animar_obj_mundo(void)
{
    if (!s_om || s_om_id < 0) return;
    if (s_om_fase == 0) { ocultar(s_om, true); s_om_id = -1; return; }
    if ((int32_t)(s_ms - s_om_aparece) >= 0 && s_om_fase < 3) ocultar(s_om, false);
    lv_obj_set_style_translate_y(s_om, (int)(3 * sinf((float)s_ms / 300.0f)), 0); /* flota apenas: es mágico */
    if (s_om_fase == 1 && (int32_t)(s_ms - s_om_hasta) > 0) { s_om_fase = 0; s_mic_objeto = -1; return; } /* no lo usaste: vuelve a la bolsa */
    if (s_om_fase == 2) {
        float dx = s_mascota.x - s_mascota.tx, dy = s_mascota.y - s_mascota.ty;
        if ((dx * dx + dy * dy < 36 && s_ms > s_om_aparece) || (int32_t)(s_ms - s_om_hasta) > 0) { /* llegó: lo toca */
            s_om_fase = 3;
            s_om_hasta = s_ms + 700;
            s_mascota.pose.espejo = s_om_x < s_mascota.x;
            mascota_modo(M_SALTA, 700);
        }
    }
    if (s_om_fase == 3 && (int32_t)(s_ms - s_om_hasta) > 0) { /* (v37) primero dice lo que hace... */
        char dice[96];
        snprintf(dice, sizeof dice, "%s", pet_object_says(s_om_id, false));
        mayusculas(dice);
        globo(ICO_OBJETO(s_om_id), s_nino ? NULL : dice, 2600);
        s_om_fase = 4;
        s_om_hasta = s_ms + 1300;
    }
    if (s_om_fase == 4 && (int32_t)(s_ms - s_om_hasta) > 0) { /* ...y después, ¡su magia! */
        for (int i = 0; i < 4; i++) flotar(ICO_ESTRELLA, NULL, 0, s_om_x - 30 + i * 20, s_om_y - 30, (uint32_t)i * 90);
        accion(UI_ACCION_OBJETO, s_om_id);
        s_om_fase = 0;
    }
}
static void usar_objeto(int id)
{
    lv_area_t a;
    lv_obj_get_coords(s_obj_grande ? s_obj_grande : s_tarjeta, &a);
    int x0 = (a.x1 + a.x2) / 2, y0 = (a.y1 + a.y2) / 2;
    cerrar_panel();
    objeto_al_mundo(id, x0, y0, s_pet && s_pet->modo_nino != MODO_ADULTO);
}

static int s_zona_pend;
static bool s_via_activo;
static bool obj_bloqueado(void) /* dormida o de viaje: no se le puede dar nada ni hacer ofrendas */
{
    return !s_pet || s_pet->dormida || s_via_activo || s_cru_activo || s_zona_pend >= 0;
}
static lv_obj_t *s_obj_pop;
static void obj_pop_cb(lv_event_t *e) { (void)e; if (s_obj_pop && lv_obj_is_valid(s_obj_pop)) lv_obj_delete(s_obj_pop); s_obj_pop = NULL; }
static void panel_objeto(int id);
static int obj_vecino(int id, int dir) /* el objeto anterior (-1) o siguiente (+1) de su familia, en el orden de la lista; -1 si no hay */
{
    int orden[PET_OBJETOS], n = 0, nuevo = s_pet->bolsa_nueva ? s_pet->bolsa_nueva - 1 : -1;
    if (nuevo >= 0) orden[n++] = nuevo;
    for (int i = 0; i < PET_OBJETOS; i++) if (i != nuevo) orden[n++] = i;
    const int cat = objeto_info(id)->cat;
    int lista[PET_OBJETOS], m = 0, pos = -1;
    for (int k = 0; k < n; k++) {
        const int i = orden[k];
        bool puesto = false;
        for (int q = 0; q < PET_OBJ_PUESTOS; q++) puesto |= s_pet->obj_puesto[q] == i + 1;
        const bool entra = s_bolsa_libro ? pet_object_known(s_pet, i) : (s_pet->bolsa[i] || puesto);
        if (!entra || objeto_info(i)->cat != cat) continue;
        if (i == id) pos = m;
        lista[m++] = i;
    }
    if (pos < 0) return -1;
    pos += dir;
    return pos >= 0 && pos < m ? lista[pos] : -1;
}
static int s_obj_x0, s_obj_y0;
static void obj_desliza_cb(lv_event_t *e) /* deslizar hacia un lado: el siguiente o el anterior; en el borde se queda */
{
    lv_indev_t *in = lv_indev_active();
    if (!in || s_panel != PANEL_OBJETO || s_obj_sel < 0 || !s_pet) return;
    lv_point_t p;
    lv_indev_get_point(in, &p);
    if (lv_event_get_code(e) == LV_EVENT_PRESSED) { s_obj_x0 = p.x; s_obj_y0 = p.y; return; }
    const int dx = p.x - s_obj_x0, dy = p.y - s_obj_y0;
    if (abs(dx) < 45 || abs(dx) < abs(dy) * 3 / 2) return;
    const int v = obj_vecino(s_obj_sel, dx < 0 ? 1 : -1);
    if (v >= 0) panel_objeto(v);
}
static void obj_lupa_cb(lv_event_t *e) /* la lupa: te recuerda qué hace */
{
    (void)e;
    const objeto_info_t *o = s_obj_sel >= 0 ? objeto_info(s_obj_sel) : NULL;
    if (!o || !s_tarjeta) return;
    if (s_obj_pop && lv_obj_is_valid(s_obj_pop)) { obj_pop_cb(NULL); return; }
    lv_obj_t *c = caja(s_tarjeta);
    lv_obj_remove_style_all(c);
    lv_obj_set_size(c, 330, 170);
    lv_obj_align(c, LV_ALIGN_CENTER, 0, 20);
    lv_obj_set_style_bg_color(c, lv_color_hex(0xFFF6DC), 0);
    lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(c, 18, 0);
    lv_obj_set_style_border_width(c, 3, 0);
    lv_obj_set_style_border_color(c, lv_color_hex(C_TINTA), 0);
    lv_obj_add_flag(c, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(c, obj_pop_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *ic = icono(c, ICO_LUPA, 3);
    lv_obj_align(ic, LV_ALIGN_TOP_MID, 0, 8);
    lv_obj_t *tx = texto(c, pet_object_legend(s_pet, s_obj_sel), C_TINTA, 2, 290);
    lv_obj_align(tx, LV_ALIGN_TOP_MID, 0, 62);
    s_obj_pop = c;
}

static void obj_panel_cb(lv_event_t *e)
{
    int q = (int)(intptr_t)lv_event_get_user_data(e);
    int id = s_obj_sel;
    if (id < 0) return;
    if ((q == 0 || q == 1) && obj_bloqueado()) return;
    switch (q) {
    case 0: usar_objeto(id); break;                                        /* dárselo / usarlo */
    case 1: accion(UI_ACCION_OBJETO, id | 1 << 8); cerrar_panel(); break; /* ofrenda al árbol hueco */
    case 2: { /* guardarlo del prado */
        for (int k = 0; k < PET_OBJ_PUESTOS; k++) if (s_pet->obj_puesto[k] == id + 1) { accion(UI_ACCION_OBJETO, k | 2 << 8); break; }
        cerrar_panel();
        break;
    }
    case 3: accion(UI_ACCION_OBJETO, 3 << 8); cerrar_panel(); break;      /* sacárselo de la cabeza */
    default: if (s_obj_cofre_sub >= -1 && s_obj_cofre) { s_obj_cofre = false; s_cofre_vista = 1; s_cofre_sub = s_obj_cofre_sub; panel_cofre(); } else panel_bolsa(); break; /* volver */
    }
}


static void lect_opa_cb2(void *var, int32_t v);
static bool s_obj_ley; /* tocaste el objeto: se ve su leyenda (qué hace) */
static void obj_toca_cb(lv_event_t *e)
{
    (void)e;
    if (s_obj_sel < 0) return;
    s_obj_ley = !s_obj_ley;
    panel_objeto(s_obj_sel);
}
/* el misterio: un medallón con un signo de interrogación brillante (lo que aún no sabes que hace) */
static void pregunta_brillo_cb(void *var, int32_t v) { lv_obj_set_style_shadow_width((lv_obj_t *)var, v, 0); }
static lv_obj_t *pregunta_magica(lv_obj_t *padre, int d)
{
    lv_obj_t *m = lv_obj_create(padre);
    lv_obj_remove_style_all(m);
    lv_obj_set_size(m, d, d);
    lv_obj_set_style_radius(m, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(m, lv_color_hex(0x8A6AE0), 0);
    lv_obj_set_style_bg_grad_color(m, lv_color_hex(0x2E2A78), 0);
    lv_obj_set_style_bg_grad_dir(m, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_bg_opa(m, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(m, 3, 0);
    lv_obj_set_style_border_color(m, lv_color_hex(0xFFE27A), 0);
    lv_obj_set_style_shadow_width(m, 22, 0);
    lv_obj_set_style_shadow_color(m, lv_color_hex(0xC8A8FF), 0);
    lv_obj_set_style_shadow_opa(m, LV_OPA_60, 0);
    lv_obj_remove_flag(m, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(m, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *q = texto(m, "?", 0xFFE27A, d >= 80 ? 6 : 5, 0); /* el signo, grande y dorado */
    lv_obj_center(q);
    lv_anim_t a; /* el resplandor late despacio */
    lv_anim_init(&a);
    lv_anim_set_var(&a, m);
    lv_anim_set_values(&a, 10, 30);
    lv_anim_set_time(&a, 1100);
    lv_anim_set_playback_time(&a, 1100);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_exec_cb(&a, pregunta_brillo_cb);
    lv_anim_start(&a);
    return m;
}
static void panel_objeto(int id)
{
    const objeto_info_t *o = objeto_info(id);
    if (!o || !s_pet) return;
    if (s_panel != PANEL_OBJETO || s_obj_sel != id) s_obj_ley = false; /* un objeto nuevo: empieza sin la leyenda */
    abrir_tarjeta(PANEL_OBJETO, 368, 380);
    s_obj_sel = id;
    s_obj_grande = NULL;
    s_obj_pop = NULL;
    lv_obj_add_flag(s_tarjeta, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(s_tarjeta, LV_OBJ_FLAG_SCROLLABLE);
    const bool bloq = obj_bloqueado();
    const bool nino = s_pet->modo_nino != MODO_ADULTO, tiene = s_pet->bolsa[id] > 0;
    const bool listo = pet_object_ready(s_pet, id, s_ultimo_t), sabe = pet_object_discovered(s_pet, id);
    s_mic_objeto = tiene && listo && o->gesto == OGE_SOPLAR ? id : -1; /* espera que soples */
    bool puesto = false;
    for (int k = 0; k < PET_OBJ_PUESTOS; k++) puesto |= s_pet->obj_puesto[k] == id + 1;
    volver(obj_panel_cb, 9);
    char nom[48];
    int nom_h = 0;
    snprintf(nom, sizeof nom, "%s", o->nombre);
    if (!s_nino) {
        /* el nombre, arriba y al centro: si es largo, en dos renglones cada uno centrado */
        char l1[48], l2[48] = "";
        snprintf(l1, sizeof l1, "%s", nom);
        if (strlen(nom) > 18) {
            int mid = (int)strlen(nom) / 2, k = -1;
            for (int d = 0; d < mid && k < 0; d++) { if (nom[mid - d] == ' ') k = mid - d; else if (nom[mid + d] == ' ') k = mid + d; }
            if (k > 0) { snprintf(l1, sizeof l1, "%.*s", k, nom); snprintf(l2, sizeof l2, "%s", nom + k + 1); }
        }
        lv_obj_t *t = texto(s_tarjeta, l1, 0xFFE9A0, 2, 0);
        lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 22);
        nom_h = lv_image_get_src(t) ? (int)((const lv_image_dsc_t *)lv_image_get_src(t))->header.h : 20;
        if (l2[0]) {
            lv_obj_t *t2 = texto(s_tarjeta, l2, 0xFFE9A0, 2, 0);
            lv_obj_align(t2, LV_ALIGN_TOP_MID, 0, 22 + nom_h + 2);
            nom_h = nom_h * 2 + 2;
        }
    }
    if (!nino) {
        char cat[48];
        snprintf(cat, sizeof cat, "%s", objeto_cat_nombre(o->cat));
        lv_obj_t *ct = texto(s_tarjeta, cat, C_VIOLETA, 2, 230);
        lv_obj_align(ct, LV_ALIGN_TOP_MID, 0, 22 + nom_h + 4);
        nom_h += 22;
    }
    /* el objeto grande (en los grandes, ahí se hace el gesto) */
    s_obj_grande = icono(s_tarjeta, ICO_OBJETO(id), nino ? 7 : 8);
    lv_obj_align(s_obj_grande, LV_ALIGN_TOP_MID, 0, (s_nino ? 40 : 22 + nom_h + 8));
    if (!listo || !tiene) lv_obj_set_style_opa(s_obj_grande, LV_OPA_50, 0);
    lv_obj_add_flag(s_obj_grande, LV_OBJ_FLAG_CLICKABLE); /* tocarlo muestra qué hace */
    lv_obj_set_ext_click_area(s_obj_grande, 10);
    lv_obj_add_event_cb(s_obj_grande, obj_toca_cb, LV_EVENT_CLICKED, NULL);
    const int sobre_y = (s_nino ? 40 : 22 + nom_h + 8) + (nino ? 7 : 8) * 14 + 10; /* dónde acaba el objeto */
    if (nino) {
        /* qué hace, en un dibujo, y dos botones grandes */
        if (s_obj_ley) { /* lo que hace: su secreto escrito debajo del dibujo (y el dibujo de su efecto); si no lo sabe, el misterio */
            if (sabe) {
                if (s_nino) { lv_obj_t *ef = icono(s_tarjeta, icono_efecto(o->efecto), 5); lv_obj_align(ef, LV_ALIGN_TOP_MID, 0, sobre_y); }
                else {
                    lv_obj_t *lt = texto_c(s_tarjeta, pet_object_legend(s_pet, id), 0xFFF3C8, 2, 276); /* su secreto, escrito debajo del dibujo */
                    lv_obj_align(lt, LV_ALIGN_TOP_MID, 0, sobre_y);
                }
            } else {
                lv_obj_t *ef = pregunta_magica(s_tarjeta, s_nino ? 84 : 64);
                lv_obj_align(ef, LV_ALIGN_TOP_MID, 0, sobre_y);
                if (!s_nino) { lv_obj_t *lt = texto_c(s_tarjeta, "TODAVÍA NO SABES QUÉ HACE.", 0xD8C8FF, 2, 276); lv_obj_align(lt, LV_ALIGN_TOP_MID, 0, sobre_y + 70); }
            }
        }
        if (tiene) {
            lv_obj_t *b = boton(s_tarjeta, 96, 52, obj_panel_cb, listo ? 0 : 9);
            lv_obj_align(b, LV_ALIGN_BOTTOM_MID, -56, -44);
            lv_obj_t *bi = icono(b, listo ? ICO_SI : ICO_LUNA, 3); lv_obj_center(bi);
            lv_obj_t *d = boton(s_tarjeta, 96, 52, obj_panel_cb, 1);
            lv_obj_align(d, LV_ALIGN_BOTTOM_MID, 56, -44);
            lv_obj_t *di = icono(d, ICO_BOSQUE_DEV, 3); lv_obj_center(di);
            lv_obj_t *t1 = texto(s_tarjeta, listo ? "DÁSELO" : "MAÑANA", C_GRIS, 2, 0);
            lv_obj_align(t1, LV_ALIGN_BOTTOM_MID, -56, -24);
            lv_obj_t *t2 = texto(s_tarjeta, "OFRENDA", C_GRIS, 2, 0);
            lv_obj_align(t2, LV_ALIGN_BOTTOM_MID, 56, -24);
            if (bloq) { /* dormida o de viaje: apagados */
                lv_obj_add_state(b, LV_STATE_DISABLED); lv_obj_add_state(d, LV_STATE_DISABLED);
                lv_obj_set_style_opa(b, LV_OPA_40, 0); lv_obj_set_style_opa(d, LV_OPA_40, 0);
            }
        }
    } else {
        static const char *const GESTO[OGE_COUNT] = {"TÓCALO", "FRÓTALO CON EL DEDO", "MANTÉN EL DEDO ENCIMA", "DESLIZA EL DEDO", "SÓPLALE AL ORBE"};
        static const char *const DESCUBRE[OGE_COUNT] = {"SÁCALO AL PRADO Y TÓCALO PARA DESCUBRIR QUÉ HACE.", "SÁCALO AL PRADO Y FRÓTALO CON EL DEDO PARA DESCUBRIR QUÉ HACE.",
                                                        "SÁCALO AL PRADO Y MANTÉN EL DEDO ENCIMA PARA DESCUBRIR QUÉ HACE.", "SÁCALO AL PRADO Y DESLIZA EL DEDO SOBRE ÉL PARA DESCUBRIR QUÉ HACE.",
                                                        "SÁCALO AL PRADO Y SÓPLALE AL ORBE (O DESLIZA EL DEDO) PARA DESCUBRIR QUÉ HACE."};
        static const art_icono_t GICO[OGE_COUNT] = {ICO_MANO, ICO_MANO_FROTA, ICO_MANO_TIENE, ICO_MANO_DESLIZA, ICO_VIENTO};
        const int g = o->gesto % OGE_COUNT;
        /* lo que hace queda oculto hasta que lo pruebas una vez */
        lv_obj_t *ar = area_texto(s_tarjeta, 30, sobre_y, 308, 300 - sobre_y);
        lv_obj_set_flex_align(ar, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        if (s_obj_ley) { /* la leyenda: qué hace y para qué sirve; si nunca lo ha usado, el misterio */
            if (sabe) texto_c(ar, pet_object_legend(s_pet, id), 0xFFF3C8, 2, 276);
            else { pregunta_magica(ar, 72); texto_c(ar, "TODAVÍA NO SABES QUÉ HACE.", 0xD8C8FF, 2, 276); }
        } else if (sabe) {
            if (tiene) texto_c(ar, listo ? GESTO[g] : "DESCANSA HASTA MAÑANA", listo ? C_VERDE : C_GRIS, 2, 0);
            if (!tiene) texto_c(ar, "TOCA LA LUPA PARA RECORDAR QUÉ HACE.", C_GRIS, 2, 276);
        } else {
            texto_c(ar, tiene ? (listo ? DESCUBRE[g] : "DESCANSA HASTA MAÑANA. DESPUÉS PODRÁS DESCUBRIR QUÉ HACE.") : "TODAVÍA NO SABES QUÉ HACE.",
                  tiene && listo ? C_VERDE : C_GRIS, 2, 276);
        }
        if (id == OBJ_CORAZON_BOSQUE && tiene) { /* el rito de la Guardiana: qué falta */
            char g2[320];
            int n = pet_nights_to_full_moon(s_ultimo_t);
            const char *luna = n == 0 ? "¡LA LUNA LLENA ES ESTA NOCHE!" : (n == 1 ? "LA PRÓXIMA LUNA LLENA ES MAÑANA EN LA NOCHE." : NULL);
            char lt[64];
            if (!luna) { snprintf(lt, sizeof lt, "LA PRÓXIMA LUNA LLENA ES EN %d NOCHES.", n); luna = lt; }
            snprintf(g2, sizeof g2, "LATE MÁS FUERTE CON LA LUNA LLENA. UNA NOCHE DE LUNA LLENA, EN EL CÍRCULO DE PIEDRAS DE LA ISLA, %s PUEDE DESPERTAR SU MAGIA. %s",
                     s_pet->etapa == STAGE_ADULTA ? "{ELLA}" : "CUANDO SEA GRANDE, {ELLA}", luna);
            texto_c(ar, g2, pet_guardian_ready(s_pet, s_ultimo_t) ? C_VERDE : C_VIOLETA, 2, 276);
        }
        area_lista(ar);
        lv_obj_add_flag(ar, LV_OBJ_FLAG_EVENT_BUBBLE);
        (void)GICO;
        if (tiene) {
            bool cabeza = s_pet->obj_cabeza == id + 1;
            if (listo && !cabeza) { /* sacarlo al prado: ahí se hace su gesto y ella va a usarlo */
                lv_obj_t *u = boton(s_tarjeta, 112, 38, obj_panel_cb, 0);
                lv_obj_align(u, LV_ALIGN_BOTTOM_MID, -60, -24);
                lv_obj_t *ut = texto(u, "SACARLO", C_BLANCO, 2, 0); lv_obj_center(ut);
                if (bloq) { lv_obj_add_state(u, LV_STATE_DISABLED); lv_obj_set_style_opa(u, LV_OPA_40, 0); }
            }
            if (id != OBJ_CORAZON_BOSQUE) { /* el Corazón no se devuelve: es para el rito */
                lv_obj_t *d = boton(s_tarjeta, 112, 38, obj_panel_cb, 1); /* ofrenda al bosque */
                lv_obj_align(d, LV_ALIGN_BOTTOM_MID, listo && !cabeza ? 60 : (cabeza ? -64 : 0), -24);
                lv_obj_t *dt = texto(d, "OFRENDA", C_BLANCO, 2, 0); lv_obj_center(dt);
                if (bloq) { lv_obj_add_state(d, LV_STATE_DISABLED); lv_obj_set_style_opa(d, LV_OPA_40, 0); }
            }
        }
    }
    /* lo que está puesto se puede guardar o sacar */
    if (puesto || s_pet->obj_cabeza == id + 1) {
        lv_obj_t *g = boton(s_tarjeta, nino ? 120 : 150, 44, obj_panel_cb, puesto ? 2 : 3);
        lv_obj_align(g, LV_ALIGN_BOTTOM_MID, tiene ? 80 : 0, tiene && nino ? -112 : -22);
        if (nino) { lv_obj_t *gi = icono(g, ICO_BOLSA, 2); lv_obj_center(gi); }
        else { lv_obj_t *gt = texto(g, puesto ? "GUARDAR" : "SACÁRSELO", C_BLANCO, 2, 0); lv_obj_center(gt); }
    }
}

/* lo que pasa cuando encuentra uno, o cuando lo usas */
/* ======================================================================
 * La niebla de la mañana, los regalos al volver y soplar al reloj
 * ==================================================================== */
#define N_NIEBLA 7
#define N_HUELLAS 7
static lv_obj_t *s_niebla[N_NIEBLA], *s_huella[N_HUELLAS], *s_palo;
static lv_obj_t *s_nb_aviso, *s_nb_aviso_c, *s_nb_aviso_t, *s_nb_aviso_ic;
static art_img_t s_nb_aviso_art;
static bool s_nb_soplado; /* ya soplaste (o barriste) esta niebla: la pista se va */
static float s_nb_x[N_NIEBLA], s_nb_opa, s_nb_barrido;
static int s_nb_ult_x = -1;
static uint32_t s_nb_ult_ms;
static bool s_bola_palo; /* la pelota es el palo perfecto */
static void empezar_juego(int j);
static bool pastel_visible(void);
static void soplar_velas(void);
static const int16_t POS_HUELLA[N_HUELLAS][2] = {{64, 330}, {92, 316}, {124, 306}, {154, 296}, {184, 290}, {214, 282}, {242, 274}};

static void palo_cb(lv_event_t *e)
{
    (void)e;
    if (!s_pet || s_juego != JG_NADA) return;
    s_bola_palo = true;
    lv_image_set_src(s_pelota, art_icono(ICO_RAMITA, 3));
    s_juego_pedido = JUEGO_PELOTA;
    accion(ACT_JUGAR, JUEGO_PELOTA);
}

static void crear_niebla(void)
{
    for (int i = 0; i < N_HUELLAS; i++) {
        s_huella[i] = lv_image_create(s_mundo);
        lv_image_set_src(s_huella[i], art_icono(ICO_HUELLA, 2));
        lv_image_set_pivot(s_huella[i], 12, 12);
        lv_image_set_rotation(s_huella[i], (i & 1) ? 700 : 500);
        lv_obj_set_pos(s_huella[i], POS_HUELLA[i][0] - 12, POS_HUELLA[i][1] - 12 + ((i & 1) ? 6 : -4));
        lv_obj_set_style_opa(s_huella[i], LV_OPA_80, 0);
        lv_obj_move_to_index(s_huella[i], 1);
        ocultar(s_huella[i], true);
    }
    s_palo = lv_image_create(s_mundo);
    lv_image_set_src(s_palo, art_icono(ICO_RAMITA, 3));
    lv_obj_add_flag(s_palo, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(s_palo, 14);
    lv_obj_add_event_cb(s_palo, palo_cb, LV_EVENT_CLICKED, NULL);
    ocultar(s_palo, true);
    for (int i = 0; i < N_NIEBLA; i++) {
        s_niebla[i] = caja(s_mundo);
        lv_obj_set_size(s_niebla[i], 230 + (i % 3) * 30, 70 + (i % 2) * 20);
        lv_obj_set_style_radius(s_niebla[i], 45, 0);
        lv_obj_set_style_bg_color(s_niebla[i], lv_color_hex(0xF2F4FA), 0);
        lv_obj_set_style_bg_opa(s_niebla[i], LV_OPA_COVER, 0);
        /* sin sombra difusa: desenfocar 7 manchas grandes en cada cuadro dejaba el reloj lentísimo */
        s_nb_x[i] = -80 + (float)(i * 97 % 420);
        ocultar(s_niebla[i], true);
    }
    /* la pista para soplar: una boquita con ondas de aire (y "SOPLA" en modo adulto) */
    s_nb_aviso = caja(s_mundo);
    lv_obj_set_size(s_nb_aviso, 140, 48);
    s_nb_aviso_c = cartel_de(s_nb_aviso, &s_nb_aviso_t, 0x2A2238, LV_OPA_70);
    art_texto(&s_nb_aviso_art, "SOPLA", 0xFFFFFF, 2, 0);
    cartel_poner(s_nb_aviso_c, s_nb_aviso_t, &s_nb_aviso_art);
    lv_obj_align(s_nb_aviso_c, LV_ALIGN_RIGHT_MID, -4, 0);
    s_nb_aviso_ic = lv_image_create(s_nb_aviso);
    lv_image_set_src(s_nb_aviso_ic, art_icono(ICO_SOPLA, 3));
    ocultar(s_nb_aviso, true);
}

static void disipar_niebla(void)
{
    if (!s_pet || !pet_fog(s_pet, s_ultimo_t)) return;
    s_nb_barrido = 900;
    s_nb_soplado = true;
    accion(UI_ACCION_NIEBLA, 0);
    for (int i = 0; i < 3; i++) flotar(ICO_VIENTO, NULL, 0, 120 + i * 110, 200, (uint32_t)i * 150);
}

/* barrer la niebla con el dedo (desde el fondo del mundo) */
static void niebla_arrastre(int x)
{
    if (!s_pet || !pet_fog(s_pet, s_ultimo_t)) return;
    if (s_nb_ult_x >= 0 && s_ms - s_nb_ult_ms < 150) s_nb_barrido += (float)abs(x - s_nb_ult_x);
    s_nb_ult_x = x;
    s_nb_ult_ms = s_ms;
    if (s_nb_barrido >= 900) disipar_niebla();
}

static void animar_niebla(void)
{
    const pet_t *p = s_pet;
    if (!p) return;
    bool afuera = !s_interior && s_escena != ESC_CUEVA;
    bool hay = pet_fog(p, s_ultimo_t) && afuera;
    if (!hay && !pet_fog(p, s_ultimo_t)) { s_nb_barrido = 0; s_nb_soplado = false; }
    float meta = hay ? 1.0f - (s_nb_barrido > 900 ? 900 : s_nb_barrido) / 900.0f : 0.0f;
    s_nb_opa += (meta - s_nb_opa) * 0.04f;
    static bool s_nb_visible;
    const bool ver = s_nb_opa >= 0.02f;
    const uint32_t paso = s_ms / 220u; /* se mueve a saltitos suaves: no se repinta todo el mundo en cada cuadro */
    static uint32_t s_nb_paso;
    const bool mover = paso != s_nb_paso;
    s_nb_paso = paso;
    for (int i = 0; i < N_NIEBLA; i++) {
        const bool oculta = !ver || i >= 4; /* cuatro manchas bastan */
        ocultar(s_niebla[i], oculta);
        if (oculta) continue;
        if (mover) {
            s_nb_x[i] += (i & 1) ? 1.2f : -1.0f;
            if (s_nb_x[i] > W) s_nb_x[i] = -280;
            if (s_nb_x[i] < -280) s_nb_x[i] = W;
            lv_obj_set_pos(s_niebla[i], (int)s_nb_x[i], 40 + i * 60 + (int)(6 * sinf((float)paso / 9.0f + i)));
        }
        const lv_opa_t op = (lv_opa_t)(s_nb_opa * (i & 1 ? 120 : 150)) & 0xF8; /* por escalones: no cambia a cada cuadro */
        /* la transparencia va en el fondo de la mancha (bg_opa), no en toda la mancha (opa): esa última obliga a LVGL a pintarla en una capa aparte en cada cuadro, y era lo que dejaba lento el reloj con niebla */
        if (lv_obj_get_style_bg_opa(s_niebla[i], 0) != op) lv_obj_set_style_bg_opa(s_niebla[i], op, 0);
        if (!s_nb_visible) lv_obj_move_foreground(s_niebla[i]);
    }
    s_nb_visible = ver;
    bool aviso = hay && s_nb_opa > 0.3f && !s_nb_soplado && s_nb_barrido < 200 && s_juego == JG_NADA;
    ocultar(s_nb_aviso, !aviso);
    if (aviso) { /* se mece suave, como si respirara */
        bool txt = !s_nino; /* como la pista de la vela: los más chicos, solo el dibujo */
        ocultar(s_nb_aviso_c, !txt);
        lv_obj_set_width(s_nb_aviso, txt ? 140 : 48);
        float f = sinf((float)s_ms / 420.0f);
        lv_obj_align(s_nb_aviso_ic, LV_ALIGN_LEFT_MID, (int)(2 * f), 0);
        lv_obj_set_pos(s_nb_aviso, CX - (txt ? 70 : 24), 300 + (int)(3 * sinf((float)s_ms / 900.0f)));
        lv_obj_set_style_opa(s_nb_aviso_ic, (lv_opa_t)(200 + 55 * f), 0);
        lv_obj_move_foreground(s_nb_aviso);
    }
    /* las huellas de barro hasta el baño */
    { static bool tuvo_barro; if (p->barro) tuvo_barro = true; else if (tuvo_barro) { tuvo_barro = false; s_barro_pochi = false; } } /* (la acción llega un cuadro después: solo se borra al limpiarse) */
    bool barro = p->barro && s_escena == ESC_PRADO && !s_interior && !s_barro_pochi;
    for (int i = 0; i < N_HUELLAS; i++) ocultar(s_huella[i], !barro);
    /* el palo perfecto espera que se lo tires */
    bool palo = s_ultimo_t < p->palo_hasta && s_juego == JG_NADA && !p->dormida && !s_interior && s_mascota.visible;
    ocultar(s_palo, !palo);
    if (palo) lv_obj_set_pos(s_palo, (int)s_mascota.x + 30, (int)s_mascota.y - 24);
}

/* main pregunta si hace falta escuchar (para no gastar batería con el micrófono prendido) */
bool ui_wants_mic(void)
{
    if (s_cru_activo) return true; /* en el bote: soplarle a la vela */
    if (zj_quiere_mic()) return true; /* (v98) las galletas de Peumo: el fuego se aviva soplando */
    if (pez_quiere_soplo()) return true; /* v89: picó y hay que soplar */
    if (s_mic_objeto >= 0) return true;
    if (s_panel != PANEL_NINGUNO || s_juego != JG_NADA || !s_pet) return false;
    return (pet_fog(s_pet, s_ultimo_t) && !s_interior) || pastel_visible();
}

/* le soplaste al reloj */
void ui_mic_blow(void)
{
    if (zj_quiere_mic()) { zj_soplido(); return; } /* (v98) */
    if (s_cum_fase == 1) { cumple_soplar(); return; }
    if (s_cru_activo) { cruce_rafaga(); return; }
    if (s_pez_on) { pez_soplo(); return; }
    if (s_mic_objeto >= 0) { if (s_om_fase == 1 && s_om_id == s_mic_objeto) om_listo(); else usar_objeto(s_mic_objeto); return; }
    if (s_panel != PANEL_NINGUNO || !s_pet) return;
    if (pet_fog(s_pet, s_ultimo_t) && !s_interior) { disipar_niebla(); return; }
    if (pastel_visible()) soplar_velas();
}

/* volviste y te esperaba con algo */
static void evento_regalo(const pet_event_t *e, int hx, int hy)
{
    static const art_icono_t ICO[REG_COUNT] = {ICO_PINA, ICO_HOJA, ICO_RAMITA, ICO_PLUMA, ICO_SAMARA, ICO_PIEDRA, ICO_BOTA};
    static const char *const TXT[REG_COUNT] = {"¡UNA PIÑA PARA TI! TIENE SEMILLAS ADENTRO", "¡MIRA MI SOMBRERO DE HOJA!", "¡EL PALO PERFECTO! ¿ME LO TIRAS?",
                                               "¡UNA PLUMA! ¡ACHÍS!", "UNA SEMILLA CON ALAS... ¡LA PLANTÉ!", "UNA PIEDRA DE LA LAGUNA, PARA TI",
                                               "¡UPS! TRAJE BARRO EN LAS PATAS"};
    int g = e->a;
    if ((unsigned)g >= REG_COUNT) return;
    lv_image_set_src(s_objeto, art_icono(ICO[g], 3));
    ocultar(s_objeto, g == REG_HOJA || g == REG_BOTAS); /* la hoja va en la cabeza; las botas, en las huellas */
    mascota_modo(g == REG_BOTAS ? M_SALTA : M_TRAE, 3800);
    const char *t = TXT[g];
    if (g == REG_HOJA && s_pet && (s_pet->clima == CLIMA_LLUVIA || s_pet->clima == CLIMA_TORMENTA)) t = "¡MIRA, UN PARAGUAS DE HOJA!";
    globo(ICO[g], t, 4000);
    switch (g) {
    case REG_PLUMA: accion(UI_ACCION_SONAR, SND_ESTORNUDO); flotar(ICO_PLUMA, NULL, 0, hx + 10, hy - 40, 400); break;
    case REG_SAMARA: /* cae girando */
        lv_image_set_src(s_vuela, art_icono(ICO_SAMARA, 3));
        lv_image_set_pivot(s_vuela, 18, 18);
        lv_obj_set_pos(s_vuela, hx - 60, 40);
        lv_image_set_scale(s_vuela, 256);
        ocultar(s_vuela, false);
        {
            lv_anim_t a;
            lv_anim_init(&a);
            lv_anim_set_var(&a, s_vuela);
            lv_anim_set_duration(&a, 2200);
            lv_anim_set_exec_cb(&a, anim_y_pos);
            lv_anim_set_values(&a, 40, hy - 40);
            lv_anim_start(&a);
            lv_anim_set_exec_cb(&a, anim_rot);
            lv_anim_set_values(&a, 0, 3600 * 3);
            lv_anim_start(&a);
            s_vuela_hasta = s_ms + 2300;
        }
        flotar(ICO_BROTE, NULL, 0, hx, hy, 2400);
        break;
    case REG_PINA: flotar(ICO_SEMILLA, NULL, 0, hx + 20, hy - 20, 1500); break;
    case REG_BOTAS: accion(UI_ACCION_SONAR, SND_SALPICON); break;
    default: break;
    }
}

static void confeti(int x, int y, int n);
static void evento_objeto(const pet_event_t *e, int hx, int hy)
{
    const pet_t *p = s_pet;
    if (e->type == EV_OBJETO) {
        int id = obj_de(e->a, e->b);
        const objeto_info_t *o = objeto_info(id);
        if (!o) return;
        bool nuevo = e->b & 2, trae = e->b & 4, oro = e->b & 64;
        char t[80];
        if (oro) { /* juego+: ¡la versión dorada! */
            confeti(hx, hy - 30, 14);
            flotar(ICO_ESTRELLA, NULL, 0, hx - 24, hy - 30, 0);
            flotar(ICO_ESTRELLA, NULL, 0, hx + 30, hy - 36, 250);
            snprintf(t, sizeof t, "¡VERSIÓN DORADA! %s", o->nombre);
            for (char *c = t; *c; c++) if (*c >= 'a' && *c <= 'z') *c = (char)(*c - 32);
            globo(ICO_OBJETO(id), t, 4500);
        }
        if (((e->b >> 3) & 7) == OFU_REGALO) { /* vino con un regalo o un premio: solo vuela a la Bolsa */
            lv_area_t a;
            lv_obj_get_coords(s_dock[3], &a);
            volar(id, hx, hy - 30, (a.x1 + a.x2) / 2, (a.y1 + a.y2) / 2, 2600, true);
            return;
        }
        if (trae) {
            lv_image_set_src(s_objeto, art_icono(ICO_OBJETO(id), 3));
            ocultar(s_objeto, false);
            mascota_modo(M_TRAE, 3800);
            snprintf(t, sizeof t, nuevo ? "¡ALGO MÁGICO! %s" : "OTRA VEZ: %s", o->nombre);
            for (char *c = t; *c; c++) if (*c >= 'a' && *c <= 'z') *c = (char)(*c - 32);
            if (!oro) globo(ICO_OBJETO(id), t, 4000);
            /* después de mostrártelo, va a la Bolsa */
            lv_area_t a;
            lv_obj_get_coords(s_dock[3], &a);
            volar(id, hx, hy - 30, (a.x1 + a.x2) / 2, (a.y1 + a.y2) / 2, 3400, true);
        } else {
            flotar(ICO_ESTRELLA, NULL, 0, hx + 22, hy - 20, 0);
            lv_area_t a;
            lv_obj_get_coords(s_dock[3], &a);
            volar(id, hx, hy - 20, (a.x1 + a.x2) / 2, (a.y1 + a.y2) / 2, 300, true);
        }
        accion(UI_ACCION_SONAR, nuevo ? SND_TESORO : SND_SORPRESA);
        return;
    }
    /* EV_OBJETO_USO */
    int id = obj_de(e->a, e->b);
    const objeto_info_t *o = objeto_info(id);
    if (!o) return;
    const int cod = (e->b >> 2) & 7; /* lo que el núcleo cuenta del uso (según el objeto) */
    if (cod == 7 && !(e->b & 2)) { globo(ICO_CANDADO, "EL CANDADO RÚNICO LO CUIDA: NO SE PUEDE GASTAR", 3200); return; }
    if (!(e->b & 2)) { globo(ICO_NO, p && p->dormida ? "SHH... ESTÁ DURMIENDO" : "AHORA NO QUIERE", 2500); return; }
    if (id == 101 && p && !p->dormida) { /* la caja de música: suena la melodía y ella baila */
        s_baile_on = true;
        s_baile_desde = s_ms;
        s_baile_aciertos = 0;
        s_baile_ult_pulso = s_baile_nota = -1;
        accion(UI_ACCION_SONAR, SND_CAJA_MUSICA);
        mascota_modo(M_BAILA, BAILE_MS + 200);
        globo(ICO_OBJETO(101), s_nino ? NULL : "¡A BAILAR! TOCA LA PANTALLA AL RITMO DE LA MÚSICA", 2600);
        return;
    }
    switch (o->efecto) {
    case OEF_ESPECIAL: objx_uso(o->param, cod, id, hx, hy); break;
    case OEF_COMER: {
        static const art_icono_t PART[6] = {ICO_CORAZON, ICO_COPO, ICO_NUBE, ICO_ARCOIRIS, ICO_ESTRELLA, ICO_CORAZON};
        mascota_modo(M_COME, 2200);
        for (int i = 0; i < 4; i++) flotar(PART[o->param % 6], NULL, 0, hx - 36 + i * 24, hy - 10, 700 + (uint32_t)i * 200);
        accion(UI_ACCION_SONAR, SND_ALEGRE);
        break;
    }
    case OEF_CALMA: case OEF_CARINO:
        mascota_modo(M_ABRAZO, 1800);
        for (int i = 0; i < 4; i++) flotar(ICO_CORAZON, NULL, 0, hx - 36 + i * 24, hy - 10, (uint32_t)i * 180);
        break;
    case OEF_ENERGIA: mascota_modo(M_SALTA, 2500); flotar(ICO_RAYO, NULL, 0, hx, hy - 30, 200); break;
    case OEF_SUENO: flotar(ICO_ZETA, NULL, 0, hx + 20, hy - 30, 300); break;
    case OEF_CURAR: for (int i = 0; i < 3; i++) flotar(ICO_ESTRELLA, NULL, 0, hx - 24 + i * 24, hy - 20, (uint32_t)i * 200); break;
    case OEF_CABEZA: mascota_modo(M_SALTA, 1500); flotar(ICO_ESTRELLA, NULL, 0, hx + 24, hy - 50, 200); break;
    case OEF_SUERTE: globo(ICO_OBJETO(id), "¡HOY ES UN DÍA DE SUERTE!", 3000); break;
    case OEF_PESCA: globo(ICO_PEZ, "¡HOY LOS PECES VAN A PICAR!", 3000); break;
    case OEF_PROTEGE: globo(ICO_OBJETO(id), "HOY NADA NOS VA A MOLESTAR", 3000); break;
    case OEF_JARDIN: for (int i = 0; i < 4; i++) flotar(ICO_BROTE, NULL, 0, 120 + i * 70, 300, (uint32_t)i * 150); break;
    case OEF_DECORAR: flotar(ICO_ESTRELLA, NULL, 0, CX, 240, 0); break;
    case OEF_CIELO: { /* (v96) se nota al tiro: un destello alrededor de ella y dice qué pasa en el cielo */
        static const char *const QUE[8] = {"¡EL CIELO SE PINTÓ DE AURORA!", "¡SE TAPÓ EL SOL UN RATITO!", "¡CHISPAS EN EL CIELO!", "¡LLUEVEN ESTRELLAS FUGACES!",
                                           "¡FUEGOS DE COLORES!", "¡LLUEVE PURPURINA DE HADAS!", "¡LLEGARON LAS LUCIÉRNAGAS!", "¡MIRA LAS CONSTELACIONES!"};
        mascota_modo(M_SALTA, 1500);
        for (int i = 0; i < 6; i++) flotar(i & 1 ? ICO_ESTRELLA : ICO_ESTRELLA_FUGAZ, NULL, 0, hx - 60 + i * 24, hy - 20 - (i & 1) * 18, (uint32_t)i * 120);
        accion(UI_ACCION_SONAR, SND_HADAS);
        if (!s_nino) globo(ICO_OBJETO(id), s_interior ? "¡AFUERA EL CIELO SE LLENÓ DE MAGIA! ¿SALIMOS A VER?" : QUE[o->param & 7], 3500);
        break;
    }
    default: mascota_modo(M_SALTA, 1200); flotar(ICO_ESTRELLA, NULL, 0, hx, hy - 30, 200); break;
    }
}

static int s_gesto = GESTO_TOQUE, s_toque_x, s_toque_y;

/* Gestos sobre la mascota (la pantalla es chica: no se apunta a partes del cuerpo):
 *   un toque = palmadita · deslizar el dedo = caricia · mantener = abrazo
 *   muchos toques rápidos seguidos = la marea (a casi todas les molesta) */
static uint32_t s_dedo_desde, s_rafaga_ms;
static int s_dedo_x, s_dedo_y, s_rafaga_n;
static uint32_t s_callar_hasta; /* si canta y la tocas varias veces, se calla hasta aquí */
static float s_dedo_dist, s_dedo_traza;
static bool s_dedo_hecho;

static void gesto(int g, int x, int y)
{
    s_gesto = g;
    s_toque_x = x;
    s_toque_y = y;
    if (s_pet && s_pet->dormida && g == GESTO_MUCHOS) { accion(ACT_DESPERTAR, 0); return; } /* así la despiertas */
    accion(ACT_ACARICIAR, g);
}

static bool s_cepillada;
static void mascota_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_point_t pt = {0, 0};
    lv_indev_t *in = lv_indev_active();
    if (in) lv_indev_get_point(in, &pt);
    if (s_baile_on) { if (code == LV_EVENT_PRESSED) baile_toque(pt.x, pt.y); return; } /* bailando: los toques son el ritmo */
    if (code == LV_EVENT_PRESSED) {
        s_dedo_desde = s_ms;
        s_dedo_x = pt.x; s_dedo_y = pt.y;
        s_dedo_dist = s_dedo_traza = 0;
        s_dedo_hecho = false;
        return;
    }
    if (code == LV_EVENT_PRESSED) s_cepillada = false;
    if (code == LV_EVENT_PRESSING) {
        s_dedo_dist += hypotf((float)(pt.x - s_dedo_x), (float)(pt.y - s_dedo_y));
        s_dedo_x = pt.x; s_dedo_y = pt.y;
        if (!s_dedo_hecho && s_dedo_dist > 40) { s_dedo_hecho = true; s_rafaga_n = 0; gesto(GESTO_CARICIA, pt.x, pt.y); }
        if (s_dedo_hecho && s_gesto == GESTO_CARICIA && s_dedo_dist - s_dedo_traza > 45 && s_pet && !s_pet->dormida) {
            s_dedo_traza = s_dedo_dist; /* corazoncitos por donde pasa el dedo */
            flotar(ICO_CORAZON, NULL, 0, pt.x, pt.y - 16, 0);
            /* una caricia larga y suave también la peina (antes era el botón del cepillo) */
            if (s_dedo_dist > 600 && !s_cepillada) { s_cepillada = true; accion(ACT_CEPILLAR, 0); }
        }
        if (!s_dedo_hecho && s_ms - s_dedo_desde > 650 && s_dedo_dist < 24) { s_dedo_hecho = true; s_rafaga_n = 0; gesto(GESTO_ABRAZO, pt.x, pt.y); }
        return;
    }
    if (code != LV_EVENT_RELEASED || s_dedo_hecho) return;
    if (s_resp_toque_hasta && (int32_t)(s_resp_toque_hasta - s_ms) > 0) { /* la tocaste justo después de responderle */
        s_resp_toque_hasta = 0;
        for (int i = 0; i < 6; i++) flotar(ICO_CORAZON, NULL, 0, pt.x - 50 + i * 20, pt.y - 20 - (i & 1) * 14, (uint32_t)i * 90);
        accion(UI_ACCION_SONAR, SND_RISA);
    }
    if (s_ms - s_rafaga_ms > 1300) s_rafaga_n = 0;
    s_rafaga_ms = s_ms;
    s_rafaga_n++;
    if (s_rafaga_n == 1) gesto(GESTO_TOQUE, pt.x, pt.y);
    else if (s_rafaga_n == 5) gesto(GESTO_MUCHOS, pt.x, pt.y);
    else if (s_rafaga_n < 5) mascota_modo(M_SALTA, 160); /* un respingo con cada toque */
    if (s_rafaga_n == 3 && s_pet && s_pet->actividad == ACTV_CANTAR && !s_pet->dormida) { /* tres toques seguidos mientras canta: la callas */
        s_callar_hasta = s_ms + 150000u;
        s_rafaga_n = 0;
        flotar(ICO_ZETA, NULL, 0, pt.x, pt.y - 30, 0);
        globo(ICO_NOTA, s_nino ? NULL : "¡SHHH! YA ME CALLO", 2500);
    }
}

/* doble toque en el prado: la llamas a ese lugar */
static float s_llamado_x, s_llamado_y;
static int s_toque1_x = -1000, s_toque1_y;
static uint32_t s_toque1_ms;

static void caca_cb(lv_event_t *e)
{
    int i = (int)(intptr_t)lv_event_get_user_data(e);
    if (i < 0 || i >= N_CACAS) return;
    for (int k = 0; k < 4; k++) salpicar(s_caca_x[i] - 8 + k * 5, s_caca_y[i] - 12, 0xFFF6A8); /* brillitos */
    flotar(ICO_ESTRELLA, NULL, 0, s_caca_x[i], s_caca_y[i] - 40, 0);
    ocultar(s_cacas[i], true);
    s_caca_sem++; /* la próxima caerá en otro lado */
    accion(ACT_LIMPIAR, 0);
}

static void fondo_cb(lv_event_t *e)
{
    (void)e;
    if (!s_pet || s_pet->etapa == STAGE_HUEVO) return;
    lv_point_t pt = {0, 0};
    lv_indev_t *in = lv_indev_active();
    if (in) lv_indev_get_point(in, &pt);
    if (s_baile_on) { baile_toque(pt.x, pt.y); return; }
    if (colocar_toque(pt.x, pt.y)) return;                    /* pones un juguete */
    if (s_juego != JG_NADA) { esco_toque(pt.x, pt.y); return; } /* jugando no se la llama */
    int d = decor_en(pt.x, pt.y);                            /* tres toques sobre una decoración: guardarla */
    if (d >= 0 && toques_seguidos(4000 + d) >= 3) { mostrar_quitar(QUITAR_DECOR, d, pt.x + 20, pt.y - 30); s_toque1_x = -1000; return; }
    bool doble = s_ms - s_toque1_ms < 500 && abs(pt.x - s_toque1_x) < 50 && abs(pt.y - s_toque1_y) < 50;
    s_toque1_x = pt.x; s_toque1_y = pt.y; s_toque1_ms = s_ms;
    if (!doble) return;
    s_toque1_x = -1000;
    if (globo_cerrar()) return; /* (v39) con un globo a la vista, el doble toque solo lo cierra (ella no va) */
    s_llamado_x = (float)pt.x;
    s_llamado_y = (float)pt.y + 20.0f; /* los pies quedan justo bajo el dedo */
    limitar(&s_llamado_x, &s_llamado_y);
    int cerca = 0;
    float mejor = 1e9f;
    /* sobre el agua (el lago, o el estanque del prado): la llamas a nadar */
    bool al_agua = false;
    if (!s_interior && s_escena == ESC_LAGO && (float)pt.y < (float)terreno_ymin((float)pt.x) - 4.0f) al_agua = true;
    if (!s_interior && s_escena == ESC_PRADO) { float ex = ((float)pt.x - 106.0f) / 72.0f, ey = ((float)pt.y - 300.0f) / 42.0f; al_agua = ex * ex + ey * ey < 1.0f; }
    if (al_agua) { mejor = 0; cerca = LUGAR_ESTANQUE; }
    for (int l = 0; l < LUGAR_COUNT && !al_agua; l++) {
        float d = hypotf(LXY(l)[0] - s_llamado_x, LXY(l)[1] - s_llamado_y);
        if (d < mejor) { mejor = d; cerca = l; }
    }
    flotar(ICO_MANO, NULL, 0, pt.x, pt.y, 0);
    accion(ACT_LLAMAR, cerca);
}

static void visita_cb(lv_event_t *e)
{
    (void)e;
    if (!s_visita.visible) return;
    flotar(ICO_CORAZON, NULL, 0, (int)s_visita.x, cabeza_y(&s_visita), 0);
    accion(ACT_MIMAR_VISITA, 0);
}

/* ======================================================================
 * El mundo vivo: zonas, fauna, jardín, charcos y hojas
 * ==================================================================== */

static int s_zona_pend = -1, s_zona_lado = 1; /* lado por el que se va: 0 oeste, 1 este, 2 norte, 3 sur */
static void refrescar_fondo(const pet_t *p, uint32_t utc);
static lv_obj_t *s_casa_pen;

/* posición de cada parcela (base de la planta): 6 en el prado y 3 en el bosque, el lago y la colina */
static const int16_t POS_PARCELA[PET_PARCELAS_TOTAL][2] = {
    {152, 120}, {192, 116}, {232, 120}, {146, 154}, {186, 150}, {226, 154}, /* prado */
    {186, 148}, {350, 318}, {388, 312},                                     /* bosque */
    {80, 318}, {356, 312}, {394, 306},                                      /* lago */
    {352, 272}, {390, 266}, {372, 308},                                     /* colina */
};
#define HOJAS_X 110
#define HOJAS_Y 250
static const int16_t POS_CHARCO[2][2] = {{282, 276}, {344, 292}};

static lv_obj_t *s_parcela[PET_PARCELAS_TOTAL], *s_planta[PET_PARCELAS_TOTAL], *s_nieve_pl[PET_PARCELAS_TOTAL][2]; /* la nieve sobre cada planta */
static uint32_t s_planta_clave[PET_PARCELAS_TOTAL];
static lv_obj_t *s_charco[2], *s_hojas;

/* --- quitar cosas del mapa: tres toques seguidos muestran una X para sacarla --- */
static lv_obj_t *s_quitar;
static int s_quitar_tipo, s_quitar_cual;
static uint32_t s_quitar_hasta;
static int s_toques_n, s_toques_clave = -1;
static uint32_t s_toques_t;

/* cuenta toques seguidos sobre la misma cosa; devuelve cuántos van */
static int toques_seguidos(int clave)
{
    if (clave == s_toques_clave && s_ms - s_toques_t < 700) s_toques_n++;
    else { s_toques_n = 1; s_toques_clave = clave; }
    s_toques_t = s_ms;
    return s_toques_n;
}

static void ocultar_quitar(void) { s_quitar_tipo = QUITAR_NADA; ocultar(s_quitar, true); }

static void mostrar_quitar(int tipo, int cual, int x, int y)
{
    s_quitar_tipo = tipo;
    s_quitar_cual = cual;
    s_quitar_hasta = s_ms + 4000;
    s_toques_n = 0;
    if (x < 30) x = 30;
    if (x > W - 30) x = W - 30;
    if (y < 40) y = 40;
    lv_obj_set_pos(s_quitar, x - 20, y - 20);
    ocultar(s_quitar, false);
    lv_obj_move_foreground(s_quitar);
}

static void quitar_cb(lv_event_t *e)
{
    (void)e;
    int t = s_quitar_tipo, c = s_quitar_cual;
    ocultar_quitar();
    if (t == QUITAR_JUGUETE) accion(ACT_PONER_JUGUETE, c | 255 << 8);
    else if (t == QUITAR_PLANTA) accion(ACT_ARRANCAR, c);
    else if (t == QUITAR_DECOR) accion(UI_ACCION_DECOR_VER, c);
}

static void crear_quitar(lv_obj_t *scr)
{
    s_quitar = lv_button_create(scr);
    lv_obj_set_size(s_quitar, 40, 40);
    lv_obj_set_style_radius(s_quitar, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(s_quitar, lv_color_hex(0xC0405A), 0);
    lv_obj_set_style_border_width(s_quitar, 3, 0);
    lv_obj_set_style_border_color(s_quitar, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_shadow_width(s_quitar, 0, 0);
    lv_obj_set_ext_click_area(s_quitar, 8);
    lv_obj_add_event_cb(s_quitar, quitar_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *ic = icono(s_quitar, ICO_NO, 2);
    lv_obj_center(ic);
    ocultar(s_quitar, true);
}

/* decoraciones del prado: dónde están en la pantalla (para quitarlas con tres toques) */
static const int16_t ZONA_DECOR[DECOR_COUNT][4] = {
    {150, 270, 180, 40}, /* flores */
    {180, 330, 54, 40},  /* hongos */
    {336, 252, 60, 80},  /* farol */
    {116, 196, 76, 56},  /* banco */
    {250, 76, 96, 104},  /* casita (tronco) */
};
static int decor_en(int x, int y)
{
    if (!s_pet || s_escena != ESC_PRADO) return -1;
    for (int d = 0; d < DECOR_COUNT; d++)
        if (pet_decor_visible(s_pet, (decor_t)d) && x >= ZONA_DECOR[d][0] && x < ZONA_DECOR[d][0] + ZONA_DECOR[d][2] &&
            y >= ZONA_DECOR[d][1] && y < ZONA_DECOR[d][1] + ZONA_DECOR[d][3])
            return d;
    return -1;
}

/* --- charcos: tócalo dos veces y va a saltar ahí --- */
static int s_charco_i = -1, s_charco_saltos;
static bool s_charco_gusta;
static uint32_t s_charco_llego;

static void charco_cb(lv_event_t *e)
{
    int i = (int)(intptr_t)lv_event_get_user_data(e);
    if (!s_pet || s_pet->dormida || s_juego != JG_NADA) return;
    salpicar(POS_CHARCO[i][0] - 6 + (int)(azar01() * 12), POS_CHARCO[i][1] - 8, 0x9ED8FF);
    { s_toques_n = 0; flotar(ICO_MANO, NULL, 0, POS_CHARCO[i][0], POS_CHARCO[i][1] - 20, 0); accion(ACT_SALPICAR, i); }
}
/* el mapa del mundo: por qué lado se sale de cada zona (0 oeste, 1 este, 2 norte, 3 sur) y a dónde lleva */
static const int8_t VECINO[ESC_COUNT][4] = {
    {-1, ESC_LAGO, -1, ESC_BOSQUE},   /* prado: al este el lago, al sur el bosque */
    {ESC_CLARO, -1, ESC_PRADO, -1},   /* bosque: al norte el prado; al oeste (v98) el sendero de las luciérnagas al claro del rey */
    {ESC_PRADO, ESC_COLINA, -1, -1},  /* lago: al oeste el prado, al este la colina (a la isla se va en el bote o por el hielo) */
    {ESC_LAGO, -1, ESC_CUEVA, -1},    /* colina: al oeste el lago, al norte la cueva (la boca de la montaña) */
    {-1, -1, -1, ESC_COLINA},         /* cueva: al sur la colina */
    {-1, -1, -1, -1},                 /* isla */
    {-1, ESC_BOSQUE, -1, -1},         /* (v98) claro del rey: al este, el sendero de las luciérnagas vuelve al bosque */
    {-1, -1, -1, ESC_COLINA},         /* (v98) mirador: al sur, el sendero baja por el lomo hasta el cerro (se sube en la nube) */
};
/* el punto de la pantalla por donde cruza el borde (los caminos de cada fondo llegan ahí); {0,0} = no hay salida por ese lado */
static const int16_t PUNTO_BORDE[ESC_COUNT][4][2] = {
    {{0, 0}, {483, 392}, {0, 0}, {246, 486}},
    {{-17, 300}, {0, 0}, {246, -20}, {0, 0}},
    {{-17, 392}, {483, 392}, {246, -20}, {0, 0}},
    {{-17, 392}, {0, 0}, {236, 214}, {0, 0}},
    {{0, 0}, {0, 0}, {0, 0}, {236, 486}},
    {{0, 0}, {0, 0}, {0, 0}, {246, 486}},
    {{0, 0}, {483, 309}, {0, 0}, {0, 0}},
    {{0, 0}, {0, 0}, {0, 0}, {236, 486}},
};
static int lado_hacia(int de, int a) /* por qué lado sale para llegar a una zona (aunque no sea vecina: el primer paso del camino) */
{
    int prev[ESC_COUNT], cola[ESC_COUNT], n = 0, h = 0;
    for (int i = 0; i < ESC_COUNT; i++) prev[i] = -2;
    if ((unsigned)de >= ESC_COUNT || (unsigned)a >= ESC_COUNT) return 1;
    if (de == ESC_LAGO && a == ESC_ISLA) return 2; /* por el hielo: hacia el norte */
    if (de == ESC_ISLA && a == ESC_LAGO) return 3; /* y de vuelta, hacia el sur */
    if (a == ESC_MIRADOR && de != ESC_COLINA) a = ESC_COLINA; /* (v98) al mirador se va por el cerro (y ahí, en la nube) */
    prev[de] = -1; cola[n++] = de;
    while (h < n) {
        int u = cola[h++];
        for (int d = 0; d < 4; d++) { int v = VECINO[u][d]; if (v >= 0 && prev[v] == -2) { prev[v] = u; cola[n++] = v; } }
    }
    if (prev[a] == -2) return a > de ? 1 : 0;
    int v = a;
    while (prev[v] != de && prev[v] >= 0) v = prev[v];
    for (int d = 0; d < 4; d++) if (VECINO[de][d] == v) return d;
    return a > de ? 1 : 0;
}
static void punto_borde(int escena, int lado, float *x, float *y)
{
    const int16_t *q = PUNTO_BORDE[(unsigned)escena < ESC_COUNT ? escena : 0][lado & 3];
    if (q[0] == 0 && q[1] == 0) { q = PUNTO_BORDE[2][lado == 3 || lado == 1 ? 1 : 0]; if (lado == 2) q = PUNTO_BORDE[1][2]; }
    *x = q[0]; *y = q[1];
}
static lv_obj_t *s_flecha[4], *s_flecha_zona[4], *s_flecha_nueva[4];
static int s_flecha_dest[4] = {-1, -1, -1, -1};

typedef struct {
    lv_obj_t *img;
    bool visible, saliendo, izq;
    float x, y, tx, ty, fase;
    uint32_t proximo, quieto_desde;
    float ult_x, ult_y;
} bicho_t;
static bicho_t s_fauna[FAUNA_COUNT];
static uint32_t s_fauna_vista[FAUNA_COUNT]; /* desde cuándo está a la vista */


static void parcela_cb(lv_event_t *e)
{
    int i = (int)(intptr_t)lv_event_get_user_data(e);
    if (!s_pet || i < 0 || i >= PET_PARCELAS_TOTAL) return;
    const planta_t *pl = pet_plant(s_pet, i);
    if (pl->tipo == PLANTA_NADA) {
        if (s_pet->inventario[ITEM_SEMILLA]) { panel_semilla(i); return; } /* elegir qué plantar */
        accion(ACT_PLANTAR, i);
    } else {
        if (toques_seguidos(2000 + i) >= 3) { /* solo se saca si ya se secó: una viva sigue creciendo */
            if (pl->agua <= PET_AGUA_SECA) mostrar_quitar(QUITAR_PLANTA, i, POS_PARCELA[i][0] + 22, POS_PARCELA[i][1] - 50);
            else globo(ICO_FLOR, s_nino ? NULL : "¡TODAVÍA ESTÁ VIVA! SE SACA CUANDO SE SEQUE", 2500);
            return;
        }
        regadera_empezar(i, POS_PARCELA[i][0], POS_PARCELA[i][1]); /* lleva la regadera hasta la planta y la riega */
    }
}

static void fauna_cb(lv_event_t *e)
{
    int f = (int)(intptr_t)lv_event_get_user_data(e);
    bicho_t *b = &s_fauna[f];
    if (!b->visible || b->saliendo) return;
    if (f == FAUNA_ZORRO) flotar(ICO_MANO, NULL, 0, (int)b->x, (int)b->y - 30, 0);
    accion(UI_ACCION_ANIMAL_TOCAR, f); /* (v39) si ya lo conoce, a lo mejor le cuenta algo del Rey */
    { /* (fw 103) a la abeja, insistir cuesta caro */
        const int veces = toques_animal(5100 + f);
        s_toq_res = PET_TOC_NADA;
        accion(UI_ACCION_TOCAR_FAUNA, f | (veces > 255 ? 255 : veces) << 8);
        if (s_toq_res > PET_TOC_NADA) toque_mostrar(s_toq_res, b->x, b->y);
        if (s_toq_res > PET_TOC_AVISA) return;
    }
    accion(ACT_ESPANTAR, f);
}

static void hojas_cb(lv_event_t *e)
{
    (void)e;
    if (!s_pet || s_pet->dormida) return;
    int x = HOJAS_X, y = HOJAS_Y;
    for (int k = 0; k < 6; k++) salpicar(x - 15 + k * 6, y - 10, k & 1 ? 0xE86A3A : 0xF2A33A);
    s_llamado_x = (float)x - 10; s_llamado_y = (float)y + 4;
    accion(ACT_LLAMAR, LUGAR_ARBOL);
}

static void bote_ir(int destino);
static void fondo_desliza_cb(lv_event_t *e);
static int s_desliza_x0, s_desliza_y0;
static void flecha_cb(lv_event_t *e)
{
    int i = (int)(intptr_t)lv_event_get_user_data(e);
    if (s_flecha_dest[i] < 0) return;
    { /* si el dedo venía deslizando (para ver sus estados o el libro), no es un toque a la flecha */
        lv_indev_t *in = lv_indev_active();
        lv_point_t p;
        if (in) { lv_indev_get_point(in, &p); if (abs(p.x - s_desliza_x0) > 30 || abs(p.y - s_desliza_y0) > 30) return; }
    }
    if (s_escena == ESC_ISLA && !(s_pet && lago_congelado(s_pet))) { bote_ir(s_flecha_dest[i]); return; } /* de la isla se vuelve en el bote: camina por el muelle y parte */
    accion(ACT_IR, s_flecha_dest[i]);
}

static void crear_mundo_vivo(void)
{
    for (int i = 0; i < PET_PARCELAS_TOTAL; i++) {
        lv_obj_t *t = caja(s_mundo); /* tierra de la parcela */
        lv_obj_set_size(t, 34, 14);
        lv_obj_set_pos(t, POS_PARCELA[i][0] - 17, POS_PARCELA[i][1] - 9);
        lv_obj_set_style_bg_color(t, lv_color_hex(0x7A5234), 0);
        lv_obj_set_style_bg_opa(t, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(t, 7, 0);
        lv_obj_set_style_border_width(t, 2, 0);
        lv_obj_set_style_border_color(t, lv_color_hex(0x4E3220), 0);
        lv_obj_add_flag(t, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_ext_click_area(t, 10);
        lv_obj_add_event_cb(t, parcela_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        s_parcela[i] = t;
        ocultar(t, true);
    }
    for (int i = 0; i < PET_PARCELAS_TOTAL; i++) {
        s_planta[i] = lv_image_create(s_mundo);
        lv_obj_add_flag(s_planta[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(s_planta[i], parcela_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        s_planta_clave[i] = 0xFFFFFFFFu;
        ocultar(s_planta[i], true);
    }
    for (int i = 0; i < PET_PARCELAS_TOTAL; i++) /* montoncitos de nieve que cubren las plantas (abajo el sombreado, arriba lo blanco) */
        for (int k = 0; k < 2; k++) {
            lv_obj_t *n = caja(s_mundo);
            lv_obj_set_style_bg_color(n, lv_color_hex(k ? 0xF6FAFF : 0xC4D4E8), 0);
            lv_obj_set_style_bg_opa(n, LV_OPA_COVER, 0);
            lv_obj_set_style_radius(n, LV_RADIUS_CIRCLE, 0);
            lv_obj_remove_flag(n, LV_OBJ_FLAG_CLICKABLE);
            ocultar(n, true);
            s_nieve_pl[i][k] = n;
        }
    for (int i = 0; i < 2; i++) { /* un charco plano en el suelo: ella pasa por encima */
        lv_obj_t *c = caja(s_mundo);
        int w = i ? 58 : 70, h = i ? 18 : 22;
        lv_obj_set_size(c, w, h);
        lv_obj_set_pos(c, POS_CHARCO[i][0] - w / 2, POS_CHARCO[i][1] - h / 2 - 4);
        lv_obj_set_style_radius(c, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(c, lv_color_hex(0x6FA8E0), 0);
        lv_obj_set_style_bg_grad_color(c, lv_color_hex(0x9ED8FF), 0);
        lv_obj_set_style_bg_grad_dir(c, LV_GRAD_DIR_VER, 0);
        lv_obj_set_style_bg_opa(c, LV_OPA_80, 0);
        lv_obj_set_style_border_width(c, 2, 0);
        lv_obj_set_style_border_color(c, lv_color_hex(0x4F86C6), 0);
        lv_obj_set_style_border_opa(c, LV_OPA_70, 0);
        lv_obj_t *b = caja(c); /* brillo del cielo en el agua */
        lv_obj_set_size(b, w / 3, 4);
        lv_obj_set_pos(b, w / 5, 3);
        lv_obj_set_style_radius(b, 2, 0);
        lv_obj_set_style_bg_color(b, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_bg_opa(b, LV_OPA_70, 0);
        lv_obj_remove_flag(b, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(c, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_ext_click_area(c, 10);
        lv_obj_add_event_cb(c, charco_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        lv_obj_move_to_index(c, 0); /* en el suelo, detrás de ella */
        s_charco[i] = c;
        ocultar(c, true);
    }
    s_hojas = lv_image_create(s_mundo);
    lv_obj_add_flag(s_hojas, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_hojas, hojas_cb, LV_EVENT_CLICKED, NULL);
    ocultar(s_hojas, true);
}

static void crear_fauna(void)
{
    for (int f = 0; f < FAUNA_COUNT; f++) {
        bicho_t *b = &s_fauna[f];
        b->img = lv_image_create(s_mundo);
        lv_image_set_src(b->img, art_icono(ICO_FAUNA[f], 3));
        lv_obj_add_flag(b->img, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_ext_click_area(b->img, 14);
        lv_obj_add_event_cb(b->img, fauna_cb, LV_EVENT_CLICKED, (void *)(intptr_t)f);
        b->fase = (float)f * 1.7f;
        ocultar(b->img, true);
    }
}

static void crear_flechas(lv_obj_t *scr)
{
    static const int ROT[4] = {1800, 0, 2700, 900}; /* oeste, este, norte, sur */
    for (int i = 0; i < 4; i++) {
        const bool vert = i >= 2; /* norte y sur: el botón es ancho y va en el borde de arriba o de abajo */
        lv_obj_t *b = boton(scr, vert ? 78 : 44, vert ? 44 : 78, flecha_cb, i);
        lv_obj_set_style_bg_opa(b, LV_OPA_50, 0);
        lv_obj_set_style_radius(b, 20, 0);
        if (i < 2) lv_obj_set_pos(b, i == 0 ? 8 : W - 52, 196);
        else lv_obj_set_pos(b, CX - 39, i == 2 ? 44 : 342); /* la de abajo va justo sobre los botones, no encima */
        lv_obj_t *f = icono(b, ICO_FLECHA, 2);
        if (vert) lv_obj_align(f, LV_ALIGN_LEFT_MID, 10, 0); else lv_obj_align(f, LV_ALIGN_TOP_MID, 0, 8);
        if (ROT[i]) { lv_image_set_pivot(f, 12, 12); lv_image_set_rotation(f, ROT[i]); }
        s_flecha_zona[i] = icono(b, ICO_ZONA_PRADO, 2);
        if (vert) lv_obj_align(s_flecha_zona[i], LV_ALIGN_RIGHT_MID, -10, 0); else lv_obj_align(s_flecha_zona[i], LV_ALIGN_BOTTOM_MID, 0, -10);
        s_flecha_nueva[i] = icono(b, ICO_ESTRELLA, 2); /* una zona que todavía no visita */
        lv_obj_align(s_flecha_nueva[i], LV_ALIGN_TOP_RIGHT, 0, -2);
        ocultar(s_flecha_nueva[i], true);
        s_flecha[i] = b;
        lv_obj_add_event_cb(b, fondo_desliza_cb, LV_EVENT_PRESSED, NULL); /* un deslizar que empieza sobre la flecha también vale (y no la pisa) */
        lv_obj_add_event_cb(b, fondo_desliza_cb, LV_EVENT_RELEASED, NULL);
        ocultar(b, true);
    }
}

/* flechas: una por cada salida de la zona (oeste, este, norte, sur) hacia una zona ya abierta */
static void actualizar_flechas(const pet_t *p)
{
    for (int i = 0; i < 4; i++) {
        int d = -1;
        if (p->etapa != STAGE_HUEVO && !p->dormida && !s_interior && s_escena == (int)p->escena && s_juego == JG_NADA && s_panel == PANEL_NINGUNO && !s_picnic_on && (unsigned)p->escena < ESC_COUNT) { /* (dentro de la casita no hay flechas a otras zonas) */
            int v = VECINO[p->escena][i];
            if (v >= 0 && pet_scene_open(p, (escena_t)v)) d = v; /* a la isla se va en el bote (o por el hielo) */
            const bool hielo = lago_congelado(p);
            if (hielo && p->escena == ESC_LAGO && i == 2 && pet_scene_open(p, ESC_ISLA)) d = ESC_ISLA; /* con el lago helado: la isla se alcanza caminando por el hielo */
            if (p->escena == ESC_ISLA && !hielo && i == 0) d = ESC_LAGO; /* en la isla, una flecha a la izquierda: volver en el bote (además de tocar el bote) */
            if (p->escena == ESC_ISLA && hielo && i == 3) d = ESC_LAGO; /* con el lago helado se vuelve caminando por el hielo */
            if (p->escena == ESC_COLINA && i == 2) d = -1; /* (v97) a la cueva no hay botón: se entra tocando la boca, que brilla */
        }
        if (d != s_flecha_dest[i] && d >= 0) poner_src(s_flecha_zona[i], art_icono(ICO_ZONA[d], 2));
        s_flecha_dest[i] = d;
        ocultar(s_flecha[i], d < 0);
        if (i == 3) lv_obj_set_y(s_flecha[i], p->escena == ESC_CUEVA ? 318 : 342); /* en la cueva, más arriba que en el resto pero pegada a los íconos de abajo (comer, jugar, más): termina justo donde ellos empiezan */
        bool nueva = d >= 0 && !pet_scene_seen(p, (escena_t)d);
        ocultar(s_flecha_nueva[i], !nueva);
        lv_obj_set_style_bg_color(s_flecha[i], lv_color_hex(nueva ? 0xFFB020 : C_BOTON), 0);
    }
}

static void actualizar_jardin(const pet_t *p)
{
    bool prado = s_escena == ESC_PRADO && !s_interior;
    for (int i = 0; i < PET_PARCELAS_TOTAL; i++) {
        const planta_t *pl = pet_plant(p, i);
        bool aqui = !s_interior && (int)pet_plant_scene(i) == s_escena && p->etapa != STAGE_HUEVO;
        ocultar(s_parcela[i], !aqui);
        bool hay = aqui && pl->tipo != PLANTA_NADA;
        ocultar(s_planta[i], !hay);
        const bool frio_ya = zona_fria(p);
        lv_obj_set_style_bg_color(s_parcela[i], lv_color_hex(frio_ya ? 0xE6EDF6 : 0x7A5234), 0); /* la tierra del huerto, bajo la nieve */
        lv_obj_set_style_border_color(s_parcela[i], lv_color_hex(frio_ya ? 0xAFC0D6 : 0x4E3220), 0);
        const bool nevada = hay && frio_ya;
        for (int k = 0; k < 2; k++) ocultar(s_nieve_pl[i][k], !nevada);
        if (nevada) { /* la nieve cubre lo que plantaste: los árboles quedan con un gorro, lo bajito desaparece bajo un montón */
            const lv_image_dsc_t *pd = (const lv_image_dsc_t *)lv_image_get_src(s_planta[i]);
            int pw = pd ? (int)pd->header.w : 24, ph = pd ? (int)pd->header.h : 24;
            bool alta = pl->tipo == PLANTA_ARBOL && pl->etapa >= ETAPA_GRANDE;
            int w = alta ? pw * 7 / 10 : pw + 10, h = alta ? 16 : (ph < 18 ? 14 : ph * 2 / 3 + 6);
            int bx = POS_PARCELA[i][0], by = alta ? POS_PARCELA[i][1] - ph + 20 : POS_PARCELA[i][1] + 2;
            for (int k = 0; k < 2; k++) {
                int ww = k ? w - 6 : w, hh = k ? h - 4 : h;
                lv_obj_set_size(s_nieve_pl[i][k], ww, hh);
                lv_obj_set_pos(s_nieve_pl[i][k], bx - ww / 2, by - hh + (k ? -1 : 3));
            }
        }
        if (!hay) continue;
        art_icono_t ico = ICO_SEMILLA;
        int esc = 3;
        if (pl->etapa == ETAPA_BROTE) ico = ICO_BROTE;
        else if (pl->etapa >= ETAPA_GRANDE) {
            if (pl->tipo == PLANTA_FLOR) ico = ICO_FLOR;
            else if (pl->tipo == PLANTA_GIRASOL) { ico = ICO_GIRASOL; esc = 4; }
            else if (pl->tipo == PLANTA_FRUTILLA) ico = ICO_FRUTILLA;
            else if (pl->tipo == PLANTA_HIERBA) ico = ICO_HIERBA;
            else { ico = pl->etapa == ETAPA_FRUTOS ? ICO_ARBOL_FRUTAL : ICO_ARBOLITO; esc = 4; }
        }
        if (pl->mustia && pl->tipo == PLANTA_FLOR && pl->etapa >= ETAPA_BROTE) ico = ICO_FLOR_MUSTIA;
        if (pl->etapa == ETAPA_SEMILLA) esc = 2;
        bool seca = pl->agua <= PET_AGUA_SECA;
        if (seca && pl->tipo != PLANTA_ARBOL) { ico = ICO_PLANTA_SECA; esc = pl->etapa >= ETAPA_GRANDE ? 4 : 3; }
        const bool amarillas = s_pet && pet_yellow_flowers_day(s_pet, s_ultimo_t); /* el Día de las Flores Amarillas: todas amanecen amarillas */
        const int color = amarillas ? 1 : pl->color;
        uint32_t clave = (uint32_t)ico | (uint32_t)esc << 8 | (uint32_t)color << 12 | (uint32_t)pl->mustia << 16 | (uint32_t)pl->etapa << 18 |
                         (uint32_t)seca << 21 | (uint32_t)i << 22;
        if (clave == s_planta_clave[i]) continue;
        s_planta_clave[i] = clave;
        const lv_image_dsc_t *d = art_icono(ico, esc);
        poner_src(s_planta[i], d);
        lv_obj_set_pos(s_planta[i], POS_PARCELA[i][0] - (int)d->header.w / 2, POS_PARCELA[i][1] - (int)d->header.h + 2);
        /* el color de cada flor */
        static const uint32_t TONO[4] = {0, 0xFFD45C, 0xB48CFF, 0xFF7A5A};
        bool tinte = ico == ICO_FLOR && (color & 3);
        lv_obj_set_style_image_recolor(s_planta[i], lv_color_hex(TONO[color & 3]), 0);
        lv_obj_set_style_image_recolor_opa(s_planta[i], tinte ? (amarillas ? LV_OPA_80 : LV_OPA_40) : LV_OPA_TRANSP, 0);
        if (pl->tipo == PLANTA_FRUTILLA && pl->etapa == ETAPA_GRANDE) { /* todavía verdes */
            lv_obj_set_style_image_recolor(s_planta[i], lv_color_hex(0x7ED37A), 0);
            lv_obj_set_style_image_recolor_opa(s_planta[i], LV_OPA_50, 0);
        }
        if (pl->mustia && pl->tipo != PLANTA_FLOR && !seca) {
            lv_obj_set_style_image_recolor(s_planta[i], lv_color_hex(0x9C8A5A), 0);
            lv_obj_set_style_image_recolor_opa(s_planta[i], seca ? LV_OPA_70 : LV_OPA_50, 0);
        }
        if (seca && pl->tipo == PLANTA_ARBOL) {
            lv_obj_set_style_image_recolor(s_planta[i], lv_color_hex(0x8C6A3A), 0);
            lv_obj_set_style_image_recolor_opa(s_planta[i], LV_OPA_60, 0);
        }
    }
    for (int i = 0; i < 2; i++) ocultar(s_charco[i], !(prado && pet_puddles(p, s_ultimo_t)));
    bool hojas = prado && p->hojas > 0;
    ocultar(s_hojas, !hojas);
    if (hojas) {
        const lv_image_dsc_t *d = art_icono(ICO_HOJAS, p->hojas >= 3 ? 5 : (p->hojas == 2 ? 4 : 3));
        poner_src(s_hojas, d);
        lv_obj_set_pos(s_hojas, HOJAS_X - (int)d->header.w / 2, HOJAS_Y - (int)d->header.h);
    }
}

/* dónde le gusta estar a cada animal */
static void fauna_destino(int f, bicho_t *b)
{
    switch (f) {
    case FAUNA_MARIPOSA: case FAUNA_ABEJA: {
        float cx = CX, cy = 230;
        if (s_escena == ESC_PRADO) { cx = POS_PARCELA[1][0]; cy = POS_PARCELA[1][1]; }
        b->tx = cx + (azar01() * 2 - 1) * 110; b->ty = cy - 20 + (azar01() * 2 - 1) * 50;
        break;
    }
    case FAUNA_PAJARO: /* se posa cerca del árbol */
        b->tx = LXY(LUGAR_ARBOL)[0] + (azar01() * 2 - 1) * 50; b->ty = LXY(LUGAR_ARBOL)[1] + 10 + azar01() * 20;
        break;
    case FAUNA_ZORRO: /* merodea cerca del rincón (donde guarda la fruta) */
        b->tx = LXY(LUGAR_CASITA)[0] + (azar01() * 2 - 1) * 70; b->ty = LXY(LUGAR_CASITA)[1] + 30 + azar01() * 50;
        break;
    default: { /* la rana salta por toda la orilla y el prado del lago */
        b->tx = 40.0f + azar01() * (W - 80.0f);
        const float ym = (float)terreno_ymin(b->tx) + 6.0f;
        b->ty = ym + azar01() * ((float)Y_MAX - ym);
        break;
    }
    }
    if (f != FAUNA_MARIPOSA && f != FAUNA_ABEJA) limitar(&b->tx, &b->ty);
}

static void fauna_sale(int f)
{
    bicho_t *b = &s_fauna[f];
    if (!b->visible || b->saliendo) return;
    b->saliendo = true;
    bool vuela = f == FAUNA_MARIPOSA || f == FAUNA_ABEJA || f == FAUNA_PAJARO;
    b->tx = b->x < CX ? -60 : W + 60;
    b->ty = vuela ? b->y - 160 : b->y;
}

static void esconder_fauna(void)
{
    for (int f = 0; f < FAUNA_COUNT; f++) { s_fauna[f].visible = s_fauna[f].saliendo = false; ocultar(s_fauna[f].img, true); }
}

static void animar_fauna(void)
{
    const pet_t *p = s_pet;
    if (!p) return;
    for (int f = 0; f < FAUNA_COUNT; f++) {
        bicho_t *b = &s_fauna[f];
        bool aqui = s_escena == (int)p->escena && pet_fauna_here(p, (fauna_t)f, s_ultimo_t) && p->etapa != STAGE_HUEVO && !s_interior;
        if (aqui && (!b->visible || b->saliendo)) { /* llega desde un borde */
            if (!b->visible) {
                bool izq = azar() & 1;
                b->x = izq ? -30 : W + 30;
                b->y = f == FAUNA_ZORRO ? 260 : 140 + azar01() * 60;
                if (f == FAUNA_RANA) b->y = 230 + azar01() * 80; /* llega por un borde, como los demás */
                fauna_destino(f, b);
                b->proximo = s_ms + 2500;
            }
            b->visible = true; b->saliendo = false;
            ocultar(b->img, false);
        }
        if (!aqui && b->visible) fauna_sale(f);
        if (!b->visible) { s_fauna_vista[f] = 0; continue; }
        if (s_panel == PANEL_NINGUNO && b->x > 20 && b->x < W - 20 && s_pantalla_visible) { /* lo vieron juntos: unos segundos a la vista */
            if (!s_fauna_vista[f]) s_fauna_vista[f] = s_ms;
            else if (s_ms - s_fauna_vista[f] > 3000 && s_ms - s_fauna_vista[f] < 3100) accion(UI_ACCION_ANIMAL_VISTO, pet_card_of_fauna((fauna_t)f));
        } else s_fauna_vista[f] = 0;

        bool perseguida = !b->saliendo && p->actividad == ACTV_PERSEGUIR && p->persigue == f && !p->dormida;
        float vel = f == FAUNA_ZORRO ? (b->saliendo ? 9.0f : 2.2f) : (f == FAUNA_RANA ? 3.0f : (b->saliendo ? 7.0f : 3.2f));
        if (perseguida) { /* escapa un poquito delante de ella */
            vel *= 1.4f;
            if (hypotf(b->x - s_mascota.x, b->y - (s_mascota.y - 40)) < 70 || (int32_t)(s_ms - b->proximo) > 0) {
                b->tx = s_mascota.x + (azar01() < 0.5f ? -1 : 1) * (90 + azar01() * 60);
                b->ty = s_mascota.y - 20 - azar01() * 40;
                limitar(&b->tx, &b->ty);
                b->ty -= 40;
                b->proximo = s_ms + 1800;
            }
        } else if (!b->saliendo && (int32_t)(s_ms - b->proximo) > 0) {
            fauna_destino(f, b);
            b->proximo = s_ms + (f == FAUNA_PAJARO ? 3500 : (f == FAUNA_RANA ? 2200 : 1800)) + azar() % 2500;
        }
        float dx = b->tx - b->x, dy = b->ty - b->y, d = hypotf(dx, dy);
        bool anda = d > 1.0f;
        if (anda && !b->saliendo) { /* no avanza (el destino quedó fuera de su alcance): nuevo destino */
            if (hypotf(b->x - b->ult_x, b->y - b->ult_y) > 2.0f) { b->ult_x = b->x; b->ult_y = b->y; b->quieto_desde = s_ms; }
            else if (s_ms - b->quieto_desde > 2500u) { fauna_destino(f, b); b->quieto_desde = s_ms; }
        } else { b->ult_x = b->x; b->ult_y = b->y; b->quieto_desde = s_ms; }
        if (d < vel) { b->x = b->tx; b->y = b->ty; } else { b->x += dx / d * vel; b->y += dy / d * vel; }
        if (b->saliendo && !anda) { b->visible = b->saliendo = false; ocultar(b->img, true); continue; }

        /* aleteo y saltitos */
        b->fase += 0.25f;
        float oy = 0;
        art_icono_t ico = ICO_FAUNA[f];
        switch (f) {
        case FAUNA_MARIPOSA: oy = sinf(b->fase) * 6; if ((s_ms / 160) % 2) ico = ICO_MARIPOSA_B; break;
        case FAUNA_ABEJA: oy = sinf(b->fase * 2.3f) * 3; if ((s_ms / 80) % 2) ico = ICO_ABEJA_B; break;
        case FAUNA_PAJARO:
            if (anda || b->saliendo) { if ((s_ms / 120) % 2) ico = ICO_PAJARO_B; oy = -fabsf(sinf(b->fase)) * 4; }
            else if ((s_ms % 1600) < 300) oy = -5; /* picotea */
            break;
        case FAUNA_RANA: if (anda) oy = -fabsf(sinf(b->fase * 1.5f)) * 14; break;
        default: if (anda) oy = ((s_ms / 140) % 2) ? -2 : 0; break;
        }
        /* mira hacia donde va (los íconos miran a la derecha) */
        if (fabsf(dx) > 1.0f) b->izq = dx < 0;
        const lv_image_dsc_t *src = b->izq && f != FAUNA_MARIPOSA ? art_icono_espejo(ico, 3) : art_icono(ico, 3);
        if (lv_image_get_src(b->img) != src) lv_image_set_src(b->img, src);
        lv_obj_set_pos(b->img, (int)b->x - 18, (int)(b->y + oy) - 30);
    }

    /* si lleva rato en el mismo punto detrás del animal (una esquina sin salida), deja de perseguirlo un rato */
    static float ult_x, ult_y; static uint32_t quieta_desde, sin_perseguir_hasta;
    if (hypotf(s_mascota.x - ult_x, s_mascota.y - ult_y) > 3.0f) { ult_x = s_mascota.x; ult_y = s_mascota.y; quieta_desde = s_ms; }
    const bool atascada = p->actividad == ACTV_PERSEGUIR && s_ms - quieta_desde > 3500u;
    if (atascada) {
        sin_perseguir_hasta = s_ms + 20000u; quieta_desde = s_ms;
        s_mascota.tx = LXY(LUGAR_CLARO)[0]; s_mascota.ty = LXY(LUGAR_CLARO)[1]; limitar(&s_mascota.tx, &s_mascota.ty);
        for (int f = 0; f < FAUNA_COUNT; f++) if (s_fauna[f].visible && !s_fauna[f].saliendo) fauna_sale(f); /* el animal se va */
    }
    /* la mascota corre detrás del animal que persigue */
    if ((int32_t)(s_ms - sin_perseguir_hasta) > 0 && p->actividad == ACTV_PERSEGUIR && !p->dormida && p->persigue < FAUNA_COUNT && s_fauna[p->persigue].visible &&
        s_mascota.modo == M_LIBRE) {
        bicho_t *b = &s_fauna[p->persigue];
        s_mascota.tx = b->x;
        s_mascota.ty = b->y + (p->persigue == FAUNA_RANA || p->persigue == FAUNA_ZORRO ? 10 : 70);
        limitar(&s_mascota.tx, &s_mascota.ty);
    }
}

/* ======================================================================
 * Fase 3: su amigo del barrio, la pesca, la casita por dentro y tu cumpleaños
 * ==================================================================== */

static bool mover(criatura_t *c, float vel);

/* --- su amigo del barrio --- */
static criatura_t s_vecino;
static lv_obj_t *s_pelota2, *s_objeto2;
static float s_juntos_x, s_juntos_y;
static int s_vec_act = -1;
static uint32_t s_vec_act_desde;
static bool s_vec_sale;

static void vecino_cb(lv_event_t *e)
{
    (void)e;
    if (!s_vecino.visible || s_vec_sale) return;
    flotar(ICO_CORAZON, NULL, 0, (int)s_vecino.x, cabeza_y(&s_vecino), 0);
    accion(ACT_MIMAR_VISITA, 0);
}

static void vecino_entra(const pet_t *p, bool caminando)
{
    criatura_t *v = &s_vecino, *c = &s_mascota;
    v->pose.especie = p->vecino.especie | (p->vecino.color & 7) << 7; /* cada amigo con sus colores */
    v->pose.etapa = p->etapa < STAGE_BEBE ? STAGE_BEBE : p->etapa;
    v->pose.forma = 0;
    v->pose.escala = 4;
    s_juntos_x = c->x < 140 ? 140 : (c->x > 326 ? 326 : c->x);
    s_juntos_y = c->y < 220 ? 220 : (c->y > 300 ? 300 : c->y);
    bool desde_der = c->x < CX;
    v->tx = s_juntos_x + (desde_der ? 80 : -80);
    v->ty = s_juntos_y;
    limitar(&v->tx, &v->ty);
    v->x = caminando ? (desde_der ? W + 30 : -30) : v->tx;
    v->y = v->ty;
    v->modo = caminando ? M_CAMINA : M_QUIETA;
    v->clave = 0xFFFFFFFFu;
    s_vec_sale = false;
    s_vec_act = -1;
    mostrar_criatura(v, true);
}

static void vecino_sale(void)
{
    if (!s_vecino.visible || s_vec_sale) return;
    s_vec_sale = true;
    s_vecino.tx = s_vecino.x < CX ? -40 : W + 40;
    s_vecino.ty = s_vecino.y;
    ocultar(s_pelota2, true);
    ocultar(s_objeto2, true);
}

static void esconder_vecino(void)
{
    mostrar_criatura(&s_vecino, false);
    s_vec_sale = false;
    ocultar(s_pelota2, true);
    ocultar(s_objeto2, true);
    if (s_vec_mantel) ocultar(s_vec_mantel, true);
}

/* posiciones de los dos en cada actividad: la mascota a la izquierda del punto de encuentro */
static void juntos_objetivos(int act, float *mx, float *my, float *vx, float *vy)
{
    float cx = s_juntos_x, cy = s_juntos_y;
    *mx = cx - 55; *my = cy; *vx = cx + 55; *vy = cy;
    if (act == VEC_JUGAR) { *mx = cx - 85; *vx = cx + 85; }
    if (act == VEC_SIESTA) { *mx = cx - 32; *vx = cx + 32; }
    if (act == VEC_COMER) { *mx = cx - 38; *vx = cx + 38; } /* se sientan los dos DENTRO del mantel */
    if (act == VEC_CORRER) { /* dan vueltas persiguiéndose */
        float a = (float)s_ms / 650.0f;
        *mx = cx + cosf(a) * 95; *my = cy + sinf(a) * 34;
        *vx = cx + cosf(a - 1.1f) * 95; *vy = cy + sinf(a - 1.1f) * 34;
    }
    if (act == VEC_REGALO && s_ms - s_vec_act_desde < 2200) { *vx = cx - 5; } /* se acerca a dárselo */
    limitar(mx, my);
    limitar(vx, vy);
}

/* el juego de la pelota con un amigo de visita: si lo dejas solo un ratito, se la pasan entre ellos */
static bool s_pelo_pasa;
static uint32_t s_pelo_pasa_desde;
static int s_pelo_pasa_n;

/* la mascota con su amigo: devuelve true si se encargó del cuadro */
static bool animar_juntos(criatura_t *c, const pet_t *p, float *sx, float *sy, bool *caminando)
{
    if (!s_vecino.visible || s_vec_sale || p->actividad != ACTV_JUNTOS) return false;
    int act = p->vecino.actividad;
    float mx, my, vx, vy;
    juntos_objetivos(act, &mx, &my, &vx, &vy);
    c->tx = mx; c->ty = my;
    art_pose_t *ps = &c->pose;
    *caminando = mover(c, act == VEC_CORRER ? 6.0f : 3.5f);
    if (!*caminando) ps->espejo = false; /* lo mira a él */
    ps->expr = EXP_FELIZ; ps->cola = 1; ps->orejas = 1;
    uint32_t t = s_ms - s_vec_act_desde;
    switch (act) {
    case VEC_JUGAR: *sy = -fabsf(sinf((float)s_ms / 160.0f)) * 8; ps->boca = true; break;
    case VEC_CORRER: *sy = -fabsf(sinf((float)s_ms / 90.0f)) * 6; ps->boca = true; *caminando = true; break;
    case VEC_COMER: ps->boca = (s_ms / 260) % 2; break;
    case VEC_CHARLAR: ps->boca = ((s_ms / 1500) % 2 == 0) && (s_ms / 180) % 2; break; /* hablan por turnos */
    case VEC_SIESTA:
        ps->expr = EXP_CERRADOS; ps->postura = -2; ps->orejas = -1;
        if (s_ms % 2400 < TICK_MS) flotar(ICO_ZETA, NULL, 0, (int)c->x + 20, cabeza_y(c) + 20, 0);
        break;
    case VEC_REGALO: if (t > 2200 && t < 3200) *sy = -fabsf(sinf((float)s_ms / 100.0f)) * 12; break;
    default: break;
    }
    return true;
}

static void animar_vecino(void)
{
    criatura_t *v = &s_vecino;
    const pet_t *p = s_pet;
    if (!v->visible || !p) return;
    float sx = 0, sy = 0;
    bool camina;
    art_pose_t *ps = &v->pose;
    ps->expr = EXP_FELIZ; ps->cola = 1; ps->orejas = 1; ps->postura = 0; ps->boca = false;
    if (s_vec_sale) {
        camina = mover(v, 4.0f);
        if (!camina) { esconder_vecino(); return; }
    } else if (v->modo == M_CAMINA) {
        camina = mover(v, 3.8f);
        if (!camina) { v->modo = M_QUIETA; flotar(ICO_CORAZON, NULL, 0, (int)v->x, cabeza_y(v), 0); }
    } else {
        int act = p->vecino.actividad;
        const bool pelota_tuya = s_juego == JG_PELOTA && !s_pelo_pasa;
        if (s_juego == JG_PELOTA) act = s_pelo_pasa ? VEC_JUGAR : VEC_LLEGA; /* o se la pasan, o mira cómo juegas */
        if (act != s_vec_act) { /* empieza algo nuevo */
            s_vec_act = act;
            s_vec_act_desde = s_ms;
            ocultar(s_pelota2, act != VEC_JUGAR);
            ocultar(s_objeto2, act != VEC_COMER && act != VEC_REGALO);
            if (s_vec_mantel) ocultar(s_vec_mantel, act != VEC_COMER);
            if (act == VEC_REGALO) lv_image_set_src(s_objeto2, art_icono(ICO_REGALO, 3));
        }
        float mx, my, vx, vy;
        juntos_objetivos(act, &mx, &my, &vx, &vy);
        if (pelota_tuya) { /* se hace a un lado y anima */
            static bool izq = true;
            if (fabsf(s_mascota.x - (izq ? 120.0f : 340.0f)) < 90) izq = !izq;
            vx = izq ? 120.0f : 340.0f; vy = 290;
        }
        v->tx = vx; v->ty = vy;
        camina = mover(v, act == VEC_CORRER ? 6.0f : 3.5f);
        if (!camina) ps->espejo = s_mascota.x < v->x; /* la mira */
        if (pelota_tuya && !camina) { sy = -fabsf(sinf((float)s_ms / 220.0f)) * 4; ps->boca = (s_ms / 700) % 2; }
        uint32_t t = s_ms - s_vec_act_desde;
        switch (act) {
        case VEC_JUGAR: { /* la pelota va y viene en arco */
            float k = (float)(s_ms % 1600) / 800.0f;
            bool ida = k < 1.0f;
            float u = ida ? k : k - 1.0f;
            float x0 = s_mascota.x, x1 = v->x;
            float bx = ida ? x0 + (x1 - x0) * u : x1 + (x0 - x1) * u;
            float by = s_juntos_y - 70 - sinf(u * 3.1416f) * 70;
            lv_obj_set_pos(s_pelota2, (int)bx - 18, (int)by - 18);
            lv_obj_move_foreground(s_pelota2);
            sy = -fabsf(sinf((float)s_ms / 160.0f + 1.5f)) * 8;
            ps->boca = true;
            break;
        }
        case VEC_CORRER: sy = -fabsf(sinf((float)s_ms / 90.0f)) * 6; ps->boca = true; break;
        case VEC_COMER: {
            ps->boca = (s_ms / 260 + 1) % 2;
            /* comen sentados en el mantel de picnic: al medio, tu fruta o las galletas que él trajo */
            if (!s_vec_mantel) {
                s_vec_mantel = lv_image_create(s_mundo);
                lv_image_set_src(s_vec_mantel, art_icono(ICO_MANTEL, 6));
                lv_obj_move_to_index(s_vec_mantel, 1); /* sobre el pasto, debajo de todos */
            }
            ocultar(s_vec_mantel, false);
            lv_image_set_pivot(s_vec_mantel, 36, 36); lv_image_set_scale(s_vec_mantel, 560); /* mantel grande: caben los dos sentados encima */
            lv_obj_set_pos(s_vec_mantel, (int)((s_mascota.x + v->x) / 2) - 36, (int)s_juntos_y - 84);
            lv_image_set_src(s_objeto2, art_icono(s_vec_comida == ITEM_FRUTA ? ICO_FRUTA : ICO_GALLETA, 3));
            lv_obj_set_pos(s_objeto2, (int)((s_mascota.x + v->x) / 2) - 18, (int)s_juntos_y - 40);
            break;
        }
        case VEC_CHARLAR:
            ps->boca = ((s_ms / 1500) % 2 == 1) && (s_ms / 180) % 2;
            if (s_ms % 1500 < TICK_MS) {
                static const art_icono_t COSAS[] = {ICO_NOTA, ICO_CORAZON, ICO_ESTRELLA, ICO_PREGUNTA, ICO_SOL, ICO_FRUTA};
                bool habla_el = (s_ms / 1500) % 2 == 1;
                const criatura_t *q = habla_el ? v : &s_mascota;
                flotar(COSAS[azar() % 6], NULL, 0, (int)q->x, cabeza_y(q) - 6, 0);
            }
            break;
        case VEC_SIESTA:
            ps->expr = EXP_CERRADOS; ps->postura = -2; ps->orejas = -1; ps->cola = 0;
            if ((s_ms + 1200) % 2400 < TICK_MS) flotar(ICO_ZETA, NULL, 0, (int)v->x + 20, cabeza_y(v) + 20, 0);
            break;
        case VEC_REGALO:
            if (t < 2200) lv_obj_set_pos(s_objeto2, (int)v->x - 18, cabeza_y(v) - 42); /* lo trae en la cabeza */
            else if (t < 2300) {
                ocultar(s_objeto2, true);
                static const art_icono_t ITEM[ITEM_COUNT] = {ICO_FRUTA, ICO_MIEL, ICO_HIERBA, ICO_SEMILLA, ICO_PIEDRA};
                flotar(ITEM[s_vec_regalo % ITEM_COUNT], NULL, 0, (int)s_mascota.x, cabeza_y(&s_mascota) - 10, 0);
                flotar(ICO_CORAZON, NULL, 0, (int)s_mascota.x + 24, cabeza_y(&s_mascota), 300);
            }
            break;
        default: break;
        }
    }
    ps->frame = (int)((s_ms / (camina ? 200 : 800)) % 2);
    dibujar(v);
    colocar(v, sx, sy);
    if (v->y > s_mascota.y) { lv_obj_move_foreground(v->sombra); lv_obj_move_foreground(v->img); }
}

/* el vecino aparece y desaparece según el motor */
static void actualizar_vecino(const pet_t *p, bool mirando)
{
    bool aqui = p->vecino.presente && s_escena == (int)p->escena && !s_interior && s_zona_pend < 0 && p->etapa != STAGE_HUEVO;
    if (aqui && !s_vecino.visible) vecino_entra(p, mirando);
    if (!p->vecino.presente && s_vecino.visible) vecino_sale();
    if (p->vecino.presente && s_vecino.visible && !aqui && !s_vec_sale) esconder_vecino();
}

/* --- pesca: el bote del lago --- */
static lv_obj_t *s_bote, *s_cana, *s_sedal, *s_boya, *s_soga;
static lv_point_precise_t s_soga_pts[3] = {{334, 126}, {340, 133}, {347, 121}};
static art_img_t s_bote_art; /* el barquito de hoja (el mismo del viaje a la isla) */
static lv_point_precise_t s_cana_pts[2], s_sedal_pts[2];
static float s_bote_x, s_bote_y;
static int s_bote_ir = -1; /* zona a la que va en el bote: primero camina por el muelle hasta él */
static uint32_t s_bote_ir_ms;
static int s_pesca_fase; /* 0 en tierra, 1 subió y se aleja, 2 pescando, 3 vuelve */
#define BOTE_AMARRE_X 288.0f
#define BOTE_AMARRE_Y 152.0f
#define BOTE_PESCA_X 168.0f
#define BOTE_PESCA_Y 116.0f
#define BOTE_ESC 9

static bool lago_congelado(const pet_t *p) { return pet_lake_frozen(p, s_ultimo_t); } /* (v38: también con la bebida helada del norte) */
static bool zona_fria(const pet_t *p)
{
    clima_t c = pet_weather(p);
    return pet_season(p, s_ultimo_t) == ESTACION_INVIERNO || c == CLIMA_NIEVE || c == CLIMA_FRIO;
}
static void panel_bote(void);
#define BOTE_ISLA_X 66.0f /* amarrado al muelle de la isla */
#define BOTE_ISLA_Y 306.0f
static void bote_ir(int destino)
{
    s_bote_ir = destino; s_bote_ir_ms = s_ms;
    mascota_modo(M_LIBRE, 0);
}
/* el bote sirve para dos cosas: pescar en el lago o, cuando ya vio la isla, ir hasta allá */
static void bote_cb(lv_event_t *e)
{
    (void)e;
    if (!s_pet || s_pesca_fase || s_pet->dormida) return;
    if (s_bote_ir >= 0) return;
    if (s_escena == ESC_ISLA) { bote_ir(ESC_LAGO); return; } /* desde la isla: volver (camina por el muelle y sube) */
    if (pet_scene_open(s_pet, ESC_ISLA)) { panel_bote(); return; }
    s_pez_pedido = true; /* v89: al decir que sí, se abre la pesca con minijuego */
    accion(ACT_PESCAR, 0);
}
static void bote_panel_cb(lv_event_t *e)
{
    int q = (int)(intptr_t)lv_event_get_user_data(e);
    cerrar_panel();
    if (q == 0) { s_pez_pedido = true; accion(ACT_PESCAR, 0); }
    else bote_ir(ESC_ISLA);
}
static void panel_bote(void)
{
    abrir_tarjeta(PANEL_BOTE, 360, 300);
    if (!s_nino) { lv_obj_t *t = texto(s_tarjeta, "¿QUÉ HACEMOS CON EL BOTE?", C_TINTA, 2, 300); lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 40); }
    static const art_icono_t I[2] = {ICO_PEZ, ICO_ZONA_ISLA};
    static const char *const T[2] = {"PESCAR", "A LA ISLA"};
    for (int i = 0; i < 2; i++) {
        lv_obj_t *b = boton(s_tarjeta, 140, 140, bote_panel_cb, i);
        lv_obj_set_style_radius(b, 28, 0);
        lv_obj_align(b, LV_ALIGN_TOP_MID, (i * 2 - 1) * 76, s_nino ? 70 : 80);
        lv_obj_t *ic = icono(b, I[i], 5);
        lv_obj_align(ic, LV_ALIGN_TOP_MID, 0, s_nino ? 30 : 16);
        if (!s_nino) { lv_obj_t *t = texto(b, T[i], 0xFFE27A, 2, 130); lv_obj_align(t, LV_ALIGN_BOTTOM_MID, 0, -14); }
        if (i == 1 && !pet_scene_seen(s_pet, ESC_ISLA)) { lv_obj_t *st = icono(b, ICO_ESTRELLA, 2); lv_obj_align(st, LV_ALIGN_TOP_RIGHT, -10, 10); }
    }
}

static void crear_bote(void)
{
    s_bote = lv_image_create(s_mundo);
    art_bote_vela(&s_bote_art, 0, 3);
    poner_src(s_bote, &s_bote_art.dsc);
    lv_obj_add_flag(s_bote, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_bote, bote_cb, LV_EVENT_CLICKED, NULL);
    s_bote_x = BOTE_AMARRE_X; s_bote_y = BOTE_AMARRE_Y;
    ocultar(s_bote, true);
    s_cana = lv_line_create(s_mundo);
    lv_obj_set_style_line_width(s_cana, 4, 0);
    lv_obj_set_style_line_color(s_cana, lv_color_hex(0x7A4A2A), 0);
    lv_obj_set_style_line_rounded(s_cana, true, 0);
    s_sedal = lv_line_create(s_mundo);
    lv_obj_set_style_line_width(s_sedal, 1, 0);
    lv_obj_set_style_line_color(s_sedal, lv_color_hex(0xF4F4FF), 0);
    s_boya = caja(s_mundo);
    lv_obj_set_size(s_boya, 10, 10);
    lv_obj_set_style_radius(s_boya, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(s_boya, lv_color_hex(0xF0506E), 0);
    lv_obj_set_style_bg_opa(s_boya, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_boya, 2, 0);
    lv_obj_set_style_border_color(s_boya, lv_color_hex(0xFFFFFF), 0);
    s_soga = lv_line_create(s_mundo); /* la soga que lo amarra al muelle */
    lv_line_set_points(s_soga, s_soga_pts, 3);
    lv_obj_set_style_line_width(s_soga, 2, 0);
    lv_obj_set_style_line_color(s_soga, lv_color_hex(0xC8A060), 0);
    lv_obj_set_style_line_rounded(s_soga, true, 0);
    ocultar(s_soga, true);
    ocultar(s_cana, true); ocultar(s_sedal, true); ocultar(s_boya, true);
}

static void colocar_bote(void)
{
    lv_obj_set_pos(s_bote, (int)s_bote_x - 60, (int)s_bote_y - 90 + (int)(sinf((float)s_ms / 500.0f) * 2));
}

static bool bote_visible(const pet_t *p)
{
    return s_escena == ESC_LAGO && !s_interior && p->etapa != STAGE_HUEVO && !lago_congelado(p);
}

static void actualizar_bote(const pet_t *p)
{
    if (s_escena == ESC_ISLA && !s_interior && p->etapa != STAGE_HUEVO) { /* en la isla: amarrado al muelle, para volver */
        s_pesca_fase = 0; s_bote_x = BOTE_ISLA_X; s_bote_y = BOTE_ISLA_Y;
        ocultar(s_soga, true);
        ocultar(s_cana, true); ocultar(s_sedal, true); ocultar(s_boya, true);
        ocultar(s_bote, false);
        colocar_bote();
        return;
    }
    if (s_bote_x == BOTE_ISLA_X && s_bote_y == BOTE_ISLA_Y) { s_bote_x = BOTE_AMARRE_X; s_bote_y = BOTE_AMARRE_Y; } /* volvió de la isla */
    bool v = bote_visible(p);
    ocultar(s_bote, !v);
    if (v) { colocar_bote(); if (s_pesca_fase == 0) { s_bote_x = BOTE_AMARRE_X; s_bote_y = BOTE_AMARRE_Y; } }
    ocultar(s_soga, !(v && s_pesca_fase == 0));
    if (v && s_pesca_fase == 0) lv_obj_move_foreground(s_soga);
    if (!v) { s_pesca_fase = 0; s_bote_x = BOTE_AMARRE_X; s_bote_y = BOTE_AMARRE_Y; ocultar(s_cana, true); ocultar(s_sedal, true); ocultar(s_boya, true); }
}

/* camina por el muelle hasta el bote y, al llegar, sube y parte */
static bool animar_bote_ir(criatura_t *c, const pet_t *p, bool *caminando)
{
    if (s_bote_ir < 0) return false;
    if (p->dormida || s_interior || (s_escena != ESC_LAGO && s_escena != ESC_ISLA) || s_pesca_fase) { s_bote_ir = -1; return false; }
    if (s_escena == ESC_ISLA) { c->tx = BOTE_ISLA_X + 10.0f; c->ty = 266.0f + (114.0f - c->tx) * 0.375f; } /* punta del muelle */
    else { c->tx = MUELLE_X; c->ty = MUELLE_PUNTA_Y; } /* por el muelle hasta el bote */
    limitar(&c->tx, &c->ty);
    *caminando = mover(c, 3.6f);
    if (!*caminando || s_ms - s_bote_ir_ms > 20000u) {
        const int d = s_bote_ir;
        s_bote_ir = -1;
        flotar(ICO_NOTA, NULL, 0, (int)c->x, cabeza_y(c), 0);
        accion(ACT_IR, d);
    }
    return true;
}

/* la mascota en el bote: devuelve true si se encargó del cuadro */
static bool animar_pesca(criatura_t *c, const pet_t *p, float *sx, float *sy, bool *caminando)
{
    if (!bote_visible(p)) return false;
    bool pesca = p->actividad == ACTV_PESCAR && !p->dormida;
    art_pose_t *ps = &c->pose;
    colocar_bote();
    if (s_pesca_fase == 0) {
        if (!pesca) return false;
        c->tx = MUELLE_X; c->ty = MUELLE_PUNTA_Y; /* camina por el muelle hasta el bote */
        *caminando = mover(c, 3.6f);
        if (!*caminando) { s_pesca_fase = 1; flotar(ICO_NOTA, NULL, 0, (int)c->x, cabeza_y(c), 0); }
        return true;
    }
    /* arriba del bote: el bote se mueve y ella con él */
    float tx = pesca && s_pesca_fase < 3 ? BOTE_PESCA_X : BOTE_AMARRE_X, ty = pesca && s_pesca_fase < 3 ? BOTE_PESCA_Y : BOTE_AMARRE_Y;
    if (!pesca && s_pesca_fase != 3) s_pesca_fase = 3;
    float dx = tx - s_bote_x, dy = ty - s_bote_y, d = hypotf(dx, dy);
    if (d > 1.5f) { s_bote_x += dx / d * 1.6f; s_bote_y += dy / d * 1.6f; }
    else if (s_pesca_fase == 1) s_pesca_fase = 2;
    else if (s_pesca_fase == 3) { /* llegó: se baja */
        s_pesca_fase = 0;
        c->x = MUELLE_X; c->y = MUELLE_PUNTA_Y; /* se baja en la punta del muelle y camina hacia la orilla */
        c->tx = MUELLE_X; c->ty = MUELLE_BASE_Y;
        ocultar(s_cana, true); ocultar(s_sedal, true); ocultar(s_boya, true);
        return false;
    }
    /* sentada en el bote: el borde le tapa las patitas (a las chicas, menos) */
    float hundida = p->etapa == STAGE_ADULTA ? 10.0f : (p->etapa == STAGE_CRIA ? 6.0f : 2.0f);
    c->x = c->tx = s_bote_x - 12; /* su asiento queda a la izquierda del mástil */
    c->y = c->ty = s_bote_y - 24 + hundida * 0.4f + sinf((float)s_ms / 500.0f) * 2;
    ps->espejo = false; /* mira hacia el centro del lago */
    ps->postura = 0;
    ps->expr = s_pesca_fase == 2 ? EXP_CURIOSO : EXP_FELIZ;
    ps->orejas = 1;
    bool cana = s_pesca_fase == 2;
    { /* v90: si la mandaste tú, la pantalla de pesca se abre recién cuando ya está en el bote, en su lugar, con la caña */
        static uint32_t en_lugar;
        if (!cana) en_lugar = 0; else if (!en_lugar) en_lugar = s_ms | 1u;
        if (s_pez_espera) {
            if (cana && s_ms - en_lugar > 1200u && !s_pez_on) { s_pez_espera = 0; pez_empezar(p); }
            else if (s_ms - s_pez_espera > 60000u) s_pez_espera = 0; /* (no llegó: sigue la pesca de siempre) */
        }
    }
    ocultar(s_cana, !cana); ocultar(s_sedal, !cana); ocultar(s_boya, !cana);
    if (cana) {
        float hx = c->x + 26, hy = c->y - 46, kx = c->x + 66, ky = c->y - 92;
        float bx = c->x + 104, by = s_bote_y - 34 + ((s_ms / 2600) % 3 == 0 && (s_ms % 2600) < 500 ? 6 : 0) + sinf((float)s_ms / 300.0f) * 2;
        s_cana_pts[0].x = hx; s_cana_pts[0].y = hy; s_cana_pts[1].x = kx; s_cana_pts[1].y = ky;
        s_sedal_pts[0].x = kx; s_sedal_pts[0].y = ky; s_sedal_pts[1].x = bx; s_sedal_pts[1].y = by;
        lv_line_set_points(s_cana, s_cana_pts, 2);
        lv_line_set_points(s_sedal, s_sedal_pts, 2);
        lv_obj_set_pos(s_boya, (int)bx - 5, (int)by - 5);
    }
    *sx = 0; *sy = 0; *caminando = false;
    (void)sx; (void)sy;
    return true;
}

/* --- (v38) la pesca en el hielo: con el lago helado no hay bote; hay un hoyo redondo en el hielo cerca de la orilla y un banquito --- */
#define HOYO_X 214.0f  /* el centro del hoyo (pantalla) */
#define HOYO_Y 172.0f
#define BANCO_X 178.0f /* el banquito, a la izquierda del hoyo: ahí se sienta */
#define BANCO_Y 186.0f
static lv_obj_t *s_hielo_hoyo, *s_hielo_agua, *s_hielo_banco, *s_hielo_pata[2], *s_hielo_cana, *s_hielo_sedal, *s_hielo_boya;
static lv_point_precise_t s_hielo_cana_pts[2], s_hielo_sedal_pts[2];
static int s_hielo_fase; /* 0 nada, 1 camina al banquito, 2 sentada pescando */
static uint32_t s_hielo_sentada; /* desde cuándo está sentada (0: no) */
static void hielo_hoyo_cb(lv_event_t *e)
{
    (void)e;
    if (!s_pet || s_pet->dormida || s_hielo_fase) return;
    s_pez_pedido = true; /* como el bote: al decir que sí, se abre la pesca con minijuego (de invierno) */
    accion(ACT_PESCAR, 0);
}
static void crear_hoyo_hielo(void)
{
    s_hielo_hoyo = caja(s_mundo); /* el borde con escarcha */
    lv_obj_set_size(s_hielo_hoyo, 44, 20);
    lv_obj_set_style_radius(s_hielo_hoyo, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(s_hielo_hoyo, lv_color_hex(0xF4FAFF), 0);
    lv_obj_set_style_bg_opa(s_hielo_hoyo, LV_OPA_COVER, 0);
    lv_obj_set_pos(s_hielo_hoyo, (int)HOYO_X - 22, (int)HOYO_Y - 10);
    lv_obj_add_flag(s_hielo_hoyo, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(s_hielo_hoyo, 18);
    lv_obj_add_event_cb(s_hielo_hoyo, hielo_hoyo_cb, LV_EVENT_CLICKED, NULL);
    s_hielo_agua = caja(s_hielo_hoyo); /* el agua oscura */
    lv_obj_set_size(s_hielo_agua, 34, 13);
    lv_obj_set_style_radius(s_hielo_agua, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(s_hielo_agua, lv_color_hex(0x1C3E62), 0);
    lv_obj_set_style_bg_grad_color(s_hielo_agua, lv_color_hex(0x2E6A9A), 0);
    lv_obj_set_style_bg_grad_dir(s_hielo_agua, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_bg_opa(s_hielo_agua, LV_OPA_COVER, 0);
    lv_obj_remove_flag(s_hielo_agua, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_center(s_hielo_agua);
    s_hielo_banco = caja(s_mundo); /* el banquito de madera */
    lv_obj_set_size(s_hielo_banco, 30, 7);
    lv_obj_set_style_radius(s_hielo_banco, 3, 0);
    lv_obj_set_style_bg_color(s_hielo_banco, lv_color_hex(0xA0703C), 0);
    lv_obj_set_style_bg_opa(s_hielo_banco, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_hielo_banco, 1, 0);
    lv_obj_set_style_border_color(s_hielo_banco, lv_color_hex(0x6A4424), 0);
    lv_obj_remove_flag(s_hielo_banco, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_pos(s_hielo_banco, (int)BANCO_X - 15, (int)BANCO_Y - 9);
    for (int i = 0; i < 2; i++) {
        s_hielo_pata[i] = caja(s_mundo);
        lv_obj_set_size(s_hielo_pata[i], 4, 8);
        lv_obj_set_style_bg_color(s_hielo_pata[i], lv_color_hex(0x6A4424), 0);
        lv_obj_set_style_bg_opa(s_hielo_pata[i], LV_OPA_COVER, 0);
        lv_obj_remove_flag(s_hielo_pata[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_pos(s_hielo_pata[i], (int)BANCO_X - 12 + i * 20, (int)BANCO_Y - 3);
    }
    s_hielo_cana = lv_line_create(s_mundo);
    lv_obj_set_style_line_width(s_hielo_cana, 3, 0);
    lv_obj_set_style_line_color(s_hielo_cana, lv_color_hex(0x7A4A2A), 0);
    lv_obj_set_style_line_rounded(s_hielo_cana, true, 0);
    s_hielo_sedal = lv_line_create(s_mundo);
    lv_obj_set_style_line_width(s_hielo_sedal, 1, 0);
    lv_obj_set_style_line_color(s_hielo_sedal, lv_color_hex(0xF4F4FF), 0);
    s_hielo_boya = caja(s_mundo);
    lv_obj_set_size(s_hielo_boya, 8, 8);
    lv_obj_set_style_radius(s_hielo_boya, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(s_hielo_boya, lv_color_hex(0xF0506E), 0);
    lv_obj_set_style_bg_opa(s_hielo_boya, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_hielo_boya, 1, 0);
    lv_obj_set_style_border_color(s_hielo_boya, lv_color_hex(0xFFFFFF), 0);
    lv_obj_t *o[] = {s_hielo_hoyo, s_hielo_banco, s_hielo_pata[0], s_hielo_pata[1], s_hielo_cana, s_hielo_sedal, s_hielo_boya};
    for (unsigned k = 0; k < sizeof o / sizeof o[0]; k++) { lv_obj_remove_flag(o[k], LV_OBJ_FLAG_SCROLLABLE); ocultar(o[k], true); }
}
static bool hoyo_visible(const pet_t *p) { return s_escena == ESC_LAGO && !s_interior && p->etapa != STAGE_HUEVO && lago_congelado(p); }
static void actualizar_hoyo(const pet_t *p)
{
    if (!s_hielo_hoyo) return;
    const bool v = hoyo_visible(p);
    ocultar(s_hielo_hoyo, !v); ocultar(s_hielo_banco, !v); ocultar(s_hielo_pata[0], !v); ocultar(s_hielo_pata[1], !v);
    const bool cana = v && s_hielo_fase == 2;
    ocultar(s_hielo_cana, !cana); ocultar(s_hielo_sedal, !cana); ocultar(s_hielo_boya, !cana);
    if (!v) s_hielo_fase = 0;
    if (v) { /* el hoyo y el banquito van detrás de todo lo del mundo (ella pasa por delante) */
        lv_obj_move_to_index(s_hielo_pata[1], 0); lv_obj_move_to_index(s_hielo_pata[0], 0);
        lv_obj_move_to_index(s_hielo_banco, 0); lv_obj_move_to_index(s_hielo_hoyo, 0);
        lv_obj_set_style_bg_opa(s_hielo_agua, (lv_opa_t)(225 + 30 * sinf((float)s_ms / 700.0f)), 0); /* el agua se mueve un poquito */
    }
}
/* camina sobre el hielo (pasitos cortos, resbalando un poco), se sienta en el banquito y pesca en el hoyo */
static bool animar_pesca_hielo(criatura_t *c, const pet_t *p, float *sx, float *sy, bool *caminando)
{
    if (!hoyo_visible(p)) { s_hielo_fase = 0; return false; }
    const bool pesca = p->actividad == ACTV_PESCAR && !p->dormida;
    art_pose_t *ps = &c->pose;
    if (!pesca) { if (s_hielo_fase) { s_hielo_fase = 0; c->tx = BANCO_X; c->ty = BANCO_Y + 30; } return false; }
    if (s_hielo_fase < 2) {
        s_hielo_fase = 1;
        s_hielo_sentada = 0;
        c->tx = BANCO_X; c->ty = BANCO_Y;
        *caminando = mover(c, 1.6f); /* pasitos cortos */
        if (*caminando) *sx += sinf((float)s_ms / 260.0f) * 2.0f; /* resbala un poco */
        if (!*caminando) { s_hielo_fase = 2; flotar(ICO_COPO, NULL, 0, (int)c->x, cabeza_y(c), 0); }
        return true;
    }
    /* sentada en el banquito, mirando el hoyo, con su caña corta */
    c->x = c->tx = BANCO_X; c->y = c->ty = BANCO_Y;
    ps->espejo = false;
    ps->postura = -1;
    ps->expr = EXP_CURIOSO;
    ps->orejas = 1;
    *sy = -6.0f; *sx = 0; *caminando = false;
    if (!s_hielo_sentada) s_hielo_sentada = s_ms | 1u;
    if (s_pez_espera) { /* si la mandaste tú, la pantalla de pesca se abre recién cuando ya se sentó */
        if (s_ms - s_hielo_sentada > 1200u && !s_pez_on) { s_pez_espera = 0; pez_empezar(p); }
        else if (s_ms - s_pez_espera > 60000u) s_pez_espera = 0;
    }
    const float hx = c->x + 14, hy = c->y - 30, kx = HOYO_X - 2, ky = HOYO_Y - 26;
    const float by = HOYO_Y + ((s_ms / 2600) % 3 == 0 && (s_ms % 2600) < 500 ? 3 : 0);
    s_hielo_cana_pts[0].x = hx; s_hielo_cana_pts[0].y = hy; s_hielo_cana_pts[1].x = kx; s_hielo_cana_pts[1].y = ky;
    s_hielo_sedal_pts[0].x = kx; s_hielo_sedal_pts[0].y = ky; s_hielo_sedal_pts[1].x = HOYO_X; s_hielo_sedal_pts[1].y = by;
    lv_line_set_points(s_hielo_cana, s_hielo_cana_pts, 2);
    lv_line_set_points(s_hielo_sedal, s_hielo_sedal_pts, 2);
    lv_obj_set_pos(s_hielo_boya, (int)HOYO_X - 4, (int)by - 4);
    lv_obj_move_foreground(s_hielo_cana); lv_obj_move_foreground(s_hielo_sedal); lv_obj_move_foreground(s_hielo_boya);
    return true;
}

/* después de dibujar a la mascota: el bote va adelante (tapa sus patitas) y la caña también */
static void bote_al_frente(void)
{
    if (lv_obj_has_flag(s_bote, LV_OBJ_FLAG_HIDDEN)) return;
    if (s_pesca_fase) {
        lv_obj_move_foreground(s_bote);
        lv_obj_move_foreground(s_cana);
        lv_obj_move_foreground(s_sedal);
        lv_obj_move_foreground(s_boya);
    }
}

/* --- la casita por dentro --- */
#define CAMA_X 292 /* el colchón de su camita (adentro de la casita) */
#define CAMA_Y 286
static lv_obj_t *s_gorro_dormir;
/* entrar y salir de la casita con una transición: puerta, un fundido y ella que aparece del otro lado */
static int s_casa_fase; /* 0 nada, 1 entrando (se oscurece), 2 aclara adentro, 3 saliendo (se oscurece), 4 aclara afuera, 5 despierta adentro y se queda un rato */
static uint32_t s_casa_t, s_casa_sale;
static lv_obj_t *s_negro;
static void casa_negro(float f)
{
    if (!s_negro) {
        s_negro = caja(lv_layer_top());
        lv_obj_set_size(s_negro, 466, 466);
        lv_obj_set_style_bg_color(s_negro, lv_color_hex(0x0A0814), 0);
        ocultar(s_negro, true);
    }
    if (f < 0) f = 0;
    if (f > 1) f = 1;
    lv_obj_set_style_bg_opa(s_negro, (lv_opa_t)(f * 255), 0);
    ocultar(s_negro, f <= 0.01f);
    if (f > 0.01f) lv_obj_move_foreground(s_negro);
}
static void entrar_casa(const pet_t *p)
{
    s_interior = true;
    for (int i = 0; i < N_ONDITA; i++) { s_ondita_t[i] = 0; ocultar(s_ondita[i], true); } /* los anillos del agua del chapoteo no entran a la casa */
    refrescar_fondo(p, s_ultimo_t);
    actualizar_jardin(p);
    actualizar_flechas(p);
    esconder_fauna();
    s_mascota.x = s_mascota.tx = CAMA_X;
    s_mascota.y = s_mascota.ty = CAMA_Y;
}

static void salir_casa(const pet_t *p)
{
    s_interior = false;
    refrescar_fondo(p, s_ultimo_t);
    actualizar_jardin(p);
    actualizar_flechas(p);
    s_mascota.x = s_mascota.tx = LXY(LUGAR_CASITA)[0];
    s_mascota.y = s_mascota.ty = LXY(LUGAR_CASITA)[1] + 10;
}

static bool duerme_en_casita(const pet_t *p)
{
    return p->dormida && p->escena == ESC_PRADO && pet_has_decor(p, DECOR_CASITA) && s_escena == ESC_PRADO;
}

/* visitar la casita: tocas la casita y entras a verla por dentro (ahí duerme el huevo de familia, en su nido) */
static bool s_casa_visita;
#define VISITA_X 205
#define VISITA_Y 312
#define HUEVO_DENTRO_X 96
#define HUEVO_DENTRO_Y 292

/* --- tu cumpleaños: un pastel en el mundo todo el día --- */
static lv_obj_t *s_pastel;
static int s_pastel_esc = -1; /* día y zona donde se celebró: el pastel se queda ahí, no viaja */
static uint32_t s_su_cumple_hasta;
static void pastel_aqui(void) { s_pastel_esc = s_pet ? (int)s_pet->escena : 0; }

static void confeti(int x, int y, int n)
{
    static const uint32_t C[] = {0xF0506E, 0xFFD45C, 0x6FB7FF, 0x7FD36B, 0xB48CFF, 0xFF9A3C};
    for (int i = 0; i < n; i++) salpicar(x - 30 + (int)(azar() % 60), y - (int)(azar() % 20), C[azar() % 6]);
}

static void pastel_cb(lv_event_t *e)
{
    (void)e;
    accion(UI_ACCION_SONAR, SND_CUMPLEANOS);
    int x = (int)lv_obj_get_x(s_pastel) + 24, y = (int)lv_obj_get_y(s_pastel);
    confeti(x, y, 14);
    for (int i = 0; i < 3; i++) flotar(ICO_NOTA, NULL, 0, x - 30 + i * 30, y - 10, (uint32_t)i * 400);
    if (s_pet && !s_pet->dormida) mascota_modo(M_SALTA, 2500);
}

static void crear_pastel(void)
{
    s_pastel = lv_image_create(s_mundo);
    lv_image_set_src(s_pastel, art_icono(ICO_PASTEL, 4));
    lv_obj_add_flag(s_pastel, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(s_pastel, 10);
    lv_obj_add_event_cb(s_pastel, pastel_cb, LV_EVENT_CLICKED, NULL);
    ocultar(s_pastel, true);
}

static bool pastel_visible(void) { return s_pastel && !lv_obj_has_flag(s_pastel, LV_OBJ_FLAG_HIDDEN); }
/* le soplaste a las velas: se apagan con humito y empieza la fiesta */
static void soplar_velas(void)
{
    int x = (int)lv_obj_get_x(s_pastel) + 24, y = (int)lv_obj_get_y(s_pastel);
    for (int i = 0; i < 3; i++) flotar(ICO_NUBE, NULL, 0, x - 16 + i * 16, y - 6, (uint32_t)i * 120);
    globo(ICO_PASTEL, "¡SOPLASTE LAS VELAS! ¡FELIZ CUMPLEAÑOS!", 3500);
    pastel_cb(NULL);
}

static void actualizar_pastel(const pet_t *p)
{
    bool fiesta = pet_owner_birthday(p, s_ultimo_t) || s_ultimo_t < s_su_cumple_hasta;
    if (!fiesta) s_pastel_esc = -1;
    else if (s_pastel_esc < 0) pastel_aqui(); /* se celebra donde esté en ese momento */
    bool v = fiesta && p->etapa != STAGE_HUEVO && !s_interior && s_zona_pend < 0 && s_escena == s_pastel_esc;
    ocultar(s_pastel, !v);
    if (v) lv_obj_set_pos(s_pastel, LXY(LUGAR_CLARO)[0] + 46, LXY(LUGAR_CLARO)[1] - 44);
}


/* ======================================================================
 * Tu cumpleaños: un claro del bosque de noche con luciérnagas y el pastel al centro. Le soplas a las velas
 * (cada una sube como una luciérnaga que pide un deseo) y el bosque te regala una flor en el Árbol de los años.
 * Nunca se dice tu edad.
 * ==================================================================== */
#define CUM_VELAS 3
#define CUM_LUCES 12
static lv_obj_t *s_cum, *s_cum_fondo, *s_cum_pastel, *s_cum_llama[CUM_VELAS], *s_cum_deseo[CUM_VELAS], *s_cum_luz[CUM_LUCES];
static lv_obj_t *s_cum_arbol, *s_cum_tit, *s_cum_frase, *s_cum_pista, *s_cum_ok;
static art_img_t s_cum_fondo_art, s_cum_arbol_art, s_cum_tit_art, s_cum_frase_art, s_cum_pista_art;
static int s_cum_apagadas, s_cum_flores; /* s_cum_fase: 0 apagado, 1 velas encendidas, 2 se apagan, 3 el árbol */
static uint32_t s_cum_t, s_cum_deseo_t[CUM_VELAS];
static float s_cum_lx[CUM_LUCES], s_cum_ly[CUM_LUCES];
static const char *s_cum_saludo;
#define CUM_PASTEL_X (CX - 48)
#define CUM_PASTEL_Y 214
static void cumple_soplar(void)
{
    if (s_cum_fase != 1) return;
    s_cum_fase = 2;
    s_cum_t = s_ms;
    ocultar(s_cum_pista, true);
}
static void cumple_cb(lv_event_t *e)
{
    (void)e;
    if (s_cum_fase == 1 && (int32_t)(s_ms - s_cum_t) > 3000) cumple_soplar(); /* sin micrófono: tocar también apaga las velas */
}
static void cumple_fin_cb(lv_event_t *e)
{
    (void)e;
    s_cum_fase = 0;
    ocultar(s_cum, true);
    /* y la fiesta de siempre: el deseo, confeti, la canción, y el pastel queda en el prado todo el día */
    int hx = (int)s_mascota.x, hy = cabeza_y(&s_mascota);
    if (s_cum_saludo) globo(ICO_PASTEL, s_nino ? NULL : s_cum_saludo, 6000);
    confeti(hx, hy - 20, 16);
    for (int i = 0; i < 5; i++) flotar(i % 2 ? ICO_ESTRELLA : ICO_REGALO, NULL, 0, hx - 60 + i * 30, hy + 10, (uint32_t)i * 300);
    mascota_modo(M_SALTA, 3000);
    accion(UI_ACCION_SONAR, SND_CUMPLEANOS);
}
static void crear_cumple(void)
{
    s_cum = caja(lv_layer_top());
    lv_obj_set_size(s_cum, W, W);
    lv_obj_add_flag(s_cum, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_cum, cumple_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_set_style_bg_color(s_cum, lv_color_hex(0x0E0E30), 0);
    lv_obj_set_style_bg_opa(s_cum, LV_OPA_COVER, 0);
    s_cum_fondo = lv_image_create(s_cum);
    lv_obj_remove_flag(s_cum_fondo, LV_OBJ_FLAG_CLICKABLE);
    for (int i = 0; i < CUM_LUCES; i++) {
        s_cum_luz[i] = caja(s_cum);
        lv_obj_set_size(s_cum_luz[i], 5, 5);
        lv_obj_set_style_radius(s_cum_luz[i], 3, 0);
        lv_obj_set_style_bg_color(s_cum_luz[i], lv_color_hex(0xEFFF8A), 0);
        lv_obj_set_style_bg_opa(s_cum_luz[i], LV_OPA_COVER, 0);
        lv_obj_set_style_shadow_color(s_cum_luz[i], lv_color_hex(0xD8FF6A), 0);
        lv_obj_set_style_shadow_width(s_cum_luz[i], 10, 0);
    }
    s_cum_pastel = lv_image_create(s_cum);
    lv_image_set_src(s_cum_pastel, art_icono(ICO_PASTEL_VELAS, 8));
    lv_obj_set_pos(s_cum_pastel, CUM_PASTEL_X, CUM_PASTEL_Y);
    for (int k = 0; k < CUM_VELAS; k++) {
        s_cum_llama[k] = caja(s_cum);
        lv_obj_set_size(s_cum_llama[k], 8, 12);
        lv_obj_set_style_radius(s_cum_llama[k], 4, 0);
        lv_obj_set_style_bg_color(s_cum_llama[k], lv_color_hex(0xFFD45C), 0);
        lv_obj_set_style_bg_opa(s_cum_llama[k], LV_OPA_COVER, 0);
        lv_obj_set_style_shadow_color(s_cum_llama[k], lv_color_hex(0xFFA030), 0);
        lv_obj_set_style_shadow_width(s_cum_llama[k], 16, 0);
        s_cum_deseo[k] = caja(s_cum);
        lv_obj_set_size(s_cum_deseo[k], 6, 6);
        lv_obj_set_style_radius(s_cum_deseo[k], 3, 0);
        lv_obj_set_style_bg_color(s_cum_deseo[k], lv_color_hex(0xFFF6B0), 0);
        lv_obj_set_style_bg_opa(s_cum_deseo[k], LV_OPA_COVER, 0);
        lv_obj_set_style_shadow_color(s_cum_deseo[k], lv_color_hex(0xFFE27A), 0);
        lv_obj_set_style_shadow_width(s_cum_deseo[k], 14, 0);
    }
    s_cum_arbol = lv_image_create(s_cum);
    s_cum_tit = lv_image_create(s_cum);
    s_cum_pista = caja(s_cum);
    lv_obj_remove_flag(s_cum_pista, LV_OBJ_FLAG_CLICKABLE);
    s_cum_frase = lv_image_create(s_cum);
    s_cum_ok = boton(s_cum, 150, 46, cumple_fin_cb, 0);
    lv_obj_set_style_bg_color(s_cum_ok, lv_color_hex(C_VERDE), 0);
    ocultar(s_cum, true);
}
static void cumple_empezar(int flores, const char *saludo)
{
    if (!s_cum) crear_cumple();
    cerrar_panel();
    s_cum_flores = flores < 1 ? 1 : flores;
    s_cum_saludo = saludo;
    s_cum_fase = 1;
    s_cum_apagadas = 0;
    s_cum_t = s_ms;
    art_claro_fondo(&s_cum_fondo_art);
    lv_image_set_src(s_cum_fondo, &s_cum_fondo_art.dsc);
    lv_obj_set_pos(s_cum_fondo, -1, -1);
    for (int i = 0; i < CUM_LUCES; i++) { s_cum_lx[i] = 70 + (float)(azar() % 320); s_cum_ly[i] = 150 + (float)(azar() % 200); }
    for (int k = 0; k < CUM_VELAS; k++) { ocultar(s_cum_llama[k], false); ocultar(s_cum_deseo[k], true); s_cum_deseo_t[k] = 0; }
    ocultar(s_cum_pastel, false);
    ocultar(s_cum_arbol, true); ocultar(s_cum_tit, true); ocultar(s_cum_frase, true); ocultar(s_cum_ok, true);
    /* arriba, el saludo (sin la edad); abajo, la pista para soplar */
    art_texto(&s_cum_tit_art, s_nino ? "" : "¡FELIZ CUMPLEAÑOS!", 0xFFF2C0, 3, 0);
    poner_src(s_cum_tit, &s_cum_tit_art.dsc);
    lv_obj_align(s_cum_tit, LV_ALIGN_TOP_MID, 0, 70);
    ocultar(s_cum_tit, s_nino);
    lv_obj_clean(s_cum_pista);
    lv_obj_t *pi = icono(s_cum_pista, ICO_SOPLA, 3);
    lv_obj_align(pi, s_nino ? LV_ALIGN_CENTER : LV_ALIGN_LEFT_MID, s_nino ? 0 : 10, 0);
    if (!s_nino) {
        lv_obj_t *pt = lv_image_create(s_cum_pista);
        art_texto(&s_cum_pista_art, "SOPLA LAS VELAS", 0xFFFFFF, 2, 0);
        lv_image_set_src(pt, &s_cum_pista_art.dsc);
        lv_obj_align(pt, LV_ALIGN_LEFT_MID, 52, 0);
    }
    lv_obj_set_style_bg_color(s_cum_pista, lv_color_hex(0x2A2238), 0);
    lv_obj_set_style_bg_opa(s_cum_pista, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(s_cum_pista, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_size(s_cum_pista, s_nino ? 60 : 240, 46);
    lv_obj_align(s_cum_pista, LV_ALIGN_TOP_MID, 0, 340);
    ocultar(s_cum_pista, false);
    ocultar(s_cum, false);
    lv_obj_move_foreground(s_cum);
    accion(UI_ACCION_SONAR, SND_CUMPLEANOS);
}
static void animar_cumple(void)
{
    if (!s_cum_fase) return;
    uint32_t dt = s_ms - s_cum_t;
    for (int i = 0; i < CUM_LUCES; i++) { /* las luciérnagas del claro */
        s_cum_lx[i] += sinf((float)s_ms / 900.0f + (float)i * 1.7f) * 0.6f;
        s_cum_ly[i] += cosf((float)s_ms / 1100.0f + (float)i) * 0.4f;
        lv_obj_set_pos(s_cum_luz[i], (int)s_cum_lx[i], (int)s_cum_ly[i]);
        lv_obj_set_style_opa(s_cum_luz[i], (lv_opa_t)(110 + 145 * (0.5f + 0.5f * sinf((float)s_ms / 300.0f + (float)i * 2.1f))), 0);
    }
    for (int k = 0; k < CUM_VELAS; k++) { /* las llamas titilan */
        int fx = CUM_PASTEL_X + (3 + k * 3) * 8, fy = CUM_PASTEL_Y + 4 + (int)(2 * sinf((float)s_ms / 120.0f + (float)k));
        lv_obj_set_pos(s_cum_llama[k], fx, fy);
        if (s_cum_deseo_t[k]) { /* el deseo: sube como luciérnaga y se pierde entre las estrellas */
            float f = (float)(s_ms - s_cum_deseo_t[k]) / 2600.0f;
            if (f > 1) { ocultar(s_cum_deseo[k], true); continue; }
            ocultar(s_cum_deseo[k], false);
            lv_obj_set_pos(s_cum_deseo[k], fx + (int)(14 * sinf(f * 9.0f + (float)k)), fy - (int)(f * 190));
            lv_obj_set_style_opa(s_cum_deseo[k], (lv_opa_t)(255 * (1.0f - f * f)), 0);
        }
    }
    if (s_cum_fase == 2) { /* se apagan una por una */
        int deben = (int)(dt / 450) + 1;
        while (s_cum_apagadas < deben && s_cum_apagadas < CUM_VELAS) {
            int k = s_cum_apagadas++;
            ocultar(s_cum_llama[k], true);
            s_cum_deseo_t[k] = s_ms;
            accion(UI_ACCION_SONAR, SND_PLOP);
        }
        if (s_cum_apagadas >= CUM_VELAS && dt > 3200) { /* el regalo del bosque: el Árbol de los años */
            s_cum_fase = 3;
            s_cum_t = s_ms;
            ocultar(s_cum_pastel, true);
            art_arbol_anios(&s_cum_arbol_art, s_cum_flores, 5);
            lv_image_set_src(s_cum_arbol, &s_cum_arbol_art.dsc);
            lv_obj_align(s_cum_arbol, LV_ALIGN_TOP_MID, 0, 86);
            ocultar(s_cum_arbol, false);
            if (!s_nino) {
                static const char *const FR[] = {"UN AÑO MÁS DE EXPERIENCIA: HOY SABES MÁS QUE AYER",
                                                 "UN AÑO MÁS DE SABIDURÍA, COMO LAS RAÍCES DE UN ÁRBOL VIEJO",
                                                 "UN AÑO MÁS DE AMIGOS NUEVOS Y ABRAZOS GUARDADOS",
                                                 "UN AÑO MÁS DE AVENTURAS: CADA FLOR ES ALGO QUE VIVISTE",
                                                 "UN AÑO MÁS DE VALENTÍA: CRECISTE AUNQUE NO LO NOTARAS",
                                                 "UN AÑO MÁS DE RISAS: EL BOSQUE LAS ESCUCHÓ TODAS"};
                art_texto(&s_cum_tit_art, s_cum_flores == 1 ? "TU ÁRBOL DE LOS AÑOS" : "¡UNA FLOR NUEVA EN TU ÁRBOL!", 0xFFF2C0, 2, 0);
                poner_src(s_cum_tit, &s_cum_tit_art.dsc);
                lv_obj_align(s_cum_tit, LV_ALIGN_TOP_MID, 0, 66);
                art_texto(&s_cum_frase_art, FR[(s_cum_flores - 1) % 6], 0xFFFFFF, 2, 300);
                poner_src(s_cum_frase, &s_cum_frase_art.dsc);
                lv_obj_align(s_cum_frase, LV_ALIGN_TOP_MID, 0, 302);
                ocultar(s_cum_frase, false);
            }
            lv_obj_clean(s_cum_ok);
            lv_obj_t *ot = s_nino ? icono(s_cum_ok, ICO_CORAZON, 3) : texto(s_cum_ok, "¡GRACIAS!", C_BLANCO, 2, 0);
            lv_obj_center(ot);
            lv_obj_align(s_cum_ok, LV_ALIGN_TOP_MID, 0, 352);
            ocultar(s_cum_ok, false);
            accion(UI_ACCION_SONAR, SND_NUEVO);
        }
    }
    if (s_cum_fase == 3) { /* el árbol aparece creciendo */
        float f = dt > 600 ? 1.0f : (float)dt / 600.0f;
        lv_image_set_scale(s_cum_arbol, (uint32_t)(128 + 128 * f));
    }
}

/* ======================================================================
 * Interacción directa: la pelota con física, la comida que arrastras,
 * inclinar el reloj, las escondidas, atrapar frutas y los trucos
 * ==================================================================== */

static uint32_t s_juego_desde, s_juego_quieto;
static lv_obj_t *s_salir;
static void terminar_juego(void);

static void sonido(sound_t s) { accion(UI_ACCION_SONAR, s); }

static void salir_cb(lv_event_t *e)
{
    (void)e;
    terminar_juego();
}

/* --- inclinar el reloj --- */
static float s_tilt_x, s_tilt_y;
static uint32_t s_tilt_desde;
static bool s_deslizando;
static int s_desliza_r = 1;

void ui_set_tilt(float x, float y)
{
    s_tilt_x = x;
    s_tilt_y = y;
}

/* --- la pelota --- */
enum { BOLA_MANO = 0, BOLA_VUELA, BOLA_SUELO, BOLA_BOCA };
static lv_obj_t *s_bola_sombra;
static float s_bx, s_by, s_bz, s_bvx, s_bvy, s_bvz;
static int s_bola = BOLA_MANO, s_lanzar_r = 1;
static bool s_bola_agarrada;
static int s_dedo_px, s_dedo_py;
static uint32_t s_dedo_pt, s_bola_desde, s_ultimo_rebote;
static float s_dvx, s_dvy;
#define MANO_X 233.0f
#define MANO_Y 300.0f

static void bola_dibujar(void)
{
    lv_obj_set_pos(s_pelota, (int)s_bx - 18, (int)(s_by - s_bz) - 30);
    int w = 22 - (int)(s_bz / 6);
    if (w < 8) w = 8;
    lv_obj_set_size(s_bola_sombra, w, w / 3 + 2);
    lv_obj_set_pos(s_bola_sombra, (int)s_bx - w / 2, (int)s_by - 4);
    lv_obj_move_foreground(s_bola_sombra);
    lv_obj_move_foreground(s_pelota);
}

static void bola_a_la_mano(void)
{
    s_bola = BOLA_MANO;
    s_bx = MANO_X; s_by = MANO_Y; s_bz = 0;
    s_bvx = s_bvy = s_bvz = 0;
    s_juego_quieto = s_ms;
}

static void pelota_cb(lv_event_t *e)
{
    if (s_juego != JG_PELOTA || s_bola == BOLA_BOCA || s_bola == BOLA_VUELA) return;
    lv_event_code_t code = lv_event_get_code(e);
    lv_point_t pt = {0, 0};
    lv_indev_t *in = lv_indev_active();
    if (in) lv_indev_get_point(in, &pt);
    if (code == LV_EVENT_PRESSED) {
        s_bola_agarrada = true;
        s_dedo_px = pt.x; s_dedo_py = pt.y; s_dedo_pt = lv_tick_get();
        s_dvx = s_dvy = 0;
        return;
    }
    if (!s_bola_agarrada) return;
    if (code == LV_EVENT_PRESSING) {
        uint32_t ahora = lv_tick_get(), dt = ahora - s_dedo_pt;
        if (dt >= 15) { /* velocidad del dedo, suavizada (px por ms) */
            float vx = (float)(pt.x - s_dedo_px) / (float)dt, vy = (float)(pt.y - s_dedo_py) / (float)dt;
            s_dvx = s_dvx * 0.4f + vx * 0.6f; s_dvy = s_dvy * 0.4f + vy * 0.6f;
            s_dedo_px = pt.x; s_dedo_py = pt.y; s_dedo_pt = ahora;
        }
        s_bx = (float)pt.x; s_by = (float)pt.y + 22; s_bz = 0;
        limitar(&s_bx, &s_by);
        bola_dibujar();
        return;
    }
    if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        s_bola_agarrada = false;
        float v = hypotf(s_dvx, s_dvy);
        if (v < 0.25f) { s_bola = BOLA_SUELO; s_juego_quieto = s_ms; return; } /* la dejaste ahí */
        float k = (float)TICK_MS * 0.55f; /* px por cuadro */
        s_bvx = s_dvx * k; s_bvy = s_dvy * k * 0.8f;
        float max = 26.0f, vv = hypotf(s_bvx, s_bvy);
        if (vv > max) { s_bvx *= max / vv; s_bvy *= max / vv; }
        s_bvz = fminf(22.0f, 6.0f + v * 10.0f);
        s_bz = 4;
        s_bola = BOLA_VUELA;
        s_bola_desde = s_ms;
        sonido(SND_LANZAR);
        accion(ACT_LANZAR, 0);
    }
}

static void fisica_bola(void)
{
    if (s_bola != BOLA_VUELA && s_bola != BOLA_SUELO) return;
    bool suelo = s_bz <= 0.01f;
    if (suelo && fabsf(s_tilt_x) > 0.15f) s_bvx += s_tilt_x * 1.5f; /* inclinando el reloj, rueda */
    if (suelo && fabsf(s_tilt_y) > 0.15f) s_bvy += s_tilt_y * 1.0f;
    if (s_bola == BOLA_SUELO && hypotf(s_bvx, s_bvy) > 0.6f) s_bola = BOLA_VUELA;
    if (s_bola != BOLA_VUELA) return;
    s_bx += s_bvx; s_by += s_bvy; s_bz += s_bvz;
    s_bvz -= 3.0f;
    if (s_bz <= 0) {
        s_bz = 0;
        if (s_bvz < -5.0f) {
            s_bvz = -s_bvz * 0.5f; s_bvx *= 0.75f; s_bvy *= 0.75f;
            if (s_ms - s_ultimo_rebote > 250) { sonido(SND_REBOTE); s_ultimo_rebote = s_ms; }
        } else { s_bvz = 0; s_bvx *= 0.86f; s_bvy *= 0.86f; }
    }
    /* rebota en los bordes del prado (la pantalla es redonda) */
    if (s_by < Y_MIN - 30) { s_by = Y_MIN - 30; s_bvy = fabsf(s_bvy) * 0.6f; }
    if (s_by > Y_MAX + 10) { s_by = Y_MAX + 10; s_bvy = -fabsf(s_bvy) * 0.6f; }
    float dx = s_bx - CX, dy = s_by - CX, d = hypotf(dx, dy);
    if (d > 185.0f) {
        float nx = dx / d, ny = dy / d, pv = s_bvx * nx + s_bvy * ny;
        if (pv > 0) { s_bvx -= 1.6f * pv * nx; s_bvy -= 1.6f * pv * ny; }
        s_bx = CX + nx * 185.0f; s_by = CX + ny * 185.0f;
    }
    if (s_bz <= 0 && fabsf(s_bvz) < 0.5f && hypotf(s_bvx, s_bvy) < 0.5f) {
        s_bola = BOLA_SUELO;
        s_bvx = s_bvy = 0;
        s_juego_quieto = s_ms;
    }
}

/* la mascota en el juego de la pelota */
static bool animar_pelota(criatura_t *c, const pet_t *p, float *sx, float *sy, bool *caminando)
{
    art_pose_t *ps = &c->pose;
    ps->cola = 1; ps->orejas = 1;
    if (s_pelo_pasa) { /* se la pasa a su amigo */
        float mx, my, vx, vy;
        juntos_objetivos(VEC_JUGAR, &mx, &my, &vx, &vy);
        c->tx = mx; c->ty = my;
        *caminando = mover(c, 3.5f);
        if (!*caminando) { ps->espejo = false; ps->expr = EXP_FELIZ; ps->boca = true; *sy = -fabsf(sinf((float)s_ms / 160.0f)) * 8; }
        return true;
    }
    if (s_bola == BOLA_MANO) { /* espera atenta a que se la tires */
        c->tx = MANO_X + 70; c->ty = MANO_Y - 30;
        *caminando = mover(c, 4.0f);
        if (!*caminando) { ps->espejo = true; ps->expr = EXP_FELIZ; ps->boca = true; *sy = -fabsf(sinf((float)s_ms / 150.0f)) * 5; }
        return true;
    }
    if (s_bola == BOLA_BOCA) {
        if (s_lanzar_r == 2 && s_ms - s_bola_desde < 4200) { /* traviesa: se escapa con la pelota */
            if (fabsf(c->tx - c->x) < 4 && fabsf(c->ty - c->y) < 4) {
                c->tx = 110 + azar01() * 246; c->ty = Y_MIN + 10 + azar01() * (Y_MAX - Y_MIN - 20);
                limitar(&c->tx, &c->ty);
            }
            *caminando = mover(c, 6.5f);
            *sy = -fabsf(sinf((float)s_ms / 90.0f)) * 6;
            ps->expr = EXP_FELIZ;
            if (s_ms % 1400 < TICK_MS) flotar(ICO_NOTA, NULL, 0, (int)c->x, cabeza_y(c), 0);
        } else if (s_lanzar_r == 2) { /* al final la suelta lejos */
            s_bola = BOLA_SUELO; s_bx = c->x; s_by = c->y; s_bz = 0; s_juego_quieto = s_ms;
        } else { /* te la trae */
            c->tx = MANO_X + 44; c->ty = MANO_Y - 8;
            *caminando = mover(c, 5.0f);
            ps->expr = EXP_FELIZ;
            if (!*caminando) {
                bola_a_la_mano();
                flotar(ICO_CORAZON, NULL, 0, (int)c->x, cabeza_y(c), 0);
                mascota_modo(M_SALTA, 500);
            }
        }
        if (s_bola == BOLA_BOCA) {
            s_bx = c->x + (ps->espejo ? -22.0f : 22.0f); s_by = c->y - 4; s_bz = 40;
        }
        return true;
    }
    /* la pelota vuela o está en el suelo */
    if (s_lanzar_r < 0) { ps->expr = EXP_ABURRIDO; ps->orejas = -1; return true; } /* no va */
    c->tx = s_bx; c->ty = s_by;
    limitar(&c->tx, &c->ty);
    float d = hypotf(c->x - s_bx, c->y - s_by);
    if (s_lanzar_r == 0 && d < 30 && s_bola == BOLA_SUELO) { /* cansada: llega y se echa al lado */
        ps->postura = -2; ps->expr = EXP_CERRADOS; ps->orejas = -1;
        return true;
    }
    *caminando = mover(c, 6.0f);
    *sy = *caminando ? -fabsf(sinf((float)s_ms / 90.0f)) * 5 : 0;
    ps->boca = true; ps->expr = EXP_FELIZ;
    if (d < 28 && s_bz < 14 && s_lanzar_r >= 1) { /* ¡la atrapa! */
        s_bola = BOLA_BOCA;
        s_bola_desde = s_ms;
        sonido(SND_ATRAPA);
        if (s_bz > 4) flotar(ICO_ESTRELLA, NULL, 0, (int)c->x, cabeza_y(c), 0); /* en el aire */
    }
    (void)sx;
    return true;
}

/* --- escondidas --- */
enum { ESCO_CUENTA = 0, ESCO_BUSCA, ESCO_FIN };
static int s_esco = ESCO_CUENTA, s_esco_lugar, s_esco_fallos;
static uint32_t s_esco_pista;
static lv_obj_t *s_cuenta, *s_cuenta_txt;
static art_img_t s_cuenta_art;
static const uint8_t ESCO_LUGARES[3] = {LUGAR_ARBOL, LUGAR_ROCAS, LUGAR_CASITA};

static void esco_xy(int i, float *x, float *y)
{
    *x = LXY(ESCO_LUGARES[i])[0];
    *y = LXY(ESCO_LUGARES[i])[1];
}

/* un toque en el mundo durante las escondidas: ¿la encontraste? */
static bool esco_toque(int x, int y)
{
    if (s_juego != JG_ESCONDIDAS || s_esco != ESCO_BUSCA) return false;
    int mejor = -1;
    float md = 80;
    for (int i = 0; i < 3; i++) {
        float lx, ly;
        esco_xy(i, &lx, &ly);
        float d = hypotf(lx - (float)x, ly - 40 - (float)y);
        if (d < md) { md = d; mejor = i; }
    }
    if (mejor < 0) return true;
    float lx, ly;
    esco_xy(mejor, &lx, &ly);
    if (mejor == s_esco_lugar) { /* ¡te encontré! */
        s_esco = ESCO_FIN;
        s_juego_quieto = s_ms;
        s_mascota.x = s_mascota.tx = lx + 30; s_mascota.y = s_mascota.ty = ly + 20;
        limitar(&s_mascota.x, &s_mascota.y);
        s_mascota.tx = s_mascota.x; s_mascota.ty = s_mascota.y;
        mostrar_criatura(&s_mascota, true);
        mascota_modo(M_SALTA, 1500);
        for (int i = 0; i < 3; i++) flotar(ICO_CORAZON, NULL, 0, (int)lx + 10 + i * 20, (int)ly - 70, (uint32_t)i * 150);
        globo(ICO_ESCONDIDAS, "¡ME ENCONTRASTE!", 2500);
        sonido(SND_ENCONTRADA);
        accion(ACT_FIN_JUEGO, JUEGO_ESCONDIDAS * 100 + 1);
    } else {
        flotar(ICO_NO, NULL, 0, (int)lx, (int)ly - 60, 0);
        if (++s_esco_fallos >= 2) { /* sale ella, muerta de risa */
            float hx, hy;
            esco_xy(s_esco_lugar, &hx, &hy);
            s_esco = ESCO_FIN;
            s_juego_quieto = s_ms;
            s_mascota.x = s_mascota.tx = hx + 30; s_mascota.y = s_mascota.ty = hy + 20;
            limitar(&s_mascota.x, &s_mascota.y);
            s_mascota.tx = s_mascota.x; s_mascota.ty = s_mascota.y;
            mostrar_criatura(&s_mascota, true);
            mascota_modo(M_SALTA, 1500);
            flotar(ICO_NOTA, NULL, 0, (int)hx + 20, (int)hy - 70, 0);
            globo(ICO_ESCONDIDAS, "¡ESTABA AQUÍ! ¡TE GANÉ!", 2500);
            accion(ACT_FIN_JUEGO, JUEGO_ESCONDIDAS * 100 + 0);
        }
    }
    return true;
}

static void animar_escondidas(void)
{
    uint32_t t = s_ms - s_juego_desde;
    if (s_esco == ESCO_CUENTA) {
        int n = (int)(t / 700) + 1;
        if (n > 3) {
            s_esco = ESCO_BUSCA;
            ocultar(s_cuenta, true);
            mostrar_criatura(&s_mascota, false);
            s_esco_pista = s_ms;
            return;
        }
        char txt[24];
        snprintf(txt, sizeof txt, "%d...", n);
        art_texto(&s_cuenta_art, txt, 0xFFFFFF, 8, 0);
        poner_src(s_cuenta_txt, &s_cuenta_art.dsc);
        lv_obj_center(s_cuenta_txt);
        return;
    }
    if (s_esco == ESCO_BUSCA && s_ms - s_esco_pista > (s_nino ? 1800u : s_suave ? 2500u : 4000u)) { /* algo se mueve donde está */
        s_esco_pista = s_ms;
        float x, y;
        esco_xy(s_esco_lugar, &x, &y);
        for (int k = 0; k < 4; k++) salpicar((int)x - 12 + k * 8, (int)y - 46, 0x5DBB63);
    }
    if (s_esco == ESCO_FIN && s_ms - s_juego_quieto > 3000) terminar_juego();
}

/* --- atrapar frutas --- */
#define N_FRUTAS_J 5
#define FRUTAS_MS 20000u
static void picnic_nivel(bool arriba);
static lv_obj_t *s_juego_pintura, *s_juego_foto; /* el paisaje del juego de la fruta: pintado con formas o como foto */
static lv_obj_t *s_fondo_pleno; /* un fondo que llega al borde: el vidrio lo recorta */
static lv_obj_t *s_canasta, *s_juego_capa, *s_fr_img[N_FRUTAS_J], *s_fr_bar;
static float s_fr_x[N_FRUTAS_J], s_fr_y[N_FRUTAS_J], s_fr_v[N_FRUTAS_J];
static bool s_fr_activa[N_FRUTAS_J];
static float s_canasta_x = CX;
static int s_fr_pts;
static int s_fr_nivel; /* 0..9: cada partida ganada (8 frutas o más) acelera la siguiente; si pierdes vuelve a 0 */
#define FRUTAS_GANAR 8
static uint32_t s_fr_proxima;
#define CANASTA_Y 345

static void capa_juego_cb(lv_event_t *e)
{
    (void)e;
    if (s_juego != JG_FRUTAS) return;
    lv_point_t pt = {0, 0};
    lv_indev_t *in = lv_indev_active();
    if (in) lv_indev_get_point(in, &pt);
    s_canasta_x = (float)pt.x;
}

static void animar_frutas(void)
{
    uint32_t t = s_ms - s_juego_desde;
    if (fabsf(s_tilt_x) > 0.12f) s_canasta_x += s_tilt_x * 16.0f; /* también inclinando el reloj */
    if (s_canasta_x < 120) s_canasta_x = 120;
    if (s_canasta_x > 346) s_canasta_x = 346;
    if ((int)s_canasta_x - 30 != lv_obj_get_x(s_canasta)) lv_obj_set_pos(s_canasta, (int)s_canasta_x - 30, CANASTA_Y - 50);
    { const int32_t bv = (int32_t)(t >= FRUTAS_MS ? 0 : (FRUTAS_MS - t) / 200); if (bv != lv_bar_get_value(s_fr_bar)) lv_bar_set_value(s_fr_bar, bv, LV_ANIM_OFF); } /* (solo cuando cambia) */
    if (t < FRUTAS_MS && (int32_t)(s_ms - s_fr_proxima) >= 0) {
        for (int i = 0; i < N_FRUTAS_J; i++)
            if (!s_fr_activa[i]) {
                s_fr_activa[i] = true;
                s_fr_x[i] = 120 + azar01() * 226;
                s_fr_y[i] = 70;
                s_fr_v[i] = ((s_nino ? 2.4f : s_suave ? 3.0f : 4.0f) + (float)t / 4000.0f + azar01() * 1.5f) * 1.35f * (1.0f + 0.12f * (float)s_fr_nivel); /* cada nivel +12 % */
                { static const art_icono_t FR[9] = {ICO_FRUTA_MANZANA, ICO_FRUTA_NARANJA, ICO_FRUTA_BANANA, ICO_FRUTA_UVAS, ICO_FRUTA_CEREZAS, ICO_FRUTA_PERA, ICO_FRUTA_SANDIA, ICO_FRUTA, ICO_MIEL};
                    lv_image_set_src(s_fr_img[i], art_icono(FR[azar() % 9], 4)); }
                ocultar(s_fr_img[i], false);
                lv_obj_move_foreground(s_fr_img[i]); /* una vez al aparecer (cada cuadro era repintado de más) */
                break;
            }
        s_fr_proxima = s_ms + (uint32_t)((900 - (t / 40 > 500 ? 500 : t / 40)) * 10 / (10 + s_fr_nivel));
    }
    for (int i = 0; i < N_FRUTAS_J; i++) {
        if (!s_fr_activa[i]) continue;
        s_fr_y[i] += s_fr_v[i];
        if (s_fr_y[i] >= CANASTA_Y - 46 && s_fr_y[i] < CANASTA_Y - 20 && fabsf(s_fr_x[i] - s_canasta_x) < 34) { /* ¡adentro! */
            s_fr_activa[i] = false;
            ocultar(s_fr_img[i], true);
            s_fr_pts++;
            sonido(SND_ATRAPA);
            if (s_nino) flotar(ICO_ESTRELLA, NULL, 0, (int)s_canasta_x, CANASTA_Y - 60, 0);
            else flotar(-1, "+1", 0xFFE27A, (int)s_canasta_x, CANASTA_Y - 60, 0);
            if (s_pet && !s_pet->dormida) mascota_modo(M_SALTA, 350);
            continue;
        }
        if (s_fr_y[i] > CANASTA_Y + 30) { /* se cayó al suelo */
            s_fr_activa[i] = false;
            ocultar(s_fr_img[i], true);
            for (int k = 0; k < 3; k++) salpicar((int)s_fr_x[i] - 4 + k * 4, CANASTA_Y + 20, 0xF0506E);
            continue;
        }
        lv_obj_set_pos(s_fr_img[i], (int)s_fr_x[i] - 18, (int)s_fr_y[i] - 18);
    }
    if (t >= FRUTAS_MS) {
        bool quedan = false;
        for (int i = 0; i < N_FRUTAS_J; i++) quedan |= s_fr_activa[i];
        if (quedan) return;
        char txt[64];
        snprintf(txt, sizeof txt, s_fr_pts == 1 ? "¡ATRAPASTE %d FRUTA!" : "¡ATRAPASTE %d FRUTAS!", s_fr_pts);
        if (s_fr_pts >= FRUTAS_GANAR) { if (s_fr_nivel < 9) s_fr_nivel++; size_t n = strlen(txt); snprintf(txt + n, sizeof txt - n, " NIVEL %d", s_fr_nivel + 1); }
        else s_fr_nivel = 0;
        int pts = s_fr_pts > 99 ? 99 : s_fr_pts;
        terminar_juego();
        globo(ICO_CANASTA, txt, 3000);
        accion(ACT_FIN_JUEGO, JUEGO_FRUTAS * 100 + pts);
    }
}

/* la mascota mientras atrapas frutas: te alienta desde un costado */
static bool animar_hinchada(criatura_t *c)
{
    c->tx = 128; c->ty = 280;
    art_pose_t *ps = &c->pose;
    bool camina = mover(c, 4.0f);
    if (!camina) { ps->espejo = false; ps->expr = EXP_FELIZ; ps->cola = 1; ps->orejas = 1; ps->boca = (s_ms / 400) % 2; }
    return camina;
}

/* v89: una luz redonda y suave, hecha una sola vez como imagen (en vez de una sombra de LVGL, que en la placa cuesta
 * muchísimo por cuadro y además se ve escalonada). El borde lleva un poco de ruido para que no se vean anillos. */
static const lv_image_dsc_t *luz_suave(art_img_t *a, uint32_t color, int lado, float fuerza)
{
    if (a->px && (int)a->dsc.header.w == lado) return &a->dsc;
    art_img_free(a);
    a->px = malloc((size_t)lado * lado * 4);
    if (!a->px) return NULL;
    a->capacidad = (uint32_t)(lado * lado);
    { extern uint32_t g_art_bytes; g_art_bytes += a->capacidad * 4u; }
    uint32_t sem = color * 2654435761u + 7u;
    for (int y = 0; y < lado; y++)
        for (int x = 0; x < lado; x++) {
            const float dx = (float)x - (lado - 1) * 0.5f, dy = (float)y - (lado - 1) * 0.5f;
            const float r = sqrtf(dx * dx + dy * dy) / (lado * 0.5f);
            float al = r >= 1.0f ? 0.0f : (1.0f - r) * (1.0f - r) * fuerza;
            sem = sem * 1664525u + 1013904223u;
            if (al > 2.0f) al += (float)((int)(sem >> 28) - 8) * 0.4f;
            const uint32_t v = al <= 0 ? 0u : (al >= 255 ? 255u : (uint32_t)al);
            a->px[y * lado + x] = (v << 24) | (color & 0xFFFFFFu);
        }
    memset(&a->dsc, 0, sizeof a->dsc);
    a->dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
    a->dsc.header.cf = LV_COLOR_FORMAT_ARGB8888;
    a->dsc.header.w = (uint32_t)lado;
    a->dsc.header.h = (uint32_t)lado;
    a->dsc.header.stride = (uint32_t)(lado * 4);
    a->dsc.data_size = (uint32_t)(lado * lado * 4);
    a->dsc.data = (const uint8_t *)a->px;
    return &a->dsc;
}

/* --- los hongos de luz: pantalla propia, de noche; se prenden en orden y hay que repetirlo --- */
#define N_HONGOS_J 4
#define HG_MAX 14
static lv_obj_t *s_hg_pant, *s_hg[N_HONGOS_J], *s_hg_luz[N_HONGOS_J], *s_hg_t, *s_hg_x, *s_hg_punto[HG_MAX], *s_hg_luci[7];
static art_img_t s_hg_t_art, s_hg_luz_art[4];
static int8_t s_hg_antes[4] = {-1, -1, -1, -1}; /* v89: cómo quedó cada hongo (prendido o no) para no repintarlo si no cambia */
static uint8_t s_hg_seq[HG_MAX];
static int s_hg_n, s_hg_paso, s_hg_fase, s_hg_prendido = -1, s_hg_mostrado = -1, s_hg_texto = -1;
static uint32_t s_hg_t0, s_hg_hasta;
static const int16_t HG_XY[N_HONGOS_J][2] = {{233, 232}, {128, 304}, {338, 304}, {233, 378}}; /* en rombo: arriba, izquierda, derecha, abajo */
static const uint32_t HG_COL[N_HONGOS_J] = {0xFF6BD0, 0x5ED0FF, 0xFFE27A, 0xB48CFF};
static const sound_t HG_SND[N_HONGOS_J] = {SND_TOQUE, SND_PLOP, SND_ATRAPA, SND_REBOTE};

static void hg_texto(int k)
{
    static const char *const T[4] = {"MIRA CÓMO SE PRENDEN...", "", "¡UPS! ERA ESTE", "¡MUY BIEN! UNO MÁS"};
    const int clave = k == 1 ? 100 + s_hg_n : k;
    if (clave == s_hg_texto) return;
    s_hg_texto = clave;
    if (s_nino || k < 0) { ocultar(s_hg_t, true); return; }
    char tt[64];
    if (k == 1) snprintf(tt, sizeof tt, s_hg_n == 1 ? "¡AHORA TÚ! TOCA 1 HONGUITO" : "¡AHORA TÚ! TOCA %d HONGUITOS EN ORDEN", s_hg_n);
    else snprintf(tt, sizeof tt, "%s", T[k]);
    art_texto(&s_hg_t_art, tt, k == 2 ? 0xFFB0A0 : 0xFFF2C0, 2, 280);
    poner_src(s_hg_t, &s_hg_t_art.dsc);
    lv_obj_align(s_hg_t, LV_ALIGN_TOP_MID, 0, 62);
    ocultar(s_hg_t, false);
}

static void hongo_prender(int i, uint32_t ms)
{
    s_hg_prendido = i;
    s_hg_hasta = s_ms + ms;
    sonido(HG_SND[i]);
}

static void hongos_fin(void)
{
    int pts = s_hg_n - 1;
    if (pts < 0) pts = 0;
    char txt[48];
    snprintf(txt, sizeof txt, pts == 1 ? "¡RECORDASTE %d HONGO!" : "¡RECORDASTE %d HONGOS!", pts);
    terminar_juego();
    globo(ICO_HONGO, txt, 3000);
    accion(ACT_FIN_JUEGO, JUEGO_HONGOS * 100 + pts);
}

static void hongo_cb(lv_event_t *e)
{
    int i = (int)(intptr_t)lv_event_get_user_data(e);
    if (s_juego != JG_HONGOS || s_hg_fase != 1) return;
    s_juego_quieto = s_ms;
    hongo_prender(i, 260);
    if (i != s_hg_seq[s_hg_paso]) { /* se equivocó: termina */
        s_hg_fase = 2;
        s_hg_t0 = s_ms;
        hg_texto(2);
        if (s_pet && !s_pet->dormida) mascota_modo(M_DUDA, 1200);
        return;
    }
    if (++s_hg_paso < s_hg_n) return;
    if (s_hg_n >= HG_MAX) { s_hg_n++; s_hg_fase = 2; s_hg_t0 = s_ms; return; }
    /* ¡bien! uno más */
    flotar(ICO_ESTRELLA, NULL, 0, CX, 170, 0);
    s_hg_seq[s_hg_n++] = (uint8_t)(azar() % N_HONGOS_J);
    s_hg_fase = 0;
    s_hg_paso = 0;
    s_hg_t0 = s_ms + 900;
    s_hg_mostrado = -1;
    hg_texto(3);
}

static void hg_salir_cb(lv_event_t *e) { (void)e; if (s_juego == JG_HONGOS) hongos_fin(); }

static void crear_hongos(lv_obj_t *scr)
{
    (void)scr;
    s_hg_pant = caja(lv_layer_top());
    lv_obj_set_size(s_hg_pant, W, W);
    lv_obj_set_style_bg_color(s_hg_pant, lv_color_hex(0x0A1424), 0);
    lv_obj_set_style_bg_grad_color(s_hg_pant, lv_color_hex(0x1C4A38), 0);
    lv_obj_set_style_bg_grad_dir(s_hg_pant, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_bg_opa(s_hg_pant, LV_OPA_COVER, 0);
    lv_obj_t *suelo = caja(s_hg_pant); /* el pasto de la noche */
    lv_obj_set_size(suelo, W, 150);
    lv_obj_set_pos(suelo, 0, W - 150);
    lv_obj_set_style_bg_color(suelo, lv_color_hex(0x16382A), 0);
    lv_obj_set_style_bg_opa(suelo, LV_OPA_COVER, 0);
    for (int k = 0; k < 7; k++) { /* luciérnagas que flotan de fondo */
        s_hg_luci[k] = caja(s_hg_pant);
        lv_obj_set_size(s_hg_luci[k], 5, 5);
        lv_obj_set_style_radius(s_hg_luci[k], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(s_hg_luci[k], lv_color_hex(0xFFF2A0), 0);
        lv_obj_set_style_bg_opa(s_hg_luci[k], LV_OPA_COVER, 0); /* (v89: sin sombra difusa: era lo más caro de dibujar) */
    }
    for (int k = 0; k < HG_MAX; k++) { /* cuántos hay que recordar (un puntito por hongo) */
        s_hg_punto[k] = caja(s_hg_pant);
        lv_obj_set_size(s_hg_punto[k], 12, 12);
        lv_obj_set_style_radius(s_hg_punto[k], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(s_hg_punto[k], 2, 0);
        lv_obj_set_style_border_color(s_hg_punto[k], lv_color_hex(0xFFF2C0), 0);
        lv_obj_set_style_bg_color(s_hg_punto[k], lv_color_hex(0xFFE27A), 0);
        lv_obj_set_style_bg_opa(s_hg_punto[k], 0, 0);
    }
    for (int i = 0; i < N_HONGOS_J; i++) {
        s_hg_luz[i] = lv_image_create(s_hg_pant); /* v89: la luz es una imagen suave hecha una vez (antes un círculo con sombra de 50 px) */
        lv_obj_remove_flag(s_hg_luz[i], LV_OBJ_FLAG_CLICKABLE);
        { const lv_image_dsc_t *lz = luz_suave(&s_hg_luz_art[i], HG_COL[i], 150, 230.0f); if (lz) lv_image_set_src(s_hg_luz[i], lz); }
        lv_obj_set_pos(s_hg_luz[i], HG_XY[i][0] - 75, HG_XY[i][1] - 44 - 75);
        ocultar(s_hg_luz[i], true);
        s_hg[i] = lv_image_create(s_hg_pant);
        lv_image_set_src(s_hg[i], art_icono(ICO_HONGO, 8));
        lv_obj_set_pos(s_hg[i], HG_XY[i][0] - 48, HG_XY[i][1] - 92);
        lv_obj_set_style_image_recolor(s_hg[i], lv_color_hex(HG_COL[i]), 0);
        lv_obj_set_style_image_recolor_opa(s_hg[i], LV_OPA_60, 0);
        lv_obj_add_flag(s_hg[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_ext_click_area(s_hg[i], 14);
        lv_obj_add_event_cb(s_hg[i], hongo_cb, LV_EVENT_PRESSED, (void *)(intptr_t)i);
    }
    s_hg_t = lv_image_create(s_hg_pant);
    lv_obj_remove_flag(s_hg_t, LV_OBJ_FLAG_CLICKABLE);
    s_hg_x = boton(s_hg_pant, BOTON_ABAJO, BOTON_ABAJO, hg_salir_cb, 0);
    lv_obj_set_style_radius(s_hg_x, LV_RADIUS_CIRCLE, 0);
    lv_obj_align(s_hg_x, LV_ALIGN_TOP_MID, 0, BOTON_ABAJO_Y);
    lv_obj_t *xi = icono(s_hg_x, ICO_NO, 2); lv_obj_center(xi);
    ocultar(s_hg_pant, true);
}

static void hongos_empezar(void)
{
    if (zona_activa()) zona_salir(); /* (v97) */
    s_juego = JG_HONGOS;
    s_hg_n = 1;
    s_hg_seq[0] = (uint8_t)(azar() % N_HONGOS_J);
    s_hg_fase = 0;
    s_hg_paso = 0;
    s_hg_t0 = s_ms + 1400;
    s_hg_mostrado = -1;
    s_hg_prendido = -1;
    s_hg_texto = -2;
    for (int i = 0; i < N_HONGOS_J; i++) s_hg_antes[i] = -1;
    ocultar(s_hg_pant, false);
    lv_obj_move_foreground(s_hg_pant);
    hg_texto(0);
    sonido(SND_HADAS);
}

static void hongos_ocultar(void)
{
    if (s_hg_pant) ocultar(s_hg_pant, true);
}

static void animar_hongos(void)
{
    const uint32_t D = s_nino ? 900 : (s_suave ? 700 : 560), ON = D * 7 / 10;
    if (s_hg_fase == 0 && (int32_t)(s_ms - s_hg_t0) >= 0) { /* muestra la secuencia */
        int k = (int)((s_ms - s_hg_t0) / D);
        hg_texto(0);
        if (k >= s_hg_n) { s_hg_fase = 1; s_hg_paso = 0; s_juego_quieto = s_ms; s_hg_prendido = -1; hg_texto(1); }
        else if (k != s_hg_mostrado && (s_ms - s_hg_t0) % D < ON) { s_hg_mostrado = k; hongo_prender(s_hg_seq[k], ON); }
    }
    if (s_hg_fase == 1 && s_ms - s_juego_quieto > 20000) { s_hg_fase = 2; s_hg_t0 = s_ms; } /* se fue */
    if (s_hg_fase == 2 && s_ms - s_hg_t0 > 1400) { hongos_fin(); return; }
    bool prendido_vale = s_hg_prendido >= 0 && (int32_t)(s_ms - s_hg_hasta) < 0;
    for (int i = 0; i < N_HONGOS_J; i++) {
        bool on = prendido_vale && s_hg_prendido == i;
        if (s_hg_fase == 2) on = (s_ms / 150) % 2 && i == s_hg_seq[s_hg_paso < s_hg_n ? s_hg_paso : 0]; /* el que era */
        /* v89: solo se toca lo que cambia (antes, en cada latido, se ponía todo de nuevo y se repintaban los cuatro con su sombra) */
        if (s_hg_antes[i] == (int8_t)on) continue;
        s_hg_antes[i] = (int8_t)on;
        ocultar(s_hg_luz[i], !on);
        lv_obj_set_style_image_recolor_opa(s_hg[i], on ? LV_OPA_30 : LV_OPA_80, 0); /* prendido: su color de verdad, brillante */
        lv_obj_set_style_translate_y(s_hg[i], on ? -8 : 0, 0); /* salta un poquito (sin agrandarlo: escalar es caro) */
        lv_obj_set_style_image_opa(s_hg[i], on ? LV_OPA_COVER : 165, 0); /* apagados, más tenues: se nota cuál se prende */
    }
    /* los puntitos de arriba: cuántos van; en turno se llenan los que ya hiciste */
    const int n = s_hg_n > HG_MAX ? HG_MAX : s_hg_n;
    const int x0 = CX - (n * 18) / 2 + 3;
    for (int k = 0; k < HG_MAX; k++) {
        ocultar(s_hg_punto[k], k >= n || s_nino);
        if (k >= n) continue;
        lv_obj_set_pos(s_hg_punto[k], x0 + k * 18, 44);
        bool lleno = s_hg_fase == 1 ? k < s_hg_paso : (s_hg_fase == 0 ? k < s_hg_mostrado || (k == s_hg_mostrado && prendido_vale) : false);
        lv_obj_set_style_bg_opa(s_hg_punto[k], lleno ? LV_OPA_COVER : 0, 0);
    }
    for (int k = 0; k < 7; k++) { /* luciérnagas */
        float t = (float)s_ms / (2400.0f + (float)k * 310.0f) + (float)k * 1.7f;
        lv_obj_set_pos(s_hg_luci[k], (int)(CX + sinf(t) * (80 + k * 18)), (int)(150 + k * 38 + cosf(t * 1.3f) * 14));
        lv_obj_set_style_opa(s_hg_luci[k], (lv_opa_t)(120 + 100 * sinf(t * 3.0f)), 0);
    }
}

/* --- empezar y terminar --- */
static void empezar_juego(int j)
{
    if (!s_pet || s_pet->dormida) return;
    if (zona_activa()) zona_salir(); /* (v97) un juego del mundo nunca corre encima de una zona (dibujaba el mundo en memoria de la zona) */
    cerrar_panel();
    s_juego_desde = s_juego_quieto = s_ms;
    ocultar(s_salir, false);
    if (j == JUEGO_FRUTAS) lv_obj_align(s_salir, LV_ALIGN_TOP_MID, 0, 12); /* en las frutas, abajo están la canasta y ella: la X va arriba (la barra de tiempo queda justo debajo) */
    else lv_obj_align(s_salir, LV_ALIGN_BOTTOM_MID, 0, -40);
    lv_obj_move_foreground(s_salir);
    ocultar(s_pelota, true);
    if (j == JUEGO_PELOTA) {
        s_juego = JG_PELOTA;
        s_pelo_pasa = false; s_pelo_pasa_n = 0;
        bola_a_la_mano();
        ocultar(s_pelota, false);
        ocultar(s_bola_sombra, false);
        bola_dibujar();
        if (!s_nino) globo(ICO_PELOTA, "¡TÍRAMELA! DESLIZA LA PELOTA", 2500);
    } else if (j == JUEGO_ESCONDIDAS) {
        s_juego = JG_ESCONDIDAS;
        s_esco = ESCO_CUENTA;
        s_esco_fallos = 0;
        s_esco_lugar = (int)(azar() % 3);
        ocultar(s_cuenta, false);
        lv_obj_move_foreground(s_cuenta);
        lv_obj_move_foreground(s_salir);
        sonido(SND_CONTAR);
    } else if (j == JUEGO_HONGOS) {
        hongos_empezar();
    } else {
        s_juego = JG_FRUTAS;
        s_fr_pts = 0;
        s_fr_proxima = s_ms + 600;
        s_canasta_x = CX;
        s_mascota.x = s_mascota.tx = CX; s_mascota.y = s_mascota.ty = 418; /* abajo, mirando la canasta */
        for (int i = 0; i < N_FRUTAS_J; i++) { s_fr_activa[i] = false; ocultar(s_fr_img[i], true); }
        ocultar(s_canasta, false);
        ocultar(s_juego_capa, false);
        ocultar(s_fr_bar, false);
        { /* el fondo: foto con la hora, la estación y el clima de ahora; sin foto, el paisaje pintado */
            int fx = 0, fy = 0;
            const int cli = (int)pet_weather(s_pet);
            const int gris = cli == CLIMA_FRIO ? 2 : (cli == CLIMA_NUBLADO || cli == CLIMA_LLUVIA || cli == CLIMA_NIEVE || cli == CLIMA_TORMENTA);
            const lv_image_dsc_t *f = art_pantalla_clima(1, s_fondo_periodo < 0 ? 0 : s_fondo_periodo, s_fondo_estacion < 0 ? 1 : s_fondo_estacion, gris, &fx, &fy);
            if (f) { lv_image_set_src(s_juego_foto, f); lv_obj_remove_flag(s_juego_foto, LV_OBJ_FLAG_HIDDEN); lv_obj_add_flag(s_juego_pintura, LV_OBJ_FLAG_HIDDEN); }
            else { lv_obj_add_flag(s_juego_foto, LV_OBJ_FLAG_HIDDEN); lv_obj_remove_flag(s_juego_pintura, LV_OBJ_FLAG_HIDDEN); }
            picnic_nivel(true); /* ella pasa al mismo nivel que el juego (si no, la tapaba) */
        }
        lv_obj_move_foreground(s_juego_capa);
        lv_obj_move_foreground(s_mascota.sombra);
        lv_obj_move_foreground(s_mascota.img);
        lv_obj_move_foreground(s_canasta);
        lv_obj_move_foreground(s_fr_bar);
        lv_obj_move_foreground(s_salir);
        if (!s_nino) globo(ICO_CANASTA, "¡ATRAPA LAS FRUTAS!", 2000);
    }
}

static void terminar_juego(void)
{
    if (s_juego == JG_NADA) return;
    s_juego = JG_NADA;
    s_pelo_pasa = false;
    if (s_bola_palo) { s_bola_palo = false; lv_image_set_src(s_pelota, art_icono(ICO_PELOTA, 3)); }
    ocultar(s_salir, true);
    ocultar(s_pelota, true);
    ocultar(s_bola_sombra, true);
    ocultar(s_cuenta, true);
    ocultar(s_canasta, true);
    ocultar(s_juego_capa, true);
    if (s_juego_foto) { lv_obj_add_flag(s_juego_foto, LV_OBJ_FLAG_HIDDEN); lv_image_set_src(s_juego_foto, NULL); } /* suelta la foto de fondo */
    art_pantalla_liberar(1);
    picnic_nivel(false);
    ocultar(s_fr_bar, true);
    hongos_ocultar();
    for (int i = 0; i < N_FRUTAS_J; i++) { s_fr_activa[i] = false; ocultar(s_fr_img[i], true); }
    if (!s_mascota.visible && s_pet && s_pet->etapa != STAGE_HUEVO) mostrar_criatura(&s_mascota, true);
}

static void animar_juegos(void)
{
    const pet_t *p = s_pet;
    if (s_juego == JG_NADA || !p) return;
    if (p->dormida || s_interior || s_zona_pend >= 0) { terminar_juego(); return; }
    if (s_juego == JG_PELOTA) {
        const bool amigo = s_vecino.visible && !s_vec_sale && p->vecino.presente;
        if (!amigo && s_pelo_pasa) { s_pelo_pasa = false; bola_a_la_mano(); ocultar(s_pelota, false); ocultar(s_bola_sombra, false); }
        if (amigo && !s_pelo_pasa && s_bola == BOLA_MANO && !s_bola_agarrada && s_pelo_pasa_n < 2 &&
            (s_ms - s_juego_quieto > 4000 || (s_ms - s_juego_desde > 1500 && (p->vecino.actividad == VEC_JUGAR || p->vecino.actividad == VEC_CORRER)))) {
            s_pelo_pasa = true; s_pelo_pasa_desde = s_ms; s_pelo_pasa_n++; /* lo dejaste solo un ratito: juegan entre ellos */
            ocultar(s_pelota, true); ocultar(s_bola_sombra, true);
        }
        if (s_pelo_pasa) {
            s_juego_quieto = s_ms;
            lv_indev_t *dd = lv_indev_get_next(NULL);
            const bool toque = dd && lv_indev_get_state(dd) == LV_INDEV_STATE_PRESSED;
            if (toque || s_ms - s_pelo_pasa_desde > 13000) { /* vuelve contigo */
                s_pelo_pasa = false; bola_a_la_mano();
                ocultar(s_pelota, false); ocultar(s_bola_sombra, false);
                flotar(ICO_CORAZON, NULL, 0, (int)s_mascota.x, cabeza_y(&s_mascota), 0);
            }
        }
        fisica_bola();
        if (s_bola != BOLA_BOCA || true) bola_dibujar();
        if ((s_bola == BOLA_MANO || s_bola == BOLA_SUELO) && s_ms - s_juego_quieto > 25000) terminar_juego(); /* se aburrió de esperar */
    } else if (s_juego == JG_ESCONDIDAS) animar_escondidas();
    else if (s_juego == JG_FRUTAS) animar_frutas();
    else if (s_juego == JG_HONGOS) animar_hongos();
}

/* la mascota durante un juego: devuelve true si se encargó del cuadro */
static bool animar_juego(criatura_t *c, const pet_t *p, float *sx, float *sy, bool *caminando)
{
    if (s_juego == JG_PELOTA) return animar_pelota(c, p, sx, sy, caminando);
    if (s_juego == JG_FRUTAS || s_juego == JG_HONGOS) { *caminando = animar_hinchada(c); return true; }
    if (s_juego == JG_ESCONDIDAS) {
        if (s_esco == ESCO_CUENTA) { /* corre a esconderse */
            float x, y;
            esco_xy(s_esco_lugar, &x, &y);
            c->tx = x; c->ty = y;
            limitar(&c->tx, &c->ty);
            *caminando = mover(c, 7.0f);
        }
        return true;
    }
    return false;
}

/* --- la comida que arrastras hasta su boca --- */
enum { COMIDA_NADA = 0, COMIDA_MANO, COMIDA_SUELO, COMIDA_DADA };
static int s_comida = COMIDA_NADA, s_comida_f = FOOD_COMIDA;
static float s_cx_, s_cy_;
static uint32_t s_comida_desde;
static bool s_comida_agarrada;
/* el picnic: al dar de comer aparece una pantalla propia con tapete y cesta; la comida sale de la cesta */
static lv_obj_t *s_picnic, *s_pic_ficha[6], *s_pic_x;
static uint32_t s_picnic_fin;
static void picnic_cerrar(void);
static void picnic_x_cb(lv_event_t *e) { (void)e; picnic_cerrar(); }
static void picnic_fondo_cb(lv_event_t *e) { (void)e; if (s_comida == COMIDA_NADA) picnic_cerrar(); }
static void picnic_crear(void)
{
    s_picnic = caja(lv_obj_get_parent(s_mundo)); /* (con fondo completo tapa el mundo; sin foto, solo el tapete y la cesta) */
    lv_obj_set_size(s_picnic, W, W);
    lv_obj_set_pos(s_picnic, 0, 0);
    lv_obj_set_style_bg_opa(s_picnic, LV_OPA_TRANSP, 0);
    lv_obj_remove_flag(s_picnic, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_t *pint = caja(s_picnic); /* lo pintado con formas: se oculta si hay foto */
    lv_obj_set_size(pint, W, W); lv_obj_set_pos(pint, 0, 0);
    lv_obj_set_style_bg_opa(pint, LV_OPA_TRANSP, 0);
    lv_obj_remove_flag(pint, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_user_data(pint, (void *)(uintptr_t)0xAD0);
    s_picnic_pintura = pint;
    lv_obj_t *tap = caja(pint); /* el tapete de cuadros */
    lv_obj_set_size(tap, 280, 170);
    lv_obj_set_pos(tap, 93, 215);
    lv_obj_set_style_radius(tap, 10, 0);
    lv_obj_set_style_bg_color(tap, lv_color_hex(0xFFF3E0), 0);
    lv_obj_set_style_bg_opa(tap, LV_OPA_COVER, 0);
    lv_obj_remove_flag(tap, LV_OBJ_FLAG_CLICKABLE);
    for (int f = 0; f < 4; f++)
        for (int c = 0; c < 6; c++) {
            if ((f + c) & 1) continue;
            lv_obj_t *q = caja(tap);
            lv_obj_set_size(q, 280 / 6, 170 / 4);
            lv_obj_set_pos(q, c * (280 / 6), f * (170 / 4));
            lv_obj_set_style_bg_color(q, lv_color_hex(0xD8454F), 0);
            lv_obj_set_style_bg_opa(q, LV_OPA_COVER, 0);
            lv_obj_remove_flag(q, LV_OBJ_FLAG_CLICKABLE);
        }
    s_picnic_foto = lv_image_create(s_picnic); /* la foto (tapete y cesta con transparencia) */
    lv_obj_add_flag(s_picnic_foto, LV_OBJ_FLAG_CLICKABLE); /* tocar el campo (sin comida en la mano) también sale */
    lv_obj_add_event_cb(s_picnic_foto, picnic_fondo_cb, LV_EVENT_CLICKED, NULL);
    { /* la bandeja de comidas: un arco abajo con las 5 de siempre; la canasta (a la izquierda del mantel) abre las de temporada */
        static const art_icono_t ICO[5] = {ICO_COMIDA, ICO_FRUTA, ICO_MIEL, ICO_TAZA, ICO_VASO};
        static const uint32_t FC[5][2] = {{0xFFB45C, 0xD8702A}, {0xFF7A7A, 0xC83C48}, {0xFFD45C, 0xD8962A}, {0xC88A5A, 0x8A5A3A}, {0x6ADC8A, 0x2E9A52}};
        for (int i = 0; i < 5; i++) {
            const float ang = (90.0f + ((float)i - 2.0f) * 18.0f) * 3.14159265f / 180.0f;
            const int cx = 233 + (int)(cosf(ang) * 188.0f), cy = 233 + (int)(sinf(ang) * 188.0f);
            lv_obj_t *b = boton(s_picnic, 54, 54, comida_cb, i);
            ficha_magica(b, FC[i][0], FC[i][1], 27);
            lv_obj_set_pos(b, cx - 27, cy - 27);
            lv_obj_center(icono(b, ICO[i], 3));
            s_pic_ficha[i] = b;
        }
        lv_obj_t *cb = boton(s_picnic, 96, 90, temporada_cb, 5); /* la canasta: un poco más arriba de donde estaba pintada */
        lv_obj_set_pos(cb, 62, 232);
        lv_obj_set_ext_click_area(cb, 8);
        lv_obj_set_style_bg_opa(cb, LV_OPA_TRANSP, 0); lv_obj_set_style_bg_opa(cb, LV_OPA_TRANSP, LV_STATE_PRESSED); lv_obj_set_style_border_width(cb, 0, 0);
        lv_obj_t *ci = icono(cb, ICO_CANASTA, 5);
        lv_obj_align(ci, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_obj_t *hi = icono(cb, ICO_HOJA, 2); /* la hojita: "de temporada" */
        lv_obj_align(hi, LV_ALIGN_TOP_RIGHT, -4, 0);
        s_pic_ficha[5] = cb;
        s_pic_x = boton(s_picnic, 44, 44, picnic_x_cb, 0); /* la salida: arriba a la derecha, lejos de la comida */
        lv_obj_set_style_radius(s_pic_x, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_pos(s_pic_x, 338, 70);
        lv_obj_center(icono(s_pic_x, ICO_NO, 2));
    }
    lv_obj_add_flag(s_picnic_foto, LV_OBJ_FLAG_HIDDEN);
    ocultar(s_picnic, true);
}
/* pone la foto del picnic (si existe) en lugar de lo pintado: primero el fondo completo (3), si no el tapete con transparencia (2) */
static void picnic_foto_poner(void)
{
    int fx = 0, fy = 0;
    const int cli = (int)pet_weather(s_pet);
    const int gris = cli == CLIMA_FRIO ? 2 : (cli == CLIMA_NUBLADO || cli == CLIMA_LLUVIA || cli == CLIMA_NIEVE || cli == CLIMA_TORMENTA);
    const lv_image_dsc_t *f = art_pantalla_clima(3, s_fondo_periodo < 0 ? 0 : s_fondo_periodo, s_fondo_estacion < 0 ? 1 : s_fondo_estacion, gris, &fx, &fy); /* con la hora, la estación y el clima de ahora */
    s_picnic_tapa = f != NULL;
    if (!f) f = art_pantalla(2, &fx, &fy);
    if (f) {
        lv_image_set_src(s_picnic_foto, f);
        lv_obj_set_pos(s_picnic_foto, fx, fy);
        lv_obj_remove_flag(s_picnic_foto, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_picnic_pintura, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_picnic_foto, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(s_picnic_pintura, LV_OBJ_FLAG_HIDDEN);
    }
}
/* ella y la comida pasan al mismo nivel que el picnic (el mundo queda oculto detrás); al cerrar vuelven al mundo */
static void picnic_nivel(bool arriba)
{
    lv_obj_t *dest = arriba ? lv_obj_get_parent(s_mundo) : s_mundo;
    if (lv_obj_get_parent(s_mascota.img) != dest) {
        lv_obj_set_parent(s_mascota.sombra, dest);
        lv_obj_set_parent(s_mascota.img, dest);
        lv_obj_set_parent(s_objeto, dest);
    }
}
static void picnic_cerrar(void)
{
    if (!s_picnic_on) return;
    s_picnic_on = false;
    s_picnic_tapa = false;
    s_comida = COMIDA_NADA; s_comida_agarrada = false; /* si salió con comida en la mano, se guarda */
    ocultar(s_objeto, true);
    picnic_nivel(false);
    ocultar(s_picnic, true);
    lv_image_set_src(s_picnic_foto, NULL);
    art_pantalla_liberar(2);
    art_pantalla_liberar(3);
}

static void comida_dibujar(void)
{
    lv_obj_set_pos(s_objeto, (int)s_cx_ - 24, (int)s_cy_ - 40);
    lv_obj_move_foreground(s_objeto);
}

static void dar_comida(void)
{
    s_comida = COMIDA_NADA; /* la reacción del motor la muestra comiendo */
    ocultar(s_objeto, true);
    s_ultima_comida = s_comida_f;
    s_picnic_fin = s_ms + 25000; /* sigue abierto por si quiere más; se sale con la X o tocando el campo */
    accion(ACT_ALIMENTAR, s_comida_f);
}

/* abre la pantalla del picnic (sin comida todavía): al tocar el icono de comer se ve de inmediato y ahí abajo se elige */
static void picnic_bandeja_actualizar(void)
{
    if (!s_picnic) return;
    for (int i = 0; i < 6; i++) {
        bool sin = s_pet && ((i == FOOD_FRUTA && s_pet->inventario[ITEM_FRUTA] == 0) || (i == FOOD_MIEL && s_pet->inventario[ITEM_MIEL] == 0));
        lv_obj_set_style_opa(s_pic_ficha[i], sin ? LV_OPA_40 : LV_OPA_COVER, 0);
    }
}
static void picnic_abrir(void)
{
    if (s_picnic_on) return;
    cerrar_panel();
    if (!s_picnic) picnic_crear();
    picnic_foto_poner();
    s_picnic_on = true;
    s_picnic_fin = s_ms + 25000;
    picnic_bandeja_actualizar();
    for (int i = 0; i < 4; i++) { ocultar(s_flecha[i], true); s_flecha_dest[i] = -1; } /* las flechas a otras zonas no van en el picnic */
    ocultar(s_picnic, false);
    picnic_nivel(true);
    lv_obj_move_to_index(s_picnic, lv_obj_get_index(s_mundo) + 1); /* justo encima del mundo, debajo de botones y globos */
    lv_obj_move_to_index(s_mascota.sombra, lv_obj_get_index(s_picnic) + 1);
    lv_obj_move_to_index(s_mascota.img, lv_obj_get_index(s_picnic) + 2);
    s_mascota.x = 233; s_mascota.y = 352; s_mascota.tx = 233; s_mascota.ty = 352; /* grande, al centro del mantel */
    mascota_modo(M_LIBRE, 0);
    if (!s_nino) globo(ICO_COMIDA, "¿QUÉ LE DAMOS?", 2500);
}
static void empezar_comida(int f)
{
    picnic_abrir();
    s_comida_f = f;
    s_comida = COMIDA_MANO;
    s_comida_desde = s_ms;
    s_picnic_fin = 0;
    s_cx_ = 190; s_cy_ = 340; /* sale un poco a la derecha de la canasta: así no se toca al dársela */
    lv_image_set_src(s_objeto, art_icono(ico_comida(f), 4));
    ocultar(s_objeto, false);
    comida_dibujar();
    if (!s_nino) {
        char t[64] = "LLÉVASELA A LA BOCA";
        if (f >= PET_COMIDA_EST_BASE && f < PET_COMIDA_EST_BASE + PET_COMIDAS_EST) { snprintf(t, sizeof t, "%s: A SU BOCA", pet_seasonal_food_info(f - PET_COMIDA_EST_BASE)->corto); mayusculas(t); }
        globo(ico_comida(f), t, 2200);
    }
}

static float boca_dist(void)
{
    criatura_t *c = &s_mascota;
    if (s_picnic_on) { /* la mascota está al doble: su boca queda más arriba */
        const int l = lado(c);
        return hypotf(s_cx_ - c->x, s_cy_ - 20 - (c->y - (l * 2 / 3.0f - 7.5f * c->pose.escala)));
    }
    return hypotf(s_cx_ - c->x, s_cy_ - 20 - (float)(cabeza_y(c) + lado(c) / 3));
}

static void objeto_cb(lv_event_t *e)
{
    if (s_comida != COMIDA_MANO && s_comida != COMIDA_SUELO) return;
    lv_event_code_t code = lv_event_get_code(e);
    lv_point_t pt = {0, 0};
    lv_indev_t *in = lv_indev_active();
    if (in) lv_indev_get_point(in, &pt);
    if (code == LV_EVENT_PRESSED) { s_comida_agarrada = true; s_comida = COMIDA_MANO; return; }
    if (!s_comida_agarrada) return;
    if (code == LV_EVENT_PRESSING) {
        s_cx_ = (float)pt.x; s_cy_ = (float)pt.y + 16;
        comida_dibujar();
        if (boca_dist() < (s_picnic_on ? 80 : 46)) { s_comida_agarrada = false; dar_comida(); } /* ¡ñam! */
        return;
    }
    if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        s_comida_agarrada = false;
        if (boca_dist() < (s_picnic_on ? 110 : 70)) { dar_comida(); return; }
        /* la dejaste en el suelo: si tiene hambre, va a buscarla */
        limitar(&s_cx_, &s_cy_);
        if (s_picnic_on) { if (s_cy_ < 235) s_cy_ = 235; if (s_cy_ > 365) s_cy_ = 365; }
        s_comida = COMIDA_SUELO;
        s_comida_desde = s_ms;
        comida_dibujar();
    }
}

/* la mascota mira la comida, abre la boca y, si tiene hambre, la sigue */
static bool animar_comida(criatura_t *c, const pet_t *p, bool *caminando)
{
    if (s_picnic_on && s_comida == COMIDA_NADA && s_picnic_fin && (int32_t)(s_ms - s_picnic_fin) > 0) picnic_cerrar();
    if (s_comida != COMIDA_MANO && s_comida != COMIDA_SUELO) return false;
    art_pose_t *ps = &c->pose;
    bool hambre = p->necesidad[NEED_SACIEDAD] < 90 || s_comida_f >= FOOD_BEBIDA_CALIENTE;
    float d = boca_dist();
    ps->espejo = s_cx_ < c->x;
    ps->orejas = 1;
    ps->boca = d < 160 && hambre;
    ps->expr = hambre ? EXP_FELIZ : EXP_ABURRIDO;
    if (s_comida == COMIDA_SUELO && hambre) { /* va a comérsela */
        c->tx = s_cx_ + (s_cx_ < c->x ? 30 : -30); c->ty = s_cy_;
        limitar(&c->tx, &c->ty);
        *caminando = mover(c, 3.5f);
        if (!*caminando && d < 90) dar_comida();
    } else if (s_comida_agarrada && hambre && d > 110) { /* la sigue con pasitos */
        c->tx = s_cx_; c->ty = s_cy_ < Y_MIN ? Y_MIN : s_cy_;
        limitar(&c->tx, &c->ty);
        *caminando = mover(c, 1.6f);
    }
    if (s_ms - s_comida_desde > 20000 && !s_comida_agarrada) { /* nadie la tocó */
        if (hambre) { c->tx = s_cx_; c->ty = s_cy_; s_comida = COMIDA_SUELO; s_comida_desde = s_ms - 15000; }
        else { s_comida = COMIDA_NADA; ocultar(s_objeto, true); s_picnic_fin = s_ms + 8000; } /* la guarda para después */
    }
    return true;
}

/* --- menú Jugar y trucos --- */
static int s_truco = TRUCO_DORMIDA;
static bool s_rotada;
static void panel_trucos(void);

static void cristales_empezar(void); /* ui_magia.inc */
static void panel_crismenu(void);    /* (v38) ui_musica.inc */
static void panel_canciones(void);
static void evento_pieza(const pet_event_t *e);
static void evento_nuestra(const pet_event_t *e);
static void animar_musica(void);
static bool tararea_cancion(const pet_t *p);
static bool cuna_cancion(void);
static bool alarma_cancion(const pet_t *ap);
static bool rep_activo(void);
static void animar_pochi(void); /* ui_pochi.inc */
static void chispa(int x, int y, uint32_t color, float vx, float vy); /* ui_magia.inc */
static void jugar_cb(lv_event_t *e)
{
    int j = (int)(intptr_t)lv_event_get_user_data(e);
    if (j == 3) { panel_trucos(); return; }
    if (j == 9) { panel_crismenu(); return; } /* los cristales (en la cueva): el juego de memoria, tocar libre, las canciones (v38) */
    if (j == 4) j = JUEGO_HONGOS;
    cerrar_panel();
    s_juego_pedido = j;
    accion(ACT_JUGAR, j);
}

/* una ficha de color con degradado (cada juego tiene el suyo) */
static void ficha_color(lv_obj_t *b, uint32_t arriba, uint32_t abajo)
{
    lv_obj_set_style_bg_color(b, lv_color_hex(arriba), 0);
    lv_obj_set_style_bg_grad_color(b, lv_color_hex(abajo), 0);
    lv_obj_set_style_bg_grad_dir(b, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_bg_color(b, lv_color_hex(abajo), LV_STATE_PRESSED);
    lv_obj_set_style_bg_grad_color(b, lv_color_hex(arriba), LV_STATE_PRESSED);
    lv_obj_set_style_border_color(b, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_opa(b, LV_OPA_60, 0);
    lv_obj_set_style_border_width(b, 2, 0);
    lv_obj_set_style_radius(b, 22, 0);
    lv_obj_set_style_shadow_width(b, 10, 0);
    lv_obj_set_style_shadow_color(b, lv_color_hex(abajo), 0);
    lv_obj_set_style_shadow_opa(b, LV_OPA_50, 0);
    lv_obj_set_style_shadow_offset_y(b, 3, 0);
}
/* el menú de juegos con el aire de la historia: un portal de noche, translúcido, con fichas redondas de luz */
static void lect_opa_cb2(void *var, int32_t v) { lv_obj_set_style_opa((lv_obj_t *)var, (lv_opa_t)v, 0); }
static void lect_estrellas(lv_obj_t *padre, int n, int r, int cx, int cy) /* chispas que titilan dentro del disco */
{
    for (int i = 0; i < n; i++) {
        lv_obj_t *e = lv_obj_create(padre);
        lv_obj_remove_style_all(e);
        const int d = 3 + (int)(azar() % 2);
        lv_obj_set_size(e, d, d);
        const int dx = (int)(azar() % (uint32_t)(2 * r)) - r, dy = (int)(azar() % (uint32_t)(2 * r)) - r;
        { /* dentro del disco y también dentro del círculo de la pantalla */
            lv_area_t ar;
            lv_obj_update_layout(padre);
            lv_obj_get_coords(padre, &ar);
            const int sx = ar.x1 + cx + dx - CX, sy = ar.y1 + cy + dy - CX;
            if (dx * dx + dy * dy > r * r || sx * sx + sy * sy > 196 * 196 || ar.y1 + cy + dy > 372) { lv_obj_delete(e); continue; }
        }
        lv_obj_set_pos(e, cx + dx, cy + dy);
        lv_obj_set_style_radius(e, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(e, lv_color_hex(0xFFF3A0), 0);
        lv_obj_set_style_bg_opa(e, LV_OPA_COVER, 0);
        lv_obj_remove_flag(e, LV_OBJ_FLAG_CLICKABLE);
        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, e);
        lv_anim_set_values(&a, 40, 255);
        lv_anim_set_time(&a, 1200 + azar() % 1600);
        lv_anim_set_playback_time(&a, 1200 + azar() % 1600);
        lv_anim_set_delay(&a, azar() % 2000);
        lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
        lv_anim_set_exec_cb(&a, lect_opa_cb2);
        lv_anim_start(&a);
    }
}
/* (s_disco, declarado arriba) el disco de noche: va detrás de la tarjeta, así el círculo queda centrado en la pantalla aunque el contenido suba un poco */
static void portal_fondo_r(int radio, int w, int h) /* un disco (o placa) de noche, translúcido, con borde dorado */
{
    /* sin disco: el vidrio oscuro ya es el fondo del menú (todo opaco: así no se dibuja el mundo de atrás) */
    lv_obj_set_style_bg_opa(s_capa, LV_OPA_COVER, 0);
    lv_obj_set_style_opa(s_vidrio, LV_OPA_COVER, 0);
    (void)radio; (void)w; (void)h;
    if (s_disco) ocultar(s_disco, true);
}
static void ficha_magica(lv_obj_t *b, uint32_t arriba, uint32_t abajo, int radio)
{
    lv_obj_set_style_bg_color(b, lv_color_hex(arriba), 0);
    lv_obj_set_style_bg_grad_color(b, lv_color_hex(abajo), 0);
    lv_obj_set_style_bg_grad_dir(b, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(b, lv_color_hex(0xFFE27A), 0);
    lv_obj_set_style_border_opa(b, LV_OPA_90, 0);
    lv_obj_set_style_border_width(b, 2, 0);
    lv_obj_set_style_radius(b, radio, 0);
    lv_obj_set_style_shadow_width(b, 0, 0);
}
static void panel_jugar(void)
{
    abrir_tarjeta(PANEL_JUGAR, 350, 350);
    if (!s_nino) { lv_obj_t *t = texto(s_tarjeta, "¿A QUÉ JUGAMOS?", 0xFFE9A0, 2, 0); lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 22); }
    static const art_icono_t ICO[6] = {ICO_PELOTA, ICO_ESCONDIDAS, ICO_CANASTA, ICO_HONGO, ICO_PATITA, ICO_CRISTAL};
    static const char *const TXT[6] = {"PELOTA", "ESCONDE", "FRUTAS", "HONGOS", "TRUCOS", "CRISTALES"};
    static const int QUE[6] = {0, 1, 2, 4, 3, 9};
    static const uint32_t COL[6][2] = {{0xFFB45C, 0xD8702A}, {0x6ADC8A, 0x2E9A52}, {0xFF7A7A, 0xC83C48}, {0xB48AF0, 0x6A4AB8}, {0x6AB8FF, 0x2E78C8}, {0xC8A8FF, 0x6A48C0}};
    const int n = s_escena == ESC_CUEVA && !s_interior ? 6 : 5; /* en la cueva, el juego de los cristales */
    const int arriba = n == 6 ? 3 : 2, D = s_nino ? 88 : 72;
    for (int i = 0; i < n; i++) { /* arriba dos (o tres) y abajo tres */
        const int fila = i < arriba ? 0 : 1, cant = fila == 0 ? arriba : n - arriba, pos = fila == 0 ? i : i - arriba;
        lv_obj_t *b = boton(s_tarjeta, D, D, jugar_cb, QUE[i]);
        ficha_magica(b, COL[i][0], COL[i][1], LV_RADIUS_CIRCLE);
        lv_obj_set_ext_click_area(b, 8);
        const int x = (int)((pos - (cant - 1) / 2.0f) * (s_nino ? D + 6 : 98)), y = fila == 0 ? 66 : 184;
        lv_obj_align(b, LV_ALIGN_TOP_MID, x, y + (s_nino ? 14 : 0));
        lv_obj_t *ic = icono(b, ICO[i], 3);
        lv_obj_center(ic);
        if (!s_nino) { lv_obj_t *t = texto(s_tarjeta, TXT[i], 0xFFF3C8, 2, 0); lv_obj_align_to(t, b, LV_ALIGN_OUT_BOTTOM_MID, 0, 3); }
    }
}

static void truco_cb(lv_event_t *e)
{
    int t = (int)(intptr_t)lv_event_get_user_data(e);
    cerrar_panel();
    s_truco = t;
    accion(ACT_TRUCO, t);
}

static const art_icono_t ICO_TRUCOS[TRUCO_COUNT] = {ICO_ZETA, ICO_ESTRELLA, ICO_VUELTA, ICO_SALUDO};

static void panel_trucos(void)
{
    abrir_tarjeta(PANEL_TRUCOS, 350, 350);
    if (!s_pet) return;
    if (!s_nino) { lv_obj_t *t = texto(s_tarjeta, "TRUCOS", 0xFFE9A0, 2, 0); lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 22); }
    static const char *const TXT[TRUCO_COUNT] = {"{LA} DORMID{A}", "VOLTERETA", "UNA VUELTA", "SALUDAR"};
    for (int i = 0; i < TRUCO_COUNT; i++) {
        lv_obj_t *b = boton(s_tarjeta, 88, 88, truco_cb, i);
        { static const uint32_t TC[TRUCO_COUNT][2] = {{0x8EA0F0, 0x4A58B8}, {0xFFD45C, 0xD8962A}, {0x6AD8E0, 0x2E8EA8}, {0xFF9AC0, 0xC84A80}}; ficha_magica(b, TC[i][0], TC[i][1], LV_RADIUS_CIRCLE); }
        lv_obj_set_ext_click_area(b, 6);
        lv_obj_align(b, LV_ALIGN_TOP_MID, (i % 2 ? 1 : -1) * 68, (s_nino ? 70 : 56) + (i / 2) * (s_nino ? 112 : 122));
        lv_obj_t *ic = icono(b, ICO_TRUCOS[i], 3);
        lv_obj_align(ic, LV_ALIGN_TOP_MID, 0, 8);
        float v = s_pet->truco[i];
        /* cuánto lo sabe: tres estrellitas (llenas según lo practicado) */
        int estrellas = v >= PET_TRUCO_APRENDIDO ? 3 : (v >= 30 ? 2 : (v >= 8 ? 1 : 0));
        for (int k = 0; k < 3; k++) {
            lv_obj_t *st = icono(b, ICO_ESTRELLA, 2);
            lv_obj_align(st, LV_ALIGN_BOTTOM_MID, (k - 1) * 18, -10);
            if (k >= estrellas) lv_obj_set_style_opa(st, LV_OPA_30, 0);
        }
        if (!s_nino) { lv_obj_t *t = texto(s_tarjeta, TXT[i], 0xFFF3C8, 2, 0); lv_obj_align_to(t, b, LV_ALIGN_OUT_BOTTOM_MID, 0, 3); }
    }
}

static const uint32_t TRUCO_MS[TRUCO_COUNT] = {2600, 1500, 1700, 2000};
static void hacer_truco(int t, bool sola)
{
    s_truco = t;
    mascota_modo(M_TRUCO, TRUCO_MS[t % TRUCO_COUNT]);
    criatura_t *c = &s_mascota;
    int hx = (int)c->x, hy = cabeza_y(c);
    if (t == TRUCO_DORMIDA) { flotar(ICO_ZETA, NULL, 0, hx + 22, hy, 500); flotar(ICO_ZETA, NULL, 0, hx + 30, hy - 8, 1100); }
    if (t == TRUCO_SALUDO) {
        flotar(ICO_SALUDO, NULL, 0, hx + 30, hy, 150);
        if (!s_nino) flotar(-1, "¡HOLA!", 0xFFFFFF, hx, hy - 30, 700);
        flotar(ICO_CORAZON, NULL, 0, hx - 26, hy, 1300);
    }
    if (!sola) flotar(ICO_ESTRELLA, NULL, 0, hx, hy - 10, TRUCO_MS[t % TRUCO_COUNT] - 400);
}

/* animación de cada truco (modo M_TRUCO): tienen que notarse */
static void animar_truco(criatura_t *c, float *sx, float *sy)
{
    art_pose_t *ps = &c->pose;
    const float dur = (float)TRUCO_MS[s_truco % TRUCO_COUNT];
    float k = 1.0f - (float)(int32_t)(c->modo_hasta - s_ms) / dur; /* 0..1 */
    if (k < 0) k = 0;
    if (k > 1) k = 1;
    ps->expr = EXP_FELIZ; ps->cola = 1; ps->orejas = 1;
    int l = lado(c);
    switch (s_truco) {
    case TRUCO_DORMIDA: /* se tira al suelo, se "duerme" y de un salto: ¡ta-dá! */
        if (k < 0.12f) { *sy = -sinf(k / 0.12f * 3.1416f) * 10; }
        else if (k < 0.8f) {
            ps->postura = -2; ps->expr = EXP_CERRADOS; ps->orejas = -1; ps->cola = -1;
            lv_image_set_pivot(c->img, l / 2, l);
            lv_image_set_rotation(c->img, ps->espejo ? -250 : 250); /* de costado */
            s_rotada = true;
        } else {
            float q = (k - 0.8f) / 0.2f;
            lv_image_set_rotation(c->img, 0);
            *sy = -sinf(q * 3.1416f) * 26; ps->boca = true;
        }
        break;
    case TRUCO_VOLTERETA: { /* salto alto con un giro completo en el aire */
        lv_image_set_pivot(c->img, l / 2, l / 2);
        float g = k < 0.15f ? 0 : (k > 0.85f ? 1 : (k - 0.15f) / 0.7f);
        lv_image_set_rotation(c->img, (int32_t)(g * (ps->espejo ? -3600.0f : 3600.0f)));
        s_rotada = true;
        *sy = k < 0.15f ? (k / 0.15f) * 6 : -sinf((k - 0.15f) / 0.85f * 3.1416f) * 52; /* se agacha y salta */
        ps->boca = k > 0.85f;
        break;
    }
    case TRUCO_VUELTA: { /* se da vuelta de un lado al otro, dos veces, como trompo */
        float a = k * 4.0f * 3.1416f;
        float cx = cosf(a);
        lv_image_set_pivot(c->img, l / 2, l);
        lv_image_set_scale_x(c->img, (uint32_t)(256.0f * (0.15f + 0.85f * fabsf(cx))));
        ps->espejo = cx < 0;
        s_rotada = true;
        *sy = -fabsf(sinf(k * 2.0f * 3.1416f)) * 8;
        break;
    }
    case TRUCO_SALUDO: /* salta y mira a un lado y al otro, saludando */
        ps->espejo = ((int)(k * 6.0f)) % 2;
        ps->orejas = (s_ms / 150) % 2 ? 1 : -1;
        *sy = -fabsf(sinf(k * 3.0f * 3.1416f)) * 18;
        ps->boca = true;
        break;
    default: break;
    }
}

/* --- inclinar: se desliza hacia el lado más bajo --- */
static bool animar_deslizar(criatura_t *c, const pet_t *p, float *sx)
{
    /* Un reloj apoyado en un soporte o la mesa queda siempre un poco inclinado: eso no es «inclinarlo». Se sigue con un promedio lento
     * la inclinación de reposo (unos 8 s) y solo cuenta lo que se aparta de ella. Antes, un reposo de ~0,4 la hacía resbalar hacia un lado,
     * volver, resbalar de nuevo… y se quedaba oscilando ahí todo el tiempo. */
    static float ref_x, ref_y;
    ref_x += (s_tilt_x - ref_x) * 0.004f;
    ref_y += (s_tilt_y - ref_y) * 0.004f;
    const float ex = s_tilt_x - ref_x, ey = s_tilt_y - ref_y;
    float m = fabsf(ex) > fabsf(ey) ? fabsf(ex) : fabsf(ey);
    bool puede = !p->dormida && s_juego == JG_NADA && s_panel == PANEL_NINGUNO && p->etapa != STAGE_HUEVO && s_pesca_fase == 0 &&
                 c->modo == M_LIBRE && s_comida == COMIDA_NADA;
    /* con el reloj parado de frente (en un soporte o sostenido frente a ti) la gravedad cae toda por la pantalla (m casi 1):
     * eso no es inclinarlo, y ella no debe caerse al fondo. Solo se desliza con inclinaciones moderadas. */
    const bool de_frente = hypotf(s_tilt_x, s_tilt_y) > 0.7f; /* más de ~45° desde la horizontal: está de pie o en la mano, mirándote */
    const float umbral = s_deslizando ? 0.30f : 0.50f; /* empieza con una inclinación clara y sigue hasta que casi la nivelas */
    if (!puede || m < umbral || de_frente) {
        if (m < 0.30f || de_frente) { s_deslizando = false; s_tilt_desde = 0; }
        return false;
    }
    if (!s_tilt_desde) s_tilt_desde = s_ms;
    if (s_ms - s_tilt_desde < 500) return false;
    if (!s_deslizando) { s_deslizando = true; s_desliza_r = 1; accion(ACT_INCLINAR, 0); }
    c->x += ex * 9.0f;
    c->y += ey * 5.0f;
    limitar(&c->x, &c->y);
    c->tx = c->x; c->ty = c->y;
    art_pose_t *ps = &c->pose;
    ps->espejo = ex < 0;
    if (s_desliza_r > 0) { ps->expr = EXP_FELIZ; ps->boca = true; ps->orejas = 1; ps->cola = 1; }
    else { ps->expr = EXP_MIEDO; ps->orejas = -1; ps->postura = -1; }
    *sx = sinf((float)s_ms / 60.0f) * 2;
    return true;
}

static void crear_juegos(lv_obj_t *scr)
{
    lv_obj_add_flag(s_pelota, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(s_pelota, 24);
    lv_obj_add_event_cb(s_pelota, pelota_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(s_pelota, pelota_cb, LV_EVENT_PRESSING, NULL);
    lv_obj_add_event_cb(s_pelota, pelota_cb, LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(s_pelota, pelota_cb, LV_EVENT_PRESS_LOST, NULL);
    lv_obj_remove_flag(s_pelota, LV_OBJ_FLAG_GESTURE_BUBBLE);
    s_bola_sombra = caja(s_mundo);
    lv_obj_set_style_bg_color(s_bola_sombra, lv_color_hex(0x10301A), 0);
    lv_obj_set_style_bg_opa(s_bola_sombra, LV_OPA_30, 0);
    lv_obj_set_style_radius(s_bola_sombra, LV_RADIUS_CIRCLE, 0);
    ocultar(s_bola_sombra, true);

    lv_obj_add_flag(s_objeto, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(s_objeto, 16);
    lv_obj_add_event_cb(s_objeto, objeto_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(s_objeto, objeto_cb, LV_EVENT_PRESSING, NULL);
    lv_obj_add_event_cb(s_objeto, objeto_cb, LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(s_objeto, objeto_cb, LV_EVENT_PRESS_LOST, NULL);
    lv_obj_remove_flag(s_objeto, LV_OBJ_FLAG_GESTURE_BUBBLE);

    s_juego_capa = caja(scr);
    lv_obj_set_size(s_juego_capa, W, W);
    lv_obj_add_flag(s_juego_capa, LV_OBJ_FLAG_CLICKABLE);
    /* pantalla propia: cielo y pasto opacos (así no se dibuja el mundo de abajo) */
    lv_obj_set_style_bg_color(s_juego_capa, lv_color_hex(0x7CC8F0), 0);
    lv_obj_set_style_bg_grad_color(s_juego_capa, lv_color_hex(0xDDF1FF), 0);
    lv_obj_set_style_bg_grad_dir(s_juego_capa, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_bg_opa(s_juego_capa, LV_OPA_COVER, 0);
    { /* paisaje: sol, nubes, dos lomas y el pasto de adelante (si hay foto de este fondo, esto se oculta) */
      lv_obj_t *pint = caja(s_juego_capa);
      lv_obj_set_size(pint, W, W); lv_obj_set_pos(pint, 0, 0);
      lv_obj_set_style_bg_opa(pint, LV_OPA_TRANSP, 0);
      lv_obj_remove_flag(pint, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_set_user_data(pint, (void *)(uintptr_t)0xAD0);
      s_juego_pintura = pint;
      lv_obj_t *sol = lv_image_create(pint);
      lv_image_set_src(sol, art_icono(ICO_SOL, 5));
      lv_obj_set_pos(sol, 330, 40);
      for (int k = 0; k < 3; k++) {
          lv_obj_t *nube = caja(pint);
          lv_obj_set_size(nube, 80 + k * 14, 26 + k * 4);
          lv_obj_set_pos(nube, k == 0 ? 70 : (k == 1 ? 250 : 130), k == 0 ? 120 : (k == 1 ? 150 : 70));
          lv_obj_set_style_radius(nube, LV_RADIUS_CIRCLE, 0);
          lv_obj_set_style_bg_color(nube, lv_color_hex(0xFFFFFF), 0);
          lv_obj_set_style_bg_opa(nube, LV_OPA_80, 0);
          lv_obj_remove_flag(nube, LV_OBJ_FLAG_CLICKABLE);
      }
      for (int k = 0; k < 3; k++) { /* montañas lejanas azuladas */
          adorno(pint, -60 + k * 170, 190, 260, 200, k == 1 ? 0x8FB4D0 : 0x9DBFD8, LV_OPA_COVER, true);
          adorno(pint, 20 + k * 150 + 70, 200, 40, 20, 0xFFFFFF, LV_OPA_60, true);
      }
      static const int LOMA[3][5] = {{-80, 262, 380, 260, 0x8CCB78}, {170, 280, 420, 260, 0x6CB85C}, {-40, 318, 560, 300, 0x58A650}};
      for (int k = 0; k < 3; k++) {
          lv_obj_t *l = caja(pint);
          lv_obj_set_size(l, LOMA[k][2], LOMA[k][3]);
          lv_obj_set_pos(l, LOMA[k][0], LOMA[k][1]);
          lv_obj_set_style_radius(l, LV_RADIUS_CIRCLE, 0);
          lv_obj_set_style_bg_color(l, lv_color_hex((uint32_t)LOMA[k][4]), 0);
          lv_obj_set_style_bg_opa(l, LV_OPA_COVER, 0);
          lv_obj_remove_flag(l, LV_OBJ_FLAG_CLICKABLE);
      }
      arbol_adorno(pint, 60, 330, 96, 0x3E9B47, 0x66C468);
      arbol_adorno(pint, 402, 336, 110, 0x358E3F, 0x5DB862);
      arbol_adorno(pint, 340, 318, 64, 0x4BA84F, 0x72CE70);
      for (int k = 0; k < 2; k++) adorno(pint, 120 + k * 200, 90 + k * 30, 14, 4, 0x3A3A55, LV_OPA_COVER, false); /* pájaros */
      lv_obj_t *pasto = caja(pint);
      lv_obj_set_size(pasto, W, 120); lv_obj_set_pos(pasto, 0, W - 120);
      lv_obj_set_style_bg_color(pasto, lv_color_hex(0x4E9E4A), 0);
      lv_obj_set_style_bg_grad_color(pasto, lv_color_hex(0x347A38), 0);
      lv_obj_set_style_bg_grad_dir(pasto, LV_GRAD_DIR_VER, 0);
      lv_obj_set_style_bg_opa(pasto, LV_OPA_COVER, 0);
      lv_obj_remove_flag(pasto, LV_OBJ_FLAG_CLICKABLE);
      for (int k = 0; k < 14; k++) flor_adorno(pint, 20 + (k * 71) % 430, 372 + (k * 29) % 70, k % 3 == 0 ? 0xFF8FB8 : (k % 3 == 1 ? 0xFFFFFF : 0xFFE15A));
      for (int k = 0; k < 16; k++) adorno(pint, 10 + (k * 59) % 440, 362 + (k * 37) % 90, 3, 9, 0x3E8F40, LV_OPA_COVER, false); /* matas de pasto */
      }
    s_juego_foto = lv_image_create(s_juego_capa); /* el huerto como foto (con la hora, la estación y el clima): un solo dibujo, no cientos de formas */
    lv_obj_remove_flag(s_juego_foto, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_pos(s_juego_foto, 0, 0);
    lv_obj_add_flag(s_juego_foto, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(s_juego_capa, capa_juego_cb, LV_EVENT_PRESSING, NULL);
    lv_obj_add_event_cb(s_juego_capa, capa_juego_cb, LV_EVENT_PRESSED, NULL);
    ocultar(s_juego_capa, true);
    s_canasta = lv_image_create(scr);
    lv_image_set_src(s_canasta, art_icono(ICO_CANASTA, 5));
    ocultar(s_canasta, true);
    for (int i = 0; i < N_FRUTAS_J; i++) { s_fr_img[i] = lv_image_create(scr); ocultar(s_fr_img[i], true); }
    s_fr_bar = lv_bar_create(scr);
    lv_obj_set_size(s_fr_bar, 160, 10);
    lv_obj_align(s_fr_bar, LV_ALIGN_TOP_MID, 0, 74);
    lv_bar_set_range(s_fr_bar, 0, (int32_t)(FRUTAS_MS / 200));
    lv_obj_set_style_bg_color(s_fr_bar, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(s_fr_bar, LV_OPA_40, 0);
    lv_obj_set_style_bg_color(s_fr_bar, lv_color_hex(0xFFD45C), LV_PART_INDICATOR);
    ocultar(s_fr_bar, true);

    s_cuenta = caja(scr);
    lv_obj_set_size(s_cuenta, W, W);
    lv_obj_set_style_bg_color(s_cuenta, lv_color_hex(0x0B0A14), 0);
    lv_obj_set_style_bg_opa(s_cuenta, 225, 0);
    s_cuenta_txt = lv_image_create(s_cuenta);
    lv_obj_t *ojos = lv_image_create(s_cuenta);
    lv_image_set_src(ojos, art_icono(ICO_CARA_CANSADA, 4)); /* ojos cerrados: cuentas mientras se esconde */
    lv_obj_align(ojos, LV_ALIGN_CENTER, 0, -100);
    ocultar(s_cuenta, true);

    s_salir = boton(scr, 56, 56, salir_cb, 0);
    lv_obj_set_style_radius(s_salir, LV_RADIUS_CIRCLE, 0);
    lv_obj_align(s_salir, LV_ALIGN_BOTTOM_MID, 0, -40);
    lv_obj_t *x = icono(s_salir, ICO_NO, 2);
    lv_obj_center(x);
    ocultar(s_salir, true);
    crear_hongos(scr);
}

/* ======================================================================
 * Fase 4: vínculo, habilidades, recuerdos y sorpresas
 * ==================================================================== */

/* --- te sigue con el dedo (cuando ya confía) --- */
static int s_sigue_x, s_sigue_y;
static uint32_t s_sigue_hasta;

static void fondo_arrastre_cb(lv_event_t *e)
{
    (void)e;
    lv_point_t pt = {0, 0};
    lv_indev_t *in = lv_indev_active();
    if (in) lv_indev_get_point(in, &pt);
    if (s_juego == JG_NADA && !s_interior) niebla_arrastre(pt.x); /* barrer la niebla (aunque duerma) */
    if (!s_pet || s_pet->dormida || s_pet->etapa == STAGE_HUEVO || s_juego != JG_NADA || s_interior) return;
    if (s_colocando >= 0) { arrastrar_fantasma(pt.x, pt.y); return; }
    s_sigue_x = pt.x;
    s_sigue_y = pt.y + 20;
    s_sigue_hasta = s_ms + 500;
}

/* la mascota mientras arrastras el dedo por el mundo: si confía te sigue; si no, solo mira */
static bool animar_seguir(criatura_t *c, const pet_t *p, float *sy, bool *caminando)
{
    if ((int32_t)(s_ms - s_sigue_hasta) > 0) return false;
    art_pose_t *ps = &c->pose;
    ps->espejo = s_sigue_x < c->x;
    ps->orejas = 1;
    if (pet_bond_level(p) < VINCULO_AMIGOS) { /* todavía no: te mira, curiosa */
        ps->expr = EXP_CURIOSO;
        return true;
    }
    c->tx = (float)s_sigue_x; c->ty = (float)s_sigue_y;
    limitar(&c->tx, &c->ty);
    *caminando = mover(c, 5.0f);
    ps->expr = EXP_FELIZ; ps->cola = 1; ps->boca = true;
    if (*caminando) *sy = -fabsf(sinf((float)s_ms / 100.0f)) * 5;
    return true;
}

/* --- nadar y trepar --- */
static lv_obj_t *s_agua;          /* tapa la mitad de abajo cuando nada */
static uint32_t s_hab_desde;
static int s_hab_act = -1;
static uint32_t s_cae_hasta;      /* se está cayendo del árbol */
static const int16_t ARBOL_TREPA[ESC_COUNT][3] = { /* x, pie del tronco, copa (pantalla) */
    {80, 196, 132}, {307, 196, 126}, {111, 262, 200}, {185, 192, 124}, {0, 0, 0}, {294, 186, 118}, {0, 0, 0}, {0, 0, 0}};

static void crear_habilidades(void)
{
    s_agua = caja(s_mundo);
    lv_obj_set_style_bg_color(s_agua, lv_color_hex(0x4DA6FF), 0);
    lv_obj_set_style_bg_opa(s_agua, LV_OPA_80, 0);
    lv_obj_set_style_radius(s_agua, 14, 0);
    lv_obj_set_style_border_width(s_agua, 2, 0);
    lv_obj_set_style_border_color(s_agua, lv_color_hex(0xCFF0FF), 0);
    lv_obj_set_style_border_side(s_agua, LV_BORDER_SIDE_TOP, 0);
    ocultar(s_agua, true);
}

static bool animar_habilidad(criatura_t *c, const pet_t *p, float *sx, float *sy, bool *caminando)
{
    int act = p->actividad;
    art_pose_t *ps = &c->pose;
    const bool estanque = s_escena == ESC_PRADO; /* en el prado nada en su estanque */
    bool nada = act == ACTV_NADAR && (s_escena == ESC_LAGO || estanque) && !s_interior;
    bool trepa = act == ACTV_TREPAR && s_escena < ESC_COUNT && ARBOL_TREPA[s_escena][0];
    if ((int32_t)(s_ms - s_cae_hasta) < 0) { /* ¡pum! */
        c->ty = (float)ARBOL_TREPA[s_escena < ESC_COUNT ? s_escena : 0][1];
        c->y += 9; if (c->y > c->ty) c->y = c->ty;
        ps->expr = EXP_MIEDO; ps->orejas = -1;
        ocultar(s_agua, true);
        return true;
    }
    if (act != s_hab_act) {
        s_hab_act = act; s_hab_desde = s_ms;
        if (nada) { /* se mete al agua: la primera vez del día, dice lo rica que está */
            static int s_agua_dia = -1;
            const int dia = (int)(s_ultimo_t / 86400u);
            sonido(SND_CHAPUZON);
            if (dia != s_agua_dia) {
                static const char *const T[] = {"¡QUÉ AGUA TAN RICA!", "¡EL AGUA ESTÁ DELICIOSA!", "¡QUÉ FRESQUITA! ¡ME ENCANTA!"};
                s_agua_dia = dia;
                globo(ICO_NADAR, T[azar() % 3], 3200);
            }
        }
    }
    if (!nada) ocultar(s_agua, true);
    if (nada) {
        float v = p->habilidad[HAB_NADAR];
        float ancho = v < 40 ? 50 : (v < 85 ? 130 : 160); /* al principio chapotea en la orilla */
        float yb = v < 40 ? 196 : 172;
        float cx0 = CX;
        if (estanque) { ancho = v < 40 ? 22 : 38; yb = 296; cx0 = 104; }
        float k = (float)(s_ms - s_hab_desde) / 3000.0f;
        float xx = cx0 + sinf(k) * ancho, yy = yb + cosf(k * 0.7f) * (estanque ? 6 : (v < 40 ? 3 : 10));
        if (hypotf(c->x - xx, c->y - yy) > 30 && s_ms - s_hab_desde < 4000) { c->tx = xx; c->ty = yy; *caminando = mover(c, 4.0f); }
        else { c->x = c->tx = xx; c->y = c->ty = yy; ps->espejo = cosf(k) < 0; }
        bool buceo = v >= 85 && (s_ms / 4000) % 4 == 3; /* las expertas bucean */
        int l = lado(c), alto = buceo ? l : l * 30 / 100;
        lv_obj_set_size(s_agua, l * 7 / 10, alto);
        lv_obj_set_pos(s_agua, (int)c->x - l * 7 / 20, (int)c->y - alto + 6);
        ocultar(s_agua, false);
        lv_obj_move_foreground(s_agua);
        ps->expr = v < 20 ? EXP_CURIOSO : EXP_FELIZ; ps->boca = true; ps->orejas = 1;
        *sy = sinf((float)s_ms / 300.0f) * 2;
        if (s_ms % 900 < TICK_MS) salpicar((int)c->x + (ps->espejo ? 26 : -26), (int)c->y - 8, 0xCFF0FF);
        return true;
    }
    if (trepa) {
        const int16_t *a = ARBOL_TREPA[s_escena];
        float v = p->habilidad[HAB_TREPAR];
        float base = a[1], tope = v < 30 ? a[1] - (a[1] - a[2]) * 0.5f : a[2]; /* aprendiendo: hasta la mitad */
        if (fabsf(c->x - a[0]) > 4 || (c->y > base + 2 && s_ms - s_hab_desde < 5000)) { /* va al tronco */
            c->tx = a[0]; c->ty = base;
            *caminando = mover(c, 3.6f);
            if (!*caminando) s_hab_desde = s_ms;
            return true;
        }
        float k = fminf(1.0f, (float)(s_ms - s_hab_desde) / (v < 40 ? 6000.0f : 3500.0f));
        c->x = c->tx = a[0];
        c->y = c->ty = base - (base - tope) * k;
        ps->postura = 0; ps->orejas = 1; ps->cola = 1;
        if (k < 1) { *sx = ((s_ms / 160) % 2) ? 2 : -2; ps->expr = EXP_CURIOSO; ps->frame = (int)((s_ms / 160) % 2); }
        else { ps->expr = EXP_FELIZ; ps->espejo = (s_ms / 2500) % 2; } /* arriba: mira el paisaje */
        lv_obj_move_foreground(c->img);
        (void)sy;
        return true;
    }
    return false;
}

/* --- cosas raras: arcoíris, estrella fugaz, ciervo, globo --- */
#define ARCO_N 7
static lv_obj_t *s_arco_img, *s_raro_img, *s_raro_sombra;
/* (v96) el arcoíris es UNA imagen pintada una vez (antes eran 7 arcos de LVGL que cambiaban de opacidad en cada cuadro:
 * redibujaban casi toda la pantalla con máscaras de arco, ~80 ms por cuadro, y todo se ponía lento mientras estaba) */
#define ARCO_X0 0
#define ARCO_Y0 48
#define ARCO_W 466
#define ARCO_H 226
static uint32_t *s_arco_px;
static lv_image_dsc_t s_arco_dsc;
static int s_arco_opa = -1;
static uint32_t s_arco_t0;
static art_img_t s_raro_ball_art;
/* da vuelta una imagen de izquierda a derecha (la ballena sale por la derecha: tiene que ir de frente) */
static void img_espejar(art_img_t *im)
{
    if (!im->px) return;
    const int w = (int)im->dsc.header.w, h = (int)im->dsc.header.h;
    for (int y = 0; y < h; y++) for (int x = 0; x < w / 2; x++) { uint32_t t = im->px[y * w + x]; im->px[y * w + x] = im->px[y * w + w - 1 - x]; im->px[y * w + w - 1 - x] = t; }
} /* la ballena beluga de nubes que pasa por el cielo */
static float s_raro_x, s_raro_y;
static float s_vent_x; static bool s_vent_avisado;
static lv_obj_t *s_vent, *s_vent_img; /* la ventana de la casita: el clima de afuera y el ciervo que pasa */
#define VENT_NUBES 3
#define VENT_GOTAS 18
/* El clima de la ventana se pinta en un solo lienzo de 114×114 con la forma del vidrio (círculo): no son 25 objetos sueltos
 * y nada se sale del círculo. Cada cuadro se rehace por software (unos 13 mil píxeles). */
#define VENT_D 114
static uint32_t *s_vent_px;          /* ARGB8888 (alfa recto) */
static lv_image_dsc_t s_vent_dsc;
static lv_obj_t *s_vent_lienzo;
static int16_t s_vent_xl[VENT_D], s_vent_xr[VENT_D]; /* por fila: de dónde a dónde llega el círculo */
static bool s_vent_con_suelo;        /* pasa el ciervo: se pinta el pasto */
static void ventana_ciervo(bool ver)
{
    s_vent_con_suelo = ver;
    if (s_vent_img) ocultar(s_vent_img, !ver);
}
static int s_raro_visto = -1;

static void raro_cb(lv_event_t *e)
{
    (void)e;
    if (!s_pet || s_pet->raro >= RARO_COUNT) return;
    if (s_pet->raro == RARO_GLOBO && pet_whale_can_ride(s_pet, s_ultimo_t)) { cielo_empezar(s_pet); return; } /* la ballena baja: ¡a pasear! */
    if (s_pet->raro == RARO_ESTRELLA_FUGAZ) globo(ICO_ESTRELLA_FUGAZ, "¡PIDE UN DESEO!", 2500);
    for (int i = 0; i < 3; i++) flotar(i == 1 ? ICO_ESTRELLA : ICO_CORAZON, NULL, 0, (int)s_raro_x - 20 + i * 20, (int)s_raro_y - 30, (uint32_t)i * 150);
}

static void crear_raros(lv_obj_t *scr)
{
    s_arco_img = lv_image_create(scr);
    lv_obj_remove_flag(s_arco_img, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_pos(s_arco_img, ARCO_X0, ARCO_Y0);
    ocultar(s_arco_img, true);
    s_raro_sombra = caja(s_mundo);
    lv_obj_set_size(s_raro_sombra, 40, 12);
    lv_obj_set_style_bg_color(s_raro_sombra, lv_color_hex(0x10301A), 0);
    lv_obj_set_style_bg_opa(s_raro_sombra, LV_OPA_20, 0);
    lv_obj_set_style_radius(s_raro_sombra, LV_RADIUS_CIRCLE, 0);
    ocultar(s_raro_sombra, true);
    s_raro_img = lv_image_create(scr);
    lv_obj_add_flag(s_raro_img, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(s_raro_img, 16);
    lv_obj_add_event_cb(s_raro_img, raro_cb, LV_EVENT_CLICKED, NULL);
    ocultar(s_raro_img, true);
    /* la ventana redonda de la casita: por ahí se ve pasar al ciervo */
    s_vent = lv_obj_create(scr);
    lv_obj_remove_style_all(s_vent);
    lv_obj_set_size(s_vent, 114, 114); /* justo el vidrio de la ventana: el clima llega hasta la orilla */
    lv_obj_set_pos(s_vent, 316 - 57, 113 - 57);
    lv_obj_set_style_radius(s_vent, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_clip_corner(s_vent, true, 0); /* lo de adentro (lluvia, nubes, pasto) se corta en círculo: no se ven las esquinas */
    lv_obj_remove_flag(s_vent, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    /* el lienzo del clima de afuera (círculo) */
    s_vent_px = (uint32_t *)calloc((size_t)VENT_D * VENT_D, 4);
    for (int y = 0; y < VENT_D; y++) {
        const float dy = (float)y + 0.5f - VENT_D / 2.0f, hw = sqrtf(fmaxf(0.0f, (VENT_D / 2.0f) * (VENT_D / 2.0f) - dy * dy));
        s_vent_xl[y] = (int16_t)ceilf(VENT_D / 2.0f - hw);
        s_vent_xr[y] = (int16_t)floorf(VENT_D / 2.0f + hw);
    }
    memset(&s_vent_dsc, 0, sizeof s_vent_dsc);
    s_vent_dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
    s_vent_dsc.header.cf = LV_COLOR_FORMAT_ARGB8888;
    s_vent_dsc.header.w = VENT_D;
    s_vent_dsc.header.h = VENT_D;
    s_vent_dsc.header.stride = VENT_D * 4;
    s_vent_dsc.data_size = (uint32_t)VENT_D * VENT_D * 4;
    s_vent_dsc.data = (const uint8_t *)s_vent_px;
    s_vent_lienzo = lv_image_create(s_vent);
    lv_obj_remove_flag(s_vent_lienzo, LV_OBJ_FLAG_CLICKABLE);
    if (s_vent_px) lv_image_set_src(s_vent_lienzo, &s_vent_dsc);
    s_vent_img = lv_image_create(s_vent); /* el ciervo, por encima */
    lv_obj_remove_flag(s_vent_img, LV_OBJ_FLAG_CLICKABLE);
    ventana_ciervo(false);
    ocultar(s_vent, true);
}

/* la ventana de la casita muestra el clima y la hora de afuera */
static void animar_ventana(void)
{
    const pet_t *p = s_pet;
    if ((s_casa_fase == 2 || s_casa_fase == 4) && s_mascota.img) { /* el fundido de vuelta; si sale, ella aparece de a poco */
        const float f = (float)(s_ms - s_casa_t) / (s_casa_fase == 2 ? 500.0f : 650.0f);
        casa_negro(1.0f - f * (s_casa_fase == 2 ? 1.0f : 1.6f));
        lv_obj_set_style_opa(s_mascota.img, (lv_opa_t)(s_casa_fase == 4 ? (f * 255 > 255 ? 255 : f * 255) : 255), 0);
        if (f >= 1.0f) { s_casa_fase = 0; casa_negro(0); lv_obj_set_style_opa(s_mascota.img, LV_OPA_COVER, 0); }
    }
    if (!s_vent) return;
    const bool ver = p && s_interior && s_panel == PANEL_NINGUNO;
    const int cl = p ? (int)pet_weather(p) : CLIMA_DESPEJADO; /* el mismo clima de afuera */
    const bool hay_ciervo = s_vent_img && !lv_obj_has_flag(s_vent_img, LV_OBJ_FLAG_HIDDEN);
    /* con cielo despejado y sin ciervo no hay nada que dibujar: la ventana ni se procesa (el fondo ya trae su cielo) */
    const bool algo = cl == CLIMA_NUBLADO || cl == CLIMA_LLUVIA || cl == CLIMA_TORMENTA || cl == CLIMA_NIEVE || cl == CLIMA_VIENTO || cl == CLIMA_FRIO || hay_ciervo;
    ocultar(s_vent, !(ver && algo));
    if (!ver || !algo) return;
    static uint32_t s_vent_t;
    if (s_ms - s_vent_t < (s_fondo_periodo == PERIODO_NOCHE ? 90u : 40u) && !hay_ciervo) return; /* la lluvia y las nubes no necesitan más de ~25 cuadros por segundo (de noche, ~11: la escena ya pesa más) */
    s_vent_t = s_ms;
    const bool noche = s_fondo_periodo == PERIODO_NOCHE;
    uint32_t tinte = 0; int opa = 0;
    uint32_t nube = 0xF2F4FA; int nubes = 0; float vel = 0.012f;
    int gotas = 0; bool nieve = false;
    switch (cl) {
    case CLIMA_NUBLADO: tinte = 0x8E98AC; opa = 110; nubes = 3; nube = 0xE6E9F2; break;
    case CLIMA_LLUVIA: tinte = 0x58627A; opa = 150; nubes = 3; nube = 0x8C95AA; gotas = 14; break;
    case CLIMA_TORMENTA: tinte = 0x2C3248; opa = 195; nubes = 3; nube = 0x424A62; gotas = 18; vel = 0.02f; break;
    case CLIMA_NIEVE: tinte = 0xDCE6F2; opa = 105; nubes = 2; nube = 0xEEF2FA; gotas = 16; nieve = true; vel = 0.006f; break;
    case CLIMA_VIENTO: tinte = 0xB8D4EC; opa = 40; nubes = 3; nube = 0xFFFFFF; vel = 0.05f; break;
    case CLIMA_FRIO: tinte = 0xBFD8F0; opa = 70; nubes = 1; nube = 0xEAF2FA; break;
    default: break;
    }
    if (noche && opa) { /* de noche, todo más oscuro */
        tinte = ((tinte >> 1) & 0x7F7F7F) + 0x080A18; nube = ((nube >> 1) & 0x7F7F7F) + 0x0A0C1C; opa += 40;
    }
    if (!s_vent_px) return;
    uint32_t *px = s_vent_px;
    /* base: el tinte del cielo, solo dentro del círculo */
    const uint32_t base = ((uint32_t)(opa > 255 ? 255 : opa) << 24) | (tinte & 0xFFFFFF);
    for (int y = 0; y < VENT_D; y++) {
        uint32_t *f = px + y * VENT_D;
        for (int x = 0; x < VENT_D; x++) f[x] = (x >= s_vent_xl[y] && x <= s_vent_xr[y]) ? base : 0;
    }
    /* pintar sobre el lienzo, siempre dentro del círculo */
#define VENT_MEZCLA(dst, col, al) do { \
        const uint32_t d_ = (dst); const int da_ = (int)(d_ >> 24), a_ = (al); \
        const int oa_ = a_ + da_ * (255 - a_) / 255; \
        if (oa_ > 0) { \
            const int r_ = ((int)((col) >> 16 & 255) * a_ * 255 + (int)(d_ >> 16 & 255) * da_ * (255 - a_)) / (oa_ * 255); \
            const int g_ = ((int)((col) >> 8 & 255) * a_ * 255 + (int)(d_ >> 8 & 255) * da_ * (255 - a_)) / (oa_ * 255); \
            const int b_ = ((int)((col) & 255) * a_ * 255 + (int)(d_ & 255) * da_ * (255 - a_)) / (oa_ * 255); \
            (dst) = ((uint32_t)oa_ << 24) | ((uint32_t)r_ << 16) | ((uint32_t)g_ << 8) | (uint32_t)b_; } } while (0)
#define VENT_RECT(X0, Y0, W, H, COL, AL) do { \
        for (int yy_ = (Y0) < 0 ? 0 : (Y0); yy_ < (Y0) + (H) && yy_ < VENT_D; yy_++) { \
            const int xa_ = (X0) > s_vent_xl[yy_] ? (X0) : s_vent_xl[yy_], xb_ = (X0) + (W) - 1 < s_vent_xr[yy_] ? (X0) + (W) - 1 : s_vent_xr[yy_]; \
            for (int xx_ = xa_; xx_ <= xb_; xx_++) VENT_MEZCLA(px[yy_ * VENT_D + xx_], (COL), (AL)); } } while (0)
    if (s_vent_con_suelo) { /* el pasto por donde camina el ciervo */
        VENT_RECT(0, 77, VENT_D, 8, 0x8FD46Au, 255);
        VENT_RECT(0, 83, VENT_D, 8, 0x5DAE4Au, 255);
        VENT_RECT(0, 91, VENT_D, 30, 0x3E7F38u, 255);
    }
    for (int i = 0; i < nubes; i++) { /* nubes: elipses que pasan despacio */
        const int ancho = 46 + i * 10, alto = 20 + i * 2;
        const float f = fmodf((float)s_ms * vel * (1.0f + (float)i * 0.35f) + (float)i * 70.0f, 108.0f + (float)ancho * 2.0f);
        const int x0 = (int)f - ancho, y0 = 6 + i * 16 + (cl == CLIMA_TORMENTA ? -2 : 0);
        const float rx = (float)ancho / 2.0f, ry = (float)alto / 2.0f;
        const int al = cl == CLIMA_NUBLADO || cl == CLIMA_VIENTO ? 204 : 230;
        for (int yy = y0; yy < y0 + alto; yy++) {
            if (yy < 0 || yy >= VENT_D) continue;
            const float dy = ((float)yy + 0.5f - (float)y0 - ry) / ry, hw = rx * sqrtf(fmaxf(0.0f, 1.0f - dy * dy));
            const int xa = (int)ceilf((float)x0 + rx - hw), xb = (int)floorf((float)x0 + rx + hw);
            const int xa2 = xa > s_vent_xl[yy] ? xa : s_vent_xl[yy], xb2 = xb < s_vent_xr[yy] ? xb : s_vent_xr[yy];
            for (int xx = xa2; xx <= xb2; xx++) VENT_MEZCLA(px[yy * VENT_D + xx], nube, al);
        }
    }
    for (int i = 0; i < gotas; i++) { /* lluvia o nieve */
        if (nieve) {
            const int x = (i * 31) % 104 + (int)(sinf((float)s_ms / 700.0f + (float)i) * 5.0f), y = (int)((s_ms * 0.035f) + (float)(i * 13)) % 118 - 6;
            VENT_RECT(x, y, 4, 4, 0xFFFFFFu, 255);
        } else {
            const int y = (int)((float)s_ms * (cl == CLIMA_TORMENTA ? 0.34f : 0.24f) + (float)(i * 19)) % 128 - 12;
            const int x = ((i * 29) % 112 - (int)((float)y * 0.12f) + 112) % 112;
            VENT_RECT(x, y, 2, 9, cl == CLIMA_TORMENTA ? 0xC8D8F0u : 0xA8C8F0u, 255);
        }
    }
    if (cl == CLIMA_TORMENTA) { /* relámpagos de vez en cuando */
        const uint32_t ph = s_ms % 5200;
        const int al = ph < 110 ? 204 : (ph > 220 && ph < 300 ? 128 : 0);
        if (al) VENT_RECT(0, 0, VENT_D, VENT_D, 0xFFFFFFu, al);
    }
#undef VENT_RECT
#undef VENT_MEZCLA
    lv_obj_invalidate(s_vent_lienzo);
    if (s_vent_img) lv_obj_move_foreground(s_vent_img);
}

static bool arco_pintar(void) /* las 7 bandas, con bordes suaves y las puntas que se desvanecen; centro (CX, 360) */
{
    if (s_arco_px) return true;
    s_arco_px = (uint32_t *)calloc((size_t)ARCO_W * ARCO_H, 4);
    if (!s_arco_px) return false;
    static const uint32_t C[ARCO_N] = {0xF0506E, 0xFF8A3C, 0xFFD84A, 0x6CD86C, 0x45D0E0, 0x4C88F0, 0x9A62E0};
    const float rmax = 300.0f, rmin = 300.0f - (float)ARCO_N * 11.0f - 1.0f;
    for (int y = 0; y < ARCO_H; y++) {
        const float dy = (float)(ARCO_Y0 + y) + 0.5f - 360.0f;
        if (dy >= 0) continue;
        for (int x = 0; x < ARCO_W; x++) {
            const float dx = (float)(ARCO_X0 + x) + 0.5f - (float)CX, d2 = dx * dx + dy * dy;
            if (d2 > (rmax + 1.0f) * (rmax + 1.0f) || d2 < rmin * rmin) continue;
            const float d = sqrtf(d2), sn = -dy / d; /* seno del ángulo hacia arriba: las puntas (200° y 340°) en 0.342 */
            if (sn < 0.30f) continue;
            const float punta = sn < 0.46f ? (sn - 0.30f) / 0.16f : 1.0f;
            const float k = (rmax - d) / 11.0f; /* 0 afuera (rojo) ... 7 adentro (morado) */
            int b = (int)k; if (b < 0) b = 0; if (b >= ARCO_N) b = ARCO_N - 1;
            float al = 170.0f * punta;
            if (k < 0.0f) al *= 1.0f + k;                              /* borde de afuera, suavizado */
            if (k > (float)ARCO_N) al *= 1.0f - (k - (float)ARCO_N);   /* borde de adentro */
            uint32_t col = C[b];
            const float fr = k - (float)(int)k;
            if (fr > 0.85f && b + 1 < ARCO_N) { /* mezcla suave entre una banda y la siguiente */
                const float t = (fr - 0.85f) / 0.15f, u = 1.0f - t;
                const uint32_t c2 = C[b + 1];
                col = ((uint32_t)(((col >> 16) & 255) * u + ((c2 >> 16) & 255) * t) << 16) | ((uint32_t)(((col >> 8) & 255) * u + ((c2 >> 8) & 255) * t) << 8) |
                      (uint32_t)((col & 255) * u + (c2 & 255) * t);
            }
            if (al < 1.0f) continue;
            s_arco_px[y * ARCO_W + x] = ((uint32_t)(al > 255.0f ? 255.0f : al) << 24) | col;
        }
    }
    memset(&s_arco_dsc, 0, sizeof s_arco_dsc);
    s_arco_dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
    s_arco_dsc.header.cf = LV_COLOR_FORMAT_ARGB8888;
    s_arco_dsc.header.w = ARCO_W;
    s_arco_dsc.header.h = ARCO_H;
    s_arco_dsc.header.stride = ARCO_W * 4;
    s_arco_dsc.data_size = (uint32_t)ARCO_W * ARCO_H * 4;
    s_arco_dsc.data = (const uint8_t *)s_arco_px;
    lv_image_set_src(s_arco_img, &s_arco_dsc);
    s_arco_opa = -1;
    return true;
}
static void arco_soltar(void)
{
    if (!s_arco_px) return;
    lv_image_set_src(s_arco_img, NULL);
    free(s_arco_px);
    s_arco_px = NULL;
}

static void animar_raros(void)
{
    const pet_t *p = s_pet;
    const bool ciervo_dentro = p && s_interior && p->raro == RARO_CIERVO;
    int r = p && (!s_interior || ciervo_dentro) && s_escena != ESC_CUEVA && p->raro < RARO_COUNT ? p->raro : -1;
    if (s_vent && !ciervo_dentro) ventana_ciervo(false);
    if (r != RARO_ARCOIRIS) { if (s_arco_px) { ocultar(s_arco_img, true); arco_soltar(); } }
    else if (arco_pintar()) { /* sale de a poco (en 8 pasos, no en cada cuadro) y suelta destellos */
        if (r != s_raro_visto) s_arco_t0 = s_ms;
        const float ap = (float)(s_ms - s_arco_t0) / 2500.0f;
        const int paso = ap >= 1.0f ? 8 : (int)(ap * 8.0f);
        const int opa = paso * 255 / 8;
        if (opa != s_arco_opa) { s_arco_opa = opa; lv_obj_set_style_image_opa(s_arco_img, (lv_opa_t)opa, 0); }
        ocultar(s_arco_img, opa == 0);
        if (ap > 1.0f && s_ms % 360 < TICK_MS) {
            const float a = (200.0f + (float)(azar() % 140)) * 0.01745f, rr = 300.0f - (float)(azar() % ARCO_N) * 11.0f;
            chispa(CX + (int)(cosf(a) * rr), 360 + (int)(sinf(a) * rr), 0xFFFFFF, 0.0f, -0.25f);
        }
    }
    bool figura = r == RARO_ESTRELLA_FUGAZ || r == RARO_CIERVO || r == RARO_GLOBO;
    bool cruzo = r == RARO_CIERVO && r == s_raro_visto && s_raro_x > W + 40; /* el ciervo cruza una sola vez */
    if (cruzo) figura = false;
    ocultar(s_raro_img, !figura || s_interior);
    ocultar(s_raro_sombra, !figura || s_interior || (r != RARO_GLOBO && r != RARO_CIERVO));
    if (r != s_raro_visto) { /* empieza: de dónde sale */
        s_raro_visto = r;
        lv_image_set_scale(s_raro_img, 256);
        lv_image_set_rotation(s_raro_img, 0);
        if (r == RARO_CIERVO) { s_raro_x = -40; s_raro_y = 236; lv_image_set_src(s_raro_img, art_icono(ICO_CIERVO, 5)); }
        if (r == RARO_GLOBO) { s_raro_x = W + 40; s_raro_y = 120; art_ballena(&s_raro_ball_art, 0); img_espejar(&s_raro_ball_art); poner_src(s_raro_img, &s_raro_ball_art.dsc); lv_image_set_scale(s_raro_img, 150); }
        if (r == RARO_ESTRELLA_FUGAZ) { s_raro_x = 80; s_raro_y = 60; lv_image_set_src(s_raro_img, art_icono(ICO_ESTRELLA_FUGAZ, 6)); }
    }
    if (ciervo_dentro && r == RARO_CIERVO && s_raro_x < 0 && !s_vent_avisado) { /* dormida: el ciervo pasa por la ventana sin que ella reaccione */
        s_vent_avisado = true;
        s_vent_x = -30.0f; /* duerme: el ciervo pasa por la ventana y ella ni se entera */
    }
    if (r != RARO_CIERVO) s_vent_avisado = false;
    if (!figura) { ventana_ciervo(false); return; }
    if (ciervo_dentro) { /* el ciervo cruza la ventana de lado a lado, sin detenerse */
        s_vent_x += 0.9f;
        const bool dentro = s_vent_x > -40 && s_vent_x < 150;
        ventana_ciervo(dentro);
        if (dentro) {
            const art_icono_t fr = ((s_ms / 260) % 2) ? ICO_CIERVO_B : ICO_CIERVO_C;
            const lv_image_dsc_t *fd = art_icono(fr, 4);
            if (lv_image_get_src(s_vent_img) != fd) lv_image_set_src(s_vent_img, fd);
            lv_obj_set_pos(s_vent_img, (int)s_vent_x - 24 + 17, 33 + (((s_ms / 260) % 2) ? -1 : 0));
        }
        if (s_vent_x >= 150) s_raro_x = W + 50; /* ya pasó: no vuelve */
        return;
    }
    if (r == RARO_CIERVO) { /* cruza despacio, se detiene a mirar */
        if ((s_ms / 3000) % 3 != 2) s_raro_x += 1.4f;
        const bool pausa = (s_ms / 3000) % 3 == 2; /* camina, camina... y se detiene a pastar y mirar */
        const art_icono_t fr = !pausa ? (((s_ms / 220) % 2) ? ICO_CIERVO_B : ICO_CIERVO_C) : ((s_ms % 3000) < 1700 ? ICO_CIERVO_D : ICO_CIERVO);
        const lv_image_dsc_t *fd = art_icono(fr, 6);
        if (lv_image_get_src(s_raro_img) != fd) lv_image_set_src(s_raro_img, fd);
        lv_obj_set_pos(s_raro_img, (int)s_raro_x - 36, (int)s_raro_y - 66 + (!pausa && ((s_ms / 220) % 2) ? -1 : 0));
        lv_obj_set_pos(s_raro_sombra, (int)s_raro_x - 20, (int)s_raro_y - 6);
    } else if (r == RARO_GLOBO) { /* flota por el cielo; su sombra pasa por el pasto */
        s_raro_x -= 1.8f; /* la ballena: antes 0.7, tardaba más de un minuto en cruzar */
        if (s_raro_x < -60) s_raro_x = W + 60;
        s_raro_y = 110 + sinf((float)s_ms / 1500.0f) * 8;
        lv_obj_set_pos(s_raro_img, (int)s_raro_x - 125, (int)s_raro_y - 65);
        lv_obj_set_pos(s_raro_sombra, (int)s_raro_x - 10, 300);
    } else { /* estrella fugaz: cruza rápido cada tanto */
        /* cruza el cielo de lado a lado dejando la estela hacia atrás, con polvo de estrellas */
        uint32_t k = s_ms % 5000;
        float u = (float)k / 1500.0f;
        lv_image_set_pivot(s_raro_img, 36, 36);
        lv_image_set_rotation(s_raro_img, 600);
        if (k < 1500) {
            s_raro_x = 50 + u * 350; s_raro_y = 44 + u * 120 - sinf(u * 3.1416f) * 14;
            const float f = u < 0.15f ? u / 0.15f : (u > 0.75f ? (1.0f - u) / 0.25f : 1.0f);
            lv_obj_set_style_opa(s_raro_img, (lv_opa_t)(255.0f * f), 0);
            if (s_ms % 120 < TICK_MS) chispa((int)s_raro_x - 22 - (int)(azar() % 14), (int)s_raro_y - 8 + (int)(azar() % 12), 0xBFD0FF, -0.2f, 0.25f);
        } else lv_obj_set_style_opa(s_raro_img, LV_OPA_TRANSP, 0);
        lv_obj_set_pos(s_raro_img, (int)s_raro_x - 36, (int)s_raro_y - 36);
    }
    if (s_panel == PANEL_NINGUNO) lv_obj_move_foreground(s_raro_img); /* con un menú abierto, queda detrás */
}

/* --- fechas especiales y su cumpleaños --- */
static lv_obj_t *s_fecha_img[3], *s_gorro;
static int s_fecha_vista = -1;

static void crear_fechas(lv_obj_t *scr)
{
    for (int i = 0; i < 3; i++) { s_fecha_img[i] = lv_image_create(i == 2 ? scr : s_mundo); ocultar(s_fecha_img[i], true); }
    s_gorro = lv_image_create(s_mundo);
    lv_image_set_src(s_gorro, art_icono(ICO_GORRO, 3));
    ocultar(s_gorro, true);
}

static void animar_fechas(void)
{
    const pet_t *p = s_pet;
    if (!p) return;
    int f = p->etapa != STAGE_HUEVO && !s_interior && s_zona_pend < 0 ? (int)pet_special_date(p, s_ultimo_t) : FECHA_NINGUNA;
    if (f != s_fecha_vista) {
        s_fecha_vista = f;
        for (int i = 0; i < 3; i++) ocultar(s_fecha_img[i], true);
        if (f == FECHA_HALLOWEEN) {
            for (int i = 0; i < 2; i++) { lv_image_set_src(s_fecha_img[i], art_icono(ICO_CALABAZA, 4)); ocultar(s_fecha_img[i], false); }
            lv_obj_set_pos(s_fecha_img[0], LXY(LUGAR_ROCAS)[0] - 10, LXY(LUGAR_ROCAS)[1] + 10);
            lv_obj_set_pos(s_fecha_img[1], LXY(LUGAR_SENDERO)[0] - 90, LXY(LUGAR_SENDERO)[1] - 40);
        } else if (f == FECHA_NAVIDAD) {
            lv_image_set_src(s_fecha_img[0], art_icono(ICO_ARBOL_NAVIDAD, 6)); ocultar(s_fecha_img[0], false);
            lv_obj_set_pos(s_fecha_img[0], LXY(LUGAR_ROCAS)[0] - 20, LXY(LUGAR_ROCAS)[1] - 76);
            lv_image_set_src(s_fecha_img[1], art_icono(ICO_REGALO, 3)); ocultar(s_fecha_img[1], false);
            lv_obj_set_pos(s_fecha_img[1], LXY(LUGAR_ROCAS)[0] + 40, LXY(LUGAR_ROCAS)[1] - 22);
        } else if (f == FECHA_FIESTAS_PATRIAS) {
            lv_image_set_src(s_fecha_img[2], art_icono(ICO_BANDERINES, 4)); ocultar(s_fecha_img[2], false);
            lv_obj_align(s_fecha_img[2], LV_ALIGN_TOP_MID, 0, 104);
            lv_image_set_src(s_fecha_img[0], art_icono(ICO_BANDERINES, 4)); ocultar(s_fecha_img[0], false);
            lv_obj_set_pos(s_fecha_img[0], CX - 120, 126);
            lv_image_set_src(s_fecha_img[1], art_icono(ICO_BANDERINES, 4)); ocultar(s_fecha_img[1], false);
            lv_obj_set_pos(s_fecha_img[1], CX + 72, 126);
        }
    }
    bool noche = s_fondo_periodo == PERIODO_NOCHE;
    if (f == FECHA_ANIO_NUEVO && noche && s_ms % 1300 < TICK_MS) { /* fuegos artificiales */
        static const uint32_t C[] = {0xF0506E, 0xFFE066, 0x6FB7FF, 0x7FD36B, 0xB48CFF};
        int x = 120 + (int)(azar() % 226), y = 90 + (int)(azar() % 80);
        uint32_t col = C[azar() % 5];
        for (int k = 0; k < 8; k++) salpicar(x - 14 + (k % 4) * 9, y - (k / 4) * 8, col);
    }
    if (f == FECHA_SAN_VALENTIN && s_ms % 2600 < TICK_MS) flotar(ICO_CORAZON, NULL, 0, 120 + (int)(azar() % 226), 200 + (int)(azar() % 100), 0);
    if (f == FECHA_PRIMAVERA && s_ms % 2200 < TICK_MS) flotar(ICO_FLOR, NULL, 0, 120 + (int)(azar() % 226), 200 + (int)(azar() % 100), 0);
    if (f == FECHA_HALLOWEEN && noche) { /* las calabazas brillan de noche */
        lv_opa_t o = (lv_opa_t)(150 + 105 * (0.5f + 0.5f * sinf((float)s_ms / 400.0f)));
        for (int i = 0; i < 2; i++) lv_obj_set_style_opa(s_fecha_img[i], o, 0);
    }
    /* su cumpleaños: el gorrito de fiesta todo el día */
    bool gorro = (pet_her_birthday(p, s_ultimo_t) || s_ny_gorro) && s_mascota.visible && !p->dormida && s_juego != JG_ESCONDIDAS;
    ocultar(s_gorro, !gorro);
    if (gorro) {
        criatura_t *c = &s_mascota;
        static const uint8_t ARRIBA[4] = {32, 50, 41, 32}; /* dónde empieza la cabeza en cada etapa (px a escala 5) */
        int tope = (int)c->y - lado(c) + c->pose.escala + ARRIBA[p->etapa & 3] * c->pose.escala / 5;
        lv_obj_set_pos(s_gorro, (int)c->x - 18 + (c->pose.espejo ? -6 : 6), tope - 28);
        lv_obj_move_foreground(s_gorro);
    }
}

/* --- sueños de lo que vivió hoy --- */
static const art_icono_t ICO_VIVIDO[VIV_COUNT] = {
    ICO_PELOTA, ICO_PEZ, ICO_AMIGOS, ICO_MARIPOSA, ICO_ZORRO, ICO_LLUVIA, ICO_COPO, ICO_PATITA, ICO_NADAR, ICO_TREPAR,
    ICO_COFRE, ICO_FRUTA, ICO_CORAZON, ICO_ZONA_BOSQUE, ICO_ZONA_LAGO, ICO_ZONA_COLINA, ICO_ZONA_CUEVA, ICO_ARCOIRIS,
    ICO_PASTEL, ICO_FLOR, ICO_ESCONDIDAS, ICO_CANASTA, ICO_PAJARO, ICO_PELUCHE, ICO_PERCHA};

/* elige una cosa que vivió hoy (o -1) */
static int recuerdo_de_hoy(const pet_t *p, int distinto)
{
    int op[VIV_COUNT], n = 0;
    for (int v = 0; v < VIV_COUNT; v++) if (((p->vivido >> v) & 1u) && v != distinto) op[n++] = v;
    return n ? op[azar() % (uint32_t)n] : -1;
}

/* --- diario, álbum y colecciones --- */

static void diario_cb(lv_event_t *e)
{
    int q = (int)(intptr_t)lv_event_get_user_data(e);
    if (q == 20) { s_cofre_vista = 0; panel_cofre(); return; } /* volver al cofre */
    if (q < 4) { s_diario_tab = q; s_diario_pag = 0; s_foto_pag = 0; }
    else if (q == 10 && s_diario_pag > 0) s_diario_pag--;
    else if (q == 11) s_diario_pag++;
    panel_diario();
}

static void fecha_corta(uint32_t t, char *buf, int n)
{
    static const char *const MES[12] = {"ENE", "FEB", "MAR", "ABR", "MAY", "JUN", "JUL", "AGO", "SEP", "OCT", "NOV", "DIC"};
    int m = 1, d = 1;
    if (s_pet) pet_local_date(s_pet, t, NULL, &m, &d);
    snprintf(buf, (size_t)n, "%d %s", d, MES[(m - 1) % 12]);
}

static const art_icono_t ICO_ANIMAL[ANIMAL_COUNT];
static const art_icono_t ICO_CRIATURA[CRI_COUNT];
static art_icono_t icono_diario(const entrada_diario_t *e)
{
    static const art_icono_t ZONA[ESC_COUNT] = {ICO_ZONA_PRADO, ICO_ZONA_BOSQUE, ICO_ZONA_LAGO, ICO_ZONA_COLINA, ICO_ZONA_CUEVA, ICO_ZONA_ISLA, ICO_ZONA_CLARO, ICO_ZONA_MIRADOR};
    static const art_icono_t RARO[RARO_COUNT] = {ICO_ARCOIRIS, ICO_ESTRELLA_FUGAZ, ICO_CIERVO, ICO_GLOBO_AIRE};
    static const art_icono_t FECHA[FECHA_COUNT] = {ICO_ESTRELLA, ICO_CALABAZA, ICO_ARBOL_NAVIDAD, ICO_FUEGOS, ICO_CORAZON, ICO_FLOR, ICO_BANDERINES};
    static const art_icono_t PESCA[PESCA_COUNT] = {ICO_CANA, ICO_PEZ, ICO_BOTA, ICO_PIEDRA, ICO_CONCHA, ICO_ALGA, ICO_CANGREJO, ICO_NENUFAR,
                                                   ICO_OBJETO(OBJ_BOTELLA), ICO_OBJETO(OBJ_LLAVE_OXIDADA), ICO_PEZ_DORADO, ICO_HIELO};
    static const art_icono_t HALL[HALL_COUNT] = {ICO_FRUTA, ICO_MIEL, ICO_HIERBA, ICO_SEMILLA, ICO_PIEDRA, ICO_RAMITA, ICO_CHAPITA, ICO_HOJA_SECA, ICO_MUSGO};
    static const art_icono_t COL[COL_COUNT] = {ICO_PIEDRA, ICO_CONCHA, ICO_PLUMA, ICO_HOJA, ICO_CRISTAL};
    int a = e->a;
    switch (e->tipo) {
    case DIA_NACIO: return ICO_HUEVO;
    case DIA_CRECIO: return ICO_ESTRELLA;
    case DIA_ZONA: return ZONA[(unsigned)a % ESC_COUNT];
    case DIA_TRUCO: return ICO_TRUCOS[(unsigned)a % TRUCO_COUNT];
    case DIA_HABILIDAD: return a == HAB_NADAR ? ICO_NADAR : ICO_TREPAR;
    case DIA_VECINO: return ICO_AMIGOS;
    case DIA_PESCA: return PESCA[(unsigned)a % PESCA_COUNT];
    case DIA_TESORO: return a == COL_CRISTALES && e->b == 3 ? ICO_HIELO : COL[(unsigned)a % COL_COUNT]; /* (fw 104) */
    case DIA_COLECCION: return ICO_COFRE;
    case DIA_RARO: return RARO[(unsigned)a % RARO_COUNT];
    case DIA_META: return ICO_ESTAMPITA;
    case DIA_CUMPLE_DUENO: return ICO_PASTEL;
    case DIA_SU_CUMPLE: return ICO_GORRO;
    case DIA_FECHA: return FECHA[(unsigned)a % FECHA_COUNT];
    case DIA_VINCULO: return ICO_CORAZON;
    case DIA_REGALO: return HALL[(unsigned)a % HALL_COUNT];
    case DIA_AMIGO: return ICO_AMIGOS;
    case DIA_NUEVO: return e->a ? ICO_JUG[(unsigned)e->b % JUG_COUNT] : ICO_ROPA[(unsigned)e->b % ROPA_COUNT];
    case DIA_ESTRENO: return ICO_JUG[(unsigned)a % JUG_COUNT];
    case DIA_ROPA: return ICO_ROPA[(unsigned)a % ROPA_COUNT];
    case DIA_OBJETO: return ICO_OBJETO(((unsigned)e->a & 0x7F) | ((unsigned)e->b & 1) << 7);
    case DIA_BALLENA: return ICO_GLOBO_AIRE;
    case DIA_GUARDIANA: return ICO_OBJETO(OBJ_CORAZON_BOSQUE);
    case DIA_RITUAL: return a ? ICO_LUNA : ICO_SOL;
    case DIA_SEMANA: return ICO_CALENDARIO;
    case DIA_FAMILIA: return ICO_HUEVO;
    case DIA_BAILE: return ICO_OBJETO(101);
    case DIA_CARTA: return ICO_CARTA;
    case DIA_ANIMAL: return (unsigned)a < ANIMAL_COUNT ? ICO_ANIMAL[a] : ICO_HUELLA;
    case DIA_ANIMAL_NUEVO: { int f = (uint8_t)a; return f < FAUNA_COUNT ? ICO_FAUNA[f] : (f < PET_FICHAS ? ICO_ANIMAL[f - FAUNA_COUNT] : ICO_HUELLA); }
    case DIA_CRIATURA: return ICO_CRIATURA[(uint8_t)a % CRI_COUNT];
    case DIA_HISTORIA: return icono_narrador((uint8_t)a < NARR_COUNT ? (uint8_t)a : 0, e->b);
    case DIA_TILDE: return ICO_TILDE;
    case DIA_LUGAR: return a ? ICO_NUBE : ICO_CUCHARA_PALO;
    case DIA_EXCURSION: return e->b == 7 ? ICO_PAPELITO : (a ? ICO_CONDOR : ICO_CHUCAO);
    case DIA_PASEO_JUNTOS: return ICO_HUELLA; /* (fw 104) */
    case DIA_HIELO: return ICO_GOTA;         /* (fw 104) se derritió: queda una gotita */
    case DIA_FINAL: return ICO_ESTRELLA;
    case DIA_GUSTO: case DIA_GUSTO_SEM: return ICO_CORAZON;
    case DIA_CANCION: return ICO_NOTA;
    case DIA_NUESTRA: return ICO_NOTA; /* (v38) nuestra canción */
    case DIA_MELODIA: return ICO_NOTA;
    case DIA_MINI: { static const art_icono_t M[MINI_COUNT] = {ICO_ESTRELLA_FUGAZ, ICO_LLUVIA, ICO_COPO, ICO_ESTRELLA, ICO_SOL, ICO_LUNA, ICO_LUNA, ICO_SOL}; return M[(unsigned)a < MINI_COUNT ? a : 0]; }
    case DIA_ESTACION: { static const art_icono_t E[4] = {ICO_FLOR, ICO_SOL, ICO_HOJA, ICO_COPO}; return E[(unsigned)a & 3]; }
    case DIA_FIESTA: { static const art_icono_t F[9] = {ICO_ESTRELLA, ICO_CALABAZA, ICO_ARBOL_NAVIDAD, ICO_FUEGOS, ICO_CORAZON, ICO_FLOR, ICO_BANDERINES, ICO_PASTEL, ICO_RAMO_AMARILLO}; return F[(uint8_t)a < 9 ? (uint8_t)a : 0]; }
    case DIA_ESPECIAL: { static const art_icono_t E[ESP_COUNT] = {ICO_HONGO_LUZ, ICO_ESTRELLA, ICO_COMETA, ICO_CRISTAL}; return E[(uint8_t)a % ESP_COUNT]; }
    default: return ICO_LIBRO;
    }
}

/* color de cada pieza de colección (se tiñe el ícono) */
static uint32_t color_pieza(int c, int i)
{
    static const uint32_t PIEDRA[5] = {0xF0506E, 0x4D7FE0, 0x5DBB63, 0xFFC93C, 0xB48CFF};
    static const uint32_t CONCHA[4] = {0xFFB3C8, 0xF2D2A0, 0xF4F1FF, 0xFF9A3C};
    static const uint32_t PLUMA[4] = {0x6FB7FF, 0xF0506E, 0xFFE066, 0xFFFFFF};
    static const uint32_t HOJA[4] = {0x9BE07A, 0x3FA86A, 0xE86A3A, 0xBFD8FF};
    static const uint32_t CRISTAL[4] = {0x7FE0D6, 0xB48CFF, 0xFFF6C8, 0xBFE8FF};
    switch (c) {
    case COL_PIEDRAS: return PIEDRA[i % 5];
    case COL_CONCHAS: return CONCHA[i % 4];
    case COL_PLUMAS: return PLUMA[i % 4];
    case COL_HOJAS: return HOJA[i % 4];
    default: return CRISTAL[i % 4];
    }
}
static const art_icono_t ICO_COL[COL_COUNT] = {ICO_PIEDRA, ICO_CONCHA, ICO_PLUMA, ICO_HOJA, ICO_CRISTAL};

static lv_obj_t *icono_pieza(lv_obj_t *padre, int c, int i, int escala, bool tiene)
{
    const bool hielo = c == COL_CRISTALES && i == 3; /* (fw 104) el trocito de hielo tiene su propio dibujo */
    lv_obj_t *ic = icono(padre, hielo ? ICO_HIELO : ICO_COL[c], escala);
    if (hielo && tiene) return ic;
    lv_obj_set_style_image_recolor(ic, lv_color_hex(tiene ? color_pieza(c, i) : 0x2A2238), 0);
    lv_obj_set_style_image_recolor_opa(ic, tiene ? LV_OPA_50 : LV_OPA_COVER, 0);
    if (!tiene) lv_obj_set_style_opa(ic, LV_OPA_30, 0);
    return ic;
}

/* una foto del álbum: se vuelve a dibujar la escena (cielo según la hora, la zona, ella y lo especial) */
static void dibujar_foto(lv_obj_t *padre, const foto_t *f, int slot, int x, int y)
{
    lv_obj_t *m = caja(padre);
    lv_obj_set_size(m, 120, 104);
    lv_obj_set_pos(m, x, y);
    bool noche = f->hora >= 20 || f->hora < 6, tarde = !noche && (f->hora >= 18 || f->hora < 7);
    lv_obj_set_style_bg_color(m, lv_color_hex(noche ? 0x2A2C68 : (tarde ? 0xFFB07A : 0x9ED8FF)), 0);
    lv_obj_set_style_bg_grad_color(m, lv_color_hex(f->escena == ESC_CUEVA ? 0x4A4562 : (noche ? 0x2E5A44 : 0x7FD36B)), 0);
    lv_obj_set_style_bg_grad_dir(m, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_bg_opa(m, LV_OPA_COVER, 0);
    const bool oro = s_pet && pet_final_seen(s_pet, PET_FINAL_SECRETO); /* tras el final secreto, todas las fotos llevan marco dorado */
    lv_obj_set_style_border_width(m, oro ? 5 : 4, 0);
    lv_obj_set_style_border_color(m, lv_color_hex(oro ? 0xE8B83C : 0xFFFFFF), 0);
    if (oro) { lv_obj_set_style_shadow_width(m, 10, 0); lv_obj_set_style_shadow_color(m, lv_color_hex(0xFFD76A), 0); lv_obj_set_style_shadow_opa(m, LV_OPA_50, 0); }
    lv_obj_set_style_radius(m, 6, 0);
    static const art_icono_t ZONA[ESC_COUNT] = {ICO_ZONA_PRADO, ICO_ZONA_BOSQUE, ICO_ZONA_LAGO, ICO_ZONA_COLINA, ICO_ZONA_CUEVA, ICO_ZONA_ISLA, ICO_ZONA_CLARO, ICO_ZONA_MIRADOR};
    lv_obj_t *z = icono(m, ZONA[f->escena % ESC_COUNT], 6);
    lv_obj_set_style_opa(z, LV_OPA_40, 0);
    lv_obj_align(z, LV_ALIGN_TOP_RIGHT, 18, -8);
    static const art_expresion_t EX[MOOD_COUNT] = {EXP_FELIZ, EXP_ABIERTOS, EXP_TRISTE, EXP_CERRADOS, EXP_TRISTE, EXP_ABURRIDO, EXP_TRISTE,
                                                   EXP_ENOJADO, EXP_MAREADO, EXP_FELIZ, EXP_CERRADOS, EXP_CURIOSO, EXP_MIEDO, EXP_ORGULLO, EXP_CELOS};
    int look = s_pet && f->especie == s_pet->especie ? pet_look(s_pet) : f->especie; /* es ella: con su mezcla y sus colores */
    lv_obj_t *yo = mini_criatura(m, slot, look, f->etapa, f->forma, 3, EX[f->emocion % MOOD_COUNT]);
    lv_obj_align(yo, LV_ALIGN_BOTTOM_MID, f->vecino_especie != 0xFF ? -22 : 0, -2);
    if (f->vecino_especie != 0xFF) {
        lv_obj_t *v = mini_criatura(m, slot + 1, (f->vecino_especie & 7) | ((f->vecino_especie >> 3) & 7) << 7, f->etapa, 0, 2, EXP_FELIZ);
        lv_obj_align(v, LV_ALIGN_BOTTOM_MID, 32, -4);
    }
    entrada_diario_t e = {f->t, f->motivo, f->a, 0, 0};
    if (f->motivo == DIA_HABILIDAD) e.a = (int8_t)(f->a / 10);
    lv_obj_t *pr = icono(m, f->motivo == DIA_CRECIO ? ICO_ESTRELLA : icono_diario(&e), 3);
    lv_obj_align(pr, LV_ALIGN_TOP_LEFT, 4, 4);
    char fc[16];
    fecha_corta(f->t, fc, sizeof fc);
    lv_obj_t *t = texto(padre, fc, C_GRIS, 2, 0);
    lv_obj_set_pos(t, x + 4, y + 106);
}

/* el dibujo de cada meta: en modo infante es lo único que se ve */
static art_icono_t icono_meta(int m)
{
    static const art_icono_t I[META_COUNT] = {ICO_PELOTA, ICO_FRUTA, ICO_REGADERA, ICO_PATITA, ICO_MANO, ICO_FLECHA, ICO_CANA, ICO_DUCHA};
    return m >= 0 && m < META_COUNT ? I[m] : ICO_META;
}

/* --- una nota: al tocar un recuerdo (un tesoro, una foto, una página) se abre y se lee entero; otro toque la cierra --- */
static void nota_cerrar_cb(lv_event_t *e) { lv_obj_delete_async((lv_obj_t *)lv_event_get_user_data(e)); }
static void nota_mostrar(art_icono_t ico, uint32_t tinte, const char *titulo, const char *cuerpo)
{
    if (!s_tarjeta) return;
    lv_obj_t *velo = caja(s_tarjeta);
    lv_obj_set_size(velo, lv_obj_get_width(s_tarjeta), lv_obj_get_height(s_tarjeta));
    lv_obj_set_pos(velo, 0, 0);
    lv_obj_add_flag(velo, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(velo, nota_cerrar_cb, LV_EVENT_CLICKED, velo);
    lv_obj_t *n = caja(velo);
    lv_obj_set_size(n, 290, 230);
    lv_obj_align(n, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_radius(n, 28, 0);
    lv_obj_set_style_bg_color(n, lv_color_hex(C_BOTON), 0);
    lv_obj_set_style_bg_opa(n, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(n, 3, 0);
    lv_obj_set_style_border_color(n, lv_color_hex(0xFFE27A), 0);
    lv_obj_add_flag(n, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(n, nota_cerrar_cb, LV_EVENT_CLICKED, velo);
    lv_obj_t *ic = icono(n, ico, 3);
    lv_obj_align(ic, LV_ALIGN_TOP_MID, 0, 12);
    if (tinte) { lv_obj_set_style_image_recolor(ic, lv_color_hex(tinte), 0); lv_obj_set_style_image_recolor_opa(ic, LV_OPA_50, 0); }
    lv_obj_t *ar = area_texto(n, 14, 54, 262, 168);
    lv_obj_set_style_pad_row(ar, 8, 0);
    lv_obj_add_event_cb(ar, nota_cerrar_cb, LV_EVENT_CLICKED, velo);
    lv_obj_add_flag(ar, LV_OBJ_FLAG_CLICKABLE);
    if (titulo) texto_l(ar, titulo, 0xFFE27A, 2, 250);
    if (cuerpo) texto_l(ar, cuerpo, C_BLANCO, 2, 250);
    lv_obj_update_layout(ar); /* la nota mide lo que su texto (si es muy largo, se desliza) */
    {
        int contenido = 0;
        for (uint32_t k = 0; k < lv_obj_get_child_count(ar); k++) contenido += lv_obj_get_height(lv_obj_get_child(ar, (int32_t)k)) + 8;
        contenido += 8;
        if (contenido < 168) { lv_obj_set_height(ar, contenido); lv_obj_set_height(n, 54 + contenido + 14); }
    }
    area_lista(ar);
}

/* lo que cuenta cada tesoro (y dónde buscar los que faltan) */
static const char *const PIEZA_TXT[COL_COUNT][5] = {
    {"ESTÁ SIEMPRE TIBIA, COMO SI GUARDARA UN POQUITO DE SOL.", "SI LA MIRAS DE CERCA, PARECE QUE ADENTRO LLUEVE.",
     "HUELE A MUSGO. DICEN QUE LAS PLANTAS CRECEN MÁS CERCA DE ELLA.", "LA MÁS DIFÍCIL DE ENCONTRAR. BRILLA AUNQUE NO HAYA LUZ.",
     "DE NOCHE ZUMBA BAJITO, COMO SI CANTARA."},
    {"SI LA ACERCAS AL OÍDO, SE ESCUCHA LA LAGUNA.", "DA VUELTAS Y VUELTAS. ELLA DICE QUE ADENTRO VIVE UN SECRETO.",
     "BRILLA CON TODOS LOS COLORES CUANDO LE DA EL SOL.", "UNA ESTRELLA QUE SE CAYÓ AL AGUA Y DECIDIÓ QUEDARSE.", ""},
    {"ES DE UN PÁJARO QUE CANTA ANTES DE QUE LLUEVA.", "LIVIANA Y CALENTITA. LE GUSTA HACERSE COSQUILLAS CON ELLA.",
     "SE LA REGALÓ EL VIENTO UNA TARDE DE SOL.", "TAN SUAVE QUE CASI NO SE SIENTE. PARECE DE NUBE.", ""},
    {"VERDE CLARITO, DE LAS PRIMERAS QUE BROTAN. HUELE A LLUVIA NUEVA.", "GRANDE Y FUERTE: SIRVE DE SOMBRERO LOS DÍAS DE CALOR.",
     "ROJA Y DORADA. CRUJE BONITO CUANDO LA PISAS.", "TIENE ESCARCHA EN LOS BORDES Y NUNCA SE DERRITE.", ""},
    {"FRÍO COMO EL AGUA DE LA CUEVA. HACE ECO SI LE HABLAS.", "GUARDA LOS SUEÑOS DE LAS NOCHES LARGAS.",
     "SE ENCIENDE SOLO CUANDO ESTÁ CERCA DE TI.",
     "LO PESCÓ EN EL LAGO CONGELADO. ESTÁ FRÍO Y BRILLA COMO VIDRIO... PERO CUANDO SE ACABA EL INVIERNO, SE DERRITE. HAY QUE DISFRUTARLO MIENTRAS DURA.", ""}};
static const char *const PIEZA_DONDE[COL_COUNT] = {
    "SE ESCONDEN EN EL CERRO Y EN EL PRADO. CUANDO OLFATEE ALGO, AYÚDAL{A} A ESCARBAR.",
    "ESTÁN EN LA LAGUNA: ESCARBANDO EN LA ORILLA O PESCANDO.",
    "CAEN EN EL BOSQUE. A VECES SE LAS DEJAN LOS PÁJAROS.",
    "CADA HOJA APARECE SOLO EN SU ESTACIÓN. HAY QUE ESPERARLA.",
    "VIVEN EN LA CUEVA, BIEN ESCONDIDOS."};
static void tesoro_cb(lv_event_t *e)
{
    int q = (int)(intptr_t)lv_event_get_user_data(e), c = q / 8, i = q % 8;
    if (!s_pet || c >= COL_COUNT) return;
    bool tiene = (s_pet->coleccion[c] >> i) & 1u;
    char ti[64];
    const bool hielo = c == COL_CRISTALES && i == 3; /* (fw 104) */
    snprintf(ti, sizeof ti, "%s", tiene ? pet_piece_name((coleccion_t)c, i) : (hielo ? "TODAVÍA NO LO ENCUENTRA" : "TODAVÍA NO LA ENCUENTRA"));
    nota_mostrar(hielo ? ICO_HIELO : ICO_COL[c], hielo ? 0 : (tiene ? color_pieza(c, i) : 0x2A2238), ti,
                 tiene ? PIEZA_TXT[c][i] : (hielo ? "SOLO EN INVIERNO: HAY QUE PESCARLO EN EL LAGO CONGELADO. FUERA DEL INVIERNO SE DERRITE, ASÍ QUE CADA INVIERNO SE PUEDE PESCAR OTRO." : PIEZA_DONDE[c]));
}
static void foto_nota_cb(lv_event_t *e)
{
    int i = (int)(intptr_t)lv_event_get_user_data(e);
    const foto_t *f = s_pet ? pet_album_get(s_pet, i) : NULL;
    if (!f) return;
    entrada_diario_t d = {f->t, f->motivo, f->a, 0, 0};
    char fc[16], txt[120];
    fecha_corta(f->t, fc, sizeof fc);
    pet_diary_text(s_pet, &d, txt, sizeof txt);
    nota_mostrar(ICO_FOTO, 0, fc, txt);
}
static void diario_nota_cb(lv_event_t *e)
{
    int i = (int)(intptr_t)lv_event_get_user_data(e);
    const entrada_diario_t *d = s_pet ? pet_diary_get(s_pet, i) : NULL;
    if (!d) return;
    if (d->tipo == DIA_HISTORIA) { panel_historia((uint8_t)d->a >= NARR_COUNT ? 0 : (uint8_t)d->a, d->b); return; } /* la historia se relee entera */
    if (d->tipo == DIA_NUESTRA && pet_song_count(s_pet)) { panel_canciones(); return; } /* (v38) el cuaderno: se escuchan */
    char fc[16], txt[120];
    fecha_corta(d->t, fc, sizeof fc);
    pet_diary_text(s_pet, d, txt, sizeof txt);
    nota_mostrar(icono_diario(d), 0, fc, txt);
}
/* una zona tocable invisible sobre algo dibujado */
static void tocable(lv_obj_t *padre, int x, int y, int w, int h, lv_event_cb_t cb, int q)
{
    lv_obj_t *t = caja(padre);
    lv_obj_set_pos(t, x, y);
    lv_obj_set_size(t, w, h);
    lv_obj_add_flag(t, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(t, cb, LV_EVENT_CLICKED, (void *)(intptr_t)q);
}

static void panel_diario(void)
{
    abrir_tarjeta(PANEL_DIARIO, 360, 376);
    if (!s_pet) return;
    /* cada recuerdo solo con lo suyo: su título, y una flecha para volver a los recuerdos */
    static const art_icono_t TAB[4] = {ICO_LIBRO, ICO_FOTO, ICO_CAMARA, ICO_COFRE};
    static const char *const TABN[4] = {"DIARIO", "FOTOS", "CÁMARA", "TESOROS"};
    {
        int tb = s_diario_tab & 3;
        volver(diario_cb, 20);
        lv_obj_t *ic = icono(s_tarjeta, TAB[tb], 2);
        if (s_nino) lv_obj_align(ic, LV_ALIGN_TOP_MID, 0, 20);
        else {
            lv_obj_t *t = texto(s_tarjeta, TABN[tb], C_TINTA, 3, 0);
            const lv_image_dsc_t *d = lv_image_get_src(t);
            int w = d ? (int)d->header.w : 100;
            lv_obj_align(t, LV_ALIGN_TOP_MID, 18, 18);
            lv_obj_align(ic, LV_ALIGN_TOP_MID, 18 - w / 2 - 20, 18);
        }
    }
    int total = 0, por_pag = 1;
    if (s_diario_tab == 0) {
        /* la meta de hoy: qué es y cuánto falta, en estrellitas (sin números) */
        lv_obj_t *mi = icono(s_tarjeta, s_pet->meta_hecha ? ICO_ESTAMPITA : ICO_META, 2);
        lv_obj_set_pos(mi, 40, 64);
        int mx = 72;
        if (s_pet->meta_dia) {
            if (!s_nino) {
                char mt[64];
                snprintf(mt, sizeof mt, "%s", pet_goal_name((meta_t)s_pet->meta));
                for (char *q = mt; *q; q++) if (*q >= 'a' && *q <= 'z') *q = (char)(*q - 32);
                lv_obj_t *t = texto(s_tarjeta, mt, s_pet->meta_hecha ? C_VERDE : C_TINTA, 2, 200);
                lv_obj_set_pos(t, 72, 66);
                const lv_image_dsc_t *dd = lv_image_get_src(t);
                mx = 72 + (dd ? (int)dd->header.w : 120) + 8;
                if (mx > 280) mx = 280;
            } else {
                lv_obj_t *qm = icono(s_tarjeta, icono_meta(s_pet->meta), 2); /* qué es la meta */
                lv_obj_set_pos(qm, 72, 64);
                mx = 104;
            }
            for (int k = 0; k < s_pet->meta_objetivo && k < 5; k++) {
                lv_obj_t *d = icono(s_tarjeta, ICO_ESTRELLA, 2);
                lv_obj_set_pos(d, mx + k * 16, 70);
                if (k >= s_pet->meta_progreso) lv_obj_set_style_opa(d, LV_OPA_30, 0);
            }
        }
        /* las páginas: una lista que se desliza; cada una se lee entera al tocarla */
        total = pet_diary_count(s_pet);
        por_pag = 1000;
        lv_obj_t *ar = area_texto(s_tarjeta, 20, 96, 320, 232);
        lv_obj_set_style_pad_row(ar, 8, 0);
        lv_obj_set_flex_align(ar, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
        for (int i = 0; i < total && i < 60; i++) {
            const entrada_diario_t *e = pet_diary_get(s_pet, i);
            if (!e) break;
            lv_obj_t *fila = caja(ar);
            lv_obj_set_size(fila, 306, LV_SIZE_CONTENT);
            lv_obj_set_style_pad_bottom(fila, 4, 0);
            lv_obj_add_flag(fila, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_flag(fila, LV_OBJ_FLAG_SCROLL_CHAIN_VER);
            lv_obj_add_event_cb(fila, diario_nota_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
            lv_obj_t *ic = icono(fila, icono_diario(e), s_nino ? 3 : 2);
            lv_obj_set_pos(ic, 4, 2);
            char fc[16];
            fecha_corta(e->t, fc, sizeof fc);
            lv_obj_t *ft = texto(fila, fc, C_GRIS, 2, 0);
            lv_obj_set_pos(ft, 44, 0);
            if (!s_nino) {
                char txt[96];
                pet_diary_text(s_pet, e, txt, sizeof txt);
                for (char *q = txt; *q; q++) if (*q >= 'a' && *q <= 'z') *q = (char)(*q - 32);
                lv_obj_t *t = texto(fila, txt, C_TINTA, 2, 250);
                lv_obj_set_pos(t, 44, 22);
            }
        }
        area_lista(ar);
        if (!total) { lv_obj_t *t = icono(s_tarjeta, ICO_LIBRO, 6); lv_obj_align(t, LV_ALIGN_CENTER, 0, 30); }
    } else if (s_diario_tab == 1) {
        total = pet_album_count(s_pet);
        por_pag = 4;
        for (int k = 0; k < por_pag; k++) {
            const foto_t *f = pet_album_get(s_pet, s_diario_pag * por_pag + k);
            if (!f) break;
            dibujar_foto(s_tarjeta, f, (k * 2) % (int)(sizeof s_mini / sizeof s_mini[0]), k % 2 ? 184 : 52, k / 2 ? 190 : 62);
            tocable(s_tarjeta, k % 2 ? 184 : 52, k / 2 ? 190 : 62, 120, 104, foto_nota_cb, s_diario_pag * por_pag + k);
        }
        if (!total) { lv_obj_t *t = icono(s_tarjeta, ICO_FOTO, 6); lv_obj_align(t, LV_ALIGN_CENTER, 0, 20); }
    } else if (s_diario_tab == 2) {
        fotos_tab();
    } else {
        /* una lista que se desliza: cada colección con su nombre y sus tesoros; tocar uno cuenta su historia
         * (o, si todavía no lo encuentra, dónde buscarlo) */
        por_pag = s_nino ? 24 : 8;
        s_ts_idx = 0; s_ts_lo = s_diario_pag * por_pag; s_ts_hi = s_ts_lo + por_pag;
        lv_obj_t *ar = area_texto(s_tarjeta, 20, 62, 320, 262);
        lv_obj_set_style_pad_row(ar, 6, 0);
        if (s_nino) { lv_obj_set_flex_flow(ar, LV_FLEX_FLOW_ROW_WRAP); lv_obj_set_style_pad_column(ar, 6, 0); lv_obj_set_flex_align(ar, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START); }
        for (int c = 0; c < COL_COUNT; c++) {
            int n = pet_collection_size((coleccion_t)c);
            bool completa = (s_pet->colecciones_completas >> c) & 1u;
            if (!s_pet->coleccion[c]) continue; /* una colección sin ninguna pieza todavía no aparece */
            if (!s_nino && ts_toca()) {
                lv_obj_t *cab = caja(ar);
                lv_obj_set_size(cab, 300, 26);
                char nm[48];
                snprintf(nm, sizeof nm, "%s", pet_collection_name((coleccion_t)c));
                lv_obj_t *t = texto(cab, nm, C_VIOLETA, 2, 260);
                lv_obj_align(t, LV_ALIGN_LEFT_MID, 6, 2);
                if (completa) { lv_obj_t *st = icono(cab, ICO_ESTRELLA, 2); lv_obj_align(st, LV_ALIGN_RIGHT_MID, -8, 0); }
            }
            for (int i = 0; i < n; i++) {
                bool tiene = (s_pet->coleccion[c] >> i) & 1u;
                if (!tiene) continue; /* lo que no ha encontrado no se muestra */
                if (!ts_toca()) continue;
                lv_obj_t *b = boton(ar, s_nino ? 56 : 300, s_nino ? 52 : 44, tesoro_cb, c * 8 + i);
                lv_obj_add_flag(b, LV_OBJ_FLAG_SCROLL_CHAIN_VER);
                lv_obj_t *ic = icono_pieza(b, c, i, 2, tiene);
                lv_obj_align(ic, s_nino ? LV_ALIGN_CENTER : LV_ALIGN_LEFT_MID, s_nino ? 0 : 12, 0);
                if (!s_nino) {
                    lv_obj_t *t = texto(b, tiene ? pet_piece_name((coleccion_t)c, i) : "SIN ENCONTRAR", tiene ? C_BLANCO : C_GRIS_CLARO, 2, 230);
                    lv_obj_align(t, LV_ALIGN_LEFT_MID, 50, 0);
                }
            }
        }
        tesoros_magia(ar); /* los animales y las criaturas que conocieron (solo los descubiertos) */
        total = s_ts_idx;
        area_lista(ar);
    }
    /* pasar páginas */
    if ((s_diario_tab < 2 || s_diario_tab == 3) && total > por_pag) {
        if (s_diario_pag > 0) {
            lv_obj_t *b = boton(s_tarjeta, 56, 40, diario_cb, 10);
            lv_obj_align(b, LV_ALIGN_BOTTOM_MID, -36, -12);
            lv_obj_t *fl = icono(b, ICO_FLECHA, 2); lv_image_set_pivot(fl, 12, 12); lv_image_set_rotation(fl, 1800); lv_obj_center(fl);
        }
        if ((s_diario_pag + 1) * por_pag < total) {
            lv_obj_t *b = boton(s_tarjeta, 56, 40, diario_cb, 11);
            lv_obj_align(b, LV_ALIGN_BOTTOM_MID, 36, -12);
            lv_obj_t *fl = icono(b, ICO_FLECHA, 2); lv_obj_center(fl);
        }
    }
}

/* --- un "clic" cuando se guarda una foto --- */
static void flash_foto(void)
{
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, s_flash);
    lv_anim_set_exec_cb(&a, anim_opa);
    lv_anim_set_values(&a, LV_OPA_40, LV_OPA_TRANSP);
    lv_anim_set_duration(&a, 300);
    lv_anim_start(&a);
}

/* deslizar el dedo hacia la izquierda en cualquier escenario (sobre el fondo, no sobre ella ni sobre un objeto): se abre la pantalla con sus barras de estado */
static uint32_t s_desliza_t0;
static bool s_desliza_vale, s_desliza_orilla;
static int s_desliza_lado;
static void fondo_desliza_cb(lv_event_t *e)
{
    lv_indev_t *in = lv_indev_active();
    if (!in) return;
    lv_point_t pt;
    lv_indev_get_point(in, &pt);
    const lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_PRESSED) {
        s_desliza_x0 = pt.x; s_desliza_y0 = pt.y; s_desliza_t0 = s_ms;
        s_desliza_vale = s_panel == PANEL_NINGUNO && s_juego == JG_NADA && s_colocando < 0 && s_pet && s_pet->etapa != STAGE_HUEVO;
        { /* los menús laterales se abren desde la orilla de la pantalla (no en medio del mundo): así no se abren al arrastrar cosas */
            const int ex = pt.x - CX, ey = pt.y - CX;
            s_desliza_orilla = ex * ex + ey * ey >= 150 * 150;
            s_desliza_lado = pt.x >= CX ? 1 : -1; /* desde la orilla derecha se barre a la izquierda; desde la izquierda, a la derecha */
        }
        return;
    }
    if (code != LV_EVENT_RELEASED || !s_desliza_vale) return;
    s_desliza_vale = false;
    const int dx = pt.x - s_desliza_x0, dy = pt.y - s_desliza_y0;
    if (!s_desliza_orilla) return;
    if (dx > 110 && s_desliza_lado < 0 && abs(dy) * 2 < dx && s_ms - s_desliza_t0 <= 800 && s_panel == PANEL_NINGUNO && hist_total() > 0 && LIBRO_HIST_ACTIVO) { /* hacia la derecha: el libro de historias (si ya hay alguna) */
        s_sigue_hasta = 0;
        panel_libro_hist();
        return;
    }
    if (s_desliza_lado < 0 || dx > -110 || abs(dy) * 2 > -dx || s_ms - s_desliza_t0 > 800) return; /* un barrido rápido y bastante horizontal */
    if (s_panel != PANEL_NINGUNO) return;
    s_sigue_hasta = 0; /* que no la deje caminando detrás del dedo */
    s_mas_tab = 0; s_pag_mundo = true; s_ella_pag = s_nino ? 1 : 2; /* primero la afinidad (los chicos, las barras) */
    panel_mas();
}

static void crear_fase4(lv_obj_t *scr)
{
    lv_obj_add_event_cb(s_fondo, fondo_desliza_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(s_fondo, fondo_desliza_cb, LV_EVENT_RELEASED, NULL);
    crear_habilidades();
    crear_raros(scr);
    crear_fechas(scr);
    lv_obj_add_event_cb(s_fondo, fondo_arrastre_cb, LV_EVENT_PRESSING, NULL);
}


/* ======================================================================
 * Fase 5: creatividad — cómo será, ropa, juguetes, jardín, pecera y fotos
 * ==================================================================== */

/* --- elegir especie, mezcla y colores: con el huevo, antes del nombre --- */
static int s_asp_paso, s_asp_e = -1, s_asp_m = -1, s_asp_c;
static bool s_asp_enviado;
static void panel_aspecto(void);
static void inicio_huevo(void) { s_asp_paso = 0; s_asp_e = -1; s_asp_m = -1; s_asp_c = 0; s_asp_listo = false; s_inicio_cfg = 0; panel_aspecto(); }

/* --- elegir el huevo: ocho huevos, cada uno con su símbolo; al tocarlo da su pista (lo que nace es sorpresa) --- */
static uint8_t s_asp_orden[8];
static bool s_asp_mezclado;
static art_img_t s_viv_fondo, s_viv_frente;
static bool s_viv_recortado;
static void aspecto_cb(lv_event_t *e)
{
    int q = (int)(intptr_t)lv_event_get_user_data(e);
    if (q == 100) {
        if (s_asp_e < 0) return;
        art_img_free(&s_viv_fondo); art_img_free(&s_viv_frente); s_viv_recortado = false;
        panel_genero(); /* antes del nombre: ¿niña o niño? */
        return;
    }
    s_asp_e = q;
    panel_aspecto();
}
/* después de elegir el huevo: ¿qué te hace ilusión que sea? niña, niño o que lo decida el bosque */
static void genero_cb(lv_event_t *e)
{
    int g = (int)(intptr_t)lv_event_get_user_data(e);
    if (g == 2) g = (int)(azar() & 1u); /* sorpresa: lo decide el bosque (no se dice; se nota al nacer) */
    if (s_pet && !s_pet->edad_dueno) { /* el primer inicio: se guarda; el huevo se prepara cuando ya esté la fecha y la hora */
        s_asp_listo = true; s_asp_gen = g; s_inicio_cfg = 1;
        art_img_free(&s_viv_fondo); art_img_free(&s_viv_frente); s_viv_recortado = false;
        panel_hora();
        return;
    }
    cerrar_panel();
    s_asp_enviado = true;
    accion(UI_ACCION_ASPECTO, s_asp_e); /* sin mezcla; el color se elige al nacer */
    accion(UI_ACCION_GENERO, g);        /* después: elegir el huevo deja todo en blanco */
}
static void panel_genero(void)
{
    abrir_tarjeta(PANEL_GENERO, 360, 360);
    if (!s_nino) {
        lv_obj_t *t = texto(s_tarjeta, "¿QUÉ TE HACE ILUSIÓN QUE SEA?", C_TINTA, 2, 280);
        lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 34);
    } else { lv_obj_t *t = icono(s_tarjeta, ICO_HUEVO, 3); lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 26); }
    static const char *const TXT[3] = {"NIÑA", "NIÑO", "QUE LO DECIDA EL DESTINO"};
    static const art_icono_t IC[3] = {ICO_MONO, ICO_GORRO_LANA, ICO_ESTRELLA};
    for (int k = 0; k < 3; k++) {
        lv_obj_t *b = boton(s_tarjeta, k < 2 ? 130 : 270, 62, genero_cb, k);
        lv_obj_align(b, LV_ALIGN_TOP_MID, k < 2 ? (k ? 70 : -70) : 0, k < 2 ? 96 : 176);
        lv_obj_set_style_radius(b, 22, 0);
        if (k == 2) lv_obj_set_style_bg_color(b, lv_color_hex(C_VIOLETA), 0);
        lv_obj_t *ic = icono(b, IC[k], 3);
        if (s_nino) { lv_obj_center(ic); continue; }
        lv_obj_align(ic, LV_ALIGN_LEFT_MID, 10, 0);
        lv_obj_t *tt = texto(b, TXT[k], C_BLANCO, 2, k < 2 ? 80 : 200);
        lv_obj_align(tt, LV_ALIGN_LEFT_MID, 50, 0);
    }
    if (!s_nino) {
        lv_obj_t *n = texto(s_tarjeta, "SI DEJAS QUE DECIDA EL BOSQUE, LO SABRÁS CUANDO NAZCA.", C_GRIS, 2, 270);
        lv_obj_align(n, LV_ALIGN_TOP_MID, 0, 254);
    }
    volver(aspecto_volver_cb, 0);
}
static void aspecto_volver_cb(lv_event_t *e) { (void)e; panel_aspecto(); }
/* el recuadro de la pista venía pintado con las esquinas de abajo fuera del círculo: se borra de la imagen (se repite el pasto)
 * y se dibuja uno nuevo, más angosto abajo y con esquinas redondas que sí se ven */
static void vivero_sin_recuadro(art_img_t *im)
{
    if (!im->px) return;
    const int IW = (int)im->dsc.header.w, IH = (int)im->dsc.header.h;
    if (IW < 440 || IH < 400) return;
    for (int y = 222; y < 398 && y < IH; y++)
        for (int x = 24; x < 442 && x < IW; x++) {
            uint32_t c = im->px[y * IW + x];
            int r = (c >> 16) & 255, g = (c >> 8) & 255, b = c & 255;
            bool oscuro = r < 70 && g < 80 && b > 30 && b < 120 && b > g - 12;
            bool dorado = r > 180 && g > 160 && b < 140 && r > b + 60;
            if (oscuro || dorado) im->px[y * IW + x] = im->px[(196 + (y - 222) % 24) * IW + x];
            else if (r <= 38 && g >= 55 && g <= 100 && b <= 72 && g > b) { /* la sombra verde oscura que rodeaba al recuadro */
                int nr = r * 145 / 100, ng = g * 145 / 100, nb = b * 145 / 100;
                im->px[y * IW + x] = (c & 0xFF000000u) | ((uint32_t)(nr > 255 ? 255 : nr) << 16) | ((uint32_t)(ng > 255 ? 255 : ng) << 8) | (uint32_t)(nb > 255 ? 255 : nb);
            }
        }
}
static void panel_aspecto(void)
{
    abrir_tarjeta(PANEL_ASPECTO, 466, 466);
    lv_obj_align(s_tarjeta, LV_ALIGN_CENTER, 0, 0);
    { /* el vivero se pinta con formas (sin imágenes enormes en memoria): cielo de noche, pasto y un nido para cada huevo */
        int px0 = 0, py0 = 0;
        const lv_image_dsc_t *foto = art_pantalla(0, &px0, &py0); /* el vivero como foto: cielo, pasto y nidos ya vienen en la imagen */
        if (foto) {
            lv_obj_t *fi = lv_image_create(s_tarjeta);
            lv_image_set_src(fi, foto);
            lv_obj_set_pos(fi, 0, 0);
            lv_obj_remove_flag(fi, LV_OBJ_FLAG_CLICKABLE);
        } else {
        lv_obj_t *cielo = caja(s_tarjeta);
        lv_obj_set_size(cielo, 466, 466); lv_obj_set_pos(cielo, 0, 0);
        lv_obj_set_style_bg_color(cielo, lv_color_hex(0x0B1633), 0);
        lv_obj_set_style_bg_grad_color(cielo, lv_color_hex(0x2C5E7C), 0);
        lv_obj_set_style_bg_grad_dir(cielo, LV_GRAD_DIR_VER, 0);
        lv_obj_set_style_bg_opa(cielo, LV_OPA_COVER, 0);
        lv_obj_remove_flag(cielo, LV_OBJ_FLAG_CLICKABLE); lv_obj_set_user_data(cielo, (void *)(uintptr_t)0xAD0);
        { /* cielo con estrellas y luna */
            static const int EST[14][3] = {{60,22,3},{110,40,2},{170,14,3},{300,16,2},{356,30,3},{402,52,2},{36,62,2},{430,86,3},{90,92,2},{250,10,2},{196,34,2},{330,60,2},{140,64,2},{380,22,2}};
            for (int k = 0; k < 14; k++) adorno(s_tarjeta, EST[k][0], EST[k][1], EST[k][2], EST[k][2], k % 3 ? 0xFFF3B0 : 0xFFFFFF, k % 2 ? LV_OPA_80 : LV_OPA_COVER, true);
            adorno(s_tarjeta, 36, 70, 14, 14, 0xFFF3C0, LV_OPA_30, true);
        }
        adorno(s_tarjeta, -60, 64, 330, 120, 0x2F7C45, LV_OPA_COVER, true); /* lomas lejanas */
        adorno(s_tarjeta, 200, 70, 340, 110, 0x2A7040, LV_OPA_COVER, true);
        lv_obj_t *suelo = caja(s_tarjeta);
        lv_obj_set_size(suelo, 466, 396); lv_obj_set_pos(suelo, 0, 70); s_fondo_pleno = suelo;
        lv_obj_set_style_bg_color(suelo, lv_color_hex(0x3F8F4C), 0);
        lv_obj_set_style_bg_grad_color(suelo, lv_color_hex(0x1D5230), 0);
        lv_obj_set_style_bg_grad_dir(suelo, LV_GRAD_DIR_VER, 0);
        lv_obj_set_style_bg_opa(suelo, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(suelo, 3, 0);
        lv_obj_set_style_border_side(suelo, LV_BORDER_SIDE_TOP, 0);
        lv_obj_set_style_border_color(suelo, lv_color_hex(0x7BCB6E), 0);
        lv_obj_remove_flag(suelo, LV_OBJ_FLAG_CLICKABLE); lv_obj_set_user_data(suelo, (void *)(uintptr_t)0xAD0);
        for (int k = 0; k < 4; k++) adorno(s_tarjeta, 40 + k * 8, 238 + k * 40, 386 - k * 16, 3, 0x2A6E3A, LV_OPA_50, false); /* franjas de pasto */
        { static const int FL[10][3] = {{70,206,0xFF8FB8},{120,220,0xFFFFFF},{352,208,0xFFE15A},{398,222,0xFF8FB8},{84,360,0xFFE15A},{150,384,0xFF8FB8},{316,386,0xFFFFFF},{384,364,0xFFE15A},{232,400,0xFF8FB8},{190,372,0xFFFFFF}};
          for (int k = 0; k < 10; k++) flor_adorno(s_tarjeta, FL[k][0], FL[k][1], (uint32_t)FL[k][2]); }
        for (int k = 0; k < 3; k++) { /* hongos */
            const int hx = k == 0 ? 54 : (k == 1 ? 396 : 300), hy = k == 0 ? 340 : (k == 1 ? 336 : 394);
            adorno(s_tarjeta, hx + 4, hy + 8, 6, 9, 0xF2E8D0, LV_OPA_COVER, false);
            adorno(s_tarjeta, hx - 3, hy, 20, 11, k == 1 ? 0xE0A030 : 0xD8433F, LV_OPA_COVER, true);
            adorno(s_tarjeta, hx + 3, hy + 3, 3, 3, 0xFFFFFF, LV_OPA_COVER, true);
            adorno(s_tarjeta, hx + 10, hy + 5, 3, 3, 0xFFFFFF, LV_OPA_COVER, true);
        }
        for (int k = 0; k < 9; k++) { /* luciérnagas */
            const int lx = 60 + (k * 97) % 350, ly = 190 + (k * 53) % 200;
            adorno(s_tarjeta, lx - 4, ly - 4, 12, 12, 0xFFEE88, LV_OPA_20, true);
            adorno(s_tarjeta, lx, ly, 4, 4, 0xFFF6B0, LV_OPA_COVER, true);
        }
        for (int i = 0; i < 8; i++) { /* los nidos (debajo de los huevos) */
            const int cx = 232 + (int)(((float)(i % 4) - 1.5f) * 86.0f), top = 52 + (i / 4) * 98;
            lv_obj_t *nido = caja(s_tarjeta);
            lv_obj_set_size(nido, 78, 28); lv_obj_set_pos(nido, cx - 39, top + 62);
            lv_obj_set_style_radius(nido, LV_RADIUS_CIRCLE, 0);
            lv_obj_set_style_bg_color(nido, lv_color_hex(0x8A5A30), 0);
            lv_obj_set_style_bg_opa(nido, LV_OPA_COVER, 0);
            lv_obj_set_style_border_width(nido, 3, 0);
            lv_obj_set_style_border_color(nido, lv_color_hex(0xB07A44), 0);
            lv_obj_remove_flag(nido, LV_OBJ_FLAG_CLICKABLE); lv_obj_set_user_data(nido, (void *)(uintptr_t)0xAD0);
            lv_obj_t *hueco = caja(s_tarjeta);
            lv_obj_set_size(hueco, 56, 14); lv_obj_set_pos(hueco, cx - 28, top + 69);
            lv_obj_set_style_radius(hueco, LV_RADIUS_CIRCLE, 0);
            lv_obj_set_style_bg_color(hueco, lv_color_hex(0x3A2312), 0);
            lv_obj_set_style_bg_opa(hueco, LV_OPA_COVER, 0);
            lv_obj_remove_flag(hueco, LV_OBJ_FLAG_CLICKABLE); lv_obj_set_user_data(hueco, (void *)(uintptr_t)0xAD0);
            for (int k = 0; k < 5; k++) adorno(s_tarjeta, cx - 36 + k * 15, top + 76 + (k % 2) * 3, 12, 3, k % 2 ? 0xE6C57A : 0xCFA25C, LV_OPA_COVER, false);
        }
        }
    }
    if (!s_asp_mezclado) { /* el orden de los huevos cambia cada vez */
        for (int i = 0; i < 8; i++) s_asp_orden[i] = (uint8_t)i;
        for (int i = 7; i > 0; i--) { int j = (int)(azar() % (uint32_t)(i + 1)); uint8_t x = s_asp_orden[i]; s_asp_orden[i] = s_asp_orden[j]; s_asp_orden[j] = x; }
        s_asp_mezclado = true;
    }
    /* la pista de cada uno: su símbolo se ve junto al huevo y, al tocarlo, cómo es ese huevo */
    static const char *const TIPO[8] = {"HUEVO DE HOJAS", "HUEVO DE RAYO", "HUEVO DE SOL", "HUEVO DE AGUA",
                                        "HUEVO DE TIERRA", "HUEVO DE CIELO", "HUEVO DE LUZ", "HUEVO DE SOMBRAS"};
    /* (106) la pista habla de lo que HACE el huevo: cada una insinúa cómo va a ser quien nazca, sin decirlo
     * (nada de «le asustan las tormentas» en el huevo de rayo: de ahí sale Chispa, y la tormenta la entusiasma) */
    static const char *const PISTA[8] = {
        "LE BROTA UNA HOJITA Y SE ESTIRA A LAS FLORES.",
        "CHISPORROTEA Y ESPERA LA TORMENTA.",
        "TIBIO: BUSCA EL PRIMER SOL DE LA MAÑANA.",
        "SI LO ESCUCHAS, ADENTRO SUENA UN RÍO.",
        "PESADO: SE VA HUNDIENDO SOLO EN EL NIDO.",
        "CASI NO PESA. DE NOCHE SE LE VEN PUNTITOS.",
        "SE ENCIENDE SOLO Y SIGUE EL COMPÁS SI CANTAS.",
        "SI LO MIRAS FIJO SE PONE BRUMOSO Y SE PIERDE."};
    static const art_icono_t PISTA_ICO[8] = {ICO_HOJA, ICO_RAYO, ICO_SOL, ICO_PEZ, ICO_SEMILLA, ICO_ESTRELLA, ICO_BOMBILLA, ICO_NUBE};
    if (!s_nino) { lv_obj_t *t = texto(s_tarjeta, "ELIGE UN HUEVO", 0xFFEFB8, 2, 0); lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 33); }
    else { lv_obj_t *t = icono(s_tarjeta, ICO_HUEVO, 2); lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 26); }
    lv_obj_t *bt[8];
    for (int i = 0; i < 8; i++) {
        int esp = s_asp_orden[i];
        if (esp == s_asp_e) { /* (106) el elegido: un óvalo de luz DETRÁS, que encierra al huevo Y a su nidito */
            lv_obj_t *gl = caja(s_tarjeta);
            lv_obj_set_size(gl, 96, 116);
            lv_obj_set_style_radius(gl, LV_RADIUS_CIRCLE, 0);
            lv_obj_set_style_bg_color(gl, lv_color_hex(0xFFE27A), 0);
            lv_obj_set_style_bg_opa(gl, 62, 0);
            lv_obj_set_style_border_width(gl, 2, 0);
            lv_obj_set_style_border_color(gl, lv_color_hex(0xFFF3B8), 0);
            lv_obj_set_style_border_opa(gl, 140, 0);
            lv_obj_align(gl, LV_ALIGN_TOP_LEFT, 232 + (int)(((float)(i % 4) - 1.5f) * 86.0f) - 48, 52 + (i / 4) * 98 - 8);
            lv_obj_remove_flag(gl, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_user_data(gl, (void *)(uintptr_t)0xAD0);
        }
        lv_obj_t *b = boton(s_tarjeta, 80, 92, aspecto_cb, esp);
        bt[i] = b;
        lv_obj_set_style_radius(b, 30, 0);
        lv_obj_set_style_bg_opa(b, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(b, 0, 0);
        lv_obj_align(b, LV_ALIGN_TOP_LEFT, 232 + (int)(((float)(i % 4) - 1.5f) * 86.0f) - 40, 52 + (i / 4) * 98);
        art_huevo_nuevo(&s_mini[i], esp, 6); /* huevos grandes: el texto de abajo es solo el nombre */
        lv_obj_t *m = lv_image_create(b);
        lv_image_set_src(m, &s_mini[i].dsc);
        lv_obj_center(m);
    }
    if (!s_nino) { /* el recuadro de la pista: cabe dentro del círculo, con las esquinas de abajo redondas */
        lv_obj_t *rc = caja(s_tarjeta);
        lv_obj_set_size(rc, 330, 112);
        lv_obj_align(rc, LV_ALIGN_TOP_MID, 0, 248);
        lv_obj_set_style_radius(rc, 30, 0);
        lv_obj_set_style_bg_color(rc, lv_color_hex(0x0B1424), 0);
        lv_obj_set_style_bg_opa(rc, 150, 0);   /* (106) se ve el prado por detrás, y el texto se sigue leyendo */
        lv_obj_set_style_border_width(rc, 2, 0);
        lv_obj_set_style_border_color(rc, lv_color_hex(0xD0B969), 0);
        lv_obj_set_style_border_opa(rc, 150, 0);
        lv_obj_remove_flag(rc, LV_OBJ_FLAG_CLICKABLE);
    }
    for (int i = 0; i < 8; i++) { /* su símbolo, siempre a la vista, en la esquina */
        lv_obj_t *sb = icono(s_tarjeta, PISTA_ICO[s_asp_orden[i]], 2);
        if (i == 3) lv_obj_align_to(sb, bt[i], LV_ALIGN_BOTTOM_RIGHT, -2, -4); /* el huevo de la esquina: su símbolo abajo, que arriba no cabe en el vidrio */
        else lv_obj_align_to(sb, bt[i], LV_ALIGN_TOP_RIGHT, 8, i < 4 ? 4 : -2);
    }
    if (s_asp_e < 0) {
        if (!s_nino) { lv_obj_t *t = texto(s_tarjeta, "TOCA UN HUEVO: ADENTRO HAY UNA SORPRESA", 0xFFEFB8, 2, 270); lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 276); }
    } else if (s_nino) {
        lv_obj_t *pi = icono(s_tarjeta, PISTA_ICO[s_asp_e & 7], 4);
        lv_obj_align(pi, LV_ALIGN_TOP_MID, 0, 250);
    } else {
        lv_obj_t *pi = icono(s_tarjeta, PISTA_ICO[s_asp_e & 7], 2);
        lv_obj_t *t0 = texto(s_tarjeta, TIPO[s_asp_e & 7], 0xFFD76A, 2, 0);
        const lv_image_dsc_t *d0 = lv_image_get_src(t0);
        int w0 = d0 ? (int)d0->header.w : 160;
        lv_obj_align(pi, LV_ALIGN_TOP_MID, -(w0 / 2) - 4, 256);
        lv_obj_align(t0, LV_ALIGN_TOP_MID, 14, 260);
        lv_obj_t *t = texto(s_tarjeta, PISTA[s_asp_e & 7], 0xFFEFB8, 2, 292);
        lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 288);
    }
    if (s_asp_e >= 0) {
        lv_obj_t *ok = boton(s_tarjeta, 150, 48, aspecto_cb, 100);
        ok_alinear(ok, 0);
        lv_obj_set_style_bg_color(ok, lv_color_hex(C_VERDE), 0);
        if (s_nino) { lv_obj_t *i = icono(ok, ICO_SI, 3); lv_obj_center(i); }
        else { lv_obj_t *ot = texto(ok, "¡ESTE!", C_BLANCO, 3, 0); lv_obj_center(ot); }
    }
}

/* --- el color, al nacer --- */
static int s_color_vista = -1;
static void color_cb(lv_event_t *e)
{
    int q = (int)(intptr_t)lv_event_get_user_data(e);
    if (q == 100) {
        cerrar_panel(); accion(UI_ACCION_COLOR, s_color_vista < 0 ? 0 : s_color_vista); s_color_vista = -1; s_color_pend = s_ms + 600000;
        if (s_inicio_cfg == 2) { s_inicio_cfg = 3; panel_modo_inicio(); }
        return;
    }
    s_color_vista = q;
    panel_color();
}
static void panel_color(void)
{
    if (!s_pet) return;
    abrir_tarjeta(PANEL_COLOR, 380, 380);
    if (s_color_vista < 0) s_color_vista = s_pet->color;
    if (!s_nino) { lv_obj_t *t = texto(s_tarjeta, "¿DE QUÉ COLOR?", C_TINTA, 3, 0); lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 26); }
    else { lv_obj_t *t = icono(s_tarjeta, ICO_PALETA, 2); lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 14); }
    int look = (s_pet->especie & 7) | s_color_vista << 7;
    lv_obj_t *m = mini_criatura(s_tarjeta, 8, look, 1, 0, 6, EXP_FELIZ);
    lv_obj_align(m, LV_ALIGN_TOP_MID, 0, 46);
    for (int c = 0; c < PET_COLORES; c++) {
        lv_obj_t *b = boton(s_tarjeta, 60, 44, color_cb, c);
        lv_obj_align(b, LV_ALIGN_TOP_MID, (int)(((float)(c % 4) - 1.5f) * 70.0f), 176 + (c / 4) * 54);
        lv_obj_set_style_bg_color(b, lv_color_hex(art_color_especie((s_pet->especie & 7) | c << 7)), 0);
        if (c == s_color_vista) { lv_obj_set_style_border_color(b, lv_color_hex(C_VIOLETA), 0); lv_obj_set_style_border_width(b, 5, 0); }
        if (c == 0) { lv_obj_t *st = icono(b, ICO_ESTRELLA, 2); lv_obj_center(st); } /* el de su especie */
    }
    lv_obj_t *ok = boton(s_tarjeta, 150, 48, color_cb, 100);
    ok_alinear(ok, 0);
    lv_obj_set_style_bg_color(ok, lv_color_hex(C_VERDE), 0);
    if (s_nino) { lv_obj_t *i = icono(ok, ICO_SI, 3); lv_obj_center(i); }
    else { lv_obj_t *ot = texto(ok, "¡ASÍ!", C_BLANCO, 3, 0); lv_obj_center(ot); }
}

/* --- su rincón: decoraciones, juguetes y ropa --- */
static int s_rincon_tab;
static int s_ropa_reac = -9, s_ropa_reac_r = -1; /* su reacción a lo último que le pusiste */
static uint32_t s_colocando_desde;
static lv_obj_t *s_fantasma;

static void rincon_tab_cb(lv_event_t *e)
{
    s_rincon_tab = (int)(intptr_t)lv_event_get_user_data(e);
    panel_rincon();
}

static void decor_ver_cb(lv_event_t *e)
{
    int d = (int)(intptr_t)lv_event_get_user_data(e);
    accion(UI_ACCION_DECOR_VER, d);
}

static void juguete_cb(lv_event_t *e)
{
    int j = (int)(intptr_t)lv_event_get_user_data(e);
    if (!s_pet || !pet_has_toy(s_pet, (juguete_t)j)) {
        lv_obj_set_style_translate_x(lv_event_get_current_target(e), 6, 0);
        return;
    }
    if (pet_toy_placed(s_pet, (juguete_t)j, NULL, NULL)) { accion(ACT_PONER_JUGUETE, j | 255 << 8); return; } /* guardarlo */
    int n = 0;
    for (int k = 0; k < JUG_COUNT; k++) n += pet_toy_placed(s_pet, (juguete_t)k, NULL, NULL);
    if (n >= PET_JUGUETES_PUESTOS) { lv_obj_set_style_translate_x(lv_event_get_current_target(e), 6, 0); return; }
    cerrar_panel();
    s_colocando = j;
    s_colocando_desde = s_ms;
    if (s_escena != ESC_PRADO) accion(ACT_IR, ESC_PRADO); /* los juguetes van en el prado */
    lv_image_set_src(s_fantasma, art_icono(ICO_JUG[j], j == JUG_COLUMPIO ? 5 : 4));
    lv_obj_set_pos(s_fantasma, CX - 24, 240);
    ocultar(s_fantasma, false);
    lv_obj_move_foreground(s_fantasma);
    globo(ICO_MANO, "TOCA DONDE QUIERES PONERLO", 4000);
}

static void ropa_cb(lv_event_t *e)
{
    int r = (int)(intptr_t)lv_event_get_user_data(e);
    if (!s_pet || !pet_has_clothes(s_pet, (ropa_t)r)) {
        lv_obj_set_style_translate_x(lv_event_get_current_target(e), 6, 0);
        return;
    }
    int s = pet_clothes_slot((ropa_t)r);
    s_ropa_reac = -9;
    s_ropa_reac_r = r;
    accion(ACT_VESTIR, pet_wearing(s_pet, (ranura_t)s) == r ? -(s + 1) : r);
}


/* ======================================================================
 * El cofre: accesorios, tesoros, enciclopedia, fotos, cámara y cartas
 * ==================================================================== */
static void panel_cofre_async(void *u) { (void)u; if (s_panel == PANEL_COFRE) panel_cofre(); }
static void cofre_hub_cb(lv_event_t *e)
{
    const int q = (int)(intptr_t)lv_event_get_user_data(e);
    if (q < 6) {
        switch (q) {
        case 0: s_rincon_tab = 0; panel_rincon(); return;
        case 1: s_cofre_vista = 1; s_cofre_sub = -1; break; /* tesoros: todas las categorías en cuadrícula */
        case 2: s_cofre_vista = 2; s_cofre_sub = -1; break; /* la enciclopedia abre su propio menú */
        case 3: s_diario_tab = 1; s_diario_pag = 0; panel_diario(); return;
        case 4: s_diario_tab = 2; s_diario_pag = 0; panel_diario(); return;
        default: panel_cartas(); return;
        }
    } else if (q == 20) s_cofre_vista = 0;
    else if (q == 21) { s_cofre_vista = 2; s_cofre_sub = -1; }
    else if (q >= 40 && q < 45) {
        if (q == 40) { s_diario_tab = 0; s_diario_pag = 0; panel_diario(); return; }
        if (q == 44) return; /* (el libro mágico se quitó por ahora) */
        s_cofre_sub = q - 40;
    }
    else if (q == 23) s_cofre_sub = -1; /* de una lista, a la cuadrícula de tesoros */
    else if (q >= 60 && q < 65) { s_cofre_vista = 1; s_cofre_sub = q - 60; } /* una categoría: su lista */
    else if (q == 30) { s_bolsa_desde_cofre = false; s_bolsa_pag = 0; s_bolsa_libro = 0; panel_bolsa(); return; }
    else if (q == 31) return; /* (el libro mágico se quitó por ahora) */
    else if (q == 32) { s_diario_tab = 0; s_diario_pag = 0; panel_diario(); return; }
    else if (q >= 10 && q < 20) s_cofre_sub = q - 10;
    lv_async_call(panel_cofre_async, NULL); /* (se reconstruye fuera del evento del botón) */
}
static void cofre_cat_boton(int id, art_icono_t ico, bool sel, int dx)
{
    lv_obj_t *b = boton(s_tarjeta, 70, 70, cofre_hub_cb, id);
    lv_obj_align(b, LV_ALIGN_TOP_MID, dx, 80);
    lv_obj_set_style_radius(b, 18, 0);
    if (sel) { lv_obj_set_style_bg_color(b, lv_color_hex(C_VIOLETA), 0); lv_obj_set_style_border_color(b, lv_color_hex(0xFFE27A), 0); }
    lv_obj_t *ic = icono(b, ico, 4);
    lv_obj_center(ic);
}
static void cofre_obj_cb(lv_event_t *e)
{
    const int id = (int)(intptr_t)lv_event_get_user_data(e);
    s_obj_cofre = true; s_obj_cofre_sub = s_cofre_sub;
    panel_objeto(id);
}
/* los objetos de la bolsa que son de una familia, dentro de la lista de su categoría de Tesoros: tocar uno muestra sus características */
static void cofre_objetos(lv_obj_t *ar, int cat)
{
    const int nuevo = s_pet->bolsa_nueva ? s_pet->bolsa_nueva - 1 : -1;
    int orden[PET_OBJETOS], n = 0;
    if (nuevo >= 0) orden[n++] = nuevo;
    for (int i = 0; i < PET_OBJETOS; i++) if (i != nuevo) orden[n++] = i;
    bool vio_nuevo = false;
    for (int k = 0; k < n; k++) {
        const int id = orden[k];
        const objeto_info_t *o = objeto_info(id);
        bool puesto = false;
        for (int q = 0; q < PET_OBJ_PUESTOS; q++) puesto |= s_pet->obj_puesto[q] == id + 1;
        if (o->cat != cat || !(pet_object_known(s_pet, id) || s_pet->bolsa[id] || puesto)) continue; /* solo los que ya descubrió */
        const bool tiene = s_pet->bolsa[id] > 0, listo = pet_object_ready(s_pet, id, s_ultimo_t);
        lv_obj_t *b = boton(ar, 300, 48, cofre_obj_cb, id);
        lv_obj_add_flag(b, LV_OBJ_FLAG_SCROLL_CHAIN_VER);
        lv_obj_set_style_radius(b, 18, 0);
        lv_obj_t *ic = icono_obj(b, id, 3);
        lv_obj_align(ic, LV_ALIGN_LEFT_MID, 8, 0);
        if (!listo || !tiene) lv_obj_set_style_opa(ic, LV_OPA_50, 0);
        lv_obj_t *t = texto(b, o->nombre, tiene ? C_BLANCO : C_GRIS_CLARO, 2, 214);
        lv_obj_align(t, LV_ALIGN_LEFT_MID, 54, 0);
        const lv_image_dsc_t *td = lv_image_get_src(t);
        if (td && (int)td->header.h + 18 > 48) lv_obj_set_height(b, (int)td->header.h + 18);
        if (id == nuevo) { vio_nuevo = true; lv_obj_t *st = icono(b, ICO_ESTRELLA, 2); lv_obj_align(st, LV_ALIGN_TOP_RIGHT, -6, 4); }
    }
    if (vio_nuevo) accion(UI_ACCION_OBJETO, 4 << 8); /* ya lo vio: deja de ser "nuevo" */
}
static int cofre_obj_cuenta(int cat)
{
    int n = 0;
    for (int i = 0; i < PET_OBJETOS; i++) {
        if (objeto_info(i)->cat != cat) continue;
        bool puesto = false;
        for (int k = 0; k < PET_OBJ_PUESTOS; k++) puesto |= s_pet->obj_puesto[k] == i + 1;
        n += pet_object_known(s_pet, i) || s_pet->bolsa[i] || puesto;
    }
    return n;
}
static void cofre_pieza(lv_obj_t *ar, int c, bool solo_tengo)
{
    const int n = pet_collection_size((coleccion_t)c);
    if (solo_tengo && !s_pet->coleccion[c]) return;
    lv_obj_t *cab = caja(ar);
    lv_obj_set_size(cab, 300, 24);
    lv_obj_t *t = texto(cab, pet_collection_name((coleccion_t)c), 0xFFE9A8, 2, 280);
    lv_obj_align(t, LV_ALIGN_LEFT_MID, 6, 0);
    for (int i = 0; i < n; i++) {
        const bool tiene = (s_pet->coleccion[c] >> i) & 1u;
        if (solo_tengo && !tiene) continue;
        lv_obj_t *b = boton(ar, 300, 44, tesoro_cb, c * 8 + i);
        lv_obj_add_flag(b, LV_OBJ_FLAG_SCROLL_CHAIN_VER);
        lv_obj_t *ic = icono_pieza(b, c, i, 2, tiene);
        lv_obj_align(ic, LV_ALIGN_LEFT_MID, 12, 0);
        lv_obj_t *tx = texto(b, tiene ? pet_piece_name((coleccion_t)c, i) : "SIN ENCONTRAR", tiene ? C_BLANCO : C_GRIS_CLARO, 2, 230);
        lv_obj_align(tx, LV_ALIGN_LEFT_MID, 50, 0);
    }
}
static void panel_cofre(void)
{
    abrir_tarjeta(PANEL_COFRE, 420, 420);
    if (!s_pet) return;
    if (s_cofre_vista == 0) {
        lv_obj_t *ti = texto(s_tarjeta, "COFRE", C_TINTA, 3, 0);
        lv_obj_align(ti, LV_ALIGN_TOP_MID, 0, 44);
        static const art_icono_t IC[6] = {ICO_PERCHA, ICO_COFRE, ICO_LIBRO, ICO_FOTO, ICO_CAMARA, ICO_CARTA};
        static const char *const NM[6] = {"ACCESORIOS", "TESOROS", "ENCICLOPEDIA", "FOTOS", "CÁMARA", "CARTAS"};
        for (int i = 0; i < 6; i++) {
            lv_obj_t *b = boton(s_tarjeta, 188, 66, cofre_hub_cb, i);
            lv_obj_align(b, LV_ALIGN_TOP_MID, (i % 2 ? 1 : -1) * 95, 88 + (i / 2) * 74);
            lv_obj_set_style_radius(b, 18, 0);
            if (i == 0) lv_obj_set_style_border_color(b, lv_color_hex(0xFFE27A), 0); /* lo primero que se abre */
            lv_obj_t *ic = icono(b, IC[i], 3);
            lv_obj_align(ic, LV_ALIGN_LEFT_MID, 7, 0);
            lv_obj_t *tx = texto(b, NM[i], C_BLANCO, 2, 138);
            lv_obj_align(tx, LV_ALIGN_LEFT_MID, 48, 0);
            if (i == 1 && s_pet->bolsa_nueva) { lv_obj_t *st = icono(b, ICO_ESTRELLA, 2); lv_obj_align(st, LV_ALIGN_TOP_RIGHT, -4, 4); }
        }
        return;
    }
    if (s_cofre_vista == 2 && s_cofre_sub < 0) { /* el menú de la enciclopedia: la misma organización del cofre */
        volver(cofre_hub_cb, 20);
        lv_obj_t *te = texto(s_tarjeta, "ENCICLOPEDIA", C_TINTA, 3, 0);
        lv_obj_align(te, LV_ALIGN_TOP_MID, 0, 44);
        static const art_icono_t IC[5] = {ICO_LIBRO, ICO_CRISTAL, ICO_HOJA, ICO_MARIPOSA, ICO_ESTRELLA};
        static const char *const NM[5] = {"SU DÍA A DÍA", "PIEDRAS Y MINERALES", "FLORA", "FAUNA", "OBJETOS MÁGICOS"};
        for (int i = 0; i < 4; i++) { /* (el libro mágico se quitó por ahora) */
            lv_obj_t *b = boton(s_tarjeta, 188, 66, cofre_hub_cb, 40 + i);
            lv_obj_align(b, LV_ALIGN_TOP_MID, (i % 2 ? 1 : -1) * 95, 88 + (i / 2) * 74);
            lv_obj_set_style_radius(b, 18, 0);
            lv_obj_t *ic = icono(b, IC[i], 3);
            lv_obj_align(ic, LV_ALIGN_LEFT_MID, 7, 0);
            lv_obj_t *tx = texto(b, NM[i], C_BLANCO, 2, 132);
            lv_obj_align(tx, LV_ALIGN_LEFT_MID, 48, 0);
        }
        return;
    }
    if (s_cofre_vista == 1 && s_cofre_sub < 0) { /* tesoros: todas las categorías en cuadrícula (con lo que antes estaba en la bolsa); tocar una abre su lista */
        volver(cofre_hub_cb, 20);
        if (!s_nino) { lv_obj_t *tm = texto(s_tarjeta, "TESOROS", C_TINTA, 3, 0); lv_obj_align(tm, LV_ALIGN_TOP_MID, 0, 40); }
        static const art_icono_t IC[5] = {ICO_PIEDRA, ICO_HOJA, ICO_PLUMA, ICO_MARIPOSA, ICO_ESTRELLA};
        static const char *const NM[5] = {"TIERRA", "FLORA", "PERDIDOS", "FAUNA", "MÍSTICOS"};
        static const int CAT[5] = {OCAT_TIERRA, OCAT_FLORA, OCAT_ANTIGUO, OCAT_ESPIRITUS, OCAT_ARTEFACTO};
        int cuenta[5];
        for (int c = 0; c < 4; c++) {
            int n = cofre_obj_cuenta(CAT[c]);
            static const coleccion_t CC[3][3] = {{COL_PIEDRAS, COL_CONCHAS, COL_CRISTALES}, {COL_HOJAS, COL_HOJAS, COL_HOJAS}, {COL_PLUMAS, COL_PLUMAS, COL_PLUMAS}};
            if (c < 3) {
                const int nc = c == 0 ? 3 : 1;
                for (int k = 0; k < nc; k++) for (int i = 0; i < 16; i++) n += (s_pet->coleccion[CC[c][k]] >> i) & 1u;
            }
            cuenta[c] = n;
        }
        cuenta[4] = cofre_obj_cuenta(OCAT_ARTEFACTO);
        const int nuevo_cat = s_pet->bolsa_nueva ? objeto_info(s_pet->bolsa_nueva - 1)->cat : -1;
        for (int i = 0; i < 5; i++) {
            const int fila = i / 3, col = i % 3, en_fila = fila == 0 ? 3 : 2;
            lv_obj_t *b = boton(s_tarjeta, 108, 106, cofre_hub_cb, 60 + i);
            lv_obj_align(b, LV_ALIGN_TOP_MID, (int)((col - (en_fila - 1) / 2.0f) * 114), 96 + fila * 114);
            lv_obj_set_style_radius(b, 22, 0);
            lv_obj_t *ic = icono(b, IC[i], 5);
            lv_obj_align(ic, LV_ALIGN_TOP_MID, 0, 10);
            if (!s_nino) { lv_obj_t *nt = texto(b, NM[i], 0xFFE27A, 2, 0); lv_obj_align(nt, LV_ALIGN_BOTTOM_MID, 0, -8); }
            char n[8];
            snprintf(n, sizeof n, "%d", cuenta[i]);
            lv_obj_t *cn = texto(b, n, C_BLANCO, 2, 0);
            lv_obj_align(cn, LV_ALIGN_TOP_RIGHT, -8, 6);
            if (nuevo_cat == CAT[i]) { lv_obj_t *st = icono(b, ICO_ESTRELLA, 2); lv_obj_align(st, LV_ALIGN_TOP_LEFT, 4, 4); }
        }
        return;
    }
    volver(cofre_hub_cb, s_cofre_vista == 2 ? 21 : 23);
    lv_obj_t *ti = texto(s_tarjeta, s_cofre_vista == 1 ? "TESOROS" : "ENCICLOPEDIA", C_TINTA, 3, 0);
    lv_obj_align(ti, LV_ALIGN_TOP_MID, 0, 40);
    s_ts_idx = 0; s_ts_lo = 0; s_ts_hi = 1 << 20;
    lv_obj_t *ar = area_texto(s_tarjeta, 45, s_cofre_vista == 1 ? 96 : 186, 330, s_cofre_vista == 1 ? 236 : 146);
    lv_obj_set_style_pad_row(ar, 6, 0);
    if (s_cofre_vista == 1) {
        static const art_icono_t IC[5] = {ICO_PIEDRA, ICO_HOJA, ICO_PLUMA, ICO_MARIPOSA, ICO_BOLSA};
        static const char *const NM[5] = {"TIERRA", "FLORA", "PERDIDOS", "FAUNA", "MÍSTICOS"};
        (void)IC;
        ocultar(ti, true);
        { lv_obj_t *nm = texto(s_tarjeta, NM[s_cofre_sub], 0xFFE9A8, 3, 330); lv_obj_align(nm, LV_ALIGN_TOP_MID, 0, 40); }
        switch (s_cofre_sub) {
        case 0: cofre_pieza(ar, COL_PIEDRAS, true); cofre_pieza(ar, COL_CONCHAS, true); cofre_pieza(ar, COL_CRISTALES, true); cofre_objetos(ar, OCAT_TIERRA); break;
        case 1: cofre_pieza(ar, COL_HOJAS, true); cofre_objetos(ar, OCAT_FLORA); break;
        case 2: cofre_pieza(ar, COL_PLUMAS, true); cofre_objetos(ar, OCAT_ANTIGUO); break;
        case 3: tesoros_magia(ar); cofre_objetos(ar, OCAT_ESPIRITUS); break; /* los animales y las criaturas que conoció */
        case 4: cofre_objetos(ar, OCAT_ARTEFACTO); break;
        default: {
            lv_obj_t *b = boton(ar, 300, 52, cofre_hub_cb, 30);
            lv_obj_t *ic = icono(b, ICO_BOLSA, 2);
            lv_obj_align(ic, LV_ALIGN_LEFT_MID, 12, 0);
            lv_obj_t *tx = texto(b, "ABRIR LA BOLSA", C_BLANCO, 2, 230);
            lv_obj_align(tx, LV_ALIGN_LEFT_MID, 56, 0);
        }
        }
    } else {
        static const art_icono_t IC[5] = {ICO_LIBRO, ICO_CRISTAL, ICO_HOJA, ICO_MARIPOSA, ICO_ESTRELLA};
        static const char *const NM[5] = {"SU DÍA A DÍA", "PIEDRAS Y MINERALES", "FLORA", "FAUNA", "OBJETOS MÁGICOS"};
        for (int k = 0; k < 4; k++) cofre_cat_boton(10 + k, IC[k], k == s_cofre_sub, (2 * k - 3) * 37);
        { lv_obj_t *nm = texto(s_tarjeta, NM[s_cofre_sub], 0xFFE9A8, 2, 330); lv_obj_align(nm, LV_ALIGN_TOP_MID, 0, 158); }
        switch (s_cofre_sub) {
        case 1: cofre_pieza(ar, COL_PIEDRAS, false); cofre_pieza(ar, COL_CRISTALES, false); break;
        case 2: cofre_pieza(ar, COL_HOJAS, false); break;
        case 3: tesoros_magia(ar); break; /* los animales y las criaturas que conoció */
        default: { /* su día a día (0) y el libro de objetos (4): se abren aparte */
            lv_obj_t *b = boton(ar, 300, 52, cofre_hub_cb, 32);
            lv_obj_t *ic = icono(b, ICO_LIBRO, 2);
            lv_obj_align(ic, LV_ALIGN_LEFT_MID, 12, 0);
            lv_obj_t *tx = texto(b, "ABRIR SU DÍA A DÍA", C_BLANCO, 2, 230);
            lv_obj_align(tx, LV_ALIGN_LEFT_MID, 56, 0);
        }
        }
    }
    area_lista(ar);
}

static void panel_rincon(void)
{
    abrir_tarjeta(PANEL_RINCON, 344, 316);
    if (!s_pet) return;
    static const art_icono_t TAB[3] = {ICO_MACETA, ICO_PELUCHE, ICO_PERCHA};
    for (int i = 0; i < 3; i++) {
        lv_obj_t *b = boton(s_tarjeta, 72, 42, rincon_tab_cb, i);
        lv_obj_align(b, LV_ALIGN_TOP_MID, (i - 1) * 80, 10);
        lv_obj_t *ic = icono(b, TAB[i], 2);
        lv_obj_center(ic);
        if (i == s_rincon_tab) lv_obj_set_style_bg_color(b, lv_color_hex(C_VIOLETA), 0);
    }
    if (s_rincon_tab == 0) {
        /* lo que tiene guardado */
        static const art_icono_t INV[ITEM_COUNT + 2] = {ICO_FRUTA, ICO_MIEL, ICO_HIERBA, ICO_SEMILLA, ICO_PIEDRA, ICO_RAMITA, ICO_MUSGO};
        for (int i = 0; i < ITEM_COUNT + 2; i++) { /* (v38: también las ramitas y el musgo de la casita) */
            lv_obj_t *ic = icono(s_tarjeta, INV[i], 2);
            lv_obj_set_pos(ic, 20 + i * 47, 60);
            char n[8];
            snprintf(n, sizeof n, "%d", i < ITEM_COUNT ? s_pet->inventario[i] : (i == ITEM_COUNT ? s_pet->ramitas : s_pet->musgo));
            lv_obj_t *t = texto(s_tarjeta, n, C_TINTA, 2, 0);
            lv_obj_set_pos(t, 46 + i * 47, 66);
        }
        /* decoraciones: las que no tiene, con su costo; las que tiene, se ponen o se guardan */
        for (int d = 0; d < DECOR_COUNT; d++) {
            bool tiene = pet_has_decor(s_pet, (decor_t)d);
            lv_obj_t *b = boton(s_tarjeta, 96, 96, tiene ? decor_ver_cb : decor_cb, d);
            lv_obj_set_pos(b, d < 3 ? 14 + d * 104 : 66 + (d - 3) * 104, d < 3 ? 94 : 196);
            lv_obj_t *ic = icono(b, ICO_DECOR[d], 4);
            lv_obj_align(ic, LV_ALIGN_TOP_MID, 0, 4);
            if (tiene) {
                bool ve = pet_decor_visible(s_pet, (decor_t)d);
                lv_obj_t *ok = icono(b, ve ? ICO_SI : ICO_NO, 2);
                lv_obj_align(ok, LV_ALIGN_BOTTOM_MID, 0, -6);
                if (ve) lv_obj_set_style_bg_color(b, lv_color_hex(0x3E6A50), 0);
                else lv_obj_set_style_opa(ic, LV_OPA_50, 0);
                continue;
            }
            int sem, pie;
            pet_decor_cost((decor_t)d, &sem, &pie);
            if (d == DECOR_CASITA) { /* (v38) ramitas y musgo */
                const art_icono_t MI[2] = {ICO_RAMITA, ICO_MUSGO};
                const int MC[2] = {PET_CASITA_RAMITAS, PET_CASITA_MUSGO};
                for (int k = 0; k < 2; k++) {
                    const int xk = k ? 22 : -22;
                    lv_obj_t *s = icono(b, MI[k], 2); lv_obj_align(s, LV_ALIGN_BOTTOM_MID, xk - 10, -6);
                    char n[6]; snprintf(n, sizeof n, "%d", MC[k]);
                    lv_obj_t *t = texto(b, n, 0xFFE27A, 2, 0); lv_obj_align(t, LV_ALIGN_BOTTOM_MID, xk + 12, -10);
                }
                if (!pet_materials(s_pet, NULL, NULL)) lv_obj_set_style_bg_color(b, lv_color_hex(0x4A4458), 0);
                continue;
            }
            int x = sem && pie ? -22 : 0;
            if (sem) {
                lv_obj_t *s = icono(b, ICO_SEMILLA, 2); lv_obj_align(s, LV_ALIGN_BOTTOM_MID, x - 10, -6);
                char n[6]; snprintf(n, sizeof n, "%d", sem);
                lv_obj_t *t = texto(b, n, 0xFFE27A, 2, 0); lv_obj_align(t, LV_ALIGN_BOTTOM_MID, x + 12, -10);
            }
            if (pie) {
                lv_obj_t *s = icono(b, ICO_PIEDRA, 2); lv_obj_align(s, LV_ALIGN_BOTTOM_MID, (sem ? 22 : 0) - 10, -6);
                char n[6]; snprintf(n, sizeof n, "%d", pie);
                lv_obj_t *t = texto(b, n, 0xFFE27A, 2, 0); lv_obj_align(t, LV_ALIGN_BOTTOM_MID, (sem ? 22 : 0) + 12, -10);
            }
            if (s_pet->inventario[ITEM_SEMILLA] < sem || s_pet->inventario[ITEM_PIEDRA] < pie)
                lv_obj_set_style_bg_color(b, lv_color_hex(0x4A4458), 0);
        }
    } else if (s_rincon_tab == 1) {
        int n = 0, vis_j = 0;
        for (int j = 0; j < JUG_COUNT; j++) {
            bool tiene = pet_has_toy(s_pet, (juguete_t)j), puesto = pet_toy_placed(s_pet, (juguete_t)j, NULL, NULL);
            if (!tiene) continue; /* lo que aún no encuentra no se muestra */
            n += puesto;
            lv_obj_t *b = boton(s_tarjeta, 96, 92, juguete_cb, j);
            lv_obj_set_pos(b, 14 + (vis_j % 3) * 104, 62 + (vis_j / 3) * 100); vis_j++;
            lv_obj_t *ic = icono(b, ICO_JUG[j], 4);
            lv_obj_align(ic, LV_ALIGN_CENTER, 0, puesto ? -6 : 0);
            if (puesto) {
                lv_obj_set_style_bg_color(b, lv_color_hex(0x3E6A50), 0);
                lv_obj_t *ok = icono(b, ICO_SI, 2);
                lv_obj_align(ok, LV_ALIGN_BOTTOM_MID, 0, -4);
            }
        }
        if (!s_nino) {
            char t[48];
            snprintf(t, sizeof t, "EN EL PRADO: %d DE %d", n, PET_JUGUETES_PUESTOS);
            lv_obj_t *tt = texto(s_tarjeta, t, C_GRIS, 2, 0);
            lv_obj_align(tt, LV_ALIGN_BOTTOM_MID, 0, -14);
        }
    } else {
        /* ella con lo que lleva puesto, y lo que opina */
        static art_img_t prev;
        art_pose_t ps = {pet_look(s_pet), s_pet->etapa < 1 ? 1 : s_pet->etapa, s_pet->etapa == STAGE_ADULTA ? s_pet->forma : 0, 4,
                         EXP_FELIZ, false, 0, false, false, 0, 0, 0, {0, 0, 0}, 0, s_pet->guardiana};
        for (int s = 0; s < RANURA_COUNT; s++) ps.ropa[s] = pet_worn_raw(s_pet, s);
        if (s_ropa_reac <= -1 && s_ropa_reac > -9) { ps.expr = s_ropa_reac == -3 ? EXP_TRISTE : EXP_ENOJADO; ps.orejas = -1; }
        if (s_ropa_reac == 0) ps.expr = EXP_ABIERTOS;
        art_criatura_pose(&prev, &ps);
        lv_obj_t *yo = lv_image_create(s_tarjeta);
        lv_image_set_src(yo, &prev.dsc);
        lv_obj_align(yo, LV_ALIGN_TOP_MID, 0, 56);
        if (s_ropa_reac > -9 && s_ropa_reac_r >= 0) {
            static const art_icono_t REAC[] = {ICO_CALOR, ICO_CARA_ENOJADA, ICO_CARA_TRISTE, ICO_PUNTOS, ICO_CARA_FELIZ, ICO_CORAZON};
            int k = s_ropa_reac + 3;
            if (k >= 0 && k < (int)(sizeof REAC / sizeof REAC[0])) {
                lv_obj_t *r = icono(s_tarjeta, REAC[k], 3);
                lv_obj_align(r, LV_ALIGN_TOP_MID, 66, 66);
            }
        }
        int vis = 0; /* solo sombreros y accesorios de cabeza o cara (lo del cuello tapaba el cuerpo) */
        for (int r = 0; r < ROPA_COUNT; r++) {
            if (pet_clothes_slot((ropa_t)r) == RANURA_CUELLO) continue;
            bool tiene = pet_has_clothes(s_pet, (ropa_t)r), puesta = pet_wearing(s_pet, pet_clothes_slot((ropa_t)r)) == r;
            if (!tiene) continue; /* lo que aún no encuentra no se muestra */
            lv_obj_t *b = boton(s_tarjeta, 72, 62, ropa_cb, r);
            lv_obj_set_pos(b, 20 + (vis % 4) * 78, 148 + (vis / 4) * 70); vis++;
            lv_obj_t *ic = icono(b, ICO_ROPA[r], 4);
            lv_obj_center(ic);
            if (puesta) lv_obj_set_style_bg_color(b, lv_color_hex(C_VIOLETA), 0);
            if ((s_pet->ropa_amada >> r) & 1u) { lv_obj_t *h = icono(b, ICO_CORAZON, 2); lv_obj_align(h, LV_ALIGN_TOP_RIGHT, -3, 3); }
        }
    }
}

/* firma de lo que se ve en el rincón: si cambia con el panel abierto, se vuelve a dibujar */
static uint32_t firma_rincon(const pet_t *p)
{
    return ((uint32_t)p->ropa_puesta[0] | (uint32_t)p->ropa_puesta[1] << 4 | (uint32_t)p->ropa_puesta[2] << 8 |
            (uint32_t)p->juguetes_puestos << 12 | (uint32_t)p->decor_ocultas << 20 | (p->decoraciones & 31u) << 26 |
            (uint32_t)(p->ropa_tiene != 0) << 31) ^ ((uint32_t)p->ropa_mano * 0x9E3779B1u);
}

/* --- juguetes en el prado --- */
static lv_obj_t *s_jug_img[JUG_COUNT];
static int s_jug_base[JUG_COUNT][2];
static bool s_caja_adelante;

static void juguete_mundo_cb(lv_event_t *e)
{
    int j = (int)(intptr_t)lv_event_get_user_data(e);
    if (lv_event_get_code(e) == LV_EVENT_LONG_PRESSED) { /* mantener: guardarlo */
        flotar(ICO_JUG[j], NULL, 0, s_jug_base[j][0], s_jug_base[j][1] - 60, 0);
        accion(ACT_PONER_JUGUETE, j | 255 << 8);
        return;
    }
    int n = toques_seguidos(3000 + j);
    if (n >= 3) { mostrar_quitar(QUITAR_JUGUETE, j, s_jug_base[j][0] + 26, s_jug_base[j][1] - 56); return; }
    if (n > 1) return; /* el primer toque ya la invitó */
    if (!s_pet || s_pet->dormida) return;
    flotar(ICO_CORAZON, NULL, 0, s_jug_base[j][0], s_jug_base[j][1] - 60, 0);
    accion(ACT_USAR_JUGUETE, j);
}

static void crear_juguetes(void)
{
    for (int j = 0; j < JUG_COUNT; j++) {
        s_jug_img[j] = lv_image_create(s_mundo);
        lv_image_set_src(s_jug_img[j], art_icono(ICO_JUG[j], j == JUG_COLUMPIO ? 5 : 4));
        lv_obj_add_flag(s_jug_img[j], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(s_jug_img[j], juguete_mundo_cb, LV_EVENT_SHORT_CLICKED, (void *)(intptr_t)j);
        lv_obj_add_event_cb(s_jug_img[j], juguete_mundo_cb, LV_EVENT_LONG_PRESSED, (void *)(intptr_t)j);
        lv_obj_move_to_index(s_jug_img[j], 0); /* detrás de ella y de las plantas */
        ocultar(s_jug_img[j], true);
    }
    s_fantasma = lv_image_create(s_mundo);
    lv_obj_set_style_opa(s_fantasma, LV_OPA_70, 0);
    ocultar(s_fantasma, true);
}

static void poner_juguete_img(int j, int x, int y)
{
    const lv_image_dsc_t *d = art_icono(ICO_JUG[j], j == JUG_COLUMPIO ? 5 : 4);
    lv_obj_set_pos(s_jug_img[j], x - (int)d->header.w / 2, y - (int)d->header.h + 4);
}

static void actualizar_juguetes(const pet_t *p)
{
    bool prado = s_escena == ESC_PRADO && !s_interior && p->etapa != STAGE_HUEVO;
    for (int j = 0; j < JUG_COUNT; j++) {
        int x, y;
        bool hay = prado && pet_toy_placed(p, (juguete_t)j, &x, &y);
        ocultar(s_jug_img[j], !hay);
        if (!hay) continue;
        if (x != s_jug_base[j][0] || y != s_jug_base[j][1]) {
            s_jug_base[j][0] = x; s_jug_base[j][1] = y;
            poner_juguete_img(j, x, y);
        }
    }
    if (s_colocando >= 0 && s_ms - s_colocando_desde > 20000) { s_colocando = -1; ocultar(s_fantasma, true); } /* se te olvidó */
}

/* tocaste el prado mientras ponías un juguete */
static bool colocar_toque(int x, int y)
{
    if (s_colocando < 0) return false;
    float fx = (float)x, fy = (float)y + 20.0f;
    if (fx < 80) fx = 80;
    if (fx > 386) fx = 386;
    if (fy < Y_MIN + 30) fy = Y_MIN + 30;
    if (fy > Y_MAX) fy = Y_MAX;
    accion(ACT_PONER_JUGUETE, s_colocando | ((int)fx / 2) << 8 | ((int)fy / 2) << 16);
    for (int k = 0; k < 4; k++) salpicar((int)fx - 12 + k * 8, (int)fy - 4, 0x8B5E3C);
    s_colocando = -1;
    ocultar(s_fantasma, true);
    return true;
}

static void arrastrar_fantasma(int x, int y)
{
    if (s_colocando < 0) return;
    const lv_image_dsc_t *d = art_icono(ICO_JUG[s_colocando], s_colocando == JUG_COLUMPIO ? 5 : 4);
    lv_obj_set_pos(s_fantasma, x - (int)d->header.w / 2, y + 20 - (int)d->header.h);
}

/* el mundo de los juguetes: el molinete gira (más con viento), el pelotón y el columpio se mueven con ella */
static void animar_juguetes_mundo(const pet_t *p)
{
    if (!p) return;
    float k = (float)s_ms / 1000.0f;
    if (!lv_obj_has_flag(s_jug_img[JUG_MOLINETE], LV_OBJ_FLAG_HIDDEN)) {
        int lado_m = 12 * 4;
        lv_image_set_pivot(s_jug_img[JUG_MOLINETE], lado_m / 2, lado_m / 2);
        float vel = pet_weather(p) == CLIMA_VIENTO ? 900.0f : 120.0f;
        lv_image_set_rotation(s_jug_img[JUG_MOLINETE], (int32_t)(k * vel) % 3600);
    }
    bool usando = p->actividad == ACTV_JUGUETE && !p->dormida;
    if (!(usando && p->juguete_uso == JUG_PELOTON)) poner_juguete_img(JUG_PELOTON, s_jug_base[JUG_PELOTON][0], s_jug_base[JUG_PELOTON][1]);
    if (!(usando && p->juguete_uso == JUG_COLUMPIO)) lv_image_set_rotation(s_jug_img[JUG_COLUMPIO], 0);
    bool caja = usando && p->juguete_uso == JUG_CAJA;
    if (caja != s_caja_adelante) {
        s_caja_adelante = caja;
        if (caja) lv_obj_move_foreground(s_jug_img[JUG_CAJA]);
        else lv_obj_move_to_index(s_jug_img[JUG_CAJA], 0);
    }
}

/* ella con un juguete */
static bool animar_juguete(criatura_t *c, const pet_t *p, float *sx, float *sy, bool *caminando)
{
    if (p->actividad != ACTV_JUGUETE || p->dormida || s_escena != ESC_PRADO || s_interior) return false;
    int j = p->juguete_uso % JUG_COUNT, x, y;
    if (!pet_toy_placed(p, (juguete_t)j, &x, &y)) return false;
    art_pose_t *ps = &c->pose;
    float k = (float)s_ms / 1000.0f;
    float ox = 0;
    float tx = (float)x + 34, ty = (float)y + 2;
    if (j == JUG_CAJA) { tx = (float)x; ty = (float)y - 4; }
    if (j == JUG_COLUMPIO) { tx = (float)x; ty = (float)y - 14; }
    if (j == JUG_PELOTON) { ox = sinf(k * 0.9f) * 46.0f; tx = (float)x + ox + (cosf(k * 0.9f) > 0 ? -34.0f : 34.0f); ty = (float)y; }
    if (j == JUG_MOLINETE) { tx = (float)x + 44; ty = (float)y + 6; }
    float d = hypotf(c->x - tx, c->y - ty);
    if (d > (j == JUG_PELOTON ? 30.0f : 6.0f)) {
        c->tx = tx; c->ty = ty;
        limitar(&c->tx, &c->ty);
        *caminando = mover(c, j == JUG_PELOTON ? 3.0f : 4.0f);
        if (*caminando) *sy = -fabsf(sinf((float)s_ms / 110.0f)) * 4;
        if (j != JUG_PELOTON) return true;
    }
    ps->expr = EXP_FELIZ;
    ps->cola = 1;
    switch (j) {
    case JUG_PELUCHE: /* lo abraza */
        ps->espejo = true; ps->orejas = -1; ps->expr = EXP_CERRADOS;
        *sy = sinf(k * 3.0f) * 1.5f;
        if (s_ms % 1800 < TICK_MS) flotar(ICO_CORAZON, NULL, 0, (int)c->x - 20, cabeza_y(c) + 10, 0);
        break;
    case JUG_PELOTON: /* lo empuja de un lado a otro */
        poner_juguete_img(JUG_PELOTON, x + (int)ox, y);
        lv_image_set_pivot(s_jug_img[JUG_PELOTON], 24, 24);
        lv_image_set_rotation(s_jug_img[JUG_PELOTON], (int32_t)(ox * 20.0f));
        c->tx = tx; c->ty = ty; limitar(&c->tx, &c->ty);
        *caminando = mover(c, 3.2f);
        ps->espejo = cosf(k * 0.9f) < 0;
        ps->boca = true;
        *sy = -fabsf(sinf((float)s_ms / 120.0f)) * 5;
        break;
    case JUG_CAJA: /* adentro de la caja: se asoma */
        ps->expr = (s_ms / 2500) % 2 ? EXP_CURIOSO : EXP_FELIZ; ps->orejas = 1;
        *sy = 18 - fabsf(sinf(k * 1.4f)) * 22;
        break;
    case JUG_COLUMPIO: { /* se columpia */
        float a = sinf(k * 2.4f);
        lv_image_set_pivot(s_jug_img[JUG_COLUMPIO], 30, 4);
        lv_image_set_rotation(s_jug_img[JUG_COLUMPIO], (int32_t)(a * 160.0f));
        *sx = a * 20.0f;
        *sy = -fabsf(a) * 6.0f;
        ps->boca = true; ps->orejas = 1;
        if (s_ms % 2600 < TICK_MS) accion(UI_ACCION_SONAR, SND_COLUMPIO);
        break;
    }
    case JUG_TAMBOR: /* le da al tambor */
        ps->espejo = true; ps->boca = true;
        if (fmodf(k * 2.0f, 1.0f) < 0.25f) *sy = -8;
        if (s_ms % 800 < TICK_MS) flotar(ICO_NOTA, NULL, 0, x + (int)(azar01() * 30) - 15, y - 50, 0);
        break;
    case JUG_MOLINETE: /* mira cómo gira */
        ps->espejo = true; ps->expr = EXP_CURIOSO; ps->orejas = 1;
        if (s_ms % 3000 < 300) *sy = -4;
        break;
    default: break;
    }
    return true;
}

/* --- jardín: elegir qué plantar --- */
static int s_parcela_sel = -1;
static void semilla_cb(lv_event_t *e)
{
    int q = (int)(intptr_t)lv_event_get_user_data(e); /* 0..3 flores de colores, 4 girasol, 5 frutilla, 6 hierba */
    int i = s_parcela_sel;
    int cultivo = q < 4 ? CULTIVO_FLOR : q - 3, color = q < 4 ? q + 1 : 0;
    if (!s_pet || !pet_seeds(s_pet, (cultivo_t)cultivo)) { lv_obj_set_style_translate_x(lv_event_get_current_target(e), 6, 0); return; }
    cerrar_panel();
    if (i < 0 || i >= PET_PARCELAS_TOTAL) return;
    flotar(ICO_SEMILLA, NULL, 0, POS_PARCELA[i][0], POS_PARCELA[i][1] - 20, 0);
    accion(ACT_PLANTAR, i | cultivo << 4 | color << 8);
}

static void panel_semilla(int parcela)
{
    s_parcela_sel = parcela;
    abrir_tarjeta(PANEL_SEMILLA, 330, 256);
    if (!s_nino) { lv_obj_t *t = texto(s_tarjeta, "¿QUÉ PLANTAMOS?", C_TINTA, 3, 0); lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 14); }
    static const uint32_t TONO[4] = {0, 0xFFD45C, 0xB48CFF, 0xFF7A5A};
    static const art_icono_t OTRO[3] = {ICO_GIRASOL, ICO_FRUTILLA, ICO_HIERBA};
    for (int q = 0; q < 7; q++) {
        int cultivo = q < 4 ? CULTIVO_FLOR : q - 3, n = s_pet ? pet_seeds(s_pet, (cultivo_t)cultivo) : 0;
        bool conoce = s_pet && pet_seed_known(s_pet, (cultivo_t)cultivo);
        lv_obj_t *b = boton(s_tarjeta, 66, 70, semilla_cb, q);
        if (q < 4) lv_obj_align(b, LV_ALIGN_TOP_MID, (int)((q - 1.5f) * 74), 52);
        else lv_obj_align(b, LV_ALIGN_TOP_MID, (q - 5) * 74, 134);
        if (!conoce) { /* todavía no encuentra esas semillas: un misterio */
            lv_obj_set_style_bg_opa(b, LV_OPA_40, 0);
            lv_obj_t *ic = icono(b, ICO_PREGUNTA, 3);
            lv_obj_center(ic);
            continue;
        }
        lv_obj_t *ic = icono(b, q < 4 ? ICO_FLOR : OTRO[q - 4], 4);
        lv_obj_align(ic, LV_ALIGN_CENTER, 0, q == 0 || q >= 4 ? -6 : 0);
        if (q > 0 && q < 4) {
            lv_obj_set_style_image_recolor(ic, lv_color_hex(TONO[q]), 0);
            lv_obj_set_style_image_recolor_opa(ic, LV_OPA_40, 0);
        }
        if (!n) { lv_obj_set_style_opa(ic, LV_OPA_40, 0); lv_obj_set_style_bg_opa(b, LV_OPA_50, 0); }
        if (q == 0 || q >= 4) { /* cuántas semillas de ese tipo tiene */
            char c[8];
            snprintf(c, sizeof c, "%d", n);
            lv_obj_t *ct = texto(b, c, n ? 0xFFE27A : 0x9A96B0, 2, 0);
            lv_obj_align(ct, LV_ALIGN_BOTTOM_RIGHT, -6, -4);
        }
    }
}

/* --- modo pecera --- */
static bool s_pecera_auto, s_pecera_no_auto;
static lv_obj_t *s_pecera_bloqueo; /* tapa todo el mundo: en la pecera solo responden los botones de abajo */
static lv_obj_t *s_pecera_vol, *s_pecera_rit, *s_pecera_btn, *s_pecera_hora, *s_pecera_fecha, *s_pecera_al, *s_pecera_ih, *s_pecera_if, *s_pecera_ia;
static art_img_t s_pecera_hora_art, s_pecera_fecha_art, s_pecera_al_art;
/* (v101) el tiempo de la pecera se elige en la misma pecera: un botón con el tiempo («30'»; la ampolleta = no se apaga) y un anillo
 * que se va vaciando; al tocarlo pasa a la opción siguiente (es el mismo ajuste de Pantalla) y avisa cuánto queda */
static lv_obj_t *s_pecera_tmp, *s_pecera_tmp_img, *s_pecera_anillo, *s_pecera_aviso, *s_pecera_aviso_img;
static art_img_t s_pecera_tmp_art, s_pecera_aviso_art;
static int s_pecera_tmp_op = -1, s_pecera_anillo_v = -1;
static uint32_t s_pecera_aviso_hasta;
static int s_pecera_min = -1, s_pecera_prox = -2;

bool ui_aquarium(void) { return s_pecera; }

static void salir_pecera(void)
{
    if (!s_pecera) return;
    s_pecera = false;
    if (!s_pecera_auto || s_cargando) s_pecera_no_auto = s_cargando; /* lo cerraste a mano: no volver solo hasta desenchufar */
    s_pecera_auto = false;
    ocultar(s_pecera_bloqueo, true);
    ocultar(s_pecera_btn, true);
    ocultar(s_pecera_rit, true);
    ocultar(s_pecera_vol, true);
    ocultar(s_pecera_tmp, true); ocultar(s_pecera_anillo, true); ocultar(s_pecera_aviso, true);
    ocultar(s_pecera_hora, true);
    ocultar(s_pecera_fecha, true);
    ocultar(s_pecera_al, true);
    ocultar(s_bat, true);
    if (s_pet) ui_update(s_pet, s_ultimo_t);
}

static void pecera_cb(lv_event_t *e) { (void)e; salir_pecera(); }
/* (v101) la pecera cumplió su tiempo: sale, y si está cargando no vuelve sola hasta desenchufar */
void ui_pecera_vencida(void) { if (!s_pecera) return; salir_pecera(); s_pecera_no_auto = s_cargando; }
static void pecera_vol_cb(lv_event_t *e) { (void)e; panel_sonido(); } /* el volumen también se ajusta desde la pecera */
static void pecera_ritmo_cb(lv_event_t *e) { (void)e; panel_ritmo(); } /* el ritmo también se elige desde la pecera */

static uint32_t s_pecera_eligio_ms; /* (recién elegido aquí: vale lo elegido mientras main lo guarda) */
static int pecera_opcion(void)
{
    if (!s_disp_aj || (s_pecera_eligio_ms && s_ms - s_pecera_eligio_ms < 3000u)) return (int)(s_disp_vista.pecera % 4);
    return (int)(s_disp_aj->pecera % 4);
}
static void pecera_aviso_mostrar(int op)
{
    static const char *const T[4] = {"SE APAGA EN 10 MINUTOS", "SE APAGA EN 30 MINUTOS", "SE APAGA EN 1 HORA", "NO SE APAGA SOLA"};
    art_texto(&s_pecera_aviso_art, T[op & 3], C_BLANCO, 2, 0);
    cartel_poner(s_pecera_aviso, s_pecera_aviso_img, &s_pecera_aviso_art);
    lv_obj_align(s_pecera_aviso, LV_ALIGN_CENTER, 0, 40);
    ocultar(s_pecera_aviso, false);
    lv_obj_move_foreground(s_pecera_aviso);
    s_pecera_aviso_hasta = s_ms + 2500u;
}
static void pecera_tmp_poner(int op)
{
    static const char *const T[3] = {"10'", "30'", "1H"};
    if (op == s_pecera_tmp_op) return;
    s_pecera_tmp_op = op;
    s_pecera_anillo_v = -1;
    if (op == 3) poner_src(s_pecera_tmp_img, art_icono(ICO_BOMBILLA, 2)); /* no se apaga: la ampolleta */
    else {
        art_texto(&s_pecera_tmp_art, T[op], C_BLANCO, 2, 0); /* (101b) letra a escala 2 (antes 3): el botón es más chico */
        poner_src(s_pecera_tmp_img, &s_pecera_tmp_art.dsc);
    }
    lv_obj_center(s_pecera_tmp_img);
}
static void pecera_tmp_cb(lv_event_t *e)
{
    (void)e;
    const int op = (pecera_opcion() + 1) % 4;
    if (s_disp_aj) s_disp_vista = *s_disp_aj;
    s_disp_vista.pecera = (uint8_t)op;
    s_pecera_eligio_ms = s_ms | 1u;
    accion(UI_ACCION_PECERA_DUR, op); /* el mismo ajuste de Pantalla; el toque ya volvió a contar el tiempo desde cero */
    pecera_tmp_poner(op);
    pecera_aviso_mostrar(op);
}
/* el anillo: cuánto le queda a la pecera antes de apagarse (se mueve como mucho una vez por minuto) */
static void pecera_anillo_poner(void)
{
    const int op = pecera_opcion();
    pecera_tmp_poner(op);
    if (op == 3) { ocultar(s_pecera_anillo, true); return; }
    static const uint32_t MS[3] = {10u * 60000u, 30u * 60000u, 60u * 60000u};
    const uint32_t ina = lv_display_get_inactive_time(NULL);
    const uint32_t queda = ina >= MS[op] ? 0 : MS[op] - ina;
    const int v = (int)((queda + 59999u) / 60000u * 100u / (MS[op] / 60000u)); /* en minutos enteros: no se redibuja a cada rato */
    ocultar(s_pecera_anillo, false);
    if (v != s_pecera_anillo_v) { s_pecera_anillo_v = v; lv_arc_set_value(s_pecera_anillo, v > 100 ? 100 : v); }
}

static void entrar_pecera(bool solo)
{
    cerrar_panel();
    s_pecera = true;
    s_pecera_auto = solo;
    s_pecera_min = -1;
    s_pecera_prox = -2;
    s_pecera_tmp_op = -1;
    pecera_aviso_mostrar(pecera_opcion()); /* (v101) cuánto va a durar, sin tener que buscarlo */
    ocultar(s_pecera_bloqueo, false);
    lv_obj_move_foreground(s_pecera_bloqueo);
    ocultar(s_pecera_btn, false);
    lv_obj_move_foreground(s_pecera_btn);
    ocultar(s_pecera_rit, false);
    lv_obj_move_foreground(s_pecera_rit);
    ocultar(s_pecera_vol, false);
    lv_obj_move_foreground(s_pecera_vol);
}

static void crear_pecera(lv_obj_t *scr)
{
    s_pecera_bloqueo = lv_obj_create(scr);
    lv_obj_remove_style_all(s_pecera_bloqueo);
    lv_obj_set_size(s_pecera_bloqueo, W, W);
    lv_obj_set_pos(s_pecera_bloqueo, 0, 0);
    lv_obj_add_flag(s_pecera_bloqueo, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(s_pecera_bloqueo, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_remove_flag(s_pecera_bloqueo, LV_OBJ_FLAG_SCROLLABLE);
    ocultar(s_pecera_bloqueo, true);
    s_pecera_btn = lv_button_create(scr);
    lv_obj_set_size(s_pecera_btn, 72, 58);
    lv_obj_align(s_pecera_btn, LV_ALIGN_BOTTOM_MID, 0, -12);
    lv_obj_set_style_radius(s_pecera_btn, 14, 0);
    lv_obj_set_style_bg_color(s_pecera_btn, lv_color_hex(C_BOTON), 0);
    lv_obj_set_style_bg_opa(s_pecera_btn, LV_OPA_40, 0);
    lv_obj_set_style_shadow_width(s_pecera_btn, 0, 0);
    lv_obj_add_event_cb(s_pecera_btn, pecera_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *ic = icono(s_pecera_btn, ICO_PECERA, 4);
    lv_obj_center(ic);
    ocultar(s_pecera_btn, true);
    s_pecera_rit = lv_button_create(scr);
    lv_obj_set_size(s_pecera_rit, 72, 58);
    lv_obj_align(s_pecera_rit, LV_ALIGN_BOTTOM_MID, 84, -34);
    lv_obj_set_style_radius(s_pecera_rit, 14, 0);
    lv_obj_set_style_bg_color(s_pecera_rit, lv_color_hex(C_BOTON), 0);
    lv_obj_set_style_bg_opa(s_pecera_rit, LV_OPA_40, 0);
    lv_obj_set_style_shadow_width(s_pecera_rit, 0, 0);
    lv_obj_add_event_cb(s_pecera_rit, pecera_ritmo_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_center(icono(s_pecera_rit, ICO_RELOJ, 4));
    ocultar(s_pecera_rit, true);
    s_pecera_vol = lv_button_create(scr);
    lv_obj_set_size(s_pecera_vol, 72, 58);
    lv_obj_align(s_pecera_vol, LV_ALIGN_BOTTOM_MID, -84, -34);
    lv_obj_set_style_radius(s_pecera_vol, 14, 0);
    lv_obj_set_style_bg_color(s_pecera_vol, lv_color_hex(C_BOTON), 0);
    lv_obj_set_style_bg_opa(s_pecera_vol, LV_OPA_40, 0);
    lv_obj_set_style_shadow_width(s_pecera_vol, 0, 0);
    lv_obj_add_event_cb(s_pecera_vol, pecera_vol_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_center(icono(s_pecera_vol, ICO_PARLANTE, 4));
    ocultar(s_pecera_vol, true);
    s_pecera_anillo = lv_arc_create(scr); /* (v101) el anillo y el botón del tiempo, arriba a la derecha del botón del ritmo */
    lv_obj_set_size(s_pecera_anillo, 56, 56); /* (101b) más chico: antes 70 (el botón 56); el mismo centro */
    lv_obj_align(s_pecera_anillo, LV_ALIGN_BOTTOM_MID, 150, -103);
    lv_arc_set_rotation(s_pecera_anillo, 270);
    lv_arc_set_bg_angles(s_pecera_anillo, 0, 360);
    lv_arc_set_range(s_pecera_anillo, 0, 100);
    lv_arc_set_value(s_pecera_anillo, 100);
    lv_obj_remove_style(s_pecera_anillo, NULL, LV_PART_KNOB);
    lv_obj_remove_flag(s_pecera_anillo, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_width(s_pecera_anillo, 4, LV_PART_MAIN);
    lv_obj_set_style_arc_width(s_pecera_anillo, 4, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(s_pecera_anillo, lv_color_hex(0x15121F), LV_PART_MAIN);
    lv_obj_set_style_arc_opa(s_pecera_anillo, LV_OPA_40, LV_PART_MAIN);
    lv_obj_set_style_arc_color(s_pecera_anillo, lv_color_hex(C_BLANCO), LV_PART_INDICATOR);
    lv_obj_set_style_arc_opa(s_pecera_anillo, LV_OPA_70, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(s_pecera_anillo, true, LV_PART_INDICATOR);
    ocultar(s_pecera_anillo, true);
    s_pecera_tmp = lv_button_create(scr);
    lv_obj_set_size(s_pecera_tmp, 44, 44);
    lv_obj_align(s_pecera_tmp, LV_ALIGN_BOTTOM_MID, 150, -109);
    lv_obj_set_style_radius(s_pecera_tmp, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(s_pecera_tmp, lv_color_hex(C_BOTON), 0);
    lv_obj_set_style_bg_opa(s_pecera_tmp, LV_OPA_40, 0);
    lv_obj_set_style_shadow_width(s_pecera_tmp, 0, 0);
    lv_obj_set_ext_click_area(s_pecera_tmp, 10);
    lv_obj_add_event_cb(s_pecera_tmp, pecera_tmp_cb, LV_EVENT_CLICKED, NULL);
    s_pecera_tmp_img = lv_image_create(s_pecera_tmp);
    ocultar(s_pecera_tmp, true);
    s_pecera_aviso = cartel_de(scr, &s_pecera_aviso_img, 0x15121F, LV_OPA_60);
    ocultar(s_pecera_aviso, true);
    s_pecera_hora = cartel_de(scr, &s_pecera_ih, 0x15121F, LV_OPA_40); /* que se lea sobre el cielo */
    s_pecera_fecha = cartel_de(scr, &s_pecera_if, 0x15121F, LV_OPA_40);
    s_pecera_al = cartel_de(scr, &s_pecera_ia, 0x15121F, LV_OPA_40);
    ocultar(s_pecera_hora, true);
    ocultar(s_pecera_fecha, true);
    ocultar(s_pecera_al, true);
}

static void actualizar_pecera(const pet_t *p, uint32_t utc)
{
    /* sola: cargando y sin tocarlo un minuto */
    if (!s_cargando) s_pecera_no_auto = false;
    if (s_pecera && s_pecera_auto && !s_cargando) { salir_pecera(); return; }
    if (!s_pecera && s_cargando && !s_pecera_no_auto && s_panel == PANEL_NINGUNO && s_juego == JG_NADA && p->edad_dueno &&
        !s_bienv_activa && lv_display_get_inactive_time(NULL) > 60000)
        entrar_pecera(true);
    if (!s_pecera) return;
    for (int i = 0; i < 4; i++) ocultar(s_dock[i], true);
    for (int i = 0; i < 4; i++) { ocultar(s_flecha[i], true); ocultar(s_flecha_zona[i], true); }
    ocultar(s_bat, true);
    if (s_panel != PANEL_NINGUNO) { /* un panel abierto (el ritmo): que los carteles y botones de la pecera no lo tapen */
        ocultar(s_pecera_hora, true); ocultar(s_pecera_fecha, true); ocultar(s_pecera_al, true);
        ocultar(s_pecera_btn, true); ocultar(s_pecera_rit, true); ocultar(s_pecera_vol, true);
        ocultar(s_pecera_tmp, true); ocultar(s_pecera_anillo, true); ocultar(s_pecera_aviso, true);
        ocultar(s_pecera_bloqueo, true);
        return;
    }
    ocultar(s_pecera_bloqueo, false);
    ocultar(s_pecera_btn, false); ocultar(s_pecera_rit, false); ocultar(s_pecera_vol, false);
    ocultar(s_pecera_tmp, false);
    pecera_anillo_poner();
    if (s_pecera_aviso_hasta && (int32_t)(s_ms - s_pecera_aviso_hasta) > 0) { s_pecera_aviso_hasta = 0; ocultar(s_pecera_aviso, true); }
    /* tu hora, grande arriba (como un reloj de escritorio), con la fecha y la próxima alarma */
    int64_t l = (int64_t)utc + p->zona_horaria;
    int min = (int)(((l % 86400) + 86400) % 86400 / 60);
    uint32_t prox = 0;
    for (int i = 0; i < PET_ALARMAS; i++) {
        uint32_t x = pet_alarm_next(p, i, utc);
        if (x && x - utc < 24u * 3600u && (!prox || x < prox)) prox = x;
    }
    int prox_min = prox ? (int)((((int64_t)prox + p->zona_horaria) % 86400 + 86400) % 86400 / 60) : -1;
    if (min != s_pecera_min || prox_min != s_pecera_prox) {
        s_pecera_min = min;
        s_pecera_prox = prox_min;
        char t[48];
        snprintf(t, sizeof t, "%02d:%02d", min / 60, min % 60);
        art_texto(&s_pecera_hora_art, t, C_BLANCO, 7, 0);
        cartel_poner(s_pecera_hora, s_pecera_ih, &s_pecera_hora_art);
        lv_obj_align(s_pecera_hora, LV_ALIGN_TOP_MID, 0, 64);
        static const char *const MES[12] = {"ENERO", "FEBRERO", "MARZO", "ABRIL", "MAYO", "JUNIO", "JULIO", "AGOSTO", "SEPTIEMBRE", "OCTUBRE", "NOVIEMBRE", "DICIEMBRE"};
        int64_t dia = ui_dia_local(utc);
        int a_, m_, d_;
        pet_local_date(p, utc, &a_, &m_, &d_);
        snprintf(t, sizeof t, "%s %d DE %s", DIA_NOMBRE[ui_dia_semana(dia)], d_, MES[(m_ - 1) % 12]);
        art_texto(&s_pecera_fecha_art, t, C_BLANCO, 2, 0);
        cartel_poner(s_pecera_fecha, s_pecera_if, &s_pecera_fecha_art);
        lv_obj_align(s_pecera_fecha, LV_ALIGN_TOP_MID, 0, 136);
        if (prox) {
            snprintf(t, sizeof t, "ALARMA %02d:%02d", prox_min / 60, prox_min % 60);
            art_texto(&s_pecera_al_art, t, 0xFFE27A, 2, 0);
            cartel_poner(s_pecera_al, s_pecera_ia, &s_pecera_al_art);
            lv_obj_align(s_pecera_al, LV_ALIGN_TOP_MID, 0, 168);
        }
    }
    ocultar(s_pecera_hora, false);
    ocultar(s_pecera_fecha, s_nino);
    ocultar(s_pecera_al, !prox || s_nino);
    lv_obj_move_foreground(s_pecera_bloqueo); /* por encima de todo el mundo, por debajo de los carteles y botones */
    lv_obj_move_foreground(s_pecera_hora);
    lv_obj_move_foreground(s_pecera_fecha);
    lv_obj_move_foreground(s_pecera_al);
    lv_obj_move_foreground(s_pecera_btn);
    lv_obj_move_foreground(s_pecera_rit);
    lv_obj_move_foreground(s_pecera_vol);
    lv_obj_move_foreground(s_pecera_anillo);
    lv_obj_move_foreground(s_pecera_tmp);
    if (s_pecera_aviso_hasta) lv_obj_move_foreground(s_pecera_aviso);
}

/* --- fotos: se guardan en el reloj y pasan al computador con el editor --- */
static const ui_fotos_t *s_fotos;
void ui_set_photo_store(const ui_fotos_t *f) { s_fotos = f; }

#define FOTO_MINI 116 /* un cuarto de la pantalla */
#define FOTO_MEDIA 233
static uint16_t *s_mini_px[4];
static lv_image_dsc_t s_mini_dsc[4];
static uint16_t *s_media_px;
static lv_image_dsc_t s_media_dsc;
static uint16_t s_foto_ids[64];
static int s_foto_n, s_foto_vista = -1, s_foto_borrar;

static bool cargar_foto(int id, uint16_t *px, int paso)
{
    if (!s_fotos) return false;
    int n = s_fotos->tamano(id);
    if (n <= 0) return false;
    uint8_t *q = lv_malloc((size_t)n);
    if (!q) return false;
    bool ok = s_fotos->leer(id, q, n) == n && foto_qoi_decode_565(q, n, px, paso);
    lv_free(q);
    return ok;
}

static void dsc_565(lv_image_dsc_t *d, uint16_t *px, int lado)
{
    memset(d, 0, sizeof *d);
    d->header.magic = LV_IMAGE_HEADER_MAGIC;
    d->header.cf = LV_COLOR_FORMAT_RGB565;
    d->header.w = (uint32_t)lado;
    d->header.h = (uint32_t)lado;
    d->header.stride = (uint32_t)lado * 2;
    d->data = (const uint8_t *)px;
    d->data_size = (uint32_t)(lado * lado * 2);
}

static void tomar_foto(void)
{
    cerrar_panel();
    if (!s_fotos) { globo(ICO_CAMARA, "AQUÍ NO SE PUEDEN GUARDAR FOTOS", 3000); return; }
#if LV_USE_SNAPSHOT
    /* sin botones ni avisos: solo el mundo y ella */
    bool dock[4], bat = lv_obj_has_flag(s_bat, LV_OBJ_FLAG_HIDDEN), glo = lv_obj_has_flag(s_globo, LV_OBJ_FLAG_HIDDEN);
    for (int i = 0; i < 4; i++) { dock[i] = lv_obj_has_flag(s_dock[i], LV_OBJ_FLAG_HIDDEN); ocultar(s_dock[i], true); }
    ocultar(s_bat, true);
    ocultar(s_globo, true);
    ocultar(s_pecera_btn, true);
    lv_draw_buf_t *db = lv_snapshot_take(lv_screen_active(), LV_COLOR_FORMAT_RGB565);
    for (int i = 0; i < 4; i++) ocultar(s_dock[i], dock[i]);
    ocultar(s_bat, true); (void)bat;
    ocultar(s_globo, glo);
    ocultar(s_pecera_btn, !s_pecera);
    if (!db) { globo(ICO_CAMARA, "NO SE PUDO SACAR LA FOTO", 3000); return; }
    int w = (int)db->header.w, h = (int)db->header.h, cap = foto_qoi_max(w, h);
    uint8_t *q = lv_malloc((size_t)cap);
    int n = q ? foto_qoi_encode_565((const uint16_t *)db->data, w, h, (int)db->header.stride / 2, q, cap) : -1;
    lv_draw_buf_destroy(db);
    if (n > 0 && s_pet && s_pet->mis_fotos_n >= PET_FOTOS_PERFIL) s_fotos->borrar(s_pet->mis_fotos[0]); /* ya tiene su máximo: se va la más vieja de ella */
    int id = n > 0 ? s_fotos->guardar(q, n) : -1;
    lv_free(q);
    if (id < 0) { globo(ICO_CAMARA, "NO QUEDA LUGAR PARA MÁS FOTOS", 3500); return; }
    accion(UI_ACCION_FOTO, id);
    flash_foto();
    accion(UI_ACCION_SONAR, SND_FOTO);
    flotar(ICO_CAMARA, NULL, 0, CX, 120, 0);
#else
    globo(ICO_CAMARA, "ESTE ORBE NO PUEDE SACAR FOTOS", 3000);
#endif
}

static void foto_cb(lv_event_t *e)
{
    int q = (int)(intptr_t)lv_event_get_user_data(e);
    if (q == 200) { tomar_foto(); return; }
    if (q == 201 && s_foto_pag > 0) s_foto_pag--;
    if (q == 202) s_foto_pag++;
    if (q >= 0 && q < 4) { s_foto_vista = s_foto_pag * 4 + q; s_foto_borrar = 0; panel_foto(); return; }
    panel_diario();
}

/* la pestaña de fotos del libro */
static void fotos_tab(void)
{
    static uint16_t todas[128];
    int nt = s_fotos ? s_fotos->listar(todas, 128) : 0;
    int maxid = 0;
    for (int i = 0; i < nt; i++) if (todas[i] > maxid) maxid = todas[i];
    if (s_pet && !s_pet->fotos_legado_ok) accion(UI_ACCION_FOTO, 0x10000 | (maxid & 0xFFFF)); /* las de antes las ve ella también */
    s_foto_n = 0;
    for (int i = 0; i < nt && s_foto_n < 64; i++) /* solo las suyas (y las de antes, que eran de todas) */
        if (!s_pet || pet_photo_mine(s_pet, todas[i]) || !s_pet->fotos_legado_ok) s_foto_ids[s_foto_n++] = todas[i];
    if (s_foto_pag * 4 >= s_foto_n && s_foto_pag > 0) s_foto_pag = (s_foto_n - 1) / 4;
    for (int k = 0; k < 4; k++) {
        int i = s_foto_pag * 4 + k;
        if (i >= s_foto_n) break;
        if (!s_mini_px[k]) s_mini_px[k] = lv_malloc(FOTO_MINI * FOTO_MINI * 2);
        if (!s_mini_px[k] || !cargar_foto(s_foto_ids[i], s_mini_px[k], 4)) continue;
        dsc_565(&s_mini_dsc[k], s_mini_px[k], FOTO_MINI);
        lv_obj_t *b = boton(s_tarjeta, FOTO_MINI + 8, FOTO_MINI + 8, foto_cb, k);
        lv_obj_set_pos(b, k % 2 ? 182 : 46, k / 2 ? 182 : 58);
        lv_obj_set_style_radius(b, 8, 0);
        lv_obj_t *im = lv_image_create(b);
        lv_image_set_src(im, &s_mini_dsc[k]);
        lv_obj_center(im);
    }
    if (!s_foto_n) { lv_obj_t *t = icono(s_tarjeta, ICO_CAMARA, 6); lv_obj_align(t, LV_ALIGN_CENTER, 0, 0); }
    lv_obj_t *cam = boton(s_tarjeta, 110, 46, foto_cb, 200);
    lv_obj_align(cam, LV_ALIGN_BOTTOM_MID, 0, -28);
    lv_obj_set_style_bg_color(cam, lv_color_hex(C_VERDE), 0);
    lv_obj_t *ci = icono(cam, ICO_CAMARA, 3);
    lv_obj_center(ci);
    if (s_foto_pag > 0) {
        lv_obj_t *b = boton(s_tarjeta, 52, 46, foto_cb, 201);
        lv_obj_align(b, LV_ALIGN_BOTTOM_MID, -86, -12);
        lv_obj_t *fl = icono(b, ICO_FLECHA, 2); lv_image_set_pivot(fl, 12, 12); lv_image_set_rotation(fl, 1800); lv_obj_center(fl);
    }
    if ((s_foto_pag + 1) * 4 < s_foto_n) {
        lv_obj_t *b = boton(s_tarjeta, 52, 46, foto_cb, 202);
        lv_obj_align(b, LV_ALIGN_BOTTOM_MID, 86, -12);
        lv_obj_t *fl = icono(b, ICO_FLECHA, 2); lv_obj_center(fl);
    }
}

static void foto_vista_cb(lv_event_t *e)
{
    int q = (int)(intptr_t)lv_event_get_user_data(e);
    if (q == 1) { /* borrar: tocar dos veces */
        if (!s_foto_borrar) { s_foto_borrar = 1; panel_foto(); return; }
        if (s_fotos && s_foto_vista >= 0 && s_foto_vista < s_foto_n) { s_fotos->borrar(s_foto_ids[s_foto_vista]); accion(UI_ACCION_FOTO_BORRADA, s_foto_ids[s_foto_vista]); }
    }
    s_foto_vista = -1;
    s_diario_tab = 2;
    panel_diario();
}

static void panel_foto(void)
{
    abrir_tarjeta(PANEL_FOTO, 300, 340);
    if (s_foto_vista < 0 || s_foto_vista >= s_foto_n) return;
    if (!s_media_px) s_media_px = lv_malloc(FOTO_MEDIA * FOTO_MEDIA * 2);
    if (s_media_px && cargar_foto(s_foto_ids[s_foto_vista], s_media_px, 2)) {
        dsc_565(&s_media_dsc, s_media_px, FOTO_MEDIA);
        lv_obj_t *im = lv_image_create(s_tarjeta);
        lv_image_set_src(im, &s_media_dsc);
        lv_obj_align(im, LV_ALIGN_TOP_MID, 0, 16);
    }
    volver(foto_vista_cb, 0);
    if (s_suave) return; /* los niños no borran fotos */
    lv_obj_t *bb = boton(s_tarjeta, 56, 46, foto_vista_cb, 1); /* borrar: a la derecha de la X */
    if (s_foto_borrar) mantener(bb, foto_vista_cb, 1);
    abajo_derecha(bb);
    lv_obj_set_style_bg_color(bb, lv_color_hex(s_foto_borrar ? C_ROJO : 0x7A4458), 0);
    lv_obj_t *bi = icono(bb, s_foto_borrar ? ICO_SI : ICO_NO, 3);
    lv_obj_center(bi);
}

/* --- de paso: una mariposa o un pajarito que cruza el cielo cada tanto (solo se ve; no la distrae) --- */
static lv_obj_t *s_pasa;
static int s_pasa_tipo = -1, s_pasa_dir;
static uint32_t s_pasa_desde, s_pasa_prox = 20000;
static float s_pasa_y;

static void pajaro_llama(void)
{
    if (!s_pasa || s_interior) return;
    s_pasa_tipo = 1;
    s_pasa_dir = azar() & 1 ? 1 : -1;
    s_pasa_y = 50.0f + azar01() * 60.0f;
    s_pasa_desde = s_ms;
    s_pasa_prox = s_ms + 35000 + azar() % 50000;
    ocultar(s_pasa, false);
    s_pajaro_t = s_ms;
}
static void animar_pasajero(void)
{
    const pet_t *p = s_pet;
    if (!p || !s_pasa) return;
    if (s_pasa_tipo < 0) {
        if ((int32_t)(s_ms - s_pasa_prox) < 0) return;
        s_pasa_prox = s_ms + 35000 + azar() % 50000; /* entre 35 s y 85 s */
        int64_t l = (int64_t)s_ultimo_t + p->zona_horaria;
        int h = (int)(((l % 86400) + 86400) % 86400 / 3600);
        clima_t c = pet_weather(p);
        if (h < 7 || h >= 20 || s_interior || s_escena == ESC_CUEVA || p->etapa == STAGE_HUEVO ||
            c == CLIMA_LLUVIA || c == CLIMA_TORMENTA || c == CLIMA_NIEVE) return;
        bool invierno = pet_season(p, s_ultimo_t) == ESTACION_INVIERNO;
        s_pasa_tipo = invierno || (azar() % 3 == 0) ? 1 : 0; /* 0 mariposa, 1 pajarito */
        s_pasa_dir = azar() & 1 ? 1 : -1;
        s_pasa_y = s_pasa_tipo ? 50.0f + azar01() * 60.0f : 120.0f + azar01() * 120.0f;
        s_pasa_desde = s_ms;
        ocultar(s_pasa, false);
    }
    if (s_pasa_tipo == 1) s_pajaro_t = s_ms; /* mientras vuela, se ve (y se oye) */
    float k = (float)(s_ms - s_pasa_desde) / (s_pasa_tipo ? 5000.0f : 9000.0f);
    if (k >= 1.0f) { s_pasa_tipo = -1; ocultar(s_pasa, true); return; }
    float x = s_pasa_dir > 0 ? -40.0f + k * (W + 80.0f) : W + 40.0f - k * (W + 80.0f);
    float y = s_pasa_y + sinf(k * (s_pasa_tipo ? 6.0f : 14.0f)) * (s_pasa_tipo ? 8.0f : 22.0f);
    bool aleteo = (s_ms / (s_pasa_tipo ? 160 : 220)) % 2;
    art_icono_t ico = s_pasa_tipo ? (aleteo ? ICO_PAJARO_B : ICO_PAJARO) : (aleteo ? ICO_MARIPOSA_B : ICO_MARIPOSA);
    lv_image_set_src(s_pasa, s_pasa_dir < 0 ? art_icono_espejo(ico, 3) : art_icono(ico, 3));
    lv_obj_set_pos(s_pasa, (int)x - 18, (int)y - 18);
}

/* ======================================================================
 * El viaje de despedida: se va a explorar el mundo (avión, barco, tren o globo)
 * ==================================================================== */
static lv_obj_t *s_via, *s_via_fondo, *s_via_veh, *s_via_cri, *s_via_t1, *s_via_t2, *s_via_c1, *s_via_c2;
static art_img_t s_via_fondo_art, s_via_veh_art[2], s_via_cri_art[2], s_via_t1_art, s_via_t2_art;
static bool s_via_activo;
static uint32_t s_via_desde;
static int s_via_tipo, s_via_sonado, s_via_fv = -1, s_via_fc = -1;
#define VIAJE_MS 17800
#define VIA_SALE 9800 /* cuando el vehículo empieza a irse */
#define VIA_N_EST 16
#define VIA_N_NUBE 5
#define VIA_N_COR 14
static lv_obj_t *s_via_est[VIA_N_EST], *s_via_nube[VIA_N_NUBE], *s_via_cor[VIA_N_COR], *s_via_fugaz, *s_via_fin, *s_via_fin_cor, *s_via_negro, *s_via_t3;
static art_img_t s_via_t3_art, s_via_t4_art;
static lv_obj_t *s_via_t4;
static uint32_t s_via_cor_t0[VIA_N_COR];
static float s_via_cor_dx[VIA_N_COR];
static int s_via_linea, s_via_dias, s_via_cor_n;
static char s_via_nombre[PET_NOMBRE_MAX + 2]; /* (el nombre cabe: PET_NOMBRE_MAX) */

static void viaje_fin(void)
{
    if (!s_via_activo) return;
    s_via_activo = false;
    ocultar(s_via, true);
    lv_image_set_src(s_via_fondo, NULL);
    lv_image_set_src(s_via_veh, NULL);
    lv_image_set_src(s_via_cri, NULL);
    art_img_free(&s_via_fondo_art); /* el paisaje pesa: se libera al terminar */
    for (int i = 0; i < 2; i++) { art_img_free(&s_via_veh_art[i]); art_img_free(&s_via_cri_art[i]); }
}

static void viaje_cb(lv_event_t *e)
{
    (void)e;
    if (s_ms - s_via_desde > 3000) viaje_fin(); /* tocar salta el final */
}

static void crear_viaje(lv_obj_t *scr)
{
    (void)scr;
    s_via = caja(lv_layer_top()); /* arriba de todo (globos, flotantes, paneles) */
    lv_obj_set_size(s_via, W, W);
    lv_obj_add_flag(s_via, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_via, viaje_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_set_style_bg_color(s_via, lv_color_hex(0x7EC8F2), 0);
    lv_obj_set_style_bg_opa(s_via, LV_OPA_COVER, 0);
    s_via_fondo = lv_image_create(s_via);
    for (int i = 0; i < VIA_N_EST; i++) { /* estrellas que titilan (o polen de luz en el amanecer) */
        s_via_est[i] = caja(s_via);
        lv_obj_set_size(s_via_est[i], 3, 3);
        lv_obj_set_style_radius(s_via_est[i], 1, 0);
        lv_obj_set_style_bg_color(s_via_est[i], lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_bg_opa(s_via_est[i], LV_OPA_COVER, 0);
        lv_obj_set_style_shadow_width(s_via_est[i], 6, 0);
        lv_obj_set_style_shadow_color(s_via_est[i], lv_color_hex(0xBFD8FF), 0);
        lv_obj_remove_flag(s_via_est[i], LV_OBJ_FLAG_CLICKABLE);
    }
    s_via_fugaz = caja(s_via); /* una estrella fugaz que cruza de vez en cuando */
    lv_obj_set_size(s_via_fugaz, 26, 3);
    lv_obj_set_style_radius(s_via_fugaz, 2, 0);
    lv_obj_set_style_bg_color(s_via_fugaz, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_grad_color(s_via_fugaz, lv_color_hex(0xFFE27A), 0);
    lv_obj_set_style_bg_grad_dir(s_via_fugaz, LV_GRAD_DIR_HOR, 0);
    lv_obj_set_style_bg_opa(s_via_fugaz, LV_OPA_COVER, 0);
    lv_obj_set_style_shadow_width(s_via_fugaz, 10, 0);
    lv_obj_set_style_shadow_color(s_via_fugaz, lv_color_hex(0xFFE27A), 0);
    lv_obj_set_style_transform_rotation(s_via_fugaz, 280, 0);
    lv_obj_remove_flag(s_via_fugaz, LV_OBJ_FLAG_CLICKABLE);
    for (int i = 0; i < VIA_N_NUBE; i++) { /* nubes que pasan a distinta velocidad */
        lv_obj_t *n = caja(s_via);
        lv_obj_set_size(n, 96, 36);
        lv_obj_remove_flag(n, LV_OBJ_FLAG_CLICKABLE);
        static const int B[3][4] = {{0, 12, 96, 24}, {16, 2, 42, 28}, {50, 8, 34, 22}};
        for (int j = 0; j < 3; j++) {
            lv_obj_t *c = caja(n);
            lv_obj_set_size(c, B[j][2], B[j][3]);
            lv_obj_set_pos(c, B[j][0], B[j][1]);
            lv_obj_set_style_radius(c, LV_RADIUS_CIRCLE, 0);
            lv_obj_set_style_bg_color(c, lv_color_hex(0xFFFFFF), 0);
            lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
            lv_obj_remove_flag(c, LV_OBJ_FLAG_CLICKABLE);
        }
        s_via_nube[i] = n;
    }
    s_via_cri = lv_image_create(s_via);
    s_via_veh = lv_image_create(s_via);
    for (int i = 0; i < VIA_N_COR; i++) { /* corazones que deja de recuerdo en el aire */
        s_via_cor[i] = lv_image_create(s_via);
        lv_image_set_src(s_via_cor[i], art_icono(ICO_CORAZON, 3));
        lv_obj_add_flag(s_via_cor[i], LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(s_via_cor[i], LV_OBJ_FLAG_CLICKABLE);
    }
    s_via_c1 = cartel(s_via, &s_via_t1);
    s_via_c2 = cartel(s_via, &s_via_t2);
    s_via_negro = caja(s_via); /* el cielo se apaga antes del final */
    lv_obj_set_size(s_via_negro, W, W);
    lv_obj_set_style_bg_color(s_via_negro, lv_color_hex(0x0A0820), 0);
    lv_obj_set_style_bg_opa(s_via_negro, LV_OPA_COVER, 0);
    lv_obj_remove_flag(s_via_negro, LV_OBJ_FLAG_CLICKABLE);
    s_via_fin_cor = lv_image_create(s_via); /* el final: un corazón grande y "hasta pronto" */
    lv_image_set_src(s_via_fin_cor, art_icono(ICO_CORAZON, 9));
    s_via_fin = cartel_de(s_via, &s_via_t3, 0xFFF6DC, LV_OPA_90);
    s_via_t4 = lv_image_create(s_via);
    ocultar(s_via, true);
}

static int s_via_forzado = -1;
static int elegir_vehiculo(void)
{
    if (s_via_forzado < 0) s_via_forzado = (int)(azar() % VIAJE_COUNT);
    return s_via_forzado % VIAJE_COUNT;
}
static void viaje_empezar(const pet_t *m)
{
    viaje_fin();
    cerrar_panel();
    s_via_tipo = s_via_forzado >= 0 ? s_via_forzado % VIAJE_COUNT : (int)(azar() % VIAJE_COUNT);
    s_via_forzado = -1;
    s_via_activo = true;
    s_via_desde = s_ms;
    s_via_sonado = 0;
    s_via_fv = s_via_fc = -1;
    art_viaje_fondo(&s_via_fondo_art, (art_viaje_t)s_via_tipo);
    poner_src(s_via_fondo, &s_via_fondo_art.dsc);
    lv_obj_set_pos(s_via_fondo, -1, -1);
    for (int f = 0; f < 2; f++) {
        art_vehiculo(&s_via_veh_art[f], (art_viaje_t)s_via_tipo, f, 4);
        if (m->etapa == STAGE_HUEVO) { art_huevo(&s_via_cri_art[f], pet_look(m), 3, 0, f ? 1 : -1); continue; }
        art_pose_t po = {pet_look(m), m->etapa, m->etapa == STAGE_ADULTA ? m->forma : 0, 4, EXP_FELIZ, f == 1, f, false, false, 1, 1, 1, {0, 0, 0}, 0, m->guardiana};
        for (int r = 0; r < RANURA_COUNT; r++) po.ropa[r] = pet_worn_raw(m, r);
        art_criatura_pose(&s_via_cri_art[f], &po);
    }
    char t[96];
    snprintf(t, sizeof t, "¡%s SE VA DE VIAJE A EXPLORAR EL MUNDO MÁGICO!", m->nombre[0] ? m->nombre : "TU MASCOTA");
    art_texto(&s_via_t1_art, t, C_TINTA, 2, 280);
    cartel_poner(s_via_c1, s_via_t1, &s_via_t1_art);
    lv_obj_align(s_via_c1, LV_ALIGN_TOP_MID, 0, 58);
    snprintf(s_via_nombre, sizeof s_via_nombre, "%.*s", (int)sizeof s_via_nombre - 1, m->nombre[0] ? m->nombre : "TU MASCOTA");
    s_via_dias = (int)pet_age_days(m, s_ultimo_t);
    s_via_linea = -1;
    s_via_cor_n = 0;
    for (int i = 0; i < VIA_N_COR; i++) { s_via_cor_t0[i] = 0; ocultar(s_via_cor[i], true); }
    art_texto(&s_via_t3_art, "¡HASTA PRONTO!", C_TINTA, 4, 0);
    cartel_poner(s_via_fin, s_via_t3, &s_via_t3_art);
    lv_obj_align(s_via_fin, LV_ALIGN_CENTER, 0, 48);
    snprintf(t, sizeof t, "%s TE ESCRIBIRÁ DESDE LEJOS", s_via_nombre);
    art_texto(&s_via_t4_art, t, 0xFFE9B0, 2, 280);
    poner_src(s_via_t4, &s_via_t4_art.dsc);
    lv_obj_align(s_via_t4, LV_ALIGN_CENTER, 0, 112);
    ocultar(s_via_fin, true); ocultar(s_via_t4, true); ocultar(s_via_fin_cor, true); ocultar(s_via_negro, false);
    lv_obj_set_style_opa(s_via_negro, LV_OPA_TRANSP, 0);
    lv_obj_set_style_opa(s_via_c1, LV_OPA_TRANSP, 0);
    lv_obj_set_style_opa(s_via_c2, LV_OPA_TRANSP, 0);
    ocultar(s_via_c1, s_nino);
    ocultar(s_via_c2, true);
    const bool noche = s_via_tipo != VIAJE_GLOBO; /* el amanecer no lleva estrellas, lleva polen de luz */
    for (int i = 0; i < VIA_N_EST; i++) {
        lv_obj_set_style_bg_color(s_via_est[i], lv_color_hex(noche ? (i & 1 ? 0xFFF2A0 : 0xFFFFFF) : 0xFFF6C0), 0);
        lv_obj_set_style_shadow_color(s_via_est[i], lv_color_hex(noche ? 0xBFD8FF : 0xFFD98A), 0);
    }
    for (int i = 0; i < VIA_N_NUBE; i++) {
        lv_obj_t *n = s_via_nube[i];
        for (uint32_t j = 0; j < 3; j++) {
            lv_obj_set_style_bg_color(lv_obj_get_child(n, (int32_t)j), lv_color_hex(noche ? 0xC8C0F0 : 0xFFFFFF), 0);
            lv_obj_set_style_bg_opa(lv_obj_get_child(n, (int32_t)j), noche ? 70 : 150, 0);
        }
    }
    lv_obj_set_pos(s_via_veh, -400, 0);
    lv_obj_set_pos(s_via_cri, -400, 0);
    ocultar(s_via, false);
    lv_obj_move_foreground(s_via);
}

void ui_demo_trip(const pet_t *m, int tipo) { s_via_forzado = tipo; viaje_empezar(m); }
void ui_demo_sky(const pet_t *m) { cielo_empezar(m); }

static float suave(float k)
{
    k = k < 0 ? 0 : (k > 1 ? 1 : k);
    return k * k * (3 - 2 * k);
}

static void via_corazon(int x, int y)
{
    if (x < 30 || x > W - 30 || y < 30) return; /* fuera de la pantalla no vale la pena */
    const int i = s_via_cor_n++ % VIA_N_COR;
    s_via_cor_t0[i] = s_ms;
    s_via_cor_dx[i] = (azar01() - 0.5f) * 40.0f;
    lv_obj_set_pos(s_via_cor[i], x - 18, y);
    ocultar(s_via_cor[i], false);
}

static void animar_viaje(void)
{
    if (!s_via_activo) return;
    const uint32_t t = s_ms - s_via_desde;
    const float vw = (float)s_via_veh_art[0].dsc.header.w, vh = (float)s_via_veh_art[0].dsc.header.h;
    const float centro = CX - vw / 2.0f;
    float x, y;
    int fr;
    const float s2 = t > VIA_SALE ? (float)(t - VIA_SALE) : 0; /* ms desde que se va */
    switch (s_via_tipo) {
    case VIAJE_AVION: { /* entra, se queda saludando y, al despedirse, se va subiendo */
        float k2 = s2 / 3800.0f;
        x = -vw - 20 + (centro + vw + 20) * suave((float)t / 3000.0f) + k2 * k2 * (W + 60);
        y = 170 + sinf((float)t / 300.0f) * 6 - k2 * k2 * 150;
        fr = (int)(t / 70) % 2;
        break;
    }
    case VIAJE_BARCO: { /* zarpa despacio, meciéndose */
        float k2 = s2 / 4200.0f;
        x = -vw - 10 + (centro + vw + 10) * suave((float)t / 3600.0f) + k2 * k2 * (W + 40);
        y = ART_VIAJE_AGUA_Y + 40 - vh + sinf((float)t / 420.0f) * 3;
        fr = (int)(t / 450) % 2;
        break;
    }
    case VIAJE_TREN: { /* llega a la estación, toca el silbato y parte */
        float k2 = s2 / 3600.0f;
        x = -vw - 10 + (centro + vw + 10) * suave((float)t / 3000.0f) + k2 * k2 * (W + 40);
        y = ART_VIAJE_RIEL_Y - vh * 0.945f;
        bool anda = t < 3000 || t > VIA_SALE;
        fr = anda ? (int)(t / 110) % 2 : (int)(t / 500) % 2;
        break;
    }
    default: { /* el globo sube, se mece y se va hacia arriba */
        float k2 = s2 / 3800.0f;
        x = centro + sinf((float)t / 700.0f) * 14;
        float arriba = CX - vh / 2.0f - 20;
        y = W + 10 - (W + 10 - arriba) * suave((float)t / 4000.0f) - k2 * k2 * (arriba + vh + 20);
        fr = (t < 1800 || (t > VIA_SALE - 400 && t < VIA_SALE + 1400)) ? (int)(t / 110) % 2 : 0;
        break;
    }
    }
    static const sound_t SON[VIAJE_COUNT] = {SND_AVION, SND_BOCINA, SND_SILBATO, SND_GLOBO};
    static const uint32_t CUANDO[VIAJE_COUNT] = {200, 2800, 2500, 300};
    if (s_via_sonado == 0 && t >= CUANDO[s_via_tipo]) { s_via_sonado = 1; accion(UI_ACCION_SONAR, SON[s_via_tipo]); }
    if (s_via_sonado == 1 && t >= VIA_SALE - 200) { s_via_sonado = 2; accion(UI_ACCION_SONAR, s_via_tipo == VIAJE_BARCO || s_via_tipo == VIAJE_TREN ? SON[s_via_tipo] : SND_ALEGRE); }
    if (s_via_sonado == 2 && t >= 14400) { s_via_sonado = 3; accion(UI_ACCION_SONAR, SND_SUSPIRO_AMOR); }
    if (fr != s_via_fv) { s_via_fv = fr; poner_src(s_via_veh, &s_via_veh_art[fr].dsc); }
    /* ella se asoma y mira atrás hacia ti: se da vuelta y saluda abriendo la boca */
    int fc = (int)(t / 380) % 2;
    if (fc != s_via_fc) { s_via_fc = fc; poner_src(s_via_cri, &s_via_cri_art[fc].dsc); }
    int ax, ay;
    art_vehiculo_asiento((art_viaje_t)s_via_tipo, 4, &ax, &ay);
    int cw = (int)s_via_cri_art[0].dsc.header.w, ch = (int)s_via_cri_art[0].dsc.header.h;
    float salto = fabsf(sinf((float)t / 260.0f)) * 5;
    lv_obj_set_pos(s_via_veh, (int)x, (int)y);
    lv_obj_set_pos(s_via_cri, (int)x + ax - cw / 2, (int)(y + ay - ch + 4 - salto));
    const int hx = (int)x + ax, hy = (int)(y + ay - ch);
    /* el título de arriba */
    int o1 = t < 300 ? 0 : (t < 900 ? (int)(t - 300) * 255 / 600 : (t < 4600 ? 255 : (t < 5200 ? (int)(5200 - t) * 255 / 600 : 0)));
    lv_obj_set_style_opa(s_via_c1, (lv_opa_t)o1, 0);
    /* las despedidas, una tras otra, abajo */
    static const uint32_t DESDE[4] = {1900, 5200, 8300, 11400}, HASTA[4] = {4900, 8000, 11100, 14200};
    int lin = -1;
    for (int i = 0; i < 4; i++) if (t >= DESDE[i] && t < HASTA[i]) lin = i;
    if (lin != s_via_linea) {
        s_via_linea = lin;
        if (lin >= 0) {
            char tx[96];
            switch (lin) {
            case 0: snprintf(tx, sizeof tx, "GRACIAS POR CUIDARME"); break;
            case 1:
                if (s_via_dias >= 2) snprintf(tx, sizeof tx, "FUIMOS FELICES %d DÍAS JUNTOS", s_via_dias);
                else snprintf(tx, sizeof tx, "FUE MUY BONITO ESTAR CONTIGO");
                break;
            case 2: snprintf(tx, sizeof tx, "NUNCA TE VOY A OLVIDAR"); break;
            default: snprintf(tx, sizeof tx, "SIEMPRE ESTARÉ EN TU CORAZÓN"); break;
            }
            art_texto(&s_via_t2_art, tx, C_TINTA, 3, 250);
            cartel_poner(s_via_c2, s_via_t2, &s_via_t2_art);
            lv_obj_align(s_via_c2, LV_ALIGN_BOTTOM_MID, 0, -62);
            ocultar(s_via_c2, s_nino);
            for (int j = 0; j < 4; j++) via_corazon(hx - 40 + j * 26, hy - 10 - (int)(azar() % 20)); /* una ráfaga de corazones */
        } else ocultar(s_via_c2, true);
    }
    if (lin >= 0) {
        const uint32_t d = t - DESDE[lin], h = HASTA[lin] - t;
        int o = d < 400 ? (int)d * 255 / 400 : (h < 400 ? (int)h * 255 / 400 : 255);
        lv_obj_set_style_opa(s_via_c2, (lv_opa_t)o, 0);
    }
    /* un corazón suelto sale de ella cada ratito mientras se despide y se queda flotando atrás */
    if (t > 2500 && t < 13800 && t % 520 < TICK_MS) via_corazon(hx + (int)(azar() % 30) - 15, hy);
    for (int i = 0; i < VIA_N_COR; i++) {
        if (!s_via_cor_t0[i]) continue;
        const uint32_t d = s_ms - s_via_cor_t0[i];
        if (d > 2400) { s_via_cor_t0[i] = 0; ocultar(s_via_cor[i], true); continue; }
        const float f = (float)d / 2400.0f;
        lv_obj_set_y(s_via_cor[i], lv_obj_get_y(s_via_cor[i]) - 1 + (d % 3 == 0 ? 0 : 0));
        lv_obj_set_x(s_via_cor[i], lv_obj_get_x(s_via_cor[i]) + (int)(sinf((float)d / 260.0f + (float)i) * 1.4f + s_via_cor_dx[i] * 0.004f));
        lv_obj_set_style_opa(s_via_cor[i], (lv_opa_t)(f < 0.15f ? f / 0.15f * 255 : 255.0f * (1.0f - (f - 0.15f) / 0.85f)), 0);
    }
    /* el cielo vivo: estrellas que titilan, nubes que pasan y una estrella fugaz */
    const bool noche = s_via_tipo != VIAJE_GLOBO;
    for (int i = 0; i < VIA_N_EST; i++) {
        float px = (float)(40 + (i * 97) % 390), py = (float)(24 + (i * 53) % 170);
        if (!noche) { py = fmodf(py + 260.0f - (float)t * (0.012f + (float)(i % 4) * 0.004f), 260.0f) + 70.0f; px += sinf((float)t / 900.0f + (float)i) * 10.0f; }
        lv_obj_set_pos(s_via_est[i], (int)px, (int)py);
        lv_obj_set_style_opa(s_via_est[i], (lv_opa_t)(110 + 140 * (0.5f + 0.5f * sinf((float)t / (260.0f + (float)i * 41.0f) + (float)i))), 0);
    }
    for (int i = 0; i < VIA_N_NUBE; i++) {
        const float sp = 0.012f + (float)i * 0.005f;
        const float nx = fmodf((float)(i * 211) + (float)t * sp, (float)W + 130.0f) - 110.0f;
        lv_obj_set_pos(s_via_nube[i], (int)(W + 0 - (nx + 110.0f) - 20), 34 + (i * 47) % 150);
    }
    {
        static const uint32_t FUG[3] = {3300, 8100, 12300};
        for (int i = 0; i < 3; i++) {
            if (t >= FUG[i] && t < FUG[i] + 800 && noche) {
                const float f = (float)(t - FUG[i]) / 800.0f;
                lv_obj_set_pos(s_via_fugaz, (int)(330.0f - f * 260.0f + (float)(i * 20)), (int)(50.0f + f * 70.0f + (float)(i * 12)));
                lv_obj_set_style_opa(s_via_fugaz, (lv_opa_t)(255.0f * (f < 0.2f ? f / 0.2f : 1.0f - (f - 0.2f) / 0.8f)), 0);
                ocultar(s_via_fugaz, false);
                break;
            } else ocultar(s_via_fugaz, true);
        }
    }
    /* el final: el cielo se apaga y queda un corazón con "hasta pronto" */
    const float oscuro = t < 13400 ? 0.0f : (t < 15200 ? (float)(t - 13400) / 1800.0f : 1.0f);
    lv_obj_set_style_opa(s_via_negro, (lv_opa_t)(oscuro * 235), 0);
    if (t > 14300) {
        const float k = suave((float)(t - 14300) / 1400.0f), lat = 1.0f + 0.07f * sinf((float)t / 220.0f);
        lv_image_set_scale(s_via_fin_cor, (uint32_t)(256.0f * lat));
        lv_obj_align(s_via_fin_cor, LV_ALIGN_CENTER, 0, -62);
        lv_obj_set_style_opa(s_via_fin_cor, (lv_opa_t)(k * 255), 0);
        lv_obj_set_style_opa(s_via_fin, (lv_opa_t)(k * 255), 0);
        lv_obj_set_style_opa(s_via_t4, (lv_opa_t)(k * 255), 0);
        ocultar(s_via_fin_cor, false); ocultar(s_via_t4, s_nino || t < 15200);
        ocultar(s_via_fin, s_nino);
        if (t % 400 < TICK_MS && t < 17000) via_corazon(CX - 70 + (int)(azar() % 140), 250);
    }
    if (t > VIAJE_MS) viaje_fin();
}


/* ======================================================================
 * El cruce a la isla: el bote de lado sobre el lago; soplándole a la vela (o tocándola) va más rápido
 * ==================================================================== */
static lv_obj_t *s_cru, *s_cru_fondo, *s_cru_bote, *s_cru_cri, *s_cru_pez, *s_cru_c, *s_cru_t, *s_cru_viento, *s_cru_marea;
static bool s_cru_con_marea; /* Marea nada junto al bote, si ya la conocen */
static lv_obj_t *s_cru_ruta, *s_cru_marca, *s_cru_desde_ico, *s_cru_hasta_ico;
static art_img_t s_cru_fondo_art, s_cru_bote_art[2], s_cru_cri_art[2], s_cru_t_art;
static bool s_cru_ida;
static float s_cru_prog;
static uint32_t s_cru_desde, s_cru_rafaga, s_cru_ult;
static int s_cru_fb = -1, s_cru_fc = -1, s_cru_rafagas;
static art_periodo_t periodo_de(const pet_t *p, uint32_t utc);

static void cruce_fin(void)
{
    if (!s_cru_activo) return;
    s_cru_activo = false;
    ocultar(s_cru, true);
    lv_image_set_src(s_cru_fondo, NULL);
    lv_image_set_src(s_cru_bote, NULL);
    lv_image_set_src(s_cru_cri, NULL);
    art_img_free(&s_cru_fondo_art); /* el paisaje pesa: se libera al llegar */
    for (int i = 0; i < 2; i++) { art_img_free(&s_cru_bote_art[i]); art_img_free(&s_cru_cri_art[i]); }
    accion(UI_ACCION_SONAR, SND_ALEGRE);
}
static lv_obj_t *s_cru_aire[4]; /* rayitas de viento que salen cuando se detecta el soplido */
static uint32_t s_cru_aire_t = 0;
static void cruce_rafaga(void) /* un soplido (o un toque en la vela): la vela se llena */
{
    if (!s_cru_activo) return;
    s_cru_aire_t = s_ms ? s_ms : 1;
    s_cru_rafaga = s_ms + 1500;
    s_cru_rafagas++;
}
static void cruce_cb(lv_event_t *e) { (void)e; cruce_rafaga(); }
static void espejar(art_img_t *a)
{
    int w = (int)a->dsc.header.w, h = (int)a->dsc.header.h;
    if (!a->px) return;
    if (a->dsc.header.cf == LV_COLOR_FORMAT_RGB565) { /* el fondo del cruce (v88) */
        uint16_t *q = (uint16_t *)a->px;
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w / 2; x++) { uint16_t t = q[y * w + x]; q[y * w + x] = q[y * w + w - 1 - x]; q[y * w + w - 1 - x] = t; }
        return;
    }
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w / 2; x++) { uint32_t t = a->px[y * w + x]; a->px[y * w + x] = a->px[y * w + w - 1 - x]; a->px[y * w + w - 1 - x] = t; }
}
static void crear_cruce(void)
{
    s_cru = caja(lv_layer_top());
    lv_obj_set_size(s_cru, W, W);
    lv_obj_add_flag(s_cru, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_cru, cruce_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_set_style_bg_color(s_cru, lv_color_hex(0xA8DCF4), 0);
    lv_obj_set_style_bg_grad_color(s_cru, lv_color_hex(0xE8F6FF), 0);
    lv_obj_set_style_bg_grad_dir(s_cru, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_bg_opa(s_cru, LV_OPA_COVER, 0);
    { lv_obj_t *agua = caja(s_cru); /* el agua de siempre, aunque el paisaje no se pueda dibujar */
      lv_obj_set_size(agua, W, 250); lv_obj_set_pos(agua, 0, W - 250);
      lv_obj_set_style_bg_color(agua, lv_color_hex(0x2E7FB8), 0);
      lv_obj_set_style_bg_grad_color(agua, lv_color_hex(0x5AB4E0), 0);
      lv_obj_set_style_bg_grad_dir(agua, LV_GRAD_DIR_VER, 0);
      lv_obj_set_style_bg_opa(agua, LV_OPA_COVER, 0);
      lv_obj_remove_flag(agua, LV_OBJ_FLAG_CLICKABLE); }
    s_cru_fondo = lv_image_create(s_cru);
    s_cru_pez = lv_image_create(s_cru);
    s_cru_marea = lv_image_create(s_cru); /* a su lado: asoma del agua y lo acompaña */
    s_cru_cri = lv_image_create(s_cru);
    s_cru_bote = lv_image_create(s_cru);
    /* arriba, el camino: del lago a la isla, con el botecito que avanza (no hay cartel: no es un viaje de despedida) */
    s_cru_ruta = caja(s_cru);
    lv_obj_set_size(s_cru_ruta, 236, 40);
    lv_obj_align(s_cru_ruta, LV_ALIGN_TOP_MID, 0, 34);
    lv_obj_set_style_radius(s_cru_ruta, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(s_cru_ruta, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(s_cru_ruta, LV_OPA_50, 0);
    for (int i = 0; i < 9; i++) {
        lv_obj_t *d = caja(s_cru_ruta);
        lv_obj_set_size(d, 4, 4);
        lv_obj_set_style_radius(d, 2, 0);
        lv_obj_set_style_bg_color(d, lv_color_hex(0x3A6A9A), 0);
        lv_obj_set_style_bg_opa(d, LV_OPA_70, 0);
        lv_obj_set_pos(d, 46 + i * 17, 18);
    }
    s_cru_desde_ico = lv_image_create(s_cru_ruta);
    lv_obj_align(s_cru_desde_ico, LV_ALIGN_LEFT_MID, 6, 0);
    s_cru_hasta_ico = lv_image_create(s_cru_ruta);
    lv_obj_align(s_cru_hasta_ico, LV_ALIGN_RIGHT_MID, -6, 0);
    s_cru_marca = lv_image_create(s_cru_ruta);
    lv_image_set_src(s_cru_marca, art_icono(ICO_BOTE, 2));
    /* abajo, una pista corta: soplar */
    s_cru_c = cartel_de(s_cru, &s_cru_t, 0x2A2238, LV_OPA_70);
    for (int i = 0; i < 4; i++) { /* el viento que se ve al soplar */
        s_cru_aire[i] = caja(s_cru);
        lv_obj_set_size(s_cru_aire[i], 34 - i * 4, 4);
        lv_obj_set_style_radius(s_cru_aire[i], 2, 0);
        lv_obj_set_style_bg_color(s_cru_aire[i], lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_bg_opa(s_cru_aire[i], LV_OPA_COVER, 0);
        lv_obj_remove_flag(s_cru_aire[i], LV_OBJ_FLAG_CLICKABLE);
        ocultar(s_cru_aire[i], true);
    }
    s_cru_viento = lv_image_create(s_cru);
    lv_image_set_src(s_cru_viento, art_icono(ICO_SOPLA, 3));
    ocultar(s_cru, true);
}
static bool s_cru_hielo; /* en invierno cruza a pie por el hielo, sin bote */
static void cruce_empezar(const pet_t *m, bool ida)
{
    cruce_fin();
    cerrar_panel();
    s_cru_activo = true;
    s_cru_ida = ida;
    s_cru_prog = 0;
    s_cru_desde = s_cru_ult = s_ms;
    s_cru_rafaga = 0;
    s_cru_rafagas = 0;
    s_cru_fb = s_cru_fc = -1;
    const bool hielo = lago_congelado(m);
    s_cru_hielo = hielo;
    art_cruce_estacion((int)pet_season(m, s_ultimo_t));
    art_cruce_fondo(&s_cru_fondo_art, (int)periodo_de(m, s_ultimo_t));
#ifdef ESP_PLATFORM
    if (!s_cru_fondo_art.px) { /* diagnóstico: si el fondo del cruce no se carga, el registro lo dice (antes se quedaba en blanco sin avisar) */
        extern void hal_evento(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
        hal_evento("cruce en bote: NO se cargó el fondo (sin memoria), periodo %d, PSRAM %u KB, bloque mayor %u KB", (int)periodo_de(m, s_ultimo_t),
                   (unsigned)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024), (unsigned)(heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM) / 1024));
    }
#endif
    if (!ida) espejar(&s_cru_fondo_art); /* de vuelta, la isla queda atrás */
    poner_src(s_cru_fondo, &s_cru_fondo_art.dsc);
    lv_obj_set_pos(s_cru_fondo, -1, -1);
    for (int f = 0; f < 2; f++) {
        art_bote_vela(&s_cru_bote_art[f], f, 3);
        if (!ida) espejar(&s_cru_bote_art[f]);
        art_pose_t po = {pet_look(m), m->etapa, m->etapa == STAGE_ADULTA ? m->forma : 0, 3, f ? EXP_FELIZ : EXP_CURIOSO, f == 1, f, !ida, false, 1, 1, 0, {0, 0, 0}, 0, m->guardiana};
        for (int r = 0; r < RANURA_COUNT; r++) po.ropa[r] = pet_worn_raw(m, r);
        art_criatura_pose(&s_cru_cri_art[f], &po);
    }
    lv_image_set_src(s_cru_pez, art_icono(hielo ? ICO_COPO : ICO_PEZ, 3));
    ocultar(s_cru_pez, true);
    s_cru_con_marea = !hielo && ((m->criaturas_vistas >> CRI_MAREA) & 1u); /* solo si ya la conocen (y no cuando el lago está helado) */
    ocultar(s_cru_marea, !s_cru_con_marea);
    if (s_cru_con_marea) lv_image_set_src(s_cru_marea, ida ? art_icono(ICO_MAREA, 4) : art_icono_espejo(ICO_MAREA, 4));
    lv_image_set_src(s_cru_desde_ico, art_icono(ICO_ZONA_LAGO, 2)); /* el lago siempre a la izquierda y la isla a la derecha, como en el cruce */
    lv_image_set_src(s_cru_hasta_ico, art_icono(ICO_ZONA_ISLA, 2));
    art_texto(&s_cru_t_art, hielo ? "SOPLA Y DESLIZA SOBRE EL HIELO" : "SOPLA PARA LLENAR LA VELA", 0xFFFFFF, 2, 0);
    cartel_poner(s_cru_c, s_cru_t, &s_cru_t_art);
    lv_obj_align(s_cru_c, LV_ALIGN_BOTTOM_MID, 18, -92);
    lv_obj_align_to(s_cru_viento, s_cru_c, LV_ALIGN_OUT_LEFT_MID, -4, 0);
    ocultar(s_cru_c, s_nino); /* los más chicos: solo el dibujo del viento */
    if (s_nino) lv_obj_align(s_cru_viento, LV_ALIGN_BOTTOM_MID, 0, -96);
    ocultar(s_cru, false);
    lv_obj_move_foreground(s_cru);
}
static void animar_cruce(void)
{
    if (!s_cru_activo) return;
    float dt = (float)(s_ms - s_cru_ult) / 1000.0f;
    s_cru_ult = s_ms;
    if (dt > 0.2f) dt = 0.2f;
    bool viento = (int32_t)(s_cru_rafaga - s_ms) > 0;
    if (s_nino && !viento && (s_ms - s_cru_desde) % 3000 < 20) cruce_rafaga(); /* a los más chicos el viento les ayuda solo */
    s_cru_prog += dt * (viento ? 0.20f : 0.065f); /* sin viento ~15 s; soplando, unos 5 */
    const int bw = (int)s_cru_bote_art[0].dsc.header.w, bh = (int)s_cru_bote_art[0].dsc.header.h;
    float k = s_cru_prog > 1 ? 1 : s_cru_prog;
    float x0 = -20.0f, x1 = (float)(W - bw + 20);
    float x = s_cru_ida ? x0 + (x1 - x0) * k : x1 - (x1 - x0) * k;
    float y = (float)ART_CRUCE_AGUA_Y + 54 - (float)bh + sinf((float)s_ms / (viento ? 260.0f : 480.0f)) * (viento ? 4 : 3);
    int fb = viento ? 1 : 0;
    if (fb != s_cru_fb) { s_cru_fb = fb; poner_src(s_cru_bote, &s_cru_bote_art[fb].dsc); }
    int fc = viento ? (int)(s_ms / 300) % 2 : 0;
    if (fc != s_cru_fc) { s_cru_fc = fc; poner_src(s_cru_cri, &s_cru_cri_art[fc].dsc); }
    int ax, ay;
    art_bote_vela_asiento(3, &ax, &ay);
    if (!s_cru_ida) ax = bw - ax;
    int cw = (int)s_cru_cri_art[0].dsc.header.w, ch = (int)s_cru_cri_art[0].dsc.header.h;
    float hunde = s_cru_hielo ? 30.0f : (s_pet && s_pet->etapa == STAGE_ADULTA ? 8.0f : 2.0f);
    lv_obj_set_pos(s_cru_bote, (int)x, (int)y);
    ocultar(s_cru_bote, s_cru_hielo);
    lv_obj_set_pos(s_cru_cri, (int)x + ax - cw / 2, (int)(y + ay - ch + hunde));
    if (s_cru_con_marea) { /* Marea nada al lado del bote (junto a la proa), subiendo y bajando con las olas; lo acompaña, no lo empuja */
        const int mw = (int)lv_obj_get_width(s_cru_marea) > 0 ? (int)lv_obj_get_width(s_cru_marea) : 48, mh = (int)lv_obj_get_height(s_cru_marea) > 0 ? (int)lv_obj_get_height(s_cru_marea) : 48;
        const float ola = sinf((float)s_ms / (viento ? 190.0f : 330.0f));
        const int mx = s_cru_ida ? (int)x + bw - 26 : (int)x - mw + 26;
        lv_obj_set_pos(s_cru_marea, mx, ART_CRUCE_AGUA_Y + 64 - mh + (int)(ola * (viento ? 5 : 3)));
    }
    { /* viento: cuatro rayitas que corren hacia donde va el bote durante ~0,7 s (así se ve que el soplido se oyó) */
        const int32_t ta = s_cru_aire_t ? (int32_t)(s_ms - s_cru_aire_t) : 9999;
        for (int i = 0; i < 4; i++) {
            const int32_t ti = ta - i * 70;
            const bool ver = ti >= 0 && ti < 600;
            ocultar(s_cru_aire[i], !ver);
            if (ver) {
                const int paso = ti / 100 * 100; /* a saltos: pocos repintados */
                const int dx = paso * 3 / 10 * 2;
                const int base = s_cru_ida ? (int)x - 30 : (int)x + bw + 30 - (34 - i * 4);
                lv_obj_set_pos(s_cru_aire[i], s_cru_ida ? base + dx : base - dx, (int)y + 14 + i * 13);
                lv_obj_set_style_opa(s_cru_aire[i], (lv_opa_t)(paso < 300 ? 200 : 100), 0);
            }
        }
    }
    /* a mitad de camino salta un pez */
    float pk = (s_cru_prog - 0.42f) / 0.16f;
    bool pez = pk > 0 && pk < 1;
    ocultar(s_cru_pez, !pez);
    if (pez) {
        float px = s_cru_ida ? x + bw + 40 + pk * 50 : x - 70 - pk * 50;
        lv_obj_set_pos(s_cru_pez, (int)px, (int)((float)ART_CRUCE_AGUA_Y + 40 - sinf(pk * 3.1416f) * 60));
    }
    /* el botecito del camino avanza con ella */
    lv_image_set_src(s_cru_marca, s_cru_ida ? art_icono(ICO_BOTE, 2) : art_icono_espejo(ICO_BOTE, 2)); /* mira hacia donde va */
    lv_obj_set_pos(s_cru_marca, 34 + (int)((s_cru_ida ? k : 1.0f - k) * 144.0f), 8 + (int)(sinf((float)s_ms / 300.0f) * 1.5f));
    /* la pista de soplar se va cuando ya sopló un par de veces (vuelve si deja de soplar un rato) */
    uint32_t t = s_ms - s_cru_desde;
    bool pista = (s_cru_rafagas < 2 && t > 600) || (!viento && (int32_t)(s_ms - s_cru_rafaga) > 4000 && t > 5000);
    lv_opa_t op = pista ? LV_OPA_COVER : LV_OPA_TRANSP;
    lv_obj_set_style_opa(s_cru_c, op, 0);
    lv_obj_set_style_opa(s_cru_viento, op, 0);
    if (pista) lv_obj_set_style_translate_x(s_cru_viento, (int)(sinf((float)s_ms / 160.0f) * 3), 0);
    if (s_cru_prog >= 1.08f) cruce_fin();
}



/* el rito de la Guardiana: una ceremonia de unos 16 segundos. Cae la noche sobre el círculo de piedras, el Corazón del
 * Bosque sale de ella y flota; ocho runas se encienden en el suelo, las luciérnagas acuden, ondas de luz, la columna la
 * levanta con los ojos cerrados y, en un destello, aparece la capa y la corona; baja despacio entre hojas de luz */
static lv_obj_t *s_rito_col, *s_rito_velo, *s_rito_noche, *s_rito_cor, *s_rito_runa[8], *s_rito_aro, *s_rito_cor2; /* cor2 (v37): la mitad que brilla dentro de ella */
static int s_rito_fase;
#define RITO_MS 16500
static void rito_empezar(void)
{
    if (!s_rito_col) {
        s_rito_col = caja(s_mundo);
        lv_obj_set_size(s_rito_col, 78, 260);
        lv_obj_set_style_radius(s_rito_col, 39, 0);
        lv_obj_set_style_bg_color(s_rito_col, lv_color_hex(0xFFF6C8), 0);
        lv_obj_set_style_bg_grad_color(s_rito_col, lv_color_hex(0xB8FF8A), 0);
        lv_obj_set_style_bg_grad_dir(s_rito_col, LV_GRAD_DIR_VER, 0);
        lv_obj_set_style_bg_main_opa(s_rito_col, 0, 0);
        lv_obj_set_style_bg_grad_opa(s_rito_col, 230, 0);
        lv_obj_set_style_bg_opa(s_rito_col, LV_OPA_COVER, 0);
        lv_obj_set_style_shadow_width(s_rito_col, 44, 0);
        lv_obj_set_style_shadow_color(s_rito_col, lv_color_hex(0xFFE27A), 0);
        lv_obj_remove_flag(s_rito_col, LV_OBJ_FLAG_CLICKABLE);
        s_rito_velo = caja(lv_screen_active());
        lv_obj_set_size(s_rito_velo, W, W);
        lv_obj_set_style_bg_color(s_rito_velo, lv_color_hex(0xFFF6D8), 0);
        lv_obj_set_style_bg_opa(s_rito_velo, LV_OPA_COVER, 0);
        lv_obj_remove_flag(s_rito_velo, LV_OBJ_FLAG_CLICKABLE);
        s_rito_noche = caja(s_mundo); /* la noche cae: oscurece el prado, no a ella */
        lv_obj_set_size(s_rito_noche, W, W);
        lv_obj_set_style_bg_color(s_rito_noche, lv_color_hex(0x070C2A), 0);
        lv_obj_set_style_bg_opa(s_rito_noche, LV_OPA_COVER, 0);
        lv_obj_remove_flag(s_rito_noche, LV_OBJ_FLAG_CLICKABLE);
        s_rito_cor = lv_image_create(s_mundo);
        lv_obj_remove_flag(s_rito_cor, LV_OBJ_FLAG_CLICKABLE);
        s_rito_cor2 = lv_image_create(s_mundo);
        lv_obj_remove_flag(s_rito_cor2, LV_OBJ_FLAG_CLICKABLE);
        lv_image_set_src(s_rito_cor2, art_icono(ICO_MEDIO_CORAZON, 2));
        s_rito_aro = caja(s_mundo); /* el círculo de runas en el suelo */
        lv_obj_set_style_radius(s_rito_aro, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(s_rito_aro, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(s_rito_aro, 2, 0);
        lv_obj_set_style_border_color(s_rito_aro, lv_color_hex(0xFFE27A), 0);
        lv_obj_set_style_shadow_width(s_rito_aro, 14, 0);
        lv_obj_set_style_shadow_color(s_rito_aro, lv_color_hex(0xFFE27A), 0);
        lv_obj_remove_flag(s_rito_aro, LV_OBJ_FLAG_CLICKABLE);
        for (int i = 0; i < 8; i++) {
            s_rito_runa[i] = caja(s_mundo);
            lv_obj_set_size(s_rito_runa[i], 9, 9);
            lv_obj_set_style_radius(s_rito_runa[i], 2, 0);
            lv_obj_set_style_bg_color(s_rito_runa[i], lv_color_hex(i & 1 ? 0xB8FF8A : 0xFFE27A), 0);
            lv_obj_set_style_bg_opa(s_rito_runa[i], LV_OPA_COVER, 0);
            lv_obj_set_style_shadow_width(s_rito_runa[i], 14, 0);
            lv_obj_set_style_shadow_color(s_rito_runa[i], lv_color_hex(i & 1 ? 0x7AFFB0 : 0xFFD24A), 0);
            lv_obj_remove_flag(s_rito_runa[i], LV_OBJ_FLAG_CLICKABLE);
        }
    }
    lv_image_set_src(s_rito_cor, art_icono(ICO_OBJETO(OBJ_CORAZON_BOSQUE), 5));
    lv_obj_set_pos(s_rito_col, LXY(LUGAR_ROCAS)[0] + 26 - 39, LXY(LUGAR_ROCAS)[1] + 14 - 260);
    lv_obj_move_to_index(s_rito_noche, 1); /* sobre el pasto, debajo de todos */
    ocultar(s_rito_col, true); ocultar(s_rito_velo, true); ocultar(s_rito_noche, false); ocultar(s_rito_cor, true); ocultar(s_rito_aro, true); ocultar(s_rito_cor2, true);
    for (int i = 0; i < 8; i++) ocultar(s_rito_runa[i], true);
    lv_obj_set_style_opa(s_rito_noche, 0, 0);
    s_rito_desde = s_ms;
    s_rito_fase = 0;
    s_rito_on = true;
}
static int rito_cx(void) { return LXY(LUGAR_ROCAS)[0] + 26; }
static int rito_cy(void) { return LXY(LUGAR_ROCAS)[1] + 10; }
/* la levitación: sube con la luz, se queda en el aire con los ojos cerrados y baja despacio */
static float rito_alza(uint32_t t)
{
    if (t < 6000) return 0;
    if (t < RITO_FLASH + 200) return 58.0f * suave((float)(t - 6000) / 1200.0f);
    if (t < 10600) return 58.0f * (1.0f - suave((float)(t - (RITO_FLASH + 200)) / 3200.0f));
    return 0;
}
static bool animar_rito_mascota(criatura_t *c, float *sx, float *sy, bool *caminando)
{
    if (!s_rito_on) return false;
    const uint32_t t = s_ms - s_rito_desde;
    art_pose_t *ps = &c->pose;
    c->tx = (float)rito_cx(); c->ty = (float)rito_cy();
    *caminando = mover(c, 3.2f);
    ps->orejas = 1; ps->cola = 1; ps->expr = EXP_FELIZ;
    if (!*caminando) ps->espejo = false;
    const float alza = rito_alza(t);
    if (alza > 0) {
        ps->expr = EXP_CERRADOS; /* se deja llevar por la luz */
        *sy = -alza + sinf((float)t / 380.0f) * 3.0f;
        *sx = sinf((float)t / 900.0f) * 2.0f;
        ps->postura = 1;
    } else if (t > 10600 && t < 12600) {
        *sy = -fabsf(sinf((float)t / 140.0f)) * 9; ps->boca = true; /* contenta: saltitos al aterrizar */
    }
    (void)sx;
    return true;
}
static void animar_rito(void)
{
    if (!s_rito_on) return;
    const uint32_t t = s_ms - s_rito_desde;
    const int cx = rito_cx(), cy = rito_cy();
    if (t > RITO_MS || s_interior || s_escena != ESC_ISLA) {
        const bool natural = t > RITO_MS && !s_interior && s_escena == ESC_ISLA;
        s_rito_on = false;
        ocultar(s_rito_col, true); ocultar(s_rito_velo, true); ocultar(s_rito_noche, true); ocultar(s_rito_cor, true); ocultar(s_rito_aro, true); ocultar(s_rito_cor2, true);
        for (int i = 0; i < 8; i++) ocultar(s_rito_runa[i], true);
        if (natural && s_pet && s_panel == PANEL_NINGUNO && !s_nino) rito_historia(); /* el Libro cuenta por qué es la Guardiana (por hojas); después, su nombre y el cierre del Libro */
        return;
    }
    /* la noche cae al empezar y se va al final */
    float nk = t < 1500 ? (float)t / 1500.0f : (t > 12800 ? (float)(RITO_MS - t) / (float)(RITO_MS - 12800) : 1.0f);
    if (t > RITO_FLASH && t < 9400) nk *= 0.55f; /* con la luz, la noche se aparta */
    lv_obj_set_style_opa(s_rito_noche, (lv_opa_t)(nk * 130), 0);
    /* globos de la ceremonia */
    if (s_rito_fase == 0 && t > 1300) { s_rito_fase = 1; globo(ICO_OBJETO(OBJ_CORAZON_BOSQUE), s_nino ? NULL : "EL CORAZÓN DEL MUNDO ESTÁ DESPERTANDO...", 3200); accion(UI_ACCION_SONAR, SND_SUSPIRO_AMOR); }
    if (s_rito_fase == 1 && t > 4700) { s_rito_fase = 2; globo(ICO_MEDIO_CORAZON, s_nino ? NULL : "¿ESCUCHAS? ALGO LATE DENTRO DE MÍ... ¡AL MISMO TIEMPO!", 2400); accion(UI_ACCION_SONAR, SND_RARO); }
    if (s_rito_fase == 2 && t > RITO_FLASH) { /* ¡el destello! */
        s_rito_fase = 3;
        if (s_pet) refrescar_fondo(s_pet, s_ultimo_t); /* ahora sí: el Corazón queda latiendo en la piedra */
        accion(UI_ACCION_SONAR, SND_META);
        for (int i = 0; i < 7; i++) onda(cx, cy + 6, i & 1 ? 0xB8FF8A : 0xFFE27A, 150 + i * 14, true, (uint32_t)i * 120);
        for (int i = 0; i < 18; i++) chispa(cx - 20 + (int)(azar() % 56), cy - 50 + (int)(azar() % 50), i & 1 ? 0xB8FF8A : 0xFFE27A, (azar01() - 0.5f) * 2.4f, -0.4f - azar01() * 1.2f);
    }
    if (s_rito_fase == 3 && t > 9500) { s_rito_fase = 4; globo(ICO_MEDIO_CORAZON, s_nino ? NULL : "¡LA OTRA MITAD... ESTABA EN MÍ! AHORA SOY {LA} {GUARDIANA} DEL MUNDO", 5200); }
    if (s_rito_fase == 4 && t > 12400) {
        s_rito_fase = 5;
        globo(ICO_CORAZON, s_nino ? NULL : "GRACIAS POR CUIDARME HASTA AQUÍ. TE VOY A PROTEGER SIEMPRE", 4000);
        for (int i = 0; i < 5; i++) flotar(ICO_CORAZON, NULL, 0, s_mascota.x - 48 + i * 24, cabeza_y(&s_mascota), (uint32_t)i * 160);
    }
    /* el círculo de runas: aparece, y las ocho se encienden una a una */
    if (t > 1700 && t < 14200) {
        float ak = t < 2400 ? (float)(t - 1700) / 700.0f : (t > 12400 ? (float)(14200 - t) / 1800.0f : 1.0f);
        const int rw = 150 + (int)(sinf((float)t / 500.0f) * 3), rh = 46;
        lv_obj_set_size(s_rito_aro, rw, rh);
        lv_obj_set_pos(s_rito_aro, cx - rw / 2, cy + 12 - rh / 2);
        lv_obj_set_style_opa(s_rito_aro, (lv_opa_t)(ak * 255), 0);
        ocultar(s_rito_aro, false);
        lv_obj_move_to_index(s_rito_aro, lv_obj_get_index(s_mascota.sombra));
        for (int i = 0; i < 8; i++) {
            const uint32_t enciende = 2000 + (uint32_t)i * 330;
            const float a = (float)i * 0.7854f + (float)t / 5200.0f; /* giran muy despacio */
            lv_obj_set_pos(s_rito_runa[i], cx + (int)(cosf(a) * rw / 2) - 4, cy + 12 + (int)(sinf(a) * rh / 2) - 4);
            const bool on = t > enciende;
            ocultar(s_rito_runa[i], !on);
            if (on) lv_obj_set_style_opa(s_rito_runa[i], (lv_opa_t)(ak * (t < enciende + 500 ? 255 : 170 + 85 * sinf((float)t / 260.0f + (float)i))), 0);
            if (t > enciende && t < enciende + 40) { chispa(cx + (int)(cosf(a) * rw / 2), cy + 12 + (int)(sinf(a) * rh / 2), 0xFFE27A, 0, -0.9f); accion(UI_ACCION_SONAR, SND_TOQUE); }
            lv_obj_move_to_index(s_rito_runa[i], lv_obj_get_index(s_mascota.sombra));
        }
    } else {
        ocultar(s_rito_aro, true);
        for (int i = 0; i < 8; i++) ocultar(s_rito_runa[i], true);
    }
    /* luciérnagas que acuden desde todos lados hacia el centro */
    if (t > 1600 && t < 7000 && t % 110 < TICK_MS) {
        const float a = azar01() * 6.2832f, d = 190.0f + azar01() * 40.0f;
        const int px = cx + (int)(cosf(a) * d), py = cy - 30 + (int)(sinf(a) * d * 0.6f);
        chispa(px, py, azar() & 1 ? 0xB8FF8A : 0xFFE27A, -cosf(a) * 2.6f, -sinf(a) * 1.6f);
    }
    /* el Corazón del Mundo: sale de ella, sube y flota sobre su cabeza, late y se enciende */
    if (t > 1900 && t < RITO_FLASH + 500) {
        const float k = suave((float)(t - 1900) / 1800.0f);
        const float lat = 1.0f + 0.12f * sinf((float)t / (t > 4700 ? 160.0f : 300.0f));
        const int hy = (int)(cy - 28 - k * 56 - rito_alza(t) * 0.0f) - (int)rito_alza(t);
        lv_image_set_scale(s_rito_cor, (uint32_t)(256 * lat));
        lv_obj_set_pos(s_rito_cor, cx - 30, hy - 30);
        lv_obj_set_style_opa(s_rito_cor, (lv_opa_t)(t > RITO_FLASH ? 255 - (int)((t - RITO_FLASH) * 255 / 500) : 255), 0);
        ocultar(s_rito_cor, false);
        lv_obj_move_foreground(s_rito_cor);
        if (t > 4700 && t % 250 < TICK_MS) onda(cx, cy - 30 - (int)(k * 40), 0xB8FF8A, 90, false, 0);
    } else ocultar(s_rito_cor, true);
    /* (v37) la otra mitad: brilla dentro de ella; primero late a destiempo con la de arriba y después, al mismo tiempo (se reconocen) */
    if (t > 4300 && t < 12600) {
        const float desfase = t < 6200 ? 1.5708f : 0.0f;
        const float lat = 1.0f + 0.18f * sinf((float)t / 160.0f + desfase);
        lv_image_set_scale(s_rito_cor2, (uint32_t)(256 * lat));
        lv_obj_set_pos(s_rito_cor2, (int)s_mascota.x - 24, cabeza_y(&s_mascota) + 18 - (int)rito_alza(t));
        lv_obj_set_style_opa(s_rito_cor2, (lv_opa_t)(t < 4800 ? (t - 4300) * 255 / 500 : (t > 11800 ? (12600 - t) * 255 / 800 : 255)), 0);
        ocultar(s_rito_cor2, false);
        lv_obj_move_foreground(s_rito_cor2);
        if (t > 6200 && t < RITO_FLASH && t % 180 < TICK_MS) { /* un hilo de luz entre las dos mitades */
            const float k = (float)((t / 180) % 6) / 6.0f;
            chispa((int)(s_mascota.x + (rito_cx() - s_mascota.x) * k), (int)(cabeza_y(&s_mascota) + 30 - (cabeza_y(&s_mascota) + 30 - (rito_cy() - 80)) * k), 0xB8FF8A, 0, -0.3f);
        }
    } else ocultar(s_rito_cor2, true);
    /* la columna de luz: crece desde que la levanta y se apaga despacio */
    if (t > 5600 && t < 11800) {
        float k = t < 6800 ? (float)(t - 5600) / 1200.0f : (t > 9400 ? (float)(11800 - t) / 2400.0f : 1.0f);
        lv_obj_set_style_opa(s_rito_col, (lv_opa_t)(k * 255), 0);
        lv_obj_set_style_translate_y(s_rito_col, (int)(sinf((float)t / 400.0f) * 4), 0);
        ocultar(s_rito_col, false);
        lv_obj_move_to_index(s_rito_col, lv_obj_get_index(s_mascota.sombra)); /* la luz queda detrás de ella */
    } else ocultar(s_rito_col, true);
    /* el velo del destello */
    if (t > RITO_FLASH - 250 && t < RITO_FLASH + 1500) {
        const float f = t < RITO_FLASH ? (float)(t - (RITO_FLASH - 250)) / 250.0f : 1.0f - (float)(t - RITO_FLASH) / 1500.0f;
        lv_obj_set_style_opa(s_rito_velo, (lv_opa_t)(f * 235), 0);
        ocultar(s_rito_velo, false);
        lv_obj_move_foreground(s_rito_velo);
    } else ocultar(s_rito_velo, true);
    /* hojas de luz que giran hacia arriba (espiral) mientras baja, y estrellitas al final */
    if (t > RITO_FLASH && t < 11500 && t % 90 < TICK_MS) {
        const float a = (float)t / 160.0f;
        chispa(cx + (int)(cosf(a) * 46), cy + 8 + (int)(sinf(a) * 14), t % 180 < 90 ? 0xB8FF8A : 0xFFF6C8, cosf(a + 1.57f) * 0.7f, -1.5f);
    }
}

/* ======================================================================
 * La familia: el huevo que dejó (se toca para cuidarlo en otro perfil) y la despedida natural
 * ==================================================================== */
static lv_obj_t *s_huevo_fam;
static int lugar_libre(void)
{
    for (int i = 0; i < PET_GUARDADAS; i++) if (!masc_leer(i, NULL)) return i;
    return -1;
}
static void familia_cb(lv_event_t *e)
{
    int q = (int)(intptr_t)lv_event_get_user_data(e);
    cerrar_panel();
    if (q == 1 && s_pet && s_pet->huevo_familia && lugar_libre() >= 0) {
        s_nombre[0] = 0;
        accion(UI_ACCION_FAMILIA, 0);
    } else if (q == 3) { s_confirmar_nueva = 0; s_masc_sel = -1; s_masc_conf = 0; panel_mascotas(); }
}
static void panel_familia(void)
{
    if (!s_pet) return;
    abrir_tarjeta(PANEL_FAMILIA, 360, 380);
    bool hay = lugar_libre() >= 0;
    lv_obj_t *h = icono(s_tarjeta, ICO_HUEVO, 5);
    lv_obj_align(h, LV_ALIGN_TOP_MID, 0, 18);
    if (!s_nino) {
        char t[360];
        if (hay) snprintf(t, sizeof t, "%s CUIDÓ ESTE HUEVITO CON MUCHO MIMO, EN SU NIDITO. ¡MIRA, LATE DESPACITO! PARECE QUE YA QUIERE CONOCERTE. TENDRÁ SU FORMA, SU COLOR Y ALGO DE SU MANERA DE SER. ¿QUIERES QUE ECLOSIONE AHORA? %s SE QUEDA EN PERFILES Y LA PUEDES TRAER CUANDO QUIERAS.",
                          s_pet->nombre, s_pet->nombre);
        else snprintf(t, sizeof t, "%s DEJÓ UN HUEVO, PERO NO HAY LUGAR PARA OTRA MASCOTA. EN PERFILES PUEDES DEJAR IR A UNA DE VIAJE; EL HUEVO TE ESPERA.", s_pet->nombre);
        lv_obj_t *ar = area_texto(s_tarjeta, 30, 96, 300, 170);
        texto_l(ar, t, C_TINTA, 2, 290);
        area_lista(ar);
    }
    lv_obj_t *si = boton(s_tarjeta, 150, 48, familia_cb, hay ? 1 : 3);
    lv_obj_align(si, LV_ALIGN_BOTTOM_MID, -80, -24);
    lv_obj_set_style_bg_color(si, lv_color_hex(hay ? C_VERDE : C_VIOLETA), 0);
    lv_obj_t *st = s_nino ? icono(si, hay ? ICO_SI : ICO_AMIGOS, 3) : texto(si, hay ? "QUE NAZCA" : "PERFILES", C_BLANCO, 2, 0);
    lv_obj_center(st);
    lv_obj_t *no = boton(s_tarjeta, 150, 48, familia_cb, 2);
    lv_obj_align(no, LV_ALIGN_BOTTOM_MID, 80, -24);
    lv_obj_t *nt = s_nino ? icono(no, ICO_RELOJ, 2) : texto(no, "MÁS TARDE", C_BLANCO, 2, 0);
    lv_obj_center(nt);
}
static void huevo_fam_cb(lv_event_t *e) { (void)e; panel_familia(); }
static lv_obj_t *s_nido_huevo;
static bool s_pide_huevo;      /* puso un huevo: en cuanto no haya nada abierto, ofrece ir a explorar el mundo */
static bool s_viaje_con_huevo; /* la pregunta de viaje que está abierta es la del huevo */
static void panel_pide_viaje(void);
static lv_obj_t *s_casita_toque, *s_casa_salir;
static void casita_toque_cb(lv_event_t *e)
{
    (void)e;
    if (!s_pet || s_pet->etapa == STAGE_HUEVO || s_pet->dormida || s_panel != PANEL_NINGUNO || s_interior || s_escena != ESC_PRADO) return;
    if (!pet_has_decor(s_pet, DECOR_CASITA)) return;
    if (s_mascota.modo == M_CHARCO || s_pet->actividad == ACTV_CHAPOTEAR) return; /* chapoteando: no se teletransporta adentro; entra cuando termine de jugar */
    s_casa_visita = true;
    entrar_casa(s_pet);
    s_mascota.x = s_mascota.tx = VISITA_X; s_mascota.y = s_mascota.ty = VISITA_Y;
}
static void casa_salir_cb(lv_event_t *e)
{
    (void)e;
    if (!s_casa_visita) return;
    s_casa_visita = false;
    if (s_pet) salir_casa(s_pet);
}
/* adentro de la casita: apagar/encender la luz (para que duerma o despierte), la puerta como otra forma de salir y saltar en la cama */
static lv_obj_t *s_casa_luz_b, *s_casa_luz_i[2], *s_casa_puerta, *s_cama_toque;
static uint32_t s_cama_salto; /* hasta cuándo está saltando en la cama */
static void casa_luz_cb(lv_event_t *e)
{
    (void)e;
    accion(s_pet && s_pet->luz_apagada ? ACT_DESPERTAR : ACT_DORMIR, 0);
}
static void cama_cb(lv_event_t *e)
{
    (void)e;
    if (!s_pet || !s_casa_visita || !s_interior || s_pet->dormida || s_panel != PANEL_NINGUNO) return;
    if ((int32_t)(s_cama_salto - s_ms) > 0) { s_cama_salto = s_ms + 5500; return; } /* ya salta: sigue un rato más */
    s_cama_salto = s_ms + 5500;
    accion(UI_ACCION_SALTO_CAMA, 0);
    accion(UI_ACCION_SONAR, SND_ALEGRE);
}
void ui_test_casita(int modo)
{
    if (!s_pet || s_pet->etapa == STAGE_HUEVO) return;
    if (modo == 0) { if (s_casa_visita) { s_casa_visita = false; salir_casa(s_pet); } return; }
    if (!s_interior && s_escena == ESC_PRADO && pet_has_decor(s_pet, DECOR_CASITA) && !s_pet->dormida) {
        s_casa_visita = true;
        entrar_casa(s_pet);
        s_mascota.x = s_mascota.tx = VISITA_X; s_mascota.y = s_mascota.ty = VISITA_Y;
    }
    if (modo == 2 && s_casa_visita) s_cama_salto = s_ms + 5500;
}
static void actualizar_huevo_fam(const pet_t *p)
{
    if (!s_huevo_fam) {
        s_nido_huevo = lv_obj_create(s_mundo); /* un nidito de ramitas */
        lv_obj_remove_style_all(s_nido_huevo);
        lv_obj_set_size(s_nido_huevo, 64, 24);
        lv_obj_set_style_radius(s_nido_huevo, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(s_nido_huevo, lv_color_hex(0x8A5A2B), 0);
        lv_obj_set_style_bg_grad_color(s_nido_huevo, lv_color_hex(0x5A3A1A), 0);
        lv_obj_set_style_bg_grad_dir(s_nido_huevo, LV_GRAD_DIR_VER, 0);
        lv_obj_set_style_bg_opa(s_nido_huevo, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(s_nido_huevo, 2, 0);
        lv_obj_set_style_border_color(s_nido_huevo, lv_color_hex(0x3A230F), 0);
        lv_obj_remove_flag(s_nido_huevo, LV_OBJ_FLAG_CLICKABLE);
        s_huevo_fam = lv_image_create(s_mundo);
        lv_image_set_src(s_huevo_fam, art_icono(ICO_HUEVO, 3));
        lv_obj_add_flag(s_huevo_fam, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_ext_click_area(s_huevo_fam, 14);
        lv_obj_add_event_cb(s_huevo_fam, huevo_fam_cb, LV_EVENT_CLICKED, NULL);
        /* la casita se toca para entrar a verla por dentro */
        s_casita_toque = lv_obj_create(s_mundo);
        lv_obj_remove_style_all(s_casita_toque);
        lv_obj_set_size(s_casita_toque, 110, 96);
        lv_obj_set_pos(s_casita_toque, CASITA_CHIMENEA_X - 55, CASITA_CHIMENEA_Y + 6);
        lv_obj_add_flag(s_casita_toque, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(s_casita_toque, casita_toque_cb, LV_EVENT_CLICKED, NULL);
        /* y por dentro, un botón para salir */
        s_casa_salir = boton(s_mundo, 116, 42, casa_salir_cb, 0); /* un letrero de madera bajo la puerta redonda */
        lv_obj_set_pos(s_casa_salir, 20, 252);
        lv_obj_set_style_bg_color(s_casa_salir, lv_color_hex(0x9A6530), 0);
        lv_obj_set_style_border_color(s_casa_salir, lv_color_hex(0x4A2A10), 0);
        lv_obj_set_style_border_width(s_casa_salir, 3, 0);
        lv_obj_set_style_radius(s_casa_salir, 8, 0);
        lv_obj_t *st = s_nino ? icono(s_casa_salir, ICO_FLECHA, 2) : texto(s_casa_salir, "SALIR", 0xFFF1C2, 2, 0);
        lv_obj_center(st);
        ocultar(s_casa_salir, true);
        /* la luz (luna: apagar; sol: encender) */
        /* la luz: la linterna de papel que cuelga del techo; su chapita (luna/sol) se toca igual que toda la linterna */
        s_casa_luz_b = boton(s_mundo, 46, 46, casa_luz_cb, 0);
        lv_obj_set_pos(s_casa_luz_b, 193, 102);
        lv_obj_set_ext_click_area(s_casa_luz_b, 36);
        lv_obj_set_style_bg_color(s_casa_luz_b, lv_color_hex(0xFFE9B0), 0);
        lv_obj_set_style_border_color(s_casa_luz_b, lv_color_hex(0x6B4A22), 0);
        lv_obj_set_style_border_width(s_casa_luz_b, 3, 0);
        lv_obj_set_style_radius(s_casa_luz_b, LV_RADIUS_CIRCLE, 0);
        s_casa_luz_i[0] = icono(s_casa_luz_b, ICO_LUNA, 2); lv_obj_center(s_casa_luz_i[0]); lv_obj_set_style_image_opa(s_casa_luz_i[0], 150, 0);
        s_casa_luz_i[1] = icono(s_casa_luz_b, ICO_SOL, 2); lv_obj_center(s_casa_luz_i[1]); lv_obj_set_style_image_opa(s_casa_luz_i[1], 150, 0);
        ocultar(s_casa_luz_b, true);
        /* la puerta redonda de la casita también sirve para salir */
        s_casa_puerta = lv_obj_create(s_mundo);
        lv_obj_remove_style_all(s_casa_puerta);
        lv_obj_set_size(s_casa_puerta, 110, 104);
        lv_obj_set_pos(s_casa_puerta, 24, 142);
        lv_obj_add_flag(s_casa_puerta, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(s_casa_puerta, casa_salir_cb, LV_EVENT_CLICKED, NULL);
        lv_obj_add_flag(s_casa_puerta, LV_OBJ_FLAG_HIDDEN);
        /* la cama: tócala y salta */
        s_cama_toque = lv_obj_create(s_mundo);
        lv_obj_remove_style_all(s_cama_toque);
        lv_obj_set_size(s_cama_toque, 200, 86);
        lv_obj_set_pos(s_cama_toque, 196, 232);
        lv_obj_add_flag(s_cama_toque, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(s_cama_toque, cama_cb, LV_EVENT_CLICKED, NULL);
        lv_obj_add_flag(s_cama_toque, LV_OBJ_FLAG_HIDDEN);
    }
    const bool casita = pet_has_decor(p, DECOR_CASITA);
    const bool afuera = p->huevo_familia && !casita && s_escena == ESC_PRADO && !s_interior; /* sin casita, el nido queda junto a los arbustos */
    const bool adentro = p->huevo_familia && casita && s_interior;                           /* con casita, el nido está adentro */
    const bool ver = afuera || adentro;
    ocultar(s_huevo_fam, !ver);
    ocultar(s_nido_huevo, !ver);
    if (ver) {
        const int bx = adentro ? HUEVO_DENTRO_X : LXY(LUGAR_CASITA)[0] - 50, by = adentro ? HUEVO_DENTRO_Y : LXY(LUGAR_CASITA)[1] - 30;
        lv_obj_set_pos(s_nido_huevo, bx - 4, by + 34);
        lv_obj_set_pos(s_huevo_fam, bx, by + (int)(sinf((float)s_ms / 700.0f) * 2)); /* late despacito en su nido */
        lv_obj_move_foreground(s_nido_huevo);
        lv_obj_move_foreground(s_huevo_fam);
    }
    /* la casita se puede tocar (afuera, despierta y sin nada abierto); adentro, el botón de salir */
    const bool tocable = casita && s_escena == ESC_PRADO && !s_interior && s_panel == PANEL_NINGUNO && !p->dormida && p->etapa != STAGE_HUEVO;
    if (tocable) lv_obj_add_flag(s_casita_toque, LV_OBJ_FLAG_CLICKABLE); else lv_obj_remove_flag(s_casita_toque, LV_OBJ_FLAG_CLICKABLE);
    ocultar(s_casa_salir, true); /* sin letrero de SALIR: se sale tocando la puerta (s_casa_puerta) */
    ocultar(s_casa_puerta, !(s_casa_visita && s_interior && s_panel == PANEL_NINGUNO));
    ocultar(s_cama_toque, !(s_casa_visita && s_interior && s_panel == PANEL_NINGUNO && !p->dormida));
    {
        const bool luz_b = s_interior && s_panel == PANEL_NINGUNO && p->etapa != STAGE_HUEVO && (s_casa_visita || p->dormida);
        ocultar(s_casa_luz_b, !luz_b);
        ocultar(s_casa_luz_i[0], p->luz_apagada);  /* despierta: la luna apaga la luz */
        ocultar(s_casa_luz_i[1], !p->luz_apagada); /* dormida: el sol la enciende */
        if (luz_b) lv_obj_move_foreground(s_casa_luz_b);
    }
    if (s_pide_huevo && p->huevo_familia && s_panel == PANEL_NINGUNO && !p->dormida && s_escena == ESC_PRADO && !s_interior) {
        s_pide_huevo = false;
        s_viaje_con_huevo = true;
        panel_pide_viaje();
    } else if (s_pide_huevo && !p->huevo_familia) s_pide_huevo = false;
}

/* (pruebas) 0: la pregunta de viaje al poner el huevo, 1: tocar el huevo en el nido (la pregunta de eclosionar) */
void ui_demo_huevo(int f)
{
    if (!s_pet) return;
    ui_close_panel();
    if (f == 0) { s_viaje_con_huevo = true; panel_pide_viaje(); }
    else panel_familia();
}
static void viaje_pide_cb(lv_event_t *e)
{
    int q = (int)(intptr_t)lv_event_get_user_data(e);
    const bool con_huevo = s_viaje_con_huevo;
    s_viaje_con_huevo = false;
    cerrar_panel();
    if (q == 1 && s_pet) {
        static EXT_RAM_BSS_ATTR pet_t viajera;
        viajera = *s_pet;
        s_nombre[0] = 0;
        if (con_huevo && s_pet->huevo_familia) accion(UI_ACCION_FAMILIA, 1 | (elegir_vehiculo() + 1) << 8); /* se despide: el huevo de su familia empieza a ser la mascota */
        else accion(UI_ACCION_NUEVA, 1 | (elegir_vehiculo() + 1) << 8); /* se va: queda un huevo nuevo en su lugar */
        viaje_empezar(&viajera);
    } else if (con_huevo) {
        accion(UI_ACCION_VIAJE_NO, 0);
        globo(ICO_HUEVO, s_nino ? NULL : "¡ME QUEDO! CUIDARÉ EL HUEVITO EN MI CASITA", 3500);
    } else { accion(UI_ACCION_VIAJE_NO, 0); globo(ICO_CORAZON, s_nino ? NULL : "¡ME QUEDO CONTIGO!", 3000); }
}
static void panel_pide_viaje(void)
{
    if (!s_pet) return;
    abrir_tarjeta(PANEL_VIAJE_PIDE, 360, 380);
    lv_obj_t *m = mini_criatura(s_tarjeta, PET_MAX_AMIGOS + 1, pet_look(s_pet), s_pet->etapa, s_pet->forma, 4, EXP_FELIZ);
    lv_obj_align(m, LV_ALIGN_TOP_MID, 0, 18);
    char t[360];
    if (s_viaje_con_huevo)
        snprintf(t, sizeof t, "%s PUSO UN HUEVITO Y SIENTE GANAS DE EXPLORAR EL MUNDO MÁGICO. SI {LO} DEJAS IR, SE DESPIDE Y EL HUEVITO ECLOSIONA CONTIGO. SI DICES QUE TODAVÍA NO, LO GUARDARÁ CON MIMO EN UN NIDITO DENTRO DE SU CASITA. ENTRA A VERLO Y TÓCALO CUANDO QUIERAS QUE NAZCA.",
                 s_pet->nombre);
    else
    snprintf(t, sizeof t, "%s SIENTE GANAS DE EXPLORAR EL MUNDO MÁGICO. ¿{LO} DEJAS IR DE VIAJE? SI DICES QUE TODAVÍA NO, SE QUEDA FELIZ CONTIGO Y QUIZÁS TE VUELVA A PREGUNTAR EN UN MES.",
             s_pet->nombre);
    lv_obj_t *ar = area_texto(s_tarjeta, 30, 120, 300, 150);
    texto(ar, t, C_TINTA, 2, 290);
    area_lista(ar);
    lv_obj_t *si = boton(s_tarjeta, 150, 48, viaje_pide_cb, 1);
    mantener(si, viaje_pide_cb, 1);
    lv_obj_align(si, LV_ALIGN_BOTTOM_MID, -80, -24);
    lv_obj_set_style_bg_color(si, lv_color_hex(0x7A4458), 0);
    lv_obj_t *st = texto(si, "MANTÉN: SÍ", C_BLANCO, 2, 140); lv_obj_center(st);
    lv_obj_t *no = boton(s_tarjeta, 150, 48, viaje_pide_cb, 0);
    lv_obj_align(no, LV_ALIGN_BOTTOM_MID, 80, -24);
    lv_obj_set_style_bg_color(no, lv_color_hex(C_VERDE), 0);
    lv_obj_t *nt = texto(no, "TODAVÍA NO", C_BLANCO, 2, 0); lv_obj_center(nt);
}

/* ======================================================================
 * El paseo por el firmamento: la ballena beluga de nubes la lleva entre la Luna, planetas, un cometa y una luciérnaga dormida.
 * Cada astro que pasa y cada estrella que junta hace hablar a la ballena (infante: solo dibujos; niño: frases cortas; adulto: más).
 * Las estrellas juntadas quedan fijas, se unen con una línea y al final forman una constelación que la ballena nombra o cuenta
 * como un cuentito (al azar). Con 5 o más estrellas trae polvo de estrellas.
 * ==================================================================== */
#include "textos_cielo.h"
#define CIELO_MS 52000u
#define CIELO_FINAL_MS 40000u /* desde aquí no salen más estrellas y la ballena cierra el cuento */
#define CIELO_ESTRELLAS 5     /* estrellas vivas a la vez */
#define CIELO_MAX 7           /* las que se pueden juntar */
#define CIELO_ASTROS 6
static lv_obj_t *s_cie, *s_cie_fondo, *s_cie_ballena, *s_cie_cri, *s_cie_astro[CIELO_ASTROS], *s_cie_est[CIELO_ESTRELLAS], *s_cie_pista, *s_cie_pista_t;
static lv_obj_t *s_cie_fija[CIELO_MAX], *s_cie_nube[3], *s_cie_lin, *s_cie_msg, *s_cie_msg_t, *s_cie_msg_i;
static art_img_t s_cie_fondo_art, s_cie_cri_art[2], s_cie_ball_art[2], s_cie_astro_art[CIELO_ASTROS], s_cie_nube_art, s_cie_pista_art, s_cie_msg_art;
static lv_point_precise_t s_cie_pts[CIELO_MAX];
static bool s_cie_activo;
static uint32_t s_cie_desde, s_cie_est_desde[CIELO_ESTRELLAS], s_cie_msg_hasta;
static int s_cie_n, s_cie_fc = -1, s_cie_fb = -1, s_cie_dicho, s_cie_estilo;
static bool s_cie_final_dicho;
static float s_cie_est_x[CIELO_ESTRELLAS], s_cie_est_y[CIELO_ESTRELLAS];

static void cielo_fin(void)
{
    if (!s_cie_activo) return;
    s_cie_activo = false;
    ocultar(s_cie, true);
    lv_image_set_src(s_cie_fondo, NULL);
    lv_image_set_src(s_cie_cri, NULL);
    lv_image_set_src(s_cie_ballena, NULL);
    lv_image_set_src(s_cie_msg_t, NULL);
    for (int i = 0; i < CIELO_ASTROS; i++) { lv_image_set_src(s_cie_astro[i], NULL); art_img_free(&s_cie_astro_art[i]); }
    for (int i = 0; i < 3; i++) lv_image_set_src(s_cie_nube[i], NULL);
    art_img_free(&s_cie_fondo_art);
    for (int i = 0; i < 2; i++) { art_img_free(&s_cie_cri_art[i]); art_img_free(&s_cie_ball_art[i]); }
    art_img_free(&s_cie_nube_art);
    art_img_free(&s_cie_msg_art);
    accion(UI_ACCION_BALLENA, s_cie_n);
}
/* lo que dice la ballena: infante, un dibujo; niño y adulto, el texto (los largos van al centro, los cortos arriba) */
static void cielo_decir(int ico, const cielo_txt_t *tx, uint32_t ms, bool centro)
{
    if (s_nino || !tx) {
        lv_image_set_src(s_cie_msg_t, NULL);
        poner_src(s_cie_msg_i, art_icono((art_icono_t)ico, 4));
        ocultar(s_cie_msg_i, false);
        lv_obj_set_size(s_cie_msg, 96, 96);
        lv_obj_center(s_cie_msg_i);
    } else {
        const char *t = s_pet && s_pet->modo_nino == MODO_NINO ? tx->nino : tx->adulto;
        art_texto(&s_cie_msg_art, t, 0xFFFFFF, 2, centro ? 280 : 250); /* la pantalla es redonda: arriba las orillas se comen el texto ancho */
        cartel_poner(s_cie_msg, s_cie_msg_t, &s_cie_msg_art);
        ocultar(s_cie_msg_i, true);
    }
    lv_obj_align(s_cie_msg, LV_ALIGN_TOP_MID, 0, centro ? 66 : 58); /* siempre arriba: abajo quedan las estrellas y la constelación */
    ocultar(s_cie_msg, false);
    lv_obj_move_foreground(s_cie_msg);
    s_cie_msg_hasta = s_ms + ms;
}
static void cielo_estrella_cb(lv_event_t *e)
{
    int i = (int)(intptr_t)lv_event_get_user_data(e);
    if (!s_cie_activo || lv_obj_has_flag(s_cie_est[i], LV_OBJ_FLAG_HIDDEN) || s_cie_n >= CIELO_MAX) return;
    ocultar(s_cie_est[i], true);
    s_cie_est_desde[i] = s_ms + 600 + azar() % 1400; /* vuelve a aparecer en otro lado */
    /* la estrella queda fija en el cielo y se une a la anterior con una línea */
    const int k = s_cie_n++;
    s_cie_pts[k].x = (int32_t)s_cie_est_x[i];
    s_cie_pts[k].y = (int32_t)s_cie_est_y[i];
    lv_obj_set_pos(s_cie_fija[k], (int)s_cie_est_x[i] - 12, (int)s_cie_est_y[i] - 12);
    ocultar(s_cie_fija[k], false);
    if (k >= 1) { lv_line_set_points(s_cie_lin, s_cie_pts, (uint32_t)(k + 1)); ocultar(s_cie_lin, false); }
    accion(UI_ACCION_SONAR, SND_TESORO);
    cielo_decir(k % 2 ? ICO_CORAZON : ICO_ESTRELLA, &CIELO_ESTRELLA[k], 4200, false);
}
static void cielo_poner_estrella(int i)
{
    s_cie_est_x[i] = 60.0f + (float)(azar() % 346);
    s_cie_est_y[i] = 200.0f + (float)(azar() % 90);
    lv_obj_set_pos(s_cie_est[i], (int)s_cie_est_x[i] - 18, (int)s_cie_est_y[i] - 18);
    ocultar(s_cie_est[i], false);
    s_cie_est_desde[i] = s_ms;
}
static void crear_cielo(void)
{
    s_cie = caja(lv_layer_top());
    lv_obj_set_size(s_cie, W, W);
    lv_obj_add_flag(s_cie, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(s_cie, lv_color_hex(0x0A0A2A), 0);
    lv_obj_set_style_bg_opa(s_cie, LV_OPA_COVER, 0);
    s_cie_fondo = lv_image_create(s_cie);
    for (int i = 0; i < CIELO_ASTROS; i++) s_cie_astro[i] = lv_image_create(s_cie);
    s_cie_lin = lv_line_create(s_cie);
    lv_obj_set_style_line_width(s_cie_lin, 2, 0);
    lv_obj_set_style_line_color(s_cie_lin, lv_color_hex(0xFFF0A8), 0);
    lv_obj_set_style_line_opa(s_cie_lin, 190, 0);
    lv_obj_set_style_line_rounded(s_cie_lin, true, 0);
    for (int i = 0; i < CIELO_MAX; i++) {
        s_cie_fija[i] = lv_image_create(s_cie);
        lv_image_set_src(s_cie_fija[i], art_icono(ICO_ESTRELLA, 2));
    }
    s_cie_ballena = lv_image_create(s_cie);
    s_cie_cri = lv_image_create(s_cie);
    for (int i = 0; i < CIELO_ESTRELLAS; i++) {
        s_cie_est[i] = lv_image_create(s_cie);
        lv_image_set_src(s_cie_est[i], art_icono(ICO_ESTRELLA, 3));
        lv_obj_add_flag(s_cie_est[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_ext_click_area(s_cie_est[i], 14);
        lv_obj_add_event_cb(s_cie_est[i], cielo_estrella_cb, LV_EVENT_PRESSED, (void *)(intptr_t)i);
    }
    for (int i = 0; i < 3; i++) s_cie_nube[i] = lv_image_create(s_cie);
    s_cie_pista = cartel_de(s_cie, &s_cie_pista_t, 0x2A2238, LV_OPA_90);
    s_cie_msg = cartel_de(s_cie, &s_cie_msg_t, 0x1A1240, LV_OPA_80);
    s_cie_msg_i = lv_image_create(s_cie_msg);
    lv_obj_remove_flag(s_cie_msg, LV_OBJ_FLAG_CLICKABLE);
    ocultar(s_cie_msg, true);
    ocultar(s_cie, true);
}
static void cielo_empezar(const pet_t *m)
{
    cielo_fin();
    cerrar_panel();
    s_cie_activo = true;
    s_cie_desde = s_ms;
    s_cie_n = 0;
    s_cie_fc = s_cie_fb = -1;
    s_cie_dicho = 0;
    s_cie_final_dicho = false;
    s_cie_estilo = (int)(azar() & 1u); /* 0: la ballena nombra la constelación; 1: cuenta un cuentito */
    art_firmamento_fondo(&s_cie_fondo_art);
    poner_src(s_cie_fondo, &s_cie_fondo_art.dsc);
    lv_obj_set_pos(s_cie_fondo, -1, -1);
    static const int TIPO[CIELO_ASTROS] = {1, 0, 2, 3, 5, 6}, ESC_A[CIELO_ASTROS] = {4, 5, 3, 3, 4, 3};
    for (int i = 0; i < CIELO_ASTROS; i++) { art_astro(&s_cie_astro_art[i], TIPO[i], ESC_A[i]); poner_src(s_cie_astro[i], &s_cie_astro_art[i].dsc); ocultar(s_cie_astro[i], true); }
    art_astro(&s_cie_nube_art, 4, 4);
    for (int i = 0; i < 3; i++) poner_src(s_cie_nube[i], &s_cie_nube_art.dsc);
    for (int f = 0; f < 2; f++) {
        art_ballena(&s_cie_ball_art[f], f);
        art_pose_t po = {pet_look(m), m->etapa, m->etapa == STAGE_ADULTA ? m->forma : 0, 3, f ? EXP_FELIZ : EXP_CURIOSO, f == 1, f, false, false, 1, 1, 1, {0, 0, 0}, 0, m->guardiana};
        for (int r = 0; r < RANURA_COUNT; r++) po.ropa[r] = pet_worn_raw(m, r);
        art_criatura_pose(&s_cie_cri_art[f], &po);
    }
    for (int i = 0; i < CIELO_MAX; i++) ocultar(s_cie_fija[i], true);
    ocultar(s_cie_lin, true);
    ocultar(s_cie_msg, true);
    for (int i = 0; i < CIELO_ESTRELLAS; i++) { ocultar(s_cie_est[i], true); s_cie_est_desde[i] = s_ms + 3500 + (uint32_t)i * 900; }
    art_texto(&s_cie_pista_art, "TOCA LAS ESTRELLAS QUE BRILLAN", 0xFFFFFF, 2, 0);
    cartel_poner(s_cie_pista, s_cie_pista_t, &s_cie_pista_art);
    lv_obj_align(s_cie_pista, LV_ALIGN_TOP_MID, 0, 66); /* arriba: abajo la tapan las nubes y la ballena */
    ocultar(s_cie_pista, s_nino);
    ocultar(s_cie, false);
    lv_obj_move_foreground(s_cie);
    accion(UI_ACCION_SONAR, SND_SORPRESA);
}
/* el cierre: la ballena nombra la constelación o cuenta su cuentito */
static void cielo_cerrar_historia(void)
{
    s_cie_final_dicho = true;
    accion(UI_ACCION_SONAR, SND_SORPRESA);
    if (s_cie_n < 3) { cielo_decir(ICO_CORAZON, &CIELO_POCAS, 8000, true); return; }
    static const int ICO_C[5] = {ICO_HOJA, ICO_BARQUITO_HOJA, ICO_FARO, ICO_GLOBO_AIRE, ICO_ZETA};
    const int c = s_cie_n - 3;
    cielo_decir(ICO_C[c], s_cie_estilo ? &CIELO_CUENTO[c] : &CIELO_NOMBRAR[c], s_cie_estilo ? 10500 : 8500, true);
}
static void animar_cielo(void)
{
    if (!s_cie_activo) return;
    const uint32_t t = s_ms - s_cie_desde;
    /* subir: las nubes pasan hacia abajo al empezar; bajar: vuelven a subir al final */
    float sube = t < 3000 ? (float)t / 3000.0f : (t > CIELO_MS - 3000 ? (float)(CIELO_MS - t) / 3000.0f : 1.0f);
    for (int i = 0; i < 3; i++) {
        bool ver = sube < 1.0f;
        ocultar(s_cie_nube[i], !ver);
        if (ver) lv_obj_set_pos(s_cie_nube[i], -40 + i * 170, (int)(260 + (float)(i % 2) * 60 + sube * 300.0f));
    }
    /* la ballena y ella, meciéndose */
    float bx = 100 + sinf((float)t / 1800.0f) * 14, by = 300 + sinf((float)t / 900.0f) * 6 + (1.0f - sube) * 140.0f;
    lv_obj_set_pos(s_cie_ballena, (int)bx, (int)by);
    int fb = (int)(t / 700) % 2;
    if (fb != s_cie_fb) { s_cie_fb = fb; poner_src(s_cie_ballena, &s_cie_ball_art[fb].dsc); }
    int fc = (int)(t / 500) % 2;
    if (fc != s_cie_fc) { s_cie_fc = fc; poner_src(s_cie_cri, &s_cie_cri_art[fc].dsc); }
    int cw = (int)s_cie_cri_art[0].dsc.header.w, ch = (int)s_cie_cri_art[0].dsc.header.h;
    lv_obj_set_pos(s_cie_cri, (int)bx + ART_BALLENA_ASIENTO_X - cw / 2, (int)by + ART_BALLENA_ASIENTO_Y - ch + 6);
    /* lo que pasa por el cielo, de derecha a izquierda; a media pantalla la ballena lo comenta */
    static const uint32_t CUANDO[CIELO_ASTROS] = {3500, 9500, 16000, 21500, 27500, 33500}, DURA[CIELO_ASTROS] = {9000, 9000, 6500, 8000, 9000, 6500};
    static const int16_t ALTO[CIELO_ASTROS] = {60, 50, 90, 140, 40, 150};
    static const int ICO_A[CIELO_ASTROS] = {ICO_LUNA, ICO_ESTRELLA, ICO_COMETA, ICO_CALOR, ICO_RAYO, ICO_ESTRELLA};
    for (int i = 0; i < CIELO_ASTROS; i++) {
        bool ver = t >= CUANDO[i] && t < CUANDO[i] + DURA[i];
        ocultar(s_cie_astro[i], !ver);
        if (!ver) continue;
        float k = (float)(t - CUANDO[i]) / (float)DURA[i];
        int w = (int)s_cie_astro_art[i].dsc.header.w;
        lv_obj_set_pos(s_cie_astro[i], (int)((float)W + 10 - k * (float)(W + w + 20)), ALTO[i] + (i == 2 ? (int)(k * 60) : 0));
        if (k > 0.4f && !((s_cie_dicho >> i) & 1) && t < CIELO_FINAL_MS + 4000 && (int32_t)(s_ms - s_cie_msg_hasta) >= 0) {
            s_cie_dicho |= 1 << i;
            cielo_decir(ICO_A[i], &CIELO_ASTRO[i], 6500, false);
        }
    }
    /* las estrellas que se pueden tocar: aparecen, titilan un rato y se van */
    for (int i = 0; i < CIELO_ESTRELLAS; i++) {
        bool vis = !lv_obj_has_flag(s_cie_est[i], LV_OBJ_FLAG_HIDDEN);
        const bool hay = t < CIELO_FINAL_MS && s_cie_n < CIELO_MAX;
        if (!vis && hay && (int32_t)(s_ms - s_cie_est_desde[i]) >= 0) cielo_poner_estrella(i);
        else if (vis) {
            uint32_t vida = s_ms - s_cie_est_desde[i];
            if (vida > 2600 || !hay) { ocultar(s_cie_est[i], true); s_cie_est_desde[i] = s_ms + 400 + azar() % 1200; }
            else lv_image_set_scale(s_cie_est[i], (uint32_t)(256 + (int)(sinf((float)vida / 120.0f) * 50)));
        }
    }
    /* las estrellas juntadas titilan despacio; en el cierre brillan todas juntas */
    for (int k = 0; k < s_cie_n; k++)
        lv_image_set_scale(s_cie_fija[k], (uint32_t)(256 + (int)(sinf((float)t / (t > CIELO_FINAL_MS ? 200.0f : 500.0f) + (float)k) * (t > CIELO_FINAL_MS ? 60 : 24))));
    if (t >= CIELO_FINAL_MS + 1500 && !s_cie_final_dicho) cielo_cerrar_historia();
    if (!lv_obj_has_flag(s_cie_msg, LV_OBJ_FLAG_HIDDEN) && (int32_t)(s_ms - s_cie_msg_hasta) >= 0) ocultar(s_cie_msg, true);
    lv_obj_set_style_opa(s_cie_pista, t < 10000 ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    if (t >= CIELO_MS) cielo_fin();
}

/* ======================================================================
 * Alarmas del dueño (hasta PET_ALARMAS) y hora de dormir
 * ==================================================================== */
static alarma_t s_al_vista[PET_ALARMAS]; /* lo que muestra la pantalla (el motor lo recibe por una acción) */
static hora_dormir_t s_dor_vista;
static int s_al_ed = 0;                   /* la que se está editando */
static alarma_t s_al_edit;

static void dias_texto(uint8_t d, char *t, int n)
{
    d &= ALARMA_TODOS;
    if (!d) snprintf(t, (size_t)n, "SOLO UNA VEZ");
    else if (d == ALARMA_TODOS) snprintf(t, (size_t)n, "TODOS LOS DÍAS");
    else if (d == ALARMA_LUN_VIE) snprintf(t, (size_t)n, "DE LUNES A VIERNES");
    else if (d == ALARMA_FINDE) snprintf(t, (size_t)n, "SÁBADO Y DOMINGO");
    else {
        int k = 0;
        t[0] = 0;
        for (int i = 0; i < 7; i++)
            if ((d >> i) & 1u) k += snprintf(t + k, (size_t)(n - k), "%s%s", k ? " " : "", DIA_CORTO[i]);
    }
}

/* "SUENA MAÑANA A LAS 07:00" (con la vista de la pantalla, no la del motor) */
static uint32_t proxima_vista(int i, const alarma_t *a)
{
    static EXT_RAM_BSS_ATTR pet_t tmp;
    if (!s_pet || !a->activa) return 0;
    tmp = *s_pet;
    pet_alarm_set(&tmp, i, true, a->hora, a->minuto, a->dias, a->sonido, a->siempre);
    tmp.alarma_sono[i] = 0;
    return pet_alarm_next(&tmp, i, s_ultimo_t);
}
static void cuando_texto(int i, const alarma_t *a, char *t, int n)
{
    uint32_t prox = proxima_vista(i, a);
    if (!prox) { snprintf(t, (size_t)n, "APAGADA"); return; }
    int64_t d = ui_dia_local(prox) - ui_dia_local(s_ultimo_t);
    char cuando[24];
    if (d == 0) snprintf(cuando, sizeof cuando, "HOY");
    else if (d == 1) snprintf(cuando, sizeof cuando, "MAÑANA");
    else snprintf(cuando, sizeof cuando, "EL %s", DIA_NOMBRE[ui_dia_semana(ui_dia_local(prox))]);
    snprintf(t, (size_t)n, "SUENA %s A LAS %02d:%02d", cuando, a->hora, a->minuto);
}

static void alarma_mandar(int i)
{
    const alarma_t *a = &s_al_vista[i];
    if (!a->usada) accion(UI_ACCION_ALARMA, i | PET_ALARMA_BORRAR);
    else accion(UI_ACCION_ALARMA, pet_alarm_pack(i, a->activa, a->hora, a->minuto, a->dias, a->sonido, a->siempre));
}

static void panel_alarma(void);
static void panel_hora_menu(void);
static void alarmas_cb(lv_event_t *e)
{
    int q = (int)(intptr_t)lv_event_get_user_data(e);
    if (q >= 0 && q < PET_ALARMAS) { s_al_ed = q; s_al_edit = s_al_vista[q]; panel_alarma(); return; }
    if (q >= 10 && q < 10 + PET_ALARMAS) { /* prender o apagar */
        alarma_t *a = &s_al_vista[q - 10];
        a->activa = !a->activa;
        alarma_mandar(q - 10);
        panel_alarmas();
        return;
    }
    if (q == 20) { /* nueva: en el primer lugar libre, a una hora razonable */
        for (int i = 0; i < PET_ALARMAS; i++)
            if (!s_al_vista[i].usada) {
                s_al_ed = i;
                alarma_t n = {1, 7, 0, ALARMA_LUN_VIE, 0, 0, 1, 0};
                s_al_edit = n;
                panel_alarma();
                return;
            }
    }
    if (q == 21) panel_hora_menu();
}

static void panel_alarmas(void)
{
    if (s_pet && s_panel != PANEL_ALARMAS && s_panel != PANEL_ALARMA) memcpy(s_al_vista, s_pet->alarma, sizeof s_al_vista);
    abrir_tarjeta(PANEL_ALARMAS, 420, 420);
    if (!s_pet) return;
    volver(alarmas_cb, 21);
    if (s_nino) { lv_obj_t *ic = icono(s_tarjeta, ICO_DESPERTADOR, 3); lv_obj_align(ic, LV_ALIGN_TOP_MID, 0, 22); }
    else { lv_obj_t *ti = texto(s_tarjeta, "ALARMAS", C_TINTA, 3, 0); lv_obj_align(ti, LV_ALIGN_TOP_MID, 0, 34); }
    char t[64];
    int idx[PET_ALARMAS], n = 0;
    for (int i = 0; i < PET_ALARMAS; i++) if (s_al_vista[i].usada) idx[n++] = i;
    const bool hay_mas = n < PET_ALARMAS;
    const int total = n + (hay_mas ? 1 : 0);
    /* mosaicos de dos en dos: la hora grande, los días como puntitos y el botón de prender y apagar; el último, "+" para otra */
    for (int m = 0; m < total; m++) {
        const bool es_mas = m >= n;
        const int fila = m / 2, enfila = (total - fila * 2) >= 2 ? 2 : 1;
        const int dx = enfila == 1 ? 0 : (m % 2 ? 1 : -1) * 78;
        const int y = 80 + fila * 90;
        if (es_mas) {
            const bool sola = n == 0;
            lv_obj_t *b = boton(s_tarjeta, sola ? 280 : 150, sola ? 110 : 84, alarmas_cb, 20);
            lv_obj_align(b, LV_ALIGN_TOP_MID, dx, sola ? 120 : y);
            lv_obj_set_style_radius(b, 24, 0);
            lv_obj_set_style_bg_color(b, lv_color_hex(C_VIOLETA), 0);
            lv_obj_t *mi = icono(b, ICO_DESPERTADOR, sola ? 5 : 4);
            if (s_nino) lv_obj_center(mi);
            else {
                lv_obj_align(mi, LV_ALIGN_LEFT_MID, 12, 0);
                lv_obj_t *bt = texto(b, sola ? "NUEVA ALARMA" : "OTRA", C_BLANCO, 2, sola ? 150 : 70);
                lv_obj_align(bt, LV_ALIGN_LEFT_MID, sola ? 100 : 66, 0);
            }
            continue;
        }
        const int i = idx[m];
        const alarma_t *a = &s_al_vista[i];
        lv_obj_t *b = boton(s_tarjeta, 150, 84, alarmas_cb, i);
        lv_obj_align(b, LV_ALIGN_TOP_MID, dx, y);
        lv_obj_set_style_radius(b, 24, 0);
        if (!a->activa) lv_obj_set_style_bg_color(b, lv_color_hex(0x4A4E60), 0);
        snprintf(t, sizeof t, "%02d:%02d", a->hora, a->minuto);
        lv_obj_t *h = texto(b, t, a->activa ? C_BLANCO : 0xB8BCCB, 3, 0);
        lv_obj_align(h, LV_ALIGN_TOP_LEFT, 10, 10);
        for (int d = 0; d < 7; d++) { /* los días en que suena */
            lv_obj_t *p = caja(b);
            lv_obj_set_size(p, 12, 12);
            lv_obj_set_style_radius(p, LV_RADIUS_CIRCLE, 0);
            lv_obj_set_style_bg_opa(p, LV_OPA_COVER, 0);
            lv_obj_set_style_bg_color(p, lv_color_hex((a->dias >> d) & 1u ? (a->activa ? 0xC9B0FF : 0x9A9EB4) : 0x2A2E40), 0);
            lv_obj_remove_flag(p, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_align(p, LV_ALIGN_BOTTOM_LEFT, 12 + d * 18, -10);
        }
        lv_obj_t *sw = boton(b, 38, 38, alarmas_cb, 10 + i);
        lv_obj_set_style_radius(sw, LV_RADIUS_CIRCLE, 0);
        lv_obj_align(sw, LV_ALIGN_TOP_RIGHT, -6, 6);
        lv_obj_set_style_bg_color(sw, lv_color_hex(a->activa ? C_VERDE : 0x6A6E80), 0);
        lv_obj_t *st = icono(sw, a->activa ? ICO_SI : ICO_NO, 2);
        lv_obj_center(st);
    }
    if (!s_nino) {
        const char *pie = s_hora_dudosa ? "PERDÍ LA HORA: LAS ALARMAS ESPERAN" : NULL;
        uint32_t pt = 0;
        int prox = -1;
        if (!pie)
            for (int i = 0; i < PET_ALARMAS; i++) {
                uint32_t x = s_al_vista[i].usada ? proxima_vista(i, &s_al_vista[i]) : 0;
                if (x && (!pt || x < pt)) { pt = x; prox = i; }
            }
        if (prox >= 0) cuando_texto(prox, &s_al_vista[prox], t, sizeof t);
        if (pie || prox >= 0) {
            lv_obj_t *pl = texto(s_tarjeta, pie ? pie : t, pie ? C_ROJO : C_GRIS, 2, 330);
            lv_obj_align(pl, LV_ALIGN_TOP_MID, 0, 80 + ((total + 1) / 2) * 90 + 2);
        }
    }
}

static lv_obj_t *s_ae_hora[2], *s_ae_cuando, *s_ae_dia[7], *s_ae_son, *s_ae_siempre;
static art_img_t s_ae_hora_art[2], s_ae_cuando_art, s_ae_son_art;

static void alarma_ed_refrescar(void)
{
    char t[200];
    for (int c = 0; c < 2; c++) {
        if (!s_ae_hora[c]) continue;
        snprintf(t, sizeof t, "%02d", c ? s_al_edit.minuto : s_al_edit.hora);
        art_texto(&s_ae_hora_art[c], t, s_portal ? 0xFFF3C8 : C_TINTA, 6, 0);
        poner_src(s_ae_hora[c], &s_ae_hora_art[c].dsc);
        lv_obj_update_layout(s_ae_hora[c]);
        lv_obj_align(s_ae_hora[c], LV_ALIGN_TOP_MID, c ? 76 : -76, 136 + (48 - lv_obj_get_height(s_ae_hora[c])) / 2);
    }
    for (int d = 0; d < 7; d++)
        if (s_ae_dia[d]) lv_obj_set_style_bg_color(s_ae_dia[d], lv_color_hex((s_al_edit.dias >> d) & 1u ? C_VIOLETA : 0x9A9EAE), 0);
    if (s_ae_siempre) lv_obj_set_style_bg_color(s_ae_siempre, lv_color_hex(s_al_edit.siempre ? C_VERDE : 0x9A9EAE), 0);
    if (!s_nino) {
        if (s_ae_son) {
            snprintf(t, sizeof t, "%s", pet_alarm_sound_name(s_al_edit.sonido));
            for (char *c = t; *c; c++) if (*c >= 'a' && *c <= 'z') *c = (char)(*c - 32);
            art_texto(&s_ae_son_art, t, C_BLANCO, 2, 0);
            poner_src(s_ae_son, &s_ae_son_art.dsc);
            lv_obj_align(s_ae_son, LV_ALIGN_CENTER, 12, 0);
        }
        alarma_t a = s_al_edit;
        a.activa = 1;
        char c2[64], d2[40];
        cuando_texto(s_al_ed, &a, c2, sizeof c2);
        d2[0] = 0;
        (void)d2;
        snprintf(t, sizeof t, "%s", s_al_edit.siempre ? "SUENA AUNQUE ESTÉ OTRA MASCOTA" : c2);
        art_texto(&s_ae_cuando_art, t, s_portal ? 0xC8C4E0 : C_GRIS, 2, 340);
        poner_src(s_ae_cuando, &s_ae_cuando_art.dsc);
        lv_obj_align(s_ae_cuando, LV_ALIGN_TOP_MID, 0, 244);
    }
}

static void alarma_ed_cb(lv_event_t *e)
{
    int k = (int)(intptr_t)lv_event_get_user_data(e);
    if (k >= 0 && k < 4) { /* flechas: hora y minutos */
        int d = (k % 2) ? -1 : 1;
        if (k < 2) s_al_edit.hora = (uint8_t)((s_al_edit.hora + d + 24) % 24);
        else {
            bool rapido = lv_event_get_code(e) == LV_EVENT_LONG_PRESSED_REPEAT; /* dejándolo apretado avanza de a 5 */
            int m = s_al_edit.minuto + d * (rapido ? 5 : 1);
            if (rapido) m = (m / 5) * 5;
            s_al_edit.minuto = (uint8_t)((m + 60) % 60);
        }
    } else if (k >= 10 && k < 17) s_al_edit.dias ^= (uint8_t)(1u << (k - 10));
    else if (k == 20) s_al_edit.dias = ALARMA_LUN_VIE;
    else if (k == 21) s_al_edit.dias = ALARMA_TODOS;
    else if (k == 22) s_al_edit.dias = 0;
    else if (k == 26) s_al_edit.siempre = !s_al_edit.siempre;
    else if (k == 25) { /* otro sonido: se oye al elegirlo */
        s_al_edit.sonido = (uint8_t)((s_al_edit.sonido + 1) % ALARMA_SONIDOS);
        accion(UI_ACCION_SONAR, pet_alarm_sound(s_al_edit.sonido));
    } else if (k == 30) { /* listo: queda prendida */
        s_al_edit.activa = 1;
        s_al_edit.usada = 1;
        s_al_vista[s_al_ed] = s_al_edit;
        alarma_mandar(s_al_ed);
        panel_alarmas();
        return;
    } else if (k == 31) { panel_alarmas(); return; }
    else if (k == 32) { /* borrar */
        memset(&s_al_vista[s_al_ed], 0, sizeof s_al_vista[s_al_ed]);
        alarma_mandar(s_al_ed);
        panel_alarmas();
        return;
    }
    alarma_ed_refrescar();
}

/* flechas arriba y abajo de una columna de números (horas, minutos...) */
static void flechas(lv_obj_t *padre, lv_event_cb_t cb, int id_arriba, int x, int y_arriba, int y_abajo)
{
    for (int k = 0; k < 2; k++) {
        int id = id_arriba + k;
        lv_obj_t *b = boton(padre, 84, 38, cb, id);
        lv_obj_remove_event_cb(b, cb);
        lv_obj_add_event_cb(b, cb, LV_EVENT_SHORT_CLICKED, (void *)(intptr_t)id);
        lv_obj_add_event_cb(b, cb, LV_EVENT_LONG_PRESSED_REPEAT, (void *)(intptr_t)id);
        lv_obj_align(b, LV_ALIGN_TOP_MID, x, k == 0 ? y_arriba : y_abajo);
        lv_obj_t *fl = icono(b, ICO_FLECHA, 2);
        lv_image_set_pivot(fl, 12, 12);
        lv_image_set_rotation(fl, k == 0 ? 2700 : 900);
        lv_obj_center(fl);
    }
}

static void boton_volver(lv_event_cb_t cb, int id, int x, int y_abajo)
{
    volver(cb, id);
}

/* una flecha redonda a un lado de la hora o de los minutos (apretada, repite) */
static void flecha_lado(lv_obj_t *padre, lv_event_cb_t cb, int id, int x, int y, bool derecha)
{
    lv_obj_t *b = boton(padre, 54, 54, cb, id);
    lv_obj_remove_event_cb(b, cb);
    lv_obj_add_event_cb(b, cb, LV_EVENT_SHORT_CLICKED, (void *)(intptr_t)id);
    lv_obj_add_event_cb(b, cb, LV_EVENT_LONG_PRESSED_REPEAT, (void *)(intptr_t)id);
    lv_obj_align(b, LV_ALIGN_TOP_MID, x, y);
    lv_obj_t *fl = icono(b, ICO_FLECHA, 3);
    lv_obj_update_layout(fl);
    lv_image_set_pivot(fl, lv_obj_get_width(fl) / 2, lv_obj_get_height(fl) / 2); /* (v99) gira sobre su centro: antes la flecha izquierda quedaba corrida */
    lv_image_set_rotation(fl, derecha ? 0 : 1800);
    lv_obj_center(fl);
}

static void panel_alarma(void)
{
    if (s_panel != PANEL_ALARMA) s_pag = 0;
    abrir_tarjeta(PANEL_ALARMA, 420, 420);
    for (int c = 0; c < 2; c++) s_ae_hora[c] = NULL;
    for (int d = 0; d < 7; d++) s_ae_dia[d] = NULL;
    s_ae_siempre = NULL; s_ae_son = NULL;
    lv_obj_t *ic = icono(s_tarjeta, ICO_DESPERTADOR, 3);
    lv_obj_align(ic, LV_ALIGN_TOP_MID, 0, 18);
    if (s_pag == 0) { /* (v99) página 1: la hora, grande, con sus flechas arriba y abajo (bien centradas, dentro del círculo) */
        for (int c = 0; c < 2; c++) {
            flechas(s_tarjeta, alarma_ed_cb, c * 2, c ? 76 : -76, 86, 196);
            s_ae_hora[c] = lv_image_create(s_tarjeta);
        }
        lv_obj_t *dp = texto(s_tarjeta, ":", C_TINTA, 6, 0);
        lv_obj_update_layout(dp);
        lv_obj_align(dp, LV_ALIGN_TOP_MID, 0, 136 + (48 - lv_obj_get_height(dp)) / 2);
    } else { /* página 2: qué días y con qué sonido */
        for (int d = 0; d < 7; d++) { /* los días: tocar para elegir */
            lv_obj_t *b = boton(s_tarjeta, 50, 50, alarma_ed_cb, 10 + d);
            lv_obj_set_style_radius(b, LV_RADIUS_CIRCLE, 0);
            lv_obj_set_style_border_width(b, 2, 0);
            lv_obj_align(b, LV_ALIGN_TOP_MID, (d - 3) * 54, 70);
            lv_obj_t *l = texto(b, DIA_CORTO[d], C_BLANCO, 2, 0);
            lv_obj_center(l);
            s_ae_dia[d] = b;
        }
        static const char *const PRE[3] = {"LUN-VIE", "TODOS", "UNA VEZ"};
        for (int k = 0; k < 3; k++) {
            lv_obj_t *b = boton(s_tarjeta, 120, 44, alarma_ed_cb, 20 + k);
            lv_obj_set_style_radius(b, 14, 0);
            lv_obj_set_style_border_width(b, 2, 0);
            lv_obj_align(b, LV_ALIGN_TOP_MID, (k - 1) * 126, 130);
            lv_obj_t *l = s_nino ? icono(b, k == 2 ? ICO_ESTRELLA : ICO_CALENDARIO, 2) : texto(b, PRE[k], C_BLANCO, 2, 0);
            lv_obj_center(l);
        }
        lv_obj_t *sb = boton(s_tarjeta, 250, 48, alarma_ed_cb, 25); /* sonido */
        lv_obj_set_style_radius(sb, 16, 0);
        lv_obj_set_style_border_width(sb, 2, 0);
        lv_obj_set_style_bg_color(sb, lv_color_hex(C_VIOLETA), 0);
        lv_obj_align(sb, LV_ALIGN_TOP_MID, -38, 186);
        s_ae_siempre = boton(s_tarjeta, 66, 48, alarma_ed_cb, 26); /* suena siempre, aunque esté otra mascota */
        lv_obj_set_style_radius(s_ae_siempre, 16, 0);
        lv_obj_set_style_border_width(s_ae_siempre, 2, 0);
        lv_obj_align(s_ae_siempre, LV_ALIGN_TOP_MID, 128, 186);
        lv_obj_t *sic = icono(s_ae_siempre, ICO_AMIGOS, 2); lv_obj_center(sic);
        if (s_nino) { lv_obj_t *n = icono(sb, ICO_NOTA, 2); lv_obj_center(n); s_ae_son = lv_image_create(sb); }
        else { lv_obj_t *n = icono(sb, ICO_NOTA, 2); lv_obj_align(n, LV_ALIGN_LEFT_MID, 10, 0); s_ae_son = lv_image_create(sb); }
    }
    s_ae_cuando = lv_image_create(s_tarjeta);
    paginas(2, panel_alarma, 272);
    boton_volver(alarma_ed_cb, 31, -128, -20);
    lv_obj_t *ok = boton(s_tarjeta, 160, 46, alarma_ed_cb, 30);
    lv_obj_align(ok, LV_ALIGN_TOP_MID, 0, 322);
    lv_obj_set_style_bg_color(ok, lv_color_hex(C_VERDE), 0);
    if (s_al_vista[s_al_ed].usada) { /* borrarla: abajo, a la derecha de la X */
        lv_obj_t *bb = boton(s_tarjeta, 46, 42, alarma_ed_cb, 32);
        abajo_derecha(bb);
        lv_obj_set_style_radius(bb, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(bb, lv_color_hex(0x7A4458), 0);
        lv_obj_t *bt = icono(bb, ICO_NO, 2);
        lv_obj_center(bt);
    }
    lv_obj_t *ot = s_nino ? icono(ok, ICO_SI, 3) : texto(ok, "ACEPTAR", C_BLANCO, 3, 0);
    lv_obj_center(ot);
    alarma_ed_refrescar();
}

/* --- hora de dormir: me acuesto y me levanto (de a 15 minutos) --- */
static lv_obj_t *s_dor_val[2];
static art_img_t s_dor_art[2];

#define DOR_Y0 90  /* "me acuesto" (y en la tarjeta) */
#define DOR_Y1 158 /* "me levanto" */
static void dormir_refrescar(void)
{
    char t[16];
    for (int q = 0; q < 2; q++) {
        int h = q ? s_dor_vista.despertar_h : s_dor_vista.acostarse_h, m = q ? s_dor_vista.despertar_m : s_dor_vista.acostarse_m;
        snprintf(t, sizeof t, "%02d:%02d", h, m);
        art_texto(&s_dor_art[q], t, s_dor_vista.modo ? C_TINTA : 0xA8A4B8, 4, 0);
        poner_src(s_dor_val[q], &s_dor_art[q].dsc);
        lv_obj_align(s_dor_val[q], LV_ALIGN_TOP_MID, 0, q ? DOR_Y1 + 22 : DOR_Y0 + 22);
    }
}

static void panel_dormir(void);
static void dormir_cb(lv_event_t *e)
{
    int k = (int)(intptr_t)lv_event_get_user_data(e);
    if (k >= 0 && k < 4) { /* 0/1 acostarse +-, 2/3 levantarse +- (de a 15 minutos) */
        int q = k / 2, d = (k % 2) ? -15 : 15;
        uint8_t *h = q ? &s_dor_vista.despertar_h : &s_dor_vista.acostarse_h, *m = q ? &s_dor_vista.despertar_m : &s_dor_vista.acostarse_m;
        int min = ((*h * 60 + (*m / 15) * 15 + d) % 1440 + 1440) % 1440;
        *h = (uint8_t)(min / 60);
        *m = (uint8_t)(min % 60);
        if (!s_dor_vista.modo) s_dor_vista.modo = DORMIR_FIJA; /* tocar la hora la activa */
        panel_dormir();
        return;
    }
    if (k == 10) { s_dor_vista.modo = s_dor_vista.modo ? DORMIR_NO : DORMIR_FIJA; panel_dormir(); return; }
    if (k == 11) { s_dor_vista.modo = s_dor_vista.modo == DORMIR_ADAPTARSE ? DORMIR_FIJA : DORMIR_ADAPTARSE; panel_dormir(); return; }
    if (k == 30) {
        const hora_dormir_t *d = &s_dor_vista;
        accion(UI_ACCION_DORMIR, pet_bedtime_pack(d->modo, d->acostarse_h, d->acostarse_m, d->despertar_h, d->despertar_m));
        panel_hora_menu();
        return;
    }
    if (k == 31) panel_hora_menu();
}

static void panel_dormir(void)
{
    if (s_pet && s_panel != PANEL_DORMIR) {
        s_dor_vista = s_pet->dormir;
        if (!s_dor_vista.acostarse_h && !s_dor_vista.acostarse_m && !s_dor_vista.despertar_h && !s_dor_vista.despertar_m) {
            s_dor_vista.acostarse_h = 22; s_dor_vista.acostarse_m = 30; s_dor_vista.despertar_h = 7; s_dor_vista.despertar_m = 0;
        }
    }
    /* todo dentro del círculo: el título arriba (angosto), el sí/no debajo, y ¡listo! sobre la X */
    abrir_tarjeta(PANEL_DORMIR, 360, 370);
    lv_obj_t *ic = icono(s_tarjeta, ICO_LUNA, 2);
    lv_obj_align(ic, LV_ALIGN_TOP_MID, s_nino ? 0 : -86, 14);
    if (!s_nino) { lv_obj_t *ti = texto(s_tarjeta, "HORA DE DORMIR", C_TINTA, 2, 0); lv_obj_align(ti, LV_ALIGN_TOP_MID, 14, 18); }
    lv_obj_t *sw = boton(s_tarjeta, 84, 34, dormir_cb, 10);
    lv_obj_align(sw, LV_ALIGN_TOP_MID, 0, 46);
    lv_obj_set_style_bg_color(sw, lv_color_hex(s_dor_vista.modo ? C_VERDE : 0x6A6E80), 0);
    lv_obj_t *st = s_nino ? icono(sw, s_dor_vista.modo ? ICO_SI : ICO_NO, 2) : texto(sw, s_dor_vista.modo ? "SÍ" : "NO", C_BLANCO, 2, 0);
    lv_obj_center(st);
    static const char *const ROT[2] = {"ME ACUESTO", "ME LEVANTO"};
    static const art_icono_t RI[2] = {ICO_LUNA, ICO_SOL};
    for (int q = 0; q < 2; q++) {
        int y0 = q ? DOR_Y1 : DOR_Y0;
        lv_obj_t *r = s_nino ? icono(s_tarjeta, RI[q], 2) : texto(s_tarjeta, ROT[q], C_GRIS, 2, 0);
        lv_obj_align(r, LV_ALIGN_TOP_MID, 0, y0);
        for (int k = 0; k < 2; k++) {
            int id = q * 2 + k;
            lv_obj_t *b = boton(s_tarjeta, 54, 44, dormir_cb, id);
            lv_obj_remove_event_cb(b, dormir_cb);
            lv_obj_add_event_cb(b, dormir_cb, LV_EVENT_SHORT_CLICKED, (void *)(intptr_t)id);
            lv_obj_add_event_cb(b, dormir_cb, LV_EVENT_LONG_PRESSED_REPEAT, (void *)(intptr_t)id);
            lv_obj_align(b, LV_ALIGN_TOP_MID, k ? -112 : 112, y0 + 18);
            lv_obj_t *fl = icono(b, ICO_FLECHA, 2);
            lv_image_set_pivot(fl, 12, 12);
            if (k) lv_image_set_rotation(fl, 1800);
            lv_obj_center(fl);
        }
        s_dor_val[q] = lv_image_create(s_tarjeta);
    }
    { /* se adapta a ti: la hora se corre hacia cuando de verdad apagas la luz y la saludas en la mañana */
        lv_obj_t *ad = boton(s_tarjeta, 250, 34, dormir_cb, 11);
        lv_obj_align(ad, LV_ALIGN_TOP_MID, 0, 228);
        bool si = s_dor_vista.modo == DORMIR_ADAPTARSE;
        lv_obj_set_style_bg_color(ad, lv_color_hex(si ? C_VIOLETA : 0x6A6E80), 0);
        lv_obj_t *at = s_nino ? icono(ad, si ? ICO_SI : ICO_NO, 2) : texto(ad, si ? "APRENDE TUS HORARIOS" : "HORA FIJA", C_BLANCO, 2, 240);
        lv_obj_center(at);
    }
    if (!s_nino) {
        /* (v97) en dos renglones: con tres, el último quedaba debajo de «ACEPTAR» */
        lv_obj_t *ex = texto_c(s_tarjeta, s_dor_vista.modo == DORMIR_ADAPTARSE ? "SE ACOMODA DE A POCO A CUANDO APAGAS LA LUZ"
                                                                                 : "DUERMEN JUNTOS: PANTALLA DE NOCHE, SIN RUIDO", C_GRIS, 2, 310);
        lv_obj_align(ex, LV_ALIGN_TOP_MID, 0, 264);
    }
    volver(dormir_cb, 31);
    lv_obj_t *ok = boton(s_tarjeta, 140, 42, dormir_cb, 30);
    lv_obj_align(ok, LV_ALIGN_TOP_MID, 0, 310);
    lv_obj_set_style_bg_color(ok, lv_color_hex(C_VERDE), 0);
    lv_obj_t *ot = s_nino ? icono(ok, ICO_SI, 3) : texto(ok, "ACEPTAR", C_BLANCO, 2, 0);
    lv_obj_center(ot);
    dormir_refrescar();
}

/* --- la alarma sonando (o la hora de dormir): pantalla completa con la mascota --- */
enum { AL_ALARMA = 0, AL_DORMIR, AL_RESERVA, AL_TIEMPO };
static lv_obj_t *s_al, *s_al_hora, *s_al_tit, *s_al_frase, *s_al_cri, *s_al_b[2], *s_al_bt[2];
static art_img_t s_al_hora_art, s_al_tit_art, s_al_frase_art, s_al_cri_art[2], s_al_bt_art[2];
static int s_al_tipo = -1, s_al_modo = -1, s_al_fc = -1;
static uint32_t s_al_desde, s_al_son;
static const pet_t *s_al_de;   /* la alarma es de otra mascota (descansa en perfiles); NULL: la activa */
static EXT_RAM_BSS_ATTR pet_t s_al_otra;
#define ALARMA_SONANDO_MS (5u * 60u * 1000u) /* 5 minutos sin respuesta: se pospone sola */

bool ui_alarm_ringing(void) { return s_al_modo == AL_ALARMA || s_al_modo == AL_DORMIR || s_al_modo == AL_TIEMPO; }

static void alarma_cerrar(void)
{
    s_al_tipo = -1;
    s_al_modo = -1;
    s_al_de = NULL;
    ocultar(s_al, true);
}

static void alarma_resp_cb(lv_event_t *e)
{
    int q = (int)(intptr_t)lv_event_get_user_data(e); /* 0 posponer, 1 listo */
    int i = s_al_tipo, modo = s_al_modo;
    bool otra = s_al_de != NULL;
    alarma_cerrar();
    if (otra) { globo(ICO_SOL, s_nino ? NULL : UNA_DE("¡BUENOS DÍAS!", "¡HOLA, BUEN DÍA!", "¡A DESPERTAR! ¡BUENOS DÍAS!", "¡QUÉ BUEN DÍA PARA JUGAR!", "¡BUENOS DÍAS! ¿DORMISTE BIEN?"), 3000); return; } /* la de otra mascota: solo se apaga */
    if (modo == AL_DORMIR) { globo(ICO_LUNA, s_nino ? NULL : UNA_DE("¡BUENAS NOCHES!", "¡A DORMIR! ¡HASTA MAÑANA!", "¡QUE DUERMAS BIEN!", "¡DULCES SUEÑOS!", "¡HASTA MAÑANA, QUE DESCANSES!", "¡BUENAS NOCHES! ¡NOS VEMOS MAÑANA!"), 3000); return; }
    if (modo == AL_TIEMPO) return;
    if (i < 0) return;
    accion(UI_ACCION_ALARMA_RESP, i | (q ? 0 : 8));
    if (q == 0) globo(ICO_ZETA, s_nino ? NULL : UNA_DE("UN RATITO MÁS...", "CINCO MINUTITOS MÁS...", "TODAVÍA TENGO SUEÑO..."), 2500);
    else globo(ICO_SOL, s_nino ? NULL : UNA_DE("¡BUENOS DÍAS!", "¡HOLA, BUEN DÍA!", "¡A DESPERTAR! ¡BUENOS DÍAS!", "¡QUÉ BUEN DÍA PARA JUGAR!", "¡BUENOS DÍAS! ¿DORMISTE BIEN?"), 3000);
}

static void alarma_mostrar(int modo, int i, uint32_t cuando);
void ui_alarm_other(const pet_t *otra, int i)
{
    if (!otra || ui_alarm_ringing()) return;
    s_al_otra = *otra;
    s_al_de = &s_al_otra;
    alarma_mostrar(AL_ALARMA, i, s_ultimo_t);
}
static bool s_al_reves;
void ui_set_upside_down(bool al_reves) { s_al_reves = al_reves; }
void ui_set_auto_rotate(bool si) { (void)si; } /* el ajuste se lee de s_disp_aj */
bool ui_upside_down(void) { return s_al_reves; }

static void reserva_cb(lv_event_t *e) { (void)e; if (s_al_modo == AL_RESERVA) alarma_cerrar(); } /* (en el simulador) */

static void crear_alarma(lv_obj_t *scr)
{
    (void)scr;
    s_al = caja(lv_layer_top()); /* arriba de todo */
    lv_obj_set_size(s_al, W, W);
    lv_obj_add_flag(s_al, LV_OBJ_FLAG_CLICKABLE); /* que no se toque lo de abajo */
    lv_obj_add_event_cb(s_al, reserva_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_set_style_bg_opa(s_al, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_grad_dir(s_al, LV_GRAD_DIR_VER, 0);
    s_al_hora = lv_image_create(s_al);
    s_al_tit = lv_image_create(s_al);
    s_al_cri = lv_image_create(s_al);
    s_al_frase = lv_image_create(s_al);
    for (int k = 0; k < 2; k++) {
        s_al_b[k] = boton(s_al, k ? 176 : 150, 58, alarma_resp_cb, k);
        lv_obj_align(s_al_b[k], LV_ALIGN_TOP_MID, k ? 90 : -86, 324);
        if (k) lv_obj_set_style_bg_color(s_al_b[k], lv_color_hex(C_VERDE), 0);
        s_al_bt[k] = lv_image_create(s_al_b[k]);
    }
    ocultar(s_al, true);
}

static void alarma_mostrar(int modo, int i, uint32_t cuando)
{
    if (!s_pet) return;
    const pet_t *ap = s_al_de ? s_al_de : s_pet; /* la alarma puede ser de otra mascota (las que suenan siempre) */
    cerrar_panel();
    const bool dormir = modo == AL_DORMIR, tiempo = modo == AL_TIEMPO;
    s_al_tipo = i;
    s_al_modo = modo;
    s_al_desde = s_al_son = s_ms;
    s_al_fc = -1;
    if (dormir && cuna_cancion()) {} /* (v38) la hora de dormir con nuestra canción de cuna */
    else if (!dormir && !tiempo && alarma_cancion(ap)) {} /* (v38) la alarma con nuestra canción */
    else accion(UI_ACCION_SONAR, dormir ? SND_A_DORMIR : (tiempo ? SND_TESORO : pet_alarm_sound(ap->alarma[i % PET_ALARMAS].sonido)));
    lv_obj_set_style_bg_color(s_al, lv_color_hex(dormir ? 0x141838 : 0xFFB878), 0);
    lv_obj_set_style_bg_grad_color(s_al, lv_color_hex(dormir ? 0x4A3A80 : 0xFFF0C0), 0);
    int64_t l = (int64_t)(cuando > s_ultimo_t ? cuando : s_ultimo_t) + ap->zona_horaria;
    int min = (int)(((l % 86400) + 86400) % 86400 / 60);
    char t[64];
    snprintf(t, sizeof t, "%02d:%02d", min / 60, min % 60);
    art_texto(&s_al_hora_art, t, dormir ? 0xFFFFFF : C_TINTA, 7, 0);
    poner_src(s_al_hora, &s_al_hora_art.dsc);
    lv_obj_align(s_al_hora, LV_ALIGN_TOP_MID, 0, 62);
    art_texto(&s_al_tit_art, dormir ? "¡HORA DE DORMIR!" : (tiempo ? "¡SE ACABÓ EL TIEMPO!" : "¡A DESPERTAR!"), dormir ? 0xFFE27A : 0xC0405A, 3, 0);
    poner_src(s_al_tit, &s_al_tit_art.dsc);
    lv_obj_align(s_al_tit, LV_ALIGN_TOP_MID, 0, 132);
    snprintf(t, sizeof t, dormir ? "%s: ¿VAMOS A DORMIR? MAÑANA JUGAMOS MÁS" : (tiempo ? "%s: ¡RIIING! YA PASÓ EL TIEMPO" : "%s: ¡BUENOS DÍAS! ¿DORMISTE BIEN?"),
             ap->nombre[0] ? ap->nombre : "TU MASCOTA");
    art_texto(&s_al_frase_art, t, dormir ? 0xD8DCF2 : 0x5A4A3A, 2, 300);
    poner_src(s_al_frase, &s_al_frase_art.dsc);
    lv_obj_align(s_al_frase, LV_ALIGN_TOP_MID, 0, 270);
    ocultar(s_al_hora, false);
    ocultar(s_al_tit, s_nino);
    ocultar(s_al_frase, s_nino);
    for (int f = 0; f < 2; f++) {
        if (ap->etapa == STAGE_HUEVO) { art_huevo(&s_al_cri_art[f], pet_look(ap), 5, 0, f ? 1 : -1); continue; }
        art_expresion_t ex = dormir ? (f ? EXP_CERRADOS : EXP_ABURRIDO) : EXP_FELIZ;
        art_pose_t po = {pet_look(ap), ap->etapa, ap->etapa == STAGE_ADULTA ? ap->forma : 0, 5, ex, f == 1, f, false, false,
                         dormir ? -1 : 1, dormir ? 0 : 1, 0, {0, 0, 0}, 0, ap->guardiana};
        for (int r = 0; r < RANURA_COUNT; r++) po.ropa[r] = pet_worn_raw(ap, r);
        art_criatura_pose(&s_al_cri_art[f], &po);
    }
    static const char *const BT[3][2] = {{"5 MIN MÁS", "¡YA DESPERTÉ!"}, {"", "BUENAS NOCHES"}, {"", "ACEPTAR"}};
    const int fila = dormir ? 1 : (tiempo ? 2 : 0);
    for (int k = 0; k < 2; k++) {
        ocultar(s_al_b[k], fila && k == 0);
        if (s_nino) lv_image_set_src(s_al_bt[k], art_icono(k ? (dormir ? ICO_LUNA : ICO_SI) : ICO_ZETA, 3));
        else { art_texto(&s_al_bt_art[k], BT[fila][k], C_BLANCO, 2, k ? 160 : 136); poner_src(s_al_bt[k], &s_al_bt_art[k].dsc); }
        lv_obj_center(s_al_bt[k]);
    }
    lv_obj_align(s_al_b[1], LV_ALIGN_TOP_MID, fila ? 0 : 90, 324);
    ocultar(s_al, false);
    lv_obj_move_foreground(s_al);
}

/* casi sin batería: se va a dormir para que el reloj guarde la hora. La escena: noche con estrellas y luna, ella dormida
 * sobre su colchoneta, su pensamiento en una burbuja (con la pila bajita) y las zzz que suben */
#define RS_ESTRELLAS 14
static lv_obj_t *s_rs_est[RS_ESTRELLAS], *s_rs_luna, *s_rs_tapete, *s_rs_burb, *s_rs_pt[2], *s_rs_zeta[3], *s_rs_txt, *s_rs_tit, *s_rs_bat;
static bool s_rs_listo;
static void reserva_crear(void)
{
    if (s_rs_listo) return;
    s_rs_listo = true;
    for (int i = 0; i < RS_ESTRELLAS; i++) {
        s_rs_est[i] = caja(s_al);
        int sz = 2 + i % 2;
        lv_obj_set_size(s_rs_est[i], sz, sz);
        lv_obj_set_style_bg_color(s_rs_est[i], lv_color_hex(i % 3 ? 0xFFFFFF : 0xFFE27A), 0);
        lv_obj_set_style_bg_opa(s_rs_est[i], LV_OPA_COVER, 0);
        lv_obj_remove_flag(s_rs_est[i], LV_OBJ_FLAG_CLICKABLE);
        int ang = (i * 137) % 360;
        lv_obj_set_pos(s_rs_est[i], CX + (int)(cosf(ang * 0.01745f) * (110 + (i * 53) % 90)), 36 + (int)(fabsf(sinf(ang * 0.01745f)) * (60 + (i * 31) % 150)));
    }
    s_rs_luna = lv_image_create(s_al);
    lv_image_set_src(s_rs_luna, art_icono(ICO_LUNA, 5));
    lv_obj_set_pos(s_rs_luna, 328, 64);
    s_rs_tapete = caja(s_al); /* su colchoneta */
    lv_obj_set_size(s_rs_tapete, 150, 38);
    lv_obj_set_style_radius(s_rs_tapete, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(s_rs_tapete, lv_color_hex(0x8E5BA8), 0);
    lv_obj_set_style_bg_opa(s_rs_tapete, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_rs_tapete, 5, 0);
    lv_obj_set_style_border_color(s_rs_tapete, lv_color_hex(0xF2D9A0), 0);
    lv_obj_set_style_outline_width(s_rs_tapete, 2, 0);
    lv_obj_set_style_outline_color(s_rs_tapete, lv_color_hex(0x2A2238), 0);
    lv_obj_t *raya = caja(s_rs_tapete);
    lv_obj_set_size(raya, 90, 6);
    lv_obj_center(raya);
    lv_obj_set_style_bg_color(raya, lv_color_hex(0xC88AD8), 0);
    lv_obj_set_style_bg_opa(raya, LV_OPA_COVER, 0);
    s_rs_burb = caja(s_al); /* el pensamiento */
    lv_obj_set_size(s_rs_burb, 310, 112);
    lv_obj_set_style_radius(s_rs_burb, 40, 0);
    lv_obj_set_style_bg_color(s_rs_burb, lv_color_hex(0xF4F0FF), 0);
    lv_obj_set_style_bg_opa(s_rs_burb, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_rs_burb, 3, 0);
    lv_obj_set_style_border_color(s_rs_burb, lv_color_hex(0x6A5AA8), 0);
    lv_obj_align(s_rs_burb, LV_ALIGN_TOP_MID, 0, 90);
    for (int i = 0; i < 2; i++) {
        s_rs_pt[i] = caja(s_al);
        int d = 12 - i * 5;
        lv_obj_set_size(s_rs_pt[i], d, d);
        lv_obj_set_style_radius(s_rs_pt[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(s_rs_pt[i], lv_color_hex(0xF4F0FF), 0);
        lv_obj_set_style_bg_opa(s_rs_pt[i], LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(s_rs_pt[i], 2, 0);
        lv_obj_set_style_border_color(s_rs_pt[i], lv_color_hex(0x6A5AA8), 0);
        lv_obj_align(s_rs_pt[i], LV_ALIGN_TOP_MID, 8 + i * 8, 196 + i * 12);
    }
    for (int i = 0; i < 3; i++) {
        s_rs_zeta[i] = lv_image_create(s_al);
        lv_image_set_src(s_rs_zeta[i], art_icono(ICO_ZETA, 2 + i));
    }
    s_rs_tit = lv_image_create(s_al);
    s_rs_txt = lv_image_create(s_rs_burb);
    s_rs_bat = caja(s_rs_burb);
    lv_obj_set_size(s_rs_bat, 60, 30);
    lv_obj_set_style_bg_opa(s_rs_bat, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_rs_bat, 0, 0);
}
static art_img_t s_rs_tit_art, s_rs_txt_art;
static bool s_rs_on;
void ui_show_reserve(void)
{
    if (!s_pet) return;
    cerrar_panel();
    s_al_tipo = -1;
    s_al_modo = AL_RESERVA;
    s_rs_on = true;
    s_al_desde = s_ms;
    reserva_crear();
    lv_obj_set_style_bg_color(s_al, lv_color_hex(0x0A0C24), 0);
    lv_obj_set_style_bg_grad_color(s_al, lv_color_hex(0x2E2658), 0);
    art_texto(&s_rs_tit_art, "CASI SIN PILA", 0xFFE27A, 3, 0);
    lv_image_set_src(s_rs_tit, &s_rs_tit_art.dsc);
    lv_obj_align(s_rs_tit, LV_ALIGN_TOP_MID, 0, 44);
    ocultar(s_rs_tit, s_nino);
    uint32_t prox = 0; /* la alarma que viene: igual despierta al reloj a esa hora aunque no haya pila para el resto */
    for (int i = 0; i < PET_ALARMAS; i++) {
        uint32_t x = pet_alarm_next(s_pet, i, s_ultimo_t);
        if (x && (!prox || x < prox)) prox = x;
    }
    char rt[96] = "ME VOY A DORMIR PARA CUIDAR LA HORA. CÁRGAME Y DESPIERTO";
    if (prox) {
        int64_t l = (int64_t)prox + s_pet->zona_horaria;
        int min = (int)(((l % 86400) + 86400) % 86400 / 60);
        snprintf(rt, sizeof rt, "ME VOY A DORMIR. TE DESPIERTO A LAS %02d:%02d POR TU ALARMA, O ANTES SI ME CARGAS", min / 60, min % 60);
    }
    art_texto(&s_rs_txt_art, rt, C_TINTA, 2, 260);
    lv_image_set_src(s_rs_txt, &s_rs_txt_art.dsc);
    ocultar(s_rs_txt, s_nino);
    lv_obj_center(s_rs_txt);
    lv_obj_clean(s_rs_bat);
    if (s_nino) { /* los chicos: solo dibujos (pila bajita y luna) */
        lv_obj_set_size(s_rs_burb, 200, 80);
        lv_obj_align(s_rs_burb, LV_ALIGN_TOP_MID, 0, 100);
        bateria_dibujo(s_rs_burb, 0, 24, false);
        lv_obj_align(s_rs_bat, LV_ALIGN_TOP_MID, 0, 0);
    } else { lv_obj_set_size(s_rs_burb, 310, 112); lv_obj_align(s_rs_burb, LV_ALIGN_TOP_MID, 0, 90); }
    ocultar(s_al_hora, true);
    ocultar(s_al_tit, true);
    ocultar(s_al_frase, true);
    for (int k = 0; k < 2; k++) ocultar(s_al_b[k], true);
    if (s_pet->etapa == STAGE_HUEVO) art_huevo(&s_al_cri_art[0], pet_look(s_pet), 5, 0, 0);
    else {
        art_pose_t po = {pet_look(s_pet), s_pet->etapa, s_pet->etapa == STAGE_ADULTA ? s_pet->forma : 0, 5, EXP_CERRADOS, false, 0, false, false, -1, -1, -2, {0, 0, 0}, 0, s_pet->guardiana};
        for (int r = 0; r < RANURA_COUNT; r++) po.ropa[r] = pet_worn_raw(s_pet, r);
        art_criatura_pose(&s_al_cri_art[0], &po);
    }
    poner_src(s_al_cri, &s_al_cri_art[0].dsc);
    int cw = (int)s_al_cri_art[0].dsc.header.w, ch = (int)s_al_cri_art[0].dsc.header.h;
    lv_obj_set_pos(s_rs_tapete, CX - 75, 330);
    lv_obj_set_pos(s_al_cri, CX - cw / 2, 344 - ch);
    for (int i = 0; i < 2; i++) lv_obj_move_foreground(s_rs_pt[i]);
    lv_obj_move_foreground(s_al_cri);
    for (int i = 0; i < 3; i++) lv_obj_move_foreground(s_rs_zeta[i]);
    ocultar(s_al, false);
    lv_obj_move_foreground(s_al);
}
/* cada cuadro: las estrellitas titilan, ella respira y las zzz suben */
static void animar_reserva(void)
{
    if (!s_rs_on || s_al_modo != AL_RESERVA) { s_rs_on = false; return; }
    const uint32_t t = s_ms - s_al_desde;
    for (int i = 0; i < RS_ESTRELLAS; i++) lv_obj_set_style_opa(s_rs_est[i], (lv_opa_t)(110 + (int)((sinf((float)t / 400.0f + (float)i * 1.7f) * 0.5f + 0.5f) * 145.0f)), 0);
    int cw = (int)s_al_cri_art[0].dsc.header.w, ch = (int)s_al_cri_art[0].dsc.header.h;
    float resp = sinf((float)t / 900.0f) * 2.0f;
    lv_obj_set_pos(s_al_cri, CX - cw / 2, (int)(344 - ch - resp));
    for (int i = 0; i < 3; i++) {
        float k = fmodf((float)t / 2600.0f + (float)i / 3.0f, 1.0f);
        lv_obj_set_pos(s_rs_zeta[i], CX + 40 + (int)(k * 36) + (int)(sinf(k * 9.0f) * 6.0f), 330 - ch / 2 - (int)(k * 70));
        lv_obj_set_style_opa(s_rs_zeta[i], (lv_opa_t)(255 - (int)(k * 220)), 0);
    }
}

static void animar_alarma(void)
{
    if (!ui_alarm_ringing()) return;
    const bool dormir = s_al_modo == AL_DORMIR, tiempo = s_al_modo == AL_TIEMPO;
    uint32_t t = s_ms - s_al_desde;
    int fc = dormir ? (int)(t / 1100) % 2 : (int)(t / 260) % 2;
    if (fc != s_al_fc) { s_al_fc = fc; poner_src(s_al_cri, &s_al_cri_art[fc].dsc); }
    int cw = (int)s_al_cri_art[0].dsc.header.w, ch = (int)s_al_cri_art[0].dsc.header.h;
    float salto = dormir ? sinf((float)t / 700.0f) * 2 : fabsf(sinf((float)t / 180.0f)) * 14;
    lv_obj_set_pos(s_al_cri, CX - cw / 2, (int)(262 - ch - salto));
    /* el despertador se repite; la de dormir suena una sola vez y se cierra sola al minuto */
    if (!dormir && !tiempo && s_pet && pet_song_for(s_al_de ? s_al_de : s_pet, 0) >= 0) { /* (v38) con nuestra canción: la canción, y entre vueltas el sonido de la alarma */
        if (!rep_activo() && s_ms - s_al_son >= 1500) { s_al_son = s_ms; static int vuelta; if (vuelta++ % 2) accion(UI_ACCION_SONAR, pet_alarm_sound((s_al_de ? s_al_de : s_pet)->alarma[s_al_tipo % PET_ALARMAS].sonido)); else alarma_cancion(s_al_de ? s_al_de : s_pet); }
    } else if (!dormir && s_ms - s_al_son >= 3000 && s_pet) { s_al_son = s_ms; accion(UI_ACCION_SONAR, tiempo ? SND_TESORO : pet_alarm_sound((s_al_de ? s_al_de : s_pet)->alarma[s_al_tipo % PET_ALARMAS].sonido)); }
    if ((dormir || tiempo) && t >= 60000) { alarma_cerrar(); return; }
    if (t >= ALARMA_SONANDO_MS) {
        int i = s_al_tipo;
        bool otra = s_al_de != NULL;
        alarma_cerrar();
        if (i >= 0 && !otra) accion(UI_ACCION_ALARMA_RESP, i | 8); /* no respondiste: vuelve a sonar en 5 minutos */
    }
}

/* sin casita, duerme en su tapete (aparece cada vez que se duerme fuera de la casita) */
static lv_obj_t *s_tapete;
static void crear_tapete(void)
{
    s_tapete = caja(s_mundo);
    lv_obj_set_size(s_tapete, 84, 24);
    lv_obj_set_style_radius(s_tapete, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(s_tapete, lv_color_hex(0x8E5BA8), 0);
    lv_obj_set_style_bg_opa(s_tapete, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_tapete, 4, 0);
    lv_obj_set_style_border_color(s_tapete, lv_color_hex(0xF2D9A0), 0);
    lv_obj_set_style_outline_width(s_tapete, 2, 0);
    lv_obj_set_style_outline_color(s_tapete, lv_color_hex(0x2A2238), 0);
    lv_obj_t *raya = caja(s_tapete);
    lv_obj_set_size(raya, 50, 4);
    lv_obj_center(raya);
    lv_obj_set_style_bg_color(raya, lv_color_hex(0xC88AD8), 0);
    lv_obj_set_style_bg_opa(raya, LV_OPA_COVER, 0);
    ocultar(s_tapete, true);
}
static void animar_tapete(void)
{
    const pet_t *p = s_pet;
    /* si va a dormir a su casita no lleva el tapete; fuera de ella, sí */
    bool ver = p && p->dormida && p->etapa != STAGE_HUEVO && !s_interior && !duerme_en_casita(p) && !lv_obj_has_flag(s_mascota.img, LV_OBJ_FLAG_HIDDEN);
    ocultar(s_tapete, !ver);
    if (!ver) return;
    if (s_escena == ESC_LAGO) { /* en el lago, la colchoneta va sobre la orilla firme: nunca sobre el agua */
        float yo = (float)terreno_ymin(s_mascota.x - 42.0f), yd = (float)terreno_ymin(s_mascota.x + 42.0f), ym = (yo > yd ? yo : yd) + 26.0f;
        if (s_mascota.y < ym) { s_mascota.y = s_mascota.ty = ym; }
    }
    lv_obj_set_pos(s_tapete, (int)s_mascota.x - 42, (int)s_mascota.y - 14);
    uint32_t i = lv_obj_get_index(s_mascota.sombra);
    if (lv_obj_get_index(s_tapete) + 1 != (int32_t)i) lv_obj_move_to_index(s_tapete, (int32_t)i);
}

static void crear_fase5(lv_obj_t *scr)
{
    crear_juguetes();
    crear_tapete();
    crear_hoyo();
    crear_pecera(scr);
    s_pasa = lv_image_create(s_mundo);
    ocultar(s_pasa, true);
}

/* ======================================================================
 * Construcción
 * ==================================================================== */

static void tick_cb(lv_timer_t *t);

static lv_obj_t *s_resp_aviso;
static void resp_aviso_cb(lv_event_t *e) { (void)e; panel_respaldo(); }

/* los últimos toques (para la prueba guiada del táctil: la placa dice dónde le tocaron) */
static int16_t s_tq_x[16], s_tq_y[16];
static volatile int s_tq_n;
void ui_touch_reset(void) { s_tq_n = 0; }
int ui_touch_get(int *x, int *y, int max)
{
    const int n = s_tq_n < 16 ? s_tq_n : 16;
    for (int i = 0; i < n && i < max; i++) { x[i] = s_tq_x[i]; y[i] = s_tq_y[i]; }
    return s_tq_n;
}
#ifdef ESP_PLATFORM
#include "esp_timer.h"
extern void hal_log(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static void toque_log_cb(lv_event_t *e)
{
    lv_point_t pt = {-1, -1};
    lv_indev_t *in = (lv_indev_t *)lv_event_get_user_data(e); /* el dispositivo táctil se guardó al registrar el aviso: el evento no lo traía (salía siempre -1,-1) */
    if (!in) in = (lv_indev_t *)lv_event_get_target(e);
    if (!in) in = lv_indev_active();
    if (in) lv_indev_get_point(in, &pt);
    if (s_tq_n < 16) { s_tq_x[s_tq_n] = (int16_t)pt.x; s_tq_y[s_tq_n] = (int16_t)pt.y; }
    s_tq_n++;
    lv_obj_t *o = lv_indev_get_active_obj();
    hal_log("toque: x %d y %d | pecera %d | sobre %s", (int)pt.x, (int)pt.y, s_pecera ? 1 : 0, o == s_pecera_bloqueo ? "bloqueo" : (o == s_pecera_btn || o == s_pecera_rit || o == s_pecera_vol || o == s_pecera_tmp) ? "boton pecera" : o ? "otro" : "nada");
}
#endif

static void obj_desliza_cb(lv_event_t *e);
/* dentro de un menú, deslizar a un lado (en CUALQUIER parte: sobre la tarjeta, un botón, el borde o la esquina) pasa de hoja.
 * Se mira el dedo en general y no un objeto en particular; solo vale si el dedo se apoyó con el menú ya abierto
 * (el deslizar que lo abrió, desde la orilla, no cuenta) */
static bool s_pag_vale;
static void pag_global_cb(lv_event_t *e)
{
    const lv_event_code_t c = lv_event_get_code(e);
    if (c == LV_EVENT_PRESSED) {
        s_pag_vale = s_panel == PANEL_MAS || s_panel == PANEL_OBJETO;
        if (s_pag_vale) { if (s_panel == PANEL_MAS) ella_pag_cb(e); else obj_desliza_cb(e); }
        return;
    }
    if (c != LV_EVENT_RELEASED || !s_pag_vale) return;
    s_pag_vale = false;
    if (s_panel == PANEL_MAS) ella_pag_cb(e);
    else if (s_panel == PANEL_OBJETO) obj_desliza_cb(e);
}

volatile int g_ui_paso; /* v88: por dónde va ui_init / ui_update (si el arranque se cuelga, el vigilante lo anota) */
static bool s_arrancando = true; /* hasta que el arranque llega al bucle: no se abre la pregunta de la hora (se abre en la primera vuelta) */
void ui_boot_done(void) { s_arrancando = false; }
void ui_init(lv_display_t *disp, ui_action_cb_t on_action)
{
    g_ui_paso = 1;
    s_on_action = on_action;
#ifdef ESP_PLATFORM
    { lv_indev_t *in0 = lv_indev_get_next(NULL); if (in0) lv_indev_add_event_cb(in0, toque_log_cb, LV_EVENT_PRESSED, in0); }
#endif
    { lv_indev_t *in1 = lv_indev_get_next(NULL); if (in1) { lv_indev_add_event_cb(in1, pag_global_cb, LV_EVENT_PRESSED, NULL); lv_indev_add_event_cb(in1, pag_global_cb, LV_EVENT_RELEASED, NULL); } }
    lv_obj_t *scr = lv_display_get_screen_active(disp);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), 0);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    s_fondo = lv_image_create(scr);
    lv_obj_set_pos(s_fondo, 0, 0);
    lv_obj_add_flag(s_fondo, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_fondo, fondo_cb, LV_EVENT_CLICKED, NULL);
    /* la penumbra de la casita cuando ella duerme: una capa encima del fondo, así no hay que repintar todo el cuarto (tardaba segundos) */
    s_casa_pen = caja(scr);
    lv_obj_set_size(s_casa_pen, W, W);
    lv_obj_set_pos(s_casa_pen, 0, 0);
    lv_obj_set_style_bg_color(s_casa_pen, lv_color_hex(0x12112A), 0);
    lv_obj_set_style_bg_opa(s_casa_pen, 85, 0); /* la penumbra al dormir: antes 170, quedaba casi negro */
    lv_obj_remove_flag(s_casa_pen, LV_OBJ_FLAG_CLICKABLE);
    ocultar(s_casa_pen, true);
    s_mundo = caja(scr);
    lv_obj_set_size(s_mundo, W, W);

    for (int i = 0; i < N_CACAS; i++) {
        s_cacas[i] = lv_image_create(s_mundo);
        lv_image_set_src(s_cacas[i], art_icono(ICO_CACA, 3));
        lv_obj_add_flag(s_cacas[i], LV_OBJ_FLAG_CLICKABLE); /* tocarla la recoge */
        lv_obj_set_ext_click_area(s_cacas[i], 8);
        lv_obj_add_event_cb(s_cacas[i], caca_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        ocultar(s_cacas[i], true);
    }
    for (int i = 0; i < N_LUCES; i++) {
        s_luces[i] = caja(s_mundo);
        lv_obj_set_size(s_luces[i], 6, 6);
        lv_obj_set_style_bg_color(s_luces[i], lv_color_hex(0xFFF4A0), 0);
        lv_obj_set_style_bg_opa(s_luces[i], LV_OPA_COVER, 0);
        s_luz_x[i] = 80 + azar01() * 300;
        s_luz_y[i] = 100 + azar01() * 230;
        s_luz_f[i] = azar01() * 6.28f;
        ocultar(s_luces[i], true);
    }
    for (int i = 0; i < N_TIERRA; i++) {
        s_tierra[i] = caja(s_mundo);
        lv_obj_set_size(s_tierra[i], 6, 6);
        lv_obj_set_style_radius(s_tierra[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(s_tierra[i], lv_color_hex(0x8B5E3C), 0);
        lv_obj_set_style_bg_opa(s_tierra[i], LV_OPA_COVER, 0);
        ocultar(s_tierra[i], true);
    }
    for (int i = 0; i < N_ONDITA; i++) {
        s_ondita[i] = caja(s_mundo);
        lv_obj_set_style_radius(s_ondita[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(s_ondita[i], LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(s_ondita[i], 2, 0);
        lv_obj_set_style_border_color(s_ondita[i], lv_color_hex(0xE8F8FF), 0);
        ocultar(s_ondita[i], true);
    }

    g_ui_paso = 2;
    crear_mundo_vivo();
    crear_criatura(&s_visita);
    lv_obj_add_flag(s_visita.img, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_visita.img, visita_cb, LV_EVENT_CLICKED, NULL);
    mostrar_criatura(&s_visita, false);
    crear_bote();
    crear_hoyo_hielo();
    crear_pastel();
    crear_criatura(&s_vecino);
    lv_obj_add_flag(s_vecino.img, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_vecino.img, vecino_cb, LV_EVENT_CLICKED, NULL);
    mostrar_criatura(&s_vecino, false);
    g_ui_paso = 3;
    crear_criatura(&s_mascota);
    lv_obj_add_flag(s_mascota.img, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(s_mascota.img, 10);
    lv_obj_add_event_cb(s_mascota.img, mascota_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(s_mascota.img, mascota_cb, LV_EVENT_PRESSING, NULL);
    lv_obj_add_event_cb(s_mascota.img, mascota_cb, LV_EVENT_RELEASED, NULL);
    lv_obj_remove_flag(s_mascota.img, LV_OBJ_FLAG_GESTURE_BUBBLE);
    mostrar_criatura(&s_mascota, true);

    s_pelota = lv_image_create(s_mundo);
    lv_image_set_src(s_pelota, art_icono(ICO_PELOTA, 3));
    ocultar(s_pelota, true);
    s_objeto = lv_image_create(s_mundo);
    ocultar(s_objeto, true);
    crear_fauna();
    s_pelota2 = lv_image_create(s_mundo);
    lv_image_set_src(s_pelota2, art_icono(ICO_PELOTA, 3));
    ocultar(s_pelota2, true);
    s_objeto2 = lv_image_create(s_mundo);
    ocultar(s_objeto2, true);

    /* clima: partículas sobre el mundo */
    for (int i = 0; i < N_PART; i++) {
        s_part[i] = caja(scr);
        lv_obj_set_style_bg_opa(s_part[i], LV_OPA_COVER, 0);
        s_part_x[i] = azar01() * W;
        s_part_y[i] = azar01() * W;
        s_part_v[i] = 0.7f + azar01() * 0.6f;
        ocultar(s_part[i], true);
    }

    for (int i = 0; i < N_FLOTANTES; i++) {
        s_flot[i].img = lv_image_create(scr);
        ocultar(s_flot[i].img, true);
    }

    /* globo: tarjeta crema con borde, y una "cola" */
    s_globo = caja(scr);
    lv_obj_set_size(s_globo, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(s_globo, lv_color_hex(0x0E1440), 0); /* noche opaca con borde dorado (que el texto se lea sobre cualquier fondo) */
    lv_obj_set_style_bg_opa(s_globo, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_globo, 2, 0);
    lv_obj_set_style_border_color(s_globo, lv_color_hex(0xD0B969), 0);
    lv_obj_set_style_border_opa(s_globo, LV_OPA_90, 0);
    lv_obj_set_style_radius(s_globo, 12, 0);
    lv_obj_set_style_pad_hor(s_globo, 12, 0);
    lv_obj_set_style_pad_ver(s_globo, 8, 0);
    lv_obj_set_style_pad_column(s_globo, 8, 0);
    lv_obj_set_flex_flow(s_globo, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(s_globo, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    s_globo_ico = lv_image_create(s_globo);
    s_globo_area = lv_obj_create(s_globo); /* si el texto no cabe, se desliza con el dedo */
    lv_obj_remove_style_all(s_globo_area);
    lv_obj_set_size(s_globo_area, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_max_height(s_globo_area, 76, 0);
    lv_obj_set_scroll_dir(s_globo_area, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(s_globo_area, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_color(s_globo_area, lv_color_hex(C_VIOLETA), LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_opa(s_globo_area, LV_OPA_70, LV_PART_SCROLLBAR);
    lv_obj_set_style_width(s_globo_area, 4, LV_PART_SCROLLBAR);
    lv_obj_set_style_pad_right(s_globo_area, 6, 0);
    lv_obj_add_flag(s_globo_area, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(s_globo_area, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_event_cb(s_globo_area, globo_toque_cb, LV_EVENT_SCROLL, NULL);
    lv_obj_add_event_cb(s_globo_area, globo_toque_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(s_globo_area, globo_clic_cb, LV_EVENT_SHORT_CLICKED, NULL); /* (v39) un toque (sin deslizar) lo cierra */
    lv_obj_add_flag(s_globo, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_globo, globo_clic_cb, LV_EVENT_SHORT_CLICKED, NULL);
    s_globo_txt = lv_image_create(s_globo_area);
    s_globo_cola = caja(scr);
    lv_obj_set_size(s_globo_cola, 12, 12);
    lv_obj_set_style_bg_color(s_globo_cola, lv_color_hex(0x0E1440), 0);
    lv_obj_set_style_bg_opa(s_globo_cola, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_globo_cola, 2, 0);
    lv_obj_set_style_border_side(s_globo_cola, LV_BORDER_SIDE_LEFT | LV_BORDER_SIDE_RIGHT | LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_color(s_globo_cola, lv_color_hex(0xD0B969), 0);
    lv_obj_set_style_border_opa(s_globo_cola, LV_OPA_90, 0);
    ocultar(s_globo, true);
    ocultar(s_globo_cola, true);

    /* pensamiento: nube redonda con dos burbujitas */
    s_pens = caja(scr);
    lv_obj_set_size(s_pens, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(s_pens, lv_color_hex(0xF4F1FF), 0);
    lv_obj_set_style_bg_opa(s_pens, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_pens, 3, 0);
    lv_obj_set_style_border_color(s_pens, lv_color_hex(0x8C84B0), 0);
    lv_obj_set_style_radius(s_pens, 24, 0);
    lv_obj_set_style_pad_all(s_pens, 8, 0);
    lv_obj_set_style_pad_column(s_pens, 6, 0);
    lv_obj_set_flex_flow(s_pens, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(s_pens, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    s_pens_emo = lv_image_create(s_pens);
    s_pens_causa = lv_image_create(s_pens);
    lv_obj_t **bur[2] = {&s_pens_b1, &s_pens_b2};
    for (int i = 0; i < 2; i++) {
        *bur[i] = caja(scr);
        lv_obj_set_size(*bur[i], 10 + i * 4, 10 + i * 4);
        lv_obj_set_style_bg_color(*bur[i], lv_color_hex(0xF4F1FF), 0);
        lv_obj_set_style_bg_opa(*bur[i], LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(*bur[i], 2, 0);
        lv_obj_set_style_border_color(*bur[i], lv_color_hex(0x8C84B0), 0);
        lv_obj_set_style_radius(*bur[i], LV_RADIUS_CIRCLE, 0);
        ocultar(*bur[i], true);
    }
    ocultar(s_pens, true);

    /* arriba solo el clima y la batería: sin hora ni números */
    s_clima_ico = lv_image_create(scr);
    lv_obj_set_pos(s_clima_ico, CX - 18, 28);
    ocultar(s_clima_ico, true);
    s_bat = caja(scr);
    lv_obj_set_size(s_bat, 28, 14);
    lv_obj_set_style_border_width(s_bat, 2, 0);
    lv_obj_set_style_border_color(s_bat, lv_color_hex(C_BLANCO), 0);
    lv_obj_set_style_radius(s_bat, 3, 0);
    lv_obj_set_style_bg_color(s_bat, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(s_bat, LV_OPA_40, 0);
    s_bat_lleno = caja(s_bat);
    lv_obj_set_height(s_bat_lleno, 6);
    lv_obj_set_pos(s_bat_lleno, 2, 2);
    lv_obj_set_style_bg_opa(s_bat_lleno, LV_OPA_COVER, 0);
    lv_obj_align(s_bat, LV_ALIGN_TOP_MID, 0, 72);
    s_resp_aviso = lv_image_create(scr); /* aviso suave: hace semanas que no la guardas en el computador */
    lv_image_set_src(s_resp_aviso, art_icono(ICO_RESPALDO, 2));
    lv_obj_add_flag(s_resp_aviso, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(s_resp_aviso, 12);
    lv_obj_add_event_cb(s_resp_aviso, resp_aviso_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_set_style_opa(s_resp_aviso, LV_OPA_70, 0);
    lv_obj_align(s_resp_aviso, LV_ALIGN_TOP_MID, -36, 66);
    ocultar(s_resp_aviso, true);
    ocultar(s_bat, true);

    /* barra de botones */
    static const art_icono_t DOCK[4] = {ICO_PLATO, ICO_PELOTA, ICO_DUCHA, ICO_ESTADO};
    for (int i = 0; i < 4; i++) {
        lv_obj_t *b = boton(scr, 72, 58, dock_cb, i); /* igual que los botones del modo pecera: más anchos y en arco, siguiendo el borde redondo */
        s_dock[i] = b;
        lv_obj_set_style_radius(b, 26, 0); /* bien redondeado: casi una píldora */
        lv_obj_set_pos(b, CX - 36 + (int)((i - 1.5f) * 76), 396);
        lv_obj_set_style_bg_opa(b, LV_OPA_40, 0); /* translúcidos, como los de la pecera */
        lv_obj_set_style_border_width(b, 0, 0);
        lv_obj_t *ic = icono(b, DOCK[i], 4);
        lv_obj_center(ic);
    }

    g_ui_paso = 4;
    crear_magia(scr);
    crear_niebla();
    s_tira = caja(scr); /* el borde de arriba: deslizar hacia abajo abre los accesos rápidos */
    lv_obj_set_size(s_tira, 180, 40);
    lv_obj_align(s_tira, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_add_flag(s_tira, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_tira, tira_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(s_tira, tira_cb, LV_EVENT_PRESSING, NULL);
    crear_flechas(scr);
    crear_quitar(scr);

    /* destello de los truenos */
    s_flash = caja(scr);
    lv_obj_set_size(s_flash, W, W);
    lv_obj_set_style_bg_color(s_flash, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(s_flash, LV_OPA_COVER, 0);
    lv_obj_set_style_opa(s_flash, LV_OPA_TRANSP, 0);

    g_ui_paso = 5;
    /* paneles */
    s_capa = caja(scr);
    lv_obj_set_size(s_capa, W, W);
    lv_obj_add_flag(s_capa, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(s_capa, lv_color_hex(0x0B0A14), 0);
    lv_obj_set_style_bg_opa(s_capa, LV_OPA_COVER, 0);
    lv_obj_add_event_cb(s_capa, capa_cb, LV_EVENT_CLICKED, NULL);
    s_vidrio = lv_image_create(s_capa); /* el vidrio redondo (ver preparar_vidrio) */
    lv_obj_set_pos(s_vidrio, 0, 0);
    s_tarjeta = caja(s_capa); /* el contenido va encima del vidrio, sin fondo propio */
    lv_obj_add_flag(s_tarjeta, LV_OBJ_FLAG_CLICKABLE);
    s_cerrar_x = lv_button_create(s_capa);
    lv_obj_set_size(s_cerrar_x, BOTON_ABAJO, BOTON_ABAJO);
    lv_obj_set_pos(s_cerrar_x, CX - BOTON_ABAJO / 2, BOTON_ABAJO_Y);
    lv_obj_set_style_radius(s_cerrar_x, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(s_cerrar_x, lv_color_hex(C_BOTON), 0);
    lv_obj_set_style_bg_color(s_cerrar_x, lv_color_hex(C_BOTON_P), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(s_cerrar_x, 3, 0);
    lv_obj_set_style_border_color(s_cerrar_x, lv_color_hex(0x15121F), 0);
    lv_obj_set_style_shadow_width(s_cerrar_x, 0, 0);
    lv_obj_set_ext_click_area(s_cerrar_x, 8);
    lv_obj_add_event_cb(s_cerrar_x, cerrar_x_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *xi = lv_image_create(s_cerrar_x);
    lv_image_set_src(xi, art_icono(ICO_NO, 2));
    lv_obj_center(xi);
    /* volver: siempre abajo a la izquierda, junto a la X */
    s_volver = lv_button_create(s_capa);
    lv_obj_set_size(s_volver, BOTON_ABAJO, BOTON_ABAJO);
    lv_obj_set_pos(s_volver, CX - BOTON_ABAJO / 2 - VOLVER_DX, BOTON_ABAJO_Y);
    lv_obj_set_style_radius(s_volver, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(s_volver, lv_color_hex(C_BOTON), 0);
    lv_obj_set_style_bg_color(s_volver, lv_color_hex(C_BOTON_P), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(s_volver, 3, 0);
    lv_obj_set_style_border_color(s_volver, lv_color_hex(0x15121F), 0);
    lv_obj_set_style_shadow_width(s_volver, 0, 0);
    lv_obj_set_ext_click_area(s_volver, 8);
    lv_obj_t *vi = icono(s_volver, ICO_FLECHA, 2);
    lv_image_set_pivot(vi, 12, 12);
    lv_image_set_rotation(vi, 1800);
    lv_obj_center(vi);
    ocultar(s_volver, true);
    ocultar(s_capa, true);

    g_ui_paso = 6;
    crear_juegos(scr);
    crear_fase4(scr);
    crear_fase5(scr);
    g_ui_paso = 7;
    crear_bienvenida(scr);
    crear_viaje(scr);
    g_ui_paso = 8;
    crear_cruce();
    crear_cielo();
    g_ui_paso = 9;
    crear_alarma(scr);
    s_tick_timer = lv_timer_create(tick_cb, TICK_MS, NULL);
    g_ui_paso = 10;
}

/* ======================================================================
 * Actualización desde el motor
 * ==================================================================== */

static int hora_de(const pet_t *p, uint32_t utc)
{
    int64_t l = (int64_t)utc + p->zona_horaria;
    return (int)(((l % 86400) + 86400) % 86400 / 3600);
}

static art_periodo_t periodo_de(const pet_t *p, uint32_t utc)
{
    int h = hora_de(p, utc);
    if (p->luz_apagada || h >= 20 || h < 6) return PERIODO_NOCHE;
    if (h >= 18 || h < 7) return PERIODO_ATARDECER;
    return PERIODO_DIA;
}

static void preparar_particulas(void);
static art_icono_t icono_ficha(int f) { return f < FAUNA_COUNT ? ICO_FAUNA[f] : (f < PET_FICHAS ? ICO_ANIMAL[f - FAUNA_COUNT] : ICO_HUELLA); }
static int s_fondo_casa = -1; /* luz y cuadro de la casita que se ven ahora */
static int s_cuadro_forzar = -1; /* para probar */
void ui_demo_cuadro(int c) { s_cuadro_forzar = c; s_fondo_casa = -1; }
void ui_demo_juego(int j) { s_juego_pedido = j; accion(ACT_JUGAR, j); }
void ui_demo_bateria(int pct, int modo) { ui_battery_aviso(pct, modo, 60000); } /* (pruebas del simulador) */
void ui_demo_picnic(void) { picnic_abrir(); } /* (pruebas del simulador) */
void ui_demo_botella(int f) { botella_nada(f); } /* (pruebas del simulador) */
void ui_demo_flotar(int ico, int x, int y) { flotar(ico, NULL, 0, x, y, 0); } /* (pruebas del simulador) */

/* el cuadro de la pared: cada día uno distinto entre lo que más le gusta (sus zonas preferidas, un animal amigo, la ballena de nubes) */
static int cuadro_del_dia(const pet_t *p, uint32_t utc, int *icono)
{
    if (s_cuadro_forzar >= 0) { *icono = s_cuadro_forzar == 7 ? (int)ICO_CONEJO : 0; return s_cuadro_forzar; }
    int mejor = -1, segundo = -1;
    for (int e = 0; e < ESC_COUNT; e++) {
        if (!pet_scene_open(p, (escena_t)e)) continue;
        const int g = pet_scene_liking(p, (escena_t)e);
        if (mejor < 0 || g > pet_scene_liking(p, (escena_t)mejor)) { segundo = mejor; mejor = e; }
        else if (segundo < 0 || g > pet_scene_liking(p, (escena_t)segundo)) segundo = e;
    }
    int cand[5], ic[5], n = 0;
    cand[n] = mejor >= 0 ? mejor : 0; ic[n++] = 0;
    if (segundo >= 0) { cand[n] = segundo; ic[n++] = 0; }
    const int fichas = pet_cards_known(p);
    if (fichas > 0) {
        const int f = pet_card_get(p, (int)(((utc + (uint32_t)p->zona_horaria) / 86400u) % (uint32_t)fichas));
        cand[n] = 7; ic[n++] = f >= 0 ? (int)icono_ficha(f) : 0;
    }
    if (p->dia_ballena) { cand[n] = 6; ic[n++] = 0; }
    const int k = (int)(((utc + (uint32_t)p->zona_horaria) / 86400u) % (uint32_t)n);
    *icono = ic[k];
    return cand[k];
}

/* fondo: zona, período del día, estación, clima y los caminos que ella marcó */
static void refrescar_fondo(const pet_t *p, uint32_t utc)
{
    int per = (int)periodo_de(p, utc);
    int est = (int)pet_season(p, utc), cli = (int)pet_weather(p);
    int gris = cli == CLIMA_FRIO ? 2 : (cli == CLIMA_NUBLADO || cli == CLIMA_LLUVIA || cli == CLIMA_NIEVE || cli == CLIMA_TORMENTA);
    int esc = s_interior ? ART_ESCENA_CASA : s_escena;
    if ((esc == ESC_LAGO || esc == ESC_ISLA) && est != ESTACION_INVIERNO && pet_lake_frozen(p, utc)) est = ESTACION_INVIERNO; /* (v38) la bebida helada del norte: el lago se ve helado */
    if (s_interior) { /* la ventana muestra la hora real de afuera: dormir de día no la oscurece (la penumbra del cuarto va aparte) */
        const int h = hora_de(p, utc);
        per = h >= 20 || h < 6 ? PERIODO_NOCHE : (h >= 18 || h < 7 ? PERIODO_ATARDECER : PERIODO_DIA);
    }
    int cuadro_ico = 0;
    const int cuadro = cuadro_del_dia(p, utc, &cuadro_ico);
    const bool luz_casa = true; /* el cuarto se pinta siempre con luz; al dormir se le pone encima una penumbra (s_casa_pen) */
    const int casa_clave = 1000000 + cuadro * 1000 + (cuadro_ico & 255);
    if (s_casa_pen) ocultar(s_casa_pen, !(s_interior && p->dormida));
    { /* (101b) si el fondo quedó sin adornos por falta de memoria (la casita del prado no se veía hasta reiniciar), se reintenta cada 30 s */
        static uint32_t parcial_ms;
        if (lv_tick_get() - parcial_ms >= 30000u) { parcial_ms = lv_tick_get(); if (art_fondo_parcial()) s_fondo_periodo = -1; }
    }
    uint32_t sendas = esc == ESC_PRADO ? pet_trail_mask(p) : 0;
    const bool en_casa = esc == ART_ESCENA_CASA;
    if (en_casa) { est = 0; gris = 0; } /* adentro no se nubla ni hiela: el clima de afuera lo muestra la ventana, sin repintar el fondo */
    if (per == s_fondo_periodo && (en_casa || p->decoraciones == s_fondo_decor) && est == s_fondo_estacion && (en_casa || cli == s_fondo_clima) &&
        esc == s_fondo_escena && sendas == s_fondo_sendas && (!en_casa || casa_clave == s_fondo_casa))
        return;
    art_set_casa(luz_casa, cuadro, cuadro_ico);
    s_fondo_casa = casa_clave;
    uint32_t deco = esc == ESC_ISLA ? (p->guardiana ? 1u : 0u) : pet_decor_mask(p); /* en la isla: el Corazón en la piedra */
    static uint32_t fallo_ms; /* v88: si no hubo memoria para el fondo, se reintenta cada 5 s (antes quedaba el fondo viejo para siempre) */
    if (fallo_ms && lv_tick_get() - fallo_ms < 5000u) return;
    const lv_image_dsc_t *d = art_fondo((art_periodo_t)per, deco, (art_estacion_t)est, gris, esc, sendas);
    if (!d) { /* (v101) antes de rendirse: suelta los fondos de las otras horas y la caché, y prueba otra vez (si no, se veía la zona anterior) */
        art_fondos_soltar_otros();
        lv_image_cache_drop(NULL);
        d = art_fondo((art_periodo_t)per, deco, (art_estacion_t)est, gris, esc, sendas);
    }
    if (!d) {
        fallo_ms = lv_tick_get() | 1u;
#ifdef ESP_PLATFORM
        extern void hal_evento(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
        hal_evento("fondo NO cargado (sin memoria): escena %d, hora %d, estación %d, clima %d; PSRAM %u KB, bloque mayor %u KB", esc, per, est, cli,
                   (unsigned)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024), (unsigned)(heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM) / 1024));
#endif
        return; /* el estado no cambia: se vuelve a intentar */
    }
    fallo_ms = 0;
    poner_src(s_fondo, d);
    bool cambia_part = cli != s_fondo_clima || esc != s_fondo_escena;
    s_fondo_periodo = per;
    s_fondo_decor = p->decoraciones;
    s_fondo_estacion = est;
    s_fondo_clima = cli;
    s_fondo_escena = esc;
    s_fondo_sendas = sendas;
    if (cambia_part) preparar_particulas();
    /* de noche cada zona tiene su luz mágica: luciérnagas en el prado, fuegos fatuos en el bosque, reflejos de
     * estrellas en el lago, chispas violeta en la colina y cristales que brillan en la cueva */
    static const uint32_t LUZ[ESC_COUNT] = {0xFFF4A0, 0x7AF0FF, 0xDDEBFF, 0xC8A8FF, 0xB48CFF, 0xFFE27A, 0xD8FF8A, 0xFFFFFF};
    bool noche = per == PERIODO_NOCHE || (s_escena == ESC_CUEVA && !s_interior);
    for (int i = 0; i < N_LUCES; i++) {
        ocultar(s_luces[i], !noche || ((cli == CLIMA_LLUVIA || cli == CLIMA_TORMENTA) && s_escena != ESC_CUEVA) || s_interior);
        int z = s_escena >= 0 && s_escena < ESC_COUNT ? s_escena : 0;
        lv_obj_set_style_bg_color(s_luces[i], lv_color_hex(LUZ[z]), 0);
        lv_obj_set_style_radius(s_luces[i], z == ESC_BOSQUE ? 4 : (z == ESC_CUEVA ? 0 : 3), 0);
        lv_obj_set_size(s_luces[i], z == ESC_BOSQUE ? 8 : 6, z == ESC_BOSQUE ? 8 : 6);
        lv_obj_set_style_shadow_width(s_luces[i], z == ESC_BOSQUE ? 14 : (z == ESC_CUEVA ? 4 : 6), 0); /* en la cueva, sombra chica: las difuminadas grandes frenan el dibujo */
        lv_obj_set_style_shadow_color(s_luces[i], lv_color_hex(LUZ[z]), 0);
    }
}

/* un globo con texto (para las pruebas del simulador) */
void ui_say(const char *texto) { globo(ICO_CORAZON, texto, 3000); }

/* pantalla de bienvenida al encender (y aviso si el reinicio fue por un fallo): un disco que se cierra solo o al tocarlo */
static void splash_cerrar_cb(lv_event_t *e) { lv_obj_delete((lv_obj_t *)lv_event_get_user_data(e)); }
static void splash_timer_cb(lv_timer_t *t)
{
    lv_obj_t *o = (lv_obj_t *)lv_timer_get_user_data(t);
    if (lv_obj_is_valid(o)) lv_obj_delete(o);
    lv_timer_delete(t);
}
void ui_splash(const char *l1, const char *l2, bool fallo, uint32_t ms)
{
    lv_obj_t *o = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(o);
    lv_obj_set_size(o, 466, 466);
    lv_obj_set_pos(o, 0, 0);
    lv_obj_set_style_radius(o, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(o, lv_color_hex(fallo ? 0x5A1018 : 0x1A1646), 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_add_flag(o, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(o, splash_cerrar_cb, LV_EVENT_CLICKED, o);
    lv_obj_t *a = texto_c(o, l1, 0xFFE9A0, 3, 360);
    lv_obj_align(a, LV_ALIGN_CENTER, 0, -30);
    if (l2 && l2[0]) { lv_obj_t *b = texto_c(o, l2, 0xE4ECFF, 2, 340); lv_obj_align(b, LV_ALIGN_CENTER, 0, 40); }
    lv_timer_t *t = lv_timer_create(splash_timer_cb, ms, o);
    (void)t;
}

/* estilo del mundo (la muestra del estilo nuevo): se redibuja el fondo */
void ui_set_style(int estilo) { art_set_estilo(estilo); s_fondo_periodo = -1; s_fondo_escena = -1; }

/* (v96) cuáles imágenes están en pantalla: se recorren todas las capas y se anotan sus fuentes (para no soltar un ícono en uso) */
#define N_EN_USO 1600
static const void **s_en_uso;
static int s_en_uso_n;
static void en_uso_juntar(lv_obj_t *o)
{
    if (!o || s_en_uso_n >= N_EN_USO) return;
    if (lv_obj_check_type(o, &lv_image_class)) { const void *src = lv_image_get_src(o); if (src) s_en_uso[s_en_uso_n++] = src; }
    const uint32_t n = lv_obj_get_child_count(o);
    for (uint32_t i = 0; i < n; i++) en_uso_juntar(lv_obj_get_child(o, (int32_t)i));
}
static int en_uso_cmp(const void *a, const void *b) { const uintptr_t x = (uintptr_t)*(const void *const *)a, y = (uintptr_t)*(const void *const *)b; return x < y ? -1 : (x > y); }
static bool en_uso(const void *src) { return s_en_uso_n >= N_EN_USO || bsearch(&src, s_en_uso, (size_t)s_en_uso_n, sizeof *s_en_uso, en_uso_cmp) != NULL; }
static uint32_t iconos_soltar(void)
{
    if (!s_en_uso) s_en_uso = (const void **)malloc(sizeof(void *) * N_EN_USO);
    if (!s_en_uso) return 0;
    s_en_uso_n = 0;
    en_uso_juntar(lv_screen_active()); en_uso_juntar(lv_layer_top()); en_uso_juntar(lv_layer_sys());
    qsort(s_en_uso, (size_t)s_en_uso_n, sizeof *s_en_uso, en_uso_cmp);
    const uint32_t b = art_iconos_soltar(en_uso);
    lv_image_cache_drop(NULL);
    return b;
}
uint32_t ui_soltar_iconos(void) { return iconos_soltar(); } /* (pruebas) */
/* (pruebas) bytes en los textos y en los íconos grandes del panel abierto */
uint32_t ui_mem_panel(int que)
{
    uint32_t b = 0;
    if (que == 0) for (int i = 0; i < s_ptxt_n; i++) b += s_ptxt[i].capacidad * 4u;
    else if (que == 1) for (int i = 0; i < s_pobj_n; i++) b += s_pobj[i].capacidad * 4u;
    else if (que == 2) b = (uint32_t)s_ptxt_n;
    else if (que == 3) b = (uint32_t)s_pobj_n;
    return b;
}
void ui_update(const pet_t *p, uint32_t utc)
{
    /* (fw 104) el papelito del bosque que espera: se abre a pantalla completa cuando no hay otro panel, juego ni pecera */
    if (s_papel_k >= 0 && (int32_t)(s_ms - s_papel_desde) >= 0 && s_panel == PANEL_NINGUNO && s_juego == JG_NADA && !s_pecera && p && !p->dormida) {
        const int k = s_papel_k;
        s_papel_k = -1;
        panel_historia(NARR_PAPELITOS, k);
    }
#ifdef ESP_PLATFORM
    { /* vigilancia de memoria: si la PSRAM o la RAM interna se acaban, LVGL se cae dibujando (malloc NULL en lv_draw_sw_mask_radius_init); se suelta la caché de imágenes antes */
        static uint32_t t_mem;
        const uint32_t ahora = lv_tick_get();
        if (ahora - t_mem > 1500u) {
            t_mem = ahora;
            const size_t ps = heap_caps_get_free_size(MALLOC_CAP_SPIRAM), ri = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
            /* (v88: ya no se sueltan aquí los fondos de las otras horas cuando baja la PSRAM: si LVGL tenía uno en pantalla
             * quedaba apuntando a memoria liberada. Solo se sueltan si de verdad falta memoria al armar un fondo nuevo.) */
            /* (v101) la RAM interna anda en ~24 KB jugando normal (el 13-nov, toda la sesión): con el límite de 28 KB la caché de imágenes
             * se botaba cada 1,5 s y todo se volvía a decodificar (más lento y más batería), sin liberar RAM interna (la caché vive en
             * la PSRAM). Ahora: PSRAM baja, o RAM interna de verdad crítica (< 16 KB) y como mucho cada 10 s */
            static uint32_t t_drop;
            const bool critica = ps < 900u * 1024u || (ri < 16u * 1024u && (!t_drop || ahora - t_drop > 10000u));
            if (critica) {
                t_drop = ahora | 1u;
                extern void hal_evento(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
                lv_image_cache_drop(NULL);
                static uint32_t t_iconos; /* (v96) y los íconos guardados que no están en pantalla (a lo más cada 10 s) */
                if (!t_iconos || ahora - t_iconos > 10000u) {
                    t_iconos = ahora | 1u;
                    const uint32_t b = iconos_soltar();
                    if (b > 64u * 1024u) hal_evento("memoria baja: se soltaron %u KB de íconos guardados (PSRAM %u KB)", (unsigned)(b / 1024), (unsigned)(ps / 1024));
                }
                static uint32_t ult_aviso; /* al resumen de eventos raros, a lo más uno por minuto */
                if (!ult_aviso || ahora - ult_aviso > (s_pantalla_visible ? 60000u : 1800000u)) { ult_aviso = ahora | 1u; hal_evento("memoria baja: PSRAM %u KB, RAM interna %u B; se soltó la caché de imágenes (panel %d)", (unsigned)(ps / 1024), (unsigned)ri, (int)s_panel); }
            }
        }
    }
#endif
    uint32_t antes = s_ultimo_t;
    s_pet = p;
    s_ultimo_t = utc;
    s_nino = p->modo_nino == MODO_INFANTE;
    s_suave = p->modo_nino != 0;

    /* zona: si cambió mientras la mirabas, se va caminando por un borde y entra por el otro */
    if (s_escena < 0 || p->etapa == STAGE_HUEVO) s_escena = p->escena;
    else if (s_po_tele && (int)p->escena != s_escena) s_escena = -2; /* portal: aparece directo en la zona nueva */
    else if (s_lw_directo && (int)p->escena != s_escena) { s_lw_directo = false; s_escena = -2; } /* (v98) en la nube, o de excursión */
    else if (s_lw_directo && (int)p->escena == s_escena) s_lw_directo = false;
    else if (s_zona_pend >= 0 && (s_mascota.modo != M_ZONA_SALE || s_ms > s_mascota.modo_hasta)) {
        s_zona_pend = -1; /* el viaje se cortó: llega directo */
        s_escena = -2;
    }
    if (s_escena == -2) {
        s_escena = p->escena;
        esconder_fauna();
        refrescar_fondo(p, utc);
        actualizar_jardin(p);
        s_mascota.x = s_mascota.tx = LXY(LUGAR_CLARO)[0];
        s_mascota.y = s_mascota.ty = LXY(LUGAR_CLARO)[1];
        mascota_modo(M_LIBRE, 0);
    } else if ((int)p->escena != s_escena && s_zona_pend < 0) {
        bool mirando = antes && utc - antes < 30 && !p->dormida && s_mascota.visible;
        if (s_escena == ESC_COLINA && p->escena == ESC_MIRADOR && !s_interior && lw_nube_desde_colina(mirando)) {
            /* (v98) al mirador desde el cerro: la nube baja, se suben y suben juntas (ui_zonas.inc); al llegar arriba, aparece allá */
        } else if ((p->escena == ESC_ISLA || s_escena == ESC_ISLA) && s_escena >= 0 && !lago_congelado(p)) { /* a la isla (o de vuelta) se va en el bote (con el lago helado, caminando) */
            if (mirando) cruce_empezar(p, p->escena == ESC_ISLA);
            s_escena = p->escena;
            esconder_fauna();
            refrescar_fondo(p, utc);
            actualizar_jardin(p);
            int lu = p->escena == ESC_ISLA ? LUGAR_SENDERO : LUGAR_CLARO; /* llega al muelle */
            s_mascota.x = s_mascota.tx = LXY(lu)[0];
            s_mascota.y = s_mascota.ty = LXY(lu)[1];
            mascota_modo(M_LIBRE, 0);
        } else if (mirando) {
            s_zona_pend = p->escena;
            s_zona_lado = lado_hacia(s_escena, (int)p->escena);
            punto_borde(s_escena, s_zona_lado, &s_mascota.tx, &s_mascota.ty);
            mascota_modo(M_ZONA_SALE, 60000);
        } else {
            s_escena = p->escena;
            esconder_fauna();
            s_mascota.x = s_mascota.tx = LXY(p->lugar)[0];
            s_mascota.y = s_mascota.ty = LXY(p->lugar)[1];
        }
    }
    bool mirando = antes && utc - antes < 30;
    if (s_quitar_tipo != QUITAR_NADA && ((int32_t)(s_ms - s_quitar_hasta) > 0 || s_panel != PANEL_NINGUNO || s_escena != (int)p->escena)) ocultar_quitar();
    if (antes && utc - antes > 20u * 60u && (s_resumen_n > 0 || s_res_n > 0) && s_panel == PANEL_NINGUNO && p->edad_dueno && !s_bienv_activa && !s_pecera)
        panel_resumen(); /* volviste después de un rato: lo que pasó */
    /* una carta sin abrir: llega su mensajero, ella la recoge y te la trae (espera a que la pantalla quede libre) */
    magia_update(p);
    /* dormida con casita: adentro, en su camita (si la estabas mirando, camina hasta la puerta) */
    if (duerme_en_casita(p) && !s_interior && (!mirando || s_escena != ESC_PRADO)) entrar_casa(p);
    if (s_casa_visita) { /* visitando la casita: ella espera adentro (si se duerme, a su camita; si el huevo nace o cambia la escena, se sale) */
        if (p->etapa == STAGE_HUEVO) { s_casa_visita = false; salir_casa(p); } /* nació el huevo: sale a conocerlo al prado */
        else if (!s_interior || s_escena != ESC_PRADO) s_casa_visita = false;
        else if (p->dormida) { s_casa_visita = false; s_mascota.x = s_mascota.tx = CAMA_X; s_mascota.y = s_mascota.ty = CAMA_Y; }
        else { s_mascota.x = s_mascota.tx = VISITA_X; s_mascota.y = s_mascota.ty = VISITA_Y; }
    }
    if (!duerme_en_casita(p) && s_interior && !mirando && !s_casa_visita) salir_casa(p);
    if (s_resp_aviso) ocultar(s_resp_aviso, !(pet_backup_due(p, utc) && !s_nino && !s_pecera && p->edad_dueno));
    refrescar_fondo(p, utc);
    actualizar_jardin(p);
    actualizar_flechas(p);
    actualizar_bote(p);
    actualizar_hoyo(p);
    actualizar_huevo_fam(p);
    actualizar_pastel(p);
    actualizar_vecino(p, mirando);

    /* primera vez: bienvenida y tu fecha de nacimiento, luego el nombre; con huevo no hay barra de botones */
    /* si el Libro de bienvenida se cerró sin pasar por "seguir" (la X, o la pantalla se apagó y se encendió), la configuración igual continúa:
     * si no, quedaba el huevo en el prado sin fecha, sin aspecto y sin nombre */
    if (s_libro_inicio && s_panel == PANEL_NINGUNO && !p->edad_dueno) { s_libro_inicio = false; inicio_huevo(); }
    bool bloqueante = s_panel == PANEL_FECHA || s_panel == PANEL_HORA || s_panel == PANEL_RESTAURAR || s_libro_inicio ||
                    (!p->edad_dueno && (s_panel == PANEL_ASPECTO || s_panel == PANEL_GENERO));
    if (!p->edad_dueno) { if (!s_bienv_activa && !bloqueante) bienvenida(); }
    else if ((s_hora_dudosa || pet_clock_behind(p, utc)) && !bloqueante && !s_bienv_activa && !s_arrancando && s_pantalla_visible &&
             (s_hora_dudosa || s_hora_pidio == 0 || utc - s_hora_pidio > 120u)) { s_hora_pidio = utc; g_ui_paso = 21; panel_hora(); g_ui_paso = 22; }
    /* (sin insistir cada segundo; v88: ni durante el arranque ni con la pantalla apagada. Antes, con la pantalla apagada, la pregunta de la hora se
     *  volvía a armar cada 2 minutos y trababa el ciclo 2 o 3 segundos: eso eran los tirones cada 121 s del registro) */
    else if (!p->pais && !s_pais_enviado && s_panel == PANEL_NINGUNO && !s_bienv_activa) { /* ¿en qué país vive? (las estaciones y sus fiestas) */
        if (s_suave) { s_pais_enviado = true; accion(UI_ACCION_PAIS, PAIS_CHILE); } /* niño e infante: Chile; se cambia en Más ajustes */
        else panel_pais(false);
    }
    else if (s_asp_listo && p->pais && p->etapa == STAGE_HUEVO && s_panel == PANEL_NINGUNO && !s_bienv_activa) { /* ya hay reloj: el huevo llega al prado */
        s_asp_listo = false; s_asp_enviado = true;
        accion(UI_ACCION_ASPECTO, s_asp_e);
        accion(UI_ACCION_GENERO, s_asp_gen);
        accion(UI_ACCION_HUEVO_EMPIEZA, 0); /* (1.0.10) desde aquí cuenta sus 5 minutos */
        s_nace_desde = s_ms ? s_ms : 1; s_nace_paso = 0;   /* (107) y se abre a la vista, no al instante */
        globo(ICO_HUEVO, "TU HUEVO LLEGÓ AL PRADO", 2800);
    }
    else if (s_nace_desde && p->etapa == STAGE_HUEVO) { /* (1.0.10) los 5 minutos: se mece, se raja; las caricias le dan calor */
        const uint32_t r = pet_hatch_remaining(p, utc);
        if (s_nace_paso == 0 && (int32_t)(s_ms - s_nace_desde) >= 4000) {
            s_nace_paso = 1;
            globo(ICO_HUEVO, "NACERÁ EN UNOS 5 MINUTOS. ACARÍCIALO PARA DARLE CALOR", 4500);
        } else if (s_nace_paso == 1 && r <= PET_HUEVO_SEGUNDOS / 2) {
            s_nace_paso = 2;
            globo(ICO_HUEVO, "¡ALGO SE MUEVE ADENTRO!", 2600);
        }
    }
    else if (s_nace_desde && p->etapa != STAGE_HUEVO) { /* (1.0.10) el motor dice que nació: ahora se abre a la vista */
        const int hx = (int)s_mascota.x, hy = cabeza_y(&s_mascota);
        s_nace_desde = 0;
        for (int i = 0; i < 8; i++) flotar(ICO_ESTRELLA, NULL, 0, hx - 70 + i * 20, hy + 26, (uint32_t)i * 110);
        mascota_modo(M_SALTA, 1600);
        globo(ICO_CORAZON, "¡HOLA!", 2600);
        s_nace_fin = s_ms + 2600;   /* el nombre espera a que se vea quién salió */
    }
    else if (s_inicio_cfg && !s_asp_listo && p->etapa != STAGE_HUEVO && s_panel == PANEL_NINGUNO && !s_bienv_activa &&
             (!s_nace_fin || (int32_t)(s_ms - s_nace_fin) >= 0)) { /* nombre → color → modo, antes de verla en el prado */
        if (s_inicio_cfg == 1) panel_nombre(); else if (s_inicio_cfg == 2) panel_color(); else panel_modo_inicio();
    }
    else if (p->pais && p->etapa == STAGE_HUEVO && !p->aspecto_ok && !s_asp_enviado && s_panel != PANEL_ASPECTO && s_panel != PANEL_GENERO && s_panel != PANEL_PAIS && !bloqueante) {
        s_asp_paso = 0; s_asp_e = -1; s_asp_m = -1; s_asp_c = 0;
        panel_aspecto(); /* cómo será: especie, mezcla y colores */
    }
    /* (107) sin nombre no se sigue: antes solo se preguntaba mientras era huevo, así que si el huevo se abría
     * primero (o el orbe se apagaba a medio inicio), la mascota quedaba para siempre «(sin nombre)» */
    else if (p->pais && !p->nombre[0] && !s_nace_desde && s_panel != PANEL_NOMBRE && s_panel != PANEL_ASPECTO && s_panel != PANEL_GENERO &&
             s_panel != PANEL_PAIS && !bloqueante) panel_nombre();
    if (p->aspecto_ok) s_asp_enviado = false;
    if (p->pais) s_pais_enviado = false;
    /* dormida no se le puede dar de comer, jugar ni bañar: solo queda "Más" (luz, despertarla) */
    /* tres botones (comer, jugar, más); el baño aparece solo cuando está sucia */
    bool sucia = pet_need(p, NEED_HIGIENE) < 60 || p->barro;
    for (int i = 0; i < 4; i++)
        ocultar(s_dock[i], p->etapa == STAGE_HUEVO || !p->edad_dueno || s_interior || (i < 3 && p->dormida) || s_juego != JG_NADA || (i == 2 && !sucia) || s_esponja_on || s_picnic_on || s_fie_tipo >= 0 || s_portal || s_panel == PANEL_FINAL || s_panel == PANEL_CINE); /* (en una fiesta o con un menú de disco abierto, sin botones) */
    int nb = sucia ? 4 : 3, k = 0;
    for (int i = 0; i < 4; i++) {
        if (i == 2 && !sucia) continue;
        { /* en arco como en la pecera: el del centro abajo y los de los lados más arriba */
            const float off = p->dormida ? 0.0f : ((float)k - (float)(nb - 1) / 2.0f) * (nb == 4 ? 76.0f : 84.0f);
            const float r = off / 84.0f;
            lv_obj_set_pos(s_dock[i], CX - 36 + (int)off, 396 - (int)(22.0f * r * r));
        }
        k++;
    }
    actualizar_juguetes(p);
    actualizar_pecera(p, utc);
    {
        static uint32_t firma = 0xFFFFFFFFu;
        uint32_t f = firma_rincon(p);
        if (f != firma) { firma = f; if (s_panel == PANEL_RINCON) panel_rincon(); }
        /* recién nacida: ¿de qué color? (una vez; aspecto_ok pasa a 2 al elegirlo) */
        if (p->etapa == STAGE_BEBE && p->aspecto_ok == 1 && p->edad_dueno && s_panel == PANEL_NINGUNO && !s_bienv_activa && !s_inicio_cfg &&
            (!s_color_pend || (int32_t)(s_ms - s_color_pend) > 0)) { s_color_pend = 0; panel_color(); }
        cine_revisar();
        libro_revisar();
        static uint32_t fb;
        uint32_t g = firma_bolsa(p);
        if (g != fb) { fb = g; if (s_panel == PANEL_BOLSA) panel_bolsa(); else if (s_panel == PANEL_OBJETO && s_obj_sel >= 0) panel_objeto(s_obj_sel); }
    }

    /* suciedad visible: en el prado, cacas para recoger; en ella, manchas de tierra (justo cuando aparece la bañera) */
    float hig = pet_need(p, NEED_HIGIENE);
    int n = pet_poops_here(p) < N_CACAS ? pet_poops_here(p) : N_CACAS; /* las que dejó en esta zona */
    if (s_interior || p->guardiana || s_escena == ESC_ISLA) n = 0; /* la isla y la Guardiana: el bosque lo mantiene limpio */
    static uint32_t s_caca_fg;
    const bool al_frente = s_ms - s_caca_fg > 1000u; /* cada segundo: la caca queda por encima de cualquier objeto (varita, cofre…) para poder recogerla */
    if (al_frente) s_caca_fg = s_ms;
    for (int i = 0; i < N_CACAS; i++) {
        ocultar(s_cacas[i], i >= n);
        if (i >= n) continue;
        caca_sitio(i, lago_congelado(p));
        lv_obj_set_pos(s_cacas[i], s_caca_x[i] - 18, s_caca_y[i] - 30);
        if (al_frente && lv_obj_get_index(s_cacas[i]) != (int32_t)lv_obj_get_child_count(lv_obj_get_parent(s_cacas[i])) - 1) lv_obj_move_foreground(s_cacas[i]);
    }

    criatura_t *c = &s_mascota;
    c->pose.sucio = p->etapa == STAGE_HUEVO ? 0 : (hig < 30 ? 3 : (hig < 45 ? 2 : (hig < 60 ? 1 : 0)));
    if (p->barro && c->pose.sucio < 2) c->pose.sucio = 2;
    c->pose.capa = p->guardiana && p->etapa != STAGE_HUEVO && !(s_rito_on && s_ms - s_rito_desde < RITO_FLASH); /* en el rito, la capa aparece con la luz */
    if (c->pose.capa) c->pose.sucio = 0;
    c->pose.especie = pet_look(p);
    for (int s = 0; s < RANURA_COUNT; s++) c->pose.ropa[s] = pet_worn_raw(p, s);
    c->pose.forma = p->etapa == STAGE_ADULTA ? p->forma : 0;
    c->pose.etapa = p->etapa;
    c->pose.escala = 4; /* (en el picnic, un escalón más grande) un 20 % más chica que antes (era 5): el arte es de píxeles y solo admite escalas enteras */
    c->pose.enferma = p->enferma != 0;
    if (p->etapa == STAGE_HUEVO) {
        uint32_t r = pet_hatch_remaining(p, utc);
        c->grietas = r > 200 ? 0 : (r > 100 ? 1 : (r > 30 ? 2 : 3)); /* (1.0.10) el primer huevo también: con su reloj de 5 min */
    }

    /* visita que ya estaba al encender */
    if (p->visita_actual >= 0 && !s_visita.visible && !s_interior) {
        s_visita.pose.especie = p->visitantes[p->visita_actual].especie;
        s_visita.pose.etapa = STAGE_ADULTA;
        s_visita.pose.escala = 3;
        s_visita.amigo = 0;
        s_visita.x = LXY(p->lugar)[0] + 70; s_visita.y = LXY(p->lugar)[1];
        limitar(&s_visita.x, &s_visita.y);
        s_visita.tx = s_visita.x; s_visita.ty = s_visita.y;
        s_visita.modo = M_QUIETA;
        mostrar_criatura(&s_visita, true);
    }
    evo_revisar();
    ritmo_revisar();
    final_revisar();
}

/* ======================================================================
 * Eventos del motor
 * ==================================================================== */

static void mascota_modo(modo_t m, uint32_t ms)
{
    /* yéndose a otra zona: nada la interrumpe (si no, se quedaba a medio camino y sin flechas) */
    if (s_zona_pend >= 0 && s_mascota.modo == M_ZONA_SALE && m != M_ZONA_SALE) return;
    s_mascota.modo = m;
    s_mascota.modo_hasta = s_ms + ms;
}

static void encuentro(int r)
{
    criatura_t *v = &s_visita, *c = &s_mascota;
    int hx = (int)((v->x + c->x) / 2), hy = (cabeza_y(c) + cabeza_y(v)) / 2;
    switch (r) {
    case ENC_JUGARON:
        flotar(ICO_NOTA, NULL, 0, (int)c->x, cabeza_y(c), 0);
        flotar(ICO_NOTA, NULL, 0, (int)v->x, cabeza_y(v), 300);
        mascota_modo(M_SALTA, 1600);
        break;
    case ENC_PELEARON:
        flotar(ICO_ENOJO, NULL, 0, hx, hy, 0);
        flotar(ICO_ENOJO, NULL, 0, hx + 20, hy - 10, 250);
        globo(ICO_ENOJO, "¡GRRR!", 2500);
        break;
    case ENC_SE_ENAMORARON:
        for (int i = 0; i < 4; i++) flotar(ICO_CORAZON, NULL, 0, hx - 30 + i * 20, hy, (uint32_t)i * 180);
        globo(ICO_CORAZON, "¡AMOR!", 3000);
        break;
    case ENC_SE_ASUSTO:
        flotar(ICO_SUDOR, NULL, 0, (int)c->x + 20, cabeza_y(c), 0);
        mascota_modo(M_NIEGA, 900);
        break;
    default:
        flotar(-1, "...", C_BLANCO, hx, hy, 0);
        break;
    }
}

static void entrar_visita(int especie, int etapa, int forma, int escala)
{
    criatura_t *v = &s_visita, *c = &s_mascota;
    v->pose.especie = especie;
    v->pose.etapa = etapa < 1 ? 1 : etapa;
    v->pose.forma = forma;
    v->pose.escala = escala;
    bool desde_der = c->x < CX;
    v->x = desde_der ? W - 40 : 40;
    v->y = c->y;
    v->tx = c->x + (desde_der ? 85 : -85);
    v->ty = c->y;
    limitar(&v->tx, &v->ty);
    v->modo = M_CAMINA;
    v->encuentro = -1;
    v->clave = 0xFFFFFFFFu;
    mostrar_criatura(v, true);
}

static void trueno(void)
{
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, s_flash);
    lv_anim_set_exec_cb(&a, anim_opa);
    lv_anim_set_values(&a, LV_OPA_70, LV_OPA_TRANSP);
    lv_anim_set_duration(&a, 350);
    lv_anim_start(&a);
}

static const art_icono_t ICO_HALL[HALL_COUNT] = {ICO_FRUTA, ICO_MIEL, ICO_HIERBA, ICO_SEMILLA, ICO_PIEDRA, ICO_RAMITA, ICO_CHAPITA, ICO_HOJA_SECA, ICO_MUSGO};

void ui_show_event(const pet_t *p, const pet_event_t *e)
{
    s_pet = p;
    s_nino = p->modo_nino == MODO_INFANTE;
    s_suave = p->modo_nino != 0;
    criatura_t *c = &s_mascota;
    int hx = (int)c->x, hy = cabeza_y(c);

    /* lo que pasó mientras la pantalla estaba apagada o no estabas: solo se resume */
    bool atrasado = e->t + 120 < p->ultima_actualizacion;
    if (atrasado) {
        if (e->type == EV_HALLAZGO && e->a < HALL_COUNT) { if (s_resumen[e->a] < 99) s_resumen[e->a]++; s_resumen_n++; }
        char t[64];
        switch (e->type) { /* lo importante, en una línea */
        case EV_VECINO: if (e->a == VEC_LLEGA) { snprintf(t, sizeof t, "Vino %s a jugar", p->vecino.nombre); resumen_linea(ICO_AMIGOS, t); } break;
        case EV_PESCA: if (e->a != PESCA_NADA) { snprintf(t, sizeof t, "Pescó %s", pet_catch_name((pesca_t)e->a)); resumen_linea(ICO_PEZ, t); } break;
        case EV_HABILIDAD:
            if (e->b >= 2) { snprintf(t, sizeof t, "¡Aprendió a %s!", pet_skill_name((habilidad_t)e->a)); resumen_linea(e->a == HAB_NADAR ? ICO_NADAR : ICO_TREPAR, t); }
            break;
        case EV_TESORO: if (e->a == 1 && e->b >= 0) { snprintf(t, sizeof t, "Encontró %s", pet_piece_name((coleccion_t)(e->b / 8), e->b % 8)); resumen_linea(ICO_COFRE, t); } break;
        case EV_COLECCION: snprintf(t, sizeof t, "¡Completó sus %s!", pet_collection_name((coleccion_t)e->a)); resumen_linea(ICO_COFRE, t); break;
        case EV_CARTA_BOSQUE: resumen_linea(ICO_PAPELITO, e->b ? "Te trajo un papelito del bosque (está en el Libro)" : "Encontraron un papelito del bosque"); break; /* (fw 104) */
        case EV_PASEO_JUNTOS: snprintf(t, sizeof t, "Salieron a caminar: %d00 pasos juntos", (int)e->a); resumen_linea(ICO_HUELLA, t); break;
        case EV_HIELO: if (e->a == 1) resumen_linea(ICO_GOTA, "Su trocito de hielo se derritió"); break;
        case EV_RARO: if (e->b) { snprintf(t, sizeof t, "Vio %s", pet_rare_name((raro_t)e->a)); resumen_linea(ICO_ESTRELLA, t); } break;
        case EV_TRUCO: if (e->b == 2) { snprintf(t, sizeof t, "Aprendió a %s", pet_trick_name((truco_t)e->a)); resumen_linea(ICO_PATITA, t); } break;
        case EV_VINCULO: snprintf(t, sizeof t, "Ahora son %s", pet_bond_name((vinculo_nivel_t)e->a)); resumen_linea(ICO_CORAZON, t); break;
        case EV_MINI: {
            static const char *const R[MINI_COUNT] = {"Vio una lluvia de estrellas", "Vio la primera lluvia del otoño", "Vio la primera nieve del invierno", "Vio luciérnagas", "Vivió un día especial del sol", "Vio la luna llena", "Vio un eclipse de luna", "Vio un eclipse de sol"};
            resumen_linea(e->a == MINI_ESTRELLAS ? ICO_ESTRELLA_FUGAZ : e->a == MINI_LUNA || e->a == MINI_ECLIPSE_LUNAR ? ICO_LUNA : e->a == MINI_ECLIPSE_SOLAR ? ICO_SOL : e->a == MINI_SOL ? ICO_SOL : e->a == MINI_PRIMERA_NIEVE ? ICO_COPO : e->a == MINI_PRIMERA_LLUVIA ? ICO_LLUVIA : ICO_ESTRELLA, R[(unsigned)e->a < MINI_COUNT ? e->a : 0]);
            break;
        }
        case EV_CANCION: if (e->a == 0) resumen_linea(ICO_NOTA, "Encontró su canción favorita"); break;
        case EV_MELODIA: resumen_linea(ICO_NOTA, "Compuso su propia melodía en la cueva"); break;
        case EV_ESTACION: if (e->b == 1) { static const art_icono_t IE[4] = {ICO_FLOR, ICO_SOL, ICO_HOJA, ICO_COPO}; resumen_linea(IE[e->a & 3], "Llegó su estación favorita"); } break;
        case EV_GUSTO: snprintf(t, sizeof t, "Descubrió que le encanta %s", pet_activity_verb(e->a)); resumen_linea(ICO_CORAZON, t); break;
        case EV_PLAN: if (e->b == 1) { snprintf(t, sizeof t, "Cumplió su plan: %s", pet_plan_name(e->a)); resumen_linea(ICO_ESTRELLA, t); } break;
        case EV_HUMOR: snprintf(t, sizeof t, "Hoy amaneció %s", pet_humor_name(e->a)); resumen_linea(ICO_SOL, t); break;
        case EV_ANTOJO: break;
        case EV_ESCENA_NUEVA: snprintf(t, sizeof t, "Descubrió el camino %s", pet_scene_al((escena_t)e->a)); resumen_linea(ICO_FLECHA, t); break;
        default: break;
        }
        if (e->type == EV_VISITA_SE_VA && s_visita.visible && !s_visita.amigo) mostrar_criatura(&s_visita, false);
        if (e->type != EV_REENCUENTRO && e->type != EV_INVITACION && e->type != EV_AMIGO_VISITA && e->type != EV_ESCENA_NUEVA &&
            e->type != EV_CUMPLE && e->type != EV_SU_CUMPLE && e->type != EV_FECHA)
            return;
    }

    switch (e->type) {
    case EV_PIDE: {
        static const art_icono_t ICO[WANT_COUNT] = {ICO_ESTRELLA, ICO_PLATO, ICO_LUNA, ICO_DUCHA, ICO_PELOTA, ICO_MANO, ICO_JERINGA, ICO_TAZA, ICO_VASO};
        static const char *const TXT[WANT_COUNT] = {"", "¡HAMBRE!", "SUEÑO...", "¡DUCHA!", "¿JUGAMOS?", "¡MIMOS!", "ME SIENTO MAL",
                                                    "¡TENGO FRÍO! ALGO CALENTITO...", "¡QUÉ CALOR! ALGO FRESCO..."};
        if (e->a > 0 && e->a < WANT_COUNT) globo(ICO[e->a], TXT[e->a], 5000);
        break;
    }
    case EV_PENSAMIENTO:
        if (s_panel == PANEL_NINGUNO) pensar((emo_t)e->a, (causa_t)e->b);
        break;
    case EV_REACCION:
        switch (e->a) {
        case ACT_ALIMENTAR:
            if (e->b == -2) { mascota_modo(M_NIEGA, 1200); break; }
            if (e->b < 0 && s_ultima_comida == FOOD_BEBIDA_CALIENTE) { mascota_modo(M_NIEGA, 1200); globo(ICO_CALOR, "¡QUEMA! HACE CALOR", 3000); break; }
            if (e->b < 0 && s_ultima_comida == FOOD_BEBIDA_FRIA) { mascota_modo(M_NIEGA, 1200); globo(ICO_FRIO, "¡BRRR! ¡QUÉ FRÍO!", 3000); break; }
            if (e->b >= 0) {
                lv_image_set_src(s_objeto, art_icono(ico_comida(s_ultima_comida), 3));
                ocultar(s_objeto, false);
                mascota_modo(M_COME, 2400);
                if (e->b > 0) flotar(ICO_CORAZON, NULL, 0, hx, hy, 1800);
            } else {
                mascota_modo(M_NIEGA, 1200);
                flotar(ICO_NO, NULL, 0, hx + 24, hy, 0);
            }
            break;
        case ACT_LANZAR:
            s_lanzar_r = e->b < 0 ? -1 : e->b;
            if (e->b < 0) flotar(e->b == -1 ? ICO_ZETA : ICO_NO, NULL, 0, hx + 24, hy, 0);
            break;
        case ACT_INCLINAR:
            if (e->b == 0) break;
            s_desliza_r = e->b;
            flotar(e->b > 0 ? ICO_NOTA : ICO_SUDOR, NULL, 0, hx + 20, hy, 0);
            break;
        case ACT_TRUCO:
            if (e->b > 0) hacer_truco(s_truco, false);
            else if (e->b == 0) { flotar(ICO_PREGUNTA, NULL, 0, hx + 22, hy, 0); mascota_modo(M_DUDA, 1300); }
            else if (e->b == -1) { mascota_modo(M_NIEGA, 900); globo(ICO_ZETA, "¡YA ME CANSÉ DE PRACTICAR!", 2500); }
            else { mascota_modo(M_NIEGA, 1200); flotar(ICO_NO, NULL, 0, hx + 24, hy, 0); }
            break;
        case ACT_FIN_JUEGO:
            if (e->b == 1 && s_fr_pts >= 8) { flotar(ICO_FRUTA, NULL, 0, hx, hy - 10, 600); globo(ICO_FRUTA, "¡PREMIO: UNA FRUTA!", 2500); }
            break;
        case ACT_BANAR:
            if (e->b == -2) { mascota_modo(M_NIEGA, 1200); flotar(ICO_GOTA, NULL, 0, hx + 24, hy, 0); break; }
            mascota_modo(M_BANO, 2200);
            for (int i = 0; i < 5; i++) flotar(ICO_BURBUJA, NULL, 0, hx - 40 + i * 20, hy + 30, (uint32_t)i * 200);
            if (e->b < 0) flotar(ICO_SUDOR, NULL, 0, hx + 30, hy, 900);
            break;
        case ACT_CEPILLAR:
            mascota_modo(M_BANO, 1600);
            flotar(ICO_CEPILLO, NULL, 0, hx - 20, hy + 20, 0);
            if (e->b > 0) flotar(ICO_CORAZON, NULL, 0, hx + 20, hy, 500);
            break;
        case ACT_JUGAR:
            if (e->b > 0 && s_juego_pedido >= 0) { empezar_juego(s_juego_pedido); s_juego_pedido = -1; break; }
            if (s_juego_pedido >= 0 && e->b <= 0) { /* no quiso: que se entienda por qué */
                const char *por = e->b == -2 ? "¡AHORA NO QUIERO!" : (p->dormida ? NULL : (p->enferma ? "ME SIENTO MAL..." : "ESTOY MUY CANSAD{A}"));
                if (por) globo(e->b == -2 ? ICO_NO : (p->enferma ? ICO_TERMOMETRO : ICO_LUNA), s_nino ? NULL : por, 2500);
            }
            s_juego_pedido = -1;
            if (e->b > 0) { mascota_modo(M_JUEGA, 3600); ocultar(s_pelota, false); }
            else if (e->b == -2) { mascota_modo(M_NIEGA, 1400); flotar(ICO_NO, NULL, 0, hx + 24, hy, 0); }
            else { mascota_modo(M_NIEGA, 1200); flotar(p->dormida ? ICO_ZETA : ICO_SUDOR, NULL, 0, hx + 24, hy, 0); }
            break;
        case ACT_ACARICIAR: {
            int tx = s_toque_x ? s_toque_x : hx, ty = s_toque_y ? s_toque_y : hy;
            if (p->etapa == STAGE_HUEVO) { flotar(ICO_CORAZON, NULL, 0, hx, hy + 20, 0); mascota_modo(M_SALTA, 400); break; }
            if (p->dormida) { flotar(ICO_ZETA, NULL, 0, hx + 20, hy, 0); break; }
            if (e->b < 0 && s_gesto == GESTO_MUCHOS) { /* mareada */
                flotar(ICO_MAREO, NULL, 0, hx, hy - 10, 0);
                flotar(ICO_MAREO, NULL, 0, hx + 26, hy - 4, 300);
                mascota_modo(M_MAREADA, 1800);
                s_toque_x = s_toque_y = 0;
                break;
            }
            if (e->b > 0 && s_gesto == GESTO_ABRAZO) {
                flotar(ICO_ABRAZO, NULL, 0, hx, hy - 10, 0);
                for (int i = 0; i < 3; i++) flotar(ICO_CORAZON, NULL, 0, hx - 30 + i * 30, hy + 10, 200 + (uint32_t)i * 200);
                mascota_modo(M_ABRAZO, 1200);
                s_toque_x = s_toque_y = 0;
                break;
            }
            if (e->b > 0 && s_gesto == GESTO_MUCHOS) { /* cosquillas */
                flotar(ICO_NOTA, NULL, 0, hx - 14, hy, 0);
                flotar(ICO_NOTA, NULL, 0, hx + 14, hy, 250);
                mascota_modo(M_SALTA, 900);
                s_toque_x = s_toque_y = 0;
                break;
            }
            if (e->b > 0) {
                flotar(ICO_CORAZON, NULL, 0, tx - 12, ty - 10, 0);
                if (s_gesto != GESTO_TOQUE) flotar(ICO_CORAZON, NULL, 0, tx + 12, ty - 10, 250);
                mascota_modo(M_SALTA, s_gesto == GESTO_TOQUE ? 300 : 500);
            } else if (e->b == 0) {
                flotar(p->vinculo < 25 ? ICO_SUDOR : ICO_CORAZON, NULL, 0, tx, ty - 10, 0);
                if (p->vinculo < 25) mascota_modo(M_NIEGA, 600); /* todavía desconfía */
            } else {
                flotar(p->rasgo[TRAIT_TIMIDA] > 0.5f ? ICO_SUDOR : ICO_ENOJO, NULL, 0, tx, ty - 10, 0);
                mascota_modo(M_NIEGA, 900);
            }
            s_toque_x = s_toque_y = 0;
            break;
        }
        case ACT_LLAMAR:
            if (e->b > 0) { /* viene corriendo */
                c->tx = s_llamado_x; c->ty = s_llamado_y;
                mascota_modo(M_VA, 8000);
            } else if (e->b == 0) { /* duda: da unos pasos y se queda mirando */
                c->tx = (c->x + s_llamado_x) / 2; c->ty = (c->y + s_llamado_y) / 2;
                mascota_modo(M_VA, 3500);
                flotar(ICO_PREGUNTA, NULL, 0, hx + 22, hy, 0);
            } else {
                mascota_modo(M_NIEGA, 1200);
                flotar(p->dormida ? ICO_ZETA : ICO_NO, NULL, 0, hx + 24, hy, 0);
            }
            break;
        case ACT_MIMAR_VISITA:
            if (e->b < 0) { flotar(ICO_ENOJO, NULL, 0, hx + 20, hy, 0); globo(ICO_ENOJO, "¡Y YO QUÉ!", 2500); }
            break;
        case ACT_PESCAR:
            if (e->b < 0 || e->b == -2) { mascota_modo(M_NIEGA, 1200); flotar(ICO_NO, NULL, 0, hx + 24, hy, 0); }
            else if (e->b > 0 && s_pez_pedido && !s_pecera) s_pez_espera = s_ms | 1u; /* v90: primero camina al bote y sube; ahí se abre la pesca */
            s_pez_pedido = false;
            break;
        case ACT_INVITAR_VECINO:
            if (e->b > 0) { flotar(ICO_AMIGOS, NULL, 0, hx, hy - 10, 0); globo(ICO_AMIGOS, "¡VOY A LLAMARLO!", 2000); }
            else globo(ICO_LUNA, "AHORA NO PUEDE VENIR. ¡OTRO RATO!", 2500);
            break;
        case ACT_CURAR: flotar(ICO_JERINGA, NULL, 0, hx, hy, 0); break;
        case ACT_SACUDIR:
            s_sacude_desde = s_ms ? s_ms : 1;
            if (e->b > 0) { mascota_modo(M_SALTA, 900); flotar(ICO_NOTA, NULL, 0, hx, hy, 0); }
            else { mascota_modo(M_NIEGA, 900); flotar(ICO_SUDOR, NULL, 0, hx + 24, hy, 0); }
            break;
        case ACT_DESPERTAR:
            if (e->b < 0) flotar(ICO_ENOJO, NULL, 0, hx + 24, hy, 0);
            else flotar(ICO_SOL, NULL, 0, hx, hy, 0);
            break;
        default: break;
        }
        break;
    case EV_PLAN: if (e->b == 0) { char t[48]; snprintf(t, sizeof t, "QUIERO %s", pet_plan_name(e->a)); globo(ICO_FLECHA, t, 3000); } break;
    case EV_ANTOJO: {
        static const char *const T[4] = {"¡MÁS MIMOS!", "¡VAMOS A JUGAR!", "ESTOY ENOJADA", "TE PERDONO"};
        globo(e->a == 2 ? ICO_NUBE : ICO_CORAZON, T[e->a & 3], 3000);
        break;
    }
    case EV_SE_DURMIO: globo(ICO_LUNA, "BUENAS NOCHES", 2500); break;
    case EV_ENFERMO: globo(ICO_TERMOMETRO, "¡ACHÍS! ME SIENTO MAL", 4000); break;
    case EV_SANO: globo(ICO_CORAZON, "¡ME SIENTO MEJOR!", 3000); break;
    case EV_EVO_LISTA:
        s_evo_abrir = true; /* la pregunta sale en cuanto no haya nada abierto */
        break;
    case EV_CRECIO: {
        const char *txt = "¡CRECÍ!";
        if (e->a == STAGE_ADULTA) txt = e->b == FORMA_RADIANTE ? "¡EVOLUCIONÉ Y BRILLO!" : (e->b == FORMA_GRUNONA ? "EVOLUCIONÉ... HMPF" : "¡EVOLUCIONÉ!");
        globo(ICO_ESTRELLA, txt, 4500);
        for (int i = 0; i < 5; i++) flotar(ICO_ESTRELLA, NULL, 0, hx - 50 + i * 25, hy + 20, (uint32_t)i * 150);
        mascota_modo(M_SALTA, 2000);
        break;
    }
    case EV_VISITA_LLEGA:
        entrar_visita(p->visitantes[e->a].especie, STAGE_ADULTA, 0, 3);
        s_visita.amigo = 0;
        globo(ICO_AMIGOS, "¡VISITA!", 2000);
        break;
    case EV_ENCUENTRO:
        if (s_visita.visible && s_visita.modo == M_CAMINA) s_visita.encuentro = e->b;
        else encuentro(e->b);
        break;
    case EV_VISITA_SE_VA:
        if (s_visita.visible && !s_visita.amigo) {
            s_visita.modo = M_SALE;
            s_visita.tx = s_visita.x < CX ? 10 : W - 10;
            s_visita.ty = s_visita.y;
        }
        break;
    case EV_REGALO:
        flotar(ICO_FRUTA, NULL, 0, LXY(LUGAR_ARBOL)[0], LXY(LUGAR_ARBOL)[1] - 60, 0);
        if (e->b == 1) { /* sacudiste el reloj: se mueve el árbol y cae una fruta */
            for (int k = 0; k < 6; k++) salpicar(LXY(LUGAR_ARBOL)[0] - 30 + k * 12, LXY(LUGAR_ARBOL)[1] - 90, 0x5DBB63);
            globo(ICO_FRUTA, "¡CAYÓ UNA FRUTA DEL ÁRBOL!", 2500);
        }
        break;
    case EV_TRUCO:
        if (e->b == 2) {
            static const char *const T[TRUCO_COUNT] = {"¡APRENDÍ A HACERME {LA} DORMID{A}!", "¡APRENDÍ A DAR UNA VOLTERETA!", "¡APRENDÍ A DAR UNA VUELTA!", "¡APRENDÍ A SALUDAR!"};
            globo(ICO_TRUCOS[e->a % TRUCO_COUNT], T[e->a % TRUCO_COUNT], 3500);
            for (int i = 0; i < 4; i++) flotar(ICO_ESTRELLA, NULL, 0, hx - 45 + i * 30, hy + 10, (uint32_t)i * 150);
        } else if (e->b == 1 && s_juego == JG_NADA && c->modo == M_LIBRE && !p->dormida) hacer_truco(e->a % TRUCO_COUNT, true); /* lo hace sola */
        break;
    case EV_SIN_ITEM: {
        static const art_icono_t I[ITEM_COUNT] = {ICO_FRUTA, ICO_MIEL, ICO_HIERBA, ICO_SEMILLA, ICO_PIEDRA};
        globo(I[e->a % ITEM_COUNT], s_nino ? NULL : (e->a == ITEM_SEMILLA ? "NO HAY DE ESAS SEMILLAS. ¡HAY QUE BUSCAR!" : "NO QUEDA. ¡HAY QUE BUSCAR!"), 3000);
        break;
    }
    case EV_DECORO:
        if (s_panel == PANEL_RINCON) panel_rincon();
        flotar(ICO_ESTRELLA, NULL, 0, CX, 120, 0);
        break;
    case EV_HALLAZGO:
        if (e->a < 0 || e->a >= HALL_COUNT) break;
        if (e->b) { /* te lo trae */
            lv_image_set_src(s_objeto, art_icono(ICO_HALL[e->a], 3));
            ocultar(s_objeto, false);
            mascota_modo(M_TRAE, 3800);
            globo(ICO_HALL[e->a], (e->a == HALL_RAMITA || e->a == HALL_MUSGO) && !pet_has_decor(s_pet, DECOR_CASITA) ? (pet_materials(s_pet, NULL, NULL) ? "¡YA ALCANZA PARA LA CASITA! MIRA EN EL RINCÓN" : "¡PARA NUESTRA CASITA DE RAMITAS Y MUSGO!")
                                     : (e->a >= HALL_RAMITA ? "¡TE TRAJE ESTO! (ES UN TESORO)" : "¡PARA TI!"), 3500); /* la basura, con cariño */
        } else {
            flotar(ICO_HALL[e->a], NULL, 0, hx, hy - 10, 0);
            flotar(ICO_ESTRELLA, NULL, 0, hx + 22, hy - 20, 300);
        }
        break;
    case EV_ENSAYA_MELODIA: { /* en la cueva prueba los cristales: tres notas */
        uint8_t m[3] = {(uint8_t)(e->b % 6), (uint8_t)((e->b + 1 + e->a) % 6), (uint8_t)((e->b + 3) % 6)};
        tocar_cristales(m, 3, 520);
        mascota_modo(M_QUIETA, 2200);
        if (e->a == 1 || azar() % 2) globo(ICO_CRISTAL, s_nino ? NULL : UNA_DE("ESTOY PROBANDO LOS CRISTALES... ¿SUENA BONITO?", "CADA CRISTAL SUENA DISTINTO... ESTOY ENSAYANDO", "ME GUSTA CÓMO SUENAN ESTOS CRISTALES", "PRUEBO UNA NOTA, OTRA... ¡CASI!"), 3500);
        break;
    }
    case EV_MELODIA: { /* decidió su melodía */
        libro_pedir(6);
        uint8_t m[PET_MELODIA_NOTAS];
        const int n = pet_melody(p, m);
        tocar_cristales(m, n, 480);
        mascota_modo(M_SALTA, 4200);
        globo(ICO_NOTA, s_nino ? NULL : UNA_DE("¡COMPUSE MI PROPIA MELODÍA CON LOS CRISTALES!", "¡YA TENGO MI MELODÍA! ¡ESCUCHA!", "¡LA ENCONTRÉ! ¡ES MI MELODÍA!"), 5000);
        for (int k = 0; k < 4; k++) flotar(ICO_ESTRELLA, NULL, 0, hx - 36 + k * 24, hy - 20, (uint32_t)k * 180);
        confeti(hx, hy - 30, 12);
        break;
    }
    case EV_MINI: { /* mini evento de temporada */
        switch (e->a) {
        case MINI_ESTRELLAS:
            for (int k = 0; k < 6; k++) flotar(ICO_ESTRELLA_FUGAZ, NULL, 0, 80 + (int)(azar() % 300), 60 + (int)(azar() % 120), (uint32_t)k * 450);
            globo(ICO_ESTRELLA_FUGAZ, s_nino ? NULL : UNA_DE("¡MIRA! ¡LLUVIA DE ESTRELLAS!", "¡CUÁNTAS ESTRELLAS FUGACES! PIDE UN DESEO", "¡ESTA NOCHE CAEN ESTRELLAS!", "¡PIDE UN DESEO! ¡ESTÁN CAYENDO ESTRELLAS!", "¡QUÉ NOCHE! ¡TODO EL CIELO BRILLA!", "¡UNA, DOS, TRES ESTRELLAS FUGACES!"), 4500);
            break;
        case MINI_PRIMERA_LLUVIA:
            for (int k = 0; k < 4; k++) flotar(ICO_LLUVIA, NULL, 0, 100 + k * 80, 90 + (int)(azar() % 60), (uint32_t)k * 250);
            globo(ICO_LLUVIA, s_nino ? NULL : UNA_DE("¡LA PRIMERA LLUVIA DEL OTOÑO!", "¡HUELE A TIERRA MOJADA! ¡LLEGÓ LA LLUVIA!", "¡PLIC, PLIC! ¡ESTÁ LLOVIENDO!", "¡QUÉ RICO SONIDO! ¡LA PRIMERA LLUVIA!", "¡LLEGÓ LA LLUVIA! ¡A SALTAR EN LOS CHARCOS!"), 4000);
            break;
        case MINI_PRIMERA_NIEVE:
            for (int k = 0; k < 5; k++) flotar(ICO_COPO, NULL, 0, 90 + k * 65, 70 + (int)(azar() % 80), (uint32_t)k * 250);
            globo(ICO_COPO, s_nino ? NULL : UNA_DE("¡LA PRIMERA NIEVE DEL INVIERNO!", "¡MIRA, ESTÁ NEVANDO! ¡LA PRIMERA NIEVE!", "¡NIEVE! ¡NO PUEDO CREERLO!", "¡CAEN COPITOS! ¡QUÉ LINDO!", "¡TODO SE ESTÁ PONIENDO BLANCO!"), 4000);
            break;
        case MINI_LUCIERNAGAS:
            for (int k = 0; k < 8; k++) flotar(ICO_ESTRELLA, NULL, 0, 70 + (int)(azar() % 320), 120 + (int)(azar() % 180), (uint32_t)k * 300);
            globo(ICO_ESTRELLA, s_nino ? NULL : UNA_DE("¡LUCIÉRNAGAS! ¡MIRA CÓMO BRILLAN!", "¡HOY HAY NOCHE DE LUCIÉRNAGAS!", "¡CUÁNTAS LUCES! ¡SON LUCIÉRNAGAS!", "¡EL BOSQUE SE LLENÓ DE LUCECITAS!", "¡MIRA ESAS LUCES TAN CHIQUITAS!"), 4000);
            break;
        case MINI_LUNA:
            flotar(ICO_LUNA, NULL, 0, hx + 46, hy - 6, 0);
            flotar(ICO_ESTRELLA, NULL, 0, hx - 46, hy + 10, 300);
            globo(ICO_LUNA, s_nino ? NULL : UNA_DE("¡MIRA QUÉ LUNA LLENA!", "¡QUÉ LUNA TAN GRANDE! ¡ESTÁ LLENA!", "¡HOY HAY LUNA LLENA!", "¡QUÉ REDONDA Y BRILLANTE!", "¡MIRA LA LUNA! ¡ESTÁ ENORME!", "¡LUNA LLENA! ¡QUÉ NOCHE TAN CLARITA!"), 4500);
            break;
        case MINI_ECLIPSE_LUNAR:
            flotar(ICO_LUNA, NULL, 0, hx + 46, hy - 6, 0);
            globo(ICO_LUNA, s_nino ? NULL : UNA_DE("¡MIRA, LA LUNA SE ESTÁ OSCURECIENDO! ¡ECLIPSE!", "¡UN ECLIPSE DE LUNA! ¡QUÉ RARO Y LINDO!", "¡LA SOMBRA DE LA TIERRA TAPA LA LUNA!"), 5000);
            break;
        case MINI_ECLIPSE_SOLAR:
            flotar(ICO_SOL, NULL, 0, hx + 46, hy - 6, 0);
            globo(ICO_SOL, s_nino ? NULL : UNA_DE("¡ECLIPSE DE SOL! ¡NO LO MIRES DIRECTO!", "¡EL SOL SE ESTÁ TAPANDO! ¡ECLIPSE!", "¡EL DÍA SE PUSO RARO! ¡ES UN ECLIPSE!"), 5000);
            break;
        default: {
            static const char *const F[3] = {"¡HOY EL DÍA Y LA NOCHE DURAN LO MISMO!", "¡HOY ES EL DÍA MÁS CORTO DEL AÑO!", "¡HOY ES EL DÍA MÁS LARGO DEL AÑO!"};
            flotar(ICO_SOL, NULL, 0, hx + 46, hy + 6, 0);
            globo(ICO_SOL, s_nino ? NULL : F[e->b % 3], 4000);
            break;
        }
        }
        accion(UI_ACCION_SONAR, SND_RARO);
        break;
    }
    case EV_ESTACION: { /* cambió la estación */
        static const art_icono_t IE[4] = {ICO_FLOR, ICO_SOL, ICO_HOJA, ICO_COPO};
        static const char *const FAV[4] = {"¡LLEGÓ LA PRIMAVERA, MI ESTACIÓN FAVORITA!", "¡LLEGÓ EL VERANO, MI ESTACIÓN FAVORITA!", "¡LLEGÓ EL OTOÑO, MI ESTACIÓN FAVORITA!", "¡LLEGÓ EL INVIERNO, MI ESTACIÓN FAVORITA!"};
        static const char *const PEOR[4] = {"YA ES PRIMAVERA... A MÍ ME CUESTA UN POQUITO", "YA ES VERANO... ME CUESTA UN POQUITO", "YA ES OTOÑO... ME CUESTA UN POQUITO", "YA ES INVIERNO... ME CUESTA UN POQUITO"};
        const int es = e->a & 3;
        flotar(IE[es], NULL, 0, hx - 46, hy + 6, 0);
        if (e->b == 1) { flotar(ICO_CORAZON, NULL, 0, hx + 46, hy + 6, 250); globo(IE[es], s_nino ? NULL : FAV[es], 4000); mascota_modo(M_SALTA, 1500); }
        else if (e->b == 2) globo(IE[es], s_nino ? NULL : PEOR[es], 3500);
        break;
    }
    case EV_CANCION: { /* ¡es su canción! */
        static const char *const F[6] = {"¡ESA ES MI CANCIÓN!", "¡ES MI CANCIÓN FAVORITA! ¡LA RECONOCÍ!", "¡SABÍA QUE ERA ESA! ¡MI CANCIÓN!",
                                         "¡OTRA VEZ MI CANCIÓN!", "¡QUÉ ALEGRÍA, MI CANCIÓN OTRA VEZ!", "¡MI CANCIÓN! ¡NO PARES!"};
        flotar(ICO_NOTA, NULL, 0, hx - 50, hy + 6, 0);
        flotar(ICO_CORAZON, NULL, 0, hx + 50, hy + 6, 200);
        flotar(ICO_ESTRELLA, NULL, 0, hx - 36, hy + 26, 400);
        globo(ICO_NOTA, s_nino ? NULL : F[(e->a ? 3 : 0) + (int)(azar() % 3u)], 4000);
        mascota_modo(M_SALTA, 1500);
        break;
    }
    case EV_PIDE_CANCION: { /* te pide su canción */
        flotar(ICO_NOTA, NULL, 0, hx - 16, hy - 24, 0);
        flotar(ICO_NOTA, NULL, 0, hx + 16, hy - 30, 300);
        globo(ICO_NOTA, s_nino ? NULL : UNA_DE("¿PONEMOS MI CANCIÓN?", "¿ME PONES MI CANCIÓN?", "¡QUIERO BAILAR MI CANCIÓN!", "¿SUENA MI MÚSICA FAVORITA?", "¡AY, QUÉ GANAS DE MI CANCIÓN!", "¿ME PONES ESA MÚSICA QUE ME GUSTA?"), 4500);
        break;
    }
    case EV_GUSTO: { /* descubrió cuál es su actividad favorita: te lo cuenta */
        char t[96];
        snprintf(t, sizeof t, UNA_DE("¡CREO QUE LO QUE MÁS ME GUSTA ES %s!", "¡ME ENCANTA %s! ¡GRACIAS!", "¡MI COSA FAVORITA ES %s!", "¡NO PARO DE PENSAR EN %s!", "¡CON %s SOY MUY FELIZ!"), pet_activity_verb(e->a));
        mayusculas(t);
        flotar(ICO_CORAZON, NULL, 0, hx, hy - 20, 0);
        flotar(ICO_ESTRELLA, NULL, 0, hx + 20, hy - 30, 250);
        globo(ICO_CORAZON, s_nino ? NULL : t, 3500);
        break;
    }
    case EV_MUSICA: { /* suena música cerca del reloj: lo nota y le encanta */
        static const char *const F[9] = {
            "¡QUÉ MÚSICA TAN SUAVE! ME ENCANTA", "¡ME ENCANTA LO QUE SUENA! ¡QUÉ TRANQUILA!", "¡QUÉ LINDA CANCIÓN PARA MECERSE!",
            "¡ME ENCANTA LO QUE SUENA!", "¡QUÉ LINDA MÚSICA! ¡A BAILAR!", "¡ESA CANCIÓN ES MI FAVORITA!",
            "¡ESTA SÍ QUE ME HACE BAILAR!", "¡QUÉ RITMO! ¡ME ENCANTA!", "¡MÁS RÁPIDO, MÁS RÁPIDO!"};
        flotar(ICO_NOTA, NULL, 0, hx - 20, hy - 20, 0);
        flotar(ICO_CORAZON, NULL, 0, hx + 20, hy - 30, 250);
        if (!s_nino) globo(ICO_NOTA, F[e->a % 9], 3500); else globo(ICO_NOTA, NULL, 3500);
        break;
    }
    case EV_COMIDA_EST: {
        if (e->a < 0 || e->a >= PET_COMIDAS_EST) break;
        art_icono_t ic = (art_icono_t)(ICO_CE_FRUTILLA + e->a);
        if (e->b) { /* te la trae */
            lv_image_set_src(s_objeto, art_icono(ic, 3));
            ocultar(s_objeto, false);
            mascota_modo(M_TRAE, 3800);
            char t[80];
            snprintf(t, sizeof t, "¡ENCONTRÉ %s PARA TI!", pet_seasonal_food_info(e->a)->nombre);
            globo(ic, t, 3500);
        } else {
            flotar(ic, NULL, 0, hx, hy - 10, 0);
            flotar(ICO_ESTRELLA, NULL, 0, hx + 22, hy - 20, 300);
        }
        break;
    }
    case EV_REGALO_VUELTA: evento_regalo(e, hx, hy); break;
    case EV_NIEBLA:
        if (e->a == 1) globo(ICO_NUBE, "¡QUÉ NIEBLA! SÓPLALE AL ORBE O BÁRRELA CON EL DEDO", 4500);
        else if (e->b) { mascota_modo(M_SALTA, 1500); globo(ICO_SOL, "¡SE FUE LA NIEBLA!", 2500); }
        break;
    case EV_OFRENDA: {
        int id = (e->a & 0x7F) | (e->b & 1) << 7;
        if (s_escena == ESC_PRADO && !s_interior && !p->dormida) {
            lv_image_set_src(s_objeto, art_icono(ICO_OBJETO(id), 3));
            ocultar(s_objeto, false);
            s_ofrenda_llego = false;
            mascota_modo(M_OFRENDA, 9000);
        } else flotar(ICO_HOJA, NULL, 0, CX, 150, 0);
        globo(ICO_BOSQUE_DEV, (e->b & 2) ? "UNA OFRENDA PARA EL BOSQUE. MAÑANA NOS VA A AGRADECER" : "UNA OFRENDA PARA EL BOSQUE", 3500);
        break;
    }
    case EV_HABILIDAD_USO: {
        int esp = e->a & 7, k = e->b & 1;
        mascota_modo(M_SALTA, 1600);
        for (int i = 0; i < 5; i++) flotar(i == 2 ? ICO_HABILIDAD[esp][k] : ICO_ESTRELLA, NULL, 0, hx - 48 + i * 24, hy - 10 - (i & 1) * 12, (uint32_t)i * 110);
        char t[48];
        snprintf(t, sizeof t, "¡%s!", pet_ability_name(esp, k));
        for (char *c = t; *c; c++) if (*c >= 'a' && *c <= 'z') *c = (char)(*c - 32);
        globo(ICO_HABILIDAD[esp][k], t, 2500);
        accion(UI_ACCION_SONAR, SND_TESORO);
        if (esp == 7 && k == 1) { s_juego_pedido = JUEGO_ESCONDIDAS; accion(ACT_JUGAR, JUEGO_ESCONDIDAS); } /* ella se esconde */
        break;
    }
    case EV_BENDICION:
        mascota_modo(M_SALTA, 2500);
        globo(e->a ? ICO_ESTRELLA : ICO_SOL, e->a ? "LAS LUCIÉRNAGAS VINIERON A DAR LAS GRACIAS" : "EL BOSQUE AGRADECE TU OFRENDA: ¡UN DÍA DE SOL!", 4000);
        break;
    case EV_OBJETO:
    case EV_OBJETO_USO: evento_objeto(e, hx, hy); break;
    case EV_PASEO: /* caminaron harto: te trae algo de lo que vio */
        if (e->a == -1) { mascota_modo(M_SALTA, 2500); globo(ICO_PATITA, s_nino ? NULL : UNA_DE("¡QUÉ PASEO! MIRA LO QUE ENCONTRÉ", "¡MIRA LO QUE TRAJE DEL PASEO!", "¡FUE UN PASEO LINDO! ¡MIRA!", "¡ENCONTRÉ ALGO PARA TI!", "¡TRAJE UNA SORPRESA!", "¡MIRA, ESTO ES PARA TI!"), 3000); break; }
        if (e->a < 0 || e->a >= HALL_COUNT) break;
        mascota_modo(M_SALTA, 2500);
        globo(ICO_PATITA, s_nino ? NULL : UNA_DE("¡QUÉ PASEO! MIRA LO QUE ENCONTRÉ", "¡MIRA LO QUE TRAJE DEL PASEO!", "¡FUE UN PASEO LINDO! ¡MIRA!", "¡ENCONTRÉ ALGO PARA TI!"), 4000);
        flotar(ICO_HALL[e->a], NULL, 0, hx, hy - 10, 300);
        flotar(ICO_ESTRELLA, NULL, 0, hx + 22, hy - 20, 600);
        break;
    case EV_TEMPORIZADOR:
        alarma_mostrar(AL_TIEMPO, 0, e->t);
        break;
    case EV_ACTIVIDAD:
        if (e->a == ACTV_OLFATEAR) flotar(-1, "?", C_BLANCO, hx + 26, hy, 600);
        break;
    case EV_REENCUENTRO:
        mascota_modo(M_SALTA, 3000);
        for (int i = 0; i < 6; i++) flotar(ICO_CORAZON, NULL, 0, hx - 60 + i * 24, hy + 10, (uint32_t)i * 160);
        globo(ICO_CORAZON, "¡VOLVISTE! ¡TE EXTRAÑÉ!", 3500);
        break;
    case EV_TRUENO: trueno(); break;
    case EV_NACIO: {
        if (s_inicio_cfg) break; /* el primer inicio: primero nombre, color y modo (el saludo viene al final) */
        char t[40];
        snprintf(t, sizeof t, "¡HOLA! SOY %s", p->nombre[0] ? p->nombre : "YO");
        globo(ICO_CORAZON, t, 5000);
        for (int i = 0; i < 6; i++) flotar(ICO_ESTRELLA, NULL, 0, hx - 60 + i * 24, hy + 30, (uint32_t)i * 120);
        mascota_modo(M_SALTA, 1500);
        s_color_pend = s_ms + 3500; /* después del hola: ¿de qué color? */
        libro_pedir(1);
        break;
    }
    case EV_CLIMA: {
        static const char *const T[] = {"¡SALIÓ EL SOL!", "SE NUBLÓ...", "¡LLUEVE!", "¡QUÉ VIENTO!", "¡NIEVA!", "¡TORMENTA!", "¡QUÉ FRÍO!"};
        static const art_icono_t I[] = {ICO_SOL, ICO_NUBE, ICO_LLUVIA, ICO_VIENTO, ICO_COPO, ICO_RAYO, ICO_FRIO};
        if (e->a >= 0 && e->a < CLIMA_COUNT && !p->dormida) {
            if (e->a == CLIMA_DESPEJADO && s_fondo_periodo == PERIODO_NOCHE) globo(ICO_LUNA, "SE DESPEJÓ EL CIELO", 3000); /* de noche no sale el sol */
            else globo(I[e->a], T[e->a], 3000);
        }
        break;
    }
    case EV_FRASE:
        if (s_pecera) globo(ICO_CORAZON, pet_phrase(e->a), 6000); /* en la pecera, sin tarjetas grandes */
        else if (s_panel == PANEL_NINGUNO) panel_frase(e->a);
        break;
    case EV_GUARDIANA: /* el rito: el Corazón en la piedra del centro y ella, Guardiana */
        cerrar_panel();
        s_mascota.tx = (float)LXY(LUGAR_ROCAS)[0] + 26; s_mascota.ty = (float)LXY(LUGAR_ROCAS)[1] + 10;
        mascota_modo(M_LIBRE, 100);
        cer_empezar(1 /* CER_RITO */, false); /* (v39) el rito, en su propia pantalla (ui_ceremonias.inc) */
        break;
    case EV_TE_ESPERABA: /* llegaste a tu hora de siempre */
        mascota_modo(M_SALTA, 1800);
        for (int i = 0; i < 4; i++) flotar(ICO_CORAZON, NULL, 0, hx - 40 + i * 26, hy, (uint32_t)i * 150);
        globo(ICO_RELOJ, s_nino ? NULL : UNA_DE("¡SABÍA QUE VENDRÍAS A ESTA HORA!", "¡TE ESTABA ESPERANDO!", "¡LLEGASTE JUSTO A TIEMPO!", "¡QUÉ BIEN QUE LLEGASTE!", "¡HOLA! ¡YA TE EXTRAÑABA!", "¡JUSTO ESTABA PENSANDO EN TI!"), 3000);
        break;
    case EV_RITUAL:
        if (e->a == 0) { /* buenos días: se estira y te saluda */
            mascota_modo(M_SALTA, 1500);
            flotar(ICO_SOL, NULL, 0, hx, hy - 20, 0);
            globo(ICO_SOL, s_nino ? NULL : (e->b ? "¡AHORA TENEMOS NUESTROS BUENOS DÍAS!" : "¡BUENOS DÍAS! ¡QUÉ BUENO VERTE!"), 3000);
        } else { /* buenas noches: un bostezo y una canción de cuna */
            if (!cuna_cancion()) accion(UI_ACCION_SONAR, SND_A_DORMIR | 0x100); /* con el volumen de efectos, no el de las alarmas (v38: o nuestra canción) */
            flotar(ICO_LUNA, NULL, 0, hx, hy - 20, 0);
            globo(ICO_LUNA, s_nino ? NULL : (e->b ? "¡AHORA TENEMOS NUESTRAS BUENAS NOCHES!" : "BUENAS NOCHES... HASTA MAÑANA"), 3000);
        }
        if (e->b) for (int i = 0; i < 4; i++) flotar(ICO_ESTRELLA, NULL, 0, hx - 40 + i * 26, hy + 50, (uint32_t)i * 150);
        break;
    case EV_ANECDOTA: { /* "¿te acuerdas...?" */
        const entrada_diario_t *d = pet_diary_get(p, e->a);
        if (!d || s_panel != PANEL_NINGUNO) break;
        char txt[120], fc[16], t[160];
        pet_diary_text(p, d, txt, sizeof txt);
        for (char *q = txt; *q; q++) if (*q >= 'a' && *q <= 'z') *q = (char)(*q - 32);
        fecha_corta(d->t, fc, sizeof fc);
        snprintf(t, sizeof t, "¿TE ACUERDAS? EL %s: %s", fc, txt);
        globo(icono_diario(d), s_nino ? NULL : t, 7000);
        break;
    }
    case EV_FAMILIA:
        if (e->a == 0) {
            mascota_modo(M_SALTA, 2000);
            for (int i = 0; i < 5; i++) flotar(i == 2 ? ICO_HUEVO : ICO_CORAZON, NULL, 0, hx - 50 + i * 26, hy, (uint32_t)i * 160);
            globo(ICO_HUEVO, s_nino ? NULL : "¡PUSE UN HUEVITO! LO GUARDARÉ EN MI CASITA", 4500);
            if (s_pet && s_pet->modo_nino == 0) s_pide_huevo = true; /* adulto: ofrece ir a explorar el mundo en cuanto no haya nada abierto */
        }
        break;
    case EV_PIDE_VIAJE:
        s_viaje_con_huevo = false;
        if (s_panel == PANEL_NINGUNO) panel_pide_viaje();
        break;
    case EV_ANIMAL: evento_animal(e, hx, hy); break;
    case EV_ANIMAL_NUEVO: case EV_CRIATURA: case EV_ESPECIAL: case EV_PISTA: case EV_HIST_NIMBA: evento_magia(e, hx, hy); break;
    case EV_TILDE: case EV_BRIZNA: case EV_PRISMA: case EV_TILO: case EV_ANIMAL_DICE: case EV_LUGAR: evento_historia(e); break;
    case EV_PIEZA: evento_pieza(e); break;     /* (v38) */
    case EV_NUESTRA: evento_nuestra(e); break; /* (v38) */
    case EV_FIESTA: break; /* (la fiesta ya se vio en el prado) */
    case EV_LUNA_LLENA: /* esta noche hay luna llena */
        flotar(ICO_LUNA, NULL, 0, hx, hy - 20, 0);
        globo(ICO_LUNA, s_nino ? NULL : (e->a ? "¡ESTA NOCHE HAY LUNA LLENA! ALGO EN LA ISLA TE ESPERA" : "¡ESTA NOCHE HAY LUNA LLENA!"), 4000);
        break;
    case EV_CARTA: /* le llegó una carta: si se puede, el sobre aparece al tiro; si no, cuando quede libre */
        libro_pedir(3);
        s_carta_vista_ms = 0;
        mensajero_nueva(); /* la trae su mensajero (ui_magia.inc) */
        break;
    case EV_CARTA_BOSQUE: /* (fw 104) un papelito del bosque: se abre a pantalla completa (como una historia) y queda en el Libro */
        flotar(ICO_PAPELITO, NULL, 0, hx, hy - 30, 0);
        accion(UI_ACCION_SONAR, SND_HADAS);
        if (e->b) { /* lo trajo ella de su excursión: te lo pasa sin leer (la hoja lo dice arriba) */
            mascota_modo(M_TRAE, 1600);
            s_papel_trajo = true;
            papelito_pendiente(e->a, 1600);
        } else {
            mascota_modo(M_SALTA, 900);
            papelito_pendiente(e->a, 1400);
        }
        break;
    case EV_PASEO_JUNTOS: { /* (fw 104) salieron a caminar de verdad con el orbe */
        mascota_modo(M_SALTA, 1400);
        for (int i = 0; i < 3; i++) flotar(ICO_HUELLA, NULL, 0, hx - 36 + i * 36, hy + 20, (uint32_t)i * 250);
        flotar(ICO_CORAZON, NULL, 0, hx, hy - 20, 900);
        if (!s_nino) {
            char t[80];
            snprintf(t, sizeof t, "¡QUÉ RICO PASEO! CAMINAMOS COMO %d00 PASOS JUNTOS", (int)e->a);
            globo(ICO_HUELLA, t, 3800);
        }
        break;
    }
    case EV_HIELO: /* (fw 104) se derritió su trocito de hielo */
        if (e->a == 1) { flotar(ICO_GOTA, NULL, 0, hx + 20, hy, 0); globo(ICO_GOTA, s_nino ? NULL : "MI TROCITO DE HIELO SE DERRITIÓ... EL PRÓXIMO INVIERNO PESCO OTRO", 3600); }
        break;
    case EV_NIEGA: { /* (fw 104) terca: no quiso a la primera (si insistes, te hace caso) */
        if (p->dormida && e->a != ACT_DESPERTAR) break;
        const char *t = NULL; art_icono_t ic = ICO_NO;
        switch (e->a) {
        case ACT_DORMIR: t = "¡UN RATITO MÁS! NO TENGO SUEÑO..."; ic = ICO_ZETA; break; /* no se quiere dormir todavía */
        case ACT_BANAR: t = "¡NO QUIERO BAÑO! JI, JI"; ic = ICO_GOTA; break;
        case ACT_ALIMENTAR: t = "¡ESO NO ME GUSTA! NI LO PRUEBO"; break;
        case ACT_JUGAR: t = "¡ESPERA, ESTOY OCUPAD{A}!"; ic = ICO_MANO_NO; break;
        case ACT_TRUCO: t = "MMM... NO TENGO GANAS"; break;
        case ACT_DESPERTAR: t = "ZZZ... CINCO MINUTITOS MÁS"; ic = ICO_ZETA; break;
        default: break;
        }
        if (ui_escena_especial() & 2) break; /* (1.0.12) cruzando a otro mundo: no la detiene a medio camino (quedaba entre los dos) */
        if (e->a == ACT_DORMIR) { mascota_modo(M_DUDA, 1200); flotar(ICO_ZETA, NULL, 0, hx + 20, hy, 0); }
        else flotar(ICO_ENOJO, NULL, 0, hx + 24, hy - 6, 150);
        if (t && !s_nino) globo(ic, t, 2600);
        break;
    }
    case EV_BOTELLA: /* en la orilla de la isla: una botella con un mensaje de lejos */
        flotar(ICO_OBJETO(OBJ_BOTELLA), NULL, 0, hx + 30, hy, 0);
        mascota_modo(M_SALTA, 900);
        if (s_pecera || s_panel != PANEL_NINGUNO) globo(ICO_OBJETO(OBJ_BOTELLA), pet_phrase(e->a), 6000);
        else botella_nada(e->a); /* primero llega nadando; luego se abre y se lee */
        break;
    case EV_DESCUBRE:
    { /* un gusto fuerte a la vista: corazón o cruz sobre la cabeza */
        int v = 0;
        if (e->a == GUSTO_GESTO && e->b < GESTO_COUNT) v = p->memoria.gesto[e->b];
        if (e->a == GUSTO_COMIDA && e->b < FOOD_COUNT) v = p->habitos.gusto[e->b] - 128;
        if (e->a == GUSTO_CLIMA && e->b < CLIMA_COUNT) v = p->memoria.clima[e->b];
        flotar(v >= 0 ? ICO_CORAZON : ICO_NO, NULL, 0, hx, hy - 20, 200);
        break;
    }
    case EV_AMIGO_CERCA: {
        if (e->a < 0 || e->a >= PET_MAX_AMIGOS) break;
        char t[48];
        snprintf(t, sizeof t, "¡%s ESTÁ CERCA!", p->amigos[e->a].nombre);
        globo(ICO_AMIGOS, t, 4000);
        if (s_panel == PANEL_AMIGOS) panel_amigos();
        break;
    }
    case EV_INVITACION:
        if (s_pecera) globo(ICO_AMIGOS, "¡UN AMIGO NOS INVITA A JUGAR!", 5000);
        else panel_invitacion(e->a);
        break;
    case EV_AMIGO_VISITA: {
        if (e->a < 0 || e->a >= PET_MAX_AMIGOS) break;
        if (s_panel == PANEL_AMIGOS) cerrar_panel();
        const amigo_t *a = &p->amigos[e->a];
        entrar_visita(a->especie, a->etapa, a->forma, 4);
        static const int ENC[] = {ENC_JUGARON, ENC_JUGARON, ENC_JUGARON, ENC_JUGARON, ENC_SE_ENAMORARON};
        s_visita.encuentro = ENC[e->b % AMI_COUNT];
        s_visita.amigo = e->a + 1;
        s_visita.se_va = s_ms + 14000;
        static const char *const T[] = {"¡A JUGAR CON %s!", "¡PICNIC CON %s!", "¡%s TRAJO UN REGALO!", "¡A BAILAR CON %s!", "¡%s Y YO NOS QUEREMOS!"};
        char t[64];
        snprintf(t, sizeof t, T[e->b % AMI_COUNT], a->nombre);
        globo(ICO_AMIGOS, t, 4500);
        if (e->b == AMI_PICNIC) { lv_image_set_src(s_objeto, art_icono(ICO_FRUTA, 3)); ocultar(s_objeto, false); mascota_modo(M_COME, 2400); }
        break;
    }
    case EV_ESCENA: { /* el viaje en sí lo anima ui_update; aquí solo dice adónde va */
        static const char *const T[ESC_COUNT] = {"¡A CASA!", "¡AL BOSQUE!", "¡A LA LAGUNA!", "¡AL CERRO!", "¡A LA CUEVA!", "¡A LA ISLA!", "¡AL CLARO DEL REY!", "¡AL MIRADOR!"};
        if (e->a >= 0 && e->a < ESC_COUNT && e->b) globo(ICO_ZONA[e->a], T[e->a], 2200);
        break;
    }
    case EV_ESCENA_NUEVA: {
        static const char *const T[ESC_COUNT] = {"", "¡ENCONTRÉ UN CAMINO AL BOSQUE DE LOS COIHUES!", "¡HAY UNA LAGUNA MÁS ALLÁ: LA LAGUNA DE LOS CISNES!",
                                                 "¡SE VE UN CERRO A LO LEJOS: EL CERRO DEL VIENTO!", "¡DESCUBRÍ UNA CUEVA SECRETA!",
                                                 "¡VEO UNA ISLA A LO LEJOS: LA ISLA DEL FARO! ¿VAMOS EN EL BOTE?", "¡EL CLARO DEL REY PEUMO!", "¡LA CIMA DEL CERRO: EL MIRADOR!"};
        if (e->a <= 0 || e->a >= ESC_COUNT) break;
        if (e->a == ESC_ISLA) libro_pedir(2);
        globo(ICO_ZONA[e->a], T[e->a], 4500);
        for (int i = 0; i < 4; i++) flotar(ICO_ESTRELLA, NULL, 0, hx - 40 + i * 26, hy + 10, (uint32_t)i * 150);
        mascota_modo(M_SALTA, 1200);
        break;
    }
    case EV_FAUNA: {
        if (e->a < 0 || e->a >= FAUNA_COUNT) break;
        bicho_t *b = &s_fauna[e->a];
        int bx = b->visible ? (int)b->x : hx, by = b->visible ? (int)b->y - 40 : hy;
        switch (e->b) {
        case FSUC_LLEGA:
            if (e->a == FAUNA_ZORRO && !s_nino) globo(ICO_ZORRO, "¡UN ZORRO! QUIERE MI FRUTA", 3500);
            else if (e->a == FAUNA_ZORRO) globo(ICO_ZORRO, NULL, 3000);
            else if (e->a == FAUNA_PAJARO && p->pajaro_amistad > 0.5f) flotar(ICO_CORAZON, NULL, 0, hx, hy, 600);
            break;
        case FSUC_SE_VA: break; /* animar_fauna lo saca de la pantalla */
        case FSUC_PERSIGUE: flotar(ICO_NOTA, NULL, 0, hx, hy, 0); break;
        case FSUC_PICO:
            mascota_modo(M_NIEGA, 1500);
            flotar(ICO_ABEJA, NULL, 0, hx + 20, hy, 0);
            flotar(ICO_LAGRIMA, NULL, 0, hx - 20, hy, 300);
            globo(ICO_ABEJA, "¡AY! ME PICÓ...", 3000);
            fauna_sale(FAUNA_ABEJA);
            break;
        case FSUC_EVITA: flotar(ICO_NO, NULL, 0, bx, by, 0); break; /* ya aprendió */
        case FSUC_ROBO:
            globo(ICO_ZORRO, "¡SE LLEVÓ UNA FRUTA!", 3500);
            flotar(ICO_FRUTA, NULL, 0, bx, by, 0);
            mascota_modo(M_NIEGA, 1200);
            fauna_sale(FAUNA_ZORRO);
            break;
        case FSUC_ESPANTADO:
            fauna_sale(FAUNA_ZORRO);
            flotar(ICO_CORAZON, NULL, 0, hx, hy, 300);
            globo(ICO_CORAZON, "¡GRACIAS POR DEFENDERME!", 3000);
            mascota_modo(M_SALTA, 900);
            break;
        case FSUC_LO_ECHO:
            fauna_sale(FAUNA_ZORRO);
            flotar(ICO_ENOJO, NULL, 0, hx + 20, hy, 0);
            globo(ICO_ZORRO, "¡FUERA, ZORRO!", 2500);
            break;
        case FSUC_REGALO:
            flotar(ICO_SEMILLA, NULL, 0, bx, by, 0);
            flotar(ICO_CORAZON, NULL, 0, hx, hy, 500);
            globo(ICO_PAJARO, "¡EL PÁJARO ME TRAJO UNA SEMILLA!", 3500);
            break;
        default: break;
        }
        break;
    }
    case EV_JARDIN: {
        if (e->a < 0 || e->a >= PET_PARCELAS_TOTAL) break;
        int px = POS_PARCELA[e->a][0], py = POS_PARCELA[e->a][1] - 30;
        bool aqui = s_escena == (int)pet_plant_scene(e->a) && !s_interior;
        static const char *const DONDE[ESC_COUNT] = {"", " DEL BOSQUE", " DE LA LAGUNA", " DEL CERRO", "", "", "", ""};
        const char *donde = DONDE[pet_plant_scene(e->a) % ESC_COUNT];
        char tj[64];
        switch (e->b) {
        case JAR_PLANTO: if (aqui) for (int k = 0; k < 3; k++) tierra(px - 6 + k * 6, py + 20); break;
        case JAR_CRECIO: if (aqui) flotar(ICO_ESTRELLA, NULL, 0, px, py, 0); break;
        case JAR_FLORECIO:
            if (aqui) for (int k = 0; k < 2; k++) flotar(ICO_ESTRELLA, NULL, 0, px - 10 + k * 20, py, (uint32_t)k * 200);
            globo(ICO_FLOR, "¡FLORECIÓ UNA FLOR!", 3000);
            break;
        case JAR_MUSTIA:
            snprintf(tj, sizeof tj, "MIS PLANTAS%s TIENEN SED", donde);
            globo(ICO_REGADERA, s_nino ? NULL : tj, 3500);
            break;
        case JAR_SECA:
            snprintf(tj, sizeof tj, "¡SE ESTÁ SECANDO UNA PLANTA%s! RIÉGALA", donde);
            globo(ICO_PLANTA_SECA, s_nino ? NULL : tj, 4000);
            break;
        case JAR_PERDIDA:
            snprintf(tj, sizeof tj, "SE SECÓ UNA PLANTA%s...", donde);
            globo(ICO_PLANTA_SECA, s_nino ? NULL : tj, 4000);
            if (aqui) for (int k = 0; k < 4; k++) salpicar(px - 9 + k * 6, py + 24, 0xB58E5A);
            break;
        case JAR_ARRANCO: if (aqui) for (int k = 0; k < 4; k++) tierra(px - 9 + k * 6, py + 24); break;
        case JAR_VIEJA: /* cumplió su vida: se marchitó sola */
            snprintf(tj, sizeof tj, "UNA PLANTA%s YA CUMPLIÓ SU TIEMPO Y SE MARCHITÓ", donde);
            globo(ICO_PLANTA_SECA, s_nino ? NULL : tj, 4000);
            break;
        case JAR_REVIVIO: if (aqui) flotar(ICO_CORAZON, NULL, 0, px, py, 0); break;
        case JAR_FRUTO: if (aqui) flotar(s_pet && pet_plant(s_pet, e->a)->tipo == PLANTA_HIERBA ? ICO_HIERBA : ICO_FRUTA, NULL, 0, px, py - 30, 0); break;
        case JAR_ENTERRO:
            globo(ICO_SEMILLA, "ENTERRÉ UNA SEMILLA... ¿QUÉ SALDRÁ?", 4000);
            for (int k = 0; k < 4; k++) tierra(hx - 10 + k * 6, (int)c->y - 6);
            break;
        default: break;
        }
        break;
    }
    case EV_SALPICA: {
        int i = e->a & 1;
        s_charco_gusta = e->b > 0;
        if (s_escena == ESC_PRADO && !s_interior && s_mascota.visible && s_juego == JG_NADA) { /* va al charco y salta encima */
            s_charco_i = i;
            s_charco_llego = 0;
            s_charco_saltos = 0;
            c->tx = POS_CHARCO[i][0];
            c->ty = POS_CHARCO[i][1] + 2;
            mascota_modo(M_CHARCO, 9000);
        } else {
            for (int k = 0; k < 8; k++) salpicar((int)c->x - 20 + k * 5, (int)c->y - 6, 0x7CC8FF);
            sonido(SND_SALPICON);
            if (e->b > 0) mascota_modo(M_SALTA, 1400);
            else mascota_modo(M_NIEGA, 900);
        }
        break;
    }
    case EV_ALARMA:
        s_al_de = NULL;
        alarma_mostrar(AL_ALARMA, e->a, e->t);
        break;
    case EV_HORA_DORMIR:
        if (e->a == 0) alarma_mostrar(AL_DORMIR, 0, e->t);
        else if (!ui_alarm_ringing()) globo(ICO_SOL, s_nino ? NULL : UNA_DE("¡BUENOS DÍAS!", "¡HOLA, BUEN DÍA!", "¡A DESPERTAR! ¡BUENOS DÍAS!", "¡QUÉ BUEN DÍA PARA JUGAR!", "¡BUENOS DÍAS! ¿DORMISTE BIEN?"), 3500);
        break;
    case EV_SEMILLA: {
        if (!e->b || e->a < 0 || e->a >= CULTIVO_COUNT || e->a == CULTIVO_FLOR) break; /* solo avisa de los tipos nuevos */
        static const art_icono_t I[CULTIVO_COUNT] = {ICO_FLOR, ICO_GIRASOL, ICO_FRUTILLA, ICO_HIERBA};
        static const char *const T[CULTIVO_COUNT] = {"", "¡SEMILLAS DE GIRASOL!", "¡SEMILLAS DE FRUTILLA!", "¡SEMILLAS DE HIERBA MEDICINAL!"};
        globo(I[e->a], s_nino ? NULL : T[e->a], 4000);
        flotar(ICO_SEMILLA, NULL, 0, hx, hy - 10, 0);
        for (int k = 0; k < 3; k++) flotar(ICO_ESTRELLA, NULL, 0, hx - 26 + k * 26, hy - 30, (uint32_t)k * 150);
        break;
    }
    case EV_VECINO: {
        const char *nom = p->vecino.nombre;
        char t[64];
        switch (e->a) {
        case VEC_LLEGA:
            snprintf(t, sizeof t, "¡LLEGÓ %s!", nom);
            globo(ICO_AMIGOS, t, 3000);
            mascota_modo(M_SALTA, 1200);
            break;
        case VEC_SE_VA:
            if (s_vecino.visible) flotar(ICO_CORAZON, NULL, 0, (int)s_vecino.x, cabeza_y(&s_vecino), 0);
            vecino_sale();
            break;
        case VEC_COMER:
            if (s_juego == JG_PELOTA) terminar_juego(); /* ¡picnic! deja la pelota */
            s_vec_comida = e->b;
            snprintf(t, sizeof t, e->b == ITEM_FRUTA ? "¡COMPARTIMOS UNA FRUTA!" : "¡%s TRAJO GALLETAS!", nom);
            globo(e->b == ITEM_FRUTA ? ICO_FRUTA : ICO_GALLETA, t, 2500);
            break;
        case VEC_REGALO:
            s_vec_regalo = e->b;
            snprintf(t, sizeof t, "¡%s ME TRAJO UN REGALO!", nom);
            globo(ICO_REGALO, t, 3000);
            break;
        case VEC_JUGAR: globo(ICO_PELOTA, NULL, 1500); break;
        case VEC_DORMIDA: {
            if (s_interior || (p->escena == ESC_PRADO && pet_has_decor(p, DECOR_CASITA))) break; /* dormida en su casita: no nos enteramos de la visita */
            char bt[PET_NOMBRE_MAX + 2 + 8], fr[160];
            snprintf(bt, sizeof bt, "%s", p->nombre);
            snprintf(fr, sizeof fr, "VEO QUE %s ESTÁ DORMID{O}. MEJOR {LO} DEJO DESCANSAR. YA NOS VEREMOS OTRO DÍA", bt);
            globo(ICO_AMIGOS, gtxt(fr), 5000);
            break;
        }
        default: break;
        }
        break;
    }
    case EV_PESCA: {
        if (pez_resultado(e->a, e->b)) break; /* v89: con la pantalla de pesca abierta, el resultado se ve allí */
        static const art_icono_t I[PESCA_COUNT] = {ICO_PUNTOS, ICO_PEZ, ICO_BOTA, ICO_PIEDRA, ICO_CONCHA, ICO_ALGA, ICO_CANGREJO, ICO_NENUFAR,
                                                   ICO_OBJETO(OBJ_BOTELLA), ICO_OBJETO(OBJ_LLAVE_OXIDADA), ICO_PEZ_DORADO, ICO_HIELO};
        static const char *const T[PESCA_COUNT] = {"NADA... OTRA VEZ SERÁ", "¡UN PEZ!", "¡UNA BOTA VIEJA! JA, JA", "¡UNA PIEDRA BRILLANTE!",
                                                   "¡UNA CONCHA PARA MI COLECCIÓN!", "¡UN ALGA! JI, JI, ¡QUÉ COSQUILLAS!",
                                                   "¡AY! ¡ME PELLIZCÓ LA NARIZ! LO DEVUELVO AL AGUA", "¡UNA HOJA DE NENÚFAR! MIRA MI SOMBRERO",
                                                   "¡UNA BOTELLA CON UN MENSAJE!", "UNA LLAVE OXIDADA... ¿QUÉ ABRIRÁ?",
                                                   "¡UN PEZ DORADO! BRILLA... LO DEVUELVO AL AGUA",
                                                   "¡UN TROCITO DE HIELO! ¡QUÉ FRÍO! LO GUARDO CON MIS CRISTALES"};
        int a = e->a >= 0 && e->a < PESCA_COUNT ? e->a : 0;
        int bx = (int)lv_obj_get_x(s_boya) + 5, by = (int)lv_obj_get_y(s_boya);
        if (lv_obj_has_flag(s_boya, LV_OBJ_FLAG_HIDDEN)) { bx = hx - 60; by = hy; }
        for (int k = 0; k < 6; k++) salpicar(bx - 10 + k * 4, by, 0x9ED8FF);
        flotar(I[a], NULL, 0, bx, by - 10, 0);
        if (a == PESCA_PEZ && e->b) globo(ICO_PEZ, "¡UN PEZ! ÑAM", 2500);
        else if (a == PESCA_PEZ) globo(ICO_PEZ, "¡UN PEZ! LO DEVUELVO AL AGUA", 2500);
        else globo(I[a], T[a], 2500);
        if (a == PESCA_PEZ_DORADO) for (int k = 0; k < 5; k++) flotar(ICO_ESTRELLA, NULL, 0, bx - 40 + k * 20, by - 20, (uint32_t)k * 160);
        if (a == PESCA_CANGREJO) { flotar(ICO_CANGREJO, NULL, 0, hx, hy + 10, 0); mascota_modo(M_MAREADA, 900); }
        else if (a != PESCA_NADA) mascota_modo(M_SALTA, 900);
        break;
    }
    case EV_CUMPLE: {
        pastel_aqui();
        /* sin la edad: un deseo tierno */
        static const char *const DESEO[] = {"¡FELIZ CUMPLEAÑOS! DESEO QUE TODOS TUS SUEÑOS SE HAGAN REALIDAD",
                                            "¡FELIZ CUMPLEAÑOS! QUE HOY TE LLUEVAN ABRAZOS Y SONRISAS",
                                            "¡FELIZ CUMPLEAÑOS! ERES MI PERSONA FAVORITA EN TODO EL MUNDO",
                                            "¡FELIZ CUMPLEAÑOS! PIDE UN DESEO GIGANTE: YO LO GUARDO CONTIGO"};
        if (!s_pecera) { cumple_empezar(e->b, DESEO[azar() % 4]); break; } /* su pantalla propia, con magia */
        globo(ICO_PASTEL, DESEO[azar() % 4], 7000);
        confeti(hx, hy - 20, 16);
        for (int i = 0; i < 5; i++) flotar(i % 2 ? ICO_ESTRELLA : ICO_REGALO, NULL, 0, hx - 60 + i * 30, hy + 10, (uint32_t)i * 300);
        mascota_modo(M_SALTA, 3000);
        accion(UI_ACCION_SONAR, SND_CUMPLEANOS);
        break;
    }
    case EV_VINCULO: {
        static const char *const T[VINCULO_COUNT] = {"", "¡YA TE CONOZCO!", "¡SOMOS AMIGOS!", "¡ERES MI MEJOR AMIGO!", "¡SOMOS INSEPARABLES!"};
        if (e->a <= 0 || e->a >= VINCULO_COUNT) break;
        globo(ICO_CORAZON, T[e->a], 3500);
        for (int i = 0; i < 5; i++) flotar(ICO_CORAZON, NULL, 0, hx - 60 + i * 30, hy + 10, (uint32_t)i * 150);
        mascota_modo(M_SALTA, 1200);
        break;
    }
    case EV_HABILIDAD: {
        bool nadar = e->a == HAB_NADAR;
        art_icono_t ic = nadar ? ICO_NADAR : ICO_TREPAR;
        if (e->b == -1) { /* se cayó del árbol: ¡pum! (no pasa nada) */
            s_cae_hasta = s_ms + 700;
            flotar(ICO_SUDOR, NULL, 0, hx + 20, hy, 0);
            for (int k = 0; k < 6; k++) salpicar((int)c->x - 15 + k * 6, (int)ARBOL_TREPA[s_escena < ESC_COUNT ? s_escena : 0][1] - 4, 0xC8A880);
        } else if (e->b == 1) globo(ic, nadar ? "¿Y SI ME METO AL AGUA?" : "¿PODRÉ SUBIR A ESE ÁRBOL?", 2500);
        else if (e->b == 2) { globo(ic, nadar ? "¡APRENDÍ A NADAR!" : "¡APRENDÍ A TREPAR!", 3500); for (int i = 0; i < 4; i++) flotar(ICO_ESTRELLA, NULL, 0, hx - 45 + i * 30, hy + 10, (uint32_t)i * 150); }
        else if (e->b == 3) globo(ic, nadar ? "¡NADO COMO UN PEZ!" : "¡TREPO HASTA LA PUNTA!", 3500);
        break;
    }
    case EV_TESORO:
        if (e->a == 0) { /* lo huele */
            flotar(ICO_NARIZ, NULL, 0, hx + 26, hy, 0);
            if (!s_nino) globo(ICO_NARIZ, "¡AQUÍ HAY ALGO ESCONDIDO!", 2200);
        } else if (e->b >= 0) {
            int col = e->b / 8, pz = e->b % 8;
            char t[64];
            snprintf(t, sizeof t, "¡UN TESORO! %s", pet_piece_name((coleccion_t)col, pz));
            for (char *q = t; *q; q++) if (*q >= 'a' && *q <= 'z') *q = (char)(*q - 32);
            globo(ICO_COFRE, t, 3500);
            flotar(ICO_ESTRELLA, NULL, 0, hx - 20, hy, 0);
            flotar(ICO_ESTRELLA, NULL, 0, hx + 20, hy, 250);
            mascota_modo(M_SALTA, 900);
        } else flotar(ICO_PIEDRA, NULL, 0, hx, hy, 0); /* repetido: una piedra brillante */
        break;
    case EV_COLECCION: {
        char t[72];
        snprintf(t, sizeof t, "¡COMPLETÉ MI COLECCIÓN DE %s!", pet_collection_name((coleccion_t)e->a));
        for (char *q = t; *q; q++) if (*q >= 'a' && *q <= 'z') *q = (char)(*q - 32);
        globo(ICO_COFRE, t, 4500);
        confeti(hx, hy - 20, 16);
        mascota_modo(M_SALTA, 2000);
        break;
    }
    case EV_RARO:
        if (e->b) {
            static const char *const T[RARO_COUNT] = {"¡MIRA, UN ARCOÍRIS!", "¡UNA ESTRELLA FUGAZ! PIDE UN DESEO", "¡UN PUDÚ BLANCO! SHHH...", "¡UNA BALLENA DE NUBES! TÓCALA Y NOS LLEVA"};
            static const art_icono_t I[RARO_COUNT] = {ICO_ARCOIRIS, ICO_ESTRELLA_FUGAZ, ICO_CIERVO, ICO_GLOBO_AIRE};
            static const char *const CIERVO[3] = {"¡UN PUDÚ BLANCO! SHHH... NO LO ASUSTEMOS", "¡QUÉ ELEGANTE! BRILLA COMO LA LUNA", "DICEN QUE TRAE BUENA SUERTE... ¡QUÉ LINDO!"};
            if (e->a >= 0 && e->a < RARO_COUNT && !p->dormida) globo(I[e->a], e->a == RARO_CIERVO ? CIERVO[s_ms / 1000 % 3] : T[e->a], 4000);
        }
        break;
    case EV_META:
        if (e->b == 0 && !p->dormida) {
            char t[72];
            snprintf(t, sizeof t, "META DE HOY: %s", pet_goal_name((meta_t)e->a));
            for (char *q = t; *q; q++) if (*q >= 'a' && *q <= 'z') *q = (char)(*q - 32);
            globo(s_nino ? icono_meta(e->a) : ICO_META, t, 4000);
        } else if (e->b == 1) {
            globo(ICO_ESTAMPITA, "¡META CUMPLIDA! UNA ESTAMPITA", 3000);
            flotar(ICO_ESTAMPITA, NULL, 0, hx, hy - 10, 0);
            mascota_modo(M_SALTA, 1000);
        } else flotar(icono_meta(e->a), NULL, 0, hx + 24, hy, 0);
        break;
    case EV_SU_CUMPLE: {
        char t[48];
        s_su_cumple_hasta = s_ultimo_t + 14u * 3600u;
        pastel_aqui();
        if (e->a == 7 && !e->b) snprintf(t, sizeof t, "¡HOY CUMPLO UNA SEMANA!");
        else if (e->b) snprintf(t, sizeof t, e->a == 1 ? "¡HOY CUMPLO UN AÑO!" : "¡HOY CUMPLO %d AÑOS!", e->a);
        else snprintf(t, sizeof t, e->a == 1 ? "¡HOY CUMPLO UN MES!" : "¡HOY CUMPLO %d MESES!", e->a);
        globo(ICO_GORRO, t, 5000);
        confeti(hx, hy - 20, 16);
        mascota_modo(M_SALTA, 2500);
        break;
    }
    case EV_FECHA: {
        static const char *const T[FECHA_COUNT] = {"", "¡FELIZ HALLOWEEN! ¿DULCE O TRUCO?", "¡FELIZ NAVIDAD!", "¡FELIZ AÑO NUEVO!",
                                                   "¡FELIZ DÍA DEL AMOR!", "¡LLEGÓ LA PRIMAVERA!", "¡FELICES FIESTAS!"};
        static const art_icono_t I[FECHA_COUNT] = {ICO_ESTRELLA, ICO_CALABAZA, ICO_ARBOL_NAVIDAD, ICO_FUEGOS, ICO_CORAZON, ICO_FLOR, ICO_BANDERINES};
        if (e->a <= 0 || e->a >= FECHA_COUNT) break;
        const celebracion_t *cel = pet_celebration_get(e->b); /* el saludo de esa celebración (se puede editar) */
        globo(I[e->a], cel && cel->deco == e->a && cel->saludo[0] ? cel->saludo : T[e->a], 4500);
        if (e->a == FECHA_PRIMAVERA) accion(UI_ACCION_SONAR, SND_PRIMAVERA); /* el 21 de septiembre suena la melodía de primavera */
        confeti(hx, hy - 20, 10);
        mascota_modo(M_SALTA, 1500);
        break;
    }
    case EV_FOTO:
        if (s_panel == PANEL_NINGUNO) { flash_foto(); flotar(ICO_FOTO, NULL, 0, CX + 120, 120, 300); }
        break;
    case EV_ROPA: {
        int r = (unsigned)e->a % ROPA_COUNT;
        if (s_panel == PANEL_RINCON) { s_ropa_reac = e->b; s_ropa_reac_r = r; } /* se ve en el panel */
        bool panel = s_panel != PANEL_NINGUNO;
        switch (e->b) {
        case 2:
            if (!panel) { globo(ICO_ROPA[r], "¡ME ENCANTA!", 2500); mascota_modo(M_SALTA, 1200); }
            for (int k = 0; k < 3; k++) flotar(ICO_CORAZON, NULL, 0, hx - 20 + k * 20, hy - 10, (uint32_t)k * 150);
            break;
        case 1: flotar(ICO_CORAZON, NULL, 0, hx, hy - 10, 0); break;
        case -1:
            if (!panel) globo(ICO_ROPA[r], s_nino ? NULL : "MMM... NO ME GUSTA MUCHO", 2500);
            mascota_modo(M_NIEGA, 900);
            break;
        case -2:
            if (!panel) globo(ICO_ROPA[r], s_nino ? NULL : "¡ME LO SAQUÉ!", 2500);
            flotar(ICO_ROPA[r], NULL, 0, hx + 30, hy, 0);
            mascota_modo(M_NIEGA, 900);
            break;
        case -3:
            if (!panel) globo(ICO_CALOR, s_nino ? NULL : "¡HACE MUCHO CALOR PARA ESO!", 2800);
            flotar(ICO_ROPA[r], NULL, 0, hx + 30, hy, 0);
            break;
        default: break;
        }
        if (s_panel == PANEL_RINCON) panel_rincon();
        break;
    }
    case EV_JUGUETE: {
        int j = (unsigned)e->a % JUG_COUNT;
        if (e->b == 1 && s_panel == PANEL_NINGUNO) {
            globo(ICO_JUG[j], s_nino ? NULL : "¡ESTRENÉ UN JUGUETE!", 3000);
            confeti(hx, hy - 20, 6);
        } else if (e->b == 2) flotar(ICO_CORAZON, NULL, 0, hx, hy - 10, 0);
        else if (e->b == -1) {
            if (s_panel == PANEL_NINGUNO) globo(ICO_JUG[j], s_nino ? NULL : "AHORA NO TENGO GANAS", 2500);
            mascota_modo(M_NIEGA, 900);
        }
        if (s_panel == PANEL_RINCON) panel_rincon();
        break;
    }
    case EV_NUEVO: {
        art_icono_t ic = e->a ? ICO_JUG[(unsigned)e->b % JUG_COUNT] : ICO_ROPA[(unsigned)e->b % ROPA_COUNT];
        if (s_panel == PANEL_NINGUNO) globo(ic, s_nino ? NULL : "¡ALGO NUEVO EN EL BAÚL!", 3500);
        flotar(ic, NULL, 0, hx, hy - 20, 0);
        confeti(hx, hy - 30, 8);
        break;
    }
    case EV_HOJAS: {
        int x = HOJAS_X, y = HOJAS_Y - 8;
        for (int k = 0; k < 10; k++) salpicar(x - 25 + k * 5, y - 10, k % 3 ? 0xE86A3A : 0xFFC14D);
        mascota_modo(M_SALTA, 1800);
        flotar(ICO_NOTA, NULL, 0, hx, hy, 200);
        ocultar(s_hojas, true);
        break;
    }
    default: break;
    }

    if (e->type == EV_REENCUENTRO && (s_resumen_n > 0 || s_res_n > 0) && !s_pecera) panel_resumen();
}

/* ======================================================================
 * Animación (cada 80 ms)
 * ==================================================================== */

static bool mover(criatura_t *c, float vel)
{
    float dx = c->tx - c->x, dy = c->ty - c->y, d = sqrtf(dx * dx + dy * dy);
    if (d < vel) { c->x = c->tx; c->y = c->ty; return false; }
    c->x += dx / d * vel;
    c->y += dy / d * vel;
    if (fabsf(dx) > 2) c->pose.espejo = dx < 0;
    return true;
}

/* lenguaje corporal a partir de las emociones */
static void cuerpo_emocional(criatura_t *c, const pet_t *p, float *vel, float *salto, float *temblor)
{
    emo_t a, b;
    int n = pet_top_emotions(p, &a, &b);
    float fa = p->emo[a], fb = n >= 2 ? p->emo[b] : 0.0f;
    art_pose_t *ps = &c->pose;
    ps->orejas = 0; ps->cola = 0; ps->postura = 0;
    ps->expr = EXP_ABIERTOS;
    *vel = 3.0f; *salto = 0; *temblor = 0;
    float k = s_suave ? 1.5f : 1.0f; /* con niños todo es más marcado */

    if (p->enferma) { /* cabeza gacha, ojos cansados y todo lento (nunca da miedo: sin calaveras ni ojos en X) */
        ps->expr = s_nino ? EXP_ENFERMA_LEVE : EXP_ENFERMA; ps->orejas = -1; ps->cola = -1; ps->postura = -1; *vel = 1.2f; return;
    }
    if (n == 0) { if (p->necesidad[NEED_SACIEDAD] < 25) { ps->orejas = -1; *vel = 2.2f; } return; }

    switch (a) {
    case EMO_ALEGRIA: ps->expr = EXP_FELIZ; ps->orejas = 1; ps->cola = 1; *vel = 3.8f; *salto = 5.0f * fa * k; break;
    case EMO_TRISTEZA: ps->expr = EXP_TRISTE; ps->orejas = -1; ps->cola = -1; ps->postura = fa > 0.6f ? -1 : 0; *vel = 1.5f; break;
    case EMO_ENOJO: ps->expr = EXP_ENOJADO; ps->cola = 0; *vel = 3.2f; *temblor = 1.0f; break;
    case EMO_MIEDO: ps->expr = EXP_MIEDO; ps->orejas = -1; ps->cola = -1; ps->postura = -1; *vel = 2.6f; *temblor = 2.0f * k; break;
    case EMO_CURIOSIDAD: ps->expr = EXP_CURIOSO; ps->orejas = 1; *vel = 3.0f; break;
    case EMO_ABURRIMIENTO: ps->expr = EXP_ABURRIDO; ps->orejas = -1; *vel = 1.6f; break;
    case EMO_CANSANCIO: ps->expr = (s_ms / 1500) % 3 == 0 ? EXP_CERRADOS : EXP_ABURRIDO; ps->orejas = -1; *vel = 1.3f; break;
    case EMO_ORGULLO: ps->expr = EXP_ORGULLO; ps->orejas = 1; ps->postura = 1; ps->cola = 1; *vel = 2.6f; *salto = 2.0f; break;
    case EMO_CELOS: ps->expr = EXP_CELOS; ps->cola = 0; *vel = 2.8f; *temblor = 0.8f; break;
    case EMO_CARINO: ps->expr = EXP_FELIZ; ps->cola = 1; *vel = 3.2f; *salto = 2.0f; break;
    default: break;
    }
    /* frío que no le gusta: tirita y se encoge (salvo abrigada en la casita) */
    int sens = pet_temperature_feel(p, s_ultimo_t);
    if (sens > 0) { /* calor: la lengua afuera, gotitas y paso lento */
        *vel *= 0.75f;
        ps->boca = true;
        if (s_ms % 2600 < TICK_MS) flotar(ICO_SUDOR, NULL, 0, (int)c->x + 22, cabeza_y(c), 0);
    }
    if (sens < 0 && p->memoria.clima[CLIMA_FRIO] < 0 &&
        !(p->actividad == ACTV_ABRIGARSE && pet_has_decor(p, DECOR_CASITA))) {
        *temblor = fmaxf(*temblor, 1.0f + (float)(-p->memoria.clima[CLIMA_FRIO]) / 80.0f);
        if (ps->postura == 0) ps->postura = -1;
        ps->cola = -1;
    }
    /* mezcla: la segunda emoción cambia orejas y paso ("feliz pero con sueño") */
    if (fb >= 0.35f) {
        if (b == EMO_CANSANCIO || b == EMO_TRISTEZA || b == EMO_ABURRIMIENTO) { ps->orejas = -1; *vel *= 0.7f; *salto *= 0.4f; }
        if (b == EMO_MIEDO) { ps->postura = -1; *temblor = fmaxf(*temblor, 1.0f); }
        if (b == EMO_ALEGRIA && ps->cola == 0) ps->cola = 1;
        if (b == EMO_CURIOSIDAD && ps->orejas == 0) ps->orejas = 1;
    }
}

/* una partícula: tipo 0 cae (terrón/gota), 1 gotita de agua, 2 nube de polvo; sz = tamaño inicial en píxeles */
static void particula(int x, int y, uint32_t color, int tipo, float vx, float vy, int sz, int vida)
{
    for (int i = 0; i < N_TIERRA; i++) {
        if (!lv_obj_has_flag(s_tierra[i], LV_OBJ_FLAG_HIDDEN)) continue;
        lv_obj_set_style_bg_color(s_tierra[i], lv_color_hex(color), 0);
        lv_obj_set_style_bg_opa(s_tierra[i], tipo == 2 ? LV_OPA_50 : LV_OPA_COVER, 0);
        s_tx_[i] = (float)x; s_ty_[i] = (float)y; s_tvx[i] = vx; s_tvy[i] = vy;
        s_tsz[i] = (float)sz; s_ttipo[i] = (uint8_t)tipo; s_tvida[i] = 0; s_tmax[i] = (uint8_t)vida;
        lv_obj_set_size(s_tierra[i], sz, sz);
        ocultar(s_tierra[i], false);
        return;
    }
}
static void salpicar(int x, int y, uint32_t color)
{
    particula(x, y, color, 0, (azar01() - 0.5f) * 8, -4 - azar01() * 4, 4 + (int)(azar() % 3), 0);
}
/* un anillo que se abre en el agua (o un charco) */
static void ondita(int x, int y)
{
    for (int i = 0; i < N_ONDITA; i++) {
        if (s_ondita_t[i]) continue;
        s_ondita_x[i] = (float)x; s_ondita_y[i] = (float)y; s_ondita_t[i] = 1;
        ocultar(s_ondita[i], false);
        return;
    }
}
/* una nubecita de polvo o de agua pulverizada que se abre y se desvanece */
static void polvo(int x, int y, uint32_t color)
{
    particula(x - 4, y - 4, color, 2, (azar01() - 0.5f) * 1.6f, -0.5f - azar01() * 0.6f, 6, 7);
}

static void tierra(int x, int y) { salpicar(x, y, 0x8B5E3C); }
/* terrones que saltan hacia atrás (dir -1 izquierda, 1 derecha), en arco */
static void tierra_atras(int x, int y, float dir)
{
    static const uint32_t C[4] = {0x8B5E3C, 0x5E3B22, 0xA87A50, 0x7A4E2E};
    const int n = (int)(azar() % 5);
    particula(x, y, n == 4 ? 0x8E8C96 : C[n], 0, dir * (2.5f + azar01() * 4.5f), -5.0f - azar01() * 4.5f, n == 4 ? 3 : 3 + (int)(azar() % 4), 0);
}

/* el hoyo cavado (borde de tierra removida, pared y fondo oscuro) con el montón de tierra a un lado;
 * cuando termina, ella tapa el hoyo con la tierra */
static lv_obj_t *s_hoyo, *s_hoyo_borde, *s_hoyo_pared, *s_monton[3], *s_terrones[4];
static uint32_t s_cavar_desde, s_cavar_hasta;
static float s_hoyo_x, s_hoyo_y, s_hoyo_dir;
static lv_obj_t *elipse_tierra(uint32_t color, uint32_t borde)
{
    lv_obj_t *o = caja(s_mundo);
    lv_obj_set_style_radius(o, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(o, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    if (borde) {
        lv_obj_set_style_border_width(o, 2, 0);
        lv_obj_set_style_border_color(o, lv_color_hex(borde), 0);
    }
    ocultar(o, true);
    return o;
}
static void crear_hoyo(void)
{
    s_hoyo_borde = elipse_tierra(0x9A6A44, 0x5E3B22);
    s_hoyo_pared = elipse_tierra(0x6A4326, 0);
    s_hoyo = elipse_tierra(0x1E0F07, 0);
    s_monton[0] = elipse_tierra(0x7A4E2E, 0x4E2F1A);
    s_monton[1] = elipse_tierra(0x9A6A44, 0);
    s_monton[2] = elipse_tierra(0xB88458, 0);
    for (int i = 0; i < 4; i++) s_terrones[i] = elipse_tierra(i & 1 ? 0x6A4326 : 0xA87A50, 0x4E2F1A);
}
/* la pelota con la que juega sola y lo que se prueba (ropa u objeto) sobre la cabeza */
static lv_obj_t *s_pel_sola, *s_prueba_img;
static uint8_t s_prueba_cual;
static void animar_extras(void)
{
    const pet_t *p = s_pet;
    const criatura_t *c = &s_mascota;
    const bool vive = p && !p->dormida && p->etapa != STAGE_HUEVO && !s_interior && s_escena == (int)p->escena && s_juego == JG_NADA && s_panel == PANEL_NINGUNO && c->visible;
    const bool pelota = vive && p->actividad == ACTV_PELOTA;
    const bool prueba = vive && p->actividad == ACTV_PROBAR && p->probando;
    if (!s_pel_sola && pelota) {
        s_pel_sola = lv_image_create(s_mundo);
        lv_image_set_src(s_pel_sola, art_icono(ICO_PELOTA, 3));
        lv_obj_remove_flag(s_pel_sola, LV_OBJ_FLAG_CLICKABLE);
        ocultar(s_pel_sola, true);
    }
    if (s_pel_sola) {
        ocultar(s_pel_sola, !pelota);
        if (pelota) {
            const float k = (float)s_ms / 650.0f, bx = c->x + sinf(k) * 52.0f, by = c->y - 4.0f - fabsf(sinf(k * 2.0f)) * 30.0f;
            lv_obj_set_pos(s_pel_sola, (int)bx - 15, (int)by - 28);
        }
    }
    if (!s_prueba_img && prueba) {
        s_prueba_img = lv_image_create(s_mundo);
        lv_obj_remove_flag(s_prueba_img, LV_OBJ_FLAG_CLICKABLE);
        ocultar(s_prueba_img, true);
        s_prueba_cual = 0;
    }
    if (s_prueba_img) {
        ocultar(s_prueba_img, !prueba);
        if (prueba) {
            if (s_prueba_cual != p->probando) {
                s_prueba_cual = p->probando;
                const int q = p->probando;
                const art_icono_t ic = q < 32 ? ICO_ROPA[(q - 1) % ROPA_COUNT] : ICO_OBJETO(q - 32);
                lv_image_set_src(s_prueba_img, art_icono(ic, 3));
                if (!s_nino) globo(ic, q < 32 ? "¡MIRA, ME PRUEBO ESTO!" : "¡MIRA LO QUE TENGO!", 2200);
            }
            lv_obj_set_pos(s_prueba_img, (int)c->x - 15 + (int)(sinf((float)s_ms / 400.0f) * 3.0f), lv_obj_get_y(c->img) + c->pose.escala * 7 - 20 + (int)(sinf((float)s_ms / 400.0f) * 1.5f)); /* apoyado en la cabeza, igual que lo que lleva puesto */
            lv_obj_move_foreground(s_prueba_img);
        } else s_prueba_cual = 0;
    }
}

static void animar_hoyo(bool cavando, const criatura_t *c)
{
    lv_obj_t *todos[10] = {s_hoyo_borde, s_hoyo_pared, s_hoyo, s_monton[0], s_monton[1], s_monton[2], s_terrones[0], s_terrones[1], s_terrones[2], s_terrones[3]};
    if (cavando) {
        if (!s_cavar_hasta || (int32_t)(s_ms - s_cavar_hasta) > 1500) { /* empezó a cavar aquí */
            s_cavar_desde = s_ms;
            s_hoyo_x = c->x + s_hoyo_dir * 26;
            s_hoyo_y = c->y;
        }
        s_cavar_hasta = s_ms;
    }
    const uint32_t desde_fin = s_ms - s_cavar_hasta;
    if (!s_cavar_hasta || desde_fin > 5600) { for (int i = 0; i < 10; i++) ocultar(todos[i], true); s_cavar_hasta = cavando ? s_cavar_hasta : 0; return; }
    float k = (float)(s_cavar_hasta - s_cavar_desde) / 6000.0f;
    if (k > 1) k = 1;
    /* al terminar: unos segundos con el hoyo abierto (lo encontrado) y luego lo tapa con la tierra del montón */
    const float tapa = desde_fin < 3000 ? 0.0f : (desde_fin > 4600 ? 1.0f : (float)(desde_fin - 3000) / 1600.0f);
    const float desv = desde_fin > 4600 ? 1.0f - (float)(desde_fin - 4600) / 1000.0f : 1.0f;
    if (tapa > 0 && tapa < 1 && s_ms % 140 < TICK_MS) { tierra((int)s_hoyo_x + (int)(azar() % 16) - 8, (int)s_hoyo_y - 4); if (s_ms % 280 < TICK_MS) polvo((int)s_hoyo_x + (int)(azar() % 20) - 10, (int)s_hoyo_y - 2, 0xB89870); }
    if (tapa == 0.0f && desde_fin > 250 && desde_fin < 2800 && s_ms % 360 < TICK_MS) particula((int)s_hoyo_x + (int)(azar() % 22) - 11, (int)s_hoyo_y - 6, azar() & 1 ? 0xFFF3A0 : 0xFFFFFF, 1, (azar01() - 0.5f) * 1.5f, -3.0f - azar01() * 1.5f, 3, 0); /* algo brilla en el fondo */
    const int hw = 16 + (int)(28 * k), hh = 7 + (int)(10 * k);
    const int hx = (int)s_hoyo_x, hy = (int)s_hoyo_y;
    const int fw = (int)((float)hw * (1.0f - tapa)), fh = (int)((float)hh * (1.0f - tapa));
    /* borde de tierra removida (se queda como parche cuando se tapa) */
    lv_obj_set_size(s_hoyo_borde, hw + 14, hh + 8);
    lv_obj_set_pos(s_hoyo_borde, hx - (hw + 14) / 2, hy - (hh + 8) / 2);
    lv_obj_set_style_bg_color(s_hoyo_borde, lv_color_hex(tapa > 0.9f ? 0x8B5E3C : 0x9A6A44), 0);
    lv_obj_set_size(s_hoyo_pared, fw + 4 > 4 ? fw + 4 : 4, fh + 3 > 3 ? fh + 3 : 3);
    lv_obj_set_pos(s_hoyo_pared, hx - (fw + 4) / 2, hy - (fh + 3) / 2 - 1);
    lv_obj_set_size(s_hoyo, fw - 4 > 2 ? fw - 4 : 2, fh - 2 > 2 ? fh - 2 : 2);
    lv_obj_set_pos(s_hoyo, hx - (fw - 4) / 2, hy - (fh - 2) / 2 + 2);
    ocultar(s_hoyo_pared, fw < 6); ocultar(s_hoyo, fw < 8);
    /* el montón de tierra a un costado: va creciendo y se achica al taparlo */
    const int mx = (int)(s_hoyo_x - s_hoyo_dir * 52), my = (int)s_hoyo_y;
    const float ms_ = 1.0f - tapa * 0.95f;
    const int mw = (int)((14 + 26 * k) * ms_), mh = (int)((7 + 14 * k) * ms_);
    const int w3[3] = {mw, mw * 7 / 10, mw * 38 / 100}, h3[3] = {mh * 8 / 10, mh, mh * 7 / 10}, dy3[3] = {0, 3, 6};
    for (int i = 0; i < 3; i++) {
        lv_obj_set_size(s_monton[i], w3[i] > 3 ? w3[i] : 3, h3[i] > 3 ? h3[i] : 3);
        lv_obj_set_pos(s_monton[i], mx - w3[i] / 2, my - h3[i] - dy3[i] * mh / 14);
        ocultar(s_monton[i], mw < 5);
    }
    static const int8_t TX[4][2] = {{-18, 4}, {16, 2}, {-8, 8}, {22, -6}};
    for (int i = 0; i < 4; i++) { /* terrones sueltos alrededor del hoyo */
        lv_obj_set_size(s_terrones[i], 5 + (i & 1) * 2, 4 + (i & 1));
        lv_obj_set_pos(s_terrones[i], hx + TX[i][0] * hw / 40 - 3, hy + TX[i][1] - 2);
        ocultar(s_terrones[i], k < 0.3f || tapa > 0.5f);
    }
    for (int q = 0; q < 10; q++) lv_obj_set_style_opa(todos[q], (lv_opa_t)(255.0f * desv), 0);
    ocultar(s_hoyo_borde, false);
    const int32_t sb = lv_obj_get_index(c->sombra), rb = lv_obj_get_index(s_hoyo_borde); /* en el suelo, debajo de ella */
    if (rb + 10 != sb) for (int q = 9; q >= 0; q--) lv_obj_move_to_index(todos[q], sb > rb ? rb : sb);
}

static void animar_tierra(void)
{
    for (int i = 0; i < N_TIERRA; i++) {
        if (lv_obj_has_flag(s_tierra[i], LV_OBJ_FLAG_HIDDEN)) continue;
        s_tx_[i] += s_tvx[i]; s_ty_[i] += s_tvy[i];
        s_tvida[i]++;
        if (s_ttipo[i] == 2) { /* polvo: se agranda, sube despacio y se desvanece */
            s_tsz[i] += 2.6f;
            const int v = s_tvida[i] > s_tmax[i] ? s_tmax[i] : s_tvida[i];
            lv_obj_set_size(s_tierra[i], (int)s_tsz[i], (int)s_tsz[i]);
            lv_obj_set_style_bg_opa(s_tierra[i], (lv_opa_t)(130 - 130 * v / (s_tmax[i] + 1)), 0);
            s_tx_[i] -= 1.3f; /* el centro se mantiene mientras crece */
            if (s_tvida[i] >= s_tmax[i]) ocultar(s_tierra[i], true);
        } else {
            s_tvy[i] += s_ttipo[i] == 1 ? 0.9f : 1.2f;
            if (s_ttipo[i] == 1) { /* gotita: se achica al caer */
                if (s_tsz[i] > 2.0f && s_tvida[i] > 3) { s_tsz[i] -= 0.5f; lv_obj_set_size(s_tierra[i], (int)s_tsz[i], (int)s_tsz[i]); }
                if (s_tvida[i] > 3) { const int o = 255 - 20 * (s_tvida[i] - 3); lv_obj_set_style_bg_opa(s_tierra[i], (lv_opa_t)(o > 40 ? o : 40), 0); }
                if (s_tvy[i] > 6 || s_tvida[i] > 14) ocultar(s_tierra[i], true);
            } else if (s_tvy[i] > 7) ocultar(s_tierra[i], true);
        }
        lv_obj_move_foreground(s_tierra[i]);
        lv_obj_set_pos(s_tierra[i], (int)s_tx_[i], (int)s_ty_[i]);
    }
    for (int i = 0; i < N_ONDITA; i++) { /* los anillos del agua: se abren y se apagan */
        if (!s_ondita_t[i]) continue;
        const int t = s_ondita_t[i]++;
        const int w = 10 + t * 8, h = 4 + t * 3;
        lv_obj_set_size(s_ondita[i], w, h);
        lv_obj_set_pos(s_ondita[i], (int)s_ondita_x[i] - w / 2, (int)s_ondita_y[i] - h / 2);
        lv_obj_set_style_border_opa(s_ondita[i], (lv_opa_t)(220 - t * 24 > 0 ? 220 - t * 24 : 0), 0);
        lv_obj_set_style_border_width(s_ondita[i], t < 4 ? 3 : 2, 0);
        lv_obj_move_to_index(s_ondita[i], lv_obj_get_index(s_mascota.sombra));
        if (t >= 9) { s_ondita_t[i] = 0; ocultar(s_ondita[i], true); }
    }
}

static bool s_aplastada;
static uint32_t s_ultimo_sueno;

/* lo que sueña sale de lo que vivió: su amigo, lo que le gusta, lo último que hizo contigo...
 * (y a veces, si tuvo un susto, un mal sueño). Una nube oscura con estrellitas. */
static void sonar_algo(const pet_t *p)
{
    art_icono_t op[12];
    int n = 0;
    op[n++] = ICO_CORAZON; /* tú */
    if (p->vecino.nombre[0] && p->vecino.amistad > 25) op[n++] = ICO_AMIGOS;
    int f = pet_favorite(p, GUSTO_COMIDA, false);
    static const art_icono_t COMIDA[FOOD_COUNT] = {ICO_COMIDA, ICO_FRUTA, ICO_MIEL, ICO_TAZA, ICO_VASO};
    op[n++] = f >= 0 && f < FOOD_COUNT ? COMIDA[f] : (p->habitos.gusto[FOOD_MIEL] > p->habitos.gusto[FOOD_FRUTA] ? ICO_MIEL : ICO_FRUTA);
    if (s_ultimo_t - p->ultimo_juego < 12u * 3600u) op[n++] = ICO_PELOTA;
    if (pet_scene_open(p, ESC_LAGO)) op[n++] = ICO_PEZ;
    if (pet_scene_open(p, ESC_BOSQUE) || p->escena == ESC_PRADO) op[n++] = ICO_MARIPOSA;
    int mejor = ESC_PRADO;
    for (int e = 0; e < ESC_COUNT; e++) if (pet_scene_open(p, (escena_t)e) && pet_scene_liking(p, (escena_t)e) > pet_scene_liking(p, (escena_t)mejor)) mejor = e;
    op[n++] = ICO_ZONA[mejor];
    for (int i = 0; i < PET_PARCELAS; i++) if (p->jardin[i].tipo == PLANTA_FLOR) { op[n++] = ICO_FLOR; break; }
    if (p->memoria.clima[CLIMA_NIEVE] > 30) op[n++] = ICO_COPO;
    art_icono_t a = op[azar() % (uint32_t)n], b = ICO_ESTRELLA;
    /* casi siempre sueña con lo que vivió hoy: dos recuerdos del día en la misma nube */
    int v1 = recuerdo_de_hoy(p, -1);
    if (v1 >= 0 && azar() % 5) {
        a = ICO_VIVIDO[v1];
        int v2 = recuerdo_de_hoy(p, v1);
        b = v2 >= 0 ? ICO_VIVIDO[v2] : ICO_CARA_FELIZ;
    }
    bool pesadilla = p->necesidad[NEED_SEGURIDAD] < 40 && azar() % 3 == 0;
    if (pesadilla) { a = p->clima == CLIMA_TORMENTA ? ICO_RAYO : ICO_ALERTA; b = ICO_CARA_MIEDO; }
    estilo_nube(true);
    poner_src(s_pens_emo, art_icono(a, 4));
    poner_src(s_pens_causa, art_icono(b, 4));
    ocultar(s_pens_causa, false);
    ocultar(s_pens, false); ocultar(s_pens_b1, false); ocultar(s_pens_b2, false);
    s_pens_hasta = s_ms + 5000;
}


/* ======================================================================
 * Los animales del bosque: erizo, ardilla, conejo, caracol, tortuga, pato, búho y topo
 * ==================================================================== */
typedef struct {
    lv_obj_t *img;
    float x, y, tx, ty;
    bool visible;
    int estado;          /* animal_suceso_t que se está viendo (ANI_LLEGA: nada especial) */
    uint32_t estado_hasta, proximo;
    uint32_t visto_desde; /* desde cuándo está a la vista (para contarlo como visto juntos) */
    bool contado, espejo;
} animal_vista_t;
static animal_vista_t s_ani[ANIMAL_COUNT];
#define N_RASTRO 8
static lv_obj_t *s_rastro[N_RASTRO];
static float s_rastro_x[N_RASTRO], s_rastro_y[N_RASTRO];
static uint32_t s_rastro_t[N_RASTRO];
static int s_rastro_i;
static const art_icono_t ICO_ANIMAL[ANIMAL_COUNT] = {ICO_ERIZO, ICO_ARDILLA, ICO_CONEJO, ICO_CARACOL, ICO_TORTUGA, ICO_PATO, ICO_BUHO, ICO_TOPO,
    ICO_COLIBRI, ICO_CARPINTERO, ICO_MARTIN, ICO_GOLONDRINA, ICO_CISNE, ICO_URRACA, ICO_CUERVO, ICO_GARZA,
    ICO_MAPACHE, ICO_MURCIELAGO, ICO_NUTRIA, ICO_CASTOR, ICO_RATON, ICO_OSO, ICO_CIERVO_CAFE, ICO_TEJON, ICO_ZORRILLO,
    ICO_GATO_MONTES, ICO_LINCE, ICO_PUMA, ICO_LAGARTIJA, ICO_SALAMANDRA, ICO_CAMALEON, ICO_CULEBRA,
    ICO_MARIQUITA, ICO_LIBELULA, ICO_GRILLO, ICO_HORMIGAS, ICO_ESCARABAJO, ICO_POLILLA, ICO_PINGUINO, ICO_FOCA, ICO_OSO_POLAR};
static const art_icono_t ICO_ANIMAL_B[ANIMAL_COUNT] = {ICO_ERIZO_B, ICO_ARDILLA_B, ICO_CONEJO_B, ICO_CARACOL_B, ICO_TORTUGA_B, ICO_PATO_B, ICO_BUHO_B, ICO_TOPO_B, ICO_COLIBRI_B, ICO_CARPINTERO_B, ICO_MARTIN_B, ICO_GOLONDRINA_B, ICO_CISNE_B, ICO_URRACA_B, ICO_CUERVO_B, ICO_GARZA_B, ICO_MAPACHE_B, ICO_MURCIELAGO_B, ICO_NUTRIA_B, ICO_CASTOR_B, ICO_RATON_B, ICO_OSO_B, ICO_CIERVO_CAFE_B, ICO_TEJON_B, ICO_ZORRILLO_B, ICO_GATO_MONTES_B, ICO_LINCE_B, ICO_PUMA_B, ICO_LAGARTIJA_B, ICO_SALAMANDRA_B, ICO_CAMALEON_B, ICO_CULEBRA_B, ICO_MARIQUITA_B, ICO_LIBELULA_B, ICO_GRILLO_B, ICO_HORMIGAS_B, ICO_ESCARABAJO_B, ICO_POLILLA_B, ICO_PINGUINO, ICO_FOCA, ICO_OSO_POLAR};
/* cómo se mueven los de la versión 23 */
typedef enum { AE_VUELA = 0, AE_POSA, AE_CAMINA, AE_SALTA, AE_NADA, AE_REPTA, AE_REVOLOTEA } animal_estilo_t;
static const uint8_t ESTILO_ANIMAL[ANIMAL_COUNT - ANIMAL_V22] = {
    AE_REVOLOTEA, AE_POSA, AE_VUELA, AE_VUELA, AE_NADA, AE_SALTA, AE_SALTA, AE_CAMINA,
    AE_CAMINA, AE_VUELA, AE_NADA, AE_NADA, AE_SALTA, AE_CAMINA, AE_CAMINA, AE_CAMINA, AE_CAMINA,
    AE_CAMINA, AE_CAMINA, AE_CAMINA, AE_REPTA, AE_REPTA, AE_REPTA, AE_REPTA,
    AE_REVOLOTEA, AE_REVOLOTEA, AE_SALTA, AE_REPTA, AE_REPTA, AE_REVOLOTEA, AE_CAMINA, AE_CAMINA, AE_CAMINA};
static int escala_animal(int a) { return a == ANIMAL_OSO_POLAR || a == ANIMAL_OSO || a == ANIMAL_CIERVO || a == ANIMAL_PUMA || a == ANIMAL_CISNE || a == ANIMAL_GARZA ? 5 : (a == ANIMAL_HORMIGAS || a == ANIMAL_MARIQUITA || a == ANIMAL_RATON || a == ANIMAL_CARACOL || a == ANIMAL_POLILLA || a == ANIMAL_GRILLO ? 3 : 4); }

static const char *const OPINA_ANIMAL[ANIMAL_COUNT][2] = {
    {"¡QUÉ PINCHOS TAN BONITOS!", "ME GUSTARÍA SER TAN VALIENTE COMO TÚ"}, /* erizo */
    {"¡ERES TAN RÁPIDA! ¿ME ENSEÑAS A TREPAR?", "¿DÓNDE ESCONDES TUS TESOROS?"}, /* ardilla */
    {"¡QUÉ OREJAS TAN SUAVES!", "ALGÚN DÍA SALTARÉ TAN ALTO COMO TÚ"}, /* conejo */
    {"QUÉ CALMA TIENES... ASÍ QUIERO VIVIR", "LLEVAS TU CASITA A TODAS PARTES"}, /* caracol */
    {"DEBE SABER MUCHAS COSAS...", "CON TANTOS AÑOS, ¿QUÉ HABRÁS VISTO?"}, /* tortuga */
    {"¡CUÁ! ¿JUGAMOS EN EL AGUA?", "ME ENCANTA CÓMO NADAS"}, /* pato */
    {"¡QUÉ OJOS TAN GRANDES! LO VE TODO", "ALGÚN DÍA ME QUEDARÉ DESPIERTA CONTIGO"}, /* buho */
    {"¡HOLA, GRUÑÓN! ¡NO TE ESCONDAS!", "¿CÓMO SERÁ VIVIR BAJO LA TIERRA?"}, /* topo */
    {"¡QUÉ RÁPIDO BATES LAS ALAS!", "¡ES COMO UNA JOYA VOLADORA!"}, /* colibri */
    {"TOC, TOC... ¡QUÉ TRABAJADOR!", "¿NO TE DUELE LA CABEZA?"}, /* carpintero */
    {"¡QUÉ PUNTERÍA PARA PESCAR!", "SU AZUL BRILLA COMO EL AGUA"}, /* martin */
    {"¡CUÁNTO VIAJAS! CUÉNTAME LO QUE VISTE", "¡VUELAS COMO EL VIENTO!"}, /* golondrina */
    {"¡QUÉ ELEGANTE! PARECE DE CUENTO", "¡QUÉ HERMOSO ES! ME HACE SENTIR EN PAZ"}, /* cisne */
    {"¡QUÉ BRILLANTE! ¿QUÉ TESORO ESCONDES?", "¡QUÉ AVE TAN LISTA!"}, /* urraca */
    {"PARECE SERIO, PERO ES MUY LISTO", "¡QUÉ PLUMAS TAN NEGRAS Y BRILLANTES!"}, /* cuervo */
    {"¡QUÉ HERMOSA! ALGÚN DÍA VOLARÉ COMO ELLA", "TAN QUIETA... ¡QUÉ PACIENCIA!"}, /* garza */
    {"¡TIENE ANTIFAZ! ¡QUÉ TRAVIESO!", "SUS MANITOS SON TAN HÁBILES"}, /* mapache */
    {"¡CÓMO ME GUSTARÍA VER EN LA OSCURIDAD!", "DUERME AL REVÉS... ¡QUÉ GRACIOSO!"}, /* murcielago */
    {"¡QUÉ JUGUETONA! ¡HAGAMOS UNA CARRERA!", "¡ME ENCANTA CÓMO FLOTA!"}, /* nutria */
    {"¡QUÉ GRAN CONSTRUCTOR!", "ALGÚN DÍA HAREMOS UN REFUGIO COMO EL TUYO"}, /* castor */
    {"¡QUÉ PEQUEÑITO Y VALIENTE!", "TIENE LOS BIGOTES TAN FINITOS"}, /* raton */
    {"¡QUÉ GRANDE! PERO PARECE BUENITO", "¡QUÉ ABRIGO TAN CALENTITO!"}, /* oso */
    {"¡QUÉ ELEGANTE! SUS CUERNOS SON UN ÁRBOL", "QUÉ SILENCIO... ¡NO LO ASUSTEMOS!"}, /* ciervo */
    {"¡QUÉ RAYAS TAN LINDAS!", "DE NOCHE SALE A PASEAR, COMO YO"}, /* tejon */
    {"SHHH... ¡MEJOR NO LO ASUSTEMOS!", "¡QUÉ COLA TAN ESPONJOSA!"}, /* zorrillo */
    {"¡QUÉ SIGILOSO! CASI NO SE OYE", "¡ES UN GATITO SALVAJE!"}, /* gato_montes */
    {"¡QUÉ PINCELITOS EN LAS OREJAS!", "LO VE TODO DESDE LEJOS"}, /* lince */
    {"¡QUÉ FUERTE Y SILENCIOSO! ME DA RESPETO", "¡QUÉ SUERTE HABERLO VISTO!"}, /* puma */
    {"¡QUÉ RÁPIDA! ¿ME ESPERAS?", "ADORA TOMAR SOL, IGUAL QUE YO"}, /* lagartija */
    {"¡QUÉ MANCHITAS AMARILLAS!", "LE GUSTA LA LLUVIA, ¡COMO A MÍ!"}, /* salamandra */
    {"¡CAMBIA DE COLOR! ¿ME ENSEÑAS?", "¡QUÉ OJOS TAN RAROS Y GRACIOSOS!"}, /* camaleon */
    {"¡ES MUY SUAVE Y TRANQUILA!", "SE MUEVE COMO UNA CINTA VERDE"}, /* culebra */
    {"¡PINTITAS! ¿CUÁNTAS TIENE?", "¡QUÉ ROJA Y BRILLANTE!"}, /* mariquita */
    {"¡SUS ALAS PARECEN VIDRIO!", "VUELA COMO UN HELICÓPTERO DE AGUA"}, /* libelula */
    {"¡CANTA TAN BONITO DE NOCHE!", "¡CRI-CRI! ¿ME ENSEÑAS A CANTAR?"}, /* grillo */
    {"¡QUÉ TRABAJADORAS! TODAS JUNTAS", "LLEVAN COSAS MÁS GRANDES QUE ELLAS"}, /* hormigas */
    {"¡QUÉ CAPARAZÓN TAN BRILLANTE!", "BRILLA COMO UNA JOYITA"}, /* escarabajo */
    {"¡LA ATRAEN LAS LUCES!", "¡SUS ALAS PARECEN POLVO SUAVE!"}, /* polilla */
    {"¡QUÉ ELEGANTE ANDAS! ¿VAS A UNA FIESTA?", "¡ME ENCANTA CÓMO TE DESLIZAS!"}, /* pingüino */
    {"¡QUÉ BIGOTES TAN SUAVES!", "CON TANTA CALMA, DAN GANAS DE DORMIR"}, /* foca */
    {"¡QUÉ BLANQUITO ERES!", "¡NO HAGAS RUIDO, QUE SE ASUSTA!"}, /* osezno polar */
};
/* ===================== (fw 103) tocar animales: lo que se ve =====================
 * El motor decide (pet_animal_touch / pet_fauna_touch) y aquí se muestra. El olor del zorrillo NO la ensucia
 * como el barro: le salen vahos verdes del cuerpo, moscas dando vueltas y arruga la nariz. */
static uint32_t s_toq_ms;
void ui_toque_resultado(int r) { s_toq_res = r; s_toq_ms = s_ms; } /* lo llama main.c al volver del motor */

/* el olor: vahos que suben de su cuerpo y un par de moscas en órbita */
#define N_VAHO 5
#define N_MOSCA 2
static lv_obj_t *s_olor[N_VAHO], *s_mosca[N_MOSCA];
static void olor_actualizar(void)
{
    const bool hay = s_pet && pet_stinky(s_pet) && !s_interior && s_mascota.visible && s_escena >= 0 && s_panel == PANEL_NINGUNO && !s_pecera;
    if (!s_olor[0]) {
        if (!hay) return;
        for (int i = 0; i < N_VAHO; i++) {
            s_olor[i] = caja(s_mundo);
            lv_obj_set_style_radius(s_olor[i], LV_RADIUS_CIRCLE, 0);
            lv_obj_set_style_bg_color(s_olor[i], lv_color_hex(0xCBE86B), 0);
            lv_obj_set_style_border_width(s_olor[i], 2, 0);
            lv_obj_set_style_border_color(s_olor[i], lv_color_hex(0x46700F), 0);
            lv_obj_remove_flag(s_olor[i], LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
        }
        for (int i = 0; i < N_MOSCA; i++) {
            s_mosca[i] = caja(s_mundo);
            lv_obj_set_size(s_mosca[i], 5, 5);
            lv_obj_set_style_radius(s_mosca[i], LV_RADIUS_CIRCLE, 0);
            lv_obj_set_style_bg_color(s_mosca[i], lv_color_hex(0x241C10), 0);
            lv_obj_set_style_bg_opa(s_mosca[i], LV_OPA_COVER, 0);
            lv_obj_remove_flag(s_mosca[i], LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
        }
    }
    if (!hay) { for (int i = 0; i < N_VAHO; i++) ocultar(s_olor[i], true); for (int i = 0; i < N_MOSCA; i++) ocultar(s_mosca[i], true); return; }
    const float cx = s_mascota.x, cy = s_mascota.y - 24.0f;
    for (int i = 0; i < N_VAHO; i++) { /* cada vaho sube desde el cuerpo, se ensancha y se desvanece */
        const uint32_t ciclo = 2200u, fase = (s_ms + (uint32_t)i * (ciclo / N_VAHO)) % ciclo;
        const float v = (float)fase / (float)ciclo;
        const int d = 8 + (int)(v * 17.0f);
        const float lado = (i % 2 ? 1.0f : -1.0f) * (10.0f + (float)(i % 3) * 7.0f);
        lv_obj_set_size(s_olor[i], d, d);
        lv_obj_set_pos(s_olor[i], (int)(cx + lado + sinf(v * 5.0f + (float)i) * 7.0f) - d / 2, (int)(cy - v * 46.0f) - d / 2);
        /* bien visible sobre el pasto: verde amarillento claro, no el verde del fondo */
        lv_obj_set_style_bg_opa(s_olor[i], (lv_opa_t)(v < 0.2f ? (int)(v * 5.0f * 205.0f) : (int)(205.0f * (1.0f - (v - 0.2f) / 0.8f))), 0);
        lv_obj_set_style_border_opa(s_olor[i], (lv_opa_t)(v < 0.5f ? 190 : (int)(190 * (1.0f - (v - 0.5f) * 2.0f))), 0);
        ocultar(s_olor[i], false);
        lv_obj_move_foreground(s_olor[i]);
    }
    for (int i = 0; i < N_MOSCA; i++) { /* las moscas que le dan vueltas */
        const float a = (float)s_ms / (i ? 230.0f : 310.0f) + (float)i * 2.1f;
        lv_obj_set_pos(s_mosca[i], (int)(cx + cosf(a) * (26.0f + (float)i * 9.0f)) - 2, (int)(cy - 12.0f + sinf(a * 1.7f) * 13.0f) - 2);
        ocultar(s_mosca[i], false);
        lv_obj_move_foreground(s_mosca[i]);
    }
}
void ui_demo_toque(int res) /* (pruebas) ver cada reacción sin tener que encontrar al animal */
{
    float ax = s_mascota.x + 90.0f, ay = s_mascota.y;
    if (ax > (float)(W - 70)) ax = s_mascota.x - 90.0f;
    toque_mostrar(res, ax, ay);
}
/* lo que se ve y se dice en cada resultado */
static void toque_mostrar(int res, float ax, float ay)
{
    const float mx = s_mascota.x, my = s_mascota.y;
    (void)mx;
    switch (res) {
    case PET_TOC_AVISA:
        flotar(ICO_ENOJO, NULL, 0, (int)ax, (int)ay - 30, 0);
        flotar(ICO_ALERTA, NULL, 0, (int)ax + 16, (int)ay - 38, 180);
        if (!s_nino) globo(ICO_ALERTA, "¡SE ENOJÓ! MEJOR NO LO MOLESTAMOS MÁS", 2800);
        accion(UI_ACCION_SONAR, SND_GRUNIDO);
        break;
    case PET_TOC_PICA:
        for (int k = 0; k < 3; k++) flotar(ICO_ENOJO, NULL, 0, (int)mx - 14 + k * 14, (int)my - 70 - k * 6, (uint32_t)k * 120);
        globo(ICO_LAGRIMA, s_nino ? NULL : "¡AYYY! ¡ME PICÓ!", 3000);
        s_sacude_desde = s_ms ? s_ms : 1; /* un estremecón */
        break;
    case PET_TOC_PUAS:
        flotar(ICO_NARIZ, NULL, 0, (int)mx, (int)my - 70, 0);
        flotar(ICO_LAGRIMA, NULL, 0, (int)mx + 16, (int)my - 60, 200);
        globo(ICO_NARIZ, s_nino ? NULL : "¡AY, MI NARICITA! ¡PINCHA!", 3000);
        break;
    case PET_TOC_ROCIA:
        for (int k = 0; k < 10; k++) chispa((int)ax + (int)(azar() % 40) - 20, (int)ay - 10 - (int)(azar() % 20), (azar() & 1) ? 0x9ACD32 : 0xC8E06A,
                                            (mx - ax) * 0.012f + (float)((int)(azar() % 7) - 3) * 0.2f, -0.5f - (float)(azar() % 5) * 0.2f);
        flotar(ICO_NUBE, NULL, 0, (int)((mx + ax) / 2), (int)my - 50, 0);
        globo(ICO_NARIZ, s_nino ? NULL : "¡PUAJ! ¡QUÉ OLOR! ¡ME ROCIÓ ENTERA!", 3600);
        s_sacude_desde = s_ms ? s_ms : 1; /* un estremecón */
        break;
    case PET_TOC_RUGE:
        flotar(ICO_ALERTA, NULL, 0, (int)ax, (int)ay - 34, 0);
        globo(ICO_CARA_MIEDO, s_nino ? NULL : "¡AAAH! ¡ES MUY GRANDE! ¡NO LO TOQUES MÁS!", 3400);
        s_sacude_desde = s_ms ? s_ms : 1; /* un estremecón */
        break;
    case PET_TOC_SE_VA:
        if (!s_nino) globo(ICO_PUNTOS, "SE FUE... SE CANSÓ DE QUE LO MOLESTARAN", 2800);
        flotar(ICO_PUNTOS, NULL, 0, (int)ax, (int)ay - 30, 0);
        break;
    case PET_TOC_SALUDA:
        flotar(ICO_CORAZON, NULL, 0, (int)ax, (int)ay - 30, 0);
        break;
    default: break;
    }
}

static void animal_cb(lv_event_t *e)
{
    int a = (int)(intptr_t)lv_event_get_user_data(e);
    accion(UI_ACCION_ANIMAL_TOCAR, pet_card_of_animal((animal_t)a)); /* (v39) si ya lo conoce, a lo mejor le cuenta algo del Rey */
    { /* (fw 103) el primer toque saluda, el siguiente avisa, y si insistes pasa algo (el motor decide) */
        const int veces = toques_animal(5000 + a);
        s_toq_res = PET_TOC_NADA;
        accion(UI_ACCION_TOCAR_ANIMAL, a | (veces > 255 ? 255 : veces) << 8);
        if (s_toq_res > PET_TOC_NADA) toque_mostrar(s_toq_res, s_ani[a].x, s_ani[a].y);
        if (s_toq_res > PET_TOC_AVISA) return; /* ya pasó algo: nada de opiniones encima */
    }
    accion(ACT_ANIMAL, a);
    /* si ya lo conoce, a veces opina de él (no siempre, y no uno tras otro) */
    static uint32_t s_opino_ms;
    bool conoce = false;
    for (int i = 0; s_pet && i < pet_cards_known(s_pet); i++) if (pet_card_get(s_pet, i) == pet_card_of_animal((animal_t)a)) conoce = true;
    if (conoce && s_pet && !s_pet->dormida && s_panel == PANEL_NINGUNO && a != ANIMAL_TOPO && a != ANIMAL_ERIZO && (uint32_t)(s_ms - s_opino_ms) > 6000u && azar() % 100 < 35) {
        s_opino_ms = s_ms;
        globo(ICO_ANIMAL[a], s_nino ? NULL : OPINA_ANIMAL[a][azar() & 1u], 4200);
        if (s_nino) flotar(ICO_CORAZON, NULL, 0, (int)s_ani[a].x, (int)s_ani[a].y - 30, 0);
    }
}
/* su lugar de siempre en la zona */
static void animal_casa(int a, float *x, float *y)
{
    const int16_t *cl = LXY(LUGAR_CLARO), *es = LXY(LUGAR_ESTANQUE), *ar = LXY(LUGAR_ARBOL), *ro = LXY(LUGAR_ROCAS), *se = LXY(LUGAR_SENDERO);
    switch (a) {
    case ANIMAL_ERIZO: *x = ro[0] - 40; *y = ro[1] + 18; break;
    case ANIMAL_ARDILLA: *x = ar[0] + 34; *y = ar[1] + 6; break;
    case ANIMAL_CONEJO: *x = cl[0] + 70; *y = cl[1] + 30; break;
    case ANIMAL_CARACOL: *x = se[0] - 20; *y = se[1] + 10; break;
    case ANIMAL_TORTUGA: *x = es[0] - 60; *y = es[1] + 26; break;
    case ANIMAL_PATO: *x = es[0] + 10; *y = es[1] - 4; break;
    case ANIMAL_BUHO: *x = ar[0] + 6; *y = ar[1] - 70; break;
    case ANIMAL_TOPO: *x = cl[0] - 70; *y = cl[1] + 40; break;
    case ANIMAL_PINGUINO: *x = es[0] - 40; *y = es[1] + 22; break; /* sobre el hielo del lago */
    case ANIMAL_FOCA: *x = es[0] + 56; *y = es[1] + 26; break;
    case ANIMAL_OSO_POLAR: *x = es[0] + 120; *y = es[1] + 34; break;
    default: { /* los de la versión 23: según cómo se mueven, y cada uno un poco corrido para que no se encimen */
        int k = a - ANIMAL_V22, dx = (k * 37) % 90 - 45;
        switch (ESTILO_ANIMAL[k]) {
        case AE_NADA: *x = es[0] + dx * 2; *y = es[1] + 4; if (s_escena == ESC_LAGO) { *y = 120 + (k % 3) * 18; return; } break; /* en el agua del lago */
        case AE_POSA: *x = ar[0] - 30; *y = ar[1] - 30; break;
        case AE_VUELA: case AE_REVOLOTEA: *x = cl[0] + dx * 2; *y = cl[1] - 70 - (k % 3) * 14; break;
        case AE_REPTA: *x = ro[0] + dx; *y = ro[1] + 22; break;
        default: *x = (k % 2 ? se[0] : cl[0]) + dx * 2; *y = (k % 2 ? se[1] : cl[1]) + 20 + (k % 3) * 8; break;
        }
        if (a == ANIMAL_MURCIELAGO && s_escena == ESC_CUEVA) { *x = CX + 40; *y = 120; }
        break;
    }
    }
    limitar(x, y);
}
static void animar_animales(void)
{
    const pet_t *p = s_pet;
    if (!s_ani[0].img) {
        for (int a = 0; a < ANIMAL_COUNT; a++) {
            s_ani[a].img = lv_image_create(s_mundo);
            lv_image_set_src(s_ani[a].img, art_icono(ICO_ANIMAL[a], escala_animal(a)));
            lv_obj_add_flag(s_ani[a].img, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_ext_click_area(s_ani[a].img, 12);
            lv_obj_add_event_cb(s_ani[a].img, animal_cb, LV_EVENT_CLICKED, (void *)(intptr_t)a);
            ocultar(s_ani[a].img, true);
        }
        for (int k = 0; k < N_RASTRO; k++) {
            s_rastro[k] = caja(s_mundo);
            lv_obj_set_size(s_rastro[k], 6, 4);
            lv_obj_set_style_radius(s_rastro[k], 2, 0);
            lv_obj_set_style_bg_color(s_rastro[k], lv_color_hex(0xE8F4FF), 0);
            lv_obj_set_style_bg_opa(s_rastro[k], LV_OPA_COVER, 0);
            ocultar(s_rastro[k], true);
        }
    }
    if (!p) return;
    for (int a = 0; a < ANIMAL_COUNT; a++) {
        animal_vista_t *v = &s_ani[a];
        bool aqui = s_escena == (int)p->escena && pet_animal_here(p, (animal_t)a, s_ultimo_t) && p->etapa != STAGE_HUEVO && !s_interior;
        if (aqui && !v->visible) { /* aparece en su lugar (el caracol y el topo, despacito desde abajo) */
            animal_casa(a, &v->x, &v->y);
            v->tx = v->x; v->ty = v->y;
            v->visto_desde = 0; v->contado = false;
            { /* llegan por cualquier lado: izquierda, derecha, arriba o abajo; por arriba no si ahí hay agua (lago e isla) y no sean voladores */
                const int est = a >= ANIMAL_V22 ? ESTILO_ANIMAL[a - ANIMAL_V22] : -1;
                const bool fija = a == ANIMAL_BUHO || a == ANIMAL_TOPO || (a >= ANIMAL_V22 && (est == AE_POSA || a == ANIMAL_MURCIELAGO || a == ANIMAL_MARIQUITA ||
                                  a == ANIMAL_LIBELULA || a == ANIMAL_POLILLA)) || est == AE_NADA;
                const bool agua_arriba = (s_escena == ESC_LAGO || s_escena == ESC_ISLA) && est != AE_VUELA && est != AE_REVOLOTEA;
                if (!fija) {
                    int lado_e = (int)((s_ms / 7u + (uint32_t)a * 5u) % 4u);
                    if (lado_e == 2 && agua_arriba) lado_e = 3;
                    if (lado_e == 0) v->x = -30; else if (lado_e == 1) v->x = W + 30;
                    else if (lado_e == 2) v->y = -30; else v->y = W + 30;
                }
            }
            v->espejo = v->x > CX; /* llega de frente: mirando hacia adentro de la pantalla, venga del lado que venga */
            v->estado = ANI_LLEGA; v->proximo = s_ms + 2000;
            v->visible = true;
        }
        if (!aqui && v->visible) v->visible = false;
        ocultar(v->img, !v->visible);
        if (!v->visible) continue;
        if (v->estado != ANI_LLEGA && (int32_t)(s_ms - v->estado_hasta) > 0) v->estado = ANI_LLEGA;
        float hx, hy;
        animal_casa(a, &hx, &hy);
        float vel = 1.2f, oy = 0;
        art_icono_t ico = ICO_ANIMAL[a];
        bool espejo = v->espejo; /* sigue mirando hacia donde iba */
        switch (a) {
        case ANIMAL_ERIZO: /* tímido: si ella anda cerca corriendo, bolita */
            if (v->estado == ANI_BOLITA || (hypotf(s_mascota.x - v->x, s_mascota.y - v->y) < 60 && s_mascota.modo != M_LIBRE)) ico = ICO_ERIZO_BOLA;
            else if ((int32_t)(s_ms - v->proximo) > 0) { v->tx = hx + (float)(azar() % 40) - 20; v->ty = hy + (float)(azar() % 16) - 8; v->proximo = s_ms + 4000 + azar() % 3000; }
            vel = 0.6f;
            break;
        case ANIMAL_ARDILLA: case ANIMAL_CONEJO: /* a saltitos */
            if (a == ANIMAL_CONEJO && v->estado == ANI_CORREN) { /* juegan a perseguirse: da vueltas alrededor de ella */
                float ang = (float)s_ms / 500.0f;
                v->tx = s_mascota.x + cosf(ang) * 80; v->ty = s_mascota.y - 10 + sinf(ang) * 30;
                vel = 4.0f;
            } else if ((int32_t)(s_ms - v->proximo) > 0) { v->tx = hx + (float)(azar() % 80) - 40; v->ty = hy + (float)(azar() % 30) - 15; v->proximo = s_ms + 2500 + azar() % 2500; }
            if (hypotf(v->tx - v->x, v->ty - v->y) > 2) oy = -fabsf(sinf((float)s_ms / 90.0f)) * 10;
            if (vel < 2) vel = 2.2f;
            break;
        case ANIMAL_CARACOL: /* despacito, dejando su rastro brillante */
            if ((int32_t)(s_ms - v->proximo) > 0) { v->tx = hx + (float)(azar() % 120) - 60; v->ty = hy + (float)(azar() % 30) - 15; v->proximo = s_ms + 9000; }
            vel = 0.25f;
            if (s_ms % 900 < TICK_MS) { int k = s_rastro_i++ % N_RASTRO; s_rastro_x[k] = v->x - 10; s_rastro_y[k] = v->y + 12; s_rastro_t[k] = s_ms; }
            break;
        case ANIMAL_PATO: /* curioso: la sigue en fila por la orilla */
            if (v->estado == ANI_EN_FILA || hypotf(s_mascota.x - v->x, s_mascota.y - v->y) < 140) {
                v->tx = s_mascota.x + (s_mascota.pose.espejo ? 46 : -46); v->ty = s_mascota.y + 4;
                vel = 2.0f;
            } else if ((int32_t)(s_ms - v->proximo) > 0) { v->tx = hx + (float)(azar() % 60) - 30; v->ty = hy; v->proximo = s_ms + 3000; }
            oy = sinf((float)s_ms / 400.0f) * 2;
            break;
        case ANIMAL_BUHO: /* quieto en su rama; parpadea */
            oy = 0;
            if ((s_ms / 200) % 25 == 0) lv_obj_set_style_opa(v->img, LV_OPA_70, 0); else lv_obj_set_style_opa(v->img, LV_OPA_COVER, 0);
            break;
        case ANIMAL_TOPO: /* asoma y se mete, gruñón */
            ico = (s_ms / 2600) % 3 == 2 ? ICO_TOPO_HOYO : ICO_TOPO;
            break;
        case ANIMAL_TORTUGA:
            if ((int32_t)(s_ms - v->proximo) > 0) { v->tx = hx + (float)(azar() % 30) - 15; v->ty = hy; v->proximo = s_ms + 8000; }
            vel = 0.3f;
            break;
        default: { /* los de la versión 23 */
            int k = a - ANIMAL_V22;
            bool hace = v->estado == ANI_HACE;
            switch (ESTILO_ANIMAL[k]) {
            case AE_VUELA: /* da vueltas en el aire; a veces baja a mirarla */
                if ((int32_t)(s_ms - v->proximo) > 0) {
                    bool baja = azar() % 3 == 0 || hace;
                    v->tx = baja ? s_mascota.x + (azar() % 2 ? 60 : -60) : hx + (float)(azar() % 160) - 80;
                    v->ty = baja ? s_mascota.y - 70 : hy + (float)(azar() % 40) - 20;
                    v->proximo = s_ms + 2500 + azar() % 2500;
                }
                if (a == ANIMAL_MARTIN && hace) { v->tx = LXY(LUGAR_ESTANQUE)[0]; v->ty = LXY(LUGAR_ESTANQUE)[1]; vel = 5.0f; } /* ¡al agua! */
                oy = sinf((float)s_ms / 160.0f) * 5;
                if (vel < 2.4f) vel = 2.6f;
                break;
            case AE_REVOLOTEA: /* chiquitos y zigzagueantes */
                if ((int32_t)(s_ms - v->proximo) > 0) { v->tx = hx + (float)(azar() % 120) - 60; v->ty = hy + (float)(azar() % 60) - 30; v->proximo = s_ms + 1200 + azar() % 1500; }
                if (a == ANIMAL_MARIQUITA && hace) { v->tx = s_mascota.x; v->ty = cabeza_y(&s_mascota) + 6; } /* se le posa encima */
                if (a == ANIMAL_POLILLA) { v->tx = hx + sinf((float)s_ms / 300.0f) * 30; v->ty = hy + cosf((float)s_ms / 300.0f) * 16; } /* baila en círculos */
                oy = sinf((float)s_ms / 90.0f) * 4;
                vel = 1.8f;
                break;
            case AE_POSA: /* en su tronco; el carpintero da picotazos */
                vel = 0.5f;
                if (hace || (s_ms / 1800) % 4 == 0) oy = (s_ms / 90) % 2 ? -2 : 0;
                break;
            case AE_NADA: /* nada tranquilo por el agua */
                if ((int32_t)(s_ms - v->proximo) > 0) { v->tx = hx + (float)(azar() % 100) - 50; v->ty = hy + (float)(azar() % 10) - 5; v->proximo = s_ms + 4000 + azar() % 3000; }
                oy = sinf((float)s_ms / 500.0f) * 2;
                vel = a == ANIMAL_NUTRIA ? 1.2f : 0.6f;
                break;
            case AE_SALTA: /* a saltitos */
                if ((int32_t)(s_ms - v->proximo) > 0) { v->tx = hx + (float)(azar() % 100) - 50; v->ty = hy + (float)(azar() % 30) - 15; v->proximo = s_ms + 2000 + azar() % 3000; }
                if (hypotf(v->tx - v->x, v->ty - v->y) > 2) oy = -fabsf(sinf((float)s_ms / 80.0f)) * 7;
                vel = 1.8f;
                break;
            case AE_REPTA: /* despacito, y a ratos quietos tomando sol */
                if ((int32_t)(s_ms - v->proximo) > 0) { v->tx = hx + (float)(azar() % 80) - 40; v->ty = hy + (float)(azar() % 16) - 8; v->proximo = s_ms + 5000 + azar() % 5000; }
                vel = a == ANIMAL_LAGARTIJA ? 1.6f : 0.35f;
                break;
            default: /* camina: se acerca un poco a ella si está tranquila; los tímidos se quedan lejos */
                if ((int32_t)(s_ms - v->proximo) > 0) {
                    bool timido = a == ANIMAL_LINCE || a == ANIMAL_PUMA || a == ANIMAL_GATO_MONTES || a == ANIMAL_OSO || a == ANIMAL_ZORRILLO;
                    v->tx = hx + (float)(azar() % 140) - 70; v->ty = hy + (float)(azar() % 30) - 15;
                    if (timido && hypotf(s_mascota.x - v->tx, s_mascota.y - v->ty) < 110) v->tx = s_mascota.x < CX ? W - 70 : 70;
                    v->proximo = s_ms + 4000 + azar() % 4000;
                }
                vel = a == ANIMAL_OSO ? 0.5f : 0.9f;
                if (hypotf(v->tx - v->x, v->ty - v->y) > 2) oy = -fabsf(sinf((float)s_ms / 160.0f)) * 2;
                break;
            }
            if (a == ANIMAL_CAMALEON) { /* cambia de color de a poco */
                static const uint32_t COL[4] = {0x5AC060, 0xF2A33A, 0xA080F0, 0x8FD8FF};
                lv_obj_set_style_image_recolor(v->img, lv_color_hex(COL[(s_ms / 2500) % 4]), 0);
                lv_obj_set_style_image_recolor_opa(v->img, (s_ms / 2500) % 4 ? LV_OPA_40 : 0, 0);
            }
            break;
        }
        }
        float dx = v->tx - v->x, dy = v->ty - v->y, d = hypotf(dx, dy);
        if (d > vel) { v->x += dx / d * vel; v->y += dy / d * vel; espejo = dx < 0; }
        const int esc = escala_animal(a), lado = 12 * esc;
        bool fijo = a == ANIMAL_BUHO || a == ANIMAL_TOPO || (a >= ANIMAL_V22 && (ESTILO_ANIMAL[a - ANIMAL_V22] == AE_POSA || a == ANIMAL_MURCIELAGO || a == ANIMAL_MARIQUITA || a == ANIMAL_LIBELULA || a == ANIMAL_POLILLA));
        if (ico == ICO_ANIMAL[a] && (a >= ANIMAL_V22 ? ESTILO_ANIMAL[a - ANIMAL_V22] == AE_VUELA || ESTILO_ANIMAL[a - ANIMAL_V22] == AE_REVOLOTEA : a == ANIMAL_COLIBRI || a == ANIMAL_MURCIELAGO || a == ANIMAL_BUHO)
            ? (s_ms / (a == ANIMAL_BUHO ? 2400 : 170)) % 2 : (ico == ICO_ANIMAL[a] && d > vel && (s_ms / 230) % 2))
            ico = ICO_ANIMAL_B[a];
        if (fijo) espejo = false;
        else if (fabsf(v->tx - v->x) < 1.0f) espejo = v->espejo;
        v->espejo = espejo;
        lv_image_set_src(v->img, espejo ? art_icono_espejo(ico, esc) : art_icono(ico, esc)); /* mirando hacia donde va */
        lv_obj_set_pos(v->img, (int)v->x - lado / 2, (int)(v->y - lado + oy));
        lv_obj_move_foreground(v->img);
        /* lo vieron juntos: unos segundos a la vista, sin paneles encima */
        if (s_panel == PANEL_NINGUNO && !s_interior && v->visible && s_pantalla_visible && s_pet && !s_pet->dormida) { /* dormida no lo conoce */
            if (!v->visto_desde) v->visto_desde = s_ms;
            if (!v->contado && s_ms - v->visto_desde > 3000) { v->contado = true; accion(UI_ACCION_ANIMAL_VISTO, pet_card_of_animal((animal_t)a)); }
        } else v->visto_desde = 0;
    }
    /* el rastro brillante del caracol: se va apagando */
    for (int k = 0; k < N_RASTRO; k++) {
        uint32_t edad = s_ms - s_rastro_t[k];
        bool ver = s_rastro_t[k] && edad < 7000 && s_ani[ANIMAL_CARACOL].visible;
        ocultar(s_rastro[k], !ver);
        if (!ver) continue;
        lv_obj_set_pos(s_rastro[k], (int)s_rastro_x[k], (int)s_rastro_y[k]);
        lv_obj_set_style_opa(s_rastro[k], (lv_opa_t)(220 - edad * 200 / 7000 + (int)(30 * sinf((float)s_ms / 150.0f + k))), 0);
    }
}
/* lo que pasa con ellos */
static void evento_animal(const pet_event_t *e, int hx, int hy)
{
    int a = e->a;
    if (a < 0 || a >= ANIMAL_COUNT) return;
    animal_vista_t *v = &s_ani[a];
    v->estado = e->b;
    v->estado_hasta = s_ms + (e->b == ANI_EN_FILA ? 20000u : 6000u);
    int ax = (int)v->x, ay = (int)v->y - 40;
    switch (e->b) {
    case ANI_LLEGA:
        if (a == ANIMAL_TORTUGA) globo(ICO_TORTUGA, s_nino ? NULL : UNA_DE("¡UNA TORTUGA! DICEN QUE ES MUY SABIA", "¡MIRA, UNA TORTUGA! VA TAN TRANQUILA", "¡WOW, UNA TORTUGA! ¡HOLA, SEÑORA SABIA!"), 3000);
        break;
    case ANI_SE_ASUSTA: globo(ICO_CONEJO, s_nino ? NULL : UNA_DE("¡EL CONEJO SE ASUSTÓ CON LOS TRUENOS!", "¡PUM! ¡EL CONEJO SALIÓ CORRIENDO!", "¡AY, EL CONEJO TIENE MIEDO DE LOS TRUENOS!"), 2500); break;
    case ANI_BOLITA: flotar(ICO_ERIZO_BOLA, NULL, 0, ax, ay, 0); break;
    case ANI_AMIGOS:
        for (int k = 0; k < 3; k++) flotar(ICO_CORAZON, NULL, 0, ax - 20 + k * 20, ay, (uint32_t)k * 150);
        globo(ICO_ERIZO, s_nino ? NULL : UNA_DE("¡EL PICHI YA NO SE ESCONDE! SOMOS AMIGOS", "¡EL PICHI ME TIENE CONFIANZA!", "¡YA NO SE HACE BOLITA CONMIGO! ¡SOMOS AMIGOS!"), 3500);
        mascota_modo(M_SALTA, 1200);
        break;
    case ANI_ROBA:
        flotar(ICO_SEMILLA, NULL, 0, ax, ay, 0);
        globo(ICO_ARDILLA, s_nino ? NULL : UNA_DE("¡EL MONITO DEL MONTE SE LLEVÓ UNA SEMILLA!", "¡MIRA, EL MONITO ROBÓ UNA SEMILLA!", "¡EL MONITO SE ESCAPÓ CON UNA SEMILLA!"), 3000);
        break;
    case ANI_DEVUELVE:
        flotar(ICO_SEMILLA, NULL, 0, hx, hy, 0);
        globo(ICO_ARDILLA, s_nino ? NULL : UNA_DE("¡EL MONITO DESENTERRÓ MI SEMILLA Y ME LA DEVOLVIÓ!", "¡EL MONITO DEL MONTE ME TRAJO MI SEMILLA!", "¡QUÉ HONESTO! ¡EL MONITO DEVOLVIÓ MI SEMILLA!"), 3500);
        break;
    case ANI_CORREN: mascota_modo(M_SALTA, 1500); break;
    case ANI_RASTRO: break;
    case ANI_CONSUELA:
        for (int k = 0; k < 3; k++) flotar(ICO_CORAZON, NULL, 0, ax + k * 16, ay, (uint32_t)k * 200);
        globo(ICO_TORTUGA, s_nino ? NULL : UNA_DE("ME SENTÉ JUNTO A LA TORTUGA Y SE ME PASÓ LA PENA", "LA TORTUGA ME HIZO COMPAÑÍA Y ME SIENTO MEJOR", "CON LA TORTUGA AL LADO, TODO ESTÁ MÁS TRANQUILO"), 3500);
        break;
    case ANI_ULULA: flotar(ICO_NOTA, NULL, 0, ax, ay, 0); break;
    case ANI_ARRULLA: flotar(ICO_NOTA, NULL, 0, ax, ay, 0); flotar(ICO_ZETA, NULL, 0, hx, hy, 600); break;
    case ANI_SE_ESCONDE: flotar(ICO_TOPO_HOYO, NULL, 0, ax, ay + 20, 0); break;
    case ANI_HACE: { /* hace lo suyo: ella lo comenta */
        char t[96];
        snprintf(t, sizeof t, "%s", pet_animal_doing((animal_t)a));
        mayusculas(t);
        v->estado_hasta = s_ms + 8000;
        if (v->visible && s_panel == PANEL_NINGUNO) globo(ICO_ANIMAL[a], s_nino ? NULL : t, 3500);
        switch (a) {
        case ANIMAL_CUERVO: flotar(ICO_PIEDRA, NULL, 0, hx, hy - 20, 600); break;
        case ANIMAL_CARPINTERO: case ANIMAL_GRILLO: flotar(ICO_NOTA, NULL, 0, ax, ay, 0); flotar(ICO_NOTA, NULL, 0, ax + 14, ay - 6, 300); break;
        case ANIMAL_MARTIN: case ANIMAL_CASTOR: case ANIMAL_NUTRIA: case ANIMAL_MAPACHE: salpicar(ax, ay + 40, 0x9ED8FF); break;
        case ANIMAL_MARIQUITA: flotar(ICO_CORAZON, NULL, 0, hx, hy - 20, 1200); break;
        case ANIMAL_CAMALEON: case ANIMAL_URRACA: flotar(ICO_ESTRELLA, NULL, 0, ax, ay, 0); break;
        default: break;
        }
        if (v->visible && (a == ANIMAL_COLIBRI || a == ANIMAL_GOLONDRINA || a == ANIMAL_NUTRIA || a == ANIMAL_MARIQUITA)) mascota_modo(M_SALTA, 1000);
        break;
    }
    default: break;
    }
}

/* la casita del tocón: humito de la chimenea (sube, se agranda y se desvanece) */
static lv_obj_t *s_humo[3];
static void animar_humo(void);
static void animar_humo(void)
{
    const pet_t *p = s_pet;
    if (!s_humo[0]) {
        for (int k = 0; k < 3; k++) {
            s_humo[k] = caja(s_mundo);
            lv_obj_set_style_radius(s_humo[k], LV_RADIUS_CIRCLE, 0);
            lv_obj_set_style_bg_color(s_humo[k], lv_color_hex(0xE8E6F0), 0);
            lv_obj_set_style_bg_opa(s_humo[k], LV_OPA_COVER, 0);
            lv_obj_remove_flag(s_humo[k], LV_OBJ_FLAG_CLICKABLE);
        }
    }
    bool ver = p && (p->decoraciones & (1u << DECOR_CASITA)) && s_escena == ESC_PRADO && !s_interior && s_panel == PANEL_NINGUNO;
    for (int k = 0; k < 3; k++) {
        ocultar(s_humo[k], !ver);
        if (!ver) continue;
        float f = (float)((s_ms + (uint32_t)k * 1000u) % 3000u) / 3000.0f;
        int d = 5 + (int)(f * 9);
        lv_obj_set_size(s_humo[k], d, d);
        lv_obj_set_pos(s_humo[k], CASITA_CHIMENEA_X - d / 2 + (int)(5 * sinf(f * 5.0f)), CASITA_CHIMENEA_Y - (int)(f * 42) - d / 2);
        lv_obj_set_style_opa(s_humo[k], (lv_opa_t)(170 * (1.0f - f)), 0);
    }
}

/* encima de ella cuando está enferma: líneas moradas que ondulan y un bichito de resfrío que parpadea */
static lv_obj_t *s_enf_ondas, *s_enf_virus;
static void animar_enferma(const criatura_t *c, const pet_t *p)
{
    if (!s_enf_ondas) {
        s_enf_ondas = lv_image_create(s_mundo);
        lv_image_set_src(s_enf_ondas, art_icono(ICO_MALESTAR, 2));
        s_enf_virus = lv_image_create(s_mundo);
        lv_image_set_src(s_enf_virus, art_icono(ICO_VIRUS, 2));
    }
    bool ver = p->enferma && !p->dormida && c->visible && !s_interior && s_juego == JG_NADA;
    ocultar(s_enf_ondas, !ver || s_nino); /* los más chicos: solo el color, las mejillas y la postura */
    ocultar(s_enf_virus, !ver || s_nino || (s_ms / 600) % 3 == 2); /* parpadea; los más chicos no lo ven */
    if (!ver) return;
    int hy = cabeza_y(c);
    lv_obj_set_pos(s_enf_ondas, (int)c->x - 12, hy - 34 + (int)(2 * sinf((float)s_ms / 300.0f)));
    lv_obj_set_pos(s_enf_virus, (int)c->x + 22, hy - 18 + (int)(3 * sinf((float)s_ms / 500.0f)));
    lv_obj_move_foreground(s_enf_ondas);
    lv_obj_move_foreground(s_enf_virus);
}

static void animar_mascota(void)
{
    criatura_t *c = &s_mascota;
    const pet_t *p = s_pet;
    if (!p) return;
    float sx = 0, sy = 0;
    bool caminando = false;
    art_pose_t *ps = &c->pose;

    if (p->etapa == STAGE_HUEVO) { /* el huevo se mece en el centro */
        c->tx = c->x = LXY(0)[0];
        c->ty = c->y = LXY(0)[1];
        uint32_t ciclo = c->grietas >= 2 ? 500 : 1200;
        ps->frame = (s_ms / ciclo) % 4 == 1 || (s_ms / ciclo) % 4 == 3;
        ps->espejo = (s_ms / ciclo) % 4 == 3;
        if (c->modo == M_SALTA && (int32_t)(s_ms - c->modo_hasta) < 0) sy = -fabsf(sinf((float)s_ms / 90.0f)) * 10;
        dibujar(c);
        colocar(c, sx, sy);
        return;
    }

    objx_pose(c, p); /* chiquitita, grande, de colores o invisible */
    float vel, salto, temblor;
    ps->boca = false;
    if (s_ms % 1000 < TICK_MS) { /* el ambiente para la ropa mágica, una vez por segundo */
        clima_t cl = pet_weather(p);
        art_set_ambiente((int)pet_season(p, s_ultimo_t), periodo_de(p, s_ultimo_t) == PERIODO_NOCHE, cl == CLIMA_DESPEJADO);
    }
    cuerpo_emocional(c, p, &vel, &salto, &temblor);

    /* fin de una animación de evento */
    if (c->modo != M_LIBRE && (int32_t)(s_ms - c->modo_hasta) > 0) {
        if (c->modo == M_JUEGA) ocultar(s_pelota, true);
        if (c->modo == M_COME || c->modo == M_TRAE || c->modo == M_OFRENDA) ocultar(s_objeto, true);
        c->modo = M_LIBRE;
        c->proximo_paseo = s_ms + 1200;
    }

    /* red de seguridad: el objeto que lleva o come solo se ve mientras lo hace (si otra animación le cambió el modo, no queda flotando) */
    if (s_comida == COMIDA_NADA && c->modo != M_COME && c->modo != M_TRAE && c->modo != M_OFRENDA && !lv_obj_has_flag(s_objeto, LV_OBJ_FLAG_HIDDEN)) ocultar(s_objeto, true);

    /* visitando la casita: ella se queda quieta adentro, sin pasear (ni salir ni aparecer en otro lado) */
    if (s_interior && s_casa_visita && !p->dormida && p->etapa != STAGE_HUEVO && (int32_t)(s_cama_salto - s_ms) > 0) { /* salta en la cama */
        c->x = c->tx = CAMA_X; c->y = c->ty = CAMA_Y - 4;
        const float ph = (float)(s_ms % 620u) / 620.0f;
        ps->expr = EXP_FELIZ; ps->postura = ph < 0.5f ? 1 : -1; ps->orejas = 1; ps->cola = 1;
        ps->frame = ph < 0.5f ? 1 : 0;
        sy = -sinf(ph * 3.1416f) * 36.0f;
        { /* un rato salta de un lado a otro de la cama y otro rato en el mismo lugar */
            const uint32_t seg = s_ms / 2200u;
            const float u = (float)(s_ms % 2200u) / 2200.0f;
            if (seg % 2 == 0) {
                const float dir = (seg / 2) % 2 ? -1.0f : 1.0f;
                sx = dir * sinf(u * 3.1416f) * 80.0f;
                ps->espejo = (dir * cosf(u * 3.1416f)) < 0;
            } else sx = 0;
        }
        if (s_ms % 620u < TICK_MS) { flotar(ICO_NOTA, NULL, 0, (int)c->x + (int)(azar() % 60) - 30, (int)(CAMA_Y - 70), 0); }
        if (s_ms % 1240u < TICK_MS) accion(UI_ACCION_SONAR, SND_ALEGRE);
        dibujar(c);
        colocar(c, sx, sy);
        return;
    }
    if (s_interior && s_casa_visita && !p->dormida && p->etapa != STAGE_HUEVO) {
        c->x = c->tx = VISITA_X; c->y = c->ty = VISITA_Y;
        ps->expr = EXP_FELIZ; ps->postura = 0; ps->orejas = 0; ps->cola = (s_ms / 900) % 2;
        ps->frame = (int)((s_ms / 700) % 2);
        sy = -fabsf(sinf((float)s_ms / 900.0f)) * 1.5f;
        dibujar(c);
        colocar(c, 0, sy);
        return;
    }
    /* despertó en la casita: sale por la puerta */
    if (s_interior && !s_casa_visita && !duerme_en_casita(p)) { /* (si solo la estás visitando, no «despierta»: espera adentro) */
        const bool aqui = !p->dormida && s_escena == ESC_PRADO && (int)p->escena == ESC_PRADO && p->etapa != STAGE_HUEVO;
        if (!aqui) { /* la zona cambió o algo raro: sale sin más */
            salir_casa(p);
            s_casa_fase = 0; casa_negro(0);
            c->tx = LXY(LUGAR_CLARO)[0]; c->ty = LXY(LUGAR_CLARO)[1];
            mascota_modo(M_ZONA_ENTRA, 5000);
        } else {
            if (s_casa_fase != 5 && s_casa_fase != 3) { /* despertó adentro: se queda un ratito, se despereza y luego sale */
                s_casa_fase = 5; s_casa_t = s_ms; s_casa_sale = s_ms + 8000 + azar() % 5000;
                if (s_gorro_dormir) ocultar(s_gorro_dormir, true);
            }
            c->x = c->tx = CAMA_X; c->y = c->ty = CAMA_Y - 6;
            const uint32_t e = s_ms - s_casa_t;
            if (s_casa_fase == 5) {
                ps->orejas = 0; ps->cola = e > 3500 ? 1 : 0;
                if (e < 1500) { ps->expr = EXP_CERRADOS; ps->postura = -2; }
                else if (e < 3500) { ps->expr = EXP_ABURRIDO; ps->postura = 0; ps->boca = (e / 400) % 2 == 0; } /* bostezo */
                else { ps->expr = EXP_FELIZ; ps->postura = 0; }
                if (e > 3500 && e < 3500 + TICK_MS && !s_nino) globo(ICO_SOL, UNA_DE("¡BUENOS DÍAS!", "¡HOLA, BUEN DÍA!", "¡A DESPERTAR! ¡BUENOS DÍAS!", "¡QUÉ BUEN DÍA PARA JUGAR!", "¡BUENOS DÍAS! ¿DORMISTE BIEN?"), 2500);
                if (e > 3500 && e < 3500 + TICK_MS && s_nino) globo(ICO_SOL, NULL, 2500);
                if (s_ms >= s_casa_sale) { s_casa_fase = 3; s_casa_t = s_ms; }
            }
            if (s_casa_fase == 3) {
                const float f = (float)(s_ms - s_casa_t) / 800.0f;
                ps->expr = EXP_FELIZ; ps->postura = 0;
                casa_negro((f - 0.4f) / 0.6f);
                if (f >= 1.0f) { /* sale por la puerta: aparece de a poco afuera y camina al prado */
                    salir_casa(p);
                    s_casa_fase = 4; s_casa_t = s_ms;
                    lv_obj_set_style_opa(c->img, LV_OPA_TRANSP, 0);
                    c->tx = LXY(LUGAR_CLARO)[0]; c->ty = LXY(LUGAR_CLARO)[1];
                    mascota_modo(M_ZONA_ENTRA, 5000);
                    return;
                }
            }
            sy = -fabsf(sinf((float)s_ms / 500.0f)) * (e > 3500 ? 3.0f : 0.0f);
            ps->frame = (int)((s_ms / 700) % 2);
            dibujar(c);
            colocar(c, 0, sy);
            return;
        }
    }

    if (p->dormida && duerme_en_casita(p) && !s_interior) { /* con sueño, camina hasta su casita y entra */
        c->tx = LXY(LUGAR_CASITA)[0]; c->ty = LXY(LUGAR_CASITA)[1];
        bool anda = mover(c, 2.0f);
        ps->expr = EXP_ABURRIDO; ps->orejas = -1; ps->cola = 0; ps->postura = 0;
        ps->frame = (int)((s_ms / 320) % 2);
        dibujar(c);
        colocar(c, 0, anda ? -fabsf(sinf((float)s_ms / 160.0f)) * 2 : 0);
        if (!anda) { /* llegó a la puerta: se desvanece al cruzarla, la pantalla se oscurece y ya está adentro */
            if (s_casa_fase != 1) { s_casa_fase = 1; s_casa_t = s_ms; }
            const float f = (float)(s_ms - s_casa_t) / 700.0f;
            lv_obj_set_style_opa(c->img, (lv_opa_t)(f >= 1 ? 0 : 255 * (1 - f)), 0);
            casa_negro((f - 0.35f) / 0.65f);
            if (f >= 1.0f) { entrar_casa(p); s_casa_fase = 2; s_casa_t = s_ms; lv_obj_set_style_opa(c->img, LV_OPA_COVER, 0); }
        }
        return;
    }
    if (p->dormida) {
        if (s_casa_fase == 5 || s_casa_fase == 3) { s_casa_fase = 0; casa_negro(0); }
        c->tx = LXY(p->lugar)[0]; c->ty = LXY(p->lugar)[1];
        if (s_interior) { c->tx = CAMA_X; c->ty = CAMA_Y; c->x = c->tx; c->y = c->ty; } /* acostada sobre su camita */
        mover(c, 1.5f);
        ps->expr = EXP_CERRADOS; ps->orejas = -1; ps->cola = 0; ps->postura = -2;
        if (s_ms % 1500 < TICK_MS) flotar(ICO_ZETA, NULL, 0, (int)c->x + 18 + (int)((s_ms / 1500) % 3) * 8, cabeza_y(c) + 20, 0);
        if (s_ms - s_ultimo_sueno > 14000 && lv_obj_has_flag(s_pens, LV_OBJ_FLAG_HIDDEN) && s_panel == PANEL_NINGUNO) {
            s_ultimo_sueno = s_ms;
            sonar_algo(p);
        }
        ps->frame = (int)((s_ms / 1400) % 2);
        dibujar(c);
        colocar(c, 0, 0);
        { /* en su cama o su tapete, con el gorrito de dormir puesto */
            if (!s_gorro_dormir) { s_gorro_dormir = lv_image_create(s_mundo); lv_image_set_src(s_gorro_dormir, art_icono(ICO_GORRO_DORMIR, 3)); }
            ocultar(s_gorro_dormir, false);
            lv_obj_set_pos(s_gorro_dormir, (int)c->x - 18, cabeza_y(c) + 12 + (int)(sinf((float)s_ms / 900.0f) * 1.5f));
            lv_obj_move_foreground(s_gorro_dormir);
        }
        return;
    }
    if (s_gorro_dormir && !lv_obj_has_flag(s_gorro_dormir, LV_OBJ_FLAG_HIDDEN)) ocultar(s_gorro_dormir, true);

    if (s_rotada && c->modo != M_TRUCO && c->modo != M_CRISTAL) { s_rotada = false; lv_image_set_rotation(c->img, 0); lv_image_set_scale_x(c->img, 256); lv_image_set_scale_y(c->img, 256); }
    if (c->modo != M_REVERENCIA && s_aura && !lv_obj_has_flag(s_aura, LV_OBJ_FLAG_HIDDEN)) ocultar(s_aura, true);
    if (p->actividad != ACTV_NADAR || p->dormida || c->modo != M_LIBRE || (s_escena != ESC_LAGO && s_escena != ESC_PRADO)) ocultar(s_agua, true);
    if (animar_deslizar(c, p, &sx)) { /* inclinaste el reloj: se resbala */
        ps->frame = 0;
        dibujar(c);
        colocar(c, sx, sy);
        return;
    }
    if (s_pesca_fase > 0 && animar_pesca(c, p, &sx, &sy, &caminando)) { /* va en el bote */
        if (c->modo == M_SALTA) sy = -fabsf(sinf((float)s_ms / 110.0f)) * 14;
        ps->frame = (int)((s_ms / 700) % 2);
        dibujar(c);
        colocar(c, sx, sy);
        return;
    }

    switch (c->modo) {
    case M_ZONA_SALE: /* se va por el borde... */
        caminando = mover(c, 5.0f);
        ps->cola = 1; ps->orejas = 1;
        sy = -fabsf(sinf((float)s_ms / 100.0f)) * 4;
        if (!caminando) {
            s_escena = s_zona_pend >= 0 ? s_zona_pend : (int)p->escena;
            s_zona_pend = -1;
            esconder_fauna();
            refrescar_fondo(p, s_ultimo_t);
            actualizar_jardin(p);
            actualizar_flechas(p);
            punto_borde(s_escena, s_zona_lado ^ 1, &c->x, &c->y); /* entra por el lado de enfrente: oeste/este y norte/sur van de a pares */
            c->tx = LXY(LUGAR_CLARO)[0]; c->ty = LXY(LUGAR_CLARO)[1];
            mascota_modo(M_ZONA_ENTRA, 7000);
        }
        break;
    case M_ZONA_ENTRA: /* ...y entra por el otro lado de la zona nueva */
        caminando = mover(c, 4.5f);
        ps->cola = 1; ps->orejas = 1; ps->expr = EXP_CURIOSO;
        if (caminando) sy = -fabsf(sinf((float)s_ms / 100.0f)) * 4;
        else c->modo_hasta = s_ms;
        break;
    case M_COME: {
        ps->boca = (s_ms / 240) % 2;
        ps->expr = EXP_FELIZ;
        int l = ps->espejo ? -1 : 1;
        lv_obj_set_pos(s_objeto, (int)c->x + l * 30 - 18, (int)c->y - 40);
        break;
    }
    case M_TRAE: { /* viene hacia ti con el regalo en la boca */
        c->tx = CX; c->ty = Y_MAX - 4;
        caminando = mover(c, 4.0f);
        ps->expr = EXP_FELIZ; ps->cola = 1; ps->orejas = 1;
        lv_obj_set_pos(s_objeto, (int)c->x - 18, cabeza_y(c) - 34);
        if (!caminando) sy = -fabsf(sinf((float)s_ms / 120.0f)) * 6;
        break;
    }
    case M_JUEGA: {
        float t = (float)(c->modo_hasta - s_ms) / 1000.0f;
        float bx = CX + sinf(t * 2.1f) * 80, by = c->y - 18 - fabsf(sinf(t * 6.0f)) * 50;
        lv_obj_set_pos(s_pelota, (int)bx - 18, (int)by - 18);
        c->tx = bx; c->ty = LXY(0)[1] + 10;
        mover(c, 5.0f);
        sy = -fabsf(sinf((float)s_ms / 120.0f)) * 10;
        ps->boca = true; ps->expr = EXP_FELIZ; ps->cola = 1; ps->orejas = 1;
        caminando = true;
        break;
    }
    case M_CRISTAL: { /* niño: corre hacia ti, se pega al vidrio y da vueltas de contenta */
        c->tx = CX; c->ty = Y_MAX;
        caminando = mover(c, 6.0f);
        ps->expr = EXP_FELIZ; ps->cola = 1; ps->orejas = 1;
        if (!caminando) {
            int l = lado(c);
            lv_image_set_pivot(c->img, l / 2, l);
            float f = (float)(s_ms % 1600) / 1600.0f;
            if (f < 0.45f) { lv_image_set_scale_x(c->img, 330); lv_image_set_scale_y(c->img, 290); } /* pegada al vidrio */
            else lv_image_set_scale_x(c->img, (uint32_t)(300 * fabsf(cosf((f - 0.45f) * 6.283f * 2))) + 20); /* giros */
            if (f >= 0.45f) lv_image_set_scale_y(c->img, 300);
            s_rotada = true;
        } else sy = -fabsf(sinf((float)s_ms / 80.0f)) * 6;
        break;
    }
    case M_REVERENCIA: /* adulto: inclina la cabeza, contenta, con un halo dorado */
        ps->expr = EXP_FELIZ; ps->postura = -1; ps->orejas = -1; ps->cola = 1;
        if (s_aura) {
            int l = lado(c);
            float a = 0.5f + 0.5f * sinf((float)s_ms / 160.0f);
            lv_obj_set_size(s_aura, l, l * 3 / 4);
            lv_obj_set_pos(s_aura, (int)c->x - l / 2, (int)c->y - l * 3 / 4 - 4);
            lv_obj_set_style_shadow_width(s_aura, 18 + (int)(16 * a), 0);
            lv_obj_set_style_opa(s_aura, (lv_opa_t)(120 + 100 * a), 0);
            ocultar(s_aura, false);
        }
        break;
    case M_ROBOT: /* la llave de cuerda: pasitos de robot de juguete */
        objx_robot(c, ps, &sx, &sy);
        caminando = true;
        break;
    case M_BAILA: { /* se da vueltas al pulso, salta en cada nota y mueve la cola */
        uint32_t tb = s_ms - s_baile_desde;
        int pulso = (int)(tb / BAILE_PULSO_MS);
        float f = (float)(tb % BAILE_PULSO_MS) / (float)BAILE_PULSO_MS;
        ps->expr = EXP_FELIZ; ps->orejas = 1; ps->cola = 1; ps->postura = 1;
        ps->espejo = (pulso / 2) & 1;
        ps->frame = pulso & 1;
        ps->boca = pulso % 4 == 0;
        sy = -fabsf(sinf(f * 3.1416f)) * 14;
        sx = sinf((float)tb / 320.0f) * 10;
        if (pulso != s_baile_nota && pulso < 16) { s_baile_nota = pulso; flotar(ICO_NOTA, NULL, 0, (int)c->x + (pulso & 1 ? 30 : -30), cabeza_y(c) - 10, 0); }
        break;
    }
    case M_OFRENDA: /* lleva el objeto en la boca hasta el árbol hueco y lo deja ahí */
        c->tx = HUECO_X + 44; c->ty = HUECO_Y + 46;
        caminando = mover(c, 4.0f);
        ps->cola = 1; ps->orejas = 1; ps->espejo = true;
        if (!s_ofrenda_llego) lv_obj_set_pos(s_objeto, (int)c->x - 18, cabeza_y(c) - 34);
        if (!caminando && !s_ofrenda_llego) {
            s_ofrenda_llego = true;
            ocultar(s_objeto, true);
            for (int i = 0; i < 4; i++) flotar(ICO_ESTRELLA, NULL, 0, HUECO_X - 10 + i * 20, HUECO_Y + 30, (uint32_t)i * 150);
            accion(UI_ACCION_SONAR, SND_TESORO);
            c->modo_hasta = s_ms + 1800;
        }
        if (s_ofrenda_llego) sy = -fabsf(sinf((float)s_ms / 110.0f)) * 6;
        break;
    case M_VA: /* va adonde la llamaste y te mira */
        caminando = mover(c, 4.5f);
        ps->cola = 1;
        if (caminando) sy = -fabsf(sinf((float)s_ms / 100.0f)) * 5;
        else { ps->espejo = false; ps->orejas = 1; if (s_ms % 2500 < TICK_MS) sy = -8; }
        break;
    case M_TRUCO: animar_truco(c, &sx, &sy); break;
    case M_CHARCO: /* camina hasta el charco y salta tres veces adentro: ¡plic, plic, plic! */
        if (s_charco_llego == 0) {
            caminando = mover(c, 5.0f);
            ps->cola = 1; ps->orejas = 1;
            if (caminando) { sy = -fabsf(sinf((float)s_ms / 100.0f)) * 4; break; }
            s_charco_llego = s_ms;
        }
        {
            uint32_t k = s_ms - s_charco_llego;
            int salto = (int)(k / 520);
            float u = (float)(k % 520) / 520.0f;
            if (salto < 3) {
                sy = -sinf(u * 3.1416f) * (s_charco_gusta ? 16.0f : 7.0f);
                ps->expr = s_charco_gusta ? EXP_FELIZ : EXP_ENOJADO;
                ps->orejas = s_charco_gusta ? 1 : -1;
                ps->boca = s_charco_gusta;
                if (salto >= s_charco_saltos) { /* cayó al agua */
                    s_charco_saltos = salto + 1;
                    int x0 = s_charco_i >= 0 ? POS_CHARCO[s_charco_i][0] : (int)c->x, y0 = s_charco_i >= 0 ? POS_CHARCO[s_charco_i][1] : (int)c->y;
                    for (int q = 0; q < 6; q++) salpicar(x0 - 22 + q * 9, y0 - 6, q & 1 ? 0x9ED8FF : 0x6FA8E0);
                    sonido(SND_SALPICON);
                }
            } else {
                s_charco_i = -1;
                if (s_charco_gusta) { flotar(ICO_NOTA, NULL, 0, (int)c->x, cabeza_y(c), 0); mascota_modo(M_SALTA, 900); }
                else { flotar(ICO_GOTA, NULL, 0, (int)c->x + 24, cabeza_y(c), 0); mascota_modo(M_NIEGA, 900); }
            }
        }
        break;
    case M_DUDA: /* no entendió: ladea la cabeza */
        ps->expr = EXP_CURIOSO; ps->orejas = 1; ps->espejo = (s_ms / 400) % 2;
        break;
    case M_MAREADA: /* la mareaste: se tambalea con los ojos en espiral */
        sx = sinf((float)s_ms / 110.0f) * 6;
        ps->expr = EXP_MAREADO; ps->orejas = -1; ps->cola = -1;
        break;
    case M_ABRAZO: { /* se aplasta contenta contra tu dedo */
        float k = sinf((float)(c->modo_hasta - s_ms) / 1200.0f * 3.1416f);
        int l = lado(c);
        lv_image_set_pivot(c->img, l / 2, l);
        lv_image_set_scale_x(c->img, (uint32_t)(256 + 40 * k));
        lv_image_set_scale_y(c->img, (uint32_t)(256 - 30 * k));
        s_aplastada = true;
        ps->expr = EXP_FELIZ; ps->cola = 1; ps->orejas = -1;
        break;
    }
    case M_SALTA:
        sy = -fabsf(sinf((float)s_ms / 110.0f)) * 18;
        ps->boca = true; ps->cola = 1;
        break;
    case M_NIEGA:
        sx = ((s_ms / 80) % 2) ? 4 : -4; /* sacude la cabeza */
        ps->expr = EXP_ENOJADO; ps->orejas = -1;
        break;
    case M_BANO:
        sx = ((s_ms / 120) % 2) ? 3 : -3;
        ps->expr = EXP_CERRADOS; ps->boca = true;
        break;
    default: {
        if (animar_rito_mascota(c, &sx, &sy, &caminando)) break; /* el rito de la Guardiana */
        if (animar_juego(c, p, &sx, &sy, &caminando)) break;     /* jugando contigo */
        if (animar_comida(c, p, &caminando)) break;              /* la comida que le acercas */
        if (animar_seguir(c, p, &sy, &caminando)) break;         /* te sigue con el dedo */
        if (animar_juguete(c, p, &sx, &sy, &caminando)) break;   /* con sus juguetes */
        if (animar_habilidad(c, p, &sx, &sy, &caminando)) break; /* nada o trepa */
        if (animar_bote_ir(c, p, &caminando)) break;             /* camina por el muelle al bote */
        if (animar_pesca(c, p, &sx, &sy, &caminando)) break;     /* al bote */
        if (animar_pesca_hielo(c, p, &sx, &sy, &caminando)) break; /* (v38) al hoyo en el hielo */
        if (animar_juntos(c, p, &sx, &sy, &caminando)) break;    /* con su amigo */
        /* va al lugar que eligió el motor y hace lo suyo */
        int act = p->actividad;
        float lx = LXY(p->lugar)[0], ly = LXY(p->lugar)[1];
        bool llego = fabsf(c->tx - c->x) < 1 && fabsf(c->ty - c->y) < 1;
        float dlugar = hypotf(c->x - lx, c->y - ly);
        if (act == ACTV_CHARLAR && p->probando && p->probando <= ANIMAL_COUNT && s_ani[p->probando - 1].visible) { /* va junto al animal con el que charla */
            const animal_vista_t *av = &s_ani[p->probando - 1];
            lx = av->x + (c->x < av->x ? -46.0f : 46.0f); ly = av->y;
            dlugar = hypotf(c->x - lx, c->y - ly);
        }
        const bool chapotea_prado = act == ACTV_CHAPOTEAR && s_escena == ESC_PRADO && !s_interior;
        if (chapotea_prado) { lx = 106.0f; ly = 300.0f; dlugar = hypotf(c->x - lx, c->y - ly); } /* chapotea DENTRO del estanque, no en la orilla */
        bool quieta = act == ACTV_SIESTA || act == ACTV_SOMBRA || act == ACTV_ESPERAR || act == ACTV_MIRAR || act == ACTV_REFUGIO || act == ACTV_ABRIGARSE || act == ACTV_AVES || act == ACTV_CHARLAR || act == ACTV_CANTAR || act == ACTV_BAILAR;
        /* excursiones: con las actividades de andar libre, a veces se va a recorrer el mundo (20 a 45 s) en vez de quedarse siempre alrededor de su lugar */
        static uint32_t exc_hasta;
        const bool vaga = act == ACTV_PASEAR || act == ACTV_EXPLORAR || act == ACTV_JUGAR_SOLA || act == ACTV_OLFATEAR || act == ACTV_BUSCAR;
        const bool exc = vaga && (int32_t)(exc_hasta - s_ms) > 0;
        if (((dlugar > 50.0f && !exc) || (chapotea_prado && !es_agua(c->x, c->y))) && llego) { c->tx = lx; c->ty = ly; limitar(&c->tx, &c->ty); }
        else if (llego && (int32_t)(s_ms - c->proximo_paseo) > 0 && !quieta) {
            if (vaga && (exc || azar() % 100 < 40)) { /* a otro rincón del mapa */
                if (!exc) exc_hasta = s_ms + 20000u + azar() % 25000u;
                c->tx = 70.0f + azar01() * 326.0f; c->ty = Y_MIN + 10 + azar01() * (float)(Y_MAX - Y_MIN - 20);
                limitar(&c->tx, &c->ty);
                c->proximo_paseo = s_ms + 1500 + azar() % 2500;
            } else {
                float r = act == ACTV_EXPLORAR || act == ACTV_JUGAR_SOLA || act == ACTV_NIEVE ? 60.0f : 35.0f;
                c->tx = lx + (azar01() * 2 - 1) * r; c->ty = ly + (azar01() * 2 - 1) * r * 0.5f;
                limitar(&c->tx, &c->ty);
                c->proximo_paseo = s_ms + (act == ACTV_OLFATEAR ? 1500 : 2500) + azar() % 3000;
            }
        }
        if (act == ACTV_EXPLORAR || act == ACTV_JUGAR_SOLA) vel *= 1.3f;
        if (act == ACTV_OLFATEAR) vel *= 0.6f;
        caminando = mover(c, vel);
        if (caminando) {
            sy = -fabsf(sinf((float)s_ms / 110.0f)) * salto;
            if (temblor > 0 && p->emo[EMO_ENOJO] > 0.4f) sy += ((s_ms / 160) % 2) ? 1.5f : 0; /* pisa fuerte */
        } else {
            switch (act) {
            case ACTV_SIESTA: ps->expr = EXP_CERRADOS; ps->postura = -2; ps->orejas = -1;
                if (s_ms % 2400 < TICK_MS) flotar(ICO_ZETA, NULL, 0, (int)c->x + 22, cabeza_y(c) + 20, 0);
                break;
            case ACTV_SOMBRA: ps->postura = -2; break;
            case ACTV_OLFATEAR: ps->postura = -1; ps->orejas = 1; ps->expr = EXP_CURIOSO; sy = ((s_ms / 200) % 2) ? 2 : 0; break;
            case ACTV_BUSCAR: ps->postura = -1; ps->orejas = 1; ps->expr = EXP_CURIOSO; sy = ((s_ms / 260) % 2) ? 2 : 0; /* husmea buscando algo rico */
                if (s_ms % 1800 < TICK_MS) flotar(ICO_HOJA, NULL, 0, (int)c->x + (ps->espejo ? -26 : 26), (int)c->y - 30, 0);
                break;
            case ACTV_AVES: /* con la cabeza en alto, sigue a los pajaritos con la vista */
                ps->orejas = 1; ps->expr = EXP_CURIOSO; ps->postura = 1;
                ps->espejo = (s_ms / 2600) % 2;
                if (s_ms % 2600 < TICK_MS) flotar(ICO_PAJARO, NULL, 0, (int)c->x + (ps->espejo ? -34 : 34), cabeza_y(c) - 16, 0);
                break;
            case ACTV_CANTAR: { /* abre la boca al ritmo y suelta notas; cada especie canta a su manera */
                if ((int32_t)(s_callar_hasta - s_ms) > 0) { ps->expr = EXP_ABIERTOS; ps->boca = false; break; } /* la callaron: se queda quieta y tranquila */
                static const uint8_t MECE[8] = {3, 1, 5, 2, 6, 1, 4, 2}; /* cuánto se mece */
                const int e8 = p->especie & 7;
                const uint32_t frase = sfx_frase_ms(e8), nf = s_ms / frase;       /* todo va al compás de la frase de su especie */
                const bool coro_v = (nf / 4) % 2 == 1;                            /* 4 frases cantadas (A A' A B) y 4 tarareadas */
                ps->expr = EXP_FELIZ; ps->orejas = 1; ps->cola = 1; ps->postura = e8 % 3 == 1 ? 2 : 1;
                ps->boca = coro_v ? false : (s_ms % frase) / (frase / 8) % 2;
                sx = sinf((float)s_ms * 6.2831853f / (float)(frase / 2)) * (float)MECE[e8] * (coro_v ? 1.4f : 1.0f);
                if (coro_v) { ps->espejo = (nf / 2) % 2; ps->postura = 0; }
                if (tararea_cancion(p)) { ps->boca = (s_ms / 180) % 2; break; } /* (v38) contenta, a veces tararea nuestra canción */
                if (s_ms % frase < TICK_MS) {
                    flotar(e8 % 4 == 3 || coro_v ? ICO_CORAZON : ICO_NOTA, NULL, 0, (int)c->x + (int)(azar() % 50) - 25, cabeza_y(c) - 8, 0);
                    static const uint8_t ORDEN[4] = {0, 1, 0, 2}; /* tonadita A, A', A, B */
                    accion(UI_ACCION_SONAR, coro_v ? SND_TARAREO_0 + e8 : SND_VOZ_0 + e8 * 3 + ORDEN[nf % 4]);
                }
                break;
            }
            case ACTV_BAILAR: { /* salta al compás, se mece y da vueltitas; cada especie baila distinto */
                static const uint8_t SALTO[8] = {9, 13, 6, 10, 8, 15, 5, 11};      /* qué tan alto salta */
                static const uint8_t MECE[8] = {8, 4, 12, 6, 14, 3, 10, 7};        /* cuánto se desplaza de lado a lado */
                static const uint16_t RITMO[8] = {230, 190, 260, 170, 300, 150, 240, 200}; /* velocidad del salto */
                static const uint16_t GIRO[8] = {500, 350, 700, 300, 900, 250, 600, 400};  /* cada cuánto se da vuelta */
                const int e8 = p->especie & 7;
                /* si suena música: despacito (más mecida), normal o a toda velocidad (más saltos) */
                const float vel = p->probando == 1 ? 0.55f : p->probando == 3 ? 1.6f : 1.0f;
                const float rit = (float)RITMO[e8] / vel;
                ps->expr = EXP_FELIZ; ps->orejas = 1; ps->cola = 1; ps->boca = (s_ms / 400) % 2;
                ps->espejo = (s_ms / (uint32_t)((float)GIRO[e8] / vel)) % 2;
                sy = -fabsf(sinf((float)s_ms / rit)) * (float)SALTO[e8] * (p->probando == 1 ? 0.5f : 1.0f);
                sx = sinf((float)s_ms / (rit * 2)) * (float)MECE[e8] * (p->probando == 1 ? 1.5f : 1.0f);
                if (e8 == 4 || e8 == 6) ps->postura = ((s_ms / RITMO[e8]) % 2) ? 1 : -1; /* se agacha y se estira al compás */
                if ((s_ms / 14000) % 2 == 1) { /* segundo paso: en vez de saltar, se desliza de lado a lado y da una vuelta cada cuatro compases */
                    const uint32_t cp = (uint32_t)rit * 2;
                    const float fase = (float)(s_ms % (cp * 4)) / (float)(cp * 4);
                    sx = (fase < 0.5f ? fase * 4.0f - 1.0f : 3.0f - fase * 4.0f) * (float)MECE[e8] * 2.2f;
                    sy = (e8 & 1) ? -fabsf(sinf((float)s_ms / rit)) * 3.0f : 0.0f;
                    ps->espejo = fase < 0.25f || fase > 0.75f ? false : true;
                    if (fase > 0.9f) sy = -(float)SALTO[e8] * 1.2f * sinf((fase - 0.9f) * 10.0f * 3.14159f);
                    ps->postura = (e8 & 1) ? 0 : (((s_ms / cp) % 2) ? 1 : -1);
                }
                if (s_ms % 900 < TICK_MS) flotar((s_ms / 900) % 2 ? ICO_NOTA : ICO_CORAZON, NULL, 0, (int)c->x + (int)(azar() % 60) - 30, cabeza_y(c) - 10, 0);
                break;
            }
            case ACTV_CHARLAR: { /* conversan por turnos: ella, el animal, ella... */
                const bool tiene = p->probando && p->probando <= ANIMAL_COUNT && s_ani[p->probando - 1].visible;
                if (tiene) ps->espejo = s_ani[p->probando - 1].x < c->x;
                ps->expr = EXP_FELIZ; ps->orejas = 1; ps->cola = 1;
                const bool turno_ella = (s_ms / 1500) % 2 == 0;
                ps->boca = turno_ella && (s_ms / 250) % 2;
                if (s_ms % 1500 < TICK_MS) {
                    if (turno_ella) flotar(ICO_NOTA, NULL, 0, (int)c->x + (ps->espejo ? -20 : 20), cabeza_y(c) - 10, 0);
                    else if (tiene) flotar(ICO_CORAZON, NULL, 0, (int)s_ani[p->probando - 1].x, (int)s_ani[p->probando - 1].y - 36, 0);
                }
                break;
            }
            case ACTV_CHAPOTEAR: { /* se agacha, salta alto, cae de golpe: se aplasta, salpica en corona y abre anillos en el agua */
                ps->expr = EXP_FELIZ; ps->boca = true; ps->cola = 1; ps->orejas = 1;
                if (s_interior) break; /* dentro de la casa no hay agua: ni salpicones ni anillos */
                const uint32_t T = 1000, ph = s_ms % T, cyc = s_ms / T;
                static uint32_t s_ch_cyc = 0xFFFFFFFFu; static int s_ch_fase;
                ps->espejo = (cyc / 2) % 2;
                float fx = 1.0f, fy = 1.0f, h = 0.0f;
                int fase;
                if (ph < 220) { const float f = (float)ph / 220.0f; fy = 1.0f - 0.15f * f; fx = 1.0f + 0.10f * f; fase = 0; } /* se agacha */
                else if (ph < 600) { const float u = (float)(ph - 220) / 380.0f; h = 26.0f * 4.0f * u * (1.0f - u); fy = 1.0f + 0.10f * sinf(u * 3.1416f); fx = 1.0f - 0.06f * sinf(u * 3.1416f); fase = 1; ps->postura = 1; } /* vuela */
                else if (ph < 780) { const float f = (float)(ph - 600) / 180.0f; fy = 0.80f + 0.20f * f; fx = 1.18f - 0.18f * f; fase = 2; } /* cae y se aplasta */
                else { const float f = (float)(ph - 780) / 220.0f; fy = 1.0f + 0.04f * sinf(f * 6.28f) * (1.0f - f); fx = 1.0f; fase = 3; }
                sy = -h;
                if (fase == 1 && s_ch_fase == 0) { for (int k = 0; k < 4; k++) particula((int)c->x - 8 + k * 5, (int)c->y - 4, k & 1 ? 0xE8F8FF : 0x9AD8FF, 1, (k - 1.5f) * 1.2f, -2.5f - azar01() * 2.0f, 4, 0); } /* al despegar, unas gotitas */
                if (fase == 2 && (s_ch_fase != 2 || s_ch_cyc != cyc)) { /* aterriza: la corona de agua */
                    s_ch_cyc = cyc;
                    for (int k = 0; k < 12; k++) {
                        const float a = -3.1416f * ((float)k + 0.5f) / 12.0f; /* abanico hacia arriba */
                        particula((int)c->x - 3 + (int)(cosf(a) * 6.0f), (int)c->y - 4, k % 3 == 0 ? 0xFFFFFF : (k & 1 ? 0xE8F8FF : 0x8FD0FF), 1, cosf(a) * (3.0f + azar01() * 3.0f), -4.5f + sinf(a) * (3.0f + azar01() * 3.0f), 3 + (int)(azar() % 4), 0);
                    }
                    ondita((int)c->x, (int)c->y + 1);
                    if (azar() & 1) ondita((int)c->x + (ps->espejo ? -18 : 18), (int)c->y + 3);
                    polvo((int)c->x, (int)c->y - 4, 0xD8F0FF);
                    sonido(SND_SALPICON);
                }
                s_ch_fase = fase;
                {
                    const int l = lado(c);
                    lv_image_set_pivot(c->img, l / 2, l);
                    lv_image_set_scale_x(c->img, (uint32_t)(256.0f * fx));
                    lv_image_set_scale_y(c->img, (uint32_t)(256.0f * fy));
                    s_squash = true;
                }
                if (s_ms % 320 < TICK_MS) ondita((int)c->x + (int)(azar() % 30) - 15, (int)c->y + 2 + (int)(azar() % 4)); /* anillitos que van quedando */
                break;
            }
            case ACTV_PELOTA: { /* le da a la pelota y va tras ella */
                ps->expr = EXP_FELIZ; ps->cola = 1; ps->orejas = 1;
                ps->espejo = cosf((float)s_ms / 650.0f) < 0;
                sy = -fabsf(sinf((float)s_ms / 160.0f)) * 5.0f;
                break;
            }
            case ACTV_PROBAR: /* se mira, se acomoda lo que se puso y da una vueltita */
                ps->expr = EXP_FELIZ; ps->orejas = 1; ps->cola = 1; ps->boca = (s_ms / 900) % 3 == 0;
                ps->espejo = (s_ms / 1300) % 2;
                if ((s_ms / 1300) % 4 == 3) sy = -fabsf(sinf((float)s_ms / 140.0f)) * 7.0f;
                break;
            case ACTV_CAVAR: { /* inclinada hacia adelante, escarba rápido y tira la tierra para atrás */
                ps->postura = -1; ps->orejas = 1; ps->cola = 1; ps->expr = EXP_CURIOSO;
                float dir = ps->espejo ? -1.0f : 1.0f;
                if (s_cavar_hasta && (int32_t)(s_ms - s_cavar_hasta) <= 1500) dir = s_hoyo_dir; /* ya empezó: sigue mirando al hoyo */
                else { /* ¿el hoyo caería en el agua? entonces mira hacia el otro lado; si no hay tierra, se aparta */
                    if (es_agua(c->x + dir * 26, c->y)) dir = -dir;
                    if (es_agua(c->x + dir * 26, c->y)) { c->tx = c->x + 40; c->ty = c->y + 40; limitar(&c->tx, &c->ty); if (es_agua(c->tx, c->ty)) { c->tx = c->x - 40; limitar(&c->tx, &c->ty); } break; }
                }
                ps->espejo = dir < 0;
                s_hoyo_dir = dir;
                /* ritmo: cinco paleadas seguidas, una pausa para mirar el hoyo y vuelve a empezar */
                const uint32_t ph = s_ms % 1500;
                const bool descansa = ph >= 1000;
                bool golpe = !descansa && (ph / 100) % 2;
                static bool s_cv_golpe;
                if (descansa) { ps->postura = 0; ps->expr = EXP_ORGULLO; ps->cola = 1; sy = -fabsf(sinf((float)(ph - 1000) / 500.0f * 6.2832f)) * 2.0f; }
                sx = dir * (golpe ? 5.0f : 1.0f);  /* se tira hacia el hoyo y vuelve */
                sy += golpe ? 4.0f : 0.0f;         /* con la cabeza abajo */
                if (golpe && !s_cv_golpe) { /* en cada paleada: terrones de distinto tamaño, una piedrita y una nubecita de polvo */
                    const int px = (int)c->x + (int)(dir * 16);
                    tierra_atras(px, (int)c->y - 4, -dir); tierra_atras(px - (int)dir * 4, (int)c->y - 2, -dir); tierra_atras(px, (int)c->y - 6, -dir);
                    polvo(px, (int)c->y - 2, 0xB89870);
                }
                s_cv_golpe = golpe;
                {
                    const int l = lado(c);
                    lv_image_set_pivot(c->img, l / 2, l);
                    lv_image_set_rotation(c->img, (int32_t)(dir * (golpe ? 50.0f : (descansa ? 0.0f : -30.0f)))); /* se balancea con el esfuerzo */
                    lv_image_set_scale_y(c->img, golpe ? 242 : 256);
                    lv_image_set_scale_x(c->img, golpe ? 266 : 256);
                    s_squash = true;
                }
                s_cavando = true;
                break;
            }
            case ACTV_MIRAR: ps->orejas = 1; ps->expr = EXP_CURIOSO; ps->espejo = (s_ms / 3000) % 2; break;
            case ACTV_NIEVE: case ACTV_JUGAR_SOLA: sy = -fabsf(sinf((float)s_ms / 130.0f)) * 12; ps->cola = 1; ps->boca = true; break;
            case ACTV_ESPERAR: ps->orejas = 1; ps->cola = 1; ps->espejo = false; if (s_ms % 4000 < TICK_MS) sy = -6; break;
            case ACTV_REFUGIO: ps->postura = -1; break;
            case ACTV_ABRIGARSE: ps->postura = -2; ps->orejas = -1; ps->expr = EXP_CERRADOS; break; /* acurrucada */
            default:
                if (salto > 3 && s_ms % 3000 < 400) sy = -fabsf(sinf((float)s_ms / 70.0f)) * salto * 2;
                break;
            }
        }
        if (temblor > 0) sx += ((s_ms / 80) % 2) ? temblor : -temblor;
        /* celos: le da la espalda a la visita */
        if (s_visita.visible && p->emo[EMO_CELOS] > 0.4f && !caminando) ps->espejo = s_visita.x > c->x;
        break;
    }
    }

    if (s_squash && !(c->modo == M_LIBRE && (p->actividad == ACTV_CHAPOTEAR || p->actividad == ACTV_CAVAR))) {
        s_squash = false;
        lv_image_set_rotation(c->img, 0);
        lv_image_set_scale_x(c->img, 256);
        lv_image_set_scale_y(c->img, 256);
    }
    if (s_aplastada && c->modo != M_ABRAZO) {
        s_aplastada = false;
        lv_image_set_scale_x(c->img, 256);
        lv_image_set_scale_y(c->img, 256);
    }
    uint32_t periodo = caminando ? (vel < 2.0f ? 320 : 200) : 700;
    if (p->enferma && c->modo == M_LIBRE) { /* enferma: más baja, todo al doble o triple de lento y escalofríos cada tanto */
        periodo = periodo * 5 / 2;
        sy = sy * 0.4f + 2.0f;
        if (s_ms % 4200 < 360) sx += ((s_ms / 90) % 2) ? 2.0f : -2.0f;
    }
    ps->frame = (int)((s_ms / periodo) % 2);
    if (pet_floating(p, s_ultimo_t) && !s_interior) { /* (v38) el polvo de hadas: flota un poquito sobre el suelo, con destellos */
        sy += -16.0f + sinf((float)s_ms / 420.0f) * 4.0f;
        if (s_ms % 900 < TICK_MS) flotar(ICO_ESTRELLA, NULL, 0, (int)c->x - 20 + (int)(azar() % 40), (int)c->y - 6, 0);
    }
    dibujar(c);
    colocar(c, sx, sy);
}

static void animar_visita(void)
{
    criatura_t *v = &s_visita;
    if (!v->visible) return;
    bool camina = mover(v, 3.5f);
    v->pose.expr = EXP_ABIERTOS;
    if (v->modo == M_CAMINA && !camina) {
        v->modo = M_QUIETA;
        v->pose.espejo = s_mascota.x < v->x;
        if (v->encuentro >= 0) { encuentro(v->encuentro); v->encuentro = -1; }
    }
    if (v->amigo && v->modo == M_QUIETA && (int32_t)(s_ms - v->se_va) > 0) {
        v->modo = M_SALE;
        v->tx = v->x < CX ? 10 : W - 10;
        flotar(ICO_CORAZON, NULL, 0, (int)v->x, cabeza_y(v), 0);
    }
    if (v->modo == M_SALE && !camina) { v->amigo = 0; mostrar_criatura(v, false); return; }
    if (v->modo == M_QUIETA) { v->pose.expr = EXP_FELIZ; v->pose.cola = 1; }
    v->pose.frame = (int)((s_ms / (camina ? 200 : 800)) % 2);
    dibujar(v);
    colocar(v, 0, 0);
    if (v->y > s_mascota.y) { lv_obj_move_foreground(v->sombra); lv_obj_move_foreground(v->img); }
    else { lv_obj_move_foreground(s_mascota.sombra); lv_obj_move_foreground(s_mascota.img); }
}

static void animar_luces(void)
{
    if (s_fondo_periodo != PERIODO_NOCHE && s_escena != ESC_CUEVA) return;
    if (s_escena == ESC_CUEVA) { /* los cristales no vuelan: titilan en su lugar */
        static const int16_t CRISTAL[N_LUCES][2] = {{96, 140}, {112, 154}, {213, 74}, {366, 104}, {336, 178}, {104, 128}, {372, 92}};
        for (int i = 0; i < N_LUCES; i++) {
            lv_obj_set_pos(s_luces[i], CRISTAL[i][0], CRISTAL[i][1]);
            s_luz_f[i] += 0.05f;
            float b = 0.5f + 0.5f * sinf(s_luz_f[i] * 1.6f + i);
            lv_obj_set_style_bg_opa(s_luces[i], (lv_opa_t)(40 + 215 * b * b), 0);
        }
        return;
    }
    for (int i = 0; i < N_LUCES; i++) {
        s_luz_f[i] += 0.07f;
        s_luz_x[i] += sinf(s_luz_f[i] * 0.7f + i) * 0.8f;
        s_luz_y[i] += cosf(s_luz_f[i] * 0.5f + i * 2) * 0.5f;
        if (s_luz_x[i] < 70) s_luz_x[i] = 70;
        if (s_luz_x[i] > 396) s_luz_x[i] = 396;
        if (s_luz_y[i] < 110) s_luz_y[i] = 110;
        if (s_luz_y[i] > 330) s_luz_y[i] = 330;
        lv_obj_set_pos(s_luces[i], (int)s_luz_x[i], (int)s_luz_y[i]);
        float b = 0.5f + 0.5f * sinf(s_luz_f[i] * 2.0f);
        lv_obj_set_style_bg_opa(s_luces[i], (lv_opa_t)(60 + 195 * b), 0);
    }
}

static void preparar_particulas(void)
{
    int n = 0, w = 3, h = 3;
    uint32_t col = 0xFFFFFF;
    lv_opa_t opa = LV_OPA_80;
    static const art_icono_t ICO_CLIMA[CLIMA_COUNT] = {ICO_SOL, ICO_NUBE, ICO_LLUVIA, ICO_VIENTO, ICO_COPO, ICO_RAYO, ICO_FRIO};
    bool adentro = s_escena == ESC_CUEVA || s_interior;
    switch (adentro ? -1 : s_fondo_clima) { /* en la cueva y en la casita no llueve */
    case CLIMA_LLUVIA: n = N_PART; w = 3; h = 16; col = 0xBFE3FF; opa = LV_OPA_COVER; break;
    case CLIMA_TORMENTA: n = N_PART; w = 3; h = 22; col = 0xD0E8FF; opa = LV_OPA_COVER; break;
    case CLIMA_NIEVE: n = N_PART; w = 6; h = 6; col = 0xFFFFFF; opa = LV_OPA_90; break;
    case CLIMA_FRIO: n = 12; w = 3; h = 3; col = 0xE8F4FF; opa = LV_OPA_70; break; /* cristalitos de hielo en el aire */
    case CLIMA_VIENTO:
        n = 14; w = 9; h = 5;
        col = s_fondo_estacion == ESTACION_OTONO ? 0xF08A3C : (s_fondo_estacion == ESTACION_PRIMAVERA ? 0xFFB8D8 : (s_fondo_estacion == ESTACION_INVIERNO ? 0xFFFFFF : 0x8FD36A));
        break;
    default: break;
    }
    for (int i = 0; i < N_PART; i++) {
        ocultar(s_part[i], i >= n);
        if (i >= n) continue;
        lv_obj_set_size(s_part[i], w, h);
        lv_obj_set_style_bg_color(s_part[i], lv_color_hex(col), 0);
        lv_obj_set_style_bg_opa(s_part[i], opa, 0);
        lv_obj_set_style_radius(s_part[i], s_fondo_clima == CLIMA_NIEVE ? 3 : 1, 0);
    }
    bool noche_clara = s_fondo_periodo == PERIODO_NOCHE && s_fondo_clima == CLIMA_DESPEJADO;
    if (s_fondo_clima >= 0 && s_fondo_clima < CLIMA_COUNT) poner_src(s_clima_ico, art_icono(noche_clara ? ICO_LUNA : ICO_CLIMA[s_fondo_clima], 3));
    ocultar(s_clima_ico, adentro || s_fondo_clima < 0 || s_fondo_clima >= CLIMA_COUNT); /* adentro no se ve el cielo */
    /* la noche de luna llena: la luna sale grande en el cielo */
    bool llena = noche_clara && !adentro && s_escena != ESC_CUEVA && s_ultimo_t && pet_nights_to_full_moon(s_ultimo_t) == 0;
    if (llena && !s_luna_grande) {
        static art_img_t luna;
        art_astro(&luna, 1, 2);
        s_luna_grande = lv_image_create(lv_obj_get_parent(s_clima_ico));
        lv_image_set_src(s_luna_grande, &luna.dsc);
        lv_obj_set_pos(s_luna_grande, 92, 40);
        lv_obj_move_to_index(s_luna_grande, lv_obj_get_index(s_mundo)); /* en el cielo, detrás de ella y de todo lo que se mueve en el mundo */
    }
    if (s_luna_grande) ocultar(s_luna_grande, !llena);
    if (llena) ocultar(s_clima_ico, true);
}

static void animar_particulas(void)
{
    int c = s_escena == ESC_CUEVA || s_interior ? -1 : s_fondo_clima;
    if (c != CLIMA_LLUVIA && c != CLIMA_NIEVE && c != CLIMA_VIENTO && c != CLIMA_TORMENTA && c != CLIMA_FRIO) return;
    for (int i = 0; i < N_PART; i++) {
        if (lv_obj_has_flag(s_part[i], LV_OBJ_FLAG_HIDDEN)) continue;
        float *x = &s_part_x[i], *y = &s_part_y[i], v = s_part_v[i];
        if (c == CLIMA_LLUVIA) { *y += 26 * v; *x -= 4 * v; }
        else if (c == CLIMA_TORMENTA) { *y += 34 * v; *x -= 10 * v; }
        else if (c == CLIMA_NIEVE) { *y += 3.2f * v; *x += sinf((s_ms / 400.0f) + i) * 1.4f; }
        else if (c == CLIMA_FRIO) { *y += 0.8f * v; *x += sinf((s_ms / 700.0f) + i) * 0.8f; lv_obj_set_style_opa(s_part[i], (s_ms / 300 + i) % 4 ? LV_OPA_COVER : LV_OPA_40, 0); }
        else { *x += 16 * v; *y += sinf((s_ms / 200.0f) + i * 1.7f) * 3.0f; }
        if (*y > W) { *y = -14; *x = azar01() * W; }
        if (*x > W) { *x = -10; *y = 60 + azar01() * (W - 120); }
        if (*x < -14) *x = W;
        lv_obj_set_pos(s_part[i], (int)*x, (int)*y);
    }
}

volatile int g_ui_tick_ms, g_ui_tick_max;
int g_dbg_fase; /* (pruebas) qué animación del latido corre ahora */ /* v89: cuánto tarda cada latido de la interfaz (las animaciones), para el registro */
static void tick_cb_cuerpo(void);
static void tick_cb(lv_timer_t *t)
{
    (void)t;
#ifdef ESP_PLATFORM
    const int64_t t0 = esp_timer_get_time();
    tick_cb_cuerpo();
    const int ms = (int)((esp_timer_get_time() - t0) / 1000);
    g_ui_tick_ms = (g_ui_tick_ms * 7 + ms) / 8;
    if (ms > g_ui_tick_max) g_ui_tick_max = ms;
#else
    tick_cb_cuerpo();
#endif
}
static void tick_cb_cuerpo(void)
{
#ifdef ESP_PLATFORM
    { /* cuadros lentos (un panel que se arma, una escena pesada): quedan en el registro con el panel abierto */
        static int64_t antes_us; static uint32_t aviso_ms;
        const int64_t ahora_us = esp_timer_get_time();
        const int dt = antes_us ? (int)((ahora_us - antes_us) / 1000) : 0;
        antes_us = ahora_us;
        if (dt > 200 && (uint32_t)(ahora_us / 1000) - aviso_ms > 1000) {
            aviso_ms = (uint32_t)(ahora_us / 1000);
            hal_log("cuadro lento: %d ms entre latidos de la interfaz (panel %d, escena %d)", dt, (int)s_panel, ui_escena_especial());
        }
    }
#endif
    s_ms += TICK_MS;
    animar_pesca_juego();
    animar_musica(); /* (v38) el reproductor de canciones */
    g_dbg_fase = 1;
    animar_bienvenida();
    g_dbg_fase = 2;
    animar_viaje();
    g_dbg_fase = 3;
    animar_cruce();
    g_dbg_fase = 4;
    animar_cielo();
    g_dbg_fase = 5;
    animar_moscas(&s_mascota);
    g_dbg_fase = 6;
    animar_baile();
    g_dbg_fase = 7;
    animar_notas_cristal();
    g_dbg_fase = 8;
    animar_bateria_aviso();
    g_dbg_fase = 9;
    animar_carta();
    g_dbg_fase = 10;
    animar_guardiana(&s_mascota, s_pet && s_pet->guardiana && !s_interior);
    g_dbg_fase = 11;
    animar_rito();
    animar_ceremonias(); /* (v39) */
    textos_revisar(); /* (v96) los textos que con la letra más grande no caben */
    animar_zona();
    animar_entradas_zonas();
    g_dbg_fase = 12;
    animar_alarma();
    g_dbg_fase = 13;
    animar_reserva();
    g_dbg_fase = 14;
    animar_juegos();
    g_dbg_fase = 15;
    animar_raros();
    g_dbg_fase = 16;
    animar_ventana();
    g_dbg_fase = 17;
    animar_fechas();
    g_dbg_fase = 18;
    animar_juguetes_mundo(s_pet);
    g_dbg_fase = 19;
    animar_pasajero();
    g_dbg_fase = 20;
    animar_fauna();
    g_dbg_fase = 21;
    animar_mascota();
    g_dbg_fase = 22;
    if (s_pet) animar_enferma(&s_mascota, s_pet);
    olor_actualizar(); /* (fw 103) el olor del zorrillo: vahos y moscas, encima de ella */
    g_dbg_fase = 23;
    animar_humo();
    g_dbg_fase = 24;
    animar_animales();
    g_dbg_fase = 25;
    animar_pochi();
    g_dbg_fase = 26;
    animar_nochevieja();
    { /* el estremecimiento del mundo al sacudir el reloj: fuerte al principio y se calma */
        static bool temblando;
        const uint32_t t = s_ms - s_sacude_desde;
        if (s_sacude_desde && t < 900u) {
            const float f = 1.0f - (float)t / 900.0f;
            lv_obj_set_style_translate_x(s_mundo, (int32_t)(sinf((float)s_ms / 22.0f) * 7.0f * f), 0);
            lv_obj_set_style_translate_y(s_mundo, (int32_t)(cosf((float)s_ms / 17.0f) * 5.0f * f), 0);
            temblando = true;
        } else if (temblando) { lv_obj_set_style_translate_x(s_mundo, 0, 0); lv_obj_set_style_translate_y(s_mundo, 0, 0); temblando = false; }
    }
    g_dbg_fase = 27;
    animar_mundo23();
    g_dbg_fase = 28;
    animar_cumple();
    g_dbg_fase = 29;
    animar_magia();
    g_dbg_fase = 30;
    animar_objx();
    g_dbg_fase = 31;
    animar_esponja();
    g_dbg_fase = 32;
    animar_obj_mundo();
    g_dbg_fase = 33;
    animar_niebla();
    g_dbg_fase = 34;
    animar_tapete();
    g_dbg_fase = 35;
    animar_hoyo(s_cavando, &s_mascota);
    s_cavando = false;
    g_dbg_fase = 36;
    animar_extras();
    g_dbg_fase = 37;
    animar_vecino();
    g_dbg_fase = 38;
    bote_al_frente();
    g_dbg_fase = 39;
    animar_visita();
    g_dbg_fase = 40;
    animar_luces();
    g_dbg_fase = 41;
    animar_particulas();
    g_dbg_fase = 42;
    animar_tierra();
    g_dbg_fase = 43;
    seguir_globos();
}

/* ======================================================================
 * Prueba de los paneles: que nada quede fuera del círculo ni tape la X o la flecha de volver
 * ==================================================================== */
#define PRUEBA_PANELES 85
#ifndef MARGEN_PRUEBA
#define MARGEN_PRUEBA 12 /* px hacia adentro del vidrio */
#endif
static const char *const PRUEBA_NOMBRE[PRUEBA_PANELES] = {
    "comer", "jugar", "trucos", "mas-ella", "mas-recuerdos", "mas-ajustes", "rincon", "amigos", "frase", "diario",
    "fotos", "camara", "tesoros", "mapa", "bolsa", "libro", "sonido", "pantalla", "hora-menu", "alarmas",
    "alarma", "dormir", "temporizador", "perfiles", "respaldo", "mas-aj", "modo", "rapido", "color", "aspecto",
    "nombre", "fecha", "hora", "familia", "bote", "carta", "cartas", "resumen", "semilla", "pide-viaje",
    "rincon-ropa", "rincon-juguetes", "objeto", "genero", "carta-hoja", "historia", "pais", "ritmo", "pantalla2", "pecera",
    "verso-propuesta", "verso-tilo", "verso-nuestra", "verso-prisma", "verso-todos", "biblioteca", "comida-temporada", "ella-barras", "paleta-0", "paleta-1", "paleta-2", "paleta-3", "paleta-4", "paleta-5", "paleta-6", "paleta-7", "verso-cuchara",
    "verso-nombre", "verso-carta", "prisma", "tilde-chisme", "brizna", "pagina-tilo", "pagina-tilo-leer", "animales", "rito-pantalla", "rito-hoja", "nota-tilde",
    "cristales-menu", "canciones", "canciones-5", "libro-narradores", "verso-fin", "rito-oro", "version"};
int ui_panel_actual(void) { return (int)s_panel; }
int ui_test_count(void) { return PRUEBA_PANELES; }
int ui_juego_actual(void) { return (int)s_juego; }
/* (pruebas) deja solo el fondo de una pantalla para sacarle una foto: 0 elegir huevo, 1 juego de la fruta, 2 picnic */
lv_obj_t *ui_fondo_solo(int k)
{
    if (k == 0) {
        if (!s_tarjeta || s_panel != PANEL_ASPECTO) return NULL;
        const uint32_t n = lv_obj_get_child_count(s_tarjeta);
        for (uint32_t i = 0; i < n; i++) {
            lv_obj_t *c = lv_obj_get_child(s_tarjeta, (int32_t)i);
            if (lv_obj_get_user_data(c) != (void *)(uintptr_t)0xAD0) lv_obj_add_flag(c, LV_OBJ_FLAG_HIDDEN);
        }
        return s_tarjeta;
    }
    if (k == 1) {
        if (s_juego != JG_FRUTAS) return NULL;
        ocultar(s_canasta, true); ocultar(s_fr_bar, true); ocultar(s_salir, true);
        for (int i = 0; i < N_FRUTAS_J; i++) ocultar(s_fr_img[i], true);
        ocultar(s_mascota.img, true); ocultar(s_mascota.sombra, true);
        return s_juego_capa;
    }
    if (k == 2) {
        if (!s_picnic) picnic_crear();
        picnic_foto_poner();
        ocultar(s_picnic, false);
        return s_picnic;
    }
    return NULL;
}

int ui_test_panel(int k)
{
    if (!s_pet || k < 0 || k >= PRUEBA_PANELES) return 0;
    cerrar_panel();
    switch (k) {
    case 0: panel_comer(); break;
    case 1: panel_jugar(); break;
    case 2: panel_trucos(); break;
    case 3: case 4: case 5: s_mas_tab = k - 3; panel_mas(); break;
    case 6: s_rincon_tab = 0; panel_rincon(); break;
    case 7: panel_amigos(); break;
    case 8: panel_frase(0); break;
    case 9: case 10: case 11: case 12: s_diario_tab = k - 9; s_diario_pag = 0; panel_diario(); break;
    case 13: panel_mapa(); break;
    case 14: case 15: s_bolsa_pag = 0; s_bolsa_libro = k - 14; panel_bolsa(); break;
    case 16: panel_sonido(); break;
    case 17: panel_pantalla(); break;
    case 18: panel_hora_menu(); break;
    case 19: panel_alarmas(); break;
    case 20: { alarma_t n = {1, 7, 0, ALARMA_LUN_VIE, 0, 0, 1, 0}; int q = 0; while (q < PET_ALARMAS - 1 && s_al_vista[q].usada) q++; n.hora = (uint8_t)(7 + q * 5); if (q == 1) n.dias = ALARMA_TODOS; s_al_ed = q; s_al_edit = n; panel_alarma(); break; }
    case 21: panel_dormir(); break;
    case 22: panel_temporizador(); break;
    case 23: panel_mascotas(); break;
    case 24: panel_respaldo(); break;
    case 25: panel_mas_ajustes(); break;
    case 26: panel_modo(); break;
    case 27: panel_rapido(); break;
    case 28: panel_color(); break;
    case 29: panel_aspecto(); break;
    case 30: panel_nombre(); break;
    case 31: panel_fecha(); break;
    case 32: panel_hora(); break;
    case 33: panel_familia(); break;
    case 34: panel_bote(); break;
    case 35: if (!pet_letter_count(s_pet)) return 0; panel_carta_llega(); break;
    case 36: if (!pet_letter_count(s_pet)) return 0; panel_cartas(); break;
    case 37: panel_resumen(); break;
    case 38: panel_semilla(0); break;
    case 39: panel_pide_viaje(); break;
    case 40: case 41: s_rincon_tab = k - 39; panel_rincon(); break;
    case 43: panel_genero(); break;
    case 44: if (!pet_letter_count(s_pet)) return 0; carta_leer(0, false); break;
    case 45: panel_historia(CRI_LUMBRE, 0); break;
    case 46: panel_pais(true); break;
    case 47: panel_ritmo(); break;
    case 48: panel_pantalla(); break;
    case 49: entrar_pecera(false); break;
    case 50: case 51: case 52: case 53: case 54: ui_demo_ceremonia(2, k - 50); break; /* (v39) el último verso */
    case 66: ui_demo_ceremonia(2, 5); break;
    case 67: case 68: ui_demo_ceremonia(2, k - 61); break;
    case 69: panel_historia(NARR_PRISMA, 6); break;
    case 70: panel_historia(NARR_TILDE, 0); break;
    case 71: panel_historia(NARR_BRIZNA, 4); break;
    case 72: panel_pagina(7); break;
    case 73: panel_historia(NARR_TILO, 0); break;
    case 74: panel_historia(NARR_ANIMALES, 23); break;
    case 75: ui_demo_ceremonia(1, 0); break; /* (v39) el rito, en su pantalla */
    case 76: ui_demo_ceremonia(1, 2); break;
    case 81: s_lib_narr = -1; panel_libro_indice(); break;
    case 82: ui_demo_ceremonia(2, 8); break;
    case 83: ui_demo_ceremonia(1, 1); break;
    case 78: panel_crismenu(); break;
    case 79: panel_canciones(); break;
    case 80: { /* el cuaderno lleno: cinco canciones de prueba (en una copia: no toca la partida) */
        static pet_t copia;
        const pet_t *orig = s_pet;
        copia = *s_pet;
        static const uint8_t N[12] = {PET_CANCION_NOTA(0, 2), PET_CANCION_NOTA(1, 2), PET_CANCION_NOTA(2, 4), PET_CANCION_NOTA(1, 1), PET_CANCION_NOTA(3, 2), PET_CANCION_NOTA(5, 2),
                                      PET_CANCION_NOTA(4, 2), PET_CANCION_NOTA(2, 2), PET_CANCION_NOTA(1, 4), PET_CANCION_NOTA(0, 2), PET_CANCION_NOTA(4, 1), PET_CANCION_NOTA(0, 8)};
        static const char *const NOM[5] = {"NUESTRA CANCIÓN", "LA DE LA LAGUNA", "LUNA DE CRISTAL", "PASITOS", "BUENAS NOCHES"};
        for (int i = 0; i < PET_CANCIONES; i++) { copia.canciones[i].n = (uint8_t)(12 - i); memcpy(copia.canciones[i].nota, N, 12); snprintf(copia.canciones[i].nombre, sizeof copia.canciones[i].nombre, "%s", NOM[i]); }
        copia.cancion_alarma = 1; copia.cancion_cuna = 3;
        s_pet = &copia;
        panel_canciones();
        s_pet = orig;
        break;
    }
    case 77: panel_lectura(ICO_TILDE, "Una nota de Tilde", "Te devuelvo algo", "Te devuelvo tu lágrima de laguna. Lo pedí prestado. Bueno, me lo llevé sin pedir. Pero lo cuidé muchísimo: lo pulí con el ala. — Tilde"); break;
    case 56: if (s_pet) for (int i = 0; i < PET_COMIDAS_EST; i++) pet_seasonal_food_give((pet_t *)s_pet, i, 1 + i % 3); panel_temporada(); break;
    case 58: case 59: case 60: case 61: case 62: case 63: case 64: case 65: { /* los 8 colores de una especie, lado a lado */
        abrir_tarjeta(PANEL_RESUMEN, 340, 250);
        for (int c = 0; c < 8; c++) {
            lv_obj_t *m = mini_criatura(s_tarjeta, c, (k - 58) | (c << 7), STAGE_ADULTA, 0, 3, EXP_ABIERTOS);
            lv_obj_align(m, LV_ALIGN_TOP_MID, (c % 4 - 2) * 80 + 40, 6 + (c / 4) * 100);
        }
        break;
    }
    case 57: s_mas_tab = 0; s_ella_pag = 0; panel_mas(); s_ella_pag = 1; panel_mas(); break;
    case 55: s_bolsa_libro = 1; s_bolsa_pag = 0; panel_bolsa(); break;
    case 84: panel_version(); break; /* (1.0.09) */
    case 42: { int id = -1; for (int i = 0; i < PET_OBJETOS && id < 0; i++) if (s_pet->bolsa[i]) id = i; if (id < 0) return 0; panel_objeto(id); break; }
    }
    if (s_panel == PANEL_NINGUNO) return 0;
    lv_obj_update_layout(lv_screen_active());
    return 1;
}
/* (1.0.10, pruebas) como si recién se hubiera elegido el huevo y el género en el primer inicio: el huevo va a llegar al prado */
void ui_test_huevo_elegido(int especie) { bienvenida_fin(); s_asp_e = especie; s_asp_listo = true; s_asp_gen = 0; s_inicio_cfg = 1; s_libro_inicio = false; cerrar_panel(); }
const char *ui_test_panel_name(int k) { return k >= 0 && k < PRUEBA_PANELES ? PRUEBA_NOMBRE[k] : "?"; }

static bool area_cruza(const lv_area_t *a, const lv_area_t *b) { return a->x1 <= b->x2 && b->x1 <= a->x2 && a->y1 <= b->y2 && b->y1 <= a->y2; }
static int revisar_obj(lv_obj_t *o, lv_area_t clip, char *out, int n, int *k, int problemas)
{
    if (lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN)) return problemas;
    lv_area_t a;
    lv_obj_get_coords(o, &a);
    { const int ty = lv_obj_get_style_translate_y(o, 0); if (ty) { a.y1 -= ty; a.y2 -= ty; } } /* una animación de entrada: se revisa dónde termina */
    if (lv_obj_get_width(o) >= W && lv_obj_get_height(o) >= W && !lv_obj_check_type(o, &lv_image_class) && o != s_tarjeta) return problemas; /* el fondo redondo de pantalla completa */
    if (o == s_fondo_pleno || lv_obj_get_user_data(o) == (void *)(uintptr_t)0xAD0) return problemas;
    bool img = lv_obj_check_type(o, &lv_image_class), btn = lv_obj_check_type(o, &lv_button_class);
    bool caja_visible = !img && !btn && o != s_tarjeta && lv_obj_get_style_bg_opa(o, 0) > LV_OPA_20; /* una hoja, un cartel */
    if (img && lv_area_get_width(&a) >= W && lv_area_get_height(&a) >= W) return problemas; /* un fondo de pantalla completa: es el vidrio mismo */
    if (img && lv_obj_get_parent(o) && lv_obj_check_type(lv_obj_get_parent(o), &lv_button_class) &&
        lv_obj_get_style_bg_opa(lv_obj_get_parent(o), 0) <= LV_OPA_20) { /* el dibujo de un huevo del vivero: sus esquinas son transparentes y caben en el vidrio (r 226) */
        float dx = (a.x1 + a.x2) / 2.0f - CX, dy = (a.y1 + a.y2) / 2.0f - CX;
        if (hypotf(dx, dy) + (a.x2 - a.x1 + 1) * 0.35f < VID_R) return problemas;
    }
    if (btn && lv_obj_get_style_bg_opa(o, 0) <= LV_OPA_20 && lv_obj_get_style_border_width(o, 0) == 0) btn = false; /* un área de toque invisible: solo importa lo que lleva dentro */
    if (img || btn || caja_visible) {
        lv_area_t v = a; /* lo que se ve: recortado por las áreas que se deslizan */
        if (v.x1 < clip.x1) v.x1 = clip.x1;
        if (v.y1 < clip.y1) v.y1 = clip.y1;
        if (v.x2 > clip.x2) v.x2 = clip.x2;
        if (v.y2 > clip.y2) v.y2 = clip.y2;
        if (v.x1 <= v.x2 && v.y1 <= v.y2) {
            int w = v.x2 - v.x1 + 1, h = v.y2 - v.y1 + 1;
            int rad = (btn || caja_visible) ? lv_obj_get_style_radius(o, 0) : 0;
            if (rad > w / 2) rad = w / 2;
            if (rad > h / 2) rad = h / 2;
            float in = (float)rad * 0.29f;
            float px[8] = {v.x1 + in, v.x2 - in, v.x1 + in, v.x2 - in, (v.x1 + v.x2) / 2.0f, (v.x1 + v.x2) / 2.0f, (float)v.x1, (float)v.x2};
            float py[8] = {v.y1 + in, v.y1 + in, v.y2 - in, v.y2 - in, (float)v.y1, (float)v.y2, (v.y1 + v.y2) / 2.0f, (v.y1 + v.y2) / 2.0f};
            float peor = 0;
            for (int i = 0; i < 8; i++) {
                float d = hypotf(px[i] - CX, py[i] - CX) - (VID_R - MARGEN_PRUEBA);
                if (d > peor) peor = d;
            }
            const char *que = NULL;
            if (peor > 0) que = "fuera del círculo";
            lv_area_t xa, va;
            if (!que && s_cerrar_x && !lv_obj_has_flag(s_cerrar_x, LV_OBJ_FLAG_HIDDEN)) {
                lv_obj_get_coords(s_cerrar_x, &xa);
                if (area_cruza(&v, &xa)) que = "tapa la X";
            }
            if (!que && s_volver && !lv_obj_has_flag(s_volver, LV_OBJ_FLAG_HIDDEN)) {
                lv_obj_get_coords(s_volver, &va);
                if (area_cruza(&v, &va)) que = "tapa la flecha de volver";
            }
            if (que) {
                problemas++;
                if (*k < n) *k += snprintf(out + *k, (size_t)(n - *k), "  %s %s en (%d,%d) %dx%d%s\n", btn ? "botón" : (img ? "imagen" : "caja"), que,
                                           (int)v.x1, (int)v.y1, w, h, peor > 0 ? " (se sale)" : "");
            }
        }
        if (img) return problemas;
    }
    if (lv_obj_has_flag(o, LV_OBJ_FLAG_SCROLLABLE) && o != s_tarjeta && !btn) { /* sus hijos se ven solo adentro */
        if (a.x1 > clip.x1) clip.x1 = a.x1;
        if (a.y1 > clip.y1) clip.y1 = a.y1;
        if (a.x2 < clip.x2) clip.x2 = a.x2;
        if (a.y2 < clip.y2) clip.y2 = a.y2;
    }
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) problemas = revisar_obj(lv_obj_get_child(o, (int32_t)i), clip, out, n, k, problemas);
    return problemas;
}
int ui_check_panel(char *out, int n)
{
    int k = 0;
    if (out && n) out[0] = 0;
    if (s_panel == PANEL_NINGUNO) return 0;
    lv_area_t clip = {0, 0, W - 1, W - 1};
    return revisar_obj(s_tarjeta, clip, out, n, &k, 0);
}

#include "ui_magia.inc"
#include "ui_pochi.inc"
#include "ui_ny.inc"
#include "ui_final.inc"
#include "ui_objx.inc"
#include "ui_pesca.inc"
#include "ui_historia.inc" /* (v37) Tilde, la Abuela Brizna, las páginas de Tilo, los animales y los lugares secretos */
#include "ui_musica.inc"   /* (v38) el modo libre de los cristales, nuestra canción y el reproductor */
#include "ui_ceremonias.inc" /* (v39) el rito de la Guardiana y el último verso, cada uno en su pantalla */
#include "ui_zonas.inc"      /* (v39) el claro de las luciérnagas (con la casita de Tilo) y el mirador de las nubes */

/* (pruebas) la deja quieta en un punto del mundo, para revisar dónde salen los globos */
static lv_obj_t *s_punto_mira;
void ui_test_punto(int x, int y) /* (pruebas) un anillo rojo con punto en (x,y) de la pantalla; x < 0 lo quita. Deja pasar los toques */
{
    if (x < 0) { if (s_punto_mira) lv_obj_add_flag(s_punto_mira, LV_OBJ_FLAG_HIDDEN); return; }
    if (!s_punto_mira) {
        s_punto_mira = lv_obj_create(lv_layer_top());
        lv_obj_remove_style_all(s_punto_mira);
        lv_obj_set_size(s_punto_mira, 64, 64);
        lv_obj_set_style_radius(s_punto_mira, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(s_punto_mira, 6, 0);
        lv_obj_set_style_border_color(s_punto_mira, lv_color_hex(0xFF2030), 0);
        lv_obj_set_style_bg_color(s_punto_mira, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_bg_opa(s_punto_mira, LV_OPA_60, 0);
        lv_obj_remove_flag(s_punto_mira, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_remove_flag(s_punto_mira, LV_OBJ_FLAG_SCROLLABLE);
    }
    lv_obj_set_pos(s_punto_mira, x - 32, y - 32);
    lv_obj_remove_flag(s_punto_mira, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_punto_mira);
}
/* (pruebas) pantalla fija del modo "prueba de descarga": fondo negro, título, tres líneas de estado y un círculo girando (para que la pantalla siga animada) */
static lv_obj_t *s_desc_fondo, *s_desc_lbl[4], *s_desc_giro;
void ui_test_descarga(const char *l0, const char *l1, const char *l2, const char *l3, bool animar)
{
    if (!l0) { if (s_desc_fondo) lv_obj_add_flag(s_desc_fondo, LV_OBJ_FLAG_HIDDEN); return; }
    if (!s_desc_fondo) {
        s_desc_fondo = lv_obj_create(lv_layer_top());
        lv_obj_remove_style_all(s_desc_fondo);
        lv_obj_set_size(s_desc_fondo, LV_PCT(100), LV_PCT(100));
        lv_obj_set_style_bg_color(s_desc_fondo, lv_color_hex(0x000000), 0);
        lv_obj_set_style_bg_opa(s_desc_fondo, LV_OPA_COVER, 0);
        lv_obj_add_flag(s_desc_fondo, LV_OBJ_FLAG_CLICKABLE); /* se come los toques: la mascota no se usa */
        for (int i = 0; i < 4; i++) {
            s_desc_lbl[i] = lv_label_create(s_desc_fondo);
            lv_obj_set_style_text_color(s_desc_lbl[i], lv_color_hex(i == 0 ? 0xFFD060 : 0xE8E8E8), 0);
            lv_obj_set_style_text_align(s_desc_lbl[i], LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_set_width(s_desc_lbl[i], 360);
            lv_obj_align(s_desc_lbl[i], LV_ALIGN_TOP_MID, 0, 70 + i * 52);
        }
        s_desc_giro = lv_spinner_create(s_desc_fondo);
        lv_obj_set_size(s_desc_giro, 70, 70);
        lv_obj_align(s_desc_giro, LV_ALIGN_BOTTOM_MID, 0, -50);
    }
    const char *t[4] = {l0, l1, l2, l3};
    for (int i = 0; i < 4; i++) lv_label_set_text(s_desc_lbl[i], t[i] ? t[i] : "");
    if (animar) lv_obj_remove_flag(s_desc_giro, LV_OBJ_FLAG_HIDDEN); else lv_obj_add_flag(s_desc_giro, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(s_desc_fondo, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_desc_fondo);
}
void ui_test_alarma(int modo) { if (modo == 0 || modo == 1 || modo == 3) alarma_mostrar(modo, 0, s_ultimo_t); } /* (pruebas) */
void ui_test_alarma_cerrar(void) { alarma_cerrar(); }
void ui_test_soltar_memoria(int modo)
{
#ifdef ESP_PLATFORM
    lv_image_cache_drop(NULL);
    if (modo >= 4) iconos_soltar(); /* (v97) 4 = además los íconos guardados que no están en pantalla (lo mismo que hace la vigilancia de memoria) */
#else
    if (modo >= 4) iconos_soltar();
    /* (liberar pantallas o fondos aquí podía soltar una imagen que está en pantalla: solo se suelta la caché de imágenes) */
#endif
}
int ui_test_cruce_fondo(void) { return s_cru_activo ? (s_cru_fondo_art.px ? 1 : 0) : -1; } /* (pruebas) -1 no hay cruce; 0 sin fondo; 1 con fondo */
int ui_test_fondo(int *esc, int *per, int *w, int *h) /* (pruebas) ¿está cargado el fondo? zona y período que dibuja, y su tamaño */
{
    *esc = s_fondo_escena; *per = s_fondo_periodo; *w = *h = 0;
    const void *src = s_fondo ? lv_image_get_src(s_fondo) : NULL;
    if (!src || lv_image_src_get_type(src) != LV_IMAGE_SRC_VARIABLE) return 0;
    *w = ((const lv_image_dsc_t *)src)->header.w; *h = ((const lv_image_dsc_t *)src)->header.h;
    return *w > 0;
}
void ui_test_bote(int q) /* (pruebas) 2 abre el menú del bote, 0 pescar, 1 ir a la isla (lo que hacen sus botones) */
{
    if (q == 2) { panel_bote(); return; }
    cerrar_panel();
    if (q == 0) accion(ACT_PESCAR, 0); else bote_ir(ESC_ISLA);
}
void ui_test_pos(int x, int y)
{
    s_mascota.x = s_mascota.tx = (float)x;
    s_mascota.y = s_mascota.ty = (float)y;
    s_mascota.modo = M_QUIETA;
    s_mascota.modo_hasta = s_ms + 60000;
}

void ui_libro_pedir(int n) { libro_pedir(n); }
/* el botón PWR apretado dos veces: modo pecera */
void ui_pecera(void) { if (s_pecera) return; cerrar_panel(); entrar_pecera(false); }
void ui_cine(int n) { s_cine_pend = n + 1; s_cine_listo = 0; }
void ui_demo_rito_historia(void) { cerrar_panel(); ui_demo_ceremonia(1, 2); } /* (el simulador: la historia con que termina el rito) */
void ui_demo_historia(int c, int k) { panel_historia(c, k); } /* (el simulador: ver una hoja de historia) */
void ui_demo_cofre(int v, int s) { s_cofre_vista = v; s_cofre_sub = s; panel_cofre(); }
void ui_demo_rapido(void) { panel_rapido(); }

/* (v99, batería) nada que leer ni jugar en pantalla: ni panel, ni juego, ni globo, ni Pochi, ni escena especial.
 * Con eso main.c atenúa la pantalla a los pocos segundos sin tocarla y baja los cuadros por segundo. */
bool ui_reposo_visual(void)
{
    return s_pet && s_panel == PANEL_NINGUNO && s_juego == JG_NADA && ui_escena_especial() == 0 && !po_activo() && s_zj_jg == ZJ_NADA &&
           !s_cie_activo && !s_via_activo && !s_cru_activo && !s_bienv_activa && !s_cine_pend && !s_pecera && !ui_alarm_ringing();
}

/* (pruebas, v89) nombre de un objeto grande de la interfaz, para encontrar quién se repinta entero */
const char *ui_dbg_obj_nombre(const void *o)
{
    static char b[64];
    const struct { const void *p; const char *n; } T[] = {
        {s_mundo, "mundo"}, {s_fondo, "fondo"}, {s_capa, "capa paneles"}, {s_flash, "destello"}, {s_cie, "cielo"}, {s_casa_pen, "penumbra casa"},
        {s_hg_pant, "hongos"}, {s_juego_capa, "capa juegos"}, {s_cru, "cruce"}, {s_via, "viaje"}, {s_bienv, "bienvenida"}, {s_cuenta, "cuenta"},
        {s_al, "alarma"}, {s_picnic, "picnic"}, {s_cum, "cumple"}, {lv_screen_active(), "pantalla"}, {lv_layer_top(), "capa top"},
    };
    for (unsigned i = 0; i < sizeof T / sizeof T[0]; i++) if (T[i].p && T[i].p == o) return T[i].n;
    const lv_obj_t *pa = lv_obj_get_parent((const lv_obj_t *)o);
    const char *np = "?";
    for (unsigned i = 0; i < sizeof T / sizeof T[0]; i++) if (T[i].p && T[i].p == pa) np = T[i].n;
    lv_area_t a; lv_obj_get_coords((const lv_obj_t *)o, &a);
    snprintf(b, sizeof b, "hijo de %s (%d,%d %dx%d) idx %d", np, (int)a.x1, (int)a.y1, (int)lv_area_get_width(&a), (int)lv_area_get_height(&a), (int)lv_obj_get_index((const lv_obj_t *)o));
    return b;
}
