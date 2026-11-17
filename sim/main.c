/*
 * Simulador de PC de la mascota virtual.
 *
 *   ./mascota_sim            modo interactivo (escribe "ayuda")
 *   ./mascota_sim --demo     3 días simulados con un dueño de rutina fija
 *   ./mascota_sim --demo --descuido   igual, pero el dueño la abandona el día 2
 *   ./mascota_sim --semilla N         otra personalidad
 *
 * El reloj es virtual: "esperar 3h" avanza el tiempo sin esperar de verdad.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "pet.h"

#define ZONA_CHILE (-3 * 3600)

static uint32_t reloj; /* hora virtual (Unix) */
static int verboso = 1;

/* ---------- presentación en texto ---------- */

static const char *icono_emocion(mood_t m)
{
    static const char *const T[] = {"(^o^)", "(-_-)", "(>_<)nam?", "(=_=)zz", "(x_x)~", "(._.)",
                                    "(T_T)", "(>:( )", "(+_+)", "(*o*)<3", "(u_u)Zz", "(o.o)?", "(;O;)", "(-v-)*", "(>_>)"};
    return (unsigned)m < MOOD_COUNT ? T[m] : "?";
}

static void barra(const char *nombre, float v)
{
    int n = (int)(v / 5.0f + 0.5f);
    printf("  %-10s [", nombre);
    for (int i = 0; i < 20; i++) putchar(i < n ? '#' : '.');
    printf("] %3.0f%s\n", v, v < 30 ? "  <-- bajo" : "");
}

static void hora_texto(uint32_t t, char *buf, size_t n)
{
    time_t local = (time_t)((int64_t)t + ZONA_CHILE);
    struct tm tmv;
#if defined(_WIN32)
    gmtime_s(&tmv, &local);
#else
    gmtime_r(&local, &tmv);
#endif
    strftime(buf, n, "%a %H:%M", &tmv);
}

static void mostrar_estado(const pet_t *p)
{
    char h[32];
    hora_texto(reloj, h, sizeof h);
    printf("\n%s  %s el %s (%s, %.1f dias)  %s\n", h, p->nombre, pet_species_name(p->especie),
           pet_stage_name((stage_t)p->etapa), pet_age_days(p, reloj), icono_emocion(pet_mood(p)));
    char sente[128];
    pet_feeling(p, sente, sizeof sente);
    printf("  \"%s\"\n  hace: %s en %s   confianza %.0f\n", sente, pet_activity_name((actividad_t)p->actividad),
           pet_place_name((lugar_t)p->lugar), p->vinculo);
    printf("  tiene: fruta %d, miel %d, hierba %d, semillas %d, piedras %d\n", p->inventario[0], p->inventario[1],
           p->inventario[2], p->inventario[3], p->inventario[4]);
    printf("  emocion: %-12s quiere: %-10s %s%s\n", pet_mood_name(pet_mood(p)), pet_want_name(pet_want(p)),
           p->enferma ? "[ENFERMA] " : "", p->luz_apagada ? "[luz apagada]" : "");
    for (int i = 0; i < NEED_COUNT; i++) barra(pet_need_name((need_t)i), pet_need(p, (need_t)i));
}

static void mostrar_personalidad(const pet_t *p)
{
    printf("  personalidad:");
    for (int i = 0; i < TRAIT_COUNT; i++) printf(" %s %.2f%s", pet_trait_name((trait_t)i), p->rasgo[i], i + 1 < TRAIT_COUNT ? "," : "\n");
    printf("  gustos: comida %d, fruta %d, miel %d (0-255); lluvia %d, sol %d, nieve %d; barriga %d, nariz %d\n",
           p->habitos.gusto[0], p->habitos.gusto[1], p->habitos.gusto[2], p->memoria.clima[CLIMA_LLUVIA],
           p->memoria.clima[CLIMA_DESPEJADO], p->memoria.clima[CLIMA_NIEVE], p->memoria.gesto[GESTO_CARICIA],
           p->memoria.gesto[GESTO_MUCHOS]);
}

static const char *const FSUC_TXT[FSUC_COUNT] = {"llego", "se fue", "la persigue", "la pico", "ya no la persigue (aprendio)", "se robo una fruta", "lo espantaste", "lo echo ella misma", "le trajo una semilla"};
static const char *const JAR_TXT[] = {"plantaste una semilla", "crecio", "florecio", "tiene sed", "revivio al regarla", "dio una fruta", "enterro una semilla: saldra un arbol"};

static void mostrar_eventos(pet_t *p)
{
    pet_event_t e;
    char h[32];
    while (pet_poll_event(p, &e)) {
        if (!verboso && e.type == EV_SONIDO) continue;
        hora_texto(e.t, h, sizeof h);
        printf("  %s  ", h);
        switch (e.type) {
        case EV_EMOCION: printf("ahora esta %s %s\n", pet_mood_name((mood_t)e.a), icono_emocion((mood_t)e.a)); break;
        case EV_PIDE: printf("PIDE: %s\n", pet_want_name((want_t)e.a)); break;
        case EV_SE_DURMIO: printf("se durmio\n"); break;
        case EV_DESPERTO: printf("desperto%s\n", e.a ? " (la despertaste)" : ""); break;
        case EV_ENFERMO: printf("se enfermo!\n"); break;
        case EV_SANO: printf("se curo\n"); break;
        case EV_REACCION:
            printf("reacciona a '%s': %s\n", pet_action_name((action_t)e.a),
                   e.b > 0 ? "le gusto" : (e.b == -2 ? "no quiso" : (e.b < 0 ? "no le gusto" : "indiferente")));
            break;
        case EV_VISITA_LLEGA:
            printf("llega de visita un %s\n", pet_species_name(p->visitantes[e.a].especie));
            break;
        case EV_ENCUENTRO:
            printf("con el %s: %s (relacion %d)\n", pet_species_name(p->visitantes[e.a].especie),
                   pet_encounter_name((encounter_t)e.b), p->visitantes[e.a].relacion);
            break;
        case EV_VISITA_SE_VA: printf("la visita se fue\n"); break;
        case EV_CRECIO:
            printf("crecio! ahora es %s%s%s\n", pet_stage_name((stage_t)e.a), e.a == STAGE_ADULTA ? ", forma " : "",
                   e.a == STAGE_ADULTA ? pet_form_name((forma_t)e.b) : "");
            break;
        case EV_SONIDO: printf("~ sonido: %s\n", pet_sound_name((sound_t)e.a)); break;
        case EV_REGALO: printf("cayo una fruta del arbol\n"); break;
        case EV_SIN_ITEM: printf("no queda %s\n", pet_item_name((item_t)e.a)); break;
        case EV_DECORO: printf("pusiste %s\n", pet_decor_name((decor_t)e.a)); break;
        case EV_PENSAMIENTO: printf("piensa: %s por %s\n", pet_emotion_name((emo_t)e.a), pet_cause_name((causa_t)e.b)); break;
        case EV_ACTIVIDAD: printf("%s en %s\n", pet_activity_name((actividad_t)e.a), pet_place_name((lugar_t)e.b)); break;
        case EV_HALLAZGO: printf("encontro %s%s\n", pet_find_name((hallazgo_t)e.a), e.b ? " y te lo trae" : ""); break;
        case EV_REENCUENTRO: printf("volviste tras %d h: te recibe feliz\n", e.a); break;
        case EV_CLIMA: printf("clima: %s\n", pet_weather_name((clima_t)e.a)); break;
        case EV_TRUENO: printf("trueno\n"); break;
        case EV_NACIO: printf("nacio!\n"); break;
        case EV_FRASE: printf("mensaje: %s\n", pet_phrase(e.a)); break;
        case EV_DESCUBRE: printf("mostro un gusto (tipo %d, %d)\n", e.a, e.b); break;
        case EV_RASGO: printf("se vuelve %s %s\n", e.b > 0 ? "mas" : "menos", pet_trait_name((trait_t)e.a)); break;
        case EV_ESCENA: printf("%s %s\n", e.b ? "se fue sola a" : "la llevaste a", pet_scene_name((escena_t)e.a)); break;
        case EV_ESCENA_NUEVA: printf("se abrio una zona nueva: %s\n", pet_scene_name((escena_t)e.a)); break;
        case EV_FAUNA: printf("%s: %s\n", pet_fauna_name((fauna_t)e.a), FSUC_TXT[e.b % FSUC_COUNT]); break;
        case EV_JARDIN: printf("jardin (parcela %d): %s\n", e.a + 1, JAR_TXT[e.b % 7]); break;
        case EV_SALPICA: printf("%s\n", e.b > 0 ? "salto en un charco: le encanto" : "piso un charco: se ensucio"); break;
        case EV_HOJAS: printf("salto en el monton de hojas\n"); break;
        case EV_VECINO:
            printf("%s: %s%s%s\n", p->vecino.nombre, pet_neighbor_activity_name((vecino_act_t)e.a),
                   e.a == VEC_REGALO ? ": " : "", e.a == VEC_REGALO ? pet_item_name((item_t)e.b) : "");
            break;
        case EV_PESCA: printf("pesco %s%s\n", pet_catch_name((pesca_t)e.a), e.a == PESCA_PEZ ? (e.b ? " y se lo comio" : " y lo devolvio al agua") : ""); break;
        case EV_VINCULO: printf("VINCULO: ahora %s\n", pet_bond_name((vinculo_nivel_t)e.a)); break;
        case EV_HABILIDAD: {
            static const char *const T[] = {"se cayo intentando", "practico", "primer intento de", "aprendio a", "es experta en"};
            printf("%s %s\n", T[(e.b + 1) % 5], pet_skill_name((habilidad_t)e.a));
            break;
        }
        case EV_TESORO:
            if (e.a == 0) printf("huele un tesoro escondido...\n");
            else if (e.b < 0) printf("desenterro algo repetido: una piedra brillante\n");
            else printf("TESORO: %s\n", pet_piece_name((coleccion_t)(e.b / 8), e.b % 8));
            break;
        case EV_COLECCION: printf("COLECCION COMPLETA: %s\n", pet_collection_name((coleccion_t)e.a)); break;
        case EV_RARO: printf(e.b ? "algo raro: %s\n" : "se fue %s\n", pet_rare_name((raro_t)e.a)); break;
        case EV_META:
            if (e.b == 0) printf("meta de hoy: %s\n", pet_goal_name((meta_t)e.a));
            else if (e.b == 1) printf("META CUMPLIDA: %s (estampitas: %d)\n", pet_goal_name((meta_t)e.a), p->estampitas);
            else printf("meta: %d de %d\n", p->meta_progreso, p->meta_objetivo);
            break;
        case EV_SU_CUMPLE: printf(e.a == 7 && !e.b ? "cumple una semana!\n" : (e.b ? "cumple %d anos!\n" : "cumple %d meses!\n"), e.a); break;
        case EV_FECHA: { const celebracion_t *c = pet_celebration_get(e.b); printf("hoy es %s\n", c ? c->nombre : pet_date_name((fecha_t)e.a)); break; }
        case EV_FOTO: printf("foto para el album\n"); break;
        case EV_ROPA: {
            static const char *const R[] = {"le da calor: se saco", "se saco", "no le gusta mucho", "le da igual", "le gusta", "le encanta", "", "le quitaste"};
            if (e.b >= -3 && e.b <= 4 && R[e.b + 3][0]) printf("%s: %s\n", R[e.b + 3], pet_clothes_name((ropa_t)e.a));
            break;
        }
        case EV_JUGUETE: {
            static const char *const J[] = {"no quiso jugar con", "juega con", "estreno", "acepto jugar con", "pusiste en el prado:", "guardaste"};
            if (e.b >= -1 && e.b <= 4) printf("%s %s\n", J[e.b + 1], pet_toy_name((juguete_t)e.a));
            break;
        }
        case EV_NUEVO: printf("algo nuevo en el baul: %s\n", e.a ? pet_toy_name((juguete_t)e.b) : pet_clothes_name((ropa_t)e.b)); break;
        case EV_TRUCO: printf(e.b == 2 ? "aprendio a %s!\n" : "hizo sola el truco: %s\n", pet_trick_name((truco_t)e.a)); break;
        case EV_CUMPLE: printf("FELIZ CUMPLEANOS! cumples %d\n", e.a); break;
        default: printf("evento %d\n", e.type); break;
        }
    }
}

static void avanzar(pet_t *p, uint32_t segundos)
{
    reloj += segundos;
    pet_update(p, reloj);
    mostrar_eventos(p);
}

static void hacer(pet_t *p, action_t a, int arg)
{
    pet_action(p, a, arg, reloj);
    mostrar_eventos(p);
}

/* ---------- demo: un dueño con rutina ---------- */

static int hora_local_actual(void)
{
    int64_t l = (int64_t)reloj + ZONA_CHILE;
    return (int)((l % 86400 + 86400) % 86400 / 3600);
}

static void demo(pet_t *p, int descuido)
{
    verboso = 0;
    printf("Demo: 3 dias simulados. Rutina del dueno: comida 8h, 13h, 20h; juego 18h;\n"
           "carinos 8h y 21h; bano 19h; luz apagada 22h; despierta 7h; cura si enferma.\n");
    if (descuido) printf("Variante: el dueno no aparece el dia 2.\n");
    for (int paso = 0; paso < 3 * 24; paso++) {
        avanzar(p, 3600);
        int h = hora_local_actual();
        int dia = paso / 24;
        if (descuido && dia == 1) continue;
        if (h == 7) hacer(p, ACT_DESPERTAR, 0);
        if (h == 8 || h == 13 || h == 20) hacer(p, ACT_ALIMENTAR, h == 13 ? FOOD_FRUTA : FOOD_COMIDA);
        if (h == 8 || h == 21) hacer(p, ACT_ACARICIAR, GESTO_TOQUE);
        if (h == 18) hacer(p, ACT_JUGAR, 0);
        if (h == 19) hacer(p, ACT_BANAR, 0);
        if (h == 22) hacer(p, ACT_DORMIR, 0);
        if (p->enferma) hacer(p, ACT_CURAR, 0);
        if (h == 12 || h == 23) mostrar_estado(p);
    }
    printf("\nHabitos aprendidos (probabilidad de juego por hora, 0-255):\n  ");
    for (int h = 0; h < 24; h++) printf("%02d:%3d%s", h, p->habitos.prob[1][h], h % 8 == 7 ? "\n  " : "  ");
    printf("\n");
    mostrar_personalidad(p);
}

/* ---------- modo interactivo ---------- */

static void ayuda(void)
{
    printf("Comandos:\n"
           "  comer [comida|fruta|miel]   banar   cepillar   jugar   sacudir\n"
           "  acariciar [toque|caricia|abrazo|muchos]   luz (apaga y duerme)   despertar   curar\n"
           "  pescar (en el lago)   invitar (a su amigo del barrio)   nacimiento DD/MM/AAAA\n"
           "  lanzar (la pelota)   inclinar   truco [sentarse|patita|vuelta|saludo]   diario\n"
           "  ir [prado|bosque|lago|colina|cueva]   plantar N [flor|girasol|frutilla|hierba]   regar N (1..6)   espantar\n"
           "  vestir [mono|panuelo|gorro|bufanda|sombrero|lentes|corona|collar|nada]   juguete [peluche|peloton|...]   baul\n"
           "  esperar N[m|h|d]   (ej: esperar 30m, esperar 5h, esperar 1d)\n"
           "  rincon   decorar N   estado   personalidad   visitas   guardar   cargar   salir\n");
}

static void visitas(const pet_t *p)
{
    int n = 0;
    for (int i = 0; i < PET_MAX_VISITORS; i++) {
        const visitor_t *v = &p->visitantes[i];
        if (!v->conocido) continue;
        printf("  %d. %-10s relacion %4d, %u encuentros%s\n", i, pet_species_name(v->especie), v->relacion,
               v->encuentros, v->enamorados ? ", enamorados" : "");
        n++;
    }
    if (!n) printf("  todavia no conoce a nadie\n");
}

static uint32_t parse_duracion(const char *s)
{
    char *fin;
    double v = strtod(s, &fin);
    if (v <= 0) return 0;
    switch (*fin) {
    case 'd': return (uint32_t)(v * 86400);
    case 'm': return (uint32_t)(v * 60);
    default: return (uint32_t)(v * 3600);
    }
}

static void interactivo(pet_t *p)
{
    char linea[128];
    ayuda();
    mostrar_estado(p);
    while (printf("\n> "), fflush(stdout), fgets(linea, sizeof linea, stdin)) {
        char cmd[32] = {0}, arg[32] = {0};
        if (sscanf(linea, "%31s %31s", cmd, arg) < 1) continue;
        if (!strcmp(cmd, "salir")) break;
        else if (!strcmp(cmd, "ayuda")) ayuda();
        else if (!strcmp(cmd, "estado")) mostrar_estado(p);
        else if (!strcmp(cmd, "personalidad")) mostrar_personalidad(p);
        else if (!strcmp(cmd, "visitas")) visitas(p);
        else if (!strcmp(cmd, "rincon")) {
            printf("  semillas %d, piedras %d\n", p->inventario[ITEM_SEMILLA], p->inventario[ITEM_PIEDRA]);
            for (int i = 0; i < DECOR_COUNT; i++) {
                int se, pi;
                pet_decor_cost((decor_t)i, &se, &pi);
                printf("  %d. %-7s %d semillas, %d piedras%s\n", i, pet_decor_name((decor_t)i), se, pi,
                       pet_has_decor(p, (decor_t)i) ? "  (ya esta)" : "");
            }
        } else if (!strcmp(cmd, "decorar")) {
            int r = pet_place_decor(p, (decor_t)atoi(arg), reloj);
            if (r == 0) printf("  te faltan semillas o piedras\n");
            else if (r < 0) printf("  no se puede\n");
            mostrar_eventos(p);
        }
        else if (!strcmp(cmd, "comer")) {
            int f = !strcmp(arg, "fruta") ? FOOD_FRUTA : (!strcmp(arg, "miel") ? FOOD_MIEL : FOOD_COMIDA);
            hacer(p, ACT_ALIMENTAR, f);
        } else if (!strcmp(cmd, "banar")) hacer(p, ACT_BANAR, 0);
        else if (!strcmp(cmd, "cepillar")) hacer(p, ACT_CEPILLAR, 0);
        else if (!strcmp(cmd, "jugar")) hacer(p, ACT_JUGAR, 0);
        else if (!strcmp(cmd, "acariciar")) {
            int g = !strcmp(arg, "caricia") ? GESTO_CARICIA : (!strcmp(arg, "abrazo") ? GESTO_ABRAZO : (!strcmp(arg, "muchos") ? GESTO_MUCHOS : GESTO_TOQUE));
            hacer(p, ACT_ACARICIAR, g);
        }
        else if (!strcmp(cmd, "sacudir")) hacer(p, ACT_SACUDIR, 0);
        else if (!strcmp(cmd, "luz")) hacer(p, ACT_DORMIR, 0);
        else if (!strcmp(cmd, "despertar")) hacer(p, ACT_DESPERTAR, 0);
        else if (!strcmp(cmd, "curar")) hacer(p, ACT_CURAR, 0);
        else if (!strcmp(cmd, "ir")) {
            int e = ESC_PRADO;
            for (int k = 0; k < ESC_COUNT; k++) if (!strcmp(arg, pet_scene_name((escena_t)k))) e = k;
            hacer(p, ACT_IR, e);
        } else if (!strcmp(cmd, "plantar")) { /* plantar <parcela> [flor|girasol|frutilla|hierba] */
            static const char *const C[CULTIVO_COUNT] = {"flor", "girasol", "frutilla", "hierba"};
            int parcela = atoi(arg) - 1, cultivo = 0;
            for (int i = 0; i < CULTIVO_COUNT; i++) if (strstr(arg, C[i])) cultivo = i;
            hacer(p, ACT_PLANTAR, parcela | cultivo << 4);
        }
        else if (!strcmp(cmd, "vestir")) { /* vestir <ropa> | vestir nada */
            int r = -1;
            for (int i = 0; i < ROPA_COUNT; i++) if (!strncmp(arg, pet_clothes_name((ropa_t)i), strlen(arg) > 3 ? 4 : strlen(arg))) r = i;
            if (!strcmp(arg, "nada")) for (int s = 0; s < RANURA_COUNT; s++) hacer(p, ACT_VESTIR, -(s + 1));
            else if (r >= 0) hacer(p, ACT_VESTIR, r);
            else printf("  ropa: mono, panuelo, gorro, bufanda, sombrero, lentes, corona, collar (o nada)\n");
        }
        else if (!strcmp(cmd, "juguete")) { /* juguete <nombre>: lo pone en el prado y la invita */
            int j = -1;
            for (int i = 0; i < JUG_COUNT; i++) if (*arg && !strncmp(arg, pet_toy_name((juguete_t)i), 3)) j = i;
            if (j < 0) printf("  juguetes: peluche, peloton, caja, columpio, tambor, molinete\n");
            else {
                if (!pet_toy_placed(p, (juguete_t)j, NULL, NULL)) hacer(p, ACT_PONER_JUGUETE, j | (120 << 8) | (140 << 16));
                hacer(p, ACT_USAR_JUGUETE, j);
            }
        }
        else if (!strcmp(cmd, "baul")) {
            printf("  ropa:");
            for (int i = 0; i < ROPA_COUNT; i++) if (pet_has_clothes(p, (ropa_t)i)) printf(" %s (%+d)", pet_clothes_name((ropa_t)i), pet_clothes_liking(p, (ropa_t)i));
            printf("\n  juguetes:");
            for (int i = 0; i < JUG_COUNT; i++) if (pet_has_toy(p, (juguete_t)i)) printf(" %s (%+d)", pet_toy_name((juguete_t)i), pet_toy_liking(p, (juguete_t)i));
            printf("\n  ambiente ahora: %s\n", pet_ambient_name(pet_ambient(p, reloj)));
        }
        else if (!strcmp(cmd, "regar")) hacer(p, ACT_REGAR, atoi(arg) - 1);
        else if (!strcmp(cmd, "espantar")) hacer(p, ACT_ESPANTAR, FAUNA_ZORRO);
        else if (!strcmp(cmd, "pescar")) hacer(p, ACT_PESCAR, 0);
        else if (!strcmp(cmd, "lanzar")) hacer(p, ACT_LANZAR, 0);
        else if (!strcmp(cmd, "diario")) {
            char buf[96];
            printf("  vinculo: %s (%.0f) | nadar %.0f%% trepar %.0f%% | estampitas %d\n", pet_bond_name(pet_bond_level(p)), p->vinculo,
                   p->habilidad[HAB_NADAR], p->habilidad[HAB_TREPAR], p->estampitas);
            if (p->meta_dia) printf("  meta de hoy: %s (%d/%d)%s\n", pet_goal_name((meta_t)p->meta), p->meta_progreso, p->meta_objetivo, p->meta_hecha ? " ¡cumplida!" : "");
            for (int i = 0; i < pet_diary_count(p) && i < 12; i++) printf("  - %s\n", pet_diary_text(p, pet_diary_get(p, i), buf, sizeof buf));
            printf("  album: %d fotos | colecciones:", pet_album_count(p));
            for (int c = 0; c < COL_COUNT; c++) {
                int n = 0;
                for (int k = 0; k < pet_collection_size((coleccion_t)c); k++) n += (p->coleccion[c] >> k) & 1;
                printf(" %s %d/%d", pet_collection_name((coleccion_t)c), n, pet_collection_size((coleccion_t)c));
            }
            printf("\n");
        }
        else if (!strcmp(cmd, "inclinar")) hacer(p, ACT_INCLINAR, 0);
        else if (!strcmp(cmd, "truco")) {
            int k = !strcmp(arg, "voltereta") ? TRUCO_VOLTERETA : (!strcmp(arg, "vuelta") ? TRUCO_VUELTA : (!strcmp(arg, "saludo") ? TRUCO_SALUDO : TRUCO_DORMIDA));
            hacer(p, ACT_TRUCO, k);
            printf("  sabe %s: %.0f%%\n", pet_trick_name((truco_t)k), p->truco[k]);
        }
        else if (!strcmp(cmd, "invitar")) hacer(p, ACT_INVITAR_VECINO, 0);
        else if (!strcmp(cmd, "nacimiento")) {
            int d = 0, m = 0, a = 0;
            if (sscanf(arg, "%d/%d/%d", &d, &m, &a) == 3 && pet_set_owner_birthdate(p, d, m, a, reloj))
                printf("  tienes %d años: modo %s\n", pet_owner_age(p, reloj), pet_mode_name(pet_mode(p)));
            else printf("  fecha inválida (DD/MM/AAAA)\n");
        }
        else if (!strcmp(cmd, "esperar")) {
            uint32_t s = parse_duracion(arg[0] ? arg : "1h");
            avanzar(p, s);
            mostrar_estado(p);
        } else if (!strcmp(cmd, "guardar")) {
            FILE *f = fopen("mascota.sav", "wb");
            pet_seal(p);
            if (f && fwrite(p, sizeof *p, 1, f) == 1) printf("  guardada en mascota.sav\n");
            if (f) fclose(f);
        } else if (!strcmp(cmd, "cargar")) {
            pet_t tmp;
            FILE *f = fopen("mascota.sav", "rb");
            if (f && fread(&tmp, sizeof tmp, 1, f) == 1 && pet_validate(&tmp)) {
                *p = tmp;
                reloj = p->ultima_actualizacion;
                printf("  cargada\n");
                mostrar_estado(p);
            } else printf("  no hay partida valida\n");
            if (f) fclose(f);
        } else printf("  no entiendo '%s' (escribe ayuda)\n", cmd);
    }
}

int main(int argc, char **argv)
{
    int modo_demo = 0, descuido = 0;
    uint32_t semilla = 12345;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--demo")) modo_demo = 1;
        else if (!strcmp(argv[i], "--descuido")) descuido = 1;
        else if (!strcmp(argv[i], "--semilla") && i + 1 < argc) semilla = (uint32_t)strtoul(argv[++i], NULL, 10);
    }
    /* empieza un lunes a las 08:00 hora de Chile, en tiempo virtual */
    reloj = 1792972800u /* 2026-10-26 00:00 UTC */ + 11u * 3600u;

    static pet_t mascota;
    pet_init(&mascota, "Pompon", (uint8_t)(semilla % pet_species_count()), reloj, semilla);
    pet_set_timezone(&mascota, ZONA_CHILE);
    pet_set_owner_age(&mascota, 30);
    pet_hatch(&mascota, reloj);
    printf("Nacio %s, un %s.\n", mascota.nombre, pet_species_name(mascota.especie));
    mostrar_personalidad(&mascota);

    if (modo_demo) demo(&mascota, descuido);
    else interactivo(&mascota);
    return 0;
}
