/*
 * Simulador web del reloj: el equivalente de firmware/main/main.c para el navegador.
 *
 * Usa el mismo motor (core/pet.c) y la misma interfaz (firmware/main/ui.c),
 * compilados a WebAssembly. La página JavaScript:
 *   - llama sim_frame() en cada cuadro y copia el framebuffer al canvas
 *   - pasa el mouse/dedo como pantalla táctil (sim_touch)
 *   - reproduce los sonidos que saca de sim_pop_sound()
 *   - guarda los bytes de la mascota (sim_pet_ptr) en el navegador
 *   - amigos: intercambia balizas con otras pestañas (BroadcastChannel) en lugar
 *     de Bluetooth, y trae un "amigo de prueba" que vive dentro del simulador
 *
 * El reloj virtual corre a "speed" veces la velocidad real.
 */
#include <stdio.h>
#include <string.h>

#include "lvgl.h"
#include "pet.h"
#include "sfx.h"
#include "ui.h"
#include "foto.h"
#include "art.h"
#include <stdlib.h>

#define EXPORT(name) __attribute__((export_name(#name)))
#define W 466
#define ZONA_HORARIA_S (-3 * 3600)
static pet_device_t s_dispo;  /* volumen, brillo y cuándo se apaga (del dispositivo) */
static int s_bolsillo = 1;   /* con la pantalla apagada el táctil no cuenta (como en el bolsillo) */

static uint32_t s_fb[W * W];
static lv_display_t *s_disp;
static pet_t s_pet;
static double s_reloj;       /* hora virtual Unix */
static pet_t s_respaldo;     /* un respaldo esperando confirmación en la pantalla */
static int s_hay_respaldo;
static uint32_t s_ms;        /* tiempo real para LVGL */
static double s_speed = 1.0;
static int s_pantalla_on = 1;
static int s_cargando_sim;
static int s_siempre_on = 1; /* en el simulador la pantalla queda encendida salvo que se pida lo contrario */
static uint32_t s_ultimo_update;

/* cola de acciones (la interfaz las encola, el bucle las aplica) */
typedef struct { uint8_t a; int32_t arg; } accion_t;
static accion_t s_acc[16];
static int s_acc_ini, s_acc_fin;

/* cola de sonidos para JavaScript */
static uint8_t s_snd[32];
static int s_snd_ini, s_snd_fin;
static void encolar_sonido(int snd)
{
    int next = (s_snd_fin + 1) % 32;
    if (next != s_snd_ini) { s_snd[s_snd_fin] = (uint8_t)snd; s_snd_fin = next; }
}

/* registro de eventos en texto para el panel lateral */
static char s_log[8192];
static int s_log_len;

static int s_tx = -1, s_ty = -1, s_tp;

/* amigos */
static pet_t s_bot;
static int s_bot_on;
static uint32_t s_bot_responde; /* hora virtual en que el amigo de prueba acepta */
static uint8_t s_baliza_out[PET_BALIZA_BYTES], s_baliza_in[PET_BALIZA_BYTES];

static uint32_t tick_cb(void) { return s_ms; }

static uint64_t s_flush_px;
static void flush_cb(lv_display_t *d, const lv_area_t *a, uint8_t *px)
{
    (void)px;
    s_flush_px += (uint64_t)(a->x2 - a->x1 + 1) * (uint64_t)(a->y2 - a->y1 + 1);
    lv_display_flush_ready(d);
}

static void touch_cb(lv_indev_t *i, lv_indev_data_t *d)
{
    (void)i;
    d->state = s_tp ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
    if (s_tx >= 0) { d->point.x = s_tx; d->point.y = s_ty; }
}

static void on_action(int a, int arg)
{
    int next = (s_acc_fin + 1) % 16;
    if (next == s_acc_ini) return;
    s_acc[s_acc_fin].a = (uint8_t)a;
    s_acc[s_acc_fin].arg = (int32_t)arg;
    s_acc_fin = next;
}

static void log_linea(uint32_t t, const char *fmt, const char *a, const char *b)
{
    char linea[160], txt[128];
    int64_t l = (int64_t)t + ZONA_HORARIA_S;
    int s = (int)(((l % 86400) + 86400) % 86400);
    snprintf(txt, sizeof txt, fmt, a ? a : "", b ? b : "");
    int n = snprintf(linea, sizeof linea, "%02d:%02d|%s\n", s / 3600, s / 60 % 60, txt);
    if (n <= 0) return;
    if (s_log_len + n >= (int)sizeof s_log) s_log_len = 0; /* JS lo vacía a menudo */
    memcpy(s_log + s_log_len, linea, (size_t)n);
    s_log_len += n;
}

static const char *const FSUC_TXT[FSUC_COUNT] = {"llego", "se fue", "la persigue", "la pico", "ya no la persigue (aprendio)", "se robo una fruta", "lo espantaste", "lo echo ella misma", "le trajo una semilla"};
static const char *const JAR_TXT[] = {"plantaste una semilla", "crecio", "florecio", "tiene sed", "revivio al regarla", "dio una fruta", "enterro una semilla: saldra un arbol",
                                      "se seco (regandola revive)", "se perdio", "la sacaste", "esta viva: no se saca", "cumplio su tiempo: se marchito sola"};

static void describir(const pet_event_t *e)
{
    char n[16];
    switch (e->type) {
    case EV_EMOCION: log_linea(e->t, "ahora esta %s", pet_mood_name((mood_t)e->a), 0); break;
    case EV_PIDE: log_linea(e->t, "pide: %s", pet_want_name((want_t)e->a), 0); break;
    case EV_SE_DURMIO: log_linea(e->t, "se durmio", 0, 0); break;
    case EV_DESPERTO: log_linea(e->t, e->a ? "la despertaste" : "desperto", 0, 0); break;
    case EV_ENFERMO: log_linea(e->t, "se enfermo", 0, 0); break;
    case EV_SANO: log_linea(e->t, "se curo", 0, 0); break;
    case EV_NIEGA: log_linea(e->t, "terca: no quiso (%s)", pet_action_name((action_t)e->a), 0); break; /* (fw 104) */
    case EV_CARTA_BOSQUE: { char n[8]; snprintf(n, sizeof n, "%d", e->a); log_linea(e->t, "papelito del bosque %s%s", n, e->b ? " (te lo trajo)" : ""); break; }
    case EV_PASEO_JUNTOS: { char n[8]; snprintf(n, sizeof n, "%d", e->a); log_linea(e->t, "paseo de verdad: %s00 pasos", n, 0); break; }
    case EV_HIELO: log_linea(e->t, "se derritio el trocito de hielo", 0, 0); break;
    case EV_HEDIONDA: log_linea(e->t, e->b ? "se le quito el olor" : "quedo hedionda sola", 0, 0); break;
    case EV_REACCION:
        log_linea(e->t, "%s: %s", pet_action_name((action_t)e->a),
                  e->b > 0 ? "le gusto" : (e->b < 0 ? "no le gusto" : "indiferente"));
        break;
    case EV_VISITA_LLEGA:
        log_linea(e->t, "llega de visita un %s", pet_species_name(s_pet.visitantes[e->a].especie), 0);
        break;
    case EV_ENCUENTRO:
        log_linea(e->t, "con el %s: %s", pet_species_name(s_pet.visitantes[e->a].especie),
                  pet_encounter_name((encounter_t)e->b));
        break;
    case EV_VISITA_SE_VA: log_linea(e->t, "la visita se fue", 0, 0); break;
    case EV_CRECIO:
        log_linea(e->t, e->a == STAGE_ADULTA ? "evoluciono: %s (%s)" : "crecio: ahora es %s", pet_stage_name((stage_t)e->a),
                  pet_form_name((forma_t)e->b));
        break;
    case EV_EVO_LISTA: log_linea(e->t, "ya puede crecer: espera tu decision (etapa %s)", pet_stage_name((stage_t)e->a), 0); break;
    case EV_REGALO: log_linea(e->t, "cayo una fruta del arbol", 0, 0); break;
    case EV_CARTA: log_linea(e->t, "le llego una carta (cerrada, con su sello)", 0, 0); break;
    case EV_ANIMAL: {
        static const char *const Q[] = {"llego", "se fue", "se hizo bolita", "ahora son amigos", "se llevo una semilla", "devolvio la semilla",
                                        "juegan a perseguirse", "se asusto con la tormenta", "dejo un rastro brillante", "la consolo",
                                        "la sigue en fila", "ulula", "la arrulla", "se escondio"};
        log_linea(e->t, "%s: %s", pet_animal_name((animal_t)e->a), (unsigned)e->b < sizeof Q / sizeof Q[0] ? Q[e->b] : "?");
        break;
    }
    case EV_ANIMAL_NUEVO: { char al[48]; log_linea(e->t, "conocio contigo %s", pet_card_name_a((uint8_t)e->a, al, sizeof al), 0); break; }
    case EV_CRIATURA: {
        static const char *const T[] = {"se asomo", "los espera para jugar", "jugaron juntos", "se fue", "le conto una historia", "saludo"};
        log_linea(e->t, "%s: %s", pet_creature_name((magica_t)((uint8_t)e->a % CRI_COUNT)), T[(e->b & 15) % 6]);
        break;
    }
    case EV_ESPECIAL: log_linea(e->t, "el secreto de la zona: %s", e->b == ESS_DESCUBRE ? "lo descubrio" : "otra vez", 0); break;
    case EV_PISTA: log_linea(e->t, "pista: %s", pet_hint_text((especial_t)e->a, e->b), 0); break;
    case EV_LUNA_LLENA: log_linea(e->t, e->a ? "aviso: esta noche hay luna llena (y algo en la isla te espera)" : "aviso: esta noche hay luna llena", 0, 0); break;
    case EV_SIN_ITEM: log_linea(e->t, "no queda %s", pet_item_name((item_t)e->a), 0); break;
    case EV_DECORO: log_linea(e->t, "pusiste %s en el prado", pet_decor_name((decor_t)e->a), 0); break;
    case EV_PENSAMIENTO: log_linea(e->t, "piensa: %s por %s", pet_emotion_name((emo_t)e->a), pet_cause_name((causa_t)e->b)); break;
    case EV_ACTIVIDAD: log_linea(e->t, "%s en %s", pet_activity_name((actividad_t)e->a), pet_place_name((lugar_t)e->b)); break;
    case EV_COMIDA_EST: log_linea(e->t, e->b ? "encontro %s y te lo trajo" : "encontro %s", pet_seasonal_food_info(e->a)->nombre, 0); break;
    case EV_HALLAZGO: log_linea(e->t, e->b ? "encontro %s y te lo trajo" : "encontro %s", pet_find_name((hallazgo_t)e->a), 0); break;
    case EV_REENCUENTRO: snprintf(n, sizeof n, "%d", e->a); log_linea(e->t, "volviste despues de %s h: te recibe feliz", n, 0); break;
    case EV_TRUENO: log_linea(e->t, "trueno", 0, 0); break;
    case EV_DESCUBRE: {
        static const char *const T[] = {"comida", "clima", "lugar", "cómo tocarla:"};
        const char *q = "?";
        if (e->a == GUSTO_COMIDA) q = pet_food_name((food_t)e->b);
        if (e->a == GUSTO_CLIMA) q = pet_weather_name((clima_t)e->b);
        if (e->a == GUSTO_LUGAR) q = pet_place_name((lugar_t)e->b);
        if (e->a == GUSTO_GESTO) q = pet_gesture_name((gesto_t)e->b);
        log_linea(e->t, "descubriste un gusto: %s %s", T[e->a % 4], q);
        break;
    }
    case EV_RASGO: log_linea(e->t, e->b > 0 ? "se esta volviendo mas %s" : "se esta volviendo menos %s", pet_trait_name((trait_t)e->a), 0); break;
    case EV_NACIO: log_linea(e->t, "nacio del huevo: %s", s_pet.nombre[0] ? s_pet.nombre : "(sin nombre)", 0); break;
    case EV_CLIMA: log_linea(e->t, "clima: %s", pet_weather_name((clima_t)e->a), 0); break;
    case EV_FRASE: log_linea(e->t, "mensaje del dia: %s", pet_phrase(e->a), 0); break;
    case EV_AMIGO_CERCA: log_linea(e->t, "amigo cerca: %s", s_pet.amigos[e->a % PET_MAX_AMIGOS].nombre, 0); break;
    case EV_INVITACION: log_linea(e->t, "%s te invita a jugar", s_pet.amigos[e->a % PET_MAX_AMIGOS].nombre, 0); break;
    case EV_AMIGO_VISITA:
        log_linea(e->t, "con %s: %s", s_pet.amigos[e->a % PET_MAX_AMIGOS].nombre, pet_friendship_name((amistad_t)e->b));
        break;
    case EV_ESCENA: log_linea(e->t, e->b ? "se fue sola a %s" : "la llevaste a %s", pet_scene_name((escena_t)e->a), 0); break;
    case EV_ESCENA_NUEVA: log_linea(e->t, "se abrio una zona nueva: %s", pet_scene_name((escena_t)e->a), 0); break;
    case EV_FAUNA: log_linea(e->t, "%s: %s", pet_fauna_name((fauna_t)e->a), FSUC_TXT[e->b % FSUC_COUNT]); break;
    case EV_JARDIN: snprintf(n, sizeof n, "%d", e->a + 1); log_linea(e->t, "jardin (parcela %s): %s", n, JAR_TXT[e->b % 12]); break;
    case EV_SALPICA: log_linea(e->t, e->b > 0 ? "salto en un charco: le encanto" : "piso un charco: se ensucio", 0, 0); break;
    case EV_HOJAS: log_linea(e->t, "salto en el monton de hojas", 0, 0); break;
    case EV_VECINO:
        if (e->a == VEC_REGALO) log_linea(e->t, "su amigo le regalo %s", pet_item_name((item_t)e->b), 0);
        else log_linea(e->t, "%s: %s", s_pet.vecino.nombre, pet_neighbor_activity_name((vecino_act_t)e->a));
        break;
    case EV_PESCA:
        log_linea(e->t, "pesco %s%s", pet_catch_name((pesca_t)e->a),
                  e->a == PESCA_PEZ ? (e->b ? " y se lo comio" : " y lo devolvio al agua") : "");
        break;
    case EV_VINCULO: log_linea(e->t, "vinculo: ahora %s", pet_bond_name((vinculo_nivel_t)e->a), 0); break;
    case EV_HABILIDAD: {
        static const char *const T[] = {"se cayo intentando %s", "practico %s", "primer intento de %s", "aprendio a %s!", "es experta en %s"};
        log_linea(e->t, T[(e->b + 1) % 5], pet_skill_name((habilidad_t)e->a), 0);
        break;
    }
    case EV_TESORO:
        if (e->a == 0) log_linea(e->t, "huele un tesoro escondido...", 0, 0);
        else if (e->b < 0) log_linea(e->t, "desenterro algo repetido: una piedra brillante", 0, 0);
        else log_linea(e->t, "tesoro: %s", pet_piece_name((coleccion_t)(e->b / 8), e->b % 8), 0);
        break;
    case EV_COLECCION: log_linea(e->t, "coleccion completa: %s!", pet_collection_name((coleccion_t)e->a), 0); break;
    case EV_RARO: log_linea(e->t, e->b ? "algo raro: %s" : "se fue %s", pet_rare_name((raro_t)e->a), 0); break;
    case EV_META:
        if (e->b == 0) log_linea(e->t, "meta de hoy: %s", pet_goal_name((meta_t)e->a), 0);
        else if (e->b == 1) log_linea(e->t, "meta cumplida: %s (una estampita)", pet_goal_name((meta_t)e->a), 0);
        break;
    case EV_SU_CUMPLE:
        snprintf(n, sizeof n, "%d", e->a);
        log_linea(e->t, e->a == 7 && !e->b ? "cumple una semana%s" : (e->b ? "cumple %s anos!" : "cumple %s meses!"), e->a == 7 && !e->b ? "!" : n, 0);
        break;
    case EV_FECHA: { const celebracion_t *c = pet_celebration_get(e->b); log_linea(e->t, "hoy es %s", c ? c->nombre : pet_date_name((fecha_t)e->a), 0); break; }
    case EV_FOTO: log_linea(e->t, "una foto para el album", 0, 0); break;
    case EV_ROPA: {
        static const char *const R[] = {"le da calor: se saco %s", "se saco %s", "no le gusta mucho %s", "le da igual %s",
                                        "le gusta %s", "le encanta %s!", "", "le quitaste %s"};
        int k = e->b + 3;
        if (k >= 0 && k < 8 && R[k][0]) log_linea(e->t, R[k], pet_clothes_name((ropa_t)e->a), 0);
        break;
    }
    case EV_JUGUETE: {
        static const char *const J[] = {"no quiso jugar con %s", "juega con %s", "estreno %s!", "acepto jugar con %s", "pusiste %s en el prado", "guardaste %s"};
        int k = e->b + 1;
        if (k >= 0 && k < 6) log_linea(e->t, J[k], pet_toy_name((juguete_t)e->a), 0);
        break;
    }
    case EV_NUEVO: log_linea(e->t, "algo nuevo en el baul: %s", e->a ? pet_toy_name((juguete_t)e->b) : pet_clothes_name((ropa_t)e->b), 0); break;
    case EV_TRUCO:
        log_linea(e->t, e->b == 2 ? "aprendio a %s!" : "hizo sola el truco: %s", pet_trick_name((truco_t)e->a), 0);
        break;
    case EV_CUMPLE: snprintf(n, sizeof n, "%d", e->a); log_linea(e->t, "feliz cumpleanos! cumples %s", n, 0); break;
    default: break;
    }
}

/* ---------------- amigo de prueba ---------------- */
static void bot_crear(uint32_t t, uint32_t seed)
{
    static const char *const NOMBRES[] = {"Kiri", "Tito", "Lulu", "Mochi", "Pipo", "Nina"};
    pet_init(&s_bot, NOMBRES[seed % 6], (uint8_t)((seed / 7) % pet_species_count()), t - 6u * 86400u, seed);
    pet_hatch(&s_bot, t - 6u * 86400u);
    s_bot.etapa = STAGE_ADULTA;
    s_bot.forma = FORMA_RADIANTE;
    s_bot.ultima_actualizacion = t;
    pet_set_timezone(&s_bot, ZONA_HORARIA_S);
}

static void bot_paso(uint32_t t)
{
    if (!s_bot_on) return;
    for (int i = 0; i < NEED_COUNT; i++) s_bot.necesidad[i] = 90; /* siempre bien cuidado */
    pet_update(&s_bot, t);
    pet_event_t e;
    while (pet_poll_event(&s_bot, &e))
        if (e.type == EV_INVITACION) s_bot_responde = t + 3; /* contesta "sí" a los 3 s */
    if (s_bot_responde && t >= s_bot_responde) {
        s_bot_responde = 0;
        pet_answer_invite(&s_bot, true, t);
    }
    /* intercambio de balizas, como si estuvieran a pocos metros */
    pet_baliza_t b;
    uint8_t buf[PET_BALIZA_BYTES];
    pet_make_baliza(&s_bot, t, &b);
    pet_baliza_encode(&b, buf);
    if (pet_baliza_decode(buf, sizeof buf, &b)) pet_on_baliza(&s_pet, &b, t);
    pet_make_baliza(&s_pet, t, &b);
    pet_baliza_encode(&b, buf);
    if (pet_baliza_decode(buf, sizeof buf, &b)) pet_on_baliza(&s_bot, &b, t);
}

static void procesar_eventos(void)
{
    pet_event_t e;
    int despertar = 0;
    while (pet_poll_event(&s_pet, &e)) {
        if (e.type == EV_SONIDO) {
            if (!pet_is_bedtime(&s_pet, (uint32_t)s_reloj)) encolar_sonido(e.a); /* a la hora de dormir, silencio */
            continue;
        }
        describir(&e);
        ui_show_event(&s_pet, &e);
        if (e.type == EV_PIDE || e.type == EV_ENFERMO || e.type == EV_VISITA_LLEGA || e.type == EV_FRASE ||
            e.type == EV_INVITACION || e.type == EV_AMIGO_VISITA || e.type == EV_NACIO || e.type == EV_HALLAZGO || e.type == EV_COMIDA_EST ||
            e.type == EV_ESCENA_NUEVA || (e.type == EV_FAUNA && e.a == FAUNA_ZORRO && e.b == FSUC_LLEGA) || e.type == EV_RARO ||
            e.type == EV_SU_CUMPLE || e.type == EV_FECHA || e.type == EV_COLECCION)
            despertar = 1;
    }
    if (despertar) lv_display_trigger_activity(s_disp);
}

/* ---------------- API para JavaScript ---------------- */

/* Fotos: en el simulador quedan en la memoria (hasta 12) y la página las baja como PNG */
#define FOTOS_SIM 12
static uint8_t *s_foto[FOTOS_SIM];
static int s_foto_len[FOTOS_SIM], s_foto_id[FOTOS_SIM], s_foto_n, s_foto_prox = 1;
static int foto_idx(int id) { for (int i = 0; i < s_foto_n; i++) if (s_foto_id[i] == id) return i; return -1; }
static int f_guardar(const uint8_t *q, int n)
{
    if (s_foto_n >= FOTOS_SIM) return -1;
    uint8_t *c = malloc((size_t)n);
    if (!c) return -1;
    memcpy(c, q, (size_t)n);
    s_foto[s_foto_n] = c; s_foto_len[s_foto_n] = n; s_foto_id[s_foto_n] = s_foto_prox++;
    char kb[16];
    snprintf(kb, sizeof kb, "%d", n / 1024);
    log_linea((uint32_t)s_reloj, "le sacaste una foto (%s KB)", kb, 0);
    return s_foto_id[s_foto_n++];
}
static int f_listar(uint16_t *ids, int max)
{
    int k = 0;
    for (int i = s_foto_n - 1; i >= 0 && k < max; i--) ids[k++] = (uint16_t)s_foto_id[i];
    return k;
}
static int f_tamano(int id) { int i = foto_idx(id); return i < 0 ? -1 : s_foto_len[i]; }
static int f_leer(int id, uint8_t *b, int cap)
{
    int i = foto_idx(id);
    if (i < 0 || cap < s_foto_len[i]) return -1;
    memcpy(b, s_foto[i], (size_t)s_foto_len[i]);
    return s_foto_len[i];
}
static bool f_borrar(int id)
{
    int i = foto_idx(id);
    if (i < 0) return false;
    free(s_foto[i]);
    for (int k = i; k < s_foto_n - 1; k++) { s_foto[k] = s_foto[k + 1]; s_foto_len[k] = s_foto_len[k + 1]; s_foto_id[k] = s_foto_id[k + 1]; }
    s_foto_n--;
    return true;
}
static const ui_fotos_t FOTOS = {f_guardar, f_listar, f_leer, f_tamano, f_borrar};
EXPORT(sim_photo_count) int sim_photo_count(void) { return s_foto_n; }
EXPORT(sim_photo_id) int sim_photo_id(int i) { return i >= 0 && i < s_foto_n ? s_foto_id[i] : -1; }
EXPORT(sim_photo_ptr) uint8_t *sim_photo_ptr(int id) { int i = foto_idx(id); return i < 0 ? NULL : s_foto[i]; }
EXPORT(sim_photo_len) int sim_photo_len(int id) { return f_tamano(id); }

/* Sonido ambiente: el mismo bucle que suena en el reloj */
static int16_t s_amb[8 * 16000 + 9600]; /* SFX_AMB_SEGUNDOS + lo que se usa para empalmar */
EXPORT(sim_ambient) int sim_ambient(void)
{
    return s_pantalla_on && s_pet.edad_dueno ? (int)pet_ambient(&s_pet, (uint32_t)s_reloj) : (int)AMB_NADA;
}
EXPORT(sim_ambient_vol) int sim_ambient_vol(void) { return s_dispo.ambiente; }
EXPORT(sim_ambient_render) int sim_ambient_render(int a) { return sfx_ambient((ambiente_t)a, s_amb, (int)(sizeof s_amb / 2), 16000); }
EXPORT(sim_ambient_pcm) int16_t *sim_ambient_pcm(void) { return s_amb; }
EXPORT(sim_name_ambient) const char *sim_name_ambient(int a) { return pet_ambient_name((ambiente_t)a); }
/* cargando la batería (para ver el modo pecera solo) */
EXPORT(sim_charging) void sim_charging(int on) { s_cargando_sim = on != 0; ui_set_battery(on ? 80 : -1, on != 0); }
EXPORT(sim_aquarium) int sim_aquarium(void) { return ui_aquarium(); }
/* abrir todo el baúl (para probar la ropa y los juguetes) */
EXPORT(sim_unlock_all) void sim_unlock_all(void)
{
    s_pet.ropa_tiene = (1u << ROPA_COUNT) - 1;
    s_pet.juguetes_tiene = (1u << JUG_COUNT) - 1;
    log_linea((uint32_t)s_reloj, "abriste todo el baul: ropa y juguetes", 0, 0);
}
EXPORT(sim_look) int sim_look(void) { return pet_look(&s_pet); }
EXPORT(sim_name_color) const char *sim_name_color(int c) { return pet_color_name(c); }

/* mis mascotas: las que se quedan descansando (la página las guarda junto con la actual) */
static pet_t s_guardadas[PET_GUARDADAS];
EXPORT(sim_saved_ptr) uint8_t *sim_saved_ptr(void) { return (uint8_t *)s_guardadas; }
EXPORT(sim_saved_size) int sim_saved_size(void) { return (int)sizeof s_guardadas; }
static bool guardada_leer(int i, pet_t *out)
{
    if (i < 0 || i >= PET_GUARDADAS) return false;
    pet_t *g = &s_guardadas[i];
    if (!pet_validate(g) && !pet_upgrade(g)) return false;
    if (out) *out = *g;
    return true;
}
static const ui_mascotas_t MASCOTAS = {guardada_leer};
/* sella la página antes de guardar las que descansan (por si vienen de una versión anterior) */
EXPORT(sim_saved_fix) void sim_saved_fix(void) { for (int i = 0; i < PET_GUARDADAS; i++) if (!pet_validate(&s_guardadas[i])) { if (!pet_upgrade(&s_guardadas[i])) memset(&s_guardadas[i], 0, sizeof(pet_t)); } }

EXPORT(sim_init) int sim_init(uint32_t ahora, uint32_t seed, int cargada)
{
    lv_init();
    lv_tick_set_cb(tick_cb);
    s_disp = lv_display_create(W, W);
    lv_display_set_color_format(s_disp, LV_COLOR_FORMAT_XRGB8888);
    lv_display_set_buffers(s_disp, s_fb, NULL, sizeof s_fb, LV_DISPLAY_RENDER_MODE_DIRECT);
    lv_display_set_flush_cb(s_disp, flush_cb);
    lv_indev_t *in = lv_indev_create();
    lv_indev_set_type(in, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(in, touch_cb);

    s_reloj = ahora;
    int ok = cargada && (pet_validate(&s_pet) || pet_upgrade(&s_pet)); /* una partida de antes se actualiza */
    if (!ok) pet_init(&s_pet, "", (uint8_t)(seed % pet_species_count()), ahora, seed);
    pet_set_timezone(&s_pet, ZONA_HORARIA_S);
    pet_update(&s_pet, ahora); /* ponerse al día con el tiempo que pasó cerrada */
    ui_init(s_disp, on_action);
    ui_boot_done();
    ui_set_photo_store(&FOTOS);
    sim_saved_fix();
    ui_set_pet_store(&MASCOTAS);
    if (s_dispo.version != PET_DEVICE_VERSION) pet_device_defaults(&s_dispo);
    ui_set_device(&s_dispo);
    ui_set_battery(-1, false);
    procesar_eventos();
    ui_update(&s_pet, ahora);
    if (ok) log_linea(ahora, "volviste: %s te estaba esperando", s_pet.nombre, 0);
    else log_linea(ahora, "un huevo nuevo de %s: elige su nombre", pet_species_name(s_pet.especie), 0);
    bot_crear(ahora, seed * 2654435761u + 7u);
    return ok;
}

EXPORT(sim_reset) void sim_reset(uint32_t seed)
{
    uint32_t t = (uint32_t)s_reloj;
    memset(s_guardadas, 0, sizeof s_guardadas); /* de cero: también las que descansaban */
    pet_init(&s_pet, "", (uint8_t)(seed % pet_species_count()), t, seed);
    pet_set_timezone(&s_pet, ZONA_HORARIA_S);
    log_linea(t, "un huevo nuevo de %s", pet_species_name(s_pet.especie), 0);
}

/* Avanza dt_ms de tiempo real. Devuelve 1 si la pantalla está encendida. */
EXPORT(sim_frame) int sim_frame(uint32_t dt_ms)
{
    s_ms += dt_ms;
    s_reloj += dt_ms / 1000.0 * s_speed;
    uint32_t t = (uint32_t)s_reloj;

    int hubo = 0;
    while (s_acc_ini != s_acc_fin) {
        int a = s_acc[s_acc_ini].a, arg = s_acc[s_acc_ini].arg;
        switch (a) {
        case UI_ACCION_DECORAR: pet_place_decor(&s_pet, (decor_t)arg, t); break;
        case UI_ACCION_EDAD:
            pet_set_owner_age(&s_pet, arg);
            log_linea(t, "modo %s", pet_mode_name(pet_mode(&s_pet)), 0);
            break;
        case UI_ACCION_HEMISFERIO: s_pet.hemisferio_sur = (uint8_t)(arg != 0); break;
        case UI_ACCION_NACIMIENTO:
            if (pet_set_owner_birthdate(&s_pet, arg % 100, arg / 100 % 100, arg / 10000, t)) {
                char f[24];
                snprintf(f, sizeof f, "%02d/%02d/%04d", arg % 100, arg / 100 % 100, arg / 10000);
                log_linea(t, "naciste el %s", f, 0);
                log_linea(t, "modo %s", pet_mode_name(pet_mode(&s_pet)), 0);
            }
            break;
        case UI_ACCION_SONAR: encolar_sonido(arg); break;
        case UI_ACCION_HORA: { /* la hora que puso el dueño (en hora local) */
            uint32_t utc = (uint32_t)((int64_t)arg - s_pet.zona_horaria);
            s_reloj = utc;
            t = utc;
            if (!s_pet.edad_dueno && pet_is_egg(&s_pet) && !s_pet.nombre[0]) { /* primera vez: el huevo empieza ahora */
                pet_init(&s_pet, "", s_pet.especie, t, s_pet.rng ^ 0x9E3779B9u);
                pet_set_timezone(&s_pet, ZONA_HORARIA_S);
            }
            ui_set_clock_doubtful(false);
            log_linea(t, "pusiste la fecha y la hora", 0, 0);
            break;
        }
        case UI_ACCION_ASPECTO:
            if (pet_choose_look(&s_pet, arg & 15, ((arg >> 4) & 15) - 1, (arg >> 8) & 15, t, s_pet.rng ^ 0x5bd1e995u)) {
                char d[64];
                snprintf(d, sizeof d, "%s%s%s, color %s", pet_species_name(s_pet.especie), s_pet.mezcla ? " con rasgos de " : "",
                         s_pet.mezcla ? pet_species_name((uint8_t)(s_pet.mezcla - 1)) : "", pet_color_name(s_pet.color));
                log_linea(t, "saldra del huevo: %s", d, 0);
            }
            break;
        case UI_ACCION_AMBIENTE: s_dispo.ambiente = (uint8_t)((arg & 0xFF) > 100 ? 100 : (arg & 0xFF)); break;
        case UI_ACCION_DECOR_VER: pet_toggle_decor(&s_pet, (decor_t)arg); break;
        case UI_ACCION_RESTAURAR:
            if (arg && s_hay_respaldo) {
                s_pet = s_respaldo;
                pet_set_timezone(&s_pet, ZONA_HORARIA_S);
                log_linea(t, "llego %s desde el respaldo", s_pet.nombre, 0);
            } else log_linea(t, "no se trajo el respaldo", 0, 0);
            s_hay_respaldo = 0;
            break;
        case UI_ACCION_NOMBRE:
            pet_set_name(&s_pet, ui_pending_name());
            log_linea(t, "se llamara %s", s_pet.nombre, 0);
            break;
        case UI_ACCION_INVITAR: pet_invite(&s_pet, arg, t); log_linea(t, "invitaste a %s", s_pet.amigos[arg % PET_MAX_AMIGOS].nombre, 0); break;
        case UI_ACCION_RESPONDER: pet_answer_invite(&s_pet, arg != 0, t); break;
        case UI_ACCION_CAMBIAR:
            if (guardada_leer(arg, NULL)) {
                static pet_t otra;
                otra = s_guardadas[arg];
                pet_seal(&s_pet);
                s_guardadas[arg] = s_pet; /* la de ahora se queda descansando */
                s_pet = otra;
                pet_resume(&s_pet, &s_guardadas[arg], t);
                log_linea(t, "volvio %s (la otra se quedo descansando)", s_pet.nombre, 0);
            }
            break;
        case UI_ACCION_SOLTAR: {
            int lugar = arg & 0xFF, veh = (arg >> 8) - 1;
            if (lugar >= 0 && lugar < PET_GUARDADAS) {
                log_linea(t, "%s se fue de viaje a explorar el mundo", s_guardadas[lugar].nombre, 0);
                pet_add_traveller(&s_pet, &s_guardadas[lugar], veh < 0 ? 0 : veh, t);
                memset(&s_guardadas[lugar], 0, sizeof(pet_t));
            }
            break;
        }
        case UI_ACCION_NUEVA: {
            if ((arg & 0xFF) == 1) { /* se fue de viaje (despedida natural): un huevo nuevo del mismo dueño */
                { static pet_t viajera; viajera = s_pet; pet_add_traveller(&s_pet, &viajera, (arg >> 8) > 0 ? (arg >> 8) - 1 : 0, t); }
                pet_rebirth(&s_pet, (uint8_t)((t * 2654435761u >> 7) % pet_species_count()), t, t * 747796405u + 1u);
                log_linea(t, "se fue de viaje; queda un huevo nuevo", 0, 0);
                break;
            }
            int libre = -1;
            for (int i = 0; i < PET_GUARDADAS && libre < 0; i++) if (!guardada_leer(i, NULL)) libre = i;
            if (libre < 0) { log_linea(t, "no hay lugar: primero deja ir a una", 0, 0); break; }
            if (s_pet.etapa != STAGE_HUEVO || s_pet.nombre[0]) { pet_seal(&s_pet); s_guardadas[libre] = s_pet; log_linea(t, "%s se queda descansando", s_pet.nombre, 0); }
        }
            pet_new_fresh(&s_pet, (uint8_t)((t * 2654435761u >> 7) % pet_species_count()), t, t * 747796405u + 1u);
            s_pet.edad_dueno = 0;
            s_pet.dueno_dia = s_pet.dueno_mes = 0;
            log_linea(t, "un huevo nuevo de %s", pet_species_name(s_pet.especie), 0);
            break;
        case UI_ACCION_ALARMA: {
            pet_alarm_set_packed(&s_pet, arg);
            const alarma_t *al = &s_pet.alarma[arg & 7];
            char d[64];
            if (!al->usada) snprintf(d, sizeof d, "%d borrada", (arg & 7) + 1);
            else snprintf(d, sizeof d, "%d %s %02d:%02d (dias %02x, %s)", (arg & 7) + 1, al->activa ? "prendida" : "apagada", al->hora, al->minuto, al->dias,
                          pet_alarm_sound_name(al->sonido));
            log_linea(t, "alarma %s", d, 0);
            break;
        }
        case UI_ACCION_ALARMA_RESP:
            pet_alarm_answer(&s_pet, arg & 7, (arg & 8) != 0, t);
            log_linea(t, (arg & 8) ? "pospusiste la alarma" : "ya despertaste", 0, 0);
            break;
        case UI_ACCION_VOLUMEN:
            s_dispo.volumen = (uint8_t)(arg & 0xFF);
            pet_device_fix(&s_dispo);
            if (arg & UI_SOLTADO) encolar_sonido(SND_TOQUE); /* un pío de prueba */
            break;
        case UI_ACCION_EFECTOS:
            s_dispo.efectos = (uint8_t)(arg & 0xFF);
            pet_device_fix(&s_dispo);
            if (arg & UI_SOLTADO) encolar_sonido(SND_ATRAPA); /* un sonido de prueba */
            break;
        case UI_ACCION_ALARMAS:
            s_dispo.alarmas = (uint8_t)(arg & 0xFF);
            pet_device_fix(&s_dispo);
            if (arg & UI_SOLTADO) encolar_sonido(pet_alarm_sound(0)); /* suena la alarma con ese volumen */
            break;
        case UI_ACCION_BRILLO: s_dispo.brillo = (uint8_t)(arg & 0xFF); pet_device_fix(&s_dispo); break;
        case UI_ACCION_APAGADO: s_dispo.apagado = (uint8_t)arg; pet_device_fix(&s_dispo); break;
        case UI_ACCION_PECERA_DUR: s_dispo.pecera = (uint8_t)arg; pet_device_fix(&s_dispo); break;
        case UI_ACCION_NIEBLA: pet_fog_clear(&s_pet, t); log_linea(t, "despejaste la niebla", 0, 0); break;
        case UI_ACCION_COLOR: pet_set_color(&s_pet, arg); break;
        case UI_ACCION_ECLOSIONAR: pet_hatch(&s_pet, t); break;
        case UI_ACCION_HUEVO_EMPIEZA: pet_egg_start(&s_pet, t); break; /* (1.0.10) */
        case UI_ACCION_AIREAR: { const int r = pet_air_out(&s_pet, t); log_linea(t, "le espantaste el olor (%s)", r == 1 ? "se le quitó" : (r == 2 ? "bajó" : "no estaba hedionda"), 0); break; } /* (1.0.18) */
        case UI_ACCION_JUEGO_ISLA: { const int r = pet_island_game(&s_pet, arg & 15, (arg >> 4) & 127, t); log_linea(t, "terminó un juego de la isla (%s)", r == 2 ? "con concha nueva" : (r ? "contó" : "no contó"), 0); break; } /* (1.0.18) */
        case UI_ACCION_JUEGO_DOS: { const int r = pet_duo_game(&s_pet, arg & 15, (arg >> 4) & 3, t); log_linea(t, "terminó un juego de a dos (%s)", r ? "contó" : "no contó", 0); break; } /* (1.0.17) */
            case UI_ACCION_HABILIDAD: pet_ability_use(&s_pet, arg & 1, t); break;
        case UI_ACCION_BALLENA: log_linea(t, pet_whale_ride(&s_pet, arg, t) ? "paseo en la ballena: trajo polvo de estrellas" : "paseo en la ballena por el firmamento", 0, 0); break;
        case UI_ACCION_BAILE:        case UI_ACCION_EVOLUCION: if (arg) pet_evo_accept(&s_pet, t); else pet_evo_defer(&s_pet); log_linea(t, arg ? "decidiste que crezca" : "todavia no: lo puedes hacer en Ajustes", 0, 0); break;
 pet_dance(&s_pet, arg, t); log_linea(t, "bailo con la caja de musica", 0, 0); break;
        case UI_ACCION_CARTA: pet_letter_open(&s_pet, t); log_linea(t, "abriste la carta", 0, 0); break;
        case UI_ACCION_GENERO: s_pet.genero = (uint8_t)(arg != 0); log_linea(t, arg ? "sera un nino" : "sera una nina", 0, 0); break;
        case UI_ACCION_ANIMAL_VISTO: pet_animal_seen(&s_pet, arg, t); break;
        case UI_ACCION_CRIATURA: pet_creature_touch(&s_pet, t); log_linea(t, "tocaste a la criatura", 0, 0); break;
        case UI_ACCION_CITA_FIN: pet_creature_done(&s_pet, (cita_t)(arg & 0xFF), arg >> 8, t); log_linea(t, "terminaron la cita: %s", pet_cita_name((cita_t)(arg & 0xFF)), 0); break;
        case UI_ACCION_ESPECIAL: pet_special_do(&s_pet, (especial_t)(arg & 15), arg >> 4, t); log_linea(t, "secreto de la zona", 0, 0); break;
        case UI_ACCION_CRISTAL: log_linea(t, "tocaste un cristal", 0, 0); pet_crystal_note(&s_pet, arg, t); break;
        case UI_ACCION_PAIS: if (arg & 0x100) pet_set_country_by_season(&s_pet, (estacion_t)(arg & 3), t); else pet_set_country(&s_pet, arg); log_linea(t, "vive en %s", pet_country_name(s_pet.pais), 0); break;
        case UI_ACCION_FECHA_OTRA: { /* ver otra fecha: solo este perfil (el reloj del dispositivo no cambia) */
            uint32_t utc = (uint32_t)((int64_t)arg - s_pet.zona_horaria);
            pet_time_shift(&s_pet, (int32_t)((int64_t)utc - (int64_t)s_reloj));
            s_reloj = utc;
            t = utc;
            log_linea(t, "viste otra fecha (solo este perfil)", 0, 0);
            break;
        }
        case UI_ACCION_TILDE: pet_tilde_touch(&s_pet, arg, t); log_linea(t, arg ? "leiste la nota de Tilde" : "Tilde te conto un chisme", 0, 0); break;
        case UI_ACCION_BRIZNA: pet_brizna_heard(&s_pet, t); log_linea(t, "la Abuela Brizna conto un recuerdo", 0, 0); break;
        case UI_ACCION_PRISMA: pet_prisma_touch(&s_pet, t); log_linea(t, "tocaste al dragon de cristal", 0, 0); break;
        case UI_ACCION_TILO: pet_tilo_page_seen(&s_pet, arg); break;
        case UI_ACCION_LUGAR: pet_secret_find(&s_pet, arg >> 4, arg & 15, t); log_linea(t, "encontraste algo en un lugar secreto", 0, 0); break;
        case UI_ACCION_PIEZA: pet_crystal_piece(&s_pet, arg & 0xFF, arg >> 8, t); log_linea(t, "tocaron una pieza en los cristales", 0, 0); break; /* (v38) */
        case UI_ACCION_CALLAR: encolar_sonido(254); break; /* (v39) el navegador corta lo que suena */
        case UI_ACCION_ANIMAL_TOCAR: if (pet_animal_tell(&s_pet, arg, t)) log_linea(t, "un animal le contó algo (lo tocaste)", 0, 0); break;
        case UI_ACCION_POCHI_CHARLA: pet_pochi_chat_done(&s_pet, arg, t); log_linea(t, "charló con Pochi", 0, 0); break;
        case UI_ACCION_TOCAR_ANIMAL: { const int r = pet_animal_touch(&s_pet, arg & 0xFF, (arg >> 8) & 0xFF, t); ui_toque_resultado(r);
            static const char *const T[] = {"nada", "saludó a un animal", "el animal se molestó", "¡la picaron!", "se pinchó con las púas", "¡el zorrillo la roció!", "un animal grande la asustó", "el animal se fue", "el animal se escondió"};
            log_linea(t, T[(unsigned)r < 9 ? r : 0], 0, 0); break; }
        case UI_ACCION_TOCAR_FAUNA: { const int r = pet_fauna_touch(&s_pet, arg & 0xFF, (arg >> 8) & 0xFF, t); ui_toque_resultado(r);
            log_linea(t, r == PET_TOC_PICA ? "¡la abeja la picó!" : (r == PET_TOC_AVISA ? "el bicho se molestó" : "tocó un bicho"), 0, 0); break; }
        case UI_ACCION_EN_ZONA: pet_in_zone(&s_pet, arg != 0); break;
        case UI_ACCION_ZONA_HALLAZGO: pet_zone_find_take(&s_pet, arg, t); log_linea(t, "sacó lo escondido de un lugar secreto", 0, 0); break;
        case UI_ACCION_JUEGO_LUGAR: { const int r = pet_place_game_result(&s_pet, arg & 7, arg >> 5, (arg >> 3) & 3, t); char q[48]; snprintf(q, sizeof q, "%d %d estrellas, premios %d", arg & 7, (arg >> 3) & 3, r); log_linea(t, "terminó un juego de los lugares: %s", q, 0); break; }
        case UI_ACCION_EXCURSION: pet_excursion_back(&s_pet, arg, t); log_linea(t, "volvió de su excursión sola", 0, 0); break;
        case UI_ACCION_LIBRO_VISTO: pet_book_seen(&s_pet, arg); break;
        case UI_ACCION_ZONA_PISTA: pet_zone_hint_done(&s_pet, arg); break;
        case UI_ACCION_CANCION: { const char *nm = NULL; int n = 0; const uint8_t *nt = ui_pending_song(&n, &nm); pet_song_save(&s_pet, arg, nt, n, nm, t); log_linea(t, "guardaron una cancion", 0, 0); break; }
        case UI_ACCION_CANCION_USO: if (arg & 0x100) pet_song_delete(&s_pet, arg & 15); else pet_song_use(&s_pet, (arg & 15) == 15 ? -1 : (arg & 15), (arg >> 4) & 1); break;
        case UI_ACCION_POCHI: pet_pochi(&s_pet, arg, t); log_linea(t, arg == 0 ? "Pochi llego de visita" : (arg == 1 ? "Pochi comio" : (arg == 2 ? "paseo con Pochi" : (arg == 3 ? "Pochi volvio con sus cachorros" : (arg == 4 ? "Pochi la salpico de barro" : (arg >= PET_POCHI_F_AMIGO ? "un cachorro de Pochi estuvo con su animal amigo" : (arg >= PET_POCHI_F_CACHORRO ? "un cachorro de Pochi jugo con ella" : (arg == PET_POCHI_F_ANIMAL ? "Pochi y un animal del lugar" : (arg >= PET_POCHI_F_FIN ? "Pochi se acuerda de como termino la visita" : "Pochi hizo de las suyas")))))))), 0, 0); break;
        case UI_ACCION_CRISTALES: pet_crystal_game(&s_pet, arg, t); log_linea(t, "juego de los cristales", 0, 0); break;
        case UI_ACCION_LIBRO_AVISO: pet_book_tutorial_done(&s_pet); break;
        case UI_ACCION_FIESTA: pet_party_done(&s_pet, arg, t); log_linea(t, "celebraron con todos", 0, 0); break;
        case UI_ACCION_PESCA: pet_fish_game(&s_pet, arg, t); log_linea(t, arg == 0 ? "pesca con minijuego: empieza" : (arg == 1 ? "pesca: no picó" : (arg == 2 ? "pesca: se escapó" : "pesca: lo sacó")), 0, 0); break;
        case UI_ACCION_VIAJE_NO: pet_trip_not_yet(&s_pet, t); log_linea(t, "le dijiste que todavia no", 0, 0); break;
        case UI_ACCION_DESPEDIDA_NAT: s_pet.despedida_natural = (uint8_t)(arg != 0); log_linea(t, arg ? "despedida natural: activada" : "despedida natural: apagada", 0, 0); break;
        case UI_ACCION_FOTO: if (arg & 0x10000) pet_photo_legacy(&s_pet, arg & 0xFFFF); else pet_photo_add(&s_pet, arg); break;
        case UI_ACCION_FOTO_BORRADA: pet_photo_forget(&s_pet, arg); break;
        case UI_ACCION_ARRULLO_DUR: s_dispo.arrullo_dur = (uint8_t)(arg < 0 || arg > 2 ? 1 : arg); break;
        case UI_ACCION_ARRULLO_VOL: s_dispo.arrullo_vol = (uint8_t)(arg & 0xFF); break;
        case UI_ACCION_ARRULLO: s_dispo.arrullo = (uint8_t)(arg < 0 || arg > 3 ? 0 : arg); break;
        case UI_ACCION_MUSICA: s_dispo.musica = (uint8_t)(arg != 0); s_pet.escucha_musica = s_dispo.musica; break;
        case UI_ACCION_FUENTE: s_dispo.fuente = (uint8_t)(arg != 0); break;
        case UI_ACCION_GIRAR: s_dispo.girar = (uint8_t)(arg != 0); break;
        case UI_ACCION_FAMILIA: {
            int libre = -1;
            for (int i = 0; i < PET_GUARDADAS && libre < 0; i++) if (!guardada_leer(i, NULL)) libre = i;
            if (libre < 0 || !s_pet.huevo_familia) { log_linea(t, "no hay lugar para el huevo", 0, 0); break; }
            static pet_t madre;
            madre = s_pet;
            pet_family_child(&madre, &s_pet, t, t * 2246822519u + 7u);
            s_guardadas[libre] = madre;
            log_linea(t, "cuidas el huevo de familia de %s", madre.nombre, 0);
            break;
        }
        case UI_ACCION_VISITA_PERFIL:
            if (arg >= 0 && arg < PET_GUARDADAS && guardada_leer(arg, NULL) && pet_profile_visit(&s_pet, &s_guardadas[arg], t))
                log_linea(t, "%s vino de visita desde su perfil", s_guardadas[arg].nombre, 0);
            break;
            case UI_ACCION_RESPUESTA: pet_phrase_hug(&s_pet, t); break;
            case UI_ACCION_OBJETO: {
                int id = arg & 0xFF, op = (arg >> 8) & 7;
                const objeto_info_t *o = objeto_info(id);
                if (op == 0) {
                    int r = pet_object_use(&s_pet, id, t);
                    log_linea(t, r == 1 ? "uso: %s" : (r == 0 ? "no quiso: %s" : "no se pudo usar: %s"), o ? o->nombre : "?", 0);
                } else if (op == 1 && pet_object_return(&s_pet, id, t)) log_linea(t, "devuelto al bosque: %s", o ? o->nombre : "?", 0);
                else if (op == 2) pet_object_unplace(&s_pet, id);
                else if (op == 3) pet_object_head_off(&s_pet);
                else if (op == 5) { if (pet_chest_open(&s_pet, t) > 0) log_linea(t, "abrió el cofre del árbol viejo", 0, 0); }
                else if (op == 6) pet_nest_look(&s_pet, id, t);
                else pet_bag_seen(&s_pet);
                break;
            }
            case UI_ACCION_RITMO: pet_set_rhythm(&s_pet, arg); log_linea(t, "ritmo %s", pet_rhythm_name(arg), 0); break;
            case UI_ACCION_LIBRO: pet_libro_mark(&s_pet, arg); break;
            case UI_ACCION_FINAL: pet_final_done(&s_pet, arg, t); log_linea(t, "final secreto", 0, 0); break;
        case UI_ACCION_MODO: pet_set_mode_manual(&s_pet, arg); log_linea(t, "modo %s", pet_mode_name(pet_mode(&s_pet)), 0); break;
        case UI_ACCION_TEMPORIZADOR:
            pet_timer_set(&s_pet, (uint32_t)arg, t);
            log_linea(t, arg ? "temporizador puesto" : "temporizador cancelado", 0, 0);
            break;
        case UI_ACCION_PERFIL_VIVE: {
            int slot = arg & 15;
            bool vive = (arg >> 4) & 1;
            pet_t *q = slot == 15 ? &s_pet : (slot < PET_GUARDADAS && guardada_leer(slot, NULL) ? &s_guardadas[slot] : NULL);
            if (q) { pet_set_profile_lives(q, vive); if (q != &s_pet) pet_seal(q); log_linea(t, vive ? "%s sigue viviendo cuando no esta" : "%s descansa cuando no esta", q->nombre, 0); }
            break;
        }
        case UI_ACCION_DORMIR: {
            pet_bedtime_set_packed(&s_pet, arg);
            char d[48];
            snprintf(d, sizeof d, "%s, %02d:%02d a %02d:%02d", s_pet.dormir.modo ? "prendida" : "apagada", s_pet.dormir.acostarse_h,
                     s_pet.dormir.acostarse_m, s_pet.dormir.despertar_h, s_pet.dormir.despertar_m);
            log_linea(t, "hora de dormir: %s", d, 0);
            break;
        }
        default: pet_action(&s_pet, (action_t)a, arg, t); break;
        }
        s_acc_ini = (s_acc_ini + 1) % 16;
        hubo = 1;
    }
    if (t != s_ultimo_update || hubo) {
        s_ultimo_update = t;
        bot_paso(t);
        pet_update(&s_pet, t);
        { /* las alarmas "suena siempre" de las que descansan */
            static uint32_t revisadas;
            if (!revisadas || t < revisadas) revisadas = t;
            if (t - revisadas >= 30 && !ui_alarm_ringing()) {
                for (int i = 0; i < PET_GUARDADAS; i++) {
                    if (!guardada_leer(i, NULL)) continue;
                    int a = pet_alarm_resting(&s_guardadas[i], revisadas, t);
                    if (a >= 0) { ui_alarm_other(&s_guardadas[i], a); log_linea(t, "suena la alarma de %s (siempre)", s_guardadas[i].nombre, 0); break; }
                }
                revisadas = t;
            }
        }
        procesar_eventos();
        ui_update(&s_pet, t);
    }
    if (s_siempre_on) lv_display_trigger_activity(s_disp);
    if (ui_alarm_ringing()) lv_display_trigger_activity(s_disp); /* la alarma mantiene la pantalla encendida */
    if (ui_aquarium() && !ui_alarm_ringing() && lv_display_get_inactive_time(s_disp) >= pet_device_aquarium_ms(&s_dispo)) ui_pecera_vencida(); /* (v101) como en la placa */
    s_pantalla_on = ui_aquarium() || lv_display_get_inactive_time(s_disp) < (s_cargando_sim ? 65000u : pet_device_screen_ms(&s_dispo));
    ui_screen_on(s_pantalla_on);
    lv_timer_handler();
    return s_pantalla_on;
}

EXPORT(sim_fb) uint32_t *sim_fb(void) { return s_fb; }
/* (v89) modo "como la placa" para medir: RGB565 en franjas de 20 líneas (como el BSP), en vez del búfer entero de 32 bits.
 * Sirve para comparar cuánto cuesta dibujar cada pantalla (las franjas multiplican lo que cuesta preparar sombras, giros y escalas). */
static uint16_t s_tira[466 * 20];
EXPORT(sim_perfil) void sim_perfil(int on)
{
    if (on) { lv_display_set_color_format(s_disp, LV_COLOR_FORMAT_RGB565); lv_display_set_buffers(s_disp, s_tira, NULL, sizeof s_tira, LV_DISPLAY_RENDER_MODE_PARTIAL); }
    else { lv_display_set_color_format(s_disp, LV_COLOR_FORMAT_XRGB8888); lv_display_set_buffers(s_disp, s_fb, NULL, sizeof s_fb, LV_DISPLAY_RENDER_MODE_DIRECT); }
    lv_obj_invalidate(lv_screen_active());
}
EXPORT(sim_touch) void sim_touch(int x, int y, int pressed)
{
    if (!s_pantalla_on) { /* de bolsillo: con la pantalla apagada el táctil no cuenta (se enciende con el botón) */
        if (pressed && !s_bolsillo) lv_display_trigger_activity(s_disp);
        return;
    }
    s_tx = x; s_ty = y; s_tp = pressed;
}
EXPORT(sim_button) void sim_button(void) { lv_display_trigger_activity(s_disp); }
EXPORT(sim_pocket) void sim_pocket(int on) { s_bolsillo = on; }
/* caminar: los pasos que contaría el acelerómetro */
EXPORT(sim_walk) void sim_walk(int pasos)
{
    pet_steps_add(&s_pet, (uint32_t)pasos, (uint32_t)s_reloj);
    char n[16];
    snprintf(n, sizeof n, "%d", pasos);
    log_linea((uint32_t)s_reloj, "caminaste %s pasos", n, 0);
} /* 0: un toque enciende la pantalla (para las pruebas) */
/* la Bolsa: le da un objeto (-1 = uno al azar, como si lo encontrara), o -2 = uno de cada uno (para mirar el libro) */
/* sendas de pasto gastado marcadas (para mirar cómo se ven) */
EXPORT(sim_trails) void sim_trails(int v) { for (int i = 0; i < LUGAR_COUNT * (LUGAR_COUNT - 1) / 2; i++) s_pet.senda[i] = v ? 10.0f : 0.0f; }
EXPORT(sim_object) void sim_object(int id)
{
    uint32_t t = (uint32_t)s_reloj;
    if (id == -2) {
        for (int i = 0; i < PET_OBJETOS - 1; i++) if (!pet_object_count(&s_pet, i)) pet_object_gives(&s_pet, i, OFU_EXPLORAR, t);
        log_linea(t, "la Bolsa llena: uno de cada objeto", 0, 0);
        return;
    }
    if (id < 0) id = pet_object_find(&s_pet, OFU_EXPLORAR, t);
    if (id >= 0 && pet_object_gives(&s_pet, id, OFU_PASEO, t)) log_linea(t, "encontró: %s", objeto_info(id)->nombre, 0);
}
/* soplarle al reloj (en el reloj lo oye el micrófono); devuelve 1 si había algo que soplar */
EXPORT(sim_blow) int sim_blow(void)
{
    if (!ui_wants_mic()) return 0;
    ui_mic_blow();
    return 1;
}
/* un regalo al volver (r = regalo_t, -1 al azar) y una mañana con niebla */
EXPORT(sim_gift) void sim_gift(int r)
{
    uint32_t t = (uint32_t)s_reloj;
    if (r < 0) r = (int)((uint32_t)rand() % REG_COUNT);
    pet_gift_now(&s_pet, (regalo_t)r, t);
    log_linea(t, "te esperaba con %s", pet_gift_name((regalo_t)r), 0);
}
EXPORT(sim_fog) void sim_fog(void)
{
    uint32_t t = (uint32_t)s_reloj;
    pet_fog_start(&s_pet, t, 3600);
    log_linea(t, "amaneció con niebla", 0, 0);
}
EXPORT(sim_say_long) void sim_say_long(void) { ui_say("ESTE ES UN MENSAJE MUY LARGO PARA PROBAR QUE EL GLOBO SE PUEDE DESLIZAR CON EL DEDO CUANDO EL TEXTO NO CABE ENTERO ADENTRO DEL GLOBO."); }
EXPORT(sim_style) void sim_style(int e) { ui_set_style(e); }
EXPORT(sim_device_ptr) uint8_t *sim_device_ptr(void) { return (uint8_t *)&s_dispo; }
EXPORT(sim_device_size) int sim_device_size(void) { return (int)sizeof s_dispo; }
EXPORT(sim_device_fix) void sim_device_fix(void) { pet_device_fix(&s_dispo); }
/* el brillo y el volumen que la página aplica (filtro del lienzo y ganancia del audio) */
EXPORT(sim_brightness) int sim_brightness(void)
{
    int64_t l = (int64_t)s_reloj + s_pet.zona_horaria;
    int hl = (int)((l % 86400 + 86400) % 86400 / 3600);
    bool alarma = ui_alarm_ringing(), pecera = ui_aquarium() && !alarma;
    bool noche = !alarma && (pet_is_bedtime(&s_pet, (uint32_t)s_reloj) || (pecera && (hl >= 22 || hl < 7)));
    return pet_device_brightness(&s_dispo, pecera, noche);
}
EXPORT(sim_volume) int sim_volume(void) { return ui_alarm_ringing() ? pet_device_alarm_volume(&s_dispo) : s_dispo.volumen; }
EXPORT(sim_volume_for) int sim_volume_for(int snd) { return pet_device_sound_volume(&s_dispo, (sound_t)snd); }
EXPORT(sim_keep_awake) void sim_keep_awake(int on) { s_siempre_on = on; lv_display_trigger_activity(s_disp); }
EXPORT(sim_set_speed) void sim_set_speed(double v) { s_speed = v; }
EXPORT(sim_now) double sim_now(void) { return s_reloj; }
EXPORT(sim_set_now) void sim_set_now(double t) { s_reloj = t; }
EXPORT(sim_action) void sim_action(int a, int arg) { on_action(a, arg); }
EXPORT(sim_tilt) void sim_tilt(double x, double y) { ui_set_tilt((float)x, (float)y); }
EXPORT(sim_trick) float sim_trick(int t) { return t >= 0 && t < TRUCO_COUNT ? s_pet.truco[t] : 0; }

/* sonidos: el mismo sintetizador que usa el reloj */
#define SFX_HZ 22050
static int16_t s_pcm[190000]; /* hasta 8,5 s a 22,05 kHz */
EXPORT(sim_sound_render) int sim_sound_render(int s) { return sfx_render((sound_t)s, s_pcm, (int)(sizeof s_pcm / 2), SFX_HZ); }
EXPORT(sim_sound_pcm) int16_t *sim_sound_pcm(void) { return s_pcm; }
EXPORT(sim_sound_hz) int sim_sound_hz(void) { return SFX_HZ; }
EXPORT(sim_vinculo) float sim_vinculo(void) { return s_pet.vinculo; }
EXPORT(sim_child) int sim_child(void) { return s_pet.modo_nino; }
EXPORT(sim_sabe) void sim_sabe(int id) { s_pet.obj_sabe[id >> 3] |= (uint8_t)(1 << (id & 7)); }
EXPORT(sim_dormir) void sim_dormir(int d) { s_pet.dormida = (uint8_t)d; }
EXPORT(sim_set_modo) void sim_set_modo(int m) { s_pet.modo_nino = (uint8_t)m; }
EXPORT(sim_emo) float sim_emo(int i) { return pet_emotion(&s_pet, (emo_t)i); }
EXPORT(sim_emo_cause) int sim_emo_cause(int i) { return pet_emotion_cause(&s_pet, (emo_t)i); }
EXPORT(sim_name_emo) const char *sim_name_emo(int i) { return pet_emotion_name((emo_t)i); }
EXPORT(sim_name_cause) const char *sim_name_cause(int i) { return pet_cause_name((causa_t)i); }
EXPORT(sim_activity) int sim_activity(void) { return s_pet.actividad; }
EXPORT(sim_set_act) void sim_set_act(int a) { s_pet.actividad = (uint8_t)a; s_pet.actividad_hasta = (uint32_t)s_reloj + 7200u; } /* (pruebas) */
void ui_demo_cuadro(int c);
EXPORT(sim_cuadro) void sim_cuadro(int c) { ui_demo_cuadro(c); }
void ui_demo_juego(int j);
void ui_demo_botella(int f);
void ui_demo_flotar(int ico, int x, int y);
EXPORT(sim_flotar) void sim_flotar(int ico, int x, int y) { ui_demo_flotar(ico, x, y); }
void ui_demo_picnic(void);
void ui_demo_bateria(int pct, int modo);
EXPORT(sim_bateria_aviso) void sim_bateria_aviso(int pct, int modo) { ui_demo_bateria(pct, modo); }
EXPORT(sim_rapido) void sim_rapido(void) { ui_demo_rapido(); }
EXPORT(sim_cofre) void sim_cofre(int v, int s) { ui_demo_cofre(v, s); }
EXPORT(sim_picnic) void sim_picnic(void) { ui_demo_picnic(); }
EXPORT(sim_botella) void sim_botella(int f) { ui_demo_botella(f); }
EXPORT(sim_juego) void sim_juego(int j) { ui_demo_juego(j); }
EXPORT(sim_cavar) void sim_cavar(void) { s_pet.actividad = ACTV_CAVAR; }
EXPORT(sim_actividad) void sim_actividad(int a, int extra) { s_pet.actividad = (uint8_t)a; s_pet.actividad_hasta = (uint32_t)s_reloj + 600; if (a == ACTV_PROBAR || a == ACTV_CHARLAR) s_pet.probando = (uint8_t)extra; }
EXPORT(sim_musica) void sim_musica(int tempo, int bpm, int en) { pet_music_heard(&s_pet, (uint32_t)s_reloj, tempo, bpm, en); } /* tempo 0 lenta, 1 media, 2 rápida */
EXPORT(sim_gusto) void sim_gusto(int a, int v) { if (a >= 0 && a < 32) s_pet.gusto[a] = (int8_t)v; }
EXPORT(sim_place) int sim_place(void) { return s_pet.lugar; }
EXPORT(sim_name_activity) const char *sim_name_activity(int a) { return pet_activity_name((actividad_t)a); }
EXPORT(sim_name_place) const char *sim_name_place(int l) { return pet_place_name((lugar_t)l); }
EXPORT(sim_name_trait) const char *sim_name_trait(int t) { return pet_trait_name((trait_t)t); }
EXPORT(sim_item) int sim_item(int i) { return i >= 0 && i < ITEM_COUNT ? s_pet.inventario[i] : 0; }
EXPORT(sim_form) int sim_form(void) { return s_pet.forma; }
EXPORT(sim_scene) int sim_scene(void) { return s_pet.escena; }
EXPORT(sim_open_all) void sim_open_all(void) { s_pet.escenas_abiertas = (1u << ESC_COUNT) - 1; }
/* hoy es tu cumpleaños: cambia tu fecha de nacimiento a hoy (mismo año) y lo celebra */
EXPORT(sim_birthday_today) void sim_birthday_today(void)
{
    uint32_t t = (uint32_t)s_reloj;
    int a, m, d;
    pet_local_date(&s_pet, t, &a, &m, &d);
    int anio = s_pet.dueno_anio ? s_pet.dueno_anio : a - 30;
    if (anio >= a) anio = a - 1;
    pet_set_owner_birthdate(&s_pet, d, m, anio, t);
}
EXPORT(sim_neighbor_now) void sim_neighbor_now(void)
{
    uint32_t t = (uint32_t)s_reloj;
    if (s_pet.vecino.presente) return;
    s_pet.vecino.se_fue = 0;
    s_pet.ultima_interaccion = t;
    s_pet.vecino.proxima = t;
}
EXPORT(sim_neighbor_here) int sim_neighbor_here(void) { return s_pet.vecino.presente; }
/* (pruebas) lo de la versión 21 */
static void sim_evento(event_type_t ty, int a, int b) { pet_event_t e = {ty, (int8_t)a, (int8_t)b, 0, (uint32_t)s_reloj}; describir(&e); ui_show_event(&s_pet, &e); }
EXPORT(sim_family_egg) void sim_family_egg(void) { s_pet.huevo_familia = 1; s_pet.dia_familia = 1; sim_evento(EV_FAMILIA, 0, 0); }
EXPORT(sim_ask_trip) void sim_ask_trip(void) { sim_evento(EV_PIDE_VIAJE, 0, 0); }
EXPORT(sim_anecdote) void sim_anecdote(void) { if (pet_diary_count(&s_pet)) sim_evento(EV_ANECDOTA, pet_diary_count(&s_pet) - 1, 0); }
EXPORT(sim_ritual) void sim_ritual(int cual, int nuevo) { sim_evento(EV_RITUAL, cual, nuevo); }
EXPORT(sim_waited) void sim_waited(void) { sim_evento(EV_TE_ESPERABA, 1, 0); }
EXPORT(sim_give_object) void sim_give_object(int id) { pet_object_gives(&s_pet, id, OFU_REGALO, (uint32_t)s_reloj); }
/* deja todo listo para el rito: adulta, en la isla, con el Corazón, de noche y con luna llena */
EXPORT(sim_guardian_ready) int sim_guardian_ready(void)
{
    uint32_t t = (uint32_t)s_reloj;
    for (int i = 0; i < 40 && !pet_full_moon(t); i++) t += 86400u;
    int64_t l = (int64_t)t + s_pet.zona_horaria;
    int h = (int)((l % 86400 + 86400) % 86400 / 3600);
    t += (uint32_t)(((22 - h) + 24) % 24) * 3600u; /* a las 22 */
    s_reloj = t;
    s_pet.etapa = STAGE_ADULTA;
    s_pet.escenas_abiertas |= 1u << ESC_ISLA;
    pet_update(&s_pet, t);
    s_pet.escena = ESC_ISLA;
    s_pet.dormida = 0;
    pet_object_gives(&s_pet, OBJ_CORAZON_BOSQUE, OFU_REGALO, t);
    return pet_guardian_ready(&s_pet, t);
}
EXPORT(sim_guardian) int sim_guardian(void) { return s_pet.guardiana; }
EXPORT(sim_set_guardian) void sim_set_guardian(int on) { s_pet.guardiana = (uint8_t)on; }
EXPORT(sim_whale_ready) int sim_whale_ready(void) { return pet_whale_can_ride(&s_pet, (uint32_t)s_reloj); }
EXPORT(sim_letter_now) int sim_letter_now(void) { if (!s_pet.vecinos_vistos) s_pet.vecinos_vistos = 0x31u; return pet_letter_now(&s_pet, (uint32_t)s_reloj); }
EXPORT(sim_letter_pending) int sim_letter_pending(void) { return pet_letter_pending(&s_pet); }
EXPORT(sim_gender) int sim_gender(void) { return s_pet.genero; }
EXPORT(sim_decor) void sim_decor(int bits) { s_pet.decoraciones |= (uint32_t)bits; }
EXPORT(sim_animal) void sim_animal(int a) { pet_animal_call(&s_pet, (animal_t)a, (uint32_t)s_reloj); }
EXPORT(sim_toque) void sim_toque(int res) { void ui_demo_toque(int); ui_demo_toque(res); } /* (fw 103) ver la reacción */
EXPORT(sim_hediondo_get) int sim_hediondo_get(void) { return s_pet.hediondo; }
/* (fw 104) las novedades: paseo de verdad, papelito del bosque, trocito de hielo y terquedad */
EXPORT(sim_paseo) int sim_paseo(int pasos) { s_pet.paseo_n = 0; s_pet.dormida = 0; return pet_walk_together(&s_pet, (uint32_t)(pasos > 0 ? pasos : 0), (uint32_t)s_reloj); }
EXPORT(sim_papelito) int sim_papelito(int ella) { return pet_papelito_test(&s_pet, ella, (uint32_t)s_reloj); }
EXPORT(sim_hielo) void sim_hielo(int que) { pet_ice_test(&s_pet, que, (uint32_t)s_reloj); }
EXPORT(sim_terca) int sim_terca(int si) { s_pet.rasgo[TRAIT_TRAVIESA] = si ? 0.95f : 0.05f; s_pet.rasgo[TRAIT_TIMIDA] = si ? 0.05f : 0.95f; return (int)(pet_stubbornness(&s_pet) * 100.0f); }
EXPORT(sim_hediondo) void sim_hediondo(int si) { s_pet.hediondo = (uint8_t)(si ? 18 : 0); s_pet.hediondo_min = 0; } /* (fw 103) el olor del zorrillo */
EXPORT(sim_go) void sim_go(int e) { s_pet.escenas_abiertas |= 1u << e; s_pet.escena = (uint8_t)e; }
/* versión 23: cartas de la familia y de las viajeras, criaturas y secretos */
EXPORT(sim_letter_from) int sim_letter_from(int de)
{
    if (de == CARTA_FAMILIA && !s_pet.padre[0]) { snprintf(s_pet.padre, sizeof s_pet.padre, "Luna"); s_pet.padre_genero = 0; }
    if (de == CARTA_VIAJERO && !s_pet.viajeros_n) { static pet_t v; v = s_pet; snprintf(v.nombre, sizeof v.nombre, "Nube"); pet_add_traveller(&s_pet, &v, 2, (uint32_t)s_reloj); }
    if (de == CARTA_AMIGO && !s_pet.vecinos_vistos) s_pet.vecinos_vistos = 0x31u;
    s_pet.carta_pend = 0;
    return pet_letter_now_from(&s_pet, (carta_de_t)de, (uint32_t)s_reloj);
}
EXPORT(sim_creature) void sim_creature(int c) { s_pet.escenas_abiertas |= 1u << pet_creature_zone((magica_t)c); s_pet.escena = (uint8_t)pet_creature_zone((magica_t)c); pet_creature_call(&s_pet, (magica_t)c, (uint32_t)s_reloj); }
EXPORT(sim_creature_invite) int sim_creature_invite(int c) { s_pet.escenas_abiertas |= 1u << pet_creature_zone((magica_t)c); s_pet.carta_pend = 0; return pet_creature_invite_now(&s_pet, (magica_t)c, (uint32_t)s_reloj); }
EXPORT(sim_creature_here) int sim_creature_here(void) { magica_t c; return pet_creature_here(&s_pet, (uint32_t)s_reloj, &c) ? (int)c + (pet_creature_waiting(&s_pet, (uint32_t)s_reloj) ? 16 : 0) : -1; }
EXPORT(sim_special_ready) int sim_special_ready(int e) { return pet_special_ready(&s_pet, (especial_t)e, (uint32_t)s_reloj); }
EXPORT(sim_special_known) int sim_special_known(int e) { return pet_special_known(&s_pet, (especial_t)e); }
EXPORT(sim_cards) int sim_cards(void) { return pet_cards_known(&s_pet); }
EXPORT(sim_card_seen) void sim_card_seen(int f) { pet_animal_seen(&s_pet, f, (uint32_t)s_reloj); }
void ui_demo_cita(int que);
void ui_demo_anillo(void);
void ui_demo_cometa(void);
EXPORT(sim_cita) void sim_cita(int q) { ui_demo_cita(q); }
EXPORT(sim_ritmo) void sim_ritmo(int r) { pet_set_rhythm(&s_pet, r); }
EXPORT(sim_hora_su) int sim_hora_su(void) { return pet_pet_hour(&s_pet, (uint32_t)s_reloj); }
EXPORT(sim_dormida) int sim_dormida(void) { return s_pet.dormida; }
EXPORT(sim_objetos_vistos) int sim_objetos_vistos(int n) { for (int i = 0; i < PET_OBJETOS; i++) { if (i < n) s_pet.obj_sabe[i >> 3] |= (uint8_t)(1 << (i & 7)); else s_pet.obj_sabe[i >> 3] &= (uint8_t)~(1 << (i & 7)); } return pet_objects_used(&s_pet); }
EXPORT(sim_final_estado) int sim_final_estado(void) { return s_pet.final_estado | (s_pet.final_pend << 4); }
EXPORT(sim_final_borrar) void sim_final_borrar(void) { s_pet.final_estado = 0; s_pet.final_pend = 0; }
EXPORT(sim_libro_bits) int sim_libro_bits(void) { return s_pet.libro; }
EXPORT(sim_libro) void sim_libro(int n) { s_pet.libro = (uint8_t)(s_pet.libro & ~(1u << n)); ui_libro_pedir(n); }
EXPORT(sim_cine) void sim_cine(int n) { ui_cine(n); }
EXPORT(sim_fuente) void sim_fuente(int g) { s_dispo.fuente = (uint8_t)g; ui_set_device(&s_dispo); }
EXPORT(sim_final_pend) void sim_final_pend(int f) { s_pet.final_pend = (uint8_t)f; }
EXPORT(sim_ev_guardiana) void sim_ev_guardiana(void) { s_pet.dormida = 0; s_pet.guardiana = 1; sim_evento(EV_GUARDIANA, 0, 0); } /* la escena del rito (Final 1) */
EXPORT(sim_final_set) void sim_final_set(int e) { s_pet.final_estado = (uint8_t)e; s_pet.final_pend = 0; }
EXPORT(sim_ritmo_visto) int sim_ritmo_visto(int v) { s_pet.ritmo_visto = (uint8_t)v; return s_pet.ritmo; }
void ui_demo_rito_historia(void);
EXPORT(sim_rito_historia) void sim_rito_historia(void) { ui_demo_rito_historia(); }
/* (v37) la historia del mundo: para probarla rápido */
EXPORT(sim_days_played) int sim_days_played(void) { return pet_days_played(&s_pet); }
EXPORT(sim_story_act) int sim_story_act(void) { return pet_story_act(&s_pet); }
EXPORT(sim_days_add) void sim_days_add(int n) { s_pet.dias_jugados = (uint16_t)(pet_days_played(&s_pet) + n); s_pet.hist_hoy = 0; s_pet.prisma_dia = 0; s_pet.animal_dia = 0; log_linea((uint32_t)s_reloj, "adelantaste dias jugados", 0, 0); }
EXPORT(sim_tilde) void sim_tilde(void)
{
    if (s_pet.escena == ESC_CUEVA) s_pet.escena = ESC_PRADO;
    s_pet.dormida = 0;
    s_pet.tilde_prox = 1;
    s_pet.ultima_interaccion = (uint32_t)s_reloj;
    for (int i = 0; i < 400 && !pet_tilde_here(&s_pet, (uint32_t)s_reloj); i++) pet_update(&s_pet, (uint32_t)s_reloj); /* (llega con un poco de azar) */
    log_linea((uint32_t)s_reloj, "llamaste a Tilde", 0, 0);
}
EXPORT(sim_brizna) void sim_brizna(void)
{
    s_pet.escenas_abiertas |= 1u << ESC_ISLA;
    s_pet.escena = ESC_ISLA;
    s_pet.dormida = 0;
    s_pet.hist_hoy = 0;
    s_pet.brizna_ultimo = 0;
    if (pet_days_played(&s_pet) < 26) s_pet.dias_jugados = (uint16_t)(pet_days_played(&s_pet) < 8 ? 8 : pet_days_played(&s_pet) + 7);
    s_pet.ultima_interaccion = (uint32_t)s_reloj;
    pet_update(&s_pet, (uint32_t)s_reloj);
    log_linea((uint32_t)s_reloj, "despertaste a la Abuela Brizna", 0, 0);
}
EXPORT(sim_tilo_page) void sim_tilo_page(void) { for (int k = 0; k < PET_TILO_PAGINAS; k++) if (!pet_tilo_page(&s_pet, k)) { s_pet.tilo_paginas |= (uint8_t)(1u << k); break; } }
void ui_demo_lugar(int l);
EXPORT(sim_secret_place) void sim_secret_place(int l) { s_pet.lugares |= (uint8_t)(1u << l); ui_demo_lugar(l); }
EXPORT(sim_prisma) void sim_prisma(void)
{
    s_pet.especiales |= 1u << ESP_CRISTALES;
    if (pet_days_played(&s_pet) < 15) s_pet.dias_jugados = 15;
    s_pet.prisma_dia = 0; s_pet.hist_hoy = 0;
    const int k = pet_prisma_touch(&s_pet, (uint32_t)s_reloj);
    if (k >= 4 && pet_days_played(&s_pet) < 22) s_pet.dias_jugados = 22;
}
void ui_demo_historia(int c, int k);
EXPORT(sim_historia) void sim_historia(int c, int k) { ui_demo_historia(c, k); }
uint32_t ui_soltar_iconos(void);
EXPORT(sim_soltar_iconos) int sim_soltar_iconos(void) { return (int)ui_soltar_iconos(); }
EXPORT(sim_art_bytes) int sim_art_bytes(void) { extern uint32_t g_art_bytes; return (int)g_art_bytes; }
EXPORT(sim_mem_panel) int sim_mem_panel(int q) { extern uint32_t ui_mem_panel(int); return (int)ui_mem_panel(q); }
EXPORT(sim_historias_todas) void sim_historias_todas(void) /* (pruebas) el Libro de historias lleno, para ver cómo se ve */
{
    for (int c = 0; c < CRI_COUNT; c++) s_pet.historias[c] = 0x1F;
    s_pet.historias_nimba = 0x1F; s_pet.prisma_caps = 0xFF; s_pet.tilde_chismes = 0x0FFF; s_pet.brizna = 0x1F; s_pet.tilo_paginas = 0xFF;
    memset(s_pet.animal_frases, 0xFF, sizeof s_pet.animal_frases);
}
EXPORT(sim_nimba) void sim_nimba(int bits, int aviso) { s_pet.historias_nimba = (uint8_t)bits; s_pet.libro_aviso = (uint8_t)aviso; s_pet.libro_aviso_de = 1; }
void ui_demo_dragon(void);
void ui_demo_pochi(int f);
void ui_demo_hongos(void);
void ui_demo_nochevieja(int f);
EXPORT(sim_hongos) void sim_hongos(void) { ui_demo_hongos(); }
int ui_ny_debug(void);
EXPORT(sim_ny_dbg) int sim_ny_dbg(void) { return ui_ny_debug(); }
EXPORT(sim_despierta) void sim_despierta(void) { s_pet.dormida = 0; s_pet.actividad = ACTV_PASEAR; }
int ui_dbg_pos(int k);
EXPORT(sim_dbg) int sim_dbg(int k) { return ui_dbg_pos(k); }
void ui_demo_frutas(void);
EXPORT(sim_frutas) void sim_frutas(void) { ui_demo_frutas(); }
EXPORT(sim_sacudir) void sim_sacudir(void) { if (ui_captura_sacudida()) ui_sacudida(); else on_action(ACT_SACUDIR, 0); }
void ui_test_pescar(void); int ui_test_pesca_fase(void);
EXPORT(sim_pescar_bote) void sim_pescar_bote(void) { ui_test_pescar(); }
EXPORT(sim_pesca_fase) int sim_pesca_fase(void) { return ui_test_pesca_fase(); }
EXPORT(sim_mic_nivel) void sim_mic_nivel(int n) { ui_mic_nivel(n); }
EXPORT(sim_pesca) void sim_pesca(int k) { if (s_pet.escena != ESC_LAGO) s_pet.escena = ESC_LAGO; pet_action(&s_pet, ACT_PESCAR, 0, (uint32_t)s_reloj); ui_demo_pesca(k); } /* (pruebas) */
EXPORT(sim_nochevieja) void sim_nochevieja(int f) { ui_demo_nochevieja(f); }
/* (v38) las novedades: para probarlas rápido */
void ui_demo_libre(int componer);
EXPORT(sim_libre) void sim_libre(int componer)
{
    s_pet.escenas_abiertas |= 1u << ESC_CUEVA;
    s_pet.escena = ESC_CUEVA; s_pet.dormida = 0; if (s_pet.etapa == STAGE_HUEVO) s_pet.etapa = STAGE_CRIA;
    pet_update(&s_pet, (uint32_t)s_reloj);
    ui_demo_libre(componer);
}
EXPORT(sim_dragon_despierto) void sim_dragon_despierto(void) { s_pet.especiales |= 1u << ESP_CRISTALES; log_linea((uint32_t)s_reloj, "desperto el dragon de cristal", 0, 0); }
EXPORT(sim_proponer_cancion) void sim_proponer_cancion(void) { if (s_pet.cris_piezas < 2) s_pet.cris_piezas = 2; else s_pet.cris_piezas = (uint16_t)((s_pet.cris_piezas / 6 + 1) * 6 - 1); s_pet.cris_propuso_dia = 0; }
EXPORT(sim_escama) void sim_escama(void) { s_pet.especiales |= 1u << ESP_CRISTALES; s_pet.cris_dias = 2; s_pet.cris_dia = 0; s_pet.cris_flags &= (uint8_t)~1u; }
EXPORT(sim_lago_helado) void sim_lago_helado(void)
{
    s_pet.escena = ESC_LAGO; s_pet.dormida = 0;
    s_pet.lago_helado_hasta = (uint32_t)s_reloj + 4u * 3600u;
    pet_update(&s_pet, (uint32_t)s_reloj);
    log_linea((uint32_t)s_reloj, "el lago se congelo (bebida helada del norte)", 0, 0);
}
EXPORT(sim_materiales) void sim_materiales(void) { s_pet.ramitas = (uint8_t)(s_pet.ramitas + 3); s_pet.musgo = (uint8_t)(s_pet.musgo + 3); log_linea((uint32_t)s_reloj, "le diste ramitas y musgo", 0, 0); }
EXPORT(sim_canciones) int sim_canciones(void) { return pet_song_count(&s_pet); }
EXPORT(sim_ano_nuevo) void sim_ano_nuevo(void) /* el 31 de diciembre, un minuto y medio antes de la cuenta (a las 21:00 en modo niño o infante) */
{
    int a = 2026, m, d;
    pet_local_date(&s_pet, (uint32_t)s_reloj, &a, &m, &d);
    if (m == 1 && d == 1) a--;
    /* días desde 1970 hasta el 31 de diciembre de ese año */
    int64_t dias = 0;
    for (int y = 1970; y <= a; y++) dias += (y % 4 == 0 && (y % 100 != 0 || y % 400 == 0)) ? 366 : 365;
    dias -= 1;
    const int hora = s_pet.modo_nino ? 20 : 23;
    s_reloj = (double)(dias * 86400 + hora * 3600 + 58 * 60 + 30 - s_pet.zona_horaria);
    s_pet.escena = ESC_PRADO; s_pet.dormida = 0; s_pet.ultima_actualizacion = (uint32_t)s_reloj;
    ui_demo_nochevieja(0);
    log_linea((uint32_t)s_reloj, "31 de diciembre, poco antes de la cuenta regresiva", 0, 0);
}
void ui_demo_dragon_ausente(int v);
EXPORT(sim_dragon_absent) void sim_dragon_absent(int v) { ui_demo_dragon_ausente(v); }
EXPORT(sim_pochi) void sim_pochi(int f) { ui_demo_pochi(f); }
/* (v39) las novedades: para probarlas rápido */
void ui_demo_zona(int z); void ui_demo_zona_juego(int jg); int ui_zona(void); void ui_demo_ceremonia(int cual, int pag);
static void v39_jugando(void) { s_pet.dormida = 0; if (s_pet.etapa == STAGE_HUEVO) s_pet.etapa = STAGE_CRIA; s_pet.ultima_interaccion = (uint32_t)s_reloj; }
EXPORT(sim_animal_cuenta) int sim_animal_cuenta(void) /* trae un animal conocido y le cuenta su frase (si ya contó todas las que tocan, avanza un acto) */
{
    v39_jugando();
    const uint32_t t = (uint32_t)s_reloj;
    const int f = FAUNA_COUNT + ANIMAL_CONEJO;
    pet_animal_seen(&s_pet, f, t); /* (la primera vez lo conoce) */
    static const uint8_t ANIM[] = {ANIMAL_CONEJO, ANIMAL_ARDILLA, ANIMAL_TOPO, ANIMAL_BUHO, ANIMAL_CARACOL, ANIMAL_TORTUGA, ANIMAL_ERIZO};
    for (int vuelta = 0; vuelta < 3; vuelta++) {
        for (unsigned i = 0; i < sizeof ANIM; i++) {
            const int fi = FAUNA_COUNT + ANIM[i];
            pet_animal_seen(&s_pet, fi, t);
            s_pet.animal_dia = 0; s_pet.animal_hoy = 0;
            if (ANIM[i] == ANIMAL_ERIZO) s_pet.erizo_confianza = 3;
            if (pet_animal_tell(&s_pet, fi, t)) { log_linea(t, "un animal le contó algo del Rey", 0, 0); return 1; }
        }
        s_pet.dias_jugados = (uint16_t)(s_pet.dias_jugados < 11 ? 11 : (s_pet.dias_jugados < 22 ? 22 : s_pet.dias_jugados)); /* ya contaron las de este acto: el siguiente */
    }
    return 0;
}
EXPORT(sim_pagina_suelta) void sim_pagina_suelta(void) { v39_jugando(); pet_object_gives(&s_pet, 110, OFU_REGALO, (uint32_t)s_reloj); s_pet.hist_hoy = 0; pet_object_use(&s_pet, 110, (uint32_t)s_reloj); }
EXPORT(sim_zona) void sim_zona(int z) /* 1 el claro, 2 la casita, 3 el mirador */
{
    v39_jugando();
    s_pet.lugares |= z == 3 ? 2u : (1u | 4u);
    s_pet.escenas_abiertas |= (1u << ESC_BOSQUE) | (1u << ESC_COLINA);
    s_pet.escena = (uint8_t)(z == 3 ? ESC_MIRADOR : ESC_CLARO); /* (v98: llega de una vez, como con un portal; sin preguntarle si quiere) */
    s_pet.escenas_vistas |= (uint8_t)(1u << s_pet.escena);
    s_pet.dormida = 0;
    pet_update(&s_pet, (uint32_t)s_reloj);
    ui_demo_zona(z);
}
EXPORT(sim_zona_juego) void sim_zona_juego(int jg) { ui_demo_zona_juego(jg); }
EXPORT(sim_nube) void sim_nube(void) /* (v97) la nube del cerro baja a buscarla */
{
    void ui_demo_nube(void);
    v39_jugando();
    s_pet.lugares |= 2u;
    s_pet.escenas_abiertas |= 1u << ESC_COLINA;
    s_pet.dormida = 0;
    s_pet.escena = (uint8_t)(ui_escena_vista() == ESC_COLINA ? ESC_MIRADOR : ESC_COLINA); /* (v98) en el cerro: suben en la nube; si no, primero al cerro */
    (void)ui_demo_nube;
}
EXPORT(sim_zj_trampa) void sim_zj_trampa(int si) { extern void ui_zj_trampa(int); ui_zj_trampa(si); }
EXPORT(sim_zj_estado) int sim_zj_estado(void) { extern int ui_zj_estado(void); return ui_zj_estado(); }
EXPORT(sim_zj_tocar) void sim_zj_tocar(int x, int y) { extern void ui_zj_tocar(int, int); ui_zj_tocar(x, y); }
EXPORT(sim_zona_salir) void sim_zona_salir(void) { extern void ui_demo_zona_salir(void); ui_demo_zona_salir(); }
EXPORT(sim_zj_dbg) int sim_zj_dbg(int q) { extern int ui_zj_dbg(int); return ui_zj_dbg(q); }
/* (1.0.17) los juegos de a dos */
EXPORT(sim_zd_abrir) void sim_zd_abrir(int n) { extern void ui_zd_abrir(int); ui_zd_abrir(n); }
EXPORT(sim_hedionda) void sim_hedionda(int pasos) { s_pet.hediondo = (uint8_t)pasos; s_pet.hediondo_min = 0; } /* (1.0.18) */
EXPORT(sim_olor) int sim_olor(void) { return s_pet.hediondo; }
EXPORT(sim_periodo) int sim_periodo(void) { extern int ui_periodo(void); return ui_periodo(); } /* (1.0.18) 0 día, 1 atardecer, 2 noche (art_periodo_t) */
EXPORT(sim_zd_estado) int sim_zd_estado(void) { extern int ui_zd_estado(void); return ui_zd_estado(); }
EXPORT(sim_dos_jugadas) int sim_dos_jugadas(void) { return s_pet.dos_jugadas * 1000 + s_pet.dos_ganadas; }
/* (v40) las excursiones sola y lo escondido de los lugares secretos */
void ui_demo_excursion(int z); int ui_excursion(void);
EXPORT(sim_excursion) void sim_excursion(int z) /* 0 al claro, 1 al mirador, -1 que vuelva ya */
{
    if (z >= 0) {
        v39_jugando();
        s_pet.lugares |= z == 1 ? 2u : (1u | 4u);
        s_pet.escenas_abiertas |= (1u << ESC_BOSQUE) | (1u << ESC_COLINA);
        s_pet.escena = (uint8_t)(z == 1 ? ESC_COLINA : ESC_BOSQUE);
        pet_update(&s_pet, (uint32_t)s_reloj);
    }
    ui_demo_excursion(z);
}
EXPORT(sim_excursion_fase) int sim_excursion_fase(void) { return ui_excursion(); }
EXPORT(sim_escondido) int sim_escondido(int zona) { s_pet.zona_hallazgo[zona % 3] = 0; return pet_zone_find(&s_pet, zona % 3, (uint32_t)s_reloj) * 10 + pet_zone_find_spot(&s_pet, zona % 3, (uint32_t)s_reloj); }
EXPORT(sim_zona_actual) int sim_zona_actual(void) { return ui_zona(); }
EXPORT(sim_lw_dbg) int sim_lw_dbg(int q) { extern int ui_lw_dbg(int); return ui_lw_dbg(q); }
EXPORT(sim_hit_dbg) int sim_hit_dbg(int x, int y) { extern int ui_hit_dbg(int, int); return ui_hit_dbg(x, y); }
EXPORT(sim_rito2) void sim_rito2(void) { v39_jugando(); ui_demo_ceremonia(1, 0); }
EXPORT(sim_verso) void sim_verso(void) /* el último verso de verdad: Guardiana desde ayer, nuestra canción, de noche en la cueva */
{
    v39_jugando();
    const uint32_t t = (uint32_t)s_reloj;
    s_pet.guardiana = 1;
    s_pet.dia_guardiana = (int32_t)(((int64_t)t + s_pet.zona_horaria) / 86400) - 1;
    if (!s_pet.canciones[0].n) {
        const uint8_t cn[6] = {PET_CANCION_NOTA(0, 2), PET_CANCION_NOTA(1, 2), PET_CANCION_NOTA(2, 2), PET_CANCION_NOTA(5, 2), PET_CANCION_NOTA(1, 2), PET_CANCION_NOTA(0, 4)};
        pet_song_save(&s_pet, 0, cn, 6, "Nuestra canción", t);
    }
    s_pet.escenas_abiertas |= 1u << ESC_CUEVA;
    s_pet.escena = ESC_CUEVA;
    s_pet.final_estado &= (uint8_t)~PET_FINAL_SECRETO;
    s_pet.final_pend = PET_FINAL_SECRETO;
    log_linea(t, "Prisma propone terminar el verso de Peumo", 0, 0);
}
EXPORT(sim_verso_ver) void sim_verso_ver(int pag) { ui_demo_ceremonia(2, pag); }
EXPORT(sim_tilde_hoy) void sim_tilde_hoy(void) { s_pet.tilde_prox = 1; s_pet.tilde_dia = 0; s_pet.tilde_hasta = 0; }
EXPORT(sim_dragon) void sim_dragon(void) { ui_demo_dragon(); }
void ui_demo_fiesta(int tipo);
EXPORT(sim_fiesta) void sim_fiesta(int tipo) { if (!s_pet.vecinos_vistos) s_pet.vecinos_vistos = 0x31u; ui_demo_fiesta(tipo); }
EXPORT(sim_anillo) void sim_anillo(void) { ui_demo_anillo(); }
EXPORT(sim_cometa) void sim_cometa(void) { ui_demo_cometa(); }
EXPORT(sim_creature_known) void sim_creature_known(int c) { if ((unsigned)c < CRI_COUNT) { s_pet.criaturas_vistas |= (uint8_t)(1u << c); s_pet.historias[c] = 7; } }
EXPORT(sim_hint) void sim_hint(void) { s_pet.pista_prox = 1; s_pet.escenas_abiertas |= 0x3E; }
EXPORT(sim_sick) void sim_sick(int on) { s_pet.enferma = (uint8_t)(on != 0); }
EXPORT(sim_wear) void sim_wear(int ranura, int ropa1) { if (ranura >= 0 && ranura < RANURA_COUNT) { if (ranura == RANURA_MANO) s_pet.ropa_mano = (uint8_t)ropa1; else s_pet.ropa_puesta[ranura] = (uint8_t)ropa1; if (ropa1) s_pet.ropa_tiene |= (uint16_t)(1u << (ropa1 - 1)); } }
EXPORT(sim_object_use) int sim_object_use(int id) { return pet_object_use(&s_pet, id, (uint32_t)s_reloj); }
static char s_chk[4096];
EXPORT(sim_test_pos) void sim_test_pos(int x, int y) { ui_test_pos(x, y); }
EXPORT(sim_test_panel) int sim_test_panel(int k) { return ui_test_panel(k); }
EXPORT(sim_test_panel_name) const char *sim_test_panel_name(int k) { return ui_test_panel_name(k); }
EXPORT(sim_check_panel) int sim_check_panel(void) { return ui_check_panel(s_chk, sizeof s_chk); }
EXPORT(sim_check_text) const char *sim_check_text(void) { return s_chk; }
EXPORT(sim_sky) void sim_sky(void) { ui_demo_sky(&s_pet); }
EXPORT(sim_rare) void sim_rare(int r)
{
    uint32_t t = (uint32_t)s_reloj;
    s_pet.raro = (uint8_t)r;
    s_pet.raro_hasta = t + 600;
    s_pet.ultimo_raro = t;
    pet_event_t e = {EV_RARO, (int8_t)r, 1, 0, t};
    describir(&e);
    ui_show_event(&s_pet, &e);
}
EXPORT(sim_treasure) void sim_treasure(void)
{
    uint32_t t = (uint32_t)s_reloj;
    if (s_pet.dormida || s_pet.etapa == STAGE_HUEVO) return;
    s_pet.actividad = ACTV_OLFATEAR;
    s_pet.tesoro_pendiente = 1;
    s_pet.actividad_hasta = t + 2;
    pet_event_t e = {EV_TESORO, 0, -1, 0, t};
    describir(&e);
    ui_show_event(&s_pet, &e);
}
EXPORT(sim_bond_level) int sim_bond_level(void) { return pet_bond_level(&s_pet); }
EXPORT(sim_name_bond) const char *sim_name_bond(int v) { return pet_bond_name((vinculo_nivel_t)v); }
EXPORT(sim_skill) float sim_skill(int h) { return h >= 0 && h < HAB_COUNT ? s_pet.habilidad[h] : 0; }
EXPORT(sim_stickers) int sim_stickers(void) { return s_pet.estampitas; }
EXPORT(sim_bond_add) void sim_bond_add(void) { s_pet.vinculo = s_pet.vinculo + 25 > 100 ? 100 : s_pet.vinculo + 25; }
EXPORT(sim_neighbor_name) const char *sim_neighbor_name(void) { return s_pet.vecino.nombre; }
/* semillas de regalo para probar: de todos los tipos, de a una */
EXPORT(sim_give_seeds) void sim_give_seeds(int n)
{
    for (int i = 0; i < n; i++) {
        int c = i % CULTIVO_COUNT;
        if (s_pet.semillas[c] < 60) s_pet.semillas[c]++;
        s_pet.semillas_vistas |= (uint8_t)(1u << c);
    }
    int k = 0;
    for (int c = 0; c < CULTIVO_COUNT; c++) k += s_pet.semillas[c];
    s_pet.inventario[ITEM_SEMILLA] = (uint8_t)(k > 255 ? 255 : k);
}
EXPORT(sim_spawn) void sim_spawn(int f)
{
    if (f < 0 || f >= FAUNA_COUNT) return;
    uint32_t t = (uint32_t)s_reloj;
    s_pet.fauna_hasta[f] = t + 1800;
    if (f == FAUNA_ZORRO && !s_pet.modo_nino) {
        s_pet.zorro_roba = t + 180;
        if (!s_pet.inventario[ITEM_FRUTA]) s_pet.inventario[ITEM_FRUTA] = 1;
    }
}
EXPORT(sim_name_scene) const char *sim_name_scene(int e) { return pet_scene_name((escena_t)e); }
EXPORT(sim_scene_open) int sim_scene_open(int e) { return pet_scene_open(&s_pet, (escena_t)e); }
EXPORT(sim_fauna) int sim_fauna(int f) { return pet_fauna_here(&s_pet, (fauna_t)f, (uint32_t)s_reloj); }
EXPORT(sim_name_fauna) const char *sim_name_fauna(int f) { return pet_fauna_name((fauna_t)f); }
EXPORT(sim_plants) int sim_plants(void)
{
    int n = 0;
    for (int i = 0; i < PET_PARCELAS; i++) n += s_pet.jardin[i].tipo != PLANTA_NADA;
    return n;
}

EXPORT(sim_pop_sound) int sim_pop_sound(void)
{
    if (s_snd_ini == s_snd_fin) return -1;
    int s = s_snd[s_snd_ini];
    s_snd_ini = (s_snd_ini + 1) % 32;
    return s;
}

EXPORT(sim_log_ptr) char *sim_log_ptr(void) { return s_log; }
EXPORT(sim_log_len) int sim_log_len(void) { return s_log_len; }
EXPORT(sim_log_clear) void sim_log_clear(void) { s_log_len = 0; }

EXPORT(sim_pet_ptr) void *sim_pet_ptr(void) { return &s_pet; }

/* Respaldo: el mismo texto que entrega el reloj por USB */
static char s_resp_txt[PET_RESPALDO_MAX + 256];
EXPORT(sim_backup_export) const char *sim_backup_export(void)
{
    pet_mark_backup(&s_pet, (uint32_t)s_reloj);
    return pet_backup_export(&s_pet, s_resp_txt, sizeof s_resp_txt) > 0 ? s_resp_txt : "";
}
EXPORT(sim_backup_buf) char *sim_backup_buf(void) { return s_resp_txt; }
EXPORT(sim_backup_buf_size) int sim_backup_buf_size(void) { return (int)sizeof s_resp_txt; }
/* El texto ya está en sim_backup_buf: si sirve, la pantalla pregunta si traerla */
EXPORT(sim_backup_offer) int sim_backup_offer(void)
{
    if (!pet_backup_import(s_resp_txt, &s_respaldo)) {
        log_linea((uint32_t)s_reloj, "ese archivo no es un respaldo valido", 0, 0);
        return 0;
    }
    s_hay_respaldo = 1;
    lv_display_trigger_activity(s_disp);
    ui_ask_restore(&s_respaldo);
    log_linea((uint32_t)s_reloj, "respaldo de %s: confirma en la pantalla", s_respaldo.nombre, 0);
    return 1;
}
/* Como si se hubiera quedado sin batería: el reloj vuelve a una hora vieja */
EXPORT(sim_lose_clock) void sim_lose_clock(void)
{
    s_reloj = (double)s_pet.ultima_actualizacion - 40.0 * 86400.0;
    ui_set_clock_doubtful(true);
    lv_display_trigger_activity(s_disp);
    log_linea((uint32_t)s_reloj, "se quedo sin bateria y perdio la hora", 0, 0);
}
EXPORT(sim_mode) int sim_mode(void) { return (int)pet_mode(&s_pet); }
EXPORT(sim_pet_size) int sim_pet_size(void) { return (int)sizeof s_pet; }
EXPORT(sim_seal) void sim_seal(void) { pet_seal(&s_pet); }

/* estado para el panel lateral */
EXPORT(sim_need) float sim_need(int i) { return pet_need(&s_pet, (need_t)i); }
EXPORT(sim_need_set) void sim_need_set(int i, float v) { if ((unsigned)i < NEED_COUNT) s_pet.necesidad[i] = v; } /* (fw 104, pruebas) */
EXPORT(sim_trait) float sim_trait(int i) { return i >= 0 && i < TRAIT_COUNT ? s_pet.rasgo[i] : 0; }
EXPORT(sim_mood) int sim_mood(void) { return pet_mood(&s_pet); }
EXPORT(sim_want) int sim_want(void) { return pet_want(&s_pet); }
EXPORT(sim_species) int sim_species(void) { return s_pet.especie; }
EXPORT(sim_stage) int sim_stage(void) { return s_pet.etapa; }
EXPORT(sim_age) float sim_age(void) { return pet_age_days(&s_pet, (uint32_t)s_reloj); }
EXPORT(sim_flags) int sim_flags(void)
{
    return (s_pet.dormida ? 1 : 0) | (s_pet.enferma ? 2 : 0) | (s_pet.luz_apagada ? 4 : 0) |
           (s_pet.visita_actual >= 0 ? 8 : 0) | (s_pet.etapa == STAGE_HUEVO ? 16 : 0);
}
EXPORT(sim_name) const char *sim_name(void) { return s_pet.nombre; }
EXPORT(sim_hatch_left) int sim_hatch_left(void) { return (int)pet_hatch_remaining(&s_pet, (uint32_t)s_reloj); }
/* para las pruebas automáticas: salta la bienvenida (fecha, edad, aspecto y nombre) y la hace nacer */
EXPORT(sim_quick) void sim_quick(int edad)
{
    uint32_t t = (uint32_t)s_reloj;
    int a, m, d;
    pet_local_date(&s_pet, t, &a, &m, &d);
    pet_set_owner_birthdate(&s_pet, d == 1 ? 2 : 1, m == 1 ? 2 : 1, a - (edad > 0 ? edad : 30) - 1, t);
    s_pet.aspecto_ok = 2; /* huevo y color ya elegidos */
    if (!s_pet.pais) pet_set_country(&s_pet, PAIS_CHILE); /* (el país, como si lo hubieran elegido) */
    pet_set_name(&s_pet, "Kiwi");
    s_pet.eclosion = t + 1;
    ui_set_clock_doubtful(false);
}
EXPORT(sim_close_panel) void sim_close_panel(void) { ui_close_panel(); }
/* (1.0.10, pruebas) como sim_quick pero con el huevo SIN elegir y sin nombre: el primer inicio de verdad desde la elección del huevo */
EXPORT(sim_huevo_elegido) void sim_huevo_elegido(int e) { ui_test_huevo_elegido(e); }
EXPORT(sim_quick_huevo) void sim_quick_huevo(int edad) { sim_quick(edad); s_pet.aspecto_ok = 0; s_pet.nombre[0] = 0; s_pet.eclosion = (uint32_t)s_reloj + 1; }
EXPORT(sim_trip) void sim_trip(int tipo) { ui_demo_trip(&s_pet, tipo); }
EXPORT(sim_reserve_preview) void sim_reserve_preview(void) { ui_show_reserve(); }
/* la alarma i suena en un minuto (una sola vez) */
EXPORT(sim_alarm_soon) void sim_alarm_soon(int i)
{
    int64_t l = (int64_t)s_reloj + s_pet.zona_horaria + 60;
    int min = (int)((l % 86400 + 86400) % 86400 / 60);
    if (i == 0) { /* la hora de dormir en un minuto (y levantarse 8 horas después) */
        int dmin = (min + 8 * 60) % 1440;
        pet_bedtime_set(&s_pet, DORMIR_FIJA, min / 60, min % 60, dmin / 60, dmin % 60);
        log_linea((uint32_t)s_reloj, "la hora de dormir es en un minuto", 0, 0);
        return;
    }
    int libre = PET_ALARMAS - 1;
    for (int k = 0; k < PET_ALARMAS; k++) if (!s_pet.alarma[k].usada) { libre = k; break; }
    pet_alarm_set(&s_pet, libre, true, min / 60, min % 60, 0, 0, false);
    log_linea((uint32_t)s_reloj, "el despertador sonara en un minuto", 0, 0);
}
/* --- lo que hiciste en el editor de Windows (un paquete .mascotapack): la página lo copia a este búfer --- */
static uint8_t s_paquete[ART_CUERPO_BYTES + 64];
EXPORT(sim_pack_buf) uint8_t *sim_pack_buf(void) { return s_paquete; }
EXPORT(sim_pack_buf_size) int sim_pack_buf_size(void) { return (int)sizeof s_paquete; }
/* un ícono: 24x24 RGBA en el búfer, o 12x12 del editor antiguo (n = 0: el de fábrica) */
EXPORT(sim_pack_icon) int sim_pack_icon(int id, int n)
{
    if (id < 0 || id >= ICO_COUNT) return 0;
    if (!n) { art_icono_propio((art_icono_t)id, NULL, 0); return 1; }
    int lado = art_icono_lado_de_bytes(n);
    if (!lado) return 0;
    static uint32_t px[ART_ICONO_LADO * ART_ICONO_LADO];
    for (int i = 0; i < lado * lado; i++) {
        const uint8_t *q = s_paquete + i * 4;
        px[i] = q[3] < 128 ? 0 : (0xFF000000u | (uint32_t)q[0] << 16 | (uint32_t)q[1] << 8 | q[2]);
    }
    art_icono_propio((art_icono_t)id, px, lado);
    lv_obj_invalidate(lv_screen_active());
    return 1;
}
/* un cuerpo de criatura (formato CUER, n = 0: el de fábrica) */
EXPORT(sim_pack_body) int sim_pack_body(int e, int et, int n)
{
    static art_cuerpo_t c;
    if (!n) { art_criatura_propia(e, et, NULL); return 1; }
    if (!art_cuerpo_desde_bytes(s_paquete, n, &c)) return 0;
    art_criatura_propia(e, et, &c);
    lv_obj_invalidate(lv_screen_active());
    return 1;
}
/* el nombre de una especie (texto con cero final en el búfer; "" = el de fábrica) */
EXPORT(sim_pack_species_name) void sim_pack_species_name(int e)
{
    s_paquete[PET_NAME_LEN - 1] = 0;
    pet_species_set_name((uint8_t)e, (const char *)s_paquete);
}
/* las celebraciones (formato FIES) */
EXPORT(sim_pack_celebrations) int sim_pack_celebrations(int n) { return n ? pet_celebrations_unpack(s_paquete, n) : (pet_celebrations_default(), 1); }
EXPORT(sim_aspecto) int sim_aspecto(void) { return s_pet.aspecto_ok | s_pet.etapa << 4 | (s_pet.edad_dueno ? 256 : 0); }
EXPORT(sim_egg_again) void sim_egg_again(void) { pet_rebirth(&s_pet, s_pet.especie, (uint32_t)s_reloj, 7); s_pet.aspecto_ok = 0; } /* (pruebas) volver a elegir el huevo */
EXPORT(sim_hatch) void sim_hatch(void) { if (pet_is_egg(&s_pet)) s_pet.eclosion = (uint32_t)s_reloj + 1; }
EXPORT(sim_season_now) int sim_season_now(void) { return pet_season(&s_pet, (uint32_t)s_reloj); }
EXPORT(sim_weather_now) int sim_weather_now(void) { return pet_weather(&s_pet); }
EXPORT(sim_set_season) void sim_set_season(int e) { s_pet.estacion_fija = (uint8_t)(e < 0 || e > 3 ? 0xFF : e); }
EXPORT(sim_set_weather) void sim_set_weather(int c) { pet_force_weather(&s_pet, (clima_t)c, (uint32_t)s_reloj, 2u * 3600u); }
EXPORT(sim_name_season) const char *sim_name_season(int e) { return pet_season_name((estacion_t)e); }
EXPORT(sim_name_weather) const char *sim_name_weather(int c) { return pet_weather_name((clima_t)c); }
EXPORT(sim_phrase_now) void sim_phrase_now(void) { s_pet.ultima_frase_slot -= 1; } /* fuerza el próximo mensaje */
EXPORT(sim_feeling) const char *sim_feeling(void)
{
    static char buf[128];
    pet_feeling(&s_pet, buf, sizeof buf);
    return buf;
}

/* amigos: amigo de prueba y balizas entre pestañas */
EXPORT(sim_bot) void sim_bot(int on) { s_bot_on = on; if (on) log_linea((uint32_t)s_reloj, "%s (amigo de prueba) esta cerca", s_bot.nombre, 0); }
EXPORT(sim_bot_invite) void sim_bot_invite(void)
{
    if (!s_bot_on) return;
    for (int i = 0; i < PET_MAX_AMIGOS; i++)
        if (s_bot.amigos[i].id == s_pet.id) { pet_invite(&s_bot, i, (uint32_t)s_reloj); return; }
}
EXPORT(sim_beacon_out) uint8_t *sim_beacon_out(void)
{
    pet_baliza_t b;
    pet_make_baliza(&s_pet, (uint32_t)s_reloj, &b);
    pet_baliza_encode(&b, s_baliza_out);
    return s_baliza_out;
}
EXPORT(sim_beacon_in_ptr) uint8_t *sim_beacon_in_ptr(void) { return s_baliza_in; }
EXPORT(sim_beacon_in) void sim_beacon_in(void)
{
    pet_baliza_t b;
    if (pet_baliza_decode(s_baliza_in, sizeof s_baliza_in, &b)) pet_on_baliza(&s_pet, &b, (uint32_t)s_reloj);
}
EXPORT(sim_beacon_size) int sim_beacon_size(void) { return PET_BALIZA_BYTES; }
EXPORT(sim_habit) int sim_habit(int kind, int hora) { return s_pet.habitos.prob[kind & 3][hora % 24]; }
EXPORT(sim_name_mood) const char *sim_name_mood(int m) { return pet_mood_name((mood_t)m); }
EXPORT(sim_name_want) const char *sim_name_want(int w) { return pet_want_name((want_t)w); }
EXPORT(sim_name_species) const char *sim_name_species(int e) { return pet_species_name((uint8_t)e); }
EXPORT(sim_name_stage) const char *sim_name_stage(int s) { return pet_stage_name((stage_t)s); }
EXPORT(sim_name_sound) const char *sim_name_sound(int s) { return pet_sound_name((sound_t)s); }
EXPORT(sim_force) void sim_force(int esp, int color, int etapa, int forma) { s_pet.especie = (uint8_t)esp; s_pet.color = (uint8_t)color; s_pet.etapa = (stage_t)etapa; s_pet.forma = (uint8_t)(forma == 9 ? 0 : forma); s_pet.evo_pregunta = forma == 9; s_pet.evo_pend = 0; s_pet.evo_decidido = 0; s_pet.nacimiento = (uint32_t)s_reloj - (uint32_t)(etapa == 2 ? 2 : (etapa == 3 ? 6 : 0)) * 86400u - 100u; }

/* prueba: duerme a la mascota y deja que un amigo pase de visita */
EXPORT(sim_dormida_visita) void sim_dormida_visita(void) { s_pet.dormida = 1; s_pet.vecino.proxima = 0; s_pet.vecino.presente = 0; }
EXPORT(sim_set_look) void sim_set_look(int e, int c) { s_pet.especie = (uint8_t)(e & 7); s_pet.color = (uint8_t)(c & 7); s_pet.mezcla = 0; }
EXPORT(sim_set_forma) void sim_set_forma(int etapa, int forma) { s_pet.etapa = (stage_t)(etapa & 3); s_pet.forma = (uint8_t)(forma & 7); } /* (pruebas) como ASPECTO en la placa */
EXPORT(sim_ev) void sim_ev(int tipo, int a, int b) { sim_evento((event_type_t)tipo, a, b); } /* muestra un evento en la interfaz (para pruebas) */

/* ---- volcado de fondos (para sacar las imágenes limpias, sin botones ni mascota) ---- */
EXPORT(sim_fondo) const uint8_t *sim_fondo(int per, int est, int tinte, int esc, int decor, int sendas)
{
    const lv_image_dsc_t *d = art_fondo((art_periodo_t)per, (uint32_t)decor, (art_estacion_t)est, tinte, esc, (uint32_t)sendas);
    return d ? d->data : NULL; /* RGB565 de 466x466 (se copia en seguida: el siguiente pedido lo pisa) */
}
EXPORT(sim_casa_set) void sim_casa_set(int luz, int cuadro) { art_set_casa(luz != 0, cuadro, 0); }
static art_img_t s_cruce_vol;
EXPORT(sim_cruce) const uint8_t *sim_cruce(int per, int est)
{
    art_cruce_estacion(est); art_cruce_fondo(&s_cruce_vol, per);
    return s_cruce_vol.px ? (const uint8_t *)s_cruce_vol.px : NULL; /* RGB565 de 468x468 (v88; antes ARGB8888) */
}
EXPORT(sim_cruce_w) int sim_cruce_w(void) { return (int)s_cruce_vol.dsc.header.w; }

/* ---- foto del fondo de una pantalla (ARGB8888 de 466x466 con transparencia) ---- */
static lv_draw_buf_t *s_snap_vol;
EXPORT(sim_snap) const uint8_t *sim_snap(int k)
{
    lv_obj_t *o = ui_fondo_solo(k);
    if (!o) return NULL;
    if (s_snap_vol) { lv_draw_buf_destroy(s_snap_vol); s_snap_vol = NULL; }
    s_snap_vol = lv_snapshot_take(o, LV_COLOR_FORMAT_ARGB8888);
    return s_snap_vol ? s_snap_vol->data : NULL;
}
EXPORT(sim_snap_w) int sim_snap_w(void) { return s_snap_vol ? (int)s_snap_vol->header.w : 0; }
EXPORT(sim_snap_h) int sim_snap_h(void) { return s_snap_vol ? (int)s_snap_vol->header.h : 0; }
EXPORT(sim_snap_stride) int sim_snap_stride(void) { return s_snap_vol ? (int)s_snap_vol->header.stride : 0; }

/* píxeles redibujados desde el último pedido (mide cuánto trabajo da una pantalla) */
/* (v89, diagnóstico) quién invalida áreas grandes: se desvía lv_obj_invalidate y se anotan los objetos con más de 100 kpx */
void __real_lv_obj_invalidate(const lv_obj_t *obj);
static const lv_obj_t *s_inv_obj[8]; static int s_inv_n;
extern int g_dbg_fase; static int s_inv_fase[80];
EXPORT(sim_inv_fase) int sim_inv_fase(int k) { int v = s_inv_fase[k]; s_inv_fase[k] = 0; return v; }
void __wrap_lv_obj_invalidate(const lv_obj_t *obj)
{
    if (obj && lv_display_is_invalidation_enabled(lv_obj_get_display(obj)) && lv_obj_get_parent(obj) && lv_obj_get_parent(obj) == lv_screen_active() && lv_obj_get_width(obj) >= 466 && g_dbg_fase < 80) s_inv_fase[g_dbg_fase]++;
    if (obj && s_inv_n < 8 && lv_display_is_invalidation_enabled(lv_obj_get_display(obj))) {
        lv_area_t a; lv_obj_get_coords(obj, &a);
        if ((int32_t)lv_area_get_width(&a) * lv_area_get_height(&a) > 100000 && lv_obj_is_visible(obj)) {
            int k = 0; while (k < s_inv_n && s_inv_obj[k] != obj) k++;
            if (k == s_inv_n) s_inv_obj[s_inv_n++] = obj;
        }
    }
    { /* (1.0.21, diagnóstico) cuántas veces se invalida cada objeto (para ver qué transparencias se redibujan) */
        extern void alfa_contar(const lv_obj_t *o);
        alfa_contar(obj);
    }
    __real_lv_obj_invalidate(obj);
}
/* (1.0.21) las imágenes con transparencia (ARGB8888) a la vista: dónde están y cuántas veces se redibujaron */
static struct { const lv_obj_t *o; int n; } s_alfa_inv[512]; static int s_alfa_inv_n;
void alfa_contar(const lv_obj_t *o)
{
    if (!o) return;
    for (int i = 0; i < s_alfa_inv_n; i++) if (s_alfa_inv[i].o == o) { s_alfa_inv[i].n++; return; }
    if (s_alfa_inv_n < 512) { s_alfa_inv[s_alfa_inv_n].o = o; s_alfa_inv[s_alfa_inv_n].n = 1; s_alfa_inv_n++; }
}
static int alfa_veces(const lv_obj_t *o)
{
    int n = 0;
    for (const lv_obj_t *p = o; p; p = lv_obj_get_parent(p))
        for (int i = 0; i < s_alfa_inv_n; i++) if (s_alfa_inv[i].o == p) { if (p == o) n += s_alfa_inv[i].n; }
    return n;
}
static char s_alfa_txt[16384]; static int s_alfa_len;
static void alfa_recorrer(lv_obj_t *o)
{
    if (!o || lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN)) return;
    if (lv_obj_check_type(o, &lv_image_class)) {
        const void *src = lv_image_get_src(o);
        if (src && lv_image_src_get_type(src) == LV_IMAGE_SRC_VARIABLE) {
            const lv_image_dsc_t *d = src;
            lv_area_t a; lv_obj_get_coords(o, &a);
            const int w = lv_area_get_width(&a), h = lv_area_get_height(&a);
            const int opa = lv_obj_get_style_opa_recursive(o, LV_PART_MAIN);
            if (w * h >= 900 && s_alfa_len < (int)sizeof s_alfa_txt - 200)
                s_alfa_len += snprintf(s_alfa_txt + s_alfa_len, sizeof s_alfa_txt - s_alfa_len, "%d %d %d %d %d %d %d %d\n",
                                       (int)a.x1, (int)a.y1, w, h, (int)d->header.cf, alfa_veces(o), opa, (int)d->header.w * (int)d->header.h);
        }
    }
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) alfa_recorrer(lv_obj_get_child(o, (int32_t)i));
}
EXPORT(sim_alfa_limpiar) void sim_alfa_limpiar(void) { s_alfa_inv_n = 0; }
EXPORT(sim_alfa_scan) const char *sim_alfa_scan(void)
{
    s_alfa_len = 0; s_alfa_txt[0] = 0;
    alfa_recorrer(lv_screen_active());
    alfa_recorrer(lv_layer_top());
    return s_alfa_txt;
}
const char *ui_dbg_obj_nombre(const void *o);
EXPORT(sim_inv_n) int sim_inv_n(void) { return s_inv_n; }
EXPORT(sim_inv_nombre) const char *sim_inv_nombre(int k) { return k < s_inv_n ? ui_dbg_obj_nombre(s_inv_obj[k]) : ""; }
EXPORT(sim_inv_limpiar) void sim_inv_limpiar(void) { s_inv_n = 0; }
EXPORT(sim_flush_px) double sim_flush_px(void) { double v = (double)s_flush_px; s_flush_px = 0; return v; }

EXPORT(sim_fondo_rapido) void sim_fondo_rapido(int on) { art_fondo_rapido(on); }
int ui_panel_actual(void);
EXPORT(sim_panel_id) int sim_panel_id(void) { return ui_panel_actual(); }

/* ---- (105) el fondo armado por franjas (cuando no hay memoria para el lienzo entero) ----
 * sim_sin_lienzo: hace como si nunca hubiera bloque para el lienzo de 876 KB (para ver los adornos en pantalla).
 * sim_fondo_cmp: dibuja el fondo de las dos maneras y cuenta los pixeles distintos (debe dar 0). */
static int s_cmp[32][2], s_cmp_n;
EXPORT(sim_cmp_n) int sim_cmp_n(void) { return s_cmp_n; }
EXPORT(sim_cmp_x) int sim_cmp_x(int i) { return i < s_cmp_n ? s_cmp[i][0] : -1; }
EXPORT(sim_cmp_y) int sim_cmp_y(int i) { return i < s_cmp_n ? s_cmp[i][1] : -1; }
EXPORT(sim_sin_lienzo) void sim_sin_lienzo(int on) { art_fondo_sin_lienzo(on); }
int sim_fondo_cmp2(int escena, int periodo, int estacion, int nublado, int decor, int senderos, int alto);
EXPORT(sim_fondo_cmp) int sim_fondo_cmp(int escena, int periodo, int estacion, int nublado, int decor, int senderos)
{
    return sim_fondo_cmp2(escena, periodo, estacion, nublado, decor, senderos, 0);
}
/* alto: 0 el alto que quepa (como en la placa); 6 o más, franjas de ese alto */
EXPORT(sim_fondo_cmp2) int sim_fondo_cmp2(int escena, int periodo, int estacion, int nublado, int decor, int senderos, int alto)
{
    art_fondo_sin_lienzo(0);
    const lv_image_dsc_t *a = art_fondo((art_periodo_t)periodo, (uint32_t)decor, (art_estacion_t)estacion, nublado, escena, (uint32_t)senderos);
    if (!a) return -1;
    const int n = (int)a->header.w * a->header.h;
    uint16_t *copia = malloc((size_t)n * 2);
    if (!copia) return -2;
    memcpy(copia, a->data, (size_t)n * 2);
    art_fondo_sin_lienzo(alto >= 6 ? alto : 1);
    const lv_image_dsc_t *b = art_fondo((art_periodo_t)periodo, (uint32_t)decor, (art_estacion_t)estacion, nublado, escena, (uint32_t)senderos);
    int malos = b ? 0 : -3;
    s_cmp_n = 0;
    if (b) {
        const uint16_t *q = (const uint16_t *)b->data;
        for (int i = 0; i < n; i++) if (q[i] != copia[i]) { if (s_cmp_n < 32) { s_cmp[s_cmp_n][0] = i % a->header.w; s_cmp[s_cmp_n][1] = i / a->header.w; s_cmp_n++; } malos++; }
    }
    free(copia);
    art_fondo_sin_lienzo(0);
    return malos;
}

/* (106) prueba: ¿se dibuja el huevo? devuelve cuántos pixeles pinta a esa escala (0 = no se dibujó) */
EXPORT(sim_huevo_px) int sim_huevo_px(int especie, int escala, int grietas)
{
    art_img_t im = {0};
    art_huevo(&im, especie, escala, grietas, 0);
    int n = 0;
    if (im.px)
        for (int i = 0; i < (int)im.dsc.header.w * (int)im.dsc.header.h; i++)
            if (im.px[i] >> 24) n++;
    const int w = im.px ? (int)im.dsc.header.w : 0;
    art_img_free(&im);
    return n * 10000 + w;
}
