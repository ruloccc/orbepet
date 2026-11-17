/* ======================================================================
 * (versión 38) Incluido al final de pet.c.
 *  - El modo libre de los cristales: piezas, la escama del dragón, la propuesta de hacer una canción.
 *  - "Nuestra canción" y el cuaderno (hasta 4 más): el ritmo parejo, la continuación que propone ella, guardar,
 *    usarla en las alarmas o de canción de cuna.
 *  - Los materiales de la casita (ramitas y musgo), el lago helado, el broche que la deja limpia y el polvo de hadas.
 * ==================================================================== */

/* --- materiales de la casita --- */
static void guardar_material(pet_t *p, int hallazgo)
{
    if (hallazgo == HALL_RAMITA && p->ramitas < 99) p->ramitas++;
    if (hallazgo == HALL_MUSGO && p->musgo < 99) p->musgo++;
}
int pet_materials(const pet_t *p, int *ramitas, int *musgo)
{
    if (ramitas) *ramitas = p->ramitas;
    if (musgo) *musgo = p->musgo;
    return p->ramitas >= PET_CASITA_RAMITAS && p->musgo >= PET_CASITA_MUSGO;
}

/* --- efectos nuevos de objetos --- */
#define OBJ_BROCHE_TILDE 115
#define OBJ_POLVO_HADAS 143
#define OBJ_BEBIDA_HELADA 144
#define OBJ_ESCAMA_PRISMA 19
bool pet_lake_frozen(const pet_t *p, uint32_t t) { return pet_season(p, t) == ESTACION_INVIERNO || (p->lago_helado_hasta && t < p->lago_helado_hasta); }
bool pet_floating(const pet_t *p, uint32_t t) { return p->flota_hasta && t < p->flota_hasta && !p->dormida; }
bool pet_always_clean(const pet_t *p, uint32_t t) { return p->limpio_hasta && t < p->limpio_hasta; }
static int pagina_suelta(pet_t *p, uint32_t t, bool *nueva);
static void efectos_v38(pet_t *p, int id, uint32_t t)
{
    switch (id) {
    case OBJ_BROCHE_TILDE: /* un día entero sin ensuciarse (y queda limpia al tiro) */
        p->limpio_hasta = t + 86400u;
        p->barro = 0;
        if (p->necesidad[NEED_HIGIENE] < 90) p->necesidad[NEED_HIGIENE] = 90;
        break;
    case OBJ_POLVO_HADAS: /* flota un poquito sobre el suelo, unos dos minutos */
        if (!p->dormida) p->flota_hasta = t + 120u;
        break;
    case 110: /* (v39) la página suelta del diario: se desdobla y muestra la siguiente página (v39.inc) */
        pagina_suelta(p, t, NULL);
        break;
    case OBJ_BEBIDA_HELADA: /* nieva y se congela el lago unas horas (en invierno ya está helado) */
        if (pet_season(p, t) != ESTACION_INVIERNO) p->lago_helado_hasta = t + 4u * 3600u;
        break;
    default: break;
    }
}

/* --- el modo libre de los cristales --- */
bool pet_song_can_compose(const pet_t *p) { return p->etapa != STAGE_HUEVO && pet_mode(p) != MODO_INFANTE; }
static int cancion_libre(const pet_t *p)
{
    if (!p->canciones[0].n) return 0;
    for (int k = 1; k < PET_CANCIONES; k++) if (!p->canciones[k].n) return k;
    return -1;
}
int pet_crystal_piece(pet_t *p, int notas, int estilo, uint32_t t)
{
    if (p->etapa == STAGE_HUEVO || notas < PET_PIEZA_MIN) return 0;
    p->ultima_interaccion = t;
    estilo &= PIEZA_RAPIDA | PIEZA_LENTA | PIEZA_LARGA | PIEZA_REPETIDA;
    sumar(p, NEED_DIVERSION, (float)(6 + (notas > 40 ? 10 : notas / 4)));
    sumar(p, NEED_CARINO, 6);
    impulsar(p, EMO_ALEGRIA, (estilo & PIEZA_RAPIDA) ? 0.7f : 0.45f, CAUSA_JUGAMOS);
    if (estilo & PIEZA_LENTA) { sumar(p, NEED_SEGURIDAD, 10); p->emo[EMO_TRISTEZA] *= 0.7f; p->emo[EMO_MIEDO] *= 0.7f; }
    if (p->cris_piezas < 0xFFFF) p->cris_piezas++;
    const uint8_t hb = dia_byte(p, t);
    if (p->cris_dia != hb) { p->cris_dia = hb; if (p->cris_dias < 255) p->cris_dias++; }
    const bool dragon = pet_special_known(p, ESP_CRISTALES);
    int res = estilo, regalo = 0;
    if (!(p->cris_flags & 1) && p->cris_dias >= 3 && dragon) { /* varios días tocando: el dragón le regala una escama */
        p->cris_flags |= 1;
        res |= PIEZA_ESCAMA;
        if (dar_objeto(p, OBJ_ESCAMA_PRISMA, OFU_REGALO, t, false)) regalo = OBJ_ESCAMA_PRISMA + 1;
        else { /* ya la tenía: un cristal para su colección */
            int pieza = (int)(rng_next(p) % CRISTALES_AZAR); /* (fw 104: el hielo no sale aquí) */
            if (guardar_pieza(p, COL_CRISTALES, pieza, t, true)) push_event(p, EV_TESORO, 1, COL_CRISTALES * 8 + pieza, t);
        }
        impulsar(p, EMO_ORGULLO, 0.8f, CAUSA_ESPECIAL);
    } else if ((estilo & PIEZA_LARGA) && chance(p, 0.3f)) { /* una pieza larga: a veces un regalo */
        int id = pet_object_find(p, OFU_REGALO, t);
        if (id >= 0 && dar_objeto(p, id, OFU_REGALO, t, false)) { regalo = id + 1; res |= PIEZA_REGALO; }
    }
    /* ¿hacemos una canción? después de tocar juntos varias veces (una vez al día; con canción ya hecha, de vez en cuando) */
    const int libre = cancion_libre(p);
    if (pet_song_can_compose(p) && libre >= 0 && p->cris_propuso_dia != hb &&
        ((libre == 0 && p->cris_piezas >= 3) || (libre > 0 && p->cris_piezas % 6 == 0))) {
        p->cris_propuso_dia = hb;
        res |= PIEZA_CANCION;
        if (dragon && p->cris_dias >= 2) { res |= PIEZA_DRAGON; p->cris_flags |= 2; } /* varios días tocando: lo propone el dragón */
        else p->cris_flags |= 4;
    }
    push_event(p, EV_PIEZA, res, regalo, t);
    vivir(p, VIV_JUGUETE);
    return res;
}

/* --- nuestra canción --- */
static const uint8_t ESCALA_CRIS[6] = {0, 4, 1, 2, 5, 3}; /* los cristales de grave a agudo */
static int grado_cris(int c)
{
    for (int i = 0; i < 6; i++) if (ESCALA_CRIS[i] == c) return i;
    return 0;
}
int pet_song_from_taps(const uint8_t *cristal, const uint16_t *ms, int n, uint8_t *out, int max)
{
    if (n <= 0 || max <= 0) return 0;
    if (n > max) n = max;
    /* el pulso: la mediana de los tiempos entre toques (sin el último, que no tiene siguiente) */
    uint16_t v[PET_CANCION_NOTAS];
    int m = 0;
    for (int i = 0; i < n - 1 && m < PET_CANCION_NOTAS; i++) v[m++] = ms[i];
    for (int i = 1; i < m; i++) for (int j = i; j > 0 && v[j - 1] > v[j]; j--) { uint16_t x = v[j]; v[j] = v[j - 1]; v[j - 1] = x; }
    float pulso = m ? (float)v[m / 2] : 400.0f;
    if (pulso < 120.0f) pulso = 120.0f;
    if (pulso > 1200.0f) pulso = 1200.0f;
    for (int i = 0; i < n; i++) {
        /* cada nota dura lo que pasó hasta el toque siguiente, ordenado a medios pulsos (dos pasos = un pulso) */
        int pasos = i < n - 1 ? (int)((float)ms[i] / pulso * 2.0f + 0.5f) : 4;
        if (pasos == 3) pasos = (float)ms[i] / pulso * 2.0f < 3.0f ? 2 : 4; /* suave: casi todo queda en 1, 2 o 4 pasos */
        if (pasos > 4 && pasos < 8) pasos = pasos < 6 ? 4 : 8;
        out[i] = PET_CANCION_NOTA(cristal[i] % 6, pasos);
    }
    return n;
}
int pet_song_continue(const uint8_t *frase, int n, uint8_t *out, int max, int variante)
{
    if (n <= 0 || max <= 0) return 0;
    int m = n > max ? max : n;
    uint32_t s = 2166136261u ^ (uint32_t)(variante * 16777619);
    for (int i = 0; i < n; i++) s = (s ^ frase[i]) * 16777619u;
    const int g0 = grado_cris(PET_CANCION_CRISTAL(frase[0]) % 6);
    const int sube = (s >> 7) & 1 ? 1 : -1;
    for (int i = 0; i < m; i++) {
        const uint8_t src = frase[i % n];
        int pasos = PET_CANCION_PASOS(src), g;
        switch (((unsigned)variante) % 3) {
        case 0: g = grado_cris(PET_CANCION_CRISTAL(src) % 6) + sube; break;                       /* la misma frase, un escalón más arriba (o abajo) */
        case 1: g = grado_cris(PET_CANCION_CRISTAL(frase[n - 1 - (i % n)]) % 6); break;            /* al revés, como un eco que vuelve */
        default: g = grado_cris(PET_CANCION_CRISTAL(src) % 6) + (i >= m / 2 ? (((s >> (i & 15)) & 1) ? 1 : -1) : 0); break; /* empieza igual y cambia al final */
        }
        if (g < 0) g = 1;
        if (g > 5) g = 4;
        if (i == m - 1) { g = g0; if (pasos < 4) pasos = 4; } /* termina en casa, con una nota larga */
        out[i] = PET_CANCION_NOTA(ESCALA_CRIS[g], pasos);
    }
    return m;
}
int pet_song_save(pet_t *p, int cual, const uint8_t *notas, int n, const char *nombre, uint32_t t)
{
    if (!notas || n <= 0 || p->etapa == STAGE_HUEVO) return -1;
    if (n > PET_CANCION_NOTAS) n = PET_CANCION_NOTAS;
    if (cual < 0) cual = cancion_libre(p);
    if (cual < 0 || cual >= PET_CANCIONES) return -1;
    cancion_t *c = &p->canciones[cual];
    memset(c, 0, sizeof *c);
    c->n = (uint8_t)n;
    for (int i = 0; i < n; i++) c->nota[i] = PET_CANCION_NOTA(PET_CANCION_CRISTAL(notas[i]) % 6, PET_CANCION_PASOS(notas[i]));
    if (nombre && *nombre) {
        int k = 0;
        for (; nombre[k] && k < PET_CANCION_NOMBRE - 1; k++) c->nombre[k] = nombre[k];
        while (k > 0 && ((unsigned char)c->nombre[k - 1] & 0xC0) == 0xC0) c->nombre[--k] = 0; /* (no deja una letra con tilde a medias) */
    } else snprintf(c->nombre, sizeof c->nombre, cual == 0 ? "NUESTRA CANCIÓN" : "CANCIÓN %d", cual + 1);
    p->ultima_interaccion = t;
    impulsar(p, EMO_ALEGRIA, 1.0f, CAUSA_JUGAMOS);
    impulsar(p, EMO_CARINO, 0.8f, CAUSA_JUGAMOS);
    impulsar(p, EMO_ORGULLO, 0.8f, CAUSA_JUGAMOS);
    subir_vinculo(p, 1.0f);
    anotar(p, DIA_NUESTRA, cual, n, t);
    push_event(p, EV_NUESTRA, cual, n, t);
    return cual;
}
const cancion_t *pet_song(const pet_t *p, int k) { return (unsigned)k < PET_CANCIONES && p->canciones[k].n ? &p->canciones[k] : NULL; }
int pet_song_count(const pet_t *p)
{
    int n = 0;
    for (int k = 0; k < PET_CANCIONES; k++) n += p->canciones[k].n > 0;
    return n;
}
void pet_song_delete(pet_t *p, int k)
{
    if ((unsigned)k >= PET_CANCIONES) return;
    memset(&p->canciones[k], 0, sizeof p->canciones[k]);
    if (p->cancion_alarma == k + 1) p->cancion_alarma = 0;
    if (p->cancion_cuna == k + 1) p->cancion_cuna = 0;
}
void pet_song_use(pet_t *p, int k, int para)
{
    uint8_t v = (uint8_t)(pet_song(p, k) ? k + 1 : 0);
    if (para == 0) p->cancion_alarma = v; else p->cancion_cuna = v;
}
int pet_song_for(const pet_t *p, int para)
{
    int k = (para == 0 ? p->cancion_alarma : p->cancion_cuna) - 1;
    return pet_song(p, k) ? k : -1;
}
