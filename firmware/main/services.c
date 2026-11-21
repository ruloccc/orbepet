/*
 * Servicios de hardware para la Waveshare ESP32-S3-Touch-AMOLED-1.75:
 * pantalla (vía BSP), RTC, batería, acelerómetro, sonido, guardado y botón.
 */
#include <dirent.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#include "board.h"
#include "bsp/esp-bsp.h"
#include "esp_lv_adapter.h"
#include "src/display/lv_display_private.h" /* (1.0.25) inv_areas / inv_p: contar lo que de verdad se dibuja */
#include "driver/gpio.h"
#include "driver/usb_serial_jtag.h"
#include "driver/usb_serial_jtag_vfs.h"
#include "driver/i2c_master.h"
#include "esp_check.h"
#include "esp_codec_dev.h"
#include "esp_spiffs.h"
#include "esp_partition.h"
#include "esp_attr.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_sleep.h"
#include "esp_system.h"
#include "driver/rtc_io.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/idf_additions.h" /* (1.0.18) xTaskCreateWithCaps: la tarea del micrófono puede tener su pila en la PSRAM */
#include "esp_core_dump.h"
#include "hal.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "sfx.h"
static void spiffs_limpiar_pasos(const char *particion, int bytes);

static const char *TAG = "hal";

/* ------------------------------------------------------------ pantalla */

/* Medición de velocidad: cada 3 s escribe por el puerto serie cuántos cuadros dibujó y cuánto tardó cada uno. */
static int64_t s_ref_ini, s_ref_ult;
static int s_ref_n, s_ref_max_ms;
static volatile uint32_t s_main_ticks, s_usb_bytes, s_usb_lineas; /* latidos de la tarea principal y lo recibido por USB (diagnóstico) */
static uint32_t s_main_prev;
void hal_main_tick(void) { s_main_ticks++; }
static int s_vel_n, s_vel_prom, s_vel_max; /* la última ventana de 3 s, para los comandos de prueba */
static int64_t s_ref_suma;

int ui_panel_actual(void); /* (en ui.c) */
void hal_vel_info(int *cuadros, int *prom_ms, int *max_ms) { *cuadros = s_vel_n; *prom_ms = s_vel_prom; *max_ms = s_vel_max; }
/* v89: también cuántos pixeles se redibujan por cuadro y cuánto tardan las animaciones de la interfaz (lo mide ui.c) */
static uint64_t s_inv_px; static int s_vel_kpx;
extern volatile int g_ui_tick_ms, g_ui_tick_max;
static int s_vel_tick, s_vel_tick_max;
void hal_vel_info2(int *cuadros, int *prom_ms, int *max_ms, int *kpx, int *tick_ms, int *tick_max)
{
    *cuadros = s_vel_n; *prom_ms = s_vel_prom; *max_ms = s_vel_max; *kpx = s_vel_kpx; *tick_ms = s_vel_tick; *tick_max = s_vel_tick_max;
}
static void inv_evento(lv_event_t *e)
{
    (void)e; /* (1.0.25) ya no se cuenta aquí: se cuenta al empezar cada cuadro (refr_evento) */
}

/* ---------- registro dentro del dispositivo ----------
 * Lo que pasa (avisos, errores, reinicios y un latido cada 20 s) queda en el archivo registro.txt (los últimos ~24 KB),
 * así, si se pega o se reinicia, después se lee desde el computador (MASCOTA REGISTRO). */
#define LOG_ARCHIVO "registro.txt"
#define LOG_VIEJO "registro_v.txt" /* el archivo anterior: al llenarse el actual se renombra (sin copiar nada) */
#define LOG_MAX 12288             /* por archivo: entre los dos, hasta 24 KB */
#define LOG_RAM 12288 /* (v101) antes 6 KB: con la pantalla encendida se llenaba en ~1 min y, escribiendo cada 5 min, se perdía casi todo */
#define LOG_RAM_AVISO 9216 /* con esto en la RAM se pide escribir antes de tiempo (como mucho una vez cada 2 min) */
static EXT_RAM_BSS_ATTR char s_log_ram[LOG_RAM];
static int s_log_n;
static uint32_t s_log_flush_ms;
static vprintf_like_t s_log_prev;
static portMUX_TYPE s_log_mux = portMUX_INITIALIZER_UNLOCKED;

static void log_casi_lleno(void);
static bool log_anotar(const char *t, int n)
{
    portENTER_CRITICAL(&s_log_mux);
    if (n > LOG_RAM) { t += n - LOG_RAM; n = LOG_RAM; }
    if (s_log_n + n > LOG_RAM) { /* se llenó: se tira lo más viejo */
        int sobra = s_log_n + n - LOG_RAM;
        memmove(s_log_ram, s_log_ram + sobra, (size_t)(s_log_n - sobra));
        s_log_n -= sobra;
    }
    memcpy(s_log_ram + s_log_n, t, (size_t)n);
    s_log_n += n;
    const bool lleno = s_log_n > LOG_RAM_AVISO;
    portEXIT_CRITICAL(&s_log_mux);
    return lleno;
}

void hal_log(const char *fmt, ...)
{
    char b[420]; /* (antes 160: las líneas largas, como "actividad:", salían cortadas) */
    int k = snprintf(b, sizeof b, "[%u s] ", (unsigned)(esp_timer_get_time() / 1000000));
    va_list ap;
    va_start(ap, fmt);
    int m = vsnprintf(b + k, sizeof b - (size_t)k - 2, fmt, ap);
    va_end(ap);
    if (m < 0) return;
    k += m < (int)sizeof b - k - 2 ? m : (int)sizeof b - k - 2;
    b[k++] = '\n';
    if (log_anotar(b, k)) log_casi_lleno(); /* (solo desde hal_log: los avisos del sistema pueden venir de cualquier parte) */
}

/* ---------- resumen de eventos raros (v88) ----------
 * Lo pendiente vive en la RAM del RTC (sobrevive a un reinicio por fallo o watchdog) y la tarea del registro lo pasa a eventos.txt. */
#define EV_ARCHIVO "eventos.txt"
#define EV_MAX 8192
#define EV_RTC 1536
#define EV_MARCA 0x45564E54u
static RTC_NOINIT_ATTR uint32_t s_ev_marca, s_ev_n;
static RTC_NOINIT_ATTR char s_ev_rtc[EV_RTC];
static uint32_t s_ev_arranque; /* número de arranque (para distinguir los eventos de cada encendido) */
static bool s_rtc_confiable;
static uint32_t s_hora_guardada;
static TaskHandle_t s_log_tarea;
void hal_rtc_trusted(bool si) { s_rtc_confiable = si; }
void hal_evento(const char *fmt, ...)
{
    char b[300];
    const time_t ahora = time(NULL);
    struct tm tm;
    gmtime_r(&ahora, &tm);
    int k = snprintf(b, sizeof b, "#%u %02d/%02d %02d:%02d UTC [%u s] ", (unsigned)s_ev_arranque, tm.tm_mday, tm.tm_mon + 1, tm.tm_hour, tm.tm_min,
                     (unsigned)(esp_timer_get_time() / 1000000));
    va_list ap;
    va_start(ap, fmt);
    int m = vsnprintf(b + k, sizeof b - (size_t)k - 2, fmt, ap);
    va_end(ap);
    if (m < 0) return;
    k += m < (int)sizeof b - k - 2 ? m : (int)sizeof b - k - 2;
    b[k++] = '\n';
    b[k] = 0;
    hal_log("EVENTO %s", b); /* también en el registro normal */
    portENTER_CRITICAL(&s_log_mux);
    if (s_ev_marca != EV_MARCA || s_ev_n > EV_RTC) { s_ev_marca = EV_MARCA; s_ev_n = 0; }
    if (s_ev_n + (uint32_t)k > EV_RTC) { /* muchos seguidos sin pasar al archivo: se tira lo más viejo */
        const uint32_t sobra = s_ev_n + (uint32_t)k - EV_RTC;
        memmove(s_ev_rtc, s_ev_rtc + sobra, s_ev_n - sobra);
        s_ev_n -= sobra;
    }
    memcpy(s_ev_rtc + s_ev_n, b, (size_t)k);
    s_ev_n += (uint32_t)k;
    portEXIT_CRITICAL(&s_log_mux);
    if (s_log_tarea) xTaskNotifyGive(s_log_tarea); /* se guarda pronto (en la tarea del registro, sin trabar el juego) */
}

static int log_vprintf(const char *fmt, va_list ap)
{
    va_list c;
    va_copy(c, ap);
    char b[160];
    int n = vsnprintf(b, sizeof b, fmt, c);
    va_end(c);
    if (n > 0) { /* solo avisos y errores ("W (123) tag:" o "E (123) tag:", a veces con color delante) */
        const char *q = b;
        if (q[0] == 27) { while (*q && *q != 'm') q++; if (*q) q++; }
        if ((q[0] == 'E' || q[0] == 'W') && q[1] == ' ' && q[2] == '(') {
            int k = n < (int)sizeof b - 1 ? n : (int)sizeof b - 1;
            while (k > 0 && (b[k - 1] == '\n' || b[k - 1] == '\r' || b[k - 1] == 27 || b[k - 1] == '[' || b[k - 1] == '0' || b[k - 1] == 'm' || b[k - 1] == ';')) k--;
            if (k > 0 && (size_t)(q - b) < (size_t)k) { char u[170]; int w = snprintf(u, sizeof u, "%.*s\n", k - (int)(q - b), q); if (w > 0) log_anotar(u, w); }
        }
    }
    return s_log_prev ? s_log_prev(fmt, ap) : vprintf(fmt, ap);
}

static void log_al_apagar(void) { hal_log_flush(true); } /* en cualquier reinicio por software el registro se guarda: antes se perdía lo de los últimos 60 s */

void hal_log_init(void)
{
    static const char *const M[] = {"desconocido", "encendido", "reinicio externo", "reinicio por software", "PÁNICO (fallo del programa)",
                                    "watchdog de interrupciones", "watchdog de tareas", "otro watchdog", "salió del sueño", "brownout (poca energía)",
                                    "SDIO", "USB", "JTAG", "EFUSE", "power glitch", "CPU lock"};
    int r = (int)esp_reset_reason();
    if (r == ESP_RST_POWERON || s_ev_marca != EV_MARCA || s_ev_n > EV_RTC) { s_ev_marca = EV_MARCA; s_ev_n = 0; } /* al enchufar, la RAM del RTC trae basura */
    { uint32_t na = 0; size_t ln = sizeof na; if (hal_storage_get("arranques", &na, &ln) && ln == sizeof na) s_ev_arranque = na; s_ev_arranque++; hal_storage_set("arranques", &s_ev_arranque, sizeof s_ev_arranque); }
    hal_log("=== ARRANQUE: %s", r >= 0 && r < (int)(sizeof M / sizeof M[0]) ? M[r] : "?");
    hal_evento("ARRANQUE: %s", r >= 0 && r < (int)(sizeof M / sizeof M[0]) ? M[r] : "?");
    /* si hubo un fallo, el informe guardado dice en qué tarea y en qué parte del programa (el probador lo traduce a archivo y línea) */
    const bool reinicio_por_fallo = r == ESP_RST_PANIC || r == ESP_RST_INT_WDT || r == ESP_RST_TASK_WDT || r == ESP_RST_WDT;
    if (esp_core_dump_image_check() == ESP_OK && !reinicio_por_fallo) hal_log("(hay un informe de un fallo anterior guardado; ya se anotó en su momento)");
    else if (esp_core_dump_image_check() == ESP_OK) {
        static esp_core_dump_summary_t sm;
        if (esp_core_dump_get_summary(&sm) == ESP_OK) {
            char bt[220];
            int n = 0;
            for (int i = 0; i < (int)sm.exc_bt_info.depth && i < 16 && n < (int)sizeof bt - 12; i++)
                n += snprintf(bt + n, sizeof bt - (size_t)n, " 0x%08x", (unsigned)sm.exc_bt_info.bt[i]);
            bt[n] = 0;
            hal_evento("FALLO: tarea %s, pc 0x%08x, causa %u, dirección 0x%08x", sm.exc_task, (unsigned)sm.exc_pc, (unsigned)sm.ex_info.exc_cause, (unsigned)sm.ex_info.exc_vaddr);
            hal_evento("FALLO_BT:%s", bt);
            /* v88: el informe ya NO se borra aquí: antes, si el registro se desbordaba, el fallo se perdía para siempre. Queda en el
             * resumen de eventos y el informe completo se conserva hasta el próximo fallo (el probador lo lee con «Informe completo del fallo»). */
        } else hal_evento("FALLO: hay un informe guardado pero no se pudo leer aquí (se conserva: el probador lo lee con «Informe completo del fallo»)");
    }
    s_log_prev = esp_log_set_vprintf(log_vprintf);
    esp_register_shutdown_handler(log_al_apagar);
}

static char s_ram_fases[220]; /* v88: la PSRAM libre al comienzo de cada fase del arranque, en una línea (va al resumen de eventos) */
void hal_ram_log(const char *donde)
{
    const unsigned ri = (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL), ps = (unsigned)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024),
                   pm = (unsigned)(heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM) / 1024);
    ESP_LOGI("ram", "%-12s interna libre %u B (bloque mayor %u B), la mayor mínima %u B; PSRAM %u KB (bloque mayor %u KB)",
             donde, ri, (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL), (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL), ps, pm);
    hal_log("memoria fase %s: PSRAM libre %u KB (bloque mayor %u KB), RAM interna %u B", donde, ps, pm, ri); /* antes solo iba al puerto serie */
    const size_t k = strlen(s_ram_fases);
    if (k < sizeof s_ram_fases - 24) snprintf(s_ram_fases + k, sizeof s_ram_fases - k, "%s%s %u KB", k ? ", " : "", donde, ps);
}
const char *hal_ram_fases(void) { return s_ram_fases; }
static void refr_evento(lv_event_t *e)
{
    int64_t ahora = esp_timer_get_time();
    if (lv_event_get_code(e) == LV_EVENT_REFR_START) {
        s_ref_ini = ahora;
        /* (1.0.25) lo que de verdad se dibuja en este cuadro: las zonas ya juntadas por LVGL (antes se sumaba cada pedido,
         * aunque cayera dentro de otro, y el registro decía 826 kpx con una pantalla de 217) */
        lv_display_t *d = (lv_display_t *)lv_event_get_current_target(e);
        if (d) for (uint32_t i = 0; i < d->inv_p; i++)
            if (!d->inv_area_joined[i]) s_inv_px += (uint64_t)lv_area_get_width(&d->inv_areas[i]) * (uint64_t)lv_area_get_height(&d->inv_areas[i]);
        return;
    }
    int ms = (int)((ahora - s_ref_ini) / 1000);
    s_ref_n++;
    s_ref_suma += ms;
    if (ms > s_ref_max_ms) s_ref_max_ms = ms;
    if (ahora - s_ref_ult > 3000000) {
        ESP_LOGI("velocidad", "%d cuadros en 3 s; cada uno tarda %d ms (máx %d ms); RAM interna libre %u B (mínima %u B), PSRAM %u KB; panel %d; principal %u vueltas; usb %u bytes %u líneas",
                 s_ref_n, s_ref_n ? (int)(s_ref_suma / s_ref_n) : 0, s_ref_max_ms,
                 (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL), (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL),
                 (unsigned)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024), ui_panel_actual(),
                 (unsigned)(s_main_ticks - s_main_prev), (unsigned)s_usb_bytes, (unsigned)s_usb_lineas);
        s_main_prev = s_main_ticks;
        s_vel_n = s_ref_n; s_vel_prom = s_ref_n ? (int)(s_ref_suma / s_ref_n) : 0; s_vel_max = s_ref_max_ms;
        s_vel_kpx = s_ref_n ? (int)(s_inv_px / (uint64_t)s_ref_n / 1000u) : 0; s_inv_px = 0;
        s_vel_tick = g_ui_tick_ms; s_vel_tick_max = g_ui_tick_max; g_ui_tick_max = 0;
        if (s_vel_prom > 60 || s_vel_tick_max > 120) /* lento: queda en el registro (lo normal solo va en el latido "vivo") */
            hal_log("pantalla lenta: %d cuadros en 3 s, dibujar %d ms (máx %d), %d kpx por cuadro, animar %d ms (máx %d), panel %d", s_vel_n, s_vel_prom, s_vel_max, s_vel_kpx, s_vel_tick, s_vel_tick_max, ui_panel_actual());
        s_ref_ult = ahora; s_ref_n = 0; s_ref_suma = 0; s_ref_max_ms = 0;
    }
}

lv_display_t *hal_display_init_liviano(void) /* (1.0.13) el modo Wi-Fi */
{
    bsp_display_modo_liviano(true);
    lv_display_t *d = hal_display_init();
    if (d) bsp_display_brightness_set(100); /* (1.0.60) el modo Wi-Fi no pasa por el brillo del juego: a la vista de inmediato */
    return d;
}
lv_display_t *hal_display_init(void)
{
    lv_display_t *d = bsp_display_start(); /* también inicia I2C, táctil y LVGL */
    if (d) {
        /* (1.0.60) arranca APAGADA (brillo 0) y en negro: antes se encendía al 100 % con el fondo blanco de LVGL mientras
         * se armaba el juego (unos 3 s), también al despertar del sueño de noche. El juego pone su brillo al estar listo. */
        bsp_display_brightness_set(0);
        if (bsp_display_lock(0) == ESP_OK) {
            lv_obj_set_style_bg_color(lv_screen_active(), lv_color_black(), 0);
            lv_obj_set_style_bg_opa(lv_screen_active(), LV_OPA_COVER, 0);
            bsp_display_unlock();
        }
        /* (1.0.48) sin «baldosas»: con 2 unidades de dibujo, LVGL 9.4 parte cada franja en 2 baldosas que dibujan los dos
         * núcleos a la vez (lv_refr.c refr_area_part). Si el lv_malloc de las baldosas falla (memoria justa), esa franja NO
         * se dibuja y se manda igual con lo que quedó en el búfer: las rayas oscuras horizontales junto a los botones con
         * lluvia (muchas áreas chicas por cuadro). Con 1 baldosa la franja se dibuja entera, como en el simulador. */
        lv_display_set_tile_cnt(d, 1);
        lv_display_add_event_cb(d, refr_evento, LV_EVENT_REFR_START, NULL);
        lv_display_add_event_cb(d, refr_evento, LV_EVENT_REFR_READY, NULL);
        { extern void ui_inv_fusionar(lv_event_t *e); lv_display_add_event_cb(d, ui_inv_fusionar, LV_EVENT_INVALIDATE_AREA, NULL); } /* (1.0.22) después del redondeo del BSP */
        lv_display_add_event_cb(d, inv_evento, LV_EVENT_INVALIDATE_AREA, NULL);
    }
    return d;
}

/* la imagen se gira en el propio panel (y el táctil con ella): no depende del giro por software de LVGL */
void hal_display_rotate_q(int cuartos)
{
    cuartos &= 3;
#if BOARD_GIRO_90_INVERTIDO
    if (cuartos & 1) cuartos ^= 2;
#endif
    if (bsp_display_orient(cuartos) != ESP_OK) ESP_LOGW(TAG, "no se pudo girar la pantalla");
    lv_obj_invalidate(lv_screen_active());
}
void hal_display_rotate(bool al_reves) { hal_display_rotate_q(al_reves ? 2 : 0); }
void hal_display_brightness(int pct) { bsp_display_brightness_set(pct < 0 ? 0 : (pct > 100 ? 100 : pct)); }
bool hal_display_pause(bool pausar) { return (pausar ? esp_lv_adapter_pause(2000) : esp_lv_adapter_resume()) == ESP_OK; }
void hal_display_sleep(bool dormir)
{
#if BOARD_PANEL_REPOSO
    bsp_display_sleep(dormir);
#else
    (void)dormir;
#endif
}
bool hal_display_lock(void) { return bsp_display_lock((uint32_t)-1) == ESP_OK; }
void hal_display_unlock(void) { bsp_display_unlock(); }

/* ------------------------------------------------------- I2C auxiliar */

static esp_err_t i2c_dev(uint8_t addr, i2c_master_dev_handle_t *h)
{
    i2c_master_bus_handle_t bus = bsp_i2c_get_handle();
    ESP_RETURN_ON_FALSE(bus, ESP_ERR_INVALID_STATE, TAG, "bus I2C no iniciado");
    if (i2c_master_probe(bus, addr, 50) != ESP_OK) return ESP_ERR_NOT_FOUND;
    i2c_device_config_t cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = addr,
        .scl_speed_hz = BOARD_I2C_HZ,
    };
    return i2c_master_bus_add_device(bus, &cfg, h);
}

static esp_err_t leer(i2c_master_dev_handle_t h, uint8_t reg, uint8_t *buf, size_t n)
{
    return i2c_master_transmit_receive(h, &reg, 1, buf, n, 50);
}

static esp_err_t escribir(i2c_master_dev_handle_t h, uint8_t reg, uint8_t v)
{
    uint8_t b[2] = {reg, v};
    return i2c_master_transmit(h, b, 2, 50);
}

/* ------------------------------------------------------------------ RTC */

static i2c_master_dev_handle_t s_rtc;

static uint8_t bcd2bin(uint8_t v) { return (uint8_t)((v >> 4) * 10 + (v & 0x0F)); }
static uint8_t bin2bcd(int v) { return (uint8_t)(((v / 10) << 4) | (v % 10)); }

esp_err_t hal_rtc_init(void)
{
    if (s_rtc) return ESP_OK;
    esp_err_t e = i2c_dev(BOARD_RTC_ADDR, &s_rtc);
    if (e != ESP_OK) ESP_LOGW(TAG, "RTC no responde; se usa el reloj interno");
    return e;
}

uint32_t hal_rtc_now(void)
{
    uint8_t r[7];
    if (s_rtc && leer(s_rtc, 0x04, r, 7) == ESP_OK && !(r[0] & 0x80)) {
        struct tm t = {
            .tm_sec = bcd2bin(r[0] & 0x7F),
            .tm_min = bcd2bin(r[1] & 0x7F),
            .tm_hour = bcd2bin(r[2] & 0x3F),
            .tm_mday = bcd2bin(r[3] & 0x3F),
            .tm_mon = bcd2bin(r[5] & 0x1F) - 1,
            .tm_year = bcd2bin(r[6]) + 100,
        };
        time_t s = mktime(&t); /* la zona del sistema es UTC */
        if (t.tm_year >= 125) return (uint32_t)s;
    }
    /* Sin chip de reloj (la 1.75C puede no traerlo): el reloj interno, y la última hora
     * conocida se guarda en la memoria para no pedirla en cada encendido. */
    uint32_t t = (uint32_t)time(NULL);
    if (s_rtc) return t;
    static uint32_t guardada;
    if (t >= 1767225600u) {
        if (t - guardada >= 120u) { guardada = t; hal_storage_set("hora", &t, sizeof t); } /* cada 2 min: así un reinicio casi no pierde tiempo */
        return t;
    }
    uint32_t g = 0;
    size_t n = sizeof g;
    if (hal_storage_get("hora", &g, &n) && n == sizeof g && g >= 1767225600u) return g;
    return t;
}

esp_err_t hal_rtc_set(uint32_t unix_s)
{
    struct timeval tv = {.tv_sec = unix_s};
    settimeofday(&tv, NULL);
    if (!s_rtc) { hal_storage_set("hora", &unix_s, sizeof unix_s); return ESP_ERR_INVALID_STATE; }
    time_t s = unix_s;
    struct tm t;
    gmtime_r(&s, &t);
    uint8_t b[8] = {0x04, bin2bcd(t.tm_sec), bin2bcd(t.tm_min), bin2bcd(t.tm_hour), bin2bcd(t.tm_mday),
                    (uint8_t)t.tm_wday, bin2bcd(t.tm_mon + 1), bin2bcd(t.tm_year - 100)};
    return i2c_master_transmit(s_rtc, b, sizeof b, 50);
}

/* ------------------------------------------------------------- batería */
/* AXP2101: registros tomados de XPowersLib (biblioteca usada por Waveshare) */
#define AXP_STATUS1 0x00
#define AXP_STATUS2 0x01
#define AXP_ADC_CTRL 0x30
#define AXP_BAT_DET 0x68
#define AXP_BAT_PCT 0xA4

static i2c_master_dev_handle_t s_pmu;

static bool s_pwr_al_iniciar; /* (v101) */
esp_err_t hal_power_init(void)
{
    if (s_pmu) return ESP_OK;
    ESP_RETURN_ON_ERROR(i2c_dev(BOARD_PMU_ADDR, &s_pmu), TAG, "AXP2101 no responde");
    uint8_t v;
    if (leer(s_pmu, AXP_ADC_CTRL, &v, 1) == ESP_OK) {
        v |= 0x01;         /* medir voltaje de batería */
        v &= (uint8_t)~0x02; /* sin sensor de temperatura (TS) en esta placa */
        escribir(s_pmu, AXP_ADC_CTRL, v);
    }
    if (leer(s_pmu, AXP_BAT_DET, &v, 1) == ESP_OK) escribir(s_pmu, AXP_BAT_DET, v | 0x01);
    if (leer(s_pmu, 0x18, &v, 1) == ESP_OK) escribir(s_pmu, 0x18, v | 0x08); /* medidor de carga (gauge) encendido */
    /* el botón PWR de la placa: aviso de pulsación corta (INTEN2 bit 3); se lee por sondeo, sin usar la línea de interrupción */
    if (leer(s_pmu, 0x41, &v, 1) == ESP_OK) escribir(s_pmu, 0x41, v | 0x08);
    uint8_t limpiar = 0xFF;
    if (leer(s_pmu, 0x49, &v, 1) == ESP_OK) s_pwr_al_iniciar = (v & 0x08) != 0; /* (v101) un PWR apretado durante el sueño de noche */
    escribir(s_pmu, 0x49, limpiar); /* borra avisos viejos */
    return ESP_OK;
}

/* true una vez por cada pulsación corta del botón PWR (el que no es BOOT) */
bool hal_pwr_short_press(void)
{
    uint8_t st;
    if (!s_pmu || leer(s_pmu, 0x49, &st, 1) != ESP_OK || !(st & 0x08)) return false;
    escribir(s_pmu, 0x49, 0x08); /* se borra escribiendo 1 en el bit */
    return true;
}

/* voltaje de la batería en mV (ADC de la AXP2101: registros 0x34 y 0x35, 14 bits); -1 si no se puede leer */
int hal_battery_mv(void)
{
    uint8_t d[2];
    if (!s_pmu || leer(s_pmu, 0x34, d, 2) != ESP_OK) return -1;
    return ((d[0] & 0x3F) << 8) | d[1];
}

/* porcentaje según el voltaje (batería de litio en reposo, aproximado) */
static int pct_de_mv(int mv)
{
    static const int T[][2] = {{3300, 0}, {3500, 5}, {3600, 10}, {3700, 25}, {3750, 40}, {3800, 55}, {3900, 70}, {4000, 82}, {4100, 92}, {4200, 100}};
    if (mv <= T[0][0]) return 0;
    for (int i = 1; i < 10; i++)
        if (mv <= T[i][0]) return T[i - 1][1] + (T[i][1] - T[i - 1][1]) * (mv - T[i - 1][0]) / (T[i][0] - T[i - 1][0]);
    return 100;
}

bool hal_battery_vbus(void) /* hay un cable con corriente conectado (aunque la batería ya esté llena y no esté "cargando") */
{
    uint8_t st;
    return s_pmu && leer(s_pmu, AXP_STATUS1, &st, 1) == ESP_OK && (st & 0x20);
}

int hal_battery_stage(void) /* fase del cargador: 0 gotita, 1 precarga, 2 corriente constante, 3 voltaje constante, 4 TERMINÓ, 5 sin cargar; -1 si no se lee */
{
    uint8_t st;
    if (!s_pmu || leer(s_pmu, AXP_STATUS2, &st, 1) != ESP_OK) return -1;
    return st & 7;
}

bool hal_battery_full(void) /* enchufada y con la carga terminada (no depende del medidor, que puede estar descalibrado) */
{
    if (!hal_battery_vbus()) return false;
    const int fase = hal_battery_stage();
    if (fase == 4) return true;
    const int mv = hal_battery_mv();
    return !hal_battery_charging() && mv >= 4150; /* enchufada, ya no carga y está en 4,15 V o más */
}

int hal_battery_gauge_raw(void) /* lo que dice el medidor interno, sin correcciones (-1 si no se lee) */
{
    uint8_t pct;
    return s_pmu && leer(s_pmu, AXP_BAT_PCT, &pct, 1) == ESP_OK ? (pct > 100 ? 100 : pct) : -1;
}

int hal_battery_percent(void)
{
    uint8_t st, pct;
    if (!s_pmu || leer(s_pmu, AXP_STATUS1, &st, 1) != ESP_OK || !(st & 0x08)) return -1;
    if (leer(s_pmu, AXP_BAT_PCT, &pct, 1) != ESP_OK) return -1;
    int g = pct > 100 ? 100 : pct;
    /* el medidor de la AXP2101 queda en 0 % (o pegado) después de sacar y volver a poner la batería: si el voltaje dice otra cosa, se usa el voltaje */
    int mv = hal_battery_mv();
    if (mv > 3400 && mv < 4400) {
        /* cargando, el voltaje en el borne sube por la corriente del cargador (más en carga rápida, y se queda en 4,2 V en la fase final):
           si no se compensa, marca ~6 puntos de más (en el log: 90 % cargando, 84 % al desconectar = ~60 mV) y al desconectar el cable "baja de golpe" */
        int tope = 100;
        uint8_t s2 = 0;
        if (leer(s_pmu, AXP_STATUS2, &s2, 1) == ESP_OK && (s2 >> 5) == 0x01) {
            switch (s2 & 7) {
            case 0: case 1: mv -= 120; break;   /* precarga */
            case 2: mv -= 70;  break;           /* corriente constante */
            case 3: mv -= 25; tope = 96; break; /* voltaje constante: casi llena, falta la cola */
            default: break;
            }
        } else if ((s2 & 7) == 4) tope = 100;
        int v = pct_de_mv(mv);
        if (v > tope) v = tope;
        if (g == 0 && v > 3) g = v < 99 ? v : 99;
        else if (g < v - 30) g = v;
    }
    if (hal_battery_full()) return 100; /* el cargador dice que terminó: está al 100 aunque el medidor no lo sepa */
    return g;
}

bool hal_battery_charging(void)
{
    uint8_t st;
    return s_pmu && leer(s_pmu, AXP_STATUS2, &st, 1) == ESP_OK && (st >> 5) == 0x01;
}

/* ---------------------------------------------------------- acelerómetro */
/* QMI8658: CTRL1 0x02, CTRL2 0x03 (rango y frecuencia), CTRL7 0x08, datos desde 0x35 */
static i2c_master_dev_handle_t s_imu;
static float s_g_prev = 1.0f, s_ax_prev, s_ay_prev, s_az_prev;
static int s_picos;
static int64_t s_primer_pico, s_ultima_sacudida;

esp_err_t hal_imu_init(void)
{
    if (!s_imu) ESP_RETURN_ON_ERROR(i2c_dev(BOARD_IMU_ADDR, &s_imu), TAG, "QMI8658 no responde"); /* (1.0.54) el modo paseo ya pudo abrirlo */
    uint8_t id = 0;
    leer(s_imu, 0x00, &id, 1);
    if (id != 0x05) ESP_LOGW(TAG, "QMI8658: identificador inesperado 0x%02X", id);
    escribir(s_imu, 0x02, 0x40);           /* autoincremento de dirección */
    escribir(s_imu, 0x03, (1 << 4) | 6);   /* +-4 g, 125 Hz */
    escribir(s_imu, 0x08, 0x01);           /* acelerómetro encendido */
    return ESP_OK;
}

static float s_tilt_x, s_tilt_y;

#include "driver/temperature_sensor.h"
int hal_temp_chip(void)
{
    static temperature_sensor_handle_t h;
    static bool intento;
    if (!h && !intento) {
        intento = true;
        temperature_sensor_config_t c = TEMPERATURE_SENSOR_CONFIG_DEFAULT(-10, 80);
        if (temperature_sensor_install(&c, &h) != ESP_OK || temperature_sensor_enable(h) != ESP_OK) h = NULL;
    }
    float v;
    if (!h || temperature_sensor_get_celsius(h, &v) != ESP_OK) return -999;
    return (int)(v * 10.0f);
}
int hal_temp_imu(void)
{
    uint8_t d[2];
    if (!s_imu || leer(s_imu, 0x33, d, 2) != ESP_OK) return -999;
    return (int)((int16_t)(d[0] | d[1] << 8) * 10 / 256); /* QMI8658: 1/256 °C por unidad */
}

static bool s_imu_facil;
void hal_imu_modo_facil(bool si) { s_imu_facil = si; }
void hal_imu_poll(bool *sacudida, bool *movimiento)
{
    *sacudida = *movimiento = false;
    uint8_t d[6];
    if (!s_imu || leer(s_imu, 0x35, d, 6) != ESP_OK) return;
    const float esc = 1.0f / 8192.0f; /* 4 g de rango */
    float ax = (int16_t)(d[0] | d[1] << 8) * esc, ay = (int16_t)(d[2] | d[3] << 8) * esc, az = (int16_t)(d[4] | d[5] << 8) * esc;
    float g = sqrtf(ax * ax + ay * ay + az * az);
    int64_t ahora = esp_timer_get_time() / 1000;

    /* sacudida: cinco picos bien fuertes en menos de 1,2 s (poco sensible: sacudirla asusta a la mascota).
     * v90: en la pesca (modo fácil) bastan dos picos medianos en 1,2 s: el ciclo lee el acelerómetro cada 50-140 ms y con
     * cinco picos fuertes casi nunca se alcanzaba a ver la sacudida */
    if (fabsf(g - s_g_prev) > (s_imu_facil ? 0.8f : 2.0f)) {
        if (s_picos == 0 || ahora - s_primer_pico > 1200) { s_picos = 0; s_primer_pico = ahora; }
        if (++s_picos >= (s_imu_facil ? 2 : 5) && ahora - s_ultima_sacudida > (s_imu_facil ? 600 : 4000)) {
            *sacudida = true;
            s_ultima_sacudida = ahora;
            s_picos = 0;
        }
    }
    /* lo sacaron del bolsillo: la pantalla pasa a mirar hacia arriba (hacia ti) después de un rato de
     * costado (en el bolsillo va de canto), y se queda así un instante. Caminando no cuenta. */
    static int64_t s_de_canto_desde, s_mirando_desde;
    bool mirando = g > 0.5f && fabsf(az) / g > 0.65f && fabsf(g - 1.0f) < 0.35f;
    if (!mirando) { if (!s_de_canto_desde) s_de_canto_desde = ahora; s_mirando_desde = 0; }
    else {
        if (!s_mirando_desde) s_mirando_desde = ahora;
        if (s_de_canto_desde && ahora - s_de_canto_desde - (ahora - s_mirando_desde) > 1500 && ahora - s_mirando_desde > 300) {
            *movimiento = true;
            s_de_canto_desde = 0;
        }
        if (ahora - s_mirando_desde > 300) s_de_canto_desde = 0;
    }
    s_g_prev = g; s_ax_prev = ax; s_ay_prev = ay; s_az_prev = az;
    /* inclinación (filtrada para que no tiemble) */
    if (g > 0.3f) {
        s_tilt_x += (BOARD_IMU_SIGNO_X * ax / g - s_tilt_x) * 0.3f;
        s_tilt_y += (BOARD_IMU_SIGNO_Y * ay / g - s_tilt_y) * 0.3f;
    }
}

/* Contador de pasos del QMI8658 (motor propio del chip: casi no gasta). Parámetros del driver de referencia
 * de QST para 125 Hz; conviene afinarlos en la placa (sensibilidad en el bolsillo). */
static bool s_pasos_ok;
static bool qmi_ctrl9(uint8_t cmd)
{
    if (escribir(s_imu, 0x0A, cmd) != ESP_OK) return false;
    for (int i = 0; i < 50; i++) { /* espera CmdDone (STATUSINT bit 7) */
        uint8_t st = 0;
        if (leer(s_imu, 0x2D, &st, 1) == ESP_OK && (st & 0x80)) { escribir(s_imu, 0x0A, 0x00); return true; } /* ACK */
        vTaskDelay(pdMS_TO_TICKS(2));
    }
    return false;
}
bool hal_steps_init(void)
{
    if (!s_imu) return false;
    /* página 1: muestras por ventana, umbrales de pico a pico y de pico, tiempo máximo entre pasos */
    const uint8_t p1[8] = {0x32, 0x00, 0xCC, 0x00, 0x66, 0x00, 0xC8, 0x01};
    for (int i = 0; i < 8; i++) escribir(s_imu, (uint8_t)(0x0B + i), p1[i]);
    bool ok = qmi_ctrl9(0x0D);
    /* página 2: tiempo mínimo entre pasos, pasos para empezar a contar, precisión y cuenta de señal */
    const uint8_t p2[8] = {0x14, 0x0A, 0x00, 0x04, 0x00, 0x00, 0x00, 0x02};
    for (int i = 0; i < 8; i++) escribir(s_imu, (uint8_t)(0x0B + i), p2[i]);
    ok = qmi_ctrl9(0x0D) && ok;
    uint8_t c8 = 0;
    leer(s_imu, 0x09, &c8, 1);
    escribir(s_imu, 0x09, c8 | 0x10); /* Pedo_EN */
    s_pasos_ok = ok;
    if (!ok) ESP_LOGW(TAG, "QMI8658: no se pudo configurar el contador de pasos");
    return ok;
}
uint32_t hal_steps_read(void)
{
    uint8_t d[3];
    if (!s_imu || !s_pasos_ok || leer(s_imu, 0x5A, d, 3) != ESP_OK) return 0;
    return (uint32_t)d[0] | (uint32_t)d[1] << 8 | (uint32_t)d[2] << 16;
}

void hal_imu_tilt(float *x, float *y)
{
    *x = s_tilt_x;
    *y = s_tilt_y;
}

/* --------------------------------------------------------------- sonido */

static esp_codec_dev_handle_t s_parlante;
static QueueHandle_t s_cola_sonidos;
static volatile uint8_t s_amb_pedido = AMB_NADA, s_amb_vol;
static volatile uint8_t s_vol_tipo[3] = {38, 38, 85}; /* ella, efectos, alarmas */
static volatile bool s_snd_vivo; /* la tarea del sonido arrancó */
static volatile bool s_snd_pantalla = true; /* la pantalla está encendida */
static bool s_par_abierto;                  /* el parlante está abierto (amplificador y reloj de audio encendidos) */
static uint8_t s_par_vol = 85;
static void parlante_abrir(void)
{
    if (s_par_abierto || !s_parlante) return;
    esp_codec_dev_sample_info_t fs = {.sample_rate = BOARD_AUDIO_HZ, .channel = 1, .bits_per_sample = 16};
    if (esp_codec_dev_open(s_parlante, &fs) == ESP_CODEC_DEV_OK) { esp_codec_dev_set_out_vol(s_parlante, s_par_vol); s_par_abierto = true; }
}
static void parlante_cerrar(void)
{
    if (!s_par_abierto) return;
    esp_codec_dev_close(s_parlante);
    s_par_abierto = false;
}
static TaskHandle_t s_mic_tarea;
void hal_sound_pantalla(bool encendida)
{
    s_snd_pantalla = encendida;
    if (!encendida && s_mic_tarea) xTaskNotifyGive(s_mic_tarea); /* (101b) el micrófono abierto sin uso se cierra al apagar la pantalla */
    if (encendida && s_cola_sonidos) { uint16_t q = 0xFF; xQueueSend(s_cola_sonidos, &q, 0); /* (DESPERTAR) */ }
}
static volatile uint64_t s_snd_propio;   /* bit por sound_t con un sonido propio en /res */
static volatile uint16_t s_amb_propio;   /* bit por ambiente_t */
static volatile bool s_amb_recargar;     /* cambió el ambiente propio que está sonando */
#define DESPERTAR 0xFF /* mensaje para que la tarea mire el ambiente pedido */
#define CALLAR 0xFE    /* (v39) corta el efecto que suena */
static volatile int64_t s_snd_pedido_us; /* v89: cuándo se pidió la última nota (para medir la demora hasta que suena) */
#define SND_SUAVE 0x100 /* bandera: este sonido usa el volumen de efectos aunque sea de las alarmas (la canción de cuna del ritual) */
#define BLOQUE 160     /* 10 ms a 16 kHz (v89: antes 20 ms; con la reserva del I2S más chica, una nota suena a los ~30-40 ms del toque) */
#define VOCES 6         /* notas de cristal sonando a la vez (v89: antes 4; tocando rápido se cortaban) */

/* Una sola tarea mezcla el ambiente (un bucle que se repite) y los efectos (uno a la vez).
 * Cuando no suena nada, duerme en la cola y el parlante queda en silencio. */
/* (1.0.61) los sonidos que no son notas se sintetizan en OTRA tarea: antes la tarea del sonido se quedaba sintetizando (a
 * prioridad 2, hasta segundos con la cueva dibujando) y las notas de los cristales que tocabas esperaban en la cola (informe:
 * «demora máx 3865 ms»). Ahora la mezcla sigue y el efecto empieza cuando está listo. Un pedido nuevo deja atrás al anterior
 * (s_sfx_gen): el búfer solo lo escribe esta tarea mientras la mezcla no lo está tocando (sfx_n = 0). */
static int16_t *s_sfx_pcm;
static int s_sfx_max;
static TaskHandle_t s_sfx_tarea;
static volatile uint16_t s_sfx_pide;
static volatile uint32_t s_sfx_gen, s_sfx_listo_gen;
static volatile int s_sfx_listo_n = -1;
static volatile uint16_t s_sfx_listo_q;
static int sfx_sintetizar(uint16_t q)
{
    const uint8_t s = (uint8_t)(q & 0xFF);
    int n = 0;
    if (s < 64 && ((s_snd_propio >> s) & 1u)) { /* el que grabaste o elegiste en el editor */
        char r[24];
        snprintf(r, sizeof r, "s%u.pcm", s);
        n = hal_res_read(r, s_sfx_pcm, s_sfx_max * 2) / 2;
    }
    if (n <= 0) n = sfx_render((sound_t)s, s_sfx_pcm, s_sfx_max, BOARD_AUDIO_HZ);
    return n;
}
static void tarea_sfx(void *arg)
{
    (void)arg;
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        for (;;) {
            const uint32_t gen = s_sfx_gen;
            const uint16_t q = s_sfx_pide;
            const int n = sfx_sintetizar(q);
            if (gen != s_sfx_gen) continue; /* mientras tanto pidieron otro: ese */
            s_sfx_listo_q = q;
            s_sfx_listo_n = n;
            s_sfx_listo_gen = gen;
            { const uint16_t d = DESPERTAR; xQueueSend(s_cola_sonidos, &d, 0); } /* que la mezcla lo vea ya (si estaba esperando) */
            break;
        }
    }
}
static void tarea_sonido(void *arg)
{
    (void)arg;
    const int max = (int)(SFX_MAX_SEGUNDOS * BOARD_AUDIO_HZ) + 64;
    const int amb_max = (int)((SFX_AMB_SEGUNDOS + 0.6f) * BOARD_AUDIO_HZ);
    /* la canción de cumpleaños dura ~8 s y el ambiente 8 s: los búferes van a la PSRAM */
    int16_t *pcm = heap_caps_malloc((size_t)max * sizeof(int16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    int16_t *amb = heap_caps_malloc((size_t)amb_max * sizeof(int16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!pcm || !amb) { ESP_LOGE(TAG, "sin memoria para audio"); vTaskDelete(NULL); }
    s_sfx_pcm = pcm;
    s_sfx_max = max;
    if (xTaskCreate(tarea_sfx, "sfx", 4096, NULL, 3, &s_sfx_tarea) != pdPASS) s_sfx_tarea = NULL; /* (1.0.61; si no hay lugar, como antes) */
    s_snd_vivo = true;
    /* las seis notas de los cristales se preparan de antemano: tocar uno suena AL INSTANTE (sintetizarlo en el momento tardaba, sobre todo con la cueva a todo trapo) */
    /* v89: también los cuatro sonidos de los hongos de luz (antes se sintetizaban al tocar: tardaban y cortaban el anterior) */
    /* v39: y los dos cristales de las orillas (el «la» de ella ya no suena en los cristales: se oía zumbón) */
    enum { N_INST = 12 };
    static const uint8_t INST[N_INST] = {SND_CRISTAL_AZUL, SND_CRISTAL_AZUL + 1, SND_CRISTAL_AZUL + 2, SND_CRISTAL_AZUL + 3, SND_CRISTAL_AZUL + 4, SND_CRISTAL_AZUL + 5,
                                         SND_TOQUE, SND_PLOP, SND_ATRAPA, SND_REBOTE, SND_CRISTAL_CELESTE, SND_CRISTAL_MORADO};
    static int16_t *nota_cris[N_INST];
    static int nota_cris_n[N_INST];
    for (int k = 0; k < N_INST; k++) {
        const sound_t sk = (sound_t)INST[k];
        if (sk < 64 && ((s_snd_propio >> sk) & 1u)) continue; /* si lo reemplazaste en el editor, se lee del archivo */
        int16_t *c = heap_caps_malloc((size_t)(1.2f * BOARD_AUDIO_HZ) * sizeof(int16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!c) break;
        const int n = sfx_render(sk, c, (int)(1.2f * BOARD_AUDIO_HZ), BOARD_AUDIO_HZ);
        if (n > 0 && n < (int)(1.2f * BOARD_AUDIO_HZ) - 1) { nota_cris[k] = c; nota_cris_n[k] = n; } else heap_caps_free(c); /* (si no cabía entero, se sintetiza al pedirlo, como antes) */
        vTaskDelay(1);
    }
    static int16_t bloque[BLOQUE];
    uint32_t ultimo_son_ms = 0;
    int sfx_n = 0, sfx_pos = 0, amb_n = 0, amb_pos = 0;
    uint8_t amb_actual = AMB_NADA;
    float g = 0; /* volumen actual del ambiente: sube y baja suave (sin chasquidos) */
    float sfx_gain = 1.0f; /* el volumen del tipo de sonido que suena (ella, efectos o alarma) */
    /* las notas de los cristales suenan juntas (varias a la vez, una sobre otra) y arrancan al instante: sin copiar nada ni esperar a las anteriores */
    struct { const int16_t *d; int n, pos; float g; uint32_t frac, paso; } voz[VOCES] = {{0}}; /* (1.0.53) paso 16.16: la nota afinada (otra altura) */
    while (true) {
        bool voces = false;
        for (int v = 0; v < VOCES; v++) voces |= voz[v].d && voz[v].pos < voz[v].n;
        bool sonando = sfx_pos < sfx_n || amb_n > 0 || s_amb_pedido != AMB_NADA || voces;
        uint16_t q = DESPERTAR;
        BaseType_t llego = xQueueReceive(s_cola_sonidos, &q, sonando ? 0 : pdMS_TO_TICKS(3000));
        const uint32_t ahora_ms = (uint32_t)(esp_timer_get_time() / 1000);
        if (llego == pdTRUE) { parlante_abrir(); ultimo_son_ms = ahora_ms; }
        else if (!sonando && !s_snd_pantalla && s_par_abierto && ahora_ms - ultimo_son_ms > 15000u) parlante_cerrar(); /* nada que sonar y pantalla apagada: amplificador y reloj de audio descansan */
        while (llego == pdTRUE) { /* se atienden TODAS las pedidas juntas (antes una por vuelta: tocando rápido se acumulaban y sonaban tarde) */
          if ((q & 0xFF) == CALLAR) { sfx_pos = sfx_n; llego = xQueueReceive(s_cola_sonidos, &q, 0); continue; }
          if ((q & 0xFF) != DESPERTAR) {
            const uint8_t s = (uint8_t)(q & 0xFF);
            int ki = -1;
            for (int k = 0; k < N_INST; k++) if (INST[k] == s && nota_cris[k]) { ki = k; break; }
            if (ki >= 0 && !(s < 64 && ((s_snd_propio >> s) & 1u))) {
                int v = 0; /* una voz libre; si todas suenan, se reemplaza la más avanzada */
                for (int k = 0, mejor = -1; k < VOCES; k++) {
                    const int resta = voz[k].d ? voz[k].n - voz[k].pos : 0;
                    if (resta <= 0) { v = k; break; }
                    if (mejor < 0 || resta < mejor) { mejor = resta; v = k; }
                }
                voz[v].d = nota_cris[ki];
                voz[v].n = nota_cris_n[ki];
                voz[v].pos = 0;
                voz[v].frac = 0;
                /* (1.0.53) afinada: bits 9-14 = semitonos + 32 (bit 15 marca): la misma nota guardada, leída más rápido o más lento */
                voz[v].paso = (q & 0x8000) ? (uint32_t)(65536.0f * powf(2.0f, (float)((int)((q >> 9) & 0x3F) - 32) / 12.0f)) : 65536u;
                voz[v].g = (float)s_vol_tipo[(q & SND_SUAVE) ? 1 : pet_sound_group((sound_t)s)] / 100.0f;
                { /* v89: la demora desde el toque hasta que la nota entra a la mezcla (más ~30 ms de reserva del I2S) */
                    static int dmax, n; static uint32_t ult_ms;
                    const int d = (int)((esp_timer_get_time() - s_snd_pedido_us) / 1000);
                    if (d > dmax) dmax = d;
                    n++;
                    if (ahora_ms - ult_ms > 15000u) { ult_ms = ahora_ms; hal_log("sonido: notas de cristal %d, demora máx %d ms hasta la mezcla (+ ~30 ms de reserva)", n, dmax); dmax = 0; n = 0; }
                }
                llego = xQueueReceive(s_cola_sonidos, &q, 0);
                continue;
            }
            sfx_n = 0;
            if (s_sfx_tarea) { /* (1.0.61) se sintetiza aparte: la mezcla (y las notas) siguen */
                sfx_pos = 0;
                s_sfx_pide = q;
                s_sfx_gen++;
                xTaskNotifyGive(s_sfx_tarea);
                llego = xQueueReceive(s_cola_sonidos, &q, 0);
                continue;
            }
            if (s < 64 && ((s_snd_propio >> s) & 1u)) { /* el que grabaste o elegiste en el editor */
                char r[24];
                snprintf(r, sizeof r, "s%u.pcm", s);
                sfx_n = hal_res_read(r, pcm, max * 2) / 2;
            }
            if (sfx_n <= 0) { /* (1.0.24) sintetizar con prioridad baja: con 10 le quitaba el núcleo 0 al dibujo (sonido 68: cuadro de 1,4 s) */
                const UBaseType_t pr = uxTaskPriorityGet(NULL);
                vTaskPrioritySet(NULL, 2);
                sfx_n = sfx_render((sound_t)s, pcm, max, BOARD_AUDIO_HZ);
                vTaskPrioritySet(NULL, pr);
            }
            sfx_pos = 0;
            sfx_gain = (float)s_vol_tipo[(q & SND_SUAVE) ? 1 : pet_sound_group((sound_t)s)] / 100.0f; /* SND_SUAVE: va con el volumen de efectos, no con el de las alarmas */
          }
          llego = xQueueReceive(s_cola_sonidos, &q, 0);
        }
        if (s_sfx_tarea && s_sfx_listo_gen == s_sfx_gen && s_sfx_listo_n >= 0) { /* (1.0.61) el efecto quedó listo: suena */
            sfx_n = s_sfx_listo_n;
            sfx_pos = 0;
            s_sfx_listo_n = -1;
            const uint16_t lq = s_sfx_listo_q;
            sfx_gain = (float)s_vol_tipo[(lq & SND_SUAVE) ? 1 : pet_sound_group((sound_t)(lq & 0xFF))] / 100.0f;
            parlante_abrir();
            ultimo_son_ms = (uint32_t)(esp_timer_get_time() / 1000);
        }
        uint8_t pedido = s_amb_vol ? s_amb_pedido : AMB_NADA;
        float meta = pedido == amb_actual && amb_n ? (float)s_amb_vol / 100.0f : 0.0f;
        if (s_amb_recargar && pedido == amb_actual) { s_amb_recargar = false; amb_actual = 0xFE; } /* cambió: se vuelve a cargar */
        if (pedido != amb_actual && g <= 0.001f) { /* ya se apagó el anterior: preparar el nuevo */
            amb_actual = pedido;
            amb_n = 0;
            if (pedido != AMB_NADA && ((s_amb_propio >> pedido) & 1u)) {
                char r[24];
                snprintf(r, sizeof r, "a%u.pcm", pedido);
                amb_n = hal_res_read(r, amb, amb_max * 2) / 2;
                if (amb_n < 0) amb_n = 0;
            }
            if (!amb_n && pedido != AMB_NADA) { /* (1.0.24) 8 s de ambiente se arman de una vez: con prioridad baja, para que el dibujo
                                                  * no espere (el ambiente 5 congelaba la pantalla hasta 3,9 s en el informe del 18-nov) */
                const UBaseType_t pr = uxTaskPriorityGet(NULL);
                vTaskPrioritySet(NULL, 2);
                amb_n = sfx_ambient((ambiente_t)pedido, amb, amb_max, BOARD_AUDIO_HZ);
                vTaskPrioritySet(NULL, pr);
            }
            amb_pos = 0;
        }
        voces = false;
        for (int v = 0; v < VOCES; v++) voces |= voz[v].d && voz[v].pos < voz[v].n;
        if (!amb_n && sfx_pos >= sfx_n && !voces) { vTaskDelay(pdMS_TO_TICKS(10)); continue; } /* nada que sonar: sin este descanso la tarea giraba al 100 % y no dejaba correr a las demás */
        /* (v39) limitador suave de las notas: con varias sonando juntas, cada una baja (~1/raíz de cuántas) para que no se sature */
        int nv = 0;
        for (int k = 0; k < VOCES; k++) nv += voz[k].d && voz[k].pos < voz[k].n;
        static float g_voces = 1.0f;
        const float g_meta = nv <= 2 ? 1.0f : 1.41f / sqrtf((float)nv);
        for (int i = 0; i < BLOQUE; i++) {
            g_voces += (g_meta - g_voces) * 0.0015f; /* (sube y baja suave: sin chasquidos) */
            const float paso = 1.0f / (0.6f * BOARD_AUDIO_HZ);
            if (g < meta) { g += paso; if (g > meta) g = meta; }
            else if (g > meta) { g -= paso; if (g < meta) g = meta; }
            float v = 0;
            if (amb_n) { v = (float)amb[amb_pos] * g; if (++amb_pos >= amb_n) amb_pos = 0; }
            if (sfx_pos < sfx_n) v += (float)pcm[sfx_pos++] * sfx_gain;
            for (int k = 0; k < VOCES; k++)
                if (voz[k].d && voz[k].pos < voz[k].n) {
                    v += (float)voz[k].d[voz[k].pos] * voz[k].g * g_voces;
                    voz[k].frac += voz[k].paso; voz[k].pos += (int)(voz[k].frac >> 16); voz[k].frac &= 0xFFFFu;
                }
            /* limitador suave: si la suma de ambiente y efectos pasa de ~20000 se redondea en vez de recortarse (el recorte «revienta» el parlante) */
            const float av = v < 0 ? -v : v;
            if (av > 20000.0f) v = (v < 0 ? -1.0f : 1.0f) * (20000.0f + 11000.0f * tanhf((av - 20000.0f) / 11000.0f));
            bloque[i] = (int16_t)v;
        }
        parlante_abrir();
        ultimo_son_ms = (uint32_t)(esp_timer_get_time() / 1000);
        esp_codec_dev_write(s_parlante, bloque, (int)sizeof bloque);
    }
}

esp_err_t hal_sound_init(void)
{
    s_parlante = bsp_audio_codec_speaker_init();
    ESP_RETURN_ON_FALSE(s_parlante, ESP_FAIL, TAG, "sin parlante");
    esp_codec_dev_sample_info_t fs = {
        .sample_rate = BOARD_AUDIO_HZ,
        .channel = 1,
        .bits_per_sample = 16,
    };
    ESP_RETURN_ON_FALSE(esp_codec_dev_open(s_parlante, &fs) == ESP_CODEC_DEV_OK, ESP_FAIL, TAG, "no se pudo abrir el audio");
    s_par_abierto = true;
    esp_codec_dev_set_out_vol(s_parlante, 85); /* (85: al 100 el amplificador de la placa distorsiona) */ /* el volumen de cada tipo de sonido se mezcla aparte (hal_sound_gains) */
    s_cola_sonidos = xQueueCreate(8, sizeof(uint16_t));
    /* v89: más prioridad que el juego (8) y la pantalla (6), fija en el núcleo 0: casi siempre está esperando al I2S, pero cuando
     * hay que mezclar el bloque siguiente no puede atrasarse (con la reserva chica, atrasarse = un corte) */
    if (xTaskCreatePinnedToCore(tarea_sonido, "sonido", 4096, NULL, 10, NULL, 0) != pdPASS) {
        ESP_LOGE(TAG, "sin memoria para la tarea del sonido: no sonará nada");
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

/* --- micrófono: la tarea lo abre al pedirlo, mide qué tan fuerte suena y lo cierra al soltarlo --- */
static esp_codec_dev_handle_t s_micro;
static volatile bool s_mic_on, s_mic_abierto;
static volatile int s_mic_nivel = -1;

static volatile bool s_mic_falla; /* v90: la última vez no se pudo abrir (falta memoria): la pesca no pide soplar */
bool hal_mic_ok(void) { return !s_mic_falla; }
/* (1.0.20) el soplido se reconoce AQUÍ, trozo a trozo (16 ms), y no en el ciclo principal: el ciclo mira el nivel cada
 * 90-250 ms (espera el candado de la pantalla) y con un solo trozo bajo entre medio el soplido se cortaba. Por eso la niebla
 * casi nunca se iba. El ciclo solo cuenta los soplidos nuevos (hal_mic_soplos) y el pico (hal_mic_pico) para dibujarlo. */
static volatile int s_mic_pico;
static volatile int s_sop_umbral = 34, s_sop_seguido = 220, s_sop_pausa = 1200;
static volatile uint32_t s_soplos;
void hal_mic_soplido(int umbral, int seguido_ms, int pausa_ms) { s_sop_umbral = umbral; s_sop_seguido = seguido_ms; s_sop_pausa = pausa_ms; }
uint32_t hal_mic_soplos(void) { return s_soplos; }
int hal_mic_pico(void) { int p = s_mic_pico; s_mic_pico = 0; return s_micro ? p : -1; }
static void tarea_mic(void *arg)
{
    (void)arg;
    static int16_t buf[256 * 2];
    uint32_t sin_uso_ms = 0;
    for (;;) {
        if (!s_mic_on && s_mic_abierto && s_snd_pantalla) {
            /* (101b) antes se cerraba al soltarlo: con «escuchar música» eso era abrir y cerrar cada 20 s, y cada apertura pedía otra vez
             * memoria interna para el DMA. En 6 h la memoria interna quedó picada (bloque mayor 3 KB) y la placa se cayó (reinicio #25 del
             * 16-nov). Ahora queda abierto hasta 5 min sin uso mientras la pantalla esté prendida; al apagarla se cierra al tiro */
            if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1000)) == 0) sin_uso_ms += 1000; /* (una notificación = cambió algo: se vuelve a mirar) */
            if (s_mic_on) { sin_uso_ms = 0; continue; }
            if (sin_uso_ms < 300000u && s_snd_pantalla) continue;
            sin_uso_ms = 0;
        }
        if (!s_mic_on) {
            if (s_mic_abierto) {
                /* al cerrar, el controlador I2S se queja («the channel has not been enabled yet») cada vez, y ese aviso iba al registro cada 20 s
                 * (cada tanda de la música): es inofensivo, se calla solo durante el cierre */
                const esp_log_level_t nivel = esp_log_level_get("i2s_common");
                esp_log_level_set("i2s_common", ESP_LOG_NONE);
                esp_codec_dev_close(s_micro);
                esp_log_level_set("i2s_common", nivel);
                s_mic_abierto = false;
            }
            sin_uso_ms = 0;
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
            continue;
        }
        if (!s_mic_abierto) {
            /* abrir el micrófono reserva memoria interna para el DMA; si no alcanza, el controlador del códec se cae (reinicio en bucle):
             * mejor no abrirlo y seguir sin micrófono */
            const size_t dma_libre = heap_caps_get_free_size(MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
            const size_t dma_bloque = heap_caps_get_largest_free_block(MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
            /* (1.0.21) antes se pedían 8 KB libres: con una partida larga la RAM interna queda en ~7,6 KB y el micrófono NO se abría
             * nunca (ni la niebla, ni el olor, ni el bote). Los canales I2S y su DMA ya se reservaron al arrancar (bsp_audio_init, con el
             * parlante); abrir el micrófono en estéreo solo cambia sus 3 búferes de 320 a 640 B (~2 KB). Con 3 KB alcanza de sobra */
            if (dma_libre < 3072 || dma_bloque < 1024) {
                static int64_t avisado_us; /* (1.0.18) antes se avisaba una sola vez: ahora una vez por minuto, para ver en el registro si es esto */
                if (!avisado_us || esp_timer_get_time() - avisado_us > 60000000) { avisado_us = esp_timer_get_time(); hal_evento("micrófono: poca memoria DMA (libre %u B, bloque %u B): no se abre", (unsigned)dma_libre, (unsigned)dma_bloque); }
                s_mic_on = false; s_mic_nivel = -1; s_mic_falla = true;
                continue;
            }
            esp_codec_dev_sample_info_t fs = {.sample_rate = BOARD_AUDIO_HZ, .channel = 2, .bits_per_sample = 16};
            if (esp_codec_dev_open(s_micro, &fs) != ESP_CODEC_DEV_OK) { hal_evento("micrófono: no se pudo abrir"); s_mic_on = false; s_mic_nivel = -1; continue; }
            esp_codec_dev_set_in_gain(s_micro, 30.0f);
            { static bool dicho; if (!dicho) { dicho = true; hal_log("micrófono: abierto (RAM DMA libre %u B, bloque %u B)", (unsigned)dma_libre, (unsigned)dma_bloque); } }
            s_mic_abierto = true;
            s_mic_falla = false;
        }
        if (esp_codec_dev_read(s_micro, buf, sizeof buf) != ESP_CODEC_DEV_OK) { vTaskDelay(pdMS_TO_TICKS(50)); continue; }
        int64_t suma = 0;
        for (int i = 0; i < (int)(sizeof buf / 2); i++) suma += (int32_t)buf[i] * buf[i];
        float rms = sqrtf((float)suma / (float)(sizeof buf / 2));
        int n = (int)(rms * 100.0f / 9000.0f); /* soplar de cerca satura el micrófono: ~100 */
        if (n > 100) n = 100;
        s_mic_nivel = n;
        if (n > s_mic_pico) s_mic_pico = n;
        { /* (1.0.20) el soplido: fuerte y seguido un rato; un bajón corto no lo corta (el aire no suena parejo) */
            static int alto_ms, bajo_ms, pausa_ms;
            const int t = (int)(sizeof buf / 4) * 1000 / BOARD_AUDIO_HZ; /* ms de este trozo (estéreo, 16 bits) */
            pausa_ms = pausa_ms > t ? pausa_ms - t : 0;
            if (n >= s_sop_umbral) {
                alto_ms += t; bajo_ms = 0;
                if (alto_ms >= s_sop_seguido && !pausa_ms) { s_soplos++; pausa_ms = s_sop_pausa; alto_ms = 0; }
            } else if (alto_ms) {
                bajo_ms += t;
                if (bajo_ms > 160) alto_ms = bajo_ms = 0;
            }
        }
    }
}

void hal_mic_enable(bool si)
{
    if (si && (!s_micro || !s_mic_tarea)) {
        /* (1.0.18) antes, si fallaba UNA vez (por ejemplo sin RAM interna justo en ese momento), ya no había micrófono hasta reiniciar:
         * el bote no oía el soplido. Ahora se vuelve a intentar cada 20 s, y la tarea puede tener su pila en la PSRAM. */
        static int64_t prox_us;
        const int64_t ahora_us = esp_timer_get_time();
        if (ahora_us < prox_us) return;
        prox_us = ahora_us + 20000000;
        if (!s_micro) s_micro = bsp_audio_codec_microphone_init();
        if (!s_micro) { hal_evento("micrófono: no se pudo iniciar el códec (RAM interna %u B)", (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL)); return; }
        if (!s_mic_tarea && xTaskCreate(tarea_mic, "micro", 4096, NULL, 3, &s_mic_tarea) != pdPASS) {
            s_mic_tarea = NULL;
            if (xTaskCreateWithCaps(tarea_mic, "micro", 4096, NULL, 3, &s_mic_tarea, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT) != pdPASS) {
                s_mic_tarea = NULL;
                hal_evento("micrófono: sin memoria para su tarea");
                return;
            }
        }
    }
    if (!s_micro || !s_mic_tarea || si == s_mic_on) return;
    s_mic_on = si;
    s_mic_nivel = 0;
    xTaskNotifyGive(s_mic_tarea);
}

int hal_mic_level(void) { return s_micro ? s_mic_nivel : -1; }

void hal_sound_gains(int ella, int efectos, int alarma)
{
    s_vol_tipo[0] = (uint8_t)(ella < 0 ? 0 : (ella > 100 ? 100 : ella));
    s_vol_tipo[1] = (uint8_t)(efectos < 0 ? 0 : (efectos > 100 ? 100 : efectos));
    s_vol_tipo[2] = (uint8_t)(alarma < 0 ? 0 : (alarma > 100 ? 100 : alarma));
}

bool hal_sound_ready(void) { return s_snd_vivo; }
void hal_sound_gains_get(int *ella, int *efectos, int *alarma) { *ella = s_vol_tipo[0]; *efectos = s_vol_tipo[1]; *alarma = s_vol_tipo[2]; }

void hal_sound_volume(int pct)
{
    s_par_vol = (uint8_t)(pct < 0 ? 0 : (pct > 100 ? 100 : pct));
    if (s_parlante && s_par_abierto) esp_codec_dev_set_out_vol(s_parlante, s_par_vol);
}

void hal_touch_enable(bool si)
{
    lv_indev_t *t = bsp_display_get_input_dev();
    if (t) lv_indev_enable(t, si);
}

void hal_sound_play(sound_t s)
{
    if (s == SND_NINGUNO || !s_cola_sonidos) return;
    s_snd_pedido_us = esp_timer_get_time();
    uint16_t v = (uint16_t)s;
    xQueueSend(s_cola_sonidos, &v, 0);
}
void hal_sound_stop(void)
{
    if (!s_cola_sonidos) return;
    uint16_t v = CALLAR;
    xQueueSend(s_cola_sonidos, &v, 0);
}
/* (1.0.53) una nota de cristal afinada: «semi» semitonos más alta (o más baja, negativo; -24..+24) */
void hal_sound_note(sound_t s, int semi, bool suave)
{
    if (s == SND_NINGUNO || !s_cola_sonidos) return;
    if (semi < -24) semi = -24;
    if (semi > 24) semi = 24;
    s_snd_pedido_us = esp_timer_get_time();
    uint16_t v = (uint16_t)((uint16_t)s | (suave ? SND_SUAVE : 0) | (semi ? (0x8000u | (uint16_t)(((semi + 32) & 0x3F) << 9)) : 0));
    xQueueSend(s_cola_sonidos, &v, 0);
}
void hal_sound_play_soft(sound_t s)
{
    if (s == SND_NINGUNO || !s_cola_sonidos) return;
    uint16_t v = (uint16_t)((uint16_t)s | SND_SUAVE);
    xQueueSend(s_cola_sonidos, &v, 0);
}

void hal_ambient_set(ambiente_t a, int vol)
{
    s_amb_pedido = (uint8_t)a;
    s_amb_vol = (uint8_t)(vol < 0 ? 0 : (vol > 100 ? 100 : vol));
    if (!s_cola_sonidos) return;
    uint16_t v = DESPERTAR;
    xQueueSend(s_cola_sonidos, &v, 0);
}

void hal_sound_custom(sound_t s, bool propio)
{
    if ((unsigned)s >= SND_COUNT || s >= 64) return;
    if (propio) s_snd_propio |= 1ull << s;
    else s_snd_propio &= ~(1ull << s);
}

void hal_ambient_custom(ambiente_t a, bool propio)
{
    if ((unsigned)a >= AMB_COUNT) return;
    if (propio) s_amb_propio |= (uint16_t)(1u << a);
    else s_amb_propio &= (uint16_t)~(1u << a);
    s_amb_recargar = true;
}

/* ------------------------------------------------------------ recursos */
/* Lo que cambiaste con el editor (íconos y sonidos) vive en la partición "recursos" (/res). */
#define RES_DIR "/res"
static bool s_res_ok;

/* Lista en memoria de los archivos de recursos: preguntar a SPIFFS por cada nombre que no existe
 * recorre toda la partición (varios segundos cada vez) y trababa el arranque. */
#define RES_MAX 512
static EXT_RAM_BSS_ATTR struct { char n[20]; int t; } s_res_lista[RES_MAX];
static int s_res_cuenta;

static portMUX_TYPE s_res_mux = portMUX_INITIALIZER_UNLOCKED; /* la lista la tocan el ciclo principal y la tarea del registro */
static void res_lista_poner(const char *nombre, int t)
{
    portENTER_CRITICAL(&s_res_mux);
    bool hecho = false;
    for (int i = 0; i < s_res_cuenta && !hecho; i++)
        if (strcmp(s_res_lista[i].n, nombre) == 0) {
            if (t < 0) s_res_lista[i] = s_res_lista[--s_res_cuenta];
            else s_res_lista[i].t = t;
            hecho = true;
        }
    if (!hecho && t >= 0 && s_res_cuenta < RES_MAX && strlen(nombre) < sizeof s_res_lista[0].n) {
        strcpy(s_res_lista[s_res_cuenta].n, nombre);
        s_res_lista[s_res_cuenta++].t = t;
    }
    portEXIT_CRITICAL(&s_res_mux);
}

static void ruta_res(const char *nombre, char *r, size_t n);
esp_err_t hal_res_init(void)
{
    esp_vfs_spiffs_conf_t c = {.base_path = RES_DIR, .partition_label = "recursos", .max_files = 3, .format_if_mount_failed = true};
    esp_err_t e = esp_vfs_spiffs_register(&c);
    s_res_ok = e == ESP_OK;
    if (!s_res_ok) ESP_LOGW(TAG, "sin partición de recursos (%s)", esp_err_to_name(e));
    s_res_cuenta = 0;
    if (s_res_ok) {
        DIR *d = opendir(RES_DIR);
        struct dirent *de;
        while (d && (de = readdir(d)) != NULL) {
            char r[64];
            struct stat st;
            snprintf(r, sizeof r, RES_DIR "/%.40s", de->d_name);
            if (stat(r, &st) == 0) res_lista_poner(de->d_name, (int)st.st_size);
        }
        if (d) closedir(d);
        /* (1.0.26) un guardado seguro que se cortó a mitad (ver hal_res_save_seguro): si el archivo de verdad no está, la copia
         * «~» (que se escribió y comprobó entera antes de borrar nada) pasa a ser el archivo; si está, la copia sobra */
        char pend[4][20];
        int np = 0;
        for (int i = 0; i < s_res_cuenta && np < 4; i++) {
            const size_t l = strlen(s_res_lista[i].n);
            if (l > 1 && s_res_lista[i].n[l - 1] == '~') { memcpy(pend[np], s_res_lista[i].n, l + 1); np++; }
        }
        for (int k = 0; k < np; k++) {
            char base[20], rt[48], rb[48];
            memcpy(base, pend[k], sizeof base);
            base[strlen(base) - 1] = 0;
            ruta_res(pend[k], rt, sizeof rt);
            ruta_res(base, rb, sizeof rb);
            const int tt = hal_res_size(pend[k]);
            if (hal_res_size(base) < 0 && rename(rt, rb) == 0) { res_lista_poner(pend[k], -1); res_lista_poner(base, tt); ESP_LOGW(TAG, "recuperado %s", base); }
            else if (hal_res_size(base) >= 0) { remove(rt); res_lista_poner(pend[k], -1); }
        }
    }
    return e;
}

static void ruta_res(const char *nombre, char *r, size_t n) { snprintf(r, n, RES_DIR "/%s", nombre); }
/* (1.0.59) cuánto ocupan las dos memorias de archivos (recursos y fotos) y los 6 archivos más grandes de recursos: SPIFFS
 * casi lleno escribe lentísimo (informe #10: el registro tardó 16 s y la placa quedó trabada 48 s con la pantalla apagada) */
int hal_res_informe(char *b, int cap)
{
    size_t tr = 0, ur = 0, tf = 0, uf = 0;
    if (esp_spiffs_mounted("recursos")) esp_spiffs_info("recursos", &tr, &ur);
    if (esp_spiffs_mounted("fotos")) esp_spiffs_info("fotos", &tf, &uf);
    int k = snprintf(b, (size_t)cap, "recursos %u de %u KB (%d archivos), fotos %u de %u KB; más grandes:",
                     (unsigned)(ur / 1024), (unsigned)(tr / 1024), s_res_cuenta, (unsigned)(uf / 1024), (unsigned)(tf / 1024));
    bool usado[RES_MAX] = {0};
    for (int q = 0; q < 6 && k < cap - 32; q++) {
        int m = -1;
        for (int i = 0; i < s_res_cuenta; i++) if (!usado[i] && (m < 0 || s_res_lista[i].t > s_res_lista[m].t)) m = i;
        if (m < 0) break;
        usado[m] = true;
        k += snprintf(b + k, (size_t)(cap - k), " %s %dK", s_res_lista[m].n, s_res_lista[m].t / 1024);
    }
    return k;
}

bool hal_res_save(const char *nombre, const void *datos, int n)
{
    if (!s_res_ok || !datos || n <= 0) return false;
    spiffs_limpiar_pasos("recursos", n + 4096); /* (v97) lugar libre primero, de a poco (ver spiffs_limpiar_pasos) */
    char r[48];
    ruta_res(nombre, r, sizeof r);
    FILE *f = fopen(r, "wb");
    if (!f) return false;
    bool ok = fwrite(datos, 1, (size_t)n, f) == (size_t)n;
    fclose(f);
    if (!ok) { remove(r); res_lista_poner(nombre, -1); }
    else res_lista_poner(nombre, n);
    return ok;
}

/* (1.0.26) guardar SIN poder perder lo anterior (las mascotas que descansan en perfiles). hal_res_save abre con "wb": eso
 * borra el archivo viejo ANTES de escribir, y si la escritura fallaba (flash llena, sin memoria, un reinicio a mitad) el
 * archivo quedaba borrado: así se perdió una mascota de perfiles el 19-nov. Aquí se escribe primero «nombre~», se relee y se
 * compara, y recién entonces se cambia por el viejo. Si algo se corta a mitad, hal_res_init lo arregla al arrancar. */
static bool res_iguales(const char *ruta, const void *datos, int n)
{
    FILE *f = fopen(ruta, "rb");
    if (!f) return false;
    uint8_t b[256];
    int i = 0;
    bool ok = true;
    while (ok && i < n) {
        const int k = n - i < (int)sizeof b ? n - i : (int)sizeof b;
        ok = (int)fread(b, 1, (size_t)k, f) == k && memcmp(b, (const uint8_t *)datos + i, (size_t)k) == 0;
        i += k;
    }
    if (ok) ok = fread(b, 1, 1, f) == 0; /* y que no sobre nada */
    fclose(f);
    return ok;
}
bool hal_res_save_seguro(const char *nombre, const void *datos, int n)
{
    if (!s_res_ok || !datos || n <= 0) return false;
    char tmp_n[24];
    if (snprintf(tmp_n, sizeof tmp_n, "%s~", nombre) >= (int)sizeof tmp_n || strlen(tmp_n) >= sizeof s_res_lista[0].n) return false;
    spiffs_limpiar_pasos("recursos", 2 * n + 4096);
    char r[48], rt[48];
    ruta_res(nombre, r, sizeof r);
    ruta_res(tmp_n, rt, sizeof rt);
    FILE *f = fopen(rt, "wb");
    if (!f) { hal_evento("guardar seguro: no se pudo abrir %s (el de antes sigue igual)", tmp_n); return false; }
    bool ok = fwrite(datos, 1, (size_t)n, f) == (size_t)n;
    if (fclose(f) != 0) ok = false;
    if (ok) ok = res_iguales(rt, datos, n);
    if (!ok) { remove(rt); res_lista_poner(tmp_n, -1); hal_evento("guardar seguro: %s NO se pudo escribir bien (el de antes sigue igual)", nombre); return false; }
    res_lista_poner(tmp_n, n);
    remove(r); /* (si no estaba, da igual) */
    res_lista_poner(nombre, -1);
    if (rename(rt, r) == 0) { res_lista_poner(tmp_n, -1); res_lista_poner(nombre, n); return true; }
    /* no se pudo renombrar: se escribe directo; la copia «~» se queda hasta que el directo esté comprobado */
    f = fopen(r, "wb");
    ok = f && fwrite(datos, 1, (size_t)n, f) == (size_t)n;
    if (f && fclose(f) != 0) ok = false;
    if (ok) ok = res_iguales(r, datos, n);
    if (ok) { res_lista_poner(nombre, n); remove(rt); res_lista_poner(tmp_n, -1); }
    else hal_evento("guardar seguro: %s quedó solo como %s (se arregla al arrancar)", nombre, tmp_n);
    return ok;
}

int hal_res_size(const char *nombre)
{
    if (!s_res_ok) return -1;
    int r = -1;
    portENTER_CRITICAL(&s_res_mux);
    for (int i = 0; i < s_res_cuenta; i++)
        if (strcmp(s_res_lista[i].n, nombre) == 0) { r = s_res_lista[i].t; break; }
    portEXIT_CRITICAL(&s_res_mux);
    return r;
}

int hal_res_read(const char *nombre, void *buf, int cap)
{
    if (!s_res_ok || !buf || hal_res_size(nombre) < 0) return -1;
    char r[48];
    ruta_res(nombre, r, sizeof r);
    FILE *f = fopen(r, "rb");
    if (!f) return -1;
    int n = (int)fread(buf, 1, (size_t)cap, f);
    fclose(f);
    return n;
}

bool hal_res_delete(const char *nombre)
{
    char r[48];
    ruta_res(nombre, r, sizeof r);
    bool ok = s_res_ok && remove(r) == 0;
    if (ok) res_lista_poner(nombre, -1);
    return ok;
}

/* El registro se ESCRIBE AL FINAL del archivo (solo lo nuevo) y cada minuto: antes se reescribía entero (24 KB) cada 20 s en la flash
 * y la placa se congelaba 5 o 6 segundos cada vez. Al llenarse, el archivo se renombra (instantáneo) y se empieza otro. */
static SemaphoreHandle_t s_log_io; /* que dos volcados no escriban a la vez */
/* (1.0.64) el registro y los eventos raros van a la partición cruda «registro» (anillo.inc), ya no a SPIFFS. Si la
 * placa todavía tiene la tabla vieja (se cambia solo grabando con cable), todo sigue como antes, en registro.txt. */
static const esp_partition_t *s_reg_part;
#define AN_LEER(d, b, n) esp_partition_read(s_reg_part, (d), (b), (n))
#define AN_ESCRIBIR(d, b, n) esp_partition_write(s_reg_part, (d), (b), (n))
#define AN_BORRAR(d) esp_partition_erase_range(s_reg_part, (d), AN_SECTOR)
#include "anillo.inc"
#define REG_SUBTIPO 0x40
#define REG_EV_SECTORES 6 /* 24 KB de eventos raros; el resto (104 KB) para el registro */
static anillo_t s_an_ev, s_an_log;
static int8_t s_an_estado; /* 0 sin mirar, 1 listo, -1 no está la partición */
static bool an_listo(void) /* (con s_log_io tomado) */
{
    if (s_an_estado == 0) {
        s_an_estado = -1;
        s_reg_part = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, (esp_partition_subtype_t)REG_SUBTIPO, "registro");
        if (s_reg_part && s_reg_part->size >= 16 * AN_SECTOR) {
            const uint32_t n = (uint32_t)(s_reg_part->size / AN_SECTOR);
            if (an_abrir(&s_an_ev, 0, REG_EV_SECTORES) && an_abrir(&s_an_log, REG_EV_SECTORES * AN_SECTOR, n - REG_EV_SECTORES))
                s_an_estado = 1;
        }
    }
    return s_an_estado == 1;
}
/* (con s_log_io tomado) una sola vez: lo que quedó en los archivos viejos pasa al anillo y los archivos se borran */
static void an_migrar(void)
{
    static bool hecho;
    if (hecho || !s_res_ok || s_an_estado != 1) return;
    hecho = true;
    static const char *const VIEJOS[3] = {"eventos.txt", "registro_v.txt", "registro.txt"};
    bool habia = false;
    for (int i = 0; i < 3; i++) {
        if (hal_res_size(VIEJOS[i]) < 0) continue;
        habia = true;
        static EXT_RAM_BSS_ATTR char m[12288];
        const int k = hal_res_read(VIEJOS[i], m, sizeof m);
        if (k > 0) an_agregar(i ? &s_an_log : &s_an_ev, m, (uint32_t)k);
        hal_res_delete(VIEJOS[i]);
    }
    if (habia) hal_evento("registro: ahora en su propia partición (%u KB, sin SPIFFS); se pasaron los archivos viejos",
                          (unsigned)(s_reg_part->size / 1024));
}
static void ev_volcar(void) /* (con s_log_io tomado) lo pendiente del resumen de eventos pasa a eventos.txt */
{
    static EXT_RAM_BSS_ATTR char pend[EV_RTC];
    portENTER_CRITICAL(&s_log_mux);
    uint32_t n = (s_ev_marca == EV_MARCA && s_ev_n <= EV_RTC) ? s_ev_n : 0;
    memcpy(pend, s_ev_rtc, n);
    s_ev_n = 0;
    portEXIT_CRITICAL(&s_log_mux);
    if (n && an_listo()) { an_agregar(&s_an_ev, pend, n); return; } /* (1.0.64) */
    if (!n || !s_res_ok) return;
    char r[48];
    ruta_res(EV_ARCHIVO, r, sizeof r);
    int actual = hal_res_size(EV_ARCHIVO);
    if (actual < 0) actual = 0;
    if (actual + (int)n > EV_MAX) { /* lleno: se quedan los 5 KB más nuevos (pasa pocas veces: son eventos raros) */
        static EXT_RAM_BSS_ATTR char viejo[EV_MAX];
        int v = hal_res_read(EV_ARCHIVO, viejo, EV_MAX);
        int desde = v > 5120 ? v - 5120 : 0;
        while (desde > 0 && desde < v && viejo[desde - 1] != '\n') desde++; /* desde una línea entera */
        FILE *f = fopen(r, "wb");
        if (f) { fputs("(… eventos más viejos descartados)\n", f); fwrite(viejo + desde, 1, (size_t)(v - desde), f); fclose(f); }
        actual = hal_res_size(EV_ARCHIVO);
        if (actual < 0) actual = 0;
    }
    FILE *f = fopen(r, "ab");
    if (!f) return;
    const bool ok = fwrite(pend, 1, n, f) == n;
    fclose(f);
    if (ok) res_lista_poner(EV_ARCHIVO, actual + (int)n);
}
static void hora_guardar(void) /* v88: sin chip de reloj, la hora segura se guarda cada 2 min (antes solo al arrancar: tras quedarse sin batería volvía muy atrás) */
{
    if (s_rtc || !s_rtc_confiable) return;
    const uint32_t t = (uint32_t)time(NULL);
    if (t < 1767225600u || t - s_hora_guardada < 120u) return;
    s_hora_guardada = t;
    hal_storage_set("hora", &t, sizeof t);
}
/* (v97) limpia SPIFFS de a poco: pide bloques libres en escalones de 8 KB y descansa entre uno y otro (cada escalón borra a lo
 * más un bloque de la flash), así nunca se queda pegado varios segundos seguidos ni dispara el perro guardián */
static void spiffs_limpiar_pasos(const char *particion, int bytes)
{
    if (!esp_spiffs_mounted(particion)) return;
    for (int b = 8 * 1024; b <= bytes; b += 8 * 1024) {
        if (esp_spiffs_gc(particion, (size_t)b) != ESP_OK) break; /* (sin lugar que limpiar) */
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
static void log_volcar(void)
{
    if (!s_log_io) return;
    xSemaphoreTake(s_log_io, portMAX_DELAY);
    if (an_listo()) an_migrar(); /* (1.0.64) antes de los eventos nuevos, así quedan en orden */
    ev_volcar();
    hora_guardar();
    static EXT_RAM_BSS_ATTR char nuevo[LOG_RAM];
    portENTER_CRITICAL(&s_log_mux);
    int n = s_log_n;
    memcpy(nuevo, s_log_ram, (size_t)n);
    s_log_n = 0;
    portEXIT_CRITICAL(&s_log_mux);
    if (an_listo()) { /* (1.0.64) al anillo: unos ms, más ~45 ms cada 4 KB al borrar el sector siguiente */
        const int64_t t1 = esp_timer_get_time();
        if (n > 0) an_agregar(&s_an_log, nuevo, (uint32_t)n);
        xSemaphoreGive(s_log_io);
        const int ms1 = (int)((esp_timer_get_time() - t1) / 1000);
        if (ms1 > 300) hal_evento("escritura lenta a flash: el registro (partición propia) tardó %d ms (%d bytes)", ms1, n);
        return;
    }
    if (n <= 0 || !s_res_ok) { xSemaphoreGive(s_log_io); return; }
    char r[48], rv[48];
    ruta_res(LOG_ARCHIVO, r, sizeof r);
    ruta_res(LOG_VIEJO, rv, sizeof rv);
    int actual = hal_res_size(LOG_ARCHIVO);
    if (actual < 0) actual = 0;
    if (actual > LOG_MAX) { remove(r); res_lista_poner(LOG_ARCHIVO, -1); actual = 0; } /* uno de la versión anterior (más grande): se descarta */
    if (actual + n > LOG_MAX) { /* se llenó: pasa a ser el "viejo" */
        remove(rv);
        res_lista_poner(LOG_VIEJO, -1);
        if (rename(r, rv) == 0) { res_lista_poner(LOG_VIEJO, actual); res_lista_poner(LOG_ARCHIVO, -1); }
        else { remove(r); res_lista_poner(LOG_ARCHIVO, -1); }
        actual = 0;
    }
    const int64_t t0 = esp_timer_get_time();
    FILE *f = fopen(r, "ab");
    if (!f) { xSemaphoreGive(s_log_io); return; }
    bool ok = fwrite(nuevo, 1, (size_t)n, f) == (size_t)n;
    fclose(f);
    if (ok) res_lista_poner(LOG_ARCHIVO, actual + n);
    xSemaphoreGive(s_log_io);
    const int ms = (int)((esp_timer_get_time() - t0) / 1000);
    if (ms > 800) hal_evento("escritura lenta a flash: el registro tardó %d ms (%d bytes)", ms, n);
}
/* En la flash (SPIFFS), escribir el registro se pegaba entre 1 y 4 segundos de vez en cuando (limpieza interna) y congelaba el ciclo principal:
 * ahora lo hace una tarea aparte; el ciclo principal solo le avisa (y solo cada minuto). Al reiniciar o leerlo, se escribe al instante. */
/* (1.0.44, pedido del usuario: «ataquemos los congelones») escribir en la flash detiene TODO unos 2 s aunque lo haga esta
 * tarea aparte: mientras la flash borra, el procesador no puede leer el programa ni la PSRAM (el informe #6: «escritura
 * lenta a flash 1,6-2,2 s» cada 5-7 min, y justo ahí «pantalla/ciclo trabado»). Ahora, con la pantalla encendida, el
 * registro y los eventos NO se escriben: esperan en la RAM (lo más viejo se pierde si se llena; los eventos raros
 * sobreviven a un reinicio en la RAM del RTC) y se escriben al apagarse la pantalla. Si la pantalla lleva más de 30 min
 * encendida (la pecera, cargando), se escribe igual, como mucho una vez cada 30 min. Al reiniciar o mandar un informe se
 * escribe al instante, como siempre. */
static bool s_log_pantalla;
static volatile bool s_log_pendiente;
static uint32_t s_log_ult_escritura;
static bool log_puede_escribir(void)
{
    if (!s_log_pantalla || s_an_estado == 1) return true; /* (1.0.64) en la partición propia escribir no traba: siempre */
    const uint32_t ahora = (uint32_t)(esp_timer_get_time() / 1000);
    return s_log_ult_escritura && ahora - s_log_ult_escritura > 1800000u;
}
static void log_tarea(void *arg)
{
    (void)arg;
    s_log_ult_escritura = (uint32_t)(esp_timer_get_time() / 1000) | 1u;
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        if (!log_puede_escribir()) { s_log_pendiente = true; continue; } /* (1.0.44) con la pantalla encendida: espera */
        s_log_pendiente = false;
        s_log_ult_escritura = (uint32_t)(esp_timer_get_time() / 1000) | 1u;
        log_volcar();
        if (s_log_pantalla) continue; /* (1.0.44) la limpieza de la flash, solo con la pantalla apagada */
        if (s_an_estado == 1) { /* (1.0.64) el registro ya no usa SPIFFS: limpiarlo una vez por hora alcanza */
            static uint32_t ult_gc;
            const uint32_t ahora = (uint32_t)(esp_timer_get_time() / 1000000);
            if (ult_gc && ahora - ult_gc < 3600u) continue;
            ult_gc = ahora | 1u;
        }
        /* (v97) y de paso, con calma, se limpia la flash (el recolector de SPIFFS) en pasos chicos con pausas: así la próxima
         * escritura (el registro, una foto, la prueba de la flash) encuentra bloques libres y no se pega 2 a 4 s en medio del juego */
        spiffs_limpiar_pasos("recursos", 48 * 1024);
        spiffs_limpiar_pasos("fotos", 48 * 1024);
    }
}
void hal_log_flush_pantalla(bool encendida)
{
    const bool se_apago = s_log_pantalla && !encendida;
    s_log_pantalla = encendida;
    if (se_apago && s_log_pendiente && s_log_tarea) xTaskNotifyGive(s_log_tarea); /* (1.0.44) lo que esperaba, ahora que nadie mira */
}
/* (v101) la RAM del registro está por llenarse: que la tarea lo escriba ya (no más de una vez cada 2 min, para no trabar el juego) */
static void log_casi_lleno(void)
{
    if (!s_log_tarea || xPortInIsrContext()) return;
    static uint32_t ult_ms;
    const uint32_t ahora = (uint32_t)(esp_timer_get_time() / 1000);
    if (ult_ms && ahora - ult_ms < 120000u) return; /* (cada escritura traba ~1,7 s: no más de una cada 2 min) */
    if (!log_puede_escribir()) { s_log_pendiente = true; return; } /* (1.0.44) con la pantalla encendida, mejor perder lo más viejo que trabar */
    ult_ms = ahora;
    s_log_flush_ms = ahora;
    xTaskNotifyGive(s_log_tarea);
}
void hal_log_flush(bool forzar)
{
    uint32_t ahora = (uint32_t)(esp_timer_get_time() / 1000);
    if (!s_log_io) s_log_io = xSemaphoreCreateMutex();
    if (!s_log_io) return;
    if (forzar) { s_log_flush_ms = ahora; log_volcar(); return; }
    if (ahora - s_log_flush_ms < (s_log_pantalla ? 300000u : 600000u)) return; /* (v100) apagada: cada 10 min (antes cada minuto: cada escritura despierta todo ~1,7 s) */
    s_log_flush_ms = ahora;
    if (!s_log_tarea && xTaskCreate(log_tarea, "log", 4096, NULL, 1, &s_log_tarea) != pdPASS) { s_log_tarea = NULL; log_volcar(); return; }
    if (s_log_tarea) xTaskNotifyGive(s_log_tarea);
}


static int log_leer_anillo(char *buf, int cap) /* (1.0.64) con s_log_io tomado y an_listo() */
{
    static const char T1[] = "=== EVENTOS RAROS (archivo aparte, no se borra con el registro) ===\n", T2[] = "=== REGISTRO ===\n";
    int v = 0;
    if (cap > (int)sizeof T1 + (int)sizeof T2 + 64) {
        memcpy(buf, T1, sizeof T1 - 1); v = (int)sizeof T1 - 1;
        const int lim = cap / 3; /* hasta un tercio: el resto para el registro */
        const int e = an_leer_todo(&s_an_ev, buf + v, lim - v);
        if (e > 0) v += e;
        else { static const char N[] = "(ninguno)\n"; memcpy(buf + v, N, sizeof N - 1); v += (int)sizeof N - 1; }
        memcpy(buf + v, T2, sizeof T2 - 1); v += (int)sizeof T2 - 1;
    }
    const int r = an_leer_todo(&s_an_log, buf + v, cap - v);
    if (r > 0) v += r;
    return v;
}
int hal_log_read(char *buf, int cap)
{
    hal_log_flush(true);
    if (s_log_io) {
        xSemaphoreTake(s_log_io, portMAX_DELAY);
        if (an_listo()) { const int r = log_leer_anillo(buf, cap); xSemaphoreGive(s_log_io); return r; }
        xSemaphoreGive(s_log_io);
    }
    int v = 0;
    { /* v88: primero el resumen de eventos raros (no lo borra el desborde del registro) */
        static const char T1[] = "=== EVENTOS RAROS (archivo aparte, no se borra con el registro) ===\n", T2[] = "=== REGISTRO ===\n";
        if (cap > (int)sizeof T1 + (int)sizeof T2 + 64) {
            memcpy(buf, T1, sizeof T1 - 1); v = (int)sizeof T1 - 1;
            const int lim = cap / 3; /* hasta un tercio: el resto para el registro */
            int e = hal_res_size(EV_ARCHIVO);
            if (e > 0) {
                static EXT_RAM_BSS_ATTR char ev[EV_MAX];
                int n = hal_res_read(EV_ARCHIVO, ev, EV_MAX);
                if (n > 0) {
                    int desde = n > lim - v ? n - (lim - v) : 0;
                    while (desde > 0 && desde < n && ev[desde - 1] != '\n') desde++;
                    memcpy(buf + v, ev + desde, (size_t)(n - desde)); v += n - desde;
                }
            } else { static const char N[] = "(ninguno)\n"; memcpy(buf + v, N, sizeof N - 1); v += (int)sizeof N - 1; }
            memcpy(buf + v, T2, sizeof T2 - 1); v += (int)sizeof T2 - 1;
        }
    }
    int a = hal_res_read(LOG_VIEJO, buf + v, cap - v); /* (v101) antes se leía en buf (desde 0) y borraba el resumen de eventos raros */
    if (a > 0) v += a;
    int b = hal_res_read(LOG_ARCHIVO, buf + v, cap - v);
    if (b > 0) v += b;
    return v;
}

void hal_log_clear(void)
{
    portENTER_CRITICAL(&s_log_mux);
    s_log_n = 0;
    portEXIT_CRITICAL(&s_log_mux);
    if (s_log_io) { /* (1.0.64) los eventos raros se quedan, como antes */
        xSemaphoreTake(s_log_io, portMAX_DELAY);
        const bool an = an_listo();
        if (an) an_vaciar(&s_an_log);
        xSemaphoreGive(s_log_io);
        if (an) return;
    }
    hal_res_delete(LOG_ARCHIVO);
    hal_res_delete(LOG_VIEJO);
}

/* --------------------------------------------------------------- fotos */
/* Cada foto es un archivo QOI en la partición "fotos" (SPIFFS). Los números crecen: la
 * más nueva es la de número mayor. */
#define FOTOS_DIR "/fotos"
#define FOTOS_MAX 96 /* entre todas las mascotas; cada una guarda hasta PET_FOTOS_PERFIL */
static bool s_fotos_ok;

esp_err_t hal_photos_init(void)
{
    esp_vfs_spiffs_conf_t c = {.base_path = FOTOS_DIR, .partition_label = "fotos", .max_files = 2, .format_if_mount_failed = true};
    esp_err_t e = esp_vfs_spiffs_register(&c);
    s_fotos_ok = e == ESP_OK;
    if (!s_fotos_ok) ESP_LOGW(TAG, "sin partición de fotos (%s)", esp_err_to_name(e));
    return e;
}

static void ruta_foto(int id, char *r, size_t n) { snprintf(r, n, FOTOS_DIR "/f%05d.qoi", id); }

int hal_photo_list(uint16_t *ids, int max)
{
    if (!s_fotos_ok || !ids || max <= 0) return 0;
    DIR *d = opendir(FOTOS_DIR);
    if (!d) return 0;
    static uint16_t todas[FOTOS_MAX + 8];
    int n = 0;
    struct dirent *e;
    while ((e = readdir(d)) != NULL && n < FOTOS_MAX + 8) {
        int id;
        if (sscanf(e->d_name, "f%05d.qoi", &id) == 1 && id > 0 && id < 65535) todas[n++] = (uint16_t)id;
    }
    closedir(d);
    for (int i = 1; i < n; i++) { /* de la más nueva a la más vieja */
        uint16_t v = todas[i];
        int k = i;
        while (k > 0 && todas[k - 1] < v) { todas[k] = todas[k - 1]; k--; }
        todas[k] = v;
    }
    if (n > max) n = max;
    memcpy(ids, todas, (size_t)n * sizeof ids[0]);
    return n;
}

int hal_photo_save(const uint8_t *datos, int n)
{
    if (!s_fotos_ok || !datos || n <= 0) return -1;
    static uint16_t ids[FOTOS_MAX];
    int k = hal_photo_list(ids, FOTOS_MAX);
    size_t total = 0, usado = 0;
    esp_spiffs_info("fotos", &total, &usado);
    if (k >= FOTOS_MAX || usado + (size_t)n + 8192 > total) return -1; /* no hay lugar */
    int id = k ? ids[0] + 1 : 1;
    spiffs_limpiar_pasos("fotos", n + 8192); /* (v97) espacio libre primero, de a poco: si no, fwrite limpiaba todo de una vez (>5 s, perro guardián) */
    char r[32];
    ruta_foto(id, r, sizeof r);
    FILE *f = fopen(r, "wb");
    if (!f) return -1;
    size_t w = fwrite(datos, 1, (size_t)n, f);
    fclose(f);
    if ((int)w != n) { remove(r); return -1; }
    return id;
}

/* limpia con calma la flash de las fotos (el "recolector de basura" de SPIFFS): así, cuando llegue una foto, ya hay bloques libres y no se congela 2 s */
void hal_fotos_mantener(void)
{
    if (!s_fotos_ok) return;
    const int64_t t0 = esp_timer_get_time();
    esp_err_t r = esp_spiffs_gc("fotos", 32 * 1024);
    hal_log("fotos: limpieza de la flash %s en %d ms", r == ESP_OK ? "lista" : "no pudo", (int)((esp_timer_get_time() - t0) / 1000));
}

int hal_photo_size(int id)
{
    char r[32];
    ruta_foto(id, r, sizeof r);
    struct stat st;
    return s_fotos_ok && stat(r, &st) == 0 ? (int)st.st_size : -1;
}

int hal_photo_read(int id, uint8_t *buf, int cap)
{
    if (!s_fotos_ok || !buf) return -1;
    char r[32];
    ruta_foto(id, r, sizeof r);
    FILE *f = fopen(r, "rb");
    if (!f) return -1;
    int n = (int)fread(buf, 1, (size_t)cap, f);
    fclose(f);
    return n;
}

bool hal_photo_delete(int id)
{
    char r[32];
    ruta_foto(id, r, sizeof r);
    return s_fotos_ok && remove(r) == 0;
}

/* ------------------------------------------------------------- guardado */

#define NVS_NS "mascota"
#define NVS_KEY "estado"

/* empezar de cero: borra todo lo guardado de la partida (mascota, mundo, objetos, amigas) */
esp_err_t hal_storage_erase_all(void)
{
    nvs_handle_t h;
    ESP_RETURN_ON_ERROR(nvs_open(NVS_NS, NVS_READWRITE, &h), TAG, "nvs");
    esp_err_t e = nvs_erase_all(h);
    if (e == ESP_OK) e = nvs_commit(h);
    nvs_close(h);
    return e;
}

esp_err_t hal_storage_init(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    return err;
}

/* Dos copias de la partida que se turnan ("estado_a" y "estado_b"), cada una con su crc:
 * si una se daña (un corte de luz justo al guardar), queda la otra. Al cargar se usa la
 * copia válida más nueva. "estado" es el formato de antes (una sola copia). */
static const char *const RANURAS[2] = {"estado_a", "estado_b"};
static int s_proxima_ranura;

static bool leer_partida(nvs_handle_t h, const char *clave, pet_t *p)
{
    size_t len = sizeof(*p);
    memset(p, 0, sizeof(*p));
    if (nvs_get_blob(h, clave, p, &len) != ESP_OK) return false;
    if (len == sizeof(*p) && pet_validate(p)) return true;
    /* una partida de una versión anterior (más corta): se actualiza */
    return len < sizeof(*p) && len == p->size && pet_upgrade(p);
}

bool hal_storage_load(pet_t *p)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READONLY, &h) != ESP_OK) return false;
    static EXT_RAM_BSS_ATTR pet_t otra; /* grande: fuera de la pila (y de la RAM interna) */
    bool a = leer_partida(h, RANURAS[0], p);
    bool b = leer_partida(h, RANURAS[1], &otra);
    if (b && (!a || otra.ultima_actualizacion > p->ultima_actualizacion)) {
        *p = otra;
        s_proxima_ranura = 0;
    } else s_proxima_ranura = 1;
    bool ok = a || b;
    if (!ok) ok = leer_partida(h, NVS_KEY, p); /* partida guardada por la versión anterior */
    nvs_close(h);
    if (!ok) return false;
    if (!(a && b)) ESP_LOGW(TAG, "se cargo la partida con %s", a || b ? "una sola copia sana" : "el formato anterior");
    return true;
}

static SemaphoreHandle_t s_guardar_mtx;

/* v88: guardar sin trabar el ciclo principal: se copia la partida y una tarea la escribe. Si una escritura tarda mucho, queda en el resumen de eventos. */
static TaskHandle_t s_guardar_tarea;
static EXT_RAM_BSS_ATTR pet_t s_guardar_copia;
static volatile bool s_guardar_pend;
/* (1.0.26) número de cada guardado: una copia vieja que la tarea tenía pendiente no puede pisar un guardado directo más nuevo
 * (al cambiar de perfil se guarda la que llega con hal_storage_save; si después la tarea escribía la copia de la que se fue,
 * al arrancar podía volver la de antes, que además ya estaba en su lugar de perfiles: dos iguales y la otra perdida) */
static uint32_t s_guardar_num, s_guardar_num_copia, s_guardar_num_escrito;
static void guardar_tarea(void *arg)
{
    (void)arg;
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        if (!s_guardar_pend) continue;
        s_guardar_pend = false;
        hal_storage_save(&s_guardar_copia); /* (el mutex de adentro evita que se cruce con un guardado directo) */
    }
}
void hal_storage_save_async(pet_t *p)
{
    pet_seal(p);
    if (!s_guardar_tarea && xTaskCreatePinnedToCore(guardar_tarea, "guardar", 4096, NULL, 2, &s_guardar_tarea, 0) != pdPASS) s_guardar_tarea = NULL;
    if (!s_guardar_tarea) { hal_storage_save(p); return; }
    if (!s_guardar_mtx) s_guardar_mtx = xSemaphoreCreateMutex();
    if (xSemaphoreTake(s_guardar_mtx, pdMS_TO_TICKS(30)) != pdTRUE) return; /* justo está escribiendo una copia de hace instantes: basta con esa */
    memcpy(&s_guardar_copia, p, sizeof s_guardar_copia);
    s_guardar_num_copia = ++s_guardar_num;
    xSemaphoreGive(s_guardar_mtx);
    s_guardar_pend = true;
    xTaskNotifyGive(s_guardar_tarea);
}

esp_err_t hal_storage_save(pet_t *p)
{
    nvs_handle_t h;
    if (p != &s_guardar_copia) pet_seal(p);
    const int64_t t0 = esp_timer_get_time();
    if (!s_guardar_mtx) s_guardar_mtx = xSemaphoreCreateMutex();
    xSemaphoreTake(s_guardar_mtx, portMAX_DELAY);
    const uint32_t num = p == &s_guardar_copia ? s_guardar_num_copia : ++s_guardar_num;
    if (p == &s_guardar_copia && num < s_guardar_num_escrito) { xSemaphoreGive(s_guardar_mtx); return ESP_OK; } /* ya se guardó algo más nuevo */
    s_guardar_num_escrito = num;
    esp_err_t err = nvs_open(NVS_NS, NVS_READWRITE, &h);
    if (err == ESP_OK) {
        err = nvs_set_blob(h, RANURAS[s_proxima_ranura], p, sizeof(*p));
        if (err == ESP_OK) err = nvs_commit(h);
        nvs_close(h);
        if (err == ESP_OK) s_proxima_ranura ^= 1;
    }
    xSemaphoreGive(s_guardar_mtx);
    const int ms = (int)((esp_timer_get_time() - t0) / 1000);
    if (ms > 400) hal_evento("escritura lenta a flash: guardar la partida tardó %d ms (%u bytes)%s", ms, (unsigned)sizeof(*p), err == ESP_OK ? "" : " y FALLÓ");
    else if (err != ESP_OK) hal_evento("guardar la partida FALLÓ (error %d)", (int)err);
    return err;
}

bool hal_storage_get(const char *clave, void *buf, size_t *len)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READONLY, &h) != ESP_OK) return false;
    esp_err_t err = nvs_get_blob(h, clave, buf, len);
    nvs_close(h);
    return err == ESP_OK;
}

esp_err_t hal_storage_set(const char *clave, const void *buf, size_t len)
{
    nvs_handle_t h;
    ESP_RETURN_ON_ERROR(nvs_open(NVS_NS, NVS_READWRITE, &h), TAG, "nvs");
    esp_err_t err = buf ? nvs_set_blob(h, clave, buf, len) : nvs_erase_key(h, clave);
    if (err == ESP_ERR_NVS_NOT_FOUND) err = ESP_OK;
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    return err;
}

/* ------------------------------------------------------------- USB */
/* Línea de texto por el USB nativo (el mismo puerto de la consola): el editor de Windows manda
 * comandos "MASCOTA ..." y lee las respuestas. Una tarea arma la línea y main la atiende. */
#define USB_LINEA 4800
static EXT_RAM_BSS_ATTR char s_usb_linea[USB_LINEA];
static int s_usb_n;
static volatile bool s_usb_lista;

static void usb_tarea(void *arg)
{
    (void)arg;
    uint8_t c[64];
    while (true) {
        int n = usb_serial_jtag_read_bytes(c, sizeof c, portMAX_DELAY);
        if (n > 0) s_usb_bytes += (uint32_t)n;
        for (int i = 0; i < n; i++) {
            while (s_usb_lista) vTaskDelay(pdMS_TO_TICKS(10)); /* main todavía no la atiende */
            if (c[i] == '\n') {
                s_usb_linea[s_usb_n] = 0;
                if (s_usb_n > 0) s_usb_lineas++;
                if (!strcmp(s_usb_linea, "MASCOTA PING")) { printf("MASCOTA PONG\n"); s_usb_n = 0; continue; } /* responde sin pasar por la tarea principal */
                if (s_usb_n > 0) s_usb_lista = true;
                s_usb_n = 0;
            } else if (c[i] != '\r' && s_usb_n < USB_LINEA - 1) s_usb_linea[s_usb_n++] = (char)c[i];
        }
    }
}

esp_err_t hal_usb_init(void)
{
    usb_serial_jtag_driver_config_t cfg = USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
    cfg.rx_buffer_size = 1024;
    cfg.tx_buffer_size = 1024;
    ESP_RETURN_ON_ERROR(usb_serial_jtag_driver_install(&cfg), TAG, "usb");
    usb_serial_jtag_vfs_use_driver(); /* printf también pasa por el driver */
    if (xTaskCreate(usb_tarea, "usb", 3072, NULL, 3, NULL) != pdPASS) {
        ESP_LOGE(TAG, "sin memoria para la tarea del USB: no se podrán recibir comandos");
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

const char *hal_usb_line(void) { return s_usb_lista ? s_usb_linea : NULL; }
void hal_usb_line_done(void) { s_usb_lista = false; }

/* ---------------------------------------------------------------- botón */

void hal_button_init(void)
{
    gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << BOARD_BTN_BOOT,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&cfg);
}

bool hal_button_pressed(void) { return gpio_get_level(BOARD_BTN_BOOT) == 0; }

bool hal_sound_open(void) { return s_par_abierto; }

/* ---------------------------------------------------------- modo reserva */
#define RESERVA_MAGIC 0x52455356u   /* "RESV" */
/* (v100) antes despertaba cada 20 min a ver si lo habían enchufado, y solo salía si el cargador ya decía «cargando»: al
 * conectar el cable parecía muerto (hubo que sacar la batería). Ahora mira cada 20 s las primeras 3 horas (después cada
 * 5 min, para que lo poco que queda alcance para el reloj) y sale apenas hay cable. */
#define RESERVA_REVISAR_S 20u
#define RESERVA_REVISAR_LENTO_S (5u * 60u)
#define RESERVA_RAPIDAS 540u
#define RESERVA_SALIR_PCT 12        /* con esto (o cargando) vuelve a encenderse */
static RTC_DATA_ATTR uint32_t s_reserva, s_reserva_alarma, s_reserva_n;

static void dormir_reserva(uint32_t ahora)
{
    s_reserva = RESERVA_MAGIC;
    uint64_t seg = s_reserva_n < RESERVA_RAPIDAS ? RESERVA_REVISAR_S : RESERVA_REVISAR_LENTO_S;
    s_reserva_n++;
    if (s_reserva_alarma > ahora + 60 && s_reserva_alarma - ahora - 60 < seg) seg = s_reserva_alarma - ahora - 60; /* 1 min antes */
    if (s_imu) escribir(s_imu, 0x08, 0x00); /* acelerómetro apagado */
    if (s_parlante) { esp_codec_dev_close(s_parlante); s_par_abierto = false; }
    esp_sleep_enable_timer_wakeup(seg * 1000000ULL);
    rtc_gpio_pullup_en((gpio_num_t)BOARD_BTN_BOOT);
    esp_sleep_enable_ext0_wakeup((gpio_num_t)BOARD_BTN_BOOT, 0);
    ESP_LOGW(TAG, "modo reserva: duermo %u s", (unsigned)seg);
    esp_deep_sleep_start();
}

void hal_reserve_enter(uint32_t alarma_utc)
{
    s_reserva_alarma = alarma_utc;
    s_reserva_n = 0;
    bsp_display_brightness_set(0);
    bsp_display_sleep(true); /* (v100) el panel también a reposo */
    dormir_reserva(hal_rtc_now());
}

/* ---------------------------------------------------------- (v101) sueño profundo de noche */
#define NOCHE_MAGIC 0x4E4F4348u /* "NOCH" */
static RTC_DATA_ATTR uint32_t s_noche, s_noche_hasta, s_noche_n, s_noche_motivo;
static void dormir_noche(uint32_t ahora)
{
    s_noche = NOCHE_MAGIC;
    uint64_t seg = BOARD_NOCHE_REVISAR_S;
    if (s_noche_hasta > ahora && s_noche_hasta - ahora < seg) seg = s_noche_hasta - ahora;
    if (seg < 1) seg = 1;
    if (s_imu) escribir(s_imu, 0x08, 0x00); /* acelerómetro apagado */
    if (s_parlante) { esp_codec_dev_close(s_parlante); s_par_abierto = false; }
    esp_sleep_enable_timer_wakeup(seg * 1000000ULL);
    rtc_gpio_pullup_en((gpio_num_t)BOARD_BTN_BOOT);
    esp_sleep_enable_ext0_wakeup((gpio_num_t)BOARD_BTN_BOOT, 0);
    esp_deep_sleep_start();
}
void hal_night_enter(uint32_t hasta_utc)
{
    s_noche_hasta = hasta_utc;
    s_noche_n = 0;
    s_noche_motivo = 0;
    if (s_pmu) escribir(s_pmu, 0x49, 0x08); /* borra un PWR viejo: solo cuenta el que aprieten durmiendo */
    bsp_display_brightness_set(0);
    bsp_display_sleep(true);
    dormir_noche(hal_rtc_now());
}
void hal_night_boot_check(void)
{
    if (s_noche != NOCHE_MAGIC) { if (esp_reset_reason() != ESP_RST_DEEPSLEEP) s_noche_motivo = 0; return; }
    const bool boton = esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT0;
    uint32_t motivo = boton ? 1u : 0u;
    if (!motivo) { /* lo justo para mirar: el bus I2C, la AXP2101 y el reloj */
        bsp_i2c_init();
        hal_power_init();
        hal_rtc_init();
        const uint32_t ahora = hal_rtc_now();
        if (s_pwr_al_iniciar || hal_pwr_short_press()) motivo = 2;
        else if (hal_battery_vbus() || hal_battery_charging()) motivo = 3;
        else if (ahora + 2u >= s_noche_hasta) motivo = 4;
        else if (ahora < 1767225600u || s_noche_n > 14u * 3600u / BOARD_NOCHE_REVISAR_S) motivo = 5; /* reloj raro o demasiado tiempo: mejor despertar */
        if (!motivo) { s_noche_n++; dormir_noche(ahora); } /* no vuelve */
    }
    s_noche = 0;
    s_noche_motivo = motivo;
}
const char *hal_night_wake_reason(void)
{
    static const char *const M[] = {NULL, "botón BOOT", "botón PWR", "cable", "terminó la noche o viene una alarma", "por las dudas"};
    const char *r = s_noche_motivo < 6 ? M[s_noche_motivo] : NULL;
    s_noche_motivo = 0; /* se cuenta una sola vez */
    return r;
}

/* ---------------------------------------------------------- (1.0.54) modo paseo: el procesador duerme y el chip cuenta los pasos
 * Caminando con la pantalla apagada, la placa guarda todo y entra en sueño profundo; el contador de pasos del QMI8658 (su
 * propio motor, casi no gasta) sigue encendido. Cada BOARD_PASEO_REVISAR_S despierta un instante (sin arrancar el juego):
 * lee los pasos, mira el botón PWR, el cable y la próxima alarma, y cada minuto anota si caminaste. Arranca el juego con
 * BOOT o PWR (pantalla encendida, «¡VOLVISTE!»), con el cable, 1 min antes de una alarma, a las 4 h, o cuando llevas 3
 * minutos sin caminar (el paseo terminó: arranca a oscuras y el paseo se cuenta como siempre). */
#define PASEO_MAGIC 0x50415345u /* "PASE" */
#define BOARD_PASEO_REVISAR_S 5u
static RTC_DATA_ATTR uint32_t s_pas, s_pas_alarma, s_pas_ult, s_pas_pend, s_pas_desde, s_pas_min_ini, s_pas_min_pasos,
    s_pas_min_cam, s_pas_quietos, s_pas_motivo;
static void dormir_paseo(uint32_t ahora)
{
    s_pas = PASEO_MAGIC;
    uint64_t seg = BOARD_PASEO_REVISAR_S;
    if (s_pas_alarma > ahora + 60u && s_pas_alarma - ahora - 60u < seg) seg = s_pas_alarma - ahora - 60u;
    if (seg < 1) seg = 1;
    /* (el acelerómetro queda encendido: es el que cuenta los pasos) */
    if (s_parlante) { esp_codec_dev_close(s_parlante); s_par_abierto = false; }
    esp_sleep_enable_timer_wakeup(seg * 1000000ULL);
    rtc_gpio_pullup_en((gpio_num_t)BOARD_BTN_BOOT);
    esp_sleep_enable_ext0_wakeup((gpio_num_t)BOARD_BTN_BOOT, 0);
    esp_deep_sleep_start();
}
void hal_walk_enter(uint32_t alarma_utc, uint32_t cuenta_chip)
{
    const uint32_t ahora = hal_rtc_now();
    s_pas_alarma = alarma_utc;
    s_pas_ult = cuenta_chip;
    s_pas_pend = 0;
    s_pas_desde = ahora;
    s_pas_min_ini = ahora;
    s_pas_min_pasos = 0;
    s_pas_min_cam = 0;
    s_pas_quietos = 0;
    s_pas_motivo = 0;
    if (s_pmu) escribir(s_pmu, 0x49, 0x08); /* borra un PWR viejo: solo cuenta el que aprieten durmiendo */
    bsp_display_brightness_set(0);
    bsp_display_sleep(true);
    dormir_paseo(ahora);
}
static void paseo_leer_pasos(void)
{
    if (!s_imu && i2c_dev(BOARD_IMU_ADDR, &s_imu) != ESP_OK) return;
    uint8_t d[3];
    if (leer(s_imu, 0x5A, d, 3) != ESP_OK) return;
    const uint32_t c = (uint32_t)d[0] | (uint32_t)d[1] << 8 | (uint32_t)d[2] << 16;
    const uint32_t dp = c >= s_pas_ult ? c - s_pas_ult : c; /* (si el chip volvió a cero, cuenta desde ahí) */
    s_pas_ult = c;
    if (dp < 2000u) { s_pas_pend += dp; s_pas_min_pasos += dp; } /* (una lectura imposible no cuenta) */
}
void hal_walk_boot_check(void)
{
    if (s_pas != PASEO_MAGIC) { if (esp_reset_reason() != ESP_RST_DEEPSLEEP) s_pas_motivo = 0; return; }
    bsp_i2c_init();
    paseo_leer_pasos(); /* siempre: también si apretaste BOOT (los pasos del último ratito no se pierden) */
    uint32_t motivo = esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT0 ? 1u : 0u;
    if (!motivo) {
        hal_power_init();
        hal_rtc_init();
        const uint32_t ahora = hal_rtc_now();
        if (ahora >= s_pas_min_ini + 60u) { /* pasó un minuto: ¿caminaste? (40 pasos o más, como el paseo de siempre) */
            if (s_pas_min_pasos >= 40u) { s_pas_min_cam++; s_pas_quietos = 0; } else s_pas_quietos++;
            s_pas_min_pasos = 0;
            s_pas_min_ini = ahora;
        }
        if (s_pwr_al_iniciar || hal_pwr_short_press()) motivo = 2;
        else if (hal_battery_vbus() || hal_battery_charging()) motivo = 3;
        else if (s_pas_alarma && ahora + 60u >= s_pas_alarma) motivo = 4;
        else if (s_pas_quietos >= 3u) motivo = 5;
        else if (ahora < 1767225600u || ahora < s_pas_desde || ahora - s_pas_desde > 4u * 3600u) motivo = 6;
        if (!motivo) dormir_paseo(ahora); /* no vuelve */
    }
    s_pas = 0;
    s_pas_motivo = motivo;
}
uint32_t hal_walk_take(uint32_t *min_caminando, uint32_t *min_quieto, bool *a_oscuras)
{
    static const bool OSCURAS[7] = {false, false, false, false, true, true, true};
    *min_caminando = s_pas_min_cam;
    *min_quieto = s_pas_quietos;
    *a_oscuras = s_pas_motivo < 7 && OSCURAS[s_pas_motivo];
    const uint32_t p = s_pas_pend;
    s_pas_pend = 0; s_pas_min_cam = 0; s_pas_quietos = 0;
    return p;
}
const char *hal_walk_wake_reason(void)
{
    static const char *const M[] = {NULL, "botón BOOT", "botón PWR", "cable", "viene una alarma", "dejaste de caminar", "por las dudas"};
    const char *r = s_pas_motivo < 7 ? M[s_pas_motivo] : NULL;
    s_pas_motivo = 0;
    return r;
}

void hal_reserve_boot_check(void)
{
    if (s_reserva != RESERVA_MAGIC) return;
    bsp_i2c_init();
    hal_power_init();
    hal_rtc_init();
    int pct = hal_battery_percent();
    bool carga = hal_battery_charging() || hal_battery_vbus(); /* (v100) basta con que haya cable: con la batería muy baja el cargador tarda en decir «cargando» */
    uint32_t ahora = hal_rtc_now();
    bool alarma = s_reserva_alarma && ahora + 90 >= s_reserva_alarma;
    const bool boton = esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT0; /* apretaste BOOT: enciende aunque la batería esté baja */
    const int mv = hal_battery_mv();
    const bool baja = (pct >= 0 && pct < RESERVA_SALIR_PCT) || (mv > 2500 && mv < 3500); /* (v101) también por voltaje: el medidor se equivoca al final */
    if (!carga && !alarma && !boton && baja) dormir_reserva(ahora); /* no vuelve */
    ESP_LOGI(TAG, "salgo del modo reserva (%d %%, %s)", pct, carga ? "cargando" : (alarma ? "alarma" : "con batería"));
    s_reserva = 0;
    s_reserva_n = 0;
    s_reserva_alarma = 0;
}
