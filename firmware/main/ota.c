/* Actualizar el firmware por Wi-Fi: ver ota.h.
 *
 * (1.0.13) Todo desde el orbe, sin celular: el modo actualizar tiene su propia pantalla. Busca las redes, eliges la
 * tuya, escribes la clave con un teclado (mayúsculas, minúsculas, números y símbolos), se conecta y, si hay una
 * versión más nueva, avisa y se actualiza. Si el Wi-Fi ya estaba guardado, se conecta solo y no pregunta nada.
 * CANCELAR (o 10 min sin tocar nada) reinicia y vuelve el juego de siempre.
 *
 * Memoria: la pantalla se prende primero (para mostrar todo de inmediato) y después el Wi-Fi solo como cliente, con
 * pocos búferes; no hay tareas extra: la descarga corre en la tarea principal. Si no alcanza, lo dice y queda anotado. */
#include "ota.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_app_desc.h"
#include "esp_app_format.h"
#include "esp_attr.h"
#include "esp_crt_bundle.h"
#include "esp_heap_caps.h"
#include "esp_event.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_ota_ops.h"
#include "esp_random.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "art.h"
#include "hal.h"
#include "version.h"

static const char *TAG = "ota";

/* dónde se publican las versiones (repositorio público: solo los .bin y version.json) */
#ifndef OTA_URL_BASE
#define OTA_URL_BASE "https://raw.githubusercontent.com/ruloccc/orbepet-firmware/main/"
#endif
#define OTA_INACTIVO_MS (10u * 60u * 1000u) /* 10 min sin tocar nada: vuelve al juego */

/* ------------------------------------------------------------------ marca entre reinicios */

#define OTA_MARCA 0x07A0B0E7u
static RTC_NOINIT_ATTR uint32_t s_marca;
/* (1.0.11) lo que pasó en el modo actualizar (memoria, errores): el próximo arranque normal lo anota en el registro */
#define OTA_DIAG_MARCA 0xD1A60B7Eu
static RTC_NOINIT_ATTR uint32_t s_diag_marca;
static RTC_NOINIT_ATTR char s_diag[240];
static void diag(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static void diag(const char *fmt, ...)
{
    if (s_diag_marca != OTA_DIAG_MARCA) { s_diag_marca = OTA_DIAG_MARCA; s_diag[0] = 0; }
    s_diag[sizeof s_diag - 1] = 0;
    size_t k = strlen(s_diag);
    if (k + 4 >= sizeof s_diag) return;
    if (k) { s_diag[k++] = ' '; s_diag[k++] = '|'; s_diag[k++] = ' '; s_diag[k] = 0; }
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(s_diag + k, sizeof s_diag - k, fmt, ap);
    va_end(ap);
    ESP_LOGI(TAG, "diag: %s", s_diag + k);
}
const char *ota_diagnostico(void)
{
    if (s_diag_marca != OTA_DIAG_MARCA) return NULL;
    s_diag_marca = 0;
    s_diag[sizeof s_diag - 1] = 0;
    return s_diag[0] ? s_diag : NULL;
}
static unsigned ram_kb(void) { return (unsigned)(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024); }
void ota_nombre_red(char *b, size_t n)
{
    uint8_t mac[6] = {0};
    esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);
    snprintf(b, n, "ORBEPET-%02X%02X", mac[4], mac[5]);
}

void ota_pedir_modo(void)
{
    s_marca = OTA_MARCA;
    vTaskDelay(pdMS_TO_TICKS(200));
    esp_restart();
}

bool ota_modo_pedido(void)
{
    const bool si = s_marca == OTA_MARCA && esp_reset_reason() == ESP_RST_SW;
    s_marca = 0;
    return si;
}

void ota_arranque_ok(void)
{
    const esp_partition_t *run = esp_ota_get_running_partition();
    esp_ota_img_states_t st;
    if (run && esp_ota_get_state_partition(run, &st) == ESP_OK && st == ESP_OTA_IMG_PENDING_VERIFY) {
        if (esp_ota_mark_app_valid_cancel_rollback() == ESP_OK) hal_evento("ACTUALIZACIÓN: la versión %s arrancó bien (queda confirmada)", FW_VERSION);
    }
}

const char *ota_version_fallida(void)
{
    static char v[33];
    const esp_partition_t *p = esp_ota_get_last_invalid_partition();
    esp_app_desc_t d;
    if (!p || esp_ota_get_partition_description(p, &d) != ESP_OK) return NULL;
    snprintf(v, sizeof v, "%s", d.version);
    return v;
}

/* ------------------------------------------------------------------ estado */

typedef struct {
    char ssid[33];
    char clave[65];
} ota_wifi_t;

typedef enum { E_INICIO, E_BUSCAR, E_LISTA, E_CLAVE, E_CONECTAR, E_REVISAR, E_AVISO, E_BAJAR, E_FIN } etapa_t;

#define N_REDES 12
typedef struct { char ssid[33]; int8_t rssi; bool abierta; } red_t;

static ota_wifi_t s_wifi;           /* el Wi-Fi guardado (NVS "wifi") */
static ota_wifi_t s_prueba;         /* el que se está probando */
static red_t s_redes[N_REDES];
static int s_nredes;
static volatile bool s_cancelar, s_ip, s_ya, s_buscar_otra;
static volatile int s_razon;        /* motivo de la última desconexión */
static volatile int s_elegida = -1; /* la red tocada en la lista */
static volatile bool s_conectar_pedido;
static char s_msg1[96], s_msg2[160];
static char s_nueva[33], s_nueva_nombre[64];
static uint32_t s_ultimo_uso_ms;
static lv_display_t *s_disp;

static uint32_t ms(void) { return (uint32_t)(esp_timer_get_time() / 1000); }
static void uso(void) { s_ultimo_uso_ms = ms(); }

/* ------------------------------------------------------------------ Wi-Fi (solo cliente) */

static void evento(void *arg, esp_event_base_t base, int32_t id, void *datos)
{
    (void)arg;
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        const wifi_event_sta_disconnected_t *d = datos;
        s_razon = d ? d->reason : -1;
        s_ip = false;
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        s_ip = true;
    }
}

static esp_err_t wifi_iniciar(void)
{
    esp_err_t e = esp_netif_init();
    if (e != ESP_OK) return e;
    e = esp_event_loop_create_default();
    if (e != ESP_OK && e != ESP_ERR_INVALID_STATE) return e;
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    e = esp_wifi_init(&cfg);
    if (e != ESP_OK) return e;
    esp_wifi_set_storage(WIFI_STORAGE_RAM); /* nada del Wi-Fi a la memoria: ahí vive la partida */
    esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, evento, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, evento, NULL);
    e = esp_wifi_set_mode(WIFI_MODE_STA);
    if (e != ESP_OK) return e;
    return esp_wifi_start();
}

static int cmp_red(const void *a, const void *b) { return ((const red_t *)b)->rssi - ((const red_t *)a)->rssi; }

static void buscar_redes(void)
{
    s_nredes = 0;
    wifi_scan_config_t sc = {0};
    esp_wifi_disconnect();
    if (esp_wifi_scan_start(&sc, true) != ESP_OK) return;
    uint16_t n = 0;
    esp_wifi_scan_get_ap_num(&n);
    if (n > 30) n = 30;
    wifi_ap_record_t *ap = n ? calloc(n, sizeof *ap) : NULL;
    if (!ap) { esp_wifi_clear_ap_list(); return; }
    esp_wifi_scan_get_ap_records(&n, ap);
    for (int i = 0; i < n && s_nredes < N_REDES; i++) {
        if (!ap[i].ssid[0]) continue;
        bool rep = false;
        for (int j = 0; j < s_nredes && !rep; j++) rep = !strcmp(s_redes[j].ssid, (const char *)ap[i].ssid);
        if (rep) continue;
        red_t *r = &s_redes[s_nredes++];
        snprintf(r->ssid, sizeof r->ssid, "%s", (const char *)ap[i].ssid);
        r->rssi = ap[i].rssi;
        r->abierta = ap[i].authmode == WIFI_AUTH_OPEN;
    }
    free(ap);
    qsort(s_redes, (size_t)s_nredes, sizeof s_redes[0], cmp_red);
}

/* conecta con s_prueba; espera hasta 25 s. true = hay IP */
static bool conectar(void)
{
    wifi_config_t c = {0};
    snprintf((char *)c.sta.ssid, sizeof c.sta.ssid, "%s", s_prueba.ssid);
    snprintf((char *)c.sta.password, sizeof c.sta.password, "%s", s_prueba.clave);
    c.sta.threshold.authmode = s_prueba.clave[0] ? WIFI_AUTH_WEP : WIFI_AUTH_OPEN;
    c.sta.pmf_cfg.capable = true;
    esp_wifi_disconnect();
    vTaskDelay(pdMS_TO_TICKS(100));
    s_ip = false; s_razon = 0;
    if (esp_wifi_set_config(WIFI_IF_STA, &c) != ESP_OK) return false;
    const uint32_t t0 = ms();
    int intentos = 0;
    esp_wifi_connect();
    while (ms() - t0 < 25000u && !s_cancelar && !s_buscar_otra) {
        vTaskDelay(pdMS_TO_TICKS(200));
        if (s_ip) return true;
        if (s_razon) { /* se cayó: unos pocos intentos más */
            if (++intentos >= 4) break;
            const int r = s_razon; s_razon = 0;
            if (r == WIFI_REASON_ASSOC_LEAVE) { intentos--; continue; } /* fue nuestro propio «desconectar» */
            if (r == WIFI_REASON_NO_AP_FOUND && intentos >= 2) { s_razon = r; break; }
            vTaskDelay(pdMS_TO_TICKS(600));
            esp_wifi_connect();
        }
    }
    return s_ip;
}

/* ------------------------------------------------------------------ internet: version.json y descarga */

/* "clave": "valor" en un JSON chico (sin librería) */
static bool json_txt(const char *j, const char *clave, char *out, size_t n)
{
    char k[40];
    snprintf(k, sizeof k, "\"%s\"", clave);
    const char *p = strstr(j, k);
    if (!p) return false;
    p = strchr(p + strlen(k), ':');
    if (!p) return false;
    p = strchr(p, '"');
    if (!p) return false;
    p++;
    size_t i = 0;
    while (*p && *p != '"' && i + 1 < n) out[i++] = *p++;
    out[i] = 0;
    return *p == '"';
}

static bool leer_version_publicada(char *ver, size_t nv, char *nombre, size_t nn)
{
    char url[160];
    snprintf(url, sizeof url, OTA_URL_BASE "version.json?n=%08lx", (unsigned long)esp_random()); /* sin caché vieja */
    esp_http_client_config_t hc = {
        .url = url,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = 15000,
        .buffer_size = 2048,
        .buffer_size_tx = 1024,
    };
    esp_http_client_handle_t c = esp_http_client_init(&hc);
    if (!c) return false;
    bool ok = false;
    char *buf = calloc(1, 2048);
    if (buf && esp_http_client_open(c, 0) == ESP_OK) {
        esp_http_client_fetch_headers(c);
        const int st = esp_http_client_get_status_code(c);
        const int n = esp_http_client_read_response(c, buf, 2047);
        if (st == 200 && n > 0) {
            buf[n] = 0;
            ok = json_txt(buf, "version", ver, nv) && fw_codigo(ver) > 0;
            if (!json_txt(buf, "nombre", nombre, nn)) nombre[0] = 0;
        } else diag("version.json: estado %d, %d bytes", st, n);
    }
    free(buf);
    esp_http_client_close(c);
    esp_http_client_cleanup(c);
    return ok;
}

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
    if (t2 && t2[0]) { lv_obj_t *c = txt(s_scr, t2, 0xFFFFFF, 2, 340, true); lv_obj_align(c, LV_ALIGN_TOP_MID, 0, 186); }
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
    /* la fila de abajo: modo, números, espacio, borrar, conectar */
    const int y = Y0 + 4 * (H + G) + 2;
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
    lv_obj_align(b, LV_ALIGN_TOP_MID, 0, 190);
    s_barra = lv_obj_create(s_scr);
    lv_obj_remove_style_all(s_barra);
    lv_obj_set_size(s_barra, 300, 18);
    lv_obj_align(s_barra, LV_ALIGN_TOP_MID, 0, 250);
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
    lv_obj_align(s_pct_txt, LV_ALIGN_TOP_MID, 0, 282);
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

/* ------------------------------------------------------------------ descarga */

static bool descargar(void)
{
    mostrar(f_bajar);
    esp_http_client_config_t hc = {
        .url = OTA_URL_BASE "mascota.bin",
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = 20000,
        .buffer_size = 4096,
        .buffer_size_tx = 2048,
        .keep_alive_enable = true,
    };
    esp_https_ota_config_t oc = {.http_config = &hc};
    esp_https_ota_handle_t h = NULL;
    esp_err_t e = esp_https_ota_begin(&oc, &h);
    if (e != ESP_OK) { diag("descarga: no partió (error 0x%x), RAM %u KB", (unsigned)e, ram_kb()); return false; }
    esp_app_desc_t d;
    e = esp_https_ota_get_img_desc(h, &d);
    if (e != ESP_OK || strcmp(d.project_name, esp_app_get_description()->project_name)) {
        esp_https_ota_abort(h);
        diag("descarga: el archivo no es del orbe");
        return false;
    }
    const int total = esp_https_ota_get_image_size(h);
    int ult = -1;
    while ((e = esp_https_ota_perform(h)) == ESP_ERR_HTTPS_OTA_IN_PROGRESS) {
        const int leido = esp_https_ota_get_image_len_read(h);
        const int pct = total > 0 ? (int)((int64_t)leido * 100 / total) : 0;
        if (pct != ult) { ult = pct; bajar_progreso(pct); uso(); }
    }
    if (e == ESP_OK && esp_https_ota_is_complete_data_received(h)) e = esp_https_ota_finish(h);
    else { esp_https_ota_abort(h); if (e == ESP_OK) e = ESP_FAIL; }
    if (e != ESP_OK) { diag("descarga: falló (error 0x%x), RAM %u KB", (unsigned)e, ram_kb()); return false; }
    diag("instalada la %s (estaba en la %s)", d.version, FW_VERSION);
    return true;
}

/* ------------------------------------------------------------------ el modo */

static void salir(const char *por_que)
{
    diag("salgo: %s, mínimo de RAM interna %u KB", por_que, (unsigned)(heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL) / 1024));
    esp_wifi_stop();
    vTaskDelay(pdMS_TO_TICKS(300));
    esp_restart();
}

/* espera un rato atendiendo CANCELAR; true si cancelaron */
static bool esperar(uint32_t t)
{
    const uint32_t t0 = ms();
    while (ms() - t0 < t) { if (s_cancelar) return true; vTaskDelay(pdMS_TO_TICKS(100)); }
    return s_cancelar;
}

static void revisar_cancelar(void) { if (s_cancelar) salir("CANCELAR"); }

void ota_modo(void)
{
    s_diag_marca = 0;
    diag("v%s: RAM interna al entrar %u KB", FW_VERSION, ram_kb());
    size_t n = sizeof s_wifi;
    if (!hal_storage_get("wifi", &s_wifi, &n) || n != sizeof s_wifi) memset(&s_wifi, 0, sizeof s_wifi);
    s_wifi.ssid[sizeof s_wifi.ssid - 1] = 0;
    s_wifi.clave[sizeof s_wifi.clave - 1] = 0;

    /* primero la pantalla: se ve de inmediato qué está pasando */
    s_disp = hal_display_init_liviano(); /* (búferes chicos: deja ~28 KB más para el Wi-Fi) */
    diag("pantalla %s, quedan %u KB", s_disp ? "sí" : "NO", ram_kb());
    msg("ENCENDIENDO EL WI-FI...", "", false, true);
    const esp_err_t e = wifi_iniciar();
    if (e == ESP_OK) diag("wifi ok, quedan %u KB", ram_kb());
    else diag("wifi: error 0x%x (0x101 = sin memoria), quedan %u KB", (unsigned)e, ram_kb());
    if (e != ESP_OK) {
        msg("NO SE PUDO ENCENDER EL WI-FI", "FALTA MEMORIA. QUEDÓ ANOTADO EN EL REGISTRO. VUELVO AL JUEGO", false, false);
        vTaskDelay(pdMS_TO_TICKS(7000));
        salir("sin Wi-Fi");
    }
    uso();

    /* con el Wi-Fi guardado, se conecta solo; si no, a buscar */
    etapa_t etapa = s_wifi.ssid[0] ? E_CONECTAR : E_BUSCAR;
    if (etapa == E_CONECTAR) s_prueba = s_wifi;
    bool guardada = etapa == E_CONECTAR; /* la red que se intenta es la guardada */

    while (true) {
        revisar_cancelar();
        if (ms() - s_ultimo_uso_ms > OTA_INACTIVO_MS) salir("10 min sin tocar nada");
        s_buscar_otra = false;
        switch (etapa) {
        case E_BUSCAR:
            msg("BUSCANDO REDES...", "", false, true);
            buscar_redes();
            s_elegida = -1;
            mostrar(f_lista);
            etapa = E_LISTA;
            break;
        case E_LISTA:
            while (s_elegida < 0 && !s_buscar_otra && !s_cancelar && ms() - s_ultimo_uso_ms < OTA_INACTIVO_MS) vTaskDelay(pdMS_TO_TICKS(100));
            revisar_cancelar();
            if (s_buscar_otra) { etapa = E_BUSCAR; break; }
            if (s_elegida < 0 || s_elegida >= s_nredes) break;
            memset(&s_prueba, 0, sizeof s_prueba);
            snprintf(s_prueba.ssid, sizeof s_prueba.ssid, "%s", s_redes[s_elegida].ssid);
            guardada = s_wifi.ssid[0] && !strcmp(s_wifi.ssid, s_prueba.ssid);
            if (guardada) { s_prueba = s_wifi; etapa = E_CONECTAR; }
            else if (s_redes[s_elegida].abierta) etapa = E_CONECTAR;
            else { s_clave_aviso = NULL; s_conectar_pedido = false; s_modo_tec = 0; s_ver_clave = false; mostrar(f_clave); etapa = E_CLAVE; }
            break;
        case E_CLAVE:
            while (!s_conectar_pedido && !s_buscar_otra && !s_cancelar && ms() - s_ultimo_uso_ms < OTA_INACTIVO_MS) {
                if (s_tec_rearmar) { s_tec_rearmar = false; mostrar(teclado_armar); }
                vTaskDelay(pdMS_TO_TICKS(60));
            }
            revisar_cancelar();
            if (s_buscar_otra) { etapa = E_BUSCAR; break; }
            if (s_conectar_pedido) { s_conectar_pedido = false; etapa = E_CONECTAR; }
            break;
        case E_CONECTAR: {
            char t[96];
            snprintf(t, sizeof t, "%s", s_prueba.ssid);
            msg("CONECTANDO...", t, true, true);
            if (conectar()) {
                if (strcmp(s_wifi.ssid, s_prueba.ssid) || strcmp(s_wifi.clave, s_prueba.clave)) { /* funciona: queda guardada */
                    s_wifi = s_prueba;
                    hal_storage_set("wifi", &s_wifi, sizeof s_wifi);
                }
                diag("conectado a la red (RAM %u KB)", ram_kb());
                etapa = E_REVISAR;
            } else {
                revisar_cancelar();
                if (s_buscar_otra) { etapa = E_BUSCAR; break; }
                const int r = s_razon;
                diag("no conectó (motivo %d)", r);
                if (r == WIFI_REASON_NO_AP_FOUND) { msg("NO ENCUENTRO ESA RED", t, true, true); if (esperar(2500)) salir("CANCELAR"); etapa = E_BUSCAR; }
                else if (s_prueba.clave[0] || !guardada) {
                    s_clave_aviso = "NO PUDE ENTRAR: REVISA LA CLAVE";
                    s_conectar_pedido = false; s_modo_tec = 0;
                    if (guardada) s_prueba.clave[0] = 0; /* la guardada ya no sirve: a escribirla de nuevo */
                    mostrar(f_clave);
                    etapa = E_CLAVE;
                } else etapa = E_BUSCAR;
                guardada = false;
            }
            break;
        }
        case E_REVISAR: {
            msg("CONECTADO", "BUSCANDO LA ÚLTIMA VERSIÓN...", false, true);
            if (!leer_version_publicada(s_nueva, sizeof s_nueva, s_nueva_nombre, sizeof s_nueva_nombre)) {
                msg("NO PUDE VER LAS VERSIONES", "HAY WI-FI PERO NO CONTESTA INTERNET. PRUEBA MÁS RATO", false, true);
                if (esperar(15000)) salir("CANCELAR");
                salir("sin respuesta de internet");
            }
            if (fw_codigo(s_nueva) <= fw_codigo(FW_VERSION)) {
                char t[96];
                snprintf(t, sizeof t, "TIENES LA %s, LA ÚLTIMA PUBLICADA", FW_VERSION);
                msg("¡ESTÁS AL DÍA!", t, false, true);
                diag("al día (publicada %s)", s_nueva);
                if (esperar(6000)) salir("CANCELAR");
                salir("al día");
            }
            etapa = E_AVISO;
            break;
        }
        case E_AVISO:
            s_ya = false;
            for (int seg = 5; seg > 0 && !s_ya; seg--) {
                s_aviso_seg = seg;
                mostrar(f_aviso);
                if (esperar(1000)) salir("CANCELAR");
            }
            etapa = E_BAJAR;
            break;
        case E_BAJAR:
            if (descargar()) {
                char t[96];
                snprintf(t, sizeof t, "QUEDÓ INSTALADA LA %s", s_nueva);
                msg("¡LISTO!", t, false, false);
                vTaskDelay(pdMS_TO_TICKS(2500));
                salir("versión nueva instalada");
            }
            msg("LA DESCARGA FALLÓ", "SIGUE LA VERSIÓN DE ANTES. PRUEBA DE NUEVO MÁS RATO", false, true);
            if (esperar(10000)) salir("CANCELAR");
            salir("descarga fallida");
            break;
        default:
            etapa = E_BUSCAR;
            break;
        }
    }
}
