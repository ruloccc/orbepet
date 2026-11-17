/* ui_magia.inc: lo mágico de la versión 23 (lo incluye ui.c al final; usa sus funciones y variables).
 *
 *  - El mensajero: la paloma (o el búho de noche), la mariposa de la familia, la grulla de papel de las
 *    viajeras o la seña de una criatura llega a la zona en que estén, deja el sobre, ella lo recoge y te lo
 *    trae; recién ahí se abre el sobre grande. Si estás en un menú, espera.
 *  - Las criaturas mágicas: aparecen en su zona (se asoman o los esperan para la cita), se tocan, y las citas
 *    son juegos cortos (picnic, plantar, escondidas con Lumbre, luciérnagas, barquitos, hojas al viento,
 *    música, tesoro, estrellas y el faro; pescar y la cometa usan lo que ya existe).
 *  - Las historias de las criaturas (panel de la historia, también desde el diario y los Tesoros).
 *  - Los secretos de cada zona: el anillo de hadas, el reflejo encantado, la cometa y los cristales que cantan.
 */

/* ======================================================================
 * El mensajero
 * ==================================================================== */
static lv_obj_t *s_men, *s_men_sobre;
static int s_men_fase, s_men_tipo;
static uint32_t s_men_t;
static float s_men_x0, s_men_y0, s_men_dx, s_men_dy;
static bool s_men_entregado; /* esta carta ya la trajo: si cierras el sobre, vuelve a aparecer sin mensajero */
static bool s_cita_on, s_com_on, s_sim_on; /* (una cita, la cometa o el juego de los cristales en curso) */
static bool s_pantalla_visible = true;
static int s_fie_tipo = -1;     /* la fiesta con todos en curso (fecha_t o FIESTA_SU_CUMPLE; -1: ninguna) */
static void animar_fiesta(void);
static void fiesta_revisar(const pet_t *p, uint32_t utc);
void ui_screen_on(bool on)
{
    s_pantalla_visible = on;
    /* apagada: toda la lógica y las animaciones siguen igual, pero se apaga la invalidación y LVGL no dibuja ni manda cuadros al panel (eso es lo que gasta).
     * (Ocultar las capas con LV_OBJ_FLAG_HIDDEN reinicia la placa: una pantalla no tiene padre y LVGL lo usa.) */
    static bool apagado;
    lv_display_t *d = lv_display_get_default();
    if (!d || apagado == !on) return;
    apagado = !on;
    lv_display_enable_invalidation(d, on);
    if (on) lv_obj_invalidate(lv_screen_active()); /* se redibuja todo de una vez */
}
static void mensajero_fin(void);
/* la carta que espera en el mundo: un sobre que flota con su brillo junto a la mascota; al tocarlo se abre */
static lv_obj_t *s_carta_mundo;
static art_img_t s_carta_mundo_img;
static void carta_mundo_cb(lv_event_t *e)
{
    (void)e;
    if (s_panel != PANEL_NINGUNO) return;
    ocultar(s_carta_mundo, true);
    panel_carta_llega();
}
static void carta_mundo_poner(void)
{
    if (!s_pet || !pet_letter_pending(s_pet)) return;
    if (!s_carta_mundo) {
        s_carta_mundo = lv_image_create(s_mundo);
        lv_obj_add_flag(s_carta_mundo, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(s_carta_mundo, LV_OBJ_FLAG_FLOATING);
        lv_obj_add_event_cb(s_carta_mundo, carta_mundo_cb, LV_EVENT_CLICKED, NULL);
        lv_obj_set_ext_click_area(s_carta_mundo, 24);
    }
    art_carta_tipo((int)pet_letter_from(s_pet, 0));
    art_carta(&s_carta_mundo_img, 0, 1);
    lv_image_set_src(s_carta_mundo, &s_carta_mundo_img.dsc);
    ocultar(s_carta_mundo, false);
    accion(UI_ACCION_SONAR, SND_NUEVO);
}
/* cada cuadro: el sobre se mece junto a la mascota, con chispas, hasta que lo toques */
static void carta_mundo_animar(void)
{
    if (!s_carta_mundo || lv_obj_has_flag(s_carta_mundo, LV_OBJ_FLAG_HIDDEN)) return;
    if (s_panel != PANEL_NINGUNO || !s_pet || !pet_letter_pending(s_pet)) { ocultar(s_carta_mundo, true); return; }
    float t = (float)s_ms * 0.004f;
    int w = lv_obj_get_width(s_carta_mundo), h = lv_obj_get_height(s_carta_mundo);
    int x = (int)s_mascota.x + (s_mascota.x < 233 ? 56 : -56), y = (int)s_mascota.y - 70 + (int)lroundf(sinf(t) * 5.0f);
    lv_obj_set_pos(s_carta_mundo, x - w / 2, y - h / 2);
    lv_obj_move_foreground(s_carta_mundo);
    static uint32_t ult;
    if (s_ms - ult > 700) { ult = s_ms; chispa(x + (int)(sinf(t * 3.0f) * 24), y - 18, 0xFFE27A, 0, -1); }
}
/* la botella con mensaje: llega nadando (se mece, da vueltitas y deja ondas), la mascota la recoge de un salto, se abre y se lee */
static lv_obj_t *s_bot_img;
static int s_bot_fase = 0, s_bot_frase;
static uint32_t s_bot_t0, s_bot_onda;
static float s_bot_x0, s_bot_x1;
static void botella_nada(int f)
{
    if (!s_bot_img) {
        s_bot_img = lv_image_create(s_mundo);
        lv_image_set_src(s_bot_img, art_icono(ICO_OBJETO(OBJ_BOTELLA), 6));
        lv_obj_remove_flag(s_bot_img, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_transform_pivot_x(s_bot_img, lv_pct(50), 0);
        lv_obj_set_style_transform_pivot_y(s_bot_img, lv_pct(50), 0);
    }
    s_bot_fase = 1; s_bot_frase = f; s_bot_t0 = s_ms; s_bot_onda = 0;
    s_bot_x0 = s_mascota.x < CX ? (float)W + 30.0f : -30.0f;
    s_bot_x1 = s_mascota.x + (s_mascota.x < CX ? 46.0f : -46.0f);
    ocultar(s_bot_img, false);
    lv_obj_move_foreground(s_bot_img);
    accion(UI_ACCION_SONAR, SND_PLOP);
}
static void botella_animar(void)
{
    if (!s_bot_fase) return;
    const uint32_t t = s_ms - s_bot_t0;
    const uint32_t LLEGA = 3800;
    if (s_bot_fase == 1) {
        float k = t >= LLEGA ? 1.0f : (float)t / (float)LLEGA;
        float e = 1.0f - (1.0f - k) * (1.0f - k); /* va frenando al llegar */
        float x = s_bot_x0 + (s_bot_x1 - s_bot_x0) * e;
        float y = s_mascota.y + 20 + sinf((float)t / 260.0f) * 5.0f;
        int w = lv_obj_get_width(s_bot_img), h = lv_obj_get_height(s_bot_img);
        lv_obj_set_pos(s_bot_img, (int)x - w / 2, (int)y - h / 2);
        lv_obj_set_style_transform_rotation(s_bot_img, (int)(sinf((float)t / 330.0f) * 220.0f) + 600, 0); /* de costado, cabeceando */
        if (s_ms - s_bot_onda > 420) { s_bot_onda = s_ms; onda((int)x, (int)y + h / 3, 0xBFE6FF, 20, true, 0); }
        if (t >= LLEGA) {
            s_bot_fase = 2; s_bot_t0 = s_ms;
            mascota_modo(M_SALTA, 900);
            flotar(ICO_ESTRELLA, NULL, 0, (int)x, (int)y - 30, 0);
            accion(UI_ACCION_SONAR, SND_ALEGRE);
        }
    } else if (s_bot_fase == 2 && t > 1100) { /* la recoge: se abre y se lee */
        s_bot_fase = 0;
        ocultar(s_bot_img, true);
        if (s_pecera || s_panel != PANEL_NINGUNO) globo(ICO_OBJETO(OBJ_BOTELLA), pet_phrase(s_bot_frase), 6000);
        else { s_frase_botella = true; panel_frase(s_bot_frase); }
    }
}
/* regar: la mascota lleva la regadera hasta la planta, la inclina sobre ella y cae agua; luego la devuelve */
static lv_obj_t *s_rg_img;
static int s_rg_fase = 0, s_rg_i;
static bool s_rg_cita;
static void cita_riego_aplicar(void); /* más abajo */
static void cita_riego_fin(void);
#define RG_TIPX 27   /* la boquilla, ya inclinada, queda a la derecha y un poco abajo del centro */
#define RG_TIPY 12
#define RG_ROT 550   /* 55° hacia delante: la boquilla apunta abajo */
static uint32_t s_rg_t0, s_rg_gota;
static float s_rg_ax, s_rg_ay, s_rg_px, s_rg_py;
static void regadera_empezar(int i, int px, int py)
{
    if (s_rg_fase) return;
    s_rg_cita = i < 0; /* i<0: la cita del amigo, riega la semilla de la escena */
    if (!s_rg_img) {
        s_rg_img = lv_image_create(s_mundo);
        lv_image_set_src(s_rg_img, art_icono(ICO_REGADERA, 5));
        lv_obj_remove_flag(s_rg_img, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_transform_pivot_x(s_rg_img, lv_pct(50), 0);
        lv_obj_set_style_transform_pivot_y(s_rg_img, lv_pct(50), 0);
    }
    s_rg_fase = 1; s_rg_i = i; s_rg_t0 = s_ms; s_rg_gota = 0;
    s_rg_ax = s_mascota.x + (px < (int)s_mascota.x ? -26.0f : 26.0f); s_rg_ay = s_mascota.y - 26.0f;
    s_rg_px = (float)(px - RG_TIPX); s_rg_py = (float)(py - 46 - RG_TIPY); /* la boquilla queda sobre la planta */
    ocultar(s_rg_img, false);
    lv_obj_move_foreground(s_rg_img);
    mascota_modo(M_LIBRE, 100);
}
static void regadera_animar(void)
{
    if (!s_rg_fase) return;
    const uint32_t t = s_ms - s_rg_t0;
    float x, y;
    int rot = 0;
    /* aparece directo sobre la planta (sin viajar), la inclina, la riega y se desvanece ahí mismo */
    x = s_rg_px; y = s_rg_py;
    int opa = 255;
    if (t < 300) opa = (int)(255.0f * (float)t / 300.0f);
    else if (t < 2000) {
        float k = (float)(t - 300) / 400.0f; if (k > 1) k = 1;
        rot = (int)(k * RG_ROT) + (t > 700 ? (int)(sinf((float)t / 70.0f) * 15.0f) : 0);
        if (s_rg_fase == 1 && t >= 500) { s_rg_fase = 2; if (s_rg_cita) cita_riego_aplicar(); else accion(ACT_REGAR, s_rg_i); }
        if (t >= 500 && t < 1700 && s_ms - s_rg_gota > 90) {
            s_rg_gota = s_ms;
            salpicar((int)s_rg_px + RG_TIPX - 4 + (int)(azar() % 9), (int)s_rg_py + RG_TIPY + 6 + (int)(azar() % 30), 0x7CC8FF); /* cae desde la boquilla */
        }
        if (t >= 1700) { opa = (int)(255.0f * (float)(2000 - t) / 300.0f); rot = RG_ROT; }
    } else { s_rg_fase = 0; lv_obj_set_style_opa(s_rg_img, 255, 0); ocultar(s_rg_img, true); if (s_rg_cita) cita_riego_fin(); return; }
    lv_obj_set_style_opa(s_rg_img, (lv_opa_t)(opa < 0 ? 0 : opa > 255 ? 255 : opa), 0);
    int w = lv_obj_get_width(s_rg_img), h = lv_obj_get_height(s_rg_img);
    lv_obj_set_pos(s_rg_img, (int)x - w / 2, (int)y - h / 2);
    lv_obj_set_style_transform_rotation(s_rg_img, rot, 0);
}
static void mensajero_nueva(void) { s_men_entregado = false; if (s_men_fase) mensajero_fin(); }
#define VENTANA_X 318        /* la ventana redonda de la casita por dentro */
#define VENTANA_Y 108

static art_icono_t men_icono(int tipo, int cuadro)
{
    switch (tipo) {
    case MENS_PALOMA: return cuadro ? ICO_PALOMA_B : ICO_PALOMA;
    case MENS_BUHO: return ICO_BUHO;
    case MENS_MARIPOSA: return cuadro ? ICO_MARIPOSA_B : ICO_MARIPOSA;
    case MENS_GRULLA: return ICO_GRULLA;
    case MENS_HOJA: return ICO_HOJA;
    case MENS_CHISPA: return ICO_ESTRELLA;
    case MENS_BURBUJA: return ICO_BURBUJA;
    case MENS_VIENTO: return ICO_VIENTO;
    case MENS_CRISTAL: return ICO_CRISTAL;
    default: return ICO_CONCHA;
    }
}
static bool men_es_sena(int tipo) { return tipo >= MENS_HOJA; } /* la seña de una criatura: se vuelve carta */
static int men_escala(int tipo) { return tipo == MENS_MARIPOSA || tipo == MENS_GRULLA ? 4 : 3; }

static void mensajero_crear(void)
{
    if (s_men) return;
    s_men = lv_image_create(s_mundo);
    s_men_sobre = lv_image_create(s_mundo);
    lv_image_set_src(s_men_sobre, art_icono(ICO_CARTA, 3));
    ocultar(s_men, true);
    ocultar(s_men_sobre, true);
}
static void mensajero_fin(void)
{
    s_men_fase = 0;
    if (s_men) { ocultar(s_men, true); ocultar(s_men_sobre, true); }
}
static void mensajero_empezar(const pet_t *p, uint32_t utc)
{
    mensajero_crear();
    s_men_tipo = pet_letter_messenger(p, 0, utc);
    s_men_fase = 1;
    s_men_t = s_ms;
    /* dónde deja el sobre: adelante de ella (o en la ventana, si está en la casita) */
    if (s_interior) { s_men_dx = VENTANA_X; s_men_dy = VENTANA_Y + 34; }
    else {
        s_men_dx = s_mascota.x + (s_mascota.x < CX ? 70 : -70);
        s_men_dy = s_mascota.y + 6;
        limitar(&s_men_dx, &s_men_dy);
    }
    /* de dónde viene: por el cielo; en la cueva, por la boca de la izquierda; en la casita, por la ventana */
    if (s_escena == ESC_CUEVA && !s_interior) { s_men_x0 = -40; s_men_y0 = 230; }
    else if (s_interior) { s_men_x0 = VENTANA_X + 60; s_men_y0 = VENTANA_Y - 40; }
    else { s_men_x0 = s_men_dx < CX ? W + 40 : -40; s_men_y0 = 60; }
    if (men_es_sena(s_men_tipo) && s_men_tipo == MENS_CRISTAL) { s_men_x0 = s_men_dx; s_men_y0 = s_men_dy + 30; } /* el cristal brota del suelo */
    ocultar(s_men, false);
    ocultar(s_men_sobre, true);
    lv_obj_set_style_opa(s_men, LV_OPA_COVER, 0);
    accion(UI_ACCION_SONAR, men_es_sena(s_men_tipo) ? SND_RARO : SND_ALETEO);
}
static void animar_mensajero(void)
{
    if (!s_men_fase || !s_men) return;
    const pet_t *p = s_pet;
    if (!p || p->dormida || !pet_letter_pending(p)) { mensajero_fin(); return; }
    uint32_t t = s_ms - s_men_t;
    float x, y;
    int cuadro = (int)(s_ms / 160) % 2;
    const float ty = s_men_dy - (s_interior ? 10 : 70);
    if (s_men_fase == 1) { /* llega volando */
        float k = t > 2600 ? 1.0f : (float)t / 2600.0f;
        float e = k * k * (3 - 2 * k);
        x = s_men_x0 + (s_men_dx - s_men_x0) * e;
        y = s_men_y0 + (ty - s_men_y0) * e - sinf(k * 3.1416f) * (s_escena == ESC_CUEVA ? 10 : 40);
        if (s_men_tipo == MENS_HOJA) x += sinf((float)t / 300.0f) * 26; /* la hoja cae meciéndose */
        if (s_men_tipo == MENS_BURBUJA) y += sinf((float)t / 200.0f) * 6;
        bool izq = s_men_x0 > s_men_dx && k < 1;
        lv_image_set_src(s_men, izq ? art_icono_espejo(men_icono(s_men_tipo, cuadro), men_escala(s_men_tipo)) : art_icono(men_icono(s_men_tipo, cuadro), men_escala(s_men_tipo)));
        lv_obj_set_pos(s_men, (int)x - 18, (int)y - 18);
        lv_obj_move_foreground(s_men);
        if (t >= 2600) { s_men_fase = 2; s_men_t = s_ms; }
        return;
    }
    if (s_men_fase == 2) { /* deja el sobre: los pájaros se van; la grulla y las señas se vuelven carta */
        float k = t > 600 ? 1.0f : (float)t / 600.0f;
        ocultar(s_men_sobre, false);
        int sy = (int)(ty + (s_men_dy - ty) * k * k);
        if (men_es_sena(s_men_tipo) || s_men_tipo == MENS_GRULLA) {
            if (s_men_tipo == MENS_GRULLA && t < 500) { lv_image_set_src(s_men, art_icono(ICO_GRULLA_ABRE, 4)); ocultar(s_men_sobre, true); }
            else { ocultar(s_men, true); if (t < 700 && t % 160 < TICK_MS) flotar(ICO_ESTRELLA, NULL, 0, (int)s_men_dx, (int)s_men_dy - 30, 0); }
            sy = (int)s_men_dy;
        } else {
            float k2 = (float)t / 2200.0f;
            x = s_men_dx + (s_men_x0 < s_men_dx ? 1 : -1) * k2 * 300;
            y = ty - k2 * 220;
            lv_image_set_src(s_men, s_men_x0 < s_men_dx ? art_icono(men_icono(s_men_tipo, cuadro), men_escala(s_men_tipo)) : art_icono_espejo(men_icono(s_men_tipo, cuadro), men_escala(s_men_tipo)));
            lv_obj_set_pos(s_men, (int)x - 18, (int)y - 18);
            if (t > 2200) ocultar(s_men, true);
        }
        lv_obj_set_pos(s_men_sobre, (int)s_men_dx - 18, sy - 30);
        lv_obj_move_foreground(s_men_sobre);
        if (t >= 900) { /* ella va a buscarlo */
            s_men_fase = 3; s_men_t = s_ms;
            s_mascota.tx = s_men_dx + (s_men_dx < s_mascota.x ? 30 : -30);
            s_mascota.ty = s_men_dy;
            limitar(&s_mascota.tx, &s_mascota.ty);
            mascota_modo(M_VA, 7000);
        }
        return;
    }
    if (s_men_fase == 3) {
        bool llego = hypotf(s_mascota.x - s_mascota.tx, s_mascota.y - s_mascota.ty) < 6 || t > 6500;
        if (t > 2200 && !s_men_tipo) ocultar(s_men, true);
        if (!llego) return;
        ocultar(s_men_sobre, true);
        ocultar(s_men, true);
        lv_image_set_src(s_objeto, art_icono(ICO_CARTA, 3));
        ocultar(s_objeto, false);
        mascota_modo(M_TRAE, 3600);
        accion(UI_ACCION_SONAR, SND_ALEGRE);
        s_men_fase = 4; s_men_t = s_ms;
        return;
    }
    if (s_men_fase == 4 && (s_mascota.modo != M_TRAE || t > 4200)) s_men_fase = 5;
    if (s_men_fase == 5 && s_panel == PANEL_NINGUNO) { /* te la trae: ahora sí, el sobre grande */
        mensajero_fin();
        ocultar(s_objeto, true);
        s_men_entregado = true;
        carta_mundo_poner(); /* la deja a su lado, esperando: se abre cuando tú quieras */
    }
}
/* lo llama ui_update: ¿llegó una carta? (espera a que la pantalla quede libre) */
static bool pantalla_libre(const pet_t *p)
{
    return s_panel == PANEL_NINGUNO && !s_pecera && !s_bienv_activa && !p->dormida && s_juego == JG_NADA && !s_cru_activo && !s_via_activo &&
           !s_cie_activo && !s_rito_on && !s_baile_on && p->edad_dueno && !s_cita_on && !s_com_on && !s_sim_on && s_fie_tipo < 0 && s_pantalla_visible;
}
static void mensajero_revisar(const pet_t *p)
{
    if (!pet_letter_pending(p)) { s_men_entregado = false; if (s_men_fase) mensajero_fin(); return; }
    if (s_men_fase || !pantalla_libre(p)) return;
    if (!s_men_entregado) { if (s_mascota.modo == M_LIBRE || s_mascota.modo == M_CAMINA || s_mascota.modo == M_QUIETA) mensajero_empezar(p, s_ultimo_t); }
    else if (!s_carta_mundo || lv_obj_has_flag(s_carta_mundo, LV_OBJ_FLAG_HIDDEN)) carta_mundo_poner(); /* la dejaste sin abrir: vuelve a esperarte */
}

/* ======================================================================
 * Las criaturas mágicas en el mundo
 * ==================================================================== */
static const art_icono_t ICO_CRIATURA[CRI_COUNT] = {ICO_MUSGUITO, ICO_LUMBRE, ICO_ONDINA, ICO_VENTOLIN, ICO_CRISTALINO, ICO_MAREA};
#define FIN_COLOR_ORO_HIST 0xFFD76A
static art_icono_t icono_narrador(int c, int k)
{
    if ((unsigned)c < CRI_COUNT) return ICO_CRIATURA[c];
    switch (c) {
    case CRI_NIMBA: return ICO_NIMBA;
    case NARR_PRISMA: return ICO_PRISMA;
    case NARR_TILDE: return ICO_TILDE;
    case NARR_BRIZNA: return ICO_BRIZNA;
    case NARR_TILO: return ICO_PAGINA_TILO;
    case NARR_PAPELITOS: return ICO_PAPELITO; /* (fw 104) */
    case NARR_ANIMALES: { const int f = pet_animal_phrase_card(k); return f < 0 ? ICO_PAJARO : (f < FAUNA_COUNT ? ICO_FAUNA[f] : ICO_ANIMAL[f - FAUNA_COUNT]); }
    default: return ICO_LIBRO;
    }
}
#define ICO_NARRADOR(c) icono_narrador((c), 0) /* Nimba, Prisma, Tilde, la Abuela Brizna y las páginas de Tilo también cuentan */
static bool modo_nino_lee(void) { return s_pet && s_pet->modo_nino == MODO_NINO; } /* (v37) en modo niño, la versión corta */
static const char *historia_texto(int c, int k) { return modo_nino_lee() ? pet_story_short(c, k) : pet_story_text((magica_t)c, k); }
static const uint32_t LUZ_CRIATURA[CRI_COUNT] = {0x8ACC60, 0xFFE27A, 0x8FD8FF, 0xC8B0F0, 0xA080F0, 0x7FE0D6};
static const art_icono_t ICO_CITA[CITA_COUNT] = {ICO_MANTEL, ICO_BROTE, ICO_ESCONDIDAS, ICO_LUMBRE, ICO_CANA, ICO_BARQUITO_HOJA,
                                                 ICO_COMETA, ICO_HOJA_SECA, ICO_NOTA, ICO_COFRE, ICO_ESTRELLA, ICO_FARO};
static lv_obj_t *s_cri, *s_cri_luz, *s_cri_burbuja;
static int s_cri_cual = -1;
static float s_cri_x, s_cri_y;
static uint32_t s_cri_desde;
static void cita_empezar(int que);
static void criatura_lugar(int c, float *x, float *y)
{
    switch (c) {
    case CRI_MUSGUITO: *x = LXY(LUGAR_ARBOL)[0] + 46; *y = LXY(LUGAR_ARBOL)[1] + 20; break;
    case CRI_LUMBRE: *x = LXY(LUGAR_CASITA)[0] + 40; *y = LXY(LUGAR_CASITA)[1] - 10; break;
    case CRI_ONDINA: *x = LXY(LUGAR_ESTANQUE)[0] + 70; *y = LXY(LUGAR_ESTANQUE)[1] - 4; break;
    case CRI_VENTOLIN: *x = LXY(LUGAR_ROCAS)[0] - 20; *y = LXY(LUGAR_ROCAS)[1] - 50; break;
    case CRI_CRISTALINO: *x = 112; *y = 330; break; /* en la cueva, abajo a un lado: no tapa ningún cristal (se tocan todos) */
    default: *x = LXY(LUGAR_CASITA)[0] - 50; *y = LXY(LUGAR_CASITA)[1] + 20; break; /* Marea, junto al faro */
    }
    limitar(x, y);
}
static void criatura_cb(lv_event_t *e)
{
    (void)e;
    if (!s_pet || s_cita_on) return;
    if (pet_creature_waiting(s_pet, s_ultimo_t)) { cita_empezar(pet_creature_invite(s_pet)); return; }
    accion(UI_ACCION_CRIATURA, 0);
    for (int k = 0; k < 3; k++) flotar(ICO_CORAZON, NULL, 0, (int)s_cri_x - 20 + k * 20, (int)s_cri_y - 50, (uint32_t)k * 150);
}
static void animar_criatura(void)
{
    const pet_t *p = s_pet;
    if (!s_cri) {
        s_cri_luz = caja(s_mundo);
        lv_obj_set_size(s_cri_luz, 40, 40);
        lv_obj_set_style_radius(s_cri_luz, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(s_cri_luz, LV_OPA_40, 0);
        lv_obj_set_style_shadow_width(s_cri_luz, 30, 0);
        lv_obj_set_style_shadow_opa(s_cri_luz, LV_OPA_70, 0);
        lv_obj_remove_flag(s_cri_luz, LV_OBJ_FLAG_CLICKABLE);
        s_cri = lv_image_create(s_mundo);
        lv_obj_add_flag(s_cri, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_ext_click_area(s_cri, 4);
        lv_obj_add_event_cb(s_cri, criatura_cb, LV_EVENT_CLICKED, NULL);
        s_cri_burbuja = lv_image_create(s_mundo);
        ocultar(s_cri, true); ocultar(s_cri_luz, true); ocultar(s_cri_burbuja, true);
    }
    magica_t c = CRI_MUSGUITO;
    bool ver = p && pet_creature_here(p, s_ultimo_t, &c) && s_escena == (int)p->escena && !s_interior && p->etapa != STAGE_HUEVO;
    if (ver && (int)c != s_cri_cual) {
        s_cri_cual = c;
        s_cri_desde = s_ms;
        criatura_lugar(c, &s_cri_x, &s_cri_y);
        lv_image_set_src(s_cri, art_icono(ICO_CRIATURA[c], 4));
        lv_obj_set_style_bg_color(s_cri_luz, lv_color_hex(LUZ_CRIATURA[c]), 0);
        lv_obj_set_style_shadow_color(s_cri_luz, lv_color_hex(LUZ_CRIATURA[c]), 0);
        for (int k = 0; k < 4; k++) flotar(ICO_ESTRELLA, NULL, 0, (int)s_cri_x - 30 + k * 20, (int)s_cri_y - 30, (uint32_t)k * 120);
    }
    if (!ver) {
        if (s_cri_cual >= 0) for (int k = 0; k < 3; k++) flotar(ICO_ESTRELLA, NULL, 0, (int)s_cri_x - 20 + k * 20, (int)s_cri_y - 30, (uint32_t)k * 100);
        s_cri_cual = -1;
        ocultar(s_cri, true); ocultar(s_cri_luz, true); ocultar(s_cri_burbuja, true);
        return;
    }
    /* flota suavecito; aparece de a poco */
    uint32_t edad = s_ms - s_cri_desde;
    float oy = sinf((float)s_ms / 500.0f) * 5, ox = 0;
    if (c == CRI_LUMBRE) { ox = sinf((float)s_ms / 900.0f) * 18; oy = sinf((float)s_ms / 350.0f) * 8; }
    if (c == CRI_VENTOLIN) ox = sinf((float)s_ms / 700.0f) * 14;
    int x = (int)(s_cri_x + ox), y = (int)(s_cri_y + oy);
    lv_opa_t op = edad > 800 ? LV_OPA_COVER : (lv_opa_t)(edad * 255 / 800);
    ocultar(s_cri, false); ocultar(s_cri_luz, false);
    lv_obj_set_style_opa(s_cri, op, 0);
    lv_obj_set_style_opa(s_cri_luz, (lv_opa_t)(op * (90 + 50 * sinf((float)s_ms / 300.0f)) / 255), 0);
    lv_obj_set_pos(s_cri, x - 24, y - 48);
    lv_obj_set_pos(s_cri_luz, x - 20, y - 44);
    lv_image_set_src(s_cri, s_mascota.x < x ? art_icono_espejo(ICO_CRIATURA[c], 4) : art_icono(ICO_CRIATURA[c], 4)); /* la mira */
    bool espera = pet_creature_waiting(p, s_ultimo_t) && !s_cita_on;
    ocultar(s_cri_burbuja, !espera);
    if (espera) { /* una burbujita con lo que quiere hacer: invita a tocarla */
        lv_image_set_src(s_cri_burbuja, art_icono(ICO_CITA[pet_creature_invite(p)], 2));
        lv_obj_set_pos(s_cri_burbuja, x + 14, y - 74 + (int)(sinf((float)s_ms / 250.0f) * 3));
        lv_obj_move_foreground(s_cri_burbuja);
    }
    lv_obj_move_foreground(s_cri_luz);
    lv_obj_move_foreground(s_cri);
}

/* --- la historia que cuenta: la hoja con su dibujo, el título y el texto (si es larga, se desliza) --- */
static int s_hist_pend_c = -1, s_hist_pend_k;
static int s_tuto_fase; /* enseñar a abrir el libro: 0 nada, 1 espera la primera lectura, 2 la lee, 3 se muestra la flecha */
static lv_obj_t *s_tuto_capa;
static uint32_t s_tuto_fin;
/* una hoja para leer: el dibujo, quién lo cuenta, el título y el texto (si es largo, se desliza) */
/* una lectura: si el texto no cabe en una vista, se parte en hojas y al tocar la pantalla sigue con el resto */
static art_icono_t s_lect_ico;
static int s_lect_fin_sig; /* dónde empieza la hoja siguiente */
static EXT_RAM_BSS_ATTR char s_lect_de[96], s_lect_tit[96], s_lect_cuerpo[640];
static int s_lect_ini; /* dónde empieza la hoja que se ve (en s_lect_cuerpo) */
static void panel_lectura_hoja(void);
static bool s_fin_libro7; /* la historia del rito: al terminar el cierre, aparece la última página del Libro */
static int lect_alto_w(const char *txt, int n, int w) /* cuánto mide ese pedazo de texto ya dibujado */
{
    char tmp[640];
    if (n > (int)sizeof tmp - 1) n = (int)sizeof tmp - 1;
    memcpy(tmp, txt, (size_t)n);
    tmp[n] = 0;
    mayusculas(tmp);
    art_img_t im;
    memset(&im, 0, sizeof im);
    art_texto(&im, tmp, C_TINTA, 2, w);
    const int h = (int)im.dsc.header.h;
    art_img_free(&im);
    return h;
}
static int lect_alto(const char *txt, int n) { return lect_alto_w(txt, n, 262); }
static int s_lect_narr = -1, s_lect_k; /* (v37) un narrador sin escena: Prisma, Tilde, Brizna, las páginas de Tilo, los animales */
static int s_lect_esc = -1; /* la escena animada de la historia (criatura*5 + cuál); -1 = lectura sin escena */
static void hist_hoja(void);
static bool s_lib_desde, s_lib_pide; /* la historia se abrió desde el libro: al terminar, se toca para volver a él */
static void lib_vuelve_cb(lv_event_t *e);
static lv_obj_t *s_hist_ar;
static void fin_aparecer_hist(lv_obj_t *o, uint32_t retraso);
static void lect_opa_cb(void *var, int32_t v) { lv_obj_set_style_opa((lv_obj_t *)var, (lv_opa_t)v, 0); }
static void lectura_cb(lv_event_t *e)
{
    (void)e;
    if (s_panel != PANEL_HISTORIA) return;
    s_lect_ini = s_lect_fin_sig;
    panel_lectura_hoja();
}
static void panel_lectura_hoja(void)
{
    if (s_lect_esc >= 0) { hist_hoja(); return; }
    abrir_tarjeta(PANEL_HISTORIA, 360, 384);
    lv_obj_t *ic = icono(s_tarjeta, s_lect_ico, 4);
    lv_obj_align(ic, LV_ALIGN_TOP_MID, 0, 14);
    char t[96];
    snprintf(t, sizeof t, "%s", s_lect_de);
    mayusculas(t);
    lv_obj_t *dt = texto(s_tarjeta, t, C_GRIS, 2, 280);
    lv_obj_align(dt, LV_ALIGN_TOP_MID, 0, 66);
    lv_obj_t *ar = area_texto(s_tarjeta, 34, 90, 292, 250);
    lv_obj_set_style_pad_row(ar, 10, 0);
    if (s_lect_ini == 0) {
        snprintf(t, sizeof t, "%s", s_lect_tit);
        mayusculas(t);
        texto(ar, t, C_VIOLETA, 2, 270);
    }
    /* cuánto texto cabe en esta hoja: lo más que entre sin cortar una palabra */
    const char *resto = s_lect_cuerpo + s_lect_ini;
    const int total = (int)strlen(resto), disp = s_lect_ini == 0 ? 190 : 232;
    int n = total;
    if (lect_alto(resto, total) > disp) {
        int lo = 1, hi = total;
        while (lo < hi) { /* el mayor prefijo que cabe */
            const int mid = (lo + hi + 1) / 2;
            if (lect_alto(resto, mid) <= disp) lo = mid; else hi = mid - 1;
        }
        n = lo;
        while (n > 1 && resto[n] != ' ' && resto[n] != 0) n--; /* hasta el espacio anterior */
        if (n <= 1) n = lo;
    }
    char txt[640];
    memcpy(txt, resto, (size_t)n);
    txt[n] = 0;
    mayusculas(txt);
    texto(ar, txt, C_TINTA, 2, 262);
    lv_obj_remove_flag(ar, LV_OBJ_FLAG_SCROLLABLE);
    const bool sigue = n < total;
    s_lect_fin_sig = s_lect_ini + n + (resto[n] == ' ' ? 1 : 0);
    if (sigue) { /* "toca para seguir": una flechita que late abajo, y tocar en cualquier parte pasa de hoja */
        lv_obj_t *fl = icono(s_tarjeta, ICO_FLECHA, 2);
        lv_image_set_pivot(fl, 12, 12);
        lv_image_set_rotation(fl, 900);
        lv_obj_align(fl, LV_ALIGN_TOP_RIGHT, -56, 336); /* a un lado: no tapa la X */
        { lv_anim_t an; lv_anim_init(&an); lv_anim_set_var(&an, fl); lv_anim_set_values(&an, 90, 255); lv_anim_set_time(&an, 700); lv_anim_set_playback_time(&an, 700);
          lv_anim_set_repeat_count(&an, LV_ANIM_REPEAT_INFINITE); lv_anim_set_exec_cb(&an, lect_opa_cb); lv_anim_start(&an); }
        lv_obj_add_flag(ar, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(ar, lectura_cb, LV_EVENT_CLICKED, NULL);
        lv_obj_add_flag(s_tarjeta, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(s_tarjeta, lectura_cb, LV_EVENT_CLICKED, NULL);
    } else if (s_lib_desde) { /* la última hoja: tocar vuelve al libro */
        lv_obj_add_flag(s_tarjeta, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(s_tarjeta, lib_vuelve_cb, LV_EVENT_CLICKED, NULL);
    }
}
static void panel_lectura(art_icono_t ico, const char *de, const char *titulo, const char *cuerpo)
{
    s_lect_ico = ico;
    snprintf(s_lect_de, sizeof s_lect_de, "%s", de);
    snprintf(s_lect_tit, sizeof s_lect_tit, "%s", titulo);
    snprintf(s_lect_cuerpo, sizeof s_lect_cuerpo, "%s", cuerpo);
    s_lect_ini = 0;
    s_lect_esc = -1;
    s_lect_narr = -1;
    panel_lectura_hoja();
}
#include "ui_escenas.inc"
static void hist_opa_cb(void *var, int32_t v) { lv_obj_set_style_opa((lv_obj_t *)var, (lv_opa_t)v, 0); }
static void fin_aparecer_hist(lv_obj_t *o, uint32_t retraso)
{
    lv_obj_set_style_opa(o, LV_OPA_TRANSP, 0);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, o);
    lv_anim_set_values(&a, 0, 255);
    lv_anim_set_time(&a, 600);
    lv_anim_set_delay(&a, retraso);
    lv_anim_set_exec_cb(&a, hist_opa_cb);
    lv_anim_start(&a);
}
/* la hoja de una historia: fondo de noche, la escena animada arriba y el texto abajo (tocar sigue) */
static void hist_fondo(void)
{
    lv_obj_align(s_tarjeta, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_opa(s_tarjeta, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_tarjeta, 0, 0);
    lv_obj_set_style_shadow_width(s_tarjeta, 0, 0);
    lv_obj_t *f = lv_obj_create(s_tarjeta);
    lv_obj_remove_style_all(f);
    lv_obj_set_size(f, 466, 466);
    lv_obj_set_pos(f, 0, 0);
    lv_obj_set_style_radius(f, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(f, lv_color_hex(0x161B48), 0); /* (color liso: el degradado de 466x466 se recalculaba en cada cuadro de la escena animada) */
    lv_obj_set_style_bg_opa(f, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(f, 3, 0);
    lv_obj_set_style_border_color(f, lv_color_hex(0xD0B969), 0);
    lv_obj_remove_flag(f, LV_OBJ_FLAG_CLICKABLE);
    for (int i = 0; i < 26; i++) {
        lv_obj_t *e = lv_obj_create(f);
        lv_obj_remove_style_all(e);
        const int d = 2 + (int)(azar() % 2), y = 20 + (int)(azar() % 430), w = zona_ancho(y) - 40;
        if (w < 24) { lv_obj_delete(e); continue; }
        lv_obj_set_size(e, d, d);
        lv_obj_set_pos(e, CX - w / 2 + (int)(azar() % (uint32_t)w), y);
        lv_obj_set_style_bg_color(e, lv_color_hex(0xFFF6D0), 0);
        lv_obj_set_style_bg_opa(e, (lv_opa_t)(70 + azar() % 120), 0);
        lv_obj_remove_flag(e, LV_OBJ_FLAG_CLICKABLE);
    }
}
/* (v96) la hoja para leer: el narrador arriba (quieto) y el texto grande en casi todo el disco. Se quitó la escena animada de
 * aquí (ocupaba la mitad de arriba y el texto quedaba en una franja chica, en letra chica). La letra va 1,3 veces más grande
 * que la normal; con «Texto grande» en los ajustes, todavía más. Si no cabe, se desliza hacia arriba con el dedo. */
static lv_obj_t *texto_libro(lv_obj_t *pa, const char *s, uint32_t col, int w)
{
    art_texto_factor(s_fuente_grande ? 1.05f : 1.3f); /* (con «Texto grande» va a escala 3: ya es grande) */
    lv_obj_t *o = texto_l(pa, s, col, 2, w);
    art_texto_factor(1.05f);
    return o;
}
static void hist_hoja(void)
{
    const int c = s_lect_narr >= 0 ? s_lect_narr : s_lect_esc / 5, k = s_lect_narr >= 0 ? s_lect_k : s_lect_esc % 5;
    abrir_tarjeta(PANEL_HISTORIA, 466, 466);
    hist_fondo();
    char t[96];
    const bool papel = c == NARR_TILO || c == NARR_PAPELITOS; /* (fw 104) los papelitos también son papel viejo con tinta café */
    if (c == NARR_TILO) snprintf(t, sizeof t, "%s", pet_final_seen(s_pet, PET_FINAL_SECRETO) ? "Diario de Peumo" : "Página suelta");
    else if (c == NARR_ANIMALES) snprintf(t, sizeof t, "Los animales cuentan");
    else if (c == NARR_PAPELITOS) { snprintf(t, sizeof t, "%s", s_papel_trajo ? "¡Te lo trajo sin leerlo!" : "Un papelito del bosque"); s_papel_trajo = false; }
    else if (c == NARR_BRIZNA) snprintf(t, sizeof t, "Abuela Brizna");
    else snprintf(t, sizeof t, "%s te cuenta", pet_narr_name(c));
    mayusculas(t);
    lv_obj_t *ic = icono(s_tarjeta, icono_narrador(c, k), 5);
    lv_obj_align(ic, LV_ALIGN_TOP_MID, 0, 26);
    lv_obj_t *dt = texto_c(s_tarjeta, t, 0xC9B98A, 2, 260);
    lv_obj_align(dt, LV_ALIGN_TOP_MID, 0, 92);
    if (papel) { /* la página escrita a mano: papel viejo y tinta café */
        lv_obj_t *pa = lv_obj_create(s_tarjeta);
        lv_obj_remove_style_all(pa);
        lv_obj_set_size(pa, 336, 262);
        lv_obj_set_pos(pa, 65, 116);
        lv_obj_set_style_radius(pa, 18, 0);
        lv_obj_set_style_bg_color(pa, lv_color_hex(0xF6E8C4), 0);
        lv_obj_set_style_bg_opa(pa, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(pa, 2, 0);
        lv_obj_set_style_border_color(pa, lv_color_hex(0xB8925A), 0);
        lv_obj_remove_flag(pa, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    }
    const int AW = 320, AH = 252; /* (termina antes de la X: no la tapa) */
    lv_obj_t *ar = area_texto(s_tarjeta, CX - (AW + 8) / 2, 122, AW + 8, AH);
    lv_obj_set_style_pad_row(ar, 10, 0);
    snprintf(t, sizeof t, "%s", s_lect_tit);
    mayusculas(t);
    lv_obj_t *tt = texto_libro(ar, t, papel ? 0x8A5A30 : 0xFFD76A, AW);
    lv_obj_set_style_text_align(tt, LV_TEXT_ALIGN_CENTER, 0);
    char txt[700];
    snprintf(txt, sizeof txt, "%s", s_lect_cuerpo);
    mayusculas(txt);
    texto_libro(ar, txt, papel ? 0x4A3020 : 0xE4ECFF, AW);
    lv_obj_update_layout(ar);
    lv_obj_set_scrollbar_mode(ar, LV_SCROLLBAR_MODE_OFF); /* si no cabe, se desliza el texto (sin barra ni flecha) */
    s_hist_ar = ar;
    lv_obj_add_flag(ar, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(ar, lib_vuelve_cb, LV_EVENT_CLICKED, NULL); /* al final del texto, tocar sale */
    lv_obj_add_flag(s_tarjeta, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_tarjeta, lib_vuelve_cb, LV_EVENT_CLICKED, NULL);
}
static int hist_total(void)
{
    int n = 0;
    if (s_pet) for (int c = 0; c < NARR_COUNT; c++) n += pet_story_count(s_pet, c);
    return n;
}
static void lib_hist_cb(lv_event_t *e)
{
    const int q = (int)(intptr_t)lv_event_get_user_data(e);
    s_lib_pide = true;
    panel_historia(q / 64, q % 64);
}
static void panel_libro_indice(void);
static void lib_nada_cb(lv_event_t *e) { (void)e; }
static void lib_vuelve_cb(lv_event_t *e)
{
    (void)e;
    if (s_panel != PANEL_HISTORIA || s_lect_esc < 0) return;
    if (s_hist_ar && lv_obj_is_valid(s_hist_ar) && lv_obj_get_scroll_bottom(s_hist_ar) > 2) return; /* aún queda texto: solo se desliza */
    if (s_lib_desde) panel_libro_indice(); else cerrar_panel();
}
static int s_lib_narr;
static void lib_abre_cb(lv_event_t *e) { (void)e; if (s_panel == PANEL_LIBHIST) { s_lib_narr = -1; panel_libro_indice(); } }
static void lib_ceremonia_cb(lv_event_t *e) { const int i = (int)(intptr_t)lv_event_get_user_data(e); cer_empezar(i == 0 ? 1 : 2, true); }
/* 1) la portada: el libro brillando; tocarlo lo abre */
static void panel_libro_hist(void)
{
    abrir_tarjeta(PANEL_LIBHIST, 466, 466);
    hist_fondo();
    lv_obj_t *cc = lv_obj_create(s_tarjeta); /* el contenido, centrado en el disco */
    lv_obj_remove_style_all(cc);
    lv_obj_set_size(cc, 380, 380);
    lv_obj_set_pos(cc, 43, 43);
    lv_obj_remove_flag(cc, LV_OBJ_FLAG_CLICKABLE);
    if (!s_pet) return;
    s_lib_desde = false;
    /* el resplandor: tres discos planos y transparentes, sin sombra difuminada ni animación (la sombra de 60 px animada obligaba a repintar el libro grande en cada cuadro y el toque tardaba una eternidad) */
    lv_obj_t *glow = NULL;
    for (int i = 0; i < 3; i++) {
        const int d = 220 - i * 50;
        lv_obj_t *g = lv_obj_create(cc);
        lv_obj_remove_style_all(g);
        lv_obj_set_size(g, d, d);
        lv_obj_align(g, LV_ALIGN_TOP_MID, 0, 125 - d / 2);
        lv_obj_set_style_radius(g, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(g, lv_color_hex(i == 2 ? 0x7048D8 : 0x4A32A0), 0);
        lv_obj_set_style_bg_opa(g, (lv_opa_t)(40 + i * 35), 0);
        lv_obj_remove_flag(g, LV_OBJ_FLAG_CLICKABLE);
        if (i == 2) glow = g;
    }
    lv_obj_add_flag(glow, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(glow, 60);
    lv_obj_add_event_cb(glow, lib_abre_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *libro = icono(cc, ICO_LIBRO, 13);
    lv_obj_align(libro, LV_ALIGN_TOP_MID, 0, 56);
    lv_obj_remove_flag(libro, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_t *t = texto_c(cc, "LIBRO DE HISTORIAS", 0xFFE9A0, 2, 320);
    lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 238);
    if (pet_final_seen(s_pet, PET_FINAL_SECRETO)) { /* (v37) completo: el nombre, escrito a mano en la tapa */
        lv_obj_t *n = texto(cc, "PEUMO", FIN_COLOR_ORO_HIST, 4, 0);
        lv_obj_align(n, LV_ALIGN_TOP_MID, 0, 120);
        lv_image_set_rotation(n, -50);
    }
    if (!s_nino) {
        lv_obj_t *sb = texto_c(cc, "TOCA EL LIBRO PARA ABRIRLO", 0xC8C4E0, 2, 300);
        lv_obj_align(sb, LV_ALIGN_TOP_MID, 0, 270);
    }
    /* (v39) volver a ver las ceremonias (ahí sí con la X) */
    const bool rito = s_pet->guardiana, verso = pet_final_seen(s_pet, PET_FINAL_SECRETO);
    for (int i = 0; i < 2; i++) {
        if (!(i == 0 ? rito : verso)) continue;
        lv_obj_t *b = boton(cc, 140, 40, lib_ceremonia_cb, i);
        lv_obj_align(b, LV_ALIGN_TOP_MID, rito && verso ? (i == 0 ? -76 : 76) : 0, 306);
        ficha_magica(b, 0x7A5AD8, 0x2A1A6A, 20);
        if (s_nino) lv_obj_center(icono(b, i == 0 ? ICO_MEDIO_CORAZON : ICO_NOTA, 2));
        else lv_obj_center(texto_c(b, i == 0 ? "EL RITO" : "EL ÚLTIMO VERSO", 0xFFF3C8, 2, 130));
    }
}
/* 2) el índice (v39): primero los narradores que ya contaron algo (un círculo con su dibujo y su nombre); al tocar uno, sus historias.
 *    Solo aparece lo ya encontrado: nada de páginas selladas ni de cuántas faltan (no se revela que hay más historias ni más personajes). */
static int s_lib_narr; /* -1: la cuadrícula de narradores; si no, las historias de ese narrador */
static const char *lib_narr_nombre(int c)
{
    if (c == NARR_TILO) return s_pet && pet_final_seen(s_pet, PET_FINAL_SECRETO) ? "DIARIO DE PEUMO" : "PÁGINAS SUELTAS";
    if (c == NARR_ANIMALES) return "LOS ANIMALES";
    if (c == NARR_PAPELITOS) return "PAPELITOS";
    if (c == NARR_BRIZNA) return "ABUELA BRIZNA";
    static char n[32];
    snprintf(n, sizeof n, "%s", pet_narr_name(c));
    mayusculas(n);
    return n;
}
static int lib_narr_primera(int c) { for (int k = 0; k < pet_narr_count(c); k++) if (pet_story_known(s_pet, c, k)) return k; return -1; }
static void lib_narr_cb(lv_event_t *e)
{
    const int c = (int)(intptr_t)lv_event_get_user_data(e);
    if (s_panel != PANEL_LIBHIST) return;
    s_lib_narr = c;
    accion(UI_ACCION_LIBRO_VISTO, c); /* (se apaga su estrellita) */
    panel_libro_indice();
}
static void lib_narr_volver_cb(lv_event_t *e) { (void)e; if (s_panel == PANEL_LIBHIST) { s_lib_narr = -1; panel_libro_indice(); } }
static void panel_libro_indice(void)
{
    abrir_tarjeta(PANEL_LIBHIST, 466, 466);
    hist_fondo();
    lv_obj_t *cc = lv_obj_create(s_tarjeta); /* el contenido, centrado en el disco */
    lv_obj_remove_style_all(cc);
    lv_obj_set_size(cc, 380, 380);
    lv_obj_set_pos(cc, 43, 43);
    lv_obj_remove_flag(cc, LV_OBJ_FLAG_CLICKABLE);
    if (!s_pet) return;
    s_lib_desde = false;
    if (s_lib_narr >= 0 && !pet_story_count(s_pet, s_lib_narr)) s_lib_narr = -1;
    const int c0 = s_lib_narr;
    lv_obj_t *t = texto_c(cc, c0 < 0 ? "HISTORIAS" : lib_narr_nombre(c0), 0xFFE9A0, 2, 320);
    lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 22);
    if (!s_nino) {
        char sb_t[48];
        const int n = c0 < 0 ? hist_total() : pet_story_count(s_pet, c0); /* (solo cuántas lleva, nunca cuántas faltan) */
        snprintf(sb_t, sizeof sb_t, n == 1 ? "1 HISTORIA" : "%d HISTORIAS", n);
        lv_obj_t *sb = texto_c(cc, sb_t, 0xC8C4E0, 2, 300);
        lv_obj_align(sb, LV_ALIGN_TOP_MID, 0, 50);
    }
    /* (v96) más grande y sin adornos: dos columnas de narradores con su dibujo grande y el nombre en letra normal; las historias,
     * una por fila (dibujo a la izquierda y el título al lado). Antes: íconos de 24 px y nombres en letra mínima (no se leían). */
    lv_obj_t *ar = area_texto(cc, 30, 78, 320, 262);
    lv_obj_set_style_pad_row(ar, 8, 0);
    lv_obj_set_style_pad_column(ar, 8, 0);
    lv_obj_set_flex_flow(ar, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(ar, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    if (c0 < 0) { /* los narradores, en el orden de la historia */
        static const uint8_t ORDEN[NARR_COUNT] = {0, 1, 2, 3, 4, 5, CRI_NIMBA, NARR_PRISMA, NARR_TILDE, NARR_BRIZNA, NARR_TILO, NARR_PAPELITOS, NARR_ANIMALES};
        for (int i = 0; i < NARR_COUNT; i++) {
            const int c = ORDEN[i], k = lib_narr_primera(c);
            if (k < 0) continue; /* todavía no contó nada: no aparece */
            lv_obj_t *b = boton(ar, 150, s_nino ? 110 : 132, lib_narr_cb, c);
            lv_obj_set_style_radius(b, 22, 0);
            lv_obj_set_style_bg_color(b, lv_color_hex(0x3A2A8A), 0);
            lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
            lv_obj_set_style_border_width(b, 2, 0);
            lv_obj_set_style_border_color(b, lv_color_hex(0x8A72E8), 0);
            lv_obj_set_style_shadow_width(b, 0, 0);
            lv_obj_t *ic = icono(b, icono_narrador(c, k), s_nino ? 7 : 5);
            lv_obj_align(ic, LV_ALIGN_TOP_MID, 0, s_nino ? 10 : 4);
            if (!s_nino) { lv_obj_t *tx = texto_c(b, lib_narr_nombre(c), 0xFFF3C8, 2, 140); lv_obj_align(tx, LV_ALIGN_BOTTOM_MID, 0, -6); }
            if (pet_book_new(s_pet, c)) { lv_obj_t *st = icono(b, ICO_ESTRELLA, 3); lv_obj_align(st, LV_ALIGN_TOP_RIGHT, 2, -2); } /* algo nuevo sin leer */
        }
    } else { /* sus historias, una por fila (solo las que ya contó) */
        for (int k = 0; k < pet_narr_count(c0); k++) {
            if (!pet_story_known(s_pet, c0, k)) continue;
            lv_obj_t *b = boton(ar, 308, s_nino ? 86 : 80, lib_hist_cb, c0 * 64 + k);
            lv_obj_set_style_radius(b, 20, 0);
            lv_obj_set_style_bg_color(b, lv_color_hex(0x3A2A8A), 0);
            lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
            lv_obj_set_style_border_width(b, 2, 0);
            lv_obj_set_style_border_color(b, lv_color_hex(0x8A72E8), 0);
            lv_obj_set_style_shadow_width(b, 0, 0);
            lv_obj_t *ic = icono(b, icono_narrador(c0, k), s_nino ? 6 : 5);
            lv_obj_align(ic, s_nino ? LV_ALIGN_CENTER : LV_ALIGN_LEFT_MID, s_nino ? 0 : 4, 0);
            if (!s_nino) {
                char tt[96];
                snprintf(tt, sizeof tt, "%s", pet_story_title((magica_t)c0, k));
                mayusculas(tt);
                lv_obj_t *tx = texto(b, tt, 0xFFF3C8, 2, 210);
                lv_obj_align(tx, LV_ALIGN_LEFT_MID, 80, 0);
            }
        }
        boton_volver(lib_narr_volver_cb, 0, -128, -20); /* la flecha: de vuelta a los narradores */
    }
    area_lista(ar);
}
static void panel_historia(int c, int k)
{
    if ((unsigned)c >= NARR_COUNT || k < 0 || k >= pet_narr_count(c)) return;
    if (s_tuto_fase == 1) s_tuto_fase = 2; /* ya está leyendo la primera: al cerrar le enseñan a reabrirla */
    s_lib_desde = s_lib_pide; s_lib_pide = false;
    char de[48];
    snprintf(de, sizeof de, "%s te cuenta", pet_narr_name(c));
    s_lect_ico = icono_narrador(c, k);
    snprintf(s_lect_de, sizeof s_lect_de, "%s", de);
    snprintf(s_lect_tit, sizeof s_lect_tit, "%s", pet_story_title((magica_t)c, k));
    snprintf(s_lect_cuerpo, sizeof s_lect_cuerpo, "%s", historia_texto(c, k));
    s_lect_narr = c > CRI_NIMBA ? c : -1;
    s_lect_k = k;
    s_lect_esc = c <= CRI_NIMBA ? c * 5 + k : 0;
    s_lect_ini = 0;
    panel_lectura_hoja();
}

/* --- la primera historia que cuenta una criatura: al terminar, ella enseña cómo volver a leerlas (deslizar desde la orilla izquierda) --- */
static void tuto_flecha_cb(void *var, int32_t v) { lv_obj_set_x((lv_obj_t *)var, (int)v); }
static void tuto_libro_cerrar(void)
{
    if (s_tuto_capa) { lv_anim_delete(s_tuto_capa, NULL); lv_obj_delete(s_tuto_capa); s_tuto_capa = NULL; }
    s_tuto_fase = 0;
    accion(UI_ACCION_LIBRO_AVISO, 0);
}
static void tuto_libro_mostrar(void)
{
    int quien = 0;
    if (!s_pet || !pet_book_tutorial_pending(s_pet, &quien)) { s_tuto_fase = 0; return; }
    s_tuto_capa = caja(s_mundo);
    lv_obj_set_size(s_tuto_capa, W, W);
    lv_obj_set_pos(s_tuto_capa, 0, 0);
    lv_obj_remove_flag(s_tuto_capa, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_t *borde = lv_obj_create(s_tuto_capa); /* la orilla izquierda brilla: de ahí se empieza a deslizar */
    lv_obj_remove_style_all(borde);
    lv_obj_set_size(borde, 14, 200);
    lv_obj_set_pos(borde, 0, (W - 200) / 2);
    lv_obj_set_style_radius(borde, 7, 0);
    lv_obj_set_style_bg_color(borde, lv_color_hex(0xFFE27A), 0);
    lv_obj_set_style_bg_opa(borde, 170, 0);
    lv_obj_remove_flag(borde, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_t *fl = icono(s_tuto_capa, ICO_FLECHA, 4);
    lv_obj_set_y(fl, W / 2 - 24);
    lv_obj_remove_flag(fl, LV_OBJ_FLAG_CLICKABLE);
    lv_anim_t an;
    lv_anim_init(&an);
    lv_anim_set_var(&an, fl);
    lv_anim_set_values(&an, 18, 150);
    lv_anim_set_time(&an, 1100);
    lv_anim_set_repeat_count(&an, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_exec_cb(&an, tuto_flecha_cb);
    lv_anim_start(&an);
    if (!s_nino) {
        lv_obj_t *t = texto_c(s_tuto_capa, "DESLIZA DESDE AQUÍ", 0xFFE9A0, 2, 220);
        lv_obj_set_pos(t, 30, W / 2 + 34);
    }
    globo(ICO_NARRADOR(quien), s_nino ? NULL : "PARA RELEERME, DESLIZA A LA DERECHA DESDE LA ORILLA", 6000);
    s_tuto_fin = s_ms + 20000;
    s_tuto_fase = 3;
}
static void tuto_libro_revisar(void)
{
    if (s_tuto_fase == 0 && s_ms > 15000 && s_hist_pend_c < 0 && s_panel == PANEL_NINGUNO && s_pet && pet_book_tutorial_pending(s_pet, NULL)) s_tuto_fase = 2; /* quedó pendiente de otra vez */
    if (s_tuto_fase == 2 && s_panel == PANEL_NINGUNO && s_hist_pend_c < 0 && !s_cita_on && !s_com_on) tuto_libro_mostrar();
    else if (s_tuto_fase == 3 && (s_panel == PANEL_LIBHIST || s_ms > s_tuto_fin)) tuto_libro_cerrar();
}

/* ======================================================================
 * Las citas: juegos cortos con la criatura (unos 30 segundos)
 * ==================================================================== */
#define CITA_COSAS 10
static int s_cita_que, s_cita_pts, s_cita_ronda, s_cita_aux;
static uint32_t s_cita_t0, s_cita_fin_ms;
static lv_obj_t *s_cita_capa, *s_cita_cosa[CITA_COSAS], *s_cita_rayo;
static float s_cc_x[CITA_COSAS], s_cc_y[CITA_COSAS], s_cc_vx[CITA_COSAS], s_cc_vy[CITA_COSAS];
static int8_t s_cc_est[CITA_COSAS];

static void cita_cosa(int i, art_icono_t ico, int escala, float x, float y)
{
    lv_image_set_src(s_cita_cosa[i], art_icono(ico, escala));
    s_cc_x[i] = x; s_cc_y[i] = y; s_cc_vx[i] = s_cc_vy[i] = 0; s_cc_est[i] = 0;
    lv_obj_set_style_image_recolor_opa(s_cita_cosa[i], 0, 0);
    lv_obj_set_style_opa(s_cita_cosa[i], LV_OPA_COVER, 0);
    lv_image_set_rotation(s_cita_cosa[i], 0);
    ocultar(s_cita_cosa[i], false);
}
static void cita_terminar(void)
{
    if (!s_cita_on) return;
    s_cita_on = false;
    ocultar(s_cita_capa, true);
    for (int i = 0; i < CITA_COSAS; i++) ocultar(s_cita_cosa[i], true);
    ocultar(s_cita_rayo, true);
    lv_obj_set_style_bg_opa(s_cita_capa, 0, 0);
    if (s_cita_pts > 5) s_cita_pts = 5;
    accion(UI_ACCION_CITA_FIN, s_cita_que | s_cita_pts << 8);
    mascota_modo(M_SALTA, 1500);
    for (int k = 0; k < 5; k++) flotar(ICO_CORAZON, NULL, 0, (int)s_mascota.x - 40 + k * 20, cabeza_y(&s_mascota) - 10, (uint32_t)k * 120);
    accion(UI_ACCION_SONAR, SND_HADAS);
    if (!s_nino) globo(ICO_CORAZON, "¡QUÉ RICO JUGAR CONTIGO!", 2500);
}
static void cita_toque(int px, int py);
static void cita_cb(lv_event_t *e)
{
    (void)e;
    lv_point_t pt = {0, 0};
    lv_indev_t *in = lv_indev_active();
    if (in) lv_indev_get_point(in, &pt);
    cita_toque((int)pt.x, (int)pt.y);
}
static void cita_crear(void)
{
    if (s_cita_capa) return;
    s_cita_capa = caja(s_mundo);
    lv_obj_set_size(s_cita_capa, W, W);
    lv_obj_set_style_bg_color(s_cita_capa, lv_color_hex(0x10183A), 0);
    lv_obj_add_flag(s_cita_capa, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_cita_capa, cita_cb, LV_EVENT_PRESSED, NULL);
    for (int i = 0; i < CITA_COSAS; i++) { s_cita_cosa[i] = lv_image_create(s_cita_capa); ocultar(s_cita_cosa[i], true); }
    s_cita_rayo = caja(s_cita_capa);
    lv_obj_set_style_bg_color(s_cita_rayo, lv_color_hex(0xFFF6B0), 0);
    lv_obj_set_style_bg_opa(s_cita_rayo, LV_OPA_50, 0);
    lv_obj_set_style_radius(s_cita_rayo, 8, 0);
    ocultar(s_cita_rayo, true);
    ocultar(s_cita_capa, true);
}
static void cometa_empezar(bool cita);
static void cita_empezar(int que)
{
    if (!s_pet || s_cita_on) return;
    cerrar_panel();
    s_cita_que = que;
    s_cita_pts = 0;
    s_cita_ronda = 0;
    s_cita_aux = 0;
    /* las que usan lo que ya existe */
    if (que == CITA_PESCAR) { accion(UI_ACCION_CITA_FIN, que | 3 << 8); accion(ACT_PESCAR, 0); if (!s_nino) globo(ICO_ONDINA, "¡ONDINA ME GUARDÓ LA MEJOR ORILLA!", 3000); return; }
    if (que == CITA_COMETA) { cometa_empezar(true); return; }
    cita_crear();
    s_cita_on = true;
    s_cita_t0 = s_ms;
    s_cita_fin_ms = 30000;
    ocultar(s_cita_capa, false);
    lv_obj_move_foreground(s_cita_capa);
    lv_obj_set_style_bg_opa(s_cita_capa, 0, 0);
    const float cx = s_cri_x, cy = s_cri_y;
    switch (que) {
    case CITA_PICNIC: /* el mantel con cosas ricas que van apareciendo: tócalas y ella se las come */
        cita_cosa(0, ICO_MANTEL, 6, CX, 290);
        s_mascota.tx = CX - 70; s_mascota.ty = 296; mascota_modo(M_VA, 3000);
        break;
    case CITA_PLANTAR: /* la semilla mágica: riégala tres veces y florece */
        cita_cosa(0, ICO_SEMILLA, 3, CX, 286);
        cita_cosa(1, ICO_REGADERA, 3, CX + 70, 230);
        s_mascota.tx = CX - 50; s_mascota.ty = 292; mascota_modo(M_VA, 3000);
        s_cita_fin_ms = 25000;
        break;
    case CITA_ESCONDIDAS: /* Lumbre apaga su luz y se esconde: busca dónde titila */
        for (int i = 0; i < 3; i++) cita_cosa(i, ICO_HOJAS, 4, 110.0f + i * 120.0f, 250.0f + (i % 2) * 40.0f);
        cita_cosa(3, ICO_LUMBRE, 3, -50, -50);
        s_cita_aux = (int)(azar() % 3);
        ocultar(s_cri, true);
        break;
    case CITA_LUCIERNAGAS: /* oscurece un poquito y las luciérnagas bailan: tócalas y se vuelven estrellas */
        lv_obj_set_style_bg_opa(s_cita_capa, LV_OPA_40, 0);
        for (int i = 0; i < 8; i++) { cita_cosa(i, ICO_LUMBRE, 2, 80.0f + azar01() * 300.0f, 120.0f + azar01() * 180.0f); s_cc_vx[i] = azar01() * 2 - 1; s_cc_vy[i] = azar01() * 2 - 1; }
        mascota_modo(M_SALTA, 2000);
        break;
    case CITA_BARQUITOS: /* dos barquitos de hoja: el tuyo avanza cuando lo tocas o soplas */
        cita_cosa(0, ICO_BARQUITO_HOJA, 4, 70, 130);
        cita_cosa(1, ICO_BARQUITO_HOJA, 4, 70, 182);
        lv_obj_set_style_image_recolor(s_cita_cosa[1], lv_color_hex(0x8FD8FF), 0);
        lv_obj_set_style_image_recolor_opa(s_cita_cosa[1], LV_OPA_40, 0);
        cita_cosa(2, ICO_BANDERINES, 3, 400, 150);
        s_cita_fin_ms = 40000;
        break;
    case CITA_VOLAR_HOJAS: /* un remolino de hojas de colores: atrápalas */
        for (int i = 0; i < 9; i++) {
            cita_cosa(i, i % 3 == 0 ? ICO_HOJA_SECA : ICO_HOJA, 3, cx + azar01() * 60 - 30, cy - 40);
            static const uint32_t COL[3] = {0xF2A33A, 0xF0506E, 0xFFD45C};
            lv_obj_set_style_image_recolor(s_cita_cosa[i], lv_color_hex(COL[i % 3]), 0);
            lv_obj_set_style_image_recolor_opa(s_cita_cosa[i], i % 3 ? LV_OPA_50 : 0, 0);
            s_cc_vx[i] = (float)i * 0.7f; /* fase de la vuelta */
        }
        break;
    case CITA_MUSICA: /* Cristalino toca y ella baila; tú también puedes tocar los cristales */
        s_baile_desde = s_ms; s_baile_nota = -1;
        mascota_modo(M_BAILA, 16000);
        accion(UI_ACCION_SONAR, SND_CAJA_MUSICA);
        s_cita_fin_ms = 17000;
        break;
    case CITA_TESORO: /* cuatro montoncitos: Cristalino da pistas brillando cerca del bueno */
        for (int i = 0; i < 4; i++) cita_cosa(i, ICO_TOPO_HOYO, 3, 120.0f + i * 75.0f, 270.0f + (i % 2) * 30.0f);
        s_cita_aux = (int)(azar() % 4);
        break;
    case CITA_ESTRELLAS: /* el cielo se llena: toca las estrellas para unirlas en un dibujo nuevo */
        lv_obj_set_style_bg_opa(s_cita_capa, LV_OPA_60, 0);
        for (int i = 0; i < 7; i++) cita_cosa(i, ICO_ESTRELLA, 2, 90.0f + i * 47.0f, 80.0f + (float)((i * 53) % 110));
        for (int i = 0; i < 7; i++) lv_obj_set_style_opa(s_cita_cosa[i], LV_OPA_50, 0);
        break;
    case CITA_FARO: /* el faro dormido: tócalo muchas veces para despertar su luz */
        cita_cosa(0, ICO_FARO, 6, 388, 232); /* el faro ya está dibujado en el fondo de la isla: solo se toca */
        lv_obj_set_style_opa(s_cita_cosa[0], LV_OPA_TRANSP, 0);
        s_cita_fin_ms = 30000;
        break;
    default: break;
    }
    if (!s_nino) {
        static const char *const T[CITA_COUNT] = {"¡UN PICNIC! TOCA LO QUE QUIERES QUE COMA", "¡RIEGA LA SEMILLA MÁGICA!", "¿DÓNDE SE ESCONDIÓ LUMBRE? BUSCA DONDE TITILA",
                                                  "¡TOCA LAS LUCIÉRNAGAS!", "", "¡TOCA TU BARQUITO PARA QUE AVANCE!", "", "¡ATRAPA LAS HOJAS!",
                                                  "¡A BAILAR! TOCA LOS CRISTALES", "¿DÓNDE ESTÁ EL TESORO? MIRA DÓNDE BRILLA", "¡TOCA LAS ESTRELLAS PARA UNIRLAS!",
                                                  "¡TOCA EL FARO PARA DESPERTAR SU LUZ!"};
        if (T[que][0]) globo(ICO_CITA[que], T[que], 3500);
    }
    accion(UI_ACCION_SONAR, SND_HADAS);
}
static int cosa_tocada(int px, int py, int desde, int hasta, int radio)
{
    for (int i = desde; i < hasta; i++) {
        if (lv_obj_has_flag(s_cita_cosa[i], LV_OBJ_FLAG_HIDDEN) || s_cc_est[i] < 0) continue;
        const lv_image_dsc_t *d = lv_image_get_src(s_cita_cosa[i]);
        int w = d ? (int)d->header.w : 36;
        if (hypotf((float)px - s_cc_x[i], (float)py - (s_cc_y[i] - (float)w / 2)) < (float)(radio + w / 2)) return i;
    }
    return -1;
}
static void cita_riego_aplicar(void)
{
    if (!s_cita_on || s_cita_que != CITA_PLANTAR || s_cita_ronda >= 3) return;
    s_cita_ronda++;
    accion(UI_ACCION_SONAR, SND_CHAPOTEO);
    static const art_icono_t ETAPA[3] = {ICO_BROTE, ICO_FLOR, ICO_GIRASOL};
    lv_image_set_src(s_cita_cosa[0], art_icono(ETAPA[s_cita_ronda - 1], 3 + s_cita_ronda));
    s_cita_pts++;
    if (s_cita_ronda == 3) { for (int k = 0; k < 5; k++) flotar(ICO_ESTRELLA, NULL, 0, (int)s_cc_x[0] - 40 + k * 20, (int)s_cc_y[0] - 80, (uint32_t)k * 100); mascota_modo(M_SALTA, 1500); s_cita_fin_ms = s_ms - s_cita_t0 + 3000; }
}
static void cita_riego_fin(void) { if (s_cita_on && s_cita_que == CITA_PLANTAR && s_cita_ronda < 3) ocultar(s_cita_cosa[1], false); }
static void cita_toque(int px, int py)
{
    if (!s_cita_on) return;
    int i;
    switch (s_cita_que) {
    case CITA_PICNIC:
        if ((i = cosa_tocada(px, py, 1, 6, 14)) >= 0) {
            s_cc_est[i] = -1; ocultar(s_cita_cosa[i], true);
            lv_image_set_src(s_objeto, lv_image_get_src(s_cita_cosa[i]));
            mascota_modo(M_COME, 1500);
            ocultar(s_objeto, false);
            accion(UI_ACCION_SONAR, SND_COMER);
            flotar(ICO_CORAZON, NULL, 0, (int)s_mascota.x, cabeza_y(&s_mascota) - 10, 400);
            s_cita_pts++;
        }
        break;
    case CITA_PLANTAR:
        if (!s_rg_fase && (i = cosa_tocada(px, py, 0, 2, 30)) >= 0 && s_cita_ronda < 3) {
            /* la regadera viaja hasta la semilla, se inclina y la riega; al caer el agua crece (cita_riego_aplicar) */
            ocultar(s_cita_cosa[1], true);
            regadera_empezar(-1, (int)s_cc_x[0], (int)s_cc_y[0] - 18);
            lv_obj_move_foreground(s_rg_img);
        }
        break;
    case CITA_ESCONDIDAS:
        if ((i = cosa_tocada(px, py, 0, 3, 20)) >= 0) {
            if (i == s_cita_aux) { /* ¡la encontraste! */
                s_cita_pts++;
                cita_cosa(3, ICO_LUMBRE, 4, s_cc_x[i], s_cc_y[i] - 30);
                s_cc_est[3] = 1; s_cc_vx[3] = (float)(s_ms - s_cita_t0); /* desde cuándo se ve */
                accion(UI_ACCION_SONAR, SND_ENCONTRADA);
                for (int k = 0; k < 4; k++) flotar(ICO_ESTRELLA, NULL, 0, (int)s_cc_x[i] - 30 + k * 20, (int)s_cc_y[i] - 60, (uint32_t)k * 100);
                s_cita_ronda++;
                s_cita_aux = (s_cita_aux + 1 + (int)(azar() % 2)) % 3;
                if (s_cita_ronda >= 3) s_cita_fin_ms = s_ms - s_cita_t0 + 2500;
            } else { accion(UI_ACCION_SONAR, SND_SUSPIRO); salpicar((int)s_cc_x[i], (int)s_cc_y[i] - 20, 0x6AB04A); }
        }
        break;
    case CITA_LUCIERNAGAS:
        if ((i = cosa_tocada(px, py, 0, 8, 16)) >= 0) {
            s_cc_est[i] = -1;
            lv_image_set_src(s_cita_cosa[i], art_icono(ICO_ESTRELLA, 2));
            s_cc_vy[i] = -3;
            accion(UI_ACCION_SONAR, SND_ATRAPA);
            s_cita_pts++;
        }
        break;
    case CITA_BARQUITOS:
        if (py < 260 && s_cc_est[0] == 0) { s_cc_vx[0] += 1.6f; salpicar((int)s_cc_x[0] - 20, (int)s_cc_y[0] + 4, 0xD6F0FF); }
        break;
    case CITA_VOLAR_HOJAS:
        if ((i = cosa_tocada(px, py, 0, 9, 14)) >= 0) {
            s_cc_est[i] = -1; ocultar(s_cita_cosa[i], true);
            flotar(ICO_ESTRELLA, NULL, 0, px, py - 10, 0);
            accion(UI_ACCION_SONAR, SND_ATRAPA);
            s_cita_pts++;
        }
        break;
    case CITA_MUSICA: {
        static const sound_t N[3] = {SND_CRISTAL_LILA, SND_CRISTAL_BLANCO, SND_CRISTAL_ORO};
        accion(UI_ACCION_SONAR, N[s_cita_ronda++ % 3]);
        flotar(ICO_NOTA, NULL, 0, px, py - 10, 0);
        if (s_cita_ronda % 3 == 0) s_cita_pts++;
        break;
    }
    case CITA_TESORO:
        if ((i = cosa_tocada(px, py, 0, 4, 18)) >= 0 && s_cc_est[i] >= 0) {
            s_cc_est[i] = -1;
            salpicar((int)s_cc_x[i], (int)s_cc_y[i] - 10, 0x8A6440);
            if (i == s_cita_aux) {
                lv_image_set_src(s_cita_cosa[i], art_icono(ICO_CRISTAL, 4));
                accion(UI_ACCION_SONAR, SND_TESORO);
                for (int k = 0; k < 5; k++) flotar(ICO_ESTRELLA, NULL, 0, (int)s_cc_x[i] - 40 + k * 20, (int)s_cc_y[i] - 60, (uint32_t)k * 100);
                s_cita_pts = 5 - s_cita_ronda;
                s_cita_fin_ms = s_ms - s_cita_t0 + 3000;
            } else { lv_image_set_src(s_cita_cosa[i], art_icono(ICO_PIEDRA, 2)); s_cita_ronda++; accion(UI_ACCION_SONAR, SND_CAVAR); }
        }
        break;
    case CITA_ESTRELLAS:
        if ((i = cosa_tocada(px, py, 0, 7, 14)) >= 0 && s_cc_est[i] == 0) {
            s_cc_est[i] = 1;
            lv_obj_set_style_opa(s_cita_cosa[i], LV_OPA_COVER, 0);
            lv_image_set_src(s_cita_cosa[i], art_icono(ICO_ESTRELLA, 3));
            static const sound_t N[7] = {SND_CRISTAL_AZUL, SND_CRISTAL_LILA, SND_CRISTAL_VERDE, SND_CRISTAL_ROSA, SND_CRISTAL_BLANCO, SND_CRISTAL_ORO, SND_CRISTAL_ORO};
            accion(UI_ACCION_SONAR, N[s_cita_ronda % 7]);
            s_cita_ronda++;
            if (s_cita_ronda % 2 == 0) s_cita_pts++;
            if (s_cita_ronda >= 7) { flotar(ICO_ESTRELLA_FUGAZ, NULL, 0, 100, 60, 300); s_cita_fin_ms = s_ms - s_cita_t0 + 3000; }
        }
        break;
    case CITA_FARO:
        if ((i = cosa_tocada(px, py, 0, 1, 30)) >= 0 && s_cita_ronda < 12) {
            s_cita_ronda++;
            accion(UI_ACCION_SONAR, SND_TOQUE);
            flotar(ICO_ESTRELLA, NULL, 0, (int)s_cc_x[0], (int)s_cc_y[0] - 70, 0);
            if (s_cita_ronda == 12) { s_cita_pts = 4; accion(UI_ACCION_SONAR, SND_HADAS); s_cita_fin_ms = s_ms - s_cita_t0 + 5000; }
        }
        break;
    default: break;
    }
}
static void animar_cita(void)
{
    if (!s_cita_on) return;
    uint32_t t = s_ms - s_cita_t0;
    if (!s_pet || s_pet->dormida || (s_escena != (int)s_pet->escena)) { s_cita_pts = 0; cita_terminar(); return; }
    switch (s_cita_que) {
    case CITA_PICNIC: { /* van apareciendo cosas ricas sobre el mantel */
        static const art_icono_t COSA[5] = {ICO_FRUTA, ICO_GALLETA, ICO_FRUTILLA, ICO_MIEL, ICO_PASTEL};
        int n = (int)(t / 1800);
        for (int i = 1; i <= 5 && i <= n; i++)
            if (lv_obj_has_flag(s_cita_cosa[i], LV_OBJ_FLAG_HIDDEN) && s_cc_est[i] == 0 && !s_cc_vx[i]) { cita_cosa(i, COSA[i - 1], 3, CX - 50.0f + (float)((i - 1) % 3) * 50.0f, 286.0f + (float)((i - 1) / 3) * 20.0f); s_cc_vx[i] = 1; flotar(ICO_ESTRELLA, NULL, 0, (int)s_cc_x[i], (int)s_cc_y[i] - 30, 0); }
        if (s_cita_pts >= 5 && t < s_cita_fin_ms - 2500) s_cita_fin_ms = t + 2500;
        break;
    }
    case CITA_ESCONDIDAS: { /* titila donde está escondida; cuando la encuentras, sale volando y se vuelve a esconder */
        int i = s_cita_aux;
        if (s_cc_est[3] == 1 && t - (uint32_t)s_cc_vx[3] > 1400) { s_cc_est[3] = 0; ocultar(s_cita_cosa[3], true); }
        for (int k = 0; k < 3; k++) lv_obj_set_style_image_recolor_opa(s_cita_cosa[k], 0, 0);
        if (s_cc_est[3] != 1 && (t / 300) % 4 == 0) { lv_obj_set_style_image_recolor(s_cita_cosa[i], lv_color_hex(0xFFF0A0), 0); lv_obj_set_style_image_recolor_opa(s_cita_cosa[i], LV_OPA_40, 0); }
        break;
    }
    case CITA_LUCIERNAGAS:
        for (int i = 0; i < 8; i++) {
            if (s_cc_est[i] < 0) { s_cc_y[i] += s_cc_vy[i]; if (s_cc_y[i] < 10) ocultar(s_cita_cosa[i], true); continue; }
            s_cc_vx[i] += (azar01() - 0.5f) * 0.4f; s_cc_vy[i] += (azar01() - 0.5f) * 0.4f;
            s_cc_vx[i] = s_cc_vx[i] > 2 ? 2 : (s_cc_vx[i] < -2 ? -2 : s_cc_vx[i]);
            s_cc_vy[i] = s_cc_vy[i] > 2 ? 2 : (s_cc_vy[i] < -2 ? -2 : s_cc_vy[i]);
            s_cc_x[i] += s_cc_vx[i]; s_cc_y[i] += s_cc_vy[i];
            if (s_cc_x[i] < 70 || s_cc_x[i] > 396) s_cc_vx[i] = -s_cc_vx[i];
            if (s_cc_y[i] < 110 || s_cc_y[i] > 320) s_cc_vy[i] = -s_cc_vy[i];
            lv_obj_set_style_opa(s_cita_cosa[i], (lv_opa_t)(150 + 100 * sinf((float)s_ms / 200.0f + (float)i)), 0);
        }
        if (s_cita_pts >= 8 && t < s_cita_fin_ms - 2000) s_cita_fin_ms = t + 2000;
        break;
    case CITA_BARQUITOS: { /* el de Ondina va parejo; el tuyo, según lo empujes */
        s_cc_vx[0] *= 0.97f;
        s_cc_x[0] += 0.25f + s_cc_vx[0] * 0.5f;
        s_cc_x[1] += 0.62f;
        s_cc_y[0] = 130 + sinf((float)s_ms / 300.0f) * 3; s_cc_y[1] = 182 + sinf((float)s_ms / 330.0f + 1) * 3;
        if (s_cc_est[0] == 0 && (s_cc_x[0] >= 390 || s_cc_x[1] >= 390)) {
            s_cc_est[0] = 1;
            bool gano = s_cc_x[0] >= s_cc_x[1];
            s_cita_pts = gano ? 4 : 2;
            accion(UI_ACCION_SONAR, gano ? SND_TRUCO : SND_ALEGRE);
            if (!s_nino) globo(ICO_BARQUITO_HOJA, gano ? "¡GANÓ MI BARQUITO!" : "¡GANÓ ONDINA! LA PRÓXIMA TE GANO", 2500);
            s_cita_fin_ms = t + 2500;
        }
        break;
    }
    case CITA_VOLAR_HOJAS: /* dan vueltas en el viento, subiendo y bajando */
        for (int i = 0; i < 9; i++) {
            if (s_cc_est[i] < 0) continue;
            float a = (float)t / 900.0f + s_cc_vx[i];
            s_cc_x[i] = s_cri_x + cosf(a) * (60 + (float)(i % 3) * 30) - 20;
            s_cc_y[i] = 210 + sinf(a * 1.3f) * 70;
            if (s_cc_x[i] < 60) s_cc_x[i] = 60;
            if (s_cc_x[i] > 406) s_cc_x[i] = 406;
            lv_image_set_rotation(s_cita_cosa[i], (int32_t)(sinf(a * 2) * 300)); /* dan vueltas */
        }
        if (s_cita_pts >= 9 && t < s_cita_fin_ms - 2000) s_cita_fin_ms = t + 2000;
        break;
    case CITA_MUSICA: /* Cristalino toca: notitas que salen de él */
        if (t % 500 < TICK_MS) flotar(ICO_NOTA, NULL, 0, (int)s_cri_x, (int)s_cri_y - 50, 0);
        break;
    case CITA_TESORO: /* brilla cerca del bueno (cada vez más claro) */
        if ((t / 250) % (8 - (t / 6000 > 4 ? 4 : t / 6000)) == 0 && s_cc_est[s_cita_aux] >= 0) flotar(ICO_ESTRELLA, NULL, 0, (int)s_cc_x[s_cita_aux] + (int)(azar() % 30) - 15, (int)s_cc_y[s_cita_aux] - 40, 0);
        break;
    case CITA_ESTRELLAS: { /* las que tocaste quedan unidas por un rayito */
        break;
    }
    case CITA_FARO: { /* la luz crece con cada toque; al final barre el lago */
        int r = s_cita_ronda;
        ocultar(s_cita_rayo, r == 0);
        if (r) {
            float a = r >= 12 ? sinf((float)t / 600.0f) : 0;
            int len = r * 14;
            lv_obj_set_size(s_cita_rayo, len, 14);
            lv_obj_set_pos(s_cita_rayo, (int)s_cc_x[0] - (a < 0 ? len : 0) - 4, (int)s_cc_y[0] - 66);
            lv_obj_set_style_bg_opa(s_cita_rayo, (lv_opa_t)(60 + r * 12), 0);
        }
        break;
    }
    default: break;
    }
    for (int i = 0; i < CITA_COSAS; i++) {
        if (lv_obj_has_flag(s_cita_cosa[i], LV_OBJ_FLAG_HIDDEN)) continue;
        const lv_image_dsc_t *d = lv_image_get_src(s_cita_cosa[i]);
        int w = d ? (int)d->header.w : 36, h = d ? (int)d->header.h : 36;
        lv_obj_set_pos(s_cita_cosa[i], (int)s_cc_x[i] - w / 2, (int)s_cc_y[i] - h);
    }
    if (t >= s_cita_fin_ms) cita_terminar();
}

/* ======================================================================
 * Los secretos de cada zona
 * ==================================================================== */
/* --- efectos de luz de los secretos: ondas que se abren y chispas que suben --- */
#define N_ONDAS 12
#define N_CHISPAS 30
static struct { lv_obj_t *o; uint32_t t0, dur; int x, y, r; bool plano, vivo; } s_onda[N_ONDAS];
static EXT_RAM_BSS_ATTR struct { lv_obj_t *o; float x, y, vx, vy; uint32_t t0, dur; bool vivo; } s_chi[N_CHISPAS];
static void onda(int x, int y, uint32_t color, int rmax, bool plano, uint32_t retraso_ms)
{
    for (int i = 0; i < N_ONDAS; i++) {
        if (s_onda[i].vivo) continue;
        if (!s_onda[i].o) {
            s_onda[i].o = caja(s_mundo);
            lv_obj_set_style_radius(s_onda[i].o, LV_RADIUS_CIRCLE, 0);
            lv_obj_set_style_bg_opa(s_onda[i].o, LV_OPA_TRANSP, 0);
            lv_obj_set_style_border_width(s_onda[i].o, 4, 0);
            lv_obj_set_style_shadow_width(s_onda[i].o, 16, 0);
        }
        lv_obj_set_style_border_color(s_onda[i].o, lv_color_hex(color), 0);
        lv_obj_set_style_shadow_color(s_onda[i].o, lv_color_hex(color), 0);
        s_onda[i].t0 = s_ms + retraso_ms; s_onda[i].dur = 1500; s_onda[i].x = x; s_onda[i].y = y; s_onda[i].r = rmax; s_onda[i].plano = plano; s_onda[i].vivo = true;
        ocultar(s_onda[i].o, true);
        if (plano && s_mascota.sombra) lv_obj_move_to_index(s_onda[i].o, lv_obj_get_index(s_mascota.sombra)); /* en el suelo, debajo de ella */
        else lv_obj_move_foreground(s_onda[i].o);
        return;
    }
}
static void chispa(int x, int y, uint32_t color, float vx, float vy)
{
    for (int i = 0; i < N_CHISPAS; i++) {
        if (s_chi[i].vivo) continue;
        if (!s_chi[i].o) {
            s_chi[i].o = caja(s_mundo);
            lv_obj_set_size(s_chi[i].o, 7, 7);
            lv_obj_set_style_radius(s_chi[i].o, LV_RADIUS_CIRCLE, 0);
            lv_obj_set_style_bg_opa(s_chi[i].o, LV_OPA_COVER, 0);
            lv_obj_set_style_shadow_width(s_chi[i].o, 14, 0);
        }
        lv_obj_set_style_bg_color(s_chi[i].o, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_shadow_color(s_chi[i].o, lv_color_hex(color), 0);
        s_chi[i].x = (float)x; s_chi[i].y = (float)y; s_chi[i].vx = vx; s_chi[i].vy = vy;
        s_chi[i].t0 = s_ms; s_chi[i].dur = 900 + azar() % 700; s_chi[i].vivo = true;
        ocultar(s_chi[i].o, false);
        lv_obj_move_foreground(s_chi[i].o);
        return;
    }
}
static void chispas(int x, int y, uint32_t color, int n, int ancho)
{
    for (int k = 0; k < n; k++) chispa(x + (int)(azar() % (uint32_t)(ancho * 2 + 1)) - ancho, y + (int)(azar() % 14) - 7, color, (azar01() - 0.5f) * 0.9f, -0.35f - azar01() * 0.7f);
}
static void animar_efectos_luz(void)
{
    for (int i = 0; i < N_ONDAS; i++) {
        if (!s_onda[i].vivo) continue;
        int32_t dt = (int32_t)(s_ms - s_onda[i].t0);
        if (dt < 0) continue;
        if ((uint32_t)dt >= s_onda[i].dur || (s_escena < 0)) { s_onda[i].vivo = false; ocultar(s_onda[i].o, true); continue; }
        float f = (float)dt / (float)s_onda[i].dur;
        int r = (int)((float)s_onda[i].r * (0.15f + 0.85f * f)), h = s_onda[i].plano ? r * 4 / 10 : r;
        lv_obj_set_size(s_onda[i].o, r * 2, h * 2 + 2);
        lv_obj_set_pos(s_onda[i].o, s_onda[i].x - r, s_onda[i].y - h);
        lv_obj_set_style_opa(s_onda[i].o, (lv_opa_t)(230.0f * (1.0f - f) * (1.0f - f)), 0);
        ocultar(s_onda[i].o, false);
    }
    for (int i = 0; i < N_CHISPAS; i++) {
        if (!s_chi[i].vivo) continue;
        float f = (float)(s_ms - s_chi[i].t0) / (float)s_chi[i].dur;
        if (f >= 1.0f) { s_chi[i].vivo = false; ocultar(s_chi[i].o, true); continue; }
        s_chi[i].x += s_chi[i].vx + sinf((float)s_ms / 200.0f + (float)i) * 0.25f;
        s_chi[i].y += s_chi[i].vy;
        lv_obj_set_pos(s_chi[i].o, (int)s_chi[i].x - 2, (int)s_chi[i].y - 2);
        lv_obj_set_style_opa(s_chi[i].o, (lv_opa_t)(255.0f * (f < 0.2f ? f / 0.2f : (1.0f - f) / 0.8f)), 0);
    }
}

/* --- el anillo de hadas del bosque --- */
#define ANILLO_X 182
#define ANILLO_Y 262
#define ANILLO_N 7
static lv_obj_t *s_ani_hongo[ANILLO_N], *s_hada[4];
static uint32_t s_anillo_baile; /* bailando desde (0: no) */
static void anillo_cb(lv_event_t *e)
{
    (void)e;
    if (!s_pet || s_anillo_baile || s_cita_on) return;
    if (pet_special_ready(s_pet, ESP_ANILLO, s_ultimo_t)) {
        s_anillo_baile = s_ms;
        s_mascota.tx = ANILLO_X; s_mascota.ty = ANILLO_Y + 6;
        mascota_modo(M_VA, 2500);
        accion(UI_ACCION_SONAR, SND_HADAS);
    } else {
        flotar(ICO_HONGO, NULL, 0, ANILLO_X, ANILLO_Y - 30, 0);
        if (!s_nino && !pet_special_known(s_pet, ESP_ANILLO)) globo(ICO_HONGO, "HONGOS EN RONDA... ¡QUÉ RAROS SON!", 2500);
    }
}
static void animar_anillo(void)
{
    const pet_t *p = s_pet;
    if (!s_ani_hongo[0]) {
        for (int i = 0; i < ANILLO_N; i++) {
            s_ani_hongo[i] = lv_image_create(s_mundo);
            lv_obj_add_flag(s_ani_hongo[i], LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_ext_click_area(s_ani_hongo[i], 8);
            lv_obj_add_event_cb(s_ani_hongo[i], anillo_cb, LV_EVENT_CLICKED, NULL);
            ocultar(s_ani_hongo[i], true);
        }
        for (int i = 0; i < 4; i++) { s_hada[i] = lv_image_create(s_mundo); lv_image_set_src(s_hada[i], art_icono(ICO_HADA, 2)); ocultar(s_hada[i], true); }
    }
    bool ver = p && s_escena == ESC_BOSQUE && !s_interior && p->etapa != STAGE_HUEVO;
    bool luz = ver && pet_special_ready(p, ESP_ANILLO, s_ultimo_t);
    for (int i = 0; i < ANILLO_N; i++) {
        ocultar(s_ani_hongo[i], !ver);
        if (!ver) continue;
        float a = (float)i / ANILLO_N * 6.2832f;
        lv_image_set_src(s_ani_hongo[i], art_icono(luz ? ICO_HONGO_LUZ : ICO_HONGO, 2));
        lv_obj_set_pos(s_ani_hongo[i], ANILLO_X + (int)(cosf(a) * 44) - 12, ANILLO_Y + (int)(sinf(a) * 16) - 20);
        if (luz) lv_obj_set_style_opa(s_ani_hongo[i], (lv_opa_t)(190 + 60 * sinf((float)s_ms / 400.0f + (float)i)), 0);
        else lv_obj_set_style_opa(s_ani_hongo[i], LV_OPA_COVER, 0);
    }
    if (luz && !s_anillo_baile) { /* listo para bailar: ondas de luz sobre el suelo y polvito de hadas */
        if (s_ms % 1900 < TICK_MS) { onda(ANILLO_X, ANILLO_Y + 4, 0xFFC8F0, 74, true, 0); onda(ANILLO_X, ANILLO_Y + 4, 0xFFF0A0, 52, true, 350); }
        if (s_ms % 240 < TICK_MS) chispas(ANILLO_X, ANILLO_Y - 8, (azar() & 1) ? 0xFFC8F0 : 0xFFF0A0, 1, 44);
    }
    /* el baile: ella gira en el centro y las hadas dan vueltas */
    bool baila = s_anillo_baile && ver;
    for (int i = 0; i < 4; i++) ocultar(s_hada[i], !baila || s_ms - s_anillo_baile < 2500);
    if (!s_anillo_baile) return;
    uint32_t t = s_ms - s_anillo_baile;
    if (!ver) { s_anillo_baile = 0; return; }
    if (t > 2500 && s_mascota.modo != M_BAILA && t < 3000) { s_baile_desde = s_ms; s_baile_nota = -1; mascota_modo(M_BAILA, 10000); }
    for (int i = 0; i < 4; i++) {
        float a = (float)t / 700.0f + (float)i * 1.5708f;
        lv_obj_set_pos(s_hada[i], ANILLO_X + (int)(cosf(a) * 60) - 12, ANILLO_Y - 50 + (int)(sinf(a) * 24) - 12);
        lv_obj_move_foreground(s_hada[i]);
        if (t > 2500 && s_ms % 120 < TICK_MS) { /* la estela de polvo de cada hada */
            static const uint32_t CH[4] = {0xFFC8F0, 0xFFF0A0, 0xB8F0FF, 0xD8C0FF};
            chispa(ANILLO_X + (int)(cosf(a) * 60), ANILLO_Y - 50 + (int)(sinf(a) * 24) + 8, CH[i], (azar01() - 0.5f) * 0.5f, 0.15f + azar01() * 0.4f);
        }
    }
    if (t > 2500 && s_ms % 700 < TICK_MS) onda(ANILLO_X, ANILLO_Y + 4, (s_ms / 700) % 2 ? 0xFFC8F0 : 0xB8F0FF, 40 + (int)(t / 220) % 50, true, 0);
    if (t > 12500) {
        s_anillo_baile = 0;
        onda(ANILLO_X, ANILLO_Y + 4, 0xFFF0A0, 110, true, 0); onda(ANILLO_X, ANILLO_Y + 4, 0xFFC8F0, 90, true, 250); onda(ANILLO_X, ANILLO_Y + 4, 0xB8F0FF, 70, true, 500);
        chispas(ANILLO_X, ANILLO_Y - 30, 0xFFF0A0, 14, 50);
        for (int i = 0; i < 4; i++) ocultar(s_hada[i], true);
        accion(UI_ACCION_ESPECIAL, ESP_ANILLO);
        for (int k = 0; k < 6; k++) flotar(ICO_HADA, NULL, 0, ANILLO_X - 50 + k * 20, ANILLO_Y - 60, (uint32_t)k * 120);
    }
}

/* --- el reflejo encantado del lago: constelaciones que no están en el cielo --- */
#define REFLEJO_N 3      /* constelaciones por noche */
#define REFLEJO_PUNTOS 5
static lv_obj_t *s_ref_zona[REFLEJO_N], *s_ref_pto[REFLEJO_N][REFLEJO_PUNTOS], *s_ref_linea[REFLEJO_N];
static lv_point_precise_t s_ref_ptos[REFLEJO_N][REFLEJO_PUNTOS];
static const int8_t FORMA[PET_CONSTELACIONES][REFLEJO_PUNTOS][2] = { /* puntitos de cada una (relativos) */
    {{-18, 4}, {-6, -6}, {8, -6}, {18, 2}, {0, 10}},   /* la tetera */
    {{-16, 8}, {-6, 0}, {2, -8}, {12, -4}, {16, 8}},   /* el caracol viajero */
    {{0, -14}, {0, -2}, {-10, 8}, {10, 8}, {14, -10}}, /* la niña del farol */
    {{-18, 0}, {-8, -6}, {6, -6}, {16, 0}, {-2, 6}},   /* el pez sonriente */
    {{-16, -12}, {-8, -4}, {0, 4}, {6, 10}, {14, 10}}, /* la escoba */
    {{-14, 0}, {-6, -8}, {0, 0}, {6, -8}, {14, 0}},    /* el abrazo */
};
static int ref_cual(int i) { return (int)((s_ultimo_t / 86400u + (uint32_t)i * 2u) % PET_CONSTELACIONES); } /* cada noche, otras tres */
static void reflejo_cb(lv_event_t *e)
{
    int i = (int)(intptr_t)lv_event_get_user_data(e);
    if (!s_pet || s_panel != PANEL_NINGUNO) return;
    int k = ref_cual(i);
    accion(UI_ACCION_ESPECIAL, ESP_REFLEJO | k << 4);
    accion(UI_ACCION_SONAR, SND_HADAS);
    onda(s_ref_zona[i] ? (int)lv_obj_get_x(s_ref_zona[i]) + 32 : 200, s_ref_zona[i] ? (int)lv_obj_get_y(s_ref_zona[i]) + 22 : 120, 0xBFE8FF, 70, true, 0);
    chispas(s_ref_zona[i] ? (int)lv_obj_get_x(s_ref_zona[i]) + 32 : 200, s_ref_zona[i] ? (int)lv_obj_get_y(s_ref_zona[i]) + 20 : 120, 0xFFF6B0, 10, 26);
    if (s_nino) { for (int q = 0; q < 5; q++) flotar(ICO_ESTRELLA, NULL, 0, 150 + q * 40, 120, (uint32_t)q * 100); return; }
    panel_lectura(ICO_ESTRELLA, "Leyenda de la laguna", pet_constellation_name(k), pet_constellation_legend(k));
}
static void animar_reflejo(void)
{
    const pet_t *p = s_pet;
    if (!s_ref_zona[0]) {
        for (int i = 0; i < REFLEJO_N; i++) {
            s_ref_zona[i] = caja(s_mundo);
            lv_obj_set_size(s_ref_zona[i], 64, 44);
            lv_obj_add_flag(s_ref_zona[i], LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_event_cb(s_ref_zona[i], reflejo_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
            s_ref_linea[i] = lv_line_create(s_ref_zona[i]); /* los hilitos de luz que unen las estrellas */
            lv_obj_set_style_line_color(s_ref_linea[i], lv_color_hex(0xBFE8FF), 0);
            lv_obj_set_style_line_width(s_ref_linea[i], 2, 0);
            lv_obj_set_style_line_rounded(s_ref_linea[i], true, 0);
            lv_obj_remove_flag(s_ref_linea[i], LV_OBJ_FLAG_CLICKABLE);
            for (int k = 0; k < REFLEJO_PUNTOS; k++) {
                lv_obj_t *d = caja(s_ref_zona[i]);
                lv_obj_set_size(d, 5, 5);
                lv_obj_set_style_radius(d, 3, 0);
                lv_obj_set_style_bg_color(d, lv_color_hex(0xFFF6D0), 0);
                lv_obj_set_style_bg_opa(d, LV_OPA_COVER, 0);
                lv_obj_set_style_shadow_width(d, 6, 0);
                lv_obj_set_style_shadow_color(d, lv_color_hex(0xFFF6B0), 0);
                lv_obj_remove_flag(d, LV_OBJ_FLAG_CLICKABLE);
                s_ref_pto[i][k] = d;
            }
            ocultar(s_ref_zona[i], true);
        }
    }
    bool ver = p && s_escena == ESC_LAGO && !s_interior && pet_special_ready(p, ESP_REFLEJO, s_ultimo_t) && !s_cita_on;
    static const int16_t POS[REFLEJO_N][2] = {{196, 140}, {272, 62}, {376, 112}}; /* en el agua del lago */
    for (int i = 0; i < REFLEJO_N; i++) {
        ocultar(s_ref_zona[i], !ver);
        if (!ver) continue;
        int k = ref_cual(i);
        lv_obj_set_pos(s_ref_zona[i], POS[i][0] - 32, POS[i][1] - 22 + (int)(sinf((float)s_ms / 900.0f + (float)i) * 2));
        for (int q = 0; q < REFLEJO_PUNTOS; q++) {
            s_ref_ptos[i][q].x = 32 + FORMA[k][q][0]; s_ref_ptos[i][q].y = 22 + FORMA[k][q][1];
        }
        lv_line_set_points(s_ref_linea[i], s_ref_ptos[i], REFLEJO_PUNTOS);
        lv_obj_set_style_line_opa(s_ref_linea[i], (lv_opa_t)(70 + 45 * sinf((float)s_ms / 700.0f + (float)i * 2)), 0);
        if ((s_ms + (uint32_t)i * 800) % 2600 < TICK_MS) onda(POS[i][0], POS[i][1] + 8, 0xBFE8FF, 40, true, 0); /* ondas en el agua */
        if ((s_ms + (uint32_t)i * 500) % 900 < TICK_MS) chispas(POS[i][0], POS[i][1], 0xFFF6B0, 1, 22);
        for (int q = 0; q < REFLEJO_PUNTOS; q++) {
            lv_obj_set_pos(s_ref_pto[i][q], 30 + FORMA[k][q][0], 20 + FORMA[k][q][1]);
            lv_obj_set_style_opa(s_ref_pto[i][q], (lv_opa_t)(150 + 100 * sinf((float)s_ms / 350.0f + (float)(q * 3 + i))), 0);
        }
    }
}

/* --- la cometa de la colina: se maneja inclinando el reloj (o tocando a un lado y al otro) --- */
#define HILO_N 9
static lv_obj_t *s_com_suelo, *s_com, *s_com_hilo, *s_com_capa, *s_com_pto[HILO_N];
static bool s_com_cita;
static float s_com_x, s_com_y, s_com_vx, s_com_viento;
static uint32_t s_com_t0, s_com_borde;
#define COLA_N 9
static lv_obj_t *s_com_cola[COLA_N];
static float s_cola_x[COLA_N], s_cola_y[COLA_N];
static uint32_t s_cola_t;
static void cometa_fin(bool bien)
{
    if (!s_com_on) return;
    s_com_on = false;
    for (int k = 0; k < COLA_N; k++) if (s_com_cola[k]) ocultar(s_com_cola[k], true);
    if (bien) { onda((int)s_com_x, (int)s_com_y, 0xFFD45C, 90, false, 0); onda((int)s_com_x, (int)s_com_y, 0xF0506E, 60, false, 250); chispas((int)s_com_x, (int)s_com_y, 0xFFD45C, 14, 40); confeti((int)s_com_x, (int)s_com_y + 30, 16); }
    ocultar(s_com, true); ocultar(s_com_hilo, true); ocultar(s_com_capa, true);
    int seg = (int)((s_ms - s_com_t0) / 1000);
    if (seg > 60) seg = 60;
    accion(UI_ACCION_ESPECIAL, ESP_COMETA | (bien ? 30 : seg) << 4);
    if (s_com_cita) accion(UI_ACCION_CITA_FIN, CITA_COMETA | (bien ? 4 : 1) << 8);
    if (!bien && !s_nino) globo(ICO_COMETA, "¡SE CAYÓ LA COMETA! OTRO DÍA CON VIENTO LA VOLAMOS", 3000);
    if (bien) { accion(UI_ACCION_SONAR, SND_HADAS); mascota_modo(M_SALTA, 2000); }
}
static void cometa_toque_cb(lv_event_t *e)
{
    (void)e;
    lv_point_t pt = {0, 0};
    lv_indev_t *in = lv_indev_active();
    if (in) lv_indev_get_point(in, &pt);
    s_com_vx += pt.x < CX ? -1.2f : 1.2f;
}
static void cometa_empezar(bool cita)
{
    if (s_com_on || !s_pet) return;
    cerrar_panel();
    s_com_on = true;
    s_com_cita = cita;
    s_com_t0 = s_ms;
    s_com_x = s_mascota.x; s_com_y = 90; s_com_vx = 0; s_com_viento = 0; s_com_borde = 0;
    ocultar(s_com, false); ocultar(s_com_hilo, false); ocultar(s_com_capa, false);
    lv_obj_move_foreground(s_com_capa);
    lv_obj_move_foreground(s_com_hilo);
    lv_obj_move_foreground(s_com);
    accion(UI_ACCION_SONAR, SND_LANZAR);
    if (!s_nino) globo(ICO_COMETA, "¡INCLINA EL ORBE PARA MANEJAR LA COMETA!", 3000);
}
static void cometa_suelo_cb(lv_event_t *e)
{
    (void)e;
    if (!s_pet) return;
    if (pet_special_ready(s_pet, ESP_COMETA, s_ultimo_t)) cometa_empezar(false);
    else if (!s_nino) globo(ICO_COMETA, "SIN VIENTO NO SE PUEDE VOLAR...", 2500);
}
static void animar_cometa(void)
{
    const pet_t *p = s_pet;
    if (!s_com) {
        s_com_suelo = lv_image_create(s_mundo);
        lv_image_set_src(s_com_suelo, art_icono(ICO_COMETA, 3));
        lv_image_set_rotation(s_com_suelo, 700);
        lv_obj_add_flag(s_com_suelo, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_ext_click_area(s_com_suelo, 10);
        lv_obj_add_event_cb(s_com_suelo, cometa_suelo_cb, LV_EVENT_CLICKED, NULL);
        s_com_capa = caja(s_mundo);
        lv_obj_set_size(s_com_capa, W, W);
        lv_obj_add_flag(s_com_capa, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(s_com_capa, cometa_toque_cb, LV_EVENT_PRESSED, NULL);
        s_com_hilo = caja(s_mundo); /* el hilo: puntitos blancos de ella a la cometa */
        lv_obj_set_size(s_com_hilo, W, W);
        lv_obj_remove_flag(s_com_hilo, LV_OBJ_FLAG_CLICKABLE);
        for (int k = 0; k < HILO_N; k++) {
            s_com_pto[k] = caja(s_com_hilo);
            lv_obj_set_size(s_com_pto[k], 4, 4);
            lv_obj_set_style_radius(s_com_pto[k], 2, 0);
            lv_obj_set_style_bg_color(s_com_pto[k], lv_color_hex(0xF4F4F8), 0);
            lv_obj_set_style_bg_opa(s_com_pto[k], LV_OPA_80, 0);
        }
        for (int k = 0; k < COLA_N; k++) { /* la cola de colores */
            static const uint32_t CL[COLA_N] = {0xF0506E, 0xFF9A3C, 0xFFD45C, 0x7FD36B, 0x6FB7FF, 0xB48CFF, 0xF0506E, 0xFF9A3C, 0xFFD45C};
            s_com_cola[k] = caja(s_mundo);
            int d = 16 - k;
            lv_obj_set_size(s_com_cola[k], d, d);
            lv_obj_set_style_radius(s_com_cola[k], LV_RADIUS_CIRCLE, 0);
            lv_obj_set_style_bg_color(s_com_cola[k], lv_color_hex(CL[k]), 0);
            lv_obj_set_style_bg_opa(s_com_cola[k], LV_OPA_COVER, 0);
            lv_obj_set_style_shadow_width(s_com_cola[k], 8, 0);
            lv_obj_set_style_shadow_color(s_com_cola[k], lv_color_hex(CL[k]), 0);
            ocultar(s_com_cola[k], true);
        }
        s_com = lv_image_create(s_mundo);
        lv_image_set_src(s_com, art_icono(ICO_COMETA, 4));
        ocultar(s_com, true); ocultar(s_com_hilo, true); ocultar(s_com_capa, true); ocultar(s_com_suelo, true);
    }
    bool zona = p && s_escena == ESC_COLINA && !s_interior && p->etapa != STAGE_HUEVO;
    bool suelo = zona && !s_com_on && (pet_special_ready(p, ESP_COMETA, s_ultimo_t) || pet_special_known(p, ESP_COMETA));
    ocultar(s_com_suelo, !suelo);
    if (suelo) lv_obj_set_pos(s_com_suelo, 318, 286);
    if (!s_com_on) return;
    if (!zona || p->dormida) { cometa_fin(false); return; }
    uint32_t t = s_ms - s_com_t0;
    /* el viento empuja a ratos; inclinando (o tocando) la corriges */
    if (t % 2400 < TICK_MS) s_com_viento = (azar01() * 2 - 1) * (1.0f + (float)t / 20000.0f);
    s_com_vx += s_com_viento * 0.06f + s_tilt_x * 0.5f;
    s_com_vx *= 0.94f;
    s_com_x += s_com_vx;
    if (s_com_x < 40) s_com_x = 40;
    if (s_com_x > W - 40) s_com_x = W - 40;
    s_com_y = 90 + sinf((float)t / 700.0f) * 14;
    bool borde = s_com_x < 90 || s_com_x > W - 90;
    if (!borde) s_com_borde = 0;
    else if (!s_com_borde) s_com_borde = s_ms;
    lv_obj_set_style_image_recolor(s_com, lv_color_hex(0xF0506E), 0);
    lv_obj_set_style_image_recolor_opa(s_com, borde ? (lv_opa_t)(80 + 60 * sinf((float)s_ms / 80.0f)) : 0, 0);
    lv_image_set_rotation(s_com, (int32_t)(s_com_vx * 60));
    lv_obj_set_pos(s_com, (int)s_com_x - 24, (int)s_com_y - 24);
    if (s_ms - s_cola_t > 55) { /* la cola sigue el camino de la cometa */
        s_cola_t = s_ms;
        for (int k = COLA_N - 1; k > 0; k--) { s_cola_x[k] = s_cola_x[k - 1]; s_cola_y[k] = s_cola_y[k - 1]; }
        s_cola_x[0] = s_com_x - s_com_vx * 2; s_cola_y[0] = s_com_y + 14;
        if (t < 120) for (int k = 0; k < COLA_N; k++) { s_cola_x[k] = s_cola_x[0]; s_cola_y[k] = s_cola_y[0]; }
    }
    for (int k = 0; k < COLA_N; k++) {
        ocultar(s_com_cola[k], false);
        lv_obj_set_pos(s_com_cola[k], (int)s_cola_x[k] - (16 - k) / 2, (int)s_cola_y[k] + k * 3 - (16 - k) / 2);
        lv_obj_set_style_opa(s_com_cola[k], (lv_opa_t)(220 - k * 22), 0);
        lv_obj_move_foreground(s_com_cola[k]);
    }
    lv_obj_move_foreground(s_com);
    if (s_ms % 160 < TICK_MS) chispas((int)s_com_x, (int)s_com_y + 10, 0xFFF0A0, 1, 18);
    for (int k = 0; k < HILO_N; k++) { /* el hilo cuelga un poquito */
        float f = (float)(k + 1) / (HILO_N + 1);
        float hx = s_mascota.x + (s_com_x - s_mascota.x) * f, hy = (float)cabeza_y(&s_mascota) + 8 + (s_com_y + 20 - (float)cabeza_y(&s_mascota) - 8) * f + sinf(f * 3.1416f) * 14;
        lv_obj_set_pos(s_com_pto[k], (int)hx - 1, (int)hy - 1);
    }
    s_mascota.tx = s_com_x * 0.5f + CX * 0.5f; s_mascota.ty = 290;
    if (s_mascota.modo != M_VA) mascota_modo(M_VA, 2000);
    if (s_com_borde && s_ms - s_com_borde > 1600) { cometa_fin(false); return; }
    if (t > 30000) cometa_fin(true);
}

/* --- los cristales que cantan, en la cueva --- */
#define CRISTALES 6
static lv_obj_t *s_cris[CRISTALES], *s_cris_luz, *s_cris_brillo[CRISTALES], *s_cris_luz_c[4];
static uint32_t s_cris_toque[CRISTALES], s_cris_luz_hasta;
static const uint32_t COLOR_CRISTAL[CRISTALES] = {0x5AA8FF, 0x6ADC8A, 0xFF8FC0, 0xFFD45C, 0xB48AF0, 0xF4F4FF};
static const int16_t POS_CRISTAL[CRISTALES][2] = {{96, 236}, {150, 196}, {214, 176}, {282, 182}, {340, 204}, {378, 248}};
/* el dragón de cristal no siempre está: una de cada cuatro tandas de tres horas anda de paseo (la primera vez, que lo despiertan, siempre está) */
static int s_drag_forzar; /* 0: según la hora, 1: ausente, 2: presente (para probar) */
static bool dragon_ausente(void)
{
    if (!s_pet || !pet_special_known(s_pet, ESP_CRISTALES)) return false;
    if (s_drag_forzar) return s_drag_forzar == 1;
    uint32_t b = (s_ultimo_t / (3u * 3600u)) * 2654435761u;
    return ((b >> 13) & 3u) == 0;
}
static lv_obj_t *s_cueva_drag;
static art_img_t s_cueva_drag_art;
static int s_cueva_drag_f = -1;
static uint32_t s_cueva_saluda;
static void cueva_drag_cb(lv_event_t *e)
{
    (void)e;
    s_cueva_saluda = s_ms + 2500;
    accion(UI_ACCION_SONAR, SND_HADAS);
    accion(UI_ACCION_PRISMA, 0); /* (v37) a lo mejor hoy le cuenta un capítulo */
    for (int k = 0; k < 4; k++) flotar(ICO_CORAZON, NULL, 0, CX - 30 + k * 20, 70, (uint32_t)k * 150);
    if (!s_nino) globo(ICO_CRISTAL, "¡EL DRAGÓN DE CRISTAL ME SALUDÓ!", 2500);
}
static void cristal_cb(lv_event_t *e)
{
    int k = (int)(intptr_t)lv_event_get_user_data(e);
    if (!s_pet) return;
    static uint32_t efecto_ms;
    const bool seguido = s_ms - efecto_ms < 260; /* tocando rápido, los efectos se saltan: cada onda y cada chispa son objetos nuevos y todo se pegaba */
    s_cris_toque[k] = s_ms;
    if (!seguido) {
        efecto_ms = s_ms;
        onda(POS_CRISTAL[k][0], POS_CRISTAL[k][1] - 20, COLOR_CRISTAL[k], 80, false, 0);
        chispas(POS_CRISTAL[k][0], POS_CRISTAL[k][1] - 30, COLOR_CRISTAL[k], 4, 14);
        flotar(ICO_NOTA, NULL, 0, POS_CRISTAL[k][0], POS_CRISTAL[k][1] - 50, 0);
    }
    accion(UI_ACCION_SONAR, SND_CRISTAL_AZUL + k);
    accion(UI_ACCION_CRISTAL, k);
    if (s_cita_on && s_cita_que == CITA_MUSICA) s_cita_pts++;
}
static void cristales_luz(void) /* la luz escondida: se despierta con la canción */
{
    s_cris_luz_hasta = s_ms + 20000;
    for (int k = 0; k < CRISTALES; k++) s_cris_toque[k] = s_ms + (uint32_t)k * 150;
    accion(UI_ACCION_SONAR, SND_HADAS);
}
static void animar_cristales(void)
{
    const pet_t *p = s_pet;
    if (!s_cris[0]) {
        /* la luz del corazón de cristal: cuatro discos planos y translúcidos, uno dentro de otro. Antes era un disco con sombra difuminada de 80 px
         * y opacidad animada: cada cuadro se dibujaba en una capa de ~290x290 px y la cueva con el dragón iba a 3 cuadros por segundo. */
        s_cris_luz = caja(s_mundo);
        lv_obj_set_size(s_cris_luz, 260, 260);
        lv_obj_set_style_bg_opa(s_cris_luz, LV_OPA_TRANSP, 0);
        lv_obj_add_flag(s_cris_luz, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
        lv_obj_remove_flag(s_cris_luz, LV_OBJ_FLAG_CLICKABLE);
        for (int i = 0; i < 4; i++) {
            static const int D[4] = {260, 210, 150, 90};
            s_cris_luz_c[i] = caja(s_cris_luz);
            lv_obj_set_size(s_cris_luz_c[i], D[i], D[i]);
            lv_obj_center(s_cris_luz_c[i]);
            lv_obj_set_style_radius(s_cris_luz_c[i], LV_RADIUS_CIRCLE, 0);
            lv_obj_set_style_bg_color(s_cris_luz_c[i], lv_color_hex(i == 3 ? 0xFFF6D0 : 0xFFE27A), 0);
            lv_obj_set_style_bg_opa(s_cris_luz_c[i], LV_OPA_TRANSP, 0);
            lv_obj_remove_flag(s_cris_luz_c[i], LV_OBJ_FLAG_CLICKABLE);
        }
        ocultar(s_cris_luz, true);
        for (int k = 0; k < CRISTALES; k++) {
            s_cris_brillo[k] = caja(s_mundo); /* el brillo de cada cristal, detrás de él */
            lv_obj_set_size(s_cris_brillo[k], 34, 34);
            lv_obj_set_style_radius(s_cris_brillo[k], LV_RADIUS_CIRCLE, 0);
            lv_obj_set_style_bg_color(s_cris_brillo[k], lv_color_hex(COLOR_CRISTAL[k]), 0);
            lv_obj_set_style_bg_opa(s_cris_brillo[k], LV_OPA_40, 0);
            lv_obj_set_size(s_cris_brillo[k], 44, 44); /* (sin sombra difusa: cada una frenaba el dibujo en la placa; el disco plano alcanza) */
            ocultar(s_cris_brillo[k], true);
            s_cris[k] = lv_image_create(s_mundo);
            lv_image_set_src(s_cris[k], art_icono(ICO_CRISTAL, 3));
            lv_obj_set_style_image_recolor(s_cris[k], lv_color_hex(COLOR_CRISTAL[k]), 0);
            lv_obj_set_style_image_recolor_opa(s_cris[k], LV_OPA_60, 0);
            lv_obj_add_flag(s_cris[k], LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_ext_click_area(s_cris[k], 8);
            lv_obj_add_event_cb(s_cris[k], cristal_cb, LV_EVENT_PRESSED, (void *)(intptr_t)k); /* suena al apoyar el dedo, no al soltarlo */
            ocultar(s_cris[k], true);
        }
    }
    bool ver = p && s_escena == ESC_CUEVA && !s_interior && p->etapa != STAGE_HUEVO;
    /* a veces el dragón no está: ella se extraña (una sola vez por entrada) */
    static uint32_t s_cueva_entro;
    static bool s_cueva_dijo;
    if (!ver) { s_cueva_entro = 0; s_cueva_dijo = false; }
    else {
        if (!s_cueva_entro) s_cueva_entro = s_ms ? s_ms : 1;
        if (!s_cueva_dijo && s_ms - s_cueva_entro > 1800 && s_panel == PANEL_NINGUNO && !s_sim_on) {
            s_cueva_dijo = true;
            if (dragon_ausente()) {
                static const char *const X[3] = {"¿Y EL DRAGÓN? ¡QUÉ RARO, HOY NO ESTÁ!", "QUÉ EXTRAÑO... EL DRAGÓN NO ESTÁ AQUÍ", "¿DÓNDE ESTARÁ EL DRAGÓN? ¡QUÉ RARO!"};
                globo(ICO_CRIS_4, s_nino ? NULL : X[azar() % 3], 4200);
            }
        }
    }
    for (int k = 0; k < CRISTALES; k++) {
        ocultar(s_cris[k], !ver);
        ocultar(s_cris_brillo[k], !ver);
        if (!ver) continue;
        int32_t dt = (int32_t)(s_ms - s_cris_toque[k]);
        float brillo = dt >= 0 && dt < 700 ? 1.0f - (float)dt / 700.0f : 0;
        ocultar(s_cris_brillo[k], false);
        lv_obj_set_pos(s_cris_brillo[k], POS_CRISTAL[k][0] - 22, POS_CRISTAL[k][1] - 41);
        { /* sin opacidad de objeto (obligaba a dibujar cada brillo en una capa aparte): solo el relleno y la sombra */
            const int o = (int)(60 + 30 * sinf((float)s_ms / 650.0f + (float)k * 1.3f) + brillo * 165);
            const int oq = (o > 255 ? 255 : o) / 24 * 24; /* a saltos: cada cambio repinta */
            lv_obj_set_style_bg_opa(s_cris_brillo[k], (lv_opa_t)(oq * 100 / 255), 0);
        }
        /* (solo si hace falta: mover un objeto de lugar repinta todo el mundo, y esto se hacía 6 veces en cada cuadro) */
        if (lv_obj_get_index(s_cris_brillo[k]) + 1 != lv_obj_get_index(s_cris[k])) lv_obj_move_to_index(s_cris_brillo[k], lv_obj_get_index(s_cris[k]));
        lv_obj_set_pos(s_cris[k], POS_CRISTAL[k][0] - 18, POS_CRISTAL[k][1] - 36 - (int)(brillo * 4));
        lv_obj_set_style_image_recolor_opa(s_cris[k], (lv_opa_t)(150 - brillo * 120), 0);
    }
    /* el dragón de cristal, cuando ya lo despertaron: duerme al fondo de la cueva, a ratos abre los ojos; si lo tocas, saluda */
    bool drag = ver && pet_special_known(p, ESP_CRISTALES) && !dragon_ausente();
    bool luz = ver && ((int32_t)(s_cris_luz_hasta - s_ms) > 0 || drag);
    ocultar(s_cris_luz, !luz);
    if (ver && (int32_t)(s_cris_luz_hasta - s_ms) > 0) { /* la canción de la luz: ondas y polvo dorado desde el centro */
        if (s_ms % 800 < TICK_MS) { onda(CX, 150, 0xFFE27A, 150, false, 0); onda(CX, 250, 0xFFF6D0, 160, true, 200); }
        if (s_ms % 160 < TICK_MS) chispas(CX, 200, 0xFFE27A, 1, 150);
    }
    if (luz) {
        lv_obj_set_pos(s_cris_luz, CX - 130, 115 - 130);
        { /* respira: más o menos relleno en cada disco (sin opacidad de objeto) */
            const float f = ((drag ? 70.0f : 150.0f) + 60.0f * sinf((float)s_ms / 400.0f)) / 255.0f;
            static const uint8_t BASE[4] = {14, 22, 34, 60};
            for (int i = 0; i < 4; i++) lv_obj_set_style_bg_opa(s_cris_luz_c[i], (lv_opa_t)(BASE[i] * f * 1.6f), 0);
        }
    }
    if (!s_cueva_drag && drag) {
        s_cueva_drag = lv_image_create(s_mundo);
        lv_obj_add_flag(s_cueva_drag, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(s_cueva_drag, cueva_drag_cb, LV_EVENT_CLICKED, NULL);
    }
    if (s_cueva_drag) {
        ocultar(s_cueva_drag, !drag);
        if (!drag) return;
        int f = (int32_t)(s_cueva_saluda - s_ms) > 0 ? 2 : ((s_ms / 1000) % 20 < 4 ? 1 : 0);
        if (f != s_cueva_drag_f) {
            s_cueva_drag_f = f;
            art_img_free(&s_cueva_drag_art);
            art_dragon_pintado(&s_cueva_drag_art, f, 1);
            poner_src(s_cueva_drag, &s_cueva_drag_art.dsc);
        }
        lv_obj_set_pos(s_cueva_drag, CX - 48, 44 + (int)(sinf((float)s_ms / 900.0f) * 2));
    }
}
/* la luz del corazón de cristal: de noche ella brilla suavecito.
 * v88: antes era una caja con sombra de LVGL (40 px) cuya opacidad cambiaba en cada cuadro: en la placa se veía como anillos
 * (la sombra en 16 bits se escalona), la dejaba pálida si quedaba delante de ella y cada cuadro redibujaba la sombra entera (tirones).
 * Ahora es una imagen de luz suave hecha una sola vez (con un poco de ruido para que no se vean escalones), SIEMPRE detrás de ella,
 * y su brillo late a lo más 8 veces por segundo. */
static lv_obj_t *s_aura_drag;
static art_img_t s_aura_drag_art;
static void aura_dragon_imagen(void)
{
    enum { L = 96 };
    if (s_aura_drag_art.px) return;
    s_aura_drag_art.px = malloc((size_t)L * L * 4);
    if (!s_aura_drag_art.px) return;
    s_aura_drag_art.capacidad = L * L;
    uint32_t sem = 0x9E3779B9u;
    for (int y = 0; y < L; y++)
        for (int x = 0; x < L; x++) {
            const float dx = (float)x - (L - 1) * 0.5f, dy = (float)y - (L - 1) * 0.5f;
            float r = sqrtf(dx * dx + dy * dy) / (L * 0.5f);
            float a = r >= 1.0f ? 0.0f : (1.0f - r) * 160.0f; /* se apaga parejo hacia el borde (el centro queda detrás de ella) */
            sem = sem * 1664525u + 1013904223u;
            a += (float)((int)(sem >> 28) - 8) * 0.35f; /* ruido de ±3: sin escalones */
            const uint32_t al = a <= 0 ? 0u : (a >= 255 ? 255u : (uint32_t)a);
            s_aura_drag_art.px[y * L + x] = (al << 24) | 0xD8C4FFu;
        }
    lv_image_dsc_t *dsc = &s_aura_drag_art.dsc;
    memset(dsc, 0, sizeof(*dsc));
    dsc->header.magic = LV_IMAGE_HEADER_MAGIC;
    dsc->header.cf = LV_COLOR_FORMAT_ARGB8888;
    dsc->header.w = L;
    dsc->header.h = L;
    dsc->header.stride = L * 4;
    dsc->data_size = L * L * 4;
    dsc->data = (const uint8_t *)s_aura_drag_art.px;
}
static void animar_aura_dragon(void)
{
    const pet_t *p = s_pet;
    bool ver = p && pet_special_known(p, ESP_CRISTALES) && s_fondo_periodo == PERIODO_NOCHE && s_mascota.visible && p->etapa != STAGE_HUEVO;
    if (!s_aura_drag) {
        if (!ver) return;
        aura_dragon_imagen();
        if (!s_aura_drag_art.px) return;
        s_aura_drag = lv_image_create(s_mundo);
        lv_obj_remove_flag(s_aura_drag, LV_OBJ_FLAG_CLICKABLE);
        lv_image_set_src(s_aura_drag, &s_aura_drag_art.dsc);
        lv_image_set_pivot(s_aura_drag, 0, 0);
    }
    ocultar(s_aura_drag, !ver);
    if (!ver) return;
    const int d = lado(&s_mascota) * 12 / 10;
    lv_image_set_scale(s_aura_drag, (uint32_t)(d * 256 / 96));
    lv_obj_set_pos(s_aura_drag, (int)s_mascota.x - d / 2, (int)s_mascota.y - lado(&s_mascota) / 2 - d / 2 + 6);
    static uint32_t t_opa;
    if (s_ms - t_opa >= 125u) { t_opa = s_ms; lv_obj_set_style_image_opa(s_aura_drag, (lv_opa_t)(170 + 60 * sinf((float)s_ms / 900.0f)), 0); }
    /* siempre detrás de ella (justo antes de su imagen), nunca delante */
    const int32_t ip = lv_obj_get_index(s_mascota.img), ia = lv_obj_get_index(s_aura_drag);
    if (ip >= 0 && ia >= 0 && ia > ip) lv_obj_move_to_index(s_aura_drag, ip);
    else if (ip >= 0 && ia >= 0 && ia < ip - 1) lv_obj_move_to_index(s_aura_drag, ip - 1);
}

/* ======================================================================
 * Cada cuadro y los avisos del motor
 * ==================================================================== */
static void animar_cristales_juego(void);
static void animar_mundo23(void)
{
    { /* si algo opaco tapa todo (el juego de cristales, los finales), el mundo de abajo no se pinta */
        const bool tapa = s_sim_on || s_cru_activo || s_panel == PANEL_FINAL || s_panel == PANEL_CINE || (s_picnic_on && s_picnic_tapa) || (s_juego == JG_FRUTAS && lv_obj_has_flag(s_juego_pintura, LV_OBJ_FLAG_HIDDEN)) || s_juego == JG_HONGOS || s_pez_on; /* (v89: también los hongos y la pesca) */
        if (tapa != lv_obj_has_flag(s_mundo, LV_OBJ_FLAG_HIDDEN)) {
            if (tapa) { lv_obj_add_flag(s_fondo, LV_OBJ_FLAG_HIDDEN); lv_obj_add_flag(s_mundo, LV_OBJ_FLAG_HIDDEN); }
            else { lv_obj_remove_flag(s_fondo, LV_OBJ_FLAG_HIDDEN); lv_obj_remove_flag(s_mundo, LV_OBJ_FLAG_HIDDEN); }
        }
    }
    animar_cristales_juego();
    animar_mensajero();
    animar_criatura();
    animar_historia();
    animar_cita();
    animar_efectos_luz();
    animar_anillo();
    animar_reflejo();
    animar_cometa();
    animar_cristales();
    animar_aura_dragon();
    animar_fiesta();
    if (s_hist_pend_c >= 0 && s_panel == PANEL_NINGUNO && !s_cita_on && !s_com_on && !s_cie_activo && s_mascota.modo != M_TRAE) { /* la historia que quedó esperando */
        int c = s_hist_pend_c;
        s_hist_pend_c = -1;
        panel_historia(c, s_hist_pend_k);
    }
    tuto_libro_revisar();
}
static void magia_update(const pet_t *p) { mensajero_revisar(p); carta_mundo_animar(); botella_animar(); regadera_animar(); fiesta_revisar(p, s_ultimo_t); }

static void evento_magia(const pet_event_t *e, int hx, int hy)
{
    if (e->type == EV_HIST_NIMBA) { s_hist_pend_c = CRI_NIMBA; s_hist_pend_k = (uint8_t)e->a % 5; return; } /* en el paseo: al volver se abre la hoja */
    const pet_t *p = s_pet;
    if (!p) return;
    char t[128];
    switch (e->type) {
    case EV_ANIMAL_NUEVO: { /* lo conocieron juntos */
        int f = (uint8_t)e->a;
        /* (fw 108) hay animales en plural («las hormigas»): con ellos la frase va en plural, no «es las hormigas» */
        const char *nom = pet_card_name(f);
        const bool varios = !strncmp(nom, "las ", 4) || !strncmp(nom, "los ", 4);
        static const char *const FR1[] = {"¡Wow, es %s!", "¡Mira, es %s!", "¡Uau! ¡%s!", "¡Oh, es %s!", "¡Wow! ¡%s!", "¡Mira! ¡%s!", "¡Ooh, es %s!"}; /* varias frases cortas al azar */
        static const char *const FRN[] = {"¡Wow, son %s!", "¡Mira, son %s!", "¡Uau! ¡%s!", "¡Oh, son %s!", "¡Wow! ¡%s!", "¡Mira! ¡%s!", "¡Ooh, son %s!"};
        snprintf(t, sizeof t, (varios ? FRN : FR1)[azar() % (sizeof FR1 / sizeof FR1[0])], nom);
        mayusculas(t);
        art_icono_t ic = f < FAUNA_COUNT ? ICO_FAUNA[f] : ICO_ANIMAL[f - FAUNA_COUNT];
        globo(ic, s_nino ? NULL : t, 3000);
        flotar(ICO_ESTRELLA, NULL, 0, hx, hy - 20, 0);
        accion(UI_ACCION_SONAR, SND_NUEVO);
        break;
    }
    case EV_CRIATURA: {
        int c = (uint8_t)e->a % CRI_COUNT, que = e->b & 15;
        bool conoce = (p->criaturas_vistas >> c) & 1u;
        snprintf(t, sizeof t, "%s", pet_creature_name((magica_t)c));
        mayusculas(t);
        char g[160];
        switch (que) {
        case CRS_ASOMA:
            if (conoce) snprintf(g, sizeof g, "¡MIRA, ES %s!", t); else snprintf(g, sizeof g, "¿QUÉ ES ESA LUCECITA?");
            if (s_escena == (int)p->escena) { globo(conoce ? ICO_CRIATURA[c] : ICO_PREGUNTA, s_nino ? NULL : g, 3000); accion(UI_ACCION_SONAR, SND_RARO); }
            break;
        case CRS_ESPERA:
            snprintf(g, sizeof g, "¡%s NOS ESTÁ ESPERANDO!", t);
            globo(ICO_CRIATURA[c], s_nino ? NULL : g, 3500);
            accion(UI_ACCION_SONAR, SND_HADAS);
            break;
        case CRS_HOLA:
            snprintf(g, sizeof g, "¡HOLA, %s!", t);
            globo(ICO_CRIATURA[c], s_nino ? NULL : g, 2500);
            if (pet_final_seen(p, PET_FINAL_SECRETO) && azar() % 3 == 0 && !rep_activo()) { /* (v39) a veces tararea la canción de Tilo */
                uint8_t ts[PET_CANCION_NOTAS];
                const int tn = pet_tilo_song(p, ts, PET_CANCION_NOTAS);
                if (tn) {
                    rep_tocar(ts, tn, 1 | 4 | 8); /* (REP_CRISTAL | REP_SUAVE | REP_FLOTAR) */
                    snprintf(g, sizeof g, "%s TARAREA LA CANCIÓN DE PEUMO", t);
                    globo(ICO_NOTA, s_nino ? NULL : g, 3500);
                }
            }
            break;
        case CRS_SIN_HISTORIAS: { /* bosteza y se despide */
            if (((e->b >> 4) & 1) && pet_final_seen(p, PET_FINAL_SECRETO)) snprintf(g, sizeof g, "%s", pet_creature_after((magica_t)c)); /* (v37) después del final */
            else snprintf(g, sizeof g, ((e->b >> 4) & 1) ? "YA TE CONTÉ TODAS MIS HISTORIAS..." : (pet_story_moments_left(p, s_ultimo_t) ? "LO QUE SIGUE TE LO CUENTO OTRO DÍA..." : "HOY YA NO ME QUEDAN HISTORIAS..."));
            mayusculas(g);
            globo(ICO_CRIATURA[c], s_nino ? NULL : g, 2600);
            accion(UI_ACCION_SONAR, SND_BOSTEZO);
            snprintf(g, sizeof g, "*BOSTEZO* ¡HASTA PRONTO!");
            globo(ICO_CRIATURA[c], s_nino ? NULL : g, 2600);
            break;
        }
        case CRS_HISTORIA: /* le cuenta una historia: se abre la hoja cuando la pantalla quede libre */
            if (pet_book_tutorial_pending(s_pet, NULL)) s_tuto_fase = 1;
            s_hist_pend_c = c;
            s_hist_pend_k = (e->b >> 4) & 7;
            break;
        default: break;
        }
        break;
    }
    case EV_ESPECIAL: {
        static const char *const DESC[ESP_COUNT] = {"¡LAS HADAS BAILARON CONMIGO Y ME DEJARON ALGO!", "¡LA LAGUNA TIENE SUS PROPIAS ESTRELLAS!",
                                                   "¡LA COMETA LLEGÓ TAN ALTO QUE TRAJO ALGO DE LEJOS!", "¡DESPERTAMOS AL DRAGÓN DE CRISTAL!"};
        int esp = (uint8_t)e->a % ESP_COUNT, que = e->b & 15;
        if (que == ESS_DESCUBRE) { s_cine_pend = esp + 1; s_cine_listo = s_ms + 2000; libro_pedir(4); if (esp == ESP_CRISTALES) libro_pedir(5); }
        if (esp == ESP_CRISTALES && (que == ESS_DESCUBRE || que == ESS_OTRA_VEZ) && !s_sim_on) cristales_luz();
        if (que == ESS_DESCUBRE && esp != ESP_REFLEJO && esp != ESP_CRISTALES && s_panel == PANEL_NINGUNO) { globo(ICO_ESTRELLA, s_nino ? NULL : DESC[esp], 4000); for (int k = 0; k < 6; k++) flotar(ICO_ESTRELLA, NULL, 0, hx - 50 + k * 20, hy - 30, (uint32_t)k * 120); }
        break;
    }
    case EV_PISTA: { /* comenta una pista del secreto */
        if (s_nino || s_panel != PANEL_NINGUNO) break;
        snprintf(t, sizeof t, "%s", pet_hint_text((especial_t)((uint8_t)e->a % ESP_COUNT), e->b));
        mayusculas(t);
        static const art_icono_t IC[ESP_COUNT] = {ICO_HONGO_LUZ, ICO_ESTRELLA, ICO_COMETA, ICO_CRISTAL};
        globo(IC[(uint8_t)e->a % ESP_COUNT], t, 4500);
        break;
    }
    default: break;
    }
}

/* ======================================================================
 * Tesoros: los animales y las criaturas que conocieron (solo los descubiertos)
 * ==================================================================== */
static void ficha_cb(lv_event_t *e)
{
    int f = (int)(intptr_t)lv_event_get_user_data(e);
    char ti[64], tx[300];
    snprintf(ti, sizeof ti, "%s", pet_card_name(f));
    snprintf(tx, sizeof tx, "%s", pet_card_story(f));
    mayusculas(ti); mayusculas(tx);
    art_icono_t ic = f < FAUNA_COUNT ? ICO_FAUNA[f] : ICO_ANIMAL[f - FAUNA_COUNT];
    nota_mostrar(ic, 0, ti, tx);
}
static void criatura_tesoro_cb(lv_event_t *e)
{
    int q = (int)(intptr_t)lv_event_get_user_data(e), c = q / 8, k = q % 8;
    if (!s_pet || (unsigned)c >= CRI_COUNT) return;
    if (k < 5) { panel_historia(c, k); return; }
    char ti[48], tx[300];
    snprintf(ti, sizeof ti, "%s", pet_creature_name((magica_t)c));
    snprintf(tx, sizeof tx, "%s", pet_creature_about((magica_t)c));
    mayusculas(ti); mayusculas(tx);
    nota_mostrar(ICO_CRIATURA[c], 0, ti, tx);
}
static void tesoros_cabecera(lv_obj_t *ar, const char *nombre)
{
    if (s_nino || !ts_toca()) return;
    lv_obj_t *cab = caja(ar);
    lv_obj_set_size(cab, 300, 26);
    lv_obj_t *t = texto(cab, nombre, C_VIOLETA, 2, 260);
    lv_obj_align(t, LV_ALIGN_LEFT_MID, 6, 2);
}
static void tesoros_magia(lv_obj_t *ar)
{
    const pet_t *p = s_pet;
    int n = pet_cards_known(p);
    if (n) {
        tesoros_cabecera(ar, "ANIMALES");
        for (int i = n - 1; i >= 0; i--) { /* el más nuevo arriba, con su estrellita */
            if (!ts_toca()) continue;
            int f = pet_card_get(p, i);
            art_icono_t ic = f < FAUNA_COUNT ? ICO_FAUNA[f] : ICO_ANIMAL[f - FAUNA_COUNT];
            lv_obj_t *b = boton(ar, s_nino ? 56 : 300, s_nino ? 52 : 44, ficha_cb, f);
            lv_obj_add_flag(b, LV_OBJ_FLAG_SCROLL_CHAIN_VER);
            lv_obj_t *im = icono(b, ic, 2);
            lv_obj_align(im, s_nino ? LV_ALIGN_CENTER : LV_ALIGN_LEFT_MID, s_nino ? 0 : 12, 0);
            if (!s_nino) {
                char nm[48];
                snprintf(nm, sizeof nm, "%s", pet_card_name(f));
                mayusculas(nm);
                lv_obj_t *t = texto(b, nm, C_BLANCO, 2, 210);
                lv_obj_align(t, LV_ALIGN_LEFT_MID, 50, 0);
            }
            if (i == n - 1) { lv_obj_t *st = icono(b, ICO_ESTRELLA, 1); lv_obj_align(st, LV_ALIGN_TOP_RIGHT, -6, 4); }
        }
    }
    if (p->criaturas_vistas) {
        tesoros_cabecera(ar, "CRIATURAS MÁGICAS");
        for (int c = 0; c < CRI_COUNT; c++) {
            if (!((p->criaturas_vistas >> c) & 1u)) continue;
            if (ts_toca()) {
                lv_obj_t *b = boton(ar, s_nino ? 56 : 300, s_nino ? 52 : 44, criatura_tesoro_cb, c * 8 + 7);
                lv_obj_add_flag(b, LV_OBJ_FLAG_SCROLL_CHAIN_VER);
                lv_obj_t *im = icono(b, ICO_CRIATURA[c], 2);
                lv_obj_align(im, s_nino ? LV_ALIGN_CENTER : LV_ALIGN_LEFT_MID, s_nino ? 0 : 12, 0);
                if (!s_nino) {
                    char nm[48];
                    snprintf(nm, sizeof nm, "%s", pet_creature_name((magica_t)c));
                    mayusculas(nm);
                    lv_obj_t *t = texto(b, nm, C_BLANCO, 2, 210);
                    lv_obj_align(t, LV_ALIGN_LEFT_MID, 50, 0);
                }
            }
            if (s_nino) continue;
            for (int k = 0; k < 5; k++) { /* sus historias, para releerlas */
                if (!((p->historias[c] >> k) & 1u)) continue;
                if (!ts_toca()) continue;
                lv_obj_t *h = boton(ar, 280, 40, criatura_tesoro_cb, c * 8 + k);
                lv_obj_add_flag(h, LV_OBJ_FLAG_SCROLL_CHAIN_VER);
                lv_obj_set_style_bg_color(h, lv_color_hex(0x3A3460), 0);
                lv_obj_t *li = icono(h, ICO_LIBRO, 1);
                lv_obj_align(li, LV_ALIGN_LEFT_MID, 14, 0);
                char tt[64];
                snprintf(tt, sizeof tt, "%s", pet_story_title((magica_t)c, k));
                mayusculas(tt);
                lv_obj_t *ht = texto(h, tt, 0xFFE27A, 2, 220);
                const lv_image_dsc_t *dd = lv_image_get_src(ht);
                if (dd && (int)dd->header.h + 16 > 40) lv_obj_set_height(h, (int32_t)dd->header.h + 16); /* los títulos largos, en dos líneas */
                lv_obj_align(ht, LV_ALIGN_LEFT_MID, 40, 0);
            }
        }
    }
}
/* para probar en el simulador: empezar una cita, el anillo o la cometa aunque no toque */
void ui_demo_cita(int que) { cita_empezar(que); }
void ui_demo_anillo(void) { s_anillo_baile = s_ms; s_mascota.tx = ANILLO_X; s_mascota.ty = ANILLO_Y + 6; mascota_modo(M_VA, 2500); }
void ui_demo_cometa(void) { cometa_empezar(false); }

/* ======================================================================
 * El juego de los cristales (Simón dice), en la cueva: se elige en Jugar
 * Tres rondas cada vez más largas; equivocarse no quita nada (se repite la misma). Cada ronda ganada toca una
 * canción con las luces al ritmo; la tercera despierta al dragón de cristal (el secreto de la cueva).
 * ==================================================================== */
#define SIM_CRIS 6
#define CRIS_TOT 8 /* (v39) más los dos de las orillas: el celeste (6, grave) y el morado (7, campanita). El juego de memoria usa los 6 del centro */
enum { SIM_MUESTRA = 0, SIM_TURNO, SIM_FALLO, SIM_CANCION, SIM_DRAGON, SIM_FIN };
static lv_obj_t *s_sim, *s_sim_cris[SIM_CRIS], *s_sim_t, *s_sim_drag, *s_sim_x, *s_sim_luz, *s_sim_fondo, *s_sim_brillo[CRIS_TOT], *s_sim_borde[2];
static art_img_t s_sim_cueva; /* la cueva pintada (se libera al salir) */
static art_img_t s_sim_t_art, s_sim_drag_art[3];
static int s_sim_fase, s_sim_ronda, s_sim_paso, s_sim_largo, s_sim_ganadas, s_sim_drag_fase = -1;
static uint8_t s_sim_seq[8];
static uint32_t s_sim_t0, s_sim_luz_k[CRIS_TOT];
static const int16_t SIM_POS[CRIS_TOT][3] = { /* x, y (la base) y escala: tres grandes al centro, tres chicos alrededor; (v39) los dos de las orillas, pintados en el fondo */
    {148, 362, 6}, {233, 376, 7}, {318, 362, 6}, {108, 300, 4}, {233, 298, 4}, {358, 300, 4}, {38, 226, 4}, {430, 216, 4}};
static const uint32_t SIM_COLOR[CRIS_TOT] = {0x5AA8FF, 0x6ADC8A, 0xFF8FC0, 0xFFD45C, 0xB48AF0, 0xF4F4FF, 0x8FF0F0, 0xC88AF8};
static sound_t snd_cristal(int k) { return k >= SIM_CRIS ? (sound_t)(SND_CRISTAL_CELESTE + (k - SIM_CRIS) % 2) : (sound_t)(SND_CRISTAL_AZUL + k % SIM_CRIS); }
/* tres canciones (propias), una por ronda: qué cristal suena en cada pulso */
static const int8_t SIM_TEMAS[3][16] = {
    {0, 1, 2, 1, 0, 1, 2, 3, 2, 1, 0, -1, 4, 5, 3, -1},
    {2, 3, 4, 3, 2, 1, 0, 1, 2, 2, 5, 3, 1, 0, -1, -1},
    {0, 2, 4, 5, 4, 2, 3, 1, 3, 5, 4, 2, 1, 3, 0, 5}};
static void sim_texto(const char *t)
{
    if (!t || s_nino) { ocultar(s_sim_t, true); return; }
    art_texto(&s_sim_t_art, t, C_BLANCO, 2, 300);
    poner_src(s_sim_t, &s_sim_t_art.dsc);
    lv_obj_align(s_sim_t, LV_ALIGN_TOP_MID, 0, s_sim_fase >= SIM_DRAGON ? 208 : 66); /* con el dragón, justo debajo de él */
    ocultar(s_sim_t, false);
}
static void sim_encender(int k) { if ((unsigned)k < CRIS_TOT) { s_sim_luz_k[k] = s_ms; accion(UI_ACCION_SONAR, snd_cristal(k)); } }
static void sim_nueva_ronda(void)
{
    static const uint8_t LARGO[3] = {3, 5, 7}, LARGO_CHICO[3] = {2, 3, 4};
    s_sim_largo = s_nino ? LARGO_CHICO[s_sim_ronda] : LARGO[s_sim_ronda];
    for (int i = 0; i < s_sim_largo; i++) {
        int k;
        do k = (int)(azar() % SIM_CRIS); while (i && k == s_sim_seq[i - 1]); /* nunca el mismo dos veces seguidas */
        s_sim_seq[i] = (uint8_t)k;
    }
    s_sim_fase = SIM_MUESTRA; s_sim_t0 = s_ms + 600; s_sim_paso = 0;
    static const char *const T[3] = {"MIRA Y ESCUCHA...", "¡AHORA UNA MÁS LARGA!", "¡LA ÚLTIMA! ESCUCHA BIEN"};
    sim_texto(T[s_sim_ronda]);
}
static uint32_t s_sim_inicio;
bool ui_silencio_ambiente(void) { return s_sim_on || s_juego == JG_FRUTAS; } /* en el juego de cristales y el de frutas solo suenan los sonidos del juego */
/* qué escena especial hay (para el registro): 1 cristales, 2 cruce en bote, 4 final, 8 globo de texto a la vista, 16 picnic */
int ui_escena_especial(void) { return (s_sim_on ? 1 : 0) | (s_cru_activo ? 2 : 0) | (s_panel == PANEL_FINAL ? 4 : 0) | (lv_obj_has_flag(s_globo, LV_OBJ_FLAG_HIDDEN) ? 0 : 8) | (s_picnic_on ? 16 : 0) | (s_pez_on ? 32 : 0); } /* (32: la pesca con minijuego, v89) */
static void lib_cerrar(void);          /* (v38) ui_musica.inc */
static void lib_toque(int k);
static void animar_libre(void);
static bool s_sim_libre;
static void sim_fin(void)
{
    if (!s_sim_on) return;
    const bool libre = s_sim_libre;
    lib_cerrar();
    s_sim_on = false;
    /* v88: primero se sueltan las imágenes grandes del juego (fondo de la cueva y el dragón) y recién después vuelve el mundo:
     * así el fondo del mundo tiene memoria para armarse de una vez y no se redibuja a tirones mientras todavía se ve la cueva */
    lv_image_set_src(s_sim_fondo, NULL);
    s_sim_drag_fase = -1;
    lv_image_set_src(s_sim_drag, NULL);
    for (int i = 0; i < 3; i++) art_img_free(&s_sim_drag_art[i]);
    art_img_free(&s_sim_cueva);
    art_cristales_fondo565_liberar();
    if (s_pet) { s_fondo_periodo = -1; refrescar_fondo(s_pet, s_ultimo_t); } /* el fondo del mundo se vuelve a pedir (si se soltó por memoria, se rehace antes de mostrarlo) */
    lv_obj_remove_flag(s_fondo, LV_OBJ_FLAG_HIDDEN); lv_obj_remove_flag(s_mundo, LV_OBJ_FLAG_HIDDEN); /* el mundo vuelve a pintarse */
    ocultar(s_sim, true);
    lv_obj_invalidate(lv_screen_active()); /* un solo redibujo completo */
    if (!libre) accion(UI_ACCION_CRISTALES, s_sim_ganadas); /* (el modo libre avisa cada pieza por su cuenta) */
}
static void sim_cris_cb(lv_event_t *e)
{
    int k = (int)(intptr_t)lv_event_get_user_data(e);
    if (s_sim_on && s_sim_libre) { lib_toque(k); return; } /* (v38) el modo libre */
    if (k >= SIM_CRIS) { if (s_sim_on) sim_encender(k); return; } /* (v39) los de las orillas suenan, pero no son del juego de memoria */
    if (!s_sim_on || s_sim_fase != SIM_TURNO) return;
    sim_encender(k);
    if (k == s_sim_seq[s_sim_paso]) {
        if (++s_sim_paso >= s_sim_largo) { /* ¡bien! la canción con las luces */
            s_sim_ganadas = s_sim_ronda + 1;
            s_sim_fase = SIM_CANCION; s_sim_t0 = s_ms + 500; s_sim_paso = 0;
            sim_texto(s_sim_ronda == 2 ? "¡LO LOGRASTE! ESCUCHA..." : "¡MUY BIEN! LA CANCIÓN DE LOS CRISTALES");
        }
    } else { /* no pasa nada: se vuelve a mostrar la misma */
        s_sim_fase = SIM_FALLO; s_sim_t0 = s_ms;
        sim_texto("¡CASI! MIRA OTRA VEZ");
    }
}
static void sim_x_cb(lv_event_t *e) { (void)e; sim_fin(); }
static void sim_fondo_cb(lv_event_t *e) { (void)e; if (s_sim_fase == SIM_FIN && !s_sim_libre) sim_fin(); }
static void cristales_empezar(void)
{
    cerrar_panel();
    if (!s_sim) {
        s_sim = caja(lv_layer_top());
        lv_obj_set_size(s_sim, W, W);
        lv_obj_set_style_bg_color(s_sim, lv_color_hex(0x1A1430), 0);
        lv_obj_set_style_bg_opa(s_sim, LV_OPA_COVER, 0);
        lv_obj_add_flag(s_sim, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(s_sim, sim_fondo_cb, LV_EVENT_CLICKED, NULL);
        s_sim_fondo = lv_image_create(s_sim); /* la cueva pintada */
        lv_obj_remove_flag(s_sim_fondo, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_pos(s_sim_fondo, 0, 0);
        s_sim_luz = caja(s_sim); /* el resplandor del fondo, donde duerme el dragón */
        lv_obj_set_size(s_sim_luz, 190, 190);
        lv_obj_set_style_radius(s_sim_luz, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(s_sim_luz, lv_color_hex(0xB48AF0), 0);
        lv_obj_set_style_bg_opa(s_sim_luz, 0, 0);
        /* (sin sombras difusas: en la placa cada una cuesta cientos de ms por cuadro) */
        lv_obj_align(s_sim_luz, LV_ALIGN_TOP_MID, 0, 10);
        s_sim_drag = lv_image_create(s_sim);
        lv_obj_remove_flag(s_sim_drag, LV_OBJ_FLAG_CLICKABLE);
        for (int k = 0; k < SIM_CRIS; k++) {
            const int lado0 = 12 * SIM_POS[k][2];
            s_sim_brillo[k] = caja(s_sim); /* la luz de cada cristal cuando suena (redonda, detrás de él) */
            lv_obj_set_size(s_sim_brillo[k], lado0 + 30, lado0 + 30);
            lv_obj_set_style_radius(s_sim_brillo[k], LV_RADIUS_CIRCLE, 0);
            lv_obj_set_style_bg_color(s_sim_brillo[k], lv_color_hex(SIM_COLOR[k]), 0);
            lv_obj_set_style_bg_opa(s_sim_brillo[k], 0, 0);
                lv_obj_set_pos(s_sim_brillo[k], SIM_POS[k][0] - (lado0 + 30) / 2, SIM_POS[k][1] - lado0 * 11 / 20 - (lado0 + 30) / 2);
            lv_obj_remove_flag(s_sim_brillo[k], LV_OBJ_FLAG_CLICKABLE);
            s_sim_cris[k] = lv_image_create(s_sim);
            lv_image_set_src(s_sim_cris[k], art_icono((art_icono_t)(ICO_CRIS_0 + k), SIM_POS[k][2]));
            lv_obj_set_style_image_recolor(s_sim_cris[k], lv_color_hex(0x0C0820), 0);
            lv_obj_add_flag(s_sim_cris[k], LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_ext_click_area(s_sim_cris[k], 10);
            lv_obj_add_event_cb(s_sim_cris[k], sim_cris_cb, LV_EVENT_PRESSED, (void *)(intptr_t)k);
            int lado = 12 * SIM_POS[k][2];
            lv_obj_set_pos(s_sim_cris[k], SIM_POS[k][0] - lado / 2, SIM_POS[k][1] - lado);
        }
        for (int b = 0; b < 2; b++) { /* (v39) los dos cristales de las orillas: su luz (se suma al dibujo del fondo) y una zona para tocarlos */
            const int k = SIM_CRIS + b;
            s_sim_brillo[k] = caja(s_sim);
            lv_obj_set_size(s_sim_brillo[k], 70, 70);
            lv_obj_set_style_radius(s_sim_brillo[k], LV_RADIUS_CIRCLE, 0);
            lv_obj_set_style_bg_color(s_sim_brillo[k], lv_color_hex(SIM_COLOR[k]), 0);
            lv_obj_set_style_bg_opa(s_sim_brillo[k], 0, 0);
            lv_obj_set_style_blend_mode(s_sim_brillo[k], LV_BLEND_MODE_ADDITIVE, 0);
            lv_obj_set_pos(s_sim_brillo[k], SIM_POS[k][0] - 35, SIM_POS[k][1] - 62);
            lv_obj_remove_flag(s_sim_brillo[k], LV_OBJ_FLAG_CLICKABLE);
            s_sim_borde[b] = caja(s_sim);
            lv_obj_set_size(s_sim_borde[b], 56, 72);
            lv_obj_set_pos(s_sim_borde[b], SIM_POS[k][0] - 28, SIM_POS[k][1] - 66);
            lv_obj_set_style_bg_opa(s_sim_borde[b], 0, 0);
            lv_obj_add_flag(s_sim_borde[b], LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_event_cb(s_sim_borde[b], sim_cris_cb, LV_EVENT_PRESSED, (void *)(intptr_t)k);
        }
        s_sim_t = lv_image_create(s_sim);
        s_sim_x = boton(s_sim, BOTON_ABAJO, BOTON_ABAJO, sim_x_cb, 0);
        lv_obj_set_style_radius(s_sim_x, LV_RADIUS_CIRCLE, 0);
        lv_obj_align(s_sim_x, LV_ALIGN_TOP_MID, 0, BOTON_ABAJO_Y);
        lv_obj_t *xi = icono(s_sim_x, ICO_NO, 2); lv_obj_center(xi);
    }
    s_sim_on = true;
    lv_obj_add_flag(s_fondo, LV_OBJ_FLAG_HIDDEN); lv_obj_add_flag(s_mundo, LV_OBJ_FLAG_HIDDEN); /* tapado por el juego: no lo pintes debajo (era lo que lo hacía lento) */
    { const lv_image_dsc_t *cv = art_cristales_fondo565(); if (cv) poner_src(s_sim_fondo, cv); }
    s_sim_ronda = 0; s_sim_ganadas = 0;
    ocultar(s_sim, false);
    ocultar(s_sim_drag, true);
    lv_obj_move_foreground(s_sim);
    lv_obj_move_foreground(s_sim_x);
    lv_obj_set_ext_click_area(s_sim_x, 20);
    s_sim_inicio = s_ms;
    sim_nueva_ronda();
}
static void animar_cristales_juego(void)
{
    if (!s_sim_on) return;
    if (s_ms - s_sim_inicio > 240000u) { sim_fin(); return; } /* por si algo se pega: sale solo */
    const int32_t t = (int32_t)(s_ms - s_sim_t0);
    if (s_sim_libre) animar_libre(); /* (v38) sin rondas */
    else switch (s_sim_fase) {
    case SIM_MUESTRA: /* se encienden uno por uno, con su nota */
        if (t >= 0 && t / 700 >= s_sim_paso && s_sim_paso < s_sim_largo) { sim_encender(s_sim_seq[s_sim_paso]); s_sim_paso++; }
        if (s_sim_paso >= s_sim_largo && t > s_sim_largo * 700 + 200) { s_sim_fase = SIM_TURNO; s_sim_paso = 0; sim_texto("¡AHORA TÚ!"); }
        break;
    case SIM_FALLO: /* tiemblan un poquito y se vuelve a mostrar la misma */
        if (t > 1400) { s_sim_fase = SIM_MUESTRA; s_sim_t0 = s_ms + 300; s_sim_paso = 0; sim_texto("MIRA Y ESCUCHA..."); }
        break;
    case SIM_CANCION: { /* la canción: las luces al ritmo */
        int pulso = t < 0 ? -1 : t / 320;
        if (pulso >= 0 && pulso < 16 && pulso >= s_sim_paso) { if (SIM_TEMAS[s_sim_ronda][pulso] >= 0) sim_encender(SIM_TEMAS[s_sim_ronda][pulso]); s_sim_paso = pulso + 1; }
        if (t > 16 * 320 + 600) {
            if (s_sim_ronda < 2) { s_sim_ronda++; sim_nueva_ronda(); }
            else if (dragon_ausente()) { /* el dragón no está: el juego termina sin él */
                s_sim_fase = SIM_FIN; s_sim_t0 = s_ms; s_sim_ganadas = 2;
                sim_texto("¿Y EL DRAGÓN? HOY NO ESTÁ... ¡QUÉ RARO!");
            }
            else { /* ¡el dragón despierta! */
                s_sim_fase = SIM_DRAGON; s_sim_t0 = s_ms; s_sim_drag_fase = -1;
                ocultar(s_sim_drag, false);
                sim_texto(s_pet && pet_special_known(s_pet, ESP_CRISTALES) ? "¡EL DRAGÓN TE SALUDA!" : "ALGO DESPIERTA AL FONDO DE LA CUEVA...");
            }
        }
        break;
    }
    case SIM_DRAGON: { /* dormido, despierta, abre las alas y le regala su corazón de cristal */
        int f = t < 1800 ? 0 : (t < 3200 ? 1 : 2);
        if (f != s_sim_drag_fase) {
            s_sim_drag_fase = f;
            if (!s_sim_drag_art[f].dsc.data) art_dragon_pintado(&s_sim_drag_art[f], f, 2);
            poner_src(s_sim_drag, &s_sim_drag_art[f].dsc);
            lv_obj_align(s_sim_drag, LV_ALIGN_TOP_MID, 0, 10);
            if (f == 2) { for (int k = 0; k < 6; k++) s_sim_luz_k[k] = s_ms + (uint32_t)k * 120; }
        }
        lv_obj_set_style_translate_y(s_sim_drag, (int32_t)(sinf((float)s_ms / 600.0f) * (f ? 4 : 1)), 0);
        if (t > 3600 && t < 3700) sim_texto("¡TE REGALA SU CORAZÓN! BRILLAS DE NOCHE");
        if (t > 4200 && (t / 400) % 2 == 0 && t % 400 < TICK_MS) s_sim_luz_k[azar() % SIM_CRIS] = s_ms;
        if (t > 7500) { s_sim_fase = SIM_FIN; sim_texto(s_nino ? NULL : "TOCA PARA VOLVER"); }
        break;
    }
    default: break;
    }
    /* cómo se ven: apagados (oscuros) o encendidos (su color, más grandes y con luz) */
    for (int k = 0; k < SIM_CRIS; k++) {
        int32_t d = (int32_t)(s_ms - s_sim_luz_k[k]);
        float b = d >= 0 && d < 450 ? 1.0f - (float)d / 450.0f : 0;
        b = (float)(int)(b * 5.0f + 0.5f) / 5.0f; /* 5 pasos: cada cambio repinta */
        lv_obj_set_style_image_recolor_opa(s_sim_cris[k], (lv_opa_t)(120 - b * 120), 0);
        lv_obj_set_style_bg_opa(s_sim_brillo[k], (lv_opa_t)(b * 120), 0);
        int32_t tx = s_sim_fase == SIM_FALLO && (s_ms / 60) % 2 ? 2 : 0;
        lv_obj_set_style_translate_x(s_sim_cris[k], tx, 0);
    }
    for (int k = SIM_CRIS; k < CRIS_TOT; k++) { /* (v39) las orillas: solo su luz */
        int32_t d = (int32_t)(s_ms - s_sim_luz_k[k]);
        float b = d >= 0 && d < 450 ? 1.0f - (float)d / 450.0f : 0;
        b = (float)(int)(b * 5.0f + 0.5f) / 5.0f;
        lv_obj_set_style_bg_opa(s_sim_brillo[k], (lv_opa_t)(b * 150), 0);
    }
    if (!s_sim_libre) lv_obj_set_style_bg_opa(s_sim_luz, s_sim_fase == SIM_DRAGON ? 70 : 24, 0); /* fija: solo cambia al despertar el dragón */
}
/* para probar: saltar al dragón */
void ui_demo_dragon_ausente(int v) { s_drag_forzar = v; }
void ui_demo_dragon(void) { if (!s_sim_on) cristales_empezar(); s_sim_ronda = 2; s_sim_ganadas = 3; s_sim_fase = SIM_CANCION; s_sim_paso = 16; s_sim_t0 = s_ms - 16 * 320 - 700; s_sim_drag_fase = -1; } /* salta al final del juego: sale (o no) el dragón */

/* ======================================================================
 * Las fiestas con todos (versión 24): en el prado se juntan sus amigos del barrio, las criaturas que
 * conocieron y algún animal amigo. Navidad (decorar el árbol y regalos), Año Nuevo (fuegos artificiales),
 * Halloween (disfraces y dulces), Fiestas Patrias (solo en Chile: banderines y volantines con los colores de
 * la bandera, siempre en el mismo orden, y empanadas) y su cumpleaños (pastel y regalos)
 * ==================================================================== */
#define FIE_AMIGOS 3
#define FIE_COSAS 14
#define FIE_FUEGOS 6
static int s_fie_n, s_fie_toques, s_fie_cri_n;
static uint32_t s_fie_t0, s_fie_dur;
static EXT_RAM_BSS_ATTR criatura_t s_fie_amigo[FIE_AMIGOS];
static bool s_fie_creados;
static lv_obj_t *s_fie_capa, *s_fie_velo, *s_fie_cosa[FIE_COSAS], *s_fie_gorro[FIE_AMIGOS + 1], *s_fie_cri[3], *s_fie_fuego[FIE_FUEGOS];
static uint32_t s_fie_fuego_t[FIE_FUEGOS];
static int s_fie_cri_c[3];
static float s_fie_fx[FIE_FUEGOS], s_fie_fy[FIE_FUEGOS];
static const int16_t FIE_LUGAR[FIE_AMIGOS][2] = {{CX - 104, 292}, {CX + 104, 292}, {CX + 54, 246}};
static const int16_t FIE_ADORNO[6][2] = {{-14, 70}, {12, 60}, {-6, 46}, {16, 84}, {-18, 92}, {4, 30}}; /* sobre el árbol (desde su base) */

static void fiesta_cosa(int i, art_icono_t ico, int esc, int x, int y)
{
    lv_image_set_src(s_fie_cosa[i], art_icono(ico, esc));
    const lv_image_dsc_t *d = lv_image_get_src(s_fie_cosa[i]);
    int w = d ? (int)d->header.w : 12 * esc, h = d ? (int)d->header.h : 12 * esc;
    lv_obj_set_pos(s_fie_cosa[i], x - w / 2, y - h);
    lv_obj_set_style_image_recolor_opa(s_fie_cosa[i], 0, 0);
    lv_obj_set_style_opa(s_fie_cosa[i], LV_OPA_COVER, 0);
    ocultar(s_fie_cosa[i], false);
}
static void fiesta_fuego(int x, int y)
{
    int k = 0;
    for (int i = 1; i < FIE_FUEGOS; i++) if (s_fie_fuego_t[i] < s_fie_fuego_t[k]) k = i;
    s_fie_fuego_t[k] = s_ms;
    s_fie_fx[k] = (float)x; s_fie_fy[k] = (float)y;
    accion(UI_ACCION_SONAR, SND_FIESTA);
}
static void fiesta_toque_cb(lv_event_t *e)
{
    (void)e;
    lv_point_t pt = {0, 0};
    lv_indev_t *in = lv_indev_active();
    if (in) lv_indev_get_point(in, &pt);
    s_fie_toques++;
    switch (s_fie_tipo) {
    case FECHA_NAVIDAD: /* cada toque pone un adorno en el árbol; el último, la estrella */
        if (s_fie_n < 6) {
            static const uint32_t COL[6] = {0xF0403E, 0xFFD45C, 0x5AA8FF, 0xFF8FC0, 0xF4F4FF, 0xFFD45C};
            int i = 1 + s_fie_n;
            fiesta_cosa(i, s_fie_n == 5 ? ICO_ESTRELLA : ICO_PELOTA, s_fie_n == 5 ? 3 : 1, CX + FIE_ADORNO[s_fie_n][0], 250 - FIE_ADORNO[s_fie_n][1]);
            if (s_fie_n < 5) { lv_obj_set_style_image_recolor(s_fie_cosa[i], lv_color_hex(COL[s_fie_n]), 0); lv_obj_set_style_image_recolor_opa(s_fie_cosa[i], LV_OPA_80, 0); }
            accion(UI_ACCION_SONAR, s_fie_n == 5 ? SND_HADAS : SND_TOQUE);
            flotar(ICO_ESTRELLA, NULL, 0, CX + FIE_ADORNO[s_fie_n][0], 230 - FIE_ADORNO[s_fie_n][1], 0);
            s_fie_n++;
        }
        break;
    case FECHA_ANIO_NUEVO: if (pt.y < 240) fiesta_fuego((int)pt.x, (int)pt.y); break;
    case FECHA_HALLOWEEN: flotar(ICO_FANTASMA, NULL, 0, (int)pt.x, (int)pt.y - 10, 0); accion(UI_ACCION_SONAR, SND_RISA); break;
    case FECHA_FIESTAS_PATRIAS: /* toca una empanada y ella se la come */
        for (int i = 9; i < 12; i++)
            if (!lv_obj_has_flag(s_fie_cosa[i], LV_OBJ_FLAG_HIDDEN) && abs((int)pt.x - (lv_obj_get_x(s_fie_cosa[i]) + 18)) < 30 && abs((int)pt.y - (lv_obj_get_y(s_fie_cosa[i]) + 18)) < 30) {
                ocultar(s_fie_cosa[i], true);
                lv_image_set_src(s_objeto, art_icono(ICO_EMPANADA, 3));
                ocultar(s_objeto, false);
                mascota_modo(M_COME, 1500);
                accion(UI_ACCION_SONAR, SND_COMER);
                break;
            }
        break;
    default: flotar(ICO_CORAZON, NULL, 0, (int)pt.x, (int)pt.y - 10, 0); break;
    }
}
static void fiesta_crear(void)
{
    if (s_fie_creados) return;
    s_fie_creados = true;
    s_fie_velo = caja(s_mundo);
    lv_obj_set_size(s_fie_velo, W, W);
    lv_obj_remove_flag(s_fie_velo, LV_OBJ_FLAG_CLICKABLE);
    for (int i = 0; i < FIE_COSAS; i++) { s_fie_cosa[i] = lv_image_create(s_mundo); ocultar(s_fie_cosa[i], true); }
    for (int i = 0; i < FIE_AMIGOS; i++) { crear_criatura(&s_fie_amigo[i]); mostrar_criatura(&s_fie_amigo[i], false); }
    for (int i = 0; i < 3; i++) { s_fie_cri[i] = lv_image_create(s_mundo); ocultar(s_fie_cri[i], true); }
    for (int i = 0; i <= FIE_AMIGOS; i++) { s_fie_gorro[i] = lv_image_create(s_mundo); ocultar(s_fie_gorro[i], true); }
    for (int i = 0; i < FIE_FUEGOS; i++) { s_fie_fuego[i] = lv_image_create(s_mundo); lv_image_set_src(s_fie_fuego[i], art_icono(ICO_FUEGOS, 4)); ocultar(s_fie_fuego[i], true); }
    s_fie_capa = caja(s_mundo);
    lv_obj_set_size(s_fie_capa, W, W);
    lv_obj_add_flag(s_fie_capa, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_fie_capa, fiesta_toque_cb, LV_EVENT_PRESSED, NULL);
    ocultar(s_fie_capa, true);
    ocultar(s_fie_velo, true);
}
static void fiesta_terminar(void)
{
    if (s_fie_tipo < 0) return;
    accion(UI_ACCION_FIESTA, s_fie_tipo);
    s_fie_tipo = -1;
    for (int i = 0; i < FIE_COSAS; i++) ocultar(s_fie_cosa[i], true);
    for (int i = 0; i < FIE_AMIGOS; i++) mostrar_criatura(&s_fie_amigo[i], false);
    for (int i = 0; i < 3; i++) ocultar(s_fie_cri[i], true);
    for (int i = 0; i <= FIE_AMIGOS; i++) ocultar(s_fie_gorro[i], true);
    for (int i = 0; i < FIE_FUEGOS; i++) ocultar(s_fie_fuego[i], true);
    ocultar(s_fie_capa, true);
    ocultar(s_fie_velo, true);
    for (int k = 0; k < 5; k++) flotar(ICO_CORAZON, NULL, 0, (int)s_mascota.x - 40 + k * 20, cabeza_y(&s_mascota) - 10, (uint32_t)k * 120);
    if (!s_nino) globo(ICO_CORAZON, "¡QUÉ LINDA FIESTA! GRACIAS POR VENIR", 3000);
}
static void fiesta_empezar(int tipo)
{
    const pet_t *p = s_pet;
    if (!p) return;
    fiesta_crear();
    cerrar_panel();
    s_fie_tipo = tipo;
    s_fie_t0 = s_ms;
    ocultar(s_objeto, true);
    for (int i = 0; i < 4; i++) ocultar(s_dock[i], true); /* la fiesta ocupa toda la pantalla */
    s_fie_n = s_fie_toques = 0;
    s_fie_dur = tipo == FECHA_NAVIDAD ? 40000 : 32000;
    memset(s_fie_fuego_t, 0, sizeof s_fie_fuego_t);
    /* los invitados: sus amigos del barrio (hasta tres) y las criaturas que conocieron (hasta tres) */
    int am[FIE_AMIGOS];
    int n = pet_party_guests(p, am, FIE_AMIGOS);
    for (int i = 0; i < FIE_AMIGOS; i++) {
        criatura_t *v = &s_fie_amigo[i];
        if (i >= n) { mostrar_criatura(v, false); continue; }
        v->pose.especie = (uint16_t)pet_friend_look(am[i]);
        v->pose.etapa = p->etapa < STAGE_BEBE ? STAGE_BEBE : p->etapa;
        v->pose.forma = 0;
        v->pose.escala = 3;
        v->pose.expr = EXP_FELIZ;
        v->tx = FIE_LUGAR[i][0]; v->ty = FIE_LUGAR[i][1];
        v->x = i % 2 ? W + 30 : -30; v->y = v->ty;
        v->clave = 0xFFFFFFFFu;
        mostrar_criatura(v, true);
    }
    s_fie_cri_n = 0;
    for (int c = 0; c < CRI_COUNT && s_fie_cri_n < 3; c++) if ((p->criaturas_vistas >> c) & 1u) s_fie_cri_c[s_fie_cri_n++] = c;
    for (int i = 0; i < 3; i++) {
        ocultar(s_fie_cri[i], i >= s_fie_cri_n);
        if (i < s_fie_cri_n) lv_image_set_src(s_fie_cri[i], art_icono(ICO_CRIATURA[s_fie_cri_c[i]], 3));
    }
    s_mascota.tx = CX; s_mascota.ty = 296;
    if (tipo != FIESTA_FLORES) mascota_modo(M_VA, 3000);
    /* la decoración de cada fiesta */
    for (int i = 0; i < FIE_COSAS; i++) ocultar(s_fie_cosa[i], true);
    ocultar(s_fie_velo, true);
    const char *saludo = "¡VINIERON TODOS A CELEBRAR!";
    switch (tipo) {
    case FECHA_NAVIDAD:
        fiesta_cosa(0, ICO_ARBOL_NAVIDAD, 8, CX, 250);
        fiesta_cosa(7, ICO_GUIRNALDA, 5, CX - 90, 150); fiesta_cosa(8, ICO_GUIRNALDA, 5, CX + 90, 150);
        saludo = "¡FELIZ NAVIDAD! TOCA EL ÁRBOL PARA DECORARLO";
        break;
    case FECHA_ANIO_NUEVO:
        lv_obj_set_style_bg_color(s_fie_velo, lv_color_hex(0x0A1030), 0);
        lv_obj_set_style_bg_opa(s_fie_velo, LV_OPA_50, 0);
        ocultar(s_fie_velo, false);
        saludo = "¡ADIÓS, AÑO VIEJO! TOCA EL CIELO PARA MÁS FUEGOS";
        break;
    case FECHA_HALLOWEEN:
        lv_obj_set_style_bg_color(s_fie_velo, lv_color_hex(0x3A1A5A), 0);
        lv_obj_set_style_bg_opa(s_fie_velo, LV_OPA_30, 0);
        ocultar(s_fie_velo, false);
        fiesta_cosa(0, ICO_CALABAZA, 4, 92, 330); fiesta_cosa(1, ICO_CALABAZA, 3, 374, 320); fiesta_cosa(2, ICO_CALABAZA, 3, 150, 220);
        saludo = "¡DULCE O TRUCO! TODOS VINIERON DISFRAZADOS";
        break;
    case FECHA_FIESTAS_PATRIAS: /* banderines rojo, blanco y azul, siempre en ese orden */
        for (int i = 0; i < 5; i++) fiesta_cosa(i, ICO_BANDERINES, 4, 89 + i * 72, 150 + (i == 0 || i == 4 ? 16 : (i == 2 ? -6 : 4)));
        fiesta_cosa(5, ICO_VOLANTIN, 4, 120, 100); fiesta_cosa(6, ICO_VOLANTIN, 3, 330, 80);
        fiesta_cosa(8, ICO_MANTEL, 6, CX, 360);
        for (int i = 0; i < 3; i++) fiesta_cosa(9 + i, ICO_EMPANADA, 3, CX - 40 + i * 40, 344);
        saludo = "¡FELICES FIESTAS PATRIAS! ¿UNA EMPANADITA?";
        break;
    case FIESTA_FLORES: /* el Día de las Flores Amarillas: ella te trae un ramo; sus amigos traen más */
        for (int i = 0; i < 6; i++) { /* flores amarillas por todo el prado */
            static const int16_t P[6][2] = {{80, 300}, {140, 340}, {330, 340}, {390, 296}, {110, 220}, {360, 216}};
            fiesta_cosa(i, ICO_FLOR, 3, P[i][0], P[i][1]);
            lv_obj_set_style_image_recolor(s_fie_cosa[i], lv_color_hex(0xFFE23A), 0);
            lv_obj_set_style_image_recolor_opa(s_fie_cosa[i], LV_OPA_80, 0);
        }
        lv_image_set_src(s_objeto, art_icono(ICO_RAMO_AMARILLO, 3));
        ocultar(s_objeto, false);
        mascota_modo(M_TRAE, 4000);
        saludo = "¡FELIZ DÍA DE LAS FLORES AMARILLAS! TE TRAJE UN RAMO";
        break;
    default: /* su cumpleaños */
        fiesta_cosa(8, ICO_MANTEL, 6, CX, 360);
        fiesta_cosa(0, ICO_PASTEL, 4, CX, 346);
        accion(UI_ACCION_SONAR, SND_CUMPLEANOS);
        saludo = "¡VINIERON A CELEBRAR MI CUMPLEAÑOS!";
        break;
    }
    ocultar(s_fie_capa, false);
    lv_obj_move_foreground(s_fie_capa);
    if (!s_nino) globo(tipo == FIESTA_SU_CUMPLE ? ICO_PASTEL : (tipo == FIESTA_FLORES ? ICO_RAMO_AMARILLO : ICO_AMIGOS), saludo, 4000);
    accion(UI_ACCION_SONAR, SND_FIESTA);
}
static void animar_fiesta(void)
{
    if (s_fie_tipo < 0) return;
    const pet_t *p = s_pet;
    uint32_t t = s_ms - s_fie_t0;
    /* (v101) traer al frente las criaturas, gorros, fuegos y la capa de toques SOLO una vez por segundo: la capa es del tamaño de
     * toda la pantalla y moverla en cada cuadro obligaba a redibujar la pantalla entera en cada cuadro (13-nov, su cumpleaños:
     * 480 kpx y 124 ms por cuadro, unos 6 cuadros por segundo) */
    static uint32_t orden_ms;
    const bool reordenar = s_ms - orden_ms >= 1000u;
    if (reordenar) orden_ms = s_ms;
    if (!p || p->dormida || s_escena != ESC_PRADO || s_interior) { fiesta_terminar(); return; }
    /* los invitados llegan, saltan de alegría y miran la fiesta */
    for (int i = 0; i < FIE_AMIGOS; i++) {
        criatura_t *v = &s_fie_amigo[i];
        if (!v->visible) continue;
        bool camina = mover(v, 3.5f);
        art_pose_t *ps = &v->pose;
        ps->expr = EXP_FELIZ; ps->cola = 1; ps->orejas = 1; ps->espejo = !camina ? s_mascota.x < v->x : v->tx < v->x;
        ps->boca = !camina && (s_ms / 300 + (uint32_t)i) % 4 == 0;
        ps->frame = (int)((s_ms / (camina ? 200 : 700)) % 2);
        float sy = !camina && (s_fie_tipo == FECHA_ANIO_NUEVO || (s_ms / 900 + (uint32_t)i) % 5 == 0) ? -fabsf(sinf((float)s_ms / 140.0f + (float)i)) * 10 : 0;
        dibujar(v);
        colocar(v, 0, sy);
    }
    for (int i = 0; i < s_fie_cri_n; i++) { /* las criaturas flotan arriba, alrededor */
        static const int16_t POS[3][2] = {{CX - 150, 210}, {CX + 150, 200}, {CX - 60, 168}};
        lv_obj_set_pos(s_fie_cri[i], POS[i][0] - 18, POS[i][1] - 36 + (int)(sinf((float)s_ms / 500.0f + (float)i) * 5));
        if (reordenar) lv_obj_move_foreground(s_fie_cri[i]);
    }
    /* cada fiesta lo suyo */
    switch (s_fie_tipo) {
    case FECHA_NAVIDAD:
        for (int i = 7; i <= 8; i++) lv_obj_set_style_opa(s_fie_cosa[i], (lv_opa_t)(200 + 55 * sinf((float)s_ms / 250.0f + (float)i)), 0);
        if (t > 22000 && s_fie_n < 6) { /* si no lo terminas, te ayudan */
            if (t % 700 < TICK_MS) { int k = s_fie_n; fiesta_cosa(1 + k, k == 5 ? ICO_ESTRELLA : ICO_PELOTA, k == 5 ? 3 : 1, CX + FIE_ADORNO[k][0], 250 - FIE_ADORNO[k][1]); s_fie_n++; }
        }
        if (s_fie_n >= 6 && s_fie_toques >= 0) { /* el árbol listo: se intercambian regalos */
            s_fie_toques = -1;
            s_fie_dur = t + 9000;
            for (int i = 0; i < FIE_AMIGOS; i++) if (s_fie_amigo[i].visible) {
                flotar(ICO_REGALO, NULL, 0, (int)s_fie_amigo[i].x, cabeza_y(&s_fie_amigo[i]) - 10, (uint32_t)i * 500);
                flotar(ICO_REGALO, NULL, 0, (int)s_mascota.x, cabeza_y(&s_mascota) - 10, 300 + (uint32_t)i * 500);
            }
            if (!s_nino) globo(ICO_REGALO, "¡REGALOS PARA TODOS!", 3000);
            accion(UI_ACCION_SONAR, SND_TESORO);
            mascota_modo(M_SALTA, 2500);
        }
        break;
    case FECHA_ANIO_NUEVO: /* cuenta regresiva y fuegos artificiales */
        if (t > 3000 && t < 6000 && t % 1000 < TICK_MS) { static const art_icono_t N[3] = {ICO_ESTRELLA, ICO_ESTRELLA, ICO_ESTRELLA}; flotar(N[0], NULL, 0, CX, 120, 0); accion(UI_ACCION_SONAR, SND_CONTAR); }
        if (t > 6000 && t % 600 < TICK_MS) fiesta_fuego(80 + (int)(azar() % 300), 60 + (int)(azar() % 150));
        if (t > 6000 && t < 6100) { mascota_modo(M_SALTA, 4000); if (!s_nino) globo(ICO_FUEGOS, "¡FELIZ AÑO NUEVO!", 3000); }
        break;
    case FECHA_HALLOWEEN: /* sombreros de bruja y fantasmitas; las calabazas titilan */
        for (int i = 0; i < 3; i++) { lv_obj_set_style_image_recolor(s_fie_cosa[i], lv_color_hex(0xFFE27A), 0); lv_obj_set_style_image_recolor_opa(s_fie_cosa[i], (lv_opa_t)(40 + 40 * sinf((float)s_ms / 300.0f + (float)i)), 0); }
        if (t % 2500 < TICK_MS) { int i = (int)(azar() % FIE_AMIGOS); if (s_fie_amigo[i].visible) flotar(ICO_GALLETA, NULL, 0, (int)s_fie_amigo[i].x, cabeza_y(&s_fie_amigo[i]) - 10, 0); }
        break;
    case FECHA_FIESTAS_PATRIAS: /* los volantines se mecen en el viento */
        lv_obj_set_style_translate_x(s_fie_cosa[5], (int32_t)(sinf((float)s_ms / 700.0f) * 14), 0);
        lv_obj_set_style_translate_y(s_fie_cosa[5], (int32_t)(cosf((float)s_ms / 900.0f) * 6), 0);
        lv_obj_set_style_translate_x(s_fie_cosa[6], (int32_t)(sinf((float)s_ms / 800.0f + 1) * 12), 0);
        break;
    case FIESTA_FLORES: /* cada amigo trae una florcita amarilla */
        if (t > 4500 && t < 4600) for (int i = 0; i < FIE_AMIGOS; i++) if (s_fie_amigo[i].visible) flotar(ICO_RAMO_AMARILLO, NULL, 0, (int)s_fie_amigo[i].x, cabeza_y(&s_fie_amigo[i]) - 10, (uint32_t)i * 600);
        for (int i = 0; i < 6; i++) lv_obj_set_style_translate_y(s_fie_cosa[i], (int32_t)(sinf((float)s_ms / 500.0f + (float)i) * 2), 0);
        break;
    default: /* su cumpleaños: confeti y regalos */
        if (t % 400 < TICK_MS) flotar(ICO_ESTRELLA, NULL, 0, 60 + (int)(azar() % 340), 120 + (int)(azar() % 60), 0);
        if (t > 6000 && t < 6100) for (int i = 0; i < FIE_AMIGOS; i++) if (s_fie_amigo[i].visible) flotar(ICO_REGALO, NULL, 0, (int)s_mascota.x, cabeza_y(&s_mascota) - 10, (uint32_t)i * 500);
        break;
    }
    /* los gorros de bruja (o fantasmitas) en Halloween */
    for (int i = 0; i <= FIE_AMIGOS; i++) {
        const criatura_t *c = i < FIE_AMIGOS ? &s_fie_amigo[i] : &s_mascota;
        bool ver = s_fie_tipo == FECHA_HALLOWEEN && c->visible;
        ocultar(s_fie_gorro[i], !ver);
        if (!ver) continue;
        lv_image_set_src(s_fie_gorro[i], art_icono(i % 2 ? ICO_FANTASMA : ICO_GORRO_BRUJA, 2));
        lv_obj_set_pos(s_fie_gorro[i], (int)c->x - 12 + (i % 2 ? 22 : 0), cabeza_y(c) - (i % 2 ? 20 : 26));
        if (reordenar) lv_obj_move_foreground(s_fie_gorro[i]);
    }
    /* los fuegos artificiales: crecen y se apagan */
    for (int i = 0; i < FIE_FUEGOS; i++) {
        uint32_t e = s_ms - s_fie_fuego_t[i];
        bool ver = s_fie_fuego_t[i] && e < 1400;
        ocultar(s_fie_fuego[i], !ver);
        if (!ver) continue;
        float k = (float)e / 1400.0f;
        lv_image_set_scale(s_fie_fuego[i], (uint32_t)(120 + 260 * k));
        lv_obj_set_pos(s_fie_fuego[i], (int)s_fie_fx[i] - 24, (int)s_fie_fy[i] - 24);
        lv_obj_set_style_opa(s_fie_fuego[i], (lv_opa_t)(255 * (1.0f - k * k)), 0);
        if (reordenar) lv_obj_move_foreground(s_fie_fuego[i]);
    }
    if (reordenar) lv_obj_move_foreground(s_fie_capa);
    if (t > s_fie_dur) fiesta_terminar();
}
/* ¿toca una fiesta? en el prado, con la pantalla libre (una vez en el día de la fiesta) */
static void fiesta_revisar(const pet_t *p, uint32_t utc)
{
    if (s_fie_tipo >= 0 || s_men_fase || !pantalla_libre(p) || s_escena != ESC_PRADO || s_interior || s_mascota.modo != M_LIBRE) return;
    int f = pet_party_now(p, utc);
    if (f >= 0 && f != FECHA_ANIO_NUEVO) fiesta_empezar(f); /* Año Nuevo: ui_ny.inc */
}
/* para probar en el simulador */
void ui_demo_fiesta(int tipo) { fiesta_empezar(tipo); }
