/* ======================================================================
 * (versión 40) Lo escondido en los lugares secretos y las excursiones sola.
 * Incluido al final de pet.c (después de v39.inc).
 *  - En el claro (0), la casita (1) y el mirador (2) hay, una vez al día cada uno, algo escondido: musgo, ramitas o un regalito.
 *    Qué es y dónde está sale del día y de la mascota (igual todo el día, distinto cada día). En el claro lo canta el chucao.
 *  - De día, tranquila en el bosque o en el cerro, a veces se va sola al claro o al mirador (dos veces al día, como mucho)
 *    y vuelve a los minutos; si lo escondido de hoy sigue ahí, lo encuentra y se lo queda (101b: antes te lo traía). De noche nunca va sola al mirador, y después de
 *    las 20 tampoco al claro: vuelve al prado a dormir.
 * ==================================================================== */
static uint8_t z40_hoy(const pet_t *p, uint32_t t) { return (uint8_t)(1u + ((uint32_t)dia_local(p, t) & 0x7Fu)); }
static uint32_t z40_mezcla(const pet_t *p, int z, uint32_t t)
{
    uint32_t h = (uint32_t)dia_local(p, t) * 2654435761u ^ (p->id * 40503u) ^ ((uint32_t)z * 0x9E3779B9u);
    h ^= h >> 15; h *= 0x2C1B3C6Du; h ^= h >> 12; h *= 0x297A2D39u; h ^= h >> 15;
    return h;
}
static int8_t s_z40_objeto = -1;
static bool z40_abierta(const pet_t *p, int z)
{
    if (z == 0) return pet_secret_open(p, PET_ZONA_CLARO);
    if (z == 1) return pet_secret_open(p, 0);
    return z == 2 && pet_secret_open(p, 1);
}
int pet_zone_find(const pet_t *p, int z, uint32_t t)
{
    if ((unsigned)z >= PET_ZONAS_SECRETAS || p->etapa == STAGE_HUEVO || !z40_abierta(p, z)) return PET_ZH_NADA;
    if (p->zona_hallazgo[z] == z40_hoy(p, t)) return PET_ZH_NADA;
    const uint32_t h = z40_mezcla(p, z, t) % 10u;
    /* (fw 104) 2-3 veces por semana, UNA de las zonas esconde un papelito del bosque en vez del tesoro normal */
    if (papelito_toca(p, t) && (int)(z40_mezcla(p, 2, t + 1u) % 3u) == z) return PET_ZH_CARTA;
    /* la casita guarda más regalitos (eran las cosas de Peumo); el mirador, más ramitas (se las trae el viento) */
    static const uint8_t T[PET_ZONAS_SECRETAS][10] = {
        {1, 1, 1, 1, 2, 2, 2, 3, 3, 1},
        {3, 3, 3, 1, 1, 2, 2, 3, 1, 2},
        {2, 2, 2, 2, 1, 1, 3, 3, 2, 1},
    };
    return T[z][h];
}
int pet_zone_find_spot(const pet_t *p, int z, uint32_t t) { return (unsigned)z < PET_ZONAS_SECRETAS ? (int)((z40_mezcla(p, z, t) >> 9) % 3u) : 0; }
int pet_zone_find_object(const pet_t *p) { (void)p; return s_z40_objeto; }
int pet_zone_find_take(pet_t *p, int z, uint32_t t)
{
    int k = pet_zone_find(p, z, t);
    s_z40_objeto = -1;
    if (k == PET_ZH_NADA) return PET_ZH_NADA;
    p->zona_hallazgo[z] = z40_hoy(p, t);
    int r = k;
    if (k == PET_ZH_CARTA) { /* (fw 104) ¡un papelito! Se lee al tiro y queda en el Libro */
        const int c = papelito_proximo(p);
        if (c >= 0) { papelito_anotar(p, c, t); push_event(p, EV_CARTA_BOSQUE, c, 0, t); sonar(p, SND_SORPRESA, t); return PET_ZH_CARTA; }
        r = k = PET_ZH_MUSGO; /* (no debería pasar) */
    }
    if (k == PET_ZH_MUSGO) guardar_material(p, HALL_MUSGO);
    else if (k == PET_ZH_RAMITAS) { guardar_material(p, HALL_RAMITA); guardar_material(p, HALL_RAMITA); }
    else {
        const int id = pet_object_find(p, OFU_REGALO, t);
        if (id >= 0 && pet_object_gives(p, id, OFU_REGALO, t)) s_z40_objeto = (int8_t)id;
        else { guardar_material(p, HALL_MUSGO); r = PET_ZH_MUSGO; } /* (ya tiene todos los regalitos: queda musgo) */
    }
    sumar(p, NEED_CURIOSIDAD, 12);
    impulsar(p, EMO_ALEGRIA, 0.5f, CAUSA_HALLAZGO);
    impulsar(p, EMO_ORGULLO, 0.4f, CAUSA_HALLAZGO);
    sonar(p, r == PET_ZH_REGALO ? SND_ALEGRE : SND_SORPRESA, t);
    return r;
}

int pet_excursion_wants(const pet_t *p, int escena, uint32_t t)
{
    if (p->etapa == STAGE_HUEVO || p->dormida || p->enferma || p->en_zona) return -1;
    const int z = escena == ESC_BOSQUE ? 0 : (escena == ESC_COLINA ? 1 : -1);
    if (z < 0 || !z40_abierta(p, z == 0 ? 0 : 2)) return -1;
    const int h = hora_local(p, t);
    if (h < 8 || h >= (z == 1 ? 18 : 20)) return -1; /* de noche nunca va sola al mirador; tarde, tampoco al claro */
    if (p->excursion_dia == z40_hoy(p, t) && p->excursiones >= 2) return -1;
    if (p->necesidad[NEED_SACIEDAD] < 45.0f || p->necesidad[NEED_ENERGIA] < 40.0f || p->necesidad[NEED_SEGURIDAD] < 50.0f ||
        p->necesidad[NEED_SALUD] < 60.0f) return -1;
    return z;
}
int pet_excursion_back(pet_t *p, int z, uint32_t t)
{
    if ((unsigned)z > 1) return PET_ZH_NADA;
    if (p->excursion_dia != z40_hoy(p, t)) { p->excursion_dia = z40_hoy(p, t); p->excursiones = 0; }
    if (p->excursiones < 255) p->excursiones++;
    /* (101b) si encontró lo escondido de hoy, SE LO QUEDA (es su tesoro): ya no va a la bolsa ni a los materiales. Ya no está
     * para que lo busques tú; mañana hay otro. En el diario va como b = 4..6 («me lo quedé») */
    const int zz = z == 0 ? 0 : 2, k = pet_zone_find(p, zz, t);
    if (k == PET_ZH_CARTA) { /* (fw 104) un papelito no se lo queda: te lo trae sin leer y lo abren juntos */
        const int c = papelito_proximo(p);
        p->zona_hallazgo[zz] = z40_hoy(p, t);
        if (c >= 0) { papelito_anotar(p, c, t); push_event(p, EV_CARTA_BOSQUE, c, 1, t); }
        impulsar(p, EMO_ORGULLO, 0.6f, CAUSA_HALLAZGO);
        sumar(p, NEED_CURIOSIDAD, 10);
        sumar(p, NEED_DIVERSION, 6);
        anotar(p, DIA_EXCURSION, z, 7, t); /* b 7: trajo un papelito */
        return k;
    }
    if (k != PET_ZH_NADA) {
        p->zona_hallazgo[zz] = z40_hoy(p, t);
        sumar(p, NEED_CURIOSIDAD, 12);
        impulsar(p, EMO_ALEGRIA, 0.5f, CAUSA_HALLAZGO);
        impulsar(p, EMO_ORGULLO, 0.4f, CAUSA_HALLAZGO);
        sonar(p, k == PET_ZH_REGALO ? SND_ALEGRE : SND_SORPRESA, t);
    }
    sumar(p, NEED_CURIOSIDAD, 10);
    sumar(p, NEED_DIVERSION, 6);
    anotar(p, DIA_EXCURSION, z, k != PET_ZH_NADA ? k + 3 : 0, t);
    return k;
}
