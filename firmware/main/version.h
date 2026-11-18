/* La versión del firmware: el ÚNICO lugar donde se cambia.
 *
 * Se lee en tres partes:
 *   - el orbe la muestra en Ajustes → Versión y en la pantalla de actualizar,
 *   - firmware/CMakeLists.txt la copia a la descripción del programa (esp_app_desc: «version»),
 *   - la compilación en GitHub (.github/workflows/firmware.yml) la usa para nombrar la publicación
 *     y escribir version.json, que es lo que el orbe consulta para saber si hay una más nueva.
 *
 * Formato: MAYOR.MENOR.PARCHE con el parche de dos cifras («1.0.09», «1.0.10»). Cada publicación sube el número.
 * (1.0.09 = el firmware 109 de la numeración vieja: el primero que se actualiza por Wi-Fi.)
 */
#pragma once

#define FW_VERSION "1.0.22"
#define FW_NOMBRE "MÁS FLUIDO"

/* «1.0.09» → 10009, para comparar cuál es más nueva (−1 si el texto no es una versión) */
static inline int fw_codigo(const char *v)
{
    int n[3] = {0, 0, 0}, k = 0, dig = 0;
    for (; v && *v && k < 3; v++) {
        if (*v >= '0' && *v <= '9') { n[k] = n[k] * 10 + (*v - '0'); dig = 1; }
        else if (*v == '.' && dig) { k++; dig = 0; }
        else break;
    }
    if (k != 2 || !dig) return -1;
    return n[0] * 10000 + n[1] * 100 + n[2];
}
