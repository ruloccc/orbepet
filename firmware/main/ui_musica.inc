/* ======================================================================
 * (v38) La música de los cristales, en la cueva:
 *  - El MODO LIBRE (Jugar → Cristales → Tocar libre): la misma pantalla de los 6 cristales, sin rondas ni errores; suenan al
 *    instante (hasta 6 a la vez). Al primer toque ella baja, se sienta en una roca abajo a la izquierda y se queda quieta
 *    hasta que termines: con lo lento cierra los ojos y se mece; con lo rápido mueve la cola y salta en su lugar; si tocas
 *    siempre el mismo cristal ladea la cabeza. No habla mientras tocas (solo flotan notitas).
 *  - Cuando paras (~3 s sin tocar, después de 6+ notas) terminó una pieza y reaccionan: el dragón (si ya despertó) abre las
 *    alas con lo rápido, ronronea y bosteza con lo lento, se llena de chispas con lo largo (20+ notas) y tararea como eco una
 *    frase repetida; sus alas brillan con el color más tocado. De noche, con música lenta, se duerme y ella cabecea. Sin
 *    dragón, algo responde lejos (un eco). A veces ella se suma: repite tus últimas notas con su «la» o te contesta.
 *    Si vuelves a tocar mientras reaccionan, se callan: la música manda.
 *  - "NUESTRA CANCIÓN": después de tocar varias veces, ella (o el dragón) propone hacer una canción: tú tocas una frase
 *    de 4 a 8 notas, ella propone cómo sigue (aceptar, pedir otra o tocarla tú), se escucha entera (cristales y ella
 *    cantando) y se le pone nombre. Queda en el diario; sirve de alarma y de canción de cuna; ella la tararea contenta.
 *  - El reproductor de canciones (también fuera de la cueva: alarmas, cuna, tarareo).
 * ==================================================================== */
#define LIB_MAX 64
enum { LB_ESPERA = 0, LB_CAMINA, LB_SENTADA };
enum { CP_NADA = 0, CP_PREGUNTA, CP_FRASE, CP_ELLA, CP_TUYA, CP_ESCUCHA, CP_NOMBRE, CP_LISTO };
/* (s_sim_libre, declarada en ui_magia.inc: la pantalla de los cristales está en modo libre) */
static uint8_t s_lib_cris[LIB_MAX];             /* la pieza que se está tocando */
static uint16_t s_lib_ms[LIB_MAX];              /* ms desde el toque anterior */
static int s_lib_n, s_lib_fase, s_lib_reac, s_lib_reac_ev = -1, s_lib_sin_msg, s_lib_drag_f = -1, s_lib_pose = -1;
static uint32_t s_lib_ult, s_lib_reac_t0, s_lib_camina_t0, s_lib_drag_luz_t;
static uint8_t s_lib_ult16[16]; static uint16_t s_lib_ult16_ms[16]; static int s_lib_ult16_n; /* las últimas 16 notas con su ritmo */
static lv_obj_t *s_lib_pet, *s_lib_roca, *s_lib_drag, *s_lib_nota[6];
static art_img_t s_lib_pet_art[6], s_lib_drag_art[3];
static uint32_t s_lib_nota_t[6]; static int s_lib_nota_k[6];
static int s_lib_mas_tocado = -1;
static bool s_lib_dragon;                       /* el dragón está (despierto y en casa) */
static int s_canc_pend;                         /* el cuaderno se redibuja cuando el motor anotó el cambio */
/* componer */
static int s_cp_fase, s_cp_variante, s_cp_frase_n, s_cp_cont_n, s_cp_toq_n, s_cp_propone; /* propone: 1 ella, 2 el dragón */
static uint8_t s_cp_frase[8], s_cp_cont[8], s_cp_toq_cris[8];
static uint16_t s_cp_toq_ms[8];
static uint32_t s_cp_toq_ult, s_cp_t0;
static lv_obj_t *s_cp_b[3], *s_cp_bi[3], *s_cp_punto[PET_CANCION_NOTAS], *s_cp_tec, *s_cp_tec_txt;
static art_img_t s_cp_tec_art;
static char s_cp_nombre[PET_CANCION_NOMBRE];
static uint8_t s_cp_guardar[PET_CANCION_NOTAS]; static int s_cp_guardar_n; /* lo que va al motor */
const uint8_t *ui_pending_song(int *n, const char **nombre) { if (n) *n = s_cp_guardar_n; if (nombre) *nombre = s_cp_nombre; return s_cp_guardar; }

/* ---------- el reproductor ---------- */
enum { REP_CRISTAL = 1, REP_VOZ = 2, REP_SUAVE = 4, REP_FLOTAR = 8 };
static uint8_t s_rep_nota[PET_CANCION_NOTAS];
static int s_rep_n, s_rep_i, s_rep_modo;
static uint32_t s_rep_prox;
static void rep_tocar(const uint8_t *notas, int n, int modo)
{
    if (n > PET_CANCION_NOTAS) n = PET_CANCION_NOTAS;
    if (n <= 0) { s_rep_n = 0; return; }
    memcpy(s_rep_nota, notas, (size_t)n);
    s_rep_n = n; s_rep_i = 0; s_rep_modo = modo; s_rep_prox = s_ms + 120;
}
static bool rep_activo(void) { return s_rep_n > 0 && s_rep_i < s_rep_n; }
static void rep_parar(void) { s_rep_n = 0; }
static int rep_nota_actual(void) { return rep_activo() || (s_rep_n && s_ms - s_rep_prox < 300) ? s_rep_i - 1 : -1; }
static void lib_nota_flota(int k);
static void animar_reproductor(void)
{
    if (!rep_activo() || (int32_t)(s_ms - s_rep_prox) < 0) return;
    const uint8_t b = s_rep_nota[s_rep_i++];
    const int k = PET_CANCION_CRISTAL(b) % CRIS_TOT;
    const int suave = (s_rep_modo & REP_SUAVE) ? 0x100 : 0;
    /* (v39) su «la» sonaba zumbón, como un sonido roto: ahora ella «canta» con notas de cristal suaves (y las notitas que flotan) */
    if (s_rep_modo & REP_CRISTAL) accion(UI_ACCION_SONAR, snd_cristal(k) | suave);
    else if (s_rep_modo & REP_VOZ) accion(UI_ACCION_SONAR, snd_cristal(k) | 0x100);
    if (s_sim_on) { s_sim_luz_k[k] = s_ms; if (s_rep_modo & REP_VOZ) lib_nota_flota(k); }
    else if ((s_rep_modo & REP_FLOTAR) && s_mascota.visible) flotar(ICO_NOTA, NULL, 0, (int)s_mascota.x - 16 + (int)(azar() % 32), cabeza_y(&s_mascota) - 6, 0);
    s_rep_prox += (uint32_t)PET_CANCION_PASOS(b) * PET_PASO_MS;
}
static void cancion_tocar(int k, int modo)
{
    const cancion_t *c = s_pet ? pet_song(s_pet, k) : NULL;
    if (c) rep_tocar(c->nota, c->n, modo);
}

/* ---------- ella en la pantalla de los cristales ---------- */
#define LIB_ROCA_X 52
#define LIB_ROCA_Y 334
#define LIB_INI_X 112
#define LIB_INI_Y 206
static void lib_pose(int f)
{
    if (f == s_lib_pose || !s_pet) return;
    s_lib_pose = f;
    if (!s_lib_pet_art[f].dsc.data) {
        const pet_t *m = s_pet;
        /* 0 de pie, 1 caminando, 2 sentada, 3 ojos cerrados (se mece), 4 feliz con la boca abierta (salta/aplaude), 5 cabecea */
        static const art_expresion_t EX[6] = {EXP_CURIOSO, EXP_CURIOSO, EXP_FELIZ, EXP_CERRADOS, EXP_FELIZ, EXP_CERRADOS};
        art_pose_t po = {pet_look(m), m->etapa, m->etapa == STAGE_ADULTA ? m->forma : 0, 4, EX[f], f == 4, f == 1, false, false,
                         f == 5 ? -1 : 1, f == 4 ? 1 : 0, f >= 2 ? -1 : 0, {0, 0, 0, 0}, 0, m->guardiana};
        for (int r = 0; r < RANURA_COUNT; r++) po.ropa[r] = pet_worn_raw(m, r);
        art_criatura_pose(&s_lib_pet_art[f], &po);
    }
    poner_src(s_lib_pet, &s_lib_pet_art[f].dsc);
}
static void lib_nota_flota(int k)
{
    int libre = 0;
    for (int i = 0; i < 6; i++) if (s_ms - s_lib_nota_t[i] > 900) { libre = i; break; }
    s_lib_nota_t[libre] = s_ms;
    s_lib_nota_k[libre] = k;
    lv_obj_set_style_image_recolor(s_lib_nota[libre], lv_color_hex(SIM_COLOR[k % CRIS_TOT]), 0);
    ocultar(s_lib_nota[libre], false);
}
static void lib_crear(void)
{
    if (s_lib_pet) return;
    s_lib_roca = caja(s_sim); /* la roca donde se sienta */
    lv_obj_set_size(s_lib_roca, 74, 26);
    lv_obj_set_style_radius(s_lib_roca, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(s_lib_roca, lv_color_hex(0x8A84A8), 0);
    lv_obj_set_style_bg_grad_color(s_lib_roca, lv_color_hex(0x34304E), 0);
    lv_obj_set_style_border_width(s_lib_roca, 2, 0);
    lv_obj_set_style_border_color(s_lib_roca, lv_color_hex(0x2A2640), 0);
    lv_obj_set_style_radius(s_lib_roca, 13, 0);
    lv_obj_set_style_bg_grad_dir(s_lib_roca, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_bg_opa(s_lib_roca, LV_OPA_COVER, 0);
    lv_obj_remove_flag(s_lib_roca, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_pos(s_lib_roca, LIB_ROCA_X - 37, LIB_ROCA_Y - 14);
    s_lib_drag = lv_image_create(s_sim); /* el dragón, chiquito y detrás de los cristales: no recibe toques */
    lv_obj_remove_flag(s_lib_drag, LV_OBJ_FLAG_CLICKABLE);
    s_lib_pet = lv_image_create(s_sim);
    lv_obj_remove_flag(s_lib_pet, LV_OBJ_FLAG_CLICKABLE);
    for (int i = 0; i < 6; i++) {
        s_lib_nota[i] = lv_image_create(s_sim);
        lv_obj_remove_flag(s_lib_nota[i], LV_OBJ_FLAG_CLICKABLE);
        lv_image_set_src(s_lib_nota[i], art_icono(ICO_NOTA, 2));
        lv_obj_set_style_image_recolor_opa(s_lib_nota[i], LV_OPA_60, 0);
        ocultar(s_lib_nota[i], true);
    }
    for (int i = 0; i < PET_CANCION_NOTAS; i++) { /* la partitura de colores (al componer) */
        s_cp_punto[i] = caja(s_sim);
        lv_obj_set_size(s_cp_punto[i], 14, 14);
        lv_obj_set_style_radius(s_cp_punto[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(s_cp_punto[i], LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(s_cp_punto[i], 1, 0);
        lv_obj_set_style_border_color(s_cp_punto[i], lv_color_hex(0xFFFFFF), 0);
        lv_obj_remove_flag(s_cp_punto[i], LV_OBJ_FLAG_CLICKABLE);
        ocultar(s_cp_punto[i], true);
    }
}
static void cp_boton_cb(lv_event_t *e);
static void cp_empezar_frase(void);
static void lib_botones_crear(void)
{
    if (s_cp_b[0]) return;
    for (int i = 0; i < 3; i++) {
        s_cp_b[i] = boton(s_sim, 64, 64, cp_boton_cb, i);
        lv_obj_set_style_radius(s_cp_b[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_ext_click_area(s_cp_b[i], 6);
        s_cp_bi[i] = icono(s_cp_b[i], ICO_SI, 3);
        lv_obj_center(s_cp_bi[i]);
        ocultar(s_cp_b[i], true);
    }
}
/* los botones de la pantalla al componer: hasta tres, con su dibujo */
static void cp_botones(int n, const art_icono_t *ic, const uint32_t *col)
{
    lib_botones_crear();
    for (int i = 0; i < 3; i++) {
        ocultar(s_cp_b[i], i >= n);
        if (i >= n) continue;
        lv_image_set_src(s_cp_bi[i], art_icono(ic[i], 3));
        lv_obj_set_style_bg_color(s_cp_b[i], lv_color_hex(col ? col[i] : C_BOTON), 0);
        lv_obj_set_pos(s_cp_b[i], CX - 32 + (int)((i - (n - 1) / 2.0f) * 80), 150);
        lv_obj_move_foreground(s_cp_b[i]);
    }
}
static void cp_partitura(const uint8_t *notas, int n, int resalta)
{
    const int x0 = CX - (n * 18) / 2;
    for (int i = 0; i < PET_CANCION_NOTAS; i++) {
        ocultar(s_cp_punto[i], i >= n);
        if (i >= n) continue;
        const int k = PET_CANCION_CRISTAL(notas[i]) % CRIS_TOT, pasos = PET_CANCION_PASOS(notas[i]);
        lv_obj_set_style_bg_color(s_cp_punto[i], lv_color_hex(SIM_COLOR[k]), 0);
        const int lado = 10 + (pasos > 4 ? 4 : pasos); /* las notas largas, un poquito más grandes */
        lv_obj_set_size(s_cp_punto[i], lado, lado);
        lv_obj_set_pos(s_cp_punto[i], x0 + i * 18 + (14 - lado) / 2, 228 - (i == resalta ? 6 : 0) + (14 - lado) / 2);
        lv_obj_set_style_border_width(s_cp_punto[i], i == resalta ? 3 : 1, 0);
    }
}
static void lib_texto(const char *t) /* (en el modo libre el texto va debajo del dragón) */
{
    if (!t || s_nino) { ocultar(s_sim_t, true); return; }
    art_texto(&s_sim_t_art, t, C_BLANCO, 2, 300);
    poner_src(s_sim_t, &s_sim_t_art.dsc);
    lv_obj_align(s_sim_t, LV_ALIGN_TOP_MID, 0, 112);
    ocultar(s_sim_t, false);
    lv_obj_move_foreground(s_sim_t);
}

/* ---------- empezar y cerrar ---------- */
static void cristales_libre(bool componer)
{
    cristales_empezar(); /* la misma pantalla (cueva, cristales, la X) */
    s_sim_libre = true;
    s_sim_fase = SIM_TURNO; /* (sin rondas: que no se muestre la secuencia del juego de memoria) */
    s_sim_paso = 0;
    lib_crear();
    s_lib_n = 0; s_lib_fase = LB_ESPERA; s_lib_reac = 0; s_lib_reac_ev = -1; s_lib_sin_msg = 0; s_lib_pose = -1; s_lib_drag_f = -1;
    s_lib_ult16_n = 0; s_lib_mas_tocado = -1;
    s_cp_fase = CP_NADA; s_cp_variante = 0;
    rep_parar();
    s_lib_dragon = s_pet && pet_special_known(s_pet, ESP_CRISTALES) && !dragon_ausente();
    ocultar(s_lib_pet, false); ocultar(s_lib_roca, false);
    lib_pose(0);
    lv_obj_set_pos(s_lib_pet, LIB_INI_X - 30, LIB_INI_Y - 60);
    lv_image_set_rotation(s_lib_pet, 0);
    ocultar(s_lib_drag, !s_lib_dragon);
    for (int i = 0; i < PET_CANCION_NOTAS; i++) ocultar(s_cp_punto[i], true);
    if (s_cp_b[0]) for (int i = 0; i < 3; i++) ocultar(s_cp_b[i], true);
    if (s_cp_tec) ocultar(s_cp_tec, true);
    lv_obj_move_background(s_lib_drag); lv_obj_move_background(s_sim_luz); lv_obj_move_background(s_sim_fondo); /* el dragón, detrás de los cristales */
    lv_obj_move_foreground(s_lib_roca); lv_obj_move_foreground(s_lib_pet);
    for (int i = 0; i < 6; i++) lv_obj_move_foreground(s_lib_nota[i]);
    lv_obj_move_foreground(s_sim_x);
    lib_texto(componer ? NULL : "TOCA LIBRE: COMO QUIERAS, SIN ERRORES");
    s_lib_reac_t0 = s_ms;
    if (componer) { s_cp_propone = 1; s_lib_fase = LB_CAMINA; s_lib_camina_t0 = s_ms; cp_empezar_frase(); } /* (desde el menú: directo a tocar la frase) */
}
static void lib_cerrar(void) /* (lo llama sim_fin) */
{
    if (!s_sim_libre) return;
    s_sim_libre = false;
    rep_parar();
    if (s_lib_pet) { lv_image_set_src(s_lib_pet, NULL); lv_image_set_src(s_lib_drag, NULL); ocultar(s_lib_pet, true); ocultar(s_lib_roca, true); ocultar(s_lib_drag, true); }
    for (int i = 0; i < 6; i++) art_img_free(&s_lib_pet_art[i]);
    for (int i = 0; i < 3; i++) art_img_free(&s_lib_drag_art[i]);
    for (int i = 0; i < 6; i++) if (s_lib_nota[i]) ocultar(s_lib_nota[i], true);
    for (int i = 0; i < PET_CANCION_NOTAS; i++) if (s_cp_punto[i]) ocultar(s_cp_punto[i], true);
    if (s_cp_b[0]) for (int i = 0; i < 3; i++) ocultar(s_cp_b[i], true);
    if (s_cp_tec) ocultar(s_cp_tec, true);
    s_cp_fase = CP_NADA;
    s_lib_pose = -1; s_lib_drag_f = -1;
}

/* ---------- tocar ---------- */
static void lib_reaccion_cortar(void)
{
    if (!s_lib_reac) return;
    s_lib_reac = 0;
    rep_parar();
    if (s_cp_fase == CP_NADA) ocultar(s_sim_t, true);
}
static void cp_toque(int k);
static void lib_toque(int k) /* tocaste un cristal en el modo libre */
{
    sim_encender(k);
    lib_nota_flota(k);
    s_sim_inicio = s_ms; /* (en el modo libre se sale solo recién después de 4 minutos sin tocar) */
    if (s_cp_fase == CP_FRASE || s_cp_fase == CP_TUYA) { cp_toque(k); return; }
    if (s_cp_fase != CP_NADA) return; /* (preguntando, escuchando o poniendo nombre: los cristales suenan, nada más) */
    lib_reaccion_cortar(); /* la música manda */
    if (s_lib_fase == LB_ESPERA) { s_lib_fase = LB_CAMINA; s_lib_camina_t0 = s_ms; ocultar(s_sim_t, true); }
    const uint32_t dt = s_lib_n ? s_ms - s_lib_ult : 0;
    if (s_lib_n < LIB_MAX) { s_lib_cris[s_lib_n] = (uint8_t)k; if (s_lib_n) s_lib_ms[s_lib_n - 1] = (uint16_t)(dt > 60000 ? 60000 : dt); s_lib_ms[s_lib_n] = 0; s_lib_n++; }
    if (s_lib_ult16_n == 16) { memmove(s_lib_ult16, s_lib_ult16 + 1, 15); memmove(s_lib_ult16_ms, s_lib_ult16_ms + 1, 15 * sizeof(uint16_t)); s_lib_ult16_n = 15; }
    if (s_lib_ult16_n) s_lib_ult16_ms[s_lib_ult16_n - 1] = (uint16_t)(dt > 60000 ? 60000 : dt);
    s_lib_ult16[s_lib_ult16_n] = (uint8_t)k; s_lib_ult16_ms[s_lib_ult16_n] = 0; s_lib_ult16_n++;
    s_lib_ult = s_ms;
}
/* cómo tocaste: el ritmo medio (sin las pausas largas), si fue larga, si repetiste una frase */
static int lib_estilo(int *mas_tocado)
{
    int suma = 0, cuenta = 0, veces[CRIS_TOT] = {0};
    for (int i = 0; i < s_lib_n; i++) {
        veces[s_lib_cris[i] % CRIS_TOT]++;
        if (i < s_lib_n - 1 && s_lib_ms[i] < 2000) { suma += s_lib_ms[i]; cuenta++; }
    }
    int m = 0;
    for (int k = 1; k < CRIS_TOT; k++) if (veces[k] > veces[m]) m = k;
    *mas_tocado = m;
    const int medio = cuenta ? suma / cuenta : 500;
    int est = 0;
    if (medio < 270) est |= PIEZA_RAPIDA;
    else if (medio > 620) est |= PIEZA_LENTA;
    if (s_lib_n >= 20) est |= PIEZA_LARGA;
    for (int L = 3; L <= 6 && !(est & PIEZA_REPETIDA); L++) /* una frase de 3 a 6 notas, dos veces seguidas */
        for (int i = 0; i + 2 * L <= s_lib_n; i++) {
            bool igual = true;
            for (int j = 0; j < L && igual; j++) igual = s_lib_cris[i + j] == s_lib_cris[i + L + j];
            bool variada = false; /* (no vale tocar un solo cristal muchas veces) */
            for (int j = 1; j < L; j++) variada |= s_lib_cris[i + j] != s_lib_cris[i];
            if (igual && variada) { est |= PIEZA_REPETIDA; break; }
        }
    return est;
}
/* lo que dice el dragón (al azar, sin repetir seguido; a lo más uno cada 2-3 piezas) */
static const char *const DRAGON_DICE[] = {
    "TU MÚSICA ME DA GANAS DE VOLAR", "ASÍ SONABAN LOS CRISTALES CUANDO ÉL VENÍA A ESCUCHARLOS", "OÍR DE VERDAD YA ES CUIDAR",
    "{NOMBRE}, CANTAS CON LOS OJOS. ME GUSTA", "ESA NOTA ME HIZO COSQUILLAS EN LAS ESCAMAS", "TOCA OTRA. TENGO TODA LA NOCHE",
    "CON ESTO DORMIRÍA CIEN AÑOS MÁS... MEJOR NO", "LA CUEVA SE ACUERDA DE TODO LO QUE TOCAS", "AFINADO. CASI. LO DIGO CON CARIÑO",
    "ME ACUERDO DE UNA CANCIÓN ASÍ. ALGUIEN LA TARAREABA MIENTRAS BARRÍA"};
static int s_lib_dice_ult = -1;
static void lib_fin_pieza(void)
{
    int mas = 0;
    const int est = lib_estilo(&mas);
    s_lib_mas_tocado = mas;
    s_lib_reac = est | 0x100; /* (0x100: hay reacción aunque no tenga estilo) */
    s_lib_reac_t0 = s_ms;
    s_lib_reac_ev = -1;
    s_lib_drag_luz_t = s_ms;
    accion(UI_ACCION_PIEZA, (s_lib_n > 255 ? 255 : s_lib_n) | est << 8);
    const bool noche = s_fondo_periodo == PERIODO_NOCHE;
    if (s_lib_dragon) {
        if (est & PIEZA_REPETIDA) { /* el dragón la tararea grave, como eco */
            accion(UI_ACCION_SONAR, SND_DRAGON_TARAREA | 0x100);
            uint8_t eco[8]; int ne = 0;
            for (int i = s_lib_n - 6 < 0 ? 0 : s_lib_n - 6; i < s_lib_n && ne < 6; i++) eco[ne++] = PET_CANCION_NOTA(s_lib_cris[i], 2);
            rep_tocar(eco, ne, REP_CRISTAL | REP_SUAVE);
        } else if (est & PIEZA_LENTA) accion(UI_ACCION_SONAR, (noche ? SND_RONQUIDO : SND_DRAGON_TARAREA) | 0x100);
        else if (est & PIEZA_RAPIDA) accion(UI_ACCION_SONAR, SND_ALEGRE);
        if (est & PIEZA_LARGA) for (int k = 0; k < CRIS_TOT; k++) s_sim_luz_k[k] = s_ms + (uint32_t)k * 110; /* chispas de cristal */
        /* a veces dice algo (no más de una vez cada 2-3 piezas): un capítulo de la historia del Rey o algo de la música */
        if (++s_lib_sin_msg >= 2 && azar() % 3 != 0) {
            s_lib_sin_msg = 0;
            if (s_suave) { static const art_icono_t IC[4] = {ICO_CORAZON, ICO_NOTA, ICO_ESTRELLA, ICO_CORONA}; flotar(IC[azar() % 4], NULL, 0, CX, 96, 400); }
            else if (s_pet && pet_prisma_ready(s_pet, s_ultimo_t) && azar() % 2) { /* (101b) solo si hoy de verdad deja un capítulo */
                accion(UI_ACCION_PRISMA, 0); /* el motor decide si hoy toca un capítulo (queda en el Libro) */
                lib_texto("EL DRAGÓN QUIERE CONTARTE ALGO DEL REY..."); /* (fw 103) sin el «te lo dejo en el libro» */
            } else {
                int k;
                do k = (int)(azar() % (sizeof DRAGON_DICE / sizeof DRAGON_DICE[0])); while (k == s_lib_dice_ult);
                s_lib_dice_ult = k;
                char t[120];
                const char *d = DRAGON_DICE[k], *q = strstr(d, "{NOMBRE}");
                if (q && s_pet) snprintf(t, sizeof t, "%.*s%s%s", (int)(q - d), d, s_pet->nombre, q + 8);
                else snprintf(t, sizeof t, "%s", d);
                lib_texto(gtxt(t));
            }
        }
    } else { /* sin dragón: algo responde lejos, como un eco (una pista) */
        uint8_t eco[3]; int ne = 0;
        for (int i = s_lib_n - 3 < 0 ? 0 : s_lib_n - 3; i < s_lib_n; i++) eco[ne++] = PET_CANCION_NOTA(s_lib_cris[i], 2);
        rep_tocar(eco, ne, REP_CRISTAL | REP_SUAVE);
        if (s_pet && !pet_special_known(s_pet, ESP_CRISTALES) && azar() % 2) lib_texto("ALGO RESPONDE AL FONDO DE LA CUEVA...");
    }
    /* (v39) ella ya no repite tus notas con su «la» (sonaba como un chicharreo): queda solo el eco del dragón, o el lejano */
    s_lib_n = 0;
}
/* el motor contestó la pieza (EV_PIEZA): la escama, un regalo, la propuesta de canción */
static void evento_pieza(const pet_event_t *e)
{
    if (!s_sim_on || !s_sim_libre) return;
    const int a = e->a, obj = e->b - 1;
    if (a & PIEZA_ESCAMA) {
        flotar(ICO_OBJETO(19), NULL, 0, CX, 150, 300);
        lib_texto("¡EL DRAGÓN TE REGALA UNA ESCAMA DE CRISTAL! VA A TU BOLSA");
        for (int k = 0; k < 6; k++) s_sim_luz_k[k] = s_ms + (uint32_t)k * 90;
    } else if ((a & PIEZA_REGALO) && obj >= 0) {
        flotar(ICO_OBJETO(obj), NULL, 0, CX, 150, 300);
        lib_texto(s_lib_dragon ? "¡QUÉ PIEZA TAN LARGA! EL DRAGÓN TE DA UN REGALO" : "¡QUÉ PIEZA TAN LARGA! LA CUEVA TE DEJA UN REGALO");
    }
    if (a & PIEZA_CANCION) { s_cp_propone = (a & PIEZA_DRAGON) ? 2 : 1; s_lib_reac_ev = 1; } /* después de la reacción */
}

/* ---------- componer ---------- */
static void cp_pregunta(void)
{
    s_cp_fase = CP_PREGUNTA;
    lib_texto(s_cp_propone == 2 ? "EL DRAGÓN PREGUNTA: ¿HACEMOS UNA CANCIÓN?" : "¿HACEMOS UNA CANCIÓN? ¡LA NUESTRA!");
    if (s_nino) flotar(ICO_NOTA, NULL, 0, CX, 120, 0);
    static const art_icono_t I[2] = {ICO_NO, ICO_SI};
    static const uint32_t C[2] = {0x7A4458, C_VERDE};
    cp_botones(2, I, C);
}
static void cp_empezar_frase(void)
{
    s_cp_fase = CP_FRASE;
    s_cp_toq_n = 0;
    s_cp_t0 = s_ms;
    lib_texto("TOCA UNA FRASE DE 4 A 8 NOTAS");
    static const art_icono_t I[2] = {ICO_VUELTA, ICO_SI};
    static const uint32_t C[2] = {C_BOTON, C_VERDE};
    cp_botones(2, I, C);
    lv_obj_set_style_opa(s_cp_b[1], LV_OPA_40, 0);
    cp_partitura(NULL, 0, -1);
}
static void cp_ella_propone(void)
{
    s_cp_cont_n = pet_song_continue(s_cp_frase, s_cp_frase_n, s_cp_cont, 8, s_cp_variante);
    s_cp_fase = CP_ELLA;
    s_cp_t0 = s_ms;
    lib_texto(s_cp_variante ? "¿Y ASÍ?" : "Y YO SIGO ASÍ... ESCUCHA");
    uint8_t todo[16];
    memcpy(todo, s_cp_frase, (size_t)s_cp_frase_n); memcpy(todo + s_cp_frase_n, s_cp_cont, (size_t)s_cp_cont_n);
    cp_partitura(todo, s_cp_frase_n + s_cp_cont_n, -1);
    rep_tocar(s_cp_cont, s_cp_cont_n, REP_CRISTAL | REP_VOZ);
    static const art_icono_t I[3] = {ICO_DADO, ICO_SI, ICO_MANO};
    static const uint32_t C[3] = {C_BOTON, C_VERDE, C_BOTON};
    cp_botones(3, I, C);
}
static void cp_escuchar(void)
{
    s_cp_fase = CP_ESCUCHA;
    s_cp_t0 = s_ms;
    s_cp_guardar_n = 0;
    for (int i = 0; i < s_cp_frase_n; i++) s_cp_guardar[s_cp_guardar_n++] = s_cp_frase[i];
    for (int i = 0; i < s_cp_cont_n && s_cp_guardar_n < PET_CANCION_NOTAS; i++) s_cp_guardar[s_cp_guardar_n++] = s_cp_cont[i];
    lib_texto("ESCUCHA: ¡NUESTRA CANCIÓN ENTERA!");
    cp_botones(0, NULL, NULL);
    rep_tocar(s_cp_guardar, s_cp_guardar_n, REP_CRISTAL | REP_VOZ);
}
/* el teclado para el nombre (dentro de la pantalla de los cristales) */
static void cp_tec_refrescar(void)
{
    char t[PET_CANCION_NOMBRE + 2];
    snprintf(t, sizeof t, "%s_", s_cp_nombre);
    art_texto(&s_cp_tec_art, t, 0xFFE27A, 3, 0);
    poner_src(s_cp_tec_txt, &s_cp_tec_art.dsc);
    lv_obj_align(s_cp_tec_txt, LV_ALIGN_TOP_MID, 0, 44);
}
static void cp_guardar(void);
static void cp_tecla_cb(lv_event_t *e)
{
    const int k = (int)(intptr_t)lv_event_get_user_data(e);
    size_t n = strlen(s_cp_nombre);
    if (k == 27) { /* borrar */
        if (!n) return;
        n--;
        while (n > 0 && ((unsigned char)s_cp_nombre[n] & 0xC0) == 0x80) n--;
        s_cp_nombre[n] = 0;
    } else if (k == 28) { cp_guardar(); return; } /* listo */
    else if (k == 29) { /* un nombre al azar */
        static const char *const SUG[] = {"LA DE LA LAGUNA", "LUNA DE CRISTAL", "PASITOS", "LA DE LOS GRILLOS", "NIEVE TIBIA", "HOJITAS",
                                          "EL DRAGÓN BAILA", "GALLETA QUEMADA", "NUESTRO BOSQUE", "BUENAS NOCHES", "ESTRELLITA", "LA DEL CAMINO"};
        snprintf(s_cp_nombre, sizeof s_cp_nombre, "%s", SUG[azar() % (sizeof SUG / sizeof SUG[0])]);
    } else if (k == 30) { if (n && n + 1 < sizeof s_cp_nombre) { s_cp_nombre[n] = ' '; s_cp_nombre[n + 1] = 0; } } /* espacio */
    else {
        if (n + 3 >= sizeof s_cp_nombre) return;
        if (k == 26) { s_cp_nombre[n] = (char)0xC3; s_cp_nombre[n + 1] = (char)0x91; s_cp_nombre[n + 2] = 0; }
        else { s_cp_nombre[n] = (char)('A' + k); s_cp_nombre[n + 1] = 0; }
    }
    cp_tec_refrescar();
}
static void cp_nombre(void)
{
    s_cp_fase = CP_NOMBRE;
    cp_botones(0, NULL, NULL);
    for (int i = 0; i < PET_CANCION_NOTAS; i++) ocultar(s_cp_punto[i], true);
    ocultar(s_sim_t, true);
    snprintf(s_cp_nombre, sizeof s_cp_nombre, "%s", s_pet && !pet_song(s_pet, 0) ? "NUESTRA CANCIÓN" : "");
    if (!s_cp_tec) {
        s_cp_tec = caja(s_sim);
        lv_obj_set_size(s_cp_tec, 400, 400);
        lv_obj_align(s_cp_tec, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_style_radius(s_cp_tec, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(s_cp_tec, lv_color_hex(0x1A1430), 0);
        lv_obj_set_style_bg_opa(s_cp_tec, LV_OPA_90, 0);
        lv_obj_add_flag(s_cp_tec, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_t *t = texto(s_cp_tec, "¿CÓMO SE LLAMA?", C_BLANCO, 2, 0);
        lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 18);
        s_cp_tec_txt = lv_image_create(s_cp_tec);
        static const char *const LETRAS = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
        for (int k = 0; k < 31; k++) {
            if (k == 28 || k == 29) continue; /* (listo y al azar van abajo) */
            const int pos = k < 28 ? k : 28, fila = pos / 7, col = pos % 7;
            lv_obj_t *b = boton(s_cp_tec, 40, 40, cp_tecla_cb, k);
            lv_obj_set_pos(b, 56 + col * 42, 84 + fila * 44);
            lv_obj_set_style_radius(b, 8, 0);
            char l[4] = {0};
            if (k < 26) l[0] = LETRAS[k];
            else if (k == 26) { l[0] = (char)0xC3; l[1] = (char)0x91; }
            else if (k == 27) l[0] = '<';
            else l[0] = '_';
            lv_obj_t *lt = texto(b, l, C_BLANCO, 3, 0);
            lv_obj_center(lt);
            if (k == 27) lv_obj_set_style_bg_color(b, lv_color_hex(0x7A4458), 0);
            if (k == 30) { lv_obj_set_size(b, 82, 40); lv_obj_set_pos(b, 56 + 3 * 42, 84 + 4 * 44); }
        }
        lv_obj_t *dado = boton(s_cp_tec, 48, 48, cp_tecla_cb, 29);
        lv_obj_set_pos(dado, 92, 312);
        lv_obj_t *di = icono(dado, ICO_DADO, 2); lv_obj_center(di);
        lv_obj_t *ok = boton(s_cp_tec, 120, 48, cp_tecla_cb, 28);
        lv_obj_set_pos(ok, 152, 312);
        lv_obj_set_style_bg_color(ok, lv_color_hex(C_VERDE), 0);
        lv_obj_t *oi = icono(ok, ICO_SI, 2); lv_obj_center(oi);
    }
    cp_tec_refrescar();
    ocultar(s_cp_tec, false);
    lv_obj_move_foreground(s_cp_tec);
}
static void cp_guardar(void)
{
    if (s_cp_tec) ocultar(s_cp_tec, true);
    s_cp_fase = CP_LISTO;
    s_cp_t0 = s_ms;
    accion(UI_ACCION_CANCION, -1); /* el motor la guarda (y contesta con EV_NUESTRA) */
    lv_obj_move_foreground(s_sim_x);
}
static void cp_toque(int k) /* tocaste un cristal mientras tocas tu frase (o la continuación) */
{
    const int max = s_cp_fase == CP_TUYA ? (s_cp_frase_n > 8 ? 8 : s_cp_frase_n) : 8;
    if (s_cp_toq_n >= max) return;
    const uint32_t dt = s_cp_toq_n ? s_ms - s_cp_toq_ult : 0;
    if (s_cp_toq_n) s_cp_toq_ms[s_cp_toq_n - 1] = (uint16_t)(dt > 4000 ? 4000 : dt);
    s_cp_toq_cris[s_cp_toq_n] = (uint8_t)k;
    s_cp_toq_ms[s_cp_toq_n] = 0;
    s_cp_toq_n++;
    s_cp_toq_ult = s_ms;
    uint8_t vista[16]; int nv = 0;
    if (s_cp_fase == CP_TUYA) for (int i = 0; i < s_cp_frase_n; i++) vista[nv++] = s_cp_frase[i];
    for (int i = 0; i < s_cp_toq_n; i++) vista[nv++] = PET_CANCION_NOTA(s_cp_toq_cris[i], 2);
    cp_partitura(vista, nv, nv - 1);
    if (s_cp_fase == CP_FRASE) lv_obj_set_style_opa(s_cp_b[1], s_cp_toq_n >= 4 ? LV_OPA_COVER : LV_OPA_40, 0);
}
static void cp_frase_lista(void)
{
    if (s_cp_fase == CP_FRASE) {
        if (s_cp_toq_n < 4) return;
        s_cp_frase_n = pet_song_from_taps(s_cp_toq_cris, s_cp_toq_ms, s_cp_toq_n, s_cp_frase, 8);
        s_cp_variante = 0;
        cp_ella_propone();
    } else if (s_cp_fase == CP_TUYA) {
        if (s_cp_toq_n < 2) return;
        s_cp_cont_n = pet_song_from_taps(s_cp_toq_cris, s_cp_toq_ms, s_cp_toq_n, s_cp_cont, 8);
        cp_escuchar();
    }
}
static void cp_boton_cb(lv_event_t *e)
{
    const int i = (int)(intptr_t)lv_event_get_user_data(e);
    switch (s_cp_fase) {
    case CP_PREGUNTA:
        if (i == 1) cp_empezar_frase();
        else { s_cp_fase = CP_NADA; cp_botones(0, NULL, NULL); lib_texto(s_cp_propone == 2 ? "EL DRAGÓN BOSTEZA: OTRO DÍA SERÁ" : "OTRO DÍA SERÁ"); }
        break;
    case CP_FRASE:
        if (i == 0) cp_empezar_frase(); /* borrar y volver a tocar */
        else cp_frase_lista();
        break;
    case CP_ELLA:
        if (rep_activo()) rep_parar();
        if (i == 0) { s_cp_variante++; cp_ella_propone(); } /* otra propuesta */
        else if (i == 1) cp_escuchar();                         /* así me gusta */
        else { /* la toco yo */
            s_cp_fase = CP_TUYA; s_cp_toq_n = 0; s_cp_t0 = s_ms;
            lib_texto("TÓCALA TÚ: ¿CÓMO SIGUE?");
            static const art_icono_t I[2] = {ICO_VUELTA, ICO_SI};
            static const uint32_t C[2] = {C_BOTON, C_VERDE};
            cp_botones(2, I, C);
            cp_partitura(s_cp_frase, s_cp_frase_n, -1);
        }
        break;
    case CP_TUYA:
        if (i == 0) { s_cp_toq_n = 0; cp_partitura(s_cp_frase, s_cp_frase_n, -1); }
        else cp_frase_lista();
        break;
    default: break;
    }
}
/* el motor guardó la canción */
static void evento_nuestra(const pet_event_t *e)
{
    if (s_sim_on && s_sim_libre) {
        lib_texto(e->a == 0 ? "¡YA TENEMOS NUESTRA CANCIÓN! QUEDÓ EN EL DIARIO" : "¡OTRA CANCIÓN PARA NUESTRO CUADERNO!");
        for (int i = 0; i < 6; i++) { flotar(ICO_CORAZON, NULL, 0, CX - 100 + i * 40, 200, (uint32_t)i * 120); s_sim_luz_k[i] = s_ms + (uint32_t)i * 100; }
        s_cp_fase = CP_NADA;
        lib_pose(4);
    } else globo(ICO_NOTA, s_nino ? NULL : "¡NUESTRA CANCIÓN!", 3000);
}

/* ---------- cada cuadro (lo llama animar_cristales_juego en el modo libre) ---------- */
static void animar_libre(void)
{
    if (!s_lib_pet) return;
    /* las notitas que flotan */
    for (int i = 0; i < 6; i++) {
        const uint32_t d = s_ms - s_lib_nota_t[i];
        if (d > 900) { ocultar(s_lib_nota[i], true); continue; }
        const int k = s_lib_nota_k[i] % CRIS_TOT;
        lv_obj_set_pos(s_lib_nota[i], SIM_POS[k][0] - 12 + (int)(sinf((float)d / 120.0f) * 6), SIM_POS[k][1] - 12 * SIM_POS[k][2] - 30 - (int)(d / 12));
        lv_obj_set_style_image_opa(s_lib_nota[i], (lv_opa_t)(255 - d * 255 / 900), 0);
    }
    /* ella */
    const uint32_t desde_toque = s_ms - s_lib_ult;
    const int32_t tr = (int32_t)(s_ms - s_lib_reac_t0);
    int pose = 0, dy = 0, dx = 0, rot = 0;
    int x = LIB_INI_X, y = LIB_INI_Y;
    if (s_lib_fase == LB_CAMINA) { /* baja hasta la roca */
        const float k = (float)(s_ms - s_lib_camina_t0) / 1300.0f;
        if (k >= 1.0f) s_lib_fase = LB_SENTADA;
        const float kk = k > 1 ? 1 : k;
        x = LIB_INI_X + (int)((LIB_ROCA_X - LIB_INI_X) * kk);
        y = LIB_INI_Y + (int)((LIB_ROCA_Y - 6 - LIB_INI_Y) * kk);
        pose = (s_ms / 200) % 2 ? 1 : 0;
        dy = -(int)fabsf(sinf((float)s_ms / 100.0f) * 3);
    }
    if (s_lib_fase == LB_SENTADA) { /* sentada y quieta hasta que termines */
        x = LIB_ROCA_X; y = LIB_ROCA_Y - 6;
        pose = 2;
        if (s_cp_fase == CP_NADA && desde_toque < 2500 && s_lib_n >= 2) { /* mientras tocas: escucha según cómo tocas */
            int suma = 0, c = 0; bool mismo = true;
            for (int i = s_lib_n - 5 < 0 ? 0 : s_lib_n - 5; i < s_lib_n - 1; i++) { suma += s_lib_ms[i]; c++; mismo &= s_lib_cris[i] == s_lib_cris[s_lib_n - 1]; }
            const int medio = c ? suma / c : 500;
            if (mismo && c >= 3) rot = 140;                                   /* siempre el mismo: ladea la cabeza */
            else if (medio > 600) { pose = 3; dx = (int)(sinf((float)s_ms / 500.0f) * 4); } /* lento: ojos cerrados, se mece */
            else if (medio < 280) { pose = 4; dy = -(int)fabsf(sinf((float)s_ms / 90.0f) * 6); } /* rápido: salta en su lugar */
        }
        if (s_lib_reac) { /* la reacción al terminar la pieza */
            const bool noche = s_fondo_periodo == PERIODO_NOCHE;
            if ((s_lib_reac & PIEZA_LENTA) && noche && s_lib_dragon) { pose = 5; dy = (tr / 700) % 2 ? 3 : 0; }              /* ella cabecea */
            else if (s_lib_reac & PIEZA_RAPIDA) { pose = 4; dy = tr < 1800 ? -(int)fabsf(sinf((float)tr / 110.0f) * 10) : 0; } /* aplaude */
            else if (s_lib_reac & PIEZA_LENTA) { pose = 3; dx = (int)(sinf((float)tr / 500.0f) * 4); }
            else pose = 4;
        }
    }
    lib_pose(pose);
    const int lw = (int)s_lib_pet_art[pose].dsc.header.w, lh = (int)s_lib_pet_art[pose].dsc.header.h;
    lv_obj_set_pos(s_lib_pet, x - lw / 2 + dx, y - lh + dy);
    lv_image_set_pivot(s_lib_pet, lw / 2, lh);
    lv_image_set_rotation(s_lib_pet, rot);
    /* el dragón: chiquito arriba al centro; reacciona a la pieza */
    if (s_lib_dragon) {
        int f = 1;
        const bool noche = s_fondo_periodo == PERIODO_NOCHE;
        if (s_lib_reac) {
            if (s_lib_reac & PIEZA_RAPIDA) f = 2;                       /* abre las alas */
            else if ((s_lib_reac & PIEZA_LENTA) && noche) f = 0;        /* se duerme */
            else if (s_lib_reac & PIEZA_LENTA) f = tr < 1500 ? 1 : 0;   /* ronronea y bosteza */
            else if (s_lib_reac & (PIEZA_LARGA | PIEZA_REPETIDA)) f = 2;
        } else if (desde_toque > 60000 && noche) f = 0;               /* sin música, de noche, dormita */
        if (f != s_lib_drag_f) {
            s_lib_drag_f = f;
            if (!s_lib_drag_art[f].dsc.data) art_dragon_pintado(&s_lib_drag_art[f], f, 1);
            poner_src(s_lib_drag, &s_lib_drag_art[f].dsc);
            lv_obj_align(s_lib_drag, LV_ALIGN_TOP_MID, 0, 14);
        }
        lv_obj_set_style_translate_y(s_lib_drag, (int32_t)(sinf((float)s_ms / 600.0f) * (f ? 3 : 1)), 0);
        if (f == 0 && s_ms % 1400 < TICK_MS) flotar(ICO_ZETA, NULL, 0, CX + 30, 40, 0); /* burbujitas al roncar */
        /* sus alas (la luz de atrás) brillan con el color más tocado */
        const bool brilla = s_lib_reac && tr < 2500 && s_lib_mas_tocado >= 0;
        lv_obj_set_style_bg_color(s_sim_luz, lv_color_hex(brilla ? SIM_COLOR[s_lib_mas_tocado] : 0xB48AF0), 0);
        lv_obj_set_style_bg_opa(s_sim_luz, brilla ? 80 : 24, 0);
    }
    /* ¿terminó una pieza? */
    if (s_cp_fase == CP_NADA && s_lib_n > 0 && desde_toque > 3000) {
        if (s_lib_n >= PET_PIEZA_MIN) lib_fin_pieza();
        else s_lib_n = 0; /* menos de 6 notas: nadie reacciona */
    }
    /* la reacción dura unos segundos; después, si propusieron una canción, la pregunta */
    if (s_lib_reac && tr > 4200 && !rep_activo()) {
        s_lib_reac = 0;
        if (s_lib_reac_ev == 1 && s_cp_fase == CP_NADA) { s_lib_reac_ev = -1; cp_pregunta(); }
    }
    if (!s_lib_reac && s_lib_reac_ev == 1 && s_cp_fase == CP_NADA && tr > 4200) { s_lib_reac_ev = -1; cp_pregunta(); }
    /* componer: la frase termina sola con 8 notas (o 2 s después de la última, si ya hay 4) */
    if ((s_cp_fase == CP_FRASE || s_cp_fase == CP_TUYA) && s_cp_toq_n > 0) {
        const int max = s_cp_fase == CP_TUYA ? (s_cp_frase_n > 8 ? 8 : s_cp_frase_n) : 8;
        if ((s_cp_toq_n >= max && s_ms - s_cp_toq_ult > 700) || (s_cp_toq_n >= 4 && s_ms - s_cp_toq_ult > 2600)) cp_frase_lista();
    }
    if (s_cp_fase == CP_ELLA || s_cp_fase == CP_ESCUCHA) { /* la partitura sigue lo que suena */
        uint8_t todo[16]; int n = 0;
        for (int i = 0; i < s_cp_frase_n; i++) todo[n++] = s_cp_frase[i];
        for (int i = 0; i < s_cp_cont_n && n < 16; i++) todo[n++] = s_cp_cont[i];
        const int ra = rep_nota_actual();
        cp_partitura(todo, n, ra < 0 ? -1 : (s_cp_fase == CP_ELLA ? s_cp_frase_n + ra : ra));
        if (s_cp_fase == CP_ESCUCHA && !rep_activo() && s_ms - s_cp_t0 > 1000) cp_nombre();
    }
    if (s_cp_fase == CP_LISTO && s_ms - s_cp_t0 > 6000) s_cp_fase = CP_NADA; /* (por si no llegara la respuesta) */
}

/* ---------- el menú de los cristales y el cuaderno de canciones ---------- */
static void panel_canciones(void);
static void crismenu_cb(lv_event_t *e)
{
    const int q = (int)(intptr_t)lv_event_get_user_data(e);
    if (q == 0) cristales_empezar();
    else if (q == 1) cristales_libre(false);
    else if (q == 2) panel_canciones();
    else if (q == 3) cristales_libre(true);
}
static void panel_crismenu(void)
{
    abrir_tarjeta(PANEL_CRISMENU, 350, 330);
    if (!s_nino) { lv_obj_t *t = texto(s_tarjeta, "LOS CRISTALES", 0xFFE9A0, 2, 0); lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 22); }
    const bool canciones = s_pet && (pet_song_count(s_pet) > 0 || pet_final_seen(s_pet, PET_FINAL_SECRETO)), componer = s_pet && pet_song_can_compose(s_pet) && pet_song_count(s_pet) < PET_CANCIONES;
    int q[4], n = 0;
    q[n++] = 0; q[n++] = 1;
    if (canciones) q[n++] = 2;
    if (componer && canciones) q[n++] = 3; /* (la primera canción la propone ella; después, cuando quieras) */
    static const art_icono_t ICO[4] = {ICO_CRISTAL, ICO_NOTA, ICO_LIBRO, ICO_MANO};
    static const char *const TXT[4] = {"MEMORIA", "TOCAR LIBRE", "CANCIONES", "COMPONER"};
    static const uint32_t COL[4][2] = {{0xC8A8FF, 0x6A48C0}, {0x6AB8FF, 0x2E78C8}, {0xFFD45C, 0xD8962A}, {0x6ADC8A, 0x2E9A52}};
    const int D = s_nino ? 96 : 84;
    for (int i = 0; i < n; i++) {
        const int fila = n > 2 ? i / 2 : 0, col = n > 2 ? i % 2 : i, cant = n > 2 ? 2 : n;
        lv_obj_t *b = boton(s_tarjeta, D, D, crismenu_cb, q[i]);
        ficha_magica(b, COL[q[i]][0], COL[q[i]][1], LV_RADIUS_CIRCLE);
        lv_obj_set_ext_click_area(b, 8);
        const int x = (int)((col - (cant - 1) / 2.0f) * 120), y = (n > 2 ? 62 + fila * 128 : 110);
        lv_obj_align(b, LV_ALIGN_TOP_MID, x, y);
        lv_obj_t *ic = icono(b, ICO[q[i]], 3);
        lv_obj_center(ic);
        if (!s_nino) { lv_obj_t *t = texto(s_tarjeta, TXT[q[i]], 0xFFF3C8, 2, 0); lv_obj_align_to(t, b, LV_ALIGN_OUT_BOTTOM_MID, 0, 3); }
    }
}
static int s_canc_borrar = -1;
static void canciones_cb(lv_event_t *e)
{
    const int v = (int)(intptr_t)lv_event_get_user_data(e), k = v & 15, que = v >> 4;
    if (s_pet && k == 15) { uint8_t ts[PET_CANCION_NOTAS]; const int tn = pet_tilo_song(s_pet, ts, PET_CANCION_NOTAS); if (tn) rep_tocar(ts, tn, REP_CRISTAL); return; } /* (v39) la canción de Tilo */
    if (!s_pet || !pet_song(s_pet, k)) return;
    if (que == 0) { cancion_tocar(k, REP_CRISTAL | REP_VOZ); return; }               /* escucharla */
    if (que == 1) accion(UI_ACCION_CANCION_USO, (pet_song_for(s_pet, 0) == k ? 15 : k) | 0 << 4); /* para las alarmas (o ya no) */
    if (que == 2) accion(UI_ACCION_CANCION_USO, (pet_song_for(s_pet, 1) == k ? 15 : k) | 1 << 4); /* de canción de cuna (o ya no) */
    if (que == 3) { /* borrar: hay que tocar dos veces */
        if (s_canc_borrar != k) { s_canc_borrar = k; lv_obj_set_style_bg_color(lv_event_get_current_target(e), lv_color_hex(C_ROJO), 0); return; }
        s_canc_borrar = -1;
        accion(UI_ACCION_CANCION_USO, k | 0x100);
    }
    s_canc_pend = 1; /* se redibuja cuando el motor lo anote */
}
static void panel_canciones(void)
{
    abrir_tarjeta(PANEL_CANCIONES, 400, 400);
    s_canc_pend = 0;
    if (!s_pet) return;
    if (!s_nino) { lv_obj_t *t = texto(s_tarjeta, "NUESTRAS CANCIONES", 0xFFE9A0, 2, 0); lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 40); }
    const int al = pet_song_for(s_pet, 0), cu = pet_song_for(s_pet, 1);
    int fila = 0;
    { /* (v39) después del último verso: la canción de Tilo, terminada (solo se escucha) */
        uint8_t ts[PET_CANCION_NOTAS];
        const int tn = pet_tilo_song(s_pet, ts, PET_CANCION_NOTAS);
        if (tn) {
            const int y = 70;
            fila++;
            if (!s_nino) { lv_obj_t *t = texto(s_tarjeta, "LA CANCIÓN DE PEUMO", 0xFFD76A, 2, 0); lv_obj_set_pos(t, 40, y); } /* (v97) en una línea: partido en dos tapaba la partitura */
            for (int i = 0; i < tn; i++) {
                lv_obj_t *pt = caja(s_tarjeta);
                lv_obj_set_size(pt, 8, 8);
                lv_obj_set_style_radius(pt, LV_RADIUS_CIRCLE, 0);
                lv_obj_set_style_bg_color(pt, lv_color_hex(SIM_COLOR[PET_CANCION_CRISTAL(ts[i]) % CRIS_TOT]), 0);
                lv_obj_set_style_bg_opa(pt, LV_OPA_COVER, 0);
                lv_obj_set_pos(pt, 50 + i * 11, y + (s_nino ? 14 : 26));
            }
            lv_obj_t *bt = boton(s_tarjeta, 36, 36, canciones_cb, 15);
            lv_obj_set_style_radius(bt, LV_RADIUS_CIRCLE, 0);
            lv_obj_set_pos(bt, 300, y + 18);
            lv_obj_center(icono(bt, ICO_FLECHA, 2));
            lv_obj_t *cu = icono(s_tarjeta, ICO_CUCHARA_PALO, 2);
            lv_obj_set_pos(cu, 342, y + 18);
        }
    }
    for (int k = 0; k < PET_CANCIONES; k++) {
        const cancion_t *c = pet_song(s_pet, k);
        if (!c) continue;
        const int y = 70 + fila * 56;
        fila++;
        if (!s_nino) { lv_obj_t *t = texto(s_tarjeta, c->nombre, k == 0 ? 0xFFE27A : C_BLANCO, 2, 0); lv_obj_set_pos(t, 40, y); } /* (v97) en una línea */
        for (int i = 0; i < c->n; i++) { /* la partitura de colores */
            lv_obj_t *pt = caja(s_tarjeta);
            const int lado = 8 + (PET_CANCION_PASOS(c->nota[i]) > 2 ? 3 : 0);
            lv_obj_set_size(pt, lado, lado);
            lv_obj_set_style_radius(pt, LV_RADIUS_CIRCLE, 0);
            lv_obj_set_style_bg_color(pt, lv_color_hex(SIM_COLOR[PET_CANCION_CRISTAL(c->nota[i]) % CRIS_TOT]), 0);
            lv_obj_set_style_bg_opa(pt, LV_OPA_COVER, 0);
            lv_obj_set_pos(pt, 50 + i * 11 + (11 - lado) / 2, y + (s_nino ? 14 : 26) + (11 - lado) / 2);
        }
        static const art_icono_t IB[4] = {ICO_FLECHA, ICO_DESPERTADOR, ICO_LUNA, ICO_NO};
        for (int b = 0; b < 4; b++) {
            if (b == 3 && s_nino) continue; /* (los más chicos no borran) */
            lv_obj_t *bt = boton(s_tarjeta, 36, 36, canciones_cb, k | b << 4);
            lv_obj_set_style_radius(bt, LV_RADIUS_CIRCLE, 0);
            lv_obj_set_pos(bt, 230 + b * 37, y + 6);
            const bool on = (b == 1 && al == k) || (b == 2 && cu == k);
            if (on) lv_obj_set_style_bg_color(bt, lv_color_hex(C_VERDE), 0);
            lv_obj_t *ic = icono(bt, IB[b], 2); lv_obj_center(ic);
        }
    }
    if (!fila && !s_nino) { lv_obj_t *t = texto(s_tarjeta, "TODAVÍA NO TENEMOS CANCIONES. TOQUEN JUNTOS EN LOS CRISTALES DE LA CUEVA", C_BLANCO, 2, 300); lv_obj_align(t, LV_ALIGN_CENTER, 0, 0); }
}

/* ---------- fuera de la cueva: el tarareo, la cuna y las alarmas ---------- */
static uint32_t s_tararea_ult;
/* contenta y cantando: a veces tararea nuestra canción (devuelve true si la está cantando) */
static bool tararea_cancion(const pet_t *p)
{
    if (!pet_song(p, 0) || s_sim_on) return false;
    if (rep_activo()) return (s_rep_modo & REP_FLOTAR) != 0;
    if (s_ms - s_tararea_ult < 45000u || p->emo[EMO_ALEGRIA] < 0.4f) return false;
    s_tararea_ult = s_ms;
    cancion_tocar(0, REP_VOZ | REP_SUAVE | REP_FLOTAR);
    return true;
}
/* la canción de cuna: si eligieron una, ella la canta (suave) en vez de la de siempre; devuelve true si la cantó */
static bool cuna_cancion(void)
{
    const int k = s_pet ? pet_song_for(s_pet, 1) : -1;
    if (k < 0) return false;
    cancion_tocar(k, REP_VOZ | REP_CRISTAL | REP_SUAVE | REP_FLOTAR);
    return true;
}
/* las alarmas con nuestra canción: suena la canción y, entre una vuelta y otra, el sonido de la alarma (así se oye igual) */
static bool alarma_cancion(const pet_t *ap)
{
    const int k = ap ? pet_song_for(ap, 0) : -1;
    if (k < 0) return false;
    if (rep_activo()) return true;
    const cancion_t *c = pet_song(ap, k);
    if (c) rep_tocar(c->nota, c->n, REP_CRISTAL | REP_VOZ);
    return true;
}
static void animar_musica(void) { animar_reproductor(); if (s_canc_pend && s_panel == PANEL_CANCIONES && s_canc_pend++ > 2) panel_canciones(); }
void ui_demo_libre(int componer) { cristales_libre(componer != 0); } /* (simulador) */
