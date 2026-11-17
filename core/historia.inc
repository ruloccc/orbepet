/* ======================================================================
 * La historia del mundo (versión 37, biblia v2). Lo incluye pet.c al final (usa sus ayudantes).
 *  - Todo avanza por días jugados (días con al menos un rato de juego). Si no se juega, la historia espera.
 *  - Tope de PET_HISTORIAS_DIA momentos de historia por día: historias de criaturas y de Nimba, capítulos de Prisma,
 *    chismes de Tilde y recuerdos de la Abuela Brizna. (Las frases de los animales y las páginas del diario no cuentan.)
 *  - Semanas: 1 historias 1; 2 historias 2 y 3; 3 historias 4 y Prisma; 4 historias 5, Nimba 5, Brizna 4-5; 5 el rito y el final.
 * ==================================================================== */

/* --- los lugares secretos: lo que se encuentra en cada uno --- */
static const char *const LUGAR_NOMBRE[PET_LUGARES] = {"La casita escondida", "El mirador de las nubes"};
static const historia_t LUGAR_COSAS[PET_LUGARES][4] = { /* (v99) solo lo que el dibujo no cuenta: lo que se ve, se ve */
    {{"El horno", "«Así venían», decía él. A él le gustaban así.", "«Así venían», decía él."},
     {"La camita", "Alguien de su tamaño dormía aquí.", "Alguien de su tamaño dormía aquí."},
     {"El gancho vacío", "Aquí iba su cuchara. Se perdió donde bailan las hadas.", "Su cuchara se perdió donde bailan las hadas."},
     {"La mesa", "Una taza para cada criatura. La más chiquita era la suya.", "La taza chiquita era la suya."}},
    {{"La piedra tibia", "Alguien se sentaba aquí todas las tardes a mirar su mundo.", "Alguien se sentaba aquí a mirar."},
     {"La corona de hojas", "Se le cayó a alguien que se reía mucho.", "Se le cayó a alguien que se reía mucho."},
     {"La nube que baja", "La última noche, alguien subió aquí al lomo de Nimba.", "Alguien subió aquí al lomo de Nimba."},
     {NULL, NULL, NULL}},
};

/* --- días jugados, actos y momentos del día --- */
int pet_days_played(const pet_t *p) { return p->dias_jugados ? p->dias_jugados : 1; }
int pet_story_act(const pet_t *p) { const int d = pet_days_played(p); return d < 11 ? 1 : (d < 22 ? 2 : 3); }
static void momentos_al_dia(pet_t *p, uint32_t t)
{
    const uint8_t hoy = (uint8_t)(dia_local(p, t) & 0xFF);
    if (p->hist_dia != hoy) { p->hist_dia = hoy; p->hist_hoy = 0; }
}
int pet_story_moments_left(const pet_t *p, uint32_t t)
{
    const uint8_t hoy = (uint8_t)(dia_local(p, t) & 0xFF);
    const int usados = p->hist_dia == hoy ? p->hist_hoy : 0;
    return usados >= PET_HISTORIAS_DIA ? 0 : PET_HISTORIAS_DIA - usados;
}
static bool momento_libre(pet_t *p, uint32_t t) { momentos_al_dia(p, t); return p->hist_hoy < PET_HISTORIAS_DIA; }
static void momento_usar(pet_t *p, uint32_t t) { momentos_al_dia(p, t); if (p->hist_hoy < 255) p->hist_hoy++; }
/* desde qué día jugado se puede contar la historia k de una criatura o de Nimba */
static int historia_desde(int k) { static const uint8_t D[5] = {1, 8, 11, 15, 22}; return (unsigned)k < 5 ? D[k] : 99; }
static bool historia_abierta(const pet_t *p, int k) { return pet_days_played(p) >= historia_desde(k); }
/* cuenta un día jugado: el primer rato de juego de cada día */
static void paso_dias_jugados(pet_t *p, uint32_t t)
{
    const int32_t hoy = dia_local(p, t);
    if (p->dia_jugado == hoy || p->etapa == STAGE_HUEVO) return;
    if (!p->ultima_interaccion || dia_local(p, p->ultima_interaccion) != hoy) return; /* hoy todavía no jugaron */
    p->dia_jugado = hoy;
    if (p->dias_jugados < 60000) p->dias_jugados++;
}

/* --- (fw 104) los papelitos del bosque: cartas de Peumo y pistas, escondidos como tesoro del día --- */
static const historia_t PAPELITOS[PET_PAPELITOS] = {
    {"La letra del rey", "Un papelito con letra antigua: «Si encontraste esto, ya sabes buscar despacio. Así cuidaba yo el bosque: mirando dos veces donde todos miran una. Firma: el Rey del claro»", "Dice: «Buscaste despacio. Así se cuida el bosque. Firma: el Rey del claro»"},
    {"El tocón", "«Este tocón era mi árbol favorito antes de ser casa. No me dio pena: un árbol que se vuelve casa sigue dando sombra por dentro. Firma: el Rey del claro»", "«Mi casa era mi árbol favorito. Ahora da sombra por dentro. Firma: el Rey del claro»"},
    {"La poza", "«La poza del claro no es honda, pero guarda el cielo entero. Cuando extrañes a alguien, mírala: todos miramos el mismo cielo. Firma: el Rey del claro»", "«La poza guarda el cielo. Si extrañas a alguien, míralo ahí. Firma: el Rey del claro»"},
    {"Receta a medias", "Una esquina de papel quemada: «...piñones molidos, miel de ulmo y esperar sin abrir el horno. Lo difícil nunca es la masa: es esperar...». Huele a galletas.", "Un pedacito de la receta de las galletas. Huele rico."},
    {"El cóndor", "Una pista con dibujos de plumas: «El cóndor guarda cosas brillantes en su roca cuando va a llover. Él cree que nadie lo ve. Todos lo vemos».", "«El cóndor esconde cositas en su roca cuando va a llover»."},
    {"Chu-cao", "«Si el chucao canta a tu derecha, sigue; si canta a tu izquierda, vuelve mañana. Él siempre canta al lado de lo que importa». Alguien agregó con otra letra: «o al lado del musgo rico».", "«El chucao canta cerquita de lo que importa»."},
    {"Para Tilde", "«Tilde: te dejé un botón celeste bajo la piedra plana. No es por nada. Bueno, sí: gracias por los chismes. Firma: P.» El botón ya no está.", "Una cartita para Tilde. El regalo ya no está: se lo llevó."},
    {"La apacheta", "«Cada piedra de la apacheta la dejó alguien que subió hasta la cima. Pon una tú también: el cerro se acuerda de todas las manos». La letra está borrosa por el viento.", "«Cada piedra de la torre la puso un visitante. Pon una tú»."},
    {"El dragón ronca", "Una pista con olor a cueva: «Si el dragón de cristal ronca, es que sueña con música. Tócale una canción bajita y a veces suelta un secreto de más».", "«Si el dragón ronca, tócale musiquita bajita»."},
    {"La nube bajita", "«La nube del cerro no es floja: espera. Las nubes que esperan son las que saben adónde ir. Súbete sin miedo. Firma: P.»", "«La nube del cerro espera para llevarte arriba. Firma: P.»"},
    {"Las luciérnagas", "«Las luciérnagas no alumbran el camino: alumbran la compañía. Por eso andan en grupo. Por eso brillan más cuando vuelves con alguien. Firma: el Rey del claro»", "«Las luciérnagas brillan más cuando no vas solo. Firma: el Rey del claro»"},
    {"El último papelito", "«Ya juntaste todos mis papelitos. Te cuento el secreto de verdad: nunca escondí nada. Solo quería que caminaras el bosque entero. Funcionó, ¿cierto? Firma: el Rey del claro»", "«No escondí nada: quería que pasearas por todo el bosque. Firma: el Rey del claro»"},
};
/* ¿hoy el escondite puede traer un papelito? (2-3 por semana, separados al menos un día de por medio) */
static int papelito_proximo(const pet_t *p)
{
    for (int k = 0; k < PET_PAPELITOS; k++) if (!((p->papelitos >> k) & 1u)) return k;
    return -1;
}
static bool papelito_toca(const pet_t *p, uint32_t t)
{
    if (p->etapa == STAGE_HUEVO || pet_mode(p) == MODO_INFANTE || papelito_proximo(p) < 0) return false;
    if (pet_days_played(p) < 10) return false; /* primero que conozca bien los escondites normales */
    const int32_t dia = dia_local(p, t);
    const uint8_t sem = (uint8_t)(1u + (((uint32_t)dia / 7u) & 0x7Fu));
    const uint8_t nsem = p->carta_sem == sem ? p->carta_sem_n : 0;
    if (nsem >= 3) return false;
    const uint8_t db = (uint8_t)(1u + ((uint32_t)dia & 0x7Fu));
    if (p->papel_dia == db) return false;                                  /* hoy ya salió */
    if (p->papel_dia == (uint8_t)(1u + (((uint32_t)dia - 1u) & 0x7Fu))) return false; /* ayer salió: un día de por medio */
    uint32_t h = (uint32_t)dia * 2246822519u ^ (p->id * 668265263u);
    h ^= h >> 13; h *= 0x5BD1E995u; h ^= h >> 15;
    return (h % 100u) < 48u; /* ~2,5 por semana con el tope de 3 */
}
static void papelito_anotar(pet_t *p, int k, uint32_t t)
{
    if ((unsigned)k >= PET_PAPELITOS || ((p->papelitos >> k) & 1u)) return;
    p->papelitos |= (uint16_t)(1u << k);
    const int32_t dia = dia_local(p, t);
    const uint8_t sem = (uint8_t)(1u + (((uint32_t)dia / 7u) & 0x7Fu));
    if (p->carta_sem != sem) { p->carta_sem = sem; p->carta_sem_n = 0; }
    if (p->carta_sem_n < 255) p->carta_sem_n++;
    p->papel_dia = (uint8_t)(1u + ((uint32_t)dia & 0x7Fu));
    anotar(p, DIA_HISTORIA, NARR_PAPELITOS, k, t); /* la estrellita del Libro incluida */
    sumar(p, NEED_CURIOSIDAD, 15);
    impulsar(p, EMO_CURIOSIDAD, 0.7f, CAUSA_HALLAZGO);
    impulsar(p, EMO_ALEGRIA, 0.4f, CAUSA_HALLAZGO);
}

/* (fw 104, pruebas) entrega el próximo papelito ya (como si lo encontraras): -1 si ya están todos */
int pet_papelito_test(pet_t *p, int lo_trajo_ella, uint32_t t)
{
    const int k = papelito_proximo(p);
    if (k < 0) return -1;
    papelito_anotar(p, k, t);
    push_event(p, EV_CARTA_BOSQUE, k, lo_trajo_ella ? 1 : 0, t);
    return k;
}

/* --- los narradores del Libro de historias --- */
int pet_narr_count(int narr)
{
    if (narr >= 0 && narr <= CRI_NIMBA) return 5;
    switch (narr) {
    case NARR_PRISMA: return PET_PRISMA_CAPS;
    case NARR_TILDE: return PET_TILDE_CHISMES;
    case NARR_BRIZNA: return PET_BRIZNA_RECUERDOS;
    case NARR_TILO: return PET_TILO_PAGINAS;
    case NARR_ANIMALES: return PET_ANIMAL_FRASES;
    case NARR_PAPELITOS: return PET_PAPELITOS;
    default: return 0;
    }
}
const char *pet_narr_name(int narr)
{
    if ((unsigned)narr < CRI_COUNT) return CRIATURA_NOMBRE[narr];
    switch (narr) {
    case CRI_NIMBA: return "Nimba";
    case NARR_PRISMA: return "Prisma";
    case NARR_TILDE: return "Tilde";
    case NARR_BRIZNA: return "La Abuela Brizna";
    case NARR_TILO: return "Páginas sueltas";
    case NARR_ANIMALES: return "Lo que dicen los animales";
    case NARR_PAPELITOS: return "Papelitos del bosque";
    default: return "?";
    }
}
static const historia_t *narr_historia(int c, int k)
{
    if (k < 0 || k >= pet_narr_count(c)) return NULL;
    if ((unsigned)c < CRI_COUNT) return &HISTORIAS[c][k];
    switch (c) {
    case CRI_NIMBA: return &NIMBA_HISTORIAS[k];
    case NARR_PRISMA: return &PRISMA_CAPITULOS[k];
    case NARR_TILDE: return &TILDE_CHISMES[k];
    case NARR_BRIZNA: return &BRIZNA_RECUERDOS[k];
    case NARR_TILO: return &TILO_PAGINAS[k];
    case NARR_PAPELITOS: return &PAPELITOS[k];
    default: return NULL;
    }
}
const char *pet_story_title(magica_t c, int k)
{
    if ((int)c == NARR_ANIMALES) return (unsigned)k < PET_ANIMAL_FRASES ? pet_card_name(FRASES_ANIMAL[k].ficha) : "";
    const historia_t *h = narr_historia((int)c, k);
    return h ? h->titulo : "";
}
const char *pet_story_text(magica_t c, int k)
{
    if ((int)c == NARR_ANIMALES) return (unsigned)k < PET_ANIMAL_FRASES ? FRASES_ANIMAL[k].txt : "";
    const historia_t *h = narr_historia((int)c, k);
    return h ? h->txt : "";
}
const char *pet_story_short(int c, int k)
{
    if (c == NARR_ANIMALES) return (unsigned)k < PET_ANIMAL_FRASES ? FRASES_ANIMAL[k].corto : "";
    const historia_t *h = narr_historia(c, k);
    return h ? h->corto : "";
}
int pet_story_count(const pet_t *p, int c)
{
    int n = 0;
    for (int k = 0; k < pet_narr_count(c); k++) n += pet_story_known(p, c, k);
    return n;
}
bool pet_story_known(const pet_t *p, int c, int k)
{
    if (k < 0 || k >= pet_narr_count(c)) return false;
    if ((unsigned)c < CRI_COUNT) return (p->historias[c] >> k) & 1u;
    switch (c) {
    case CRI_NIMBA: return (p->historias_nimba >> k) & 1u;
    case NARR_PRISMA: return (p->prisma_caps >> k) & 1u;
    case NARR_TILDE: return (p->tilde_chismes >> k) & 1u;
    case NARR_BRIZNA: return (p->brizna >> k) & 1u;
    case NARR_TILO: return k == 8 ? (p->tilo_ultima & 1u) : (p->tilo_paginas >> k) & 1u;
    case NARR_ANIMALES: return (p->animal_frases[k >> 5] >> (k & 31)) & 1u;
    case NARR_PAPELITOS: return (p->papelitos >> k) & 1u;
    default: return false;
    }
}
const char *pet_creature_after(magica_t c) { return (unsigned)c < CRI_COUNT ? CRIATURA_DESPUES[c] : ""; }

/* --- las páginas del diario de Tilo --- */
bool pet_tilo_page(const pet_t *p, int k) { return k == 8 ? (p->tilo_ultima & 1u) != 0 : ((unsigned)k < 8 && ((p->tilo_paginas >> k) & 1u)); }
int pet_tilo_page_new(const pet_t *p)
{
    for (int k = 0; k < 8; k++) if (((p->tilo_paginas & ~p->tilo_vistas) >> k) & 1u) return k;
    return -1; /* (la 8, la carta del segundo final, la muestra el final mismo) */
}
void pet_tilo_page_seen(pet_t *p, int k)
{
    if (k == 8) p->tilo_ultima |= 2u;
    else if ((unsigned)k < 8) p->tilo_vistas |= (uint8_t)(1u << k);
}
static void tilo_pagina(pet_t *p, int k, uint32_t t)
{
    if ((unsigned)k >= PET_TILO_PAGINAS || pet_tilo_page(p, k)) return;
    if (k == 8) p->tilo_ultima |= 3u; /* la carta: la muestra el final (queda vista) */
    else p->tilo_paginas |= (uint8_t)(1u << k);
    if (pet_mode(p) == MODO_INFANTE && k < 8) p->tilo_vistas |= (uint8_t)(1u << k); /* (los más chicos no leen: queda en el Libro sin aparecer) */
    anotar(p, DIA_HISTORIA, NARR_TILO, k, t);
    push_event(p, EV_TILO, k, 0, t);
    sumar(p, NEED_CURIOSIDAD, 15);
    impulsar(p, EMO_CURIOSIDAD, 0.6f, CAUSA_ESPECIAL);
}

/* --- Prisma, el dragón de cristal: al tocarlo en la cueva, un capítulo por día (desde el día 15; los del 5 en adelante, desde el 22) --- */
/* (101b) ¿tocarlo AHORA le dejaría un capítulo? (sin cambiar nada): la música de la cueva anunciaba «quiere contarte algo» aunque hoy
 * no tocaba ninguno (ya contó hoy, no quedaban momentos de historia o todavía no es el día) y en el Libro no aparecía nada */
bool pet_prisma_ready(const pet_t *p, uint32_t t)
{
    if (!p || p->etapa == STAGE_HUEVO || !pet_special_known(p, ESP_CRISTALES) || pet_mode(p) == MODO_INFANTE) return false;
    if (p->prisma_dia == dia_byte(p, t) || pet_story_moments_left(p, t) <= 0) return false;
    for (int k = 0; k < PET_PRISMA_CAPS; k++) {
        if ((p->prisma_caps >> k) & 1u) continue;
        return pet_days_played(p) >= (k < 4 ? 15 : 22);
    }
    return false;
}
int pet_prisma_touch(pet_t *p, uint32_t t)
{
    if (p->etapa == STAGE_HUEVO || !pet_special_known(p, ESP_CRISTALES) || pet_mode(p) == MODO_INFANTE) return -1;
    p->ultima_interaccion = t;
    impulsar(p, EMO_ALEGRIA, 0.4f, CAUSA_ESPECIAL);
    if (p->prisma_dia == dia_byte(p, t) || !momento_libre(p, t)) return -1;
    for (int k = 0; k < PET_PRISMA_CAPS; k++) {
        if ((p->prisma_caps >> k) & 1u) continue;
        if (pet_days_played(p) < (k < 4 ? 15 : 22)) return -1;
        p->prisma_caps |= (uint8_t)(1u << k);
        p->prisma_dia = dia_byte(p, t);
        momento_usar(p, t);
        anotar(p, DIA_HISTORIA, NARR_PRISMA, k, t);
        push_event(p, EV_PRISMA, k, 0, t);
        sumar(p, NEED_CURIOSIDAD, 12);
        return k;
    }
    return -1;
}

/* --- Tilde, la tenca --- */
bool pet_tilde_here(const pet_t *p, uint32_t t) { return p->tilde_hasta && t < p->tilde_hasta && p->escena == p->tilde_escena && p->etapa != STAGE_HUEVO; }
int pet_tilde_gossip(const pet_t *p) { return p->tilde_chisme == 0xFF ? -1 : (p->tilde_chisme & 0x7F) % PET_TILDE_CHISMES; }
int pet_tilde_takes(const pet_t *p) { return p->tilde_toma ? p->tilde_toma - 1 : -1; }
int pet_tilde_returns(const pet_t *p) { return p->tilde_trae ? p->tilde_trae - 1 : -1; }
const char *pet_tilde_note(const pet_t *p, char *buf, int n)
{
    const int id = pet_tilde_returns(p);
    const objeto_info_t *o = objeto_info(id);
    char nombre[48];
    snprintf(nombre, sizeof nombre, "%s", o ? o->nombre : "cosa brillante");
    if (nombre[0] >= 'A' && nombre[0] <= 'Z') nombre[0] = (char)(nombre[0] - 'A' + 'a');
    const char *s = TILDE_NOTAS[(p->tilde_nota + N_TILDE_NOTAS - 1) % N_TILDE_NOTAS];
    int k = 0;
    while (*s && k < n - 1) {
        if (s[0] == '{' && s[1] == 'O' && s[2] == '}') { for (const char *r = nombre; *r && k < n - 1;) buf[k++] = *r++; s += 3; }
        else buf[k++] = *s++;
    }
    buf[k] = 0;
    return buf;
}
/* lo que se puede llevar: algo brillante y común de la bolsa (nunca lo valioso) */
static int tilde_elige_objeto(pet_t *p)
{
    int cand[24], n = 0;
    for (int i = 0; i < PET_OBJETOS && n < 24; i++) {
        const objeto_info_t *o = objeto_info(i);
        if (!p->bolsa[i] || i == OBJ_CORAZON_BOSQUE || i == OBJ_CANDADO_RUNICO || i == OBJ_LLAVE_OXIDADA || i == pet_guarded_object(p)) continue;
        if (o->rareza != 1 && o->rareza != 2) continue;
        if (o->familia == OFA_GEMA || o->familia == OFA_CRISTAL || o->familia == OFA_DISCO || o->familia == OFA_ORBE || o->familia == OFA_AMULETO) cand[n++] = i;
    }
    return n ? cand[rng_next(p) % (uint32_t)n] : -1;
}
static int tilde_proximo_chisme(const pet_t *p)
{
    const int lim = p->guardiana ? 12 : (p->criaturas_vistas == (1u << CRI_COUNT) - 1u ? 11 : 8);
    for (int k = 0; k < lim; k++) if (!((p->tilde_chismes >> k) & 1u)) return k;
    return -1;
}
static void tilde_llega(pet_t *p, uint32_t t)
{
    const bool chico = pet_mode(p) == MODO_INFANTE;
    p->tilde_hasta = t + 4u * 60u;
    p->tilde_escena = p->escena;
    p->tilde_oido = 0;
    p->tilde_trae = p->tilde_toma = 0;
    if (p->tilde_visitas < 255) p->tilde_visitas++;
    p->tilde_prox = (uint16_t)(pet_days_played(p) + 2); /* (v39) vuelve a los 2 días jugados */
    p->tilde_dia = dia_byte(p, t);
    /* el chisme: uno nuevo si hoy queda un momento de historia; si no, uno que ya contó */
    int k = chico ? -1 : tilde_proximo_chisme(p);
    if (k >= 0 && !momento_libre(p, t)) k = -1;
    if (k >= 0) p->tilde_chisme = (uint8_t)k;
    else if (!chico && p->tilde_chismes) { int r; do r = (int)(rng_next(p) % PET_TILDE_CHISMES); while (!((p->tilde_chismes >> r) & 1u)); p->tilde_chisme = (uint8_t)(r | 0x80); }
    else p->tilde_chisme = 0xFF;
    /* devuelve lo que se llevó la otra vez (con una nota) o, a veces, se lleva algo brillante */
    if (p->tilde_lleva) {
        const int id = p->tilde_lleva - 1;
        p->tilde_lleva = 0;
        if (p->bolsa[id] < 255) p->bolsa[id]++;
        p->tilde_trae = (uint8_t)(id + 1);
        if (p->tilde_nota < 255) p->tilde_nota++;
        anotar(p, DIA_TILDE, 1, (int8_t)id, t);
    } else if (!chico && p->tilde_visitas > 1 && chance(p, 0.45f)) {
        const int id = tilde_elige_objeto(p);
        if (id >= 0) {
            sacar_de_bolsa(p, id);
            p->tilde_toma = (uint8_t)(id + 1);
            p->tilde_lleva = (uint8_t)(id + 1);
            anotar(p, DIA_TILDE, 0, (int8_t)id, t);
        }
    }
    push_event(p, EV_TILDE, 0, 0, t);
    impulsar(p, EMO_CURIOSIDAD, 0.6f, CAUSA_NOVEDAD);
    sonar(p, SND_RISA, t);
}
void pet_tilde_touch(pet_t *p, int que, uint32_t t)
{
    if (!pet_tilde_here(p, t)) return;
    p->ultima_interaccion = t;
    impulsar(p, EMO_ALEGRIA, 0.5f, CAUSA_NOVEDAD);
    sumar(p, NEED_DIVERSION, 6);
    if (que == 1) { p->tilde_oido |= 2; return; }
    if (p->tilde_oido & 1) return;
    p->tilde_oido |= 1;
    if (p->tilde_chisme == 0xFF || (p->tilde_chisme & 0x80)) return; /* uno repetido no cuenta */
    const int k = p->tilde_chisme % PET_TILDE_CHISMES;
    if ((p->tilde_chismes >> k) & 1u) return;
    p->tilde_chismes |= (uint16_t)(1u << k);
    momento_usar(p, t);
    anotar(p, DIA_HISTORIA, NARR_TILDE, k, t);
    sumar(p, NEED_CURIOSIDAD, 10);
}
static void paso_tilde(pet_t *p, uint32_t t)
{
    if (p->tilde_hasta && t >= p->tilde_hasta) { /* se va (con lo que se llevó en el pico) */
        push_event(p, EV_TILDE, 1, p->tilde_toma ? (int8_t)(p->tilde_toma - 1) : -1, t);
        p->tilde_hasta = 0;
        p->tilde_toma = p->tilde_trae = 0;
        return;
    }
    if (p->tilde_hasta || p->dormida || p->etapa == STAGE_HUEVO) return;
    if (!p->tilde_prox) p->tilde_prox = 2; /* (v39) la primera visita, desde el día 2 */
    if (pet_days_played(p) < p->tilde_prox || p->tilde_dia == dia_byte(p, t)) return; /* una visita por día, como mucho */
    const int h = hora_local(p, t);
    if (h < 8 || h >= 20 || p->escena == ESC_CUEVA) return;
    if (t - p->ultima_interaccion > 15u * 60u) return; /* viene cuando están jugando: así la ven */
    if (chance(p, 0.08f)) tilde_llega(p, t);
}

/* --- la Abuela Brizna --- */
static int brizna_desde(int k) { static const uint8_t D[5] = {1, 8, 15, 22, 26}; return (unsigned)k < 5 ? D[k] : 99; }
bool pet_brizna_awake(const pet_t *p, uint32_t t, int *recuerdo)
{
    if (!p->brizna_hasta || t >= p->brizna_hasta || p->escena != ESC_ISLA) return false;
    if (recuerdo) *recuerdo = p->brizna_k < PET_BRIZNA_RECUERDOS ? p->brizna_k : -1;
    return true;
}
const char *pet_brizna_hello(int i) { return BRIZNA_SALUDOS[(unsigned)i % N_BRIZNA_SALUDOS]; }
static void brizna_despierta(pet_t *p, int k, uint32_t t)
{
    p->brizna_k = (uint8_t)(k < 0 ? 0xFE : k);
    p->brizna_hasta = t + 5u * 60u;
    push_event(p, EV_BRIZNA, k, 0, t);
    impulsar(p, EMO_CURIOSIDAD, 0.8f, CAUSA_RARO);
    sonar(p, SND_BOSTEZO, t);
}
void pet_brizna_heard(pet_t *p, uint32_t t)
{
    int k;
    if (!pet_brizna_awake(p, t, &k)) return;
    p->ultima_interaccion = t;
    impulsar(p, EMO_CARINO, 0.6f, CAUSA_RARO);
    sumar(p, NEED_SEGURIDAD, 10);
    p->brizna_hasta = t + 20u; /* bosteza y se vuelve a dormir */
    if (k < 0 || ((p->brizna >> k) & 1u)) return;
    p->brizna |= (uint8_t)(1u << k);
    p->brizna_ultimo = (uint16_t)pet_days_played(p);
    momento_usar(p, t);
    anotar(p, DIA_HISTORIA, NARR_BRIZNA, k, t);
    if (k == PET_BRIZNA_RECUERDOS - 1 && !p->guardiana && !p->bolsa[OBJ_CORAZON_BOSQUE] && !pet_object_known(p, OBJ_CORAZON_BOSQUE))
        pet_object_gives(p, OBJ_CORAZON_BOSQUE, OFU_REGALO, t); /* la mitad del Corazón que dormía en su lomo */
}
static void paso_brizna(pet_t *p, uint32_t t)
{
    if (p->brizna_hasta && t >= p->brizna_hasta) {
        push_event(p, EV_BRIZNA, p->brizna_k < PET_BRIZNA_RECUERDOS ? p->brizna_k : -1, 1, t);
        p->brizna_hasta = 0;
        return;
    }
    if (p->brizna_hasta || p->escena != ESC_ISLA || p->dormida || p->etapa == STAGE_HUEVO) return;
    const int h = hora_local(p, t);
    if (h < 8 || h >= 21 || t - p->ultima_interaccion > 10u * 60u) return;
    int k = -1;
    for (int i = 0; i < PET_BRIZNA_RECUERDOS; i++) if (!((p->brizna >> i) & 1u)) { k = i; break; }
    if (k >= 0) { /* el próximo recuerdo: una vez por semana de juego, más o menos */
        if (pet_mode(p) == MODO_INFANTE) return;
        if (pet_days_played(p) < brizna_desde(k)) return;
        if (k > 0 && pet_days_played(p) < p->brizna_ultimo + 4) return;
        if (!momento_libre(p, t)) return;
        brizna_despierta(p, k, t);
        return;
    }
    /* después del final: solo se asoma a saludar en su cumpleaños o al llegar la primavera */
    if (!pet_final_seen(p, PET_FINAL_SECRETO) || p->brizna_saludo == dia_byte(p, t)) return;
    const bool primavera = pet_season(p, t) == ESTACION_PRIMAVERA && dia_local(p, t) - p->dia_jugado < 1 && chance(p, 0.02f);
    if (pet_her_birthday(p, t) || primavera) {
        p->brizna_saludo = dia_byte(p, t);
        brizna_despierta(p, -1, t);
    }
}

/* --- lo que dicen los animales: cuando uno que ya conoce vuelve a estar con ella (le tiene confianza), uno por día --- */
int pet_animal_phrase_card(int i) { return (unsigned)i < PET_ANIMAL_FRASES ? FRASES_ANIMAL[i].ficha : -1; }
const char *pet_animal_phrase(int i, bool corto) { return (unsigned)i < PET_ANIMAL_FRASES ? (corto ? FRASES_ANIMAL[i].corto : FRASES_ANIMAL[i].txt) : ""; }
bool pet_animal_phrase_known(const pet_t *p, int i) { return pet_story_known(p, NARR_ANIMALES, i); }
static bool animal_cuenta(pet_t *p, int f, uint32_t t)
{
    if (pet_mode(p) == MODO_INFANTE) return false;
    if (p->animal_dia != dia_byte(p, t)) p->animal_hoy = 0;
    const int tope = pet_days_played(p) <= 14 ? 2 : 1; /* (v39) en las primeras dos semanas, hasta dos por día */
    if (p->animal_dia == dia_byte(p, t) && p->animal_hoy >= tope) return false;
    if (f == FAUNA_COUNT + ANIMAL_ERIZO && p->erizo_confianza < 3) return false; /* el erizo, cuando ya no se hace bolita */
    const int acto = pet_story_act(p);
    for (int i = 0; i < PET_ANIMAL_FRASES; i++) {
        if (FRASES_ANIMAL[i].ficha != f || pet_animal_phrase_known(p, i)) continue;
        int n = 0; /* cuál de las suyas es (1.º, 2.º o 3.º acto) */
        for (int j = 0; j < i; j++) n += FRASES_ANIMAL[j].ficha == f;
        if (n + 1 > acto) return false;
        p->animal_frases[i >> 5] |= 1u << (i & 31);
        p->lib_nuevo[NARR_ANIMALES >> 3] |= (uint8_t)(1u << (NARR_ANIMALES & 7));
        if (p->animal_dia != dia_byte(p, t)) p->animal_hoy = 0;
        p->animal_dia = dia_byte(p, t);
        if (p->animal_hoy < 255) p->animal_hoy++;
        push_event(p, EV_ANIMAL_DICE, f, i, t);
        impulsar(p, EMO_CURIOSIDAD, 0.5f, CAUSA_NOVEDAD);
        return true;
    }
    return false;
}
bool pet_animal_tell(pet_t *p, int f, uint32_t t)
{
    if (p->etapa == STAGE_HUEVO) return false;
    for (int i = 0; i < pet_cards_known(p); i++) if (p->fichas[i] == f) { p->ultima_interaccion = t; return animal_cuenta(p, f, t); } /* solo los que ya conoce */
    return false;
}

/* --- los lugares secretos --- */
bool pet_secret_open(const pet_t *p, int l) { return (unsigned)l <= PET_ZONA_CLARO && ((p->lugares >> l) & 1u); }
bool pet_zone_hint_pending(const pet_t *p, int z) { return (unsigned)z < 2 && !((p->zona_pista >> z) & 1u); }
void pet_zone_hint_done(pet_t *p, int z) { if ((unsigned)z < 2) p->zona_pista |= (uint8_t)(1u << z); }
int pet_secret_things(int l) { return l == 0 ? PET_CASITA_COSAS : (l == 1 ? PET_MIRADOR_COSAS : 0); }
bool pet_secret_found(const pet_t *p, int l, int c) { return (unsigned)l < PET_LUGARES && (unsigned)c < 8 && ((p->lugar_hallado[l] >> c) & 1u); }
const char *pet_secret_place_name(int l) { return (unsigned)l < PET_LUGARES ? LUGAR_NOMBRE[l] : ""; }
const char *pet_secret_text(int l, int c, bool corto)
{
    if ((unsigned)l >= PET_LUGARES || c < 0 || c >= pet_secret_things(l)) return "";
    return corto ? LUGAR_COSAS[l][c].corto : LUGAR_COSAS[l][c].txt;
}
const char *pet_secret_title(int l, int c) { return (unsigned)l < PET_LUGARES && c >= 0 && c < pet_secret_things(l) ? LUGAR_COSAS[l][c].titulo : ""; }
bool pet_secret_find(pet_t *p, int l, int c, uint32_t t)
{
    if ((unsigned)l >= PET_LUGARES) return false;
    if (!pet_secret_open(p, l) || c < 0 || c >= pet_secret_things(l) || pet_secret_found(p, l, c)) return false;
    p->lugar_hallado[l] |= (uint8_t)(1u << c);
    p->ultima_interaccion = t;
    sumar(p, NEED_CURIOSIDAD, 15);
    impulsar(p, EMO_CURIOSIDAD, 0.7f, CAUSA_ESPECIAL);
    if (l == 0 && c == 0) pet_object_gives(p, OBJ_GALLETA_TILO, OFU_REGALO, t);  /* una galleta quemada de recuerdo */
    if (l == 1 && c == 1) pet_object_gives(p, OBJ_CORONA_HOJAS, OFU_REGALO, t);  /* la corona de hojas */
    const int todas = (1 << pet_secret_things(l)) - 1;
    if ((p->lugar_hallado[l] & todas) == todas) tilo_pagina(p, l == 0 ? 5 : 6, t); /* lo encontró todo: la hoja escrita a mano */
    push_event(p, EV_LUGAR, l, c, t);
    return true;
}
static void paso_lugares(pet_t *p, uint32_t t)
{
    if (p->dormida || p->etapa == STAGE_HUEVO) return;
    /* (v39) el claro de las luciérnagas: en el bosque, desde el día 12, al atardecer (o a cualquier hora desde el día 14):
     * Lumbre deja un caminito de luces hacia un borde del bosque. Adentro del claro está la entrada a la casita de Tilo. */
    const int h = hora_local(p, t);
    if (!pet_secret_open(p, PET_ZONA_CLARO) && p->escena == ESC_BOSQUE && pet_days_played(p) >= 12 && t - p->ultima_interaccion < 10u * 60u &&
        ((h >= 17 && h < 22) || pet_days_played(p) >= 14)) {
        p->lugares |= 1u | (1u << PET_ZONA_CLARO);
        anotar(p, DIA_LUGAR, 0, 0, t);
        push_event(p, EV_LUGAR, PET_ZONA_CLARO, -1, t);
    }
    /* el mirador: en la cima de la colina, en la semana 4 (o cuando Nimba ya le contó su último paseo) */
    if (!pet_secret_open(p, 1) && p->escena == ESC_COLINA && (pet_days_played(p) >= 24 || ((p->historias_nimba >> 4) & 1u)) && t - p->ultima_interaccion < 10u * 60u) {
        p->lugares |= 2u;
        anotar(p, DIA_LUGAR, 1, 0, t);
        push_event(p, EV_LUGAR, 1, -1, t);
    }
}

/* --- todo lo de la historia, en cada paso --- */
static void paso_historia(pet_t *p, uint32_t t)
{
    paso_dias_jugados(p, t);
    paso_tilde(p, t);
    paso_brizna(p, t);
    paso_lugares(p, t);
    /* cuando ya conoce a las seis criaturas: la página «Quién me cuida a mí» */
    if (p->criaturas_vistas == (1u << CRI_COUNT) - 1u && pet_days_played(p) >= 11) tilo_pagina(p, 4, t);
}

/* --- los objetos: la leyenda según el acto y lo que dice al usarlo --- */
const char *pet_object_legend(const pet_t *p, int id)
{
    if (id == OBJ_CORAZON_BOSQUE && p->guardiana) return "Late en la piedra del centro de la isla, al mismo tiempo que ella.";
    return objeto_leyenda_acto(id, pet_story_act(p));
}
const char *pet_object_says(int id, bool corto) { (void)corto; return objeto_al_usar(id); }
