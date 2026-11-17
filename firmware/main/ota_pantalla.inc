/* (1.0.13) Las pantallas del modo Wi-Fi (lista de redes, teclado, aviso, descarga). Las incluye ota.c y también
 * sim/preview/ota_sheet.c (vista previa en el PC). Usa del que la incluye: s_prueba, s_redes, s_nredes, s_nueva,
 * s_nueva_nombre, s_cancelar, s_buscar_otra, s_ya, s_elegida, s_conectar_pedido, s_disp, s_msg1, s_msg2 y uso(). */
/* ------------------------------------------------------------------ la pantalla */

#define C_FONDO 0x1B1530
#define C_AMARILLO 0xFFE27A
#define C_SUAVE 0xB9B0E0
#define C_BOTON 0x3A3160
#define C_VERDE 0x2E9E50
#define C_ROJO 0x8A3A4A

/* un texto (imagen de la letra del orbe) con su búfer propio, que se suelta solo al borrarse */
static void txt_borrado(lv_event_t *e)
{
    art_img_t *a = lv_event_get_user_data(e);
    if (a) { art_img_free(a); free(a); }
}
static lv_obj_t *txt(lv_obj_t *padre, const char *s, uint32_t color, int escala, int ancho, bool minus)
{
    lv_obj_t *img = lv_image_create(padre);
    art_img_t *a = calloc(1, sizeof *a);
    if (!a) return img;
    art_texto_minusculas(minus);
    art_texto_centrado(a, s && s[0] ? s : " ", color, escala, ancho);
    art_texto_minusculas(false);
    lv_image_set_src(img, &a->dsc);
    lv_obj_add_event_cb(img, txt_borrado, LV_EVENT_DELETE, a);
    lv_obj_remove_flag(img, LV_OBJ_FLAG_CLICKABLE);
    return img;
}
static lv_obj_t *txt_cambiar(lv_obj_t *img, const char *s, uint32_t color, int escala, int ancho, bool minus)
{
    if (!img) return NULL;
    lv_obj_t *p = lv_obj_get_parent(img);
    const int32_t x = lv_obj_get_x_aligned(img), y = lv_obj_get_y_aligned(img);
    lv_obj_t *n = txt(p, s, color, escala, ancho, minus);
    lv_obj_align(n, LV_ALIGN_TOP_MID, x, y);
    lv_obj_delete(img);
    return n;
}

static lv_obj_t *boton(lv_obj_t *padre, int w, int h, uint32_t color, const char *s, int escala, lv_event_cb_t cb, intptr_t dato, bool minus)
{
    lv_obj_t *b = lv_obj_create(padre);
    lv_obj_remove_style_all(b);
    lv_obj_set_size(b, w, h);
    lv_obj_set_style_bg_color(b, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(b, 12, 0);
    lv_obj_set_style_bg_color(b, lv_color_hex(0x6A5AA8), LV_STATE_PRESSED);
    lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(b, LV_OBJ_FLAG_SCROLLABLE);
    if (cb) lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, (void *)dato);
    if (s) { lv_obj_t *t = txt(b, s, 0xFFFFFF, escala, w - 8, minus); lv_obj_center(t); }
    return b;
}

static lv_obj_t *s_scr, *s_barra, *s_barra_fill, *s_pct_txt, *s_campo, *s_teclas, *s_aviso_txt;

static void limpiar(void)
{
    s_scr = lv_screen_active();
    lv_obj_clean(s_scr);
    s_barra = s_barra_fill = s_pct_txt = s_campo = s_teclas = s_aviso_txt = NULL;
    lv_obj_set_style_bg_color(s_scr, lv_color_hex(C_FONDO), 0);
    lv_obj_set_style_bg_opa(s_scr, LV_OPA_COVER, 0);
    lv_obj_remove_flag(s_scr, LV_OBJ_FLAG_SCROLLABLE);
}

/* --- botones comunes */
static void cancelar_cb(lv_event_t *e) { (void)e; uso(); s_cancelar = true; }
static void otra_cb(lv_event_t *e) { (void)e; uso(); s_buscar_otra = true; }
static void ya_cb(lv_event_t *e) { (void)e; uso(); s_ya = true; }
static void boton_cancelar(int y, const char *s)
{
    lv_obj_t *b = boton(s_scr, 170, 44, C_ROJO, s ? s : "CANCELAR", 2, cancelar_cb, 0, false);
    lv_obj_align(b, LV_ALIGN_TOP_MID, 0, y);
}

/* --- pantalla de mensaje (título, texto, botones) */
static void pantalla_msg(const char *t1, const char *t2, bool con_otra, bool con_cancelar)
{
    limpiar();
    lv_obj_t *a = txt(s_scr, "MODO WI-FI", C_SUAVE, 2, 300, false);
    lv_obj_align(a, LV_ALIGN_TOP_MID, 0, 44);
    lv_obj_t *b = txt(s_scr, t1, C_AMARILLO, 3, 360, false);
    lv_obj_align(b, LV_ALIGN_TOP_MID, 0, 120);
    if (t2 && t2[0]) { lv_obj_t *c = txt(s_scr, t2, 0xFFFFFF, 2, 340, true); lv_obj_align_to(c, b, LV_ALIGN_OUT_BOTTOM_MID, 0, 22); }
    if (con_otra) { lv_obj_t *o = boton(s_scr, 200, 44, C_BOTON, "OTRA RED", 2, otra_cb, 0, false); lv_obj_align(o, LV_ALIGN_TOP_MID, 0, 310); }
    if (con_cancelar) boton_cancelar(370, NULL);
}

/* --- la lista de redes */
static void red_cb(lv_event_t *e) { uso(); s_elegida = (int)(intptr_t)lv_event_get_user_data(e); }
static void pantalla_lista(void)
{
    limpiar();
    lv_obj_t *a = txt(s_scr, s_nredes ? "ELIGE TU WI-FI" : "NO ENCONTRÉ REDES", C_AMARILLO, 3, 340, false);
    lv_obj_align(a, LV_ALIGN_TOP_MID, 0, 40);
    lv_obj_t *l = lv_obj_create(s_scr);
    lv_obj_remove_style_all(l);
    lv_obj_set_size(l, 330, 236);
    lv_obj_align(l, LV_ALIGN_TOP_MID, 0, 82);
    lv_obj_set_flex_flow(l, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(l, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(l, 6, 0);
    lv_obj_set_scroll_dir(l, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(l, LV_SCROLLBAR_MODE_OFF);
    for (int i = 0; i < s_nredes; i++) {
        lv_obj_t *b = boton(l, 310, 46, C_BOTON, NULL, 2, red_cb, i, true);
        lv_obj_add_flag(b, LV_OBJ_FLAG_SCROLL_CHAIN_VER);
        lv_obj_t *t = txt(b, s_redes[i].ssid, 0xFFFFFF, 2, 230, true);
        lv_obj_align(t, LV_ALIGN_LEFT_MID, 12, 0);
        const int barras = s_redes[i].rssi > -55 ? 4 : s_redes[i].rssi > -67 ? 3 : s_redes[i].rssi > -78 ? 2 : 1;
        for (int k = 0; k < 4; k++) { /* la señal: cuatro palitos */
            lv_obj_t *p = lv_obj_create(b);
            lv_obj_remove_style_all(p);
            lv_obj_set_size(p, 5, 6 + k * 5);
            lv_obj_align(p, LV_ALIGN_BOTTOM_RIGHT, -12 - (3 - k) * 8, -10);
            lv_obj_set_style_bg_color(p, lv_color_hex(k < barras ? C_AMARILLO : 0x5A527A), 0);
            lv_obj_set_style_bg_opa(p, LV_OPA_COVER, 0);
            lv_obj_remove_flag(p, LV_OBJ_FLAG_CLICKABLE);
        }
    }
    lv_obj_t *o = boton(s_scr, 210, 44, C_BOTON, "BUSCAR DE NUEVO", 2, otra_cb, 0, false);
    lv_obj_align(o, LV_ALIGN_TOP_MID, 0, 326);
    boton_cancelar(378, NULL);
}

/* --- el teclado */
static int s_modo_tec;      /* 0 minúsculas, 1 mayúsculas, 2 números y símbolos */
static bool s_ver_clave;
static volatile bool s_tec_rearmar; /* (se rearma desde la tarea principal, no dentro del toque) */
static const char *const FILAS[3][4] = {
    {"qwertyuiop", "asdfghjkl", "zxcvbnm", ""},
    {"QWERTYUIOP", "ASDFGHJKL", "ZXCVBNM", ""},
    {"1234567890", "@#$%&*-+=_", ".,;:!?'\"/\\", "()[]<>{}|~"},
};
enum { T_BORRAR = 1000, T_ESPACIO, T_MODO, T_NUM, T_VER, T_CONECTAR };
static void teclado_armar(void);
static void campo_refrescar(void)
{
    char b[70];
    const size_t n = strlen(s_prueba.clave);
    if (s_ver_clave) snprintf(b, sizeof b, "%s_", s_prueba.clave);
    else { size_t i = 0; for (; i < n && i < sizeof b - 2; i++) b[i] = '*'; b[i++] = '_'; b[i] = 0; }
    if (s_campo) s_campo = txt_cambiar(s_campo, b, 0xFFFFFF, 2, 280, true);
}
static void tecla_cb(lv_event_t *e)
{
    uso();
    const int k = (int)(intptr_t)lv_event_get_user_data(e);
    size_t n = strlen(s_prueba.clave);
    if (k == T_BORRAR) { if (n) s_prueba.clave[n - 1] = 0; }
    else if (k == T_ESPACIO) { if (n + 1 < sizeof s_prueba.clave) { s_prueba.clave[n] = ' '; s_prueba.clave[n + 1] = 0; } }
    else if (k == T_MODO) { s_modo_tec = s_modo_tec == 0 ? 1 : 0; s_tec_rearmar = true; return; }
    else if (k == T_NUM) { s_modo_tec = s_modo_tec == 2 ? 0 : 2; s_tec_rearmar = true; return; }
    else if (k == T_VER) { s_ver_clave = !s_ver_clave; }
    else if (k == T_CONECTAR) { s_conectar_pedido = true; return; }
    else if (k > 0 && k < 128 && n + 1 < sizeof s_prueba.clave) { s_prueba.clave[n] = (char)k; s_prueba.clave[n + 1] = 0; }
    campo_refrescar();
}
static void teclado_armar(void)
{
    if (!s_teclas) return;
    lv_obj_clean(s_teclas);
    const int W = 33, H = 38, G = 3, Y0 = 0;
    for (int f = 0; f < 4; f++) {
        const char *fila = FILAS[s_modo_tec][f];
        const int n = (int)strlen(fila);
        if (!n) continue;
        const int ancho = n * W + (n - 1) * G;
        for (int i = 0; i < n; i++) {
            char t[2] = {fila[i], 0};
            lv_obj_t *b = boton(s_teclas, W, H, C_BOTON, t, 2, tecla_cb, (intptr_t)(unsigned char)fila[i], s_modo_tec == 0);
            lv_obj_set_pos(b, (370 - ancho) / 2 + i * (W + G), Y0 + f * (H + G));
        }
    }
    /* la fila de abajo: modo, números, espacio, borrar, conectar (justo debajo de las filas que haya) */
    const int filas = FILAS[s_modo_tec][3][0] ? 4 : 3;
    const int y = Y0 + filas * (H + G) + 2;
    lv_obj_t *b;
    b = boton(s_teclas, 50, 40, C_BOTON, s_modo_tec == 1 ? "abc" : "ABC", 2, tecla_cb, T_MODO, true); lv_obj_set_pos(b, 22, y);
    b = boton(s_teclas, 50, 40, C_BOTON, s_modo_tec == 2 ? "abc" : "123", 2, tecla_cb, T_NUM, true); lv_obj_set_pos(b, 76, y);
    b = boton(s_teclas, 74, 40, C_BOTON, "ESPACIO", 1, tecla_cb, T_ESPACIO, false); lv_obj_set_pos(b, 130, y);
    b = boton(s_teclas, 48, 40, C_BOTON, "<", 2, tecla_cb, T_BORRAR, false); lv_obj_set_pos(b, 208, y);
    b = boton(s_teclas, 108, 40, C_VERDE, "CONECTAR", 2, tecla_cb, T_CONECTAR, false); lv_obj_set_pos(b, 260, y);
}
static void pantalla_clave(const char *aviso)
{
    limpiar();
    char t[60];
    snprintf(t, sizeof t, "%s", s_prueba.ssid);
    lv_obj_t *a = txt(s_scr, t, C_AMARILLO, 2, 300, true);
    lv_obj_align(a, LV_ALIGN_TOP_MID, 0, 30);
    s_aviso_txt = txt(s_scr, aviso ? aviso : "ESCRIBE LA CLAVE", aviso ? 0xFF9A9A : C_SUAVE, 1, 300, false);
    lv_obj_align(s_aviso_txt, LV_ALIGN_TOP_MID, 0, 54);
    lv_obj_t *caja = lv_obj_create(s_scr); /* el campo de la clave */
    lv_obj_remove_style_all(caja);
    lv_obj_set_size(caja, 300, 38);
    lv_obj_align(caja, LV_ALIGN_TOP_MID, -22, 74);
    lv_obj_set_style_bg_color(caja, lv_color_hex(0x0E0B1E), 0);
    lv_obj_set_style_bg_opa(caja, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(caja, 10, 0);
    lv_obj_set_style_border_color(caja, lv_color_hex(C_SUAVE), 0);
    lv_obj_set_style_border_width(caja, 2, 0);
    lv_obj_remove_flag(caja, LV_OBJ_FLAG_SCROLLABLE);
    s_campo = txt(caja, "_", 0xFFFFFF, 2, 280, true);
    lv_obj_align(s_campo, LV_ALIGN_TOP_MID, 0, 10);
    lv_obj_t *v = boton(s_scr, 44, 38, C_BOTON, "VER", 1, tecla_cb, T_VER, false); /* mostrar u ocultar la clave */
    lv_obj_align(v, LV_ALIGN_TOP_MID, 152, 74);
    s_teclas = lv_obj_create(s_scr);
    lv_obj_remove_style_all(s_teclas);
    lv_obj_set_size(s_teclas, 370, 210);
    lv_obj_align(s_teclas, LV_ALIGN_TOP_MID, 0, 124);
    lv_obj_remove_flag(s_teclas, LV_OBJ_FLAG_SCROLLABLE);
    teclado_armar();
    campo_refrescar();
    lv_obj_t *o = boton(s_scr, 120, 38, C_BOTON, "OTRA RED", 1, otra_cb, 0, false);
    lv_obj_align(o, LV_ALIGN_TOP_MID, -66, 340);
    lv_obj_t *c = boton(s_scr, 120, 38, C_ROJO, "CANCELAR", 1, cancelar_cb, 0, false);
    lv_obj_align(c, LV_ALIGN_TOP_MID, 66, 340);
}

/* --- hay una versión nueva: aviso con cuenta regresiva */
static void pantalla_aviso(int seg)
{
    limpiar();
    lv_obj_t *a = txt(s_scr, "¡HAY UNA VERSIÓN NUEVA!", C_AMARILLO, 3, 360, false);
    lv_obj_align(a, LV_ALIGN_TOP_MID, 0, 70);
    char t[160];
    snprintf(t, sizeof t, "TIENES LA %s\nLLEGA LA %s\n%s", FW_VERSION, s_nueva, s_nueva_nombre);
    lv_obj_t *b = txt(s_scr, t, 0xFFFFFF, 2, 340, false);
    lv_obj_align(b, LV_ALIGN_TOP_MID, 0, 150);
    snprintf(t, sizeof t, "SE ACTUALIZA EN %d S", seg);
    s_aviso_txt = txt(s_scr, t, C_SUAVE, 2, 340, false);
    lv_obj_align(s_aviso_txt, LV_ALIGN_TOP_MID, 0, 250);
    lv_obj_t *y = boton(s_scr, 200, 46, C_VERDE, "ACTUALIZAR YA", 2, ya_cb, 0, false);
    lv_obj_align(y, LV_ALIGN_TOP_MID, 0, 300);
    boton_cancelar(360, NULL);
}

/* --- bajando: barra y porcentaje */
static void pantalla_bajar(void)
{
    limpiar();
    char t[80];
    snprintf(t, sizeof t, "ACTUALIZANDO A LA %s", s_nueva);
    lv_obj_t *a = txt(s_scr, t, C_AMARILLO, 3, 360, false);
    lv_obj_align(a, LV_ALIGN_TOP_MID, 0, 120);
    lv_obj_t *b = txt(s_scr, "NO LO APAGUES. SE REINICIA SOLO", 0xFFFFFF, 2, 340, false);
    lv_obj_align_to(b, a, LV_ALIGN_OUT_BOTTOM_MID, 0, 14);
    s_barra = lv_obj_create(s_scr);
    lv_obj_remove_style_all(s_barra);
    lv_obj_set_size(s_barra, 300, 18);
    lv_obj_align(s_barra, LV_ALIGN_TOP_MID, 0, 262);
    lv_obj_set_style_bg_color(s_barra, lv_color_hex(0x433868), 0);
    lv_obj_set_style_bg_opa(s_barra, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(s_barra, 9, 0);
    s_barra_fill = lv_obj_create(s_barra);
    lv_obj_remove_style_all(s_barra_fill);
    lv_obj_set_size(s_barra_fill, 0, 18);
    lv_obj_set_style_bg_color(s_barra_fill, lv_color_hex(C_AMARILLO), 0);
    lv_obj_set_style_bg_opa(s_barra_fill, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(s_barra_fill, 9, 0);
    s_pct_txt = txt(s_scr, "0 %", C_SUAVE, 2, 200, false);
    lv_obj_align(s_pct_txt, LV_ALIGN_TOP_MID, 0, 294);
}
static void bajar_progreso(int pct)
{
    if (!s_disp || !s_barra_fill || !hal_display_lock()) return;
    lv_obj_set_width(s_barra_fill, 300 * (pct > 100 ? 100 : pct) / 100);
    char t[16];
    snprintf(t, sizeof t, "%d %%", pct);
    s_pct_txt = txt_cambiar(s_pct_txt, t, C_SUAVE, 2, 200, false);
    hal_display_unlock();
}

/* arma la pantalla que toca (desde la tarea principal, con el candado de la pantalla) */
static void mostrar(void (*f)(void))
{
    if (s_disp && hal_display_lock()) { f(); hal_display_unlock(); }
}
static const char *s_m1, *s_m2;
static bool s_m_otra, s_m_cancel;
static void f_msg(void) { pantalla_msg(s_m1, s_m2, s_m_otra, s_m_cancel); }
static void msg(const char *t1, const char *t2, bool otra, bool cancel)
{
    snprintf(s_msg1, sizeof s_msg1, "%s", t1 ? t1 : "");
    snprintf(s_msg2, sizeof s_msg2, "%s", t2 ? t2 : "");
    s_m1 = s_msg1; s_m2 = s_msg2; s_m_otra = otra; s_m_cancel = cancel;
    mostrar(f_msg);
}
static void f_lista(void) { pantalla_lista(); }
static const char *s_clave_aviso;
static void f_clave(void) { pantalla_clave(s_clave_aviso); }
static int s_aviso_seg;
static void f_aviso(void) { pantalla_aviso(s_aviso_seg); }
static void f_bajar(void) { pantalla_bajar(); }

