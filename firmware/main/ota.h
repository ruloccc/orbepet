/* Actualizar el firmware por Wi-Fi (desde la versión 1.0.09).
 *
 * El orbe se reinicia en un «modo actualizar» liviano: no arma el juego (ni sonido, ni Bluetooth, ni micrófono),
 * así sobra memoria para el Wi-Fi y la conexión segura. En ese modo:
 *   - crea su propia red (ORBEPET-XXXX, clave orbepet1) con una página en http://192.168.4.1 para el celular:
 *     ahí se elige el Wi-Fi de la casa, se busca la actualización o se sube un .bin a mano;
 *   - si ya conoce el Wi-Fi de la casa, se conecta, mira version.json en GitHub (ruloccc/orbepet-firmware)
 *     y, si hay una versión más nueva, la baja y se reinicia en ella.
 * Si la versión nueva se cae antes de su primer minuto, el arranque vuelve solo a la anterior (rollback). */
#pragma once

#include <stdbool.h>
#include <stddef.h>

/* desde el juego: deja la marca y reinicia (la partida hay que guardarla antes) */
void ota_pedir_modo(void);
/* al arrancar: ¿se pidió el modo actualizar? (borra la marca: si el modo falla, el próximo arranque es normal) */
bool ota_modo_pedido(void);
/* el modo actualizar: no vuelve (termina reiniciando) */
void ota_modo(void);
/* la versión que está corriendo funciona: cancela la vuelta atrás (llamar tras ~1 minuto de juego y antes de dormir profundo) */
void ota_arranque_ok(void);
/* (1.0.11) lo que pasó en el último modo actualizar (para el registro; NULL si nada). Se lee una sola vez. */
const char *ota_diagnostico(void);
/* (1.0.11) el nombre de la red propia del modo actualizar (ORBEPET-XXXX), sin prender el Wi-Fi */
void ota_nombre_red(char *b, size_t n);
/* si la última actualización falló y se volvió atrás: la versión que falló (NULL si no) */
const char *ota_version_fallida(void);
