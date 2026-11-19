/* ======================================================================
 * Los finales (incluido desde ui.c)
 *  - El final de siempre es el rito de la Guardiana del Mundo (Corazón del Mundo, el último objeto).
 *  - Final secreto "El libro se escribe solo", después del rito, con los 150 objetos ya usados: una última página con su nombre,
 *    sus recuerdos dibujados, los objetos que se elevan como luciérnagas y una carta según cómo es ella.
 *  - La Biblioteca: el libro mágico, ya dorado, y desde ahí se puede volver a ver esa última página.
 * ==================================================================== */
static lv_obj_t *pagina_vieja(lv_obj_t *padre, int y, int ancho, int alto, const char *escrito); /* (v99) más abajo */
static int s_fin_cual, s_fin_pag;
#define FIN_COLOR_ORO 0xFFD76A

/* una luciérnaga: sube despacio, brilla y se apaga (y vuelve a empezar) */
static void fin_luci_cb(void *var, int32_t v)
{
    lv_obj_t *o = (lv_obj_t *)var;
    const intptr_t ud = (intptr_t)lv_obj_get_user_data(o);
    const int bx = (int)(ud >> 16), base = (int)(ud & 0xFFFF);
    v = v / 50 * 50; /* a saltos: menos repintados (la pantalla es lenta con sombras animadas) */
    const int y = base - (int)(v * 140 / 1000);
    lv_obj_set_pos(o, bx + (int)(sinf((float)v * 0.012f + (float)(base % 7)) * 9.0f), y);
    int op = v < 250 ? v * 255 / 250 : (v > 700 ? (1000 - v) * 255 / 300 : 255);
    op = (op + 21) / 43 * 43; /* 7 niveles */
    lv_obj_set_style_opa(o, (lv_opa_t)(op < 0 ? 0 : op > 255 ? 255 : op), 0);
}
static void fin_luciernagas(lv_obj_t *padre, int n, int y0, int y1)
{
    if (n > 6) n = 6; /* pocas: cada una repinta la pantalla en cada paso y el sonido se cortaba */
    for (int i = 0; i < n; i++) {
        lv_obj_t *o = lv_obj_create(padre);
        lv_obj_remove_style_all(o);
        int d = 7 + (int)(azar() % 4);
        lv_obj_set_size(o, d, d);
        lv_obj_set_style_radius(o, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(o, lv_color_hex(0xFFF3A0), 0);
        lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(o, 2, 0); /* halo barato en vez de sombra difusa */
        lv_obj_set_style_border_color(o, lv_color_hex(0xFFE27A), 0);
        lv_obj_set_style_border_opa(o, LV_OPA_40, 0);
        lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE);
        int y = y0 + (int)(azar() % (uint32_t)(y1 - y0));
        int w = zona_ancho(y) - 24; if (w < 40) w = 40;
        int x = CX - w / 2 + (int)(azar() % (uint32_t)(w > 40 ? w - 20 : 20));
        lv_obj_set_pos(o, x, y);
        lv_obj_set_user_data(o, (void *)(intptr_t)((x << 16) | y));
        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, o);
        lv_anim_set_values(&a, 0, 1000);
        lv_anim_set_time(&a, 3200 + azar() % 2400);
        lv_anim_set_delay(&a, azar() % 3000);
        lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
        lv_anim_set_exec_cb(&a, fin_luci_cb);
        lv_anim_start(&a);
    }
}

/* aparece de a poco */
static void fin_opa_cb(void *var, int32_t v)
{
    v = (v + 16) / 32 * 32; /* fundidos en pocos pasos: cada paso repinta, y en la placa cuesta */
    lv_obj_set_style_opa((lv_obj_t *)var, (lv_opa_t)(v > 255 ? 255 : v), 0);
}
static void fin_aparecer(lv_obj_t *o, uint32_t retraso)
{
    lv_obj_set_style_opa(o, LV_OPA_TRANSP, 0);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, o);
    lv_anim_set_values(&a, 0, 255);
    lv_anim_set_time(&a, 700);
    lv_anim_set_delay(&a, retraso);
    lv_anim_set_exec_cb(&a, fin_opa_cb);
    lv_anim_start(&a);
}


/* el brillo cálido de las dos mitades del Corazón cuando se reconocen: late suave y es tenue (cuida la batería) */
static void fin_brillo(lv_obj_t *padre)
{
    (void)padre; /* sin el destello de pantalla completa que latía: repintar los 466x466 con transparencia cortaba el sonido */
}

/* la carta de cierre: cambia según lo que más la define */
static const char *fin_carta(int cual, const pet_t *p, int parte)
{
    int mejor = TRAIT_TRAVIESA;
    static const int RAS[4] = {TRAIT_TRAVIESA, TRAIT_CURIOSA, TRAIT_ENERGICA, TRAIT_CARINOSA};
    for (int i = 1; i < 4; i++) if (p->rasgo[RAS[i]] > p->rasgo[mejor]) mejor = RAS[i];
    int k = mejor == TRAIT_TRAVIESA ? 0 : mejor == TRAIT_CURIOSA ? 1 : mejor == TRAIT_ENERGICA ? 2 : 3;
    /* la carta va en dos hojas: lo que sintió, y la despedida */
    static const char *const SEC[4][2] = {
        {"EL LIBRO ESTÁ LLENO DE TODOS LOS LÍOS QUE ARMAMOS JUNTOS, Y ME REÍ EN CADA UNO. GRACIAS POR NO REGAÑARME... DEMASIADO.",
         "NO OLVIDES QUE NOS SEGUIREMOS VIENDO. TE QUIERO MUCHO."},
        {"CADA PÁGINA FUE UNA PREGUNTA, Y SIEMPRE ME ACOMPAÑASTE A BUSCAR LA RESPUESTA.",
         "GRACIAS POR QUEDARTE, Y NO OLVIDES QUE NOS SEGUIREMOS VIENDO. TE QUIERO MUCHO."},
        {"NADAMOS, TREPAMOS Y CORRIMOS HASTA LLENAR HASTA LA ÚLTIMA PÁGINA. CONTIGO NUNCA ME CANSO.",
         "NO OLVIDES QUE NOS SEGUIREMOS VIENDO. ¡TE QUIERO MUCHO!"},
        {"EL LIBRO SE LLENÓ DE OBJETOS, PERO LO QUE MÁS GUARDÉ FUERON CADA CARICIA, MIMO Y PREOCUPACIÓN.",
         "GRACIAS POR QUEDARTE, Y NO OLVIDES QUE NOS SEGUIREMOS VIENDO. TE QUIERO MUCHO."},
    };
    (void)cual;
    return SEC[k][parte ? 1 : 0];
}

/* un cielo de noche con estrellas, redondo como la pantalla */
static void fin_fondo(void)
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
    lv_obj_set_style_bg_color(f, lv_color_hex(0x0A0E2A), 0);
    lv_obj_set_style_bg_grad_color(f, lv_color_hex(0x2A2F6A), 0);
    lv_obj_set_style_bg_grad_dir(f, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_bg_opa(f, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(f, 3, 0);
    lv_obj_set_style_border_color(f, lv_color_hex(0xD0B969), 0);
    lv_obj_remove_flag(f, LV_OBJ_FLAG_CLICKABLE);
    for (int i = 0; i < 46; i++) {
        lv_obj_t *e = lv_obj_create(f);
        lv_obj_remove_style_all(e);
        int d = 2 + (int)(azar() % 2);
        lv_obj_set_size(e, d, d);
        int y = 14 + (int)(azar() % 440), w = zona_ancho(y);
        if (w < 24) continue;
        lv_obj_set_pos(e, CX - w / 2 + (int)(azar() % (uint32_t)w), y);
        lv_obj_set_style_bg_color(e, lv_color_hex(0xFFF6D0), 0);
        lv_obj_set_style_bg_opa(e, (lv_opa_t)(90 + azar() % 160), 0);
        lv_obj_remove_flag(e, LV_OBJ_FLAG_CLICKABLE);
    }
}

static uint32_t s_fin_toque_desde = 0xFFFFFFFFu; /* el cierre con toque: no antes de que aparezca todo (0xFFFFFFFF = no activo) */
static void final_cb(lv_event_t *e);
static void final_toque_cb(lv_event_t *e)
{
    if (s_panel != PANEL_FINAL || s_fin_toque_desde == 0xFFFFFFFFu || s_ms < s_fin_toque_desde) return;
    s_fin_toque_desde = 0xFFFFFFFFu;
    lv_obj_remove_event_cb(s_tarjeta, final_toque_cb);
    final_cb(e);
}
static void final_cb(lv_event_t *e)
{
    int q = (int)(intptr_t)lv_event_get_user_data(e);
    if (q >= 0) { panel_final(s_fin_cual, q); return; }
    if (s_fin_libro7) { s_fin_libro7 = false; cerrar_panel(); libro_pedir(7); return; } /* tras el rito: se cierra el Libro */
    /* el último botón: se guarda y se celebra */
    const int cual = s_fin_cual;
    const bool repetida = s_fin_repetida;
    cerrar_panel();
    if (repetida) return;
    accion(UI_ACCION_FINAL, cual);
    accion(UI_ACCION_SONAR, SND_META);
    int hx = (int)s_mascota.x, hy = cabeza_y(&s_mascota);
    confeti(hx, hy - 30, 24);
    mascota_modo(M_SALTA, 2500);
    globo(ICO_ESTAMPITA, "¡ESTAMPITA DORADA!", 4500);
    for (int i = 0; i < 5; i++) flotar(ICO_ESTRELLA, NULL, 0, hx - 60 + i * 30, hy - 10, (uint32_t)i * 200);
}

/* --- movimiento del final: subir, aparecer, rebotar, aros de luz --- */
static void fin_ty_cb(void *var, int32_t y) { lv_obj_set_style_translate_y((lv_obj_t *)var, y, 0); }
static void fin_escala_cb(void *var, int32_t s) { lv_image_set_scale((lv_obj_t *)var, (uint32_t)s); }

/* sube desde abajo mientras aparece */
static void fin_subir(lv_obj_t *o, uint32_t retraso, int dy)
{
    fin_aparecer(o, retraso);
    lv_obj_set_style_translate_y(o, dy, 0);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, o);
    lv_anim_set_values(&a, dy, 0);
    lv_anim_set_time(&a, 900);
    lv_anim_set_delay(&a, retraso);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_set_exec_cb(&a, fin_ty_cb);
    lv_anim_start(&a);
}

/* aparece creciendo desde "desde" (256 = tamaño normal), con un pequeño rebote */
static void fin_pop(lv_obj_t *o, uint32_t retraso, int desde)
{
    lv_obj_update_layout(o);
    lv_image_set_pivot(o, lv_obj_get_width(o) / 2, lv_obj_get_height(o) / 2);
    lv_image_set_scale(o, (uint32_t)desde);
    fin_aparecer(o, retraso);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, o);
    lv_anim_set_values(&a, desde, 256);
    lv_anim_set_time(&a, 800);
    lv_anim_set_delay(&a, retraso);
    lv_anim_set_path_cb(&a, lv_anim_path_overshoot);
    lv_anim_set_exec_cb(&a, fin_escala_cb);
    lv_anim_start(&a);
}

/* se ve un rato y se va: una tarjeta que cede su lugar a la siguiente */
static void fin_visto(lv_obj_t *o, uint32_t ini, uint32_t dur, bool se_va)
{
    fin_subir(o, ini, 26);
    if (!se_va) return;
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, o);
    lv_anim_set_values(&a, 255, 0);
    lv_anim_set_time(&a, 500);
    lv_anim_set_delay(&a, ini + dur);
    lv_anim_set_exec_cb(&a, fin_opa_cb);
    lv_anim_start(&a);
}

/* salta sin parar (ella, contenta) */
static void fin_salta(lv_obj_t *o, uint32_t retraso)
{
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, o);
    lv_anim_set_values(&a, 0, -16);
    lv_anim_set_time(&a, 380);
    lv_anim_set_playback_time(&a, 380);
    lv_anim_set_delay(&a, retraso);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_set_exec_cb(&a, fin_ty_cb);
    lv_anim_start(&a);
}

/* un aro de luz que se abre desde un punto y se apaga */
static void fin_aro_cb(void *var, int32_t v)
{
    lv_obj_t *o = (lv_obj_t *)var;
    const intptr_t ud = (intptr_t)lv_obj_get_user_data(o);
    v = v / 125 * 125; /* 8 pasos */
    const int cx = (int)(ud >> 16), cy = (int)(ud & 0xFFFF), d = 24 + v * 400 / 1000;
    lv_obj_set_size(o, d, d);
    lv_obj_set_pos(o, cx - d / 2, cy - d / 2);
    lv_obj_set_style_border_opa(o, (lv_opa_t)(170 * (1000 - v) / 1000), 0);
}
static void fin_aro(lv_obj_t *padre, int cx, int cy, uint32_t retraso, bool siempre)
{
    lv_obj_t *o = lv_obj_create(padre);
    lv_obj_remove_style_all(o);
    lv_obj_set_style_radius(o, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(o, 3, 0);
    lv_obj_set_style_border_color(o, lv_color_hex(0xFFE9A8), 0);
    lv_obj_set_style_border_opa(o, LV_OPA_TRANSP, 0);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_user_data(o, (void *)(intptr_t)((cx << 16) | cy));
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, o);
    lv_anim_set_values(&a, 0, 1000);
    lv_anim_set_time(&a, 2600);
    lv_anim_set_delay(&a, retraso);
    if (siempre) lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_exec_cb(&a, fin_aro_cb);
    lv_anim_start(&a);
}

/* el contador de objetos: 0, 10, 20... hasta el total (cada número es una imagen y se enciende de a una) */
static void fin_mostrar_cb(lv_anim_t *a) { ocultar((lv_obj_t *)a->var, false); }
static void fin_num_cb(void *var, int32_t v)
{
    (void)var;
    if (v < 0 || v > 15) return;
    if (!s_fin_num[v] && s_tarjeta) { /* cada número se dibuja cuando le toca (hacer los 16 de golpe frenaba el cuadro y el sonido) */
        char t[8];
        snprintf(t, sizeof t, "%d", (int)(v < 15 ? v * 10 : PET_OBJETOS));
        s_fin_num[v] = texto(s_tarjeta, t, FIN_COLOR_ORO, 5, 0);
        lv_obj_align(s_fin_num[v], LV_ALIGN_TOP_MID, 0, 198);
    }
    for (int k = 0; k < 16; k++) if (s_fin_num[k]) ocultar(s_fin_num[k], k != v);
}

/* sin botón: se toca la pantalla (cuando ya apareció todo) y sigue a la página `sig` (o cierra, si ultimo) */
static lv_obj_t *final_boton(const char *txt, int sig, bool ultimo, uint32_t retraso)
{
    (void)txt;
    s_fin_toque_desde = s_ms + (retraso > 900 ? retraso : 900);
    lv_obj_remove_event_cb(s_tarjeta, final_toque_cb);
    lv_obj_add_flag(s_tarjeta, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_tarjeta, final_toque_cb, LV_EVENT_CLICKED, (void *)(intptr_t)(ultimo ? -1 : sig));
    return NULL;
}

static void fin_carta_papel(int cual, int y, int alto, int parte)
{
    lv_obj_t *papel = lv_obj_create(s_tarjeta);
    lv_obj_remove_style_all(papel);
    int pw = zona_ancho(y + 6) - 34;
    if (pw > 300) pw = 300;
    lv_obj_set_size(papel, pw, alto);
    lv_obj_align(papel, LV_ALIGN_TOP_MID, 0, y);
    lv_obj_set_style_bg_color(papel, lv_color_hex(0xFBF2DC), 0);
    lv_obj_set_style_bg_opa(papel, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(papel, 16, 0);
    lv_obj_set_style_border_width(papel, 3, 0);
    lv_obj_set_style_border_color(papel, lv_color_hex(0xB8925A), 0);
    lv_obj_set_style_shadow_width(papel, 18, 0);
    lv_obj_set_style_shadow_color(papel, lv_color_hex(FIN_COLOR_ORO), 0);
    lv_obj_set_style_shadow_opa(papel, LV_OPA_50, 0);
    lv_obj_remove_flag(papel, LV_OBJ_FLAG_SCROLLABLE);
    if (!parte) {
        lv_obj_t *ph = texto(papel, "PARA TI,", 0xB8925A, 2, 0);
        lv_obj_align(ph, LV_ALIGN_TOP_LEFT, 18, 12);
        fin_aparecer(ph, 1000);
    }
    lv_obj_t *tx = texto(papel, fin_carta(cual, s_pet, parte), C_TINTA, 2, pw - 36);
    lv_obj_align(tx, LV_ALIGN_TOP_LEFT, 18, parte ? 16 : 40);
    fin_aparecer(tx, 1300);
    if (parte) {
        char fr[40];
        snprintf(fr, sizeof fr, "- %s", s_pet->nombre);
        lv_obj_t *fi = texto(papel, fr, 0xB8925A, 2, 0);
        lv_obj_align(fi, LV_ALIGN_BOTTOM_RIGHT, -18, -10);
        fin_aparecer(fi, 3600);
    }
    fin_subir(papel, 0, 170); /* la hoja llega desde abajo, como una carta que se desliza */
}

/* sus recuerdos: una tarjeta por vez, con su dibujo grande, que cede el lugar a la siguiente */
static int fin_recuerdos(void)
{
    int n = pet_diary_count(s_pet);
    int sel[4], k = 0;
    if (n > 0) {
        int m = n < 4 ? n : 4;
        for (int i = 0; i < m; i++) sel[k++] = m == 1 ? 0 : (i * (n - 1)) / (m - 1);
    }
    const uint32_t DUR = 2900;
    for (int i = 0; i < k; i++) {
        const entrada_diario_t *d = pet_diary_get(s_pet, sel[i]);
        if (!d) continue;
        char fc[16], txt[120];
        fecha_corta(d->t, fc, sizeof fc);
        pet_diary_text(s_pet, d, txt, sizeof txt);
        const uint32_t ini = 300u + (uint32_t)i * DUR;
        const bool va = i < k - 1;
        lv_obj_t *papel = lv_obj_create(s_tarjeta);
        lv_obj_remove_style_all(papel);
        lv_obj_set_size(papel, 290, 230);
        lv_obj_align(papel, LV_ALIGN_TOP_MID, 0, 84);
        lv_obj_set_style_bg_color(papel, lv_color_hex(0x16203C), 0);
        lv_obj_set_style_bg_opa(papel, LV_OPA_90, 0);
        lv_obj_set_style_radius(papel, 26, 0);
        lv_obj_set_style_border_width(papel, 3, 0);
        lv_obj_set_style_border_color(papel, lv_color_hex(0xD0B969), 0);
        lv_obj_set_style_shadow_width(papel, 22, 0);
        lv_obj_set_style_shadow_color(papel, lv_color_hex(FIN_COLOR_ORO), 0);
        lv_obj_set_style_shadow_opa(papel, LV_OPA_40, 0);
        lv_obj_remove_flag(papel, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
        lv_obj_t *ic = icono(papel, icono_diario(d), 7);
        lv_obj_align(ic, LV_ALIGN_TOP_MID, 0, 14);
        lv_obj_t *fe = texto(papel, fc, FIN_COLOR_ORO, 2, 0);
        lv_obj_align(fe, LV_ALIGN_TOP_MID, 0, 104);
        lv_obj_t *tt = texto(papel, txt, 0xFFEFB8, 2, 250);
        lv_obj_align(tt, LV_ALIGN_TOP_MID, 0, 130);
        fin_visto(papel, ini, DUR - 700u, va);
        fin_aro(s_tarjeta, CX, 150, ini, false);
    }
    return k;
}

static void panel_final(int cual, int pag)
{
    if (!s_pet) return;
    s_fin_toque_desde = 0xFFFFFFFFu;
    lv_obj_remove_event_cb(s_tarjeta, final_toque_cb);
    s_fin_cual = cual;
    s_fin_pag = pag;
    fin_fondo();
    if (s_cerrar_x) ocultar(s_cerrar_x, !s_fin_repetida);
    char t[96];
    lv_obj_t *x;
    /* el secreto */
    if (pag == 0) { /* el libro brilla y se abre: aparece una última página */
        fin_aro(s_tarjeta, CX, 120, 0, true);
        fin_aro(s_tarjeta, CX, 120, 1800, true);
        fin_luciernagas(s_tarjeta, 14, 100, 300);
        x = icono(s_tarjeta, ICO_LIBRO_MAGICO, 10);
        lv_obj_align(x, LV_ALIGN_TOP_MID, 0, s_nino ? 36 : 40);
        fin_pop(x, 0, 40);
        { lv_anim_t a; lv_anim_init(&a); lv_anim_set_var(&a, x); lv_anim_set_values(&a, 0, -8); lv_anim_set_time(&a, 1400); lv_anim_set_playback_time(&a, 1400); lv_anim_set_delay(&a, 900);
          lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE); lv_anim_set_exec_cb(&a, fin_ty_cb); lv_anim_start(&a); } /* flota */
        /* (v99) una última página sale del Libro, escrita para ella con su letra chueca y la cucharita en la esquina: se ve, no se lee */
        snprintf(t, sizeof t, "PARA %s:\nTÚ APRENDISTE A CUIDARTE Y A CUIDAR. ¡GRACIAS!", s_pet->nombre);
        x = pagina_vieja(s_tarjeta, s_nino ? 190 : 176, 300, 132, t);
        fin_subir(x, 900, 24);
        final_boton("SIGUE", 1, false, 4200);
        if (!s_fin_repetida) accion(UI_ACCION_SONAR, SND_SORPRESA);
    } else if (pag == 1) { /* sus recuerdos, uno por vez */
        fin_luciernagas(s_tarjeta, 8, 90, 330);
        x = texto(s_tarjeta, "NUESTRA HISTORIA", FIN_COLOR_ORO, 2, 0);
        lv_obj_align(x, LV_ALIGN_TOP_MID, 0, 50);
        fin_aparecer(x, 0);
        const int k = fin_recuerdos();
        final_boton("SIGUE", 2, false, 1200u + (uint32_t)(k > 1 ? k - 1 : 0) * 2900u);
    } else if (pag == 2) { /* los 150 objetos caen y se acomodan en un mosaico; el contador sube */
        fin_luciernagas(s_tarjeta, 14, 100, 330);
        const int FILAS = 3, DY = 44;
        int n = 0;
        for (int f = 0; f < FILAS; f++) {
            const int y = 62 + f * DY;
            int cols = (zona_ancho(y + 10) - 40) / 44;
            if (cols > 7) cols = 7;
            if (cols < 2) cols = 2;
            for (int c = 0; c < cols; c++, n++) {
                const int id = (n * 7 + 3) % PET_OBJETOS; /* repartidos entre todas las familias */
                lv_obj_t *o = icono_obj(s_tarjeta, id, 3);
                lv_obj_set_pos(o, CX - cols * 44 / 2 + c * 44 + 4, y);
                lv_obj_set_style_translate_y(o, -150, 0);
                const uint32_t ret = 250u + (uint32_t)n * 70u;
                ocultar(o, true); /* aparece al empezar a caer (sin fundido: repintar con opacidad cuesta) */
                lv_anim_t a;
                lv_anim_init(&a);
                lv_anim_set_var(&a, o);
                lv_anim_set_values(&a, -150, 0);
                lv_anim_set_time(&a, 650);
                lv_anim_set_delay(&a, ret);
                lv_anim_set_path_cb(&a, lv_anim_path_bounce);
                lv_anim_set_exec_cb(&a, fin_ty_cb);
                lv_anim_set_start_cb(&a, fin_mostrar_cb);
                lv_anim_start(&a);
            }
        }
        const uint32_t fin_caida = 250u + (uint32_t)n * 70u + 700u;
        for (int k = 0; k < 16; k++) s_fin_num[k] = NULL;
        fin_num_cb(NULL, 0);
        { lv_anim_t a; lv_anim_init(&a); lv_anim_set_var(&a, s_tarjeta); lv_anim_set_values(&a, 0, 15); lv_anim_set_time(&a, 2400); lv_anim_set_delay(&a, 400);
          lv_anim_set_path_cb(&a, lv_anim_path_ease_out); lv_anim_set_exec_cb(&a, fin_num_cb); lv_anim_start(&a); }
        x = texto(s_tarjeta, "OBJETOS MÁGICOS, TODOS JUNTOS", 0xFFEFB8, 2, 380);
        lv_obj_align(x, LV_ALIGN_TOP_MID, 0, 240);
        fin_subir(x, fin_caida, 14);
        lv_obj_t *m = mini_criatura(s_tarjeta, PET_MAX_AMIGOS + 1, pet_look(s_pet), s_pet->etapa, s_pet->forma, 3, EXP_FELIZ);
        lv_obj_align(m, LV_ALIGN_TOP_MID, 0, 274);
        fin_aparecer(m, fin_caida);
        fin_salta(m, fin_caida + 600);
        fin_aro(s_tarjeta, CX, 296, fin_caida, false);
        final_boton("SIGUE", 3, false, fin_caida + 800);
        if (!s_fin_repetida) accion(UI_ACCION_SONAR, SND_HADAS);
    } else if (pag == 3) { /* el recuerdo dorado: Tilo, chiquito, guarda la mitad del Corazón en un huevo y lo esconde bajo el árbol viejo */
        escena_construir(s_tarjeta, 0, 0, &CINE_ORO, 0, 2, 466, 466, 176, false);
        if (!s_nino) {
            lv_obj_t *pl = lv_obj_create(s_tarjeta); /* una placa para que se lea sobre el dibujo */
            lv_obj_remove_style_all(pl);
            lv_obj_set_size(pl, 290, LV_SIZE_CONTENT);
            lv_obj_set_style_pad_ver(pl, 8, 0);
            lv_obj_align(pl, LV_ALIGN_TOP_MID, 0, 236);
            lv_obj_set_style_radius(pl, 20, 0);
            lv_obj_set_style_bg_color(pl, lv_color_hex(0x3A2410), 0);
            lv_obj_set_style_bg_opa(pl, LV_OPA_70, 0);
            lv_obj_remove_flag(pl, LV_OBJ_FLAG_CLICKABLE);
            x = texto(pl, "ANTES DE DORMIRSE CON LAS ESTRELLAS, GUARDÓ LA OTRA MITAD DEL CORAZÓN EN UN HUEVO. Y LO ESCONDIÓ PARA QUE LLEGARA A BUENAS MANOS.", 0xFFEFB8, 2, 262);
            lv_obj_set_style_text_align(x, LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_center(x);
            fin_aparecer(pl, 5200);
        }
        final_boton("SIGUE", 4, false, 6000);
        if (!s_fin_repetida) accion(UI_ACCION_SONAR, SND_HADAS);
    } else if (pag == 4) { /* el nombre: TILO, escrito a mano en la primera página del Libro; y la canción, completa */
        fin_luciernagas(s_tarjeta, 12, 60, 300);
        fin_aro(s_tarjeta, CX, 130, 300, true);
        lv_obj_t *li = icono(s_tarjeta, ICO_LIBRO_MAGICO, 6);
        lv_obj_align(li, LV_ALIGN_TOP_MID, 0, 46);
        fin_aparecer(li, 0);
        lv_obj_t *tilo = icono(s_tarjeta, ICO_TILO, 4);
        lv_obj_align(tilo, LV_ALIGN_TOP_MID, -110, 96);
        fin_subir(tilo, 1600, 20);
        x = texto(s_tarjeta, "PEUMO", FIN_COLOR_ORO, 6, 0);
        lv_obj_align(x, LV_ALIGN_TOP_MID, 0, 184);
        lv_image_set_rotation(x, -40); /* escrito a mano: un poquito chueco */
        fin_pop(x, 1000, 60);
        if (!s_nino) {
            x = texto(s_tarjeta, s_suave && s_pet->modo_nino == MODO_NINO ? "SE LLAMABA PEUMO. ERA CHIQUITO, COMO TÚ." :
                      "SE LLAMABA PEUMO. ERA CHIQUITO, COMO TÚ. TÚ CUIDASTE SU HUEVO, SU MUNDO Y SU CANCIÓN.", 0xCFF5C8, 2, 320);
            lv_obj_set_style_text_align(x, LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_align(x, LV_ALIGN_TOP_MID, 0, 250);
            fin_aparecer(x, 2600);
            x = texto(s_tarjeta, "HABÍA UNA VEZ ALGUIEN QUE CUIDÓ A QUIEN LO CUIDABA. Y EL MUNDO RECORDÓ.", 0xFFEFB8, 2, 280);
            lv_obj_set_style_text_align(x, LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_align(x, LV_ALIGN_TOP_MID, 0, 324);
            fin_aparecer(x, 5200);
        }
        final_boton("SIGUE", 5, false, 6000);
        if (!s_fin_repetida) {
            accion(UI_ACCION_SONAR, SND_PRIMAVERA); /* el primer verso de su canción... */
            uint8_t m[PET_MELODIA_NOTAS];
            const int nm = pet_melody(s_pet, m);
            if (nm > 0) { tocar_cristales(m, nm, 480); s_mel_prox = s_ms + 4200; } /* ...y el que ella compuso en la cueva: el verso que faltaba */
        }
    } else if (pag == 5) { /* la carta, primera hoja */
        fin_luciernagas(s_tarjeta, 8, 330, 420);
        fin_carta_papel(cual, 60, 250, 0); /* las dos hojas, del mismo tamaño y en el mismo lugar */
        final_boton("SIGUE", 6, false, 2600);
    } else if (pag == 6) { /* la carta, la despedida y la estampita dorada que cae como un sello */
        fin_luciernagas(s_tarjeta, 8, 330, 420);
        fin_carta_papel(cual, 60, 250, 1);
        x = icono(s_tarjeta, ICO_ESTAMPITA, 4);
        lv_obj_align(x, LV_ALIGN_TOP_MID, -62, 252);
        lv_obj_set_style_image_recolor(x, lv_color_hex(FIN_COLOR_ORO), 0);
        lv_obj_set_style_image_recolor_opa(x, LV_OPA_50, 0);
        fin_pop(x, 4200, 900);
        fin_aro(s_tarjeta, CX - 62, 270, 4300, false);
        final_boton("SIGUE", 7, false, 5200);
    } else { /* el cierre: ella, su nombre, el tiempo juntos */
        fin_aro(s_tarjeta, CX, 190, 0, true);
        fin_aro(s_tarjeta, CX, 190, 1300, true);
        fin_luciernagas(s_tarjeta, 22, 80, 380);
        lv_obj_t *m = mini_criatura(s_tarjeta, PET_MAX_AMIGOS + 1, pet_look(s_pet), s_pet->etapa, s_pet->forma, 8, EXP_FELIZ);
        lv_obj_align(m, LV_ALIGN_TOP_MID, 0, 110);
        fin_pop(m, 200, 40);
        fin_salta(m, 1200);
        x = texto(s_tarjeta, s_pet->nombre, FIN_COLOR_ORO, 4, 0);
        lv_obj_align(x, LV_ALIGN_TOP_MID, 0, 48);
        fin_subir(x, 600, 14);
        if (!s_nino) {
            const int dj = (int)pet_age_days(s_pet, s_ultimo_t) + 1;
            snprintf(t, sizeof t, dj == 1 ? "1 DÍA JUNTOS" : "%d DÍAS JUNTOS", dj);
            x = texto(s_tarjeta, t, 0xFFEFB8, 3, 0);
            lv_obj_align(x, LV_ALIGN_TOP_MID, 0, 262);
            fin_subir(x, 1800, 14);
            x = texto(s_tarjeta, cual == PET_FINAL_REGULAR ? "{GUARDIANA} DEL MUNDO" : "HASTA PRONTO", 0xCFF5C8, 2, 0);
            lv_obj_align(x, LV_ALIGN_TOP_MID, 0, 306);
            fin_subir(x, 3000, 14);
        }
        /* sin botón: se toca la pantalla (cuando ya apareció todo) y sigue el juego */
        s_fin_toque_desde = s_ms + 4200;
        lv_obj_remove_event_cb(s_tarjeta, final_toque_cb);
        lv_obj_add_flag(s_tarjeta, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(s_tarjeta, final_toque_cb, LV_EVENT_CLICKED, (void *)(intptr_t)-1);
    }
}


/* ======================================================================
 * La historia del rito: por qué ella es la Guardiana. Sobre el cielo estrellado del Libro, en hojas cortas:
 * se avanza tocando la pantalla (o SIGUE); al final, su nombre y el cierre del Libro.
 * ==================================================================== */
#define RITO_HOJAS 5
static const char *const RITO_TXT[RITO_HOJAS] = {
    "ANTES, EL CUIDADOR, EL REY DE ESTE MUNDO, LO CUIDABA TODO ÉL SOLO. CUANDO SE FUE A DORMIR CON LAS ESTRELLAS, TODOS CREYERON QUE MEDIO CORAZÓN DEL MUNDO SE HABÍA IDO CON ÉL.",
    "LA OTRA MITAD QUEDÓ DORMIDA EN LA ISLA, SOBRE EL LOMO DE LA ABUELA BRIZNA, ESPERANDO A QUIEN APRENDIERA A CUIDAR SIN QUE SE LO PIDIERAN.",
    "LLEGÓ {ELLA}. ESCUCHÓ A LAS CRIATURAS, ENCONTRÓ LO ESCONDIDO Y CANTÓ CON LOS CRISTALES. CUANDO PUSO EL CORAZÓN EN LA PIEDRA, ALGO LATIÓ DENTRO DE {ELLA}.",
    "LA MITAD QUE FALTABA NUNCA SE FUE: ESTABA EN {ELLA}. POR ESO, CUANDO RESPIRA HONDO, EL BOSQUE ENTERO RESPIRA CON {ELLA}.",
    "{GUARDIANA} NO ES QUIEN MANDA: ES QUIEN NO DEJA A NADIE SOLO. Y DEJARSE CUIDAR TAMBIÉN ES CUIDAR.",
};
static void panel_rito_hoja(int h);
static void rito_hoja_cb(lv_event_t *e)
{
    const int h = (int)(intptr_t)lv_event_get_user_data(e);
    if (s_panel != PANEL_FINAL) return;
    if (h >= RITO_HOJAS) { /* se acabó la historia: su nombre y el tiempo juntos, y luego el Libro */
        s_fin_repetida = true;
        s_fin_libro7 = true;
        panel_final(PET_FINAL_REGULAR, 7);
        return;
    }
    panel_rito_hoja(h);
}
static void panel_rito_hoja(int h)
{
    if (!s_pet) return;
    s_fin_cual = PET_FINAL_REGULAR;
    s_fin_repetida = true;
    fin_fondo();
    if (s_cerrar_x) ocultar(s_cerrar_x, true); /* se sigue con SIGUE o tocando */
    fin_luciernagas(s_tarjeta, 10, 100, 330);
    lv_obj_t *x = NULL;
    int y = 90; /* solo la primera hoja lleva el libro; las que siguen, solo el texto */
    if (h == 0) {
        x = icono(s_tarjeta, ICO_LIBRO_MAGICO, 6);
        lv_obj_align(x, LV_ALIGN_TOP_MID, 0, 40);
        fin_aparecer(x, 0);
        y = 128;
    }
    if (h == 0) {
        char t[64];
        snprintf(t, sizeof t, "POR QUÉ %s ES {LA} {GUARDIANA}", s_pet->nombre);
        mayusculas(t);
        x = texto(s_tarjeta, t, FIN_COLOR_ORO, 2, 320);
        lv_obj_set_style_text_align(x, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(x, LV_ALIGN_TOP_MID, 0, y);
        fin_aparecer(x, 400);
        y += 44;
    } else y += 70;
    x = texto(s_tarjeta, RITO_TXT[h], h == RITO_HOJAS - 1 ? 0xFFEFB8 : 0xCFF5C8, 2, 330);
    lv_obj_set_style_text_align(x, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(x, LV_ALIGN_TOP_MID, 0, y);
    fin_aparecer(x, 900);
    /* tocar en cualquier parte pasa a la hoja siguiente (sin botón); la última lleva al final */
    lv_obj_add_flag(s_tarjeta, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_tarjeta, rito_hoja_cb, LV_EVENT_CLICKED, (void *)(intptr_t)(h + 1));
    if (h == 0) accion(UI_ACCION_SONAR, SND_SORPRESA);
}
static void rito_historia(void)
{
    if (!s_pet) return;
    panel_rito_hoja(0);
}

/* cuando el motor lo avisa y no hay nada abierto, empieza el final */
static void final_revisar(void)
{
    if (!s_pet || s_panel != PANEL_NINGUNO || s_pet->dormida || s_pet->etapa == STAGE_HUEVO) return;
    const int f = pet_final_pending(s_pet);
    if (!f || s_interior || s_cie_activo || s_via_activo || s_cru_activo || s_escena < 0 || s_juego != JG_NADA || s_bienv_activa) return;
    s_fin_repetida = false;
#ifdef ESP_PLATFORM
    /* (1.0.24) el final (el Libro grande que flota y se agranda, aros, luciérnagas, fondo con degradé) pide memoria y
     * dibujo. En las pruebas del 18-nov, con la PSRAM en ~300 KB, el orbe dejaba de contestar apenas empezaba (íconos
     * soltándose y rehaciéndose sin parar). Antes de abrirlo se hace lugar, como en la despedida. */
    {
        extern void hal_evento(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
        const size_t ps0 = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
        uint32_t b = 0;
        if (ps0 < 1200u * 1024u || heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM) < 600u * 1024u) {
            lv_image_cache_drop(NULL);
            b = iconos_soltar();
            art_fondos_soltar_otros();
        }
        hal_evento("final: empieza (%s); PSRAM %u KB -> %u KB (bloque mayor %u KB; íconos soltados %u KB), RAM interna %u B",
                   f == PET_FINAL_SECRETO ? "secreto" : "regular", (unsigned)(ps0 / 1024),
                   (unsigned)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024),
                   (unsigned)(heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM) / 1024), (unsigned)(b / 1024),
                   (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    }
#endif
    if (f == PET_FINAL_SECRETO) { cer_empezar(2 /* CER_VERSO */, false); return; } /* (v39) el último verso, en su pantalla */
    panel_final(f, 0);
}

/* La Biblioteca: ya completo, se puede volver a ver la última página */
static void biblioteca_cb(lv_event_t *e)
{
    (void)e;
    cer_empezar(2 /* CER_VERSO */, true); /* (v39) volver a ver el último verso */
}

/* (v39) La cuchara de Tilo: después del último verso, en las noches despejadas se ve en lo alto del cielo, y parpadea despacio */
#define CONST_N 10
static lv_obj_t *s_const_e[CONST_N], *s_const_l[2];
static lv_point_precise_t s_const_p[CONST_N + 1];
static bool s_const_ok;
static void constelacion_actualizar(bool ver)
{
    if (!s_pet) return;
    if (!s_const_ok) {
        if (!ver) return;
        static const int8_t CU[CONST_N][2] = {{-70, 0}, {-56, -18}, {-34, -20}, {-20, -4}, {-34, 14}, {-56, 14}, {-4, -2}, {22, 4}, {48, 10}, {74, 16}}; /* el cuenco y el mango */
        for (int i = 0; i < CONST_N; i++) {
            lv_obj_t *e = lv_obj_create(lv_screen_active());
            lv_obj_remove_style_all(e);
            lv_obj_set_size(e, 6, 6);
            lv_obj_set_style_radius(e, LV_RADIUS_CIRCLE, 0);
            lv_obj_set_style_bg_color(e, lv_color_hex(0xFFF3B0), 0);
            lv_obj_set_style_bg_opa(e, LV_OPA_COVER, 0);
            lv_obj_remove_flag(e, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_pos(e, CX + CU[i][0] - 3, 92 + CU[i][1] - 3);
            s_const_e[i] = e;
            s_const_p[i].x = CX + CU[i][0];
            s_const_p[i].y = 92 + CU[i][1];
        }
        s_const_p[6] = s_const_p[0]; /* (cierra el cuenco: 0..5 y vuelta al 0) */
        static lv_point_precise_t cuenco[7], mango[5];
        for (int i = 0; i < 6; i++) cuenco[i] = (lv_point_precise_t){CX + CU[i][0], 92 + CU[i][1]};
        cuenco[6] = cuenco[0];
        mango[0] = (lv_point_precise_t){CX + CU[3][0], 92 + CU[3][1]};
        for (int i = 6; i < CONST_N; i++) mango[i - 5] = (lv_point_precise_t){CX + CU[i][0], 92 + CU[i][1]};
        for (int k = 0; k < 2; k++) {
            s_const_l[k] = lv_line_create(lv_screen_active());
            lv_line_set_points(s_const_l[k], k == 0 ? cuenco : mango, k == 0 ? 7 : 5);
            lv_obj_set_style_line_width(s_const_l[k], 1, 0);
            lv_obj_set_style_line_color(s_const_l[k], lv_color_hex(0xFFF3B0), 0);
            lv_obj_set_style_line_opa(s_const_l[k], LV_OPA_40, 0);
            lv_obj_remove_flag(s_const_l[k], LV_OBJ_FLAG_CLICKABLE);
        }
        s_const_ok = true;
    }
    for (int i = 0; i < CONST_N; i++) {
        ocultar(s_const_e[i], !ver);
        if (ver) lv_obj_set_style_opa(s_const_e[i], (lv_opa_t)(170 + 80 * sinf((float)s_ms / 700.0f + (float)i * 0.4f)), 0);
    }
    for (int k = 0; k < 2; k++) ocultar(s_const_l[k], !ver);
}


/* ======================================================================
 * Las páginas del Libro: el Libro cuenta el primer libro del cuento en ocho momentos, una sola vez cada uno.
 * (el cuento completo está en el documento; aquí va lo que cabe en un reloj)
 * ==================================================================== */
static uint8_t s_libro_cola;   /* páginas que esperan a que no haya otra cosa en pantalla */
static uint32_t s_libro_listo; /* no antes de este instante (ms) */

static const char *const LIBRO_TXT[PET_LIBRO_PAGINAS][3] = {
    /* (fw 108) las dos primeras, contadas por el Libro mismo y más concretas (antes mezclaban «este libro» con «tuve») */
    {"TUVE UNA HISTORIA Y, CON EL TIEMPO, SE FUE BORRANDO.", "ME QUEDAN LAS TAPAS, UN HILO SUELTO Y LAS PÁGINAS EN BLANCO.", "HOY LLEGASTE TÚ: EMPECEMOS DE NUEVO."},
    {"ANTES FUI UN DIARIO: COSAS POR CUIDAR, RECETAS DE GALLETAS Y CHISTES MALOS.", "CUANDO SU DUEÑO SE ENFERMÓ, ME OLVIDARON EN UN RINCÓN.", "HASTA HOY. AHORA ME LLENO CON LO QUE VIVAN USTEDES DOS."},
    {"PARA LLEGAR A LA ISLA HAY QUE CRUZAR EN BOTE.", "MAREA ACOMPAÑA A TODOS: DICE QUE NADIE DEBERÍA CRUZAR SOLO. ELLA SABE POR QUÉ.", NULL},
    {"LAS CRIATURAS RECUERDAN AL CUIDADOR, EL REY DE ESTE MUNDO.", "CADA UNA LO RECUERDA DISTINTO. QUIZÁS NINGUNA SE EQUIVOCA. QUIZÁS TODAS UN POCO.", NULL},
    {"AL DESCUBRIR UN SECRETO, UNA PÁGINA VIEJA VOLVIÓ A APARECER.", NULL, NULL}, /* (v99) la hoja se dibuja: se ve, no se lee */
    {"PRISMA, EL DRAGÓN DE CRISTAL, DESPERTÓ DESPUÉS DE CIEN AÑOS.", "SI LO VISITAS EN LA CUEVA, TE CONTARÁ SU PARTE. UN CAPÍTULO POR DÍA.", NULL},
    {"ELLA COMPUSO UNA MELODÍA PROPIA CON LOS CRISTALES.", "NADIE SABÍA TODAVÍA QUE ERA EL VERSO QUE FALTABA.", NULL},
    {"ASÍ TERMINA EL LIBRO.", "LA MITAD QUE FALTABA NUNCA SE FUE: ESTABA CONTIGO.", "PERO A USTEDES TODAVÍA LES QUEDAN MUCHAS PÁGINAS POR ESCRIBIR."},
};

/* (v99) una hoja del diario de Peumo, dibujada: papel gastado puesto un poco torcido, lo que escribió con su letra (se lee),
 * una mancha de galleta y la cucharita en la esquina (su firma). */
static lv_obj_t *pagina_vieja(lv_obj_t *padre, int y, int ancho, int alto, const char *escrito)
{
    lv_obj_t *h = lv_obj_create(padre);
    lv_obj_remove_style_all(h);
    lv_obj_set_size(h, ancho, alto);
    lv_obj_align(h, LV_ALIGN_TOP_MID, 0, y);
    lv_obj_set_style_bg_color(h, lv_color_hex(0xF2E4C0), 0);
    lv_obj_set_style_bg_grad_color(h, lv_color_hex(0xE2CC9A), 0);
    lv_obj_set_style_bg_grad_dir(h, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_bg_opa(h, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(h, 4, 0);
    lv_obj_set_style_border_width(h, 2, 0);
    lv_obj_set_style_border_color(h, lv_color_hex(0x9A7A50), 0);
    lv_obj_set_style_shadow_width(h, 14, 0);
    lv_obj_set_style_shadow_color(h, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(h, LV_OPA_40, 0);
    lv_obj_remove_flag(h, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_transform_pivot_x(h, ancho / 2, 0);
    lv_obj_set_style_transform_pivot_y(h, alto / 2, 0);
    lv_obj_set_style_transform_rotation(h, -25, 0); /* -2,5°: puesta de cualquier manera, como la dejaba él */
    lv_obj_t *m = lv_obj_create(h); /* la mancha de galleta */
    lv_obj_remove_style_all(m);
    lv_obj_set_size(m, 24, 16);
    lv_obj_set_style_radius(m, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(m, lv_color_hex(0xB0864A), 0);
    lv_obj_set_style_bg_opa(m, LV_OPA_30, 0);
    lv_obj_set_pos(m, ancho - 70, 8);
    if (escrito) { /* lo que escribió, con su letra: un poco chueca, pero se lee */
        lv_obj_t *l = texto_c(h, escrito, 0x4A2E18, 2, ancho - 40);
        lv_obj_set_pos(l, 14, 10);
        lv_obj_set_style_transform_rotation(l, 12, 0);
    }
    lv_obj_t *c = icono(h, ICO_CUCHARA_PALO, 2); /* la cucharita en la esquina: su firma */
    lv_obj_align(c, LV_ALIGN_BOTTOM_RIGHT, -6, -4);
    lv_obj_set_style_transform_rotation(c, 300, 0);
    return h;
}

/* (declarada en ui.c) */ /* la página 0 de una partida nueva: al seguir, empieza la configuración (hora, nombre...) */
static void libro_cb(lv_event_t *e)
{
    (void)e;
    cerrar_panel();
    if (s_libro_inicio) { s_libro_inicio = false; inicio_huevo(); }
}

static void panel_libro(int n)
{
    if (!s_pet || n < 0 || n >= PET_LIBRO_PAGINAS) return;
    s_fin_cual = -1;
    s_fin_repetida = !s_libro_inicio; /* se puede cerrar con la X (la primera, no: hay que seguir a la configuración) */
    fin_fondo();
    if (s_cerrar_x) ocultar(s_cerrar_x, s_libro_inicio);
    fin_luciernagas(s_tarjeta, 8, 330, 420); /* (abajo: antes flotaban detrás del título y del texto) */
    lv_obj_t *x = icono(s_tarjeta, ICO_LIBRO_MAGICO, 6);
    lv_obj_align(x, LV_ALIGN_TOP_MID, 0, 40);
    lv_obj_set_style_image_recolor(x, lv_color_hex(0x8A6A3A), 0); /* (v37) la portada, gastada de vieja */
    lv_obj_set_style_image_recolor_opa(x, LV_OPA_30, 0);
    fin_aparecer(x, 0);
    x = texto(s_tarjeta, "EL LIBRO", FIN_COLOR_ORO, 3, 0);
    lv_obj_align(x, LV_ALIGN_TOP_MID, 0, 128);
    fin_aparecer(x, 500);
    lv_obj_t *col = lv_obj_create(s_tarjeta);
    lv_obj_remove_style_all(col);
    lv_obj_set_size(col, 356, 162);
    lv_obj_align(col, LV_ALIGN_TOP_MID, 0, 160);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(col, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(col, 6, 0);
    lv_obj_remove_flag(col, LV_OBJ_FLAG_CLICKABLE);
    for (int i = 0; i < 3 && LIBRO_TXT[n][i]; i++) {
        lv_obj_t *l = texto_c(col, LIBRO_TXT[n][i], i == 0 ? 0xFFEFB8 : 0xCFF5C8, 2, 350);
        lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
        fin_aparecer(l, 1300 + (uint32_t)i * 1500);
    }
    if (n == 4) { /* (v99) la página vieja que volvió: su letra chueca y la cucharita en la esquina se ven en el dibujo */
        char pt[200];
        snprintf(pt, sizeof pt, "%s", pet_story_short(NARR_TILO, 0)); /* «Cosas por cuidar hoy: regar, consolar a la nube...» */
        mayusculas(pt);
        lv_obj_t *pg = pagina_vieja(s_tarjeta, 214, 350, 106, pt);
        fin_subir(pg, 2400, 16);
    }
    lv_obj_t *ok = boton(s_tarjeta, 150, 48, libro_cb, 0);
    ok_alinear(ok, 0);
    lv_obj_set_style_bg_color(ok, lv_color_hex(C_VIOLETA), 0);
    lv_obj_set_style_border_width(ok, 2, 0);
    lv_obj_set_style_border_color(ok, lv_color_hex(FIN_COLOR_ORO), 0);
    lv_obj_t *t = texto(ok, "SIGUE", C_BLANCO, 3, 0);
    lv_obj_center(t);
    fin_aparecer(ok, 1300 + 1500u * 2);
    accion(UI_ACCION_LIBRO, n);
    accion(UI_ACCION_SONAR, SND_SORPRESA);
}

/* pide una página: sale sola cuando no hay otra cosa en pantalla (y solo si todavía no la leyó) */
static void libro_pedir(int n)
{
    if (!s_pet || s_nino || n < 0 || n >= PET_LIBRO_PAGINAS || pet_libro_seen(s_pet, n)) return;
    s_libro_cola |= (uint8_t)(1u << n);
    s_libro_listo = s_ms + 3000; /* que antes se vea lo que la provocó */
}

static void libro_revisar(void)
{
    if (s_fin_libro7 && s_panel == PANEL_NINGUNO) { s_fin_libro7 = false; libro_pedir(7); } /* cerró el cierre del rito con la X: el Libro igual aparece */
    if (!s_libro_cola || s_cine_pend || !s_pet || s_panel != PANEL_NINGUNO || s_bienv_activa || s_pecera || s_pet->dormida) return;
    if ((int32_t)(s_ms - s_libro_listo) < 0) return;
    int n = 0;
    while (!((s_libro_cola >> n) & 1u)) n++;
    s_libro_cola &= (uint8_t)~(1u << n);
    if (pet_libro_seen(s_pet, n)) return;
    panel_libro(n);
}

/* lo primero que ve: después de caer el huevo, el Libro se presenta (y de ahí sigue la configuración de siempre) */
static void libro_inicio(void)
{
    if (!s_pet || s_nino || pet_libro_seen(s_pet, 0)) { if (s_pet && !s_pet->edad_dueno) inicio_huevo(); else panel_hora(); return; }
    s_libro_inicio = true;
    panel_libro(0);
}
