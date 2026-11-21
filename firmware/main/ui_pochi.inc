/* ======================================================================
 * Pochi: una chihuahua visitante, con mucha personalidad y la lengua afuera. Es la mamá de cuatro cachorros:
 * Chapi (el cochino amoroso), Pasita y Morita (tiernas; Morita, tímida) y Rony (el más loco, juega con cosas chiquitas).
 * Llega con hambre, come, y muy seguido invita a pasear. Si la acompañas, la lleva por todos los mundos
 * mientras le habla, y al final se despide: tiene que volver con sus cachorros.
 * (fw 102) Más personalidad: se acuerda de cómo terminó la visita anterior; si estás triste o enferma te consuela; si
 * duermes llega en puntillas y se acurruca a tu lado; con lluvia llega mojada y se sacude; en la nieve deja huellitas;
 * tiene manías (se roba una ramita, persigue luciérnagas, le ladra al cóndor, le tiene miedo al agua); se tira de espalda
 * con las patas al aire y se rasca el poto en el pasto; tocarla varias veces: panza arriba, y si sigues, se marea.
 * A veces viene con uno de sus cachorros, que hace lo suyo. (Vive solo en la pantalla: la mascota recibe alegría y cariño.)
 * ==================================================================== */
typedef enum { PO_NADA = 0, PO_LLEGA, PO_HAMBRE, PO_COME, PO_INVITA, PO_PASEO, PO_DESPIDE, PO_SE_VA, PO_CORRE, PO_BARRO, PO_CHARLA,
               PO_ACURRUCA, PO_CONSUELA, PO_MOJADA, PO_OFENDIDA, PO_MANIA, PO_ANIMAL, PO_TAPITA } po_fase_t;
/* (1.0.30) no todas las visitas son iguales: a veces viene por papas y a pasear (la de siempre), a veces solo por papas,
 * a veces solo a invitarte a pasear, a veces a jugar con una tapita que tú le tiras, y a veces solo a saludar y
 * presentarte a sus cachorros. Usa lo que ya había; solo cambia qué partes trae cada visita. */
enum { PT_COMPLETA = 0, PT_PAPAS, PT_PASEO, PT_TAPITA, PT_SALUDA, PT_N };
static int s_po_tipo, s_po_tipo_ant = -1, s_po_tipo_forzar = -1;
static int s_pc_n; /* (más abajo: cuántos cachorros vinieron) */
static bool po_tipo_pasea(void) { return s_po_tipo == PT_COMPLETA || s_po_tipo == PT_PASEO; }
/* (fw 102) trucos que se ponen encima de cualquier fase (el tiempo de la fase espera): panza arriba, mareada, rascarse el poto */
enum { TR_NADA = 0, TR_PANZA, TR_MAREO, TR_RASCA, TR_LAME }; /* (TR_LAME: le lame la cabeza a un cachorro) */
enum { MAN_RAMITA = 0, MAN_LUZ, MAN_CONDOR, MAN_AGUA };
static int s_po_truco, s_po_toques, s_po_ult, s_po_mania = -1, s_po_mania_vuelve, s_po_mst;
static uint32_t s_po_truco_t0, s_po_truco_dur, s_po_toque_ms, s_po_ult_ms;
static bool s_po_rasco, s_po_panzo, s_po_comio, s_po_paseo_ok, s_po_mania_hecha, s_po_rapido, s_po_dormida_visita, s_po_rascada, s_po_nieve;
static lv_obj_t *s_po_cosa; /* lo que lleva o persigue: la ramita robada, la luciérnaga, el cóndor que pasa */
static int s_po_cosa_ico = -1;
static float s_po_cosa_x, s_po_cosa_y;
static int s_po_mania_try = -1;
static bool s_pa_on, s_pa_hecha; /* el encuentro con un animal del lugar (más abajo) */
static void po_cosa(int ico, int esc, float x, float y);
static void pc_empezar(void);
static void pc_solo_uno(int k);
static void pc_esconder(void);
static void po_toque(void);
static int s_po_charla_k = -1; /* (v39) la charla de esta visita */
static bool s_po_tilo_ladro;    /* (v39) ya ladró el nombre en esta visita */
static float s_po_dtf = 1.0f; /* cuántos «cuadros normales» pasaron: Pochi se mueve igual de rápido aunque la pantalla vaya lenta (la cueva) */
static const char *s_po_resp_t;
static uint32_t s_po_resp_ms;
/* ella le contesta a Pochi unos instantes después de que él termina de hablar */
static void po_responde(const char *a, const char *b, uint32_t despues)
{
    s_po_resp_t = (azar() & 1) ? a : b;
    s_po_resp_ms = s_ms + despues;
}
static lv_obj_t *s_po_img, *s_po_plato, *s_po_charco;
static float s_po_cx, s_po_cy; /* dónde está el charco de barro */
static const char *const PO_DESP[] = { /* (fw 102) Pochi es la mamá: vuelve con sus cachorros (una frase para cada uno: todos igual de importantes) */
    "MIS CACHORROS ME ESPERAN EN CASA PARA QUE LES DÉ AMOR",
    "RONY YA DEBE ESTAR MORDIENDO ALGO CHIQUITITO... ¡TENGO QUE IR!",
    "CHAPI SE VA A REVOLCAR SIN MÍ... ¡QUÉ CHANCHITO!",
    "PASITA ME ESPERA PARA QUE LA PEINE",
    "MORITA NO SE DUERME SI NO LE CANTO",
    "ES HORA DE LA LECHE DE MIS CUATRO POCHITOS",
    "TENGO QUE ACOSTAR A CHAPI, PASITA, MORITA Y RONY",
};
#define PO_N_DESP ((int)(sizeof PO_DESP / sizeof PO_DESP[0]))
static int s_po_desp_i, s_po_dir;
static int s_po_fase, s_po_zona_i, s_po_nz, s_po_pregunta, s_po_hablo, s_po_lado;
static uint8_t s_po_zonas[ESC_COUNT + 1]; /* +1: al final, la zona de donde salieron (el paseo siempre te devuelve a casa) */
static int s_po_nvis; /* cuántas son del paseo; la que sobra al final es el regreso */
static uint32_t s_po_fade; /* cuándo empezó a aparecer (0: no) */
static int s_po_esc_ref = -1; /* en qué mundo está con ella (para saber si ella se fue sola) */
static float s_po_x, s_po_y;
static bool s_po_espejo, s_po_corre;
static uint32_t s_po_t0, s_po_zona_t0, s_po_chequeo, s_po_proxima;
static void po_toque_cb(lv_event_t *e);
#define PO_ESPERA_S (6u * 3600u) /* entre una visita y otra: cada 6 horas, como mínimo (antes cada tres días) */

/* el globo de Pochi: sale de él (no de la mascota) y es durazno con borde café, para que se note quién habla */
static lv_obj_t *s_pg, *s_pg_ico, *s_pg_txt, *s_pg_cola;
static art_img_t s_pg_art;
static uint32_t s_pg_hasta;
static void po_globo_crear(void)
{
    s_pg = caja(lv_layer_top());
    lv_obj_set_size(s_pg, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(s_pg, lv_color_hex(0x3A1E0C), 0); /* como los demás globos: translúcido con borde, pero en tonos cálidos (así se nota que habla Pochi) */
    lv_obj_set_style_bg_opa(s_pg, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_pg, 2, 0);
    lv_obj_set_style_border_color(s_pg, lv_color_hex(0xE8A860), 0);
    lv_obj_set_style_border_opa(s_pg, LV_OPA_90, 0);
    lv_obj_set_style_radius(s_pg, 14, 0);
    lv_obj_set_style_pad_hor(s_pg, 12, 0);
    lv_obj_set_style_pad_ver(s_pg, 8, 0);
    lv_obj_set_style_pad_column(s_pg, 8, 0);
    lv_obj_set_flex_flow(s_pg, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(s_pg, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_remove_flag(s_pg, LV_OBJ_FLAG_CLICKABLE);
    s_pg_ico = lv_image_create(s_pg);
    s_pg_txt = lv_image_create(s_pg);
    s_pg_cola = caja(lv_layer_top());
    lv_obj_set_size(s_pg_cola, 12, 12);
    lv_obj_set_style_bg_color(s_pg_cola, lv_color_hex(0x3A1E0C), 0);
    lv_obj_set_style_bg_opa(s_pg_cola, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_pg_cola, 2, 0);
    lv_obj_set_style_border_opa(s_pg_cola, LV_OPA_90, 0);
    lv_obj_set_style_border_side(s_pg_cola, LV_BORDER_SIDE_LEFT | LV_BORDER_SIDE_RIGHT | LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_color(s_pg_cola, lv_color_hex(0xE8A860), 0);
    lv_obj_remove_flag(s_pg_cola, LV_OBJ_FLAG_CLICKABLE);
    ocultar(s_pg, true); ocultar(s_pg_cola, true);
}
static bool po_globo_visible(void) { return s_pg && !lv_obj_has_flag(s_pg, LV_OBJ_FLAG_HIDDEN); }
static void po_globo_cerrar(void) { if (s_pg) { ocultar(s_pg, true); ocultar(s_pg_cola, true); s_pg_hasta = s_ms; } }
typedef struct { art_icono_t ico; char t[100]; uint32_t ms; } po_pend_t;
static po_pend_t s_po_pend[4];
static int s_po_pend_n;
/* (fw 108) ¿está ocupada con algo? Pochi no interrumpe: ni el baño, ni el picnic, ni la pecera, ni una fiesta o
 * ceremonia, ni cuando está pescando, nadando, trepando, cantando, bailando, hablando con un animal, jugando con
 * un juguete o con la pelota, con su amigo del barrio, probándose algo o durmiendo la siesta. */
static bool po_ocupada(void)
{
    if (!s_pet) return true;
    if (s_esponja_on || s_picnic_on || s_pecera || s_fie_tipo >= 0 || s_cie_activo || s_rito_on || s_portal) return true;
    /* (1.0.18) ni en un juego, ni en el bote, ni con un menú, el diario o los ajustes abiertos, ni mientras come */
    if (s_panel != PANEL_NINGUNO || s_juego != JG_NADA || zj_activo() || s_cru_activo || s_via_activo || s_mascota.modo == M_COME) return true;
    switch (s_pet->actividad) {
    case ACTV_PESCAR: case ACTV_NADAR: case ACTV_TREPAR: case ACTV_CANTAR: case ACTV_BAILAR:
    case ACTV_CHARLAR: case ACTV_JUGUETE: case ACTV_PELOTA: case ACTV_JUNTOS: case ACTV_PROBAR:
    case ACTV_SIESTA:
        return true;
    default: break;
    }
    return false;
}
static bool s_po_quiere; /* (fw 108) le tocaba llegar mientras estaba ocupada: llega apenas quede libre */


static bool po_en_transito(void) { return s_cru_activo || s_via_activo || s_zona_pend >= 0 || s_escena < 0 || s_po_tele || zona_activa(); } /* no se ve: ni diálogos ni globo */
static void po_di(const char *t, art_icono_t ico, uint32_t ms)
{
    if (s_interior || po_en_transito()) return;
    if (!s_pg) po_globo_crear();
    if (mundo_en_pausa() || (s_globo && !lv_obj_has_flag(s_globo, LV_OBJ_FLAG_HIDDEN)) || po_globo_visible()) { /* alguien está hablando: espera su turno */
        for (int i = 0; i < s_po_pend_n; i++) if (!strcmp(s_po_pend[i].t, t ? t : "")) return;
        if (s_po_pend_n >= 4) { for (int i = 1; i < 4; i++) s_po_pend[i - 1] = s_po_pend[i]; s_po_pend_n = 3; }
        s_po_pend[s_po_pend_n].ico = ico; s_po_pend[s_po_pend_n].ms = ms;
        snprintf(s_po_pend[s_po_pend_n].t, sizeof s_po_pend[0].t, "%s", t ? t : "");
        s_po_pend_n++;
        return;
    }
    const bool con_texto = t && t[0] && !s_nino;
    poner_src(s_pg_ico, art_icono(ico, con_texto ? 3 : 5));
    if (con_texto) {
        art_texto(&s_pg_art, t, 0xFFE8CC, 2, 230);
        poner_src(s_pg_txt, &s_pg_art.dsc);
    }
    ocultar(s_pg_txt, !con_texto);
    ocultar(s_pg, false); ocultar(s_pg_cola, false);
    lv_obj_move_foreground(s_pg); lv_obj_move_foreground(s_pg_cola);
    s_pg_hasta = s_ms + ms + (con_texto ? (uint32_t)strlen(t) * 40 : 0);
}
static void po_seguir_globo(void)
{
    if (!s_pg || lv_obj_has_flag(s_pg, LV_OBJ_FLAG_HIDDEN)) return;
    if (s_po_fase == PO_NADA || po_en_transito() || s_panel != PANEL_NINGUNO || s_cie_activo || s_sim_on || (int32_t)(s_ms - s_pg_hasta) > 0) { ocultar(s_pg, true); ocultar(s_pg_cola, true); return; }
    lv_obj_update_layout(s_pg);
    const int w = lv_obj_get_width(s_pg), h = lv_obj_get_height(s_pg);
    int x = (int)s_po_x - w / 2, y = (int)s_po_y - 74 - h - 10;
    if (x < 56) x = 56;
    if (x + w > W - 56) x = W - 56 - w;
    if (y < 60) y = 60;
    lv_obj_set_pos(s_pg, x, y);
    int cx = (int)s_po_x - 6;
    if (cx < x + 14) cx = x + 14;
    if (cx > x + w - 26) cx = x + w - 26;
    lv_obj_set_pos(s_pg_cola, cx, y + h - 3);
}
/* las papas: dos botones, ¡SÍ! o HOY NO (si no responde, se va con calma) */
static lv_obj_t *s_po_op;
static art_img_t s_po_op_a[2];
static bool s_po_hoyno;
static void po_op_ocultar(void) { if (s_po_op) ocultar(s_po_op, true); }
static void po_op_cb(lv_event_t *e)
{
    const int si = (int)(intptr_t)lv_event_get_user_data(e);
    po_op_ocultar();
    if (s_po_fase != PO_HAMBRE) return;
    if (si) { s_po_fase = PO_COME; s_po_t0 = s_ms; }
    else {
        po_di("ESTÁ BIEN... ¡VUELVO OTRO DÍA!", ICO_POCHI, 3200); po_responde("¡HASTA PRONTO, POCHI!", "¡VUELVE CUANDO QUIERAS!", 3400);
        s_po_hoyno = true; s_po_fase = PO_SE_VA; s_po_t0 = s_ms; s_po_lado = s_po_x < CX ? -1 : 1;
    }
}
static void po_op_mostrar(void)
{
    if (!s_po_op) {
        s_po_op = lv_obj_create(s_mundo);
        lv_obj_remove_style_all(s_po_op);
        lv_obj_set_size(s_po_op, 280, 48);
        lv_obj_remove_flag(s_po_op, LV_OBJ_FLAG_CLICKABLE);
        for (int k = 0; k < 2; k++) {
            lv_obj_t *b = lv_obj_create(s_po_op);
            lv_obj_remove_style_all(b);
            lv_obj_set_size(b, 130, 46);
            lv_obj_set_pos(b, k * 150, 0);
            ficha_magica(b, k ? 0xC86A6A : 0x6ADC8A, k ? 0x6A2A3A : 0x2E9A52, 23);
            lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_remove_flag(b, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_flex_flow(b, LV_FLEX_FLOW_ROW);
            lv_obj_set_flex_align(b, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
            lv_obj_set_style_pad_column(b, 6, 0);
            lv_obj_add_event_cb(b, po_op_cb, LV_EVENT_CLICKED, (void *)(intptr_t)(k ? 0 : 1));
            lv_obj_t *i = lv_image_create(b);
            lv_image_set_src(i, art_icono(k ? ICO_NO : ICO_SI, 2));
            lv_obj_remove_flag(i, LV_OBJ_FLAG_CLICKABLE);
            if (!s_nino) {
                lv_obj_t *l = lv_image_create(b);
                art_texto(&s_po_op_a[k], k ? "HOY NO" : "¡SÍ!", 0xFFF3C8, 2, 0);
                lv_image_set_src(l, &s_po_op_a[k].dsc);
                lv_obj_remove_flag(l, LV_OBJ_FLAG_CLICKABLE);
            }
        }
    }
    lv_obj_set_pos(s_po_op, CX - 140, 256);
    ocultar(s_po_op, false);
    lv_obj_move_foreground(s_po_op);
}
/* Pochi aparece DENTRO de la pantalla, cerca de ella (con un destello), y camina desde ahí: antes entraba corriendo por los bordes del mapa */
static void po_coloca(void)
{
    s_po_lado = s_mascota.x < CX ? 1 : -1;
    s_po_x = s_mascota.x + (float)s_po_lado * 110.0f;
    if (s_po_x < 60.0f) s_po_x = 60.0f;
    if (s_po_x > (float)(W - 60)) s_po_x = (float)(W - 60);
    s_po_y = s_mascota.y + 8;
    s_po_fade = s_ms ? s_ms : 1;
    for (int k = 0; k < 4; k++) chispa((int)s_po_x - 24 + k * 16, (int)s_po_y - 40 - (k % 2) * 14, 0xFFF3C8, 0, -0.4f);
}
static bool po_visible_cerca(void) { return s_po_x > 40.0f && s_po_x < (float)(W - 40) && hypotf(s_po_x - s_mascota.x, s_po_y - s_mascota.y) < 140.0f; }
static void po_empezar(void)
{
    if (!s_po_img) {
        s_po_img = lv_image_create(s_mundo);
        lv_obj_add_flag(s_po_img, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_ext_click_area(s_po_img, 14);
        lv_obj_add_event_cb(s_po_img, po_toque_cb, LV_EVENT_CLICKED, NULL);
        s_po_plato = lv_image_create(s_mundo);
        lv_image_set_src(s_po_plato, art_icono(ICO_POCHI_PLATO, 3));
        lv_obj_remove_flag(s_po_plato, LV_OBJ_FLAG_CLICKABLE);
        s_po_charco = lv_obj_create(s_mundo); /* el charco de barro (una mancha café) */
        lv_obj_set_size(s_po_charco, 120, 34);
        lv_obj_set_style_radius(s_po_charco, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(s_po_charco, lv_color_hex(0x6B4A2B), 0);
        lv_obj_set_style_bg_opa(s_po_charco, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(s_po_charco, 3, 0);
        lv_obj_set_style_border_color(s_po_charco, lv_color_hex(0x4A311B), 0);
        lv_obj_remove_flag(s_po_charco, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(s_po_charco, LV_OBJ_FLAG_HIDDEN);
    }
    lv_image_set_pivot(s_po_img, 36, 36); /* (fw 102) gira sobre su centro: panza arriba, mareada */
    po_coloca();
    s_po_fase = PO_LLEGA; s_po_t0 = s_ms; s_po_pregunta = 0; s_po_hablo = 0; s_po_esc_ref = s_escena;
    s_po_charla_k = -1; s_po_tilo_ladro = false;
    /* (fw 102) lo de esta visita: si viene un cachorro, cómo terminó la anterior, y nada hecho todavía */
    s_po_ult = s_pet ? pet_pochi_last(s_pet) : 0;
    s_po_comio = s_po_paseo_ok = s_po_mania_hecha = s_po_rasco = s_po_panzo = s_po_rapido = false;
    s_po_truco = TR_NADA; s_po_mania = -1; s_po_mania_try = -1; s_po_toques = 0;
    s_pa_on = false; s_pa_hecha = false;
    po_cosa(-1, 0, 0, 0);
    pc_empezar();
    { /* (1.0.30) qué trae esta visita (la primera vez y si quedó sentida, la de siempre; nunca dos iguales seguidas) */
        int tp = PT_COMPLETA;
        const bool primera = s_pet && pet_pochi_visits(s_pet) == 0;
        const bool sentida = s_po_ult == PET_POCHI_ULT_COMIO || s_po_ult == PET_POCHI_ULT_HOYNO;
        if (s_po_tipo_forzar >= 0) tp = s_po_tipo_forzar;
        else if (!primera && !sentida && !s_po_dormida_visita)
            for (int intento = 0; intento < 3; intento++) {
                const uint32_t r = azar() % 100u;
                tp = r < 28 ? PT_COMPLETA : r < 48 ? PT_PAPAS : r < 66 ? PT_PASEO : r < 84 ? PT_TAPITA : PT_SALUDA;
                if (tp != s_po_tipo_ant) break;
            }
        if (tp == PT_SALUDA && s_pc_n == 0) pc_solo_uno((int)(azar() % PET_POCHI_CACHORROS)); /* a saludar viene con uno de sus hijos, por lo menos */
        s_po_tipo = tp; s_po_tipo_forzar = -1;
        if (!s_po_dormida_visita) s_po_tipo_ant = tp;
    }
    ocultar(s_po_img, false); ocultar(s_po_plato, true);
    if (!s_po_dormida_visita) accion(UI_ACCION_POCHI, 0); /* (durmiendo no cuenta: se acurruca y nada más) */
}
static void po_a_paseo(void)
{
    s_po_nz = 0;
    for (int e = 0; e < ESC_COUNT; e++) if (s_pet && pet_scene_open(s_pet, (escena_t)e) && !ESC_ES_SECRETA(e) && e != ESC_ISLA) s_po_zonas[s_po_nz++] = (uint8_t)e; /* (v98: a los lugares del rey no la lleva de paseo; 1.0.23: a la isla tampoco, no sabe nadar) */
    s_po_nvis = s_po_nz;
    if (s_pet && s_po_nz > 0 && s_po_zonas[s_po_nz - 1] != (uint8_t)s_pet->escena) s_po_zonas[s_po_nz++] = (uint8_t)s_pet->escena; /* el último paso: volver al mundo del que salieron (a pie o por el portal) */
    s_po_zona_i = -1; s_po_fase = PO_PASEO; s_po_t0 = s_ms; s_po_zona_t0 = 0;
    s_po_paseo_ok = true;
    po_di("¡VAMOS, VAMOS! ¡TE LLEVO A CONOCER!", ICO_PATITA, 3200);
    accion(UI_ACCION_SONAR, SND_POCHI_FELIZ);
    flotar(ICO_CORAZON, NULL, 0, (int)s_po_x, (int)s_po_y - 70, 0);
}
static void po_toque_cb(lv_event_t *e)
{
    (void)e;
    if (s_po_truco) po_toque(); /* (panza arriba: la rascas) */
    else if (s_po_fase == PO_HAMBRE) { s_po_fase = PO_COME; s_po_t0 = s_ms; }
    else if (s_po_fase == PO_INVITA) po_a_paseo();
    else if (s_po_fase != PO_NADA) po_toque(); /* (fw 102) uno ladra; tres, panza arriba; seis, se marea */
}
static const char *const PO_ZONA[ESC_COUNT][2] = {
    {"¡HUELE A PASTO FRESQUITO!", "¡AQUÍ TRAIGO A CORRER A MIS CACHORROS!"},
    {"¡CUÁNTOS COIHUES! ¿HABRÁ MONITOS DEL MONTE?", "SHH... ¿OYES A LOS PÁJAROS?"},
    {"¡QUÉ AGUA TAN BONITA! ¡GUAU!", "¡NO ME MOJO LAS PATITAS!"},
    {"¡DESDE AQUÍ SE VE TODO!", "¡EL VIENTO ME MUEVE LAS OREJAS!"},
    {"¡UY, QUÉ OSCURO! YO TE CUIDO", "¡MIRA QUÉ BRILLANTES LOS CRISTALES!"},
    {"¡UNA ISLA! ¡QUÉ AVENTURA!", "¡LA ARENA ESTÁ TIBIECITA!"},
    {"¡LA CASA DEL REY! ¡QUÉ PUERTA TAN BONITA!", "¿OYES AL CHUCAO? ¡CHU-CAO!"},
    {"¡QUÉ ALTO! ¡SE VE TODO EL VALLE!", "¡HOLA, CÓNDOR! ¡QUÉ GRANDE ERES!"}};
/* con nieve, hielo o frío ya no huele a pasto fresco ni se oyen pájaros que no hay */
static const char *const PO_ZONA_FRIO[ESC_COUNT][2] = {
    {"¡BRRR! ¡LA NIEVE HACE CRUJ-CRUJ!", "¡LA NIEVE TAPÓ TODO! ¡PERO ME GUSTA!"},
    {"¡CUÁNTA NIEVE EN LOS ÁRBOLES!", "SHH... QUÉ SILENCIO, TODO QUIETITO"},
    {"¡EL AGUA ESTÁ DURITA! ¡ES HIELO!", "¡CUIDADO, QUE SE RESBALA!"},
    {"¡BRRR! ¡DESDE AQUÍ SE VE TODO BLANCO!", "¡EL VIENTO FRÍO ME MUEVE LAS OREJAS!"},
    {"¡UY, QUÉ OSCURO! YO TE CUIDO", "¡MIRA QUÉ BRILLANTES LOS CRISTALES!"},
    {"¡UNA ISLA! ¡QUÉ AVENTURA!", "¡LA ARENA ESTÁ FRÍA! ¡BRRR!"},
    {"¡LA CASA DEL REY TIENE NIEVE EN EL TECHO!", "¡LA POZA ESTÁ CONGELADA!"},
    {"¡BRRR! ¡AQUÍ ARRIBA HACE MUCHO FRÍO!", "¡TODO EL VALLE ESTÁ BLANCO!"}};
static uint32_t s_po_pedido; /* cuándo pidió que apareciera un pajarito (0: no) */
static void po_mover(float tx, float ty, float vel)
{
    float dx = tx - s_po_x, dy = ty - s_po_y, d = hypotf(dx, dy);
    s_po_corre = d > vel * s_po_dtf + 1;
    vel *= s_po_dtf;
    if (d > vel) { s_po_x += dx / d * vel; s_po_y += dy / d * vel; if (fabsf(dx) > 1) s_po_espejo = dx < 0; }
    else { s_po_x = tx; s_po_y = ty; }
}
static void po_barro_empezar(void)
{
    s_po_fase = PO_BARRO; s_po_t0 = s_ms; s_po_pregunta = 0;
    s_po_cx = s_mascota.x + (float)(s_po_x < s_mascota.x ? -100 : 100);
    if (s_po_cx < 70) s_po_cx = 70;
    if (s_po_cx > W - 70) s_po_cx = (float)(W - 70);
    s_po_cy = s_mascota.y + 26;
}
/* el portal de Pochi: lo invoca, entran los dos y salen juntos en la zona nueva */
static lv_obj_t *s_po_pt;
static int s_po_pf; /* 0 no; 1 abre y entran; 2 viajando; 3 salen */
static uint32_t s_po_pt0;
static float s_po_px, s_po_py;
static void po_portal_tam(float f) /* 0..1 */
{
    if (!s_po_pt) {
        s_po_pt = lv_obj_create(s_mundo);
        lv_obj_remove_style_all(s_po_pt);
        lv_obj_remove_flag(s_po_pt, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_radius(s_po_pt, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(s_po_pt, lv_color_hex(0x7048D8), 0);
        lv_obj_set_style_bg_opa(s_po_pt, 215, 0);
        lv_obj_set_style_border_width(s_po_pt, 4, 0);
        lv_obj_set_style_border_color(s_po_pt, lv_color_hex(0xF4E8FF), 0);
        lv_obj_set_style_shadow_width(s_po_pt, 36, 0);
        lv_obj_set_style_shadow_color(s_po_pt, lv_color_hex(0xB894FF), 0);
        lv_obj_set_style_shadow_opa(s_po_pt, 255, 0);
    }
    if (f <= 0.02f) { ocultar(s_po_pt, true); return; }
    const int w = (int)(70.0f * f), h = (int)(112.0f * f);
    lv_obj_set_size(s_po_pt, w, h);
    lv_obj_set_pos(s_po_pt, (int)s_po_px - w / 2, (int)s_po_py - 46 - h / 2);
    ocultar(s_po_pt, false);
    lv_obj_move_foreground(s_po_pt);
}
static void po_opa(int a)
{
    if (a < 0) a = 0;
    if (a > 255) a = 255;
    lv_obj_set_style_opa(s_po_img, (lv_opa_t)a, 0);
    if (s_mascota.img) lv_obj_set_style_opa(s_mascota.img, (lv_opa_t)a, 0);
    if (s_mascota.sombra) lv_obj_set_style_opa(s_mascota.sombra, (lv_opa_t)a, 0);
}
static void po_portal_fin(void)
{
    s_po_pf = 0; s_po_tele = false;
    if (s_po_pt) ocultar(s_po_pt, true);
    po_opa(255);
}
static void po_portal_empezar(void)
{
    s_po_pf = 1; s_po_pt0 = s_ms;
    s_po_px = (s_mascota.x + s_po_x) / 2; s_po_py = s_mascota.y + 8;
    po_di("¡MIRA, UN PORTAL MÁGICO! ¡ENTREMOS!", ICO_POCHI, 2400);
    accion(UI_ACCION_POCHI, 2);
}
static void po_portal_paso(art_icono_t *ico, int dest)
{
    const uint32_t t = s_ms - s_po_pt0;
    *ico = (s_ms / 200) % 2 ? ICO_POCHI : ICO_POCHI_B;
    if (s_po_pf == 1) {
        po_mover(s_po_px, s_po_py, 1.6f);
        po_portal_tam(t < 600 ? (float)t / 600.0f : 1.0f);
        if (t > 700) {
            po_opa(255 - (int)((t - 700) * 255 / 900));
            const float a = (float)s_ms / 90.0f;
            chispa((int)(s_po_px + cosf(a) * 36), (int)(s_po_py - 46 + sinf(a) * 54), 0xD8C8FF, (s_po_px - s_mascota.x) * 0.01f, 0);
        }
        if (t > 1600) { po_opa(0); s_po_tele = true; accion(ACT_IR, dest); s_po_pf = 2; s_po_pt0 = s_ms; }
    } else if (s_po_pf == 2) {
        po_opa(0);
        po_portal_tam(t < 300 ? 1.0f - (float)t / 300.0f : 0);
        if ((s_escena == dest) || t > 3000) {
            s_po_tele = false;
            s_po_pf = 3; s_po_pt0 = s_ms;
            s_po_px = s_mascota.x + 34; s_po_py = s_mascota.y + 8;
            s_po_x = s_po_px; s_po_y = s_po_py; s_po_lado = 1; s_po_espejo = false;
        }
    } else {
        s_po_x = s_po_px; s_po_y = s_po_py;
        po_portal_tam(t < 450 ? (float)t / 450.0f : (t < 1400 ? 1.0f : 1.0f - (float)(t - 1400) / 300.0f));
        po_opa((int)(t > 300 ? (t - 300) * 255 / 700 : 0));
        if (t < 900) chispa((int)(s_po_px + cosf((float)s_ms / 80.0f) * 34), (int)(s_po_py - 46 + sinf((float)s_ms / 80.0f) * 52), 0xD8C8FF, 0, -0.3f);
        if (t > 1700) {
            po_portal_fin();
            s_po_zona_t0 = s_ms; s_po_hablo = 0;
        }
    }
}
static bool po_activo(void) { return s_po_fase != PO_NADA; }
/* --- las patas de barro: después de revolcarse, Pochi deja huellas SOLO por donde camina; duran un par de minutos --- */
#define N_PATAS 18
#define PATA_VIDA_MS 120000u /* dos minutos en el piso */
static lv_obj_t *s_pata[N_PATAS];
static uint32_t s_pata_t[N_PATAS];
static int s_pata_esc[N_PATAS];
static int s_pata_i, s_pata_lado, s_po_barro_pasos; /* pasos de barro que le quedan en las patas */
static float s_pata_x0, s_pata_y0;
static void po_patas_actualizar(void)
{
    for (int i = 0; i < N_PATAS; i++) {
        if (!s_pata[i] || lv_obj_has_flag(s_pata[i], LV_OBJ_FLAG_HIDDEN)) continue;
        const uint32_t edad = s_ms - s_pata_t[i];
        if (edad > PATA_VIDA_MS || s_interior || s_escena != s_pata_esc[i]) { ocultar(s_pata[i], true); continue; } /* (en otra zona no se ven) */
        lv_obj_set_style_opa(s_pata[i], edad > PATA_VIDA_MS - 20000u ? (lv_opa_t)(200u * (PATA_VIDA_MS - edad) / 20000u) : 200, 0); /* se van borrando al final */
    }
}
static void po_pata_dejar(float x, float y, bool espejo)
{
    if (s_interior || po_en_transito() || s_escena < 0) return;
    if (s_po_barro_pasos <= 0 && !s_po_nieve) return; /* (fw 102) en la nieve deja huellitas siempre */
    if (hypotf(x - s_pata_x0, y - s_pata_y0) < 24.0f) return; /* solo si de verdad se movió: nunca antes de que camine por ahí */
    { /* en el agua no se marcan patas: ni en el estanque del prado, ni fuera de lo que se puede pisar (lago, isla, orillas) */
        float cx = x, cy = y;
        limitar(&cx, &cy);
        if (hypotf(cx - x, cy - y) > 5.0f) return;
        if (s_escena == ESC_PRADO) { const float dx = (x - 106.0f) / 74.0f, dy = (y - 300.0f) / 50.0f; if (dx * dx + dy * dy < 1.0f) return; }
    }
    s_pata_x0 = x; s_pata_y0 = y;
    const bool barro = s_po_barro_pasos > 0;
    if (barro) s_po_barro_pasos--;
    const int i = s_pata_i++ % N_PATAS;
    if (!s_pata[i]) {
        s_pata[i] = lv_image_create(s_mundo);
        lv_image_set_src(s_pata[i], art_icono(ICO_HUELLA, 2));
        lv_image_set_scale(s_pata[i], 200);
        lv_obj_remove_flag(s_pata[i], LV_OBJ_FLAG_CLICKABLE);
    }
    lv_obj_set_style_image_recolor(s_pata[i], lv_color_hex(0x6F88AE), 0); /* en la nieve: huellitas azuladas (hundidas) */
    lv_obj_set_style_image_recolor_opa(s_pata[i], barro ? 0 : 170, 0);
    s_pata_lado = -s_pata_lado;
    lv_obj_set_pos(s_pata[i], (int)x - 12 + (espejo ? 10 : -10), (int)y - 12 + s_pata_lado * 6);
    lv_image_set_pivot(s_pata[i], 12, 12);
    lv_image_set_rotation(s_pata[i], espejo ? 700 : 2900);
    s_pata_t[i] = s_ms; s_pata_esc[i] = s_escena;
    ocultar(s_pata[i], false);
    lv_obj_move_to_index(s_pata[i], 1); /* en el piso: debajo de ella y de Pochi */
}

static lv_obj_t *s_po_luz; /* la luciérnaga que persigue (manía del claro) */
/* ===================== (fw 102) Pochi con más personalidad ===================== */
static void po_cosa(int ico, int esc, float x, float y) /* lo que lleva o persigue (ico < 0: se esconde) */
{
    if (ico < 0) { if (s_po_cosa) ocultar(s_po_cosa, true); s_po_cosa_ico = -1; return; }
    if (!s_po_cosa) { s_po_cosa = lv_image_create(s_mundo); lv_obj_remove_flag(s_po_cosa, LV_OBJ_FLAG_CLICKABLE); }
    if (ico != s_po_cosa_ico) { s_po_cosa_ico = ico; lv_image_set_src(s_po_cosa, art_icono((art_icono_t)ico, esc)); }
    s_po_cosa_x = x; s_po_cosa_y = y;
    const int L = 12 * esc;
    lv_obj_set_pos(s_po_cosa, (int)x - L / 2, (int)y - L / 2);
    ocultar(s_po_cosa, s_interior || po_en_transito());
    lv_obj_move_foreground(s_po_cosa);
}
static bool po_hay_pasto(void) { return s_escena != ESC_CUEVA && !s_interior; }
/* los trucos solos: fuera de la cueva, sin charla ni manía, y no en medio de un viaje */
static bool po_truco_puede(void)
{
    if (s_po_truco || s_po_pf || po_en_transito() || s_interior) return false;
    return s_po_fase == PO_CORRE || s_po_fase == PO_INVITA || s_po_fase == PO_PASEO || s_po_fase == PO_DESPIDE;
}
static void po_truco(int k, uint32_t dur)
{
    s_po_truco = k; s_po_truco_t0 = s_ms; s_po_truco_dur = dur; s_po_rascada = false;
    if (k == TR_RASCA) s_po_espejo = s_po_x > CX; /* se arrastra hacia el medio */
}
static const char *po_buf(const char *fmt, const char *a) { static char b[4][110]; static int i; i = (i + 1) & 3; snprintf(b[i], sizeof b[i], fmt, a); return b[i]; }
static void po_rasca_empezar(void)
{
    po_truco(TR_RASCA, 3000);
    const bool frio = s_pet && zona_fria(s_pet);
    static const char *const T[3] = {"¡AY, ME PICA! ¡RASCA, RASCA!", "¡QUÉ RICO EL PASTITO!", "¡ESTE PASTO RASCA MEJOR QUE NADA EN EL MUNDO MUNDIAL!"};
    po_di(frio ? "¡LA NIEVE ESTÁ HELADITA EN EL POTO! ¡BRRR!" : (s_escena == ESC_ISLA ? "¡LA ARENA RASCA RIQUÍSIMO!" : T[azar() % 3]), ICO_POCHI, 2800);
    po_responde("¡POCHI! ¡QUÉ COCHINA!", "¡JAJAJA, POCHI!", 3000);
    accion(UI_ACCION_SONAR, SND_POCHI_FELIZ);
}
static void po_panza_empezar(bool por_toques)
{
    po_truco(TR_PANZA, 5200);
    po_di(por_toques ? "¡RASCA, RASCA LA PANCITA!" : "¡MIRA! ¡PANZA ARRIBA! ¿ME RASCAS?", ICO_CORAZON, 2600);
    accion(UI_ACCION_SONAR, SND_POCHI_FELIZ);
}
/* la manía de cada lugar (-1: aquí no tiene) */
static int po_mania_de(int esc)
{
    if (esc == ESC_PRADO) return MAN_RAMITA;
    if (esc == ESC_BOSQUE) return s_fondo_periodo == PERIODO_NOCHE ? MAN_LUZ : MAN_RAMITA;
    if (esc == ESC_CLARO) return MAN_LUZ;
    if (esc == ESC_MIRADOR) return MAN_CONDOR;
    if (esc == ESC_LAGO || esc == ESC_ISLA) return MAN_AGUA;
    return -1;
}
static void po_mania_empezar(int m, int vuelve)
{
    s_po_mania = m; s_po_mania_vuelve = vuelve; s_po_mst = 0;
    s_po_fase = PO_MANIA; s_po_t0 = s_ms; s_po_mania_hecha = true; s_po_hablo = 0;
    if (m == MAN_RAMITA) { /* la ramita, en el suelo un poco más allá */
        float x = s_po_x + (s_po_x < CX ? 70.0f : -70.0f), y = s_po_y + 4;
        limitar(&x, &y);
        po_cosa(ICO_RAMITA, 3, x, y - 6);
    }
}
static void po_mania_fin(void)
{
    po_cosa(-1, 0, 0, 0);
    if (s_po_luz) ocultar(s_po_luz, true);
    s_po_fase = s_po_mania_vuelve; s_po_t0 = s_ms; s_po_pregunta = 0;
    if (s_po_fase == PO_PASEO) { s_po_zona_t0 = s_ms > 12000u ? s_ms - 12000u : 1u; s_po_hablo = 2; }
}
/* el saludo al llegar: según cuántas veces vino, cómo terminó la vez anterior, la hora y el clima */
static void po_saludo_cachorro(void);
static void po_saludo(void)
{
    if (!s_pet) return;
    const int v = pet_pochi_visits(s_pet), h = hora_de(s_pet, s_ultimo_t);
    const clima_t c = pet_weather(s_pet);
    const char *t;
    s_po_rapido = false;
    if (v <= 1) t = zona_fria(s_pet) ? "¡BRRR! ¡SOY POCHI! ¡QUIERO PAPAS CALENTITAS!" : "¡GUAU! ¡SOY POCHI! ¡QUIERO PAPAS!";
    else if (s_po_ult == PET_POCHI_ULT_PASEO) { t = "¡VOLVÍ! ¡LA OTRA VEZ LO PASAMOS REGIO! ¿HOY TAMBIÉN HAY PAPAS?"; s_po_rapido = true; } /* llega directo al plato */
    else if (v >= 10 && azar() % 2) t = "¡MI AMIGA FAVORITA DEL MUNDO MUNDIAL! ¿HAY PAPAS?";
    else if (zona_fria(s_pet)) t = "¡BRRR! ¡QUIERO PAPAS CALENTITAS!";
    else if (c == CLIMA_VIENTO) t = "¡EL VIENTO CASI ME VUELA LAS OREJAS! ¿HAY PAPAS?";
    else if (c == CLIMA_NUBLADO) t = "¡QUÉ DÍA MÁS GRIS! ¡UNAS PAPAS LO ARREGLAN!";
    else if (h < 11) t = "¡BUENOS DÍAS! ¡YA LES DI DESAYUNO A MIS CACHORROS! ¿Y MIS PAPAS?";
    else if (h >= 19) t = "¡VINE ANTES DE ACOSTAR A MIS CACHORROS! ¿HAY PAPAS?";
    else t = "¡QUÉ SOLCITO MÁS RICO! ¡GUAU! ¿HAY PAPAS?";
    po_di(t, ICO_POCHI, 4200);
    po_responde("¡HOLA, POCHI! ¡YO TE DOY PAPAS!", "¡HOLA, POCHI! ¡QUÉ BUENO QUE VINISTE!", 4400);
    accion(UI_ACCION_SONAR, SND_POCHI_LADRA);
    po_saludo_cachorro();
}
static void po_hambre(void) { s_po_fase = PO_HAMBRE; s_po_t0 = s_ms; s_po_pregunta = 0; s_po_espejo = s_po_x > s_mascota.x; }
static void po_tapita_empezar(void);
/* (1.0.30) ya llegó (y saludó, consoló o se sacudió): lo que trae esta visita */
static void po_tras_llegar(void)
{
    if (s_po_tipo == PT_COMPLETA || s_po_tipo == PT_PAPAS) { po_saludo(); po_hambre(); return; }
    s_po_comio = true; /* (sin papas: igual sus cachorros tienen su momento) */
    s_po_espejo = s_po_x > s_mascota.x; s_po_t0 = s_ms; s_po_pregunta = 0;
    accion(UI_ACCION_SONAR, SND_POCHI_LADRA);
    if (s_po_tipo == PT_PASEO) {
        po_di("¡HOLA! ¡HOY NO VENGO POR PAPAS: VENGO A INVITARTE A PASEAR!", ICO_PATITA, 3800);
        po_responde("¡HOLA, POCHI! ¡QUÉ ENTRETENIDO!", "¡HOLA, POCHI!", 4000);
        po_saludo_cachorro();
        s_po_fase = PO_INVITA; s_po_t0 = s_ms + 2500u; /* (un poco más de tiempo antes de preguntar) */
    } else if (s_po_tipo == PT_TAPITA) {
        po_di("¡HOLA! ¡MIRA LO QUE ENCONTRÉ: UNA TAPITA! ¿ME LA TIRAS?", ICO_CHAPITA, 3600);
        po_responde("¡HOLA, POCHI! ¡SÍ, JUGUEMOS!", "¡HOLA, POCHI!", 3800);
        po_saludo_cachorro();
        po_tapita_empezar();
    } else { /* PT_SALUDA: viene, saluda y te presenta a sus cachorros */
        po_di("¡HOLA! ¡SOLO PASABA A SALUDARTE!", ICO_CORAZON, 3000);
        po_responde("¡HOLA, POCHI! ¡QUÉ LINDA SORPRESA!", "¡HOLA, POCHI!", 3200);
        po_saludo_cachorro();
        s_po_fase = PO_INVITA;
    }
}
/* al llegar junto a ella: ¿qué le pasa? (triste o enferma: consuela; lluvia: llega mojada; la otra vez no la acompañaste: ofendida) */
static void po_llega_fin(void)
{
    s_po_espejo = s_po_x > s_mascota.x; s_po_t0 = s_ms; s_po_pregunta = 0;
    const mood_t m = s_pet ? pet_mood(s_pet) : MOOD_FELIZ;
    if (s_pet && (m == MOOD_TRISTE || m == MOOD_ENFERMA || s_pet->enferma)) { s_po_fase = PO_CONSUELA; return; }
    if (s_pet && pet_weather(s_pet) == CLIMA_LLUVIA && po_hay_pasto()) { s_po_fase = PO_MOJADA; return; }
    if (s_po_ult == PET_POCHI_ULT_COMIO || s_po_ult == PET_POCHI_ULT_HOYNO) { s_po_fase = PO_OFENDIDA; return; }
    po_tras_llegar();
}

/* ---- los cachorros de esta visita (uno, dos o los cuatro): siguen a su mamá, cada uno tiene su momento, juegan entre ellos ----
 * Los cuatro son igual de importantes: cada uno tiene un momento de duración parecida y un juego propio para cuando está libre
 * (Chapi se revuelca, Pasita olfatea florcitas, Morita se esconde y se asoma, Rony corre en círculos). */
#define PC_ESC 4 /* (Pochi va a 6: los cachorros, más chicos) */
#define PC_MAX PET_POCHI_CACHORROS
enum { PC_SIGUE = 0, PC_TURNO, PC_PANZA, PC_RASCA, PC_PROPIO, PC_FAM, PC_ESCONDIDO, PC_CARGADO, PC_ANIMAL };
static bool pa_cachorro(int i, int *f, float *oy, int *rot); /* (más abajo: el encuentro con un animal) */
enum { FAM_PILLARSE = 0, FAM_TIRA, FAM_SIESTA, FAM_ESCONDE, FAM_LAME };
typedef struct {
    int k;                      /* POCHI_CHAPI.. */
    lv_obj_t *img, *cosa;
    float x, y, cx, cy, cvx, cvy;
    bool esp, hecho;
    int modo, paso, hito, cosa_k, ult_ico, ult_rot;
    bool ult_esp;
    uint32_t t0, chequeo;
    uint32_t lengua_hasta, lengua_prox; /* (solo Pochi anda siempre con la lengua afuera: ellos, de vez en cuando) */
} cach_t;
static cach_t s_pc[PC_MAX];
static int s_pc_n;                          /* cuántos vinieron */
static int s_pc_turno = -1;                 /* quién está en su momento (índice en s_pc) */
static uint32_t s_pc_libre;                 /* desde cuándo nadie está en su momento */
static int s_pc_fam = -1, s_pc_fa, s_pc_fb; /* la escena de familia de esta visita y quiénes */
static uint32_t s_pc_fam_t0;
static bool s_pc_fam_hecha;
static int s_pc_esc = -1, s_pc_esc_res;     /* el que se escondió al despedirse (y si lo encontraste: 1, o salió solo: 2) */
static uint32_t s_pc_esc_t0;
static float s_pc_esc_x, s_pc_esc_y;
static lv_obj_t *s_pc_esc_toque; /* dónde se escondió: no se le ve, solo se mueve el pasto ahí */
static int s_pc_carga = -1;                 /* el que no se quiere ir: Pochi se lo lleva en la boca */
static bool s_pc_esc_forzar;               /* (pruebas) que se esconda sí o sí */
static int s_pc_fam_forzar = -1;           /* (pruebas) esta escena de familia */
static const char *const PC_NOMBRE[PET_POCHI_CACHORROS] = {"CHAPI", "PASITA", "MORITA", "RONY"};
static const bool PC_ELLA[PET_POCHI_CACHORROS] = {false, true, true, false};
static art_icono_t pc_ico(int k, int f) { return (art_icono_t)(ICO_CACH_CHAPI + k * 5 + f); }
/* quieto: con la boca cerrada; de vez en cuando (y después de jugar) saca la lengua un ratito moviendo la cola */
static int pc_quieto(cach_t *c)
{
    if ((int32_t)(s_ms - c->lengua_prox) > 0) { c->lengua_hasta = s_ms + 2500u; c->lengua_prox = s_ms + 12000u + azar() % 14000u; }
    return (int32_t)(c->lengua_hasta - s_ms) > 0 ? 1 : 0;
}
static void pc_cansado(cach_t *c) { c->lengua_hasta = s_ms + 3200u; } /* después de correr o jugar: jadea */
static void pc_toque_cb(lv_event_t *e)
{
    const int i = (int)(intptr_t)lv_event_get_user_data(e);
    if (i < 0 || i >= s_pc_n) return;
    cach_t *c = &s_pc[i];
    static const char *const T[PET_POCHI_CACHORROS] = {"¡CHAPI ADORA LOS CARIÑOS!", "¡A PASITA LE ENCANTA QUE LA ACARICIEN!",
                                                       "¡MORITA SE PUSO COLORADA!", "¡RONY QUIERE JUGAR, JUGAR Y JUGAR!"};
    accion(UI_ACCION_SONAR, SND_POCHI_LADRA);
    flotar(ICO_CORAZON, NULL, 0, (int)c->x, (int)c->y - 50, 0);
    static uint32_t ult[PC_MAX];
    if (s_ms - ult[i] > 6000) { ult[i] = s_ms; po_di(T[c->k], ICO_CORAZON, 2400); }
}
static void pc_mover(cach_t *c, float tx, float ty, float vel)
{
    const float dx = tx - c->x, dy = ty - c->y, d = hypotf(dx, dy);
    vel *= s_po_dtf;
    if (d > vel) { c->x += dx / d * vel; c->y += dy / d * vel; if (fabsf(dx) > 1) c->esp = dx < 0; c->paso = 1; }
    else { c->x = tx; c->y = ty; c->paso = 0; }
}
static void pc_cosa(cach_t *c, int ico, float x, float y)
{
    if (ico < 0) { if (c->cosa) ocultar(c->cosa, true); return; }
    if (!c->cosa) { c->cosa = lv_image_create(s_mundo); lv_obj_remove_flag(c->cosa, LV_OBJ_FLAG_CLICKABLE); }
    lv_image_set_src(c->cosa, art_icono((art_icono_t)ico, 2));
    lv_obj_set_pos(c->cosa, (int)x - 12, (int)y - 12);
    ocultar(c->cosa, s_interior || po_en_transito());
    lv_obj_move_foreground(c->cosa);
}
static void pc_esconder(void)
{
    for (int i = 0; i < PC_MAX; i++) { if (s_pc[i].img) ocultar(s_pc[i].img, true); pc_cosa(&s_pc[i], -1, 0, 0); }
    if (s_pc_esc_toque) ocultar(s_pc_esc_toque, true);
}
/* dónde va cada uno detrás de su mamá */
static void pc_puesto(int i, float *x, float *y)
{
    static const int8_t P[PC_MAX][2] = {{-46, 10}, {-82, 22}, {-40, 32}, {-86, -2}};
    *x = s_po_x + (float)(s_po_espejo ? -P[i][0] : P[i][0]); *y = s_po_y + P[i][1];
}
static void pc_empezar(void)
{
    int o[PC_MAX];
    s_pc_n = s_pet ? pet_pochi_pups_pick(s_pet, s_ultimo_t, o) : 0;
    s_pc_turno = -1; s_pc_libre = s_ms; s_pc_fam = -1; s_pc_fam_hecha = false; s_pc_esc = -1; s_pc_carga = -1;
    pc_esconder();
    for (int i = 0; i < s_pc_n; i++) {
        cach_t *c = &s_pc[i];
        c->k = o[i];
        if (!c->img) {
            c->img = lv_image_create(s_mundo);
            lv_obj_add_flag(c->img, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_ext_click_area(c->img, 10);
            lv_obj_add_event_cb(c->img, pc_toque_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
            lv_image_set_pivot(c->img, 6 * PC_ESC, 6 * PC_ESC);
        }
        c->ult_ico = -1; c->ult_rot = 0; lv_image_set_rotation(c->img, 0);
        pc_puesto(i, &c->x, &c->y);
        c->modo = PC_SIGUE; c->hecho = false; c->chequeo = s_ms + (uint32_t)i * 300u; c->paso = 0;
        c->lengua_hasta = 0; c->lengua_prox = s_ms + 4000u + azar() % 12000u;
        c->cosa_k = (int)(azar() % 4); /* (1.0.36) lo que te trae Rony se elige al llegar: el saludo ya lo nombra */
    }
}
static void pc_solo_uno(int k) /* (1.0.30) viene con este cachorro (para la visita de saludo, si no venía ninguno) */
{
    s_pc_n = 1;
    cach_t *c = &s_pc[0];
    c->k = k;
    if (!c->img) {
        c->img = lv_image_create(s_mundo);
        lv_obj_add_flag(c->img, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_ext_click_area(c->img, 10);
        lv_obj_add_event_cb(c->img, pc_toque_cb, LV_EVENT_CLICKED, (void *)(intptr_t)0);
        lv_image_set_pivot(c->img, 6 * PC_ESC, 6 * PC_ESC);
    }
    c->ult_ico = -1; c->ult_rot = 0; lv_image_set_rotation(c->img, 0);
    pc_puesto(0, &c->x, &c->y);
    c->modo = PC_SIGUE; c->hecho = false; c->chequeo = s_ms; c->paso = 0;
    c->lengua_hasta = 0; c->lengua_prox = s_ms + 4000u + azar() % 12000u;
    c->cosa_k = (int)(azar() % 4); /* (1.0.36) */
}
static void pc_reubicar(void) /* (cambió la zona sin ellos: aparecen junto a su mamá) */
{
    for (int i = 0; i < s_pc_n; i++) { pc_puesto(i, &s_pc[i].x, &s_pc[i].y); if (s_pc[i].modo != PC_CARGADO) s_pc[i].modo = PC_SIGUE; pc_cosa(&s_pc[i], -1, 0, 0); }
    s_pc_turno = -1; s_pc_fam = -1;
}
/* su momento con ella (una vez por visita, cuando su mamá ya comió; de a uno). Todos duran parecido (~10 s) */
static const art_icono_t PC_COSAS[4] = {ICO_PIEDRA, ICO_HOJA, ICO_PLUMA, ICO_CHAPITA};
static const char *const PC_COSAS_N[4] = {"PIEDRITA", "HOJITA", "PLUMITA", "TAPITA"}; /* (1.0.36: «TAPITA», como la de Pochi) */
static bool pc_turno(cach_t *c, int *f, float *oy, int *rot) /* false: terminó */
{
    const uint32_t t = s_ms - c->t0;
    const float mx = s_mascota.x, my = s_mascota.y + 10;
    const float lado = c->x < mx ? -1.0f : 1.0f;
    if (c->k == POCHI_RONY) { /* persigue una cosita chiquita, la patea, y se la trae */
        if (c->hito == 0) {
            c->hito = 1; if ((unsigned)c->cosa_k >= 4u) c->cosa_k = (int)(azar() % 4); /* (1.0.36: ya elegida al llegar) */
            c->cx = mx + (azar() % 2 ? 90.0f : -90.0f); c->cy = my + 6; c->cvx = c->cvy = 0;
            po_di(po_buf("¡RONY ENCONTRÓ UNA %s! ¡SIEMPRE CON COSAS CHIQUITITAS!", PC_COSAS_N[c->cosa_k]), ICO_POCHI, 3000);
        }
        const float suelo = my + 6;
        if (t < 5500) {
            c->cvy += 0.45f * s_po_dtf; c->cx += c->cvx * s_po_dtf; c->cy += c->cvy * s_po_dtf; c->cvx *= 0.97f;
            if (c->cy > suelo) { c->cy = suelo; c->cvy = -c->cvy * 0.45f; if (fabsf(c->cvy) < 0.8f) c->cvy = 0; }
            if (c->cx < 60) { c->cx = 60; c->cvx = fabsf(c->cvx); }
            if (c->cx > W - 60) { c->cx = (float)(W - 60); c->cvx = -fabsf(c->cvx); }
            pc_mover(c, c->cx - (c->esp ? -14.0f : 14.0f), suelo + 4, 4.6f);
            if (hypotf(c->x - c->cx, c->y - 4 - c->cy) < 22.0f && c->cvy == 0) {
                c->cvx = (azar() % 2 ? 1.0f : -1.0f) * (3.0f + (float)(azar() % 30) / 10.0f); c->cvy = -4.5f;
                accion(UI_ACCION_SONAR, SND_POCHI_LADRA);
            }
            *oy = c->paso ? -fabsf(sinf((float)s_ms / 60.0f)) * 6.0f : 0;
            pc_cosa(c, PC_COSAS[c->cosa_k], c->cx, c->cy - 8);
        } else if (t < 7800) {
            pc_mover(c, mx + lado * 40.0f, my, 3.4f);
            pc_cosa(c, PC_COSAS[c->cosa_k], c->x + (c->esp ? -18.0f : 18.0f), c->y - 22);
        } else if (t < 10000) {
            if (c->hito != 7) {
                c->hito = 7; accion(UI_ACCION_POCHI, PET_POCHI_F_CACHORRO + c->k);
                po_responde(po_buf("¡GRACIAS, RONY! ¡QUÉ %s MÁS BONITA!", PC_COSAS_N[c->cosa_k]), "¡RONY, ERES UN LOQUITO!", 600);
                flotar(ICO_CORAZON, NULL, 0, (int)c->x, (int)c->y - 50, 0);
            }
            pc_cosa(c, PC_COSAS[c->cosa_k], mx + lado * 22.0f, my - 6);
            *f = 4;
        } else { pc_cosa(c, -1, 0, 0); return false; }
        if (*f < 0) *f = c->paso == 1 ? ((s_ms / 70) % 2 ? 2 : 3) : pc_quieto(c);
        return true;
    }
    if (c->k == POCHI_CHAPI) { /* se revuelca y le da un abrazo cochino */
        if (c->hito == 0) { c->hito = 9; po_di("¡CHAPI! ¡YA TE ESTÁS REVOLCANDO, CHANCHITO!", ICO_POCHI, 2800); }
        if (t < 3400) {
            *rot = (int)((t / 6) % 3600); *f = (s_ms / 140) % 2 ? 2 : 3; *oy = 6;
            if (s_ms % 220 < TICK_MS) chispa((int)c->x, (int)c->y, 0x7A5A2A, (float)(azar() % 5) * 0.2f - 0.4f, -0.6f);
        } else if (t < 6000) { pc_mover(c, mx + lado * 30.0f, my, 3.6f); *f = (s_ms / 90) % 2 ? 2 : 3; }
        else if (t < 10000) {
            if (c->hito != 8) {
                c->hito = 8; accion(UI_ACCION_POCHI, PET_POCHI_F_CACHORRO + c->k);
                po_di("¡ES MUY AMOROSO... AUNQUE ESTÉ SUCIO!", ICO_CORAZON, 2800);
                po_responde("¡AY, CHAPI! ¡ME ENSUCIASTE CON CARIÑO!", "¡QUÉ ABRAZO MÁS RICO, CHAPI!", 3000);
                flotar(ICO_ABRAZO, NULL, 0, (int)mx, (int)my - 80, 0);
            }
            c->esp = c->x > mx; *f = 0; *oy = -fabsf(sinf((float)s_ms / 200.0f)) * 3.0f;
            if (s_ms % 600 < TICK_MS) flotar(ICO_CORAZON, NULL, 0, (int)c->x, (int)c->y - 50, 0);
        } else return false;
        return true;
    }
    if (c->k == POCHI_PASITA) { /* busca una florcita con la nariz y se la regala */
        if (t < 2600) { pc_mover(c, mx + lado * 110.0f, my + 18, 2.2f); *f = c->paso ? ((s_ms / 120) % 2 ? 2 : 3) : 4; }
        else if (t < 5200) {
            if (c->hito == 0) { c->hito = 6; flotar(ICO_FLOR, NULL, 0, (int)c->x, (int)c->y - 40, 0); po_di("PASITA ANDA BUSCANDO ALGO CON LA NARIZ...", ICO_POCHI, 2400); }
            pc_mover(c, mx + lado * 34.0f, my, 2.4f); pc_cosa(c, ICO_FLOR, c->x + (c->esp ? -16.0f : 16.0f), c->y - 22); *f = (s_ms / 120) % 2 ? 2 : 3;
        } else if (t < 10000) {
            if (c->hito != 5) {
                c->hito = 5; pc_cosa(c, -1, 0, 0); accion(UI_ACCION_POCHI, PET_POCHI_F_CACHORRO + c->k);
                flotar(ICO_FLOR, NULL, 0, (int)mx, (int)my - 70, 0);
                po_di("PASITA TE TRAJO UNA FLORCITA. ¡ES UN AMOR!", ICO_CORAZON, 2800);
                po_responde("¡GRACIAS, PASITA! ¡QUÉ LINDA!", "¡AY, PASITA, QUÉ TIERNA!", 3000);
            }
            c->esp = c->x > mx; *f = 4;
            if (s_ms % 900 < TICK_MS) flotar(ICO_CORAZON, NULL, 0, (int)c->x, (int)c->y - 46, 0);
        } else return false;
        return true;
    }
    /* Morita: se esconde detrás de su mamá, se asoma, y al final se atreve y le da un besito */
    if (t < 2800) { pc_mover(c, s_po_x + (s_po_x < mx ? -36.0f : 36.0f), s_po_y + 6, 2.6f); *f = c->paso ? 2 : 4; }
    else if (t < 5000) { if (c->hito != 4) { c->hito = 4; po_di("MORITA ES UN POQUITO TÍMIDA...", ICO_POCHI, 2600); }
        c->esp = ((s_ms / 700) % 2) ? c->x > mx : c->x < mx; *f = 4; }
    else if (t < 6800) { pc_mover(c, mx + lado * 32.0f, my, 1.4f); *f = (s_ms / 180) % 2 ? 2 : 3; }
    else if (t < 10000) {
        if (c->hito != 3) { c->hito = 3; accion(UI_ACCION_POCHI, PET_POCHI_F_CACHORRO + c->k);
            po_di("¡MIRA! ¡TE DIO UN BESITO!", ICO_CORAZON, 2600); po_responde("¡QUÉ TIERNA, MORITA!", "¡GRACIAS, MORITA!", 2800); }
        c->esp = c->x > mx; *f = (s_ms / 300) % 2 ? 0 : 1;
        if (s_ms % 700 < TICK_MS) flotar(ICO_CORAZON, NULL, 0, (int)c->x, (int)c->y - 46, 0);
    } else return false;
    return true;
}
/* su juego propio, cuando está libre (los cuatro tienen uno, de la misma duración) */
static bool pc_propio(cach_t *c, int i, int *f, float *oy, int *rot)
{
    const uint32_t t = s_ms - c->t0;
    if (t > 3600) return false;
    if (c->k == POCHI_RONY) { /* corre en círculos alrededor de su mamá */
        const float a = (float)t / 260.0f;
        pc_mover(c, s_po_x + cosf(a) * 70.0f, s_po_y + 8 + sinf(a) * 26.0f, 6.0f);
        *f = (s_ms / 60) % 2 ? 2 : 3; *oy = -fabsf(sinf((float)s_ms / 70.0f)) * 4.0f;
    } else if (c->k == POCHI_CHAPI) { /* se revuelca donde está */
        *rot = (int)((t / 7) % 3600); *f = (s_ms / 140) % 2 ? 2 : 3; *oy = 6;
        if (s_ms % 260 < TICK_MS) chispa((int)c->x, (int)c->y, 0x7A5A2A, (float)(azar() % 5) * 0.2f - 0.4f, -0.6f);
    } else if (c->k == POCHI_PASITA) { /* olfatea una florcita */
        float px, py; pc_puesto(i, &px, &py);
        pc_mover(c, px + 30.0f, py + 12, 1.8f);
        *f = c->paso ? ((s_ms / 120) % 2 ? 2 : 3) : 4;
        if (!c->paso && s_ms % 1200 < TICK_MS) flotar(ICO_FLOR, NULL, 0, (int)c->x + (c->esp ? -18 : 18), (int)c->y - 30, 0);
    } else { /* Morita se esconde detrás de su mamá y se asoma */
        pc_mover(c, s_po_x + (s_po_espejo ? 30.0f : -30.0f), s_po_y + 2, 2.4f);
        *f = c->paso ? 2 : 4;
        if (!c->paso) c->esp = (t / 700) % 2;
    }
    return true;
}
/* la escena de familia de esta visita (los hermanos entre ellos, o Pochi haciendo de mamá) */
static void pc_familia_empezar(void)
{
    const int n = s_pc_n;
    int opc[5], m = 0;
    if (n >= 2) { opc[m++] = FAM_PILLARSE; opc[m++] = FAM_TIRA; opc[m++] = FAM_SIESTA; }
    int mo = -1, otro = -1;
    for (int i = 0; i < n; i++) { if (s_pc[i].k == POCHI_MORITA) mo = i; }
    for (int i = 0; i < n; i++) if (i != mo) { otro = i; break; }
    if (mo >= 0 && otro >= 0) opc[m++] = FAM_ESCONDE;
    opc[m++] = FAM_LAME;
    s_pc_fam = opc[azar() % (uint32_t)m];
    if (s_pc_fam_forzar >= 0) { s_pc_fam = s_pc_fam_forzar; s_pc_fam_forzar = -1; }
    s_pc_fa = (int)(azar() % (uint32_t)n);
    s_pc_fb = n >= 2 ? (s_pc_fa + 1 + (int)(azar() % (uint32_t)(n - 1))) % n : s_pc_fa;
    if (s_pc_fam == FAM_ESCONDE) { s_pc_fa = mo; s_pc_fb = otro; }
    s_pc_fam_t0 = s_ms; s_pc_fam_hecha = true;
    for (int i = 0; i < n; i++) if (s_pc[i].modo == PC_SIGUE || s_pc[i].modo == PC_PROPIO) { s_pc[i].modo = PC_SIGUE; }
    const char *a = PC_NOMBRE[s_pc[s_pc_fa].k], *b = PC_NOMBRE[s_pc[s_pc_fb].k];
    static char t[120];
    switch (s_pc_fam) {
    case FAM_PILLARSE: snprintf(t, sizeof t, "¡%s Y %s JUEGAN A LA PILLADA!", a, b); po_di(t, ICO_CORAZON, 2600); break;
    case FAM_TIRA: snprintf(t, sizeof t, "¡%s Y %s SE PELEAN UNA RAMITA! ¡A COMPARTIR, NIÑOS!", a, b); po_di(t, ICO_POCHI, 3000); break;
    case FAM_SIESTA: po_di(n >= 3 ? "SHHH... MIS CACHORROS SE QUEDARON DORMIDITOS, TODOS AMONTONADOS" : "SHHH... SE QUEDARON DORMIDITOS", ICO_ZETA, 3200); break;
    case FAM_ESCONDE: snprintf(t, sizeof t, "MORITA SE ESCONDE DETRÁS DE %s... ¡QUÉ VERGONZOSA!", b); po_di(t, ICO_POCHI, 2800); break;
    default: snprintf(t, sizeof t, PC_ELLA[s_pc[s_pc_fa].k] ? "¡VEN ACÁ, %s! ¡ESTÁS TODA DESPEINADA!" : "¡VEN ACÁ, %s! ¡ESTÁS TODO DESPEINADO!", a);
        po_di(t, ICO_CORAZON, 2600); po_truco(TR_LAME, 5000); break;
    }
}
/* lo que hace cada uno durante la escena de familia (false: ya terminó para todos) */
static bool pc_familia(cach_t *c, int i, int *f, float *oy, int *rot)
{
    const uint32_t t = s_ms - s_pc_fam_t0;
    const bool a = i == s_pc_fa, b = i == s_pc_fb;
    switch (s_pc_fam) {
    case FAM_PILLARSE: {
        if (t > 7000) return false;
        if (!a && !b) return false;
        cach_t *o = &s_pc[b ? s_pc_fa : s_pc_fb];
        if (b) { const float an = (float)t / 420.0f; pc_mover(c, s_mascota.x + cosf(an) * 120.0f, s_mascota.y + 20 + sinf(an) * 40.0f, 4.4f); }
        else pc_mover(c, o->x - (o->esp ? -24.0f : 24.0f), o->y, 4.0f);
        *f = (s_ms / 60) % 2 ? 2 : 3; *oy = -fabsf(sinf((float)s_ms / 70.0f)) * 4.0f;
        if (t > 6000) { *f = 4; *oy = 0; } /* cansados */
        return true;
    }
    case FAM_TIRA: { /* cada uno de un lado de la ramita: tira y afloja; Pochi la parte en dos */
        if (t > 7500 || (!a && !b)) { if (a) pc_cosa(c, -1, 0, 0); return t <= 7500 && (a || b); }
        const float cx = (s_mascota.x + s_po_x) / 2, cy = s_mascota.y + 34;
        const float tira = sinf((float)t / 180.0f) * 8.0f;
        pc_mover(c, cx + (a ? -26.0f : 26.0f) + tira, cy, 3.0f);
        if (!c->paso) c->esp = !a;
        *f = c->paso ? ((s_ms / 90) % 2 ? 2 : 3) : 0;
        if (a) {
            if (t < 5200) pc_cosa(c, ICO_RAMITA, cx + tira, cy - 18);
            else { pc_cosa(c, ICO_RAMITA, c->x + (c->esp ? -14.0f : 14.0f), c->y - 20); }
            if (t > 3400 && c->hito != 20) {
                c->hito = 20;
                const int kb = s_pc[s_pc_fb].k;
                po_di(po_buf(PC_ELLA[kb] ? "¡%s, SUELTA A TU HERMANA!" : "¡%s, SUELTA A TU HERMANO!", PC_NOMBRE[c->k]), ICO_POCHI, 2400);
            }
            if (t > 5200 && c->hito != 21) { c->hito = 21; po_di("¡LISTO! UN PEDACITO PARA CADA UNO", ICO_RAMITA, 2400); }
        }
        if (b && t > 5200) pc_cosa(c, ICO_RAMITA, c->x + (c->esp ? -14.0f : 14.0f), c->y - 20);
        return true;
    }
    case FAM_SIESTA: { /* todos amontonados junto a su mamá, de lado, durmiendo */
        if (t > 8000) return false;
        pc_mover(c, s_po_x + (s_po_espejo ? 1.0f : -1.0f) * (40.0f + (float)i * 22.0f), s_po_y + 14 + (float)(i % 2) * 8.0f, 2.4f);
        if (!c->paso) { *f = 4; *rot = c->esp ? -900 : 900; *oy = 12.0f + sinf((float)s_ms / 600.0f + (float)i); if (s_ms % 2400 < TICK_MS && i == 0) flotar(ICO_ZETA, NULL, 0, (int)c->x, (int)c->y - 40, 0); }
        else *f = (s_ms / 120) % 2 ? 2 : 3;
        if (t > 3000 && i == 0 && c->hito != 22) { c->hito = 22; po_responde("¡QUÉ TIERNOS!", "AWWW... ¡QUÉ LINDOS!", 400); }
        return true;
    }
    case FAM_ESCONDE: { /* Morita detrás de su hermano/a, asomándose */
        if (t > 6500 || (!a && !b)) return t <= 6500 && (a || b);
        cach_t *o = &s_pc[s_pc_fb];
        if (b) { c->paso = 0; *f = pc_quieto(c); c->esp = c->x > s_mascota.x; return true; }
        pc_mover(c, o->x + (o->esp ? 22.0f : -22.0f), o->y - 2, 2.4f);
        *f = c->paso ? 2 : 4;
        if (!c->paso) c->esp = ((t / 800) % 2) ? !o->esp : o->esp;
        return true;
    }
    default: { /* FAM_LAME: Pochi le lame la cabeza; el cachorro se deja, sentadito */
        if (t > 5200) return false;
        if (!a) return false;
        c->paso = 0; *f = 4; c->esp = c->x > s_po_x;
        if (s_ms % 800 < TICK_MS) flotar(ICO_CORAZON, NULL, 0, (int)c->x, (int)c->y - 46, 0);
        return true;
    }
    }
}
static void pc_dibujar(cach_t *c, int f, float oy, int rot)
{
    if (f < 0) f = 0;
    const art_icono_t ico = pc_ico(c->k, f);
    if ((int)ico != c->ult_ico || c->esp != c->ult_esp) { c->ult_ico = (int)ico; c->ult_esp = c->esp; lv_image_set_src(c->img, c->esp ? art_icono_espejo(ico, PC_ESC) : art_icono(ico, PC_ESC)); }
    if (rot != c->ult_rot) { c->ult_rot = rot; lv_image_set_rotation(c->img, rot); }
    const int L = 12 * PC_ESC, px = (int)c->x - L / 2, py = (int)(c->y - L + 2 + oy);
    if (px != lv_obj_get_x(c->img) || py != lv_obj_get_y(c->img)) lv_obj_set_pos(c->img, px, py);
    lv_obj_set_style_opa(c->img, lv_obj_get_style_opa(s_po_img, 0), 0);
    ocultar(c->img, false);
    lv_obj_move_foreground(c->img);
}
static void pc_esc_cb(lv_event_t *e) { (void)e; if (s_pc_esc >= 0 && !s_pc_esc_res) s_pc_esc_res = 1; }
/* al despedirse: «¿Y MORITA?» uno se escondió (detrás de un arbusto); tócalo para encontrarlo */
static void pc_esconderse_empezar(void)
{
    s_pc_esc = (int)(azar() % (uint32_t)s_pc_n); s_pc_esc_res = 0; s_pc_esc_t0 = s_ms; s_pc_esc_forzar = false;
    cach_t *c = &s_pc[s_pc_esc];
    float x = s_mascota.x < CX ? s_mascota.x + 150.0f : s_mascota.x - 150.0f, y = s_mascota.y - 40.0f + (float)(azar() % 80);
    limitar(&x, &y);
    s_pc_esc_x = x; s_pc_esc_y = y;
    c->modo = PC_ESCONDIDO; c->x = x; c->y = y;
    if (!s_pc_esc_toque) {
        s_pc_esc_toque = lv_obj_create(s_mundo);
        lv_obj_remove_style_all(s_pc_esc_toque);
        lv_obj_set_size(s_pc_esc_toque, 90, 80);
        lv_obj_add_flag(s_pc_esc_toque, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(s_pc_esc_toque, pc_esc_cb, LV_EVENT_CLICKED, NULL);
    }
    lv_obj_set_pos(s_pc_esc_toque, (int)x - 45, (int)y - 56);
    ocultar(s_pc_esc_toque, false);
    lv_obj_move_foreground(s_pc_esc_toque);
    static const char *const NUM[4] = {"", "UNO", "UNO, DOS", "UNO, DOS, TRES"};
    char t[110];
    if (s_pc_n == 1) snprintf(t, sizeof t, "¿Y %s? ¿DÓNDE SE METIÓ? ¡AYÚDAME A BUSCAR%s!", PC_NOMBRE[c->k], PC_ELLA[c->k] ? "LA" : "LO");
    else snprintf(t, sizeof t, "%s... ¿Y %s? ¡AYÚDAME A BUSCAR%s!", NUM[s_pc_n - 1], PC_NOMBRE[c->k], PC_ELLA[c->k] ? "LA" : "LO");
    po_di(t, ICO_PREGUNTA, 3600);
}
static void pc_animar(void)
{
    if (s_pc_n <= 0) return;
    if (s_po_fase == PO_NADA || s_po_fase == PO_ACURRUCA || s_interior || s_po_pf || po_en_transito()) { pc_esconder(); return; }
    /* ¿quién tiene su momento ahora? (de a uno, con una pausa entre uno y otro) */
    const bool libre_fase = (s_po_fase == PO_CORRE || s_po_fase == PO_INVITA || s_po_fase == PO_PASEO) && s_po_comio && !s_po_truco && s_pc_fam < 0;
    if (s_pc_turno < 0 && libre_fase && s_ms - s_pc_libre > 2500)
        for (int i = 0; i < s_pc_n; i++) if (!s_pc[i].hecho) { s_pc_turno = i; s_pc[i].hecho = true; s_pc[i].modo = PC_TURNO; s_pc[i].t0 = s_ms; s_pc[i].hito = 0; s_pc[i].paso = 0; break; }
    /* la escena de familia: cuando ya pasaron los momentos de todos */
    bool todos = true;
    for (int i = 0; i < s_pc_n; i++) todos &= s_pc[i].hecho;
    if (s_pc_turno < 0 && todos && !s_pc_fam_hecha && libre_fase && s_ms - s_pc_libre > 3000) {
        static uint32_t chk;
        if (s_ms - chk > 1000) { chk = s_ms; if (azar() % 100 < 20) pc_familia_empezar(); }
    }
    bool fam_viva = false;
    for (int i = 0; i < s_pc_n; i++) {
        cach_t *c = &s_pc[i];
        int f = -1, rot = 0;
        float oy = 0;
        if (c->modo == PC_SIGUE && s_pc_fam < 0 && c->hecho && s_ms - c->chequeo > 1000) { /* de vez en cuando, algo propio */
            c->chequeo = s_ms;
            if ((s_po_fase == PO_INVITA || s_po_fase == PO_PASEO || s_po_fase == PO_CORRE) && azar() % 100 < 4) {
                const uint32_t r = azar() % 3;
                c->modo = r == 0 ? PC_PROPIO : (r == 1 ? PC_PANZA : (po_hay_pasto() ? PC_RASCA : PC_PANZA));
                c->t0 = s_ms;
                if (c->modo == PC_RASCA) c->esp = c->x > CX;
            }
        }
        const uint32_t tt = s_ms - c->t0;
        if (s_pc_fam >= 0 && c->modo != PC_TURNO && c->modo != PC_ESCONDIDO && c->modo != PC_CARGADO) c->modo = PC_FAM;
        switch (c->modo) {
        case PC_TURNO: if (!pc_turno(c, &f, &oy, &rot)) { c->modo = PC_SIGUE; s_pc_turno = -1; s_pc_libre = s_ms; f = -1; rot = 0; pc_cansado(c); } break;
        case PC_FAM:
            if (s_pc_fam >= 0 && pc_familia(c, i, &f, &oy, &rot)) fam_viva = true;
            else { c->modo = PC_SIGUE; pc_cosa(c, -1, 0, 0); f = -1; rot = 0; oy = 0; pc_cansado(c); }
            break;
        case PC_ANIMAL: if (!pa_cachorro(i, &f, &oy, &rot)) { c->modo = PC_SIGUE; f = -1; rot = 0; oy = 0; pc_cansado(c); } break;
        case PC_PROPIO: if (!pc_propio(c, i, &f, &oy, &rot)) { c->modo = PC_SIGUE; f = -1; rot = 0; oy = 0; pc_cansado(c); } break;
        case PC_PANZA: f = (s_ms / 150) % 2 ? 2 : 3; rot = 1800; oy = 14; if (tt > 3200) c->modo = PC_SIGUE; break;
        case PC_RASCA:
            f = 4; rot = (s_ms / 130) % 2 ? 50 : -50; c->x += (c->esp ? -0.9f : 0.9f) * s_po_dtf;
            if (s_ms % 180 < TICK_MS) chispa((int)c->x + (c->esp ? 14 : -14), (int)c->y, 0x7CC860, c->esp ? 0.4f : -0.4f, -0.5f);
            if (tt > 2200) c->modo = PC_SIGUE;
            break;
        case PC_ESCONDIDO: { /* se asoma de vez en cuando (y a veces ladra bajito) */
            const uint32_t e = s_ms - s_pc_esc_t0;
            if (s_pc_esc_res) {
                c->modo = PC_SIGUE; ocultar(s_pc_esc_toque, true);
                lv_obj_set_style_opa(c->img, 255, 0);
                for (int j = 0; j < 8; j++) chispa((int)c->x - 16 + (int)(azar() % 32), (int)c->y - 8 - (int)(azar() % 20), 0x7CD85A, (float)((int)(azar() % 9) - 4) * 0.45f, -1.3f); /* salta del pasto */
                flotar(ICO_CORAZON, NULL, 0, (int)c->x, (int)c->y - 50, 0);
                accion(UI_ACCION_SONAR, SND_POCHI_FELIZ);
                if (s_pc_esc_res == 1) { po_di(po_buf(PC_ELLA[c->k] ? "¡AHÍ ESTABA %s! ¡GRACIAS POR ENCONTRARLA!" : "¡AHÍ ESTABA %s! ¡GRACIAS POR ENCONTRARLO!", PC_NOMBRE[c->k]), pc_ico(c->k, 1), 2800); }
                else po_di(po_buf(PC_ELLA[c->k] ? "¡AQUÍ ESTOY, MAMÁ! (%s SALIÓ SOLITA)" : "¡AQUÍ ESTOY, MAMÁ! (%s SALIÓ SOLITO)", PC_NOMBRE[c->k]), pc_ico(c->k, 1), 2600);
                break;
            }
            if (e > 25000) { s_pc_esc_res = 2; break; }
            /* no se le ve. Cada 2,6 s: se mueve el pasto (saltan hojitas y se oye un quejidito), y recién
             * entonces asoma un momento, entero, y se vuelve a meter. Nada de siluetas ni parches de color */
            lv_obj_move_foreground(s_pc_esc_toque);
            const uint32_t ciclo = e % 2600;
            if (ciclo < 900) { /* se mueve el pasto */
                ocultar(c->img, true);
                if (ciclo < TICK_MS * 2) {
                    accion(UI_ACCION_SONAR, SND_POCHI_PIDE);
                    flotar(ICO_HIERBA, NULL, 0, (int)c->x - 12, (int)c->y - 6, 0);
                    flotar(ICO_HIERBA, NULL, 0, (int)c->x + 14, (int)c->y - 2, 220);
                }
                if (s_ms % 110 < TICK_MS) chispa((int)c->x - 16 + (int)(azar() % 32), (int)c->y - 4, (azar() & 1) ? 0x7CD85A : 0x3E8A32,
                                                 (float)((int)(azar() % 9) - 4) * 0.3f, -0.45f);
            } else if (ciclo < 1800) { /* y ahí asoma, entero */
                const uint32_t a = ciclo - 900;
                const float sube = a < 200 ? (float)a / 200.0f : (a < 700 ? 1.0f : 1.0f - (float)(a - 700) / 200.0f);
                c->esp = ((e / 2600) % 2) != 0;
                pc_dibujar(c, 4, 26.0f - (sube < 0 ? 0 : sube) * 26.0f, 0);
            } else ocultar(c->img, true);
        }
        case PC_CARGADO: /* en la boca de su mamá, colgando del cuello */
            c->x = s_po_x + (s_po_espejo ? -26.0f : 26.0f); c->y = s_po_y - 6; c->esp = s_po_espejo;
            f = 0; rot = s_po_espejo ? -150 : 150; oy = sinf((float)s_ms / 120.0f) * 2.0f;
            break;
        default: { /* sigue a su mamá */
            float tx, ty; pc_puesto(i, &tx, &ty);
            float vel = hypotf(tx - c->x, ty - c->y) * 0.07f;
            if (vel < 2.0f) vel = 2.0f;
            if (vel > 6.0f) vel = 6.0f;
            pc_mover(c, tx, ty, vel);
            if (c->paso) f = (s_ms / 90) % 2 ? 2 : 3;
            else { c->esp = s_po_espejo; f = (s_po_fase == PO_HAMBRE || s_po_fase == PO_COME || s_po_fase == PO_CHARLA || s_po_fase == PO_DESPIDE) ? 4 : pc_quieto(c); }
            if (s_po_fase == PO_INVITA && !c->paso) oy = -fabsf(sinf((float)s_ms / 140.0f + (float)i)) * 4.0f;
            if (s_po_fase == PO_SE_VA) f = (s_ms / 80) % 2 ? 2 : 3;
        }
        }
        pc_dibujar(c, f, oy, rot);
    }
    if (s_pc_fam >= 0 && !fam_viva && s_ms - s_pc_fam_t0 > 1000) { s_pc_fam = -1; s_pc_libre = s_ms; }
    for (int i = 0; i < s_pc_n; i++) if (s_pc[i].cosa && !lv_obj_has_flag(s_pc[i].cosa, LV_OBJ_FLAG_HIDDEN)) lv_obj_move_foreground(s_pc[i].cosa);
}
/* los toques: uno ladra; dos, cosquillas; tres, panza arriba; seis, se marea */
static void po_toque(void)
{
    if (s_po_fase == PO_ACURRUCA) { flotar(ICO_ZETA, NULL, 0, (int)s_po_x, (int)s_po_y - 70, 0); po_di("SHHH... QUE ESTÁ DURMIENDO", ICO_ZETA, 2000); return; }
    if (s_po_fase == PO_OFENDIDA && s_po_pregunta < 2) { s_po_t0 = s_ms - 5800; return; } /* con un cariño se le pasa */
    if (s_po_fase == PO_MANIA && s_po_mania == MAN_RAMITA && s_po_mst == 1) { s_po_mst = 2; s_po_t0 = s_ms; return; } /* ¡la pillaste! */
    s_po_toques = s_ms - s_po_toque_ms < 2500 ? s_po_toques + 1 : 1;
    s_po_toque_ms = s_ms;
    flotar(ICO_CORAZON, NULL, 0, (int)s_po_x, (int)s_po_y - 70, 0);
    accion(UI_ACCION_SONAR, SND_POCHI_LADRA);
    if (s_po_truco == TR_PANZA) {
        if (!s_po_rascada) { s_po_rascada = true; accion(UI_ACCION_POCHI, PET_POCHI_F_PANZA); po_di("¡AHÍ, AHÍ! ¡JUSTO AHÍ!", ICO_CORAZON, 2000); }
        if (s_po_toques >= 6) { po_truco(TR_MAREO, 3600); po_di("¡AY, CUÁNTO CARIÑO... ME MAREÉ!", ICO_MAREO, 2600); flotar(ICO_MAREO, NULL, 0, (int)s_po_x, (int)s_po_y - 80, 0); }
        else s_po_truco_dur = s_ms - s_po_truco_t0 + 2500; /* mientras la rasques, sigue panza arriba */
        return;
    }
    if (s_po_truco) return;
    if (s_po_toques == 2) po_di("¡JIJI, COSQUILLAS!", ICO_POCHI, 1500);
    else if (s_po_toques >= 3 && po_truco_puede()) po_panza_empezar(true);
    else if (s_po_toques == 1 && s_ms % 3 == 0) po_di("¡GUAU!", ICO_POCHI, 1500);
}
/* ===================== (fw 102) Pochi y sus cachorros con los animales del lugar =====================
 * Reaccionan al animal que de verdad está en pantalla. Solo cuando Pochi viene sola o con UN cachorro (con dos o más, la visita
 * es de familia), y una vez por visita. Cada tipo de animal tiene una reacción distinta de cada uno; además hay encuentros
 * especiales (zorrillo, erizo, urraca o cuervo, búho, chucao) y cada cachorro tiene un animal amigo que saluda cuando lo ve
 * (Chapi: la rana; Pasita: la mariposa; Morita: el caracol; Rony: la ardilla). */
enum { AT_BICHO = 0, AT_PAJARO, AT_MANSO, AT_LENTO, AT_AGUA, AT_GRANDE, AT_ZORRILLO, AT_ERIZO, AT_LADRON, AT_BUHO, AT_CHUCAO, AT_AMIGO, AT_N };
enum { MV_HUELE = 0, MV_SALTA, MV_QUIETA, MV_LEJOS, MV_VUELTAS, MV_ESCONDE, MV_LADRA, MV_ECHA, MV_LENTO, MV_HUYE };
static int s_pa_tipo, s_pa_src, s_pa_id, s_pa_actor, s_pa_vuelve, s_pa_st, s_pa_forzar = -1; /* src: 0 fauna, 1 animal, 2 de mentira (pruebas y chucao) */
static bool s_pa_amigo_nuevo;
static float s_pa_fx, s_pa_fy;
static art_icono_t s_pa_fico;
static int pa_tipo_animal(int a)
{
    switch (a) {
    case ANIMAL_ERIZO: return AT_ERIZO;
    case ANIMAL_ZORRILLO: return AT_ZORRILLO;
    case ANIMAL_URRACA: case ANIMAL_CUERVO: return AT_LADRON;
    case ANIMAL_BUHO: return AT_BUHO;
    case ANIMAL_CARACOL: case ANIMAL_TORTUGA: return AT_LENTO;
    case ANIMAL_PATO: case ANIMAL_CISNE: case ANIMAL_GARZA: case ANIMAL_NUTRIA: case ANIMAL_CASTOR: case ANIMAL_PINGUINO: case ANIMAL_FOCA: return AT_AGUA;
    case ANIMAL_COLIBRI: case ANIMAL_CARPINTERO: case ANIMAL_MARTIN: case ANIMAL_GOLONDRINA: case ANIMAL_MURCIELAGO: return AT_PAJARO;
    case ANIMAL_OSO: case ANIMAL_CIERVO: case ANIMAL_GATO_MONTES: case ANIMAL_LINCE: case ANIMAL_PUMA: case ANIMAL_OSO_POLAR: case ANIMAL_CULEBRA: return AT_GRANDE;
    case ANIMAL_ARDILLA: case ANIMAL_CONEJO: case ANIMAL_TOPO: case ANIMAL_MAPACHE: case ANIMAL_RATON: case ANIMAL_TEJON: return AT_MANSO;
    default: return AT_BICHO; /* lagartijas e insectos */
    }
}
static int pa_tipo_fauna(int f) { return f == FAUNA_PAJARO ? AT_PAJARO : (f == FAUNA_ZORRO ? AT_GRANDE : (f == FAUNA_RANA ? AT_AGUA : AT_BICHO)); }
/* el animal amigo de cada cachorro */
static const uint8_t PA_AMIGO[PET_POCHI_CACHORROS][2] = {{0, FAUNA_RANA}, {0, FAUNA_MARIPOSA}, {1, ANIMAL_CARACOL}, {1, ANIMAL_ARDILLA}};
static bool pa_en_pantalla(float x, float y) { return x > 70.0f && x < (float)(W - 70) && y > 90.0f && y < (float)(W - 90); }
static bool pa_vivo(void)
{
    if (s_pa_src == 2) return true;
    if (s_pa_src == 0) return s_fauna[s_pa_id].visible && !s_fauna[s_pa_id].saliendo;
    return s_ani[s_pa_id].visible;
}
/* dónde está el animal, y dónde se paran ellos (en el suelo, aunque el animal vuele) */
static void pa_pos(float *ax, float *ay, float *gy)
{
    if (s_pa_src == 2) { *ax = s_pa_fx; *ay = s_pa_fy; }
    else if (s_pa_src == 0) { *ax = s_fauna[s_pa_id].x; *ay = s_fauna[s_pa_id].y; }
    else { *ax = s_ani[s_pa_id].x; *ay = s_ani[s_pa_id].y; }
    *gy = *ay < s_mascota.y - 50.0f ? s_mascota.y + 8.0f : *ay + 4.0f;
}
static art_icono_t pa_ico(void) { return s_pa_src == 2 ? s_pa_fico : (s_pa_src == 0 ? ICO_FAUNA[s_pa_id] : ICO_ANIMAL[s_pa_id]); }
/* ¿hay un animal a la vista con quien encontrarse? (prefiere al amigo del cachorro que vino) */
static bool pa_buscar(void)
{
    int n = 0, src[8], id[8];
    const int k = s_pc_n == 1 ? s_pc[0].k : -1;
    s_pa_amigo_nuevo = false;
    if (s_pa_forzar >= 0) { /* (pruebas) un animal de mentira del tipo pedido, junto a ella */
        static const art_icono_t IC[AT_N] = {ICO_MARIQUITA, ICO_PAJARO, ICO_CONEJO, ICO_TORTUGA, ICO_PATO, ICO_CIERVO_CAFE, ICO_ZORRILLO, ICO_ERIZO, ICO_URRACA, ICO_BUHO, ICO_CHUCAO, ICO_RANA};
        static const art_icono_t ICA[PET_POCHI_CACHORROS] = {ICO_RANA, ICO_MARIPOSA, ICO_CARACOL, ICO_ARDILLA};
        s_pa_tipo = s_pa_forzar; s_pa_forzar = -1; s_pa_src = 2; s_pa_id = 0;
        s_pa_fico = s_pa_tipo == AT_AMIGO && k >= 0 ? ICA[k] : IC[s_pa_tipo];
        s_pa_fx = s_mascota.x < CX ? s_mascota.x + 150.0f : s_mascota.x - 150.0f;
        s_pa_fy = s_pa_tipo == AT_PAJARO || s_pa_tipo == AT_BUHO || s_pa_tipo == AT_LADRON ? s_mascota.y - 90.0f : s_mascota.y + 20.0f;
        if (s_pa_tipo == AT_AMIGO) s_pa_amigo_nuevo = k >= 0 && s_pet && !pet_pochi_pup_friend(s_pet, k);
        return s_pa_tipo != AT_AMIGO || k >= 0;
    }
    for (int f = 0; f < FAUNA_COUNT; f++) if (s_fauna[f].visible && !s_fauna[f].saliendo && pa_en_pantalla(s_fauna[f].x, s_fauna[f].y)) {
        if (k >= 0 && PA_AMIGO[k][0] == 0 && PA_AMIGO[k][1] == f) { s_pa_src = 0; s_pa_id = f; s_pa_tipo = AT_AMIGO; s_pa_amigo_nuevo = s_pet && !pet_pochi_pup_friend(s_pet, k); return true; }
        if (n < 8) { src[n] = 0; id[n++] = f; }
    }
    for (int a = 0; a < ANIMAL_COUNT; a++) if (s_ani[a].visible && !s_ani[a].saliendo && pa_en_pantalla(s_ani[a].x, s_ani[a].y)) {
        if (k >= 0 && PA_AMIGO[k][0] == 1 && PA_AMIGO[k][1] == a) { s_pa_src = 1; s_pa_id = a; s_pa_tipo = AT_AMIGO; s_pa_amigo_nuevo = s_pet && !pet_pochi_pup_friend(s_pet, k); return true; }
        if (n < 8) { src[n] = 1; id[n++] = a; }
    }
    if (n == 0) { /* en el claro, de día: el chucao (siempre anda por ahí) */
        if (s_escena != ESC_CLARO || s_fondo_periodo == PERIODO_NOCHE) return false;
        s_pa_src = 2; s_pa_tipo = AT_CHUCAO; s_pa_fico = ICO_CHUCAO; s_pa_fx = s_po_x; s_pa_fy = s_po_y - 120.0f;
        return true;
    }
    const int e = (int)(azar() % (uint32_t)n);
    s_pa_src = src[e]; s_pa_id = id[e];
    s_pa_tipo = s_pa_src == 0 ? pa_tipo_fauna(s_pa_id) : pa_tipo_animal(s_pa_id);
    return true;
}
/* cómo se mueve uno de ellos frente al animal. pose: 0 de pie, 1 trota, 2 sentado, 3 olfatea */
static void pa_mov(int mv, uint32_t t, float rx, float ry, float *x, float *y, bool *esp, int *pose, float *oy, int *rot)
{
    float ax, ay, gy;
    pa_pos(&ax, &ay, &gy);
    const float lado = *x < ax ? -1.0f : 1.0f;
    float tx = *x, ty = *y, vel = 2.6f;
    switch (mv) {
    case MV_HUELE: tx = ax + lado * 34.0f; ty = gy; break;
    case MV_SALTA: tx = ax + lado * 30.0f; ty = gy + 2; vel = 3.4f; break;
    case MV_QUIETA: tx = ax + lado * 40.0f; ty = gy; vel = 2.2f; break;
    case MV_LEJOS: tx = ax + lado * 110.0f; ty = gy + 6; break;
    case MV_VUELTAS: { const float a = (float)t / 300.0f; tx = ax + cosf(a) * 54.0f; ty = gy + 4 + sinf(a) * 22.0f; vel = 5.0f; break; }
    case MV_ESCONDE: tx = rx + (rx < ax ? -30.0f : 30.0f); ty = ry + 4; vel = 3.2f; break;
    case MV_LADRA: tx = ax + lado * 84.0f; ty = gy + 2; vel = 3.0f; break;
    case MV_ECHA: tx = ax + lado * 38.0f; ty = gy + 2; break;
    case MV_LENTO: tx = ax + lado * 36.0f; ty = gy; vel = 0.9f; break;
    default: tx = ax + lado * 190.0f; ty = gy + 10; vel = 5.0f; break; /* MV_HUYE */
    }
    if (tx < 50.0f) tx = 50.0f;
    if (tx > (float)(W - 50)) tx = (float)(W - 50);
    limitar(&tx, &ty);
    const float dx = tx - *x, dy = ty - *y, d = hypotf(dx, dy);
    vel *= s_po_dtf;
    bool mueve = d > vel + 0.5f;
    if (mueve) { *x += dx / d * vel; *y += dy / d * vel; if (fabsf(dx) > 1) *esp = dx < 0; }
    else { *x = tx; *y = ty; if (mv != MV_ESCONDE) *esp = *x > ax; }
    *pose = mueve ? 1 : 0; *oy = 0; *rot = 0;
    if (mueve) { if (mv == MV_VUELTAS || mv == MV_HUYE) *oy = -fabsf(sinf((float)s_ms / 70.0f)) * 3.0f; return; }
    switch (mv) {
    case MV_HUELE: *pose = 3; break;
    case MV_SALTA: *oy = -fabsf(sinf((float)s_ms / 110.0f)) * 18.0f; break;
    case MV_QUIETA: case MV_LEJOS: case MV_LENTO: case MV_HUYE: *pose = 2; break;
    case MV_ESCONDE: *pose = 2; *esp = ((t / 700) % 2) ? *x > ax : *x < ax; break; /* se asoma */
    case MV_LADRA: *oy = (s_ms / 200) % 2 ? -6.0f : 0; break;
    case MV_ECHA: *pose = 2; *rot = *esp ? -900 : 900; *oy = 10.0f; break;
    default: break;
    }
}
/* la reacción de cada uno a cada tipo de animal: [tipo][0 Pochi, 1 Chapi, 2 Pasita, 3 Morita, 4 Rony] */
static const struct { uint8_t mv; const char *t; } PA_GEN[5][5] = {
    { /* bichitos */
        {MV_HUELE, "¡SNIF, SNIF! ¡ME HACE COSQUILLAS EN LA NARIZ! ¡ACHÍS!"}, {MV_SALTA, "¡CHAPI SE LE TIRÓ ENCIMA... Y CAYÓ DE GUATA!"},
        {MV_QUIETA, "PASITA SE QUEDA QUIETITA, A VER SI SE LE POSA"}, {MV_LEJOS, "MORITA LO MIRA DE LEJITOS... ¡LE ENCANTA!"},
        {MV_VUELTAS, "¡RONY LO PERSIGUE! ¡ES CHIQUITITO, COMO LE GUSTAN!"}},
    { /* pájaros */
        {MV_LADRA, "¡GUAU, GUAU! ¿VISTE? ¡LO DEJÉ TIESO DE MIEDO!"}, {MV_VUELTAS, "¡CHAPI LO CORRETEA HASTA QUE SE CANSA!"},
        {MV_QUIETA, "¡PASITA LE CANTA! AUUU-UUU"}, {MV_ESCONDE, "MORITA SE ESCONDE... ¡ES QUE ALETEA MUCHO!"},
        {MV_SALTA, "¡RONY SALTA PARA ALCANZARLO! ¡CASI, CASI!"}},
    { /* chicos y mansos */
        {MV_HUELE, "¡HOLA, VECINO! ¡HUELES A PASTITO!"}, {MV_HUELE, "¡CHAPI LO QUIERE ABRAZAR!"},
        {MV_QUIETA, "PASITA SE SENTÓ A SU LADO, CALLADITA"}, {MV_LENTO, "MORITA SE ACERCA DE A POQUITO... ¡SE ATREVIÓ!"},
        {MV_VUELTAS, "¡RONY LO INVITA A CORRER!"}},
    { /* lentos */
        {MV_HUELE, "¡APÚRATE, PUES! ...¿NO? ¡QUÉ PACIENCIA HAY QUE TENER!"}, {MV_ECHA, "CHAPI SE ECHÓ A SU LADO. ¡SIN APURO!"},
        {MV_QUIETA, "PASITA LO ACOMPAÑA PASITO A PASITO"}, {MV_LENTO, "CON LOS LENTOS, MORITA SÍ SE ATREVE"},
        {MV_VUELTAS, "¡RONY LE DA VUELTAS Y VUELTAS! ¡LO VA A MAREAR!"}},
    { /* de agua */
        {MV_LADRA, "¡GUAU! ¡TE LADRO DESDE AQUÍ, QUE ALLÁ ESTÁ MOJADO!"}, {MV_ECHA, "¡CHAPI ENCONTRÓ BARRO EN LA ORILLA! ¡OBVIO!"},
        {MV_QUIETA, "PASITA LO MIRA NADAR... ¡QUÉ ELEGANTE!"}, {MV_LEJOS, "¡MORITA DIO UN SALTITO DEL SUSTO!"},
        {MV_SALTA, "¡RONY LO SIGUE A SALTOS POR LA ORILLA!"}},
};
static const char *const PA_RESP[5][2] = {
    {"¡SALUD, POCHI!", "¡JAJA! ¡QUÉ LINDO!"}, {"¡NO LO ASUSTEN!", "¡JAJA! ¡CASI!"}, {"¡QUÉ TIERNOS!", "¡YA TIENEN UN AMIGO NUEVO!"},
    {"¡CON CALMA, CON CALMA!", "¡JAJA! ¡NO LO APUREN!"}, {"¡CUIDADO CON MOJARSE!", "¡JAJA! ¡QUÉ FAMILIA!"}};
static void pa_empezar(int vuelve)
{
    s_pa_on = true; s_pa_hecha = true; s_pa_vuelve = vuelve; s_pa_st = 0;
    s_po_fase = PO_ANIMAL; s_po_t0 = s_ms;
    /* quién reacciona: con un cachorro, él o ella (su amigo, siempre); a veces Pochi */
    s_pa_actor = -1;
    if (s_pc_n == 1 && (s_pa_tipo == AT_AMIGO || s_pa_tipo == AT_GRANDE || s_pa_tipo == AT_ZORRILLO || s_pa_tipo == AT_BUHO || azar() % 100 < 65)) s_pa_actor = 0;
    if (s_pa_tipo == AT_CHUCAO) s_pa_actor = -1;
    if (s_pc_n == 1) { s_pc[0].modo = PC_ANIMAL; s_pc[0].t0 = s_ms; }
}
static void pa_fin(void)
{
    s_pa_on = false;
    po_cosa(-1, 0, 0, 0);
    s_po_fase = s_pa_vuelve; s_po_t0 = s_ms; s_po_pregunta = 0;
    if (s_po_fase == PO_PASEO) { s_po_zona_t0 = s_ms > 12000u ? s_ms - 12000u : 1u; s_po_hablo = 2; }
}
/* qué hace cada uno en este momento del encuentro: quien = -1 Pochi, 0 el cachorro */
static int pa_papel(int quien, uint32_t t)
{
    const bool actor = quien == s_pa_actor;
    const int k = quien >= 0 ? s_pc[quien].k : -1;
    switch (s_pa_tipo) {
    case AT_GRANDE: /* Pochi se pone adelante; el cachorro se esconde (Rony quiere ir a verlo, y vuelve) */
        if (quien < 0) return MV_LADRA;
        return k == POCHI_RONY && t > 1500 && t < 4500 ? MV_QUIETA : MV_ESCONDE;
    case AT_ZORRILLO: /* le ladran, levanta la cola... ¡y todos arrancan! (Chapi llega tarde) */
        if (t < 2600) return MV_LADRA;
        if (k == POCHI_CHAPI && t < 5200) return MV_HUELE;
        return MV_HUYE;
    case AT_ERIZO: /* el que lo huele se pincha la nariz; Pochi consuela */
        if (actor) return t < 3000 ? MV_HUELE : MV_LEJOS;
        return quien < 0 ? MV_LEJOS : MV_ESCONDE;
    case AT_LADRON: return actor ? (t < 6500 ? MV_SALTA : MV_QUIETA) : MV_LEJOS;
    case AT_BUHO: return quien < 0 ? MV_LEJOS : MV_ESCONDE;
    case AT_CHUCAO: return MV_LADRA;
    case AT_AMIGO: return actor ? (k == POCHI_RONY ? (t < 5000 ? MV_VUELTAS : MV_QUIETA) : (k == POCHI_CHAPI ? MV_ECHA : (k == POCHI_MORITA ? MV_LENTO : MV_QUIETA))) : MV_LEJOS;
    default: return actor ? PA_GEN[s_pa_tipo][k + 1].mv : MV_LEJOS;
    }
}
/* el cachorro durante el encuentro (false: ya terminó) */
static bool pa_cachorro(int i, int *f, float *oy, int *rot)
{
    if (!s_pa_on || s_po_fase != PO_ANIMAL || i != 0) return false;
    cach_t *c = &s_pc[i];
    int pose = 0;
    pa_mov(pa_papel(i, s_ms - s_po_t0), s_ms - s_po_t0, s_po_x, s_po_y, &c->x, &c->y, &c->esp, &pose, oy, rot);
    c->paso = pose == 1;
    *f = pose == 1 ? ((s_ms / 80) % 2 ? 2 : 3) : (pose == 0 ? pc_quieto(c) : 4);
    return true;
}
/* Pochi durante el encuentro, y los textos (cada cuadro) */
static void pa_paso(uint32_t t, art_icono_t *ico, float *oy, int *rot)
{
    if (!pa_vivo() && s_pa_st < 90) { /* el animal se fue antes de tiempo */
        if (s_pa_st > 0 && t > 1500) po_di("¡SE FUE! ¡CHAO, VECINO!", pa_ico(), 2200);
        pa_fin(); return;
    }
    float ax, ay, gy;
    pa_pos(&ax, &ay, &gy);
    int pose = 0;
    pa_mov(pa_papel(-1, t), t, s_mascota.x, s_mascota.y, &s_po_x, &s_po_y, &s_po_espejo, &pose, oy, rot);
    *ico = pose == 1 ? ((s_ms / 110) % 2 ? ICO_POCHI_C : ICO_POCHI_D) : (pose == 2 ? ICO_POCHI_SENT : (pose == 3 ? ICO_POCHI_COME : ((s_ms / 220) % 2 ? ICO_POCHI : ICO_POCHI_B)));
    const int k = s_pc_n == 1 ? s_pc[0].k : -1;
    const cach_t *c = s_pc_n == 1 ? &s_pc[0] : NULL;
    const art_icono_t ia = pa_ico();
    uint32_t dur = 8500;
    switch (s_pa_tipo) {
    case AT_GRANDE:
        if (s_pa_st == 0 && t > 500) { s_pa_st = 1; po_di(c ? "¡ATRÁS DE MÍ! ¡YO LOS CUIDO! (AY, QUÉ GRANDOTE...)" : "¡ATRÁS DE MÍ! ¡YO TE CUIDO! (AY, QUÉ GRANDOTE...)", ia, 3000); accion(UI_ACCION_SONAR, SND_POCHI_LADRA); }
        if (s_pa_st == 1 && t > 3600 && k == POCHI_RONY) { s_pa_st = 2; po_di("¡RONY, VEN PARA ACÁ! ¡ESO NO ES UN JUGUETE!", ICO_POCHI, 2600); }
        if (s_pa_st < 3 && t > 6200) { s_pa_st = 3; po_di("¿VISTE? ¡NI SE ACERCÓ! ...UF", ICO_POCHI, 2600); po_responde("¡QUÉ VALIENTE, POCHI!", "¡GRACIAS POR CUIDARNOS, POCHI!", 2800); }
        if (t > 2000 && t < 6000) s_po_x += (s_ms / 50) % 2 ? 0.7f : -0.7f; /* valiente... y temblando */
        break;
    case AT_ZORRILLO:
        if (s_pa_st == 0 && t > 400) { s_pa_st = 1; po_di("¡GUAU! ¿Y TÚ QUIÉN ERES, RAYADITO?", ia, 2200); accion(UI_ACCION_SONAR, SND_POCHI_LADRA); }
        if (s_pa_st == 1 && t > 2600) {
            s_pa_st = 2; po_di("¡LEVANTÓ LA COLA! ¡CORRAAAN!", ICO_ALERTA, 2400); accion(UI_ACCION_SONAR, SND_POCHI_PIDE);
            po_responde("¡PUAJ! ¡QUÉ OLOR!", "¡AAAY, CORRAN!", 2600);
        }
        if (t > 2600 && t < 5600 && s_ms % 140 < TICK_MS) chispa((int)ax + (int)(azar() % 50) - 25, (int)ay - 10, 0x9ACD32, (float)((int)(azar() % 7) - 3) * 0.3f, -0.5f);
        if (s_pa_st == 2 && t > 5400) {
            s_pa_st = 3;
            if (k == POCHI_CHAPI) { po_di("¡CHAPI! ¡LLEGASTE TARDE! AHORA SÍ QUE HUELES A CHANCHITO...", ICO_POCHI, 3200); if (c) flotar(ICO_NARIZ, NULL, 0, (int)c->x, (int)c->y - 50, 0); }
            else po_di("¡UF! ¡POR POQUITO! CON ESE NO SE JUEGA", ICO_POCHI, 2800);
        }
        if (k == POCHI_CHAPI && c && t > 5400 && s_ms % 300 < TICK_MS) chispa((int)c->x, (int)c->y - 20, 0x9ACD32, 0, -0.5f);
        dur = 9000;
        break;
    case AT_ERIZO: {
        const float px = s_pa_actor < 0 ? s_po_x : c->x, py = s_pa_actor < 0 ? s_po_y : c->y;
        if (s_pa_st == 0 && t > 500) { s_pa_st = 1; po_di(s_pa_actor < 0 ? "¿Y ESTA PELOTITA? A VER, LA HUELO..." : po_buf("%s VA A OLER ESA PELOTITA...", PC_NOMBRE[k]), ia, 2400); }
        if (s_pa_st == 1 && t > 3000) {
            s_pa_st = 2; flotar(ICO_LAGRIMA, NULL, 0, (int)px, (int)py - 50, 0); accion(UI_ACCION_SONAR, SND_POCHI_PIDE);
            po_di(s_pa_actor < 0 ? "¡AY, AY, AY! ¡PINCHA! ¡MI NARICITA!" : po_buf("¡AY! ¡%s SE PINCHÓ LA NARIZ!", PC_NOMBRE[k]), ICO_NARIZ, 2600);
            if (s_pa_actor < 0) po_responde("¡POBRE POCHI! ¡ES UN ERIZO!", "¡CUIDADO, POCHI, QUE PINCHA!", 2800);
        }
        if (s_pa_st == 2 && t > 5600) {
            s_pa_st = 3;
            if (s_pa_actor >= 0) { po_di("YA PASÓ, MI AMOR. UN LENGÜETAZO Y LISTO", ICO_CORAZON, 2800); s_pc_fa = 0; po_truco(TR_LAME, 2600); }
            else po_di("YA APRENDÍ: A LOS ERIZOS SE LES SALUDA DE LEJITOS", ICO_POCHI, 2800);
        }
        dur = 9000;
        break;
    }
    case AT_LADRON: { /* le roba su cosita y se la lleva en el pico; al final la suelta */
        static const char *const QUE[PET_POCHI_CACHORROS] = {"SU RAMITA A CHAPI", "SU FLORCITA A PASITA", "SU HOJITA A MORITA", "SU COSITA A RONY"};
        static const art_icono_t QI[PET_POCHI_CACHORROS] = {ICO_RAMITA, ICO_FLOR, ICO_HOJA, ICO_CHAPITA};
        const art_icono_t qi = s_pa_actor < 0 ? ICO_POCHI_PLATO : QI[k];
        if (s_pa_st == 0 && t > 400) {
            s_pa_st = 1; accion(UI_ACCION_SONAR, SND_POCHI_LADRA);
            po_di(s_pa_actor < 0 ? "¡OYE! ¡ESAS PAPAS SON MÍAS! ¡LADRONA!" : po_buf("¡OYE! ¡LE ROBÓ %s!", QUE[k]), ia, 2800);
        }
        if (s_pa_st == 1 && t > 3400 && k == POCHI_RONY && s_pa_actor >= 0) { s_pa_st = 2; po_di("AHORA SABES LO QUE SE SIENTE, RONY... ¡JIJI!", ICO_POCHI, 2600); }
        if (t < 6500) po_cosa(qi, 2, ax + 14.0f, ay + 6.0f);
        else {
            const float cae = (float)(t - 6500) * 0.12f;
            po_cosa(qi, 2, ax + 14.0f, ay + 6.0f + (cae > gy - ay ? gy - ay : cae));
            if (s_pa_st < 3) { s_pa_st = 3; po_di("¡LA SOLTÓ! ¡GANAMOS!", ICO_CORAZON, 2400); po_responde("¡BIEN HECHO!", "¡JAJA! ¡LA RECUPERARON!", 2600); accion(UI_ACCION_SONAR, SND_POCHI_FELIZ); }
        }
        dur = 9200;
        break;
    }
    case AT_BUHO:
        if (s_pa_st == 0 && t > 600) { s_pa_st = 1; po_di("¡UUUH! ¡UUUH!", ia, 2200); }
        if (t > 900 && t < 4200) { *ico = ICO_POCHI_SENT; *oy = 0; s_po_x += (s_ms / 60) % 2 ? 0.6f : -0.6f; } /* tiesa */
        if (s_pa_st == 1 && t > 2800) { s_pa_st = 2; po_di("...¿QUIÉN DIJO ESO?", ICO_PREGUNTA, 2000); }
        if (s_pa_st == 2 && t > 5200) { s_pa_st = 3; po_di(c ? "ERA EL BÚHO NOMÁS... NOSOTROS NO NOS ASUSTAMOS, ¿CIERTO?" : "ERA EL BÚHO NOMÁS... YO NO ME ASUSTÉ, ¿AH?", ICO_POCHI, 3000); po_responde("¡JAJA! ¡CLARO QUE NO!", "MMM... ¡UN POQUITO SÍ!", 3200); }
        break;
    case AT_CHUCAO:
        if (s_pa_st == 0 && t > 400) { s_pa_st = 1; accion(UI_ACCION_SONAR, SND_CHUCAO); po_di("¡CHU-CAO!", ia, 1600); }
        if (s_pa_st == 1 && t > 2200) { s_pa_st = 2; po_di("¡YO TAMBIÉN SÉ! ¡CHU-GUAU! ¡CHU-GUAU!", ICO_POCHI, 2600); accion(UI_ACCION_SONAR, SND_POCHI_LADRA); }
        if (s_pa_st == 2 && t > 5000) { s_pa_st = 3; accion(UI_ACCION_SONAR, SND_CHUCAO); po_di("¿VISTE? ¡ME CONTESTÓ! SOMOS IGUALITOS", ICO_CORAZON, 2600); po_responde("¡JAJA! CASI, POCHI", "¡CHU-GUAU! ¡QUÉ BUENA!", 2800); }
        dur = 8000;
        break;
    case AT_AMIGO: {
        static const char *const NUEVO[PET_POCHI_CACHORROS] = {"¡CHAPI Y LA RANA SE HICIERON AMIGOS! LOS DOS ADORAN EL BARRO", "¡LA MARIPOSA SE LE POSÓ A PASITA EN LA NARIZ! ¡YA SON AMIGAS!",
            "MORITA SE ATREVIÓ CON EL CARACOL... ¡YA SON AMIGOS!", "¡RONY Y LA ARDILLA! ¡IGUAL DE LOCOS LOS DOS! ¡YA SON AMIGOS!"};
        static const char *const OTRA[PET_POCHI_CACHORROS] = {"¡CHAPI SALUDA A SU AMIGA LA RANA!", "¡PASITA SALUDA A SU AMIGA LA MARIPOSA!", "¡MORITA SALUDA A SU AMIGO EL CARACOL!", "¡RONY SALUDA A SU AMIGA LA ARDILLA!"};
        if (s_pa_st == 0 && t > 1800) { s_pa_st = 1; po_di((s_pa_amigo_nuevo ? NUEVO : OTRA)[k], ia, 3400); accion(UI_ACCION_SONAR, SND_POCHI_FELIZ); accion(UI_ACCION_POCHI, PET_POCHI_F_AMIGO + k); }
        if (s_pa_st == 1 && t > 5200) { s_pa_st = 3; po_responde("¡QUÉ LINDA AMISTAD!", "AWWW... ¡QUÉ TIERNOS!", 300); }
        if (c && t > 1800 && s_ms % 800 < TICK_MS) flotar(ICO_CORAZON, NULL, 0, (int)(c->x + ax) / 2, (int)c->y - 50, 0);
        dur = 8000;
        break;
    }
    default: /* la reacción de cada uno a ese tipo de animal */
        if (s_pa_st == 0 && t > 1600) {
            s_pa_st = 1; po_di(PA_GEN[s_pa_tipo][s_pa_actor < 0 ? 0 : k + 1].t, ia, 3200);
            accion(UI_ACCION_SONAR, s_pa_tipo == AT_PAJARO || s_pa_tipo == AT_AGUA ? SND_POCHI_LADRA : SND_POCHI_FELIZ);
        }
        if (s_pa_st == 1 && t > 5000) { s_pa_st = 3; po_responde(PA_RESP[s_pa_tipo][0], PA_RESP[s_pa_tipo][1], 300); }
        if (s_pa_tipo == AT_MANSO && c && s_pa_actor >= 0 && t > 2500 && s_ms % 900 < TICK_MS) flotar(ICO_CORAZON, NULL, 0, (int)c->x, (int)c->y - 50, 0);
        dur = 8000;
        break;
    }
    if (s_pa_src == 2 && s_pa_tipo != AT_LADRON) po_cosa((int)s_pa_fico, 3, s_pa_fx, s_pa_fy - 12.0f); /* (el animal de mentira de las pruebas, y el chucao) */
    if (s_pa_src == 2 && s_pa_tipo == AT_LADRON && t < 300) po_cosa((int)s_pa_fico, 3, s_pa_fx, s_pa_fy - 12.0f);
    if (t > dur) { accion(UI_ACCION_POCHI, PET_POCHI_F_ANIMAL); pa_fin(); }
}
static void po_saludo_cachorro(void)
{
    if (!s_pet) return;
    /* (1.0.36) a uno que ya conoces no te lo presenta de nuevo: te cuenta a qué vino. Y al presentarlo queda anotado
     * (PET_POCHI_F_CONOCE): antes solo se anotaba al terminar su momento, y si la visita no llegaba hasta ahí, la próxima
     * vez lo volvía a presentar */
    static const char *const PRES[PET_POCHI_CACHORROS] = {"¡TE PRESENTO A CHAPI! ES UN COCHINITO... ¡PERO MUY AMOROSO!",
        "¡ELLA ES PASITA, MI HIJITA! ES LA MÁS TIERNA", "¡ELLA ES MORITA! ES TIERNA Y UN POQUITO TÍMIDA",
        "¡TE PRESENTO A RONY! ES EL MÁS LOCO DE MIS CACHORROS"};
    static const char *const HOY[PET_POCHI_CACHORROS][3] = {
        {"¡CHAPI TE QUISO VENIR A DAR UN ABRAZO! ¡OJO, QUE VIENE CON BARRO!", "¡HOY SE VINO CHAPI! ¡OJO, QUE VIENE CON GANAS DE ABRAZAR!",
         "¡CHAPI TE EXTRAÑABA! SE REVOLCÓ TODO EL CAMINO DE CONTENTO"},
        {"¡PASITA TE QUISO BUSCAR UNA FLORCITA!", "¡HOY ME ACOMPAÑA PASITA! PREGUNTÓ POR TI TODA LA MAÑANA",
         "¡PASITA QUERÍA VERTE OTRA VEZ!"},
        {"¡MORITA TE QUISO VENIR A VER! AUNQUE ESTÁ UN POCO VERGONZOSA", "¡HOY VINO MORITA! SE ACUERDA DE TI, PERO LE DA VERGÜENZA",
         "¡MORITA ME PIDIÓ VENIR! DICE QUE TE GUARDÓ UN BESITO"},
        {NULL, NULL, "¡HOY SE VINO RONY! ¡CUIDA TUS COSAS CHIQUITAS!"}};
    int conocidos = 0;
    for (int i = 0; i < s_pc_n; i++) conocidos += pet_pochi_pup_met(s_pet, s_pc[i].k);
    if (s_pc_n == PET_POCHI_CACHORROS && conocidos == s_pc_n) po_di("¡HOY VINIMOS TODOS! CHAPI, PASITA, MORITA Y RONY", ICO_CORAZON, 3600);
    else if (s_pc_n >= 2 && conocidos == s_pc_n) {
        char t[100]; snprintf(t, sizeof t, "¡HOY SE VINIERON %s Y %s CONMIGO!", PC_NOMBRE[s_pc[0].k], PC_NOMBRE[s_pc[1].k]);
        po_di(t, ICO_CORAZON, 3200);
    } else for (int i = 0; i < s_pc_n; i++) {
        const int k = s_pc[i].k;
        if (!pet_pochi_pup_met(s_pet, k)) { po_di(PRES[k], ICO_CORAZON, 3600); continue; }
        const int v = (int)(azar() % 3);
        if (k == POCHI_RONY && v < 2) /* lo que te trae ya está elegido (pc_empezar): «¡RONY TE QUISO TRAER UNA TAPITA!» */
            po_di(po_buf("¡RONY TE QUISO TRAER UNA %s!", PC_COSAS_N[(unsigned)s_pc[i].cosa_k < 4u ? s_pc[i].cosa_k : 3]), ICO_CORAZON, 3600);
        else po_di(HOY[k][v] ? HOY[k][v] : HOY[k][2], ICO_CORAZON, 3600);
    }
    for (int i = 0; i < s_pc_n; i++) if (!pet_pochi_pup_met(s_pet, s_pc[i].k)) accion(UI_ACCION_POCHI, PET_POCHI_F_CONOCE + s_pc[i].k);
}
/* ---- (1.0.30) la tapita: Pochi la deja a sus pies; tocas donde quieras que caiga, sale volando, ella corre, la trae
 * en la boca y te la devuelve. Tres veces. Si no la tiras, te la vuelve a pedir y al final la tira ella. ---- */
static int s_po_tp, s_po_tp_n, s_po_tp_aviso;
static uint32_t s_po_tp_t0;
static float s_po_tp_ax, s_po_tp_ay, s_po_tp_bx, s_po_tp_by;
static void po_tapita_empezar(void)
{
    s_po_fase = PO_TAPITA; s_po_t0 = s_ms; s_po_tp = 0; s_po_tp_n = 0; s_po_tp_aviso = 0; s_po_tp_t0 = s_ms;
}
static void po_tapita_tirar(float x, float y)
{
    limitar(&x, &y);
    if (hypotf(x - s_po_cosa_x, y - s_po_cosa_y) < 50.0f) x += x < CX ? 70.0f : -70.0f; /* que vuele un poquito al menos */
    limitar(&x, &y);
    if (!s_interior && s_escena == ESC_PRADO) { /* al estanque no: cae en la orilla (Pochi no se mete al agua) */
        const float ex = (x - 106.0f) / 72.0f, ey = (y - 300.0f) / 42.0f, d = sqrtf(ex * ex + ey * ey);
        if (d < 1.25f) { x = 106.0f + 72.0f * 1.4f; limitar(&x, &y); } /* (a la derecha del estanque: hacia abajo no se puede caminar) */
    }
    if (!s_interior && s_escena == ESC_LAGO && y < (float)terreno_ymin(x) + 6.0f) y = (float)terreno_ymin(x) + 10.0f;
    s_po_tp_ax = s_po_cosa_x; s_po_tp_ay = s_po_cosa_y; s_po_tp_bx = x; s_po_tp_by = y;
    s_po_tp = 2; s_po_tp_t0 = s_ms;
    accion(UI_ACCION_SONAR, SND_POCHI_FELIZ);
}
static bool po_tapita_toque(int x, int y) /* (fondo_cb) */
{
    if (s_po_fase != PO_TAPITA || s_po_tp != 1 || s_po_truco || po_en_transito()) return false;
    po_tapita_tirar((float)x, (float)y + 10.0f);
    return true;
}
static void po_tapita_paso(float tx, float ty, art_icono_t *ico, float *oy)
{
    const uint32_t t = s_ms - s_po_tp_t0;
    const float suelo_x = (s_po_x + s_mascota.x) * 0.5f, suelo_y = s_mascota.y + 16.0f;
    switch (s_po_tp) {
    case 0: /* se acerca y la deja en el suelo, entre las dos */
        po_mover(tx, ty, 3.0f);
        *ico = s_po_corre ? ((s_ms / 300) % 2 ? ICO_POCHI : ICO_POCHI_B) : ICO_POCHI_SENT;
        if (!s_po_corre) po_cosa(ICO_CHAPITA, 2, s_po_x + (s_po_espejo ? -20.0f : 20.0f), s_po_y - 20.0f);
        if (!s_po_corre && t > 2600) {
            po_cosa(ICO_CHAPITA, 2, suelo_x, suelo_y);
            po_di(s_nino ? "¡TÍRAMELA!" : "¡TÍRAMELA! TOCA DONDE QUIERAS QUE CAIGA", ICO_CHAPITA, 3200);
            s_po_tp = 1; s_po_tp_t0 = s_ms;
        }
        break;
    case 1: /* esperando, moviendo la cola, mirando la tapita */
        po_cosa(ICO_CHAPITA, 2, s_po_cosa_x, s_po_cosa_y);
        s_po_espejo = s_po_cosa_x < s_po_x;
        *ico = (s_ms / 200) % 2 ? ICO_POCHI : ICO_POCHI_B;
        *oy = -fabsf(sinf((float)s_ms / 160.0f)) * 3;
        if (t > 12000) {
            s_po_tp_t0 = s_ms;
            if (++s_po_tp_aviso < 3) { po_di(s_po_tp_aviso == 1 ? "¡TÍRALA, TÍRALA! ¡TOCA EN CUALQUIER PARTE!" : "¿NO ME LA TIRAS? ¡GUAU!", ICO_CHAPITA, 2800); accion(UI_ACCION_SONAR, SND_POCHI_PIDE); }
            else { /* la tira ella */
                s_po_tp_aviso = 0;
                po_responde("¡AHÍ VA, POCHI!", "¡AHÍ VA!", 200);
                po_tapita_tirar(s_mascota.x + (s_mascota.x < CX ? 150.0f : -150.0f), s_mascota.y + (float)((int)(azar() % 80) - 40));
            }
        }
        break;
    case 2: { /* vuela en arco; ella ya sale corriendo */
        float f = (float)t / 650.0f;
        if (f > 1.0f) f = 1.0f;
        const float x = s_po_tp_ax + (s_po_tp_bx - s_po_tp_ax) * f, y = s_po_tp_ay + (s_po_tp_by - s_po_tp_ay) * f - sinf(f * 3.14159f) * 90.0f;
        po_cosa(ICO_CHAPITA, 2, x, y);
        if (t > 250) po_mover(s_po_tp_bx, s_po_tp_by, 4.6f);
        *ico = s_po_corre ? ((s_ms / 110) % 2 ? ICO_POCHI_C : ICO_POCHI_D) : ICO_POCHI;
        if (f >= 1.0f) { s_po_tp = 3; s_po_tp_t0 = s_ms; for (int k = 0; k < 3; k++) chispa((int)s_po_tp_bx - 10 + k * 10, (int)s_po_tp_by - 4, 0xFFF3C8, (float)(k - 1) * 0.6f, -0.8f); }
        break;
    }
    case 3: /* corre a buscarla */
        po_mover(s_po_tp_bx + (s_po_x < s_po_tp_bx ? -16.0f : 16.0f), s_po_tp_by, 4.6f);
        *ico = s_po_corre ? ((s_ms / 110) % 2 ? ICO_POCHI_C : ICO_POCHI_D) : ICO_POCHI_COME;
        if (!s_po_corre) { s_po_tp = 4; s_po_tp_t0 = s_ms; accion(UI_ACCION_SONAR, SND_POCHI_LADRA); }
        break;
    case 4: /* la toma con la boca y te la trae */
        if (t > 350) po_mover(tx, ty, 3.6f);
        po_cosa(ICO_CHAPITA, 2, s_po_x + (s_po_espejo ? -22.0f : 22.0f), s_po_y - 22.0f);
        *ico = s_po_corre ? ((s_ms / 130) % 2 ? ICO_POCHI_C : ICO_POCHI_D) : ICO_POCHI_SENT;
        if (t > 350 && !s_po_corre) {
            s_po_tp_n++;
            flotar(ICO_CORAZON, NULL, 0, (int)s_po_x, (int)s_po_y - 70, 0);
            if (s_po_tp_n >= 3) {
                po_cosa(-1, 0, 0, 0);
                po_di("¡QUÉ RICO JUGAR CONTIGO! ¡ME CANSÉ!", ICO_CORAZON, 3200);
                po_responde("¡ERES MUY RÁPIDA, POCHI!", "¡QUÉ ENTRETENIDO, POCHI!", 3400);
                accion(UI_ACCION_POCHI, PET_POCHI_F_MANIA); /* (jugaron: diversión y alegría) */
                accion(UI_ACCION_SONAR, SND_POCHI_FELIZ);
                s_po_fase = PO_INVITA; s_po_t0 = s_ms; s_po_pregunta = 0; /* (se queda un ratito y se despide) */
            } else {
                static const char *const OTRA[2] = {"¡OTRA, OTRA!", "¡UNA MÁS! ¡MÁS LEJOS!"};
                po_cosa(ICO_CHAPITA, 2, suelo_x, suelo_y);
                po_di(OTRA[s_po_tp_n - 1], ICO_CHAPITA, 2400);
                accion(UI_ACCION_SONAR, SND_POCHI_FELIZ);
                s_po_tp = 1; s_po_tp_t0 = s_ms; s_po_tp_aviso = 0;
            }
        }
        break;
    }
}
int ui_test_po_estado(void) { return s_po_fase * 1000 + s_po_tipo * 100 + s_po_tp * 10 + s_po_tp_n; } /* (1.0.30, pruebas) */
static void po_acurruca_empezar(void)
{
    s_po_dormida_visita = true;
    po_empezar();
    s_pc_n = 0; pc_esconder();
    s_po_fase = PO_ACURRUCA; s_po_t0 = s_ms; s_po_pregunta = 0;
}
/* las manías: la ramita robada (prado y bosque), la luciérnaga (claro, y el bosque de noche), el cóndor (mirador), el agua (lago e isla) */
static void po_mania_paso(uint32_t t, art_icono_t *ico, float *oy)
{
    const float mx = s_mascota.x, my = s_mascota.y + 8;
    const bool ladoi = s_po_x < mx;
    if (s_po_mania == MAN_RAMITA) {
        if (s_po_mst == 0) { /* va a la ramita y la agarra */
            po_mover(s_po_cosa_x, s_po_cosa_y + 6, 3.0f);
            *ico = s_po_corre ? ((s_ms / 110) % 2 ? ICO_POCHI_C : ICO_POCHI_D) : ICO_POCHI_COME;
            if ((!s_po_corre && t > 600) || t > 5000) {
                s_po_mst = 1; s_po_t0 = s_ms;
                po_di("¡ME ROBÉ UNA RAMITA! ¡ATRÁPAME!", ICO_RAMITA, 2600);
                po_responde("¡POCHI, DEVUÉLVEMELA!", "¡TE VOY A PILLAR, POCHI!", 2800);
                accion(UI_ACCION_SONAR, SND_POCHI_FELIZ);
            }
            if (s_po_mst == 0) po_cosa(ICO_RAMITA, 3, s_po_cosa_x, s_po_cosa_y);
        } else if (s_po_mst == 1) { /* arranca para que la persigan (tócala para pillarla) */
            const float lado = ladoi ? -1.0f : 1.0f;
            float tx = mx + lado * 150.0f + sinf((float)s_ms / 400.0f) * 40.0f, ty = my + cosf((float)s_ms / 530.0f) * 40.0f;
            if (tx < 60.0f || tx > (float)(W - 60)) tx = mx - lado * 150.0f;
            limitar(&tx, &ty);
            po_mover(tx, ty, 4.2f);
            *ico = (s_ms / 70) % 2 ? ICO_POCHI_C : ICO_POCHI_D;
            *oy = -fabsf(sinf((float)s_ms / 80.0f)) * 3.0f;
            if (s_ms % 1400 < TICK_MS) accion(UI_ACCION_SONAR, SND_POCHI_FELIZ);
            po_cosa(ICO_RAMITA, 3, s_po_x + (s_po_espejo ? -28.0f : 28.0f), s_po_y - 30);
            if (t > 8500) { s_po_mst = 3; s_po_t0 = s_ms; }
        } else if (s_po_mst == 2) { /* ¡la pillaste! */
            *ico = ICO_POCHI_SENT;
            if (s_po_hablo != 50) {
                s_po_hablo = 50; po_cosa(-1, 0, 0, 0);
                po_di("¡ME PILLASTE! ¡TOMA, TE LA REGALO!", ICO_RAMITA, 2600);
                po_responde("¡GRACIAS, POCHI!", "¡TE PILLÉ, POCHI!", 2800);
                accion(UI_ACCION_POCHI, PET_POCHI_F_MANIA);
                flotar(ICO_RAMITA, NULL, 0, (int)mx, (int)my - 70, 0);
            }
            if (t > 3200) po_mania_fin();
        } else { /* nadie la pilló: vuelve orgullosa y la suelta */
            po_mover(mx + (ladoi ? -60.0f : 60.0f), my, 3.0f);
            *ico = s_po_corre ? ((s_ms / 110) % 2 ? ICO_POCHI_C : ICO_POCHI_D) : ICO_POCHI_SENT;
            if (!s_po_corre && s_po_hablo != 51) {
                s_po_hablo = 51;
                po_di("¡JIJI! ¡NADIE ME ALCANZA! ¡SOY LA MÁS RÁPIDA DEL MUNDO MUNDIAL!", ICO_POCHI, 3200);
                accion(UI_ACCION_POCHI, PET_POCHI_F_MANIA);
            }
            if (s_po_hablo == 51) po_cosa(ICO_RAMITA, 3, s_po_x + (s_po_espejo ? -34.0f : 34.0f), s_po_y - 2);
            else po_cosa(ICO_RAMITA, 3, s_po_x + (s_po_espejo ? -28.0f : 28.0f), s_po_y - 30);
            if (t > 6500) po_mania_fin();
        }
        return;
    }
    if (s_po_mania == MAN_LUZ) {
        if (!s_po_luz) {
            s_po_luz = lv_obj_create(s_mundo);
            lv_obj_remove_style_all(s_po_luz);
            lv_obj_set_size(s_po_luz, 13, 13); /* (de día, con contorno y brillo para que se vea sobre el pasto) */
            lv_obj_set_style_radius(s_po_luz, LV_RADIUS_CIRCLE, 0);
            lv_obj_set_style_bg_color(s_po_luz, lv_color_hex(0xFFE95C), 0);
            lv_obj_set_style_bg_opa(s_po_luz, LV_OPA_COVER, 0);
            lv_obj_set_style_border_width(s_po_luz, 2, 0);
            lv_obj_set_style_border_color(s_po_luz, lv_color_hex(0x8A6A10), 0);
            lv_obj_set_style_shadow_width(s_po_luz, 18, 0);
            lv_obj_set_style_shadow_spread(s_po_luz, 2, 0);
            lv_obj_set_style_shadow_color(s_po_luz, lv_color_hex(0xFFF6A8), 0);
            lv_obj_set_style_shadow_opa(s_po_luz, LV_OPA_COVER, 0);
            lv_obj_remove_flag(s_po_luz, LV_OBJ_FLAG_CLICKABLE);
        }
        const float cx0 = mx + (ladoi ? -70.0f : 70.0f);
        float lx = cx0 + sinf((float)t / 430.0f) * 80.0f + sinf((float)t / 170.0f) * 18.0f, ly = my - 70.0f + cosf((float)t / 310.0f) * 34.0f;
        if (t > 8200) { ly -= (float)(t - 8200) * 0.12f; lx += (float)(t - 8200) * 0.03f; }
        lv_obj_set_pos(s_po_luz, (int)lx - 6, (int)ly - 6);
        int opa = (s_ms / 300) % 3 ? 255 : 120;
        if (t > 8200) opa = opa * (int)(t < 10200 ? 10200 - t : 0) / 2000;
        lv_obj_set_style_opa(s_po_luz, (lv_opa_t)opa, 0);
        ocultar(s_po_luz, s_interior || po_en_transito());
        lv_obj_move_foreground(s_po_luz);
        if (t < 8200) {
            float gx = lx, gy = ly + 66.0f;
            limitar(&gx, &gy);
            po_mover(gx, gy, 3.4f);
            *ico = s_po_corre ? ((s_ms / 90) % 2 ? ICO_POCHI_C : ICO_POCHI_D) : ICO_POCHI;
            if (hypotf(s_po_x - lx, s_po_y - 60.0f - ly) < 70.0f) *oy = -fabsf(sinf((float)s_ms / 90.0f)) * 16.0f; /* ¡salta! */
        } else { *ico = ICO_POCHI_SENT; s_po_espejo = lx < s_po_x; }
        if (s_po_hablo == 0) { s_po_hablo = 60; po_di("¡UNA LUCIÉRNAGA! ¡LA VOY A PILLAR!", ICO_POCHI, 2400); accion(UI_ACCION_SONAR, SND_POCHI_FELIZ); }
        if (t > 4800 && s_po_hablo == 60) { s_po_hablo = 61; po_di("¡CASI! ¡CASI LA PILLO!", ICO_POCHI, 2200); }
        if (t > 8400 && s_po_hablo == 61) {
            s_po_hablo = 62;
            po_di("¡SE ME ESCAPÓ! ¡LA PRÓXIMA SÍ!", ICO_POCHI, 2600);
            po_responde("¡CASI, POCHI!", "¡QUÉ SALTARINA ERES, POCHI!", 2800);
            accion(UI_ACCION_POCHI, PET_POCHI_F_MANIA);
        }
        if (t > 11000) po_mania_fin();
        return;
    }
    if (s_po_mania == MAN_CONDOR) {
        const float cxp = -60.0f + (float)t * 0.065f, cyp = 104.0f + sinf((float)t / 600.0f) * 10.0f;
        if (cxp < (float)(W + 60)) po_cosa(ICO_CONDOR_B, 4, cxp, cyp); else po_cosa(-1, 0, 0, 0);
        if (t < 3000) {
            *ico = (s_ms / 180) % 2 ? ICO_POCHI : ICO_POCHI_B;
            s_po_espejo = cxp < s_po_x;
            *oy = (s_ms / 200) % 2 ? -6.0f : 0;
            if (s_po_hablo == 0) { s_po_hablo = 70; po_di("¡GUAU! ¡GUAU! ¡VETE, PAJARRACO!", ICO_POCHI, 2400); }
            if (s_ms % 600 < TICK_MS) accion(UI_ACCION_SONAR, SND_POCHI_LADRA);
        } else if (t < 9000) { /* se esconde detrás de ella, temblando */
            if (s_po_hablo == 70) {
                s_po_hablo = 71;
                po_di("¡AY! ES MUY GRANDE... ME ESCONDO DETRÁS DE TI", ICO_POCHI, 2800);
                po_responde("¡NO TENGAS MIEDO, POCHI! ¡EL CÓNDOR ES BUENO!", "¡TRANQUILA, POCHI! YO TE CUIDO", 3000);
            }
            po_mover(mx + (ladoi ? -30.0f : 30.0f), my - 6, 3.0f);
            if (s_po_corre) *ico = (s_ms / 90) % 2 ? ICO_POCHI_C : ICO_POCHI_D;
            else { *ico = ICO_POCHI_SENT; s_po_espejo = s_po_x > mx; s_po_x += (s_ms / 50) % 2 ? 0.8f : -0.8f; }
        } else {
            if (s_po_hablo == 71) { s_po_hablo = 72; po_di("¿YA SE FUE? ¡JA! ¡LE DIO MIEDO DE MÍ!", ICO_POCHI, 2800); accion(UI_ACCION_POCHI, PET_POCHI_F_MANIA); }
            *ico = ICO_POCHI; *oy = -fabsf(sinf((float)s_ms / 150.0f)) * 5.0f;
        }
        if (t > 11500) po_mania_fin();
        return;
    }
    /* el agua: se acerca, la huele y salta para atrás */
    if (t < 1400) { po_mover(mx + (ladoi ? -50.0f : 50.0f), my + 26, 1.4f); *ico = s_po_corre ? ((s_ms / 160) % 2 ? ICO_POCHI_C : ICO_POCHI_D) : ICO_POCHI; }
    else if (t < 2400) {
        *ico = ICO_POCHI_COME; /* olfatea */
        if (s_po_hablo == 0) { s_po_hablo = 80; po_di(s_pet && zona_fria(s_pet) ? "¿Y ESTO? EL AGUA ESTÁ DURITA... ¡IGUAL NO ME FÍO!" : "¿Y ESTO? ¿AGUA?", ICO_POCHI, 1800); }
    } else if (t < 3400) {
        if (s_po_hablo == 80) {
            s_po_hablo = 81;
            po_di("¡UY, EL AGUA! ¡NO, NO, NO! ¡LAS PATITAS NO SE MOJAN!", ICO_GOTA, 2800);
            flotar(ICO_GOTA, NULL, 0, (int)s_po_x, (int)s_po_y - 70, 0);
            accion(UI_ACCION_SONAR, SND_POCHI_PIDE);
        }
        s_po_x += (ladoi ? -2.6f : 2.6f) * s_po_dtf;
        if (s_po_x < 50.0f) s_po_x = 50.0f;
        if (s_po_x > (float)(W - 50)) s_po_x = (float)(W - 50);
        *oy = -sinf(3.14159f * (float)(t - 2400) / 1000.0f) * 24.0f;
        *ico = ICO_POCHI_C;
    } else if (t < 7000) {
        *ico = ICO_POCHI_SENT; *oy = (s_ms / 60) % 2 ? 1.0f : 0; s_po_x += (s_ms / 60) % 2 ? 0.6f : -0.6f; /* tiritando */
        if (s_po_hablo == 81) {
            s_po_hablo = 82;
            po_responde("¡JAJA, POCHI! ¡SI ES SOLO AGUA!", "¡POCHI, QUÉ MIEDOSA!", 600);
            accion(UI_ACCION_POCHI, PET_POCHI_F_MANIA);
        }
    } else po_mania_fin();
}

static void animar_pochi(void)
{
    if (!s_mundo) return;
    po_patas_actualizar();
    { static uint32_t ult; const uint32_t dt = ult ? s_ms - ult : 55; ult = s_ms; s_po_dtf = dt < 30 ? 0.55f : (dt > 330 ? 6.0f : (float)dt / 55.0f); }
    const uint32_t dtms = s_po_ult_ms ? s_ms - s_po_ult_ms : 0; /* (fw 102) para que los trucos no le coman tiempo a la fase */
    s_po_ult_ms = s_ms;
    if (s_po_resp_t && (int32_t)(s_ms - s_po_resp_ms) >= 0 && s_pet && !s_pet->dormida) {
        if (!po_globo_visible() && s_globo && lv_obj_has_flag(s_globo, LV_OBJ_FLAG_HIDDEN) && s_gpend_n == 0) {
            s_po_bypass = true;
            globo(ICO_CORAZON, s_nino ? NULL : s_po_resp_t, 2800);
            s_po_bypass = false;
            s_po_resp_t = NULL;
        } else if ((int32_t)(s_ms - s_po_resp_ms) > 6000) s_po_resp_t = NULL; /* ya no viene al caso */
    }
    po_seguir_globo();
    if (s_po_pend_n > 0 && !po_globo_visible() && s_globo && lv_obj_has_flag(s_globo, LV_OBJ_FLAG_HIDDEN) && s_gpend_n == 0 && !po_en_transito() && !mundo_en_pausa()) {
        po_pend_t g = s_po_pend[0];
        for (int i = 1; i < s_po_pend_n; i++) s_po_pend[i - 1] = s_po_pend[i];
        s_po_pend_n--;
        po_di(g.t, g.ico, g.ms);
    }
    if (s_po_fase != PO_HAMBRE) po_op_ocultar();
    if (s_po_fase == PO_NADA) { /* ¿llega de visita? (rara vez: una vez cada tanto, de día y con calma) */
        if (!s_pet || s_ms - s_po_chequeo < 5000) return;
        s_po_chequeo = s_ms;
        if (s_pet->etapa == STAGE_HUEVO || s_interior || s_panel != PANEL_NINGUNO || s_juego != JG_NADA || s_sim_on) return;
        if (s_ultimo_t < s_po_proxima || pet_weather(s_pet) == CLIMA_TORMENTA || s_escena != (int)s_pet->escena) return;
        if (s_escena == ESC_ISLA || s_pet->escena == ESC_ISLA) return; /* (1.0.23) Pochi no sabe nadar: a la isla no llega nunca */
        { /* (1.0.21) con una visita (o el vecino) no llega: dos criaturas más ponían lento el juego. Espera a que se vaya y un rato más */
            static uint32_t visita_ms;
            if (s_visita.visible || s_vecino.visible) { visita_ms = s_ms ? s_ms : 1; s_po_quiere = false; return; }
            if (visita_ms && s_ms - visita_ms < 6u * 60u * 1000u) return;
            visita_ms = 0;
        }
        if (s_pet->dormida) { if (!s_nino && azar() % 180 < 1) po_acurruca_empezar(); return; } /* (fw 102) durmiendo: rara vez llega en puntillas */
        if (po_ocupada()) { /* (fw 108) no llega en medio de una actividad: si le tocaba, espera y llega apenas termine */
            if (azar() % 60 < 1) s_po_quiere = true;
            return;
        }
        if (s_po_quiere || azar() % 60 < 1) { s_po_quiere = false; s_po_dormida_visita = false; po_empezar(); } /* se revisa cada 5 s: llega, en promedio, a los ~5 min de cumplirse la espera (antes ~83 min) */
        return;
    }
    const uint32_t t = s_ms - s_po_t0;
    art_icono_t ico = ICO_POCHI;
    float oy = 0;
    int rot = 0;
    const float tx = s_mascota.x + (float)(s_po_lado > 0 ? 64 : -64), ty = s_mascota.y + 8;
    s_po_corre = false;
    if (s_po_charco && s_po_fase != PO_BARRO) ocultar(s_po_charco, true);
    if (s_po_pf && s_po_fase != PO_PASEO) po_portal_fin();
    /* si te vas a otro mundo mientras te invita (o come, o corre): "¡espera, te acompaño!" y sigue contigo */
    if (s_po_fase == PO_PASEO || s_po_fase == PO_DESPIDE || s_po_fase == PO_SE_VA) s_po_esc_ref = s_escena;
    else if (s_escena == ESC_ISLA && s_po_fase != PO_NADA && s_po_fase != PO_ACURRUCA) { /* (1.0.23) Pochi no sabe nadar: a la isla no va (se queda en tierra y se va a su casa) */
        s_po_esc_ref = s_escena;
        po_op_ocultar(); ocultar(s_po_plato, true);
        s_po_fase = PO_SE_VA; s_po_t0 = s_ms; s_po_lado = s_po_x < CX ? -1 : 1;
        s_po_hoyno = true;
    } else if (s_escena >= 0 && s_escena != s_po_esc_ref && !po_en_transito() && s_po_pf == 0) {
        s_po_esc_ref = s_escena;
        po_coloca();
        ocultar(s_po_plato, true); po_op_ocultar();
        po_cosa(-1, 0, 0, 0); if (s_po_luz) ocultar(s_po_luz, true); s_po_truco = TR_NADA;
        pc_reubicar();
        s_po_fase = PO_INVITA; s_po_t0 = s_ms; s_po_pregunta = 0;
        po_di("¡ESPERA, TE ACOMPAÑO!", ICO_POCHI, 2600);
        accion(UI_ACCION_SONAR, SND_POCHI_LADRA);
    }
    if (s_pet && s_pet->dormida && s_po_fase != PO_SE_VA && s_po_fase != PO_NADA && s_po_fase != PO_ACURRUCA) { s_po_fase = PO_SE_VA; s_po_t0 = s_ms; }
    s_po_nieve = s_pet && (pet_season(s_pet, s_ultimo_t) == ESTACION_INVIERNO || pet_weather(s_pet) == CLIMA_NIEVE) && s_escena != ESC_CUEVA;
    { /* (fw 102) de vez en cuando, sola: se rasca el poto en el pasto o se tira de espalda con las patas al aire */
        static uint32_t chk;
        const bool quieta = s_po_fase == PO_INVITA || (s_po_fase == PO_PASEO && s_po_zona_t0 && s_ms - s_po_zona_t0 > 3000 && s_ms - s_po_zona_t0 < 15000);
        if (s_ms - chk > 1000 && quieta && po_truco_puede()) {
            chk = s_ms;
            if (!s_po_rasco && po_hay_pasto() && azar() % 100 < 9) { s_po_rasco = true; po_rasca_empezar(); }
            else if (!s_po_panzo && azar() % 100 < 5) { s_po_panzo = true; po_panza_empezar(false); }
        }
    }
    if (s_pa_on && s_po_fase != PO_ANIMAL) { s_pa_on = false; po_cosa(-1, 0, 0, 0); } /* (el encuentro se cortó: cambió la zona, se durmió...) */
    { /* (fw 102) un animal del lugar a la vista: se encuentran con él (solo si viene sola o con un cachorro; una vez por visita) */
        static uint32_t chk2;
        const bool quieta = s_po_fase == PO_INVITA || (s_po_fase == PO_PASEO && s_po_zona_t0 && s_ms - s_po_zona_t0 > 3000 && s_ms - s_po_zona_t0 < 15000);
        if (s_ms - chk2 > 1000 && quieta && po_truco_puede() && !s_pa_hecha && s_pc_n <= 1 && s_pc_turno < 0 && s_pc_fam < 0 && s_pc_esc < 0 &&
            (s_pc_n == 0 || (s_pc[0].hecho && s_pc[0].modo == PC_SIGUE)) && s_panel == PANEL_NINGUNO) {
            chk2 = s_ms;
            if ((azar() % 100 < 30 || s_pa_forzar >= 0) && pa_buscar()) pa_empezar(s_po_fase);
        }
    }
    if (s_po_truco && (s_po_fase == PO_NADA || s_po_fase == PO_SE_VA || s_po_fase == PO_ACURRUCA)) s_po_truco = TR_NADA;
    if (s_po_truco) { /* (fw 102) el truco manda; el tiempo de la fase espera */
        const uint32_t tt = s_ms - s_po_truco_t0;
        s_po_t0 += dtms;
        if (s_po_zona_t0) s_po_zona_t0 += dtms;
        if (s_po_truco == TR_PANZA) { /* de espalda, pataleando */
            ico = (s_ms / 160) % 2 ? ICO_POCHI_C : ICO_POCHI_D; rot = 1800; oy = 26.0f + ((s_ms / 160) % 2 ? 1.0f : 0);
            if (s_ms % 900 < TICK_MS) flotar(ICO_CORAZON, NULL, 0, (int)s_po_x, (int)s_po_y - 40, 0);
        } else if (s_po_truco == TR_MAREO) {
            if (tt < 1800) { ico = ICO_POCHI; rot = (int)(sinf((float)tt / 110.0f) * 260.0f); if (s_ms % 500 < TICK_MS) flotar(ICO_MAREO, NULL, 0, (int)s_po_x, (int)s_po_y - 80, 0); }
            else { ico = ICO_POCHI; rot = s_po_espejo ? -900 : 900; oy = 18.0f; } /* se cae de lado */
        } else if (s_po_truco == TR_LAME) { /* le lame la cabeza a uno de sus cachorros */
            if (s_pc_fa >= 0 && s_pc_fa < s_pc_n) {
                const cach_t *c = &s_pc[s_pc_fa];
                po_mover(c->x + (s_po_x < c->x ? -30.0f : 30.0f), c->y - 2, 2.4f);
                ico = s_po_corre ? ((s_ms / 110) % 2 ? ICO_POCHI_C : ICO_POCHI_D) : ICO_POCHI_COME;
                if (!s_po_corre) { s_po_espejo = s_po_x > c->x; oy = (s_ms / 250) % 2 ? 2.0f : 0; }
            } else s_po_truco_dur = 0;
        } else { /* se rasca el poto: sentada, se arrastra por el pasto */
            ico = ICO_POCHI_SENT; rot = (s_ms / 120) % 2 ? 60 : -60;
            s_po_x += (s_po_espejo ? -1.1f : 1.1f) * s_po_dtf;
            if (s_po_x < 50.0f) s_po_x = 50.0f;
            if (s_po_x > (float)(W - 50)) s_po_x = (float)(W - 50);
            if (s_ms % 160 < TICK_MS) chispa((int)s_po_x + (s_po_espejo ? 22 : -22), (int)s_po_y - 4, zona_fria(s_pet) ? 0xEEF4FF : 0x7CC860, s_po_espejo ? 0.5f : -0.5f, -0.6f);
        }
        if (tt > s_po_truco_dur) {
            if (s_po_truco == TR_MAREO) po_di("¡UY... TODO DA VUELTAS! ¿OTRA VEZ? ¡NO, MEJOR NO!", ICO_MAREO, 2800);
            s_po_truco = TR_NADA; rot = 0;
        }
    } else switch (s_po_fase) {
    case PO_LLEGA:
        po_mover(tx, ty, 3.6f);
        if (!s_po_corre) po_llega_fin(); /* (fw 102) según cómo esté ella y cómo terminó la vez anterior */
        break;
    case PO_HAMBRE: /* sentado junto a su platito, esperando */
        ico = ICO_POCHI_SENT; oy = 0;
        ocultar(s_po_plato, false);
        lv_obj_set_pos(s_po_plato, (int)s_po_x + (s_po_espejo ? -52 : 20), (int)s_po_y - 28);
        if (t > 7000u * (uint32_t)(s_po_pregunta + 1) && s_po_pregunta < 2) { s_po_pregunta++; po_di(s_po_pregunta == 1 ? "¿ME DAS PAPAS? ¡POR FAVOR!" : "¡GUAU! ¡PAPAS, PAPAS!", ICO_POCHI_PLATO, 3000); accion(UI_ACCION_SONAR, s_po_pregunta == 1 ? SND_POCHI_PIDE : SND_POCHI_LADRA); }
        if (t > (s_po_rapido ? 800u : 2800u) && s_panel == PANEL_NINGUNO && !po_en_transito()) po_op_mostrar(); else po_op_ocultar(); /* (la otra vez paseamos: va directo al plato) */
        if (t > 40000) { po_op_ocultar(); po_di("ESTÁ BIEN... ¡VUELVO OTRO DÍA!", ICO_POCHI, 3200); s_po_hoyno = true; s_po_fase = PO_SE_VA; s_po_t0 = s_ms; s_po_lado = s_po_x < CX ? -1 : 1; }
        break;
    case PO_COME: /* come con muchas ganas */
        ico = ICO_POCHI_COME; oy = ((s_ms / 140) % 2) ? 1.5f : 0;
        s_po_comio = true;
        ocultar(s_po_plato, false);
        lv_obj_set_pos(s_po_plato, (int)s_po_x + (s_po_espejo ? -60 : 28), (int)s_po_y - 20);
        if (t < TICK_MS * 2 || (t > 2200 && t < 2200 + TICK_MS)) accion(UI_ACCION_SONAR, SND_POCHI_COME);
        if (t > 5200) {
            ocultar(s_po_plato, true);
            s_po_fase = PO_CORRE; s_po_t0 = s_ms; s_po_pregunta = 0; s_po_dir = s_po_x < CX ? 1 : -1;
            accion(UI_ACCION_POCHI, 1);
            accion(UI_ACCION_SONAR, SND_POCHI_FELIZ);
            po_di(s_pet && zona_fria(s_pet) ? "¡PAPAS CALENTITAS! ¡MIS FAVORITAS! ¡GRACIAS!" : "¡PAPAS! ¡MIS FAVORITAS! ¡GRACIAS!", ICO_CORAZON, 3400);
            po_responde("¡DE NADA, POCHI!", "¡QUÉ BUENO QUE TE GUSTARON!", 3600);
            flotar(ICO_CORAZON, NULL, 0, (int)s_po_x, (int)s_po_y - 70, 0);
        }
        break;
    case PO_CORRE: { /* feliz con las papas: corre de lado a lado muy rápido moviendo la cola */
        s_po_x += (float)s_po_dir * 7.0f * s_po_dtf;
        if (s_po_x > CX + 140) s_po_dir = -1;
        if (s_po_x < CX - 140) s_po_dir = 1;
        s_po_espejo = s_po_dir < 0;
        s_po_y += (s_mascota.y + 8 - s_po_y) * 0.1f;
        ico = (s_ms / 60) % 2 ? ICO_POCHI_C : ICO_POCHI_D;
        oy = -fabsf(sinf((float)s_ms / 70.0f)) * 3;
        if (s_ms % 500 < TICK_MS) flotar(ICO_CORAZON, NULL, 0, (int)s_po_x, (int)s_po_y - 70, 0);
        if (s_ms % 1600 < TICK_MS) accion(UI_ACCION_SONAR, SND_POCHI_FELIZ);
        if (t > 5500) {
            /* (v39) con la panza llena, se sienta a preguntarle cosas: una charla por visita */
            const int ck = s_pet && s_po_charla_k < 0 && s_pc_turno < 0 && s_pc_fam < 0 ? pet_pochi_chat_next(s_pet) : -1; /* (no encima de lo de sus cachorros) */
            if (ck >= 0) { s_po_charla_k = ck; s_po_fase = PO_CHARLA; s_po_t0 = s_ms; s_po_hablo = 0; s_po_espejo = s_po_x > s_mascota.x; break; }
            { const int m = po_mania_de(s_escena); /* (fw 102) una de sus manías (una por visita) */
              if (m >= 0 && !s_po_mania_hecha && !s_interior && s_pc_turno < 0 && s_pc_fam < 0 && azar() % 100 < 55) { po_mania_empezar(m, PO_INVITA); break; } }
            /* a veces (no siempre) encuentra un charco de barro y se revuelca */
            if (!s_interior && (s_escena == ESC_PRADO || s_escena == ESC_BOSQUE) && azar() % 100 < 40) po_barro_empezar();
            else { s_po_fase = PO_INVITA; s_po_t0 = s_ms; s_po_pregunta = 0; }
        }
        break;
    }
    case PO_BARRO: { /* corre al charco, se revuelca y después salpica a la mascota: queda sucia (aparece el botón del baño) */
        ocultar(s_po_charco, false);
        lv_obj_set_pos(s_po_charco, (int)s_po_cx - 60, (int)s_po_cy - 8);
        if (t < 1600 && s_po_pregunta == 0) {
            po_mover(s_po_cx, s_po_cy - 6, 4.0f);
            if (t < TICK_MS * 2) { po_di("¡UN CHARCO DE BARRO! ¡GUAU!", ICO_POCHI, 2600); accion(UI_ACCION_SONAR, SND_POCHI_LADRA); }
        } else if (t < 6200) {
            if (s_po_pregunta == 0) { s_po_pregunta = 1; po_di("¡ME REVUELCO EN EL BARRO!", ICO_POCHI, 3200); po_responde("¡POCHI, NO TE REVUELQUES!", "¡QUÉ DIVERTIDO, POCHI!", 3400); }
            s_po_x += (s_po_cx - s_po_x) * 0.2f; s_po_y += (s_po_cy - 6 - s_po_y) * 0.2f;
            ico = (s_ms / 130) % 2 ? ICO_POCHI_C : ICO_POCHI_D;
            s_po_espejo = (s_ms / 260) % 2;
            oy = ((s_ms / 130) % 2) ? 3.0f : -2.0f;
            if (s_ms % 1500 < TICK_MS) accion(UI_ACCION_SONAR, SND_POCHI_FELIZ);
        } else if (t < 9200) {
            if (s_po_barro_pasos <= 0 && s_po_pregunta == 1) { s_po_barro_pasos = 14; s_pata_x0 = s_po_x; s_pata_y0 = s_po_y; } /* sale del charco con las patas embarradas */
            ico = ICO_POCHI; oy = -fabsf(sinf((float)s_ms / 90.0f)) * 4; /* se sacude el barro */
            if (s_po_pregunta == 1) {
                s_po_pregunta = 2;
                po_di("¡JIJIJI! ¡TE SALPIQUÉ!", ICO_POCHI, 2800);
                po_responde("¡POCHI! ¡ME ENSUCIASTE!", "¡AAAY, ESTOY TODA SUCIA!", 3000);
                accion(UI_ACCION_POCHI, 4); /* ella queda sucia */
                s_barro_pochi = true;
                accion(UI_ACCION_SONAR, SND_POCHI_FELIZ);
                for (int k = 0; k < 3; k++) flotar(ICO_SUDOR, NULL, 0, (int)s_mascota.x - 20 + k * 20, (int)s_mascota.y - 60, (uint32_t)k * 200);
            }
        } else { s_po_fase = PO_INVITA; s_po_t0 = s_ms; s_po_pregunta = 0; }
        break;
    }
    case PO_CHARLA: { /* (v39) Pochi pregunta como una niña curiosa; ella responde con lo que sabe; Pochi reacciona (y se distrae) */
        po_mover(tx, ty, 1.6f);
        ico = ICO_POCHI_SENT;
        const int k = s_po_charla_k;
        const bool corto = s_suave && !s_nino;
        const bool nombre = k == PET_POCHI_NOMBRE;
        const uint32_t lee = (uint32_t)strlen(pet_pochi_chat(k, 0, false)) * 40u;
        rot = s_po_hablo == 1 && t < 3000 + lee ? (s_po_espejo ? 90 : -90) : 0; /* ladea la cabeza mientras pregunta */
        if (t > 600 && s_po_hablo == 0) {
            s_po_hablo = 1;
            po_di(pet_pochi_chat(k, 0, corto), ICO_POCHI, 3000);
            po_responde(pet_pochi_chat(k, 1, corto), pet_pochi_chat(k, 1, corto), 3400 + lee);
            accion(UI_ACCION_SONAR, SND_POCHI_LADRA);
        }
        const uint32_t t2 = 3400u + lee + 4000u + (uint32_t)strlen(pet_pochi_chat(k, 1, corto)) * 90u;
        if (t > t2 && s_po_hablo == 1) {
            s_po_hablo = 2;
            po_di(pet_pochi_chat(k, 2, corto), nombre ? ICO_CORAZON : ICO_POCHI, 3200);
            accion(UI_ACCION_SONAR, nombre ? SND_POCHI_FELIZ : SND_POCHI_LADRA);
            accion(UI_ACCION_POCHI_CHARLA, k);
            for (int j = 0; j < (nombre ? 5 : 2); j++) flotar(ICO_CORAZON, NULL, 0, (int)s_po_x - 24 + j * 12, (int)s_po_y - 70 - j * 4, (uint32_t)j * 180);
        }
        if (s_po_hablo == 2 && (s_ms / 700) % 4 == 1) ico = ICO_POCHI_COME; /* se distrae: ¿quedarán papas? */
        if (t > t2 + 4500u + (uint32_t)strlen(pet_pochi_chat(k, 2, corto)) * 40u) { s_po_fase = PO_INVITA; s_po_t0 = s_ms; s_po_pregunta = 0; }
        break;
    }
    case PO_INVITA: /* ¡a pasear, a pasear! */
        if (s_pc_turno >= 0 || s_pc_fam >= 0) s_po_t0 += dtms; /* (fw 102) mientras un cachorro tiene su momento, espera (así les alcanza a todos) */
        if (s_po_charla_k < 0 && t > 800 && s_pet && s_pc_turno < 0 && s_pc_fam < 0) { /* (v39) si todavía no charlaron en esta visita (fw 102: ni encima de sus cachorros) */
            const int ck = pet_pochi_chat_next(s_pet);
            if (ck >= 0) { s_po_charla_k = ck; s_po_fase = PO_CHARLA; s_po_t0 = s_ms; s_po_hablo = 0; s_po_espejo = s_po_x > s_mascota.x; break; }
            s_po_charla_k = 99; /* (no hay charla: no se vuelve a preguntar) */
        }
        if (!po_tipo_pasea()) { /* (1.0.30) esta vez no viene a pasear: se queda un ratito con ella (y sus cachorros juegan) y se despide */
            po_mover(tx + (float)(int)(sinf((float)s_ms / 700.0f) * 10), ty, 1.6f);
            ico = s_po_corre ? ((s_ms / 300) % 2 ? ICO_POCHI : ICO_POCHI_B) : ((s_ms / 900) % 3 ? ICO_POCHI_SENT : ICO_POCHI);
            if (t > 14000) { s_po_fase = PO_DESPIDE; s_po_t0 = s_ms; s_po_hablo = 0; }
            break;
        }
        if (!s_po_tilo_ladro && s_pet && pet_pochi_knows_name(s_pet) && s_po_charla_k != PET_POCHI_NOMBRE && s_po_charla_k != -1 && (s_escena == ESC_PRADO || s_escena == ESC_LAGO) && t > 1500) {
            s_po_tilo_ladro = true; /* (v39) desde que sabe el nombre: lo ladra mirando hacia la isla */
            s_po_espejo = false;
            po_di("¡PEUMO! ¡GUAU, GUAU! ¡PEUMOOO!", ICO_CORAZON, 2800);
            accion(UI_ACCION_SONAR, SND_POCHI_FELIZ);
            for (int j = 0; j < 4; j++) flotar(ICO_CORAZON, NULL, 0, (int)s_po_x + 20 + j * 10, (int)s_po_y - 72 - j * 6, (uint32_t)j * 200);
        }
        po_mover(tx + (float)(int)(sinf((float)s_ms / 500.0f) * 14), ty, 1.8f);
        ico = (s_ms / 220) % 2 ? ICO_POCHI : ICO_POCHI_B;
        oy = -fabsf(sinf((float)s_ms / 160.0f)) * 5; /* salta de ganas */
        if (t > 3500u + 9000u * (uint32_t)s_po_pregunta) {
            static const char *const P[3] = {"¿PASEAMOS? ¡VAMOS!", "¡ACOMPÁÑAME A PASEAR!", "¡SÍ, SÍ, SÍ! ¡A PASEAR!"};
            if (s_po_pregunta < 6) { po_di(P[s_po_pregunta % 3], ICO_PATITA, 3200); accion(UI_ACCION_SONAR, SND_POCHI_FELIZ); s_po_pregunta++; }
            else { po_di("OTRA VEZ SERÁ... ¡ADIÓS!", ICO_POCHI, 3200); s_po_fase = PO_SE_VA; s_po_t0 = s_ms; }
        }
        break;
    case PO_PASEO: {
        po_mover(tx, ty, 2.6f);
        ico = s_po_corre ? ((s_ms / 130) % 2 ? ICO_POCHI_C : ICO_POCHI_D) : ((s_ms / 300) % 2 ? ICO_POCHI : ICO_POCHI_B);
        if (s_po_pf) { po_portal_paso(&ico, s_po_zonas[s_po_zona_i]); break; }
        if (po_en_transito() && s_po_zona_t0) s_po_zona_t0 += TICK_MS; /* de viaje no se ve: el tiempo de la zona espera */
        const uint32_t z = s_po_zona_t0 ? s_ms - s_po_zona_t0 : 0;
        if (s_po_zona_i < 0 || (s_po_zona_t0 && z > 17000)) { /* a la siguiente zona */
            s_po_zona_i++;
            if (s_po_zona_i >= s_po_nz) { s_po_fase = PO_DESPIDE; s_po_t0 = s_ms; s_po_hablo = 0; break; }
            const int dest = s_po_zonas[s_po_zona_i];
            s_po_zona_t0 = 0; s_po_hablo = 0; s_po_pregunta = (int)s_ms; /* esperando que cambie la zona */
            if (dest != (int)s_pet->escena) {
                if (dest == ESC_ISLA || s_pet->escena == ESC_ISLA) { po_portal_empezar(); break; } /* la isla: portal; el resto, a pie */
                accion(ACT_IR, dest);
            }
        }
        if (!s_po_zona_t0 && s_po_zona_i >= 0) {
            const int dest_ = s_po_zonas[s_po_zona_i];
            const bool regreso = s_po_zona_i >= s_po_nvis; /* el último paso: volver al mundo del que salieron. No se da por hecho hasta que de verdad estén ahí */
            if (s_escena == dest_ || (!regreso && (int32_t)(s_ms - (uint32_t)s_po_pregunta) > 3500 && !po_en_transito())) {
                s_po_zona_t0 = s_ms; s_po_hablo = 0;
                if (!po_visible_cerca()) po_coloca(); /* aparece junto a ella, no por el borde del mapa */
            } else if (regreso && !po_en_transito() && (int32_t)(s_ms - (uint32_t)s_po_pregunta) > 4000) {
                s_po_pregunta = (int)s_ms; /* si no quiso o dormía, se lo vuelve a pedir hasta que llegue (nunca la deja en otra zona) */
                if (dest_ == ESC_ISLA || s_pet->escena == ESC_ISLA) po_portal_empezar(); else accion(ACT_IR, dest_);
            }
        }
        if (s_po_zona_t0 && s_po_zona_i >= s_po_nvis) { /* ya volvieron al mundo de origen: se despide */
            s_po_fase = PO_DESPIDE; s_po_t0 = s_ms; s_po_hablo = 0;
            break;
        }
        if (s_po_zona_t0) {
            const int e = s_po_zonas[s_po_zona_i] % ESC_COUNT;
            const bool frio = s_pet && zona_fria(s_pet);
            if (z > 1800 && s_po_hablo == 0) { s_po_hablo = 1; po_di((frio ? PO_ZONA_FRIO : PO_ZONA)[e][0], ICO_POCHI, 3600); accion(UI_ACCION_POCHI, 2); accion(UI_ACCION_SONAR, SND_POCHI_LADRA); s_po_pedido = 0; }
            if (z > 9000 && s_po_hablo == 1) {
                bool habla = true;
                if (e == ESC_BOSQUE && !frio) { /* "¿oyes a los pájaros?": solo si hay uno; si no, hace que aparezca uno antes */
                    if (!(s_pajaro_t && s_ms - s_pajaro_t < 7000)) {
                        if (!s_po_pedido) { s_po_pedido = s_ms; pajaro_llama(); }
                        habla = s_ms - s_po_pedido > 2200;
                    }
                }
                if (habla) { s_po_hablo = 2; s_po_pedido = 0; po_di((frio ? PO_ZONA_FRIO : PO_ZONA)[e][1], ICO_POCHI, 3600); flotar(ICO_CORAZON, NULL, 0, (int)s_po_x, (int)s_po_y - 70, 0); }
            }
            if (z > 11000 && s_po_hablo == 2 && !s_po_mania_hecha && s_po_mania_try != s_po_zona_i && !s_po_truco && s_pc_turno < 0 && s_pc_fam < 0) { /* (fw 102) su manía de este lugar */
                s_po_mania_try = s_po_zona_i;
                const int m = po_mania_de(e);
                if (m >= 0 && azar() % 100 < 45) { po_mania_empezar(m, PO_PASEO); break; }
            }
        }
        break;
    }
    case PO_DESPIDE: /* se sienta y le cuenta que tiene que volver con sus cachorros */
        po_mover(tx, ty, 2.0f);
        ico = s_po_corre ? ((s_ms / 130) % 2 ? ICO_POCHI_C : ICO_POCHI_D) : ICO_POCHI_SENT;
        if (s_po_hablo == 10) { /* (fw 102) buscando al cachorro que se escondió: la despedida espera */
            s_po_t0 += dtms; ico = ICO_POCHI_SENT; s_po_espejo = (s_ms / 900) % 2;
            if (s_pc_esc < 0 || s_pc[s_pc_esc].modo != PC_ESCONDIDO) { s_po_hablo = 0; s_po_t0 = s_ms - 900; }
            break;
        }
        if (t > 800 && s_po_hablo == 0 && s_pc_n > 0 && s_pc_esc < 0 && s_pc_carga < 0 && (azar() % 100 < 40 || s_pc_esc_forzar)) { /* «¿Y MORITA?»: uno se escondió */
            s_po_hablo = 10; pc_esconderse_empezar(); accion(UI_ACCION_SONAR, SND_POCHI_PIDE);
            break;
        }
        if (t > 800 && s_po_hablo == 0) {
            static int s_po_desp = 0;
            s_po_desp = (s_po_desp + 1 + (int)(s_ms % 3)) % PO_N_DESP;
            s_po_desp_i = s_po_desp;
            s_po_hablo = 1; po_di("TENGO QUE IRME...", ICO_POCHI, 3800); accion(UI_ACCION_SONAR, SND_POCHI_ADIOS);
        }
        if (t > 4600 && s_po_hablo == 1) {
            s_po_hablo = 2;
            if (s_pc_n > 0 && s_pc_esc < 0 && azar() % 100 < 35) { /* uno no se quiere ir: se lo lleva en la boca */
                s_pc_carga = (int)(azar() % (uint32_t)s_pc_n); s_pc[s_pc_carga].modo = PC_CARGADO;
                const int k = s_pc[s_pc_carga].k;
                po_di(po_buf("¡%s, A CASA! ...¿NO QUIERES IRTE? ¡TE LLEVO YO, PUES!", PC_NOMBRE[k]), ICO_CORAZON, 5200);
            } else po_di(s_pc_n >= 2 ? "¡VAMOS, NIÑOS! ¡A CASA, QUE SE HACE TARDE!" : (s_pc_n == 1 ? po_buf("VAMOS, %s... ¡A CASA CON TUS HERMANITOS!", PC_NOMBRE[s_pc[0].k]) : PO_DESP[s_po_desp_i]), ICO_CORAZON, 5200);
        }
        if (t > 10200 && s_po_hablo == 2) { s_po_hablo = 3; po_di(s_po_paseo_ok ? "¡GRACIAS POR PASEAR CONMIGO!" : (s_po_tipo == PT_TAPITA ? "¡GRACIAS POR JUGAR CONMIGO!" : "¡GRACIAS POR ESTE RATITO!"), ICO_POCHI, 3600);
            po_responde("¡ADIÓS, POCHI! ¡VUELVE PRONTO!", s_po_paseo_ok ? "¡QUÉ LINDO PASEO! ¡HASTA LUEGO, POCHI!" : "¡QUÉ LINDA VISITA! ¡HASTA LUEGO, POCHI!", 3800); for (int k = 0; k < 3; k++) flotar(ICO_CORAZON, NULL, 0, (int)s_po_x - 20 + k * 20, (int)s_po_y - 70 - k * 6, (uint32_t)k * 250); accion(UI_ACCION_POCHI, 3); }
        if (t > 14400) { s_po_fase = PO_SE_VA; s_po_t0 = s_ms; s_po_lado = s_po_x < CX ? -1 : 1; }
        break;
    case PO_SE_VA: { /* corre hacia su casa */
        const float meta = s_po_x < CX ? -50.0f : (float)(W + 50);
        po_mover(meta, s_po_y, 4.4f);
        ico = (s_ms / 110) % 2 ? ICO_POCHI_C : ICO_POCHI_D;
        if (fabsf(s_po_x - meta) < 6) {
            if (!s_po_dormida_visita) /* (fw 102) se acuerda de cómo terminó: la próxima vez saluda distinto */
                accion(UI_ACCION_POCHI, PET_POCHI_F_FIN + (s_po_paseo_ok ? PET_POCHI_ULT_PASEO : (s_po_hoyno ? PET_POCHI_ULT_HOYNO : (s_po_comio && po_tipo_pasea() ? PET_POCHI_ULT_COMIO : PET_POCHI_ULT_NADA))));
                /* (1.0.30: solo queda sentida por no acompañarla si esta vez venía a pasear) */
            s_po_dormida_visita = false; s_po_truco = TR_NADA;
            po_cosa(-1, 0, 0, 0); if (s_po_luz) ocultar(s_po_luz, true); pc_esconder();
            s_po_fase = PO_NADA; s_po_barro_pasos = 0; ocultar(s_po_img, true); ocultar(s_po_plato, true);
            s_po_proxima = s_ultimo_t + (s_po_hoyno ? 4u * 3600u : PO_ESPERA_S); s_po_hoyno = false;
            return;
        }
        break;
    }
    case PO_ACURRUCA: { /* (fw 102) ella duerme: llega en puntillas y se acurruca a su lado */
        const float lado = s_po_x < s_mascota.x ? -1.0f : 1.0f;
        if (s_pet && !s_pet->dormida) { /* se despertó: ¡buenos días! y la visita sigue como siempre */
            s_po_dormida_visita = false;
            accion(UI_ACCION_POCHI, 0);
            po_di("¡BUENOS DÍAS, DORMILONA! TE CUIDÉ TODO EL RATO", ICO_CORAZON, 3200);
            po_saludo(); po_hambre();
            break;
        }
        po_mover(s_mascota.x + lado * 46.0f, s_mascota.y + 10, 0.9f);
        ico = s_po_corre ? ((s_ms / 260) % 2 ? ICO_POCHI_C : ICO_POCHI_D) : ICO_POCHI_SENT;
        if (!s_po_corre && s_po_pregunta == 0) {
            s_po_pregunta = 1; s_po_espejo = lado > 0;
            accion(UI_ACCION_POCHI, PET_POCHI_F_ACURRUCA);
            po_di("SHHH... ME ACURRUCO AQUÍ UN RATITO", ICO_ZETA, 2600);
        }
        if (s_po_pregunta == 1) { oy = 2.0f + sinf((float)s_ms / 700.0f); if (s_ms % 2600 < TICK_MS) flotar(ICO_ZETA, NULL, 0, (int)s_po_x, (int)s_po_y - 60, 0); }
        if (t > 60000) { s_po_fase = PO_SE_VA; s_po_t0 = s_ms; s_po_lado = s_po_x < CX ? -1 : 1; }
        break;
    }
    case PO_CONSUELA: { /* (fw 102) triste o enferma: le lame la cara o le trae una hojita de matico */
        const bool enf = s_pet && (s_pet->enferma || pet_mood(s_pet) == MOOD_ENFERMA);
        po_mover(s_mascota.x + (s_po_x < s_mascota.x ? -40.0f : 40.0f), s_mascota.y + 8, 2.2f);
        ico = s_po_corre ? ((s_ms / 110) % 2 ? ICO_POCHI_C : ICO_POCHI_D) : (t > 2400 && t < 6000 ? ICO_POCHI_COME : ICO_POCHI_SENT);
        if (!s_po_corre) s_po_espejo = s_po_x > s_mascota.x;
        if (s_po_pregunta == 0 && t > 400) {
            s_po_pregunta = 1;
            po_di(enf ? "¡AY! ¿ESTÁS ENFERMITA? TE TRAJE UNA HOJITA DE MATICO PARA QUE TE MEJORES" : "¿QUÉ TE PASA? VEN, TE DOY UN LENGÜETAZO", ICO_CORAZON, 3400);
            accion(UI_ACCION_SONAR, SND_POCHI_PIDE);
        }
        if (s_po_pregunta == 1 && t > 2600) {
            s_po_pregunta = 2;
            accion(UI_ACCION_POCHI, PET_POCHI_F_CONSUELA);
            if (enf) flotar(ICO_HOJA, NULL, 0, (int)s_mascota.x, (int)s_mascota.y - 80, 0);
            po_responde("GRACIAS, POCHI... YA ME SIENTO UN POQUITO MEJOR", "¡AY, POCHI! ¡QUÉ CARIÑOSA ERES!", 2400);
        }
        if (t > 2600 && t < 6000 && s_ms % 700 < TICK_MS) flotar(ICO_CORAZON, NULL, 0, (int)s_mascota.x, (int)s_mascota.y - 70, 0);
        if (t > 8500) po_tras_llegar();
        break;
    }
    case PO_MOJADA: /* (fw 102) con lluvia: llega mojada y se sacude encima */
        ico = ICO_POCHI_SENT;
        if (s_po_pregunta == 0) { s_po_pregunta = 1; po_di("¡LLEGUÉ MOJADITA! ¡LLUEVE MUCHO!", ICO_LLUVIA, 2600); }
        if (t > 1800 && t < 4200) {
            ico = ICO_POCHI; rot = (s_ms / 70) % 2 ? 120 : -120; oy = (s_ms / 70) % 2 ? -2.0f : 1.0f;
            if (s_ms % 90 < TICK_MS) chispa((int)s_po_x + (int)(azar() % 60) - 30, (int)s_po_y - 40, 0x9AD4FF, (float)((int)(azar() % 9) - 4) * 0.6f, -1.2f);
            if (s_po_pregunta == 1 && t > 2600) {
                s_po_pregunta = 2;
                po_di("¡UPS! ¡TE MOJÉ!", ICO_GOTA, 2200);
                po_responde("¡AAAY, POCHI!", "¡JAJAJA! ¡ME SALPICASTE!", 2400);
                accion(UI_ACCION_POCHI, PET_POCHI_F_MOJADA);
                for (int k = 0; k < 3; k++) flotar(ICO_GOTA, NULL, 0, (int)s_mascota.x - 20 + k * 20, (int)s_mascota.y - 60, (uint32_t)k * 200);
            }
        }
        if (t > 5600) po_tras_llegar();
        break;
    case PO_OFENDIDA: /* (fw 102) la otra vez no la acompañaste (o no le diste papas): le da la espalda un rato; un cariño se lo quita */
        ico = ICO_POCHI_SENT;
        if (s_po_pregunta < 2) s_po_espejo = s_po_x < s_mascota.x;
        if (s_po_pregunta == 0 && t > 400) {
            s_po_pregunta = 1;
            po_di(s_po_ult == PET_POCHI_ULT_HOYNO ? "HMPF... LA OTRA VEZ NO ME DISTE PAPAS" : "HMPF... LA OTRA VEZ NO ME ACOMPAÑASTE A PASEAR", ICO_POCHI, 3000);
            po_responde("¡PERDÓN, POCHI!", "¡NO TE ENOJES, POCHI!", 3200);
        }
        if (s_po_pregunta < 2 && t > 6000) {
            s_po_pregunta = 2; s_po_ult = PET_POCHI_ULT_NADA;
            s_po_espejo = s_po_x > s_mascota.x;
            po_di("...¡BUENO, TE PERDONO! ¿HAY PAPAS?", ICO_CORAZON, 3000);
            accion(UI_ACCION_SONAR, SND_POCHI_FELIZ);
            flotar(ICO_CORAZON, NULL, 0, (int)s_po_x, (int)s_po_y - 70, 0);
            po_saludo_cachorro();
        }
        if (s_po_pregunta == 2) ico = (s_ms / 300) % 2 ? ICO_POCHI : ICO_POCHI_B;
        if (t > 8800) po_hambre();
        break;
    case PO_MANIA: po_mania_paso(t, &ico, &oy); break;
    case PO_TAPITA: po_tapita_paso(tx, ty, &ico, &oy); break;
    case PO_ANIMAL: pa_paso(t, &ico, &oy, &rot); break;
    default: break;
    }
    if (s_po_fase != PO_BARRO) po_pata_dejar(s_po_x, s_po_y + 2, s_po_espejo);
    ocultar(s_po_img, s_interior || (po_en_transito() && !s_po_pf));
    po_seguir_globo();
    const int esc = 6;
    { /* (solo si cambió: poner la imagen o moverla repinta; antes se hacía en cada cuadro) */
        static art_icono_t ult_ico = (art_icono_t)-1;
        static bool ult_esp;
        if (ico != ult_ico || s_po_espejo != ult_esp) { ult_ico = ico; ult_esp = s_po_espejo; lv_image_set_src(s_po_img, s_po_espejo ? art_icono_espejo(ico, esc) : art_icono(ico, esc)); }
        { static int ult_rot; if (rot != ult_rot) { ult_rot = rot; lv_image_set_rotation(s_po_img, rot); } } /* (fw 102) */
        const int px = (int)s_po_x - 36, py = (int)(s_po_y - 68 + oy);
        if (px != lv_obj_get_x(s_po_img) || py != lv_obj_get_y(s_po_img)) lv_obj_set_pos(s_po_img, px, py);
        if (s_po_fade && !s_po_pf) { /* aparece poco a poco */
            const uint32_t d = s_ms - s_po_fade;
            if (d >= 700) { s_po_fade = 0; lv_obj_set_style_opa(s_po_img, 255, 0); }
            else lv_obj_set_style_opa(s_po_img, (lv_opa_t)(d * 255 / 700 / 32 * 32), 0);
        }
        lv_obj_t *pa = lv_obj_get_parent(s_po_img);
        if (lv_obj_get_index(s_po_img) != (int32_t)lv_obj_get_child_count(pa) - 1) lv_obj_move_foreground(s_po_img);
    }
    if (s_po_fase != PO_NADA) pc_animar(); /* (fw 102) sus cachorros */
    /* (fw 102) lo que lleva en la boca o persigue va encima de ella */
    if (s_po_cosa && !lv_obj_has_flag(s_po_cosa, LV_OBJ_FLAG_HIDDEN)) lv_obj_move_foreground(s_po_cosa);
    if (s_po_luz && !lv_obj_has_flag(s_po_luz, LV_OBJ_FLAG_HIDDEN)) lv_obj_move_foreground(s_po_luz);
    for (int i = 0; i < s_pc_n; i++) if (s_pc[i].cosa && !lv_obj_has_flag(s_pc[i].cosa, LV_OBJ_FLAG_HIDDEN)) lv_obj_move_foreground(s_pc[i].cosa);
}
/* para probar: f=0 llega, 1 ya comió y invita, 2 empieza el paseo, 3 se despide */
void ui_demo_hongos(void) { empezar_juego(JUEGO_HONGOS); }
void ui_demo_pochi(int f)
{
    /* (fw 102) 10+k: viene con el cachorro k (ya comió: le toca su momento); 20 se rasca, 21 panza arriba, 22 mareada;
     * 30+m: la manía m (0 ramita, 1 luciérnaga, 2 cóndor, 3 agua); 40 consuela, 41 mojada, 42 ofendida, 43 acurrucada (ella dormida) */
    if (f == 43) { if (s_po_fase != PO_NADA) { s_po_fase = PO_NADA; ocultar(s_po_img, true); } po_acurruca_empezar(); return; }
    if (f >= 120 && f < 120 + PT_N) { /* (1.0.30) 120+tipo: una visita nueva de ese tipo (0 la de siempre, 1 papas, 2 paseo, 3 tapita, 4 saluda) */
        if (s_po_fase != PO_NADA) { s_po_fase = PO_NADA; ocultar(s_po_img, true); po_cosa(-1, 0, 0, 0); }
        s_po_tipo_forzar = f - 120; s_po_dormida_visita = false; po_empezar(); s_po_ult = PET_POCHI_ULT_NADA;
        return;
    }
    if (f == 125) { if (s_po_fase == PO_TAPITA && s_po_tp == 1) po_tapita_tirar(s_mascota.x - 140.0f, s_mascota.y); return; } /* (prueba: tirarla) */
    if (s_po_fase == PO_NADA) { s_po_dormida_visita = false; s_po_tipo_forzar = PT_COMPLETA; po_empezar(); }
    if ((f >= 10 && f <= 17) || (f >= 50 && f <= 54)) { /* 10-13 con ese cachorro; 14 los cuatro; 15 dos; 16 dos y su escena de familia ya;
                                                          * 17 uno y la despedida («¿y ...?»); 50-54 la escena de familia 0-4 (con Morita y Chapi) */
        int ks[PC_MAX], n = 1;
        if (f >= 50) { n = 2; ks[0] = POCHI_MORITA; ks[1] = POCHI_CHAPI; s_pc_fam_forzar = f - 50; f = 16; }
        else if (f <= 13) ks[0] = f - 10;
        else { const int a = (int)(azar() % 4); n = f == 14 ? 4 : (f == 17 ? 1 : 2); for (int i = 0; i < n; i++) ks[i] = (a + i * (f == 14 ? 1 : 2)) % 4; }
        pc_empezar();
        s_pc_n = n;
        s_po_comio = true; s_po_fase = PO_INVITA; s_po_t0 = s_ms; s_po_x = s_mascota.x + 64; s_po_pregunta = 0; ocultar(s_po_plato, true);
        for (int i = 0; i < n; i++) {
            cach_t *c = &s_pc[i]; c->k = ks[i];
            if (!c->img) {
                c->img = lv_image_create(s_mundo); lv_obj_add_flag(c->img, LV_OBJ_FLAG_CLICKABLE); lv_obj_set_ext_click_area(c->img, 10);
                lv_obj_add_event_cb(c->img, pc_toque_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i); lv_image_set_pivot(c->img, 6 * PC_ESC, 6 * PC_ESC);
            }
            c->ult_ico = -1; pc_puesto(i, &c->x, &c->y); c->modo = PC_SIGUE; c->hecho = f == 16 || f == 17; c->chequeo = s_ms;
            c->lengua_hasta = 0; c->lengua_prox = s_ms + 3000u + azar() % 8000u;
        }
        if (f < 16) po_saludo_cachorro();
        if (f == 16) { s_pc_libre = s_ms - 5000; pc_familia_empezar(); }
        if (f == 17) { s_po_fase = PO_DESPIDE; s_po_t0 = s_ms - 700; s_po_hablo = 0; s_pc_carga = -2; s_pc_esc_forzar = true; } /* (-2: que no lo cargue; que se esconda) */
        return;
    }
    if (f >= 60 && f <= 103) { /* 60+tipo: Pochi sola con un animal de ese tipo; 80+tipo: con un cachorro al azar; 100+k: el cachorro k y su animal amigo */
        const int con = f >= 100 ? f - 100 : (f >= 80 ? (int)(azar() % 4) : -1), tipo = f >= 100 ? AT_AMIGO : (f >= 80 ? f - 80 : f - 60);
        if (tipo < 0 || tipo >= AT_N || con >= PET_POCHI_CACHORROS || (tipo == AT_AMIGO && con < 0)) return;
        pc_empezar(); s_pc_n = 0;
        s_po_comio = true; s_po_fase = PO_INVITA; s_po_t0 = s_ms; s_po_x = s_mascota.x + 64; s_po_y = s_mascota.y + 8; s_po_pregunta = 0; ocultar(s_po_plato, true);
        if (con >= 0) {
            cach_t *c = &s_pc[0]; c->k = con; s_pc_n = 1;
            if (!c->img) {
                c->img = lv_image_create(s_mundo); lv_obj_add_flag(c->img, LV_OBJ_FLAG_CLICKABLE); lv_obj_set_ext_click_area(c->img, 10);
                lv_obj_add_event_cb(c->img, pc_toque_cb, LV_EVENT_CLICKED, (void *)(intptr_t)0); lv_image_set_pivot(c->img, 6 * PC_ESC, 6 * PC_ESC);
            }
            c->ult_ico = -1; pc_puesto(0, &c->x, &c->y); c->modo = PC_SIGUE; c->hecho = true; c->chequeo = s_ms; c->lengua_hasta = 0; c->lengua_prox = s_ms + 5000u;
        }
        s_po_charla_k = 99; s_po_rasco = s_po_panzo = true; /* (que nada más se meta en la prueba) */
        s_pa_hecha = false; s_pa_forzar = tipo;
        return;
    }
    if (f >= 20 && f <= 22) {
        s_po_fase = PO_INVITA; s_po_t0 = s_ms; s_po_x = s_mascota.x + 64; s_po_y = s_mascota.y + 8; s_po_pregunta = 0; ocultar(s_po_plato, true);
        if (f == 20) po_rasca_empezar(); else if (f == 21) po_panza_empezar(false); else { po_truco(TR_MAREO, 3600); po_di("¡AY, CUÁNTO CARIÑO... ME MAREÉ!", ICO_MAREO, 2600); }
        return;
    }
    if (f >= 30 && f <= 33) { s_po_x = s_mascota.x + 64; s_po_y = s_mascota.y + 8; ocultar(s_po_plato, true); po_mania_empezar(f - 30, PO_INVITA); return; }
    if (f >= 40 && f <= 42) {
        s_po_x = s_mascota.x + 90; s_po_y = s_mascota.y + 8; s_po_t0 = s_ms; s_po_pregunta = 0; ocultar(s_po_plato, true);
        s_po_fase = f == 40 ? PO_CONSUELA : (f == 41 ? PO_MOJADA : PO_OFENDIDA);
        if (f == 42) s_po_ult = PET_POCHI_ULT_COMIO;
        return;
    }
    if (f == 5) { s_po_x = s_mascota.x + 64; po_barro_empezar(); ocultar(s_po_plato, true); return; }
    if (f == 4) { s_po_fase = PO_CORRE; s_po_t0 = s_ms; s_po_dir = 1; ocultar(s_po_plato, true); return; }
    if (f >= 1 && f != 4) { s_po_fase = PO_INVITA; s_po_t0 = s_ms; s_po_x = s_mascota.x + 64; s_po_pregunta = 0; ocultar(s_po_plato, true); }
    if (f == 2) po_a_paseo();
    if (f >= 3) { s_po_fase = PO_DESPIDE; s_po_t0 = s_ms; s_po_hablo = 0; s_po_x = s_mascota.x + 64; }
}

/* (fw 108) Pochi se queda en SU mundo: al cambiar de perfil se va sin despedirse (cada mascota tiene sus visitas
 * y sus historias; antes la visita seguía en pantalla con la mascota nueva). */
static void po_fuera(void)
{
    s_po_fase = PO_NADA;
    s_po_quiere = false;
    s_po_barro_pasos = 0;
    s_po_dormida_visita = false;
    s_po_truco = TR_NADA;
    s_po_pend_n = 0;
    s_po_resp_t = NULL;
    s_po_charla_k = -1;
    s_po_esc_ref = -1;
    po_cosa(-1, 0, 0, 0);
    pc_esconder();
    po_op_ocultar();
    if (s_po_luz) ocultar(s_po_luz, true);
    ocultar(s_po_img, true);
    ocultar(s_po_plato, true);
    if (s_po_charco) ocultar(s_po_charco, true);
    s_po_proxima = s_ultimo_t + PO_ESPERA_S;   /* en el mundo nuevo, su visita empieza de cero */
    s_po_chequeo = s_ms;
}
