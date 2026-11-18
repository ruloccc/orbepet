/* ======================================================================
 * (1.0.17) Los juegos de a dos: cuando hay una visita (el amigo del barrio o un visitante), el menú JUGAR cambia por
 * cuatro juegos para los dos. Cada uno en su pantalla (el marco de ui_zjuegos.inc: su propio reloj de 33 ms, el mundo de
 * atrás se aparta y deja de animarse) con su fondo, que vive en la memoria de recursos (/res/pj0..pj3.bin; si falta, se
 * pinta uno sencillo y el juego funciona igual).
 *   PING PONG:      tú mueves su paleta con el dedo (abajo); la visita juega arriba. Gana el primero en llegar a 5.
 *   TIRA Y AFLOJA:  toca rápido para tirar la cuerda. La visita tira fuerte a ratos y después se cansa: ¡ahí hay que
 *                   tirar! El que lleva la cinta a su bandera gana; el otro cae al barro.
 *   MEMORICE:       16 cartas en el mantel; por turnos, dos cada vez. La visita se acuerda de algunas.
 *   ESCONDIDAS:     por turnos: la visita se esconde (las hojas se mueven donde está) y tú la buscas; después tú eliges
 *                   dónde se esconde ella y la visita la busca.
 * Al terminar: quién ganó, el marcador, OTRA VEZ o la X. El motor anota la partida (pet_duo_game).
 * Incluido desde ui_zjuegos.inc (usa sus piezas: s_zj, s_zj_o[], zj_img, zj_caja, zj_chispas, zj_texto...).
 * ==================================================================== */
static int s_zd_b, s_zd_vis = -1;        /* puntos de la visita (los de ella: s_zj_pts) y el marcador que está escrito */
static art_img_t s_zd_art[2];            /* las caritas chicas del marcador */
static uint32_t s_zd_key[2];             /* (para no redibujarlas en cada cuadro) qué pose tiene cada criatura grande */
static uint32_t s_zd_puesta[2];          /* y cuál está puesta en su imagen */
static uint32_t s_zd_fin_t;              /* cuándo termina la animación del final de una partida (0: no) */
static int s_zd_res = -1;                /* 0 perdió ella, 1 empate, 2 ganó */
static bool zd_es(int jg) { return jg >= ZJ_PING && jg <= ZJ_BUCEO; } /* (1.0.18: también los de la isla, que usan este marco) */
static bool zi_es(int jg) { return jg == ZJ_FARO || jg == ZJ_BUCEO; } /* (1.0.18) los de la isla (ui_isla.inc) */
static int zd_n(void) { return s_zj_jg - ZJ_PING; } /* 0..3 = PET_DOS_* */

/* ---------- quién es la visita ---------- */
static bool dos_hay_visita(void)
{
    if (!s_pet || s_pet->etapa == STAGE_HUEVO || s_pet->dormida) return false;
    if (s_vecino.visible && !s_vec_sale && s_pet->vecino.presente) return true;
    return s_visita.visible && s_pet->visita_actual >= 0;
}
static const art_pose_t *zd_rival_pose(void) { return s_vecino.visible ? &s_vecino.pose : &s_visita.pose; }
static void zd_rival_nombre(char *b, size_t n)
{
    if (s_pet && s_vecino.visible && s_pet->vecino.nombre[0]) snprintf(b, n, "%s", s_pet->vecino.nombre);
    else snprintf(b, n, "%s", gtxt("TU VISITA"));
    mayusculas(b);
}

/* ---------- las dos criaturas (k: 0 de pie, 1 contenta, 2 apurada, 3 ojos cerrados, 4 feliz, 5 triste) ---------- */
static const lv_image_dsc_t *zd_criatura(int quien, int k, int escala, bool espejo)
{
    if (!s_pet || quien < 0 || quien > 1) return NULL;
    const uint32_t key = 1u + (uint32_t)k * 8u + (uint32_t)escala * 64u + (espejo ? 4096u : 0u);
    if (s_zd_key[quien] == key && s_zj_pet_art[quien].px) return &s_zj_pet_art[quien].dsc;
    static const art_expresion_t EX[6] = {EXP_FELIZ, EXP_FELIZ, EXP_CURIOSO, EXP_CERRADOS, EXP_FELIZ, EXP_TRISTE};
    art_pose_t po;
    if (quien == 0) {
        const pet_t *m = s_pet;
        memset(&po, 0, sizeof po);
        po.especie = pet_look(m); po.etapa = m->etapa; po.forma = m->etapa == STAGE_ADULTA ? m->forma : 0;
        for (int r = 0; r < RANURA_COUNT; r++) po.ropa[r] = pet_worn_raw(m, r);
        po.capa = m->guardiana;
    } else {
        po = *zd_rival_pose();
        po.sucio = 0; po.enferma = false;
    }
    po.escala = escala;
    po.expr = EX[k % 6];
    po.boca = k == 1 || k == 4;
    po.frame = 0;
    po.espejo = espejo;
    po.orejas = k == 5 ? -1 : 1;
    po.cola = k == 5 ? -1 : (k == 1 || k == 4 ? 1 : 0);
    po.postura = k == 5 ? -1 : 0;
    art_criatura_pose(&s_zj_pet_art[quien], &po);
    s_zd_key[quien] = key;
    return s_zj_pet_art[quien].px ? &s_zj_pet_art[quien].dsc : NULL;
}
/* la criatura grande en el objeto quien (0 ella, 1 la visita), con sus pies en (x, y) */
static void zd_poner(int quien, int k, int escala, bool espejo, float x, float y)
{
    const lv_image_dsc_t *d = zd_criatura(quien, k, escala, espejo);
    if (!d) return;
    lv_obj_t *o = s_zj_o[quien];
    if (!o || s_zd_puesta[quien] != s_zd_key[quien]) { o = zj_img(quien, d); s_zd_puesta[quien] = s_zd_key[quien]; }
    else ocultar(o, false);
    lv_obj_set_pos(o, (int)x - (int)d->header.w / 2, (int)y - (int)d->header.h + 4);
}

/* ---------- el marcador: carita de ella, "3 - 2", carita de la visita ---------- */
static void zd_marcador(void)
{
    if (!s_zj_hud || s_zj_fase != 1 || s_zj_jg == ZJ_CUERDA || zi_es(s_zj_jg)) return; /* (en la cuerda, el marcador es la cinta) */
    const int clave = s_zj_pts * 100 + s_zd_b;
    if (clave == s_zd_vis) return;
    s_zd_vis = clave;
    char b[24];
    snprintf(b, sizeof b, "%d - %d", s_zj_pts, s_zd_b);
    art_texto(&s_zj_hud_art, b, 0xFFE27A, 3, 0);
    poner_src(s_zj_hud_img, &s_zj_hud_art.dsc);
    lv_obj_update_layout(s_zj_hud);
    lv_obj_align(s_zj_hud, LV_ALIGN_TOP_MID, 0, 22);
    ocultar(s_zj_hud, false);
    if (s_zj_jg == ZJ_MEMO) return; /* (en el memorice las dos están arriba, grandes) */
    for (int q = 0; q < 2; q++) {
        if (!s_zd_art[q].px) {
            art_pose_t po;
            if (q == 0) {
                memset(&po, 0, sizeof po);
                po.especie = pet_look(s_pet); po.etapa = s_pet->etapa; po.forma = s_pet->etapa == STAGE_ADULTA ? s_pet->forma : 0;
            } else { po = *zd_rival_pose(); po.sucio = 0; po.enferma = false; for (int r = 0; r < 4; r++) po.ropa[r] = 0; }
            po.escala = 1; po.expr = EXP_FELIZ; po.espejo = q == 1; po.orejas = 1;
            art_criatura_pose(&s_zd_art[q], &po);
        }
        if (!s_zd_art[q].px) continue;
        lv_obj_t *o = zj_img(38 + q, &s_zd_art[q].dsc);
        lv_obj_align_to(o, s_zj_hud, q ? LV_ALIGN_OUT_RIGHT_MID : LV_ALIGN_OUT_LEFT_MID, q ? 6 : -6, 0);
    }
}
static void zd_punto(int quien, float x, float y)
{
    if (quien == 0) { s_zj_pts++; zj_mas(1, x, y, 0xFFF3B0); zj_chispas(x, y, 8, 0, 200.0f, 300.0f); accion(UI_ACCION_SONAR, SND_ALEGRE); }
    else { s_zd_b++; zj_mas(1, x, y, 0x9FD8F0); accion(UI_ACCION_SONAR, SND_RISA); }
}

/* ======================================================================
 * 1) PING PONG (fondo: la cancha de tierra, de y 66 a 404 y x 96 a 370; la red en y 235)
 * ==================================================================== */
#define ZPP_X0 106.0f
#define ZPP_X1 360.0f
#define ZPP_YE 368.0f  /* la paleta de ella */
#define ZPP_YR 102.0f  /* la de la visita */
#define ZPP_GANA (s_nino ? 3 : 5)
static float s_zpp_bx, s_zpp_by, s_zpp_vx, s_zpp_vy, s_zpp_e, s_zpp_r, s_zpp_err;
static int s_zpp_saca;        /* quién saca: 0 ella, 1 la visita */
static uint32_t s_zpp_sale;   /* cuándo sale la pelota (0: en juego) */
static int s_zpp_golpes;
static float zpp_ancho(void) { return s_nino ? 96.0f : 72.0f; }
static void zpp_saque(int quien)
{
    s_zpp_saca = quien;
    s_zpp_sale = s_zj_t + 1000u;
    s_zpp_vx = s_zpp_vy = 0;
    s_zpp_golpes = 0;
}
static void zpp_empezar(void)
{
    s_zpp_e = s_zpp_r = CX;
    s_zpp_err = 0;
    lv_obj_t *o = zj_caja(2, (int)zpp_ancho(), 12, 0xD98A3F, 6);
    lv_obj_set_style_border_width(o, 2, 0); lv_obj_set_style_border_color(o, lv_color_hex(0x7A3E14), 0);
    o = zj_caja(3, (int)zpp_ancho(), 12, 0x5A9BD8, 6);
    lv_obj_set_style_border_width(o, 2, 0); lv_obj_set_style_border_color(o, lv_color_hex(0x24507A), 0);
    o = zj_caja(5, 16, 7, 0x000000, LV_RADIUS_CIRCLE); lv_obj_set_style_bg_opa(o, LV_OPA_40, 0); /* la sombra */
    o = zj_caja(4, 14, 14, 0xFFFDF0, LV_RADIUS_CIRCLE);
    lv_obj_set_style_border_width(o, 1, 0); lv_obj_set_style_border_color(o, lv_color_hex(0xC8C0A0), 0);
    zpp_saque((int)(azar() % 2));
    zj_texto("MUEVE SU PALETA CON EL DEDO. ¡GANA EL PRIMERO EN LLEGAR A 5!", 3500);
}
static void zpp_animar(float dt)
{
    if (s_zj_fase != 1) return;
    const float pw = zpp_ancho(), media = pw / 2.0f;
    /* la paleta de ella: sigue el dedo (o, en las pruebas, la pelota) */
    float obj = s_zpp_e;
    if (s_zj_trampa) obj = s_zpp_bx;
    else if (s_zj_dedo) obj = s_zj_dedo_x;
    float paso = 1100.0f * dt;
    if (obj > s_zpp_e + paso) s_zpp_e += paso; else if (obj < s_zpp_e - paso) s_zpp_e -= paso; else s_zpp_e = obj;
    if (s_zpp_e < ZPP_X0 + media - 12) s_zpp_e = ZPP_X0 + media - 12;
    if (s_zpp_e > ZPP_X1 - media + 12) s_zpp_e = ZPP_X1 - media + 12;
    /* la visita: va hacia la pelota cuando viene (con un poco de error); si no, vuelve al medio */
    const float total = (float)(s_zj_pts + s_zd_b);
    float vel = s_nino ? 105.0f : 150.0f + 9.0f * total;
    if (s_suave) vel *= 0.8f;
    float meta = s_zpp_vy < 0 ? s_zpp_bx + s_zpp_err : CX;
    if (s_zpp_sale && s_zpp_saca == 1) meta = CX + s_zpp_err * 0.3f;
    paso = vel * dt;
    if (meta > s_zpp_r + paso) s_zpp_r += paso; else if (meta < s_zpp_r - paso) s_zpp_r -= paso; else s_zpp_r = meta;
    if (s_zpp_r < ZPP_X0 + media - 12) s_zpp_r = ZPP_X0 + media - 12;
    if (s_zpp_r > ZPP_X1 - media + 12) s_zpp_r = ZPP_X1 - media + 12;
    /* la pelota */
    if (s_zpp_sale) {
        s_zpp_bx = s_zpp_saca ? s_zpp_r : s_zpp_e;
        s_zpp_by = s_zpp_saca ? ZPP_YR + 14 : ZPP_YE - 14;
        if ((int32_t)(s_zj_t - s_zpp_sale) >= 0) {
            s_zpp_sale = 0;
            const float v0 = s_nino ? 170.0f : 235.0f;
            s_zpp_vy = s_zpp_saca ? v0 : -v0;
            s_zpp_vx = (azar01() - 0.5f) * v0 * 0.8f;
            s_zpp_err = (azar01() - 0.5f) * (s_nino ? 110.0f : 70.0f);
            accion(UI_ACCION_SONAR, SND_TOQUE);
        }
    } else {
        const float py = s_zpp_by;
        s_zpp_bx += s_zpp_vx * dt;
        s_zpp_by += s_zpp_vy * dt;
        if (s_zpp_bx < ZPP_X0) { s_zpp_bx = ZPP_X0; s_zpp_vx = -s_zpp_vx; }
        if (s_zpp_bx > ZPP_X1) { s_zpp_bx = ZPP_X1; s_zpp_vx = -s_zpp_vx; }
        const float tope = s_nino ? 300.0f : 520.0f;
        if (s_zpp_vy > 0 && py <= ZPP_YE - 13 && s_zpp_by >= ZPP_YE - 13 && fabsf(s_zpp_bx - s_zpp_e) <= media + 8) {
            s_zpp_by = ZPP_YE - 13;
            s_zpp_vy = -fminf(fabsf(s_zpp_vy) * 1.06f, tope);
            s_zpp_vx += (s_zpp_bx - s_zpp_e) / media * 150.0f;
            s_zpp_err = (azar01() - 0.5f) * (s_nino ? 110.0f : 70.0f);
            s_zpp_golpes++;
            accion(UI_ACCION_SONAR, SND_REBOTE);
        } else if (s_zpp_vy < 0 && py >= ZPP_YR + 13 && s_zpp_by <= ZPP_YR + 13 && fabsf(s_zpp_bx - s_zpp_r) <= media + 8) {
            s_zpp_by = ZPP_YR + 13;
            s_zpp_vy = fminf(fabsf(s_zpp_vy) * 1.06f, tope);
            s_zpp_vx += (s_zpp_bx - s_zpp_r) / media * 130.0f + (azar01() - 0.5f) * 80.0f;
            s_zpp_golpes++;
            accion(UI_ACCION_SONAR, SND_REBOTE);
        }
        if (s_zpp_vx > 300.0f) s_zpp_vx = 300.0f;
        if (s_zpp_vx < -300.0f) s_zpp_vx = -300.0f;
        if (s_zpp_by > ZPP_YE + 34) { zd_punto(1, s_zpp_bx, ZPP_YE); zpp_saque(0); }
        else if (s_zpp_by < ZPP_YR - 34) { zd_punto(0, s_zpp_bx, ZPP_YR + 30); zpp_saque(1); }
    }
    /* dibujar */
    zj_centro(s_zj_o[2], s_zpp_e, ZPP_YE);
    zj_centro(s_zj_o[3], s_zpp_r, ZPP_YR);
    const float alto = s_zpp_sale ? 0.0f : 10.0f * sinf((s_zpp_by - ZPP_YR) / (ZPP_YE - ZPP_YR) * 3.1416f); /* (vuela un poco sobre la red) */
    zj_centro(s_zj_o[5], s_zpp_bx, s_zpp_by + 4);
    zj_centro(s_zj_o[4], s_zpp_bx, s_zpp_by - alto);
    const int ke = s_zj_pts > s_zd_b ? 1 : 0, kr = s_zd_b > s_zj_pts ? 1 : 0;
    /* (cada una al lado de su paleta, hacia el medio de la cancha: así no se sale del círculo) */
    zd_poner(0, ke, 2, s_zpp_e > CX, s_zpp_e + (s_zpp_e > CX ? -1.0f : 1.0f) * (media + 18), ZPP_YE + 16);
    zd_poner(1, kr, 2, s_zpp_r > CX, s_zpp_r + (s_zpp_r > CX ? -1.0f : 1.0f) * (media + 18), ZPP_YR + 14);
    if (s_zj_pts >= ZPP_GANA || s_zd_b >= ZPP_GANA) { s_zd_res = s_zj_pts > s_zd_b ? 2 : 0; s_zd_fin_t = s_zj_t + 700u; s_zj_fase = 3; }
}

/* ======================================================================
 * 2) TIRA Y AFLOJA (fondo: el barro al centro (233, 252), las banderitas en x 176 y 290)
 * ==================================================================== */
#define ZC_Y 262.0f
#define ZC_BANDERA 57.0f   /* la cinta llega a una bandera: 233 ∓ 57 */
static float s_zc_off;     /* dónde está la cinta (negativo: hacia ella) */
static int s_zc_fase;      /* 0 tira parejo, 1 tira fuerte, 2 cansada */
static uint32_t s_zc_hasta, s_zc_auto, s_zc_tiron;
static int s_zc_cae = -1;  /* quién cae al barro (al final) */
static void zc_empezar(void)
{
    s_zc_off = 0; s_zc_fase = 0; s_zc_hasta = s_zj_t + 2500u + azar() % 1500u; s_zc_auto = 0; s_zc_tiron = 0; s_zc_cae = -1;
    zj_caja(2, 250, 6, 0xC9A16A, 3);
    lv_obj_t *o = zj_caja(3, 10, 24, 0xE0483C, 3);
    lv_obj_set_style_border_width(o, 2, 0); lv_obj_set_style_border_color(o, lv_color_hex(0xFFF0E0), 0);
    lv_obj_t *g = zj_img(4, art_icono(ICO_GOTA, 2)); ocultar(g, true); /* la gotita: la visita se cansó */
    zj_texto("¡TOCA RÁPIDO PARA TIRAR! CUANDO SE CANSE, TIRA CON TODO", 3500);
}
static void zc_toque(void)
{
    if (s_zj_fase != 1 || s_zc_cae >= 0 || s_zd_fin_t) return;
    const float f = (s_nino ? 9.0f : 6.0f) * (s_zc_fase == 2 ? 1.8f : 1.0f);
    s_zc_off -= f;
    s_zc_tiron = s_zj_t;
    if (s_zc_fase == 2 && !(azar() % 3)) zj_chispas(CX - 125.0f + s_zc_off, ZC_Y + 20, 2, 0xA07850, 90.0f, 300.0f);
}
static void zc_animar(float dt)
{
    if (s_zj_fase != 1) return;
    if (s_zc_cae < 0 && !s_zd_fin_t) {
        if (s_zj_trampa && (int32_t)(s_zj_t - s_zc_auto) >= 0) { s_zc_auto = s_zj_t + 90u; zc_toque(); }
        const float dif = zj_tiempo() / 40000.0f > 1.0f ? 1.0f : zj_tiempo() / 40000.0f;
        float rate = s_nino ? 14.0f : 24.0f + 12.0f * dif;
        if (s_suave) rate *= 0.8f;
        if (s_zc_fase == 1) rate *= 2.6f; else if (s_zc_fase == 2) rate *= 0.15f;
        s_zc_off += rate * dt;
        if ((int32_t)(s_zj_t - s_zc_hasta) >= 0) {
            s_zc_fase = s_zc_fase == 0 ? 1 : (s_zc_fase == 1 ? 2 : 0);
            s_zc_hasta = s_zj_t + (s_zc_fase == 1 ? 1300u : (s_zc_fase == 2 ? 1700u : 2500u + azar() % 2000u));
            if (s_zc_fase == 1) { zj_texto("¡TIRA FUERTE!", 1200); accion(UI_ACCION_SONAR, SND_GRUNIDO); }
            if (s_zc_fase == 2) { zj_texto("¡SE CANSÓ! ¡AHORA!", 1500); accion(UI_ACCION_SONAR, SND_SUSPIRO); }
        }
        ocultar(s_zj_o[4], s_zc_fase != 2);
        if (s_zc_off <= -ZC_BANDERA || s_zc_off >= ZC_BANDERA || zj_tiempo() > 45000) {
            if (s_zc_off <= -4.0f) s_zd_res = 2; else if (s_zc_off >= 4.0f) s_zd_res = 0; else s_zd_res = 1;
            if (s_zd_res == 2) s_zj_pts = 1; else if (s_zd_res == 0) s_zd_b = 1;
            s_zc_cae = s_zd_res == 2 ? 1 : (s_zd_res == 0 ? 0 : -1);
            s_zd_fin_t = s_zj_t + (s_zc_cae >= 0 ? 1600u : 600u);
            ocultar(s_zj_o[4], true);
            if (s_zc_cae >= 0) { accion(UI_ACCION_SONAR, SND_CHAPOTEO); zj_chispas(CX, 252.0f, 14, 0x7A5634, 220.0f, 420.0f); }
        }
    }
    /* dibujar: la cinta, la cuerda entre las manos y las dos tirando */
    const float cx = CX + s_zc_off;
    float ex = cx - 125.0f, rx = cx + 125.0f, ey = ZC_Y, ry = ZC_Y;
    const bool tira = s_zj_t - s_zc_tiron < 160u;
    if (s_zc_cae >= 0) { /* el que pierde se va de cabeza al barro */
        const float f = 1.0f - (float)(int32_t)(s_zd_fin_t - s_zj_t) / 1600.0f;
        const float q = f < 0 ? 0 : (f > 0.5f ? 1.0f : f / 0.5f);
        const float salto = q < 1.0f ? 8.0f - 30.0f * sinf(q * 3.1416f) : 4.0f;
        if (s_zc_cae == 0) { ex = ex + (CX - 20 - ex) * q; ey = ZC_Y + salto; }
        else { rx = rx + (CX + 20 - rx) * q; ry = ZC_Y + salto; }
    }
    zd_poner(0, s_zc_cae == 0 ? 5 : (s_zc_cae == 1 ? 4 : (tira ? 2 : 0)), 3, false, ex - (tira && s_zc_cae < 0 ? 4 : 0), ey + 22);
    zd_poner(1, s_zc_cae == 1 ? 5 : (s_zc_cae == 0 ? 4 : (s_zc_fase == 1 ? 2 : (s_zc_fase == 2 ? 3 : 0))), 3, true, rx + (s_zc_fase == 1 ? 3 : 0), ry + 22);
    if (s_zc_cae >= 0) zj_tinte(s_zj_o[s_zc_cae], 0x6B4A2A, (lv_opa_t)(s_zj_t + 1100u > s_zd_fin_t ? 110 : 0));
    const float a = ex + 22.0f, b = rx - 22.0f;
    if (s_zj_o[2]) { lv_obj_set_width(s_zj_o[2], (int)(b - a) > 10 ? (int)(b - a) : 10); lv_obj_set_pos(s_zj_o[2], (int)a, (int)ZC_Y - 3); }
    zj_centro(s_zj_o[3], cx, ZC_Y);
    if (s_zj_o[4] && s_zc_fase == 2) zj_centro(s_zj_o[4], rx + 22, ZC_Y - 58 - (float)((s_zj_t / 200) % 3));
    if (s_zd_fin_t) s_zj_fase = 3;
}

/* ======================================================================
 * 3) MEMORICE (fondo: el mantel de cuadros, x 66 a 400, y 86 a 392)
 * ==================================================================== */
#define ZM_MAX 16
#define ZM_L 62
#define ZM_SEP 9
static const art_icono_t ZM_ICO[8] = {ICO_FRUTA_MANZANA, ICO_FRUTA_PERA, ICO_FRUTA_UVAS, ICO_FRUTA_CEREZAS, ICO_FRUTA_NARANJA, ICO_FRUTA_BANANA, ICO_FRUTILLA, ICO_HONGO};
static int8_t s_zm_par[ZM_MAX], s_zm_est[ZM_MAX]; /* est: 0 boca abajo, 1 dada vuelta, 2 de ella, 3 de la visita */
static uint8_t s_zm_sabe[ZM_MAX];                /* la visita se acuerda de esta carta */
static int s_zm_n, s_zm_filas, s_zm_turno, s_zm_a, s_zm_b2;
static uint32_t s_zm_hasta, s_zm_pensar;
static void zm_pos(int i, float *x, float *y)
{
    const int f = i / 4, c = i % 4;
    *x = CX + ((float)c - 1.5f) * (ZM_L + ZM_SEP);
    *y = 246.0f + ((float)f - (float)(s_zm_filas - 1) / 2.0f) * (ZM_L + ZM_SEP);
}
static void zm_pintar(int i)
{
    lv_obj_t *o = s_zj_o[2 + i];
    if (!o) return;
    const int e = s_zm_est[i];
    lv_obj_t *ic = lv_obj_get_child(o, 0), *dorso = lv_obj_get_child(o, 1);
    if (e == 0) {
        lv_obj_set_style_bg_color(o, lv_color_hex(0x4E78C0), 0);
        lv_obj_set_style_bg_grad_color(o, lv_color_hex(0x2F4F8E), 0);
        lv_obj_set_style_border_color(o, lv_color_hex(0xFFF3C8), 0);
        lv_obj_set_style_opa(o, LV_OPA_COVER, 0);
        if (ic) ocultar(ic, true);
        if (dorso) ocultar(dorso, false);
    } else {
        lv_obj_set_style_bg_color(o, lv_color_hex(0xFFF8E6), 0);
        lv_obj_set_style_bg_grad_color(o, lv_color_hex(0xF2E4C4), 0);
        lv_obj_set_style_border_color(o, lv_color_hex(e == 2 ? 0xFFC83C : (e == 3 ? 0x6AB8FF : 0xFFFFFF)), 0);
        lv_obj_set_style_opa(o, e >= 2 ? LV_OPA_70 : LV_OPA_COVER, 0);
        if (ic) ocultar(ic, false);
        if (dorso) ocultar(dorso, true);
    }
}
static void zm_empezar(void)
{
    s_zm_filas = s_nino ? 3 : 4;
    s_zm_n = s_zm_filas * 4;
    for (int i = 0; i < s_zm_n; i++) { s_zm_par[i] = (int8_t)(i / 2); s_zm_est[i] = 0; s_zm_sabe[i] = 0; }
    for (int i = s_zm_n - 1; i > 0; i--) { const int j = (int)(azar() % (uint32_t)(i + 1)); const int8_t t = s_zm_par[i]; s_zm_par[i] = s_zm_par[j]; s_zm_par[j] = t; }
    for (int i = 0; i < s_zm_n; i++) {
        lv_obj_t *o = zj_caja(2 + i, ZM_L, ZM_L, 0x4E78C0, 10);
        lv_obj_set_style_bg_grad_dir(o, LV_GRAD_DIR_VER, 0);
        lv_obj_set_style_border_width(o, 3, 0);
        lv_obj_set_style_shadow_width(o, 6, 0); lv_obj_set_style_shadow_opa(o, LV_OPA_40, 0); lv_obj_set_style_shadow_offset_y(o, 2, 0);
        lv_obj_t *ic = icono(o, ZM_ICO[s_zm_par[i] & 7], 4); lv_obj_center(ic);
        lv_obj_t *dorso = icono(o, ICO_HOJA, 2); lv_obj_center(dorso); lv_obj_set_style_opa(dorso, LV_OPA_60, 0);
        float x, y; zm_pos(i, &x, &y);
        zj_centro(o, x, y);
        zm_pintar(i);
    }
    s_zm_turno = (int)(azar() % 2); s_zm_a = s_zm_b2 = -1; s_zm_hasta = 0; s_zm_pensar = s_zj_t + 1200u;
    zj_texto(s_zm_turno ? "EMPIEZA LA VISITA" : "¡EMPIEZAS TÚ! DA VUELTA DOS CARTAS", 2200);
}
static void zm_dar_vuelta(int i)
{
    if (i < 0 || i >= s_zm_n || s_zm_est[i] != 0) return;
    s_zm_est[i] = 1;
    zm_pintar(i);
    accion(UI_ACCION_SONAR, SND_PLOP);
    if (s_zm_a < 0) s_zm_a = i;
    else { s_zm_b2 = i; s_zm_hasta = s_zj_t + 1000u; }
}
static int zm_bajo(int x, int y)
{
    for (int i = 0; i < s_zm_n; i++) { float cx, cy; zm_pos(i, &cx, &cy); if (fabsf((float)x - cx) <= ZM_L / 2 + 3 && fabsf((float)y - cy) <= ZM_L / 2 + 3) return i; }
    return -1;
}
static void zm_toque(int x, int y)
{
    if (s_zj_fase != 1 || s_zm_turno != 0 || s_zm_hasta) return;
    zm_dar_vuelta(zm_bajo(x, y));
}
/* qué carta da vuelta la visita (o ella, en las pruebas): si se acuerda de un par, ese; si no, una que no ha visto */
static int zm_elegir(bool memoria_perfecta)
{
    const float acuerda = memoria_perfecta ? 1.0f : (s_nino ? 0.3f : (s_suave ? 0.5f : 0.7f));
    if (s_zm_a >= 0) { /* la segunda: la pareja de la primera, si la sabe */
        for (int i = 0; i < s_zm_n; i++)
            if (i != s_zm_a && s_zm_est[i] == 0 && s_zm_par[i] == s_zm_par[s_zm_a] && (s_zm_sabe[i] || memoria_perfecta) && azar01() < acuerda) return i;
    } else {
        for (int i = 0; i < s_zm_n; i++)
            for (int j = i + 1; j < s_zm_n; j++)
                if (s_zm_est[i] == 0 && s_zm_est[j] == 0 && s_zm_par[i] == s_zm_par[j] && ((s_zm_sabe[i] && s_zm_sabe[j]) || memoria_perfecta) && azar01() < acuerda) return i;
    }
    int cand[ZM_MAX], nc = 0;
    for (int i = 0; i < s_zm_n; i++) if (s_zm_est[i] == 0 && i != s_zm_a && !s_zm_sabe[i]) cand[nc++] = i;
    if (!nc) for (int i = 0; i < s_zm_n; i++) if (s_zm_est[i] == 0 && i != s_zm_a) cand[nc++] = i;
    return nc ? cand[azar() % (uint32_t)nc] : -1;
}
static void zm_animar(float dt)
{
    (void)dt;
    if (s_zj_fase != 1) return;
    /* las dos arriba: la que juega salta */
    for (int q = 0; q < 2; q++) {
        const bool juega = s_zm_turno == q;
        const float salto = juega ? fabsf(sinf((float)s_zj_t / 180.0f)) * 6.0f : 0.0f;
        zd_poner(q, juega ? 1 : 0, 2, q == 1, CX + (q ? 118.0f : -118.0f), 82.0f - salto);
    }
    if (s_zm_hasta) {
        if ((int32_t)(s_zj_t - s_zm_hasta) < 0) return;
        s_zm_hasta = 0;
        const int a = s_zm_a, b = s_zm_b2;
        s_zm_a = s_zm_b2 = -1;
        if (a < 0 || b < 0) return;
        float x, y; zm_pos(b, &x, &y);
        if (s_zm_par[a] == s_zm_par[b]) {
            s_zm_est[a] = s_zm_est[b] = (int8_t)(2 + s_zm_turno);
            zd_punto(s_zm_turno, x, y);
            if (s_zm_turno == 0) zj_chispas(x, y, 6, 0xFFE27A, 160.0f, 260.0f);
        } else {
            s_zm_est[a] = s_zm_est[b] = 0;
            if (azar01() < (s_nino ? 0.5f : 0.85f)) s_zm_sabe[a] = 1; /* la visita mira y (casi siempre) se acuerda */
            if (azar01() < (s_nino ? 0.5f : 0.85f)) s_zm_sabe[b] = 1;
            s_zm_turno = 1 - s_zm_turno;
        }
        zm_pintar(a); zm_pintar(b);
        int quedan = 0;
        for (int i = 0; i < s_zm_n; i++) if (s_zm_est[i] == 0) quedan++;
        if (!quedan) { s_zd_res = s_zj_pts > s_zd_b ? 2 : (s_zj_pts == s_zd_b ? 1 : 0); s_zd_fin_t = s_zj_t + 600u; s_zj_fase = 3; }
        else s_zm_pensar = s_zj_t + 900u;
        return;
    }
    /* la visita (o ella, en las pruebas) juega sola: piensa un poquito entre carta y carta */
    if ((s_zm_turno == 1 || s_zj_trampa) && (int32_t)(s_zj_t - s_zm_pensar) >= 0) {
        s_zm_pensar = s_zj_t + 750u;
        const int i = zm_elegir(s_zm_turno == 0);
        if (i >= 0) zm_dar_vuelta(i);
    }
}

/* ======================================================================
 * 4) ESCONDIDAS (fondo: el árbol, los arbustos, la roca, el tronco y el pastizal)
 * ==================================================================== */
#define ZE_SITIOS 5
static const int16_t ZE_XY[ZE_SITIOS][2] = {{118, 150}, {356, 168}, {238, 108}, {132, 330}, {338, 334}};
static const art_icono_t ZE_ICO_FALTA[ZE_SITIOS] = {ICO_ARBOLITO, ICO_HIERBA, ICO_PIEDRA, ICO_HOJAS, ICO_HIERBA};
#define ZE_BX 233.0f
#define ZE_BY 262.0f
static int s_ze_paso;     /* 10 la visita se esconde, 11 ella busca, 20 ella elige, 21 ella va, 22 la visita busca, 30 se ve el resultado */
static int s_ze_donde, s_ze_quedan, s_ze_meta, s_ze_vuelta, s_ze_rondas;
static uint8_t s_ze_mirado;
static float s_ze_x[2], s_ze_y[2];
static bool s_ze_oculta[2];
static uint32_t s_ze_t, s_ze_pista;
static int ze_rondas(void) { return s_nino ? 4 : 6; }
static void ze_ronda(void)
{
    s_ze_mirado = 0;
    s_ze_x[0] = ZE_BX - 34; s_ze_y[0] = ZE_BY; s_ze_x[1] = ZE_BX + 34; s_ze_y[1] = ZE_BY;
    s_ze_oculta[0] = s_ze_oculta[1] = false;
    s_ze_meta = -1;
    if (s_ze_vuelta % 2 == 0) { /* se esconde la visita: ella cierra los ojos */
        s_ze_paso = 10;
        s_ze_donde = (int)(azar() % ZE_SITIOS);
        s_ze_quedan = s_nino ? 3 : 2;
        s_ze_t = s_zj_t + 3000u;
        lv_obj_t *n = zj_caja(2, W, W, 0x050810, 0); lv_obj_set_pos(n, 0, 0); lv_obj_set_style_bg_opa(n, 225, 0);
        lv_obj_move_foreground(n);
        zd_poner(0, 3, 3, false, s_ze_x[0], s_ze_y[0]); /* ella, con los ojos cerrados, encima de la oscuridad */
        if (s_zj_o[0]) lv_obj_move_foreground(s_zj_o[0]);
        lv_obj_move_foreground(s_zj_x);
        zj_texto("¡CIERRA LOS OJOS! 1, 2, 3...", 2800);
        accion(UI_ACCION_SONAR, SND_CONTAR);
    } else { /* se esconde ella: tú eliges dónde */
        s_ze_paso = 20;
        s_ze_quedan = s_nino ? 1 : 2;
        zj_texto("¿DÓNDE SE ESCONDE? TOCA UN LUGAR", 3000);
    }
}
static void ze_empezar(void)
{
    if (!art_fondo_dos_hay(3))
        for (int i = 0; i < ZE_SITIOS; i++) { lv_obj_t *o = zj_img(10 + i, art_icono(ZE_ICO_FALTA[i], 5)); zj_centro(o, ZE_XY[i][0], ZE_XY[i][1]); }
    s_ze_vuelta = 0; s_ze_rondas = 0;
    ze_ronda();
}
static int ze_sitio(int x, int y)
{
    int mejor = -1;
    float d0 = 62.0f * 62.0f;
    for (int i = 0; i < ZE_SITIOS; i++) { const float dx = (float)x - ZE_XY[i][0], dy = (float)y - ZE_XY[i][1], d = dx * dx + dy * dy; if (d < d0) { d0 = d; mejor = i; } }
    return mejor;
}
static void ze_toque(int x, int y)
{
    if (s_zj_fase != 1) return;
    const int s = ze_sitio(x, y);
    if (s < 0) return;
    if (s_ze_paso == 11 && s_ze_meta < 0 && !(s_ze_mirado & (1u << s))) { s_ze_meta = s; accion(UI_ACCION_SONAR, SND_TOQUE); }
    else if (s_ze_paso == 20) { s_ze_donde = s; s_ze_meta = s; s_ze_paso = 21; accion(UI_ACCION_SONAR, SND_TOQUE); }
}
/* camina hacia (tx, ty): true al llegar */
static bool ze_caminar(int q, float tx, float ty, float dt)
{
    const float dx = tx - s_ze_x[q], dy = ty - s_ze_y[q], d = sqrtf(dx * dx + dy * dy), p = 250.0f * dt;
    if (d <= p) { s_ze_x[q] = tx; s_ze_y[q] = ty; return true; }
    s_ze_x[q] += dx / d * p; s_ze_y[q] += dy / d * p;
    return false;
}
static void ze_fin_ronda(int gana) /* gana: 0 ella, 1 la visita */
{
    zd_punto(gana, gana ? s_ze_x[1] : s_ze_x[0], (gana ? s_ze_y[1] : s_ze_y[0]) - 60);
    s_ze_paso = 30;
    s_ze_t = s_zj_t + 1700u;
    s_ze_oculta[0] = s_ze_oculta[1] = false;
}
static void ze_aparece_visita(void) { s_ze_oculta[1] = false; s_ze_x[1] = ZE_XY[s_ze_donde][0] - 20; s_ze_y[1] = ZE_XY[s_ze_donde][1] + 30; }
static void ze_animar(float dt)
{
    if (s_zj_fase != 1) return;
    switch (s_ze_paso) {
    case 10: /* la visita corre a esconderse (no se ve: la pantalla está oscura) */
        s_ze_oculta[1] = true;
        s_ze_x[1] = ZE_XY[s_ze_donde][0]; s_ze_y[1] = ZE_XY[s_ze_donde][1] + 30;
        if ((int32_t)(s_zj_t - s_ze_t) >= 0) {
            ocultar(s_zj_o[2], true);
            s_ze_paso = 11; s_ze_t = s_zj_t + 25000u; s_ze_pista = s_zj_t + 1500u;
            zj_texto("¿DÓNDE ESTÁ? TOCA DONDE QUIERAS BUSCAR", 2600);
        }
        break;
    case 11: /* ella busca */
        if ((int32_t)(s_zj_t - s_ze_pista) >= 0) { /* las hojas se mueven donde está escondida */
            s_ze_pista = s_zj_t + (s_nino ? 1500u : 2800u);
            zj_chispas(ZE_XY[s_ze_donde][0], ZE_XY[s_ze_donde][1], 4, 0x6ACB5A, 90.0f, 200.0f);
        }
        if (s_zj_trampa && s_ze_meta < 0) s_ze_meta = s_ze_donde;
        if (s_ze_meta >= 0 && ze_caminar(0, ZE_XY[s_ze_meta][0] + 26, ZE_XY[s_ze_meta][1] + 30, dt)) {
            s_ze_mirado |= (uint8_t)(1u << s_ze_meta);
            if (s_ze_meta == s_ze_donde) { /* ¡la pilló! */
                ze_aparece_visita();
                accion(UI_ACCION_SONAR, SND_ENCONTRADA);
                ze_fin_ronda(0);
            } else {
                zj_chispas(ZE_XY[s_ze_meta][0], ZE_XY[s_ze_meta][1], 5, 0x8FD07A, 140.0f, 300.0f);
                s_ze_quedan--;
                s_ze_meta = -1;
                if (s_ze_quedan <= 0) { ze_aparece_visita(); ze_fin_ronda(1); }
                else zj_texto("¡NO ESTÁ AQUÍ!", 1400);
            }
        } else if (s_ze_meta < 0 && (int32_t)(s_zj_t - s_ze_t) >= 0) { ze_aparece_visita(); ze_fin_ronda(1); }
        break;
    case 20: /* tú eliges dónde se esconde ella; la visita cuenta con los ojos cerrados */
        if (s_zj_trampa) { s_ze_donde = s_ze_meta = (int)(azar() % ZE_SITIOS); s_ze_paso = 21; }
        break;
    case 21: /* ella va y se esconde */
        if (ze_caminar(0, ZE_XY[s_ze_donde][0], ZE_XY[s_ze_donde][1] + 30, dt)) {
            s_ze_oculta[0] = true;
            s_ze_paso = 22; s_ze_meta = -1; s_ze_t = s_zj_t + 1400u;
            zj_texto("¡LISTA! AHORA BUSCA LA VISITA...", 1800);
        }
        break;
    case 22: /* la visita busca */
        if ((int32_t)(s_zj_t - s_ze_t) < 0) break;
        if (s_ze_meta < 0) { /* elige un lugar donde no ha mirado */
            int c[ZE_SITIOS], n = 0;
            for (int i = 0; i < ZE_SITIOS; i++) if (!(s_ze_mirado & (1u << i))) c[n++] = i;
            s_ze_meta = n ? c[azar() % (uint32_t)n] : s_ze_donde;
        }
        if (ze_caminar(1, ZE_XY[s_ze_meta][0] - 26, ZE_XY[s_ze_meta][1] + 30, dt * 0.85f)) {
            s_ze_mirado |= (uint8_t)(1u << s_ze_meta);
            if (s_ze_meta == s_ze_donde) { accion(UI_ACCION_SONAR, SND_ENCONTRADA); ze_fin_ronda(1); }
            else {
                zj_chispas(ZE_XY[s_ze_meta][0], ZE_XY[s_ze_meta][1], 5, 0x8FD07A, 140.0f, 300.0f);
                s_ze_meta = -1; s_ze_t = s_zj_t + 600u;
                if (--s_ze_quedan <= 0) ze_fin_ronda(0);
            }
        }
        break;
    case 30: /* se ve quién ganó la ronda; después, la que sigue */
        if ((int32_t)(s_zj_t - s_ze_t) >= 0) {
            s_ze_rondas++;
            if (s_ze_rondas >= ze_rondas()) { s_zd_res = s_zj_pts > s_zd_b ? 2 : (s_zj_pts == s_zd_b ? 1 : 0); s_zd_fin_t = s_zj_t + 400u; s_zj_fase = 3; }
            else { s_ze_vuelta++; ze_ronda(); }
        }
        break;
    default: break;
    }
    /* las dos */
    for (int q = 0; q < 2; q++) {
        int k = 0;
        if (s_ze_paso == 10 && q == 0) k = 3;
        if ((s_ze_paso == 20 || s_ze_paso == 21) && q == 1) k = 3;
        if ((s_ze_paso == 11 && q == 0) || (s_ze_paso == 22 && q == 1)) k = 2;
        if (s_ze_paso == 30) k = 1;
        const bool espejo = q == 1 ? s_ze_x[1] > s_ze_x[0] - 10 : s_ze_x[0] > s_ze_x[1] + 10;
        if (s_ze_oculta[q]) { if (s_zj_o[q]) ocultar(s_zj_o[q], true); continue; }
        zd_poner(q, k, 3, espejo, s_ze_x[q], s_ze_y[q]);
    }
}

#include "ui_isla.inc" /* (1.0.18) los juegos de la isla: el faro y el buceo */

/* ---------- el final ---------- */
static void zd_terminar(void)
{
    if (zi_es(s_zj_jg)) { zi_terminar(); return; }
    if (s_zj_fase != 3) return;
    s_zj_fase = 2;
    s_zj_gano = true;
    s_zj_fin_t = s_zj_t;
    const int res = s_zd_res < 0 ? 1 : s_zd_res;
    accion(UI_ACCION_JUEGO_DOS, (zd_n() & 15) | (res << 4));
    accion(UI_ACCION_SONAR, res == 2 ? SND_TESORO : (res == 1 ? SND_ALEGRE : SND_SUSPIRO));
    if (res == 2) for (int k = 0; k < 3; k++) zj_chispas(CX + (float)(k - 1) * 90.0f, 170.0f, 6, 0, 240.0f, 380.0f);
    ocultar(s_zj_txt, true);
    for (int i = 2; i < 40; i++) if (s_zj_o[i]) ocultar(s_zj_o[i], true);
    s_zj_fin = caja(s_zj);
    lv_obj_set_size(s_zj_fin, 320, 236);
    lv_obj_align(s_zj_fin, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(s_zj_fin, lv_color_hex(0x1A1430), 0);
    lv_obj_set_style_bg_opa(s_zj_fin, LV_OPA_90, 0);
    lv_obj_set_style_radius(s_zj_fin, 26, 0);
    lv_obj_set_style_border_width(s_zj_fin, 3, 0);
    lv_obj_set_style_border_color(s_zj_fin, lv_color_hex(res == 2 ? 0xFFE9A0 : 0xCFE8FF), 0);
    /* las dos en la tarjeta: la que ganó, con corona; la otra, contenta igual (es un juego con un amigo) */
    if (s_zj_o[0]) ocultar(s_zj_o[0], true);
    if (s_zj_o[1]) ocultar(s_zj_o[1], true);
    for (int q = 0; q < 2; q++) {
        const bool gano = (q == 0 && res == 2) || (q == 1 && res == 0);
        const lv_image_dsc_t *d = zd_criatura(q, gano || res == 1 ? 4 : 0, 3, q == 1);
        if (!d) continue;
        lv_obj_t *im = lv_image_create(s_zj_fin); lv_obj_remove_flag(im, LV_OBJ_FLAG_CLICKABLE);
        lv_image_set_src(im, d);
        lv_obj_align(im, LV_ALIGN_TOP_MID, q ? 70 : -70, s_nino ? 50 : 76);
        if (gano) { lv_obj_t *c = icono(s_zj_fin, ICO_CORONA, 2); lv_obj_align_to(c, im, LV_ALIGN_OUT_TOP_MID, 0, 4); }
    }
    s_zd_puesta[0] = s_zd_puesta[1] = 0;
    if (!s_nino) {
        char b[64], nom[PET_NAME_LEN + 16];
        zd_rival_nombre(nom, sizeof nom);
        if (res == 2) snprintf(b, sizeof b, "%s", gtxt("¡GANASTE!"));
        else if (res == 1) snprintf(b, sizeof b, "%s", gtxt("¡EMPATE!"));
        else snprintf(b, sizeof b, "%s %s", gtxt("GANÓ"), nom);
        art_texto(&s_zj_fin_art[0], b, res == 2 ? 0xFFE27A : 0xCFE8FF, 3, 280);
        lv_obj_t *t = lv_image_create(s_zj_fin); poner_src(t, &s_zj_fin_art[0].dsc); lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 16);
        if (s_zj_jg != ZJ_CUERDA) {
            snprintf(b, sizeof b, "%d - %d", s_zj_pts, s_zd_b);
            art_texto(&s_zj_fin_art[1], b, 0xFFF3C8, 3, 0);
            lv_obj_t *r = lv_image_create(s_zj_fin); poner_src(r, &s_zj_fin_art[1].dsc); lv_obj_align(r, LV_ALIGN_BOTTOM_MID, 0, -20);
        }
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

/* ---------- la portada: las dos frente a frente y cómo se juega ---------- */
static const char *zd_nombre(int jg)
{
    switch (jg) {
    case ZJ_PING: return "PING PONG";
    case ZJ_CUERDA: return "TIRA Y AFLOJA";
    case ZJ_MEMO: return "MEMORICE";
    case ZJ_ESCONDE: return "ESCONDIDAS";
    default: return "";
    }
}
static void zd_portada(void)
{
    if (zi_es(s_zj_jg)) { zi_portada(); return; }
    zj_titulo(zd_nombre(s_zj_jg));
    lv_obj_t *c = caja(s_zj);
    lv_obj_set_size(c, 300, 200);
    lv_obj_align(c, LV_ALIGN_CENTER, 0, 6);
    lv_obj_set_style_bg_color(c, lv_color_hex(0x1A1430), 0);
    lv_obj_set_style_bg_opa(c, LV_OPA_80, 0);
    lv_obj_set_style_radius(c, 24, 0);
    for (int q = 0; q < 2; q++) {
        const lv_image_dsc_t *d = zd_criatura(q, 1, 3, q == 1);
        if (!d) continue;
        lv_obj_t *im = lv_image_create(c); lv_obj_remove_flag(im, LV_OBJ_FLAG_CLICKABLE);
        lv_image_set_src(im, d);
        lv_obj_align(im, LV_ALIGN_TOP_MID, q ? 72 : -72, 12);
    }
    lv_obj_t *vs = texto_c(c, "VS", 0xFFE27A, 3, 80); lv_obj_align(vs, LV_ALIGN_TOP_MID, 0, 40);
    if (!s_nino) {
        static const char *const REGLA[4] = {"EL PRIMERO EN LLEGAR A 5", "TOCA RÁPIDO PARA TIRAR", "DOS CARTAS POR TURNO", "TE ESCONDES Y BUSCAS"};
        lv_obj_t *t = texto_c(c, REGLA[zd_n() & 3], 0xCFE8FF, 2, 280); lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 104);
        lv_obj_t *t2 = texto_c(c, "TOCA PARA EMPEZAR", 0xFFE9A0, 2, 280); lv_obj_align(t2, LV_ALIGN_BOTTOM_MID, 0, -14);
    }
    s_zj_o[45] = c;
}
/* el fondo (de la memoria de recursos; si falta, uno pintado sencillo) */
static void zd_fondo(void)
{
    int px = 0, py = 0;
    const lv_image_dsc_t *f = art_fondo_dos(zd_n(), &px, &py);
    if (f) { poner_src(s_zj_fondo, f); lv_obj_set_pos(s_zj_fondo, px, py); return; }
    lv_obj_set_style_bg_color(s_zj, lv_color_hex(0x3C7A3A), 0);
    lv_obj_t *o = NULL;
    switch (s_zj_jg) {
    case ZJ_PING:
        o = caja(s_zj); lv_obj_set_size(o, 274, 338); lv_obj_set_pos(o, 96, 66); lv_obj_set_style_bg_color(o, lv_color_hex(0xC8A070), 0); lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(o, 3, 0); lv_obj_set_style_border_color(o, lv_color_hex(0xF4ECDC), 0);
        o = caja(s_zj); lv_obj_set_size(o, 286, 4); lv_obj_set_pos(o, 90, 233); lv_obj_set_style_bg_color(o, lv_color_hex(0xF4ECDC), 0); lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
        break;
    case ZJ_CUERDA:
        o = caja(s_zj); lv_obj_set_size(o, 100, 60); lv_obj_set_pos(o, CX - 50, 222); lv_obj_set_style_radius(o, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(o, lv_color_hex(0x6B4A2A), 0); lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
        for (int k = 0; k < 2; k++) { o = caja(s_zj); lv_obj_set_size(o, 4, 40); lv_obj_set_pos(o, k ? 288 : 174, 270); lv_obj_set_style_bg_color(o, lv_color_hex(k ? 0x5A9BD8 : 0xFFC83C), 0); lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0); }
        break;
    case ZJ_MEMO:
        o = caja(s_zj); lv_obj_set_size(o, 334, 306); lv_obj_set_pos(o, 66, 86); lv_obj_set_style_bg_color(o, lv_color_hex(0xDCE8F8), 0); lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
        break;
    case ZJ_FARO: /* (sin el fondo: el mar de noche, el faro y las rocas, sencillos) */
        lv_obj_set_style_bg_color(s_zj, lv_color_hex(0x0E2546), 0);
        o = caja(s_zj); lv_obj_set_size(o, 24, 160); lv_obj_set_pos(o, 84, 170); lv_obj_set_style_bg_color(o, lv_color_hex(0xCFC3AC), 0); lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
        for (int k = 0; k < 5; k++) { o = caja(s_zj); const int r = ZF_ROCA[k][2]; lv_obj_set_size(o, r * 2, r * 2); lv_obj_set_pos(o, ZF_ROCA[k][0] - r, ZF_ROCA[k][1] - r); lv_obj_set_style_radius(o, LV_RADIUS_CIRCLE, 0); lv_obj_set_style_bg_color(o, lv_color_hex(0x444A60), 0); lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0); }
        o = caja(s_zj); lv_obj_set_size(o, 140, 14); lv_obj_set_pos(o, 330, 350); lv_obj_set_style_bg_color(o, lv_color_hex(0x6E4A2A), 0); lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
        break;
    case ZJ_BUCEO:
        lv_obj_set_style_bg_color(s_zj, lv_color_hex(0x1E6AA0), 0);
        o = caja(s_zj); lv_obj_set_size(o, W, 100); lv_obj_set_pos(o, 0, 376); lv_obj_set_style_bg_color(o, lv_color_hex(0xD8C08A), 0); lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
        break;
    default: break;
    }
}
static void zd_empezar(void)
{
    s_zd_b = 0; s_zd_vis = -1; s_zd_fin_t = 0; s_zd_res = -1;
    switch (s_zj_jg) {
    case ZJ_PING: zpp_empezar(); break;
    case ZJ_CUERDA: zc_empezar(); break;
    case ZJ_MEMO: zm_empezar(); break;
    case ZJ_ESCONDE: ze_empezar(); break;
    default: if (zi_es(s_zj_jg)) zi_empezar(); break;
    }
}
static void zd_toque(int x, int y)
{
    switch (s_zj_jg) {
    case ZJ_CUERDA: zc_toque(); break;
    case ZJ_MEMO: zm_toque(x, y); break;
    case ZJ_ESCONDE: ze_toque(x, y); break;
    default: break; /* (el ping pong sigue el dedo en cada cuadro) */
    }
}
static void zd_animar(float dt)
{
    if (s_zj_fase == 3) { /* la última jugada se ve un momento; después, la tarjeta */
        if (s_zj_jg == ZJ_CUERDA) { s_zj_fase = 1; zc_animar(dt); s_zj_fase = 3; }
        if ((int32_t)(s_zj_t - s_zd_fin_t) >= 0) zd_terminar();
        return;
    }
    switch (s_zj_jg) {
    case ZJ_PING: zpp_animar(dt); break;
    case ZJ_CUERDA: zc_animar(dt); break;
    case ZJ_MEMO: zm_animar(dt); break;
    case ZJ_ESCONDE: ze_animar(dt); break;
    default: if (zi_es(s_zj_jg)) zi_animar(dt); break;
    }
    if (s_zj_fase == 1) zd_marcador();
    if (s_zj_fase == 1 && zj_tiempo() > 240000) { s_zd_res = s_zj_pts > s_zd_b ? 2 : (s_zj_pts == s_zd_b ? 1 : 0); s_zd_fin_t = s_zj_t; s_zj_fase = 3; } /* (por si nadie juega) */
}
static void zd_liberar(void)
{
    art_img_free(&s_zd_art[0]); art_img_free(&s_zd_art[1]);
    s_zd_key[0] = s_zd_key[1] = s_zd_puesta[0] = s_zd_puesta[1] = 0;
}
