/* ======================================================================
 * (versión 39) Incluido al final de pet.c, después de musica.inc.
 *  - Pochi en la historia: una charla por visita (3 por acto); después del segundo final, le cuenta el nombre.
 *  - La página suelta del diario (objeto 110): muestra la siguiente página que le toca (cuenta como momento de historia).
 *  - El segundo final, «El último verso»: cuándo empieza, el verso de Tilo y su canción terminada.
 * ==================================================================== */

/* --- Pochi (la mamá chihuahua) pregunta como una niña curiosa; ella responde con lo que ya sabe (según el acto). Nunca el secreto del huevo. --- */
static const char *const POCHI_CHARLA[PET_POCHI_CHARLAS + 1][3] = {
    /* acto 1: anécdotas */
    {"¿QUIÉN ERA EL CUIDADOR? ¿UN PERRO?", "NO, POCHI. CUIDABA TODO EL MUNDO: LAS PLANTAS, LAS NUBES, LAS CRIATURAS...", "¡ENTONCES ERA UN PERRO MUY GRANDE! ¡GUAU!"},
    {"¿Y QUÉ COMÍA EL CUIDADOR? ¿PAPAS?", "GALLETAS DE PIÑÓN CON MIEL DE ULMO. SIEMPRE SE LE QUEMABAN, Y DECÍA QUE «ASÍ VENÍAN».", "¿QUEMADAS? ¡YO ME LAS COMO IGUAL! ÑAM."},
    {"¿ES VERDAD QUE EL CUIDADOR CANTABA?", "¡SÍ! MUY DESAFINADO... PERO CON MUCHAS GANAS.", "¡AUUUU! ¡YO TAMBIÉN CANTO ASÍ!"},
    /* acto 2: recuerdos que no calzan */
    {"¿EL CUIDADOR ERA UN GIGANTE?", "MUSGUITO DICE QUE SÍ... PERO EL CONEJO DICE QUE NO ERA MÁS ALTO QUE ÉL.", "¿UN GIGANTE CHIQUITO? ¡ME MAREO!"},
    {"¿POR QUÉ CADA UNO LO RECUERDA DISTINTO?", "CREO QUE CADA UNO LO CONOCIÓ CUANDO MÁS LO NECESITABA.", "¡A MÍ ME CONOCISTE CUANDO TENÍA HAMBRE!"},
    {"¿Y DÓNDE ESTÁ SU CUCHARA DE PALO?", "SE LE PERDIÓ... DICEN QUE DONDE BAILAN LAS HADAS.", "¡YO LA BUSCO CON MI NARIZ! SNIF, SNIF... ¿PAPAS?"},
    /* acto 3: su defecto y su enfermedad */
    {"¿Y QUIÉN CUIDABA AL CUIDADOR?", "NADIE, POCHI. NUNCA DEJÓ QUE LO AYUDARAN.", "¡YO SIEMPRE PIDO AYUDA! SOBRE TODO CON LAS PAPAS."},
    {"¿POR QUÉ DEJÓ DE CUIDAR?", "SE ENFERMÓ. Y NO SE LO CONTÓ A NADIE.", "CUANDO MIS CACHORROS SE ENFERMAN, YO LOS ABRAZO A TODOS... ¿A ÉL QUIÉN LO ABRAZÓ?"},
    {"¿Y DÓNDE ESTÁ AHORA EL CUIDADOR?", "SE FUE CON NIMBA A MIRAR LAS ESTRELLAS DE CERCA.", "ENTONCES LE LADRO AL CIELO: ¡GUAU! PARA QUE SEPA QUE VINE."},
    /* después del segundo final: el nombre */
    {"¿YA SABES CÓMO SE LLAMABA EL CUIDADOR?", "SÍ. SE LLAMABA PEUMO. TÚ ERES LA PRIMERA EN SABERLO.", "¡PEUMO! ¡PEUMO! ¡SE LO VOY A LADRAR A MIS CACHORROS PARA QUE NUNCA SE OLVIDE!"},
};
static const char *const POCHI_CHARLA_CORTA[PET_POCHI_CHARLAS + 1] = { /* lo que ella responde en modo niño */
    "NO, POCHI. ÉL CUIDABA TODO EL MUNDO.", "GALLETAS... ¡SIEMPRE QUEMADAS!", "¡SÍ! MUY DESAFINADO.",
    "UNOS DICEN QUE ERA GIGANTE. OTROS, QUE ERA CHIQUITO.", "CADA UNO LO CONOCIÓ CUANDO LO NECESITABA.", "SE LE PERDIÓ DONDE BAILAN LAS HADAS.",
    "NADIE. NO DEJABA QUE LO AYUDARAN.", "SE ENFERMÓ, Y NO LE DIJO A NADIE.", "SE FUE CON NIMBA, A LAS ESTRELLAS.",
    "SE LLAMABA PEUMO. ¡TÚ ERES LA PRIMERA EN SABERLO!",
};
const char *pet_pochi_chat(int k, int linea, bool corto)
{
    if ((unsigned)k > PET_POCHI_CHARLAS || (unsigned)linea > 2) return "";
    if (corto && linea == 1) return POCHI_CHARLA_CORTA[k];
    return POCHI_CHARLA[k][linea];
}
bool pet_pochi_knows_name(const pet_t *p) { return (p->pochi_charla >> 6) & 1u; }
int pet_pochi_chat_next(const pet_t *p)
{
    if (p->etapa == STAGE_HUEVO) return -1;
    if (pet_final_seen(p, PET_FINAL_SECRETO)) return pet_pochi_knows_name(p) ? -1 : PET_POCHI_NOMBRE;
    const int acto = pet_story_act(p);
    const int hechas = (p->pochi_charla >> ((acto - 1) * 2)) & 3;
    return hechas < 3 ? (acto - 1) * 3 + hechas : -1;
}
void pet_pochi_chat_done(pet_t *p, int k, uint32_t t)
{
    if (k == PET_POCHI_NOMBRE) { p->pochi_charla |= 1u << 6; impulsar(p, EMO_CARINO, 0.8f, CAUSA_CARICIAS); return; }
    if ((unsigned)k >= PET_POCHI_CHARLAS) return;
    const int acto = k / 3, sh = acto * 2;
    int hechas = (p->pochi_charla >> sh) & 3;
    if (hechas <= k % 3 && hechas < 3) hechas = k % 3 + 1;
    p->pochi_charla = (uint8_t)((p->pochi_charla & ~(3u << sh)) | ((unsigned)hechas << sh));
    p->ultima_interaccion = t;
    sumar(p, NEED_CARINO, 4);
    impulsar(p, EMO_ALEGRIA, 0.4f, CAUSA_JUGAMOS);
}

/* --- la página suelta del diario: se desdobla y muestra la siguiente página que todavía no tenga (del acto en que va) --- */
static const uint8_t PAGINA_ACTO[PET_TILO_PAGINAS] = {1, 1, 1, 2, 2, 2, 3, 99, 99}; /* la del huevo (7) y la carta (8), nunca por aquí */
#define OBJ_PAGINA_SUELTA 110
static int pagina_suelta(pet_t *p, uint32_t t, bool *nueva)
{
    if (nueva) *nueva = false;
    if (p->etapa == STAGE_HUEVO) return -1;
    const int acto = pet_story_act(p);
    for (int k = 0; k < PET_TILO_PAGINAS; k++) {
        if (pet_tilo_page(p, k) || PAGINA_ACTO[k] > acto) continue;
        if (!momento_libre(p, t)) break; /* hoy ya no quedan momentos: muestra una ya leída */
        momento_usar(p, t);
        tilo_pagina(p, k, t); /* (aparece sola, con su animación: pagina_revisar en la interfaz) */
        if (nueva) *nueva = true;
        return k;
    }
    int ya[PET_TILO_PAGINAS], n = 0;
    for (int k = 0; k < PET_TILO_PAGINAS; k++) if (pet_tilo_page(p, k)) ya[n++] = k;
    const int k = n ? ya[rng_next(p) % (uint32_t)n] : -1;
    push_event(p, EV_PAGINA, k, 0, t);
    return k;
}
int pet_loose_page(pet_t *p, uint32_t t, bool *nueva) { return pagina_suelta(p, t, nueva); }

/* --- el segundo final --- */
bool pet_final2_ready(const pet_t *p, uint32_t t)
{
    if (p->dormida || p->etapa == STAGE_HUEVO || p->final_pend || !p->guardiana || (p->final_estado & PET_FINAL_SECRETO)) return false;
    if (dia_local(p, t) <= p->dia_guardiana) return false; /* después del rito: otro día */
    if (p->escena != ESC_CUEVA) return false;
    const int h = hora_local(p, t);
    if (!(h >= 19 || h < 6)) return false;
    const clima_t c = pet_weather(p);
    if (c == CLIMA_LLUVIA || c == CLIMA_TORMENTA || c == CLIMA_NIEVE) return false; /* una noche despejada */
    return p->canciones[0].n > 0 || pet_melody(p, NULL) > 0;
}
/* su verso, desafinado (la quinta nota se le va de tono) y cortado antes del final */
static const uint8_t TILO_VERSO[] = {PET_CANCION_NOTA(0, 2), PET_CANCION_NOTA(4, 2), PET_CANCION_NOTA(1, 2), PET_CANCION_NOTA(2, 4),
                                     PET_CANCION_NOTA(5, 2), PET_CANCION_NOTA(1, 2), PET_CANCION_NOTA(4, 4)};
#define TILO_VERSO_N ((int)sizeof TILO_VERSO)
int pet_tilo_verse(uint8_t *out, int max)
{
    int n = TILO_VERSO_N < max ? TILO_VERSO_N : max;
    if (out) memcpy(out, TILO_VERSO, (size_t)n);
    return n;
}
/* la canción de Tilo: su verso, nuestra canción encima, y la última nota que afinó Prisma (do, larga) */
int pet_tilo_song(const pet_t *p, uint8_t *out, int max)
{
    if (!(p->final_estado & PET_FINAL_SECRETO) || max < 2) return 0;
    int n = 0;
    for (int i = 0; i < TILO_VERSO_N - 1 && n < max - 1; i++) out[n++] = TILO_VERSO[i];
    if (p->canciones[0].n) {
        for (int i = 0; i < p->canciones[0].n && n < max - 1 && i < 6; i++) out[n++] = p->canciones[0].nota[i];
    } else {
        uint8_t m[PET_MELODIA_NOTAS];
        const int nm = pet_melody(p, m);
        for (int i = 0; i < nm && n < max - 1 && i < 6; i++) out[n++] = PET_CANCION_NOTA(m[i], 2);
    }
    out[n++] = PET_CANCION_NOTA(0, 8);
    return n;
}

/* --- el Libro de historias: la estrellita del narrador con algo nuevo --- */
bool pet_book_new(const pet_t *p, int c) { return (unsigned)c < 16 && ((p->lib_nuevo[c >> 3] >> (c & 7)) & 1u); }
void pet_book_seen(pet_t *p, int c) { if ((unsigned)c < 16) p->lib_nuevo[c >> 3] &= (uint8_t)~(1u << (c & 7)); }
void pet_in_zone(pet_t *p, bool si) { p->en_zona = si ? 1 : 0; }
