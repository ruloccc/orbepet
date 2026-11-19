/* ======================================================================
 * (1.0.18) Los juegos de la isla (Jugar → EL MAR, solo en la isla). Usan el marco de ui_zjuegos.inc y las piezas de los
 * juegos de a dos (ui_dos.inc: zd_criatura, zd_poner, la portada, el fondo de la memoria de recursos: pj4 y pj5).
 *   EL FARO (de noche): la isla misma, de noche, vista desde arriba (su faro, su muelle, sus rocas). Mueves la luz del
 *       faro con el dedo; los barquitos que quedan dentro del haz navegan hacia donde apuntas. Hay que llevarlos al anillo
 *       del muelle rodeando la isla, sin que encallen en la orilla o en las rocas. Los que quedan a oscuras se van con la
 *       corriente hacia la isla. 3 corazones; dura 90 s.
 *   BUCEO CON MAREA: la llevas con el dedo bajo el agua a buscar perlas, conchas y caracoles. Las medusas pican (quitan
 *       aire); las burbujas grandes dan aire. Cuando se acaba el aire, sube. Marea nada con ella si ya la conocen.
 * Al final: cuántos barquitos / tesoritos, y si el mar le trajo una concha para su colección (pet_island_game).
 * Incluido desde ui_dos.inc (al final).
 * ==================================================================== */
static int s_zi_vidas, s_zi_perdidos;
static uint32_t s_zi_triste; /* (el faro) cuándo se perdió el último barquito: ella se pone triste un ratito */
static bool s_zi_falta_concha; /* (al empezar) le falta alguna concha de la colección: el final lo puede contar */
static int zi_meta(void) { return s_zj_jg == ZJ_FARO ? 4 : 6; } /* desde cuántos puntos el mar trae una concha (igual que el motor) */

/* el marcador de la isla: el ícono del juego, los puntos y (en el faro) los corazones */
static int s_zi_vis = -1;
static void zi_marcador(void)
{
    if (!s_zj_hud || s_zj_fase != 1) return;
    const int clave = s_zj_pts * 10 + s_zi_vidas;
    if (clave == s_zi_vis) return;
    s_zi_vis = clave;
    char b[16];
    snprintf(b, sizeof b, "%d", s_zj_pts);
    art_texto(&s_zj_hud_art, b, 0xFFE27A, 3, 0);
    poner_src(s_zj_hud_img, &s_zj_hud_art.dsc);
    lv_obj_update_layout(s_zj_hud);
    lv_obj_align(s_zj_hud, LV_ALIGN_TOP_MID, 12, 22);
    ocultar(s_zj_hud, false);
    lv_obj_t *ic = zj_img(38, art_icono(s_zj_jg == ZJ_FARO ? ICO_BOTE : ICO_CONCHA, 2));
    lv_obj_align_to(ic, s_zj_hud, LV_ALIGN_OUT_LEFT_MID, -6, 0);
    if (s_zj_jg == ZJ_FARO)
        for (int k = 0; k < 3; k++) {
            lv_obj_t *c = zj_img(30 + k, art_icono(ICO_CORAZON, 2));
            lv_obj_set_pos(c, CX - 38 + k * 26, 58);
            lv_obj_set_style_opa(c, k < s_zi_vidas ? LV_OPA_COVER : LV_OPA_20, 0);
        }
}

/* ======================================================================
 * EL FARO (fondo pj4: su propia pantalla DE NOCHE; la lámpara, el muelle y el mapa de agua salen de herramientas/hacer_fondos_juegos.py)
 * ==================================================================== */
/* (1.0.24) pantalla propia del faro: el faro de la isla a la derecha, en la costa con su muelle; a la izquierda, mar abierto */
#define ZF_LX 391.0f   /* la lámpara del faro (FARO2_LAMPARA en hacer_fondos_juegos.py) */
#define ZF_LY 150.0f
#define ZF_MX 206.0f   /* la punta del muelle, con el salvavidas (FARO2_MUELLE) */
#define ZF_MY 330.0f
#define ZF_CX 360.0f   /* a oscuras, la corriente lleva a los barquitos hacia la costa (y encallan) */
#define ZF_CY 260.0f
#define ZF_MR 40.0f    /* qué tan cerca de la punta cuenta como «llegó» */
#define ZF_BOTES 3
#define ZF_DURA 90000u
#include "isla_mapa.inc" /* (generado) dónde hay agua: los barquitos chocan con la orilla y las rocas */
static bool zf_agua(float x, float y)
{
    const float dx = x - CX, dy = y - CX;
    if (dx * dx + dy * dy > 224.0f * 224.0f) return true; /* (afuera del círculo: se maneja aparte) */
    const int i = (int)x / ISLA_MAPA_C, j = (int)y / ISLA_MAPA_C;
    if (i < 0 || j < 0 || i >= ISLA_MAPA_N || j >= ISLA_MAPA_N) return true;
    return (ISLA_AGUA[j] >> i) & 1u;
}
static float s_zf_x[ZF_BOTES], s_zf_y[ZF_BOTES], s_zf_fase[ZF_BOTES], s_zf_dir[ZF_BOTES];
static int8_t s_zf_est[ZF_BOTES];     /* 0 nada, 1 navegando, 2 se hunde, 3 llegó */
static uint32_t s_zf_t[ZF_BOTES], s_zf_sale;
static float s_zf_ax = 233.0f, s_zf_ay = 233.0f; /* hacia dónde apunta la luz */
/* (1.0.26) ¿está en la dirección del haz? media = medio ancho en radianes; alcance: además no más allá de la punta (+60 px) */
static bool zf_rayo(float x, float y, float media, bool alcance)
{
    const float bx = x - ZF_LX, by = y - ZF_LY, ax = s_zf_ax - ZF_LX, ay = s_zf_ay - ZF_LY;
    const float db = sqrtf(bx * bx + by * by), da = sqrtf(ax * ax + ay * ay);
    if (db < 1 || da < 1 || (alcance && db > da + 60.0f)) return false;
    const float c = (bx * ax + by * ay) / (db * da);
    return c > cosf(media);
}
static bool zf_iluminado(float x, float y) { return zf_rayo(x, y, s_nino ? 0.32f : 0.24f, true); }
/* (1.0.26) un barquito que alumbraste queda «guiado»: sigue a la luz aunque la acortes (la traigas de vuelta) o la muevas
 * un poco de lado. Antes, al retraer la luz quedaba fuera del alcance y se quedaba a oscuras: no la seguía. Se suelta solo si
 * pasa 1,2 s fuera de un haz más ancho (sin límite de largo). */
static uint32_t s_zf_guia[ZF_BOTES];
static void zf_empezar(void)
{
    for (int i = 0; i < ZF_BOTES; i++) { s_zf_est[i] = 0; s_zf_guia[i] = 0; }
    s_zf_sale = s_zj_t + 2500u; /* (el primero, cuando ya se leyó cómo se juega) */
    s_zf_ax = 130.0f; s_zf_ay = 220.0f;
    s_zi_vidas = 3; s_zi_perdidos = 0; s_zi_triste = 0;
    for (int k = 0; k < 8; k++) { /* el haz: círculos de luz cada vez más grandes */
        lv_obj_t *o = zj_caja(20 + k, 10, 10, 0xFFF0B0, LV_RADIUS_CIRCLE);
        lv_obj_set_style_bg_opa(o, (lv_opa_t)(46 - k * 3), 0);
    }
    lv_obj_t *l = zj_caja(28, 30, 30, 0xFFE9A0, LV_RADIUS_CIRCLE); lv_obj_set_style_bg_opa(l, LV_OPA_50, 0);
    zj_centro(l, ZF_LX, ZF_LY);
    lv_obj_t *m = zj_caja(29, 76, 76, 0xFFE27A, LV_RADIUS_CIRCLE); /* la meta: un anillo que late en la punta del muelle */
    lv_obj_set_style_bg_opa(m, LV_OPA_TRANSP, 0); lv_obj_set_style_border_width(m, 3, 0); lv_obj_set_style_border_color(m, lv_color_hex(0xFFE27A), 0);
    zj_centro(m, ZF_MX, ZF_MY);
    zj_texto("ARRASTRA EL DEDO Y ALUMBRA LOS BARQUITOS: TE SIGUEN A DONDE LLEVES LA LUZ. ¡LLÉVALOS AL MUELLE!", 4500);
    zd_mano(ICO_MANO_DESLIZA, 130.0f, 220.0f, 5000u);
}
static void zf_dibujar_haz(void)
{
    const float dx = s_zf_ax - ZF_LX, dy = s_zf_ay - ZF_LY, d = sqrtf(dx * dx + dy * dy) + 40.0f;
    const float ux = dx / (d - 30.0f > 1 ? d - 30.0f : 1), uy = dy / (d - 30.0f > 1 ? d - 30.0f : 1);
    for (int k = 0; k < 8; k++) {
        lv_obj_t *o = s_zj_o[20 + k];
        if (!o) continue;
        const float t = 0.14f + 0.86f * (float)k / 7.0f, r = 6.0f + t * d * (s_nino ? 0.30f : 0.22f);
        lv_obj_set_size(o, (int)(r * 2), (int)(r * 2));
        lv_obj_set_pos(o, (int)(ZF_LX + ux * t * d - r), (int)(ZF_LY + uy * t * d - r));
    }
    if (s_zj_o[28]) lv_obj_set_style_bg_opa(s_zj_o[28], (lv_opa_t)(110 + 30 * sinf((float)s_zj_t / 300.0f)), 0);
}
static void zf_perder(int i, bool choque)
{
    s_zf_est[i] = 2; s_zf_t[i] = s_zj_t;
    s_zi_vidas--; s_zi_perdidos++; s_zi_triste = s_zj_t | 1u;
    zj_chispas(s_zf_x[i], s_zf_y[i], 10, 0xE8F4FF, 160.0f, 300.0f);
    accion(UI_ACCION_SONAR, choque ? SND_CHAPOTEO : SND_SUSPIRO);
    zj_texto(choque ? "¡ENCALLÓ! HAY QUE ALUMBRARLO ANTES" : "SE PERDIÓ EN LA OSCURIDAD...", 1400);
}
static void zf_animar(float dt)
{
    if (s_zj_fase != 1) return;
    /* hacia dónde apunta la luz: el dedo (o, en las pruebas, sola) */
    if (s_zj_dedo && !s_zj_trampa) { s_zf_ax = s_zj_dedo_x; s_zf_ay = s_zj_dedo_y; zd_mano_fuera(); }
    /* llega un barquito nuevo por el horizonte */
    const bool queda_tiempo = zj_tiempo() < ZF_DURA - 12000u;
    if (queda_tiempo && (int32_t)(s_zj_t - s_zf_sale) >= 0) {
        for (int i = 0; i < ZF_BOTES; i++)
            if (s_zf_est[i] == 0) {
                s_zf_est[i] = 1; s_zf_fase[i] = azar01() * 6.28f; s_zf_dir[i] = 0; s_zf_guia[i] = 0;
                for (int k = 0; k < 30; k++) { /* aparece en el mar abierto, lejos del muelle (a la izquierda, arriba o abajo) */
                    const float a = 3.14159f - 1.75f + azar01() * 3.3f; /* (1.0.24: de arriba a abajo pasando por la izquierda) */
                    s_zf_x[i] = CX + cosf(a) * 200.0f; s_zf_y[i] = CX + sinf(a) * 200.0f;
                    bool libre = true; /* agua alrededor (no recién al lado de una roca) */
                    for (int q = 0; q < 9 && libre; q++) libre = zf_agua(s_zf_x[i] + (float)(q % 3 - 1) * 20.0f, s_zf_y[i] + (float)(q / 3 - 1) * 20.0f);
                    if (libre) break;
                }
                lv_obj_t *o = zj_img(2 + i, art_icono(ICO_BOTE, 3)); zj_tinte(o, 0x000000, 0); lv_obj_set_style_opa(o, LV_OPA_COVER, 0);
                accion(UI_ACCION_SONAR, SND_TOQUE);
                break;
            }
        const float apuro = zj_tiempo() / (float)ZF_DURA;
        s_zf_sale = s_zj_t + (uint32_t)((s_nino ? 6500.0f : 5200.0f) - 2200.0f * apuro) + azar() % 1200u;
    }
    for (int i = 0; i < ZF_BOTES; i++) {
        lv_obj_t *o = s_zj_o[2 + i];
        if (!o) continue;
        if (s_zf_est[i] == 0) { ocultar(o, true); continue; }
        if (s_zf_est[i] == 2 || s_zf_est[i] == 3) { /* se hunde, o se amarra en el muelle y se desvanece */
            const float q = (float)(s_zj_t - s_zf_t[i]) / 900.0f;
            if (q >= 1.0f) { s_zf_est[i] = 0; ocultar(o, true); continue; }
            if (s_zf_est[i] == 2) { s_zf_y[i] += 18.0f * dt; zj_tinte(o, 0x0A1E3C, (lv_opa_t)(q * 220)); }
            lv_obj_set_style_opa(o, (lv_opa_t)(255 * (1.0f - q)), 0);
            zj_centro(o, s_zf_x[i], s_zf_y[i]);
            continue;
        }
        float vx, vy;
        bool guia = false;
        if (!s_zj_trampa) { /* (1.0.26) alumbrado una vez, sigue a la luz */
            const bool antes = s_zf_guia[i] != 0;
            if (zf_iluminado(s_zf_x[i], s_zf_y[i]) || (antes && zf_rayo(s_zf_x[i], s_zf_y[i], s_nino ? 0.60f : 0.48f, false))) s_zf_guia[i] = s_zj_t | 1u;
            guia = s_zf_guia[i] && s_zj_t - s_zf_guia[i] < 1200u;
            if (!guia) s_zf_guia[i] = 0;
            else if (!antes) zj_chispas(s_zf_x[i], s_zf_y[i] - 6.0f, 5, 0xFFF0B0, 90.0f, 260.0f); /* ¡te vio! */
        }
        if (s_zj_trampa) { /* (pruebas) va al muelle rodeando la isla: si adelante hay tierra, gira (siempre hacia el mismo lado) */
            const float base = atan2f(ZF_MY - s_zf_y[i], ZF_MX - s_zf_x[i]);
            if (s_zf_dir[i] == 0) s_zf_dir[i] = s_zf_y[i] < CX ? 1.0f : -1.0f; /* por arriba o por abajo de la isla (girando hacia el oeste) */
            float a = base, vel = 60.0f;
            int k = 0;
            for (; k < 28; k++) {
                a = base + s_zf_dir[i] * 0.22f * (float)k;
                bool libre = true;
                for (int q = 3; q <= 30 && libre; q += 3) /* (cada 3 px: si no, corta las esquinas de la orilla) */
                    libre = zf_agua(s_zf_x[i] + cosf(a) * (float)q, s_zf_y[i] + sinf(a) * (float)q) &&
                            zf_agua(s_zf_x[i] + cosf(a) * (float)q - sinf(a) * 6.0f, s_zf_y[i] + sinf(a) * (float)q + cosf(a) * 6.0f) &&
                            zf_agua(s_zf_x[i] + cosf(a) * (float)q + sinf(a) * 6.0f, s_zf_y[i] + sinf(a) * (float)q - cosf(a) * 6.0f);
                if (libre) break;
            }
            if (k == 28) vel = 0;
            vx = cosf(a) * vel; vy = sinf(a) * vel;
        } else if (guia) { /* guiado por la luz: navega hacia donde apunta (y frena suave al llegar a la punta) */
            vx = s_zf_ax - s_zf_x[i]; vy = s_zf_ay - s_zf_y[i];
            const float d = sqrtf(vx * vx + vy * vy), vmax = s_nino ? 50.0f : 58.0f;
            const float v = d < 8.0f ? 0.0f : (d * 1.6f + 12.0f < vmax ? d * 1.6f + 12.0f : vmax);
            vx = d > 0.1f ? vx / d * v : 0; vy = d > 0.1f ? vy / d * v : 0;
        } else { /* a oscuras: la corriente lo lleva despacito hacia la isla (si nadie lo guía, encalla) */
            const float dx0 = ZF_CX - s_zf_x[i], dy0 = ZF_CY - s_zf_y[i], d0 = sqrtf(dx0 * dx0 + dy0 * dy0) + 0.01f;
            const float v = s_nino ? 4.5f : 6.5f;
            vx = dx0 / d0 * v + 5.0f * sinf((float)s_zj_t / 1300.0f + s_zf_fase[i]);
            vy = dy0 / d0 * v;
        }
        s_zf_x[i] += vx * dt; s_zf_y[i] += vy * dt;
        const bool luz = s_zj_trampa || guia;
        zj_tinte(o, 0x0A1430, luz ? 0 : 120);
        /* el balanceo de las olas */
        zj_centro(o, s_zf_x[i], s_zf_y[i] + 2.0f * sinf((float)s_zj_t / 260.0f + s_zf_fase[i]));
        /* ¿llegó al muelle? ¿chocó? ¿se perdió? */
        const float mx = s_zf_x[i] - ZF_MX, my = s_zf_y[i] - ZF_MY;
        if (mx * mx + my * my < ZF_MR * ZF_MR) {
            s_zf_est[i] = 3; s_zf_t[i] = s_zj_t;
            s_zj_pts++;
            zj_mas(1, ZF_MX, ZF_MY - 30.0f, 0xFFF3B0);
            zj_chispas(ZF_MX, ZF_MY - 10.0f, 10, 0xFFE27A, 200.0f, 300.0f);
            accion(UI_ACCION_SONAR, SND_ALEGRE);
            continue;
        }
        if (!zf_agua(s_zf_x[i], s_zf_y[i])) { zf_perder(i, true); continue; }
        const float cx = s_zf_x[i] - CX, cy = s_zf_y[i] - CX;
        if (cx * cx + cy * cy > 226.0f * 226.0f) zf_perder(i, false);
    }
    zf_dibujar_haz();
    /* ella, en el balcón del faro, mirando el mar */
    if (s_zj_o[29]) { const int r = 76 + (int)(6.0f * sinf((float)s_zj_t / 250.0f)); lv_obj_set_size(s_zj_o[29], r, r); zj_centro(s_zj_o[29], ZF_MX, ZF_MY); }
    zd_poner(0, s_zi_triste && s_zj_t - s_zi_triste < 1400u ? 5 : 1, 2, true, 431.0f, 190.0f); /* ella, en el balcón del faro */
    bool quedan = false;
    for (int i = 0; i < ZF_BOTES; i++) if (s_zf_est[i] == 1 || s_zf_est[i] == 2 || s_zf_est[i] == 3) quedan = true;
    if (s_zi_vidas <= 0 || (zj_tiempo() >= ZF_DURA - 12000u && !quedan) || zj_tiempo() >= ZF_DURA + 20000u) {
        s_zd_fin_t = s_zj_t + 900u; s_zj_fase = 3;
    }
}

/* ======================================================================
 * BUCEO CON MAREA (fondo pj5: la superficie arriba (y ~ 50), la arena abajo (y ~ 372))
 * ==================================================================== */
#define ZB_COSAS 6
#define ZB_MEDUSAS 3
static float s_zb_x, s_zb_y, s_zb_vx, s_zb_aire, s_zb_mx, s_zb_my;
static float s_zb_cx[ZB_COSAS], s_zb_cy[ZB_COSAS];
static int8_t s_zb_cq[ZB_COSAS];             /* -1 nada, 0 perla, 1 concha, 2 caracol, 3 perla dorada, 4 burbuja de aire */
static float s_zb_jx[ZB_MEDUSAS], s_zb_jy[ZB_MEDUSAS], s_zb_jv[ZB_MEDUSAS], s_zb_jf[ZB_MEDUSAS];
static uint32_t s_zb_pica, s_zb_otra, s_zb_burbuja;
static bool s_zb_marea, s_zb_sube;
static bool zb_dentro(float x, float y) { const float dx = x - CX, dy = y - CX; return dx * dx + dy * dy < 196.0f * 196.0f; }
static void zb_poner_cosa(int i, int q)
{
    s_zb_cq[i] = (int8_t)q;
    for (int k = 0; k < 20; k++) {
        s_zb_cx[i] = 90.0f + azar01() * 286.0f;
        s_zb_cy[i] = q == 4 ? 400.0f : (q == 0 || q == 3 ? 130.0f + azar01() * 240.0f : 360.0f + azar01() * 36.0f);
        if (zb_dentro(s_zb_cx[i], s_zb_cy[i]) || q == 4) break;
    }
    lv_obj_t *o;
    if (q == 0 || q == 3) {
        o = zj_caja(2 + i, q == 3 ? 16 : 13, q == 3 ? 16 : 13, q == 3 ? 0xFFD84A : 0xF4F4FF, LV_RADIUS_CIRCLE);
        lv_obj_set_style_border_width(o, 2, 0);
        lv_obj_set_style_border_color(o, lv_color_hex(q == 3 ? 0xC89A2A : 0xB8C4E0), 0);
        lv_obj_set_style_shadow_width(o, 10, 0); lv_obj_set_style_shadow_color(o, lv_color_hex(q == 3 ? 0xFFE27A : 0xFFFFFF), 0);
        lv_obj_set_style_shadow_opa(o, LV_OPA_60, 0);
    } else if (q == 4) {
        o = zj_img(2 + i, art_icono(ICO_BURBUJA, 3));
    } else {
        o = zj_img(2 + i, art_icono(q == 1 ? ICO_CONCHA : ICO_CARACOL, 3));
    }
    lv_obj_set_style_opa(o, LV_OPA_COVER, 0);
    zj_centro(o, s_zb_cx[i], s_zb_cy[i]);
}
static void zb_empezar(void)
{
    s_zb_x = CX; s_zb_y = 90.0f; s_zb_vx = 0; s_zb_aire = 1.0f; s_zb_pica = 0; s_zb_sube = false;
    s_zb_mx = CX + 40.0f; s_zb_my = 70.0f;
    s_zb_marea = s_pet && ((s_pet->criaturas_vistas >> CRI_MAREA) & 1u);
    for (int i = 0; i < ZB_COSAS; i++) zb_poner_cosa(i, i == 0 ? 3 * (azar() % 4 == 0) : (int)(azar() % 3));
    for (int j = 0; j < ZB_MEDUSAS; j++) { /* la medusa: una campana rosada traslúcida con tres patitas */
        s_zb_jx[j] = 120.0f + (float)j * 110.0f; s_zb_jy[j] = 170.0f + (float)(j % 2) * 110.0f;
        s_zb_jv[j] = (j % 2 ? -1.0f : 1.0f) * (s_nino ? 18.0f : 26.0f); s_zb_jf[j] = azar01() * 6.28f;
        lv_obj_t *m = zj_caja(10 + j, 30, 22, 0xF29ADC, 15);
        lv_obj_set_style_bg_opa(m, 190, 0);
        lv_obj_set_style_radius(m, 14, 0);
        lv_obj_set_style_border_width(m, 2, 0); lv_obj_set_style_border_color(m, lv_color_hex(0xFFD0F4), 0);
        lv_obj_set_style_shadow_width(m, 12, 0); lv_obj_set_style_shadow_color(m, lv_color_hex(0xF29ADC), 0); lv_obj_set_style_shadow_opa(m, LV_OPA_50, 0);
        lv_obj_set_style_clip_corner(m, false, 0);
        lv_obj_remove_flag(m, LV_OBJ_FLAG_SCROLLABLE);
        for (int k = 0; k < 3; k++) {
            lv_obj_t *p = zj_caja(13 + j * 3 + k, 3, 14, 0xF7B8E8, 2);
            lv_obj_set_style_bg_opa(p, 170, 0);
        }
    }
    /* la barra del aire (arriba, debajo del marcador) */
    lv_obj_t *b = zj_caja(24, 120, 12, 0x0A1E3C, 6); lv_obj_set_style_bg_opa(b, LV_OPA_70, 0);
    lv_obj_set_style_border_width(b, 2, 0); lv_obj_set_style_border_color(b, lv_color_hex(0xE0F8FF), 0);
    lv_obj_set_pos(b, CX - 60, 58);
    lv_obj_t *f = zj_caja(25, 112, 6, 0x7AE0FF, 3); lv_obj_set_pos(f, CX - 56, 61);
    lv_obj_t *ic = zj_img(26, art_icono(ICO_BURBUJA, 2)); lv_obj_set_pos(ic, CX - 88, 52);
    if (s_zb_marea) zj_img(27, art_icono_espejo(ICO_MAREA, 3));
    s_zb_otra = s_zj_t + 2500u; s_zb_burbuja = s_zj_t + 9000u;
    zj_texto(s_zb_marea ? "ARRASTRA EL DEDO PARA LLEVARLA. JUNTA PERLAS Y CONCHAS. ¡OJO CON LAS MEDUSAS! MAREA TE AYUDA" : "ARRASTRA EL DEDO PARA LLEVARLA. JUNTA PERLAS Y CONCHAS. ¡OJO CON LAS MEDUSAS!", 4200);
    zd_mano(ICO_MANO_DESLIZA, CX, 200.0f, 5000u);
}
static void zb_animar(float dt)
{
    if (s_zj_fase != 1) return;
    /* el aire se va acabando */
    if (!s_zb_sube) {
        s_zb_aire -= dt / (s_nino ? 70.0f : 50.0f) * (s_zj_trampa ? 3.0f : 1.0f); /* (en las pruebas se acaba antes) */
        if (s_zb_aire <= 0) { s_zb_aire = 0; s_zb_sube = true; zj_texto("¡SE ACABÓ EL AIRE! A LA SUPERFICIE", 1600); accion(UI_ACCION_SONAR, SND_PLOP); }
    }
    if (s_zj_o[25]) { lv_obj_set_width(s_zj_o[25], (int)(4 + 108 * s_zb_aire)); lv_obj_set_style_bg_color(s_zj_o[25], lv_color_hex(s_zb_aire < 0.25f ? 0xFF7A6A : 0x7AE0FF), 0); }
    /* ella nada hacia el dedo (o hacia la cosa más cerca, en las pruebas) */
    float tx = s_zb_x, ty = s_zb_y;
    if (s_zb_sube) { tx = s_zb_x; ty = 30.0f; }
    else if (s_zj_trampa) {
        float best = 1e9f;
        for (int i = 0; i < ZB_COSAS; i++) if (s_zb_cq[i] >= 0) { const float dx = s_zb_cx[i] - s_zb_x, dy = s_zb_cy[i] - s_zb_y, d = dx * dx + dy * dy; if (d < best) { best = d; tx = s_zb_cx[i]; ty = s_zb_cy[i]; } }
    } else if (s_zj_dedo) { tx = s_zj_dedo_x; ty = s_zj_dedo_y; zd_mano_fuera(); }
    {
        const float dx = tx - s_zb_x, dy = ty - s_zb_y, d = sqrtf(dx * dx + dy * dy), v = (s_zb_sube ? 120.0f : 150.0f) * dt;
        if (d > v) { s_zb_x += dx / d * v; s_zb_y += dy / d * v; s_zb_vx = dx; } else if (!s_zb_sube) { s_zb_x = tx; s_zb_y = ty; }
        if (!s_zb_sube) {
            if (s_zb_y < 80.0f) s_zb_y = 80.0f;
            if (s_zb_y > 410.0f) s_zb_y = 410.0f;
            const float cx = s_zb_x - CX, cy = s_zb_y - CX, cd = sqrtf(cx * cx + cy * cy);
            if (cd > 196.0f) { s_zb_x = CX + cx / cd * 196.0f; s_zb_y = CX + cy / cd * 196.0f; }
        }
    }
    const bool picada = s_zb_pica && s_zj_t - s_zb_pica < 900u;
    const float ondula = 3.0f * sinf((float)s_zj_t / 220.0f);
    zd_poner(0, picada ? 5 : (s_zb_sube ? 3 : 1), 3, s_zb_vx < 0, s_zb_x, s_zb_y + 30.0f + ondula);
    /* Marea la sigue de cerca (y a veces le muestra dónde hay algo) */
    if (s_zb_marea && s_zj_o[27]) {
        const float mx = s_zb_x + (s_zb_vx < 0 ? 46.0f : -46.0f), my = s_zb_y - 10.0f;
        s_zb_mx += (mx - s_zb_mx) * (dt * 2.5f > 1 ? 1 : dt * 2.5f);
        s_zb_my += (my - s_zb_my) * (dt * 2.5f > 1 ? 1 : dt * 2.5f);
        poner_src(s_zj_o[27], s_zb_vx < 0 ? art_icono(ICO_MAREA, 3) : art_icono_espejo(ICO_MAREA, 3));
        zj_centro(s_zj_o[27], s_zb_mx, s_zb_my + 3.0f * sinf((float)s_zj_t / 260.0f + 1.0f));
        if ((s_zj_t / 100u) % 40u == 0u) /* cada 4 s: chispitas sobre una perla */
            for (int i = 0; i < ZB_COSAS; i++) if (s_zb_cq[i] == 0 || s_zb_cq[i] == 3) { zj_chispas(s_zb_cx[i], s_zb_cy[i], 3, 0xC8F4FF, 80.0f, 0.0f); break; }
    }
    /* las cosas del fondo: se recogen al pasar */
    for (int i = 0; i < ZB_COSAS; i++) {
        lv_obj_t *o = s_zj_o[2 + i];
        if (s_zb_cq[i] < 0) { if (o) ocultar(o, true); continue; }
        if (s_zb_cq[i] == 4) { /* la burbuja de aire sube */
            s_zb_cy[i] -= 40.0f * dt; s_zb_cx[i] += 10.0f * sinf((float)s_zj_t / 300.0f) * dt;
            if (s_zb_cy[i] < 40.0f) { s_zb_cq[i] = -1; continue; }
        }
        if (o) zj_centro(o, s_zb_cx[i], s_zb_cy[i] + (s_zb_cq[i] == 0 || s_zb_cq[i] == 3 ? 2.0f * sinf((float)s_zj_t / 400.0f + (float)i) : 0));
        const float dx = s_zb_cx[i] - s_zb_x, dy = s_zb_cy[i] - (s_zb_y + 6.0f);
        if (!s_zb_sube && dx * dx + dy * dy < 30.0f * 30.0f) {
            const int q = s_zb_cq[i];
            s_zb_cq[i] = -1;
            if (o) ocultar(o, true);
            if (q == 4) { s_zb_aire = s_zb_aire + 0.25f > 1.0f ? 1.0f : s_zb_aire + 0.25f; zj_chispas(s_zb_cx[i], s_zb_cy[i], 8, 0xE0F8FF, 140.0f, -60.0f); accion(UI_ACCION_SONAR, SND_PLOP); zj_texto("¡AIRE!", 900); }
            else {
                const int n = q == 3 ? 3 : 1;
                s_zj_pts += n;
                zj_mas(n, s_zb_cx[i], s_zb_cy[i] - 20.0f, q == 3 ? 0xFFE27A : 0xFFF3B0);
                zj_chispas(s_zb_cx[i], s_zb_cy[i], q == 3 ? 12 : 6, q == 3 ? 0xFFE27A : 0xFFFFFF, 160.0f, 0.0f);
                accion(UI_ACCION_SONAR, q == 3 ? SND_TESORO : SND_TOQUE);
            }
        }
    }
    /* aparecen cosas nuevas (y de vez en cuando una burbuja grande de aire) */
    if ((int32_t)(s_zj_t - s_zb_otra) >= 0 && !s_zb_sube) {
        s_zb_otra = s_zj_t + 1800u + azar() % 1500u;
        for (int i = 0; i < ZB_COSAS; i++) if (s_zb_cq[i] < 0) { zb_poner_cosa(i, azar() % 9 == 0 ? 3 : (int)(azar() % 3)); break; }
    }
    if ((int32_t)(s_zj_t - s_zb_burbuja) >= 0 && !s_zb_sube) {
        s_zb_burbuja = s_zj_t + 9000u + azar() % 5000u;
        for (int i = 0; i < ZB_COSAS; i++) if (s_zb_cq[i] < 0) { zb_poner_cosa(i, 4); break; }
    }
    /* las medusas: suben y bajan latiendo, van de lado a lado; si la tocan, pica (quita aire) */
    const float dif = zj_tiempo() / 40000.0f > 1.0f ? 1.0f : zj_tiempo() / 40000.0f;
    for (int j = 0; j < ZB_MEDUSAS; j++) {
        s_zb_jx[j] += s_zb_jv[j] * (1.0f + 0.5f * dif) * dt;
        if (s_zb_jx[j] < 90.0f) { s_zb_jx[j] = 90.0f; s_zb_jv[j] = fabsf(s_zb_jv[j]); }
        if (s_zb_jx[j] > 376.0f) { s_zb_jx[j] = 376.0f; s_zb_jv[j] = -fabsf(s_zb_jv[j]); }
        const float late = sinf((float)s_zj_t / 380.0f + s_zb_jf[j]);
        const float y = s_zb_jy[j] + 22.0f * sinf((float)s_zj_t / 1700.0f + s_zb_jf[j]);
        if (s_zj_o[10 + j]) { lv_obj_set_size(s_zj_o[10 + j], (int)(30 + 4 * late), (int)(22 - 3 * late)); zj_centro(s_zj_o[10 + j], s_zb_jx[j], y); }
        for (int k = 0; k < 3; k++)
            if (s_zj_o[13 + j * 3 + k]) zj_centro(s_zj_o[13 + j * 3 + k], s_zb_jx[j] - 9.0f + 9.0f * (float)k + 2.0f * sinf((float)s_zj_t / 200.0f + (float)k), y + 18.0f);
        const float dx = s_zb_jx[j] - s_zb_x, dy = y - (s_zb_y + 6.0f);
        if (!s_zj_trampa && !s_zb_sube && dx * dx + dy * dy < 30.0f * 30.0f && (!s_zb_pica || s_zj_t - s_zb_pica > 1400u)) {
            s_zb_pica = s_zj_t;
            s_zb_aire -= s_nino ? 0.08f : 0.14f;
            zj_chispas(s_zb_x, s_zb_y, 6, 0xF29ADC, 140.0f, 0.0f);
            accion(UI_ACCION_SONAR, SND_QUEJA);
            zj_texto("¡AY! UNA MEDUSA", 900);
        }
    }
    if (s_zb_sube && s_zb_y < 46.0f) { s_zd_fin_t = s_zj_t + 300u; s_zj_fase = 3; }
}

/* ---------- la portada, el final y lo demás ---------- */
static void zi_portada(void)
{
    zj_titulo(s_zj_jg == ZJ_FARO ? "EL FARO" : "BUCEO CON MAREA");
    lv_obj_t *c = caja(s_zj);
    lv_obj_set_size(c, 320, 250);
    lv_obj_align(c, LV_ALIGN_CENTER, 0, 10);
    lv_obj_set_style_bg_color(c, lv_color_hex(0x0E1C38), 0);
    lv_obj_set_style_bg_opa(c, LV_OPA_80, 0);
    lv_obj_set_style_radius(c, 24, 0);
    const lv_image_dsc_t *d = zd_criatura(0, 1, 3, false);
    if (d) { lv_obj_t *im = lv_image_create(c); lv_obj_remove_flag(im, LV_OBJ_FLAG_CLICKABLE); lv_image_set_src(im, d); lv_obj_align(im, LV_ALIGN_TOP_MID, -50, 14); }
    const bool marea = s_pet && ((s_pet->criaturas_vistas >> CRI_MAREA) & 1u);
    lv_obj_t *ic = icono(c, s_zj_jg == ZJ_FARO ? ICO_FARO : (marea ? ICO_MAREA : ICO_CONCHA), 4);
    lv_obj_align(ic, LV_ALIGN_TOP_MID, 50, 18);
    if (!s_nino) {
        lv_obj_t *t = zj_texto_c(c, 0, s_zj_jg == ZJ_FARO ? "MUEVE LA LUZ CON EL DEDO. LOS BARQUITOS ILUMINADOS LA SIGUEN: LLÉVALOS AL ANILLO DEL MUELLE."
                                                    : "LLÉVALA CON EL DEDO A JUNTAR PERLAS Y CONCHAS. LAS MEDUSAS QUITAN AIRE; LAS BURBUJAS DAN AIRE.", 0xCFE8FF, 2, 296);
        lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 92);
        lv_obj_t *t2 = zj_texto_c(c, 1, "TOCA PARA EMPEZAR", 0xFFE9A0, 2, 280); lv_obj_align(t2, LV_ALIGN_BOTTOM_MID, 0, -14);
    }
    s_zj_o[45] = c;
}
static void zi_empezar(void)
{
    s_zi_vis = -1;
    s_zi_falta_concha = false;
    if (s_pet) for (int k = 0; k < 4; k++) if (!((s_pet->coleccion[COL_CONCHAS] >> k) & 1u)) s_zi_falta_concha = true;
    if (s_zj_jg == ZJ_FARO) zf_empezar(); else zb_empezar();
}
static void zi_terminar(void)
{
    if (s_zj_fase != 3) return;
    s_zj_fase = 2;
    s_zj_gano = true;
    s_zj_fin_t = s_zj_t;
    const int pts = s_zj_pts > 127 ? 127 : s_zj_pts;
    const bool concha = s_zi_falta_concha && pts >= zi_meta();
    accion(UI_ACCION_JUEGO_ISLA, (s_zj_jg == ZJ_FARO ? 0 : 1) | (pts << 4));
    accion(UI_ACCION_SONAR, pts >= zi_meta() ? SND_TESORO : SND_ALEGRE);
    if (pts >= zi_meta()) for (int k = 0; k < 3; k++) zj_chispas(CX + (float)(k - 1) * 90.0f, 170.0f, 6, 0, 240.0f, 380.0f);
    ocultar(s_zj_txt, true);
    for (int i = 0; i < 40; i++) if (s_zj_o[i]) ocultar(s_zj_o[i], true);
    s_zj_fin = caja(s_zj);
    lv_obj_set_size(s_zj_fin, 320, 236);
    lv_obj_align(s_zj_fin, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(s_zj_fin, lv_color_hex(0x0E1C38), 0);
    lv_obj_set_style_bg_opa(s_zj_fin, LV_OPA_90, 0);
    lv_obj_set_style_radius(s_zj_fin, 26, 0);
    lv_obj_set_style_border_width(s_zj_fin, 3, 0);
    lv_obj_set_style_border_color(s_zj_fin, lv_color_hex(pts >= zi_meta() ? 0xFFE9A0 : 0xCFE8FF), 0);
    const lv_image_dsc_t *d = zd_criatura(0, 4, 3, false);
    s_zd_puesta[0] = 0;
    if (d) { lv_obj_t *im = lv_image_create(s_zj_fin); lv_obj_remove_flag(im, LV_OBJ_FLAG_CLICKABLE); lv_image_set_src(im, d); lv_obj_align(im, LV_ALIGN_TOP_MID, s_zj_jg == ZJ_BUCEO && s_zb_marea ? -40 : 0, s_nino ? 50 : 70); }
    if (s_zj_jg == ZJ_BUCEO && s_zb_marea) { lv_obj_t *m = icono(s_zj_fin, ICO_MAREA, 4); lv_obj_align(m, LV_ALIGN_TOP_MID, 40, s_nino ? 56 : 76); }
    lv_obj_t *ic = icono(s_zj_fin, s_zj_jg == ZJ_FARO ? ICO_BOTE : ICO_CONCHA, 3);
    if (!s_nino) {
        char b[64];
        if (s_zj_jg == ZJ_FARO) { if (pts == 1) snprintf(b, sizeof b, "¡UN BARQUITO A SALVO!"); else snprintf(b, sizeof b, "¡%d BARQUITOS A SALVO!", pts); }
        else { if (pts == 1) snprintf(b, sizeof b, "¡UN TESORITO!"); else snprintf(b, sizeof b, "¡%d TESORITOS!", pts); }
        if (pts == 0) snprintf(b, sizeof b, "%s", s_zj_jg == ZJ_FARO ? "LA NOCHE ESTABA DIFÍCIL" : "ESTA VEZ NO HUBO SUERTE");
        art_texto(&s_zj_fin_art[0], b, pts >= zi_meta() ? 0xFFE27A : 0xCFE8FF, 3, 290);
        lv_obj_t *t = lv_image_create(s_zj_fin); poner_src(t, &s_zj_fin_art[0].dsc); lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 16);
        if (concha) {
            art_texto(&s_zj_fin_art[1], "¡EL MAR LE TRAJO UNA CONCHA PARA SU COLECCIÓN!", 0xFFF3C8, 2, 280);
            lv_obj_t *r = lv_image_create(s_zj_fin); poner_src(r, &s_zj_fin_art[1].dsc); lv_obj_align(r, LV_ALIGN_BOTTOM_MID, 0, -16);
        }
        lv_obj_align(ic, LV_ALIGN_TOP_MID, 66, s_nino ? 60 : 84);
    } else lv_obj_align(ic, LV_ALIGN_TOP_MID, 0, 14);
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
static void zi_animar(float dt)
{
    if (s_zj_jg == ZJ_FARO) zf_animar(dt); else zb_animar(dt);
    if (s_zj_fase == 1) zi_marcador();
}
