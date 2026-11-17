/* ======================================================================
 * (v98) Los lugares del Rey Peumo. Incluido desde ui.c, después de ui_ceremonias.inc.
 *  - El CLARO DEL REY y el MIRADOR ahora son zonas del mundo de verdad (ESC_CLARO, ESC_MIRADOR), vistas desde arriba
 *    como las demás: con sus animales, las visitas, el clima, las flechas y el mapa. Aquí está lo que tienen de especial:
 *      · el claro: la casa del rey (un tocón de peumo, como la casita del prado): al tocarla, ella va y entran; las
 *        luciérnagas (más de noche), el chucao (canta cuando hay algo escondido), la poza, el anillo de hongos, el tronco
 *        caído y el huerto. Se llega por el sendero de las luciérnagas del bosque (al oeste) o desde el mapa.
 *      · el mirador: la cima del cerro, justo sobre la cueva. La piedra tibia, el arbusto de la corona, la nube del
 *        último paseo (los tres juegos), el cóndor en su roca, la apacheta y la grieta por donde brilla la cueva. Se sube
 *        en la nube del cerro y se baja por el sendero del lomo (al sur).
 *  - La CASITA (adentro de la casa del rey) sigue siendo una pantalla propia: el horno y la camita (los juegos), el
 *    gancho, la mesa y las cositas que reaccionan. Se sale con la X, al claro.
 *  - Los juegos (ui_zjuegos.inc) se abren en su propia pantalla desde la casita o desde el mirador.
 * ==================================================================== */
enum { ZO_NADA = 0, ZO_CLARO, ZO_CASITA, ZO_MIRADOR };
enum { ZJ_NADA = 0, ZJ_HORNO, ZJ_CAMITA, ZJ_NUBE, ZJ_CORONA, ZJ_PIEDRA };
static int s_zo, s_zo_pend = -1, s_zo_cosa = -1, s_zo_pose = -1;
static uint32_t s_zo_t0, s_zo_txt_hasta, s_zo_quieta;
static float s_zo_x, s_zo_y, s_zo_tx, s_zo_ty;
static bool s_zo_esp;
static lv_obj_t *s_zo_txt_img, *s_zo_vivo[4]; /* lo que se mueve en el fondo de la casita: el fuego del horno, la lamparita */
static int s_zj_pant = -1; /* la pantalla del juego que está abierto (o -1) */
#define ZO_PANT_CASITA 5
static lv_obj_t *s_zo_capa, *s_zo_fondo, *s_zo_pet, *s_zo_txt, *s_zo_salir, *s_zo_marca[4];
static EXT_RAM_BSS_ATTR art_img_t s_zo_art[14], s_zo_txt_art; /* 0..6 las poses (6: el otro paso al caminar), y +7 mirando al otro lado */
static bool s_esc_hay; /* hay algo escondido hoy en la casita */
static void esc_anunciar(int zz); /* (101b) la hoja que avisa lo escondido de hoy */
/* dónde se camina en la casita */
#define ZO_SUELO_MIN 336
#define ZO_SUELO_MAX 410
/* las cosas de la casita: x, y, radio (el horno, la camita, el gancho, la mesa) */
static const int16_t ZO_COSAS[4][3] = {{100, 248, 56}, {365, 318, 58}, {326, 196, 40}, {220, 342, 52}};

/* ella en la casita */
static void zo_pose(int f)
{
    if (f == s_zo_pose || !s_pet) return;
    s_zo_pose = f;
    const int k = (f & 7) % 7, esp = (f >> 3) & 1, slot = k + esp * 7;
    if (!s_zo_art[slot].dsc.data) {
        const pet_t *m = s_pet;
        /* 0 de pie, 1 y 6 caminando (los dos pasos), 2 sentada, 3 ojos cerrados (duerme), 4 feliz con la boca abierta, 5 curiosa */
        static const art_expresion_t EX[7] = {EXP_FELIZ, EXP_FELIZ, EXP_FELIZ, EXP_CERRADOS, EXP_FELIZ, EXP_CURIOSO, EXP_FELIZ};
        art_pose_t po = {pet_look(m), m->etapa, m->etapa == STAGE_ADULTA ? m->forma : 0, 4, EX[k], k == 4, k == 6 ? 1 : 0, esp != 0, false,
                         k == 5 ? 1 : 0, k == 4 ? 1 : 0, k == 2 ? -1 : (k == 3 ? -2 : 0), {0, 0, 0, 0}, 0, m->guardiana};
        for (int r = 0; r < RANURA_COUNT; r++) po.ropa[r] = pet_worn_raw(m, r);
        art_criatura_pose(&s_zo_art[slot], &po);
    }
    poner_src(s_zo_pet, &s_zo_art[slot].dsc);
}
static void zo_texto(const char *t, uint32_t ms)
{
    if (!s_zo_txt) return;
    if (!t || s_nino) { ocultar(s_zo_txt, true); return; }
    char b[300];
    snprintf(b, sizeof b, "%s", gtxt(t));
    mayusculas(b);
    art_texto(&s_zo_txt_art, b, 0xFFF3C8, 2, 290);
    poner_src(s_zo_txt_img, &s_zo_txt_art.dsc);
    lv_obj_update_layout(s_zo_txt);
    lv_obj_align(s_zo_txt, LV_ALIGN_TOP_MID, 0, 74);
    ocultar(s_zo_txt, false);
    lv_obj_move_foreground(s_zo_txt);
    s_zo_txt_hasta = s_ms + (ms ? ms : 4000u + (uint32_t)strlen(b) * 70u);
}
static void zo_ir(float x, float y)
{
    if (y < ZO_SUELO_MIN) y = ZO_SUELO_MIN;
    if (y > ZO_SUELO_MAX) y = ZO_SUELO_MAX;
    const int w = zona_ancho((int)y) / 2 - 36;
    if (x < CX - w) x = (float)(CX - w);
    if (x > CX + w) x = (float)(CX + w);
    s_zo_tx = x; s_zo_ty = y;
}

static void zona_entrar(int z);
static void zona_salir(void);
static void zo_salir_cb(lv_event_t *e);
/* (la casita va encima de todo, como los cristales: sus íconos flotantes son propios) */
#define ZO_FLOT 8
static lv_obj_t *s_zo_fl[ZO_FLOT];
static uint32_t s_zo_fl_t[ZO_FLOT];
static int16_t s_zo_fl_x[ZO_FLOT], s_zo_fl_y[ZO_FLOT];
static void zo_flota(int ico, const char *txt, uint32_t color, int x, int y, uint32_t retraso)
{
    (void)txt; (void)color;
    if (!s_zo_capa || ico < 0) return;
    int k = 0;
    for (int i = 0; i < ZO_FLOT; i++) { if (!s_zo_fl[i] || lv_obj_has_flag(s_zo_fl[i], LV_OBJ_FLAG_HIDDEN)) { k = i; break; } if (s_zo_fl_t[i] < s_zo_fl_t[k]) k = i; }
    if (!s_zo_fl[k]) { s_zo_fl[k] = lv_image_create(s_zo_capa); lv_obj_remove_flag(s_zo_fl[k], LV_OBJ_FLAG_CLICKABLE); }
    lv_image_set_src(s_zo_fl[k], art_icono((art_icono_t)ico, 3));
    s_zo_fl_t[k] = s_ms + retraso; s_zo_fl_x[k] = (int16_t)x; s_zo_fl_y[k] = (int16_t)y;
    ocultar(s_zo_fl[k], retraso > 0);
    lv_obj_set_pos(s_zo_fl[k], x - 18, y - 18);
    lv_obj_move_foreground(s_zo_fl[k]);
}
static void zo_flota_animar(void)
{
    for (int i = 0; i < ZO_FLOT; i++) {
        if (!s_zo_fl[i]) continue;
        const int32_t d = (int32_t)(s_ms - s_zo_fl_t[i]);
        if (d < 0) continue;
        if (d > 1300) { ocultar(s_zo_fl[i], true); continue; }
        ocultar(s_zo_fl[i], false);
        lv_obj_set_pos(s_zo_fl[i], s_zo_fl_x[i] - 18, s_zo_fl_y[i] - 18 - d * 50 / 1300);
        lv_obj_set_style_image_opa(s_zo_fl[i], (lv_opa_t)(d < 700 ? 255 : (1300 - d) * 255 / 600), 0);
    }
}
static void zo_suelo_cb(lv_event_t *e);

/* las cosas de la casita que no son juegos (el gancho, la mesa): el motor las anota la primera vez */
static void zo_encontro(int c)
{
    if (!s_pet) return;
    const bool nueva = !pet_secret_found(s_pet, 0, c);
    accion(UI_ACCION_LUGAR, 0 * 16 + c);
    accion(UI_ACCION_SONAR, nueva ? SND_TESORO : SND_TOQUE);
    if (s_zo_marca[c]) ocultar(s_zo_marca[c], true);
    char t[400];
    snprintf(t, sizeof t, "%s: %s", pet_secret_title(0, c), pet_secret_text(0, c, modo_nino_lee() || s_nino));
    zo_texto(t, 0);
}
static void zo_fondo_poner(void);
#include "ui_zjuegos.inc"

/* ---------- lo escondido de hoy en la casita: brilla un poquito; tocarlo: ella va y lo saca ---------- */
static lv_obj_t *s_zo_esc, *s_zo_esc_toque;
static int s_zo_esc_k;
static const int16_t ZO_ESCONDITE[3][2] = {{165, 394}, {290, 402}, {392, 382}}; /* bajo la alfombra, junto a la mesa, al pie de la camita */
static bool zo_noche(void) { return s_fondo_periodo == PERIODO_NOCHE; }
static void zo_esc_cb(lv_event_t *e)
{
    (void)e;
    if (!s_zo || zj_activo() || !s_esc_hay) return;
    s_zo_cosa = 9;
    zo_ir(ZO_ESCONDITE[s_zo_esc_k][0], ZO_ESCONDITE[s_zo_esc_k][1]);
}
static void zo_esc_poner(void)
{
    s_esc_hay = s_pet && pet_zone_find(s_pet, 1, s_ultimo_t) != PET_ZH_NADA;
    s_zo_esc_k = s_pet ? pet_zone_find_spot(s_pet, 1, s_ultimo_t) : 0;
    if (!s_zo_esc) {
        s_zo_esc = lv_image_create(s_zo_capa);
        lv_image_set_src(s_zo_esc, art_icono(ICO_ESTRELLA, 2));
        lv_obj_remove_flag(s_zo_esc, LV_OBJ_FLAG_CLICKABLE);
        s_zo_esc_toque = caja(s_zo_capa);
        lv_obj_set_size(s_zo_esc_toque, 64, 64);
        lv_obj_set_style_bg_opa(s_zo_esc_toque, 0, 0);
        lv_obj_add_flag(s_zo_esc_toque, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(s_zo_esc_toque, zo_esc_cb, LV_EVENT_CLICKED, NULL);
    }
    const int ex = ZO_ESCONDITE[s_zo_esc_k][0], ey = ZO_ESCONDITE[s_zo_esc_k][1];
    lv_obj_set_pos(s_zo_esc, ex - 12, ey - 20);
    lv_obj_set_pos(s_zo_esc_toque, ex - 32, ey - 44);
    ocultar(s_zo_esc, !s_esc_hay); ocultar(s_zo_esc_toque, !s_esc_hay);
    lv_obj_move_foreground(s_zo_esc); lv_obj_move_foreground(s_zo_esc_toque);
}
static void zo_sacar(void) /* llegó a lo escondido */
{
    if (!s_pet || !s_esc_hay) return;
    const int k = pet_zone_find(s_pet, 1, s_ultimo_t);
    s_esc_hay = false;
    ocultar(s_zo_esc, true); ocultar(s_zo_esc_toque, true);
    if (k == PET_ZH_NADA) return;
    accion(UI_ACCION_ZONA_HALLAZGO, 1);
    zo_pose(4); s_zo_quieta = s_ms + 2400;
    if (k == PET_ZH_CARTA) return; /* (fw 104) un papelito: lo muestra el aviso del motor (se abre a pantalla completa) */
    const art_icono_t ico = k == PET_ZH_MUSGO ? ICO_MUSGO : (k == PET_ZH_RAMITAS ? ICO_RAMITA : ICO_REGALO);
    zo_flota(ico, NULL, 0, (int)s_zo_x, (int)s_zo_y - 90, 0);
    if (k == PET_ZH_RAMITAS) zo_flota(ICO_RAMITA, NULL, 0, (int)s_zo_x + 22, (int)s_zo_y - 80, 250);
    static const char *const T[4] = {"", "¡MUSGO! PEUMO LO GUARDABA PARA SU CAMITA", "¡RAMITAS PARA EL HORNO! QUEDABAN DE PEUMO", "¡UN REGALITO! PEUMO LO DEJÓ GUARDADO AQUÍ"};
    zo_texto(s_nino ? NULL : T[k & 3], 3200);
}

/* ---------- tocar en la casita ---------- */
static void zo_hacer(int c) /* llegó a la cosa c */
{
    if (c == 9) { zo_sacar(); return; }
    if (c == 0) zj_abrir(ZJ_HORNO);
    else if (c == 1) zj_abrir(ZJ_CAMITA);
    else zo_encontro(c);
}
static void zo_cosa_cb(lv_event_t *e)
{
    const int c = (int)(intptr_t)lv_event_get_user_data(e);
    if (!s_zo || zj_activo()) return;
    s_zo_cosa = c;
    zo_ir(ZO_COSAS[c][0], ZO_COSAS[c][1] + 30);
}
/* las cositas de la casita que reaccionan al tocarlas (no son secretos: solo para jugar) */
typedef struct { int16_t x, y, r; uint8_t tipo; } zo_deco_t;
enum { ZD_FRASCOS = 1, ZD_VENTANA, ZD_HIERBAS, ZD_CUADRO, ZD_LAMPARA };
static const zo_deco_t ZO_DECO[6] = {{96, 104, 46, ZD_FRASCOS}, {233, 92, 44, ZD_VENTANA}, {376, 52, 40, ZD_HIERBAS}, {410, 145, 30, ZD_CUADRO}, {444, 210, 22, ZD_LAMPARA}, {0}};
static uint32_t s_zd_ult;
static bool zo_deco_toque(int x, int y)
{
    if (!s_zo) return false;
    for (int k = 0; k < 6 && ZO_DECO[k].tipo; k++) {
        const zo_deco_t *d = &ZO_DECO[k];
        if (hypotf((float)(x - d->x), (float)(y - d->y)) > (float)d->r) continue;
        if (s_ms - s_zd_ult < 600u) return true; /* (un toque a la vez) */
        s_zd_ult = s_ms;
        s_zo_esp = d->x < s_zo_x; zo_pose(5 | (s_zo_esp ? 8 : 0)); s_zo_quieta = s_ms + 1800; /* ella mira hacia allá, curiosa */
        const int v = (int)(azar() % 3);
        switch (d->tipo) {
        case ZD_FRASCOS: {
            static const char *const T[3] = {"FRASCOS DE PIÑONES, DE MIEL DE ULMO... Y UNO CON PURPURINA", "¡CLIN, CLIN! SUENAN COMO CRISTALITOS", "UNO DICE «PARA LAS GALLETAS». ESTÁ VACÍO"};
            accion(UI_ACCION_SONAR, (int)SND_CRISTAL_AZUL + v); zo_flota(ICO_NOTA, NULL, 0, d->x, d->y - 30, 0); zo_texto(T[v], 2600); break; }
        case ZD_VENTANA: zo_flota(ICO_ESTRELLA, NULL, 0, d->x, d->y, 0); accion(UI_ACCION_SONAR, SND_HADAS);
            zo_texto(zo_noche() ? "AFUERA, LAS LUCIÉRNAGAS DEL CLARO SE ASOMAN A MIRAR" : "AFUERA SE VE EL CLARO Y LA POZA, LLENOS DE LUZ", 2600); break;
        case ZD_HIERBAS: zo_flota(ICO_HOJA, NULL, 0, d->x, d->y + 10, 0); accion(UI_ACCION_SONAR, SND_HUELE);
            zo_texto(v == 0 ? "HUELE A MENTA Y A ROMERO" : (v == 1 ? "HIERBAS SECÁNDOSE. PEUMO HACÍA TECITOS" : "¡ACHÚ! ¡HUELEN MUY FUERTE!"), 2400); break;
        case ZD_CUADRO: zo_flota(ICO_CORAZON, NULL, 0, d->x, d->y - 20, 0); accion(UI_ACCION_SONAR, SND_SUSPIRO_AMOR);
            zo_texto("UN DIBUJO DE TRES CRIATURAS Y UN ARCOÍRIS. ¿LO HIZO ÉL?", 2800); break;
        case ZD_LAMPARA: zo_flota(ICO_BOMBILLA, NULL, 0, d->x - 20, d->y, 0); accion(UI_ACCION_SONAR, SND_TOQUE);
            zo_texto("LA LAMPARITA DE PEUMO. TODAVÍA ALUMBRA", 2200); break;
        default: break;
        }
        return true;
    }
    return false;
}
static void zo_suelo_cb(lv_event_t *e)
{
    (void)e;
    if (!s_zo) return;
    lv_point_t pt = {0, 0};
    lv_indev_t *in = lv_indev_active();
    if (in) lv_indev_get_point(in, &pt);
    if (s_zo_txt && !lv_obj_has_flag(s_zo_txt, LV_OBJ_FLAG_HIDDEN) && !zj_activo()) { /* tocar el texto lo cierra */
        lv_area_t a; lv_obj_get_coords(s_zo_txt, &a);
        if (pt.x >= a.x1 && pt.x <= a.x2 && pt.y >= a.y1 && pt.y <= a.y2) { ocultar(s_zo_txt, true); return; }
    }
    if (zj_activo()) return;
    if (zo_deco_toque(pt.x, pt.y)) return;
    s_zo_cosa = -1;
    zo_ir((float)pt.x, (float)pt.y + 16);
}
static void zo_salir_cb(lv_event_t *e) { (void)e; zona_salir(); } /* la X: por la puerta, al claro */

/* ---------- entrar y salir de la casita ---------- */
static bool zona_usa_pantalla(int id);
static void zo_crear(void)
{
    if (s_zo_capa) return;
    art_pantalla_en_uso_fn(zona_usa_pantalla);
    s_zo_capa = caja(lv_layer_top());
    lv_obj_set_size(s_zo_capa, W, W);
    lv_obj_set_style_bg_color(s_zo_capa, lv_color_hex(0x10161A), 0);
    lv_obj_set_style_bg_opa(s_zo_capa, LV_OPA_COVER, 0);
    lv_obj_add_flag(s_zo_capa, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_zo_capa, zo_suelo_cb, LV_EVENT_CLICKED, NULL);
    s_zo_fondo = lv_image_create(s_zo_capa);
    lv_obj_remove_flag(s_zo_fondo, LV_OBJ_FLAG_CLICKABLE);
    for (int c = 0; c < 4; c++) {
        s_zo_marca[c] = lv_image_create(s_zo_capa);
        lv_image_set_src(s_zo_marca[c], art_icono(ICO_ESTRELLA, 2));
        lv_obj_remove_flag(s_zo_marca[c], LV_OBJ_FLAG_CLICKABLE);
    }
    s_zo_pet = lv_image_create(s_zo_capa);
    lv_obj_remove_flag(s_zo_pet, LV_OBJ_FLAG_CLICKABLE);
    s_zo_txt = caja(s_zo_capa); /* el texto va en una cajita con su fondo */
    lv_obj_set_size(s_zo_txt, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(s_zo_txt, lv_color_hex(0x1A1430), 0);
    lv_obj_set_style_bg_opa(s_zo_txt, LV_OPA_80, 0);
    lv_obj_set_style_pad_hor(s_zo_txt, 12, 0);
    lv_obj_set_style_pad_ver(s_zo_txt, 8, 0);
    lv_obj_set_style_radius(s_zo_txt, 14, 0);
    lv_obj_set_style_border_width(s_zo_txt, 2, 0);
    lv_obj_set_style_border_color(s_zo_txt, lv_color_hex(0xFFE9A0), 0);
    lv_obj_set_style_border_opa(s_zo_txt, LV_OPA_50, 0);
    s_zo_txt_img = lv_image_create(s_zo_txt);
    lv_obj_remove_flag(s_zo_txt_img, LV_OBJ_FLAG_CLICKABLE);
    /* la salida como en los demás juegos: la X redonda abajo (sale por la puerta, al claro) */
    s_zo_salir = boton(s_zo_capa, BOTON_ABAJO, BOTON_ABAJO, zo_salir_cb, 0);
    lv_obj_set_ext_click_area(s_zo_salir, 14);
    lv_obj_center(icono(s_zo_salir, ICO_NO, 2));
    lv_obj_set_pos(s_zo_salir, CX - BOTON_ABAJO / 2, BOTON_ABAJO_Y);
    ocultar(s_zo_capa, true);
}
static lv_obj_t *s_zo_toque[4];
static void zo_fondo_poner(void)
{
    if (!s_zo) return;
    int px = 0, py = 0;
    poner_src(s_zo_fondo, art_pantalla(ZO_PANT_CASITA, &px, &py));
    lv_obj_set_pos(s_zo_fondo, px, py);
}
static void lw_esconder(void);
static void lw_ir(int escena);
static void zona_entrar(int z)
{
    if (!s_pet) return;
    if (z != ZO_CASITA) { /* (v98) el claro y el mirador son zonas del mundo: ir allá */
        if (s_zo) zona_salir();
        lw_ir(z == ZO_MIRADOR ? ESC_MIRADOR : ESC_CLARO);
        return;
    }
    zo_crear();
    if (zj_activo()) { s_zj_sin_fondo = true; zj_cerrar(false); s_zj_sin_fondo = false; }
    for (int c = 0; c < 4; c++) if (s_zo_toque[c]) { lv_obj_delete(s_zo_toque[c]); s_zo_toque[c] = NULL; }
    s_zo = ZO_CASITA;
    s_zo_t0 = s_ms;
    s_zo_cosa = -1;
    s_zo_pose = -1;
    for (int i = 0; i < 14; i++) art_img_free(&s_zo_art[i]);
    zo_fondo_poner();
    /* las cosas que se tocan (101b: sin estrellita encima, que delataba qué tocar; descubrirlas es parte del juego) */
    for (int c = 0; c < 4; c++) {
        const int r = ZO_COSAS[c][2];
        ocultar(s_zo_marca[c], true);
        lv_obj_t *b = caja(s_zo_capa);
        lv_obj_set_size(b, r * 2, r * 2);
        lv_obj_set_pos(b, ZO_COSAS[c][0] - r, ZO_COSAS[c][1] - r);
        lv_obj_set_style_bg_opa(b, 0, 0);
        lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(b, zo_cosa_cb, LV_EVENT_CLICKED, (void *)(intptr_t)c);
        s_zo_toque[c] = b;
        if (false && !pet_secret_found(s_pet, 0, c)) { /* (101b) apagado a pedido */
            lv_obj_set_pos(s_zo_marca[c], ZO_COSAS[c][0] - 12, ZO_COSAS[c][1] - r / 2 - 12);
            ocultar(s_zo_marca[c], false);
            lv_obj_move_foreground(s_zo_marca[c]);
        }
    }
    for (int k = 0; k < 4; k++) if (s_zo_vivo[k]) { lv_obj_delete(s_zo_vivo[k]); s_zo_vivo[k] = NULL; }
    static const int16_t LV[2][3] = {{100, 258, 70}, {444, 206, 34}}; /* el fuego del horno y la lamparita */
    for (int k = 0; k < 2; k++) {
        lv_obj_t *o = caja(s_zo_capa);
        lv_obj_set_size(o, LV[k][2], LV[k][2] * 3 / 4);
        lv_obj_set_pos(o, LV[k][0] - LV[k][2] / 2, LV[k][1] - LV[k][2] * 3 / 8);
        lv_obj_set_style_radius(o, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(o, lv_color_hex(k ? 0xFFD27A : 0xFF9A3A), 0);
        lv_obj_set_style_bg_opa(o, LV_OPA_30, 0);
        lv_obj_set_style_blend_mode(o, LV_BLEND_MODE_ADDITIVE, 0);
        s_zo_vivo[k] = o;
    }
    lv_obj_move_background(s_zo_fondo);
    for (int k = 0; k < 4; k++) if (s_zo_vivo[k]) lv_obj_move_to_index(s_zo_vivo[k], 1);
    s_zo_x = 150; s_zo_y = 392; /* entra por la puerta */
    s_zo_tx = s_zo_x; s_zo_ty = s_zo_y;
    s_zo_quieta = 0;
    ocultar(s_zo_txt, true);
    ocultar(s_zo_capa, false);
    lv_obj_move_foreground(s_zo_capa);
    accion(UI_ACCION_EN_ZONA, 1); /* (no se va sola a otra zona mientras están aquí) */
    lv_obj_add_flag(s_fondo, LV_OBJ_FLAG_HIDDEN); lv_obj_add_flag(s_mundo, LV_OBJ_FLAG_HIDDEN); /* el mundo no se pinta debajo */
    if (s_globo) { ocultar(s_globo, true); ocultar(s_globo_cola, true); }
    zo_esc_poner();
    lv_obj_move_foreground(s_zo_pet); lv_obj_move_foreground(s_zo_txt); lv_obj_move_foreground(s_zo_salir);
    zo_pose(0);
    accion(UI_ACCION_SONAR, SND_TOQUE);
    zo_texto(s_nino ? NULL : pet_secret_place_name(0), 2600);
    esc_anunciar(1); /* (101b) lo escondido de hoy en la casa del rey */
}
static void zona_salir(void)
{
    if (!s_zo) return;
    if (zj_activo()) { s_zj_sin_fondo = true; zj_cerrar(false); s_zj_sin_fondo = false; }
    s_zo = ZO_NADA;
    accion(UI_ACCION_EN_ZONA, 0);
    for (int i = 0; i < ZO_FLOT; i++) if (s_zo_fl[i]) ocultar(s_zo_fl[i], true);
    for (int k = 0; k < 4; k++) if (s_zo_vivo[k]) { lv_obj_delete(s_zo_vivo[k]); s_zo_vivo[k] = NULL; }
    ocultar(s_zo_capa, true);
    poner_src(s_zo_fondo, NULL);
    poner_src(s_zo_pet, NULL);
    for (int i = 0; i < 14; i++) art_img_free(&s_zo_art[i]);
    art_pantalla_liberar(ZO_PANT_CASITA);
    lv_obj_remove_flag(s_fondo, LV_OBJ_FLAG_HIDDEN); lv_obj_remove_flag(s_mundo, LV_OBJ_FLAG_HIDDEN);
    if (s_pet) {
        s_fondo_periodo = -1; refrescar_fondo(s_pet, s_ultimo_t);
        if (s_escena == ESC_CLARO && !s_pet->dormida) { /* sale por la puerta de la casa del rey */
            s_mascota.x = s_mascota.tx = (float)ART_LUGAR_XY[ESC_CLARO][LUGAR_CASITA][0];
            s_mascota.y = s_mascota.ty = (float)ART_LUGAR_XY[ESC_CLARO][LUGAR_CASITA][1] + 8.0f;
            mascota_modo(M_LIBRE, 0);
        }
    }
    lv_obj_invalidate(lv_screen_active());
}
static bool zona_activa(void) { return s_zo != ZO_NADA; }
/* ¿esta pantalla está de fondo ahora (la casita o un juego)? entonces no se suelta */
static bool zona_usa_pantalla(int id) { return (s_zo && id == ZO_PANT_CASITA) || (zj_activo() && id == s_zj_pant); }
static void zona_tras_panel(void) { if (s_zo && s_zo_capa && !zj_activo()) ocultar(s_zo_capa, false); }
static void zona_bajo_panel(void) { if (s_zo && s_zo_capa) ocultar(s_zo_capa, true); if (s_zj && zj_activo()) ocultar(s_zj, true); }
/* (ui_zjuegos.inc) mientras se juega, lo de abajo (la casita o el mundo) se aparta para no gastar memoria ni cuadros */
static void lw_refrescar(void);
static void zj_mundo_ver(bool si)
{
    if (s_zo) {
        if (!si) { poner_src(s_zo_fondo, NULL); art_pantalla_liberar(ZO_PANT_CASITA); ocultar(s_zo_capa, true); }
        else { zo_fondo_poner(); ocultar(s_zo_capa, s_panel != PANEL_NINGUNO); zo_pose(4); s_zo_quieta = s_ms + 1800; }
        return;
    }
    if (!si) { lv_obj_add_flag(s_fondo, LV_OBJ_FLAG_HIDDEN); lv_obj_add_flag(s_mundo, LV_OBJ_FLAG_HIDDEN); if (s_globo) { ocultar(s_globo, true); ocultar(s_globo_cola, true); } }
    else {
        lv_obj_remove_flag(s_fondo, LV_OBJ_FLAG_HIDDEN); lv_obj_remove_flag(s_mundo, LV_OBJ_FLAG_HIDDEN);
        if (s_pet) { s_fondo_periodo = -1; refrescar_fondo(s_pet, s_ultimo_t); mascota_modo(M_SALTA, 900); }
        lw_refrescar(); /* (las estrellitas de los juegos que ya probó se apagan) */
        lv_obj_invalidate(lv_screen_active());
    }
}

/* ---------- la casita, cada cuadro ---------- */
static void animar_zona(void)
{
    if (zj_activo()) { /* jugando en la pantalla aparte: un panel encima aparta el juego */
        const bool tapa = s_panel != PANEL_NINGUNO;
        if (tapa != lv_obj_has_flag(s_zj, LV_OBJ_FLAG_HIDDEN)) ocultar(s_zj, tapa);
        static bool pausado;
        if (s_zj_timer && tapa != pausado) { pausado = tapa; if (tapa) lv_timer_pause(s_zj_timer); else { lv_timer_resume(s_zj_timer); s_zj_ult = 0; } }
        if (!s_zo && s_pet && (s_pet->dormida || s_cie_activo || s_via_activo || s_cru_activo)) zj_cerrar(false);
        if (s_zo) return;
    }
    if (!s_zo) return;
    if (!s_pet || s_pet->dormida || s_cie_activo || s_via_activo || s_cru_activo || s_sim_on || s_pet->escena != ESC_CLARO) { zona_salir(); return; } /* se durmió o se fue a otra zona */
    if (s_zo_txt && !lv_obj_has_flag(s_zo_txt, LV_OBJ_FLAG_HIDDEN) && (int32_t)(s_ms - s_zo_txt_hasta) > 0) ocultar(s_zo_txt, true);
    zo_flota_animar();
    { const bool tapa = s_panel != PANEL_NINGUNO; /* un panel encima (una carta, una página del diario): la casita se aparta y vuelve al cerrarlo */
      if (tapa != lv_obj_has_flag(s_zo_capa, LV_OBJ_FLAG_HIDDEN)) ocultar(s_zo_capa, tapa);
      if (tapa) return; }
    for (int k = 0; k < 2; k++) if (s_zo_vivo[k]) { /* el fuego titila; la lamparita, apenas */
        const float a = (float)s_ms / (k ? 380.0f : 150.0f);
        lv_obj_set_style_bg_opa(s_zo_vivo[k], (lv_opa_t)((k ? 60 : 70) + (k ? 15 : 35) * sinf(a) + (k ? 0 : 15) * sinf(a * 2.3f)), 0);
    }
    if (s_zo_esc && s_esc_hay) {
        const float a = (float)s_ms / 260.0f;
        lv_obj_set_style_image_opa(s_zo_esc, (lv_opa_t)(150 + 100 * sinf(a)), 0);
        lv_image_set_scale(s_zo_esc, (uint32_t)(220 + 50 * sinf(a * 0.7f)));
    }
    /* ella camina hasta donde tocaste */
    const float dx = s_zo_tx - s_zo_x, dy = s_zo_ty - s_zo_y, d = hypotf(dx, dy);
    int pose = 0;
    if (d > 2.0f) {
        const float v = 5.0f;
        s_zo_x += d < v ? dx : dx / d * v;
        s_zo_y += d < v ? dy : dy / d * v;
        if (fabsf(dx) > 1.0f) s_zo_esp = dx < 0;
        pose = 1;
        if (hypotf(s_zo_tx - s_zo_x, s_zo_ty - s_zo_y) <= 2.0f) { /* llegó */
            s_zo_x = s_zo_tx; s_zo_y = s_zo_ty;
            if (s_zo_cosa >= 0) { const int c = s_zo_cosa; s_zo_cosa = -1; zo_hacer(c); if (!s_zo || zj_activo()) return; }
        }
    } else if ((int32_t)(s_ms - s_zo_quieta) < 0) pose = s_zo_pose & 7;
    else pose = (s_ms / 3000) % 4 == 3 ? 5 : 0;
    if (pose == 1 && (s_ms / 160) % 2) pose = 6; /* dos pasos: cada uno se dibuja una vez y se alternan */
    zo_pose(pose | (s_zo_esp ? 8 : 0));
    const lv_image_dsc_t *im = lv_image_get_src(s_zo_pet);
    const int w = im ? (int)im->header.w : 60, h = im ? (int)im->header.h : 60;
    const int bote = (pose & 7) == 4 ? -(int)fabsf(sinf((float)s_ms / 130.0f) * 6) : 0;
    lv_obj_set_pos(s_zo_pet, (int)s_zo_x - w / 2, (int)s_zo_y - h + bote);
}

/* ======================================================================
 * (v98) El claro y el mirador en el mundo: lo que se toca, lo que vive ahí y cómo se llega
 * ==================================================================== */
#define LW_COSAS 6
typedef struct { int16_t x, y, r, ix, iy; } lw_cosa_t; /* dónde se toca (x, y, radio) y adónde camina ella (ix, iy) */
/* claro: 0 la casa del rey, 1 la poza, 2 el anillo de hongos, 3 el tronco caído, 4 el huerto */
static const lw_cosa_t LW_CLARO[LW_COSAS] = {{205, 120, 62, 207, 194}, {339, 204, 42, 300, 250}, {157, 318, 32, 170, 340}, {242, 376, 34, 242, 360}, {118, 196, 28, 130, 214}, {0}};
/* mirador: 0 la piedra tibia, 1 el arbusto de la corona, 2 la nube, 3 el cóndor, 4 la apacheta, 5 la grieta de la cueva */
static const lw_cosa_t LW_MIRADOR[LW_COSAS] = {{164, 222, 34, 176, 252}, {311, 268, 32, 296, 294}, {356, 186, 40, 330, 232}, {222, 100, 34, 224, 180}, {295, 136, 28, 286, 180}, {140, 278, 22, 156, 296}};
static const int16_t LW_ESCONDITE[2][3][2] = {
    {{157, 322}, {255, 362}, {290, 246}},   /* claro: entre los hongos, junto al tronco, al lado de la poza */
    {{200, 300}, {285, 190}, {250, 318}},   /* mirador: entre las llaretas, junto a la apacheta, en el pasto */
};
static lv_obj_t *s_lw_toque[LW_COSAS], *s_lw_marca[3], *s_lw_brillo, *s_lw_luci[8], *s_lw_vec, *s_lw_esc, *s_lw_nube;
static art_img_t s_lw_vec_art[2];
static int s_lw_esc_en = -1, s_lw_vec_ico = -1, s_lw_vec_cuadro = -1, s_lw_pend = -1, s_lw_esc_k;
static bool s_lw_esc_hay;
static uint32_t s_lw_pend_t, s_lw_vec_canta, s_lw_vuela_t, s_lw_aviso, s_lw_zd_ult, s_lw_llego;
static float s_lw_vec_x, s_lw_vec_y;
static int lw_aqui(void) { return !s_pet || s_interior || s_zo || s_escena < 0 ? -1 : (s_escena == ESC_CLARO ? 0 : (s_escena == ESC_MIRADOR ? 1 : -1)); }
static const lw_cosa_t *lw_cosas(int z) { return z == 0 ? LW_CLARO : LW_MIRADOR; }
static void lw_cosa_cb(lv_event_t *e);
static void lw_vec_cb(lv_event_t *e);
static void lw_esc_cb(lv_event_t *e);
static void lw_crear(void)
{
    if (s_lw_brillo) return;
    s_lw_brillo = caja(s_mundo); /* la luz de la puerta de la casa del rey (como la entrada secreta del bosque) */
    lv_obj_set_style_radius(s_lw_brillo, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(s_lw_brillo, lv_color_hex(0xFFF3B0), 0);
    lv_obj_set_style_bg_opa(s_lw_brillo, LV_OPA_60, 0);
    lv_obj_set_style_blend_mode(s_lw_brillo, LV_BLEND_MODE_ADDITIVE, 0);
    lv_obj_remove_flag(s_lw_brillo, LV_OBJ_FLAG_CLICKABLE);
    for (int i = 0; i < 8; i++) {
        s_lw_luci[i] = caja(s_mundo);
        lv_obj_set_size(s_lw_luci[i], 6, 6);
        lv_obj_set_style_radius(s_lw_luci[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(s_lw_luci[i], lv_color_hex(i & 1 ? 0xD8FF8A : 0xFFE27A), 0);
        lv_obj_set_style_bg_opa(s_lw_luci[i], LV_OPA_COVER, 0);
        lv_obj_set_style_shadow_width(s_lw_luci[i], 8, 0);
        lv_obj_set_style_shadow_color(s_lw_luci[i], lv_color_hex(0xE8FF9A), 0);
        lv_obj_remove_flag(s_lw_luci[i], LV_OBJ_FLAG_CLICKABLE);
    }
    s_lw_nube = lv_image_create(s_mundo); /* la nube del último paseo, en el mirador */
    lv_image_set_src(s_lw_nube, art_icono(ICO_NUBE, 5));
    lv_obj_remove_flag(s_lw_nube, LV_OBJ_FLAG_CLICKABLE);
    for (int c = 0; c < 3; c++) {
        s_lw_marca[c] = lv_image_create(s_mundo);
        lv_image_set_src(s_lw_marca[c], art_icono(ICO_ESTRELLA, 2));
        lv_obj_remove_flag(s_lw_marca[c], LV_OBJ_FLAG_CLICKABLE);
    }
    s_lw_esc = lv_image_create(s_mundo);
    lv_image_set_src(s_lw_esc, art_icono(ICO_ESTRELLA, 2));
    lv_obj_add_flag(s_lw_esc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(s_lw_esc, 22);
    lv_obj_add_event_cb(s_lw_esc, lw_esc_cb, LV_EVENT_CLICKED, NULL);
    for (int c = 0; c < LW_COSAS; c++) {
        s_lw_toque[c] = caja(s_mundo);
        lv_obj_set_style_bg_opa(s_lw_toque[c], 0, 0);
        lv_obj_add_flag(s_lw_toque[c], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(s_lw_toque[c], lw_cosa_cb, LV_EVENT_CLICKED, (void *)(intptr_t)c);
    }
    s_lw_vec = lv_image_create(s_mundo); /* el chucao (claro) o el cóndor (mirador) */
    lv_obj_add_flag(s_lw_vec, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(s_lw_vec, 10);
    lv_obj_add_event_cb(s_lw_vec, lw_vec_cb, LV_EVENT_CLICKED, NULL);
    lw_esconder();
}
static void lw_esconder(void)
{
    if (!s_lw_brillo) return;
    ocultar(s_lw_brillo, true); ocultar(s_lw_nube, true); ocultar(s_lw_esc, true); ocultar(s_lw_vec, true);
    for (int i = 0; i < 8; i++) ocultar(s_lw_luci[i], true);
    for (int c = 0; c < 3; c++) ocultar(s_lw_marca[c], true);
    for (int c = 0; c < LW_COSAS; c++) ocultar(s_lw_toque[c], true);
    if (s_lw_vec_cuadro >= 0) { poner_src(s_lw_vec, NULL); for (int k = 0; k < 2; k++) art_img_free(&s_lw_vec_art[k]); s_lw_vec_cuadro = -1; }
    s_lw_esc_en = -1;
    s_lw_aviso = 0; s_lw_pend = -1;
}
static void lw_vec_cuadro(int k)
{
    if (!s_lw_vec || k == s_lw_vec_cuadro || s_lw_vec_ico < 0) return;
    s_lw_vec_cuadro = k;
    if (!s_lw_vec_art[k].dsc.data) art_icono_en(&s_lw_vec_art[k], (art_icono_t)(s_lw_vec_ico + k), s_escena == ESC_MIRADOR ? 4 : 3);
    poner_src(s_lw_vec, &s_lw_vec_art[k].dsc);
}
/* al llegar al claro o al mirador: lo escondido de hoy, las cosas que se tocan, quién vive aquí */
static void lw_poner(int z)
{
    lw_crear();
    lw_esconder();
    s_lw_esc_en = z;
    const lw_cosa_t *C = lw_cosas(z);
    for (int c = 0; c < LW_COSAS; c++) {
        if (!C[c].r) continue;
        lv_obj_set_size(s_lw_toque[c], C[c].r * 2, C[c].r * 2);
        lv_obj_set_pos(s_lw_toque[c], C[c].x - C[c].r, C[c].y - C[c].r);
        ocultar(s_lw_toque[c], false);
        lv_obj_move_foreground(s_lw_toque[c]);
    }
    /* (101b) los juegos del mirador ya no llevan estrellita encima (delataba dónde tocar); la de «lo escondido de hoy» sí se queda */
    if (false && z == 1) for (int c = 0; c < 3; c++) if (!pet_secret_found(s_pet, 1, c)) {
        lv_obj_set_pos(s_lw_marca[c], C[c].x - 12, C[c].y - C[c].r - 18);
        ocultar(s_lw_marca[c], false);
    }
    const int zz = z == 0 ? 0 : 2;
    s_lw_esc_hay = pet_zone_find(s_pet, zz, s_ultimo_t) != PET_ZH_NADA;
    s_lw_esc_k = pet_zone_find_spot(s_pet, zz, s_ultimo_t);
    const int ex = LW_ESCONDITE[z][s_lw_esc_k][0], ey = LW_ESCONDITE[z][s_lw_esc_k][1];
    lv_obj_set_pos(s_lw_esc, ex - 12, ey - 20);
    ocultar(s_lw_esc, !s_lw_esc_hay);
    /* el vecino: el chucao (al lado de lo escondido, o junto a la poza) o el cóndor (de día, en su roca) */
    for (int k = 0; k < 2; k++) art_img_free(&s_lw_vec_art[k]);
    s_lw_vec_cuadro = -1;
    s_lw_vec_ico = z == 0 ? ICO_CHUCAO : (!zo_noche() ? ICO_CONDOR : -1);
    if (s_lw_vec_ico >= 0) {
        if (z == 0) { if (s_lw_esc_hay) { s_lw_vec_x = (float)(ex + (ex < 300 ? 34 : -34)); s_lw_vec_y = (float)(ey - 4); } else { s_lw_vec_x = 372; s_lw_vec_y = 246; } }
        else { s_lw_vec_x = 222; s_lw_vec_y = 104; }
        lw_vec_cuadro(0);
        ocultar(s_lw_vec, false);
        lv_obj_move_foreground(s_lw_vec);
    }
    s_lw_vuela_t = s_ms + 16000u;
    s_lw_aviso = s_lw_esc_hay && z == 0 ? s_ms + 3000u : 0;
    lv_obj_move_foreground(s_lw_esc);
}
static void lw_refrescar(void) { s_lw_esc_en = -2; }
static void lw_ir(int escena) /* ir al claro o al mirador desde donde esté (al mirador desde el cerro: en la nube) */
{
    if (!s_pet || s_pet->dormida || !pet_scene_open(s_pet, (escena_t)escena)) return;
    if ((int)s_pet->escena == escena) return;
    accion(ACT_IR, escena);
}
static void lw_hacer(int c) /* ella llegó a la cosa c */
{
    const int z = lw_aqui();
    if (z < 0) return;
    if (z == 0) {
        switch (c) {
        case 0: zona_entrar(ZO_CASITA); return; /* la casa del rey: entran */
        case 1: accion(UI_ACCION_SONAR, SND_CHAPOTEO); salpicar(320, 236, 0x9ED8FF); salpicar(334, 226, 0x9ED8FF);
            globo(ICO_RANA, s_nino ? NULL : (s_pet && lago_congelado(s_pet) ? "¡LA POZA ESTÁ CONGELADITA!" : "LA POZA DEL REY: EL AGUA ESTÁ FRESQUITA"), 2200); break;
        case 2: accion(UI_ACCION_SONAR, SND_REBOTE); flotar(ICO_HONGO, NULL, 0, 157, 296, 0);
            globo(ICO_HONGO, s_nino ? NULL : (zo_noche() ? "EN EL ANILLO DE HONGOS BAILAN LAS LUCIÉRNAGAS" : "UN ANILLO DE HONGOS. DE NOCHE BAILAN AQUÍ LAS LUCIÉRNAGAS"), 2600); break;
        case 3: mascota_modo(M_QUIETA, 3000); globo(ICO_RAMITA, s_nino ? NULL : "UN TRONCO PARA SENTARSE A MIRAR LAS LUCIÉRNAGAS", 2600); break;
        case 4: accion(UI_ACCION_SONAR, SND_HUELE); globo(ICO_BROTE, s_nino ? NULL : "EL HUERTO DEL REY: PEUMO PLANTABA AQUÍ SUS FRUTILLAS", 2600); break;
        default: break;
        }
        return;
    }
    switch (c) {
    case 0: zj_abrir(ZJ_PIEDRA); return;
    case 1: zj_abrir(ZJ_CORONA); return;
    case 2: zj_abrir(ZJ_NUBE); return;
    case 4: accion(UI_ACCION_SONAR, SND_PLOP); flotar(ICO_PIEDRA, NULL, 0, 295, 110, 0);
        globo(ICO_PIEDRA, s_nino ? NULL : "UNA APACHETA: CADA VIAJERO DEJA UNA PIEDRITA. ¡PUSIMOS LA NUESTRA!", 3000); break;
    case 5: accion(UI_ACCION_SONAR, SND_CRISTAL_LILA); flotar(ICO_CRISTAL, NULL, 0, 140, 256, 0);
        globo(ICO_CRISTAL, s_nino ? NULL : "POR LA GRIETA BRILLA LA CUEVA DE CRISTAL: ESTÁ JUSTO AQUÍ ABAJO", 3000); break;
    default: break;
    }
}
static int s_lw_toques, s_lw_rechazo;
static void lw_cosa_cb(lv_event_t *e)
{
    const int c = (int)(intptr_t)lv_event_get_user_data(e);
    const int z = lw_aqui();
    s_lw_toques++;
    s_lw_rechazo = z < 0 ? 1 : (!s_pet ? 2 : (s_pet->dormida ? 3 : (s_panel != PANEL_NINGUNO ? 4 : (s_juego != JG_NADA ? 5 : (zj_activo() ? 6 : 0)))));
    if (z < 0 || !s_pet || s_pet->dormida || s_panel != PANEL_NINGUNO || s_juego != JG_NADA || zj_activo()) return;
    if (z == 1 && c == 3) { lw_vec_cb(NULL); return; } /* (el cóndor) */
    const lw_cosa_t *C = lw_cosas(z);
    s_lw_pend = c; s_lw_pend_t = s_ms;
    s_mascota.tx = C[c].ix; s_mascota.ty = C[c].iy;
    mascota_modo(M_VA, 5000);
}
static void lw_vec_cb(lv_event_t *e)
{
    (void)e;
    const int z = lw_aqui();
    if (z < 0 || s_lw_vec_ico < 0) return;
    s_lw_vec_canta = s_ms;
    if (z == 0) {
        accion(UI_ACCION_SONAR, SND_CHUCAO);
        flotar(ICO_NOTA, NULL, 0, (int)s_lw_vec_x + 10, (int)s_lw_vec_y - 44, 0);
        globo(ICO_CHUCAO, s_nino ? NULL : (s_lw_esc_hay ? "¡CHU-CAO! HAY ALGO ESCONDIDO AQUÍ CERQUITA" : "¡CHU-CAO! CANTA FUERTE, PERO ES CHIQUITITO"), 3000);
    } else {
        accion(UI_ACCION_SONAR, SND_CONDOR);
        globo(ICO_CONDOR, s_nino ? NULL : "EL CÓNDOR NOS MIRA DESDE SU ROCA: VE TODO EL VALLE", 3000);
    }
}
static void lw_esc_cb(lv_event_t *e)
{
    (void)e;
    const int z = lw_aqui();
    if (z < 0 || !s_lw_esc_hay || s_panel != PANEL_NINGUNO || s_pet->dormida) return;
    s_lw_pend = 9; s_lw_pend_t = s_ms;
    s_mascota.tx = LW_ESCONDITE[z][s_lw_esc_k][0]; s_mascota.ty = LW_ESCONDITE[z][s_lw_esc_k][1] + 6;
    mascota_modo(M_VA, 5000);
}
static void lw_sacar(int z)
{
    const int zz = z == 0 ? 0 : 2, k = pet_zone_find(s_pet, zz, s_ultimo_t);
    s_lw_esc_hay = false;
    ocultar(s_lw_esc, true);
    if (k == PET_ZH_NADA) return;
    accion(UI_ACCION_ZONA_HALLAZGO, zz);
    mascota_modo(M_SALTA, 1200);
    if (k == PET_ZH_CARTA) { if (z == 0) { s_lw_vec_canta = s_ms; accion(UI_ACCION_SONAR, SND_CHUCAO); } return; } /* (fw 104) el papelito lo abre el aviso del motor */
    const art_icono_t ico = k == PET_ZH_MUSGO ? ICO_MUSGO : (k == PET_ZH_RAMITAS ? ICO_RAMITA : ICO_REGALO);
    flotar(ico, NULL, 0, (int)s_mascota.x, (int)s_mascota.y - 80, 0);
    if (k == PET_ZH_RAMITAS) flotar(ICO_RAMITA, NULL, 0, (int)s_mascota.x + 22, (int)s_mascota.y - 70, 250);
    static const char *const T[2][4] = {
        {"", "¡MUSGO SUAVECITO! EL CHUCAO SABÍA DÓNDE ESTABA", "¡DOS RAMITAS! EL CHUCAO LAS ESTABA CUIDANDO", "¡UN REGALITO ESCONDIDO ENTRE LOS HONGOS!"},
        {"", "¡MUSGO DEL CERRO! ESTÁ FRESQUITO", "¡DOS RAMITAS! EL VIENTO LAS TRAJO HASTA ACÁ ARRIBA", "¡UN REGALITO! SE LO TRAJO EL VIENTO"},
    };
    globo(ico, s_nino ? NULL : T[z][k & 3], 3200);
    if (z == 0) { s_lw_vec_canta = s_ms; accion(UI_ACCION_SONAR, SND_CHUCAO); }
}
/* (101b) al llegar a un lugar secreto con algo escondido hoy, una hoja a pantalla completa lo cuenta (una vez al día por lugar;
 * no en modo infante ni encima de otro panel o de un juego). Lo escondido dura todo el día; solo se va si ella lo encuentra
 * primero en una de sus excursiones (y se lo queda) o al cambiar el día */
static void esc_anunciar(int zz)
{
    static uint16_t dia_visto[PET_ZONAS_SECRETAS];
    if (!s_pet || (unsigned)zz >= PET_ZONAS_SECRETAS || s_nino || s_pecera || s_panel != PANEL_NINGUNO || zj_activo()) return;
    if (s_pet->etapa == STAGE_HUEVO || pet_zone_find(s_pet, zz, s_ultimo_t) == PET_ZH_NADA) return;
    const uint16_t dia = (uint16_t)(1u + (uint32_t)(((int64_t)s_ultimo_t + s_pet->zona_horaria) / 86400) % 65000u);
    if (dia_visto[zz] == dia) return;
    dia_visto[zz] = dia;
    static const char *const T[PET_ZONAS_SECRETAS] = {
        "Hoy hay algo escondido en el claro del Rey Peumo. El chucao sabe dónde está: cuando cante, busca la estrellita y tócala.",
        "Hoy hay algo escondido en la casa del rey. Algo brilla un poquito entre sus cosas: busca la estrellita y tócala.",
        "Hoy hay algo escondido en la cima del cerro. Algo brilla un poquito entre las piedras: busca la estrellita y tócala.",
    };
    char cuerpo[300];
    snprintf(cuerpo, sizeof cuerpo, "%s Queda ahí todo el día... salvo que ella lo encuentre primero en uno de sus paseos.", T[zz]);
    panel_lectura(ICO_ESTRELLA, "Lo escondido de hoy", "¡Hay un secreto aquí!", cuerpo);
}
/* cada cuadro, en el claro o el mirador */
static void animar_lugar_mundo(void)
{
    const int z = lw_aqui();
    if (z != s_lw_esc_en) {
        if (z >= 0 && s_pet && s_pet->etapa != STAGE_HUEVO) {
            lw_poner(z);
            esc_anunciar(z == 0 ? 0 : 2); /* (101b) */
            static int anunciado = -1; /* el nombre del lugar, al llegar (no al salir de la casita) */
            if (anunciado != (int)s_pet->escena || s_ms - s_lw_llego > 120000u) {
                if (!s_nino && s_panel == PANEL_NINGUNO) globo(z == 0 ? ICO_ZONA_CLARO : ICO_ZONA_MIRADOR, z == 0 ? "EL CLARO DEL REY PEUMO" : "LA CIMA DEL CERRO: EL MIRADOR", 2400);
            } else s_lw_aviso = 0;
            anunciado = s_pet->escena;
            s_lw_llego = s_ms;
        } else lw_esconder();
    }
    if (z >= 0 || s_zo) s_lw_llego = s_ms;
    if (z < 0 || !s_lw_brillo) return;
    const bool tapa = s_panel != PANEL_NINGUNO || s_juego != JG_NADA || zj_activo();
    for (int c = 0; c < LW_COSAS; c++) if (lw_cosas(z)[c].r) ocultar(s_lw_toque[c], tapa);
    /* ella llegó a lo que tocaste */
    if (s_lw_pend >= 0) {
        const lw_cosa_t *C = lw_cosas(z);
        float tx = s_lw_pend == 9 ? LW_ESCONDITE[z][s_lw_esc_k][0] : C[s_lw_pend].ix, ty = s_lw_pend == 9 ? LW_ESCONDITE[z][s_lw_esc_k][1] + 6 : C[s_lw_pend].iy;
        const bool llego = hypotf(s_mascota.x - tx, s_mascota.y - ty) < 14.0f;
        if (llego || s_ms - s_lw_pend_t > 5000u || s_panel != PANEL_NINGUNO) {
            const int c = s_lw_pend; s_lw_pend = -1;
            if (s_panel == PANEL_NINGUNO && s_pet && !s_pet->dormida) { if (c == 9) lw_sacar(z); else lw_hacer(c); }
            if (lw_aqui() != z) return;
        }
    }
    const bool noche = s_fondo_periodo == PERIODO_NOCHE || s_fondo_periodo == PERIODO_ATARDECER;
    if (z == 0) {
        /* la luz de la puerta */
        const int d = 26 + (int)(sinf((float)s_ms / 300.0f) * 5);
        lv_obj_set_size(s_lw_brillo, d, d);
        lv_obj_set_pos(s_lw_brillo, 207 - d / 2, 166 - d / 2);
        lv_obj_set_style_bg_opa(s_lw_brillo, (lv_opa_t)(noche ? 130 : 70), 0);
        ocultar(s_lw_brillo, false);
        /* las luciérnagas (de día unas pocas; de noche, todas) */
        for (int i = 0; i < 8; i++) {
            const bool ver = i < 3 || noche;
            ocultar(s_lw_luci[i], !ver || tapa);
            if (!ver) continue;
            const float a = (float)s_ms / (1500.0f + (float)i * 230.0f) + (float)i * 1.7f;
            const int x = i < 4 ? 157 + (int)(cosf(a) * 34.0f) : 120 + (i * 41) % 240 + (int)(sinf(a) * 30.0f);
            const int y = i < 4 ? 300 + (int)(sinf(a * 1.3f) * 18.0f) : 200 + (i * 53) % 150 + (int)(cosf(a * 1.3f) * 20.0f);
            lv_obj_set_pos(s_lw_luci[i], x, y);
            lv_obj_set_style_bg_opa(s_lw_luci[i], (lv_opa_t)((noche ? 190 : 110) + 60 * sinf(a * 3.0f)), 0);
            lv_obj_move_foreground(s_lw_luci[i]);
        }
    } else {
        /* la nube del último paseo flota al borde de la cima */
        const lv_image_dsc_t *nd = lv_image_get_src(s_lw_nube);
        const int nw = nd ? (int)nd->header.w : 60, nh = nd ? (int)nd->header.h : 40;
        lv_obj_set_pos(s_lw_nube, LW_MIRADOR[2].x - nw / 2, LW_MIRADOR[2].y - nh / 2 + (int)(sinf((float)s_ms / 700.0f) * 4));
        ocultar(s_lw_nube, false);
        lv_obj_move_foreground(s_lw_nube);
        for (int c = 0; c < 3; c++) if (!lv_obj_has_flag(s_lw_marca[c], LV_OBJ_FLAG_HIDDEN)) lv_obj_move_foreground(s_lw_marca[c]);
    }
    if (s_lw_esc_hay) {
        const float a = (float)s_ms / 260.0f;
        lv_obj_set_style_image_opa(s_lw_esc, (lv_opa_t)(150 + 100 * sinf(a)), 0);
        lv_image_set_scale(s_lw_esc, (uint32_t)(220 + 50 * sinf(a * 0.7f)));
    }
    /* el vecino */
    if (s_lw_vec_ico >= 0) {
        if (s_lw_aviso && s_pet->escena != (uint8_t)s_escena) s_lw_aviso = 0; /* (ya se va: no avisa) */
        if (s_lw_aviso && (int32_t)(s_ms - s_lw_aviso) >= 0 && s_panel == PANEL_NINGUNO) { /* el chucao avisa una vez al llegar */
            s_lw_aviso = 0; s_lw_vec_canta = s_ms;
            accion(UI_ACCION_SONAR, SND_CHUCAO);
            flotar(ICO_NOTA, NULL, 0, (int)s_lw_vec_x + 10, (int)s_lw_vec_y - 44, 0);
            globo(ICO_CHUCAO, s_nino ? NULL : "¡CHU-CAO! ALGO BRILLA AL LADO DEL CHUCAO", 3200);
        }
        if (z == 0) {
            if (s_lw_esc_hay && s_ms - s_lw_vec_canta > 8000u) { s_lw_vec_canta = s_ms; flotar(ICO_NOTA, NULL, 0, (int)s_lw_vec_x + 10, (int)s_lw_vec_y - 44, 0); }
            const bool canta = s_ms - s_lw_vec_canta < 900u && (s_ms / 150u) % 2;
            lw_vec_cuadro(canta ? 1 : 0);
            const int salto = canta ? -3 : -(int)(((s_ms / 90u) % 40u) < 3u ? 4 : 0);
            const lv_image_dsc_t *vd = lv_image_get_src(s_lw_vec);
            const int vw = vd ? (int)vd->header.w : 36, vh = vd ? (int)vd->header.h : 36;
            lv_obj_set_pos(s_lw_vec, (int)s_lw_vec_x - vw / 2, (int)s_lw_vec_y - vh + salto);
        } else { /* el cóndor: a ratos sale a planear sobre el valle y vuelve a su roca */
            const int32_t dv = (int32_t)(s_ms - s_lw_vuela_t);
            const lv_image_dsc_t *vd = lv_image_get_src(s_lw_vec);
            const int vw = vd ? (int)vd->header.w : 48, vh = vd ? (int)vd->header.h : 48;
            if (dv >= 0 && dv < 9000) {
                if (s_lw_vec_cuadro != 1) accion(UI_ACCION_SONAR, SND_CONDOR);
                lw_vec_cuadro(1);
                const float f = (float)dv / 9000.0f, a = f * 6.283f;
                lv_obj_set_pos(s_lw_vec, (int)(233.0f + cosf(a - 1.57f) * 190.0f) - vw / 2, (int)(233.0f + sinf(a - 1.57f) * 170.0f) - vh / 2);
            } else {
                if (dv >= 9000) s_lw_vuela_t = s_ms + 24000u;
                lw_vec_cuadro(0);
                lv_obj_set_pos(s_lw_vec, (int)s_lw_vec_x - vw / 2, (int)s_lw_vec_y - vh);
            }
        }
        ocultar(s_lw_vec, tapa);
        lv_obj_move_foreground(s_lw_vec);
    }
}

/* ---------- las entradas desde el mundo: el sendero de luciérnagas del bosque y la nube del cerro ---------- */
enum { EXC_NADA = 0, EXC_VA, EXC_FUERA, EXC_VUELVE };
static lv_obj_t *s_ze_luci[4], *s_ze_nube, *s_ze_toque;
static int s_exc_fase, s_exc_z; /* la excursión sola (ver más abajo) */
static void animar_excursion(void);
static uint32_t s_ze_sube;
#define ZE_BOSQUE_X 52   /* el borde del bosque donde empieza el sendero de las luciérnagas */
#define ZE_BOSQUE_Y 300
#define ZE_NUBE_X 316    /* la nube del cerro, a un costado de la boca de la cueva */
#define ZE_NUBE_Y 120
#define ZE_CUEVA_X 237   /* la boca de la cueva en el dibujo del cerro: brilla y se toca para entrar */
#define ZE_CUEVA_Y 180
/* el viaje en la nube: ella camina debajo, la nube baja, se sube y suben juntas hasta el mirador */
enum { ZV_NADA = 0, ZV_CAMINA, ZV_BAJA, ZV_SUBE };
static int s_ze_viaje;
static uint32_t s_ze_vt;
static lv_obj_t *s_ze_jinete, *s_ze_cueva_luz, *s_ze_cueva_toque, *s_ze_cueva_br[3];
static bool s_ze_cueva_pista;
static float ze_suelo_nube(void) { return (float)terreno_ymin(ZE_NUBE_X) + 6.0f; }
static void ze_cueva_cb(lv_event_t *e)
{
    (void)e;
    if (!s_pet || s_panel != PANEL_NINGUNO || s_pet->dormida || s_juego != JG_NADA || s_ze_viaje || s_escena != ESC_COLINA || !pet_scene_open(s_pet, ESC_CUEVA)) return;
    accion(UI_ACCION_SONAR, SND_HADAS);
    accion(ACT_IR, ESC_CUEVA);
}
static void ze_toque_cb(lv_event_t *e)
{
    (void)e;
    if (!s_pet || s_panel != PANEL_NINGUNO || s_pet->dormida || s_juego != JG_NADA) return;
    if (s_exc_fase == EXC_FUERA) { /* ahí estaba: aparecen allá, juntas */
        s_exc_fase = EXC_NADA;
        s_exc_oculta = false;
        accion(UI_ACCION_EN_ZONA, 0);
        s_lw_directo = true;
        accion(ACT_IR, s_exc_z == 0 ? ESC_CLARO : ESC_MIRADOR);
        if (!s_nino) globo(s_exc_z == 0 ? ICO_CHUCAO : ICO_CONDOR, s_exc_z == 0 ? "¡AQUÍ ESTABA! JUGANDO CON EL CHUCAO" : "¡AQUÍ ESTABA! MIRANDO AL CÓNDOR", 2600);
        return;
    }
    if (s_escena == ESC_BOSQUE && pet_scene_open(s_pet, ESC_CLARO)) { accion(UI_ACCION_SONAR, SND_HADAS); accion(ACT_IR, ESC_CLARO); }
    else if (s_escena == ESC_COLINA && pet_scene_open(s_pet, ESC_MIRADOR) && !s_ze_viaje) { accion(UI_ACCION_SONAR, SND_HADAS); accion(ACT_IR, ESC_MIRADOR); } /* (el viaje en la nube lo empieza ui_update) */
}
/* (ui.c) el motor ya está en el mirador y en la pantalla se ve el cerro: el viaje en la nube. Devuelve true si se encarga */
static bool lw_nube_desde_colina(bool mirando)
{
    if (s_ze_viaje) return true;
    if (!mirando || !s_ze_nube) return false;
    s_ze_viaje = ZV_CAMINA; s_ze_vt = s_ms;
    s_mascota.tx = ZE_NUBE_X; s_mascota.ty = ze_suelo_nube();
    mascota_modo(M_VA, 4000);
    return true;
}
/* la nube (y ella encima, si va) en el viaje: devuelve true si el viaje sigue */
static bool ze_viaje_animar(void)
{
    if (!s_ze_viaje) return false;
    const uint32_t t = s_ms - s_ze_vt;
    const lv_image_dsc_t *nd = lv_image_get_src(s_ze_nube);
    const int nw = nd ? (int)nd->header.w : 60, nh = nd ? (int)nd->header.h : 60;
    const float suelo = ze_suelo_nube(), nube_pies = suelo + (float)nh * 0.28f; /* la nube queda bajo sus pies */
    float ny = ZE_NUBE_Y;
    bool jinete = false;
    if (s_ze_viaje == ZV_CAMINA) { /* ella camina hasta debajo de la nube */
        if ((fabsf(s_mascota.x - ZE_NUBE_X) < 8.0f && fabsf(s_mascota.y - suelo) < 8.0f) || t > 4000u) {
            s_mascota.x = s_mascota.tx = ZE_NUBE_X; s_mascota.y = s_mascota.ty = suelo;
            mascota_modo(M_QUIETA, 4000);
            s_ze_viaje = ZV_BAJA; s_ze_vt = s_ms;
            accion(UI_ACCION_SONAR, SND_ALETEO);
        }
    } else if (s_ze_viaje == ZV_BAJA) { /* la nube baja hasta sus pies */
        const float f = t >= 800u ? 1.0f : (float)t / 800.0f;
        ny = ZE_NUBE_Y + (nube_pies - ZE_NUBE_Y) * (1.0f - (1.0f - f) * (1.0f - f));
        if (t >= 900u) { s_ze_viaje = ZV_SUBE; s_ze_vt = s_ms; s_exc_oculta = true; accion(UI_ACCION_SONAR, SND_REBOTE); }
    } else if (s_ze_viaje == ZV_SUBE) { /* suben juntas, hasta perderse arriba: llegan a la cima */
        const float f = t >= 1500u ? 1.0f : (float)t / 1500.0f;
        ny = nube_pies + (-90.0f - nube_pies) * f * f;
        jinete = true;
        if (t >= 1500u) {
            s_ze_viaje = ZV_NADA; s_exc_oculta = false;
            ocultar(s_ze_jinete, true);
            s_lw_directo = true; /* aparece arriba, en el mirador */
            if (s_pet && s_pet->escena != ESC_MIRADOR) accion(ACT_IR, ESC_MIRADOR);
            return false;
        }
    }
    const int bote = (int)(sinf((float)s_ms / 700.0f) * 3);
    lv_obj_set_pos(s_ze_nube, ZE_NUBE_X - nw / 2, (int)ny - nh / 2 + (s_ze_viaje == ZV_CAMINA ? bote : 0));
    lv_obj_move_foreground(s_ze_nube);
    if (jinete && s_ze_jinete) { /* ella encima: el mismo dibujo que tiene en el mundo */
        poner_src(s_ze_jinete, &s_mascota.sprite.dsc);
        const int jw = (int)s_mascota.sprite.dsc.header.w, jh = (int)s_mascota.sprite.dsc.header.h;
        lv_obj_set_pos(s_ze_jinete, ZE_NUBE_X - jw / 2, (int)ny - (int)((float)nh * 0.22f) - jh + 4);
        ocultar(s_ze_jinete, false);
        lv_obj_move_foreground(s_ze_jinete);
        lv_obj_move_foreground(s_ze_nube); /* la nube adelante: tapa sus pies */
    } else if (s_ze_jinete) ocultar(s_ze_jinete, true);
    return s_ze_viaje != ZV_NADA;
}
static void animar_entradas_zonas(void)
{
    if (!s_pet || !s_mundo) return;
    animar_excursion();
    animar_lugar_mundo();
    if (!s_zo && s_pet->en_zona && s_exc_fase != EXC_FUERA) accion(UI_ACCION_EN_ZONA, 0); /* (quedó marcado de antes: ya no están ahí) */
    if (s_zo_pend == ZO_CASITA && s_escena == ESC_CLARO && !s_zo && !zj_activo() && s_panel == PANEL_NINGUNO && s_zona_pend < 0 && !s_pet->dormida) { s_zo_pend = -1; zona_entrar(ZO_CASITA); }
    const bool afuera = !s_interior && !s_zo && s_pet->etapa != STAGE_HUEVO;
    const bool claro = afuera && s_escena == ESC_BOSQUE && pet_scene_open(s_pet, ESC_CLARO);
    const bool mira = afuera && s_escena == ESC_COLINA && pet_scene_open(s_pet, ESC_MIRADOR);
    /* la boca de la cueva brilla en el cerro (si ya se puede entrar): tocarla es ir a la cueva */
    const bool cueva = afuera && s_escena == ESC_COLINA && pet_scene_open(s_pet, ESC_CUEVA) && s_juego == JG_NADA && !s_pet->dormida && !s_ze_viaje;
    if (!s_ze_cueva_toque && cueva) {
        s_ze_cueva_luz = caja(s_mundo);
        lv_obj_set_style_radius(s_ze_cueva_luz, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(s_ze_cueva_luz, lv_color_hex(0xB8A0FF), 0);
        lv_obj_set_style_bg_opa(s_ze_cueva_luz, LV_OPA_40, 0);
        lv_obj_set_style_blend_mode(s_ze_cueva_luz, LV_BLEND_MODE_ADDITIVE, 0);
        for (int i = 0; i < 3; i++) {
            s_ze_cueva_br[i] = lv_image_create(s_mundo);
            lv_image_set_src(s_ze_cueva_br[i], art_icono(ICO_ESTRELLA, 3));
            lv_obj_remove_flag(s_ze_cueva_br[i], LV_OBJ_FLAG_CLICKABLE);
        }
        s_ze_cueva_toque = caja(s_mundo);
        lv_obj_set_size(s_ze_cueva_toque, 92, 72);
        lv_obj_set_pos(s_ze_cueva_toque, ZE_CUEVA_X - 46, ZE_CUEVA_Y - 46);
        lv_obj_add_flag(s_ze_cueva_toque, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(s_ze_cueva_toque, ze_cueva_cb, LV_EVENT_CLICKED, NULL);
    }
    if (s_ze_cueva_toque) {
        ocultar(s_ze_cueva_luz, !cueva); ocultar(s_ze_cueva_toque, !cueva);
        for (int i = 0; i < 3; i++) ocultar(s_ze_cueva_br[i], !cueva);
        if (cueva) {
            const bool nueva = !pet_scene_seen(s_pet, ESC_CUEVA); /* todavía no entra: brilla más */
            const float a = (float)s_ms / 420.0f;
            const int d = 38 + (int)(sinf(a) * 6) + (nueva ? 8 : 0);
            lv_obj_set_size(s_ze_cueva_luz, d + 10, d);
            lv_obj_set_pos(s_ze_cueva_luz, ZE_CUEVA_X - (d + 10) / 2, ZE_CUEVA_Y + 4 - d / 2);
            lv_obj_set_style_bg_opa(s_ze_cueva_luz, (lv_opa_t)((nueva ? 110 : 70) + 40 * sinf(a)), 0);
            static const int8_t BR[3][2] = {{-22, -8}, {20, -16}, {4, 14}};
            for (int i = 0; i < 3; i++) { /* tres destellos que titilan por turnos */
                const float b = sinf(a * 1.3f + (float)i * 2.1f);
                lv_obj_set_pos(s_ze_cueva_br[i], ZE_CUEVA_X + BR[i][0] - 18, ZE_CUEVA_Y + BR[i][1] - 18);
                lv_obj_set_style_image_opa(s_ze_cueva_br[i], (lv_opa_t)(b > 0 ? 80 + 175 * b : 0), 0);
                lv_image_set_scale(s_ze_cueva_br[i], (uint32_t)(150 + 90 * (b > 0 ? b : 0)));
            }
            if (nueva && !s_ze_cueva_pista && s_panel == PANEL_NINGUNO && !s_ze_viaje) { s_ze_cueva_pista = true; globo(ICO_ESTRELLA, s_nino ? NULL : "¡ALGO BRILLA EN LA CUEVA! TÓCALA", 3500); }
        }
    }
    if (!s_ze_toque && (claro || mira)) {
        for (int i = 0; i < 4; i++) {
            s_ze_luci[i] = caja(s_mundo);
            lv_obj_set_size(s_ze_luci[i], 7, 7);
            lv_obj_set_style_radius(s_ze_luci[i], LV_RADIUS_CIRCLE, 0);
            lv_obj_set_style_bg_color(s_ze_luci[i], lv_color_hex(i & 1 ? 0xD8FF8A : 0xFFE27A), 0);
            lv_obj_set_style_bg_opa(s_ze_luci[i], LV_OPA_COVER, 0);
            lv_obj_remove_flag(s_ze_luci[i], LV_OBJ_FLAG_CLICKABLE);
        }
        s_ze_nube = lv_image_create(s_mundo);
        lv_image_set_src(s_ze_nube, art_icono(ICO_NUBE, 5)); /* (grande: ella se sube encima) */
        lv_obj_remove_flag(s_ze_nube, LV_OBJ_FLAG_CLICKABLE);
        s_ze_jinete = lv_image_create(s_mundo);
        lv_obj_remove_flag(s_ze_jinete, LV_OBJ_FLAG_CLICKABLE);
        ocultar(s_ze_jinete, true);
        s_ze_toque = caja(s_mundo);
        lv_obj_set_size(s_ze_toque, 96, 80);
        lv_obj_set_style_bg_opa(s_ze_toque, 0, 0);
        lv_obj_add_flag(s_ze_toque, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(s_ze_toque, ze_toque_cb, LV_EVENT_CLICKED, NULL);
    }
    if (!s_ze_toque) return;
    for (int i = 0; i < 4; i++) ocultar(s_ze_luci[i], !claro);
    ocultar(s_ze_nube, !(mira || s_ze_viaje));
    ocultar(s_ze_toque, !(claro || mira));
    if (claro) { /* un caminito de luciérnagas hacia el borde del oeste (de noche brillan más) */
        const bool noche = s_fondo_periodo == PERIODO_NOCHE || s_fondo_periodo == PERIODO_ATARDECER;
        for (int i = 0; i < 4; i++) {
            const float a = (float)s_ms / 600.0f + (float)i * 1.3f;
            lv_obj_set_pos(s_ze_luci[i], ZE_BOSQUE_X + 64 - i * 18 + (int)(sinf(a) * 4), ZE_BOSQUE_Y - i * 10 + (int)(cosf(a) * 4));
            lv_obj_set_style_bg_opa(s_ze_luci[i], (lv_opa_t)((noche ? 190 : 110) + 60 * sinf(a * 2.0f)), 0);
            lv_obj_move_foreground(s_ze_luci[i]);
        }
        lv_obj_set_pos(s_ze_toque, ZE_BOSQUE_X - 10, ZE_BOSQUE_Y - 60);
        if (pet_zone_hint_pending(s_pet, 0) && s_panel == PANEL_NINGUNO && s_ms - s_ze_sube > 4000 && !s_pet->dormida) { accion(UI_ACCION_ZONA_PISTA, 0); globo(ICO_ESTRELLA, s_nino ? NULL : "¡LAS LUCIÉRNAGAS NOS LLEVAN AL CLARO DEL REY!", 3500); }
    }
    if (s_ze_viaje && !(s_escena == ESC_COLINA && !s_interior && !s_zo)) { s_ze_viaje = ZV_NADA; s_exc_oculta = false; if (s_ze_jinete) ocultar(s_ze_jinete, true); } /* (se fue a otra parte) */
    if (mira || s_ze_viaje) { /* la nube baja, al lado de la cueva: flota quieta como un escalón */
        if (!ze_viaje_animar()) {
            const lv_image_dsc_t *nd = lv_image_get_src(s_ze_nube);
            const int nw = nd ? (int)nd->header.w : 60, nh = nd ? (int)nd->header.h : 60;
            lv_obj_set_pos(s_ze_nube, ZE_NUBE_X - nw / 2, ZE_NUBE_Y - nh / 2 + (int)(sinf((float)s_ms / 700.0f) * 3));
        }
        ocultar(s_ze_nube, false);
        lv_obj_set_pos(s_ze_toque, ZE_NUBE_X - 48, ZE_NUBE_Y - 42);
        if (pet_zone_hint_pending(s_pet, 1) && s_panel == PANEL_NINGUNO && !s_pet->dormida && !s_ze_viaje) { accion(UI_ACCION_ZONA_PISTA, 1); globo(ICO_NUBE, s_nino ? NULL : "¡LA NUBE NOS ESPERA! NOS LLEVA A LA CIMA", 3500); }
    }
}

/* ---------- las excursiones sola: de día, tranquila en el bosque o en el cerro, a veces se va al claro o al mirador ----------
 * Camina hasta la entrada (el sendero de luciérnagas o la nube baja), desaparece un ratito y vuelve contando qué vio (y, si
 * lo escondido de hoy seguía ahí, lo encontró y se lo quedó: 101b). Si tocas la entrada mientras no está, van allá y la encuentras. Si la llamas
 * (la cuidas, le das algo), vuelve al tiro. De noche nunca va sola al mirador; tarde, tampoco al claro. */
static uint32_t s_exc_t, s_exc_dura, s_exc_mirar, s_exc_inter;
static int s_exc_pend = -1; /* (pruebas) que se vaya apenas llegue al bosque o al cerro */
static void exc_empezar(int z)
{
    s_exc_fase = EXC_VA; s_exc_z = z; s_exc_t = s_ms;
    s_exc_inter = s_pet->ultima_interaccion;
    s_mascota.tx = z == 0 ? (float)(ZE_BOSQUE_X + 30) : (float)ZE_NUBE_X;
    s_mascota.ty = z == 0 ? (float)ZE_BOSQUE_Y : (float)terreno_ymin(s_mascota.tx) + 6;
    mascota_modo(M_VA, 6000);
    if (!s_nino) globo(z == 0 ? ICO_CHUCAO : ICO_CONDOR, z == 0 ? "VOY UN RATITO AL CLARO... ¡YA VUELVO!" : "VOY A SALUDAR AL CÓNDOR AL MIRADOR. ¡YA VUELVO!", 2800);
}
static void exc_volver(bool contar)
{
    const int z = s_exc_z;
    s_exc_fase = EXC_NADA; s_exc_oculta = false;
    accion(UI_ACCION_EN_ZONA, 0);
    if (!s_pet) return;
    const int k = pet_zone_find(s_pet, z == 0 ? 0 : 2, s_ultimo_t);
    accion(UI_ACCION_EXCURSION, z);
    s_mascota.x = s_mascota.tx = z == 0 ? (float)(ZE_BOSQUE_X + 30) : (float)ZE_NUBE_X;
    s_mascota.y = s_mascota.ty = z == 0 ? (float)ZE_BOSQUE_Y : (float)terreno_ymin(s_mascota.x) + 6;
    s_mascota.tx = (float)CX; s_mascota.ty = z == 0 ? 330.0f : (float)terreno_ymin((float)CX) + 70; /* vuelve al medio, donde la ves */
    mascota_modo(M_VA, 3000);
    accion(UI_ACCION_SONAR, k == PET_ZH_REGALO ? SND_ALEGRE : SND_HADAS);
    if (!contar || s_nino) return;
    if (k == PET_ZH_CARTA) return; /* (fw 104) trajo un papelito: lo cuenta el aviso del motor («¡te traje un papelito!») */
    /* (101b) lo escondido de hoy que encontró, se lo queda (es su tesoro): ya no te lo pasa */
    static const char *const T[2][4] = {
        {"¡VOLVÍ! FUI AL CLARO Y EL CHUCAO ME CANTÓ «¡CHU-CAO!»", "¡VOLVÍ DEL CLARO! ENCONTRÉ MUSGO SUAVECITO ESCONDIDO... ¡ME LO QUEDÉ!",
         "¡VOLVÍ DEL CLARO! ENCONTRÉ RAMITAS ESCONDIDAS... ¡ME LAS QUEDÉ!", "¡VOLVÍ DEL CLARO! ENCONTRÉ UN REGALITO ESCONDIDO... ¡ES MI TESORO!"},
        {"¡VOLVÍ! EL CÓNDOR ME SALUDÓ DESDE SU ROCA. ¡QUÉ GRANDE ES!", "¡VOLVÍ DEL MIRADOR! ENCONTRÉ MUSGO DEL CERRO... ¡ME LO QUEDÉ!",
         "¡VOLVÍ DEL MIRADOR! ENCONTRÉ RAMITAS QUE TRAJO EL VIENTO... ¡ME LAS QUEDÉ!", "¡VOLVÍ DEL MIRADOR! ENCONTRÉ UN REGALITO... ¡ES MI TESORO!"},
    };
    globo(k == PET_ZH_MUSGO ? ICO_MUSGO : (k == PET_ZH_RAMITAS ? ICO_RAMITA : (k == PET_ZH_REGALO ? ICO_REGALO : (z == 0 ? ICO_CHUCAO : ICO_CONDOR))), T[z & 1][k & 3], 4200);
}
static void animar_excursion(void)
{
    if (!s_pet) return;
    if (s_exc_fase == EXC_NADA) {
        if (s_zo || zj_activo() || s_panel != PANEL_NINGUNO || s_juego != JG_NADA || s_interior || s_cie_activo || s_via_activo || s_cru_activo || s_zo_pend >= 0 ||
            s_mascota.modo != M_LIBRE || s_ms < s_exc_mirar || s_ze_viaje) return;
        if (s_exc_pend >= 0) {
            if (s_escena == (s_exc_pend == 0 ? ESC_BOSQUE : ESC_COLINA)) { exc_empezar(s_exc_pend); s_exc_pend = -1; }
            return;
        }
        s_exc_mirar = s_ms + 20000u; /* lo piensa cada 20 segundos */
        if (s_ultimo_t - s_pet->ultima_interaccion < 120u) return; /* (si estás jugando con ella, no se va) */
        const int z = pet_excursion_wants(s_pet, s_escena, s_ultimo_t);
        if (z < 0 || (lv_rand(0, 5) != 0)) return;
        exc_empezar(z);
        return;
    }
    /* algo la interrumpe: se durmió, cambió de lugar, la cuidaste */
    const bool otra = s_pet->dormida || (s_exc_z == 0 ? s_escena != ESC_BOSQUE : s_escena != ESC_COLINA) || s_interior;
    if (s_exc_fase == EXC_VA) {
        if (otra || (s_pet->ultima_interaccion > s_exc_inter + 1u) || s_panel != PANEL_NINGUNO) { s_exc_fase = EXC_NADA; return; } /* se arrepintió: se queda contigo */
        if (fabsf(s_mascota.x - s_mascota.tx) < 10.0f || s_ms - s_exc_t > 6000u) {
            s_exc_fase = EXC_FUERA; s_exc_t = s_ms; s_exc_oculta = true;
            s_exc_dura = 45000u + (uint32_t)lv_rand(0, 45000);
            accion(UI_ACCION_EN_ZONA, 1);
            accion(UI_ACCION_SONAR, s_exc_z == 0 ? SND_HADAS : SND_ALETEO);
            if (s_globo) { ocultar(s_globo, true); ocultar(s_globo_cola, true); }
        }
        return;
    }
    if (s_exc_fase == EXC_FUERA) {
        const int h = s_pet->hora_actual;
        const bool tarde = h >= (s_exc_z == 1 ? 18 : 20) || h < 6;
        if (s_pet->dormida || s_interior) { s_exc_fase = EXC_NADA; s_exc_oculta = false; accion(UI_ACCION_EN_ZONA, 0); return; }
        if (otra || tarde || s_pet->ultima_interaccion > s_exc_inter + 1u || s_ms - s_exc_t > s_exc_dura) exc_volver(true);
    }
}
/* el mapa: ir al claro o al mirador (zonas como las demás; queda por compatibilidad) */
static void zona_desde_mapa(int z) { zona_entrar(z); }
/* (pruebas) */
void ui_demo_zona(int z) /* llega al lugar de una vez (como con un portal); la casita: llega al claro y entra */
{
    if (!s_pet) return;
    cerrar_panel();
    if (s_zo && z != ZO_CASITA) zona_salir();
    const int esc = z == ZO_MIRADOR ? ESC_MIRADOR : ESC_CLARO;
    if (!pet_scene_open(s_pet, (escena_t)esc)) return;
    if ((int)s_pet->escena != esc || s_escena != esc) { s_lw_directo = true; if ((int)s_pet->escena != esc) accion(ACT_IR, esc); }
    if (z == ZO_CASITA) { if (s_escena == ESC_CLARO && s_pet->escena == ESC_CLARO) zona_entrar(ZO_CASITA); else s_zo_pend = ZO_CASITA; }
}
void ui_demo_zona_juego(int jg) { if (s_pet) { cerrar_panel(); zj_abrir(jg); } }
int ui_zona(void) { return s_zo ? s_zo : (lw_aqui() == 0 ? ZO_CLARO : (lw_aqui() == 1 ? ZO_MIRADOR : ZO_NADA)); }
void ui_demo_zona_salir(void)
{
    if (zj_activo()) { zj_cerrar(false); return; }
    if (s_zo) { zona_salir(); return; }
    if (s_pet && s_escena == ESC_CLARO) accion(ACT_IR, ESC_BOSQUE);
    else if (s_pet && s_escena == ESC_MIRADOR) accion(ACT_IR, ESC_COLINA);
}
void ui_demo_nube(void) /* (pruebas) subir en la nube: si no está en el cerro, primero va */
{
    if (!s_pet) return;
    if (s_zo) zona_salir();
    if (s_escena == ESC_COLINA && !s_interior) { if (pet_scene_open(s_pet, ESC_MIRADOR)) accion(ACT_IR, ESC_MIRADOR); }
    else accion(ACT_IR, ESC_COLINA);
}
void ui_demo_excursion(int z) { if (s_pet) { cerrar_panel(); if (z < 0) { if (s_exc_fase == EXC_FUERA) exc_volver(true); } else { s_exc_pend = z & 1; s_exc_mirar = 0; } } }
int ui_excursion(void) { return s_exc_fase; }
int ui_escena_vista(void) { return s_escena; }
int ui_hit_dbg(int x, int y) /* (pruebas) qué objeto recibe un toque en (x, y): 1 el fondo, 2 una cosa del lugar, 100+i hijo i del mundo, 200+i hijo i de la pantalla, 300+i de la capa de arriba */
{
    lv_point_t pt = {x, y};
    lv_obj_t *o = lv_indev_search_obj(lv_layer_top(), &pt);
    if (!o || o == lv_layer_top()) o = lv_indev_search_obj(lv_screen_active(), &pt);
    if (!o) return 0;
    if (o == s_fondo) return 1;
    for (int c = 0; c < LW_COSAS; c++) if (o == s_lw_toque[c]) return 2;
    lv_obj_t *p = o;
    while (p && lv_obj_get_parent(p) && lv_obj_get_parent(p) != s_mundo && lv_obj_get_parent(p) != lv_screen_active() && lv_obj_get_parent(p) != lv_layer_top()) p = lv_obj_get_parent(p);
    if (!p || !lv_obj_get_parent(p)) return -1;
    if (lv_obj_get_parent(p) == s_mundo) return 100 + (int)lv_obj_get_index(p);
    if (lv_obj_get_parent(p) == lv_layer_top()) return 300 + (int)lv_obj_get_index(p);
    return 200 + (int)lv_obj_get_index(p);
}
int ui_lw_dbg(int q) /* (pruebas) 0 el lugar del mundo (0 claro, 1 mirador, -1 otro), 1 toques en sus cosas, 2 por qué no hizo caso, 3 a qué va, 4 viaje en la nube */
{
    switch (q) {
    case 0: return s_lw_esc_en;
    case 1: return s_lw_toques;
    case 2: return s_lw_rechazo;
    case 3: return s_lw_pend;
    case 4: return s_ze_viaje;
    case 5: return s_lw_esc_hay;
    default: if (q >= 1000) { lv_obj_t *o = lv_obj_get_child(lv_screen_active(), q % 1000 > 500 ? -1 : (int32_t)(q % 1000)); if (!o) return -1; lv_area_t a; lv_obj_get_coords(o, &a);
                 return q >= 2000 ? (int)(a.x1 * 1000 + a.y1) : (int)((a.x2 - a.x1 + 1) * 1000 + (a.y2 - a.y1 + 1)); }
             return 0;
    }
}
