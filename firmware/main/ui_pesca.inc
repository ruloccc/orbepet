/* ======================================================================
 * Pesca con minijuego (v89): cuando TÚ la mandas a pescar se abre una pantalla propia. Ella en el bote,
 * en medio del lago (el fondo del cruce, según la estación y la hora), con su caña, el sedal y la boya.
 * De pronto "¡PICÓ!" y hay que hacer lo que pide (al azar): sacudir el orbe, soplar o tocar la pantalla.
 * No siempre pica, a veces se escapa, y lo que sale depende de la estación y la hora (lo decide el motor).
 * Si ella va a pescar sola (por su cuenta), sigue la pesca automática de siempre, sin interrumpir.
 * ==================================================================== */
enum { PZ_LANZA = 0, PZ_ESPERA, PZ_PICO, PZ_SACA, PZ_RESULTADO, PZ_ESCAPA, PZ_NADA };
enum { PZ_SACUDIR = 0, PZ_SOPLAR, PZ_TOCAR };
static lv_obj_t *s_pez, *s_pez_fondo, *s_pez_bote, *s_pez_cri, *s_pez_cana, *s_pez_sedal, *s_pez_boya, *s_pez_onda[3];
static lv_obj_t *s_pez_ico, *s_pez_c, *s_pez_t, *s_pez_x, *s_pez_item;
static art_img_t s_pez_fondo_art, s_pez_bote_art, s_pez_cri_art[2], s_pez_t_art;
static lv_point_precise_t s_pez_cana_pts[2], s_pez_sedal_pts[2];
static int s_pez_fase = -1, s_pez_accion, s_pez_fc = -1, s_pez_texto = -1;
static uint32_t s_pez_t0, s_pez_pica, s_pez_ventana, s_pez_amago, s_pez_onda_t;
static bool s_pez_va_a_picar;
/* v90: lo que se ve cuando soplas o sacudes (para saber que el orbe te oyó), y la pantalla de celebración al sacar algo */
static lv_obj_t *s_pez_aire, *s_pez_raya[4], *s_pez_cel, *s_pez_cel_luz, *s_pez_cel_ico, *s_pez_cel_c, *s_pez_cel_t, *s_pez_cel_n, *s_pez_conf[16];
static art_img_t s_pez_cel_luz_art, s_pez_cel_t_art, s_pez_cel_n_art;
static uint32_t s_pez_aire_t, s_pez_meneo_t, s_pez_cel_t0;
static float s_pez_conf_x[16], s_pez_conf_y[16], s_pez_conf_v[16];
static bool s_pez_mic_ok = true;
static int s_pez_toques, s_pez_toques_meta; /* v90: tocar = tocar la pantalla VARIAS veces rápido */
static lv_obj_t *s_pez_punto[8], *s_pez_viento[3]; /* la brisa que cruza la pantalla mientras soplas (como en la niebla y el bote) */
static int s_pez_res = -1, s_pez_nivel;
static int s_pez_especie = -1, s_pez_nueva; /* (1.0.32) cuál animal de la colección salió y si es nuevo */
static char s_pez_nom[64]; /* (lo fuerte que suena el micrófono ahora, 0..100) */
void ui_mic_nivel(int nivel) { s_pez_nivel = nivel < 0 ? 0 : nivel; }
void ui_set_mic_ok(bool ok) { s_pez_mic_ok = ok; }
#define PZ_BOTE_X 120 /* dónde flota el bote (esquina de arriba a la izquierda de su imagen) */
#define PZ_ESC 4
/* (v38) en invierno (o con el lago helado): no hay bote; ella en un banquito junto a un hoyo en el hielo, y caen copitos */
static bool s_pez_hielo;
static lv_obj_t *s_pez_hoyo, *s_pez_hoyo_agua, *s_pez_banco, *s_pez_copo[6];
#define PZ_HOYO_X 312
#define PZ_HOYO_Y (ART_CRUCE_AGUA_Y + 84)
#define PZ_BANCO_X 160
#define PZ_BANCO_Y (ART_CRUCE_AGUA_Y + 100)

static void pez_texto(const char *t, int clave)
{
    if (clave == s_pez_texto) return;
    s_pez_texto = clave;
    if (!t || (s_nino && clave < 100)) { ocultar(s_pez_c, true); return; } /* los más chicos: solo los dibujos */
    art_texto(&s_pez_t_art, t, 0xFFFFFF, 2, 0);
    cartel_poner(s_pez_c, s_pez_t, &s_pez_t_art);
    lv_obj_align(s_pez_c, LV_ALIGN_TOP_MID, 0, 74);
    ocultar(s_pez_c, false);
}

static void pez_cerrar(void)
{
    if (!s_pez_on) return;
    s_pez_on = false;
    s_pez_fase = -1;
    ocultar(s_pez, true);
    if (s_pez_cel) ocultar(s_pez_cel, true);
    lv_image_set_src(s_pez_fondo, NULL);
    lv_image_set_src(s_pez_cri, NULL);
    lv_image_set_src(s_pez_bote, NULL);
    if (s_pez_item) lv_image_set_src(s_pez_item, NULL);
    if (s_pez_cel_ico) lv_image_set_src(s_pez_cel_ico, NULL);
    art_peces_soltar(); /* (1.0.32) */
    art_img_free(&s_pez_fondo_art); /* el paisaje pesa (~430 KB): se suelta al salir */
    for (int f = 0; f < 2; f++) art_img_free(&s_pez_cri_art[f]);
    art_img_free(&s_pez_bote_art);
    if (s_pez_cel_luz) lv_image_set_src(s_pez_cel_luz, NULL);
    art_img_free(&s_pez_cel_luz_art); /* (1.0.55) la luz de la celebración (225 KB): el informe #8 la mostró guardada siempre */
}

/* lo que pidió hacer ya se hizo (sacudir, soplar o tocar) */
static void pez_hecho(int que)
{
    if (!s_pez_on || s_pez_fase != PZ_PICO || que != s_pez_accion) return;
    if (que == PZ_SOPLAR) s_pez_aire_t = s_ms;   /* el soplido se ve: una nube de aire y rayitas hacia la boya */
    if (que == PZ_SACUDIR) s_pez_meneo_t = s_ms; /* la sacudida se ve: todo se mece */
    if (azar() % 100 < 20) { /* a veces se suelta igual: así es la pesca */
        s_pez_fase = PZ_ESCAPA; s_pez_t0 = s_ms;
        accion(UI_ACCION_PESCA, 2);
        sonido(SND_CAIDA);
        return;
    }
    s_pez_fase = PZ_SACA; s_pez_t0 = s_ms;
    sonido(SND_SALPICON);
    accion(UI_ACCION_PESCA, 3); /* el motor decide qué sale y responde con EV_PESCA */
}

static void pez_toque_cb(lv_event_t *e)
{
    (void)e;
    if (!s_pez_on || s_pez_fase != PZ_PICO || s_pez_accion != PZ_TOCAR) return;
    s_pez_toques++;
    s_pez_meneo_t = s_ms; /* cada toque se nota: el bote da un tironcito */
    sonido(SND_TOQUE);
    if (s_pez_toques >= s_pez_toques_meta) pez_hecho(PZ_TOCAR);
}
static void pez_x_cb(lv_event_t *e)
{
    (void)e;
    if (s_pez_fase <= PZ_PICO) accion(UI_ACCION_PESCA, 1); /* lo dejó antes de tiempo: no sacó nada */
    pez_cerrar();
}

static void crear_pesca(void)
{
    s_pez = caja(lv_layer_top());
    lv_obj_set_size(s_pez, W, W);
    lv_obj_set_style_bg_color(s_pez, lv_color_hex(0x7FC4E8), 0);
    lv_obj_set_style_bg_opa(s_pez, LV_OPA_COVER, 0);
    lv_obj_add_flag(s_pez, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_pez, pez_toque_cb, LV_EVENT_PRESSED, NULL);
    s_pez_fondo = lv_image_create(s_pez);
    lv_obj_remove_flag(s_pez_fondo, LV_OBJ_FLAG_CLICKABLE);
    for (int i = 0; i < 3; i++) { /* las ondas en el agua cuando pica */
        s_pez_onda[i] = caja(s_pez);
        lv_obj_set_style_radius(s_pez_onda[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(s_pez_onda[i], LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(s_pez_onda[i], 2, 0);
        lv_obj_set_style_border_color(s_pez_onda[i], lv_color_hex(0xE8F8FF), 0);
        lv_obj_remove_flag(s_pez_onda[i], LV_OBJ_FLAG_CLICKABLE);
        ocultar(s_pez_onda[i], true);
    }
    s_pez_cri = lv_image_create(s_pez);
    lv_obj_remove_flag(s_pez_cri, LV_OBJ_FLAG_CLICKABLE);
    s_pez_bote = lv_image_create(s_pez);
    lv_obj_remove_flag(s_pez_bote, LV_OBJ_FLAG_CLICKABLE);
    s_pez_hoyo = caja(s_pez); /* (v38) el hoyo en el hielo: borde de escarcha y agua oscura */
    lv_obj_set_size(s_pez_hoyo, 96, 34);
    lv_obj_set_style_radius(s_pez_hoyo, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(s_pez_hoyo, lv_color_hex(0xF4FAFF), 0);
    lv_obj_set_style_bg_opa(s_pez_hoyo, LV_OPA_COVER, 0);
    lv_obj_remove_flag(s_pez_hoyo, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_pos(s_pez_hoyo, PZ_HOYO_X - 48, PZ_HOYO_Y - 17);
    s_pez_hoyo_agua = caja(s_pez_hoyo);
    lv_obj_set_size(s_pez_hoyo_agua, 80, 24);
    lv_obj_set_style_radius(s_pez_hoyo_agua, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(s_pez_hoyo_agua, lv_color_hex(0x1C3E62), 0);
    lv_obj_set_style_bg_grad_color(s_pez_hoyo_agua, lv_color_hex(0x2E6A9A), 0);
    lv_obj_set_style_bg_grad_dir(s_pez_hoyo_agua, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_bg_opa(s_pez_hoyo_agua, LV_OPA_COVER, 0);
    lv_obj_remove_flag(s_pez_hoyo_agua, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_center(s_pez_hoyo_agua);
    s_pez_banco = caja(s_pez); /* el banquito */
    lv_obj_set_size(s_pez_banco, 76, 14);
    lv_obj_set_style_radius(s_pez_banco, 5, 0);
    lv_obj_set_style_bg_color(s_pez_banco, lv_color_hex(0xA0703C), 0);
    lv_obj_set_style_bg_opa(s_pez_banco, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_pez_banco, 2, 0);
    lv_obj_set_style_border_color(s_pez_banco, lv_color_hex(0x6A4424), 0);
    lv_obj_remove_flag(s_pez_banco, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_pos(s_pez_banco, PZ_BANCO_X - 38, PZ_BANCO_Y - 4);
    for (int i = 0; i < 6; i++) {
        s_pez_copo[i] = lv_image_create(s_pez);
        lv_obj_remove_flag(s_pez_copo[i], LV_OBJ_FLAG_CLICKABLE);
        lv_image_set_src(s_pez_copo[i], art_icono(ICO_COPO, 2));
        ocultar(s_pez_copo[i], true);
    }
    ocultar(s_pez_hoyo, true); ocultar(s_pez_banco, true);
    s_pez_cana = lv_line_create(s_pez);
    lv_obj_set_style_line_width(s_pez_cana, 5, 0);
    lv_obj_set_style_line_color(s_pez_cana, lv_color_hex(0x7A4A2A), 0);
    lv_obj_set_style_line_rounded(s_pez_cana, true, 0);
    lv_obj_remove_flag(s_pez_cana, LV_OBJ_FLAG_CLICKABLE);
    s_pez_sedal = lv_line_create(s_pez);
    lv_obj_set_style_line_width(s_pez_sedal, 1, 0);
    lv_obj_set_style_line_color(s_pez_sedal, lv_color_hex(0xF4F4F4), 0);
    lv_obj_remove_flag(s_pez_sedal, LV_OBJ_FLAG_CLICKABLE);
    s_pez_boya = caja(s_pez); /* la boya: roja arriba, blanca abajo */
    lv_obj_set_size(s_pez_boya, 14, 14);
    lv_obj_set_style_radius(s_pez_boya, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(s_pez_boya, lv_color_hex(0xF0403A), 0);
    lv_obj_set_style_bg_grad_color(s_pez_boya, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_grad_dir(s_pez_boya, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_bg_main_stop(s_pez_boya, 120, 0);
    lv_obj_set_style_bg_grad_stop(s_pez_boya, 136, 0);
    lv_obj_set_style_bg_opa(s_pez_boya, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_pez_boya, 1, 0);
    lv_obj_set_style_border_color(s_pez_boya, lv_color_hex(0x3A2020), 0);
    lv_obj_remove_flag(s_pez_boya, LV_OBJ_FLAG_CLICKABLE);
    s_pez_item = lv_image_create(s_pez); /* lo que sacó, que sube desde el agua */
    lv_obj_remove_flag(s_pez_item, LV_OBJ_FLAG_CLICKABLE);
    ocultar(s_pez_item, true);
    s_pez_ico = lv_image_create(s_pez); /* qué hay que hacer: sacudir, soplar o tocar */
    lv_obj_remove_flag(s_pez_ico, LV_OBJ_FLAG_CLICKABLE);
    ocultar(s_pez_ico, true);
    for (int i = 0; i < 8; i++) { /* cuántos toques van (se llenan) */
        s_pez_punto[i] = caja(s_pez);
        lv_obj_set_size(s_pez_punto[i], 14, 14);
        lv_obj_set_style_radius(s_pez_punto[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(s_pez_punto[i], 2, 0);
        lv_obj_set_style_border_color(s_pez_punto[i], lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_bg_color(s_pez_punto[i], lv_color_hex(0xFFE27A), 0);
        lv_obj_set_style_bg_opa(s_pez_punto[i], 0, 0);
        lv_obj_remove_flag(s_pez_punto[i], LV_OBJ_FLAG_CLICKABLE);
        ocultar(s_pez_punto[i], true);
    }
    s_pez_aire = lv_image_create(s_pez); /* el soplido */
    lv_obj_remove_flag(s_pez_aire, LV_OBJ_FLAG_CLICKABLE);
    lv_image_set_src(s_pez_aire, art_icono(ICO_SOPLA, 3));
    ocultar(s_pez_aire, true);
    for (int i = 0; i < 3; i++) {
        s_pez_viento[i] = lv_image_create(s_pez);
        lv_obj_remove_flag(s_pez_viento[i], LV_OBJ_FLAG_CLICKABLE);
        lv_image_set_src(s_pez_viento[i], art_icono(ICO_VIENTO, 3));
        ocultar(s_pez_viento[i], true);
    }
    for (int i = 0; i < 4; i++) {
        s_pez_raya[i] = caja(s_pez);
        lv_obj_set_size(s_pez_raya[i], 30 - i * 4, 4);
        lv_obj_set_style_radius(s_pez_raya[i], 2, 0);
        lv_obj_set_style_bg_color(s_pez_raya[i], lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_bg_opa(s_pez_raya[i], LV_OPA_COVER, 0);
        lv_obj_remove_flag(s_pez_raya[i], LV_OBJ_FLAG_CLICKABLE);
        ocultar(s_pez_raya[i], true);
    }
    s_pez_c = cartel_de(s_pez, &s_pez_t, 0x2A2238, LV_OPA_70);
    lv_obj_remove_flag(s_pez_c, LV_OBJ_FLAG_CLICKABLE);
    s_pez_x = boton(s_pez, BOTON_ABAJO, BOTON_ABAJO, pez_x_cb, 0);
    lv_obj_set_style_radius(s_pez_x, LV_RADIUS_CIRCLE, 0);
    lv_obj_align(s_pez_x, LV_ALIGN_TOP_MID, 0, BOTON_ABAJO_Y);
    lv_obj_t *xi = icono(s_pez_x, ICO_NO, 2); lv_obj_center(xi);
    ocultar(s_pez, true);
}

/* v90: la celebración: una pantalla aparte con lo que consiguió, grande, con su luz, confeti y "¡CONSEGUISTE...!" */
static void pez_cel_cb(lv_event_t *e) { (void)e; if (s_ms - s_pez_cel_t0 > 900) pez_cerrar(); }
static void crear_celebracion(void)
{
    s_pez_cel = caja(s_pez);
    lv_obj_set_size(s_pez_cel, W, W);
    lv_obj_set_style_bg_color(s_pez_cel, lv_color_hex(0x1A1440), 0);
    lv_obj_set_style_bg_grad_color(s_pez_cel, lv_color_hex(0x3A2A70), 0);
    lv_obj_set_style_bg_grad_dir(s_pez_cel, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_bg_opa(s_pez_cel, LV_OPA_COVER, 0);
    lv_obj_add_flag(s_pez_cel, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_pez_cel, pez_cel_cb, LV_EVENT_CLICKED, NULL);
    s_pez_cel_luz = lv_image_create(s_pez_cel);
    lv_obj_remove_flag(s_pez_cel_luz, LV_OBJ_FLAG_CLICKABLE);
    /* (1.0.55) la luz (225 KB) se hace al mostrar la celebración y se suelta al cerrar la pesca */
    lv_obj_set_pos(s_pez_cel_luz, CX - 120, 205 - 120);
    s_pez_cel_ico = lv_image_create(s_pez_cel);
    lv_obj_remove_flag(s_pez_cel_ico, LV_OBJ_FLAG_CLICKABLE);
    s_pez_cel_c = lv_image_create(s_pez_cel); /* "¡CONSEGUISTE!" */
    lv_obj_remove_flag(s_pez_cel_c, LV_OBJ_FLAG_CLICKABLE);
    s_pez_cel_n = lv_image_create(s_pez_cel); /* lo que es */
    lv_obj_remove_flag(s_pez_cel_n, LV_OBJ_FLAG_CLICKABLE);
    s_pez_cel_t = lv_image_create(s_pez_cel); /* "TOCA PARA SEGUIR" */
    lv_obj_remove_flag(s_pez_cel_t, LV_OBJ_FLAG_CLICKABLE);
    static const uint32_t CC[5] = {0xFF6BD0, 0x5ED0FF, 0xFFE27A, 0x7AF07A, 0xB48CFF};
    for (int i = 0; i < 16; i++) {
        s_pez_conf[i] = caja(s_pez_cel);
        lv_obj_set_size(s_pez_conf[i], 6 + i % 3 * 2, 8);
        lv_obj_set_style_radius(s_pez_conf[i], 2, 0);
        lv_obj_set_style_bg_color(s_pez_conf[i], lv_color_hex(CC[i % 5]), 0);
        lv_obj_set_style_bg_opa(s_pez_conf[i], LV_OPA_COVER, 0);
        lv_obj_remove_flag(s_pez_conf[i], LV_OBJ_FLAG_CLICKABLE);
    }
    ocultar(s_pez_cel, true);
}
static void pez_celebrar(art_icono_t ico, const char *nombre)
{
    if (!s_pez_cel) crear_celebracion();
    const lv_image_dsc_t *im = s_pez_especie >= 0 ? art_pez(s_pez_especie, 9, false) : NULL; /* (1.0.32) el animal de la colección */
    lv_image_set_src(s_pez_cel_ico, im ? im : art_icono(ico, 9));
    lv_obj_set_pos(s_pez_cel_ico, CX - 54, 205 - 54);
    art_texto(&s_pez_cel_t_art, s_pez_especie >= 0 && s_pez_nueva ? "¡NUEVO EN TU COLECCIÓN!" : "¡CONSEGUISTE!", 0xFFE27A, 3, 380);
    poner_src(s_pez_cel_c, &s_pez_cel_t_art.dsc);
    lv_obj_align(s_pez_cel_c, LV_ALIGN_TOP_MID, 0, 56);
    art_texto(&s_pez_cel_n_art, nombre, 0xFFFFFF, 2, 340);
    poner_src(s_pez_cel_n, &s_pez_cel_n_art.dsc);
    lv_obj_align(s_pez_cel_n, LV_ALIGN_TOP_MID, 0, 300);
    lv_image_set_src(s_pez_cel_t, art_icono(ICO_MANO, 2));
    lv_obj_align(s_pez_cel_t, LV_ALIGN_TOP_MID, 0, 352);
    for (int i = 0; i < 16; i++) { s_pez_conf_x[i] = (float)(60 + azar() % 346); s_pez_conf_y[i] = -(float)(azar() % 200); s_pez_conf_v[i] = 2.0f + (float)(azar() % 30) / 10.0f; }
    { const lv_image_dsc_t *lz = luz_suave(&s_pez_cel_luz_art, 0xFFE27A, 240, 420.0f); if (lz) lv_image_set_src(s_pez_cel_luz, lz); } /* (1.0.55) */
    ocultar(s_pez_cel, false);
    lv_obj_move_foreground(s_pez_cel);
    s_pez_cel_t0 = s_ms;
    sonido(SND_FIESTA);
}

/* se abre la pantalla de pesca (el motor ya la tiene pescando: ACT_PESCAR respondió que sí) */
static void pez_empezar(const pet_t *m)
{
    if (!s_pez) crear_pesca();
    cerrar_panel();
    s_pez_on = true;
    s_pez_fase = PZ_LANZA;
    s_pez_t0 = s_ms;
    s_pez_fc = -1;
    s_pez_texto = -1;
    s_pez_amago = 0;
    s_pez_onda_t = 0;
    /* ¿pica? casi siempre; a veces no en toda la espera. Cuándo: entre 5 y 25 s (los más chicos esperan menos) */
    s_pez_va_a_picar = azar() % 100 < 85;
    const uint32_t larga = s_nino ? 12000u : 25000u;
    s_pez_pica = s_ms + 1400u + (s_pez_va_a_picar ? 5000u + azar() % (larga - 5000u) : larga);
    s_pez_ventana = s_suave && s_nino ? 10000u : (s_nino ? 6000u : 4000u); /* (v90: un poco más de tiempo para reaccionar) */
    s_pez_accion = s_suave && s_nino ? PZ_TOCAR : (int)(azar() % 3); /* (infante: siempre tocar) */
    s_pez_toques = 0;
    s_pez_toques_meta = s_suave && s_nino ? 3 : (s_nino ? 4 : 6); /* tocar rápido: 6 toques (niño 4, infante 3) */
    if (s_pez_accion == PZ_SOPLAR && !s_pez_mic_ok) s_pez_accion = azar() % 2 ? PZ_SACUDIR : PZ_TOCAR; /* el micrófono no pudo abrirse: otra cosa */
#ifdef ESP_PLATFORM
    { extern void hal_log(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
      hal_log("pesca: empieza (va a picar %d, pide %s, ventana %u ms)", s_pez_va_a_picar ? 1 : 0, s_pez_accion == PZ_SACUDIR ? "sacudir" : s_pez_accion == PZ_SOPLAR ? "soplar" : "tocar", (unsigned)s_pez_ventana); }
#endif
    s_pez_hielo = lago_congelado(m);
    art_cruce_estacion(s_pez_hielo ? 3 : (int)pet_season(m, s_ultimo_t)); /* (3: el lago helado) */
    hacer_lugar("la pesca", 460u); /* (1.0.51) ahora pide un solo bloque de ~428 KB (los índices van dentro) */ /* (1.0.31) el lago pide ~428 KB + 212 KB seguidos: sin ellos quedaba todo celeste (foto del 19-nov) */
    art_cruce_fondo(&s_pez_fondo_art, (int)periodo_de(m, s_ultimo_t));
    if (!s_pez_fondo_art.px && hacer_lugar("la pesca (2.º intento)", 0u)) art_cruce_fondo(&s_pez_fondo_art, (int)periodo_de(m, s_ultimo_t));
    ocultar(s_pez_bote, s_pez_hielo); ocultar(s_pez_hoyo, !s_pez_hielo); ocultar(s_pez_banco, !s_pez_hielo);
    for (int i = 0; i < 6; i++) ocultar(s_pez_copo[i], !s_pez_hielo);
    if (s_pez_fondo_art.px) { poner_src(s_pez_fondo, &s_pez_fondo_art.dsc); lv_obj_set_pos(s_pez_fondo, -1, -1); }
    art_bote_vela(&s_pez_bote_art, 0, PZ_ESC);
    poner_src(s_pez_bote, &s_pez_bote_art.dsc);
    for (int f = 0; f < 2; f++) {
        art_pose_t po = {pet_look(m), m->etapa, m->etapa == STAGE_ADULTA ? m->forma : 0, PZ_ESC, f ? EXP_FELIZ : EXP_CURIOSO, f == 1, f, false, false, 1, 0, 0, {0, 0, 0, 0}, 0, m->guardiana};
        for (int r = 0; r < RANURA_COUNT; r++) po.ropa[r] = pet_worn_raw(m, r);
        art_criatura_pose(&s_pez_cri_art[f], &po);
    }
    ocultar(s_pez_item, true);
    ocultar(s_pez_ico, true);
    for (int i = 0; i < 3; i++) ocultar(s_pez_onda[i], true);
    ocultar(s_pez, false);
    lv_obj_move_foreground(s_pez);
    pez_texto(s_pez_hielo ? "¡A PESCAR EN EL HIELO! ESPERA QUE PIQUE..." : "¡A PESCAR! ESPERA QUE PIQUE...", 0);
    s_pez_res = -1;
    if (s_pez_cel) ocultar(s_pez_cel, true);
    accion(UI_ACCION_PESCA, 0); /* el motor no termina la pesca solo mientras juegan */
    animar_pesca_juego(); /* todo en su lugar desde el primer cuadro (el bote no aparece un instante arriba a la izquierda) */
}

bool ui_captura_sacudida(void) { return s_pez_on; } /* (v90: pescando, ninguna sacudida va a molestarla) */
void ui_sacudida(void) { pez_hecho(PZ_SACUDIR); }

/* el motor dijo qué salió del agua */
static bool pez_resultado(int que, int comio)
{
    if (!s_pez_on) return false;
    static const art_icono_t I[PESCA_COUNT] = {ICO_PUNTOS, ICO_PEZ, ICO_BOTA, ICO_PIEDRA, ICO_CONCHA, ICO_ALGA, ICO_CANGREJO, ICO_NENUFAR,
                                               ICO_OBJETO(OBJ_BOTELLA), ICO_OBJETO(OBJ_LLAVE_OXIDADA), ICO_PEZ_DORADO, ICO_HIELO};
    static const char *const T[PESCA_COUNT] = {"NADA ESTA VEZ", "¡UN PEZ!", "¡UNA BOTA VIEJA! JA, JA", "¡UNA PIEDRA BRILLANTE!",
                                               "¡UNA CONCHA PARA LA COLECCIÓN!", "¡UN ALGA! ¡QUÉ COSQUILLAS!", "¡UN CANGREJO! ¡AY, LA NARIZ!",
                                               "¡UNA HOJA DE NENÚFAR! MIRA MI SOMBRERO", "¡UNA BOTELLA CON UN MENSAJE!", "UNA LLAVE OXIDADA... ¿QUÉ ABRIRÁ?",
                                               "¡UN PEZ DORADO! BRILLA... LO DEVUELVO", "¡UN TROCITO DE HIELO! ¡QUÉ FRÍO!"};
    const int a = que >= 0 && que < PESCA_COUNT ? que : 0;
    const int esp = ((comio >> 1) & 31) - 1; /* (1.0.32) EV_PESCA b: bit0 se lo comió, bits 1-5 = 1 + cuál, bit6 nuevo */
    s_pez_especie = esp >= 0 && esp < PET_PECES ? esp : -1;
    s_pez_nueva = (comio >> 6) & 1;
    comio &= 1;
    if (s_pez_especie >= 0) { snprintf(s_pez_nom, sizeof s_pez_nom, "¡%s!", pet_fish_species_with_article(s_pez_especie)); mayusculas(s_pez_nom); }
    if (s_pez_fase != PZ_SACA) return true;
    s_pez_res = a; /* (no pescó: ya está el mensaje de que se escapó o no picó) */
    s_pez_fase = PZ_RESULTADO;
    s_pez_t0 = s_ms;
    { const lv_image_dsc_t *im = s_pez_especie >= 0 ? art_pez(s_pez_especie, 5, false) : NULL; lv_image_set_src(s_pez_item, im ? im : art_icono(I[a], 5)); }
    ocultar(s_pez_item, a == PESCA_NADA);
    if (s_pez_especie >= 0) pez_texto(s_pez_nom, 300 + s_pez_especie);
    else pez_texto(a == PESCA_PEZ ? (comio ? "¡UN PEZ! ÑAM" : "¡UN PEZ! LO DEVUELVO AL AGUA") : T[a], 200 + a);
#ifdef ESP_PLATFORM
    { extern void hal_log(const char *fmt, ...) __attribute__((format(printf, 1, 2))); hal_log("pesca: sacó %s", pet_catch_name((pesca_t)a)); }
#endif
    if (a != PESCA_NADA) { /* sube del agua un momento y luego la celebración en su propia pantalla */
        static const char *const N[PESCA_COUNT] = {"", "UN PEZ", "UNA BOTA VIEJA", "UNA PIEDRA BRILLANTE", "UNA CONCHA PARA LA COLECCIÓN", "UN ALGA",
                                                   "UN CANGREJO", "UNA HOJA DE NENÚFAR", "UNA BOTELLA CON UN MENSAJE", "UNA LLAVE OXIDADA", "¡UN PEZ DORADO!",
                                                   "UN TROCITO DE HIELO PARA LA COLECCIÓN"};
        s_pez_cel_t0 = 0;
        (void)N;
    }
    return true;
}

static void animar_pesca_juego(void)
{
    if (!s_pez_on) return;
    const uint32_t t = s_ms - s_pez_t0;
    const int bw = (int)s_pez_bote_art.dsc.header.w, bh = (int)s_pez_bote_art.dsc.header.h;
    const float ola = sinf((float)s_ms / 520.0f);
    const int bx = PZ_BOTE_X, by = ART_CRUCE_AGUA_Y + 80 - bh + (int)(ola * 3);
    lv_obj_set_pos(s_pez_bote, bx, by);
    /* ella, sentada en el bote (o en el banquito del hielo): alegre al sacar algo, curiosa esperando */
    int ax, ay;
    art_bote_vela_asiento(PZ_ESC, &ax, &ay);
    const int fc = s_pez_fase == PZ_RESULTADO || s_pez_fase == PZ_SACA ? 1 : 0;
    if (fc != s_pez_fc) { s_pez_fc = fc; poner_src(s_pez_cri, &s_pez_cri_art[fc].dsc); }
    const int cw = (int)s_pez_cri_art[0].dsc.header.w, ch = (int)s_pez_cri_art[0].dsc.header.h;
    int cx = bx + ax - 10, cy = by + ay - ch + (s_pet && s_pet->etapa == STAGE_ADULTA ? 4 : -8) - ch / 8; /* sentada: el borde del bote le tapa solo las patitas */
    if (s_pez_hielo) { /* (v38) sentada en el banquito, junto al hoyo; los copitos caen despacio */
        cx = PZ_BANCO_X; cy = PZ_BANCO_Y - ch + ch / 5;
        for (int i = 0; i < 6; i++) {
            const int ciclo = (int)((s_ms / 16 + (uint32_t)i * 97u) % 520u);
            lv_obj_set_pos(s_pez_copo[i], 70 + i * 58 + (int)(sinf((float)s_ms / 900.0f + (float)i) * 10), ciclo - 40);
        }
        lv_obj_move_foreground(s_pez_cri);
    }
    lv_obj_set_pos(s_pez_cri, cx - cw / 2, cy);
    (void)bw;
    /* la caña: de sus manos hacia arriba y adelante; tira hacia atrás al sacar */
    const bool tira = s_pez_fase == PZ_SACA || (s_pez_fase == PZ_PICO);
    const float hx = (float)(cx + cw / 4), hy = (float)(cy + ch / 2);
    const float kx = hx + (s_pez_hielo ? (tira ? 50.0f : 70.0f) : (tira ? 60.0f : 90.0f)), ky = hy - (s_pez_hielo ? (tira ? 80.0f : 60.0f) : (tira ? 120.0f : 96.0f)); /* (en el hielo, una caña corta) */
    /* la boya: va al agua al lanzar; se mece; en un amago tiembla; cuando pica se hunde */
    float bxo = 330.0f + sinf((float)s_ms / 900.0f) * 3, byo = (float)ART_CRUCE_AGUA_Y + 60 + ola * 2;
    if (s_pez_hielo) { bxo = (float)PZ_HOYO_X + sinf((float)s_ms / 900.0f) * 2; byo = (float)PZ_HOYO_Y - 2 + ola; } /* la boya, dentro del hoyo */
    if (s_pez_fase == PZ_LANZA) { const float k = t < 900 ? (float)t / 900.0f : 1.0f; bxo = kx + (bxo - kx) * k; byo = ky + (byo - ky) * k - sinf(k * 3.1416f) * 60; }
    if (s_pez_amago && s_ms - s_pez_amago < 380) byo += (float)((s_ms / 60) % 2 ? 3 : -2);
    if (s_pez_fase == PZ_PICO) byo += 10 + (float)((s_ms / 90) % 2) * 3;
    if (s_pez_fase == PZ_SACA) { const float k = t < 1200 ? (float)t / 1200.0f : 1.0f; bxo += (kx - bxo) * k * 0.5f; byo -= k * 90; }
    const bool boya_ver = s_pez_fase <= PZ_SACA;
    ocultar(s_pez_boya, !boya_ver); ocultar(s_pez_sedal, !boya_ver);
    s_pez_cana_pts[0].x = hx; s_pez_cana_pts[0].y = hy; s_pez_cana_pts[1].x = kx; s_pez_cana_pts[1].y = ky;
    lv_line_set_points(s_pez_cana, s_pez_cana_pts, 2);
    if (boya_ver) {
        s_pez_sedal_pts[0].x = kx; s_pez_sedal_pts[0].y = ky; s_pez_sedal_pts[1].x = bxo; s_pez_sedal_pts[1].y = byo;
        lv_line_set_points(s_pez_sedal, s_pez_sedal_pts, 2);
        lv_obj_set_pos(s_pez_boya, (int)bxo - 7, (int)byo - 7);
    }
    /* las ondas: al caer la boya y cuando pica */
    if (s_pez_onda_t) {
        const uint32_t to = s_ms - s_pez_onda_t;
        for (int i = 0; i < 3; i++) {
            const int32_t ti = (int32_t)to - i * 220;
            const bool v = ti >= 0 && ti < 900;
            ocultar(s_pez_onda[i], !v);
            if (!v) continue;
            const int r = 8 + ti / 18;
            lv_obj_set_size(s_pez_onda[i], r * 2, r);
            lv_obj_set_pos(s_pez_onda[i], (int)bxo - r, (int)byo + 4 - r / 2);
            lv_obj_set_style_border_opa(s_pez_onda[i], (lv_opa_t)(220 - ti / 4), 0);
        }
        if (to > 1500) s_pez_onda_t = 0;
    }
    { /* v90: se VE que el orbe te oyó. Soplando: la nubecita de aire sale de su boca y unas rayitas van hacia la boya
       * (empieza a verse apenas suena fuerte, antes de que cuente como soplido). Sacudiendo: el bote y ella se mecen. */
        const bool soplando = s_pez_fase == PZ_PICO && s_pez_accion == PZ_SOPLAR && s_pez_nivel >= 12; /* (se ve apenas soplas, antes de que cuente) */
        const int32_t ta = s_pez_aire_t ? (int32_t)(s_ms - s_pez_aire_t) : 99999;
        const bool aire = soplando || ta < 900;
        ocultar(s_pez_aire, !aire);
        const int mx = cx + cw / 2 + 6, my = cy + ch / 3;
        if (aire) {
            const float k = ta < 900 ? (float)ta / 900.0f : (float)((s_ms / 80) % 6) / 12.0f;
            lv_obj_set_pos(s_pez_aire, mx + (int)((bxo - (float)mx) * k * 0.5f), my - 18 + (int)((byo - (float)my) * k * 0.3f));
            lv_obj_set_style_image_opa(s_pez_aire, ta < 900 ? (lv_opa_t)(255 - ta / 5) : (lv_opa_t)(120 + (s_pez_nivel > 60 ? 135 : s_pez_nivel * 2)), 0);
        }
        for (int i = 0; i < 4; i++) {
            const int32_t ti = (ta < 900 ? ta : (int32_t)(s_ms % 600)) - i * 90;
            const bool ver = aire && ti >= 0 && ti < 600;
            ocultar(s_pez_raya[i], !ver);
            if (!ver) continue;
            const float k = (float)ti / 600.0f;
            lv_obj_set_pos(s_pez_raya[i], mx + 20 + (int)((bxo - (float)mx - 20) * k), my + i * 9 - 10 + (int)((byo - (float)my) * k * 0.5f));
        }
        /* la brisa: tres soplos de viento que cruzan la pantalla de izquierda a derecha mientras suena fuerte (y un rato al lograrlo) */
        for (int i = 0; i < 3; i++) {
            ocultar(s_pez_viento[i], !aire);
            if (!aire) continue;
            const int ciclo = (int)((s_ms + (uint32_t)i * 420u) % 1300u);
            lv_obj_set_pos(s_pez_viento[i], -40 + ciclo * (W + 40) / 1300, 130 + i * 55 + (int)(sinf((float)s_ms / 200.0f + (float)i) * 6));
        }
        const int32_t tm = s_pez_meneo_t ? (int32_t)(s_ms - s_pez_meneo_t) : 99999;
        const int32_t meneo = tm < 700 ? (int32_t)(sinf((float)tm / 35.0f) * 8.0f * (1.0f - (float)tm / 700.0f)) : 0;
        lv_obj_set_style_translate_x(s_pez_bote, meneo, 0);
        if (s_pez_hielo) lv_obj_set_style_translate_x(s_pez_banco, meneo / 2, 0);
        lv_obj_set_style_translate_x(s_pez_cri, meneo, 0);
    }
    if (s_pez_fase != PZ_PICO) for (int i = 0; i < 8; i++) ocultar(s_pez_punto[i], true);
    switch (s_pez_fase) {
    case PZ_LANZA:
        if (t >= 900 && !s_pez_onda_t && t < 1100) { s_pez_onda_t = s_ms; sonido(SND_PLOP); }
        if (t > 1400) { s_pez_fase = PZ_ESPERA; s_pez_t0 = s_ms; }
        break;
    case PZ_ESPERA:
        pez_texto("ESPERA... MIRA LA BOYA", 1);
        if (!s_pez_amago || s_ms - s_pez_amago > 2500u) if (azar() % 100 < 2) s_pez_amago = s_ms; /* amagos: la boya tiembla y nada */
        if ((int32_t)(s_ms - s_pez_pica) >= 0) {
            if (!s_pez_va_a_picar) { s_pez_fase = PZ_NADA; s_pez_t0 = s_ms; accion(UI_ACCION_PESCA, 1); break; }
            s_pez_fase = PZ_PICO; s_pez_t0 = s_ms; s_pez_onda_t = s_ms;
            sonido(SND_CHAPUZON);
            static const art_icono_t ICO[3] = {ICO_SACUDIDA, ICO_SOPLA, ICO_MANO};
            lv_image_set_src(s_pez_ico, art_icono(ICO[s_pez_accion], 6));
            lv_obj_align(s_pez_ico, LV_ALIGN_TOP_MID, 0, 120);
            ocultar(s_pez_ico, false);
            static const char *const TX[3] = {"¡PICÓ! ¡SACUDE EL ORBE!", "¡PICÓ! ¡SOPLA!", "¡PICÓ! ¡TOCA RÁPIDO, MUCHAS VECES!"};
            s_pez_texto = -1;
            pez_texto(TX[s_pez_accion], 10 + s_pez_accion);
        }
        break;
    case PZ_PICO:
        lv_obj_set_style_translate_y(s_pez_ico, (int32_t)((s_ms / 120) % 2 ? -4 : 0), 0); /* el ícono salta: ¡apúrate! */
        if (s_pez_accion == PZ_TOCAR) {
            const int n = s_pez_toques_meta > 8 ? 8 : s_pez_toques_meta, x0 = CX - n * 20 / 2 + 3;
            for (int i = 0; i < 8; i++) {
                ocultar(s_pez_punto[i], i >= n);
                if (i >= n) continue;
                lv_obj_set_pos(s_pez_punto[i], x0 + i * 20, 210);
                lv_obj_set_style_bg_opa(s_pez_punto[i], i < s_pez_toques ? LV_OPA_COVER : 0, 0);
            }
            /* tocar RÁPIDO: si pasan 1,2 s sin tocar, se pierde un toque */
            static uint32_t ult; static int ant;
            if (s_pez_toques != ant) { ant = s_pez_toques; ult = s_ms; }
            else if (s_pez_toques > 0 && s_ms - ult > 1200u) { s_pez_toques--; ant = s_pez_toques; ult = s_ms; }
        }
        if (t > s_pez_ventana) { /* no alcanzó: se escapa */
            ocultar(s_pez_ico, true);
            s_pez_fase = PZ_ESCAPA; s_pez_t0 = s_ms;
            accion(UI_ACCION_PESCA, 2);
        }
        break;
    case PZ_SACA:
        ocultar(s_pez_ico, true);
        pez_texto("¡LO TIENE! ¡TIRA!", 2);
        if (t > 4000) { s_pez_fase = PZ_RESULTADO; s_pez_t0 = s_ms; } /* (por si la respuesta del motor no llega) */
        break;
    case PZ_RESULTADO:
        if (!lv_obj_has_flag(s_pez_item, LV_OBJ_FLAG_HIDDEN)) { /* sube desde el agua y queda flotando */
            const float k = t < 700 ? (float)t / 700.0f : 1.0f;
            if (s_pez_hielo) lv_obj_set_pos(s_pez_item, PZ_HOYO_X - 30, (int)(PZ_HOYO_Y - 30 - k * 110 + sinf((float)s_ms / 300.0f) * 4)); /* sale del hoyo */
            else lv_obj_set_pos(s_pez_item, 330 - 30, (int)(ART_CRUCE_AGUA_Y + 40 - k * 120 + sinf((float)s_ms / 300.0f) * 4));
        }
        if (t > 1300 && !lv_obj_has_flag(s_pez_item, LV_OBJ_FLAG_HIDDEN) && (!s_pez_cel || lv_obj_has_flag(s_pez_cel, LV_OBJ_FLAG_HIDDEN))) {
            static const char *const N[PESCA_COUNT] = {"", "¡UN PEZ!", "¡UNA BOTA VIEJA!", "¡UNA PIEDRA BRILLANTE!", "¡UNA CONCHA PARA LA COLECCIÓN!", "¡UN ALGA!",
                                                       "¡UN CANGREJO!", "¡UNA HOJA DE NENÚFAR!", "¡UNA BOTELLA CON UN MENSAJE!", "¡UNA LLAVE OXIDADA!", "¡UN PEZ DORADO!",
                                                       "¡UN TROCITO DE HIELO!"};
            const int a = s_pez_res >= 0 && s_pez_res < PESCA_COUNT ? s_pez_res : 1;
            static const art_icono_t I2[PESCA_COUNT] = {ICO_PUNTOS, ICO_PEZ, ICO_BOTA, ICO_PIEDRA, ICO_CONCHA, ICO_ALGA, ICO_CANGREJO, ICO_NENUFAR,
                                                        ICO_OBJETO(OBJ_BOTELLA), ICO_OBJETO(OBJ_LLAVE_OXIDADA), ICO_PEZ_DORADO, ICO_HIELO};
            pez_celebrar(I2[a], s_pez_especie >= 0 ? s_pez_nom : N[a]);
        }
        if (s_pez_cel && !lv_obj_has_flag(s_pez_cel, LV_OBJ_FLAG_HIDDEN)) { /* la celebración: confeti, el premio que late; se cierra sola a los 8 s o tocando */
            const uint32_t tc = s_ms - s_pez_cel_t0;
            for (int i = 0; i < 16; i++) {
                s_pez_conf_y[i] += s_pez_conf_v[i] * 2.0f;
                s_pez_conf_x[i] += sinf((float)s_ms / 300.0f + (float)i) * 1.2f;
                if (s_pez_conf_y[i] > W) { s_pez_conf_y[i] = -10; s_pez_conf_x[i] = (float)(60 + azar() % 346); }
                lv_obj_set_pos(s_pez_conf[i], (int)s_pez_conf_x[i], (int)s_pez_conf_y[i]);
            }
            lv_obj_set_style_translate_y(s_pez_cel_ico, (int32_t)(sinf((float)s_ms / 260.0f) * 6.0f), 0);
            lv_obj_set_style_image_opa(s_pez_cel_luz, (lv_opa_t)(190 + 50 * sinf((float)s_ms / 400.0f)), 0);
            if (tc > 8000) pez_cerrar();
        } else if (t > 3200 && lv_obj_has_flag(s_pez_item, LV_OBJ_FLAG_HIDDEN)) pez_cerrar();
        break;
    case PZ_ESCAPA:
        ocultar(s_pez_ico, true);
        pez_texto("¡OH, NO! ¡SE ESCAPÓ!", 3);
        if (t > 2600) pez_cerrar();
        break;
    case PZ_NADA:
        pez_texto("HOY NO PICAN... OTRA VEZ SERÁ", 4);
        if (t > 2600) pez_cerrar();
        break;
    default: break;
    }
}
void ui_demo_pesca(int accion_k) /* (pruebas) abre la pesca y hace que pique enseguida con esa acción */
{
    if (!s_pet) return;
    pez_empezar(s_pet);
    s_pez_va_a_picar = true;
    s_pez_pica = s_ms + 1600;
    if (accion_k >= 0 && accion_k < 3) s_pez_accion = accion_k;
}
static bool pez_quiere_soplo(void) { return s_pez_on && s_pez_fase == PZ_PICO && s_pez_accion == PZ_SOPLAR; }
static void pez_soplo(void) { pez_hecho(PZ_SOPLAR); }
void ui_test_pescar(void) { s_pez_pedido = true; accion(ACT_PESCAR, 0); } /* (pruebas) como tocar el bote y elegir pescar */
int ui_test_pesca_fase(void) { return s_pez_on ? s_pez_fase : -1; }
