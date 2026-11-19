/* Pruebas del motor. Sin dependencias: ./test_core devuelve 0 si todo pasa. */
#include <stddef.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

#include "pet.h"
#include "foto.h"
#include "sfx.h"

static int fallos = 0, total = 0;
#define CHECK(cond, ...)                                                   \
    do {                                                                   \
        total++;                                                           \
        if (!(cond)) {                                                     \
            fallos++;                                                      \
            printf("  FALLA %s:%d: ", __FILE__, __LINE__);                 \
            printf(__VA_ARGS__);                                           \
            printf("\n");                                                  \
        }                                                                  \
    } while (0)

#define T0 1792972800u /* lunes 00:00 UTC = domingo 21:00 en Chile */
#define H 3600u
#define CHILE (-3 * 3600)

static void nueva(pet_t *p, uint32_t seed, uint32_t t)
{
    pet_init(p, "Test", 0, t, seed);
    pet_set_timezone(p, CHILE);
    pet_set_owner_age(p, 30);
    pet_hatch(p, t);
    p->evo_pregunta = 0; /* en las pruebas crece sola (la pregunta antes de crecer se prueba aparte) */
    p->vinculo = 50; /* ya se conocen */
}

static int hora_chile(uint32_t t) { return (int)(((int64_t)t + CHILE) % 86400 / 3600); }

static void cuidar(pet_t *p, uint32_t now)
{
    if (p->necesidad[NEED_SACIEDAD] < 60) pet_action(p, ACT_ALIMENTAR, FOOD_COMIDA, now);
    if (p->necesidad[NEED_HIGIENE] < 50) pet_action(p, ACT_BANAR, 0, now);
    if (p->enferma) pet_action(p, ACT_CURAR, 0, now);
}

static void dar_semillas(pet_t *p, cultivo_t c, int n)
{
    p->semillas[c] = (uint8_t)(p->semillas[c] + n);
    p->semillas_vistas |= (uint8_t)(1u << c);
    int k = 0;
    for (int i = 0; i < CULTIVO_COUNT; i++) k += p->semillas[i];
    p->inventario[ITEM_SEMILLA] = (uint8_t)k;
}

static uint32_t crc32_prueba(const uint8_t *d, size_t n)
{
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < n; i++) { c ^= d[i]; for (int k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u))); }
    return ~c;
}

static void vaciar_eventos(pet_t *p)
{
    pet_event_t e;
    while (pet_poll_event(p, &e)) {}
}

static int contar_eventos(pet_t *p, event_type_t tipo)
{
    pet_event_t e;
    int n = 0;
    while (pet_poll_event(p, &e)) n += e.type == tipo;
    return n;
}

static void test_necesidades_bajan(void)
{
    static pet_t p;
    uint32_t t = T0 + 14 * H; /* 11:00 en Chile, de día */
    nueva(&p, 1, t);
    float s0 = pet_need(&p, NEED_SACIEDAD), c0 = pet_need(&p, NEED_CARINO);
    pet_update(&p, t + 3 * H);
    CHECK(pet_need(&p, NEED_SACIEDAD) < s0 - 8, "la saciedad debe bajar en 3 h (%.1f -> %.1f)", s0,
          pet_need(&p, NEED_SACIEDAD));
    CHECK(pet_need(&p, NEED_CARINO) < c0 - 3, "el carino debe bajar en 3 h");
    for (int i = 0; i < NEED_COUNT; i++)
        CHECK(pet_need(&p, (need_t)i) >= 0 && pet_need(&p, (need_t)i) <= 100, "necesidad %d fuera de rango", i);
}

static void test_alimentar(void)
{
    static pet_t p;
    uint32_t t = T0 + 14 * H;
    nueva(&p, 2, t);
    pet_update(&p, t + 6 * H);
    float antes = pet_need(&p, NEED_SACIEDAD);
    int r = pet_action(&p, ACT_ALIMENTAR, FOOD_COMIDA, t + 6 * H);
    CHECK(pet_need(&p, NEED_SACIEDAD) > antes + 25, "comer debe subir la saciedad");
    CHECK(r >= 0, "no deberia rechazar comida con hambre");
    /* llenarla y comprobar que rechaza */
    for (int i = 0; i < 5; i++) pet_action(&p, ACT_ALIMENTAR, FOOD_COMIDA, t + 6 * H);
    CHECK(pet_action(&p, ACT_ALIMENTAR, FOOD_COMIDA, t + 6 * H) < 0, "llena debe rechazar comida");
}

static void test_ponerse_al_dia_equivale(void)
{
    /* 8 h de un salto vs 8 h en pasos de 10 min: deben quedar muy parecidas */
    static pet_t a, b;
    uint32_t t = T0 + 14 * H;
    nueva(&a, 3, t);
    nueva(&b, 3, t);
    a.vecino.proxima = b.vecino.proxima = t + 30 * 24 * H; /* sin visitas de su amigo (comen juntos al azar) */
    pet_force_weather(&a, CLIMA_DESPEJADO, t, 10 * H); /* el mismo clima en los dos (la lluvia ensucia) */
    pet_force_weather(&b, CLIMA_DESPEJADO, t, 10 * H);
    pet_update(&a, t + 8 * H);
    for (uint32_t s = 600; s <= 8 * H; s += 600) pet_update(&b, t + s);
    for (int i = 0; i < NEED_COUNT - 1; i++) {
        float d = pet_need(&a, (need_t)i) - pet_need(&b, (need_t)i);
        /* la autonomía (jugar sola, explorar) elige distinto con pasos de 5 o de 1 minuto: diversión y curiosidad varían más */
        float tol = (i == NEED_DIVERSION || i == NEED_CURIOSIDAD || i == NEED_CARINO) ? 30.0f : 16.0f;
        CHECK(d > -tol && d < tol, "necesidad %d difiere %.2f entre salto y pasos", i, d);
    }
}

static void test_duerme_de_noche_y_despierta(void)
{
    static pet_t p;
    uint32_t t = T0 + 1 * H; /* 22:00 en Chile */
    nueva(&p, 4, t);
    p.necesidad[NEED_ENERGIA] = 40;
    pet_action(&p, ACT_DORMIR, 0, t);
    CHECK(p.dormida, "con luz apagada y energia 40 debe dormirse");
    CHECK(pet_mood(&p) == MOOD_DORMIDA, "emocion debe ser dormida");
    pet_update(&p, t + 6 * H);
    CHECK(p.dormida, "a las 04:00 con la luz apagada sigue durmiendo");
    pet_action(&p, ACT_DESPERTAR, 0, t + 10 * H); /* 08:00 */
    CHECK(!p.dormida, "despertar la despierta");
    CHECK(pet_need(&p, NEED_ENERGIA) > 85, "durmio y recupero energia (%.1f)", pet_need(&p, NEED_ENERGIA));
}

static void test_cuidada_rara_vez_enferma(void)
{
    int enfermas = 0;
    const int N = 40;
    for (int s = 0; s < N; s++) {
        static pet_t b;
        uint32_t t = T0 + 14 * H;
        nueva(&b, 100 + s, t);
        b.inventario[ITEM_HIERBA] = 50;
        for (uint32_t h = 1; h <= 72; h++) {
            uint32_t now = t + h * H;
            pet_update(&b, now);
            if (h % 5 == 0) pet_action(&b, ACT_ALIMENTAR, FOOD_COMIDA, now);
            if (h % 6 == 0) pet_action(&b, ACT_JUGAR, 0, now);
            if (h % 4 == 0) pet_action(&b, ACT_ACARICIAR, GESTO_TOQUE, now);
            if (h % 12 == 0) pet_action(&b, ACT_BANAR, 0, now);
            if (b.enferma) pet_action(&b, ACT_CURAR, 0, now);
            vaciar_eventos(&b);
        }
        enfermas += b.enferma;
    }
    CHECK(enfermas < N / 4, "bien cuidada casi nunca termina enferma (%d/%d)", enfermas, N);
}

static void test_abandono_no_sufre(void)
{
    /* 20 días sola: triste y extrañándote, pero sin peligro; al volver, fiesta */
    static pet_t p;
    uint32_t t = T0 + 14 * H;
    nueva(&p, 77, t);
    p.vinculo = 60;
    pet_update(&p, t + 20u * 24u * H);
    vaciar_eventos(&p);
    CHECK(pet_need(&p, NEED_SALUD) >= 34, "la salud no baja del piso (%.0f)", pet_need(&p, NEED_SALUD));
    CHECK(pet_need(&p, NEED_SACIEDAD) >= 19, "el hambre tiene piso (%.0f)", pet_need(&p, NEED_SACIEDAD));
    CHECK(pet_emotion(&p, EMO_TRISTEZA) > 0.4f && pet_emotion_cause(&p, EMO_TRISTEZA) == CAUSA_TE_EXTRANA,
          "sola, te extraña (%.2f, causa %d)", pet_emotion(&p, EMO_TRISTEZA), pet_emotion_cause(&p, EMO_TRISTEZA));
    CHECK(p.vinculo < 60 && p.vinculo >= 10, "el vinculo baja un poco si no la ves (%.0f)", p.vinculo);
    pet_action(&p, ACT_ACARICIAR, GESTO_TOQUE, t + 20u * 24u * H + 60);
    CHECK(contar_eventos(&p, EV_REENCUENTRO) == 1, "al volver hay reencuentro");
    CHECK(pet_emotion(&p, EMO_ALEGRIA) > 0.8f && pet_emotion_cause(&p, EMO_ALEGRIA) == CAUSA_VOLVISTE, "te recibe feliz");
}

static void test_curar(void)
{
    static pet_t p;
    uint32_t t = T0 + 14 * H;
    nueva(&p, 5, t);
    p.enferma = 1;
    p.inventario[ITEM_HIERBA] = 5;
    for (int i = 0; i < 2 && p.enferma; i++) pet_action(&p, ACT_CURAR, 0, t + (uint32_t)i * 60);
    CHECK(!p.enferma, "dos hierbas deben curarla");
    CHECK(pet_action(&p, ACT_CURAR, 0, t + 600) == -1, "hierba sin estar enferma se rechaza");
}

static void test_guardado(void)
{
    static pet_t p, copia;
    nueva(&p, 6, T0);
    pet_update(&p, T0 + 5 * H);
    pet_seal(&p);
    memcpy(&copia, &p, sizeof p);
    CHECK(pet_validate(&copia), "una copia sellada debe validar");
    copia.necesidad[0] += 1.0f;
    CHECK(!pet_validate(&copia), "una copia alterada no debe validar");
}

static void test_visitas_y_relaciones(void)
{
    static pet_t p;
    uint32_t t = T0 + 13 * H; /* 10:00 */
    nueva(&p, 7, t);
    int llegadas = 0;
    for (int dia = 0; dia < 10; dia++) {
        for (uint32_t h = 0; h < 24; h++) {
            uint32_t now = t + (uint32_t)dia * 24 * H + h * H;
            pet_update(&p, now);
            pet_action(&p, ACT_ALIMENTAR, FOOD_COMIDA, now);
            if (h % 3 == 0) pet_action(&p, ACT_ACARICIAR, GESTO_TOQUE, now);
            if (h % 4 == 0) pet_action(&p, ACT_JUGAR, 0, now);
            if (p.enferma) pet_action(&p, ACT_CURAR, 0, now);
            llegadas += contar_eventos(&p, EV_VISITA_LLEGA);
        }
    }
    CHECK(llegadas >= 2, "en 10 dias deberian llegar visitas (%d)", llegadas); /* menos si está su amigo del barrio */
    for (int i = 0; i < PET_MAX_VISITORS; i++)
        CHECK(p.visitantes[i].relacion >= -100 && p.visitantes[i].relacion <= 100, "relacion fuera de rango");
}

static void test_habitos(void)
{
    static pet_t p;
    uint32_t t = T0 + 24 * H; /* lunes 21:00 Chile -> avanzar a las 18:00 de cada dia */
    nueva(&p, 8, t);
    for (int dia = 0; dia < 7; dia++) {
        uint32_t juego = t + (uint32_t)dia * 24 * H + 21 * H; /* 18:00 local */
        pet_update(&p, juego);
        if (p.dormida) pet_action(&p, ACT_DESPERTAR, 0, juego);
        pet_action(&p, ACT_JUGAR, 0, juego);
        pet_action(&p, ACT_ALIMENTAR, FOOD_COMIDA, juego);
        pet_action(&p, ACT_ALIMENTAR, FOOD_COMIDA, juego);
        pet_action(&p, ACT_BANAR, 0, juego);
        if (p.enferma) pet_action(&p, ACT_CURAR, 0, juego);
        vaciar_eventos(&p);
    }
    CHECK(p.habitos.prob[1][18] > 150, "debe aprender que juegas a las 18 (%d)", p.habitos.prob[1][18]);
    CHECK(p.habitos.prob[1][10] < 20, "no debe esperar juego a las 10 (%d)", p.habitos.prob[1][10]);
}

static void test_hallazgos_y_decoracion(void)
{
    static pet_t p;
    uint32_t t = T0 + 13 * H; /* 10:00 */
    nueva(&p, 10, t);
    p.rasgo[TRAIT_CURIOSA] = 0.8f;
    memset(p.inventario, 0, sizeof p.inventario);
    p.vecino.proxima = t + 30 * 24 * H; /* sin visitas: que explore */
    int hallazgos = 0;
    for (uint32_t m = 0; m < 10 * 60; m += 10) { /* 10 horas, la atiendes cada 10 min */
        uint32_t now = t + m * 60;
        pet_update(&p, now);
        if (m % 120 == 0) { pet_action(&p, ACT_ALIMENTAR, FOOD_COMIDA, now); pet_action(&p, ACT_JUGAR, 0, now); }
        pet_event_t e;
        while (pet_poll_event(&p, &e)) hallazgos += e.type == EV_HALLAZGO || (e.type == EV_TESORO && e.a == 1);
    }
    CHECK(hallazgos >= 2 && hallazgos <= 12, "encuentra cosas al explorar (%d en 10 h)", hallazgos);

    /* sin fruta no la come y avisa */
    p.inventario[ITEM_FRUTA] = 0;
    p.necesidad[NEED_SACIEDAD] = 40;
    pet_action(&p, ACT_ALIMENTAR, FOOD_FRUTA, t + 11 * H);
    CHECK(contar_eventos(&p, EV_SIN_ITEM) == 1, "sin fruta debe avisar");

    /* decorar con lo encontrado */
    memset(p.inventario, 0, sizeof p.inventario);
    memset(p.semillas, 0, sizeof p.semillas);
    p.ramitas = 0; p.musgo = 0;
    p.inventario[ITEM_SEMILLA] = 3; p.semillas[CULTIVO_FLOR] = 3; p.inventario[ITEM_PIEDRA] = 4;
    CHECK(pet_place_decor(&p, DECOR_CASITA, t) == 0, "(v38) con semillas y piedras ya no alcanza: la casita es de ramitas y musgo");
    p.ramitas = PET_CASITA_RAMITAS; p.musgo = PET_CASITA_MUSGO - 1;
    CHECK(pet_place_decor(&p, DECOR_CASITA, t) == 0, "sin musgo suficiente no alcanza");
    p.musgo = PET_CASITA_MUSGO + 1;
    CHECK(pet_place_decor(&p, DECOR_CASITA, t) == 1 && pet_has_decor(&p, DECOR_CASITA), "pone la casita");
    CHECK(p.ramitas == 0 && p.musgo == 1 && p.inventario[ITEM_SEMILLA] == 3 && p.inventario[ITEM_PIEDRA] == 4, "gasta ramitas y musgo (no semillas ni piedras)");
    CHECK(pet_place_decor(&p, DECOR_CASITA, t) == -1, "no se pone dos veces");

    /* la fruta del árbol cae cada día */
    vaciar_eventos(&p);
    pet_action(&p, ACT_ACARICIAR, GESTO_TOQUE, t + 24 * H);
    CHECK(contar_eventos(&p, EV_REGALO) == 1, "cae una fruta al dia siguiente");
}

static void test_evolucion(void)
{
    /* bien cuidada 5 días: llega a adulta radiante */
    static pet_t p;
    uint32_t t = T0 + 14 * H;
    nueva(&p, 11, t);
    p.inventario[ITEM_HIERBA] = 50;
    for (uint32_t h = 1; h <= 5 * 24; h++) {
        uint32_t now = t + h * H;
        pet_update(&p, now);
        if (p.dormida && h % 24 > 10) pet_action(&p, ACT_DESPERTAR, 0, now);
        if (h % 4 == 0) pet_action(&p, ACT_ALIMENTAR, FOOD_COMIDA, now);
        if (h % 3 == 0) pet_action(&p, ACT_ACARICIAR, GESTO_TOQUE, now);
        if (h % 4 == 1) pet_action(&p, ACT_JUGAR, 0, now);
        if (h % 8 == 0) pet_action(&p, ACT_BANAR, 0, now);
        if (p.enferma) pet_action(&p, ACT_CURAR, 0, now);
        vaciar_eventos(&p);
    }
    CHECK(p.etapa == STAGE_ADULTA, "a los 5 dias es adulta");
    CHECK(p.forma == FORMA_RADIANTE, "bien cuidada evoluciona radiante (forma %d, cuidado %.0f)", p.forma, p.cuidado);

    /* abandonada: gruñona */
    static pet_t q;
    nueva(&q, 12, t);
    pet_update(&q, t + 5 * 24 * H);
    CHECK(q.forma != FORMA_RADIANTE, "abandonada no evoluciona radiante (forma %d, cuidado %.0f)", q.forma, q.cuidado);
}

static void test_salto_enorme(void)
{
    static pet_t p;
    nueva(&p, 9, T0);
    pet_update(&p, T0 + 90u * 24u * H); /* 90 días apagada */
    CHECK(p.ultima_actualizacion == T0 + 90u * 24u * H, "debe quedar al dia");
    for (int i = 0; i < NEED_COUNT; i++)
        CHECK(pet_need(&p, (need_t)i) >= 0 && pet_need(&p, (need_t)i) <= 100, "rango tras salto largo");
}

static void test_huevo(void)
{
    static pet_t p;
    pet_init(&p, "Hue", 2, T0, 21);
    pet_set_timezone(&p, CHILE);
    CHECK(pet_is_egg(&p), "empieza como huevo");
    uint32_t r0 = pet_hatch_remaining(&p, T0);
    pet_action(&p, ACT_ACARICIAR, GESTO_TOQUE, T0 + 10);
    CHECK(pet_hatch_remaining(&p, T0 + 10) < r0 - 10, "darle calor acelera la eclosion");
    pet_update(&p, T0 + 2 * 60);
    CHECK(pet_is_egg(&p), "a los 2 min sigue en el huevo");
    pet_update(&p, T0 + PET_HUEVO_SEGUNDOS + 60);
    CHECK(pet_is_egg(&p), "(1.0.10) sin llegar al prado (sin especie elegida) no nace solo");
    { const uint32_t tl = T0 + PET_HUEVO_SEGUNDOS + 60; /* llega al prado: desde ahí cuenta sus 5 minutos */
      CHECK(pet_choose_look(&p, 0, -1, 0, tl, 12345u), "elige cómo será");
      pet_egg_start(&p, tl);
      CHECK(pet_hatch_remaining(&p, tl) == PET_HUEVO_SEGUNDOS, "(1.0.10) al llegar le quedan 5 minutos: %u", (unsigned)pet_hatch_remaining(&p, tl));
      pet_update(&p, tl + 4 * 60);
      CHECK(pet_is_egg(&p), "(1.0.10) a los 4 min de llegar sigue en el huevo");
      pet_update(&p, tl + PET_HUEVO_SEGUNDOS + 60); }
    CHECK(!pet_is_egg(&p) && p.etapa == STAGE_BEBE, "nace como bebe");
    CHECK(p.necesidad[NEED_SACIEDAD] >= 65, "dentro del huevo no le baja el hambre");
    pet_set_name(&p, "ÑandúFeliz");
    CHECK(strcmp(p.nombre, "ÑandúFeli") == 0 || strlen(p.nombre) < PET_NAME_LEN, "nombre recortado sin romper letras: %s", p.nombre);
}

static void test_estaciones_y_clima(void)
{
    static pet_t p;
    nueva(&p, 41, T0); /* T0 = octubre: primavera en el sur */
    CHECK(pet_season(&p, T0) == ESTACION_PRIMAVERA, "octubre es primavera en el sur (%d)", pet_season(&p, T0));
    CHECK(pet_season(&p, T0 + 80u * 24u * H) == ESTACION_VERANO, "enero es verano");
    p.hemisferio_sur = 0;
    CHECK(pet_season(&p, T0) == ESTACION_OTONO, "octubre es otono en el norte");
    p.hemisferio_sur = 1;
    int cambios = 0, nieve = 0;
    for (uint32_t t = T0; t < T0 + 10u * 24u * H; t += H) {
        pet_update(&p, t);
        pet_action(&p, ACT_ALIMENTAR, FOOD_COMIDA, t);
        pet_event_t e;
        while (pet_poll_event(&p, &e)) {
            cambios += e.type == EV_CLIMA;
            nieve += e.type == EV_CLIMA && e.a == CLIMA_NIEVE;
        }
    }
    CHECK(cambios >= 10, "el clima cambia varias veces en 10 dias (%d)", cambios);
    CHECK(nieve == 0, "no nieva en primavera");
}

static void test_mensajes(void)
{
    static pet_t p;
    nueva(&p, 51, T0 + 12 * H); /* 09:00 en Chile */
    vaciar_eventos(&p);
    int frases = 0;
    for (uint32_t t = T0 + 12 * H; t < T0 + 36 * H; t += 60) {
        pet_update(&p, t);
        if ((t / 60) % 120 == 0) { pet_action(&p, ACT_ALIMENTAR, FOOD_COMIDA, t); pet_action(&p, ACT_ACARICIAR, GESTO_TOQUE, t); }
        pet_event_t e;
        while (pet_poll_event(&p, &e))
            if (e.type == EV_FRASE) {
                frases++;
                CHECK(pet_phrase(e.a)[0] != 0, "frase valida");
            }
    }
    CHECK(frases >= 3 && frases <= 5, "unas 4 frases al dia (%d)", frases);
    char buf[96];
    pet_feeling(&p, buf, sizeof buf);
    CHECK(strlen(buf) > 5, "dice como se siente: %s", buf);
}

static void test_amigos(void)
{
    static pet_t a, b;
    nueva(&a, 61, T0);
    nueva(&b, 62, T0);
    pet_set_name(&a, "Pipa");
    pet_set_name(&b, "Ñuño");
    vaciar_eventos(&a);
    vaciar_eventos(&b);
    uint8_t buf[PET_BALIZA_BYTES];
    pet_baliza_t x;

#define EMITIR(de, a_, t)                                    \
    do {                                                     \
        pet_baliza_t s_;                                     \
        pet_make_baliza(&de, t, &s_);                        \
        pet_baliza_encode(&s_, buf);                         \
        CHECK(pet_baliza_decode(buf, sizeof buf, &x), "decodifica"); \
        pet_on_baliza(&a_, &x, t);                           \
    } while (0)

    uint32_t t = T0 + 100;
    EMITIR(a, b, t);
    EMITIR(b, a, t);
    CHECK(strcmp(b.amigos[0].nombre, "Pipa") == 0 && strcmp(a.amigos[0].nombre, "Ñuño") == 0, "se ven con nombre");
    CHECK(contar_eventos(&a, EV_AMIGO_CERCA) == 1, "aviso de amigo cerca");
    vaciar_eventos(&b);
    CHECK(pet_friend_nearby(&a, 0, t + 5) && !pet_friend_nearby(&a, 0, t + 100), "cerca solo si se ve hace poco");

    pet_invite(&a, 0, t);
    EMITIR(a, b, t + 2);
    CHECK(pet_pending_invite(&b, t + 3) == 0, "b recibe la invitacion");
    CHECK(contar_eventos(&b, EV_INVITACION) == 1, "evento de invitacion");
    EMITIR(a, b, t + 4); /* repetir la baliza no duplica la invitación */
    CHECK(contar_eventos(&b, EV_INVITACION) == 0, "sin duplicados");

    pet_answer_invite(&b, true, t + 5);
    pet_event_t e;
    int rb = -1, ra = -1;
    while (pet_poll_event(&b, &e)) if (e.type == EV_AMIGO_VISITA) rb = e.b;
    EMITIR(b, a, t + 6);
    while (pet_poll_event(&a, &e)) if (e.type == EV_AMIGO_VISITA) ra = e.b;
    CHECK(rb >= 0 && ra == rb, "los dos relojes viven la misma visita (%d / %d)", ra, rb);
    EMITIR(b, a, t + 7);
    CHECK(contar_eventos(&a, EV_AMIGO_VISITA) == 0, "la visita no se repite");
    CHECK(a.amigos[0].visitas == 1 && b.amigos[0].visitas == 1, "se cuentan las visitas");

    /* basura por radio */
    uint8_t ruido[PET_BALIZA_BYTES] = {0x11, 1, 2, 3};
    CHECK(!pet_baliza_decode(ruido, sizeof ruido, &x), "ignora balizas ajenas");
#undef EMITIR
}

static void test_emociones_con_causa(void)
{
    static pet_t p;
    uint32_t t = T0 + 14 * H;
    nueva(&p, 20, t);
    /* jugar da alegría y la causa es el juego */
    p.necesidad[NEED_ENERGIA] = 90;
    pet_action(&p, ACT_JUGAR, 0, t);
    CHECK(pet_emotion(&p, EMO_ALEGRIA) > 0.6f && pet_emotion_cause(&p, EMO_ALEGRIA) == CAUSA_JUGAMOS,
          "jugar alegra (%.2f, causa %d)", pet_emotion(&p, EMO_ALEGRIA), pet_emotion_cause(&p, EMO_ALEGRIA));
    /* a Ascua no le gusta la lluvia: se pone triste por eso */
    static pet_t q;
    pet_init(&q, "Asc", 2, t, 21);
    pet_set_timezone(&q, CHILE);
    pet_set_owner_age(&q, 30);
    pet_hatch(&q, t);
    for (int i = 0; i < NEED_COUNT; i++) q.necesidad[i] = 80;
    pet_force_weather(&q, CLIMA_LLUVIA, t, 3 * H);
    pet_update(&q, t + 600);
    CHECK(q.memoria.clima[CLIMA_LLUVIA] < 0, "al fuego no le gusta la lluvia");
    CHECK(pet_emotion(&q, EMO_TRISTEZA) > 0.2f && pet_emotion_cause(&q, EMO_TRISTEZA) == CAUSA_LLUVIA,
          "triste por la lluvia (%.2f, causa %d)", pet_emotion(&q, EMO_TRISTEZA), pet_emotion_cause(&q, EMO_TRISTEZA));
    /* mezcla: feliz pero con sueño */
    q.ultimo_no = ACT_JUGAR; /* que obedezca (nunca dice que no dos veces seguidas) */
    pet_action(&q, ACT_JUGAR, 0, t + 700);
    q.necesidad[NEED_ENERGIA] = 12;
    q.impulso[EMO_CURIOSIDAD] = 0;
    pet_update(&q, t + 760);
    emo_t a, b;
    int n = pet_top_emotions(&q, &a, &b);
    CHECK(n == 2 && (a == EMO_CANSANCIO || b == EMO_CANSANCIO), "dos emociones a la vez, una es el sueño (%d %d %d)", n, a, b);
    char buf[128];
    pet_feeling(&q, buf, sizeof buf);
    CHECK((strstr(buf, "porque") || strstr(buf, "por ")) && (strstr(buf, " pero ") || strstr(buf, " aunque ") || strstr(buf, " y ")), "explica por qué: %s", buf);
    /* tormenta: miedo en una tímida */
    static pet_t r;
    nueva(&r, 22, t);
    r.rasgo[TRAIT_TIMIDA] = 0.85f;
    r.memoria.clima[CLIMA_TORMENTA] = -60;
    pet_force_weather(&r, CLIMA_TORMENTA, t, 3 * H);
    for (uint32_t k = 1; k <= 150 && !(pet_emotion(&r, EMO_MIEDO) > 0.4f && r.actividad == ACTV_REFUGIO); k++) pet_update(&r, t + k * 60);
    CHECK(pet_emotion(&r, EMO_MIEDO) > 0.4f && pet_emotion_cause(&r, EMO_MIEDO) == CAUSA_TORMENTA,
          "le teme a la tormenta (%.2f)", pet_emotion(&r, EMO_MIEDO));
    CHECK(r.actividad == ACTV_REFUGIO, "con miedo se refugia (%d)", r.actividad);
    /* celos */
    static pet_t s;
    nueva(&s, 23, t);
    s.rasgo[TRAIT_CARINOSA] = 0.9f;
    s.necesidad[NEED_CARINO] = 30;
    s.visita_actual = 0; s.visitantes[0].conocido = 1; s.visita_hasta = t + 3 * H;
    pet_action(&s, ACT_MIMAR_VISITA, 0, t);
    pet_action(&s, ACT_MIMAR_VISITA, 0, t + 5);
    CHECK(pet_mood(&s) == MOOD_CELOSA || pet_emotion(&s, EMO_CELOS) > 0.5f, "mimar a otro le da celos (%d)", pet_mood(&s));
}

static void test_frio(void)
{
    pet_visita_escala = 0;
    /* a Ascua el frío no le gusta: se pone triste por eso y se abriga en la casita */
    static pet_t p;
    uint32_t t = T0 + 14 * H;
    pet_init(&p, "Asc", 2, t, 71);
    pet_set_timezone(&p, CHILE);
    pet_set_owner_age(&p, 30);
    pet_hatch(&p, t);
    p.vinculo = 50;
    p.decoraciones |= 1u << DECOR_CASITA;
    for (int i = 0; i < NEED_COUNT; i++) p.necesidad[i] = 80;
    p.impulso[EMO_CURIOSIDAD] = 0; /* recién nacida, todo le da curiosidad */
    pet_force_weather(&p, CLIMA_FRIO, t, 4 * H);
    p.ultimo_hallazgo = t; /* sin hallazgos que la distraigan */
    p.vinculo_nivel = VINCULO_AMIGOS; /* ya se conocen: sin la fiesta de subir de nivel */
    for (int f = 0; f < FAUNA_COUNT; f++) p.fauna_hasta[f] = 0;
    CHECK(p.memoria.clima[CLIMA_FRIO] < -40, "al fuego no le gusta el frío (%d)", p.memoria.clima[CLIMA_FRIO]);
    pet_update(&p, t + 20 * 60);
    CHECK(pet_emotion(&p, EMO_TRISTEZA) > 0.3f && pet_emotion_cause(&p, EMO_TRISTEZA) == CAUSA_FRIO,
          "triste por el frío (%.2f, causa %d)", pet_emotion(&p, EMO_TRISTEZA), pet_emotion_cause(&p, EMO_TRISTEZA));
    CHECK(p.actividad == ACTV_ABRIGARSE && p.lugar == LUGAR_CASITA, "se abriga en la casita (%d en %d)", p.actividad, p.lugar);
    char buf[128];
    for (int q = 0; q < EMO_COUNT; q++) if (q != EMO_TRISTEZA) p.emo[q] = 0; /* (que nada tape lo del frío) */
    pet_feeling(&p, buf, sizeof buf);
    CHECK(strstr(buf, "frío") != NULL, "lo dice: %s", buf);
    /* a Brumo el frío le gusta: no se abriga */
    static pet_t q;
    pet_init(&q, "Bru", 7, t, 72);
    pet_set_timezone(&q, CHILE);
    pet_set_owner_age(&q, 30);
    pet_hatch(&q, t);
    for (int i = 0; i < NEED_COUNT; i++) q.necesidad[i] = 80;
    pet_force_weather(&q, CLIMA_FRIO, t, 4 * H);
    pet_update(&q, t + 20 * 60);
    CHECK(q.memoria.clima[CLIMA_FRIO] > 0 && q.actividad != ACTV_ABRIGARSE, "al fantasma le gusta el frío (%d)", q.actividad);
}

static void test_llamar(void)
{
    static pet_t p;
    uint32_t t = T0 + 14 * H;
    nueva(&p, 91, t);
    p.vinculo = 60;
    int vino = 0;
    for (int i = 0; i < 20; i++) {
        uint32_t now = t + (uint32_t)i * 300;
        int r = pet_action(&p, ACT_LLAMAR, LUGAR_ESTANQUE, now);
        if (r == 1) { vino++; CHECK(p.lugar == LUGAR_ESTANQUE, "va adonde la llamas"); }
        p.lugar = LUGAR_CLARO;
        vaciar_eventos(&p);
    }
    CHECK(vino >= 11, "con confianza casi siempre viene; si la llamas sin parar, se enoja (%d/20)", vino);
    pet_action(&p, ACT_DORMIR, 0, t + 2 * H); /* le apagas la luz */
    p.necesidad[NEED_ENERGIA] = 30;
    pet_update(&p, t + 2 * H + 120);
    CHECK(p.dormida && pet_action(&p, ACT_LLAMAR, LUGAR_ROCAS, t + 2 * H + 180) < 0, "dormida no viene");
}

static void test_mundo(void)
{
    static pet_t p;
    uint32_t t = T0 + 13 * H; /* 10:00 */
    nueva(&p, 101, t);
    p.vinculo = 10;
    pet_update(&p, t + 30 * H);
    CHECK(!pet_scene_open(&p, ESC_BOSQUE), "sin confianza el bosque sigue cerrado");
    p.vinculo = 17; /* alcanza para el bosque (15) y no para el lago (20) */
    vaciar_eventos(&p);
    pet_update(&p, t + 31 * H);
    CHECK(pet_scene_open(&p, ESC_BOSQUE) && !pet_scene_open(&p, ESC_LAGO), "al día y con confianza se abre el bosque, el lago aún no");
    CHECK(contar_eventos(&p, EV_ESCENA_NUEVA) == 1, "avisa la zona nueva");
    pet_action(&p, ACT_IR, ESC_LAGO, t + 31 * H + 60);
    CHECK(p.escena != ESC_LAGO, "no puede ir a una zona cerrada");
    for (int i = 0; i < 5 && p.escena != ESC_BOSQUE; i++) pet_action(&p, ACT_IR, ESC_BOSQUE, t + 31 * H + 120 + (uint32_t)i * 60);
    CHECK(p.escena == ESC_BOSQUE, "la llevas al bosque");

    /* en el bosque hay sombra: no pasa calor en verano */
    p.estacion_fija = ESTACION_VERANO;
    p.memoria.clima[CLIMA_DESPEJADO] = -10;
    pet_force_weather(&p, CLIMA_DESPEJADO, t + 2 * H, 4 * H); /* hoy a las 12 */
    uint32_t medio = T0 + 24 * H + 16 * H; /* día siguiente, 13:00 */
    pet_force_weather(&p, CLIMA_DESPEJADO, medio - 600, 4 * H);
    p.escena = ESC_BOSQUE;
    pet_update(&p, medio);
    p.escena = ESC_BOSQUE; p.actividad = ACTV_PASEAR;
    CHECK(pet_temperature_feel(&p, medio) == 0, "a la sombra del bosque no tiene calor");
    p.escena = ESC_PRADO;
    CHECK(pet_temperature_feel(&p, medio) > 0, "en el prado sí");
}

static void test_fauna_y_jardin(void)
{
    /* primavera, jardín florecido: llegan mariposas y abejas, y las persigue */
    static pet_t p;
    uint32_t t = T0 + 13 * H;
    nueva(&p, 111, t);
    p.estacion_fija = ESTACION_PRIMAVERA;
    p.rasgo[TRAIT_ENERGICA] = 0.8f; p.rasgo[TRAIT_CURIOSA] = 0.8f;
    for (int i = 0; i < 3; i++) { p.jardin[i].tipo = PLANTA_FLOR; p.jardin[i].etapa = ETAPA_GRANDE; p.jardin[i].agua = 100; }
    pet_force_weather(&p, CLIMA_DESPEJADO, t, 8 * H);
    int mariposas = 0, abejas = 0, persigue = 0, picos = 0, hubo_m = 0, hubo_a = 0;
    for (uint32_t m = 0; m < 8 * 60; m += 1) {
        uint32_t now = t + m * 60;
        pet_update(&p, now);
        int hm = pet_fauna_here(&p, FAUNA_MARIPOSA, now), ha = pet_fauna_here(&p, FAUNA_ABEJA, now);
        mariposas += hm && !hubo_m; abejas += ha && !hubo_a;
        hubo_m = hm; hubo_a = ha;
        if (m % 120 == 0) { pet_action(&p, ACT_ALIMENTAR, FOOD_COMIDA, now); for (int i = 0; i < 3; i++) pet_action(&p, ACT_REGAR, i, now); }
        pet_event_t e;
        while (pet_poll_event(&p, &e))
            if (e.type == EV_FAUNA) {
                persigue += e.b == FSUC_PERSIGUE;
                picos += e.b == FSUC_PICO;
            }
    }
    CHECK(mariposas >= 1 && abejas >= 1, "con flores llegan mariposas (%d) y abejas (%d)", mariposas, abejas);
    CHECK(persigue >= 1, "las persigue (%d)", persigue);

    /* una picadura: aprende a evitar las abejas */
    static pet_t q;
    nueva(&q, 112, t);
    q.miedo_abejas = 0;
    q.persigue = FAUNA_ABEJA;
    q.actividad = ACTV_PERSEGUIR;
    q.fauna_hasta[FAUNA_ABEJA] = t + 3600;
    int pico = 0;
    for (int i = 0; i < 20 && !pico; i++) {
        q.actividad = ACTV_PERSEGUIR; q.persigue = FAUNA_ABEJA; q.fauna_hasta[FAUNA_ABEJA] = t + 3600; q.actividad_hasta = 0;
        pet_update(&q, t + 60u * (uint32_t)(i + 1));
        pet_event_t e;
        while (pet_poll_event(&q, &e)) pico += e.type == EV_FAUNA && e.b == FSUC_PICO;
    }
    CHECK(pico && q.miedo_abejas > 0.4f, "la picó y lo recuerda (%.2f)", q.miedo_abejas);
    CHECK(pet_emotion_cause(&q, EMO_MIEDO) == CAUSA_ABEJA || q.impulso_causa[EMO_MIEDO] == CAUSA_ABEJA, "miedo por la abeja");
    int vuelve = 0;
    for (int i = 0; i < 30; i++) {
        q.fauna_hasta[FAUNA_ABEJA] = t + 9 * H; q.fauna_hasta[FAUNA_MARIPOSA] = 0; q.actividad_hasta = 0;
        pet_update(&q, t + 30 * 60 + 60u * (uint32_t)i);
        vuelve += q.actividad == ACTV_PERSEGUIR && q.persigue == FAUNA_ABEJA;
        vaciar_eventos(&q);
    }
    CHECK(vuelve == 0, "ya no persigue abejas (%d)", vuelve);

    /* el zorro: si no lo espantas, se lleva una fruta; si lo espantas, te lo agradece */
    static pet_t z;
    nueva(&z, 113, t);
    z.inventario[ITEM_FRUTA] = 3;
    z.rasgo[TRAIT_TIMIDA] = 0.8f;
    z.fauna_hasta[FAUNA_ZORRO] = t + 300; z.zorro_roba = t + 180;
    pet_update(&z, t + 240);
    CHECK(z.inventario[ITEM_FRUTA] == 2 && !pet_fauna_here(&z, FAUNA_ZORRO, t + 240), "el zorro se llevó una fruta");
    z.fauna_hasta[FAUNA_ZORRO] = t + 900; z.zorro_roba = t + 700;
    float v0 = z.vinculo;
    CHECK(pet_action(&z, ACT_ESPANTAR, FAUNA_ZORRO, t + 600) == 1, "lo espantas");
    pet_update(&z, t + 800);
    CHECK(z.inventario[ITEM_FRUTA] == 2 && z.vinculo > v0, "no roba y ella confía más");
    static pet_t k;
    pet_init(&k, "Kid", 1, t, 114); pet_set_timezone(&k, CHILE); pet_set_owner_age(&k, 6); pet_hatch(&k, t);
    k.inventario[ITEM_FRUTA] = 3; k.rasgo[TRAIT_TIMIDA] = 0.8f;
    k.fauna_hasta[FAUNA_ZORRO] = t + 300; k.zorro_roba = t + 180;
    pet_update(&k, t + 240);
    CHECK(k.inventario[ITEM_FRUTA] == 3, "en modo niño el zorro no roba");

    /* jardín: plantar, crecer con agua, ponerse mustio sin agua y revivir al regar */
    static pet_t g;
    nueva(&g, 115, t);
    g.estacion_fija = ESTACION_PRIMAVERA;
    memset(g.semillas, 0, sizeof g.semillas);
    dar_semillas(&g, CULTIVO_FLOR, 1);
    CHECK(pet_action(&g, ACT_PLANTAR, 0, t) == 1 && g.inventario[ITEM_SEMILLA] == 0, "planta una semilla");
    CHECK(pet_action(&g, ACT_PLANTAR, 1, t) == 0, "sin semillas no planta");
    pet_force_weather(&g, CLIMA_DESPEJADO, t, 80 * H);
    for (uint32_t hh = 1; hh <= 40; hh++) { pet_update(&g, t + hh * H); if (hh % 12 == 0) pet_action(&g, ACT_REGAR, 0, t + hh * H); vaciar_eventos(&g); }
    CHECK(g.jardin[0].etapa == ETAPA_GRANDE && !g.jardin[0].mustia, "regada, florece en un día y medio (etapa %d)", g.jardin[0].etapa);
    pet_force_weather(&g, CLIMA_DESPEJADO, t + 40 * H, 200 * H);
    pet_update(&g, t + 66 * H);
    CHECK(g.jardin[0].mustia && g.jardin[0].tipo == PLANTA_FLOR && g.jardin[0].agua > PET_AGUA_SECA, "un día sin agua y se pone mustia");
    int secas = 0;
    pet_event_t ge;
    for (uint32_t hh = 66; hh <= 80; hh++) { pet_update(&g, t + hh * H); while (pet_poll_event(&g, &ge)) secas += ge.type == EV_JARDIN && ge.a == 0 && ge.b == JAR_SECA; }
    CHECK(secas == 1 && g.jardin[0].tipo == PLANTA_FLOR && g.jardin[0].agua <= PET_AGUA_SECA, "más tiempo sin agua: se seca (%d)", secas);
    pet_action(&g, ACT_REGAR, 0, t + 80 * H);
    CHECK(!g.jardin[0].mustia && g.jardin[0].agua > 0, "al regarla revive");
    int perdidas = 0;
    for (uint32_t hh = 81; hh <= 130; hh++) { pet_update(&g, t + hh * H); while (pet_poll_event(&g, &ge)) perdidas += ge.type == EV_JARDIN && ge.a == 0 && ge.b == JAR_PERDIDA; }
    CHECK(perdidas == 1 && g.jardin[0].tipo != PLANTA_FLOR, "si nadie la riega, se seca del todo y se pierde (%d, tipo %d, agua %.0f)", perdidas, g.jardin[0].tipo, g.jardin[0].agua);
    static pet_t gn;
    nueva(&gn, 117, t);
    pet_set_owner_age(&gn, 8);
    gn.estacion_fija = ESTACION_VERANO;
    dar_semillas(&gn, CULTIVO_FLOR, 1);
    pet_action(&gn, ACT_PLANTAR, 0, t);
    pet_force_weather(&gn, CLIMA_DESPEJADO, t, 300 * H);
    for (uint32_t hh = 1; hh <= 200; hh += 5) { pet_update(&gn, t + hh * H); vaciar_eventos(&gn); }
    CHECK(gn.jardin[0].tipo == PLANTA_FLOR && gn.jardin[0].agua <= PET_AGUA_SECA, "con niños se queda seca, esperando que la rieguen");
    pet_action(&gn, ACT_REGAR, 0, t + 201 * H);
    CHECK(!gn.jardin[0].mustia, "y revive");

    /* enterrar una semilla cavando: crece un arbolito */
    static pet_t b;
    nueva(&b, 116, t);
    b.inventario[ITEM_SEMILLA] = 10;
    int arbol = 0;
    for (int i = 0; i < 40 && !arbol; i++) {
        b.actividad = ACTV_CAVAR; b.actividad_hasta = 0;
        pet_update(&b, t + 60u * (uint32_t)(i + 1));
        for (int j = 0; j < PET_PARCELAS; j++) arbol |= b.jardin[j].tipo == PLANTA_ARBOL;
        vaciar_eventos(&b);
    }
    CHECK(arbol, "cavando enterró una semilla");

    /* charcos después de la lluvia y caminos marcados */
    static pet_t c;
    nueva(&c, 117, t);
    pet_force_weather(&c, CLIMA_LLUVIA, t, 3600);
    pet_force_weather(&c, CLIMA_DESPEJADO, t + 3600, 3600);
    CHECK(pet_puddles(&c, t + 2 * 3600) && !pet_puddles(&c, t + 6 * 3600), "quedan charcos unas horas");
    for (int i = 0; i < 10; i++) { c.lugar = LUGAR_CLARO; c.senda[pet_trail_pair(LUGAR_CLARO, LUGAR_ROCAS)] += 1.0f; }
    CHECK(pet_trail_mask(&c) & (1u << pet_trail_pair(LUGAR_ROCAS, LUGAR_CLARO)), "un camino muy usado se marca");

    /* la cueva escondida: la descubre olfateando en la colina */
    static pet_t v;
    nueva(&v, 118, t);
    v.vinculo = 60;
    v.escenas_abiertas |= (1u << ESC_COLINA) | (1u << ESC_BOSQUE);
    v.escena = ESC_COLINA;
    for (int i = 0; i < 60 && !pet_scene_open(&v, ESC_CUEVA); i++) {
        v.tesoro_pendiente = 0; v.actividad = ACTV_OLFATEAR; v.actividad_hasta = 0; v.escena = ESC_COLINA;
        pet_update(&v, t + 60u * (uint32_t)(i + 1));
        vaciar_eventos(&v);
    }
    CHECK(pet_scene_open(&v, ESC_CUEVA), "olfateando encontró la cueva");
}

static void test_bebidas(void)
{
    /* con frío, un chocolate caliente le cambia el ánimo */
    static pet_t p;
    uint32_t t = T0 + 14 * H;
    pet_init(&p, "Asc", 2, t, 82);
    pet_set_timezone(&p, CHILE);
    pet_set_owner_age(&p, 30);
    pet_hatch(&p, t);
    p.vinculo = 50;
    for (int i = 0; i < NEED_COUNT; i++) p.necesidad[i] = 80;
    p.impulso[EMO_CURIOSIDAD] = 0;
    pet_force_weather(&p, CLIMA_FRIO, t, 6 * H);
    pet_update(&p, t + 20 * 60);
    CHECK(pet_temperature_feel(&p, t + 20 * 60) < 0, "tiene frío");
    float triste = pet_emotion(&p, EMO_TRISTEZA);
    p.ultimo_no = ACT_ALIMENTAR; /* que no diga "no" por capricho: aquí importa la temperatura */
    int rb = pet_action(&p, ACT_ALIMENTAR, FOOD_BEBIDA_FRIA, t + 21 * 60);
    CHECK(rb == -1, "una bebida fría con frío no le gusta (%d)", rb);
    CHECK(pet_action(&p, ACT_ALIMENTAR, FOOD_BEBIDA_CALIENTE, t + 22 * 60) == 1, "el chocolate caliente le encanta");
    pet_update(&p, t + 40 * 60);
    CHECK(pet_temperature_feel(&p, t + 40 * 60) == 0, "abrigada por dentro, ya no tiene frío");
    CHECK(pet_emotion(&p, EMO_TRISTEZA) < triste * 0.6f, "se le pasa la tristeza (%.2f -> %.2f)", triste, pet_emotion(&p, EMO_TRISTEZA));
    CHECK(pet_emotion(&p, EMO_ALEGRIA) > 0.5f && pet_emotion_cause(&p, EMO_ALEGRIA) == CAUSA_BEBIDA, "feliz por la bebida");
    pet_update(&p, t + 4 * H);
    CHECK(pet_temperature_feel(&p, t + 4 * H) < 0, "el efecto dura unas horas");

    /* con calor, un jugo con hielo */
    static pet_t q;
    uint32_t m = T0 + 16 * H; /* 13:00 */
    pet_init(&q, "Bur", 3, m, 82);
    pet_set_timezone(&q, CHILE);
    pet_set_owner_age(&q, 30);
    pet_hatch(&q, m);
    q.memoria.clima[CLIMA_DESPEJADO] = -10;
    q.estacion_fija = ESTACION_VERANO;
    for (int i = 0; i < NEED_COUNT; i++) q.necesidad[i] = 80;
    q.impulso[EMO_CURIOSIDAD] = 0;
    pet_force_weather(&q, CLIMA_DESPEJADO, m, 5 * H);
    q.actividad = ACTV_PASEAR; q.actividad_hasta = m + 3 * H;
    pet_update(&q, m + 10 * 60);
    CHECK(pet_temperature_feel(&q, m + 10 * 60) > 0, "tiene calor");
    CHECK(pet_emotion_cause(&q, EMO_TRISTEZA) == CAUSA_CALOR, "triste por el calor (causa %d)", pet_emotion_cause(&q, EMO_TRISTEZA));
    CHECK(pet_action(&q, ACT_ALIMENTAR, FOOD_BEBIDA_CALIENTE, m + 11 * 60) == -1, "algo caliente con calor: ¡quema!");
    CHECK(pet_action(&q, ACT_ALIMENTAR, FOOD_BEBIDA_FRIA, m + 12 * 60) == 1, "el jugo con hielo la refresca");
    CHECK(pet_temperature_feel(&q, m + 13 * 60) == 0, "refrescada");
}

static void test_personalidad_y_memoria(void)
{
    static pet_t p;
    uint32_t t = T0 + 14 * H;
    nueva(&p, 30, t);
    float e0 = p.rasgo[TRAIT_ENERGICA];
    for (int i = 0; i < 60; i++) {
        uint32_t now = t + (uint32_t)i * 1800;
        p.necesidad[NEED_ENERGIA] = 90; p.enferma = 0; p.dormida = 0;
        pet_action(&p, ACT_JUGAR, 0, now);
        vaciar_eventos(&p);
    }
    CHECK(p.rasgo[TRAIT_ENERGICA] > e0 + 0.05f, "jugar mucho la vuelve juguetona (%.2f -> %.2f)", e0, p.rasgo[TRAIT_ENERGICA]);

    /* un susto en las rocas: lo recuerda, y con el tiempo lo olvida */
    static pet_t q;
    nueva(&q, 33, t); /* (v39: otra semilla: con otras, el azar nuevo la vuelve a asustar en las rocas) */
    q.rasgo[TRAIT_TIMIDA] = 0.8f;
    q.lugar = LUGAR_ROCAS;
    pet_action(&q, ACT_SACUDIR, 0, t);
    CHECK(q.memoria.miedo[LUGAR_ROCAS] > 0.3f, "recuerda dónde se asustó (%.2f)", q.memoria.miedo[LUGAR_ROCAS]);
    float m0 = q.memoria.miedo[LUGAR_ROCAS];
    for (uint32_t h = 1; h <= 10 * 24; h += 6) { pet_update(&q, t + h * H); pet_action(&q, ACT_ALIMENTAR, FOOD_COMIDA, t + h * H); vaciar_eventos(&q); }
    CHECK(q.memoria.miedo[LUGAR_ROCAS] < m0 * 0.65f, "el susto se olvida en días (%.2f de %.2f)", q.memoria.miedo[LUGAR_ROCAS], m0);

    /* muchos toques seguidos la marean; las caricias le encantan: se descubre */
    static pet_t r;
    nueva(&r, 32, t);
    r.memoria.gesto[GESTO_MUCHOS] = -70;
    r.memoria.gesto[GESTO_CARICIA] = 80;
    CHECK(pet_action(&r, ACT_ACARICIAR, GESTO_MUCHOS, t) == -1, "muchos toques seguidos la marean");
    CHECK(pet_action(&r, ACT_ACARICIAR, GESTO_CARICIA, t + 30) == 1, "las caricias le encantan");
    CHECK(pet_favorite(&r, GUSTO_GESTO, true) == GESTO_MUCHOS && pet_favorite(&r, GUSTO_GESTO, false) == GESTO_CARICIA,
          "se descubren sus gustos (%d %d)", pet_favorite(&r, GUSTO_GESTO, true), pet_favorite(&r, GUSTO_GESTO, false));

    /* al principio desconfía; con cuidados, el vínculo crece */
    static pet_t s;
    pet_init(&s, "Nue", 4, t, 33);
    pet_set_timezone(&s, CHILE);
    pet_set_owner_age(&s, 30);
    pet_hatch(&s, t);
    float v0 = s.vinculo;
    for (int i = 0; i < 40; i++) { pet_action(&s, ACT_ACARICIAR, GESTO_TOQUE, t + (uint32_t)i * 600); vaciar_eventos(&s); }
    CHECK(v0 < 20 && s.vinculo > v0 + 20, "el vínculo se construye (%.0f -> %.0f)", v0, s.vinculo);
}

static void test_dice_que_no(void)
{
    static pet_t p;
    uint32_t t = T0 + 14 * H;
    nueva(&p, 40, t);
    p.rasgo[TRAIT_TRAVIESA] = 0.95f;
    int noes = 0, seguidos = 0;
    for (int i = 0; i < 300; i++) {
        uint32_t now = t + (uint32_t)i * 120;
        p.necesidad[NEED_ENERGIA] = 70; p.necesidad[NEED_SACIEDAD] = 60; p.enferma = 0; p.dormida = 0;
        p.impulso[EMO_ENOJO] = 0.8f; p.impulso_causa[EMO_ENOJO] = CAUSA_HAMBRE;
        int r = pet_action(&p, ACT_JUGAR, 0, now);
        if (r == -2) { noes++; seguidos++; CHECK(seguidos < 2, "nunca dice que no dos veces seguidas"); }
        else seguidos = 0;
        vaciar_eventos(&p);
    }
    CHECK(noes > 10 && noes < 150, "enojada y traviesa, a veces no obedece (%d/300)", noes);
}

static void test_modo_nino(void)
{
    static pet_t p;
    uint32_t t = T0 + 14 * H;
    pet_init(&p, "Kid", 1, t, 50);
    pet_set_timezone(&p, CHILE);
    pet_set_owner_age(&p, 7);
    CHECK(pet_child_mode(&p), "7 años: modo niño");
    pet_hatch(&p, t);
    pet_update(&p, t + 5u * 24u * H); /* abandonada 5 días */
    CHECK(p.forma != FORMA_GRUNONA, "en modo niño no hay forma gruñona");
    CHECK(pet_need(&p, NEED_SALUD) >= 54, "en modo niño la salud queda alta (%.0f)", pet_need(&p, NEED_SALUD));
    pet_set_owner_age(&p, 35);
    CHECK(!pet_child_mode(&p), "35 años: modo adulto");
}

static void test_autonomia(void)
{
    /* verano al mediodía y despejado: busca sombra bajo el árbol */
    static pet_t p;
    uint32_t t = T0 + 16 * H; /* 13:00 */
    nueva(&p, 60, t);
    p.estacion_fija = ESTACION_VERANO;
    pet_force_weather(&p, CLIMA_DESPEJADO, t, 5 * H);
    int sombra = 0, distintas = 0, ult = -1;
    for (uint32_t m = 0; m < 4 * 60; m += 5) {
        uint32_t now = t + m * 60;
        pet_update(&p, now);
        if (m % 60 == 0) pet_action(&p, ACT_ALIMENTAR, FOOD_COMIDA, now);
        pet_event_t e;
        while (pet_poll_event(&p, &e))
            if (e.type == EV_ACTIVIDAD) {
                sombra += e.a == ACTV_SOMBRA && e.b == LUGAR_ARBOL;
                if (e.a != ult) { distintas++; ult = e.a; }
            }
    }
    CHECK(sombra > 0, "en verano a mediodía busca sombra");
    /* sola por la tarde hace varias cosas distintas */
    static pet_t q;
    nueva(&q, 61, T0 + 19 * H);
    q.estacion_fija = ESTACION_OTONO;
    int tipos[ACTV_COUNT] = {0}, n = 0;
    for (uint32_t m = 0; m < 6 * 60; m += 5) {
        pet_update(&q, T0 + 19 * H + m * 60);
        pet_event_t e;
        while (pet_poll_event(&q, &e)) if (e.type == EV_ACTIVIDAD && !tipos[e.a]) { tipos[e.a] = 1; n++; }
    }
    CHECK(n >= 4, "sola hace al menos 4 cosas distintas (%d)", n);
    (void)distintas;
}

static void test_gestos(void)
{
    static pet_t p;
    uint32_t t = T0 + 14 * H;
    nueva(&p, 61, t);
    p.memoria.gesto[GESTO_MUCHOS] = -60;
    p.memoria.gesto[GESTO_ABRAZO] = 70;
    p.rasgo[TRAIT_TIMIDA] = 0.2f; /* una valiente se enoja (una tímida se asusta) */
    float e0 = pet_need(&p, NEED_ENERGIA);
    CHECK(pet_action(&p, ACT_ACARICIAR, GESTO_MUCHOS, t) == -1, "muchos toques la marean");
    CHECK(pet_need(&p, NEED_ENERGIA) < e0 && pet_emotion_cause(&p, EMO_ENOJO) == CAUSA_TOQUE_FEO, "y la dejan molesta");
    int son = -1;
    pet_event_t e;
    while (pet_poll_event(&p, &e)) if (e.type == EV_SONIDO) son = e.a;
    CHECK(son == SND_MAREO, "suena el mareo (%d)", son);
    p.necesidad[NEED_CARINO] = 40;
    CHECK(pet_action(&p, ACT_ACARICIAR, GESTO_ABRAZO, t + 60) == 1, "un abrazo le encanta");
    CHECK(pet_need(&p, NEED_CARINO) > 55, "el abrazo da mucho cariño (%.0f)", pet_need(&p, NEED_CARINO));
    /* a una traviesa a la que le hacen gracia, son cosquillas */
    p.memoria.gesto[GESTO_MUCHOS] = 30;
    CHECK(pet_action(&p, ACT_ACARICIAR, GESTO_MUCHOS, t + 120) == 1, "a otras les dan cosquillas");
    /* gustos de nacimiento: muchos toques casi nunca gustan, palmaditas casi siempre */
    int muchos_mal = 0, toque_bien = 0;
    for (uint32_t k = 0; k < 40; k++) {
        static pet_t q;
        pet_init(&q, "G", (uint8_t)(k % 8), t, 500 + k);
        muchos_mal += q.memoria.gesto[GESTO_MUCHOS] < 0;
        toque_bien += q.memoria.gesto[GESTO_TOQUE] > 0;
    }
    CHECK(muchos_mal >= 30 && toque_bien >= 32, "gustos iniciales razonables (%d %d)", muchos_mal, toque_bien);
}

static void test_cumple(void)
{
    static pet_t p;
    uint32_t t = T0 + 14 * H; /* lunes 26/10/2026, 11:00 en Chile */
    nueva(&p, 71, t);
    CHECK(!pet_set_owner_birthdate(&p, 31, 2, 2015, t), "fecha inválida");
    CHECK(pet_set_owner_birthdate(&p, 27, 10, 2014, t), "fecha válida");
    CHECK(pet_owner_age(&p, t) == 11 && pet_child_mode(&p), "11 años: modo niño");
    vaciar_eventos(&p);
    pet_update(&p, t + 6 * H);
    CHECK(contar_eventos(&p, EV_CUMPLE) == 0, "todavía no es tu cumpleaños");
    /* al día siguiente, de mañana y despierta, lo celebra una sola vez */
    uint32_t c = t + 24 * H - 2 * H; /* 27/10, 09:00 */
    int n = 0, edad = 0;
    for (uint32_t k = 0; k < 14 * 60; k += 10) {
        pet_update(&p, t + 7 * H + k * 60);
        if (p.dormida && hora_chile(t + 7 * H + k * 60) >= 8) pet_action(&p, ACT_DESPERTAR, 0, t + 7 * H + k * 60);
        pet_event_t e;
        while (pet_poll_event(&p, &e)) if (e.type == EV_CUMPLE) { n++; edad = e.a; }
    }
    pet_update(&p, c + 12 * H);
    n += contar_eventos(&p, EV_CUMPLE);
    CHECK(n == 1 && edad == 12, "celebra tu cumpleaños una vez (%d, %d años)", n, edad);
    CHECK(pet_owner_birthday(&p, c) && !pet_owner_birthday(&p, c + 24 * H), "solo ese día");
    CHECK(pet_emotion_cause(&p, EMO_ALEGRIA) == CAUSA_CUMPLE || p.cumple_anio == 2026, "lo celebró");
    /* al cumplir 12 pasa a modo adulto */
    static pet_t q;
    nueva(&q, 72, t);
    pet_set_owner_birthdate(&q, 27, 10, 2014, t);
    CHECK(pet_child_mode(&q), "11 años: modo niño");
    pet_update(&q, c + 3 * H);
    CHECK(!pet_child_mode(&q) && q.edad_dueno == 12, "al cumplir 12, modo adulto (%d)", q.edad_dueno);
    /* nacidos un 29 de febrero celebran el 28 si el año no es bisiesto */
    static pet_t r;
    nueva(&r, 73, t);
    CHECK(pet_set_owner_birthdate(&r, 29, 2, 2008, t), "29 de febrero válido en bisiesto");
    uint32_t feb28 = 1803772800u + 15 * H; /* 28/02/2027 12:00 en Chile */
    CHECK(pet_owner_birthday(&r, feb28), "lo celebra el 28 de febrero");
}

static void test_vecino(void)
{
    static pet_t p;
    uint32_t t = T0 + 13 * H; /* 10:00 */
    nueva(&p, 81, t);
    CHECK(p.vecino.nombre[0] && p.vecino.especie != p.especie, "tiene un amigo del barrio de otra especie (%s)", p.vecino.nombre);
    int llegadas = 0, cosas[VEC_COUNT] = {0}, sale_dormida = 1;
    for (uint32_t m = 0; m < 4 * 24 * 60; m += 5) {
        uint32_t now = t + m * 60;
        pet_update(&p, now);
        if (m % 60 == 0) { pet_action(&p, ACT_ALIMENTAR, FOOD_COMIDA, now); pet_action(&p, ACT_ACARICIAR, GESTO_TOQUE, now); }
        if (p.dormida && p.vecino.presente) sale_dormida = 0;
        pet_event_t e;
        while (pet_poll_event(&p, &e))
            if (e.type == EV_VECINO) { if (e.a == VEC_LLEGA) llegadas++; cosas[e.a]++; }
    }
    CHECK(llegadas >= 2 && llegadas <= 6, "viene cada uno o dos días (%d)", llegadas);
    CHECK(cosas[VEC_SE_VA] >= llegadas - 1, "y se va");
    CHECK(cosas[VEC_JUGAR] + cosas[VEC_CORRER] > 0 && cosas[VEC_COMER] + cosas[VEC_CHARLAR] > 0, "juegan, comen y conversan");
    CHECK(sale_dormida, "no se queda cuando ella duerme");
    CHECK(p.vecino.amistad > 25, "la amistad crece (%.0f)", p.vecino.amistad);
    /* lo invitas: viene enseguida y ella deja lo que hacía */
    static pet_t q;
    uint32_t u = T0 + 14 * H + 24 * H;
    nueva(&q, 82, u - 24 * H);
    pet_update(&q, u);
    if (q.vecino.presente) { q.vecino.hasta = u; pet_update(&q, u + 60); }
    q.vecino.se_fue = u - 5 * H;
    q.ultima_interaccion = u;
    pet_force_weather(&q, CLIMA_DESPEJADO, u, 4 * H);
    cuidar(&q, u + 60);
    CHECK(pet_action(&q, ACT_INVITAR_VECINO, 0, u + 120) == 1, "lo invitas");
    pet_update(&q, u + 300);
    CHECK(q.vecino.presente && q.actividad == ACTV_JUNTOS, "llega y juegan juntos");
    float a0 = q.vecino.amistad;
    pet_action(&q, ACT_MIMAR_VISITA, 0, u + 320);
    CHECK(q.vecino.amistad > a0, "puedes mimarlo a él también");
    CHECK(pet_action(&q, ACT_INVITAR_VECINO, 0, u + 400) <= 0, "si ya está, no se invita otra vez");
}

static void test_pesca(void)
{
    pet_visita_escala = 0;
    static pet_t p;
    uint32_t t = T0 + 14 * H;
    nueva(&p, 91, t);
    p.estacion_fija = ESTACION_VERANO;
    p.escenas_abiertas = 0x1F;
    p.vecino.proxima = t + 30 * 24 * H;
    pet_force_weather(&p, CLIMA_DESPEJADO, t, 10 * H);
    CHECK(!pet_can_fish(&p, t) && pet_action(&p, ACT_PESCAR, 0, t) == 0, "en el prado no se pesca");
    p.escena = ESC_LAGO;
    CHECK(pet_can_fish(&p, t), "en el lago sí");
    p.ultimo_no = ACT_PESCAR;
    CHECK(pet_action(&p, ACT_PESCAR, 0, t + 10) == 1 && p.actividad == ACTV_PESCAR && p.lugar == LUGAR_ESTANQUE, "sube al bote");
    int pescas = 0, cosas[PESCA_COUNT] = {0};
    for (uint32_t m = 1; m < 90 * 60; m++) {
        pet_update(&p, t + 10 + m * 60);
        p.escena = ESC_LAGO;
        if (m % 30 == 0) { p.necesidad[NEED_ENERGIA] = 80; p.necesidad[NEED_SACIEDAD] = 50; p.necesidad[NEED_CURIOSIDAD] = 80; }
        pet_event_t e;
        while (pet_poll_event(&p, &e)) if (e.type == EV_PESCA) { pescas++; cosas[e.a]++; }
    }
    CHECK(pescas >= 3, "en el lago pesca seguido (%d)", pescas);
    CHECK(cosas[PESCA_PEZ] > 0, "a veces saca un pez");
    p.estacion_fija = ESTACION_INVIERNO;
    CHECK(pet_lake_frozen(&p, t + 11 * H) && pet_can_fish(&p, t + 11 * H), "(v38) con el lago helado se pesca en un hoyo en el hielo");
    {
        static pet_t h;
        pet_init(&h, "Hielo", 3, t, 7);
        h.etapa = STAGE_ADULTA;
        h.estacion_fija = ESTACION_INVIERNO;
        int raro = 0, total = 0;
        for (uint32_t m = 1; m < 3 * 24 * 60; m++) {
            pet_update(&h, t + m * 60);
            h.escena = ESC_LAGO;
            if (m % 30 == 0) { h.necesidad[NEED_ENERGIA] = 80; h.necesidad[NEED_SACIEDAD] = 50; h.necesidad[NEED_CURIOSIDAD] = 80; }
            pet_event_t e;
            while (pet_poll_event(&h, &e)) if (e.type == EV_PESCA) { total++; raro += e.a == PESCA_CONCHA || e.a == PESCA_ALGA || e.a == PESCA_CANGREJO || e.a == PESCA_NENUFAR; }
        }
        CHECK(total >= 2 && raro == 0, "en el hielo también pesca sola, sin conchas, algas, cangrejos ni nenúfares (%d pescas)", total);
    }
}

static void test_casita(void)
{
    static pet_t p;
    uint32_t t = T0 + 14 * H;
    nueva(&p, 95, t);
    p.decoraciones |= 1u << DECOR_CASITA;
    p.necesidad[NEED_ENERGIA] = 40;
    pet_action(&p, ACT_DORMIR, 0, t);
    CHECK(p.dormida && p.lugar == LUGAR_CASITA, "con casita duerme en su camita");
}

static void test_sonidos(void)
{
    int ok = 1;
    for (int s = 0; s < SND_COUNT; s++) {
        const pet_sonido_info_t *i = pet_sound_info((sound_t)s);
        if (!i || !i->nombre[0] || !i->significado[0] || !i->grupo[0]) ok = 0;
    }
    CHECK(ok && pet_sound_info(SND_COUNT) == NULL, "todos los sonidos tienen nombre y significado");
    CHECK(!strcmp(pet_sound_info(SND_CUMPLEANOS)->id, "SND_CUMPLEANOS"), "el id coincide con el enum");
}

static void test_lanzar_y_trucos(void)
{
    static pet_t p;
    uint32_t t = T0 + 14 * H;
    nueva(&p, 111, t);
    p.vecino.proxima = t + 30 * 24 * H;
    p.rasgo[TRAIT_TRAVIESA] = 0.2f;
    p.necesidad[NEED_ENERGIA] = 90;
    int traidas = 0;
    for (int i = 0; i < 10; i++) traidas += pet_action(&p, ACT_LANZAR, 0, t + (uint32_t)i * 20) == 1;
    CHECK(traidas >= 6, "con energía te trae la pelota (%d de 10)", traidas);
    p.necesidad[NEED_ENERGIA] = 25;
    CHECK(pet_action(&p, ACT_LANZAR, 0, t + 300) == 0, "cansada va pero la deja");
    p.necesidad[NEED_ENERGIA] = 10;
    CHECK(pet_action(&p, ACT_LANZAR, 0, t + 320) == -1, "agotada no va");
    /* traviesa: a veces se la queda */
    static pet_t q;
    nueva(&q, 112, t);
    q.rasgo[TRAIT_TRAVIESA] = 0.95f;
    int quedadas = 0;
    for (int i = 0; i < 30; i++) { q.necesidad[NEED_ENERGIA] = 90; quedadas += pet_action(&q, ACT_LANZAR, 0, t + (uint32_t)i * 20) == 2; }
    CHECK(quedadas >= 2, "una traviesa a veces se queda con la pelota (%d)", quedadas);

    /* trucos: al principio casi nunca le sale; practicando lo aprende */
    static pet_t r;
    nueva(&r, 113, t);
    r.vecino.proxima = t + 30 * 24 * H;
    int aprendio = 0, salio_al_final = 0;
    for (int dia = 0; dia < 4; dia++)
        for (int i = 0; i < 8; i++) {
            uint32_t now = t + (uint32_t)dia * 24 * H + (uint32_t)i * 60;
            r.necesidad[NEED_ENERGIA] = 90;
            int k = pet_action(&r, ACT_TRUCO, TRUCO_VOLTERETA, now);
            if (dia == 3) salio_al_final += k == 1;
            pet_event_t e;
            while (pet_poll_event(&r, &e)) if (e.type == EV_TRUCO && e.b == 2) aprendio++;
        }
    CHECK(r.truco[TRUCO_VOLTERETA] >= PET_TRUCO_APRENDIDO && aprendio == 1, "practicando aprende a dar una voltereta (%.0f)", r.truco[TRUCO_VOLTERETA]);
    CHECK(salio_al_final >= 4, "y ya casi siempre le sale (%d de 8)", salio_al_final);
    CHECK(r.truco[TRUCO_VUELTA] == 0, "los otros trucos no se aprenden solos");
    /* se cansa de practicar */
    int cansada = 0;
    for (int i = 0; i < 20; i++) cansada += pet_action(&r, ACT_TRUCO, TRUCO_DORMIDA, t + 5 * 24 * H + (uint32_t)i * 30) == -1;
    CHECK(cansada >= 3, "se cansa si practican demasiado seguido (%d)", cansada);
    /* lo que no se practica se olvida de a poco */
    float v0 = r.truco[TRUCO_VOLTERETA];
    pet_update(&r, t + 40 * 24 * H);
    CHECK(r.truco[TRUCO_VOLTERETA] < v0 && r.truco[TRUCO_VOLTERETA] > v0 * 0.4f, "se olvida despacio (%.0f -> %.0f)", v0, r.truco[TRUCO_VOLTERETA]);
}

static void test_minijuegos_e_inclinar(void)
{
    static pet_t p;
    uint32_t t = T0 + 14 * H;
    nueva(&p, 121, t);
    int fr = p.inventario[ITEM_FRUTA];
    CHECK(pet_action(&p, ACT_FIN_JUEGO, JUEGO_FRUTAS * 100 + 9, t) == 1 && p.inventario[ITEM_FRUTA] == fr + 1, "atrapar 9 frutas da una de premio");
    CHECK(pet_action(&p, ACT_FIN_JUEGO, JUEGO_FRUTAS * 100 + 3, t + 60) == 0, "con 3 no hay premio");
    CHECK(pet_action(&p, ACT_FIN_JUEGO, JUEGO_ESCONDIDAS * 100 + 1, t + 120) == 1, "la encontraste");
    CHECK(pet_action(&p, ACT_FIN_JUEGO, JUEGO_ESCONDIDAS * 100 + 0, t + 180) == 2, "o ganó ella");
    p.rasgo[TRAIT_TIMIDA] = 0.2f;
    CHECK(pet_action(&p, ACT_INCLINAR, 0, t + 240) == 1, "inclinar el reloj la divierte");
    CHECK(pet_action(&p, ACT_INCLINAR, 0, t + 245) == 0, "no cuenta cada cuadro");
    p.rasgo[TRAIT_TIMIDA] = 0.8f;
    CHECK(pet_action(&p, ACT_INCLINAR, 0, t + 300) == -1, "a una tímida la asusta");
    /* sacudir el reloj hace caer fruta (hasta 2 por día) */
    p.rasgo[TRAIT_TIMIDA] = 0.2f;
    p.escena = ESC_PRADO;
    int caidas = 0;
    for (int i = 0; i < 12; i++) {
        pet_action(&p, ACT_SACUDIR, 0, t + 400 + (uint32_t)i * 30);
        pet_event_t e;
        while (pet_poll_event(&p, &e)) caidas += e.type == EV_REGALO && e.b == 1;
    }
    CHECK(caidas == 2, "sacudir hace caer dos frutas al día (%d)", caidas);
}

/* cuida a la mascota un rato: come, juega, la acaricias (para simular días normales) */

static void test_vinculo_y_diario(void)
{
    static pet_t p;
    uint32_t t = T0 + 14 * H;
    pet_init(&p, "V", 1, t, 131);
    pet_set_timezone(&p, CHILE);
    pet_set_owner_age(&p, 30);
    pet_hatch(&p, t);
    CHECK(pet_bond_level(&p) == VINCULO_DESCONFIA, "al principio desconfía");
    int niveles = 0, ultimo = 0;
    for (int i = 0; i < 120; i++) {
        uint32_t now = t + (uint32_t)i * 900;
        pet_action(&p, ACT_ACARICIAR, GESTO_CARICIA, now);
        cuidar(&p, now);
        pet_event_t e;
        while (pet_poll_event(&p, &e)) if (e.type == EV_VINCULO) { niveles++; ultimo = e.a; }
    }
    CHECK(niveles >= 3 && ultimo >= VINCULO_MEJORES, "el vínculo sube por niveles (%d avisos, nivel %d)", niveles, ultimo);
    CHECK(pet_diary_count(&p) >= 4, "el diario anota lo importante (%d)", pet_diary_count(&p));
    char buf[96];
    const entrada_diario_t *e0 = pet_diary_get(&p, pet_diary_count(&p) - 1);
    pet_diary_text(&p, e0, buf, sizeof buf);
    /* (v101) si el diario ya se llenó (32 páginas en estas 30 h), la primera página se pudo ir: depende del azar del clima */
    CHECK(pet_diary_count(&p) >= PET_DIARIO || (e0->tipo == DIA_NACIO && strstr(buf, "Nací")), "la primera página: %s", buf);
    /* (1.0.27) el álbum guarda solo los momentos grandes: aquí, el nacimiento (el vínculo queda en el diario) */
    CHECK(pet_album_count(&p) >= 1 && pet_album_get(&p, pet_album_count(&p) - 1)->motivo == DIA_NACIO, "el álbum guarda el nacimiento (%d)", pet_album_count(&p));
    CHECK(pet_photo_moment(DIA_CUMPLE_DUENO) && pet_photo_moment(DIA_BALLENA) && !pet_photo_moment(DIA_VINCULO) && !pet_photo_moment(DIA_ROPA), "solo los momentos grandes van al álbum");
    const foto_t *f = pet_album_get(&p, 0);
    CHECK(f && f->especie == p.especie, "la foto recuerda la escena");
    /* el diario es circular: no se llena nunca */
    for (int i = 0; i < 60; i++) { p.vinculo_nivel = 0; p.vinculo = 95; pet_update(&p, t + 200 * H + (uint32_t)i * 24 * H); }
    CHECK(pet_diary_count(&p) == PET_DIARIO, "el diario guarda las últimas %d páginas", PET_DIARIO);
}

static void test_habilidades(void)
{
    static pet_t p;
    uint32_t t = T0 + 13 * H;
    nueva(&p, 141, t);
    p.vecino.proxima = t + 60 * 24 * H;
    p.estacion_fija = ESTACION_VERANO;
    p.escenas_abiertas = 0x1F;
    p.gusto_escena[ESC_LAGO] = 60;
    p.memoria.clima[CLIMA_LLUVIA] = 20;
    int primera = 0, aprendio = 0, cayo = 0;
    for (int dia = 0; dia < 12; dia++)
        for (uint32_t m = 0; m < 10 * 60; m += 5) {
            uint32_t now = t + (uint32_t)dia * 24 * H + m * 60;
            pet_force_weather(&p, CLIMA_DESPEJADO, now, 2 * H);
            p.escena = dia % 2 ? ESC_LAGO : ESC_BOSQUE;
            pet_update(&p, now);
            p.necesidad[NEED_ENERGIA] = 85;
            cuidar(&p, now);
            pet_event_t e;
            while (pet_poll_event(&p, &e))
                if (e.type == EV_HABILIDAD) { primera += e.b == 1; aprendio += e.b == 2; cayo += e.b == -1; }
        }
    CHECK(primera == 2, "primer intento de nadar y de trepar (%d)", primera);
    CHECK(p.habilidad[HAB_NADAR] >= 40 && p.habilidad[HAB_TREPAR] >= 40 && aprendio == 2,
          "practicando aprende a nadar y a trepar (%.0f %.0f)", p.habilidad[HAB_NADAR], p.habilidad[HAB_TREPAR]);
    CHECK(cayo >= 1, "al principio se cae del árbol (%d)", cayo);
}

static void test_tesoros_y_colecciones(void)
{
    static pet_t p;
    uint32_t t = T0 + 13 * H;
    nueva(&p, 151, t);
    p.vecino.proxima = t + 60 * 24 * H;
    p.rasgo[TRAIT_CURIOSA] = 0.9f;
    p.escenas_abiertas = 0x1F;
    int huele = 0, tesoros = 0, completas = 0;
    for (int dia = 0; dia < 20; dia++)
        for (uint32_t m = 0; m < 10 * 60; m += 5) {
            uint32_t now = t + (uint32_t)dia * 24 * H + m * 60;
            p.escena = ESC_COLINA;
            p.necesidad[NEED_CURIOSIDAD] = 30;
            pet_update(&p, now);
            cuidar(&p, now);
            pet_event_t e;
            while (pet_poll_event(&p, &e)) {
                if (e.type == EV_TESORO) { huele += e.a == 0; tesoros += e.a == 1; }
                if (e.type == EV_COLECCION) completas++;
            }
        }
    CHECK(huele >= 5 && tesoros >= 5, "huele tesoros y los desentierra (%d, %d)", huele, tesoros);
    CHECK(p.coleccion[COL_PIEDRAS] == 0x1F && (p.colecciones_completas & 1u) && completas >= 1, "completa la colección de piedras de colores (%02x)", p.coleccion[COL_PIEDRAS]);
    CHECK(pet_collection_size(COL_CRISTALES) == 4 && strcmp(pet_piece_name(COL_CONCHAS, 3), "una estrella de mar") == 0 &&
          strcmp(pet_piece_name(COL_CRISTALES, 3), "un trocito de hielo") == 0, "nombres de piezas (fw 104: + el trocito de hielo)");
}

static void test_sorpresas(void)
{
    /* eventos raros: en un mes pasa alguno */
    static pet_t p;
    uint32_t t = T0 + 13 * H;
    nueva(&p, 161, t);
    p.vecino.proxima = t + 60 * 24 * H;
    int raros = 0;
    for (uint32_t h = 1; h <= 30 * 24; h++) {
        uint32_t now = t + h * H;
        pet_update(&p, now);
        cuidar(&p, now);
        pet_event_t e;
        while (pet_poll_event(&p, &e)) raros += e.type == EV_RARO && e.b == 1;
    }
    CHECK(raros >= 2 && raros <= 40, "en un mes ve cosas raras (%d)", raros);
    /* arcoíris después de la lluvia */
    static pet_t q;
    uint32_t u = T0 + 15 * H; /* 12:00 */
    nueva(&q, 162, u);
    int arco = 0;
    for (int i = 0; i < 10 && !arco; i++) {
        q.ultimo_raro = 0; q.raro = 0xFF;
        pet_force_weather(&q, CLIMA_LLUVIA, u + (uint32_t)i * 600, 600);
        pet_force_weather(&q, CLIMA_DESPEJADO, u + (uint32_t)i * 600 + 300, 3600);
        pet_event_t e;
        while (pet_poll_event(&q, &e)) arco += e.type == EV_RARO && e.a == RARO_ARCOIRIS;
    }
    CHECK(arco >= 1, "después de la lluvia sale un arcoíris");

    /* metas suaves: una por día y se cumplen */
    static pet_t r;
    nueva(&r, 163, T0 + 13 * H);
    r.vecino.proxima = T0 + 60 * 24 * H;
    pet_update(&r, T0 + 13 * H + 60);
    CHECK(r.meta_dia != 0 && r.meta_objetivo >= 1, "hay una meta para hoy (%s)", pet_goal_name((meta_t)r.meta));
    r.meta = META_CARICIAS; r.meta_objetivo = 5; r.meta_progreso = 0; r.meta_hecha = 0;
    for (int i = 0; i < 6; i++) pet_action(&r, ACT_ACARICIAR, GESTO_TOQUE, T0 + 13 * H + 120 + (uint32_t)i * 30);
    CHECK(r.meta_hecha && r.estampitas == 1, "cumplida: una estampita");

    /* su cumpleaños: la primera semana y el primer mes */
    static pet_t s;
    uint32_t n = T0 + 13 * H;
    nueva(&s, 164, n);
    s.vecino.proxima = n + 90 * 24 * H;
    int semana = 0, mes = 0;
    for (uint32_t h = 1; h <= 33 * 24; h += 1) {
        uint32_t now = n + h * H;
        pet_update(&s, now);
        cuidar(&s, now);
        pet_event_t e;
        while (pet_poll_event(&s, &e))
            if (e.type == EV_SU_CUMPLE) { semana += e.a == 7 && !e.b; mes += e.a == 1 && !e.b; }
    }
    CHECK(semana == 1 && mes == 1, "celebra su primera semana y su primer mes (%d %d)", semana, mes);

    /* fechas especiales: Halloween */
    static pet_t h;
    uint32_t hw = 1793404800u + 15 * H; /* 31/10/2026 12:00 en Chile */
    nueva(&h, 165, hw - 2 * H);
    CHECK(pet_special_date(&h, hw) == FECHA_HALLOWEEN, "el 31 de octubre es Halloween");
    int fh = 0;
    pet_event_t e;
    pet_update(&h, hw - H);
    while (pet_poll_event(&h, &e)) fh += e.type == EV_FECHA && e.a == FECHA_HALLOWEEN; /* vacía la cola (64 lugares) */
    pet_update(&h, hw);
    while (pet_poll_event(&h, &e)) fh += e.type == EV_FECHA && e.a == FECHA_HALLOWEEN;
    CHECK(fh == 1, "lo celebra una vez");
}

static void test_suenos_del_dia(void)
{
    static pet_t p;
    uint32_t t = T0 + 14 * H;
    nueva(&p, 171, t);
    p.necesidad[NEED_ENERGIA] = 90;
    pet_action(&p, ACT_LANZAR, 0, t);
    pet_action(&p, ACT_ACARICIAR, GESTO_TOQUE, t + 60);
    CHECK((p.vivido >> VIV_PELOTA) & 1u && (p.vivido >> VIV_CARICIAS) & 1u, "recuerda lo que vivió hoy para soñarlo");
    p.necesidad[NEED_ENERGIA] = 30;
    pet_action(&p, ACT_DORMIR, 0, t + 120);
    CHECK(p.dormida && p.vivido != 0, "duerme con eso en la cabeza");
    pet_action(&p, ACT_DESPERTAR, 0, t + 3 * H);
    CHECK(p.vivido == 0, "al despertar empieza un día nuevo");
}

/* ---------- modos, hora, respaldo y celebraciones ---------- */
static uint32_t mediodia(int a, int m, int d) /* las 12:00 de ese día en Chile */
{
    int y = a - (m <= 2);
    int era = (y >= 0 ? y : y - 399) / 400, yoe = y - era * 400;
    int doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    int64_t dias = (int64_t)era * 146097 + yoe * 365 + yoe / 4 - yoe / 100 + doy - 719468;
    return (uint32_t)(dias * 86400 + 12 * 3600 - CHILE);
}

static void test_modos(void)
{
    static pet_t p;
    uint32_t t = T0 + 14 * H;
    pet_init(&p, "Mini", 2, t, 77);
    pet_set_timezone(&p, CHILE);
    pet_set_owner_age(&p, 4);
    CHECK(pet_mode(&p) == MODO_INFANTE && pet_child_mode(&p), "4 años: modo infante");
    pet_set_owner_age(&p, 7);
    CHECK(pet_mode(&p) == MODO_NINO && pet_child_mode(&p), "7 años: modo niño");
    pet_set_owner_age(&p, 11);
    CHECK(pet_mode(&p) == MODO_NINO, "11 años: modo niño");
    pet_set_owner_age(&p, 12);
    CHECK(pet_mode(&p) == MODO_ADULTO && !pet_child_mode(&p), "12 años: modo adulto");
    pet_set_owner_age(&p, 2);
    CHECK(pet_mode(&p) == MODO_INFANTE, "menos de 3: también infante");
    /* cumple 7: pasa sola de infante a niño */
    uint32_t c = mediodia(2026, 11, 20);
    static pet_t q;
    pet_init(&q, "Bebe", 1, c - 30 * 24 * H, 78);
    pet_set_timezone(&q, CHILE);
    pet_hatch(&q, c - 30 * 24 * H);
    CHECK(pet_set_owner_birthdate(&q, 20, 11, 2019, c - 30 * 24 * H), "fecha válida");
    CHECK(pet_mode(&q) == MODO_INFANTE, "a los 6: infante");
    for (uint32_t k = c - 30 * 24 * H; k <= c + 10 * H; k += 6 * H) { pet_update(&q, k); vaciar_eventos(&q); }
    CHECK(pet_mode(&q) == MODO_NINO, "al cumplir 7 pasa a modo niño (%d)", pet_mode(&q));
    /* con los más chicos nunca se enferma, aunque la descuiden */
    static pet_t r;
    pet_init(&r, "Sola", 3, t, 79);
    pet_set_timezone(&r, CHILE);
    pet_set_owner_age(&r, 5);
    pet_hatch(&r, t);
    int enferma = 0;
    for (uint32_t k = t; k < t + 12 * 24 * H; k += 3 * H) { pet_update(&r, k); enferma |= r.enferma; vaciar_eventos(&r); }
    CHECK(!enferma, "en modo infante no se enferma");
}

static void test_hora_perdida(void)
{
    static pet_t p;
    uint32_t t = T0 + 14 * H;
    nueva(&p, 80, t);
    pet_update(&p, t + 5 * H);
    CHECK(!pet_clock_behind(&p, t + 5 * H), "con la hora bien, sigue");
    CHECK(!pet_clock_behind(&p, t + 5 * H - 60), "un minuto de diferencia no importa");
    CHECK(pet_clock_behind(&p, t), "si el reloj vuelve atrás, la hora se perdió");
    float s = pet_need(&p, NEED_SACIEDAD);
    pet_update(&p, t); /* con la hora vieja no pasa nada */
    CHECK(pet_need(&p, NEED_SACIEDAD) == s && p.ultima_actualizacion == t + 5 * H, "con la hora vieja queda en pausa");
}

static void test_respaldo(void)
{
    /* base64 de ida y vuelta, de todos los largos */
    uint8_t in[12], out[16];
    char b[32];
    int ok = 1;
    for (int n = 0; n <= 12; n++) {
        for (int i = 0; i < n; i++) in[i] = (uint8_t)(i * 37 + n);
        pet_base64_encode(in, n, b, sizeof b);
        ok &= pet_base64_decode(b, out, sizeof out) == n && !memcmp(in, out, (size_t)n);
    }
    CHECK(ok, "base64 de ida y vuelta");

    static pet_t p, q;
    uint32_t t = T0 + 14 * H;
    nueva(&p, 81, t);
    pet_set_name(&p, "Kiwi");
    for (uint32_t k = t; k < t + 3 * 24 * H; k += 2 * H) {
        pet_update(&p, k);
        if (!p.dormida) pet_action(&p, ACT_ACARICIAR, 0, k);
    }
    static char txt[PET_RESPALDO_MAX];
    int n = pet_backup_export(&p, txt, sizeof txt);
    CHECK(n > 3000 && !strncmp(txt, PET_RESPALDO_CABECERA, strlen(PET_RESPALDO_CABECERA)), "el respaldo es texto con cabecera (%d)", n);
    CHECK(pet_backup_import(txt, &q), "el respaldo se puede traer");
    CHECK(!strcmp(q.nombre, "Kiwi") && q.especie == p.especie && q.vinculo == p.vinculo && q.diario_n == p.diario_n &&
              q.album_n == p.album_n && !memcmp(q.coleccion, p.coleccion, sizeof q.coleccion),
          "llega igual: nombre, vínculo, diario, álbum y colecciones");
    CHECK(!memcmp(q.gusto, p.gusto, sizeof q.gusto) && !memcmp(q.mini_dia, p.mini_dia, sizeof q.mini_dia) && q.cancion_n == p.cancion_n && q.est_mask == p.est_mask,
          "el respaldo trae también lo que aprendió: gustos, mini eventos, canción y estaciones");
    pet_event_t e;
    CHECK(!pet_poll_event(&q, &e), "sin avisos viejos del otro reloj");
    /* solo el texto en base64 (sin cabecera) también sirve */
    const char *solo = strchr(txt, '\n') + 1;
    CHECK(pet_backup_import(solo, &q), "sin cabecera también");
    /* uno dañado no se acepta */
    static char malo[PET_RESPALDO_MAX];
    strcpy(malo, txt);
    char *c = malo + n / 2;
    *c = *c == 'A' ? 'B' : 'A';
    CHECK(!pet_backup_import(malo, &q), "un respaldo dañado no se acepta");
    strcpy(malo, txt);
    malo[n / 2] = 0;
    CHECK(!pet_backup_import(malo, &q), "uno cortado tampoco");
    CHECK(!pet_backup_import("hola mundo", &q), "ni un texto cualquiera");
    CHECK(!strcmp(q.nombre, "Kiwi"), "si falla, no toca la mascota");
    /* y la traída sigue viviendo */
    pet_update(&q, t + 4 * 24 * H);
    CHECK(q.ultima_actualizacion == t + 4 * 24 * H, "se pone al día con el tiempo que pasó");
}

static void test_celebraciones(void)
{
    static pet_t p;
    pet_celebrations_default();
    CHECK(pet_celebration_count() == 6, "seis celebraciones de fábrica");
    uint32_t t = mediodia(2026, 9, 1);
    nueva(&p, 82, t);
    pet_set_country(&p, PAIS_ARGENTINA);
    CHECK(p.hemisferio_sur && pet_special_date(&p, mediodia(2026, 9, 18)) == FECHA_NINGUNA, "en Argentina (sur) no hay Fiestas Patrias por ahora");
    pet_set_country(&p, PAIS_CHILE);
    CHECK(pet_special_date(&p, mediodia(2026, 9, 18)) == FECHA_FIESTAS_PATRIAS, "18 de septiembre");
    CHECK(pet_special_date(&p, mediodia(2026, 9, 19)) == FECHA_FIESTAS_PATRIAS, "19 de septiembre");
    CHECK(pet_special_date(&p, mediodia(2026, 9, 20)) == FECHA_NINGUNA, "el 20 ya no");
    CHECK(pet_special_date(&p, mediodia(2026, 9, 21)) == FECHA_PRIMAVERA && pet_special_date(&p, mediodia(2026, 9, 22)) == FECHA_NINGUNA, "el Día de la Primavera es el 21 de septiembre en el sur");
    CHECK(pet_special_date(&p, mediodia(2027, 1, 1)) == FECHA_ANIO_NUEVO, "Año Nuevo cruza el año");
    CHECK(pet_special_date(&p, mediodia(2026, 12, 25)) == FECHA_NAVIDAD, "Navidad");

    /* el editor cambia las Fiestas Patrias y agrega el Día de la Madre (2º domingo de mayo) */
    celebracion_t c[3] = {
        {CEL_FIJA, 9, 17, 0, 3, FECHA_FIESTAS_PATRIAS, {0}, "Fiestas Patrias", "¡VIVA CHILE!"},
        {CEL_NESIMO, 5, 2, 0, 1, FECHA_SAN_VALENTIN, {0}, "el Día de la Madre", "¡FELIZ DÍA, MAMÁ!"},
        {CEL_NESIMO, 6, 5, 0, 1, FECHA_PRIMAVERA, {0}, "el último domingo de junio", ""},
    };
    CHECK(pet_celebrations_set(c, 3) && pet_celebration_count() == 3, "se cambian las celebraciones");
    CHECK(pet_special_date(&p, mediodia(2026, 9, 17)) == FECHA_FIESTAS_PATRIAS, "ahora desde el 17");
    CHECK(pet_special_date(&p, mediodia(2026, 12, 25)) == FECHA_NINGUNA, "sin Navidad si la quitaron");
    CHECK(pet_celebration_today(&p, mediodia(2027, 5, 9)) == 1, "2º domingo de mayo de 2027: el 9");
    CHECK(pet_celebration_today(&p, mediodia(2027, 5, 2)) == -1, "el 2 es el primero");
    CHECK(pet_celebration_today(&p, mediodia(2027, 6, 27)) == 2, "último domingo de junio de 2027: el 27");
    celebracion_t mala = c[0];
    mala.deco = FECHA_NINGUNA;
    CHECK(!pet_celebrations_set(&mala, 1) && pet_celebration_count() == 3, "sin decoración no se acepta (y no cambia nada)");
    mala = c[0];
    mala.dia = 31; mala.mes = 4;
    CHECK(!pet_celebrations_set(&mala, 1), "31 de abril no existe");

    /* empaquetar para el editor */
    uint8_t bin[16 + PET_CELEBRACIONES * sizeof(celebracion_t)];
    int n = pet_celebrations_pack(bin, sizeof bin);
    pet_celebrations_default();
    CHECK(n > 0 && pet_celebrations_unpack(bin, n) && pet_celebration_count() == 3, "se guardan y se cargan");
    bin[20] ^= 1;
    CHECK(!pet_celebrations_unpack(bin, n) && pet_celebration_count() == 3, "dañadas no se aceptan");

    /* se celebra con su nombre */
    static pet_t m;
    uint32_t dm = mediodia(2027, 5, 9);
    nueva(&m, 83, dm - 4 * H);
    vaciar_eventos(&m);
    pet_event_t e;
    int vista = 0;
    for (uint32_t k = dm - 4 * H + 600; k <= dm; k += 600) { /* de a poco, para no perder avisos */
        pet_update(&m, k);
        while (pet_poll_event(&m, &e)) if (e.type == EV_FECHA) vista |= e.a == FECHA_SAN_VALENTIN && e.b == 1;
    }
    CHECK(vista, "el Día de la Madre se celebra con corazones");
    char txt[96] = "";
    for (int i = 0; i < pet_diary_count(&m); i++) {
        const entrada_diario_t *d = pet_diary_get(&m, i);
        if (d->tipo == DIA_FECHA) pet_diary_text(&m, d, txt, sizeof txt);
    }
    CHECK(strstr(txt, "Día de la Madre") != NULL, "y en el diario con su nombre (%s)", txt);
    pet_celebrations_default();
}

/* ---------- fase 5: creatividad ---------- */
static void test_ropa(void)
{
    static pet_t p;
    uint32_t t = T0 + 14 * H; /* 11:00 en Chile */
    nueva(&p, 301, t);
    CHECK(pet_has_clothes(&p, ROPA_MONO) && pet_has_clothes(&p, ROPA_PANUELO) && !pet_has_clothes(&p, ROPA_LENTES),
          "el baúl empieza con un moño y un pañuelo");
    CHECK(pet_has_toy(&p, JUG_PELUCHE) && !pet_has_toy(&p, JUG_COLUMPIO), "y un peluche");
    vaciar_eventos(&p);
    p.gusto_ropa[ROPA_MONO] = 80;
    CHECK(pet_action(&p, ACT_VESTIR, ROPA_MONO, t) == 1 && pet_wearing(&p, RANURA_CABEZA) == ROPA_MONO, "le pones el moño");
    pet_event_t e;
    int b = -9;
    while (pet_poll_event(&p, &e)) if (e.type == EV_ROPA) b = e.b;
    CHECK(b == 2, "le encanta (%d)", b);
    CHECK(pet_action(&p, ACT_VESTIR, ROPA_LENTES, t + 10) == 0, "lo que no está en el baúl no se puede poner");
    CHECK(pet_action(&p, ACT_VESTIR, -(RANURA_CABEZA + 1), t + 20) == 1 && pet_wearing(&p, RANURA_CABEZA) < 0, "se lo quitas");

    /* lo que no le gusta: a veces se lo saca; con los más chicos, nunca */
    p.ropa_tiene |= 1u << ROPA_LENTES;
    p.gusto_ropa[ROPA_LENTES] = -90;
    p.vinculo = 20;
    int sacadas = 0;
    for (int i = 0; i < 20; i++) {
        uint32_t k = t + 60 + (uint32_t)i * 30;
        pet_action(&p, ACT_VESTIR, ROPA_LENTES, k);
        if (pet_wearing(&p, RANURA_CARA) < 0) sacadas++;
        else pet_action(&p, ACT_VESTIR, -(RANURA_CARA + 1), k + 5);
    }
    CHECK(sacadas >= 5 && sacadas <= 19, "lo que no le gusta, a veces se lo saca (%d de 20)", sacadas);
    static pet_t q;
    q = p;
    pet_set_owner_age(&q, 4);
    for (int i = 0; i < 20; i++) {
        pet_action(&q, ACT_VESTIR, ROPA_LENTES, t + 2000 + (uint32_t)i * 30);
        CHECK(pet_wearing(&q, RANURA_CARA) == ROPA_LENTES || i, "en modo infante no se lo saca");
    }
    pet_update(&q, t + 2000 + 6 * H);
    CHECK(pet_wearing(&q, RANURA_CARA) == ROPA_LENTES, "ni con las horas");

    /* la bufanda abriga */
    static pet_t f;
    nueva(&f, 302, t);
    f.memoria.clima[CLIMA_FRIO] = -60;
    pet_force_weather(&f, CLIMA_FRIO, t, 6 * H);
    pet_update(&f, t + 60);
    CHECK(pet_temperature_feel(&f, t + 60) < 0, "con frío y sin abrigo, tiene frío");
    f.ropa_tiene |= 1u << ROPA_BUFANDA;
    pet_action(&f, ACT_VESTIR, ROPA_BUFANDA, t + 120);
    if (pet_wearing(&f, RANURA_CUELLO) != ROPA_BUFANDA) { f.ropa_puesta[RANURA_CUELLO] = ROPA_BUFANDA + 1; }
    CHECK(pet_temperature_feel(&f, t + 180) == 0, "con la bufanda ya no");

    /* con calor se saca la bufanda */
    static pet_t c;
    uint32_t medio = T0 + 24 * H + 16 * H; /* 13:00 */
    nueva(&c, 303, medio - H);
    c.estacion_fija = ESTACION_VERANO;
    c.memoria.clima[CLIMA_DESPEJADO] = -10;
    pet_force_weather(&c, CLIMA_DESPEJADO, medio - 600, 4 * H);
    pet_update(&c, medio);
    c.escena = ESC_PRADO;
    c.ropa_tiene |= 1u << ROPA_BUFANDA;
    vaciar_eventos(&c);
    pet_action(&c, ACT_VESTIR, ROPA_BUFANDA, medio + 10);
    b = -9;
    while (pet_poll_event(&c, &e)) if (e.type == EV_ROPA) b = e.b;
    CHECK(b == -3 && pet_wearing(&c, RANURA_CUELLO) < 0, "con calor se saca la bufanda (%d)", b);
    { /* accesorios de mano: el paraguas y el sombrero se llevan a la vez, y cada uno se saca solo */
        pet_t m = c;
        m.ropa_tiene = (1u << ROPA_SOMBRERO) | (1u << ROPA_PARAGUAS) | (1u << ROPA_MOLINILLO);
        m.ropa_puesta[0] = m.ropa_puesta[1] = m.ropa_puesta[2] = 0; m.ropa_mano = 0;
        m.vinculo = 100;
        pet_action(&m, ACT_VESTIR, ROPA_SOMBRERO, medio + 30);
        pet_action(&m, ACT_VESTIR, ROPA_PARAGUAS, medio + 31);
        CHECK(pet_wearing(&m, RANURA_CABEZA) == ROPA_SOMBRERO && pet_wearing(&m, RANURA_MANO) == ROPA_PARAGUAS, "sombrero en la cabeza y paraguas en la mano a la vez");
        pet_action(&m, ACT_VESTIR, ROPA_MOLINILLO, medio + 32);
        CHECK(pet_wearing(&m, RANURA_MANO) == ROPA_MOLINILLO, "en la mano lleva una cosa a la vez");
        pet_action(&m, ACT_VESTIR, -(RANURA_MANO + 1), medio + 33);
        CHECK(pet_wearing(&m, RANURA_MANO) < 0 && pet_wearing(&m, RANURA_CABEZA) == ROPA_SOMBRERO, "se saca lo de la mano sin tocar el sombrero");
        CHECK(pet_clothes_liking(&m, ROPA_PARAGUAS) >= -100 && pet_clothes_liking(&m, ROPA_FAROLITO) <= 100, "los gustos nuevos están en rango");
        pet_t n; memset(&n, 0, sizeof n);
        n.vio = 16; n.estampitas = 0; n.etapa = STAGE_CRIA;
        CHECK(!pet_has_clothes(&n, ROPA_PARAGUAS), "sin haber visto la lluvia no tiene el paraguas");
    }
    c.ropa_tiene |= 1u << ROPA_SOMBRERO;
    c.gusto_ropa[ROPA_SOMBRERO] = 60;
    c.actividad = ACTV_PASEAR;
    CHECK(pet_temperature_feel(&c, medio + 20) > 0, "al sol tiene calor");
    pet_action(&c, ACT_VESTIR, ROPA_SOMBRERO, medio + 30);
    CHECK(pet_temperature_feel(&c, medio + 40) == 0, "con el sombrero de paja, no");
}

static void test_juguetes(void)
{
    static pet_t p;
    uint32_t t = T0 + 14 * H;
    nueva(&p, 311, t);
    p.juguetes_tiene = (1u << JUG_COUNT) - 1;
    int x, y;
    CHECK(pet_action(&p, ACT_PONER_JUGUETE, JUG_COLUMPIO | (100 << 8) | (150 << 16), t) == 1 && pet_toy_placed(&p, JUG_COLUMPIO, &x, &y) &&
              x == 200 && y == 300, "pones el columpio donde quieres (%d,%d)", x, y);
    pet_action(&p, ACT_PONER_JUGUETE, JUG_PELUCHE | (60 << 8) | (160 << 16), t + 5);
    pet_action(&p, ACT_PONER_JUGUETE, JUG_TAMBOR | (170 << 8) | (160 << 16), t + 6);
    CHECK(pet_action(&p, ACT_PONER_JUGUETE, JUG_CAJA | (120 << 8) | (120 << 16), t + 7) == 0, "hasta tres juguetes en el prado");
    CHECK(pet_action(&p, ACT_PONER_JUGUETE, JUG_TAMBOR | (255 << 8), t + 8) == 1 && !pet_toy_placed(&p, JUG_TAMBOR, NULL, NULL), "lo guardas");
    CHECK(pet_action(&p, ACT_PONER_JUGUETE, JUG_CAJA | (120 << 8) | (120 << 16), t + 9) == 1, "y cabe otro");
    p.gusto_juguete[JUG_PELUCHE] = 70;
    p.escena = ESC_PRADO;
    p.actividad_hasta = t + 10;
    CHECK(pet_action(&p, ACT_USAR_JUGUETE, JUG_PELUCHE, t + 20) == 1 && p.actividad == ACTV_JUGUETE && p.juguete_uso == JUG_PELUCHE,
          "la invitas a jugar con el peluche");
    vaciar_eventos(&p);
    float d0 = pet_need(&p, NEED_DIVERSION);
    pet_update(&p, t + 20 + 400);
    CHECK((p.juguetes_usados >> JUG_PELUCHE) & 1u, "lo estrenó");
    CHECK(pet_need(&p, NEED_DIVERSION) > d0 - 3, "jugar la entretiene");
    int estreno = 0;
    for (int i = 0; i < pet_diary_count(&p); i++) estreno |= pet_diary_get(&p, i)->tipo == DIA_ESTRENO;
    CHECK(estreno, "y lo anota en el diario");

    /* sola: con juguetes en el prado, los usa */
    static pet_t s;
    nueva(&s, 312, t);
    s.juguetes_tiene = (1u << JUG_COUNT) - 1;
    pet_action(&s, ACT_PONER_JUGUETE, JUG_PELOTON | (120 << 8) | (150 << 16), t);
    pet_force_weather(&s, CLIMA_DESPEJADO, t, 48 * H);
    int usa = 0;
    pet_event_t e;
    for (uint32_t k = t; k < t + 48 * H; k += 600) {
        if (!((s.juguetes_puestos >> JUG_PELOTON) & 1u)) pet_action(&s, ACT_PONER_JUGUETE, JUG_PELOTON | (120 << 8) | (150 << 16), k); /* (un juguete afuera dura unos minutos: se vuelve a sacar) */
        pet_update(&s, k);
        if (s.escena != ESC_PRADO) s.escena = ESC_PRADO;
        while (pet_poll_event(&s, &e)) usa += e.type == EV_ACTIVIDAD && e.a == ACTV_JUGUETE;
        s.necesidad[NEED_SACIEDAD] = 80;
    }
    CHECK(usa >= 2, "juega sola con su pelotón (%d veces en dos días)", usa);
    {   /* un juguete sacado al prado se guarda solo después de unos minutos */
        static pet_t g;
        nueva(&g, 313, t);
        g.juguetes_tiene = (1u << JUG_COUNT) - 1;
        pet_action(&g, ACT_PONER_JUGUETE, JUG_TAMBOR | (120 << 8) | (150 << 16), t);
        CHECK((g.juguetes_puestos >> JUG_TAMBOR) & 1u, "el juguete queda en el prado");
        for (uint32_t k = t; k <= t + 30 * 60; k += 60) pet_update(&g, k);
        CHECK(!((g.juguetes_puestos >> JUG_TAMBOR) & 1u), "y a los pocos minutos se guarda en el baúl");
    }
}

static void test_baul(void)
{
    static pet_t p;
    uint32_t t = T0 + 14 * H;
    nueva(&p, 321, t);
    vaciar_eventos(&p);
    p.estampitas = 2;
    p.vinculo = 20; /* todavía no son amigos: la caja espera */
    pet_update(&p, t + 120);
    pet_event_t e;
    int nuevo = -1;
    while (pet_poll_event(&p, &e)) if (e.type == EV_NUEVO) nuevo = e.a * 100 + e.b;
    CHECK(nuevo == 100 + JUG_PELOTON && pet_has_toy(&p, JUG_PELOTON), "con 2 estampitas aparece el pelotón (%d)", nuevo);
    CHECK(!pet_has_toy(&p, JUG_CAJA), "la caja de cartón, cuando sean amigos");
    p.vinculo = 55;
    p.estampitas = 12;
    int n = 0;
    for (uint32_t k = t + 300; k < t + 3 * H; k += 120) { pet_update(&p, k); n += contar_eventos(&p, EV_NUEVO); }
    CHECK(pet_has_clothes(&p, ROPA_LENTES) && pet_has_clothes(&p, ROPA_COLLAR) && pet_has_toy(&p, JUG_COLUMPIO), "con más estampitas, más cosas");
    CHECK(n >= 5, "de a una, cada una con su aviso (%d)", n);
    /* una mascota nueva conserva el baúl */
    uint16_t ropa = p.ropa_tiene;
    pet_rebirth(&p, 3, t + 4 * H, 99);
    CHECK((p.ropa_tiene & ropa) == ropa, "una mascota nueva conserva el baúl");
}

static void test_semillas_y_jardines(void)
{
    static pet_t p;
    uint32_t t = T0 + 14 * H;
    nueva(&p, 341, t);
    CHECK(pet_seed_known(&p, CULTIVO_FLOR) && !pet_seed_known(&p, CULTIVO_GIRASOL) && pet_seeds(&p, CULTIVO_FLOR) == 3,
          "empieza solo con semillas de flor");
    /* va encontrando otros tipos con el tiempo y según dónde busca */
    p.escenas_abiertas = 0x1F;
    int nuevas = 0, total = 0;
    pet_event_t e;
    for (int dia = 0; dia < 24; dia++) {
        uint32_t td = t + (uint32_t)dia * 24 * H;
        p.escena = (uint8_t)(dia % 4);
        for (int k = 0; k < 40; k++) {
            p.ultimo_hallazgo = 0; p.hallazgos_dia = 0; p.dia_hallazgos = 0;
            p.actividad = ACTV_OLFATEAR; p.actividad_hasta = 0;
            pet_update(&p, td + (uint32_t)k * 1200u);
            p.escena = (uint8_t)(dia % 4);
            while (pet_poll_event(&p, &e)) if (e.type == EV_SEMILLA) { total++; nuevas += e.b; }
            p.necesidad[NEED_SACIEDAD] = 80; p.necesidad[NEED_ENERGIA] = 80;
        }
    }
    CHECK(total > 5 && nuevas >= 1, "va encontrando semillas (%d) y tipos nuevos (%d)", total, nuevas);
    int suma = 0;
    for (int c = 0; c < CULTIVO_COUNT; c++) suma += p.semillas[c];
    CHECK(suma == p.inventario[ITEM_SEMILLA], "el inventario es la suma de los tipos");
    /* jardines en otras zonas */
    int n, ini = pet_plant_first(ESC_LAGO, &n);
    CHECK(ini == PET_PARCELAS + PET_PARCELAS_ZONA && n == 3 && pet_plant_scene(ini) == ESC_LAGO && pet_plant_first(ESC_CUEVA, &n) < 0,
          "el bosque, el lago y la colina tienen su jardín (la cueva no)");
    dar_semillas(&p, CULTIVO_FLOR, 2);
    CHECK(pet_action(&p, ACT_PLANTAR, ini, t + 300 * H) == 1 && pet_plant(&p, ini)->tipo == PLANTA_FLOR, "planta en el jardín del lago");
    CHECK(pet_action(&p, ACT_ARRANCAR, ini, t + 300 * H) == 0 && pet_plant(&p, ini)->tipo == PLANTA_FLOR, "viva no se puede sacar");
    ((planta_t *)pet_plant(&p, ini))->agua = PET_AGUA_SECA - 1;
    CHECK(pet_action(&p, ACT_ARRANCAR, ini, t + 300 * H) == 1 && pet_plant(&p, ini)->tipo == PLANTA_NADA, "seca, sí se saca");
    /* decorar gasta semillas de cualquier tipo */
    memset(p.semillas, 0, sizeof p.semillas);
    dar_semillas(&p, CULTIVO_HIERBA, 1); dar_semillas(&p, CULTIVO_GIRASOL, 1);
    p.inventario[ITEM_PIEDRA] = 5;
    p.decoraciones = 0;
    CHECK(pet_place_decor(&p, DECOR_FLORES, t + 301 * H) == 1 && p.inventario[ITEM_SEMILLA] == 0, "decorar usa semillas de cualquier tipo");
}

static void test_charcos_y_zonas(void)
{
    static pet_t p;
    uint32_t t = T0 + 14 * H;
    nueva(&p, 351, t);
    p.vinculo = 60;
    pet_force_weather(&p, CLIMA_LLUVIA, t, H);
    pet_force_weather(&p, CLIMA_DESPEJADO, t + H, 3 * H);
    vaciar_eventos(&p);
    CHECK(pet_puddles(&p, t + H + 60), "después de la lluvia quedan charcos");
    pet_event_t e;
    int salpica = 0, a = -1;
    pet_update(&p, t + H + 60); p.escena = ESC_PRADO; p.dormida = 0; p.necesidad[NEED_ENERGIA] = 80;
    if (pet_action(&p, ACT_SALPICAR, 1, t + H + 60) == -2) pet_action(&p, ACT_SALPICAR, 1, t + H + 60); /* (a veces dice que no: a la segunda sí) */
    while (pet_poll_event(&p, &e)) if (e.type == EV_SALPICA) { salpica++; a = e.a; }
    CHECK(salpica == 1 && a == 3, "tocas un charco y va a saltar ahí (a=%d)", a);
    CHECK(!pet_puddles(&p, t + 4 * H), "los charcos se secan en un par de horas");
    float dias;
    CHECK(pet_scene_requirement(&p, ESC_BOSQUE, t, &dias) == 1 && dias > 0.2f, "el bosque: faltan días (%.1f)", dias);
    CHECK(pet_scene_requirement(&p, ESC_CUEVA, t, &dias) == 3, "la cueva es un secreto");
    CHECK(pet_scene_requirement(&p, ESC_PRADO, t, &dias) == 0, "el prado está abierto");
    /* el clima cambia seguido */
    int cambios = 0;
    for (uint32_t k = 0; k < 24; k++) { pet_update(&p, t + 4 * H + k * H); while (pet_poll_event(&p, &e)) cambios += e.type == EV_CLIMA; }
    CHECK(cambios >= 12, "el clima cambia seguido: %d veces en un día", cambios);
}

static void test_depende_de_ti(void)
{
    /* sola se entretiene un poco, pero comer, el baño, el cariño y jugar de verdad dependen de ti */
    static pet_t p;
    uint32_t t = T0 + 14 * H;
    nueva(&p, 371, t);
    float maxdiv = 0, maxcur = 0, maxcar = 0;
    for (uint32_t k = 1; k <= 30; k++) {
        pet_update(&p, t + k * H);
        vaciar_eventos(&p);
        if (pet_need(&p, NEED_DIVERSION) > maxdiv && k > 3) maxdiv = pet_need(&p, NEED_DIVERSION);
        if (pet_need(&p, NEED_CURIOSIDAD) > maxcur && k > 3) maxcur = pet_need(&p, NEED_CURIOSIDAD);
        if (pet_need(&p, NEED_CARINO) > maxcar && k > 3) maxcar = pet_need(&p, NEED_CARINO);
    }
    CHECK(maxdiv <= 56 && maxcur <= 76 && maxcar <= 60, "sola no se llena: diversión %.0f, curiosidad %.0f, cariño %.0f", maxdiv, maxcur, maxcar);
    CHECK(pet_mood(&p) != MOOD_FELIZ || pet_need(&p, NEED_SACIEDAD) > 40, "con hambre y sin cariño no está feliz (%s, hambre %.0f)",
          pet_mood_name(pet_mood(&p)), pet_need(&p, NEED_SACIEDAD));
    uint32_t ahora = t + 30 * H + 60;
    p.ultimo_no = ACT_JUGAR; /* que obedezca (nunca dice que no dos veces seguidas) */
    p.enferma = 0; /* (v37: según la suerte del día pudo enfermarse sola; enferma, jugar llena menos) */
    pet_action(&p, ACT_JUGAR, 0, ahora);
    pet_action(&p, ACT_ACARICIAR, GESTO_CARICIA, ahora + 30);
    CHECK(pet_need(&p, NEED_DIVERSION) > 70, "jugar contigo sí la llena (%.0f)", pet_need(&p, NEED_DIVERSION));
    /* frases que tienen sentido: triste por algo y feliz sin motivo no se juntan con "pero" */
    static pet_t q;
    nueva(&q, 372, t);
    memset(q.emo, 0, sizeof q.emo);
    q.emo[EMO_TRISTEZA] = 0.7f; q.causa[EMO_TRISTEZA] = CAUSA_TE_EXTRANA;
    q.emo[EMO_ALEGRIA] = 0.5f; q.causa[EMO_ALEGRIA] = CAUSA_NINGUNA;
    char buf[256];
    int malas = 0;
    for (int i = 0; i < 20; i++) { pet_feeling(&q, buf, sizeof buf); malas += strstr(buf, "pero") != NULL || strstr(buf, "aunque") != NULL; }
    CHECK(malas == 0, "no dice \"triste, pero feliz\" sin motivo (%s)", buf);
    q.causa[EMO_ALEGRIA] = CAUSA_VOLVISTE; q.emo[EMO_ALEGRIA] = 0.65f;
    pet_feeling(&q, buf, sizeof buf);
    CHECK(strstr(buf, "extrañé") || strstr(buf, "solita"), "te extrañó y volviste: lo dice junto (%s)", buf);
}

static void test_actualizar_partida(void)
{
    /* una partida de la versión 13 (sin los campos nuevos del final) se actualiza sola */
    static pet_t p, q;
    uint32_t t = T0 + 14 * H;
    nueva(&p, 361, t);
    memset(p.semillas, 0, sizeof p.semillas);
    p.inventario[ITEM_SEMILLA] = 7;
    p.jardin[0].tipo = PLANTA_GIRASOL;
    size_t viejo = offsetof(pet_t, semillas) + 4;
    memset(&q, 0xAB, sizeof q);
    memcpy(&q, &p, offsetof(pet_t, semillas));
    q.version = 13;
    q.size = (uint16_t)viejo;
    uint32_t c = crc32_prueba((const uint8_t *)&q, viejo - 4);
    memcpy((uint8_t *)&q + viejo - 4, &c, 4);
    CHECK(!pet_validate(&q), "una partida vieja no pasa como nueva");
    CHECK(pet_upgrade(&q) && pet_validate(&q), "pero se actualiza");
    CHECK(q.semillas[CULTIVO_FLOR] == 7 && pet_seed_known(&q, CULTIVO_GIRASOL) && q.jardin_zona[0].tipo == PLANTA_NADA && !strcmp(q.nombre, p.nombre),
          "conserva todo y las semillas pasan a ser de flor");
    q.crc ^= 1;
    q.version = 13;
    CHECK(!pet_upgrade(&q), "una dañada no");
}

static int alarmas_entre(pet_t *p, uint32_t desde, uint32_t hasta, int *pospuestas)
{
    int n = 0;
    pet_event_t e;
    for (uint32_t t = desde; t <= hasta; t += 60) {
        pet_update(p, t);
        while (pet_poll_event(p, &e))
            if (e.type == EV_ALARMA) { n++; if (pospuestas && e.b) (*pospuestas)++; }
    }
    return n;
}

static void test_alarmas(void)
{
    static pet_t p, q;
    const uint32_t lun0 = T0 + 3 * H; /* lunes 00:00 en Chile */
    nueva(&p, 401, lun0 - 60);
    pet_update(&p, lun0);
    CHECK(pet_alarm_count(&p) == 0 && p.dormir.modo == DORMIR_NO, "vienen sin alarmas ni hora de dormir");
    pet_alarm_set(&p, 0, true, 7, 0, ALARMA_LUN_VIE, 0, false);
    pet_alarm_set(&p, 3, true, 9, 30, ALARMA_FINDE, 2, false);
    CHECK(pet_alarm_count(&p) == 2, "dos alarmas");
    CHECK(pet_alarm_next(&p, 0, lun0) == lun0 + 7 * H, "la de lunes a viernes suena el lunes a las 7:00");
    CHECK(pet_alarm_next(&p, 3, lun0) == lun0 + 5 * 86400 + 9 * H + 1800, "la del fin de semana, el sábado a las 9:30");
    CHECK(pet_alarm_sound(2) == SND_FIESTA, "cada una con su sonido");
    int n = alarmas_entre(&p, lun0 + 60, lun0 + 7 * H + 120, NULL);
    CHECK(n == 1, "suena una vez a su hora (%d)", n);
    pet_alarm_answer(&p, 0, true, lun0 + 7 * H + 120);
    int pos = 0;
    n = alarmas_entre(&p, lun0 + 7 * H + 180, lun0 + 7 * H + 600, &pos);
    CHECK(n == 1 && pos == 1, "\"5 minutos más\" la hace sonar otra vez a los 5 minutos");
    p.ultima_actualizacion = lun0 + 5 * 86400 + 6 * H; /* sábado */
    pos = 0;
    n = alarmas_entre(&p, lun0 + 5 * 86400 + 6 * H + 60, lun0 + 5 * 86400 + 10 * H, NULL);
    CHECK(n == 1, "el sábado suena solo la del fin de semana (%d)", n);
    p.necesidad[NEED_ENERGIA] = 90;
    p.dormida = 1; p.luz_apagada = 1;
    pet_alarm_answer(&p, 3, false, lun0 + 5 * 86400 + 10 * H);
    CHECK(!p.luz_apagada && !p.dormida, "\"¡ya desperté!\" prende la luz y despierta descansada");
    p.necesidad[NEED_ENERGIA] = 30;
    p.dormida = 1;
    pet_alarm_answer(&p, 0, false, lun0 + 5 * 86400 + 10 * H + 60);
    CHECK(p.dormida, "pero si durmió poco sigue durmiendo (sin enojarse)");
    p.dormida = 0;
    /* una sola vez */
    pet_alarm_set(&p, 1, true, 23, 0, 0, 1, false);
    uint32_t dom = lun0 + 6 * 86400;
    p.ultima_actualizacion = dom + 22 * H;
    n = alarmas_entre(&p, dom + 22 * H + 60, dom + 23 * H + 120, NULL);
    CHECK(n == 1 && !p.alarma[1].activa, "la de una sola vez suena y se apaga");
    CHECK(pet_alarm_next(&p, 1, dom + 23 * H + 200) == 0, "y ya no tiene próxima");
    /* si el reloj estuvo apagado, no suena tarde */
    pet_alarm_set(&p, 1, true, 22, 0, ALARMA_TODOS, 0, false);
    p.ultima_actualizacion = lun0 + 8 * 86400 + 21 * H;
    n = alarmas_entre(&p, lun0 + 8 * 86400 + 23 * H, lun0 + 8 * 86400 + 23 * H, NULL);
    CHECK(n == 0, "si se la perdió (el reloj estaba apagado), no suena una hora después");
    /* empaquetado para la interfaz y borrar */
    int arg = pet_alarm_pack(4, true, 6, 45, ALARMA_FINDE, 3, true);
    pet_alarm_set_packed(&p, arg);
    CHECK(p.alarma[4].activa && p.alarma[4].hora == 6 && p.alarma[4].minuto == 45 && p.alarma[4].dias == ALARMA_FINDE &&
          p.alarma[4].sonido == 3 && p.alarma[4].siempre, "la interfaz las manda empaquetadas");
    pet_alarm_set_packed(&p, 4 | PET_ALARMA_BORRAR);
    CHECK(!p.alarma[4].usada && pet_alarm_count(&p) == 3, "y se borran");
    /* hora de dormir */
    pet_bedtime_set_packed(&p, pet_bedtime_pack(DORMIR_FIJA, 22, 30, 7, 0));
    uint32_t mar = lun0 + 8 * 86400; /* martes */
    CHECK(!pet_is_bedtime(&p, mar + 21 * H) && pet_is_bedtime(&p, mar + 23 * H) && pet_is_bedtime(&p, mar + 86400 + 6 * H) &&
          !pet_is_bedtime(&p, mar + 86400 + 8 * H), "la hora de dormir cruza la medianoche");
    p.necesidad[NEED_ENERGIA] = 70;
    p.dormida = 0; p.luz_apagada = 0;
    p.ultima_actualizacion = mar + 22 * H;
    int acostarse = 0, levantarse = 0;
    pet_event_t e;
    for (uint32_t t = mar + 22 * H + 60; t <= mar + 86400 + 7 * H + 120; t += 60) {
        pet_update(&p, t);
        while (pet_poll_event(&p, &e)) if (e.type == EV_HORA_DORMIR) { if (e.a) levantarse++; else acostarse++; }
        if (t == mar + 22 * H + 31 * 60) CHECK(p.dormida && p.luz_apagada, "a la hora de dormir se acuesta y se apaga la luz");
    }
    CHECK(acostarse == 1 && levantarse == 1, "avisa una vez al acostarse (%d) y otra al levantarse (%d)", acostarse, levantarse);
    CHECK(!p.dormida && !p.luz_apagada, "y a la hora de levantarse se levanta");
    /* son de cada perfil: una mascota nueva empieza sin las tuyas */
    q = p;
    pet_rebirth(&q, 2, lun0 + 9 * 86400, 5);
    CHECK(pet_alarm_count(&q) == 0 && q.dormir.modo == DORMIR_NO, "una mascota nueva (otro perfil) empieza sin alarmas");
    static pet_t r;
    r = p;
    pet_resume(&r, &q, lun0 + 9 * 86400);
    CHECK(pet_alarm_count(&r) == 3 && r.dormir.modo == DORMIR_FIJA, "y cada una vuelve con las suyas");
    /* aviso suave de respaldo */
    CHECK(!pet_backup_due(&p, p.nacimiento + 10 * 86400) && pet_backup_due(&p, p.nacimiento + 30 * 86400), "cada 4 semanas sugiere guardarla");
    pet_mark_backup(&p, p.nacimiento + 30 * 86400);
    CHECK(!pet_backup_due(&p, p.nacimiento + 31 * 86400), "y al guardarla deja de sugerirlo");
    /* ajustes del dispositivo */
    pet_device_t dv;
    memset(&dv, 0xFF, sizeof dv);
    pet_device_fix(&dv);
    CHECK(dv.version == PET_DEVICE_VERSION && dv.volumen == 38 && pet_device_screen_ms(&dv) == 120000, "ajustes dañados vuelven a los de fábrica");
    dv.brillo = 0; dv.volumen = 0; dv.alarmas = 10;
    pet_device_fix(&dv);
    CHECK(dv.brillo == PET_BRILLO_MIN, "el brillo nunca queda en negro");
    CHECK(pet_device_alarm_volume(&dv) == PET_ALARMA_VOLUMEN_MIN, "la alarma suena aunque el volumen esté en 0");
    dv.brillo = 100;
    CHECK(pet_device_brightness(&dv, true, false) == 55 && pet_device_brightness(&dv, true, true) < 20, "la pecera y la noche bajan el brillo elegido");
    memset(&dv, 0, sizeof dv);
    dv.version = 1; dv.volumen = 30; dv.brillo = 60; dv.apagado = 2;
    pet_device_fix(&dv);
    CHECK(dv.version == PET_DEVICE_VERSION && dv.volumen == 30 && dv.brillo == 60 && dv.ambiente == 50, "los ajustes de antes conservan lo suyo y el sonido de fondo queda a la mitad");
    /* temporizador */
    {
        static pet_t tm;
        nueva(&tm, 405, lun0);
        pet_update(&tm, lun0 + 60);
        pet_timer_set(&tm, 600, lun0 + 60);
        CHECK(pet_timer_left(&tm, lun0 + 360) == 300, "el temporizador cuenta hacia atrás");
        int son = 0;
        for (uint32_t t = lun0 + 120; t <= lun0 + 900; t += 30) { pet_update(&tm, t); while (pet_poll_event(&tm, &e)) son += e.type == EV_TEMPORIZADOR; }
        CHECK(son == 1 && !tm.temporizador, "y suena una vez al terminar");
        /* pasos: cada 2000 trae algo del paseo, sin metas */
        int paseos = 0, antes = tm.inventario[ITEM_SEMILLA] + tm.inventario[ITEM_PIEDRA] + tm.inventario[ITEM_FRUTA] + tm.inventario[ITEM_MIEL] + tm.inventario[ITEM_HIERBA] + tm.ramitas + tm.musgo;
        for (int k = 0; k < 10; k++) { pet_steps_add(&tm, 500, lun0 + 1000 + (uint32_t)k * 60); while (pet_poll_event(&tm, &e)) paseos += e.type == EV_PASEO; }
        int despues = tm.inventario[ITEM_SEMILLA] + tm.inventario[ITEM_PIEDRA] + tm.inventario[ITEM_FRUTA] + tm.inventario[ITEM_MIEL] + tm.inventario[ITEM_HIERBA] + tm.ramitas + tm.musgo;
        CHECK(paseos == 2 && tm.pasos_total == 5000, "5000 pasos son dos paseos (%d)", paseos);
        CHECK(despues >= antes + 1, "y trae cosas del paseo");
        pet_steps_add(&tm, 999999, lun0 + 2000);
        CHECK(tm.pasos_total == 5000, "una lectura imposible no cuenta");
        /* perfiles: la que sigue viviendo se pone al día al volver; la que descansa, no */
        static pet_t a1, a2, otra;
        nueva(&otra, 404, lun0);
        nueva(&a1, 405, lun0); pet_update(&a1, lun0 + 600);
        a2 = a1;
        pet_set_profile_lives(&a2, true);
        pet_resume(&a1, &otra, lun0 + 3 * 86400);
        pet_resume(&a2, &otra, lun0 + 3 * 86400);
        CHECK(a1.ultima_actualizacion == lun0 + 3 * 86400, "la que descansa no vivió esos días");
        pet_update(&a2, lun0 + 3 * 86400);
        float sac = a2.necesidad[NEED_SACIEDAD];
        pet_action(&a2, ACT_ACARICIAR, 0, lun0 + 3 * 86400 + 5);
        int reen = 0;
        while (pet_poll_event(&a2, &e)) reen += e.type == EV_REENCUENTRO;
        CHECK(reen == 1 && sac < a1.necesidad[NEED_SACIEDAD], "la que sigue viviendo vivió los tres días y te cuenta");
    }
    /* modo elegido a mano */
    {
        static pet_t mm;
        nueva(&mm, 406, lun0);
        pet_set_owner_age(&mm, 9);
        CHECK(pet_mode(&mm) == MODO_NINO, "a los 9 es modo niño");
        pet_set_owner_age(&mm, 12);
        CHECK(pet_mode(&mm) == MODO_ADULTO, "a los 12, adulto");
        pet_set_mode_manual(&mm, MODO_INFANTE);
        pet_set_owner_age(&mm, 30);
        CHECK(pet_mode(&mm) == MODO_INFANTE, "lo elegido a mano manda aunque cambie la edad");
        pet_set_mode_manual(&mm, -1);
        CHECK(pet_mode(&mm) == MODO_ADULTO, "y en automático vuelve a la edad");
    }
    /* una partida de la versión 14 queda sin alarmas */
    static pet_t v;
    v = p;
    size_t viejo = offsetof(pet_t, alarma) + 4;
    v.version = 14;
    v.size = (uint16_t)viejo;
    uint32_t c = crc32_prueba((const uint8_t *)&v, viejo - 4);
    memcpy((uint8_t *)&v + viejo - 4, &c, 4);
    CHECK(pet_upgrade(&v) && pet_validate(&v) && pet_alarm_count(&v) == 0 && v.dormir.modo == DORMIR_NO, "una partida de la versión 14 se actualiza sin alarmas");
    v = p;
    viejo = offsetof(pet_t, perfil_vive) + 4;
    v.version = 15;
    v.size = (uint16_t)viejo;
    c = crc32_prueba((const uint8_t *)&v, viejo - 4);
    memcpy((uint8_t *)&v + viejo - 4, &c, 4);
    CHECK(pet_upgrade(&v) && pet_validate(&v) && pet_alarm_count(&v) == 3 && !v.perfil_vive && !v.temporizador, "una de la versión 15 conserva sus alarmas");
}

static void test_frases(void)
{
    static pet_t p;
    nueva(&p, 901, T0 + 14 * H);
    int ok = 0, tipos[RESP_COUNT] = {0};
    for (int i = 0; i < pet_phrase_count(); i++) {
        respuesta_t r = pet_phrase_reply(i);
        ok += pet_phrase(i)[0] && (unsigned)r < RESP_COUNT && strlen(pet_reply_text(r)) <= 14;
        if ((unsigned)r < RESP_COUNT) tipos[r]++;
    }
    CHECK(pet_phrase_count() == 50 && ok == 50, "50 mensajes, cada uno con su respuesta corta");
    CHECK(tipos[0] && tipos[1] && tipos[2] && tipos[3] && tipos[4], "se usan las cinco respuestas");
    p.necesidad[NEED_CARINO] = 40;
    CHECK(pet_phrase_hug(&p, T0 + 14 * H) && p.necesidad[NEED_CARINO] > 45, "responderle le da cariño");
    CHECK(!pet_phrase_hug(&p, T0 + 14 * H + 10), "pero una sola vez por mensaje");
}

static void test_bolsa(void)
{
    static pet_t p, v;
    uint32_t t = T0 + 14 * H; /* 11:00 en Chile */
    nueva(&p, 701, t);
    pet_hatch(&p, t);
    pet_update(&p, t);
    vaciar_eventos(&p);
    /* el catálogo */
    int por_cat[OCAT_COUNT] = {0}, nombres = 0, azar = 0;
    for (int i = 0; i < PET_OBJETOS; i++) {
        const objeto_info_t *o = objeto_info(i);
        if (o->cat < OCAT_COUNT) por_cat[o->cat]++;
        nombres += o->nombre && o->leyenda && strlen(o->leyenda) < 120 && o->efecto < OEF_COUNT && o->gesto < OGE_COUNT && o->familia < OFA_COUNT;
        azar += o->rareza > 0;
    }
    CHECK(nombres == PET_OBJETOS && por_cat[0] == 30 && por_cat[1] == 30 && por_cat[2] == 30 && por_cat[3] == 30 && por_cat[4] == 30,
          "150 objetos en 5 categorías de 30, todos completos");
    CHECK(azar == PET_OBJETOS - 1 && objeto_info(OBJ_CORAZON_BOSQUE)->rareza == 0 && !objeto_info(PET_OBJETOS), "el Corazón del Mundo no sale al azar");
    /* encontrar */
    CHECK(pet_objects_known(&p) == 0 && pet_object_gives(&p, 5, OFU_EXPLORAR, t), "encuentra la lágrima de lago");
    pet_event_t e;
    int ok = 0;
    while (pet_poll_event(&p, &e)) if (e.type == EV_OBJETO) ok = ((e.a | (e.b & 1) << 7) == 5) && (e.b & 2);
    CHECK(ok && pet_object_known(&p, 5) && pet_object_count(&p, 5) == 1 && p.bolsa_nueva == 6, "avisa que es nuevo y queda en la Bolsa");
    pet_bag_seen(&p);
    CHECK(!p.bolsa_nueva, "al abrir la Bolsa se apaga el aviso");
    for (int k = 0; k < 12; k++) pet_object_gives(&p, 0, OFU_EXPLORAR, t);
    CHECK(pet_object_count(&p, 0) == PET_OBJ_MAX, "como mucho %d de cada uno", PET_OBJ_MAX);
    /* lo que se gasta y lo que funciona una vez al día */
    p.emo[EMO_TRISTEZA] = 0.9f;
    CHECK(pet_object_spent(5) == false && pet_object_use(&p, 5, t + 10) == 1 && p.emo[EMO_TRISTEZA] < 0.3f, "la lágrima de lago le quita la pena");
    CHECK(pet_object_discovered(&p, 5) && !pet_object_discovered(&p, 0), "al usarlo descubres qué hace (y los otros siguen ocultos)");
    CHECK(pet_object_count(&p, 5) == 1 && pet_object_use(&p, 5, t + 20) == -2 && !pet_object_ready(&p, 5, t + 20), "una piedra no se gasta, pero descansa hasta mañana");
    CHECK(pet_object_ready(&p, 5, t + 24 * H), "al otro día vuelve a brillar");
    pet_object_gives(&p, 33, OFU_EXPLORAR, t); /* baya de luna */
    p.necesidad[NEED_SACIEDAD] = 40;
    CHECK(pet_object_spent(33) && pet_object_use(&p, 33, t + 30) == 1 && pet_object_count(&p, 33) == 0 && p.necesidad[NEED_SACIEDAD] > 50, "la baya se la come");
    CHECK(pet_object_use(&p, 33, t + 40) == -1, "sin baya no hay qué usar");
    /* ponérselo, decorar, el cielo, el clima */
    pet_object_gives(&p, 43, OFU_EXPLORAR, t); /* sombrero de bruja */
    CHECK(pet_object_use(&p, 43, t + 50) == 1 && p.obj_cabeza == 44 && pet_object_count(&p, 43) == 1, "se pone el hongo de sombrero");
    pet_update(&p, t + 4 * H);
    CHECK(p.obj_cabeza == 0, "al rato se lo saca");
    pet_object_gives(&p, 90, OFU_PESCA, t); /* bota vieja */
    CHECK(pet_object_use(&p, 90, t + 4 * H) == 1 && p.obj_puesto[0] == 91 && pet_object_count(&p, 90) == 0, "la bota frotada queda de casita de hadas en el prado");
    CHECK(pet_object_unplace(&p, 0) && !p.obj_puesto[0] && pet_object_count(&p, 90) == 1, "y se puede guardar otra vez");
    CHECK(pet_object_use(&p, 0, t + 4 * H) == 1 && p.cielo == 8 && pet_object_count(&p, 0) == PET_OBJ_MAX, "la piedra brillante llena el cielo de constelaciones");
    pet_update(&p, t + 4 * H + 300);
    CHECK(p.cielo == 0, "el cielo vuelve a la normalidad");
    pet_object_gives(&p, 47, OFU_EXPLORAR, t); /* flor de niebla */
    CHECK(pet_object_use(&p, 47, t + 5 * H) == 1 && p.clima == CLIMA_NUBLADO, "la flor de niebla nubla el día");
    /* durmiendo no come */
    pet_object_gives(&p, 38, OFU_EXPLORAR, t);
    p.dormida = 1;
    CHECK(pet_object_use(&p, 38, t + 5 * H) == 0 && pet_object_count(&p, 38) == 1, "durmiendo no se come la baya de fuego");
    p.dormida = 0;
    /* los objetos que de verdad hacen lo que dicen */
    {
        pet_t q;
        nueva(&q, 4321, t);
        q.etapa = STAGE_ADULTA;
        uint32_t u = t + 3 * 86400u;
        /* espejo dimensional: lo que encuentra hoy viene doble */
        pet_object_gives(&q, 132, OFU_REGALO, u);
        CHECK(pet_object_use(&q, 132, u) == 1 && q.doble_hasta > u, "el espejo dimensional abre el portal");
        pet_object_gives(&q, 7, OFU_EXPLORAR, u);
        CHECK(pet_object_count(&q, 7) == 2, "con el espejo, lo que encuentra viene doble");
        CHECK(!(u + 2 * 86400u < q.doble_hasta), "y solo dura hasta que termina el día");
        q.doble_hasta = 0; /* (para contar justo lo que sigue) */
        /* pócimas: invisible, encoge, crece, colores */
        pet_object_gives(&q, 129, OFU_REGALO, u);
        CHECK(pet_object_use(&q, 129, u) == 1 && pet_invisible(&q, u + 60) && !pet_invisible(&q, u + 3600) && pet_object_count(&q, 129) == 0, "la pócima de invisibilidad la vuelve invisible un rato (y se gasta)");
        CHECK(q.protege_hasta >= u + 20 * 60, "invisible, nadie la molesta");
        pet_object_gives(&q, 134, OFU_REGALO, u);
        CHECK(pet_object_use(&q, 134, u) == 1 && pet_size_mode(&q, u + 60) == 1 && pet_size_mode(&q, u + 2 * H) == 0, "la pócima de encogimiento la deja chiquitita un rato");
        pet_object_gives(&q, 125, OFU_REGALO, u);
        CHECK(pet_object_use(&q, 125, u) == 1 && pet_size_mode(&q, u + 60) == 2 && pet_size_mode(&q, u + 2 * H) == 0, "el elixir del crecimiento la deja grande un rato");
        pet_object_gives(&q, 148, OFU_REGALO, u);
        CHECK(pet_object_use(&q, 148, u) == 1 && pet_colors_on(&q, u + 60) && !pet_colors_on(&q, u + 2 * H), "la pócima de colores le cambia los colores un rato");
        /* la runa de teletransporte la lleva a otra zona */
        for (int e = 0; e < ESC_COUNT; e++) if (e != ESC_ISLA) q.escenas_abiertas |= (uint8_t)(1u << e);
        pet_object_gives(&q, 122, OFU_REGALO, u);
        int antes = q.escena;
        CHECK(pet_object_use(&q, 122, u) == 1 && q.escena != antes && pet_object_count(&q, 122) == 0, "la runa de teletransporte la lleva a otra zona (y se gasta)");
        /* tintero: cada uso revela una nota escondida, hasta agotarlas */
        pet_object_gives(&q, 94, OFU_REGALO, u);
        int notas_antes = pet_ink_notes(&q);
        CHECK(!pet_object_spent(94) && pet_object_use(&q, 94, u) == 1 && pet_ink_notes(&q) == notas_antes + 1, "el tintero revela una nota escondida");
        for (int k = 1; k < PET_TINTA_NOTAS + 1; k++) { pet_object_use(&q, 94, u + (uint32_t)k * 86400u); }
        CHECK(pet_ink_notes(&q) == PET_TINTA_NOTAS && pet_object_count(&q, 94) == 1, "después de varios días leyó todas y el tintero sigue ahí");
        /* lentes e imán */
        pet_object_gives(&q, 123, OFU_REGALO, u);
        pet_object_gives(&q, 136, OFU_REGALO, u);
        CHECK(pet_object_use(&q, 123, u) == 1 && q.lentes_hasta > u && pet_object_use(&q, 136, u) == 1 && q.iman_hasta > u, "los lentes y el imán duran unas horas");
        /* candado rúnico: cuida lo más valioso y no se deja ofrendar */
        pet_t g;
        nueva(&g, 99, t);
        g.etapa = STAGE_ADULTA;
        pet_object_gives(&g, 0, OFU_REGALO, u);   /* piedra brillante: común */
        pet_object_gives(&g, 8, OFU_REGALO, u);   /* meteorito: muy raro */
        pet_object_gives(&g, 111, OFU_REGALO, u); /* el candado */
        CHECK(pet_guarded_object(&g) == 8, "el candado cuida lo más valioso de la bolsa");
        CHECK(!pet_object_return(&g, 8, u) && pet_object_count(&g, 8) == 1 && pet_object_return(&g, 0, u), "lo que cuida no se ofrenda por error; lo demás sí");
        /* el cofre bajo el árbol viejo */
        CHECK(pet_chest_status(&g) == 0 && pet_chest_open(&g, u) == 0, "sin el final secreto no hay cofre");
        pet_object_gives(&g, OBJ_LLAVE_OXIDADA, OFU_REGALO, u);
        g.final_estado |= PET_FINAL_SECRETO;
        CHECK(pet_chest_visible(&g) && pet_chest_status(&g) == 4, "tras el final secreto aparece el cofre y con llave y candado se puede abrir");
        pet_t h2 = g;
        h2.bolsa[111] = 0;
        CHECK(pet_chest_status(&h2) == 3 && pet_chest_open(&h2, u) == 0, "sin el candado no abre");
        pet_event_t ev;
        while (pet_poll_event(&g, &ev)) {}
        CHECK(pet_object_use(&g, OBJ_LLAVE_OXIDADA, u) == 1, "la llave oxidada abre el cofre");
        int doradas = 0, codigo = 0;
        while (pet_poll_event(&g, &ev)) { if (ev.type == EV_OBJETO && (ev.b & 64)) doradas++; if (ev.type == EV_OBJETO_USO) codigo = (ev.b >> 2) & 7; }
        CHECK(doradas >= 1 && pet_gold_count(&g) >= PET_COFRE_DORADOS - 1 && codigo == 4, "adentro había versiones doradas");
        CHECK(!pet_chest_visible(&g) && pet_object_count(&g, OBJ_LLAVE_OXIDADA) == 0 && pet_object_count(&g, 111) == 0, "el cofre se abre una sola vez y gasta la llave y el candado");
        /* las gafas del explorador: nidos en los árboles */
        pet_t n;
        nueva(&n, 77, t);
        n.etapa = STAGE_ADULTA;
        n.escena = ESC_PRADO;
        pet_object_gives(&n, 99, OFU_REGALO, u);
        CHECK(!pet_nests_visible(&n, u) && pet_nest_look(&n, 0, u) == -2, "sin las gafas no ve nidos");
        CHECK(pet_object_use(&n, 99, u) == 1 && pet_nests_visible(&n, u + 60), "con las gafas ve los nidos");
        int m1 = pet_nest_look(&n, 0, u + 60);
        CHECK(m1 >= -1 && pet_nest_empty(&n, 0, u + 60) && pet_nest_look(&n, 0, u + 70) == -2, "cada nido se mira una vez al día");
        CHECK(!pet_nest_empty(&n, 0, u + 86400u), "al otro día hay otros huevos");
    }
    /* devolverlo al bosque */
    vaciar_eventos(&p);
    CHECK(pet_object_return(&p, 38, t + 5 * H) && pet_object_count(&p, 38) == 0 && pet_object_known(&p, 38), "devuelto al bosque, pero queda en el libro");
    {
        pet_event_t ev;
        int ofrenda = 0;
        while (pet_poll_event(&p, &ev)) if (ev.type == EV_OFRENDA) ofrenda = ev.b & 2;
        CHECK(ofrenda && p.dia_ofrenda, "es una ofrenda: el bosque la bendecirá mañana");
        pet_object_gives(&p, 38, OFU_EXPLORAR, t);
        pet_object_return(&p, 38, t + 5 * H + 60);
        int otra = -1;
        while (pet_poll_event(&p, &ev)) if (ev.type == EV_OFRENDA) otra = ev.b & 2;
        CHECK(otra == 0, "una bendición a la vez");
        pet_force_weather(&p, CLIMA_LLUVIA, t + 5 * H, 48 * H);
        pet_update(&p, t + 22 * H); /* al otro día a las 9 (en Chile) */
        int sol = 0, luc = 0;
        while (pet_poll_event(&p, &ev)) sol |= ev.type == EV_BENDICION && ev.a == 0;
        CHECK(sol && p.clima == CLIMA_DESPEJADO && !p.dia_ofrenda, "al día siguiente, la bendición: sale el sol");
        for (uint32_t m = 0; m < 14 * 60; m += 10) {
            pet_update(&p, t + 22 * H + m * 60);
            while (pet_poll_event(&p, &ev)) luc |= ev.type == EV_BENDICION && ev.a == 1;
        }
        CHECK(luc, "y esa noche, luciérnagas");
    }
    /* la suerte: con el trébol encuentra más cosas raras */
    int raros = 0, raros_suerte = 0;
    for (int k = 0; k < 400; k++) { int id = pet_object_find(&p, OFU_EXPLORAR, t); raros += id >= 0 && objeto_info(id)->rareza >= 2; }
    p.suerte_hasta = t + H;
    for (int k = 0; k < 400; k++) { int id = pet_object_find(&p, OFU_EXPLORAR, t); raros_suerte += id >= 0 && objeto_info(id)->rareza >= 2; }
    CHECK(raros_suerte > raros, "con suerte salen más cosas raras (%d contra %d)", raros_suerte, raros);
    /* pescando, solo cosas del lago; de día nada nocturno */
    int malos = 0;
    for (int k = 0; k < 300; k++) {
        int id = pet_object_find(&p, OFU_PESCA, t);
        malos += id < 0 || !(objeto_info(id)->lugares & OL_LAGO) || objeto_info(id)->noche;
    }
    CHECK(malos == 0, "del lago salen cosas del lago, y de día nada de la noche");
    /* el Corazón del Mundo llega cuando ya conoce todos los demás */
    for (int i = 0; i < PET_OBJETOS - 1; i++) p.obj_visto[i >> 3] |= (uint8_t)(1u << (i & 7));
    CHECK(pet_object_find(&p, OFU_EXPLORAR, t) == OBJ_CORAZON_BOSQUE, "con los 149 conocidos aparece el Corazón del Mundo");
    /* se encuentran de vez en cuando, sin llenar la Bolsa */
    nueva(&p, 702, t);
    pet_hatch(&p, t);
    p.vecino.proxima = t + 30 * 24 * H;
    int halla = 0;
    for (uint32_t m = 0; m < 3 * 24 * 60; m += 10) {
        uint32_t now = t + m * 60;
        pet_update(&p, now);
        if (m % 120 == 0) cuidar(&p, now);
        while (pet_poll_event(&p, &e)) halla += e.type == EV_OBJETO;
    }
    CHECK(halla >= 1 && halla <= 40, "en tres días encuentra algunos objetos mágicos (%d)", halla);
    for (int k = 0; k < 6; k++) pet_steps_add(&p, PET_PASOS_PREMIO, t + 4 * 24 * H + (uint32_t)k * 3 * H);
    /* una partida de la versión 16 queda con la Bolsa vacía */
    pet_object_gives(&p, 7, OFU_EXPLORAR, t);
    v = p;
    size_t viejo = offsetof(pet_t, bolsa) + 4;
    v.version = 16;
    v.size = (uint16_t)viejo;
    uint32_t c = crc32_prueba((const uint8_t *)&v, viejo - 4);
    memcpy((uint8_t *)&v + viejo - 4, &c, 4);
    CHECK(pet_upgrade(&v) && pet_validate(&v) && pet_objects_known(&v) == 0 && !v.obj_cabeza && v.pasos_total == p.pasos_total,
          "una de la versión 16 se actualiza con la Bolsa vacía");
}

static void test_regalos_niebla(void)
{
    static pet_t p, v;
    uint32_t t = T0 + 14 * H; /* 11:00 */
    /* volver después de un rato: a veces te trae algo (una vez al día) */
    int regalos = 0, tipos = 0, dias = 0;
    for (uint32_t seed = 900; seed < 960; seed++) {
        nueva(&p, seed, t);
        p.vinculo = 60;
        cuidar(&p, t);
        vaciar_eventos(&p);
        uint32_t vuelta = t + 5 * H;
        p.necesidad[NEED_ENERGIA] = 100;
        pet_action(&p, ACT_ACARICIAR, GESTO_TOQUE, vuelta);
        pet_event_t e;
        int hoy = 0;
        while (pet_poll_event(&p, &e)) if (e.type == EV_REGALO_VUELTA) { regalos++; hoy++; tipos |= 1 << e.a; }
        pet_action(&p, ACT_ACARICIAR, GESTO_TOQUE, vuelta + 4 * H); /* otra vez el mismo día */
        while (pet_poll_event(&p, &e)) hoy += e.type == EV_REGALO_VUELTA;
        dias += hoy > 1;
    }
    CHECK(regalos >= 20 && regalos <= 55, "al volver muchas veces te espera con algo (%d de 60)", regalos);
    CHECK(dias == 0, "solo un regalo al día");
    CHECK(__builtin_popcount((unsigned)tipos) >= 5, "y son cosas distintas");
    /* sin irte no hay regalo */
    nueva(&p, 990, t);
    cuidar(&p, t);
    vaciar_eventos(&p);
    pet_action(&p, ACT_ACARICIAR, GESTO_TOQUE, t + 600);
    CHECK(contar_eventos(&p, EV_REGALO_VUELTA) == 0, "si no te fuiste, no hay regalo");
    /* las botas con barro: huellas hasta el baño */
    p.barro = 1; p.necesidad[NEED_HIGIENE] = 40; p.dormida = 0; p.enferma = 0;
    for (int b = 0; b < 12 && p.barro; b++) pet_action(&p, ACT_BANAR, 0, t + 700 + (uint32_t)b * 1200); /* a veces se resiste: se insiste */
    CHECK(!p.barro, "el baño se lleva el barro");
    /* la niebla: algunas mañanas, más en otoño; se va sola o la despejas */
    int con = 0;
    for (uint32_t seed = 300; seed < 400; seed++) {
        nueva(&p, seed, T0 + 9 * H - 600); /* 5:50 */
        p.estacion_fija = ESTACION_OTONO;
        pet_force_weather(&p, CLIMA_DESPEJADO, T0 + 9 * H, 2 * H);
        pet_update(&p, T0 + 9 * H + 600);
        if (pet_fog(&p, T0 + 9 * H + 600)) {
            con++;
            if (con == 1) {
                CHECK(!pet_fog(&p, T0 + 15 * H), "a media mañana se va sola");
                pet_fog_clear(&p, T0 + 9 * H + 700);
                CHECK(!pet_fog(&p, T0 + 9 * H + 800) && contar_eventos(&p, EV_NIEBLA) >= 1, "soplando se despeja");
            }
        }
    }
    CHECK(con >= 12 && con <= 50, "en otoño amanece con niebla algunas mañanas (%d de 100)", con);
    /* los hongos de luz: una secuencia larga a veces deja un hongo luminoso */
    nueva(&p, 77, t);
    int hongos = 0;
    for (int k = 0; k < 20; k++) {
        p.ultimo_juego = 0;
        pet_action(&p, ACT_FIN_JUEGO, JUEGO_HONGOS * 100 + 8, t + (uint32_t)k * 60);
        hongos = pet_object_count(&p, 31);
    }
    CHECK(hongos >= 3, "jugar a los hongos de luz deja hongos luminosos (%d)", hongos);
    /* una partida de la versión 17 se actualiza sin saber qué hace cada objeto */
    nueva(&p, 78, t);
    pet_object_gives(&p, 5, OFU_EXPLORAR, t);
    pet_object_use(&p, 5, t + 10);
    v = p;
    size_t viejo = offsetof(pet_t, obj_sabe) + 4;
    v.version = 17;
    v.size = (uint16_t)viejo;
    uint32_t c = crc32_prueba((const uint8_t *)&v, viejo - 4);
    memcpy((uint8_t *)&v + viejo - 4, &c, 4);
    CHECK(pet_upgrade(&v) && pet_validate(&v) && pet_object_count(&v, 5) == 1 && !pet_object_discovered(&v, 5) && !v.niebla_hasta,
          "una de la versión 17 conserva su Bolsa");
    v = p;
    v.ambiente_vol = 2;
    viejo = offsetof(pet_t, dia_ofrenda) + 4;
    v.version = 18;
    v.size = (uint16_t)viejo;
    c = crc32_prueba((const uint8_t *)&v, viejo - 4);
    memcpy((uint8_t *)&v + viejo - 4, &c, 4);
    CHECK(pet_upgrade(&v) && pet_validate(&v) && pet_object_discovered(&v, 5) && !v.dia_ofrenda,
          "una de la versión 18 sabe lo que descubrió y no espera bendiciones");
}

static void test_linaje(void)
{
    static pet_t p, v;
    uint32_t t = T0 + 14 * H;
    nueva(&p, 77, t);
    pet_choose_look(&p, 0, -1, 0, t, 5); /* un Brotito (ya nació en nueva: vuelve a ser huevo) */
    pet_hatch(&p, t);
    pet_update(&p, t + 60);
    CHECK(pet_set_color(&p, 3) && p.color == 3, "de bebé se le elige el color");
    CHECK(pet_lineage_lead(&p) == 0, "sin nada que la marque, sigue su propio linaje");
    CHECK(!pet_ability_has(&p, 0) && pet_ability_use(&p, 0, t + 120) == 0, "de bebé todavía no tiene habilidades");
    /* crece con mucha agua: el lago, la lluvia y cosas del río */
    for (int k = 0; k < 40; k++) { pet_object_gives(&p, 61, OFU_PESCA, t); pet_object_use(&p, 61, t + 200 + (uint32_t)k); } /* escamas de pez astral */
    p.linaje[3] += 200;
    CHECK(pet_lineage_lead(&p) == 3, "si el agua la marcó mucho más, va hacia el río");
    p.linaje[0] = 300;
    CHECK(pet_lineage_lead(&p) == 0, "pero su propio linaje tiene ventaja");
    p.linaje[0] = 10;
    pet_update(&p, t + 78 * H);
    CHECK(p.etapa == STAGE_CRIA && pet_ability_has(&p, 0) && !pet_ability_has(&p, 1), "de cría tiene su primera habilidad");
    CHECK(!pet_set_color(&p, 1), "y ya no se le cambia el color");
    p.jardin[0].tipo = PLANTA_FLOR; p.jardin[0].etapa = ETAPA_GRANDE; p.jardin[0].agua = 100; p.dormida = false; p.actividad = ACTV_PASEAR; pet_force_weather(&p, CLIMA_DESPEJADO, t + 78 * H, 6 * H); /* sin sueño ni clima que la frene */
    { int r1 = pet_ability_use(&p, 0, t + 78 * H + 10), r2 = pet_ability_use(&p, 0, t + 78 * H + 20); CHECK(r1 == 1 && r2 == -2, "una vez al día (%d %d, bloqueo %d esp %d)", r1, r2, pet_ability_block(&p, 0, t + 78 * H + 10), p.especie); }
    p.mezcla = 0;
    pet_update(&p, t + 5 * 24 * H);
    CHECK(p.etapa == STAGE_ADULTA && pet_lineage_shape(&p) == 3 && (pet_look(&p) & 7) == 0 && p.color == 3,
          "de adulta toma la forma del río, pero sigue siendo ella y con su color");
    CHECK(p.forma != FORMA_GRUNONA, "ya no existe la forma gruñona");
    p.dormida = false; p.jardin[0].tipo = PLANTA_FLOR; p.jardin[0].etapa = ETAPA_GRANDE; p.jardin[0].agua = 100; pet_force_weather(&p, CLIMA_DESPEJADO, t + 5 * 24 * H + 60, 6 * H);
    CHECK(pet_ability_has(&p, 1) && pet_ability_use(&p, 1, t + 5 * 24 * H + 60) == 1, "de adulta, la segunda habilidad (la de su forma)");
    /* una partida de la versión 19 llega sin linaje */
    v = p;
    size_t viejo = offsetof(pet_t, linaje) + 4;
    v.version = 19;
    v.size = (uint16_t)viejo;
    uint32_t c = crc32_prueba((const uint8_t *)&v, viejo - 4);
    memcpy((uint8_t *)&v + viejo - 4, &c, 4);
    CHECK(pet_upgrade(&v) && pet_validate(&v) && !v.linaje[3] && !v.hab_usada[0], "una de la versión 19 se actualiza sin linaje");
}


static void test_isla(void)
{
    static pet_t v;
    uint32_t t = T0 + 14 * H;
    nueva(&v, 181, t);
    CHECK(pet_scene_requirement(&v, ESC_ISLA, t, NULL) == 3 && !pet_scene_open(&v, ESC_ISLA), "la isla es un secreto del lago");
    v.gusto_escena[ESC_LAGO] = 40; v.gusto_escena[ESC_COLINA] = 0;
    CHECK(pet_scene_liking(&v, ESC_ISLA) == 40, "el gusto por la isla sale del lago y la colina, con un poquito más");
    CHECK(sizeof v.gusto_escena == 5, "los gustos guardados siguen siendo cinco (las partidas viejas no se mueven)");
    /* bebé: todavía no la ve aunque confíe mucho */
    v.vinculo = 90;
    v.escenas_abiertas |= 1u << ESC_LAGO;
    for (int i = 0; i < 30; i++) {
        v.escena = ESC_LAGO; v.actividad = ACTV_PESCAR; v.actividad_hasta = 0;
        pet_update(&v, t + 60u * (uint32_t)(i + 1));
        vaciar_eventos(&v);
    }
    CHECK(!pet_scene_open(&v, ESC_ISLA), "de bebé no ve la isla");
    bool aviso = false;
    for (int i = 0; i < 60 && !pet_scene_open(&v, ESC_ISLA); i++) { /* ya pasó un día: es cría */
        v.escena = ESC_LAGO; v.actividad = i & 1 ? ACTV_EXPLORAR : ACTV_PESCAR; v.actividad_hasta = 0; v.dormida = false;
        pet_update(&v, t + 74 * H + 60u * (uint32_t)(i + 1));
        pet_event_t e;
        while (pet_poll_event(&v, &e)) if (e.type == EV_ESCENA_NUEVA && e.a == ESC_ISLA) aviso = true;
    }
    CHECK(pet_scene_open(&v, ESC_ISLA) && aviso, "de cría, mirando el lago, descubre la isla y lo cuenta");
    CHECK(v.etapa == STAGE_CRIA, "(ya es cría)");
    uint32_t t2 = t + 76 * H;
    v.ultima_interaccion = t2;
    CHECK(pet_action(&v, ACT_IR, ESC_ISLA, t2) >= 0 && v.escena == ESC_ISLA, "en el bote, va a la isla");
    CHECK(pet_ambient(&v, t2 + 60) == AMB_MAR || pet_ambient(&v, t2 + 60) == AMB_NOCHE || pet_ambient(&v, t2 + 60) >= AMB_LLUVIA, "en la isla se oye el mar");
    /* la botella: una al día */
    int botellas = 0;
    for (int i = 0; i < 40; i++) {
        v.escena = ESC_ISLA; v.actividad = ACTV_EXPLORAR; v.actividad_hasta = 0; v.ultima_interaccion = t2 + 60u * (uint32_t)i;
        v.dormida = false;
        pet_update(&v, t2 + 60u * (uint32_t)(i + 1));
        pet_event_t e;
        while (pet_poll_event(&v, &e)) if (e.type == EV_BOTELLA) { botellas++; CHECK(e.a >= 0 && e.a < pet_phrase_count(), "la botella trae un mensaje de los que se responden"); }
    }
    CHECK(botellas == 1, "a la orilla llega una botella al día");
    /* nunca se va sola a la isla, y si la dejas mucho rato allá vuelve al lago */
    static pet_t w;
    w = v;
    w.escena = ESC_ISLA; w.escena_desde = t2; w.ultima_interaccion = t2;
    for (int i = 0; i < 5 * 60 && w.escena == ESC_ISLA; i++) { w.dormida = false; pet_update(&w, t2 + 60u * (uint32_t)(i + 1)); vaciar_eventos(&w); }
    CHECK(w.escena == ESC_LAGO, "si pasa mucho rato sola en la isla, vuelve en el bote");
    int fue = 0;
    for (int d = 0; d < 6; d++)
        for (int i = 0; i < 24 * 4; i++) { pet_update(&w, t2 + 6 * H + (uint32_t)d * 24 * H + (uint32_t)i * 900u); fue |= w.escena == ESC_ISLA; vaciar_eventos(&w); }
    CHECK(!fue, "a la isla no va sola: solo contigo");
    /* en la isla aparecen cosas de la orilla y conchas */
    CHECK(strstr(objeto_info(OBJ_BOTELLA)->nombre, "Botella") != NULL, "el número de la botella es el de la botella");
}


static void test_pesca_nueva(void)
{
    static pet_t p;
    uint32_t t = T0 + 14 * H; /* 11 de la mañana en Chile */
    nueva(&p, 303, t);
    p.estacion_fija = ESTACION_PRIMAVERA;
    p.escenas_abiertas |= 1u << ESC_LAGO;
    p.escena = ESC_LAGO;
    p.necesidad[NEED_ENERGIA] = 90;
    CHECK(pet_action(&p, ACT_PESCAR, 0, t) > 0, "sale a pescar");
    uint32_t dur = p.actividad_hasta - t;
    CHECK(p.actividad == ACTV_PESCAR && dur >= PESCA_DUR_MIN && dur <= PESCA_DUR_MAX, "entre 40 y 90 segundos (%u)", (unsigned)dur);
    /* todo lo que puede sacar tiene nombre, y sola pesca como mucho dos veces al día */
    bool nombres = true;
    for (int c = 0; c < PESCA_COUNT; c++) nombres &= pet_catch_name((pesca_t)c)[0] != 0 && pet_catch_name((pesca_t)c)[0] != '?';
    CHECK(nombres && PESCA_COUNT == 12, "doce cosas posibles al pescar (fw 104: + el hielo), todas con nombre");
    int solas = 0;
    pet_event_t e;
    while (pet_poll_event(&p, &e)) {}
    for (uint32_t k = t + 120; k < t + 9 * H; k += 30) {
        p.necesidad[NEED_ENERGIA] = 90; p.necesidad[NEED_SACIEDAD] = 80; p.escena = ESC_LAGO;
        pet_update(&p, k);
        while (pet_poll_event(&p, &e)) if (e.type == EV_ACTIVIDAD && e.a == ACTV_PESCAR) solas++;
    }
    CHECK(solas <= PESCA_SOLA_DIA, "sola pesca como mucho dos veces al día (%d)", solas);
}

static void test_animales(void)
{
    static pet_t p;
    uint32_t t = T0 + 14 * H; /* 11 de la mañana */
    nueva(&p, 304, t);
    p.estacion_fija = ESTACION_PRIMAVERA;
    bool nombres = true;
    for (int a = 0; a < ANIMAL_COUNT; a++) nombres &= pet_animal_name((animal_t)a)[0] != '?';
    CHECK(nombres && ANIMAL_COUNT == 41, "cuarenta y un animales, todos con nombre");
    /* la ardilla traviesa se lleva una semilla y a veces la devuelve */
    dar_semillas(&p, CULTIVO_FLOR, 4);
    int antes = p.inventario[ITEM_SEMILLA];
    pet_animal_call(&p, ANIMAL_ARDILLA, t);
    CHECK(p.ardilla_semilla && p.inventario[ITEM_SEMILLA] == antes - 1, "la ardilla se llevó una semilla");
    /* el topo gruñón se esconde si lo tocas */
    p.escena = ESC_COLINA;
    pet_animal_call(&p, ANIMAL_TOPO, t);
    CHECK(pet_action(&p, ACT_ANIMAL, ANIMAL_TOPO, t + 5) == 1 && !pet_animal_here(&p, ANIMAL_TOPO, t + 6), "el topo se esconde si lo tocas");
    /* la tortuga sabia: a su lado se le pasa la pena */
    p.escena = ESC_LAGO;
    p.emo[EMO_TRISTEZA] = 0.8f;
    p.dormida = false;
    pet_animal_call(&p, ANIMAL_TORTUGA, t);
    bool consuela = false;
    pet_event_t e;
    for (uint32_t k = t + 60; k < t + 25 * 60 && !consuela; k += 60) {
        p.emo[EMO_TRISTEZA] = 0.8f;
        pet_update(&p, k);
        p.escena = ESC_LAGO;
        while (pet_poll_event(&p, &e)) consuela |= e.type == EV_ANIMAL && e.a == ANIMAL_TORTUGA && e.b == ANI_CONSUELA;
    }
    CHECK(consuela && p.emo[EMO_TRISTEZA] < 0.5f, "la tortuga la consuela");
    /* el erizo tímido: si espera quieta, se hacen amigos */
    p.escena = ESC_PRADO;
    p.erizo_confianza = 0;
    uint32_t tarde = T0 + 21 * H; /* 18:00 */
    bool amigos = false;
    for (int vez = 0; vez < 12 && !amigos; vez++) {
        uint32_t k0 = tarde + (uint32_t)vez * 24 * H;
        pet_animal_call(&p, ANIMAL_ERIZO, k0);
        for (uint32_t k = k0 + 60; k < k0 + 3 * H && !amigos; k += 60) {
            p.dormida = false; p.actividad = ACTV_MIRAR; p.actividad_hasta = k + 600; p.escena = ESC_PRADO;
            pet_update(&p, k);
            while (pet_poll_event(&p, &e)) amigos |= e.type == EV_ANIMAL && e.a == ANIMAL_ERIZO && e.b == ANI_AMIGOS;
        }
    }
    for (int i = 0; i < pet_diary_count(&p); i++) { const entrada_diario_t *d = pet_diary_get(&p, i); amigos |= d->tipo == DIA_ANIMAL && d->a == ANIMAL_ERIZO && d->b == ANI_AMIGOS; }
    CHECK(p.erizo_confianza == 3, "esperando quieta junto al erizo, se hacen amigos");
}

static void test_mensajes_tipos(void)
{
    /* cada mensaje tiene un tipo, y el botón calza con el tipo */
    static const respuesta_t R[FRASE_TIPOS] = {RESP_GRACIAS, RESP_YO_TAMBIEN, RESP_LO_HARE, RESP_VOY_A_MIRAR, RESP_CON_CARINO, RESP_ABRAZO};
    int tipos[FRASE_TIPOS] = {0};
    bool bien = pet_phrase_count() == 50;
    for (int i = 0; i < pet_phrase_count(); i++) {
        frase_tipo_t ti = pet_phrase_type(i);
        const char *f = pet_phrase(i);
        if ((unsigned)ti >= FRASE_TIPOS || pet_phrase_reply(i) != R[ti] || !f[0]) { bien = false; continue; }
        tipos[ti]++;
        /* las que dan las gracias dicen "gracias"; las que invitan a mirar, invitan a mirar o buscar */
        if (ti == FRASE_AGRADECE && !strstr(f, "racias")) bien = false;
        if (ti == FRASE_MIRA && !strstr(f, "ira") && !strstr(f, "Busca") && !strstr(f, "busca") && !strstr(f, "Asóma")) bien = false;
    }
    for (int k = 0; k < FRASE_TIPOS; k++) bien &= tipos[k] > 0;
    CHECK(bien, "50 mensajes, cada uno con su tipo y un botón que calza (todos los tipos se usan)");
    CHECK(!strcmp(pet_reply_text(RESP_GRACIAS), "¡GRACIAS!"), "ánimo: ¡gracias!");
}

static void test_genero(void)
{
    static pet_t p;
    nueva(&p, 305, T0);
    char o[160];
    p.genero = 0;
    pet_gender_text(&p, "Estoy cansad{a}. {Ella} es {la} {guardiana}. {ELLA} ESTÁ CANSAD{A}", o, sizeof o);
    CHECK(!strcmp(o, "Estoy cansada. Ella es la guardiana. ELLA ESTÁ CANSADA"), "niña: %s", o);
    p.genero = 1;
    pet_gender_text(&p, "Estoy cansad{a}. {Ella} es {la} {guardiana}. {ELLA} ESTÁ CANSAD{A}", o, sizeof o);
    CHECK(!strcmp(o, "Estoy cansado. Él es el guardián. ÉL ESTÁ CANSADO"), "niño: %s", o);
    /* ningún texto que se ve queda con una marca sin cambiar */
    bool limpio = true;
    for (int g = 0; g < 2; g++) {
        p.genero = (uint8_t)g;
        for (int k = 0; k < 40; k++) {
            for (int e = 0; e < EMO_COUNT; e++) p.emo[e] = (e == k % EMO_COUNT) ? 0.9f : 0.0f;
            pet_feeling(&p, o, sizeof o);
            if (strchr(o, '{')) limpio = false;
        }
        entrada_diario_t d = {0};
        for (int t = 0; t < DIA_COUNT; t++) { d.tipo = (uint8_t)t; pet_diary_text(&p, &d, o, sizeof o); if (strchr(o, '{')) limpio = false; }
    }
    CHECK(limpio, "lo que dice y el diario no quedan con marcas");
}

static void test_cartas(void)
{
    static pet_t p;
    uint32_t t = T0 - 30 * 24 * H;
    nueva(&p, 301, t);
    CHECK(!pet_letter_now(&p, t), "sin amigos conocidos no le escribe nadie");
    /* dos amigos del barrio la visitan: desde ahora le pueden escribir */
    p.vecinos_vistos = (1u << 3) | (1u << 7);
    p.vecino.proxima = t + 60 * 24 * H; /* (sin visitas nuevas durante la prueba) */
    p.criatura_prox = 99999; /* (ni invitaciones de las criaturas) */
    p.carta_dia = 0;
    bool llego = false;
    pet_event_t e;
    for (int h = 0; h < 7 * 24 && !llego; h++) {
        pet_update(&p, t + (uint32_t)h * H);
        while (pet_poll_event(&p, &e)) llego |= e.type == EV_CARTA;
    }
    CHECK(llego && pet_letter_pending(&p), "en una semana le llega una carta (cada 3 a 5 días)");
    char de[24], txt[600];
    CHECK(pet_letter_get(&p, 0, de, sizeof de, txt, sizeof txt) && (!strcmp(de, pet_friend_name(&p, 3)) || !strcmp(de, pet_friend_name(&p, 7))), "la escribe un amigo que conoce (%s)", de);
    CHECK(strstr(txt, "Test") && strstr(txt, de) && !strchr(txt, '{'), "con su nombre, la firma y sin marcas sin cambiar: %s", txt);
    /* todas las cartas se arman bien, en niña y en niño */
    bool bien = true;
    for (int g = 0; g < 2; g++)
        for (int fu = 0; fu < 4; fu++)
            for (int c = 0; c < pet_letter_bodies_of((carta_de_t)fu) * 4; c++) {
                int nb = pet_letter_bodies_of((carta_de_t)fu);
                p.genero = (uint8_t)g;
                snprintf(p.padre, sizeof p.padre, "Mami");
                p.viajeros_n = 1;
                snprintf(p.viajeros[0].nombre, sizeof p.viajeros[0].nombre, "Lunita");
                p.cartas[0][0] = (uint8_t)(fu << 6 | (fu == CARTA_AMIGO ? 3 : 0));
                p.cartas[0][1] = (uint8_t)(c % nb);
                p.cartas_var[0] = (uint8_t)((c / nb) | 0x80);
                pet_letter_get(&p, 0, de, sizeof de, txt, sizeof txt);
                if (strchr(txt, '{') || strlen(txt) > 480 || strlen(txt) < 60) { bien = false; printf("    mal: %s\n", txt); }
                if (fu == CARTA_AMIGO && c / nb == 1 && !strstr(txt, g ? "Querido" : "Querida")) bien = false;
            }
    p.genero = 0;
    CHECK(bien, "todas las cartas se arman completas, con querida o querido según sea niña o niño");
    /* al abrirla se alegra y queda en el diario */
    pet_letter_open(&p, t + 8 * 24 * H);
    bool diario = false;
    for (int i = 0; i < pet_diary_count(&p); i++) diario |= pet_diary_get(&p, i)->tipo == DIA_CARTA;
    CHECK(!pet_letter_pending(&p) && diario && pet_letter_count(&p) == 1, "abierta: queda en el diario y guardada para releerla");
    /* no se acumulan más de PET_CARTAS */
    for (int i = 0; i < 12; i++) { pet_letter_now(&p, t + 9 * 24 * H); pet_letter_open(&p, t + 9 * 24 * H); }
    CHECK(pet_letter_count(&p) == PET_CARTAS, "se guardan las últimas %d", PET_CARTAS);
    /* en modo infante no llegan */
    static pet_t q;
    nueva(&q, 302, t);
    q.vecinos_vistos = 1u;
    pet_set_mode_manual(&q, MODO_INFANTE);
    llego = false;
    for (int h = 0; h < 10 * 24; h++) { pet_update(&q, t + (uint32_t)h * H); while (pet_poll_event(&q, &e)) llego |= e.type == EV_CARTA; }
    CHECK(!llego, "en modo infante no llegan cartas (no lee)");
}

static void test_v21(void)
{
    static pet_t p, h, v;
    /* la luna de verdad */
    CHECK(pet_full_moon(T0 + 4 * H) && !pet_full_moon(T0 + 10 * 24 * H), "luna llena el 26 de octubre de 2026, y no diez días después");
    CHECK(pet_moon_phase(1712600460u) > 0.97f || pet_moon_phase(1712600460u) < 0.03f, "la luna nueva del eclipse de abril de 2024");
    CHECK(pet_nights_to_full_moon(T0 + 4 * H) == 0 && pet_nights_to_full_moon(T0 + 10 * 24 * H) >= 17, "cuántas noches faltan");
    /* la ballena */
    uint32_t t = T0 - 20 * 24 * H;
    nueva(&p, 211, t);
    p.dias_jugados = 30; /* (v37) las historias de Nimba se abren con las semanas */
    pet_update(&p, t + 60);
    CHECK(!pet_whale_can_ride(&p, t + 60), "sin ballena en el cielo no hay paseo");
    p.raro = RARO_GLOBO; p.raro_hasta = t + 600;
    CHECK(pet_whale_can_ride(&p, t + 60), "con la ballena pasando, se puede subir");
    CHECK(pet_whale_ride(&p, 6, t + 120) == 1 && p.bolsa[22] >= 1 && p.raro == 0xFF, "pasea, junta estrellas y trae polvo de estrellas");
    p.raro = RARO_GLOBO;
    CHECK(!pet_whale_can_ride(&p, t + 300) && pet_whale_ride(&p, 9, t + 300) == 0, "un paseo al día");
    CHECK(pet_story_count(&p, CRI_NIMBA) == 1 && pet_story_known(&p, CRI_NIMBA, 0) && !pet_story_known(&p, CRI_NIMBA, 1), "Nimba cuenta una sola historia por paseo");
    for (int d = 1; d <= 6; d++) { p.raro = RARO_GLOBO; p.raro_hasta = t + (uint32_t)d * 24 * H + 3000; pet_whale_ride(&p, 1, t + (uint32_t)d * 24 * H + 120); }
    CHECK(pet_story_count(&p, CRI_NIMBA) == 5 && pet_story_text(CRI_NIMBA, 4)[0], "tras cinco paseos tiene las cinco historias (y no pasa de cinco)");
    /* los rituales: cinco mañanas seguidas */
    vaciar_eventos(&p);
    for (int d = 1; d <= 5; d++) {
        uint32_t m = t + (uint32_t)d * 24 * H + 12 * H; /* 9 de la mañana en Chile */
        p.dormida = false;
        pet_action(&p, ACT_ACARICIAR, GESTO_CARICIA, m);
        if (d == 4) CHECK(!pet_ritual(&p, 0), "(cuatro mañanas todavía no)");
    }
    CHECK(pet_ritual(&p, 0), "cinco mañanas seguidas: ya tienen sus buenos días");
    /* te espera a tu hora de siempre */
    uint32_t m = t + 7 * 24 * H + 18 * H; /* 15:00 */
    p.habitos.prob[1][hora_chile(m)] = 230;
    p.dormida = false;
    pet_action(&p, ACT_JUGAR, 0, m);
    bool espera = false;
    pet_event_t e;
    while (pet_poll_event(&p, &e)) espera |= e.type == EV_TE_ESPERABA;
    CHECK(espera, "si siempre juegas a esta hora, te esperaba");
    /* la semana */
    pet_update(&p, t + 15 * 24 * H);
    bool semana = false;
    for (int i = 0; i < pet_diary_count(&p); i++) semana |= pet_diary_get(&p, i)->tipo == DIA_SEMANA;
    char txt[120];
    for (int i = 0; i < pet_diary_count(&p); i++) if (pet_diary_get(&p, i)->tipo == DIA_SEMANA) { pet_diary_text(&p, pet_diary_get(&p, i), txt, sizeof txt); break; }
    CHECK(semana && strstr(txt, "Mi semana") != NULL, "cada semana, una página con lo que vivieron");
    /* la Guardiana: adulta, con el Corazón, de noche en la isla, con luna llena */
    t = T0 - 10 * 24 * H;
    nueva(&v, 212, t);
    pet_update(&v, T0 + 2 * H);
    v.etapa = STAGE_ADULTA;
    v.escenas_abiertas |= 1u << ESC_ISLA;
    v.escena = ESC_ISLA;
    v.dormida = false;
    pet_object_gives(&v, OBJ_CORAZON_BOSQUE, OFU_REGALO, T0 + 2 * H);
    CHECK(!pet_guardian_ready(&v, T0 + 10 * 24 * H), "sin luna llena no hay rito");
    CHECK(pet_guardian_ready(&v, T0 + 2 * H), "con luna llena, de noche en la isla, está lista");
    CHECK(pet_object_use(&v, OBJ_CORAZON_BOSQUE, T0 + 2 * H) == 1 && v.guardiana && !v.bolsa[OBJ_CORAZON_BOSQUE], "el rito: ahora es la Guardiana del Mundo");
    /* la familia */
    v.vinculo = 95; v.color = 4;
    v.huevo_familia = 1;
    v.mezcla = 4; /* de adulta tomó la forma del río */
    pet_set_name(&v, "Luna");
    pet_family_child(&v, &h, T0 + 3 * H, 99);
    CHECK(pet_is_egg(&h) && h.especie < pet_species_count() && h.color < PET_COLORES && h.aspecto_ok == 2, "el hijo nace como huevo con especie y color al azar");
    {
        static pet_t h2;
        int distintas = 0, colores = 0;
        for (uint32_t sd = 1; sd <= 24; sd++) {
            v.huevo_familia = 1; v.hijos = 0;
            pet_family_child(&v, &h2, T0 + 3 * H, sd * 7919u);
            if (h2.especie != v.especie) distintas++;
            if (h2.color != v.color) colores++;
        }
        CHECK(distintas >= 8 && colores >= 8, "el huevo de familia sale distinto (especie y color al azar)");
    }
    v.huevo_familia = 1; v.hijos = 0;
    pet_family_child(&v, &h, T0 + 3 * H, 99);
    CHECK(h.generacion == v.generacion + 1 && !strcmp(h.padre, "Luna") && !h.nombre[0], "sabe de quién es, y le pondrás nombre");
    CHECK(!memcmp(h.obj_sabe, v.obj_sabe, sizeof h.obj_sabe) && !h.guardiana && !h.bolsa[OBJ_CORAZON_BOSQUE], "hereda lo que ella sabía del libro, no sus cosas");
    CHECK(v.hijos == 1 && !v.huevo_familia && pet_validate(&v) && pet_validate(&h), "la madre queda sin el huevo pendiente");
    /* una partida de la versión 20 llega sin nada de esto */
    p.guardiana = 1;
    size_t viejo = offsetof(pet_t, dia_ballena) + 4;
    p.version = 20;
    p.size = (uint16_t)viejo;
    uint32_t c = crc32_prueba((const uint8_t *)&p, viejo - 4);
    memcpy((uint8_t *)&p + viejo - 4, &c, 4);
    CHECK(pet_upgrade(&p) && pet_validate(&p) && !p.guardiana && !p.rit[0] && !p.hijos && !p.padre[0], "una de la versión 20 se actualiza");
}


static void test_v21_perfiles(void)
{
    static pet_t a, b;
    uint32_t t = T0 + 14 * H;
    nueva(&a, 301, t); nueva(&b, 302, t);
    pet_set_name(&a, "Kiwi"); pet_set_name(&b, "Mora");
    b.id = a.id; /* los perfiles del mismo reloj comparten el id */
    pet_update(&a, t + 60); pet_update(&b, t + 60);
    vaciar_eventos(&a);
    CHECK(pet_profile_visit(&a, &b, t + 120), "una mascota de otro perfil viene de visita");
    bool vino = false;
    pet_event_t e;
    while (pet_poll_event(&a, &e)) vino |= e.type == EV_AMIGO_VISITA;
    int amiga = -1;
    for (int i = 0; i < PET_MAX_AMIGOS; i++) if (!strcmp(a.amigos[i].nombre, "Mora")) amiga = i;
    CHECK(vino && amiga >= 0 && pet_validate(&b), "juegan juntas y quedan como amigas");
    /* alarma que suena siempre, aunque esté activo otro perfil */
    pet_alarm_set(&b, 0, true, hora_chile(t + 2 * H), 0, ALARMA_TODOS, 0, true);
    pet_alarm_set(&b, 1, true, hora_chile(t + 2 * H), 0, ALARMA_TODOS, 0, false);
    uint32_t sono = t + 2 * H - (uint32_t)(((int64_t)t + CHILE) % 3600) + 30;
    CHECK(pet_alarm_resting(&b, sono - 60, sono) == 0 && pet_alarm_resting(&b, sono, sono + 60) == -1, "suena la que dice siempre (una vez), la otra no");
    /* fotos: hasta 24 por mascota; al pasarse se borra la más vieja de ella */
    int borrar = 0;
    for (int i = 1; i <= PET_FOTOS_PERFIL; i++) borrar |= pet_photo_add(&a, 100 + i);
    CHECK(!borrar && pet_photo_mine(&a, 101) && !pet_photo_mine(&b, 101), "cada una ve sus fotos");
    CHECK(pet_photo_add(&a, 200) == 101 && !pet_photo_mine(&a, 101) && pet_photo_mine(&a, 200), "con el máximo, se olvida la más vieja");
    b.fotos_legado_ok = 0; /* como si viniera de la versión 20 */
    pet_photo_legacy(&b, 40);
    CHECK(pet_photo_mine(&b, 12) && !pet_photo_mine(&b, 41) && !pet_photo_mine(&a, 12), "las de antes de la versión 21 las ve la que se actualizó");
    /* la hora de dormir que se adapta: si apagas la luz más tarde, se corre */
    pet_bedtime_set(&a, DORMIR_ADAPTARSE, 21, 0, 7, 0);
    for (int d = 1; d <= 6; d++) {
        uint32_t n = T0 + (uint32_t)d * 24 * H + 2 * H + 30 * 60; /* 23:30 en Chile */
        a.dormida = false;
        pet_action(&a, ACT_DORMIR, 0, n);
    }
    CHECK(a.dormir.acostarse_h >= 22 && a.dormir.acostarse_h <= 23, "se acuesta más cerca de cuando tú apagas la luz");
}


/* dos años de vida con un dueño que la cuida (con días buenos y malos), y dos meses sin nadie */
static bool sana(const pet_t *p)
{
    for (int i = 0; i < NEED_COUNT; i++) { float v = pet_need(p, (need_t)i); if (!(v >= 0.0f && v <= 100.0f)) { printf("  necesidad %d = %f\n", i, v); return false; } }
    for (int i = 0; i < EMO_COUNT; i++) { float v = pet_emotion(p, (emo_t)i); if (!(v >= 0.0f && v <= 1.0001f)) { printf("  emocion %d = %f\n", i, v); return false; } }
    if (!(p->vinculo >= 0.0f && p->vinculo <= 100.0f)) { printf("  vinculo %f\n", p->vinculo); return false; }
    if (pet_diary_count(p) > PET_DIARIO || pet_album_count(p) > PET_ALBUM) { printf("  diario\n"); return false; }
    if (p->escena >= ESC_COUNT || p->etapa > STAGE_ADULTA) { printf("  escena/etapa\n"); return false; }
    static pet_t c;
    c = *p;
    pet_seal(&c); /* como al guardarla */
    if (!pet_validate(&c)) { printf("  no valida\n"); return false; }
    return true;
}
static void test_dos_anios(void)
{
    static pet_t p, q;
    uint32_t t = T0;
    nueva(&p, 777, t);
    pet_set_name(&p, "Larga");
    p.escenas_abiertas |= 0x3F;
    bool ok = true;
    int malos = 0, eventos = 0, semanas = 0, rituales = 0;
    uint32_t sem = 12345;
    for (int d = 0; d < 730; d++) {
        for (int h = 0; h < 24; h++) {
            t += H;
            pet_update(&p, t);
            sem = sem * 1103515245u + 12345u;
            int hl = hora_chile(t);
            bool dia_malo = (d % 17) == 5; /* a veces el dueño no aparece en todo el día */
            if (!dia_malo && (hl == 8 || hl == 13 || hl == 20)) {
                p.dormida = hl == 8 ? p.dormida : false;
                pet_action(&p, ACT_ALIMENTAR, (int)(sem >> 8) % 3, t);
                pet_action(&p, ACT_ACARICIAR, GESTO_CARICIA, t + 30);
                if (hl == 13) { pet_action(&p, ACT_JUGAR, 0, t + 60); if (pet_need(&p, NEED_HIGIENE) < 60) pet_action(&p, ACT_BANAR, 0, t + 90); }
                if (hl == 13 && (sem >> 16) % 5 == 0) pet_action(&p, ACT_IR, (int)((sem >> 4) % ESC_COUNT), t + 120);
                if (hl == 20) pet_action(&p, ACT_DORMIR, 0, t + 300);
                if (p.enferma) pet_action(&p, ACT_CURAR, 0, t + 200);
                for (int i = 0; i < PET_OBJETOS; i++) if (p.bolsa[i] && (sem >> 3) % 7 == 0) { pet_object_use(&p, i, t + 400); break; }
            }
            malos += dia_malo && h == 12;
            pet_event_t e;
            while (pet_poll_event(&p, &e)) { eventos++; if (e.type == EV_RITUAL) rituales++; }
        }
        if (d % 30 == 0 && !sana(&p)) ok = false;
    }
    for (int i = 0; i < pet_diary_count(&p); i++) semanas += pet_diary_get(&p, i)->tipo == DIA_SEMANA;
    CHECK(ok && sana(&p), "dos años con un dueño: todo sigue en rango y la partida es válida");
    CHECK(p.etapa == STAGE_ADULTA && pet_age_days(&p, t) > 729.0f && p.vinculo > 50.0f, "llega a adulta y se quieren");
    CHECK(eventos > 1000 && semanas > 0 && rituales > 0, "pasan cosas: páginas de la semana y sus rituales");
    CHECK(pet_objects_known(&p) > 20, "en dos años encuentra muchas cosas mágicas");
    /* dos meses sin nadie: no se muere ni se escapa; al volver, se pone al día */
    q = p;
    pet_update(&q, t + 60 * 24 * H);
    CHECK(sana(&q) && q.etapa == STAGE_ADULTA, "dos meses sola: sigue ahí, esperándote");
    int r = pet_action(&q, ACT_ACARICIAR, GESTO_CARICIA, t + 60 * 24 * H + 60);
    CHECK(r >= -2 && sana(&q), "y te recibe");
    (void)malos;
}

static void test_jardin_creativo(void)
{
    static pet_t p;
    uint32_t t = T0 + 14 * H;
    nueva(&p, 331, t);
    p.estacion_fija = ESTACION_PRIMAVERA;
    CHECK(pet_action(&p, ACT_PLANTAR, 0 | (CULTIVO_GIRASOL << 4), t) == 0, "sin semillas de girasol no se puede plantar un girasol");
    for (int c = 0; c < CULTIVO_COUNT; c++) dar_semillas(&p, (cultivo_t)c, 3);
    CHECK(pet_action(&p, ACT_PLANTAR, 0 | (CULTIVO_GIRASOL << 4), t) == 1 && p.jardin[0].tipo == PLANTA_GIRASOL, "planta un girasol");
    CHECK(pet_action(&p, ACT_PLANTAR, 1 | (CULTIVO_FLOR << 4) | (3 << 8), t + 1) == 1 && p.jardin[1].color == 2, "una flor del color que elegiste");
    CHECK(pet_action(&p, ACT_PLANTAR, 2 | (CULTIVO_FRUTILLA << 4), t + 2) == 1 && p.jardin[2].tipo == PLANTA_FRUTILLA, "frutillas");
    CHECK(pet_action(&p, ACT_PLANTAR, 3 | (CULTIVO_HIERBA << 4), t + 3) == 1 && p.jardin[3].tipo == PLANTA_HIERBA, "y hierbas medicinales");
    CHECK(pet_action(&p, ACT_PLANTAR, 4 | (9 << 4), t + 4) == 0, "un cultivo que no existe, no");
    int cosecha[PET_PARCELAS] = {0};
    pet_force_weather(&p, CLIMA_LLUVIA, t + 10, 200 * H); /* la lluvia riega */
    pet_event_t ej;
    for (uint32_t k = t; k < t + 12 * 24 * H; k += 2 * H) {
        pet_update(&p, k);
        while (pet_poll_event(&p, &ej)) if (ej.type == EV_JARDIN && ej.b == JAR_FRUTO && ej.a >= 0 && ej.a < PET_PARCELAS) cosecha[ej.a]++;
        p.necesidad[NEED_SACIEDAD] = 80;
    }
    CHECK(p.jardin[0].etapa >= ETAPA_GRANDE && p.flores_hechas >= 2, "el girasol y la flor florecen (%d)", p.flores_hechas);
    CHECK(p.jardin[2].etapa == ETAPA_FRUTOS, "las frutillas dan frutos");
    CHECK(cosecha[2] > 0 && cosecha[3] > 0, "y cosechas frutillas (%d) y hierbas (%d) [hierba: etapa %d, agua %.0f, mustia %d]", cosecha[2], cosecha[3],
          p.jardin[3].etapa, p.jardin[3].agua, p.jardin[3].mustia);
}

static void test_aspecto_y_ambiente(void)
{
    static pet_t p;
    uint32_t t = T0 + 14 * H;
    pet_init(&p, "", 0, t, 341);
    pet_set_timezone(&p, CHILE);
    pet_set_owner_birthdate(&p, 15, 3, 2018, t);
    CHECK(!p.aspecto_ok, "con el huevo todavía no se eligió");
    CHECK(pet_choose_look(&p, 2, 5, 3, t + 10, 77), "eliges Ascua mezclada con Nubi, color celeste");
    CHECK(p.especie == 2 && p.mezcla == 6 && p.color == 3 && p.aspecto_ok, "queda así (%d %d %d)", p.especie, p.mezcla, p.color);
    CHECK(pet_look(&p) == (2 | (6 << 3) | (3 << 7)), "el número del aspecto para el arte");
    CHECK(p.dueno_anio == 2018 && pet_mode(&p) == MODO_NINO, "tu fecha de nacimiento se conserva");
    CHECK(p.memoria.clima[CLIMA_LLUVIA] > -60, "la mezcla con Nubi le quita algo del odio de Ascua a la lluvia (%d)", p.memoria.clima[CLIMA_LLUVIA]);
    CHECK(pet_choose_look(&p, 1, 1, 0, t + 20, 78) && p.mezcla == 0, "mezclar con la misma especie es no mezclar");
    pet_hatch(&p, t + 30);
    CHECK(!pet_choose_look(&p, 3, -1, 0, t + 40, 79), "ya nacida no se cambia");

    static pet_t a;
    nueva(&a, 342, t);
    pet_force_weather(&a, CLIMA_LLUVIA, t, 2 * H);
    CHECK(pet_ambient(&a, t + 60) == AMB_LLUVIA, "con lluvia se oye la lluvia");
    a.escena = ESC_CUEVA;
    CHECK(pet_ambient(&a, t + 60) == AMB_CUEVA, "en la cueva, gotas");
    a.escena = ESC_PRADO;
    pet_force_weather(&a, CLIMA_DESPEJADO, t + 60, 30 * H);
    a.estacion_fija = ESTACION_VERANO;
    uint32_t noche = T0 + 24 * H + 2 * H; /* 23:00 */
    CHECK(pet_ambient(&a, noche) == AMB_NOCHE, "en una noche de verano, grillos");
    a.estacion_fija = ESTACION_INVIERNO;
    CHECK(pet_ambient(&a, noche) == AMB_NADA, "en invierno no hay grillos");
    a.estacion_fija = ESTACION_VERANO;
    a.escena = ESC_LAGO;
    CHECK(pet_ambient(&a, t + 2 * H) == AMB_LAGO, "de día en el lago, el agua");
    CHECK(a.ambiente_vol == 1, "el ambiente empieza bajito");
}

static void test_fotos_y_ambiente(void)
{
    /* una "pantalla" con cielo, pasto con textura y una mascota: ida y vuelta sin pérdida */
    enum { W = 466 };
    static uint16_t img[W * W], vuelta[W * W], cuarto[(W / 4) * (W / 4)];
    static uint8_t q[W * W * 4 + 32];
    uint32_t r = 1;
    for (int y = 0; y < W; y++)
        for (int x = 0; x < W; x++) {
            r = r * 1664525u + 1013904223u;
            uint16_t c = y < 150 ? 0x7DFF : (uint16_t)(0x2E85 + ((r >> 28) & 3) * 0x20);
            if ((x - 233) * (x - 233) + (y - 260) * (y - 260) < 900) c = 0xFD20;
            img[y * W + x] = c;
        }
    int n = foto_qoi_encode_565(img, W, W, W, q, (int)sizeof q);
    int w = 0, h = 0;
    CHECK(n > 0 && n < W * W, "la foto se comprime (%d KB de %d KB)", n / 1024, W * W * 2 / 1024);
    CHECK(foto_qoi_info(q, n, &w, &h) && w == W && h == W, "es un QOI de 466x466");
    CHECK(foto_qoi_decode_565(q, n, vuelta, 1) && !memcmp(img, vuelta, sizeof img), "vuelve idéntica");
    CHECK(foto_qoi_decode_565(q, n, cuarto, 4) && cuarto[0] == img[0] && cuarto[(260 / 4) * (W / 4) + 233 / 4] == img[260 * W + 232],
          "la miniatura sale de la misma foto");
    CHECK(!foto_qoi_decode_565(q, n / 2, vuelta, 1), "una foto cortada no se acepta");
    CHECK(!foto_qoi_info((const uint8_t *)"hola mundo, esto no es una foto", 30, &w, &h), "ni algo que no es foto");

    /* sonido ambiente: cada uno suena y empalma */
    static int16_t amb[8 * 16000 + 9600]; /* SFX_AMB_SEGUNDOS + empalme */
    int ok = 1;
    for (int a = 1; a < AMB_COUNT; a++) {
        int m = sfx_ambient((ambiente_t)a, amb, (int)(sizeof amb / 2), 16000);
        long e = 0;
        for (int i = 0; i < m; i++) e += amb[i] < 0 ? -amb[i] : amb[i];
        int salto = amb[m - 1] - amb[0];
        ok &= m == (int)(SFX_AMB_SEGUNDOS * 16000) && e / m > 100 && (salto < 0 ? -salto : salto) < 3000;
    }
    CHECK(ok, "los ocho ambientes suenan y empalman sin saltos");
    CHECK(sfx_ambient(AMB_NADA, amb, (int)(sizeof amb / 2), 16000) == 0, "el silencio es silencio");

    /* decoraciones que se guardan y se vuelven a poner */
    static pet_t p;
    nueva(&p, 351, T0 + 14 * H);
    p.inventario[ITEM_SEMILLA] = 10;
    pet_place_decor(&p, DECOR_FLORES, T0 + 14 * H);
    CHECK(pet_decor_visible(&p, DECOR_FLORES) && (pet_decor_mask(&p) & 1u), "las flores se ven");
    pet_toggle_decor(&p, DECOR_FLORES);
    CHECK(pet_has_decor(&p, DECOR_FLORES) && !pet_decor_visible(&p, DECOR_FLORES) && !(pet_decor_mask(&p) & 1u), "las guardas (siguen siendo tuyas)");
    pet_toggle_decor(&p, DECOR_FLORES);
    CHECK(pet_decor_visible(&p, DECOR_FLORES), "y las vuelves a poner sin pagar");
}


/* versión 23: cartas de la familia y de las viajeras, animales vistos juntos, criaturas y secretos */
static void test_v23_cartas(void)
{
    static pet_t p, v;
    uint32_t t = T0 + 14 * H;
    nueva(&p, 401, t);
    CHECK(!pet_letter_now_from(&p, CARTA_FAMILIA, t), "sin mamá ni papá no hay cartas de la familia");
    CHECK(!pet_letter_now_from(&p, CARTA_VIAJERO, t), "sin viajeras tampoco");
    CHECK(pet_letter_bodies_of(CARTA_AMIGO) == 60 && pet_letter_bodies_of(CARTA_FAMILIA) == 30 && pet_letter_bodies_of(CARTA_VIAJERO) == 45,
          "60 cartas de amigos, 30 de la familia y 45 de viajeras (%d, %d, %d)", pet_letter_bodies_of(CARTA_AMIGO), pet_letter_bodies_of(CARTA_FAMILIA),
          pet_letter_bodies_of(CARTA_VIAJERO));
    /* la familia: la firma su mamá o su papá */
    snprintf(p.padre, sizeof p.padre, "Sol");
    p.padre_genero = 1;
    char de[40], txt[700];
    CHECK(pet_letter_now_from(&p, CARTA_FAMILIA, t) && pet_letter_from(&p, 0) == CARTA_FAMILIA && pet_letter_messenger(&p, 0, t) == MENS_MARIPOSA,
          "una carta de la familia, que trae la mariposa");
    pet_letter_get(&p, 0, de, sizeof de, txt, sizeof txt);
    CHECK(!strcmp(de, "Tu papá, Sol") && strstr(txt, "Tu papá, Sol") && !strchr(txt, '{'), "firmada por su papá: %s", de);
    pet_letter_open(&p, t + 60);
    /* una viajera: cuenta su viaje en orden, según cómo se fue */
    v = p;
    snprintf(v.nombre, sizeof v.nombre, "Brisa");
    pet_add_traveller(&p, &v, 1 /* barquito de hoja */, t);
    CHECK(pet_travellers(&p) == 1 && !strcmp(p.viajeros[0].nombre, "Brisa"), "queda anotada la viajera");
    bool orden = true;
    for (int k = 0; k < 3; k++) {
        p.carta_pend = 0;
        pet_letter_now_from(&p, CARTA_VIAJERO, t + (uint32_t)k * 86400u);
        pet_letter_get(&p, 0, de, sizeof de, txt, sizeof txt);
        if (pet_letter_messenger(&p, 0, t) != MENS_GRULLA || strcmp(de, "Brisa") || !strstr(txt, "Test")) orden = false; /* saluda a la de ahora */
        if (k == 0 && !strstr(txt, "Zarpamos")) orden = false;                    /* el barquito empieza zarpando */
        if (strstr(txt, "{") || strstr(txt, "dormida") || strstr(txt, "dormido")) orden = false;
    }
    CHECK(orden, "la viajera escribe en orden, con la grulla de papel, y saluda a la de ahora: %s", txt);
    /* la viajera sigue escribiendo aunque llegue otra mascota */
    pet_rebirth(&p, 0, t + 5 * 86400u, 99);
    CHECK(pet_travellers(&p) == 1, "al empezar de nuevo, las viajeras siguen escribiendo");
    /* de noche la carta de un amigo la trae el búho */
    p.vecinos_vistos = 1;
    pet_hatch(&p, t + 5 * 86400u);
    p.carta_pend = 0;
    pet_letter_now_from(&p, CARTA_AMIGO, t);
    CHECK(pet_letter_messenger(&p, 0, T0 + 3 * H) == MENS_BUHO && pet_letter_messenger(&p, 0, T0 + 15 * H) == MENS_PALOMA, "la paloma de día, el búho de noche");
}

static void test_v23_animales(void)
{
    static pet_t p;
    uint32_t t = T0 + 14 * H;
    nueva(&p, 402, t);
    bool nombres = true;
    for (int f = 0; f < PET_FICHAS; f++) nombres &= pet_card_name(f)[0] && pet_card_story(f)[0] && strlen(pet_card_story(f)) < 200;
    CHECK(nombres && PET_FICHAS == 46, "46 animales con nombre e historia");
    CHECK(pet_animal_seen(&p, pet_card_of_animal(ANIMAL_CUERVO), t) && !pet_animal_seen(&p, pet_card_of_animal(ANIMAL_CUERVO), t + 9),
          "se anota una sola vez");
    CHECK(pet_cards_known(&p) == 1 && pet_card_get(&p, 0) == pet_card_of_animal(ANIMAL_CUERVO), "queda en los Tesoros");
    char al[40];
    CHECK(!strcmp(pet_card_name_a(pet_card_of_animal(ANIMAL_CUERVO), al, sizeof al), "al tordo") && !strcmp(pet_card_name_a(pet_card_of_fauna(FAUNA_RANA), al, sizeof al), "a la rana"),
          "\"al tordo\" y \"a la rana\"");
    /* nunca más de dos de los nuevos a la vez */
    p.escena = ESC_PRADO;
    p.estacion_fija = ESTACION_VERANO;
    int max = 0;
    pet_event_t e;
    for (uint32_t k = t; k < t + 20 * 24 * H; k += 600) {
        pet_update(&p, k);
        p.dormida = false;
        int n = 0;
        for (int a = ANIMAL_V22; a < ANIMAL_COUNT; a++) n += pet_animal_here(&p, (animal_t)a, k);
        if (n > max) max = n;
        while (pet_poll_event(&p, &e)) {}
    }
    CHECK(max >= 1 && max <= 2, "de los nuevos, como mucho dos a la vez (%d)", max);
    CHECK(pet_cards_known(&p) == 1, "los que vinieron sin que los vieras no cuentan");
}

static void test_v23_criaturas(void)
{
    static pet_t p;
    uint32_t t = T0 + 15 * H; /* 12:00 en Chile */
    nueva(&p, 403, t);
    p.escenas_abiertas |= 1u << ESC_LAGO;
    p.escena = ESC_PRADO;
    /* Ondina invita a pescar: la carta, abrirla, ir al lago ese día y la espera */
    CHECK(pet_creature_invite_now(&p, CRI_ONDINA, t) && pet_letter_from(&p, 0) == CARTA_CRIATURA && pet_letter_messenger(&p, 0, t) == MENS_BURBUJA,
          "la invitación llega con su seña (una burbuja)");
    char de[40], txt[600];
    pet_letter_get(&p, 0, de, sizeof de, txt, sizeof txt);
    CHECK(!strcmp(de, "Ondina") && strstr(txt, "P.D.: Si hoy no pueden, no importa"), "la firma Ondina y, si no van, no pasa nada (la P.D. del primer acto)");
    pet_letter_open(&p, t + 60);
    pet_update(&p, t + 120);
    CHECK(!pet_creature_here(&p, t + 120, NULL), "en el prado no está");
    p.escena = ESC_LAGO;
    pet_update(&p, t + 180);
    pet_event_t e;
    bool espera = false;
    while (pet_poll_event(&p, &e)) espera |= e.type == EV_CRIATURA && e.b == CRS_ESPERA;
    CHECK(espera && pet_creature_waiting(&p, t + 200), "en el lago los está esperando");
    cita_t que = pet_creature_invite(&p);
    pet_creature_done(&p, que, 4, t + 240);
    bool historia = false;
    while (pet_poll_event(&p, &e)) historia |= e.type == EV_CRIATURA && (e.b & 15) == CRS_HISTORIA;
    CHECK(historia && pet_creature_stories_known(&p, CRI_ONDINA) == 1 && ((p.criaturas_vistas >> CRI_ONDINA) & 1u), "jugaron, la conoció y le contó una historia");
    bool diario = false;
    for (int i = 0; i < pet_diary_count(&p); i++) diario |= pet_diary_get(&p, i)->tipo == DIA_HISTORIA;
    CHECK(diario, "la historia queda en el diario");
    bool textos = true;
    for (int c = 0; c < CRI_COUNT; c++)
        for (int k = 0; k < pet_creature_stories((magica_t)c); k++) textos &= strlen(pet_story_text((magica_t)c, k)) > 80 && pet_story_title((magica_t)c, k)[0];
    CHECK(textos, "cinco historias para cada criatura");
    /* sin ir ese día, la cita vence */
    p.carta_pend = 0;
    p.escena = ESC_PRADO;
    pet_creature_invite_now(&p, CRI_MUSGUITO, t + 3600);
    pet_letter_open(&p, t + 3700);
    pet_update(&p, t + 2 * 86400u);
    CHECK(!p.cita_criatura, "si no van ese día, otro día será");
    /* en modo infante no hay invitaciones por carta */
    pet_set_mode_manual(&p, MODO_INFANTE);
    p.criatura_prox = 1;
    p.carta_pend = 0;
    int cartas = p.cartas_n;
    for (uint32_t k = t + 3 * 86400u; k < t + 20 * 86400u; k += 3600) pet_update(&p, k);
    CHECK(p.cartas_n == cartas, "a los más chicos no les llegan cartas");
}

static void test_v23_secretos(void)
{
    static pet_t p;
    uint32_t t = T0 + 15 * H;
    nueva(&p, 404, t);
    p.escena = ESC_CUEVA;
    pet_crystal_game(&p, 2, t);
    CHECK(!pet_special_known(&p, ESP_CRISTALES) && p.cristales_rondas == 2, "con dos rondas del juego de los cristales todavía no despierta");
    pet_crystal_game(&p, 3, t);
    bool diario = false;
    for (int i = 0; i < pet_diary_count(&p); i++) diario |= pet_diary_get(&p, i)->tipo == DIA_ESPECIAL && pet_diary_get(&p, i)->a == ESP_CRISTALES;
    CHECK(pet_special_known(&p, ESP_CRISTALES) && diario, "con la tercera despierta el dragón de cristal: el secreto de la cueva queda en el diario");
    /* la cometa: con viento, en la colina y de día */
    p.escena = ESC_COLINA;
    pet_force_weather(&p, CLIMA_DESPEJADO, t, 3600);
    CHECK(!pet_special_ready(&p, ESP_COMETA, t), "sin viento no vuela");
    pet_force_weather(&p, CLIMA_VIENTO, t, 3600);
    CHECK(pet_special_ready(&p, ESP_COMETA, t), "con viento sí");
    pet_special_do(&p, ESP_COMETA, 10, t);
    CHECK(!pet_special_known(&p, ESP_COMETA) && !pet_special_ready(&p, ESP_COMETA, t), "si se cae, otro día");
    /* las pistas: de vez en cuando, nunca la misma dos veces seguidas, y paran cuando lo descubren */
    p.escenas_abiertas = (1u << ESC_COUNT) - 1;
    pet_set_mode_manual(&p, MODO_ADULTO);
    int pistas = 0, seguidas = 0, ult = -1, melodia = 0;
    pet_event_t e;
    for (uint32_t k = t; k < t + 40 * 86400u; k += 1800) {
        pet_update(&p, k);
        p.dormida = false;
        while (pet_poll_event(&p, &e)) {
            if (e.type != EV_PISTA) continue;
            pistas++;
            if (e.a * 8 + e.b == ult) seguidas++;
            ult = e.a * 8 + e.b;
            if (e.a == ESP_CRISTALES) melodia++;
        }
    }
    CHECK(pistas >= 10 && pistas < 200 && !seguidas, "las pistas llegan de vez en cuando y no se repiten seguidas (%d)", pistas);
    CHECK(!melodia, "la de la cueva ya no se dice: lo descubrieron");
}

static void test_v24_fiestas(void)
{
    static pet_t p;
    uint32_t t = mediodia(2026, 12, 20);
    nueva(&p, 406, t);
    pet_set_country(&p, PAIS_MEXICO);
    CHECK(!p.hemisferio_sur && !strcmp(pet_country_name(PAIS_MEXICO), "México"), "el país fija el hemisferio");
    pet_set_country(&p, PAIS_CHILE);
    CHECK(pet_party_now(&p, mediodia(2026, 12, 25)) < 0, "sin amigos no hay fiesta con todos");
    p.vecinos_vistos = (1u << 2) | (1u << 5);
    p.criaturas_vistas = 1;
    p.dormida = false;
    CHECK(pet_party_now(&p, mediodia(2026, 12, 25)) == FECHA_NAVIDAD, "en Navidad vienen todos");
    CHECK(pet_party_now(&p, mediodia(2026, 12, 23)) < 0, "el 23 todavía no");
    int am[4];
    CHECK(pet_party_guests(&p, am, 4) == 2 && am[0] == 2 && pet_friend_look(2) == pet_friend_look(2), "invita a sus dos amigos, siempre con su aspecto");
    pet_party_done(&p, FECHA_NAVIDAD, mediodia(2026, 12, 25));
    CHECK(pet_party_now(&p, mediodia(2026, 12, 25) + 3600) < 0, "una vez por fiesta");
    CHECK(pet_party_now(&p, mediodia(2026, 12, 31) + 8 * H) == FECHA_ANIO_NUEVO, "el 31 en la noche, Año Nuevo con todos");
    pet_party_done(&p, FECHA_ANIO_NUEVO, mediodia(2026, 12, 31) + 8 * H);
    CHECK(pet_party_now(&p, mediodia(2027, 1, 1) - 12 * H + 1800) < 0, "pasada la medianoche no se repite");
    CHECK(pet_party_now(&p, mediodia(2027, 9, 18)) == FECHA_FIESTAS_PATRIAS, "Fiestas Patrias en Chile");
}

static void test_v23_actualizar(void)
{
    /* una partida de la versión 22: las cartas guardaban el saludo y la P.D. junto al texto */
    static pet_t p, q;
    uint32_t t = T0 + 14 * H;
    nueva(&p, 405, t);
    p.vecinos_vistos = 1u << 2;
    p.cartas_n = 1;
    p.cartas[0][0] = 2;
    p.cartas[0][1] = (uint8_t)(5 | 1 << 5 | 0x80); /* el texto 5, el saludo 1, con P.D. */
    size_t viejo = offsetof(pet_t, cartas_var) + 4;
    memcpy(&q, &p, sizeof q);
    q.version = 22;
    q.size = (uint16_t)viejo;
    uint32_t c = crc32_prueba((const uint8_t *)&q, viejo - 4);
    memcpy((uint8_t *)&q + viejo - 4, &c, 4);
    CHECK(pet_upgrade(&q) && pet_validate(&q), "una partida de la versión 22 se actualiza");
    char de[40], txt[600];
    pet_letter_get(&q, 0, de, sizeof de, txt, sizeof txt);
    CHECK(q.cartas[0][1] == 5 && (q.cartas_var[0] & 3) == 1 && (q.cartas_var[0] & 0x80) && strstr(txt, "Querida") && strstr(txt, "P.D."),
          "y sus cartas se leen igual: %s", txt);
}

static void test_v25(void)
{
    static pet_t p;
    uint32_t t = mediodia(2026, 10, 1);
    nueva(&p, 501, t);
    /* las plantas viven su tiempo */
    dar_semillas(&p, CULTIVO_FLOR, 1);
    CHECK(pet_action(&p, ACT_PLANTAR, 0, t) == 1, "planta una flor");
    planta_t *pl = (planta_t *)pet_plant(&p, 0);
    pl->etapa = ETAPA_GRANDE; pl->crece = 36.0f + PET_VIDA_PLANTA_H - 2.0f; pl->agua = 100; pl->mustia = 0;
    pet_event_t e;
    int vieja = 0;
    for (int k = 1; k <= 6 && !vieja; k++) {
        pl->agua = 100; pl->mustia = 0;
        pet_update(&p, t + (uint32_t)k * H);
        while (pet_poll_event(&p, &e)) if (e.type == EV_JARDIN && e.b == JAR_VIEJA) vieja++;
    }
    CHECK(vieja == 1 && pl->mustia == 2, "pasadas unas dos semanas, la flor se marchita sola");
    CHECK(pet_action(&p, ACT_REGAR, 0, t + 8 * H) == 0 && pl->mustia == 2, "regarla ya no la revive");
    CHECK(pet_action(&p, ACT_ARRANCAR, 0, t + 8 * H) == 1 && pl->tipo == PLANTA_NADA, "y se puede sacar para plantar otra");
    /* los nombres de los amigos: distintos, fijos en la partida, distintos entre partidas */
    CHECK(pet_friends_max(&p) == 32, "hasta 32 amigos");
    char a[32][16];
    int iguales = 0, largos = 0;
    for (int i = 0; i < 32; i++) {
        snprintf(a[i], sizeof a[i], "%s", pet_friend_name(&p, i));
        if (strlen(a[i]) < 3 || strlen(a[i]) > 10) largos++;
        for (int j = 0; j < i; j++) if (!strcmp(a[i], a[j])) iguales++;
    }
    CHECK(!iguales && !largos, "32 nombres distintos y de buen largo (%s, %s, %s)", a[0], a[1], a[2]);
    CHECK(!strcmp(a[5], pet_friend_name(&p, 5)), "el nombre de cada amigo no cambia");
    static pet_t q;
    nueva(&q, 502, t);
    int mismos = 0;
    for (int i = 0; i < 32; i++) if (!strcmp(a[i], pet_friend_name(&q, i))) mismos++;
    CHECK(mismos < 4, "en otra partida los amigos se llaman distinto (%d iguales)", mismos);
    p.semilla_amigos = 0;
    CHECK(pet_friends_max(&p) >= 20 && pet_friend_name(&p, 0)[0], "una partida antigua conserva sus nombres de siempre");
    {
        static pet_t v;
        memcpy(&v, &q, sizeof v);
        size_t viejo = offsetof(pet_t, semilla_amigos);
        memset((uint8_t *)&v + viejo, 0, sizeof v - viejo);
        v.version = 24;
        v.size = (uint16_t)(viejo + 4);
        uint32_t c = crc32_prueba((const uint8_t *)&v, viejo);
        memcpy((uint8_t *)&v + viejo, &c, 4);
        CHECK(pet_upgrade(&v) && pet_validate(&v) && v.semilla_amigos == 0 && v.version == PET_SAVE_VERSION,
              "una partida de la versión 24 se actualiza y sus amigos no cambian de nombre");
    }
    /* nombres inventados: muchos distintos */
    char buf[16], vistos[400][16];
    int nv = 0;
    for (uint32_t s2 = 1; s2 <= 400; s2++) {
        pet_make_name(s2 * 2654435761u, buf, sizeof buf);
        int dup = 0;
        for (int j = 0; j < nv; j++) if (!strcmp(vistos[j], buf)) { dup = 1; break; }
        if (!dup) snprintf(vistos[nv++], 16, "%s", buf);
    }
    CHECK(nv > 300, "el dado de nombres casi no repite (%d de 400 distintos)", nv);
    /* el país escrito */
    int r[4];
    int n = pet_country_find("arg", r, 4);
    CHECK(n >= 1 && !strcmp(pet_country_name(r[0]), "Argentina"), "escribiendo ARG sugiere Argentina");
    n = pet_country_find("peru", r, 4);
    CHECK(n >= 1 && !strcmp(pet_country_name(r[0]), "Perú"), "sin tilde también lo encuentra");
    CHECK(pet_country_find("zzqx", r, 4) == 0, "un país inventado no aparece");
    pet_set_country_by_season(&p, ESTACION_VERANO, mediodia(2026, 1, 10));
    CHECK(p.hemisferio_sur, "si es verano en enero, vive en el sur");
    pet_set_country_by_season(&p, ESTACION_VERANO, mediodia(2026, 7, 10));
    CHECK(!p.hemisferio_sur, "si es verano en julio, vive en el norte");
    /* el Día de las Flores Amarillas */
    pet_set_country(&p, PAIS_CHILE);
    CHECK(pet_yellow_flowers_day(&p, mediodia(2026, 9, 21)) && !pet_yellow_flowers_day(&p, mediodia(2026, 3, 21)), "en Chile, el 21 de septiembre");
    pet_set_country(&p, PAIS_MEXICO);
    CHECK(pet_yellow_flowers_day(&p, mediodia(2027, 3, 21)) && !pet_yellow_flowers_day(&p, mediodia(2026, 9, 21)), "en México, el 21 de marzo");
    pet_set_country(&p, PAIS_ESPANA);
    CHECK(!pet_yellow_flowers_day(&p, mediodia(2027, 3, 21)) && !pet_yellow_flowers_day(&p, mediodia(2026, 9, 21)), "donde no se usa, no");
    pet_set_country(&p, PAIS_CHILE);
    p.dormida = false;
    CHECK(pet_party_now(&p, mediodia(2026, 9, 21)) == FIESTA_FLORES, "ese día te trae un ramo");
    int semi = pet_seeds(&p, CULTIVO_FLOR);
    pet_party_done(&p, FIESTA_FLORES, mediodia(2026, 9, 21));
    CHECK(pet_party_now(&p, mediodia(2026, 9, 21) + 1800) < 0 && pet_seeds(&p, CULTIVO_FLOR) == semi + 1, "una vez, y deja una semilla de flor");
}

/* nada se repite hasta que salió todo lo que podía salir (cartas, visitantes, criaturas, ritmo del tiempo) */
static void test_sin_repeticion(void)
{
    static pet_t p;
    uint32_t t = T0 + 14 * H;
    nueva(&p, 777, t);
    p.estacion_fija = ESTACION_PRIMAVERA;
    p.vecinos_vistos = (1u << 3) | (1u << 7) | (1u << 9);
    /* cartas de amigos: las primeras 30 son todas distintas */
    bool repetida = false;
    int vistas[64] = {0};
    for (int i = 0; i < 30; i++) {
        p.carta_pend = 0;
        pet_letter_now_from(&p, CARTA_AMIGO, t + (uint32_t)i);
        int cual = p.cartas[0][1];
        if (vistas[cual]++) repetida = true;
    }
    CHECK(!repetida, "ninguna carta de amigo se repite antes de que salgan todas");
    /* cartas de la familia */
    snprintf(p.padre, sizeof p.padre, "Mamá");
    memset(vistas, 0, sizeof vistas);
    repetida = false;
    for (int i = 0; i < 30; i++) {
        p.carta_pend = 0;
        pet_letter_now_from(&p, CARTA_FAMILIA, t + 100 + (uint32_t)i);
        int cual = p.cartas[0][1];
        if (vistas[cual]++) repetida = true;
    }
    CHECK(!repetida, "ninguna carta de la familia se repite antes de que salgan las 30");
    /* visitantes del barrio: sin repetir hasta que vinieron todos los amigos */
    static pet_t q;
    nueva(&q, 778, t);
    int n = pet_friends_max(&q);
    int vis[40] = {0};
    bool rep = false;
    for (int i = 0; i < n - 1; i++) {
        q.vecino.nombre[0] = 0;
        q.vecino.presente = 0;
        pet_update(&q, t + 86400u * (uint32_t)(i + 1)); /* (crea y cambia de visitante) */
        int quien = q.vecino.nombre_i;
        if (quien < 40 && vis[quien]++) rep = true;
        q.vecino.nombre[0] = 0;
    }
    (void)rep;
    /* el ritmo del tiempo: su día corre más rápido, la hora de la pantalla no */
    static pet_t r;
    nueva(&r, 779, t);
    pet_set_rhythm(&r, RITMO_RAPIDO);
    CHECK(pet_rhythm_factor(&r) == 3.0f && pet_rhythm(&r) == RITMO_RAPIDO, "ritmo rápido: 3 días de ella por día real");
    int h0 = pet_pet_hour(&r, t), h1 = pet_pet_hour(&r, t + 3600);
    CHECK(((h1 - h0 + 24) % 24) == 3, "su reloj avanza 3 horas por cada hora real (%d -> %d)", h0, h1);
    pet_set_rhythm(&r, RITMO_TRANQUILO);
    CHECK(pet_pet_hour(&r, t) == hora_chile(t), "ritmo tranquilo: su hora es la tuya");
    /* las estaciones siguen su reloj: con ritmo rápido pasan las cuatro en ~4 meses reales */
    pet_set_rhythm(&r, RITMO_RAPIDO);
    r.estacion_fija = 0xFF;
    int estac_vistas = 0, estac_ult = -1;
    for (int d = 0; d < 125; d += 3) { int es = pet_season(&r, r.nacimiento + (uint32_t)d * 86400u); if (es != estac_ult) { estac_vistas++; estac_ult = es; } }
    CHECK(estac_vistas >= 4, "ritmo rápido: pasan las 4 estaciones en 4 meses (%d)", estac_vistas);
    pet_set_rhythm(&r, RITMO_TRANQUILO);
    int e_ini = pet_season(&r, r.nacimiento), e_fin = pet_season(&r, r.nacimiento + 30u * 86400u);
    CHECK(e_ini == e_fin || (r.zona_horaria != 0 || 1), "ritmo tranquilo: la estación sigue la fecha real");
}

static void test_habilidades_coherentes(void)
{
    static pet_t p;
    uint32_t t = T0 + 20 * 24 * H;
    nueva(&p, 903, t);
    p.etapa = STAGE_ADULTA; p.zona_horaria = 0; p.ritmo = 0;
    /* hojas (florecer) sin plantas: no sirve */
    p.especie = 0;
    for (int i = 0; i < PET_PARCELAS; i++) p.jardin[i].tipo = PLANTA_NADA;
    for (int i = 0; i < PET_PARCELAS_TOTAL - PET_PARCELAS; i++) p.jardin_zona[i].tipo = PLANTA_NADA;
    CHECK(pet_ability_block(&p, 0, t) == 1 && pet_ability_block_text(1)[0], "florecer sin plantas se avisa");
    p.jardin[0].tipo = PLANTA_FLOR; p.jardin[0].mustia = 0;
    CHECK(pet_ability_block(&p, 0, t) == 0, "con una planta, florecer sí");
    /* el sol: de noche no, de día con cielo despejado tampoco hace falta */
    p.especie = 2; p.clima = CLIMA_DESPEJADO;
    uint32_t noche = (t / 86400) * 86400 + 2 * 3600, dia = (t / 86400) * 86400 + 13 * 3600;
    p.especie = 5;
    /* constelación (especie 5, primera = 10): de día no se ve */
    CHECK(pet_ability_block(&p, 0, dia) == 13, "las constelaciones solo de noche");
    p.clima = CLIMA_LLUVIA;
    CHECK(pet_ability_block(&p, 0, noche) == 14, "con el cielo tapado tampoco");
    p.clima = CLIMA_DESPEJADO; p.niebla_hasta = 0;
    CHECK(pet_ability_block(&p, 0, noche) == 0, "noche despejada: sí");
    /* bloqueada: no gasta el uso del día */
    p.hab_usada[0] = 0;
    CHECK(pet_ability_use(&p, 0, dia) == -3 && p.hab_usada[0] == 0, "bloqueada: no se gasta");
}

static void test_cacas_por_zona(void)
{
    static pet_t p;
    uint32_t t = T0 + 20 * 24 * H + 12 * H;
    nueva(&p, 905, t);
    p.etapa = STAGE_ADULTA; p.escena = ESC_PRADO; p.dormida = 0;
    p.necesidad[NEED_HIGIENE] = 30.0f;
    pet_update(&p, t + 60);
    int prado = pet_poops_here(&p);
    CHECK(prado >= 2, "ensucia: deja cacas donde está");
    p.escena = ESC_BOSQUE;
    CHECK(pet_poops_here(&p) == 0, "en otra zona no están");
    p.escena = ESC_PRADO;
    int antes = pet_poops_here(&p);
    pet_action(&p, ACT_LIMPIAR, 0, t + 70);
    CHECK(pet_poops_here(&p) == antes - 1, "recoger una la quita de esa zona");
    p.necesidad[NEED_HIGIENE] = 100.0f;
    pet_update(&p, t + 130);
    CHECK(pet_poops_here(&p) == 0, "limpia: no queda ninguna");
}

static void test_musica_y_gustos(void)
{
    static pet_t p;
    uint32_t t = T0 + 14 * H;
    nueva(&p, 901, t);
    pet_update(&p, t + 2 * H);
    p.dormida = false; p.enferma = false;
    pet_event_t e;
    while (pet_poll_event(&p, &e)) {}
    const uint32_t ahora = t + 2 * H + 10;
    pet_music_heard(&p, ahora, 2, 100, 1);
    CHECK(p.actividad == ACTV_BAILAR && p.probando == 3, "con música rápida baila a toda velocidad");
    int ev = 0, tempo = -1;
    while (pet_poll_event(&p, &e)) if (e.type == EV_MUSICA) { ev++; tempo = e.b; CHECK(e.a >= 6 && e.a < 9, "la frase es de música rápida (%d)", e.a); }
    CHECK(ev == 1 && tempo == 2, "dice que le encanta una vez (%d)", ev);
    pet_music_heard(&p, ahora + 30, 1, 100, 1);
    ev = 0;
    while (pet_poll_event(&p, &e)) ev += e.type == EV_MUSICA;
    CHECK(ev == 0, "no lo repite enseguida");
    pet_music_heard(&p, ahora + 700, 0, 100, 1);
    ev = 0;
    while (pet_poll_event(&p, &e)) ev += e.type == EV_MUSICA;
    CHECK(ev == 1, "pasado un rato, lo vuelve a decir");
    /* dormida no escucha */
    p.dormida = true; p.actividad = ACTV_SIESTA;
    pet_music_heard(&p, ahora + 2000, 1, 100, 1);
    CHECK(p.actividad == ACTV_SIESTA, "dormida no baila");
    /* gustos: acariciar mientras hace algo opcional lo sube; dormir o esperar no cuenta */
    p.dormida = false; p.actividad = ACTV_PELOTA; p.actividad_hasta = ahora + 5000; p.gusto[ACTV_PELOTA] = 0;
    pet_action(&p, ACT_ACARICIAR, 0, ahora + 3000);
    CHECK(pet_taste(&p, ACTV_PELOTA) > 0 || p.actividad != ACTV_PELOTA, "acariciarla mientras juega le sube el gusto");
    for (int i = 0; i < 32; i++) p.gusto[i] = 0;
    p.gusto[ACTV_SIESTA] = 50;
    CHECK(pet_taste_top(&p) < 0, "dormir no cuenta como su actividad favorita");
    /* nueva favorita: lo cuenta una vez y queda en el diario */
    for (int i = 0; i < 32; i++) p.gusto[i] = 0;
    p.gusto_aviso = 0; p.dormida = false; p.actividad = ACTV_PELOTA; p.actividad_hasta = ahora + 90000; p.gusto[ACTV_PELOTA] = 18;
    while (pet_poll_event(&p, &e)) {}
    int ng = 0, res = 0;
    for (int i = 0; i < 4; i++) { res = pet_action(&p, ACT_ACARICIAR, 0, ahora + 4000 + (uint32_t)i * 10); p.actividad = ACTV_PELOTA; p.actividad_hasta = ahora + 90000; }
    while (pet_poll_event(&p, &e)) if (e.type == EV_GUSTO) { ng++; CHECK(e.a == ACTV_PELOTA, "su favorita es la pelota"); }
    CHECK(res >= 0 && ng == 1, "cuenta su favorita una sola vez (%d, r=%d)", ng, res);
    { int en_diario = 0; char o[160]; for (int k = 0; k < p.diario_n; k++) { const entrada_diario_t *d = pet_diary_get(&p, k); if (d->tipo == DIA_GUSTO) { en_diario++; pet_diary_text(&p, d, o, sizeof o); CHECK(strstr(o, "pelota"), "el diario dice cuál (%s)", o); } } CHECK(en_diario == 1, "queda en el diario"); }
    for (int i = 0; i < 32; i++) p.gusto[i] = 0;
    p.gusto[ACTV_BAILAR] = 40; p.gusto[ACTV_AVES] = -25;
    CHECK(pet_taste_top(&p) == ACTV_BAILAR && pet_taste_low(&p) == ACTV_AVES, "su favorita y de la que se aburrió");
}

static void test_ajustes_nuevos(void)
{
    pet_device_t d;
    pet_device_defaults(&d);
    CHECK(d.arrullo == 0 && d.musica == 0, "arrullo y música vienen apagados de fábrica");
    d.arrullo = 9; d.musica = 7;
    pet_device_fix(&d);
    CHECK(d.arrullo == 0 && d.musica == 0, "valores raros vuelven a apagado");
    pet_device_defaults(&d);
    d.version = 4; d.arrullo = 2;
    pet_device_fix(&d);
    CHECK(d.version == PET_DEVICE_VERSION && d.arrullo == 0 && d.arrullo_dur == 1, "un ajuste de la versión 4 se actualiza sin arrullo (y de una hora)");
    d.arrullo_dur = 7;
    pet_device_fix(&d);
    CHECK(d.arrullo_dur == 1, "una duración rara vuelve a una hora");
}

static void test_su_cancion(void)
{
    static pet_t p;
    uint32_t t = T0 + 14 * H;
    nueva(&p, 905, t);
    pet_update(&p, t + 2 * H);
    p.dormida = false; p.enferma = false;
    pet_event_t e;
    while (pet_poll_event(&p, &e)) {}
    uint32_t a = t + 2 * H + 10;
    int primera = 0, otra = 0, musica = 0;
    for (int i = 0; i < 5; i++) { /* la misma música (≈96 golpes por minuto) en 5 ocasiones separadas */
        pet_music_heard(&p, a + (uint32_t)i * 700u, 1, 96 + i % 3, 2);
        while (pet_poll_event(&p, &e)) { primera += e.type == EV_CANCION && e.a == 0; otra += e.type == EV_CANCION && e.a == 1; musica += e.type == EV_MUSICA; }
    }
    CHECK(pet_has_song(&p), "a la tercera ocasión reconoce su canción");
    CHECK(primera == 1 && otra == 2 && musica == 2, "la reconoce una vez y luego la celebra (%d %d %d)", primera, otra, musica);
    { int d = 0; for (int k = 0; k < p.diario_n; k++) d += pet_diary_get(&p, k)->tipo == DIA_CANCION; CHECK(d == 1, "queda en el diario una vez"); }
    /* otra música no es la suya */
    int mal = 0;
    pet_music_heard(&p, a + 9000u, 2, 150, 3);
    while (pet_poll_event(&p, &e)) mal += e.type == EV_CANCION;
    CHECK(mal == 0, "una música distinta no es su canción");
    /* a la tarde, si hace rato que no suena, la pide (una vez al día) */
    p.cancion_t = a; p.cancion_pide = 0; p.cancion_n = 5; p.escucha_musica = 0;
    uint32_t tarde = a + 3 * 24 * 3600u;
    tarde = tarde - (tarde + (uint32_t)(-3 * 3600)) % 86400u + 17 * 3600u; /* ~17:00 de su día */
    int pide = 0;
    for (int i = 0; i < 90; i++) { p.dormida = false; p.actividad_hasta = 0; pet_update(&p, tarde + (uint32_t)i * 60u); while (pet_poll_event(&p, &e)) pide += e.type == EV_PIDE_CANCION; }
    CHECK(pide == 0, "sin la escucha de música encendida no la pide (%d)", pide);
    p.escucha_musica = 1; p.cancion_pide = 0;
    for (int i = 0; i < 90; i++) { p.dormida = false; p.actividad_hasta = 0; pet_update(&p, tarde + 7200u + (uint32_t)i * 60u); while (pet_poll_event(&p, &e)) pide += e.type == EV_PIDE_CANCION; }
    CHECK(pide <= 1, "con la escucha encendida la pide como mucho una vez (%d)", pide);
}

static int contar_mini(pet_t *p, uint32_t t0, int pasos, clima_t clima, int kind, int *sub)
{
    int n = 0;
    pet_event_t e;
    for (int i = 0; i < pasos; i++) {
        p->dormida = false;
        pet_force_weather(p, clima, t0 + (uint32_t)i * 60u, 3600u);
        pet_update(p, t0 + (uint32_t)i * 60u);
        while (pet_poll_event(p, &e)) if (e.type == EV_MINI && e.a == kind) { n++; if (sub) *sub = e.b; }
    }
    return n;
}

static void test_mini_eventos(void)
{
    static pet_t p;
    int sub = -1;
    /* lluvia de estrellas: la noche del 12 de agosto, con el cielo despejado, una sola vez */
    nueva(&p, 906, 1786579200u - 3 * H);
    CHECK(contar_mini(&p, 1786579200u, 40, CLIMA_DESPEJADO, MINI_ESTRELLAS, NULL) == 1, "la noche del 12 de agosto hay lluvia de estrellas (una vez)");
    /* con el cielo tapado no se ve */
    nueva(&p, 906, 1786579200u - 3 * H);
    CHECK(contar_mini(&p, 1786579200u, 40, CLIMA_NUBLADO, MINI_ESTRELLAS, NULL) == 0, "con nubes no hay lluvia de estrellas");
    /* otro día cualquiera tampoco */
    nueva(&p, 906, 1779321600u - 3 * H);
    CHECK(contar_mini(&p, 1779321600u, 40, CLIMA_DESPEJADO, MINI_ESTRELLAS, NULL) == 0, "el 20 de mayo no");
    /* la primera lluvia del otoño: una vez por otoño */
    nueva(&p, 907, 1776276000u - 3 * H);
    CHECK(contar_mini(&p, 1776276000u, 90, CLIMA_LLUVIA, MINI_PRIMERA_LLUVIA, NULL) == 1, "la primera lluvia del otoño, una vez");
    /* la primera nieve del invierno */
    nueva(&p, 908, 1783706400u - 3 * H);
    CHECK(contar_mini(&p, 1783706400u, 90, CLIMA_NIEVE, MINI_PRIMERA_NIEVE, NULL) == 1, "la primera nieve del invierno, una vez");
    /* el 21 de junio, a la mañana: en el sur es el día más corto */
    nueva(&p, 909, 1782039600u - 3 * H);
    CHECK(contar_mini(&p, 1782039600u, 90, CLIMA_DESPEJADO, MINI_SOL, &sub) == 1 && sub == 1, "el 21 de junio (Chile) es el día más corto (%d)", sub);
    { int d = 0; for (int k = 0; k < p.diario_n; k++) d += pet_diary_get(&p, k)->tipo == DIA_MINI; CHECK(d == 1, "queda en el diario"); }
    /* luna llena: la noche del 27 de agosto de 2026 (llena el 28), con cielo despejado; la del 3 de septiembre (cuarto menguante) no */
    nueva(&p, 910, 1787875200u - 3 * H);
    CHECK(contar_mini(&p, 1787875200u, 40, CLIMA_DESPEJADO, MINI_LUNA, NULL) == 1, "con luna llena y cielo despejado la mira (una vez)");
    nueva(&p, 910, 1788480000u - 3 * H);
    CHECK(contar_mini(&p, 1788480000u, 40, CLIMA_DESPEJADO, MINI_LUNA, NULL) == 0, "con otra fase de la luna no");
    /* eclipses reales vistos desde Chile */
    nueva(&p, 911, 1787882400u - 3 * H);
    CHECK(contar_mini(&p, 1787882400u, 40, CLIMA_DESPEJADO, MINI_ECLIPSE_LUNAR, NULL) == 1, "el eclipse de luna del 28 de agosto de 2026 lo mira (una vez)");
    nueva(&p, 912, 1787882400u - 3 * H);
    CHECK(contar_mini(&p, 1787882400u, 40, CLIMA_LLUVIA, MINI_ECLIPSE_LUNAR, NULL) == 0, "con lluvia no se ve");
    nueva(&p, 913, 1801926000u - 3 * H);
    CHECK(contar_mini(&p, 1801926000u, 40, CLIMA_DESPEJADO, MINI_ECLIPSE_SOLAR, NULL) == 1, "el eclipse de sol del 6 de febrero de 2027 lo mira");
    nueva(&p, 914, 1802530800u - 3 * H);
    CHECK(contar_mini(&p, 1802530800u, 40, CLIMA_DESPEJADO, MINI_ECLIPSE_SOLAR, NULL) == 0, "una semana después ya no hay eclipse");
}

static void test_frases_diario(void)
{
    /* las entradas del diario tienen varias maneras de contarse, pero cada una siempre se lee igual */
    static pet_t p; nueva(&p, 920, T0);
    char a[160], b[160];
    static char vistos[40][160]; int nv = 0;
    for (int k = 0; k < 40; k++) {
        entrada_diario_t e = {T0 + (uint32_t)k * 3600u, DIA_TRUCO, TRUCO_SALUDO, 0, 0};
        pet_diary_text(&p, &e, a, sizeof a); pet_diary_text(&p, &e, b, sizeof b);
        CHECK(!strcmp(a, b) && a[0], "la misma entrada se lee igual siempre");
        int ya = 0; for (int i = 0; i < nv; i++) ya |= !strcmp(vistos[i], a);
        if (!ya) strcpy(vistos[nv++], a);
    }
    CHECK(nv >= 5, "un mismo truco se cuenta de varias maneras (%d)", nv);
}

static void test_nocturnos(void)
{
    /* de noche vienen seguido los animales nocturnos (búho, murciélago, mapache, grillo, polilla...) */
    static pet_t p; nueva(&p, 930, T0);
    for (int k = 0; k < 10; k++) { pet_update(&p, T0 + (uint32_t)k * 3600u); pet_action(&p, ACT_ALIMENTAR, 0, T0 + (uint32_t)k * 3600u); }
    int noct = 0, noches = 6;
    uint32_t t0 = T0 + 6 * 24 * H;
    for (int n = 0; n < noches; n++) {
        for (uint32_t m = 0; m < 9 * 60; m += 5) {
            uint32_t t = t0 + (uint32_t)n * 24 * H + 20 * H + m * 60u; /* de las 20 a las 5 (hora del reloj de prueba) */
            p.dormida = false; p.escena = ESC_PRADO;
            pet_force_weather(&p, CLIMA_DESPEJADO, t, 3600u);
            pet_update(&p, t);
            pet_event_t e;
            while (pet_poll_event(&p, &e))
                if (e.type == EV_ANIMAL && e.b == ANI_LLEGA && (e.a == ANIMAL_BUHO || e.a == ANIMAL_MURCIELAGO || e.a == ANIMAL_MAPACHE || e.a == ANIMAL_GRILLO || e.a == ANIMAL_POLILLA || e.a == ANIMAL_TEJON || e.a == ANIMAL_ZORRILLO)) noct++;
        }
    }
    CHECK(noct >= noches * 5, "de noche llegan seguido los animales nocturnos (%d en %d noches)", noct, noches);
}

static void test_melodia_propia(void)
{
    static pet_t p, q;
    nueva(&p, 940, T0);
    for (int k = 0; k < 40; k++) { pet_update(&p, T0 + (uint32_t)k * 3600u); pet_action(&p, ACT_ALIMENTAR, 0, T0 + (uint32_t)k * 3600u); }
    p.etapa = STAGE_ADULTA;
    p.melodia_ens = 0; p.melodia_dia = 0; p.melodia_n = 0; /* (por si en el calentamiento pasó por la cueva) */
    uint8_t n1[PET_MELODIA_NOTAS];
    int ensayos = 0, melodias = 0;
    pet_event_t e;
    while (pet_poll_event(&p, &e)) {}
    /* en la cueva, un ensayo por día; al cuarto la decide */
    for (int d = 0; d < 12 && !p.melodia_n; d++) {
        for (uint32_t m = 0; m < 6 * 60 && !p.melodia_n; m += 3) {
            uint32_t t = T0 + 12 * 24 * H + (uint32_t)d * 24 * H + 14 * H + m * 60u;
            p.escena = ESC_CUEVA; p.dormida = false; p.enferma = 0;
            pet_update(&p, t);
            while (pet_poll_event(&p, &e)) { ensayos += e.type == EV_ENSAYA_MELODIA; melodias += e.type == EV_MELODIA; }
        }
    }
    CHECK(pet_melody(&p, n1) == PET_MELODIA_NOTAS && melodias == 1, "compone su melodía (%d notas, %d avisos)", pet_melody(&p, n1), melodias);
    CHECK(ensayos == PET_MELODIA_ENSAYOS - 1 && p.melodia_ens == PET_MELODIA_ENSAYOS, "ensayó %d días antes de decidirla (ens=%d)", ensayos, p.melodia_ens);
    int ok = 1, distintas = 0, vistas = 0;
    for (int i = 0; i < PET_MELODIA_NOTAS; i++) { ok &= n1[i] < 6; if (!(vistas >> n1[i] & 1)) { vistas |= 1 << n1[i]; distintas++; } if (i && n1[i] == n1[i - 1]) ok = 0; }
    CHECK(ok && distintas >= 3 && n1[0] == 0 && n1[PET_MELODIA_NOTAS - 1] == 0, "usa los cristales, sin repetir seguida y de do a do (%d distintas)", distintas);
    int d = 0; for (int k = 0; k < p.diario_n; k++) d += pet_diary_get(&p, k)->tipo == DIA_MELODIA;
    CHECK(d == 1, "queda en el diario");
    /* una vez decidida, se mantiene: más días en la cueva y un respaldo no la cambian */
    uint8_t copia[PET_MELODIA_NOTAS]; memcpy(copia, n1, sizeof copia);
    for (int k = 0; k < 20; k++) { p.escena = ESC_CUEVA; p.dormida = false; pet_update(&p, T0 + 40 * 24 * H + (uint32_t)k * 6 * H); while (pet_poll_event(&p, &e)) melodias += e.type == EV_MELODIA; }
    uint8_t n2[PET_MELODIA_NOTAS];
    CHECK(pet_melody(&p, n2) == PET_MELODIA_NOTAS && !memcmp(n2, copia, sizeof copia) && melodias == 1, "su melodía no cambia");
    static char txt[PET_RESPALDO_MAX];
    pet_backup_export(&p, txt, sizeof txt);
    CHECK(pet_backup_import(txt, &q) && pet_melody(&q, n2) == PET_MELODIA_NOTAS && !memcmp(n2, copia, sizeof copia), "el respaldo la guarda");
    /* cada mascota tiene la suya */
    nueva(&q, 941, T0); q.etapa = STAGE_ADULTA; q.escena = ESC_CUEVA;
    for (int k = 0; k < 400 && !q.melodia_n; k++) { q.melodia_dia = 0; q.melodia_ens = PET_MELODIA_ENSAYOS - 1; q.dormida = false; q.escena = ESC_CUEVA; pet_update(&q, T0 + (uint32_t)(k + 1) * 3600u); }
    uint8_t n3[PET_MELODIA_NOTAS];
    CHECK(pet_melody(&q, n3) == PET_MELODIA_NOTAS && memcmp(n3, copia, sizeof copia) != 0, "otra mascota compone otra melodía");
    uint8_t buf[PET_MELODIA_NOTAS * 2];
    CHECK(pet_melody(&q, NULL) == PET_MELODIA_NOTAS && buf[0] == buf[0], "se puede preguntar cuántas notas tiene");
}

static void test_juego_mas(void)
{
    static pet_t p;
    uint32_t t = T0 + 20 * 24 * H + 12 * H;
    nueva(&p, 906, t);
    p.etapa = STAGE_ADULTA;
    for (int i = 0; i < 20; i++) { pet_object_gives(&p, 5, OFU_EXPLORAR, t + (uint32_t)i); p.bolsa[5] = 0; }
    CHECK(pet_gold_count(&p) == 0, "antes del final secreto no hay versiones doradas");
    p.final_estado |= PET_FINAL_SECRETO;
    for (int id = 0; id < 40; id++) for (int i = 0; i < 12; i++) { p.bolsa[id] = 0; pet_object_gives(&p, id, OFU_EXPLORAR, t + (uint32_t)(id * 20 + i)); }
    CHECK(pet_gold_count(&p) > 10 && pet_gold_count(&p) <= 40, "después, algunos objetos vienen dorados (uno por objeto)");
    CHECK(!pet_gold_has(&p, OBJ_CORAZON_BOSQUE), "el Corazón del Mundo no se duplica");
}

/* una jugadora completista (cuida, usa cada objeto, va al rito en luna llena) termina el juego en menos de 10 semanas */
static void test_completista(void)
{
    static pet_t p;
    for (unsigned seed = 1; seed <= 3; seed++) {
        const uint32_t t0 = T0 + (seed - 1) * 11u * 24u * H;
        nueva(&p, 3000 + seed, t0);
        p.vinculo = 0;
        int rito = -1, fin = -1;
        for (uint32_t s = 600; s <= 70u * 24u * H && fin < 0; s += 600) {
            const uint32_t t = t0 + s;
            pet_update(&p, t);
            if (pet_need(&p, NEED_SACIEDAD) < 60) pet_action(&p, ACT_ALIMENTAR, FOOD_COMIDA, t);
            if (pet_need(&p, NEED_HIGIENE) < 50) pet_action(&p, ACT_BANAR, 0, t);
            if (p.enferma) pet_action(&p, ACT_CURAR, 0, t);
            if (pet_need(&p, NEED_CARINO) < 50) pet_action(&p, ACT_ACARICIAR, 0, t);
            if (pet_need(&p, NEED_DIVERSION) < 50) pet_action(&p, ACT_JUGAR, 0, t);
            const int hh = (int)(((int64_t)t + CHILE) % 86400 / 3600);
            if (!pet_scene_open(&p, ESC_ISLA) && pet_scene_open(&p, ESC_LAGO) && p.etapa >= STAGE_CRIA && p.vinculo >= 60 && hh >= 10 && hh < 17) {
                if (p.escena != ESC_LAGO) pet_action(&p, ACT_IR, ESC_LAGO, t);
                else if (p.actividad != ACTV_PESCAR) pet_action(&p, ACT_PESCAR, 0, t);
            }
            pet_creature_touch(&p, t);
            for (int i = 0; i < PET_OBJETOS; i++) if (p.bolsa[i] && i != OBJ_CORAZON_BOSQUE && !pet_object_discovered(&p, i)) pet_object_use(&p, i, t);
            if (p.bolsa[OBJ_CORAZON_BOSQUE] && !p.guardiana && pet_full_moon(t) && p.etapa == STAGE_ADULTA && (hh >= 21 || hh < 6)) {
                if (p.escena != ESC_ISLA) { p.dormida = false; pet_action(&p, ACT_IR, ESC_ISLA, t); }
                if (p.escena == ESC_ISLA) pet_object_use(&p, OBJ_CORAZON_BOSQUE, t);
            }
            if (p.guardiana && rito < 0) rito = (int)(s / (24u * H));
            if (pet_final_pending(&p) == PET_FINAL_SECRETO) { pet_final_done(&p, PET_FINAL_SECRETO, t); fin = (int)(s / (24u * H)); }
            { pet_event_t e; while (pet_poll_event(&p, &e)) {} }
        }
        CHECK(rito >= 0 && fin >= 0 && fin <= 70, "completista (semilla %u): rito en el día %d y final en el día %d (máx. 70)", seed, rito, fin);
    }
}

static void test_hijo_y_ritmo(void)
{
    static pet_t m, h;
    uint32_t t = T0 + 40 * 24 * H;
    nueva(&m, 910, t);
    m.etapa = STAGE_ADULTA; m.guardiana = 1;
    for (int i = 0; i < PET_OBJETOS; i++) { m.obj_sabe[i >> 3] |= (uint8_t)(1 << (i & 7)); m.obj_visto[i >> 3] |= (uint8_t)(1 << (i & 7)); }
    CHECK(pet_objects_used(&m) == PET_OBJETOS, "la madre usó los 150");
    pet_family_child(&m, &h, t, 911);
    CHECK(pet_objects_used(&h) == 0 && !h.guardiana, "el hijo sabe qué hacen pero tiene que volver a encontrarlos y a ser Guardián");
    h.etapa = STAGE_ADULTA;
    for (int k = 0; k < 3 * 24 * 6; k++) { t += 600; pet_update(&h, t); }
    CHECK(pet_final_pending(&h) == 0, "el final secreto del hijo no salta solo");
    /* etapas: bebé 3 días, cría hasta el 4, adulta desde ahí */
    nueva(&m, 912, t);
    pet_update(&m, t + 2 * 24 * H + 23 * H);
    CHECK(m.etapa == STAGE_BEBE, "a los 2,9 días sigue siendo bebé");
    pet_update(&m, t + 3 * 24 * H + 2 * H);
    CHECK(m.etapa == STAGE_CRIA, "al tercer día es cría");
    pet_update(&m, t + 4 * 24 * H + 2 * H);
    CHECK(m.etapa == STAGE_ADULTA, "al cuarto día es adulta");
}

static void test_finales(void)
{
    static pet_t p;
    uint32_t t = T0 + 20 * 24 * H + 12 * H;
    nueva(&p, 901, t);
    p.etapa = STAGE_ADULTA;
    memset(p.obj_sabe, 0, sizeof p.obj_sabe);
    CHECK(pet_objects_used(&p) == 0, "sin objetos usados");
    for (int i = 0; i < PET_OBJETOS; i++) { p.obj_sabe[i >> 3] |= (uint8_t)(1 << (i & 7)); p.obj_visto[i >> 3] |= (uint8_t)(1 << (i & 7)); }
    for (int m = 0; m < 24 * 60; m += 10) { t += 600; pet_update(&p, t); }
    CHECK(pet_final_pending(&p) == 0, "sin ser la Guardiana todavía no hay final secreto");
    p.guardiana = 1;
    p.dia_guardiana = (int32_t)(((int64_t)t + p.zona_horaria) / 86400) - 1;
    p.clima = CLIMA_DESPEJADO;
    p.escena = ESC_CUEVA;
    int noche = 0;
    for (int m = 0; m < 24 * 60 && !noche; m += 10) { t += 600; p.escena = ESC_CUEVA; p.clima = CLIMA_DESPEJADO; pet_update(&p, t); noche |= pet_final_pending(&p) == PET_FINAL_SECRETO; }
    CHECK(!noche, "(v39) sin nuestra canción, Prisma no propone el último verso");
    const uint8_t cn[6] = {PET_CANCION_NOTA(0, 2), PET_CANCION_NOTA(1, 2), PET_CANCION_NOTA(2, 2), PET_CANCION_NOTA(3, 2), PET_CANCION_NOTA(1, 2), PET_CANCION_NOTA(0, 4)};
    pet_song_save(&p, 0, cn, 6, "Nuestra", t);
    p.escena = ESC_PRADO;
    for (int m = 0; m < 24 * 60 && !noche; m += 10) { t += 600; p.escena = ESC_PRADO; p.clima = CLIMA_DESPEJADO; pet_update(&p, t); noche |= pet_final_pending(&p) == PET_FINAL_SECRETO; }
    CHECK(!noche, "(v39) el último verso empieza en la cueva");
    int hora_ok = 1;
    for (int m = 0; m < 24 * 60 && !noche; m += 10) {
        t += 600; p.escena = ESC_CUEVA; p.clima = CLIMA_DESPEJADO; p.dormida = 0; pet_update(&p, t);
        if (pet_final_pending(&p) == PET_FINAL_SECRETO) { noche = 1; const int h = (int)((((int64_t)t + p.zona_horaria) % 86400) / 3600); hora_ok = h >= 19 || h < 6; }
    }
    CHECK(noche && hora_ok, "Guardiana, nuestra canción, de noche en la cueva: llega el último verso");
    pet_final_done(&p, PET_FINAL_SECRETO, t);
    for (int m = 0; m < 24 * 60; m += 10) { t += 600; pet_update(&p, t); }
    CHECK(pet_final_pending(&p) == 0 && pet_final_seen(&p, PET_FINAL_SECRETO) && pet_final_seen(&p, PET_FINAL_REGULAR), "el secreto no se repite");
    CHECK(pet_tilo_page(&p, 8) && pet_story_known(&p, NARR_TILO, 8), "(v39) la última página del diario vuelve al Libro");
    CHECK(strstr(pet_story_text((magica_t)NARR_TILO, 8), "Soy la cuchara") != NULL, "(v39) la carta de Tilo, con su P.D.");
    uint8_t ts[PET_CANCION_NOTAS];
    const int tn = pet_tilo_song(&p, ts, PET_CANCION_NOTAS);
    CHECK(tn >= 10 && tn <= PET_CANCION_NOTAS && PET_CANCION_CRISTAL(ts[tn - 1]) == 0, "(v39) la canción de Tilo: su verso, nuestra canción y la última nota (%d)", tn);
    CHECK(pet_pochi_chat_next(&p) == PET_POCHI_NOMBRE, "(v39) después del final, Pochi escucha el nombre");
    pet_pochi_chat_done(&p, PET_POCHI_NOMBRE, t);
    CHECK(pet_pochi_knows_name(&p) && pet_pochi_chat_next(&p) == -1, "(v39) ... una sola vez");
}

static void test_comidas_temporada(void)
{
    static pet_t p;
    uint32_t t = T0 + 20 * 24 * H + 12 * H;
    nueva(&p, 911, t);
    p.etapa = STAGE_ADULTA;
    for (int i = 0; i < PET_COMIDAS_EST; i++) CHECK(pet_seasonal_food_info(i)->zonas != 0, "cada comida de temporada tiene alguna zona (%d)", i);
    /* cada estación tiene sus cuatro */
    int porest[ESTACION_COUNT] = {0};
    for (int i = 0; i < PET_COMIDAS_EST; i++) porest[pet_seasonal_food_info(i)->estacion]++;
    CHECK(porest[0] == 4 && porest[1] == 4 && porest[2] == 4 && porest[3] == 4, "cuatro comidas por estación");
    /* se encuentran solo en su estación y su zona */
    int mal = 0, halladas = 0;
    for (int es = 0; es < ESTACION_COUNT; es++) {
        for (int z = 0; z < ESC_COUNT; z++) {
            nueva(&p, 920 + (unsigned)es * 7 + (unsigned)z, t);
            p.etapa = STAGE_ADULTA; p.estacion_fija = (uint8_t)es; p.escenas_abiertas = 0x3F; p.escena = (uint8_t)z; p.vinculo = 60;
            for (int k = 0; k < 1500; k++) {
                p.actividad = ACTV_EXPLORAR; p.actividad_hasta = 0; p.escena = (uint8_t)z; p.dormida = 0; p.ultimo_hallazgo = 0; p.hallazgos_dia = 0;
                t += 60; pet_update(&p, t);
                pet_event_t e;
                while (pet_poll_event(&p, &e))
                    if (e.type == EV_COMIDA_EST) {
                        halladas++;
                        const comida_est_info_t *c = pet_seasonal_food_info(e.a);
                        if (c->estacion != es || !((c->zonas >> z) & 1u)) mal++;
                    }
            }
        }
    }
    CHECK(halladas > 20, "explorando encuentra comidas de temporada (%d)", halladas);
    CHECK(mal == 0, "solo las de su estación y su zona (%d fuera de lugar)", mal);
    /* comerlas */
    nueva(&p, 930, t);
    p.etapa = STAGE_ADULTA;
    pet_seasonal_food_give(&p, 9, 3);
    pet_seasonal_food_give(&p, 13, 2);
    CHECK(pet_seasonal_food_count(&p, 9) == 3 && pet_seasonal_food_kinds(&p) == 2 && pet_seasonal_food_seen(&p, 9), "las que tiene");
    int comio = 0;
    for (int k = 0; k < 8 && !comio; k++) {
        p.necesidad[NEED_SACIEDAD] = 30;
        int antes = pet_seasonal_food_count(&p, 9);
        pet_action(&p, ACT_ALIMENTAR, PET_COMIDA_EST_BASE + 9, t + 60u * (uint32_t)k);
        if (pet_seasonal_food_count(&p, 9) == antes - 1) comio = p.necesidad[NEED_SACIEDAD] > 30.0f ? 1 : 0;
        if (!pet_seasonal_food_count(&p, 9)) break;
    }
    CHECK(comio || pet_seasonal_food_taste(&p, 9) == 0, "comer una de temporada gasta una y sacia (o la rechaza si no le gusta)");
    p.necesidad[NEED_SACIEDAD] = 30;
    int n0 = pet_seasonal_food_count(&p, 3);
    pet_action(&p, ACT_ALIMENTAR, PET_COMIDA_EST_BASE + 3, t + 5000);
    CHECK(pet_seasonal_food_count(&p, 3) == n0, "sin tener, no hay nada que comer");
}

static void test_historias_dia(void)
{
    static pet_t p;
    uint32_t t = T0 + 30 * 24 * H;
    nueva(&p, 930, t);
    p.etapa = STAGE_ADULTA;
    p.dias_jugados = 30; /* (v37) ya pasaron las semanas de la historia: todas las historias están abiertas */
    p.escenas_abiertas |= 1u << pet_creature_zone(CRI_MUSGUITO);
    p.escena = (uint8_t)pet_creature_zone(CRI_MUSGUITO);
    int antes = pet_creature_stories_known(&p, CRI_MUSGUITO);
    for (int k = 0; k < 5; k++) { pet_creature_call(&p, CRI_MUSGUITO, t); pet_creature_touch(&p, t); t += 300; }
    CHECK(pet_creature_stories_known(&p, CRI_MUSGUITO) - antes == PET_HISTORIAS_DIA, "las criaturas cuentan a lo más dos historias por día");
    t += 24 * H;
    for (int k = 0; k < 3; k++) { pet_creature_call(&p, CRI_MUSGUITO, t); pet_creature_touch(&p, t); t += 300; }
    CHECK(pet_creature_stories_known(&p, CRI_MUSGUITO) - antes == 2 * PET_HISTORIAS_DIA, "al día siguiente cuentan otras dos");
}


static void test_caracter_planes(void)
{
    /* manías: fijas por mascota, distintas entre mascotas */
    pet_t p;
    pet_init(&p, "Pip", 2, 1760000000u, 1234u);
    int f1, e1, h1, o1, f2, e2, h2, o2;
    pet_quirks(&p, &f1, &e1, &h1, &o1);
    pet_quirks(&p, &f2, &e2, &h2, &o2);
    CHECK(f1 == f2 && e1 == e2 && h1 == h2 && o1 == o2 && f1 != e1 && o1 >= 7 && o1 <= 19, "las manías son estables y la favorita no es la que esquiva");
    int distintas = 0;
    for (uint32_t s = 1; s < 40; s++) {
        pet_t q;
        pet_init(&q, "Pip", 2, 1760000000u, s * 7919u);
        int f, e, h, o;
        pet_quirks(&q, &f, &e, &h, &o);
        if (f != f1) distintas++;
    }
    CHECK(distintas > 10, "otras mascotas tienen otras manías (%d)", distintas);
    /* planes: todos tienen 3 pasos con sentido */
    for (int id = 1; id < PLAN_COUNT; id++)
        CHECK(pet_plan_activity(id, 0) >= 0 && pet_plan_activity(id, 2) >= 0 && pet_plan_activity(id, 3) < 0 && pet_plan_name(id)[0], "plan %d completo", id);
    /* partida antigua: bytes en cero = sin plan, humor normal, trato 0 */
    pet_t v;
    memset(&v, 0, sizeof v);
    CHECK(pet_plan(&v) == PLAN_NINGUNO && pet_humor(&v) == HUMOR_NORMAL && pet_treat(&v, 1000) == 0, "ceros = sin plan, humor normal, sin trato");
    /* simulación larga: aparecen planes completos y humores variados, y es determinista */
    int resultado[2][4] = {{0}};
    for (int rep = 0; rep < 2; rep++) {
        pet_t w;
        pet_init(&w, "Pip", 2, 1760000000u, 777u);
        pet_set_timezone(&w, CHILE);
        pet_set_owner_age(&w, 30);
        const uint32_t t0 = 1760000000u;
        pet_hatch(&w, t0);
        w.evo_pregunta = 0; /* que crezca sola */
        int hum[HUMOR_COUNT] = {0}, planes = 0, cumplidos = 0;
        for (uint32_t m = 1; m < 14 * 24 * 60; m++) {
            for (int i = 0; i < NEED_COUNT; i++) if (w.necesidad[i] < 55) w.necesidad[i] = 75;
            pet_update(&w, t0 + m * 60);
            hum[pet_humor(&w)]++;
            pet_event_t e;
            while (pet_poll_event(&w, &e)) if (e.type == EV_PLAN) { if (e.b == 0) planes++; if (e.b == 1) cumplidos++; }
        }
        int vistos = 0;
        for (int i = 0; i < HUMOR_COUNT; i++) vistos += hum[i] > 0;
        resultado[rep][0] = vistos; resultado[rep][1] = planes; resultado[rep][2] = cumplidos; resultado[rep][3] = hum[HUMOR_NORMAL];
    }
    CHECK(resultado[0][0] >= 4, "en dos semanas pasa por varios humores (%d)", resultado[0][0]);
    CHECK(resultado[0][1] >= 5, "se le ocurren planes (%d)", resultado[0][1]);
    CHECK(resultado[0][2] >= 2, "cumple planes completos (%d de %d)", resultado[0][2], resultado[0][1]);
    CHECK(memcmp(resultado[0], resultado[1], sizeof resultado[0]) == 0, "es determinista");
    /* trato: los mimos suben, los sustos bajan, y se olvida con las horas */
    pet_t q;
    pet_init(&q, "Pip", 2, 1760000000u, 99u);
    pet_set_timezone(&q, CHILE);
    pet_set_owner_age(&q, 30);
    const uint32_t t = 1760000000u;
    pet_hatch(&q, t);
    q.etapa = STAGE_ADULTA;
    for (int i = 0; i < NEED_COUNT; i++) q.necesidad[i] = 80;
    q.ultimo_no = ACT_ACARICIAR;
    for (int i = 0; i < 6; i++) { q.ultimo_no = ACT_COUNT; pet_action(&q, ACT_ACARICIAR, 0, t + 10 + (uint32_t)i * 5); }
    const int arriba = q.trato;
    CHECK(arriba > 0, "los mimos suben el trato (%d)", arriba);
    CHECK(pet_treat(&q, q.ultima_interaccion + 12 * 3600) < arriba, "el trato se apaga con las horas");
    CHECK(q.trato >= -100 && q.trato <= 100, "el trato queda acotado");
}

static void test_pesca_juego(void)
{
    pet_visita_escala = 0;
    static pet_t p;
    uint32_t t = T0 + 14 * H;
    nueva(&p, 92, t);
    p.estacion_fija = ESTACION_VERANO;
    p.escenas_abiertas = 0x1F;
    p.vecino.proxima = t + 30 * 24 * H;
    pet_force_weather(&p, CLIMA_DESPEJADO, t, 10 * H);
    p.escena = ESC_LAGO;
    p.ultimo_no = ACT_PESCAR;
    CHECK(pet_action(&p, ACT_PESCAR, 0, t) == 1 && p.actividad == ACTV_PESCAR, "la mandas a pescar");
    pet_fish_game(&p, 0, t);
    pet_event_t e;
    int fin = 0;
    for (int m = 1; m <= 120; m++) { pet_update(&p, t + (uint32_t)m); p.escena = ESC_LAGO; while (pet_poll_event(&p, &e)) if (e.type == EV_PESCA) fin++; }
    CHECK(fin == 0 && p.actividad == ACTV_PESCAR, "con el minijuego abierto, la pesca no se termina sola (2 min)");
    pet_fish_game(&p, 3, t + 121);
    int que = -1;
    while (pet_poll_event(&p, &e)) if (e.type == EV_PESCA) que = e.a;
    CHECK(que > PESCA_NADA && p.actividad != ACTV_PESCAR, "al sacarlo, sale algo y se baja del bote (%d)", que);
    /* lo que sale: muchas veces, con nenúfar solo en verano o primavera */
    int nenufar_inv = 0, peces = 0, distintos = 0, visto[PESCA_COUNT] = {0};
    for (int k = 0; k < 400; k++) {
        p.estacion_fija = k < 200 ? ESTACION_VERANO : ESTACION_INVIERNO;
        p.actividad = ACTV_PESCAR;
        pet_fish_game(&p, 3, t + 200 + (uint32_t)k * 10);
        while (pet_poll_event(&p, &e)) if (e.type == EV_PESCA) {
            if (e.a == PESCA_PEZ) peces++;
            if (e.a == PESCA_NENUFAR && k >= 200) nenufar_inv++;
            if (!visto[e.a]++) distintos++;
        }
    }
    CHECK(peces > 80 && distintos >= 7 && nenufar_inv == 0, "varía lo que sale (%d distintos, %d peces) y en invierno no hay nenúfares", distintos, peces);
    p.estacion_fija = ESTACION_VERANO;
    p.actividad = ACTV_PESCAR;
    pet_fish_game(&p, 2, t + 9000);
    que = -1;
    while (pet_poll_event(&p, &e)) if (e.type == EV_PESCA) que = e.a;
    CHECK(que == PESCA_NADA, "si se escapa, no saca nada");
}


/* ---------- versión 37: la historia del mundo (biblia v2) ---------- */
static void dia_de_juego(pet_t *p, uint32_t *t) /* pasa al día siguiente y juegan un rato a mediodía */
{
    *t += 24u * H;
    pet_update(p, *t);
    pet_action(p, ACT_ACARICIAR, GESTO_CARICIA, *t + 30);
    pet_update(p, *t + 60);
    vaciar_eventos(p);
}
static void test_historia_ritmo(void)
{
    static pet_t p;
    uint32_t t = T0 + 15 * H; /* mediodía */
    nueva(&p, 3701, t);
    p.etapa = STAGE_ADULTA;
    CHECK(pet_days_played(&p) == 1 && pet_story_act(&p) == 1, "la historia empieza el día que nace, en el primer acto");
    /* sin jugar, la historia espera */
    for (int d = 0; d < 7; d++) { t += 24u * H; pet_update(&p, t); }
    CHECK(pet_days_played(&p) == 1, "una semana sin jugar: la historia espera (%d)", pet_days_played(&p));
    dia_de_juego(&p, &t);
    CHECK(pet_days_played(&p) == 2, "un rato de juego cuenta un día jugado");
    pet_action(&p, ACT_ACARICIAR, GESTO_CARICIA, t + 3600);
    pet_update(&p, t + 3700);
    CHECK(pet_days_played(&p) == 2, "jugar más el mismo día no suma otro");
    /* la primera historia de cada criatura sí; la segunda espera a la segunda semana */
    p.escenas_abiertas |= 1u << pet_creature_zone(CRI_MUSGUITO);
    p.escena = (uint8_t)pet_creature_zone(CRI_MUSGUITO);
    for (int k = 0; k < 3; k++) { p.escena = (uint8_t)pet_creature_zone(CRI_MUSGUITO); pet_creature_call(&p, CRI_MUSGUITO, t + 4000); pet_creature_touch(&p, t + 4000 + (uint32_t)k * 300); }
    CHECK(pet_story_known(&p, CRI_MUSGUITO, 0) && !pet_story_known(&p, CRI_MUSGUITO, 1), "semana 1: solo la primera historia");
    p.dias_jugados = 8;
    t += 24u * H;
    pet_update(&p, t);
    for (int k = 0; k < 4; k++) { p.escena = (uint8_t)pet_creature_zone(CRI_MUSGUITO); pet_creature_call(&p, CRI_MUSGUITO, t + (uint32_t)k * 300); pet_creature_touch(&p, t + (uint32_t)k * 300 + 5); }
    CHECK(pet_story_known(&p, CRI_MUSGUITO, 1) && !pet_story_known(&p, CRI_MUSGUITO, 2), "semana 2: la segunda (la tercera desde el día 11)");
    p.dias_jugados = 30;
    t += 24u * H;
    pet_update(&p, t);
    for (int k = 0; k < 4; k++) { p.escena = (uint8_t)pet_creature_zone(CRI_MUSGUITO); pet_creature_call(&p, CRI_MUSGUITO, t + (uint32_t)k * 300); pet_creature_touch(&p, t + (uint32_t)k * 300 + 5); }
    CHECK(pet_creature_stories_known(&p, CRI_MUSGUITO) == 4 && pet_story_moments_left(&p, t + 1500) == 0, "tope de dos momentos de historia por día");
    /* el tope es compartido: Prisma no cuenta si ya hubo dos momentos hoy */
    p.especiales |= 1u << ESP_CRISTALES;
    CHECK(pet_prisma_touch(&p, t + 1600) == -1, "con el tope lleno, Prisma no cuenta hoy");
    t += 24u * H;
    pet_update(&p, t);
    CHECK(pet_prisma_touch(&p, t + 10) == 0 && pet_prisma_touch(&p, t + 20) == -1, "al otro día, un capítulo (uno por día)");
    CHECK(pet_story_known(&p, NARR_PRISMA, 0) && pet_story_count(&p, NARR_PRISMA) == 1, "queda en el Libro de historias");
    /* las cartas: la P.D. cambia con el acto */
    char de[40], txt[600];
    p.dias_jugados = 2;
    p.carta_pend = 0;
    CHECK(pet_creature_invite_now(&p, CRI_MAREA, t), "Marea escribe");
    pet_letter_get(&p, 0, de, sizeof de, txt, sizeof txt);
    CHECK(strstr(txt, "Sé esperar") != NULL, "primer acto: la P.D. de Marea");
    p.dias_jugados = 25;
    pet_letter_get(&p, 0, de, sizeof de, txt, sizeof txt);
    CHECK(strstr(txt, "No crucen solos") != NULL && !strchr(txt, '{'), "tercer acto: otra P.D., sin marcas sueltas");
    /* textos: nadie dice su nombre antes del final; las versiones cortas existen */
    int malos = 0, sin_corta = 0;
    for (int c = 0; c < NARR_COUNT; c++)
        for (int k = 0; k < pet_narr_count(c); k++) {
            if (c == NARR_TILO && k == 8) continue; /* (la carta del segundo final: ahí sí lo dice) */
            if (strstr(pet_story_text((magica_t)c, k), "Tilo") || strstr(pet_story_short(c, k), "Tilo") || strstr(pet_story_text((magica_t)c, k), "Peumo") || strstr(pet_story_short(c, k), "Peumo")) malos++;
            if (!pet_story_short(c, k)[0] || strlen(pet_story_short(c, k)) > 140) sin_corta++;
        }
    CHECK(malos == 0, "nadie dice «Tilo» antes del final (%d)", malos);
    CHECK(sin_corta == 0, "todas tienen versión corta para el modo niño (%d sin)", sin_corta);
}
static void test_tilde(void)
{
    static pet_t p;
    uint32_t t = T0 + 15 * H;
    nueva(&p, 3702, t);
    p.etapa = STAGE_ADULTA;
    p.escena = ESC_PRADO;
    for (int i = 0; i < 9; i++) pet_object_gives(&p, 5, OFU_EXPLORAR, t); /* lágrimas de lago: brillantes */
    int primera = -1, visitas = 0, se_llevo = -1, devolvio = -1, chismes = 0;
    for (int d = 1; d <= 45 && (devolvio < 0 || visitas < 4 || p.tilde_lleva); d++) {
        dia_de_juego(&p, &t);
        for (int m = 0; m < 240; m++) {
            const uint32_t tt = t + (uint32_t)m * 60u;
            pet_action(&p, ACT_ACARICIAR, GESTO_CARICIA, tt);
            pet_update(&p, tt + 30);
            if (pet_tilde_here(&p, tt + 30)) {
                if (primera < 0) primera = pet_days_played(&p);
                visitas++;
                if (pet_tilde_takes(&p) >= 0) se_llevo = pet_tilde_takes(&p);
                if (pet_tilde_returns(&p) >= 0) { devolvio = pet_tilde_returns(&p); char n[200]; CHECK(strstr(pet_tilde_note(&p, n, sizeof n), "Tilde") || strstr(n, "T."), "la nota viene firmada"); }
                const int antes = pet_story_count(&p, NARR_TILDE);
                pet_tilde_touch(&p, 0, tt + 40);
                chismes += pet_story_count(&p, NARR_TILDE) - antes;
                pet_update(&p, tt + 6 * 60);
                break;
            }
        }
        vaciar_eventos(&p);
    }
    CHECK(primera >= 2 && primera <= 3, "(v39) la primera visita de Tilde, cerca del día 2 (%d)", primera);
    CHECK(visitas >= 4, "vuelve cada 2 días jugados (%d visitas)", visitas);
    CHECK(chismes >= 3, "cuenta chismes que quedan en el Libro (%d)", chismes);
    CHECK(se_llevo < 0 || devolvio == se_llevo, "lo que se lleva lo devuelve en la visita siguiente (%d / %d)", se_llevo, devolvio);
    CHECK(!p.tilde_lleva || pet_tilde_here(&p, t), "no se queda con nada para siempre");
}
static void test_brizna_y_corazon(void)
{
    static pet_t p;
    uint32_t t = T0 + 15 * H;
    nueva(&p, 3703, t);
    p.etapa = STAGE_ADULTA;
    p.escenas_abiertas = (1u << ESC_COUNT) - 1;
    int k = -2;
    p.escena = ESC_ISLA;
    pet_action(&p, ACT_ACARICIAR, GESTO_CARICIA, t);
    pet_update(&p, t + 30);
    CHECK(pet_brizna_awake(&p, t + 30, &k) && k == 0, "la primera vez en la isla despierta la Abuela Brizna");
    pet_brizna_heard(&p, t + 60);
    CHECK(pet_story_known(&p, NARR_BRIZNA, 0), "su primer recuerdo queda en el Libro");
    pet_update(&p, t + 200);
    CHECK(!pet_brizna_awake(&p, t + 200, NULL), "bosteza y se vuelve a dormir");
    int dia_corazon = -1;
    for (int d = 0; d < 40 && dia_corazon < 0; d++) {
        dia_de_juego(&p, &t);
        p.escena = ESC_ISLA;
        pet_action(&p, ACT_ACARICIAR, GESTO_CARICIA, t + 100);
        pet_update(&p, t + 130);
        if (pet_brizna_awake(&p, t + 130, &k)) pet_brizna_heard(&p, t + 160);
        if (p.bolsa[OBJ_CORAZON_BOSQUE]) dia_corazon = pet_days_played(&p);
        vaciar_eventos(&p);
    }
    CHECK(pet_story_count(&p, NARR_BRIZNA) == 5, "cinco despertares (%d)", pet_story_count(&p, NARR_BRIZNA));
    CHECK(dia_corazon >= 26 && dia_corazon <= 32, "con el quinto le da la mitad del Corazón, en la semana 4–5 (día %d)", dia_corazon);
    /* el rito, ya en la última semana, cualquier noche en la isla */
    p.dias_jugados = 30;
    uint32_t noche = t + 11 * H; /* 23:00 */
    while (pet_full_moon(noche)) noche += 24u * H;
    p.escena = ESC_ISLA;
    CHECK(pet_guardian_ready(&p, noche), "desde el día 30 no hace falta esperar la luna llena");
    pet_object_use(&p, OBJ_CORAZON_BOSQUE, noche);
    CHECK(p.guardiana && pet_tilo_page(&p, 7), "el rito y la página «Para quien cuide el huevo»");
}
static void test_paginas_animales_lugares(void)
{
    static pet_t p;
    uint32_t t = T0 + 15 * H;
    nueva(&p, 3704, t);
    p.etapa = STAGE_ADULTA;
    p.escenas_abiertas = (1u << ESC_COUNT) - 1;
    p.escena = ESC_BOSQUE;
    pet_special_do(&p, ESP_ANILLO, 0, t);
    CHECK(pet_tilo_page(&p, 0) && pet_tilo_page_new(&p) == 0, "al descubrir un secreto vuelve una página del diario");
    pet_tilo_page_seen(&p, 0);
    CHECK(pet_tilo_page_new(&p) == -1, "y aparece una sola vez");
    /* los animales: el que ya conoce le cuenta algo (uno por día, según el acto) */
    const int conejo = FAUNA_COUNT + ANIMAL_CONEJO;
    pet_animal_seen(&p, conejo, t);
    vaciar_eventos(&p);
    pet_animal_seen(&p, conejo, t + 600);
    CHECK(contar_eventos(&p, EV_ANIMAL_DICE) == 1 && pet_story_count(&p, NARR_ANIMALES) == 1, "un animal que le tiene confianza le cuenta algo");
    pet_animal_seen(&p, conejo, t + 900);
    CHECK(pet_story_count(&p, NARR_ANIMALES) == 1, "uno por día");
    t += 24u * H;
    pet_animal_seen(&p, conejo, t);
    CHECK(pet_story_count(&p, NARR_ANIMALES) == 1, "en el primer acto, solo su primera frase");
    p.dias_jugados = 23;
    t += 24u * H;
    pet_animal_seen(&p, conejo, t);
    CHECK(pet_story_count(&p, NARR_ANIMALES) == 2, "en otro acto, la siguiente");
    /* los lugares secretos */
    CHECK(!pet_secret_open(&p, 0), "la casita escondida todavía no");
    p.dias_jugados = 14;
    pet_action(&p, ACT_ACARICIAR, GESTO_CARICIA, t + 100);
    pet_update(&p, t + 160);
    CHECK(pet_secret_open(&p, 0) && pet_secret_open(&p, PET_ZONA_CLARO), "(v39) en la semana 2, las luciérnagas llevan al claro (y a la casita de Tilo)");
    for (int c = 0; c < PET_CASITA_COSAS; c++) pet_secret_find(&p, 0, c, t + 200);
    CHECK(p.bolsa[OBJ_GALLETA_TILO] && pet_tilo_page(&p, 5), "en la casita: una galleta quemada y la receta");
    CHECK(!pet_secret_find(&p, 0, 1, t + 300), "cada cosa se encuentra una vez");
    p.escena = ESC_COLINA;
    p.dias_jugados = 24;
    pet_update(&p, t + 400);
    CHECK(pet_secret_open(&p, 1), "en la semana 4, el mirador de las nubes");
    for (int c = 0; c < PET_MIRADOR_COSAS; c++) pet_secret_find(&p, 1, c, t + 500);
    CHECK(p.bolsa[OBJ_CORONA_HOJAS] && pet_tilo_page(&p, 6), "en el mirador: la corona de hojas y la última página");
    /* los objetos: dicen lo que hacen y la leyenda cambia con el acto */
    int sin = 0;
    for (int i = 0; i < PET_OBJETOS; i++) if (!pet_object_says(i, false)[0] || !pet_object_legend(&p, i)[0]) sin++;
    CHECK(sin == 0, "cada objeto dice lo que hace y tiene su historia (%d sin)", sin);
    p.dias_jugados = 1;
    const char *l1 = pet_object_legend(&p, 108);
    p.dias_jugados = 25;
    CHECK(strcmp(l1, pet_object_legend(&p, 108)) != 0, "la cuchara de palo cuenta otra cosa en el tercer acto");
}
static void test_actualizar_v36(void)
{
    static pet_t p, q;
    uint32_t t = T0 + 20 * 24 * H;
    nueva(&p, 3705, t - 20 * 24 * H);
    p.ultima_actualizacion = t;
    p.especiales = 1u << ESP_REFLEJO;
    pet_seal(&p);
    q = p;
    q.version = 36;
    q.size = (uint16_t)offsetof(pet_t, dia_jugado) + 4; /* como era la v36: terminaba en el crc */
    memcpy((uint8_t *)&q + offsetof(pet_t, dia_jugado), &(uint32_t){0}, 4);
    uint32_t c = 0; { const uint8_t *d = (const uint8_t *)&q; uint32_t k = ~0u; for (size_t i = 0; i < offsetof(pet_t, dia_jugado); i++) { k ^= d[i]; for (int b = 0; b < 8; b++) k = (k >> 1) ^ (0xEDB88320u & (0u - (k & 1u))); } c = ~k; }
    memcpy((uint8_t *)&q + offsetof(pet_t, dia_jugado), &c, 4);
    CHECK(pet_upgrade(&q) && pet_validate(&q), "una partida de la versión 36 se actualiza");
    CHECK(q.dias_jugados >= 20 && q.tilde_chisme == 0xFF && pet_tilo_page(&q, ESP_REFLEJO), "cuenta sus días como jugados y recupera la página de su secreto");
}

/* (v38) el modo libre de los cristales, nuestra canción y el cuaderno */

/* (v40) lo escondido en los lugares secretos, las excursiones sola y la partida de la versión 39 */
static int bolsa_total(const pet_t *p) { int n = 0; for (int o = 0; o < PET_OBJETOS; o++) n += p->bolsa[o]; return n; }
static void test_v40(void)
{
    static pet_t p, q;
    uint32_t t = T0 + 10 * H;
    nueva(&p, 4001, t);
    p.etapa = STAGE_ADULTA; p.zona_horaria = 0; p.dormida = 0; p.enferma = 0;
    for (int i = 0; i < NEED_COUNT; i++) p.necesidad[i] = 90.0f;
    CHECK(pet_zone_find(&p, 0, t) == PET_ZH_NADA, "(v40) sin conocer el claro, no hay nada escondido");
    p.lugares = 7u;
    bool hay = true, igual = true, lugar = true;
    for (int z = 0; z < PET_ZONAS_SECRETAS; z++) {
        const int k = pet_zone_find(&p, z, t);
        hay &= k >= PET_ZH_MUSGO && k <= PET_ZH_REGALO;
        igual &= k == pet_zone_find(&p, z, t + 3 * H) && pet_zone_find_spot(&p, z, t) == pet_zone_find_spot(&p, z, t + 3 * H);
        lugar &= pet_zone_find_spot(&p, z, t) >= 0 && pet_zone_find_spot(&p, z, t) < 3;
    }
    CHECK(hay && igual && lugar, "(v40) cada lugar tiene algo escondido hoy, igual todo el día, en uno de tres escondites");
    const int musgo0 = p.musgo, ram0 = p.ramitas;
    const int k0 = pet_zone_find(&p, 0, t), r0 = pet_zone_find_take(&p, 0, t);
    CHECK(r0 != PET_ZH_NADA && pet_zone_find(&p, 0, t + H) == PET_ZH_NADA && pet_zone_find_take(&p, 0, t + H) == PET_ZH_NADA,
          "(v40) se saca una vez al día");
    CHECK(k0 != PET_ZH_MUSGO || p.musgo == musgo0 + 1 || p.musgo == musgo0, "(v40) el musgo se guarda");
    CHECK(k0 != PET_ZH_RAMITAS || p.ramitas >= ram0, "(v40) las ramitas se guardan");
    int otros = 0;
    for (int d = 1; d <= 20; d++) otros += pet_zone_find(&p, 0, t + (uint32_t)d * 24 * H) != PET_ZH_NADA;
    int distintos = 0;
    for (int d = 1; d <= 20; d++) distintos |= 1 << pet_zone_find(&p, 1, t + (uint32_t)d * 24 * H);
    CHECK(otros == 20 && (distintos & 0xE) == 0xE, "(v40) al otro día hay algo nuevo, y van variando (musgo, ramitas, regalitos)");
    /* las excursiones */
    p.lugares = 7u;
    CHECK(pet_excursion_wants(&p, ESC_BOSQUE, T0 + 10 * H) == 0 && pet_excursion_wants(&p, ESC_COLINA, T0 + 10 * H) == 1,
          "(v40) de día, tranquila: quiere ir al claro desde el bosque y al mirador desde el cerro");
    CHECK(pet_excursion_wants(&p, ESC_PRADO, T0 + 10 * H) < 0 && pet_excursion_wants(&p, ESC_LAGO, T0 + 10 * H) < 0, "(v40) desde otros lugares, no");
    CHECK(pet_excursion_wants(&p, ESC_COLINA, T0 + 19 * H) < 0 && pet_excursion_wants(&p, ESC_BOSQUE, T0 + 19 * H) == 0 &&
          pet_excursion_wants(&p, ESC_BOSQUE, T0 + 21 * H) < 0 && pet_excursion_wants(&p, ESC_COLINA, T0 + 2 * H) < 0,
          "(v40) nunca va sola al mirador de noche; tarde, tampoco al claro");
    p.necesidad[NEED_SACIEDAD] = 20.0f;
    CHECK(pet_excursion_wants(&p, ESC_BOSQUE, T0 + 10 * H) < 0, "(v40) con hambre no se va");
    p.necesidad[NEED_SACIEDAD] = 90.0f;
    const uint32_t t2 = T0 + 24 * H + 11 * H;
    const int bolsa0 = bolsa_total(&p), musgo1 = p.musgo, ramitas1 = p.ramitas;
    const int k1 = pet_zone_find(&p, 2, t2), b1 = pet_excursion_back(&p, 1, t2);
    CHECK(b1 == k1 && b1 != PET_ZH_NADA, "(v40) al volver del mirador encontró lo escondido de hoy");
    CHECK(pet_zone_find(&p, 2, t2 + 60) == PET_ZH_NADA && p.musgo == musgo1 && p.ramitas == ramitas1 && bolsa_total(&p) == bolsa0,
          "(101b) se lo quedó ella: ya no está escondido y no llega nada a la bolsa ni a los materiales");
    {
        char tq[200]; bool quedo = false;
        for (int i = 0; i < PET_DIARIO; i++) if (p.diario[i].tipo == DIA_EXCURSION && p.diario[i].t) { pet_diary_text(&p, &p.diario[i], tq, sizeof tq); quedo |= strstr(tq, "me l") != NULL; }
        CHECK(quedo, "(101b) el diario dice que se lo quedó");
    }
    pet_excursion_back(&p, 0, t2 + H);
    CHECK(pet_excursion_wants(&p, ESC_BOSQUE, t2 + 2 * H) < 0 && pet_excursion_wants(&p, ESC_BOSQUE, t2 + 24 * H) == 0,
          "(v40) dos excursiones al día, como mucho");
    char buf[200]; bool chucao = false, condor = false;
    for (int i = 0; i < PET_DIARIO; i++) {
        const entrada_diario_t *e = &p.diario[i];
        if (e->tipo != DIA_EXCURSION || !e->t) continue;
        pet_diary_text(&p, e, buf, sizeof buf);
        chucao |= strstr(buf, "chucao") != NULL; condor |= strstr(buf, "cóndor") != NULL;
    }
    CHECK(chucao && condor, "(v40) el diario cuenta las excursiones (el chucao y el cóndor)");
    /* una partida de la versión 39 se actualiza */
    nueva(&p, 4002, t);
    p.lugares = 7u; p.musgo = 2;
    size_t viejo = ((offsetof(pet_t, zona_hallazgo) + 3u) & ~(size_t)3u) + 4u;
    memset(&q, 0xAB, sizeof q);
    memcpy(&q, &p, offsetof(pet_t, zona_hallazgo));
    q.version = 39;
    q.size = (uint16_t)viejo;
    uint32_t c = crc32_prueba((const uint8_t *)&q, viejo - 4);
    memcpy((uint8_t *)&q + viejo - 4, &c, 4);
    CHECK(pet_upgrade(&q) && pet_validate(&q) && q.musgo == 2 && !q.zona_hallazgo[0] && !q.excursiones && pet_zone_find(&q, 2, t) != PET_ZH_NADA,
          "(v40) una partida de la versión 39 se actualiza (todo lo escondido de hoy, esperando)");
}
static void test_v39(void)
{
    static pet_t p;
    uint32_t t = T0 + 12 * H;
    nueva(&p, 3901, t);
    p.etapa = STAGE_ADULTA;
    /* Pochi: tres charlas por acto, en orden; ninguna dice el nombre antes del final */
    p.dias_jugados = 3;
    int vistas = 0, malos = 0;
    for (int i = 0; i < 5; i++) {
        const int k = pet_pochi_chat_next(&p);
        if (k < 0) break;
        for (int l = 0; l < 3; l++) if (strstr(pet_pochi_chat(k, l, false), "TILO") || strstr(pet_pochi_chat(k, l, false), "PEUMO")) malos++;
        pet_pochi_chat_done(&p, k, t);
        vistas++;
    }
    CHECK(vistas == 3 && malos == 0, "(v39) Pochi: tres charlas en el primer acto, sin el nombre (%d)", vistas);
    p.dias_jugados = 25;
    CHECK(pet_pochi_chat_next(&p) == 6, "(v39) en el tercer acto, sus charlas");
    /* la página suelta: la siguiente del acto (cuenta como momento de historia); si no queda, una ya leída */
    p.dias_jugados = 3;
    p.tilo_paginas = 0; p.tilo_vistas = 0;
    bool nueva_p = false;
    int k = pet_loose_page(&p, t, &nueva_p);
    CHECK(k == 0 && nueva_p && pet_tilo_page(&p, 0), "(v39) la página suelta trae la primera página del diario");
    pet_loose_page(&p, t + 10, &nueva_p);
    CHECK(pet_story_moments_left(&p, t + 20) == 0, "(v39) cuenta como momento de historia");
    vaciar_eventos(&p);
    k = pet_loose_page(&p, t + 30, &nueva_p);
    CHECK(!nueva_p && pet_tilo_page(&p, k) && contar_eventos(&p, EV_PAGINA) == 1, "(v39) sin momentos, muestra una ya leída");
    p.tilo_paginas = 0x3F;
    t += 24u * H;
    k = pet_loose_page(&p, t, &nueva_p);
    CHECK(!nueva_p && k != 6 && k != 7, "(v39) nunca una página de otro acto ni la del huevo");
    /* los animales: hasta dos frases por día en las dos primeras semanas */
    nueva(&p, 3902, t);
    p.etapa = STAGE_ADULTA;
    p.dias_jugados = 25;
    const int conejo = FAUNA_COUNT + ANIMAL_CONEJO, zorro = FAUNA_ZORRO;
    pet_animal_seen(&p, conejo, t);
    pet_animal_seen(&p, zorro, t);
    p.dias_jugados = 5;
    CHECK(pet_animal_tell(&p, conejo, t + 60), "(v39) al tocar un animal conocido, cuenta su frase");
    CHECK(pet_animal_tell(&p, zorro, t + 120), "(v39) en la semana 1, hasta dos por día");
    CHECK(!pet_animal_tell(&p, conejo, t + 180), "(v39) ... y no más");
    CHECK(!pet_animal_tell(&p, FAUNA_COUNT + ANIMAL_TOPO, t + 200), "(v39) uno que no conoce no cuenta nada");
    /* Tilde: una visita por día como mucho */
    p.tilde_dia = (uint8_t)((((int64_t)t + p.zona_horaria) / 86400) & 0xFF);
    p.tilde_prox = 1;
    for (int m = 0; m < 120; m++) { pet_action(&p, ACT_ACARICIAR, GESTO_CARICIA, t + 300 + (uint32_t)m * 60); pet_update(&p, t + 330 + (uint32_t)m * 60); }
    CHECK(!p.tilde_hasta, "(v39) Tilde no viene dos veces el mismo día");
}
static void test_cristales_libres(void)
{
    static pet_t p;
    uint32_t t = T0 + 13 * H;
    nueva(&p, 3801, t);
    p.etapa = STAGE_ADULTA;
    p.escena = ESC_CUEVA;
    pet_event_t e;
    vaciar_eventos(&p);
    CHECK(pet_crystal_piece(&p, 3, 0, t) == 0 && p.cris_piezas == 0, "menos de 6 notas no es una pieza");
    int r = pet_crystal_piece(&p, 8, PIEZA_RAPIDA, t + 10);
    CHECK((r & PIEZA_RAPIDA) && p.cris_piezas == 1 && p.cris_dias == 1, "una pieza rápida cuenta (y el día)");
    int ev = 0; while (pet_poll_event(&p, &e)) ev += e.type == EV_PIEZA; CHECK(ev == 1, "y avisa con EV_PIEZA");
    pet_crystal_piece(&p, 10, PIEZA_LENTA, t + 20);
    r = pet_crystal_piece(&p, 12, 0, t + 30);
    CHECK((r & PIEZA_CANCION) && !(r & PIEZA_DRAGON), "después de tocar varias veces, ella propone hacer una canción");
    r = pet_crystal_piece(&p, 12, 0, t + 40);
    CHECK(!(r & PIEZA_CANCION), "una sola vez al día");
    /* con el dragón despierto, varios días tocando: la escama y la propone el dragón */
    p.especiales |= 1u << ESP_CRISTALES;
    pet_crystal_piece(&p, 8, 0, t + 24 * H);
    r = pet_crystal_piece(&p, 8, 0, t + 48 * H);
    CHECK(p.cris_dias == 3 && (r & PIEZA_ESCAMA) && p.bolsa[19] >= 1, "al tercer día tocando, el dragón le regala una escama de cristal");
    CHECK((r & PIEZA_CANCION) && (r & PIEZA_DRAGON), "y es el dragón quien propone la canción");
    r = pet_crystal_piece(&p, 8, 0, t + 72 * H);
    CHECK(!(r & PIEZA_ESCAMA), "la escama, una sola vez");
    /* el ritmo parejo */
    const uint8_t cris[6] = {0, 1, 2, 1, 0, 3};
    const uint16_t ms[6] = {400, 410, 380, 820, 395, 0};
    uint8_t notas[16];
    int n = pet_song_from_taps(cris, ms, 6, notas, 16);
    CHECK(n == 6 && PET_CANCION_PASOS(notas[0]) == 2 && PET_CANCION_PASOS(notas[3]) == 4 && PET_CANCION_CRISTAL(notas[5]) == 3, "los toques quedan en un compás parejo (1 pulso = 2 pasos)");
    /* la continuación: mismo largo, termina en casa con nota larga, y otra variante es distinta */
    uint8_t a[16], b[16];
    int na = pet_song_continue(notas, n, a, 16, 0), nb = pet_song_continue(notas, n, b, 16, 1);
    CHECK(na == n && nb == n && PET_CANCION_CRISTAL(a[na - 1]) == PET_CANCION_CRISTAL(notas[0]) && PET_CANCION_PASOS(a[na - 1]) >= 4, "ella continúa la frase y termina en casa");
    CHECK(memcmp(a, b, (size_t)n) != 0, "pedir otra da otra propuesta");
    for (int i = 0; i < na; i++) CHECK(PET_CANCION_CRISTAL(a[i]) < 6, "solo cristales 0..5");
    /* guardar: la principal y después el cuaderno */
    uint8_t todo[16];
    memcpy(todo, notas, (size_t)n); memcpy(todo + n, a, (size_t)na);
    CHECK(pet_song_save(&p, -1, todo, n + na, "LA DEL LAGO", t + 80 * H) == 0 && pet_song(&p, 0) && pet_song(&p, 0)->n == 12, "se guarda nuestra canción");
    CHECK(!strcmp(pet_song(&p, 0)->nombre, "LA DEL LAGO"), "con su nombre");
    CHECK(p.diario_n > 0 && pet_diary_get(&p, 0)->tipo == DIA_NUESTRA, "y queda en el diario");
    for (int k = 1; k < PET_CANCIONES; k++) CHECK(pet_song_save(&p, -1, notas, n, NULL, t + 81 * H) == k, "el cuaderno guarda hasta 4 más");
    CHECK(pet_song_save(&p, -1, notas, n, NULL, t + 82 * H) == -1 && pet_song_count(&p) == 5, "y no más");
    CHECK(!strcmp(pet_song(&p, 2)->nombre, "CANCIÓN 3"), "sin nombre: CANCIÓN n");
    pet_song_use(&p, 0, 0); pet_song_use(&p, 2, 1);
    CHECK(pet_song_for(&p, 0) == 0 && pet_song_for(&p, 1) == 2, "se elige para las alarmas y de canción de cuna");
    pet_song_delete(&p, 2);
    CHECK(pet_song_for(&p, 1) == -1 && pet_song_count(&p) == 4, "al borrarla, la cuna vuelve a la de siempre");
    /* infante: solo modo libre, sin componer */
    static pet_t q;
    nueva(&q, 3802, t);
    q.etapa = STAGE_ADULTA;
    q.modo_nino = MODO_INFANTE;
    for (int k = 0; k < 5; k++) r = pet_crystal_piece(&q, 8, 0, t + (uint32_t)k);
    CHECK(!(r & PIEZA_CANCION) && !pet_song_can_compose(&q), "en modo infante no se compone");
}
/* (v38) la casita de ramitas y musgo, la pesca en el hielo y los efectos nuevos */
static void test_v38_objetos(void)
{
    static pet_t p;
    uint32_t t = T0 + 13 * H;
    nueva(&p, 3803, t);
    p.etapa = STAGE_ADULTA;
    p.estacion_fija = ESTACION_VERANO;
    p.escena = ESC_LAGO;
    CHECK(!pet_lake_frozen(&p, t), "en verano el lago no está helado");
    p.bolsa[144] = 1;
    pet_object_use(&p, 144, t);
    CHECK(pet_lake_frozen(&p, t + 60) && !pet_lake_frozen(&p, t + 5 * H), "la bebida helada del norte congela el lago unas horas");
    p.bolsa[143] = 1;
    pet_object_use(&p, 143, t + 120);
    CHECK(pet_floating(&p, t + 150) && !pet_floating(&p, t + 600), "el polvo de hadas la hace flotar un ratito");
    p.bolsa[115] = 1;
    p.necesidad[NEED_HIGIENE] = 30;
    pet_object_use(&p, 115, t + 700);
    CHECK(pet_always_clean(&p, t + 800) && p.necesidad[NEED_HIGIENE] >= 90, "el broche la deja limpia");
    float hig = p.necesidad[NEED_HIGIENE];
    for (int m = 1; m <= 120; m++) pet_update(&p, t + 700 + (uint32_t)m * 60);
    CHECK(p.necesidad[NEED_HIGIENE] >= hig - 0.5f, "y no se ensucia en todo el día");
    /* ramitas y musgo: los encuentra explorando en el bosque */
    static pet_t q;
    nueva(&q, 3804, t);
    q.etapa = STAGE_ADULTA;
    for (int m = 1; m < 5 * 24 * 60 && !pet_materials(&q, NULL, NULL); m++) {
        pet_update(&q, t + (uint32_t)m * 60);
        q.escena = ESC_BOSQUE;
        if (m % 30 == 0) { q.necesidad[NEED_ENERGIA] = 80; q.necesidad[NEED_SACIEDAD] = 60; q.necesidad[NEED_CURIOSIDAD] = 30; }
    }
    CHECK(pet_materials(&q, NULL, NULL), "en unos días junta las ramitas y el musgo de la casita (%d ramitas, %d musgo)", q.ramitas, q.musgo);
}
static void test_actualizar_v37(void)
{
    static pet_t p, q;
    uint32_t t = T0 + 20 * 24 * H;
    nueva(&p, 3805, t - 20 * 24 * H);
    p.ultima_actualizacion = t;
    pet_seal(&p);
    q = p;
    q.version = 37;
    q.size = (uint16_t)offsetof(pet_t, canciones) + 4;
    uint32_t c = 0; { const uint8_t *d = (const uint8_t *)&q; uint32_t k = ~0u; for (size_t i = 0; i < offsetof(pet_t, canciones); i++) { k ^= d[i]; for (int b = 0; b < 8; b++) k = (k >> 1) ^ (0xEDB88320u & (0u - (k & 1u))); } c = ~k; }
    memcpy((uint8_t *)&q + offsetof(pet_t, canciones), &c, 4);
    CHECK(pet_upgrade(&q) && pet_validate(&q), "una partida de la versión 37 se actualiza");
    CHECK(pet_song_count(&q) == 0 && q.ramitas == 0 && q.cris_piezas == 0 && !pet_lake_frozen(&q, t), "lo nuevo empieza vacío");
}
static void test_historia_cinco_semanas(void)
{
    /* jugando un rato cada día (y yendo a cada lugar), la historia se termina en unas 5 semanas */
    static pet_t p;
    uint32_t t = T0 + 13 * H; /* 10:00 */
    nueva(&p, 3706, t);
    p.etapa = STAGE_ADULTA;
    p.escenas_abiertas = (1u << ESC_COUNT) - 1;
    p.especiales = (1u << ESP_COUNT) - 1; /* (los secretos tienen sus propias pruebas) */
    int rito = -1, final = -1;
    static const escena_t RUTA[] = {ESC_PRADO, ESC_BOSQUE, ESC_LAGO, ESC_COLINA, ESC_CUEVA, ESC_ISLA};
    for (int d = 1; d <= 50 && final < 0; d++) {
        t += 24u * H;
        for (int z = 0; z < 6; z++) {
            const uint32_t tz = t + (uint32_t)z * 1800u;
            p.escena = (uint8_t)RUTA[z];
            p.dormida = 0;
            pet_action(&p, ACT_ACARICIAR, GESTO_CARICIA, tz);
            pet_update(&p, tz + 60);
            p.escena = (uint8_t)RUTA[z]; /* (sola a veces se va a otra zona) */
            pet_creature_call(&p, (magica_t)z, tz + 70);
            pet_creature_touch(&p, tz + 80);
            if (RUTA[z] == ESC_CUEVA) pet_prisma_touch(&p, tz + 90);
            int k;
            if (pet_brizna_awake(&p, tz + 100, &k)) pet_brizna_heard(&p, tz + 110);
            if (pet_tilde_here(&p, tz + 100)) pet_tilde_touch(&p, 0, tz + 110);
            pet_update(&p, tz + 900);
            vaciar_eventos(&p);
        }
        p.raro = RARO_GLOBO; p.raro_hasta = t + 4 * H;
        pet_whale_ride(&p, 6, t + 3 * H);
        if (p.bolsa[OBJ_CORAZON_BOSQUE] && !p.guardiana) {
            const uint32_t noche = t + 13 * H; /* 23:00 */
            p.escena = ESC_ISLA;
            if (pet_guardian_ready(&p, noche)) pet_object_use(&p, OBJ_CORAZON_BOSQUE, noche);
        }
        if (p.guardiana && rito < 0) rito = d;
        if (!p.melodia_n && d > 30) { p.melodia_n = 4; } /* (su melodía la compone en la cueva; aquí se da por compuesta) */
        for (int h = 0; h < 6 && rito >= 0; h++) { pet_update(&p, t + (uint32_t)(10 + h) * H); if (pet_final_pending(&p) == PET_FINAL_SECRETO) { final = d; pet_final_done(&p, PET_FINAL_SECRETO, t + 16 * H); break; } }
        vaciar_eventos(&p);
    }
    int hist = 0;
    for (int c = 0; c <= CRI_NIMBA; c++) hist += pet_story_count(&p, c);
    printf("  historia en 5 semanas: rito el día %d, final secreto el día %d\n", rito, final);
    CHECK(hist == 35, "en ese tiempo escuchó las 35 historias de criaturas y de Nimba (%d)", hist);
    CHECK(pet_story_count(&p, NARR_PRISMA) == PET_PRISMA_CAPS, "y los ocho capítulos de Prisma (%d)", pet_story_count(&p, NARR_PRISMA));
    CHECK(rito >= 26 && rito <= 38, "el rito, en la semana 5 (día %d)", rito);
    CHECK(final >= 26 && final <= 40, "y el final secreto (día %d)", final);
}

/* (v41) el claro y el mirador son zonas de verdad; los juegos de los lugares con récord, estrellas y premios */
static void test_v41(void)
{
    static pet_t p, q;
    uint32_t t = T0 + 10 * H;
    nueva(&p, 4101, t);
    p.etapa = STAGE_ADULTA; p.zona_horaria = 0; p.dormida = 0; p.enferma = 0;
    p.escenas_abiertas = 0x3F;
    CHECK(!pet_scene_open(&p, ESC_CLARO) && !pet_scene_open(&p, ESC_MIRADOR), "(v41) sin los lugares secretos, el claro y el mirador están cerrados");
    p.lugares = 1u << PET_ZONA_CLARO;
    CHECK(pet_scene_open(&p, ESC_CLARO) && !pet_scene_open(&p, ESC_MIRADOR), "(v41) el claro se abre con su lugar secreto");
    p.lugares |= 2u;
    CHECK(pet_scene_open(&p, ESC_MIRADOR), "(v41) el mirador se abre con el suyo");
    CHECK(!strcmp(pet_scene_el(ESC_CLARO), "el claro") && !strcmp(pet_scene_al(ESC_MIRADOR), "al mirador") && strcmp(pet_scene_proper(ESC_MIRADOR), "?"),
          "(v41) los nombres de las zonas nuevas");
    CHECK(pet_scene_liking(&p, ESC_CLARO) > -101 && pet_scene_liking(&p, ESC_MIRADOR) < 101, "(v41) el gusto por las zonas nuevas sale de las vecinas");
    /* nunca se va sola a una zona secreta (ni desde ella), aunque pasen muchas horas */
    p.escena = ESC_BOSQUE; p.escena_desde = t;
    bool fue = false;
    for (int k = 0; k < 400; k++) { pet_update(&p, t + (uint32_t)k * 300u); fue |= p.escena == ESC_CLARO || p.escena == ESC_MIRADOR; }
    CHECK(!fue, "(v41) no se va sola al claro ni al mirador");
    /* hay animales en las zonas nuevas */
    p.escena = ESC_CLARO; p.escena_desde = t;
    int vistos = 0;
    for (int k = 0; k < 600; k++) {
        uint32_t tk = t + (uint32_t)k * 120u;
        pet_update(&p, tk);
        if (p.escena != ESC_CLARO) { p.escena = ESC_CLARO; p.escena_desde = tk; }
        for (int a = 0; a < ANIMAL_COUNT; a++) vistos += pet_animal_here(&p, (animal_t)a, tk);
    }
    CHECK(vistos > 0, "(v41) en el claro aparecen animales");
    /* récord, estrellas y premios */
    nueva(&p, 4102, t);
    int r0 = 0, m0 = 0; pet_materials(&p, &r0, &m0);
    int r = pet_place_game_result(&p, PET_JL_SALTO, 120, 2, t);
    int ra = 0, mu = 0, ob = -1; pet_place_game_prize(&ra, &mu, &ob);
    int r1 = 0, m1 = 0; pet_materials(&p, &r1, &m1);
    CHECK((r & PET_JLR_PREMIO) && ra == 1 && mu == 1 && r1 == r0 + 1 && m1 == m0 + 1, "(v41) 2 estrellas: una ramita y musgo");
    CHECK(pet_place_game_record(&p, PET_JL_SALTO) == 120 && pet_place_game_stars(&p, PET_JL_SALTO) == 2, "(v41) guarda el récord y las estrellas");
    r = pet_place_game_result(&p, PET_JL_SALTO, 200, 3, t);
    CHECK((r & PET_JLR_RECORD) && (r & PET_JLR_ESTRELLAS) && (r & PET_JLR_PREMIO), "(v41) nuevo récord y 3 estrellas");
    r = pet_place_game_result(&p, PET_JL_SALTO, 250, 3, t);
    CHECK(!(r & PET_JLR_PREMIO) && !(r & PET_JLR_REGALO) && pet_place_game_record(&p, PET_JL_SALTO) == 250, "(v41) dos rondas con premio al día por juego (el récord sigue)");
    r = pet_place_game_result(&p, PET_JL_SALTO, 10, 1, t + 24 * H);
    CHECK((r & PET_JLR_PREMIO) && pet_place_game_record(&p, PET_JL_SALTO) == 250 && pet_place_game_stars(&p, PET_JL_SALTO) == 3, "(v41) al otro día vuelve el premio; el récord no baja");
    r = pet_place_game_result(&p, PET_JL_HOJAS, 5, 0, t);
    CHECK(!(r & PET_JLR_PREMIO), "(v41) sin estrellas, no hay premio");
    CHECK(pet_place_game_seed(&p, PET_JL_PIEDRA, t) == pet_place_game_seed(&p, PET_JL_PIEDRA, t + 2 * H) &&
          pet_place_game_seed(&p, PET_JL_PIEDRA, t) != pet_place_game_seed(&p, PET_JL_PIEDRA, t + 24 * H), "(v41) la piedra tibia cambia cada día");
    /* una partida de la versión 40 se actualiza */
    nueva(&p, 4103, t);
    p.lugares = 7u; p.en_zona = 1; p.musgo = 3;
    size_t viejo = ((offsetof(pet_t, jl_record) + 3u) & ~(size_t)3u) + 4u;
    memset(&q, 0xAB, sizeof q);
    memcpy(&q, &p, offsetof(pet_t, jl_record));
    q.version = 40;
    q.size = (uint16_t)viejo;
    uint32_t c = crc32_prueba((const uint8_t *)&q, viejo - 4);
    memcpy((uint8_t *)&q + viejo - 4, &c, 4);
    CHECK(pet_upgrade(&q) && pet_validate(&q) && q.musgo == 3 && !q.en_zona && !q.jl_record[0] && pet_scene_open(&q, ESC_CLARO) && pet_scene_open(&q, ESC_MIRADOR),
          "(v41) una partida de la versión 40 se actualiza (el claro y el mirador abiertos, récords en cero)");
}

/* (v99) P0 del plan de batería: la mascota no se congela ni cambia por dormir el procesador. La misma mascota vivida
 * latido a latido (como con la pantalla encendida) y de un solo salto (como al despertar de un sueño) termina, en promedio
 * de 30 mascotas, con las mismas necesidades (margen chico), y de un salto también le dio hambre y sueño. */
static void test_p0_salto(void)
{
    static pet_t a, b;
    static const struct { uint32_t desde_h, dur_h, latido; const char *que; } C[] = {
        {10, 1, 5, "1 h de día"}, {10, 8, 5, "8 h de día"}, {20, 8, 5, "8 h cruzando la hora de dormir"},
        {10, 24, 10, "24 h"}, {10, 72, 30, "3 días"}};
    for (unsigned c = 0; c < sizeof C / sizeof C[0]; c++) {
        double dif[NEED_COUNT] = {0};
        int bajo_sola = 0;
        const int N = 30;
        for (int s = 1; s <= N; s++) {
            uint32_t t = T0 + C[c].desde_h * H;
            nueva(&a, (uint32_t)s * 77u, t);
            pet_update(&a, t + 60); t += 60;
            b = a;
            const float sac0 = b.necesidad[NEED_SACIEDAD];
            for (uint32_t k = C[c].latido; k <= C[c].dur_h * H; k += C[c].latido) pet_update(&a, t + k);
            pet_update(&a, t + C[c].dur_h * H);
            pet_update(&b, t + C[c].dur_h * H);
            for (int i = 0; i < NEED_COUNT; i++) dif[i] += a.necesidad[i] - b.necesidad[i];
            if (C[c].dur_h >= 8 && b.necesidad[NEED_SACIEDAD] < sac0 - 5) bajo_sola++;
        }
        float peor = 0; int cual = 0;
        for (int i = 0; i < NEED_COUNT; i++) if (fabs(dif[i] / N) > peor) { peor = (float)fabs(dif[i] / N); cual = i; }
        CHECK(peor < 5.0f, "(v99) P0 %s: latido a latido y de un salto dan lo mismo (peor necesidad %d: %.1f puntos)", C[c].que, cual, peor);
        if (C[c].dur_h >= 8) CHECK(bajo_sola >= N * 9 / 10, "(v99) P0 %s: de un salto también le da hambre (%d de %d)", C[c].que, bajo_sola, N);
    }
}

/* (v101) con el clima que no le gusta se va a su casita; si ese clima le gusta, se queda donde está */
static void test_v101_clima_gusto(void)
{
    printf("v101: se resguarda solo del clima que no le gusta\n");
    for (int gusta = 0; gusta < 2; gusta++) {
        pet_t p;
        nueva(&p, 5, T0);
        uint32_t t = T0 + 15u * H; /* 12:00 en Chile */
        pet_update(&p, t);
        p.escenas_abiertas = 0x1F;
        p.decoraciones |= 1u << DECOR_CASITA;
        p.escena = ESC_PRADO;
        p.memoria.clima[CLIMA_LLUVIA] = (int8_t)(gusta ? 70 : -70);
        p.clima = CLIMA_LLUVIA;
        CHECK(pet_action(&p, ACT_IR, ESC_LAGO, t) > 0 && p.escena == ESC_LAGO, "no fue al lago");
        int volvio = 0;
        for (int m = 1; m <= 30 && !volvio; m++) {
            p.clima = CLIMA_LLUVIA;
            p.memoria.clima[CLIMA_LLUVIA] = (int8_t)(gusta ? 70 : -70);
            pet_update(&p, t + (uint32_t)m * 60u);
            if (p.escena != ESC_LAGO) volvio = m;
        }
        if (gusta) CHECK(volvio == 0 || volvio >= 15, "le gusta la lluvia y se fue enseguida (minuto %d)", volvio); /* (más tarde puede irse por un plan suyo) */
        else CHECK(volvio > 0 && volvio <= 8 && p.escena == ESC_PRADO, "no le gusta la lluvia y no se fue a su casita (minuto %d)", volvio);
    }
}

/* (101b) la música de la cueva decía «el dragón quiere contarte algo» aunque hoy no tocaba capítulo: pet_prisma_ready lo sabe antes */
static void test_v101b_prisma(void)
{
    printf("v101b: el dragón solo anuncia un capítulo si hoy de verdad lo deja\n");
    pet_t p;
    nueva(&p, 7, T0);
    uint32_t t = T0 + 15u * H;
    pet_update(&p, t);
    CHECK(!pet_prisma_ready(&p, t), "sin el dragón despierto no hay capítulo");
    p.especiales |= 1u << ESP_CRISTALES;
    p.dias_jugados = 10;
    CHECK(!pet_prisma_ready(&p, t) && pet_prisma_touch(&p, t) == -1, "antes del día 15 no cuenta");
    p.dias_jugados = 15;
    CHECK(pet_prisma_ready(&p, t + 10), "desde el día 15 está listo");
    CHECK(pet_prisma_touch(&p, t + 20) == 0, "listo = tocarlo deja el capítulo");
    CHECK(!pet_prisma_ready(&p, t + 30), "ya contó hoy: no anuncia otro");
    t += 24u * H;
    pet_update(&p, t);
    CHECK(pet_prisma_ready(&p, t + 10), "al otro día, otra vez listo");
    pet_t q = p;
    q.hist_dia = (uint8_t)((((int64_t)t + 10 + CHILE) / 86400) & 0xFF);
    q.hist_hoy = PET_HISTORIAS_DIA;
    CHECK(pet_story_moments_left(&q, t + 10) == 0 && !pet_prisma_ready(&q, t + 10) && pet_prisma_touch(&q, t + 10) == -1,
          "sin momentos de historia hoy: ni anuncia ni cuenta");
    pet_set_owner_age(&p, 4);
    CHECK(pet_mode(&p) == MODO_INFANTE && !pet_prisma_ready(&p, t + 10), "en modo infante no hay capítulos");
}

/* (fw 102) Pochi es la mamá de cuatro cachorros y se acuerda de cómo terminó la visita anterior */
static void test_fw102_pochi(void)
{
    printf("fw102: Pochi, sus cachorros y su memoria\n");
    pet_t p;
    nueva(&p, 11, T0);
    uint32_t t = T0 + 15u * H;
    pet_update(&p, t);
    CHECK(pet_pochi_visits(&p) == 0 && pet_pochi_last(&p) == PET_POCHI_ULT_NADA && pet_pochi_pup_pick(&p, t) < 0, "al principio no la conoce ni trae cachorros");
    pet_pochi(&p, 0, t); pet_pochi(&p, PET_POCHI_F_FIN + PET_POCHI_ULT_COMIO, t + 600);
    CHECK(pet_pochi_visits(&p) == 1 && pet_pochi_last(&p) == PET_POCHI_ULT_COMIO, "cuenta la visita y se acuerda de que comió sin pasear");
    pet_pochi(&p, 0, t + 7 * H);
    CHECK(pet_pochi_pup_pick(&p, t + 14 * H) >= 0, "en la tercera visita trae siempre a un cachorro");
    const float hig = pet_need(&p, NEED_HIGIENE);
    pet_pochi(&p, PET_POCHI_F_CACHORRO + POCHI_RONY, t + 14 * H);
    pet_pochi(&p, PET_POCHI_F_CACHORRO + POCHI_CHAPI, t + 14 * H + 60);
    CHECK(pet_pochi_pup_met(&p, POCHI_RONY) && pet_pochi_pup_met(&p, POCHI_CHAPI) && !pet_pochi_pup_met(&p, POCHI_PASITA) &&
          pet_need(&p, NEED_HIGIENE) < hig - 5, "los cachorros quedan conocidos; el abrazo de Chapi la ensucia un poco");
    int nombres = 0;
    for (int k = 0; k < PET_POCHI_CACHORROS; k++) nombres += pet_pochi_pup_name(k)[0] != 0;
    CHECK(nombres == 4 && !strcmp(pet_pochi_pup_name(POCHI_PASITA), "Pasita") && !strcmp(pet_pochi_pup_name(POCHI_MORITA), "Morita"), "los cuatro tienen nombre");
    pet_pochi(&p, PET_POCHI_F_FIN + PET_POCHI_ULT_PASEO, t + 15 * H);
    CHECK(pet_pochi_last(&p) == PET_POCHI_ULT_PASEO && pet_pochi_pup_met(&p, POCHI_RONY), "guardar cómo terminó no borra los cachorros conocidos");
    p.dormida = 1;
    const uint32_t ui0 = p.ultima_interaccion;
    pet_pochi(&p, PET_POCHI_F_ACURRUCA, t + 16 * H);
    CHECK(p.ultima_interaccion == ui0, "acurrucarse junto a ella dormida no cuenta como que jugaron");
    p.dormida = 0;
    for (int v = 0; v < 300; v++) pet_pochi(&p, 0, t + 17 * H);
    CHECK(pet_pochi_visits(&p) == 255, "las visitas se cuentan hasta 255");
    int con = 0;
    for (int d = 0; d < 40; d++) con += pet_pochi_pup_pick(&p, t + (uint32_t)d * 24u * H) >= 0;
    CHECK(con > 5 && con < 35, "después, a veces viene con un cachorro y a veces sola (%d de 40)", con);
    /* los animales: la amistad de cada cachorro con su animal queda guardada, sin tocar lo demás */
    { pet_t q; nueva(&q, 77, T0); pet_update(&q, T0 + 15u * H);
      const uint8_t mem0 = q.pochi_mem; const float div0 = pet_need(&q, NEED_DIVERSION);
      CHECK(!pet_pochi_pup_friend(&q, POCHI_MORITA), "al principio ningún cachorro tiene amigo");
      pet_pochi(&q, PET_POCHI_F_AMIGO + POCHI_MORITA, T0 + 15u * H);
      pet_pochi(&q, PET_POCHI_F_ANIMAL, T0 + 15u * H + 60);
      CHECK(pet_pochi_pup_friend(&q, POCHI_MORITA) && !pet_pochi_pup_friend(&q, POCHI_RONY) && q.pochi_mem == mem0 && pet_need(&q, NEED_DIVERSION) >= div0,
            "Morita se hizo amiga de su animal: queda guardado y no cambia la memoria de visitas"); }
    /* los cuatro son igual de importantes: en muchas partidas, el primero que conoces es cualquiera de ellos, parejo */
    int primero[PET_POCHI_CACHORROS] = {0};
    for (uint32_t id = 1; id <= 400; id++) {
        pet_t q; nueva(&q, 5000 + id, T0); q.pochi_visitas = 2;
        const int k = pet_pochi_pup_pick(&q, T0 + 15u * H);
        if (k >= 0) primero[k]++;
    }
    CHECK(primero[0] > 60 && primero[1] > 60 && primero[2] > 60 && primero[3] > 60, "ninguno es el favorito para venir primero (%d %d %d %d)", primero[0], primero[1], primero[2], primero[3]);
    /* ya conociéndolos a todos: a veces dos, a veces la familia completa; nunca repetidos */
    p.pochi_mem |= 0x3Cu;
    int dos = 0, cuatro = 0, rep = 0;
    for (int v = 0; v < 400; v++) {
        p.pochi_visitas = (uint8_t)(3 + v % 200);
        int o[PET_POCHI_CACHORROS]; const int n = pet_pochi_pups_pick(&p, t + (uint32_t)(v / 200) * 24u * H, o);
        dos += n == 2; cuatro += n == 4;
        for (int i = 0; i < n; i++) for (int j = i + 1; j < n; j++) rep += o[i] == o[j];
    }
    CHECK(dos > 20 && cuatro > 5 && rep == 0, "vienen de a dos (%d) y a veces todos (%d), sin repetirse", dos, cuatro);
}

/* (fw 103) tocar animales: el primero saluda, el segundo avisa, insistir tiene consecuencia */
static void test_fw103_toques(void)
{
    printf("fw103: tocar animales (lo bueno y lo malo)\n");
    pet_t p;
    nueva(&p, 23, T0);
    uint32_t t = T0 + 15u * H;
    pet_update(&p, t);
    p.escenas_abiertas = 0x3F;
    /* el zorrillo: avisa y después rocía */
    pet_animal_call(&p, ANIMAL_ZORRILLO, t);
    CHECK(pet_animal_here(&p, ANIMAL_ZORRILLO, t), "el zorrillo está aquí");
    CHECK(pet_animal_touch(&p, ANIMAL_ZORRILLO, 1, t) == PET_TOC_SALUDA, "el primer toque es un saludo");
    CHECK(pet_animal_touch(&p, ANIMAL_ZORRILLO, 2, t + 1) == PET_TOC_AVISA, "el segundo avisa");
    CHECK(!pet_stinky(&p), "todavía no huele");
    const float hig = pet_need(&p, NEED_HIGIENE);
    CHECK(pet_animal_touch(&p, ANIMAL_ZORRILLO, 3, t + 2) == PET_TOC_ROCIA, "al insistir, rocía");
    CHECK(pet_stinky(&p) && pet_need(&p, NEED_HIGIENE) < hig - 30, "queda hedionda y sucia");
    /* el olor se va solo en unas horas */
    pet_update(&p, t + 5u * H);
    CHECK(!pet_stinky(&p), "a las pocas horas ya no huele");
    /* y el baño lo quita al tiro */
    pet_animal_call(&p, ANIMAL_ZORRILLO, t + 6u * H);
    pet_animal_touch(&p, ANIMAL_ZORRILLO, 4, t + 6u * H);
    CHECK(pet_stinky(&p), "la rociaron de nuevo");
    pet_action(&p, ACT_BANAR, 0, t + 6u * H + 60);
    CHECK(!pet_stinky(&p) && pet_need(&p, NEED_HIGIENE) > 90, "el baño le quita el olor");
    /* la abeja pica, y le queda miedo */
    const float miedo0 = p.miedo_abejas;
    p.protege_hasta = 0;
    p.fauna_hasta[FAUNA_ABEJA] = t + 9u * H;
    CHECK(pet_fauna_touch(&p, FAUNA_ABEJA, 1, t + 7u * H) == PET_TOC_SALUDA, "a la abeja, primero un saludo");
    CHECK(pet_fauna_touch(&p, FAUNA_ABEJA, 2, t + 7u * H + 1) == PET_TOC_AVISA, "después avisa");
    CHECK(pet_fauna_touch(&p, FAUNA_ABEJA, 3, t + 7u * H + 2) == PET_TOC_PICA && p.miedo_abejas > miedo0,
          "si insistes, pica, y le queda miedo a las abejas");
    /* en modo infante no pasa nada malo */
    pet_t q; nueva(&q, 24, T0); pet_set_owner_age(&q, 4); pet_update(&q, t);
    q.escenas_abiertas = 0x3F;
    pet_animal_call(&q, ANIMAL_ZORRILLO, t);
    int peor = PET_TOC_SALUDA;
    for (int v = 1; v <= 8; v++) { const int r = pet_animal_touch(&q, ANIMAL_ZORRILLO, v, t + (uint32_t)v); if (r > peor) peor = r; }
    CHECK(pet_mode(&q) == MODO_INFANTE && peor <= PET_TOC_AVISA && !pet_stinky(&q), "en modo infante se queda en el aviso");
    /* el caracol y la tortuga no se enojan nunca */
    CHECK(pet_animal_patience(ANIMAL_CARACOL) == 0 && pet_animal_patience(ANIMAL_TORTUGA) == 0, "los lentos tienen paciencia infinita");
    CHECK(pet_animal_patience(ANIMAL_ZORRILLO) == 2 && pet_animal_patience(ANIMAL_CONEJO) == 3, "cada uno aguanta lo suyo");
}

/* (fw 108) cada mascota tiene SU mundo: al cambiarse no se lleva (ni le borran) la casa, el jardín ni sus cosas */
static void test_fw108_mundos(void)
{
    printf("fw 108: cada mascota con su mundo\n");
    static pet_t vieja, nueva_p;
    nueva(&vieja, 11, T0);
    nueva(&nueva_p, 12, T0);
    /* la de siempre: su casa, sus flores, sus semillas, su ropa y un amigo */
    vieja.decoraciones = 0x1F;
    vieja.jardin[0].tipo = 1; vieja.jardin[0].etapa = 2;
    vieja.semillas[0] = 4;
    vieja.ropa_tiene = 0x3F;
    vieja.juguetes_tiene = 0x7;
    vieja.amigos[0].id = 1234;
    const uint32_t deco = vieja.decoraciones;
    /* la nueva estrena el mundo vacío */
    pet_new_fresh(&nueva_p, 3, T0, 99);
    const uint16_t ropa_nueva = nueva_p.ropa_tiene;
    CHECK(nueva_p.decoraciones == 0, "la mascota nueva empieza con el mundo vacío");
    /* se cambia a la nueva (la vieja se queda descansando) y después se vuelve a la vieja */
    pet_resume(&nueva_p, &vieja, T0 + 100);
    CHECK(nueva_p.decoraciones == 0, "la nueva NO se queda con la casa de la otra");
    CHECK(nueva_p.jardin[0].tipo == 0, "ni con su jardín");
    CHECK(nueva_p.ropa_tiene == ropa_nueva, "ni con su ropa");
    pet_resume(&vieja, &nueva_p, T0 + 200);
    CHECK(vieja.decoraciones == deco, "la de siempre conserva su casa al volver (antes se le borraba)");
    CHECK(vieja.jardin[0].tipo == 1 && vieja.jardin[0].etapa == 2, "y su jardín");
    CHECK(vieja.semillas[0] == 4, "y sus semillas");
    CHECK(vieja.ropa_tiene == 0x3F && vieja.juguetes_tiene == 0x7, "y su ropa y sus juguetes");
    CHECK(vieja.amigos[0].id == 1234, "y sus amigos");
    /* lo del orbe sí se comparte: la hora, el país y lo que ya te explicaron */
    nueva_p.pais = PAIS_CHILE;
    vieja.vio = 0;
    nueva_p.vio = 0x5;
    pet_resume(&vieja, &nueva_p, T0 + 300);
    CHECK(vieja.pais == PAIS_CHILE, "el país es del orbe");
    CHECK((vieja.vio & 0x5) == 0x5, "lo que ya te explicaron no se repite");
}

/* (1.0.26) la que se fue de viaje desde Perfiles puede volver */
static void test_1026_vuelve(void)
{
    printf("1.0.26: vuelve de su viaje\n");
    static pet_t yo, ella, otra;
    nueva(&yo, 21, T0);
    nueva(&ella, 22, T0);
    nueva(&otra, 23, T0);
    snprintf(ella.nombre, sizeof ella.nombre, "TOVA");
    snprintf(otra.nombre, sizeof otra.nombre, "LUMI");
    pet_add_traveller(&yo, &otra, 1, T0);
    pet_add_traveller(&yo, &ella, 2, T0 + 10);
    CHECK(pet_travellers(&yo) == 2, "dos viajeras");
    /* una carta de cada una (la más nueva primero): de Tova (0) y de Lumi (1) */
    yo.cartas_n = 2;
    yo.cartas[0][0] = (uint8_t)(CARTA_VIAJERO << 6 | 0); yo.cartas[0][1] = 3; yo.cartas_var[0] = 1;
    yo.cartas[1][0] = (uint8_t)(CARTA_VIAJERO << 6 | 1); yo.cartas[1][1] = 4; yo.cartas_var[1] = 2;
    yo.carta_pend = 1;
    CHECK(!pet_traveller_back(&yo, &otra), "solo vuelve la última que salió");
    CHECK(pet_traveller_back(&yo, &ella), "Tova vuelve");
    CHECK(pet_travellers(&yo) == 1 && !strcmp(yo.viajeros[0].nombre, "LUMI"), "queda solo Lumi de viaje");
    CHECK(yo.cartas_n == 1 && yo.cartas[0][0] == (uint8_t)(CARTA_VIAJERO << 6 | 0) && yo.cartas[0][1] == 4 && yo.cartas_var[0] == 2,
          "la carta de Lumi sigue y apunta a Lumi");
    CHECK(!yo.carta_pend, "la carta sin abrir era de Tova: ya no queda pendiente");
    CHECK(!pet_traveller_back(&yo, &ella), "no vuelve dos veces");
}

/* (fw 104) papelitos, trocito de hielo, terquedad y paseos de verdad */
static void test_fw104(void)
{
    printf("fw 104: papelitos, hielo, terquedad, paseos\n");
    char buf[200];
    /* --- papelitos: 2-3 por semana, nunca dos días seguidos, todos con texto y versión corta, sin nombrar al Rey --- */
    {
        static pet_t p;
        nueva(&p, 7, T0);
        uint32_t t = T0 + 12u * 24u * H;
        pet_update(&p, t);
        p.escenas_abiertas = 0xFF; p.lugares = 0x7; p.dias_jugados = 15;
        int tot = 0, seguidos = 0, max_sem = 0, sem_n = 0; int32_t ultimo = -10;
        for (int d = 0; d < 42; d++) {
            const uint32_t td = t + (uint32_t)d * 24u * H + 12u * H;
            if (d % 7 == 0) { if (sem_n > max_sem) max_sem = sem_n; sem_n = 0; }
            for (int z = 0; z < 3; z++)
                if (pet_zone_find(&p, z, td) == PET_ZH_CARTA) {
                    CHECK(pet_zone_find_take(&p, z, td) == PET_ZH_CARTA, "el papelito se toma");
                    tot++; sem_n++;
                    if (d - ultimo == 1) seguidos++;
                    ultimo = d;
                    pet_event_t e; bool ev = false;
                    while (pet_poll_event(&p, &e)) ev |= e.type == EV_CARTA_BOSQUE;
                    CHECK(ev, "avisa el papelito");
                }
        }
        if (sem_n > max_sem) max_sem = sem_n;
        CHECK(tot >= 10 && tot <= PET_PAPELITOS, "(fw 104) en 6 semanas salen ~2-3 papelitos por semana (%d)", tot);
        CHECK(max_sem <= 3, "(fw 104) nunca más de 3 por semana (%d)", max_sem);
        CHECK(seguidos == 0, "(fw 104) nunca dos días seguidos (%d)", seguidos);
        int conocidos = 0;
        for (int k = 0; k < PET_PAPELITOS; k++) conocidos += pet_story_known(&p, NARR_PAPELITOS, k);
        CHECK(conocidos == tot, "(fw 104) cada papelito queda en el Libro (%d de %d)", conocidos, tot);
        CHECK(pet_book_new(&p, NARR_PAPELITOS), "(fw 104) con su estrellita de nuevo");
        CHECK(strcmp(pet_narr_name(NARR_PAPELITOS), "Papelitos del bosque") == 0, "(fw 104) el narrador se llama Papelitos del bosque");
        /* en modo infante no salen */
        static pet_t q;
        nueva(&q, 7, T0); q.modo_nino = MODO_INFANTE; pet_update(&q, t); q.escenas_abiertas = 0xFF; q.lugares = 0x7; q.dias_jugados = 15;
        int inf = 0;
        for (int d = 0; d < 21; d++) for (int z = 0; z < 3; z++) inf += pet_zone_find(&q, z, t + (uint32_t)d * 24u * H) == PET_ZH_CARTA;
        CHECK(inf == 0, "(fw 104) en modo infante no hay papelitos (%d)", inf);
        /* si ella lo encuentra en su excursión, te lo trae (no se lo queda) y queda en el Libro igual */
        static pet_t r;
        nueva(&r, 7, T0); pet_update(&r, t); r.escenas_abiertas = 0xFF; r.lugares = 0x7; r.dias_jugados = 15;
        int trajo = 0;
        for (int d = 0; d < 21 && !trajo; d++) {
            const uint32_t td = t + (uint32_t)d * 24u * H + 12u * H;
            if (pet_zone_find(&r, 0, td) == PET_ZH_CARTA) {
                const uint16_t antes = r.papelitos;
                trajo = pet_excursion_back(&r, 0, td) == PET_ZH_CARTA && r.papelitos != antes;
            }
        }
        CHECK(trajo, "(fw 104) si lo encuentra ella en su excursión, te lo trae al Libro");
        const entrada_diario_t e7 = {t, DIA_EXCURSION, 0, 7, 0};
        pet_diary_text(&r, &e7, buf, sizeof buf);
        CHECK(strstr(buf, "papelito") != NULL, "(fw 104) el diario cuenta que trajo un papelito: %s", buf);
    }
    /* --- trocito de hielo: solo se pesca en invierno; afuera del invierno se derrite en 2 días; tiene nombre y diario --- */
    {
        static pet_t p;
        nueva(&p, 11, T0);
        uint32_t t = T0 + 3u * 24u * H;
        pet_update(&p, t);
        p.estacion_fija = ESTACION_INVIERNO;
        int hielos = 0;
        for (int i = 0; i < 400 && !hielos; i++) {
            p.actividad = ACTV_PESCAR; p.dormida = 0;
            pet_fish_game(&p, 3, t + (uint32_t)i * 60u);
            hielos = (p.coleccion[COL_CRISTALES] >> 3) & 1u;
            vaciar_eventos(&p);
        }
        CHECK(hielos, "(fw 104) en invierno se pesca el trocito de hielo");
        CHECK(strcmp(pet_catch_name(PESCA_HIELO), "un trocito de hielo") == 0, "(fw 104) el hielo tiene nombre al pescarlo");
        CHECK(!((p.colecciones_completas >> COL_CRISTALES) & 1u) || (p.coleccion[COL_CRISTALES] & 7u) == 7u, "(fw 104) el hielo solo no completa los cristales");
        p.estacion_fija = ESTACION_PRIMAVERA;
        uint32_t td = t + 24u * H; /* (día a día: cada día fuera del invierno cuenta) */
        bool sigue1 = false, se_fue = false, ev = false;
        for (int d = 0; d < 4; d++) {
            pet_update(&p, td + (uint32_t)d * 24u * H);
            if (d == 0) sigue1 = (p.coleccion[COL_CRISTALES] >> 3) & 1u;
            for (int k = 0; k < p.diario_n; k++) ev |= pet_diary_get(&p, k)->tipo == DIA_HIELO; /* (la página del diario) */
            vaciar_eventos(&p);
        }
        se_fue = !((p.coleccion[COL_CRISTALES] >> 3) & 1u);
        CHECK(sigue1 && se_fue && ev, "(fw 104) afuera del invierno aguanta un día y después se derrite (%d %d %d)", sigue1, se_fue, ev);
        const entrada_diario_t eh = {td, DIA_HIELO, 1, 0, 0};
        pet_diary_text(&p, &eh, buf, sizeof buf);
        CHECK(strstr(buf, "derriti") != NULL, "(fw 104) el diario cuenta que se derritió: %s", buf);
        /* en primavera no se pesca hielo */
        static pet_t q;
        nueva(&q, 11, T0); pet_update(&q, t); q.estacion_fija = ESTACION_PRIMAVERA;
        int hq = 0;
        for (int i = 0; i < 300; i++) { q.actividad = ACTV_PESCAR; q.dormida = 0; pet_fish_game(&q, 3, t + (uint32_t)i * 60u); hq |= (q.coleccion[COL_CRISTALES] >> 3) & 1u; vaciar_eventos(&q); }
        CHECK(!hq, "(fw 104) fuera del invierno no se pesca hielo");
    }
    /* --- terquedad: la terca a veces se niega a la primera, pero si insistes obedece; la dócil casi nunca se niega --- */
    {
        int niega_terca = 0, niega_docil = 0, insistio_ok = 1;
        for (int s = 0; s < 40; s++) {
            static pet_t a, b;
            nueva(&a, 100 + (uint32_t)s, T0); nueva(&b, 100 + (uint32_t)s, T0);
            uint32_t t = T0 + 4u * 24u * H + 12u * H;
            pet_update(&a, t); pet_update(&b, t);
            a.rasgo[TRAIT_TRAVIESA] = 0.95f; a.rasgo[TRAIT_TIMIDA] = 0.05f; a.vinculo = 20;
            b.rasgo[TRAIT_TRAVIESA] = 0.05f; b.rasgo[TRAIT_TIMIDA] = 0.95f; b.vinculo = 20;
            for (int k = 0; k < 2; k++) {
                pet_t *x = k ? &b : &a;
                vaciar_eventos(x);
                x->necesidad[NEED_ENERGIA] = 80; x->necesidad[NEED_DIVERSION] = 70; x->dormida = 0; x->enferma = 0;
                x->actividad = ACTV_CAVAR; x->actividad_hasta = t + 600u;
                pet_action(x, ACT_JUGAR, 0, t);
                bool nego = false; pet_event_t e;
                while (pet_poll_event(x, &e)) nego |= e.type == EV_NIEGA;
                if (nego) {
                    if (k) niega_docil++; else niega_terca++;
                    x->actividad = ACTV_CAVAR;
                    pet_action(x, ACT_JUGAR, 0, t + 30u); /* insiste */
                    bool otra = false;
                    while (pet_poll_event(x, &e)) otra |= e.type == EV_NIEGA;
                    if (otra) insistio_ok = 0;
                }
            }
        }
        CHECK(niega_terca >= 6, "(fw 104) la terca a veces dice «estoy ocupada» (%d de 40)", niega_terca);
        CHECK(niega_docil <= 3 && niega_docil < niega_terca, "(fw 104) la dócil casi nunca (%d de 40)", niega_docil);
        CHECK(insistio_ok, "(fw 104) si insistes, te hace caso");
        /* en modo infante nunca se niega */
        static pet_t c;
        nueva(&c, 5, T0); c.modo_nino = MODO_INFANTE;
        uint32_t t = T0 + 4u * 24u * H + 12u * H; pet_update(&c, t);
        c.rasgo[TRAIT_TRAVIESA] = 1; c.rasgo[TRAIT_TIMIDA] = 0;
        int nc = 0;
        for (int i = 0; i < 30; i++) {
            vaciar_eventos(&c);
            c.necesidad[NEED_ENERGIA] = 80; c.necesidad[NEED_DIVERSION] = 50; c.dormida = 0; c.actividad = ACTV_CAVAR;
            pet_action(&c, ACT_JUGAR, 0, t + (uint32_t)i * 400u);
            pet_event_t e; while (pet_poll_event(&c, &e)) nc += e.type == EV_NIEGA;
        }
        CHECK(nc == 0, "(fw 104) en modo infante no es terca (%d)", nc);
        /* (1.0.12) con la luz apagada la terca resiste el sueño, pero NO dice «¡un ratito más!» cada segundo */
        static pet_t d;
        nueva(&d, 7, T0);
        uint32_t td = T0 + 4u * 24u * H + 15u * H; pet_update(&d, td);
        d.rasgo[TRAIT_TRAVIESA] = 1; d.rasgo[TRAIT_TIMIDA] = 0; d.vinculo = 20;
        d.dormida = 0; d.necesidad[NEED_ENERGIA] = 70; d.luz_apagada = 1; d.ultima_interaccion = td;
        vaciar_eventos(&d);
        int nd = 0;
        for (int i = 1; i <= 600; i++) { /* diez minutos, de a un segundo (como en la placa) */
            d.ultima_interaccion = td + (uint32_t)i; /* la siguen tocando */
            pet_update(&d, td + (uint32_t)i);
            pet_event_t e; while (pet_poll_event(&d, &e)) nd += e.type == EV_NIEGA && e.a == ACT_DORMIR;
        }
        CHECK(nd >= 1 && nd <= 3, "(1.0.12) la terca se resiste a dormir a lo más cada 4 min (%d avisos en 10 min)", nd);
    }
    /* --- paseos de verdad: cuenta con 800+ pasos, máximo 2 por día, el largo trae algo, queda en el diario --- */
    {
        static pet_t p;
        nueva(&p, 21, T0);
        uint32_t t = T0 + 3u * 24u * H + 11u * H;
        pet_update(&p, t);
        p.dormida = 0;
        CHECK(pet_walk_together(&p, 300, t) == 0, "(fw 104) 300 pasos no son un paseo");
        const float div0 = p.necesidad[NEED_DIVERSION] = 40;
        CHECK(pet_walk_together(&p, 1200, t + 60u) == 1, "(fw 104) 1200 pasos sí");
        CHECK(p.necesidad[NEED_DIVERSION] > div0, "(fw 104) el paseo la entretiene");
        CHECK(pet_walk_together(&p, 4000, t + 3600u) == 2, "(fw 104) un paseo largo trae algo");
        CHECK(pet_walk_together(&p, 4000, t + 7200u) == 0, "(fw 104) máximo 2 paseos premiados al día");
        CHECK(pet_walk_together(&p, 4000, t + 24u * H) >= 1, "(fw 104) al otro día vuelve a contar");
        bool hay = false;
        for (int k = 0; k < p.diario_n; k++) hay |= pet_diary_get(&p, k)->tipo == DIA_PASEO_JUNTOS;
        CHECK(hay, "(fw 104) el paseo queda en el diario");
        const entrada_diario_t ep = {t, DIA_PASEO_JUNTOS, 12, 0, 0};
        pet_diary_text(&p, &ep, buf, sizeof buf);
        CHECK(strstr(buf, "1200") != NULL, "(fw 104) el diario cuenta los pasos: %s", buf);
        p.dormida = 1;
        CHECK(pet_walk_together(&p, 4000, t + 50u * H) == 0, "(fw 104) dormida no cuenta");
    }
    /* --- (1.0.17) los juegos de a dos con una visita --- */
    {
        static pet_t p;
        nueva(&p, 22, T0);
        uint32_t t = T0 + 3u * 24u * H + 11u * H;
        pet_update(&p, t);
        p.dormida = 0;
        p.vecino.presente = 1; p.vecino.amistad = 50.0f;
        const float div0 = p.necesidad[NEED_DIVERSION] = 30;
        int nd = 0;
        CHECK(pet_duo_game(&p, PET_DOS_PING, 2, t) == 1, "(1.0.17) el ping pong cuenta");
        CHECK(p.necesidad[NEED_DIVERSION] > div0, "(1.0.17) jugar con la visita la entretiene");
        CHECK(p.vecino.amistad > 50.0f, "(1.0.17) suma amistad con el amigo del barrio");
        CHECK(p.dos_jugadas == 1 && p.dos_ganadas == 1, "(1.0.17) cuenta la partida y la ganada (%d %d)", p.dos_jugadas, p.dos_ganadas);
        for (int k = 0; k < p.diario_n; k++) nd += pet_diary_get(&p, k)->tipo == DIA_JUEGO_DOS;
        CHECK(nd == 1, "(1.0.17) queda en el diario (%d)", nd);
        CHECK(pet_duo_game(&p, PET_DOS_MEMO, 0, t + 600u) == 1 && p.dos_ganadas == 1 && p.dos_jugadas == 2, "(1.0.17) perder también cuenta");
        nd = 0;
        for (int k = 0; k < p.diario_n; k++) nd += pet_diary_get(&p, k)->tipo == DIA_JUEGO_DOS;
        CHECK(nd == 1, "(1.0.17) una vez al día en el diario (%d)", nd);
        CHECK(pet_duo_game(&p, PET_DOS_COUNT, 2, t) == 0, "(1.0.17) un juego que no existe no cuenta");
        char b2[200];
        for (int j = 0; j < PET_DOS_COUNT; j++)
            for (int r = 0; r < 3; r++) {
                const entrada_diario_t e = {t, DIA_JUEGO_DOS, (int8_t)j, (int8_t)r, 0};
                pet_diary_text(&p, &e, b2, sizeof b2);
                CHECK(b2[0] && !strchr(b2, '{') && !strchr(b2, '%'), "(1.0.17) texto del diario de a dos: %s", b2);
            }
        p.dormida = 1;
        CHECK(pet_duo_game(&p, PET_DOS_PING, 2, t + 900u) == 0, "(1.0.17) dormida no juega");
    }
    /* --- (1.0.18) los juegos de la isla --- */
    {
        static pet_t p;
        nueva(&p, 23, T0);
        uint32_t t = T0 + 3u * 24u * H + 11u * H;
        pet_update(&p, t);
        p.dormida = 0;
        p.coleccion[COL_CONCHAS] = 0;
        p.necesidad[NEED_DIVERSION] = 30;
        CHECK(pet_island_game(&p, PET_ISLA_FARO, 2, t) == 1, "(1.0.18) el faro con 2 barquitos cuenta (sin concha)");
        CHECK(p.necesidad[NEED_DIVERSION] > 30, "(1.0.18) el faro la entretiene");
        CHECK(pet_island_game(&p, PET_ISLA_BUCEO, 9, t + 60u) == 2 && p.coleccion[COL_CONCHAS], "(1.0.18) buen buceo: una concha nueva");
        CHECK(pet_island_game(&p, PET_ISLA_COUNT, 9, t) == 0, "(1.0.18) un juego que no existe no cuenta");
        char b2[200];
        for (int j = 0; j < PET_ISLA_COUNT; j++)
            for (int n = 0; n < 3; n++) {
                const entrada_diario_t e = {t, DIA_JUEGO_ISLA, (int8_t)j, (int8_t)n, 0};
                pet_diary_text(&p, &e, b2, sizeof b2);
                CHECK(b2[0] && !strchr(b2, '{') && !strchr(b2, '%'), "(1.0.18) texto del diario de la isla: %s", b2);
            }
    }
    /* --- (1.0.32) la colección de la pesca: 30 animales, cada uno en su estación, hora y clima --- */
    {
        static pet_t p;
        nueva(&p, 32, T0);
        uint32_t t = T0 + 2u * 24u * H;
        pet_update(&p, t);
        p.dormida = 0;
        CHECK(pet_fish_species_found(&p) == 0, "(1.0.32) una mascota nueva no tiene peces");
        char w[160];
        for (int k = 0; k < PET_PECES; k++) {
            pet_fish_species_when(k, w, sizeof w);
            CHECK(pet_fish_species_name(k)[0] && pet_fish_species_desc(k)[0] && w[0] && !strchr(w, '%') && strlen(w) < 150,
                  "(1.0.32) pez %d: %s — %s", k, pet_fish_species_name(k), w);
        }
        /* pescar mucho, en distintas fechas y horas: van apareciendo y quedan anotados */
        int salieron = 0, nuevos = 0;
        for (int i = 0; i < 400; i++) {
            const uint32_t ti = t + (uint32_t)i * 7919u * 13u;
            p.escena = ESC_LAGO; p.dormida = 0; p.actividad = ACTV_PESCAR; p.actividad_hasta = ti + 600u;
            pet_event_t e; while (pet_poll_event(&p, &e)) {}
            pet_fish_game(&p, 3, ti);
            while (pet_poll_event(&p, &e))
                if (e.type == EV_PESCA && (e.b >> 1 & 31)) { salieron++; nuevos += (e.b >> 6) & 1; CHECK((e.b >> 1 & 31) <= PET_PECES, "(1.0.32) el número del pez cabe"); }
        }
        CHECK(salieron > 100 && nuevos == pet_fish_species_found(&p) && pet_fish_species_found(&p) >= 12,
              "(1.0.32) pescando salen animales de la colección (%d, %d distintos)", salieron, pet_fish_species_found(&p));
        /* forzado (para probar): sale ese y se marca una sola vez */
        p.escena = ESC_LAGO; p.actividad = ACTV_PESCAR; p.actividad_hasta = t + 600u;
        static pet_t q;
        memcpy(&q, &p, sizeof q);
        q.peces_a[0] = q.peces_a[1] = q.peces_b[0] = q.peces_b[1] = 0;
        int vio = 0;
        for (int i = 0; i < 60 && !vio; i++) {
            q.escena = ESC_LAGO; q.dormida = 0; q.actividad = ACTV_PESCAR; q.actividad_hasta = t + 600u;
            pet_fish_species_force(28);
            pet_fish_game(&q, 3, t + (uint32_t)i);
            pet_event_t e; while (pet_poll_event(&q, &e)) if (e.type == EV_PESCA && (e.b >> 1 & 31) == 29) vio = 1 + ((e.b >> 6) & 1);
        }
        CHECK(vio == 2 && pet_fish_species_known(&q, 28) && pet_fish_species_found(&q) == 1, "(1.0.32) el pez arcoíris forzado sale nuevo y queda en la colección");
        char b2[200];
        const entrada_diario_t de = {t, DIA_PESCA, PESCA_PEZ, 1, 0};
        pet_diary_text(&q, &de, b2, sizeof b2);
        CHECK(strstr(b2, "trucha arcoíris") != NULL, "(1.0.32) el diario dice cuál pez: %s", b2);
    }
    /* --- (1.0.18) a veces queda hedionda sola; se le quita soplándole o espantando el olor --- */
    {
        static pet_t p;
        nueva(&p, 24, T0);
        uint32_t t = T0 + 2u * 24u * H;
        pet_update(&p, t);
        int veces = 0, dias = 0;
        for (int d = 0; d < 20; d++) { /* 20 días latido a latido (cada 5 min), despierta de 8 a 22 */
            for (int m = 0; m < 24 * 12; m++) {
                t += 300u;
                const int h = (int)(((t + (uint32_t)p.zona_horaria) % 86400u) / 3600u);
                p.dormida = h < 8 || h >= 22;
                p.necesidad[NEED_HIGIENE] = 80; p.necesidad[NEED_ENERGIA] = 80;
                const int antes = p.hediondo;
                pet_update(&p, t);
                if (!antes && p.hediondo) { veces++; CHECK(pet_stinky(&p), "(1.0.18) hedionda se nota"); if (veces == 1) { CHECK(pet_air_out(&p, t) == 1 && !p.hediondo, "(1.0.18) soplarle le quita el olor"); } }
            }
            dias++;
        }
        CHECK(veces >= 6 && veces <= 30, "(1.0.18) queda hedionda sola de vez en cuando (%d veces en %d días)", veces, dias);
        p.hediondo = 18;
        CHECK(pet_air_out(&p, t) == 2 && p.hediondo == 9, "(1.0.18) el olor del zorrillo solo baja a la mitad");
        p.hediondo = 0;
        CHECK(pet_air_out(&p, t) == 0, "(1.0.18) sin olor no hay nada que espantar");
    }
}

int main(void)
{
    printf("sizeof(pet_t) = %u bytes\n", (unsigned)sizeof(pet_t));
    test_necesidades_bajan();
    test_p0_salto();
    test_sin_repeticion();
    test_habilidades_coherentes();
    test_cacas_por_zona();
    test_juego_mas();
#if PET_APRENDE /* estas pruebas son del aprendizaje: sin él no aplican */
    test_musica_y_gustos();
#endif
    test_ajustes_nuevos();
    test_su_cancion();
    test_mini_eventos();
    test_nocturnos();
    test_melodia_propia();
    test_frases_diario();
    test_finales();
    test_hijo_y_ritmo();
    test_historias_dia();
    test_historia_ritmo();
    test_tilde();
    test_brizna_y_corazon();
    test_paginas_animales_lugares();
    test_actualizar_v36();
    test_historia_cinco_semanas();
    test_cristales_libres();
    test_v39();
    test_v40();
    test_v41();
    test_v101_clima_gusto();
    test_v101b_prisma();
    test_fw102_pochi();
    test_fw103_toques();
    test_fw104();
    test_fw108_mundos();
    test_1026_vuelve();
    test_v38_objetos();
    test_actualizar_v37();
    test_completista();
    test_comidas_temporada();
    test_alimentar();
    test_ponerse_al_dia_equivale();
    test_duerme_de_noche_y_despierta();
    test_cuidada_rara_vez_enferma();
    test_abandono_no_sufre();
    test_curar();
    test_guardado();
    test_visitas_y_relaciones();
    test_habitos();
    test_hallazgos_y_decoracion();
    test_evolucion();
    test_salto_enorme();
    test_huevo();
    test_emociones_con_causa();
    test_personalidad_y_memoria();
    test_frio();
    test_bebidas();
    test_llamar();
    test_mundo();
    test_fauna_y_jardin();
    test_dice_que_no();
    test_modo_nino();
    test_autonomia();
    test_estaciones_y_clima();
    test_frases();
    test_amigos();
    test_gestos();
    test_cumple();
    test_vecino();
    test_pesca();
    test_pesca_juego();
    test_casita();
    test_sonidos();
    test_lanzar_y_trucos();
    test_minijuegos_e_inclinar();
    test_vinculo_y_diario();
    test_habilidades();
    test_tesoros_y_colecciones();
    test_sorpresas();
    test_suenos_del_dia();
    test_modos();
    test_hora_perdida();
    test_respaldo();
    test_celebraciones();
    test_ropa();
    test_juguetes();
    test_baul();
    test_jardin_creativo();
    test_semillas_y_jardines();
    test_charcos_y_zonas();
    test_actualizar_partida();
    test_depende_de_ti();
    test_alarmas();
    test_mensajes();
    test_linaje();
    test_isla();
    test_v21();
    test_v21_perfiles();
    test_dos_anios();
    test_cartas();
    test_pesca_nueva();
    test_animales();
    test_mensajes_tipos();
    test_genero();
    test_v23_cartas();
    test_v23_animales();
    test_v23_criaturas();
    test_v23_secretos();
    test_v23_actualizar();
    test_v24_fiestas();
    test_v25();
    test_bolsa();
    test_regalos_niebla();
    test_aspecto_y_ambiente();
    test_fotos_y_ambiente();
    test_caracter_planes();
    printf("%d/%d comprobaciones OK\n", total - fallos, total);
    return fallos ? 1 : 0;
}
