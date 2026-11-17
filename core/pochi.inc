/* ======================================================================
 * (fw 102) Pochi, la chihuahua visitante, es la mamá de Chapi, Pasita, Morita y Rony, y se acuerda de ti.
 * Incluido al final de pet.c (después de v41.inc). Los datos van en dos bytes que eran relleno (pochi_visitas, pochi_mem).
 *  - Chapi: el cochino amoroso (se revuelca y te abraza aunque esté sucio).
 *  - Pasita y Morita: tiernas (Morita, además, tímida).
 *  - Rony: el más loco; le encanta jugar con cosas chiquititas.
 * Los cuatro son igual de importantes: ninguno viene primero ni más seguido que los otros.
 * ==================================================================== */
const char *pet_pochi_pup_name(int k)
{
    static const char *const N[PET_POCHI_CACHORROS] = {"Chapi", "Pasita", "Morita", "Rony"};
    return (unsigned)k < PET_POCHI_CACHORROS ? N[k] : "";
}
int pet_pochi_visits(const pet_t *p) { return p->pochi_visitas; }
int pet_pochi_last(const pet_t *p) { return p->pochi_mem & 3; }
bool pet_pochi_pup_friend(const pet_t *p, int k) { return (unsigned)k < PET_POCHI_CACHORROS && ((p->pochi_amigos >> k) & 1u); }
bool pet_pochi_pup_met(const pet_t *p, int k) { return (unsigned)k < PET_POCHI_CACHORROS && ((p->pochi_mem >> (2 + k)) & 1u); }
/* Desde la tercera visita, a veces viene con sus cachorros (cerca de la mitad de las veces): uno, dos o (cuando ya los conoces a
 * todos) la familia completa. Los cuatro son igual de importantes: el orden es al azar (distinto en cada partida y visita), y
 * los que todavía no conoces van primero, de a uno nuevo por visita. La tercera visita, siempre con uno. */
static uint32_t pochi_mezcla(const pet_t *p, uint32_t t, uint32_t sal)
{
    uint32_t h = (uint32_t)p->pochi_visitas * 2654435761u ^ (p->id * 40503u) ^ ((uint32_t)dia_local(p, t) * 0x9E3779B9u) ^ sal * 0x85EBCA6Bu;
    h ^= h >> 15; h *= 0x2C1B3C6Du; h ^= h >> 12; h *= 0x297A2D39u; h ^= h >> 15;
    return h;
}
int pet_pochi_pups_pick(const pet_t *p, uint32_t t, int out[PET_POCHI_CACHORROS])
{
    if (p->etapa == STAGE_HUEVO || p->pochi_visitas < 2) return 0;
    const uint32_t h = pochi_mezcla(p, t, 1);
    if (p->pochi_visitas > 2 && h % 100u >= 45u) return 0;
    /* un orden al azar de los cuatro */
    int orden[PET_POCHI_CACHORROS] = {0, 1, 2, 3};
    uint32_t r = pochi_mezcla(p, t, 2);
    for (int i = PET_POCHI_CACHORROS - 1; i > 0; i--) { const int j = (int)(r % (uint32_t)(i + 1)); r = r / (uint32_t)(i + 1) ^ (r << 7); const int x = orden[i]; orden[i] = orden[j]; orden[j] = x; }
    int conocidos = 0, nuevo = -1;
    for (int i = 0; i < PET_POCHI_CACHORROS; i++) { if (pet_pochi_pup_met(p, orden[i])) conocidos++; else if (nuevo < 0) nuevo = orden[i]; }
    const uint32_t c = (h >> 8) % 100u;
    int cuantos = 1;
    if (conocidos == PET_POCHI_CACHORROS && c < 15u) cuantos = 4;      /* ¡la familia completa! */
    else if (conocidos >= 1 && c < 50u) cuantos = 2;                    /* dos hermanos */
    int n = 0;
    if (nuevo >= 0) out[n++] = nuevo;                                   /* uno nuevo para conocer (de a uno) */
    for (int i = 0; i < PET_POCHI_CACHORROS && n < cuantos; i++) if (orden[i] != nuevo && pet_pochi_pup_met(p, orden[i])) out[n++] = orden[i];
    return n;
}
int pet_pochi_pup_pick(const pet_t *p, uint32_t t) { int o[PET_POCHI_CACHORROS]; return pet_pochi_pups_pick(p, t, o) > 0 ? o[0] : -1; }
