/* Actualizar el firmware por Wi-Fi: ver ota.h. */
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
#include "esp_http_server.h"
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
#define OTA_AP_CLAVE "ORBEPET1" /* (1.0.11) en mayúsculas: el orbe solo escribe mayúsculas */
#define OTA_INACTIVO_MS (15u * 60u * 1000u) /* 15 min sin que nadie haga nada: vuelve al juego */

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

/* ------------------------------------------------------------------ estado compartido */

typedef struct {
    char ssid[33];
    char clave[65];
} ota_wifi_t;

static SemaphoreHandle_t s_mx;
static ota_wifi_t s_wifi;          /* el Wi-Fi de la casa (guardado en la memoria del orbe) */
static char s_ap[24];              /* la red propia: ORBEPET-XXXX */
static char s_linea1[96], s_linea2[96];
static char s_ultima[33];          /* la última versión publicada (vacía: no se sabe todavía) */
static int s_pct = -1;             /* progreso de la descarga o la subida (−1: sin barra) */
static volatile bool s_conectado, s_buscar, s_salir, s_ocupado, s_cambio;
static volatile int s_intentos;
static volatile uint32_t s_reiniciar_ms, s_ultimo_uso_ms;
static TaskHandle_t s_tarea;

static uint32_t ms(void) { return (uint32_t)(esp_timer_get_time() / 1000); }
static void uso(void) { s_ultimo_uso_ms = ms(); }

static void estado(const char *l1, const char *l2, int pct)
{
    xSemaphoreTake(s_mx, portMAX_DELAY);
    if (l1) snprintf(s_linea1, sizeof s_linea1, "%s", l1);
    if (l2) snprintf(s_linea2, sizeof s_linea2, "%s", l2);
    s_pct = pct;
    s_cambio = true;
    xSemaphoreGive(s_mx);
    ESP_LOGI(TAG, "%s | %s | %d", l1 ? l1 : "", l2 ? l2 : "", pct);
}

static bool tomar(void)
{
    bool ok = false;
    xSemaphoreTake(s_mx, portMAX_DELAY);
    if (!s_ocupado && !s_reiniciar_ms) { s_ocupado = true; ok = true; }
    xSemaphoreGive(s_mx);
    return ok;
}
static void soltar(void) { s_ocupado = false; }

static void linea_wifi(char *b, size_t n)
{
    if (!s_wifi.ssid[0]) snprintf(b, n, "SIN WI-FI DE CASA");
    else snprintf(b, n, "WI-FI: %s%s", s_wifi.ssid, s_conectado ? "" : " (SIN CONEXIÓN)");
}

/* ------------------------------------------------------------------ Wi-Fi */

static void conectar_casa(void)
{
    if (!s_wifi.ssid[0]) return;
    wifi_config_t c = {0};
    snprintf((char *)c.sta.ssid, sizeof c.sta.ssid, "%s", s_wifi.ssid);
    snprintf((char *)c.sta.password, sizeof c.sta.password, "%s", s_wifi.clave);
    c.sta.threshold.authmode = s_wifi.clave[0] ? WIFI_AUTH_WEP : WIFI_AUTH_OPEN;
    c.sta.pmf_cfg.capable = true;
    esp_wifi_disconnect();
    esp_wifi_set_config(WIFI_IF_STA, &c);
    s_intentos = 0;
    esp_wifi_connect();
}

static void evento(void *arg, esp_event_base_t base, int32_t id, void *datos)
{
    (void)arg;
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        if (s_wifi.ssid[0]) esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        s_conectado = false;
        /* pocos intentos: mientras busca la red de la casa, la red propia (la del celular) se corta a ratos */
        if (s_wifi.ssid[0] && ++s_intentos <= 4) esp_wifi_connect();
        else if (s_wifi.ssid[0] && s_intentos == 5) {
            char b[96];
            snprintf(b, sizeof b, "NO PUDE ENTRAR A %s: REVISA LA CLAVE EN EL CELULAR", s_wifi.ssid);
            estado("SIN INTERNET", b, -1);
        }
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        s_conectado = true;
        s_intentos = 0;
        s_buscar = true; /* recién conectado: mira si hay versión nueva */
        if (s_tarea) xTaskNotifyGive(s_tarea);
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_AP_STACONNECTED) {
        uso();
        estado("CELULAR CONECTADO", "ABRE 192.168.4.1 EN EL NAVEGADOR", -1);
    }
    (void)datos;
}

/* (1.0.11) sin ESP_ERROR_CHECK: si algo falla (en la 1.0.09 esp_wifi_init daba «sin memoria» y la placa entraba en
 * pánico) se anota, se avisa y se vuelve al juego */
static esp_err_t wifi_iniciar(void)
{
    esp_err_t e = esp_netif_init();
    if (e != ESP_OK) return e;
    e = esp_event_loop_create_default();
    if (e != ESP_OK && e != ESP_ERR_INVALID_STATE) return e;
    esp_netif_create_default_wifi_ap();
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    e = esp_wifi_init(&cfg);
    if (e != ESP_OK) return e;
    esp_wifi_set_storage(WIFI_STORAGE_RAM); /* nada del Wi-Fi a la memoria: ahí vive la partida */
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, evento, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, evento, NULL);
    e = esp_wifi_set_mode(WIFI_MODE_APSTA);
    if (e != ESP_OK) return e;
    ota_nombre_red(s_ap, sizeof s_ap);
    wifi_config_t ap = {0};
    snprintf((char *)ap.ap.ssid, sizeof ap.ap.ssid, "%s", s_ap);
    ap.ap.ssid_len = (uint8_t)strlen(s_ap);
    snprintf((char *)ap.ap.password, sizeof ap.ap.password, "%s", OTA_AP_CLAVE);
    ap.ap.authmode = WIFI_AUTH_WPA2_PSK;
    ap.ap.max_connection = 2;
    ap.ap.channel = 1;
    e = esp_wifi_set_config(WIFI_IF_AP, &ap);
    if (e != ESP_OK) return e;
    if (s_wifi.ssid[0]) {
        wifi_config_t c = {0};
        snprintf((char *)c.sta.ssid, sizeof c.sta.ssid, "%s", s_wifi.ssid);
        snprintf((char *)c.sta.password, sizeof c.sta.password, "%s", s_wifi.clave);
        c.sta.threshold.authmode = s_wifi.clave[0] ? WIFI_AUTH_WEP : WIFI_AUTH_OPEN;
        esp_wifi_set_config(WIFI_IF_STA, &c);
    }
    return esp_wifi_start();
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
        } else ESP_LOGW(TAG, "version.json: estado %d, %d bytes", st, n);
    }
    free(buf);
    esp_http_client_close(c);
    esp_http_client_cleanup(c);
    return ok;
}

static void descargar(const char *ver)
{
    char t[96];
    snprintf(t, sizeof t, "BAJANDO LA %s...", ver);
    estado(t, "NO LO APAGUES", 0);
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
    if (e != ESP_OK) { diag("descarga: no partió (error 0x%x), RAM %u KB", (unsigned)e, ram_kb()); estado("NO SE PUDO BAJAR", "PRUEBA DE NUEVO", -1); return; }
    esp_app_desc_t d;
    e = esp_https_ota_get_img_desc(h, &d);
    if (e != ESP_OK || strcmp(d.project_name, esp_app_get_description()->project_name)) {
        esp_https_ota_abort(h);
        estado("ESE ARCHIVO NO ES DEL ORBE", "NO SE INSTALÓ NADA", -1);
        return;
    }
    const int total = esp_https_ota_get_image_size(h);
    int ult = -1;
    while ((e = esp_https_ota_perform(h)) == ESP_ERR_HTTPS_OTA_IN_PROGRESS) {
        const int leido = esp_https_ota_get_image_len_read(h);
        const int pct = total > 0 ? (int)((int64_t)leido * 100 / total) : 0;
        if (pct != ult) { ult = pct; estado(NULL, NULL, pct); uso(); }
    }
    if (e == ESP_OK && esp_https_ota_is_complete_data_received(h)) e = esp_https_ota_finish(h);
    else { esp_https_ota_abort(h); if (e == ESP_OK) e = ESP_FAIL; }
    if (e != ESP_OK) { diag("descarga: falló (error 0x%x)", (unsigned)e); estado("LA DESCARGA FALLÓ", "SIGUE LA VERSIÓN DE ANTES. PRUEBA DE NUEVO", -1); return; }
    snprintf(t, sizeof t, "¡LISTO! INSTALADA LA %s", d.version);
    estado(t, "REINICIANDO...", 100);
    ESP_LOGI(TAG, "bajada la %s desde internet (estaba en la %s)", d.version, FW_VERSION);
    s_reiniciar_ms = ms() + 2500;
}

static void buscar(void)
{
    if (!s_conectado) {
        char b[96];
        linea_wifi(b, sizeof b);
        estado(s_wifi.ssid[0] ? "CONECTANDO A INTERNET..." : "FALTA EL WI-FI DE CASA", s_wifi.ssid[0] ? b : "ELÍGELO DESDE EL CELULAR", -1);
        return;
    }
    if (!tomar()) return;
    estado("BUSCANDO LA ÚLTIMA VERSIÓN...", "", -1);
    char ver[33] = "", nombre[64] = "";
    if (!leer_version_publicada(ver, sizeof ver, nombre, sizeof nombre)) {
        diag("version.json: no se pudo leer (RAM %u KB)", ram_kb());
        estado("NO PUDE VER LA ÚLTIMA VERSIÓN", "¿HAY INTERNET? PRUEBA DE NUEVO", -1);
        soltar();
        return;
    }
    snprintf(s_ultima, sizeof s_ultima, "%s", ver);
    if (fw_codigo(ver) <= fw_codigo(FW_VERSION)) {
        char b[96];
        snprintf(b, sizeof b, "LA ÚLTIMA PUBLICADA ES LA %s", ver);
        estado("¡ESTÁS AL DÍA!", b, -1);
    } else {
        descargar(ver);
    }
    soltar();
}

static void tarea_ota(void *arg)
{
    (void)arg;
    while (true) {
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1000));
        if (s_buscar) { s_buscar = false; buscar(); }
    }
}

/* ------------------------------------------------------------------ la página para el celular */

static const char PAGINA[] =
    "<!DOCTYPE html><html lang=es><head><meta charset=utf-8><meta name=viewport content='width=device-width,initial-scale=1'>"
    "<title>OrbePet</title><style>"
    "body{font-family:system-ui,sans-serif;background:#1b1530;color:#fff;margin:0;padding:16px;max-width:520px;margin:auto}"
    "h1{font-size:22px;color:#ffe27a;margin:4px 0 12px}section{background:#2a2148;border-radius:14px;padding:14px;margin:12px 0}"
    "h2{font-size:16px;margin:0 0 8px;color:#ffe27a}input,select,button{font-size:16px;width:100%;box-sizing:border-box;padding:10px;"
    "margin:5px 0;border-radius:10px;border:0}button{background:#2e9e50;color:#fff;font-weight:600}button.b2{background:#5a4a8a}"
    "#est{font-size:15px;line-height:1.4}.bar{height:12px;background:#433868;border-radius:6px;overflow:hidden;margin-top:6px}"
    ".bar div{height:100%;width:0;background:#ffe27a}small{color:#bbb}</style></head><body>"
    "<h1>OrbePet · actualizar</h1>"
    "<section><h2>Estado</h2><div id=est>...</div><div class=bar><div id=pb></div></div></section>"
    "<section><h2>Buscar en internet</h2><button onclick=buscar()>Buscar actualización</button>"
    "<small>Usa el Wi-Fi de la casa. Si hay una versión más nueva, la baja y se reinicia sola.</small></section>"
    "<section><h2>Wi-Fi de la casa</h2><select id=redes><option>Buscando redes...</option></select>"
    "<input id=ssid placeholder='Nombre de la red'><input id=clave type=password placeholder='Clave'>"
    "<button onclick=guardar()>Guardar y conectar</button><button class=b2 onclick=redes()>Volver a buscar redes</button></section>"
    "<section><h2>Subir un archivo .bin</h2><input type=file id=bin accept='.bin'><button onclick=subir()>Instalar este archivo</button>"
    "<small>El mascota.bin que sale al compilar (o el de la página de versiones de GitHub).</small></section>"
    "<section><button class=b2 onclick=\"fetch('/salir',{method:'POST'})\">Salir y volver al juego</button></section>"
    "<script>"
    "const $=i=>document.getElementById(i);"
    "async function est(){try{const r=await fetch('/estado');const j=await r.json();"
    "$('est').innerHTML='Versión instalada: <b>'+j.version+'</b>'+(j.ultima?' · última publicada: <b>'+j.ultima+'</b>':'')+'<br>'+j.wifi+'<br>'+j.l1+(j.l2?'<br>'+j.l2:'');"
    "$('pb').style.width=(j.pct<0?0:j.pct)+'%';}catch(e){}}"
    "async function redes(){$('redes').innerHTML='<option>Buscando redes...</option>';try{const r=await fetch('/redes');const l=await r.json();"
    "$('redes').innerHTML='<option value=\"\">Elige tu red...</option>'+l.map(s=>'<option>'+s.replace(/</g,'&lt;')+'</option>').join('');}catch(e){}}"
    "$('redes').onchange=e=>{if(e.target.value)$('ssid').value=e.target.value};"
    "function guardar(){fetch('/wifi',{method:'POST',body:new URLSearchParams({ssid:$('ssid').value,clave:$('clave').value})}).then(est)}"
    "function buscar(){fetch('/buscar',{method:'POST'}).then(est)}"
    "function subir(){const f=$('bin').files[0];if(!f){alert('Elige el archivo .bin');return}"
    "const x=new XMLHttpRequest();x.open('POST','/subir');x.upload.onprogress=e=>{if(e.lengthComputable)$('pb').style.width=(e.loaded*100/e.total)+'%'};"
    "x.onload=()=>{$('est').innerHTML=x.responseText};x.onerror=()=>{$('est').innerHTML='Se cortó la conexión'};x.send(f)}"
    "est();redes();setInterval(est,1500);"
    "</script></body></html>";

static esp_err_t h_pagina(httpd_req_t *r)
{
    uso();
    httpd_resp_set_type(r, "text/html; charset=utf-8");
    return httpd_resp_send(r, PAGINA, HTTPD_RESP_USE_STRLEN);
}

/* escapa comillas y barras para meter un texto en JSON */
static void json_esc(char *dst, size_t n, const char *s)
{
    size_t k = 0;
    for (; *s && k + 2 < n; s++) {
        if (*s == '"' || *s == '\\') dst[k++] = '\\';
        dst[k++] = *s;
    }
    dst[k] = 0;
}

static esp_err_t h_estado(httpd_req_t *r)
{
    char w[96], l1[200], l2[200], wf[200], ul[40];
    linea_wifi(w, sizeof w);
    xSemaphoreTake(s_mx, portMAX_DELAY);
    json_esc(l1, sizeof l1, s_linea1);
    json_esc(l2, sizeof l2, s_linea2);
    json_esc(ul, sizeof ul, s_ultima);
    const int pct = s_pct;
    xSemaphoreGive(s_mx);
    json_esc(wf, sizeof wf, w);
    char *b = malloc(900);
    if (!b) return httpd_resp_send_500(r);
    snprintf(b, 900, "{\"version\":\"%s\",\"nombre\":\"%s\",\"ultima\":\"%s\",\"wifi\":\"%s\",\"l1\":\"%s\",\"l2\":\"%s\",\"pct\":%d}",
             FW_VERSION, FW_NOMBRE, ul, wf, l1, l2, pct);
    httpd_resp_set_type(r, "application/json");
    esp_err_t e = httpd_resp_send(r, b, HTTPD_RESP_USE_STRLEN);
    free(b);
    return e;
}

static esp_err_t h_redes(httpd_req_t *r)
{
    uso();
    wifi_scan_config_t sc = {0};
    uint16_t n = 0;
    if (!s_ocupado && esp_wifi_scan_start(&sc, true) == ESP_OK) esp_wifi_scan_get_ap_num(&n);
    if (n > 20) n = 20;
    wifi_ap_record_t *ap = n ? calloc(n, sizeof *ap) : NULL;
    if (ap) esp_wifi_scan_get_ap_records(&n, ap);
    else { n = 0; esp_wifi_clear_ap_list(); }
    char *b = malloc(20 * 70 + 8);
    if (!b) { free(ap); return httpd_resp_send_500(r); }
    size_t k = 0;
    b[k++] = '[';
    for (int i = 0; i < n; i++) {
        if (!ap[i].ssid[0]) continue;
        bool rep = false;
        for (int j = 0; j < i && !rep; j++) rep = !strcmp((char *)ap[j].ssid, (char *)ap[i].ssid);
        if (rep) continue;
        char e[70];
        json_esc(e, sizeof e, (char *)ap[i].ssid);
        k += (size_t)snprintf(b + k, 20 * 70 + 8 - k, "%s\"%s\"", k > 1 ? "," : "", e);
    }
    b[k++] = ']';
    b[k] = 0;
    free(ap);
    httpd_resp_set_type(r, "application/json");
    esp_err_t e = httpd_resp_send(r, b, HTTPD_RESP_USE_STRLEN);
    free(b);
    return e;
}

/* lee un campo de un formulario urlencoded (con %XX y +) */
static void campo(const char *q, const char *nombre, char *out, size_t n)
{
    out[0] = 0;
    char k[16];
    snprintf(k, sizeof k, "%s=", nombre);
    const char *p = q;
    while ((p = strstr(p, k)) != NULL && p != q && p[-1] != '&') p++;
    if (!p) return;
    p += strlen(k);
    size_t i = 0;
    while (*p && *p != '&' && i + 1 < n) {
        if (*p == '+') { out[i++] = ' '; p++; }
        else if (*p == '%' && p[1] && p[2]) { char h[3] = {p[1], p[2], 0}; out[i++] = (char)strtol(h, NULL, 16); p += 3; }
        else out[i++] = *p++;
    }
    out[i] = 0;
}

static esp_err_t h_wifi(httpd_req_t *r)
{
    uso();
    char q[300] = "";
    const int n = r->content_len < (int)sizeof q - 1 ? r->content_len : (int)sizeof q - 1;
    int k = 0;
    while (k < n) { const int m = httpd_req_recv(r, q + k, (size_t)(n - k)); if (m <= 0) break; k += m; }
    q[k] = 0;
    ota_wifi_t w = {0};
    campo(q, "ssid", w.ssid, sizeof w.ssid);
    campo(q, "clave", w.clave, sizeof w.clave);
    if (!w.ssid[0]) return httpd_resp_sendstr(r, "Falta el nombre de la red");
    s_wifi = w;
    hal_storage_set("wifi", &s_wifi, sizeof s_wifi);
    char b[96];
    snprintf(b, sizeof b, "CONECTANDO A %s...", w.ssid);
    estado("WI-FI GUARDADO", b, -1);
    conectar_casa();
    return httpd_resp_sendstr(r, "OK");
}

static esp_err_t h_buscar(httpd_req_t *r)
{
    uso();
    s_buscar = true;
    if (s_tarea) xTaskNotifyGive(s_tarea);
    return httpd_resp_sendstr(r, "OK");
}

static esp_err_t h_salir(httpd_req_t *r)
{
    s_salir = true;
    return httpd_resp_sendstr(r, "Volviendo al juego...");
}

/* el celular manda el .bin crudo (sin formulario): se escribe en la otra ranura a medida que llega */
static esp_err_t h_subir(httpd_req_t *r)
{
    uso();
    if (!tomar()) { httpd_resp_set_status(r, "409 Conflict"); return httpd_resp_sendstr(r, "El orbe está ocupado bajando otra versión"); }
    const esp_partition_t *p = esp_ota_get_next_update_partition(NULL);
    const int total = r->content_len;
    if (!p || total <= 0 || (size_t)total > p->size) {
        soltar();
        httpd_resp_set_status(r, "400 Bad Request");
        return httpd_resp_sendstr(r, "El archivo está vacío o es más grande que el espacio del programa");
    }
    enum { CAB = sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t) + sizeof(esp_app_desc_t) };
    uint8_t *buf = malloc(4096);
    uint8_t cab[CAB];
    int ncab = 0, recib = 0, errores = 0;
    esp_ota_handle_t oh = 0;
    bool empezado = false;
    const char *falla = NULL;
    char ver[33] = "?";
    estado("RECIBIENDO DEL CELULAR...", "NO LO APAGUES", 0);
    while (buf && recib < total && !falla) {
        int n = httpd_req_recv(r, (char *)buf, (size_t)(total - recib < 4096 ? total - recib : 4096));
        if (n == HTTPD_SOCK_ERR_TIMEOUT && ++errores < 5) continue;
        if (n <= 0) { falla = "Se cortó la conexión"; break; }
        errores = 0;
        int desde = 0;
        if (!empezado) { /* junta la cabecera para revisar que es un programa del orbe antes de borrar nada */
            const int c = n < CAB - ncab ? n : CAB - ncab;
            memcpy(cab + ncab, buf, (size_t)c);
            ncab += c;
            desde = c;
            if (ncab == CAB || recib + n == total) {
                const esp_app_desc_t *d = (const esp_app_desc_t *)(cab + sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t));
                if (ncab < CAB || cab[0] != ESP_IMAGE_HEADER_MAGIC || d->magic_word != ESP_APP_DESC_MAGIC_WORD) falla = "Ese archivo no es un programa del ESP32";
                else if (strncmp(d->project_name, esp_app_get_description()->project_name, sizeof d->project_name)) falla = "Ese programa no es del orbe (proyecto distinto)";
                else {
                    snprintf(ver, sizeof ver, "%s", d->version);
                    if (esp_ota_begin(p, OTA_WITH_SEQUENTIAL_WRITES, &oh) != ESP_OK) falla = "No se pudo preparar la memoria";
                    else if (esp_ota_write(oh, cab, (size_t)ncab) != ESP_OK) falla = "No se pudo escribir";
                    else empezado = true;
                }
            }
        }
        if (!falla && empezado && n > desde && esp_ota_write(oh, buf + desde, (size_t)(n - desde)) != ESP_OK) falla = "No se pudo escribir";
        recib += n;
        estado(NULL, NULL, (int)((int64_t)recib * 100 / total));
    }
    free(buf);
    if (!buf) falla = "Sin memoria";
    if (!falla && empezado) {
        if (esp_ota_end(oh) != ESP_OK) { falla = "El archivo llegó dañado (no pasó la revisión)"; empezado = false; }
        else if (esp_ota_set_boot_partition(p) != ESP_OK) falla = "No se pudo elegir la versión nueva";
    }
    if (empezado && falla) esp_ota_abort(oh);
    char resp[160];
    if (falla) {
        estado("NO SE INSTALÓ", falla, -1);
        snprintf(resp, sizeof resp, "No se instaló: %s", falla);
        soltar();
        httpd_resp_set_status(r, "400 Bad Request");
        return httpd_resp_sendstr(r, resp);
    }
    char t[96];
    snprintf(t, sizeof t, "¡LISTO! INSTALADA LA %s", ver);
    estado(t, "REINICIANDO...", 100);
    ESP_LOGI(TAG, "instalada la %s desde el celular (estaba en la %s)", ver, FW_VERSION);
    snprintf(resp, sizeof resp, "¡Listo! Instalada la versión %s. El orbe se reinicia.", ver);
    s_reiniciar_ms = ms() + 2500;
    return httpd_resp_sendstr(r, resp);
}

static void web_iniciar(void)
{
    httpd_config_t c = HTTPD_DEFAULT_CONFIG();
    c.stack_size = 7168;
    c.max_uri_handlers = 10;
    c.lru_purge_enable = true;
    c.recv_wait_timeout = 15;
    httpd_handle_t s = NULL;
    const esp_err_t e = httpd_start(&s, &c);
    if (e != ESP_OK) { diag("servidor web: error 0x%x", (unsigned)e); return; }
    const httpd_uri_t U[] = {
        {.uri = "/", .method = HTTP_GET, .handler = h_pagina},
        {.uri = "/estado", .method = HTTP_GET, .handler = h_estado},
        {.uri = "/redes", .method = HTTP_GET, .handler = h_redes},
        {.uri = "/wifi", .method = HTTP_POST, .handler = h_wifi},
        {.uri = "/buscar", .method = HTTP_POST, .handler = h_buscar},
        {.uri = "/salir", .method = HTTP_POST, .handler = h_salir},
        {.uri = "/subir", .method = HTTP_POST, .handler = h_subir},
    };
    for (size_t i = 0; i < sizeof U / sizeof U[0]; i++) httpd_register_uri_handler(s, &U[i]);
}

/* ------------------------------------------------------------------ la pantalla del orbe */

typedef struct {
    lv_obj_t *img;
    art_img_t art;
} rotulo_t;
static rotulo_t s_r[6];
static lv_obj_t *s_barra, *s_barra_fill;

static void rotulo(int i, const char *txt, uint32_t color, int escala, int y)
{
    rotulo_t *r = &s_r[i];
    if (!r->img) r->img = lv_image_create(lv_screen_active());
    art_texto_centrado(&r->art, txt, color, escala, 340);
    lv_image_set_src(r->img, &r->art.dsc);
    lv_obj_align(r->img, LV_ALIGN_TOP_MID, 0, y);
    lv_obj_invalidate(r->img);
}

static void salir_cb(lv_event_t *e) { (void)e; s_salir = true; }

static void pantalla_crear(void)
{
    lv_obj_t *scr = lv_screen_active();
    lv_obj_clean(scr);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x1B1530), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    rotulo(0, "ACTUALIZAR", 0xFFE27A, 3, 46);
    rotulo(1, "VERSIÓN " FW_VERSION, 0xFFFFFF, 2, 92);
    s_barra = lv_obj_create(scr);
    lv_obj_remove_style_all(s_barra);
    lv_obj_set_size(s_barra, 280, 14);
    lv_obj_align(s_barra, LV_ALIGN_TOP_MID, 0, 214);
    lv_obj_set_style_bg_color(s_barra, lv_color_hex(0x433868), 0);
    lv_obj_set_style_bg_opa(s_barra, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(s_barra, 7, 0);
    s_barra_fill = lv_obj_create(s_barra);
    lv_obj_remove_style_all(s_barra_fill);
    lv_obj_set_size(s_barra_fill, 0, 14);
    lv_obj_set_style_bg_color(s_barra_fill, lv_color_hex(0xFFE27A), 0);
    lv_obj_set_style_bg_opa(s_barra_fill, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(s_barra_fill, 7, 0);
    lv_obj_add_flag(s_barra, LV_OBJ_FLAG_HIDDEN);
    char b[160];
    snprintf(b, sizeof b, "DESDE EL CELULAR:\nRED %s\nCLAVE " OTA_AP_CLAVE "\nY ABRE 192.168.4.1", s_ap);
    rotulo(4, b, 0xB9B0E0, 2, 244);
    lv_obj_t *bt = lv_obj_create(scr);
    lv_obj_remove_style_all(bt);
    lv_obj_set_size(bt, 150, 50);
    lv_obj_align(bt, LV_ALIGN_TOP_MID, 0, 372);
    lv_obj_set_style_bg_color(bt, lv_color_hex(0x5A4A8A), 0);
    lv_obj_set_style_bg_opa(bt, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(bt, 16, 0);
    lv_obj_add_flag(bt, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(bt, salir_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *bi = lv_image_create(bt);
    art_texto(&s_r[5].art, "SALIR", 0xFFFFFF, 2, 0);
    lv_image_set_src(bi, &s_r[5].art.dsc);
    lv_obj_center(bi);
}

static void pantalla_refrescar(void)
{
    char l1[96], l2[96];
    xSemaphoreTake(s_mx, portMAX_DELAY);
    snprintf(l1, sizeof l1, "%s", s_linea1);
    snprintf(l2, sizeof l2, "%s", s_linea2);
    const int pct = s_pct;
    s_cambio = false;
    xSemaphoreGive(s_mx);
    if (!l2[0]) linea_wifi(l2, sizeof l2);
    rotulo(2, l1, 0xFFFFFF, 2, 134);
    rotulo(3, l2, 0xB9B0E0, 2, 168);
    if (pct < 0) lv_obj_add_flag(s_barra, LV_OBJ_FLAG_HIDDEN);
    else { lv_obj_remove_flag(s_barra, LV_OBJ_FLAG_HIDDEN); lv_obj_set_width(s_barra_fill, 280 * (pct > 100 ? 100 : pct) / 100); }
}

/* ------------------------------------------------------------------ el modo */

void ota_modo(void)
{
    s_mx = xSemaphoreCreateMutex();
    ESP_LOGI(TAG, "modo actualizar (versión %s)", FW_VERSION); /* (sin el registro del juego: sus archivos no se montan en este modo) */
    size_t n = sizeof s_wifi;
    if (!hal_storage_get("wifi", &s_wifi, &n) || n != sizeof s_wifi) memset(&s_wifi, 0, sizeof s_wifi);
    s_wifi.ssid[sizeof s_wifi.ssid - 1] = 0;
    s_wifi.clave[sizeof s_wifi.clave - 1] = 0;

    /* (1.0.11) ORDEN: la pantalla (LVGL y sus tareas) se come ~70 KB de RAM interna; en la 1.0.09 se iniciaba primero y
     * al Wi-Fi ya no le quedaba (esp_wifi_init: sin memoria → pánico). Ahora: Wi-Fi, página y tarea primero; la pantalla
     * después y solo si sobra memoria. Sin pantalla igual funciona: todo se ve en la página del celular. */
    s_diag_marca = 0;
    diag("v%s: RAM interna al entrar %u KB", FW_VERSION, ram_kb());
    ota_nombre_red(s_ap, sizeof s_ap);
    esp_err_t e = wifi_iniciar();
    if (e == ESP_OK) diag("wifi ok, quedan %u KB", ram_kb()); else diag("wifi: error 0x%x (0x101 = sin memoria), quedan %u KB", (unsigned)e, ram_kb());
    if (e != ESP_OK) {
        lv_display_t *d0 = ram_kb() >= 80 ? hal_display_init() : NULL;
        estado("NO SE PUDO PRENDER EL WI-FI", "VUELVO AL JUEGO", -1);
        if (d0 && hal_display_lock()) { pantalla_crear(); pantalla_refrescar(); hal_display_unlock(); }
        vTaskDelay(pdMS_TO_TICKS(6000));
        esp_restart();
    }
    if (s_wifi.ssid[0]) { char b[96]; snprintf(b, sizeof b, "CONECTANDO A %s...", s_wifi.ssid); estado("BUSCANDO INTERNET...", b, -1); }
    else estado("FALTA EL WI-FI DE CASA", "ELÍGELO DESDE EL CELULAR", -1);
    web_iniciar();
    if (xTaskCreate(tarea_ota, "ota", 8192, NULL, 5, &s_tarea) != pdPASS) { s_tarea = NULL; diag("sin memoria para la tarea de descarga"); }
    lv_display_t *d = NULL;
    const unsigned antes = ram_kb();
    if (antes >= 84) d = hal_display_init(); /* la pantalla necesita ~70 KB y deja un margen para la conexión segura */
    diag("pantalla %s (había %u KB), quedan %u KB", d ? "sí" : "NO (falta memoria; todo se ve en el celular)", antes, ram_kb());
    if (d && hal_display_lock()) { pantalla_crear(); pantalla_refrescar(); hal_display_unlock(); }
    uso();

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(200));
        const uint32_t t = ms();
        if (s_cambio && d && hal_display_lock()) { pantalla_refrescar(); hal_display_unlock(); }
        if (!s_tarea && s_buscar) { s_buscar = false; buscar(); } /* sin tarea aparte: busca aquí mismo */
        const bool listo = s_reiniciar_ms && (int32_t)(t - s_reiniciar_ms) >= 0;
        const bool aburrido = !s_ocupado && t - s_ultimo_uso_ms > OTA_INACTIVO_MS;
        if (listo || (s_salir && !s_ocupado) || aburrido) {
            if (!listo) estado("VOLVIENDO AL JUEGO...", "", -1);
            if (d && hal_display_lock()) { pantalla_refrescar(); hal_display_unlock(); }
            vTaskDelay(pdMS_TO_TICKS(600));
            diag("salgo: %s, mínimo de RAM interna %u KB", listo ? "versión nueva instalada" : (s_salir ? "SALIR" : "15 min sin uso"),
                 (unsigned)(heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL) / 1024));
            esp_wifi_stop();
            esp_restart();
        }
    }
}
