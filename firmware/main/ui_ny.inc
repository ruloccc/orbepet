/* ======================================================================
 * Nochevieja con espera (31 de diciembre → 1 de enero), en el prado:
 * (v38: en modo niño e infante, todo 3 horas antes: la cuenta regresiva termina a las 21:00)
 *   20:00  el prado se llena de adornos (luces doradas, farolitos de papel, globos, serpentinas) y ella se pone un gorro
 *   23:55  aviso "¡QUEDAN 5 MINUTOS PARA AÑO NUEVO!" y llegan sus amigos
 *   23:59  cuenta regresiva a pantalla completa, un tic por segundo; los últimos 10 números, grandes y latiendo
 *   00:00  la cuenta se cierra sola y salen los fuegos artificiales (tocar el cielo lanza más), después el abrazo
 *          de todos y "¡FELIZ AÑO NUEVO!"
 * Si dormía o el reloj estaba en el bolsillo: no se despierta por ruido; al encender, los fuegos se ven una vez.
 * ==================================================================== */
#define NY_LUCES 14
#define NY_FAROLES 3
#define NY_GLOBOS 5
#define NY_COHETES 8
enum { NY_NADA = 0, NY_PREP, NY_AVISO, NY_CUENTA, NY_FUEGOS, NY_ABRAZO };
static int s_ny_fase, s_ny_anio_hecho = -1, s_ny_aviso_dado, s_ny_seg_ant = -1, s_ny_salva;
static uint32_t s_ny_t0, s_ny_seg_ms, s_ny_prox_cohete;
static lv_obj_t *s_ny_luz[NY_LUCES], *s_ny_farol[NY_FAROLES], *s_ny_globo[NY_GLOBOS], *s_ny_serp[2], *s_ny_toque;
static lv_obj_t *s_ny_cu, *s_ny_cu_num, *s_ny_cu_txt;
static art_img_t s_ny_num_art, s_ny_txt_art;
static int s_ny_num_ult = -1;
static bool s_ny_creado;
static struct {
    uint32_t t0;
    float x, y;
    int tam;     /* 0 chico, 1 mediano, 2 grande */
    bool lejos;  /* los de lejos suenan más suave y se ven chicos */
    uint32_t col;
    uint8_t estado; /* 0 libre, 1 sube, 2 estalló */
    bool chispas;
    lv_obj_t *estela, *estallido;
} s_ny_coh[NY_COHETES];
static const uint32_t NY_COL[] = {0xFF5A7A, 0xFFE066, 0x6FB7FF, 0x7FE08B, 0xC090FF, 0xFF9A3C, 0xFFFFFF};
static const int16_t NY_GLOBO_XY[NY_GLOBOS][2] = {{52, 262}, {80, 240}, {410, 250}, {386, 232}, {432, 272}};
static const uint32_t NY_GLOBO_COL[NY_GLOBOS] = {0xFF6A9A, 0x6FB7FF, 0xFFE066, 0x7FE08B, 0xC090FF};

static void ny_cohete(float x, float y, int tam, bool lejos);

static void ny_toque_cb(lv_event_t *e)
{
    (void)e;
    if (s_ny_fase != NY_FUEGOS) return;
    lv_point_t pt = {0, 0};
    lv_indev_t *in = lv_indev_active();
    if (in) lv_indev_get_point(in, &pt);
    if (pt.y > 280) return;
    ny_cohete((float)pt.x, (float)(pt.y < 70 ? 70 : pt.y), (int)(azar() % 3), false); /* tocar el cielo lanza más */
}

static void ny_crear(void)
{
    if (s_ny_creado) return;
    s_ny_creado = true;
    fiesta_crear();
    for (int i = 0; i < NY_LUCES; i++) { /* luces doradas colgadas en arco entre los árboles */
        s_ny_luz[i] = caja(s_mundo);
        lv_obj_set_size(s_ny_luz[i], 7, 7);
        lv_obj_set_style_radius(s_ny_luz[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(s_ny_luz[i], lv_color_hex(0xFFD45C), 0);
        lv_obj_set_style_bg_opa(s_ny_luz[i], LV_OPA_COVER, 0);
        lv_obj_set_style_shadow_width(s_ny_luz[i], 12, 0);
        lv_obj_set_style_shadow_color(s_ny_luz[i], lv_color_hex(0xFFE27A), 0);
        const float u = (float)i / (float)(NY_LUCES - 1);
        lv_obj_set_pos(s_ny_luz[i], 64 + (int)(u * 340.0f), 112 + (int)(sinf(u * 3.1416f) * 34.0f));
        ocultar(s_ny_luz[i], true);
    }
    for (int i = 0; i < NY_FAROLES; i++) {
        s_ny_farol[i] = lv_image_create(s_mundo);
        lv_image_set_src(s_ny_farol[i], art_icono(ICO_FAROL_PAPEL, 3));
        lv_obj_set_pos(s_ny_farol[i], 130 + i * 90 - 18, 128 + (i == 1 ? 14 : 0));
        ocultar(s_ny_farol[i], true);
    }
    for (int i = 0; i < NY_GLOBOS; i++) {
        s_ny_globo[i] = lv_image_create(s_mundo);
        lv_image_set_src(s_ny_globo[i], art_icono(ICO_GLOBO_FIESTA, 3));
        lv_obj_set_style_image_recolor(s_ny_globo[i], lv_color_hex(NY_GLOBO_COL[i]), 0);
        lv_obj_set_style_image_recolor_opa(s_ny_globo[i], LV_OPA_50, 0);
        lv_obj_set_pos(s_ny_globo[i], NY_GLOBO_XY[i][0] - 18, NY_GLOBO_XY[i][1] - 36);
        ocultar(s_ny_globo[i], true);
    }
    for (int i = 0; i < 2; i++) { /* serpentinas */
        s_ny_serp[i] = lv_image_create(s_mundo);
        lv_image_set_src(s_ny_serp[i], art_icono(ICO_GUIRNALDA, 4));
        lv_obj_set_pos(s_ny_serp[i], i ? 296 : 118, 150);
        ocultar(s_ny_serp[i], true);
    }
    for (int i = 0; i < NY_COHETES; i++) {
        s_ny_coh[i].estela = caja(s_mundo);
        lv_obj_set_size(s_ny_coh[i].estela, 6, 14);
        lv_obj_set_style_radius(s_ny_coh[i].estela, 3, 0);
        lv_obj_set_style_bg_color(s_ny_coh[i].estela, lv_color_hex(0xFFF0B0), 0);
        lv_obj_set_style_bg_opa(s_ny_coh[i].estela, LV_OPA_COVER, 0);
        lv_obj_set_style_shadow_width(s_ny_coh[i].estela, 10, 0);
        lv_obj_set_style_shadow_color(s_ny_coh[i].estela, lv_color_hex(0xFFE27A), 0);
        ocultar(s_ny_coh[i].estela, true);
        s_ny_coh[i].estallido = lv_image_create(s_mundo);
        lv_image_set_src(s_ny_coh[i].estallido, art_icono(ICO_FUEGOS, 4));
        lv_obj_set_style_image_recolor_opa(s_ny_coh[i].estallido, LV_OPA_70, 0);
        ocultar(s_ny_coh[i].estallido, true);
    }
    s_ny_toque = caja(s_mundo); /* el cielo: tocarlo lanza más fuegos */
    lv_obj_set_size(s_ny_toque, W, 280);
    lv_obj_add_flag(s_ny_toque, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_ny_toque, ny_toque_cb, LV_EVENT_PRESSED, NULL);
    ocultar(s_ny_toque, true);
    /* la cuenta regresiva: toda la pantalla, por encima de todo */
    s_ny_cu = caja(lv_layer_top());
    lv_obj_set_size(s_ny_cu, W, W);
    lv_obj_set_style_bg_color(s_ny_cu, lv_color_hex(0x070C26), 0);
    lv_obj_set_style_bg_grad_color(s_ny_cu, lv_color_hex(0x1C1450), 0);
    lv_obj_set_style_bg_grad_dir(s_ny_cu, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_bg_opa(s_ny_cu, LV_OPA_COVER, 0);
    lv_obj_add_flag(s_ny_cu, LV_OBJ_FLAG_CLICKABLE); /* (se traga los toques: se cierra sola a las 00:00) */
    s_ny_cu_num = lv_image_create(s_ny_cu);
    s_ny_cu_txt = lv_image_create(s_ny_cu);
    ocultar(s_ny_cu, true);
}

static void ny_mostrar_decor(bool si)
{
    for (int i = 0; i < NY_LUCES; i++) ocultar(s_ny_luz[i], !si);
    for (int i = 0; i < NY_FAROLES; i++) ocultar(s_ny_farol[i], !si);
    for (int i = 0; i < NY_GLOBOS; i++) ocultar(s_ny_globo[i], !si);
    for (int i = 0; i < 2; i++) ocultar(s_ny_serp[i], !si);
}

/* un cohete: sube silbando, estalla (chico, mediano o grande) y crepita al final; los de lejos, suaves */
static void ny_cohete(float x, float y, int tam, bool lejos)
{
    int k = -1;
    for (int i = 0; i < NY_COHETES; i++) if (s_ny_coh[i].estado == 0) { k = i; break; }
    if (k < 0) return;
    s_ny_coh[k].t0 = s_ms;
    s_ny_coh[k].x = x; s_ny_coh[k].y = y;
    s_ny_coh[k].tam = lejos ? 0 : tam;
    s_ny_coh[k].lejos = lejos;
    s_ny_coh[k].col = NY_COL[azar() % 7];
    s_ny_coh[k].estado = 1;
    s_ny_coh[k].chispas = false;
    lv_obj_set_style_image_recolor(s_ny_coh[k].estallido, lv_color_hex(s_ny_coh[k].col), 0);
    if (!lejos) accion(UI_ACCION_SONAR, SND_COHETE_SILBA);
}

static void ny_cohetes_animar(void)
{
    for (int i = 0; i < NY_COHETES; i++) {
        if (!s_ny_coh[i].estado) continue;
        const uint32_t e = s_ms - s_ny_coh[i].t0;
        const uint32_t SUBE = s_ny_coh[i].lejos ? 380 : 620;
        if (e < SUBE) { /* sube con su estelita */
            float k = (float)e / (float)SUBE;
            ocultar(s_ny_coh[i].estela, false);
            lv_obj_set_pos(s_ny_coh[i].estela, (int)s_ny_coh[i].x - 3, (int)(340.0f - (340.0f - s_ny_coh[i].y) * (1.0f - (1.0f - k) * (1.0f - k))));
            lv_obj_set_style_opa(s_ny_coh[i].estela, s_ny_coh[i].lejos ? LV_OPA_50 : LV_OPA_COVER, 0);
            continue;
        }
        ocultar(s_ny_coh[i].estela, true);
        const uint32_t t = e - SUBE;
        if (s_ny_coh[i].estado == 1) { /* ¡bum! */
            s_ny_coh[i].estado = 2;
            static const sound_t BUM[3] = {SND_COHETE_BUM_P, SND_COHETE_BUM_M, SND_COHETE_BUM_G};
            accion(UI_ACCION_SONAR, s_ny_coh[i].lejos ? SND_COHETE_LEJOS : BUM[s_ny_coh[i].tam]);
            const int n = s_ny_coh[i].lejos ? 4 : 6 + 4 * s_ny_coh[i].tam;
            for (int p = 0; p < n; p++) salpicar((int)s_ny_coh[i].x - 20 + (int)(azar() % 40), (int)s_ny_coh[i].y - 14 + (int)(azar() % 28), s_ny_coh[i].col);
        }
        if (!s_ny_coh[i].chispas && t > 260 && !s_ny_coh[i].lejos) { /* el crepitar final */
            s_ny_coh[i].chispas = true;
            accion(UI_ACCION_SONAR, SND_COHETE_CHISPAS);
        }
        const uint32_t DUR = s_ny_coh[i].lejos ? 1100 : 1500 + 300u * (uint32_t)s_ny_coh[i].tam;
        if (t >= DUR) { s_ny_coh[i].estado = 0; ocultar(s_ny_coh[i].estallido, true); continue; }
        const float k = (float)t / (float)DUR;
        static const float ESC[3] = {1.8f, 3.0f, 4.6f}; /* chico, mediano y grande (el de lejos, más chico aún) */
        const float esc = s_ny_coh[i].lejos ? 1.1f : ESC[s_ny_coh[i].tam];
        const int lado = (int)(48.0f * esc * (0.4f + 0.6f * sqrtf(k)));
        lv_image_set_scale(s_ny_coh[i].estallido, (uint32_t)(256.0f * esc * (0.4f + 0.6f * sqrtf(k))));
        lv_obj_set_pos(s_ny_coh[i].estallido, (int)s_ny_coh[i].x - 24, (int)s_ny_coh[i].y - 24);
        lv_obj_set_style_translate_x(s_ny_coh[i].estallido, 0, 0);
        (void)lado;
        lv_obj_set_style_opa(s_ny_coh[i].estallido, (lv_opa_t)(255.0f * (s_ny_coh[i].lejos ? 0.5f : 1.0f) * (1.0f - k * k)), 0);
        ocultar(s_ny_coh[i].estallido, false);
        lv_obj_move_foreground(s_ny_coh[i].estallido);
    }
}

static void ny_cuenta_mostrar(int quedan)
{
    if (quedan == s_ny_num_ult) return;
    s_ny_num_ult = quedan;
    char t[8];
    snprintf(t, sizeof t, "%d", quedan);
    const bool final = quedan <= 10;
    art_texto(&s_ny_num_art, t, final ? 0xFFE27A : 0xF0F4FF, final ? 22 : 12, 0);
    poner_src(s_ny_cu_num, &s_ny_num_art.dsc);
    lv_obj_center(s_ny_cu_num);
    lv_obj_set_style_translate_y(s_ny_cu_num, 6, 0);
    if (!s_nino) {
        art_texto(&s_ny_txt_art, final ? "¡YA CASI!" : "AÑO NUEVO EN...", 0xB8C4FF, 3, 0);
        poner_src(s_ny_cu_txt, &s_ny_txt_art.dsc);
        lv_obj_align(s_ny_cu_txt, LV_ALIGN_TOP_MID, 0, 70);
        ocultar(s_ny_cu_txt, false);
    } else ocultar(s_ny_cu_txt, true);
    accion(UI_ACCION_SONAR, quedan <= 3 ? SND_TOQUE : SND_TOQUE); /* un tic por segundo */
}

static void ny_amigos_llegan(void)
{
    const pet_t *p = s_pet;
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
}

static void ny_amigos_animar(bool abrazo)
{
    for (int i = 0; i < FIE_AMIGOS; i++) {
        criatura_t *v = &s_fie_amigo[i];
        if (!v->visible) continue;
        if (abrazo) { v->tx = s_mascota.x + (i == 0 ? -46.0f : (i == 1 ? 46.0f : 0.0f)); v->ty = s_mascota.y + (i == 2 ? -34.0f : 4.0f); }
        bool camina = mover(v, 3.5f);
        art_pose_t *ps = &v->pose;
        ps->expr = EXP_FELIZ; ps->cola = 1; ps->orejas = 1; ps->espejo = !camina ? s_mascota.x < v->x : v->tx < v->x;
        ps->boca = !camina && (s_ms / 300 + (uint32_t)i) % 4 == 0;
        ps->frame = (int)((s_ms / (camina ? 200 : 700)) % 2);
        float sy = !camina && s_ny_fase >= NY_FUEGOS ? -fabsf(sinf((float)s_ms / 140.0f + (float)i)) * 8 : 0;
        dibujar(v);
        colocar(v, 0, sy);
    }
}

static void ny_fin(void)
{
    ocultar(s_ny_cu, true);
    ny_mostrar_decor(false);
    for (int i = 0; i < FIE_AMIGOS; i++) mostrar_criatura(&s_fie_amigo[i], false);
    for (int i = 0; i < NY_COHETES; i++) { s_ny_coh[i].estado = 0; ocultar(s_ny_coh[i].estela, true); ocultar(s_ny_coh[i].estallido, true); }
    ocultar(s_ny_toque, true);
    s_ny_fase = NY_NADA;
    s_ny_gorro = false;
}

static int s_ny_dbg;
int ui_ny_debug(void) { return s_ny_dbg; }
static void animar_nochevieja(void)
{
    const pet_t *p = s_pet;
    if (!p || !s_mundo) return;
    ny_crear();
    int a = 0, m = 0, d = 0;
    /* (v38) Año Nuevo temprano: en modo niño e infante la cuenta regresiva es a las 21:00 (los niños se duermen antes);
     * todo se corre 3 horas: adornos a las 17:00, aviso a las 20:55, cuenta a las 20:59 y fuegos a las 21:00 */
    const uint32_t adelanto = s_suave ? 3u * 3600u : 0u;
    pet_local_date(p, s_ultimo_t + adelanto, &a, &m, &d);
    const int64_t l = (int64_t)s_ultimo_t + adelanto + p->zona_horaria;
    const int seg = (int)(((l % 86400) + 86400) % 86400);
    const bool vispera = m == 12 && d == 31;
    const bool dia1 = m == 1 && d == 1;
    const int anio = dia1 ? a - 1 : a; /* es la fiesta del año que termina */
    const bool visible_ok = p->etapa != STAGE_HUEVO && s_pantalla_visible && !s_interior && s_zona_pend < 0;
    const bool hecho = s_ny_anio_hecho == anio;
    s_ny_dbg = s_ny_fase * 100000 + seg / 60 * 10 + (p->dormida ? 1 : 0) + (m * 1000000);
    /* ¿en qué momento de la noche estamos? */
    int quiero = NY_NADA;
    if (!hecho) {
        if (vispera && seg >= 20 * 3600) quiero = seg >= 86400 - 300 ? (seg >= 86400 - 60 ? NY_CUENTA : NY_AVISO) : NY_PREP;
        else if (dia1 && seg < 6 * 3600) quiero = NY_FUEGOS; /* pasada la medianoche: los fuegos se ven una vez */
    }
    /* fuera de la noche, o ella duerme: nada suena ni aparece (no se despierta por ruido) */
    if (s_ny_fase == NY_NADA && (quiero == NY_NADA || p->dormida || !visible_ok)) { s_ny_gorro = false; return; }
    if (s_ny_fase != NY_NADA && (p->dormida || s_interior || s_zona_pend >= 0)) { ny_fin(); return; }
    if (s_ny_fase >= NY_FUEGOS && s_escena != ESC_PRADO) { /* se fue del prado a mitad de los fuegos: se ven igual allá, pero no hay fiesta */
        ny_fin();
        s_ny_anio_hecho = anio;
        return;
    }
    const bool en_prado = s_escena == ESC_PRADO;
    /* adornos y gorro: de las 20:00 hasta que termina (solo en el prado) */
    const bool noche_de_fiesta = !hecho && ((vispera && seg >= 20 * 3600) || (dia1 && seg < 2 * 3600)) && !p->dormida;
    ny_mostrar_decor(noche_de_fiesta && en_prado && !s_interior);
    s_ny_gorro = noche_de_fiesta;
    if (noche_de_fiesta && en_prado) {
        for (int i = 0; i < NY_LUCES; i++) lv_obj_set_style_opa(s_ny_luz[i], (lv_opa_t)(170 + 85 * sinf((float)s_ms / 260.0f + (float)i * 0.9f)), 0);
        for (int i = 0; i < NY_FAROLES; i++) lv_obj_set_style_translate_x(s_ny_farol[i], (int32_t)(sinf((float)s_ms / 900.0f + (float)i) * 3), 0);
        for (int i = 0; i < NY_GLOBOS; i++) lv_obj_set_style_translate_y(s_ny_globo[i], (int32_t)(sinf((float)s_ms / 700.0f + (float)i * 1.3f) * 4), 0);
    }
    /* transiciones según la hora (solo con la pantalla libre) */
    if (s_ny_fase < NY_FUEGOS && quiero != NY_NADA && en_prado && pantalla_libre(p)) {
        if (quiero >= NY_AVISO && s_ny_fase < NY_AVISO) { /* llegan los amigos y avisa */
            s_ny_fase = NY_AVISO;
            ny_amigos_llegan();
            if (!s_ny_aviso_dado) { s_ny_aviso_dado = 1; globo(ICO_FUEGOS, "¡QUEDAN 5 MINUTOS PARA AÑO NUEVO!", 5000); mascota_modo(M_SALTA, 1500); }
        }
        if (quiero == NY_CUENTA && s_ny_fase < NY_CUENTA) { /* la cuenta regresiva a pantalla completa */
            cerrar_panel();
            s_ny_fase = NY_CUENTA;
            s_ny_num_ult = -1;
            s_ny_seg_ant = -1;
            ocultar(s_ny_cu, false);
            lv_obj_move_foreground(s_ny_cu);
        }
        if (quiero == NY_FUEGOS) { s_ny_fase = NY_FUEGOS; s_ny_t0 = s_ms; s_ny_salva = 0; s_ny_prox_cohete = s_ms + 300; ny_amigos_llegan(); mascota_modo(M_SALTA, 2500); }
        if (s_ny_fase == NY_NADA && quiero == NY_PREP) s_ny_fase = NY_PREP;
    }
    if (s_ny_fase == NY_PREP || s_ny_fase == NY_AVISO) {
        if (s_ny_fase == NY_AVISO) ny_amigos_animar(false);
        return;
    }
    if (s_ny_fase == NY_CUENTA) {
        const int quedan = 86400 - seg; /* 60 .. 1 */
        if (seg < 86400 - 60 || dia1) { /* ya es medianoche: la cuenta se cierra sola y salen los fuegos */
            ocultar(s_ny_cu, true);
            s_ny_fase = NY_FUEGOS; s_ny_t0 = s_ms; s_ny_salva = 0; s_ny_prox_cohete = s_ms + 250;
            accion(UI_ACCION_SONAR, SND_FIESTA);
            mascota_modo(M_SALTA, 3000);
            return;
        }
        if (quedan != s_ny_seg_ant) { s_ny_seg_ant = quedan; s_ny_seg_ms = s_ms; }
        ny_cuenta_mostrar(quedan);
        const float fr = (float)(s_ms - s_ny_seg_ms) / 1000.0f;
        float pulso = 0;
        if (quedan <= 10) pulso = 0.35f * (1.0f - fr) * (1.0f - fr); /* cada número "late" al aparecer */
        else pulso = 0.06f * (1.0f - fr);
        lv_image_set_scale(s_ny_cu_num, (uint32_t)(256.0f * (1.0f + pulso)));
        ny_amigos_animar(false);
        return;
    }
    if (s_ny_fase >= NY_FUEGOS) {
        const uint32_t t = s_ms - s_ny_t0;
        ocultar(s_ny_toque, s_ny_fase != NY_FUEGOS);
        lv_obj_move_foreground(s_ny_toque);
        if (s_ny_fase == NY_FUEGOS) {
            if (s_ms >= s_ny_prox_cohete && t < 26000) { /* un cohete cada tanto: chicos, medianos, grandes y algunos lejanos */
                const uint32_t r = azar() % 100;
                const bool lejos = r < 28;
                const int tam = r < 55 ? 0 : (r < 85 ? 1 : 2);
                ny_cohete((float)(70 + (int)(azar() % 326)), (float)((lejos ? 60 : 80) + (int)(azar() % (lejos ? 50 : 130))), tam, lejos);
                s_ny_prox_cohete = s_ms + 650 + azar() % 700;
            }
            if (t >= 26000 && s_ny_salva < 8 && s_ms >= s_ny_prox_cohete) { /* la salva final: varios seguidos */
                ny_cohete((float)(80 + s_ny_salva * 44), (float)(90 + (int)(azar() % 90)), s_ny_salva % 3, false);
                s_ny_salva++;
                s_ny_prox_cohete = s_ms + 190;
            }
            if (t == 0 || (t < 200 && !s_nino && s_ny_salva == 0 && t < TICK_MS * 2)) {}
            if (t > 3000 && t < 3000 + TICK_MS * 2 && !s_nino) globo(ICO_FUEGOS, "¡FUEGOS ARTIFICIALES! TOCA EL CIELO PARA MÁS", 3500);
            if (t > 31500) { s_ny_fase = NY_ABRAZO; s_ny_t0 = s_ms; mascota_modo(M_ABRAZO, 6000); }
            ny_amigos_animar(false);
        } else { /* el abrazo de todos y el feliz año nuevo */
            ny_amigos_animar(true);
            if (t > 600 && t < 600 + TICK_MS * 2) {
                if (!s_nino) globo(ICO_FUEGOS, "¡FELIZ AÑO NUEVO!", 5000);
                for (int k = 0; k < 6; k++) flotar(ICO_CORAZON, NULL, 0, (int)s_mascota.x - 50 + k * 20, cabeza_y(&s_mascota) - 10, (uint32_t)k * 160);
                accion(UI_ACCION_SONAR, SND_FIESTA);
            }
            if (t > 9000) { accion(UI_ACCION_FIESTA, FECHA_ANIO_NUEVO); s_ny_anio_hecho = anio; ny_fin(); }
        }
        ny_cohetes_animar();
    }
}

/* para probar en el simulador: 1 aviso, 2 cuenta, 3 fuegos */
void ui_demo_nochevieja(int f) { s_ny_anio_hecho = -1; s_ny_aviso_dado = 0; if (f == 3) { ny_crear(); s_ny_fase = NY_FUEGOS; s_ny_t0 = s_ms; s_ny_salva = 0; s_ny_prox_cohete = s_ms + 200; ny_amigos_llegan(); } }

int ui_dbg_pos(int k)
{
    switch (k) {
    case 0: return (int)s_mascota.x; case 1: return (int)s_mascota.y; case 2: return (int)s_mascota.modo;
    case 3: return s_fauna[FAUNA_RANA].visible ? (int)s_fauna[FAUNA_RANA].x : -1; case 4: return (int)s_fauna[FAUNA_RANA].y;
    case 5: return s_pet ? (int)s_pet->actividad : -1; case 6: return s_pesca_fase; case 7: return s_bote_ir;
    /* (v88) la luz del corazón de cristal: existe, oculta, x, y, su índice, el de ella, mismo padre, escala */
    case 20: return s_aura_drag ? 1 : 0; case 21: return s_aura_drag ? lv_obj_has_flag(s_aura_drag, LV_OBJ_FLAG_HIDDEN) : -1;
    case 22: return s_aura_drag ? (int)lv_obj_get_x(s_aura_drag) : -1; case 23: return s_aura_drag ? (int)lv_obj_get_y(s_aura_drag) : -1;
    case 24: return s_aura_drag ? (int)lv_obj_get_index(s_aura_drag) : -1; case 25: return (int)lv_obj_get_index(s_mascota.img);
    case 26: return s_aura_drag ? (lv_obj_get_parent(s_aura_drag) == lv_obj_get_parent(s_mascota.img)) : -1;
    case 27: return s_aura_drag ? (int)lv_image_get_scale(s_aura_drag) : -1; case 28: return s_aura_drag ? (int)lv_obj_get_width(s_aura_drag) : -1;
    }
    return 0;
}
void ui_demo_frutas(void) { empezar_juego(JUEGO_FRUTAS); }
