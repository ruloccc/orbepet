/*
 * Sonidos de la mascota sintetizados (sin archivos de audio).
 * El mismo código suena en el reloj (por el parlante) y en el simulador.
 */
#ifndef SFX_H
#define SFX_H

#include <stdint.h>

#include "pet.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SFX_MAX_SEGUNDOS 8.5f /* la canción de cumpleaños es la más larga */

/* Escribe el sonido como PCM de 16 bits mono. Devuelve la cantidad de muestras
 * escritas (0 si no hay sonido). max_muestras debe alcanzar para SFX_MAX_SEGUNDOS. */
/* duración (ms) de una frase cantada de la especie: el compás en que se repite */
uint32_t sfx_frase_ms(int especie);
int sfx_render(sound_t s, int16_t *out, int max_muestras, int frecuencia_hz);

/* Sonido ambiente (pet_ambient): escribe un bucle de SFX_AMB_SEGUNDOS que empalma sin cortes
 * (para repetirlo). out necesita SFX_AMB_SEGUNDOS + 0.5 s de lugar (se usa para empalmar).
 * Devuelve las muestras del bucle (0 = silencio). */
#define SFX_AMB_SEGUNDOS 8.0f
int sfx_ambient(ambiente_t a, int16_t *out, int max_muestras, int frecuencia_hz);

#ifdef __cplusplus
}
#endif
#endif
