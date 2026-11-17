/* ======================================================================
 * (v39) Las dos ceremonias, cada una en su propia pantalla (incluido desde ui.c, después de ui_musica.inc):
 *  - EL RITO DE LA GUARDIANA: todo se detiene y se funde a blanco (de noche, un crema clarito). Ella se eleva; abajo, las
 *    ocho piedras; del centro sale la mitad del Corazón y en su pecho brilla la otra mitad. Laten, se reconocen y se unen.
 *    Estallido de luciérnagas, las criaturas y los animales que conoce saltan alrededor, Nimba cruza. Después, el recuerdo
 *    dorado (Tilo guarda la mitad del Corazón en un huevo) y la historia del rito, de a una hoja. Todo vuelve, en la isla.
 *  - EL ÚLTIMO VERSO (el segundo final): en la cueva, Prisma propone terminar el verso de Tilo. Los cristales tocan su
 *    verso desafinado, tú tocas nuestra canción encima, Prisma afina la última nota. Cantan todos; Nimba sube la canción;
 *    aparece la cuchara en el cielo y parpadea al ritmo. Por fin, su nombre: Tilo, y la última página de su diario.
 * Sin fondos detrás: el mundo se oculta mientras dura (ahorra memoria y cuadros).
 * ==================================================================== */
enum { CER_NADA = 0, CER_RITO, CER_VERSO };
static int s_cer, s_cer_pag, s_cer_paso;
static uint32_t s_cer_t0, s_cer_toque_desde;
static bool s_cer_repetida;          /* se está volviendo a ver desde el Libro (con X) */
static lv_obj_t *s_cer_ella, *s_cer_ella2, *s_cer_medio[2], *s_cer_cor, *s_cer_obj[16], *s_cer_txt;
static int s_cer_nobj;
#define CER_PART 18
static lv_obj_t *s_cer_part[CER_PART];
static float s_cer_px[CER_PART], s_cer_py[CER_PART], s_cer_vx[CER_PART], s_cer_vy[CER_PART];
static uint32_t s_cer_pt0[CER_PART];
static lv_obj_t *s_cer_velo;         /* el blanco que se funde al volver al mundo */
static uint32_t s_cer_velo_t0;
static void cer_pagina(int pag);
static void cer_terminar(void);

static bool cer_noche(void) { return s_fondo_periodo == PERIODO_NOCHE; }
static uint32_t cer_blanco(void) { return cer_noche() ? 0xEFE3C8 : 0xFFFDF6; }
#define CER_TINTA 0x5A4A7A
#define CER_ORO 0xB8862A
static void cer_mundo(bool ver)
{
    if (ver) { lv_obj_remove_flag(s_fondo, LV_OBJ_FLAG_HIDDEN); lv_obj_remove_flag(s_mundo, LV_OBJ_FLAG_HIDDEN); }
    else { lv_obj_add_flag(s_fondo, LV_OBJ_FLAG_HIDDEN); lv_obj_add_flag(s_mundo, LV_OBJ_FLAG_HIDDEN); }
}
/* la pantalla: un disco liso (blanco o de noche), sin nada detrás */
static void cer_fondo(uint32_t color, uint32_t abajo)
{
    abrir_tarjeta(PANEL_FINAL, 466, 466);
    lv_obj_align(s_tarjeta, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_opa(s_tarjeta, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_tarjeta, 0, 0);
    lv_obj_set_style_shadow_width(s_tarjeta, 0, 0);
    lv_obj_t *f = lv_obj_create(s_tarjeta);
    lv_obj_remove_style_all(f);
    lv_obj_set_size(f, 466, 466);
    lv_obj_set_pos(f, 0, 0);
    lv_obj_set_style_radius(f, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(f, lv_color_hex(color), 0);
    if (abajo != color) { lv_obj_set_style_bg_grad_color(f, lv_color_hex(abajo), 0); lv_obj_set_style_bg_grad_dir(f, LV_GRAD_DIR_VER, 0); }
    lv_obj_set_style_bg_opa(f, LV_OPA_COVER, 0);
    lv_obj_remove_flag(f, LV_OBJ_FLAG_CLICKABLE);
    if (s_cerrar_x) ocultar(s_cerrar_x, !s_cer_repetida);
    s_cer_nobj = 0;
    s_cer_ella = s_cer_ella2 = s_cer_cor = s_cer_txt = NULL;
    s_cer_medio[0] = s_cer_medio[1] = NULL;
    for (int i = 0; i < CER_PART; i++) s_cer_part[i] = NULL;
    s_cer_t0 = s_ms;
    s_cer_paso = 0;
    s_cer_toque_desde = 0xFFFFFFFFu;
}
static lv_obj_t *cer_texto(const char *t, uint32_t color, int y, uint32_t retraso)
{
    if (!t || s_nino) return NULL;
    char b[420];
    snprintf(b, sizeof b, "%s", gtxt(t));
    mayusculas(b);
    const int w = y < 110 || y > 340 ? 280 : 320; /* (arriba y abajo el círculo es más angosto) */
    lv_obj_t *x = texto_c(s_tarjeta, b, color, 2, w); /* (cada renglón centrado) */
    lv_obj_align(x, LV_ALIGN_TOP_MID, 0, y);
    fin_aparecer(x, retraso);
    return x;
}
/* chispas y luciérnagas: unos puntitos de luz que salen de un lugar y se apagan */
static void cer_chispa(int x, int y, uint32_t color, float vx, float vy)
{
    int k = 0;
    for (int i = 0; i < CER_PART; i++) { if (!s_cer_part[i]) { k = i; break; } if (s_cer_pt0[i] < s_cer_pt0[k]) k = i; }
    if (!s_cer_part[k]) {
        s_cer_part[k] = lv_obj_create(s_tarjeta);
        lv_obj_remove_style_all(s_cer_part[k]);
        lv_obj_set_size(s_cer_part[k], 6, 6);
        lv_obj_set_style_radius(s_cer_part[k], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(s_cer_part[k], LV_OPA_COVER, 0);
        lv_obj_remove_flag(s_cer_part[k], LV_OBJ_FLAG_CLICKABLE);
    }
    lv_obj_set_style_bg_color(s_cer_part[k], lv_color_hex(color), 0);
    s_cer_px[k] = (float)x; s_cer_py[k] = (float)y; s_cer_vx[k] = vx; s_cer_vy[k] = vy; s_cer_pt0[k] = s_ms;
    ocultar(s_cer_part[k], false);
}
static void cer_chispas_mover(void)
{
    for (int i = 0; i < CER_PART; i++) {
        if (!s_cer_part[i]) continue;
        const uint32_t d = s_ms - s_cer_pt0[i];
        if (d > 1400) { ocultar(s_cer_part[i], true); continue; }
        s_cer_px[i] += s_cer_vx[i]; s_cer_py[i] += s_cer_vy[i];
        s_cer_vy[i] += 0.02f;
        lv_obj_set_pos(s_cer_part[i], (int)s_cer_px[i] - 3, (int)s_cer_py[i] - 3);
        lv_obj_set_style_opa(s_cer_part[i], (lv_opa_t)(d < 1000 ? 255 : (1400 - d) * 255 / 400), 0);
    }
}
/* tocar para seguir (cuando ya apareció todo) */
static void cer_toque_cb(lv_event_t *e)
{
    (void)e;
    if (s_panel != PANEL_FINAL || !s_cer || s_cer_toque_desde == 0xFFFFFFFFu || (int32_t)(s_ms - s_cer_toque_desde) < 0) return;
    s_cer_toque_desde = 0xFFFFFFFFu;
    cer_pagina(s_cer_pag + 1);
}
static void cer_tocar_para_seguir(uint32_t desde_ms)
{
    s_cer_toque_desde = s_cer_t0 + desde_ms;
    lv_obj_add_flag(s_tarjeta, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_event_cb(s_tarjeta, cer_toque_cb);
    lv_obj_add_event_cb(s_tarjeta, cer_toque_cb, LV_EVENT_CLICKED, NULL);
}
/* los que celebran: las criaturas que conoce y los animales de sus fichas (nunca alguien que no ha visto) */
static int cer_invitados(art_icono_t *ico, int max)
{
    int n = 0;
    if (!s_pet) return 0;
    for (int c = 0; c < CRI_COUNT && n < max; c++) if ((s_pet->criaturas_vistas >> c) & 1u) ico[n++] = ICO_CRIATURA[c];
    if (pet_special_known(s_pet, ESP_CRISTALES) && n < max) ico[n++] = ICO_PRISMA;
    if (s_pet->tilde_visitas && n < max) ico[n++] = ICO_TILDE;
    if (s_pet->brizna && n < max) ico[n++] = ICO_BRIZNA;
    for (int i = 0; i < pet_cards_known(s_pet) && n < max; i++) {
        const int f = pet_card_get(s_pet, i);
        if (f >= 0 && f < FAUNA_COUNT) ico[n++] = ICO_FAUNA[f];
        else if (f >= FAUNA_COUNT && f < PET_FICHAS) ico[n++] = ICO_ANIMAL[f - FAUNA_COUNT];
    }
    return n;
}
static lv_obj_t *cer_ella_en(int slot, art_expresion_t ex, int esc)
{
    lv_obj_t *m = mini_criatura(s_tarjeta, slot, pet_look(s_pet), s_pet->etapa, s_pet->forma, esc, ex);
    lv_obj_remove_flag(m, LV_OBJ_FLAG_CLICKABLE);
    return m;
}
static lv_obj_t *cer_ella(art_expresion_t ex, int esc)
{
    lv_obj_t *m = mini_criatura(s_tarjeta, PET_MAX_AMIGOS + 1, pet_look(s_pet), s_pet->etapa, s_pet->forma, esc, ex);
    lv_obj_remove_flag(m, LV_OBJ_FLAG_CLICKABLE);
    return m;
}
static void cer_centrar(lv_obj_t *o, int cx, int cy)
{
    if (!o) return;
    const lv_image_dsc_t *d = lv_image_get_src(o);
    const int w = d ? (int)d->header.w : 40, h = d ? (int)d->header.h : 40;
    lv_obj_set_pos(o, cx - w / 2, cy - h / 2);
}

/* ======================================================================
 * El rito de la Guardiana
 * ==================================================================== */
#define RITO2_CX 233
#define RITO2_ELLA_Y 196   /* donde queda flotando */
#define RITO2_PECHO 30     /* el pecho, bajo su cabeza */
#define RITO2_PIEDRAS_Y 348
#define RITO2_DUR 16500
static void rito2_pag0(void)
{
    cer_fondo(cer_blanco(), cer_blanco());
    /* las ocho piedras: formas simples, en un óvalo */
    for (int i = 0; i < 8; i++) {
        const float a = (float)i * 0.7854f;
        lv_obj_t *pi = lv_obj_create(s_tarjeta);
        lv_obj_remove_style_all(pi);
        const int w = 30 + (i % 3) * 4, h = 22 + (i % 2) * 6;
        lv_obj_set_size(pi, w, h);
        lv_obj_set_pos(pi, RITO2_CX + (int)(cosf(a) * 112) - w / 2, RITO2_PIEDRAS_Y + (int)(sinf(a) * 24) - h);
        lv_obj_set_style_radius(pi, 10, 0);
        lv_obj_set_style_bg_color(pi, lv_color_hex(0xB8B2C8), 0);
        lv_obj_set_style_bg_grad_color(pi, lv_color_hex(0x8A84A0), 0);
        lv_obj_set_style_bg_grad_dir(pi, LV_GRAD_DIR_VER, 0);
        lv_obj_set_style_bg_opa(pi, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(pi, 2, 0);
        lv_obj_set_style_border_color(pi, lv_color_hex(0x6A6488), 0);
        lv_obj_remove_flag(pi, LV_OBJ_FLAG_CLICKABLE);
        fin_aparecer(pi, 2600 + (uint32_t)i * 120);
        if (s_cer_nobj < 16) s_cer_obj[s_cer_nobj++] = pi;
    }
    s_cer_ella = cer_ella(EXP_CERRADOS, 6);
    s_cer_ella2 = cer_ella_en(PET_MAX_AMIGOS + 2, EXP_FELIZ, 6); /* (la misma, contenta: aparece al unirse las mitades) */
    ocultar(s_cer_ella2, true);
    s_cer_medio[0] = icono(s_tarjeta, ICO_MEDIO_CORAZON, 4); /* la de la isla */
    s_cer_medio[1] = icono(s_tarjeta, ICO_MEDIO_CORAZON, 3); /* la que lleva ella dentro */
    lv_image_set_pivot(s_cer_medio[0], 24, 24);
    lv_image_set_pivot(s_cer_medio[1], 18, 18);
    lv_obj_set_style_image_recolor(s_cer_medio[1], lv_color_hex(0xFFE27A), 0);
    s_cer_cor = icono(s_tarjeta, ICO_OBJETO(OBJ_CORAZON_BOSQUE), 5);
    lv_image_set_pivot(s_cer_cor, 30, 30);
    ocultar(s_cer_medio[0], true); ocultar(s_cer_medio[1], true); ocultar(s_cer_cor, true);
    accion(UI_ACCION_SONAR, SND_SUSPIRO_AMOR);
}
static void rito2_animar(void)
{
    const uint32_t t = s_ms - s_cer_t0;
    cer_chispas_mover();
    /* ella: sube despacio desde abajo hasta el centro, y se mece en el aire */
    const float sube = t < 1200 ? 0.0f : (t < 4200 ? suave((float)(t - 1200) / 3000.0f) : 1.0f);
    const int ey = 330 - (int)((330 - RITO2_ELLA_Y) * sube) + (int)(sinf((float)t / 420.0f) * 4.0f * sube);
    cer_centrar(s_cer_ella, RITO2_CX, ey);
    cer_centrar(s_cer_ella2, RITO2_CX, ey);
    if (t > 1200 && t < 9000 && t % 160 < TICK_MS) cer_chispa(RITO2_CX - 50 + (int)(azar() % 100), ey - 40 + (int)(azar() % 80), azar() & 1 ? 0xFFD76A : 0xB8E8FF, 0, -0.6f);
    /* la mitad de la isla: sale del centro de las piedras y sube */
    const int pecho_y = ey + RITO2_PECHO;
    if (t > 4000 && t < 9200) {
        const float k = suave((float)(t - 4000) / 2200.0f);
        float yk = RITO2_PIEDRAS_Y - 30 - (RITO2_PIEDRAS_Y - 30 - (pecho_y + 70)) * k;
        if (t > 6400) yk += ((float)pecho_y - yk) * suave((float)(t - 6400) / 2600.0f); /* se acercan */
        const float rap = t < 6400 ? 420.0f : (t < 8000 ? 220.0f : 120.0f); /* laten cada vez más rápido */
        const float lat = 1.0f + 0.16f * sinf((float)t / rap);
        lv_image_set_scale(s_cer_medio[0], (uint32_t)(256 * lat));
        cer_centrar(s_cer_medio[0], RITO2_CX + 8, (int)yk);
        ocultar(s_cer_medio[0], false);
    } else ocultar(s_cer_medio[0], true);
    /* la mitad que ella lleva dentro: brilla en su pecho; primero a destiempo, después al mismo tiempo (se reconocen) */
    if (t > 4600 && t < 9200) {
        const float rap = t < 6400 ? 420.0f : (t < 8000 ? 220.0f : 120.0f);
        const float lat = 1.0f + 0.2f * sinf((float)t / rap + (t < 6400 ? 1.57f : 0.0f));
        lv_image_set_scale(s_cer_medio[1], (uint32_t)(256 * lat));
        cer_centrar(s_cer_medio[1], RITO2_CX - 8, pecho_y);
        lv_obj_set_style_image_recolor_opa(s_cer_medio[1], (lv_opa_t)(90 + 60 * sinf((float)t / rap)), 0);
        ocultar(s_cer_medio[1], false);
        if (s_cer_paso == 0 && t > 4700) { s_cer_paso = 1; accion(UI_ACCION_SONAR, SND_RARO); }
        if (t > 6400 && t % (uint32_t)rap < TICK_MS * 2) accion(UI_ACCION_SONAR, SND_TOQUE);
    } else ocultar(s_cer_medio[1], true);
    /* ¡se unen! un latido fuerte, el Corazón entero y aros de luz */
    if (s_cer_paso <= 1 && t > 9200) {
        s_cer_paso = 2;
        accion(UI_ACCION_SONAR, SND_META);
        fin_aro(s_tarjeta, RITO2_CX, pecho_y, 0, false);
        fin_aro(s_tarjeta, RITO2_CX, pecho_y, 300, false);
        fin_aro(s_tarjeta, RITO2_CX, pecho_y, 600, false);
        ocultar(s_cer_ella, true); ocultar(s_cer_ella2, false);
        for (int i = 0; i < CER_PART; i++) { const float a = (float)i * 0.349f; cer_chispa(RITO2_CX, pecho_y, i & 1 ? 0xB8FF8A : 0xFFE27A, cosf(a) * 3.2f, sinf(a) * 3.2f - 0.6f); } /* el estallido de luciérnagas */
    }
    if (t > 9200 && t < 12400) {
        const uint32_t d = t - 9200;
        const float pop = d < 500 ? 0.4f + 0.9f * (float)d / 500.0f : 1.0f + 0.08f * sinf((float)t / 260.0f);
        lv_image_set_scale(s_cer_cor, (uint32_t)(256 * (pop > 1.3f ? 1.3f : pop)));
        cer_centrar(s_cer_cor, RITO2_CX, pecho_y - 4 - (int)(d > 1600 ? (d - 1600) / 40 : 0));
        lv_obj_set_style_opa(s_cer_cor, (lv_opa_t)(d > 2400 ? (3200 - (d > 3200 ? 3200 : d)) * 255 / 800 : 255), 0);
        ocultar(s_cer_cor, false);
    } else ocultar(s_cer_cor, true);
    /* la celebración: saltan alrededor los que conoce; Nimba cruza por arriba */
    if (s_cer_paso == 2 && t > 9900) {
        s_cer_paso = 3;
        art_icono_t ic[16];
        const int n = cer_invitados(ic, 14);
        for (int i = 0; i < n; i++) {
            const float a = 3.1416f * (0.08f + 0.84f * (float)i / (float)(n > 1 ? n - 1 : 1)); /* un arco por abajo */
            lv_obj_t *o = icono(s_tarjeta, ic[i], 3);
            lv_obj_set_pos(o, RITO2_CX + (int)(cosf(a) * 170) - 18, 250 + (int)(sinf(a) * 120) - 18);
            lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE);
            fin_pop(o, (uint32_t)i * 120, 60);
            fin_salta(o, 800 + (uint32_t)(i % 4) * 140);
        }
        if (s_pet && s_pet->historias_nimba) {
            lv_obj_t *nb = icono(s_tarjeta, ICO_NIMBA, 4);
            lv_obj_remove_flag(nb, LV_OBJ_FLAG_CLICKABLE);
            s_cer_obj[0] = nb; /* (las piedras ya no se mueven: el primero se usa para Nimba) */
            lv_obj_set_pos(nb, -120, 56);
        } else s_cer_obj[0] = NULL;
        accion(UI_ACCION_SONAR, SND_HADAS);
        cer_texto(s_suave ? "¡AHORA SOY {LA} {GUARDIANA}!" : "LA OTRA MITAD... ¡ESTABA EN MÍ! AHORA SOY {LA} {GUARDIANA} DEL MUNDO", CER_ORO, 84, 1400);
    }
    if (s_cer_paso >= 3 && s_cer_obj[0]) lv_obj_set_x(s_cer_obj[0], -120 + (int)((t - 9900) * 640 / 6000)); /* Nimba, de un lado a otro */
    if (t > 10000 && t < 14000 && t % 220 < TICK_MS) cer_chispa(60 + (int)(azar() % 346), 120 + (int)(azar() % 220), azar() & 1 ? 0xB8FF8A : 0xFFE27A, 0, -0.5f);
    if (t > RITO2_DUR) cer_pagina(1);
}
/* el recuerdo dorado: Tilo, chiquito, guarda la mitad del Corazón en un huevo y lo esconde bajo el árbol viejo */
static void rito2_oro(void)
{
    abrir_tarjeta(PANEL_FINAL, 466, 466);
    lv_obj_align(s_tarjeta, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_opa(s_tarjeta, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_tarjeta, 0, 0);
    if (s_cerrar_x) ocultar(s_cerrar_x, !s_cer_repetida);
    for (int i = 0; i < CER_PART; i++) s_cer_part[i] = NULL;
    s_cer_t0 = s_ms;
    escena_construir(s_tarjeta, 0, 0, &CINE_ORO, 0, 2, 466, 466, 176, false);
    if (!s_nino) {
        lv_obj_t *pl = lv_obj_create(s_tarjeta);
        lv_obj_remove_style_all(pl);
        lv_obj_set_size(pl, 290, LV_SIZE_CONTENT);
        lv_obj_set_style_pad_ver(pl, 8, 0);
        lv_obj_align(pl, LV_ALIGN_TOP_MID, 0, 66); /* (arriba, en el cielo dorado: abajo va la X) */
        lv_obj_set_style_radius(pl, 20, 0);
        lv_obj_set_style_bg_color(pl, lv_color_hex(0x3A2410), 0);
        lv_obj_set_style_bg_opa(pl, LV_OPA_70, 0);
        lv_obj_remove_flag(pl, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_t *x = texto_c(pl, s_suave ? "ÉL GUARDÓ LA MITAD DEL CORAZÓN EN UN HUEVO. ¡EN TU HUEVO!" :
                                          "UN RECUERDO: ANTES DE DORMIRSE CON LAS ESTRELLAS, EL CUIDADOR GUARDÓ LA OTRA MITAD DEL CORAZÓN EN UN HUEVO. Y LO ESCONDIÓ PARA QUE LLEGARA A BUENAS MANOS.", 0xFFEFB8, 2, 262);
        lv_obj_center(x);
        fin_aparecer(pl, 4200);
    }
    accion(UI_ACCION_SONAR, SND_HADAS);
    s_cer_toque_desde = 0xFFFFFFFFu;
    cer_tocar_para_seguir(5200);
}
/* la historia del rito sobre el mismo blanco, de a una hoja (RITO_TXT, en ui_final.inc) */
static void rito2_hoja(int h)
{
    cer_fondo(cer_blanco(), cer_blanco());
    if (h == 0) {
        lv_obj_t *li = icono(s_tarjeta, ICO_LIBRO_MAGICO, 5);
        lv_obj_align(li, LV_ALIGN_TOP_MID, 0, 48);
        fin_aparecer(li, 0);
        char t[64];
        snprintf(t, sizeof t, "POR QUÉ %s ES {LA} {GUARDIANA}", s_pet->nombre);
        cer_texto(t, CER_ORO, 140, 300);
    }
    cer_texto(RITO_TXT[h], h == RITO_HOJAS - 1 ? CER_ORO : CER_TINTA, h == 0 ? 186 : 150, 700);
    for (int i = 0; i < 6; i++) { lv_obj_t *e = icono(s_tarjeta, ICO_ESTRELLA, 1); lv_obj_set_pos(e, 120 + (int)(azar() % 230), 340 + (int)(azar() % 50)); fin_aparecer(e, 900 + (uint32_t)i * 150); }
    cer_tocar_para_seguir(1400);
}
/* las hojas: todas (adulto), las dos últimas (niño), ninguna (infante) */
static int rito2_hojas(int *primera)
{
    if (s_nino) { *primera = 0; return 0; }
    if (s_suave) { *primera = RITO_HOJAS - 2; return 2; }
    *primera = 0;
    return RITO_HOJAS;
}

/* ======================================================================
 * El último verso (el segundo final)
 * ==================================================================== */
#define VER_CRIS_Y 300
static const int16_t VER_CRIS_X[CRIS_TOT] = {78, 128, 178, 228, 278, 328, 378, 0}; /* (el 8.º no se usa aquí: son los 6 del verso y el celeste) */
static int ver_cris_x(int k) { static const uint8_t ORD[CRIS_TOT] = {1, 3, 4, 6, 2, 5, 0, 7}; return VER_CRIS_X[ORD[k % CRIS_TOT] % 7]; } /* de grave a agudo, de izquierda a derecha */
static lv_obj_t *s_ver_cris[CRIS_TOT], *s_ver_aro;
static uint32_t s_ver_luz[CRIS_TOT];
static uint8_t s_ver_notas[PET_CANCION_NOTAS];
static int s_ver_n, s_ver_i;
static void ver_luz(int k, bool suena, bool suave_)
{
    if ((unsigned)k >= CRIS_TOT) return;
    s_ver_luz[k] = s_ms;
    if (suena) accion(UI_ACCION_SONAR, snd_cristal(k) | (suave_ ? 0x100 : 0));
}
static void ver_cris_cb(lv_event_t *e);
static void ver_cristales(bool tocables)
{
    for (int k = 0; k < CRIS_TOT; k++) s_ver_cris[k] = NULL;
    for (int k = 0; k < 7; k++) {
        const int c = k; /* cristal */
        lv_obj_t *o = icono(s_tarjeta, c < 6 ? (art_icono_t)(ICO_CRIS_0 + c) : ICO_CRIS_0, c < 6 ? 4 : 3);
        if (c == 6) { lv_obj_set_style_image_recolor(o, lv_color_hex(SIM_COLOR[6]), 0); lv_obj_set_style_image_recolor_opa(o, LV_OPA_60, 0); }
        lv_obj_set_pos(o, ver_cris_x(c) - 24, VER_CRIS_Y - 48);
        if (tocables) {
            lv_obj_add_flag(o, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_ext_click_area(o, 8);
            lv_obj_add_event_cb(o, ver_cris_cb, LV_EVENT_PRESSED, (void *)(intptr_t)c);
        } else lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE);
        s_ver_cris[c] = o;
        s_ver_luz[c] = 0;
    }
    s_ver_aro = lv_obj_create(s_tarjeta); /* el que te toca tocar */
    lv_obj_remove_style_all(s_ver_aro);
    lv_obj_set_size(s_ver_aro, 60, 60);
    lv_obj_set_style_radius(s_ver_aro, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(s_ver_aro, 3, 0);
    lv_obj_set_style_border_color(s_ver_aro, lv_color_hex(0xFFE27A), 0);
    lv_obj_remove_flag(s_ver_aro, LV_OBJ_FLAG_CLICKABLE);
    ocultar(s_ver_aro, true);
}
static void ver_cristales_animar(void)
{
    for (int k = 0; k < 7; k++) {
        if (!s_ver_cris[k]) continue;
        const int32_t d = (int32_t)(s_ms - s_ver_luz[k]);
        const float b = s_ver_luz[k] && d >= 0 && d < 500 ? 1.0f - (float)d / 500.0f : 0;
        lv_obj_set_style_translate_y(s_ver_cris[k], (int)(-b * 8), 0);
        lv_obj_set_style_image_recolor(s_ver_cris[k], lv_color_hex(b > 0 ? 0xFFFFFF : 0x0C0820), 0);
        lv_obj_set_style_image_recolor_opa(s_ver_cris[k], (lv_opa_t)(b > 0 ? b * 120 : (k == 6 ? 60 : 70)), 0);
        if (b > 0.9f && s_ms % 3 == 0) cer_chispa(ver_cris_x(k), VER_CRIS_Y - 50, SIM_COLOR[k], (azar01() - 0.5f) * 1.6f, -1.2f);
    }
}
static int ver_objetivo(void) { return s_ver_i < s_ver_n ? PET_CANCION_CRISTAL(s_ver_notas[s_ver_i]) % 7 : -1; }
static void ver_cris_cb(lv_event_t *e)
{
    const int k = (int)(intptr_t)lv_event_get_user_data(e);
    if (s_panel != PANEL_FINAL || s_cer != CER_VERSO || s_cer_pag != 2) return;
    const int obj = ver_objetivo();
    if (obj < 0) return;
    if (k == obj || s_nino) { /* (los más chicos: cualquier cristal sirve) */
        ver_luz(obj, true, false);
        s_ver_i++;
        if (s_ver_i >= s_ver_n) { s_cer_paso = 1; s_cer_t0 = s_ms; ocultar(s_ver_aro, true); }
    } else ver_luz(k, true, true); /* otro: suena bajito, sin problema */
}
static void ver_pag(int pag)
{
    switch (pag) {
    case 0: { /* la propuesta de Prisma, en la cueva */
        cer_fondo(0x1A1430, 0x2A2050);
        lv_obj_t *pr = icono(s_tarjeta, ICO_PRISMA, 7);
        lv_obj_align(pr, LV_ALIGN_TOP_MID, 0, 60);
        fin_pop(pr, 200, 60);
        fin_aro(s_tarjeta, CX, 130, 600, true);
        fin_luciernagas(s_tarjeta, 10, 220, 340);
        cer_texto(s_suave ? "PRISMA: «SU CANCIÓN NO TIENE FINAL. ¿LA TERMINAMOS?»" :
                            "PRISMA: «SU VERSO SIGUE AHÍ, ENTRE LOS CRISTALES. SIN TERMINAR. CIEN AÑOS ESPERANDO... ¿LO TERMINAMOS?»", 0xFFEFB8, 236, 900);
        if (s_nino) { lv_obj_t *n = icono(s_tarjeta, ICO_NOTA, 4); lv_obj_align(n, LV_ALIGN_TOP_MID, 0, 240); fin_salta(n, 600); }
        accion(UI_ACCION_SONAR, SND_DRAGON_TARAREA);
        cer_tocar_para_seguir(1800);
        break;
    }
    case 1: /* su verso: desafinado, y se corta */
        cer_fondo(0x1A1430, 0x2A2050);
        ver_cristales(false);
        s_ver_n = pet_tilo_verse(s_ver_notas, PET_CANCION_NOTAS);
        s_ver_i = 0;
        cer_texto(s_suave ? "ASÍ CANTABA ÉL..." : "LOS CRISTALES SE ACUERDAN DE SU VERSO. ASÍ CANTABA ÉL: DESAFINADO Y CON GANAS...", 0xCFF5C8, 90, 300);
        break;
    case 2: /* nuestra canción, encima: se ilumina el que toca, y tú lo tocas */
        cer_fondo(0x1A1430, 0x2A2050);
        ver_cristales(true);
        s_ver_n = 0;
        if (s_pet && s_pet->canciones[0].n) { s_ver_n = s_pet->canciones[0].n; if (s_ver_n > 8) s_ver_n = 8; memcpy(s_ver_notas, s_pet->canciones[0].nota, (size_t)s_ver_n); }
        else { uint8_t m[PET_MELODIA_NOTAS]; const int nm = s_pet ? pet_melody(s_pet, m) : 0; for (int i = 0; i < nm && i < 8; i++) s_ver_notas[s_ver_n++] = PET_CANCION_NOTA(m[i], 2); }
        s_ver_i = 0;
        cer_texto(s_suave ? "¡AHORA TÚ: NUESTRA CANCIÓN!" : "AHORA TÚ: TOCA NUESTRA CANCIÓN ENCIMA. SIGUE LA LUZ", 0xFFEFB8, 90, 200);
        break;
    case 3: /* Prisma afina la última nota */
        cer_fondo(0x1A1430, 0x2A2050);
        ver_cristales(false);
        { lv_obj_t *pr = icono(s_tarjeta, ICO_PRISMA, 5); lv_obj_align(pr, LV_ALIGN_TOP_MID, 0, 60); fin_aparecer(pr, 0); s_cer_obj[0] = pr; s_cer_nobj = 1; }
        cer_texto(s_suave ? "PRISMA AFINÓ LA ÚLTIMA NOTA. ¡LA CANCIÓN ESTÁ COMPLETA!" : "PRISMA AFINA LA ÚLTIMA NOTA, LA QUE ÉL NUNCA ALCANZÓ. LA CANCIÓN ESTÁ COMPLETA.", 0xFFEFB8, 170, 2600);
        cer_tocar_para_seguir(5200);
        break;
    case 4: { /* todos cantan: por primera vez, todos cuidan juntos a alguien. A él */
        fin_fondo();
        for (int i = 0; i < CER_PART; i++) s_cer_part[i] = NULL;
        s_cer_t0 = s_ms; s_cer_paso = 0; s_cer_nobj = 0; s_cer_toque_desde = 0xFFFFFFFFu;
        if (s_cerrar_x) ocultar(s_cerrar_x, !s_cer_repetida);
        s_cer_ella = cer_ella(EXP_FELIZ, 4);
        cer_centrar(s_cer_ella, CX, 330);
        fin_salta(s_cer_ella, 600);
        art_icono_t ic[16];
        const int n = cer_invitados(ic, 12);
        for (int i = 0; i < n; i++) {
            const float a = 3.1416f * (1.05f + 0.9f * (float)i / (float)(n > 1 ? n - 1 : 1)); /* un arco por arriba de ella */
            lv_obj_t *o = icono(s_tarjeta, ic[i], 3);
            lv_obj_set_pos(o, CX + (int)(cosf(a) * 160) - 18, 300 + (int)(sinf(a) * 110) - 18);
            lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE);
            fin_pop(o, 300 + (uint32_t)i * 160, 60);
            fin_salta(o, 1200 + (uint32_t)(i % 3) * 200);
        }
        if (s_pet && s_pet->historias_nimba) { lv_obj_t *nb = icono(s_tarjeta, ICO_NIMBA, 4); lv_obj_remove_flag(nb, LV_OBJ_FLAG_CLICKABLE); lv_obj_set_pos(nb, CX - 48, 480); s_cer_obj[0] = nb; s_cer_nobj = 1; }
        uint8_t ts[PET_CANCION_NOTAS];
        int tn = 0;
        { /* su canción entera (todavía no queda guardada: eso pasa al terminar) */
            uint8_t v[PET_CANCION_NOTAS]; const int nv = pet_tilo_verse(v, PET_CANCION_NOTAS);
            for (int i = 0; i < nv - 1 && tn < PET_CANCION_NOTAS - 1; i++) ts[tn++] = v[i];
            for (int i = 0; i < s_ver_n && tn < PET_CANCION_NOTAS - 1 && i < 6; i++) ts[tn++] = s_ver_notas[i];
            ts[tn++] = PET_CANCION_NOTA(0, 8);
        }
        rep_tocar(ts, tn, REP_CRISTAL | REP_FLOTAR);
        s_cer_txt = cer_texto(s_suave ? "¡CANTAN TODOS JUNTOS!" : "LLEGAN TODOS A LA CUEVA Y CANTAN, CADA UNO A SU MANERA.", 0xFFEFB8, 84, 400);
        break;
    }
    case 5: { /* la cuchara en el cielo, que parpadea al ritmo */
        fin_fondo();
        for (int i = 0; i < CER_PART; i++) s_cer_part[i] = NULL;
        s_cer_t0 = s_ms; s_cer_paso = 0; s_cer_nobj = 0; s_cer_toque_desde = 0xFFFFFFFFu;
        if (s_cerrar_x) ocultar(s_cerrar_x, !s_cer_repetida);
        static const int8_t CU[10][2] = {{-70, 0}, {-56, -18}, {-34, -20}, {-20, -4}, {-34, 14}, {-56, 14}, {-4, -2}, {22, 4}, {48, 10}, {74, 16}};
        for (int i = 0; i < 10; i++) {
            lv_obj_t *e = lv_obj_create(s_tarjeta);
            lv_obj_remove_style_all(e);
            lv_obj_set_size(e, 10, 10);
            lv_obj_set_style_radius(e, LV_RADIUS_CIRCLE, 0);
            lv_obj_set_style_bg_color(e, lv_color_hex(0xFFF3B0), 0);
            lv_obj_set_style_bg_opa(e, LV_OPA_COVER, 0);
            lv_obj_set_style_shadow_width(e, 12, 0);
            lv_obj_set_style_shadow_color(e, lv_color_hex(0xFFE27A), 0);
            lv_obj_set_style_shadow_opa(e, LV_OPA_70, 0);
            lv_obj_remove_flag(e, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_pos(e, CX + CU[i][0] * 2 - 5, 150 + CU[i][1] * 2 - 5);
            fin_aparecer(e, 400 + (uint32_t)i * 260);
            s_cer_obj[i] = e;
        }
        s_cer_nobj = 10;
        cer_texto(s_suave ? "¡ÉL LA ESCUCHÓ!" : "ÉL LA ESCUCHÓ.", 0xFFEFB8, 250, 3200); /* (v99) la cuchara de estrellas se ve: no se cuenta */
        accion(UI_ACCION_SONAR, SND_HADAS);
        cer_tocar_para_seguir(5400);
        break;
    }
    case 6: { /* el nombre: TILO, escrito a mano en la primera página del Libro */
        fin_fondo();
        s_cer_t0 = s_ms; s_cer_toque_desde = 0xFFFFFFFFu;
        if (s_cerrar_x) ocultar(s_cerrar_x, !s_cer_repetida);
        fin_luciernagas(s_tarjeta, 12, 60, 300);
        fin_aro(s_tarjeta, CX, 130, 300, true);
        lv_obj_t *li = icono(s_tarjeta, ICO_LIBRO_MAGICO, 6);
        lv_obj_align(li, LV_ALIGN_TOP_MID, 0, 46);
        fin_aparecer(li, 0);
        lv_obj_t *tilo = icono(s_tarjeta, ICO_TILO, 4);
        lv_obj_align(tilo, LV_ALIGN_TOP_MID, -110, 96);
        fin_subir(tilo, 1600, 20);
        lv_obj_t *x = texto(s_tarjeta, "PEUMO", FIN_COLOR_ORO, 5, 0);
        lv_obj_align(x, LV_ALIGN_TOP_MID, 0, 184);
        lv_image_set_rotation(x, -40); /* escrito a mano: un poquito chueco */
        fin_pop(x, 1000, 60);
        cer_texto(s_suave ? "SE LLAMABA PEUMO. ERA CHIQUITO, COMO TÚ." : "POR FIN, SU NOMBRE. SE LLAMABA PEUMO, COMO EL ÁRBOL QUE DA SOMBRA Y PERFUMA. ERA CHIQUITO, COMO TÚ. NADIE LO CUIDÓ NUNCA... HASTA HOY.", 0xCFF5C8, 254, 2600);
        accion(UI_ACCION_SONAR, SND_SORPRESA);
        cer_tocar_para_seguir(4200);
        break;
    }
    case 7: { /* la última página de su diario: la única escrita para ella */
        fin_fondo();
        s_cer_t0 = s_ms; s_cer_toque_desde = 0xFFFFFFFFu;
        if (s_cerrar_x) ocultar(s_cerrar_x, !s_cer_repetida);
        fin_luciernagas(s_tarjeta, 8, 360, 430);
        lv_obj_t *papel = lv_obj_create(s_tarjeta);
        lv_obj_remove_style_all(papel);
        lv_obj_set_size(papel, 300, 296);
        lv_obj_align(papel, LV_ALIGN_TOP_MID, 0, 76);
        lv_obj_set_style_bg_color(papel, lv_color_hex(0xF6E8C4), 0);
        lv_obj_set_style_bg_opa(papel, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(papel, 14, 0);
        lv_obj_set_style_border_width(papel, 3, 0);
        lv_obj_set_style_border_color(papel, lv_color_hex(0xB8925A), 0);
        lv_obj_remove_flag(papel, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
        static const char *const CARTA = "SI ESTÁS LEYENDO ESTO, TERMINASTE MI CANCIÓN... YO NUNCA SUPE PEDIR AYUDA, Y ENFERMÉ. TÚ APRENDISTE A CUIDARTE Y A CUIDAR. GRACIAS POR ENSEÑARLE AL MUNDO LO QUE YO NO SUPE HACER.";
        static const char *const CARTA_C = "TERMINASTE MI CANCIÓN. TÚ APRENDISTE A CUIDARTE Y A CUIDAR. ¡GRACIAS!";
        if (!s_nino) {
            lv_obj_t *tx = texto(papel, s_suave ? CARTA_C : CARTA, 0x4A3020, 2, 262);
            lv_obj_align(tx, LV_ALIGN_TOP_LEFT, 18, 16);
            fin_aparecer(tx, 800);
            lv_obj_update_layout(papel);
            lv_obj_t *fi = texto(papel, "- PEUMO", 0x8A5A30, 2, 0);
            lv_obj_align_to(fi, tx, LV_ALIGN_OUT_BOTTOM_RIGHT, 0, 6); /* (la firma, justo debajo de la carta) */
            fin_aparecer(fi, 3200);
            lv_obj_t *pd = texto(papel, "PD: BÚSCAME EN LAS ESTRELLAS. SOY LA CUCHARA.", 0x8A5A30, 2, 266);
            lv_obj_align_to(pd, tx, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 30);
            fin_aparecer(pd, 4400);
        }
        lv_obj_t *cu = icono(papel, ICO_CUCHARA_PALO, 2);
        lv_obj_align(cu, LV_ALIGN_BOTTOM_RIGHT, -16, -12); /* la cucharita, la firma de siempre */
        fin_aparecer(cu, 3400);
        fin_subir(papel, 0, 160);
        accion(UI_ACCION_SONAR, SND_SUSPIRO_AMOR);
        cer_tocar_para_seguir(s_nino ? 2400 : 5200);
        break;
    }
    default: { /* la última frase y la estampita dorada */
        fin_fondo();
        s_cer_t0 = s_ms; s_cer_toque_desde = 0xFFFFFFFFu;
        if (s_cerrar_x) ocultar(s_cerrar_x, !s_cer_repetida);
        fin_aro(s_tarjeta, CX, 190, 0, true);
        fin_luciernagas(s_tarjeta, 22, 80, 380);
        lv_obj_t *m = cer_ella(EXP_FELIZ, 7);
        lv_obj_align(m, LV_ALIGN_TOP_MID, 0, 100);
        fin_pop(m, 200, 40);
        fin_salta(m, 1200);
        cer_texto("HABÍA UNA VEZ ALGUIEN QUE CUIDÓ A QUIEN LO CUIDABA. Y EL MUNDO RECORDÓ.", 0xFFEFB8, 286, 1400);
        lv_obj_t *es = icono(s_tarjeta, ICO_ESTAMPITA, 4);
        lv_obj_align(es, LV_ALIGN_TOP_MID, 0, 46);
        lv_obj_set_style_image_recolor(es, lv_color_hex(FIN_COLOR_ORO), 0);
        lv_obj_set_style_image_recolor_opa(es, LV_OPA_50, 0);
        fin_pop(es, 3600, 900);
        cer_tocar_para_seguir(4400);
        break;
    }
    }
}
static void ver_animar(void)
{
    const uint32_t t = s_ms - s_cer_t0;
    cer_chispas_mover();
    switch (s_cer_pag) {
    case 1: { /* el verso: cada nota a su paso; la quinta se desafina; la última se corta */
        ver_cristales_animar();
        static uint32_t prox;
        if (t < 200) prox = s_cer_t0 + 1400;
        if (s_ver_i < s_ver_n && (int32_t)(s_ms - prox) >= 0) {
            const int k = PET_CANCION_CRISTAL(s_ver_notas[s_ver_i]) % 7;
            if (s_ver_i == 4) { ver_luz(k, true, false); accion(UI_ACCION_SONAR, snd_cristal(2) | 0x100); } /* desafinado: dos notas que chocan */
            else if (s_ver_i == s_ver_n - 1) { ver_luz(k, true, true); } /* y la última, casi sin voz */
            else ver_luz(k, true, false);
            prox = s_ms + (uint32_t)PET_CANCION_PASOS(s_ver_notas[s_ver_i]) * PET_PASO_MS * 2;
            s_ver_i++;
            if (s_ver_i == s_ver_n) { s_cer_paso = 1; s_cer_t0 = s_ms; }
        }
        if (s_cer_paso == 1 && t > 600 && !s_cer_txt) s_cer_txt = cer_texto(s_suave ? "...Y AQUÍ SE LE ACABÓ LA VOZ." : "...Y AQUÍ SE LE ACABÓ LA VOZ. FALTA EL FINAL.", 0xFFD76A, 150, 0);
        if (s_cer_paso == 1 && t > 3400) cer_pagina(2);
        break;
    }
    case 2: { /* te toca: el aro marca el cristal que sigue */
        ver_cristales_animar();
        const int obj = ver_objetivo();
        if (obj >= 0 && s_cer_paso == 0) {
            const int d = 60 + (int)(sinf((float)s_ms / 160.0f) * 6);
            lv_obj_set_size(s_ver_aro, d, d);
            lv_obj_set_pos(s_ver_aro, ver_cris_x(obj) - d / 2, VER_CRIS_Y - 26 - d / 2);
            ocultar(s_ver_aro, false);
            lv_obj_move_foreground(s_ver_aro);
        }
        if (s_cer_paso == 1 && t > 1200) cer_pagina(3);
        break;
    }
    case 3: { /* Prisma brilla; los cristales, todos juntos; y la nota que faltaba */
        ver_cristales_animar();
        if (s_cer_obj[0]) lv_obj_set_style_translate_y(s_cer_obj[0], (int)(sinf((float)s_ms / 500.0f) * 4), 0);
        if (s_cer_paso == 0 && t > 600) { s_cer_paso = 1; accion(UI_ACCION_SONAR, SND_DRAGON_TARAREA); }
        if (s_cer_paso == 1 && t > 1400) { for (int k = 0; k < 7; k++) s_ver_luz[k] = s_ms + (uint32_t)k * 110; s_cer_paso = 2; }
        if (s_cer_paso == 2 && t > 2400) {
            s_cer_paso = 3;
            accion(UI_ACCION_SONAR, SND_CRISTAL_CELESTE); accion(UI_ACCION_SONAR, SND_CRISTAL_AZUL); /* la última nota: do, grave y redondo, afinado */
            fin_aro(s_tarjeta, CX, VER_CRIS_Y - 30, 0, false);
            fin_aro(s_tarjeta, CX, VER_CRIS_Y - 30, 400, false);
            for (int i = 0; i < 12; i++) cer_chispa(CX, VER_CRIS_Y - 40, i & 1 ? 0xFFE27A : 0xB8E8FF, cosf((float)i * 0.52f) * 2.6f, sinf((float)i * 0.52f) * 2.0f - 1.0f);
        }
        break;
    }
    case 4: { /* cantan todos: notitas y frases de cada uno; Nimba sube la canción al cielo */
        if (t % 260 < TICK_MS) cer_chispa(80 + (int)(azar() % 300), 220 + (int)(azar() % 120), azar() & 1 ? 0xFFE27A : 0xCFF5C8, 0, -1.2f);
        if (t % 700 < TICK_MS) flotar(ICO_NOTA, NULL, 0, 80 + (int)(azar() % 300), 260, 0);
        static const char *const DICEN[3] = {"ONDINA LLORA DE EMOCIÓN (COMO SIEMPRE).", "CRISTALINO: «¡ALGUIEN DESAFINA!» (ERA ÉL MISMO).", "TILDE CANTA OTRA LETRA. NADIE LA CORRIGE."};
        const bool hay[3] = {s_pet && ((s_pet->criaturas_vistas >> CRI_ONDINA) & 1u), s_pet && ((s_pet->criaturas_vistas >> CRI_CRISTALINO) & 1u), s_pet && s_pet->tilde_visitas};
        const int idx = (int)(t / 2600) - 1;
        if (!s_suave && idx >= 0 && idx < 3 && s_cer_paso == idx) {
            s_cer_paso = idx + 1;
            if (s_cer_txt) { lv_obj_delete(s_cer_txt); s_cer_txt = NULL; }
            if (hay[idx]) s_cer_txt = cer_texto(DICEN[idx], 0xCFF5C8, 84, 0);
        }
        if (s_cer_obj[0] && t > 7800) { /* Nimba sube */
            const float k = suave((float)(t - 7800) / 3800.0f);
            lv_obj_set_y(s_cer_obj[0], 480 - (int)(k * 460));
            if (t % 300 < TICK_MS) cer_chispa(CX, 480 - (int)(k * 460) + 60, 0xFFE27A, 0, -1.6f);
        }
        if (t > 8000 && s_cer_paso < 4) {
            s_cer_paso = 4;
            if (s_cer_txt) { lv_obj_delete(s_cer_txt); s_cer_txt = NULL; }
            s_cer_txt = cer_texto(s_pet && s_pet->historias_nimba ? "LA CANCIÓN SALE DE LA CUEVA... Y NIMBA LA SUBE AL CIELO." : "LA CANCIÓN SALE DE LA CUEVA Y SUBE AL CIELO.", 0xFFEFB8, 84, 0);
        }
        if (t > 11800 && s_cer_toque_desde == 0xFFFFFFFFu) cer_tocar_para_seguir(t);
        break;
    }
    case 5: { /* la cuchara parpadea al ritmo */
        const uint32_t pulso = (uint32_t)PET_PASO_MS * 2;
        const bool on = (t / pulso) % 2 == 0;
        for (int i = 0; i < s_cer_nobj && i < 10; i++) if (s_cer_obj[i] && t > 400 + (uint32_t)i * 260 + 800) lv_obj_set_style_opa(s_cer_obj[i], on ? LV_OPA_COVER : LV_OPA_60, 0);
        if (t > 3200 && t % pulso < TICK_MS && (t / pulso) % 4 == 0) accion(UI_ACCION_SONAR, SND_TOQUE | 0x100);
        break;
    }
    default: break;
    }
}

/* ======================================================================
 * empezar, avanzar, terminar
 * ==================================================================== */
static void cer_pagina(int pag)
{
    if (!s_pet || !s_cer) return;
    s_cer_pag = pag;
    s_cer_txt = NULL;
    for (int i = 0; i < 16; i++) s_cer_obj[i] = NULL; /* (los de la página anterior ya no existen) */
    s_cer_nobj = 0;
    if (s_cer == CER_RITO) {
        int h0;
        const int nh = rito2_hojas(&h0);
        if (pag == 0) rito2_pag0();
        else if (pag == 1) rito2_oro();
        else if (pag - 2 < nh) rito2_hoja(h0 + pag - 2);
        else cer_terminar();
        return;
    }
    if (pag == 7 && s_nino) { cer_pagina(8); return; } /* (infante: sin carta) */
    if (pag > 8) { cer_terminar(); return; }
    ver_pag(pag);
}
static void cer_empezar(int cual, bool repetida)
{
    if (!s_pet) return;
    cerrar_panel();
    s_cer = cual;
    s_cer_repetida = repetida;
    s_fin_repetida = repetida; /* (abrir_tarjeta: la primera vez no se puede cerrar con la X) */
    rep_parar();
    cer_mundo(false);
    cer_pagina(0);
}
static void cer_velo(void) /* el blanco se funde de vuelta al mundo */
{
    if (!s_cer_velo) {
        s_cer_velo = caja(lv_layer_top());
        lv_obj_set_size(s_cer_velo, W, W);
        lv_obj_set_style_radius(s_cer_velo, LV_RADIUS_CIRCLE, 0);
        lv_obj_remove_flag(s_cer_velo, LV_OBJ_FLAG_CLICKABLE);
    }
    lv_obj_set_style_bg_color(s_cer_velo, lv_color_hex(s_cer == CER_RITO ? cer_blanco() : 0x0A0E2A), 0);
    lv_obj_set_style_bg_opa(s_cer_velo, LV_OPA_COVER, 0);
    ocultar(s_cer_velo, false);
    lv_obj_move_foreground(s_cer_velo);
    s_cer_velo_t0 = s_ms;
}
static void cer_terminar(void)
{
    const int cual = s_cer;
    const bool repetida = s_cer_repetida;
    s_cer = CER_NADA;
    rep_parar();
    if (s_panel == PANEL_FINAL) { s_fin_repetida = true; cerrar_panel(); }
    s_fin_repetida = false;
    cer_mundo(true);
    if (s_pet) { s_fondo_periodo = -1; refrescar_fondo(s_pet, s_ultimo_t); }
    s_cer = cual; cer_velo(); s_cer = CER_NADA;
    if (repetida) return;
    const int hx = (int)s_mascota.x, hy = cabeza_y(&s_mascota);
    if (cual == CER_RITO) {
        confeti(hx, hy - 30, 20);
        mascota_modo(M_SALTA, 2500);
        globo(ICO_CORAZON, s_nino ? NULL : "GRACIAS POR CUIDARME HASTA AQUÍ. TE VOY A PROTEGER SIEMPRE", 5000);
        libro_pedir(7); /* «Así termina el Libro...» */
    } else {
        accion(UI_ACCION_FINAL, PET_FINAL_SECRETO); /* la última página vuelve al Libro; la cuchara ya está en el cielo */
        accion(UI_ACCION_SONAR, SND_META);
        confeti(hx, hy - 30, 24);
        mascota_modo(M_SALTA, 2500);
        globo(ICO_ESTAMPITA, "¡ESTAMPITA DORADA!", 4500);
        for (int i = 0; i < 5; i++) flotar(ICO_ESTRELLA, NULL, 0, hx - 60 + i * 30, hy - 10, (uint32_t)i * 200);
    }
}
static void animar_ceremonias(void)
{
    if (s_cer_velo && !lv_obj_has_flag(s_cer_velo, LV_OBJ_FLAG_HIDDEN)) {
        const uint32_t d = s_ms - s_cer_velo_t0;
        if (d > 900) ocultar(s_cer_velo, true);
        else lv_obj_set_style_bg_opa(s_cer_velo, (lv_opa_t)((900 - d) * 255 / 900 / 32 * 32), 0);
    }
    if (!s_cer) return;
    if (s_panel != PANEL_FINAL) { /* algo la cerró (la X al volver a verla, o una alarma): el mundo vuelve */
        s_cer = CER_NADA; rep_parar(); cer_mundo(true);
        if (s_pet) { s_fondo_periodo = -1; refrescar_fondo(s_pet, s_ultimo_t); }
        return;
    }
    if (s_cer == CER_RITO) { if (s_cer_pag == 0) rito2_animar(); else cer_chispas_mover(); }
    else ver_animar();
}
bool ui_ceremonia_activa(void) { return s_cer != CER_NADA; }
/* (pruebas) */
void ui_demo_ceremonia(int cual, int pag) { if (!s_pet) return; cer_empezar(cual == 2 ? CER_VERSO : CER_RITO, true); if (pag > 0) cer_pagina(pag); }
