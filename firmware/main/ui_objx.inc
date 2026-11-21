/*
 * Los objetos que de verdad hacen lo que dicen (incluido en ui.c, al final).
 *   cofre bajo el árbol viejo · nidos de las gafas del explorador · burbujas cuadradas de la pipa
 *   brújula loca · robot de la llave de cuerda · destello de la runa de teletransporte
 *   y lo que cambia en ella: invisible, chiquitita, grande, colores
 * El núcleo (pet.c) decide qué pasa; aquí solo se ve y se toca.
 */

/* ---- el cofre bajo el árbol viejo (pie del tronco, en el prado) ---- */
#define COFRE_X 66
#define COFRE_Y 248
static lv_obj_t *s_cofre, *s_cofre_cand;
static bool s_cofre_estaba;
static uint32_t s_cofre_abre;

static void cofre_cb(lv_event_t *e)
{
    (void)e;
    if (!s_pet || s_panel != PANEL_NINGUNO) return;
    if (durmiendo_aviso()) return; /* (1.0.40) */
    switch (pet_chest_status(s_pet)) {
    case 4: accion(UI_ACCION_OBJETO, 5 << 8); break;
    case 3: globo(ICO_CANDADO, "LE FALTA EL CANDADO RÚNICO", 3000); break;
    case 2: globo(ICO_CANDADO, "LE FALTA LA LLAVE OXIDADA", 3000); break;
    default: globo(ICO_COFRE, "UN COFRE VIEJO BAJO EL ÁRBOL. NECESITA UNA LLAVE Y UN CANDADO", 3800); break;
    }
}

/* ---- los nidos que ve con las gafas ---- */
static lv_obj_t *s_nido[PET_NIDOS];
static uint8_t s_nido_tocado;
static int s_nido_dia = -1;
/* [0] prado, [1] bosque: entre las ramas, lejos de donde camina */
static const int16_t NIDO_XY[2][PET_NIDOS][2] = {{{58, 138}, {150, 112}, {392, 150}}, {{110, 120}, {250, 96}, {380, 140}}};

static void nido_cb(lv_event_t *e)
{
    int i = (int)(intptr_t)lv_event_get_user_data(e);
    if (i < 0 || i >= PET_NIDOS || !s_pet || s_panel != PANEL_NINGUNO) return;
    if (durmiendo_aviso()) return; /* (1.0.40) */
    s_nido_tocado |= (uint8_t)(1u << i);
    ocultar(s_nido[i], true);
    for (int k = 0; k < 3; k++) flotar(ICO_ESTRELLA, NULL, 0, NIDO_XY[s_escena == ESC_BOSQUE][i][0] - 12 + k * 12, NIDO_XY[s_escena == ESC_BOSQUE][i][1] - 6, (uint32_t)k * 120);
    accion(UI_ACCION_OBJETO, i | 6 << 8);
}

/* ---- las burbujas cuadradas de la pipa del druida ---- */
#define N_BURB 6
static lv_obj_t *s_burb[N_BURB];
static float s_burb_x[N_BURB], s_burb_y[N_BURB], s_burb_f[N_BURB];
static bool s_burb_viva[N_BURB];
static uint32_t s_burb_hasta, s_burb_sig;

static void burbuja_cb(lv_event_t *e)
{
    int i = (int)(intptr_t)lv_event_get_user_data(e);
    if (i < 0 || i >= N_BURB || !s_burb_viva[i]) return;
    s_burb_viva[i] = false;
    ocultar(s_burb[i], true);
    for (int k = 0; k < 4; k++) salpicar((int)s_burb_x[i] + 6 + k * 5, (int)s_burb_y[i] + 8, k & 1 ? 0xFFFFFF : 0xBFE6FF); /* revienta en pedacitos */
    flotar(ICO_ESTRELLA, NULL, 0, (int)s_burb_x[i] + 14, (int)s_burb_y[i] - 4, 0);
    accion(UI_ACCION_SONAR, SND_RISA);
}

/* ---- la brújula loca ---- */
#define BRU_X (CX - 42)
#define BRU_Y 56 /* arriba al centro, justo debajo del reloj y el clima */
static lv_obj_t *s_bru, *s_bru_aguja;
static uint32_t s_bru_desde, s_bru_hasta;

/* ---- el robot de cuerda ---- */
static uint32_t s_robot_paso, s_robot_meta;

/* ---- el destello de la runa de teletransporte ---- */
static lv_obj_t *s_tp_flash;
static uint32_t s_tp_desde;

static void objx_crear(void)
{
    s_cofre = lv_image_create(s_mundo);
    lv_image_set_src(s_cofre, art_icono(ICO_COFRE, 5));
    lv_obj_add_flag(s_cofre, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(s_cofre, 10);
    lv_obj_add_event_cb(s_cofre, cofre_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_set_pos(s_cofre, COFRE_X - 30, COFRE_Y - 54);
    ocultar(s_cofre, true);
    s_cofre_cand = lv_image_create(s_mundo);
    lv_image_set_src(s_cofre_cand, art_icono(ICO_CANDADO, 2));
    lv_obj_remove_flag(s_cofre_cand, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_pos(s_cofre_cand, COFRE_X - 12, COFRE_Y - 34);
    ocultar(s_cofre_cand, true);

    for (int i = 0; i < PET_NIDOS; i++) {
        lv_obj_t *n = caja(s_mundo);
        lv_obj_set_size(n, 44, 32);
        lv_obj_add_flag(n, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_ext_click_area(n, 12);
        lv_obj_add_event_cb(n, nido_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        lv_obj_t *c = caja(n); /* el cuenco de ramitas */
        lv_obj_set_size(c, 44, 18);
        lv_obj_set_pos(c, 0, 14);
        lv_obj_set_style_radius(c, 9, 0);
        lv_obj_set_style_bg_color(c, lv_color_hex(0x8A5A2A), 0);
        lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(c, 2, 0);
        lv_obj_set_style_border_color(c, lv_color_hex(0x4E3220), 0);
        for (int k = 0; k < 2; k++) { /* dos huevitos */
            lv_obj_t *h = caja(n);
            lv_obj_set_size(h, 14, 17);
            lv_obj_set_pos(h, 8 + k * 17, 3 + k * 2);
            lv_obj_set_style_radius(h, 8, 0);
            lv_obj_set_style_bg_color(h, lv_color_hex(k ? 0xBFE6FF : 0xF6F2EA), 0);
            lv_obj_set_style_bg_opa(h, LV_OPA_COVER, 0);
            lv_obj_set_style_border_width(h, 1, 0);
            lv_obj_set_style_border_color(h, lv_color_hex(0x4E3220), 0);
        }
        lv_obj_t *r = caja(n); /* el frente del nido tapa la mitad de abajo de los huevos */
        lv_obj_set_size(r, 44, 9);
        lv_obj_set_pos(r, 0, 23);
        lv_obj_set_style_radius(r, 5, 0);
        lv_obj_set_style_bg_color(r, lv_color_hex(0x6A4524), 0);
        lv_obj_set_style_bg_opa(r, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(r, 2, 0);
        lv_obj_set_style_border_color(r, lv_color_hex(0x4E3220), 0);
        ocultar(n, true);
        s_nido[i] = n;
    }

    for (int i = 0; i < N_BURB; i++) {
        lv_obj_t *b = caja(s_mundo);
        lv_obj_set_size(b, 30, 30);
        lv_obj_set_style_radius(b, 6, 0);
        lv_obj_set_style_bg_color(b, lv_color_hex(0xBFE6FF), 0);
        lv_obj_set_style_bg_opa(b, LV_OPA_40, 0);
        lv_obj_set_style_border_width(b, 3, 0);
        lv_obj_set_style_border_color(b, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_border_opa(b, LV_OPA_80, 0);
        lv_obj_t *br = caja(b); /* el brillito */
        lv_obj_set_size(br, 7, 7);
        lv_obj_set_pos(br, 4, 4);
        lv_obj_set_style_radius(br, 2, 0);
        lv_obj_set_style_bg_color(br, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_bg_opa(br, LV_OPA_COVER, 0);
        lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_ext_click_area(b, 14);
        lv_obj_add_event_cb(b, burbuja_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        ocultar(b, true);
        s_burb[i] = b;
    }

    s_bru = caja(s_mundo);
    lv_obj_set_size(s_bru, 84, 84);
    lv_obj_set_pos(s_bru, BRU_X, BRU_Y);
    lv_obj_set_style_radius(s_bru, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(s_bru, lv_color_hex(0x2A2238), 0);
    lv_obj_set_style_bg_opa(s_bru, LV_OPA_90, 0);
    lv_obj_set_style_border_width(s_bru, 4, 0);
    lv_obj_set_style_border_color(s_bru, lv_color_hex(0xC8B89A), 0);
    for (int k = 0; k < 4; k++) { /* las cuatro marcas */
        lv_obj_t *m = caja(s_bru);
        lv_obj_set_size(m, k & 1 ? 8 : 3, k & 1 ? 3 : 8);
        lv_obj_set_style_bg_color(m, lv_color_hex(0xF6F2EA), 0);
        lv_obj_set_style_bg_opa(m, LV_OPA_COVER, 0);
        if (k == 0) lv_obj_set_pos(m, 38, 2);
        else if (k == 1) lv_obj_set_pos(m, 69, 38);
        else if (k == 2) lv_obj_set_pos(m, 38, 69);
        else lv_obj_set_pos(m, 2, 38);
    }
    s_bru_aguja = caja(s_bru);
    lv_obj_set_size(s_bru_aguja, 8, 56); /* la punta roja (arriba) y la cola clara (abajo) */
    lv_obj_set_pos(s_bru_aguja, 34, 10);
    lv_obj_set_style_radius(s_bru_aguja, 4, 0);
    lv_obj_set_style_bg_color(s_bru_aguja, lv_color_hex(0xF6F2EA), 0);
    lv_obj_set_style_bg_opa(s_bru_aguja, LV_OPA_COVER, 0);
    lv_obj_t *pu = caja(s_bru_aguja);
    lv_obj_set_size(pu, 8, 28);
    lv_obj_set_pos(pu, 0, 0);
    lv_obj_set_style_radius(pu, 4, 0);
    lv_obj_set_style_bg_color(pu, lv_color_hex(0xE0485E), 0);
    lv_obj_set_style_bg_opa(pu, LV_OPA_COVER, 0);
    lv_obj_set_style_transform_pivot_x(s_bru_aguja, 4, 0);
    lv_obj_set_style_transform_pivot_y(s_bru_aguja, 28, 0);
    ocultar(s_bru, true);

    s_tp_flash = caja(s_mundo);
    lv_obj_set_size(s_tp_flash, W, W);
    lv_obj_set_pos(s_tp_flash, 0, 0);
    lv_obj_set_style_radius(s_tp_flash, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(s_tp_flash, lv_color_hex(0xDFF8FF), 0);
    lv_obj_set_style_bg_opa(s_tp_flash, LV_OPA_COVER, 0);
    ocultar(s_tp_flash, true);
}

/* qué se ve cuando usa uno de estos objetos (param = objeto_x_t, cod = lo que el núcleo cuenta) */
static void objx_uso(int param, int cod, int id, int hx, int hy)
{
    switch (param) {
    case OBJX_ESPEJO:
        globo(ICO_OBJETO(id), "SE ABRIÓ UN PORTALITO: HOY TODO LO QUE ENCUENTRE VIENE DOBLE", 3600);
        for (int i = 0; i < 4; i++) flotar(ICO_ESTRELLA, NULL, 0, hx - 36 + i * 24, hy - 10, (uint32_t)i * 140);
        accion(UI_ACCION_SONAR, SND_NUEVO);
        break;
    case OBJX_RUNA_TP: {
        s_tp_desde = s_ms;
        s_zona_pend = -1;
        s_escena = -2; /* llega directo, sin caminar hasta el borde */
        mascota_modo(M_LIBRE, 0);
        if (cod >= 1 && cod <= ESC_COUNT) {
            static const char *const T[ESC_COUNT] = {"¡PUF! YA ESTÁ EN CASA", "¡PUF! AHORA ESTÁN EN EL BOSQUE", "¡PUF! ESTÁN JUNTO A LA LAGUNA", "¡PUF! ESTÁN EN EL CERRO", "¡PUF! ESTÁN EN LA CUEVA", "¡PUF! ESTÁN EN LA ISLA", "¡PUF! ESTÁN EN EL CLARO DEL REY", "¡PUF! ESTÁN EN EL MIRADOR"};
            globo(ICO_ZONA[cod - 1], T[cod - 1], 3000);
        }
        accion(UI_ACCION_SONAR, SND_SORPRESA);
        break;
    }
    case OBJX_INVISIBLE:
        globo(ICO_OBJETO(id), "¿DÓNDE ESTÁ? ¡AQUÍ, PERO CASI NO SE VE!", 3400);
        for (int i = 0; i < 5; i++) flotar(ICO_ESTRELLA, NULL, 0, hx - 40 + i * 20, hy + (i & 1) * 20, (uint32_t)i * 100);
        accion(UI_ACCION_SONAR, SND_SORPRESA);
        break;
    case OBJX_ENCOGE:
        globo(ICO_OBJETO(id), "¡SE PUSO CHIQUITITA Y CURIOSA!", 3200);
        mascota_modo(M_SALTA, 1400);
        accion(UI_ACCION_SONAR, SND_RISA);
        break;
    case OBJX_CRECE:
        globo(ICO_OBJETO(id), "¡UN TRAGO Y SE SIENTE GRANDE Y FUERTE!", 3200);
        mascota_modo(M_SALTA, 1800);
        accion(UI_ACCION_SONAR, SND_ALEGRE);
        break;
    case OBJX_COLORES:
        globo(ICO_OBJETO(id), "¡MIRA CÓMO LE CAMBIAN LOS COLORES!", 3200);
        mascota_modo(M_SALTA, 1600);
        for (int i = 0; i < 4; i++) flotar(ICO_ARCOIRIS, NULL, 0, hx - 36 + i * 24, hy - 20, (uint32_t)i * 150);
        accion(UI_ACCION_SONAR, SND_ALEGRE);
        break;
    case OBJX_CANDADO: {
        int g = s_pet ? pet_guarded_object(s_pet) : -1;
        if (cod == 5 && g >= 0) {
            char t[96];
            snprintf(t, sizeof t, "EL CANDADO CUIDA %s", objeto_info(g)->nombre);
            mayusculas(t);
            globo(ICO_OBJETO(g), t, 3600);
        } else globo(ICO_CANDADO, "TODAVÍA NO HAY NADA PARA CUIDAR", 3000);
        break;
    }
    case OBJX_TINTERO:
        if (cod == 4) {
            globo(ICO_OBJETO(id), "LA TINTA REVELÓ UNA NOTA ESCONDIDA EN SU DIARIO", 3800);
            for (int i = 0; i < 3; i++) flotar(ICO_LIBRO, NULL, 0, hx - 24 + i * 24, hy - 20, (uint32_t)i * 160);
            accion(UI_ACCION_SONAR, SND_SORPRESA);
        } else globo(ICO_OBJETO(id), "YA LEYÓ TODAS LAS NOTAS ESCONDIDAS", 3200);
        break;
    case OBJX_LENTES:
        globo(ICO_OBJETO(id), "VE EL BOSQUE MÁGICO: HOY APARECEN MÁS SECRETOS", 3600);
        for (int i = 0; i < 4; i++) flotar(ICO_ESTRELLA, NULL, 0, hx - 36 + i * 24, hy - 14, (uint32_t)i * 130);
        break;
    case OBJX_IMAN:
        globo(ICO_OBJETO(id), "ATRAE LAS COSAS PERDIDAS: HOY APARECEN MÁS ANTIGÜEDADES", 3600);
        for (int i = 0; i < 4; i++) flotar(ICO_ESTRELLA, NULL, 0, hx - 36 + i * 24, hy - 14, (uint32_t)i * 130);
        break;
    case OBJX_BRUJULA:
        s_bru_desde = s_ms;
        s_bru_hasta = s_ms + 16000;
        globo(ICO_OBJETO(id), "LA AGUJA ENLOQUECE... Y SIEMPRE APUNTA HACIA ELLA", 3400);
        break;
    case OBJX_LLAVE:
        if (cod == 2) globo(ICO_OBJETO(id), "TODAVÍA NO ENCUENTRA QUÉ ABRE", 3200);
        else if (cod == 3) globo(ICO_CANDADO, "ABRE UN COFRE BAJO EL ÁRBOL VIEJO... PERO LE FALTA SU CANDADO", 3800);
        /* (cod 4: el cofre se abre y lo cuenta animar_objx) */
        break;
    case OBJX_CUERDA:
        s_robot_paso = 0; s_robot_meta = 0;
        mascota_modo(M_ROBOT, 18000);
        globo(ICO_OBJETO(id), "CLIC, CLIC, CLIC... ¡CAMINA COMO ROBOT DE JUGUETE!", 3200);
        accion(UI_ACCION_SONAR, SND_RISA);
        break;
    case OBJX_PIPA:
        s_burb_hasta = s_ms + 14000;
        s_burb_sig = s_ms;
        globo(ICO_OBJETO(id), "¡BURBUJAS CUADRADAS! TÓCALAS PARA REVENTARLAS", 3200);
        break;
    default: break;
    }
}

/* ella misma: tamaño, colores y transparencia según lo que bebió */
static void objx_pose(criatura_t *c, const pet_t *p)
{
    art_pose_t *ps = &c->pose;
    const int talla = pet_size_mode(p, s_ultimo_t);
    ps->escala = talla == 1 ? 2 : (talla == 2 ? 5 : 4); /* normal 4 (un 20 % menos que antes); chica y grande, en proporción */
    if (pet_colors_on(p, s_ultimo_t)) /* va pasando por todos los tintes */
        ps->especie = (pet_look(p) & ~(7 << 7)) | (int)(((s_ms / 500u) % (PET_COLORES - 1u)) + 1u) << 7;
    const bool inv = pet_invisible(p, s_ultimo_t);
    if (c == &s_mascota && s_exc_oculta) { lv_obj_set_style_opa(c->img, LV_OPA_TRANSP, 0); lv_obj_set_style_opa(c->sombra, LV_OPA_TRANSP, 0); return; } /* (v40) de excursión */
    lv_obj_set_style_opa(c->img, inv ? (lv_opa_t)(60 + 30 * sinf((float)s_ms / 400.0f)) : LV_OPA_COVER, 0);
    lv_obj_set_style_opa(c->sombra, inv ? LV_OPA_30 : LV_OPA_COVER, 0);
}

/* con la llave de cuerda: pasitos tiesos, uno cada rato */
static void objx_robot(criatura_t *c, art_pose_t *ps, float *sx, float *sy)
{
    const uint32_t paso = s_ms / 240u;
    ps->expr = EXP_FELIZ; ps->orejas = 0; ps->cola = 0; ps->postura = 1; ps->boca = false;
    if (s_ms >= s_robot_meta) { /* nuevo rumbo cada dos segundos */
        s_robot_meta = s_ms + 2200;
        c->tx = 110 + azar01() * 250;
        c->ty = LXY(0)[1] - 40 + azar01() * 90;
        limitar(&c->tx, &c->ty);
    }
    if (paso != s_robot_paso) { /* un pasito rígido */
        s_robot_paso = paso;
        float dx = c->tx - c->x, dy = c->ty - c->y, d = hypotf(dx, dy);
        if (d > 6.0f) {
            c->x += dx / d * 9.0f;
            c->y += dy / d * 9.0f;
            ps->espejo = dx < 0;
            if (paso % 3 == 0) flotar(ICO_ESTRELLA, NULL, 0, (int)c->x + (ps->espejo ? 26 : -26), cabeza_y(c) + 20, 0);
        }
    }
    ps->frame = (int)(paso & 1);
    *sx = (paso & 1) ? 3.0f : -3.0f; /* se bambolea como un juguete */
    *sy = 0;
}

static void animar_objx(void)
{
    const pet_t *p = s_pet;
    if (!p) return;
    if (!s_cofre) objx_crear();
    const bool prado = s_escena == ESC_PRADO && !s_interior && p->etapa != STAGE_HUEVO && s_juego == JG_NADA;

    /* el cofre */
    const bool vis = pet_chest_visible(p);
    if (s_cofre_estaba && !vis && prado) { /* se acaba de abrir */
        s_cofre_abre = s_ms;
        for (int i = 0; i < 8; i++) flotar(ICO_ESTRELLA, NULL, 0, COFRE_X - 42 + i * 12, COFRE_Y - 40 - (i & 1) * 16, (uint32_t)i * 110);
        for (int i = 0; i < 8; i++) salpicar(COFRE_X - 20 + i * 6, COFRE_Y - 30, i & 1 ? 0xFFD45C : 0xFFF2A0);
        globo(ICO_COFRE, "¡EL COFRE SE ABRIÓ! ADENTRO HABÍA TESOROS DORADOS", 4200);
        accion(UI_ACCION_SONAR, SND_TESORO);
        mascota_modo(M_SALTA, 2200);
    }
    s_cofre_estaba = vis;
    const bool abre = s_cofre_abre && s_ms - s_cofre_abre < 900;
    const int st = vis ? pet_chest_status(p) : 0;
    ocultar(s_cofre, !((vis && prado) || (abre && prado)));
    ocultar(s_cofre_cand, !(vis && prado && st != 4));
    if (vis && prado) {
        const bool listo = st == 4;
        lv_obj_set_style_shadow_width(s_cofre, listo ? 14 + (int)(10 * sinf((float)s_ms / 260.0f)) : 0, 0);
        lv_obj_set_style_shadow_color(s_cofre, lv_color_hex(0xFFD45C), 0);
        lv_obj_set_style_shadow_opa(s_cofre, LV_OPA_70, 0);
        lv_obj_set_pos(s_cofre, COFRE_X - 30 + (listo ? (int)(2 * sinf((float)s_ms / 130.0f)) : 0), COFRE_Y - 54);
    } else if (abre) lv_obj_set_pos(s_cofre, COFRE_X - 30 + (int)(3 * sinf((float)s_ms / 40.0f)), COFRE_Y - 54);
    if (s_cofre_abre && !abre) s_cofre_abre = 0;

    /* los nidos de las gafas */
    const int dia = (int)(s_ultimo_t / 86400u);
    if (dia != s_nido_dia) { s_nido_dia = dia; s_nido_tocado = 0; }
    const bool nidos = pet_nests_visible(p, s_ultimo_t) && !s_interior && s_juego == JG_NADA && s_panel == PANEL_NINGUNO;
    for (int i = 0; i < PET_NIDOS; i++) {
        const bool ver = nidos && !(s_nido_tocado >> i & 1) && !pet_nest_empty(p, i, s_ultimo_t);
        ocultar(s_nido[i], !ver);
        if (!ver) continue;
        const int16_t *xy = NIDO_XY[s_escena == ESC_BOSQUE][i];
        lv_obj_set_pos(s_nido[i], xy[0] - 22, xy[1] - 16 + (int)(2 * sinf((float)s_ms / 500.0f + (float)i)));
    }

    /* las burbujas cuadradas */
    if (s_burb_hasta && (int32_t)(s_ms - s_burb_hasta) < 0 && s_mascota.visible) {
        if ((int32_t)(s_ms - s_burb_sig) >= 0) {
            for (int i = 0; i < N_BURB; i++) if (!s_burb_viva[i]) {
                s_burb_viva[i] = true;
                s_burb_x[i] = s_mascota.x + (s_mascota.pose.espejo ? -34.0f : 34.0f) - 15.0f;
                s_burb_y[i] = (float)cabeza_y(&s_mascota) + 10.0f;
                s_burb_f[i] = azar01() * 6.28f;
                ocultar(s_burb[i], false);
                lv_obj_move_foreground(s_burb[i]);
                break;
            }
            s_burb_sig = s_ms + 650 + azar() % 400;
        }
    }
    for (int i = 0; i < N_BURB; i++) {
        if (!s_burb_viva[i]) continue;
        s_burb_y[i] -= 3.2f;
        s_burb_x[i] += sinf(s_burb_f[i] + (float)s_ms / 420.0f) * 2.2f;
        if (s_burb_y[i] < 50 || s_panel != PANEL_NINGUNO) { /* se pierde sola: se deshace arriba */
            s_burb_viva[i] = false;
            ocultar(s_burb[i], true);
            continue;
        }
        lv_obj_set_pos(s_burb[i], (int)s_burb_x[i], (int)s_burb_y[i]);
    }

    /* la brújula loca: da vueltas y al final se clava hacia ella */
    if (s_bru_hasta && (int32_t)(s_ms - s_bru_hasta) < 0 && s_panel == PANEL_NINGUNO) {
        const float t = (float)(s_ms - s_bru_desde);
        const float objetivo = atan2f(s_mascota.x - (float)(BRU_X + 42), -((float)cabeza_y(&s_mascota) - (float)(BRU_Y + 42))) * 57.2958f; /* grados desde arriba, hacia la derecha */
        float ang = objetivo;
        if (t < 2400.0f) ang = objetivo + (1.0f - t / 2400.0f) * (t * 0.9f + 360.0f * sinf(t / 180.0f)); /* loca al principio */
        else ang = objetivo + 6.0f * sinf(t / 90.0f) * (t < 3600.0f ? 1.0f : 0.2f); /* y tiembla un poquito */
        lv_obj_set_style_transform_rotation(s_bru_aguja, (int32_t)(ang * 10.0f), 0);
        ocultar(s_bru, false);
        lv_obj_move_foreground(s_bru);
    } else if (!lv_obj_has_flag(s_bru, LV_OBJ_FLAG_HIDDEN)) { ocultar(s_bru, true); s_bru_hasta = 0; }

    /* el destello del teletransporte */
    if (s_tp_desde && s_ms - s_tp_desde < 800u) {
        const float f = sinf(3.1416f * (float)(s_ms - s_tp_desde) / 800.0f);
        lv_obj_set_style_opa(s_tp_flash, (lv_opa_t)(240.0f * f), 0);
        ocultar(s_tp_flash, false);
        lv_obj_move_foreground(s_tp_flash);
    } else if (s_tp_desde) { ocultar(s_tp_flash, true); s_tp_desde = 0; }
}
