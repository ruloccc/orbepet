/* ======================================================================
 * (v98) Los juegos de los lugares del Rey Peumo: cinco juegos que se repiten, cada uno en su pantalla.
 * Incluido desde ui_zonas.inc. Reemplazan a los de la v97 (se jugaban una vez y ya).
 *   Casita:   GALLETAS DE PEUMO (piñón y miel de ulmo: la mezcla, el horno que se aviva SOPLANDO y sacarlas a tiempo)
 *             y CANCIÓN DE CUNA (tocar al ritmo; espantar las polillas antes de que apaguen las velas).
 *   Mirador:  SALTO AL CIELO (de nube en nube, chiquitas, hacia los lados: normales, frágiles, saltarinas y que se mueven;
 *             plumas y el cóndor que te sube), ATRAPA HOJAS (con la canasta: doradas, secas, piñas que pinchan y el chucao
 *             que roba) y LA PIEDRA TIBIA (adivinar dónde está tibia; la lagartija la cambia de lugar; cambia cada día).
 * Todos tienen puntaje, récord, de 1 a 3 estrellas y premios (el motor: pet_place_game_result), rondas cortas que se ponen
 * más difíciles y una versión para los más chiquitos (modo infante: más lento, sin peligros, siempre se gana algo).
 * La primera vez que se juega cada uno, el motor anota el secreto de esa cosa (como antes). Al terminar: OTRA VEZ o la X.
 * Corre con su propio reloj de 33 ms mientras está abierto.
 * ==================================================================== */
#define ZJ_MS 33
#define ZJ_N 56          /* objetos movibles del juego */
#define ZJ_CHISPAS 24
static lv_obj_t *s_zj, *s_zj_fondo, *s_zj_tit, *s_zj_tit_img, *s_zj_txt, *s_zj_txt_img, *s_zj_x, *s_zj_hud, *s_zj_hud_img, *s_zj_barra, *s_zj_barra_f;
static lv_obj_t *s_zj_o[ZJ_N], *s_zj_ch[ZJ_CHISPAS], *s_zj_fin, *s_zj_mas, *s_zj_mas_img;
static art_img_t s_zj_tit_art, s_zj_txt_art, s_zj_hud_art, s_zj_mas_art, s_zj_pet_art[3], s_zj_fin_art[3];
static lv_timer_t *s_zj_timer;
static int s_zj_jg, s_zj_paso, s_zj_cuenta, s_zj_trampa, s_zj_fase, s_zj_pts, s_zj_pts_vis = -1, s_zj_vidas, s_zj_ronda, s_zj_combo, s_zj_estrellas;
static uint32_t s_zj_t, s_zj_t0, s_zj_txt_hasta, s_zj_fin_t, s_zj_juego_t, s_zj_dura, s_zj_mas_t;
static bool s_zj_gano, s_zj_dedo;
static float s_zj_dedo_x, s_zj_dedo_y;
static float s_zj_x_[ZJ_N], s_zj_y_[ZJ_N], s_zj_v[ZJ_N], s_zj_f[ZJ_N], s_zj_g[ZJ_N];
static int8_t s_zj_tipo[ZJ_N];
static float s_zj_chx[ZJ_CHISPAS], s_zj_chy[ZJ_CHISPAS], s_zj_chvx[ZJ_CHISPAS], s_zj_chvy[ZJ_CHISPAS], s_zj_chg[ZJ_CHISPAS];
static uint32_t s_zj_chv[ZJ_CHISPAS];
static float s_zj_calor, s_zj_scroll, s_zj_hx, s_zj_hy, s_zj_px, s_zj_py, s_zj_pvx, s_zj_pvy, s_zj_prog, s_zj_quema;
static uint32_t s_zj_salto_t, s_zj_rafaga_t, s_zj_proxima, s_zj_prox2;
static int s_zj_toques;
static uint32_t s_zj_ult;
static void zj_cerrar(bool gano);
static bool s_zj_sin_fondo;
static void zj_terminar(void);

/* ---------- ayudas ---------- */
static int zj_motor(int jg) { return jg - 1; } /* ZJ_HORNO.. -> PET_JL_GALLETAS.. */
static void zj_lugar_cosa(int jg, int *l, int *c) /* qué cosa del lugar es cada juego (el secreto del motor) */
{
    switch (jg) {
    case ZJ_HORNO: *l = 0; *c = 0; break;
    case ZJ_CAMITA: *l = 0; *c = 1; break;
    case ZJ_PIEDRA: *l = 1; *c = 0; break;
    case ZJ_CORONA: *l = 1; *c = 1; break;
    case ZJ_NUBE: *l = 1; *c = 2; break;
    default: *l = -1; *c = -1; break;
    }
}
static void zj_texto(const char *t, uint32_t ms)
{
    if (!s_zj_txt) return;
    if (!t || s_nino) { ocultar(s_zj_txt, true); return; }
    char b[300];
    snprintf(b, sizeof b, "%s", gtxt(t));
    mayusculas(b);
    art_texto(&s_zj_txt_art, b, 0xFFF3C8, 2, 300);
    poner_src(s_zj_txt_img, &s_zj_txt_art.dsc);
    lv_obj_update_layout(s_zj_txt);
    lv_obj_align(s_zj_txt, LV_ALIGN_TOP_MID, 0, 96);
    ocultar(s_zj_txt, false);
    lv_obj_move_foreground(s_zj_txt);
    s_zj_txt_hasta = s_zj_t + (ms ? ms : 3200u + (uint32_t)strlen(b) * 55u);
}
static void zj_titulo(const char *t)
{
    if (!s_zj_tit) return;
    if (s_nino || !t) { ocultar(s_zj_tit, true); return; }
    art_texto(&s_zj_tit_art, gtxt(t), 0xFFE9A0, 2, 0);
    poner_src(s_zj_tit_img, &s_zj_tit_art.dsc);
    lv_obj_update_layout(s_zj_tit);
    lv_obj_align(s_zj_tit, LV_ALIGN_TOP_MID, 0, 40);
    ocultar(s_zj_tit, false);
}
static lv_obj_t *zj_img(int i, const lv_image_dsc_t *d)
{
    if (i < 0 || i >= ZJ_N) return NULL;
    if (!s_zj_o[i]) { s_zj_o[i] = lv_image_create(s_zj); lv_obj_remove_flag(s_zj_o[i], LV_OBJ_FLAG_CLICKABLE); }
    poner_src(s_zj_o[i], d);
    ocultar(s_zj_o[i], false);
    return s_zj_o[i];
}
static lv_obj_t *zj_caja(int i, int w, int h, uint32_t color, int radio)
{
    if (i < 0 || i >= ZJ_N) return NULL;
    if (!s_zj_o[i]) { s_zj_o[i] = caja(s_zj); }
    lv_obj_t *o = s_zj_o[i];
    lv_obj_set_size(o, w, h);
    lv_obj_set_style_radius(o, radio, 0);
    lv_obj_set_style_bg_color(o, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    ocultar(o, false);
    return o;
}
static void zj_centro(lv_obj_t *o, float x, float y)
{
    if (!o) return;
    const lv_image_dsc_t *d = lv_obj_check_type(o, &lv_image_class) ? lv_image_get_src(o) : NULL;
    const int w = d ? (int)d->header.w : (int)lv_obj_get_style_width(o, LV_PART_MAIN), h = d ? (int)d->header.h : (int)lv_obj_get_style_height(o, LV_PART_MAIN);
    lv_obj_set_pos(o, (int)x - w / 2, (int)y - h / 2);
}
static bool zj_visible(int i) { return s_zj_o[i] && !lv_obj_has_flag(s_zj_o[i], LV_OBJ_FLAG_HIDDEN); }
static void zj_tinte(lv_obj_t *o, uint32_t c, lv_opa_t a) { if (!o) return; lv_obj_set_style_image_recolor(o, lv_color_hex(c), 0); lv_obj_set_style_image_recolor_opa(o, a, 0); }
/* ella en el juego (k: 0 de pie, 2 sentada, 3 durmiendo, 4 feliz, 5 curiosa) */
static const lv_image_dsc_t *zj_ella(int slot, int k, int escala)
{
    if (!s_pet || slot < 0 || slot > 2) return NULL;
    static const art_expresion_t EX[7] = {EXP_FELIZ, EXP_FELIZ, EXP_FELIZ, EXP_CERRADOS, EXP_FELIZ, EXP_CURIOSO, EXP_FELIZ};
    const pet_t *m = s_pet;
    art_pose_t po = {pet_look(m), m->etapa, m->etapa == STAGE_ADULTA ? m->forma : 0, escala, EX[k % 7], k == 4, 0, false, false,
                     1, k == 4 ? 1 : 0, k == 2 ? -1 : (k == 3 ? -2 : 0), {0, 0, 0, 0}, 0, m->guardiana};
    for (int r = 0; r < RANURA_COUNT; r++) po.ropa[r] = pet_worn_raw(m, r);
    art_criatura_pose(&s_zj_pet_art[slot], &po);
    return s_zj_pet_art[slot].px ? &s_zj_pet_art[slot].dsc : NULL;
}
/* chispitas de colores: salen de (x, y) y caen o suben */
static void zj_chispas(float x, float y, int n, uint32_t color, float fuerza, float grav)
{
    for (int k = 0; k < n; k++) {
        int i = -1;
        for (int j = 0; j < ZJ_CHISPAS; j++) if (!s_zj_chv[j]) { i = j; break; }
        if (i < 0) return;
        if (!s_zj_ch[i]) { s_zj_ch[i] = caja(s_zj); lv_obj_set_style_bg_opa(s_zj_ch[i], LV_OPA_COVER, 0); }
        static const uint32_t CONF[6] = {0xFFE27A, 0xFF8FA8, 0x9FD8F0, 0xB6F0A0, 0xC8A8FF, 0xFFFFFF};
        const uint32_t c = color ? color : CONF[azar() % 6];
        const int t = 5 + (int)(azar() % 4);
        lv_obj_set_size(s_zj_ch[i], t, t);
        lv_obj_set_style_radius(s_zj_ch[i], color ? LV_RADIUS_CIRCLE : 2, 0);
        lv_obj_set_style_bg_color(s_zj_ch[i], lv_color_hex(c), 0);
        lv_obj_set_style_opa(s_zj_ch[i], LV_OPA_COVER, 0);
        const float a = azar01() * 6.283f, v = fuerza * (0.4f + azar01() * 0.8f);
        s_zj_chx[i] = x; s_zj_chy[i] = y; s_zj_chvx[i] = cosf(a) * v; s_zj_chvy[i] = sinf(a) * v - fuerza * 0.6f;
        s_zj_chv[i] = s_zj_t + 900u + azar() % 500u;
        s_zj_chg[i] = grav;
        ocultar(s_zj_ch[i], false);
        lv_obj_move_foreground(s_zj_ch[i]);
    }
}
static void zj_chispas_animar(float dt)
{
    for (int i = 0; i < ZJ_CHISPAS; i++) {
        if (!s_zj_chv[i] || !s_zj_ch[i]) continue;
        if ((int32_t)(s_zj_t - s_zj_chv[i]) > 0) { s_zj_chv[i] = 0; ocultar(s_zj_ch[i], true); continue; }
        s_zj_chvy[i] += s_zj_chg[i] * dt;
        s_zj_chx[i] += s_zj_chvx[i] * dt; s_zj_chy[i] += s_zj_chvy[i] * dt;
        lv_obj_set_pos(s_zj_ch[i], (int)s_zj_chx[i], (int)s_zj_chy[i]);
        const int32_t queda = (int32_t)(s_zj_chv[i] - s_zj_t);
        if (queda < 400) lv_obj_set_style_opa(s_zj_ch[i], (lv_opa_t)(queda * 255 / 400), 0);
    }
}
/* el puntaje arriba (se vuelve a escribir solo cuando cambia) y la barrita del tiempo que queda */
static void zj_hud(void)
{
    if (!s_zj_hud || s_zj_fase != 1) return;
    if (s_zj_pts != s_zj_pts_vis) {
        s_zj_pts_vis = s_zj_pts;
        char b[24];
        snprintf(b, sizeof b, "%d", s_zj_pts);
        art_texto(&s_zj_hud_art, b, 0xFFE27A, 3, 0);
        poner_src(s_zj_hud_img, &s_zj_hud_art.dsc);
        lv_obj_update_layout(s_zj_hud);
        lv_obj_align(s_zj_hud, LV_ALIGN_TOP_MID, 0, 26);
        ocultar(s_zj_hud, false);
    }
    if (s_zj_barra && s_zj_dura) {
        const int32_t pasa = (int32_t)(s_zj_t - s_zj_juego_t);
        float f = 1.0f - (float)pasa / (float)s_zj_dura;
        if (f < 0) f = 0;
        lv_obj_set_width(s_zj_barra_f, 4 + (int)(f * 152.0f));
        lv_obj_set_style_bg_color(s_zj_barra_f, lv_color_hex(f > 0.3f ? 0x9FE87A : 0xFF8A5A), 0);
    }
}
/* "+5" que sube donde pasó algo */
static void zj_mas(int n, float x, float y, uint32_t color)
{
    if (!s_zj_mas || s_nino) return;
    char b[12];
    snprintf(b, sizeof b, n >= 0 ? "+%d" : "%d", n);
    art_texto(&s_zj_mas_art, b, color, 3, 0);
    poner_src(s_zj_mas_img, &s_zj_mas_art.dsc);
    lv_obj_update_layout(s_zj_mas);
    s_zj_mas_t = s_zj_t; s_zj_f[ZJ_N - 1] = x; s_zj_g[ZJ_N - 1] = y;
    ocultar(s_zj_mas, false);
    lv_obj_move_foreground(s_zj_mas);
}
static void zj_mas_animar(void)
{
    if (!s_zj_mas || lv_obj_has_flag(s_zj_mas, LV_OBJ_FLAG_HIDDEN)) return;
    const float f = (float)(s_zj_t - s_zj_mas_t) / 800.0f;
    if (f >= 1.0f) { ocultar(s_zj_mas, true); return; }
    const int w = (int)lv_obj_get_width(s_zj_mas), h = (int)lv_obj_get_height(s_zj_mas);
    lv_obj_set_pos(s_zj_mas, (int)s_zj_f[ZJ_N - 1] - w / 2, (int)(s_zj_g[ZJ_N - 1] - f * 50.0f) - h / 2);
    lv_obj_set_style_opa(s_zj_mas, (lv_opa_t)(f < 0.6f ? 255 : (1.0f - f) / 0.4f * 255.0f), 0);
}
static void zj_sumar(int n, float x, float y)
{
    s_zj_pts += n;
    if (s_zj_pts < 0) s_zj_pts = 0;
    if (n) zj_mas(n, x, y, n > 0 ? 0xFFF3B0 : 0xFF8A7A);
}
static bool zj_jugando(void) { return s_zj_fase == 1 && !s_zj_gano; }
static int zj_tiempo(void) { return (int)(s_zj_t - s_zj_juego_t); } /* ms desde que empezó a jugar */
static float zj_dif(void) /* qué tan difícil va: de 0 (empieza) a 1 (al final) */
{
    if (!s_zj_dura) return 0;
    float f = (float)zj_tiempo() / (float)s_zj_dura;
    return f < 0 ? 0 : (f > 1 ? 1 : f);
}

/* ======================================================================
 * 1) GALLETAS DE PEUMO (el horno): tres pedidos de los animales. Cada uno: la mezcla (tocar los ingredientes en el
 *    orden de la receta), el horno (el fuego baja solo: SOPLA al reloj, o toca el fuelle, para mantenerlo en la franja
 *    verde mientras se hornean) y sacarlas justo cuando están doradas.
 * ==================================================================== */
#define ZG_ING 5
#define ZG_PEDIDOS 3
#define ZG_BX 410   /* el termómetro del fuego */
#define ZG_BY0 150
#define ZG_BH 180
/* s_zj_o: [0..4] ingredientes, [5..8] la receta, [9] quién pide, [10] el resplandor, [11..17] llamas, [18] termómetro, [19] su relleno,
 *         [20] la franja verde, [21] el fuelle (botón), [22] la bandeja de galletas, [23] la barra del horneado, [24] su relleno,
 *         [25] la marca de "justo", [26..28] humo, [29] "¡SOPLA!" (texto), [30] ella */
static const art_icono_t ZG_ICO[ZG_ING] = {ICO_PINA, ICO_MIEL, ICO_SEMILLA, ICO_HUEVO, ICO_FRUTILLA};
static const char *const ZG_NOM[ZG_ING] = {"PIÑONES", "MIEL DE ULMO", "HARINA", "UN HUEVO", "FRUTILLAS"};
static const art_icono_t ZG_QUIEN[ZG_PEDIDOS] = {ICO_CHUCAO, ICO_ARDILLA, ICO_CIERVO_CAFE};
static int8_t s_zg_receta[4], s_zg_largo, s_zg_va;
static float s_zg_lo, s_zg_hi, s_zg_baja, s_zg_dorado;
static art_img_t s_zg_sopla_art;
static void zg_ing_pos(int i, float *x, float *y) { *x = CX + ((float)i - (ZG_ING - 1) / 2.0f) * 62.0f; *y = 344.0f + (i % 2 ? 6.0f : 0.0f); } /* sobre el mesón */
static void zg_fuego_ver(bool si)
{
    for (int i = 10; i <= 21; i++) if (s_zj_o[i]) ocultar(s_zj_o[i], !si);
    if (s_zj_o[29]) ocultar(s_zj_o[29], !si || s_nino);
}
static void zg_pedido(void) /* un pedido nuevo: la receta */
{
    s_zj_paso = 0; s_zg_va = 0;
    s_zg_largo = (int8_t)(s_nino ? 1 + (s_zj_ronda > 0) : 2 + s_zj_ronda);
    if (s_zg_largo > 4) s_zg_largo = 4;
    int usados = 0;
    for (int k = 0; k < s_zg_largo; k++) {
        int i;
        do { i = (int)(azar() % ZG_ING); } while (usados & (1 << i));
        usados |= 1 << i; s_zg_receta[k] = (int8_t)i;
    }
    for (int k = 0; k < 4; k++) {
        lv_obj_t *o = zj_img(5 + k, art_icono(ZG_ICO[s_zg_receta[k < s_zg_largo ? k : 0]], 3));
        ocultar(o, k >= s_zg_largo);
        lv_obj_set_style_image_opa(o, LV_OPA_COVER, 0);
        zj_centro(o, CX + 20.0f - (float)(s_zg_largo - 1) * 23.0f + (float)k * 46.0f, 118.0f);
    }
    { lv_obj_t *t = zj_caja(31, 70 + s_zg_largo * 46, 62, 0x2A1A12, 18); lv_obj_set_style_bg_opa(t, LV_OPA_70, 0); lv_obj_set_style_border_width(t, 2, 0); lv_obj_set_style_border_color(t, lv_color_hex(0xFFE9A0), 0);
      zj_centro(t, CX, 118.0f); lv_obj_move_to_index(t, 1); } /* el papelito del pedido */
    lv_obj_t *q = zj_img(9, art_icono(ZG_QUIEN[s_zj_ronda % ZG_PEDIDOS], 4));
    zj_centro(q, CX - 20.0f - (float)(s_zg_largo + 1) * 23.0f, 118.0f);
    for (int i = 0; i < ZG_ING; i++) { float x, y; zg_ing_pos(i, &x, &y); lv_obj_t *o = zj_img(i, art_icono(ZG_ICO[i], 4)); zj_centro(o, x, y); lv_obj_set_style_image_opa(o, LV_OPA_COVER, 0); }
    zg_fuego_ver(false);
    for (int k = 22; k <= 28; k++) if (s_zj_o[k]) ocultar(s_zj_o[k], true);
    /* el fuego: más difícil en cada pedido */
    s_zg_baja = s_nino ? 6.0f : 12.0f + 5.0f * (float)s_zj_ronda;
    s_zg_lo = s_nino ? 30.0f : 44.0f + 3.0f * (float)s_zj_ronda;
    s_zg_hi = s_nino ? 90.0f : 76.0f - 2.0f * (float)s_zj_ronda;
    s_zj_calor = 30.0f; s_zj_prog = 0; s_zj_quema = 0;
    static const char *const T[ZG_PEDIDOS] = {"EL CHUCAO QUIERE GALLETAS. TOCA LOS INGREDIENTES EN EL ORDEN DE LA RECETA",
                                              "¡AHORA EL MONITO DEL MONTE! OTRA RECETA", "¡EL HUEMUL TIENE HAMBRE! LA ÚLTIMA, MÁS LARGA"};
    zj_texto(s_suave ? "TOCA LO QUE BRILLA" : T[s_zj_ronda % ZG_PEDIDOS], s_zj_ronda ? 2600u : 0u);
    accion(UI_ACCION_SONAR, SND_HOLA_AMIGO);
}
static void zg_empezar(void)
{
    s_zj_ronda = 0;
    lv_obj_t *g = zj_caja(10, 220, 120, 0xFF8A2A, LV_RADIUS_CIRCLE); lv_obj_set_style_blend_mode(g, LV_BLEND_MODE_ADDITIVE, 0); lv_obj_set_style_bg_opa(g, LV_OPA_20, 0);
    for (int i = 0; i < 7; i++) {
        lv_obj_t *l = zj_caja(11 + i, 26, 30, 0xFF7A2A, 14);
        lv_obj_set_style_bg_grad_color(l, lv_color_hex(0xFFE27A), 0);
        lv_obj_set_style_bg_grad_dir(l, LV_GRAD_DIR_VER, 0);
        lv_obj_set_style_bg_main_stop(l, 40, 0);
        lv_obj_set_style_blend_mode(l, LV_BLEND_MODE_ADDITIVE, 0);
        s_zj_f[11 + i] = azar01() * 6.283f;
    }
    lv_obj_t *t = zj_caja(18, 24, ZG_BH + 8, 0x2A2238, 12); lv_obj_set_pos(t, ZG_BX - 12, ZG_BY0 - 4); lv_obj_set_style_bg_opa(t, LV_OPA_80, 0); lv_obj_set_style_border_width(t, 3, 0); lv_obj_set_style_border_color(t, lv_color_hex(0xFFF3C8), 0);
    zj_caja(20, 30, 40, 0x6AE07A, 6); lv_obj_set_style_bg_opa(s_zj_o[20], LV_OPA_50, 0);
    zj_caja(19, 14, 10, 0xFF9A3A, 7);
    lv_obj_t *b = zj_caja(21, 84, 84, 0x7A4A2A, LV_RADIUS_CIRCLE); /* el fuelle */
    lv_obj_set_style_border_width(b, 4, 0); lv_obj_set_style_border_color(b, lv_color_hex(0xFFE9A0), 0);
    lv_obj_center(icono(b, ICO_SOPLA, 4));
    lv_obj_set_pos(b, 50, 236);
    if (!s_nino) { art_texto(&s_zg_sopla_art, gtxt("¡SOPLA!"), 0xFFE9A0, 2, 0); lv_obj_t *s = zj_img(29, &s_zg_sopla_art.dsc); lv_obj_set_pos(s, 52, 206); }
    zg_pedido();
}
static void zg_soplo(float fuerza) /* soplido (micrófono) o fuelle: el fuego sube */
{
    if (s_zj_paso != 1 || s_zj_gano) return;
    s_zj_calor += fuerza;
    if (s_zj_calor > 100.0f) s_zj_calor = 100.0f;
    zj_chispas(200.0f + azar01() * 66.0f, 300.0f, 2, 0xFFC83A, 110.0f, -60.0f);
}
static void zg_toque(int x, int y)
{
    if (!zj_jugando()) return;
    if (s_zj_paso == 0) { /* la mezcla */
        int mejor = -1; float md = 44.0f;
        for (int i = 0; i < ZG_ING; i++) { float ix, iy; zg_ing_pos(i, &ix, &iy); const float d = hypotf(ix - (float)x, iy - (float)y); if (d < md) { md = d; mejor = i; } }
        if (s_zj_trampa) mejor = s_zg_receta[s_zg_va];
        if (mejor < 0) return;
        float ix, iy; zg_ing_pos(mejor, &ix, &iy);
        if (mejor == s_zg_receta[s_zg_va]) {
            zj_sumar(10, ix, iy - 40);
            accion(UI_ACCION_SONAR, (int)SND_CRISTAL_AZUL + s_zg_va);
            zj_chispas(ix, iy, 5, 0xFFF3B0, 100.0f, 120.0f);
            lv_obj_set_style_image_opa(s_zj_o[5 + s_zg_va], LV_OPA_40, 0);
            s_zg_va++;
            if (s_zg_va >= s_zg_largo) { /* ¡al horno! */
                s_zj_paso = 1; s_zj_salto_t = s_zj_t;
                for (int i = 0; i < ZG_ING; i++) ocultar(s_zj_o[i], true);
                zg_fuego_ver(true);
                zj_texto(s_suave ? "¡SOPLA EL FUEGO!" : (s_pez_mic_ok ? "¡AL HORNO! SOPLA AL RELOJ (O TOCA EL FUELLE) PARA QUE EL FUEGO QUEDE EN LO VERDE"
                                                                      : "¡AL HORNO! TOCA EL FUELLE PARA QUE EL FUEGO QUEDE EN LO VERDE"), 2600);
                lv_obj_t *bj = zj_img(22, art_icono(ICO_GALLETA, 4)); zj_centro(bj, CX, 226.0f);
                zj_tinte(bj, 0xF4E4C0, LV_OPA_60);
                lv_obj_t *bb = zj_caja(23, 160, 14, 0x2A2238, 7); lv_obj_set_pos(bb, CX - 80, 160); lv_obj_set_style_border_width(bb, 2, 0); lv_obj_set_style_border_color(bb, lv_color_hex(0xFFF3C8), 0);
                zj_caja(24, 4, 8, 0xFFD27A, 4); lv_obj_set_pos(s_zj_o[24], CX - 77, 163);
            }
        } else {
            if (!s_nino) zj_sumar(-4, ix, iy - 40); /* (los más chiquitos no pierden puntos) */
            accion(UI_ACCION_SONAR, SND_PLOP);
            s_zj_f[mejor] = (float)s_zj_t; /* tiembla */
            if (!s_nino) zj_texto(s_suave ? "¡ESA NO!" : "ESA NO VA AHORA: MIRA LA RECETA DE ARRIBA", 1500);
        }
        return;
    }
    if (s_zj_paso == 1) { /* el fuelle */
        if (hypotf((float)x - 92.0f, (float)y - 278.0f) < 56.0f || s_zj_trampa || s_nino) { zg_soplo(s_zj_trampa ? 30.0f : 9.0f); accion(UI_ACCION_SONAR, SND_TOQUE); lv_obj_set_style_transform_scale(s_zj_o[21], 230, 0); s_zj_f[21] = (float)s_zj_t; }
        return;
    }
    if (s_zj_paso == 2) { /* ¡sácalas! */
        const float d = fabsf(s_zg_dorado - 0.5f);
        int p; const char *t;
        if (s_zj_trampa || d < 0.08f) { p = 30; t = "¡PERFECTAS, DORADITAS!"; }
        else if (d < 0.18f) { p = 18; t = "¡RICAS!"; }
        else if (s_zg_dorado < 0.5f) { p = 6; t = "UN POCO CRUDAS..."; }
        else { p = 4; t = "¡UPS, SE TOSTARON!"; }
        if (s_nino) p = p < 18 ? 18 : p;
        zj_sumar(p, CX, 190);
        accion(UI_ACCION_SONAR, p >= 18 ? SND_ATRAPA : SND_PLOP);
        zj_chispas(CX, 226.0f, p >= 18 ? 10 : 4, p >= 18 ? 0xFFE27A : 0x8A6A4A, 160.0f, 160.0f);
        zj_texto(s_suave ? (p >= 18 ? "¡RICAS!" : "¡LISTAS!") : t, 1500);
        s_zj_paso = 3; s_zj_salto_t = s_zj_t;
    }
}
static void zg_animar(float dt)
{
    if (s_zj_trampa && zj_jugando() && s_zj_t - (uint32_t)s_zj_g[1] > 180u) { /* (pruebas) juega sola */
        s_zj_g[1] = (float)s_zj_t;
        if (s_zj_paso == 0) zg_toque(0, 0);
        else if (s_zj_paso == 1 && s_zj_calor < s_zg_lo + 8.0f) zg_soplo(12.0f);
    }
    for (int i = 0; i < ZG_ING; i++) if (zj_visible(i) && s_zj_f[i] > 0) { /* el ingrediente equivocado tiembla */
        const float f = ((float)s_zj_t - s_zj_f[i]) / 300.0f;
        float x, y; zg_ing_pos(i, &x, &y);
        if (f >= 1.0f) { s_zj_f[i] = 0; zj_centro(s_zj_o[i], x, y); } else zj_centro(s_zj_o[i], x + sinf(f * 30.0f) * 6.0f, y);
    }
    if (s_zj_paso == 0 && !s_zj_gano) { /* el que toca ahora brilla un poquito (en modo infante, mucho) */
        const int k = s_zg_receta[s_zg_va];
        for (int i = 0; i < ZG_ING; i++) if (s_zj_o[i]) lv_image_set_scale(s_zj_o[i], (uint32_t)(i == k && s_suave ? 256 + 40 * sinf((float)s_zj_t / 150.0f) : 256));
    }
    if (s_zj_paso == 1 && !s_zj_gano) { /* el horno */
        s_zj_calor -= s_zg_baja * dt;
        if (s_zj_calor < 4.0f) s_zj_calor = 4.0f;
        if (s_pez_mic_ok && s_pez_nivel > 28) zg_soplo((float)(s_pez_nivel - 20) * 1.6f * dt); /* soplando: sube mientras dure el soplido */
        const bool en = s_zj_calor >= s_zg_lo && s_zj_calor <= s_zg_hi;
        s_zj_prog += (en ? 22.0f : (s_zj_calor > s_zg_hi ? 10.0f : 4.0f)) * dt * (s_nino ? 1.6f : 1.0f);
        if (s_zj_calor > s_zg_hi + 6.0f) s_zj_quema += 20.0f * dt;
        if (en && ((s_zj_t / 500u) % 2 == 0) && (s_zj_t / 500u) != (uint32_t)s_zj_g[0]) { s_zj_g[0] = (float)(s_zj_t / 500u); zj_sumar(1, ZG_BX - 30.0f, (float)(ZG_BY0 - 14)); }
        if (s_zj_prog >= 100.0f) {
            s_zj_prog = 100.0f; s_zj_paso = 2; s_zj_salto_t = s_zj_t; s_zg_dorado = 0;
            zg_fuego_ver(false);
            ocultar(s_zj_o[23], true); ocultar(s_zj_o[24], true);
            if (s_zj_quema > 10.0f) zj_sumar(-(int)(s_zj_quema / 4.0f), CX, 180);
            zj_texto(s_suave ? "¡TOCA CUANDO ESTÉN DORADAS!" : "¡ATENTA! TOCA CUANDO LAS GALLETAS ESTÉN DORADAS (NI BLANCAS NI QUEMADAS)", 2200);
            lv_obj_t *m = zj_caja(25, 70, 10, 0xFFE27A, 5); lv_obj_set_style_bg_opa(m, LV_OPA_70, 0);
        }
        /* las llamas y el termómetro */
        const float h = s_zj_calor;
        for (int i = 0; i < 7; i++) {
            const float t = (float)s_zj_t / 120.0f + s_zj_f[11 + i];
            const float alto = 14.0f + h * (0.9f + 0.35f * sinf((float)i * 1.7f)) + sinf(t) * (4.0f + h * 0.08f);
            const int w = 22 + (int)(h * 0.12f) - abs(i - 3) * 2;
            lv_obj_set_size(s_zj_o[11 + i], w, (int)alto);
            lv_obj_set_pos(s_zj_o[11 + i], 166 + i * 20 - w / 2 + (int)(sinf(t * 0.7f) * 3.0f), 302 - (int)alto);
            lv_obj_set_style_bg_opa(s_zj_o[11 + i], (lv_opa_t)(150 + (int)h), 0);
        }
        lv_obj_set_size(s_zj_o[10], 160 + (int)(h * 1.2f), 70 + (int)(h * 0.8f)); zj_centro(s_zj_o[10], CX, 280.0f);
        lv_obj_set_style_bg_opa(s_zj_o[10], (lv_opa_t)(30 + (int)(h * 1.3f)), 0);
        const int bh = 10 + (int)(h / 100.0f * (float)(ZG_BH - 10));
        lv_obj_set_size(s_zj_o[19], 14, bh); lv_obj_set_pos(s_zj_o[19], ZG_BX - 7, ZG_BY0 + ZG_BH - bh);
        lv_obj_set_style_bg_color(s_zj_o[19], lv_color_hex(en ? 0x8AF07A : (h > s_zg_hi ? 0xFF5A3A : 0x7AB8FF)), 0);
        const int zy0 = ZG_BY0 + ZG_BH - (int)(s_zg_hi / 100.0f * ZG_BH), zy1 = ZG_BY0 + ZG_BH - (int)(s_zg_lo / 100.0f * ZG_BH);
        lv_obj_set_size(s_zj_o[20], 34, zy1 - zy0); lv_obj_set_pos(s_zj_o[20], ZG_BX - 17, zy0);
        if (s_zj_o[24]) lv_obj_set_width(s_zj_o[24], 4 + (int)(s_zj_prog * 1.5f));
        if (s_zj_f[21] > 0 && (float)s_zj_t - s_zj_f[21] > 120.0f) { lv_obj_set_style_transform_scale(s_zj_o[21], 256, 0); s_zj_f[21] = 0; }
        if (s_zj_o[22]) { /* la masa se va dorando (y quemando) */
            const float c = s_zj_prog / 100.0f;
            zj_tinte(s_zj_o[22], s_zj_quema > 12.0f ? 0x4A2A12 : 0xF4E4C0, (lv_opa_t)(s_zj_quema > 12.0f ? 120 : (int)(150.0f * (1.0f - c))));
        }
        if (s_zj_o[29]) lv_obj_set_style_opa(s_zj_o[29], (lv_opa_t)(h < s_zg_lo ? 160 + 95 * sinf((float)s_zj_t / 120.0f) : 90), 0);
    }
    if (s_zj_paso == 2 && !s_zj_gano) { /* el color va y vuelve: de cruda a quemada */
        const float per = s_nino ? 3400.0f : 2600.0f - 400.0f * (float)s_zj_ronda;
        s_zg_dorado = fmodf((float)(s_zj_t - s_zj_salto_t) / per, 1.0f);
        uint32_t c; lv_opa_t a;
        if (s_zg_dorado < 0.5f) { c = 0xF4E4C0; a = (lv_opa_t)((0.5f - s_zg_dorado) * 300.0f); }
        else { c = 0x3A1A0A; a = (lv_opa_t)((s_zg_dorado - 0.5f) * 300.0f); }
        zj_tinte(s_zj_o[22], c, a);
        zj_centro(s_zj_o[25], CX, 262.0f);
        lv_obj_set_style_bg_opa(s_zj_o[25], (lv_opa_t)(fabsf(s_zg_dorado - 0.5f) < 0.1f ? 230 : 60), 0);
        if (s_zj_trampa && s_zj_t - s_zj_salto_t > 300u) zg_toque(CX, 250);
    }
    if (s_zj_paso == 3) { /* la galleta vuela hasta quien la pidió */
        const float f = (float)(s_zj_t - s_zj_salto_t) / 700.0f, ff = f > 1 ? 1 : f;
        ocultar(s_zj_o[25], true);
        const float qx = s_zj_o[9] ? (float)lv_obj_get_x(s_zj_o[9]) + 24.0f : 106.0f;
        zj_centro(s_zj_o[22], CX + (qx - CX) * ff, 226.0f + (118.0f - 226.0f) * ff - sinf(ff * 3.1416f) * 50.0f);
        if (f >= 1.0f) {
            ocultar(s_zj_o[22], true);
            accion(UI_ACCION_SONAR, SND_COMER);
            zj_chispas(qx, 118.0f, 6, 0xFF8FA8, 90.0f, 80.0f);
            s_zj_ronda++;
            if (s_zj_ronda >= ZG_PEDIDOS) zj_terminar();
            else zg_pedido();
        }
    }
    (void)dt;
}

/* ======================================================================
 * 2) CANCIÓN DE CUNA (la camita): las notas bajan hasta el aro sobre la camita: toca cuando pasan por el aro (justo:
 *    más puntos; seguidas: más todavía). Las polillas vuelan hacia la vela: tócalas para que se vayan (si llegan, la
 *    apagan: tres velas). Al final ella se queda dormidita.
 * ==================================================================== */
#define ZK_NOTAS 8
#define ZK_AX 233.0f      /* el aro */
#define ZK_AY 262.0f
#define ZK_Y0 40.0f
#define ZK_POLI 3
/* s_zj_o: [0..7] notas, [8] el aro, [9] su brillo, [10..12] polillas, [13..15] velas (llamitas), [16] ella, [17] zetas, [18] combo */
static int s_zk_sale, s_zk_toc; /* notas que salieron / que se tocaron bien */
static float zk_ritmo(void) { return s_nino ? 1300.0f : 950.0f - 380.0f * zj_dif(); } /* ms entre notas */
static float zk_viaje(void) { return s_nino ? 2600.0f : 2000.0f - 500.0f * zj_dif(); } /* ms que tarda en bajar */
static void zk_empezar(void)
{
    static const art_icono_t NOTA[3] = {ICO_NOTA, ICO_ESTRELLA, ICO_LUNA};
    for (int i = 0; i < ZK_NOTAS; i++) { lv_obj_t *o = zj_img(i, art_icono(NOTA[i % 3], 3)); ocultar(o, true); s_zj_v[i] = 0; }
    lv_obj_t *a = zj_caja(8, 76, 76, 0, LV_RADIUS_CIRCLE); lv_obj_set_style_bg_opa(a, 0, 0); lv_obj_set_style_border_width(a, 5, 0); lv_obj_set_style_border_color(a, lv_color_hex(0xFFE9A0), 0); zj_centro(a, ZK_AX, ZK_AY);
    lv_obj_t *b = zj_caja(9, 90, 90, 0xFFF3B0, LV_RADIUS_CIRCLE); lv_obj_set_style_blend_mode(b, LV_BLEND_MODE_ADDITIVE, 0); lv_obj_set_style_bg_opa(b, 0, 0); zj_centro(b, ZK_AX, ZK_AY);
    for (int k = 0; k < ZK_POLI; k++) { lv_obj_t *o = zj_img(10 + k, art_icono(ICO_POLILLA, 3)); ocultar(o, true); s_zj_v[10 + k] = 0; }
    for (int k = 0; k < 3; k++) { lv_obj_t *l = zj_caja(13 + k, 12, 18, 0xFFC83A, 8); lv_obj_set_style_bg_grad_color(l, lv_color_hex(0xFFF3B0), 0); lv_obj_set_style_bg_grad_dir(l, LV_GRAD_DIR_VER, 0); lv_obj_set_pos(l, 370 + k * 18, 220); }
    lv_obj_t *e = zj_img(16, zj_ella(0, 0, 4)); zj_centro(e, 140.0f, 300.0f);
    s_zj_vidas = 3; s_zk_sale = s_zk_toc = 0; s_zj_combo = 0;
    s_zj_proxima = s_zj_t + 1200u; s_zj_prox2 = s_zj_t + 7000u;
    zj_texto(s_suave ? "TOCA CUANDO LA NOTA ESTÉ EN EL ARO" : "CANTA CON ELLA: TOCA CUANDO CADA NOTA PASE POR EL ARO. ¡Y ESPANTA LAS POLILLAS DE LA VELA!", 0);
}
static void zk_toque(int x, int y)
{
    if (!zj_jugando()) return;
    /* ¿una polilla? */
    for (int k = 0; k < ZK_POLI; k++) {
        if (!zj_visible(10 + k)) continue;
        if (hypotf(s_zj_x_[10 + k] - (float)x, s_zj_y_[10 + k] - (float)y) < 42.0f) {
            ocultar(s_zj_o[10 + k], true); s_zj_v[10 + k] = 0;
            zj_sumar(3, (float)x, (float)y - 30);
            accion(UI_ACCION_SONAR, SND_ALETEO);
            zj_chispas((float)x, (float)y, 4, 0xD8C8A8, 90.0f, 60.0f);
            return;
        }
    }
    /* la nota más cercana al aro */
    int mejor = -1; float md = 1e9f;
    for (int i = 0; i < ZK_NOTAS; i++) { if (!s_zj_v[i]) continue; const float d = fabsf(s_zj_y_[i] - ZK_AY); if (d < md) { md = d; mejor = i; } }
    const float justo = s_nino ? 34.0f : 18.0f, bien = s_nino ? 70.0f : 44.0f;
    if (mejor >= 0 && (md < bien || s_zj_trampa)) {
        const bool perfecta = md < justo || s_zj_trampa;
        s_zj_combo++;
        const int mult = s_zj_combo >= 10 ? 3 : (s_zj_combo >= 5 ? 2 : 1);
        zj_sumar((perfecta ? 10 : 5) * mult, ZK_AX, ZK_AY - 60);
        static const int SND[6] = {SND_CRISTAL_AZUL, SND_CRISTAL_VERDE, SND_CRISTAL_ROSA, SND_CRISTAL_ORO, SND_CRISTAL_LILA, SND_CRISTAL_BLANCO};
        accion(UI_ACCION_SONAR, SND[s_zk_toc % 6]);
        s_zk_toc++;
        s_zj_v[mejor] = 0; ocultar(s_zj_o[mejor], true);
        zj_chispas(ZK_AX, ZK_AY, perfecta ? 8 : 4, perfecta ? 0xFFE27A : 0xFFFFFF, 120.0f, 60.0f);
        s_zj_f[9] = (float)s_zj_t;
    } else {
        s_zj_combo = 0;
        accion(UI_ACCION_SONAR, SND_TOQUE);
    }
}
static void zk_animar(float dt)
{
    (void)dt;
    const bool juega = zj_jugando();
    if (juega && (int32_t)(s_zj_t - s_zj_proxima) >= 0) { /* sale una nota */
        for (int i = 0; i < ZK_NOTAS; i++) if (!s_zj_v[i]) { s_zj_v[i] = (float)s_zj_t; s_zj_x_[i] = ZK_AX + (azar01() - 0.5f) * 20.0f; ocultar(s_zj_o[i], false); s_zk_sale++; break; }
        const bool corchea = !s_nino && zj_dif() > 0.4f && azar() % 4 == 0; /* más adelante, a veces dos seguidas */
        s_zj_proxima = s_zj_t + (uint32_t)(corchea ? zk_ritmo() / 2 : zk_ritmo());
    }
    for (int i = 0; i < ZK_NOTAS; i++) {
        if (!s_zj_v[i]) continue;
        const float f = ((float)s_zj_t - s_zj_v[i]) / zk_viaje();
        const float y = ZK_Y0 + (ZK_AY - ZK_Y0) * f;
        s_zj_y_[i] = y;
        zj_centro(s_zj_o[i], s_zj_x_[i] + sinf(f * 7.0f + (float)i) * 18.0f * (1.0f - f), y);
        if (y > ZK_AY + (s_nino ? 70.0f : 48.0f)) { /* se pasó */
            s_zj_v[i] = 0; ocultar(s_zj_o[i], true);
            if (juega) { s_zj_combo = 0; if (s_zj_trampa) zk_toque((int)ZK_AX, (int)ZK_AY); }
        } else if (s_zj_trampa && fabsf(y - ZK_AY) < 10.0f) zk_toque((int)ZK_AX, (int)ZK_AY);
    }
    /* el aro brilla al acertar */
    if (s_zj_f[9] > 0) { const float f = ((float)s_zj_t - s_zj_f[9]) / 300.0f; if (f >= 1.0f) { s_zj_f[9] = 0; lv_obj_set_style_bg_opa(s_zj_o[9], 0, 0); } else lv_obj_set_style_bg_opa(s_zj_o[9], (lv_opa_t)((1.0f - f) * 140.0f), 0); }
    /* las polillas (no en modo infante) */
    if (juega && !s_nino && (int32_t)(s_zj_t - s_zj_prox2) >= 0) {
        for (int k = 0; k < ZK_POLI; k++) if (!s_zj_v[10 + k]) {
            s_zj_v[10 + k] = 1; s_zj_x_[10 + k] = azar() & 1 ? -20.0f : 486.0f; s_zj_y_[10 + k] = 120.0f + azar01() * 200.0f;
            s_zj_g[10 + k] = 40.0f + 30.0f * zj_dif() + azar01() * 20.0f;
            ocultar(s_zj_o[10 + k], false); break;
        }
        s_zj_prox2 = s_zj_t + 5200u - (uint32_t)(2600.0f * zj_dif()) + azar() % 1500u;
    }
    for (int k = 0; k < ZK_POLI; k++) {
        if (!s_zj_v[10 + k]) continue;
        const int vela = 3 - s_zj_vidas < 3 ? 3 - s_zj_vidas : 2;
        const float tx = 376.0f + (float)vela * 18.0f, ty = 216.0f;
        const float dx = tx - s_zj_x_[10 + k], dy = ty - s_zj_y_[10 + k], d = hypotf(dx, dy);
        s_zj_x_[10 + k] += dx / (d + 1) * s_zj_g[10 + k] * dt * 1.0f + sinf((float)s_zj_t / 140.0f + (float)k) * 1.4f;
        s_zj_y_[10 + k] += dy / (d + 1) * s_zj_g[10 + k] * dt + cosf((float)s_zj_t / 110.0f + (float)k) * 1.4f;
        zj_centro(s_zj_o[10 + k], s_zj_x_[10 + k], s_zj_y_[10 + k]);
        if (d < 14.0f) { /* ¡apagó una vela! */
            s_zj_v[10 + k] = 0; ocultar(s_zj_o[10 + k], true);
            if (s_zj_vidas > 0) { s_zj_vidas--; ocultar(s_zj_o[13 + (2 - s_zj_vidas)], true); }
            accion(UI_ACCION_SONAR, SND_SUSPIRO);
            zj_texto(s_suave ? "¡LA VELA!" : "¡UNA POLILLA APAGÓ UNA VELA! TÓCALAS ANTES", 1800);
            if (s_zj_vidas <= 0) zj_terminar();
        }
    }
    for (int k = 0; k < 3; k++) if (zj_visible(13 + k)) lv_obj_set_height(s_zj_o[13 + k], 16 + (int)(sinf((float)s_zj_t / 90.0f + (float)k) * 3.0f));
    if (s_zj_trampa && juega) for (int k = 0; k < ZK_POLI; k++) if (s_zj_v[10 + k]) zk_toque((int)s_zj_x_[10 + k], (int)s_zj_y_[10 + k]);
}

/* ======================================================================
 * 3) SALTO AL CIELO (la nube): ella rebota sola de nube en nube; tú la mueves hacia los lados (deja el dedo donde
 *    quieres que vaya). Nubes chiquitas: blancas, grises que se rompen, rosadas que te lanzan alto y otras que se
 *    mueven. Plumas: puntos. El cóndor: si lo tocas, te sube mucho. Si se cae, termina (en modo infante, abajo hay
 *    una nube grande que siempre la recibe y el juego dura un rato).
 * ==================================================================== */
#define ZS_NUBES 12
#define ZS_PLUMAS 4
#define ZS_G 1150.0f
#define ZS_SALTO 640.0f
enum { ZSN_NORMAL = 0, ZSN_FRAGIL, ZSN_SALTA, ZSN_MUEVE, ZSN_ROTA };
/* s_zj_o: [0..11] nubes, [12..15] plumas, [16] el cóndor, [17] ella, [18] el suelo de nubes (modo infante), [19] la flecha del dedo */
static float s_zs_alto, s_zs_tope, s_zs_cam, s_zs_cond_x, s_zs_cond_y;
static int s_zs_pl;
static float zs_pantalla_y(float wy) { return 360.0f - (wy - s_zs_cam); } /* mundo (sube) -> pantalla */
static void zs_nube(int i, float wy)
{
    const float d = (float)s_zs_tope / 4000.0f > 1.0f ? 1.0f : (float)s_zs_tope / 4000.0f;
    int tipo = ZSN_NORMAL;
    if (!s_nino) {
        const uint32_t r = azar() % 100u;
        if (r < 10u + (uint32_t)(22.0f * d)) tipo = ZSN_FRAGIL;
        else if (r < 22u + (uint32_t)(24.0f * d)) tipo = ZSN_MUEVE;
        else if (r < 32u + (uint32_t)(24.0f * d)) tipo = ZSN_SALTA;
    } else if (azar() % 6 == 0) tipo = ZSN_SALTA;
    s_zj_tipo[i] = (int8_t)tipo;
    s_zj_x_[i] = 96.0f + azar01() * 274.0f;
    s_zj_y_[i] = wy;
    s_zj_v[i] = tipo == ZSN_MUEVE ? (50.0f + 60.0f * d) * (azar() & 1 ? 1.0f : -1.0f) : 0.0f;
    s_zj_g[i] = 0;
    lv_obj_t *o = zj_img(i, art_icono(ICO_NUBE, s_nino ? 5 : 4));
    lv_image_set_rotation(o, 0);
    lv_obj_set_style_image_opa(o, LV_OPA_COVER, 0);
    zj_tinte(o, tipo == ZSN_FRAGIL ? 0x8A8A9A : (tipo == ZSN_SALTA ? 0xFF9AC8 : (tipo == ZSN_MUEVE ? 0x9AD0FF : 0xFFFFFF)), tipo == ZSN_NORMAL ? 0 : LV_OPA_50);
    if (!s_nino && azar() % 4 == 0) { /* una pluma encima */
        for (int k = 0; k < ZS_PLUMAS; k++) if (!zj_visible(12 + k)) { s_zj_x_[12 + k] = s_zj_x_[i]; s_zj_y_[12 + k] = wy + 46.0f; ocultar(s_zj_o[12 + k], false); break; }
    }
}
static float zs_paso(void) { const float d = s_zs_tope / 5000.0f > 1.0f ? 1.0f : s_zs_tope / 5000.0f; return s_nino ? 62.0f : 66.0f + 34.0f * d; }
static void zs_empezar(void)
{
    s_zs_cam = 0; s_zs_alto = 0; s_zs_tope = 0; s_zs_pl = 0;
    for (int k = 0; k < ZS_PLUMAS; k++) { zj_img(12 + k, art_icono(ICO_PLUMA, 3)); ocultar(s_zj_o[12 + k], true); }
    float wy = 20.0f;
    for (int i = 0; i < ZS_NUBES; i++) { zs_nube(i, wy); wy += zs_paso(); }
    s_zj_x_[0] = CX; s_zj_tipo[0] = ZSN_NORMAL; s_zj_v[0] = 0; zj_tinte(s_zj_o[0], 0xFFFFFF, 0); /* la primera, bajo ella */
    s_zs_tope = wy;
    zj_img(16, art_icono(ICO_CONDOR_B, 4)); ocultar(s_zj_o[16], true); s_zj_proxima = s_zj_t + 9000u;
    zj_img(17, zj_ella(0, 4, 3));
    if (s_nino) { lv_obj_t *s = zj_caja(18, 300, 26, 0xFFFFFF, 13); lv_obj_set_style_bg_opa(s, LV_OPA_80, 0); }
    { lv_obj_t *c = zj_caja(20, W, 130, 0xBFE3F7, 0); lv_obj_set_pos(c, 0, 336); lv_obj_set_style_bg_opa(c, 0, 0); lv_obj_move_to_index(c, 1); } /* el cielo que tapa el cerro de abajo a medida que suben */
    s_zj_px = CX; s_zj_py = 60.0f; s_zj_pvx = 0; s_zj_pvy = ZS_SALTO;
    s_zj_vidas = 1;
    zj_texto(s_suave ? "DEJA EL DEDO DONDE QUIERES QUE VAYA" : "¡SALTA DE NUBE EN NUBE! DEJA EL DEDO A UN LADO PARA QUE VAYA PARA ALLÁ. LAS GRISES SE ROMPEN; LAS ROSADAS TE LANZAN", 0);
}
static void zs_toque(int x, int y)
{
    if (!zj_jugando()) return;
    if (zj_visible(16) && hypotf(s_zs_cond_x - (float)x, s_zs_cond_y - (float)y) < 50.0f) { /* el cóndor la sube */
        s_zj_pvy = 1500.0f; ocultar(s_zj_o[16], true);
        zj_sumar(25, s_zs_cond_x, s_zs_cond_y);
        accion(UI_ACCION_SONAR, SND_CONDOR);
        zj_chispas(s_zs_cond_x, s_zs_cond_y, 8, 0xFFFFFF, 140.0f, 100.0f);
    }
}
static void zs_animar(float dt)
{
    if (!zj_jugando()) {
        if (zj_visible(17)) zj_centro(s_zj_o[17], s_zj_px, zs_pantalla_y(s_zj_py) - 22.0f);
        return;
    }
    /* hacia los lados: va hacia el dedo (en pruebas: hacia la próxima nube) */
    float tx = s_zj_dedo ? s_zj_dedo_x : s_zj_px;
    if (s_zj_trampa) { float mejor = 1e9f; for (int i = 0; i < ZS_NUBES; i++) { if (s_zj_tipo[i] == ZSN_ROTA || s_zj_tipo[i] == ZSN_FRAGIL) continue; const float dy = s_zj_y_[i] - s_zj_py; if (dy > -10.0f && dy < mejor) { mejor = dy; tx = s_zj_x_[i]; } } }
    const float vmax = s_nino ? 230.0f : 320.0f;
    float vx = (tx - s_zj_px) * 5.0f;
    if (vx > vmax) vx = vmax;
    if (vx < -vmax) vx = -vmax;
    s_zj_pvx += (vx - s_zj_pvx) * (dt * 10.0f > 1 ? 1 : dt * 10.0f);
    s_zj_px += s_zj_pvx * dt;
    if (s_zj_px < 70.0f) s_zj_px = 70.0f;
    if (s_zj_px > 396.0f) s_zj_px = 396.0f;
    const float antes = s_zj_py;
    s_zj_pvy -= ZS_G * dt * (s_nino ? 0.8f : 1.0f);
    s_zj_py += s_zj_pvy * dt;
    /* las nubes que se mueven */
    for (int i = 0; i < ZS_NUBES; i++) {
        if (s_zj_tipo[i] == ZSN_MUEVE) {
            s_zj_x_[i] += s_zj_v[i] * dt;
            if (s_zj_x_[i] < 90.0f) { s_zj_x_[i] = 90.0f; s_zj_v[i] = fabsf(s_zj_v[i]); }
            if (s_zj_x_[i] > 376.0f) { s_zj_x_[i] = 376.0f; s_zj_v[i] = -fabsf(s_zj_v[i]); }
        } else if (s_zj_tipo[i] == ZSN_ROTA) { s_zj_g[i] += 900.0f * dt; s_zj_y_[i] -= s_zj_g[i] * dt; }
    }
    /* ¿cae sobre una nube? */
    if (s_zj_pvy < 0) {
        const float ancho = s_nino ? 48.0f : 38.0f;
        for (int i = 0; i < ZS_NUBES; i++) {
            if (s_zj_tipo[i] == ZSN_ROTA) continue;
            const float top = s_zj_y_[i] + 10.0f;
            if (antes >= top && s_zj_py <= top && fabsf(s_zj_px - s_zj_x_[i]) < ancho) {
                s_zj_py = top;
                if (s_zj_tipo[i] == ZSN_SALTA) { s_zj_pvy = ZS_SALTO * 1.55f; accion(UI_ACCION_SONAR, SND_GLOBO); zj_chispas(s_zj_px, zs_pantalla_y(top), 6, 0xFF9AC8, 120.0f, 120.0f); }
                else { s_zj_pvy = ZS_SALTO; accion(UI_ACCION_SONAR, SND_REBOTE); }
                if (s_zj_tipo[i] == ZSN_FRAGIL) { s_zj_tipo[i] = ZSN_ROTA; s_zj_g[i] = 0; lv_obj_set_style_image_opa(s_zj_o[i], LV_OPA_50, 0); lv_image_set_rotation(s_zj_o[i], 150); accion(UI_ACCION_SONAR, SND_PLOP); }
                break;
            }
        }
        if (s_nino && s_zj_py < s_zs_cam - 10.0f) { s_zj_py = s_zs_cam - 10.0f; s_zj_pvy = ZS_SALTO * 1.1f; accion(UI_ACCION_SONAR, SND_REBOTE); } /* la nube grande de abajo */
    }
    /* la cámara sube con ella (nunca baja) */
    if (s_zj_py - s_zs_cam > 170.0f) s_zs_cam = s_zj_py - 170.0f;
    if (s_zj_py > s_zs_alto) {
        const int antes_pts = (int)(s_zs_alto / 12.0f);
        s_zs_alto = s_zj_py;
        const int ahora_pts = (int)(s_zs_alto / 12.0f);
        if (ahora_pts > antes_pts) s_zj_pts += ahora_pts - antes_pts;
    }
    /* nubes que quedaron abajo: arriba de nuevo */
    for (int i = 0; i < ZS_NUBES; i++) if (s_zj_y_[i] < s_zs_cam - 140.0f) { zs_nube(i, s_zs_tope); s_zs_tope += zs_paso(); }
    for (int i = 0; i < ZS_NUBES; i++) zj_centro(s_zj_o[i], s_zj_x_[i], zs_pantalla_y(s_zj_y_[i]));
    if (s_zj_o[18]) zj_centro(s_zj_o[18], CX, zs_pantalla_y(s_zs_cam - 24.0f));
    if (s_zj_o[20]) { const int a = (int)(s_zs_cam * 0.8f); lv_obj_set_style_bg_opa(s_zj_o[20], (lv_opa_t)(a > 255 ? 255 : a), 0); lv_obj_set_style_bg_grad_color(s_zj_o[20], lv_color_hex(0xD8EEFA), 0); lv_obj_set_style_bg_grad_dir(s_zj_o[20], LV_GRAD_DIR_VER, 0); }
    /* plumas */
    for (int k = 0; k < ZS_PLUMAS; k++) {
        if (!zj_visible(12 + k)) continue;
        if (s_zj_y_[12 + k] < s_zs_cam - 140.0f) { ocultar(s_zj_o[12 + k], true); continue; }
        zj_centro(s_zj_o[12 + k], s_zj_x_[12 + k], zs_pantalla_y(s_zj_y_[12 + k]) + sinf((float)s_zj_t / 300.0f + (float)k) * 4.0f);
        if (hypotf(s_zj_x_[12 + k] - s_zj_px, s_zj_y_[12 + k] - (s_zj_py + 20.0f)) < 34.0f) {
            ocultar(s_zj_o[12 + k], true); s_zs_pl++;
            zj_sumar(10, s_zj_x_[12 + k], zs_pantalla_y(s_zj_y_[12 + k]));
            accion(UI_ACCION_SONAR, SND_ATRAPA);
        }
    }
    /* el cóndor cruza de vez en cuando */
    if (!s_nino && (int32_t)(s_zj_t - s_zj_proxima) >= 0 && !zj_visible(16)) {
        ocultar(s_zj_o[16], false); s_zj_f[16] = (float)s_zj_t; s_zj_g[16] = azar() & 1 ? 1.0f : -1.0f;
        s_zj_proxima = s_zj_t + 14000u + azar() % 6000u;
        accion(UI_ACCION_SONAR, SND_CONDOR);
    }
    if (zj_visible(16)) {
        const float f = ((float)s_zj_t - s_zj_f[16]) / 5000.0f;
        if (f >= 1.0f) ocultar(s_zj_o[16], true);
        else {
            s_zs_cond_x = s_zj_g[16] > 0 ? -40.0f + f * 546.0f : 506.0f - f * 546.0f;
            s_zs_cond_y = 170.0f + sinf(f * 9.0f) * 30.0f;
            zj_centro(s_zj_o[16], s_zs_cond_x, s_zs_cond_y);
            if (s_zj_trampa) zs_toque((int)s_zs_cond_x, (int)s_zs_cond_y);
        }
    }
    /* ella */
    zj_centro(s_zj_o[17], s_zj_px, zs_pantalla_y(s_zj_py) - 22.0f);
    lv_obj_move_foreground(s_zj_o[17]);
    /* ¿se cayó? */
    if (zs_pantalla_y(s_zj_py) > 500.0f) { accion(UI_ACCION_SONAR, SND_CAIDA); zj_terminar(); }
    if (s_nino && zj_tiempo() > 40000) zj_terminar();
    if (s_zj_trampa && s_zs_alto > 1800.0f) zj_terminar();
}

/* ======================================================================
 * 4) ATRAPA HOJAS (la corona): ella con su canasta abajo; la mueves con el dedo. Caen hojas verdes (1), secas (1) y
 *    doradas (5); las piñas pinchan (pierdes un corazón). Seguidas sin perder: más puntos. A veces el chucao viene a
 *    robar: tócalo para que se vaya. 45 segundos.
 * ==================================================================== */
#define ZA_HOJAS 8
#define ZA_Y 352.0f
enum { ZAH_VERDE = 0, ZAH_SECA, ZAH_DORADA, ZAH_PINA };
/* s_zj_o: [0..7] lo que cae, [8] ella, [9] la canasta, [10] el chucao, [11..13] corazones, [14..16] rayas de viento */
static float s_za_viento;
static void za_hoja(int i)
{
    const float d = zj_dif();
    int t = ZAH_VERDE;
    const uint32_t r = azar() % 100u;
    if (r < 8u) t = ZAH_DORADA;
    else if (!s_nino && r < 14u + (uint32_t)(14.0f * d)) t = ZAH_PINA;
    else if (r < 50u) t = ZAH_SECA;
    s_zj_tipo[i] = (int8_t)t;
    s_zj_x_[i] = 90.0f + azar01() * 286.0f;
    s_zj_y_[i] = -20.0f - azar01() * 160.0f;
    s_zj_v[i] = (s_nino ? 70.0f : 95.0f + 90.0f * d) * (t == ZAH_DORADA ? 1.35f : 1.0f) * (0.85f + azar01() * 0.3f);
    s_zj_f[i] = azar01() * 6.283f;
    lv_obj_t *o = zj_img(i, art_icono(t == ZAH_PINA ? ICO_PINA : (t == ZAH_SECA ? ICO_HOJA_SECA : ICO_HOJA), 3));
    zj_tinte(o, t == ZAH_DORADA ? 0xFFD23A : 0xFFFFFF, t == ZAH_DORADA ? LV_OPA_70 : 0);
}
static void za_empezar(void)
{
    for (int i = 0; i < ZA_HOJAS; i++) { za_hoja(i); if (i >= (s_nino ? 3 : 4)) { ocultar(s_zj_o[i], true); s_zj_v[i] = 0; } }
    zj_img(8, zj_ella(0, 4, 4));
    zj_img(9, art_icono(ICO_CANASTA, 4));
    zj_img(10, art_icono(ICO_CHUCAO, 3)); ocultar(s_zj_o[10], true);
    s_zj_vidas = s_nino ? 0 : 3;
    for (int k = 0; k < 3; k++) { lv_obj_t *c = zj_img(11 + k, art_icono(ICO_CORAZON, 2)); lv_obj_set_pos(c, 160 + k * 26, 64); ocultar(c, s_nino); }
    for (int k = 0; k < 3; k++) { lv_obj_t *r = zj_caja(14 + k, 90, 4, 0xFFFFFF, 2); lv_obj_set_style_bg_opa(r, LV_OPA_60, 0); ocultar(r, true); }
    s_zj_px = CX; s_zj_combo = 0; s_za_viento = 0;
    s_zj_proxima = s_zj_t + 9000u; s_zj_prox2 = s_zj_t + 7000u;
    zj_texto(s_suave ? "MUEVE LA CANASTA CON EL DEDO" : "¡ATRAPA LAS HOJAS CON LA CANASTA! MUÉVELA CON EL DEDO. LAS DORADAS VALEN MÁS; LAS PIÑAS PINCHAN", 0);
}
static void za_toque(int x, int y)
{
    if (!zj_jugando()) return;
    if (zj_visible(10) && hypotf(s_zj_x_[10] - (float)x, s_zj_y_[10] - (float)y) < 46.0f) { /* el chucao se va */
        ocultar(s_zj_o[10], true); s_zj_v[10] = 0;
        accion(UI_ACCION_SONAR, SND_CHUCAO);
        zj_sumar(3, (float)x, (float)y - 30);
        zj_chispas((float)x, (float)y, 4, 0xC86A3A, 90.0f, 100.0f);
    }
}
static void za_animar(float dt)
{
    const bool juega = zj_jugando();
    float tx = s_zj_dedo ? s_zj_dedo_x : s_zj_px;
    if (s_zj_trampa) { float mejor = -1e9f; for (int i = 0; i < ZA_HOJAS; i++) if (s_zj_v[i] && s_zj_tipo[i] != ZAH_PINA && s_zj_y_[i] > mejor && s_zj_y_[i] < ZA_Y) { mejor = s_zj_y_[i]; tx = s_zj_x_[i]; } }
    if (juega) {
        float vx = (tx - s_zj_px) * 9.0f;
        const float vmax = 520.0f;
        if (vx > vmax) vx = vmax;
        if (vx < -vmax) vx = -vmax;
        s_zj_px += vx * dt;
        if (s_zj_px < 80.0f) s_zj_px = 80.0f;
        if (s_zj_px > 386.0f) s_zj_px = 386.0f;
    }
    zj_centro(s_zj_o[8], s_zj_px, ZA_Y + 10.0f);
    zj_centro(s_zj_o[9], s_zj_px, ZA_Y - 10.0f);
    lv_obj_move_foreground(s_zj_o[9]);
    /* el viento: ráfagas que empujan las hojas a un lado */
    if (juega && !s_nino && (int32_t)(s_zj_t - s_zj_prox2) >= 0) { s_zj_prox2 = s_zj_t + 7000u + azar() % 4000u; s_zj_rafaga_t = s_zj_t; s_za_viento = azar() & 1 ? 120.0f : -120.0f; accion(UI_ACCION_SONAR, SND_ALETEO); }
    const bool rafaga = s_zj_rafaga_t && s_zj_t - s_zj_rafaga_t < 1600u;
    for (int k = 0; k < 3; k++) {
        ocultar(s_zj_o[14 + k], !rafaga);
        if (rafaga) { const float f = (float)((s_zj_t - s_zj_rafaga_t + (uint32_t)k * 400u) % 900u) / 900.0f; lv_obj_set_pos(s_zj_o[14 + k], s_za_viento > 0 ? (int)(-80.0f + f * 560.0f) : (int)(480.0f - f * 560.0f), 140 + k * 70); }
    }
    /* cuántas hojas caen a la vez: más con el tiempo */
    const int activas = s_nino ? 3 : 4 + (int)(4.0f * zj_dif());
    int n = 0;
    for (int i = 0; i < ZA_HOJAS; i++) if (s_zj_v[i]) n++;
    for (int i = 0; i < ZA_HOJAS && n < activas && juega; i++) if (!s_zj_v[i]) { za_hoja(i); ocultar(s_zj_o[i], false); n++; }
    for (int i = 0; i < ZA_HOJAS; i++) {
        if (!s_zj_v[i]) continue;
        s_zj_y_[i] += s_zj_v[i] * dt;
        s_zj_x_[i] += (sinf(s_zj_f[i] + (float)s_zj_t / 380.0f) * 40.0f + (rafaga ? s_za_viento : 0.0f)) * dt;
        if (s_zj_x_[i] < 70.0f) s_zj_x_[i] = 70.0f;
        if (s_zj_x_[i] > 396.0f) s_zj_x_[i] = 396.0f;
        zj_centro(s_zj_o[i], s_zj_x_[i], s_zj_y_[i]);
        if (s_zj_tipo[i] != ZAH_PINA) lv_image_set_rotation(s_zj_o[i], (int32_t)(sinf(s_zj_f[i] + (float)s_zj_t / 260.0f) * 400.0f));
        /* ¿en la canasta? */
        if (juega && s_zj_y_[i] > ZA_Y - 30.0f && s_zj_y_[i] < ZA_Y + 4.0f && fabsf(s_zj_x_[i] - s_zj_px) < (s_nino ? 52.0f : 40.0f)) {
            s_zj_v[i] = 0; ocultar(s_zj_o[i], true);
            if (s_zj_tipo[i] == ZAH_PINA) {
                s_zj_combo = 0;
                if (s_zj_vidas > 0) { s_zj_vidas--; ocultar(s_zj_o[11 + s_zj_vidas], true); }
                accion(UI_ACCION_SONAR, SND_QUEJA);
                zj_chispas(s_zj_px, ZA_Y - 30.0f, 5, 0x8A5A2A, 120.0f, 200.0f);
                if (s_zj_vidas <= 0 && !s_nino) { zj_terminar(); return; }
            } else {
                s_zj_combo++;
                const int mult = s_zj_combo >= 12 ? 3 : (s_zj_combo >= 6 ? 2 : 1);
                const int p = (s_zj_tipo[i] == ZAH_DORADA ? 5 : 1) * mult;
                zj_sumar(p, s_zj_px, ZA_Y - 60.0f);
                accion(UI_ACCION_SONAR, s_zj_tipo[i] == ZAH_DORADA ? SND_TESORO : SND_ATRAPA);
                if (s_zj_tipo[i] == ZAH_DORADA) zj_chispas(s_zj_px, ZA_Y - 30.0f, 6, 0xFFE27A, 120.0f, 120.0f);
            }
            continue;
        }
        if (s_zj_y_[i] > 440.0f) { s_zj_v[i] = 0; ocultar(s_zj_o[i], true); if (juega && s_zj_tipo[i] != ZAH_PINA) s_zj_combo = 0; }
    }
    /* el chucao ladrón (no en modo infante) */
    if (juega && !s_nino && (int32_t)(s_zj_t - s_zj_proxima) >= 0 && !zj_visible(10)) {
        ocultar(s_zj_o[10], false); s_zj_v[10] = 1;
        s_zj_x_[10] = s_zj_px < CX ? 430.0f : 36.0f; s_zj_y_[10] = ZA_Y + 4.0f;
        s_zj_proxima = s_zj_t + 11000u + azar() % 5000u;
        accion(UI_ACCION_SONAR, SND_CHUCAO);
    }
    if (zj_visible(10)) {
        const float dx = s_zj_px - s_zj_x_[10];
        s_zj_x_[10] += (dx > 0 ? 1.0f : -1.0f) * 70.0f * dt;
        zj_centro(s_zj_o[10], s_zj_x_[10], s_zj_y_[10] - (float)(((s_zj_t / 120u) % 4u) == 0u ? 6 : 0));
        if (s_zj_trampa) za_toque((int)s_zj_x_[10], (int)s_zj_y_[10]);
        else if (fabsf(dx) < 24.0f) { /* ¡robó! */
            ocultar(s_zj_o[10], true); s_zj_v[10] = 0;
            zj_sumar(-5, s_zj_px, ZA_Y - 60.0f);
            accion(UI_ACCION_SONAR, SND_CHUCAO);
            zj_texto(s_suave ? "¡EL CHUCAO!" : "¡EL CHUCAO SE LLEVÓ HOJAS! TÓCALO CUANDO VENGA", 1800);
        }
    }
    if (juega && zj_tiempo() >= (int)s_zj_dura) zj_terminar();
}

/* ======================================================================
 * 5) LA PIEDRA TIBIA: piedritas en el suelo; una está tibia (sale del día: cada día es distinta). Tocas una y te dice
 *    si está fría, tibia o caliente (por cuán cerca está). Tres rondas; la lagartija, a veces, corre y la tibia cambia de
 *    lugar. Menos toques, más estrellas.
 * ==================================================================== */
#define ZP_MAX 16
#define ZP_RONDAS 3
/* s_zj_o: [0..15] piedras, [16..31] su marca de color, [32] la lagartija, [33] el brillo tibio, [34] ella sentada */
static int s_zp_n, s_zp_cols, s_zp_tibia, s_zp_toques, s_zp_toq_ronda, s_zp_semilla;
static uint8_t s_zp_tocada[ZP_MAX];
static void zp_pos(int k, float *x, float *y)
{
    const int filas = (s_zp_n + s_zp_cols - 1) / s_zp_cols;
    const int f = k / s_zp_cols, c = k % s_zp_cols;
    const float sx = s_zp_cols == 4 ? 78.0f : 96.0f, sy = 70.0f;
    *x = CX + ((float)c - (float)(s_zp_cols - 1) / 2.0f) * sx + (f % 2 ? 10.0f : -10.0f);
    *y = 250.0f + ((float)f - (float)(filas - 1) / 2.0f) * sy;
}
static int zp_azar(void) { s_zp_semilla = s_zp_semilla * 1103515245 + 12345; return (s_zp_semilla >> 16) & 0x7FFF; }
static void zp_ronda(void)
{
    s_zp_toq_ronda = 0;
    memset(s_zp_tocada, 0, sizeof s_zp_tocada);
    s_zp_tibia = zp_azar() % s_zp_n;
    for (int k = 0; k < s_zp_n; k++) { lv_obj_set_style_bg_color(s_zj_o[k], lv_color_hex(0xB4B2C2), 0); lv_obj_set_style_bg_grad_color(s_zj_o[k], lv_color_hex(0x7E7C90), 0); }
    ocultar(s_zj_o[33], true);
    if (s_zj_o[34]) ocultar(s_zj_o[34], true);
    s_zj_paso = 0;
    static const char *const T[ZP_RONDAS] = {"UNA DE ESTAS PIEDRITAS ESTÁ TIBIA. TOCA UNA: TE DIGO SI ESTÁ FRÍA O CALIENTE", "¡OTRA VEZ! AHORA LA TIBIA ESTÁ EN OTRA", "LA ÚLTIMA... ¡OJO CON LA LAGARTIJA!"};
    zj_texto(s_suave ? "¿CUÁL ESTÁ TIBIA?" : T[s_zj_ronda % ZP_RONDAS], s_zj_ronda ? 2200u : 0u);
}
static void zp_empezar(void)
{
    s_zp_n = s_nino ? 6 : 12; s_zp_cols = s_nino ? 3 : 4;
    s_zp_semilla = s_pet ? pet_place_game_seed(s_pet, PET_JL_PIEDRA, s_ultimo_t) : 7; /* cambia cada día */
    s_zp_semilla += s_zj_toques * 7919; /* (cada partida del mismo día, otra disposición) */
    for (int k = 0; k < s_zp_n; k++) {
        float x, y; zp_pos(k, &x, &y);
        const int pw = s_nino ? 70 : 56, ph = s_nino ? 50 : 40;
        lv_obj_t *o = zj_caja(k, pw, ph, 0xB4B2C2, LV_RADIUS_CIRCLE); /* una piedrita lisa del cerro */
        lv_obj_set_style_bg_grad_color(o, lv_color_hex(0x7E7C90), 0); lv_obj_set_style_bg_grad_dir(o, LV_GRAD_DIR_VER, 0);
        lv_obj_set_style_border_width(o, 3, 0); lv_obj_set_style_border_color(o, lv_color_hex(0x4A4858), 0);
        lv_obj_set_style_shadow_width(o, 8, 0); lv_obj_set_style_shadow_ofs_y(o, 4, 0); lv_obj_set_style_shadow_opa(o, LV_OPA_40, 0);
        zj_centro(o, x, y);
        lv_obj_t *m = zj_caja(16 + k, pw / 3, ph / 4, 0xFFFFFF, LV_RADIUS_CIRCLE); lv_obj_set_style_bg_opa(m, LV_OPA_70, 0); zj_centro(m, x - pw / 6, y - ph / 4); /* su brillo */
    }
    zj_img(32, art_icono(ICO_LAGARTIJA, 4)); ocultar(s_zj_o[32], true);
    lv_obj_t *b = zj_caja(33, 80, 56, 0xFFB050, LV_RADIUS_CIRCLE); lv_obj_set_style_blend_mode(b, LV_BLEND_MODE_ADDITIVE, 0); ocultar(b, true);
    s_zj_ronda = 0; s_zp_toques = 0;
    zp_ronda();
}
static int zp_dist(int a, int b) { const int fa = a / s_zp_cols, ca = a % s_zp_cols, fb = b / s_zp_cols, cb = b % s_zp_cols; return abs(fa - fb) + abs(ca - cb); }
static void zp_toque(int x, int y)
{
    if (!zj_jugando() || s_zj_paso) return;
    int k = -1; float md = 40.0f;
    for (int i = 0; i < s_zp_n; i++) { float px, py; zp_pos(i, &px, &py); const float d = hypotf(px - (float)x, py - (float)y); if (d < md) { md = d; k = i; } }
    if (s_zj_trampa) k = s_zp_tibia;
    if (k < 0) return;
    float px, py; zp_pos(k, &px, &py);
    s_zp_toques++; s_zp_toq_ronda++;
    s_zp_tocada[k] = 1;
    const int d = zp_dist(k, s_zp_tibia);
    if (d == 0) { /* ¡aquí! */
        const int p = s_zp_toq_ronda <= 1 ? 40 : (s_zp_toq_ronda <= 2 ? 30 : (s_zp_toq_ronda <= 4 ? 20 : 10));
        zj_sumar(p, px, py - 60.0f);
        accion(UI_ACCION_SONAR, SND_SUSPIRO_AMOR);
        zj_centro(s_zj_o[33], px, py + 4.0f); ocultar(s_zj_o[33], false);
        lv_obj_set_style_bg_color(s_zj_o[k], lv_color_hex(0xFF8A3A), 0); lv_obj_set_style_bg_grad_color(s_zj_o[k], lv_color_hex(0xD8501E), 0);
        lv_obj_t *e = zj_img(34, zj_ella(0, 2, 4)); zj_centro(e, px, py - 34.0f); lv_obj_move_foreground(e);
        zj_chispas(px, py, 10, 0xFFB050, 140.0f, 60.0f);
        s_zj_paso = 1; s_zj_salto_t = s_zj_t;
        return;
    }
    static const uint32_t COL[4] = {0xFF7A3A, 0xFFC85A, 0xA8E0D0, 0x7AB8FF};
    static const char *const T[4] = {"", "¡CALIENTE!", "TIBIO...", "FRÍA"};
    const int q = d == 1 ? 1 : (d == 2 ? 2 : 3);
    lv_obj_set_style_bg_color(s_zj_o[k], lv_color_hex(COL[q]), 0); /* la piedrita toma el color: fría (azul) a caliente (naranja) */
    lv_obj_set_style_bg_grad_color(s_zj_o[k], lv_color_hex(q == 1 ? 0xC8501E : (q == 2 ? 0x6AA898 : 0x3A6AB8)), 0);
    accion(UI_ACCION_SONAR, q == 1 ? SND_HADAS : SND_TOQUE);
    if (!s_nino) zj_texto(T[q], 900);
    /* la lagartija: en la ronda 2 y 3, a los 3 toques, corre y la tibia se mueve a una vecina */
    if (!s_nino && s_zj_ronda >= 1 && s_zp_toq_ronda == 3) {
        int nueva = s_zp_tibia;
        for (int tries = 0; tries < 12; tries++) { const int c = zp_azar() % s_zp_n; if (c != s_zp_tibia && zp_dist(c, s_zp_tibia) <= 2 && !s_zp_tocada[c]) { nueva = c; break; } }
        float ax, ay, bx, by; zp_pos(s_zp_tibia, &ax, &ay); zp_pos(nueva, &bx, &by);
        s_zj_x_[32] = ax; s_zj_y_[32] = ay; s_zj_f[32] = bx; s_zj_g[32] = by; s_zj_v[32] = (float)s_zj_t;
        ocultar(s_zj_o[32], false);
        s_zp_tibia = nueva;
        for (int i = 0; i < s_zp_n; i++) { lv_obj_set_style_bg_color(s_zj_o[i], lv_color_hex(0xB4B2C2), 0); lv_obj_set_style_bg_grad_color(s_zj_o[i], lv_color_hex(0x7E7C90), 0); s_zp_tocada[i] = 0; }
        accion(UI_ACCION_SONAR, SND_DESLIZA);
        zj_texto(s_suave ? "¡LA LAGARTIJA!" : "¡LA LAGARTIJA CORRIÓ Y SE LLEVÓ LO TIBIO! MIRA HACIA DÓNDE FUE", 2200);
    }
}
static void zp_animar(float dt)
{
    (void)dt;
    if (s_zj_trampa && zj_jugando() && !s_zj_paso && s_zj_t - (uint32_t)s_zj_g[1] > 250u) { s_zj_g[1] = (float)s_zj_t; zp_toque(0, 0); } /* (pruebas) */
    if (zj_visible(32)) { /* la lagartija corre de la tibia vieja a la nueva y se va */
        const float f = ((float)s_zj_t - s_zj_v[32]) / 900.0f;
        if (f >= 1.4f) ocultar(s_zj_o[32], true);
        else { const float ff = f > 1 ? 1 : f; zj_centro(s_zj_o[32], s_zj_x_[32] + (s_zj_f[32] - s_zj_x_[32]) * ff, s_zj_y_[32] + (s_zj_g[32] - s_zj_y_[32]) * ff - sinf(ff * 3.1416f) * 20.0f + (f > 1 ? (f - 1) * 200.0f : 0)); lv_obj_move_foreground(s_zj_o[32]); }
    }
    if (zj_visible(33)) { const float a = (float)s_zj_t / 300.0f; lv_obj_set_style_bg_opa(s_zj_o[33], (lv_opa_t)(110 + 40 * sinf(a)), 0); }
    if (s_zj_paso == 1 && s_zj_t - s_zj_salto_t > 1500u && !s_zj_gano) {
        s_zj_ronda++;
        if (s_zj_ronda >= (s_nino ? 2 : ZP_RONDAS)) zj_terminar(); else zp_ronda();
    }
}

static void zj_otra_cb(lv_event_t *e);
static void zj_fin_x_cb(lv_event_t *e);
#include "ui_dos.inc" /* (1.0.17) los juegos de a dos con una visita */

/* ---------- el final: puntaje, récord, estrellas y premios ---------- */
static const int16_t ZJ_ESTRELLAS[6][3] = { /* puntos para 1, 2 y 3 estrellas (adultos) */
    {0, 0, 0}, {120, 200, 255}, {60, 160, 260}, {60, 140, 240}, {25, 55, 90}, {40, 80, 105}};
static int zj_estrellas_de(int jg, int pts)
{
    int e = 0;
    for (int k = 0; k < 3; k++) {
        int u = ZJ_ESTRELLAS[jg % 6][k];
        if (s_suave) u = u * 6 / 10;
        if (pts >= u) e = k + 1;
    }
    if (s_nino && e < 1) e = 1; /* los más chiquitos siempre ganan algo */
    return e;
}
static void zj_fin_x_cb(lv_event_t *e) { (void)e; zj_cerrar(true); }
static void zj_terminar(void)
{
    if (s_zj_fase != 1) return;
    s_zj_fase = 2;
    s_zj_gano = true;
    s_zj_fin_t = s_zj_t;
    if (s_zj_trampa && s_zj_pts < ZJ_ESTRELLAS[s_zj_jg % 6][2]) s_zj_pts = ZJ_ESTRELLAS[s_zj_jg % 6][2] + 5;
    const int jm = zj_motor(s_zj_jg);
    const int rec = s_pet ? pet_place_game_record(s_pet, jm) : 0, est_antes = s_pet ? pet_place_game_stars(s_pet, jm) : 0;
    s_zj_estrellas = zj_estrellas_de(s_zj_jg, s_zj_pts);
    /* lo que va a ganar (el motor hace lo mismo: las dos primeras rondas con estrellas de cada juego, cada día) */
    int premios_hoy = 0;
    if (s_pet) {
        const int64_t local = (int64_t)s_ultimo_t + s_pet->zona_horaria;
        const uint8_t hoy = (uint8_t)(1u + ((uint32_t)(local >= 0 ? local / 86400 : (local - 86399) / 86400) & 0x7Fu));
        premios_hoy = s_pet->jl_dia == hoy ? s_pet->jl_premios[jm] : 0;
    }
    const bool premio = s_zj_estrellas >= 1 && premios_hoy < PET_JL_PREMIOS_DIA;
    const int ram = premio ? (s_zj_estrellas >= 3 ? 2 : 1) : 0, mus = premio && s_zj_estrellas >= 2 ? 1 : 0;
    accion(UI_ACCION_JUEGO_LUGAR, (jm & 7) | ((s_zj_estrellas & 3) << 3) | (s_zj_pts << 5));
    { int l, c; zj_lugar_cosa(s_zj_jg, &l, &c); if (l >= 0 && s_pet && !pet_secret_found(s_pet, l, c)) accion(UI_ACCION_LUGAR, l * 16 + c); } /* la primera vez: el secreto */
    accion(UI_ACCION_SONAR, s_zj_estrellas >= 2 ? SND_TESORO : SND_ALEGRE);
    for (int k = 0; k < 3; k++) zj_chispas(CX + (float)(k - 1) * 90.0f, 170.0f, 6, 0, 240.0f, 380.0f);
    ocultar(s_zj_txt, true); ocultar(s_zj_hud, true);
    if (s_zj_barra) ocultar(s_zj_barra, true);
    /* la tarjeta */
    s_zj_fin = caja(s_zj);
    lv_obj_set_size(s_zj_fin, 320, 250);
    lv_obj_align(s_zj_fin, LV_ALIGN_CENTER, 0, -10);
    lv_obj_set_style_bg_color(s_zj_fin, lv_color_hex(0x1A1430), 0);
    lv_obj_set_style_bg_opa(s_zj_fin, LV_OPA_90, 0);
    lv_obj_set_style_radius(s_zj_fin, 26, 0);
    lv_obj_set_style_border_width(s_zj_fin, 3, 0);
    lv_obj_set_style_border_color(s_zj_fin, lv_color_hex(0xFFE9A0), 0);
    char b[64];
    if (!s_nino) {
        snprintf(b, sizeof b, "%d PUNTOS", s_zj_pts);
        art_texto(&s_zj_fin_art[0], b, 0xFFE27A, 3, 0);
        lv_obj_t *t = lv_image_create(s_zj_fin); poner_src(t, &s_zj_fin_art[0].dsc); lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 16);
        if (rec > 0 && s_zj_pts > rec) snprintf(b, sizeof b, "%s", gtxt("¡NUEVO RÉCORD!"));
        else snprintf(b, sizeof b, "%s %d", gtxt("RÉCORD:"), rec > s_zj_pts ? rec : s_zj_pts);
        art_texto(&s_zj_fin_art[1], b, rec > 0 && s_zj_pts > rec ? 0xFF9AC8 : 0xCFE8FF, 2, 0);
        lv_obj_t *r = lv_image_create(s_zj_fin); poner_src(r, &s_zj_fin_art[1].dsc); lv_obj_align(r, LV_ALIGN_TOP_MID, 0, 56);
    }
    for (int k = 0; k < 3; k++) { /* las estrellas (las ganadas, doradas; aparecen una por una) */
        lv_obj_t *s = icono(s_zj_fin, ICO_ESTRELLA, 5);
        lv_obj_align(s, LV_ALIGN_TOP_MID, (k - 1) * 70, s_nino ? 40 : 84);
        if (k >= s_zj_estrellas) { lv_obj_set_style_image_recolor(s, lv_color_hex(0x3A3450), 0); lv_obj_set_style_image_recolor_opa(s, LV_OPA_80, 0); }
        else { lv_obj_set_style_opa(s, LV_OPA_TRANSP, 0); s_zj_f[40 + k] = 1; }
        s_zj_o[40 + k] = s;
    }
    if (s_zj_estrellas > est_antes && est_antes > 0 && !s_nino) { art_texto(&s_zj_fin_art[2], gtxt("¡MÁS ESTRELLAS QUE NUNCA!"), 0xFFE27A, 2, 0); lv_obj_t *t = lv_image_create(s_zj_fin); poner_src(t, &s_zj_fin_art[2].dsc); lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 146); }
    if (ram || mus) { /* los premios */
        int n = 0;
        for (int k = 0; k < ram; k++, n++) { lv_obj_t *p = icono(s_zj_fin, ICO_RAMITA, 3); lv_obj_align(p, LV_ALIGN_BOTTOM_MID, (n - (ram + mus - 1) / 2.0f) * 44, -18); }
        for (int k = 0; k < mus; k++, n++) { lv_obj_t *p = icono(s_zj_fin, ICO_MUSGO, 3); lv_obj_align(p, LV_ALIGN_BOTTOM_MID, (n - (ram + mus - 1) / 2.0f) * 44, -18); }
    }
    /* abajo: OTRA VEZ (a la izquierda) y la X */
    lv_obj_t *o = boton(s_zj, BOTON_ABAJO, BOTON_ABAJO, zj_otra_cb, 0);
    lv_obj_set_ext_click_area(o, 14);
    lv_obj_center(icono(o, ICO_VUELTA, 2));
    lv_obj_set_pos(o, CX - BOTON_ABAJO / 2 - VOLVER_DX / 2, BOTON_ABAJO_Y);
    s_zj_o[44] = o;
    ocultar(s_zj_x, true); /* la X del final: sale contando lo que se ganó */
    s_zj_x = boton(s_zj, BOTON_ABAJO, BOTON_ABAJO, zj_fin_x_cb, 0);
    lv_obj_set_ext_click_area(s_zj_x, 14);
    lv_obj_center(icono(s_zj_x, ICO_NO, 2));
    lv_obj_set_pos(s_zj_x, CX - BOTON_ABAJO / 2 + VOLVER_DX / 2, BOTON_ABAJO_Y);
}
static void zj_fin_animar(void)
{
    if (s_zj_fase != 2 || !s_zj_fin) return;
    for (int k = 0; k < 3; k++) {
        if (!s_zj_o[40 + k] || !s_zj_f[40 + k]) continue;
        const int32_t d = (int32_t)(s_zj_t - s_zj_fin_t) - 500 - k * 450;
        if (d < 0) continue;
        if (d < 300) { lv_obj_set_style_opa(s_zj_o[40 + k], (lv_opa_t)(d * 255 / 300), 0); lv_image_set_scale(s_zj_o[40 + k], (uint32_t)(512 - d * 256 / 300)); if (d < 40) accion(UI_ACCION_SONAR, (int)SND_CRISTAL_AZUL + k * 2); }
        else { lv_obj_set_style_opa(s_zj_o[40 + k], LV_OPA_COVER, 0); lv_image_set_scale(s_zj_o[40 + k], 256); s_zj_f[40 + k] = 0; }
    }
    if (s_zj_t - s_zj_fin_t > 30000u) zj_cerrar(true); /* (nadie la mira: se cierra sola) */
}

/* ---------- la pantalla del juego: abrir, el reloj, tocar, cerrar ---------- */
static void zj_empezar_juego(void);
static void zj_toque_cb(lv_event_t *e)
{
    const lv_event_code_t code = lv_event_get_code(e);
    lv_point_t pt = {0, 0};
    lv_indev_t *in = lv_indev_active();
    if (in) lv_indev_get_point(in, &pt);
    if (code == LV_EVENT_PRESSING) { s_zj_dedo = true; s_zj_dedo_x = (float)pt.x; s_zj_dedo_y = (float)pt.y; return; }
    if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) { s_zj_dedo = false; return; }
    s_zj_toques++;
    s_zj_dedo = true; s_zj_dedo_x = (float)pt.x; s_zj_dedo_y = (float)pt.y;
    if (s_zj_fase == 0) { zj_empezar_juego(); return; }
    if (s_zj_txt && !lv_obj_has_flag(s_zj_txt, LV_OBJ_FLAG_HIDDEN) && pt.y < 150) { ocultar(s_zj_txt, true); return; } /* tocar las instrucciones las cierra */
    switch (s_zj_jg) {
    case ZJ_NUBE: zs_toque(pt.x, pt.y); break;
    case ZJ_CORONA: za_toque(pt.x, pt.y); break;
    case ZJ_PIEDRA: zp_toque(pt.x, pt.y); break;
    case ZJ_HORNO: zg_toque(pt.x, pt.y); break;
    case ZJ_CAMITA: zk_toque(pt.x, pt.y); break;
    default: if (zd_es(s_zj_jg)) zd_toque(pt.x, pt.y); break;
    }
}
static void zj_x_cb(lv_event_t *e) { (void)e; zj_cerrar(false); }
static void zj_timer_cb(lv_timer_t *t)
{
    (void)t;
    if (!s_zj_jg) return;
    const uint32_t ahora = lv_tick_get();
    uint32_t d = s_zj_ult ? ahora - s_zj_ult : ZJ_MS;
    s_zj_ult = ahora;
    if (d > 120u) d = 120u; /* (tras una pausa larga no salta de golpe) */
    s_zj_t += d;
    const float dt = (float)d / 1000.0f;
    if (s_zj_txt && !lv_obj_has_flag(s_zj_txt, LV_OBJ_FLAG_HIDDEN) && (int32_t)(s_zj_t - s_zj_txt_hasta) > 0) ocultar(s_zj_txt, true);
    if (s_zj_fase == 0) { if (s_zj_t > (s_zj_trampa ? 200u : 4500u)) zj_empezar_juego(); }
    else {
        switch (s_zj_jg) {
        case ZJ_NUBE: zs_animar(dt); break;
        case ZJ_CORONA: za_animar(dt); break;
        case ZJ_PIEDRA: zp_animar(dt); break;
        case ZJ_HORNO: zg_animar(dt); break;
        case ZJ_CAMITA: zk_animar(dt); break;
        default: if (zd_es(s_zj_jg)) zd_animar(dt); break;
        }
        if (s_zj_fase == 1 && s_zj_dura && zj_tiempo() >= (int)s_zj_dura && s_zj_jg != ZJ_NUBE && s_zj_jg != ZJ_PIEDRA && s_zj_jg != ZJ_HORNO) zj_terminar();
        if (s_zj_fase == 1 && s_zj_jg == ZJ_HORNO && zj_tiempo() > 120000) zj_terminar(); /* (por si nadie juega) */
        if (!zd_es(s_zj_jg)) zj_hud();
    }
    zj_chispas_animar(dt);
    zj_mas_animar();
    zj_fin_animar();
}
static void zj_crear(void)
{
    if (s_zj) return;
    s_zj = caja(lv_layer_top());
    lv_obj_set_size(s_zj, W, W);
    lv_obj_set_style_bg_color(s_zj, lv_color_hex(0x10161A), 0);
    lv_obj_set_style_bg_opa(s_zj, LV_OPA_COVER, 0);
    lv_obj_add_flag(s_zj, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(s_zj, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_event_cb(s_zj, zj_toque_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(s_zj, zj_toque_cb, LV_EVENT_PRESSING, NULL);
    lv_obj_add_event_cb(s_zj, zj_toque_cb, LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(s_zj, zj_toque_cb, LV_EVENT_PRESS_LOST, NULL);
    ocultar(s_zj, true);
}
static void zj_piezas_encima(void)
{
    s_zj_tit = cartel_de(s_zj, &s_zj_tit_img, 0x1A1430, LV_OPA_70);
    lv_obj_set_size(s_zj_tit, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_hor(s_zj_tit, 14, 0); lv_obj_set_style_pad_ver(s_zj_tit, 6, 0);
    lv_obj_set_style_border_width(s_zj_tit, 2, 0); lv_obj_set_style_border_color(s_zj_tit, lv_color_hex(0xFFE9A0), 0); lv_obj_set_style_border_opa(s_zj_tit, LV_OPA_60, 0);
    s_zj_txt = caja(s_zj);
    lv_obj_set_size(s_zj_txt, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(s_zj_txt, lv_color_hex(0x1A1430), 0);
    lv_obj_set_style_bg_opa(s_zj_txt, LV_OPA_80, 0);
    lv_obj_set_style_pad_hor(s_zj_txt, 12, 0); lv_obj_set_style_pad_ver(s_zj_txt, 8, 0);
    lv_obj_set_style_radius(s_zj_txt, 14, 0);
    s_zj_txt_img = lv_image_create(s_zj_txt); lv_obj_remove_flag(s_zj_txt_img, LV_OBJ_FLAG_CLICKABLE);
    ocultar(s_zj_txt, true);
    s_zj_hud = cartel_de(s_zj, &s_zj_hud_img, 0x1A1430, LV_OPA_60);
    lv_obj_set_size(s_zj_hud, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_hor(s_zj_hud, 12, 0); lv_obj_set_style_pad_ver(s_zj_hud, 3, 0);
    ocultar(s_zj_hud, true);
    s_zj_mas = cartel_de(s_zj, &s_zj_mas_img, 0x000000, LV_OPA_TRANSP);
    lv_obj_set_size(s_zj_mas, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    ocultar(s_zj_mas, true);
    s_zj_x = boton(s_zj, BOTON_ABAJO, BOTON_ABAJO, zj_x_cb, 0);
    lv_obj_set_ext_click_area(s_zj_x, 14);
    lv_obj_center(icono(s_zj_x, ICO_NO, 2));
    lv_obj_set_pos(s_zj_x, CX - BOTON_ABAJO / 2, BOTON_ABAJO_Y);
}
static const char *zj_nombre(int jg)
{
    switch (jg) {
    case ZJ_HORNO: return "GALLETAS DE PEUMO";
    case ZJ_CAMITA: return "CANCIÓN DE CUNA";
    case ZJ_NUBE: return "SALTO AL CIELO";
    case ZJ_CORONA: return "ATRAPA HOJAS";
    case ZJ_PIEDRA: return "LA PIEDRA TIBIA";
    default: return "";
    }
}
/* la portada: el nombre, el récord y las estrellas que tiene; toca para empezar (o empieza sola) */
static void zj_portada(void)
{
    zj_titulo(zj_nombre(s_zj_jg));
    const int jm = zj_motor(s_zj_jg);
    const int rec = s_pet ? pet_place_game_record(s_pet, jm) : 0, est = s_pet ? pet_place_game_stars(s_pet, jm) : 0;
    lv_obj_t *c = caja(s_zj);
    lv_obj_set_size(c, 260, 150);
    lv_obj_align(c, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(c, lv_color_hex(0x1A1430), 0);
    lv_obj_set_style_bg_opa(c, LV_OPA_80, 0);
    lv_obj_set_style_radius(c, 24, 0);
    for (int k = 0; k < 3; k++) {
        lv_obj_t *s = icono(c, ICO_ESTRELLA, 4);
        lv_obj_align(s, LV_ALIGN_TOP_MID, (k - 1) * 56, 16);
        if (k >= est) { lv_obj_set_style_image_recolor(s, lv_color_hex(0x3A3450), 0); lv_obj_set_style_image_recolor_opa(s, LV_OPA_80, 0); }
    }
    if (!s_nino) {
        char b[48];
        if (rec > 0) snprintf(b, sizeof b, "%s %d", gtxt("RÉCORD:"), rec); else snprintf(b, sizeof b, "%s", gtxt("¡A JUGAR!"));
        lv_obj_t *t = texto_c(c, b, 0xCFE8FF, 2, 240); lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 72);
        lv_obj_t *t2 = texto_c(c, "TOCA PARA EMPEZAR", 0xFFE9A0, 2, 240); lv_obj_align(t2, LV_ALIGN_BOTTOM_MID, 0, -14);
    }
    s_zj_o[45] = c;
}
static void zj_empezar_juego(void)
{
    if (s_zj_fase != 0) return;
    if (s_zj_o[45]) { lv_obj_delete(s_zj_o[45]); s_zj_o[45] = NULL; }
    s_zj_fase = 1; s_zj_juego_t = s_zj_t; s_zj_pts = 0; s_zj_pts_vis = -1;
    switch (s_zj_jg) {
    case ZJ_NUBE: s_zj_dura = 0; zs_empezar(); break;
    case ZJ_CORONA: s_zj_dura = s_nino ? 35000u : 45000u; za_empezar(); break;
    case ZJ_PIEDRA: s_zj_dura = 0; zp_empezar(); break;
    case ZJ_HORNO: s_zj_dura = 0; zg_empezar(); break;
    case ZJ_CAMITA: s_zj_dura = s_nino ? 30000u : 42000u; zk_empezar(); break;
    default: s_zj_dura = 0; if (zd_es(s_zj_jg)) zd_empezar(); break;
    }
    if (s_zj_dura) { /* la barrita del tiempo */
        s_zj_barra = caja(s_zj); lv_obj_set_size(s_zj_barra, 160, 12); lv_obj_align(s_zj_barra, LV_ALIGN_TOP_MID, 0, 74);
        lv_obj_set_style_bg_color(s_zj_barra, lv_color_hex(0x1A1430), 0); lv_obj_set_style_bg_opa(s_zj_barra, LV_OPA_70, 0); lv_obj_set_style_radius(s_zj_barra, 6, 0);
        s_zj_barra_f = caja(s_zj_barra); lv_obj_set_size(s_zj_barra_f, 156, 8); lv_obj_set_pos(s_zj_barra_f, 2, 2); lv_obj_set_style_radius(s_zj_barra_f, 4, 0); lv_obj_set_style_bg_opa(s_zj_barra_f, LV_OPA_COVER, 0);
    }
    ocultar(s_zj_tit, true);
    lv_obj_move_foreground(s_zj_txt); lv_obj_move_foreground(s_zj_hud); lv_obj_move_foreground(s_zj_mas); lv_obj_move_foreground(s_zj_x);
    if (!zd_es(s_zj_jg)) zj_hud();
    accion(UI_ACCION_SONAR, SND_SILBATO);
}
static void zj_mundo_ver(bool si); /* (ui_zonas.inc: el mundo o la casita se apartan mientras se juega) */
static void zj_abrir(int jg)
{
    if (!s_pet || jg <= ZJ_NADA || jg > ZJ_BUCEO) return;
    zj_crear();
    if (s_zj_jg) zj_cerrar(false);
    static const int8_t PANT[6] = {-1, 11, 12, 8, 9, 10};
    s_zj_jg = jg;
    s_zj_gano = false; s_zj_t = 1; s_zj_t0 = 0; s_zj_ult = 0; s_zj_paso = 0; s_zj_cuenta = 0; s_zj_fase = 0; s_zj_pts = 0; s_zj_ronda = 0; s_zj_combo = 0;
    s_zj_fin = NULL; s_zj_barra = s_zj_barra_f = NULL; s_zj_dedo = false; s_zj_dura = 0; s_zj_vidas = 0;
    memset(s_zj_chv, 0, sizeof s_zj_chv); memset(s_zj_f, 0, sizeof s_zj_f); memset(s_zj_g, 0, sizeof s_zj_g); memset(s_zj_v, 0, sizeof s_zj_v); memset(s_zj_tipo, 0, sizeof s_zj_tipo);
    zj_mundo_ver(false); /* el fondo del mundo o de la casita se suelta: hace lugar al del juego */
    s_zj_fondo = lv_image_create(s_zj); lv_obj_remove_flag(s_zj_fondo, LV_OBJ_FLAG_CLICKABLE);
    if (zd_es(jg)) { /* (1.0.17) los de a dos: su fondo viene de la memoria de recursos */
        lv_obj_set_style_bg_color(s_zj, lv_color_hex(0x10161A), 0);
        s_zj_pant = ART_PANT_DOS + (jg - ZJ_PING);
        zd_fondo();
        zj_piezas_encima();
        zd_portada();
        ocultar(s_zj, false);
        lv_obj_move_foreground(s_zj);
        if (!s_zj_timer) s_zj_timer = lv_timer_create(zj_timer_cb, ZJ_MS, NULL);
        accion(UI_ACCION_SONAR, zi_es(jg) ? SND_SALPICON : SND_HOLA_AMIGO);
        return;
    }
    s_zj_pant = PANT[jg];
    int px = 0, py = 0;
    const bool afuera = jg == ZJ_NUBE || jg == ZJ_CORONA || jg == ZJ_PIEDRA;
    const lv_image_dsc_t *f = afuera ? art_pantalla_clima(s_zj_pant, (int)s_fondo_periodo < 0 ? 0 : (int)s_fondo_periodo, 0, 0, &px, &py) : art_pantalla(s_zj_pant, &px, &py);
    poner_src(s_zj_fondo, f);
    lv_obj_set_pos(s_zj_fondo, px, py);
    zj_piezas_encima();
    zj_portada();
    ocultar(s_zj, false);
    lv_obj_move_foreground(s_zj);
    if (!s_zj_timer) s_zj_timer = lv_timer_create(zj_timer_cb, ZJ_MS, NULL);
    accion(UI_ACCION_SONAR, SND_HADAS);
}
static void zj_otra_cb(lv_event_t *e) { (void)e; const int jg = s_zj_jg; s_zj_sin_fondo = true; zj_cerrar(false); s_zj_sin_fondo = false; zj_abrir(jg); }
static void zj_cerrar(bool gano)
{
    (void)gano;
    if (!s_zj_jg) return;
    s_zj_jg = ZJ_NADA;
    s_zj_fase = 0;
    if (s_zj_timer) { lv_timer_delete(s_zj_timer); s_zj_timer = NULL; }
    lv_obj_clean(s_zj); /* (todo lo del juego: se vuelve a armar la próxima vez) */
    memset(s_zj_o, 0, sizeof s_zj_o); memset(s_zj_ch, 0, sizeof s_zj_ch); memset(s_zj_chv, 0, sizeof s_zj_chv);
    s_zj_fondo = s_zj_tit = s_zj_tit_img = s_zj_txt = s_zj_txt_img = s_zj_x = s_zj_hud = s_zj_hud_img = s_zj_barra = s_zj_barra_f = s_zj_fin = s_zj_mas = s_zj_mas_img = NULL;
    art_img_free(&s_zj_tit_art); art_img_free(&s_zj_txt_art); art_img_free(&s_zj_hud_art); art_img_free(&s_zj_mas_art); art_img_free(&s_zg_sopla_art);
    for (int k = 0; k < 3; k++) { art_img_free(&s_zj_pet_art[k]); art_img_free(&s_zj_fin_art[k]); }
    zd_liberar();
    ocultar(s_zj, true);
    if (s_zj_pant >= 0) art_pantalla_liberar(s_zj_pant);
    s_zj_pant = -1;
    s_zj_dedo = false;
    if (s_zj_sin_fondo) return;
    zj_mundo_ver(true);
}
static bool zj_activo(void) { return s_zj_jg != ZJ_NADA; }
static bool zj_quiere_mic(void) { return s_zj_jg == ZJ_HORNO && s_zj_fase == 1 && s_zj_paso == 1; }
static void zj_soplido(void) { if (zj_quiere_mic()) { zg_soplo(18.0f); accion(UI_ACCION_SONAR, SND_TOQUE); } } /* (un soplido de golpe: el aviso de main) */
/* (pruebas) */
void ui_zj_trampa(int si) { s_zj_trampa = si; }
/* (pruebas desde la placa) un toque en (x, y) del juego abierto, sin el táctil */
void ui_zj_tocar(int x, int y)
{
    if (!s_zj_jg) return;
    if (y >= BOTON_ABAJO_Y - 6 && s_zj_fase == 2) { if (x < CX) zj_otra_cb(NULL); else zj_cerrar(true); return; }
    if (y >= BOTON_ABAJO_Y - 6 && abs(x - CX) < 30) { zj_cerrar(false); return; }
    s_zj_toques++;
    if (s_zj_fase == 0) { zj_empezar_juego(); return; }
    s_zj_dedo_x = (float)x; s_zj_dedo_y = (float)y;
    switch (s_zj_jg) {
    case ZJ_NUBE: zs_toque(x, y); break;
    case ZJ_CORONA: za_toque(x, y); break;
    case ZJ_PIEDRA: zp_toque(x, y); break;
    case ZJ_HORNO: zg_toque(x, y); break;
    case ZJ_CAMITA: zk_toque(x, y); break;
    default: if (zd_es(s_zj_jg)) zd_toque(x, y); break;
    }
}
/* jg * 100 + 50 si terminó (con estrellas) + puntos (hasta 49) */
int ui_zj_estado(void) { return s_zj_jg * 100 + (s_zj_gano ? 50 : 0) + (s_zj_pts > 49 ? 49 : s_zj_pts); }
int ui_zj_dbg(int q)
{
    switch (q) {
    case 0: return (int)s_zj_t;
    case 1: return (int)s_zj_fin_t;
    case 2: return s_zj_timer ? 1 : 0;
    case 3: return s_zj_paso;
    case 4: return s_zj_fase;
    case 5: return s_zj_pts;
    case 6: return s_zj_estrellas;
    case 7: return s_zj_vidas;
    case 8: return s_zj_ronda;
    case 9: return s_zj_toques;
    default: return 0;
    }
}
/* (pruebas) 1.0.17: abre un juego de a dos (0..3) aunque no haya visita; el estado: fase * 1000 + ella * 10 + la visita */
void ui_zd_abrir(int n) { if (n >= 0 && n < 6) zj_abrir(ZJ_PING + n); } /* (4 y 5: el faro y el buceo) */
int ui_zd_estado(void) { return zd_es(s_zj_jg) ? s_zj_fase * 1000 + (s_zj_pts > 99 ? 99 : s_zj_pts) * 10 + (s_zd_b > 9 ? 9 : s_zd_b) : -1; }
