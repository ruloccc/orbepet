/* ======================================================================
 * (versión 41) Los juegos de los lugares secretos: récord, estrellas y premios.
 * Incluido al final de pet.c (después de v40.inc).
 * ==================================================================== */
static int8_t s_jl_ram, s_jl_mus, s_jl_obj = -1;
int pet_place_game_record(const pet_t *p, int jg) { return (unsigned)jg < PET_JUEGOS_LUGAR ? p->jl_record[jg] : 0; }
int pet_place_game_stars(const pet_t *p, int jg) { return (unsigned)jg < PET_JUEGOS_LUGAR ? p->jl_estrellas[jg] : 0; }
void pet_place_game_prize(int *ramitas, int *musgo, int *objeto)
{
    if (ramitas) *ramitas = s_jl_ram;
    if (musgo) *musgo = s_jl_mus;
    if (objeto) *objeto = s_jl_obj;
}
int pet_place_game_seed(const pet_t *p, int jg, uint32_t t)
{
    uint32_t h = (uint32_t)dia_local(p, t) * 2654435761u ^ (p->id * 2246822519u) ^ ((uint32_t)(jg + 1) * 0x85EBCA6Bu);
    h ^= h >> 15; h *= 0x2C1B3C6Du; h ^= h >> 12;
    return (int)(h & 0x7FFFFFFF);
}
int pet_place_game_result(pet_t *p, int jg, int puntos, int estrellas, uint32_t t)
{
    s_jl_ram = s_jl_mus = 0; s_jl_obj = -1;
    if (!p || (unsigned)jg >= PET_JUEGOS_LUGAR) return 0;
    if (puntos < 0) puntos = 0;
    if (puntos > 60000) puntos = 60000;
    if (estrellas < 0) estrellas = 0;
    if (estrellas > 3) estrellas = 3;
    int r = 0;
    if (p->jl_jugadas < 255) p->jl_jugadas++;
    if (puntos > p->jl_record[jg]) { if (p->jl_record[jg] > 0) r |= PET_JLR_RECORD; p->jl_record[jg] = (uint16_t)puntos; }
    const bool tres_primera = estrellas == 3 && p->jl_estrellas[jg] < 3;
    if (estrellas > p->jl_estrellas[jg]) { r |= PET_JLR_ESTRELLAS; p->jl_estrellas[jg] = (uint8_t)estrellas; }
    const uint8_t hoy = (uint8_t)(1u + ((uint32_t)dia_local(p, t) & 0x7Fu));
    if (p->jl_dia != hoy) { p->jl_dia = hoy; memset(p->jl_premios, 0, sizeof p->jl_premios); }
    if (estrellas >= 1 && p->jl_premios[jg] < PET_JL_PREMIOS_DIA) {
        p->jl_premios[jg]++;
        r |= PET_JLR_PREMIO;
        s_jl_ram = (int8_t)(estrellas >= 3 ? 2 : 1);
        s_jl_mus = (int8_t)(estrellas >= 2 ? 1 : 0);
        for (int k = 0; k < s_jl_ram; k++) guardar_material(p, HALL_RAMITA);
        for (int k = 0; k < s_jl_mus; k++) guardar_material(p, HALL_MUSGO);
    }
    if (tres_primera) {
        const int id = pet_object_find(p, OFU_REGALO, t);
        if (id >= 0 && pet_object_gives(p, id, OFU_REGALO, t)) { s_jl_obj = (int8_t)id; r |= PET_JLR_REGALO; }
    }
    sumar(p, NEED_DIVERSION, 8.0f + 4.0f * (float)estrellas);
    sumar(p, NEED_ENERGIA, -2);
    impulsar(p, EMO_ALEGRIA, 0.3f + 0.15f * (float)estrellas, CAUSA_JUGAMOS);
    if (r & (PET_JLR_RECORD | PET_JLR_ESTRELLAS)) impulsar(p, EMO_ORGULLO, 0.7f, CAUSA_JUGAMOS);
    return r;
}
