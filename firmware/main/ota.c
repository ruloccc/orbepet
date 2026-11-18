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

/* (1.0.20) el mismo modo Wi-Fi, pero para mandar un informe (en vez de buscar la versión nueva) */
#define OTA_MARCA_INFORME 0x1F0A3E55u
static bool s_informe;
void ota_pedir_informe(void)
{
    s_marca = OTA_MARCA_INFORME;
    vTaskDelay(pdMS_TO_TICKS(200));
    esp_restart();
}

bool ota_modo_pedido(void)
{
    const bool sw = esp_reset_reason() == ESP_RST_SW;
    const bool si = (s_marca == OTA_MARCA || s_marca == OTA_MARCA_INFORME) && sw;
    s_informe = si && s_marca == OTA_MARCA_INFORME;
    s_marca = 0;
    return si;
}

/* (1.0.20) la llave para los informes: una llave de GitHub que SOLO puede crear avisos en el repositorio privado
 * orbepet-informes. Se carga una vez por cable (MASCOTA LLAVE) y vive en la NVS; nunca va en el código ni en el registro. */
#define INF_CLAVE_NVS "inf_llave"
bool ota_informe_llave(void)
{
    char k[200];
    size_t n = sizeof k;
    const bool ok = hal_storage_get(INF_CLAVE_NVS, k, &n) && n >= 20;
    memset(k, 0, sizeof k);
    return ok;
}
bool ota_informe_guardar_llave(const char *llave)
{
    if (!llave || !llave[0]) return hal_storage_set(INF_CLAVE_NVS, NULL, 0) == ESP_OK;
    const size_t n = strlen(llave);
    if (n < 20 || n > 190) return false;
    for (size_t i = 0; i < n; i++) if ((unsigned char)llave[i] <= ' ' || (unsigned char)llave[i] >= 127) return false;
    return hal_storage_set(INF_CLAVE_NVS, llave, n) == ESP_OK;
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

typedef enum { E_INICIO, E_BUSCAR, E_LISTA, E_CLAVE, E_CONECTAR, E_REVISAR, E_AVISO, E_BAJAR, E_FIN, E_INFORME } etapa_t;

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

static EXT_RAM_BSS_ATTR char s_json[2048]; /* (1.0.17) version.json entero: también trae la lista de "recursos" */
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
            snprintf(s_json, sizeof s_json, "%s", buf);
            ok = json_txt(buf, "version", ver, nv) && fw_codigo(ver) > 0;
            if (!json_txt(buf, "nombre", nombre, nn)) nombre[0] = 0;
        } else diag("version.json: estado %d, %d bytes", st, n);
    }
    free(buf);
    esp_http_client_close(c);
    esp_http_client_cleanup(c);
    return ok;
}

/* ------------------------------------------------------------------ la pantalla (en ota_pantalla.inc: también la usa
 * la vista previa del PC, sim/preview/ota_sheet.c) */
#include "ota_pantalla.inc"

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

/* ------------------------------------------------------------------ (1.0.17) los archivos de la memoria de recursos
 * version.json trae "recursos": "pj0.bin:45213:248a13e3 pj1.bin:...": nombre, bytes y crc32. Se bajan los que faltan o
 * cambiaron (desde OTA_URL_BASE "recursos/") a la PSRAM, se revisan y se guardan. Así los fondos de los juegos de a dos
 * llegan por el aire: no ocupan lugar en el programa. */
static uint32_t crc32_bytes(const uint8_t *d, int n)
{
    uint32_t c = 0xFFFFFFFFu;
    for (int i = 0; i < n; i++) {
        c ^= d[i];
        for (int k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u)));
    }
    return ~c;
}
static bool s_res_montada;
/* el archivo nom ya está y es ese (mismo largo y crc) */
static bool recurso_igual(const char *nom, int tam, uint32_t crc)
{
    if (hal_res_size(nom) != tam) return false;
    uint8_t *b = heap_caps_malloc((size_t)tam, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!b) return false;
    const bool ok = hal_res_read(nom, b, tam) == tam && crc32_bytes(b, tam) == crc;
    free(b);
    return ok;
}
static bool recurso_bajar(const char *nom, int tam, uint32_t crc)
{
    char url[192];
    snprintf(url, sizeof url, OTA_URL_BASE "recursos/%s?n=%08lx", nom, (unsigned long)esp_random());
    esp_http_client_config_t hc = {
        .url = url,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = 20000,
        .buffer_size = 4096,
        .buffer_size_tx = 1024,
    };
    uint8_t *b = heap_caps_malloc((size_t)tam, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!b) { diag("recurso %s: sin memoria", nom); return false; }
    esp_http_client_handle_t c = esp_http_client_init(&hc);
    int va = 0, st = 0;
    if (c && esp_http_client_open(c, 0) == ESP_OK) {
        esp_http_client_fetch_headers(c);
        st = esp_http_client_get_status_code(c);
        while (st == 200 && va < tam && !s_cancelar) {
            const int k = esp_http_client_read(c, (char *)b + va, tam - va);
            if (k <= 0) break;
            va += k;
            uso();
        }
    }
    if (c) { esp_http_client_close(c); esp_http_client_cleanup(c); }
    bool ok = st == 200 && va == tam && crc32_bytes(b, tam) == crc;
    if (ok) ok = hal_res_save(nom, b, tam);
    if (!ok) diag("recurso %s: no se pudo (estado %d, %d de %d bytes)", nom, st, va, tam);
    free(b);
    return ok;
}
/* devuelve cuántos se bajaron, o -1 si alguno falló */
static int recursos_al_dia(void)
{
    static char lista[512];
    if (!json_txt(s_json, "recursos", lista, sizeof lista) || !lista[0]) return 0;
    if (!s_res_montada) {
        if (ram_kb() < 8) { diag("recursos: poca RAM interna (%u KB)", ram_kb()); return -1; }
        s_res_montada = hal_res_init() == ESP_OK;
        if (!s_res_montada) { diag("recursos: no se pudo abrir la memoria de recursos"); return -1; }
    }
    struct { char n[24]; int t; uint32_t c; } r[8];
    int nr = 0, faltan = 0;
    for (char *q = lista; *q && nr < 8;) {
        while (*q == ' ') q++;
        if (!*q) break;
        char *fin = strchr(q, ' ');
        if (fin) *fin = 0;
        char *a = strchr(q, ':'), *b2 = a ? strchr(a + 1, ':') : NULL;
        if (a && b2 && a - q < (int)sizeof r[0].n && !strchr(q, '/')) {
            *a = 0;
            snprintf(r[nr].n, sizeof r[nr].n, "%s", q);
            r[nr].t = atoi(a + 1);
            r[nr].c = (uint32_t)strtoul(b2 + 1, NULL, 16);
            if (r[nr].t > 0 && r[nr].t <= 600 * 1024) {
                if (recurso_igual(r[nr].n, r[nr].t, r[nr].c)) r[nr].t = -r[nr].t; /* (negativo: ya está) */
                else faltan++;
                nr++;
            }
        }
        if (!fin) break;
        q = fin + 1;
    }
    if (!faltan) return 0;
    int hechos = 0, k = 0;
    for (int i = 0; i < nr; i++) {
        if (r[i].t < 0) continue;
        char t[64];
        snprintf(t, sizeof t, "%d DE %d", ++k, faltan);
        msg("BAJANDO LOS FONDOS NUEVOS...", t, false, true);
        if (s_cancelar) return -1;
        if (!recurso_bajar(r[i].n, r[i].t, r[i].c)) return -1;
        hechos++;
    }
    diag("recursos: %d bajados", hechos);
    return hechos;
}

/* ------------------------------------------------------------------ (1.0.20) el informe de errores */

#define INF_URL "https://api.github.com/repos/ruloccc/orbepet-informes/issues"
#define INF_TXT (24576 + 12288 + 512) /* lo mismo que lee el cable (MASCOTA REGISTRO): eventos raros + registro */

/* deja el texto en UTF-8 válido (un corte puede partir una letra con tilde: GitHub rechaza el JSON entero) */
static void utf8_sanear(char *t, int n)
{
    for (int i = 0; i < n;) {
        const unsigned char c = (unsigned char)t[i];
        int k = c < 0x80 ? 1 : (c & 0xE0) == 0xC0 ? 2 : (c & 0xF0) == 0xE0 ? 3 : (c & 0xF8) == 0xF0 ? 4 : 0;
        if (k > 1 && i + k <= n) for (int j = 1; j < k; j++) if (((unsigned char)t[i + j] & 0xC0) != 0x80) { k = 0; break; }
        if (k == 0 || i + k > n) { t[i++] = '?'; continue; }
        i += k;
    }
}
/* agrega s (ya en UTF-8 válido) escapado para JSON; devuelve el nuevo largo */
static int json_poner(char *o, int v, int cap, const char *s, int n)
{
    for (int i = 0; i < n && v < cap - 8; i++) {
        const unsigned char c = (unsigned char)s[i];
        if (c == '"' || c == '\\') { o[v++] = '\\'; o[v++] = (char)c; }
        else if (c == '\n') { o[v++] = '\\'; o[v++] = 'n'; }
        else if (c == '\t') { o[v++] = '\\'; o[v++] = 't'; }
        else if (c == '\r') {}
        else if (c < 0x20) o[v++] = ' ';
        else o[v++] = (char)c;
    }
    return v;
}
static int json_poner_txt(char *o, int v, int cap, const char *s) { return json_poner(o, v, cap, s, (int)strlen(s)); }

/* crea un aviso (issue) en orbepet-informes con el registro; devuelve su número, o ≤ 0 y el motivo en por_que */
static int enviar_informe(char *por_que, size_t npq)
{
    char llave[200];
    size_t nl = sizeof llave - 1;
    memset(llave, 0, sizeof llave);
    if (!hal_storage_get(INF_CLAVE_NVS, llave, &nl) || nl < 20) {
        snprintf(por_que, npq, "FALTA LA LLAVE DE LOS INFORMES: SE CARGA UNA SOLA VEZ CON EL CABLE");
        return 0;
    }
    llave[nl] = 0;
    if (!s_res_montada) s_res_montada = hal_res_init() == ESP_OK;
    char *txt = heap_caps_malloc(INF_TXT + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    const int cap = INF_TXT * 2 + 2048;
    char *js = heap_caps_malloc((size_t)cap, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!txt || !js) {
        free(txt); free(js); memset(llave, 0, sizeof llave);
        snprintf(por_que, npq, "SIN MEMORIA PARA ARMAR EL INFORME");
        return 0;
    }
    int n = s_res_montada ? hal_log_read(txt, INF_TXT) : 0;
    if (n < 0) n = 0;
    txt[n] = 0;
    utf8_sanear(txt, n);
    const bool panico = strstr(txt, "PÁNICO") != NULL;
    char cab[512];
    snprintf(cab, sizeof cab,
             "Informe mandado desde el orbe (Ajustes → Versión → ENVIAR INFORME).\n\n"
             "- Versión: %s (%s)\n- RAM interna en el modo Wi-Fi: %u KB\n- Registro: %d bytes%s\n\n",
             FW_VERSION, FW_NOMBRE, ram_kb(), n, s_res_montada ? "" : " (NO se pudo abrir la memoria de recursos)");
    /* (el título no lleva comillas ni barras: va tal cual) */
    int v = snprintf(js, (size_t)cap, "{\"title\":\"Informe del orbe v%s%s\",\"body\":\"", FW_VERSION, panico ? " (hubo un pánico)" : "");
    v = json_poner_txt(js, v, cap, cab);
    v = json_poner_txt(js, v, cap, "```text\n");
    v = json_poner(js, v, cap, txt, n);
    v = json_poner_txt(js, v, cap, "\n```\n");
    v += snprintf(js + v, (size_t)(cap - v), "\"}");
    free(txt);

    esp_http_client_config_t hc = {
        .url = INF_URL,
        .method = HTTP_METHOD_POST,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = 20000,
        .buffer_size = 2048,
        .buffer_size_tx = 2048,
    };
    esp_http_client_handle_t c = esp_http_client_init(&hc);
    int num = 0, st = 0;
    if (c) {
        char aut[220];
        snprintf(aut, sizeof aut, "Bearer %s", llave);
        esp_http_client_set_header(c, "Authorization", aut);
        memset(aut, 0, sizeof aut);
        esp_http_client_set_header(c, "Accept", "application/vnd.github+json");
        esp_http_client_set_header(c, "X-GitHub-Api-Version", "2022-11-28");
        esp_http_client_set_header(c, "User-Agent", "orbepet");
        esp_http_client_set_header(c, "Content-Type", "application/json; charset=utf-8");
        if (esp_http_client_open(c, v) == ESP_OK) {
            int va = 0;
            while (va < v) {
                const int k = esp_http_client_write(c, js + va, v - va > 1024 ? 1024 : v - va);
                if (k <= 0) break;
                va += k;
                uso();
            }
            if (va == v) {
                esp_http_client_fetch_headers(c);
                st = esp_http_client_get_status_code(c);
                char r[1536];
                const int nr = esp_http_client_read_response(c, r, sizeof r - 1);
                r[nr > 0 ? nr : 0] = 0;
                const char *q = strstr(r, "\"number\":");
                if (st == 201 && q) num = atoi(q + 9);
                else if (st == 201) num = 1;
            }
        }
        esp_http_client_close(c);
        esp_http_client_cleanup(c);
    }
    memset(llave, 0, sizeof llave);
    free(js);
    if (num > 0) return num;
    if (st == 401) snprintf(por_que, npq, "GITHUB NO ACEPTÓ LA LLAVE (¿LA BORRARON?). HAY QUE CARGAR OTRA");
    else if (st == 403 || st == 404) snprintf(por_que, npq, "LA LLAVE NO TIENE PERMISO PARA ESE REPOSITORIO (ESTADO %d)", st);
    else if (st) snprintf(por_que, npq, "GITHUB CONTESTÓ CON UN ERROR (ESTADO %d). PRUEBA MÁS RATO", st);
    else snprintf(por_que, npq, "NO PUDE CONECTARME A GITHUB. PRUEBA MÁS RATO");
    return 0;
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
                etapa = s_informe ? E_INFORME : E_REVISAR;
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
                const int rb = recursos_al_dia(); /* (1.0.17) los fondos que faltan (la versión nueva los pide en su primer viaje) */
                revisar_cancelar();
                if (rb < 0) {
                    msg("NO PUDE BAJAR LOS FONDOS", "EL JUEGO FUNCIONA IGUAL. PRUEBA DE NUEVO MÁS RATO", false, true);
                    if (esperar(8000)) salir("CANCELAR");
                    salir("recursos fallidos");
                }
                char t[96];
                snprintf(t, sizeof t, "TIENES LA %s, LA ÚLTIMA PUBLICADA", FW_VERSION);
                msg(rb > 0 ? "¡LISTO! FONDOS NUEVOS" : "¡ESTÁS AL DÍA!", t, false, true);
                diag("al día (publicada %s)", s_nueva);
                if (esperar(6000)) salir("CANCELAR");
                salir("al día");
            }
            etapa = E_AVISO;
            break;
        }
        case E_INFORME: { /* (1.0.20) */
            msg("CONECTADO", "MANDANDO EL INFORME...", false, false);
            char por_que[96] = "";
            const int num = enviar_informe(por_que, sizeof por_que);
            char t[120];
            if (num > 0) {
                snprintf(t, sizeof t, "QUEDÓ COMO EL AVISO N° %d. ¡GRACIAS!", num);
                msg("¡INFORME ENVIADO!", t, false, true);
                diag("informe enviado: aviso #%d", num);
            } else {
                msg("NO SE PUDO MANDAR EL INFORME", por_que, false, true);
                diag("informe: no se pudo (%s)", por_que);
            }
            if (esperar(8000)) salir("CANCELAR");
            salir(num > 0 ? "informe enviado" : "informe fallido");
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
                /* (1.0.21) los fondos de la versión nueva se bajan en este mismo viaje (antes había que volver a BUSCAR ACTUALIZACIÓN
                 * ya con la versión nueva puesta, y si no, los juegos quedaban sin fondo). Se hace después de grabar el programa, para no
                 * quitarle memoria a la descarga; si fallan, la versión nueva igual queda y los pide en el próximo BUSCAR */
                const int rb = recursos_al_dia();
                if (rb < 0) diag("recursos: fallaron junto a la versión nueva (se piden en el próximo BUSCAR)");
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
