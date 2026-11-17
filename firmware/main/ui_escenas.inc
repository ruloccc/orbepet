/* ======================================================================
 * Las escenas de las historias: cuando una criatura cuenta algo del Cuidador, arriba se anima lo que cuenta.
 * Una viñeta de 240x120: cielo, colinas y siluetas de fondo, y unos pocos dibujos que siguen un guion de diez segundos
 * (aparecen, se mueven con suavidad y se van en su momento); los personajes respiran y las luces titilan.
 * ==================================================================== */
#define ESC_W 240
#define ESC_H 120
#define SUELO 96
#define MAX_SH 20
#define MAX_AC 40
#define ESC_PERIODO 10000
enum { SH_EL, SH_RE, SH_IC };
typedef struct { uint8_t k; art_icono_t ico; uint8_t esc; int16_t x, y, w, h; uint32_t col; uint8_t opa; } shape_t;
typedef struct { int16_t t, x, y; uint8_t s, o; } kf_t;          /* t en % del guion; s en % del tamaño; o opacidad */
typedef struct { art_icono_t ico; uint8_t esc; int8_t wx, wy; uint8_t wn, wph; int8_t rot; uint8_t tw; uint8_t n; kf_t k[6]; } act_t;
typedef struct { uint32_t cielo, fondo, suelo; uint8_t ns, na; shape_t sh[MAX_SH]; act_t a[MAX_AC]; } vineta_t;

#define K(t, x, y, s, o) {t, x, y, s, o}
#define EL(x, y, w, h, c) {SH_EL, 0, 0, x, y, w, h, c, 255}
#define ELO(x, y, w, h, c, o) {SH_EL, 0, 0, x, y, w, h, c, o}
#define RE(x, y, w, h, c) {SH_RE, 0, 0, x, y, w, h, c, 255}
#define REO(x, y, w, h, c, o) {SH_RE, 0, 0, x, y, w, h, c, o}
#define SI(i, e, x, y, c) {SH_IC, i, e, x, y, 0, 0, c, 255}          /* silueta (el dibujo teñido) */
#define SIC(i, e, x, y) {SH_IC, i, e, x, y, 0, 0, 0xFFFFFFFFu, 255}  /* el dibujo tal cual */
/* los que actúan */
#define ST(i, e, x, y) {i, e, 0, 2, 8, 0, 0, 0, 2, {K(0, x, y, 100, 255), K(100, x, y, 100, 255)}}   /* siempre ahí, respirando */
#define SS(i, e, x, y) {i, e, 0, 0, 0, 0, 0, 0, 2, {K(0, x, y, 100, 255), K(100, x, y, 100, 255)}}   /* siempre ahí, quieto */
#define APS(i, e, x, y, t0) {i, e, 0, 2, 8, 0, 0, 0, 5, {K(0, x, y, 50, 0), K(t0, x, y, 50, 0), K((t0) + 5, x, y, 100, 255), K(94, x, y, 100, 255), K(98, x, y, 100, 0)}}
#define MOV(i, e, x0, y0, x1, y1, t0, t1) {i, e, 0, 0, 0, 0, 0, 0, 6, {K(0, x0, y0, 100, 0), K(t0, x0, y0, 100, 0), K((t0) + 4, x0, y0, 100, 255), K((t1) - 4, x1, y1, 100, 255), K(t1, x1, y1, 100, 0), K(100, x1, y1, 100, 0)}}
#define MOW(i, e, x0, y0, x1, y1, t0, t1, wy, wn) {i, e, 0, wy, wn, 0, 0, 0, 6, {K(0, x0, y0, 100, 0), K(t0, x0, y0, 100, 0), K((t0) + 4, x0, y0, 100, 255), K((t1) - 4, x1, y1, 100, 255), K(t1, x1, y1, 100, 0), K(100, x1, y1, 100, 0)}}
#define GRO(i, e, x, y, t0, t1, sm) {i, e, 0, 0, 0, 0, 0, 0, 6, {K(0, x, y, 20, 0), K(t0, x, y, 20, 0), K((t0) + 3, x, y, 30, 255), K(t1, x, y, sm, 255), K(95, x, y, sm, 255), K(99, x, y, sm, 0)}}
#define SP(i, e, x, y, t0, tm, t1) {i, e, 0, 0, 0, 0, 0, 0, 4, {K(0, x, y, 40, 0), K(t0, x, y, 40, 0), K(tm, x, y, 140, 255), K(t1, x, y, 40, 0)}}
#define STAR(x, y, ph) {ICO_ESTRELLA, 1, 0, 0, 0, ph, 0, 3, 2, {K(0, x, y, 70, 255), K(100, x, y, 70, 255)}} /* estrellas chicas */
#define ORB(i, e, cx, cy, rx, ry, n, ph) {i, e, rx, ry, n, ph, 0, 0, 2, {K(0, cx, cy, 100, 255), K(100, cx, cy, 100, 255)}}
#define ORBF(i, e, cx, cy, rx, ry, n, ph, tw) {i, e, rx, ry, n, ph, 0, tw, 2, {K(0, cx, cy, 100, 255), K(100, cx, cy, 100, 255)}}
#define CRIS(i, e, x, y, tw, ph) {i, e, 0, 1, 4, ph, 0, tw, 2, {K(0, x, y, 100, 255), K(100, x, y, 100, 255)}}
#define SPIN(i, e, x, y, r) {i, e, 0, 0, 0, 0, r, 0, 2, {K(0, x, y, 100, 255), K(100, x, y, 100, 255)}}
#define NUB(i, e, y, ph) {i, e, 0, 1, 2, 0, 0, 0, 6, {K(0, -30 + (ph), y, 100, 0), K(2, -26 + (ph), y, 100, 255), K(50, 120, y, 100, 255), K(97, 266, y, 100, 255), K(99, 270, y, 100, 0), K(100, 270, y, 100, 0)}}

static lv_obj_t *s_esc_o[MAX_AC];
static int16_t s_esc_w[MAX_AC], s_esc_h[MAX_AC];
static const vineta_t *s_esc_v;
/* la quinta historia de cada criatura (la del Cuidador) abre con el cielo estrellado y una pequeña señal: una estrella que late y otra que cruza */
static lv_obj_t *s_esc_velo, *s_esc_senal, *s_esc_fugaz;
static int s_esc_z = 1; /* ampliación: 1 en las historias; 2 en las escenas a pantalla completa */

static EXT_RAM_BSS_ATTR act_t s_amb[28]; /* lo de ambiente de cada escena: nubes, pájaros, luciérnagas, bichitos... (se arma al mostrarla) */
static int s_amb_n;
static int esc_suave(int a, int b, int f) /* f 0..1000, con aceleración y frenado */
{
    const int e = f * f / 1000 * (3000 - 2 * f) / 1000;
    return a + (b - a) * e / 1000;
}
static void esc_cb(void *var, int32_t v)
{
    (void)var;
    const vineta_t *e = s_esc_v;
    if (!e) return;
    const int tt = (int)(v % 1000);
    if (s_esc_velo) {
        const int op = tt < 220 ? 255 : tt < 360 ? 255 * (360 - tt) / 140 : 0;
        lv_obj_set_style_opa(s_esc_velo, (lv_opa_t)op, 0);
        if (s_esc_senal) lv_obj_set_style_opa(s_esc_senal, (lv_opa_t)(130 + 125 * lv_trigo_sin((int16_t)(tt * 3 * 360 / 1000)) / 32767), 0);
        if (s_esc_fugaz) { /* la señal: una estrella chiquita cruza el cielo */
            const int f = tt < 90 ? 0 : tt > 250 ? 160 : (tt - 90);
            lv_obj_set_pos(s_esc_fugaz, 20 + f * 120 / 160, 18 + f * 40 / 160);
            lv_obj_set_style_opa(s_esc_fugaz, (lv_opa_t)(tt < 90 || tt > 250 ? 0 : 255), 0);
        }
    }
    for (int i = 0; i < e->na + s_amb_n; i++) {
        const act_t *a = i < e->na ? &e->a[i] : &s_amb[i - e->na];
        lv_obj_t *o = s_esc_o[i];
        if (!o || !a->n) continue;
        int j = 0;
        while (j + 1 < a->n && tt >= a->k[j + 1].t * 10) j++;
        int x, y, s, op;
        if (j + 1 >= a->n) { x = a->k[j].x; y = a->k[j].y; s = a->k[j].s; op = a->k[j].o; }
        else {
            const int t0 = a->k[j].t * 10, t1 = a->k[j + 1].t * 10;
            int f = t1 > t0 ? (tt - t0) * 1000 / (t1 - t0) : 1000;
            if (f < 0) f = 0;
            x = esc_suave(a->k[j].x, a->k[j + 1].x, f); y = esc_suave(a->k[j].y, a->k[j + 1].y, f);
            s = esc_suave(a->k[j].s, a->k[j + 1].s, f); op = esc_suave(a->k[j].o, a->k[j + 1].o, f);
        }
        if (a->wn && (a->wx || a->wy)) {
            const int ang = (a->wn * tt + a->wph * 10) * 360 / 1000;
            x += a->wx * lv_trigo_sin((int16_t)ang) / 32767;
            y += a->wy * lv_trigo_cos((int16_t)ang) / 32767;
        }
        if (a->tw) {
            const int ang = (a->tw * tt + a->wph * 10) * 360 / 1000;
            op = op * (150 + 105 * lv_trigo_sin((int16_t)ang) / 32767) / 255;
        }
        lv_obj_set_style_opa(o, (lv_opa_t)(op < 0 ? 0 : op > 255 ? 255 : op), 0);
        lv_image_set_scale(o, (uint32_t)(s * 256 / 100));
        if (a->rot) lv_image_set_rotation(o, (int32_t)((a->rot * 3600 * tt / 1000) % 3600));
        lv_obj_set_pos(o, x * s_esc_z - s_esc_w[i] / 2, y * s_esc_z - s_esc_h[i] / 2);
    }
}

static const vineta_t ESCENAS[(CRI_COUNT + 1) * 5] = { /* la última fila es Nimba, la ballena de nubes (una nube grande que cruza) */
    {0x9FD8F0, 0xD8F0C0, 0x5DB27A, 7, 12,
     {EL(36,106,160,40,0x7FC48A), EL(196,108,180,40,0x6DBA7C), SI(ICO_ARBOLITO,2,224,76,0x4E9A62), SI(ICO_ARBOLITO,1,200,88,0x5AA870), SIC(ICO_FLOR,1,18,98), SIC(ICO_HIERBA,1,150,102), SIC(ICO_FLOR,1,100,104)},
     {NUB(ICO_NUBE,3,22,0),
      NUB(ICO_NUBE,2,40,120),
      ST(ICO_SOL,3,214,24),
      ST(ICO_CUIDADOR,3,46,76),
      APS(ICO_MUSGUITO,3,88,84,6),
      MOV(ICO_SEMILLA,2,62,54,126,92,10,26),
      MOV(ICO_GOTA,2,126,16,126,86,30,40),
      MOV(ICO_GOTA,2,122,16,122,86,42,52),
      GRO(ICO_BROTE,3,126,84,52,72,100),
      APS(ICO_HIERBA,3,170,90,68),
      APS(ICO_HIERBA,3,204,88,76),
      APS(ICO_FLOR,3,152,94,84)}},
    {0x1E2A52, 0x3A4A7A, 0x3E7A4E, 5, 10,
     {EL(36,106,160,40,0x2E6A44), EL(196,108,180,40,0x2A5E3E), SI(ICO_ARBOLITO,2,18,80,0x1E4A30), SIC(ICO_HIERBA,1,72,102), SIC(ICO_HONGO,1,214,102)},
     {ST(ICO_ARBOL_OFRENDAS,5,118,58),
      ST(ICO_LUNA,3,206,24),
      STAR(40,22,0),
      STAR(72,42,30),
      STAR(170,18,60),
      ST(ICO_CUIDADOR,2,56,82),
      MOV(ICO_REGALO,2,76,84,114,74,8,30),
      MOV(ICO_SOL,2,118,70,150,34,56,82),
      ORBF(ICO_LUMBRE,2,168,56,22,12,3,0,5),
      ORBF(ICO_LUMBRE,2,140,48,26,10,2,50,4)}},
    {0x2A2A5E, 0x4A4A8A, 0x3E7A4E, 5, 11,
     {EL(36,106,160,40,0x2E6A44), EL(196,108,180,40,0x2A5E3E), SI(ICO_ARBOLITO,2,18,80,0x1E4A30), SIC(ICO_HIERBA,1,72,102), SIC(ICO_HONGO,1,214,102)},
     {{ICO_FLOR,3,0,1,4,0,0,0,5,{K(0,60,80,100,255),K(25,60,80,100,255),K(45,60,80,55,255),K(85,60,80,55,255),K(100,60,80,100,255)}},
      {ICO_FLOR,3,0,1,4,30,0,0,5,{K(0,118,84,100,255),K(35,118,84,100,255),K(55,118,84,55,255),K(85,118,84,55,255),K(100,118,84,100,255)}},
      {ICO_FLOR,3,0,1,4,60,0,0,5,{K(0,176,80,100,255),K(45,176,80,100,255),K(65,176,80,55,255),K(85,176,80,55,255),K(100,176,80,100,255)}},
      ST(ICO_CUIDADOR,3,26,78),
      ST(ICO_LUNA,3,206,24),
      STAR(40,20,0),
      STAR(110,30,40),
      STAR(160,16,70),
      MOV(ICO_NOTA,2,66,70,80,36,30,60),
      MOV(ICO_NOTA,2,124,72,140,38,48,78),
      MOV(ICO_NOTA,2,180,70,170,34,62,92)}},
    {0x8FC8F0, 0xCCE8F8, 0x5DB27A, 7, 10,
     {EL(36,106,160,40,0x7FC48A), EL(196,108,180,40,0x6DBA7C), SI(ICO_ARBOLITO,2,224,76,0x4E9A62), SI(ICO_ARBOLITO,1,200,88,0x5AA870), SIC(ICO_FLOR,1,18,98), SIC(ICO_HIERBA,1,150,102), SIC(ICO_FLOR,1,100,104)},
     {ST(ICO_SOL,3,34,26),
      MOV(ICO_NUBE,4,262,30,170,30,2,72),
      MOV(ICO_GOTA,2,170,40,168,88,18,34),
      MOV(ICO_GOTA,2,180,40,182,90,24,40),
      MOV(ICO_GOTA,2,160,40,158,88,30,46),
      GRO(ICO_ARCOIRIS,6,120,58,38,58,100),
      ST(ICO_CUIDADOR,3,60,78),
      SP(ICO_CORAZON,2,96,48,60,70,86),
      APS(ICO_HIERBA,3,130,94,40),
      APS(ICO_FLOR,3,200,92,48)}},
    {0xF2D8A0, 0xFFF0C8, 0x7A9A4E, 7, 8,
     {EL(36,106,160,40,0x7FC48A), EL(196,108,180,40,0x6DBA7C), SI(ICO_ARBOLITO,2,224,76,0x4E9A62), SI(ICO_ARBOLITO,1,200,88,0x5AA870), SIC(ICO_FLOR,1,18,98), SIC(ICO_HIERBA,1,150,102), SIC(ICO_FLOR,1,100,104)},
     {ST(ICO_SOL,3,208,24),
      ST(ICO_CUIDADOR,3,92,76),
      ST(ICO_MUSGUITO,3,150,80),
      SP(ICO_SEMILLA,2,60,50,6,14,24),
      SP(ICO_NOTA,2,186,48,28,36,46),
      SP(ICO_PERSONA,2,122,40,50,58,68),
      SP(ICO_CORAZON,2,122,44,74,84,94),
      NUB(ICO_NUBE,2,22,40)}},
    {0x0C1A2E, 0x1E3A3A, 0x1F4A30, 7, 10,
     {EL(120,114,300,44,0x143828), SI(ICO_ARBOLITO,4,14,62,0x08140F), SI(ICO_ARBOLITO,3,48,76,0x0C1E16), SI(ICO_ARBOLITO,4,228,60,0x08140F), SI(ICO_ARBOLITO,3,198,76,0x0C1E16), SIC(ICO_HONGO,1,100,106), SIC(ICO_HIERBA,1,150,104)},
     {ST(ICO_LUNA,3,206,24),
      STAR(30,20,0),
      STAR(90,34,40),
      STAR(150,16,70),
      ST(ICO_CUIDADOR,3,28,80),
      MOV(ICO_ESTRELLA_FUGAZ,2,190,24,70,64,6,30),
      ORBF(ICO_LUMBRE,2,70,64,20,12,2,0,6),
      ORBF(ICO_LUMBRE,2,118,76,26,10,3,30,5),
      ORBF(ICO_LUMBRE,2,164,60,20,14,2,60,7),
      ORBF(ICO_HADA,2,100,48,30,12,1,10,4)}},
    {0x0C1A2E, 0x2A4A44, 0x1F4A30, 7, 10,
     {EL(120,114,300,44,0x143828), SI(ICO_ARBOLITO,4,14,62,0x08140F), SI(ICO_ARBOLITO,3,48,76,0x0C1E16), SI(ICO_ARBOLITO,4,228,60,0x08140F), SI(ICO_ARBOLITO,3,198,76,0x0C1E16), SIC(ICO_HONGO,1,100,106), SIC(ICO_HIERBA,1,150,104)},
     {ST(ICO_ARBOLITO,6,150,54), /* (v39) el pino más viejo, sin adornos: de noche, con el Cuidador chiquito al lado y la luz de Lumbre que se acerca */
      ST(ICO_LUNA,3,30,26),
      STAR(80,14,0),
      STAR(190,16,50),
      STAR(120,30,20),
      MOV(ICO_LUMBRE,2,222,40,128,72,30,70),
      MOW(ICO_ZETA,2,66,66,76,40,6,40,0,0),
      MOW(ICO_ZETA,2,60,66,70,40,28,60,0,0),
      ST(ICO_CUIDADOR,2,96,82),
      GRO(ICO_ESTRELLA,3,150,18,50,70,150)}},
    {0x0C1A2E, 0x24443A, 0x1F4A30, 7, 11,
     {EL(120,114,300,44,0x143828), SI(ICO_ARBOLITO,4,14,62,0x08140F), SI(ICO_ARBOLITO,3,48,76,0x0C1E16), SI(ICO_ARBOLITO,4,228,60,0x08140F), SI(ICO_ARBOLITO,3,198,76,0x0C1E16), SIC(ICO_HONGO,1,100,106), SIC(ICO_HIERBA,1,150,104)},
     {MOV(ICO_HUELLA,2,30,92,70,86,4,26),
      MOV(ICO_HUELLA,2,74,84,112,80,16,38),
      MOV(ICO_HUELLA,2,116,88,154,82,28,50),
      MOV(ICO_HUELLA,2,158,80,196,76,40,62),
      GRO(ICO_HONGO_LUZ,3,214,84,60,70,110),
      ORB(ICO_ARBOLITO,3,26,56,6,0,2,0),
      ORB(ICO_ARBOLITO,2,170,52,8,0,3,40),
      ORBF(ICO_LUMBRE,2,120,40,40,10,1,0,5),
      STAR(120,18,0),
      STAR(60,22,50),
      APS(ICO_CUIDADOR,3,200,80,64)}},
    {0x1A1240, 0x3A2A6A, 0x244A44, 7, 10,
     {EL(120,114,300,44,0x143828), SI(ICO_ARBOLITO,4,14,62,0x08140F), SI(ICO_ARBOLITO,3,48,76,0x0C1E16), SI(ICO_ARBOLITO,4,228,60,0x08140F), SI(ICO_ARBOLITO,3,198,76,0x0C1E16), SIC(ICO_HONGO,1,100,106), SIC(ICO_HIERBA,1,150,104)},
     {ST(ICO_LUNA,3,120,22),
      ORB(ICO_HADA,2,120,70,64,14,1,0),
      ORB(ICO_HADA,2,120,70,64,14,1,20),
      ORB(ICO_HADA,2,120,70,64,14,1,40),
      ORB(ICO_HADA,2,120,70,64,14,1,60),
      ORB(ICO_HADA,2,120,70,64,14,1,80),
      STAR(36,40,0),
      STAR(206,44,40),
      MOV(ICO_REGALO,2,120,80,120,60,62,90),
      APS(ICO_CUIDADOR,2,206,84,50)}},
    {0x0C1A2E, 0x1E3A3A, 0x1F4A30, 7, 8,
     {EL(120,114,300,44,0x143828), SI(ICO_ARBOLITO,4,14,62,0x08140F), SI(ICO_ARBOLITO,3,48,76,0x0C1E16), SI(ICO_ARBOLITO,4,228,60,0x08140F), SI(ICO_ARBOLITO,3,198,76,0x0C1E16), SIC(ICO_HONGO,1,100,106), SIC(ICO_HIERBA,1,150,104)},
     {MOW(ICO_CUIDADOR,3,24,78,214,78,2,98,3,16),
      MOW(ICO_FAROL,2,38,78,228,78,2,98,3,16),
      ORBF(ICO_LUMBRE,2,170,50,26,12,1,0,6),
      ORBF(ICO_LUMBRE,2,60,44,20,12,2,40,5),
      ORBF(ICO_LUMBRE,2,110,66,18,10,2,70,7),
      STAR(120,18,0),
      STAR(190,26,50),
      STAR(40,24,20)}},
    {0x6A7A9A, 0x9AAAC4, 0x3A6AA0, 7, 11,
     {EL(26,100,100,32,0x5A8A5A), EL(222,100,90,28,0x5A8A5A), SIC(ICO_HIERBA,1,18,88), SIC(ICO_HIERBA,1,230,90), REO(70,104,28,2,0xFFFFFF,120), REO(150,110,34,2,0xFFFFFF,100), REO(196,102,22,2,0xFFFFFF,110)},
     {MOV(ICO_NUBE,4,40,26,120,26,2,60),
      MOV(ICO_LAGRIMA,2,100,42,100,92,10,22),
      MOV(ICO_LAGRIMA,2,116,42,116,92,16,28),
      MOV(ICO_LAGRIMA,2,132,42,132,92,22,34),
      MOV(ICO_LAGRIMA,2,108,42,108,92,32,44),
      MOV(ICO_LAGRIMA,2,124,42,124,92,38,50),
      GRO(ICO_CHARCO,5,118,100,12,56,160),
      ST(ICO_CUIDADOR,2,40,78),
      GRO(ICO_NUBE,2,150,102,58,76,100),
      SP(ICO_ESTRELLA,2,190,34,62,74,88),
      SP(ICO_CORAZON,2,70,52,66,76,92)}},
    {0x7AB8E0, 0xB8E0F0, 0x2F6AA0, 7, 8,
     {EL(26,100,100,32,0x5A8A5A), EL(222,100,90,28,0x5A8A5A), SIC(ICO_HIERBA,1,18,88), SIC(ICO_HIERBA,1,230,90), REO(70,104,28,2,0xFFFFFF,120), REO(150,110,34,2,0xFFFFFF,100), REO(196,102,22,2,0xFFFFFF,110)},
     {ST(ICO_CUIDADOR,2,30,78),
      MOW(ICO_PEZ_DORADO,3,40,86,190,80,4,96,4,4),
      MOW(ICO_BURBUJA,2,60,100,66,70,8,28,0,0),
      MOW(ICO_BURBUJA,2,120,100,114,66,22,46,0,0),
      MOW(ICO_BURBUJA,2,160,100,166,70,36,60,0,0),
      SP(ICO_CORAZON,2,50,52,40,50,62),
      SP(ICO_ESTRELLA,2,196,30,70,78,90),
      NUB(ICO_NUBE,2,24,20)}},
    {0x1E2A52, 0x3A4A7A, 0x2F6AA0, 7, 12,
     {EL(26,100,100,32,0x5A8A5A), EL(222,100,90,28,0x5A8A5A), SIC(ICO_HIERBA,1,18,88), SIC(ICO_HIERBA,1,230,90), REO(70,104,28,2,0xFFFFFF,120), REO(150,110,34,2,0xFFFFFF,100), REO(196,102,22,2,0xFFFFFF,110)},
     {ORB(ICO_NENUFAR,3,60,90,0,3,3,0),
      ORB(ICO_NENUFAR,3,130,94,0,3,3,30),
      ORB(ICO_NENUFAR,3,196,90,0,3,3,60),
      ORB(ICO_RANA,2,60,76,0,3,3,0),
      ORB(ICO_RANA,2,130,80,0,3,3,30),
      ORB(ICO_RANA,2,196,76,0,3,3,60),
      MOW(ICO_ZETA,2,70,60,84,30,10,40,0,0),
      MOW(ICO_ZETA,2,136,64,150,34,30,60,0,0),
      MOW(ICO_ZETA,2,202,60,216,30,50,80,0,0),
      ST(ICO_LUNA,3,36,24),
      STAR(100,22,0),
      STAR(160,18,50)}},
    {0x0C1236, 0x1E2A5E, 0x2F5A9A, 7, 11,
     {EL(26,100,100,32,0x5A8A5A), EL(222,100,90,28,0x5A8A5A), SIC(ICO_HIERBA,1,18,88), SIC(ICO_HIERBA,1,230,90), REO(70,104,28,2,0xFFFFFF,120), REO(150,110,34,2,0xFFFFFF,100), REO(196,102,22,2,0xFFFFFF,110)},
     {ST(ICO_LUNA,3,36,24),
      STAR(50,98,0),
      STAR(90,106,20),
      STAR(130,98,40),
      STAR(170,106,60),
      STAR(210,98,80),
      MOV(ICO_MANO_DESLIZA,3,40,88,214,88,6,86),
      MOV(ICO_ESTRELLA_FUGAZ,2,200,22,110,50,50,74),
      ST(ICO_CUIDADOR,2,24,78),
      STAR(100,20,30),
      STAR(180,28,10)}},
    {0x7A8AB0, 0xB8C4D8, 0x3A6AA0, 7, 9,
     {EL(26,100,100,32,0x5A8A5A), EL(222,100,90,28,0x5A8A5A), SIC(ICO_HIERBA,1,18,88), SIC(ICO_HIERBA,1,230,90), REO(70,104,28,2,0xFFFFFF,120), REO(150,110,34,2,0xFFFFFF,100), REO(196,102,22,2,0xFFFFFF,110)},
     {ST(ICO_CUIDADOR,3,80,76),
      ST(ICO_ONDINA,3,150,78),
      ORB(ICO_CONCHA,2,112,54,10,6,3,0),
      MOV(ICO_LAGRIMA,2,70,40,70,70,6,20),
      SP(ICO_CORAZON,2,116,36,40,52,66),
      SP(ICO_CARA_FELIZ,2,86,40,60,70,86),
      NUB(ICO_NUBE,3,20,0),
      STAR(206,28,0),
      APS(ICO_CONCHA,2,206,90,10)}},
    {0x8FC8F0, 0xD8EEF8, 0x7AA85A, 5, 8,
     {EL(120,140,330,120,0x86B85E), EL(30,118,170,60,0x78AA55), SIC(ICO_FLOR,1,60,102), SI(ICO_ARBOLITO,2,224,86,0x4A7A3A), SIC(ICO_HIERBA,1,150,104)},
     {GRO(ICO_NUBE,4,130,26,2,40,130),
      NUB(ICO_NUBE,2,48,60),
      SPIN(ICO_VIENTO,3,176,60,1),
      SPIN(ICO_VIENTO,2,92,40,-1),
      MOW(ICO_HOJA,2,20,84,250,70,30,92,6,3),
      MOW(ICO_HOJA,1,10,64,250,50,38,96,6,3),
      ST(ICO_CUIDADOR,3,50,76),
      ST(ICO_SOL,3,214,22)}},
    {0xF0A868, 0xF8D8A0, 0x7AA85A, 5, 10,
     {EL(120,140,330,120,0x86B85E), EL(30,118,170,60,0x78AA55), SIC(ICO_FLOR,1,60,102), SI(ICO_ARBOLITO,2,224,86,0x4A7A3A), SIC(ICO_HIERBA,1,150,104)},
     {ST(ICO_SOL,4,40,70),
      ST(ICO_PIEDRA,6,126,72),
      MOW(ICO_ZETA,2,132,50,150,26,6,40,0,0),
      MOW(ICO_ZETA,2,146,44,166,20,26,60,0,0),
      MOW(ICO_ZETA,2,138,48,160,22,46,80,0,0),
      MOV(ICO_CUIDADOR,3,206,80,150,70,4,30),
      MOV(ICO_BUFANDA,2,150,70,128,58,30,40),
      ST(ICO_BUFANDA,2,128,58),
      NUB(ICO_NUBE,2,24,100),
      STAR(190,20,0)}},
    {0x8FC8F0, 0xD8EEF8, 0x7AA85A, 5, 8,
     {EL(120,140,330,120,0x86B85E), EL(30,118,170,60,0x78AA55), SIC(ICO_FLOR,1,60,102), SI(ICO_ARBOLITO,2,224,86,0x4A7A3A), SIC(ICO_HIERBA,1,150,104)},
     {ST(ICO_CUIDADOR,3,40,78),
      MOV(ICO_COMETA,3,60,60,86,10,8,44),
      MOV(ICO_VOLANTIN,3,64,64,150,6,20,56),
      MOV(ICO_COMETA,2,58,66,200,18,34,70),
      NUB(ICO_NUBE,3,26,0),
      MOW(ICO_REGALO,2,150,14,140,80,56,84,0,0),
      SPIN(ICO_VIENTO,2,190,70,1),
      SP(ICO_CORAZON,2,70,40,70,80,92)}},
    {0x8FC8F0, 0xD8EEF8, 0x7AA85A, 5, 9,
     {EL(120,140,330,120,0x86B85E), EL(30,118,170,60,0x78AA55), SIC(ICO_FLOR,1,60,102), SI(ICO_ARBOLITO,2,224,86,0x4A7A3A), SIC(ICO_HIERBA,1,150,104)},
     {MOW(ICO_VENTOLIN,3,60,10,70,86,6,34,0,0),
      APS(ICO_HOJAS,3,74,94,34),
      MOW(ICO_VENTOLIN,3,70,86,166,24,46,86,3,5),
      SPIN(ICO_VIENTO,2,150,60,1),
      ST(ICO_CUIDADOR,3,200,80),
      APS(ICO_MANO,2,180,70,40),
      MOW(ICO_HOJAS,2,100,10,130,90,10,34,4,3),
      NUB(ICO_NUBE,2,24,40),
      ST(ICO_SOL,3,214,22)}},
    {0xF2C890, 0xFCE8C0, 0x7AA85A, 5, 9,
     {EL(120,140,330,120,0x86B85E), EL(30,118,170,60,0x78AA55), SIC(ICO_FLOR,1,60,102), SI(ICO_ARBOLITO,2,224,86,0x4A7A3A), SIC(ICO_HIERBA,1,150,104)},
     {ST(ICO_CUIDADOR,3,120,56),
      SPIN(ICO_VIENTO,3,60,60,1),
      SPIN(ICO_VIENTO,2,190,50,-1),
      MOW(ICO_HOJA,2,110,50,260,40,20,90,6,3),
      MOW(ICO_HOJA,2,120,70,260,60,28,96,6,3),
      MOW(ICO_CORAZON,2,130,54,250,24,40,90,4,2),
      NUB(ICO_NUBE,2,30,60),
      ST(ICO_SOL,3,206,24),
      APS(ICO_NUBE,2,150,40,10)}},
    {0x1A1030, 0x3A2A5A, 0x2A1E44, 6, 8,
     {RE(120,6,260,22,0x120A24), SI(ICO_CRISTAL,4,14,88,0x160C2C), SI(ICO_CRISTAL,3,38,98,0x1A1034), SI(ICO_CRISTAL,4,226,90,0x160C2C), SI(ICO_CRISTAL,3,202,100,0x1A1034), EL(120,114,300,40,0x1C1236)},
     {ORBF(ICO_CRISTAL,3,50,68,0,3,2,0,3),
      ORBF(ICO_CRISTAL,4,120,62,0,3,2,30,2),
      ORBF(ICO_CRISTAL,3,190,70,0,3,2,60,3),
      ST(ICO_CUIDADOR,2,90,84),
      MOW(ICO_NOTA,2,56,50,62,14,6,40,6,2),
      MOW(ICO_NOTA,2,124,44,130,12,26,60,6,2),
      MOW(ICO_NOTA,2,190,52,184,14,46,80,6,2),
      MOW(ICO_NOTA,2,86,60,90,26,60,90,6,2)}},
    {0x0E0A1E, 0x241A44, 0x1C3A5A, 6, 8,
     {RE(120,6,260,22,0x120A24), SI(ICO_CRISTAL,4,14,88,0x160C2C), SI(ICO_CRISTAL,3,38,98,0x1A1034), SI(ICO_CRISTAL,4,226,90,0x160C2C), SI(ICO_CRISTAL,3,202,100,0x1A1034), EL(120,114,300,40,0x1C1236)},
     {MOV(ICO_GOTA,2,10,100,230,98,4,40),
      MOV(ICO_CRISTAL,2,10,96,230,94,24,60),
      MOV(ICO_PIEDRA,2,10,100,230,100,44,84),
      ST(ICO_CUIDADOR,2,40,64),
      MOW(ICO_NOTA,2,70,70,76,36,10,36,5,2),
      MOW(ICO_NOTA,2,160,70,154,36,40,66,5,2),
      ORBF(ICO_CRISTAL,3,204,44,0,3,2,0,3),
      STAR(120,40,0)}},
    {0x1A1030, 0x34285A, 0x2A1E44, 6, 7,
     {RE(120,6,260,22,0x120A24), SI(ICO_CRISTAL,4,14,88,0x160C2C), SI(ICO_CRISTAL,3,38,98,0x1A1034), SI(ICO_CRISTAL,4,226,90,0x160C2C), SI(ICO_CRISTAL,3,202,100,0x1A1034), EL(120,114,300,40,0x1C1236)},
     {MOW(ICO_GOTA,2,100,24,100,86,2,22,0,0),
      MOW(ICO_GOTA,2,100,24,100,86,34,54,0,0),
      GRO(ICO_CRISTAL,4,100,82,4,92,150),
      ST(ICO_CUIDADOR,3,190,76),
      ORBF(ICO_RELOJ,2,150,40,0,3,2,0,2),
      ORBF(ICO_CRISTAL,2,40,74,0,3,2,20,3),
      ORBF(ICO_CRISTAL,2,214,60,0,3,2,50,2)}},
    {0x0A0818, 0x1E1440, 0x2A1E44, 6, 8,
     {RE(120,6,260,22,0x120A24), SI(ICO_CRISTAL,4,14,88,0x160C2C), SI(ICO_CRISTAL,3,38,98,0x1A1034), SI(ICO_CRISTAL,4,226,90,0x160C2C), SI(ICO_CRISTAL,3,202,100,0x1A1034), EL(120,114,300,40,0x1C1236)},
     {ORB(ICO_MURCIELAGO,3,110,52,30,14,1,0),
      ORBF(ICO_CRISTAL,3,40,42,0,3,2,0,3),
      ORBF(ICO_CRISTAL,2,110,20,0,3,2,16,3),
      ORBF(ICO_CRISTAL,3,190,46,0,3,2,33,3),
      ORBF(ICO_CRISTAL,2,210,84,0,3,2,50,3),
      ORBF(ICO_CRISTAL,2,40,88,0,3,2,66,3),
      ST(ICO_CUIDADOR,2,130,84),
      APS(ICO_CORAZON,2,120,32,60)}},
    {0x1A1030, 0x34285A, 0x2A1E44, 6, 9,
     {RE(120,6,260,22,0x120A24), SI(ICO_CRISTAL,4,14,88,0x160C2C), SI(ICO_CRISTAL,3,38,98,0x1A1034), SI(ICO_CRISTAL,4,226,90,0x160C2C), SI(ICO_CRISTAL,3,202,100,0x1A1034), EL(120,114,300,40,0x1C1236)},
     {ST(ICO_CUIDADOR,3,60,76),
      MOW(ICO_NOTA,2,96,62,100,24,4,40,6,2),
      MOW(ICO_NOTA,2,126,62,120,24,22,58,6,2),
      MOW(ICO_NOTA,2,106,62,112,24,38,76,6,2),
      ST(ICO_CRISTALINO,3,152,80),
      ORBF(ICO_CRISTAL,3,196,66,0,3,2,0,3),
      ORBF(ICO_CRISTAL,2,216,84,0,3,2,40,3),
      ORBF(ICO_CRISTAL,2,176,40,0,3,2,70,3),
      APS(ICO_CORAZON,2,152,46,70)}},
    {0x7AC8E8, 0xC8F0F4, 0x2F8AA8, 5, 12,
     {REO(120,70,240,2,0xFFFFFF,90), EL(120,124,300,30,0x3AA8C0), REO(60,108,30,2,0xFFFFFF,120), REO(170,112,38,2,0xFFFFFF,110), REO(120,102,24,2,0xFFFFFF,100)},
     {NUB(ICO_NUBE,3,20,0),
      ORB(ICO_TORTUGA,5,120,82,0,3,2,0),
      ST(ICO_ARBOLITO,3,92,56),
      ST(ICO_FARO,3,152,46),
      APS(ICO_PIEDRA,1,106,64,10),
      APS(ICO_PIEDRA,1,116,64,16),
      APS(ICO_PIEDRA,1,126,64,22),
      APS(ICO_PIEDRA,1,136,64,28),
      ST(ICO_SOL,3,210,22),
      APS(ICO_FLOR,1,76,66,40),
      APS(ICO_FLOR,1,170,66,46),
      MOW(ICO_GOTA,2,40,102,40,92,2,98,0,0)}},
    {0x0C1A3E, 0x23386A, 0x1F5A7A, 5, 10,
     {REO(120,70,240,2,0xFFFFFF,90), EL(120,124,300,30,0x3AA8C0), REO(60,108,30,2,0xFFFFFF,120), REO(170,112,38,2,0xFFFFFF,110), REO(120,102,24,2,0xFFFFFF,100)},
     {GRO(ICO_FARO,5,120,56,6,70,100),
      MOW(ICO_BARQUITO_HOJA,2,10,100,120,100,4,60,3,3),
      MOW(ICO_BARQUITO_HOJA,2,230,104,126,102,20,80,3,3),
      MOW(ICO_PIEDRA,2,30,98,110,80,12,40,0,0),
      MOW(ICO_PIEDRA,2,210,100,128,76,30,56,0,0),
      ST(ICO_LUNA,3,200,22),
      STAR(36,24,0),
      STAR(80,40,40),
      STAR(170,14,70),
      APS(ICO_ESTRELLA,3,120,16,72)}},
    {0x7AC8E8, 0xC8F0F4, 0x2F8AA8, 5, 8,
     {REO(120,70,240,2,0xFFFFFF,90), EL(120,124,300,30,0x3AA8C0), REO(60,108,30,2,0xFFFFFF,120), REO(170,112,38,2,0xFFFFFF,110), REO(120,102,24,2,0xFFFFFF,100)},
     {GRO(ICO_CONCHA,5,120,66,4,40,110),
      MOW(ICO_NOTA,2,86,46,80,18,30,56,6,2),
      MOW(ICO_NOTA,2,152,46,160,18,44,70,6,2),
      MOW(ICO_CORAZON,2,120,40,120,10,60,92,4,2),
      ST(ICO_CUIDADOR,2,44,82),
      MOW(ICO_GOTA,2,210,104,210,92,2,98,0,0),
      NUB(ICO_NUBE,2,24,40),
      ST(ICO_SOL,3,210,22)}},
    {0x7AC8E8, 0xE4F8F8, 0x2F8AA8, 5, 8,
     {REO(120,70,240,2,0xFFFFFF,90), EL(120,124,300,30,0x3AA8C0), REO(60,108,30,2,0xFFFFFF,120), REO(170,112,38,2,0xFFFFFF,110), REO(120,102,24,2,0xFFFFFF,100)},
     {NUB(ICO_NUBE,6,34,0),
      MOW(ICO_NUBE,3,60,10,100,56,40,92,3,2),
      ORB(ICO_BARQUITO_HOJA,2,60,100,0,3,3,0),
      ST(ICO_SOL,3,208,20),
      MOW(ICO_CORAZON,2,130,80,134,46,52,80,4,2),
      ST(ICO_MAREA,3,184,80),
      ST(ICO_CUIDADOR,2,36,82),
      APS(ICO_SALUDO,2,150,64,20)}},
    {0xF0A070, 0xFCD8A0, 0x2F8AA8, 5, 9,
     {REO(120,70,240,2,0xFFFFFF,90), EL(120,124,300,30,0x3AA8C0), REO(60,108,30,2,0xFFFFFF,120), REO(170,112,38,2,0xFFFFFF,110), REO(120,102,24,2,0xFFFFFF,100)},
     {ST(ICO_FARO,4,70,56),
      ORBF(ICO_ESTRELLA,2,70,24,0,3,2,0,4),
      ST(ICO_CUIDADOR,2,110,78),
      ST(ICO_MAREA,2,150,82),
      ORB(ICO_BARQUITO_HOJA,2,190,100,0,3,3,0),
      ST(ICO_SOL,3,206,66),
      STAR(170,26,30),
      SP(ICO_CORAZON,2,130,50,50,60,72),
      SP(ICO_PERSONA,2,96,40,76,82,96)}},
    /* Nimba 1: el anillo de hadas, visto desde arriba */
    {0x1E2A52, 0x3A4A7A, 0x3E7A4E, 4, 7,
     {EL(36,106,160,40,0x2E6A44), EL(196,108,180,40,0x2A5E3E), REO(120,100,120,10,0xFFF6C8,60), SIC(ICO_HONGO,1,70,102)},
     {NUB(ICO_NIMBA,6,30,0), ST(ICO_LUNA,3,206,22),
      ORBF(ICO_HADA,2,120,88,52,6,2,0,4), ORBF(ICO_HADA,2,120,88,52,6,2,50,4),
      ORBF(ICO_ESTRELLA,1,120,88,52,6,2,25,5), ST(ICO_CUIDADOR,2,40,84), APS(ICO_CORAZON,2,40,62,60)}},
    /* Nimba 2: la cometa suelta */
    {0x1E2A52, 0x3A4A7A, 0x3E7A4E, 3, 6,
     {EL(36,106,160,40,0x2E6A44), EL(196,108,180,40,0x2A5E3E), SI(ICO_ARBOLITO,2,214,80,0x1E4A30)},
     {ST(ICO_CUIDADOR,3,40,80), MOV(ICO_COMETA,3,60,70,150,14,10,60), NUB(ICO_NIMBA,6,34,40),
      APS(ICO_MANO,1,56,66,8), STAR(180,30,20), STAR(90,16,60)}},
    /* Nimba 3: el reflejo del lago */
    {0x1E2A52, 0x2F4A86, 0x1F5A8A, 3, 6,
     {REO(120,70,240,2,0xFFFFFF,90), EL(120,124,300,30,0x2A6AA0), REO(120,96,100,2,0xFFFFFF,100)},
     {NUB(ICO_NIMBA,6,30,0), APS(ICO_NIMBA,4,120,86,30), ST(ICO_LUNA,3,206,22),
      ST(ICO_CUIDADOR,2,40,66), APS(ICO_ESTRELLA,3,120,90,50), APS(ICO_CORAZON,2,120,70,70)}},
    /* Nimba 4: el dragón de cristal dormido bajo la cueva */
    {0x14183A, 0x2A2A66, 0x2A3A5A, 3, 8,
     {EL(36,106,160,40,0x2A3A5A), EL(196,108,180,40,0x24324E), SIC(ICO_CRISTAL,2,28,100)},
     {NUB(ICO_NIMBA,6,26,0), ST(ICO_CRISTALINO,3,120,84),
      ORBF(ICO_CRISTAL,3,70,78,0,3,2,0,3), ORBF(ICO_CRISTAL,3,176,74,0,3,2,40,3), ORBF(ICO_CRISTAL,2,120,56,0,3,2,70,3),
      MOW(ICO_NOTA,2,100,60,90,20,20,70,6,2), MOW(ICO_NOTA,2,140,60,150,20,36,86,6,2), APS(ICO_ESTRELLA,2,120,100,60)}},
    /* Nimba 5: el último paseo del Rey (arriba, el cielo estrellado del principio) */
    {0x1E2A52, 0x3A4A7A, 0x3E7A4E, 2, 7,
     {EL(36,110,160,40,0x2E6A44), EL(196,112,180,40,0x2A5E3E)},
     {NUB(ICO_NIMBA,6,70,0), MOV(ICO_CUIDADOR,3,30,56,200,56,6,94), ST(ICO_LUNA,3,206,24),
      STAR(40,22,0), STAR(100,40,30), STAR(170,18,60), APS(ICO_CORAZON,2,200,40,80)}},
};

/* el cielo, el fondo y los dibujos de la historia (c = criatura, k = cuál); el guion se repite cada diez segundos */

static void amb(act_t a) { if (s_amb_n < 28) s_amb[s_amb_n++] = a; }
/* lo que le da vida a cada viñeta, según el lugar de la criatura y si es de día o de noche */
static void esc_ambiente(const vineta_t *e, int c, int k)
{
    s_amb_n = 0;
    const int r = (int)((e->cielo >> 16) & 255), g = (int)((e->cielo >> 8) & 255), b = (int)(e->cielo & 255);
    const bool noche = (r * 3 + g * 6 + b) / 10 < 110;
    const int ph = (c * 37 + k * 23) % 100;
    if (c != 4) {
        if (noche) {
            for (int i = 0; i < 6; i++) amb((act_t)STAR(10 + (i * 41 + ph * 3) % 215, 6 + (i * 17 + ph) % 26, (i * 19 + ph) % 100));
            amb((act_t)MOV(ICO_ESTRELLA_FUGAZ, 1, 210 - ph % 40, 8, 120 - ph % 30, 44, 8 + ph % 40, 24 + ph % 40));
        } else {
            amb((act_t)NUB(ICO_NUBE, 2, 14 + ph % 14, ph));
            amb((act_t)NUB(ICO_NUBE, 1, 30 + (ph * 3) % 12, (ph + 50) % 100));
            amb((act_t)MOW(ICO_GOLONDRINA, 1, -20, 22 + ph % 20, 260, 14 + (ph * 2) % 24, 6 + ph % 20, 96, 4, 3));
        }
    }
    switch (c) {
    case 0: /* prado */
        if (!noche) { amb((act_t)MOW(ICO_ABEJA, 1, -10, 80, 250, 60, 4 + ph % 20, 94, 8, 5)); amb((act_t)MOW(ICO_MARIPOSA, 1, 250, 90, -10, 70, 14 + ph % 20, 98, 10, 4)); }
        amb((act_t)ST(ICO_FLOR, 1, 8 + ph % 6, 106)); amb((act_t)ST(ICO_FLOR, 1, 228 - ph % 6, 108));
        break;
    case 1: /* bosque */
        if (noche) { amb((act_t)ST(ICO_HONGO_LUZ, 1, 12, 106)); amb((act_t)ST(ICO_HONGO_LUZ, 1, 226, 108)); }
        else amb((act_t)MOW(ICO_MARIPOSA, 1, -10, 80, 250, 56, 6 + ph % 20, 96, 10, 4));
        break;
    case 2: /* lago */
        amb((act_t)ST(ICO_NENUFAR, 1, 10, 108)); amb((act_t)ST(ICO_NENUFAR, 1, 226, 110));
        amb((act_t)MOV(ICO_PEZ_DORADO, 1, 150 + ph % 40, 102, 176 + ph % 40, 80, 40 + ph % 20, 54 + ph % 20));
        if (!noche) amb((act_t)MOW(ICO_LIBELULA, 1, 250, 70, -10, 56, 10 + ph % 20, 94, 6, 6));
        break;
    case 3: /* colina */
        amb((act_t)MOW(ICO_HOJA, 1, -10, 70, 250, 40, 6 + ph % 20, 80, 8, 3)); amb((act_t)MOW(ICO_HOJA, 1, -10, 90, 250, 70, 22 + ph % 20, 94, 10, 2));
        amb((act_t)MOW(ICO_HOJA, 1, -10, 50, 250, 30, 40 + ph % 20, 98, 6, 3));
        break;
    case 4: /* cueva */
        amb((act_t)CRIS(ICO_CRIS_0, 1, 14, 104, 5, 0)); amb((act_t)CRIS(ICO_CRIS_3, 1, 226, 106, 6, 40)); amb((act_t)CRIS(ICO_CRIS_5, 1, 38, 108, 5, 70));
        amb((act_t)MOV(ICO_GOTA, 1, 40 + ph % 40, 8, 40 + ph % 40, 96, 6 + ph % 30, 24 + ph % 30)); amb((act_t)MOV(ICO_GOTA, 1, 170 + ph % 40, 8, 170 + ph % 40, 98, 50 + ph % 30, 68 + ph % 20));
        amb((act_t)MOW(ICO_MURCIELAGO, 1, -20, 22, 260, 40, 20 + ph % 40, 92, 6, 5));
        break;
    case 5: /* isla */
        amb((act_t)MOW(ICO_GOLONDRINA, 1, 250, 40, -10, 30, 10 + ph % 20, 94, 5, 3));
        amb((act_t)MOV(ICO_BURBUJA, 1, 20, 112, 24, 86, 20 + ph % 20, 50 + ph % 20)); amb((act_t)MOV(ICO_BURBUJA, 1, 210, 112, 206, 90, 50 + ph % 20, 80 + ph % 15));
        break;
    }
    for (int i = 0; i < 3; i++) /* luces chicas que flotan: luciérnagas de noche, polen de día */
        amb((act_t)ORBF(ICO_ESTRELLA, 1, 40 + (i * 70 + ph) % 170, 50 + (i * 23 + ph) % 40, 14 + i * 4, 8 + i * 3, 2 + i, (ph + i * 33) % 100, 5 + i));
    if (c != 4) { amb((act_t)ST(ICO_HIERBA, 1, 6, 112)); amb((act_t)ST(ICO_HIERBA, 1, 234, 112)); }
}
static lv_obj_t *escena_construir(lv_obj_t *padre, int x0, int y0, const vineta_t *e, int k, int z, int ew, int eh, int suelo_y, bool una_vez)
{
    s_esc_z = z;
    lv_obj_t *v = lv_obj_create(padre);
    lv_obj_remove_style_all(v);
    lv_obj_set_size(v, ew, eh);
    lv_obj_set_pos(v, x0, y0);
    /* el marco dorado se dibuja DESPUÉS de lo que hay adentro (border_post): antes el cielo, el suelo, el velo estrellado y las nubes que entran
     * por un lado lo tapaban y la viñeta quedaba abierta. Con esquinas rectas, porque lo de adentro se recorta en rectángulo y sobresalía de las redondeadas. */
    lv_obj_set_style_radius(v, z > 1 ? LV_RADIUS_CIRCLE : 0, 0);
    if (z == 1) lv_obj_set_style_border_post(v, true, 0);
    /* sin recorte redondeado (clip_corner): cada recorte obliga a dibujar una capa extra por cuadro y la placa va justa */
    lv_obj_set_style_bg_color(v, lv_color_hex(e->cielo), 0);
    lv_obj_set_style_bg_grad_color(v, lv_color_hex(e->fondo), 0);
    lv_obj_set_style_bg_grad_dir(v, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_bg_opa(v, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(v, 2, 0);
    lv_obj_set_style_border_color(v, lv_color_hex(0xD0B969), 0);
    lv_obj_remove_flag(v, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(v, LV_OBJ_FLAG_SCROLLABLE);
    for (int i = 0; i < e->ns; i++) { /* colinas, siluetas y detalles del fondo */
        const shape_t *s = &e->sh[i];
        if (s->k == SH_IC) {
            lv_obj_t *o = icono(v, s->ico, z > 1 ? (s->esc ? s->esc : 2) * z : (s->esc ? s->esc : 2) + 1);
            lv_obj_update_layout(o);
            lv_obj_set_pos(o, s->x * z - (int)lv_obj_get_width(o) / 2, s->y * z - (int)lv_obj_get_height(o) / 2);
            lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE);
            if (s->col != 0xFFFFFFFFu) { lv_obj_set_style_image_recolor(o, lv_color_hex(s->col), 0); lv_obj_set_style_image_recolor_opa(o, LV_OPA_COVER, 0); }
        } else {
            lv_obj_t *o = lv_obj_create(v);
            lv_obj_remove_style_all(o);
            lv_obj_set_size(o, s->w * z, s->h * z);
            lv_obj_set_pos(o, (s->x - s->w / 2) * z, (s->y - s->h / 2) * z);
            lv_obj_set_style_bg_color(o, lv_color_hex(s->col), 0);
            lv_obj_set_style_bg_opa(o, s->opa, 0);
            lv_obj_set_style_radius(o, s->k == SH_EL ? LV_RADIUS_CIRCLE : 4, 0);
            lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE);
        }
        if (i == 0) { /* la franja del suelo va justo después del primer relieve, para tapar sus pies */ }
    }
    lv_obj_t *su = lv_obj_create(v);
    lv_obj_remove_style_all(su);
    lv_obj_set_size(su, ew, eh - suelo_y * z + 6 * z);
    lv_obj_set_pos(su, 0, (suelo_y + 8) * z);
    lv_obj_set_style_bg_color(su, lv_color_hex(e->suelo), 0);
    lv_obj_set_style_bg_opa(su, LV_OPA_COVER, 0);
    lv_obj_remove_flag(su, LV_OBJ_FLAG_CLICKABLE);
    for (int i = 0; i < MAX_AC; i++) s_esc_o[i] = NULL;
    s_amb_n = 0;
    if (z == 1) esc_ambiente(e, (int)(e - ESCENAS) / 5, k);
    for (int i = 0; i < e->na + s_amb_n; i++) {
        const act_t *a = i < e->na ? &e->a[i] : &s_amb[i - e->na];
        if (!a->n) continue; /* (casilla vacía) */
        lv_obj_t *o = icono(v, a->ico, z > 1 ? (a->esc ? a->esc : 2) * z : (a->esc ? a->esc : 2) + 1);
        lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_update_layout(o);
        const int w = (int)lv_obj_get_width(o), h = (int)lv_obj_get_height(o);
        lv_obj_set_size(o, w, h); /* el tamaño no cambia al escalar: se escala desde el centro */
        lv_image_set_pivot(o, w / 2, h / 2);
        lv_obj_set_style_opa(o, LV_OPA_TRANSP, 0);
        s_esc_o[i] = o; s_esc_w[i] = (int16_t)w; s_esc_h[i] = (int16_t)h;
    }
    s_esc_velo = s_esc_senal = s_esc_fugaz = NULL;
    if (k == 4 && z == 1) { /* el cielo estrellado del principio */
        lv_obj_t *ve = lv_obj_create(v);
        lv_obj_remove_style_all(ve);
        lv_obj_set_size(ve, ESC_W, ESC_H);
        lv_obj_set_style_bg_color(ve, lv_color_hex(0x0B1030), 0);
        lv_obj_set_style_bg_grad_color(ve, lv_color_hex(0x2A2A66), 0);
        lv_obj_set_style_bg_grad_dir(ve, LV_GRAD_DIR_VER, 0);
        lv_obj_set_style_bg_opa(ve, LV_OPA_COVER, 0);
        lv_obj_remove_flag(ve, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_remove_flag(ve, LV_OBJ_FLAG_SCROLLABLE);
        static const int16_t ES[][3] = {{14,16,2},{38,60,2},{52,22,3},{74,84,2},{92,12,2},{150,70,2},{176,26,3},{198,52,2},{222,88,2},{30,96,2},{128,100,2},{210,14,2},{66,46,2},{188,92,2},{108,30,2}};
        for (unsigned q = 0; q < sizeof ES / sizeof ES[0]; q++) {
            lv_obj_t *st = lv_obj_create(ve);
            lv_obj_remove_style_all(st);
            lv_obj_set_size(st, ES[q][2], ES[q][2]);
            lv_obj_set_pos(st, ES[q][0], ES[q][1]);
            lv_obj_set_style_bg_color(st, lv_color_hex(q % 3 ? 0xFFF6C8 : 0xCFE0FF), 0);
            lv_obj_set_style_bg_opa(st, LV_OPA_COVER, 0);
            lv_obj_remove_flag(st, LV_OBJ_FLAG_CLICKABLE);
        }
        s_esc_senal = icono(ve, ICO_ESTRELLA, 3); /* la señal: una estrella que late, un poco más grande que las otras */
        lv_obj_update_layout(s_esc_senal);
        lv_obj_set_pos(s_esc_senal, ESC_W / 2 - (int)lv_obj_get_width(s_esc_senal) / 2, 34);
        lv_obj_remove_flag(s_esc_senal, LV_OBJ_FLAG_CLICKABLE);
        s_esc_fugaz = lv_obj_create(ve);
        lv_obj_remove_style_all(s_esc_fugaz);
        lv_obj_set_size(s_esc_fugaz, 14, 3);
        lv_obj_set_style_radius(s_esc_fugaz, 2, 0);
        lv_obj_set_style_bg_color(s_esc_fugaz, lv_color_hex(0xFFF6C8), 0);
        lv_obj_set_style_bg_opa(s_esc_fugaz, LV_OPA_COVER, 0);
        lv_obj_remove_flag(s_esc_fugaz, LV_OBJ_FLAG_CLICKABLE);
        s_esc_velo = ve;
    }
    s_esc_v = e;
    lv_anim_t an;
    lv_anim_init(&an);
    lv_anim_set_var(&an, v);
    lv_anim_set_values(&an, 0, 1000);
    lv_anim_set_time(&an, ESC_PERIODO);
    lv_anim_set_repeat_count(&an, una_vez ? 1 : LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&an, lv_anim_path_linear);
    lv_anim_set_exec_cb(&an, esc_cb);
    lv_anim_start(&an);
    esc_cb(v, 0);
    return v;
}
static void escena_armar(lv_obj_t *padre, int x0, int y0, int c, int k)
{
    escena_construir(padre, x0, y0, &ESCENAS[c * 5 + k], k, 1, ESC_W, ESC_H, SUELO, false);
}

/* ======================================================================
 * Las cuatro cosas que dejó el Cuidador: al descubrir cada secreto se ve una escena a pantalla completa (unos diez segundos,
 * con más detalle y movimiento que las de las historias) y al terminar se vuelve al juego. Espacio de 233x233, ampliado al doble.
 * ==================================================================== */
#define GH(x, y, t0, t1, o) {ICO_CUIDADOR, 3, 0, 2, 6, 0, 0, 0, 5, {K(0, x, y, 100, 0), K(t0, x, y, 100, 0), K((t0) + 8, x, y, 100, o), K(t1, x, y, 100, o), K((t1) + 6, x, y, 100, 0)}}
#define CST(x, y, ph) {ICO_ESTRELLA, 1, 0, 0, 0, ph, 0, 3, 2, {K(0, x, y, 70, 255), K(100, x, y, 70, 255)}}
static const int CINE_SUELO[4] = {160, 400, 150, 190};
static const vineta_t CINES[4] = {
    /* 0 · el anillo de hadas: la cuchara de palo del Cuidador entre el musgo, las hadas bailan alrededor */
    {0x0B1A2E, 0x1F4A3A, 0x2E7A4A, MAX_SH, MAX_AC,
     {ELO(116,152,210,62,0x5CFFA0,50), ELO(116,152,150,44,0x9CFFB8,90), EL(36,172,170,50,0x2A6A44), EL(198,176,170,50,0x255E3C),
      SI(ICO_ARBOLITO,3,24,118,0x123A28), SI(ICO_ARBOLITO,3,212,114,0x123A28), SI(ICO_ARBOLITO,2,70,100,0x163E30), SI(ICO_ARBOLITO,2,168,96,0x163E30),
      SIC(ICO_HONGO_LUZ,2,60,176), SIC(ICO_HONGO_LUZ,2,176,180), SIC(ICO_HIERBA,2,38,186), SIC(ICO_FLOR,2,198,188), SIC(ICO_HIERBA,2,94,192)},
     {ST(ICO_LUNA,3,178,32), CST(30,44,0), CST(62,22,20), CST(96,40,40), CST(140,20,60), CST(202,70,80), CST(48,76,10), CST(130,54,30),
      GRO(ICO_FLOR,3,84,170,26,46,100), GRO(ICO_FLOR,3,150,174,32,52,100), GRO(ICO_FLOR,3,116,184,38,58,100),
      ST(ICO_RAMITA,3,116,150),
      ORBF(ICO_HADA,3,116,128,54,20,2,0,4), ORBF(ICO_HADA,3,116,134,66,26,2,33,5), ORBF(ICO_HADA,2,116,124,40,16,3,66,6),
      ORBF(ICO_ESTRELLA,1,70,112,14,10,3,0,6), ORBF(ICO_ESTRELLA,1,164,102,16,10,2,40,5), ORBF(ICO_ESTRELLA,1,44,146,12,8,3,70,6), ORBF(ICO_ESTRELLA,1,192,142,14,9,2,20,5),
      SP(ICO_ESTRELLA,3,116,126,50,66,84), SP(ICO_ESTRELLA,2,98,138,56,68,86), SP(ICO_ESTRELLA,2,134,138,58,70,88),
      GH(186,128,58,90,150)}},
    /* 1 · el reflejo: las estrellas del lago dibujan una silueta sentada a la orilla y se apagan */
    {0x070B24, 0x1B2A5C, 0x153466, MAX_SH, MAX_AC,
     {ELO(116,126,300,10,0x6E88D8,120), EL(40,118,150,34,0x0E1A3E), EL(206,120,150,34,0x0E1A3E),
      RE(116,184,240,120,0x153466), ELO(116,142,150,12,0x4A66B8,150), ELO(116,172,120,14,0x2A4C90,150), ELO(116,194,170,20,0x1E3E7A,150), ELO(116,214,200,24,0x183268,130),
      EL(30,216,130,40,0x2A5A4A), SIC(ICO_NENUFAR,2,156,150), SIC(ICO_NENUFAR,2,188,186), SIC(ICO_NENUFAR,1,92,200), SIC(ICO_HIERBA,2,26,198)},
     {ST(ICO_LUNA,3,60,40), CST(100,22,0), CST(140,40,25), CST(180,26,50), CST(210,62,75), CST(26,70,15), CST(84,60,40), CST(160,70,60), CST(124,24,10),
      APS(ICO_ESTRELLA,1,118,160,10), APS(ICO_ESTRELLA,1,106,174,18), APS(ICO_ESTRELLA,1,130,174,26), APS(ICO_ESTRELLA,1,118,188,34), APS(ICO_ESTRELLA,1,102,200,42), APS(ICO_ESTRELLA,1,134,200,50), APS(ICO_ESTRELLA,1,118,208,56),
      MOV(ICO_ESTRELLA_FUGAZ,2,206,30,130,74,4,22), MOV(ICO_ESTRELLA_FUGAZ,2,188,20,100,64,40,58),
      MOV(ICO_PEZ_DORADO,2,150,176,176,150,66,80), SP(ICO_GOTA,2,176,150,70,76,82),
      ORBF(ICO_ESTRELLA,1,50,160,14,10,3,0,6), ORBF(ICO_ESTRELLA,1,190,152,14,8,2,50,5), ORBF(ICO_ESTRELLA,1,70,182,12,9,3,25,6),
      GH(48,170,60,92,150)}},
    /* 2 · la cometa: su última cometa, sin hilo, vuela sola y baja a descansar a la colina un momento */
    {0x4A3480, 0xFFAA6A, 0x2E5A3A, MAX_SH, MAX_AC,
     {ELO(116,146,104,104,0xFFE29A,150), EL(60,180,230,70,0x3F7A4A), EL(192,188,230,70,0x356A42), EL(116,232,330,80,0x2E5A3A),
      SI(ICO_ARBOLITO,3,204,140,0x244A30), SIC(ICO_FLOR,2,40,178), SIC(ICO_HIERBA,2,92,190), SIC(ICO_FLOR,2,160,194), SIC(ICO_GIRASOL,2,64,198), SIC(ICO_HIERBA,2,190,206)},
     {CST(30,30,0), CST(90,20,30), CST(170,26,60), CST(204,54,10),
      NUB(ICO_NUBE,3,40,0), NUB(ICO_NUBE,2,74,80), NUB(ICO_NUBE,3,104,40),
      MOW(ICO_GOLONDRINA,2,-20,64,260,30,5,95,6,3), MOW(ICO_GOLONDRINA,2,-40,84,250,50,10,98,5,4),
      MOV(ICO_ESTRELLA,1,50,164,176,52,10,46), MOV(ICO_ESTRELLA,1,48,162,176,50,12,48), MOV(ICO_ESTRELLA,1,50,166,176,54,14,50),
      {ICO_VOLANTIN,3,0,5,6,0,0,0,6,{K(0,40,172,70,0),K(8,50,160,100,255),K(42,176,48,100,255),K(70,122,140,100,255),K(88,122,142,100,255),K(99,200,10,100,0)}},
      MOW(ICO_HOJA,1,-10,150,250,120,10,80,10,3), MOW(ICO_HOJA,1,-10,170,250,40,20,90,12,2), MOW(ICO_HOJA,1,-10,190,250,60,30,94,8,3),
      GH(92,140,30,92,150)}},
    /* 3 · los cristales: un verso de su canción, tarareado entre los cristales, y la luz que despierta */
    {0x0E0820, 0x2A1A4A, 0x1C1232, MAX_SH, MAX_AC,
     {EL(116,36,300,130,0x1A1038), EL(30,118,90,210,0x1E1442), EL(204,118,90,210,0x1E1442), ELO(116,152,170,100,0xA06CFF,60), ELO(116,152,110,64,0xD6B4FF,60), EL(116,226,300,70,0x241640),
      SI(ICO_PIEDRA,3,70,24,0x2E1E50), SI(ICO_PIEDRA,2,160,18,0x2E1E50), SI(ICO_PIEDRA,3,202,34,0x2E1E50), SI(ICO_PIEDRA,2,32,60,0x2E1E50),
      SIC(ICO_CRISTAL,2,24,110), SIC(ICO_CRISTAL,2,208,104), SIC(ICO_CRISTAL,1,40,138), SIC(ICO_CRISTAL,1,196,136), SIC(ICO_CRISTAL,2,18,160), SIC(ICO_CRISTAL,2,214,158)},
     {GH(78,136,18,94,150),
      CRIS(ICO_CRIS_0,3,52,178,5,0), CRIS(ICO_CRIS_1,3,84,188,6,30), CRIS(ICO_CRIS_2,4,116,194,5,60), CRIS(ICO_CRIS_3,3,148,188,6,15), CRIS(ICO_CRIS_4,3,180,178,5,45), CRIS(ICO_CRIS_5,2,116,160,7,75),
      MOW(ICO_NOTA,2,60,170,70,40,8,48,6,2), MOW(ICO_NOTA,2,100,172,110,30,18,58,6,2), MOW(ICO_NOTA,2,150,170,140,28,28,68,6,2), MOW(ICO_NOTA,2,180,170,176,38,38,78,6,2), MOW(ICO_NOTA,2,116,160,116,24,50,92,8,2),
      MOW(ICO_MURCIELAGO,2,-20,54,260,74,12,86,8,4),
      ORBF(ICO_ESTRELLA,1,116,118,70,40,2,0,6), ORBF(ICO_ESTRELLA,1,116,118,50,28,3,50,7),
      GRO(ICO_ESTRELLA,5,116,112,56,86,170),
      MOV(ICO_GOTA,1,70,30,70,150,6,22), MOV(ICO_GOTA,1,160,24,160,150,30,46), MOV(ICO_GOTA,1,196,40,196,160,58,74)}},
};
/* (v37) el recuerdo dorado del final secreto, como dibujo de cuento: Tilo, chiquito, llega al árbol viejo con la mitad del Corazón,
 * la guarda en un huevo que brilla igual que el de ella, lo tapa con hojas y se va; arriba pasa Nimba a buscarlo */
static const vineta_t CINE_ORO = {0xF6DCA0, 0xE8BC70, 0xB8884A, MAX_SH, MAX_AC,
     {ELO(116,120,260,180,0xFFE9B0,120), ELO(116,96,150,110,0xFFF4D0,140), EL(40,186,190,60,0xC89A50), EL(200,190,200,60,0xB8884A), EL(116,222,330,70,0xA87A40),
      SI(ICO_ARBOLITO,3,22,150,0x9A6A34), SI(ICO_ARBOLITO,2,212,156,0x9A6A34), SI(ICO_ARBOL_OFRENDAS,6,150,128,0x7A4A24),
      SIC(ICO_FLOR,2,40,196), SIC(ICO_HIERBA,2,90,200), SIC(ICO_HIERBA,2,200,206)},
     {CST(30,30,0), CST(70,18,25), CST(190,26,50), CST(214,58,75), CST(120,14,10), CST(50,60,35),
      MOV(ICO_NIMBA,4,-40,40,280,30,62,98),
      {ICO_TILO,3,0,1,6,0,0,0,6,{K(0,-20,176,100,0),K(4,0,176,100,255),K(26,112,178,100,255),K(54,112,178,100,255),K(70,250,170,100,255),K(74,262,168,100,0)}},
      MOV(ICO_MEDIO_CORAZON,2,8,150,128,172,6,30),
      SP(ICO_ESTRELLA,3,128,176,28,32,40),
      {ICO_HUEVO_ORO,3,0,1,4,0,0,4,5,{K(0,140,182,30,0),K(30,140,182,30,0),K(36,140,182,110,255),K(95,140,182,110,255),K(99,140,182,110,0)}},
      MOW(ICO_HOJA,2,110,120,138,182,40,56,6,2), MOW(ICO_HOJA,2,170,110,146,184,44,60,6,2), MOW(ICO_HOJA_SECA,2,140,100,134,186,48,64,8,2),
      ORBF(ICO_ESTRELLA,1,140,176,26,12,2,0,6), ORBF(ICO_ESTRELLA,1,140,176,18,8,3,40,6),
      MOW(ICO_MARIPOSA,2,-20,120,260,100,10,90,10,3)}};
static const char *const CINE_TXT[4] = {
    "UNA CUCHARA DE PALO, OLVIDADA ENTRE EL MUSGO, ESPERABA A QUIEN QUISIERA CUIDAR.",
    "EN LAS ESTRELLAS DE LA LAGUNA SE SENTABA A DESCANSAR EL CUIDADOR.",
    "SU ÚLTIMA COMETA TODAVÍA VUELA SOLA. SOLO BAJA A DESCANSAR UN RATO.",
    "ANTES DE DORMIR, EL CUIDADOR DEJÓ UN VERSO DE SU CANCIÓN ENTRE LOS CRISTALES.",
};

static void fin_aparecer(lv_obj_t *o, uint32_t retraso); /* ui_final.inc */
static int s_cine_pend;        /* 1 + cuál (0 = ninguna) */
static uint32_t s_cine_listo;  /* no antes de este instante (ms) */
static uint32_t s_cine_ini;
static void cine_fin_cb(lv_anim_t *a) { (void)a; if (s_panel == PANEL_CINE) cerrar_panel(); }
static void cine_toque_cb(lv_event_t *e) { (void)e; if (s_panel == PANEL_CINE && s_ms - s_cine_ini > 2500) cerrar_panel(); }
static void panel_cine(int esp)
{
    if (esp < 0 || esp > 3) return;
    abrir_tarjeta(PANEL_CINE, 466, 466);
    lv_obj_align(s_tarjeta, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_opa(s_tarjeta, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_tarjeta, 0, 0);
    lv_obj_set_style_shadow_width(s_tarjeta, 0, 0);
    if (s_cerrar_x) ocultar(s_cerrar_x, true);
    s_cine_ini = s_ms;
    lv_obj_t *v = escena_construir(s_tarjeta, 0, 0, &CINES[esp], 0, 2, 466, 466, CINE_SUELO[esp], true);
    lv_anim_t *an = lv_anim_get(v, esc_cb);
    if (an) lv_anim_set_completed_cb(an, cine_fin_cb);
    lv_obj_t *pl = lv_obj_create(s_tarjeta); /* una placa oscura detrás del texto, para que se lea sobre cualquier escena */
    lv_obj_remove_style_all(pl);
    lv_obj_set_size(pl, 262, 92);
    lv_obj_align(pl, LV_ALIGN_TOP_MID, 0, 54);
    lv_obj_set_style_radius(pl, 22, 0);
    lv_obj_set_style_bg_color(pl, lv_color_hex(0x0A0E2A), 0);
    lv_obj_set_style_bg_opa(pl, LV_OPA_60, 0);
    lv_obj_remove_flag(pl, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_t *tx = texto(pl, CINE_TXT[esp], 0xFFEFB8, 2, 240);
    lv_obj_set_style_text_align(tx, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(tx);
    fin_aparecer(pl, 4200);
    lv_obj_t *ov = lv_obj_create(s_tarjeta); /* un toque (pasados unos segundos) la salta */
    lv_obj_remove_style_all(ov);
    lv_obj_set_size(ov, 466, 466);
    lv_obj_set_pos(ov, 0, 0);
    lv_obj_add_flag(ov, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(ov, cine_toque_cb, LV_EVENT_CLICKED, NULL);
    accion(UI_ACCION_SONAR, SND_HADAS);
}
static void cine_revisar(void)
{
    if (!s_cine_pend || !s_pet || s_panel != PANEL_NINGUNO || s_bienv_activa || s_pecera || s_pet->dormida) return;
    if ((int32_t)(s_ms - s_cine_listo) < 0) return;
    const int e = s_cine_pend - 1;
    s_cine_pend = 0;
    panel_cine(e);
}
