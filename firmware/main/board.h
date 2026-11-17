/*
 * Waveshare ESP32-S3-Touch-AMOLED-1.75C.
 *
 * La pantalla (CO5300, reset GPIO1), el táctil (CST9217, reset GPIO2, int GPIO11),
 * el audio (ES8311, MCLK GPIO16, amplificador GPIO46) y el bus I2C (SDA 15, SCL 14)
 * los maneja el BSP oficial de Waveshare (componente waveshare/esp32_s3_touch_amoled_1_75c).
 * Esta placa NO tiene expansor TCA9554. Aquí solo van las direcciones de los chips
 * que usamos directamente por el bus I2C del BSP. Si la placa no trae el RTC, el
 * firmware usa el reloj interno (ver hal_rtc_init).
 */
#pragma once

#define BOARD_LCD_H_RES 466
#define BOARD_LCD_V_RES 466

/* chips en el bus I2C compartido (SDA 15, SCL 14) */
#define BOARD_RTC_ADDR 0x51 /* PCF85063: reloj de tiempo real    */
#define BOARD_IMU_ADDR 0x6B /* QMI8658: acelerómetro y giroscopio */
#define BOARD_PMU_ADDR 0x34 /* AXP2101: batería y carga          */
#define BOARD_I2C_HZ 400000

#define BOARD_BTN_BOOT 0 /* botón BOOT, programable */

/* audio */
#define BOARD_AUDIO_HZ 16000
#define BOARD_AUDIO_VOLUMEN 75 /* 0..100 */
/* inclinación: signo de cada eje del acelerómetro respecto de la pantalla (ajustar en la placa
 * si al inclinar a la derecha la mascota se va a la izquierda) */
#define BOARD_IMU_SIGNO_X (-1.0f)
#define BOARD_IMU_SIGNO_Y (1.0f)
/* el signo de la inclinación Y cuando el reloj está derecho (la parte de arriba de la pantalla hacia arriba).
 * Si en la placa se da vuelta justo al revés, se cambia a (-1.0f). */
#define BOARD_IMU_DERECHO_Y (1.0f)
/* igual para el eje X: signo de la inclinación X cuando el reloj está acostado con su lado derecho hacia abajo.
 * Si al girarlo así la imagen queda de lado al revés, se cambia a (-1.0f). */
#define BOARD_IMU_DERECHO_X (1.0f)
/* si en los giros de lado la imagen queda al revés (derecho y al revés salen bien), se pone 1: invierte los dos giros de 90° del panel */
#define BOARD_GIRO_90_INVERTIDO 0

/* (v99, batería) con la pantalla apagada, el panel AMOLED y el táctil entran en reposo de verdad (0x28/0x10).
 * Si en la placa la pantalla no vuelve bien al encender, poner 0: queda como antes (solo brillo 0). */
#define BOARD_PANEL_REPOSO 1
/* (v99) a los 8 s sin tocarla (y sin nada que leer ni jugar) la pantalla baja su brillo y dibuja a la mitad de cuadros */
#define BOARD_ATENUAR_MS 8000u
#define BOARD_ATENUAR_PCT 40
#define BOARD_REFRESCO_MS 30u
#define BOARD_REFRESCO_REPOSO_MS 66u

/* (v100, batería etapa 3) con la pantalla apagada, sin cable y sin sonido, el procesador duerme (sueño ligero) entre vuelta
 * y vuelta del juego. Ella sigue viviendo: el ciclo despierta 4 veces por segundo. Si da problemas en la placa, poner 0. */
#define BOARD_SUENO_LIGERO 1

/* (v101, batería etapa 4) de noche (ella dormida y en su hora de dormir, pantalla apagada BOARD_NOCHE_TRAS_MS, sin cable ni
 * sonido) la placa entra en sueño PROFUNDO hasta el fin de la noche o 1 min antes de una alarma. BOOT la despierta al instante;
 * PWR y el cable, en hasta BOARD_NOCHE_REVISAR_S segundos (cada tanto despierta un momento a mirar). Si da problemas, poner 0. */
#define BOARD_SUENO_NOCHE 1
#define BOARD_NOCHE_TRAS_MS (10u * 60u * 1000u)
#define BOARD_NOCHE_REVISAR_S 5u
/* (v101) con la pantalla apagada, el arrullo (grillos mientras ella se duerme) suena con el procesador a 80 MHz. Si suena cortado: 0 */
#define BOARD_ARRULLO_80MHZ 1
