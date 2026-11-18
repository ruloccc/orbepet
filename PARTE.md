# PARTE — estado del trabajo (18-nov-2026, 17:20)

Proyecto: mascota virtual en un orbe ESP32-S3 (AMOLED redonda 466×466, LVGL 9.4, ESP-IDF 5.5). **Responder siempre en
español.** El usuario casi no puede usar el PC: todo se publica para actualizar por Wi-Fi desde el orbe. «Deja de romper
cosas»: tras cada cambio, las pruebas de la sección 5. Nada puede quedar más lento.
Versión publicada: **1.0.23** («PRUEBAS, POCHI Y FARO»). Partida **versión 42** (no cambia desde el 104).

**El proyecto vive en GitHub** (cuenta `ruloccc`, conectada a Claude):
- `ruloccc/orbepet` (PRIVADO): el código. Clonar con add_repo + `git clone`, commit y push a `main`.
- `ruloccc/orbepet-firmware` (PÚBLICO): lo que baja el orbe: `mascota.bin`, `version.json`, `bootloader.bin`,
  `partition-table.bin`, `ota_data_initial.bin` y `recursos/pj0..pj5.bin`. **NUNCA** subir ahí `mascota.elf`.
- `ruloccc/orbepet-informes` (PRIVADO): los informes que manda el orbe (Ajustes → Versión → ENVIAR INFORME), como issues.
  Leerlos: `gh issue list -R ruloccc/orbepet-informes` y `gh issue view N -R ruloccc/orbepet-informes`.
  La llave (fine-grained, solo Issues de ese repo) la tiene el usuario; se carga por cable con el programa del PC
  (Respaldo → «Cargar llave de informes…»). **Nunca** pedirla por el chat ni ponerla en el código o el registro.

**Compilar:** cada push a main compila en GitHub Actions (`.github/workflows/firmware.yml`, ~11 min) y deja todo en la
rama `compilados` (incluido `mascota.elf`). Ver: `gh run list -L 3` / `gh run view <id>`.
**Publicar** (desde aquí no se pueden empujar etiquetas): subir `FW_VERSION`/`FW_NOMBRE` en `firmware/main/version.h`,
sección en README y PARTE, push; cuando compile:
`git fetch origin +refs/heads/compilados:refs/remotes/origin/compilados`, sacar los archivos a una carpeta
(`git --work-tree=/tmp/comp checkout origin/compilados -- .` y `git reset`) y copiar a `orbepet-firmware` todo
**menos** `mascota.elf` y `flasher_args.json` (con `recursos/`), commit y push. Comprobar con
`gh api repos/ruloccc/orbepet-firmware/contents/version.json -q .content | base64 -d`.
En el orbe: Ajustes → Versión → BUSCAR ACTUALIZACIÓN. Desde la 1.0.21 los fondos nuevos (pj*.bin) bajan en el mismo
viaje; si el orbe venía de una anterior a la 1.0.21, hay que apretar dos veces.
**Pánicos:** el registro trae «FALLO_BT: 0x42...». El .elf de la versión publicada está en `compilados`; para otra:
poner el commit en `.github/simbolos.txt`, push, esperar «Simbolos de una version», `git fetch origin elf-<commit7>` y
`/home/claude/tc/xtensa-esp-elf/bin/xtensa-esp32s3-elf-addr2line -pfiaC -e mascota.elf <direcciones>`
(la cadena de herramientas hay que bajarla de nuevo en cada sesión).

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

**1.0.23 (pruebas, Pochi y faro)** — Pochi: ui_pochi.inc no llega con s_escena o s_pet->escena == ESC_ISLA, sus
paseos no incluyen la isla y si la mascota llega a la isla con Pochi, Pochi pasa a PO_SE_VA. Faro: herramientas/
hacer_fondos_juegos.py faro() arma la pantalla nueva (FARO2_LAMPARA 391,150; FARO2_MUELLE 206,330) y regenera
isla_mapa.inc; ui_isla.inc ZF_LX/LY/MX/MY/CX/CY, salen por la izquierda (ángulo π±), ella en (431,190). Órdenes de
prueba por cable: ZDABRIR, ZDINFO, ISLATOQUE, RECURSOS, FONDODOS, OLOR, SOPLOS, FUSION, VELOCIDAD → herramientas/
prueba_nuevo.py («Probar lo nuevo» y fase 29d de la PRUEBA TOTAL).

**1.0.22 (más fluido)** — ui.c ui_inv_fusionar (LV_EVENT_INVALIDATE_AREA, registrado DESPUÉS del redondeo del BSP en
services.c y en el simulador): con inv_p ≥ LV_INV_BUF_SIZE-4 une el área nueva con la que menos crece y la deja «adentro»
(LVGL no la agrega). Antes, al pasar de 32 áreas LVGL invalidaba la pantalla entera (lv_refr.c lv_inv_area): eso eran los
178 kpx/cuadro del registro. Prueba: sim/web/pruebas/redibujo.js [semilla] (compara con un redibujo completo; diferencias
de 1-12 niveles son redondeo). sim_inv_fusion(0) lo apaga para comparar. Memoria: EXT_RAM_BSS_ATTR en los búferes de
cable de main.c, lista de ota.c y PET_EXT_RAM (core/pet.c) para el tmp del respaldo. Isla: isla_toque() en fondo_cb.
sim_alfa_scan lista las imágenes con transparencia a la vista (ARGB8888) y cuántas veces se redibujaron.

**1.0.21 (micrófono y fondos)** — services.c: el micrófono pedía dma_libre ≥ 8192 y en partida larga la interna queda en
~7,6 KB → no abría nunca. bsp_audio_init ya reservó los DMA de tx y rx (3×160 cuadros); abrir en estéreo solo los rehace
a 640 B (~2 KB): ahora pide 3072/1024. Registra «micrófono: abierto (RAM DMA …)» la primera vez. ota.c E_BAJAR: tras grabar
el programa llama recursos_al_dia() (los pj*.bin nuevos llegan en el mismo viaje). art.c art_fondo_dos: un solo bloque
(los índices se descomprimen al final del búfer de la imagen y se convierten hacia adelante), reintento tras soltar las
otras pantallas y la caché; eventos «juego: no está el fondo pjN» / «sin memoria para el fondo». ui_pochi.inc: con
s_visita o s_vecino visibles Pochi no llega (ni queda pendiente); llega recién 6 min después de que se fueron.

**1.0.20 (soplido e informes)** — SOPLIDO: services.c reconoce el soplido en tarea_mic (trozos de 16 ms;
hal_mic_soplido(umbral, seguido, pausa), hal_mic_soplos() cuenta, hal_mic_pico() para dibujar); main.c 5c solo cuenta
los nuevos y llama ui_mic_blow. Umbral 34/220 ms (bote/pesca 24/140 ms). Si esperó ≥ 8 s sin oír nada: evento
«micrófono: esperó un soplido…» con el pico (cada 10 min como mucho). Si la niebla sigue sin irse, pedir un informe:
ahí sale si el micrófono abrió y cuánto sonó (si el pico es bajo, bajar el umbral; si «NO pudo abrirse», es la RAM DMA).
INFORMES: Ajustes → Versión → ENVIAR INFORME (UI_ACCION_INFORME 191) → ota_pedir_informe() (marca 0x1F0A3E55) → ota_modo
con s_informe: tras conectar va a E_INFORME → enviar_informe(): monta /res, hal_log_read (lo mismo que MASCOTA REGISTRO),
UTF-8 saneado, POST a api.github.com/repos/ruloccc/orbepet-informes/issues con la llave de NVS «inf_llave»
(MASCOTA LLAVE <llave>|BORRAR|?; botón en mascota_gui → Respaldo). La llave (fine-grained, sin vencimiento, Issues
lectura/escritura solo en orbepet-informes) la tiene el usuario; NUNCA ponerla en el código, el registro ni el chat.
Para leer los informes: `gh issue list -R ruloccc/orbepet-informes` (repo privado: requiere la sesión de GitHub).

**1.0.19 (arreglo de los juegos)** — PÁNICO de la 1.0.18 en el buceo: s_zj_o[i] usado como caja y como imagen
(zj_tipo lo arregla). Fondos rehechos sobre fondos_zonas (herramientas/hacer_fondos_juegos.py; el faro = la isla de
noche + isla_mapa.inc). Manito de ayuda (zd_mano), anillos, portadas claras, textos propios (zj_texto_c).

**1.0.18 (el faro y el buceo)** — ui_isla.inc (ZJ_FARO 10, ZJ_BUCEO 11, dentro del marco de ui_dos.inc), fondos
pj4/pj5 (herramientas/dibujar_juegos_isla.py; el faro es el MISMO de la isla). Menú Jugar: primero los de la zona.
`mundo_en_pausa()` ampliado (juegos de pantalla, bote, picnic, paneles; no baño/pecera/comer); po_ocupada bloquea a
Pochi en actividades. Hedionda sola (EV_HEDIONDA, momentos fijos por día con dado_fijo, no gasta el azar), se quita
con pet_air_out (soplar 3 o tocar 8 vahos). Micrófono reintenta (services.c). Pendiente: ver en el registro del
usuario si sale «micrófono: poca memoria DMA» al viajar en bote.

**1.0.17 (juegos de a dos)** — con visita, JUGAR muestra ping pong, tira y afloja, memorice y escondidas
(`firmware/main/ui_dos.inc`, dentro del marco zj: ZJ_PING..ZJ_ESCONDE = 6..9). Motor: `pet_duo_game`,
`DIA_JUEGO_DOS`, `UI_ACCION_JUEGO_DOS` (188); dos_jugadas/dos_ganadas/dos_dia en los 3 bytes de reserva104.
Mundo en pausa (`mundo_en_pausa()`: cielo, fauna, animales, luces, partículas, niebla, humo, tierra) mientras zj tapa
todo. Fondos en /res/pj0..pj3.bin: `art_lector` + `art_fondo_dos` (pantallas 16..19); el modo Wi-Fi los baja si
faltan (version.json "recursos", ota.c `recursos_al_dia`); por cable `MASCOTA ARCHIVO` + `probar.enviar_recursos`.
La CI copia firmware/recursos/*.bin al repo público. Prueba: `node sim/web/pruebas/dos.js`.
Al publicar a mano: copiar también la carpeta `recursos/` de la rama compilados.

**1.0.16 (arreglo al crear mascota)** — pánico en po_fuera() (ui_pochi.inc) con s_po_img NULL. ocultar() ahora
ignora NULL. Publicada en GitHub (con la 1.0.15 adentro).

**1.0.15 (teclado grande)** — SOLO zip por USB (el usuario pidió no pasarla por GitHub: commit local, sin push).
El usuario notó la 1.0.14 «muy lenta»: los datos de cada cuadro volvieron a la RAM interna (s_pet, s_ani, s_flot,
s_gpend, s_chi, s_amb, s_ptxt_info). Falta medir: pedir el registro y comparar «pantalla lenta: dibujar X ms» y
«vuelta prom». Ojo: ya en la 1.0.09 el prado dibujaba ~104 ms por cuadro (9 cuadros/s) con la pantalla encendida.
Zip liviano (firmware, core, herramientas, tests, raíz): el zip completo solo si lo pide.
Idea para la próxima: juegos de dos (ping pong y otros) cuando llega un vecino o amigo.

**1.0.14 (más memoria libre)** — pánicos en la 1.0.12: fopen → lock_init → sin RAM interna (135 B) → abort. El Wi-Fi
fija ~6,5 KB de RAM interna. Se pasaron ~20 KB de estáticos a la PSRAM (s_pet, textos, animales, etc.). Ver en el
registro «ARRANQUE listo: ... RAM interna» (antes ~12 KB) y los «vivo: RAM interna» durante el juego (antes 1-7 KB).
Modo Wi-Fi de la 1.0.13 (entrar: 124 KB; Wi-Fi gastó 55 KB en la 1.0.11; pantalla normal 70 KB, liviana ~42 KB).

**1.0.13 (Wi-Fi desde el orbe)** — pedido del usuario: nada de celular. Modo Wi-Fi con pantalla propia: lista de
redes → teclado (minúsculas/mayúsculas/símbolos) → conecta → aviso con 5 s → descarga → reinicio. Con Wi-Fi guardado
no pregunta nada. CANCELAR reinicia al juego. Pantalla en modo liviano (bsp_display_modo_liviano) para que quepa el
Wi-Fi. Pantallas en `ota_pantalla.inc`; vista previa en el PC: `make mascota_ota && ./mascota_ota carpeta` (cmake con
-DLVGL_DIR). ota.c pasó `-fsyntax-only` con el compilador xtensa y los encabezados de ESP-IDF 5.5.6 (sdkconfig.h a
mano en /tmp/chk), pero NO COMPILADO del todo con ESP-IDF al entregar el zip (el usuario pidió solo el zip; aquí no hay ESP-IDF completo):
revisar la primera compilación. Ni la pantalla del modo Wi-Fi ni la memoria están probadas en la placa.
Sin publicar en orbepet-firmware (el usuario avisa cuándo). Commits locales sin subir desde 1.0.12.

**1.0.12 (menos terca al dormir)** — con la luz apagada decía «¡UN RATITO MÁS!» y bostezaba cada segundo
(prob_paso(1.2f) = 100 % por paso) y eso la dejaba a medio cruzar entre mundos. Ahora a lo más cada 4 min
(estático `s_terca_aviso` en pet.c) y EV_NIEGA no la toca mientras cruza (ui_escena_especial() & 2).
ACTUALIZAR «pegado» (1.0.11 en la placa): el modo actualizar corre SIN pantalla (no le alcanza la RAM interna para
LVGL) y el AMOLED conserva la última imagen. Ahora el juego deja dibujada una pantalla «MODO ACTUALIZAR» con la red,
la clave y la dirección antes de reiniciar. Pendiente: ver en el registro la línea «ACTUALIZACIÓN (modo Wi-Fi): ...»
para saber cuánta RAM hay y si la descarga funciona.
OJO: `prob_paso(x)` con x ≥ 1 da siempre 1; revisar otros usos con tasas > 1 si aparece algo parecido.

**1.0.11 (arreglo del modo Wi-Fi)** — el usuario grabó la 1.0.09 y al entrar a ACTUALIZAR: pánico en
`esp_wifi_init` (0x101 sin memoria). La pantalla gasta ~70 KB de RAM interna (inicio 123 KB → pantalla 53 KB). Ahora
Wi-Fi primero y pantalla solo si quedan ≥84 KB (si no, sin pantalla). Diagnóstico en RTC → evento
«ACTUALIZACIÓN (modo Wi-Fi): ...» en el registro del siguiente arranque. Clave de la red: ORBEPET1.
OJO aparte (no es del Wi-Fi): en el juego normal la RAM interna queda en 2-7 KB y el micrófono no abre
(«poca memoria DMA»). Es lo de siempre del punto 3 de «Qué falta».

**1.0.10 (huevo de 5 minutos; juego igual de rápido)** — se graba CON CABLE (cambia la tabla de particiones)
- El usuario NO quiere que nada quede más lento: el juego (core, ui.c, art.c, LVGL) va con -O2 como siempre. El
  espacio para el Wi-Fi salió de igualar ota_0/ota_1 a 0x7D0000 (0x20000 y 0x7F0000). Solo red y BT con -Os.
  Si algún día no cabe: lo grande es el arte (5,5 MB de .rodata en libmain); la salida de fondo es llevarlo a su
  propia partición (no hay más espacio para ranuras: deben quedar antes de los 16 MB).
- Primer huevo: 5 min reales (pet_egg_start al llegar al prado, UI_ACCION_HUEVO_EMPIEZA 187). Sin aspecto_ok el
  huevo no nace solo (pet_update). Prueba en test_core; en el simulador: sim_quick_huevo + sim_huevo_elegido.

**1.0.09 (actualizar por Wi-Fi; número de versión)** — se graba CON CABLE una vez (trae el arranque con vuelta atrás)
- `firmware/main/version.h`: FW_VERSION/FW_NOMBRE, único lugar. CMake la pone en esp_app_desc (PROJECT_VER).
- Ajustes: botón «VERSIÓN 1.0.09» bajo la cuadrícula → `panel_version` (PANEL_VERSION, prueba 84) → ACTUALIZAR
  (UI_ACCION_ACTUALIZAR 186): main guarda y `ota_pedir_modo()` (marca en RTC_NOINIT + reinicio).
- `ota.c`: al arrancar con la marca, `ota_modo()` en vez del juego (sin sonido, BLE, micrófono, registro). Red propia
  ORBEPET-XXXX / clave ORBEPET1, página en 192.168.4.1 (Wi-Fi de casa, buscar, subir .bin crudo por XHR, salir).
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

0. **Esperar el registro del usuario.** Va a instalar la 1.0.23 y correr en el programa del PC «Probar lo nuevo
   (guiada)» (o la PRUEBA TOTAL, que la incluye como fase 29d) y mandar `informe.txt` + `registro_placa_*.txt`.
   Revisar sobre todo: los 6 fondos (pj4 nuevo = 78145 bytes), los 6 juegos terminan y se cierran, tocar el faro y la
   piedra en la isla, **soplar la niebla, el olor y el bote** (si falla: mirar en el registro «micrófono: abierto (RAM
   DMA…)» y el evento «micrófono: esperó un soplido… lo más fuerte: N»; si N es bajo, bajar el umbral 34/24 en main.c
   paso 5c; si «NO pudo abrirse», es la RAM DMA), y la tabla de velocidad con y sin «juntar áreas» (FUSION 0/1).
1. **Rendimiento** (sección 3b): según lo que diga la tabla de velocidad del orbe.
2. **Pendiente del usuario «o la hormigas»** (sección 1): preguntarle dónde salió.
3. **Editor de personajes** (conversación del 17-nov): el arte nuevo es de 48 px (96 los grandes) y el editor dibuja
   íconos de 24×24. Acordado: primero abrir el dibujo de fábrica en 48×48 para retocarlo (`art_icono_lado_de_bytes`),
   después un editor por partes (colores, ropa, orejas, cola, expresión).
4. Batería: noche sin cable con sueño profundo (nunca probado); %/h con pantalla apagada.
5. Retoques que pida el usuario. Los pendientes viejos siguen en `claude/pendientes-v89.md`.

## 3b. Rendimiento: lo medido y lo que se propone (18-nov)

Lo que dice el registro del orbe (1.0.19): el juego normal va a **9-11 cuadros por segundo**; **dibujar** cuesta
~95 ms por cuadro y el **motor 1 ms**: todo lo que valga la pena está en el dibujo. No sirve cambiar de lenguaje (ya
es C, y LVGL también): serían meses, errores nuevos y nada más rápido.

Hecho:
- **1.0.22 — juntar áreas por redibujar** (`ui_inv_fusionar`, ui.c): LVGL guarda hasta 32 rectángulos por cuadro y al
  pasarse redibujaba la PANTALLA ENTERA (los 178 kpx/cuadro del registro). En el simulador: lluvia 72 → 3 kpx, nieve
  72 → 4, tormenta 72 → 11, noche en el prado 72 → 6, en el lago 74 → 16, bosque de noche 72 → 11. Se puede apagar
  por cable (`MASCOTA PROBAR FUSION 0`) para comparar en el orbe. Prueba: `sim/web/pruebas/redibujo.js [semilla]`.
- **1.0.22 — 25 KB más de RAM interna** (dram0.bss 80,3 → 55,2 KB): los búferes de las órdenes por cable a la PSRAM.
- Transparencias: medidas con `sim_alfa_scan` (simulador). Son pocas y chicas (mascota 80×80, animales 36×36, botones
  48×48, globo, bote): quitarlas no ahorra nada. **Descartado.**

Propuesto (en este orden; esperar la tabla de velocidad del orbe antes de decidir):
1. **Franjas de dibujo más grandes**: hoy 20 filas (`buffer_height` en
   `components/waveshare__esp32_s3_touch_amoled_1_75c/esp32_s3_touch_amoled_1_75c.c`, búfer doble en RAM interna:
   2×466×20×2 = 37 KB). Con los 25 KB liberados se puede subir a ~30 filas (56 KB): menos costo fijo por franja.
   Esperado 10-20 %. Riesgo bajo, pero OJO con la RAM interna (no bajar de ~15 KB libres en partida larga) y con el
   modo Wi-Fi (`s_liviano`: ahí 10 filas).
2. **Instrucciones vectoriales del S3 para mezclar píxeles**: Espressif publica rutinas de mezcla para LVGL que
   aprovechan el SIMD del ESP32-S3 (buscar en esp_lvgl_port / esp_lvgl_adapter la opción de dibujo con
   ensamblador propio, `LV_DRAW_SW_ASM_CUSTOM`). Es lo de más potencial (la mezcla varias veces más rápida) pero
   hay que verificar que funcione con LVGL 9.4 y el adaptador que usamos. Investigar primero, probar con el
   simulador no sirve (es WASM): solo en el orbe, con FUSION/VELOCIDAD para medir.
3. **Sprites en formato más liviano**: hoy ARGB8888 (4 bytes/píxel, en PSRAM, que es lenta de leer). RGB565A8 (3
   bytes) se ve igual y lee 25 % menos. Toca art.c (art_img_t) y todo lo que asume 4 bytes por píxel: cuidado.
4. **Adornos a menos cuadros por segundo**: el agua, los brillos y las partículas no necesitan moverse cada cuadro
   (la lluvia ya va a ~25 cps y de noche ~11: `s_vent_t` en ui.c).
5. Medir siempre con `MASCOTA PROBAR VELOCIDAD` (cuadros, ms, kpx, animar) y la prueba «RENDIMIENTO automático».

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
- No usar pedazos del prado ni del bosque en fondos nuevos de ZONAS (dibujos propios con `pixel_mundo.py`). Los fondos de
  los JUEGOS sí van sobre las ilustraciones de `fondos_zonas/` (pedido del usuario: «detallados como acostumbrados»).
- **El faro de los juegos es el MISMO de la isla** (recortado, sin espejo). Los juegos de cada zona salen PRIMERO en Jugar.
- **Pochi no va a la isla** (no sabe nadar) y **no llega con una visita** (espera que se vaya + 6 min).
- **Durante una actividad el mundo se apaga** (juegos, bote, comer, paneles, diario): ni Pochi ni globos; el baño no.
- **El micrófono viene apagado** y se abre solo mientras algo espera un soplido (niebla, olor, bote, velas, pesca).
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
cd build && cmake .. && make test_core && ./test_core          -> 1216/1216 comprobaciones OK
export WASI_SDK=/home/claude/sdk/wasi-sdk-25.0-x86_64-linux LVGL_DIR=/home/claude/lvgl BINARYEN=/home/claude/sdk/binaryen-version_123
sh sim/web/build.sh
node sim/web/pruebas/paneles.js          -> 18 problemas (la base de siempre)
node sim/web/pruebas/regresion.js        -> «dibujo: igual que la línea base» (--base para regrabarla)
node sim/web/pruebas/soak.js             -> soak: 180 días sin caerse   (~10 min)
node sim/web/pruebas/fondos_franjas.js   -> 1260 casos, 0 distintos
node sim/web/pruebas/inicio.js | libro.js | libro_flujo.js | final.js
node sim/web/pruebas/dos.js | isla.js | olor.js   -> juegos de a dos / de la isla / el olor: todo bien
node sim/web/pruebas/redibujo.js [semilla]        -> redibujo: todo bien (juntar áreas no deja restos)
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

En la placa: `herramientas/prueba_total.py` (fase 29c fondos por franjas; 29d lo nuevo 1.0.16-1.0.23),
`herramientas/prueba_nuevo.py` («Probar lo nuevo») y el programa `herramientas/mascota_gui.py`.

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
- Actualizar por Wi-Fi e informes: `firmware/main/ota.c`, `ota.h`, `version.h`, `.github/workflows/firmware.yml`
- Juegos de a dos y de la isla: `firmware/main/ui_dos.inc`, `ui_isla.inc`, `isla_mapa.inc` (generado),
  `herramientas/hacer_fondos_juegos.py` → `hacer_fondos_dos.py` (→ `firmware/recursos/pj*.bin`, `fondos_dos_sim.inc`)
- Documentación: `README.md` (una sección por firmware) y `claude/pendientes-v89.md`
