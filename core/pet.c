/*
 * Motor de la mascota virtual — implementación (versión 5).
 *
 * Organización:
 *   1. Utilidades (azar, límites, hora local, cola de eventos)
 *   2. Especies, personalidad, gustos y nacimiento
 *   3. Memoria afectiva y personalidad que cambia
 *   4. Simulación por pasos: necesidades, sueño, salud, clima, visitas, ausencia
 *   5. Autonomía: qué hace sola y dónde
 *   6. Emociones mezcladas con causa, y el estado de ánimo visible
 *   7. Acciones del dueño (y cuándo dice que no)
 *   8. Decoraciones, consultas y guardado
 *   9. Mensajes del día, cómo se siente
 *  10. Amigos cercanos
 *
 * Todas las tasas están expresadas "por hora" y se escalan por el paso.
 */
#include "pet.h"

#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ======================================================================
 * 1. Utilidades
 * ==================================================================== */

#define HORA 3600.0f
#define MAX_PONERSE_AL_DIA (14u * 24u * 3600u) /* tope: 14 días */
#define AVISO_CADA_S (20u * 60u)                /* repetir el mismo pedido cada 20 min */
#define AVISO_CAMBIO_S (5u * 60u)               /* un pedido distinto, tras 5 min */
#define UMBRAL_PEDIR 0.35f
#define AUSENCIA_S (12u * 3600u) /* desde aquí "la dejaste sola" (más que una noche): todo baja más lento */
#define REENCUENTRO_S (16u * 3600u) /* volver tras esto es un reencuentro */
#define UMBRAL_EMOCION 0.35f
#define PENSAR_CADA_S (25u * 60u) /* repetir el mismo pensamiento */

static float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

/* xorshift32: barato, determinista, suficiente para conducta */
static uint32_t rng_next(pet_t *p)
{
    uint32_t x = p->rng ? p->rng : 0x9E3779B9u;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    p->rng = x;
    return x;
}
static float rng01(pet_t *p) { return (float)(rng_next(p) >> 8) * (1.0f / 16777216.0f); }

/* Una "bolsa" sin repetición: elige al azar entre lo permitido que todavía no salió; cuando ya salió todo, empieza otra vuelta.
 * (permitidos: un bit por cosa; la bolsa se guarda en la partida) */
static int bolsa_tomar(pet_t *p, uint64_t *bolsa, int n, uint64_t permitidos)
{
    const uint64_t todos = n >= 64 ? ~0ull : ((1ull << n) - 1ull);
    permitidos &= todos;
    if (!permitidos) return -1;
    uint64_t disp = permitidos & ~*bolsa;
    if (!disp) { *bolsa = 0; disp = permitidos; } /* salió todo: otra vuelta */
    int cuantos = 0;
    for (uint64_t d = disp; d; d &= d - 1) cuantos++;
    int k = (int)(rng_next(p) % (uint32_t)cuantos), idx = 0;
    for (uint64_t d = disp;; d &= d - 1) {
        idx = 0;
        while (!((d >> idx) & 1ull)) idx++;
        if (!k--) break;
    }
    *bolsa |= 1ull << idx;
    return idx;
}
static bool chance(pet_t *p, float prob) { return rng01(p) < prob; }

/* probabilidad por hora -> probabilidad en un paso de dt segundos */
static float prob_paso(float por_hora, float dt) { return 1.0f - powf(1.0f - clampf(por_hora, 0, 1), dt / HORA); }
/* decaimiento exponencial con vida media en horas */
static float decae(float v, float h, float vida_media_h) { return v * powf(0.5f, h / vida_media_h); }

static int hora_local(const pet_t *p, uint32_t t)
{
    int64_t local = (int64_t)t + p->zona_horaria;
    int64_t s = local % 86400;
    if (s < 0) s += 86400;
    return (int)(s / 3600);
}
static bool es_noche(const pet_t *p, uint32_t t)
{
    int h = hora_local(p, t);
    return h >= 22 || h < 7;
}
/* su propio reloj: con ritmo 1 es tu hora; más rápido, su día dura menos y puede dormir a las 3 de la tarde */
float pet_rhythm_factor(const pet_t *p) { static const float K[RITMO_COUNT] = {1.0f, 1.5f, 3.0f}; return K[p->ritmo < RITMO_COUNT ? p->ritmo : 0]; }
int pet_rhythm(const pet_t *p) { return p->ritmo < RITMO_COUNT ? p->ritmo : 0; }
void pet_set_rhythm(pet_t *p, int ritmo) { p->ritmo = (uint8_t)(ritmo < 0 || ritmo >= RITMO_COUNT ? 0 : ritmo); p->ritmo_visto = 1; }
const char *pet_rhythm_name(int r) { static const char *const N[RITMO_COUNT] = {"Tranquilo", "Equilibrado", "Rápido"}; return r >= 0 && r < RITMO_COUNT ? N[r] : "?"; }
int pet_pet_hour(const pet_t *p, uint32_t t)
{
    const float k = pet_rhythm_factor(p);
    if (k <= 1.0f || t <= p->nacimiento) return hora_local(p, t);
    const int64_t nac = (int64_t)p->nacimiento + p->zona_horaria;
    const double s = fmod((double)(((nac % 86400) + 86400) % 86400) + (double)(t - p->nacimiento) * (double)k, 86400.0);
    return (int)(s / 3600.0);
}
static bool es_noche_su(const pet_t *p, uint32_t t)
{
    int h = pet_pet_hour(p, t);
    return h >= 22 || h < 7;
}
static int32_t dia_local(const pet_t *p, uint32_t t)
{
    int64_t local = (int64_t)t + p->zona_horaria;
    return (int32_t)(local >= 0 ? local / 86400 : (local - 86399) / 86400);
}

static uint32_t mezcla32(uint32_t h);
static void push_event(pet_t *p, event_type_t type, int a, int b, uint32_t t)
{
    uint8_t next = (uint8_t)((p->ev_fin + 1) % PET_EVENT_QUEUE);
    if (next == p->ev_ini) /* cola llena: se descarta el más antiguo */
        p->ev_ini = (uint8_t)((p->ev_ini + 1) % PET_EVENT_QUEUE);
    pet_event_t *e = &p->eventos[p->ev_fin];
    e->type = (uint8_t)type;
    e->a = (int8_t)a;
    e->b = (int8_t)b;
    e->_pad = 0;
    e->t = t;
    p->ev_fin = next;
}

bool pet_poll_event(pet_t *p, pet_event_t *out)
{
    if (p->ev_ini == p->ev_fin) return false;
    *out = p->eventos[p->ev_ini];
    p->ev_ini = (uint8_t)((p->ev_ini + 1) % PET_EVENT_QUEUE);
    return true;
}

static void sonar(pet_t *p, sound_t s, uint32_t t) { push_event(p, EV_SONIDO, s, 0, t); }

/* Lo que hace sola la entretiene, pero no la llena: comer bien, estar limpia, sentirse querida y
 * divertirse de verdad dependen de ti. Mientras el motor avanza solo (pet_update), estas necesidades
 * suben como mucho hasta un techo; lo que haces tú las llena del todo. */
static bool s_por_su_cuenta;
#define TECHO_SOLA 55.0f
#define TECHO_SOLA_NINO 65.0f
#define TECHO_CURIOSIDAD 75.0f

static void sumar(pet_t *p, need_t n, float delta)
{
    if (s_por_su_cuenta && delta > 0 &&
        (n == NEED_SACIEDAD || n == NEED_HIGIENE || n == NEED_CARINO || n == NEED_DIVERSION || n == NEED_CURIOSIDAD)) {
        float techo = n == NEED_CURIOSIDAD ? TECHO_CURIOSIDAD : (p->modo_nino ? TECHO_SOLA_NINO : TECHO_SOLA);
        if (p->necesidad[n] >= techo) return;
        if (p->necesidad[n] + delta > techo) delta = techo - p->necesidad[n];
    }
    p->necesidad[n] = clampf(p->necesidad[n] + delta, 0.0f, 100.0f);
}

/* un empuje emocional por algo que pasó: se queda con el más fuerte */
static void impulsar(pet_t *p, emo_t e, float fuerza, causa_t c)
{
    if (p->modo_nino && (e == EMO_ENOJO || e == EMO_CELOS || e == EMO_TRISTEZA)) fuerza *= 0.6f; /* suave con los niños */
    if (fuerza >= p->impulso[e] * 0.8f) p->impulso_causa[e] = (uint8_t)c;
    p->impulso[e] = clampf(fuerza > p->impulso[e] ? fuerza : p->impulso[e] + fuerza * 0.3f, 0, 1);
}

#define DIA_ANIMAL_FRASES 20
static void anotar(pet_t *p, diario_t tipo, int a, int b, uint32_t t);
static void paso_historia(pet_t *p, uint32_t t);
static void efectos_v38(pet_t *p, int id, uint32_t t);
static void guardar_material(pet_t *p, int hallazgo);
static bool momento_libre(pet_t *p, uint32_t t);
static void momento_usar(pet_t *p, uint32_t t);
static bool historia_abierta(const pet_t *p, int k);
static void tilo_pagina(pet_t *p, int k, uint32_t t);
static bool animal_cuenta(pet_t *p, int f, uint32_t t);
static void programar_carta(pet_t *p, uint32_t t);
#define NOMBRE(tabla, i, n) ((unsigned)(i) < (n) && (unsigned)(i) < sizeof(tabla) / sizeof((tabla)[0]) ? (tabla)[i] : "?")
static const uint8_t TAM_COLECCION[COL_COUNT] = {5, 4, 4, 4, 4}; /* piezas de cada colección (fw 104: + el trocito de hielo en cristales) */
#define CRISTALES_AZAR 3 /* los cristales que salen escarbando o en la cita: los 3 de la cueva (el hielo solo se pesca en invierno) */
static void foto(pet_t *p, diario_t motivo, int a, uint32_t t);
static void vivir(pet_t *p, vivido_t v);
static bool guardar_pieza(pet_t *p, coleccion_t c, int pieza, uint32_t t, bool vivo);
static void desenterrar(pet_t *p, uint32_t t, bool vivo);
static bool oler_tesoro(pet_t *p, uint32_t t, bool vivo);
static void fin_habilidad(pet_t *p, habilidad_t h, uint32_t t, bool vivo);
static void empezar_habilidad(pet_t *p, habilidad_t h, uint32_t t, bool vivo);
static bool puede_trepar(const pet_t *p);
static int practicas_hoy(pet_t *p, habilidad_t h, uint32_t t);
static void empezar_raro(pet_t *p, raro_t r, uint32_t t, uint32_t dur, bool vivo);
static void meta_avanza(pet_t *p, meta_t m, uint32_t t);
static void paso_recuerdos(pet_t *p, uint32_t t, float dt, bool vivo);
static bool aprende_de(int a);
static uint8_t dia_byte(const pet_t *p, uint32_t t);
static bool quizas_objeto(pet_t *p, objeto_fuente_t f, float prob, uint32_t t);
static int linaje_de_objeto(int id);
static void linaje_suma(pet_t *p, int l, int puntos);
static void paso_objetos(pet_t *p, uint32_t t);
static void paso_cacas(pet_t *p);
static void adaptarse(pet_t *p, action_t a, uint32_t t);
static void paso_largo(pet_t *p, uint32_t t, bool vivo);
static void regalo_al_volver(pet_t *p, uint32_t ausente, uint32_t t);
static void paso_niebla(pet_t *p, uint32_t t, bool vivo);
static void gustos_creativos(pet_t *p);
static int salpicar(pet_t *p, int charco, uint32_t t, bool vivo, bool invitada);
static bool ropa_abriga(const pet_t *p);
static bool lleva(const pet_t *p, ropa_t r);
static int vestir(pet_t *p, int arg, uint32_t t);
static int poner_juguete(pet_t *p, int arg, uint32_t t);
static int usar_juguete(pet_t *p, int arg, uint32_t t);
static void fin_juguete(pet_t *p, uint32_t t, bool vivo);
static int juguete_preferido(pet_t *p, float *u);
static void empezar_juguete(pet_t *p, juguete_t j, uint32_t t, uint32_t dur, bool vivo);
static int dias_mes(int m, int a);

/* --- semillas: cada una es de un tipo; inventario[ITEM_SEMILLA] es la suma --- */
static void contar_semillas(pet_t *p)
{
    int n = 0;
    for (int c = 0; c < CULTIVO_COUNT; c++) n += p->semillas[c];
    p->inventario[ITEM_SEMILLA] = (uint8_t)(n > 255 ? 255 : n);
}

/* qué semilla encuentra: depende de la zona, la estación y de cuánto tiempo lleva (los tipos se van descubriendo) */
static cultivo_t tipo_de_semilla(pet_t *p, uint32_t t)
{
    float dias = pet_age_days(p, t);
    estacion_t e = pet_season(p, t);
    float w[CULTIVO_COUNT] = {3.0f, 0.0f, 0.0f, 0.0f};
    switch (p->escena) {
    case ESC_BOSQUE: w[CULTIVO_FLOR] = 0.6f; w[CULTIVO_HIERBA] = 2.0f; w[CULTIVO_FRUTILLA] = 1.4f; break;
    case ESC_LAGO: w[CULTIVO_FLOR] = 2.0f; w[CULTIVO_HIERBA] = 1.0f; break;
    case ESC_COLINA: w[CULTIVO_FLOR] = 1.4f; w[CULTIVO_GIRASOL] = 2.0f; break;
    case ESC_CUEVA: w[CULTIVO_FLOR] = 0.5f; w[CULTIVO_HIERBA] = 1.0f; break;
    default: w[CULTIVO_GIRASOL] = 0.8f; w[CULTIVO_HIERBA] = 0.4f; break;
    }
    if (e == ESTACION_VERANO || e == ESTACION_PRIMAVERA) w[CULTIVO_GIRASOL] *= 1.5f;
    if (e == ESTACION_PRIMAVERA) w[CULTIVO_FRUTILLA] *= 1.4f;
    /* con el tiempo: el girasol desde el segundo día, la frutilla desde el tercero, la hierba desde el primero */
    if (dias < 1.0f) w[CULTIVO_HIERBA] = 0;
    if (dias < 2.0f) w[CULTIVO_GIRASOL] = 0;
    if (dias < 3.0f) w[CULTIVO_FRUTILLA] = 0;
    float tot = 0;
    for (int c = 0; c < CULTIVO_COUNT; c++) tot += w[c];
    if (tot <= 0) return CULTIVO_FLOR;
    float r = rng01(p) * tot;
    for (int c = 0; c < CULTIVO_COUNT; c++) { if (r < w[c]) return (cultivo_t)c; r -= w[c]; }
    return CULTIVO_FLOR;
}

static void ganar_semilla(pet_t *p, int tipo, uint32_t t)
{
    if (tipo < 0 || tipo >= CULTIVO_COUNT) tipo = (int)tipo_de_semilla(p, t);
    if (p->semillas[tipo] < 60) p->semillas[tipo]++;
    bool nueva = !((p->semillas_vistas >> tipo) & 1u);
    p->semillas_vistas |= (uint8_t)(1u << tipo);
    contar_semillas(p);
    push_event(p, EV_SEMILLA, tipo, nueva ? 1 : 0, t);
    if (nueva && tipo != CULTIVO_FLOR) {
        anotar(p, DIA_SEMILLA, tipo, 0, t);
        sonar(p, SND_SEMILLA, t);
    }
}

/* gasta una semilla de ese tipo (-1: de la que más tiene). false si no hay. */
static bool gastar_semilla(pet_t *p, int tipo)
{
    if (tipo < 0) {
        tipo = 0;
        for (int c = 1; c < CULTIVO_COUNT; c++) if (p->semillas[c] > p->semillas[tipo]) tipo = c;
    }
    if (tipo >= CULTIVO_COUNT || !p->semillas[tipo]) return false;
    p->semillas[tipo]--;
    contar_semillas(p);
    return true;
}

static void guardar_item(pet_t *p, item_t it)
{
    if (it == ITEM_SEMILLA) { ganar_semilla(p, -1, p->ultima_actualizacion); return; }
    if ((unsigned)it < ITEM_COUNT && p->inventario[it] < 99) p->inventario[it]++;
}

int pet_seeds(const pet_t *p, cultivo_t c) { return (unsigned)c < CULTIVO_COUNT ? p->semillas[c] : 0; }
bool pet_seed_known(const pet_t *p, cultivo_t c) { return (unsigned)c < CULTIVO_COUNT && ((p->semillas_vistas >> c) & 1u); }

/* --- jardines --- */
static planta_t *parcela(pet_t *p, int i)
{
    if (i < 0 || i >= PET_PARCELAS_TOTAL) return NULL;
    return i < PET_PARCELAS ? &p->jardin[i] : &p->jardin_zona[i - PET_PARCELAS];
}
const planta_t *pet_plant(const pet_t *p, int i) { return parcela((pet_t *)p, i); }
escena_t pet_plant_scene(int i)
{
    if (i < PET_PARCELAS) return ESC_PRADO;
    static const escena_t Z[3] = {ESC_BOSQUE, ESC_LAGO, ESC_COLINA};
    int k = (i - PET_PARCELAS) / PET_PARCELAS_ZONA;
    return k >= 0 && k < 3 ? Z[k] : ESC_PRADO;
}
int pet_plant_first(escena_t e, int *n)
{
    int ini = -1, c = 0;
    switch (e) {
    case ESC_PRADO: ini = 0; c = PET_PARCELAS; break;
    case ESC_BOSQUE: ini = PET_PARCELAS; c = PET_PARCELAS_ZONA; break;
    case ESC_LAGO: ini = PET_PARCELAS + PET_PARCELAS_ZONA; c = PET_PARCELAS_ZONA; break;
    case ESC_COLINA: ini = PET_PARCELAS + 2 * PET_PARCELAS_ZONA; c = PET_PARCELAS_ZONA; break;
    default: break;
    }
    if (n) *n = c;
    return ini;
}

/* ======================================================================
 * 2. Especies, personalidad, gustos y nacimiento
 * ==================================================================== */

static const char *const ESPECIES[] = {
    "Brotito", "Chispa", "Cervato", "Burbu", "Caelum", "Astro", "Lumi", "Brumo",
};
#define N_ESPECIES (sizeof(ESPECIES) / sizeof(ESPECIES[0]))

static char s_especie_propia[N_ESPECIES][PET_NAME_LEN]; /* nombres que se pusieron con el editor */
uint8_t pet_species_count(void) { return (uint8_t)N_ESPECIES; }
const char *pet_species_name(uint8_t e) { return e < N_ESPECIES ? (s_especie_propia[e][0] ? s_especie_propia[e] : ESPECIES[e]) : "?"; }
const char *pet_species_factory_name(uint8_t e) { return e < N_ESPECIES ? ESPECIES[e] : "?"; }
void pet_species_set_name(uint8_t e, const char *nombre)
{
    if (e >= N_ESPECIES) return;
    s_especie_propia[e][0] = 0;
    if (!nombre) return;
    /* hasta PET_NAME_LEN - 1 bytes sin cortar una letra por la mitad */
    int n = 0;
    while (nombre[n] && n < PET_NAME_LEN - 1) n++;
    while (n > 0 && nombre[n] && ((unsigned char)nombre[n] & 0xC0) == 0x80) n--;
    memcpy(s_especie_propia[e], nombre, (size_t)n);
    s_especie_propia[e][n] = 0;
}

/* gusto por el clima según el elemento: despejado, nublado, lluvia, viento, nieve, tormenta, frío */
static const int8_t CLIMA_ESPECIE[N_ESPECIES][CLIMA_COUNT] = {
    {60, 0, 50, 0, -20, -40, -40},     /* Brotito: planta, ama el sol y la lluvia; el frío la marchita */
    {40, 0, -10, 20, 0, 60, -10},      /* Chispa: eléctrica, le encantan las tormentas */
    {80, -20, -70, -10, -60, -60, -80}, /* Ascua: fuego, odia mojarse y el frío */
    {-10, 20, 80, 0, 20, 0, 10},       /* Burbu: agua */
    {20, 0, -10, -20, 10, -20, 0},     /* Roco: roca, tranquilo */
    {0, 60, 30, 70, 10, 0, 30},        /* Nubi: nube, el frío le gusta */
    {70, -30, -10, 0, 30, -70, -20},   /* Lumi: luz */
    {-30, 50, 20, 10, 20, 20, 40},     /* Brumo: fantasma, ama el frío */
};
/* lugar preferido de nacimiento por especie */
static const uint8_t LUGAR_ESPECIE[N_ESPECIES] = {
    LUGAR_CASITA, LUGAR_ROCAS, LUGAR_SENDERO, LUGAR_ESTANQUE, LUGAR_ROCAS, LUGAR_CLARO, LUGAR_CLARO, LUGAR_ARBOL,
};

static int8_t i8(float v) { return (int8_t)clampf(v, -100, 100); }

/* gusto por cada zona según el elemento: prado, bosque, lago, colina, cueva */
static const int8_t ESCENA_ESPECIE[N_ESPECIES][ESC_GUSTOS] = {
    {30, 60, 20, 10, -40}, /* Brotito: el bosque */
    {20, 0, -30, 50, 10},  /* Chispa: la colina (viento y tormentas) */
    {30, 0, -60, 50, 10},  /* Ascua: lejos del agua */
    {20, 10, 80, -10, 10}, /* Burbu: el lago */
    {10, 10, 0, 40, 60},   /* Roco: rocas y cueva */
    {20, 0, 10, 70, -30},  /* Nubi: la colina */
    {30, -10, 20, 60, -50},/* Lumi: la luz de la colina, nada de oscuridad */
    {0, 40, 10, 0, 80},    /* Brumo: la cueva */
};

static void nacer_gustos(pet_t *p)
{
    memoria_t *m = &p->memoria;
    const float *R = p->rasgo;
    for (int c = 0; c < CLIMA_COUNT; c++)
        m->clima[c] = i8((float)CLIMA_ESPECIE[p->especie][c] + (rng01(p) - 0.5f) * 50.0f);
    m->clima[CLIMA_NIEVE] = i8((float)m->clima[CLIMA_NIEVE] + (R[TRAIT_ENERGICA] - 0.5f) * 60.0f);
    m->clima[CLIMA_TORMENTA] = i8((float)m->clima[CLIMA_TORMENTA] - R[TRAIT_TIMIDA] * 40.0f);
    m->clima[CLIMA_FRIO] = i8((float)m->clima[CLIMA_FRIO] + (R[TRAIT_ENERGICA] - 0.5f) * 30.0f);

    /* gestos: casi a todas les gustan las palmaditas y las caricias; los abrazos, a las
     * cariñosas (a las tímidas les cuestan); muchos toques seguidos las marean (salvo a
     * algunas traviesas, que lo toman como cosquillas) */
    m->gesto[GESTO_TOQUE] = i8(40 + (rng01(p) - 0.5f) * 50);
    m->gesto[GESTO_CARICIA] = i8(30 + R[TRAIT_CARINOSA] * 50 - R[TRAIT_TIMIDA] * 20 + (rng01(p) - 0.5f) * 40);
    m->gesto[GESTO_MUCHOS] = i8(-55 + R[TRAIT_TRAVIESA] * 50 + (rng01(p) - 0.5f) * 50);
    m->gesto[GESTO_ABRAZO] = i8(15 + R[TRAIT_CARINOSA] * 70 - R[TRAIT_TIMIDA] * 40 + (rng01(p) - 0.5f) * 40);

    /* comidas: la comida de siempre está bien; fruta o miel pueden encantarle o no */
    p->habitos.gusto[FOOD_COMIDA] = (uint8_t)(120 + 60 * R[TRAIT_GLOTONA]);
    p->habitos.gusto[FOOD_FRUTA] = (uint8_t)(60 + 140 * rng01(p) + 30 * R[TRAIT_PULCRA]);
    p->habitos.gusto[FOOD_MIEL] = (uint8_t)(110 + 120 * rng01(p));
    p->habitos.gusto[FOOD_BEBIDA_CALIENTE] = (uint8_t)(150 + 60 * rng01(p));
    p->habitos.gusto[FOOD_BEBIDA_FRIA] = (uint8_t)(150 + 60 * rng01(p));
    if (chance(p, 0.35f)) p->habitos.gusto[chance(p, 0.5f) ? FOOD_FRUTA : FOOD_MIEL] = (uint8_t)(20 + 30 * rng01(p));

    m->agrado[LUGAR_ESPECIE[p->especie]] = 0.3f;
}

void pet_init(pet_t *p, const char *nombre, uint8_t especie, uint32_t ahora, uint32_t seed)
{
    memset(p, 0, sizeof(*p));
    p->magic = PET_SAVE_MAGIC;
    p->version = PET_SAVE_VERSION;
    p->size = (uint16_t)sizeof(pet_t);
    if (nombre) pet_set_name(p, nombre);
    p->especie = (uint8_t)(especie % N_ESPECIES);
    p->rng = seed ? seed : 0xC0FFEEu;
    p->semilla_amigos = mezcla32((seed ? seed : 0xC0FFEEu) ^ 0xA5A5F00Du) | 1u; /* los nombres de sus amigos */
    for (int i = 0; i < 8; i++) rng_next(p); /* mezclar semilla */
    p->dias_jugados = 1; /* (versión 37) la historia empieza el día que nace */
    p->dia_jugado = dia_local(p, ahora);
    p->tilde_chisme = 0xFF;
    p->brizna_k = 0xFF;

    /* personalidad: valores entre 0.15 y 0.85 para que nada sea extremo */
    for (int i = 0; i < TRAIT_COUNT; i++) p->rasgo[i] = p->rasgo_base[i] = 0.15f + 0.7f * rng01(p);

    p->necesidad[NEED_SACIEDAD] = 70;
    p->necesidad[NEED_ENERGIA] = 90;
    p->necesidad[NEED_HIGIENE] = 100;
    p->necesidad[NEED_DIVERSION] = 70;
    p->necesidad[NEED_CARINO] = 60;
    p->necesidad[NEED_SALUD] = 100;
    p->necesidad[NEED_CURIOSIDAD] = 60;
    p->necesidad[NEED_SEGURIDAD] = 70;

    p->eclosion = ahora + PET_HUEVO_SEGUNDOS;
    p->nacimiento = p->eclosion;
    p->ultima_actualizacion = ahora;
    p->ultima_interaccion = ahora;
    p->visita_actual = -1;
    p->hora_actual = (int8_t)hora_local(p, ahora);
    p->emocion = MOOD_TRANQUILA;
    p->etapa = STAGE_HUEVO;
    p->hemisferio_sur = 1;
    p->estacion_fija = 0xFF;
    p->clima = CLIMA_DESPEJADO;
    p->clima_hasta = ahora + 3600;
    p->vinculo = 15.0f; /* al principio desconfía */
    p->raro = 0xFF;
    p->fotos_legado_ok = 1; /* una mascota nueva no tiene fotos de antes */
    p->ultimo_no = 0xFF;
    p->lugar = LUGAR_CLARO;
    p->escena = ESC_PRADO;
    p->escenas_abiertas = 1u << ESC_PRADO;
    p->escenas_vistas = 1u << ESC_PRADO;

    nacer_gustos(p);
    for (int e = 0; e < ESC_GUSTOS; e++) p->gusto_escena[e] = i8((float)ESCENA_ESPECIE[p->especie][e] + (rng01(p) - 0.5f) * 40.0f);

    p->inventario[ITEM_FRUTA] = 2;
    p->inventario[ITEM_HIERBA] = 1;
    p->semillas[CULTIVO_FLOR] = 3; /* alcanza para unas flores */
    p->semillas_vistas = 1u << CULTIVO_FLOR;
    contar_semillas(p);
    p->cuidado = 70.0f;
    p->ultimo_regalo = dia_local(p, ahora);
    p->id = (rng_next(p) | 1u);
    p->ultima_frase_slot = -1;
    /* el baúl de siempre: un moño, un pañuelo y un peluche; el ambiente, bajito */
    p->ropa_tiene = (1u << ROPA_MONO) | (1u << ROPA_PANUELO);
    p->juguetes_tiene = 1u << JUG_PELUCHE;
    p->ambiente_vol = 1;
    gustos_creativos(p);
}

void pet_set_name(pet_t *p, const char *nombre)
{
    memset(p->nombre, 0, sizeof p->nombre);
    if (!nombre) return;
    /* copia hasta PET_NOMBRE_MAX letras sin cortar una letra UTF-8 por la mitad */
    int letras = 0, i = 0;
    while (nombre[i] && letras < PET_NOMBRE_MAX) {
        int largo = ((unsigned char)nombre[i] & 0xE0) == 0xC0 ? 2 : (((unsigned char)nombre[i] & 0xF0) == 0xE0 ? 3 : 1);
        if (i + largo > PET_NAME_LEN - 1) break;
        memcpy(p->nombre + i, nombre + i, (size_t)largo);
        i += largo;
        letras++;
    }
}

void pet_set_owner_age(pet_t *p, int edad)
{
    if (edad < 1) edad = 1;
    if (edad > 120) edad = 120;
    p->edad_dueno = (uint8_t)edad;
    p->modo_nino = (uint8_t)(edad < PET_EDAD_INFANTE ? MODO_INFANTE : edad < PET_EDAD_NINO ? MODO_NINO : MODO_ADULTO);
    if (p->modo_manual && p->modo_manual <= 3) p->modo_nino = (uint8_t)(p->modo_manual - 1); /* lo elegido a mano manda */
}
void pet_set_mode_manual(pet_t *p, int modo)
{
    p->modo_manual = (uint8_t)(modo < 0 || modo > MODO_INFANTE ? 0 : modo + 1);
    if (p->modo_manual) p->modo_nino = (uint8_t)modo;
    else if (p->edad_dueno) pet_set_owner_age(p, p->edad_dueno);
}
bool pet_child_mode(const pet_t *p) { return p->modo_nino != 0; }
modo_juego_t pet_mode(const pet_t *p) { return p->modo_nino <= MODO_INFANTE ? (modo_juego_t)p->modo_nino : MODO_NINO; }
const char *pet_mode_name(modo_juego_t m)
{
    static const char *const T[] = {"adulto", "niño", "infante"};
    return m <= MODO_INFANTE ? T[m] : "?";
}

void pet_set_timezone(pet_t *p, int32_t offset_s)
{
    p->zona_horaria = offset_s;
    p->hora_actual = (int8_t)hora_local(p, p->ultima_actualizacion);
}

float pet_age_days(const pet_t *p, uint32_t ahora)
{
    if (p->etapa == STAGE_HUEVO) return 0.0f;
    return ahora > p->nacimiento ? (float)(ahora - p->nacimiento) / 86400.0f : 0.0f;
}

/* ======================================================================
 * 3. Memoria afectiva y personalidad que cambia
 * ==================================================================== */

/* la personalidad se mueve muy de a poco según cómo la tratan */
static void moldear(pet_t *p, trait_t t, float delta, uint32_t ahora)
{
    float antes = p->rasgo[t];
    p->rasgo[t] = clampf(antes + delta, 0.05f, 0.95f);
    /* aviso cuando el cambio respecto de cómo nació cruza un escalón de 0.15 */
    int a = (int)floorf((antes - p->rasgo_base[t]) / 0.15f), b = (int)floorf((p->rasgo[t] - p->rasgo_base[t]) / 0.15f);
    if (a != b) push_event(p, EV_RASGO, t, b > a ? 1 : -1, ahora);
}

static void descubrir(pet_t *p, gusto_tipo_t tipo, int id, uint32_t ahora)
{
    uint8_t *bits = &p->memoria.descubierto[tipo];
    if (id < 0 || id > 7 || (*bits >> id) & 1u) return;
    *bits |= (uint8_t)(1u << id);
    push_event(p, EV_DESCUBRE, tipo, id, ahora);
}

static void subir_vinculo(pet_t *p, float cuanto)
{
    /* rendimientos decrecientes: cuesta más llegar a 100 */
    p->vinculo = clampf(p->vinculo + cuanto * (1.2f - p->vinculo / 100.0f), 0, 100);
}

static void susto_aqui(pet_t *p, float fuerza) { p->memoria.miedo[p->lugar] = clampf(p->memoria.miedo[p->lugar] + fuerza, 0, 1); }

static void olvidar(pet_t *p, float h)
{
    memoria_t *m = &p->memoria;
    for (int l = 0; l < LUGAR_COUNT; l++) {
        m->agrado[l] = decae(m->agrado[l], h, 96.0f); /* vida media 4 días */
        m->miedo[l] = decae(m->miedo[l], h, 72.0f);   /* el susto se olvida en ~3 días */
    }
    /* el lugar de su especie siempre le gusta un poco */
    if (m->agrado[LUGAR_ESPECIE[p->especie]] < 0.15f) m->agrado[LUGAR_ESPECIE[p->especie]] = 0.15f;
    for (int i = 0; i < PET_MAX_VISITORS; i++) /* las relaciones con visitas vuelven a 0 */
        p->visitantes[i].relacion = (int8_t)(p->visitantes[i].relacion * powf(0.5f, h / 240.0f));
    p->enamoramiento = decae(p->enamoramiento, h, 48.0f);
    for (int i = 0; i < LUGAR_COUNT * (LUGAR_COUNT - 1) / 2; i++) p->senda[i] = decae(p->senda[i], h, 168.0f); /* una semana */
    /* los trucos se olvidan muy de a poco si no se practican (lo bien aprendido dura semanas) */
    for (int i = 0; i < TRUCO_COUNT; i++)
        p->truco[i] = decae(p->truco[i], h, p->truco[i] >= PET_TRUCO_APRENDIDO ? 24.0f * 40.0f : 24.0f * 10.0f);
}

/* ======================================================================
 * 4. Simulación por pasos
 * ==================================================================== */

static void dormirse(pet_t *p, uint32_t t)
{
    if (p->dormida) return;
    p->dormida = 1;
    p->deseo = WANT_NADA;
    p->actividad = ACTV_SIESTA;
    /* para dormir se saca todo lo que lleva puesto (la ropa vuelve al baúl y lo de la cabeza a la Bolsa) */
    for (int s = 0; s < RANURAS_VIEJAS; s++) p->ropa_puesta[s] = 0;
    p->ropa_mano = 0;
    p->obj_cabeza = 0;
    p->cabeza_hasta = 0;
    p->probando = 0;
    if (p->escena == ESC_PRADO && ((p->decoraciones >> DECOR_CASITA) & 1u)) p->lugar = LUGAR_CASITA; /* a su camita */
    push_event(p, EV_SE_DURMIO, 0, 0, t);
    sonar(p, SND_BOSTEZO, t);
}

static void despertarse(pet_t *p, uint32_t t, bool forzado)
{
    if (!p->dormida) return;
    p->dormida = 0;
    p->vivido = 0; /* lo que soñó ya pasó: hoy se vive algo nuevo */
    p->actividad = ACTV_PASEAR;
    p->actividad_hasta = t;
    push_event(p, EV_DESPERTO, forzado ? 1 : 0, 0, t);
    if (forzado && p->necesidad[NEED_ENERGIA] < 50) {
        impulsar(p, EMO_ENOJO, 0.5f, CAUSA_LA_DESPERTASTE);
        sonar(p, SND_GRUNIDO, t);
    } else {
        sonar(p, SND_ALEGRE, t);
    }
}

static void enfermar(pet_t *p, uint32_t t)
{
    if (p->enferma) return;
    p->enferma = 1;
    p->dosis = 0;
    p->enfermo_desde = t;
    push_event(p, EV_ENFERMO, 0, 0, t);
    sonar(p, SND_ESTORNUDO, t);
}

static void sanar(pet_t *p, uint32_t t)
{
    if (!p->enferma) return;
    p->enferma = 0;
    p->dosis = 0;
    push_event(p, EV_SANO, 0, 0, t);
    sonar(p, SND_ALEGRE, t);
}

/* --- hábitos del dueño --- */
#define HAB_ALIMENTAR 0
#define HAB_JUGAR 1
#define HAB_ACARICIAR 2
#define HAB_BANAR 3

static void cerrar_hora(pet_t *p, int hora, bool despierta)
{
    for (int k = 0; k < HABIT_KINDS; k++) {
        uint8_t *v = &p->habitos.prob[k][hora];
        if (p->hora_flags & (1u << k)) continue; /* ya se reforzó al interactuar */
        /* la rutina esperada no ocurrió: lo nota si estaba despierta */
        if (despierta && *v > 170 && (k == HAB_JUGAR || k == HAB_ACARICIAR))
            impulsar(p, EMO_TRISTEZA, 0.35f, k == HAB_JUGAR ? CAUSA_NO_JUGASTE : CAUSA_TE_EXTRANA);
        *v = (uint8_t)(*v - (*v * 8) / 100); /* olvido lento */
    }
    p->hora_flags = 0;
}

static void registrar_habito(pet_t *p, int kind, uint32_t t)
{
    int h = hora_local(p, t);
    uint8_t *v = &p->habitos.prob[kind][h];
    *v = (uint8_t)(*v + ((255 - *v) * 25) / 100);
    p->hora_flags |= (uint8_t)(1u << kind);
}

static float expectativa(const pet_t *p, int kind, uint32_t t)
{
    return p->habitos.prob[kind][hora_local(p, t)] / 255.0f;
}

/* --- visitas --- */

static int buscar_slot_visitante(pet_t *p)
{
    int libres[PET_MAX_VISITORS], nl = 0, conocidos[PET_MAX_VISITORS], nc = 0;
    for (int i = 0; i < PET_MAX_VISITORS; i++) {
        if (p->visitantes[i].conocido) conocidos[nc++] = i;
        else libres[nl++] = i;
    }
    bool repetir = nc > 0 && (nl == 0 || chance(p, 0.6f));
    if (repetir) return conocidos[rng_next(p) % (uint32_t)nc];
    if (nl == 0) return -1;
    int i = libres[0];
    visitor_t *v = &p->visitantes[i];
    memset(v, 0, sizeof(*v));
    v->conocido = 1;
    v->especie = (uint8_t)(rng_next(p) % N_ESPECIES);
    for (int k = 0; k < TRAIT_COUNT; k++) v->rasgos[k] = (uint8_t)(40 + rng_next(p) % 176);
    return i;
}

static encounter_t resolver_encuentro(pet_t *p, visitor_t *v, uint32_t t)
{
    float timida = p->rasgo[TRAIT_TIMIDA];
    float v_carinosa = v->rasgos[TRAIT_CARINOSA] / 255.0f;
    float v_energica = v->rasgos[TRAIT_ENERGICA] / 255.0f;
    float parecido = 1.0f - fabsf(p->rasgo[TRAIT_ENERGICA] - v_energica);
    bool de_mal_humor = p->emo[EMO_ENOJO] > 0.5f || p->emo[EMO_TRISTEZA] > 0.6f;
    encounter_t r;

    if (v->enamorados) {
        r = ENC_JUGARON;
    } else if (timida > 0.6f && v->relacion < 20 && chance(p, timida * 0.6f)) {
        r = ENC_SE_ASUSTO;
    } else {
        float agresion = (de_mal_humor ? 0.35f : 0.0f) + (1.0f - v_carinosa) * 0.3f +
                         (v->relacion < -20 ? 0.3f : 0.0f) - parecido * 0.15f;
        if (p->modo_nino) agresion *= 0.4f;
        if (chance(p, clampf(agresion, 0, 1) * 0.6f)) {
            r = ENC_PELEARON;
        } else if (v->relacion > 60 && p->etapa == STAGE_ADULTA && v_carinosa > 0.5f && parecido > 0.5f &&
                   chance(p, 0.35f)) {
            r = ENC_SE_ENAMORARON;
        } else if (p->necesidad[NEED_ENERGIA] > 25 && chance(p, 0.55f + parecido * 0.3f)) {
            r = ENC_JUGARON;
        } else {
            r = ENC_SE_IGNORARON;
        }
    }

    int rel = v->relacion;
    switch (r) {
    case ENC_JUGARON:
        sumar(p, NEED_DIVERSION, 20);
        sumar(p, NEED_CARINO, v->enamorados ? 15 : 5);
        sumar(p, NEED_ENERGIA, -5);
        impulsar(p, EMO_ALEGRIA, 0.6f, CAUSA_VISITA);
        rel += 10;
        if (v->enamorados) p->enamoramiento = 1.0f;
        sonar(p, v->enamorados ? SND_SUSPIRO_AMOR : SND_RISA, t);
        break;
    case ENC_PELEARON:
        sumar(p, NEED_SEGURIDAD, -15);
        sumar(p, NEED_HIGIENE, -10);
        impulsar(p, EMO_ENOJO, 0.6f, CAUSA_VISITA);
        rel -= 15;
        sonar(p, SND_PELEA, t);
        break;
    case ENC_SE_ENAMORARON:
        v->enamorados = 1;
        p->enamoramiento = 1.0f;
        impulsar(p, EMO_CARINO, 0.9f, CAUSA_VISITA);
        sumar(p, NEED_CARINO, 30);
        rel += 20;
        sonar(p, SND_SUSPIRO_AMOR, t);
        break;
    case ENC_SE_ASUSTO:
        sumar(p, NEED_SEGURIDAD, -20);
        impulsar(p, EMO_MIEDO, 0.6f, CAUSA_VISITA);
        susto_aqui(p, 0.2f);
        rel += 3;
        sonar(p, SND_SORPRESA, t);
        break;
    default:
        rel += 1;
        break;
    }
    v->relacion = (int8_t)(rel > 100 ? 100 : (rel < -100 ? -100 : rel));
    if (v->encuentros < 0xFFFF) v->encuentros++;
    return r;
}

float pet_visita_escala = 1.0f; /* (las pruebas lo ponen en 0 para que una visita no tape lo que miden) */
static void paso_visitas(pet_t *p, uint32_t t, float dt, bool vivo)
{
    if (p->visita_actual >= 0) {
        if (t >= p->visita_hasta || p->dormida) {
            push_event(p, EV_VISITA_SE_VA, p->visita_actual, 0, t);
            p->visita_actual = -1;
        }
        return;
    }
    if (p->dormida || p->vecino.presente) return;
    int h = hora_local(p, t);
    if (h < 9 || h >= 21 || p->clima == CLIMA_TORMENTA) return;
    float por_hora = (0.2f + 0.2f * p->rasgo[TRAIT_CURIOSA]) * pet_visita_escala; /* visitas más seguido (antes 0.04 + 0.05) */
    if (!chance(p, prob_paso(por_hora, dt))) return;

    int i = buscar_slot_visitante(p);
    if (i < 0) return;
    p->visita_actual = (int8_t)i;
    p->visita_hasta = t + 600 + rng_next(p) % 1200; /* 10 a 30 minutos */
    sumar(p, NEED_CURIOSIDAD, 20);
    impulsar(p, EMO_CURIOSIDAD, 0.6f, CAUSA_VISITA);
    push_event(p, EV_VISITA_LLEGA, i, 0, t);
    if (vivo) sonar(p, SND_SORPRESA, t);
    encounter_t r = resolver_encuentro(p, &p->visitantes[i], t);
    push_event(p, EV_ENCUENTRO, i, r, t);
}

/* para el probador: llega una visita ya (sin esperar la hora, el azar ni el clima) */
bool pet_visit_now(pet_t *p, uint32_t t)
{
    if (p->etapa == STAGE_HUEVO || p->dormida || p->visita_actual >= 0) return false;
    int i = buscar_slot_visitante(p);
    if (i < 0) return false;
    p->visita_actual = (int8_t)i;
    p->visita_hasta = t + 600;
    impulsar(p, EMO_CURIOSIDAD, 0.6f, CAUSA_VISITA);
    push_event(p, EV_VISITA_LLEGA, i, 0, t);
    sonar(p, SND_SORPRESA, t);
    encounter_t r = resolver_encuentro(p, &p->visitantes[i], t);
    push_event(p, EV_ENCUENTRO, i, r, t);
    return true;
}

/* --- su amigo del barrio: viene cada uno o dos días, juegan, comen juntos y conversan --- */

static const char *const NOMBRES_VECINO[] = {"Pipo", "Lulú", "Tito", "Mochi", "Kiri", "Nina", "Coco", "Bimba", "Pompón", "Chispi",
                                             "Toto", "Mela", "Rulo", "Fifí", "Bruno", "Lila", "Tomi", "Nube", "Pecas", "Yuyu"};
#define N_NOMBRES_VECINO (sizeof NOMBRES_VECINO / sizeof NOMBRES_VECINO[0])
#define N_AMIGOS 32 /* (versión 25) sus amigos del barrio, con nombres inventados */

/* --- nombres inventados: dos o tres sílabas suaves y, a veces, un final (Lumi, Tavo, Nirú, Pelusa, Kío...) --- */
static uint32_t mezcla32(uint32_t h) { h ^= h >> 16; h *= 0x7FEB352Du; h ^= h >> 15; h *= 0x846CA68Bu; h ^= h >> 16; return h; }
static bool nombre_feo(const char *s)
{
    static const char *const MAL[] = {"puta", "puto", "caca", "culo", "pene", "teta", "pito", "poto", "pija", "verga", "polla",
                                      "cago", "pedo", "mierd", "coño", "chupa", "mama", "papa", "kk", "nazi", "pis", "mear", "moco", "feo", "gord"};
    char b[24];
    int k = 0;
    for (const char *q = s; *q && k < 23; q++) b[k++] = (char)(*q >= 'A' && *q <= 'Z' ? *q + 32 : *q);
    b[k] = 0;
    for (unsigned i = 0; i < sizeof MAL / sizeof MAL[0]; i++) if (strstr(b, MAL[i])) return true;
    return false;
}
const char *pet_make_name(uint32_t semilla, char *out, int n)
{
    static const char *const S[] = {"lu", "mi", "ta", "ki", "po", "ni", "pe", "bo", "chi", "ra", "fi", "to", "ma", "nu", "pi", "sa",
                                    "ti", "yu", "co", "li", "mo", "da", "fe", "ku", "ne", "ro", "su", "va", "zu", "be", "ga", "la",
                                    "no", "ri", "so", "tu", "ka", "na", "lo", "mu", "bi", "di", "pa", "vi", "ye", "ju"};
    static const char *const F[] = {"n", "s", "l", "ty", "lú", "rí", "ní", "to", "na", "sa", "lín", "ta", "co", "mi"};
    const int NS = (int)(sizeof S / sizeof S[0]), NF = (int)(sizeof F / sizeof F[0]);
    for (uint32_t intento = 0; intento < 8; intento++) {
        uint32_t h = mezcla32(semilla * 2654435761u + intento * 40503u + 1u);
        int a = (int)(h % (uint32_t)NS), b = (int)((h >> 8) % (uint32_t)NS), c = (int)((h >> 16) % (uint32_t)NS), f = (int)((h >> 24) % (uint32_t)NF);
        if (b == a) b = (b + 7) % NS;
        int forma = (int)((h >> 5) % 10u); /* 0-4: dos sílabas, 5-7: dos y un final, 8-9: tres sílabas */
        char t[24];
        if (forma < 5) snprintf(t, sizeof t, "%s%s", S[a], S[b]);
        else if (forma < 8) snprintf(t, sizeof t, "%s%s%s", S[a], S[b], F[f]);
        else snprintf(t, sizeof t, "%s%s%s", S[a], S[b], S[c]);
        if (nombre_feo(t)) continue;
        if (t[0] >= 'a' && t[0] <= 'z') t[0] = (char)(t[0] - 32);
        snprintf(out, (size_t)n, "%s", t);
        return out;
    }
    snprintf(out, (size_t)n, "Lumi");
    return out;
}
int pet_friends_max(const pet_t *p) { return p->semilla_amigos ? N_AMIGOS : (int)N_NOMBRES_VECINO; }
/* el nombre de su amigo i: inventado con su semilla (distinto del de ella y de los otros amigos) */
static void nombre_amigo_en(const pet_t *p, int i, char *out, int n)
{
    if (!p->semilla_amigos) { snprintf(out, (size_t)n, "%s", (unsigned)i < N_NOMBRES_VECINO ? NOMBRES_VECINO[i] : "un amigo"); return; }
    if ((unsigned)i >= N_AMIGOS) { snprintf(out, (size_t)n, "un amigo"); return; }
    for (uint32_t sal = 0; sal < 6; sal++) {
        pet_make_name(p->semilla_amigos ^ ((uint32_t)i * 0x9E3779B9u) ^ (sal * 0x85EBCA6Bu), out, n);
        bool repetido = !strcmp(out, p->nombre);
        char otro[PET_NAME_LEN];
        for (int j = 0; j < i && !repetido; j++) { /* (se compara con los anteriores, así nunca hay dos iguales) */
            pet_make_name(p->semilla_amigos ^ ((uint32_t)j * 0x9E3779B9u), otro, sizeof otro);
            repetido = !strcmp(out, otro);
        }
        if (!repetido) return;
    }
}
const char *pet_friend_name(const pet_t *p, int i)
{
    static char ring[4][PET_NAME_LEN];
    static int k;
    char *o = ring[k++ & 3];
    nombre_amigo_en(p, i, o, PET_NAME_LEN);
    return o;
}

/* un amigo distinto en cada visita: otro nombre, otra especie, otros colores */
static void nuevo_visitante(pet_t *p)
{
    vecino_t *v = &p->vecino;
    int ant = v->nombre_i, i = 0, n = pet_friends_max(p);
    uint64_t ok = 0;
    for (int k = 0; k < n; k++) if (k != ant && strcmp(pet_friend_name(p, k), p->nombre)) ok |= 1ull << k;
    uint64_t bolsa = p->bolsa_amigo;
    i = bolsa_tomar(p, &bolsa, n, ok);
    p->bolsa_amigo = (uint32_t)bolsa;
    if (i < 0) i = (ant + 1) % n;
    v->nombre_i = (uint8_t)i;
    snprintf(v->nombre, sizeof v->nombre, "%s", pet_friend_name(p, i));
    v->especie = (uint8_t)((p->especie + 1 + rng_next(p) % (N_ESPECIES - 1)) % N_ESPECIES);
    v->color = (uint8_t)(chance(p, 0.5f) ? 0 : rng_next(p) % PET_COLORES);
}

static void crear_vecino(pet_t *p, uint32_t t)
{
    vecino_t *v = &p->vecino;
    if (!v->nombre[0]) {
        v->nombre_i = 0xFF;
        nuevo_visitante(p);
        v->amistad = 20.0f;
    }
    v->presente = 0;
    v->proxima = t + 3u * 3600u + rng_next(p) % (5u * 3600u); /* la primera visita, el mismo día o el siguiente */
}

bool pet_neighbor_here(const pet_t *p) { return p->vecino.presente != 0; }

static void vecino_se_va(pet_t *p, uint32_t t)
{
    vecino_t *v = &p->vecino;
    v->presente = 0;
    v->se_fue = t;
    float horas = 10.0f + rng01(p) * 20.0f; /* otro amigo viene en medio día o un día */
    if (p->modo_nino) horas *= 0.6f;
    v->proxima = t + (uint32_t)(horas * 3600.0f);
    if (p->actividad == ACTV_JUNTOS) p->actividad_hasta = t;
    push_event(p, EV_VECINO, VEC_SE_VA, 0, t);
}

static void vecino_actividad(pet_t *p, uint32_t t, bool vivo)
{
    vecino_t *v = &p->vecino;
    const float *N = p->necesidad;
    float u[VEC_COUNT] = {0};
    u[VEC_JUGAR] = N[NEED_ENERGIA] > 35 ? 0.5f + 0.3f * p->rasgo[TRAIT_ENERGICA] : 0.0f;
    u[VEC_CORRER] = N[NEED_ENERGIA] > 45 ? 0.4f : 0.0f;
    u[VEC_COMER] = N[NEED_SACIEDAD] < 75 ? 0.4f + (75.0f - N[NEED_SACIEDAD]) / 60.0f : 0.1f;
    u[VEC_CHARLAR] = 0.35f + 0.2f * p->rasgo[TRAIT_CARINOSA];
    u[VEC_SIESTA] = N[NEED_ENERGIA] < 45 ? 0.8f : 0.0f;
    u[VEC_REGALO] = v->regalo_hecho ? 0.0f : 0.3f;
    if (v->actividad < VEC_COUNT && v->actividad >= VEC_JUGAR) u[v->actividad] *= 0.3f; /* no repetir tanto */
    int mejor = VEC_CHARLAR;
    float mu = -1;
    for (int i = VEC_JUGAR; i < VEC_COUNT; i++) {
        float x = u[i] > 0 ? u[i] + rng01(p) * 0.3f : 0;
        if (x > mu) { mu = x; mejor = i; }
    }
    v->actividad = (uint8_t)mejor;
    v->actividad_hasta = t + 150u + rng_next(p) % 150u;
    int b = 0;
    switch (mejor) {
    case VEC_JUGAR: sumar(p, NEED_DIVERSION, 20); sumar(p, NEED_ENERGIA, -5); v->amistad += 3; impulsar(p, EMO_ALEGRIA, 0.7f, CAUSA_VECINO); break;
    case VEC_CORRER: sumar(p, NEED_DIVERSION, 14); sumar(p, NEED_ENERGIA, -7); v->amistad += 2; impulsar(p, EMO_ALEGRIA, 0.6f, CAUSA_VECINO); break;
    case VEC_COMER:
        if (p->inventario[ITEM_FRUTA]) { p->inventario[ITEM_FRUTA]--; sumar(p, NEED_SACIEDAD, 22); b = ITEM_FRUTA; } /* comparten tu fruta */
        else { sumar(p, NEED_SACIEDAD, 15); b = -1; }                                                       /* él trajo galletas */
        sumar(p, NEED_CARINO, 4); v->amistad += 2; impulsar(p, EMO_ALEGRIA, 0.5f, CAUSA_VECINO);
        if (vivo) sonar(p, SND_COMER, t);
        break;
    case VEC_CHARLAR: sumar(p, NEED_CARINO, 8); sumar(p, NEED_SEGURIDAD, 6); sumar(p, NEED_CURIOSIDAD, 8); v->amistad += 2; impulsar(p, EMO_CARINO, 0.4f, CAUSA_VECINO); break;
    case VEC_SIESTA: sumar(p, NEED_ENERGIA, 10); sumar(p, NEED_SEGURIDAD, 8); v->amistad += 1; break;
    case VEC_REGALO: {
        static const item_t R[] = {ITEM_SEMILLA, ITEM_PIEDRA, ITEM_FRUTA, ITEM_MIEL, ITEM_SEMILLA};
        item_t it = R[rng_next(p) % 5];
        guardar_item(p, it);
        b = it;
        v->regalo_hecho = 1;
        v->amistad += 3;
        impulsar(p, EMO_ALEGRIA, 0.7f, CAUSA_VECINO);
        if (vivo) sonar(p, SND_SORPRESA, t);
        break;
    }
    default: break;
    }
    v->amistad = clampf(v->amistad, 0, 100);
    push_event(p, EV_VECINO, mejor, b, t);
}

static void paso_vecino(pet_t *p, uint32_t t, bool vivo)
{
    vecino_t *v = &p->vecino;
    if (!v->nombre[0]) crear_vecino(p, t);
    if (v->presente) {
        if (t >= v->hasta || p->dormida || p->clima == CLIMA_TORMENTA) { vecino_se_va(p, t); return; }
        if (t >= v->actividad_hasta) vecino_actividad(p, t, vivo);
        if (p->actividad != ACTV_JUNTOS && t >= p->actividad_hasta) { p->actividad = ACTV_JUNTOS; p->actividad_hasta = v->hasta; }
        return;
    }
    if (p->dormida) { /* llega de visita, la ve dormida y se va sin molestar */
        if (t >= v->proxima && p->visita_actual < 0 && p->clima != CLIMA_TORMENTA) {
            int hd = hora_local(p, t);
            if (hd >= 9 && hd < 19) {
                v->proxima = t + (8u + rng_next(p) % 8u) * 3600u;
                push_event(p, EV_VECINO, VEC_DORMIDA, 0, t);
            }
        }
        return;
    }
    if (t < v->proxima || p->visita_actual >= 0) return;
    if (t - p->ultima_interaccion > AUSENCIA_S) { v->proxima = t + 6u * 3600u; return; } /* viene cuando tú andas cerca */
    int h = hora_local(p, t);
    if (h < 9 || h >= 19 || p->clima == CLIMA_TORMENTA || p->clima == CLIMA_LLUVIA) {
        if (h >= 19) v->proxima = t + 12u * 3600u; /* mañana */
        return;
    }
    if (v->visitas) nuevo_visitante(p); /* cada vez, alguien distinto */
    v->presente = 1;
    v->regalo_hecho = 0;
    v->hasta = t + 20u * 60u + rng_next(p) % (20u * 60u); /* 20 a 40 minutos */
    v->actividad = VEC_LLEGA;
    v->actividad_hasta = t + 60;
    if (v->visitas < 0xFFFF) v->visitas++;
    v->amistad = clampf(v->amistad + 1, 0, 100);
    p->actividad = ACTV_JUNTOS;
    p->actividad_hasta = v->hasta;
    sumar(p, NEED_CURIOSIDAD, 10);
    sumar(p, NEED_CARINO, 5);
    impulsar(p, EMO_ALEGRIA, 0.8f, CAUSA_VECINO);
    push_event(p, EV_VECINO, VEC_LLEGA, 0, t);
    vivir(p, VIV_VECINO);
    anotar(p, DIA_VECINO, v->nombre_i, 0, t);
    if (v->nombre_i < pet_friends_max(p)) {
        bool primero = !p->vecinos_vistos;
        p->vecinos_vistos |= 1u << v->nombre_i;
        if (primero && !p->carta_dia) programar_carta(p, t); /* desde ahora le pueden escribir */
    }
    if (vivo) sonar(p, SND_HOLA_AMIGO, t);
}

/* --- cartas de los amigos del barrio --- */
const char *pet_g(const pet_t *p, const char *nina, const char *nino) { return p && p->genero ? nino : nina; }

/* textos con marcas de género: {a} -> a/o, {la} -> la/el, {ella} -> ella/él, {una} -> una/un, {guardiana} -> guardiana/guardián
 * (cada marca también en MAYÚSCULAS o con Mayúscula inicial, y la palabra sale igual) */
const char *pet_gender_text(const pet_t *p, const char *src, char *out, int n)
{
    static const char *const M[][3] = {{"a", "a", "o"}, {"la", "la", "el"}, {"ella", "ella", "él"}, {"una", "una", "un"},
                                       {"guardiana", "guardiana", "guardián"}, {"o", "a", "o"}, {"lo", "la", "lo"}};
    if (!out || n <= 0) return src;
    int k = 0;
    const bool nino = p && p->genero;
    while (src && *src && k < n - 1) {
        const char *cierra = src[0] == '{' ? strchr(src, '}') : NULL;
        if (cierra && cierra - src <= 11) {
            char tok[12];
            int lt = (int)(cierra - src - 1);
            for (int i = 0; i < lt; i++) tok[i] = (char)((src[1 + i] >= 'A' && src[1 + i] <= 'Z') ? src[1 + i] + 32 : src[1 + i]);
            tok[lt] = 0;
            const bool mayus = src[1] >= 'A' && src[1] <= 'Z', todo = mayus && lt > 1 && src[2] >= 'A' && src[2] <= 'Z';
            const char *rep = NULL;
            for (unsigned m = 0; m < sizeof M / sizeof M[0]; m++) if (!strcmp(tok, M[m][0])) rep = M[m][nino ? 2 : 1];
            if (rep) {
                for (int i = 0; rep[i] && k < n - 1; i++) {
                    char c = rep[i];
                    if ((todo || (mayus && i == 0) || (mayus && lt == 1)) && c >= 'a' && c <= 'z') c = (char)(c - 32);
                    if ((todo || (mayus && i == 0)) && (unsigned char)c == 0xC3 && rep[i + 1]) { out[k++] = c; i++; c = (char)(rep[i] - 32); } /* á é í ó ú -> Á É Í Ó Ú */
                    out[k++] = c;
                }
                src = cierra + 1;
                continue;
            }
        }
        out[k++] = *src++;
    }
    out[k] = 0;
    return out;
}
static void genero_en(const pet_t *p, char *buf, int n)
{
    if (!buf || !strchr(buf, '{')) return;
    char tmp[640];
    pet_gender_text(p, buf, tmp, (int)sizeof tmp);
    snprintf(buf, (size_t)n, "%s", tmp);
}

typedef enum { CT_VIDA = 0, CT_EXTRANA, CT_RECUERDO, CT_PRIMAVERA, CT_VERANO, CT_OTONO, CT_INVIERNO, CT_OTRO_AMIGO, CT_MAGICA } carta_tipo_t;
typedef struct { uint8_t tipo; const char *txt; } carta_txt_t;
/* {N}: su nombre, {A}: otro amigo que conoce, {o}: "a" si es niña y "o" si es niño */
static const carta_txt_t CARTAS[] = {
    {CT_VIDA, "Te cuento que en el árbol de mi casa nacieron tres pajaritos. Hacen pío-pío toda la mañana y ya no puedo dormir siesta, ¡pero son tan lindos!"},
    {CT_VIDA, "Ayer aprendí a silbar con una hoja. Me salió un sonido rarísimo y un sapo me contestó desde el charco. Creo que ahora somos amigos."},
    {CT_VIDA, "Mi abuela me enseñó a hacer mermelada de moras. Me quedó un poquito quemada, pero igual me la comí toda. La próxima vez te guardo un frasco."},
    {CT_VIDA, "Estoy juntando piedras redondas para hacer un caminito hasta mi puerta. Ya tengo once. Si encuentras una bonita, ¡guárdamela!"},
    {CT_VIDA, "Se me perdió un calcetín y lo encontré en el nido de una urraca. No me lo quería devolver, así que al final le regalé el otro."},
    {CT_VIDA, "Esta semana llovió tanto que el arroyo llegó hasta mi casa. Hice un barquito de corteza y navegó solito hasta la cocina."},
    {CT_VIDA, "Estoy aprendiendo a tocar la flauta. Todavía suena como un pato resfriado, pero practico todos los días. Algún día te toco una canción."},
    {CT_VIDA, "Plantamos un manzano con mi familia. Dicen que tarda años en dar manzanas. Cuando dé la primera, es para ti."},
    {CT_EXTRANA, "Hace días que no voy a verte y te echo de menos. Guardé la piña más grande del bosque para cuando nos veamos."},
    {CT_EXTRANA, "Hoy pasé cerca de tu prado y me acordé de ti. Te mando un abrazo grande, grande como un árbol."},
    {CT_EXTRANA, "El bosque está más aburrido sin nuestras carreras. ¿Me esperas? Pronto voy a visitarte."},
    {CT_EXTRANA, "Anoche soñé que volábamos sobre una hoja gigante. Desperté con muchas ganas de verte. ¡Cuídate mucho!"},
    {CT_EXTRANA, "Te escribo solo para decirte que eres de mis amigos favoritos. Bueno, y también que te extraño un montón."},
    {CT_RECUERDO, "¿Te acuerdas cuando nos tiramos la pelota y cayó en el estanque? Todavía me río cada vez que lo pienso."},
    {CT_RECUERDO, "Me acordé del picnic que hicimos en tu prado. Esas galletas estaban riquísimas. ¿Repetimos pronto?"},
    {CT_RECUERDO, "Todavía me acuerdo de lo mucho que corrimos la última vez. ¡Qué rápido eres! La próxima vez te gano, ya vas a ver."},
    {CT_RECUERDO, "Me acordé de la siesta que dormimos bajo el árbol. Roncabas bajito, como un gatito. ¡No te enojes, era muy tierno!"},
    {CT_RECUERDO, "Me acordé de cuando nos contamos secretos. Guardé el tuyo muy bien, te lo prometo."},
    {CT_RECUERDO, "Encontré la hoja con forma de corazón que vimos juntos. La tengo pegada en mi ventana y la miro todas las mañanas."},
    {CT_PRIMAVERA, "Aquí ya florecieron los cerezos y todo huele a miel. Las abejas andan felices y yo estornudo un poquito."},
    {CT_PRIMAVERA, "Afuera de mi casa salió un hongo con lunares. Le puse nombre: se llama Don Sombrero. Te lo presento cuando vengas."},
    {CT_VERANO, "Hace tanto calor que me paso el día en el arroyo. Ayer un pececito me hizo cosquillas en los pies."},
    {CT_VERANO, "Anoche me quedé mirando las luciérnagas hasta tarde. Pedí un deseo por ti, pero no te lo cuento porque si no, no se cumple."},
    {CT_OTONO, "Las hojas están cayendo y hago montones para saltar encima. Es lo mejor del año, ¿no crees?"},
    {CT_OTONO, "Estoy guardando nueces para el invierno. Ya tengo tantas que no me cabe nada más debajo de la cama."},
    {CT_INVIERNO, "Aquí nevó toda la noche. Hice un muñeco de nieve que se parece un poco a ti. Le puse una ramita de nariz."},
    {CT_INVIERNO, "Hace tanto frío que tomo chocolate caliente todas las tardes. Tú abrígate bien, ¿ya?"},
    {CT_OTRO_AMIGO, "El otro día jugué con {A} y hablamos de ti. Los dos dijimos que eres la mejor compañía del bosque."},
    {CT_OTRO_AMIGO, "{A} me prestó su cometa y casi me lleva volando por encima de los pinos. Te lo cuento para que te rías."},
};
#include "textos23.h"
#define N_CARTAS_V22 (int)(sizeof CARTAS / sizeof CARTAS[0])
#define N_CARTAS_MAS (int)(sizeof CARTAS_MAS / sizeof CARTAS_MAS[0])
#define N_CARTAS (N_CARTAS_V22 + N_CARTAS_MAS)
#define N_FAMILIA (int)(sizeof CARTAS_FAMILIA / sizeof CARTAS_FAMILIA[0])
#define N_VIAJERO (int)(sizeof CARTAS_VIAJERO / sizeof CARTAS_VIAJERO[0])
#define N_INVITACION (int)(sizeof INVITACIONES / sizeof INVITACIONES[0])
static const carta_txt_t *carta_amigo(int i) { return i < N_CARTAS_V22 ? &CARTAS[i] : &CARTAS_MAS[(i - N_CARTAS_V22) % N_CARTAS_MAS]; }
int pet_letter_bodies(void) { return N_CARTAS + N_FAMILIA + N_VIAJERO + N_INVITACION; }
int pet_letter_bodies_of(carta_de_t de)
{
    switch (de) {
    case CARTA_AMIGO: return N_CARTAS;
    case CARTA_FAMILIA: return N_FAMILIA;
    case CARTA_VIAJERO: return N_VIAJERO;
    default: return N_INVITACION;
    }
}
static const char *const CARTA_SALUDO[4] = {"¡Hola, {N}!", "Querid{o} {N}:", "¡Hola, hola, {N}!", "Para {N}, con cariño:"};
static const char *const CARTA_FIRMA[4] = {"Con cariño,", "Un abrazo grande,", "Hasta pronto,", "Tu amistad del bosque,"};
static const char *const CARTA_PD[4] = {"P.D.: Saluda de mi parte a quien te cuida.", "P.D.: Te mando un beso en la nariz.",
                                        "P.D.: Escríbeme con el viento, que yo lo escucho.", "P.D.: ¡No te olvides de mí!"};
static const char *const FAMILIA_SALUDO[4] = {"Mi querid{o} {N}:", "¡Hola, mi tesoro!", "Para mi {N}, con todo mi amor:", "Mi pequeñ{o} {N}:"};
static const char *const FAMILIA_FIRMA[4] = {"Te quiere,", "Con todo mi amor,", "Un abrazo de oso,", "Siempre contigo,"};
static const char *const VIAJERO_SALUDO[4] = {"¡Hola, mi persona favorita!", "Para quien me cuidó tanto:", "¡Hola desde muy lejos!", "Querida persona de mi corazón:"};
static const char *const VIAJERO_FIRMA[4] = {"Con todo mi cariño,", "Desde mi viaje,", "Te extraña,", "Un abrazo que vuela,"};
static const char *const VIAJERO_PD[4] = {"P.D.: Dale un abrazo a {N} de mi parte.", "P.D.: Dile a {N} que lo hace muy bien.",
                                          "P.D.: Cuida mucho a {N}, ¿ya?", "P.D.: Saludos a {N}. ¡Que juegue mucho!"};

static void programar_carta(pet_t *p, uint32_t t)
{
    p->carta_dia = dia_local(p, t) + 2 + (int32_t)(rng_next(p) % 3u); /* cada 2 a 4 días */
    p->carta_hora = (uint8_t)(10 + rng_next(p) % 9u);                 /* entre las 10 y las 18 */
}

static int contar_bits(uint32_t v) { int n = 0; while (v) { n += (int)(v & 1u); v >>= 1; } return n; }
/* el i-ésimo amigo que conoce, distinto de "salvo" (o -1) */
static int amigo_conocido(const pet_t *p, int k, int salvo)
{
    for (int i = 0; i < pet_friends_max(p); i++) {
        if (!((p->vecinos_vistos >> i) & 1u) || i == salvo) continue;
        if (k-- == 0) return i;
    }
    return -1;
}

/* guarda la carta nueva (la más nueva primero) y avisa */
static void carta_guardar(pet_t *p, int de, int cual, uint32_t t)
{
    memmove(&p->cartas[1], &p->cartas[0], sizeof p->cartas[0] * (PET_CARTAS - 1));
    memmove(&p->cartas_var[1], &p->cartas_var[0], PET_CARTAS - 1);
    p->cartas[0][0] = (uint8_t)de;
    p->cartas[0][1] = (uint8_t)cual;
    p->cartas_var[0] = (uint8_t)((rng_next(p) % 4u) | (chance(p, 0.3f) ? 0x80 : 0));
    if (p->cartas_n < PET_CARTAS) p->cartas_n++;
    p->carta_pend = 1;
    push_event(p, EV_CARTA, de, 0, t);
}
static int criatura_invitacion(pet_t *p, magica_t c);
static bool zona_abierta_criatura(const pet_t *p, magica_t c);

bool pet_letter_now_from(pet_t *p, carta_de_t de, uint32_t t)
{
    if (de == CARTA_AMIGO) {
        int n = contar_bits(p->vecinos_vistos);
        if (!n) return false;
        int quien = amigo_conocido(p, (int)(rng_next(p) % (uint32_t)n), -1);
        if (quien < 0) return false;
        estacion_t es = pet_season(p, t);
        uint64_t ok = 0;
        for (int i = 0; i < N_CARTAS; i++) { /* las que calzan con la estación y con cuántos amigos conoce */
            uint8_t ti = carta_amigo(i)->tipo;
            if (ti >= CT_PRIMAVERA && ti <= CT_INVIERNO && ti - CT_PRIMAVERA != (int)es) continue;
            if (ti == CT_OTRO_AMIGO && n < 2) continue;
            ok |= 1ull << i;
        }
        int elegido = bolsa_tomar(p, &p->bolsa_carta, N_CARTAS, ok);
        if (elegido < 0) elegido = 0;
        carta_guardar(p, CARTA_AMIGO << 6 | quien, elegido, t);
    } else if (de == CARTA_FAMILIA) {
        if (!p->padre[0]) return false;
        int k = bolsa_tomar(p, &p->bolsa_familia, N_FAMILIA, ~0ull);
        carta_guardar(p, CARTA_FAMILIA << 6, k, t);
    } else if (de == CARTA_VIAJERO) {
        if (!p->viajeros_n) return false;
        int v = (int)(rng_next(p) % (uint32_t)p->viajeros_n);
        viajero_t *vi = &p->viajeros[v];
        int n = vi->cartas, cuerpo;
        if (n < 10) cuerpo = (vi->vehiculo & 3) * 10 + n;                       /* su viaje, en orden */
        else cuerpo = n % 3 == 0 ? 40 + (n / 3) % 5 : bolsa_tomar(p, &p->bolsa_viajero, 40, 0x3FFull << ((vi->vehiculo & 3) * 10));
        if (vi->cartas < 255) vi->cartas++;
        carta_guardar(p, CARTA_VIAJERO << 6 | v, cuerpo, t);
    } else {
        int c = -1, n = 0;
        for (int k = 0; k < CRI_COUNT; k++) if (zona_abierta_criatura(p, (magica_t)k) && rng_next(p) % (uint32_t)(++n) == 0) c = k;
        if (c < 0) return false;
        return pet_creature_invite_now(p, (magica_t)c, t);
    }
    return true;
}

/* quién escribe esta vez: los amigos casi siempre; la familia y los viajeros, de vez en cuando; las criaturas tienen su propio turno */
bool pet_letter_now(pet_t *p, uint32_t t)
{
    float r = rng01(p);
    if (p->viajeros_n && r < 0.28f && pet_letter_now_from(p, CARTA_VIAJERO, t)) { programar_carta(p, t); return true; }
    if (p->padre[0] && r >= 0.28f && r < 0.5f && pet_letter_now_from(p, CARTA_FAMILIA, t)) { programar_carta(p, t); return true; }
    if (pet_letter_now_from(p, CARTA_AMIGO, t)) { programar_carta(p, t); return true; }
    if (pet_letter_now_from(p, CARTA_VIAJERO, t) || pet_letter_now_from(p, CARTA_FAMILIA, t)) { programar_carta(p, t); return true; }
    return false;
}

static void paso_cartas(pet_t *p, uint32_t t, bool vivo)
{
    if ((!p->vecinos_vistos && !p->viajeros_n && !p->padre[0]) || p->carta_pend || pet_mode(p) == MODO_INFANTE || p->etapa == STAGE_HUEVO) return;
    if (!p->carta_dia) { programar_carta(p, t); return; }
    if (dia_local(p, t) < p->carta_dia || hora_local(p, t) < p->carta_hora || hora_local(p, t) >= 21 || p->dormida) return;
    if (pet_letter_now(p, t) && vivo) sonar(p, SND_TESORO, t);
}

bool pet_letter_pending(const pet_t *p) { return p->carta_pend && p->cartas_n; }
int pet_letter_count(const pet_t *p) { return p->cartas_n < PET_CARTAS ? p->cartas_n : PET_CARTAS; }
carta_de_t pet_letter_from(const pet_t *p, int i)
{
    if (i < 0 || i >= pet_letter_count(p)) return CARTA_AMIGO;
    return (carta_de_t)(p->cartas[i][0] >> 6);
}
mensajero_t pet_letter_messenger(const pet_t *p, int i, uint32_t t)
{
    static const mensajero_t SENA[CRI_COUNT] = {MENS_HOJA, MENS_CHISPA, MENS_BURBUJA, MENS_VIENTO, MENS_CRISTAL, MENS_CARACOLA};
    switch (pet_letter_from(p, i)) {
    case CARTA_FAMILIA: return MENS_MARIPOSA;
    case CARTA_VIAJERO: return MENS_GRULLA;
    case CARTA_CRIATURA: return SENA[(p->cartas[i][0] & 63) % CRI_COUNT];
    default: { int h = hora_local(p, t); return (h >= 19 || h < 7) ? MENS_BUHO : MENS_PALOMA; }
    }
}

/* copia "s" en "o" cambiando {N}, {A}, {o}, {P} (quien la dejó), {p} (a/o de mamá o papá) y {V} (la viajera) */
static int carta_poner(const pet_t *p, char *o, int n, int k, const char *s, int otro, const char *viajera)
{
    while (*s && k < n - 1) {
        const char *rep = NULL;
        if (s[0] == '{' && s[1] && s[2] == '}') {
            if (s[1] == 'N') rep = p->nombre;
            else if (s[1] == 'A') rep = otro >= 0 ? pet_friend_name(p, otro) : "un amigo";
            else if (s[1] == 'o') rep = pet_g(p, "a", "o"); /* (también lo hace genero_en, pero así se ve aquí) */
            else if (s[1] == 'P') rep = p->padre;
            else if (s[1] == 'p') rep = p->padre_genero ? "o" : "a";
            else if (s[1] == 'V') rep = viajera ? viajera : "";
        }
        if (rep) { while (*rep && k < n - 1) o[k++] = *rep++; s += 3; }
        else o[k++] = *s++;
    }
    o[k] = 0;
    return k;
}

bool pet_letter_get(const pet_t *p, int i, char *de, int nde, char *txt, int ntxt)
{
    if (i < 0 || i >= pet_letter_count(p)) return false;
    const int fuente = p->cartas[i][0] >> 6, quien = p->cartas[i][0] & 63, cuerpo = p->cartas[i][1];
    const int var = p->cartas_var[i] & 3, pd = p->cartas_var[i] & 0x80;
    char firma_de[40];
    const char *saludo, *texto, *firma, *posd = NULL, *viajera = NULL;
    int otro = -1;
    switch (fuente) {
    case CARTA_FAMILIA:
        snprintf(firma_de, sizeof firma_de, "%s %s", p->padre_genero ? "Tu papá," : "Tu mamá,", p->padre);
        saludo = FAMILIA_SALUDO[var]; texto = CARTAS_FAMILIA[cuerpo % N_FAMILIA]; firma = FAMILIA_FIRMA[(var + cuerpo) & 3];
        if (pd) posd = "P.D.: Pórtate bien y haz caso a quien te cuida.";
        break;
    case CARTA_VIAJERO: {
        const viajero_t *v = &p->viajeros[quien % PET_VIAJEROS];
        viajera = v->nombre;
        snprintf(firma_de, sizeof firma_de, "%s", v->nombre[0] ? v->nombre : "Tu viajera");
        saludo = VIAJERO_SALUDO[var]; texto = CARTAS_VIAJERO[cuerpo % N_VIAJERO]; firma = VIAJERO_FIRMA[(var + cuerpo) & 3];
        posd = VIAJERO_PD[(cuerpo + quien) & 3]; /* siempre saluda a la de ahora */
        break;
    }
    case CARTA_CRIATURA: {
        const invitacion_t *in = &INVITACIONES[cuerpo % N_INVITACION];
        const int cr = in->criatura % CRI_COUNT;
        snprintf(firma_de, sizeof firma_de, "%s", CRIATURA_NOMBRE[cr]);
        saludo = CARTA_CRI_SALUDO[cr]; texto = in->txt; firma = CARTA_CRI_FIRMA[cr];
        posd = CARTA_CRI_PD[cr][pet_story_act(p) - 1]; /* la P.D. deja caer una pista del acto en que van */
        break;
    }
    default: {
        int q = quien >= pet_friends_max(p) ? 0 : quien;
        snprintf(firma_de, sizeof firma_de, "%s", pet_friend_name(p, q));
        int nn = contar_bits(p->vecinos_vistos & ~(1u << q));
        if (nn) otro = amigo_conocido(p, (q + cuerpo) % nn, q); /* siempre el mismo para esta carta */
        saludo = CARTA_SALUDO[var]; texto = carta_amigo(cuerpo % N_CARTAS)->txt; firma = CARTA_FIRMA[(var + cuerpo) & 3];
        if (pd) posd = CARTA_PD[(cuerpo + q) & 3];
        break;
    }
    }
    if (de && nde > 0) snprintf(de, (size_t)nde, "%s", firma_de);
    if (!txt || ntxt <= 0) return true;
    int k = carta_poner(p, txt, ntxt, 0, saludo, otro, viajera);
    k = carta_poner(p, txt, ntxt, k, "\n\n", otro, viajera);
    k = carta_poner(p, txt, ntxt, k, texto, otro, viajera);
    k = carta_poner(p, txt, ntxt, k, "\n\n", otro, viajera);
    k = carta_poner(p, txt, ntxt, k, firma, otro, viajera);
    k = carta_poner(p, txt, ntxt, k, "\n", otro, viajera);
    k = carta_poner(p, txt, ntxt, k, firma_de, otro, viajera);
    if (posd) {
        k = carta_poner(p, txt, ntxt, k, "\n\n", otro, viajera);
        carta_poner(p, txt, ntxt, k, posd, otro, viajera);
    }
    genero_en(p, txt, ntxt);
    return true;
}

void pet_letter_open(pet_t *p, uint32_t t)
{
    if (!pet_letter_pending(p)) return;
    p->carta_pend = 0;
    const int fuente = p->cartas[0][0] >> 6;
    impulsar(p, EMO_ALEGRIA, 0.7f, CAUSA_CARTA);
    impulsar(p, EMO_CARINO, fuente == CARTA_FAMILIA ? 0.8f : 0.4f, CAUSA_CARTA);
    sumar(p, NEED_CURIOSIDAD, 8);
    if (fuente == CARTA_AMIGO && p->vecino.amistad < 100) p->vecino.amistad = clampf(p->vecino.amistad + 2, 0, 100);
    if (fuente == CARTA_CRIATURA && p->cita_criatura) { /* la leíste: hoy los espera */
        p->cita_estado = 1;
        p->cita_dia = (uint8_t)(1 + (dia_local(p, t) & 0xFF));
    }
    anotar(p, DIA_CARTA, (int8_t)p->cartas[0][0], 0, t);
}

/* ======================================================================
 * Las que se fueron de viaje (versión 23)
 * ==================================================================== */
void pet_add_traveller(pet_t *p, const pet_t *v, int vehiculo, uint32_t t)
{
    (void)t;
    if (!v || !v->nombre[0]) return;
    memmove(&p->viajeros[1], &p->viajeros[0], sizeof p->viajeros[0] * (PET_VIAJEROS - 1));
    viajero_t *n = &p->viajeros[0];
    memset(n, 0, sizeof *n);
    snprintf(n->nombre, sizeof n->nombre, "%s", v->nombre);
    n->especie = v->especie;
    n->genero = v->genero;
    n->vehiculo = (uint8_t)(vehiculo & 3);
    if (p->viajeros_n < PET_VIAJEROS) p->viajeros_n++;
    /* las cartas que eran de ella se quedan (son tuyas también) pero las de la viajera que se iba se corren */
    for (int i = 0; i < PET_CARTAS; i++)
        if ((p->cartas[i][0] >> 6) == CARTA_VIAJERO) {
            int q = (p->cartas[i][0] & 63) + 1;
            p->cartas[i][0] = (uint8_t)(CARTA_VIAJERO << 6 | (q < PET_VIAJEROS ? q : PET_VIAJEROS - 1));
        }
    if (!p->carta_dia) programar_carta(p, t);
}
int pet_travellers(const pet_t *p) { return p->viajeros_n; }

/* ======================================================================
 * Los animales del bosque (versión 22): cada uno con su manera de ser
 * ==================================================================== */
static uint32_t *animal_hasta(pet_t *p, int a) { return a < ANIMAL_V22 ? &p->animal_hasta[a] : &p->animal_hasta2[a - ANIMAL_V22]; }
static uint32_t animal_hasta_de(const pet_t *p, int a) { return a < ANIMAL_V22 ? p->animal_hasta[a] : p->animal_hasta2[a - ANIMAL_V22]; }
bool pet_animal_here(const pet_t *p, animal_t a, uint32_t t) { return (unsigned)a < ANIMAL_COUNT && t < animal_hasta_de(p, a); }
const char *pet_animal_name(animal_t a)
{
    if ((unsigned)a >= ANIMAL_COUNT) return "?";
    return FICHA_NOMBRE[FAUNA_COUNT + a];
}
const char *pet_animal_doing(animal_t a) { return (unsigned)a >= ANIMAL_V22 && (unsigned)a < ANIMAL_COUNT ? ANIMAL_HACE[a - ANIMAL_V22] : ""; }

/* los de la versión 23: dónde viven (bits de escena_t), a qué horas, en qué estaciones y cuán seguido vienen */
enum { Z_P = 1 << ESC_PRADO, Z_B = 1 << ESC_BOSQUE, Z_L = 1 << ESC_LAGO, Z_C = 1 << ESC_COLINA, Z_V = 1 << ESC_CUEVA, Z_I = 1 << ESC_ISLA, Z_K = 1 << ESC_CLARO, Z_M = 1 << ESC_MIRADOR }; /* (v41) Z_K el claro de Tilo, Z_M el mirador */
enum { H_DIA = 0, H_NOCHE, H_CREPUSCULO, H_SIEMPRE };
enum { S_TODAS = 0, S_NO_INVIERNO, S_CALOR, S_OTONO, S_FRIO };
enum { C_NADA = 0, C_SECO, C_SOL, C_CHARCOS };
typedef struct { uint8_t zonas, horas, estaciones, clima; float por_hora; uint8_t min, mas; } animal_vive_t;
static const animal_vive_t VIVE[ANIMAL_COUNT - ANIMAL_V22] = {
    {Z_P | Z_B | Z_C | Z_K, H_DIA, S_CALOR, C_SECO, 0.25f, 8, 10},        /* colibrí */
    {Z_B | Z_P | Z_K, H_DIA, S_TODAS, C_SECO, 0.25f, 15, 15},             /* carpintero */
    {Z_L | Z_I, H_DIA, S_TODAS, C_SECO, 0.25f, 12, 12},             /* martín pescador */
    {Z_P | Z_L | Z_C | Z_M, H_DIA, S_CALOR, C_SECO, 0.3f, 10, 15},        /* golondrina */
    {Z_L | Z_I, H_DIA, S_TODAS, C_NADA, 0.2f, 25, 20},              /* cisne */
    {Z_P | Z_B, H_DIA, S_TODAS, C_SECO, 0.25f, 12, 12},             /* urraca */
    {Z_P | Z_B | Z_C | Z_M, H_DIA, S_TODAS, C_NADA, 0.25f, 12, 15},       /* cuervo */
    {Z_L, H_DIA, S_TODAS, C_SECO, 0.2f, 20, 20},                    /* garza */
    {Z_B | Z_P, H_NOCHE, S_TODAS, C_SECO, 0.25f, 15, 15},           /* mapache */
    {Z_V, H_SIEMPRE, S_TODAS, C_NADA, 0.4f, 15, 15},                /* murciélago (y en el prado al atardecer, ver abajo) */
    {Z_L, H_DIA, S_NO_INVIERNO, C_NADA, 0.25f, 15, 15},             /* nutria */
    {Z_L, H_DIA, S_NO_INVIERNO, C_SECO, 0.2f, 20, 15},              /* castor */
    {Z_P | Z_C, H_SIEMPRE, S_TODAS, C_SECO, 0.25f, 8, 10},          /* ratón de campo */
    {Z_B, H_DIA, S_OTONO, C_SECO, 0.08f, 15, 10},                   /* oso (raro, en otoño) */
    {Z_B | Z_K, H_CREPUSCULO, S_TODAS, C_SECO, 0.25f, 12, 12},            /* ciervo */
    {Z_B | Z_M, H_NOCHE, S_NO_INVIERNO, C_SECO, 0.2f, 12, 12},            /* tejón */
    {Z_P | Z_C, H_NOCHE, S_NO_INVIERNO, C_SECO, 0.15f, 10, 10},     /* zorrillo */
    {Z_P | Z_B | Z_C, H_CREPUSCULO, S_TODAS, C_SECO, 0.07f, 8, 8},  /* gato montés (raro) */
    {Z_C | Z_M, H_DIA, S_TODAS, C_SECO, 0.06f, 8, 8},                     /* lince (raro) */
    {Z_C | Z_M, H_CREPUSCULO, S_TODAS, C_SECO, 0.05f, 8, 8},              /* puma (raro) */
    {Z_C | Z_P | Z_M, H_DIA, S_NO_INVIERNO, C_SOL, 0.35f, 10, 15},        /* lagartija */
    {Z_P | Z_B | Z_K, H_SIEMPRE, S_NO_INVIERNO, C_CHARCOS, 0.5f, 15, 15}, /* salamandra (después de la lluvia) */
    {Z_C, H_DIA, S_NO_INVIERNO, C_SECO, 0.2f, 15, 15},              /* camaleón */
    {Z_P | Z_C, H_DIA, S_NO_INVIERNO, C_SOL, 0.2f, 8, 8},           /* culebrita verde */
    {Z_P | Z_B | Z_C | Z_K, H_DIA, S_CALOR, C_SECO, 0.35f, 10, 10},       /* mariquita */
    {Z_L | Z_P | Z_K, H_DIA, S_CALOR, C_SECO, 0.35f, 10, 10},             /* libélula */
    {Z_P | Z_C, H_NOCHE, S_NO_INVIERNO, C_SECO, 0.4f, 20, 20},      /* grillo */
    {Z_P | Z_B | Z_C, H_DIA, S_NO_INVIERNO, C_SECO, 0.35f, 15, 15}, /* hormigas */
    {Z_B | Z_P, H_CREPUSCULO, S_NO_INVIERNO, C_SECO, 0.3f, 12, 12}, /* escarabajo */
    {Z_P | Z_B | Z_K, H_NOCHE, S_NO_INVIERNO, C_SECO, 0.35f, 12, 12},     /* polilla */
    {Z_L, H_DIA, S_FRIO, C_NADA, 0.3f, 20, 20},                     /* pingüino (en el hielo del lago) */
    {Z_L, H_DIA, S_FRIO, C_NADA, 0.22f, 20, 20},                    /* foca */
    {Z_L, H_DIA, S_FRIO, C_NADA, 0.16f, 12, 10},                    /* osezno polar (raro) */
};

/* dónde y cuándo vive cada uno */
static bool animal_puede(const pet_t *p, animal_t a, uint32_t t)
{
    const int h = hora_local(p, t);
    const escena_t e = (escena_t)p->escena;
    const clima_t c = pet_weather(p);
    const bool lluvia = c == CLIMA_LLUVIA || c == CLIMA_TORMENTA, dia = h >= 8 && h < 19;
    if (a >= ANIMAL_V22 && a < ANIMAL_COUNT) {
        const animal_vive_t *v = &VIVE[a - ANIMAL_V22];
        bool zona = (v->zonas >> e) & 1u;
        if (a == ANIMAL_MURCIELAGO && e == ESC_PRADO && h >= 19 && h < 21) zona = true; /* cruza el prado al atardecer */
        if (!zona) return false;
        bool hora;
        switch (v->horas) {
        case H_DIA: hora = dia; break;
        case H_NOCHE: hora = h >= 20 || h < 5; break;
        case H_CREPUSCULO: hora = (h >= 6 && h < 9) || (h >= 17 && h < 21); break;
        default: hora = a != ANIMAL_MURCIELAGO || e == ESC_CUEVA || (h >= 19 && h < 21); break;
        }
        if (!hora) return false;
        const estacion_t es = pet_season(p, t);
        if (v->estaciones == S_NO_INVIERNO && es == ESTACION_INVIERNO) return false;
        if (v->estaciones == S_CALOR && es != ESTACION_PRIMAVERA && es != ESTACION_VERANO) return false;
        if (v->estaciones == S_OTONO && es != ESTACION_OTONO) return false;
        if (v->estaciones == S_FRIO && es != ESTACION_INVIERNO && c != CLIMA_NIEVE && c != CLIMA_FRIO) return false; /* solo con frío o nieve */
        if (c == CLIMA_TORMENTA && e != ESC_CUEVA) return false;
        if (v->clima == C_SECO && lluvia && e != ESC_CUEVA) return false;
        if (v->clima == C_SOL && c != CLIMA_DESPEJADO) return false;
        if (v->clima == C_CHARCOS && (!pet_puddles(p, t) || lluvia)) return false;
        return true;
    }
    switch (a) {
    case ANIMAL_ERIZO: return (e == ESC_PRADO || e == ESC_BOSQUE) && ((h >= 17 && h < 22) || (h >= 5 && h < 8)) && !lluvia;
    case ANIMAL_ARDILLA: return (e == ESC_BOSQUE || e == ESC_PRADO || e == ESC_CLARO) && dia && !lluvia && pet_season(p, t) != ESTACION_INVIERNO;
    case ANIMAL_CONEJO: return (e == ESC_PRADO || e == ESC_COLINA || e == ESC_MIRADOR) && h >= 7 && h < 20 && c != CLIMA_TORMENTA;
    case ANIMAL_CARACOL: return (e == ESC_PRADO || e == ESC_BOSQUE || e == ESC_CLARO) && pet_puddles(p, t) && !lluvia;
    case ANIMAL_TORTUGA: return (e == ESC_LAGO || e == ESC_ISLA) && h >= 9 && h < 18 && !lluvia && pet_season(p, t) != ESTACION_INVIERNO;
    case ANIMAL_PATO: return e == ESC_LAGO && dia && pet_season(p, t) != ESTACION_INVIERNO && c != CLIMA_TORMENTA;
    case ANIMAL_BUHO: return (e == ESC_PRADO || e == ESC_BOSQUE || e == ESC_CLARO) && (h >= 20 || h < 5) && c != CLIMA_TORMENTA;
    case ANIMAL_TOPO: return e == ESC_COLINA && dia && !lluvia;
    default: return false;
    }
}
/* los animales usan su propio azar (un hash del minuto), así no cambian la suerte de lo demás */
static float azar_animal(const pet_t *p, uint32_t t, int i, int sal)
{
    uint32_t h = (t / 60u) * 2654435761u ^ (uint32_t)(i * 7919 + sal * 104729) ^ p->id;
    h ^= h >> 15; h *= 2246822519u; h ^= h >> 13; h *= 3266489917u; h ^= h >> 16;
    return (float)(h & 0xFFFFFF) / 16777216.0f;
}
/* (v99) el azar de los animales cambia una vez por minuto: se sortea una vez por cada minuto que cruza el paso, con la
 * probabilidad de esos minutos. Antes se sorteaba en cada vuelta con la probabilidad de la vuelta y el mismo número: con la
 * placa avanzando varias veces por segundo, los animales venían hasta 60 veces menos que de un salto (prueba P0). */
static float prob_minutos(float por_hora, uint32_t t, float dt)
{
    const uint32_t desde = t > (uint32_t)dt ? t - (uint32_t)dt : 0u;
    const uint32_t m = t / 60u - desde / 60u;
    return m ? prob_paso(por_hora, (float)m * 60.0f) : 0.0f;
}
static void animal_llega(pet_t *p, animal_t a, uint32_t t, bool vivo)
{
    static const uint16_t MIN[ANIMAL_V22] = {20, 15, 20, 30, 20, 20, 60, 15}, MAS[ANIMAL_V22] = {20, 15, 20, 30, 10, 20, 60, 15};
    uint32_t mi = a < ANIMAL_V22 ? MIN[a] : VIVE[a - ANIMAL_V22].min, ma = a < ANIMAL_V22 ? MAS[a] : VIVE[a - ANIMAL_V22].mas;
    *animal_hasta(p, a) = t + 60u * (mi + (uint32_t)(azar_animal(p, t, a, 4) * (float)(ma + 1u)));
    push_event(p, EV_ANIMAL, a, ANI_LLEGA, t);
    sumar(p, NEED_CURIOSIDAD, 6);
    if (!p->dormida) impulsar(p, EMO_CURIOSIDAD, 0.4f, CAUSA_NOVEDAD);
    if (a == ANIMAL_ARDILLA && !p->ardilla_semilla && p->inventario[ITEM_SEMILLA] >= 3) { /* traviesa: se lleva una semilla */
        int c = -1, n = 0;
        for (int k = 0; k < CULTIVO_COUNT; k++) if (p->semillas[k] && azar_animal(p, t, k, 5) * (float)(++n) < 1.0f) c = k;
        if (c >= 0 && gastar_semilla(p, c)) {
            p->ardilla_semilla = (uint8_t)(c + 1);
            p->ardilla_vuelve = t + 2u * 3600u + (uint32_t)(azar_animal(p, t, a, 6) * 4.0f * 3600.0f);
            push_event(p, EV_ANIMAL, a, ANI_ROBA, t);
        }
    }
    if (a == ANIMAL_CARACOL) push_event(p, EV_ANIMAL, a, ANI_RASTRO, t);
    if (a == ANIMAL_BUHO && vivo) sonar(p, SND_LLAMADO, t);
}
static void animal_se_va(pet_t *p, animal_t a, uint32_t t, animal_suceso_t por)
{
    *animal_hasta(p, a) = 0;
    push_event(p, EV_ANIMAL, a, por, t);
}
/* lo que hace uno de los nuevos cuando está con ella */
static void animal_hace(pet_t *p, animal_t a, uint32_t t, bool vivo)
{
    if (p->dormida) return;
    push_event(p, EV_ANIMAL, a, ANI_HACE, t);
    sumar(p, NEED_CURIOSIDAD, 4);
    impulsar(p, EMO_CURIOSIDAD, 0.3f, CAUSA_NOVEDAD);
    switch (a) {
    case ANIMAL_CUERVO: guardar_item(p, ITEM_PIEDRA); impulsar(p, EMO_ALEGRIA, 0.5f, CAUSA_HALLAZGO); break; /* le trae una piedrita */
    case ANIMAL_MARIQUITA: impulsar(p, EMO_ALEGRIA, 0.5f, CAUSA_NOVEDAD); break;
    case ANIMAL_GRILLO: case ANIMAL_POLILLA: impulsar(p, EMO_CANSANCIO, 0.2f, CAUSA_SUENO); sumar(p, NEED_SEGURIDAD, 4); break;
    case ANIMAL_CISNE: case ANIMAL_GARZA: sumar(p, NEED_SEGURIDAD, 6); break;
    case ANIMAL_PINGUINO: case ANIMAL_FOCA: sumar(p, NEED_DIVERSION, 6); impulsar(p, EMO_ALEGRIA, 0.4f, CAUSA_NOVEDAD); break;
    case ANIMAL_NUTRIA: case ANIMAL_GOLONDRINA: case ANIMAL_COLIBRI: sumar(p, NEED_DIVERSION, 6); impulsar(p, EMO_ALEGRIA, 0.4f, CAUSA_NOVEDAD); break;
    case ANIMAL_ZORRILLO: case ANIMAL_PUMA: case ANIMAL_OSO: impulsar(p, EMO_MIEDO, 0.1f, CAUSA_NOVEDAD); break;
    default: break;
    }
    if (vivo && (a == ANIMAL_GRILLO || a == ANIMAL_CUERVO)) sonar(p, a == ANIMAL_CUERVO ? SND_TESORO : SND_LLAMADO, t);
}
static int nuevos_aqui(const pet_t *p, uint32_t t)
{
    int n = 0;
    for (int i = ANIMAL_V22; i < ANIMAL_COUNT; i++) if (pet_animal_here(p, (animal_t)i, t)) n++;
    return n;
}
static void paso_animales(pet_t *p, uint32_t t, float dt, bool vivo)
{
    if (p->etapa == STAGE_HUEVO) return;
    static const float POR_HORA[ANIMAL_V22] = {0.3f, 0.3f, 0.3f, 0.6f, 0.06f, 0.35f, 0.4f, 0.35f};
    const uint8_t hoy = (uint8_t)(1 + (dia_local(p, t) & 0xFF));
    const bool quieta = !p->dormida && (p->actividad == ACTV_MIRAR || p->actividad == ACTV_ESPERAR || p->actividad == ACTV_SIESTA ||
                                        p->actividad == ACTV_SOMBRA || p->actividad == ACTV_OLFATEAR);
    const bool corre = !p->dormida && (p->actividad == ACTV_PERSEGUIR || p->actividad == ACTV_JUGAR_SOLA || p->actividad == ACTV_JUGUETE);
    int nuevos = nuevos_aqui(p, t);
    bool faltan = false; /* ¿queda alguno que pueda venir ahora y todavía no vino en esta vuelta? */
    for (int i = 0; i < ANIMAL_COUNT && i < 64; i++)
        if (!((p->bolsa_animal >> i) & 1ull) && !pet_animal_here(p, (animal_t)i, t) && animal_puede(p, (animal_t)i, t)) { faltan = true; break; }
    if (!faltan) p->bolsa_animal = 0; /* vinieron todos los de aquí y ahora: otra vuelta */
    for (int i = 0; i < ANIMAL_COUNT; i++) {
        animal_t a = (animal_t)i;
        bool aqui = pet_animal_here(p, a, t), puede = animal_puede(p, a, t);
        if (aqui && !puede) { /* se fue la condición: el conejo se espanta con la tormenta, los demás se van */
            animal_se_va(p, a, t, a == ANIMAL_CONEJO && pet_weather(p) == CLIMA_TORMENTA ? ANI_SE_ASUSTA : ANI_SE_VA);
            if (i >= ANIMAL_V22) nuevos--;
            continue;
        }
        if (aqui && t + (uint32_t)dt >= animal_hasta_de(p, a)) { animal_se_va(p, a, t, ANI_SE_VA); if (i >= ANIMAL_V22) nuevos--; continue; }
        if (!aqui) {
            if (i >= ANIMAL_V22 && nuevos >= 2) continue; /* de los nuevos, dos a la vez como mucho */
            float ph = i < ANIMAL_V22 ? POR_HORA[i] : VIVE[i - ANIMAL_V22].por_hora;
            /* de noche vienen más seguido los animales nocturnos (búho, murciélago, grillo, polilla, mapache...): el bosque de noche está vivo */
            if (puede && (hora_local(p, t) >= 20 || hora_local(p, t) < 5) && (a == ANIMAL_BUHO || a == ANIMAL_MURCIELAGO || (i >= ANIMAL_V22 && VIVE[i - ANIMAL_V22].horas == H_NOCHE))) ph *= PET_NOCTURNOS_X;
            /* sin repetir hasta que vinieron todos los que pueden venir aquí y ahora: los que ya vinieron en esta vuelta vienen mucho menos */
            if (puede && ((p->bolsa_animal >> i) & 1ull) && faltan) ph *= 0.15f;
            if (ESC_ES_SECRETA(p->escena)) ph *= 2.5f; /* (v98) en el claro del rey y en el mirador se ve más vida: son lugares cortos de visitar */
            if (puede && azar_animal(p, t, i, 1) < prob_minutos(ph, t, dt)) {
                animal_llega(p, a, t, vivo);
                p->bolsa_animal |= 1ull << i;
                if (i >= ANIMAL_V22) nuevos++;
            }
            continue;
        }
        /* mientras está: lo que hace con ella (cada tanto) */
        if (azar_animal(p, t, i, 2) >= prob_minutos(0.7f, t, dt)) continue; /* más o menos una vez por hora */
        if (i >= ANIMAL_V22) { animal_hace(p, a, t, vivo); continue; }
        switch (a) {
        case ANIMAL_ERIZO:
            if (corre) push_event(p, EV_ANIMAL, a, ANI_BOLITA, t);
            else if (quieta && p->erizo_confianza < 3) {
                p->erizo_confianza++;
                if (p->erizo_confianza == 3) {
                    push_event(p, EV_ANIMAL, a, ANI_AMIGOS, t);
                    anotar(p, DIA_ANIMAL, a, ANI_AMIGOS, t);
                    impulsar(p, EMO_ALEGRIA, 0.7f, CAUSA_NOVEDAD);
                    if (vivo) sonar(p, SND_ALEGRE, t);
                }
            }
            break;
        case ANIMAL_CONEJO:
            if (!p->dormida && p->necesidad[NEED_ENERGIA] > 30) {
                push_event(p, EV_ANIMAL, a, ANI_CORREN, t);
                sumar(p, NEED_DIVERSION, 10);
                impulsar(p, EMO_ALEGRIA, 0.5f, CAUSA_JUGAMOS);
                if (vivo) sonar(p, SND_RISA, t);
            }
            break;
        case ANIMAL_TORTUGA:
            if (!p->dormida && p->dia_tortuga != hoy && (p->emo[EMO_TRISTEZA] > 0.2f || p->emo[EMO_MIEDO] > 0.2f)) {
                p->dia_tortuga = hoy;
                p->emo[EMO_TRISTEZA] *= 0.2f;
                p->emo[EMO_MIEDO] *= 0.3f;
                sumar(p, NEED_SEGURIDAD, 25);
                push_event(p, EV_ANIMAL, a, ANI_CONSUELA, t);
                anotar(p, DIA_ANIMAL, a, ANI_CONSUELA, t);
            }
            break;
        case ANIMAL_PATO: if (!p->dormida) push_event(p, EV_ANIMAL, a, ANI_EN_FILA, t); break;
        case ANIMAL_BUHO:
            if (!p->dormida) { /* la arrulla: se calma y le da sueño */
                sumar(p, NEED_SEGURIDAD, 10);
                impulsar(p, EMO_CANSANCIO, 0.5f, CAUSA_SUENO);
                push_event(p, EV_ANIMAL, a, ANI_ARRULLA, t);
            } else push_event(p, EV_ANIMAL, a, ANI_ULULA, t);
            if (vivo) sonar(p, SND_LLAMADO, t);
            break;
        default: break;
        }
    }
    /* la ardilla desentierra la semilla que se llevó (a veces) */
    if (p->ardilla_semilla && t >= p->ardilla_vuelve) {
        int c = p->ardilla_semilla - 1;
        p->ardilla_semilla = 0;
        if (azar_animal(p, t, ANIMAL_ARDILLA, 3) < 0.6f && c < CULTIVO_COUNT) {
            p->semillas[c]++;
            contar_semillas(p);
            push_event(p, EV_ANIMAL, ANIMAL_ARDILLA, ANI_DEVUELVE, t);
            anotar(p, DIA_ANIMAL, ANIMAL_ARDILLA, ANI_DEVUELVE, t);
        }
    }
}
void pet_animal_call(pet_t *p, animal_t a, uint32_t t) { if ((unsigned)a < ANIMAL_COUNT) animal_llega(p, a, t, true); }

/* --- las fichas: los animales que conoció contigo --- */
int pet_card_of_fauna(fauna_t f) { return (unsigned)f < FAUNA_COUNT ? (int)f : -1; }
int pet_card_of_animal(animal_t a) { return (unsigned)a < ANIMAL_COUNT ? FAUNA_COUNT + (int)a : -1; }
const char *pet_card_name(int f) { return (unsigned)f < PET_FICHAS ? FICHA_NOMBRE[f] : "?"; }
const char *pet_card_name_a(int f, char *buf, int n) /* "al colibrí", "a la rana" */
{
    const char *nm = pet_card_name(f);
    if (!strncmp(nm, "el ", 3)) snprintf(buf, (size_t)n, "al %s", nm + 3);
    else snprintf(buf, (size_t)n, "a %s", nm);
    return buf;
}
const char *pet_card_story(int f) { return (unsigned)f < PET_FICHAS ? FICHA_HISTORIA[f] : ""; }
int pet_cards_known(const pet_t *p) { return p->fichas_n < PET_FICHAS_MAX ? p->fichas_n : PET_FICHAS_MAX; }
int pet_card_get(const pet_t *p, int i) { return i >= 0 && i < pet_cards_known(p) ? p->fichas[i] : -1; }
bool pet_animal_seen(pet_t *p, int f, uint32_t t)
{
    if ((unsigned)f >= PET_FICHAS || p->etapa == STAGE_HUEVO || p->dormida) return false; /* dormida no puede conocerlo */
    for (int i = 0; i < pet_cards_known(p); i++) if (p->fichas[i] == f) { animal_cuenta(p, f, t); return false; } /* ya lo conoce: a lo mejor le cuenta algo */
    if (p->fichas_n >= PET_FICHAS_MAX) return false;
    p->fichas[p->fichas_n++] = (uint8_t)f;
    push_event(p, EV_ANIMAL_NUEVO, f, 0, t);
    anotar(p, DIA_ANIMAL_NUEVO, f, (int)(rng_next(p) % DIA_ANIMAL_FRASES), t); /* b = cuál de las frases (así se lee igual siempre) */
    impulsar(p, EMO_ALEGRIA, 0.4f, CAUSA_NOVEDAD);
    sumar(p, NEED_CURIOSIDAD, 10);
    return true;
}

/* ======================================================================
 * Las criaturas mágicas (versión 23): una en cada zona
 * ==================================================================== */
_Static_assert(ACTV_COUNT <= 32, "gusto[] tiene 32 lugares");
_Static_assert(sizeof FICHA_NOMBRE / sizeof FICHA_NOMBRE[0] == PET_FICHAS, "una ficha por animal");
_Static_assert(sizeof FICHA_HISTORIA / sizeof FICHA_HISTORIA[0] == PET_FICHAS, "una historia por animal");
_Static_assert(PET_FICHAS <= PET_FICHAS_MAX, "caben las fichas");
_Static_assert(sizeof CARTAS_VIAJERO / sizeof CARTAS_VIAJERO[0] == 45, "diez por camino y cinco más");
static const escena_t CRIATURA_ZONA[CRI_COUNT] = {ESC_PRADO, ESC_BOSQUE, ESC_LAGO, ESC_COLINA, ESC_CUEVA, ESC_ISLA};
const char *pet_creature_name(magica_t c) { return (unsigned)c < CRI_COUNT ? CRIATURA_NOMBRE[c] : "?"; }
escena_t pet_creature_zone(magica_t c) { return (unsigned)c < CRI_COUNT ? CRIATURA_ZONA[c] : ESC_PRADO; }
const char *pet_creature_about(magica_t c) { return (unsigned)c < CRI_COUNT ? CRIATURA_SOBRE[c] : ""; }
const char *pet_cita_name(cita_t c)
{
    static const char *const T[] = {"un picnic", "plantar", "las escondidas", "bailar con las luciérnagas", "pescar", "la carrera de barquitos",
                                    "elevar la cometa", "atrapar hojas en el viento", "hacer música", "buscar un tesoro", "contar estrellas", "encender el faro"};
    return NOMBRE(T, c, CITA_COUNT);
}
int pet_creature_stories(magica_t c) { (void)c; return 5; }
int pet_creature_stories_known(const pet_t *p, magica_t c) { return (unsigned)c < CRI_COUNT ? contar_bits(p->historias[c]) : 0; }
bool pet_book_tutorial_pending(const pet_t *p, int *quien) { if (p->libro_aviso != 1) return false; if (quien) *quien = p->libro_aviso_de; return true; }
void pet_book_tutorial_done(pet_t *p) { p->libro_aviso = 2; }
static bool zona_abierta_criatura(const pet_t *p, magica_t c) { return pet_scene_open(p, CRIATURA_ZONA[c]); }
static uint8_t hoy_byte(const pet_t *p, uint32_t t) { return (uint8_t)(1 + (dia_local(p, t) & 0xFF)); }

bool pet_creature_here(const pet_t *p, uint32_t t, magica_t *c)
{
    if (!p->criatura_hasta || t >= p->criatura_hasta || p->criatura_cual >= CRI_COUNT) return false;
    if (p->escena != CRIATURA_ZONA[p->criatura_cual] || p->etapa == STAGE_HUEVO) return false;
    if (c) *c = (magica_t)p->criatura_cual;
    return true;
}
bool pet_creature_waiting(const pet_t *p, uint32_t t) { return p->criatura_espera && pet_creature_here(p, t, NULL); }
cita_t pet_creature_invite(const pet_t *p) { return (cita_t)(p->cita_que % CITA_COUNT); }

static int criatura_invitacion(pet_t *p, magica_t c)
{
    uint64_t ok = 0;
    for (int i = 0; i < N_INVITACION; i++) if (INVITACIONES[i].criatura == c) ok |= 1ull << i;
    return bolsa_tomar(p, &p->bolsa_invitacion, N_INVITACION, ok);
}
bool pet_creature_invite_now(pet_t *p, magica_t c, uint32_t t)
{
    if ((unsigned)c >= CRI_COUNT || p->carta_pend) return false;
    int k = criatura_invitacion(p, c);
    if (k < 0) return false;
    p->cita_criatura = (uint8_t)(c + 1);
    p->cita_que = INVITACIONES[k].cita;
    p->cita_estado = 0;
    p->cita_dia = 0;
    carta_guardar(p, CARTA_CRIATURA << 6 | c, k, t);
    return true;
}
static void criatura_aparece(pet_t *p, magica_t c, uint32_t t, uint32_t dura, bool espera)
{
    p->criatura_cual = (uint8_t)c;
    p->criatura_hasta = t + dura;
    p->criatura_espera = espera;
    push_event(p, EV_CRIATURA, c, espera ? CRS_ESPERA : CRS_ASOMA, t);
    impulsar(p, EMO_CURIOSIDAD, 0.6f, CAUSA_CRIATURA);
}
void pet_creature_call(pet_t *p, magica_t c, uint32_t t)
{
    if ((unsigned)c >= CRI_COUNT) return;
    bool cita = p->cita_criatura == c + 1 && p->cita_estado == 1;
    criatura_aparece(p, c, t, cita ? 2u * 3600u : 4u * 60u, cita);
}
/* le cuenta la siguiente historia de su lugar (si le quedan) */
static bool contar_historia(pet_t *p, magica_t c, uint32_t t)
{
    if (pet_mode(p) == MODO_INFANTE) return false;
    if (!momento_libre(p, t)) return false; /* hoy ya hubo dos momentos de historia: vuelven mañana */
    for (int k = 0; k < 5; k++) {
        if ((p->historias[c] >> k) & 1u) continue;
        if (!historia_abierta(p, k)) return false; /* la historia espera: esta se cuenta más adelante */
        momento_usar(p, t);
        p->historias[c] |= (uint8_t)(1u << k);
        anotar(p, DIA_HISTORIA, c, k, t);
        if (p->libro_aviso == 0) { p->libro_aviso = 1; p->libro_aviso_de = (uint8_t)c; } /* la primera: esa criatura le enseña a abrir el libro */
        push_event(p, EV_CRIATURA, c, CRS_HISTORIA | k << 4, t);
        sumar(p, NEED_CURIOSIDAD, 10);
        return true;
    }
    return false;
}
static void conocer_criatura(pet_t *p, magica_t c, uint32_t t)
{
    if ((p->criaturas_vistas >> c) & 1u) return;
    p->criaturas_vistas |= (uint8_t)(1u << c);
    anotar(p, DIA_CRIATURA, c, -1, t);
    foto(p, DIA_CRIATURA, c, t);
}
void pet_creature_touch(pet_t *p, uint32_t t)
{
    magica_t c;
    if (!pet_creature_here(p, t, &c) || p->criatura_espera) return; /* la cita la empieza la interfaz */
    bool nueva = !((p->criaturas_vistas >> c) & 1u);
    conocer_criatura(p, c, t);
    impulsar(p, EMO_ALEGRIA, 0.6f, CAUSA_CRIATURA);
    sumar(p, NEED_CARINO, 10);
    p->ultima_interaccion = t;
    (void)nueva;
    if (contar_historia(p, c, t)) { /* se queda un ratito más mientras le cuenta */
        if (p->criatura_hasta < t + 120u) p->criatura_hasta = t + 120u;
        return;
    }
    if (pet_mode(p) == MODO_INFANTE) { push_event(p, EV_CRIATURA, c, CRS_HOLA, t); return; }
    /* ya no tiene historias (las cinco, o las de hoy): bosteza y se despide */
    push_event(p, EV_CRIATURA, c, CRS_SIN_HISTORIAS | ((p->historias[c] & 0x1Fu) == 0x1Fu ? 1 << 4 : 0), t);
    if (p->criatura_hasta > t + 8u) p->criatura_hasta = t + 8u;
}
static void regalo_magico(pet_t *p, uint32_t t)
{
    int id = pet_object_find(p, OFU_REGALO, t);
    if (id >= 0) pet_object_gives(p, id, OFU_REGALO, t);
}
void pet_creature_done(pet_t *p, cita_t que, int puntos, uint32_t t)
{
    magica_t c = (magica_t)(p->criatura_cual % CRI_COUNT);
    conocer_criatura(p, c, t);
    p->criatura_espera = 0;
    p->cita_estado = 2;
    p->cita_criatura = 0;
    p->criatura_hasta = t + 90u;
    p->ultima_interaccion = t;
    impulsar(p, EMO_ALEGRIA, 1.0f, CAUSA_CRIATURA);
    impulsar(p, EMO_CARINO, 0.5f, CAUSA_CRIATURA);
    sumar(p, NEED_DIVERSION, 30);
    sumar(p, NEED_CARINO, 25);
    switch (que) {
    case CITA_PICNIC: if (pet_mode(p) != MODO_INFANTE) sumar(p, NEED_SACIEDAD, 25); else sumar(p, NEED_SACIEDAD, 10); break;
    case CITA_PLANTAR: { int k = (int)(rng_next(p) % CULTIVO_COUNT); p->semillas[k] += 2; p->semillas_vistas |= (uint8_t)(1u << k); contar_semillas(p); break; }
    case CITA_TESORO: { int pieza = (int)(rng_next(p) % CRISTALES_AZAR); if (guardar_pieza(p, COL_CRISTALES, pieza, t, true)) push_event(p, EV_TESORO, 1, COL_CRISTALES * 8 + pieza, t); break; }
    case CITA_LUCIERNAGAS: case CITA_ESTRELLAS: sumar(p, NEED_SEGURIDAD, 15); break;
    default: break;
    }
    if (puntos >= 3 && chance(p, 0.4f)) regalo_magico(p, t); /* a veces, un regalito */
    vivir(p, VIV_VECINO);
    anotar(p, DIA_CRIATURA, c, que, t);
    push_event(p, EV_CRIATURA, c, CRS_JUGARON, t);
    contar_historia(p, c, t);
    sonar(p, SND_ALEGRE, t);
}
static void paso_criaturas(pet_t *p, uint32_t t, float dt, bool vivo)
{
    (void)vivo;
    if (p->etapa == STAGE_HUEVO) return;
    const int32_t hoy = dia_local(p, t);
    const int h = hora_local(p, t);
    const uint8_t hb = hoy_byte(p, t);
    /* la cita vence al terminar el día (otro día será) */
    if (p->cita_criatura && p->cita_estado == 1 && p->cita_dia && p->cita_dia != hb) { p->cita_criatura = 0; p->cita_estado = 0; p->criatura_espera = 0; }
    /* la invitación por carta (no a los más chicos), cada uno a tres días */
    if (pet_mode(p) != MODO_INFANTE) {
        if (!p->criatura_prox) p->criatura_prox = hoy + 2 + (int32_t)(azar_animal(p, t, 91, 1) * 3.0f);
        if (hoy >= p->criatura_prox && h >= 9 && h < 17 && !p->dormida && !p->carta_pend && !p->cita_criatura) {
            uint64_t ok = 0;
            for (int k = 0; k < CRI_COUNT; k++) if (zona_abierta_criatura(p, (magica_t)k)) ok |= 1ull << k;
            uint64_t bolsa = p->bolsa_criatura;
            int c = bolsa_tomar(p, &bolsa, CRI_COUNT, ok);
            p->bolsa_criatura = (uint8_t)bolsa;
            p->criatura_prox = hoy + 1 + (int32_t)(azar_animal(p, t, 91, 3) * 3.0f);
            if (c >= 0 && pet_creature_invite_now(p, (magica_t)c, t) && vivo) sonar(p, SND_RARO, t);
        }
    }
    magica_t aqui;
    bool esta = pet_creature_here(p, t, &aqui);
    if (p->criatura_hasta && !esta) { /* se fue (o se fueron ustedes) */
        push_event(p, EV_CRIATURA, p->criatura_cual, CRS_SE_VA, t);
        p->criatura_hasta = 0;
        p->criatura_espera = 0;
        esta = false;
    }
    if (esta || p->dormida) return;
    /* llegaron a la cita: los espera */
    if (p->cita_criatura && p->cita_estado == 1 && p->cita_dia == hb && p->escena == CRIATURA_ZONA[p->cita_criatura - 1]) {
        criatura_aparece(p, (magica_t)(p->cita_criatura - 1), t, 2u * 3600u, true);
        return;
    }
    /* muy de vez en cuando se asoma sola en su zona */
    for (int c = 0; c < CRI_COUNT; c++) {
        if (CRIATURA_ZONA[c] != p->escena) continue;
        if (azar_animal(p, t, 80 + c, 7) < prob_minutos(0.05f, t, dt)) criatura_aparece(p, (magica_t)c, t, 3u * 60u + (uint32_t)(azar_animal(p, t, 80 + c, 8) * 120.0f), false);
    }
}

/* ======================================================================
 * El país y las fiestas con todos (versión 24)
 * ==================================================================== */
#include "paises.h"
int pet_countries(void) { return N_PAISES; }
const char *pet_country_name(int c) { return c > 0 && c < N_PAISES ? PAISES[c].nombre : (c == 255 ? "Otro país" : "Sin elegir"); }
void pet_set_country(pet_t *p, int c)
{
    if (c <= 0 || c >= N_PAISES) return;
    p->pais = (uint8_t)c;
    p->hemisferio_sur = PAISES[c].sur;
}
/* sin tildes y en minúsculas (para comparar lo que escribiste) */
static void sin_tildes(const char *s, char *o, int n)
{
    int k = 0;
    for (const unsigned char *u = (const unsigned char *)s; *u && k < n - 1; u++) {
        unsigned char c = *u;
        if (c == 0xC3 && u[1]) {
            unsigned char d = *++u;
            static const char T[] = "aaaaaaaceeeeiiiidnooooo/ouuuuyty"; /* 0xA0.. (y en mayúsculas 0x80..) */
            c = (unsigned char)T[(d & 0x1F)];
            if ((d & 0x1F) == 0x11) c = 'n'; /* ñ */
        } else if (c >= 'A' && c <= 'Z') c = (unsigned char)(c + 32);
        o[k++] = (char)c;
    }
    o[k] = 0;
}
int pet_country_find(const char *texto, int *res, int max)
{
    char q[32];
    sin_tildes(texto ? texto : "", q, sizeof q);
    int lq = (int)strlen(q), n = 0;
    while (lq && q[lq - 1] == ' ') q[--lq] = 0;
    if (lq < 2) return 0;
    /* primero los que empiezan así; después los que tienen una palabra que empieza así */
    for (int pasada = 0; pasada < 2; pasada++)
        for (int c = 1; c < N_PAISES && n < max; c++) {
            if (c == 13 || c == 14) continue; /* (los "otro país") */
            char nm[48];
            sin_tildes(PAISES[c].nombre, nm, sizeof nm);
            bool empieza = !strncmp(nm, q, (size_t)lq), palabra = false;
            for (const char *w = strchr(nm, ' '); w && !palabra; w = strchr(w + 1, ' ')) palabra = !strncmp(w + 1, q, (size_t)lq);
            if (pasada == 0 ? empieza : (!empieza && palabra)) { if (res) res[n] = c; n++; }
        }
    return n;
}
void pet_set_country_by_season(pet_t *p, estacion_t es, uint32_t t)
{
    int m;
    pet_local_date(p, t, NULL, &m, NULL);
    /* en el norte: primavera de marzo a mayo, verano de junio a agosto... */
    static const uint8_t NORTE[13] = {0, ESTACION_INVIERNO, ESTACION_INVIERNO, ESTACION_PRIMAVERA, ESTACION_PRIMAVERA, ESTACION_PRIMAVERA,
                                      ESTACION_VERANO, ESTACION_VERANO, ESTACION_VERANO, ESTACION_OTONO, ESTACION_OTONO, ESTACION_OTONO, ESTACION_INVIERNO};
    bool sur = NORTE[m >= 1 && m <= 12 ? m : 1] != es;
    p->pais = sur ? 13 : 14; /* "otro país" del sur o del norte */
    p->hemisferio_sur = sur;
}
bool pet_yellow_flowers_day(const pet_t *p, uint32_t t)
{
    if (p->pais <= 0 || p->pais >= N_PAISES || !PAISES[p->pais].flores) return false;
    int m, d;
    pet_local_date(p, t, NULL, &m, &d);
    return d == 21 && m == (PAISES[p->pais].flores == 1 ? 9 : 3);
}
int pet_friend_look(int i) { return (int)((unsigned)(i * 5 + 2) % N_ESPECIES) | ((i * 3 + 1) % PET_COLORES) << 7; }
int pet_party_guests(const pet_t *p, int *amigos, int max)
{
    int n = 0;
    for (int i = 0; i < pet_friends_max(p) && n < max; i++)
        if ((p->vecinos_vistos >> i) & 1u) { if (amigos) amigos[n] = i; n++; }
    return n;
}
static int fiesta_bit(int f) { return f == FIESTA_SU_CUMPLE ? 0x80 : (f == FIESTA_FLORES ? 0x01 : 1 << (f & 7)); }
int pet_party_now(const pet_t *p, uint32_t t)
{
    if (p->etapa == STAGE_HUEVO || p->dormida) return -1;
    int a, m, d;
    pet_local_date(p, t, &a, &m, &d);
    const int h = hora_local(p, t);
    int f = -1;
    if (pet_yellow_flowers_day(p, t) && h >= 9 && h < 20) f = FIESTA_FLORES; /* (aunque no tenga amigos: te trae flores a ti) */
    else if (!p->vecinos_vistos && !p->criaturas_vistas) return -1; /* sin nadie a quien invitar */
    else if (pet_her_birthday(p, t) && h >= 10 && h < 20) f = FIESTA_SU_CUMPLE;
    else switch (pet_special_date(p, t)) {
    case FECHA_NAVIDAD: if ((m == 12 && d == 24 && h >= 17) || (m == 12 && d == 25 && h >= 9 && h < 21)) f = FECHA_NAVIDAD; break;
    case FECHA_ANIO_NUEVO: if ((m == 12 && d == 31 && h >= 19) || (m == 1 && d == 1 && h < 1)) f = FECHA_ANIO_NUEVO; break;
    case FECHA_HALLOWEEN: if (h >= 17 && h < 22) f = FECHA_HALLOWEEN; break;
    case FECHA_FIESTAS_PATRIAS: if (h >= 10 && h < 20) f = FECHA_FIESTAS_PATRIAS; break;
    default: break;
    }
    if (f < 0) return -1;
    int anio = (m == 1 && f == FECHA_ANIO_NUEVO) ? a - 1 : a; /* el Año Nuevo es del año que termina */
    if (p->fiestas_anio == anio && (p->fiestas_hechas & fiesta_bit(f))) return -1;
    return f;
}
void pet_party_done(pet_t *p, int f, uint32_t t)
{
    int a, m;
    pet_local_date(p, t, &a, &m, NULL);
    int anio = (m == 1 && f == FECHA_ANIO_NUEVO) ? a - 1 : a;
    if (p->fiestas_anio != anio) { p->fiestas_anio = (int16_t)anio; p->fiestas_hechas = 0; }
    p->fiestas_hechas |= (uint8_t)fiesta_bit(f);
    p->ultima_interaccion = t;
    impulsar(p, EMO_ALEGRIA, 1.0f, CAUSA_FECHA);
    impulsar(p, EMO_CARINO, 0.7f, CAUSA_AMIGO);
    sumar(p, NEED_DIVERSION, 40);
    sumar(p, NEED_CARINO, 30);
    if (f == FECHA_FIESTAS_PATRIAS || f == FIESTA_SU_CUMPLE) sumar(p, NEED_SACIEDAD, 20); /* las empanadas, el pastel */
    if (f == FIESTA_FLORES) { impulsar(p, EMO_CARINO, 1.0f, CAUSA_FECHA); ganar_semilla(p, CULTIVO_FLOR, t); } /* te regaló flores amarillas (y le queda una semilla) */
    if (f == FECHA_NAVIDAD || f == FIESTA_SU_CUMPLE || chance(p, 0.5f)) regalo_magico(p, t); /* intercambian regalos */
    int n = pet_party_guests(p, NULL, 3) + contar_bits(p->criaturas_vistas);
    anotar(p, DIA_FIESTA, f, n, t);
    foto(p, DIA_FIESTA, f, t);
    push_event(p, EV_FIESTA, f, n, t);
}

/* ======================================================================
 * El secreto de cada zona (versión 23)
 * ==================================================================== */
static const escena_t ESPECIAL_ZONA[ESP_COUNT] = {ESC_BOSQUE, ESC_LAGO, ESC_COLINA, ESC_CUEVA};
static const uint8_t MELODIA[PET_MELODIA_LARGO] = {0, 1, 2, 1, 3}; /* azul, verde, rosado, verde, dorado */
bool pet_special_known(const pet_t *p, especial_t e) { return (unsigned)e < ESP_COUNT && ((p->especiales >> e) & 1u); }
const char *pet_constellation_name(int k) { return (unsigned)k < PET_CONSTELACIONES ? CONSTELACION[k] : ""; }
const char *pet_constellation_legend(int k) { return (unsigned)k < PET_CONSTELACIONES ? CONSTELACION_LEYENDA[k] : ""; }
const char *pet_hint_text(especial_t e, int k)
{
    switch (e) {
    case ESP_ANILLO: return PISTA_ANILLO[(unsigned)k % 3];
    case ESP_REFLEJO: return PISTA_REFLEJO[(unsigned)k % 3];
    case ESP_COMETA: return PISTA_COMETA[(unsigned)k % 3];
    case ESP_CRISTALES: return PISTA_CRISTALES[(unsigned)k % 3];
    default: return "";
    }
}
bool pet_special_ready(const pet_t *p, especial_t e, uint32_t t)
{
    if ((unsigned)e >= ESP_COUNT || p->escena != ESPECIAL_ZONA[e] || p->etapa == STAGE_HUEVO || p->dormida) return false;
    const int h = hora_local(p, t);
    const bool noche = h >= 20 || h < 6; /* como el cielo de la pantalla */
    const clima_t c = pet_weather(p);
    const bool despejado = c == CLIMA_DESPEJADO || c == CLIMA_VIENTO || c == CLIMA_FRIO;
    const float luna = pet_moon_phase(t);
    switch (e) {
    case ESP_ANILLO: return noche && despejado && luna > 0.3f && luna < 0.7f && p->especial_dia[e] != hoy_byte(p, t);
    case ESP_REFLEJO: return noche && despejado;
    case ESP_COMETA: return (c == CLIMA_VIENTO || (p->criatura_espera && p->criatura_cual == CRI_VENTOLIN)) && !noche && p->especial_dia[e] != hoy_byte(p, t);
    default: return true;
    }
}
static void descubrir_secreto(pet_t *p, especial_t e, uint32_t t)
{
    if ((p->especiales >> e) & 1u) { push_event(p, EV_ESPECIAL, e, ESS_OTRA_VEZ, t); return; }
    p->especiales |= (uint8_t)(1u << e);
    anotar(p, DIA_ESPECIAL, e, 0, t);
    tilo_pagina(p, e, t); /* una página vieja del diario de Tilo vuelve al Libro */
    foto(p, DIA_ESPECIAL, e, t);
    push_event(p, EV_ESPECIAL, e, ESS_DESCUBRE, t);
    impulsar(p, EMO_ORGULLO, 0.8f, CAUSA_ESPECIAL);
    sumar(p, NEED_CURIOSIDAD, 30);
}
void pet_special_do(pet_t *p, especial_t e, int r, uint32_t t)
{
    if ((unsigned)e >= ESP_COUNT || p->etapa == STAGE_HUEVO) return;
    p->ultima_interaccion = t;
    impulsar(p, EMO_ALEGRIA, 0.8f, CAUSA_ESPECIAL);
    sumar(p, NEED_DIVERSION, 20);
    switch (e) {
    case ESP_ANILLO: /* bailó en el anillo de hadas: las hadas le dejan algo */
        p->especial_dia[e] = hoy_byte(p, t);
        descubrir_secreto(p, e, t);
        regalo_magico(p, t);
        push_event(p, EV_ESPECIAL, e, ESS_REGALO, t);
        break;
    case ESP_REFLEJO: /* tocó una constelación del reflejo: r = cuál */
        if ((unsigned)r < PET_CONSTELACIONES) p->leyendas |= (uint8_t)(1u << r);
        descubrir_secreto(p, e, t);
        push_event(p, EV_ESPECIAL, e, ESS_LEYENDA | (r & 7) << 4, t);
        sumar(p, NEED_SEGURIDAD, 10);
        break;
    case ESP_COMETA: /* r = segundos que la mantuvo arriba */
        p->especial_dia[e] = hoy_byte(p, t);
        if (r >= 30) { descubrir_secreto(p, e, t); regalo_magico(p, t); push_event(p, EV_ESPECIAL, e, ESS_REGALO, t); }
        else push_event(p, EV_ESPECIAL, e, ESS_COMETA_CAE, t);
        break;
    default: break;
    }
    vivir(p, VIV_JUGUETE);
}
/* el juego de los cristales: rondas que ganó esta vez (0..3); la tercera despierta al dragón de cristal */
/* Pochi, la chihuahua visitante: fase 0 llega, 1 come, 2 un lugar del paseo, 3 se despide (vuelve con sus cachorros); 5.. ver PET_POCHI_F_* */
void pet_pochi(pet_t *p, int fase, uint32_t t)
{
    if (p->etapa == STAGE_HUEVO) return;
    if (fase >= PET_POCHI_F_FIN && fase < PET_POCHI_F_FIN + 4) { p->pochi_mem = (uint8_t)((p->pochi_mem & ~3u) | (unsigned)(fase - PET_POCHI_F_FIN)); return; }
    if (fase >= PET_POCHI_F_CACHORRO && fase < PET_POCHI_F_CACHORRO + PET_POCHI_CACHORROS) {
        const int k = fase - PET_POCHI_F_CACHORRO;
        p->pochi_mem |= (uint8_t)(1u << (2 + k));
        if (p->dormida) return;
        p->ultima_interaccion = t;
        if (k == POCHI_CHAPI) { sumar(p, NEED_HIGIENE, -10); sumar(p, NEED_CARINO, 6); impulsar(p, EMO_CARINO, 0.6f, CAUSA_CARICIAS); } /* el abrazo cochino */
        else if (k == POCHI_RONY) { sumar(p, NEED_DIVERSION, 8); impulsar(p, EMO_ALEGRIA, 0.6f, CAUSA_JUGAMOS); }
        else { sumar(p, NEED_CARINO, 6); impulsar(p, EMO_CARINO, 0.7f, CAUSA_CARICIAS); } /* Pasita y Morita, tiernas */
        return;
    }
    if (fase >= PET_POCHI_F_AMIGO && fase < PET_POCHI_F_AMIGO + PET_POCHI_CACHORROS) {
        p->pochi_amigos |= (uint8_t)(1u << (fase - PET_POCHI_F_AMIGO));
        if (p->dormida) return;
        p->ultima_interaccion = t;
        sumar(p, NEED_CURIOSIDAD, 5); impulsar(p, EMO_ALEGRIA, 0.4f, CAUSA_NOVEDAD);
        return;
    }
    if (fase == PET_POCHI_F_ACURRUCA) { sumar(p, NEED_SEGURIDAD, 6); sumar(p, NEED_CARINO, 3); return; } /* (dormida: no cuenta como que jugaron) */
    p->ultima_interaccion = t;
    switch (fase) {
    case 0:
        if (p->pochi_visitas < 255) p->pochi_visitas++;
        sumar(p, NEED_CURIOSIDAD, 8); impulsar(p, EMO_ALEGRIA, 0.5f, CAUSA_NOVEDAD); break;
    case PET_POCHI_F_CONSUELA: sumar(p, NEED_CARINO, 8); impulsar(p, EMO_ALEGRIA, 0.6f, CAUSA_CARICIAS); impulsar(p, EMO_CARINO, 0.6f, CAUSA_CARICIAS); break;
    case PET_POCHI_F_PANZA: sumar(p, NEED_CARINO, 4); sumar(p, NEED_DIVERSION, 4); impulsar(p, EMO_ALEGRIA, 0.4f, CAUSA_JUGAMOS); break;
    case PET_POCHI_F_MANIA: sumar(p, NEED_DIVERSION, 6); impulsar(p, EMO_ALEGRIA, 0.4f, CAUSA_JUGAMOS); break;
    case PET_POCHI_F_MOJADA: sumar(p, NEED_DIVERSION, 3); break;
    case PET_POCHI_F_ANIMAL: sumar(p, NEED_DIVERSION, 5); sumar(p, NEED_CURIOSIDAD, 4); impulsar(p, EMO_ALEGRIA, 0.3f, CAUSA_NOVEDAD); break;
    case 1: sumar(p, NEED_DIVERSION, 6); impulsar(p, EMO_ALEGRIA, 0.4f, CAUSA_JUGAMOS); break;
    case 2: sumar(p, NEED_DIVERSION, 5); sumar(p, NEED_CURIOSIDAD, 3); subir_vinculo(p, 0.4f); break;
    case 4: sumar(p, NEED_HIGIENE, -25); p->barro = 1; sumar(p, NEED_DIVERSION, 4); break; /* Pochi la salpicó de barro: hay que bañarla */
    default: sumar(p, NEED_DIVERSION, 10); impulsar(p, EMO_CARINO, 0.8f, CAUSA_CARICIAS); subir_vinculo(p, 1.5f); break;
    }
}
void pet_jump_bed(pet_t *p, uint32_t t)
{
    if (p->etapa == STAGE_HUEVO || p->dormida) return;
    p->ultima_interaccion = t;
    sumar(p, NEED_DIVERSION, 4);
    impulsar(p, EMO_ALEGRIA, 0.3f, CAUSA_JUGAMOS);
}
void pet_crystal_game(pet_t *p, int rondas, uint32_t t)
{
    if (p->etapa == STAGE_HUEVO) return;
    if (rondas < 0) rondas = 0;
    if (rondas > 3) rondas = 3;
    p->ultima_interaccion = t;
    sumar(p, NEED_DIVERSION, (float)(10 + 8 * rondas));
    impulsar(p, EMO_ALEGRIA, 0.3f + 0.2f * (float)rondas, CAUSA_JUGAMOS);
    if (rondas > p->cristales_rondas) p->cristales_rondas = (uint8_t)rondas;
    if (rondas >= 3) {
        bool nuevo = !pet_special_known(p, ESP_CRISTALES);
        descubrir_secreto(p, ESP_CRISTALES, t);
        if (nuevo) { impulsar(p, EMO_ALEGRIA, 1.0f, CAUSA_ESPECIAL); impulsar(p, EMO_ORGULLO, 0.9f, CAUSA_ESPECIAL); }
        else if (p->especial_dia[ESP_CRISTALES] != hoy_byte(p, t)) regalo_magico(p, t); /* el dragón, a veces, le da algo */
        p->especial_dia[ESP_CRISTALES] = hoy_byte(p, t);
    }
    vivir(p, VIV_JUGUETE);
}
/* (versión 23: la melodía secreta; ahora los cristales de la cueva solo cantan) */
int pet_crystal_note(pet_t *p, int k, uint32_t t)
{
    (void)t;
    if (p->escena != ESC_CUEVA || (unsigned)k > 5) return 0;
    if (p->melodia_pos < PET_MELODIA_LARGO && MELODIA[p->melodia_pos] == k) {
        if (++p->melodia_pos < PET_MELODIA_LARGO) return 1;
        p->melodia_pos = 0;
        return 1;
    }
    p->melodia_pos = MELODIA[0] == k ? 1 : 0;
    return 0;
}
/* de vez en cuando comenta una pista de un secreto que todavía no descubren (en una zona que ya conoce) */
static void paso_pistas(pet_t *p, uint32_t t, bool vivo)
{
    (void)vivo;
    if (p->etapa == STAGE_HUEVO || p->dormida || pet_mode(p) == MODO_INFANTE) return;
    if (!p->pista_prox) { p->pista_prox = t + 4u * 3600u; return; }
    if (t < p->pista_prox) return;
    p->pista_prox = t + 6u * 3600u + (uint32_t)(azar_animal(p, t, 70, 1) * 8.0f * 3600.0f);
    int e = -1, n = 0;
    for (int k = 0; k < ESP_COUNT; k++)
        if (!pet_special_known(p, (especial_t)k) && pet_scene_open(p, ESPECIAL_ZONA[k]) && azar_animal(p, t, 71 + k, 2) * (float)(++n) < 1.0f) e = k;
    if (e < 0) return;
    int cual;
    cual = (int)(azar_animal(p, t, 76 + e, 3) * 3.0f) % 3;
    if (p->pista_ultima == 1 + e * 8 + cual) cual = (cual + 1) % 3; /* nunca la misma dos veces seguidas */
    if (p->pistas_dadas[e] < 255) p->pistas_dadas[e]++;
    p->pista_ultima = (uint8_t)(1 + e * 8 + cual);
    push_event(p, EV_PISTA, e, cual, t);
}

/* --- pesca en el lago --- */
bool pet_can_fish(const pet_t *p, uint32_t t)
{
    (void)t;
    return p->escena == ESC_LAGO && !p->dormida && p->etapa != STAGE_HUEVO; /* (v38: con el lago helado, en un hoyo en el hielo) */
}

static void aplicar_pesca(pet_t *p, pesca_t c, uint32_t t, bool vivo);
static void fin_pesca(pet_t *p, uint32_t t, bool vivo)
{
    float r = rng01(p);
    if (t < p->pesca_hasta) r *= 0.6f; /* con un amuleto del pescador pican más */
    static const float HASTA[] = {0.34f, 0.36f, 0.44f, 0.51f, 0.59f, 0.65f, 0.70f, 0.75f, 0.78f, 0.80f};
    static const pesca_t QUE[] = {PESCA_PEZ, PESCA_PEZ_DORADO, PESCA_BOTA, PESCA_PIEDRA, PESCA_CONCHA, PESCA_ALGA, PESCA_CANGREJO,
                                  PESCA_NENUFAR, PESCA_BOTELLA, PESCA_LLAVE};
    if (pet_lake_frozen(p, t)) { /* (v38) en el hoyo del hielo: sin conchas, algas, cangrejos ni nenúfares */
        static const float HH[] = {0.36f, 0.385f, 0.47f, 0.56f, 0.60f, 0.63f};
        static const pesca_t QH[] = {PESCA_PEZ, PESCA_PEZ_DORADO, PESCA_BOTA, PESCA_PIEDRA, PESCA_BOTELLA, PESCA_LLAVE};
        pesca_t c = PESCA_NADA;
        for (unsigned k = 0; k < sizeof HH / sizeof HH[0]; k++) if (r < HH[k]) { c = QH[k]; break; }
        aplicar_pesca(p, c, t, vivo);
        return;
    }
    pesca_t c = PESCA_NADA;
    for (unsigned k = 0; k < sizeof HASTA / sizeof HASTA[0]; k++) if (r < HASTA[k]) { c = QUE[k]; break; }
    aplicar_pesca(p, c, t, vivo);
}

/* v89: la pesca con minijuego (cuando TÚ la mandas a pescar). Lo que pica no es siempre igual: depende de la estación y la hora. */
static pesca_t pesca_atrapada(pet_t *p, uint32_t t)
{
    const estacion_t est = pet_season(p, t);
    const int h = hora_local(p, t);
    const bool tarde = h >= 18 && h < 21, noche = h >= 21 || h < 6;
    if (pet_lake_frozen(p, t)) { /* (v38) en el hielo: pez, pez dorado (más raro), piedra, bota, botella, llave */
        const bool sin_hielo = !((p->coleccion[COL_CRISTALES] >> 3) & 1u); /* (fw 104) su trocito de hielo, si no lo tiene */
        struct { pesca_t c; float w; } H[] = {{PESCA_PEZ, 50}, {PESCA_PEZ_DORADO, tarde ? 4.0f : 1.2f}, {PESCA_BOTA, 9}, {PESCA_PIEDRA, 10}, {PESCA_BOTELLA, 4}, {PESCA_LLAVE, 3}, {PESCA_HIELO, sin_hielo && est == ESTACION_INVIERNO ? 7.0f : 0.0f}};
        float th = 0;
        for (unsigned k = 0; k < sizeof H / sizeof H[0]; k++) th += H[k].w;
        float rh = rng01(p) * th;
        for (unsigned k = 0; k < sizeof H / sizeof H[0]; k++) { if (rh < H[k].w) return H[k].c; rh -= H[k].w; }
        return PESCA_PEZ;
    }
    struct { pesca_t c; float w; } T[] = {
        {PESCA_PEZ, 40}, {PESCA_PEZ_DORADO, tarde ? 5.0f : 1.5f}, {PESCA_BOTA, 8}, {PESCA_PIEDRA, 7}, {PESCA_CONCHA, 9}, {PESCA_ALGA, 7},
        {PESCA_CANGREJO, noche ? 9.0f : 5.0f}, {PESCA_NENUFAR, est == ESTACION_VERANO ? 9.0f : (est == ESTACION_PRIMAVERA ? 4.0f : 0.0f)},
        {PESCA_BOTELLA, 3}, {PESCA_LLAVE, 2},
    };
    float tot = 0;
    for (unsigned k = 0; k < sizeof T / sizeof T[0]; k++) tot += T[k].w;
    float r = rng01(p) * tot;
    for (unsigned k = 0; k < sizeof T / sizeof T[0]; k++) { if (r < T[k].w) return T[k].c; r -= T[k].w; }
    return PESCA_PEZ;
}
void pet_fish_game(pet_t *p, int arg, uint32_t t)
{
    if (arg == 0) { /* empezó el minijuego: la pesca no se termina sola mientras juegan (a lo más 10 min) */
        if (p->actividad == ACTV_PESCAR) p->actividad_hasta = t + 600u;
        return;
    }
    if (p->actividad != ACTV_PESCAR) return;
    const int res = arg - 1; /* 0 no picó, 1 se escapó, 2 lo sacó */
    if (res == 2) aplicar_pesca(p, pesca_atrapada(p, t), t, true);
    else {
        if (res == 1) impulsar(p, EMO_CURIOSIDAD, 0.4f, CAUSA_PESCA); /* "¡se me escapó!" */
        aplicar_pesca(p, PESCA_NADA, t, true);
    }
    p->actividad = ACTV_PASEAR; /* se baja del bote y pasea por la orilla */
    p->actividad_hasta = t + 30u;
}

static void aplicar_pesca(pet_t *p, pesca_t c, uint32_t t, bool vivo)
{
    int comio = 0;
    switch (c) {
    case PESCA_PEZ:
        if (p->necesidad[NEED_SACIEDAD] < 65 && !p->modo_nino) { sumar(p, NEED_SACIEDAD, 18); comio = 1; }
        else sumar(p, NEED_DIVERSION, 6); /* lo devuelve al agua */
        impulsar(p, EMO_ORGULLO, 0.6f, CAUSA_PESCA);
        impulsar(p, EMO_ALEGRIA, 0.6f, CAUSA_PESCA);
        break;
    case PESCA_PEZ_DORADO: /* lo mira brillar y lo devuelve: un recuerdo para siempre */
        impulsar(p, EMO_ALEGRIA, 1.0f, CAUSA_PESCA);
        impulsar(p, EMO_ORGULLO, 0.8f, CAUSA_PESCA);
        sumar(p, NEED_CURIOSIDAD, 20);
        break;
    case PESCA_BOTA: case PESCA_ALGA: sumar(p, NEED_DIVERSION, 10); impulsar(p, EMO_ALEGRIA, 0.5f, CAUSA_PESCA); break;
    case PESCA_PIEDRA: guardar_item(p, ITEM_PIEDRA); impulsar(p, EMO_ORGULLO, 0.6f, CAUSA_PESCA); break;
    case PESCA_CONCHA: {
        int pieza = (int)(rng_next(p) % TAM_COLECCION[COL_CONCHAS]);
        if (guardar_pieza(p, COL_CONCHAS, pieza, t, vivo)) push_event(p, EV_TESORO, 1, COL_CONCHAS * 8 + pieza, t);
        impulsar(p, EMO_ORGULLO, 0.5f, CAUSA_PESCA);
        break;
    }
    case PESCA_CANGREJO: impulsar(p, EMO_MIEDO, 0.2f, CAUSA_PESCA); impulsar(p, EMO_ALEGRIA, 0.4f, CAUSA_PESCA); sumar(p, NEED_DIVERSION, 8); break;
    case PESCA_NENUFAR: p->hoja_hasta = t + 3u * 3600u; p->hoja_nenufar = 1; impulsar(p, EMO_ALEGRIA, 0.6f, CAUSA_PESCA); break;
    case PESCA_BOTELLA: {
        int f = (int)(rng_next(p) % (uint32_t)pet_phrase_count());
        if (f == p->ultima_frase) f = (f + 1) % pet_phrase_count();
        p->ultima_frase = (uint8_t)f;
        p->frase_respondida = 0;
        impulsar(p, EMO_CURIOSIDAD, 0.7f, CAUSA_PESCA);
        push_event(p, EV_BOTELLA, f, 0, t);
        break;
    }
    case PESCA_LLAVE: pet_object_gives(p, OBJ_LLAVE_OXIDADA, OFU_PESCA, t); impulsar(p, EMO_CURIOSIDAD, 0.6f, CAUSA_PESCA); break;
    case PESCA_HIELO: { /* (fw 104) ¡el trocito de hielo! Va a su colección de cristales (pieza 4); fuera del invierno se derrite */
        if (guardar_pieza(p, COL_CRISTALES, 3, t, vivo)) push_event(p, EV_TESORO, 1, COL_CRISTALES * 8 + 3, t);
        p->hielo_fuera = 0;
        impulsar(p, EMO_ALEGRIA, 0.9f, CAUSA_PESCA);
        impulsar(p, EMO_CURIOSIDAD, 0.7f, CAUSA_PESCA);
        sumar(p, NEED_CURIOSIDAD, 20);
        break;
    }
    default: if (!p->modo_nino) impulsar(p, EMO_ABURRIMIENTO, 0.2f, CAUSA_PESCA); break;
    }
    sumar(p, NEED_DIVERSION, 8);
    sumar(p, NEED_SEGURIDAD, 6); /* pescar la calma */
    push_event(p, EV_PESCA, c, comio, t);
    if (c == PESCA_NADA || c == PESCA_BOTA || c == PESCA_PIEDRA) quizas_objeto(p, OFU_PESCA, 0.6f, t); /* a veces el anzuelo saca algo mágico */
    vivir(p, VIV_PESCA);
    if (c != PESCA_NADA) {
        bool primera = true;
        for (int k = 0; k < p->diario_n; k++) if (pet_diary_get(p, k)->tipo == DIA_PESCA) { primera = false; break; }
        anotar(p, DIA_PESCA, c, 0, t);
        if (primera || c == PESCA_PEZ_DORADO) foto(p, DIA_PESCA, c, t);
    }
    if (vivo) sonar(p, c == PESCA_NADA ? SND_SUSPIRO : (c == PESCA_ALGA || c == PESCA_BOTA ? SND_RISA : SND_PESCO), t);
}

/* salir a pescar dura entre 40 y 90 segundos: remar, tirar la caña, esperar la boya y sacar algo */
static uint32_t duracion_pesca(pet_t *p) { return PESCA_DUR_MIN + rng_next(p) % (PESCA_DUR_MAX - PESCA_DUR_MIN + 1); }
static bool puede_pescar_sola(const pet_t *p, uint32_t t)
{
    return p->pesca_dia != (uint8_t)(1 + (dia_local(p, t) & 0xFF)) || p->pescas_solas < PESCA_SOLA_DIA;
}

static void marcar_senda(pet_t *p, int desde, int hasta);
static void empezar_pesca(pet_t *p, uint32_t t, uint32_t dur, bool vivo)
{
    if (p->lugar != LUGAR_ESTANQUE) marcar_senda(p, p->lugar, LUGAR_ESTANQUE);
    p->actividad = ACTV_PESCAR;
    p->lugar = LUGAR_ESTANQUE;
    p->actividad_hasta = t + dur;
    push_event(p, EV_ACTIVIDAD, ACTV_PESCAR, LUGAR_ESTANQUE, t);
    if (vivo) sonar(p, SND_PLOP, t);
}

/* --- nacer y clima --- */

static int32_t slot_frase(const pet_t *p, uint32_t t);
static float bienestar(const pet_t *p);

/* pasa a una etapa nueva: la forma adulta se decide según cómo la acompañaste y lo que la marcó */
static void evo_forma(const pet_t *p, uint32_t t, int *forma, int *mezcla)
{
    bool acompanada = t - p->ultima_interaccion < 2u * 86400u; /* radiante solo si la acompañaste mientras crecía */
    *forma = p->cuidado > 68.0f && acompanada ? FORMA_RADIANTE : FORMA_NORMAL; /* ya no hay forma gruñona */
    int lin = pet_lineage_lead(p); /* lo que la marcó al crecer: la forma adulta de otra especie */
    *mezcla = lin != (p->especie & 7) ? lin + 1 : p->mezcla;
}
static void crecer(pet_t *p, stage_t e, uint32_t t)
{
    p->etapa = (uint8_t)e;
    p->evo_pend = 0;
    if (e == STAGE_ADULTA) {
        int forma, mezcla;
        evo_forma(p, t, &forma, &mezcla);
        p->forma = (uint8_t)forma;
        p->mezcla = (uint8_t)mezcla;
    }
    impulsar(p, EMO_ORGULLO, 0.9f, CAUSA_CRECIO);
    push_event(p, EV_CRECIO, e, p->forma, t);
    anotar(p, DIA_CRECIO, e, p->forma, t);
    if (e == STAGE_ADULTA) foto(p, DIA_CRECIO, e, t);
    sonar(p, SND_ALEGRE, t);
}
int pet_evo_pending(const pet_t *p) { return p->evo_pregunta && p->etapa != STAGE_HUEVO && p->evo_pend > p->etapa && p->evo_pend <= STAGE_ADULTA ? p->evo_pend : 0; }
void pet_evo_preview(const pet_t *p, uint32_t ahora, int *etapa, int *forma, int *mezcla)
{
    const int sig = pet_evo_pending(p);
    *etapa = sig ? sig : p->etapa;
    *forma = p->forma;
    *mezcla = p->mezcla;
    if (sig == STAGE_ADULTA) evo_forma(p, ahora, forma, mezcla);
}
bool pet_evo_accept(pet_t *p, uint32_t ahora)
{
    const int sig = pet_evo_pending(p);
    if (!sig) return false;
    p->evo_decidido = 1;
    crecer(p, (stage_t)sig, ahora);
    return true;
}
void pet_evo_defer(pet_t *p)
{
    if (pet_evo_pending(p)) p->evo_decidido = 1;
}

static void nacer(pet_t *p, uint32_t t)
{
    p->etapa = STAGE_BEBE;
    p->evo_pregunta = 1; /* antes de cada crecimiento te pregunta */
    p->evo_pend = 0;
    p->evo_decidido = 0;
    p->nacimiento = t;
    p->ultima_interaccion = t;
    p->ultimo_regalo = dia_local(p, t);
    p->hora_actual = (int8_t)hora_local(p, t);
    p->ultima_frase_slot = slot_frase(p, t);
    p->actividad_hasta = t + 60;
    impulsar(p, EMO_CURIOSIDAD, 0.9f, CAUSA_NOVEDAD);
    crear_vecino(p, t);
    push_event(p, EV_NACIO, 0, 0, t);
    anotar(p, DIA_NACIO, 0, 0, t);
    foto(p, DIA_NACIO, 0, t);
    sonar(p, SND_ALEGRE, t);
}

void pet_egg_start(pet_t *p, uint32_t ahora)
{
    if (p->etapa != STAGE_HUEVO) return;
    p->eclosion = ahora + PET_HUEVO_SEGUNDOS;
    p->nacimiento = p->eclosion;
}

void pet_hatch(pet_t *p, uint32_t ahora)
{
    if (p->etapa != STAGE_HUEVO) return;
    if (p->ultima_actualizacion < ahora) p->ultima_actualizacion = ahora;
    nacer(p, ahora);
}

bool pet_is_egg(const pet_t *p) { return p->etapa == STAGE_HUEVO; }
uint32_t pet_hatch_remaining(const pet_t *p, uint32_t ahora)
{
    return (p->etapa == STAGE_HUEVO && p->eclosion > ahora) ? p->eclosion - ahora : 0;
}

/* fecha civil a partir de los días desde 1970 (algoritmo de H. Hinnant) */
void pet_local_date(const pet_t *p, uint32_t t, int *anio, int *mes, int *dia)
{
    int64_t z = (int64_t)dia_local(p, t) + 719468;
    int64_t era = (z >= 0 ? z : z - 146096) / 146097;
    int64_t doe = z - era * 146097;
    int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    int64_t mp = (5 * doy + 2) / 153;
    int m = (int)(mp < 10 ? mp + 3 : mp - 9); /* 1..12 */
    if (dia) *dia = (int)(doy - (153 * mp + 2) / 5 + 1);
    if (mes) *mes = m;
    if (anio) *anio = (int)(yoe + era * 400 + (m <= 2 ? 1 : 0));
}
static int mes_local(const pet_t *p, uint32_t t)
{
    int m;
    pet_local_date(p, t, NULL, &m, NULL);
    return m;
}

/* el mes de SU mundo: con el ritmo acelerado las estaciones pasan más rápido para ella (el calendario y el reloj de pantalla siguen siendo los tuyos) */
static int mes_mundo(const pet_t *p, uint32_t t)
{
    const float k = pet_rhythm_factor(p);
    if (k <= 1.0f || t <= p->nacimiento) return mes_local(p, t);
    const int64_t wt = (int64_t)p->nacimiento + (int64_t)((double)(t - p->nacimiento) * (double)k) + p->zona_horaria;
    const int64_t z = (wt >= 0 ? wt / 86400 : (wt - 86399) / 86400) + 719468;
    const int64_t era = (z >= 0 ? z : z - 146096) / 146097;
    const int64_t doe = z - era * 146097;
    const int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const int64_t mp = (5 * doy + 2) / 153;
    return (int)(mp < 10 ? mp + 3 : mp - 9);
}

/* --- tu cumpleaños --- */
static bool bisiesto(int a) { return (a % 4 == 0 && a % 100 != 0) || a % 400 == 0; }
static int dias_mes(int m, int a)
{
    static const uint8_t D[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    return m == 2 && bisiesto(a) ? 29 : D[(m - 1) % 12];
}

bool pet_set_owner_birthdate(pet_t *p, int dia, int mes, int anio, uint32_t ahora)
{
    int hoy_a;
    pet_local_date(p, ahora, &hoy_a, NULL, NULL);
    if (mes < 1 || mes > 12 || anio < 1900 || anio > hoy_a || dia < 1 || dia > dias_mes(mes, anio)) return false;
    p->dueno_dia = (uint8_t)dia;
    p->dueno_mes = (uint8_t)mes;
    p->dueno_anio = (uint16_t)anio;
    p->cumple_anio = 0; /* si hoy es tu cumpleaños, se celebra hoy mismo */
    int e = pet_owner_age(p, ahora);
    pet_set_owner_age(p, e < 1 ? 1 : e);
    return true;
}

int pet_owner_age(const pet_t *p, uint32_t ahora)
{
    if (!p->dueno_mes) return p->edad_dueno;
    int a, m, d;
    pet_local_date(p, ahora, &a, &m, &d);
    int e = a - p->dueno_anio;
    if (m < p->dueno_mes || (m == p->dueno_mes && d < p->dueno_dia)) e--;
    return e < 0 ? 0 : e;
}

bool pet_owner_birthday(const pet_t *p, uint32_t ahora)
{
    if (!p->dueno_mes) return false;
    int a, m, d;
    pet_local_date(p, ahora, &a, &m, &d);
    if (m != p->dueno_mes) return false;
    if (d == p->dueno_dia) return true;
    return p->dueno_mes == 2 && p->dueno_dia == 29 && !bisiesto(a) && d == 28; /* nacidos un 29 de febrero */
}

/* el día de tu cumpleaños, en cuanto ella está despierta, lo celebra (una vez por año) */
static void revisar_cumple(pet_t *p, uint32_t t)
{
    if (!p->dueno_mes || !pet_owner_birthday(p, t)) return;
    int a;
    pet_local_date(p, t, &a, NULL, NULL);
    if (p->cumple_anio == a) return;
    if (p->etapa == STAGE_HUEVO) return;
    if (p->dormida || hora_local(p, t) < 7) return;
    p->cumple_anio = (int16_t)a;
    int e = pet_owner_age(p, t);
    pet_set_owner_age(p, e < 1 ? 1 : e); /* al cumplir 13 pasa a modo adulto */
    sumar(p, NEED_DIVERSION, 15);
    sumar(p, NEED_CARINO, 10);
    impulsar(p, EMO_ALEGRIA, 0.95f, CAUSA_CUMPLE);
    subir_vinculo(p, 3.0f);
    if (p->anillos < 255) p->anillos++; /* el Árbol de los años: una flor nueva */
    push_event(p, EV_CUMPLE, e > 127 ? 127 : e, p->anillos, t);
    vivir(p, VIV_CUMPLE);
    anotar(p, DIA_CUMPLE_DUENO, 0, 0, t);
    foto(p, DIA_CUMPLE_DUENO, 0, t);
}

estacion_t pet_season(const pet_t *p, uint32_t t)
{
    if (p->estacion_fija < ESTACION_COUNT) return (estacion_t)p->estacion_fija;
    int m = mes_mundo(p, t);
    static const uint8_t NORTE[13] = {0, ESTACION_INVIERNO, ESTACION_INVIERNO, ESTACION_PRIMAVERA, ESTACION_PRIMAVERA,
                                      ESTACION_PRIMAVERA, ESTACION_VERANO, ESTACION_VERANO, ESTACION_VERANO,
                                      ESTACION_OTONO, ESTACION_OTONO, ESTACION_OTONO, ESTACION_INVIERNO};
    static const uint8_t SUR[13] = {0, ESTACION_VERANO, ESTACION_VERANO, ESTACION_OTONO, ESTACION_OTONO,
                                    ESTACION_OTONO, ESTACION_INVIERNO, ESTACION_INVIERNO, ESTACION_INVIERNO,
                                    ESTACION_PRIMAVERA, ESTACION_PRIMAVERA, ESTACION_PRIMAVERA, ESTACION_VERANO};
    return (estacion_t)(p->hemisferio_sur ? SUR[m] : NORTE[m]);
}

clima_t pet_weather(const pet_t *p) { return (clima_t)(p->clima < CLIMA_COUNT ? p->clima : 0); }

static void cambio_de_clima(pet_t *p, clima_t c, uint32_t t)
{
    if (c == p->clima) return;
    if ((p->clima == CLIMA_LLUVIA || p->clima == CLIMA_TORMENTA) && c != CLIMA_FRIO && c != CLIMA_NIEVE)
        p->charcos_hasta = t + 2u * 3600u; /* después de la lluvia quedan charcos */
    /* después de la lluvia, con sol, a veces sale un arcoíris */
    int h = hora_local(p, t);
    if ((p->clima == CLIMA_LLUVIA || p->clima == CLIMA_TORMENTA) && (c == CLIMA_DESPEJADO || c == CLIMA_NUBLADO) && h >= 8 && h < 19 &&
        p->raro == 0xFF && t - p->ultimo_raro > 3u * 3600u && chance(p, 0.5f)) {
        p->clima = (uint8_t)c;
        empezar_raro(p, RARO_ARCOIRIS, t, 1200, false);
    }
    p->clima = (uint8_t)c;
    if (c == CLIMA_LLUVIA || c == CLIMA_TORMENTA) vivir(p, VIV_LLUVIA);
    if (c == CLIMA_NIEVE) vivir(p, VIV_NIEVE);
    push_event(p, EV_CLIMA, c, 0, t);
    sumar(p, NEED_CURIOSIDAD, 8);
    int g = p->memoria.clima[c];
    if (g > 45) descubrir(p, GUSTO_CLIMA, c, t);
    else if (g < -45 && c != CLIMA_DESPEJADO) descubrir(p, GUSTO_CLIMA, c, t);
    p->actividad_hasta = t; /* reconsidera qué hacer */
}

void pet_force_weather(pet_t *p, clima_t c, uint32_t ahora, uint32_t segundos)
{
    p->clima_hasta = ahora + segundos;
    cambio_de_clima(p, c, ahora);
}

static void nuevo_clima(pet_t *p, uint32_t t)
{
    /* probabilidades (de 100) por estación: despejado, nublado, lluvia, viento, nieve, tormenta, frío */
    static const uint8_t PROB[ESTACION_COUNT][CLIMA_COUNT] = {
        {48, 20, 14, 10, 0, 5, 3},   /* primavera */
        {64, 10, 5, 12, 0, 9, 0},    /* verano    */
        {29, 22, 20, 15, 3, 5, 6},   /* otoño (con alguna nevada, sobre todo al final) */
        {22, 18, 15, 8, 17, 3, 17},  /* invierno  */
    };
    const uint8_t *pr = PROB[pet_season(p, t)];
    clima_t c = CLIMA_DESPEJADO;
    for (int intento = 0; intento < 2; intento++) { /* si sale el mismo, se tira otra vez: más variado */
        uint32_t r = rng_next(p) % 100, acum = 0;
        c = CLIMA_DESPEJADO;
        for (int i = 0; i < CLIMA_COUNT; i++) {
            acum += pr[i];
            if (r < acum) { c = (clima_t)i; break; }
        }
        if (c != (clima_t)p->clima) break;
    }
    /* cambia seguido para que el día se sienta vivo: de 30 a 90 minutos (la tormenta, 15 a 40; el frío, 1 a 2 horas) */
    uint32_t dur = c == CLIMA_TORMENTA ? 900u + rng_next(p) % 1500u
                 : (c == CLIMA_FRIO ? 3600u + rng_next(p) % 3600u : 1800u + rng_next(p) % 3600u);
    p->clima_hasta = t + dur;
    cambio_de_clima(p, c, t);
}

/* ======================================================================
 * 4b. El mundo: zonas, fauna, jardín, charcos, hojas y caminos
 * ==================================================================== */

static bool tiene_refugio(const pet_t *p)
{
    return (((p->decoraciones >> DECOR_CASITA) & 1u) && p->escena == ESC_PRADO) || p->escena == ESC_CUEVA;
}

bool pet_scene_open(const pet_t *p, escena_t e)
{
    if ((unsigned)e >= ESC_COUNT) return false;
    if (e == ESC_CLARO) return (p->lugares >> PET_ZONA_CLARO) & 1u; /* (v41) los lugares secretos se abren con la historia */
    if (e == ESC_MIRADOR) return (p->lugares >> 1) & 1u;
    return (p->escenas_abiertas >> e) & 1u;
}
int pet_scene_liking(const pet_t *p, escena_t e)
{
    if ((unsigned)e < ESC_GUSTOS) return p->gusto_escena[e];
    if (e == ESC_ISLA) { int g = (p->gusto_escena[ESC_LAGO] + p->gusto_escena[ESC_COLINA]) / 2 + 20; return g > 100 ? 100 : (g < -100 ? -100 : g); }
    if (e == ESC_CLARO) { int g = p->gusto_escena[ESC_BOSQUE] + 25; return g > 100 ? 100 : (g < -100 ? -100 : g); } /* (v41) el claro de Tilo: como el bosque, pero más querido */
    if (e == ESC_MIRADOR) { int g = p->gusto_escena[ESC_COLINA] + 20; return g > 100 ? 100 : (g < -100 ? -100 : g); }
    return 0;
}

bool pet_fauna_here(const pet_t *p, fauna_t f, uint32_t t) { return (unsigned)f < FAUNA_COUNT && t < p->fauna_hasta[f]; }

bool pet_puddles(const pet_t *p, uint32_t t) { return t < p->charcos_hasta && p->escena != ESC_CUEVA; }

int pet_trail_pair(int a, int b)
{
    if (a == b || (unsigned)a >= LUGAR_COUNT || (unsigned)b >= LUGAR_COUNT) return -1;
    if (a > b) { int x = a; a = b; b = x; }
    return a * LUGAR_COUNT - a * (a + 1) / 2 + (b - a - 1);
}

uint32_t pet_trail_mask(const pet_t *p)
{
    uint32_t m = 0;
    for (int i = 0; i < LUGAR_COUNT * (LUGAR_COUNT - 1) / 2; i++)
        if (p->senda[i] >= 6.0f) m |= 1u << i;
    return m;
}

static void marcar_senda(pet_t *p, int desde, int hasta)
{
    if (p->escena != ESC_PRADO) return;
    int k = pet_trail_pair(desde, hasta);
    if (k >= 0 && p->senda[k] < 30.0f) p->senda[k] += 1.0f;
}

static void cambiar_escena(pet_t *p, escena_t e, uint32_t t, bool sola)
{
    if (e == p->escena || !pet_scene_open(p, e)) return;
    if (sola && p->en_zona) return; /* (v39) está en el claro, la casita o el mirador contigo: no se va sola */
    p->escena = (uint8_t)e;
    p->escena_desde = t;
    for (int f = 0; f < FAUNA_COUNT; f++) p->fauna_hasta[f] = 0; /* los animales se quedan donde estaban */
    p->zorro_roba = 0;
    p->lugar = LUGAR_CLARO;
    p->lugar_antes = LUGAR_CLARO;
    p->actividad = ACTV_EXPLORAR;
    p->actividad_hasta = t + 240;
    sumar(p, NEED_CURIOSIDAD, 25);
    if (!((p->escenas_vistas >> e) & 1u)) { /* primera vez: ¡todo es nuevo! */
        p->escenas_vistas |= (uint8_t)(1u << e);
        anotar(p, DIA_ZONA, e, 0, t);
        foto(p, DIA_ZONA, e, t);
        impulsar(p, EMO_CURIOSIDAD, 0.9f, CAUSA_NOVEDAD);
        impulsar(p, EMO_ALEGRIA, 0.6f, CAUSA_NOVEDAD);
    } else if (pet_scene_liking(p, e) > 40) {
        impulsar(p, EMO_ALEGRIA, 0.5f, CAUSA_LUGAR);
    }
    push_event(p, EV_ESCENA, e, sola ? 1 : 0, t);
    static const int8_t VIV_ZONA[ESC_COUNT] = {-1, VIV_BOSQUE, VIV_LAGO, VIV_COLINA, VIV_CUEVA, VIV_LAGO, VIV_BOSQUE, VIV_COLINA};
    if (VIV_ZONA[e] >= 0) vivir(p, (vivido_t)VIV_ZONA[e]);
}

/* zonas que se abren con los días y la confianza (más fácil en modo niño) */
static const struct { escena_t e; float dias, vinculo; } REQ_ZONA[] = {{ESC_BOSQUE, 0.4f, 15.0f}, {ESC_LAGO, 0.8f, 20.0f}, {ESC_COLINA, 1.2f, 28.0f}}; /* zonas más rápido (antes 1/2/3 días y 25/30/40 de confianza) */
#define CUEVA_VINCULO 38.0f
#define ISLA_VINCULO 46.0f

/* Saca a la mascota de la zona e (la lleva al prado) y borra lo que quedó escrito de su primera visita (la marca de «ya visitada», la nota del diario y la foto del álbum),
 * sin tocar nada más de la partida: para cuando entró a una zona sin haberla descubierto como debía. Devuelve cuántas cosas borró. */
int pet_scene_forget(pet_t *p, escena_t e, uint32_t ahora)
{
    if (!p || e <= ESC_PRADO || (unsigned)e >= ESC_COUNT) return -1;
    int n = 0;
    if (p->escena == (uint8_t)e) { p->escena = ESC_PRADO; p->escena_desde = ahora; p->lugar = LUGAR_CLARO; p->lugar_antes = LUGAR_CLARO; p->actividad = ACTV_EXPLORAR; p->actividad_hasta = ahora + 240; n++; }
    if ((p->escenas_vistas >> e) & 1u) { p->escenas_vistas &= (uint8_t)~(1u << e); n++; }
    { /* diario: se quitan las notas de «descubrió esta zona» y el resto se queda en su orden */
        entrada_diario_t tmp[PET_DIARIO];
        int m = 0;
        for (int k = 0; k < p->diario_n; k++) {
            const entrada_diario_t *d = &p->diario[(p->diario_ini + k) % PET_DIARIO];
            if (d->tipo == DIA_ZONA && d->a == (int8_t)e) { n++; continue; }
            tmp[m++] = *d;
        }
        memset(p->diario, 0, sizeof p->diario);
        for (int k = 0; k < m; k++) p->diario[k] = tmp[k];
        p->diario_n = (uint8_t)m; p->diario_ini = 0;
    }
    { /* álbum: igual con las fotos de esa primera visita */
        foto_t tmp[PET_ALBUM];
        int m = 0;
        for (int k = 0; k < p->album_n; k++) {
            const foto_t *f = &p->album[(p->album_ini + k) % PET_ALBUM];
            if (f->motivo == DIA_ZONA && f->a == (int8_t)e) { n++; continue; }
            tmp[m++] = *f;
        }
        memset(p->album, 0, sizeof p->album);
        for (int k = 0; k < m; k++) p->album[k] = tmp[k];
        p->album_n = (uint8_t)m; p->album_ini = 0;
    }
    return n;
}
bool pet_scene_seen(const pet_t *p, escena_t e) { return (unsigned)e < ESC_COUNT && ((p->escenas_vistas >> e) & 1u); }

int pet_scene_requirement(const pet_t *p, escena_t e, uint32_t t, float *dias)
{
    if (dias) *dias = 0;
    if (pet_scene_open(p, e)) return 0;
    if (e == ESC_CUEVA || e == ESC_ISLA) return 3;
    float d = pet_age_days(p, t), v = p->vinculo + (p->modo_nino ? 10.0f : 0.0f);
    for (unsigned i = 0; i < sizeof REQ_ZONA / sizeof REQ_ZONA[0]; i++) {
        if (REQ_ZONA[i].e != e) continue;
        if (d < REQ_ZONA[i].dias) { if (dias) *dias = REQ_ZONA[i].dias - d; return 1; }
        if (v < REQ_ZONA[i].vinculo) return 2;
        return 1; /* se abre en el próximo paso */
    }
    return 3;
}

static void revisar_escenas(pet_t *p, uint32_t t, bool vivo)
{
    float dias = pet_age_days(p, t), v = p->vinculo + (p->modo_nino ? 10.0f : 0.0f);
    for (unsigned i = 0; i < sizeof REQ_ZONA / sizeof REQ_ZONA[0]; i++) {
        if (pet_scene_open(p, REQ_ZONA[i].e) || dias < REQ_ZONA[i].dias || v < REQ_ZONA[i].vinculo) continue;
        p->escenas_abiertas |= (uint8_t)(1u << REQ_ZONA[i].e);
        push_event(p, EV_ESCENA_NUEVA, REQ_ZONA[i].e, 0, t);
        if (vivo) sonar(p, SND_SORPRESA, t);
    }
}

/* flores abiertas en la zona donde está (las del prado cuentan también la decoración) */
static int flores_en_flor(const pet_t *p)
{
    int n = p->escena == ESC_PRADO && ((p->decoraciones >> DECOR_FLORES) & 1u) && pet_decor_visible(p, DECOR_FLORES) ? 2 : 0;
    int cuantas, ini = pet_plant_first((escena_t)p->escena, &cuantas);
    for (int i = ini; ini >= 0 && i < ini + cuantas; i++) {
        const planta_t *pl = pet_plant(p, i);
        if ((pl->tipo == PLANTA_FLOR || pl->tipo == PLANTA_GIRASOL) && pl->etapa >= ETAPA_GRANDE && !pl->mustia) n++;
    }
    return n;
}

static void aparece(pet_t *p, fauna_t f, uint32_t t, uint32_t dur, bool vivo)
{
    p->fauna_hasta[f] = t + dur;
    p->fauna_perseguida &= (uint8_t)~(1u << f);
    if (f == FAUNA_ZORRO) p->zorro_roba = t + 180u;
    if (vivo) push_event(p, EV_FAUNA, f, FSUC_LLEGA, t);
    if (f == FAUNA_MARIPOSA) vivir(p, VIV_MARIPOSA);
    if (f == FAUNA_ZORRO) vivir(p, VIV_ZORRO);
    if (f == FAUNA_PAJARO) vivir(p, VIV_PAJARO);
    if (f == FAUNA_PAJARO && p->pajaro_amistad > 0.3f) impulsar(p, EMO_CARINO, 0.3f, CAUSA_PAJARO);
    else if (f != FAUNA_ZORRO) impulsar(p, EMO_CURIOSIDAD, 0.45f, f == FAUNA_MARIPOSA ? CAUSA_MARIPOSA : (f == FAUNA_PAJARO ? CAUSA_PAJARO : CAUSA_NOVEDAD));
}

static void se_va(pet_t *p, fauna_t f, uint32_t t, bool vivo)
{
    p->fauna_hasta[f] = 0;
    if (vivo) push_event(p, EV_FAUNA, f, FSUC_SE_VA, t);
}

static void paso_mundo(pet_t *p, uint32_t t, float dt, bool vivo)
{
    const float h = dt / HORA;
    const clima_t c = pet_weather(p);
    const estacion_t est = pet_season(p, t);
    const int hl = hora_local(p, t);
    const bool dia = hl >= 8 && hl < 19;
    const escena_t e = (escena_t)p->escena;
    const bool buen_tiempo = c == CLIMA_DESPEJADO || c == CLIMA_NUBLADO;

    revisar_escenas(p, t, vivo);

    /* --- fauna --- */
    if (!p->dormida && e != ESC_CUEVA) {
        int flores = flores_en_flor(p) + (e == ESC_COLINA ? 2 : 0); /* la colina tiene flores silvestres */
        bool calido = est == ESTACION_PRIMAVERA || est == ESTACION_VERANO;
        const bool dia_largo = hl >= 7 && hl < 20;
        const float casa = e == ESC_PRADO ? 1.3f : 1.0f; /* en su prado se ven más seguido */
        if (!pet_fauna_here(p, FAUNA_MARIPOSA, t) && dia && buen_tiempo && est != ESTACION_INVIERNO &&
            chance(p, prob_paso(((calido ? 0.75f : 0.3f) + 0.12f * (float)flores) * casa, dt)))
            aparece(p, FAUNA_MARIPOSA, t, 900u + rng_next(p) % 1500u, vivo);
        if (!pet_fauna_here(p, FAUNA_ABEJA, t) && dia && c == CLIMA_DESPEJADO && calido && flores >= 1 &&
            chance(p, prob_paso(0.3f + 0.25f * (float)flores, dt)))
            aparece(p, FAUNA_ABEJA, t, 600u + rng_next(p) % 900u, vivo);
        if (!pet_fauna_here(p, FAUNA_PAJARO, t) && dia_largo && c != CLIMA_TORMENTA && e != ESC_COLINA &&
            chance(p, prob_paso(0.6f * casa, dt)))
            aparece(p, FAUNA_PAJARO, t, 900u + rng_next(p) % 1500u, vivo);
        if (!pet_fauna_here(p, FAUNA_ZORRO, t) && (e == ESC_PRADO || e == ESC_BOSQUE) && hl >= 9 && hl < 21 && t >= p->protege_hasta &&
            (p->inventario[ITEM_FRUTA] || p->inventario[ITEM_MIEL]) && chance(p, prob_paso(0.07f, dt)))
            aparece(p, FAUNA_ZORRO, t, 300u, vivo);
        if (!pet_fauna_here(p, FAUNA_RANA, t) && e == ESC_LAGO && est != ESTACION_INVIERNO && chance(p, prob_paso(1.2f, dt)))
            aparece(p, FAUNA_RANA, t, 900u + rng_next(p) % 900u, vivo);
    }
    for (int f = 0; f < FAUNA_COUNT; f++) {
        if (!p->fauna_hasta[f] || t < p->fauna_hasta[f]) continue;
        /* el pájaro, si se hicieron amigos, a veces deja una semilla al irse */
        if (f == FAUNA_PAJARO) {
            if (p->actividad != ACTV_PERSEGUIR) p->pajaro_amistad = clampf(p->pajaro_amistad + 0.05f, 0, 1);
            if (p->pajaro_amistad > 0.35f && chance(p, 0.35f)) {
                int pluma = (int)(rng_next(p) % 4);
                if (chance(p, 0.3f) && guardar_pieza(p, COL_PLUMAS, pluma, t, vivo)) push_event(p, EV_TESORO, 1, COL_PLUMAS * 8 + pluma, t); /* ¡una pluma! */
                else guardar_item(p, ITEM_SEMILLA);
                push_event(p, EV_FAUNA, FAUNA_PAJARO, FSUC_REGALO, t);
                impulsar(p, EMO_ALEGRIA, 0.5f, CAUSA_PAJARO);
            }
        }
        se_va(p, (fauna_t)f, t, vivo);
    }
    /* el zorro: si nadie lo espanta, se lleva una fruta (o miel); una valiente lo echa sola */
    if (pet_fauna_here(p, FAUNA_ZORRO, t) && p->zorro_roba && !p->dormida) {
        if (p->rasgo[TRAIT_TIMIDA] < 0.4f && chance(p, prob_paso(1.5f * (0.5f - p->rasgo[TRAIT_TIMIDA]), dt))) {
            p->zorro_roba = 0;
            impulsar(p, EMO_ORGULLO, 0.8f, CAUSA_ZORRO);
            push_event(p, EV_FAUNA, FAUNA_ZORRO, FSUC_LO_ECHO, t);
            se_va(p, FAUNA_ZORRO, t, false);
        } else if (t >= p->zorro_roba) {
            p->zorro_roba = 0;
            if (!p->modo_nino) { /* con niños el zorro solo se asoma */
                item_t it = p->inventario[ITEM_FRUTA] ? ITEM_FRUTA : ITEM_MIEL;
                if (p->inventario[it]) p->inventario[it]--;
                impulsar(p, p->rasgo[TRAIT_TIMIDA] > 0.6f ? EMO_MIEDO : EMO_ENOJO, 0.6f, CAUSA_ZORRO);
                push_event(p, EV_FAUNA, FAUNA_ZORRO, FSUC_ROBO, t);
            }
            se_va(p, FAUNA_ZORRO, t, false);
        }
    }
    p->miedo_abejas = decae(p->miedo_abejas, h, 240.0f); /* se olvida en semanas */

    /* --- jardines: crecen con agua. Sin agua se ponen mustios, después se secan y (con adultos) se pierden.
     * Un día sin regar ya les da sed: de vez en cuando hay que ir a regarlos. --- */
    int32_t hoy = dia_local(p, t);
    bool llueve = c == CLIMA_LLUVIA || c == CLIMA_TORMENTA;
    const float sed = est == ESTACION_VERANO ? 5.5f : (est == ESTACION_INVIERNO ? 1.6f : 4.0f); /* agua por hora */
    for (int i = 0; i < PET_PARCELAS_TOTAL; i++) {
        planta_t *pl = parcela(p, i);
        if (pl->tipo == PLANTA_NADA) continue;
        if (pl->mustia == 2) continue; /* se marchitó de vieja: espera que la saques */
        if (llueve) {
            if (pl->agua < PET_AGUA_SECA) pl->agua = 40.0f; /* la lluvia también las revive */
            else pl->agua = 100.0f;
            if (pl->mustia) { pl->mustia = 0; push_event(p, EV_JARDIN, i, JAR_REVIVIO, t); }
        }
        float antes = pl->agua;
        if (!llueve) pl->agua = clampf(pl->agua - sed * (pl->tipo == PLANTA_ARBOL ? 0.5f : 1.0f) * h, PET_AGUA_PERDIDA, 100);
        if (pl->agua <= 0.0f && !pl->mustia) { pl->mustia = 1; push_event(p, EV_JARDIN, i, JAR_MUSTIA, t); }
        if (pl->agua <= PET_AGUA_SECA && antes > PET_AGUA_SECA) push_event(p, EV_JARDIN, i, JAR_SECA, t);
        if (pl->agua <= PET_AGUA_PERDIDA) {
            if (p->modo_nino) pl->agua = PET_AGUA_PERDIDA + 1.0f; /* con niños se queda seca, esperando agua */
            else {
                memset(pl, 0, sizeof *pl);
                push_event(p, EV_JARDIN, i, JAR_PERDIDA, t);
                if (!p->dormida) impulsar(p, EMO_TRISTEZA, 0.35f, CAUSA_JARDIN);
            }
            continue;
        }
        if (pl->mustia || est == ESTACION_INVIERNO) continue; /* en invierno descansa */
        pl->crece += h;
        static const float FLOR[] = {12.0f, 36.0f, 1e9f}, ARBOL[] = {24.0f, 72.0f, 120.0f}, GIRASOL[] = {16.0f, 44.0f, 1e9f},
                           FRUTILLA[] = {12.0f, 30.0f, 60.0f}, HIERBA[] = {10.0f, 30.0f, 1e9f};
        const float *tabla = pl->tipo == PLANTA_FLOR ? FLOR : pl->tipo == PLANTA_GIRASOL ? GIRASOL : pl->tipo == PLANTA_FRUTILLA ? FRUTILLA
                             : pl->tipo == PLANTA_HIERBA ? HIERBA : ARBOL;
        /* (versión 25) cada planta vive su tiempo: ya grande, las flores, girasoles, frutillas y hierbas duran unas dos
         * semanas; los árboles, una estación. Después se marchitan solas (no por falta de agua) y ya se pueden sacar.
         * A veces dejan una semilla para volver a plantar */
        {
            const bool final_ = (pl->tipo == PLANTA_ARBOL || pl->tipo == PLANTA_FRUTILLA) ? pl->etapa == ETAPA_FRUTOS : pl->etapa >= ETAPA_GRANDE;
            const float hecha = pl->tipo == PLANTA_ARBOL || pl->tipo == PLANTA_FRUTILLA ? tabla[2] : tabla[1];
            const float vida = pl->tipo == PLANTA_ARBOL ? PET_VIDA_ARBOL_H : PET_VIDA_PLANTA_H;
            if (final_ && pl->crece >= hecha + vida) {
                pl->mustia = 2; /* de vieja: no revive */
                pl->agua = PET_AGUA_SECA - 1.0f;
                push_event(p, EV_JARDIN, i, JAR_VIEJA, t);
                static const int8_t CULT[] = {-1, CULTIVO_FLOR, -1, CULTIVO_GIRASOL, CULTIVO_FRUTILLA, CULTIVO_HIERBA};
                if (pl->tipo <= PLANTA_HIERBA && CULT[pl->tipo] >= 0 && ((uint32_t)hoy * 2246822519u ^ (uint32_t)i * 374761393u ^ p->id) % 100u < 50u)
                    ganar_semilla(p, CULT[pl->tipo], t);
                continue;
            }
        }
        if (pl->etapa < ETAPA_FRUTOS && pl->crece >= tabla[pl->etapa]) {
            pl->etapa++;
            bool flor = (pl->tipo == PLANTA_FLOR || pl->tipo == PLANTA_GIRASOL) && pl->etapa == ETAPA_GRANDE;
            if (flor && p->flores_hechas < 255) p->flores_hechas++;
            push_event(p, EV_JARDIN, i, flor ? JAR_FLORECIO : JAR_CRECIO, t);
            if (flor) vivir(p, VIV_FLORES);
            if (flor || pl->etapa == ETAPA_FRUTOS) impulsar(p, EMO_ALEGRIA, 0.6f, CAUSA_JARDIN);
        }
    }
    /* el árbol que creció donde enterró algo da una fruta al día */
    if (hoy != p->dia_jardin && p->etapa != STAGE_HUEVO) {
        p->dia_jardin = hoy;
        for (int i = 0; i < PET_PARCELAS_TOTAL; i++) {
            const planta_t *pl = parcela(p, i);
            if (pl->tipo == PLANTA_ARBOL && pl->etapa == ETAPA_FRUTOS && !pl->mustia) {
                guardar_item(p, ITEM_FRUTA);
                push_event(p, EV_JARDIN, i, JAR_FRUTO, t);
            } else if (pl->tipo == PLANTA_FRUTILLA && pl->etapa == ETAPA_FRUTOS && !pl->mustia && chance(p, 0.6f)) {
                guardar_item(p, ITEM_FRUTA); /* frutillas */
                push_event(p, EV_JARDIN, i, JAR_FRUTO, t);
            } else if (pl->tipo == PLANTA_HIERBA && pl->etapa >= ETAPA_GRANDE && !pl->mustia &&
                       ((uint32_t)hoy * 2654435761u ^ (uint32_t)i * 40503u ^ p->id) % 100u < 35u) { /* un día de cada tres, más o menos */
                guardar_item(p, ITEM_HIERBA);
                push_event(p, EV_JARDIN, i, JAR_FRUTO, t);
            } else if ((pl->tipo == PLANTA_FLOR || pl->tipo == PLANTA_GIRASOL) && pl->etapa >= ETAPA_GRANDE && !pl->mustia && chance(p, 0.12f)) {
                ganar_semilla(p, pl->tipo == PLANTA_GIRASOL ? CULTIVO_GIRASOL : CULTIVO_FLOR, t); /* una flor abierta a veces deja semillas */
            }
        }
    }

    /* --- charcos y hojas --- */
    if (!p->dormida && pet_puddles(p, t) && e == ESC_PRADO &&
        (p->actividad == ACTV_PASEAR || p->actividad == ACTV_JUGAR_SOLA || p->actividad == ACTV_EXPLORAR) &&
        chance(p, prob_paso(0.6f, dt)))
        salpicar(p, (int)(rng_next(p) % 2u), t, vivo, false);
    if (est == ESTACION_OTONO && p->hojas < 3 && t - p->ultimas_hojas > 4u * 3600u && c != CLIMA_LLUVIA) {
        p->hojas++;
        p->ultimas_hojas = t;
    }
    if (est != ESTACION_OTONO) p->hojas = 0;
    if (!p->dormida && p->hojas >= 2 && e == ESC_PRADO && (p->actividad == ACTV_JUGAR_SOLA || p->lugar == LUGAR_ARBOL) &&
        p->rasgo[TRAIT_ENERGICA] + p->rasgo[TRAIT_TRAVIESA] > 0.6f && chance(p, prob_paso(1.0f, dt))) {
        p->hojas = 0;
        p->ultimas_hojas = t;
        sumar(p, NEED_DIVERSION, 15);
        impulsar(p, EMO_ALEGRIA, 0.7f, CAUSA_HOJAS);
        if (vivo) { push_event(p, EV_HOJAS, 0, 0, t); sonar(p, SND_RISA, t); }
    }
}

/* salta en un charco (sola o porque la invitaste tocándolo) */
static int salpicar(pet_t *p, int charco, uint32_t t, bool vivo, bool invitada)
{
    bool le_gusta = p->memoria.clima[CLIMA_LLUVIA] > 0 || p->rasgo[TRAIT_ENERGICA] > 0.6f || (invitada && p->vinculo >= 50.0f);
    if (invitada && p->modo_nino) le_gusta = true; /* con niños siempre es divertido */
    sumar(p, NEED_HIGIENE, -12);
    if (le_gusta) { sumar(p, NEED_DIVERSION, invitada ? 14 : 10); impulsar(p, EMO_ALEGRIA, 0.6f, CAUSA_CHARCO); }
    else impulsar(p, EMO_ENOJO, 0.25f, CAUSA_CHARCO);
    p->actividad = ACTV_JUGAR_SOLA;
    p->actividad_hasta = t + 60;
    if (vivo) push_event(p, EV_SALPICA, (charco & 1) + (invitada ? 2 : 0), le_gusta ? 1 : -1, t);
    return le_gusta ? 1 : -1;
}

/* al terminar una persecución: mariposas divierten; abejas a veces pican (y se aprende) */
static void fin_persecucion(pet_t *p, uint32_t t, bool vivo)
{
    fauna_t f = (fauna_t)p->persigue;
    if (f == FAUNA_ABEJA) {
        if (!p->modo_nino && t >= p->protege_hasta && chance(p, 0.45f)) {
            sumar(p, NEED_SALUD, -3);
            sumar(p, NEED_SEGURIDAD, -15);
            impulsar(p, EMO_MIEDO, 0.8f, CAUSA_ABEJA);
            p->miedo_abejas = clampf(p->miedo_abejas + 0.55f, 0, 1);
            susto_aqui(p, 0.3f);
            push_event(p, EV_FAUNA, f, FSUC_PICO, t);
            if (vivo) sonar(p, SND_LLANTO, t);
            se_va(p, f, t, false);
            return;
        }
        sumar(p, NEED_DIVERSION, 8);
        impulsar(p, EMO_ALEGRIA, 0.4f, CAUSA_ABEJA);
    } else if (f == FAUNA_PAJARO) {
        p->pajaro_amistad = clampf(p->pajaro_amistad - 0.1f, 0, 1); /* lo asustó */
        sumar(p, NEED_DIVERSION, 8);
        se_va(p, f, t, vivo);
        return;
    } else {
        sumar(p, NEED_DIVERSION, 15);
        sumar(p, NEED_CURIOSIDAD, 10);
        impulsar(p, EMO_ALEGRIA, 0.7f, f == FAUNA_MARIPOSA ? CAUSA_MARIPOSA : CAUSA_NOVEDAD);
    }
}

/* entierra una semilla mientras cava: ahí crecerá un arbolito */
static void enterrar(pet_t *p, uint32_t t)
{
    if (p->escena != ESC_PRADO || p->inventario[ITEM_SEMILLA] < 3 || !chance(p, 0.3f)) return;
    for (int i = 0; i < PET_PARCELAS; i++) {
        planta_t *pl = &p->jardin[i];
        if (pl->tipo != PLANTA_NADA) continue;
        memset(pl, 0, sizeof *pl);
        pl->tipo = PLANTA_ARBOL;
        pl->agua = 60.0f;
        gastar_semilla(p, -1);
        impulsar(p, EMO_ORGULLO, 0.5f, CAUSA_JARDIN);
        push_event(p, EV_JARDIN, i, JAR_ENTERRO, t);
        return;
    }
}

/* --- sensación térmica --- */
static bool bebida_activa(const pet_t *p, food_t tipo, uint32_t t) { return p->bebida_tipo == tipo && t < p->bebida_hasta; }

/* cuánto le molesta el calor (0..0.55): depende de cuánto le gusta el sol */
static float molestia_calor(const pet_t *p) { return clampf((float)(60 - p->memoria.clima[CLIMA_DESPEJADO]) / 100.0f, 0.0f, 0.55f); }

static bool hace_calor(const pet_t *p, uint32_t t)
{
    int h = hora_local(p, t);
    return pet_season(p, t) == ESTACION_VERANO && pet_weather(p) == CLIMA_DESPEJADO && h >= 12 && h < 18 &&
           p->escena != ESC_BOSQUE && p->escena != ESC_CUEVA; /* el bosque da sombra; la cueva es fresca */
}

static bool hace_frio(const pet_t *p)
{
    clima_t c = pet_weather(p);
    return (c == CLIMA_FRIO && p->memoria.clima[CLIMA_FRIO] < 20) || (c == CLIMA_NIEVE && p->memoria.clima[CLIMA_NIEVE] < 0);
}

int pet_temperature_feel(const pet_t *p, uint32_t t)
{
    if (p->etapa == STAGE_HUEVO) return 0;
    if (hace_frio(p) && !bebida_activa(p, FOOD_BEBIDA_CALIENTE, t) && !ropa_abriga(p)) return -1;
    if (hace_calor(p, t) && molestia_calor(p) > 0.05f && !bebida_activa(p, FOOD_BEBIDA_FRIA, t) && p->actividad != ACTV_SOMBRA &&
        !lleva(p, ROPA_SOMBRERO)) return 1;
    return 0;
}

/* --- un paso de simulación de dt segundos que termina en t --- */
static void paso(pet_t *p, uint32_t t, float dt, bool vivo)
{
    /* su tiempo corre a su ritmo: hambre, sueño y ganas de jugar llegan más seguido (la mitad de lo que corre su reloj,
     * para que cuidarla no se vuelva agobiante) */
    const float h = dt / HORA * (1.0f + (pet_rhythm_factor(p) - 1.0f) * 0.5f);
    /* el cariño corre más suave que lo demás con el ritmo rápido: a ×3 pide cada 3 o 4 horas, no cada hora */
    const float hc = h * (1.0f + (pet_rhythm_factor(p) - 1.0f) * 0.1f) / (1.0f + (pet_rhythm_factor(p) - 1.0f) * 0.5f);
    const float *R = p->rasgo;
    float *N = p->necesidad;
    const bool dormida = p->dormida != 0;
    const bool sola = t - p->ultima_interaccion > AUSENCIA_S;
    /* si la dejaste sola, todo baja más lento: se pone triste, pero no sufre en serio */
    const float ritmo = (sola ? 0.35f : 1.0f) * (p->modo_nino ? 0.75f : 1.0f);
    const bool refugio = tiene_refugio(p);
    const clima_t clima = p->escena == ESC_CUEVA && pet_weather(p) != CLIMA_FRIO ? CLIMA_NUBLADO : pet_weather(p); /* en la cueva no llueve */

    /* hambre */
    float hambre = (dormida ? 1.5f : 4.0f) * (0.7f + 0.6f * R[TRAIT_GLOTONA]);
    sumar(p, NEED_SACIEDAD, -hambre * h * ritmo);

    /* energía: la siesta y la sombra en verano la recuperan */
    if (dormida) sumar(p, NEED_ENERGIA, 12.5f * h);
    else {
        float gasto = 4.0f * (0.8f + 0.4f * R[TRAIT_ENERGICA]);
        if (p->actividad == ACTV_SIESTA) gasto = -6.0f;
        else if (p->actividad == ACTV_SOMBRA) gasto *= 0.4f;
        else if (hace_calor(p, t) && !bebida_activa(p, FOOD_BEBIDA_FRIA, t)) gasto *= 1.0f + molestia_calor(p); /* calor */
        sumar(p, NEED_ENERGIA, -gasto * h * (gasto > 0 ? ritmo : 1.0f));
    }

    /* higiene: se ensucia sola, al digerir y al cavar */
    float sucio = (dormida ? 1.0f : 2.5f) + (p->actividad == ACTV_CAVAR ? 10.0f : 0.0f) + (p->actividad == ACTV_CHAPOTEAR && p->charcos_hasta > t ? 6.0f : 0.0f) +
                  (clima == CLIMA_VIENTO && !refugio ? 1.0f : 0.0f);
    if (pet_always_clean(p, t)) { sucio = 0; p->barro = 0; p->hediondo = p->hediondo_min = 0; } /* (v38) con el broche de Tilde no se ensucia */
    if (p->hediondo) { /* (fw 103) el olor del zorrillo se va solo en unas 3 h (o con un baño); mientras tanto se ensucia más */
        const uint32_t mins = (uint32_t)(dt / 60.0f) + p->hediondo_min; /* los minutos sueltos quedan en la partida: el olor no se reinicia con la placa */
        const uint32_t pasos = mins / 10u;
        p->hediondo_min = (uint8_t)(mins % 10u);
        p->hediondo = pasos >= p->hediondo ? 0 : (uint8_t)(p->hediondo - pasos);
        sucio += 2.0f;
    }
    sumar(p, NEED_HIGIENE, -sucio * h * ritmo);
    if (p->digestion > 0) {
        float d = fminf(p->digestion, 0.25f * h); /* digiere en ~4 h */
        p->digestion -= d;
        sumar(p, NEED_HIGIENE, -d * 30.0f);
    }

    paso_cacas(p);

    /* diversión, cariño y curiosidad */
    if (!dormida) {
        float aburre = 4.5f * (0.7f + 0.6f * R[TRAIT_ENERGICA]);
        if (p->actividad == ACTV_JUGAR_SOLA || p->actividad == ACTV_NIEVE || p->actividad == ACTV_CHAPOTEAR || p->actividad == ACTV_PELOTA) aburre = -8.0f;
        else if (p->actividad == ACTV_PROBAR) aburre = -3.0f;
        sumar(p, NEED_DIVERSION, -aburre * h * (aburre > 0 ? ritmo : 1.0f));
        sumar(p, NEED_CARINO, -2.5f * (0.6f + 0.8f * R[TRAIT_CARINOSA]) * hc * ritmo);
        float rutina = 3.0f * (0.6f + 0.8f * R[TRAIT_CURIOSA]);
        if (p->actividad == ACTV_EXPLORAR || p->actividad == ACTV_OLFATEAR || p->actividad == ACTV_CAVAR) rutina = -30.0f;
        if (p->actividad == ACTV_MIRAR) rutina = -10.0f;
        sumar(p, NEED_CURIOSIDAD, -rutina * h * (rutina > 0 ? ritmo : 1.0f));
    } else {
        sumar(p, NEED_CARINO, -0.5f * hc * ritmo);
    }

    /* seguridad: vuelve sola a la calma; tormenta, noche al aire libre y lugares feos la bajan */
    float calma = p->necesidad[NEED_SEGURIDAD] < 80.0f ? 8.0f : 0.0f; /* se calma sola; del todo tranquila, contigo */
    if (clima == CLIMA_TORMENTA && !(refugio && p->actividad == ACTV_REFUGIO))
        calma = -12.0f * (0.5f + R[TRAIT_TIMIDA]) * (refugio ? 0.6f : 1.0f);
    if (p->memoria.miedo[p->lugar] > 0.3f && !dormida) calma -= 10.0f * p->memoria.miedo[p->lugar];
    if (p->actividad == ACTV_REFUGIO) calma += 10.0f;
    sumar(p, NEED_SEGURIDAD, calma * h);

    /* frío: cansa y, a las que no les gusta, las deja incómodas; abrigarse en la casita lo evita */
    const bool abrigada = (p->actividad == ACTV_ABRIGARSE && refugio) || bebida_activa(p, FOOD_BEBIDA_CALIENTE, t) || ropa_abriga(p);
    if (clima == CLIMA_FRIO && !abrigada) {
        float molesta = p->memoria.clima[CLIMA_FRIO] < 0 ? (float)(-p->memoria.clima[CLIMA_FRIO]) / 100.0f : 0.0f;
        if (!dormida) sumar(p, NEED_ENERGIA, -(1.0f + 2.0f * molesta) * h);
        sumar(p, NEED_SEGURIDAD, -4.0f * molesta * h);
        if (!dormida && p->actividad != ACTV_ABRIGARSE) sumar(p, NEED_CARINO, -1.5f * molesta * h); /* busca calor */
    }

    /* salud: nunca baja de un piso; la ausencia no la enferma en serio */
    float piso = p->modo_nino ? 55.0f : 35.0f;
    if (p->enferma) {
        bool cuidada = N[NEED_SACIEDAD] > 45 && N[NEED_HIGIENE] > 45 && N[NEED_ENERGIA] > 30;
        if (N[NEED_SALUD] > piso) sumar(p, NEED_SALUD, (cuidada ? -1.0f : -2.5f) * h);
        /* bien atendida (o tras un tiempo), se mejora sola */
        float cura = (cuidada ? 0.08f : 0.03f) + (t - p->enfermo_desde > 36u * 3600u ? 0.2f : 0.0f);
        if (chance(p, prob_paso(cura, dt))) sanar(p, t);
    } else {
        float dano = 0;
        if (N[NEED_SACIEDAD] < 15) dano += 1.5f;
        if (N[NEED_HIGIENE] < 15) dano += 1.0f;
        if (N[NEED_ENERGIA] < 10) dano += 1.0f;
        if (dano > 0 && N[NEED_SALUD] > piso) sumar(p, NEED_SALUD, -dano * h);
        else if (dano == 0) sumar(p, NEED_SALUD, 1.5f * h);
        float riesgo = 0.0015f;
        if (N[NEED_SACIEDAD] < 20) riesgo += 0.008f;
        if (N[NEED_HIGIENE] < 20) riesgo += 0.010f;
        if (N[NEED_SALUD] < 50) riesgo += 0.006f;
        riesgo += 0.004f * p->dulces;
        if (clima == CLIMA_FRIO && !abrigada && !refugio) riesgo += 0.006f; /* un resfrío leve */
        if (sola) riesgo *= 0.3f;
        if (p->modo_nino) riesgo *= 0.5f;
        if (p->modo_nino == MODO_INFANTE) riesgo = 0; /* con los más chicos nunca se enferma */
        if (chance(p, prob_paso(riesgo, dt))) enfermar(p, t);
    }
    /* pisos de la ausencia: triste y con hambre, pero nunca en peligro */
    if (sola) {
        float piso_n = p->modo_nino ? 30.0f : 20.0f;
        for (int i = 0; i < NEED_SALUD; i++) if (N[i] < piso_n) N[i] = piso_n;
    }

    p->dulces = decae(p->dulces, h, 6.0f);
    for (int e = 0; e < EMO_COUNT; e++) p->impulso[e] = decae(p->impulso[e], h, e == EMO_CELOS || e == EMO_MIEDO ? 0.5f : 2.0f);
    olvidar(p, h);

    /* el vínculo se cuida: si pasan días sin verte, baja un poco (nunca de 10) */
    if (t - p->ultima_interaccion > 3u * 86400u && p->vinculo > 10.0f) p->vinculo = fmaxf(10.0f, p->vinculo - 2.0f * h / 24.0f);

    /* sueño */
    if (!dormida) {
        bool agotada = N[NEED_ENERGIA] < 10;
        bool luz = p->luz_apagada && N[NEED_ENERGIA] < 85;
        /* (fw 104) terquedad: si recién estaban jugando, resiste el sueño un rato (bosteza, se frota los ojos, sigue).
         * Las dóciles se duermen al tiro; las más tercas aguantan hasta ~40 min desde el último toque. Sola (sin toques
         * recientes) nada cambia: la prueba P0 sigue igual. */
        const float terca = pet_stubbornness(p);
        const uint32_t aguante = 300u + (uint32_t)(2100.0f * terca);
        if (luz && !agotada && N[NEED_ENERGIA] > 25 && terca > 0.35f && pet_mode(p) != MODO_INFANTE &&
            t - p->ultima_interaccion < aguante) {
            luz = false;
            /* (1.0.12) antes: chance(prob_paso(1.2f, dt)), pero prob_paso recorta a 1 por hora → probabilidad 100 % en CADA paso:
             * bostezaba y decía «¡UN RATITO MÁS!» cada segundo mientras la luz estuviera apagada. Ahora a lo más cada 4 minutos. */
            static uint32_t s_terca_aviso;
            if (vivo && (t - s_terca_aviso >= 240u || t < s_terca_aviso)) { s_terca_aviso = t; sonar(p, SND_BOSTEZO, t); push_event(p, EV_NIEGA, ACT_DORMIR, 0, t); }
        }
        bool hora_de_dormir = es_noche_su(p, t) && N[NEED_ENERGIA] < 60 && (t - p->ultima_interaccion) > 1800; /* (sola, a su hora: igual que siempre) */
        if (agotada || luz || hora_de_dormir) {
            if (p->escena != ESC_PRADO && p->escena != ESC_CUEVA && p->escena != ESC_ISLA) cambiar_escena(p, ESC_PRADO, t, true); /* a dormir a casa (en la isla, bajo el árbol viejo) */
            dormirse(p, t);
        }
    } else {
        bool descansada = N[NEED_ENERGIA] >= 95.0f;
        bool de_noche_y_oscuro = es_noche_su(p, t) && p->luz_apagada;
        if (descansada && !de_noche_y_oscuro && !p->luz_apagada && !es_noche_su(p, t)) despertarse(p, t, false);
        else if (N[NEED_ENERGIA] >= 100.0f && !es_noche_su(p, t)) despertarse(p, t, false);
        else if (clima == CLIMA_TORMENTA && R[TRAIT_TIMIDA] > 0.6f && chance(p, prob_paso(0.5f, dt))) {
            despertarse(p, t, false); /* un trueno la despierta */
            impulsar(p, EMO_MIEDO, 0.7f, CAUSA_TORMENTA);
        }
    }

    /* truenos */
    if (clima == CLIMA_TORMENTA && chance(p, prob_paso(0.9f, dt))) {
        if (vivo) { push_event(p, EV_TRUENO, 0, 0, t); sonar(p, SND_TRUENO, t); }
        if (!p->dormida && R[TRAIT_TIMIDA] > 0.35f && p->memoria.clima[CLIMA_TORMENTA] < 30) {
            impulsar(p, EMO_MIEDO, 0.4f + 0.5f * R[TRAIT_TIMIDA], CAUSA_TORMENTA);
            sumar(p, NEED_SEGURIDAD, -10);
            susto_aqui(p, 0.1f);
        }
    }

    /* hábitos: al cambiar de hora local se cierra la anterior */
    int hl = hora_local(p, t);
    if (hl != p->hora_actual) {
        cerrar_hora(p, p->hora_actual, !dormida);
        p->hora_actual = (int8_t)hl;
    }

    /* el cuidado promedio decide la evolución */
    float b = bienestar(p);
    p->cuidado += (b - p->cuidado) * clampf(h / 24.0f, 0, 1);

    paso_visitas(p, t, dt, vivo);
    paso_vecino(p, t, vivo);
    paso_cartas(p, t, vivo);
    paso_mundo(p, t, dt, vivo);
    paso_animales(p, t, dt, vivo);
    paso_criaturas(p, t, dt, vivo);
    paso_historia(p, t);
    paso_pistas(p, t, vivo);
    revisar_cumple(p, t);
    paso_recuerdos(p, t, dt, vivo);
}

/* ======================================================================
 * 5. Autonomía: qué hace sola y dónde
 * ==================================================================== */

static float bienestar(const pet_t *p)
{
    const float *N = p->necesidad;
    float s = 0;
    for (int i = 0; i < NEED_COUNT; i++) s += N[i];
    return s / NEED_COUNT;
}

static int lugar_favorito(const pet_t *p)
{
    int mejor = LUGAR_ESPECIE[p->especie];
    for (int l = 0; l < LUGAR_COUNT; l++)
        if (p->memoria.agrado[l] - p->memoria.miedo[l] > p->memoria.agrado[mejor] - p->memoria.miedo[mejor]) mejor = l;
    return mejor;
}

/* elige un lugar evitando los que le dan miedo */
static int elegir_lugar(pet_t *p, int preferido)
{
    if (preferido >= 0 && p->memoria.miedo[preferido] < 0.35f) return preferido;
    int menos_visto = -1;
    for (int k = 0; k < 6; k++) {
        int l = (int)(rng_next(p) % LUGAR_COUNT);
        if (p->memoria.miedo[l] >= 0.35f) continue;
        if (menos_visto < 0 || p->memoria.visitas[l] < p->memoria.visitas[menos_visto]) menos_visto = l;
    }
    return menos_visto >= 0 ? menos_visto : LUGAR_CLARO;
}

static int reaccion(pet_t *p, action_t a, int r, sound_t s, uint32_t t);
/* (fw 104) terquedad: cuánto le cuesta hacer caso cuando no quiere. Sale de sus rasgos: las traviesas y valientes son más
 * tercas; las tímidas, más dóciles. No es maldad: es carácter. */
float pet_stubbornness(const pet_t *p) { return clampf(0.6f * p->rasgo[TRAIT_TRAVIESA] + 0.4f * (1.0f - p->rasgo[TRAIT_TIMIDA]), 0, 1); }
/* ¿se niega a esta acción? La primera vez puede negarse (según lo terca); si insistes al tiro (2 min), obedece siempre.
 * En modo infante nunca se niega. Avisa con EV_NIEGA para que se vea el gesto. */
/* está metida en algo suyo que le gusta (no quiere soltarlo) */
static bool absorta_en_lo_suyo(const pet_t *p)
{
    switch (p->actividad) {
    case ACTV_CAVAR: case ACTV_PERSEGUIR: case ACTV_TREPAR: case ACTV_NADAR: case ACTV_JUGUETE: case ACTV_CHAPOTEAR:
    case ACTV_PELOTA: case ACTV_NIEVE: case ACTV_CHARLAR: case ACTV_CANTAR: case ACTV_BAILAR: case ACTV_EXPLORAR: return true;
    default: return false;
    }
}
static bool terca_niega(pet_t *p, action_t a, float prob, uint32_t t)
{
    if (pet_mode(p) == MODO_INFANTE || p->enferma) return false;
    if (p->terca_act == (uint8_t)(a + 1) && t - p->terca_t <= 300u) { p->terca_act = 0; return false; } /* insististe (dentro de 5 min): te hace caso */
    /* cuanto más te quiere, más caso te hace (con vínculo 100, la mitad de terca que recién conocidos) */
    if (!chance(p, prob * pet_stubbornness(p) * (1.0f - clampf(p->vinculo, 0, 100) / 200.0f))) return false;
    p->terca_act = (uint8_t)(a + 1);
    p->terca_t = t;
    p->ultimo_no = (uint8_t)a; /* (insistir también vale para el «no» general) */
    impulsar(p, EMO_ENOJO, 0.12f, CAUSA_NINGUNA);
    sonar(p, SND_GRUNIDO, t);
    push_event(p, EV_NIEGA, a, 0, t);
    push_event(p, EV_REACCION, a, -2, t); /* la pantalla ya sabe mostrar «no quiere» (y suelta lo que tenía pedido) */
    return true;
}
/* ---- comidas de temporada ---- */
#define Zp (1u << ESC_PRADO)
#define Zb (1u << ESC_BOSQUE)
#define Zl (1u << ESC_LAGO)
#define Zc (1u << ESC_COLINA)
#define Zq (1u << ESC_CUEVA)
static const comida_est_info_t COMIDA_EST[PET_COMIDAS_EST] = {
    /* primavera */
    {"las frutillas", "frutillas", ESTACION_PRIMAVERA, Zp | Zc, 20, 3, 4, 0},
    {"los brotes tiernos", "brotes tiernos", ESTACION_PRIMAVERA, Zb | Zp, 16, 4, 0, 0},
    {"los pétalos dulces", "pétalos dulces", ESTACION_PRIMAVERA, Zc, 12, 1, 10, 0},
    {"los berros", "berros", ESTACION_PRIMAVERA, Zl | Zb, 14, 5, 0, 0},
    /* verano */
    {"la sandía", "sandía", ESTACION_VERANO, Zp | Zl, 22, 2, 6, 0},
    {"los duraznos", "duraznos", ESTACION_VERANO, Zb | Zp, 20, 3, 5, 0},
    {"los arándanos", "arándanos", ESTACION_VERANO, Zc | Zb, 16, 4, 4, 0},
    {"el choclo", "choclo", ESTACION_VERANO, Zp, 26, 2, 0, 0},
    /* otoño */
    {"la calabaza", "calabaza", ESTACION_OTONO, Zp | Zc, 26, 3, 0, 4},
    {"las castañas", "castañas", ESTACION_OTONO, Zb, 24, 2, 2, 5},
    {"los hongos", "hongos", ESTACION_OTONO, Zb | Zq, 18, 2, 6, 0},
    {"los higos", "higos", ESTACION_OTONO, Zc | Zp, 18, 3, 6, 0},
    /* invierno */
    {"los piñones", "piñones", ESTACION_INVIERNO, Zb, 22, 3, 0, 8},
    {"el camote asado", "camote asado", ESTACION_INVIERNO, Zp | Zc, 30, 2, 0, 12},
    {"las bayas de escarcha", "bayas de escarcha", ESTACION_INVIERNO, Zc | Zl, 14, 4, 6, 4},
    {"la miel cristalizada", "miel cristalizada", ESTACION_INVIERNO, Zq, 12, 0, 12, 6},
};
#undef Zp
#undef Zb
#undef Zl
#undef Zc
#undef Zq
const comida_est_info_t *pet_seasonal_food_info(int i) { return (unsigned)i < PET_COMIDAS_EST ? &COMIDA_EST[i] : &COMIDA_EST[0]; }
int pet_seasonal_food_count(const pet_t *p, int i) { return (unsigned)i < PET_COMIDAS_EST ? p->comida_est[i] : 0; }
bool pet_seasonal_food_seen(const pet_t *p, int i) { return (unsigned)i < PET_COMIDAS_EST && ((p->comida_vista >> i) & 1u); }
int pet_seasonal_food_kinds(const pet_t *p)
{
    int n = 0;
    for (int i = 0; i < PET_COMIDAS_EST; i++) n += p->comida_est[i] > 0;
    return n;
}
void pet_seasonal_food_give(pet_t *p, int i, int n)
{
    if ((unsigned)i >= PET_COMIDAS_EST) return;
    int v = p->comida_est[i] + n;
    p->comida_est[i] = (uint8_t)(v > 9 ? 9 : (v < 0 ? 0 : v));
    p->comida_vista |= (uint16_t)(1u << i);
}
/* a cada una le gusta distinto: sale de su manera de ser y de la comida (y con el tiempo se acostumbra) */
static int gusto_est_de(const pet_t *p, int i)
{
    if (p->gusto_est[i]) return p->gusto_est[i];
    const uint32_t h = mezcla32(p->nacimiento * 2654435761u ^ (uint32_t)(i + 1) * 40503u ^ (uint32_t)p->especie * 977u);
    return 70 + (int)(h % 150u); /* 70..219 */
}
int pet_seasonal_food_taste(const pet_t *p, int i)
{
    if ((unsigned)i >= PET_COMIDAS_EST) return 1;
    const int g = gusto_est_de(p, i);
    return g < 105 ? 0 : (g > 180 ? 2 : 1);
}
/* ¿encuentra algo de temporada? según la estación y la zona donde está */
static bool hallar_comida_est(pet_t *p, uint32_t t, bool vivo)
{
    const estacion_t es = pet_season(p, t);
    float w[PET_COMIDAS_EST], tot = 0;
    for (int i = 0; i < PET_COMIDAS_EST; i++) {
        w[i] = 0;
        if (COMIDA_EST[i].estacion != es || !((COMIDA_EST[i].zonas >> p->escena) & 1u) || p->comida_est[i] >= 9) continue;
        w[i] = 1.0f;
        if (!pet_seasonal_food_seen(p, i)) w[i] += 0.5f; /* le llama la atención lo nuevo */
        tot += w[i];
    }
    if (tot <= 0) return false;
    float r = rng01(p) * tot;
    int cual = 0;
    for (int i = 0; i < PET_COMIDAS_EST; i++) { if (r < w[i]) { cual = i; break; } r -= w[i]; }
    p->comida_est[cual]++;
    p->comida_vista |= (uint16_t)(1u << cual);
    p->ultimo_hallazgo = t;
    p->hallazgos_dia++;
    sumar(p, NEED_CURIOSIDAD, 10);
    impulsar(p, EMO_ORGULLO, 0.5f, CAUSA_HALLAZGO);
    impulsar(p, EMO_ALEGRIA, 0.4f, CAUSA_HALLAZGO);
    const bool trae = vivo && p->vinculo >= 40.0f && t - p->ultima_interaccion < 15u * 60u;
    push_event(p, EV_COMIDA_EST, cual, trae ? 1 : 0, t);
    if (vivo) sonar(p, trae ? SND_ALEGRE : SND_SORPRESA, t);
    return true;
}
/* comerla: sacia, y la variedad hace bien */
static int comer_est(pet_t *p, int i, uint32_t ahora)
{
    if ((unsigned)i >= PET_COMIDAS_EST) return 0;
    float *N = p->necesidad;
    if (N[NEED_SACIEDAD] > 95) return reaccion(p, ACT_ALIMENTAR, -1, SND_QUEJA, ahora);
    if (!p->comida_est[i]) { push_event(p, EV_SIN_ITEM, ITEM_FRUTA, 0, ahora); return 0; }
    p->comida_est[i]--;
    const comida_est_info_t *c = &COMIDA_EST[i];
    const int gusto = gusto_est_de(p, i);
    if (gusto < 105) { /* no le gusta: la prueba y se acostumbra de a poco */
        sumar(p, NEED_SACIEDAD, 6);
        impulsar(p, EMO_ENOJO, 0.3f, CAUSA_COMIDA_FEA);
        p->gusto_est[i] = (uint8_t)(gusto + 6);
        registrar_habito(p, HAB_ALIMENTAR, ahora);
        return reaccion(p, ACT_ALIMENTAR, -1, SND_QUEJA, ahora);
    }
    sumar(p, NEED_SACIEDAD, c->saciedad);
    sumar(p, NEED_SALUD, c->salud);
    sumar(p, NEED_DIVERSION, c->diversion);
    p->digestion = clampf(p->digestion + 0.15f, 0, 1);
    if (c->abrigo && hace_frio(p)) { sumar(p, NEED_SEGURIDAD, c->abrigo); impulsar(p, EMO_ALEGRIA, 0.3f, CAUSA_BEBIDA); } /* calentita */
    /* la variedad: probar algo distinto a lo último que comió la deja contenta y curiosa */
    bool repetida = false;
    for (int k = 0; k < 4; k++) repetida |= p->comida_ult[k] == i + 1;
    if (!repetida) { sumar(p, NEED_CURIOSIDAD, 8); sumar(p, NEED_SALUD, 1); impulsar(p, EMO_CURIOSIDAD, 0.4f, CAUSA_NOVEDAD); }
    for (int k = 3; k > 0; k--) p->comida_ult[k] = p->comida_ult[k - 1];
    p->comida_ult[0] = (uint8_t)(i + 1);
    p->gusto_est[i] = (uint8_t)(gusto + ((220 - gusto) * 5) / 100); /* familiaridad */
    if (gusto > 180) impulsar(p, EMO_ALEGRIA, 0.6f, CAUSA_COMIDA_RICA);
    registrar_habito(p, HAB_ALIMENTAR, ahora);
    return reaccion(p, ACT_ALIMENTAR, gusto > 180 ? 1 : 0, SND_COMER, ahora);
}

static void hallar(pet_t *p, uint32_t t, bool vivo, float suerte)
{
    const float *R = p->rasgo;
    int32_t hoy = dia_local(p, t);
    if (hoy != p->dia_hallazgos) { p->dia_hallazgos = hoy; p->hallazgos_dia = 0; }
    if (p->hallazgos_dia >= 12 || t - p->ultimo_hallazgo < 30u * 60u) return;
    suerte *= 1.0f + p->vinculo / 200.0f; /* con confianza busca más cosas para ti */
    if (!chance(p, (0.25f + 0.3f * R[TRAIT_CURIOSA]) * suerte)) return;
    if (quizas_objeto(p, OFU_EXPLORAR, 0.5f, t)) { /* ¡algo mágico! (va a la Bolsa) */
        p->ultimo_hallazgo = t;
        p->hallazgos_dia++;
        if (vivo) sonar(p, SND_SORPRESA, t);
        return;
    }

    if (chance(p, 0.4f) && hallar_comida_est(p, t, vivo)) return; /* a veces es algo de temporada */
    /* qué encuentra depende de la estación y de dónde está */
    estacion_t e = pet_season(p, t);
    float pesos[HALL_COUNT] = {1.0f, 0.3f, 0.7f, 1.4f, 1.5f, 0.6f + R[TRAIT_TRAVIESA], 0.2f + 0.5f * R[TRAIT_TRAVIESA], 0.3f, 0.4f};
    if (!pet_has_decor(p, DECOR_CASITA)) { pesos[HALL_RAMITA] += 0.8f; pesos[HALL_MUSGO] += 0.4f; } /* (v38) sin casita, junta materiales */
    if (e == ESTACION_OTONO) pesos[HALL_HOJA_SECA] += 1.0f;
    if (e == ESTACION_VERANO) pesos[HALL_FRUTA] += 1.5f;
    if (e == ESTACION_OTONO) pesos[HALL_SEMILLA] += 1.5f;
    if (e == ESTACION_PRIMAVERA) { pesos[HALL_MIEL] += 0.8f; pesos[HALL_HIERBA] += 0.6f; }
    if (e == ESTACION_INVIERNO) pesos[HALL_PIEDRA] += 1.2f;
    if (p->lugar == LUGAR_ROCAS) pesos[HALL_PIEDRA] += 1.5f;
    if (p->lugar == LUGAR_ARBOL) pesos[HALL_FRUTA] += 1.0f;
    if (p->enferma) pesos[HALL_HIERBA] += 2.0f;
    switch (p->escena) { /* cada zona esconde cosas distintas */
    case ESC_BOSQUE: pesos[HALL_SEMILLA] += 1.0f; pesos[HALL_HIERBA] += 1.0f; pesos[HALL_RAMITA] += 1.0f; pesos[HALL_MUSGO] += 1.0f; break;
    case ESC_LAGO: pesos[HALL_PIEDRA] += 1.5f; pesos[HALL_MUSGO] += 0.6f; break;
    case ESC_COLINA: pesos[HALL_MIEL] += 1.0f; pesos[HALL_SEMILLA] += 0.8f; break;
    case ESC_CUEVA: pesos[HALL_PIEDRA] += 3.0f; pesos[HALL_FRUTA] = 0.2f; break;
    default: break;
    }
    float total = 0;
    for (int i = 0; i < HALL_COUNT; i++) total += pesos[i];
    float r = rng01(p) * total;
    int cual = HALL_RAMITA;
    for (int i = 0; i < HALL_COUNT; i++) { if (r < pesos[i]) { cual = i; break; } r -= pesos[i]; }

    static const int8_t A_ITEM[HALL_COUNT] = {ITEM_FRUTA, ITEM_MIEL, ITEM_HIERBA, ITEM_SEMILLA, ITEM_PIEDRA, -1, -1, -1, -1};
    if (A_ITEM[cual] >= 0) guardar_item(p, (item_t)A_ITEM[cual]);
    guardar_material(p, cual);
    p->ultimo_hallazgo = t;
    p->hallazgos_dia++;
    sumar(p, NEED_CURIOSIDAD, 15);
    impulsar(p, EMO_ORGULLO, 0.6f, CAUSA_HALLAZGO);
    impulsar(p, EMO_ALEGRIA, 0.4f, CAUSA_HALLAZGO);
    /* con confianza te lo trae de regalo (aunque sea una ramita) */
    bool trae = vivo && p->vinculo >= 40.0f && t - p->ultima_interaccion < 15u * 60u;
    push_event(p, EV_HALLAZGO, cual, trae ? 1 : 0, t);
    if (trae) anotar(p, DIA_REGALO, cual, 0, t);
    if (vivo) sonar(p, trae ? SND_ALEGRE : SND_SORPRESA, t);
}

/* ======================================================================
 * Carácter, planes y trato: la capa de intención sobre elegir_actividad.
 * No cambia lo urgente (miedo, frío, sueño): inclina lo que le apetece hacer.
 * - Humor del día: amanece distinta cada día según su manera de ser, el clima y cómo la trataste.
 * - Manías: cosas suyas, fijas, que la distinguen de otra mascota (una favorita, una que esquiva, una que hace a cierta hora).
 * - Planes: tres pasos con sentido (ir al lago, pescar, chapotear, siesta) en vez de actividades sueltas.
 * - Trato: lo que haces deja huella un rato en cómo se porta (se acurruca, te invita a jugar, se enfurruña, perdona).
 * ==================================================================== */
#define HM(h) (1u << (h))
#define NP 0xFF
typedef struct { uint8_t destino; uint8_t paso[3]; uint8_t humores; } plan_def_t;
static const plan_def_t PLANES[PLAN_COUNT] = {
    [PLAN_NINGUNO] = {NP, {NP, NP, NP}, 0},
    [PLAN_LAGO] = {ESC_LAGO, {ACTV_PESCAR, ACTV_CHAPOTEAR, ACTV_SIESTA}, HM(HUMOR_AVENTURERA) | HM(HUMOR_SONADORA)},
    [PLAN_BOSQUE_BUSCAR] = {ESC_BOSQUE, {ACTV_BUSCAR, ACTV_OLFATEAR, ACTV_CAVAR}, HM(HUMOR_AVENTURERA)},
    [PLAN_BOSQUE_TREPAR] = {ESC_BOSQUE, {ACTV_TREPAR, ACTV_AVES, ACTV_SIESTA}, HM(HUMOR_AVENTURERA) | HM(HUMOR_JUGUETONA)},
    [PLAN_COLINA_TESORO] = {ESC_COLINA, {ACTV_EXPLORAR, ACTV_OLFATEAR, ACTV_CAVAR}, HM(HUMOR_AVENTURERA)},
    [PLAN_ESTRELLAS] = {NP, {ACTV_MIRAR, ACTV_CANTAR, ACTV_SIESTA}, HM(HUMOR_SONADORA) | HM(HUMOR_PEREZOSA)},
    [PLAN_DESFILE] = {NP, {ACTV_PROBAR, ACTV_BAILAR, ACTV_ESPERAR}, HM(HUMOR_JUGUETONA) | HM(HUMOR_SOCIAL) | HM(HUMOR_SONADORA)},
    [PLAN_JUEGO] = {NP, {ACTV_PELOTA, ACTV_JUGAR_SOLA, ACTV_SIESTA}, HM(HUMOR_JUGUETONA)},
    [PLAN_TERTULIA] = {NP, {ACTV_CHARLAR, ACTV_AVES, ACTV_MIRAR}, HM(HUMOR_SOCIAL) | HM(HUMOR_SONADORA)},
    [PLAN_EXTRANA] = {NP, {ACTV_PASEAR, ACTV_MIRAR, ACTV_ESPERAR}, HM(HUMOR_SOCIAL) | HM(HUMOR_PEREZOSA)},
};

int pet_humor(const pet_t *p) { return p->humor < HUMOR_COUNT ? p->humor : HUMOR_NORMAL; }
int pet_plan(const pet_t *p) { return p->plan_tipo < PLAN_COUNT ? p->plan_tipo : PLAN_NINGUNO; }
int pet_plan_step(const pet_t *p) { return p->plan_paso < 3 ? p->plan_paso : 2; }
const char *pet_humor_name(int h)
{
    static const char *const T[HUMOR_COUNT] = {"normal", "aventurera", "perezosa", "sociable", "juguetona", "soñadora"};
    return (unsigned)h < HUMOR_COUNT ? T[h] : "?";
}
const char *pet_plan_name(int pl)
{
    static const char *const T[PLAN_COUNT] = {"ninguno", "ir a la laguna", "buscar en el bosque", "trepar en el bosque", "buscar un tesoro en el cerro",
                                              "mirar las estrellas", "hacer un desfile", "jugar y descansar", "charlar con los animales", "esperarte"};
    return (unsigned)pl < PLAN_COUNT ? T[pl] : "?";
}
int pet_plan_activity(int plan, int paso)
{
    if ((unsigned)plan >= PLAN_COUNT || (unsigned)paso > 2 || PLANES[plan].paso[paso] == NP) return -1;
    return PLANES[plan].paso[paso];
}
int pet_plan_scene(int plan) { return (unsigned)plan < PLAN_COUNT && PLANES[plan].destino != NP ? PLANES[plan].destino : -1; }

/* el trato, con las horas ya aplicadas: se apaga a la mitad cada 6 horas sin verla */
int pet_treat(const pet_t *p, uint32_t ahora)
{
    const uint32_t s = ahora > p->ultima_interaccion ? ahora - p->ultima_interaccion : 0;
    return (int)((float)p->trato * powf(0.5f, (float)s / (6.0f * HORA)));
}

void pet_quirks(const pet_t *p, int *favorita, int *esquiva, int *hora_actividad, int *hora)
{
    static const uint8_t M[] = {ACTV_MIRAR, ACTV_CANTAR, ACTV_BAILAR, ACTV_CAVAR, ACTV_AVES, ACTV_CHAPOTEAR, ACTV_PELOTA,
                                ACTV_PROBAR, ACTV_EXPLORAR, ACTV_BUSCAR, ACTV_CHARLAR, ACTV_JUGAR_SOLA, ACTV_OLFATEAR, ACTV_PASEAR};
    const unsigned n = (unsigned)sizeof M;
    uint32_t s = mezcla32((p->id ? p->id : p->nacimiento) ^ 0x5EED1234u ^ ((uint32_t)p->especie << 24));
    const int a = M[s % n];
    s = mezcla32(s);
    int b = M[s % n];
    if (b == a) b = M[(s % n + 1u) % n];
    s = mezcla32(s);
    const int c = M[s % n];
    s = mezcla32(s);
    if (favorita) *favorita = a;
    if (esquiva) *esquiva = b;
    if (hora_actividad) *hora_actividad = c;
    if (hora) *hora = 7 + (int)(s % 13u); /* entre las 7 y las 19 */
}

/* amanece con un humor: una vez por día, sorteado según su manera de ser, el clima y cómo la trataste */
static void elegir_humor(pet_t *p, uint32_t t, bool vivo)
{
    const uint8_t hoy = hoy_byte(p, t);
    if (p->humor_dia == hoy) return;
    p->humor_dia = hoy;
    /* (fw 104) el trocito de hielo: cada día fuera del invierno se derrite un poco; al segundo, chao (vuelve a pescarse el próximo invierno) */
    if ((p->coleccion[COL_CRISTALES] >> 3) & 1u) {
        if (pet_season(p, t) == ESTACION_INVIERNO) p->hielo_fuera = 0;
        else if (++p->hielo_fuera >= 2) {
            p->coleccion[COL_CRISTALES] &= (uint8_t)~(1u << 3);
            p->hielo_fuera = 0;
            anotar(p, DIA_HIELO, 1, 0, t);
            push_event(p, EV_HIELO, 1, 0, t);
            impulsar(p, EMO_TRISTEZA, 0.3f, CAUSA_TESORO);
        }
    }
    const float *R = p->rasgo;
    const clima_t c = pet_weather(p);
    const float tr = (float)pet_treat(p, t) / 100.0f;
    float w[HUMOR_COUNT], tot = 0;
    w[HUMOR_NORMAL] = 1.2f;
    w[HUMOR_AVENTURERA] = 0.6f + 1.4f * R[TRAIT_CURIOSA] + (c == CLIMA_DESPEJADO ? 0.6f : 0.0f);
    w[HUMOR_PEREZOSA] = 0.6f + 1.2f * (1.0f - R[TRAIT_ENERGICA]) + (c == CLIMA_LLUVIA || c == CLIMA_FRIO || c == CLIMA_NUBLADO ? 0.6f : 0.0f);
    w[HUMOR_SOCIAL] = 0.6f + 1.4f * R[TRAIT_CARINOSA] + (tr > 0.2f ? 0.6f : 0.0f);
    w[HUMOR_JUGUETONA] = 0.6f + 1.4f * R[TRAIT_ENERGICA] + (tr > 0.2f ? 0.4f : 0.0f);
    w[HUMOR_SONADORA] = 0.5f + 0.6f * R[TRAIT_CURIOSA] + 0.5f * R[TRAIT_TIMIDA] + (c == CLIMA_NUBLADO || c == CLIMA_LLUVIA ? 0.7f : 0.0f);
    if (p->humor < HUMOR_COUNT) w[p->humor] *= 0.5f; /* rara vez dos días igual */
    for (int i = 0; i < HUMOR_COUNT; i++) tot += w[i];
    float r = rng01(p) * tot;
    int h = HUMOR_NORMAL;
    for (int i = 0; i < HUMOR_COUNT; i++) { if (r < w[i]) { h = i; break; } r -= w[i]; }
    p->humor = (uint8_t)h;
    if (vivo && !p->dormida && h != HUMOR_NORMAL) push_event(p, EV_HUMOR, h, 0, t);
}

static void escalar_u(float *u, const uint8_t *lst, int n, float k)
{
    for (int i = 0; i < n; i++) { const int a = lst[i]; if (u[a] > 0 && u[a] < 0.6f) u[a] *= k; } /* lo urgente (0.6 o más) no se toca */
}

/* inclina las ganas por su humor del día y por sus manías */
static void aplicar_caracter(pet_t *p, float *u, int hl)
{
    static const uint8_t AV[] = {ACTV_EXPLORAR, ACTV_OLFATEAR, ACTV_TREPAR, ACTV_NADAR, ACTV_PERSEGUIR, ACTV_BUSCAR, ACTV_CAVAR};
    static const uint8_t AV_MENOS[] = {ACTV_MIRAR, ACTV_ESPERAR};
    static const uint8_t PE[] = {ACTV_SIESTA, ACTV_MIRAR, ACTV_ESPERAR, ACTV_PASEAR};
    static const uint8_t PE_MENOS[] = {ACTV_EXPLORAR, ACTV_PELOTA, ACTV_TREPAR, ACTV_NADAR, ACTV_PERSEGUIR};
    static const uint8_t SO[] = {ACTV_CHARLAR, ACTV_ESPERAR, ACTV_AVES, ACTV_JUNTOS, ACTV_PASEAR};
    static const uint8_t JU[] = {ACTV_JUGAR_SOLA, ACTV_PELOTA, ACTV_CHAPOTEAR, ACTV_BAILAR, ACTV_CANTAR, ACTV_NIEVE, ACTV_JUGUETE};
    static const uint8_t SN[] = {ACTV_MIRAR, ACTV_AVES, ACTV_CANTAR, ACTV_PROBAR, ACTV_PASEAR};
    static const uint8_t SN_MENOS[] = {ACTV_PELOTA, ACTV_CAVAR};
#define ESC(L, k) escalar_u(u, L, (int)sizeof L, k)
    switch (pet_humor(p)) {
    case HUMOR_AVENTURERA: ESC(AV, 1.5f); ESC(AV_MENOS, 0.7f); break;
    case HUMOR_PEREZOSA: ESC(PE, 1.5f); ESC(PE_MENOS, 0.6f); if (p->necesidad[NEED_ENERGIA] < 80 && u[ACTV_SIESTA] <= 0) u[ACTV_SIESTA] = 0.3f; break;
    case HUMOR_SOCIAL: ESC(SO, 1.6f); break;
    case HUMOR_JUGUETONA: ESC(JU, 1.5f); break;
    case HUMOR_SONADORA: ESC(SN, 1.5f); ESC(SN_MENOS, 0.7f); break;
    default: break;
    }
#undef ESC
    int fav, esq, hact, hora;
    pet_quirks(p, &fav, &esq, &hact, &hora);
    if (u[fav] > 0 && u[fav] < 0.6f) u[fav] *= 1.35f;
    if (u[esq] > 0 && u[esq] < 0.6f) u[esq] *= 0.7f;
    if (hl >= hora && hl <= hora + 1 && u[hact] > 0 && u[hact] < 0.6f) u[hact] *= 1.8f; /* a su hora */
}

/* ¿se puede (y apetece) este plan ahora? */
static bool plan_posible(const pet_t *p, int id, uint32_t t, int hl, clima_t c)
{
    const float *N = p->necesidad;
    const bool lindo = c == CLIMA_DESPEJADO || c == CLIMA_NUBLADO;
    const bool seco = c != CLIMA_TORMENTA && c != CLIMA_LLUVIA && c != CLIMA_NIEVE && c != CLIMA_FRIO;
    const uint32_t sin_verte = t > p->ultima_interaccion ? t - p->ultima_interaccion : 0;
    const int d = PLANES[id].destino;
    if (d != NP && !pet_scene_open(p, (escena_t)d)) return false;
    switch (id) {
    case PLAN_LAGO: return hl >= 9 && hl < 18 && lindo && N[NEED_ENERGIA] > 50;
    case PLAN_BOSQUE_BUSCAR: return hl >= 8 && hl < 18 && seco;
    case PLAN_BOSQUE_TREPAR: return hl >= 8 && hl < 18 && lindo && N[NEED_ENERGIA] > 55;
    case PLAN_COLINA_TESORO: return hl >= 8 && hl < 18 && lindo;
    case PLAN_ESTRELLAS: return hl >= 19 && hl < 23 && c == CLIMA_DESPEJADO && p->escena != ESC_CUEVA;
    case PLAN_DESFILE: {
        bool algo = p->ropa_tiene != 0;
        for (int o = 0; o < PET_OBJETOS && !algo; o++) algo = p->bolsa[o] != 0;
        return algo && N[NEED_ENERGIA] > 35;
    }
    case PLAN_JUEGO: return N[NEED_ENERGIA] > 60 && N[NEED_DIVERSION] < 85 && hl >= 8 && hl < 19 && seco;
    case PLAN_TERTULIA: return hl >= 8 && hl < 19 && c != CLIMA_TORMENTA;
    case PLAN_EXTRANA: return p->vinculo > 30.0f && sin_verte >= 5400u && sin_verte < 10u * 3600u && hl >= 8 && hl < 21;
    default: return false;
    }
}

static void plan_terminar(pet_t *p, uint32_t t, bool vivo, bool cumplido)
{
    const int id = pet_plan(p);
    p->plan_tipo = 0;
    p->plan_paso = 0;
    p->plan_fallos = (uint8_t)(id << 4); /* los 4 bits de arriba recuerdan cuál fue (para no repetirlo enseguida) */
    if (!id) return;
    if (cumplido) {
        impulsar(p, EMO_ORGULLO, 0.5f, CAUSA_EXPLORAR);
        impulsar(p, EMO_ALEGRIA, 0.3f, CAUSA_EXPLORAR);
        sumar(p, NEED_CURIOSIDAD, 8);
    }
    if (vivo) push_event(p, EV_PLAN, id, cumplido ? 1 : 2, t);
}

/* arranca el plan; si queda lejos, parte hacia allá (devuelve true si cambió de zona) */
static bool plan_iniciar(pet_t *p, int id, uint32_t t, bool vivo)
{
    p->plan_tipo = (uint8_t)id;
    p->plan_paso = 0;
    p->plan_fallos = (uint8_t)(p->plan_fallos & 0xF0);
    if (vivo) push_event(p, EV_PLAN, id, 0, t);
    const int d = PLANES[id].destino;
    if (d != NP && p->escena != d) { cambiar_escena(p, (escena_t)d, t, true); return true; }
    return false;
}

static int plan_elegir(pet_t *p, uint32_t t, int hl, clima_t c)
{
    float w[PLAN_COUNT], tot = 0;
    const float *R = p->rasgo;
    const int hum = pet_humor(p), ultimo = p->plan_fallos >> 4;
    const uint32_t sin_verte = t > p->ultima_interaccion ? t - p->ultima_interaccion : 0;
    int fav, esq, hact, hora;
    pet_quirks(p, &fav, &esq, &hact, &hora);
    for (int id = 1; id < PLAN_COUNT; id++) {
        w[id] = 0;
        if (!plan_posible(p, id, t, hl, c)) continue;
        float x = 1.0f;
        if (PLANES[id].humores & HM(hum)) x *= 2.2f;
        if (id == PLAN_BOSQUE_BUSCAR || id == PLAN_COLINA_TESORO) x *= 0.6f + 0.8f * R[TRAIT_CURIOSA];
        if (id == PLAN_JUEGO || id == PLAN_BOSQUE_TREPAR || id == PLAN_LAGO) x *= 0.6f + 0.8f * R[TRAIT_ENERGICA];
        if (id == PLAN_DESFILE) x *= 0.6f + 0.6f * R[TRAIT_TRAVIESA] + 0.4f * R[TRAIT_CARINOSA];
        if (id == PLAN_EXTRANA) x *= 0.5f + R[TRAIT_CARINOSA] + (sin_verte > 3u * 3600u ? 0.8f : 0.0f);
        for (int k = 0; k < 3; k++) { const int a = PLANES[id].paso[k]; if (a == fav) x *= 1.3f; if (a == esq) x *= 0.8f; }
        if (id == ultimo) x *= 0.3f;
        w[id] = x;
        tot += x;
    }
    if (tot <= 0) return 0;
    float r = rng01(p) * tot;
    for (int id = 1; id < PLAN_COUNT; id++) { if (r < w[id]) return id; r -= w[id]; }
    return 0;
}

/* al empezar a elegir: sigue, termina o arranca un plan. Devuelve true si cambió de zona (elegir_actividad termina ahí) */
static bool plan_antes(pet_t *p, uint32_t t, bool vivo)
{
    if (p->plan_tipo >= PLAN_COUNT) p->plan_tipo = 0;
    if (p->plan_tipo) {
        const plan_def_t *d = &PLANES[p->plan_tipo];
        if (t - p->actividad_hasta > 1800u || (p->plan_fallos & 15u) >= 3 || p->enferma || p->dormida) { plan_terminar(p, t, vivo, false); return false; }
        if (p->plan_paso >= 3 || d->paso[p->plan_paso] == NP) { plan_terminar(p, t, vivo, true); return false; }
        if (d->destino != NP && p->escena != d->destino) {
            const clima_t c = pet_weather(p);
            if (p->plan_paso == 0 && pet_scene_open(p, (escena_t)d->destino) && c != CLIMA_TORMENTA) { cambiar_escena(p, (escena_t)d->destino, t, true); return true; }
            plan_terminar(p, t, vivo, false); /* ya no está en la zona del plan (el clima la mandó de vuelta, o se fue) */
        }
        return false;
    }
    /* sin plan: si está tranquila, a veces se le ocurre uno */
    const float *N = p->necesidad;
    if (p->enferma || p->dormida || p->etapa < STAGE_CRIA || p->escena == ESC_ISLA || N[NEED_ENERGIA] <= 40 || N[NEED_SEGURIDAD] < 50 || p->emo[EMO_MIEDO] >= 0.3f) return false;
    const float prob = 0.12f + 0.10f * p->rasgo[TRAIT_CURIOSA] + (pet_humor(p) == HUMOR_AVENTURERA ? 0.06f : 0.0f);
    if (!chance(p, prob)) return false;
    const int id = plan_elegir(p, t, hora_local(p, t), pet_weather(p));
    return id ? plan_iniciar(p, id, t, vivo) : false;
}

/* los pasos del plan pesan fuerte (salvo que haya algo urgente: entonces espera) */
static void plan_sesgo(pet_t *p, float *u, int *lugar, bool urgente)
{
    if (!p->plan_tipo) return;
    const plan_def_t *d = &PLANES[p->plan_tipo];
    const float *N = p->necesidad;
    while (p->plan_paso < 3) {
        const int act = d->paso[p->plan_paso];
        if (act == NP) return;
        if (u[act] <= 0) { /* algunas se pueden hacer siempre, aunque nada las pida */
            const bool libre = act == ACTV_MIRAR || act == ACTV_CANTAR || act == ACTV_BAILAR || act == ACTV_ESPERAR || act == ACTV_PASEAR ||
                               (act == ACTV_SIESTA && N[NEED_ENERGIA] < 85);
            if (libre && N[NEED_ENERGIA] > 25 && !p->enferma) u[act] = 0.3f;
        }
        if (u[act] > 0) {
            if (urgente || N[NEED_ENERGIA] < 30) { if ((p->plan_fallos & 15u) < 15) p->plan_fallos++; return; } /* lo urgente primero: el plan espera */
            u[act] = fmaxf(u[act], 0.5f) + 1.2f;
            if (act == ACTV_ESPERAR && lugar[act] < 0) lugar[act] = lugar_favorito(p);
            return;
        }
        p->plan_paso++; /* ahora no se puede: pasa al siguiente */
        if ((p->plan_fallos & 15u) < 15) p->plan_fallos++;
    }
}

/* lo que haces deja huella: sube con mimos y juegos, baja con sustos y despertares; se apaga con las horas */
static int trato_registrar(pet_t *p, action_t a, int r, uint32_t ausente)
{
    const int antes = (int)((float)p->trato * powf(0.5f, (float)ausente / (6.0f * HORA)));
    int d = 0;
    switch (a) {
    case ACT_ACARICIAR: d = r >= 0 ? 4 : -3; break;
    case ACT_CEPILLAR: d = r > 0 ? 3 : 1; break;
    case ACT_JUGAR: case ACT_LANZAR: d = r >= 0 ? 5 : -2; break;
    case ACT_ALIMENTAR: d = r > 0 ? 3 : (r == 0 ? 1 : -1); break;
    case ACT_BANAR: d = r < 0 ? -3 : 1; break;
    case ACT_SACUDIR: d = r < 0 ? -8 : 2; break;
    case ACT_DESPERTAR: d = r < 0 ? -10 : 0; break;
    case ACT_CURAR: d = 2; break;
    case ACT_TRUCO: d = r > 0 ? 3 : 0; break;
    default: break;
    }
    p->trato = (int8_t)clampf((float)(antes + d), -100, 100);
    return antes;
}

/* ...y reacciona en el momento, a su manera */
static void reaccion_viva(pet_t *p, action_t a, int r, int antes, uint32_t t)
{
    if (p->dormida || p->etapa == STAGE_HUEVO || p->enferma) return;
    const int tr = p->trato;
    const bool mimo = a == ACT_ACARICIAR || a == ACT_CEPILLAR, juego = a == ACT_JUGAR || a == ACT_LANZAR;
    if (mimo && r >= 0 && antes <= -25 && tr > -10) { /* ya te perdonó */
        impulsar(p, EMO_CARINO, 0.6f, CAUSA_CARICIAS);
        push_event(p, EV_ANTOJO, 3, 0, t);
    } else if (mimo && r >= 0 && tr >= 40 && p->vinculo > 20.0f && chance(p, 0.25f)) { /* quiere más: se queda pegada a ti */
        p->actividad = ACTV_ESPERAR;
        p->actividad_hasta = t + 150u;
        impulsar(p, EMO_CARINO, 0.4f, CAUSA_CARICIAS);
        push_event(p, EV_ANTOJO, 0, 0, t);
    } else if (juego && r >= 0 && tr >= 30 && p->necesidad[NEED_ENERGIA] > 45 && !p->plan_tipo &&
               plan_posible(p, PLAN_JUEGO, t, hora_local(p, t), pet_weather(p)) && chance(p, 0.3f)) { /* se entusiasma: ahora quiere jugar de verdad */
        plan_iniciar(p, PLAN_JUEGO, t, true);
        push_event(p, EV_ANTOJO, 1, 0, t);
    } else if ((a == ACT_SACUDIR || a == ACT_DESPERTAR) && r < 0 && tr <= -20) { /* se enfurruña: se va a su rincón un rato */
        if (p->plan_tipo) plan_terminar(p, t, true, false);
        p->actividad = ACTV_REFUGIO;
        p->lugar = tiene_refugio(p) ? LUGAR_CASITA : LUGAR_ARBOL;
        p->actividad_hasta = t + 120u;
        push_event(p, EV_ANTOJO, 2, 0, t);
    }
}

static void elegir_actividad(pet_t *p, uint32_t t, bool vivo)
{
    const float *N = p->necesidad, *R = p->rasgo;
    const memoria_t *m = &p->memoria;
    clima_t c = pet_weather(p);
    estacion_t est = pet_season(p, t);
    int hl = hora_local(p, t);
    bool refugio = tiene_refugio(p);

    elegir_humor(p, t, vivo);
    if (p->actividad == ACTV_PERSEGUIR) fin_persecucion(p, t, vivo);
    if (p->actividad == ACTV_CAVAR && !p->tesoro_pendiente) enterrar(p, t);
    if (p->actividad == ACTV_PESCAR) fin_pesca(p, t, vivo);
    if (p->actividad == ACTV_NADAR) fin_habilidad(p, HAB_NADAR, t, vivo);
    if (p->actividad == ACTV_TREPAR) fin_habilidad(p, HAB_TREPAR, t, vivo);
    if (p->actividad == ACTV_JUGUETE) fin_juguete(p, t, vivo);
    if (p->actividad == ACTV_CAVAR && p->tesoro_pendiente) desenterrar(p, t, vivo);
    if (p->actividad == ACTV_OLFATEAR && p->tesoro_pendiente && !p->vecino.presente) { /* lo olió: ¡a cavar ahí mismo! */
        p->actividad = ACTV_CAVAR;
        p->actividad_hasta = t + 90;
        push_event(p, EV_ACTIVIDAD, ACTV_CAVAR, p->lugar, t);
        if (vivo) sonar(p, SND_CAVAR, t);
        return;
    }
    if (p->vecino.presente) { /* con su amigo no se va a ninguna parte */
        p->actividad = ACTV_JUNTOS;
        p->actividad_hasta = p->vecino.hasta;
        return;
    }
    /* olfateando en la colina o el bosque puede dar con una cueva escondida */
    if ((p->actividad == ACTV_OLFATEAR || p->actividad == ACTV_EXPLORAR) && !pet_scene_open(p, ESC_CUEVA) &&
        (p->escena == ESC_COLINA || p->escena == ESC_BOSQUE) && p->vinculo + (p->modo_nino ? 10.0f : 0.0f) >= CUEVA_VINCULO &&
        chance(p, 0.25f + 0.3f * R[TRAIT_CURIOSA])) {
        p->escenas_abiertas |= 1u << ESC_CUEVA;
        impulsar(p, EMO_ORGULLO, 0.8f, CAUSA_HALLAZGO);
        push_event(p, EV_ESCENA_NUEVA, ESC_CUEVA, 1, t);
        if (vivo) sonar(p, SND_SORPRESA, t);
    }

    /* el lago le muestra la isla: pescando o mirando el agua, cuando ya confía mucho y no es tan chica */
    if ((p->actividad == ACTV_PESCAR || p->actividad == ACTV_EXPLORAR || p->actividad == ACTV_NADAR) && p->escena == ESC_LAGO &&
        !pet_scene_open(p, ESC_ISLA) && p->etapa >= STAGE_CRIA && p->vinculo + (p->modo_nino ? 10.0f : 0.0f) >= ISLA_VINCULO &&
        chance(p, 0.3f + 0.3f * R[TRAIT_CURIOSA])) {
        p->escenas_abiertas |= 1u << ESC_ISLA;
        impulsar(p, EMO_CURIOSIDAD, 0.9f, CAUSA_NOVEDAD);
        push_event(p, EV_ESCENA_NUEVA, ESC_ISLA, 1, t);
        if (vivo) sonar(p, SND_SORPRESA, t);
    }
    paso_largo(p, t, vivo);
    /* en la isla: a la orilla llega a veces una botella con un mensaje (una al día) */
    if (p->escena == ESC_ISLA && !p->dormida && p->dia_botella != (uint8_t)(1 + (dia_local(p, t) & 0xFF)) && chance(p, 0.35f)) {
        p->dia_botella = (uint8_t)(1 + (dia_local(p, t) & 0xFF));
        int f = (int)(rng_next(p) % (uint32_t)pet_phrase_count());
        if (f == p->ultima_frase) f = (f + 1) % pet_phrase_count();
        p->ultima_frase = (uint8_t)f;
        p->frase_respondida = 0;
        impulsar(p, EMO_CURIOSIDAD, 0.7f, CAUSA_HALLAZGO);
        push_event(p, EV_BOTELLA, f, 0, t);
        if (vivo) sonar(p, SND_SORPRESA, t);
    }
    /* de la isla se vuelve en el bote: si pasa mucho rato sola allá, o se pone feo, vuelve al lago */
    if (p->escena == ESC_ISLA) {
        bool feo = c == CLIMA_TORMENTA;
        if (feo || (t - p->escena_desde > 3u * 3600u && t - p->ultima_interaccion > 3600u)) { cambiar_escena(p, ESC_LAGO, t, true); return; }
    }

    /* ¿cambia de zona? con mal tiempo vuelve a resguardarse; si se aburre, se va de excursión */
    {
        /* «malo» es el clima que A ELLA no le gusta: la lluvia si no le gusta, el frío o la nieve si los siente (sin abrigo ni bebida
         * caliente; a las que les gusta el frío no les pasa) y (v101) la tormenta salvo a las que de verdad les encanta (antes, a todas) */
        bool malo = (c == CLIMA_TORMENTA && m->clima[CLIMA_TORMENTA] < 20) || (c == CLIMA_LLUVIA && m->clima[CLIMA_LLUVIA] < 0) || pet_temperature_feel(p, t) < 0;
        bool casita = (p->decoraciones >> DECOR_CASITA) & 1u;
        if (malo && p->escena != ESC_CUEVA && p->escena != ESC_ISLA && !(p->escena == ESC_PRADO && casita)) {
            escena_t a = casita || !pet_scene_open(p, ESC_CUEVA) ? ESC_PRADO : ESC_CUEVA;
            if (a != p->escena) { cambiar_escena(p, a, t, true); return; }
        }
        if (!malo && hl >= 8 && hl < 19 && N[NEED_ENERGIA] > 40 && t - p->escena_desde > 40u * 60u &&
            chance(p, 0.04f + 0.08f * R[TRAIT_CURIOSA] + (N[NEED_CURIOSIDAD] < 60 ? 0.1f : 0.0f))) {
            int mejor_e = -1;
            float mejor_u = -1e9f;
            for (int e = 0; e < ESC_COUNT; e++) {
                if (e == p->escena || e == ESC_ISLA || p->escena == ESC_ISLA || ESC_ES_SECRETA(e) || ESC_ES_SECRETA(p->escena) || !pet_scene_open(p, (escena_t)e)) continue; /* a la isla solo contigo (y a los lugares secretos: contigo o de excursión) */
                float ue = (float)pet_scene_liking(p, (escena_t)e) / 100.0f + rng01(p) * 0.6f;
                if (ue > mejor_u) { mejor_u = ue; mejor_e = e; }
            }
            if (mejor_e >= 0) { cambiar_escena(p, (escena_t)mejor_e, t, true); return; }
        }
    }

    if (plan_antes(p, t, vivo)) return; /* un plan que parte hacia otra zona */

    /* lo que termina deja algo: olfatear y cavar a veces encuentran cosas */
    if (p->actividad == ACTV_OLFATEAR || p->actividad == ACTV_CAVAR || p->actividad == ACTV_EXPLORAR || p->actividad == ACTV_BUSCAR) hallar(p, t, vivo, p->actividad == ACTV_BUSCAR ? 1.3f : 1.0f);
    else if (p->actividad == ACTV_PASEAR || p->actividad == ACTV_JUGAR_SOLA) hallar(p, t, vivo, 0.35f); /* paseando también aparece algo */

    float u[ACTV_COUNT] = {0};
    int lugar[ACTV_COUNT];
    for (int i = 0; i < ACTV_COUNT; i++) lugar[i] = -1;

    u[ACTV_PASEAR] = 0.25f;
    u[ACTV_OLFATEAR] = 0.09f + 0.15f * R[TRAIT_CURIOSA] + (100.0f - N[NEED_CURIOSIDAD]) / 100.0f * (0.4f + 0.6f * R[TRAIT_CURIOSA]); /* aun satisfecha, a veces husmea */
    if (p->dia_tesoros != dia_local(p, t) || p->tesoros_dia < 1) u[ACTV_OLFATEAR] += 0.2f; /* cada día, al menos una vez, busca algo escondido */
    u[ACTV_EXPLORAR] = (N[NEED_CURIOSIDAD] < 50 ? 0.35f : 0.1f) + 0.2f * R[TRAIT_CURIOSA];
    u[ACTV_CAVAR] = (0.06f + 0.22f * R[TRAIT_TRAVIESA]) * (N[NEED_CURIOSIDAD] < 75 ? 1.0f : 0.75f) * (c == CLIMA_NIEVE ? 0.3f : 1.0f);
    lugar[ACTV_CAVAR] = chance(p, 0.5f) ? LUGAR_SENDERO : LUGAR_CLARO;
    if (N[NEED_ENERGIA] < 45 || p->emo[EMO_CANSANCIO] > 0.5f) u[ACTV_SIESTA] = 0.7f;
    lugar[ACTV_SIESTA] = est == ESTACION_VERANO ? LUGAR_ARBOL : lugar_favorito(p);
    /* jugar sola la entretiene hasta cierto punto (TECHO_SOLA): más allá, te espera a ti */
    if (N[NEED_DIVERSION] < (p->modo_nino ? TECHO_SOLA_NINO : TECHO_SOLA) - 5.0f && N[NEED_ENERGIA] > 35) u[ACTV_JUGAR_SOLA] = 0.25f + 0.3f * R[TRAIT_ENERGICA];
    else if (N[NEED_DIVERSION] < 75 && N[NEED_ENERGIA] > 35) u[ACTV_JUGAR_SOLA] = 0.1f;

    if (c == CLIMA_LLUVIA || c == CLIMA_NUBLADO || c == CLIMA_VIENTO) {
        if (m->clima[c] > 20) { u[ACTV_MIRAR] = 0.45f; lugar[ACTV_MIRAR] = c == CLIMA_LLUVIA ? LUGAR_ESTANQUE : LUGAR_CLARO; }
        else if (c == CLIMA_LLUVIA) { u[ACTV_REFUGIO] = 0.6f; }
    }
    if (c == CLIMA_NIEVE && m->clima[CLIMA_NIEVE] > 0 && N[NEED_ENERGIA] > 30) { u[ACTV_NIEVE] = 0.6f + 0.2f * R[TRAIT_ENERGICA]; lugar[ACTV_NIEVE] = LUGAR_CLARO; }
    if (c == CLIMA_DESPEJADO && est == ESTACION_VERANO && hl >= 12 && hl < 18) { u[ACTV_SOMBRA] = 0.65f; lugar[ACTV_SOMBRA] = LUGAR_ARBOL; }
    if (c == CLIMA_FRIO) {
        if (pet_temperature_feel(p, t) < 0) { u[ACTV_ABRIGARSE] = 0.75f - (float)m->clima[CLIMA_FRIO] / 400.0f; lugar[ACTV_ABRIGARSE] = refugio ? LUGAR_CASITA : LUGAR_ARBOL; }
        else if (m->clima[CLIMA_FRIO] >= 20 && N[NEED_ENERGIA] > 35) u[ACTV_JUGAR_SOLA] += 0.25f; /* el frío la despierta */
    }
    if (p->emo[EMO_MIEDO] > 0.45f || N[NEED_SEGURIDAD] < 40) u[ACTV_REFUGIO] = 0.9f;
    lugar[ACTV_REFUGIO] = refugio ? LUGAR_CASITA : LUGAR_ARBOL;
    if (p->vinculo > 30.0f && (expectativa(p, HAB_JUGAR, t) > 0.5f || expectativa(p, HAB_ACARICIAR, t) > 0.5f) &&
        t - p->ultima_interaccion > 20u * 60u) {
        /* imita al dueño: te espera donde sueles acariciarla */
        u[ACTV_ESPERAR] = 0.75f;
        lugar[ACTV_ESPERAR] = lugar_favorito(p);
    }
    /* perseguir animales: mariposas siempre tientan; abejas, hasta que aprende */
    int presa = -1;
#define PERSEGUIBLE(f) (pet_fauna_here(p, f, t) && !((p->fauna_perseguida >> (f)) & 1u)) /* una vez por visita */
    if (PERSEGUIBLE(FAUNA_MARIPOSA)) presa = FAUNA_MARIPOSA;
    if (PERSEGUIBLE(FAUNA_ABEJA) && (presa < 0 || chance(p, 0.4f))) {
        if (p->miedo_abejas < 0.4f) presa = FAUNA_ABEJA;
        else if (vivo && chance(p, 0.3f)) push_event(p, EV_FAUNA, FAUNA_ABEJA, FSUC_EVITA, t); /* aprendió */
    }
    if (presa < 0 && PERSEGUIBLE(FAUNA_PAJARO) && R[TRAIT_TRAVIESA] > 0.6f) presa = FAUNA_PAJARO;
    if (presa < 0 && PERSEGUIBLE(FAUNA_RANA)) presa = FAUNA_RANA;
#undef PERSEGUIBLE
    if (presa >= 0 && N[NEED_ENERGIA] > 30) {
        u[ACTV_PERSEGUIR] = 0.35f + 0.3f * R[TRAIT_ENERGICA] + 0.2f * R[TRAIT_CURIOSA];
        p->persigue = (uint8_t)presa;
    }
    if (pet_fauna_here(p, FAUNA_PAJARO, t) && presa != FAUNA_PAJARO) u[ACTV_MIRAR] = fmaxf(u[ACTV_MIRAR], 0.4f);
    /* habilidades: nadar en el lago y trepar árboles; con práctica, mejor */
    if (pet_can_fish(p, t) && !pet_lake_frozen(p, t) && hl >= 9 && hl < 19 && N[NEED_ENERGIA] > 40 && c != CLIMA_TORMENTA && practicas_hoy(p, HAB_NADAR, t) < 3) {
        u[ACTV_NADAR] = 0.15f + 0.2f * R[TRAIT_ENERGICA] + (float)p->gusto_escena[ESC_LAGO] / 250.0f + (p->habilidad[HAB_NADAR] >= 40 ? 0.15f : 0.0f);
        if (p->memoria.clima[CLIMA_LLUVIA] < -40) u[ACTV_NADAR] *= 0.3f; /* a las que odian mojarse les cuesta */
        lugar[ACTV_NADAR] = LUGAR_ESTANQUE;
    }
    if (puede_trepar(p) && practicas_hoy(p, HAB_TREPAR, t) < 3 && N[NEED_ENERGIA] > 45 && c != CLIMA_LLUVIA && c != CLIMA_TORMENTA && c != CLIMA_NIEVE && hl >= 8 && hl < 19) {
        u[ACTV_TREPAR] = 0.13f + 0.18f * R[TRAIT_ENERGICA] + 0.1f * R[TRAIT_CURIOSA] + (p->habilidad[HAB_TREPAR] >= 40 ? 0.1f : 0.0f);
        lugar[ACTV_TREPAR] = LUGAR_ARBOL;
    }
    /* en el lago, si no está helado, a veces sube al bote a pescar */
    if (pet_can_fish(p, t) && puede_pescar_sola(p, t) && hl >= 7 && hl < 20 && N[NEED_ENERGIA] > 30 && c != CLIMA_TORMENTA && c != CLIMA_VIENTO) {
        u[ACTV_PESCAR] = 0.3f + 0.25f * (1.0f - R[TRAIT_ENERGICA]) + (float)p->gusto_escena[ESC_LAGO] / 400.0f;
        lugar[ACTV_PESCAR] = LUGAR_ESTANQUE;
    }
    /* sus juguetes del prado */
    int jug = -1;
    if (p->juguetes_puestos && p->escena == ESC_PRADO && N[NEED_ENERGIA] > 30 && c != CLIMA_TORMENTA && c != CLIMA_LLUVIA) {
        float uj = 0;
        jug = juguete_preferido(p, &uj);
        if (jug >= 0) u[ACTV_JUGUETE] = uj;
    }
    /* más cosas que hace por su cuenta, según lo que haya a su alrededor */
    const bool afuera = p->escena != ESC_CUEVA, templado = c != CLIMA_TORMENTA && c != CLIMA_NIEVE && c != CLIMA_FRIO;
    if (afuera && templado && N[NEED_ENERGIA] > 35 && est != ESTACION_INVIERNO) { /* chapotear: charcos, el estanque o la orilla del lago */
        const bool agua = pet_puddles(p, t) || p->escena == ESC_LAGO || p->escena == ESC_PRADO;
        if (agua) {
            u[ACTV_CHAPOTEAR] = 0.12f + 0.2f * R[TRAIT_ENERGICA] + (pet_puddles(p, t) ? 0.2f : 0.0f) + (hace_calor(p, t) ? 0.12f : 0.0f);
            if (c == CLIMA_LLUVIA) u[ACTV_CHAPOTEAR] *= m->clima[CLIMA_LLUVIA] < -20 ? 0.2f : 1.4f;
            lugar[ACTV_CHAPOTEAR] = LUGAR_ESTANQUE;
        }
    }
    if (afuera && c != CLIMA_TORMENTA && c != CLIMA_LLUVIA && c != CLIMA_NIEVE && N[NEED_ENERGIA] > 35 && N[NEED_DIVERSION] < 92)
        { u[ACTV_PELOTA] = 0.12f + 0.25f * R[TRAIT_ENERGICA] + (N[NEED_DIVERSION] < 60 ? 0.1f : 0.0f); lugar[ACTV_PELOTA] = LUGAR_CLARO; }
    int quedan = 0; /* ropa del baúl que no lleva puesta, y objetos que tiene en la bolsa */
    for (int r = 0; r < ROPA_COUNT; r++) if (((p->ropa_tiene >> r) & 1u) && pet_wearing(p, pet_clothes_slot((ropa_t)r)) != r) quedan++;
    for (int o = 0; o < PET_OBJETOS; o++) if (p->bolsa[o]) quedan++;
    if (quedan > 0 && !p->enferma && N[NEED_ENERGIA] > 30) { u[ACTV_PROBAR] = 0.1f + 0.22f * R[TRAIT_CURIOSA]; lugar[ACTV_PROBAR] = LUGAR_CLARO; }
    if (afuera && p->escena != ESC_ISLA && N[NEED_ENERGIA] > 30) {
        u[ACTV_BUSCAR] = 0.1f + 0.3f * clampf((70.0f - N[NEED_SACIEDAD]) / 70.0f, 0, 1) + 0.12f * R[TRAIT_CURIOSA];
        lugar[ACTV_BUSCAR] = chance(p, 0.6f) ? LUGAR_ARBOL : LUGAR_SENDERO;
    }
    if (N[NEED_ENERGIA] > 35 && !p->enferma && p->emo[EMO_ALEGRIA] > 0.25f) { /* contenta, de vez en cuando canta o baila por su cuenta */
        u[ACTV_CANTAR] = 0.07f + 0.22f * p->emo[EMO_ALEGRIA] + 0.06f * R[TRAIT_CARINOSA];
        u[ACTV_BAILAR] = 0.06f + 0.2f * p->emo[EMO_ALEGRIA] * (0.5f + R[TRAIT_ENERGICA]);
        lugar[ACTV_CANTAR] = lugar[ACTV_BAILAR] = LUGAR_CLARO;
    }
    if (afuera && templado && hl >= 7 && hl < 19 && N[NEED_ENERGIA] > 25 && c != CLIMA_LLUVIA) { /* mirar aves: más si hay un pajarito cerca */
        u[ACTV_AVES] = 0.08f + 0.12f * R[TRAIT_CURIOSA] + (pet_fauna_here(p, FAUNA_PAJARO, t) ? 0.3f : 0.0f);
        lugar[ACTV_AVES] = LUGAR_ARBOL;
    }
    int amigos = 0, quien = -1; /* charlar con algún animal de la zona (no con los que se esconden) */
    for (int a2 = 0; a2 < ANIMAL_COUNT; a2++) {
        if (a2 == ANIMAL_TOPO || a2 == ANIMAL_ERIZO || !pet_animal_here(p, (animal_t)a2, t)) continue;
        amigos++;
        if (rng_next(p) % (unsigned)amigos == 0) quien = a2;
    }
    if (quien >= 0 && afuera && N[NEED_ENERGIA] > 25 && !p->enferma) { u[ACTV_CHARLAR] = 0.12f + 0.2f * R[TRAIT_CURIOSA] + 0.1f * (p->vinculo > 40.0f ? 1.0f : 0.0f); lugar[ACTV_CHARLAR] = LUGAR_CLARO; }
    const bool urgente = u[ACTV_REFUGIO] >= 0.6f || u[ACTV_ABRIGARSE] >= 0.6f || u[ACTV_SOMBRA] >= 0.6f; /* (lo que va a ir antes que cualquier plan) */
    /* lo aprendido: las actividades opcionales pesan hasta casi el doble, o menos de la mitad, según lo que te alegró (las urgentes no cambian) */
    { /* en su estación favorita hace más lo típico de ella; en la que menos le gusta, menos */
        static const uint8_t TIP[4][4] = {{ACTV_PERSEGUIR, ACTV_AVES, ACTV_EXPLORAR, ACTV_BUSCAR}, {ACTV_NADAR, ACTV_CHAPOTEAR, ACTV_TREPAR, ACTV_PELOTA},
                                          {ACTV_OLFATEAR, ACTV_BUSCAR, ACTV_CAVAR, ACTV_PASEAR}, {ACTV_NIEVE, ACTV_PROBAR, ACTV_CHARLAR, ACTV_CANTAR}};
        const int ee = (int)pet_season(p, t) & 3;
        const float k = pet_season_fav(p, false) == ee ? 1.3f : pet_season_fav(p, true) == ee ? 0.8f : 1.0f;
        if (k != 1.0f) for (int j = 0; j < 4; j++) if (u[TIP[ee][j]] > 0 && u[TIP[ee][j]] < 0.6f) u[TIP[ee][j]] *= k;
    }
    if (PET_APRENDE) for (int i = 0; i < ACTV_COUNT; i++) if (u[i] > 0 && u[i] < 0.6f) u[i] *= 1.0f + (float)p->gusto[i] / 60.0f * (p->gusto[i] > 0 ? 0.9f : 0.6f);
    aplicar_caracter(p, u, hl);
    { /* el trato de estas horas: mimada se queda cerca y está contenta; molesta se aparta y se calla */
        const float tr = (float)pet_treat(p, t);
        static const uint8_t ALEGRES[] = {ACTV_CANTAR, ACTV_BAILAR};
        if (tr > 25.0f) {
            if (p->vinculo > 20.0f && t - p->ultima_interaccion > 300u) { u[ACTV_ESPERAR] = fmaxf(u[ACTV_ESPERAR], 0.2f + tr / 250.0f); if (lugar[ACTV_ESPERAR] < 0) lugar[ACTV_ESPERAR] = lugar_favorito(p); }
            escalar_u(u, ALEGRES, 2, 1.3f);
        } else if (tr < -25.0f) {
            u[ACTV_ESPERAR] *= 0.3f;
            u[ACTV_MIRAR] = fmaxf(u[ACTV_MIRAR], 0.25f);
            if (N[NEED_ENERGIA] < 80) u[ACTV_SIESTA] = fmaxf(u[ACTV_SIESTA], 0.2f);
            escalar_u(u, ALEGRES, 2, 0.4f);
            if (u[ACTV_CAVAR] > 0 && u[ACTV_CAVAR] < 0.6f) u[ACTV_CAVAR] *= 1.3f;
        }
    }
    plan_sesgo(p, u, lugar, urgente);
    for (int i = 0; i < ACTV_COUNT; i++) if (u[i] > 0) u[i] += (rng01(p) - 0.5f) * 0.12f;
    /* lo mismo muchas veces seguidas aburre: lo que hizo recién pesa menos (así no se pasa la vida cavando) */
    if (p->actividad < ACTV_COUNT && u[p->actividad] < 0.6f) u[p->actividad] *= 0.4f;
    if (p->actividad == ACTV_CAVAR || p->actividad == ACTV_OLFATEAR) u[ACTV_CAVAR] *= 0.5f;

    /* elige al azar según el ánimo: lo urgente (miedo, sueño, abrigo) casi siempre gana, lo demás se reparte */
    int mejor = ACTV_PASEAR;
    {
        float tot = 0, w[ACTV_COUNT];
        for (int i = 0; i < ACTV_COUNT; i++) { w[i] = u[i] > 0 ? u[i] * u[i] : 0; tot += w[i]; }
        if (tot > 0) {
            float r = rng01(p) * tot;
            for (int i = 0; i < ACTV_COUNT; i++) { if (r < w[i]) { mejor = i; break; } r -= w[i]; }
        }
    }

    /* se aburre de lo que repite sin que le des cariño ni juegos: poco a poco le gusta menos (hasta -30) */
    if (PET_APRENDE && u[mejor] < 0.6f && p->gusto[mejor] <= 10 && p->gusto[mejor] > -30 && aprende_de(mejor) && chance(p, 0.08f)) p->gusto[mejor]--;
    if (mejor < 32 && aprende_de(mejor) && p->sem_act[mejor] < 255) p->sem_act[mejor]++;
    if (PET_APRENDE && vivo && !p->dormida && mejor == pet_taste_top(p) && p->gusto[mejor] >= 30 && p->dia_foto_gusto != (uint8_t)(1 + ((dia_local(p, t) / 7) & 0x7F))) {
        p->dia_foto_gusto = (uint8_t)(1 + ((dia_local(p, t) / 7) & 0x7F)); /* una foto por semana haciendo lo que más le gusta (el álbum es chico) */
        foto(p, DIA_GUSTO, mejor, t);
    }
    int l = elegir_lugar(p, lugar[mejor]);
    if (mejor == ACTV_EXPLORAR) l = elegir_lugar(p, -1);
    if (mejor == ACTV_REFUGIO) l = lugar[ACTV_REFUGIO]; /* con miedo va igual a su refugio */
    if (mejor == ACTV_ABRIGARSE) l = lugar[ACTV_ABRIGARSE];
    uint32_t dur = 180u + rng_next(p) % 300u;
    if (mejor == ACTV_PERSEGUIR) dur = 120u + rng_next(p) % 120u;
    if (mejor == ACTV_PESCAR) {
        uint8_t hoy = (uint8_t)(1 + (dia_local(p, t) & 0xFF));
        if (p->pesca_dia != hoy) { p->pesca_dia = hoy; p->pescas_solas = 0; }
        p->pescas_solas++;
        empezar_pesca(p, t, duracion_pesca(p), vivo);
        return;
    }
    if (mejor == ACTV_NADAR) { dur = 180u + rng_next(p) % 120u; l = LUGAR_ESTANQUE; }
    if (mejor == ACTV_TREPAR) { dur = 120u + rng_next(p) % 120u; l = LUGAR_ARBOL; }
    if (mejor == ACTV_JUGUETE && jug >= 0) { empezar_juguete(p, (juguete_t)jug, t, 150u + rng_next(p) % 150u, vivo); return; }
    if (mejor == ACTV_CANTAR) { dur = 60u + rng_next(p) % 90u; if (vivo) sonar(p, SND_ALEGRE, t); }
    if (mejor == ACTV_BAILAR) { dur = 60u + rng_next(p) % 120u; p->probando = 0; } /* bailando sola: a su manera */
    if (mejor == ACTV_AVES) dur = 120u + rng_next(p) % 180u;
    if (mejor == ACTV_CHARLAR) { dur = 90u + rng_next(p) % 120u; p->probando = (uint8_t)(quien + 1); }
    if (mejor == ACTV_CHAPOTEAR) { dur = 120u + rng_next(p) % 120u; l = LUGAR_ESTANQUE; }
    if (mejor == ACTV_PELOTA) dur = 150u + rng_next(p) % 150u;
    if (mejor == ACTV_BUSCAR) dur = 120u + rng_next(p) % 120u;
    if (mejor == ACTV_PROBAR) {
        dur = 90u + rng_next(p) % 90u;
        int n = 0, pick[ROPA_COUNT + PET_OBJETOS];
        for (int r = 0; r < ROPA_COUNT; r++) if (((p->ropa_tiene >> r) & 1u) && pet_wearing(p, pet_clothes_slot((ropa_t)r)) != r) pick[n++] = r + 1;
        for (int o = 0; o < PET_OBJETOS; o++) if (p->bolsa[o]) pick[n++] = 32 + o;
        p->probando = n ? (uint8_t)pick[rng_next(p) % (unsigned)n] : 0;
        if (!p->probando) mejor = ACTV_PASEAR;
    }
    if (mejor == ACTV_OLFATEAR && oler_tesoro(p, t, vivo)) dur = 60u;
    if (mejor == ACTV_SIESTA) dur = 1200u + rng_next(p) % 1200u;
    if (mejor == ACTV_REFUGIO && c == CLIMA_TORMENTA) dur = 900u;

    if (p->plan_tipo && p->plan_paso < 3 && mejor == PLANES[p->plan_tipo].paso[p->plan_paso]) p->plan_paso++; /* cumplió el paso */
    bool cambio = mejor != p->actividad || l != p->lugar;
    if (l != p->lugar) marcar_senda(p, p->lugar, l);
    p->actividad = (uint8_t)mejor;
    p->lugar = (uint8_t)l;
    p->actividad_hasta = t + dur;
    if (p->memoria.visitas[l] < 0xFFFF) p->memoria.visitas[l]++;
    if (mejor == ACTV_EXPLORAR || mejor == ACTV_OLFATEAR) impulsar(p, EMO_CURIOSIDAD, 0.5f, CAUSA_EXPLORAR);
    if (mejor == ACTV_NIEVE) impulsar(p, EMO_ALEGRIA, 0.6f, CAUSA_NIEVE);
    if (mejor == ACTV_PERSEGUIR) {
        p->fauna_perseguida |= (uint8_t)(1u << p->persigue);
        if (vivo) push_event(p, EV_FAUNA, p->persigue, FSUC_PERSIGUE, t);
    }
    if (cambio) push_event(p, EV_ACTIVIDAD, mejor, l, t);
    /* contenta, a veces muestra sola un truco que ya sabe */
    if (vivo && p->emo[EMO_ALEGRIA] > 0.45f && (((t / 60u) * 2654435761u) >> 24) < 20u) { /* ~8%, sin gastar el azar */
        int k = (int)((t / 60u) % TRUCO_COUNT);
        if (p->truco[k] >= PET_TRUCO_APRENDIDO) push_event(p, EV_TRUCO, k, 1, t);
    }
    if (vivo && mejor == ACTV_OLFATEAR) sonar(p, SND_OLFATEO, t);
    if (vivo && mejor == ACTV_CAVAR) sonar(p, SND_CAVAR, t);
    if (mejor == ACTV_NADAR) empezar_habilidad(p, HAB_NADAR, t, vivo);
    if (mejor == ACTV_TREPAR) empezar_habilidad(p, HAB_TREPAR, t, vivo);
}

/* ======================================================================
 * 6. Emociones mezcladas con causa
 * ==================================================================== */

static float bajo(float v, float desde) { return clampf((desde - v) / desde, 0, 1); } /* 0 si v>=desde, 1 si v=0 */
static float alto(float v, float desde) { return clampf((v - desde) / (100.0f - desde), 0, 1); }

static void calcular_emociones(pet_t *p, uint32_t t)
{
    const float *N = p->necesidad, *R = p->rasgo;
    const memoria_t *m = &p->memoria;
    float base[EMO_COUNT] = {0};
    uint8_t cb[EMO_COUNT] = {0};
    clima_t c = pet_weather(p);
    if (p->escena == ESC_CUEVA && c != CLIMA_FRIO) c = CLIMA_NUBLADO; /* adentro de la cueva no se nota el tiempo */
    int gclima = m->clima[c];
    static const uint8_t CAUSA_CLIMA[CLIMA_COUNT] = {CAUSA_SOL, CAUSA_NUBLADO, CAUSA_LLUVIA, CAUSA_VIENTO, CAUSA_NIEVE, CAUSA_TORMENTA, CAUSA_FRIO};
    bool sola = t - p->ultima_interaccion > AUSENCIA_S;

    /* alegría: necesidades cubiertas y un clima que le gusta. Si le falta algo básico (comida, baño,
     * cariño o juego), no puede estar feliz del todo */
    float b = bienestar(p);
    float falta = fmaxf(fmaxf(bajo(N[NEED_SACIEDAD], 40), bajo(N[NEED_HIGIENE], 35)), fmaxf(bajo(N[NEED_CARINO], 45), bajo(N[NEED_DIVERSION], 40)));
    base[EMO_ALEGRIA] = clampf((b - 55.0f) / 35.0f, 0, 1) * 0.8f * (1.0f - falta);
    /* la causa de fondo es lo que pasó hace poco; si no pasó nada, solo está contenta */
    cb[EMO_ALEGRIA] = CAUSA_NINGUNA;
    if (p->ultima_caricia && t - p->ultima_caricia < 2u * 3600u) cb[EMO_ALEGRIA] = CAUSA_CARICIAS;
    if (p->ultimo_juego && t - p->ultimo_juego < 3u * 3600u && (!p->ultima_caricia || p->ultimo_juego >= p->ultima_caricia)) cb[EMO_ALEGRIA] = CAUSA_JUGAMOS;
    if (gclima > 40 && !p->dormida) { base[EMO_ALEGRIA] += (float)gclima / 300.0f; cb[EMO_ALEGRIA] = CAUSA_CLIMA[c]; }

    /* tristeza: falta de cariño, de juego, o un clima que no le gusta */
    float t_afecto = bajo(N[NEED_CARINO], 55), t_juego = bajo(N[NEED_DIVERSION], 45) * 0.8f;
    float t_clima = gclima < -30 && c != CLIMA_DESPEJADO ? (float)(-gclima - 30) / 100.0f : 0.0f;
    if ((c == CLIMA_FRIO || c == CLIMA_NIEVE) && bebida_activa(p, FOOD_BEBIDA_CALIENTE, t)) t_clima = 0; /* el chocolate la abriga */
    uint8_t causa_clima = CAUSA_CLIMA[c];
    if (pet_temperature_feel(p, t) > 0) { t_clima = fmaxf(t_clima, molestia_calor(p)); causa_clima = CAUSA_CALOR; }
    base[EMO_TRISTEZA] = t_afecto;
    cb[EMO_TRISTEZA] = sola ? CAUSA_TE_EXTRANA : CAUSA_FALTA_CARINO;
    if (t_juego > base[EMO_TRISTEZA]) { base[EMO_TRISTEZA] = t_juego; cb[EMO_TRISTEZA] = CAUSA_NO_JUGASTE; }
    float t_hambre = bajo(N[NEED_SACIEDAD], 35) * 0.85f, t_sucia = bajo(N[NEED_HIGIENE], 30) * (0.4f + 0.5f * R[TRAIT_PULCRA]);
    if (t_hambre > base[EMO_TRISTEZA]) { base[EMO_TRISTEZA] = t_hambre; cb[EMO_TRISTEZA] = CAUSA_HAMBRE; }
    if (t_sucia > base[EMO_TRISTEZA]) { base[EMO_TRISTEZA] = t_sucia; cb[EMO_TRISTEZA] = CAUSA_SUCIA; }
    if (t_clima > base[EMO_TRISTEZA]) { base[EMO_TRISTEZA] = t_clima; cb[EMO_TRISTEZA] = causa_clima; }
    base[EMO_ALEGRIA] = fmaxf(0.0f, base[EMO_ALEGRIA] - t_clima); /* un clima que odia le apaga la alegría */
    /* la zona donde está: su lugar favorito la alegra; uno que no le gusta la apaga */
    int ge = pet_scene_liking(p, (escena_t)p->escena);
    if (ge >= 50 && !p->dormida && cb[EMO_ALEGRIA] == CAUSA_NINGUNA) { base[EMO_ALEGRIA] += 0.15f; cb[EMO_ALEGRIA] = CAUSA_LUGAR; }
    if (ge <= -40) {
        float tl = (float)(-ge - 30) / 100.0f;
        if (tl > base[EMO_TRISTEZA]) { base[EMO_TRISTEZA] = tl; cb[EMO_TRISTEZA] = CAUSA_LUGAR; }
    }
    if (p->enferma && 0.5f > base[EMO_TRISTEZA]) { base[EMO_TRISTEZA] = 0.5f; cb[EMO_TRISTEZA] = CAUSA_ENFERMA; }
    if (sola) { base[EMO_TRISTEZA] = fmaxf(base[EMO_TRISTEZA], 0.55f); cb[EMO_TRISTEZA] = CAUSA_TE_EXTRANA; }

    /* enojo: hambre con sueño, o mucha suciedad */
    float hambre = bajo(N[NEED_SACIEDAD], 30), cansancio = bajo(N[NEED_ENERGIA], 35);
    base[EMO_ENOJO] = hambre * cansancio * 1.2f;
    cb[EMO_ENOJO] = CAUSA_HAMBRE;
    if (bajo(N[NEED_HIGIENE], 25) * R[TRAIT_PULCRA] > base[EMO_ENOJO]) { base[EMO_ENOJO] = bajo(N[NEED_HIGIENE], 25) * R[TRAIT_PULCRA]; cb[EMO_ENOJO] = CAUSA_SUCIA; }

    /* miedo: poca seguridad (causa: tormenta, un lugar feo, visitas) */
    base[EMO_MIEDO] = bajo(N[NEED_SEGURIDAD], 55) * (0.6f + 0.6f * R[TRAIT_TIMIDA]);
    cb[EMO_MIEDO] = c == CLIMA_TORMENTA ? CAUSA_TORMENTA : (m->miedo[p->lugar] > 0.3f ? CAUSA_LUGAR_FEO : CAUSA_VISITA);

    /* curiosidad y aburrimiento */
    base[EMO_CURIOSIDAD] = R[TRAIT_CURIOSA] * 0.35f * alto(N[NEED_ENERGIA], 40);
    cb[EMO_CURIOSIDAD] = CAUSA_NOVEDAD;
    base[EMO_ABURRIMIENTO] = (bajo(N[NEED_DIVERSION], 50) + bajo(N[NEED_CURIOSIDAD], 40)) * 0.6f;
    cb[EMO_ABURRIMIENTO] = N[NEED_DIVERSION] < N[NEED_CURIOSIDAD] ? CAUSA_NO_JUGASTE : CAUSA_NADA_QUE_HACER;

    /* cansancio y cariño */
    base[EMO_CANSANCIO] = bajo(N[NEED_ENERGIA], 45);
    cb[EMO_CANSANCIO] = CAUSA_SUENO;
    base[EMO_CARINO] = alto(N[NEED_CARINO], 70) * (0.3f + 0.7f * p->vinculo / 100.0f) + p->enamoramiento * 0.6f;
    cb[EMO_CARINO] = p->enamoramiento > 0.5f ? CAUSA_VISITA : (p->ultima_caricia && t - p->ultima_caricia < 6u * 3600u ? CAUSA_CARICIAS : CAUSA_NINGUNA);

    /* el hambre y la suciedad se ven en la cara aunque no sean "emociones" */
    for (int e = 0; e < EMO_COUNT; e++) {
        float v = base[e];
        uint8_t causa = cb[e];
        /* lo que acaba de pasar pesa más que el fondo: su causa es la que se ve */
        if (p->impulso[e] >= 0.35f && p->impulso[e] >= v * 0.7f) { v = fmaxf(v, p->impulso[e]); causa = p->impulso_causa[e]; }
        else v = clampf(v + p->impulso[e] * 0.3f, 0, 1);
        if (p->dormida && e != EMO_CANSANCIO) v *= 0.3f;
        /* modo niño: emociones simples y marcadas (se exageran las fuertes) */
        if (p->modo_nino) v = v > 0.5f ? clampf(v * 1.15f, 0, 1) : v * 0.8f;
        p->emo[e] = clampf(v, 0, 1);
        p->causa[e] = causa;
    }
    if (t < p->celos_hasta) { p->emo[EMO_CELOS] = fmaxf(p->emo[EMO_CELOS], 0.6f); p->causa[EMO_CELOS] = CAUSA_MIMASTE_OTRO; }
    /* con algo básico que le falta mucho, una mariposa la distrae un ratito, pero no la pone feliz */
    if (falta > 0.3f && !p->dormida && p->causa[EMO_ALEGRIA] != CAUSA_VOLVISTE) { /* (verte llegar sí la pone feliz) */
        float tope = 0.75f - 0.5f * falta;
        if (p->emo[EMO_ALEGRIA] > tope) p->emo[EMO_ALEGRIA] = tope;
        if (p->emo[EMO_TRISTEZA] < tope && p->emo[EMO_ENOJO] < tope) {
            p->emo[EMO_TRISTEZA] = fmaxf(p->emo[EMO_TRISTEZA], falta * 0.7f);
            if (p->causa[EMO_TRISTEZA] == CAUSA_NINGUNA) p->causa[EMO_TRISTEZA] = cb[EMO_TRISTEZA];
        }
    }
}

int pet_top_emotions(const pet_t *p, emo_t *primera, emo_t *segunda)
{
    int a = 0, b = -1;
    for (int e = 1; e < EMO_COUNT; e++) if (p->emo[e] > p->emo[a]) a = e;
    for (int e = 0; e < EMO_COUNT; e++) if (e != a && (b < 0 || p->emo[e] > p->emo[b])) b = e;
    if (primera) *primera = (emo_t)a;
    if (segunda) *segunda = (emo_t)b;
    return (p->emo[a] >= UMBRAL_EMOCION) + (p->emo[b] >= UMBRAL_EMOCION);
}

static mood_t calcular_mood(const pet_t *p)
{
    const float *N = p->necesidad;
    if (p->dormida) return MOOD_DORMIDA;
    if (p->enferma) return MOOD_ENFERMA;
    emo_t a, b;
    int n = pet_top_emotions(p, &a, &b);
    if (N[NEED_SACIEDAD] < 20 && p->emo[a] < 0.75f) return MOOD_HAMBRIENTA;
    if (N[NEED_HIGIENE] < 20 && p->emo[a] < 0.6f) return MOOD_SUCIA;
    if (n == 0) return MOOD_TRANQUILA;
    static const uint8_t MAPA[EMO_COUNT] = {
        MOOD_FELIZ, MOOD_TRISTE, MOOD_ENOJADA, MOOD_ASUSTADA, MOOD_CURIOSA,
        MOOD_ABURRIDA, MOOD_SOMNOLIENTA, MOOD_ORGULLOSA, MOOD_CELOSA, MOOD_ENAMORADA,
    };
    return (mood_t)MAPA[a];
}

/* curva de urgencia: casi nada con déficit chico, mucho al acercarse a 0 */
static float urgencia(float valor) { float d = (100.0f - valor) / 100.0f; return d * d * 1.2f; }

static want_t decidir_pedido(pet_t *p, uint32_t t)
{
    if (p->dormida) return WANT_NADA;
    const float *N = p->necesidad, *R = p->rasgo;
    float u[WANT_COUNT] = {0};
    u[WANT_COMER] = urgencia(N[NEED_SACIEDAD]) * (0.8f + 0.4f * R[TRAIT_GLOTONA]);
    u[WANT_DORMIR] = urgencia(N[NEED_ENERGIA]) + (es_noche_su(p, t) ? 0.2f : 0.0f);
    u[WANT_BANO] = urgencia(N[NEED_HIGIENE]) * (0.7f + 0.6f * R[TRAIT_PULCRA]);
    u[WANT_JUGAR] = urgencia(N[NEED_DIVERSION]) * (0.7f + 0.6f * R[TRAIT_ENERGICA]);
    u[WANT_CARINO] = urgencia(N[NEED_CARINO]) * (0.6f + 0.8f * R[TRAIT_CARINOSA]);
    u[WANT_MEDICINA] = p->enferma ? 0.9f : 0.0f;
    int sens = pet_temperature_feel(p, t);
    if (sens < 0) u[WANT_BEBIDA_CALIENTE] = 0.45f + (float)(-p->memoria.clima[CLIMA_FRIO]) / 400.0f;
    if (sens > 0) u[WANT_BEBIDA_FRIA] = 0.4f + molestia_calor(p) * 0.5f;
    if (N[NEED_DIVERSION] < 85) u[WANT_JUGAR] += 0.3f * expectativa(p, HAB_JUGAR, t);
    if (N[NEED_CARINO] < 85) u[WANT_CARINO] += 0.3f * expectativa(p, HAB_ACARICIAR, t);
    if (N[NEED_SACIEDAD] < 80) u[WANT_COMER] += 0.25f * expectativa(p, HAB_ALIMENTAR, t);
    int mejor = WANT_NADA;
    float max = UMBRAL_PEDIR;
    for (int i = 1; i < WANT_COUNT; i++) if (u[i] > max) { max = u[i]; mejor = i; }
    /* con poca confianza pide menos (todavía no sabe que la ayudarás) */
    if (mejor && p->vinculo < 20.0f && max < 0.6f) mejor = WANT_NADA;
    return (want_t)mejor;
}

static void actualizar_mente(pet_t *p, uint32_t t, bool vivo)
{
    calcular_emociones(p, t);
    mood_t m = calcular_mood(p);
    emo_t a, b;
    pet_top_emotions(p, &a, &b);
    if (m != p->emocion) {
        p->emocion = (uint8_t)m;
        push_event(p, EV_EMOCION, m, a, t);
    }
    /* globo de pensamiento: muestra por qué se siente así */
    bool nuevo = a != p->pensamiento_emo || p->causa[a] != p->pensamiento_causa;
    if (vivo && !p->dormida && p->emo[a] >= 0.45f && p->causa[a] != CAUSA_NINGUNA &&
        (nuevo || t - p->ultimo_pensamiento >= PENSAR_CADA_S) && t - p->ultimo_pensamiento >= 90u) {
        p->pensamiento_emo = (uint8_t)a;
        p->pensamiento_causa = p->causa[a];
        p->ultimo_pensamiento = t;
        push_event(p, EV_PENSAMIENTO, a, p->causa[a], t);
    }

    want_t w = decidir_pedido(p, t);
    uint32_t desde = t - p->ultimo_deseo_avisado;
    if (w != WANT_NADA && vivo && ((w != p->ultimo_pedido && desde >= AVISO_CAMBIO_S) || desde >= AVISO_CADA_S)) {
        p->ultimo_deseo_avisado = t;
        p->ultimo_pedido = (uint8_t)w;
        push_event(p, EV_PIDE, w, 0, t);
        sonar(p, SND_LLAMADO, t);
    }
    p->deseo = (uint8_t)w;

    /* crecer: cada etapa es un hito que se celebra (con permiso: te muestra cómo será y decides) */
    float dias = pet_age_days(p, t);
    stage_t e = dias < 3.0f ? STAGE_BEBE : (dias < 4.0f ? STAGE_CRIA : STAGE_ADULTA);
    if (e != p->etapa && p->etapa != STAGE_HUEVO) {
        if (e > p->etapa && p->evo_pregunta) {
            const uint8_t sig = (uint8_t)(p->etapa + 1); /* de a un paso: de bebé a cría y recién después a adulta */
            if (p->evo_pend != sig) {
                p->evo_pend = sig;
                p->evo_decidido = 0;
                push_event(p, EV_EVO_LISTA, sig, 0, t);
                sonar(p, SND_ALEGRE, t);
            }
        } else {
            crecer(p, e, t);
        }
    }
}

/* --- mensajes del día --- */
static const uint8_t HORAS_FRASE[] = {9, 12, 15, 18, 21};
#define N_HORAS_FRASE ((int)sizeof(HORAS_FRASE))

static int32_t slot_frase(const pet_t *p, uint32_t t)
{
    int h = hora_local(p, t), s = -1;
    for (int i = 0; i < N_HORAS_FRASE; i++)
        if (h >= HORAS_FRASE[i]) s = i;
    if (s < 0) return dia_local(p, t) * 8 - 1;
    return dia_local(p, t) * 8 + s;
}

/* la frase tiene que ir con la hora, el clima y lo que pasó hoy (nada de "el rayito de la mañana" a las 9 de la noche) */
static bool frase_cuadra(const pet_t *p, uint32_t t, int i)
{
    const char *s = pet_phrase(i);
    const int h = hora_local(p, t);
    const bool dia = h >= 7 && h < 20;
    const clima_t cl = pet_weather(p);
    if (strstr(s, "rayito de sol") || strstr(s, "cada mañana") || strstr(s, "de anoche")) return h >= 7 && h < 13;
    if (strstr(s, "color tiene hoy el cielo")) return dia && cl != CLIMA_TORMENTA;
    if (strstr(s, "Las nubes de hoy")) return dia && (cl == CLIMA_NUBLADO || cl == CLIMA_DESPEJADO);
    if (strstr(s, "hojas que se mueven con el viento")) return cl == CLIMA_VIENTO;
    if (strstr(s, "Las luciérnagas")) return h >= 18 || h < 6;
    if (strstr(s, "Después de la lluvia")) return cl != CLIMA_TORMENTA;
    if (strstr(s, "Gracias por jugar conmigo")) return (p->vivido & ((1u << VIV_PELOTA) | (1u << VIV_ESCONDIDAS) | (1u << VIV_JUEGO_FRUTAS) | (1u << VIV_JUGUETE))) != 0;
    if (strstr(s, "Gracias por los mimos")) return (p->vivido & (1u << VIV_CARICIAS)) != 0;
    if (strstr(s, "Cuando no estás")) return t - p->ultima_interaccion > 3600u;
    if (strstr(s, "Te extrañé")) return t - p->ultima_interaccion > 2u * 3600u;
    return true;
}

static void revisar_frase(pet_t *p, uint32_t t)
{
    if (p->dormida) return;
    int32_t s = slot_frase(p, t);
    if (s <= p->ultima_frase_slot) return;
    bool primera = p->ultima_frase_slot < 0;
    p->ultima_frase_slot = s;
    if (primera) return;
    int n = pet_phrase_count();
    uint64_t ok = 0;
    for (int k = 0; k < n && k < 64; k++) if (frase_cuadra(p, t, k)) ok |= 1ull << k;
    int f = bolsa_tomar(p, &p->bolsa_frase, n, ok);
    if (f < 0) f = (p->ultima_frase + 1) % n;
    p->ultima_frase = (uint8_t)f;
    p->frase_respondida = 0;
    push_event(p, EV_FRASE, f, p->emocion, t);
    sonar(p, SND_ALEGRE, t);
}

static void revisar_alarmas(pet_t *p, uint32_t desde, uint32_t ahora);
/* un juguete sacado al prado se queda solo unos minutos: después ella (o el tiempo) lo guarda otra vez en el baúl */
static void guardar_juguetes_viejos(pet_t *p, uint32_t t)
{
    for (int j = 0; j < JUG_COUNT; j++) {
        if (!((p->juguetes_puestos >> j) & 1u)) continue;
        if (!p->juguete_hasta[j]) { p->juguete_hasta[j] = t + PET_JUGUETE_AFUERA_S; continue; } /* de una partida anterior: empieza a contar */
        if (t < p->juguete_hasta[j]) continue;
        if (p->actividad == ACTV_JUGUETE && p->juguete_uso == j && t < p->actividad_hasta) continue; /* termina de jugar */
        p->juguetes_puestos &= (uint8_t)~(1u << j);
        p->juguete_hasta[j] = 0;
        push_event(p, EV_JUGUETE, j, 4, t);
    }
}

void pet_update(pet_t *p, uint32_t ahora)
{
    if (ahora <= p->ultima_actualizacion) return;
    revisar_alarmas(p, p->ultima_actualizacion, ahora);
    uint32_t gap = ahora - p->ultima_actualizacion;
    if (gap > MAX_PONERSE_AL_DIA) {
        p->ultima_actualizacion = ahora - MAX_PONERSE_AL_DIA;
        gap = MAX_PONERSE_AL_DIA;
    }
    const uint32_t step = gap > 6u * 3600u ? 300u : 60u;
    uint32_t t = p->ultima_actualizacion;
    bool antes = s_por_su_cuenta;
    s_por_su_cuenta = true; /* lo que pasa aquí lo hace ella sola */
    while (t < ahora) {
        uint32_t dt = (ahora - t) < step ? (ahora - t) : step;
        t += dt;
        bool vivo = (ahora - t) < 120; /* solo los últimos 2 min suenan y piden */
        if (t >= p->clima_hasta) nuevo_clima(p, t);
        if (p->juguetes_puestos) guardar_juguetes_viejos(p, t);
        if (p->etapa == STAGE_HUEVO) {
            /* (1.0.10) un huevo que todavía no llega al prado (sin especie elegida: el primer inicio) no nace solo:
             * su cuenta de 5 minutos empieza al llegar (pet_egg_start). Los huevos de familia ya traen aspecto_ok. */
            if (t >= p->eclosion && p->aspecto_ok) nacer(p, p->eclosion);
            continue;
        }
        paso(p, t, (float)dt, vivo);
        if (!p->dormida && t >= p->actividad_hasta) elegir_actividad(p, t, vivo);
        actualizar_mente(p, t, vivo);
        if (vivo) revisar_frase(p, t);
    }
    s_por_su_cuenta = antes;
    p->ultima_actualizacion = ahora;
}

/* ======================================================================
 * 7. Acciones del dueño
 * ==================================================================== */

static int reaccion(pet_t *p, action_t a, int r, sound_t s, uint32_t t)
{
    push_event(p, EV_REACCION, a, r, t);
    if (s != SND_NINGUNO) sonar(p, s, t);
    if (r > 0) subir_vinculo(p, 1.5f);
    return r;
}

/* ¿obedece? A veces dice que no, pero nunca dos veces seguidas la misma cosa */
static bool no_quiere(pet_t *p, action_t a)
{
    const float *R = p->rasgo, *N = p->necesidad;
    if (p->ultimo_no == a) { p->ultimo_no = 0xFF; return false; }
    if (a == ACT_ALIMENTAR && N[NEED_SACIEDAD] < 40) return false;
    if (a == ACT_CURAR || a == ACT_DORMIR || a == ACT_DESPERTAR || a == ACT_MIMAR_VISITA || a == ACT_SACUDIR || a == ACT_ACARICIAR ||
        a == ACT_LANZAR || a == ACT_INCLINAR || a == ACT_FIN_JUEGO)
        return false; /* estas tienen sus propias respuestas */
    float prob = 0.03f + p->emo[EMO_ENOJO] * 0.35f + R[TRAIT_TRAVIESA] * 0.08f + (p->vinculo < 25.0f ? 0.08f : 0.0f);
    /* (fw 104) terquedad para todo: las tercas dicen que no más seguido, sobre todo si están metidas en lo suyo; cuanto más te
     * quiere, menos (y siempre: si insistes, obedece, porque nunca dice dos veces seguidas que no a lo mismo) */
    prob += pet_stubbornness(p) * (absorta_en_lo_suyo(p) ? 0.12f : 0.05f) * (1.0f - clampf(p->vinculo, 0, 100) / 200.0f);
    if (a == ACT_JUGAR) prob += p->emo[EMO_CANSANCIO] * 0.3f;
    if (a == ACT_BANAR) prob += (1.0f - R[TRAIT_PULCRA]) * 0.15f;
    if (p->modo_nino) prob *= 0.4f;
    if (chance(p, prob)) { p->ultimo_no = (uint8_t)a; return true; }
    return false;
}

static int acariciar(pet_t *p, gesto_t z, uint32_t ahora)
{
    float *N = p->necesidad;
    int g = p->memoria.gesto[z];
    bool saturada = N[NEED_CARINO] > 95;
    registrar_habito(p, HAB_ACARICIAR, ahora);
    p->memoria.agrado[p->lugar] = clampf(p->memoria.agrado[p->lugar] + 0.08f, 0, 1);
    /* al principio desconfía: a veces se aparta */
    float esquiva = z == GESTO_ABRAZO ? 1.5f : (z == GESTO_TOQUE ? 0.7f : 1.0f); /* un abrazo cuesta más al principio */
    if (p->vinculo < 25.0f && chance(p, 0.35f * esquiva * (1.0f - p->vinculo / 25.0f) * (0.5f + p->rasgo[TRAIT_TIMIDA]))) {
        sumar(p, NEED_CARINO, 4);
        subir_vinculo(p, 1.0f); /* igual suma confianza */
        impulsar(p, EMO_MIEDO, 0.3f, CAUSA_NOVEDAD);
        return reaccion(p, ACT_ACARICIAR, 0, SND_SORPRESA, ahora);
    }
    if (g < -20) { /* donde no le gusta */
        sumar(p, NEED_SEGURIDAD, -6);
        impulsar(p, p->rasgo[TRAIT_TIMIDA] > 0.5f ? EMO_MIEDO : EMO_ENOJO, 0.4f + (float)(-g) / 250.0f, CAUSA_TOQUE_FEO);
        if (g < -45) descubrir(p, GUSTO_GESTO, z, ahora);
        p->memoria.gesto[z] = i8((float)(g - 2)); /* insistir lo empeora */
        if (z == GESTO_MUCHOS) sumar(p, NEED_ENERGIA, -2); /* la mareaste */
        return reaccion(p, ACT_ACARICIAR, -1, z == GESTO_MUCHOS ? SND_MAREO : SND_QUEJA, ahora);
    }
    if (z == GESTO_MUCHOS) { /* a esta le dan cosquillas */
        sumar(p, NEED_DIVERSION, 10);
        impulsar(p, EMO_ALEGRIA, 0.5f, CAUSA_JUGAMOS);
        p->memoria.gesto[z] = i8((float)(g + 1));
        subir_vinculo(p, 0.5f);
        return reaccion(p, ACT_ACARICIAR, 1, SND_RISA, ahora);
    }
    float k = z == GESTO_ABRAZO ? 1.4f : (z == GESTO_TOQUE ? 0.7f : 1.0f);
    sumar(p, NEED_CARINO, (12.0f + (float)g / 8.0f) * k);
    p->ultima_caricia = ahora;
    sumar(p, NEED_SEGURIDAD, z == GESTO_ABRAZO ? 14.0f : 8.0f);
    impulsar(p, EMO_CARINO, 0.3f + (float)g / 200.0f, CAUSA_CARICIAS);
    if (g > 55) { impulsar(p, EMO_ALEGRIA, 0.6f, CAUSA_CARICIAS); descubrir(p, GUSTO_GESTO, z, ahora); }
    moldear(p, TRAIT_CARINOSA, 0.002f, ahora);
    if (p->emo[EMO_MIEDO] > 0.4f) moldear(p, TRAIT_TIMIDA, -0.003f, ahora); /* consolarla la hace más valiente */
    p->memoria.gesto[z] = i8((float)(g + 1)); /* lo que se repite con cariño, gusta más */
    subir_vinculo(p, 1.0f);
    sound_t son = z == GESTO_ABRAZO ? SND_ABRAZO : (g > 55 ? SND_RONRONEO : (z == GESTO_TOQUE ? SND_TOQUE : SND_ALEGRE));
    return reaccion(p, ACT_ACARICIAR, saturada ? 0 : 1, son, ahora);
}

int pet_action(pet_t *p, action_t a, int arg, uint32_t ahora)
{
    pet_update(p, ahora);
    s_por_su_cuenta = false; /* esto lo hiciste tú */
    const int haciendo = (!p->dormida && p->etapa != STAGE_HUEVO && p->actividad < ACTV_COUNT && p->actividad_hasta > ahora) ? p->actividad : -1; /* lo que hacía por su cuenta */
    float *N = p->necesidad;
    const float *R = p->rasgo;
    int r = 0;

    /* reencuentro: volviste después de mucho tiempo */
    uint32_t ausente = ahora - p->ultima_interaccion;
    p->ultima_interaccion = ahora;
    adaptarse(p, a, ahora);
    if (ausente >= PET_REGALO_AUSENCIA) regalo_al_volver(p, ausente, ahora);
    if (ausente >= REENCUENTRO_S && p->etapa != STAGE_HUEVO) {
        int horas = (int)(ausente / 3600u);
        impulsar(p, EMO_ALEGRIA, 1.0f, CAUSA_VOLVISTE);
        impulsar(p, EMO_CARINO, 0.7f, CAUSA_VOLVISTE);
        p->impulso[EMO_TRISTEZA] = 0;
        if (p->dormida) despertarse(p, ahora, false);
        push_event(p, EV_REENCUENTRO, horas > 127 ? 127 : horas, 0, ahora);
        sonar(p, SND_ALEGRE, ahora);
        p->actividad_hasta = ahora + 60;
    }

    /* hoy es noche de luna llena: lo avisa la primera vez que la ves en el día */
    if (p->etapa != STAGE_HUEVO && p->dia_luna_aviso != (uint8_t)(1 + (dia_local(p, ahora) & 0xFF)) && pet_nights_to_full_moon(ahora) == 0 &&
        hora_local(p, ahora) >= 7) {
        p->dia_luna_aviso = (uint8_t)(1 + (dia_local(p, ahora) & 0xFF));
        bool pista = p->etapa == STAGE_ADULTA && p->guardiana == 0 && p->bolsa[OBJ_CORAZON_BOSQUE] > 0;
        push_event(p, EV_LUNA_LLENA, pista, 0, ahora);
    }
    /* fruta del árbol: la primera visita de cada día */
    int32_t hoy = dia_local(p, ahora);
    if (hoy != p->ultimo_regalo && p->etapa != STAGE_HUEVO) {
        p->ultimo_regalo = hoy;
        guardar_item(p, ITEM_FRUTA);
        push_event(p, EV_REGALO, ITEM_FRUTA, 0, ahora);
    }

    if (p->etapa == STAGE_HUEVO) { /* al huevo solo se le puede dar calor */
        if (a == ACT_ACARICIAR || a == ACT_SACUDIR) {
            uint32_t min = ahora + 1;
            p->eclosion = p->eclosion > min + PET_HUEVO_CALOR_S ? p->eclosion - PET_HUEVO_CALOR_S : min;
            push_event(p, EV_REACCION, a, 1, ahora);
            sonar(p, SND_RONRONEO, ahora);
            return 1;
        }
        return 0;
    }

    /* vestirla y poner juguetes: son cosas tuyas (dormida ni se entera) */
    if (a == ACT_VESTIR) return vestir(p, arg, ahora);
    if (a == ACT_PONER_JUGUETE) return poner_juguete(p, arg, ahora);
    if (a == ACT_USAR_JUGUETE) return usar_juguete(p, arg, ahora);
    if (a == ACT_LIMPIAR) { /* recoges una de sus cacas: un poco más limpio */
        if (p->escena < 8 && p->caca_zona[p->escena]) p->caca_zona[p->escena]--;
        float h = N[NEED_HIGIENE];
        N[NEED_HIGIENE] = h < 35 ? 36.0f : (h < 55 ? 56.0f : (h < 75 ? 76.0f : h));
        if (!p->dormida) impulsar(p, EMO_ALEGRIA, 0.15f, CAUSA_BANO);
        sonar(p, SND_LIMPIO, ahora);
        return 1;
    }

    /* el jardín y espantar al zorro son cosas tuyas: no dependen de ella */
    if (a == ACT_ANIMAL) { /* tocar a un animal del bosque */
        if (!pet_animal_here(p, (animal_t)arg, ahora)) return 0;
        if (arg == ANIMAL_TOPO) { animal_se_va(p, ANIMAL_TOPO, ahora, ANI_SE_ESCONDE); sonar(p, SND_GRUNIDO, ahora); return 1; } /* gruñón */
        if (arg == ANIMAL_ERIZO) { push_event(p, EV_ANIMAL, arg, ANI_BOLITA, ahora); return 1; }                         /* tímido */
        if (arg == ANIMAL_CONEJO && !p->dormida) { push_event(p, EV_ANIMAL, arg, ANI_CORREN, ahora); sumar(p, NEED_DIVERSION, 8); return 1; }
        sumar(p, NEED_CURIOSIDAD, 4);
        return 1;
    }
    if (a == ACT_ARRANCAR) {
        planta_t *pl = parcela(p, arg);
        if (!pl || pl->tipo == PLANTA_NADA) return 0;
        /* una planta viva no se saca: solo cuando ya se secó (creció, se puso mustia y murió) */
        if (pl->agua > PET_AGUA_SECA) { push_event(p, EV_JARDIN, arg, JAR_VIVA, ahora); return 0; }
        /* si alcanzó a dar flores o frutos, deja una semilla de lo que era */
        if (pl->etapa >= ETAPA_GRANDE && pl->tipo != PLANTA_ARBOL) {
            static const int8_t SEM[] = {-1, CULTIVO_FLOR, -1, CULTIVO_GIRASOL, CULTIVO_FRUTILLA, CULTIVO_HIERBA};
            if (pl->tipo < sizeof SEM && SEM[pl->tipo] >= 0) ganar_semilla(p, SEM[pl->tipo], ahora);
        }
        memset(pl, 0, sizeof *pl);
        push_event(p, EV_JARDIN, arg, JAR_ARRANCO, ahora);
        sonar(p, SND_CAVAR, ahora);
        return 1;
    }
    if (a == ACT_PLANTAR || a == ACT_REGAR) {
        int cultivo = (arg >> 4) & 0xF, color_flor = (arg >> 8) & 0xF; /* color: 0 al azar, 1..4 */
        arg &= 0xF;
        planta_t *pl = parcela(p, arg);
        if (!pl || cultivo >= CULTIVO_COUNT) return 0;
        if (a == ACT_PLANTAR) {
            if (pl->tipo != PLANTA_NADA) return 0;
            if (!gastar_semilla(p, cultivo)) { push_event(p, EV_SIN_ITEM, ITEM_SEMILLA, cultivo, ahora); return 0; }
            memset(pl, 0, sizeof *pl);
            static const uint8_t TIPO[CULTIVO_COUNT] = {PLANTA_FLOR, PLANTA_GIRASOL, PLANTA_FRUTILLA, PLANTA_HIERBA};
            pl->tipo = TIPO[cultivo];
            pl->agua = 100.0f;
            pl->color = (uint8_t)(color_flor ? (uint32_t)(color_flor - 1) % 4u : rng_next(p) % 4u);
            push_event(p, EV_JARDIN, arg, JAR_PLANTO, ahora);
        } else {
            if (pl->tipo == PLANTA_NADA) return 0;
            if (pl->mustia == 2) { push_event(p, EV_JARDIN, arg, JAR_VIEJA, ahora); return 0; } /* de vieja ya no revive */
            bool revive = pl->mustia;
            pl->agua = pl->agua < PET_AGUA_SECA ? 30.0f : 100.0f; /* seca: revive, pero queda con sed */
            pl->mustia = 0;
            if (revive) push_event(p, EV_JARDIN, arg, JAR_REVIVIO, ahora);
            meta_avanza(p, META_REGAR, ahora);
        }
        if (!p->dormida) { impulsar(p, EMO_CURIOSIDAD, 0.4f, CAUSA_JARDIN); sumar(p, NEED_CURIOSIDAD, 5); }
        sonar(p, a == ACT_REGAR ? SND_CHAPOTEO : SND_CAVAR, ahora);
        return 1;
    }
    if (a == ACT_ESPANTAR) {
        if (arg == FAUNA_ZORRO && pet_fauna_here(p, FAUNA_ZORRO, ahora)) {
            p->zorro_roba = 0;
            sumar(p, NEED_SEGURIDAD, 15);
            if (!p->dormida) impulsar(p, EMO_ALEGRIA, 0.6f, CAUSA_ESPANTASTE);
            subir_vinculo(p, 2.0f); /* la defendiste */
            push_event(p, EV_FAUNA, FAUNA_ZORRO, FSUC_ESPANTADO, ahora);
            se_va(p, FAUNA_ZORRO, ahora, false);
            return 1;
        }
        if ((unsigned)arg < FAUNA_COUNT && arg != FAUNA_ZORRO && pet_fauna_here(p, (fauna_t)arg, ahora)) {
            if (arg == FAUNA_PAJARO) p->pajaro_amistad = clampf(p->pajaro_amistad - 0.05f, 0, 1);
            se_va(p, (fauna_t)arg, ahora, true); /* sale volando */
        }
        return 0;
    }

    /* casi todo requiere que esté despierta */
    if (p->dormida && a != ACT_DESPERTAR && a != ACT_SACUDIR && a != ACT_DORMIR) {
        return reaccion(p, a, -1, SND_RONQUIDO, ahora);
    }
    bool en_bote = a == ACT_IR && (arg == ESC_ISLA || p->escena == ESC_ISLA); /* el bote a la isla es una aventura juntos: no dice que no */
    if (!en_bote && no_quiere(p, a)) {
        p->terca_act = (uint8_t)(a + 1); p->terca_t = ahora; /* (fw 104) que la terquedad sepa que ya dijo que no: la próxima obedece */
        impulsar(p, EMO_ENOJO, 0.15f, p->causa[EMO_ENOJO]);
        push_event(p, EV_REACCION, a, -2, ahora);
        sonar(p, SND_GRUNIDO, ahora);
        actualizar_mente(p, ahora, true);
        return -2;
    }

    switch (a) {
    case ACT_ALIMENTAR: {
        if (arg >= PET_COMIDA_EST_BASE && arg < PET_COMIDA_EST_BASE + PET_COMIDAS_EST) { r = comer_est(p, arg - PET_COMIDA_EST_BASE, ahora); break; } /* una de temporada */
        food_t f = (arg >= 0 && arg < FOOD_COUNT) ? (food_t)arg : FOOD_COMIDA;
        if (f == FOOD_BEBIDA_CALIENTE || f == FOOD_BEBIDA_FRIA) {
            /* bebidas: siempre hay; sirven si tiene frío o calor, y al revés molestan */
            int sens = pet_temperature_feel(p, ahora);
            bool caliente = f == FOOD_BEBIDA_CALIENTE;
            if ((caliente && hace_calor(p, ahora) && molestia_calor(p) > 0.05f) || (!caliente && hace_frio(p))) {
                if (caliente) impulsar(p, EMO_ENOJO, 0.35f, CAUSA_CALOR); /* ¡quema! */
                else { impulsar(p, EMO_TRISTEZA, 0.35f, CAUSA_FRIO); sumar(p, NEED_SEGURIDAD, -5); } /* ¡brrr! */
                r = reaccion(p, a, -1, caliente ? SND_QUEJA : SND_ESTORNUDO, ahora);
                break;
            }
            sumar(p, NEED_SACIEDAD, 5);
            registrar_habito(p, HAB_ALIMENTAR, ahora);
            if ((caliente && sens < 0) || (!caliente && sens > 0)) {
                p->bebida_tipo = (uint8_t)f;
                p->bebida_hasta = ahora + 3u * 3600u;
                p->impulso[EMO_TRISTEZA] = 0;
                sumar(p, NEED_SEGURIDAD, 10);
                sumar(p, NEED_CARINO, 6);
                impulsar(p, EMO_ALEGRIA, 0.8f, CAUSA_BEBIDA);
                impulsar(p, EMO_CARINO, 0.4f, CAUSA_BEBIDA);
                subir_vinculo(p, 1.5f);
                r = reaccion(p, a, 1, SND_RONRONEO, ahora);
            } else { /* no la necesitaba: rica, pero sin más */
                sumar(p, NEED_DIVERSION, 3);
                r = reaccion(p, a, p->habitos.gusto[f] > 170 ? 1 : 0, SND_COMER, ahora);
            }
            break;
        }
        if (N[NEED_SACIEDAD] > 95) { r = reaccion(p, a, -1, SND_QUEJA, ahora); break; }
        if (f != FOOD_COMIDA) {
            item_t it = f == FOOD_FRUTA ? ITEM_FRUTA : ITEM_MIEL;
            if (p->inventario[it] == 0) { push_event(p, EV_SIN_ITEM, it, 0, ahora); r = 0; break; }
            p->inventario[it]--;
        }
        uint8_t *g = &p->habitos.gusto[f];
        int gusto = *g;
        if (gusto < 70 && N[NEED_SACIEDAD] > 35 && terca_niega(p, ACT_ALIMENTAR, 0.7f, ahora)) {
            /* (fw 104) terca y sin tanta hambre: cruza la cara y ni la prueba (insiste y al menos la prueba) */
            r = -2;
            break;
        }
        if (gusto < 70) { /* comida que odia: la prueba y la escupe */
            sumar(p, NEED_SACIEDAD, 5);
            impulsar(p, EMO_ENOJO, 0.35f, CAUSA_COMIDA_FEA);
            descubrir(p, GUSTO_COMIDA, f, ahora);
            *g = (uint8_t)(gusto + 3); /* con paciencia, a veces se acostumbra */
            registrar_habito(p, HAB_ALIMENTAR, ahora);
            r = reaccion(p, a, -1, SND_QUEJA, ahora);
            break;
        }
        switch (f) {
        case FOOD_COMIDA: sumar(p, NEED_SACIEDAD, 35); sumar(p, NEED_SALUD, 1); p->digestion += 0.35f; break;
        case FOOD_FRUTA: sumar(p, NEED_SACIEDAD, 22); sumar(p, NEED_SALUD, 3); p->digestion += 0.2f; break;
        case FOOD_MIEL:
            sumar(p, NEED_SACIEDAD, 10); sumar(p, NEED_DIVERSION, 12); sumar(p, NEED_SALUD, -2);
            p->digestion += 0.15f; p->dulces += 1.0f; moldear(p, TRAIT_GLOTONA, 0.004f, ahora); break;
        default: break;
        }
        p->digestion = clampf(p->digestion, 0, 1);
        *g = (uint8_t)(gusto + ((220 - gusto) * 6) / 100); /* familiaridad */
        registrar_habito(p, HAB_ALIMENTAR, ahora);
        if (gusto > 190) { impulsar(p, EMO_ALEGRIA, 0.6f, CAUSA_COMIDA_RICA); descubrir(p, GUSTO_COMIDA, f, ahora); }
        r = reaccion(p, a, gusto > 140 ? 1 : 0, SND_COMER, ahora);
        break;
    }
    case ACT_BANAR: {
        bool le_gusta = R[TRAIT_PULCRA] > 0.4f || p->memoria.clima[CLIMA_LLUVIA] > 40;
        /* (fw 104) terca y odia el baño: la primera vez lo esquiva (si no está tan sucia); insiste y se baña igual */
        if (!le_gusta && N[NEED_HIGIENE] > 40 && !p->hediondo && terca_niega(p, ACT_BANAR, 0.6f, ahora)) {
            sumar(p, NEED_DIVERSION, 3); /* esquivarte le pareció un juego */
            r = -2;
            break;
        }
        N[NEED_HIGIENE] = 100;
        p->barro = 0; /* se van las huellas de barro */
        p->hediondo = p->hediondo_min = 0; /* (fw 103) y el olor del zorrillo */
        if (!le_gusta) { sumar(p, NEED_DIVERSION, -5); impulsar(p, EMO_ENOJO, 0.25f, CAUSA_BANO); }
        else impulsar(p, EMO_ALEGRIA, 0.4f, CAUSA_BANO);
        moldear(p, TRAIT_PULCRA, 0.002f, ahora);
        registrar_habito(p, HAB_BANAR, ahora);
        r = reaccion(p, a, le_gusta ? 1 : -1, SND_BANO, ahora); /* aunque no le guste, queda limpia */
        break;
    }
    case ACT_CEPILLAR:
        sumar(p, NEED_HIGIENE, 15);
        sumar(p, NEED_CARINO, 8);
        impulsar(p, EMO_CARINO, 0.3f, CAUSA_CARICIAS);
        r = reaccion(p, a, R[TRAIT_PULCRA] > 0.3f ? 1 : 0, SND_CEPILLO, ahora);
        break;
    case ACT_JUGAR:
        registrar_habito(p, HAB_JUGAR, ahora);
        if (N[NEED_ENERGIA] < 15) { r = reaccion(p, a, -1, SND_BOSTEZO, ahora); break; }
        if (p->enferma) { r = reaccion(p, a, -1, SND_TOS, ahora); break; }
        /* (fw 104) terca: si está metida en lo suyo, «¡espera, estoy ocupada!» (insiste y viene) */
        if (absorta_en_lo_suyo(p) && N[NEED_DIVERSION] > 40 && terca_niega(p, ACT_JUGAR, 0.5f, ahora)) { r = -2; break; }
        sumar(p, NEED_DIVERSION, 30); sumar(p, NEED_ENERGIA, -6); sumar(p, NEED_SACIEDAD, -4);
        sumar(p, NEED_CARINO, 5); sumar(p, NEED_HIGIENE, -3); sumar(p, NEED_CURIOSIDAD, 5);
        impulsar(p, EMO_ALEGRIA, 0.75f, CAUSA_JUGAMOS);
        p->ultimo_juego = ahora;
        p->memoria.agrado[p->lugar] = clampf(p->memoria.agrado[p->lugar] + 0.05f, 0, 1);
        moldear(p, TRAIT_ENERGICA, 0.003f, ahora);
        p->actividad = ACTV_JUGAR_SOLA;
        p->actividad_hasta = ahora + 300; /* un rato de juego contigo */
        r = reaccion(p, a, 1, SND_RISA, ahora);
        break;
    case ACT_LANZAR: {
        /* le tiras la pelota: si tiene energía la trae; cansada va pero la deja; una traviesa a veces se la queda */
        if (N[NEED_ENERGIA] < 18 || p->enferma) { r = reaccion(p, a, -1, SND_BOSTEZO, ahora); break; }
        bool seguido = ahora - p->ultimo_lanzar < 600;
        p->ultimo_lanzar = ahora;
        p->ultimo_juego = ahora;
        p->actividad = ACTV_JUGAR_SOLA;
        p->actividad_hasta = ahora + 180;
        sumar(p, NEED_ENERGIA, -2);
        sumar(p, NEED_CARINO, 2);
        moldear(p, TRAIT_ENERGICA, 0.0015f, ahora);
        int res = 1;
        if (N[NEED_ENERGIA] < 35) res = 0;
        else if (chance(p, 0.25f * R[TRAIT_TRAVIESA] + (p->vinculo < 30 ? 0.1f : 0.0f))) res = 2;
        sumar(p, NEED_DIVERSION, seguido ? 6 : 10);
        impulsar(p, EMO_ALEGRIA, res == 0 ? 0.3f : 0.6f, CAUSA_JUGAMOS);
        r = reaccion(p, a, res, res == 2 ? SND_RISA : SND_ALEGRE, ahora);
        break;
    }
    case ACT_INCLINAR:
        /* la hiciste resbalar: a casi todas les divierte; a las tímidas las asusta un poco */
        if (ahora - p->ultimo_inclinar < 15) { r = 0; break; } /* no cuenta cada cuadro */
        p->ultimo_inclinar = ahora;
        if (R[TRAIT_TIMIDA] > 0.6f) {
            sumar(p, NEED_SEGURIDAD, -6);
            impulsar(p, EMO_MIEDO, 0.3f, CAUSA_INCLINAR);
            r = reaccion(p, a, -1, SND_SORPRESA, ahora);
        } else {
            sumar(p, NEED_DIVERSION, 5);
            impulsar(p, EMO_ALEGRIA, 0.4f, CAUSA_INCLINAR);
            r = reaccion(p, a, 1, SND_DESLIZA, ahora);
        }
        break;
    case ACT_TRUCO: {
        truco_t k = (unsigned)arg < TRUCO_COUNT ? (truco_t)arg : TRUCO_DORMIDA;
        if (ahora - p->ultimo_truco > 15u * 60u) p->racha_trucos = 0;
        p->ultimo_truco = ahora;
        if (++p->racha_trucos > 10 || N[NEED_ENERGIA] < 20) { /* ya se cansó de practicar */
            impulsar(p, EMO_ABURRIMIENTO, 0.4f, CAUSA_NADA_QUE_HACER);
            r = reaccion(p, a, -1, SND_SUSPIRO, ahora);
            break;
        }
        /* (fw 104) terca: un truco que ya se sabe, a veces no le da la gana hacerlo a la primera (los que aprende, sí intenta) */
        if (p->truco[k] > 60.0f && terca_niega(p, ACT_TRUCO, 0.3f, ahora)) { r = -2; break; }
        float v = p->truco[k];
        float aprende = (2.0f + 4.0f * R[TRAIT_ENERGICA] + p->vinculo / 25.0f) * (p->modo_nino ? 1.5f : 1.0f);
        bool sale = chance(p, clampf(v / 100.0f, 0.06f, 0.97f));
        float nuevo = clampf(v + aprende * (sale ? 0.6f : 1.0f), 0, 100);
        p->truco[k] = nuevo;
        sumar(p, NEED_DIVERSION, 4);
        sumar(p, NEED_CURIOSIDAD, 3);
        subir_vinculo(p, 0.3f);
        if (v < PET_TRUCO_APRENDIDO && nuevo >= PET_TRUCO_APRENDIDO && !((p->trucos_sabidos >> k) & 1u)) { /* ¡lo aprendió! (una vez) */
            p->trucos_sabidos |= (uint8_t)(1u << k);
            push_event(p, EV_TRUCO, k, 2, ahora);
            anotar(p, DIA_TRUCO, k, 0, ahora);
            foto(p, DIA_TRUCO, k, ahora);
        }
        if (sale) {
            sumar(p, NEED_CARINO, 4);
            impulsar(p, EMO_ORGULLO, 0.6f, CAUSA_TRUCO);
            impulsar(p, EMO_ALEGRIA, 0.5f, CAUSA_TRUCO);
            r = reaccion(p, a, 1, SND_TRUCO, ahora);
        } else {
            impulsar(p, EMO_CURIOSIDAD, 0.3f, CAUSA_TRUCO);
            r = reaccion(p, a, 0, SND_SORPRESA, ahora);
        }
        break;
    }
    case ACT_FIN_JUEGO: {
        int juego = arg / 100, pts = arg % 100;
        r = 0;
        if (juego == JUEGO_ESCONDIDAS) {
            sumar(p, NEED_DIVERSION, 12);
            sumar(p, NEED_CURIOSIDAD, 8);
            if (pts) { impulsar(p, EMO_ALEGRIA, 0.7f, CAUSA_JUGAMOS); r = 1; } /* la encontraste */
            else { impulsar(p, EMO_ORGULLO, 0.6f, CAUSA_JUGAMOS); r = 2; }   /* ¡ganó ella! */
            sonar(p, pts ? SND_RISA : SND_ALEGRE, ahora);
        } else if (juego == JUEGO_FRUTAS) {
            sumar(p, NEED_DIVERSION, 6 + (float)pts);
            impulsar(p, EMO_ALEGRIA, clampf(0.3f + (float)pts / 20.0f, 0, 0.9f), CAUSA_JUGAMOS);
            if (pts >= 8) { guardar_item(p, ITEM_FRUTA); r = 1; } /* una fruta de premio */
            sonar(p, SND_ALEGRE, ahora);
        } else if (juego == JUEGO_HONGOS) { /* los hongos de luz: repetir la secuencia */
            sumar(p, NEED_DIVERSION, 6 + (float)pts);
            sumar(p, NEED_CURIOSIDAD, 4 + (float)pts);
            impulsar(p, EMO_ALEGRIA, clampf(0.3f + (float)pts / 12.0f, 0, 0.9f), CAUSA_JUGAMOS);
            /* una secuencia larga a veces deja un hongo luminoso en la Bolsa */
            if (pts >= (p->modo_nino ? 4 : 6) && chance(p, 0.5f) && pet_object_gives(p, 31, OFU_REGALO, ahora)) r = 1;
            sonar(p, SND_ALEGRE, ahora);
        }
        p->ultimo_juego = ahora;
        push_event(p, EV_REACCION, a, r, ahora);
        break;
    }
    case ACT_ACARICIAR:
        r = acariciar(p, (gesto_t)((unsigned)arg < GESTO_COUNT ? arg : GESTO_TOQUE), ahora);
        break;
    case ACT_IR: {
        if (!pet_scene_open(p, (escena_t)arg)) { r = 0; break; }
        if (arg == p->escena) { r = 0; break; }
        /* (fw 104) aquí NO hay terquedad: muchos recorridos de la pantalla (portales, la nube, excursiones) dan por hecho que va */
        cambiar_escena(p, (escena_t)arg, ahora, false);
        subir_vinculo(p, 0.5f);
        r = reaccion(p, a, pet_scene_liking(p, (escena_t)arg) >= -20 ? 1 : 0, pet_scene_liking(p, (escena_t)arg) >= -20 ? SND_ALEGRE : SND_QUEJA, ahora);
        break;
    }
    case ACT_SALPICAR:
        /* tocaste un charco: va a saltar ahí (si confía en ti y no está muy cansada) */
        if (!pet_puddles(p, ahora) || p->escena != ESC_PRADO) { r = 0; break; }
        if (N[NEED_ENERGIA] < 15) { r = reaccion(p, a, -1, SND_BOSTEZO, ahora); break; }
        r = salpicar(p, arg, ahora, true, true);
        if (r > 0) subir_vinculo(p, 0.5f);
        push_event(p, EV_REACCION, a, r, ahora);
        break;
    case ACT_PESCAR:
        if (!pet_can_fish(p, ahora) || p->vecino.presente) { r = 0; break; }
        empezar_pesca(p, ahora, duracion_pesca(p), true);
        subir_vinculo(p, 0.5f);
        r = reaccion(p, a, 1, SND_ALEGRE, ahora);
        break;
    case ACT_INVITAR_VECINO: {
        vecino_t *v = &p->vecino;
        int h = hora_local(p, ahora);
        if (v->presente || !v->nombre[0] || h < 8 || h >= 21 || p->clima == CLIMA_TORMENTA || ahora - v->se_fue < 2u * 3600u) { r = 0; break; }
        v->proxima = ahora + 30; /* viene enseguida */
        r = reaccion(p, a, 1, SND_ALEGRE, ahora);
        break;
    }
    case ACT_LLAMAR:
        /* la llamas a un lugar: con poca confianza duda; si confía, va contenta */
        if (p->vinculo < 20.0f && chance(p, 0.5f)) { r = reaccion(p, a, 0, SND_SORPRESA, ahora); break; }
        if ((unsigned)arg < LUGAR_COUNT) p->lugar = (uint8_t)arg;
        p->actividad = ACTV_PASEAR;
        p->actividad_hasta = ahora + 120; /* se queda un rato donde la llamaste */
        if (arg == LUGAR_ESTANQUE && (p->escena == ESC_LAGO || p->escena == ESC_PRADO) && p->necesidad[NEED_ENERGIA] >= 15.0f && !p->enferma &&
            pet_weather(p) != CLIMA_TORMENTA) { /* la llamaste al agua: se mete y nada (no camina sobre ella) */
            p->actividad = ACTV_NADAR;
            p->actividad_hasta = ahora + 150u + rng_next(p) % 90u;
            empezar_habilidad(p, HAB_NADAR, ahora, false);
        }
        sumar(p, NEED_DIVERSION, 2);
        impulsar(p, EMO_ALEGRIA, 0.3f, CAUSA_JUGAMOS);
        subir_vinculo(p, 0.5f);
        r = reaccion(p, a, 1, SND_ALEGRE, ahora);
        break;
    case ACT_MIMAR_VISITA:
        if (p->visita_actual < 0 && !p->vecino.presente) break;
        if (p->visita_actual >= 0) { if (p->visitantes[p->visita_actual].relacion < 100) p->visitantes[p->visita_actual].relacion++; }
        else p->vecino.amistad = clampf(p->vecino.amistad + 2, 0, 100);
        /* celos: más si es cariñosa y se siente poco querida */
        if (chance(p, 0.4f + 0.5f * R[TRAIT_CARINOSA] - N[NEED_CARINO] / 300.0f)) {
            p->celos_hasta = ahora + 300;
            impulsar(p, EMO_CELOS, 0.7f, CAUSA_MIMASTE_OTRO);
            r = reaccion(p, a, -1, SND_GRUNIDO, ahora);
        } else {
            r = reaccion(p, a, 0, SND_NINGUNO, ahora);
        }
        break;
    case ACT_DORMIR:
        p->luz_apagada = 1;
        if (p->dormida) { r = 0; break; }
        /* el botón de dormir siempre funciona: si no tiene mucho sueño, echa una siesta (despierta al descansar o cuando enciendas la luz) */
        if (p->necesidad[NEED_ENERGIA] > 88) p->necesidad[NEED_ENERGIA] = 88;
        dormirse(p, ahora);
        r = reaccion(p, a, 1, SND_NINGUNO, ahora);
        break;
    case ACT_DESPERTAR:
        p->luz_apagada = 0;
        if (p->dormida) {
            bool malhumor = N[NEED_ENERGIA] < 50;
            /* (fw 104) terca y con sueño: se da vuelta y sigue durmiendo (insiste y se despierta, de malas) */
            if (malhumor && terca_niega(p, ACT_DESPERTAR, 0.6f, ahora)) { r = -2; break; }
            despertarse(p, ahora, true);
            r = malhumor ? -1 : 1;
            push_event(p, EV_REACCION, a, r, ahora);
        }
        break;
    case ACT_CURAR:
        if (!p->enferma) { r = reaccion(p, a, -1, SND_QUEJA, ahora); break; }
        if (p->inventario[ITEM_HIERBA] == 0) { push_event(p, EV_SIN_ITEM, ITEM_HIERBA, 0, ahora); r = 0; break; }
        p->inventario[ITEM_HIERBA]--;
        p->dosis++;
        sumar(p, NEED_SALUD, 15);
        if (p->dosis >= 2 || chance(p, 0.5f)) sanar(p, ahora);
        subir_vinculo(p, 2.0f);
        r = reaccion(p, a, 0, SND_QUEJA, ahora); /* sabe raro, pero sirve */
        break;
    case ACT_SACUDIR:
        if (p->dormida) { despertarse(p, ahora, true); r = -1; push_event(p, EV_REACCION, a, r, ahora); break; }
        if (R[TRAIT_TIMIDA] > 0.6f) {
            sumar(p, NEED_SEGURIDAD, -20);
            impulsar(p, EMO_MIEDO, 0.6f, CAUSA_SACUDIDA);
            susto_aqui(p, 0.35f); /* recuerda dónde se asustó */
            moldear(p, TRAIT_TIMIDA, 0.003f, ahora);
            r = reaccion(p, a, -1, SND_SORPRESA, ahora);
        } else {
            sumar(p, NEED_DIVERSION, 8);
            impulsar(p, EMO_ALEGRIA, 0.4f, CAUSA_JUGAMOS);
            moldear(p, TRAIT_TRAVIESA, 0.003f, ahora);
            r = reaccion(p, a, 1, SND_RISA, ahora);
        }
        /* sacudir el reloj hace caer fruta del árbol (hasta 2 por día) */
        if ((p->escena == ESC_PRADO || p->escena == ESC_BOSQUE) && !p->dormida) {
            int32_t dia = dia_local(p, ahora);
            if (p->dia_sacudidas != dia) { p->dia_sacudidas = dia; p->frutas_sacudidas = 0; }
            if (p->frutas_sacudidas < 2 && chance(p, 0.6f)) {
                p->frutas_sacudidas++;
                guardar_item(p, ITEM_FRUTA);
                push_event(p, EV_REGALO, 0, 1, ahora);
            }
        }
        break;
    default:
        break;
    }
    /* lo vivido (para soñar) y la meta del día */
    switch (a) {
    case ACT_LANZAR: if (r >= 0) { vivir(p, VIV_PELOTA); meta_avanza(p, META_PELOTA, ahora); if (p->sem[0] < 255) p->sem[0]++; } break;
    case ACT_JUGAR: if (r > 0 && (arg == JUEGO_PELOTA || arg == 0)) vivir(p, VIV_PELOTA); break;
    case ACT_FIN_JUEGO: vivir(p, arg / 100 == JUEGO_ESCONDIDAS ? VIV_ESCONDIDAS : VIV_JUEGO_FRUTAS); if (p->sem[0] < 255) p->sem[0]++; break;
    case ACT_ALIMENTAR: if (r >= 0 && arg == FOOD_FRUTA) { vivir(p, VIV_FRUTA); meta_avanza(p, META_FRUTA, ahora); } break;
    case ACT_TRUCO: if (r >= 0) meta_avanza(p, META_TRUCO, ahora); if (r > 0) vivir(p, VIV_TRUCO); break;
    case ACT_ACARICIAR: if (r >= 0 && p->etapa != STAGE_HUEVO) { vivir(p, VIV_CARICIAS); meta_avanza(p, META_CARICIAS, ahora); if (p->sem[1] < 255) p->sem[1]++; } break;
    case ACT_IR: if (p->escena == (uint8_t)arg && r >= 0) { meta_avanza(p, META_PASEO, ahora); if (p->sem[3] < 255) p->sem[3]++; } break;
    case ACT_PESCAR: if (r > 0) meta_avanza(p, META_PESCAR, ahora); break;
    case ACT_BANAR: if (r >= -1) meta_avanza(p, META_BANO, ahora); break;
    default: break;
    }
    /* aprende: si la acaricias o juegas con ella mientras hace algo, eso le gustó más */
    if (PET_APRENDE && haciendo >= 0 && aprende_de(haciendo) && r >= 0 && (a == ACT_ACARICIAR || a == ACT_JUGAR || a == ACT_LANZAR) && p->gusto[haciendo] < 60) {
        const int antes = pet_taste_top(p);
        p->gusto[haciendo] = (int8_t)(p->gusto[haciendo] + (p->gusto[haciendo] < 30 ? 3 : 1));
        const int ahora_top = pet_taste_top(p);
        if (ahora_top >= 0 && ahora_top != antes && (!p->gusto_aviso || ahora - p->gusto_aviso > 2u * 24u * 3600u)) { /* nueva favorita: lo cuenta (y al diario) */
            p->gusto_aviso = ahora;
            push_event(p, EV_GUSTO, ahora_top, 0, ahora);
            anotar(p, DIA_GUSTO, ahora_top, (int)(rng_next(p) % 4u), ahora);
        }
    }
    {
        const int antes = trato_registrar(p, a, r, ausente);
        reaccion_viva(p, a, r, antes, ahora);
    }
    actualizar_mente(p, ahora, true);
    return r;
}

/* ======================================================================
 * 8. Decoraciones, consultas y guardado
 * ==================================================================== */

static const uint8_t COSTO_DECOR[DECOR_COUNT][2] = {
    {2, 0}, /* flores: semillas */
    {3, 1}, /* hongos */
    {1, 2}, /* farol: piedras brillantes */
    {2, 3}, /* banco */
    {0, 0}, /* casita: (v38) con ramitas y musgo (PET_CASITA_RAMITAS, PET_CASITA_MUSGO) */
};

void pet_decor_cost(decor_t d, int *semillas, int *piedras)
{
    if ((unsigned)d >= DECOR_COUNT) { *semillas = *piedras = 0; return; }
    *semillas = COSTO_DECOR[d][0];
    *piedras = COSTO_DECOR[d][1];
}

bool pet_has_decor(const pet_t *p, decor_t d) { return (p->decoraciones >> d) & 1u; }

int pet_place_decor(pet_t *p, decor_t d, uint32_t ahora)
{
    if ((unsigned)d >= DECOR_COUNT || pet_has_decor(p, d)) return -1;
    if (d == DECOR_CASITA) { /* (v38) la casita se hace con ramitas y musgo */
        if (p->ramitas < PET_CASITA_RAMITAS || p->musgo < PET_CASITA_MUSGO) return 0;
        p->ramitas = (uint8_t)(p->ramitas - PET_CASITA_RAMITAS);
        p->musgo = (uint8_t)(p->musgo - PET_CASITA_MUSGO);
        p->decoraciones |= 1u << d;
        sumar(p, NEED_CURIOSIDAD, 25);
        sumar(p, NEED_SEGURIDAD, 20);
        impulsar(p, EMO_ORGULLO, 0.9f, CAUSA_NOVEDAD);
        push_event(p, EV_DECORO, d, 0, ahora);
        sonar(p, SND_ALEGRE, ahora);
        return 1;
    }
    if (p->inventario[ITEM_SEMILLA] < COSTO_DECOR[d][0] || p->inventario[ITEM_PIEDRA] < COSTO_DECOR[d][1]) return 0;
    for (int k = 0; k < COSTO_DECOR[d][0]; k++) gastar_semilla(p, -1);
    p->inventario[ITEM_PIEDRA] = (uint8_t)(p->inventario[ITEM_PIEDRA] - COSTO_DECOR[d][1]);
    p->decoraciones |= 1u << d;
    sumar(p, NEED_CURIOSIDAD, 25);
    impulsar(p, EMO_CURIOSIDAD, 0.7f, CAUSA_NOVEDAD);
    push_event(p, EV_DECORO, d, 0, ahora);
    sonar(p, SND_ALEGRE, ahora);
    return 1;
}

float pet_need(const pet_t *p, need_t n) { return n < NEED_COUNT ? p->necesidad[n] : 0.0f; }
mood_t pet_mood(const pet_t *p) { return (mood_t)p->emocion; }
want_t pet_want(const pet_t *p) { return (want_t)p->deseo; }
float pet_emotion(const pet_t *p, emo_t e) { return (unsigned)e < EMO_COUNT ? p->emo[e] : 0.0f; }
causa_t pet_emotion_cause(const pet_t *p, emo_t e) { return (unsigned)e < EMO_COUNT ? (causa_t)p->causa[e] : CAUSA_NINGUNA; }

int pet_favorite(const pet_t *p, gusto_tipo_t tipo, bool odiado)
{
    const memoria_t *m = &p->memoria;
    int mejor = -1, valor = 0;
    switch (tipo) {
    case GUSTO_COMIDA:
        for (int i = 0; i < FOOD_COUNT; i++) {
            if (!((m->descubierto[tipo] >> i) & 1u)) continue;
            int v = p->habitos.gusto[i] - 128;
            if (odiado ? v < valor : v > valor) { valor = v; mejor = i; }
        }
        return mejor;
    case GUSTO_CLIMA:
        for (int i = 0; i < CLIMA_COUNT; i++) {
            if (!((m->descubierto[tipo] >> i) & 1u)) continue;
            if (odiado ? m->clima[i] < valor : m->clima[i] > valor) { valor = m->clima[i]; mejor = i; }
        }
        return mejor;
    case GUSTO_GESTO:
        for (int i = 0; i < GESTO_COUNT; i++) {
            if (!((m->descubierto[tipo] >> i) & 1u)) continue;
            if (odiado ? m->gesto[i] < valor : m->gesto[i] > valor) { valor = m->gesto[i]; mejor = i; }
        }
        return mejor;
    case GUSTO_LUGAR: {
        float vf = 0.25f;
        for (int i = 0; i < LUGAR_COUNT; i++) {
            float v = odiado ? m->miedo[i] : m->agrado[i];
            if (v > vf) { vf = v; mejor = i; }
        }
        return mejor;
    }
    default: return -1;
    }
}

sound_t pet_mood_sound(const pet_t *p)
{
    static const uint8_t MAPA[MOOD_COUNT] = {
        SND_ALEGRE, SND_RONRONEO, SND_QUEJA, SND_BOSTEZO, SND_QUEJA, SND_SUSPIRO,
        SND_LLANTO, SND_GRUNIDO, SND_TOS, SND_SUSPIRO_AMOR, SND_RONQUIDO,
        SND_OLFATEO, SND_SORPRESA, SND_RISA, SND_GRUNIDO,
    };
    return (sound_t)MAPA[p->emocion < MOOD_COUNT ? p->emocion : 0];
}

uint32_t pet_next_wakeup(const pet_t *p, uint32_t ahora)
{
    const float tasa[NEED_SALUD] = {4.0f, 4.0f, 2.5f, 4.5f, 2.5f};
    float horas = 0.5f;
    for (int i = 0; i < NEED_SALUD; i++) {
        float v = p->necesidad[i];
        if (v > 30 && !(p->dormida && i == NEED_ENERGIA)) {
            float hh = (v - 30.0f) / tasa[i];
            if (hh < horas) horas = hh;
        }
    }
    uint32_t s = (uint32_t)(horas * HORA);
    if (p->visita_actual >= 0 && p->visita_hasta > ahora && p->visita_hasta - ahora < s) s = p->visita_hasta - ahora;
    if (p->actividad_hasta > ahora && p->actividad_hasta - ahora < s) s = p->actividad_hasta - ahora;
    if (s < 60) s = 60;
    return s;
}

static uint32_t crc32(const uint8_t *d, size_t n)
{
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < n; i++) {
        c ^= d[i];
        for (int k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u)));
    }
    return ~c;
}

void pet_seal(pet_t *p)
{
    p->magic = PET_SAVE_MAGIC;
    p->version = PET_SAVE_VERSION;
    p->size = (uint16_t)sizeof(pet_t);
    p->crc = crc32((const uint8_t *)p, offsetof(pet_t, crc));
}

bool pet_validate(const pet_t *p)
{
    return p->magic == PET_SAVE_MAGIC && p->version == PET_SAVE_VERSION && p->size == sizeof(pet_t) &&
           p->crc == crc32((const uint8_t *)p, offsetof(pet_t, crc));
}

bool pet_upgrade(pet_t *p)
{
    if (p->magic != PET_SAVE_MAGIC || p->version < PET_SAVE_VERSION_MIN || p->version >= PET_SAVE_VERSION) return false;
    size_t n = p->size;
    if (n < offsetof(pet_t, crc) - 2048 || n >= sizeof(pet_t) || (n & 3u)) return false;
    uint32_t crc_viejo;
    memcpy(&crc_viejo, (const uint8_t *)p + n - 4, 4);
    if (crc_viejo != crc32((const uint8_t *)p, n - 4)) return false;
    memset((uint8_t *)p + n - 4, 0, sizeof(pet_t) - (n - 4)); /* lo nuevo empieza en cero */
    if (p->version < 23) /* las cartas guardaban el saludo y la P.D. junto al texto */
        for (int i = 0; i < PET_CARTAS; i++) {
            uint8_t cual = p->cartas[i][1];
            p->cartas_var[i] = (uint8_t)(((cual >> 5) & 3) | (cual & 0x80));
            p->cartas[i][1] = (uint8_t)(cual & 31);
            p->cartas[i][0] &= 63;
        }
    if (p->version < 22 && p->vecino.visitas && p->vecino.nombre_i < N_NOMBRES_VECINO)
        p->vecinos_vistos = 1u << p->vecino.nombre_i; /* al que ya conocía le puede escribir */
    if (p->version < 14) {
        /* las semillas de antes eran todas iguales: quedan como semillas de flor; los tipos que ya
         * plantó se dan por conocidos */
        p->semillas[CULTIVO_FLOR] = p->inventario[ITEM_SEMILLA];
        p->semillas_vistas = 1u << CULTIVO_FLOR;
        for (int i = 0; i < PET_PARCELAS; i++) {
            uint8_t ti = p->jardin[i].tipo;
            if (ti == PLANTA_GIRASOL) p->semillas_vistas |= 1u << CULTIVO_GIRASOL;
            if (ti == PLANTA_FRUTILLA) p->semillas_vistas |= 1u << CULTIVO_FRUTILLA;
            if (ti == PLANTA_HIERBA) p->semillas_vistas |= 1u << CULTIVO_HIERBA;
            if (p->jardin[i].agua > 100.0f || p->jardin[i].agua < PET_AGUA_PERDIDA) p->jardin[i].agua = 50.0f;
        }
        contar_semillas(p);
    }
    if (p->version < 37) { /* la historia por días jugados: cuenta como jugados los días que ya vivió (así nada de lo que ya tenía queda esperando) */
        float d = pet_age_days(p, p->ultima_actualizacion);
        p->dias_jugados = (uint16_t)(d < 1.0f ? 1 : (d > 60.0f ? 60 : (int)d + 1));
        p->dia_jugado = dia_local(p, p->ultima_actualizacion);
        p->tilde_chisme = 0xFF;
        p->brizna_k = 0xFF;
        p->tilde_prox = (uint16_t)(p->dias_jugados + 1);
        for (int e = 0; e < ESP_COUNT; e++) if ((p->especiales >> e) & 1u) p->tilo_paginas |= (uint8_t)(1u << e); /* las páginas de los secretos que ya descubrió */
        if (p->guardiana) p->tilo_paginas |= 1u << 7;
    }
    if (p->version < 39) { /* (v39) la casita ahora está dentro del claro de las luciérnagas; Tilde viene más seguido */
        if (p->lugares & 1u) p->lugares |= 1u << PET_ZONA_CLARO;
        if (p->tilde_prox > pet_days_played(p) + 2) p->tilde_prox = (uint16_t)(pet_days_played(p) + 2);
        if (p->final_estado & PET_FINAL_SECRETO) p->tilo_ultima = 3u;
    }
    /* (v40) lo escondido de los lugares secretos y las excursiones empiezan en cero (no hay nada que convertir) */
    if (p->version < 41) p->en_zona = 0; /* (v41) el claro y el mirador ahora son zonas (se abren solas con los lugares secretos); los récords empiezan en cero */
    pet_seal(p);
    return true;
}

/* ======================================================================
 * Alarmas del dueño y hora de dormir
 * ==================================================================== */
static int dia_semana(int64_t dia) { return (int)(((dia + 3) % 7 + 7) % 7); } /* 0 lunes ... 6 domingo (1/1/1970 fue jueves) */
static uint32_t hora_en_dia(const pet_t *p, int64_t dia, int h, int m)
{
    int64_t t = dia * 86400 + h * 3600 + m * 60 - p->zona_horaria;
    return t > 0 ? (uint32_t)t : 0;
}
/* cuándo suena (UTC) en un día local, o 0 si ese día no le toca */
static uint32_t alarma_en_dia(const pet_t *p, int i, int64_t dia)
{
    const alarma_t *a = &p->alarma[i];
    if (!a->usada || !a->activa || a->hora > 23 || a->minuto > 59) return 0;
    if (a->dias && !((a->dias >> dia_semana(dia)) & 1u)) return 0;
    return hora_en_dia(p, dia, a->hora, a->minuto);
}

static const sound_t ALARMA_SND[ALARMA_SONIDOS] = {SND_DESPERTADOR, SND_TESORO, SND_FIESTA, SND_HOLA_AMIGO};
static const char *const ALARMA_SND_NOMBRE[ALARMA_SONIDOS] = {"Despertador", "Campanitas", "Fiesta", "Saludo"};
sound_t pet_alarm_sound(int sonido) { return ALARMA_SND[(unsigned)sonido % ALARMA_SONIDOS]; }
const char *pet_alarm_sound_name(int sonido) { return ALARMA_SND_NOMBRE[(unsigned)sonido % ALARMA_SONIDOS]; }

void pet_alarm_set(pet_t *p, int i, bool activa, int hora, int minuto, uint8_t dias, int sonido, bool siempre)
{
    if (i < 0 || i >= PET_ALARMAS) return;
    alarma_t *a = &p->alarma[i];
    a->usada = 1;
    a->activa = activa ? 1 : 0;
    a->hora = (uint8_t)(hora < 0 ? 0 : (hora > 23 ? 23 : hora));
    a->minuto = (uint8_t)(minuto < 0 ? 0 : (minuto > 59 ? 59 : minuto));
    a->dias = (uint8_t)(dias & ALARMA_TODOS);
    a->sonido = (uint8_t)((unsigned)sonido % ALARMA_SONIDOS);
    a->siempre = siempre ? 1 : 0;
    p->alarma_pospuesta[i] = 0;
}

void pet_alarm_delete(pet_t *p, int i)
{
    if (i < 0 || i >= PET_ALARMAS) return;
    memset(&p->alarma[i], 0, sizeof p->alarma[i]);
    p->alarma_sono[i] = p->alarma_pospuesta[i] = 0;
}

int pet_alarm_count(const pet_t *p)
{
    int n = 0;
    for (int i = 0; i < PET_ALARMAS; i++) n += p->alarma[i].usada ? 1 : 0;
    return n;
}

int pet_alarm_pack(int i, bool activa, int hora, int minuto, uint8_t dias, int sonido, bool siempre)
{
    return (i & 7) | (activa ? 1 << 3 : 0) | (hora & 31) << 4 | (minuto & 63) << 9 | (dias & 127) << 15 | (sonido & 3) << 22 |
           (siempre ? 1 << 24 : 0);
}
void pet_alarm_set_packed(pet_t *p, int arg)
{
    if (arg & PET_ALARMA_BORRAR) { pet_alarm_delete(p, arg & 7); return; }
    pet_alarm_set(p, arg & 7, (arg >> 3) & 1, (arg >> 4) & 31, (arg >> 9) & 63, (uint8_t)((arg >> 15) & 127), (arg >> 22) & 3, (arg >> 24) & 1);
}

uint32_t pet_alarm_next(const pet_t *p, int i, uint32_t ahora)
{
    if (i < 0 || i >= PET_ALARMAS) return 0;
    uint32_t mejor = p->alarma_pospuesta[i] > ahora ? p->alarma_pospuesta[i] : 0;
    int64_t d0 = dia_local(p, ahora);
    for (int k = 0; k < 8; k++) {
        uint32_t t = alarma_en_dia(p, i, d0 + k);
        if (t > ahora && t != p->alarma_sono[i]) { if (!mejor || t < mejor) mejor = t; break; }
    }
    return mejor;
}

/* la hora de dormir: avisa y ella se acuesta; a la de levantarse, se levanta */
static void revisar_dormir(pet_t *p, uint32_t desde, uint32_t ahora)
{
    hora_dormir_t *d = &p->dormir;
    if (d->modo == DORMIR_NO) return;
    int64_t dia = dia_local(p, ahora);
    for (int q = 0; q < 2; q++)
        for (int k = 0; k <= 1; k++) {
            uint32_t t = q ? hora_en_dia(p, dia - k, d->despertar_h, d->despertar_m) : hora_en_dia(p, dia - k, d->acostarse_h, d->acostarse_m);
            if (!t || t <= desde || t > ahora || t == d->aviso[q]) continue;
            d->aviso[q] = t;
            if (ahora - t >= 120) break; /* pasó hace rato (estaba apagado) */
            if (p->etapa != STAGE_HUEVO) {
                if (!q) { p->luz_apagada = 1; if (!p->dormida && p->necesidad[NEED_ENERGIA] < 95) dormirse(p, ahora); }
                else { p->luz_apagada = 0; if (p->dormida && p->necesidad[NEED_ENERGIA] >= 40) despertarse(p, ahora, false); }
            }
            push_event(p, EV_HORA_DORMIR, q, 0, ahora);
            break;
        }
}

/* en pet_update: suena si su hora cayó entre la revisión anterior y ahora (y hace menos de 2 min) */
static void revisar_alarmas(pet_t *p, uint32_t desde, uint32_t ahora)
{
    revisar_dormir(p, desde, ahora);
    if (p->temporizador && p->temporizador <= ahora) {
        if (ahora - p->temporizador < 120) push_event(p, EV_TEMPORIZADOR, 0, 0, ahora);
        p->temporizador = 0;
    }
    for (int i = 0; i < PET_ALARMAS; i++) {
        uint32_t pos = p->alarma_pospuesta[i];
        if (pos && pos > desde && pos <= ahora) {
            p->alarma_pospuesta[i] = 0;
            if (ahora - pos < 120) push_event(p, EV_ALARMA, i, 1, ahora);
            continue;
        }
        if (pos && pos + 3600 < ahora) p->alarma_pospuesta[i] = 0; /* quedó vieja (el reloj estuvo apagado) */
        int64_t d = dia_local(p, ahora);
        for (int k = 0; k <= 1; k++) {
            uint32_t t = alarma_en_dia(p, i, d - k);
            if (!t || t <= desde || t > ahora || t == p->alarma_sono[i]) continue;
            p->alarma_sono[i] = t;
            if (!p->alarma[i].dias) p->alarma[i].activa = 0; /* la de una sola vez ya sonó */
            if (ahora - t < 120) push_event(p, EV_ALARMA, i, 0, ahora);
            break;
        }
    }
}

void pet_alarm_answer(pet_t *p, int i, bool posponer, uint32_t ahora)
{
    if (i < 0 || i >= PET_ALARMAS) return;
    if (posponer) { p->alarma_pospuesta[i] = ahora + ALARMA_POSPONER_S; return; }
    p->alarma_pospuesta[i] = 0;
    if (p->etapa == STAGE_HUEVO) return;
    /* buenos días: prendes la luz; si ya durmió lo suficiente, despierta contigo */
    p->luz_apagada = 0;
    if (p->dormida && p->necesidad[NEED_ENERGIA] >= 55) despertarse(p, ahora, false);
    sumar(p, NEED_CARINO, 6);
    if (!p->dormida) impulsar(p, EMO_ALEGRIA, 0.5f, CAUSA_VOLVISTE);
    p->ultima_interaccion = ahora;
}

void pet_bedtime_set(pet_t *p, int modo, int ah, int am, int dh, int dm)
{
    hora_dormir_t *d = &p->dormir;
    d->modo = (uint8_t)((unsigned)modo % DORMIR_COUNT);
    d->acostarse_h = (uint8_t)((unsigned)ah % 24); d->acostarse_m = (uint8_t)((unsigned)am % 60);
    d->despertar_h = (uint8_t)((unsigned)dh % 24); d->despertar_m = (uint8_t)((unsigned)dm % 60);
}
int pet_bedtime_pack(int modo, int ah, int am, int dh, int dm) { return (modo & 3) | (ah & 31) << 2 | (am & 63) << 7 | (dh & 31) << 13 | (dm & 63) << 18; }
void pet_bedtime_set_packed(pet_t *p, int arg) { pet_bedtime_set(p, arg & 3, (arg >> 2) & 31, (arg >> 7) & 63, (arg >> 13) & 31, (arg >> 18) & 63); }

bool pet_is_bedtime(const pet_t *p, uint32_t ahora)
{
    const hora_dormir_t *d = &p->dormir;
    if (d->modo == DORMIR_NO) return false;
    int64_t l = (int64_t)ahora + p->zona_horaria;
    int min = (int)((l % 86400 + 86400) % 86400 / 60);
    int a = d->acostarse_h * 60 + d->acostarse_m, b = d->despertar_h * 60 + d->despertar_m;
    if (a == b) return false;
    return a < b ? (min >= a && min < b) : (min >= a || min < b); /* normalmente cruza la medianoche */
}

void pet_device_defaults(pet_device_t *d)
{
    d->version = PET_DEVICE_VERSION;
    d->volumen = 38;
    d->efectos = 38;
    d->brillo = 80;
    d->apagado = 4; /* de fábrica, el tiempo más largo (2 min) */
    d->ambiente = 25;
    d->girar = 1;
    d->musica = 0;
    d->arrullo = 0;
    d->arrullo_dur = 1;
    d->fuente = 0;
    d->alarmas = PET_ALARMA_VOLUMEN;
    d->arrullo_vol = 55;
    d->pecera = 1; /* media hora */
}
void pet_device_fix(pet_device_t *d)
{
    if (d->version == 1) { d->version = 2; d->ambiente = 50; } /* la versión 1 no tenía el sonido de fondo */
    if (d->version == 2) { d->version = 3; d->efectos = d->volumen; } /* la versión 2 tenía un solo volumen */
    if (d->version == 3) { d->version = 4; d->musica = 0; }          /* la 3 no escuchaba música */
    if (d->version == 4) { d->version = 5; d->arrullo = 0; }         /* la 4 no tenía arrullo */
    if (d->version == 5) { d->version = 6; d->arrullo_dur = 1; }     /* la 5 lo dejaba en una hora */
    if (d->version == 6) { d->version = 7; d->fuente = 0; }          /* la 6 tenía un solo tamaño de letra */
    if (d->version == 7) { d->version = 8; d->girar = 1; }           /* la 7 dejaba apagado el enderezar la pantalla: ahora viene encendido */
    if (d->version == 8) { d->version = 9; d->arrullo_vol = 55; if (d->arrullo) d->arrullo = 3; } /* la 8 no tenía volumen del arrullo; la lluvia y el mar pasan a grillos */
    if (d->version == 9) { d->version = 10; d->musica = 0; }          /* la 9: la escucha de música vuelve a venir apagada (consume batería; se enciende en Sonido) */
    if (d->version == 10) { d->version = 11; d->pecera = 1; }        /* la 10 apagaba la pecera a los 10 min: ahora se elige (de fábrica, 30 min) */
    if (d->version != PET_DEVICE_VERSION) { pet_device_defaults(d); return; }
    if (d->ambiente > 100) d->ambiente = 100;
    if (d->volumen > 100) d->volumen = 100;
    if (d->efectos > 100) d->efectos = 100;
    if (d->brillo > 100) d->brillo = 100;
    if (d->brillo < PET_BRILLO_MIN) d->brillo = PET_BRILLO_MIN;
    if (d->girar > 1) d->girar = 1;
    if (d->musica > 1) d->musica = 0;
    if (d->arrullo > 3) d->arrullo = 0;
    if (d->arrullo) d->arrullo = 3;
    if (d->arrullo_vol > 100) d->arrullo_vol = 100;
    if (d->arrullo_dur > 2) d->arrullo_dur = 1;
    if (d->fuente > 1) d->fuente = 0;
    if (d->apagado > 4) d->apagado = 4;
    if (d->pecera > 3) d->pecera = 1;
    if (!d->alarmas) d->alarmas = PET_ALARMA_VOLUMEN;
    if (d->alarmas > 100) d->alarmas = 100;
    if (d->alarmas < PET_ALARMA_VOLUMEN_MIN) d->alarmas = PET_ALARMA_VOLUMEN_MIN;
}
uint32_t pet_device_screen_ms(const pet_device_t *d)
{
    static const uint32_t MS[5] = {10000, 20000, 30000, 60000, 120000};
    return MS[d->apagado > 4 ? 4 : d->apagado];
}
uint32_t pet_device_aquarium_ms(const pet_device_t *d)
{
    static const uint32_t MS[4] = {10u * 60000u, 30u * 60000u, 60u * 60000u, UINT32_MAX};
    return MS[d->pecera > 3 ? 1 : d->pecera];
}
int pet_device_brightness(const pet_device_t *d, bool pecera, bool noche)
{
    float b = d->brillo;
    if (pecera) b *= 0.55f;
    if (noche) b *= 0.3f;
    int r = (int)(b + 0.5f);
    return r < 5 ? 5 : r;
}
int pet_device_alarm_volume(const pet_device_t *d)
{
    if (!d->alarmas) return PET_ALARMA_VOLUMEN;
    return d->alarmas < PET_ALARMA_VOLUMEN_MIN ? PET_ALARMA_VOLUMEN_MIN : (d->alarmas > 100 ? 100 : d->alarmas);
}
int pet_sound_group(sound_t s)
{
    const pet_sonido_info_t *in = pet_sound_info(s);
    if (!in) return 1;
    if (s == SND_HUELE) return 0;
    if (!strcmp(in->grupo, "alarma")) return 2;
    if (!strcmp(in->grupo, "animo") || !strcmp(in->grupo, "cuerpo") || !strcmp(in->grupo, "accion")) return 0;
    return 1; /* juegos, cueva, magia, viaje, música, mundo (truenos) y los fuegos */
}
int pet_device_sound_volume(const pet_device_t *d, sound_t s)
{
    const int g = pet_sound_group(s);
    return g == 0 ? d->volumen : (g == 1 ? d->efectos : pet_device_alarm_volume(d));
}

void pet_mark_backup(pet_t *p, uint32_t ahora) { p->ultimo_respaldo = ahora; }

void pet_timer_set(pet_t *p, uint32_t segundos, uint32_t ahora) { p->temporizador = segundos ? ahora + segundos : 0; }
uint32_t pet_timer_left(const pet_t *p, uint32_t ahora) { return p->temporizador > ahora ? p->temporizador - ahora : 0; }
void pet_set_profile_lives(pet_t *p, bool vive) { p->perfil_vive = vive ? 1 : 0; }

/* ======================================================================
 * La Bolsa de objetos mágicos
 * ==================================================================== */
int pet_object_count(const pet_t *p, int id) { return (unsigned)id < PET_OBJETOS ? p->bolsa[id] : 0; }
bool pet_object_known(const pet_t *p, int id) { return (unsigned)id < PET_OBJETOS && (p->obj_visto[id >> 3] >> (id & 7) & 1); }
int pet_objects_known(const pet_t *p)
{
    int n = 0;
    for (int i = 0; i < PET_OBJETOS; i++) n += pet_object_known(p, i);
    return n;
}
bool pet_object_spent(int id)
{
    const objeto_info_t *o = objeto_info(id);
    if (!o || o->efecto == OEF_MIRAR || o->efecto == OEF_CABEZA || o->efecto == OEF_DECORAR) return false;
    if (o->efecto == OEF_ESPECIAL && o->param == OBJX_TINTERO) return false; /* el tintero se vuelve a usar */
    switch (o->familia) { /* lo que se come, se bebe, se esparce o se planta */
    case OFA_POLVO: case OFA_FLOR: case OFA_HONGO: case OFA_SEMILLA: case OFA_BAYA: case OFA_HOJA: case OFA_PLANTA:
    case OFA_FRASCO: case OFA_POCION: case OFA_HUEVO: case OFA_ESCAMA: case OFA_PLUMA: case OFA_HILO: case OFA_RUNA:
        return true;
    default: return false;
    }
}
static int objetos_puestos(const pet_t *p, int id)
{
    int n = 0;
    for (int i = 0; i < PET_OBJ_PUESTOS; i++) n += p->obj_puesto[i] == id + 1;
    return n;
}
/* ---- lo que de verdad hacen los objetos más raros (versión 33) ---- */
static const char *const NOTAS_TINTA[PET_TINTA_NOTAS] = {
    "(con tinta invisible) Al principio tenía miedo del bosque. Ahora me gusta dormir escuchándolo",
    "(con tinta invisible) Guardo una piedrecita bajo la cama. No se la digas a nadie",
    "(con tinta invisible) Cuando te vas cuento los minutos. Son muchos, pero siempre vuelves",
    "(con tinta invisible) Me gustaría saber qué hay más allá del cerro",
    "(con tinta invisible) Los días de lluvia pienso en la historia que no sé cómo termina",
    "(con tinta invisible) A veces finjo que no escucho para que me llames otra vez",
    "(con tinta invisible) El árbol viejo me susurra cosas. Casi las entiendo",
    "(con tinta invisible) Mi lugar favorito es donde estás tú",
    "(con tinta invisible) Una vez me comí una baya sin lavar. Me dio vergüenza contarlo",
    "(con tinta invisible) Creo que el bosque se acuerda de mí",
    "(con tinta invisible) Si me perdiera, sé que me buscarías hasta encontrarme",
    "(con tinta invisible) Gracias por cuidarme aunque no sea perfecta",
};
const char *pet_ink_note(int i) { return (unsigned)i < PET_TINTA_NOTAS ? NOTAS_TINTA[i] : ""; }
int pet_ink_notes(const pet_t *p) { int n = 0; for (int i = 0; i < PET_TINTA_NOTAS; i++) n += (p->tinta >> i) & 1; return n; }
/* el candado rúnico cuida el objeto de más valor de la bolsa (nunca el Corazón: ese ya está a salvo) */
int pet_guarded_object(const pet_t *p)
{
    if (!p->bolsa[OBJ_CANDADO_RUNICO]) return -1;
    int mejor = -1, mv = -1;
    for (int i = 0; i < PET_OBJETOS; i++) {
        if (!p->bolsa[i] || i == OBJ_CORAZON_BOSQUE || i == OBJ_CANDADO_RUNICO) continue;
        const objeto_info_t *o = objeto_info(i);
        int v = o->rareza * 10 + (pet_gold_has(p, i) ? 6 : 0);
        if (v > mv) { mv = v; mejor = i; }
    }
    return mejor;
}
bool pet_chest_visible(const pet_t *p) { return pet_final_seen(p, PET_FINAL_SECRETO) && !(p->cofre & 2); }
int pet_chest_status(const pet_t *p)
{
    if (!pet_chest_visible(p)) return 0;
    bool llave = p->bolsa[OBJ_LLAVE_OXIDADA] > 0, cand = p->bolsa[OBJ_CANDADO_RUNICO] > 0;
    return llave && cand ? 4 : (llave ? 3 : (cand ? 2 : 1));
}
int pet_size_mode(const pet_t *p, uint32_t ahora) { return p->talla && ahora < p->talla_hasta ? p->talla : 0; }
bool pet_invisible(const pet_t *p, uint32_t ahora) { return ahora < p->invisible_hasta; }
bool pet_colors_on(const pet_t *p, uint32_t ahora) { return ahora < p->colores_hasta; }
bool pet_nests_visible(const pet_t *p, uint32_t ahora)
{
    return p->obj_cabeza == OBJ_GAFAS_EXPLORADOR + 1 && ahora < p->cabeza_hasta && !p->dormida && p->etapa != STAGE_HUEVO &&
           (p->escena == ESC_PRADO || p->escena == ESC_BOSQUE);
}
bool pet_nest_empty(const pet_t *p, int i, uint32_t ahora)
{
    return p->dia_nidos == dia_local(p, ahora) && (p->nidos >> (i & 7)) & 1;
}
bool pet_juego_mas(const pet_t *p) { return pet_final_seen(p, PET_FINAL_SECRETO); }
bool pet_gold_has(const pet_t *p, int id) { return (unsigned)id < PET_OBJETOS && (p->obj_oro[id >> 3] >> (id & 7)) & 1u; }
int pet_gold_count(const pet_t *p) { int n = 0; for (int i = 0; i < PET_OBJETOS; i++) n += pet_gold_has(p, i); return n; }
static bool dar_objeto(pet_t *p, int id, objeto_fuente_t f, uint32_t ahora, bool dorado)
{
    if ((unsigned)id >= PET_OBJETOS || p->bolsa[id] + objetos_puestos(p, id) >= PET_OBJ_MAX) return false;
    bool nuevo = !pet_object_known(p, id);
    p->bolsa[id]++;
    p->obj_visto[id >> 3] |= (uint8_t)(1u << (id & 7));
    p->bolsa_nueva = (uint8_t)(id + 1);
    p->ultimo_objeto = ahora;
    if (p->sem[2] < 255) p->sem[2]++;
    bool trae = f == OFU_PASEO || f == OFU_REGALO || (p->vinculo >= 40.0f && ahora - p->ultima_interaccion < 15u * 60u);
    /* juego+: después del final secreto, a veces viene en versión dorada (una vez por objeto) */
    bool oro = false;
    if ((dorado || pet_juego_mas(p)) && id != OBJ_CORAZON_BOSQUE && !pet_gold_has(p, id) && (dorado || chance(p, 0.22f))) {
        p->obj_oro[id >> 3] |= (uint8_t)(1u << (id & 7));
        oro = true;
        p->suerte_hasta = ahora + 3u * 3600u; /* trae suerte un rato */
        sumar(p, NEED_DIVERSION, 10);
        impulsar(p, EMO_ORGULLO, 0.5f, CAUSA_HALLAZGO);
    }
    push_event(p, EV_OBJETO, id & 0x7F, (id >> 7) | (nuevo ? 2 : 0) | (trae ? 4 : 0) | (int)f << 3 | (oro ? 64 : 0), ahora);
    if (nuevo) anotar(p, DIA_OBJETO, id & 0x7F, id >> 7, ahora);
    if (nuevo && objeto_info(id)->rareza >= 3) foto(p, DIA_OBJETO, id & 0x7F, ahora);
    sumar(p, NEED_CURIOSIDAD, nuevo ? 25 : 10);
    impulsar(p, EMO_ORGULLO, 0.6f, CAUSA_HALLAZGO);
    impulsar(p, nuevo ? EMO_CURIOSIDAD : EMO_ALEGRIA, 0.6f, CAUSA_HALLAZGO);
    /* el espejo dimensional: hoy lo que encuentra viene doble */
    if (!dorado && ahora < p->doble_hasta && p->bolsa[id] + objetos_puestos(p, id) < PET_OBJ_MAX) p->bolsa[id]++;
    return true;
}
bool pet_object_gives(pet_t *p, int id, objeto_fuente_t f, uint32_t ahora) { return dar_objeto(p, id, f, ahora, false); }

static void sacar_de_bolsa(pet_t *p, int id);
/* el cofre bajo el árbol viejo: la llave oxidada y el candado rúnico lo abren, y adentro hay versiones doradas */
int pet_chest_open(pet_t *p, uint32_t ahora)
{
    if (pet_chest_status(p) != 4) return 0;
    sacar_de_bolsa(p, OBJ_LLAVE_OXIDADA);
    sacar_de_bolsa(p, OBJ_CANDADO_RUNICO);
    p->cofre |= 3;
    int dados = 0;
    for (int k = 0; k < PET_COFRE_DORADOS; k++) {
        float pesos[PET_OBJETOS], total = 0;
        for (int i = 0; i < PET_OBJETOS; i++) {
            const objeto_info_t *o = objeto_info(i);
            float w = 0;
            if (i != OBJ_CORAZON_BOSQUE && i != OBJ_LLAVE_OXIDADA && i != OBJ_CANDADO_RUNICO && !pet_gold_has(p, i) && p->bolsa[i] + objetos_puestos(p, i) < PET_OBJ_MAX)
                w = (pet_object_known(p, i) ? 3.0f : 1.0f) * (float)(o->rareza ? o->rareza : 1);
            pesos[i] = w;
            total += w;
        }
        if (total <= 0) break;
        float r = rng01(p) * total;
        int id = -1;
        for (int i = 0; i < PET_OBJETOS; i++) { if (r < pesos[i]) { id = i; break; } r -= pesos[i]; }
        if (id < 0) break;
        if (dar_objeto(p, id, OFU_REGALO, ahora, true)) dados++;
    }
    anotar(p, DIA_COFRE, dados, 0, ahora);
    foto(p, DIA_COFRE, dados, ahora);
    sonar(p, SND_NUEVO, ahora);
    impulsar(p, EMO_ALEGRIA, 0.9f, CAUSA_HALLAZGO);
    impulsar(p, EMO_ORGULLO, 0.7f, CAUSA_HALLAZGO);
    return dados > 0 ? dados : 1;
}
/* con las gafas del explorador ve nidos en los árboles: en cada uno, a veces, hay algo (una vez al día cada uno) */
int pet_nest_look(pet_t *p, int i, uint32_t ahora)
{
    if (!pet_nests_visible(p, ahora) || (unsigned)i >= PET_NIDOS) return -2;
    int32_t hoy = dia_local(p, ahora);
    if (p->dia_nidos != hoy) { p->dia_nidos = hoy; p->nidos = 0; }
    if ((p->nidos >> i) & 1) return -2;
    p->nidos |= (uint8_t)(1u << i);
    sumar(p, NEED_CURIOSIDAD, 8);
    impulsar(p, EMO_CURIOSIDAD, 0.6f, CAUSA_EXPLORAR);
    if (chance(p, 0.4f)) {
        int id = pet_object_find(p, OFU_EXPLORAR, ahora);
        if (id >= 0 && dar_objeto(p, id, OFU_PASEO, ahora, false)) return id;
    }
    return -1;
}
/* objetos de estación: aparecen más en la suya (según el calendario de SU mundo) y menos en las otras; bits = estacion_t */
static uint8_t objeto_estacion(int id)
{
    static const struct { uint8_t id, m; } T[] = {
        {32, 1}, {36, 1}, {40, 1}, {49, 1}, {54, 1}, {73, 1}, {79, 1}, {42, 1},      /* primavera: flores, capullos, esporas */
        {38, 2}, {35, 2}, {83, 2}, {76, 2}, {58, 2}, {140, 2}, {51, 2},               /* verano: fuego, loto, sol, lagartijas */
        {39, 4}, {50, 4}, {43, 4}, {31, 4}, {56, 4},                                  /* otoño: arce, calabaza, hongos, nueces */
        {45, 8}, {144, 8}, {86, 8}, {11, 8}, {47, 8}};                                /* invierno: baya invernal, helada, lobo, ágata, niebla */
    for (unsigned i = 0; i < sizeof T / sizeof T[0]; i++) if (T[i].id == id) return T[i].m;
    return 0;
}
/* el Corazón del Mundo aparece cuando ya conoce buena parte de lo demás (casi 3 de cada 4); el resto lo sigue encontrando después, hasta el final secreto */
#define CORAZON_UMBRAL (PET_OBJETOS - 40)
int pet_object_find(pet_t *p, objeto_fuente_t f, uint32_t ahora)
{
    /* el Corazón del Mundo aparece solo cuando ya conoce casi todos los demás */
    if (!pet_object_known(p, OBJ_CORAZON_BOSQUE) && pet_objects_known(p) >= CORAZON_UMBRAL && f == OFU_EXPLORAR)
        return OBJ_CORAZON_BOSQUE;
    const int faltan = PET_OBJETOS - pet_objects_known(p);
    const int hsu = pet_pet_hour(p, ahora);
    const bool noche = hsu >= (faltan <= 6 ? 16 : 19) || hsu < 7 /* anochecer incluido: de madrugada duerme (y al final del libro, el atardecer se alarga) */, suerte = ahora < p->suerte_hasta;
    static const float RAREZA[4] = {0, 1.0f, 0.3f, 0.07f};
    const float conocidos = (float)pet_objects_known(p) / (float)PET_OBJETOS;
    const float novedad = 1.6f * (1.0f + 14.0f * conocidos * conocidos * conocidos); /* cuanto más completo el libro, más la atrae lo que falta (así el último no tarda meses) */
    float pesos[PET_OBJETOS], total = 0;
    for (int i = 0; i < PET_OBJETOS; i++) {
        const objeto_info_t *o = objeto_info(i);
        float w = RAREZA[o->rareza & 3];
        if (conocidos > 0.6f && w > 0 && w < (conocidos > 0.85f ? 1.0f : 0.5f) && !pet_object_known(p, i)) w = conocidos > 0.85f ? 1.0f : 0.5f; /* ya casi completo el libro: lo que falta deja de ser tan raro */
        if (o->noche && !noche) w = 0;
        { const uint8_t me = objeto_estacion(i); if (me) w *= ((me >> pet_season(p, ahora)) & 1u) ? 3.0f : (pet_object_known(p, i) ? 0.4f : 1.0f); }
        if (f == OFU_PESCA) { if (!(o->lugares & OL_LAGO)) w = 0; }
        else if (f == OFU_EXPLORAR && o->lugares && !(o->lugares & (p->escena == ESC_ISLA ? OL_LAGO : 1u << p->escena))) w *= (conocidos > 0.8f && !pet_object_known(p, i)) ? 0.6f : 0.1f; /* al final del libro, lo que falta aparece aunque no esté en su zona */
        if (p->escena == ESC_ISLA && (o->familia == OFA_CONCHA || i == OBJ_BOTELLA)) w *= 3.0f; /* la orilla de la isla */
        if (suerte && o->rareza >= 2) w *= 2.5f;
        if (ahora < p->iman_hasta && o->cat == OCAT_ANTIGUO) w *= 3.5f; /* el imán atrae lo perdido */
        if (ahora < p->lentes_hasta && o->rareza >= 3) w *= 1.8f;      /* los lentes muestran lo escondido */
        if (!pet_object_known(p, i)) { w *= novedad; if (o->noche) w *= 4.0f; } /* le interesa más lo que no conoce (y de noche, lo nocturno: duerme y tiene poco rato) */
        else if (faltan <= 12) w *= 0.25f; /* a punto de completar el libro: lo conocido pesa menos */
        else if (pet_juego_mas(p) && !pet_gold_has(p, i)) w *= 1.4f; /* juego+: le atrae lo que aún no tiene dorado */
        if (p->bolsa[i] + objetos_puestos(p, i) >= PET_OBJ_MAX) w = 0;
        pesos[i] = w;
        total += w;
    }
    if (total <= 0) return -1;
    float r = rng01(p) * total;
    for (int i = 0; i < PET_OBJETOS; i++) { if (r < pesos[i]) return i; r -= pesos[i]; }
    return -1;
}
/* ¿encuentra algo mágico esta vez? (muy de vez en cuando; con suerte, más) */
static bool quizas_objeto(pet_t *p, objeto_fuente_t f, float prob, uint32_t t)
{
    if (p->etapa == STAGE_HUEVO || (p->ultimo_objeto && t - p->ultimo_objeto < (pet_objects_known(p) >= PET_OBJETOS - 12 ? 900u : 1800u))) return false;
    if (pet_objects_known(p) >= PET_OBJETOS - 12) prob *= 3.0f; /* a punto de completar el libro: las últimas se dejan ver */
    if (t < p->suerte_hasta) prob *= 1.8f;
    if (t < p->lentes_hasta) prob *= 1.4f;
    if (t < p->iman_hasta) prob *= 1.3f;
    if (p->guardiana) prob *= 1.25f; /* el bosque la cuida: encuentra un poco más */
    if (!chance(p, prob)) return false;
    int id = pet_object_find(p, f, t);
    return id >= 0 && pet_object_gives(p, id, f, t);
}
bool pet_object_ready(const pet_t *p, int id, uint32_t ahora)
{
    if ((unsigned)id >= PET_OBJETOS || !p->bolsa[id]) return false;
    const objeto_info_t *oi = objeto_info(id);
    (void)oi; /* (en invierno también se pueden usar los de pesca: así nadie se queda sin completar el libro por una estación) */
    return pet_object_spent(id) || p->obj_dia[id] != (uint8_t)((dia_local(p, ahora) & 0xFF) + 1);
}
static void sacar_de_bolsa(pet_t *p, int id)
{
    if (p->bolsa[id]) p->bolsa[id]--;
    if (!p->bolsa[id] && p->obj_cabeza == id + 1) p->obj_cabeza = 0;
}
static void rito_guardiana(pet_t *p, uint32_t t);
int pet_object_use(pet_t *p, int id, uint32_t ahora)
{
    const objeto_info_t *o = objeto_info(id);
    if (!o || !p->bolsa[id]) return -1;
    if (id == OBJ_CORAZON_BOSQUE && pet_guardian_ready(p, ahora)) { /* con la luna llena, en el círculo de piedras: el rito */
        rito_guardiana(p, ahora);
        push_event(p, EV_OBJETO_USO, id & 0x7F, (id >> 7) | 2, ahora);
        return 1;
    }
    if (!pet_object_ready(p, id, ahora)) return -2;
    if (p->etapa == STAGE_HUEVO) return 0;
    /* durmiendo solo funciona lo que no la necesita despierta */
    const int ef = o->efecto;
    if (ef != OEF_ESPECIAL && pet_object_spent(id) && p->bolsa[id] == 1 && pet_guarded_object(p) == id) { /* el candado rúnico no deja gastar lo más valioso por error */
        push_event(p, EV_OBJETO_USO, id & 0x7F, (id >> 7) | (7 << 2), ahora);
        return 0;
    }
    const bool dormida_ok = ef == OEF_ESPECIAL && (o->param == OBJX_ESPEJO || o->param == OBJX_CANDADO || o->param == OBJX_TINTERO ||
                                                    o->param == OBJX_LENTES || o->param == OBJX_IMAN || o->param == OBJX_LLAVE);
    if (p->dormida && !dormida_ok && !(ef == OEF_MIRAR || ef == OEF_DECORAR || ef == OEF_CIELO || ef == OEF_JARDIN || ef == OEF_CLIMA ||
                        ef == OEF_SUERTE || ef == OEF_PESCA || ef == OEF_PROTEGE || ef == OEF_CURAR))
        return 0;
    int r = 1, code = 0; /* code: lo que la interfaz cuenta (bits 2..4 del evento) */
    efectos_v38(p, id, ahora);
    switch (ef) {
    case OEF_MIRAR:
        sumar(p, NEED_CURIOSIDAD, 8); impulsar(p, EMO_CURIOSIDAD, 0.5f, CAUSA_NOVEDAD);
        if (id == OBJ_SEDA_ARANA && !pet_has_clothes(p, ROPA_BUFANDA)) { /* con la seda, alguien le teje su bufanda */
            p->ropa_tiene |= (uint16_t)(1u << ROPA_BUFANDA);
            push_event(p, EV_NUEVO, 0, ROPA_BUFANDA, ahora);
            anotar(p, DIA_NUEVO, 0, ROPA_BUFANDA, ahora);
            sonar(p, SND_NUEVO, ahora);
        }
        break;
    case OEF_COMER:
        if (p->necesidad[NEED_SACIEDAD] > 95) { r = 0; break; }
        sumar(p, NEED_SACIEDAD, 15);
        sumar(p, NEED_DIVERSION, 8);
        impulsar(p, EMO_ALEGRIA, 0.7f, CAUSA_COMIDA_RICA);
        break;
    case OEF_CALMA:
        for (int e = EMO_TRISTEZA; e <= EMO_MIEDO; e++) p->emo[e] *= 0.3f;
        p->emo[EMO_CELOS] *= 0.3f;
        sumar(p, NEED_SEGURIDAD, 30);
        impulsar(p, EMO_ALEGRIA, 0.5f, CAUSA_NOVEDAD);
        break;
    case OEF_ENERGIA: sumar(p, NEED_ENERGIA, 25); sumar(p, NEED_DIVERSION, 10); impulsar(p, EMO_ALEGRIA, 0.5f, CAUSA_NOVEDAD); break;
    case OEF_SUENO:
        if (p->necesidad[NEED_ENERGIA] < 85) dormirse(p, ahora);
        else { sumar(p, NEED_SEGURIDAD, 15); r = 0; }
        break;
    case OEF_CARINO: sumar(p, NEED_CARINO, 20); impulsar(p, EMO_CARINO, 0.8f, CAUSA_CARICIAS); subir_vinculo(p, 0.5f); break;
    case OEF_CURAR: if (p->enferma) sanar(p, ahora); sumar(p, NEED_SALUD, 30); break;
    case OEF_CABEZA:
        p->obj_cabeza = (uint8_t)(id + 1);
        p->cabeza_hasta = ahora + 10800u;
        impulsar(p, EMO_ORGULLO, 0.6f, CAUSA_NOVEDAD);
        break;
    case OEF_DECORAR: {
        int s = 0;
        while (s < PET_OBJ_PUESTOS && p->obj_puesto[s]) s++;
        if (s == PET_OBJ_PUESTOS) { /* el más antiguo vuelve a la Bolsa */
            int viejo = p->obj_puesto[0] - 1;
            memmove(p->obj_puesto, p->obj_puesto + 1, PET_OBJ_PUESTOS - 1);
            if ((unsigned)viejo < PET_OBJETOS) p->bolsa[viejo]++;
            s = PET_OBJ_PUESTOS - 1;
        }
        p->obj_puesto[s] = (uint8_t)(id + 1);
        sacar_de_bolsa(p, id);
        impulsar(p, EMO_ALEGRIA, 0.5f, CAUSA_NOVEDAD);
        break;
    }
    case OEF_JARDIN:
        for (int i = 0; i < PET_PARCELAS_TOTAL; i++) {
            planta_t *pl = parcela(p, i);
            if (pl->tipo == PLANTA_NADA || pl->mustia == 2) continue; /* (las que se marchitaron de viejas no vuelven) */
            if (pl->agua < 100.0f) pl->agua = 100.0f;
            pl->mustia = 0;
            if (o->param == 1 && pl->etapa < ETAPA_GRANDE) { pl->etapa++; pl->crece = 0; }
        }
        push_event(p, EV_JARDIN, -1, JAR_CRECIO, ahora);
        impulsar(p, EMO_ALEGRIA, 0.5f, CAUSA_JARDIN);
        break;
    case OEF_CIELO: p->cielo = (uint8_t)(o->param + 1); p->cielo_hasta = ahora + 120u; impulsar(p, EMO_CURIOSIDAD, 0.6f, CAUSA_NOVEDAD); break;
    case OEF_COMPA: p->compa = (uint8_t)(o->param + 1); p->compa_hasta = ahora + 30u * 60u; impulsar(p, EMO_ALEGRIA, 0.6f, CAUSA_NOVEDAD); break;
    case OEF_SUERTE: p->suerte_hasta = ahora + 21600u; impulsar(p, EMO_CURIOSIDAD, 0.6f, CAUSA_NOVEDAD); break;
    case OEF_PESCA: p->pesca_hasta = ahora + 21600u; break;
    case OEF_PROTEGE: p->protege_hasta = ahora + 43200u; sumar(p, NEED_SEGURIDAD, 20); break;
    case OEF_CLIMA: pet_force_weather(p, (clima_t)(o->param % CLIMA_COUNT), ahora, 7200u); break;
    case OEF_ESPECIAL:
        switch (o->param) {
        case OBJX_ESPEJO:
            p->doble_hasta = hora_en_dia(p, dia_local(p, ahora) + 1, 0, 0); /* hasta que termina el día */
            impulsar(p, EMO_CURIOSIDAD, 0.7f, CAUSA_NOVEDAD);
            break;
        case OBJX_RUNA_TP: {
            int cand[ESC_COUNT], n = 0;
            for (int e = 0; e < ESC_COUNT; e++)
                if (e != (int)p->escena && pet_scene_open(p, (escena_t)e) && (e != ESC_ISLA || p->escena == ESC_ISLA) && !ESC_ES_SECRETA(e)) cand[n++] = e;
            if (!n) { r = 0; break; }
            escena_t dest = (escena_t)cand[rng_next(p) % (unsigned)n];
            cambiar_escena(p, dest, ahora, false);
            impulsar(p, EMO_CURIOSIDAD, 0.7f, CAUSA_EXPLORAR);
            sumar(p, NEED_CURIOSIDAD, 10);
            code = 1 + (int)dest; /* a dónde fue (1..6) */
            break;
        }
        case OBJX_INVISIBLE:
            p->invisible_hasta = ahora + 20u * 60u;
            if (p->protege_hasta < ahora + 20u * 60u) p->protege_hasta = ahora + 20u * 60u; /* nadie la ve: ni zorro ni abejas */
            impulsar(p, EMO_CURIOSIDAD, 0.6f, CAUSA_NOVEDAD);
            sumar(p, NEED_DIVERSION, 10);
            break;
        case OBJX_ENCOGE:
            p->talla = 1; p->talla_hasta = ahora + 30u * 60u;
            sumar(p, NEED_CURIOSIDAD, 25);
            impulsar(p, EMO_CURIOSIDAD, 0.8f, CAUSA_NOVEDAD);
            break;
        case OBJX_CRECE:
            p->talla = 2; p->talla_hasta = ahora + 30u * 60u;
            sumar(p, NEED_ENERGIA, 30); sumar(p, NEED_SEGURIDAD, 20);
            if (p->protege_hasta < ahora + 30u * 60u) p->protege_hasta = ahora + 30u * 60u; /* grande y fuerte: nadie se le acerca */
            impulsar(p, EMO_ORGULLO, 0.7f, CAUSA_NOVEDAD);
            break;
        case OBJX_COLORES:
            p->colores_hasta = ahora + 20u * 60u;
            sumar(p, NEED_DIVERSION, 15);
            impulsar(p, EMO_ALEGRIA, 0.7f, CAUSA_NOVEDAD);
            break;
        case OBJX_CANDADO: {
            int g = pet_guarded_object(p);
            code = g >= 0 ? 5 : 6; /* 5: cuida a g; 6: no hay nada que cuidar todavía */
            if (g >= 0) { sumar(p, NEED_SEGURIDAD, 15); impulsar(p, EMO_ORGULLO, 0.4f, CAUSA_NOVEDAD); }
            break;
        }
        case OBJX_TINTERO: {
            int n = -1;
            for (int i = 0; i < PET_TINTA_NOTAS; i++) if (!((p->tinta >> i) & 1)) { n = i; break; }
            if (n < 0) { code = 1; break; } /* ya reveló todas */
            n = (int)((unsigned)n + rng_next(p) % 3u); /* no siempre en el mismo orden */
            while (n >= PET_TINTA_NOTAS || ((p->tinta >> n) & 1)) n = (n + 1) % PET_TINTA_NOTAS;
            p->tinta |= (uint16_t)(1u << n);
            anotar(p, DIA_NOTA_TINTA, n, 0, ahora);
            sumar(p, NEED_CURIOSIDAD, 12);
            impulsar(p, EMO_CURIOSIDAD, 0.6f, CAUSA_NOVEDAD);
            code = 4; /* 4: reveló una nota nueva */
            break;
        }
        case OBJX_LENTES: p->lentes_hasta = ahora + 6u * 3600u; impulsar(p, EMO_CURIOSIDAD, 0.7f, CAUSA_NOVEDAD); break;
        case OBJX_IMAN: p->iman_hasta = ahora + 6u * 3600u; impulsar(p, EMO_CURIOSIDAD, 0.5f, CAUSA_NOVEDAD); break;
        case OBJX_BRUJULA: sumar(p, NEED_CURIOSIDAD, 8); impulsar(p, EMO_CURIOSIDAD, 0.5f, CAUSA_NOVEDAD); break;
        case OBJX_CUERDA: sumar(p, NEED_DIVERSION, 12); sumar(p, NEED_ENERGIA, 10); impulsar(p, EMO_ALEGRIA, 0.6f, CAUSA_NOVEDAD); break;
        case OBJX_PIPA: sumar(p, NEED_ENERGIA, 15); sumar(p, NEED_DIVERSION, 15); impulsar(p, EMO_ALEGRIA, 0.6f, CAUSA_NOVEDAD); break;
        case OBJX_LLAVE: {
            int st = pet_chest_status(p);
            if (st == 4) { int n = pet_chest_open(p, ahora); code = n > 0 ? 4 : 2; }
            else code = st == 0 ? 2 : 3; /* 2: todavía no encuentra qué abre; 3: le falta el candado (o la llave) */
            break;
        }
        default: break;
        }
        break;
    default: break;
    }
    if (r == 1) {
        p->obj_sabe[id >> 3] |= (uint8_t)(1u << (id & 7)); /* ahora sabes qué hace */
        linaje_suma(p, linaje_de_objeto(id), 6);
        if (ef == OEF_CLIMA && o->param == CLIMA_DESPEJADO && p->niebla_hasta) pet_fog_clear(p, ahora); /* el viento se lleva la niebla */
        if (pet_object_spent(id)) sacar_de_bolsa(p, id);
        else if (ef != OEF_DECORAR && !(ef == OEF_ESPECIAL && o->param == OBJX_LLAVE && code != 4)) p->obj_dia[id] = (uint8_t)((dia_local(p, ahora) & 0xFF) + 1);
        p->ultima_interaccion = ahora;
    }
    push_event(p, EV_OBJETO_USO, id & 0x7F, (id >> 7) | (r == 1 ? 2 : 0) | (code << 2), ahora);
    return r;
}
bool pet_object_return(pet_t *p, int id, uint32_t ahora)
{
    if ((unsigned)id >= PET_OBJETOS || !p->bolsa[id]) return false;
    if (p->bolsa[id] == 1 && pet_guarded_object(p) == id) return false; /* el candado lo cuida */
    sacar_de_bolsa(p, id);
    sumar(p, NEED_CARINO, 3);
    impulsar(p, EMO_ALEGRIA, 0.4f, CAUSA_NOVEDAD);
    int32_t hoy = dia_local(p, ahora);
    bool bendice = p->dia_ofrenda == 0; /* una bendición a la vez: la primera ofrenda que espera */
    if (bendice) p->dia_ofrenda = hoy;
    push_event(p, EV_OFRENDA, id & 0x7F, (id >> 7) | (bendice ? 2 : 0), ahora);
    p->ultima_interaccion = ahora;
    return true;
}
bool pet_object_unplace(pet_t *p, int slot)
{
    if ((unsigned)slot >= PET_OBJ_PUESTOS || !p->obj_puesto[slot]) return false;
    int id = p->obj_puesto[slot] - 1;
    memmove(p->obj_puesto + slot, p->obj_puesto + slot + 1, (size_t)(PET_OBJ_PUESTOS - 1 - slot));
    p->obj_puesto[PET_OBJ_PUESTOS - 1] = 0;
    if ((unsigned)id < PET_OBJETOS && p->bolsa[id] < 255) p->bolsa[id]++;
    return true;
}
void pet_object_head_off(pet_t *p) { p->obj_cabeza = 0; p->cabeza_hasta = 0; }
void pet_bag_seen(pet_t *p) { p->bolsa_nueva = 0; }
static void linaje_hora(pet_t *p, uint32_t t);
static int linaje_de_objeto(int id);
static void linaje_suma(pet_t *p, int l, int puntos);
/* las cacas caen donde ella estaba al ensuciarse y ahí se quedan: hay que ir a recogerlas a cada zona */
static void paso_cacas(pet_t *p)
{
    if (p->etapa == STAGE_HUEVO || p->guardiana) { memset(p->caca_zona, 0, sizeof p->caca_zona); return; }
    const float h = p->necesidad[NEED_HIGIENE];
    const int need = h < 35 ? 3 : (h < 55 ? 2 : (h < 75 ? 1 : 0));
    int sum = 0;
    for (int z = 0; z < 8; z++) { if (p->caca_zona[z] > 3) p->caca_zona[z] = 3; sum += p->caca_zona[z]; }
    const int aqui = p->escena < 8 ? p->escena : 0;
    if (need == 0) { memset(p->caca_zona, 0, sizeof p->caca_zona); return; }
    for (int z = 0; z < 8 && sum > need; z++) { /* ya no hace falta tanto: sobran las de otras zonas primero */
        if (z == aqui) continue;
        while (p->caca_zona[z] && sum > need) { p->caca_zona[z]--; sum--; }
    }
    while (sum > need && p->caca_zona[aqui]) { p->caca_zona[aqui]--; sum--; }
    if (p->escena == ESC_ISLA) return; /* en la isla el mar se la lleva: no se ensucia el suelo */
    while (sum < need && p->caca_zona[aqui] < 3) { p->caca_zona[aqui]++; sum++; }
}
/* suena música cerca del reloj (la detecta main con el micrófono, si lo activaste): baila y le encanta */
void pet_inject_event(pet_t *p, int tipo, int a, int b, uint32_t ahora) { push_event(p, (event_type_t)tipo, a, b, ahora); }
bool pet_has_song(const pet_t *p) { return p->cancion_n >= 3; }
int pet_melody(const pet_t *p, uint8_t *notas)
{
    if (!p || p->melodia_n == 0) return 0;
    const int n = p->melodia_n > PET_MELODIA_NOTAS ? PET_MELODIA_NOTAS : p->melodia_n;
    if (notas) memcpy(notas, p->melodia, (size_t)n);
    return n;
}
/* de grave a agudo los cristales suenan: azul (do), violeta (re), verde (mi), rosado (sol), blanco (la), dorado (do alto) */
static const uint8_t MEL_ESCALA[6] = {0, 4, 1, 2, 5, 3};
/* su melodía: un paseo por la escala de los cristales (de a un escalón, a veces un salto), que empieza y termina en "do" y sube y baja como una frase.
 * Sale de su identidad (id y especie), así que cada mascota tiene la suya. */
static void melodia_componer(pet_t *p)
{
    uint32_t h = mezcla32(p->id * 2654435761u + (uint32_t)p->especie * 40503u + 0x4D454Cu);
    int r = 0; /* escalón de la escala: 0..5 */
    p->melodia[0] = MEL_ESCALA[0];
    for (int i = 1; i < PET_MELODIA_NOTAS; i++) {
        h = mezcla32(h + (uint32_t)i * 977u);
        const int obj = i < PET_MELODIA_NOTAS / 2 ? 4 : 0; /* sube hacia la mitad y vuelve a casa */
        int paso = (int)(h % 5u) - 2; /* -2..+2 */
        if (h >> 20 & 1u) paso = obj > r ? 1 + (int)(h >> 8 & 1u) : -(1 + (int)(h >> 8 & 1u)); /* a veces, directo hacia donde va */
        if (i == PET_MELODIA_NOTAS - 1) paso = -r; /* la última, "do" */
        int nr = r + paso;
        if (nr < 0) nr = -nr;
        if (nr > 5) nr = 10 - nr;
        if (nr == r && i != PET_MELODIA_NOTAS - 1) nr = r < 5 ? r + 1 : r - 1; /* sin repetir la misma nota seguida */
        r = nr;
        p->melodia[i] = MEL_ESCALA[r];
    }
    p->melodia_n = PET_MELODIA_NOTAS;
}
static void paso_melodia(pet_t *p, uint32_t t, float dt, bool vivo)
{
    if (!vivo || p->melodia_n || p->etapa < STAGE_CRIA || p->dormida || p->enferma || p->escena != ESC_CUEVA) return;
    const uint16_t dia = (uint16_t)(1 + (dia_local(p, t) & 0x7FFF));
    if (p->melodia_dia == dia) return; /* un ensayo por día */
    if (!chance(p, prob_paso(1.5f, dt))) return; /* a los pocos minutos de estar en la cueva */
    p->melodia_dia = dia;
    p->melodia_ens++;
    impulsar(p, EMO_CURIOSIDAD, 0.5f, CAUSA_NOVEDAD);
    sumar(p, NEED_DIVERSION, 8);
    p->actividad = ACTV_MIRAR;
    p->actividad_hasta = t + 90u;
    if (p->melodia_ens < PET_MELODIA_ENSAYOS) {
        push_event(p, EV_ENSAYA_MELODIA, p->melodia_ens, (int)(mezcla32(p->id + (uint32_t)p->melodia_ens * 31u) % 6u), t);
        return;
    }
    melodia_componer(p); /* ya lo decidió: queda así para siempre */
    impulsar(p, EMO_ORGULLO, 0.9f, CAUSA_NOVEDAD);
    push_event(p, EV_MELODIA, p->melodia_n, 0, t);
    anotar(p, DIA_MELODIA, 0, 0, t);
}
void pet_music_heard(pet_t *p, uint32_t ahora, int tempo, int bpm, int energia)
{
    tempo = tempo < 0 ? 0 : tempo > 2 ? 2 : tempo;
    bpm = bpm < 0 ? 0 : bpm > 255 ? 255 : bpm;
    energia = energia < 0 ? 0 : energia > 3 ? 3 : energia;
    if (p->etapa == STAGE_HUEVO || p->dormida || p->enferma) return;
    p->actividad = ACTV_BAILAR;
    p->probando = (uint8_t)(1 + tempo); /* cómo baila: despacito, normal o a toda velocidad */
    p->actividad_hasta = ahora + 25u; /* sigue bailando mientras suene */
    /* su canción: la huella (ritmo y energía) que más se repite; cuenta cada ocasión distinta (más de 5 min entre una y otra) */
    bool suya = false, nueva_ocasion = !p->cancion_t || ahora - p->cancion_t > 300u;
    if (p->cancion_n == 0) { p->cancion_bpm = (uint8_t)bpm; p->cancion_en = (uint8_t)energia; p->cancion_n = 1; p->cancion_t = ahora; }
    else if (abs(bpm - (int)p->cancion_bpm) <= 7 && abs(energia - (int)p->cancion_en) <= 1) {
        suya = p->cancion_n >= 3;
        if (nueva_ocasion && p->cancion_n < 255) p->cancion_n++;
        p->cancion_bpm = (uint8_t)(((int)p->cancion_bpm * 3 + bpm) / 4);
        p->cancion_t = ahora;
    } else if (nueva_ocasion) { /* otra música: la huella se debilita y, si se borra, la reemplaza */
        if (--p->cancion_n == 0) { p->cancion_bpm = (uint8_t)bpm; p->cancion_en = (uint8_t)energia; p->cancion_n = 1; p->cancion_t = ahora; }
    }
    if (ahora - p->musica_ult > 600u || !p->musica_ult) { /* la primera vez (y cada 10 min): lo dice */
        p->musica_ult = ahora;
        impulsar(p, EMO_ALEGRIA, 0.8f, CAUSA_NOVEDAD);
        sumar(p, NEED_DIVERSION, 12);
        sumar(p, NEED_CARINO, 4);
        subir_vinculo(p, 0.4f);
        if (PET_APRENDE && p->gusto[ACTV_BAILAR] < 60) p->gusto[ACTV_BAILAR] = (int8_t)(p->gusto[ACTV_BAILAR] + 3);
        if (suya || p->cancion_n == 3) { /* ¡es su canción! */
            const bool primera = p->cancion_n == 3 && !suya;
            impulsar(p, EMO_ALEGRIA, 1.0f, CAUSA_NOVEDAD);
            subir_vinculo(p, 0.4f);
            push_event(p, EV_CANCION, primera ? 0 : 1, tempo, ahora);
            if (primera) anotar(p, DIA_CANCION, 0, tempo, ahora);
        } else push_event(p, EV_MUSICA, tempo * 3 + (int)(rng_next(p) % 3u), tempo, ahora);
    }
}
int pet_poops_here(const pet_t *p) { return p->escena < 8 ? p->caca_zona[p->escena] : 0; }
static void paso_semana(pet_t *p, uint32_t t);
static void paso_objetos(pet_t *p, uint32_t t)
{
    linaje_hora(p, t);
    paso_semana(p, t);
    if (p->guardiana) { /* la Guardiana: la magia del bosque la mantiene limpia (sin barro y sin cacas) */
        p->necesidad[NEED_HIGIENE] = 100.0f;
        p->barro = 0;
    }
    /* la Guardiana: en las noches de luna llena el bosque le manda luciérnagas */
    if (p->guardiana && pet_full_moon(t) && es_noche(p, t) && !p->dia_luciernagas && !p->cielo && hora_local(p, t) == 20 && t % 3600u < 1500u)
        p->dia_luciernagas = dia_local(p, t);
    /* la bendición del bosque: al día siguiente de la ofrenda, desde las 7 de la mañana */
    if (p->dia_ofrenda && dia_local(p, t) > p->dia_ofrenda && hora_local(p, t) >= 7) {
        p->dia_ofrenda = 0;
        if (p->etapa != STAGE_HUEVO) {
            int h = hora_local(p, t);
            if (h < 18 && p->clima != CLIMA_DESPEJADO) pet_force_weather(p, CLIMA_DESPEJADO, t, (uint32_t)(18 - h) * 3600u);
            p->dia_luciernagas = dia_local(p, t);
            push_event(p, EV_BENDICION, 0, 0, t);
        }
    }
    if (p->dia_luciernagas && dia_local(p, t) == p->dia_luciernagas && es_noche(p, t) && hora_local(p, t) >= 19 && !p->cielo) {
        p->dia_luciernagas = 0; /* esa noche: luciérnagas por todas partes un rato */
        p->cielo = 7; /* (1 + param 6: luciérnagas) */
        p->cielo_hasta = t + 1800u;
        push_event(p, EV_BENDICION, 1, 0, t);
    } else if (p->dia_luciernagas && dia_local(p, t) > p->dia_luciernagas) p->dia_luciernagas = 0;
    if (p->obj_cabeza && t >= p->cabeza_hasta) p->obj_cabeza = 0;
    if (p->cielo && t >= p->cielo_hasta) p->cielo = 0;
    if (p->compa && t >= p->compa_hasta) p->compa = 0;
}


/* ======================================================================
 * Versión 21: la luna, la ballena, la Guardiana, rituales, anécdotas, la semana y la familia
 * ==================================================================== */
static uint8_t dia_byte(const pet_t *p, uint32_t t) { return (uint8_t)(1 + (dia_local(p, t) & 0xFF)); }

float pet_moon_phase(uint32_t ahora)
{
    /* una luna nueva conocida: 6 de enero de 2000, 18:14 UTC; el mes lunar dura 29,53 días */
    double dias = ((double)ahora - 947182440.0) / 86400.0;
    double f = fmod(dias / 29.530588853, 1.0);
    if (f < 0) f += 1.0;
    return (float)f;
}
bool pet_full_moon(uint32_t ahora) { return fabsf(pet_moon_phase(ahora) - 0.5f) < 0.036f; }
int pet_nights_to_full_moon(uint32_t ahora)
{
    if (pet_full_moon(ahora)) return 0;
    float f = pet_moon_phase(ahora), falta = 0.5f - f;
    if (falta < 0) falta += 1.0f;
    int n = (int)ceilf(falta * 29.530588853f - 1.0f);
    return n < 1 ? 1 : n;
}

/* --- el paseo por el firmamento en la ballena de nubes --- */
bool pet_whale_can_ride(const pet_t *p, uint32_t ahora)
{
    return p->raro == RARO_GLOBO && !p->dormida && p->etapa != STAGE_HUEVO && p->dia_ballena != dia_byte(p, ahora);
}
int pet_whale_ride(pet_t *p, int estrellas, uint32_t ahora)
{
    if (p->etapa == STAGE_HUEVO || p->dia_ballena == dia_byte(p, ahora)) return 0;
    p->dia_ballena = dia_byte(p, ahora);
    if (p->raro == RARO_GLOBO) { push_event(p, EV_RARO, p->raro, 0, ahora); p->raro = 0xFF; } /* la ballena ya la dejó */
    p->ultima_interaccion = ahora;
    sumar(p, NEED_DIVERSION, 30);
    sumar(p, NEED_CURIOSIDAD, 30);
    impulsar(p, EMO_ALEGRIA, 0.9f, CAUSA_RARO);
    impulsar(p, EMO_CURIOSIDAD, 0.7f, CAUSA_RARO);
    subir_vinculo(p, 2.0f);
    int trae = estrellas >= 5 && pet_object_gives(p, 22, OFU_REGALO, ahora); /* polvo de estrellas: el Talco de estrellas */
    anotar(p, DIA_BALLENA, trae, 0, ahora);
    foto(p, DIA_BALLENA, 0, ahora);
    for (int k = 0; k < 5; k++) { /* un paseo, una historia, en orden (si hoy queda un momento de historia y ya es su semana) */
        if ((p->historias_nimba >> k) & 1u) continue;
        if (pet_mode(p) == MODO_INFANTE || !historia_abierta(p, k) || !momento_libre(p, ahora)) break;
        momento_usar(p, ahora);
        p->historias_nimba |= (uint8_t)(1u << k);
        anotar(p, DIA_HISTORIA, CRI_NIMBA, k, ahora);
        push_event(p, EV_HIST_NIMBA, k, 0, ahora);
        break;
    }
    return trae;
}

/* --- el rito de la Guardiana --- */
bool pet_guardian_ready(const pet_t *p, uint32_t ahora)
{
    return !p->guardiana && p->etapa == STAGE_ADULTA && p->bolsa[OBJ_CORAZON_BOSQUE] && p->escena == ESC_ISLA &&
           es_noche(p, ahora) && (pet_full_moon(ahora) || pet_days_played(p) >= 30); /* con luna llena (o, ya en la última semana de la historia, cualquier noche); aunque esté durmiendo: el Corazón la despierta */
}
static void rito_guardiana(pet_t *p, uint32_t t)
{
    if (p->dormida) despertarse(p, t, false);
    sumar(p, NEED_ENERGIA, 45); /* la magia la despierta del todo: se queda mirando la isla brillar un rato */
    p->necesidad[NEED_HIGIENE] = 100.0f; /* al rito llega limpia: la luz del Corazón la deja reluciente */
    p->barro = 0;
    p->guardiana = 1;
    p->dia_guardiana = dia_local(p, t);
    sacar_de_bolsa(p, OBJ_CORAZON_BOSQUE); /* el Corazón queda en la piedra del centro, latiendo */
    p->obj_sabe[OBJ_CORAZON_BOSQUE >> 3] |= (uint8_t)(1u << (OBJ_CORAZON_BOSQUE & 7));
    impulsar(p, EMO_ORGULLO, 1.0f, CAUSA_HALLAZGO);
    impulsar(p, EMO_ALEGRIA, 0.9f, CAUSA_HALLAZGO);
    subir_vinculo(p, 5.0f);
    sumar(p, NEED_SEGURIDAD, 40);
    p->cielo = 7; /* luciérnagas por toda la isla */
    p->cielo_hasta = t + 1800u;
    anotar(p, DIA_GUARDIANA, 0, 0, t);
    foto(p, DIA_GUARDIANA, 0, t);
    tilo_pagina(p, 7, t); /* «Para quien cuide el huevo» */
    push_event(p, EV_GUARDIANA, 0, 0, t);
    sonar(p, SND_SORPRESA, t);
}

/* --- la caja de música --- */
void pet_dance(pet_t *p, int aciertos, uint32_t ahora)
{
    if (p->etapa == STAGE_HUEVO || p->dormida) return;
    p->ultima_interaccion = ahora;
    sumar(p, NEED_DIVERSION, (float)(20 + (aciertos > 8 ? 8 : aciertos) * 2));
    impulsar(p, EMO_ALEGRIA, aciertos >= 4 ? 0.9f : 0.6f, CAUSA_JUGAMOS);
    if (aciertos >= 4) { subir_vinculo(p, 1.5f); impulsar(p, EMO_CARINO, 0.5f, CAUSA_JUGAMOS); }
    if (p->sem[0] < 255) p->sem[0]++;
    anotar(p, DIA_BAILE, aciertos >= 4, 0, ahora);
    push_event(p, EV_BAILE, aciertos, 0, ahora);
}

/* --- se adapta a ti: te espera a tu hora de siempre y aprende sus rituales --- */
bool pet_ritual(const pet_t *p, int cual) { return (unsigned)cual < 2 && (p->rit[cual] & 0x80); }
static void contar_ritual(pet_t *p, int k, uint32_t t)
{
    uint8_t hoy = dia_byte(p, t), ayer = (uint8_t)(1 + ((dia_local(p, t) - 1) & 0xFF));
    if (p->dia_rit[k] == hoy) return;
    int n = p->dia_rit[k] == ayer ? (p->rit[k] & 0x7F) + 1 : 1;
    bool sabe = p->rit[k] & 0x80;
    p->dia_rit[k] = hoy;
    if (!sabe && n >= 5) { /* cinco días seguidos: ya es nuestro ritual */
        sabe = true;
        anotar(p, DIA_RITUAL, k, 0, t);
        push_event(p, EV_RITUAL, k, 1, t);
        impulsar(p, EMO_CARINO, 0.8f, CAUSA_CARICIAS);
        subir_vinculo(p, 2.0f);
    } else if (sabe) {
        push_event(p, EV_RITUAL, k, 0, t);
        impulsar(p, EMO_CARINO, 0.5f, CAUSA_CARICIAS);
    }
    p->rit[k] = (uint8_t)((sabe ? 0x80 : 0) | (n > 127 ? 127 : n));
}
/* la hora de dormir que se adapta a ti: se corre de a poco hacia la hora en que apagas la luz (0) y en que la
 * saludas en la mañana (1) */
static void aprender_hora(pet_t *p, int q, uint32_t t)
{
    hora_dormir_t *d = &p->dormir;
    if (d->modo != DORMIR_ADAPTARSE) return;
    int64_t l = (int64_t)t + p->zona_horaria;
    int ahora = (int)((l % 86400 + 86400) % 86400 / 60);
    int antes = q ? d->despertar_h * 60 + d->despertar_m : d->acostarse_h * 60 + d->acostarse_m;
    if (!q) { if (ahora < 720) ahora += 1440; if (antes < 720) antes += 1440; } /* de noche cruza la medianoche */
    int nuevo = antes + (ahora - antes) * 3 / 10;
    nuevo = ((nuevo + 7) / 15) * 15 % 1440; /* de a cuarto de hora */
    if (q) { d->despertar_h = (uint8_t)(nuevo / 60); d->despertar_m = (uint8_t)(nuevo % 60); }
    else { d->acostarse_h = (uint8_t)(nuevo / 60); d->acostarse_m = (uint8_t)(nuevo % 60); }
}
static void adaptarse(pet_t *p, action_t a, uint32_t t)
{
    if (p->etapa == STAGE_HUEVO) return;
    int h = hora_local(p, t);
    bool manana_nueva = h >= 5 && h < 11 && a != ACT_DORMIR && p->dia_rit[0] != dia_byte(p, t);
    if (a == ACT_DORMIR && (h >= 19 || h < 2)) {
        contar_ritual(p, 1, t); /* apagar la luz cada noche: buenas noches */
        aprender_hora(p, 0, t);
    }
    if (manana_nueva) aprender_hora(p, 1, t);
    if (p->dormida) return;
    if (h >= 5 && h < 11 && a != ACT_DORMIR) contar_ritual(p, 0, t); /* lo primero de la mañana: buenos días */
    int k = a == ACT_ALIMENTAR ? HAB_ALIMENTAR : (a == ACT_JUGAR ? HAB_JUGAR : (a == ACT_ACARICIAR ? HAB_ACARICIAR : -1));
    if (k >= 0 && p->dia_te_esperaba != dia_byte(p, t) && expectativa(p, k, t) > 0.6f) {
        p->dia_te_esperaba = dia_byte(p, t); /* llegaste a tu hora de siempre */
        impulsar(p, EMO_ALEGRIA, 0.8f, CAUSA_CARICIAS);
        push_event(p, EV_TE_ESPERABA, k, 0, t);
    }
}

/* --- la semana: cada lunes una página con lo que vivieron --- */
static void paso_semana(pet_t *p, uint32_t t)
{
    if (p->etapa == STAGE_HUEVO) return;
    int32_t w = (dia_local(p, t) + 3) / 7; /* semanas que empiezan el lunes */
    if (!p->semana) { p->semana = w; return; }
    if (w == p->semana) return;
    if (p->sem[0] || p->sem[1] || p->sem[2]) {
        int juegos = p->sem[0] > 127 ? 127 : p->sem[0], mimos = p->sem[1] > 15 ? 15 : p->sem[1], cosas = p->sem[2] > 7 ? 7 : p->sem[2];
        anotar(p, DIA_SEMANA, juegos, mimos << 3 | cosas, t);
    }
    { /* la actividad que más repitió: una página con "lo mío fue…" */
        int mejor = -1, mv = 3;
        for (int i = 0; i < ACTV_COUNT; i++) if (aprende_de(i) && p->sem_act[i] > mv) { mv = p->sem_act[i]; mejor = i; }
        if (PET_APRENDE && mejor >= 0 && pet_activity_verb(mejor)[0]) anotar(p, DIA_GUSTO_SEM, mejor, mv > 127 ? 127 : mv, t);
    }
    memset(p->sem_act, 0, sizeof p->sem_act);
    memset(p->sem, 0, sizeof p->sem);
    p->semana = w;
}

/* --- anécdotas, la familia y la despedida natural: se revisan cuando decide qué hacer --- */
static void paso_largo(pet_t *p, uint32_t t, bool vivo)
{
    if (p->etapa == STAGE_HUEVO || p->dormida) return;
    const int32_t hoy = dia_local(p, t);
    const bool contigo = t - p->ultima_interaccion < 600u;
    /* "¿te acuerdas...?": algo que vivieron hace días (una vez al día, si estás con ella) */
    if (contigo && p->dia_anecdota != dia_byte(p, t) && pet_diary_count(p) >= 6 && chance(p, 0.12f)) {
        int n = pet_diary_count(p), cand[PET_DIARIO], nc = 0;
        for (int i = 0; i < n; i++) {
            const entrada_diario_t *d = pet_diary_get(p, i);
            if (d && t - d->t > 3u * 86400u && d->tipo != DIA_SEMANA && d->tipo != DIA_META) cand[nc++] = i;
        }
        if (nc) {
            p->dia_anecdota = dia_byte(p, t);
            push_event(p, EV_ANECDOTA, cand[rng_next(p) % (uint32_t)nc], 0, t);
            impulsar(p, EMO_CARINO, 0.5f, CAUSA_CARICIAS);
        }
    }
    /* la familia: de adulta, muy querida y ya grande, a veces deja un huevo (uno por mes, hasta tres) */
    if (p->etapa == STAGE_ADULTA && !p->huevo_familia && p->hijos < 3 && p->vinculo >= 90.0f && pet_age_days(p, t) >= 21.0f &&
        (!p->dia_familia || hoy - p->dia_familia >= 30) && chance(p, 0.05f)) {
        p->huevo_familia = 1;
        p->dia_familia = hoy;
        anotar(p, DIA_FAMILIA, 0, 0, t);
        foto(p, DIA_FAMILIA, 0, t);
        impulsar(p, EMO_ORGULLO, 0.9f, CAUSA_HALLAZGO);
        push_event(p, EV_FAMILIA, 0, 0, t);
        if (vivo) sonar(p, SND_SORPRESA, t);
    }
    /* (v39) el segundo final, «El último verso»: ya es la Guardiana (desde otro día), compusieron nuestra canción, y una noche
     * despejada están en la cueva: Prisma propone terminar el verso de Tilo */
    if (pet_final2_ready(p, t)) p->final_pend = PET_FINAL_SECRETO;
    /* la despedida natural (solo si la activaste): ya grande, a veces siente ganas de explorar el mundo y te pregunta */
    if (p->despedida_natural && contigo && p->etapa == STAGE_ADULTA && pet_age_days(p, t) >= 365.0f && (p->hijos || pet_age_days(p, t) >= 730.0f) &&
        (!p->dia_pide_viaje || hoy - p->dia_pide_viaje >= 30) && hora_local(p, t) >= 10 && hora_local(p, t) < 19 && chance(p, 0.1f)) {
        p->dia_pide_viaje = hoy;
        push_event(p, EV_PIDE_VIAJE, 0, 0, t);
    }
}
void pet_trip_not_yet(pet_t *p, uint32_t ahora) { p->dia_pide_viaje = dia_local(p, ahora); impulsar(p, EMO_CARINO, 0.6f, CAUSA_CARICIAS); }

int pet_objects_used(const pet_t *p)
{
    int n = 0;
    for (int i = 0; i < PET_OBJETOS; i++) n += ((p->obj_sabe[i >> 3] >> (i & 7)) & 1) & ((p->obj_visto[i >> 3] >> (i & 7)) & 1); /* (el hijo hereda lo que sabe, pero tiene que volver a encontrarlos) */
    return n;
}
int pet_final_pending(const pet_t *p) { return p->final_pend; }
bool pet_final_seen(const pet_t *p, int cual) { return cual == PET_FINAL_REGULAR ? p->guardiana != 0 : (p->final_estado & cual) != 0; }
bool pet_libro_seen(const pet_t *p, int n) { return n >= 0 && n < PET_LIBRO_PAGINAS && ((p->libro >> n) & 1u); }
void pet_libro_mark(pet_t *p, int n) { if (n >= 0 && n < PET_LIBRO_PAGINAS) p->libro |= (uint8_t)(1u << n); }

void pet_final_done(pet_t *p, int cual, uint32_t t)
{
    if (cual != PET_FINAL_SECRETO) return; /* el regular es el rito de la Guardiana: ya lo hace pet_guardian_rite */
    p->final_pend = 0;
    if (p->final_estado & cual) return;
    p->final_estado |= (uint8_t)cual;
    tilo_pagina(p, 8, t); /* (v39) la última página del diario: la carta, escrita para ella */
    anotar(p, DIA_FINAL, 1, 0, t);
    foto(p, DIA_FINAL, 1, t);
    subir_vinculo(p, 5.0f);
    impulsar(p, EMO_ALEGRIA, 1.0f, CAUSA_HALLAZGO);
    impulsar(p, EMO_CARINO, 1.0f, CAUSA_CARICIAS);
}

void pet_family_child(pet_t *madre, pet_t *hijo, uint32_t ahora, uint32_t seed)
{
    if (hijo != madre) *hijo = *madre; /* lo del dueño (fecha, hora, amigos, el jardín) es el mismo */
    /* el huevo es una sorpresa: la especie y el color salen al azar (no copian a su madre) */
    const uint32_t sorteo = seed * 2654435761u + 0x9E3779B9u;
    pet_rebirth(hijo, (uint8_t)((sorteo >> 8) % (uint32_t)pet_species_count()), ahora, seed);
    hijo->color = (uint8_t)((sorteo >> 20) % PET_COLORES);
    hijo->libro = madre->libro; /* el Libro ya le contó el primer libro a su madre */
    hijo->aspecto_ok = 2; /* la forma y el color ya vienen de familia */
    hijo->genero = (uint8_t)((seed >> 3) & 1u); /* niña o niño: lo decide el bosque */
    for (int i = 0; i < TRAIT_COUNT; i++) hijo->rasgo[i] = 0.5f * hijo->rasgo[i] + 0.5f * madre->rasgo[i]; /* algo de su manera de ser */
    for (int e = 0; e < ESC_GUSTOS; e++) hijo->gusto_escena[e] = (int8_t)((hijo->gusto_escena[e] + madre->gusto_escena[e]) / 2);
    memcpy(hijo->obj_sabe, madre->obj_sabe, sizeof hijo->obj_sabe); /* lo que ella sabía del libro mágico: él sabe qué hacen, pero tendrá que encontrarlos otra vez */
    hijo->linaje[hijo->especie & 7] = 40;
    hijo->generacion = (uint8_t)(madre->generacion < 250 ? madre->generacion + 1 : 250);
    snprintf(hijo->padre, sizeof hijo->padre, "%s", madre->nombre);
    hijo->padre_genero = madre->genero; /* la carta la firma su mamá o su papá */
    hijo->despedida_natural = madre->despedida_natural;
    hijo->hijos = 0; hijo->huevo_familia = 0; hijo->dia_familia = 0; hijo->dia_pide_viaje = 0; hijo->guardiana = 0;
    madre->huevo_familia = 0;
    if (madre->hijos < 255) madre->hijos++;
    anotar(madre, DIA_FAMILIA, 1, 0, ahora);
    push_event(madre, EV_FAMILIA, 1, 0, ahora);
    pet_seal(madre);
    pet_seal(hijo);
}

/* ======================================================================
 * Regalos al volver y la niebla de la mañana
 * ==================================================================== */
bool pet_object_discovered(const pet_t *p, int id) { return (unsigned)id < PET_OBJETOS && (p->obj_sabe[id >> 3] >> (id & 7) & 1); }
const char *pet_gift_name(regalo_t r)
{
    static const char *const T[REG_COUNT] = {"una piña", "una hoja gigante", "el palo perfecto", "una pluma", "una semilla con alas",
                                             "una piedra de río", "las botas con barro"};
    return (unsigned)r < REG_COUNT ? T[r] : "algo";
}

/* volviste después de un rato: a veces te esperaba con algo de afuera (una vez al día) */
static void regalo_al_volver(pet_t *p, uint32_t ausente, uint32_t t)
{
    if (p->etapa == STAGE_HUEVO || p->dormida) return;
    int32_t hoy = dia_local(p, t);
    if (hoy == p->dia_regalo_vuelta) return;
    float prob = clampf(0.35f + (float)(ausente / 3600u) * 0.04f + p->vinculo / 300.0f, 0, 0.85f);
    if (!chance(p, prob)) return;
    estacion_t e = pet_season(p, t);
    float w[REG_COUNT] = {1.0f, 1.0f, 1.2f, 1.0f, 0.8f, 1.0f, 0.5f};
    if (e == ESTACION_OTONO) { w[REG_HOJA] += 1.5f; w[REG_SAMARA] += 1.5f; w[REG_PINA] += 0.8f; }
    if (e == ESTACION_INVIERNO) w[REG_PINA] += 1.2f;
    if (p->clima == CLIMA_LLUVIA || p->memoria.clima[CLIMA_LLUVIA] > 50) w[REG_BOTAS] += 1.5f;
    if (p->modo_nino == MODO_INFANTE) w[REG_BOTAS] = 0; /* nada que limpiar para los más chicos */
    float tot = 0;
    for (int i = 0; i < REG_COUNT; i++) tot += w[i];
    float r = rng01(p) * tot;
    int g = REG_PINA;
    for (int i = 0; i < REG_COUNT; i++) { if (r < w[i]) { g = i; break; } r -= w[i]; }
    pet_gift_now(p, (regalo_t)g, t);
}
void pet_gift_now(pet_t *p, regalo_t g, uint32_t t)
{
    if ((unsigned)g >= REG_COUNT || p->etapa == STAGE_HUEVO) return;
    p->dia_regalo_vuelta = dia_local(p, t);
    switch (g) {
    case REG_PINA: ganar_semilla(p, tipo_de_semilla(p, t), t); break;
    case REG_HOJA: p->hoja_hasta = t + 4u * 3600u; p->hoja_nenufar = 0; break;
    case REG_PALO: p->palo_hasta = t + 2u * 3600u; break;
    case REG_PLUMA: pet_object_gives(p, 60, OFU_REGALO, t); break; /* pluma de arrendajo */
    case REG_SAMARA: { /* la planta: crecerá un arbolito (si no hay lugar, queda en la Bolsa) */
        bool planto = false;
        for (int i = 0; i < PET_PARCELAS && !planto; i++) {
            planta_t *pl = &p->jardin[i];
            if (pl->tipo != PLANTA_NADA) continue;
            memset(pl, 0, sizeof *pl);
            pl->tipo = PLANTA_ARBOL;
            pl->agua = 80.0f;
            push_event(p, EV_JARDIN, i, JAR_ENTERRO, t);
            planto = true;
        }
        if (!planto) pet_object_gives(p, 42, OFU_REGALO, t); /* semilla helicóptero */
        break;
    }
    case REG_PIEDRA: if (!pet_object_gives(p, 10, OFU_REGALO, t)) guardar_item(p, ITEM_PIEDRA); break; /* guijarro cantarín */
    case REG_BOTAS: p->barro = 1; sumar(p, NEED_HIGIENE, -25); break;
    default: break;
    }
    p->regalo_vuelta = (uint8_t)(g + 1);
    impulsar(p, EMO_ORGULLO, 0.6f, CAUSA_VOLVISTE);
    subir_vinculo(p, 0.5f);
    push_event(p, EV_REGALO_VUELTA, g, 0, t);
    anotar(p, DIA_REGALO_VUELTA, g, 0, t);
    sonar(p, SND_ALEGRE, t);
}

void pet_fog_start(pet_t *p, uint32_t ahora, uint32_t segundos)
{
    p->niebla_hasta = ahora + segundos;
    push_event(p, EV_NIEBLA, 1, 0, ahora);
}
bool pet_fog(const pet_t *p, uint32_t ahora) { return p->niebla_hasta && ahora < p->niebla_hasta; }
void pet_fog_clear(pet_t *p, uint32_t ahora)
{
    if (!pet_fog(p, ahora)) return;
    p->niebla_hasta = 0;
    impulsar(p, EMO_ALEGRIA, 0.5f, CAUSA_SOL);
    push_event(p, EV_NIEBLA, 0, 1, ahora);
}
/* algunas mañanas amanece con niebla (más en otoño e invierno); se va sola a media mañana */
static void paso_niebla(pet_t *p, uint32_t t, bool vivo)
{
    if (p->niebla_hasta && t >= p->niebla_hasta) {
        p->niebla_hasta = 0;
        if (vivo) push_event(p, EV_NIEBLA, 0, 0, t);
    }
    int h = hora_local(p, t);
    int32_t hoy = dia_local(p, t);
    if (h < 5 || h >= 9 || hoy == p->dia_niebla) return;
    p->dia_niebla = hoy;
    estacion_t e = pet_season(p, t);
    float prob = (e == ESTACION_OTONO || e == ESTACION_INVIERNO) ? 0.3f : 0.12f;
    if (p->clima == CLIMA_TORMENTA || p->clima == CLIMA_VIENTO || !chance(p, prob)) return;
    p->niebla_hasta = t + (uint32_t)(11 - h) * 3600u;
    impulsar(p, EMO_CURIOSIDAD, 0.4f, CAUSA_NOVEDAD);
    push_event(p, EV_NIEBLA, 1, 0, t);
}

/* ======================================================================
 * Linaje y habilidades
 * ==================================================================== */
static int linaje_de_objeto(int id)
{
    const objeto_info_t *o = objeto_info(id);
    if (!o) return -1;
    if ((o->efecto == OEF_CLIMA && o->param == CLIMA_TORMENTA) || o->gesto == OGE_FROTAR) return 1;
    if (o->efecto == OEF_PESCA || o->familia == OFA_ESCAMA || o->familia == OFA_CONCHA || o->lugares == OL_LAGO) return 3;
    if ((o->efecto == OEF_CLIMA && o->param == CLIMA_DESPEJADO) || o->efecto == OEF_CURAR || o->efecto == OEF_CALMA) return 2;
    if (o->noche || o->lugares == OL_CUEVA) return 7;
    if (o->efecto == OEF_COMPA || o->familia == OFA_LAMPARA || o->familia == OFA_HONGO) return 6;
    if (o->efecto == OEF_CIELO || o->familia == OFA_PLUMA) return 5;
    if (o->cat == OCAT_ANTIGUO || o->familia == OFA_SEMILLA || o->familia == OFA_LLAVE || o->familia == OFA_CAJA) return 4;
    if (o->cat == OCAT_FLORA || o->efecto == OEF_JARDIN) return 0;
    return -1;
}
static void linaje_suma(pet_t *p, int l, int puntos)
{
    if ((unsigned)l >= 8 || p->etapa == STAGE_ADULTA || p->etapa == STAGE_HUEVO) return;
    p->linaje[l] = (uint16_t)(p->linaje[l] + puntos > 60000 ? 60000 : p->linaje[l] + puntos);
}
/* cada hora: dónde estuvo y qué clima vivió */
static void linaje_hora(pet_t *p, uint32_t t)
{
    if (p->etapa == STAGE_ADULTA || p->etapa == STAGE_HUEVO || !p->linaje_hora || t < p->linaje_hora) { p->linaje_hora = t; return; }
    int horas = 0;
    while (t - p->linaje_hora >= 3600u && horas < 72) { p->linaje_hora += 3600u; horas++; }
    if (!horas) return;
    static const int8_t ZONA[ESC_COUNT] = {2, 0, 3, 5, 7, 3, 0, 5}; /* prado: el sol; bosque: hojas; lago: río; colina: el cielo; cueva: sombras; isla: río */
    int z = ZONA[p->escena % ESC_COUNT];
    if (p->escena == ESC_ISLA && es_noche(p, t)) z = 5; /* de noche en la isla: el faro y las estrellas, el cielo */
    linaje_suma(p, z, horas);
    if (p->clima == CLIMA_TORMENTA) linaje_suma(p, 1, 3 * horas);
    if (p->clima == CLIMA_LLUVIA) linaje_suma(p, 3, horas);
    if (p->clima == CLIMA_DESPEJADO) linaje_suma(p, 2, horas / 2 + 1);
}
int pet_lineage_lead(const pet_t *p)
{
    int propia = p->especie & 7, mejor = propia;
    /* su propio linaje parte con ventaja: otro tiene que marcarla el doble y bastante */
    float ref = (float)p->linaje[propia] + 20.0f;
    for (int i = 0; i < 8; i++)
        if (i != propia && p->linaje[i] >= 40 && (float)p->linaje[i] >= 2.0f * ref && (mejor == propia || p->linaje[i] > p->linaje[mejor])) mejor = i;
    return mejor;
}
int pet_lineage_shape(const pet_t *p) { return p->mezcla && p->etapa == STAGE_ADULTA ? (p->mezcla - 1) & 7 : (p->especie & 7); }

bool pet_ability_has(const pet_t *p, int cual) { return cual == 0 ? (p->etapa >= STAGE_CRIA && p->etapa <= STAGE_ADULTA) : (cual == 1 && p->etapa == STAGE_ADULTA); }
bool pet_ability_ready(const pet_t *p, int cual, uint32_t ahora)
{
    return pet_ability_has(p, cual) && p->hab_usada[cual & 1] != (uint8_t)((dia_local(p, ahora) & 0xFF) + 1);
}
/* ¿tiene sentido usarla ahora? 0 = sí; si no, un motivo (pet_ability_block_text) y no se gasta el uso del día */
int pet_ability_block(const pet_t *p, int cual, uint32_t ahora)
{
    cual &= 1;
    const int esp = cual == 0 ? (p->especie & 7) : pet_lineage_shape(p);
    const bool noche = es_noche_su(p, ahora), tapado = p->clima == CLIMA_NUBLADO || p->clima == CLIMA_LLUVIA || p->clima == CLIMA_TORMENTA || p->clima == CLIMA_NIEVE || p->niebla_hasta > ahora;
    int plantas = 0, secas = 0;
    for (int k = 0; k < PET_PARCELAS_TOTAL; k++) {
        const planta_t *pl = pet_plant(p, k);
        if (pl->tipo == PLANTA_NADA || pl->mustia == 2) continue;
        plantas++;
        if (pl->agua < 30.0f) secas++;
    }
    switch (esp * 2 + cual) {
    case 0: return plantas ? 0 : 1;                                                   /* florecer: sin plantas */
    case 1: { float m = 0; for (int e = EMO_TRISTEZA; e <= EMO_MIEDO; e++) if (p->emo[e] > m) m = p->emo[e];
              return m < 0.15f && p->necesidad[NEED_SEGURIDAD] > 85.0f ? 2 : 0; }     /* pétalos: no tiene penas */
    case 2: return p->clima == CLIMA_DESPEJADO && p->niebla_hasta <= ahora ? 3 : 0;   /* despejar */
    case 3: { int n = 0; for (int k = 0; k < PET_OBJETOS; k++) if (p->obj_dia[k]) n++; return n ? 0 : 4; } /* recargar */
    case 4: return !p->barro && p->necesidad[NEED_HIGIENE] > 85.0f && !secas ? 5 : 0; /* purificar */
    case 5: return noche ? 6 : (p->clima == CLIMA_DESPEJADO && p->niebla_hasta <= ahora ? 7 : 0); /* sol */
    case 6: return !pet_scene_open(p, ESC_LAGO) ? 8 : (p->pesca_hasta > ahora ? 9 : 0); /* buena pesca */
    case 7: return !plantas ? 1 : (p->clima == CLIMA_LLUVIA || p->clima == CLIMA_TORMENTA ? 10 : (p->clima == CLIMA_NIEVE || p->clima == CLIMA_FRIO ? 11 : 0)); /* llovizna */
    case 8: return p->tesoro_pendiente ? 12 : 0;                                      /* olfatear */
    case 10: return !noche ? 13 : (tapado ? 14 : 0);                                  /* constelación */
    case 11: return p->raro && p->raro_hasta > ahora ? 15 : 0;                        /* ballena */
    case 12: return p->suerte_hasta > ahora ? 16 : 0;                                 /* polvo de hadas */
    case 13: return noche ? 17 : (p->clima == CLIMA_LLUVIA || p->clima == CLIMA_TORMENTA || p->clima == CLIMA_NIEVE || p->clima == CLIMA_FRIO ? 18 : (plantas ? 0 : 1)); /* mariposas */
    case 14: return p->protege_hasta > ahora ? 19 : 0;                                /* espantar */
    default: return 0;
    }
}
const char *pet_ability_block_text(int code)
{
    static const char *const T[] = {"", "NO HAY PLANTAS QUE CUIDAR", "ESTÁ TRANQUILA, NO TIENE PENAS", "EL CIELO YA ESTÁ DESPEJADO", "NINGÚN OBJETO NECESITA RECARGA",
        "TODO ESTÁ LIMPIO Y SANO", "EL SOL SOLO SALE DE DÍA", "YA HAY SOL", "AÚN NO CONOCE LA LAGUNA", "YA HAY BUENA PESCA HOY",
        "YA ESTÁ LLOVIENDO", "CON ESTE FRÍO NO LLOVIZNA", "YA OLIÓ UN TESORO: ¡A CAVAR!", "LAS ESTRELLAS SOLO SALEN DE NOCHE", "EL CIELO ESTÁ TAPADO: NO SE VEN LAS ESTRELLAS",
        "YA HAY ALGO ESPECIAL EN EL CIELO", "YA TIENE SUERTE POR AHORA", "LAS MARIPOSAS DUERMEN DE NOCHE", "CON ESTE CLIMA NO VUELAN MARIPOSAS", "YA ESTÁ PROTEGIDO POR HOY"};
    return code > 0 && code < (int)(sizeof T / sizeof T[0]) ? T[code] : "";
}
const char *pet_ability_name(int especie, int cual)
{
    static const char *const N[8][2] = {{"Florecer", "Pétalos"}, {"Despejar", "Recargar"}, {"Purificar", "Sol"}, {"Buena pesca", "Llovizna"},
                                        {"Olfatear", "Semilla nueva"}, {"Constelación", "Ballena"}, {"Polvo de hadas", "Mariposas"},
                                        {"Espantar", "Escondidas"}};
    return N[especie & 7][cual & 1];
}
int pet_ability_use(pet_t *p, int cual, uint32_t ahora)
{
    cual &= 1;
    if (!pet_ability_has(p, cual) || p->dormida) return 0;
    if (!pet_ability_ready(p, cual, ahora)) return -2;
    if (pet_ability_block(p, cual, ahora)) return -3; /* no tiene sentido ahora: no se gasta */
    /* la primera es de su especie; la segunda, de la forma que tomó al crecer */
    const int esp = cual == 0 ? (p->especie & 7) : pet_lineage_shape(p);
    switch (esp * 2 + cual) {
    case 0: /* hojas: el jardín florece */
        for (int i = 0; i < PET_PARCELAS_TOTAL; i++) {
            planta_t *pl = parcela(p, i);
            if (pl->tipo == PLANTA_NADA || pl->mustia == 2) continue; /* (las que se marchitaron de viejas no vuelven) */
            pl->agua = 100.0f; pl->mustia = 0;
            if (pl->etapa < ETAPA_GRANDE) { pl->etapa++; pl->crece = 0; }
        }
        push_event(p, EV_JARDIN, -1, JAR_CRECIO, ahora);
        break;
    case 1: /* pétalos: se le pasa cualquier pena */
        for (int e = EMO_TRISTEZA; e <= EMO_MIEDO; e++) p->emo[e] *= 0.2f;
        sumar(p, NEED_SEGURIDAD, 25);
        break;
    case 2: /* rayo: despeja la niebla y la tormenta */
        p->niebla_hasta = 0;
        if (p->clima == CLIMA_TORMENTA || p->clima == CLIMA_LLUVIA || p->clima == CLIMA_NUBLADO) pet_force_weather(p, CLIMA_DESPEJADO, ahora, 10800u);
        break;
    case 3: memset(p->obj_dia, 0, sizeof p->obj_dia); break; /* recargar: los objetos que descansaban vuelven a brillar */
    case 4: /* purificar: sin barro y las plantas secas reviven */
        p->barro = 0;
        sumar(p, NEED_HIGIENE, 30);
        for (int i = 0; i < PET_PARCELAS_TOTAL; i++) {
            planta_t *pl = parcela(p, i);
            if (pl->tipo != PLANTA_NADA && pl->mustia != 2 && pl->agua < 30.0f) { pl->agua = 100.0f; pl->mustia = 0; }
        }
        break;
    case 5: pet_force_weather(p, CLIMA_DESPEJADO, ahora, 28800u); break; /* sol */
    case 6: p->pesca_hasta = ahora + 21600u; break;                      /* buena pesca */
    case 7: /* llovizna que riega todo */
        pet_force_weather(p, CLIMA_LLUVIA, ahora, 1800u);
        for (int i = 0; i < PET_PARCELAS_TOTAL; i++) { planta_t *pl = parcela(p, i); if (pl->tipo != PLANTA_NADA && pl->mustia != 2) { pl->agua = 100.0f; pl->mustia = 0; } }
        break;
    case 8: p->tesoro_pendiente = 1; desenterrar(p, ahora, true); break; /* olfatear: desentierra un tesoro */
    case 9: { /* una semilla de un tipo que no tiene */
        int tipo = -1;
        for (int c = 0; c < CULTIVO_COUNT && tipo < 0; c++) if (!(p->semillas_vistas >> c & 1)) tipo = c;
        if (tipo < 0) tipo = (int)(rng_next(p) % CULTIVO_COUNT);
        ganar_semilla(p, tipo, ahora);
        break;
    }
    case 10: p->cielo = 8; p->cielo_hasta = ahora + 120u; break; /* constelaciones */
    case 11: empezar_raro(p, es_noche(p, ahora) ? RARO_ESTRELLA_FUGAZ : RARO_GLOBO, ahora, 360, true); break; /* la ballena o una estrella */
    case 12: p->suerte_hasta = ahora + 21600u; p->cielo = 6; p->cielo_hasta = ahora + 90u; break;             /* polvo de hadas */
    case 13: /* mariposas que hacen florecer */
        aparece(p, FAUNA_MARIPOSA, ahora, 1200u, true);
        for (int i = 0; i < PET_PARCELAS_TOTAL; i++) { planta_t *pl = parcela(p, i); if (pl->tipo != PLANTA_NADA && pl->etapa == ETAPA_GRANDE) pl->etapa = ETAPA_FRUTOS; }
        break;
    case 14: /* espantar al zorro */
        if (pet_fauna_here(p, FAUNA_ZORRO, ahora)) { p->zorro_roba = 0; se_va(p, FAUNA_ZORRO, ahora, false); push_event(p, EV_FAUNA, FAUNA_ZORRO, FSUC_ESPANTADO, ahora); }
        p->protege_hasta = ahora + 21600u;
        break;
    default: break; /* escondidas: el juego lo pone la pantalla */
    }
    p->hab_usada[cual] = (uint8_t)((dia_local(p, ahora) & 0xFF) + 1);
    sumar(p, NEED_DIVERSION, 10);
    impulsar(p, EMO_ORGULLO, 0.7f, CAUSA_NOVEDAD);
    p->ultima_interaccion = ahora;
    push_event(p, EV_HABILIDAD_USO, esp, cual, ahora);
    return 1;
}
bool pet_set_color(pet_t *p, int color)
{
    if (p->etapa != STAGE_BEBE) return false;
    p->color = (uint8_t)((unsigned)color % PET_COLORES);
    p->aspecto_ok = 2; /* ya se eligió */
    return true;
}

/* (fw 104) paseo de verdad: el firmware avisa cuando hubo una caminata sostenida (pasos con la pantalla apagada). Cuenta como
 * una excursión juntos: ánimo, diario y, si fue larga, algo encontrado. Máximo 2 premiados por día (agitarlo no sirve). */
/* (fw 104, pruebas) el trocito de hielo: 1 lo pesca (como en invierno), 2 se derrite ya */
void pet_ice_test(pet_t *p, int que, uint32_t t)
{
    if (que == 1) aplicar_pesca(p, PESCA_HIELO, t, true);
    else if (que == 2 && ((p->coleccion[COL_CRISTALES] >> 3) & 1u)) {
        p->coleccion[COL_CRISTALES] &= (uint8_t)~(1u << 3);
        p->hielo_fuera = 0;
        anotar(p, DIA_HIELO, 1, 0, t);
        push_event(p, EV_HIELO, 1, 0, t);
    }
}
int pet_duo_game(pet_t *p, int juego, int resultado, uint32_t t)
{
    if (!p || p->etapa == STAGE_HUEVO || p->dormida || (unsigned)juego >= PET_DOS_COUNT) return 0;
    if (resultado < 0) resultado = 0;
    if (resultado > 2) resultado = 2;
    if (p->dos_jugadas < 255) p->dos_jugadas++;
    if (resultado == 2 && p->dos_ganadas < 255) p->dos_ganadas++;
    sumar(p, NEED_DIVERSION, 12.0f + 4.0f * (float)resultado);
    sumar(p, NEED_ENERGIA, -3);
    impulsar(p, EMO_ALEGRIA, 0.4f + 0.15f * (float)resultado, CAUSA_JUGAMOS);
    if (resultado == 2) impulsar(p, EMO_ORGULLO, 0.5f, CAUSA_JUGAMOS);
    if (p->vecino.presente) { p->vecino.amistad += 2.0f; if (p->vecino.amistad > 100.0f) p->vecino.amistad = 100.0f; }
    p->ultima_interaccion = t;
    const uint8_t hoy = (uint8_t)(1u + ((uint32_t)dia_local(p, t) & 0x7Fu));
    if (p->dos_dia != hoy) { p->dos_dia = hoy; anotar(p, DIA_JUEGO_DOS, juego, resultado, t); }
    return 1;
}
int pet_walk_together(pet_t *p, uint32_t pasos, uint32_t t)
{
    if (p->etapa == STAGE_HUEVO || p->dormida || pasos < 800u || pasos > 30000u) return 0;
    const uint8_t hoy = (uint8_t)(1u + ((uint32_t)dia_local(p, t) & 0x7Fu));
    if (p->paseo_dia != hoy) { p->paseo_dia = hoy; p->paseo_n = 0; }
    if (p->paseo_n >= 2) return 0;
    p->paseo_n++;
    sumar(p, NEED_DIVERSION, 25);
    sumar(p, NEED_CURIOSIDAD, 20);
    sumar(p, NEED_CARINO, 15);
    sumar(p, NEED_ENERGIA, -8);
    sumar(p, NEED_SACIEDAD, -6);
    impulsar(p, EMO_ALEGRIA, 0.8f, CAUSA_HALLAZGO);
    impulsar(p, EMO_CARINO, 0.5f, CAUSA_CARICIAS);
    subir_vinculo(p, 1.0f);
    p->ultima_interaccion = t;
    const int cientos = (int)(pasos / 100u);
    anotar(p, DIA_PASEO_JUNTOS, cientos > 255 ? 255 : cientos, 0, t);
    push_event(p, EV_PASEO_JUNTOS, cientos > 127 ? 127 : cientos, 0, t);
    sonar(p, SND_ALEGRE, t);
    if (pasos >= 2500u) { /* largo: trae algo del camino (lo mismo que encontraría explorando) */
        if (!quizas_objeto(p, OFU_EXPLORAR, 0.5f, t)) guardar_material(p, chance(p, 0.5f) ? HALL_MUSGO : HALL_RAMITA);
        return 2;
    }
    return 1;
}

void pet_steps_add(pet_t *p, uint32_t pasos, uint32_t ahora)
{
    if (!pasos || pasos > 20000) return; /* (una lectura imposible no cuenta) */
    p->pasos_total += pasos;
    if (p->etapa == STAGE_HUEVO) { p->pasos_premio = p->pasos_total; return; }
    if (p->pasos_total - p->pasos_premio < PET_PASOS_PREMIO) return;
    p->pasos_premio = p->pasos_total;
    if (quizas_objeto(p, OFU_PASEO, 0.6f, ahora)) {
        sumar(p, NEED_CURIOSIDAD, 20);
        impulsar(p, EMO_ALEGRIA, 0.6f, CAUSA_EXPLORAR);
        push_event(p, EV_PASEO, -1, 0, ahora);
        return;
    }
    /* fueron de paseo contigo: trae algo de lo que vio (más semillas y piedras) */
    static const uint8_t TRAE[] = {HALL_SEMILLA, HALL_SEMILLA, HALL_PIEDRA, HALL_PIEDRA, HALL_FRUTA, HALL_MIEL, HALL_HIERBA, HALL_RAMITA, HALL_MUSGO};
    int cual = TRAE[rng_next(p) % (sizeof TRAE)];
    static const int8_t A_ITEM[HALL_COUNT] = {ITEM_FRUTA, ITEM_MIEL, ITEM_HIERBA, ITEM_SEMILLA, ITEM_PIEDRA, -1, -1, -1, -1};
    if (A_ITEM[cual] >= 0) guardar_item(p, (item_t)A_ITEM[cual]);
    guardar_material(p, cual);
    sumar(p, NEED_CURIOSIDAD, 20);
    impulsar(p, EMO_ALEGRIA, 0.6f, CAUSA_EXPLORAR);
    push_event(p, EV_PASEO, cual, 0, ahora);
    anotar(p, DIA_REGALO, cual, 0, ahora);
}
bool pet_backup_due(const pet_t *p, uint32_t ahora)
{
    if (pet_mode(p) != MODO_ADULTO || p->etapa == STAGE_HUEVO) return false;
    uint32_t desde = p->ultimo_respaldo ? p->ultimo_respaldo : p->nacimiento;
    return ahora > desde && ahora - desde > 28u * 86400u;
}

void pet_resume(pet_t *p, const pet_t *m, uint32_t ahora)
{
    const uint32_t vivia_hasta = p->ultima_actualizacion, vivia_interaccion = p->ultima_interaccion;
    /* (fw 108) CADA MASCOTA TIENE SU MUNDO. Antes, la que llegaba se quedaba con el mundo de la que se iba a
     * descansar (decoraciones, jardín, semillas, ropa, juguetes, amigos, vecino, viajeras...). Con eso, al
     * estrenar una mascota nueva —que empieza con el mundo vacío— la antigua perdía su casa y sus flores en
     * cuanto volvías a ella. Aquí solo se pasa lo que es del ORBE, no de la mascota: la hora, el hemisferio,
     * el país, la estación forzada y el volumen del ambiente. El clima es el de afuera: el mismo para todas. */
    p->zona_horaria = m->zona_horaria;
    p->hemisferio_sur = m->hemisferio_sur;
    p->estacion_fija = m->estacion_fija;
    /* (los datos del dueño quedan con cada mascota: puede ser de otra persona de la casa) */
    contar_semillas(p);
    p->ambiente_vol = m->ambiente_vol;
    p->vio |= m->vio;   /* lo que YA te explicaron no se vuelve a explicar: eso es del dueño, no de ella */
    if (m->pais) p->pais = m->pais;
    p->clima = m->clima; p->clima_hasta = m->clima_hasta; p->charcos_hasta = m->charcos_hasta;
    for (int i = 0; i < PET_ALARMAS; i++) p->alarma_pospuesta[i] = 0; /* sus alarmas son suyas; lo pospuesto ya pasó */
    /* para ella no pasó el tiempo: estaba descansando */
    p->ultima_actualizacion = ahora;
    p->ultima_interaccion = ahora;
    p->actividad_hasta = ahora;
    p->ultimo_regalo = m->ultimo_regalo;
    for (int f = 0; f < FAUNA_COUNT; f++) p->fauna_hasta[f] = 0;
    p->zorro_roba = 0;
    p->raro = 0xFF;
    p->visita_actual = -1;
    p->invitando_a = p->aceptando_a = p->invitacion_de = 0;
    p->ev_ini = p->ev_fin = 0;
    if (p->juguete_uso < JUG_COUNT && !((p->juguetes_puestos >> p->juguete_uso) & 1u) && p->actividad == ACTV_JUGUETE) p->actividad = ACTV_PASEAR;
    impulsar(p, EMO_ALEGRIA, 0.8f, CAUSA_VOLVISTE);
    if (p->perfil_vive) { p->ultima_actualizacion = vivia_hasta; p->ultima_interaccion = vivia_interaccion; } /* siguió viviendo: pet_update la pone al día */
    pet_seal(p);
}

void pet_new_fresh(pet_t *p, uint8_t especie, uint32_t ahora, uint32_t seed)
{
    /* mascota de verdad nueva: mundo, objetos, jardín y ropa empiezan de cero; solo quedan el id, la hora y el hemisferio */
    uint32_t id = p->id;
    int32_t zona = p->zona_horaria;
    uint8_t hemi = p->hemisferio_sur, pais = p->pais;
    pet_init(p, "", especie, ahora, seed);
    p->id = id ? id : p->id;
    p->hemisferio_sur = hemi;
    p->pais = pais;
    pet_set_timezone(p, zona);
}

void pet_rebirth(pet_t *p, uint8_t especie, uint32_t ahora, uint32_t seed)
{
    /* mascota nueva: se conservan los amigos, el id, la hora, las decoraciones y la edad del dueño */
    uint32_t id = p->id, deco = p->decoraciones;
    planta_t jardin[PET_PARCELAS], jardin_zona[3 * PET_PARCELAS_ZONA];
    memcpy(jardin, p->jardin, sizeof jardin);
    memcpy(jardin_zona, p->jardin_zona, sizeof jardin_zona);
    uint8_t semillas[CULTIVO_COUNT], vistas = p->semillas_vistas;
    memcpy(semillas, p->semillas, sizeof semillas);
    int32_t zona = p->zona_horaria;
    uint8_t edad = p->edad_dueno, hemi = p->hemisferio_sur, dd = p->dueno_dia, dm = p->dueno_mes;
    uint16_t da = p->dueno_anio;
    int16_t cumple = p->cumple_anio;
    vecino_t vecino = p->vecino;
    uint16_t ropa = p->ropa_tiene;
    uint8_t jug = p->juguetes_tiene, jpuestos = p->juguetes_puestos, ocultas = p->decor_ocultas, vol = p->ambiente_vol, vio = p->vio;
    uint8_t jxy[JUG_COUNT][2];
    memcpy(jxy, p->juguete_xy, sizeof jxy);
    amigo_t amigos[PET_MAX_AMIGOS];
    memcpy(amigos, p->amigos, sizeof amigos);
    viajero_t viajeros[PET_VIAJEROS];
    memcpy(viajeros, p->viajeros, sizeof viajeros);
    uint8_t nviajeros = p->viajeros_n, pais = p->pais;
    uint32_t semilla_amigos = p->semilla_amigos;
    pet_init(p, "", especie, ahora, seed);
    p->pais = pais;
    if (semilla_amigos) p->semilla_amigos = semilla_amigos; /* sus amigos del barrio siguen siendo los mismos */
    memcpy(p->viajeros, viajeros, sizeof viajeros); /* las que se fueron de viaje te siguen escribiendo */
    p->viajeros_n = nviajeros;
    p->id = id ? id : p->id;
    p->decoraciones = deco;
    p->hemisferio_sur = hemi;
    memcpy(p->amigos, amigos, sizeof amigos);
    memcpy(p->jardin, jardin, sizeof jardin);
    memcpy(p->jardin_zona, jardin_zona, sizeof jardin_zona);
    for (int c = 0; c < CULTIVO_COUNT; c++) p->semillas[c] = (uint8_t)(semillas[c] > p->semillas[c] ? semillas[c] : p->semillas[c]);
    p->semillas_vistas |= vistas;
    contar_semillas(p);
    if (edad) pet_set_owner_age(p, edad);
    p->dueno_dia = dd; p->dueno_mes = dm; p->dueno_anio = da; p->cumple_anio = cumple;
    p->vecino = vecino; /* su amigo del barrio sigue siendo el mismo */
    p->vecino.presente = 0;
    p->ropa_tiene |= ropa; /* el baúl y los juguetes son tuyos: se quedan */
    p->juguetes_tiene |= jug;
    p->juguetes_puestos = jpuestos;
    memcpy(p->juguete_xy, jxy, sizeof jxy);
    p->decor_ocultas = ocultas;
    p->ambiente_vol = vol;
    p->vio = vio;
    pet_set_timezone(p, zona);
}

/* --- nombres --- */

/* ======================================================================
 * 4c. Fase 4: vínculo, habilidades, recuerdos y sorpresas
 * ==================================================================== */

/* --- diario y álbum --- */
static void anotar(pet_t *p, diario_t tipo, int a, int b, uint32_t t)
{
    if (tipo == DIA_HISTORIA && (unsigned)a < 16) p->lib_nuevo[a >> 3] |= (uint8_t)(1u << (a & 7)); /* (v39) la estrellita del Libro */
    /* sin repetir lo mismo el mismo día (ej.: "vino Pipo" dos veces) */
    int32_t hoy = dia_local(p, t);
    for (int k = 0; k < p->diario_n && k < 10; k++) {
        const entrada_diario_t *e = pet_diary_get(p, k);
        if (dia_local(p, e->t) != hoy) break;
        if (e->tipo == tipo && e->a == a && e->b == b) return;
    }
    int i = (p->diario_ini + p->diario_n) % PET_DIARIO;
    if (p->diario_n == PET_DIARIO) { i = p->diario_ini; p->diario_ini = (uint8_t)((p->diario_ini + 1) % PET_DIARIO); }
    else p->diario_n++;
    entrada_diario_t *e = &p->diario[i];
    e->t = t;
    e->tipo = (uint8_t)tipo;
    e->a = (int8_t)a;
    e->b = (int8_t)b;
}

static void foto(pet_t *p, diario_t motivo, int a, uint32_t t)
{
    int i = (p->album_ini + p->album_n) % PET_ALBUM;
    if (p->album_n == PET_ALBUM) { i = p->album_ini; p->album_ini = (uint8_t)((p->album_ini + 1) % PET_ALBUM); }
    else p->album_n++;
    foto_t *f = &p->album[i];
    memset(f, 0, sizeof *f);
    f->t = t;
    f->motivo = (uint8_t)motivo;
    f->a = (int8_t)a;
    f->escena = p->escena;
    f->hora = (uint8_t)hora_local(p, t);
    f->estacion = (uint8_t)pet_season(p, t);
    f->especie = p->especie;
    f->etapa = p->etapa;
    f->forma = p->forma;
    f->emocion = p->emocion;
    f->vecino_especie = p->vecino.presente ? (uint8_t)((p->vecino.especie & 7) | (p->vecino.color & 7) << 3) : 0xFF;
    push_event(p, EV_FOTO, p->album_n - 1, motivo, t);
}

int pet_diary_count(const pet_t *p) { return p->diario_n; }
const entrada_diario_t *pet_diary_get(const pet_t *p, int i)
{
    if (i < 0 || i >= p->diario_n) return NULL;
    return &p->diario[(p->diario_ini + p->diario_n - 1 - i) % PET_DIARIO];
}
int pet_album_count(const pet_t *p) { return p->album_n; }
const foto_t *pet_album_get(const pet_t *p, int i)
{
    if (i < 0 || i >= p->album_n) return NULL;
    return &p->album[(p->album_ini + p->album_n - 1 - i) % PET_ALBUM];
}

static void vivir(pet_t *p, vivido_t v) { p->vivido |= 1u << v; }

/* --- vínculo por niveles --- */
static const float UMBRAL_VINCULO[VINCULO_COUNT] = {0, 25, 50, 75, 90};

vinculo_nivel_t pet_bond_level(const pet_t *p)
{
    int n = 0;
    for (int i = 1; i < VINCULO_COUNT; i++) if (p->vinculo >= UMBRAL_VINCULO[i]) n = i;
    return (vinculo_nivel_t)n;
}

static void revisar_vinculo(pet_t *p, uint32_t t, bool vivo)
{
    int n = pet_bond_level(p);
    if (n <= p->vinculo_nivel) return;
    p->vinculo_nivel = (uint8_t)n;
    push_event(p, EV_VINCULO, n, 0, t);
    anotar(p, DIA_VINCULO, n, 0, t);
    if (n >= VINCULO_AMIGOS) foto(p, DIA_VINCULO, n, t);
    impulsar(p, EMO_CARINO, 0.8f, CAUSA_CARICIAS);
    if (vivo) sonar(p, SND_SUSPIRO_AMOR, t);
}

/* --- colecciones --- */
int pet_collection_size(coleccion_t c) { return (unsigned)c < COL_COUNT ? TAM_COLECCION[c] : 0; }

const char *pet_piece_name(coleccion_t c, int i)
{
    static const char *const P[COL_COUNT][PET_PIEZAS_MAX] = {
        {"una piedra roja", "una piedra azul", "una piedra verde", "una piedra dorada", "una piedra violeta"},
        {"una concha rosada", "un caracol espiral", "una concha perlada", "una estrella de mar", ""},
        {"una pluma azul", "una pluma roja", "una pluma amarilla", "una pluma blanca", ""},
        {"una hoja de primavera", "una hoja de verano", "una hoja de otoño", "una hoja de invierno", ""},
        {"un cristal celeste", "un cristal violeta", "un cristal de luz", "un trocito de hielo", ""},
    };
    if ((unsigned)c >= COL_COUNT || i < 0 || i >= TAM_COLECCION[c]) return "?";
    return P[c][i];
}

/* guarda una pieza; devuelve true si era nueva */
static bool guardar_pieza(pet_t *p, coleccion_t c, int pieza, uint32_t t, bool vivo)
{
    if ((p->coleccion[c] >> pieza) & 1u) return false;
    p->coleccion[c] |= (uint8_t)(1u << pieza);
    anotar(p, DIA_TESORO, c, pieza, t);
    vivir(p, VIV_TESORO);
    impulsar(p, EMO_ORGULLO, 0.7f, CAUSA_TESORO);
    impulsar(p, EMO_ALEGRIA, 0.6f, CAUSA_TESORO);
    sumar(p, NEED_CURIOSIDAD, 15);
    /* (fw 104) la colección de cristales se completa con los 3 de la cueva: el trocito de hielo es un extra que viene y se va */
    const uint8_t llena = c == COL_CRISTALES ? (uint8_t)((1u << CRISTALES_AZAR) - 1u) : (uint8_t)((1u << TAM_COLECCION[c]) - 1u);
    if ((p->coleccion[c] & llena) == llena && !((p->colecciones_completas >> c) & 1u)) {
        p->colecciones_completas |= (uint8_t)(1u << c);
        push_event(p, EV_COLECCION, c, 0, t);
        anotar(p, DIA_COLECCION, c, 0, t);
        foto(p, DIA_COLECCION, c, t);
        p->estampitas += 3;
        for (int k = 0; k < 3; k++) guardar_item(p, ITEM_SEMILLA); /* un premio para el jardín */
        guardar_item(p, ITEM_PIEDRA);
        if (vivo) sonar(p, SND_FIESTA, t);
    }
    return true;
}

/* qué colección se esconde en cada zona; la hoja depende de la estación */
static void desenterrar(pet_t *p, uint32_t t, bool vivo)
{
    p->tesoro_pendiente = 0;
    coleccion_t c;
    switch (p->escena) {
    case ESC_LAGO: case ESC_ISLA: c = COL_CONCHAS; break;
    case ESC_CUEVA: c = COL_CRISTALES; break;
    case ESC_COLINA: c = COL_PIEDRAS; break;
    case ESC_BOSQUE: c = chance(p, 0.5f) ? COL_PLUMAS : COL_HOJAS; break;
    default: c = chance(p, 0.5f) ? COL_HOJAS : COL_PIEDRAS; break;
    }
    int pieza;
    if (c == COL_HOJAS) {
        /* la hoja de cada estación aparece cuando el clima se parece a esa estación (más seguido si es la de verdad):
         * la de invierno, con frío o nieve; la de otoño, con viento, nubes o lluvia; la de primavera, con lluvia o sol suave; la de verano, con sol o tormenta */
        const clima_t cl = pet_weather(p);
        const estacion_t es = pet_season(p, t);
        const bool parece[ESTACION_COUNT] = {
            [ESTACION_PRIMAVERA] = cl == CLIMA_LLUVIA || cl == CLIMA_DESPEJADO,
            [ESTACION_VERANO] = cl == CLIMA_DESPEJADO || cl == CLIMA_TORMENTA,
            [ESTACION_OTONO] = cl == CLIMA_NUBLADO || cl == CLIMA_VIENTO || cl == CLIMA_LLUVIA,
            [ESTACION_INVIERNO] = cl == CLIMA_FRIO || cl == CLIMA_NIEVE};
        float w[ESTACION_COUNT], tot = 0;
        for (int i = 0; i < ESTACION_COUNT; i++) {
            w[i] = parece[i] ? 1.0f : 0.0f;
            if (i == (int)es) w[i] *= 3.0f;
            if (!((p->coleccion[COL_HOJAS] >> i) & 1u)) w[i] *= 2.0f; /* la que le falta */
            tot += w[i];
        }
        pieza = (int)es;
        if (tot > 0) {
            float r = rng01(p) * tot;
            for (int i = 0; i < ESTACION_COUNT; i++) { if (r < w[i]) { pieza = i; break; } r -= w[i]; }
        }
    } else {
        int faltan[PET_PIEZAS_MAX], n = 0;
        const int tope = c == COL_CRISTALES ? CRISTALES_AZAR : TAM_COLECCION[c]; /* (fw 104) el hielo no sale escarbando */
        for (int i = 0; i < tope; i++) if (!((p->coleccion[c] >> i) & 1u)) faltan[n++] = i;
        pieza = n && chance(p, 0.75f) ? faltan[rng_next(p) % (uint32_t)n] : (int)(rng_next(p) % (uint32_t)tope);
    }
    bool nueva = guardar_pieza(p, c, pieza, t, vivo);
    if (!nueva) { guardar_item(p, ITEM_PIEDRA); impulsar(p, EMO_ALEGRIA, 0.3f, CAUSA_HALLAZGO); } /* repetida: una piedra brillante */
    push_event(p, EV_TESORO, 1, nueva ? (int)c * 8 + pieza : -1, t);
    if (vivo) sonar(p, SND_TESORO, t);
}

/* olfateando, a veces huele un tesoro: lo dice y va a cavar ahí */
static bool oler_tesoro(pet_t *p, uint32_t t, bool vivo)
{
    int32_t hoy = dia_local(p, t);
    if (p->dia_tesoros != hoy) { p->dia_tesoros = hoy; p->tesoros_dia = 0; }
    if (p->tesoros_dia >= 3 || !chance(p, 0.35f + 0.3f * p->rasgo[TRAIT_CURIOSA])) return false;
    p->tesoros_dia++;
    p->tesoro_pendiente = 1;
    push_event(p, EV_TESORO, 0, -1, t);
    if (vivo) sonar(p, SND_HUELE, t);
    impulsar(p, EMO_CURIOSIDAD, 0.8f, CAUSA_TESORO);
    return true;
}

/* --- habilidades: nadar y trepar, se aprenden practicando --- */
static bool puede_trepar(const pet_t *p) { return p->escena != ESC_CUEVA && !ESC_ES_SECRETA(p->escena); } /* (en la cima no hay árboles; los del claro quedan lejos) */

static int practicas_hoy(pet_t *p, habilidad_t h, uint32_t t)
{
    int32_t hoy = dia_local(p, t);
    if (p->hab_dia != hoy) { p->hab_dia = hoy; memset(p->hab_hoy, 0, sizeof p->hab_hoy); }
    return p->hab_hoy[h];
}

static void fin_habilidad(pet_t *p, habilidad_t h, uint32_t t, bool vivo)
{
    float v = p->habilidad[h];
    if (practicas_hoy(p, h, t) < 255) p->hab_hoy[h]++;
    float gana = (2.5f + 3.5f * p->rasgo[TRAIT_ENERGICA]) * (1.0f - v / 125.0f); /* cuesta más a medida que mejora */
    if (h == HAB_NADAR && p->gusto_escena[ESC_LAGO] > 30) gana *= 1.5f; /* a las que aman el agua les cuesta menos */
    if (h == HAB_TREPAR && p->escena == ESC_BOSQUE) gana *= 1.2f;
    if (p->modo_nino) gana *= 1.3f;
    float nuevo = clampf(v + gana, 0, 100);
    p->habilidad[h] = nuevo;
    vivir(p, h == HAB_NADAR ? VIV_NADAR : VIV_TREPAR);
    sumar(p, NEED_DIVERSION, 10);
    sumar(p, NEED_ENERGIA, -6);
    int hito = 0;
    if (h == HAB_TREPAR && v < 30 && chance(p, 0.5f)) { /* todavía se cae */
        hito = -1;
        impulsar(p, EMO_MIEDO, 0.2f, CAUSA_HABILIDAD);
        if (vivo) sonar(p, SND_CAIDA, t);
    } else if (v < 40 && nuevo >= 40) hito = 2;
    else if (v < 85 && nuevo >= 85) hito = 3;
    if (hito >= 2 && p->hab_hito[h] < hito) {
        p->hab_hito[h] = (uint8_t)hito;
        impulsar(p, EMO_ORGULLO, 0.9f, CAUSA_HABILIDAD);
        anotar(p, DIA_HABILIDAD, h, hito, t);
        foto(p, DIA_HABILIDAD, h * 10 + hito, t);
        if (vivo) sonar(p, SND_TRUCO, t);
    } else if (hito >= 2) hito = 0;
    if (hito != 0) push_event(p, EV_HABILIDAD, h, hito, t);
    /* las expertas encuentran cosas: conchas al bucear, plumas en los nidos */
    if (h == HAB_NADAR && nuevo >= 85 && chance(p, 0.15f)) {
        int pieza = (int)(rng_next(p) % TAM_COLECCION[COL_CONCHAS]);
        if (guardar_pieza(p, COL_CONCHAS, pieza, t, vivo)) push_event(p, EV_TESORO, 1, COL_CONCHAS * 8 + pieza, t);
    }
    if (h == HAB_TREPAR && nuevo >= 40) {
        sumar(p, NEED_CURIOSIDAD, 20); /* desde arriba se ve lejos */
        if (nuevo >= 85 && chance(p, 0.15f)) { /* en lo alto hay nidos con plumas */
            int pieza = (int)(rng_next(p) % TAM_COLECCION[COL_PLUMAS]);
            if (guardar_pieza(p, COL_PLUMAS, pieza, t, vivo)) push_event(p, EV_TESORO, 1, COL_PLUMAS * 8 + pieza, t);
        }
    }
}

static void empezar_habilidad(pet_t *p, habilidad_t h, uint32_t t, bool vivo)
{
    if (p->habilidad[h] == 0 && p->hab_hito[h] == 0) {
        p->hab_hito[h] = 1;
        push_event(p, EV_HABILIDAD, h, 1, t); /* primera vez */
    }
    if (vivo && h == HAB_NADAR) sonar(p, SND_CHAPUZON, t);
}

/* --- cosas raras y bonitas --- */
static void empezar_raro(pet_t *p, raro_t r, uint32_t t, uint32_t dur, bool vivo)
{
    p->raro = (uint8_t)r;
    p->raro_hasta = t + dur;
    p->ultimo_raro = t;
    push_event(p, EV_RARO, r, 1, t);
    anotar(p, DIA_RARO, r, 0, t);
    vivir(p, VIV_ARCOIRIS);
    if (!p->dormida) {
        impulsar(p, EMO_ALEGRIA, 0.7f, CAUSA_RARO);
        impulsar(p, EMO_CURIOSIDAD, 0.6f, CAUSA_RARO);
        sumar(p, NEED_CURIOSIDAD, 25);
    }
    foto(p, DIA_RARO, r, t);
    if (vivo) sonar(p, SND_RARO, t);
}

static bool lluvia_de_estrellas(const pet_t *p, uint32_t t)
{
    int m, d;
    pet_local_date(p, t, NULL, &m, &d);
    return (m == 8 && (d == 12 || d == 13)) || (m == 12 && (d == 13 || d == 14)); /* Perseidas y Gemínidas */
}

static void revisar_raros(pet_t *p, uint32_t t, float dt, bool vivo)
{
    if (p->raro != 0xFF) {
        if (t >= p->raro_hasta) { push_event(p, EV_RARO, p->raro, 0, t); p->raro = 0xFF; }
        return;
    }
    const bool lentes = t < p->lentes_hasta; /* con los lentes reveladores se ven más secretos */
    if (t - p->ultimo_raro < (lentes ? 4u : 30u) * 3600u) return; /* que sigan siendo raros */
    int h = hora_local(p, t);
    clima_t c = pet_weather(p);
    bool afuera = p->escena != ESC_CUEVA;
    if (!afuera) return;
    if (h >= 20 && h < 24 && c == CLIMA_DESPEJADO && chance(p, prob_paso((lluvia_de_estrellas(p, t) ? 0.5f : 0.03f) * (lentes ? 4.0f : 1.0f), dt)))
        empezar_raro(p, RARO_ESTRELLA_FUGAZ, t, 120, vivo);
    else if (!p->dormida && ((h >= 6 && h < 9) || (h >= 18 && h < 20)) && p->escena != ESC_LAGO && chance(p, prob_paso(0.012f * (lentes ? 4.0f : 1.0f), dt)))
        empezar_raro(p, RARO_CIERVO, t, 45, vivo); /* cruza una sola vez, despacio */
    else if (!p->dormida && h >= 10 && h < 17 && c == CLIMA_DESPEJADO && chance(p, prob_paso(0.015f * (lentes ? 4.0f : 1.0f), dt)))
        empezar_raro(p, RARO_GLOBO, t, 360, vivo);
}

/* --- metas suaves --- */
static const uint8_t OBJETIVO_META[META_COUNT] = {3, 1, 2, 3, 5, 1, 1, 1};
int pet_goal_target(meta_t m) { return (unsigned)m < META_COUNT ? OBJETIVO_META[m] : 1; }

static bool meta_posible(const pet_t *p, meta_t m)
{
    int abiertas = 0;
    for (int e = 0; e < ESC_COUNT; e++) abiertas += pet_scene_open(p, (escena_t)e);
    bool plantas = false;
    for (int i = 0; i < PET_PARCELAS_TOTAL; i++) plantas |= pet_plant(p, i)->tipo != PLANTA_NADA;
    switch (m) {
    case META_PASEO: return abiertas > 1;
    case META_PESCAR: return pet_scene_open(p, ESC_LAGO);
    case META_REGAR: return plantas;
    case META_FRUTA: return p->inventario[ITEM_FRUTA] > 0;
    default: return true;
    }
}

static void revisar_meta(pet_t *p, uint32_t t)
{
    int32_t hoy = dia_local(p, t);
    if (p->meta_dia == hoy || p->dormida || hora_local(p, t) < 8 || p->etapa == STAGE_HUEVO) return;
    meta_t opciones[META_COUNT];
    int n = 0;
    for (int m = 0; m < META_COUNT; m++)
        if (meta_posible(p, (meta_t)m) && (m != p->meta || p->meta_dia == 0)) opciones[n++] = (meta_t)m;
    if (!n) return;
    meta_t m = opciones[rng_next(p) % (uint32_t)n];
    p->meta = (uint8_t)m;
    p->meta_objetivo = OBJETIVO_META[m];
    p->meta_progreso = 0;
    p->meta_hecha = 0;
    p->meta_dia = hoy;
    push_event(p, EV_META, m, 0, t);
}

static void meta_avanza(pet_t *p, meta_t m, uint32_t t)
{
    if (p->meta != m || p->meta_hecha || p->meta_dia != dia_local(p, t)) return;
    if (++p->meta_progreso < p->meta_objetivo) { push_event(p, EV_META, m, 2, t); return; }
    p->meta_hecha = 1;
    p->estampitas++;
    impulsar(p, EMO_ALEGRIA, 0.7f, CAUSA_META);
    impulsar(p, EMO_ORGULLO, 0.5f, CAUSA_META);
    subir_vinculo(p, 2.0f);
    anotar(p, DIA_META, m, 0, t);
    push_event(p, EV_META, m, 1, t);
    sonar(p, SND_META, t);
}

/* --- su cumpleaños: la primera semana, cada mes y cada año --- */
static int meses_de(const pet_t *p, uint32_t t, bool *es_hoy)
{
    int an, mn, dn, a, m, d;
    pet_local_date(p, p->nacimiento, &an, &mn, &dn);
    pet_local_date(p, t, &a, &m, &d);
    int meses = (a - an) * 12 + (m - mn) - (d < dn ? 1 : 0);
    int dia_cumple = dn > dias_mes(m, a) ? dias_mes(m, a) : dn; /* nacida un 31: el último día del mes */
    *es_hoy = d == dia_cumple && meses >= 1;
    return meses;
}

bool pet_her_birthday(const pet_t *p, uint32_t t)
{
    if (p->etapa == STAGE_HUEVO) return false;
    bool hoy;
    meses_de(p, t, &hoy);
    return hoy || dia_local(p, t) - dia_local(p, p->nacimiento) == 7;
}

static void revisar_su_cumple(pet_t *p, uint32_t t, bool vivo)
{
    if (p->etapa == STAGE_HUEVO || p->dormida || hora_local(p, t) < 8) return;
    int dias = dia_local(p, t) - dia_local(p, p->nacimiento);
    bool hoy;
    int meses = meses_de(p, t, &hoy);
    int a = 0, b = 0;
    uint16_t marca;
    if (hoy) { marca = (uint16_t)(100 + meses); a = meses % 12 == 0 ? meses / 12 : meses; b = meses % 12 == 0; }
    else if (dias == 7) { marca = 1; a = 7; }
    else return;
    if (p->su_cumple_visto >= marca) return;
    p->su_cumple_visto = marca;
    sumar(p, NEED_DIVERSION, 20);
    sumar(p, NEED_CARINO, 10);
    impulsar(p, EMO_ALEGRIA, 0.95f, CAUSA_SU_CUMPLE);
    impulsar(p, EMO_ORGULLO, 0.6f, CAUSA_SU_CUMPLE);
    guardar_item(p, ITEM_MIEL); /* un dulce de cumpleaños */
    vivir(p, VIV_CUMPLE);
    push_event(p, EV_SU_CUMPLE, a > 127 ? 127 : a, b, t);
    anotar(p, DIA_SU_CUMPLE, a > 127 ? 127 : a, b, t);
    foto(p, DIA_SU_CUMPLE, a > 127 ? 127 : a, t);
    if (vivo) sonar(p, SND_FIESTA, t);
}

/* --- fechas especiales: celebraciones configurables --- */
static int64_t dias_civil(int y, int m, int d) /* días desde 1970-01-01 */
{
    y -= m <= 2;
    int64_t era = (y >= 0 ? y : y - 399) / 400;
    int64_t yoe = y - era * 400;
    int64_t doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    int64_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

static const celebracion_t CEL_DEF[] = {
    {CEL_FIJA, 12, 31, 0, 2, FECHA_ANIO_NUEVO, {0}, "Año Nuevo", "¡FELIZ AÑO NUEVO!"},
    {CEL_FIJA, 2, 14, 0, 1, FECHA_SAN_VALENTIN, {0}, "San Valentín", "¡FELIZ DÍA DEL AMOR!"},
    {CEL_PRIMAVERA, 0, 0, 0, 1, FECHA_PRIMAVERA, {0}, "el primer día de primavera", "¡LLEGÓ LA PRIMAVERA!"},
    {CEL_FIJA, 9, 18, 0, 2, FECHA_FIESTAS_PATRIAS, {0}, "Fiestas Patrias", "¡FELICES FIESTAS PATRIAS!"},
    {CEL_FIJA, 10, 31, 0, 1, FECHA_HALLOWEEN, {0}, "Halloween", "¡FELIZ HALLOWEEN! ¿DULCE O TRUCO?"},
    {CEL_FIJA, 12, 24, 0, 2, FECHA_NAVIDAD, {0}, "Navidad", "¡FELIZ NAVIDAD!"},
};
static celebracion_t s_cel[PET_CELEBRACIONES];
static int s_cel_n = -1; /* -1: todavía las de fábrica */

static void cel_listas(void)
{
    if (s_cel_n >= 0) return;
    s_cel_n = (int)(sizeof CEL_DEF / sizeof CEL_DEF[0]);
    memcpy(s_cel, CEL_DEF, sizeof CEL_DEF);
}
void pet_celebrations_default(void) { s_cel_n = -1; cel_listas(); }
int pet_celebration_count(void) { cel_listas(); return s_cel_n; }
const celebracion_t *pet_celebration_get(int i)
{
    cel_listas();
    return i >= 0 && i < s_cel_n ? &s_cel[i] : NULL;
}

static bool texto_ok(const char *t, int n)
{
    for (int i = 0; i < n; i++) if (!t[i]) return true;
    return false; /* sin cero final */
}
static bool cel_valida(const celebracion_t *c)
{
    if (c->regla >= CEL_REGLAS || c->deco == FECHA_NINGUNA || c->deco >= FECHA_COUNT || c->dias < 1 || c->dias > 7) return false;
    if (!texto_ok(c->nombre, sizeof c->nombre) || !texto_ok(c->saludo, sizeof c->saludo) || !c->nombre[0]) return false;
    if (c->regla == CEL_FIJA) return c->mes >= 1 && c->mes <= 12 && c->dia >= 1 && c->dia <= dias_mes(c->mes, 2024);
    if (c->regla == CEL_NESIMO) return c->mes >= 1 && c->mes <= 12 && c->dia >= 1 && c->dia <= 5 && c->semana <= 6;
    return true;
}
bool pet_celebrations_set(const celebracion_t *c, int n)
{
    if (n < 0 || n > PET_CELEBRACIONES || (n && !c)) return false;
    for (int i = 0; i < n; i++) if (!cel_valida(&c[i])) return false;
    if (n) memcpy(s_cel, c, (size_t)n * sizeof *c);
    s_cel_n = n;
    return true;
}

/* primer día de la celebración en el año a (en días desde 1970) */
static int64_t cel_inicio(const pet_t *p, const celebracion_t *c, int a)
{
    switch (c->regla) {
    case CEL_FIJA: {
        int d = c->dia > dias_mes(c->mes, a) ? dias_mes(c->mes, a) : c->dia; /* 29/2 en años no bisiestos: el 28 */
        return dias_civil(a, c->mes, d);
    }
    case CEL_NESIMO: {
        int64_t uno = dias_civil(a, c->mes, 1);
        int ds = (int)((uno % 7 + 7 + 4) % 7); /* 1/1/1970 fue jueves */
        int d = 1 + (c->semana - ds + 7) % 7;
        if (c->dia >= 5) { while (d + 7 <= dias_mes(c->mes, a)) d += 7; }
        else d += 7 * (c->dia - 1);
        return dias_civil(a, c->mes, d);
    }
    default: return p->hemisferio_sur ? dias_civil(a, 9, 21) : dias_civil(a, 3, 21); /* el Día de la Primavera, como se celebra en Chile */
    }
}

int pet_celebration_today(const pet_t *p, uint32_t t)
{
    cel_listas();
    int a;
    pet_local_date(p, t, &a, NULL, NULL);
    int64_t hoy = dia_local(p, t);
    for (int i = 0; i < s_cel_n; i++)
        for (int y = a - 1; y <= a; y++) { /* una que empezó el año pasado puede seguir (31/12 y 1/1) */
            if (s_cel[i].deco == FECHA_FIESTAS_PATRIAS && p->pais != PAIS_CHILE) continue; /* por ahora, solo en Chile */
            int64_t ini = cel_inicio(p, &s_cel[i], y);
            if (hoy >= ini && hoy < ini + s_cel[i].dias) return i;
        }
    return -1;
}

fecha_t pet_special_date(const pet_t *p, uint32_t t)
{
    int i = pet_celebration_today(p, t);
    return i < 0 ? FECHA_NINGUNA : (fecha_t)s_cel[i].deco;
}

int pet_celebrations_pack(uint8_t *out, int cap)
{
    cel_listas();
    int n = 10 + s_cel_n * (int)sizeof(celebracion_t);
    if (!out || cap < n) return -1;
    memcpy(out, "FIES", 4);
    out[4] = 1;
    out[5] = (uint8_t)s_cel_n;
    memcpy(out + 10, s_cel, (size_t)s_cel_n * sizeof(celebracion_t));
    uint32_t c = crc32(out + 10, n - 10);
    memcpy(out + 6, &c, 4);
    return n;
}
bool pet_celebrations_unpack(const uint8_t *in, int n)
{
    if (!in || n < 10 || memcmp(in, "FIES", 4) || in[4] != 1) return false;
    int k = in[5];
    if (k > PET_CELEBRACIONES || n != 10 + k * (int)sizeof(celebracion_t)) return false;
    uint32_t c;
    memcpy(&c, in + 6, 4);
    if (c != crc32(in + 10, n - 10)) return false;
    celebracion_t t[PET_CELEBRACIONES];
    memcpy(t, in + 10, (size_t)k * sizeof(celebracion_t));
    return pet_celebrations_set(t, k);
}
const char *pet_rule_name(cel_regla_t r)
{
    static const char *const T[] = {"fija", "nesimo", "primavera"};
    return NOMBRE(T, r, CEL_REGLAS);
}

static void revisar_fecha(pet_t *p, uint32_t t, bool vivo)
{
    int ci = pet_celebration_today(p, t);
    fecha_t f = ci < 0 ? FECHA_NINGUNA : (fecha_t)s_cel[ci].deco;
    int32_t hoy = dia_local(p, t);
    if (f == FECHA_NINGUNA || p->dia_fecha == hoy || p->dormida || hora_local(p, t) < 8 || p->etapa == STAGE_HUEVO) return;
    p->dia_fecha = hoy;
    if (f == FECHA_NAVIDAD) p->vio |= 8u; /* para la bufanda */
    sumar(p, NEED_DIVERSION, 15);
    impulsar(p, EMO_ALEGRIA, 0.8f, CAUSA_FECHA);
    push_event(p, EV_FECHA, f, ci, t);
    anotar(p, DIA_FECHA, f, ci, t);
    foto(p, DIA_FECHA, f, t);
    if (vivo) sonar(p, SND_FIESTA, t);
}

/* todo lo de la fase 4 que se revisa en cada paso */

/* ======================================================================
 * 4d. Fase 5: creatividad (ropa, juguetes, aspecto, ambiente, desbloqueos)
 * ==================================================================== */

static uint32_t mezclar32(uint32_t x) /* un dado fijo por mascota (no gasta el azar de la partida) */
{
    x ^= x >> 16; x *= 0x7FEB352Du; x ^= x >> 15; x *= 0x846CA68Bu; x ^= x >> 16;
    return x;
}
static float dado_fijo(const pet_t *p, uint32_t k) { return (float)(mezclar32(p->id * 2654435761u + k * 40503u) >> 8) / 16777216.0f; }

/* Gustos de ropa y juguetes: salen de su personalidad y de su especie, con algo de azar propio */
static void gustos_creativos(pet_t *p)
{
    const float *R = p->rasgo;
    const memoria_t *m = &p->memoria;
    float r[ROPA_VIEJAS] = {
        10 + 50 * R[TRAIT_PULCRA] - 30 * R[TRAIT_TRAVIESA],                         /* moño */
        20 + 30 * R[TRAIT_ENERGICA],                                                /* pañuelo */
        (m->clima[CLIMA_FRIO] < 0 ? 40.0f : -5.0f) - 20 * R[TRAIT_ENERGICA],        /* gorro de lana */
        10 + 40 * R[TRAIT_CARINOSA] + (m->clima[CLIMA_FRIO] < 0 ? 30.0f : 0.0f),    /* bufanda */
        30 * R[TRAIT_CURIOSA] - 20 * R[TRAIT_TRAVIESA] + (m->clima[CLIMA_DESPEJADO] < 40 ? 25.0f : 0.0f), /* sombrero */
        -10 + 60 * R[TRAIT_TRAVIESA] - 30 * R[TRAIT_TIMIDA],                         /* lentes */
        10 + 40 * R[TRAIT_PULCRA] + (p->especie == 0 ? 30.0f : 0.0f),                /* corona de flores */
        20 * R[TRAIT_CURIOSA] + (p->gusto_escena[ESC_LAGO] > 40 ? 40.0f : 0.0f),     /* collar de conchas */
    };
    for (int i = 0; i < ROPA_VIEJAS; i++) p->gusto_ropa[i] = i8(r[i] + (dado_fijo(p, 100u + (uint32_t)i) - 0.5f) * 80.0f);
    float j[JUG_COUNT] = {
        10 + 50 * R[TRAIT_CARINOSA] + 30 * R[TRAIT_TIMIDA] - 20 * R[TRAIT_ENERGICA], /* peluche */
        -10 + 70 * R[TRAIT_ENERGICA],                                                /* pelotón */
        50 * R[TRAIT_CURIOSA] + 30 * R[TRAIT_TRAVIESA],                              /* caja */
        40 * R[TRAIT_ENERGICA] + 30 * R[TRAIT_CURIOSA] - 40 * R[TRAIT_TIMIDA],       /* columpio */
        -20 + 60 * R[TRAIT_TRAVIESA] + 30 * R[TRAIT_ENERGICA] - 30 * R[TRAIT_TIMIDA], /* tambor */
        40 * R[TRAIT_CURIOSA] + (m->clima[CLIMA_VIENTO] > 0 ? 30.0f : 0.0f),          /* molinete */
    };
    for (int i = 0; i < JUG_COUNT; i++) p->gusto_juguete[i] = i8(j[i] + (dado_fijo(p, 200u + (uint32_t)i) - 0.5f) * 80.0f);
}

/* --- aspecto --- */
int pet_look(const pet_t *p) { return (p->especie & 7) | ((p->mezcla & 15) << 3) | ((p->color & 7) << 7); }

bool pet_choose_look(pet_t *p, int especie, int mezcla, int color, uint32_t ahora, uint32_t seed)
{
    if (p->etapa != STAGE_HUEVO || (unsigned)especie >= N_ESPECIES) return false;
    char nombre[PET_NAME_LEN];
    memcpy(nombre, p->nombre, sizeof nombre);
    pet_rebirth(p, (uint8_t)especie, ahora, seed);
    memcpy(p->nombre, nombre, sizeof nombre);
    p->color = (uint8_t)((unsigned)color % PET_COLORES);
    p->mezcla = 0;
    if (mezcla >= 0 && (unsigned)mezcla < N_ESPECIES && mezcla != especie) {
        p->mezcla = (uint8_t)(mezcla + 1);
        /* mitad y mitad: el clima y las zonas que le gustan */
        memoria_t *m = &p->memoria;
        for (int c = 0; c < CLIMA_COUNT; c++) m->clima[c] = i8(((float)m->clima[c] + (float)CLIMA_ESPECIE[mezcla][c]) * 0.5f);
        for (int e = 0; e < ESC_GUSTOS; e++) p->gusto_escena[e] = i8(((float)p->gusto_escena[e] + (float)ESCENA_ESPECIE[mezcla][e]) * 0.5f);
        if (m->agrado[LUGAR_ESPECIE[mezcla]] < 0.2f) m->agrado[LUGAR_ESPECIE[mezcla]] = 0.2f;
        gustos_creativos(p);
    }
    p->aspecto_ok = 1;
    return true;
}

/* --- ropa --- */
static const uint8_t RANURA_ROPA[ROPA_COUNT] = {RANURA_CABEZA, RANURA_CUELLO, RANURA_CABEZA, RANURA_CUELLO,
                                                 RANURA_CABEZA, RANURA_CARA, RANURA_CABEZA, RANURA_CUELLO,
                                                 RANURA_CABEZA, RANURA_MANO, RANURA_MANO, RANURA_MANO};
static void poner_en(pet_t *p, int s, int v) { if (s == RANURA_MANO) p->ropa_mano = (uint8_t)v; else if ((unsigned)s < RANURAS_VIEJAS) p->ropa_puesta[s] = (uint8_t)v; }
ranura_t pet_clothes_slot(ropa_t r) { return (unsigned)r < ROPA_COUNT ? (ranura_t)RANURA_ROPA[r] : RANURA_CABEZA; }
bool pet_has_clothes(const pet_t *p, ropa_t r) { return (unsigned)r < ROPA_COUNT && ((p->ropa_tiene >> r) & 1u); }
int pet_wearing(const pet_t *p, ranura_t s) { return pet_worn_raw(p, (int)s) ? pet_worn_raw(p, (int)s) - 1 : -1; }
static bool lleva(const pet_t *p, ropa_t r) { return pet_worn_raw(p, RANURA_ROPA[r]) == (int)r + 1; }
static bool ropa_abriga(const pet_t *p) { return lleva(p, ROPA_GORRO_LANA) || lleva(p, ROPA_BUFANDA); }

/* el gusto por lo que se agregó después (nenúfar, paraguas...): sale de su personalidad y de su azar propio, sin guardarse */
static int gusto_nuevo(const pet_t *p, ropa_t r)
{
    const float *R = p->rasgo;
    const memoria_t *m = &p->memoria;
    float v = 0;
    switch (r) {
    case ROPA_NENUFAR: v = 15 + 40 * R[TRAIT_CURIOSA] + (p->gusto_escena[ESC_LAGO] > 40 ? 30.0f : 0.0f); break;
    case ROPA_PARAGUAS: v = 10 + 30 * R[TRAIT_PULCRA] + (m->clima[CLIMA_LLUVIA] < 0 ? 30.0f : 0.0f); break;
    case ROPA_MOLINILLO: v = 20 + 40 * R[TRAIT_ENERGICA] + 20 * R[TRAIT_TRAVIESA]; break;
    case ROPA_FAROLITO: v = 10 + 40 * R[TRAIT_TIMIDA] + 20 * R[TRAIT_CURIOSA]; break;
    default: break;
    }
    return i8(v + (dado_fijo(p, 100u + (uint32_t)r) - 0.5f) * 80.0f);
}
int pet_clothes_liking(const pet_t *p, ropa_t r)
{
    if ((unsigned)r >= ROPA_COUNT) return 0;
    int g = (r < ROPA_VIEJAS ? p->gusto_ropa[r] : gusto_nuevo(p, r)) + (int)(p->vinculo / 5.0f); /* si te quiere, se deja más */
    if (p->modo_nino == MODO_INFANTE && g < 5) g = 5;
    return g > 100 ? 100 : (g < -100 ? -100 : g);
}

static void quitar_ropa(pet_t *p, ropa_t r, int motivo, uint32_t t)
{
    poner_en(p, RANURA_ROPA[r], 0);
    push_event(p, EV_ROPA, r, motivo, t);
}

static int vestir(pet_t *p, int arg, uint32_t t)
{
    if (arg < 0) { /* quitársela */
        int s = -arg - 1;
        if (s >= RANURA_COUNT || !pet_worn_raw(p, s)) return 0;
        ropa_t r = (ropa_t)(pet_worn_raw(p, s) - 1);
        quitar_ropa(p, r, 4, t);
        if (!p->dormida && pet_clothes_liking(p, r) < -20) impulsar(p, EMO_ALEGRIA, 0.3f, CAUSA_ROPA); /* ¡alivio! */
        sonar(p, SND_ROPA, t);
        return 1;
    }
    if (!pet_has_clothes(p, (ropa_t)arg)) return 0;
    ropa_t r = (ropa_t)arg;
    poner_en(p, RANURA_ROPA[r], r + 1);
    p->ropa_revisada = t;
    sonar(p, SND_ROPA, t);
    if (p->dormida) { push_event(p, EV_ROPA, r, 0, t); return 1; } /* dormida ni se entera */
    int g = pet_clothes_liking(p, r);
    vivir(p, VIV_ROPA);
    if ((r == ROPA_GORRO_LANA || r == ROPA_BUFANDA) && hace_calor(p, t)) {
        impulsar(p, EMO_ENOJO, 0.2f, CAUSA_CALOR);
        quitar_ropa(p, r, -3, t);
        return 1;
    }
    if (g >= 40) {
        impulsar(p, EMO_ALEGRIA, 0.7f, CAUSA_ROPA);
        impulsar(p, EMO_ORGULLO, 0.5f, CAUSA_ROPA);
        sumar(p, NEED_DIVERSION, 8);
        if (!((p->ropa_amada >> r) & 1u)) {
            p->ropa_amada |= (uint16_t)(1u << r);
            anotar(p, DIA_ROPA, r, 0, t);
            foto(p, DIA_ROPA, r, t);
        }
        push_event(p, EV_ROPA, r, 2, t);
        sonar(p, SND_ALEGRE, t);
    } else if (g >= 10) {
        impulsar(p, EMO_ALEGRIA, 0.4f, CAUSA_ROPA);
        push_event(p, EV_ROPA, r, 1, t);
    } else if (g > -20 || p->modo_nino == MODO_INFANTE) {
        push_event(p, EV_ROPA, r, 0, t);
    } else {
        impulsar(p, EMO_ENOJO, 0.3f, CAUSA_ROPA);
        float se_saca = clampf(0.75f - p->vinculo / 150.0f, 0.1f, 0.75f) * (p->modo_nino ? 0.4f : 1.0f);
        if (chance(p, se_saca)) quitar_ropa(p, r, -2, t);
        else push_event(p, EV_ROPA, r, -1, t);
        sonar(p, SND_QUEJA, t);
    }
    return 1;
}

/* mientras la lleva: lo que le encanta la pone orgullosa; lo que no, a veces se lo saca */
static void paso_ropa(pet_t *p, uint32_t t, float h, bool vivo)
{
    if (p->dormida || p->etapa == STAGE_HUEVO) return;
    bool alguna = false;
    for (int s = 0; s < RANURA_COUNT; s++) {
        if (!pet_worn_raw(p, s)) continue;
        alguna = true;
        ropa_t r = (ropa_t)(pet_worn_raw(p, s) - 1);
        int g = pet_clothes_liking(p, r);
        if (g >= 40) sumar(p, NEED_DIVERSION, 0.6f * h);
        { /* lo de la mano sirve de algo: el paraguas con lluvia, el molinillo con viento, el farolito de noche */
            const clima_t cl = pet_weather(p);
            if (r == ROPA_PARAGUAS && (cl == CLIMA_LLUVIA || cl == CLIMA_TORMENTA)) { sumar(p, NEED_DIVERSION, 1.2f * h); sumar(p, NEED_SEGURIDAD, 0.6f * h); }
            if (r == ROPA_MOLINILLO && cl == CLIMA_VIENTO) sumar(p, NEED_DIVERSION, 1.5f * h);
            if (r == ROPA_FAROLITO && es_noche(p, t)) sumar(p, NEED_SEGURIDAD, 1.0f * h);
        }
        if (t - p->ropa_revisada < 1800u) continue;
        if ((r == ROPA_GORRO_LANA || r == ROPA_BUFANDA) && hace_calor(p, t)) { quitar_ropa(p, r, -3, t); continue; }
        if (g < -20 && p->modo_nino != MODO_INFANTE && chance(p, p->modo_nino ? 0.1f : 0.3f)) {
            quitar_ropa(p, r, -2, t);
            if (vivo) sonar(p, SND_ROPA, t);
        }
    }
    if (alguna && t - p->ropa_revisada >= 1800u) p->ropa_revisada = t;
}

/* --- juguetes --- */
bool pet_has_toy(const pet_t *p, juguete_t j) { return (unsigned)j < JUG_COUNT && ((p->juguetes_tiene >> j) & 1u); }
bool pet_toy_placed(const pet_t *p, juguete_t j, int *x, int *y)
{
    if ((unsigned)j >= JUG_COUNT || !((p->juguetes_puestos >> j) & 1u)) return false;
    if (x) *x = p->juguete_xy[j][0] * 2;
    if (y) *y = p->juguete_xy[j][1] * 2;
    return true;
}
int pet_toy_liking(const pet_t *p, juguete_t j)
{
    if ((unsigned)j >= JUG_COUNT) return 0;
    int g = p->gusto_juguete[j];
    if (p->modo_nino == MODO_INFANTE && g < 10) g = 10;
    return g;
}
static int puestos(const pet_t *p)
{
    int n = 0;
    for (int j = 0; j < JUG_COUNT; j++) n += (p->juguetes_puestos >> j) & 1u;
    return n;
}

static void empezar_juguete(pet_t *p, juguete_t j, uint32_t t, uint32_t dur, bool vivo)
{
    p->actividad = ACTV_JUGUETE;
    p->juguete_uso = (uint8_t)j;
    p->lugar = LUGAR_CLARO;
    p->actividad_hasta = t + dur;
    push_event(p, EV_ACTIVIDAD, ACTV_JUGUETE, p->lugar, t);
    if (vivo) {
        static const sound_t S[JUG_COUNT] = {SND_PELUCHE, SND_REBOTE, SND_RISA, SND_COLUMPIO, SND_TAMBOR, SND_RISA};
        sonar(p, S[j], t);
    }
}

static void fin_juguete(pet_t *p, uint32_t t, bool vivo)
{
    juguete_t j = (juguete_t)(p->juguete_uso % JUG_COUNT);
    int g = pet_toy_liking(p, j);
    sumar(p, NEED_DIVERSION, 10.0f + (float)g / 10.0f);
    if (j == JUG_PELOTON || j == JUG_COLUMPIO || j == JUG_TAMBOR) sumar(p, NEED_ENERGIA, -4);
    if (j == JUG_PELUCHE) { sumar(p, NEED_SEGURIDAD, 8); sumar(p, NEED_CARINO, 3); }
    if (j == JUG_CAJA) sumar(p, NEED_CURIOSIDAD, 8);
    impulsar(p, EMO_ALEGRIA, clampf(0.3f + (float)g / 200.0f, 0.1f, 0.8f), CAUSA_JUGUETE);
    vivir(p, VIV_JUGUETE);
    if (p->gusto_juguete[j] < 100) p->gusto_juguete[j] = i8((float)p->gusto_juguete[j] + 2.0f); /* lo que usa, le gusta más */
    if (!((p->juguetes_usados >> j) & 1u)) {
        p->juguetes_usados |= (uint8_t)(1u << j);
        anotar(p, DIA_ESTRENO, j, 0, t);
        foto(p, DIA_ESTRENO, j, t);
        push_event(p, EV_JUGUETE, j, 1, t);
    } else if (vivo) push_event(p, EV_JUGUETE, j, 0, t);
}

/* ganas de jugar sola con algún juguete del prado (devuelve el mejor o -1) */
static int juguete_preferido(pet_t *p, float *u)
{
    if (p->escena != ESC_PRADO || !p->juguetes_puestos) return -1;
    int mejor = -1;
    float mu = -1e9f;
    for (int j = 0; j < JUG_COUNT; j++) {
        if (!((p->juguetes_puestos >> j) & 1u)) continue;
        float v = (float)pet_toy_liking(p, (juguete_t)j) / 250.0f + rng01(p) * 0.15f;
        if (!((p->juguetes_usados >> j) & 1u)) v += 0.3f; /* algo nuevo: hay que probarlo */
        if (j == JUG_MOLINETE && pet_weather(p) == CLIMA_VIENTO) v += 0.25f;
        if (v > mu) { mu = v; mejor = j; }
    }
    *u = 0.2f + mu + (p->necesidad[NEED_DIVERSION] < 70 ? 0.15f : 0.0f);
    return mejor;
}

static int poner_juguete(pet_t *p, int arg, uint32_t t)
{
    juguete_t j = (juguete_t)(arg & 0xFF);
    int x2 = (arg >> 8) & 0xFF, y2 = (arg >> 16) & 0xFF;
    if (!pet_has_toy(p, j)) return 0;
    if (x2 == 0xFF) {
        if (!((p->juguetes_puestos >> j) & 1u)) return 0;
        p->juguetes_puestos &= (uint8_t)~(1u << j);
        if (p->actividad == ACTV_JUGUETE && p->juguete_uso == j) p->actividad_hasta = t;
        p->juguete_hasta[j] = 0;
        push_event(p, EV_JUGUETE, j, 4, t);
        return 1;
    }
    if (!((p->juguetes_puestos >> j) & 1u) && puestos(p) >= PET_JUGUETES_PUESTOS) return 0;
    p->juguetes_puestos |= (uint8_t)(1u << j);
    p->juguete_hasta[j] = t + PET_JUGUETE_AFUERA_S;
    p->juguete_xy[j][0] = (uint8_t)x2;
    p->juguete_xy[j][1] = (uint8_t)y2;
    push_event(p, EV_JUGUETE, j, 3, t);
    if (!p->dormida) {
        impulsar(p, EMO_CURIOSIDAD, 0.5f, CAUSA_NOVEDAD);
        sumar(p, NEED_CURIOSIDAD, 6);
        /* algo nuevo en el prado: va a ver qué es */
        if (p->escena == ESC_PRADO && !((p->juguetes_usados >> j) & 1u) && p->actividad != ACTV_PESCAR && !p->vecino.presente)
            empezar_juguete(p, j, t, 120u, true);
    }
    return 1;
}

static int usar_juguete(pet_t *p, int arg, uint32_t t)
{
    juguete_t j = (juguete_t)arg;
    if (!pet_toy_placed(p, j, NULL, NULL) || p->escena != ESC_PRADO) return 0;
    if (p->dormida) return 0;
    registrar_habito(p, HAB_JUGAR, t);
    int g = pet_toy_liking(p, j);
    if (g < -30 && !p->modo_nino && chance(p, 0.7f)) {
        push_event(p, EV_JUGUETE, j, -1, t);
        sonar(p, SND_QUEJA, t);
        return 0;
    }
    subir_vinculo(p, 0.5f);
    empezar_juguete(p, j, t, 150u + rng_next(p) % 120u, true);
    push_event(p, EV_JUGUETE, j, 2, t);
    return 1;
}

/* --- lo que aparece en el baúl: sin tienda, por cosas que pasan --- */
enum { R_INICIAL, R_ESTAMPITAS, R_FRIO, R_CALOR, R_VIENTO, R_NAVIDAD, R_FLORES, R_CONCHAS, R_VINCULO, R_SU_CUMPLE, R_LLUVIA, R_LAGO };
typedef struct { uint8_t juguete, id, regla, n, alt_estampitas; } desbloqueo_t;
static const desbloqueo_t DESBLOQUEO[] = {
    {0, ROPA_MONO, R_INICIAL, 0, 0},
    {0, ROPA_PANUELO, R_INICIAL, 0, 0},
    {0, ROPA_GORRO_LANA, R_FRIO, 0, 3},
    {0, ROPA_SOMBRERO, R_CALOR, 0, 4},
    {0, ROPA_BUFANDA, R_NAVIDAD, 0, 6},
    {0, ROPA_LENTES, R_ESTAMPITAS, 8, 0},
    {0, ROPA_CORONA, R_FLORES, 3, 10},
    {0, ROPA_COLLAR, R_CONCHAS, 0, 12},
    {0, ROPA_PARAGUAS, R_LLUVIA, 0, 5},
    {0, ROPA_MOLINILLO, R_VIENTO, 0, 7},
    {0, ROPA_NENUFAR, R_LAGO, 0, 9},
    {0, ROPA_FAROLITO, R_ESTAMPITAS, 10, 0},
    {1, JUG_PELUCHE, R_INICIAL, 0, 0},
    {1, JUG_PELOTON, R_ESTAMPITAS, 2, 0},
    {1, JUG_CAJA, R_VINCULO, VINCULO_AMIGOS, 0},
    {1, JUG_COLUMPIO, R_ESTAMPITAS, 5, 0},
    {1, JUG_TAMBOR, R_SU_CUMPLE, 0, 9},
    {1, JUG_MOLINETE, R_VIENTO, 0, 7},
};

static bool cumple_regla(const pet_t *p, const desbloqueo_t *d)
{
    if (d->alt_estampitas && p->estampitas >= d->alt_estampitas) return true;
    switch (d->regla) {
    case R_INICIAL: return true;
    case R_ESTAMPITAS: return p->estampitas >= d->n;
    case R_FRIO: return (p->vio & 1u) != 0;
    case R_CALOR: return (p->vio & 2u) != 0;
    case R_VIENTO: return (p->vio & 4u) != 0;
    case R_NAVIDAD: return (p->vio & 8u) != 0;
    case R_FLORES: return p->flores_hechas >= d->n;
    case R_CONCHAS: return ((p->colecciones_completas >> COL_CONCHAS) & 1u) != 0;
    case R_VINCULO: return pet_bond_level(p) >= d->n;
    case R_SU_CUMPLE: return p->su_cumple_visto >= 1;
    case R_LLUVIA: return (p->vio & 16u) != 0;
    case R_LAGO: return ((p->escenas_vistas >> ESC_LAGO) & 1u) != 0;
    default: return false;
    }
}

static void revisar_baul(pet_t *p, uint32_t t, bool vivo)
{
    if (p->etapa == STAGE_HUEVO) return;
    /* lo que vivió y abre cosas nuevas */
    clima_t c = pet_weather(p);
    if (c == CLIMA_FRIO || c == CLIMA_NIEVE) p->vio |= 1u;
    if (c == CLIMA_LLUVIA || c == CLIMA_TORMENTA) p->vio |= 16u;
    if (hace_calor(p, t)) p->vio |= 2u;
    if (c == CLIMA_VIENTO) p->vio |= 4u;
    for (unsigned i = 0; i < sizeof DESBLOQUEO / sizeof DESBLOQUEO[0]; i++) {
        const desbloqueo_t *d = &DESBLOQUEO[i];
        bool tiene = d->juguete ? pet_has_toy(p, (juguete_t)d->id) : pet_has_clothes(p, (ropa_t)d->id);
        if (tiene || !cumple_regla(p, d)) continue;
        if (d->juguete) p->juguetes_tiene |= (uint8_t)(1u << d->id);
        else p->ropa_tiene |= (uint16_t)(1u << d->id);
        if (d->regla == R_INICIAL) continue;
        push_event(p, EV_NUEVO, d->juguete, d->id, t);
        anotar(p, DIA_NUEVO, d->juguete, d->id, t);
        if (vivo) sonar(p, SND_NUEVO, t);
        return; /* de a una: cada cosa nueva se disfruta */
    }
}

/* --- sonido ambiente --- */
ambiente_t pet_ambient(const pet_t *p, uint32_t t)
{
    clima_t c = pet_weather(p);
    int h = hora_local(p, t);
    bool noche = h >= 21 || h < 6;
    if (p->escena == ESC_CUEVA) return AMB_CUEVA;
    if (c == CLIMA_TORMENTA) return AMB_TORMENTA;
    if (c == CLIMA_LLUVIA) return AMB_LLUVIA;
    if (c == CLIMA_VIENTO) return AMB_VIENTO;
    if (noche) return pet_season(p, t) == ESTACION_INVIERNO || c == CLIMA_FRIO || c == CLIMA_NIEVE ? AMB_NADA : AMB_NOCHE;
    if (p->escena == ESC_LAGO) return AMB_LAGO;
    if (p->escena == ESC_ISLA) return AMB_MAR;
    if (p->escena == ESC_BOSQUE) return AMB_BOSQUE;
    if (c == CLIMA_NIEVE || c == CLIMA_FRIO) return AMB_VIENTO;
    return AMB_PRADO;
}

/* --- decoraciones que se guardan y se vuelven a poner --- */
void pet_toggle_decor(pet_t *p, decor_t d)
{
    if ((unsigned)d >= DECOR_COUNT || !pet_has_decor(p, d)) return;
    p->decor_ocultas ^= (uint8_t)(1u << d);
}
bool pet_decor_visible(const pet_t *p, decor_t d) { return pet_has_decor(p, d) && !((p->decor_ocultas >> d) & 1u); }
uint32_t pet_decor_mask(const pet_t *p) { return p->decoraciones & ~(uint32_t)p->decor_ocultas; }

/* --- nombres --- */
const char *pet_clothes_name(ropa_t r)
{
    static const char *const T[] = {"moño de pétalos", "pañuelo de hojas", "gorro de estrellas", "bufanda de seda de araña",
                                    "sombrero con flor", "lentes de cristal de luna", "corona de flores de luz", "collar con perla",
                                    "nenúfar con florcita", "paraguas de hoja", "molinillo de viento", "farolito de papel"};
    return NOMBRE(T, r, ROPA_COUNT);
}
const char *pet_toy_name(juguete_t j)
{
    static const char *const T[] = {"peluche", "pelotón", "caja de cartón", "columpio", "tambor", "molinete"};
    return NOMBRE(T, j, JUG_COUNT);
}
const char *pet_crop_name(cultivo_t c)
{
    static const char *const T[] = {"flor", "girasol", "frutilla", "hierba medicinal"};
    return NOMBRE(T, c, CULTIVO_COUNT);
}
const char *pet_ambient_name(ambiente_t a)
{
    static const char *const T[] = {"silencio", "prado", "bosque", "lago", "cueva", "grillos", "lluvia", "tormenta", "viento", "mar"};
    return NOMBRE(T, a, AMB_COUNT);
}
const char *pet_color_name(int c)
{
    static const char *const T[] = {"el de su especie", "rosado", "celeste", "menta", "lila", "durazno", "dorado", "noche"};
    return NOMBRE(T, c, PET_COLORES);
}

/* sus estaciones: qué le gusta de cada una sale de su personalidad (rasgos) y de lo que aprendió a disfrutar */
static float gusto_estacion(const pet_t *p, int e)
{
    const float *R = p->rasgo;
    float v = 0;
    static const uint8_t TIP[4][4] = {{ACTV_PERSEGUIR, ACTV_AVES, ACTV_EXPLORAR, ACTV_BUSCAR}, {ACTV_NADAR, ACTV_CHAPOTEAR, ACTV_TREPAR, ACTV_PELOTA},
                                      {ACTV_OLFATEAR, ACTV_BUSCAR, ACTV_CAVAR, ACTV_PASEAR}, {ACTV_NIEVE, ACTV_PROBAR, ACTV_CHARLAR, ACTV_CANTAR}};
    switch (e & 3) {
    case 0: v = R[TRAIT_CURIOSA] + 0.6f * R[TRAIT_TRAVIESA] + 0.3f * R[TRAIT_ENERGICA]; break;
    case 1: v = R[TRAIT_ENERGICA] + 0.5f * (1.0f - R[TRAIT_TIMIDA]) + 0.4f * R[TRAIT_PULCRA]; break;
    case 2: v = R[TRAIT_GLOTONA] + 0.4f * R[TRAIT_TIMIDA] + 0.3f * R[TRAIT_TRAVIESA]; break;
    default: v = R[TRAIT_CARINOSA] + 0.4f * R[TRAIT_TIMIDA] + 0.2f * R[TRAIT_GLOTONA]; break;
    }
    if (PET_APRENDE) for (int k = 0; k < 4; k++) v += (float)p->gusto[TIP[e & 3][k]] / 60.0f * 0.25f;
    return v;
}
int pet_season_like(const pet_t *p, int e) { return (int)(gusto_estacion(p, e) * 100.0f); }
int pet_season_fav(const pet_t *p, bool peor)
{
    int mejor = -1; float vm = peor ? 1e9f : -1e9f, vo = peor ? -1e9f : 1e9f;
    for (int e = 0; e < 4; e++) {
        const float v = gusto_estacion(p, e);
        if (peor ? v < vm : v > vm) { vm = v; mejor = e; }
        if (peor ? v > vo : v < vo) vo = v;
    }
    if (mejor < 0 || !((p->est_mask >> mejor) & 1u) || p->etapa == STAGE_HUEVO) return -1; /* solo si ya la vivió */
    return fabsf(vm - vo) >= 0.25f ? mejor : -1; /* y si de verdad destaca */
}
/* de lo que sí aprende gustos: lo urgente o de descanso (dormir, esperarte, refugiarse…) no cuenta */
static bool aprende_de(int a)
{
    return a != ACTV_SIESTA && a != ACTV_ESPERAR && a != ACTV_REFUGIO && a != ACTV_ABRIGARSE && a != ACTV_SOMBRA && a != ACTV_JUNTOS && a != ACTV_PESCAR;
}
int pet_taste(const pet_t *p, int a) { return PET_APRENDE && (unsigned)a < 32u ? p->gusto[a] : 0; }
const char *pet_activity_verb(int a)
{
    static const char *const V[ACTV_COUNT] = {
        [ACTV_PASEAR] = "pasear", [ACTV_OLFATEAR] = "olfatear", [ACTV_CAVAR] = "cavar", [ACTV_EXPLORAR] = "explorar", [ACTV_MIRAR] = "mirar el cielo",
        [ACTV_JUGAR_SOLA] = "jugar sola", [ACTV_NIEVE] = "jugar en la nieve", [ACTV_PERSEGUIR] = "perseguir mariposas", [ACTV_NADAR] = "nadar",
        [ACTV_TREPAR] = "trepar a los árboles", [ACTV_JUGUETE] = "jugar con mis juguetes", [ACTV_CHAPOTEAR] = "chapotear", [ACTV_PELOTA] = "jugar con la pelota",
        [ACTV_PROBAR] = "probarme cosas", [ACTV_BUSCAR] = "buscar cosas ricas", [ACTV_AVES] = "mirar a los pajaritos", [ACTV_CHARLAR] = "charlar con los animales",
        [ACTV_CANTAR] = "cantar", [ACTV_BAILAR] = "bailar"};
    return (unsigned)a < ACTV_COUNT && V[a] ? V[a] : "";
}
int pet_taste_low(const pet_t *p) /* de la que más se aburrió, o -1 */
{
    int peor = -1, v = -15;
    if (!PET_APRENDE) return -1;
    for (int i = 0; i < ACTV_COUNT; i++) if (aprende_de(i) && p->gusto[i] < v) { v = p->gusto[i]; peor = i; }
    return peor;
}
int pet_taste_top(const pet_t *p)
{
    int mejor = -1, v = 19;
    if (!PET_APRENDE) return -1;
    for (int i = 0; i < ACTV_COUNT; i++) if (aprende_de(i) && p->gusto[i] > v) { v = p->gusto[i]; mejor = i; }
    return mejor;
}

/* --- mini eventos de temporada: lluvia de estrellas en sus fechas, la primera lluvia y la primera nieve, noches de luciérnagas, equinoccios y solsticios --- */
static void paso_mini(pet_t *p, uint32_t t, bool vivo)
{
    if (!vivo || p->etapa == STAGE_HUEVO || p->dormida || p->escena == ESC_CUEVA) return;
    int y = 0, m = 0, d = 0;
    pet_local_date(p, t, &y, &m, &d);
    const int hs = pet_pet_hour(p, t);
    const uint16_t dia = (uint16_t)(1 + (dia_local(p, t) & 0x7FFF));
    const estacion_t est = pet_season(p, t);
    const uint8_t ciclo = (uint8_t)((y * 4 + (int)est) & 0xFF);
    if (p->mini_ciclo != ciclo) { p->mini_ciclo = ciclo; p->mini_flags = 0; }
    const clima_t c = (clima_t)p->clima;
    int kind = -1, sub = 0;
    /* lluvia de estrellas: cuatro noches de cielo despejado al año (Eta Acuáridas, Delta Acuáridas, Perseidas y Gemínidas) */
    const bool lluvia_est = (m == 5 && d >= 4 && d <= 7) || (m == 7 && d >= 28) || (m == 8 && d >= 11 && d <= 14) || (m == 12 && d >= 13 && d <= 15);
    if (lluvia_est && hs >= 20 && hs <= 23 && c == CLIMA_DESPEJADO && p->mini_dia[MINI_ESTRELLAS] != dia) kind = MINI_ESTRELLAS;
    else if (c == CLIMA_LLUVIA && est == ESTACION_OTONO && !(p->mini_flags & 1u)) { kind = MINI_PRIMERA_LLUVIA; p->mini_flags |= 1u; }
    else if (c == CLIMA_NIEVE && est == ESTACION_INVIERNO && !(p->mini_flags & 2u)) { kind = MINI_PRIMERA_NIEVE; p->mini_flags |= 2u; }
    else if ((est == ESTACION_PRIMAVERA || est == ESTACION_VERANO) && hs >= 20 && hs <= 22 && (c == CLIMA_DESPEJADO || c == CLIMA_NUBLADO) &&
             ((uint32_t)dia * 2654435761u >> 8) % 5u == 0 && p->mini_dia[MINI_LUCIERNAGAS] != dia) kind = MINI_LUCIERNAGAS;
    else if (hs >= 7 && hs <= 10 && c != CLIMA_TORMENTA && p->mini_dia[MINI_SOL] != dia &&
             ((m == 3 && d == 20) || (m == 6 && d == 21) || (m == 9 && d == 22) || (m == 12 && d == 21))) {
        kind = MINI_SOL;
        sub = m == 3 || m == 9 ? 0 : ((m == 6) == (p->hemisferio_sur != 0) ? 1 : 2); /* 0 equinoccio, 1 el día más corto, 2 el día más largo */
    }
    if (kind < 0) { /* luna llena: una noche de despejado por mes (la fase sale de la fecha real, con un día de margen) */
        const double ciclos = ((double)t - 947182440.0) / 86400.0 / 29.530588853;
        const double fase = ciclos - floor(ciclos);
        if (fase > 0.47 && fase < 0.53 && hs >= 20 && hs <= 23 && c == CLIMA_DESPEJADO && (!p->mini_dia[MINI_LUNA] || (uint16_t)(dia - p->mini_dia[MINI_LUNA]) > 20u)) kind = MINI_LUNA;
    }
    if (kind < 0) { /* eclipses de verdad vistos desde Chile (fechas de timeanddate.com y Meteored); con cielo despejado y a la hora real */
        const int hr = hora_local(p, t);
        if (((y == 2026 && m == 8 && ((d == 27 && hr >= 22) || (d == 28 && hr <= 3))) && c == CLIMA_DESPEJADO) && !p->mini_dia[MINI_ECLIPSE_LUNAR]) kind = MINI_ECLIPSE_LUNAR; /* lunar parcial del 28 de agosto de 2026 */
        else if (y == 2027 && m == 2 && d == 6 && hr >= 11 && hr <= 15 && (c == CLIMA_DESPEJADO || c == CLIMA_NUBLADO) && !p->mini_dia[MINI_ECLIPSE_SOLAR]) kind = MINI_ECLIPSE_SOLAR; /* solar anular del 6 de febrero de 2027 (parcial en casi todo Chile) */
    }
    if (kind < 0) return;
    p->mini_dia[kind] = dia;
    impulsar(p, EMO_CURIOSIDAD, 0.9f, CAUSA_NOVEDAD);
    sumar(p, NEED_CURIOSIDAD, 15);
    sumar(p, NEED_DIVERSION, 10);
    p->actividad = ACTV_MIRAR;
    p->actividad_hasta = t + 150u; /* se queda mirando un rato */
    push_event(p, EV_MINI, kind, sub, t);
    anotar(p, DIA_MINI, kind, sub, t);
    if (kind == MINI_ESTRELLAS) foto(p, DIA_MINI, kind, t);
}

static void paso_recuerdos(pet_t *p, uint32_t t, float dt, bool vivo)
{
    paso_mini(p, t, vivo);
    paso_melodia(p, t, dt, vivo);
    if (p->etapa != STAGE_HUEVO) { /* notó que cambió la estación: si es su favorita (o la que menos le gusta), lo dice */
        const int es = (int)pet_season(p, t) & 3;
        if (p->est_ult != (uint8_t)(es + 1)) {
            const bool primera_vez = p->est_ult == 0;
            p->est_ult = (uint8_t)(es + 1);
            p->est_mask |= (uint8_t)(1u << es);
            if (!primera_vez && vivo) {
                const int kind = pet_season_fav(p, false) == es ? 1 : pet_season_fav(p, true) == es ? 2 : 0;
                if (kind == 1) { impulsar(p, EMO_ALEGRIA, 0.7f, CAUSA_NOVEDAD); sumar(p, NEED_DIVERSION, 10); }
                push_event(p, EV_ESTACION, es, kind, t);
                if (kind) anotar(p, DIA_ESTACION, es, kind, t);
            }
        }
    }
    /* si ya tiene su canción y hace rato que no suena (entre 1 día y 2 semanas: después se resigna), a la tarde te la pide, como mucho cada 2 días */
    if (vivo && PET_APRENDE && p->escucha_musica && p->cancion_n >= 3 && !p->dormida && p->etapa != STAGE_HUEVO && !p->enferma && hora_local(p, t) >= 16 && hora_local(p, t) < 21 &&
        t - p->cancion_t > 24u * 3600u && t - p->cancion_t < 14u * 24u * 3600u && t - p->cancion_pide > 48u * 3600u && chance(p, prob_paso(0.5f, dt))) {
        p->cancion_pide = t;
        push_event(p, EV_PIDE_CANCION, 0, p->cancion_en, t);
    }
    for (int i = 0; i < 32; i++) /* con el tiempo lo aprendido se va suavizando (unas semanas) */
        if (p->gusto[i] && chance(p, prob_paso(0.03f, dt))) p->gusto[i] = (int8_t)(p->gusto[i] + (p->gusto[i] > 0 ? -1 : 1));
    revisar_vinculo(p, t, vivo);
    revisar_raros(p, t, dt, vivo);
    revisar_meta(p, t);
    revisar_su_cumple(p, t, vivo);
    revisar_fecha(p, t, vivo);
    revisar_baul(p, t, vivo);
    paso_ropa(p, t, dt / HORA, vivo);
    paso_objetos(p, t);
    paso_niebla(p, t, vivo);
}

/* --- textos --- */
const char *pet_skill_name(habilidad_t h)
{
    static const char *const T[] = {"nadar", "trepar"};
    return NOMBRE(T, h, HAB_COUNT);
}
const char *pet_collection_name(coleccion_t c)
{
    static const char *const T[] = {"piedras de colores", "conchas de la laguna", "plumas", "hojas de cada estación", "cristales de la cueva"};
    return NOMBRE(T, c, COL_COUNT);
}
const char *pet_rare_name(raro_t r)
{
    static const char *const T[] = {"un arcoíris", "una estrella fugaz", "un pudú blanco", "Nimba, la ballena de nubes"};
    return NOMBRE(T, r, RARO_COUNT);
}
const char *pet_goal_name(meta_t m)
{
    static const char *const T[] = {"tirarle la pelota 3 veces", "darle una fruta", "regar 2 plantas", "practicar 3 trucos",
                                    "hacerle 5 mimos", "ir a pasear a otra zona", "salir a pescar", "darle un baño"};
    return NOMBRE(T, m, META_COUNT);
}
const char *pet_date_name(fecha_t f)
{
    static const char *const T[] = {"", "Halloween", "Navidad", "Año Nuevo", "San Valentín", "el primer día de primavera", "Fiestas Patrias"};
    return NOMBRE(T, f, FECHA_COUNT);
}
const char *pet_bond_name(vinculo_nivel_t v)
{
    static const char *const T[] = {"desconfía", "te conoce", "amigos", "mejores amigos", "inseparables"};
    return NOMBRE(T, v, VINCULO_COUNT);
}

const char *pet_diary_text(const pet_t *p, const entrada_diario_t *e, char *buf, int n)
{
    (void)p; /* hoy todo sale de la entrada; queda por si algún texto necesita a la mascota */
    static const char *const HALL[] = {"una fruta", "miel", "una hierba", "una semilla", "una piedra brillante", "una ramita",
                                       "una chapita", "una hoja seca"};
    static const char *const ETAPA[] = {"", "bebé", "cría", "grande"};
    buf[0] = 0;
    if (!e) return buf;
    int a = e->a, b = e->b;
    /* para que una misma entrada se lea siempre igual, la frase se elige con sus propios datos */
    const unsigned var_ = (((unsigned)(e->t / 60u) + (unsigned)(a & 255) * 7u + (unsigned)(b & 255) * 13u) * 2654435761u) >> 16;
#define VARIANTE(arr) ((arr)[var_ % (sizeof (arr) / sizeof (arr)[0])])
    switch (e->tipo) {
    case DIA_NACIO: snprintf(buf, (size_t)n, "Nací. ¡Hola, mundo!"); break;
    case DIA_CRECIO: snprintf(buf, (size_t)n, a == STAGE_ADULTA ? "¡Crecí y evolucioné!" : "Crecí: ya soy %s", ETAPA[a & 3]); break;
    case DIA_ZONA: { /* (v40) con su artículo: «al prado», «a la laguna», «el cerro», «la cueva» */
        static const char *const Z[] = {"Fui por primera vez %s", "Hoy descubrí el camino %s", "Me animé a ir %s", "Conocí %s, ¡qué lugar!", "Salimos a explorar y llegamos %s", "Hoy el paseo fue hasta %s", "Nunca había estado en %s", "Así es %s: ¡me gustó!"};
        static const uint8_t AL[] = {1, 1, 1, 0, 1, 0, 0, 0};
        const int vi = var_ % 8;
        snprintf(buf, (size_t)n, Z[vi], AL[vi] ? pet_scene_al((escena_t)a) : pet_scene_el((escena_t)a)); break; }
    case DIA_TRUCO: { static const char *const Z[] = {"Aprendí a %s", "¡Ya sé %s!", "Practiqué mucho y ahora sé %s", "Me salió %s, ¡por fin!", "Hoy aprendí un truco nuevo: %s", "Lo logré: %s", "Mira lo que aprendí: %s", "Tanto practicar... ¡y ya sé %s!"}; snprintf(buf, (size_t)n, VARIANTE(Z), pet_trick_name((truco_t)a)); break; }
    case DIA_HABILIDAD:
        if (a == HAB_NADAR) snprintf(buf, (size_t)n, b >= 3 ? "Ya nado como un pez" : "¡Aprendí a nadar!");
        else snprintf(buf, (size_t)n, b >= 3 ? "Trepo hasta la copa más alta" : "¡Aprendí a trepar árboles!");
        break;
    case DIA_VECINO: { static const char *const Z[] = {"Vino %s a jugar", "%s vino a visitarme", "Hoy jugué con %s", "Me visitó %s, ¡qué alegría!", "Estuvimos jugando con %s", "Llegó %s y la pasamos bien", "%s pasó a saludar", "Con %s nos reímos mucho"}; snprintf(buf, (size_t)n, VARIANTE(Z), (int)(uint8_t)e->a < pet_friends_max(p) ? pet_friend_name(p, (uint8_t)e->a) : "un amigo"); break; }
    case DIA_PESCA: { static const char *const Z[] = {"Pesqué %s", "Hoy pesqué %s", "Algo picó: ¡%s!", "Esperé un buen rato y pesqué %s", "Saqué del agua %s", "Qué buena pesca: %s", "Pesqué %s, ¡qué alegría!", "La caña se movió y salió %s"}; snprintf(buf, (size_t)n, VARIANTE(Z), pet_catch_name((pesca_t)a)); break; }
    case DIA_TESORO: { static const char *const Z[] = {"Encontré un tesoro: %s", "Hoy encontré algo brillante: %s", "¡Mira lo que encontré! %s", "Escarbando apareció un tesoro: %s", "Un hallazgo para mi colección: %s", "Descubrí un tesoro escondido: %s", "Tropecé con algo especial: %s", "Qué suerte la de hoy: %s"}; snprintf(buf, (size_t)n, VARIANTE(Z), pet_piece_name((coleccion_t)a, b)); break; }
    case DIA_COLECCION: snprintf(buf, (size_t)n, "¡Completé mi colección de %s!", pet_collection_name((coleccion_t)a)); break;
    case DIA_RARO: { static const char *const Z[] = {"Vi %s", "Hoy vi %s, ¡qué suerte!", "No lo podía creer: vi %s", "Apareció %s y me quedé quietita", "Vi %s, ¡algo muy raro!", "Qué día especial: vi %s"}; snprintf(buf, (size_t)n, VARIANTE(Z), pet_rare_name((raro_t)a)); break; }
    case DIA_META: snprintf(buf, (size_t)n, "Cumplimos la meta: %s", pet_goal_name((meta_t)a)); break;
    case DIA_CUMPLE_DUENO: snprintf(buf, (size_t)n, "Celebramos tu cumpleaños"); break;
    case DIA_SU_CUMPLE:
        if (a == 7 && !b) snprintf(buf, (size_t)n, "Cumplí una semana");
        else if (b) snprintf(buf, (size_t)n, a == 1 ? "¡Cumplí un año!" : "¡Cumplí %d años!", a);
        else snprintf(buf, (size_t)n, a == 1 ? "Cumplí un mes" : "Cumplí %d meses", a);
        break;
    case DIA_FECHA: {
        const celebracion_t *c = pet_celebration_get(e->b);
        snprintf(buf, (size_t)n, "Celebramos %s", c && c->deco == a ? c->nombre : pet_date_name((fecha_t)a));
        break;
    }
    case DIA_VINCULO:
        if (a == VINCULO_TE_CONOCE) snprintf(buf, (size_t)n, "Ya te conozco un poco");
        else snprintf(buf, (size_t)n, "Ahora somos %s", pet_bond_name((vinculo_nivel_t)a));
        break;
    case DIA_REGALO: { static const char *const Z[] = {"Te traje %s", "Encontré %s y pensé en ti", "Esto es para ti: %s", "Te guardé %s", "Mira, te traje %s", "Fui a pasear y te traje %s", "Un regalito para ti: %s", "Pensé en ti y encontré %s"}; snprintf(buf, (size_t)n, VARIANTE(Z), (unsigned)a < 8 ? HALL[a] : "algo"); break; }
    case DIA_AMIGO: snprintf(buf, (size_t)n, "Jugué con un amigo de otro orbe"); break;
    case DIA_NUEVO: snprintf(buf, (size_t)n, "Algo nuevo en el baúl: %s", e->a ? pet_toy_name((juguete_t)e->b) : pet_clothes_name((ropa_t)e->b)); break;
    case DIA_ESTRENO: snprintf(buf, (size_t)n, "Estrené mi %s", pet_toy_name((juguete_t)a)); break;
    case DIA_ROPA: snprintf(buf, (size_t)n, "¡Me encanta mi %s!", pet_clothes_name((ropa_t)a)); break;
    case DIA_SEMILLA: snprintf(buf, (size_t)n, "Encontré semillas de %s", pet_crop_name((cultivo_t)a)); break;
    case DIA_REGALO_VUELTA: snprintf(buf, (size_t)n, "Te esperaba con %s", pet_gift_name((regalo_t)a)); break;
    case DIA_OBJETO: {
        const objeto_info_t *o = objeto_info((a & 0x7F) | (b & 1) << 7);
        snprintf(buf, (size_t)n, "Encontré algo mágico: %s", o ? o->nombre : "?");
        break;
    }
    case DIA_BALLENA: snprintf(buf, (size_t)n, a ? "Paseé en Nimba, la ballena de nubes, entre las estrellas y traje polvo de estrellas" : "Paseé en Nimba, la ballena de nubes, entre las estrellas"); break;
    case DIA_GUARDIANA: snprintf(buf, (size_t)n, "Con la luna llena llevé el Corazón del Mundo al círculo de piedras. Ahora soy {la} {Guardiana} del Bosque"); break;
    case DIA_RITUAL: snprintf(buf, (size_t)n, a ? "Ya tenemos nuestras buenas noches" : "Ya tenemos nuestros buenos días"); break;
    case DIA_SEMANA: {
        int mimos = (b >> 3) & 15, cosas = b & 7;
        char m[24], c[48];
        if (mimos >= 15) snprintf(m, sizeof m, "muchísimos mimos");
        else if (mimos) snprintf(m, sizeof m, "%d mimo%s", mimos, mimos == 1 ? "" : "s");
        else snprintf(m, sizeof m, "pocos mimos");
        if (cosas) snprintf(c, sizeof c, " y encontré %d cosa%s mágica%s", cosas, cosas == 1 ? "" : "s", cosas == 1 ? "" : "s");
        else c[0] = 0;
        if (a) snprintf(buf, (size_t)n, "Mi semana: jugamos %d ve%s, %s%s", a, a == 1 ? "z" : "ces", m, c);
        else snprintf(buf, (size_t)n, "Mi semana: %s%s", m, c);
        break;
    }
    case DIA_FAMILIA: snprintf(buf, (size_t)n, a ? "Mi huevo ya está en buenas manos" : "Dejé un huevo: va a ser de mi familia"); break;
    case DIA_BAILE: snprintf(buf, (size_t)n, a ? "Bailamos con la caja de música, ¡y me seguiste el ritmo!" : "Bailé con la caja de música"); break;
    case DIA_CARTA: {
        const int fu = (uint8_t)a >> 6, q = (uint8_t)a & 63;
        if (fu == CARTA_FAMILIA) snprintf(buf, (size_t)n, "Me llegó una carta de mi %s", p->padre_genero ? "papá" : "mamá");
        else if (fu == CARTA_VIAJERO) snprintf(buf, (size_t)n, "Llegó una carta de %s desde su viaje", q < PET_VIAJEROS && p->viajeros[q].nombre[0] ? p->viajeros[q].nombre : "una viajera");
        else if (fu == CARTA_CRIATURA) snprintf(buf, (size_t)n, "%s me invitó a jugar", CRIATURA_NOMBRE[q % CRI_COUNT]);
        else snprintf(buf, (size_t)n, "Me llegó una carta de %s", q < pet_friends_max(p) ? pet_friend_name(p, q) : "un amigo");
        break;
    }
    case DIA_ANIMAL_NUEVO: {
        /* 20 maneras de contarlo: {frase, 1 = lleva "al/a la" (pet_card_name_a), 0 = "el/la" (pet_card_name)} */
        static const struct { const char *f; uint8_t a; } FR[DIA_ANIMAL_FRASES] = {
            {"Conocimos %s", 1}, {"Hoy vimos por primera vez %s", 1}, {"Descubrimos %s", 1}, {"Apareció %s, ¡qué sorpresa!", 0},
            {"Nos encontramos con %s", 0}, {"Hoy conocí %s", 1}, {"¡Mira! Ahí estaba %s", 0}, {"Me encontré con %s", 0},
            {"Qué lindo conocer %s", 1}, {"Un nuevo amigo del bosque: %s", 0}, {"Primera vez que veo %s", 1}, {"Se asomó %s y me quedé mirando", 0},
            {"Hoy me visitó %s", 0}, {"Conocí %s, ¡qué emoción!", 1}, {"No sabía que existía %s", 0}, {"Descubrí %s en mi paseo", 1},
            {"Estuvimos un rato con %s", 0}, {"Un encuentro inesperado con %s", 0}, {"Hoy apareció %s por primera vez", 0}, {"Saludé %s", 1}};
        char al[48];
        const int v = (b >= 0 && b < DIA_ANIMAL_FRASES) ? b : 0; /* lo guardado antes de esto tiene 0: "Conocimos…" */
        if (FR[v].a) pet_card_name_a((uint8_t)a, al, sizeof al); else snprintf(al, sizeof al, "%s", pet_card_name((uint8_t)a));
        snprintf(buf, (size_t)n, FR[v].f, al);
        break;
    }
    case DIA_CRIATURA:
        if (b < 0) snprintf(buf, (size_t)n, "Conocí a %s, una criatura mágica", CRIATURA_NOMBRE[(uint8_t)a % CRI_COUNT]);
        else snprintf(buf, (size_t)n, "Fuimos a %s con %s", pet_cita_name((cita_t)b), CRIATURA_NOMBRE[(uint8_t)a % CRI_COUNT]);
        break;
    case DIA_HISTORIA:
        if ((uint8_t)a == NARR_TILO) snprintf(buf, (size_t)n, "Apareció una página vieja en el Libro: «%s»", pet_story_title((magica_t)NARR_TILO, b));
        else if ((uint8_t)a == NARR_PRISMA) snprintf(buf, (size_t)n, "El dragón de cristal me contó: «%s»", pet_story_title((magica_t)NARR_PRISMA, b));
        else if ((uint8_t)a == NARR_TILDE) snprintf(buf, (size_t)n, "Tilde me contó un chisme: «%s»", pet_story_title((magica_t)NARR_TILDE, b));
        else if ((uint8_t)a == NARR_BRIZNA) snprintf(buf, (size_t)n, "La Abuela Brizna despertó: «%s»", pet_story_title((magica_t)NARR_BRIZNA, b));
        else snprintf(buf, (size_t)n, "%s me contó: «%s»", pet_narr_name((uint8_t)a > CRI_NIMBA ? 0 : (uint8_t)a), pet_story_title((magica_t)((uint8_t)a > CRI_NIMBA ? 0 : (uint8_t)a), b));
        break;
    case DIA_TILDE: {
        const objeto_info_t *oi = objeto_info((uint8_t)b);
        snprintf(buf, (size_t)n, a ? "Tilde me devolvió %s, con una nota" : "Tilde se llevó %s en el pico (dijo que lo devolvía)", oi ? oi->nombre : "algo brillante");
        break;
    }
    case DIA_NUESTRA: { const cancion_t *c = pet_song(p, a); /* (v38) */
        if (a == 0) snprintf(buf, (size_t)n, "Compusimos nuestra canción: %s. Yo la canto, tú la tocas", c ? c->nombre : "nuestra canción");
        else snprintf(buf, (size_t)n, "Otra canción para el cuaderno: %s", c ? c->nombre : "sin nombre");
        break; }
    case DIA_EXCURSION: { /* (v40) */
        /* (b 1..3: lo traje, partidas viejas; 4..6 desde la 101b: lo encontré y me lo quedé) */
        static const char *const TRAJO[] = {"", " y traje musgo suavecito", " y traje unas ramitas", " y traje un regalito",
                                            ". Encontré musgo suavecito escondido: me lo quedé", ". Encontré unas ramitas escondidas: me las quedé",
                                            ". Encontré un regalito escondido: me lo quedé"};
        if (b == 7) { snprintf(buf, (size_t)n, a ? "Fui solita al mirador y encontré un papelito doblado. No lo leí: lo abrimos juntos" : "Fui solita al claro y encontré un papelito doblado. No lo leí: lo abrimos juntos"); break; } /* (fw 104) */
        snprintf(buf, (size_t)n, a ? "Fui solita al mirador de las nubes: el cóndor me saludó desde su roca%s" : "Fui solita al claro de las luciérnagas: el chucao cantó%s",
                 (unsigned)b < 7 ? TRAJO[b] : "");
        break; }
    case DIA_PASEO_JUNTOS: { /* (fw 104) */
        static const char *const Z[] = {"¡Hoy salimos a caminar de verdad! Como %d00 pasos juntos", "Me llevaste de paseo: %d00 pasos mirando el mundo de afuera",
                                        "Caminamos juntos un buen rato (%d00 pasos). Volví con las patitas cansadas y feliz", "Paseo de verdad: %d00 pasos. ¡Qué grande es el mundo de afuera!"};
        snprintf(buf, (size_t)n, VARIANTE(Z), a);
        break; }
    case DIA_JUEGO_DOS: { /* (1.0.17) */
        static const char *const J[PET_DOS_COUNT] = {"ping pong", "tira y afloja", "memorice", "escondidas"};
        const char *j = (unsigned)a < PET_DOS_COUNT ? J[a] : "un juego";
        if (b == 2) snprintf(buf, (size_t)n, "Jugamos %s con una visita y ¡gané yo!", j);
        else if (b == 1) snprintf(buf, (size_t)n, "Jugamos %s con una visita: quedamos empatados", j);
        else snprintf(buf, (size_t)n, "Jugamos %s con una visita. Perdí, pero nos reímos harto", j);
        break; }
    case DIA_HIELO: snprintf(buf, (size_t)n, "Mi trocito de hielo se derritió... Quedó una gotita. El próximo invierno pescaré otro"); break; /* (fw 104) */
    case DIA_LUGAR: snprintf(buf, (size_t)n, a ? "Subimos al mirador de las nubes, en la cima del cerro" : "El sendero que se mueve nos llevó a una casita escondida"); break;
    case DIA_FIESTA: {
        if (a == FIESTA_FLORES) { snprintf(buf, (size_t)n, b > 0 ? "Día de las Flores Amarillas: te regalé un ramo y mis amigos trajeron más" : "Día de las Flores Amarillas: te regalé un ramo"); break; }
        static const char *const F[] = {"", "Halloween", "Navidad", "Año Nuevo", "San Valentín", "la primavera", "Fiestas Patrias", "mi cumpleaños"};
        const char *qu = (unsigned)a < 8 ? F[a] : "una fiesta";
        if (b > 0) snprintf(buf, (size_t)n, "Celebramos %s con todos nuestros amigos", qu);
        else snprintf(buf, (size_t)n, "Celebramos %s", qu);
        break;
    }
    case DIA_MINI: {
        static const char *const M[MINI_COUNT] = {"Esta noche vimos una lluvia de estrellas", "Cayó la primera lluvia del otoño", "Cayó la primera nieve del invierno", "Hubo una noche de luciérnagas", "", "Hubo luna llena y la miramos juntos", "Vimos cómo la sombra de la Tierra tapaba la luna", "Hubo un eclipse de sol y el día se puso raro"};
        static const char *const M2[MINI_COUNT] = {"Cayeron estrellas fugaces y pedí un deseo", "Llegó la primera lluvia del otoño y olía a tierra", "Empezó a nevar: ¡la primera nieve!", "Las luciérnagas encendieron el bosque", "", "La luna llena estaba enorme esta noche", "La luna se fue oscureciendo: ¡un eclipse!", "El sol se hizo una uñita: ¡eclipse!"};
        if (a == MINI_SOL) snprintf(buf, (size_t)n, "%s", b == 0 ? "Hoy el día y la noche duran lo mismo" : b == 1 ? "Hoy es el día más corto del año" : "Hoy es el día más largo del año");
        else snprintf(buf, (size_t)n, "%s", (var_ & 1 ? M2 : M)[(unsigned)a < MINI_COUNT ? a : 0]);
        break;
    }
    case DIA_ESTACION: {
        static const char *const N[4] = {"la primavera", "el verano", "el otoño", "el invierno"};
        if (b == 1) { static const char *const Z[] = {"Llegó %s: ¡mi estación favorita!", "Por fin %s, mi estación preferida", "¡Llegó %s! No me pude quedar quieta"}; snprintf(buf, (size_t)n, VARIANTE(Z), N[(unsigned)a & 3]); }
        else { static const char *const Z[] = {"Llegó %s... a mí me cuesta un poquito", "Con %s me cuesta un poco, pero aquí estamos", "Empezó %s: no es mi favorita, pero se pasa"}; snprintf(buf, (size_t)n, VARIANTE(Z), N[(unsigned)a & 3]); }
        break;
    }
    case DIA_CANCION: { static const char *const Z[] = {"Encontré mi canción favorita: una %s que me hace bailar", "Esa música %s es mi favorita", "Ya tengo canción: una %s que me alegra el día", "Escuché una melodía %s y supe que era la mía", "Mi canción favorita es %s, ¡no puedo quedarme quieta!", "Creo que mi música es la %s"}; snprintf(buf, (size_t)n, VARIANTE(Z), b == 0 ? "lenta y suave" : b == 1 ? "alegre" : "rápida"); break; }
    case DIA_MELODIA: { static const char *const Z[] = {"Compuse mi propia melodía con los cristales de la cueva", "Después de ensayar mucho, ya tengo mi melodía", "Los cristales de la cueva me dieron mi propia melodía"}; snprintf(buf, (size_t)n, "%s", VARIANTE(Z)); break; }
    case DIA_GUSTO_SEM: { static const char *const Z[] = {"Esta semana lo mío fue %s: %d veces", "Esta semana me pasé %s: %d veces", "Semana de %s: lo hice %d veces", "Qué semana: %s, %d veces", "No me cansé de %s: %d veces esta semana"}; snprintf(buf, (size_t)n, VARIANTE(Z), pet_activity_verb(a), b); break; }
    case DIA_GUSTO: {
        static const char *const F[4] = {"Descubrí que lo que más me gusta es %s", "Me encanta %s, ¡y tú me haces feliz!", "Creo que mi cosa favorita es %s", "Hoy me di cuenta: ¡me fascina %s!"};
        snprintf(buf, (size_t)n, F[(unsigned)b & 3], pet_activity_verb(a));
        break;
    }
    case DIA_NOTA_TINTA: snprintf(buf, (size_t)n, "%s", pet_ink_note(a)); break;
    case DIA_COFRE: snprintf(buf, (size_t)n, a > 1 ? "Abrimos el cofre bajo el árbol viejo: adentro había %d tesoros dorados" : "Abrimos el cofre bajo el árbol viejo: adentro brillaba un tesoro dorado", a); break;
    case DIA_FINAL:
        snprintf(buf, (size_t)n, a ? "El libro mágico se escribió solo: ahora cuenta nuestra historia" : "Descubrimos casi todo el libro mágico");
        break;
    case DIA_ESPECIAL: {
        static const char *const E[ESP_COUNT] = {"Bailé en el anillo de las hadas del bosque", "Descubrí el reflejo encantado de la laguna",
                                                 "Elevamos la cometa en el cerro y nos trajo algo de lejos", "El secreto de la cueva: despertamos a Prisma, el dragón de cristal"};
        snprintf(buf, (size_t)n, "%s", E[(uint8_t)a % ESP_COUNT]);
        break;
    }
    case DIA_ANIMAL:
        if (a == ANIMAL_ERIZO) snprintf(buf, (size_t)n, "El pichi ya no se esconde conmigo: ¡somos amigos!");
        else if (a == ANIMAL_ARDILLA) snprintf(buf, (size_t)n, "El monito del monte me devolvió la semilla que se había llevado");
        else if (a == ANIMAL_TORTUGA) snprintf(buf, (size_t)n, "Me senté junto a la tortuga sabia y se me pasó la pena");
        else snprintf(buf, (size_t)n, "Vi a %s", pet_animal_name((animal_t)a));
        break;
    default: break;
    }
    genero_en(p, buf, n);
#undef VARIANTE
    return buf;
}


const char *pet_need_name(need_t n)
{
    static const char *const T[] = {"saciedad", "energia", "higiene", "diversion", "carino", "salud", "curiosidad", "seguridad"};
    return NOMBRE(T, n, NEED_COUNT);
}
const char *pet_mood_name(mood_t m)
{
    static const char *const T[] = {"feliz", "tranquila", "hambrienta", "somnolienta", "sucia", "aburrida", "triste", "enojada",
                                    "enferma", "enamorada", "dormida", "curiosa", "asustada", "orgullosa", "celosa"};
    return NOMBRE(T, m, MOOD_COUNT);
}
const char *pet_emotion_name(emo_t e)
{
    static const char *const T[] = {"alegria", "tristeza", "enojo", "miedo", "curiosidad", "aburrimiento", "cansancio", "orgullo", "celos", "carino"};
    return NOMBRE(T, e, EMO_COUNT);
}
const char *pet_cause_name(causa_t c)
{
    static const char *const T[] = {"-", "jugamos", "no jugaste", "caricias", "te extrana", "hambre", "comida rica", "comida fea",
                                    "sueno", "sucia", "enferma", "lluvia", "sol", "nieve", "viento", "tormenta", "nublado",
                                    "visita", "amigo", "toque feo", "sacudida", "hallazgo", "crecio", "mimaste a otro",
                                    "novedad", "lugar feo", "volviste", "nada que hacer", "la despertaste", "bano", "explorar", "frio",
                                    "calor", "bebida", "mariposa", "abeja", "pajaro", "zorro", "espantaste", "jardin",
                                    "lugar", "charco", "hojas", "vecino", "pesca", "cumpleanos", "truco", "inclinar",
                                    "tesoro", "algo raro", "meta", "su cumpleanos", "fecha especial", "habilidad", "ropa", "juguete", "falta de carino", "carta",
                                    "criatura", "secreto"};
    return NOMBRE(T, c, CAUSA_COUNT);
}
const char *pet_want_name(want_t w)
{
    static const char *const T[] = {"nada", "comer", "dormir", "bano", "jugar", "carino", "medicina", "bebida caliente", "bebida fria"};
    return NOMBRE(T, w, WANT_COUNT);
}
const char *pet_action_name(action_t a)
{
    static const char *const T[] = {"alimentar", "banar", "jugar", "acariciar", "dormir", "despertar", "curar", "sacudir", "cepillar", "mimar visita", "llamar", "ir", "plantar", "regar", "espantar", "pescar", "invitar vecino", "lanzar", "inclinar", "truco", "fin del juego",
                                    "vestir", "poner juguete", "usar juguete", "limpiar", "salpicar", "arrancar", "animal"};
    return NOMBRE(T, a, sizeof T / sizeof T[0]);
}
const char *pet_activity_name(actividad_t a)
{
    static const char *const T[] = {"pasea", "olfatea", "cava", "siesta", "explora", "mira el cielo", "juega sola", "se refugia",
                                    "busca sombra", "juega en la nieve", "te espera", "se abriga", "persigue", "pesca",
                                    "juega con su amigo", "nada", "trepa un arbol", "juega con un juguete", "chapotea", "juega con la pelota", "se prueba algo", "busca comida", "mira aves", "charla con un animal", "canta", "baila"};
    return NOMBRE(T, a, ACTV_COUNT);
}
const char *pet_place_name(lugar_t l)
{
    static const char *const T[] = {"el claro", "el estanque", "el arbol", "las rocas", "el sendero", "la casita"};
    return NOMBRE(T, l, LUGAR_COUNT);
}
const char *pet_find_name(hallazgo_t h)
{
    static const char *const T[] = {"una fruta", "miel", "una hierba", "una semilla", "una piedra brillante", "una ramita",
                                    "una chapita", "una hoja seca", "un puñado de musgo"};
    return NOMBRE(T, h, HALL_COUNT);
}
const char *pet_trait_name(trait_t t)
{
    static const char *const T[] = {"glotona", "juguetona", "carinosa", "timida", "pulcra", "curiosa", "traviesa"};
    return NOMBRE(T, t, TRAIT_COUNT);
}
const char *pet_gesture_name(gesto_t g)
{
    static const char *const T[] = {"las palmaditas", "las caricias", "muchos toques seguidos", "los abrazos"};
    return NOMBRE(T, g, GESTO_COUNT);
}
const char *pet_neighbor_activity_name(vecino_act_t a)
{
    static const char *const T[] = {"llega", "se va", "juegan a la pelota", "comen juntos", "se persiguen", "conversan",
                                    "duermen una siesta", "le trae un regalo", "pasa de visita y la deja dormir"};
    return NOMBRE(T, a, VEC_DORMIDA + 1);
}
const char *pet_trick_name(truco_t t)
{
    static const char *const T[] = {"hacerse {la} dormid{a}", "dar una voltereta", "dar una vuelta", "saludar"};
    return NOMBRE(T, t, TRUCO_COUNT);
}
const char *pet_game_name(juego_t j)
{
    static const char *const T[] = {"la pelota", "las escondidas", "atrapar frutas", "los hongos de luz"};
    return NOMBRE(T, j, JUEGO_COUNT);
}
const char *pet_catch_name(pesca_t c)
{
    static const char *const T[] = {"nada", "un pez", "una bota vieja", "una piedra brillante", "una concha", "un alga",
                                    "un cangrejito", "una hoja de nenúfar", "una botella con un mensaje", "una llave oxidada",
                                    "¡un pez dorado!", "un trocito de hielo"};
    return NOMBRE(T, c, PESCA_COUNT);
}
const char *pet_food_name(food_t f)
{
    static const char *const T[] = {"comida", "fruta", "miel", "chocolate caliente", "jugo con hielo"};
    return NOMBRE(T, f, FOOD_COUNT);
}
const char *pet_stage_name(stage_t s)
{
    static const char *const T[] = {"huevo", "bebe", "cria", "adulta"};
    return NOMBRE(T, s, STAGE_COUNT);
}
#define PET_SONIDO_INFO(id, g, n, sig, c) {#id, g, n, sig, c},
static const pet_sonido_info_t SONIDOS_INFO[SND_COUNT] = {PET_SONIDOS(PET_SONIDO_INFO)};
const pet_sonido_info_t *pet_sound_info(sound_t s) { return (unsigned)s < SND_COUNT ? &SONIDOS_INFO[s] : NULL; }
const char *pet_sound_name(sound_t s) { return (unsigned)s < SND_COUNT ? SONIDOS_INFO[s].nombre : "?"; }
const char *pet_encounter_name(encounter_t e)
{
    static const char *const T[] = {"jugaron", "pelearon", "se enamoraron", "se asusto", "se ignoraron"};
    return NOMBRE(T, e, ENC_COUNT);
}
const char *pet_item_name(item_t i)
{
    static const char *const T[] = {"fruta", "miel", "hierba", "semilla", "piedra brillante"};
    return NOMBRE(T, i, ITEM_COUNT);
}
const char *pet_decor_name(decor_t d)
{
    static const char *const T[] = {"flores", "hongos", "farol", "banco", "casita"};
    return NOMBRE(T, d, DECOR_COUNT);
}
const char *pet_form_name(forma_t f)
{
    static const char *const T[] = {"normal", "radiante", "grunona"};
    return NOMBRE(T, f, FORMA_COUNT);
}
const char *pet_season_name(estacion_t e)
{
    static const char *const T[] = {"primavera", "verano", "otono", "invierno"};
    return NOMBRE(T, e, ESTACION_COUNT);
}
const char *pet_scene_name(escena_t e)
{
    static const char *const T[] = {"prado", "bosque", "laguna", "cerro", "cueva", "isla", "claro", "mirador"};
    return NOMBRE(T, e, ESC_COUNT);
}
const char *pet_scene_el(escena_t e) { static const char *const T[] = {"el prado", "el bosque", "la laguna", "el cerro", "la cueva", "la isla", "el claro", "el mirador"}; return NOMBRE(T, e, ESC_COUNT); }
const char *pet_scene_al(escena_t e) { static const char *const T[] = {"al prado", "al bosque", "a la laguna", "al cerro", "a la cueva", "a la isla", "al claro", "al mirador"}; return NOMBRE(T, e, ESC_COUNT); }
/* (v40) los nombres propios de cada lugar (en el mapa y en las historias) */
const char *pet_scene_proper(escena_t e) { static const char *const T[] = {"El Prado", "Bosque de los Coihues", "Laguna de los Cisnes", "Cerro del Viento", "La Cueva de Cristal", "Isla del Faro", "El Claro del Rey", "El Mirador"}; return NOMBRE(T, e, ESC_COUNT); }
const char *pet_fauna_name(fauna_t f)
{
    static const char *const T[] = {"mariposa", "abeja", "pajaro", "zorro", "rana"};
    return NOMBRE(T, f, FAUNA_COUNT);
}
const char *pet_weather_name(clima_t c)
{
    static const char *const T[] = {"despejado", "nublado", "lluvia", "viento", "nieve", "tormenta", "frio"};
    return NOMBRE(T, c, CLIMA_COUNT);
}

/* ======================================================================
 * 9. Mensajes del día y cómo se siente
 * ==================================================================== */

/* los mensajes del día: cada uno con su tipo, que decide la respuesta del botón (revisadas una por una) */
static const struct { const char *texto; uint8_t tipo; } FRASES[] = {
    {"El bosque me contó que hoy te va a ir muy bien.", FRASE_ANIMO},
    {"Un paso pequeño también deja huella en el bosque.", FRASE_ANIMO},
    {"Tienes más magia de la que crees.", FRASE_ANIMO},
    {"Respira hondo, como el viento entre los árboles.", FRASE_CUIDATE},
    {"Las luciérnagas y yo estamos felices de verte.", FRASE_TE_QUIERE},
    {"Lo estás haciendo mejor de lo que piensas.", FRASE_ANIMO},
    {"Hasta las flores toman agua. Tómate un vaso tú.", FRASE_CUIDATE},
    {"Asómate a la ventana: cada mañana trae algo nuevo que descubrir.", FRASE_MIRA},
    {"Tu sonrisa ilumina mi pradera.", FRASE_ANIMO},
    {"Descansar también es parte de la magia. Descansa un ratito.", FRASE_CUIDATE},
    {"Eres importante para mí y para el bosque.", FRASE_TE_QUIERE},
    {"Confía en ti. Yo confío.", FRASE_ANIMO},
    {"Lo difícil se vuelve fácil, como la semilla que se vuelve árbol.", FRASE_ANIMO},
    {"Hoy es buen día para descubrir algo nuevo. ¡Mira a tu alrededor!", FRASE_MIRA},
    {"Eres valiente, incluso si no lo sientes.", FRASE_ANIMO},
    {"Gracias por cuidarme. El bosque lo nota.", FRASE_AGRADECE},
    {"No tienes que hacerlo todo hoy. Los árboles crecen despacio.", FRASE_ANIMO},
    {"Tu esfuerzo brilla, aunque nadie lo vea.", FRASE_ANIMO},
    {"Sal a mirar el cielo un ratito. Quizás veas algo mágico.", FRASE_MIRA},
    {"Tienes un corazón muy bonito.", FRASE_ANIMO},
    {"Lo que siembras hoy, florece mañana.", FRASE_ANIMO},
    {"Equivocarse es parte de aprender. Yo me caigo trepando y vuelvo a subir.", FRASE_ANIMO},
    {"Eres mi persona favorita de todo el mundo.", FRASE_TE_QUIERE},
    {"Haz una pausa y estírate, como un gato al sol.", FRASE_CUIDATE},
    {"Hoy algo bueno brilla para ti.", FRASE_ANIMO},
    {"Puedes con esto. Yo te acompaño.", FRASE_ABRAZO},
    {"Celebra tus pequeños logros. Yo guardo cada uno.", FRASE_ANIMO},
    {"Sé amable contigo, como lo eres conmigo.", FRASE_CUIDATE},
    {"Después de la lluvia sale el arcoíris.", FRASE_ABRAZO},
    {"Juntos somos un gran equipo, tú y yo.", FRASE_ABRAZO},
    {"Te guardé el rayito de sol más tibio de la mañana.", FRASE_ANIMO},
    {"Cuando no estás, le cuento a la laguna cosas de ti.", FRASE_ABRAZO},
    {"Mira por la ventana: ¿qué color tiene hoy el cielo?", FRASE_MIRA},
    {"Una canción bonita cura casi todo. Canta una hoy.", FRASE_CUIDATE},
    {"Los hongos dicen que eres muy paciente. Yo también lo creo.", FRASE_ANIMO},
    {"No te olvides de comer algo rico. Yo comí una baya enorme.", FRASE_CUIDATE},
    {"Gracias por los mimos. Todavía me hacen cosquillas.", FRASE_AGRADECE},
    {"Si tienes sueño, cierra los ojos un ratito.", FRASE_CUIDATE},
    {"Busca algo verde a tu alrededor. Es un pedacito de mi bosque.", FRASE_MIRA},
    {"Tu risa hace florecer los dientes de león.", FRASE_ANIMO},
    {"Hoy aprendí algo nuevo. Busca tú también algo que no conocías.", FRASE_MIRA},
    {"Para que te quieran no hace falta hacerlo todo bien.", FRASE_ABRAZO},
    {"Las estrellas de anoche brillaron un poquito por ti.", FRASE_ANIMO},
    {"Te extrañé, aunque fuera un ratito.", FRASE_TE_QUIERE},
    {"Saluda a alguien que quieras. Le va a gustar.", FRASE_CUIDATE},
    {"Las nubes de hoy tienen forma de abrazo.", FRASE_ABRAZO},
    {"Eres como un árbol: cada día un poquito más alto.", FRASE_ANIMO},
    {"Mira las hojas que se mueven con el viento: están bailando.", FRASE_MIRA},
    {"Me encanta que seamos amigos.", FRASE_TE_QUIERE},
    {"Gracias por jugar conmigo. Guardé ese momento con mis tesoros.", FRASE_AGRADECE},
};
#define N_FRASES ((int)(sizeof(FRASES) / sizeof(FRASES[0])))

int pet_phrase_count(void) { return N_FRASES; }
const char *pet_phrase(int i) { return (i >= 0 && i < N_FRASES) ? FRASES[i].texto : ""; }
frase_tipo_t pet_phrase_type(int i) { return (i >= 0 && i < N_FRASES && FRASES[i].tipo < FRASE_TIPOS) ? (frase_tipo_t)FRASES[i].tipo : FRASE_ANIMO; }
respuesta_t pet_phrase_reply(int i)
{
    static const uint8_t R[FRASE_TIPOS] = {RESP_GRACIAS, RESP_YO_TAMBIEN, RESP_LO_HARE, RESP_VOY_A_MIRAR, RESP_CON_CARINO, RESP_ABRAZO};
    return (respuesta_t)R[pet_phrase_type(i)];
}
const char *pet_reply_text(respuesta_t r)
{
    static const char *const T[RESP_COUNT] = {"¡UN ABRAZO!", "¡LO HARÉ!", "¡YO TAMBIÉN!", "¡VOY A MIRAR!", "¡CON CARIÑO!", "¡GRACIAS!"};
    return (unsigned)r < RESP_COUNT ? T[r] : T[0];
}
/* respondiste el mensaje del día: un poquito de cariño, una vez por mensaje */
bool pet_phrase_hug(pet_t *p, uint32_t ahora)
{
    if (p->frase_respondida || p->etapa == STAGE_HUEVO) return false;
    p->frase_respondida = 1;
    sumar(p, NEED_CARINO, 8);
    impulsar(p, EMO_CARINO, 0.7f, CAUSA_CARICIAS);
    impulsar(p, EMO_ALEGRIA, 0.5f, CAUSA_CARICIAS);
    subir_vinculo(p, 0.3f);
    p->ultima_interaccion = ahora;
    return true;
}

/* "Me siento feliz porque jugamos, pero tengo sueño." */
/* cómo lo dice: tres intensidades (un poco, normal, mucho) y dos maneras de decir cada una */
static const char *texto_emocion(emo_t e, float valor, uint32_t v)
{
    static const char *const T[EMO_COUNT][3][2] = {
        {{"Estoy content{a}", "Me siento bien"}, {"Me siento feliz", "Estoy feliz"}, {"Estoy feliz feliz", "Qué feliz estoy"}},
        {{"Estoy un poquito triste", "Me siento algo bajonead{a}"}, {"Me siento triste", "Estoy triste"}, {"Estoy muy triste", "Tengo ganas de llorar"}},
        {{"Estoy un poco molest{a}", "Ando algo gruñon{a}"}, {"Estoy de mal humor", "Estoy enojad{a}"}, {"Estoy muy enojad{a}", "Grrr, qué rabia"}},
        {{"Estoy un poco nervios{a}", "Algo me inquieta"}, {"Tengo miedo", "Me asusté"}, {"Tengo mucho miedo", "Estoy temblando de miedo"}},
        {{"Tengo un poco de curiosidad", "Algo me llama la atención"}, {"Tengo curiosidad", "Quiero saber qué es"}, {"Me muero de curiosidad", "Quiero verlo todo"}},
        {{"Me estoy aburriendo", "Estoy medio aburrid{a}"}, {"Me aburro", "Estoy aburrid{a}"}, {"Me aburro muchísimo", "Ya no sé qué hacer"}},
        {{"Tengo un poquito de sueño", "Estoy algo cansad{a}"}, {"Tengo sueño", "Estoy cansad{a}"}, {"Me caigo de sueño", "Estoy agotad{a}"}},
        {{"Estoy orgullos{a}", "Me salió bien"}, {"Me siento importante", "Estoy muy orgullos{a}"}, {"Soy {la} mejor", "Mírame, mírame"}},
        {{"Me dan un poco de celos", "Mmm... no me gusta eso"}, {"Siento celos", "Estoy celos{a}"}, {"Estoy muy celos{a}", "Quiero que me mires solo a mí"}},
        {{"Te quiero", "Me gusta estar contigo"}, {"Te quiero mucho", "Me siento querid{a}"}, {"Te quiero muchísimo", "Eres lo mejor del mundo"}},
    };
    int nivel = valor > 0.75f ? 2 : (valor < 0.35f ? 0 : 1);
    return T[(unsigned)e % EMO_COUNT][nivel][v == 0xFFFFFFFFu ? 0 : (v & 1u)]; /* v = ~0: la forma más simple */
}

static const char *texto_causa_base(const pet_t *p, causa_t c);
/* las causas que más se repiten tienen varias maneras de decirse */
static const char *texto_causa(const pet_t *p, causa_t c, uint32_t v)
{
    static const char *const NOV[] = {"porque hay algo nuevo", "porque descubrí algo", "porque todo es nuevo para mí"};
    static const char *const EXP[] = {"porque estoy explorando", "porque quiero ver qué hay por aquí", "porque ando de aventura"};
    static const char *const CAR[] = {"porque me acariciaste", "porque me haces cariño", "por tus mimos"};
    static const char *const JUG[] = {"porque jugamos", "porque jugaste conmigo", "porque nos divertimos"};
    static const char *const MAR[] = {"por las mariposas", "porque vi una mariposa", "porque hay mariposas volando"};
    static const char *const PAJ[] = {"por mi amigo el pájaro", "porque vino el pajarito"};
    static const char *const HAL[] = {"porque encontré algo", "porque encontré un tesorito", "porque encontré una cosa linda"};
    static const char *const NAD[] = {"porque no hay nada nuevo", "porque todo está muy tranquilo"};
    static const char *const HAM[] = {"porque tengo hambre", "porque me suena la pancita"};
    static const char *const SOL[] = {"porque hay sol", "porque salió el sol", "por este solcito"};
    static const char *const VOL[] = {"porque volviste", "porque ya estás aquí"};
    static const char *const EXT[] = {"porque te extraño", "porque te fuiste mucho rato"};
    static const char *const FCA[] = {"porque hace rato que no me haces cariño", "porque quiero mimos", "porque necesito cariño"};
#define VAR(t) return t[v % (sizeof t / sizeof t[0])]
    switch (c) {
    case CAUSA_NOVEDAD: if (pet_age_days(p, p->ultima_actualizacion) < 1.0f) return NOV[2]; v %= 2; VAR(NOV);
    case CAUSA_EXPLORAR: VAR(EXP);
    case CAUSA_CARICIAS: VAR(CAR);
    case CAUSA_JUGAMOS: VAR(JUG);
    case CAUSA_MARIPOSA: VAR(MAR);
    case CAUSA_PAJARO: VAR(PAJ);
    case CAUSA_HALLAZGO: VAR(HAL);
    case CAUSA_NADA_QUE_HACER: VAR(NAD);
    case CAUSA_HAMBRE: VAR(HAM);
    case CAUSA_SOL: VAR(SOL);
    case CAUSA_VOLVISTE: VAR(VOL);
    case CAUSA_TE_EXTRANA: VAR(EXT);
    case CAUSA_FALTA_CARINO: VAR(FCA);
    default: return texto_causa_base(p, c);
    }
#undef VAR
}

static const char *texto_causa_base(const pet_t *p, causa_t c)
{
    switch (c) {
    case CAUSA_JUGAMOS: return "porque jugamos";
    case CAUSA_NO_JUGASTE: return "porque hoy no jugamos";
    case CAUSA_CARICIAS: return "porque me acariciaste";
    case CAUSA_TE_EXTRANA: return "porque te extraño";
    case CAUSA_HAMBRE: return "porque tengo hambre";
    case CAUSA_COMIDA_RICA: return "porque la comida estaba rica";
    case CAUSA_COMIDA_FEA: return "porque esa comida no me gusta";
    case CAUSA_SUENO: return "porque no he dormido";
    case CAUSA_SUCIA: return "porque necesito un baño";
    case CAUSA_ENFERMA: return "porque no me siento bien";
    case CAUSA_LLUVIA: return p->memoria.clima[CLIMA_LLUVIA] >= 0 ? "porque me encanta la lluvia" : "porque llueve y prefiero el sol";
    case CAUSA_SOL: return "porque hay sol";
    case CAUSA_NIEVE: return p->memoria.clima[CLIMA_NIEVE] >= 0 ? "porque hay nieve" : "porque no me gusta la nieve";
    case CAUSA_FRIO: return p->memoria.clima[CLIMA_FRIO] >= 0 ? "porque me gusta el frío" : "porque hace mucho frío";
    case CAUSA_CALOR: return "porque hace mucho calor";
    case CAUSA_BEBIDA: return p->bebida_tipo == FOOD_BEBIDA_CALIENTE ? "por el chocolate calentito" : "por el jugo fresquito";
    case CAUSA_MARIPOSA: return "por las mariposas";
    case CAUSA_ABEJA: return p->miedo_abejas > 0.3f ? "porque me picó una abeja" : "por las abejas";
    case CAUSA_PAJARO: return "por mi amigo el pájaro";
    case CAUSA_ZORRO: return "por culpa del zorro";
    case CAUSA_ESPANTASTE: return "porque espantaste al zorro";
    case CAUSA_JARDIN: return "por el jardín";
    case CAUSA_LUGAR: return pet_scene_liking(p, (escena_t)p->escena) >= 0 ? "porque me encanta este lugar" : "porque no me gusta este lugar";
    case CAUSA_CHARCO: return "por el charco";
    case CAUSA_HOJAS: return "por el montón de hojas";
    case CAUSA_VIENTO: return p->memoria.clima[CLIMA_VIENTO] >= 0 ? "porque sopla el viento" : "porque el viento me despeina";
    case CAUSA_TORMENTA: return p->memoria.clima[CLIMA_TORMENTA] >= 0 ? "porque hay tormenta" : "porque hay truenos";
    case CAUSA_NUBLADO: return "porque está nublado";
    case CAUSA_VISITA: return "por la visita";
    case CAUSA_AMIGO: return "porque vino un amigo";
    case CAUSA_TOQUE_FEO: return "porque no me gusta que me toquen así";
    case CAUSA_VECINO: {
        static char t[48];
        snprintf(t, sizeof t, "porque vino %s", p->vecino.nombre[0] ? p->vecino.nombre : "mi amigo");
        return t;
    }
    case CAUSA_PESCA: return "porque fuimos a pescar";
    case CAUSA_CUMPLE: return "porque es tu cumpleaños";
    case CAUSA_TRUCO: return "porque me salió el truco";
    case CAUSA_INCLINAR: return p->rasgo[TRAIT_TIMIDA] > 0.6f ? "porque me resbalé" : "porque me deslicé, ¡wiii!";
    case CAUSA_TESORO: return "porque encontré un tesoro";
    case CAUSA_RARO: return p->raro != 0xFF && p->raro < RARO_COUNT ? (p->raro == RARO_ARCOIRIS ? "porque salió un arcoíris" : (p->raro == RARO_ESTRELLA_FUGAZ ? "porque vi una estrella fugaz" : (p->raro == RARO_CIERVO ? "porque vi un pudú blanco" : "porque pasó un globo"))) : "porque vi algo mágico";
    case CAUSA_META: return "porque cumplimos la meta";
    case CAUSA_SU_CUMPLE: return "porque es mi cumpleaños";
    case CAUSA_FECHA: return "porque hoy es un día especial";
    case CAUSA_HABILIDAD: return "porque estoy aprendiendo algo nuevo";
    case CAUSA_ROPA: return "por lo que me pusiste";
    case CAUSA_JUGUETE: return "porque jugué con mis juguetes";
    case CAUSA_CARTA: return "porque me llegó una carta";
    case CAUSA_SACUDIDA: return "porque me sacudiste";
    case CAUSA_HALLAZGO: return "porque encontré algo";
    case CAUSA_CRECIO: return "porque crecí";
    case CAUSA_MIMASTE_OTRO: return "porque mimaste a otro";
    case CAUSA_NOVEDAD: return "porque hay algo nuevo";
    case CAUSA_LUGAR_FEO: return "porque aquí me asusté una vez";
    case CAUSA_VOLVISTE: return "porque volviste";
    case CAUSA_NADA_QUE_HACER: return "porque no hay nada nuevo";
    case CAUSA_LA_DESPERTASTE: return "porque me despertaste";
    case CAUSA_BANO: return "por el baño";
    case CAUSA_EXPLORAR: return "porque estoy explorando";
    default: return "";
    }
}

static bool positiva(emo_t e) { return e == EMO_ALEGRIA || e == EMO_CURIOSIDAD || e == EMO_ORGULLO || e == EMO_CARINO; }

/* algo más que contar, en primera persona: lo que hace, lo que quiere, el clima o algo de hoy */
static const char *contexto(const pet_t *p, uint32_t v, char *t, int n)
{
    static const char *const HACE[ACTV_COUNT] = {
        "Estoy dando un paseo.", "Estoy olfateando algo.", "Estoy cavando un hoyito.", "Estoy tomando una siesta.",
        "Estoy explorando.", "Estoy mirando el paisaje.", "Estoy jugando sol{a}.", "Me escondí un ratito.",
        "Estoy a la sombrita.", "¡Estoy jugando en la nieve!", "Te estaba esperando.", "Me estoy abrigando.",
        "¡Estoy persiguiendo algo!", "Estoy pescando.", "Estoy con mi amigo.", "Estoy nadando.", "Estoy trepando un árbol.",
        "Estoy jugando con mis juguetes.", "¡Estoy chapoteando!", "Estoy jugando con la pelota.", "Me estoy probando algo.", "Estoy buscando algo rico.", "Estoy mirando los pajaritos.", "Estoy charlando con un amigo del bosque.", "¡Estoy cantando!", "¡Estoy bailando!",
    };
    static const char *const QUIERE[WANT_COUNT] = {"", "¿Me das algo de comer?", "Quiero dormir un poco.", "Necesito un baño.",
                                                   "¿Jugamos?", "¿Me haces cariño?", "No me siento muy bien...",
                                                   "¿Me das algo calentito?", "¿Me das algo fresquito?"};
    static const char *const HOY[VIV_COUNT] = {
        "Hoy jugamos a la pelota.", "Hoy fui a pescar.", "Hoy vino un amigo.", "Hoy vi mariposas.", "Hoy vino el zorro.",
        "Hoy llovió.", "Hoy jugué en la nieve.", "Hoy hice un truco.", "Hoy nadé.", "Hoy trepé un árbol.", "Hoy encontré un tesoro.",
        "Hoy comí fruta.", "Hoy me hiciste cariño.", "Hoy fui al bosque.", "Hoy fui a la laguna.", "Hoy subí al cerro.",
        "Hoy entré a la cueva.", "Hoy vi un arcoíris.", "Hoy hubo fiesta.", "Hoy vi flores.", "Hoy jugamos a las escondidas.",
        "Hoy atrapamos frutas.", "Hoy vi al pajarito.", "Hoy jugué con mis juguetes.", "Hoy me pusiste algo lindo.",
    };
    t[0] = 0;
    /* primero lo que necesita: eso es lo más importante de decir */
    if (p->deseo > WANT_NADA && p->deseo < WANT_COUNT && v % 3 != 2) { snprintf(t, (size_t)n, "%s", QUIERE[p->deseo]); return t; }
    switch (v % 4) {
    case 0:
        if (p->actividad < ACTV_COUNT) snprintf(t, (size_t)n, "%s", HACE[p->actividad]);
        break;
    case 1: { /* algo que vivió hoy */
        int cuantos = 0;
        for (int k = 0; k < VIV_COUNT; k++) cuantos += (p->vivido >> k) & 1u;
        if (!cuantos) break;
        int elegido = (int)(v / 4 % (uint32_t)cuantos);
        for (int k = 0; k < VIV_COUNT; k++)
            if ((p->vivido >> k) & 1u) { if (!elegido--) { snprintf(t, (size_t)n, "%s", HOY[k]); break; } }
        break;
    }
    case 2: { /* el clima, según le guste o no */
        clima_t c = pet_weather(p);
        int g = p->memoria.clima[c];
        static const char *const BIEN[CLIMA_COUNT] = {"¡Qué lindo día!", "Me gustan las nubes.", "Me encanta la lluvia.",
                                                       "¡Qué rico el viento!", "¡Nieve, nieve!", "Las tormentas me emocionan.", "El frío me gusta."};
        static const char *const MAL[CLIMA_COUNT] = {"Hay mucho sol para mí.", "Está muy gris hoy.", "Ojalá deje de llover.",
                                                      "El viento me despeina.", "La nieve me enfría las patitas.", "Los truenos me asustan.",
                                                      "Brrr, qué frío."};
        if (p->escena != ESC_CUEVA && (g > 30 || g < -30)) snprintf(t, (size_t)n, "%s", g > 0 ? BIEN[c] : MAL[c]);
        break;
    }
    default: /* lo que siente por ti */
        if (p->vinculo >= 75) snprintf(t, (size_t)n, "%s", v / 4 % 2 ? "Contigo todo es mejor." : "Me alegra que estés aquí.");
        else if (p->vinculo >= 40) snprintf(t, (size_t)n, "%s", v / 4 % 2 ? "Cada día te conozco más." : "Me gusta cuando me visitas.");
        break;
    }
    return t;
}

/* ¿tiene sentido decir "siento <emoción> <porque causa>"? Evita frases como "estoy feliz porque esa comida no me gusta" o
 * "tengo miedo porque encontré un tesoro": la causa tiene que ir en la misma dirección que la emoción */
static int causa_signo(const pet_t *p, causa_t c)
{
    switch (c) {
    /* el clima y el lugar: según le gusten o no (es lo mismo que dice la frase de la causa) */
    case CAUSA_LLUVIA: return p->memoria.clima[CLIMA_LLUVIA] >= 0 ? 1 : -1;
    case CAUSA_NIEVE: return p->memoria.clima[CLIMA_NIEVE] >= 0 ? 1 : -1;
    case CAUSA_FRIO: return p->memoria.clima[CLIMA_FRIO] >= 0 ? 1 : -1;
    case CAUSA_VIENTO: return p->memoria.clima[CLIMA_VIENTO] >= 0 ? 1 : -1;
    case CAUSA_LUGAR: return pet_scene_liking(p, (escena_t)p->escena) >= 0 ? 1 : -1;
    case CAUSA_INCLINAR: return p->rasgo[TRAIT_TIMIDA] > 0.6f ? -1 : 1;
    case CAUSA_ABEJA: return p->miedo_abejas > 0.3f ? -1 : 0;
    case CAUSA_JUGAMOS: case CAUSA_CARICIAS: case CAUSA_COMIDA_RICA: case CAUSA_SOL: case CAUSA_MARIPOSA: case CAUSA_PAJARO:
    case CAUSA_ESPANTASTE: case CAUSA_JARDIN: case CAUSA_VECINO: case CAUSA_AMIGO: case CAUSA_PESCA: case CAUSA_CUMPLE: case CAUSA_TRUCO:
    case CAUSA_TESORO: case CAUSA_RARO: case CAUSA_META: case CAUSA_SU_CUMPLE: case CAUSA_FECHA: case CAUSA_HABILIDAD: case CAUSA_JUGUETE:
    case CAUSA_CARTA: case CAUSA_HALLAZGO: case CAUSA_CRECIO: case CAUSA_VOLVISTE: case CAUSA_BANO: case CAUSA_CRIATURA: case CAUSA_ESPECIAL:
    case CAUSA_BEBIDA:
        return 1;
    case CAUSA_NO_JUGASTE: case CAUSA_TE_EXTRANA: case CAUSA_HAMBRE: case CAUSA_COMIDA_FEA: case CAUSA_SUENO: case CAUSA_SUCIA: case CAUSA_ENFERMA:
    case CAUSA_TOQUE_FEO: case CAUSA_SACUDIDA: case CAUSA_MIMASTE_OTRO: case CAUSA_LUGAR_FEO: case CAUSA_NADA_QUE_HACER: case CAUSA_LA_DESPERTASTE:
    case CAUSA_CALOR: case CAUSA_ZORRO: case CAUSA_TORMENTA: case CAUSA_FALTA_CARINO:
        return -1;
    default: return 0;
    }
}
static bool cuadra(const pet_t *p, emo_t e, causa_t c)
{
    if (c == CAUSA_NINGUNA) return false;
    const int s = causa_signo(p, c);
    switch (e) {
    case EMO_ALEGRIA: return s >= 0;
    case EMO_ORGULLO: /* orgullo es por algo que logró o que le quedó lindo */
        return c == CAUSA_TRUCO || c == CAUSA_HABILIDAD || c == CAUSA_META || c == CAUSA_CRECIO || c == CAUSA_TESORO || c == CAUSA_HALLAZGO ||
               c == CAUSA_PESCA || c == CAUSA_JUGAMOS || c == CAUSA_CUMPLE || c == CAUSA_SU_CUMPLE || c == CAUSA_ROPA || c == CAUSA_JARDIN ||
               c == CAUSA_ESPANTASTE || c == CAUSA_CRIATURA || c == CAUSA_ESPECIAL || c == CAUSA_RARO || c == CAUSA_JUGUETE || c == CAUSA_BANO;
    case EMO_CURIOSIDAD: return s >= 0 && c != CAUSA_COMIDA_RICA && c != CAUSA_BANO;
    case EMO_CARINO: return s >= 0 || c == CAUSA_TE_EXTRANA;
    case EMO_ENOJO: return s <= 0 && c != CAUSA_TE_EXTRANA && c != CAUSA_NADA_QUE_HACER;
    case EMO_TRISTEZA: return s <= 0 && c != CAUSA_ZORRO && c != CAUSA_ABEJA;
    case EMO_MIEDO:
        return c == CAUSA_ZORRO || c == CAUSA_ABEJA || c == CAUSA_TORMENTA || c == CAUSA_LUGAR_FEO || c == CAUSA_TOQUE_FEO || c == CAUSA_SACUDIDA ||
               c == CAUSA_LA_DESPERTASTE || c == CAUSA_NOVEDAD || c == CAUSA_VISITA || c == CAUSA_INCLINAR || c == CAUSA_VIENTO || c == CAUSA_CRIATURA;
    case EMO_ABURRIMIENTO:
        return c == CAUSA_NADA_QUE_HACER || c == CAUSA_NO_JUGASTE || c == CAUSA_FALTA_CARINO || c == CAUSA_TE_EXTRANA || c == CAUSA_NUBLADO ||
               (s < 0 && (c == CAUSA_LLUVIA || c == CAUSA_NIEVE || c == CAUSA_FRIO)) || c == CAUSA_CALOR;
    case EMO_CANSANCIO:
        return c == CAUSA_SUENO || c == CAUSA_ENFERMA || c == CAUSA_CALOR || c == CAUSA_FRIO || c == CAUSA_LA_DESPERTASTE || c == CAUSA_JUGAMOS ||
               c == CAUSA_EXPLORAR || c == CAUSA_PESCA || c == CAUSA_HABILIDAD || c == CAUSA_HAMBRE || c == CAUSA_SACUDIDA || c == CAUSA_TORMENTA ||
               c == CAUSA_VIENTO || c == CAUSA_LLUVIA || c == CAUSA_NIEVE;
    case EMO_CELOS:
        return c == CAUSA_MIMASTE_OTRO || c == CAUSA_VISITA || c == CAUSA_AMIGO || c == CAUSA_VECINO || c == CAUSA_CARTA || c == CAUSA_FALTA_CARINO ||
               c == CAUSA_TE_EXTRANA;
    default: return true;
    }
}

static void sentir(const pet_t *p, char *buf, int n, bool corto)
{
    if (!buf || n <= 0) return;
    buf[0] = 0;
    if (p->etapa == STAGE_HUEVO) { snprintf(buf, (size_t)n, "Algo se mueve dentro del huevo..."); return; }
    if (p->dormida) { snprintf(buf, (size_t)n, "Zzz... estoy soñando."); return; }
    /* cambia con el tiempo, cada vez que lo cuenta y entre mascotas, sin gastar el azar de la partida */
    static uint32_t veces; /* cada vez que lo cuenta, de otra manera (aunque sea en el mismo minuto) */
    veces++;
    uint32_t v = (p->ultima_actualizacion / 180u + veces * 7919u) * 2654435761u ^ p->id * 40503u;
    v ^= v >> 13;
    emo_t a, b;
    int k = pet_top_emotions(p, &a, &b);
    int w;
    if (k == 0) {
        static const char *const CALMA[] = {"Me siento en calma.", "Todo está tranquilo.", "Estoy bien, gracias.", "Qué día más tranquilo."};
        w = snprintf(buf, (size_t)n, "%s", p->necesidad[NEED_SACIEDAD] < 25 ? "Tengo hambre." : CALMA[v % 4]);
    } else {
        const char *ca = cuadra(p, a, (causa_t)p->causa[a]) ? texto_causa(p, (causa_t)p->causa[a], v >> 3) : "";
        bool fuerte = (k < 2 || corto) && pet_emotion(p, a) > 0.75f; /* sola y muy fuerte: con exclamación */
        w = snprintf(buf, (size_t)n, "%s%s%s%s%s", fuerte ? "¡" : "", texto_emocion(a, pet_emotion(p, a), v), ca[0] ? " " : "", ca, fuerte ? "!" : "");
        /* la segunda emoción solo si la frase sigue teniendo sentido: si es contraria a la primera
         * (triste y feliz a la vez), tiene que tener su propio porqué y ser casi igual de fuerte */
        bool contraria = positiva(a) != positiva(b);
        if (k >= 2 && contraria && (!cuadra(p, b, (causa_t)p->causa[b]) || p->causa[b] == p->causa[a] || pet_emotion(p, b) < 0.6f * pet_emotion(p, a)))
            k = 1;
        if (k >= 2 && contraria && ((a == EMO_TRISTEZA && p->causa[a] == CAUSA_TE_EXTRANA && b == EMO_ALEGRIA && p->causa[b] == CAUSA_VOLVISTE) ||
                                     (b == EMO_TRISTEZA && p->causa[b] == CAUSA_TE_EXTRANA && a == EMO_ALEGRIA && p->causa[a] == CAUSA_VOLVISTE))) {
            w = snprintf(buf, (size_t)n, "%s", (v >> 7) % 2 ? "Te extrañé mucho, ¡pero ya estás aquí!" : "Me sentía solit{a}, ¡pero volviste!");
            k = 1;
        }
        if (k >= 2 && w > 0 && w < n && !corto) {
            /* la segunda emoción, en minúscula, unida con "y" o "pero" */
            char seg[64];
            snprintf(seg, sizeof seg, "%s", texto_emocion(b, pet_emotion(p, b), 0xFFFFFFFFu));
            if (seg[0] >= 'A' && seg[0] <= 'Z') seg[0] = (char)(seg[0] + 32);
            /* "me siento querido y me siento importante" suena raro: se dice "me siento querido y importante" */
            bool recorte = false;
            {
                const char *pre = NULL;
                if (!strncmp(seg, "me siento ", 10)) pre = "me siento ";
                else if (!strncmp(seg, "estoy ", 6)) pre = "estoy ";
                const bool abre_siento = strstr(buf, "Me siento ") != NULL, abre_estoy = strstr(buf, "Estoy ") != NULL;
                if (pre && ((pre[0] == 'm' && abre_siento) || (pre[0] == 'e' && (abre_estoy || abre_siento)))) { memmove(seg, seg + strlen(pre), strlen(seg + strlen(pre)) + 1); recorte = true; }
            }
            const char *cb = p->causa[b] != p->causa[a] && cuadra(p, b, (causa_t)p->causa[b]) ? texto_causa(p, (causa_t)p->causa[b], v >> 5) : "";
            bool igual = positiva(a) == positiva(b);
            w += snprintf(buf + w, (size_t)(n - w), "%s%s%s%s", igual ? ((v >> 7) % 2 && !recorte ? " y además " : " y ") : ((v >> 7) % 2 ? ", aunque " : ", pero "),
                          seg, cb[0] ? " " : "", cb);
        }
        /* si la frase terminaba en "!" no se agrega punto */
        if (w > 0 && w < n - 1 && buf[w - 1] != '!' && buf[w - 1] != '.') { snprintf(buf + w, (size_t)(n - w), "."); w++; }
    }
    char ctx[64];
    if (!corto && (v >> 9) % 3 != 0 && w > 0 && w < n - 2) { /* dos de cada tres veces, algo más */
        contexto(p, v >> 11, ctx, sizeof ctx);
        if (ctx[0]) snprintf(buf + w, (size_t)(n - w), " %s", ctx);
    }
}


void pet_feeling(const pet_t *p, char *buf, int n) { sentir(p, buf, n, false); genero_en(p, buf, n); }
void pet_feeling_short(const pet_t *p, char *buf, int n) { sentir(p, buf, n, true); genero_en(p, buf, n); }

/* ======================================================================
 * 10. Amigos cercanos
 * ==================================================================== */

#define AMIGO_CERCA_S 30u      /* se considera cerca si su baliza llegó hace menos de 30 s */
#define AMIGO_REAVISO_S 600u   /* avisar "amigo cerca" de nuevo tras 10 min sin verlo */
#define INVITACION_S 60u       /* cuánto dura una invitación o una respuesta */

static const char *const AMISTADES[] = {"jugaron", "hicieron un picnic", "se dieron regalos", "bailaron", "se enamoraron"};
const char *pet_friendship_name(amistad_t a) { return NOMBRE(AMISTADES, a, AMI_COUNT); }

static void copiar_nombre(char *dst, int cap, const char *src)
{
    memset(dst, 0, (size_t)cap);
    int i = 0;
    while (src[i]) {
        int largo = ((unsigned char)src[i] & 0xE0) == 0xC0 ? 2 : (((unsigned char)src[i] & 0xF0) == 0xE0 ? 3 : 1);
        if (i + largo > cap - 1) break;
        memcpy(dst + i, src + i, (size_t)largo);
        i += largo;
    }
}

void pet_make_baliza(const pet_t *p, uint32_t ahora, pet_baliza_t *b)
{
    memset(b, 0, sizeof(*b));
    b->id = p->id;
    b->especie = p->especie;
    b->etapa = p->etapa;
    b->forma = p->forma;
    b->animo = p->emocion;
    copiar_nombre(b->nombre, sizeof b->nombre, p->nombre);
    if (ahora < p->envio_hasta) {
        if (p->aceptando_a) { b->invita_a = p->aceptando_a; b->acepta = 1; }
        else b->invita_a = p->invitando_a;
        b->token = p->token;
    }
}

bool pet_baliza_encode(const pet_baliza_t *b, uint8_t o[PET_BALIZA_BYTES])
{
    memset(o, 0, PET_BALIZA_BYTES);
    o[0] = PET_BALIZA_MAGIC;
    for (int i = 0; i < 4; i++) { o[1 + i] = (uint8_t)(b->id >> (8 * i)); o[5 + i] = (uint8_t)(b->invita_a >> (8 * i)); }
    o[9] = (uint8_t)b->token;
    o[10] = (uint8_t)(b->token >> 8);
    o[11] = b->especie;
    o[12] = (uint8_t)((b->etapa & 7) | ((b->forma & 7) << 3) | (b->acepta ? 0x80 : 0));
    o[13] = b->animo;
    memcpy(o + 14, b->nombre, 9);
    return true;
}

bool pet_baliza_decode(const uint8_t *in, int n, pet_baliza_t *b)
{
    if (!in || n < PET_BALIZA_BYTES || in[0] != PET_BALIZA_MAGIC) return false;
    memset(b, 0, sizeof(*b));
    for (int i = 0; i < 4; i++) { b->id |= (uint32_t)in[1 + i] << (8 * i); b->invita_a |= (uint32_t)in[5 + i] << (8 * i); }
    b->token = (uint16_t)(in[9] | (in[10] << 8));
    b->especie = in[11];
    b->etapa = in[12] & 7;
    b->forma = (in[12] >> 3) & 7;
    b->acepta = (in[12] & 0x80) ? 1 : 0;
    b->animo = in[13];
    memcpy(b->nombre, in + 14, 9);
    b->nombre[9] = 0;
    if (b->id == 0 || b->especie >= N_ESPECIES || b->etapa >= STAGE_COUNT || b->animo >= MOOD_COUNT) return false;
    return true;
}

static int buscar_amigo(const pet_t *p, uint32_t id)
{
    for (int i = 0; i < PET_MAX_AMIGOS; i++)
        if (p->amigos[i].id == id) return i;
    return -1;
}

bool pet_friend_nearby(const pet_t *p, int i, uint32_t ahora)
{
    if (i < 0 || i >= PET_MAX_AMIGOS || !p->amigos[i].id) return false;
    return ahora - p->amigos[i].visto <= AMIGO_CERCA_S;
}

static bool puede_jugar(const pet_t *p) { return p->etapa != STAGE_HUEVO; }

/* la visita: ambos relojes calculan lo mismo con los dos ids y el token */
static void visita_amigo(pet_t *p, int i, uint16_t token, uint32_t t)
{
    amigo_t *a = &p->amigos[i];
    uint32_t lo = p->id < a->id ? p->id : a->id, hi = p->id < a->id ? a->id : p->id;
    uint32_t h = lo * 2654435761u ^ hi * 40503u ^ token * 2246822519u;
    h ^= h >> 15;
    h *= 2654435761u;
    h ^= h >> 13;
    amistad_t r = (amistad_t)(h % (AMI_COUNT - 1)); /* enamorarse es aparte */
    if (a->amistad >= 60 && (h >> 8) % 4 == 0) r = AMI_ENAMORARON;

    sumar(p, NEED_DIVERSION, 30);
    sumar(p, NEED_CARINO, 15);
    sumar(p, NEED_CURIOSIDAD, 20);
    impulsar(p, EMO_ALEGRIA, 0.8f, CAUSA_AMIGO);
    if (r == AMI_ENAMORARON) impulsar(p, EMO_CARINO, 0.9f, CAUSA_AMIGO);
    if (r == AMI_PICNIC) sumar(p, NEED_SACIEDAD, 15);
    if (r == AMI_REGALO) guardar_item(p, (item_t)(h % ITEM_COUNT));
    if (r == AMI_ENAMORARON) p->enamoramiento = clampf(p->enamoramiento + 0.5f, 0, 1);
    a->amistad = (uint8_t)(a->amistad + 10 > 100 ? 100 : a->amistad + 10);
    if (a->visitas < 0xFFFF) a->visitas++;
    p->ultima_interaccion = t;
    push_event(p, EV_AMIGO_VISITA, i, r, t);
    anotar(p, DIA_AMIGO, i, 0, t);
    sonar(p, r == AMI_ENAMORARON ? SND_SUSPIRO_AMOR : SND_RISA, t);
}

void pet_on_baliza(pet_t *p, const pet_baliza_t *b, uint32_t ahora)
{
    if (!b->id || b->id == p->id) return;
    int i = buscar_amigo(p, b->id);
    bool nuevo_cerca = false;
    if (i < 0) { /* nuevo: ocupar un lugar libre o el del amigo visto hace más tiempo */
        i = 0;
        for (int k = 0; k < PET_MAX_AMIGOS; k++) {
            if (!p->amigos[k].id) { i = k; break; }
            if (p->amigos[k].visto < p->amigos[i].visto) i = k;
        }
        memset(&p->amigos[i], 0, sizeof(amigo_t));
        p->amigos[i].id = b->id;
        nuevo_cerca = true;
    } else if (ahora - p->amigos[i].visto > AMIGO_REAVISO_S) {
        nuevo_cerca = true;
    }
    amigo_t *a = &p->amigos[i];
    copiar_nombre(a->nombre, sizeof a->nombre, b->nombre);
    a->especie = b->especie;
    a->etapa = b->etapa;
    a->forma = b->forma;
    a->animo = b->animo;
    a->visto = ahora;
    if (nuevo_cerca && puede_jugar(p)) push_event(p, EV_AMIGO_CERCA, i, 0, ahora);

    if (b->invita_a != p->id || !puede_jugar(p)) return;
    if (b->acepta) {
        /* aceptó nuestra invitación: jugamos y dejamos de invitar */
        if (p->invitando_a == b->id && b->token == p->token && ahora < p->envio_hasta) {
            p->invitando_a = 0;
            p->envio_hasta = 0;
            visita_amigo(p, i, b->token, ahora);
        }
    } else if (p->aceptando_a != b->id && (p->invitacion_de != b->id || ahora >= p->invitacion_hasta)) {
        /* una invitación nueva */
        p->invitacion_de = b->id;
        p->invitacion_hasta = ahora + INVITACION_S;
        p->token = b->token;
        a->invito = ahora;
        push_event(p, EV_INVITACION, i, 0, ahora);
        sonar(p, SND_LLAMADO, ahora);
    }
}

void pet_invite(pet_t *p, int i, uint32_t ahora)
{
    if (!puede_jugar(p) || i < 0 || i >= PET_MAX_AMIGOS || !p->amigos[i].id) return;
    p->invitando_a = p->amigos[i].id;
    p->aceptando_a = 0;
    p->token = (uint16_t)(rng_next(p) | 1u);
    p->envio_hasta = ahora + INVITACION_S;
}

int pet_pending_invite(const pet_t *p, uint32_t ahora)
{
    if (!p->invitacion_de || ahora >= p->invitacion_hasta) return -1;
    return buscar_amigo(p, p->invitacion_de);
}

void pet_answer_invite(pet_t *p, bool acepta, uint32_t ahora)
{
    int i = pet_pending_invite(p, ahora);
    p->invitacion_de = 0;
    if (i < 0 || !acepta || !puede_jugar(p)) return;
    /* respondemos "sí" durante un minuto y jugamos ya: el otro reloj juega al recibirlo */
    p->aceptando_a = p->amigos[i].id;
    p->invitando_a = 0;
    p->envio_hasta = ahora + INVITACION_S;
    visita_amigo(p, i, p->token, ahora);
}



/* --- visitas entre perfiles del mismo reloj --- */
static uint32_t id_perfil(const pet_t *q)
{
    uint32_t h = 2166136261u; /* los perfiles comparten el id del reloj: se distinguen por el nombre */
    for (const char *c = q->nombre; *c; c++) { h ^= (uint8_t)*c; h *= 16777619u; }
    return (q->id ^ h) | 1u;
}
static int amiga_perfil(pet_t *p, const pet_t *q, uint32_t t)
{
    pet_baliza_t b;
    pet_make_baliza(q, t, &b);
    b.id = id_perfil(q);
    b.invita_a = 0;
    b.acepta = 0;
    pet_on_baliza(p, &b, t);
    return buscar_amigo(p, b.id);
}
bool pet_profile_visit(pet_t *p, pet_t *otra, uint32_t ahora)
{
    if (!puede_jugar(p) || !puede_jugar(otra) || p == otra) return false;
    if (otra->ultima_actualizacion < ahora) pet_update(otra, ahora); /* se pone al día antes de salir */
    int i = amiga_perfil(p, otra, ahora), j = amiga_perfil(otra, p, ahora);
    if (i < 0 || j < 0) return false;
    uint16_t tok = (uint16_t)(rng_next(p) | 1u);
    visita_amigo(p, i, tok, ahora);
    visita_amigo(otra, j, tok, ahora);
    pet_seal(otra);
    return true;
}

/* --- las alarmas "suena siempre" de las que descansan --- */
int pet_alarm_resting(pet_t *p, uint32_t desde, uint32_t ahora)
{
    int64_t d = dia_local(p, ahora);
    for (int i = 0; i < PET_ALARMAS; i++) {
        if (!p->alarma[i].siempre) continue;
        for (int k = 0; k <= 1; k++) {
            uint32_t t = alarma_en_dia(p, i, d - k);
            if (!t || t <= desde || t > ahora || t == p->alarma_sono[i]) continue;
            p->alarma_sono[i] = t;
            if (!p->alarma[i].dias) p->alarma[i].activa = 0;
            if (ahora - t < 120) { pet_seal(p); return i; }
            break;
        }
    }
    return -1;
}

/* --- las fotos de la cámara de cada mascota --- */
int pet_photo_add(pet_t *p, int id)
{
    if (id <= 0) return 0;
    int viejo = 0;
    if (p->mis_fotos_n >= PET_FOTOS_PERFIL) {
        viejo = p->mis_fotos[0];
        memmove(p->mis_fotos, p->mis_fotos + 1, (PET_FOTOS_PERFIL - 1) * sizeof p->mis_fotos[0]);
        p->mis_fotos_n = PET_FOTOS_PERFIL - 1;
    }
    p->mis_fotos[p->mis_fotos_n++] = (uint16_t)id;
    return viejo;
}
bool pet_photo_mine(const pet_t *p, int id)
{
    if (id > 0 && id <= p->fotos_legado) return true;
    for (int i = 0; i < p->mis_fotos_n; i++) if (p->mis_fotos[i] == id) return true;
    return false;
}
void pet_photo_forget(pet_t *p, int id)
{
    for (int i = 0; i < p->mis_fotos_n; i++)
        if (p->mis_fotos[i] == id) { memmove(p->mis_fotos + i, p->mis_fotos + i + 1, (size_t)(p->mis_fotos_n - i - 1) * sizeof p->mis_fotos[0]); p->mis_fotos_n--; return; }
}
void pet_photo_legacy(pet_t *p, int hasta) { if (!p->fotos_legado_ok) { p->fotos_legado_ok = 1; p->fotos_legado = (uint16_t)(hasta < 0 ? 0 : hasta); } }

/* ------------------------------------------------------------------ la hora */

void pet_time_shift(pet_t *p, int32_t delta)
{
#define SH(x) do { if ((x)) (x) = (uint32_t)((int64_t)(x) + delta); } while (0)
    for (int i = 0; i < 2; i++) SH(p->dormir.aviso[i]);
    SH(p->nacimiento); SH(p->ultima_actualizacion); SH(p->ultima_interaccion); SH(p->ultimo_deseo_avisado);
    SH(p->enfermo_desde); SH(p->visita_hasta); SH(p->actividad_hasta); SH(p->ultimo_hallazgo);
    SH(p->ultimo_pensamiento); SH(p->celos_hasta); SH(p->ultimo_juego); SH(p->ultima_caricia); SH(p->bebida_hasta);
    SH(p->escena_desde);
    for (int i = 0; i < FAUNA_COUNT; i++) SH(p->fauna_hasta[i]);
    SH(p->zorro_roba); SH(p->charcos_hasta); SH(p->ultimas_hojas); SH(p->ultimo_truco); SH(p->ultimo_inclinar);
    SH(p->ultimo_lanzar); SH(p->raro_hasta); SH(p->ultimo_raro); SH(p->ropa_revisada); SH(p->eclosion); SH(p->clima_hasta);
    SH(p->invitacion_hasta); SH(p->envio_hasta);
    for (int i = 0; i < PET_ALARMAS; i++) { SH(p->alarma_sono[i]); SH(p->alarma_pospuesta[i]); }
    SH(p->ultimo_respaldo); SH(p->temporizador);
    SH(p->cabeza_hasta); SH(p->cielo_hasta); SH(p->compa_hasta); SH(p->suerte_hasta); SH(p->pesca_hasta); SH(p->protege_hasta);
    SH(p->doble_hasta); SH(p->invisible_hasta); SH(p->talla_hasta); SH(p->colores_hasta); SH(p->lentes_hasta); SH(p->iman_hasta);
    SH(p->ultimo_objeto); SH(p->hoja_hasta); SH(p->palo_hasta); SH(p->niebla_hasta); SH(p->linaje_hora);
    for (int i = 0; i < ANIMAL_V22; i++) SH(p->animal_hasta[i]);
    SH(p->ardilla_vuelve);
    for (int i = 0; i < JUG_COUNT; i++) SH(p->juguete_hasta[i]);
    SH(p->vecino.proxima); SH(p->vecino.hasta); SH(p->vecino.actividad_hasta); SH(p->vecino.se_fue);
#undef SH
    p->desfase_s += delta;
}

bool pet_clock_behind(const pet_t *p, uint32_t ahora) { return ahora + 120u < p->ultima_actualizacion; }

/* ------------------------------------------------------------------ respaldo */
static const char B64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

int pet_base64_encode(const uint8_t *in, int n, char *out, int cap)
{
    int k = 0;
    if (cap < 4 * ((n + 2) / 3) + 1) return -1;
    for (int i = 0; i < n; i += 3) {
        uint32_t v = (uint32_t)in[i] << 16 | (i + 1 < n ? (uint32_t)in[i + 1] << 8 : 0) | (i + 2 < n ? in[i + 2] : 0);
        out[k++] = B64[v >> 18 & 63];
        out[k++] = B64[v >> 12 & 63];
        out[k++] = i + 1 < n ? B64[v >> 6 & 63] : '=';
        out[k++] = i + 2 < n ? B64[v & 63] : '=';
    }
    out[k] = 0;
    return k;
}

int pet_base64_decode(const char *in, uint8_t *out, int cap)
{
    uint32_t v = 0;
    int bits = 0, k = 0;
    for (; *in; in++) {
        char c = *in;
        int d;
        if (c >= 'A' && c <= 'Z') d = c - 'A';
        else if (c >= 'a' && c <= 'z') d = c - 'a' + 26;
        else if (c >= '0' && c <= '9') d = c - '0' + 52;
        else if (c == '+' || c == '-') d = 62;
        else if (c == '/' || c == '_') d = 63;
        else if (c == '=') break;
        else if (c == ' ' || c == '\n' || c == '\r' || c == '\t') continue;
        else return -1;
        v = v << 6 | (uint32_t)d;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            if (k >= cap) return -1;
            out[k++] = (uint8_t)(v >> bits);
        }
    }
    return k;
}

int pet_backup_export(pet_t *p, char *out, int cap)
{
    pet_seal(p);
    int h = snprintf(out, (size_t)cap, "%s %d\n", PET_RESPALDO_CABECERA, PET_SAVE_VERSION);
    if (h <= 0 || h >= cap) return -1;
    int k = pet_base64_encode((const uint8_t *)p, (int)sizeof *p, out + h, cap - h - 1);
    if (k < 0) return -1;
    out[h + k] = '\n';
    out[h + k + 1] = 0;
    return h + k + 1;
}

bool pet_backup_import(const char *txt, pet_t *out)
{
    if (!txt || !out) return false;
    while (*txt == ' ' || *txt == '\n' || *txt == '\r') txt++;
    size_t hl = strlen(PET_RESPALDO_CABECERA);
    if (!strncmp(txt, PET_RESPALDO_CABECERA, hl)) { /* "MASCOTA-RESPALDO 11" y salto de línea */
        txt += hl;
        while (*txt && *txt != '\n') txt++;
    }
    static pet_t tmp; /* grande: fuera de la pila */
    memset(&tmp, 0, sizeof tmp);
    int n = pet_base64_decode(txt, (uint8_t *)&tmp, (int)sizeof tmp);
    if (n == (int)sizeof tmp && pet_validate(&tmp)) { /* misma versión */ }
    else if (n > 0 && n == (int)tmp.size && pet_upgrade(&tmp)) { /* un respaldo de una versión anterior */ }
    else return false;
    *out = tmp;
    out->ev_ini = out->ev_fin = 0; /* los avisos pendientes eran del otro reloj */
    return true;
}

#include "historia.inc"
#include "musica.inc"
#include "v39.inc"
#include "v40.inc"
#include "v41.inc"
#include "pochi.inc" /* (fw 102) */
#include "toques.inc" /* (fw 103) */
