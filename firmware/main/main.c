/*
 * Firmware de la mascota virtual para la Waveshare ESP32-S3-Touch-AMOLED-1.75.
 *
 * Una sola tarea es dueña del estado de la mascota:
 *   - recibe las acciones que la interfaz pone en una cola (incluido decorar)
 *   - avanza el motor con la hora real
 *   - reparte los eventos a la interfaz y al parlante
 *   - lee el acelerómetro: sacudir = jugar con ella; mover la muñeca = encender pantalla
 *   - muestra la batería, guarda en flash y apaga la pantalla tras 20 s sin uso
 *   - emite y escucha balizas Bluetooth para encontrar amigos cercanos
 *   - atiende los comandos del editor por el USB: respaldo de la mascota y celebraciones
 *   - si el reloj perdió la hora (sin batería), la pregunta en pantalla y no avanza mientras
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#include "esp_log.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "esp_random.h"
#include "soc/rtc_cntl_reg.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "hal.h"
#include "ota.h"     /* (1.0.09) actualizar por Wi-Fi */
#include "version.h" /* (1.0.09) FW_VERSION */
#include "esp_timer.h"
#include "esp_pm.h"
#include "esp_sleep.h"
#include "driver/gpio.h"
#include "esp_cpu.h"
#include "esp_private/esp_clk.h"
#include "pet.h"
#include "art.h"
#include "ui.h"
#include "board.h"

/* los bloques grandes que no se usan a cada rato van a la PSRAM: la RAM interna es lo que escasea */
#ifdef ESP_PLATFORM
#include "esp_attr.h"

/* Cada sonido que pide el juego o la interfaz queda anotado en el registro (nombre y modo), para saber qué sonó y cuándo.
 * Las notas de los cristales no se anotan: serían decenas por minuto. */
static void sonido_anotar(sound_t s, const char *modo)
{
    if (s >= SND_CRISTAL_AZUL && s <= SND_CRISTAL_BLANCO) return;
    hal_log("sonido: %s (%s)", pet_sound_name(s), modo);
}
static void sonido_suena(sound_t s) { sonido_anotar(s, "efecto"); hal_sound_play(s); }
static void sonido_suena_suave(sound_t s) { sonido_anotar(s, "suave"); hal_sound_play_soft(s); }
#define hal_sound_play sonido_suena
#define hal_sound_play_soft sonido_suena_suave

#endif
#ifndef EXT_RAM_BSS_ATTR
#define EXT_RAM_BSS_ATTR
#endif

static const char *TAG = "mascota";

/* ---------- configuración ---------- */
#define ZONA_HORARIA_S (-3 * 3600) /* Chile continental en horario de verano */
#define BALIZA_CADA_S 2
#define GUARDAR_CADA_S 300
static int64_t s_sec_us[12]; /* cuándo empezó cada paso del ciclo principal (para saber cuál se pega) */
#define HORA_MINIMA_VALIDA 1767225600u /* 2026-01-01 */
#define RESERVA_ENTRAR_PCT 10             /* con 10 % o menos (sin cargar) pasa a modo reserva: el medidor de la AXP2101 baja de golpe al final y se apagaba sin avisar */
#define RESERVA_ENTRAR_MV 3420            /* (v101) o con la batería bajo 3,42 V: el 12-nov marcaba 9 % y se apagó un minuto después (al enchufarla: 2,97 V) */
#define RESERVA_LECTURAS 6                /* lecturas seguidas (una cada 3 s) antes de entrar: antes 10 (30 s) y no alcanzó */
#ifndef BUILD_UNIX_TIME
#define BUILD_UNIX_TIME HORA_MINIMA_VALIDA
#endif

typedef struct {
    uint8_t accion;
    int32_t arg; /* la fecha de nacimiento va empaquetada (AAAAMMDD) */
} accion_msg_t;

static pet_t s_pet; /* (1.0.15) de vuelta en la RAM interna: el motor la recorre todo el tiempo (en la PSRAM se notaba más lento) */
static QueueHandle_t s_acciones;
static volatile bool s_guardar_ext; /* un comando del probador cambió la partida: guardarla pronto */
static lv_display_t *s_disp;
static bool s_pantalla_on = true;
static bool s_apagada_boton; /* la apagaste con el botón: sigue apagada hasta que lo vuelvas a apretar */
/* matriz del acelerómetro al marco de la pantalla: de fábrica la medida en la placa (el chip está girado 90°); se puede recalibrar */
static const int8_t GIRO_M_FABRICA[4] = {0, 1, -1, 0};
static int8_t s_giro_m[4] = {0, 1, -1, 0};
static float s_giro_raw_x, s_giro_raw_y;  /* última lectura del acelerómetro (sin matriz) */
static int8_t s_giro_u[2], s_giro_v[2];   /* ejes medidos: derecho y lado derecho abajo */
static void giro_cargar(void)
{
    int8_t t[4]; size_t len = sizeof t;
    if (hal_storage_get("giroM", t, &len) && len == sizeof t) memcpy(s_giro_m, t, sizeof t);
    ESP_LOGW("giro", "matriz del acelerómetro: %d %d %d %d (0 1 -1 0 = de fábrica)", s_giro_m[0], s_giro_m[1], s_giro_m[2], s_giro_m[3]);
}
/* mide hacia dónde cae la gravedad con el orbe en la mano: eje dominante y su signo (0: derecho, 1: lado derecho abajo) */
static bool giro_medir(int cual)
{
    const float x = s_giro_raw_x, y = s_giro_raw_y;
    const float ax = x < 0 ? -x : x, ay = y < 0 ? -y : y;
    if ((ax > ay ? ax : ay) < 0.35f) return false; /* acostado: no hay gravedad en el plano */
    int8_t v[2] = {0, 0};
    if (ax > ay) v[0] = x > 0 ? 1 : -1; else v[1] = y > 0 ? 1 : -1;
    if (cual == 0) { s_giro_u[0] = v[0]; s_giro_u[1] = v[1]; }
    else {
        if (v[0] * s_giro_u[0] + v[1] * s_giro_u[1] != 0 || (!s_giro_u[0] && !s_giro_u[1])) return false; /* tiene que ser un cuarto de vuelta */
        s_giro_v[0] = v[0]; s_giro_v[1] = v[1];
        s_giro_m[0] = s_giro_v[0]; s_giro_m[1] = s_giro_v[1];
        s_giro_m[2] = s_giro_u[0]; s_giro_m[3] = s_giro_u[1];
        hal_storage_set("giroM", s_giro_m, sizeof s_giro_m);
    }
    return true;
}
static uint32_t s_arrullo_prueba_ms; /* hasta cuándo suenan los grillos de prueba al mover la barra del arrullo */
static pet_device_t s_dispo;     /* volumen, brillo y cuándo se apaga: del dispositivo (NVS "disp") */
static bool s_calor;             /* protección de calor activa: brillo bajo hasta que se enfríe */
static bool s_quiere_mic; /* (v101) la interfaz espera un soplido (se pregunta en el paso 4) */
static int s_brillo = 100;       /* brillo encendida: menos en modo pecera */
static bool s_cargando;
static int32_t s_zona = ZONA_HORARIA_S; /* la del computador si el editor la mandó (horario de verano o invierno) */
static bool s_hora_dudosa;      /* el RTC perdió la hora: se pregunta en pantalla */
static EXT_RAM_BSS_ATTR pet_t s_respaldo;        /* un respaldo llegado por USB, esperando que lo confirmes */
static bool s_hay_respaldo;

static void on_action(int a, int arg) /* desde la tarea de LVGL: solo encola */
{
    if (a == UI_ACCION_SONAR) { /* un sonido pedido por la interfaz (cristales, notas...) suena AL INSTANTE: antes esperaba su turno en el ciclo principal (50 a 145 ms, o más si se pegaba) */
        if (arg & 0x100) hal_sound_play_soft((sound_t)(arg & 0xFF)); else hal_sound_play((sound_t)arg);
        return;
    }
    accion_msg_t m = {.accion = (uint8_t)a, .arg = (int32_t)arg};
    xQueueSend(s_acciones, &m, 0);
}

static uint32_t ahora(void) { return (uint32_t)((int64_t)time(NULL) + s_pet.desfase_s); } /* la hora real + el viaje en el tiempo de este perfil */

/* ahorro de energía: con la pantalla apagada (y sin cargar, sin sonido) el procesador baja a 80 MHz */
static esp_pm_lock_handle_t s_pm_rapido;
static bool s_pm_tomado;
static uint32_t s_inactivo_ms; /* hace cuánto nadie toca la pantalla */
static volatile int s_n_sac, s_n_mov, s_n_pwr1, s_n_pwr2, s_giro_pos; /* contadores para las pruebas guiadas: sacudidas, movimientos, botón PWR (una y dos veces), giro actual */
static int s_amb_dbg;           /* el ambiente que suena (para el probador) */
static struct { uint32_t ms; int16_t mv; int8_t pct; uint8_t vbus; } s_cev[8]; /* cambios del cable USB / cargador (para la prueba de carga: el computador no oye mientras está desconectado) */
static int s_cev_n;
static uint32_t s_espera_ms = 50; /* la pausa de cada vuelta del ciclo principal (v88: para no anotar como "lento" lo que es la pausa normal) */
static int64_t s_p4[5];          /* tiempos dentro del paso 4 (motor y pantalla) para el aviso "lento" */
static bool s_profundo;         /* pantalla apagada y ella dormida, sin nada activo: el bucle gira más lento y el motor se actualiza cada 3 s (las necesidades igual avanzan: van por la hora real) */
static bool s_energia_suave;    /* dormida y quieta: CPU lenta y latidos espaciados */
static bool s_vbus = true;        /* (v100) hay cable USB (hasta leerlo, se supone que sí: no duerme) */
static bool s_atenuada;         /* (v99) pantalla encendida, nadie la toca hace 8 s y nada que leer ni jugar: brillo bajo, menos cuadros y CPU lenta */
static void cpu_rapido(bool rapido)
{
    if (!s_pm_rapido || s_pm_tomado == rapido) return;
    if (rapido) esp_pm_lock_acquire(s_pm_rapido); else esp_pm_lock_release(s_pm_rapido);
    s_pm_tomado = rapido;
}
/* (v100, batería etapa 3) sueño ligero: con la pantalla apagada, sin cable, sin sonido y sin alarma, se detiene LVGL y se
 * suelta el candado «despierto»: el procesador duerme entre vuelta y vuelta (despierta solo, 4 veces por segundo, y al
 * apretar BOOT). Ella no se congela: el ciclo sigue y el motor avanza por la hora real (prueba P0). */
static esp_pm_lock_handle_t s_pm_despierto;
static bool s_ls_ok, s_dormitando;
static uint32_t s_dormita_n; /* cuántas veces entró (para el registro y el probador) */
static void dormitar(bool on)
{
    if (!s_ls_ok || on == s_dormitando) return;
    if (on) {
        static bool gpio_listo;
        if (!gpio_listo) { gpio_listo = true; gpio_wakeup_enable((gpio_num_t)BOARD_BTN_BOOT, GPIO_INTR_LOW_LEVEL); esp_sleep_enable_gpio_wakeup(); }
        if (!hal_display_pause(true)) return; /* LVGL no se pudo detener: se queda despierto */
        esp_pm_lock_release(s_pm_despierto);
        s_dormita_n++;
    } else {
        esp_pm_lock_acquire(s_pm_despierto);
        hal_display_pause(false);
    }
    s_dormitando = on;
    hal_log("energia: sueño ligero %s", on ? "SÍ (pantalla apagada, sin cable ni sonido)" : "no");
}
static void cpu_ahorro_iniciar(void)
{
    esp_pm_config_t pm = { .max_freq_mhz = 240, .min_freq_mhz = 80, .light_sleep_enable = false };
#if BOARD_SUENO_LIGERO && defined(CONFIG_FREERTOS_USE_TICKLESS_IDLE)
    pm.light_sleep_enable = true;
    if (esp_pm_configure(&pm) == ESP_OK && esp_pm_lock_create(ESP_PM_NO_LIGHT_SLEEP, 0, "despierto", &s_pm_despierto) == ESP_OK) {
        esp_pm_lock_acquire(s_pm_despierto); /* arranca despierto */
        s_ls_ok = true;
    } else {
        s_pm_despierto = NULL;
        pm.light_sleep_enable = false;
    }
#endif
    if (!s_ls_ok && esp_pm_configure(&pm) != ESP_OK) return;
    if (esp_pm_lock_create(ESP_PM_CPU_FREQ_MAX, 0, "rapido", &s_pm_rapido) != ESP_OK) { s_pm_rapido = NULL; return; }
    esp_pm_lock_acquire(s_pm_rapido); /* arranca rápido */
    s_pm_tomado = true;
}

static void encender_pantalla(bool on, int brillo)
{
    if (on == s_pantalla_on && (!on || brillo == s_brillo)) return;
    const bool cambia = on != s_pantalla_on;
    if (cambia && on) hal_display_sleep(false); /* (v99) primero despierta el panel (120 ms) y después se dibuja */
    if (cambia) hal_touch_enable(on); /* en el bolsillo, con la pantalla apagada, el táctil no cuenta */
    s_pantalla_on = on;
    if (cambia && hal_display_lock()) { ui_screen_on(on); hal_display_unlock(); } /* (v99: solo al cambiar; atenuar no la redibuja) */
    hal_sound_pantalla(on); /* apagada: el parlante se duerme tras un rato; encendida: se despierta antes de que haga falta */
    s_brillo = brillo;
    hal_display_brightness(on ? brillo : 0);
    if (cambia && !on) hal_display_sleep(true); /* (v99) apagada: el panel y el táctil a reposo de verdad, no solo brillo 0 */
}

/* ======================================================================
 * Prueba de DESCARGA de la batería: la placa corre sola por etapas (peor caso, uso normal, dormida) con una pantalla fija encima,
 * y cada 2 minutos anota en su registro: batería, temperaturas, frecuencia pedida, frecuencia medida y carga de cada núcleo.
 * ==================================================================== */
static struct {
    bool activo, largo, espera_cable, pausa;
    int etapa, ciclo, pct0, pct_et0, mhz_n, mhz_min;
    uint32_t t0, et0, ult_log, ult_pant, cc_ant, id_ant[2], tot_ant;
    int64_t us_ant;
    float mhz_sum;
} s_desc;
static volatile bool s_carga_on;
static bool s_carga_creada;
static const char *const DESC_ETAPA[3] = {"PEOR CASO", "USO NORMAL", "DORMIDA"};

static void carga_tarea(void *arg)
{
    (void)arg;
    volatile uint32_t x = 1;
    while (true) {
        if (!s_carga_on) { vTaskDelay(pdMS_TO_TICKS(200)); continue; }
        const int64_t fin = esp_timer_get_time() + 200000; /* 200 ms de cuentas seguidas y 1 ms de respiro: el núcleo queda ~99 % ocupado sin matar al perro guardián */
        while (esp_timer_get_time() < fin) for (int i = 0; i < 2000; i++) x = x * 1664525u + 1013904223u;
        vTaskDelay(1);
    }
}

static void descarga_log(const char *tag)
{
    uint32_t id[2] = {0, 0}, tot = 0;
    static TaskStatus_t st[32];
    const UBaseType_t n = uxTaskGetSystemState(st, 32, &tot);
    for (UBaseType_t i = 0; i < n; i++) {
        if (!strcmp(st[i].pcTaskName, "IDLE0")) id[0] = st[i].ulRunTimeCounter;
        else if (!strcmp(st[i].pcTaskName, "IDLE1")) id[1] = st[i].ulRunTimeCounter;
    }
    int c0 = -1, c1 = -1;
    const uint32_t dt = tot - s_desc.tot_ant;
    if (dt > 0 && s_desc.tot_ant) {
        c0 = 100 - (int)(100ull * (id[0] - s_desc.id_ant[0]) / dt);
        c1 = 100 - (int)(100ull * (id[1] - s_desc.id_ant[1]) / dt);
    }
    s_desc.tot_ant = tot; s_desc.id_ant[0] = id[0]; s_desc.id_ant[1] = id[1];
    const int tc = hal_temp_chip(), ti = hal_temp_imu();
    const uint32_t ms = (uint32_t)(esp_timer_get_time() / 1000);
    hal_log("DESCARGA %s t=%u min etapa=%d ciclo=%d pct=%d mv=%d chip=%d imu=%d mhz_pedido=%d mhz_medido=%d mhz_min=%d carga0=%d carga1=%d brillo=%d vbus=%d",
            tag, (unsigned)((ms - s_desc.t0) / 60000u), s_desc.etapa, s_desc.ciclo, hal_battery_percent(), hal_battery_mv(), tc, ti, (int)(esp_clk_cpu_freq() / 1000000),
            s_desc.mhz_n ? (int)(s_desc.mhz_sum / (float)s_desc.mhz_n) : -1, s_desc.mhz_n ? s_desc.mhz_min : -1, c0, c1, s_pantalla_on ? s_brillo : 0, hal_battery_vbus() ? 1 : 0);
    s_desc.mhz_sum = 0; s_desc.mhz_n = 0; s_desc.mhz_min = 100000;
    s_desc.ult_log = ms;
}

static void descarga_pantalla(const char *l0, const char *l1, const char *l2, const char *l3, bool animar)
{
    if (hal_display_lock()) { ui_test_descarga(l0, l1, l2, l3, animar); hal_display_unlock(); }
}

static void descarga_etapa(int e)
{
    s_desc.etapa = e;
    s_desc.et0 = (uint32_t)(esp_timer_get_time() / 1000);
    s_desc.pct_et0 = hal_battery_percent();
    if (e == 0) { encender_pantalla(true, 100); cpu_rapido(true); s_carga_on = true; hal_mic_enable(true); }
    else if (e == 1) { s_carga_on = false; hal_mic_enable(false); cpu_rapido(false); encender_pantalla(true, 50); }
    else { s_carga_on = false; hal_mic_enable(false); cpu_rapido(false); encender_pantalla(false, 0); }
    descarga_log("etapa");
}

static void descarga_fin(const char *motivo)
{
    descarga_log("fin");
    hal_log("DESCARGA fin: %s", motivo);
    s_carga_on = false;
    hal_mic_enable(false);
    s_desc.activo = false;
    descarga_pantalla(NULL, NULL, NULL, NULL, false);
    s_apagada_boton = false;
    s_brillo = -1;
    encender_pantalla(true, 100);
}

static bool descarga_iniciar(bool largo)
{
    if (s_desc.activo) return true;
    if (hal_battery_percent() < 90) return false;
    if (!s_carga_creada) {
        s_carga_creada = true;
        xTaskCreatePinnedToCore(carga_tarea, "carga0", 2048, NULL, 1, NULL, 0);
        xTaskCreatePinnedToCore(carga_tarea, "carga1", 2048, NULL, 1, NULL, 1);
    }
    memset(&s_desc, 0, sizeof s_desc);
    s_desc.activo = s_desc.espera_cable = true;
    s_desc.largo = largo;
    s_desc.mhz_min = 100000;
    s_desc.t0 = (uint32_t)(esp_timer_get_time() / 1000);
    s_desc.pct0 = hal_battery_percent();
    hal_log("DESCARGA inicio (%s) con %d %%", largo ? "larga, hasta el 10 %" : "corta, una vuelta de 3 etapas", s_desc.pct0);
    if (hal_display_lock()) { ui_close_panel(); hal_display_unlock(); }
    encender_pantalla(true, 60);
    descarga_pantalla("PRUEBA DE DESCARGA", "Desenchufa el cable USB", "para empezar", "No uses la mascota", false);
    return true;
}

static void descarga_paso(void)
{
    const uint32_t ms = (uint32_t)(esp_timer_get_time() / 1000);
    const int64_t us = esp_timer_get_time();
    const uint32_t cc = esp_cpu_get_cycle_count();
    if (s_desc.us_ant && us - s_desc.us_ant > 20000) { /* frecuencia MEDIDA: ciclos del procesador entre dos instantes conocidos */
        const int mhz = (int)((cc - s_desc.cc_ant) / (uint32_t)(us - s_desc.us_ant));
        s_desc.mhz_sum += (float)mhz; s_desc.mhz_n++;
        if (mhz < s_desc.mhz_min) s_desc.mhz_min = mhz;
    }
    s_desc.cc_ant = cc; s_desc.us_ant = us;

    if (hal_pwr_short_press()) { /* PWR dos veces seguidas: cancela */
        static uint32_t antes;
        if (antes && ms - antes < 800) { descarga_fin("cancelada con PWR (dos veces)"); return; }
        antes = ms;
    }
    const bool vbus = hal_battery_vbus();
    if (s_desc.espera_cable) {
        if (!vbus) {
            s_desc.espera_cable = false;
            s_desc.t0 = ms;
            s_desc.pct0 = hal_battery_percent();
            hal_log("DESCARGA: cable fuera, empieza con %d %%", s_desc.pct0);
            descarga_etapa(0);
        }
        return;
    }
    if (vbus != s_desc.pausa) { /* enchufaron o desenchufaron: se pausa mientras haya cable (con carga la medición no vale) */
        s_desc.pausa = vbus;
        if (vbus) { s_carga_on = false; hal_log("DESCARGA: pausa, hay un cable enchufado"); descarga_pantalla("PRUEBA DE DESCARGA", "PAUSA", "Desenchufa el cable USB", "", false); }
        else { hal_log("DESCARGA: sigue, cable fuera"); descarga_etapa(s_desc.etapa); }
    }
    if (s_desc.pausa) return;

    /* qué hace el hardware en cada etapa */
    if (s_desc.etapa == 0) { /* peor caso: sensores y radio sin parar */
        bool a, b; uint8_t baliza[PET_BALIZA_BYTES];
        hal_imu_poll(&a, &b);
        (void)hal_radio_poll(baliza);
    } else if (s_desc.etapa == 1) {
        static uint32_t u;
        if (ms - u >= 1000) { u = ms; bool a, b; hal_imu_poll(&a, &b); }
    }

    const int pct = hal_battery_percent();
    if (ms - s_desc.ult_log >= 120000u) descarga_log("medida");
    if (s_desc.etapa < 2 && ms - s_desc.ult_pant >= 1000 && s_pantalla_on) {
        s_desc.ult_pant = ms;
        const unsigned min = (ms - s_desc.t0) / 60000u;
        char b1[48], b2[48], b3[48];
        snprintf(b1, sizeof b1, "etapa: %s", DESC_ETAPA[s_desc.etapa]);
        snprintf(b2, sizeof b2, "bateria %d %%   %uh %02umin", pct, min / 60, min % 60);
        snprintf(b3, sizeof b3, "PWR dos veces = cancelar");
        descarga_pantalla("PRUEBA DE DESCARGA", b1, b2, b3, true);
    }
    /* protección de calor */
    if (s_desc.etapa == 0) {
        const int tc = hal_temp_chip(), ti = hal_temp_imu();
        if (tc >= 750 || ti >= 650) { hal_log("DESCARGA: calor (procesador %d, acelerómetro %d décimas de °C): se pasa a la etapa tranquila", tc, ti); descarga_etapa(1); return; }
    }
    if (pct >= 0 && pct <= 10) { descarga_fin("batería al 10 %"); return; }
    if ((ms - s_desc.et0) / 60000u >= 20u || (s_desc.largo && pct >= 0 && s_desc.pct_et0 - pct >= 5)) { /* cada etapa: 20 min o 5 puntos de batería */
        if (s_desc.etapa < 2) descarga_etapa(s_desc.etapa + 1);
        else {
            s_desc.ciclo++;
            if (!s_desc.largo) descarga_fin("vuelta completa de 3 etapas");
            else descarga_etapa(0);
        }
    }
}

static void cargar_dispositivo(void)
{
    size_t len = sizeof s_dispo;
    memset(&s_dispo, 0, sizeof s_dispo); /* la versión 1 era más corta: lo nuevo queda en cero y pet_device_fix lo completa */
    if (!hal_storage_get("disp", &s_dispo, &len) || len > sizeof s_dispo || len < 4) pet_device_defaults(&s_dispo);
    pet_device_fix(&s_dispo);
    giro_cargar();
}
static void guardar_dispositivo(void) { hal_storage_set("disp", &s_dispo, sizeof s_dispo); }

/* la zona horaria que mandó el editor (queda guardada) */
static void cargar_zona(void)
{
    int32_t z;
    size_t len = sizeof z;
    if (hal_storage_get("zona", &z, &len) && len == sizeof z && z >= -14 * 3600 && z <= 14 * 3600) s_zona = z;
    uint8_t tam = 0;
    size_t lt = sizeof tam;
    if (hal_storage_get("tamano", &tam, &lt) && lt == sizeof tam) art_tamano_set(100 + tam); /* tamaño de las criaturas (100, 120, 130, 140 %) */
}

static void preparar_hora(void)
{
    uint32_t t = hal_rtc_now();
    if (t < HORA_MINIMA_VALIDA) {
        t = BUILD_UNIX_TIME; /* sin hora válida: provisoria, hasta que la pongas en pantalla */
        s_hora_dudosa = true;
        ESP_LOGW(TAG, "RTC sin hora; se pregunta en pantalla");
    }
    struct timeval tv = {.tv_sec = t};
    settimeofday(&tv, NULL);
}

static void cargar_o_crear_mascota(void)
{
    if (hal_storage_load(&s_pet)) {
        ESP_LOGI(TAG, "mascota cargada: %s (%s)", s_pet.nombre, pet_species_name(s_pet.especie));
    } else {
        uint8_t especie = (uint8_t)(esp_random() % pet_species_count());
        pet_init(&s_pet, "", especie, ahora(), esp_random()); /* el nombre se elige en pantalla */
        ESP_LOGI(TAG, "un huevo nuevo de %s", pet_species_name(especie));
    }
    pet_set_timezone(&s_pet, s_zona);
    if (pet_clock_behind(&s_pet, ahora())) {
        /* Sin pila para el reloj, la hora guardada se queda unos minutos atrás de la última partida (se guarda cada pocos minutos). Si el atraso es chico
         * (hasta 30 min), es eso: se sigue desde la última hora conocida y NO se pregunta (antes preguntaba en cada reinicio y después de cada respaldo). */
        const int64_t falta = (int64_t)s_pet.ultima_actualizacion - (int64_t)ahora();
        if (!s_hora_dudosa && falta <= 30 * 60) {
            const uint32_t base = (uint32_t)((int64_t)s_pet.ultima_actualizacion - s_pet.desfase_s);
            hal_rtc_set(base);
            ESP_LOGW(TAG, "el reloj iba %d s atrás de la partida (sin pila): se sigue desde la última hora conocida", (int)falta);
        } else {
            s_hora_dudosa = true; /* la hora quedó muy antes de la última vez: se perdió de verdad */
            ESP_LOGW(TAG, "la hora del reloj es anterior a la partida; se pregunta en pantalla");
        }
    }
    if (!s_hora_dudosa) pet_update(&s_pet, ahora());
}

static void cargar_celebraciones(void)
{
    static uint8_t buf[16 + PET_CELEBRACIONES * sizeof(celebracion_t)];
    size_t len = sizeof buf;
    if (hal_storage_get("fiestas", buf, &len) && pet_celebrations_unpack(buf, (int)len))
        ESP_LOGI(TAG, "%d celebraciones del editor", pet_celebration_count());
}

/* La fecha y hora que pusiste en pantalla (en hora local) */
static void poner_hora(int32_t local)
{
    uint32_t utc = (uint32_t)((int64_t)local - s_pet.zona_horaria);
    /* la hora del juego es el reloj + el desfase de este perfil: el reloj se pone para que el juego quede EXACTO en lo que se eligió
     * (antes, con una "otra fecha" activa, el juego quedaba desfasado y la pantalla volvía a pedir la fecha una y otra vez) */
    const uint32_t reloj = (uint32_t)((int64_t)utc - s_pet.desfase_s);
    hal_rtc_set(reloj);
    struct timeval tv = {.tv_sec = reloj};
    settimeofday(&tv, NULL);
    s_hora_dudosa = false;
    hal_rtc_trusted(true); /* v88: desde ahora la hora se guarda cada 2 min en la memoria (si no hay chip de reloj) */
    if (!s_pet.edad_dueno && pet_is_egg(&s_pet) && !s_pet.nombre[0]) { /* primera vez: el huevo empieza ahora */
        pet_init(&s_pet, "", s_pet.especie, utc, esp_random());
        pet_set_timezone(&s_pet, s_zona);
    }
    ESP_LOGI(TAG, "hora puesta a mano");
}

/* ---------- comandos del editor por USB ----------
 *   MASCOTA HOLA                 -> MASCOTA OK <versión de partida> <nombre>
 *   MASCOTA HORA <unix utc> <zona en s> -> MASCOTA HORA OK <1 si la ajustó, 0 si ya estaba bien>: la hora del computador
 *   MASCOTA RESPALDO             -> MASCOTA RESPALDO <versión> <base64 de la partida>
 *   MASCOTA RESTAURAR <base64>   -> MASCOTA ESPERANDO (se confirma en el reloj) y luego
 *                                   MASCOTA RESTAURADA o MASCOTA RECHAZADA
 *   MASCOTA FIESTAS              -> MASCOTA FIESTAS <base64 de la tabla>
 *   MASCOTA FIESTAS <base64>     -> MASCOTA OK (o MASCOTA ERROR)
 *   MASCOTA FIESTAS FABRICA      -> vuelve a las de fábrica
 *   MASCOTA REGISTRO             -> MASCOTA REGISTRO INICIO <bytes>, líneas "MASCOTA REGISTRO | <texto>" y MASCOTA REGISTRO FIN
 *   MASCOTA REGISTRO BORRAR      -> MASCOTA OK
 *   MASCOTA FOTOS                -> MASCOTA FOTOS <n> <id>:<bytes> ...
 *   MASCOTA FOTO <id>            -> MASCOTA FOTO <id> <total> <desde> <base64> (en trozos) y MASCOTA FOTO <id> FIN
 *   MASCOTA BORRAR FOTO <id>     -> MASCOTA OK
 *   MASCOTA ICONO <id> <base64>  -> MASCOTA OK (24x24 pixeles RGBA, 2304 bytes; o 12x12 del editor antiguo);
 *                                   ICONO <id> FABRICA lo devuelve
 *   MASCOTA SONIDO <n> INICIO <bytes> | DATOS <desde> <base64> | FIN <crc32 hex> | FABRICA
 *   MASCOTA AMBIENTE <n> ...     -> igual que SONIDO, para los bucles de fondo (PCM 16 kHz, mono, 16 bits)
 *   MASCOTA CRIATURA <especie> <etapa> <base64>  -> MASCOTA OK (el cuerpo dibujado: ver art_cuerpo_desde_bytes);
 *                                   CRIATURA <especie> <etapa> FABRICA lo devuelve
 *   MASCOTA ESPECIE <especie> NOMBRE <texto> | ESPECIE <especie> FABRICA -> MASCOTA OK
 *   MASCOTA CAMBIOS              -> MASCOTA CAMBIOS I <íconos> S <sonidos> A <ambientes> C <especie*10+etapa> N <especies> */

static EXT_RAM_BSS_ATTR char s_usb_txt[PET_RESPALDO_MAX + 64];

static void enviar_foto(int id);

/* ---------- lo que se cambia con el editor: íconos y sonidos ---------- */
static uint32_t crc32_bytes(const uint8_t *d, int n)
{
    uint32_t c = 0xFFFFFFFFu;
    for (int i = 0; i < n; i++) {
        c ^= d[i];
        for (int k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u)));
    }
    return ~c;
}

/* un ícono del editor: lado x lado pixeles R, G, B, A -> ARGB8888 */
static void aplicar_icono(int id, const uint8_t *rgba, int lado)
{
    static EXT_RAM_BSS_ATTR uint32_t px[ART_ICONO_LADO * ART_ICONO_LADO];
    for (int i = 0; i < lado * lado; i++) {
        const uint8_t *q = rgba + i * 4;
        px[i] = q[3] < 128 ? 0 : (0xFF000000u | (uint32_t)q[0] << 16 | (uint32_t)q[1] << 8 | q[2]);
    }
    if (hal_display_lock()) {
        art_icono_propio((art_icono_t)id, px, lado);
        lv_obj_invalidate(lv_screen_active());
        hal_display_unlock();
    }
}

/* un cuerpo de criatura o el nombre de una especie (con la pantalla tomada) */
static void aplicar_criatura(int e, int et, const art_cuerpo_t *c)
{
    if (hal_display_lock()) {
        art_criatura_propia(e, et, c);
        lv_obj_invalidate(lv_screen_active());
        hal_display_unlock();
    }
}

static void comando_criatura(const char *l)
{
    int e = -1, et = -1, k = 0;
    if (sscanf(l, "%d %d %n", &e, &et, &k) < 2 || e < 0 || e >= pet_species_count() || et < 1 || et > 3 || !k) {
        printf("MASCOTA ERROR criatura\n");
        return;
    }
    const char *q = l + k;
    char nombre[24];
    snprintf(nombre, sizeof nombre, "c%d%d.bin", e, et);
    if (!strcmp(q, "FABRICA")) {
        hal_res_delete(nombre);
        aplicar_criatura(e, et, NULL);
        printf("MASCOTA OK\n");
        return;
    }
    static EXT_RAM_BSS_ATTR uint8_t b[ART_CUERPO_BYTES + 8];
    static art_cuerpo_t c;
    int n = pet_base64_decode(q, b, sizeof b);
    if (!art_cuerpo_desde_bytes(b, n, &c) || !hal_res_save(nombre, b, n)) { printf("MASCOTA ERROR criatura\n"); return; }
    aplicar_criatura(e, et, &c);
    printf("MASCOTA OK\n");
}

static void comando_especie(const char *l)
{
    int e = atoi(l);
    const char *q = strchr(l, ' ');
    if (e < 0 || e >= pet_species_count() || !q) { printf("MASCOTA ERROR especie\n"); return; }
    q++;
    char nombre[24];
    snprintf(nombre, sizeof nombre, "n%d.txt", e);
    if (!strcmp(q, "FABRICA")) {
        hal_res_delete(nombre);
        pet_species_set_name((uint8_t)e, NULL);
    } else if (!strncmp(q, "NOMBRE ", 7) && q[7] && strlen(q + 7) < PET_NAME_LEN) {
        if (!hal_res_save(nombre, (const uint8_t *)q + 7, (int)strlen(q + 7))) { printf("MASCOTA ERROR especie\n"); return; }
        pet_species_set_name((uint8_t)e, q + 7);
    } else { printf("MASCOTA ERROR especie\n"); return; }
    if (hal_display_lock()) { lv_obj_invalidate(lv_screen_active()); hal_display_unlock(); }
    printf("MASCOTA OK\n");
}

/* ---------- mis mascotas: las que descansan, en la partición de recursos (m0.bin, m1.bin, m2.bin) ---------- */
static bool guardada_leer(int i, pet_t *out)
{
    if (i < 0 || i >= PET_GUARDADAS) return false;
    char n[16];
    snprintf(n, sizeof n, "m%d.bin", i);
    static EXT_RAM_BSS_ATTR pet_t tmp;
    int k = hal_res_size(n);
    if (k <= 0 || k > (int)sizeof tmp) return false;
    memset(&tmp, 0, sizeof tmp);
    if (hal_res_read(n, &tmp, (int)sizeof tmp) != k) return false;
    if (!pet_validate(&tmp) && !(k < (int)sizeof tmp && k == tmp.size && pet_upgrade(&tmp))) return false;
    if (out) *out = tmp;
    return true;
}
static bool guardada_escribir(int i, const pet_t *p)
{
    char n[16];
    snprintf(n, sizeof n, "m%d.bin", i);
    return i >= 0 && i < PET_GUARDADAS && hal_res_save(n, p, (int)sizeof *p);
}
static const ui_mascotas_t MASCOTAS = {guardada_leer};

/* al encender: lo que quedó guardado de otras veces */
static void cargar_recursos(void)
{
    static EXT_RAM_BSS_ATTR uint8_t rgba[ART_ICONO_LADO * ART_ICONO_LADO * 4];
    static EXT_RAM_BSS_ATTR uint32_t px[ART_ICONO_LADO * ART_ICONO_LADO];
    char n[24];
    int ic = 0, so = 0;
    for (int i = 0; i < ICO_COUNT; i++) {
        snprintf(n, sizeof n, "i%d.bin", i);
        int t = hal_res_size(n), lado = art_icono_lado_de_bytes(t);
        if (lado && hal_res_read(n, rgba, sizeof rgba) == t) {
            for (int k = 0; k < lado * lado; k++) {
                const uint8_t *q = rgba + k * 4;
                px[k] = q[3] < 128 ? 0 : (0xFF000000u | (uint32_t)q[0] << 16 | (uint32_t)q[1] << 8 | q[2]);
            }
            art_icono_propio((art_icono_t)i, px, lado);
            ic++;
        }
    }
    for (int i = 1; i < SND_COUNT; i++) {
        snprintf(n, sizeof n, "s%d.pcm", i);
        if (hal_res_size(n) > 0) { hal_sound_custom((sound_t)i, true); so++; }
    }
    for (int i = 1; i < AMB_COUNT; i++) {
        snprintf(n, sizeof n, "a%d.pcm", i);
        if (hal_res_size(n) > 0) { hal_ambient_custom((ambiente_t)i, true); so++; }
    }
    int cu = 0;
    static EXT_RAM_BSS_ATTR uint8_t b[ART_CUERPO_BYTES];
    static art_cuerpo_t c;
    for (int e = 0; e < pet_species_count(); e++) {
        for (int et = 1; et <= 3; et++) {
            snprintf(n, sizeof n, "c%d%d.bin", e, et);
            int t = hal_res_size(n);
            if ((t == ART_CUERPO_BYTES || t == ART_CUERPO_BYTES_V1) && hal_res_read(n, b, sizeof b) == t && art_cuerpo_desde_bytes(b, t, &c)) {
                art_criatura_propia(e, et, &c);
                cu++;
            }
        }
        char nom[PET_NAME_LEN];
        snprintf(n, sizeof n, "n%d.txt", e);
        int k = hal_res_size(n);
        if (k > 0 && k < (int)sizeof nom && hal_res_read(n, (uint8_t *)nom, k) == k) { nom[k] = 0; pet_species_set_name((uint8_t)e, nom); }
    }
    if (ic || so || cu) ESP_LOGI(TAG, "%d íconos, %d sonidos y %d criaturas del editor", ic, so, cu);
}

/* un sonido llega en trozos: INICIO <bytes>, DATOS <desde> <base64>..., FIN <crc32> */
static uint8_t *s_rx;
static int s_rx_n, s_rx_cap, s_rx_id = -1;
static bool s_rx_amb;

static void comando_sonido(const char *l, bool ambiente)
{
    int id = atoi(l);
    const char *q = strchr(l, ' ');
    int max = ambiente ? AMB_COUNT : SND_COUNT;
    if (id < 1 || id >= max || !q) { printf("MASCOTA ERROR sonido\n"); return; }
    q++;
    char nombre[24];
    snprintf(nombre, sizeof nombre, "%c%d.pcm", ambiente ? 'a' : 's', id);
    if (!strcmp(q, "FABRICA")) {
        hal_res_delete(nombre);
        if (ambiente) hal_ambient_custom((ambiente_t)id, false);
        else hal_sound_custom((sound_t)id, false);
        printf("MASCOTA OK\n");
    } else if (!strncmp(q, "INICIO ", 7)) {
        int n = atoi(q + 7), lim = (int)((ambiente ? 8.6f : 8.5f) * 16000) * 2;
        if (n <= 0 || n > lim || (n & 1)) { printf("MASCOTA ERROR largo\n"); return; }
        free(s_rx);
        s_rx = heap_caps_malloc((size_t)n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!s_rx) { printf("MASCOTA ERROR memoria\n"); return; }
        s_rx_cap = n; s_rx_n = 0; s_rx_id = id; s_rx_amb = ambiente;
        printf("MASCOTA OK\n");
    } else if (!strncmp(q, "DATOS ", 6)) {
        int desde = atoi(q + 6);
        const char *b = strchr(q + 6, ' ');
        if (!s_rx || id != s_rx_id || ambiente != s_rx_amb || !b || desde != s_rx_n) { printf("MASCOTA ERROR orden\n"); return; }
        int k = pet_base64_decode(b + 1, s_rx + s_rx_n, s_rx_cap - s_rx_n);
        if (k <= 0) { printf("MASCOTA ERROR datos\n"); return; }
        s_rx_n += k;
        printf("MASCOTA OK %d\n", s_rx_n);
    } else if (!strncmp(q, "FIN ", 4)) {
        uint32_t crc = (uint32_t)strtoul(q + 4, NULL, 16);
        bool ok = s_rx && id == s_rx_id && ambiente == s_rx_amb && s_rx_n == s_rx_cap && crc32_bytes(s_rx, s_rx_n) == crc &&
                  hal_res_save(nombre, s_rx, s_rx_n);
        if (ok) {
            if (ambiente) hal_ambient_custom((ambiente_t)id, true);
            else { hal_sound_custom((sound_t)id, true); hal_sound_play((sound_t)id); } /* para oírlo ya */
        }
        free(s_rx);
        s_rx = NULL; s_rx_id = -1;
        printf(ok ? "MASCOTA OK\n" : "MASCOTA ERROR no llego completo\n");
    } else printf("MASCOTA ERROR sonido\n");
}

static void comando_icono(const char *l)
{
    int id = atoi(l);
    const char *q = strchr(l, ' ');
    if (id < 0 || id >= ICO_COUNT || !q) { printf("MASCOTA ERROR icono\n"); return; }
    q++;
    char nombre[24];
    snprintf(nombre, sizeof nombre, "i%d.bin", id);
    if (!strcmp(q, "FABRICA")) {
        hal_res_delete(nombre);
        if (hal_display_lock()) {
            art_icono_propio((art_icono_t)id, NULL, 0);
            lv_obj_invalidate(lv_screen_active());
            hal_display_unlock();
        }
        printf("MASCOTA OK\n");
        return;
    }
    static EXT_RAM_BSS_ATTR uint8_t rgba[ART_ICONO_LADO * ART_ICONO_LADO * 4 + 8];
    int n = pet_base64_decode(q, rgba, sizeof rgba), lado = art_icono_lado_de_bytes(n);
    if (!lado || !hal_res_save(nombre, rgba, n)) { printf("MASCOTA ERROR icono\n"); return; }
    aplicar_icono(id, rgba, lado);
    printf("MASCOTA OK\n");
}

static void listar_cambios(void)
{
    char n[24];
    printf("MASCOTA CAMBIOS I");
    for (int i = 0; i < ICO_COUNT; i++) { snprintf(n, sizeof n, "i%d.bin", i); if (hal_res_size(n) > 0) printf(" %d", i); }
    printf(" S");
    for (int i = 1; i < SND_COUNT; i++) { snprintf(n, sizeof n, "s%d.pcm", i); if (hal_res_size(n) > 0) printf(" %d", i); }
    printf(" A");
    for (int i = 1; i < AMB_COUNT; i++) { snprintf(n, sizeof n, "a%d.pcm", i); if (hal_res_size(n) > 0) printf(" %d", i); }
    printf(" C");
    for (int e = 0; e < pet_species_count(); e++)
        for (int et = 1; et <= 3; et++) { snprintf(n, sizeof n, "c%d%d.bin", e, et); if (hal_res_size(n) > 0) printf(" %d", e * 10 + et); }
    printf(" N");
    for (int e = 0; e < pet_species_count(); e++) { snprintf(n, sizeof n, "n%d.txt", e); if (hal_res_size(n) > 0) printf(" %d", e); }
    printf("\n");
}

/* --- "desbloquear todo" para las pruebas, con respaldo de la partida en la memoria de recursos (sobrevive a un reinicio) */
#define RESP_PRUEBA "respaldo_prueba.bin"

static bool probar_respaldar(void)
{
    if (hal_res_size(RESP_PRUEBA) > 0) return true; /* ya hay uno de antes: ese es el original, no se pisa */
    pet_seal(&s_pet);
    return hal_res_save(RESP_PRUEBA, &s_pet, (int)sizeof s_pet);
}

static bool probar_restaurar(void)
{
    static EXT_RAM_BSS_ATTR pet_t vieja;
    if (hal_res_size(RESP_PRUEBA) != (int)sizeof vieja || hal_res_read(RESP_PRUEBA, &vieja, (int)sizeof vieja) != (int)sizeof vieja || !pet_validate(&vieja)) return false;
    s_pet = vieja;
    pet_seal(&s_pet);
    hal_storage_save(&s_pet);
    hal_res_delete(RESP_PRUEBA);
    return true;
}

static void probar_desbloquear_todo(void)
{
    pet_t *p = &s_pet;
    uint32_t t = ahora();
    p->escenas_abiertas = p->escenas_vistas = (uint8_t)((1u << ESC_COUNT) - 1);
    p->ropa_tiene = (uint16_t)((1u << ROPA_COUNT) - 1);
    p->juguetes_tiene = (uint8_t)((1u << JUG_COUNT) - 1);
    for (int i = 0; i < PET_OBJETOS; i++) {
        p->bolsa[i] = 3;
        p->obj_visto[i >> 3] |= (uint8_t)(1u << (i & 7));
        p->obj_sabe[i >> 3] |= (uint8_t)(1u << (i & 7));
        p->obj_oro[i >> 3] |= (uint8_t)(1u << (i & 7));
    }
    for (int i = 0; i < ITEM_COUNT; i++) p->inventario[i] = 9;
    for (int i = 0; i < CULTIVO_COUNT; i++) p->semillas[i] = 9;
    p->semillas_vistas = (uint8_t)((1u << CULTIVO_COUNT) - 1);
    for (int i = 0; i < PET_COMIDAS_EST; i++) p->comida_est[i] = 9;
    p->comida_vista = 0xFFFF;
    p->decoraciones = (1u << DECOR_COUNT) - 1;
    for (int c = 0; c < COL_COUNT; c++) {
        p->coleccion[c] = (uint8_t)((1u << pet_collection_size((coleccion_t)c)) - 1u);
        p->colecciones_completas |= (uint8_t)(1u << c);
    }
    p->criaturas_vistas = (uint8_t)((1u << CRI_COUNT) - 1);
    for (int i = 0; i < CRI_COUNT; i++) p->historias[i] = 0x1F;
    p->especiales = (uint8_t)((1u << ESP_COUNT) - 1);
    p->leyendas = (uint8_t)((1u << PET_CONSTELACIONES) - 1);
    p->trucos_sabidos = 0xFF;
    p->fichas_n = PET_FICHAS < PET_FICHAS_MAX ? PET_FICHAS : PET_FICHAS_MAX;
    for (int i = 0; i < p->fichas_n; i++) p->fichas[i] = (uint8_t)i;
    /* cartas: de los amigos, de la familia y de las que viajan (con cada una el texto de verdad) */
    p->vecinos_vistos = pet_friends_max(p) >= 32 ? 0xFFFFFFFFu : (1u << pet_friends_max(p)) - 1u;
    if (!p->padre[0]) snprintf(p->padre, sizeof p->padre, "Mamá");
    if (!p->viajeros_n) {
        snprintf(p->viajeros[0].nombre, sizeof p->viajeros[0].nombre, "Pluma");
        p->viajeros[0].especie = 0; p->viajeros[0].genero = 0; p->viajeros[0].vehiculo = 0; p->viajeros[0].cartas = 0;
        p->viajeros_n = 1;
    }
    /* (v96) también lo de la historia nueva: los lugares secretos (la casita, el mirador y el claro), todo lo que cuentan Nimba,
     * Prisma, Tilde y la Abuela Brizna, las páginas del diario de Peumo (menos la última, que es del final), lo que cuentan los
     * animales, materiales para la casita y lo escondido de hoy en cada lugar. Los finales y el rito NO: esos se prueban aparte. */
    p->lugares |= 7u;
    p->historias_nimba = 0x1F;
    p->prisma_caps = 0xFF;
    p->tilde_chismes = (uint16_t)((1u << PET_TILDE_CHISMES) - 1u);
    p->brizna = (uint8_t)((1u << PET_BRIZNA_RECUERDOS) - 1u);
    p->tilo_paginas = 0xFF;
    for (int k = 0; k < PET_ANIMAL_FRASES; k++) p->animal_frases[k >> 5] |= 1u << (k & 31);
    if (p->ramitas < 9) p->ramitas = 9;
    if (p->musgo < 9) p->musgo = 9;
    memset(p->zona_hallazgo, 0, sizeof p->zona_hallazgo);
    p->dormida = 0; /* despierta (si no, no se abren los secretos ni otras escenas) */
    p->luz_apagada = 0;
    p->cartas_n = 0;
    for (int k = 0; k < PET_CARTAS; k++) pet_letter_now_from(p, k % 3 == 0 ? CARTA_AMIGO : (k % 3 == 1 ? CARTA_FAMILIA : CARTA_VIAJERO), t);
    p->carta_pend = 0;
}

/* --- desbloqueo por tipo (el probador): LISTA <tipo> da los nombres; ABRIR <tipo> <n | -1 = todos>; ESTACION <n | -1>; CLIMA <n> --- */
static const char *const PRUEBA_SECRETOS[ESP_COUNT] = {"anillo", "reflejo", "cometa", "cristales"};
static const char *const PRUEBA_CLIMAS[CLIMA_COUNT] = {"despejado", "nublado", "lluvia", "viento", "nieve", "tormenta", "frío"};

static int prueba_cuenta(const char *t)
{
    if (!strcmp(t, "zonas")) return ESC_COUNT;
    if (!strcmp(t, "criaturas")) return CRI_COUNT;
    if (!strcmp(t, "historias")) return CRI_COUNT * 5;
    if (!strcmp(t, "amigos")) return pet_friends_max(&s_pet);
    if (!strcmp(t, "animales")) return PET_FICHAS < PET_FICHAS_MAX ? PET_FICHAS : PET_FICHAS_MAX;
    if (!strcmp(t, "secretos")) return ESP_COUNT;
    if (!strcmp(t, "objetos")) return PET_OBJETOS;
    if (!strcmp(t, "ropa")) return ROPA_COUNT;
    if (!strcmp(t, "juguetes")) return JUG_COUNT;
    if (!strcmp(t, "decoraciones")) return DECOR_COUNT;
    if (!strcmp(t, "comidas")) return PET_COMIDAS_EST;
    if (!strcmp(t, "colecciones")) return COL_COUNT;
    if (!strcmp(t, "leyendas")) return PET_CONSTELACIONES;
    if (!strcmp(t, "estacion")) return ESTACION_COUNT;
    if (!strcmp(t, "clima")) return CLIMA_COUNT;
    if (!strcmp(t, "sonidos")) return SND_COUNT;
    return -1;
}

static const char *prueba_nombre(const char *t, int i)
{
    if (!strcmp(t, "zonas")) return pet_scene_name((escena_t)i);
    if (!strcmp(t, "criaturas")) return pet_creature_name((magica_t)i);
    if (!strcmp(t, "historias")) { static char h[48]; snprintf(h, sizeof h, "%s: %s", pet_creature_name((magica_t)(i / 5)), pet_story_title((magica_t)(i / 5), i % 5)); return h; }
    if (!strcmp(t, "amigos")) return pet_friend_name(&s_pet, i);
    if (!strcmp(t, "animales")) return pet_card_name(i);
    if (!strcmp(t, "secretos")) return PRUEBA_SECRETOS[i];
    if (!strcmp(t, "objetos")) { const objeto_info_t *o = objeto_info(i); return o ? o->nombre : "?"; }
    if (!strcmp(t, "ropa")) return pet_clothes_name((ropa_t)i);
    if (!strcmp(t, "juguetes")) return pet_toy_name((juguete_t)i);
    if (!strcmp(t, "sonidos")) return pet_sound_name((sound_t)i);
    if (!strcmp(t, "decoraciones")) return pet_decor_name((decor_t)i);
    if (!strcmp(t, "comidas")) return pet_seasonal_food_info(i)->nombre;
    if (!strcmp(t, "colecciones")) return pet_collection_name((coleccion_t)i);
    if (!strcmp(t, "leyendas")) return pet_constellation_name(i);
    if (!strcmp(t, "estacion")) return pet_season_name((estacion_t)i);
    if (!strcmp(t, "clima")) return PRUEBA_CLIMAS[i];
    return "?";
}

static void prueba_abrir_uno(const char *t, int i)
{
    pet_t *p = &s_pet;
    const uint32_t bit = 1u << (i & 31);
    if (!strcmp(t, "zonas")) p->escenas_abiertas |= (uint8_t)bit;
    else if (!strcmp(t, "criaturas")) p->criaturas_vistas |= (uint8_t)bit; /* solo la conoce: sus historias se abren aparte */
    else if (!strcmp(t, "historias")) { p->criaturas_vistas |= (uint8_t)(1u << (i / 5)); p->historias[i / 5] |= (uint8_t)(1u << (i % 5)); }
    else if (!strcmp(t, "amigos")) p->vecinos_vistos |= bit;
    else if (!strcmp(t, "animales")) {
        for (int k = 0; k < p->fichas_n; k++) if (p->fichas[k] == i) return;
        if (p->fichas_n < PET_FICHAS_MAX) p->fichas[p->fichas_n++] = (uint8_t)i;
    }
    else if (!strcmp(t, "secretos")) p->especiales |= (uint8_t)bit;
    else if (!strcmp(t, "objetos")) {
        p->bolsa[i] = 3;
        p->obj_visto[i >> 3] |= (uint8_t)(1u << (i & 7));
        p->obj_sabe[i >> 3] |= (uint8_t)(1u << (i & 7));
    }
    else if (!strcmp(t, "ropa")) p->ropa_tiene |= (uint16_t)bit;
    else if (!strcmp(t, "juguetes")) p->juguetes_tiene |= (uint8_t)bit;
    else if (!strcmp(t, "decoraciones")) p->decoraciones |= bit;
    else if (!strcmp(t, "comidas")) { p->comida_est[i] = 9; p->comida_vista |= (uint16_t)bit; }
    else if (!strcmp(t, "colecciones")) { p->coleccion[i] = (uint8_t)((1u << pet_collection_size((coleccion_t)i)) - 1u); p->colecciones_completas |= (uint8_t)bit; }
    else if (!strcmp(t, "leyendas")) p->leyendas |= (uint8_t)bit;
}

/* lo contrario de prueba_abrir_uno: vuelve a cerrar un contenido (para revertir un desbloqueo sin tocar el resto de la partida) */
static void prueba_cerrar_uno(const char *t, int i)
{
    pet_t *p = &s_pet;
    const uint32_t bit = 1u << (i & 31);
    if (!strcmp(t, "zonas")) { if (i == 0) return; p->escenas_abiertas &= (uint8_t)~bit; if (p->escena == i) p->escena = 0; } /* el prado nunca se cierra */
    else if (!strcmp(t, "criaturas")) { p->criaturas_vistas &= (uint8_t)~bit; p->historias[i] = 0; }
    else if (!strcmp(t, "historias")) p->historias[i / 5] &= (uint8_t)~(1u << (i % 5));
    else if (!strcmp(t, "amigos")) p->vecinos_vistos &= ~bit;
    else if (!strcmp(t, "animales")) {
        for (int k = 0; k < p->fichas_n; k++) if (p->fichas[k] == i) { for (int m = k; m + 1 < p->fichas_n; m++) p->fichas[m] = p->fichas[m + 1]; p->fichas_n--; break; }
    }
    else if (!strcmp(t, "secretos")) p->especiales &= (uint8_t)~bit;
    else if (!strcmp(t, "objetos")) {
        p->bolsa[i] = 0;
        p->obj_visto[i >> 3] &= (uint8_t)~(1u << (i & 7));
        p->obj_sabe[i >> 3] &= (uint8_t)~(1u << (i & 7));
    }
    else if (!strcmp(t, "ropa")) p->ropa_tiene &= (uint16_t)~bit;
    else if (!strcmp(t, "juguetes")) p->juguetes_tiene &= (uint8_t)~bit;
    else if (!strcmp(t, "decoraciones")) p->decoraciones &= ~bit;
    else if (!strcmp(t, "comidas")) { p->comida_est[i] = 0; p->comida_vista &= (uint16_t)~bit; }
    else if (!strcmp(t, "colecciones")) { p->coleccion[i] = 0; p->colecciones_completas &= (uint8_t)~bit; }
    else if (!strcmp(t, "leyendas")) p->leyendas &= (uint8_t)~bit;
}

/* al abrir UNO desde el probador, se aplica al estado actual para verlo ya: va a la zona, se pone la ropa, deja el juguete o la decoración en el prado,
 * y los secretos y las historias se muestran. Al volver a bloquearlo se quita. */
static void prueba_aplicar_uno(const char *t, int i, bool abrir)
{
    pet_t *p = &s_pet;
    const uint32_t bit = 1u << (i & 31);
    if (!strcmp(t, "zonas")) {
        if (abrir) { p->escena = (uint8_t)i; p->escena_desde = ahora(); p->lugar = LUGAR_CLARO; }
    } else if (!strcmp(t, "ropa")) {
        const int r = (int)pet_clothes_slot((ropa_t)i);
        if (r == RANURA_MANO) { if (abrir) p->ropa_mano = (uint8_t)(i + 1); else if (p->ropa_mano == (uint8_t)(i + 1)) p->ropa_mano = 0; }
        else if (abrir) p->ropa_puesta[r] = (uint8_t)(i + 1);
        else if (p->ropa_puesta[r] == (uint8_t)(i + 1)) p->ropa_puesta[r] = 0;
    } else if (!strcmp(t, "juguetes")) {
        if (abrir) { p->juguetes_puestos |= (uint8_t)bit; p->escena = ESC_PRADO; p->lugar = LUGAR_CLARO; } else p->juguetes_puestos &= (uint8_t)~bit;
    } else if (!strcmp(t, "decoraciones")) {
        if (abrir) { p->decor_ocultas &= (uint8_t)~bit; p->escena = ESC_PRADO; p->lugar = LUGAR_CLARO; } else p->decor_ocultas &= (uint8_t)~bit;
    } else if (!strcmp(t, "secretos")) {
        if (abrir && hal_display_lock()) { ui_close_panel(); ui_cine(i); hal_display_unlock(); }
    } else if (!strcmp(t, "historias")) {
        if (abrir && hal_display_lock()) { ui_close_panel(); ui_demo_historia(i / 5, i % 5); hal_display_unlock(); }
    }
}

/* --- pruebas desde el computador: "MASCOTA PROBAR ..." (ver probar.py). No cambian la partida salvo lo que dice cada comando. */
static void probar_info(const char *que)
{
    int n, prom, max;
    hal_vel_info(&n, &prom, &max);
    int ve, vf, va;
    hal_sound_gains_get(&ve, &vf, &va);
    printf("MASCOTA PROBAR %s OK tam=%d sonido=%d vol=%d/%d/%d panel=%d juego=%d zona=%d zonas=%u final=%d/%d cuadros=%d ms=%d max=%d ram=%u mayor=%u psram=%u resp=%d\n", que, art_tamano_get(),
           hal_sound_ready() ? 1 : 0, ve, vf, va, ui_panel_actual(), ui_juego_actual(), (int)s_pet.escena, (unsigned)s_pet.escenas_abiertas, (int)s_pet.final_estado, (int)s_pet.final_pend,
           n, prom, max, (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL), (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),
           (unsigned)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024), hal_res_size(RESP_PRUEBA) > 0 ? 1 : 0);
}

static int s_prueba_amb = -1; /* (pruebas) ambiente forzado desde el computador; -1 = el normal */

/* tiempo en microsegundos de una lectura, repetida n veces (promedio) */
#define MIDE_US(n, expr) ({ int64_t t0_ = esp_timer_get_time(); for (int i_ = 0; i_ < (n); i_++) { expr; } (int)((esp_timer_get_time() - t0_) / (n)); })

static void probar_sensores(void)
{
    bool sac = false, mov = false;
    float tx = 0, ty = 0, tx2 = 0, ty2 = 0;
    int imu_us = MIDE_US(20, hal_imu_poll(&sac, &mov));
    hal_imu_tilt(&tx, &ty);
    vTaskDelay(pdMS_TO_TICKS(200));
    bool s2, m2;
    hal_imu_poll(&s2, &m2);
    hal_imu_tilt(&tx2, &ty2);
    int bat = 0, carga = 0;
    int bat_us = MIDE_US(5, bat = hal_battery_percent(); carga = hal_battery_charging() ? 1 : 0);
    uint32_t rtc = 0, pasos = 0;
    int rtc_us = MIDE_US(5, rtc = hal_rtc_now());
    int pasos_us = MIDE_US(5, pasos = hal_steps_read());
    /* flash: guardar un archivo de 4 KB y la partida */
    static EXT_RAM_BSS_ATTR uint8_t blk[4096];
    memset(blk, 0xA5, sizeof blk);
    int64_t t0 = esp_timer_get_time();
    bool res_ok = hal_res_save("prueba_rend.bin", blk, sizeof blk);
    int res_ms = (int)((esp_timer_get_time() - t0) / 1000);
    int leer_ok = hal_res_read("prueba_rend.bin", blk, sizeof blk) == (int)sizeof blk;
    hal_res_delete("prueba_rend.bin");
    t0 = esp_timer_get_time();
    pet_seal(&s_pet);
    bool gu_ok = hal_storage_save(&s_pet) == ESP_OK;
    int guardar_ms = (int)((esp_timer_get_time() - t0) / 1000);
    uint16_t ids[8];
    int fotos = hal_photo_list(ids, 8);
    uint8_t baliza[PET_BALIZA_BYTES];
    int radio_us = MIDE_US(5, (void)hal_radio_poll(baliza));
    printf("MASCOTA PROBAR SENSORES OK imu_us=%d tilt_x=%d tilt_y=%d tilt2_x=%d tilt2_y=%d bat=%d carga=%d bat_us=%d rtc=%u rtc_us=%d pasos=%u pasos_us=%d res_ok=%d res_ms=%d leer_ok=%d guardar_ok=%d guardar_ms=%d fotos=%d radio_us=%d\n",
           imu_us, (int)(tx * 1000), (int)(ty * 1000), (int)(tx2 * 1000), (int)(ty2 * 1000), bat, carga, bat_us, (unsigned)rtc, rtc_us, (unsigned)pasos, pasos_us,
           res_ok ? 1 : 0, res_ms, leer_ok, gu_ok ? 1 : 0, guardar_ms, fotos, radio_us);
}

/* escucha el micrófono ms milisegundos y dice cuánto sonido hubo (0..100) */
static void probar_mic(int ms)
{
    if (ms < 100) ms = 100;
    if (ms > 4000) ms = 4000;
    hal_mic_enable(true);
    vTaskDelay(pdMS_TO_TICKS(100));
    int n = 0, mn = 1000, mx = -1, suma = 0, falla = 0;
    for (int t = 0; t < ms; t += 30) {
        int v = hal_mic_level();
        if (v < 0) { falla++; } else { n++; suma += v; if (v < mn) mn = v; if (v > mx) mx = v; }
        vTaskDelay(pdMS_TO_TICKS(30));
    }
    hal_mic_enable(false);
    printf("MASCOTA PROBAR MIC OK muestras=%d prom=%d min=%d max=%d fallas=%d\n", n, n ? suma / n : 0, n ? mn : 0, n ? mx : 0, falla);
}

static void despertar_pantalla(void);
/* las pruebas guiadas que esperan algo (toque, sacudida, botón, giro) NO bloquean el bucle principal: él sigue leyendo el acelerómetro, el botón y el giro mientras tanto */
static struct {
    int tipo; uint32_t fin; int sal, cx, cy, cr, minimo, que, s0, m0, u0, d0; unsigned vistas; float mnx, mxx, mny, mxy;
} s_esp;

static void probar_espera_paso(void)
{
    if (!s_esp.tipo) return;
    const uint32_t ahora_ms = (uint32_t)(esp_timer_get_time() / 1000);
    const bool vencio = (int32_t)(ahora_ms - s_esp.fin) >= 0;
    if (s_esp.tipo == 1) {
        int xs[16], ys[16];
        const int m = ui_touch_get(xs, ys, 16);
        bool ok = false;
        for (int i = 0; i < m && i < 16; i++)
            if (s_esp.cr <= 0 || (abs(xs[i] - s_esp.cx) <= s_esp.cr && abs(ys[i] - s_esp.cy) <= s_esp.cr)) ok = true;
        if (!(vencio || (s_esp.sal && ok))) return;
        printf("MASCOTA PROBAR TOQUES OK n=%d pts=", m);
        for (int i = 0; i < m && i < 16; i++) printf("%s%d,%d", i ? ";" : "", xs[i], ys[i]);
        printf("\n");
    } else if (s_esp.tipo == 2) {
        float x = 0, y = 0;
        hal_imu_tilt(&x, &y);
        if (x < s_esp.mnx) s_esp.mnx = x;
        if (x > s_esp.mxx) s_esp.mxx = x;
        if (y < s_esp.mny) s_esp.mny = y;
        if (y > s_esp.mxy) s_esp.mxy = y;
        if (!(vencio || (s_esp.minimo && s_n_sac - s_esp.s0 >= s_esp.minimo))) return;
        printf("MASCOTA PROBAR SACUDIDAS OK sacudidas=%d movimientos=%d rango_x=%d rango_y=%d\n", s_n_sac - s_esp.s0, s_n_mov - s_esp.m0,
               (int)((s_esp.mxx - s_esp.mnx) * 1000), (int)((s_esp.mxy - s_esp.mny) * 1000));
    } else if (s_esp.tipo == 3) {
        if (!(vencio || (s_esp.que == 1 && s_n_pwr1 > s_esp.u0) || (s_esp.que == 2 && s_n_pwr2 > s_esp.d0))) return;
        printf("MASCOTA PROBAR BOTONES OK una_vez=%d dos_veces=%d\n", s_n_pwr1 - s_esp.u0, s_n_pwr2 - s_esp.d0);
    } else if (s_esp.tipo == 4) {
        s_esp.vistas |= 1u << (s_giro_pos & 3);
        if (!(vencio || (s_esp.sal && s_esp.vistas == 15u))) return;
        printf("MASCOTA PROBAR GIROREAL OK posiciones=%u ahora=%d\n", s_esp.vistas, (int)s_giro_pos);
    }
    s_esp.tipo = 0;
}

static void comando_probar(const char *l)
{
    int a = 0, b = 0, c = 0, d = 0;
    char tipo[16];
    if (!strcmp(l, "INFO")) {
        probar_info("INFO");
    } else if (!strcmp(l, "PANELES")) {
        printf("MASCOTA PROBAR PANELES %d", ui_test_count());
        for (int k = 0; k < ui_test_count(); k++) printf(" %s", ui_test_panel_name(k));
        printf("\n");
    } else if (sscanf(l, "PANEL %d", &a) == 1) {
        int ok = 0;
        if (hal_display_lock()) { ok = ui_test_panel(a); hal_display_unlock(); }
        if (ok) probar_info("PANEL"); else printf("MASCOTA PROBAR PANEL ERROR %d\n", a);
    } else if (!strcmp(l, "CERRAR")) {
        if (hal_display_lock()) { ui_close_panel(); hal_display_unlock(); }
        probar_info("CERRAR");
    } else if (sscanf(l, "JUEGO %d", &a) == 1) { /* 0 pelota, 1 escondidas, 2 frutas, 3 hongos */
        if (a < 0 || a >= JUEGO_COUNT) { printf("MASCOTA PROBAR JUEGO ERROR %d\n", a); return; }
        if (hal_display_lock()) { ui_close_panel(); ui_demo_juego(a); hal_display_unlock(); }
        probar_info("JUEGO");
    } else if (!strcmp(l, "ZONAS")) { /* abre todas las zonas */
        s_pet.escenas_abiertas = (1u << ESC_COUNT) - 1;
        probar_info("ZONAS");
    } else if (sscanf(l, "ZONA %d", &a) == 1) { /* abre una zona y lleva a la mascota */
        if (a < 0 || a >= ESC_COUNT) { printf("MASCOTA PROBAR ZONA ERROR %d\n", a); return; }
        s_pet.escenas_abiertas |= 1u << a;
        s_pet.escena = (uint8_t)a;
        probar_info("ZONA");
    } else if (sscanf(l, "SECRETO %d", &a) == 1) { /* la escena de un secreto: 0 anillo, 1 reflejo, 2 cometa, 3 cristales */
        if (a < 0 || a >= ESP_COUNT) { printf("MASCOTA PROBAR SECRETO ERROR %d\n", a); return; }
        if (hal_display_lock()) { ui_close_panel(); ui_cine(a); hal_display_unlock(); }
        probar_info("SECRETO");
    } else if (!strcmp(l, "FINAL REGULAR") || !strcmp(l, "FINAL SECRETO")) { /* deja el final pendiente: la pantalla lo muestra sola */
        s_pet.final_pend = l[6] == 'R' ? PET_FINAL_REGULAR : PET_FINAL_SECRETO;
        probar_info("FINAL");
    } else if (!strcmp(l, "FINAL BORRAR")) {
        s_pet.final_estado = 0;
        s_pet.final_pend = 0;
        probar_info("FINAL");
    } else if (sscanf(l, "DIAS %d", &a) == 1) { /* (v37) días jugados de la historia (y libera el tope de hoy) */
        s_pet.dias_jugados = (uint16_t)(a < 1 ? 1 : a);
        s_pet.hist_hoy = 0; s_pet.prisma_dia = 0; s_pet.animal_dia = 0;
        probar_info("DIAS");
    } else if (!strcmp(l, "TILDE")) { /* (v37) Tilde llega de visita a la zona donde está */
        uint32_t t = ahora();
        if (s_pet.escena == ESC_CUEVA) s_pet.escena = ESC_PRADO;
        s_pet.dormida = 0; s_pet.tilde_prox = 1; s_pet.ultima_interaccion = t;
        for (int i = 0; i < 400 && !pet_tilde_here(&s_pet, t); i++) pet_update(&s_pet, t);
        probar_info("TILDE");
    } else if (!strcmp(l, "BRIZNA")) { /* (v37) a la isla: despierta la Abuela Brizna con su próximo recuerdo */
        uint32_t t = ahora();
        s_pet.escenas_abiertas |= 1u << ESC_ISLA; s_pet.escena = ESC_ISLA; s_pet.dormida = 0;
        s_pet.hist_hoy = 0; s_pet.brizna_ultimo = 0; s_pet.ultima_interaccion = t;
        if (s_pet.dias_jugados < 26) s_pet.dias_jugados = (uint16_t)(s_pet.dias_jugados < 8 ? 8 : s_pet.dias_jugados + 7);
        pet_update(&s_pet, t);
        probar_info("BRIZNA");
    } else if (sscanf(l, "PAGINA %d", &a) == 1) { /* (v37) una página del diario de Tilo (0..7) aparece en el Libro */
        if (a < 0 || a >= PET_TILO_PAGINAS) { printf("MASCOTA PROBAR PAGINA ERROR %d\n", a); return; }
        s_pet.tilo_paginas |= (uint8_t)(1u << a); s_pet.tilo_vistas &= (uint8_t)~(1u << a);
        probar_info("PAGINA");
    } else if (sscanf(l, "LUGAR %d", &a) == 1) { /* (v37) abre un lugar secreto: 0 la casita de Tilo, 1 el mirador */
        if (a < 0 || a >= PET_LUGARES) { printf("MASCOTA PROBAR LUGAR ERROR %d\n", a); return; }
        s_pet.lugares |= (uint8_t)(1u << a);
        if (hal_display_lock()) { ui_close_panel(); ui_demo_lugar(a); hal_display_unlock(); }
        probar_info("LUGAR");
    } else if (sscanf(l, "HISTORIA %d %d", &a, &b) == 2) {
        if (hal_display_lock()) { ui_close_panel(); ui_demo_historia(a, b); hal_display_unlock(); }
        probar_info("HISTORIA");
    } else if (sscanf(l, "HUEVO %d", &a) == 1) { /* 0 pone un huevo y ofrece el viaje, 1 pone el huevo en el nido (para tocarlo), 2 quita el huevo */
        if (a < 0 || a > 2) { printf("MASCOTA PROBAR HUEVO ERROR %d\n", a); return; }
        if (a == 2) s_pet.huevo_familia = 0;
        else {
            s_pet.huevo_familia = 1;
            s_pet.dormida = 0;
            s_pet.escena = ESC_PRADO;
            s_pet.decoraciones |= (1u << DECOR_CASITA); /* el nido está en la casita: se la pone para poder entrar */
            s_pet.decor_ocultas &= (uint8_t)~(1u << DECOR_CASITA);
        }
        if (a == 0 && hal_display_lock()) { ui_demo_huevo(0); hal_display_unlock(); }
        probar_info("HUEVO");
    } else if (!strcmp(l, "BALLENA")) { /* hace pasar la ballena de nubes (5 min) y libera el paseo de hoy: tócala para subir */
        uint32_t t = ahora();
        s_pet.dormida = 0;
        if (s_pet.escena == ESC_CUEVA) s_pet.escena = ESC_PRADO;
        s_pet.dia_ballena = 0;
        s_pet.raro = RARO_GLOBO;
        s_pet.raro_hasta = t + 300;
        probar_info("BALLENA");
    } else if (!strcmp(l, "ARCOIRIS")) { /* (v96) sale el arcoíris ahora (5 min), donde ella esté (no en la cueva) */
        uint32_t t = ahora();
        s_pet.dormida = 0;
        if (s_pet.escena == ESC_CUEVA) s_pet.escena = ESC_PRADO;
        s_pet.raro = RARO_ARCOIRIS;
        s_pet.raro_hasta = t + 300;
        probar_info("ARCOIRIS");
    } else if (!strcmp(l, "LUGARES")) { /* (v96) abre el claro de las luciérnagas, la casita y el mirador (y todas las zonas) */
        s_pet.lugares |= 7u;
        s_pet.escenas_abiertas = (1u << ESC_COUNT) - 1;
        probar_info("LUGARES");
    } else if (sscanf(l, "ENTRAR %d", &a) == 1) { /* (v96) entra a una zona nueva: 1 el claro del rey, 2 la casita (adentro de la casa del rey), 3 el mirador */
        if (a < 1 || a > 3) { printf("MASCOTA PROBAR ENTRAR ERROR %d\n", a); return; }
        s_pet.lugares |= 7u;
        s_pet.escenas_abiertas |= (1u << ESC_BOSQUE) | (1u << ESC_COLINA);
        s_pet.dormida = 0;
        s_pet.escena = (uint8_t)(a == 3 ? ESC_MIRADOR : ESC_CLARO); /* (v98) llega de una vez */
        s_pet.ultima_interaccion = ahora();
        if (hal_display_lock()) { ui_close_panel(); ui_demo_zona(a); hal_display_unlock(); }
        probar_info("ENTRAR");
    } else if (sscanf(l, "JUEGOLUGAR %d", &a) == 1) { /* (v97) abre un juego de los lugares secretos en su pantalla: 1 horno, 2 camita, 3 nubes, 4 corona, 5 piedra */
        if (a < 1 || a > 5) { printf("MASCOTA PROBAR JUEGOLUGAR ERROR %d\n", a); return; }
        const int z = a <= 2 ? 1 : 3; /* (v98) los de la casita se juegan desde el claro; los otros, desde el mirador */
        s_pet.lugares |= 7u;
        s_pet.escenas_abiertas |= (1u << ESC_BOSQUE) | (1u << ESC_COLINA);
        s_pet.dormida = 0;
        s_pet.ultima_interaccion = ahora();
        if (hal_display_lock()) {
            ui_close_panel();
            const int zz = ui_zona();
            if (zz != z && zz != 2) { s_pet.escena = (uint8_t)(z == 3 ? ESC_MIRADOR : ESC_CLARO); ui_demo_zona(z); }
            ui_demo_zona_juego(a);
            hal_display_unlock();
        }
        probar_info("JUEGOLUGAR");
    } else if (!strcmp(l, "LUGARINFO")) { /* (v97) zona secreta abierta (0 ninguna, 1 claro, 2 casita, 3 mirador) y el juego: juego*100 + 50 si ganó + avance */
        int zz = 0, jj = 0, vv = -1;
        if (hal_display_lock()) { zz = ui_zona(); jj = ui_zj_estado(); vv = ui_escena_vista(); hal_display_unlock(); }
        printf("MASCOTA PROBAR LUGARINFO OK zona=%d juego=%d vista=%d escena=%d\n", zz, jj, vv, (int)s_pet.escena);
    } else if (!strcmp(l, "SALIRLUGAR")) { /* (v97) cierra el juego del lugar (si hay) y sale de la zona secreta */
        if (hal_display_lock()) { ui_demo_zona_salir(); hal_display_unlock(); }
        probar_info("SALIRLUGAR");
    } else if (sscanf(l, "TOCARLUGAR %d %d", &a, &b) == 2) { /* (v97) un toque en (x, y) del juego del lugar abierto (sin el táctil) */
        if (hal_display_lock()) { ui_zj_tocar(a, b); hal_display_unlock(); }
        printf("MASCOTA PROBAR TOCARLUGAR OK\n");
    } else if (sscanf(l, "TRAMPALUGAR %d", &a) == 1) { /* (v97) pruebas: con 1, cada toque en un juego de los lugares acierta */
        if (hal_display_lock()) { ui_zj_trampa(a); hal_display_unlock(); }
        probar_info("TRAMPALUGAR");
    } else if (!strcmp(l, "NUBE")) { /* (v97) en el cerro: la nube baja a buscarla y suben al mirador (el viaje completo) */
        s_pet.lugares |= 2u;
        s_pet.escenas_abiertas |= (1u << ESC_COLINA);
        s_pet.dormida = 0;
        s_pet.ultima_interaccion = ahora();
        int vista = -1;
        if (hal_display_lock()) { ui_close_panel(); vista = ui_escena_vista(); hal_display_unlock(); }
        s_pet.escena = (uint8_t)(vista == ESC_COLINA ? ESC_MIRADOR : ESC_COLINA); /* (v98) en el cerro: suben (sin preguntarle si quiere); si no, primero va al cerro */
        probar_info("NUBE");
    } else if (sscanf(l, "EXCURSION %d", &a) == 1) { /* (v96) se va sola: 0 al claro, 1 al mirador; -1 que vuelva ya */
        if (a < -1 || a > 1) { printf("MASCOTA PROBAR EXCURSION ERROR %d\n", a); return; }
        if (a >= 0) {
            s_pet.lugares |= 7u;
            s_pet.escenas_abiertas |= (1u << ESC_BOSQUE) | (1u << ESC_COLINA);
            s_pet.dormida = 0;
            s_pet.escena = (uint8_t)(a == 1 ? ESC_COLINA : ESC_BOSQUE);
        }
        if (hal_display_lock()) { ui_demo_excursion(a); hal_display_unlock(); }
        probar_info("EXCURSION");
    } else if (!strcmp(l, "ESCONDER")) { /* (v96) vuelve a esconder algo hoy en el claro, la casita y el mirador */
        s_pet.zona_hallazgo[0] = s_pet.zona_hallazgo[1] = s_pet.zona_hallazgo[2] = 0;
        probar_info("ESCONDER");
    } else if (sscanf(l, "PASEO %d", &a) == 1) { /* (fw 104) como si hubieran caminado a pasos (800+ cuenta; 2500+ trae algo) */
        s_pet.paseo_n = 0; s_pet.dormida = 0;
        const int r = pet_walk_together(&s_pet, (uint32_t)(a > 0 ? a : 0), ahora());
        printf("MASCOTA PASEO r=%d\n", r);
        probar_info("PASEO");
    } else if (sscanf(l, "PAPELITO %d", &a) == 1) { /* (fw 104) el próximo papelito del bosque: 0 lo encuentras tú, 1 te lo trae ella */
        const int k = pet_papelito_test(&s_pet, a, ahora());
        printf("MASCOTA PAPELITO k=%d\n", k);
        probar_info("PAPELITO");
    } else if (sscanf(l, "HIELO %d", &a) == 1) { /* (fw 104) el trocito de hielo: 1 lo pesca, 2 se derrite */
        pet_ice_test(&s_pet, a, ahora());
        probar_info("HIELO");
    } else if (sscanf(l, "TERCA %d", &a) == 1) { /* (fw 104) 1 la vuelve muy terca (traviesa, valiente, poco vínculo), 0 dócil: para ver los «no» */
        s_pet.rasgo[TRAIT_TRAVIESA] = a ? 0.95f : 0.05f; s_pet.rasgo[TRAIT_TIMIDA] = a ? 0.05f : 0.95f;
        printf("MASCOTA TERCA %.2f\n", (double)pet_stubbornness(&s_pet));
        probar_info("TERCA");
    } else if (!strncmp(l, "SINLIENZO", 9)) { /* (fw 105) fondos por franjas: 1 como si faltara memoria para el lienzo, 0 normal, R rehacer el fondo de ahora */
        const char *q = l + 9;
        while (*q == ' ') q++;
        if (*q == 'R' || *q == 'r') art_fondo_rehacer();
        else art_fondo_sin_lienzo(*q == '1' ? 1 : 0);
        printf("MASCOTA SINLIENZO %d\n", art_fondo_sin_lienzo_ver());
        probar_info("SINLIENZO");
    } else if (sscanf(l, "TOQUE %d", &a) == 1) { /* (fw 103) ver la reacción a tocar un animal: 1 saluda, 2 avisa, 3 pica, 4 púas, 5 rocía, 6 ruge, 7 se va */
        if (a < 0 || a > 8) { printf("MASCOTA PROBAR TOQUE ERROR %d\n", a); return; }
        if (a == 5) { s_pet.hediondo = 18; s_pet.hediondo_min = 0; } /* el olor, para verlo */
        if (hal_display_lock()) { ui_close_panel(); ui_demo_toque(a); hal_display_unlock(); }
        probar_info("TOQUE");
    } else if (sscanf(l, "POCHI %d", &a) == 1) { /* llama a Pochi: 0 llega, 1 invita a pasear, 2 paseo, 3 se despide, 4 corre, 5 barro (y ver ui_demo_pochi) */
        if (a < 0 || a > 103) { printf("MASCOTA PROBAR POCHI ERROR %d\n", a); return; } /* (fw 102) también 10-13 cachorros, 20-22 trucos, 30-33 manías, 40-43 llegadas */
        if (hal_display_lock()) { ui_close_panel(); ui_demo_pochi(a); hal_display_unlock(); }
        probar_info("POCHI");
    } else if (sscanf(l, "CASITA %d", &a) == 1) { /* 0 sale de la casita, 1 entra a visitarla, 2 entra y salta en la cama (hace falta la decoración de la casita y estar en el prado) */
        if (a < 0 || a > 2) { printf("MASCOTA PROBAR CASITA ERROR %d\n", a); return; }
        if (hal_display_lock()) { ui_close_panel(); ui_test_casita(a); hal_display_unlock(); }
        probar_info("CASITA");
    } else if (sscanf(l, "NIEBLA %d", &a) == 1) { /* 1 hay niebla (1 h), 0 la despeja */
        if (a == 2) { printf("MASCOTA PROBAR NIEBLA OK niebla=%d\n", pet_fog(&s_pet, ahora()) ? 1 : 0); return; } /* 2 = solo mira si hay niebla */
        if (a) pet_fog_start(&s_pet, ahora(), 3600); else pet_fog_clear(&s_pet, ahora());
        printf("MASCOTA PROBAR NIEBLA OK niebla=%d\n", pet_fog(&s_pet, ahora()) ? 1 : 0);
        probar_info("NIEBLA");
    } else if (sscanf(l, "TAMANO %d", &a) == 1) { /* tamaño de las criaturas en todas partes: 100, 120, 130 o 140 %; queda guardado */
        if (a != 100 && a != 120 && a != 130 && a != 140) { printf("MASCOTA PROBAR TAMANO ERROR %d (usa 100, 120, 130 o 140)\n", a); return; }
        uint8_t v = (uint8_t)(a - 100);
        hal_storage_set("tamano", &v, sizeof v);
        if (hal_display_lock()) { art_tamano_set(a); ui_close_panel(); hal_display_unlock(); }
        printf("MASCOTA PROBAR TAMANO OK %d\n", a);
        probar_info("TAMANO");
    } else if (!strcmp(l, "SENSORES")) { /* acelerómetro, batería, reloj, pasos, flash, fotos y radio: cuánto tarda cada uno */
        probar_sensores();
        probar_info("SENSORES");
    } else if (sscanf(l, "MIC %d", &a) == 1) { /* escucha el micrófono a ms (100..4000) */
        probar_mic(a);
        probar_info("MIC");
    } else if (sscanf(l, "MUSICA %d", &a) == 1) { /* 1 activa que ella escuche música (tandas de 6 s cada 20 s), 0 la apaga; no se guarda */
        s_dispo.musica = a ? 1 : 0;
        probar_info("MUSICA");
    } else if (sscanf(l, "AMBIENTE %d", &a) == 1) { /* fuerza un sonido ambiente (0 nada ... AMB_COUNT-1); -1 vuelve al normal */
        if (a < -1 || a >= AMB_COUNT) { printf("MASCOTA PROBAR AMBIENTE ERROR %d\n", a); return; }
        s_prueba_amb = a;
        probar_info("AMBIENTE");
    } else if (!strcmp(l, "CUENTAS")) {
        printf("MASCOTA PROBAR CUENTAS OK sonidos=%d ambientes=%d criaturas=%d secretos=%d zonas=%d especies=%d colores=%d\n", (int)SND_COUNT, (int)AMB_COUNT, (int)CRI_COUNT, (int)ESP_COUNT, (int)ESC_COUNT, (int)pet_species_count(), (int)PET_COLORES);
    } else if (!strcmp(l, "VISITA")) { /* llega una visita de otra especie ahora mismo (si la mascota está despierta) */
        printf("MASCOTA PROBAR VISITA %s\n", pet_visit_now(&s_pet, ahora()) ? "OK" : "ERROR no se pudo (¿dormida o ya hay una visita?)");
        probar_info("VISITA");
    } else if (sscanf(l, "VOLUMEN %d %d %d", &a, &b, &c) == 3) { /* ella, efectos, alarmas (0..100); no se guarda */
        s_dispo.volumen = (uint8_t)a; s_dispo.efectos = (uint8_t)b; s_dispo.alarmas = (uint8_t)c;
        pet_device_fix(&s_dispo);
        hal_sound_gains(s_dispo.volumen, s_dispo.efectos, pet_device_alarm_volume(&s_dispo));
        probar_info("VOLUMEN");
    } else if (sscanf(l, "SACAR_ZONA %d", &a) == 1) { /* la saca de la zona a (1..5) y borra el registro de su primera visita; el resto de la partida no se toca */
        const int r = pet_scene_forget(&s_pet, (escena_t)a, ahora());
        if (r < 0) { printf("MASCOTA PROBAR SACAR_ZONA ERROR %d\n", a); return; }
        s_guardar_ext = true;
        printf("MASCOTA PROBAR SACAR_ZONA OK zona=%d borrado=%d\n", a, r);
        probar_info("SACAR_ZONA");
    } else if (sscanf(l, "ACTIVIDAD %d", &a) == 1) { /* fuerza una actividad 4 min (24 = cantar, 18 = chapotear, 2 = cavar, 25 = bailar...) */
        if (a < 0 || a >= ACTV_COUNT) { printf("MASCOTA PROBAR ACTIVIDAD ERROR %d\n", a); return; }
        s_pet.dormida = false;
        s_pet.actividad = (uint8_t)a;
        s_pet.actividad_hasta = ahora() + 240u;
        if (a == ACTV_CANTAR) hal_sound_play(SND_ALEGRE);
        probar_info("ACTIVIDAD");
    } else if (!strcmp(l, "BATERIA")) { /* para el probador: la batería en vivo */
        printf("MASCOTA PROBAR BATERIA OK pct=%d crudo=%d mv=%d vbus=%d carga=%d fase=%d llena=%d pantalla=%d\n", hal_battery_percent(), hal_battery_gauge_raw(), hal_battery_mv(),
               hal_battery_vbus() ? 1 : 0, hal_battery_charging() ? 1 : 0, hal_battery_stage(), hal_battery_full() ? 1 : 0, s_pantalla_on ? 1 : 0);
    } else if (!strcmp(l, "CARGAEV") || !strcmp(l, "CARGAEV 0")) { /* los cambios del cable USB anotados por la placa (CARGAEV 0 los borra) */
        if (!strcmp(l, "CARGAEV 0")) { s_cev_n = 0; printf("MASCOTA PROBAR CARGAEV OK borrados up=%u vbus=%d\n", (unsigned)(esp_timer_get_time() / 1000), hal_battery_vbus() ? 1 : 0); return; }
        printf("MASCOTA PROBAR CARGAEV OK n=%d up=%u vbus=%d mv=%d pct=%d ev=", s_cev_n, (unsigned)(esp_timer_get_time() / 1000), hal_battery_vbus() ? 1 : 0, hal_battery_mv(), hal_battery_percent());
        for (int i = 0; i < s_cev_n; i++) printf("%s%d:%u:%d:%d", i ? ";" : "", s_cev[i].vbus, (unsigned)s_cev[i].ms, s_cev[i].mv, s_cev[i].pct);
        printf("\n");
    } else if (sscanf(l, "SONIDO %d", &a) == 1) { /* 0 toque (ella), 1 efecto, 2 alarma */
        hal_sound_play(a == 0 ? SND_TOQUE : (a == 1 ? SND_ATRAPA : pet_alarm_sound(0)));
        probar_info("SONIDO");
    } else if (sscanf(l, "SONAR %d", &a) == 1) { /* hace sonar el sonido número a (por ejemplo, las voces de los personajes) */
        if (a <= 0 || a >= SND_COUNT) { printf("MASCOTA PROBAR SONAR ERROR %d\n", a); return; }
        hal_sound_play((sound_t)a);
        printf("MASCOTA PROBAR SONAR OK %d\n", a);
    } else if (sscanf(l, "LISTA %15s", tipo) == 1) { /* los nombres de un tipo, para el probador */
        int n = prueba_cuenta(tipo);
        if (n < 0) { printf("MASCOTA PROBAR LISTA ERROR tipo desconocido\n"); return; }
        printf("MASCOTA PROBAR LISTA %s OK %d", tipo, n);
        for (int i = 0; i < n; i++) {
            char nm[64];
            snprintf(nm, sizeof nm, "%s", prueba_nombre(tipo, i));
            for (char *q = nm; *q; q++) if (*q == '|' || *q == '\n' || *q == '\r') *q = ' ';
            printf("|%s", nm);
        }
        printf("\n");
    } else if (sscanf(l, "ABRIR %15s %d", tipo, &a) == 2) { /* abre uno (o todos con -1); guarda antes la partida */
        int n = prueba_cuenta(tipo);
        if (n < 0 || a >= n || !strcmp(tipo, "estacion") || !strcmp(tipo, "clima") || !strcmp(tipo, "sonidos")) { printf("MASCOTA PROBAR ABRIR ERROR %s %d\n", tipo, a); return; }
        if (!probar_respaldar()) { printf("MASCOTA PROBAR ABRIR ERROR no pude guardar el respaldo\n"); return; }
        for (int i = a < 0 ? 0 : a; i < (a < 0 ? n : a + 1); i++) prueba_abrir_uno(tipo, i);
        if (a >= 0) prueba_aplicar_uno(tipo, a, true);
        probar_info("ABRIR");
    } else if (sscanf(l, "BLOQUEAR %15s %d", tipo, &a) == 2) { /* vuelve a cerrar uno (o todos con -1) del tipo; guarda antes la partida */
        int n = prueba_cuenta(tipo);
        if (n < 0 || a >= n || !strcmp(tipo, "estacion") || !strcmp(tipo, "clima") || !strcmp(tipo, "sonidos")) { printf("MASCOTA PROBAR BLOQUEAR ERROR %s %d\n", tipo, a); return; }
        if (!probar_respaldar()) { printf("MASCOTA PROBAR BLOQUEAR ERROR no pude guardar el respaldo\n"); return; }
        for (int i = a < 0 ? 0 : a; i < (a < 0 ? n : a + 1); i++) prueba_cerrar_uno(tipo, i);
        if (a >= 0) prueba_aplicar_uno(tipo, a, false);
        probar_info("BLOQUEAR");
    } else if (sscanf(l, "ESTACION %d", &a) == 1) { /* 0 primavera .. 3 invierno; -1: la de la fecha */
        if (a < -1 || a >= ESTACION_COUNT || !probar_respaldar()) { printf("MASCOTA PROBAR ESTACION ERROR %d\n", a); return; }
        s_pet.estacion_fija = a < 0 ? 0xFF : (uint8_t)a;
        probar_info("ESTACION");
    } else if (!strncmp(l, "GIRO CAL ", 9)) { /* calibrar el giro: 0 = sosteniéndolo derecho, 1 = con su lado derecho abajo, R = borrar */
        if (l[9] == 'R') { memcpy(s_giro_m, GIRO_M_FABRICA, sizeof s_giro_m); hal_storage_set("giroM", s_giro_m, sizeof s_giro_m); printf("MASCOTA PROBAR GIRO CAL OK borrada (vuelve a la de fábrica)\n"); return; }
        const int cual = l[9] == '1' ? 1 : 0;
        if (!giro_medir(cual)) { printf("MASCOTA PROBAR GIRO CAL ERROR %s\n", cual ? "no es un cuarto de vuelta desde la posición derecha" : "no se ve gravedad en la pantalla: sostenlo de pie"); return; }
        printf("MASCOTA PROBAR GIRO CAL OK %d (matriz %d %d %d %d)\n", cual, s_giro_m[0], s_giro_m[1], s_giro_m[2], s_giro_m[3]);
    } else if (sscanf(l, "GIRO %d", &a) == 1) { /* prueba el giro de la pantalla sin el acelerómetro: 0 derecho, 1 = 90°, 2 = al revés, 3 = 270° */
        if (a < 0 || a > 3) { printf("MASCOTA PROBAR GIRO ERROR %d\n", a); return; }
        if (hal_display_lock()) { hal_display_rotate_q(a); ui_set_upside_down(a == 2); hal_display_unlock(); }
        printf("MASCOTA PROBAR GIRO OK %d\n", a);
    } else if (!strncmp(l, "MOMENTO ", 8)) { /* cambia la hora del día de este perfil: MANANA, TARDE, NOCHE, MADRUGADA, HH[:MM] o REAL (no toca el reloj) */
        const char *q = l + 8;
        int hh = 0, mm = 0, objetivo = -1;
        if (!strcmp(q, "MANANA")) objetivo = 8 * 3600;
        else if (!strcmp(q, "TARDE")) objetivo = 15 * 3600;
        else if (!strcmp(q, "NOCHE")) objetivo = 21 * 3600 + 30 * 60;
        else if (!strcmp(q, "MADRUGADA")) objetivo = 3 * 3600;
        else if (!strcmp(q, "REAL")) { pet_time_shift(&s_pet, -s_pet.desfase_s); printf("MASCOTA PROBAR MOMENTO OK real\n"); probar_info("MOMENTO"); return; }
        else if (sscanf(q, "%d:%d", &hh, &mm) >= 1 && hh >= 0 && hh < 24 && mm >= 0 && mm < 60) objetivo = hh * 3600 + mm * 60;
        if (objetivo < 0) { printf("MASCOTA PROBAR MOMENTO ERROR %s\n", q); return; }
        int64_t local = (int64_t)ahora() + s_pet.zona_horaria;
        int32_t delta = (int32_t)(objetivo - (int32_t)(((local % 86400) + 86400) % 86400));
        if (delta > 43200) delta -= 86400;
        if (delta < -43200) delta += 86400;
        pet_time_shift(&s_pet, delta);
        printf("MASCOTA PROBAR MOMENTO OK %02d:%02d\n", objetivo / 3600, objetivo / 60 % 60);
        probar_info("MOMENTO");
    } else if (sscanf(l, "CLIMA %d", &a) == 1) {
        if (a < 0 || a >= CLIMA_COUNT || !probar_respaldar()) { printf("MASCOTA PROBAR CLIMA ERROR %d\n", a); return; }
        pet_force_weather(&s_pet, (clima_t)a, ahora(), 6 * 3600);
        probar_info("CLIMA");
    } else if (!strcmp(l, "DESBLOQUEAR TODO")) { /* todo: zonas, ropa, juguetes, objetos, comidas, cartas, secretos... (guarda antes la partida) */
        if (!probar_respaldar()) { printf("MASCOTA PROBAR DESBLOQUEAR TODO ERROR no pude guardar el respaldo\n"); return; }
        probar_desbloquear_todo();
        probar_info("DESBLOQUEAR TODO");
    } else if (!strcmp(l, "RESTAURAR")) { /* devuelve la partida al estado de antes de DESBLOQUEAR TODO */
        if (!probar_restaurar()) { printf("MASCOTA PROBAR RESTAURAR ERROR no hay respaldo\n"); return; }
        if (hal_display_lock()) { ui_close_panel(); hal_display_unlock(); }
        probar_info("RESTAURAR");
    } else if (!strcmp(l, "DESBLOQUEAR")) { /* zonas, ropa y juguetes */
        s_pet.escenas_abiertas = (1u << ESC_COUNT) - 1;
        s_pet.ropa_tiene = (1u << ROPA_COUNT) - 1;
        s_pet.juguetes_tiene = (1u << JUG_COUNT) - 1;
        probar_info("DESBLOQUEAR");
    } else if (sscanf(l, "ACCION %d %d", &a, &b) == 2) { /* cualquier acción del juego (alimentar, bañar, acariciar, vestir, ir a una zona, truco...): la prueba automática las recorre */
        if (a < 0 || a >= ACT_COUNT) { printf("MASCOTA PROBAR ACCION ERROR %d\n", a); return; }
        const int r = pet_action(&s_pet, (action_t)a, b, ahora());
        printf("MASCOTA PROBAR ACCION OK accion=%d arg=%d resultado=%d\n", a, b, r);
        probar_info("ACCION");
    } else if (sscanf(l, "PANTALLA %d", &a) == 1) { /* 0 apaga como el botón PWR, 1 enciende, 2 modo pecera (PWR dos veces) */
        if (a < 0 || a > 2) { printf("MASCOTA PROBAR PANTALLA ERROR %d\n", a); return; }
        if (a == 0) s_apagada_boton = true;
        else {
            s_apagada_boton = false;
            despertar_pantalla();
            if (a == 2 && hal_display_lock()) { ui_pecera(); hal_display_unlock(); }
        }
        vTaskDelay(pdMS_TO_TICKS(700)); /* que el bucle principal alcance a aplicarlo */
        printf("MASCOTA PROBAR PANTALLA OK %d pantalla=%d brillo=%d\n", a, s_pantalla_on ? 1 : 0, s_brillo);
        probar_info("PANTALLA");
    } else if (sscanf(l, "BRILLO %d", &a) == 1) { /* 5..100; no se guarda */
        if (a < 5) a = 5;
        if (a > 100) a = 100;
        s_dispo.brillo = (uint8_t)a;
        s_brillo = -1; /* que se aplique ya */
        printf("MASCOTA PROBAR BRILLO OK %d\n", a);
        probar_info("BRILLO");
    } else if (!strcmp(l, "VOLSUBIR") || !strcmp(l, "VOLBAJAR")) { /* como apretar subir / bajar volumen: ±10 en las tres barras; no se guarda */
        const int paso = l[3] == 'S' ? 10 : -10;
        int v[3] = {s_dispo.volumen + paso, s_dispo.efectos + paso, s_dispo.alarmas + paso};
        for (int i = 0; i < 3; i++) { if (v[i] < 0) v[i] = 0; if (v[i] > 100) v[i] = 100; }
        s_dispo.volumen = (uint8_t)v[0]; s_dispo.efectos = (uint8_t)v[1]; s_dispo.alarmas = (uint8_t)v[2];
        pet_device_fix(&s_dispo);
        hal_sound_gains(s_dispo.volumen, s_dispo.efectos, pet_device_alarm_volume(&s_dispo));
        hal_sound_play(SND_TOQUE); /* se oye el cambio */
        probar_info(paso > 0 ? "VOLSUBIR" : "VOLBAJAR");
    } else if (!strncmp(l, "MARCA ", 6)) { /* una marca en el registro de la placa: así se ve en el log en qué parte de la prueba pasó cada cosa */
        hal_log("PRUEBA: %.80s", l + 6);
        printf("MASCOTA PROBAR MARCA OK\n");
    } else if (!strcmp(l, "TOQUE")) { /* como tocar la pantalla: cuenta como actividad (enciende, sube el procesador) */
        if (hal_display_lock()) { lv_display_trigger_activity(s_disp); hal_display_unlock(); }
        printf("MASCOTA PROBAR TOQUE OK\n");
    } else if (sscanf(l, "OBJETO %d", &a) == 1) { /* usa un objeto de la Bolsa y lo devuelve (hay que tenerlo: ABRIR objetos) */
        if (a < 0 || a >= PET_OBJETOS) { printf("MASCOTA PROBAR OBJETO ERROR %d\n", a); return; }
        const int r = pet_object_use(&s_pet, a, ahora());
        pet_object_return(&s_pet, a, ahora());
        printf("MASCOTA PROBAR OBJETO OK id=%d resultado=%d\n", a, r);
        probar_info("OBJETO");
    } else if (sscanf(l, "LLAMAR %d", &a) == 1) { /* llama a un animal del bosque: aparece ahora */
        if (a < 0 || a >= ANIMAL_COUNT) { printf("MASCOTA PROBAR LLAMAR ERROR %d\n", a); return; }
        s_pet.dormida = 0;
        pet_animal_call(&s_pet, (animal_t)a, ahora());
        printf("MASCOTA PROBAR LLAMAR OK %d %s\n", a, pet_animal_name((animal_t)a));
        probar_info("LLAMAR");
    } else if (!strcmp(l, "FONDO")) { /* ¿está cargado el fondo de la zona? (la prueba del bote y de las zonas lo mira) */
        int esc = -1, per = -1, w = 0, h = 0, ok = 0;
        if (hal_display_lock()) { ok = ui_test_fondo(&esc, &per, &w, &h); hal_display_unlock(); }
        int cruce = -1;
        if (hal_display_lock()) { cruce = ui_test_cruce_fondo(); hal_display_unlock(); }
        printf("MASCOTA PROBAR FONDO OK cargado=%d escena_dibujada=%d periodo=%d ancho=%d alto=%d escena_pet=%d cruce=%d psram=%u\n", ok, esc, per, w, h, (int)s_pet.escena, cruce,
               (unsigned)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024));
    } else if (sscanf(l, "BOTE %d", &a) == 1) { /* 2 abre el menú del bote, 0 pescar, 1 ir a la isla: lo mismo que sus botones */
        if (a < 0 || a > 2) { printf("MASCOTA PROBAR BOTE ERROR %d\n", a); return; }
        if (hal_display_lock()) { ui_test_bote(a); hal_display_unlock(); }
        probar_info("BOTE");
    } else if (!strcmp(l, "PICNIC")) { /* el camino real de «comer»: la pantalla del picnic (el panel 0 del probador abre el menú ANTIGUO) */
        s_pet.dormida = 0;
        if (hal_display_lock()) { ui_close_panel(); ui_demo_picnic(); hal_display_unlock(); }
        probar_info("PICNIC");
    } else if (sscanf(l, "ECO %d", &a) == 1) { /* suena el sonido a y el micrófono escucha lo que llega: ¿el parlante suena de verdad? */
        if (a <= 0 || a >= SND_COUNT) { printf("MASCOTA PROBAR ECO ERROR %d\n", a); return; }
        hal_mic_enable(true);
        vTaskDelay(pdMS_TO_TICKS(150));
        int base = 0, nb = 0, pico = 0, suma = 0, ns = 0;
        for (int t0 = 0; t0 < 450; t0 += 30) { int v = hal_mic_level(); if (v >= 0) { base += v; nb++; } vTaskDelay(pdMS_TO_TICKS(30)); }
        hal_sound_play((sound_t)a);
        for (int t0 = 0; t0 < 1500; t0 += 30) { int v = hal_mic_level(); if (v >= 0) { suma += v; ns++; if (v > pico) pico = v; } vTaskDelay(pdMS_TO_TICKS(30)); }
        hal_mic_enable(false);
        printf("MASCOTA PROBAR ECO OK base=%d con_sonido=%d pico=%d muestras=%d\n", nb ? base / nb : 0, ns ? suma / ns : 0, pico, ns);
    } else if (sscanf(l, "TOQUES %d", &a) == 1) { /* espera a segundos y dice dónde le tocaron la pantalla (prueba guiada del táctil); no bloquea el bucle: contesta cuando termina */
        memset(&s_esp, 0, sizeof s_esp);
        if (a < 1) a = 1;
        if (a > 30) a = 30;
        s_esp.cx = s_esp.cy = -1;
        sscanf(l, "TOQUES %*d %d %d %d %d", &s_esp.sal, &s_esp.cx, &s_esp.cy, &s_esp.cr); /* sal=1: termina apenas lo tocan; con x y radio, solo cerca de ese punto */
        ui_touch_reset();
        s_esp.fin = (uint32_t)(esp_timer_get_time() / 1000) + (uint32_t)a * 1000u;
        s_esp.tipo = 1;
    } else if (sscanf(l, "FLASH %d", &a) == 1) { /* guarda, lee y compara un archivo de 4 KB a veces seguidas (a ≤ 300): errores de memoria y tiempo */
        if (a < 1) a = 1;
        if (a > 300) a = 300;
        static EXT_RAM_BSS_ATTR uint8_t esc_b[4096], lee_b[4096];
        int malos = 0, fallo_g = 0, fallo_l = 0, peor = 0;
        int64_t ini = esp_timer_get_time();
        for (int i = 0; i < a; i++) {
            for (int k = 0; k < (int)sizeof esc_b; k++) esc_b[k] = (uint8_t)(k * 7 + i * 13);
            int64_t t0 = esp_timer_get_time();
            if (!hal_res_save("prueba_flash.bin", esc_b, sizeof esc_b)) fallo_g++;
            else if (hal_res_read("prueba_flash.bin", lee_b, sizeof lee_b) != (int)sizeof lee_b) fallo_l++;
            else if (memcmp(esc_b, lee_b, sizeof esc_b)) malos++;
            const int ms = (int)((esp_timer_get_time() - t0) / 1000);
            if (ms > peor) peor = ms;
            vTaskDelay(1);
        }
        hal_res_delete("prueba_flash.bin");
        printf("MASCOTA PROBAR FLASH OK vueltas=%d fallo_guardar=%d fallo_leer=%d distinto=%d peor_ms=%d total_ms=%d\n", a, fallo_g, fallo_l, malos, peor, (int)((esp_timer_get_time() - ini) / 1000));
    } else if (!strcmp(l, "FOTOPRUEBA")) { /* guarda una foto falsa, la lista, la lee, la borra y comprueba que se fue */
        static uint8_t foto[2048], vuelta[2048];
        for (int k = 0; k < (int)sizeof foto; k++) foto[k] = (uint8_t)(k * 31 + 5);
        uint16_t ids[16];
        const int antes = hal_photo_list(ids, 16);
        const int id = hal_photo_save(foto, sizeof foto);
        int en_lista = 0, tam = -1, igual = 0, borrada = 0;
        if (id >= 0) {
            const int n = hal_photo_list(ids, 16);
            for (int i = 0; i < n; i++) if (ids[i] == id) en_lista = 1;
            tam = hal_photo_size(id);
            igual = hal_photo_read(id, vuelta, sizeof vuelta) == (int)sizeof foto && !memcmp(foto, vuelta, sizeof foto);
            hal_photo_delete(id);
            const int d = hal_photo_list(ids, 16);
            borrada = 1;
            for (int i = 0; i < d; i++) if (ids[i] == id) borrada = 0;
        }
        printf("MASCOTA PROBAR FOTOPRUEBA OK id=%d antes=%d en_lista=%d tam=%d igual=%d borrada=%d\n", id, antes, en_lista, tam, igual, borrada);
    } else if (!strcmp(l, "ESTADO")) { /* lo que debe sobrevivir a un reinicio (se compara antes y después) */
        unsigned h = 2166136261u;
        for (const char *q = s_pet.nombre; *q; q++) h = (h ^ (uint8_t)*q) * 16777619u;
        printf("MASCOTA PROBAR ESTADO OK nombre_h=%u especie=%d etapa=%d color=%d zonas=%u ropa=%u juguetes=%u deco=%u final=%d\n", h, (int)s_pet.especie, (int)s_pet.etapa, (int)s_pet.color,
               (unsigned)s_pet.escenas_abiertas, (unsigned)s_pet.ropa_tiene, (unsigned)s_pet.juguetes_tiene, (unsigned)s_pet.decoraciones, (int)s_pet.final_estado);
    } else if (sscanf(l, "DESCARGA %d", &a) == 1) { /* 0 para, 1 corta (una vuelta de 3 etapas), 2 larga (hasta el 10 %); hace falta la batería sobre el 90 % */
        if (a == 0) { if (s_desc.activo) descarga_fin("parada desde el computador"); printf("MASCOTA PROBAR DESCARGA OK parada\n"); }
        else if (descarga_iniciar(a == 2)) printf("MASCOTA PROBAR DESCARGA OK iniciada %s: desenchufa el cable\n", a == 2 ? "larga" : "corta");
        else printf("MASCOTA PROBAR DESCARGA ERROR bateria %d%% (hace falta 90%% o más)\n", hal_battery_percent());
    } else if (sscanf(l, "MEMLIMPIAR %d", &a) == 1) { /* diagnóstico: ¿la PSRAM ocupada es caché (se puede soltar) o una fuga? 1 = caché de imágenes de LVGL, 2 = además las fotos de pantallas */
        const unsigned antes = (unsigned)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024);
        if (hal_display_lock()) { ui_close_panel(); ui_test_soltar_memoria(a); hal_display_unlock(); }
        vTaskDelay(pdMS_TO_TICKS(300));
        printf("MASCOTA PROBAR MEMLIMPIAR OK modo=%d psram_antes=%u psram_despues=%u liberado=%d\n", a, antes, (unsigned)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024), (int)((unsigned)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024)) - (int)antes);
    } else if (!strcmp(l, "TEMPERATURA")) { /* décimas de °C: procesador y acelerómetro (junto a la batería); calor=1 si la protección bajó el brillo */
        printf("MASCOTA PROBAR TEMPERATURA OK chip=%d imu=%d calor=%d\n", hal_temp_chip(), hal_temp_imu(), s_calor ? 1 : 0);
    } else if (!strcmp(l, "GUARDAR")) { /* guarda ya la partida en la flash */
        const esp_err_t r = hal_storage_save(&s_pet);
        printf("MASCOTA PROBAR GUARDAR %s\n", r == ESP_OK ? "OK" : "ERROR no se pudo guardar");
    } else if (!strcmp(l, "REINICIAR")) { /* reinicia la placa (la partida se guarda antes): se comprueba que vuelva igual */
        hal_storage_save(&s_pet);
        printf("MASCOTA PROBAR REINICIAR OK\n");
        fflush(stdout);
        vTaskDelay(pdMS_TO_TICKS(300));
        esp_restart();
    } else if (!strcmp(l, "VALIDAR")) { /* una partida sana debe pasar y una con cualquier byte cambiado debe ser rechazada (en memoria: la real no se toca) */
        static EXT_RAM_BSS_ATTR pet_t copia;
        copia = s_pet;
        pet_seal(&copia);
        const int sana = pet_validate(&copia) ? 1 : 0;
        int detectadas = 0, probadas = 0;
        const size_t pos[] = {8, 40, sizeof copia / 3, sizeof copia / 2, sizeof copia - 64, sizeof copia - 9};
        for (unsigned i = 0; i < sizeof pos / sizeof pos[0]; i++) {
            static EXT_RAM_BSS_ATTR pet_t mala;
            mala = copia;
            ((uint8_t *)&mala)[pos[i]] ^= 0x5A;
            probadas++;
            if (!pet_validate(&mala)) detectadas++;
        }
        printf("MASCOTA PROBAR VALIDAR OK sana=%d corruptas_detectadas=%d de=%d\n", sana, detectadas, probadas);
    } else if (sscanf(l, "SALTAR %d", &a) == 1) { /* viaja en el tiempo a horas (−1500..1500): así se ven los días, las semanas y las estaciones pasar; MOMENTO REAL lo deshace */
        if (a < -1500 || a > 1500 || !probar_respaldar()) { printf("MASCOTA PROBAR SALTAR ERROR %d\n", a); return; }
        pet_time_shift(&s_pet, a * 3600);
        printf("MASCOTA PROBAR SALTAR OK %d horas\n", a);
        probar_info("SALTAR");
    } else if (sscanf(l, "FECHA %d %d", &a, &b) == 2) { /* lleva la fecha de este perfil al mes a, día b (a las 12:00) del año en curso: fiestas y cumpleaños */
        if (a < 1 || a > 12 || b < 1 || b > 31 || !probar_respaldar()) { printf("MASCOTA PROBAR FECHA ERROR %d %d\n", a, b); return; }
        const time_t base = (time_t)((int64_t)time(NULL) + s_pet.zona_horaria + s_pet.desfase_s);
        struct tm tmv;
        gmtime_r(&base, &tmv);
        tmv.tm_mon = a - 1; tmv.tm_mday = b; tmv.tm_hour = 12; tmv.tm_min = 0; tmv.tm_sec = 0;
        const time_t objetivo = mktime(&tmv); /* (sin zona horaria en la placa: mktime trabaja en UTC) */
        const int32_t delta = (int32_t)(objetivo - base);
        pet_time_shift(&s_pet, delta);
        printf("MASCOTA PROBAR FECHA OK %02d-%02d\n", a, b);
        probar_info("FECHA");
    } else if (sscanf(l, "SACUDIDAS %d", &a) == 1) { /* guiada: durante a segundos cuenta las sacudidas y los movimientos; no bloquea el bucle */
        memset(&s_esp, 0, sizeof s_esp);
        if (a < 1) a = 1;
        if (a > 30) a = 30;
        sscanf(l, "SACUDIDAS %*d %d", &s_esp.minimo); /* con un segundo número, termina al llegar a esas sacudidas */
        s_esp.s0 = s_n_sac; s_esp.m0 = s_n_mov;
        s_esp.mnx = s_esp.mny = 9; s_esp.mxx = s_esp.mxy = -9;
        s_esp.fin = (uint32_t)(esp_timer_get_time() / 1000) + (uint32_t)a * 1000u;
        s_esp.tipo = 2;
    } else if (sscanf(l, "BOTONES %d", &a) == 1) { /* guiada: cuenta cuántas veces apretas el botón PWR (una y dos veces seguidas); no bloquea el bucle */
        memset(&s_esp, 0, sizeof s_esp);
        if (a < 1) a = 1;
        if (a > 30) a = 30;
        sscanf(l, "BOTONES %*d %d", &s_esp.que); /* 1 = termina al ver una pulsación; 2 = termina al ver una doble */
        s_esp.u0 = s_n_pwr1; s_esp.d0 = s_n_pwr2;
        s_esp.fin = (uint32_t)(esp_timer_get_time() / 1000) + (uint32_t)a * 1000u;
        s_esp.tipo = 3;
    } else if (sscanf(l, "GIROREAL %d", &a) == 1) { /* guiada: anota en qué posiciones estuvo la pantalla (0 derecho, 1 = 90°, 2 = al revés, 3 = 270°); no bloquea el bucle */
        memset(&s_esp, 0, sizeof s_esp);
        if (a < 1) a = 1;
        if (a > 40) a = 40;
        sscanf(l, "GIROREAL %*d %d", &s_esp.sal); /* 1 = termina al ver las 4 posiciones */
        s_esp.vistas = 1u << (s_giro_pos & 3);
        s_esp.fin = (uint32_t)(esp_timer_get_time() / 1000) + (uint32_t)a * 1000u;
        s_esp.tipo = 4;
    } else if (sscanf(l, "PUNTO %d %d", &a, &b) == 2) { /* dibuja un punto de mira en la pantalla de la mascota (x < 0 lo quita) */
        if (hal_display_lock()) { ui_test_punto(a, b); hal_display_unlock(); }
        printf("MASCOTA PROBAR PUNTO OK %d %d\n", a, b);
    } else if (sscanf(l, "SOPLO %d", &a) == 1) { /* guiada: escucha a segundos (sopla o aplaude): pico, muestras fuertes y golpes bruscos */
        if (a < 1) a = 1;
        if (a > 20) a = 20;
        hal_mic_enable(true);
        vTaskDelay(pdMS_TO_TICKS(120));
        int pico = 0, fuertes = 0, golpes = 0, n = 0, antes = 0, base = 0, nb = 0;
        for (int t0 = 0; t0 < 400; t0 += 25) { int v = hal_mic_level(); if (v >= 0) { base += v; nb++; } vTaskDelay(pdMS_TO_TICKS(25)); }
        int sal = 0; sscanf(l, "SOPLO %*d %d", &sal); /* 1 = termina al oír un soplido o un golpe fuerte */
        for (int t0 = 0; t0 < a * 1000; t0 += 25) {
            int v = hal_mic_level();
            if (v >= 0) { n++; if (v > pico) pico = v; if (v >= 20) fuertes++; if (v - antes >= 15) golpes++; antes = v; }
            vTaskDelay(pdMS_TO_TICKS(25));
            if (sal && (fuertes >= 6 || golpes >= 1)) break;
        }
        hal_mic_enable(false);
        printf("MASCOTA PROBAR SOPLO OK base=%d pico=%d fuertes=%d golpes=%d muestras=%d\n", nb ? base / nb : 0, pico, fuertes, golpes, n);
    } else if (!strcmp(l, "RELOJ")) { /* el reloj de la placa y el del juego: para ver si la hora sobrevive a un reinicio */
        printf("MASCOTA PROBAR RELOJ OK rtc=%u juego=%u desfase=%d zona=%d\n", (unsigned)hal_rtc_now(), (unsigned)ahora(), (int)s_pet.desfase_s, (int)s_pet.zona_horaria);
    } else if (sscanf(l, "ASPECTO %d %d %d %d", &a, &b, &c, &d) == 4) { /* especie, color, etapa (0 huevo..3 adulta) y forma: se ve distinta cada una (guarda antes la partida) */
        if (a < 0 || a >= (int)pet_species_count() || b < 0 || b >= PET_COLORES || c < 0 || c > 3 || d < 0 || d > 7 || !probar_respaldar()) { printf("MASCOTA PROBAR ASPECTO ERROR %d %d %d %d\n", a, b, c, d); return; }
        s_pet.especie = (uint8_t)a; s_pet.color = (uint8_t)b; s_pet.etapa = (stage_t)c; s_pet.forma = (uint8_t)d;
        printf("MASCOTA PROBAR ASPECTO OK especie=%d color=%d etapa=%d forma=%d\n", a, b, c, d);
        probar_info("ASPECTO");
    } else if (sscanf(l, "NINO %d", &a) == 1) { /* 0 normal, 1 niño (7 a 12), 2 infante (3 a 6) (guarda antes la partida) */
        if (a < 0 || a > 2 || !probar_respaldar()) { printf("MASCOTA PROBAR NINO ERROR %d\n", a); return; }
        s_pet.modo_nino = (uint8_t)a;
        probar_info("NINO");
    } else if (sscanf(l, "ALARMA %d", &a) == 1) { /* 0 suena la alarma, 1 la hora de dormir, 3 el aviso de tiempo; 9 la apaga */
        if (a != 0 && a != 1 && a != 3 && a != 9) { printf("MASCOTA PROBAR ALARMA ERROR %d\n", a); return; }
        if (hal_display_lock()) { if (a == 9) ui_test_alarma_cerrar(); else ui_test_alarma(a); hal_display_unlock(); }
        if (a == 0) hal_sound_play(pet_alarm_sound(0));
        printf("MASCOTA PROBAR ALARMA OK %d sonando=%d\n", a, ui_alarm_ringing() ? 1 : 0);
        probar_info("ALARMA");
    } else if (!strcmp(l, "ENERGIA")) { /* estado del ahorro de energía y de la pantalla */
        printf("MASCOTA PROBAR ENERGIA OK ligero=%d dormitadas=%u atenuada=%d cpu_rapido=%d suave=%d pantalla=%d brillo=%d sin_toques_s=%u dormida=%d cargando=%d profundo=%d bat=%d mv=%d panel=%d ambiente=%d alarma=%d\n", s_ls_ok ? 1 : 0, (unsigned)s_dormita_n, s_atenuada ? 1 : 0, s_pm_tomado ? 1 : 0, s_energia_suave ? 1 : 0,
               s_pantalla_on ? 1 : 0, s_brillo, (unsigned)(s_inactivo_ms / 1000), s_pet.dormida ? 1 : 0, s_cargando ? 1 : 0, s_profundo ? 1 : 0, hal_battery_percent(), hal_battery_mv(), (int)ui_panel_actual(), s_amb_dbg, ui_alarm_ringing() ? 1 : 0);
    } else if (!strcmp(l, "DRAGON")) { /* la cueva con el dragón de cristal despierto: escena del secreto y la melodía de los cristales */
        s_pet.escenas_abiertas |= 1u << ESC_CUEVA;
        s_pet.escena = ESC_CUEVA;
        s_pet.dormida = 0;
        s_pet.cristales_rondas = 3;
        /* (101b) ya no enciende «escuchar música»: la prueba la dejaba activada para siempre (micrófono cada 20 s) */
        if (hal_display_lock()) { ui_close_panel(); ui_cine(3); hal_display_unlock(); }
        static const sound_t MELODIA[] = {SND_CRISTAL_AZUL, SND_CRISTAL_VERDE, SND_CRISTAL_ROSA, SND_CRISTAL_ORO, SND_CRISTAL_LILA, SND_CRISTAL_ORO, SND_CRISTAL_ROSA, SND_CRISTAL_VERDE, SND_CRISTAL_AZUL};
        for (unsigned i = 0; i < sizeof MELODIA / sizeof MELODIA[0]; i++) { hal_sound_play(MELODIA[i]); vTaskDelay(pdMS_TO_TICKS(380)); }
        probar_info("DRAGON");
    } else printf("MASCOTA PROBAR ERROR comando desconocido\n");
}

static void comando_usb(const char *l)
{
    if (strncmp(l, "MASCOTA ", 8)) return;
    l += 8;
    if (!strcmp(l, "VERSION")) { /* (1.0.09) qué versión tiene: «MASCOTA VERSION 1.0.09 <nombre>» */
        printf("MASCOTA VERSION %s %s\n", FW_VERSION, FW_NOMBRE);
    } else if (!strcmp(l, "HOLA")) {
        printf("MASCOTA OK %d %s\n", PET_SAVE_VERSION, s_pet.nombre);
    } else if (!strncmp(l, "HORA ", 5)) {
        long long u = 0;
        long z = 0;
        if (sscanf(l + 5, "%lld %ld", &u, &z) != 2 || u < HORA_MINIMA_VALIDA || z < -14 * 3600 || z > 14 * 3600) {
            printf("MASCOTA ERROR hora\n");
            return;
        }
        long long dif = u - (long long)ahora();
        bool ajusta = s_hora_dudosa || dif > 20 || dif < -20 || z != s_zona;
        if (ajusta) {
            hal_rtc_set((uint32_t)u);
            struct timeval tv = {.tv_sec = (time_t)u};
            settimeofday(&tv, NULL);
            if (z != s_zona) { s_zona = (int32_t)z; hal_storage_set("zona", &s_zona, sizeof s_zona); }
            pet_set_timezone(&s_pet, s_zona);
            s_hora_dudosa = false;
            hal_rtc_trusted(true);
            if (hal_display_lock()) { ui_set_clock_doubtful(false); hal_display_unlock(); }
            ESP_LOGI(TAG, "hora del computador (zona %ld s)", z);
        }
        printf("MASCOTA HORA OK %d\n", ajusta ? 1 : 0);
    } else if (!strcmp(l, "RESPALDO")) {
        pet_seal(&s_pet);
        pet_mark_backup(&s_pet, ahora()); /* (para el aviso suave de respaldo) */
        pet_seal(&s_pet);
        if (pet_base64_encode((const uint8_t *)&s_pet, (int)sizeof s_pet, s_usb_txt, sizeof s_usb_txt) > 0)
            printf("MASCOTA RESPALDO %d %s\n", PET_SAVE_VERSION, s_usb_txt);
        else printf("MASCOTA ERROR\n");
    } else if (!strncmp(l, "RESTAURAR ", 10)) {
        if (!pet_backup_import(l + 10, &s_respaldo)) { printf("MASCOTA ERROR respaldo no valido o de otra version\n"); return; }
        s_hay_respaldo = true;
        if (hal_display_lock()) {
            ui_ask_restore(&s_respaldo);
            lv_display_trigger_activity(s_disp);
            hal_display_unlock();
        }
        encender_pantalla(true, 100);
        printf("MASCOTA ESPERANDO\n");
    } else if (!strcmp(l, "FIESTAS")) {
        static uint8_t bin[16 + PET_CELEBRACIONES * sizeof(celebracion_t)];
        int n = pet_celebrations_pack(bin, sizeof bin);
        if (n > 0 && pet_base64_encode(bin, n, s_usb_txt, sizeof s_usb_txt) > 0) printf("MASCOTA FIESTAS %s\n", s_usb_txt);
        else printf("MASCOTA ERROR\n");
    } else if (!strcmp(l, "FIESTAS FABRICA")) {
        pet_celebrations_default();
        hal_storage_set("fiestas", NULL, 0);
        printf("MASCOTA OK\n");
    } else if (!strncmp(l, "FIESTAS ", 8)) {
        static uint8_t bin[16 + PET_CELEBRACIONES * sizeof(celebracion_t)];
        int n = pet_base64_decode(l + 8, bin, sizeof bin);
        if (n > 0 && pet_celebrations_unpack(bin, n) && hal_storage_set("fiestas", bin, (size_t)n) == ESP_OK) printf("MASCOTA OK\n");
        else printf("MASCOTA ERROR celebraciones no validas\n");
    } else if (!strcmp(l, "REGISTRO")) {
        static EXT_RAM_BSS_ATTR char reg[24576 + 12288 + 512 + 1]; /* v88: + el resumen de eventos raros al comienzo (v101: con margen, para que no se corte lo más nuevo) */
        int n = hal_log_read(reg, 24576 + 12288 + 512);
        reg[n] = 0;
        printf("MASCOTA REGISTRO INICIO %d\n", n);
        for (char *q = reg; *q;) {
            char *fin = strchr(q, '\n');
            if (fin) *fin = 0;
            printf("MASCOTA REGISTRO | %s\n", q);
            if (!fin) break;
            q = fin + 1;
            vTaskDelay(1); /* que no se atragante el USB */
        }
        printf("MASCOTA REGISTRO FIN\n");
    } else if (!strcmp(l, "DESCARGA")) { /* se reinicia directo en modo descarga: así se instala sin apretar BOOT */
        hal_log("reinicio en modo descarga pedido desde el computador");
        hal_log_flush(true);
        printf("MASCOTA OK modo descarga\n");
        vTaskDelay(pdMS_TO_TICKS(300));
        REG_WRITE(RTC_CNTL_OPTION1_REG, RTC_CNTL_FORCE_DOWNLOAD_BOOT);
        esp_restart();
    } else if (!strcmp(l, "EMPEZAR DE CERO")) { /* borra la partida y reinicia: nace una mascota nueva */
        hal_log("partida borrada desde el computador");
        hal_log_flush(true);
        if (hal_storage_erase_all() == ESP_OK) {
            printf("MASCOTA OK reiniciando\n");
            vTaskDelay(pdMS_TO_TICKS(300));
            esp_restart();
        } else printf("MASCOTA ERROR no se pudo borrar\n");
    } else if (!strcmp(l, "REGISTRO BORRAR")) {
        hal_log_clear();
        hal_log("registro borrado desde el computador");
        printf("MASCOTA OK\n");
    } else if (!strcmp(l, "FOTOS")) {
        static uint16_t ids[64];
        int n = hal_photo_list(ids, 64);
        printf("MASCOTA FOTOS %d", n);
        for (int i = 0; i < n; i++) printf(" %u:%d", ids[i], hal_photo_size(ids[i]));
        printf("\n");
    } else if (!strncmp(l, "FOTO ", 5)) {
        enviar_foto(atoi(l + 5));
    } else if (!strncmp(l, "ICONO ", 6)) {
        comando_icono(l + 6);
    } else if (!strncmp(l, "SONIDO ", 7)) {
        comando_sonido(l + 7, false);
    } else if (!strncmp(l, "AMBIENTE ", 9)) {
        comando_sonido(l + 9, true);
    } else if (!strncmp(l, "CRIATURA ", 9)) {
        comando_criatura(l + 9);
    } else if (!strncmp(l, "ESPECIE ", 8)) {
        comando_especie(l + 8);
    } else if (!strncmp(l, "PROBAR ", 7)) {
        comando_probar(l + 7);
    } else if (!strcmp(l, "CAMBIOS")) {
        listar_cambios();
    } else if (!strncmp(l, "BORRAR FOTO ", 12)) {
        printf(hal_photo_delete(atoi(l + 12)) ? "MASCOTA OK\n" : "MASCOTA ERROR\n");
    } else printf("MASCOTA ERROR comando desconocido\n");
}

/* las fotos: la interfaz las saca y estas funciones las guardan en la flash */
static int foto_guardar(const uint8_t *q, int n) { return hal_photo_save(q, n); }
static int foto_listar(uint16_t *ids, int max) { return hal_photo_list(ids, max); }
static int foto_leer(int id, uint8_t *b, int cap) { return hal_photo_read(id, b, cap); }
static int foto_tamano(int id) { return hal_photo_size(id); }
static bool foto_borrar(int id) { return hal_photo_delete(id); }
static const ui_fotos_t FOTOS = {foto_guardar, foto_listar, foto_leer, foto_tamano, foto_borrar};

/* pasar una foto al computador: en trozos de base64 por el USB */
static void enviar_foto(int id)
{
    int n = hal_photo_size(id);
    uint8_t *buf = n > 0 ? heap_caps_malloc((size_t)n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT) : NULL;
    if (!buf || hal_photo_read(id, buf, n) != n) {
        printf("MASCOTA ERROR no existe la foto %d\n", id);
        free(buf);
        return;
    }
    static char b64[4 * (3000 / 3) + 8];
    for (int k = 0; k < n; k += 3000) {
        int m = n - k < 3000 ? n - k : 3000;
        pet_base64_encode(buf + k, m, b64, sizeof b64);
        printf("MASCOTA FOTO %d %d %d %s\n", id, n, k, b64);
    }
    printf("MASCOTA FOTO %d FIN\n", id);
    free(buf);
}

static void despertar_pantalla(void)
{
    if (hal_display_lock()) {
        lv_display_trigger_activity(s_disp);
        hal_display_unlock();
    }
}

/* en qué paso del arranque iba (sobrevive a un reinicio por software): si el arranque se cuelga, el siguiente dice dónde */
static RTC_NOINIT_ATTR uint32_t s_arr_paso, s_arr_marca, s_arr_sub, s_arr_ui; /* v88: también el sub-paso del 17 y por dónde iba la interfaz */
#define ARR_MARCA 0xA55A1234u
#define PASO(n) (s_arr_paso = (n))
static const char *const ARR_NOMBRE[] = {"-", "almacenamiento", "usb", "zona y dispositivo", "pantalla", "reloj", "energía", "acelerómetro", "pasos", "sonido", "botón", "radio", "fotos", "recursos", "registro", "hora", "mascota", "interfaz", "bucle"};
static void arranque_vigilante(void *arg)
{
    (void)arg;
    if (s_arr_paso < 18) { /* 12 s y el arranque no llegó al bucle: se reinicia solo y el próximo arranque lo anota */
        s_arr_ui = (uint32_t)g_ui_paso; /* (este aviso corre en otra tarea: el ciclo principal está pegado, la interfaz dice dónde) */
        ESP_LOGE(TAG, "arranque colgado en el paso %u.%u (interfaz %u): reinicio", (unsigned)s_arr_paso, (unsigned)s_arr_sub, (unsigned)s_arr_ui);
        esp_restart();
    }
}

void app_main(void)
{
    const bool tenia_marca = s_arr_marca == ARR_MARCA;
    const uint32_t paso_antes = tenia_marca ? s_arr_paso : 0, sub_antes = tenia_marca ? s_arr_sub : 0, ui_antes = tenia_marca ? s_arr_ui : 0;
    s_arr_sub = 0; s_arr_ui = 0;
    s_arr_marca = ARR_MARCA;
    PASO(0);
    static esp_timer_handle_t vig;
    { const esp_timer_create_args_t a = {.callback = arranque_vigilante, .name = "arr_vig"};
      if (esp_timer_create(&a, &vig) == ESP_OK) esp_timer_start_once(vig, 12ull * 1000000ull); }
    vTaskPrioritySet(NULL, 8); /* la lógica del juego por encima de LVGL (4) y del sonido: antes se quedaba sin tiempo y no corría nunca */
    hal_night_boot_check(); /* (v101) si dormía de noche y nadie la despertó, vuelve a dormir aquí (en un momento) */
    hal_reserve_boot_check(); /* si estaba en modo reserva y sigue sin carga, vuelve a dormir aquí */
    ESP_ERROR_CHECK(hal_storage_init());
    PASO(1);
    if (ota_modo_pedido()) { /* (1.0.09) se pidió actualizar por Wi-Fi: modo liviano, sin el juego (no vuelve: termina reiniciando) */
        if (vig) esp_timer_stop(vig);
        PASO(18);
        ota_modo();
    }
    hal_usb_init();
    PASO(2); /* primero: la tarea del USB necesita RAM interna, que después se acaba */
    cargar_zona();
    cargar_dispositivo();
    PASO(3);
    hal_ram_log("inicio");
    s_disp = hal_display_init();
    PASO(4);
    hal_ram_log("pantalla");
    if (!s_disp) {
        ESP_LOGE(TAG, "no se pudo iniciar la pantalla");
        return;
    }
    hal_rtc_init();
    PASO(5);
    hal_power_init();
    PASO(6);
    cpu_ahorro_iniciar();
    if (hal_imu_init() != ESP_OK) ESP_LOGW("giro", "el acelerómetro no responde: la pantalla no se endereza ni se inclina");
    PASO(7);
    hal_steps_init();
    PASO(8);
    hal_sound_init();
    PASO(9);
    hal_button_init();
    PASO(10);
    hal_radio_init();
    PASO(11);
    hal_photos_init();
    PASO(12);
    hal_res_init();
    PASO(13);
    hal_log_init();
    { const char *dg = ota_diagnostico(); if (dg) hal_evento("ACTUALIZACIÓN (modo Wi-Fi): %s", dg); } /* (1.0.11) */
    { char red[24]; ota_nombre_red(red, sizeof red); ui_set_red_ota(red); } /* (1.0.11) para Ajustes → Versión */
    { const char *m = hal_night_wake_reason(); if (m) hal_log("energia: desperté del sueño profundo de noche (%s)", m); } /* (v101) */
    PASO(14);
    if (tenia_marca && paso_antes < 18) /* registro guardado en el dispositivo: desde acá anota el motivo del arranque (y al resumen de eventos raros) */
        hal_evento("ARRANQUE anterior: se quedó en el paso %u (%s), sub-paso %u (%s), interfaz en %u", (unsigned)paso_antes, paso_antes < 19 ? ARR_NOMBRE[paso_antes] : "?",
                   (unsigned)sub_antes, sub_antes == 1 ? "esperando el candado de la pantalla" : sub_antes == 2 ? "armando la interfaz" : sub_antes == 3 ? "primera actualización" : sub_antes == 4 ? "saludo" : "-",
                   (unsigned)ui_antes);
    hal_ram_log("servicios");
    preparar_hora();
    PASO(15);
    cargar_celebraciones();
    cargar_recursos();
    cargar_o_crear_mascota();
    PASO(16);

    hal_ram_log("mascota");
    s_acciones = xQueueCreate(8, sizeof(accion_msg_t));
    PASO(17);
    s_arr_sub = 1;
    if (hal_display_lock()) {
        s_arr_sub = 2;
        ui_init(s_disp, on_action);
        s_arr_sub = 3;
        ui_set_photo_store(&FOTOS);
        ui_set_pet_store(&MASCOTAS);
        ui_set_device(&s_dispo);
        ui_set_battery(hal_battery_percent(), hal_battery_charging());
        ui_set_clock_doubtful(s_hora_dudosa);
        ui_update(&s_pet, ahora());
        s_arr_sub = 4;
        if (s_pet.edad_dueno) { /* ya configurada: te saluda al encender; si el reinicio fue por un fallo, lo dice en rojo */
            const esp_reset_reason_t rr = esp_reset_reason();
            const bool fallo = rr == ESP_RST_PANIC || rr == ESP_RST_INT_WDT || rr == ESP_RST_TASK_WDT || rr == ESP_RST_WDT || rr == ESP_RST_BROWNOUT;
            /* (1.0.09) ¿cambió la versión desde el último arranque? (se actualizó) ¿o una actualización falló y volvió a la de antes? */
            char vant[33] = "", vmala[33] = "", t1[64];
            size_t lv = sizeof vant, lm = sizeof vmala;
            if (!hal_storage_get("fwv", vant, &lv)) vant[0] = 0;
            vant[sizeof vant - 1] = 0;
            if (!hal_storage_get("fwmala", vmala, &lm)) vmala[0] = 0;
            vmala[sizeof vmala - 1] = 0;
            const char *fallida = ota_version_fallida();
            const bool volvio = fallida && strcmp(fallida, vmala) && strcmp(fallida, FW_VERSION);
            const bool nueva = strcmp(vant, FW_VERSION) != 0;
            if (volvio) hal_storage_set("fwmala", fallida, strlen(fallida) + 1);
            if (nueva) hal_storage_set("fwv", FW_VERSION, strlen(FW_VERSION) + 1);
            if (fallo) ui_splash("¡UPS! ME REINICIÉ", rr == ESP_RST_PANIC ? "FALLO DEL PROGRAMA (PÁNICO)" : (rr == ESP_RST_BROWNOUT ? "POCA ENERGÍA" : "SE TRABÓ (WATCHDOG)"), true, 10000);
            else if (volvio) {
                snprintf(t1, sizeof t1, "LA %s NO FUNCIONÓ", fallida);
                hal_evento("ACTUALIZACIÓN: la %s no arrancó bien y se volvió a la %s", fallida, FW_VERSION);
                ui_splash(t1, "VOLVÍ A LA VERSIÓN " FW_VERSION, true, 8000);
            } else if (nueva) {
                hal_evento("ACTUALIZACIÓN: ahora en la versión %s (antes %s)", FW_VERSION, vant[0] ? vant : "sin número");
                ui_splash("¡VERSIÓN " FW_VERSION "!", FW_NOMBRE, false, 6000);
            } else ui_splash("¡HOLA! ¡VOLVISTE!", "", false, 3000);
        }
        hal_display_unlock();
        hal_ram_log("interfaz");
    }
    PASO(18);
    if (hal_display_lock()) { ui_boot_done(); hal_display_unlock(); } /* recién ahora puede abrirse la pregunta de la hora */
    hal_rtc_trusted(!s_hora_dudosa);
    { /* memoria al terminar de arrancar: el piso de PSRAM con el que parte el juego */
        hal_evento("ARRANQUE listo: PSRAM libre %u KB (bloque mayor %u KB), RAM interna %u B | por fase: %s", (unsigned)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024),
                   (unsigned)(heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM) / 1024), (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL), hal_ram_fases());
    }

    uint32_t ultimo_guardado = ahora(), ultimo_tick = 0, ultima_bateria = 0, ultima_baliza = 0;
    int amb_actual = -1, amb_vol = -1;
    bool guardar_pronto = false, bateria_guardada = false;
    int lecturas_bajas = 0, vol_actual = -1;
    uint32_t ultimo_paso = 0, pasos_antes = 0;
    uint32_t reserva_desde = 0;

    while (true) {
        uint32_t t = ahora();
        bool hubo_accion = false;
        { static bool confirmada; /* (1.0.09) un minuto jugando sin caerse: esta versión queda confirmada (si no, el arranque vuelve a la anterior) */
          if (!confirmada && esp_timer_get_time() > 60ll * 1000000ll) { confirmada = true; ota_arranque_ok(); } }
        if (s_guardar_ext) { s_guardar_ext = false; guardar_pronto = true; hubo_accion = true; }

        hal_main_tick();
        { /* latido del registro: si se pega, la última línea dice hasta cuándo vivió y en qué pantalla estaba */
            static uint32_t lat_ms, ant_ms, max_dt, suma_dt, n_dt, lento_ms;
            uint32_t ms = (uint32_t)(esp_timer_get_time() / 1000);
            { /* cuánto tarda cada vuelta del ciclo principal: las lentitudes quedan escritas con la escena en que ocurren */
                const uint32_t dt = ant_ms ? ms - ant_ms : 0;
                ant_ms = ms;
                if (dt > max_dt) max_dt = dt;
                suma_dt += dt; n_dt++;
                /* v88: la vuelta normal ya incluye su pausa (250 ms con ella dormida y la pantalla apagada): antes eso contaba como "lento" y llenaba el registro.
                 * Ahora es lenta solo si tardó 200 ms más que su pausa. */
                if (dt > s_espera_ms + 200u && ms - lento_ms > 2000) {
                    lento_ms = ms;
                    static const char *const SEC[11] = {"0 registro (log)", "1 acciones", "1b editor USB", "2 acelerómetro", "3 amigos", "4 motor y pantalla", "5 botón PWR", "5c micrófono (espera la pantalla)", "5a pasos", "5b reserva", "6 guardar"};
                    int peor = 0; int64_t pd = 0;
                    for (int k = 0; k < 11; k++) if (s_sec_us[k + 1] > s_sec_us[k] && s_sec_us[k] > 0 && s_sec_us[k + 1] - s_sec_us[k] > pd) { pd = s_sec_us[k + 1] - s_sec_us[k]; peor = k; }
                    char det[160] = "";
                    if (peor == 5) /* el paso 4 por dentro: dónde se trabó */
                        snprintf(det, sizeof det, " [candado pantalla %d ms, motor %d ms, eventos %d ms, batería %d ms, interfaz %d ms, resto %d ms]",
                                 (int)((s_p4[1] - s_p4[0]) / 1000), (int)((s_p4[0] - s_sec_us[5]) / 1000), (int)((s_p4[2] - s_p4[1]) / 1000),
                                 (int)((s_p4[3] - s_p4[2]) / 1000), (int)((s_p4[4] - s_p4[3]) / 1000), (int)((s_sec_us[6] - s_p4[4]) / 1000));
                    hal_log("lento: vuelta de %u ms (pausa %u), escena=%d panel=%d juego=%d pantalla %s | el paso más lento: %s (%d ms)%s", (unsigned)dt, (unsigned)s_espera_ms, ui_escena_especial(), ui_panel_actual(), ui_juego_actual(),
                            s_pantalla_on ? "on" : "off", SEC[peor], (int)(pd / 1000), det);
                    if (dt > 1500u) hal_evento("pantalla/ciclo trabado %u ms: %s%s, panel %d, escena %d, pantalla %s", (unsigned)dt, SEC[peor], det, ui_panel_actual(), ui_escena_especial(), s_pantalla_on ? "on" : "off");
                }
            }
            s_sec_us[0] = esp_timer_get_time();
            const uint32_t periodo = s_dormitando ? 300000u : ((ui_escena_especial() & ~8) ? 15000u : 20000u); /* (v100) dormitando: cada 5 min; (v101) en escenas especiales cada 15 s (antes 5: llenaba el registro) */ /* en juegos y escenas especiales, el latido es más seguido */
            if (ms - lat_ms > periodo) {
                lat_ms = ms;
                int vc, vp, vm, vk, vt, vtm;
                hal_vel_info2(&vc, &vp, &vm, &vk, &vt, &vtm);
                hal_log("vivo: escena=%d panel=%d juego=%d vuelta prom %u ms, máx %u ms (%u vueltas); RAM interna %u B (mín %u B, bloque %u B), PSRAM %u KB | pantalla: %d cuadros en 3 s, dibujar %d ms (máx %d), %d kpx por cuadro; animar %d ms (máx %d)", ui_escena_especial(), ui_panel_actual(), ui_juego_actual(),
                        n_dt ? (unsigned)(suma_dt / n_dt) : 0, (unsigned)max_dt, (unsigned)n_dt,
                        (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL), (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL),
                        (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL), /* (101b) el bloque mayor: si se pica la memoria se ve antes de caerse */
                        (unsigned)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024), vc, vp, vm, vk, vt, vtm);
                max_dt = suma_dt = n_dt = 0;
                {   /* qué pasa en el juego: ella (etapa, actividad, ánimo, necesidades) y lo que se ve */
                    char nec[NEED_COUNT * 5 + 1];
                    int k = 0;
                    for (int i = 0; i < NEED_COUNT && k < (int)sizeof nec - 5; i++) k += snprintf(nec + k, sizeof nec - (size_t)k, "%d ", (int)s_pet.necesidad[i]);
                    nec[k] = 0;
                    hal_log("juego: etapa=%u actividad=%u emoción=%u vínculo=%d dormida=%d luz_apagada=%d | necesidades [%s] | pantalla %s, panel=%d, escena=%d, juego=%d, alarma=%d",
                            (unsigned)s_pet.etapa, (unsigned)s_pet.actividad, (unsigned)s_pet.emocion, (int)s_pet.vinculo, s_pet.dormida ? 1 : 0, s_pet.luz_apagada ? 1 : 0, nec,
                            s_pantalla_on ? "on" : "off", ui_panel_actual(), ui_escena_especial(), ui_juego_actual(), ui_alarm_ringing() ? 1 : 0);
                }
            }
            { /* batería: cuánto baja mientras se usa sin el cargador (una línea cada vez que cambia el porcentaje, y al conectar/desconectar) */
                static uint32_t bat_ms, desde_ms;
                static int pct_desde = -1, pct_ult = -2, carga_ant = -1;
                if (ms - bat_ms >= 10000 || !bat_ms) {
                    bat_ms = ms ? ms : 1;
                    const int pct = hal_battery_percent();
                    const int carga = hal_battery_vbus() ? 1 : 0; /* enchufado (aunque ya esté llena y no esté cargando) */
                    if (pct >= 0) {
                        if (carga != carga_ant) {
                            hal_log("bateria: %s el cargador, %d%% (%d mV, medidor %d%%, fase %d)", carga ? "conectaron" : "sin", pct, hal_battery_mv(), hal_battery_gauge_raw(), hal_battery_stage());
                            carga_ant = carga;
                            pct_ult = pct;
                            if (!carga) { desde_ms = ms; pct_desde = pct; } else pct_desde = -1;
                        } else if (!carga && pct != pct_ult) {
                            if (pct_desde < 0) { desde_ms = ms; pct_desde = pct_ult >= 0 ? pct_ult : pct; }
                            const unsigned min = (unsigned)((ms - desde_ms) / 60000u);
                            const int bajo = pct_desde - pct;
                            hal_log("bateria: %d%% sin cargador hace %u min (bajó %d puntos), pantalla %s", pct, min, bajo, s_pantalla_on ? "encendida" : "apagada");
                            if (min >= 5 && bajo > 0) hal_log("bateria: ritmo %d.%d %%/hora", bajo * 60 / (int)min, (bajo * 600 / (int)min) % 10);
                            pct_ult = pct;
                        } else if (carga) pct_ult = pct;
                    }
                }
            }
            { /* qué está activo (cada 10 min y al arrancar): para saber en qué se va la batería */
                static uint32_t act_ms;
                if (!act_ms || ms - act_ms >= 600000u) {
                    act_ms = ms ? ms : 1;
#ifdef CONFIG_PM_ENABLE
                    const char *pm = "sí";
#else
                    const char *pm = "no";
#endif
                    hal_log("actividad: batería %d%% %s | pantalla %s brillo %u | CPU %d MHz, ahorro de energía %s | bluetooth %s | micrófono (música) %s | sonido: volumen %u, efectos %u, ambiente %u (suena: %s), arrullo %s | giro %s | ella %s%s | alarma sonando %s | panel %d, escena %d",
                            hal_battery_percent(), hal_battery_charging() ? "cargando" : "sin cargador",
                            s_pantalla_on ? "encendida" : "apagada", (unsigned)s_dispo.brillo,
                            s_pm_tomado ? 240 : 80, pm, /* v88: la frecuencia que de verdad se pidió (antes decía siempre 240: era la de fábrica, no la actual) */
                            MASCOTA_BLUETOOTH ? "activo (baliza cada 2 s)" : "apagado",
                            s_dispo.musica ? "escuchando" : "apagado",
                            (unsigned)s_dispo.volumen, (unsigned)s_dispo.efectos, (unsigned)s_dispo.ambiente,
                            amb_actual > 0 && s_dispo.ambiente ? "sí" : "no", s_dispo.arrullo ? "prendido" : "apagado",
                            s_dispo.girar ? "activo (acelerómetro)" : "apagado",
                            s_pet.dormida ? "dormida" : "despierta", s_pet.luz_apagada ? ", luz apagada" : "",
                            ui_alarm_ringing() ? "sí" : "no", ui_panel_actual(), ui_escena_especial());
                }
            }
            { /* v88: memoria al cambiar de zona o de escena especial (cruce, cristales, final, picnic): para ver dónde se va la PSRAM */
                static int zona_ant = -1, esp_ant = -1;
                const int zo = (int)s_pet.escena, es = ui_escena_especial() & ~8;
                if (zo != zona_ant || es != esp_ant) {
                    hal_log("memoria fase zona=%d escena=%d: PSRAM libre %u KB (bloque mayor %u KB), RAM interna %u B", zo, es, (unsigned)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024),
                            (unsigned)(heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM) / 1024), (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
                    zona_ant = zo; esp_ant = es;
                }
            }
            static int panel_ant = -1;
            int pa = ui_panel_actual();
            if (pa != panel_ant) { panel_ant = pa; hal_log("pantalla: panel=%d | RAM interna libre %u B (bloque mayor %u B, mínima %u B), PSRAM %u KB", pa, (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL), (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL), (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL), (unsigned)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024)); }
            hal_log_flush_pantalla(s_pantalla_on); /* v89: con la pantalla encendida, el registro se escribe cada 5 min (no cada minuto) */
            hal_log_flush(false);
        }
        s_sec_us[1] = esp_timer_get_time();
        /* 1. acciones del dueño (pantalla y tienda) */
        accion_msg_t m;
        while (xQueueReceive(s_acciones, &m, 0) == pdTRUE) {
            switch (m.accion) {
            case UI_ACCION_DECORAR: pet_place_decor(&s_pet, (decor_t)m.arg, t); break;
            case UI_ACCION_EDAD: pet_set_owner_age(&s_pet, m.arg); break;
            case UI_ACCION_NACIMIENTO:
                pet_set_owner_birthdate(&s_pet, m.arg % 100, m.arg / 100 % 100, m.arg / 10000, t);
                if (!s_pet.modo_manual) pet_set_mode_manual(&s_pet, MODO_ADULTO); /* por ahora siempre empieza en adulto; si la fecha es de un niño, la pantalla pregunta */
                break;
            case UI_ACCION_SONAR: if (m.arg & 0x100) hal_sound_play_soft((sound_t)(m.arg & 0xFF)); else hal_sound_play((sound_t)m.arg); break; /* la interfaz los pide a propósito: suenan */
            case UI_ACCION_VOLUMEN:
                s_dispo.volumen = (uint8_t)(m.arg & 0xFF);
                pet_device_fix(&s_dispo);
                hal_sound_gains(s_dispo.volumen, s_dispo.efectos, pet_device_alarm_volume(&s_dispo));
                if (m.arg & UI_SOLTADO) { guardar_dispositivo(); hal_sound_play(SND_TOQUE); } /* un pío de prueba */
                break;
            case UI_ACCION_EFECTOS:
                s_dispo.efectos = (uint8_t)(m.arg & 0xFF);
                pet_device_fix(&s_dispo);
                hal_sound_gains(s_dispo.volumen, s_dispo.efectos, pet_device_alarm_volume(&s_dispo));
                if (m.arg & UI_SOLTADO) { guardar_dispositivo(); hal_sound_play(SND_ATRAPA); } /* un sonido de prueba */
                break;
            case UI_ACCION_ALARMAS:
                s_dispo.alarmas = (uint8_t)(m.arg & 0xFF);
                pet_device_fix(&s_dispo);
                hal_sound_gains(s_dispo.volumen, s_dispo.efectos, pet_device_alarm_volume(&s_dispo));
                if (m.arg & UI_SOLTADO) { guardar_dispositivo(); hal_sound_play(pet_alarm_sound(0)); } /* suena la alarma con ese volumen */
                break;
            case UI_ACCION_BRILLO:
                s_dispo.brillo = (uint8_t)(m.arg & 0xFF);
                pet_device_fix(&s_dispo);
                s_brillo = -1; /* que se aplique ya */
                if (m.arg & UI_SOLTADO) guardar_dispositivo();
                break;
            case UI_ACCION_APAGADO: s_dispo.apagado = (uint8_t)m.arg; pet_device_fix(&s_dispo); guardar_dispositivo(); break;
            case UI_ACCION_PECERA_DUR: s_dispo.pecera = (uint8_t)m.arg; pet_device_fix(&s_dispo); guardar_dispositivo(); break;
            case UI_ACCION_DORMIR: pet_bedtime_set_packed(&s_pet, m.arg); break;
            case UI_ACCION_TEMPORIZADOR: pet_timer_set(&s_pet, (uint32_t)m.arg, t); break;
            case UI_ACCION_NIEBLA: pet_fog_clear(&s_pet, t); break;
            case UI_ACCION_COLOR: pet_set_color(&s_pet, m.arg); break;
            case UI_ACCION_ECLOSIONAR: pet_hatch(&s_pet, t); break;
            case UI_ACCION_HUEVO_EMPIEZA: pet_egg_start(&s_pet, t); break; /* (1.0.10) */
            case UI_ACCION_HABILIDAD: pet_ability_use(&s_pet, m.arg & 1, t); break;
            case UI_ACCION_BALLENA: pet_whale_ride(&s_pet, m.arg, t); break;
            case UI_ACCION_EVOLUCION: if (m.arg) pet_evo_accept(&s_pet, t); else pet_evo_defer(&s_pet); break;
            case UI_ACCION_BAILE: pet_dance(&s_pet, m.arg, t); break;
            case UI_ACCION_CARTA: pet_letter_open(&s_pet, t); break;
            case UI_ACCION_GENERO: s_pet.genero = (uint8_t)(m.arg != 0); break;
            case UI_ACCION_ANIMAL_VISTO: pet_animal_seen(&s_pet, m.arg, t); break;
            case UI_ACCION_CRIATURA: pet_creature_touch(&s_pet, t); break;
            case UI_ACCION_CITA_FIN: pet_creature_done(&s_pet, (cita_t)(m.arg & 0xFF), m.arg >> 8, t); break;
            case UI_ACCION_ESPECIAL: pet_special_do(&s_pet, (especial_t)(m.arg & 15), m.arg >> 4, t); break;
            case UI_ACCION_CRISTAL: pet_crystal_note(&s_pet, m.arg, t); break;
            case UI_ACCION_PAIS: if (m.arg & 0x100) pet_set_country_by_season(&s_pet, (estacion_t)(m.arg & 3), t); else pet_set_country(&s_pet, m.arg); break;
            case UI_ACCION_FECHA_OTRA: { /* otra fecha, solo para este perfil: la hora del reloj no se toca */
                int32_t nuevo = (int32_t)((int64_t)m.arg - s_pet.zona_horaria - (int64_t)time(NULL));
                pet_time_shift(&s_pet, nuevo - s_pet.desfase_s);
                t = ahora();
                break;
            }
            case UI_ACCION_POCHI: pet_pochi(&s_pet, m.arg, t); break;
            case UI_ACCION_TILDE: pet_tilde_touch(&s_pet, m.arg, t); break;
            case UI_ACCION_BRIZNA: pet_brizna_heard(&s_pet, t); break;
            case UI_ACCION_PRISMA: pet_prisma_touch(&s_pet, t); break;
            case UI_ACCION_TILO: pet_tilo_page_seen(&s_pet, m.arg); break;
            case UI_ACCION_LUGAR: pet_secret_find(&s_pet, m.arg >> 4, m.arg & 15, t); break;
            case UI_ACCION_PIEZA: pet_crystal_piece(&s_pet, m.arg & 0xFF, m.arg >> 8, t); break; /* (v38) */
            case UI_ACCION_CALLAR: hal_sound_stop(); break; /* (v39) */
            case UI_ACCION_ANIMAL_TOCAR: pet_animal_tell(&s_pet, m.arg, t); break;
            case UI_ACCION_POCHI_CHARLA: pet_pochi_chat_done(&s_pet, m.arg, t); break;
            case UI_ACCION_TOCAR_ANIMAL: ui_toque_resultado(pet_animal_touch(&s_pet, m.arg & 0xFF, (m.arg >> 8) & 0xFF, t)); break; /* (fw 103) */
            case UI_ACCION_TOCAR_FAUNA: ui_toque_resultado(pet_fauna_touch(&s_pet, m.arg & 0xFF, (m.arg >> 8) & 0xFF, t)); break;
            case UI_ACCION_EN_ZONA: pet_in_zone(&s_pet, m.arg != 0); break;
            case UI_ACCION_ZONA_HALLAZGO: pet_zone_find_take(&s_pet, m.arg, t); break;
            case UI_ACCION_JUEGO_LUGAR: pet_place_game_result(&s_pet, m.arg & 7, m.arg >> 5, (m.arg >> 3) & 3, t); break;
            case UI_ACCION_EXCURSION: pet_excursion_back(&s_pet, m.arg, t); break;
            case UI_ACCION_LIBRO_VISTO: pet_book_seen(&s_pet, m.arg); break;
            case UI_ACCION_ZONA_PISTA: pet_zone_hint_done(&s_pet, m.arg); break;
            case UI_ACCION_CANCION: { const char *nm = NULL; int n = 0; const uint8_t *nt = ui_pending_song(&n, &nm); pet_song_save(&s_pet, m.arg, nt, n, nm, t); break; }
            case UI_ACCION_CANCION_USO: if (m.arg & 0x100) pet_song_delete(&s_pet, m.arg & 15); else pet_song_use(&s_pet, (m.arg & 15) == 15 ? -1 : (m.arg & 15), (m.arg >> 4) & 1); break;
            case UI_ACCION_LIBRO_AVISO: pet_book_tutorial_done(&s_pet); break;
            case UI_ACCION_SALTO_CAMA: pet_jump_bed(&s_pet, t); break;
            case UI_ACCION_CRISTALES: pet_crystal_game(&s_pet, m.arg, t); break;
            case UI_ACCION_FIESTA: pet_party_done(&s_pet, m.arg, t); break;
            case UI_ACCION_VIAJE_NO: pet_trip_not_yet(&s_pet, t); break;
            case UI_ACCION_DESPEDIDA_NAT: s_pet.despedida_natural = (uint8_t)(m.arg != 0); break;
            case UI_ACCION_ACTUALIZAR: /* (1.0.09) guarda todo y reinicia en el modo actualizar por Wi-Fi */
                vTaskDelay(pdMS_TO_TICKS(500)); /* (1.0.12) que alcance a dibujarse la pantalla con las instrucciones: queda a la vista */
                pet_seal(&s_pet);
                hal_storage_save(&s_pet);
                guardar_dispositivo();
                hal_log("actualizar: reinicio en el modo Wi-Fi (versión %s)", FW_VERSION);
                hal_log_flush(true);
                ota_pedir_modo();
                break;
            case UI_ACCION_FOTO:
                if (m.arg & 0x10000) pet_photo_legacy(&s_pet, m.arg & 0xFFFF);
                else pet_photo_add(&s_pet, m.arg); /* la más vieja (si había que borrarla) ya la borró la interfaz */
                break;
            case UI_ACCION_FOTO_BORRADA: pet_photo_forget(&s_pet, m.arg); break;
            case UI_ACCION_ARRULLO_DUR: s_dispo.arrullo_dur = (uint8_t)(m.arg < 0 || m.arg > 2 ? 1 : m.arg); guardar_dispositivo(); break;
            case UI_ACCION_ARRULLO: s_dispo.arrullo = (uint8_t)(m.arg < 0 || m.arg > 3 ? 0 : m.arg); guardar_dispositivo(); break;
            case UI_ACCION_ARRULLO_VOL:
                s_dispo.arrullo_vol = (uint8_t)(m.arg & 0xFF);
                pet_device_fix(&s_dispo);
                s_arrullo_prueba_ms = (uint32_t)(esp_timer_get_time() / 1000) + 4000u; /* se oyen los grillos un momento para elegir el volumen */
                if (m.arg & UI_SOLTADO) guardar_dispositivo();
                break;
            case UI_ACCION_MUSICA: s_dispo.musica = (uint8_t)(m.arg != 0); guardar_dispositivo(); break;
            case UI_ACCION_FUENTE: s_dispo.fuente = (uint8_t)(m.arg != 0); guardar_dispositivo(); break;
            case UI_ACCION_GIRAR: s_dispo.girar = (uint8_t)(m.arg != 0); guardar_dispositivo(); break;
            case UI_ACCION_FAMILIA: { /* el huevo de familia pasa a ser la mascota activa; ella queda descansando en perfiles */
                int libre = -1;
                for (int i = 0; i < PET_GUARDADAS && libre < 0; i++) if (!guardada_leer(i, NULL)) libre = i;
                if (!s_pet.huevo_familia) break;
                if ((m.arg & 0xFF) == 1) { /* ella se despide y se va de viaje: el huevo de su familia empieza a ser la mascota */
                    static EXT_RAM_BSS_ATTR pet_t viajera;
                    viajera = s_pet;
                    pet_family_child(&viajera, &s_pet, t, esp_random());
                    pet_add_traveller(&s_pet, &viajera, (m.arg >> 8) > 0 ? (m.arg >> 8) - 1 : 0, t);
                    break;
                }
                if (libre < 0) break; /* la interfaz lo revisa antes */
                static EXT_RAM_BSS_ATTR pet_t madre;
                madre = s_pet;
                pet_family_child(&madre, &s_pet, t, esp_random());
                if (!guardada_escribir(libre, &madre)) s_pet = madre; /* no se pudo guardar: todo queda como estaba */
                break;
            }
            case UI_ACCION_VISITA_PERFIL: {
                static EXT_RAM_BSS_ATTR pet_t otra;
                if (!guardada_leer(m.arg, &otra)) break;
                if (pet_profile_visit(&s_pet, &otra, t)) guardada_escribir(m.arg, &otra);
                break;
            }
            case UI_ACCION_RESPUESTA: pet_phrase_hug(&s_pet, t); break;
            case UI_ACCION_OBJETO: {
                int id = m.arg & 0xFF, op = (m.arg >> 8) & 7;
                if (op == 0) pet_object_use(&s_pet, id, t);
                else if (op == 1) pet_object_return(&s_pet, id, t);
                else if (op == 2) pet_object_unplace(&s_pet, id);
                else if (op == 3) pet_object_head_off(&s_pet);
                else if (op == 5) pet_chest_open(&s_pet, t);          /* el cofre bajo el árbol viejo */
                else if (op == 6) pet_nest_look(&s_pet, id, t);       /* un nido de las gafas del explorador */
                else pet_bag_seen(&s_pet);
                break;
            }
            case UI_ACCION_RITMO: pet_set_rhythm(&s_pet, m.arg); break;
            case UI_ACCION_FINAL: pet_final_done(&s_pet, m.arg, t); break;
            case UI_ACCION_LIBRO: pet_libro_mark(&s_pet, m.arg); break;
            case UI_ACCION_MODO: pet_set_mode_manual(&s_pet, m.arg); break;
            case UI_ACCION_PERFIL_VIVE: {
                int slot = m.arg & 15;
                bool vive = (m.arg >> 4) & 1;
                static EXT_RAM_BSS_ATTR pet_t otra;
                if (slot == 15) pet_set_profile_lives(&s_pet, vive);
                else if (guardada_leer(slot, &otra)) { pet_set_profile_lives(&otra, vive); pet_seal(&otra); guardada_escribir(slot, &otra); }
                break;
            }
            case UI_ACCION_HORA: poner_hora(m.arg); t = ahora(); break;
            case UI_ACCION_RESTAURAR:
                if (m.arg && s_hay_respaldo) {
                    s_pet = s_respaldo;
                    pet_set_timezone(&s_pet, s_zona);
                    hal_storage_save(&s_pet); /* ya: que no se pierda */
                    ultimo_guardado = t;
                    printf("MASCOTA RESTAURADA\n");
                } else printf("MASCOTA RECHAZADA\n");
                s_hay_respaldo = false;
                break;
            case UI_ACCION_HEMISFERIO: s_pet.hemisferio_sur = (uint8_t)(m.arg != 0); break;
            case UI_ACCION_ASPECTO:
                pet_choose_look(&s_pet, m.arg & 15, ((m.arg >> 4) & 15) - 1, (m.arg >> 8) & 15, t, esp_random());
                break;
            case UI_ACCION_AMBIENTE:
                s_dispo.ambiente = (uint8_t)((m.arg & 0xFF) > 100 ? 100 : (m.arg & 0xFF));
                if (m.arg & UI_SOLTADO) guardar_dispositivo();
                break;
            case UI_ACCION_DECOR_VER: pet_toggle_decor(&s_pet, (decor_t)m.arg); break;
            case UI_ACCION_NOMBRE: pet_set_name(&s_pet, ui_pending_name()); break;
            case UI_ACCION_INVITAR: pet_invite(&s_pet, m.arg, t); ultima_baliza = 0; break;
            case UI_ACCION_RESPONDER: pet_answer_invite(&s_pet, m.arg != 0, t); ultima_baliza = 0; break;
            case UI_ACCION_CAMBIAR: {
                static EXT_RAM_BSS_ATTR pet_t otra, vieja;
                if (!guardada_leer(m.arg, &otra)) break;
                pet_seal(&s_pet);
                vieja = s_pet;
                if (!guardada_escribir(m.arg, &vieja)) break; /* la de ahora se queda descansando */
                s_pet = otra;
                pet_resume(&s_pet, &vieja, t); /* toma el mundo (jardín, baúl, clima...) de la que se queda */
                break;
            }
            case UI_ACCION_SOLTAR: {
                char n[16];
                int lugar = m.arg & 0xFF, veh = (m.arg >> 8) - 1;
                snprintf(n, sizeof n, "m%d.bin", lugar);
                if (lugar >= 0 && lugar < PET_GUARDADAS) {
                    static EXT_RAM_BSS_ATTR pet_t viajera;
                    if (guardada_leer(lugar, &viajera)) pet_add_traveller(&s_pet, &viajera, veh < 0 ? 0 : veh, t); /* te escribirá desde su viaje */
                    hal_res_delete(n);
                }
                break;
            }
            case UI_ACCION_NUEVA: {
                if ((m.arg & 0xFF) == 1) { /* se fue de viaje (despedida natural): en su lugar, un huevo nuevo del mismo dueño */
                    { static EXT_RAM_BSS_ATTR pet_t viajera; viajera = s_pet; pet_add_traveller(&s_pet, &viajera, (m.arg >> 8) > 0 ? (m.arg >> 8) - 1 : 0, t); }
                    pet_rebirth(&s_pet, (uint8_t)(esp_random() % pet_species_count()), t, esp_random());
                    break;
                }
                int libre = -1;
                for (int i = 0; i < PET_GUARDADAS && libre < 0; i++) if (!guardada_leer(i, NULL)) libre = i;
                if (libre < 0) break; /* la interfaz no lo deja: primero hay que dejar ir a una */
                if (s_pet.etapa != STAGE_HUEVO || s_pet.nombre[0]) { pet_seal(&s_pet); if (!guardada_escribir(libre, &s_pet)) break; }
            }
                pet_new_fresh(&s_pet, (uint8_t)(esp_random() % pet_species_count()), t, esp_random());
                s_pet.edad_dueno = 0; /* vuelve a preguntar la fecha de nacimiento (quizás es otra persona) */
                s_pet.dueno_dia = s_pet.dueno_mes = 0;
                break;
            case UI_ACCION_ALARMA: pet_alarm_set_packed(&s_pet, m.arg); break;
            case UI_ACCION_PESCA: pet_fish_game(&s_pet, m.arg, t); break; /* v89 */
            case UI_ACCION_ALARMA_RESP: pet_alarm_answer(&s_pet, m.arg & 7, (m.arg & 8) != 0, t); break;
            default: pet_action(&s_pet, (action_t)m.accion, m.arg, t); break;
            }
            hubo_accion = guardar_pronto = true;
        }

        s_sec_us[2] = esp_timer_get_time();
        /* 1b. comandos del editor por USB */
        const char *linea = hal_usb_line();
        if (linea) {
            comando_usb(linea);
            hal_usb_line_done();
        }

        if (s_desc.activo) { /* prueba de descarga: la mascota queda quieta bajo una pantalla fija y el bucle solo mide */
            descarga_paso();
            vTaskDelay(pdMS_TO_TICKS(s_desc.etapa == 2 ? 250 : (s_desc.etapa == 1 ? 200 : 50)));
            continue;
        }

        s_sec_us[3] = esp_timer_get_time();
        /* 2. acelerómetro */
        bool sacudida, movimiento;
        hal_imu_modo_facil(ui_escena_especial() & 32); /* v90: en la pesca, sacudir es fácil */
        hal_imu_poll(&sacudida, &movimiento);
        if (sacudida) s_n_sac++;
        if (movimiento) s_n_mov++;
        /* de bolsillo: sacudir e inclinar solo cuentan con la pantalla encendida (caminando se sacude solo);
         * la pantalla se enciende con el botón o al sacarlo del bolsillo */
        if (sacudida && s_pantalla_on) {
            bool para_la_pesca = false; /* v89: si la pesca pide sacudir, la sacudida es para el minijuego (no la molesta a ella) */
            if (hal_display_lock()) { para_la_pesca = ui_captura_sacudida(); if (para_la_pesca) ui_sacudida(); hal_display_unlock(); }
            if (!para_la_pesca) pet_action(&s_pet, ACT_SACUDIR, 0, t);
            hubo_accion = true;
        }
        if (movimiento && !s_pantalla_on && !s_apagada_boton) despertar_pantalla();
        float incl_x = 0, incl_y = 0;
        if (s_pantalla_on) hal_imu_tilt(&incl_x, &incl_y);
        /* la pantalla se endereza sola: si el reloj queda de lado o al revés un momento, la imagen gira de a 90° para verse recta */
        {
            static int giro; /* 0 derecho, 1 = 90°, 2 = al revés, 3 = 270° */
            static int64_t desde;
            int64_t ms = esp_timer_get_time() / 1000;
            /* el acelerómetro puede estar montado girado respecto de la pantalla: una matriz lo lleva al marco de la pantalla
             * (gy: positivo con el orbe derecho; gx: positivo con su lado derecho hacia abajo). De fábrica es la medida en la placa;
             * «Calibrar giro» (en el probador) la mide con el orbe en la mano y la guarda. */
            const float r0x = incl_x * BOARD_IMU_DERECHO_X, r0y = incl_y * BOARD_IMU_DERECHO_Y;
            const float gx = (float)s_giro_m[0] * r0x + (float)s_giro_m[1] * r0y, gy = (float)s_giro_m[2] * r0x + (float)s_giro_m[3] * r0y;
            s_giro_raw_x = r0x; s_giro_raw_y = r0y;
            incl_x = gx; incl_y = gy; /* la inclinación de la mascota también va en el marco de la pantalla */
            /* el ángulo de la gravedad en el plano de la pantalla: 0° = derecho, +90° = lado derecho abajo, 180° = al revés, -90° = lado izquierdo abajo.
             * Se mide el ángulo (no cada eje) para que también funcione con el orbe inclinado hacia ti, como se sostiene en la mano. */
            int quiere = giro;
            const float plano = sqrtf(gx * gx + gy * gy);
            if (plano > 0.45f) { /* acostado sobre la mesa casi no hay gravedad en el plano: no se toca nada */
                static const float CENTRO[4] = {0.0f, -90.0f, 180.0f, 90.0f}; /* ángulo de cada giro (0 derecho, 1 = 90°, 2 = al revés, 3 = 270°) */
                const float ang = atan2f(gx, gy) * 57.2958f;
                float dif = ang - CENTRO[giro];
                while (dif > 180.0f) dif -= 360.0f;
                while (dif < -180.0f) dif += 360.0f;
                if (fabsf(dif) > 66.0f) { /* se fue claro (más de 66°, un margen de 21° sobre la mitad) de la posición actual: busca la más cercana */
                    float mejor = 999.0f;
                    for (int k = 0; k < 4; k++) {
                        float d = ang - CENTRO[k];
                        while (d > 180.0f) d -= 360.0f;
                        while (d < -180.0f) d += 360.0f;
                        if (fabsf(d) < mejor) { mejor = fabsf(d); quiere = k; }
                    }
                }
            }
            if (!s_dispo.girar || !s_pantalla_on || (ui_escena_especial() & 32)) quiere = giro, desde = 0; /* (v90: pescando, sacudir no gira la pantalla) */
            if (quiere != giro) {
                static int quiere_ant = -1;
                if (quiere != quiere_ant) { desde = 0; quiere_ant = quiere; } /* cambió de idea a medio camino: se empieza a contar de nuevo */
                if (!desde) { desde = ms; ESP_LOGW("giro", "detecta posición %d (ahora %d): x %.2f y %.2f", quiere, giro, gx, gy); }
                if (ms - desde > 900) { /* casi un segundo firme en la nueva posición: así no gira cuando solo la sostienes inclinada */
                    giro = quiere;
                    s_giro_pos = giro;
                    { static const char *const NG[4] = {"derecho", "90 grados", "al reves", "270 grados"}; ESP_LOGW("giro", "pantalla girada: %s (posicion %d)", NG[giro & 3], giro); }
                    desde = 0;
                    if (hal_display_lock()) { hal_display_rotate_q(giro); ui_set_upside_down(giro == 2); hal_display_unlock(); }
                }
            } else desde = 0;
            if (!s_dispo.girar && giro) { giro = 0; if (hal_display_lock()) { hal_display_rotate_q(0); ui_set_upside_down(false); hal_display_unlock(); } }
            /* la inclinación se mide en el marco de la imagen girada */
            { const float x0 = incl_x, y0 = incl_y;
              if (giro == 2) { incl_x = -x0; incl_y = -y0; }
              else if (giro == 3) { incl_x = -y0; incl_y = x0; }
              else if (giro == 1) { incl_x = y0; incl_y = -x0; } }
        }
        ui_set_tilt(incl_x, incl_y); /* solo guarda dos números: no necesita el lock de LVGL */

        s_sec_us[4] = esp_timer_get_time();
        /* 3. amigos: balizas recibidas y la nuestra cada 2 s (o al invitar/responder) */
#if MASCOTA_BLUETOOTH
        uint8_t rx[PET_BALIZA_BYTES];
        pet_baliza_t b;
        while (hal_radio_poll(rx))
            if (pet_baliza_decode(rx, sizeof rx, &b)) pet_on_baliza(&s_pet, &b, t);
        if (t - ultima_baliza >= BALIZA_CADA_S) {
            ultima_baliza = t;
            uint8_t tx[PET_BALIZA_BYTES];
            pet_make_baliza(&s_pet, t, &b);
            pet_baliza_encode(&b, tx);
            hal_radio_set_beacon(tx, sizeof tx);
        }
#else
        (void)ultima_baliza;
#endif

        s_sec_us[5] = esp_timer_get_time();
        /* 4. motor y pantalla, una vez por segundo o tras una acción */
        if ((t != ultimo_tick && (!s_profundo || t - ultimo_tick >= 3u)) || hubo_accion) {
            ultimo_tick = t;
            if (!s_hora_dudosa) pet_update(&s_pet, t); /* sin hora segura, todo queda en pausa */
            { /* (v101) cada cambio de zona queda en el registro (el 13-nov «volvía siempre a la misma» y no había cómo verlo) */
                static int esc_log = -1;
                if ((int)s_pet.escena != esc_log) {
                    if (esc_log >= 0) hal_log("zona: %s -> %s (la tocaste hace %u s, clima %d, sensación %d)", pet_scene_name((escena_t)esc_log), pet_scene_name((escena_t)s_pet.escena),
                                              (unsigned)(t - s_pet.ultima_interaccion), (int)pet_weather(&s_pet), pet_temperature_feel(&s_pet, t));
                    esc_log = (int)s_pet.escena;
                }
            }
            bool avisar = false;
            const bool dormir_ahora = pet_is_bedtime(&s_pet, t);
            s_p4[0] = s_p4[1] = s_p4[2] = s_p4[3] = s_p4[4] = esp_timer_get_time();
            if (hal_display_lock()) {
                s_p4[1] = esp_timer_get_time();
                pet_event_t e;
                while (pet_poll_event(&s_pet, &e)) {
                    if (e.type == EV_SONIDO) { if (!dormir_ahora) hal_sound_play((sound_t)e.a); } /* a la hora de dormir, silencio */
                    else ui_show_event(&s_pet, &e);
                    if (e.type == EV_ANIMAL && (e.b == ANI_LLEGA || e.b == ANI_SE_VA)) hal_log("animal: %s %s", pet_animal_name((animal_t)e.a), e.b == ANI_LLEGA ? "llega" : "se va");
                    if (e.type == EV_SONIDO && dormir_ahora) hal_log("sonido pedido pero es hora de dormir: %s (queda en silencio)", pet_sound_name((sound_t)e.a));
                    switch (e.type) {
                    case EV_PIDE: case EV_ENFERMO: case EV_VISITA_LLEGA: case EV_NACIO: case EV_HALLAZGO:
                    case EV_FRASE: case EV_AMIGO_CERCA: case EV_INVITACION: case EV_AMIGO_VISITA: case EV_TRUENO:
                    case EV_ESCENA_NUEVA: case EV_CUMPLE: case EV_ALARMA: case EV_HORA_DORMIR:
                        avisar = true;
                        break;
                    case EV_VECINO: /* llegó su amigo del barrio */
                        if (e.a == VEC_LLEGA) avisar = true;
                        break;
                    case EV_FAUNA: /* el zorro viene por la fruta: se avisa para que alcances a espantarlo */
                        if (e.a == FAUNA_ZORRO && e.b == FSUC_LLEGA) avisar = true;
                        break;
                    default: break;
                    }
                }
                s_p4[2] = esp_timer_get_time();
                if (t - ultima_bateria >= 3) { /* cada 3 s: al enchufar el cargador el icono cambia enseguida */
                    ultima_bateria = t;
                    { /* protección de calor: cada 15 s mira las temperaturas; muy caliente = brillo mínimo y avisa, hasta que baje */
                        static uint32_t t_calor;
                        if (t - t_calor >= 15) {
                            t_calor = t;
                            const int tc = hal_temp_chip(), ti = hal_temp_imu();
                            const int mx = tc > ti ? tc : ti;
                            if (!s_calor && mx >= 750) { s_calor = true; hal_display_brightness(15); hal_log("CALOR: procesador %d.%d °C, acelerómetro %d.%d °C: brillo al mínimo", tc / 10, abs(tc % 10), ti / 10, abs(ti % 10)); }
                            else if (s_calor && mx < 650) { s_calor = false; s_brillo = -1; hal_log("CALOR: ya bajó (%d.%d °C)", mx / 10, abs(mx % 10)); }
                        }
                    }
                    int pct = hal_battery_percent();
                    bool carga = hal_battery_charging();
                    s_cargando = carga;
                    s_vbus = hal_battery_vbus();
                    ui_set_battery(pct, carga);
                    { /* anota cada vez que el cable USB (cargador) se va o vuelve: la prueba de carga lo lee después */
                        static bool vb_ant, vb_listo;
                        const bool vb = hal_battery_vbus();
                        if (vb_listo && vb != vb_ant && s_cev_n < 8) {
                            s_cev[s_cev_n].ms = (uint32_t)(esp_timer_get_time() / 1000); s_cev[s_cev_n].mv = (int16_t)hal_battery_mv();
                            s_cev[s_cev_n].pct = (int8_t)pct; s_cev[s_cev_n].vbus = vb ? 1 : 0; s_cev_n++;
                        }
                        vb_ant = vb; vb_listo = true;
                    }
                    { /* avisos que se ven sin tocar nada: al enchufar, y cuando la carga TERMINA (el cargador lo dice; el medidor puede estar descalibrado) */
                        static bool vbus_ant, lleno_ant, visto;
                        const bool vbus = hal_battery_vbus(), lleno = hal_battery_full();
                        if (visto && vbus && !vbus_ant && !lleno && pct >= 0) {
                            if (!s_apagada_boton) lv_display_trigger_activity(s_disp);
                            ui_battery_aviso(pct, 0, 4000);
                        }
                        if (visto && lleno && !lleno_ant) {
                            if (!s_apagada_boton) lv_display_trigger_activity(s_disp); /* enciende la pantalla aunque esté apagada */
                            ui_battery_aviso(100, 1, 12000);
                            if (!dormir_ahora && !s_pet.dormida) hal_sound_play(SND_ALEGRE);
                            hal_log("bateria: CARGA COMPLETA (el cargador terminó), %d mV", hal_battery_mv());
                        }
                        if (!vbus) lleno_ant = false; else if (lleno) lleno_ant = true;
                        vbus_ant = vbus; visto = true;
                    }
                    /* casi sin batería: guardar ya, por si se apaga */
                    if (pct >= 0 && pct <= 4 && !carga && !bateria_guardada) { guardar_pronto = true; ultimo_guardado = 0; bateria_guardada = true; }
                    if (pct > 10 || carga) bateria_guardada = false;
                    /* y con muy poca: modo reserva, para que el reloj no pierda la hora (dos lecturas seguidas) */
                    {
                        const int mv_b = hal_battery_mv();
                        const bool baja = (pct >= 0 && pct <= RESERVA_ENTRAR_PCT) || (mv_b > 2500 && mv_b < RESERVA_ENTRAR_MV);
                        lecturas_bajas = (baja && !carga && !s_vbus) ? lecturas_bajas + 1 : 0;
                        if (lecturas_bajas == RESERVA_LECTURAS) hal_log("bateria: casi vacía (%d%%, %d mV): modo reserva", pct, mv_b);
                    }
                    bool alarma_cerca = false; /* si una alarma está por sonar, primero que suene */
                    for (int i = 0; i < PET_ALARMAS; i++) {
                        uint32_t x = pet_alarm_next(&s_pet, i, t);
                        if (x && x - t < 180) alarma_cerca = true;
                    }
                    if (lecturas_bajas >= RESERVA_LECTURAS && !reserva_desde && !ui_alarm_ringing() && !alarma_cerca) {
                        reserva_desde = t;
                        ui_show_reserve();
                        lv_display_trigger_activity(s_disp);
                    }
                }
                s_p4[3] = esp_timer_get_time();
                ui_update(&s_pet, t);
                s_p4[4] = esp_timer_get_time();
                s_quiere_mic = s_pantalla_on && ui_wants_mic(); /* (v101) para el paso 5c, sin pedir otra vez el candado */
                /* (v101) en la pecera los avisos ya se ven (la pantalla está encendida): no reinician su tiempo; antes cada frase o pedido
                 * de ella volvía a contar los 30 min desde cero y la pecera no se apagaba nunca */
                if ((avisar && !(ui_aquarium() && s_pantalla_on)) || ui_alarm_ringing()) lv_display_trigger_activity(s_disp); /* la alarma mantiene la pantalla encendida */
                uint32_t inactivo = lv_display_get_inactive_time(s_disp);
                bool pecera = ui_aquarium() && !ui_alarm_ringing();
                if (pecera && inactivo >= pet_device_aquarium_ms(&s_dispo)) { /* (v101) pasó el tiempo elegido (también con el cargador): sale de la pecera y se apaga */
                    ui_pecera_vencida();
                    pecera = false;
                    hal_log("pecera: pasaron %u min sin tocarla (%s): se apaga", (unsigned)(inactivo / 60000u), s_cargando ? "cargando" : "con batería");
                }
                /* (v99, batería) nadie la toca hace 8 s y no hay nada que leer ni jugar: se atenúa y dibuja a la mitad de cuadros */
                const bool atenuar = s_pantalla_on && !s_cargando && inactivo > BOARD_ATENUAR_MS && ui_reposo_visual();
                {
                    lv_timer_t *rt = lv_display_get_refr_timer(s_disp);
                    if (rt) lv_timer_set_period(rt, atenuar || (pecera && !s_cargando) ? BOARD_REFRESCO_REPOSO_MS : BOARD_REFRESCO_MS); /* la pecera con batería: mitad de cuadros, para que dure */
                }
                s_atenuada = atenuar;
                hal_display_unlock();
                /* las alarmas "suena siempre" de las mascotas que descansan (cada 30 s se miran sus horas).
                 * v88: se leen sus archivos SIN el candado de la pantalla (antes la interfaz quedaba quieta mientras se leía la flash) */
                {
                    static uint32_t revisadas;
                    if (!revisadas) revisadas = t;
                    if (t - revisadas >= 30 && !ui_alarm_ringing()) {
                        static EXT_RAM_BSS_ATTR pet_t otra;
                        for (int i = 0; i < PET_GUARDADAS; i++) {
                            if (!guardada_leer(i, &otra)) continue;
                            int a = pet_alarm_resting(&otra, revisadas, t);
                            if (a >= 0) { guardada_escribir(i, &otra); if (hal_display_lock()) { ui_alarm_other(&otra, a); hal_display_unlock(); } break; }
                        }
                        revisadas = t;
                    }
                }
                /* en modo pecera la pantalla sigue encendida el tiempo elegido en Pantalla (10/30 min, 1 h o siempre) y más tenue */
                /* cargando, espera un poco más: al minuto sin tocarlo pasa sola a modo pecera */
                uint32_t limite = pecera ? pet_device_aquarium_ms(&s_dispo) : /* (v101) el tiempo elegido vale también cargando («no se apaga» = siempre) */ /* (v99) lo que dure la pecera se elige en Pantalla */ (s_cargando ? 65000u : pet_device_screen_ms(&s_dispo));
                int hl = (int)(((int64_t)t + s_pet.zona_horaria) % 86400 / 3600);
                bool alarma = ui_alarm_ringing();
                bool noche = !alarma && (dormir_ahora || s_pet.dormida || (pecera && (hl >= 22 || hl < 7))); /* dormida (también de día): brillo bajo */
                s_inactivo_ms = inactivo;
                if (alarma) s_apagada_boton = false; /* la alarma siempre enciende */
                {
                    int br = pet_device_brightness(&s_dispo, pecera, noche);
                    if (atenuar && !pecera && !alarma && !ui_battery_aviso_activo() && limite > BOARD_ATENUAR_MS + 2000u) { /* (v99) un toque la devuelve al brillo de siempre */
                        const int bajo = br * BOARD_ATENUAR_PCT / 100;
                        br = bajo < 8 ? (br < 8 ? br : 8) : bajo;
                    }
                    encender_pantalla((inactivo < limite || ui_battery_aviso_activo()) && !s_apagada_boton, br);
                }
                /* ella, efectos y alarmas tienen su volumen: la alarma suena aunque las barras estén en 0 */
                int vol = (s_dispo.volumen * 1000 + s_dispo.efectos) * 1000 + s_dispo.alarmas;
                if (vol != vol_actual) { vol_actual = vol; hal_sound_gains(s_dispo.volumen, s_dispo.efectos, pet_device_alarm_volume(&s_dispo)); }
                /* sonido ambiente: solo con la pantalla encendida, y no a la hora de dormir */
                int amb = (int)AMB_NADA; /* EN PAUSA: el ruido de fondo de cada zona (parecía oleaje) queda apagado hasta tener algo mejor; el arrullo de abajo sigue siendo opcional */
                (void)pet_ambient; /* antes: s_pantalla_on && s_pet.edad_dueno && !s_hora_dudosa && !dormir_ahora ? pet_ambient(&s_pet, t) : AMB_NADA */
                if (ui_silencio_ambiente()) amb = (int)AMB_NADA; /* durante el juego de los cristales solo suenan las notas */
                int vol_amb = s_dispo.ambiente;
                /* modo arrullo: mientras ella duerme (la primera hora desde que se durmió) suenan los grillos, aunque la pantalla esté apagada */
                {
                    static bool dormida_antes;
                    static uint32_t arrullo_hasta;
                    if (s_pet.dormida && !dormida_antes && s_dispo.arrullo && !ui_alarm_ringing()) {
                        arrullo_hasta = (uint32_t)(esp_timer_get_time() / 1000) + (s_dispo.arrullo_dur == 0 ? 1800u : s_dispo.arrullo_dur == 2 ? 7200u : 3600u) * 1000u; /* se cuenta con el reloj de la placa, no con la hora del juego: con la hora dudosa (el RTC no responde) también suenan */
                        hal_sound_play_soft(SND_A_DORMIR); /* una canción de cuna corta al empezar (con el volumen de efectos: si están en silencio, no suena) */
                    }
                    dormida_antes = s_pet.dormida;
                    if (s_dispo.arrullo && s_pet.dormida && (int32_t)(arrullo_hasta - (uint32_t)(esp_timer_get_time() / 1000)) > 0 && !ui_alarm_ringing()) {
                        amb = AMB_NOCHE; /* el canto de los grillos, con su propio volumen */
                        vol_amb = s_dispo.arrullo_vol;
                    } else if ((int32_t)(s_arrullo_prueba_ms - (uint32_t)(esp_timer_get_time() / 1000)) > 0) { /* probando el volumen en los ajustes */
                        amb = AMB_NOCHE;
                        vol_amb = s_dispo.arrullo_vol;
                    }
                }
                if (s_prueba_amb >= 0) { amb = s_prueba_amb; vol_amb = s_dispo.ambiente ? s_dispo.ambiente : 60; } /* (pruebas) */
                if (amb != amb_actual || vol_amb != amb_vol) {
                    hal_log("ambiente: %s -> %s (volumen %d, %s)", amb_actual == (int)AMB_NADA ? "nada" : "suena", amb == (int)AMB_NADA ? "nada" : "coro/ambiente", vol_amb, s_pet.dormida ? "ella duerme" : "despierta");
                    amb_actual = amb;
                    amb_vol = vol_amb;
                    hal_ambient_set((ambiente_t)amb, amb_vol);
                }
                { /* dormida con la pantalla encendida y sin que nadie la toque hace 8 s: la animación es casi quieta, el procesador baja a 80 MHz; un toque lo sube */
                    s_amb_dbg = amb;
                    s_energia_suave = s_pantalla_on && (s_pet.dormida || s_hora_dudosa || s_atenuada) && !s_cargando && amb == (int)AMB_NADA && !ui_alarm_ringing() && (ui_panel_actual() == 0 || (s_hora_dudosa && ui_panel_actual() == 14)) && s_inactivo_ms > 8000u; /* (con la hora perdida el juego está en pausa y la pregunta de la hora queda abierta: no debe gastar batería a 240 MHz) */
                    /* (v101) el arrullo con la pantalla apagada ya no sube el procesador a 240 MHz (sonar grillos no lo necesita; el 13-nov
                     * su siesta lo tuvo a 240 MHz). Si en la placa suena cortado, poner BOARD_ARRULLO_80MHZ en 0 */
                    const bool amb_rapido = amb != (int)AMB_NADA && (s_pantalla_on || !BOARD_ARRULLO_80MHZ);
                    const bool rapido = (s_pantalla_on && !s_energia_suave) || s_cargando || amb_rapido || ui_alarm_ringing();
                    static int ant = -1;
                    if (ant != (rapido ? 1 : 0) + (s_energia_suave ? 2 : 0)) {
                        ant = (rapido ? 1 : 0) + (s_energia_suave ? 2 : 0);
                        /* v88: y POR QUÉ va rápido (para la batería: con la pantalla apagada debería ir a 80 MHz) */
                        hal_log("energia: CPU %s (pantalla %s, dormida %d, sin toques %u s) | motivo: %s%s%s%s", rapido ? "240 MHz" : "80 MHz", s_pantalla_on ? "on" : "off", s_pet.dormida ? 1 : 0, (unsigned)(s_inactivo_ms / 1000),
                                (s_pantalla_on && !s_energia_suave) ? "pantalla encendida " : "", s_cargando ? "cargando " : "", amb != (int)AMB_NADA ? "sonido ambiente/arrullo " : "", ui_alarm_ringing() ? "alarma " : "");
                    }
                    cpu_rapido(rapido);
                }
            }
        }

        s_sec_us[6] = esp_timer_get_time();
        /* 5. botón PWR (el que no es BOOT, queda cómodo al costado): enciende o apaga la pantalla */
        bool boton = hal_pwr_short_press();
        if (boton) {
            static uint32_t pwr_antes_ms;
            const uint32_t ahora_ms = (uint32_t)(esp_timer_get_time() / 1000);
            const bool doble = pwr_antes_ms && ahora_ms - pwr_antes_ms < 800;
            pwr_antes_ms = doble ? 0 : ahora_ms;
            if (doble) { /* dos veces seguidas: la pantalla queda encendida y entra al modo pecera */
                s_n_pwr2++;
                s_apagada_boton = false;
                despertar_pantalla();
                if (hal_display_lock()) { ui_pecera(); hal_display_unlock(); }
            } else {
                s_n_pwr1++;
                s_apagada_boton = s_pantalla_on && !s_apagada_boton;
                if (!s_apagada_boton) despertar_pantalla();
            }
        }

        s_sec_us[7] = esp_timer_get_time();
        /* 5c. soplar: el micrófono escucha solo cuando la interfaz espera un soplido (niebla, velas, un objeto) */
        {
            static uint32_t soplo_desde, soplo_pausa;
            uint32_t ms = (uint32_t)(esp_timer_get_time() / 1000);
            bool quiere = false;
            /* (v101) antes se pedía el candado de la pantalla en CADA vuelta solo para preguntar esto, y esperaba que terminara de
             * dibujar (100-260 ms en el registro del 13-nov). Ahora se pregunta una vez por segundo en el paso 4 (que ya tiene el
             * candado) y aquí solo mientras de verdad espera un soplido */
            if (s_pantalla_on && s_quiere_mic && hal_display_lock()) { quiere = s_quiere_mic = ui_wants_mic(); hal_display_unlock(); }
            /* música: si lo activaste, escucha con la pantalla prendida y ella despierta (no graba: solo mide el volumen) */
            static uint32_t mus_ini, mus_n, mus_alto, mus_golpes, mus_ult_golpe;
            static float mus_suma, mus_cuad, mus_antes, mus_media;
            s_pet.escucha_musica = s_dispo.musica; /* así ella sabe si puede pedirte su canción */
            bool musica = s_dispo.musica && s_pantalla_on && !s_pet.dormida && s_pet.etapa != STAGE_HUEVO;
            bool tanda = musica && (ms % 20000u) < 6000u; /* en tandas: 6 s de cada 20, para gastar poca batería */
            hal_mic_enable(quiere || tanda);
            int nivel = (quiere || tanda) ? hal_mic_level() : 0;
            { /* v90: la interfaz ve lo fuerte que suena (el soplido se ve) y sabe si el micrófono funciona */
                static bool mic_ok_ant = true;
                const bool ok = hal_mic_ok();
                if (quiere || ok != mic_ok_ant) if (hal_display_lock()) { ui_mic_nivel(quiere ? nivel : 0); ui_set_mic_ok(ok); hal_display_unlock(); }
                mic_ok_ant = ok;
            }
            if (tanda && !quiere && nivel >= 0) {
                if (!mus_ini) { mus_ini = ms; mus_media = (float)nivel; mus_antes = (float)nivel; mus_golpes = 0; mus_ult_golpe = 0; }
                mus_n++; mus_suma += (float)nivel; mus_cuad += (float)nivel * (float)nivel;
                if (nivel >= 10) mus_alto++;
                /* los golpes del compás: el volumen sube de golpe sobre su promedio móvil (con 250 ms de pausa mínima) */
                mus_media += ((float)nivel - mus_media) * 0.05f;
                if ((float)nivel > mus_media * 1.35f + 3.0f && mus_antes <= mus_media * 1.35f + 3.0f && ms - mus_ult_golpe > 250u) { mus_golpes++; mus_ult_golpe = ms; }
                mus_antes = (float)nivel;
                if (ms - mus_ini >= 5500) { /* al final de cada tanda: ¿sonó seguido y con altibajos (no un zumbido parejo ni un soplido)? */
                    float media = mus_suma / (float)mus_n;
                    float var = mus_cuad / (float)mus_n - media * media;
                    if (mus_n > 20 && mus_alto * 10 >= mus_n * 6 && media < 60.0f && var > 6.0f) {
                        const float bpm = (float)mus_golpes * 60000.0f / (float)(ms - mus_ini); /* golpes por minuto */
                        pet_music_heard(&s_pet, ahora(), bpm < 70.0f ? 0 : bpm < 115.0f ? 1 : 2, (int)(bpm + 0.5f), media < 20.0f ? 0 : media < 35.0f ? 1 : media < 50.0f ? 2 : 3);
                    }
                    mus_ini = 0; mus_n = mus_alto = 0; mus_suma = mus_cuad = 0;
                }
            } else mus_ini = 0, mus_n = mus_alto = 0, mus_suma = mus_cuad = 0;
            /* en el bote soplar tiene que ser fácil: umbral bajo, poco tiempo seguido y se puede repetir enseguida */
            const bool bote = quiere && ui_escena_especial() & (2 | 32); /* (el cruce en bote y la pesca, v89) */
            const int umbral = bote ? 26 : 40;
            const uint32_t seguido = bote ? 160u : 250u, pausa = bote ? 700u : 1500u;
            static uint32_t soplo_bajo;
            if (quiere && nivel >= umbral) { /* un soplido: fuerte y seguido un rato */
                soplo_bajo = 0;
                if (!soplo_desde) soplo_desde = ms;
                else if (ms - soplo_desde > seguido && (int32_t)(ms - soplo_pausa) > 0) {
                    if (hal_display_lock()) { ui_mic_blow(); hal_display_unlock(); }
                    soplo_pausa = ms + pausa;
                    soplo_desde = 0;
                }
            } else if (soplo_desde) { /* un bajón corto no corta el soplido (el aire no suena parejo) */
                if (!soplo_bajo) soplo_bajo = ms;
                if (ms - soplo_bajo > 120) soplo_desde = soplo_bajo = 0;
            }
        }

        s_sec_us[8] = esp_timer_get_time();
        /* 5a. pasos: cada minuto le cuenta los que diste (si caminas, ella se entera) */
        if (t - ultimo_paso >= 60) {
            ultimo_paso = t;
            uint32_t pasos = hal_steps_read();
            const uint32_t d_min = pasos_antes && pasos >= pasos_antes ? pasos - pasos_antes : 0;
            if (d_min) pet_steps_add(&s_pet, d_min, t);
            pasos_antes = pasos ? pasos : pasos_antes;
            /* (fw 104) paseo de verdad: minutos seguidos caminando (≥ 40 pasos por minuto) con la pantalla casi siempre apagada (en el
             * bolsillo o en la mano). Se permiten 2 minutos de pausa (un semáforo); con 3 sin caminar, o al encender la pantalla, el
             * paseo termina y, si fueron 8 minutos o más y 800 pasos o más, cuenta como una excursión juntos. Agitarlo no sirve: el
             * chip cuenta pasos con ritmo, y hacen falta varios minutos seguidos. */
            static uint32_t paseo_pasos, paseo_min, paseo_pausa, paseo_pant;
            const bool caminando = d_min >= 40u && d_min < 260u;
            if (caminando) { paseo_pasos += d_min; paseo_min++; paseo_pausa = 0; if (s_pantalla_on) paseo_pant++; }
            else if (paseo_min) paseo_pausa++;
            const bool termina = paseo_min && (paseo_pausa >= 3u || (s_pantalla_on && paseo_pausa >= 1u));
            if (termina) {
                const bool vale = paseo_min >= 8u && paseo_pasos >= 800u && paseo_pant * 2u <= paseo_min; /* (mirando la pantalla todo el rato no es un paseo con ella en el bolsillo) */
                const int r = vale ? pet_walk_together(&s_pet, paseo_pasos, t) : 0;
                hal_log("paseo: %u pasos en %u min (pantalla %u min) -> %s", (unsigned)paseo_pasos, (unsigned)paseo_min, (unsigned)paseo_pant,
                        r == 2 ? "contó y trajo algo" : (r == 1 ? "contó" : (vale ? "ya van 2 hoy (no cuenta)" : "muy corto")));
                /* (si terminó porque encendiste la pantalla, lo ve en vivo; si no, sale en el resumen de lo que pasó) */
                paseo_pasos = paseo_min = paseo_pausa = paseo_pant = 0;
            }
        }

        { /* con la pantalla apagada y dormida, una vez por hora: limpia la flash de las fotos (si no, guardar una foto puede congelar la animación 2 s) */
            static uint32_t ult_limpia;
            if (!s_pantalla_on && s_pet.dormida && t - ult_limpia >= 3600u) { ult_limpia = t; hal_fotos_mantener(); }
        }

        s_sec_us[9] = esp_timer_get_time();
        /* 5b. modo reserva: guarda todo y apaga lo que puede; despierta para la próxima alarma o al cargar */
        if (reserva_desde && t - reserva_desde >= 5) {
            if (hal_battery_charging()) reserva_desde = 0; /* lo enchufaste justo */
            else {
                pet_seal(&s_pet);
                hal_storage_save(&s_pet);
                uint32_t prox = 0;
                for (int i = 0; i < PET_ALARMAS; i++) {
                    uint32_t x = pet_alarm_next(&s_pet, i, t);
                    if (x && (!prox || x < prox)) prox = x;
                }
                ESP_LOGW(TAG, "casi sin batería: modo reserva");
                ota_arranque_ok(); /* (1.0.09) despertar es un arranque nuevo: que no cuente como versión fallida */
                hal_reserve_enter(prox);
            }
        }

        s_sec_us[10] = esp_timer_get_time();
        /* 6. guardar */
        /* v89: escribir en la flash congela todo (los dos núcleos) mientras dura, y en la placa tarda entre 0,7 y 2,6 s:
         * antes se guardaba 10 s después de CADA acción (en un juego, cada 10 s un tirón). Ahora, tras una acción, se guarda
         * al apagarse la pantalla, o a los 90 s si sigue encendida; y siempre cada 5 min. */
        static bool pantalla_antes = true;
        const bool se_apago = pantalla_antes && !s_pantalla_on;
        pantalla_antes = s_pantalla_on;
        if ((guardar_pronto && (se_apago || !s_pantalla_on || t - ultimo_guardado >= 90 || ultimo_guardado == 0)) || t - ultimo_guardado >= GUARDAR_CADA_S) {
            hal_storage_save_async(&s_pet); /* v88: lo escribe otra tarea (antes el ciclo principal esperaba la flash) */
            ultimo_guardado = t;
            guardar_pronto = false;
        }

        s_sec_us[11] = esp_timer_get_time();
        /* con la pantalla apagada, sin cargar y sin alarma: la vuelta del juego baja de 20 a ~7 por segundo (gasta menos batería) */
        probar_espera_paso();
        s_profundo = !s_pantalla_on && s_pet.dormida && !s_cargando && !ui_alarm_ringing() && ui_panel_actual() == 0 && s_prueba_amb < 0 && !s_hora_dudosa;
        { /* (v100) sueño ligero: pantalla apagada hace 5 s, sin cable, sin sonido, sin alarma ni pruebas, y ya pasó el arranque */
            static uint32_t apagada_ms;
            const uint32_t ms_ya = (uint32_t)(esp_timer_get_time() / 1000);
            if (s_pantalla_on) apagada_ms = ms_ya;
            if (s_dormitando) s_vbus = hal_battery_vbus(); /* dormitando, el cable se mira en cada vuelta (el editor lo ve enseguida) */
            const bool puede = !s_pantalla_on && ms_ya - apagada_ms > 5000u && ms_ya > 60000u && !s_cargando && !s_vbus && !ui_alarm_ringing() &&
                               !hal_sound_open() && s_prueba_amb < 0 && !reserva_desde && !guardar_pronto;
            dormitar(puede);
#if BOARD_SUENO_NOCHE
            /* (v101, batería etapa 4) de noche: ella duerme, es su hora de dormir y la pantalla lleva 10 min apagada, sin cable ni
             * sonido: se guarda todo y la placa entra en sueño PROFUNDO hasta que termine la noche (o 1 min antes de una alarma).
             * Despierta con BOOT al instante, con PWR o al enchufar en unos segundos (se revisa cada BOARD_NOCHE_REVISAR_S).
             * Al despertar arranca de nuevo (1-2 s) y el motor avanza las horas que pasaron (ella no se congela: prueba P0). */
            static uint32_t noche_mira;
            if (puede && s_pet.dormida && !s_hora_dudosa && ms_ya - apagada_ms > BOARD_NOCHE_TRAS_MS && ms_ya > 180000u && !s_desc.activo &&
                ui_panel_actual() == 0 && t - noche_mira >= 30u && pet_is_bedtime(&s_pet, t)) {
                noche_mira = t;
                uint32_t hasta = 0;
                for (uint32_t k = 600u; k <= 14u * 3600u; k += 600u) if (!pet_is_bedtime(&s_pet, t + k)) { hasta = t + k; break; } /* fin de la noche */
                if (!hasta) hasta = t + 8u * 3600u;
                uint32_t alarma = 0; /* sus alarmas y las de las mascotas que descansan */
                for (int i = 0; i < PET_ALARMAS; i++) { uint32_t x = pet_alarm_next(&s_pet, i, t); if (x && (!alarma || x < alarma)) alarma = x; }
                {
                    static EXT_RAM_BSS_ATTR pet_t otra;
                    for (int g = 0; g < PET_GUARDADAS; g++) {
                        if (!guardada_leer(g, &otra)) continue;
                        for (int i = 0; i < PET_ALARMAS; i++) { uint32_t x = pet_alarm_next(&otra, i, t); if (x && (!alarma || x < alarma)) alarma = x; }
                    }
                }
                { const uint32_t tl = pet_timer_left(&s_pet, t); if (tl && (!alarma || t + tl < alarma)) alarma = t + tl; } /* y el temporizador */
                if (alarma && alarma < hasta + 60u) hasta = alarma > t + 60u ? alarma - 60u : t;
                if (hasta > t + 300u) { /* vale la pena (si la alarma está muy cerca, mejor esperarla despierta) */
                    hal_log("energia: SUEÑO PROFUNDO de noche hasta dentro de %u min (batería %d%%, %d mV)", (unsigned)((hasta - t) / 60u), hal_battery_percent(), hal_battery_mv());
                    hal_storage_save(&s_pet);
                    hal_log_flush(true);
                    ota_arranque_ok(); /* (1.0.09) */
                    hal_night_enter(hasta); /* no vuelve: al despertar la placa arranca de nuevo */
                }
            }
#endif
        }
        s_espera_ms = (s_profundo || s_dormitando) ? 250u : (!s_pantalla_on && !s_cargando && !ui_alarm_ringing() ? 140u : (s_energia_suave ? 90u : 50u));
        vTaskDelay(pdMS_TO_TICKS(s_espera_ms));
    }
}
