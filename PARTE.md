# PARTE — estado del trabajo (17-nov-2026, 16:30)

Proyecto: mascota virtual en un orbe ESP32-S3 (AMOLED redonda 466×466, LVGL 9.4). **Responder siempre en español.**
Versión en curso: **1.0.09** (así se numeran ahora: 1.0.09 = el «firmware 109»; siguen 1.0.10, 1.0.11...).
Partida **versión 42** (no cambia desde el 104).

**El proyecto vive en GitHub** (cuenta `ruloccc`, conectada a Claude):
- `ruloccc/orbepet` (PRIVADO): el código. Se trabaja ahí: clonar, cambiar, commit y push a `main`.
- `ruloccc/orbepet-firmware` (PÚBLICO): solo `mascota.bin` + `version.json` (lo que baja el orbe) y las Releases.
- Ya no hace falta pedir el zip: `git clone --depth 1 https://github.com/ruloccc/orbepet` (con add_repo antes).
  Igual se le entrega un zip cuando necesite grabar por cable.

**Compilar firmware:** GitHub Actions (`.github/workflows/firmware.yml`) compila con ESP-IDF v5.5 en cada push a main
y revisa que quepa en ota_1. Con una etiqueta `v<versión>` además publica. Ver el resultado:
`gh api repos/ruloccc/orbepet/actions/runs` (gh funciona por el proxy aunque `gh auth status` diga que no).
El usuario también puede compilar y grabar con cable con el probador, como siempre.

**Publicar una versión:** subir `FW_VERSION` en `firmware/main/version.h` (y `FW_NOMBRE`), sección en README,
commit, `git tag v1.0.10 && git push origin v1.0.10`. Sin el secreto `ORBEPET_FIRMWARE_TOKEN` el paso de publicar
no hace nada: entonces bajar el artefacto de la ejecución y subir mascota.bin + version.json a orbepet-firmware a mano
(push directo a su main) y crear la Release.

---

## 1. Lo primero que hay que preguntar (quedó pendiente)

**«o la hormigas».** El usuario vio una frase mal armada al referirse a un animal (ejemplo: «o la hormigas») y a
los **berros** («Berrios» en su mensaje). Se revisó todo el código que arma frases con nombres de animales y de
comidas de temporada y no aparece ese texto. **Preguntarle en qué pantalla o en qué momento salió** (una foto
basta) y arreglarlo ahí.
Lo que sí estaba mal y ya se arregló en el 108: al conocer un animal de nombre plural decía «¡Wow, ES las
hormigas!»; ahora dice «son» (`ui_magia.inc`, EV_ANIMAL_NUEVO). Ojo: los nombres vienen CON artículo en
`FICHA_NOMBRE` (core/textos23.h) y las comidas tienen dos formas (`"los berros"` y `"berros"`, core/pet.c):
la mayoría de los errores de concordancia van a estar en frases que mezclan las dos.

---

## 2. Qué se hizo en las últimas versiones

**1.0.09 (actualizar por Wi-Fi; número de versión)** — se graba CON CABLE una vez (trae el arranque con vuelta atrás)
- `firmware/main/version.h`: FW_VERSION/FW_NOMBRE, único lugar. CMake la pone en esp_app_desc (PROJECT_VER).
- Ajustes: botón «VERSIÓN 1.0.09» bajo la cuadrícula → `panel_version` (PANEL_VERSION, prueba 84) → ACTUALIZAR
  (UI_ACCION_ACTUALIZAR 186): main guarda y `ota_pedir_modo()` (marca en RTC_NOINIT + reinicio).
- `ota.c`: al arrancar con la marca, `ota_modo()` en vez del juego (sin sonido, BLE, micrófono, registro). Red propia
  ORBEPET-XXXX / clave orbepet1, página en 192.168.4.1 (Wi-Fi de casa, buscar, subir .bin crudo por XHR, salir).
  Con Wi-Fi: lee raw.githubusercontent.com/ruloccc/orbepet-firmware/main/version.json y baja mascota.bin con
  esp_https_ota. Revisa que el proyecto sea «mascota». Sale con SALIR o a los 15 min sin uso.
- Vuelta atrás: CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE; `ota_arranque_ok()` confirma tras 60 s de juego y antes de
  dormir profundo. Al arrancar: «¡VERSIÓN x!» si cambió (NVS "fwv"), o «LA x NO FUNCIONÓ» (NVS "fwmala").
- Credenciales Wi-Fi en NVS clave "wifi"; el Wi-Fi no guarda nada propio (WIFI_STORAGE_RAM).
- USB: `MASCOTA VERSION`. sdkconfig: Wi-Fi/lwIP/mbedTLS en PSRAM, bundle de certificados, cabeceras HTTP 1024.
- NADA de esto se probó en la placa todavía (aquí no hay ESP-IDF): lo valida la compilación de GitHub y el usuario.

**108 (perfiles, Pochi, paleta, textos)**
- **Cada mascota tiene SU mundo.** `pet_resume` (core/pet.c) le pasaba a la que llegaba el mundo de la que se
  quedaba; con un perfil nuevo (mundo vacío) la mascota antigua perdía casa, flores, jardín, ropa, juguetes y
  amigos al volver a ella. Ahora solo se pasa lo del orbe: hora, hemisferio, país, estación forzada, volumen del
  ambiente, clima y `vio`. Prueba `test_fw108_mundos`. Lo ya perdido en la placa no se recupera sin respaldo.
- **Pochi no interrumpe** (`po_ocupada()` en ui_pochi.inc): no llega en baño, picnic, pecera, fiesta, ceremonia,
  ni pescando, nadando, trepando, cantando, bailando, charlando con un animal, con juguete/pelota, con el vecino,
  probándose algo o en la siesta. Si le tocaba, queda `s_po_quiere` y llega apenas termina.
- **Pochi no se pasa de perfil** (`po_fuera()`), al cambiar de mascota y al estrenar una.
- **Paleta de colores**: el botón usaba la paleta vieja y las criaturas se pintan con `BEBE_PAL`; ahora muestra el
  color de verdad (`color_cuerpo_nuevo` en art.c).
- **Mensaje inicial**: reescritas las dos primeras páginas del Libro (ui_final.inc), en primera persona.

**107 (el huevo del inicio)**
- Al poner el reloj se mandaban ASPECTO + GENERO + ECLOSIONAR juntos: nacía al instante y el huevo nunca se veía.
  Ahora el huevo llega al prado, se mece y se le abren las grietas **9 s** (`NACE_MS`, `s_nace_desde`) y recién
  ahí nace, con estrellitas y «¡HOLA!»; el nombre espera 2,6 s (`s_nace_fin`).
- **El nombre ya no se pierde**: el panel del ritmo se colaba antes del nombre (ahora espera) y la pregunta del
  nombre ya no exige ser huevo: cualquier mascota sin nombre vuelve a tenerla.

**106 (pantalla de elegir huevo)**
- Vivero nuevo dibujado como las zonas (`herramientas/dibujar_vivero_v106.py` → `fondos_pantallas/0_huevos.png`):
  prado de noche, pasto, arbustos, piedras, hongos, luciérnagas y **ocho nidos tejidos**.
- Huevos al doble de detalle (92×90) **con el mismo diseño** (`herramientas/iconos_nuevos/huevos92.py`, parte de
  `huevos_base.png`, EPX/Scale2x + luz). art.c los dibuja a la mitad: ocupan lo mismo que antes.
- Barra de la pista transparente; el elegido lleva un **óvalo** de luz que encierra huevo y nidito; pistas nuevas
  de dos líneas (el huevo de rayo «espera la tormenta», ya no «le asustan»).

**105 (la casita que no aparecía)**
- Si no hay bloque de 876 KB para el lienzo, el fondo se arma **por franjas** (`fd_franjas` en art.c; 156, 78, 36,
  12 o 6 filas) con todos sus adornos. Sale igual pixel a pixel: `sim/web/pruebas/fondos_franjas.js` (1.260 casos).
  Se prueba a mano con `SINLIENZO 1|0|R`, el botón del simulador, el probador o la fase **29c** de prueba_total.
- **Probador** (`herramientas/mascota_gui.py`): 8 pestañas por función y cuadros **plegables** (clase `Plegable`);
  el registro pasó de 12 a 20 líneas. Aquí no hay tkinter: no se puede ejecutar, solo revisar a ojo.

---

## 3. Qué falta (en orden)

0. **Probar la 1.0.09 en la placa**: grabar con cable, ver Ajustes → Versión, entrar a ACTUALIZAR, conectar el
   celular a ORBEPET-XXXX, poner el Wi-Fi de la casa, «Buscar» (debe decir «¡ESTÁS AL DÍA!»), y después publicar
   una 1.0.10 de prueba para ver que llega sola. Revisar que el programa siga cabiendo (lo dice la compilación).
1. **Probar en la placa todo lo del 102 al 108.** Nada de eso se ha probado salvo lo que el usuario vio: Pochi
   hembra y los 4 cachorros, escenas de familia, toques a animales, el 104 entero (papelitos, hielo, paseos,
   terquedad), los fondos por franjas (fase 29c), el huevo del inicio y los perfiles.
2. **El tamaño.** El programa creció ~200 KB en el 106 (huevos 160 KB + vivero 40 KB). Si al grabar no cabe en
   `ota_1` (0x7A0000): alias de primavera en FD_TABLA (−80 KB), bajar `CUANTOS` en huevos92.py, o dejar las
   grietas 2 y 3 en el dibujo chico.
3. **Batería y memoria, lo de siempre**: confirmar que el micrófono ya no se come la RAM interna (registro con
   «escuchar música» varias horas); noche sin cable con sueño profundo (nunca probado); %/h con pantalla apagada;
   reposo del táctil (CST9217 en GPIO2).
4. **Editor de personajes** (conversación del 17-nov 11:52): el arte nuevo es de 48 px (y 96 los grandes) y el
   editor dibuja íconos de 24×24; además, cuando hay dibujo propio, `ico_render` se salta la tabla HD y el pulido.
   Acordado: primero **abrir el dibujo de fábrica en 48×48 para retocarlo** (el firmware tiene que aceptar ese
   tamaño: `art_icono_lado_de_bytes`), y después un editor por partes (colores, ropa, orejas, cola, expresión).
   La partición de recursos son 6 MB: espacio no es problema.
5. Retoques que pida el usuario. Los pendientes viejos siguen en `claude/pendientes-v89.md`.

---

## 4. Decisiones del usuario que NO hay que deshacer

- **Pochi** es hembra y mamá de **Chapi** y **Rony** (niños) y **Pasita** y **Morita** (niñas). Pasita es la MÁS
  clara. Rony **sin collar**; Chapi verde, Pasita rosado, Morita morado. Los cuatro **igual de importantes**.
  Solo Pochi anda con la lengua afuera. Con dos o más cachorros NO hay encuentros con animales.
- El cachorro escondido **no se dibuja** mientras está escondido: se mueve el pasto y después asoma entero.
- El olor del zorrillo **no es suciedad** (vahos verde claro 0xCBE86B, opacidad hasta 205; no bajar de ahí).
- **Los huevos mantienen su diseño original**: el 106 probó rediseñarlos (cáscara del color de su elemento) y el
  usuario lo rechazó. Solo subir calidad, nunca cambiar el dibujo.
- La marca del huevo elegido es un **óvalo** que encierra huevo y nidito, no un círculo sobre el huevo.
- **Cada mascota tiene su mundo y sus historias**; Pochi no se pasa de perfil.
- No usar pedazos del prado ni del bosque en fondos nuevos (dibujos propios con `pixel_mundo.py`).
- No dañar lo que ya existe: tras cada cambio, test_core, paneles (18), regresión y, si toca el motor, soak.
- Textos en MAYÚSCULAS vía `gtxt`; el Rey es **Peumo**; galletas de **piñón con miel de ulmo**.
- Los enum ya guardados en partidas no se reordenan; campos nuevos al final, antes de `crc`, con conversión.
- Versiones con formato **1.0.XX** (parche de dos cifras) y visibles en el orbe. Repos: código privado, .bin público.

---

## 5. Cómo comprobar (todo esto corre aquí)

Herramientas (hay que bajarlas en cada sesión nueva, GitHub responde):
```
git clone --depth 1 -b v9.4.0 https://github.com/lvgl/lvgl.git /home/claude/lvgl
curl -sL -o wasi.tgz https://github.com/WebAssembly/wasi-sdk/releases/download/wasi-sdk-25/wasi-sdk-25.0-x86_64-linux.tar.gz
curl -sL -o bin.tgz  https://github.com/WebAssembly/binaryen/releases/download/version_123/binaryen-version_123-x86_64-linux.tar.gz
# (descomprimir los dos en /home/claude/sdk)
```
Pruebas:
```
cd build && cmake .. && make test_core && ./test_core          -> 1160/1160 comprobaciones OK
export WASI_SDK=/home/claude/sdk/wasi-sdk-25.0-x86_64-linux LVGL_DIR=/home/claude/lvgl BINARYEN=/home/claude/sdk/binaryen-version_123
sh sim/web/build.sh
node sim/web/pruebas/paneles.js          -> 18 problemas (la base de siempre)
node sim/web/pruebas/regresion.js        -> «dibujo: igual que la línea base» (--base para regrabarla)
node sim/web/pruebas/soak.js             -> soak: 180 días sin caerse   (~10 min)
node sim/web/pruebas/fondos_franjas.js   -> 1260 casos, 0 distintos
node sim/web/pruebas/inicio.js | libro.js | libro_flujo.js | final.js
gcc -fsyntax-only -std=gnu17 -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers -Wno-sign-compare \
  -DLV_CONF_INCLUDE_SIMPLE -Isim/preview -I/home/claude/lvgl -Icore -Ifirmware/main firmware/main/ui.c
```
Mirar una pantalla sin la placa (muy útil para revisar dibujos):
```
node -e "const {crear}=require('./sim/web/pruebas/arnes.js');(async()=>{const s=await crear({seed:7});const x=s.x;
 s.listo(30); x.sim_close_panel(); s.frames(60); x.sim_test_panel(29); s.frames(10); s.png('/tmp/p.png');})()"
```
(`sim_test_panel(k)` abre un panel por número; `sim_test_panel_name(k)` dice cuál es; `sim_panel_id()` el que está
abierto. Ojo: `s.listo(30)` deja abierto el panel del huevo: hay que cerrarlo con `sim_close_panel()`.)

En la placa: `herramientas/prueba_total.py` (fase 29c para los fondos por franjas) y el probador
`herramientas/mascota_gui.py`.

---

## 6. Archivos clave

- Motor: `core/pet.h`, `core/pet.c`, `core/v41.inc`, `core/historia.inc`, `core/toques.inc`, `tests/test_core.c`
- Interfaz: `firmware/main/ui.c` + `ui_pochi.inc`, `ui_zonas.inc`, `ui_zjuegos.inc`, `ui_final.inc`, `ui_magia.inc`,
  `ui_historia.inc`, `ui_escenas.inc`, `ui_ceremonias.inc`
- Arte: `firmware/main/art.c`, `art.h`, `fondos_data.inc`, `pantallas_data.inc`, `huevos_data.inc`,
  `iconos_hd.inc`, `iconos_hd2.inc`, `personajes_data.inc`
- Dibujo (Python): `herramientas/pixel_mundo.py`, `dibujar_zonas_v98.py`, `dibujar_vivero_v106.py`,
  `hacer_fondos.py`, `hacer_pantallas.py`, `iconos_nuevos/huevos92.py` (+ `huevos_base.png`)
- Placa y pruebas: `firmware/main/main.c`, `herramientas/prueba_total.py`, `herramientas/mascota_gui.py`
- Simulador: `sim/web/web_main.c`, `sim/web/plantilla.html`, `sim/web/pruebas/*.js`
- Actualizar por Wi-Fi: `firmware/main/ota.c`, `ota.h`, `version.h`, `.github/workflows/firmware.yml`
- Documentación: `README.md` (una sección por firmware) y `claude/pendientes-v89.md`
