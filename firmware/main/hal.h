/* Capa de hardware: lo único que main necesita saber del reloj. */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "lvgl.h"
#include "pet.h"

/* pantalla + táctil + LVGL (vía BSP) */
lv_display_t *hal_display_init(void);
lv_display_t *hal_display_init_liviano(void); /* (1.0.13) con búferes chicos: el modo actualizar, para que alcance el Wi-Fi */
void hal_vel_info(int *cuadros, int *prom_ms, int *max_ms); /* última ventana de 3 s: cuadros, ms promedio y ms máximo */
void hal_main_tick(void); /* la tarea principal da una vuelta (diagnóstico) */
void hal_log(const char *fmt, ...) __attribute__((format(printf, 1, 2))); /* anota en el registro que queda guardado en el dispositivo */
void hal_log_init(void);   /* motivo del último reinicio + captura de avisos y errores */
void hal_log_flush(bool forzar); /* pasa lo pendiente al archivo (cada ~20 s si no se fuerza) */
int hal_log_read(char *buf, int cap); /* el archivo + lo pendiente; devuelve los bytes */
void hal_log_clear(void);
void hal_imu_modo_facil(bool si); /* v90: sacudida fácil (la pesca) */
bool hal_mic_ok(void);            /* v90: false si el micrófono no se pudo abrir (falta memoria) */
void hal_log_flush_pantalla(bool encendida); /* v89: con la pantalla encendida el registro se escribe cada 5 min */
void hal_vel_info2(int *cuadros, int *prom_ms, int *max_ms, int *kpx, int *tick_ms, int *tick_max); /* v89 */
/* v88: resumen aparte de eventos raros (reinicios y su motivo, fallos, memoria baja, pantallas trabadas, fondos que no cargan).
 * Va a su propio lugar (1.0.64: la zona de eventos de la partición «registro», 24 KB; antes eventos.txt), no lo borra el
 * desborde del registro normal y sobrevive a un reinicio por fallo. */
void hal_evento(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void hal_ram_log(const char *donde); /* anota cuánta RAM interna queda */
const char *hal_ram_fases(void); /* v88: PSRAM libre al comienzo de cada fase del arranque */
void hal_display_brightness(int porcentaje); /* 0 = apagada */
void hal_display_sleep(bool dormir); /* (v99) reposo de verdad del panel y del táctil (BOARD_PANEL_REPOSO) */
bool hal_display_pause(bool pausar); /* (v100) detiene la tarea de LVGL y su reloj (pantalla apagada) para que el procesador pueda dormir; false la reanuda */
bool hal_sound_open(void);            /* (v100) el parlante está abierto (reloj de audio andando) */
bool hal_display_lock(void);
void hal_display_unlock(void);

/* reloj de tiempo real PCF85063 */
esp_err_t hal_rtc_init(void);
uint32_t hal_rtc_now(void); /* segundos Unix (UTC) */
esp_err_t hal_rtc_set(uint32_t unix_s);
void hal_rtc_trusted(bool si); /* v88: la hora es segura: sin chip de reloj se guarda en la memoria cada 2 min */

/* batería (AXP2101) */
esp_err_t hal_power_init(void);
int hal_battery_mv(void); /* mV, -1 si no se lee */
int hal_battery_percent(void); /* -1 sin batería */
bool hal_battery_charging(void);
int hal_temp_chip(void); /* temperatura del procesador en décimas de °C, -999 si no se lee */
int hal_temp_imu(void);  /* temperatura del acelerómetro (cerca de la batería y la pantalla) en décimas de °C, -999 si no se lee */
bool hal_battery_vbus(void);       /* hay cable con corriente (aunque ya esté llena) */
int hal_battery_stage(void);       /* fase del cargador (4 = terminó) */
bool hal_battery_full(void);       /* enchufada y con la carga terminada */
int hal_battery_gauge_raw(void);   /* el medidor interno, sin correcciones */

/* Modo reserva: con la batería casi vacía se apaga todo (pantalla, sonido, radio, acelerómetro) y el
 * ESP32 duerme profundo; el reloj de tiempo real sigue contando con lo poco que queda. Despierta cada
 * 20 minutos a ver si lo enchufaron, un poco antes de la próxima alarma (alarma_utc, 0 = ninguna) y con
 * el botón BOOT. No vuelve. */
void hal_reserve_enter(uint32_t alarma_utc);
/* Al arrancar: si venía de la reserva y todavía no lo cargan (ni toca una alarma), vuelve a dormir sin
 * encender nada. Llamar primero en app_main. */
void hal_reserve_boot_check(void);
/* (v101) sueño profundo de noche: guarda nada (hay que guardar antes) y duerme hasta hasta_utc; despierta con BOOT, PWR o el cable.
 * hal_night_boot_check() va al comienzo del arranque (si no hay motivo para despertar, vuelve a dormir ahí mismo).
 * hal_night_wake_reason(): por qué despertó de la noche (NULL si este arranque no viene de ahí). */
void hal_night_enter(uint32_t hasta_utc);
void hal_night_boot_check(void);
const char *hal_night_wake_reason(void);
/* (1.0.54) modo paseo: sueño profundo caminando, con el contador de pasos del chip encendido (ver services.c) */
void hal_walk_enter(uint32_t alarma_utc, uint32_t cuenta_chip);
void hal_walk_boot_check(void);
int hal_res_informe(char *b, int cap); /* (1.0.59) cuánto ocupan recursos y fotos, y los archivos más grandes */
uint32_t hal_walk_take(uint32_t *min_caminando, uint32_t *min_quieto, bool *a_oscuras); /* los pasos que dio durmiendo */
const char *hal_walk_wake_reason(void);

/* acelerómetro (QMI8658) */
esp_err_t hal_imu_init(void);
/* llamar ~20 veces por segundo; marca sacudida y "lo sacaron del bolsillo" (la pantalla pasó a mirar
 * hacia arriba después de un rato de canto) */
void hal_imu_poll(bool *sacudida, bool *movimiento);
/* inclinación suavizada de la pantalla: -1..1 en cada eje (x: derecha, y: hacia abajo) */
void hal_imu_tilt(float *x, float *y);
/* da vuelta la pantalla (180°) o la deja derecha; el táctil sigue a la pantalla (LVGL) */
void hal_display_rotate(bool al_reves);
/* giro en pasos de 90°: 0 derecho, 1 = 90°, 2 = al revés, 3 = 270° (sentido horario del contenido) */
void hal_display_rotate_q(int cuartos);
/* contador de pasos por hardware del QMI8658 (24 bits; 0 si no hay) */
bool hal_steps_init(void);
uint32_t hal_steps_read(void);

/* guardado en NVS */
esp_err_t hal_storage_init(void);
bool hal_storage_load(pet_t *p);
esp_err_t hal_storage_save(pet_t *p);
void hal_storage_save_async(pet_t *p); /* v88: copia la partida y la escribe otra tarea */
esp_err_t hal_storage_erase_all(void); /* empezar de cero */
/* otros datos guardados (las celebraciones); buf = NULL borra */
bool hal_storage_get(const char *clave, void *buf, size_t *len);
esp_err_t hal_storage_set(const char *clave, const void *buf, size_t len);

/* línea de texto recibida por el USB (comandos del editor); NULL si no hay */
esp_err_t hal_usb_init(void);
const char *hal_usb_line(void);
void hal_usb_line_done(void);

/* sonido por el parlante (sintetizado con sfx.c) */
esp_err_t hal_sound_init(void);
void hal_sound_play(sound_t s);
void hal_sound_stop(void); /* (v39) corta el sonido que esté sonando (no las notas de cristal ni el ambiente) */
void hal_sound_play_soft(sound_t s); /* igual, pero con el volumen de efectos aunque sea un sonido de alarma (no suena si los efectos están en 0) */
void hal_sound_note(sound_t s, int semi, bool suave); /* (1.0.53) una nota de cristal afinada (semitonos -24..+24) */
void hal_sound_volume(int pct); /* 0..100 (el volumen general del parlante: queda en 100) */
void hal_sound_pantalla(bool encendida); /* con la pantalla apagada el parlante (amplificador y reloj de audio) se apaga tras 15 s sin sonar; al encender se prepara */
bool hal_sound_ready(void); /* la tarea del sonido está viva */
void hal_sound_gains_get(int *ella, int *efectos, int *alarma);
void hal_sound_gains(int ella, int efectos, int alarma); /* 0..100 por tipo de sonido (ver pet_sound_group) */
/* el táctil: con la pantalla apagada se ignora (en el bolsillo) */
void hal_touch_enable(bool si);
/* micrófono (para soplar): se prende solo mientras la interfaz espera un soplido */
void hal_mic_enable(bool si);
int hal_mic_level(void); /* fuerza del sonido de los últimos ~30 ms: 0..100 (-1 si no hay micrófono) */
/* (1.0.20) el soplido se reconoce en la tarea del micrófono: umbral 0..100, cuánto rato seguido y la pausa entre dos */
void hal_mic_soplido(int umbral, int seguido_ms, int pausa_ms);
uint32_t hal_mic_soplos(void); /* cuántos soplidos lleva (solo crece): el ciclo mira si cambió */
int hal_mic_pico(void);        /* el sonido más fuerte desde la última pregunta (0..100, -1 sin micrófono) */
/* sonido ambiente de fondo, mezclado con los efectos: vol 0 apagado, 1 bajito, 2 normal */
void hal_ambient_set(ambiente_t a, int vol);
/* sonidos propios (desde el editor): el reloj usa /res/s<n>.pcm o /res/a<n>.pcm (16 kHz, mono, 16 bits) */
void hal_sound_custom(sound_t s, bool propio);
void hal_ambient_custom(ambiente_t a, bool propio);

/* archivos del editor en la partición "recursos" (/res) */
esp_err_t hal_res_init(void);
bool hal_res_save(const char *nombre, const void *datos, int n);
bool hal_res_save_seguro(const char *nombre, const void *datos, int n); /* (1.0.26) escribe «nombre~», lo comprueba y recién ahí reemplaza: el de antes nunca se pierde */
int hal_res_size(const char *nombre);  /* -1 si no está */
int hal_res_read(const char *nombre, void *buf, int cap);
bool hal_res_delete(const char *nombre);

/* fotos en la partición "fotos" (SPIFFS, /fotos/f00001.qoi...) */
esp_err_t hal_photos_init(void);
int hal_photo_save(const uint8_t *datos, int n);  /* id o -1 */
int hal_photo_list(uint16_t *ids, int max);       /* las más nuevas primero */
int hal_photo_size(int id);
int hal_photo_read(int id, uint8_t *buf, int cap);
bool hal_photo_delete(int id);

/* botón BOOT */
void hal_button_init(void);
bool hal_button_pressed(void);
void hal_fotos_mantener(void); /* limpieza de la flash de fotos: llamar con la pantalla apagada */
bool hal_pwr_short_press(void); /* pulsación corta del botón PWR (una vez por pulsación) */

/* amigos cercanos por Bluetooth LE (radio.c) */
esp_err_t hal_radio_init(void);
void hal_radio_set_beacon(const uint8_t *datos, int n); /* PET_BALIZA_BYTES */
bool hal_radio_poll(uint8_t out[PET_BALIZA_BYTES]);     /* una baliza recibida, si hay */
