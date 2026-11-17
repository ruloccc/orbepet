/* ======================================================================
 * La historia del mundo (versión 37): lo que se ve de los personajes nuevos (incluido desde ui.c, después de ui_magia.inc)
 *  - Tilde, la tenca: llega volando, salta de rama en rama, ladea la cabeza; tocarla cuenta su chisme (o lee la nota con que
 *    devuelve lo que se llevó); se va con lo que robó en el pico.
 *  - La Abuela Brizna: la isla respira (tiembla suavecito), asoma una cabeza enorme por la orilla, parpadea, bosteza, cuenta un recuerdo
 *    y se vuelve a dormir.
 *  - Las páginas del diario de Tilo: brillan, se desdoblan y quedan en el Libro.
 *  - Lo que dicen los animales: ella lo cuenta en su globo, con el ícono del animal.
 *  - Los lugares secretos: la casita escondida de Tilo (bosque) y el mirador de las nubes (colina): un fondo dibujado y cosas para tocar.
 * Todo con pocos objetos y sin sombras animadas (la placa es lenta con transparencias grandes).
 * ==================================================================== */

/* ---------------- Tilde ---------------- */
static lv_obj_t *s_ti_img, *s_ti_obj, *s_ti_burb;
static int s_ti_fase; /* 0 nada, 1 llega volando, 2 posada, 3 se va */
static uint32_t s_ti_t0, s_ti_salto;
static float s_ti_x, s_ti_y, s_ti_x0, s_ti_y0, s_ti_x1, s_ti_y1;
static int s_ti_perch, s_ti_lleva = -1; /* el objeto que se lleva en el pico (para dibujarlo) */
static bool s_ti_nota_leida;
/* dónde se posa en cada zona: copas de árboles, rocas y postes (coordenadas de pantalla) */
static const int16_t TI_PERCHAS[ESC_COUNT][3][2] = {
    {{78, 128}, {398, 150}, {300, 236}},  /* prado: el árbol viejo, el arbusto de flores, el pasto */
    {{330, 96}, {150, 150}, {290, 262}},  /* bosque */
    {{120, 186}, {350, 100}, {300, 280}}, /* lago: el sauce, el muelle, la orilla */
    {{132, 128}, {322, 222}, {250, 300}}, /* colina: el árbol rosado, la roca, el sendero */
    {{233, 233}, {233, 233}, {233, 233}}, /* cueva (no entra) */
    {{296, 96}, {392, 214}, {226, 300}},  /* isla: el cerezo, el faro, el pasto */
    {{204, 50}, {96, 170}, {244, 372}},   /* (v98) claro del rey: el techo de la casa, el cerquito del huerto, el tronco caído */
    {{295, 134}, {222, 104}, {164, 218}}, /* (v98) mirador: la apacheta, la roca del cóndor, la piedra tibia */
};
static void ti_toque_cb(lv_event_t *e)
{
    (void)e;
    if (!s_pet || s_ti_fase != 2) return;
    flotar(ICO_CORAZON, NULL, 0, (int)s_ti_x, (int)s_ti_y - 40, 0);
    accion(UI_ACCION_SONAR, SND_RISA);
    if (s_nino) { for (int k = 0; k < 3; k++) flotar(ICO_ESTRELLA, NULL, 0, (int)s_ti_x - 20 + k * 20, (int)s_ti_y - 30, (uint32_t)k * 120); return; }
    const int vuelve = pet_tilde_returns(s_pet);
    if (vuelve >= 0 && !s_ti_nota_leida) { /* primero, la nota con que devuelve lo que se llevó */
        char n[260];
        pet_tilde_note(s_pet, n, sizeof n);
        s_ti_nota_leida = true;
        accion(UI_ACCION_TILDE, 1);
        panel_lectura(ICO_TILDE, "Una nota de Tilde", "Te devuelvo algo", n);
        return;
    }
    const int k = pet_tilde_gossip(s_pet);
    if (k < 0) { globo(ICO_TILDE, "TILDE SOLO VINO A MIRAR COSAS BRILLANTES", 3000); return; }
    accion(UI_ACCION_TILDE, 0);
    panel_historia(NARR_TILDE, k);
}
static void ti_crear(void)
{
    s_ti_img = lv_image_create(s_mundo);
    lv_obj_add_flag(s_ti_img, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(s_ti_img, 10);
    lv_obj_add_event_cb(s_ti_img, ti_toque_cb, LV_EVENT_CLICKED, NULL);
    lv_image_set_pivot(s_ti_img, 24, 34);
    s_ti_obj = lv_image_create(s_mundo);
    lv_obj_remove_flag(s_ti_obj, LV_OBJ_FLAG_CLICKABLE);
    s_ti_burb = lv_image_create(s_mundo);
    lv_obj_remove_flag(s_ti_burb, LV_OBJ_FLAG_CLICKABLE);
    ocultar(s_ti_img, true); ocultar(s_ti_obj, true); ocultar(s_ti_burb, true);
}
static void ti_ir(float x1, float y1) { s_ti_x0 = s_ti_x; s_ti_y0 = s_ti_y; s_ti_x1 = x1; s_ti_y1 = y1; s_ti_t0 = s_ms; }
static void tilde_empieza(void)
{
    if (!s_pet || s_escena < 0 || s_escena >= ESC_COUNT) return;
    if (!s_ti_img) ti_crear();
    s_ti_fase = 1;
    s_ti_nota_leida = false;
    s_ti_perch = 0;
    s_ti_lleva = pet_tilde_takes(s_pet);
    s_ti_x = 500; s_ti_y = 40;
    ti_ir(TI_PERCHAS[s_escena][0][0], TI_PERCHAS[s_escena][0][1]);
    const bool primera = s_pet->tilde_visitas <= 1;
    globo(ICO_TILDE, s_nino ? NULL : (primera ? "¡UNA TENCA CON PAÑUELO! ¿QUIÉN SERÁ?" : "¡TILDE VINO DE VISITA!"), 3000);
}
static void tilde_se_va(int lleva)
{
    if (!s_ti_img || s_ti_fase == 0) return;
    s_ti_fase = 3;
    if (lleva >= 0) s_ti_lleva = lleva;
    ti_ir(-60, 30);
    if (s_ti_lleva >= 0 && !s_nino) {
        const objeto_info_t *o = objeto_info(s_ti_lleva);
        char t[96];
        snprintf(t, sizeof t, "¡TILDE SE LLEVÓ MI %s EN EL PICO!", o ? o->nombre : "TESORO");
        mayusculas(t);
        globo(ICO_OBJETO(s_ti_lleva), t, 3500);
    }
    accion(UI_ACCION_SONAR, SND_RISA);
}
static void animar_tilde(void)
{
    if (!s_ti_img) return;
    const bool aqui = s_pet && pet_tilde_here(s_pet, s_ultimo_t) && s_escena == (int)s_pet->escena && !s_interior;
    if (s_ti_fase == 2 && !aqui) tilde_se_va(-1);
    if (s_ti_fase == 0) { ocultar(s_ti_img, true); ocultar(s_ti_obj, true); ocultar(s_ti_burb, true); return; }
    const float f = s_ms - s_ti_t0 > 1400 ? 1.0f : (float)(s_ms - s_ti_t0) / 1400.0f;
    const float e = f * f * (3 - 2 * f);
    s_ti_x = s_ti_x0 + (s_ti_x1 - s_ti_x0) * e;
    s_ti_y = s_ti_y0 + (s_ti_y1 - s_ti_y0) * e - sinf(f * 3.1416f) * (s_ti_fase == 2 ? 26.0f : 40.0f);
    bool vuela = f < 1.0f;
    if (s_ti_fase == 1 && f >= 1.0f) { s_ti_fase = 2; s_ti_salto = s_ms + 3500; }
    if (s_ti_fase == 2 && (int32_t)(s_ms - s_ti_salto) > 0) { /* salta a otra rama */
        s_ti_perch = (s_ti_perch + 1 + (int)(azar() % 2)) % 3;
        ti_ir(TI_PERCHAS[s_escena][s_ti_perch][0], TI_PERCHAS[s_escena][s_ti_perch][1]);
        s_ti_salto = s_ms + 3800 + azar() % 2500;
    }
    if (s_ti_fase == 3 && f >= 1.0f) { s_ti_fase = 0; s_ti_lleva = -1; return; }
    const bool espejo = s_ti_x1 < s_ti_x0;
    lv_image_set_src(s_ti_img, espejo ? art_icono_espejo(vuela && (s_ms / 120) & 1 ? ICO_TILDE_B : ICO_TILDE, 4) : art_icono(vuela && (s_ms / 120) & 1 ? ICO_TILDE_B : ICO_TILDE, 4));
    /* posada: ladea la cabeza de vez en cuando (curiosa) */
    lv_image_set_rotation(s_ti_img, !vuela && (s_ms / 1400) % 3 == 1 ? (espejo ? 140 : -140) : 0);
    ocultar(s_ti_img, false);
    lv_obj_set_pos(s_ti_img, (int)s_ti_x - 24, (int)s_ti_y - 44);
    lv_obj_move_foreground(s_ti_img);
    /* lo que se lleva, en el pico (se ve desde que llega: ya le echó el ojo) */
    if (s_ti_lleva >= 0) {
        lv_image_set_src(s_ti_obj, art_icono(ICO_OBJETO(s_ti_lleva), 2));
        lv_obj_set_pos(s_ti_obj, (int)s_ti_x + (espejo ? -34 : 12), (int)s_ti_y - 40);
        ocultar(s_ti_obj, false);
        lv_obj_move_foreground(s_ti_obj);
    } else ocultar(s_ti_obj, true);
    /* una burbujita: tiene algo que contar (o una nota) */
    const bool algo = s_ti_fase == 2 && !s_nino && ((pet_tilde_returns(s_pet) >= 0 && !s_ti_nota_leida) || (pet_tilde_gossip(s_pet) >= 0 && !(s_pet->tilde_oido & 1)));
    ocultar(s_ti_burb, !algo);
    if (algo) {
        lv_image_set_src(s_ti_burb, art_icono(pet_tilde_returns(s_pet) >= 0 && !s_ti_nota_leida ? ICO_CARTA : ICO_PREGUNTA, 2));
        lv_obj_set_pos(s_ti_burb, (int)s_ti_x + 14, (int)s_ti_y - 78 + (int)(sinf((float)s_ms / 250.0f) * 3));
        lv_obj_move_foreground(s_ti_burb);
    }
}

/* ---------------- la Abuela Brizna ---------------- */
static lv_obj_t *s_br_img;
static uint32_t s_br_t0;
static int s_br_k = -2; /* -2 dormida; -1 solo saluda; 0.. el recuerdo */
static bool s_br_conto, s_br_va;
#define BR_X 376 /* la orilla de la isla, abajo a la derecha (en el agua, junto a las rocas) */
#define BR_Y 366
static void br_toque_cb(lv_event_t *e)
{
    (void)e;
    if (!s_pet || s_br_k < -1) return;
    if (s_br_k >= 0) panel_historia(NARR_BRIZNA, s_br_k);
    else globo(ICO_BRIZNA, pet_brizna_hello((int)(s_ms / 1000)), 4000);
}
static void brizna_despierta(int k)
{
    if (!s_br_img) {
        s_br_img = lv_image_create(s_mundo);
        lv_obj_add_flag(s_br_img, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(s_br_img, br_toque_cb, LV_EVENT_CLICKED, NULL);
    }
    s_br_k = k;
    s_br_t0 = s_ms;
    s_br_conto = false;
    s_br_va = false;
    accion(UI_ACCION_SONAR, SND_BOSTEZO);
}
static void animar_brizna(void)
{
    if (!s_br_img || s_br_k < -1) return;
    const bool ver = s_pet && s_escena == ESC_ISLA && !s_interior;
    const uint32_t t = s_ms - s_br_t0;
    if (!ver) { ocultar(s_br_img, true); lv_obj_set_style_translate_x(s_mundo, 0, 0); return; }
    /* la isla respira: tiembla suavecito al principio */
    lv_obj_set_style_translate_x(s_mundo, t < 2200 ? (int)(sinf((float)t / 45.0f) * 3.0f * (1.0f - (float)t / 2200.0f)) : 0, 0);
    if (t < 1200) { ocultar(s_br_img, true); return; }
    /* asoma despacio, parpadea, bosteza; al irse, baja */
    float sube = t < 4200 ? (float)(t - 1200) / 3000.0f : 1.0f;
    if (s_br_va) { const uint32_t tv = s_ms - s_br_t0; (void)tv; }
    art_icono_t ic = ICO_BRIZNA;
    if (t < 3600) ic = ICO_BRIZNA_B;
    else if (t > 6000 && t < 7600) ic = ICO_BRIZNA_C;
    else if ((t / 250) % 14 == 0) ic = ICO_BRIZNA_B; /* parpadea despacio */
    if (s_br_va) ic = ICO_BRIZNA_B;
    lv_image_set_src(s_br_img, art_icono(ic, 6));
    const int y = BR_Y - (int)(sube * 70.0f) + (int)(sinf((float)s_ms / 900.0f) * 2.0f);
    ocultar(s_br_img, false);
    lv_obj_set_pos(s_br_img, BR_X - 72, y - 72);
    if (t > 3000 && t % 1600 < TICK_MS) onda(BR_X, BR_Y + 10, 0x9ED8FF, 70, true, 0); /* ondas en el agua */
    if (t > 8200 && !s_br_conto && s_panel == PANEL_NINGUNO) { /* cuenta su recuerdo (y lo deja en el Libro) */
        s_br_conto = true;
        accion(UI_ACCION_BRIZNA, 0);
        if (s_br_k >= 0) panel_historia(NARR_BRIZNA, s_br_k);
        else globo(ICO_BRIZNA, s_nino ? NULL : pet_brizna_hello((int)(s_ultimo_t / 86400)), 5000);
    }
}
static void brizna_duerme(void)
{
    if (!s_br_img) return;
    s_br_va = true;
    s_br_k = -2;
    ocultar(s_br_img, true);
    lv_obj_set_style_translate_x(s_mundo, 0, 0);
    if (s_escena == ESC_ISLA) for (int k = 0; k < 3; k++) flotar(ICO_ESTRELLA, NULL, 0, BR_X - 20 + k * 20, BR_Y - 40, (uint32_t)k * 200);
}

/* ---------------- las páginas del diario de Tilo: brillan, se desdoblan y quedan en el Libro ---------------- */
static int s_pg_k = -1, s_pg_rep = -1; /* (v39) rep: la página suelta mostró una ya leída */
static bool s_pg_ya;
static uint32_t s_pg_listo;
static void pg_escala_cb(void *var, int32_t v) { lv_image_set_scale((lv_obj_t *)var, (uint32_t)v); }
static void pg_gira_cb(void *var, int32_t v) { lv_image_set_rotation((lv_obj_t *)var, v); }
static void pg_leer_cb(lv_event_t *e) { (void)e; if (s_panel == PANEL_PAGINA && s_pg_k >= 0) { const int k = s_pg_k; s_pg_k = -1; panel_historia(NARR_TILO, k); } }
static void panel_pagina(int k)
{
    if (!s_pet || (unsigned)k >= PET_TILO_PAGINAS) return;
    s_pg_k = k;
    abrir_tarjeta(PANEL_PAGINA, 466, 466);
    hist_fondo();
    fin_aro(s_tarjeta, CX, 170, 0, true);
    fin_aro(s_tarjeta, CX, 170, 900, true);
    fin_luciernagas(s_tarjeta, 8, 300, 420);
    lv_obj_t *pg = icono(s_tarjeta, ICO_PAGINA_TILO, 5);
    lv_obj_align(pg, LV_ALIGN_TOP_MID, 0, 110);
    lv_image_set_pivot(pg, 60, 60);
    { lv_anim_t a; lv_anim_init(&a); lv_anim_set_var(&a, pg); lv_anim_set_values(&a, 40, 256); lv_anim_set_time(&a, 1300); lv_anim_set_path_cb(&a, lv_anim_path_overshoot); lv_anim_set_exec_cb(&a, pg_escala_cb); lv_anim_start(&a); }
    { lv_anim_t a; lv_anim_init(&a); lv_anim_set_var(&a, pg); lv_anim_set_values(&a, -1800, 0); lv_anim_set_time(&a, 1300); lv_anim_set_exec_cb(&a, pg_gira_cb); lv_anim_start(&a); }
    lv_obj_t *x = texto_c(s_tarjeta, s_nino ? "" : (s_pg_ya ? "ESTA YA LA LEÍMOS" : "UNA PÁGINA VIEJA VOLVIÓ AL LIBRO"), FIN_COLOR_ORO, 2, 330);
    lv_obj_align(x, LV_ALIGN_TOP_MID, 0, 250);
    fin_aparecer(x, 1200);
    if (!s_nino) {
        char t[96];
        snprintf(t, sizeof t, "«%s»", pet_story_title((magica_t)NARR_TILO, k));
        mayusculas(t);
        x = texto_c(s_tarjeta, t, 0xFFEFB8, 2, 330);
        lv_obj_align(x, LV_ALIGN_TOP_MID, 0, 306);
        fin_aparecer(x, 1700);
        x = texto_c(s_tarjeta, "TOCA PARA LEERLA", 0xC8C4E0, 2, 300);
        lv_obj_align(x, LV_ALIGN_TOP_MID, 0, 356);
        fin_aparecer(x, 2400);
    }
    lv_obj_add_flag(s_tarjeta, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_tarjeta, pg_leer_cb, LV_EVENT_CLICKED, NULL);
    accion(UI_ACCION_TILO, k);
    accion(UI_ACCION_SONAR, SND_SORPRESA);
}
static void pagina_revisar(void)
{
    if (!s_pet || s_panel != PANEL_NINGUNO || s_cine_pend || s_bienv_activa || s_pecera || s_pet->dormida || s_hist_pend_c >= 0) return;
    if ((int32_t)(s_ms - s_pg_listo) < 0) return;
    if (s_pg_rep >= 0) { const int r = s_pg_rep; s_pg_rep = -1; if (!s_nino) { s_pg_ya = true; panel_pagina(r); s_pg_ya = false; } return; } /* (v39) la página suelta */
    const int k = pet_tilo_page_new(s_pet);
    if (k < 0) return;
    if (s_nino) { accion(UI_ACCION_TILO, k); return; } /* (los más chicos no leen: queda en el Libro) */
    s_pg_listo = s_ms + 4000;
    panel_pagina(k);
}

/* ---------------- los lugares secretos ---------------- */
static lv_obj_t *s_lu_entrada;
static int s_lu_cual = -1;
/* las cosas que se tocan en cada fondo (centro y radio) */
static const int16_t LU_COSAS[PET_LUGARES][4][3] = {
    {{100, 248, 56}, {365, 318, 58}, {326, 196, 40}, {220, 342, 52}}, /* casita: el horno, la camita, el gancho, la mesa */
    {{235, 360, 58}, {384, 276, 40}, {112, 196, 58}, {0, 0, 0}},      /* mirador: la piedra, la corona, la nube */
};
static const int16_t LU_ENTRADA[PET_LUGARES][2] = {{412, 236}, {228, 72}}; /* dónde se ve la entrada: el sendero del bosque, la cima de la colina */
static lv_obj_t *s_lu_texto, *s_lu_marca[4];
static void panel_lugar(int l);
static void lu_cosa_cb(lv_event_t *e)
{
    const int c = (int)(intptr_t)lv_event_get_user_data(e);
    if (s_panel != PANEL_LUGAR || s_lu_cual < 0 || !s_pet) return;
    const int l = s_lu_cual;
    const bool nueva = !pet_secret_found(s_pet, l, c);
    accion(UI_ACCION_LUGAR, l * 16 + c);
    if (nueva) { chispas(LU_COSAS[l][c][0], LU_COSAS[l][c][1], 0xFFE27A, 6, 24); accion(UI_ACCION_SONAR, SND_TESORO); }
    else accion(UI_ACCION_SONAR, SND_TOQUE);
    if (s_lu_marca[c]) ocultar(s_lu_marca[c], true);
    if (s_lu_texto) lv_obj_delete(s_lu_texto);
    s_lu_texto = lv_obj_create(s_tarjeta); /* el recuerdo de esa cosa, en una placa abajo */
    lv_obj_remove_style_all(s_lu_texto);
    lv_obj_set_size(s_lu_texto, 320, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(s_lu_texto, lv_color_hex(0x1A1430), 0);
    lv_obj_set_style_bg_opa(s_lu_texto, LV_OPA_80, 0);
    lv_obj_set_style_radius(s_lu_texto, 18, 0);
    lv_obj_set_style_pad_all(s_lu_texto, 12, 0);
    lv_obj_set_style_border_width(s_lu_texto, 2, 0);
    lv_obj_set_style_border_color(s_lu_texto, lv_color_hex(0xD0B969), 0);
    lv_obj_set_flex_flow(s_lu_texto, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(s_lu_texto, 6, 0);
    lv_obj_remove_flag(s_lu_texto, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    char t[400];
    snprintf(t, sizeof t, "%s", pet_secret_title(l, c));
    mayusculas(t);
    texto(s_lu_texto, t, FIN_COLOR_ORO, 2, 296);
    snprintf(t, sizeof t, "%s", pet_secret_text(l, c, modo_nino_lee() || s_nino));
    mayusculas(t);
    if (!s_nino) texto(s_lu_texto, t, 0xFFF3C8, 2, 296);
    lv_obj_align(s_lu_texto, LV_ALIGN_TOP_MID, 0, 84); /* arriba: abajo están la X y ella */
    if (l == 0 && c == 1) globo(ICO_CORAZON, s_nino ? NULL : "¡ME QUEDA PERFECTA! ES DE MI TAMAÑO", 3500);
    if (l == 1 && c == 0) globo(ICO_ESTRELLA, s_nino ? NULL : "EL HUECO ES DE MI TAMAÑO... ¿QUIÉN SE SENTABA AQUÍ?", 3500);
}
static void panel_lugar(int l)
{
    if (!s_pet || (unsigned)l >= PET_LUGARES) return;
    s_lu_cual = l;
    abrir_tarjeta(PANEL_LUGAR, 466, 466);
    lv_obj_align(s_tarjeta, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_opa(s_tarjeta, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_tarjeta, 0, 0);
    lv_obj_set_style_shadow_width(s_tarjeta, 0, 0);
    int px = 0, py = 0;
    const lv_image_dsc_t *f = art_pantalla(l == 0 ? 5 : 6, &px, &py);
    if (f) { lv_obj_t *im = lv_image_create(s_tarjeta); lv_image_set_src(im, f); lv_obj_set_pos(im, px, py); lv_obj_remove_flag(im, LV_OBJ_FLAG_CLICKABLE); }
    else hist_fondo();
    s_lu_texto = NULL;
    lv_obj_t *nom = texto_c(s_tarjeta, s_nino ? "" : pet_secret_place_name(l), l == 0 ? 0x5A3A22 : FIN_COLOR_ORO, 2, 240);
    lv_obj_align(nom, LV_ALIGN_TOP_MID, 0, 58);
    for (int c = 0; c < 4; c++) {
        s_lu_marca[c] = NULL;
        if (c >= pet_secret_things(l)) continue;
        lv_obj_t *b = lv_obj_create(s_tarjeta); /* el lugar que se toca: invisible, con una chispa si todavía no lo descubre */
        lv_obj_remove_style_all(b);
        const int r = LU_COSAS[l][c][2];
        lv_obj_set_size(b, r * 2, r * 2);
        lv_obj_set_pos(b, LU_COSAS[l][c][0] - r, LU_COSAS[l][c][1] - r);
        lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(b, lu_cosa_cb, LV_EVENT_CLICKED, (void *)(intptr_t)c);
        if (!pet_secret_found(s_pet, l, c)) {
            lv_obj_t *m = icono(s_tarjeta, ICO_ESTRELLA, 2);
            lv_obj_set_pos(m, LU_COSAS[l][c][0] - 12, LU_COSAS[l][c][1] - r / 2 - 12);
            lv_obj_remove_flag(m, LV_OBJ_FLAG_CLICKABLE);
            fin_aro(s_tarjeta, LU_COSAS[l][c][0], LU_COSAS[l][c][1] - r / 2, (uint32_t)c * 500, true);
            s_lu_marca[c] = m;
        }
    }
    /* ella, chiquitita, mirando todo */
    lv_obj_t *m = mini_criatura(s_tarjeta, PET_MAX_AMIGOS + 1, pet_look(s_pet), s_pet->etapa, s_pet->forma, 3, EXP_CURIOSO);
    lv_obj_set_pos(m, l == 0 ? 96 : 118, l == 0 ? 318 : 316);
    lv_obj_remove_flag(m, LV_OBJ_FLAG_CLICKABLE);
    accion(UI_ACCION_SONAR, SND_SORPRESA);
}
static void lu_entrada_cb(lv_event_t *e) { (void)e; if (s_pet && s_panel == PANEL_NINGUNO) panel_lugar(s_escena == ESC_BOSQUE ? 0 : 1); }
static void animar_lugares(void)
{
    if (!s_pet) return;
    const int l = s_escena == ESC_BOSQUE ? 0 : (s_escena == ESC_COLINA ? 1 : -1);
    const bool ver = false && l >= 0; /* (v39) ya no: se entra al claro (caminito de luciérnagas) y al mirador (la nube), ui_zonas.inc */
    if (!s_lu_entrada && ver) {
        s_lu_entrada = lv_image_create(s_mundo);
        lv_obj_add_flag(s_lu_entrada, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_ext_click_area(s_lu_entrada, 14);
        lv_obj_add_event_cb(s_lu_entrada, lu_entrada_cb, LV_EVENT_CLICKED, NULL);
    }
    if (!s_lu_entrada) return;
    ocultar(s_lu_entrada, !ver);
    if (!ver) return;
    lv_image_set_src(s_lu_entrada, art_icono(l == 0 ? ICO_CUCHARA_PALO : ICO_NUBE, 2));
    lv_obj_set_pos(s_lu_entrada, LU_ENTRADA[l][0] - 24, LU_ENTRADA[l][1] - 24 + (int)(sinf((float)s_ms / 500.0f) * 3));
    if (s_ms % 1400 < TICK_MS) chispas(LU_ENTRADA[l][0], LU_ENTRADA[l][1], 0xFFE27A, 2, 16);
}

/* ---------------- cada cuadro ---------------- */
static void animar_historia(void)
{
    animar_tilde();
    animar_brizna();
    animar_lugares();
    pagina_revisar();
    if (s_pet && s_ti_fase == 0 && pet_tilde_here(s_pet, s_ultimo_t) && s_escena == (int)s_pet->escena && !s_interior && s_panel == PANEL_NINGUNO)
        tilde_empieza(); /* (si llegó mientras no se veía) */
    int k;
    if (s_pet && s_br_k < -1 && pet_brizna_awake(s_pet, s_ultimo_t, &k) && s_escena == ESC_ISLA && !s_interior) brizna_despierta(k);
}
/* ---------------- los avisos del motor ---------------- */
static void evento_historia(const pet_event_t *e)
{
    if (!s_pet) return;
    char t[200];
    switch (e->type) {
    case EV_TILDE:
        if (e->a == 0) { if (s_escena == (int)s_pet->escena && !s_interior) tilde_empieza(); }
        else tilde_se_va(e->b);
        break;
    case EV_BRIZNA:
        if (e->b == 0) { if (s_escena == ESC_ISLA) brizna_despierta(e->a); }
        else brizna_duerme();
        break;
    case EV_PRISMA: s_hist_pend_c = NARR_PRISMA; s_hist_pend_k = (uint8_t)e->a % PET_PRISMA_CAPS; break;
    case EV_TILO: s_pg_listo = s_ms + 2500; break; /* (sale sola cuando la pantalla quede libre: pagina_revisar) */
    case EV_PAGINA: /* (v39) la página suelta: hoy muestra una ya leída (o se ve borrosa) */
        if (e->a >= 0) { s_pg_rep = e->a; s_pg_listo = s_ms + 1200; }
        else globo(ICO_PAGINA_TILO, s_nino ? NULL : "LA LETRA SE VE BORROSA... MAÑANA LA INTENTO LEER", 3500);
        break;
    case EV_ANIMAL_DICE: {
        if (s_nino) break;
        const int f = (uint8_t)e->a, i = (uint8_t)e->b;
        /* (fw 103) solo lo que dijo: el «lo guardé en el libro» en cada mensaje cansaba (igual queda guardado) */
        snprintf(t, sizeof t, "%s", pet_animal_phrase(i, modo_nino_lee()));
        mayusculas(t);
        globo(f < FAUNA_COUNT ? ICO_FAUNA[f] : ICO_ANIMAL[f - FAUNA_COUNT], t, 9000); /* (v39) dura más; se cierra con doble toque */
        break;
    }
    case EV_LUGAR:
        if (e->b < 0) {
            globo(e->a != 1 ? ICO_ESTRELLA : ICO_NUBE, s_nino ? NULL : (e->a != 1 ? "¡LUMBRE DEJÓ UN CAMINITO DE LUCIÉRNAGAS HACIA EL BORDE DEL BOSQUE! ¿ADÓNDE LLEVARÁ?" : "¡DESDE LA CIMA SE PUEDE SUBIR MÁS ALTO, EN ESA NUBE BAJITA!"), 4500);
            accion(UI_ACCION_SONAR, SND_SORPRESA);
        }
        break;
    default: break;
    }
}
/* (pruebas) */
void ui_demo_tilde(void) { if (s_pet) { tilde_empieza(); } }
void ui_demo_brizna(int k) { brizna_despierta(k); }
void ui_demo_pagina(int k) { cerrar_panel(); panel_pagina(k); }
void ui_demo_lugar(int l) { cerrar_panel(); panel_lugar(l); }
