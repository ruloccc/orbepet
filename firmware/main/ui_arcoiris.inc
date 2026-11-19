/* ======================================================================
 * (1.0.31) EL TOBOGÁN DEL ARCOÍRIS: solo cuando hay un arcoíris en el cielo (se abre tocándolo, o en Jugar).
 * Ella se desliza por el arcoíris, que baja desde una nube lejana hacia la pantalla (en perspectiva). Deslizas el dedo a
 * los lados en cualquier parte y ella se mueve igual (no va debajo del dedo). Junta estrellitas (y a veces un prisma, que
 * vale 3); esquiva las nubes de tormenta y los pájaros (3 corazones; un corazón que baja te devuelve uno). Mientras más
 * avanza, más rápido. Dura un minuto: al final llega a la punta del arcoíris. En modo infante no hay peligros.
 * Fondo: pj6 (herramientas/hacer_fondos_juegos.py arcoiris → hacer_fondos_dos.py). Si falta, un cielo pintado sencillo.
 * Usa el marco de ui_zjuegos.inc y las piezas de ui_dos.inc (zd_poner, zd_criatura, zd_mano). Incluido desde ui_dos.inc.
 * Al terminar: accion(UI_ACCION_JUEGO_ISLA, PET_ISLA_ARCOIRIS | puntos << 4) (pet_island_game: diversión y el diario).
 * ==================================================================== */
#define ZR_HY 64.0f      /* el arcoíris nace en esta fila (ARCO_HY en hacer_fondos_juegos.py) */
#define ZR_HW0 34.0f     /* medio ancho en ZR_HY ... */
#define ZR_HWK 0.24f     /* ... y cuánto crece por fila (ARCO_HW0, ARCO_HWK) */
#define ZR_PY 378.0f     /* los pies de ella */
#define ZR_COSAS 12      /* s_zj_o[2..13] */
#define ZR_RAYAS 8       /* s_zj_o[14..21]: rayitas de luz que pasan (la velocidad) */
#define ZR_DURA 60000u
enum { ZRC_NADA = -1, ZRC_ESTRELLA = 0, ZRC_PRISMA, ZRC_CORAZON, ZRC_NUBE, ZRC_PAJARO };
static float s_zr_u, s_zr_vel, s_zr_avance;
static float s_zr_cu[ZR_COSAS], s_zr_cy[ZR_COSAS], s_zr_cdu[ZR_COSAS];
static int8_t s_zr_cq[ZR_COSAS], s_zr_cesc[ZR_COSAS];
static float s_zr_ru[ZR_RAYAS], s_zr_ry[ZR_RAYAS];
static int s_zr_vidas, s_zr_vis = -1, s_zr_pose;
static uint32_t s_zr_prox, s_zr_golpe, s_zr_feliz, s_zr_empieza;
static bool s_zr_tenia, s_zr_solto, s_zr_cayo;
static float s_zr_dx0, s_zr_u0;
static float zr_hw(float y) { return ZR_HW0 + (y - ZR_HY) * ZR_HWK; }
static float zr_x(float u, float y) { return (float)CX + u * zr_hw(y); }
static float zr_prof(float y) { const float f = (y - ZR_HY) / (ZR_PY - ZR_HY); return 0.30f + 0.70f * (f < 0 ? 0 : f); } /* lejos va lento; cerca, rápido */
static bool zr_hay_arcoiris(void) { return s_pet && s_pet->raro == RARO_ARCOIRIS && !s_interior && s_escena != ESC_CUEVA; }

static void zr_marcador(void)
{
    if (!s_zj_hud || s_zj_fase != 1) return;
    const int clave = s_zj_pts * 10 + s_zr_vidas;
    if (clave == s_zr_vis) return;
    s_zr_vis = clave;
    char b[16];
    snprintf(b, sizeof b, "%d", s_zj_pts);
    art_texto(&s_zj_hud_art, b, 0xFFE27A, 3, 0);
    poner_src(s_zj_hud_img, &s_zj_hud_art.dsc);
    lv_obj_update_layout(s_zj_hud);
    lv_obj_align(s_zj_hud, LV_ALIGN_TOP_MID, 12, 22);
    ocultar(s_zj_hud, false);
    lv_obj_t *ic = zj_img(38, art_icono(ICO_ESTRELLA, 2));
    lv_obj_align_to(ic, s_zj_hud, LV_ALIGN_OUT_LEFT_MID, -6, 0);
    if (!s_nino)
        for (int k = 0; k < 3; k++) {
            lv_obj_t *c = zj_img(30 + k, art_icono(ICO_CORAZON, 2));
            lv_obj_set_pos(c, 108 + k * 24, 70 + k * 2);
            lv_obj_set_style_opa(c, k < s_zr_vidas ? LV_OPA_COVER : LV_OPA_20, 0);
        }
}
static const art_icono_t ZR_ICO[5] = {ICO_ESTRELLA, ICO_PRISMA, ICO_CORAZON, ICO_NUBE, ICO_PAJARO};
static void zr_cosa_src(int i)
{
    const int q = s_zr_cq[i];
    const int esc = s_zr_cy[i] < 190.0f ? 2 : 3; /* lejos, chiquitas; cerca, grandes */
    art_icono_t ico = ZR_ICO[q];
    if (q == ZRC_PAJARO && (s_zj_t / 180u) % 2u) ico = ICO_PAJARO_B; /* aletea */
    if (esc == s_zr_cesc[i] && q != ZRC_PAJARO && s_zj_o[2 + i] && !lv_obj_has_flag(s_zj_o[2 + i], LV_OBJ_FLAG_HIDDEN)) return;
    s_zr_cesc[i] = (int8_t)esc;
    lv_obj_t *o = zj_img(2 + i, s_zr_cdu[i] < 0 && q == ZRC_PAJARO ? art_icono_espejo(ico, esc) : art_icono(ico, esc));
    if (q == ZRC_NUBE) zj_tinte(o, 0x5A6080, 150); /* nube de tormenta: gris oscura */
    else zj_tinte(o, 0xFFFFFF, 0);
}
static void zr_soltar(void) /* una cosa nueva baja desde la nube */
{
    int i = 0;
    while (i < ZR_COSAS && s_zr_cq[i] != ZRC_NADA) i++;
    if (i >= ZR_COSAS) return;
    const uint32_t r = azar() % 100u;
    int q;
    if (s_nino) q = r < 88 ? ZRC_ESTRELLA : ZRC_PRISMA;
    else if (r < 52) q = ZRC_ESTRELLA;
    else if (r < 58) q = ZRC_PRISMA;
    else if (r < 62 && s_zr_vidas < 3) q = ZRC_CORAZON;
    else if (r < 84) q = ZRC_NUBE;
    else q = ZRC_PAJARO;
    s_zr_cq[i] = (int8_t)q;
    s_zr_cu[i] = -0.8f + azar01() * 1.6f;
    s_zr_cy[i] = ZR_HY + 18.0f;
    s_zr_cdu[i] = q == ZRC_PAJARO ? (azar() % 2 ? 0.35f : -0.35f) : 0.0f;
    s_zr_cesc[i] = 0;
    if (q == ZRC_ESTRELLA && azar() % 3 == 0) { /* a veces, una fila de tres estrellitas seguidas */
        for (int k = 1, j = 0; k < 3 && j < ZR_COSAS; j++)
            if (s_zr_cq[j] == ZRC_NADA) { s_zr_cq[j] = ZRC_ESTRELLA; s_zr_cu[j] = s_zr_cu[i]; s_zr_cy[j] = s_zr_cy[i] - 34.0f * (float)k; s_zr_cdu[j] = 0; s_zr_cesc[j] = 0; k++; }
    }
}
static void zr_empezar(void)
{
    s_zr_u = 0; s_zr_vel = s_nino ? 110.0f : 130.0f; s_zr_avance = 0; s_zr_vidas = 3; s_zr_vis = -1; s_zr_golpe = 0; s_zr_feliz = 0;
    s_zr_cayo = false; s_zr_pose = -1;
    for (int i = 0; i < ZR_COSAS; i++) { s_zr_cq[i] = ZRC_NADA; if (s_zj_o[2 + i]) ocultar(s_zj_o[2 + i], true); }
    for (int k = 0; k < ZR_RAYAS; k++) {
        s_zr_ru[k] = -0.9f + azar01() * 1.8f; s_zr_ry[k] = ZR_HY + azar01() * (ZR_PY + 80.0f - ZR_HY);
        lv_obj_t *o = zj_caja(14 + k, 3, 12, 0xFFFFFF, 2);
        lv_obj_set_style_bg_opa(o, 150, 0);
    }
    /* la barrita del camino (cuánto falta para la punta del arcoíris) */
    lv_obj_t *b = zj_caja(24, 120, 12, 0x1A1430, 6); lv_obj_set_style_bg_opa(b, LV_OPA_60, 0);
    lv_obj_set_style_border_width(b, 2, 0); lv_obj_set_style_border_color(b, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_pos(b, CX - 30, 70);
    lv_obj_t *f = zj_caja(25, 4, 6, 0xFFE27A, 3); lv_obj_set_pos(f, CX - 26, 73);
    lv_obj_t *ic = zj_img(26, art_icono(ICO_ARCOIRIS, 2)); lv_obj_set_pos(ic, CX + 94, 64);
    s_zr_prox = s_zj_t + 1200u;
    zj_texto(s_nino ? NULL : "DESLIZA EL DEDO A LOS LADOS EN CUALQUIER PARTE Y ELLA SE MUEVE IGUAL. JUNTA ESTRELLITAS; ESQUIVA LAS NUBES DE TORMENTA Y LOS PÁJAROS", 5200);
    s_zr_empieza = s_zj_trampa ? 0 : ((s_nino ? s_zj_t + 1500u : s_zj_txt_hasta) | 1u);
    s_zr_tenia = false; s_zr_solto = false;
    zd_mano(ICO_MANO_DESLIZA, CX, 300.0f, 5000u);
}
static void zr_fin(bool cayo)
{
    if (s_zj_fase != 1) return;
    s_zr_cayo = cayo;
    s_zd_fin_t = s_zj_t + 900u; s_zj_fase = 3;
    zd_mano_fuera();
    if (!cayo) { zj_texto("¡LLEGÓ A LA PUNTA DEL ARCOÍRIS!", 1400); for (int k = 0; k < 3; k++) zj_chispas(zr_x(s_zr_u, ZR_PY) - 30.0f + 30.0f * (float)k, ZR_PY - 50.0f, 8, 0, 200.0f, 200.0f); }
    else zj_texto("¡UY! SE RESBALÓ DEL ARCOÍRIS", 1400);
}
static void zr_animar(float dt)
{
    if (s_zj_fase != 1) return;
    if (s_zr_empieza) { /* quieto mientras se leen las instrucciones (o hasta que tocas) */
        if (!s_zj_dedo) s_zr_solto = true;
        if ((int32_t)(s_zj_t - s_zr_empieza) < 0 && !(s_zj_dedo && s_zr_solto)) dt = 0;
        else {
            if ((int32_t)(s_zj_t - s_zr_empieza) < 0) zj_texto(NULL, 0);
            s_zr_empieza = 0; s_zr_prox = s_zj_t + 700u; s_zj_juego_t = s_zj_t;
        }
    }
    const float dura = (float)(s_nino ? 45000u : ZR_DURA) * (s_zj_trampa ? 0.35f : 1.0f);
    if (dt > 0) s_zr_avance += dt * 1000.0f / dura;
    const float dif = s_zr_avance > 1.0f ? 1.0f : s_zr_avance;
    s_zr_vel = (s_nino ? 110.0f : 130.0f) + (s_nino ? 50.0f : 120.0f) * dif;
    /* ella: el dedo la mueve a los lados como él se mueve (desde donde está) */
    const float hwp = zr_hw(ZR_PY);
    float meta = s_zr_u;
    if (s_zj_trampa) { /* (pruebas) va hacia la estrellita más cerca y se aparta de las nubes */
        float best = 1e9f;
        for (int i = 0; i < ZR_COSAS; i++) if (s_zr_cq[i] == ZRC_ESTRELLA || s_zr_cq[i] == ZRC_PRISMA) { const float d = ZR_PY - s_zr_cy[i]; if (d > 0 && d < best) { best = d; meta = s_zr_cu[i]; } }
    } else if (s_zj_dedo && !s_zr_empieza) {
        if (!s_zr_tenia) { s_zr_tenia = true; s_zr_dx0 = s_zj_dedo_x; s_zr_u0 = s_zr_u; }
        meta = s_zr_u0 + (s_zj_dedo_x - s_zr_dx0) * 1.3f / hwp;
        if (meta < -0.86f || meta > 0.86f) { meta = meta < 0 ? -0.86f : 0.86f; s_zr_u0 = meta - (s_zj_dedo_x - s_zr_dx0) * 1.3f / hwp; } /* (en el borde: el dedo la vuelve a tomar desde ahí) */
        zd_mano_fuera();
    }
    if (!s_zj_dedo) s_zr_tenia = false;
    if (meta < -0.86f) meta = -0.86f;
    if (meta > 0.86f) meta = 0.86f;
    { const float paso = 3.2f * dt, du = meta - s_zr_u; s_zr_u += du > paso ? paso : (du < -paso ? -paso : du); }
    const float px = zr_x(s_zr_u, ZR_PY);
    /* las rayitas de luz (se estiran al acercarse: da sensación de velocidad) */
    for (int k = 0; k < ZR_RAYAS; k++) {
        s_zr_ry[k] += s_zr_vel * 1.25f * zr_prof(s_zr_ry[k]) * dt;
        if (s_zr_ry[k] > (float)W + 20.0f) { s_zr_ry[k] = ZR_HY + 10.0f; s_zr_ru[k] = -0.9f + azar01() * 1.8f; }
        lv_obj_t *o = s_zj_o[14 + k];
        if (!o) continue;
        const int largo = 6 + (int)(26.0f * zr_prof(s_zr_ry[k]));
        if (lv_obj_get_style_height(o, LV_PART_MAIN) != largo) lv_obj_set_height(o, largo);
        lv_obj_set_pos(o, (int)zr_x(s_zr_ru[k], s_zr_ry[k]) - 1, (int)s_zr_ry[k] - largo / 2);
    }
    /* las cosas que bajan */
    if (dt > 0 && (int32_t)(s_zj_t - s_zr_prox) >= 0 && s_zr_avance < 0.97f) {
        s_zr_prox = s_zj_t + (uint32_t)((s_nino ? 1100.0f : 950.0f) - (s_nino ? 200.0f : 430.0f) * dif) + azar() % 300u;
        zr_soltar();
    }
    const bool invul = s_zr_golpe && s_zj_t - s_zr_golpe < 1500u;
    for (int i = 0; i < ZR_COSAS; i++) {
        lv_obj_t *o = s_zj_o[2 + i];
        if (s_zr_cq[i] == ZRC_NADA) { if (o) ocultar(o, true); continue; }
        s_zr_cy[i] += s_zr_vel * zr_prof(s_zr_cy[i]) * dt;
        if (s_zr_cdu[i] != 0) { s_zr_cu[i] += s_zr_cdu[i] * dt; if (s_zr_cu[i] < -0.85f || s_zr_cu[i] > 0.85f) s_zr_cdu[i] = -s_zr_cdu[i]; }
        if (s_zr_cy[i] > (float)W + 30.0f) { s_zr_cq[i] = ZRC_NADA; if (o) ocultar(o, true); continue; }
        zr_cosa_src(i);
        o = s_zj_o[2 + i];
        const float cx = zr_x(s_zr_cu[i], s_zr_cy[i]);
        const float flota = s_zr_cq[i] == ZRC_ESTRELLA || s_zr_cq[i] == ZRC_PRISMA ? 3.0f * sinf((float)s_zj_t / 200.0f + (float)i) : 0.0f;
        zj_centro(o, cx, s_zr_cy[i] + flota);
        ocultar(o, s_zr_cy[i] < 96.0f); /* (sale de detrás de la nube: arriba no tapa la barrita) */
        /* ¿la tocó? (las cosas pasan a la altura de su cuerpo) */
        const float dy = s_zr_cy[i] - (ZR_PY - 34.0f), dx = cx - px;
        if (dy > -26.0f && dy < 20.0f && fabsf(dx) < 32.0f) {
            const int q = s_zr_cq[i];
            if ((q == ZRC_NUBE || q == ZRC_PAJARO) && (invul || s_zj_trampa)) continue;
            s_zr_cq[i] = ZRC_NADA;
            if (o) ocultar(o, true);
            if (q == ZRC_ESTRELLA || q == ZRC_PRISMA) {
                const int n = q == ZRC_PRISMA ? 3 : 1;
                s_zj_pts += n;
                zj_mas(n, cx, s_zr_cy[i] - 24.0f, q == ZRC_PRISMA ? 0xFF9AE0 : 0xFFE27A);
                zj_chispas(cx, s_zr_cy[i], q == ZRC_PRISMA ? 12 : 6, q == ZRC_PRISMA ? 0 : 0xFFE27A, 160.0f, 0.0f);
                accion(UI_ACCION_SONAR, q == ZRC_PRISMA ? SND_TESORO : SND_TOQUE);
                s_zr_feliz = s_zj_t;
            } else if (q == ZRC_CORAZON) {
                if (s_zr_vidas < 3) s_zr_vidas++;
                zj_chispas(cx, s_zr_cy[i], 8, 0xFF8AB0, 140.0f, 0.0f);
                accion(UI_ACCION_SONAR, SND_ALEGRE);
                s_zr_feliz = s_zj_t;
            } else { /* una nube de tormenta o un pájaro: pierde un corazón */
                s_zr_vidas--;
                s_zr_golpe = s_zj_t;
                zj_chispas(px, ZR_PY - 40.0f, 8, q == ZRC_NUBE ? 0x8A90B0 : 0xFFFFFF, 150.0f, 0.0f);
                accion(UI_ACCION_SONAR, SND_QUEJA);
                if (s_zr_vidas > 0) zj_texto(q == ZRC_NUBE ? "¡UY! UNA NUBE DE TORMENTA" : "¡UY! UN PAJARITO", 900);
                else { zr_fin(true); return; }
            }
        }
    }
    /* ella, deslizándose (se mece un poquito; parpadea si la golpearon) */
    const int pose = invul ? 5 : (s_zr_feliz && s_zj_t - s_zr_feliz < 600u ? 4 : 1);
    s_zr_pose = pose;
    const float mece = 2.0f * sinf((float)s_zj_t / 160.0f);
    zd_poner(0, pose, 3, s_zr_u < -0.02f, px, ZR_PY + mece);
    if (s_zj_o[0]) ocultar(s_zj_o[0], invul && (s_zj_t / 120u) % 2u);
    if (dt > 0 && (s_zj_t / 90u) % 2u) zj_chispas(px + (azar01() - 0.5f) * 30.0f, ZR_PY + 4.0f, 1, 0xFFFFFF, 40.0f, -40.0f); /* polvito de colores detrás */
    /* la barrita del camino */
    if (s_zj_o[25]) { const int w = 4 + (int)(108.0f * dif); if (lv_obj_get_style_width(s_zj_o[25], LV_PART_MAIN) != w) lv_obj_set_width(s_zj_o[25], w); }
    if (s_zr_avance >= 1.0f) zr_fin(false);
    zr_marcador();
}
static void zr_portada(void)
{
    zj_titulo("EL TOBOGÁN DEL ARCOÍRIS");
    lv_obj_t *c = caja(s_zj);
    lv_obj_set_size(c, 320, 250);
    lv_obj_align(c, LV_ALIGN_CENTER, 0, 10);
    lv_obj_set_style_bg_color(c, lv_color_hex(0x2A2458), 0);
    lv_obj_set_style_bg_opa(c, LV_OPA_80, 0);
    lv_obj_set_style_radius(c, 24, 0);
    lv_obj_set_style_border_width(c, 3, 0);
    lv_obj_set_style_border_color(c, lv_color_hex(0xFFE9A0), 0);
    const lv_image_dsc_t *d = zd_criatura(0, 1, 3, false);
    if (d) { lv_obj_t *im = lv_image_create(c); lv_obj_remove_flag(im, LV_OBJ_FLAG_CLICKABLE); lv_image_set_src(im, d); lv_obj_align(im, LV_ALIGN_TOP_MID, -50, 14); }
    lv_obj_t *ic = icono(c, ICO_ARCOIRIS, 4);
    lv_obj_align(ic, LV_ALIGN_TOP_MID, 50, 18);
    if (!s_nino) {
        lv_obj_t *t = zj_texto_c(c, 0, "SE DESLIZA POR EL ARCOÍRIS. MUÉVELA A LOS LADOS CON EL DEDO: JUNTA ESTRELLITAS Y ESQUIVA LAS NUBES DE TORMENTA Y LOS PÁJAROS.", 0xCFE8FF, 2, 296);
        lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 92);
        lv_obj_t *t2 = zj_texto_c(c, 1, "TOCA PARA EMPEZAR", 0xFFE9A0, 2, 280); lv_obj_align(t2, LV_ALIGN_BOTTOM_MID, 0, -14);
    }
    s_zj_o[45] = c;
}
static void zr_fondo_simple(void) /* (si falta pj6: un cielo y un arcoíris pintados con cajas) */
{
    lv_obj_set_style_bg_color(s_zj, lv_color_hex(0x5AA0E8), 0);
    lv_obj_set_style_bg_grad_color(s_zj, lv_color_hex(0xD4EEFF), 0);
    lv_obj_set_style_bg_grad_dir(s_zj, LV_GRAD_DIR_VER, 0);
    static const uint32_t C[7] = {0xF0506E, 0xFF8A3C, 0xFFD84A, 0x6CD86C, 0x45D0E0, 0x4C88F0, 0x9A62E0};
    for (int k = 0; k < 7; k++) {
        lv_obj_t *o = caja(s_zj);
        const int w = 30;
        lv_obj_set_size(o, w, W);
        lv_obj_set_pos(o, CX - 105 + k * w, 0);
        lv_obj_set_style_bg_color(o, lv_color_hex(C[k]), 0);
        lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    }
}
static void zr_terminar(void)
{
    if (s_zj_fase != 3) return;
    s_zj_fase = 2;
    s_zj_gano = true;
    s_zj_fin_t = s_zj_t;
    const int pts = s_zj_pts > 127 ? 127 : s_zj_pts;
    const bool bien = pts >= (s_nino ? 10 : 20);
    accion(UI_ACCION_JUEGO_ISLA, PET_ISLA_ARCOIRIS | (pts << 4));
    accion(UI_ACCION_SONAR, bien ? SND_TESORO : SND_ALEGRE);
    for (int k = 0; k < 3; k++) zj_chispas(CX + (float)(k - 1) * 90.0f, 170.0f, 6, 0, 240.0f, 380.0f);
    ocultar(s_zj_txt, true);
    for (int i = 0; i < 40; i++) if (s_zj_o[i]) ocultar(s_zj_o[i], true);
    s_zj_fin = caja(s_zj);
    lv_obj_set_size(s_zj_fin, 320, 236);
    lv_obj_align(s_zj_fin, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(s_zj_fin, lv_color_hex(0x2A2458), 0);
    lv_obj_set_style_bg_opa(s_zj_fin, LV_OPA_90, 0);
    lv_obj_set_style_radius(s_zj_fin, 26, 0);
    lv_obj_set_style_border_width(s_zj_fin, 3, 0);
    lv_obj_set_style_border_color(s_zj_fin, lv_color_hex(bien ? 0xFFE9A0 : 0xCFE8FF), 0);
    const lv_image_dsc_t *d = zd_criatura(0, 4, 3, false);
    s_zd_puesta[0] = 0;
    if (d) { lv_obj_t *im = lv_image_create(s_zj_fin); lv_obj_remove_flag(im, LV_OBJ_FLAG_CLICKABLE); lv_image_set_src(im, d); lv_obj_align(im, LV_ALIGN_TOP_MID, -40, s_nino ? 50 : 70); }
    lv_obj_t *ic = icono(s_zj_fin, ICO_ARCOIRIS, 4);
    lv_obj_align(ic, LV_ALIGN_TOP_MID, 50, s_nino ? 56 : 78);
    if (!s_nino) {
        char b[64];
        if (pts == 0) snprintf(b, sizeof b, "ESTA VEZ NO HUBO ESTRELLITAS");
        else if (pts == 1) snprintf(b, sizeof b, "¡UNA ESTRELLITA!");
        else snprintf(b, sizeof b, "¡%d ESTRELLITAS!", pts);
        art_texto(&s_zj_fin_art[0], b, bien ? 0xFFE27A : 0xCFE8FF, 3, 290);
        lv_obj_t *t = lv_image_create(s_zj_fin); poner_src(t, &s_zj_fin_art[0].dsc); lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 16);
        art_texto(&s_zj_fin_art[1], s_zr_cayo ? "SE RESBALÓ ANTES DE LA PUNTA" : "¡LLEGÓ A LA PUNTA DEL ARCOÍRIS!", 0xFFF3C8, 2, 280);
        lv_obj_t *r = lv_image_create(s_zj_fin); poner_src(r, &s_zj_fin_art[1].dsc); lv_obj_align(r, LV_ALIGN_BOTTOM_MID, 0, -16);
    }
    ocultar(s_zj_hud, true);
    lv_obj_t *o = boton(s_zj, BOTON_ABAJO, BOTON_ABAJO, zj_otra_cb, 0);
    lv_obj_set_ext_click_area(o, 14);
    lv_obj_center(icono(o, ICO_VUELTA, 2));
    lv_obj_set_pos(o, CX - BOTON_ABAJO / 2 - VOLVER_DX / 2, BOTON_ABAJO_Y);
    s_zj_o[44] = o;
    ocultar(s_zj_x, true);
    s_zj_x = boton(s_zj, BOTON_ABAJO, BOTON_ABAJO, zj_fin_x_cb, 0);
    lv_obj_set_ext_click_area(s_zj_x, 14);
    lv_obj_center(icono(s_zj_x, ICO_NO, 2));
    lv_obj_set_pos(s_zj_x, CX - BOTON_ABAJO / 2 + VOLVER_DX / 2, BOTON_ABAJO_Y);
}
int ui_test_zr(int q) /* (pruebas) 0 vidas, 1 avance ×1000, 2 posición de ella (x) */
{
    switch (q) {
    case 0: return s_zr_vidas;
    case 1: return (int)(s_zr_avance * 1000.0f);
    case 2: return (int)zr_x(s_zr_u, ZR_PY);
    default: return 0;
    }
}
