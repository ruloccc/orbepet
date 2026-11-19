/* ======================================================================
 * (1.0.32) La colección de la pesca: 30 animales del agua. Cada vez que algo pica y sale un pez, un cangrejo o uno de
 * los raros, el motor elige CUÁL según la estación, la hora, el clima y si el lago está congelado, y lo anota en la
 * colección (prefiere un poquito los que todavía no tiene). Se guarda en 4 bytes que eran relleno (peces_a y peces_b:
 * parten en cero, no cambia la versión de la partida). El número de cada uno es su bit: NO reordenar la tabla.
 * Dibujos: firmware/main/peces_hd.inc (herramientas/iconos_nuevos/peces48.py, mismo orden).
 * Incluido desde pet.c.
 * ==================================================================== */
enum { PZT_PEZ = 1, PZT_CANG = 2, PZT_RARO = 4 };                                       /* con qué pesca_t sale */
enum { PZE_PRI = 1, PZE_VER = 2, PZE_OTO = 4, PZE_INV = 8, PZE_TODAS = 15 };            /* estaciones */
enum { PZH_MAN = 1, PZH_DIA = 2, PZH_TAR = 4, PZH_NOC = 8, PZH_TODAS = 15 };            /* 6-11, 11-18, 18-21, 21-6 */
#define PZC(c) (1u << (c))                                                                /* climas (0: cualquiera) */
enum { PZI_IGUAL = 0, PZI_SOLO = 1, PZI_NUNCA = 2 };                                     /* el lago congelado */
typedef struct {
    const char *nombre, *con_art, *desc;
    uint8_t tipo, est, hora, hielo;
    uint8_t clima;   /* máscara de clima_t (0: cualquiera) */
    uint8_t peso;
} pez_info_t;
static const pez_info_t PECES[PET_PECES] = {
    {"Trucha arcoíris", "una trucha arcoíris", "Tiene una franja rosada brillante en el costado, como si se hubiera tragado un pedacito de arcoíris.", PZT_PEZ, PZE_TODAS, PZH_MAN | PZH_DIA | PZH_TAR, PZI_IGUAL, 0, 10},
    {"Pejerrey", "un pejerrey", "Plateado y finito. Nada en grupos grandes cerca de la orilla y brilla cuando le da el sol.", PZT_PEZ, PZE_TODAS, PZH_TODAS, PZI_IGUAL, 0, 10},
    {"Carmelita", "una carmelita", "Chiquitita y café, con pintitas. Se esconde entre las piedras del fondo del río.", PZT_PEZ, PZE_PRI | PZE_VER, PZH_MAN | PZH_DIA, PZI_NUNCA, 0, 6},
    {"Puye", "un puye", "¡Casi transparente! Se le ven los ojitos y la espina. Sube por los ríos cuando empieza la primavera.", PZT_PEZ, PZE_PRI, PZH_MAN, PZI_NUNCA, 0, 5},
    {"Bagrecito", "un bagrecito", "Tiene bigotes largos para buscar comida a oscuras en el barro del fondo.", PZT_PEZ, PZE_TODAS, PZH_TAR | PZH_NOC, PZI_NUNCA, 0, 6},
    {"Perca trucha", "una perca trucha", "Rayada como un tigre de agua y muy curiosa: muerde todo lo que brilla.", PZT_PEZ, PZE_VER, PZH_DIA | PZH_TAR, PZI_NUNCA, 0, 6},
    {"Salmón", "un salmón", "Viaja desde el mar hasta el mismo río donde nació. ¡Qué buena memoria!", PZT_PEZ, PZE_OTO, PZH_TODAS, PZI_NUNCA, 0, 5},
    {"Lamprea", "una lamprea", "Parece una cinta que nada. Tiene una boquita redonda como una ventosa.", PZT_PEZ, PZE_INV, PZH_NOC, PZI_IGUAL, 0, 3},
    {"Pez luna", "un pez luna", "Redondo y enorme. Le gusta tomar sol flotando de lado en los días despejados.", PZT_PEZ, PZE_VER, PZH_DIA, PZI_NUNCA, PZC(CLIMA_DESPEJADO), 2},
    {"Sardina", "una sardina", "Nunca anda sola: con miles de amigas forma una nube plateada bajo el agua.", PZT_PEZ, PZE_PRI | PZE_VER, PZH_MAN | PZH_DIA, PZI_NUNCA, 0, 7},
    {"Jurel", "un jurel", "Rápido y fuerte. Tiene una línea de escamas duras en el costado, como un cierre.", PZT_PEZ, PZE_VER | PZE_OTO, PZH_DIA | PZH_TAR, PZI_NUNCA, 0, 6},
    {"Congrio", "un congrio", "Largo como una anguila y rosado. Vive en cuevitas entre las rocas y sale cuando oscurece.", PZT_PEZ, PZE_OTO | PZE_INV, PZH_TAR | PZH_NOC, PZI_IGUAL, 0, 4},
    {"Corvina", "una corvina", "Cuando está contenta hace un ruidito como de tambor. ¡Bum, bum!", PZT_PEZ, PZE_PRI | PZE_VER, PZH_TAR, PZI_NUNCA, 0, 5},
    {"Reineta", "una reineta", "Alta y plana como un plato de plata. Le gustan las aguas frías.", PZT_PEZ, PZE_OTO | PZE_INV, PZH_MAN | PZH_DIA, PZI_IGUAL, 0, 5},
    {"Pez globo", "un pez globo", "Si se asusta se infla como una pelota con espinas. ¡Tranquilo, amiguito, no te haremos nada!", PZT_PEZ, PZE_VER, PZH_DIA, PZI_NUNCA, 0, 2},
    {"Caballito de mar", "un caballito de mar", "Nada paradito y se agarra de las algas con la cola. El papá es el que cuida los huevitos.", PZT_PEZ, PZE_PRI, PZH_MAN | PZH_DIA, PZI_NUNCA, PZC(CLIMA_DESPEJADO) | PZC(CLIMA_NUBLADO), 2},
    {"Pulpo", "un pulpo", "Tiene ocho brazos y cambia de color para esconderse. ¡Me saludó con todos a la vez!", PZT_PEZ, PZE_OTO | PZE_INV, PZH_NOC, PZI_NUNCA, 0, 3},
    {"Estrella de mar", "una estrella de mar", "Camina con cientos de piecitos chiquititos. Si pierde un brazo, le crece otro.", PZT_PEZ, PZE_TODAS, PZH_MAN, PZI_NUNCA, 0, 4},
    {"Erizo de mar", "un erizo de mar", "Una pelotita de espinas moradas que pasea muy, muy lento por el fondo.", PZT_PEZ, PZE_INV | PZE_PRI, PZH_MAN | PZH_DIA, PZI_NUNCA, 0, 4},
    {"Pez linterna", "un pez linterna", "Vive en lo más hondo y oscuro. Tiene lucecitas en la panza y una lamparita en la frente.", PZT_PEZ, PZE_TODAS, PZH_NOC, PZI_IGUAL, 0, 2},
    {"Anguila", "una anguila", "Sale cuando llueve y se desliza como un fideo resbaloso. ¡No hay quien la sujete!", PZT_PEZ, PZE_TODAS, PZH_TODAS, PZI_NUNCA, PZC(CLIMA_LLUVIA) | PZC(CLIMA_TORMENTA), 7},
    {"Pez de hielo", "un pez de hielo", "Vive bajo el lago congelado. Es transparente como el hielo y su sangre nunca se congela.", PZT_PEZ, PZE_INV, PZH_TODAS, PZI_SOLO, 0, 8},
    {"Pancora", "una pancora", "Un cangrejo de río con tenazas rojas. Vive escondido bajo las piedras de la orilla.", PZT_CANG, PZE_TODAS, PZH_TODAS, PZI_NUNCA, 0, 10},
    {"Camarón de río", "un camarón de río", "Escapa saltando hacia atrás de un coletazo. Hace hoyitos en el barro de la orilla.", PZT_CANG, PZE_PRI | PZE_VER, PZH_TAR | PZH_NOC, PZI_NUNCA, 0, 7},
    {"Jaiba", "una jaiba", "Camina de lado y tiene las patas de atrás como remitos para nadar.", PZT_CANG, PZE_VER | PZE_OTO, PZH_MAN | PZH_DIA, PZI_NUNCA, 0, 7},
    {"Cangrejo ermitaño", "un cangrejo ermitaño", "Vive en una concha prestada y, cuando crece, busca una más grande para mudarse.", PZT_CANG, PZE_PRI | PZE_VER, PZH_TODAS, PZI_NUNCA, 0, 6},
    {"Centolla", "una centolla", "Una gigante de patas larguísimas y caparazón con espinas. Le encantan las aguas frías del sur.", PZT_CANG, PZE_OTO | PZE_INV, PZH_TODAS, PZI_NUNCA, 0, 3},
    {"Pez dorado", "un pez dorado", "Brilla como un pedacito de sol. Dicen que trae buena suerte a quien lo devuelve al agua.", PZT_RARO, PZE_TODAS, PZH_TODAS, PZI_IGUAL, 0, 10},
    {"Pez arcoíris", "un pez arcoíris", "Solo aparece cuando hay un arcoíris en el cielo: sus escamas tienen todos los colores.", PZT_PEZ | PZT_RARO, PZE_TODAS, PZH_MAN | PZH_DIA | PZH_TAR, PZI_NUNCA, 0, 0},
    {"Pez de las estrellas", "un pez de las estrellas", "Sube de noche cuando el cielo está despejado. Tiene puntitos que brillan como estrellas.", PZT_PEZ | PZT_RARO, PZE_TODAS, PZH_NOC, PZI_IGUAL, PZC(CLIMA_DESPEJADO) | PZC(CLIMA_FRIO), 3},
};
static int pez_hora_bit(int h) { return h >= 6 && h < 11 ? PZH_MAN : (h >= 11 && h < 18 ? PZH_DIA : (h >= 18 && h < 21 ? PZH_TAR : PZH_NOC)); }
bool pet_fish_species_known(const pet_t *p, int k)
{
    if (!p || (unsigned)k >= PET_PECES) return false;
    const uint8_t *b = k < 16 ? p->peces_a : p->peces_b;
    const int j = k & 15;
    return (b[j >> 3] >> (j & 7)) & 1u;
}
static bool pez_marcar(pet_t *p, int k)
{
    if (pet_fish_species_known(p, k)) return false;
    uint8_t *b = k < 16 ? p->peces_a : p->peces_b;
    const int j = k & 15;
    b[j >> 3] |= (uint8_t)(1u << (j & 7));
    return true;
}
int pet_fish_species_found(const pet_t *p) { int n = 0; for (int k = 0; k < PET_PECES; k++) n += pet_fish_species_known(p, k); return n; }
const char *pet_fish_species_name(int k) { return (unsigned)k < PET_PECES ? PECES[k].nombre : ""; }
const char *pet_fish_species_with_article(int k) { return (unsigned)k < PET_PECES ? PECES[k].con_art : "un pez"; }
const char *pet_fish_species_desc(int k) { return (unsigned)k < PET_PECES ? PECES[k].desc : ""; }
/* cuándo se puede pescar (en palabras): «En primavera y verano, de mañana. Con sol.» */
void pet_fish_species_when(int k, char *buf, int n)
{
    if (!buf || n <= 0) return;
    buf[0] = 0;
    if ((unsigned)k >= PET_PECES) return;
    const pez_info_t *z = &PECES[k];
    static const char *const EST[4] = {"primavera", "verano", "otoño", "invierno"};
    static const char *const HOR[4] = {"de mañana", "de día", "al atardecer", "de noche"};
    char e[64] = "", h[64] = "";
    const char *le[4], *lh[4];
    int ne = 0, nh = 0;
    for (int i = 0; i < 4; i++) { if ((z->est >> i) & 1) le[ne++] = EST[i]; if ((z->hora >> i) & 1) lh[nh++] = HOR[i]; }
    if (ne == 4) snprintf(e, sizeof e, "Todo el año");
    else {
        snprintf(e, sizeof e, "En %s", le[0]);
        for (int i = 1; i < ne; i++) { const size_t l = strlen(e); snprintf(e + l, sizeof e - l, "%s%s", i == ne - 1 ? " y " : ", ", le[i]); }
    }
    if (nh < 4) {
        snprintf(h, sizeof h, "%s", lh[0]);
        for (int i = 1; i < nh; i++) { const size_t l = strlen(h); snprintf(h + l, sizeof h - l, "%s%s", i == nh - 1 ? " o " : ", ", lh[i]); }
    }
    const char *extra = "";
    if (k == 28) extra = " Solo cuando hay un arcoíris.";
    else if (z->hielo == PZI_SOLO) extra = " Solo con el lago congelado.";
    else if (z->clima == (PZC(CLIMA_LLUVIA) | PZC(CLIMA_TORMENTA))) extra = " Solo cuando llueve.";
    else if (z->clima & PZC(CLIMA_DESPEJADO)) extra = " Con el cielo despejado.";
    if (z->tipo == PZT_RARO) extra = " Muy raro: casi nunca pica.";
    snprintf(buf, (size_t)n, "%s%s%s.%s", e, nh < 4 ? ", " : "", h, extra);
}
/* qué animal salió (la pesca ya decidió que fue un pez, un cangrejo o uno raro) */
static int pez_elegir(pet_t *p, pesca_t c, uint32_t t)
{
    const int tipo = c == PESCA_CANGREJO ? PZT_CANG : (c == PESCA_PEZ_DORADO ? PZT_RARO : PZT_PEZ);
    const int e = 1 << (int)pet_season(p, t), h = pez_hora_bit(hora_local(p, t));
    const int cl = (int)pet_weather(p);
    const bool hielo = pet_lake_frozen(p, t), arco = p->raro == RARO_ARCOIRIS;
    float peso[PET_PECES], tot = 0;
    for (int k = 0; k < PET_PECES; k++) {
        const pez_info_t *z = &PECES[k];
        float w = 0;
        if ((z->tipo & tipo) && (z->est & e) && (z->hora & h) && (!z->clima || ((z->clima >> cl) & 1u)) &&
            !(z->hielo == PZI_SOLO && !hielo) && !(z->hielo == PZI_NUNCA && hielo)) w = (float)z->peso;
        if (k == 28) w = arco && (z->hora & h) && !hielo ? (tipo == PZT_RARO ? 40.0f : 12.0f) : 0.0f; /* el pez arcoíris: solo con arcoíris */
        if (k == 29 && tipo == PZT_RARO && w > 0) w = 8.0f;                                          /* (entre los raros, más seguido) */
        if (w > 0 && !pet_fish_species_known(p, k)) w *= 1.6f; /* un poquito más los que todavía no tiene */
        peso[k] = w; tot += w;
    }
    if (tot <= 0) return tipo == PZT_CANG ? 22 : (tipo == PZT_RARO ? 27 : 1);
    float r = rng01(p) * tot;
    for (int k = 0; k < PET_PECES; k++) { if (r < peso[k]) return k; r -= peso[k]; }
    return tipo == PZT_CANG ? 22 : (tipo == PZT_RARO ? 27 : 1);
}
/* (pruebas) que salga este (o -1: al azar como siempre) */
static int s_pez_forzado = -1;
void pet_fish_species_force(int k) { s_pez_forzado = k; }
