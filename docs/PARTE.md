# ACTUALIZACIÓN (12-nov-2026, 06:50) — firmware 100 entregado (zip), sin compilar para la placa

- El usuario instaló el 99 y mandó el registro: 10,1 %/h con pantalla apagada (~9 h) y no encendió al enchufar (modo reserva).
- Firmware 100: reserva arreglada (cada 20 s + VBUS), etapa 3 sueño ligero (`dormitar()`, `BOARD_SUENO_LIGERO`, tickless idle:
  **hay que borrar firmware/sdkconfig**), menos escrituras a flash con la pantalla apagada. Ver README «Firmware 100» y
  `claude/plan-bateria.md`. Pruebas del simulador OK (test_core 1063, paneles 18, regresión igual).
- No se puede compilar en el PC del usuario por control remoto (las terminales no aceptan que Claude escriba). Esperar su
  informe: errores de compilación, `ENERGIA` (ligero/dormitadas) y el nuevo %/hora.

# ACTUALIZACIÓN (11-nov-2026, 19:15) — firmware 99 en curso (lo de abajo es el parte del 98)

- **Herramientas**: LVGL 9.4 en /home/claude/lvgl (`git clone --depth 1 -b v9.4.0 https://github.com/lvgl/lvgl.git`: GitHub sí
  responde); binaryen 123 en /home/claude/sdk (release de GitHub); wasi-sdk: si falla libclang-cpp, `tar xzf wasi.tgz` de nuevo.
- **Hecho en el 99** (todo probado: test_core 1063/1063, paneles 28, regresión regrabada, soak 180 días):
  1. `textos_del_juego_v98.docx` (3.595 textos numerados; herramientas/textos/). El usuario los revisa y devuelve.
  2. Personajes y animales en pixel art 48 px, sin base (gen_pixel48.py + pix48.py; Peumo = Cuidador; pingüino nuevo).
  3. Batería: P0 + arreglo `prob_minutos`; etapa 1 (reposo del panel y táctil, `BOARD_PANEL_REPOSO`) y etapa 2 (atenuar a
     los 8 s sin tocar + LVGL 66 ms + CPU 80 MHz, `ui_reposo_visual()`); prueba_total fase 2 con 50 ciclos y atenuar.
     main.c/services.c/BSP NO se pudieron compilar aquí (no hay ESP-IDF): lo compila el usuario.
  4. Textos «obvios» (describen lo que se ve): la hoja con letra chueca y cucharita ahora se DIBUJA (`pagina_vieja()` en
     ui_final.inc: final y Libro pág. 4); LUGAR_COSAS solo con lo que el dibujo no cuenta; cuchara de estrellas; objeto 110.
  5. Pecera: duración elegible (10/30 min, 1 h, nunca; dispositivo v11). Paneles con páginas y deslizar (`paginas()`): alarma
     y Pantalla en 2 páginas; flechas redondas centradas. La hoja de Peumo lleva texto legible (el usuario rechazó las rayas:
     «si había texto hay que poder leerlo»). **Paneles: la base ahora es 18 problemas** (no 28).
- **Pendiente**: el usuario revisa el documento de textos (más textos obvios: ceremonias 7-097, 7-109..113, etc.); medir en la
  placa y llenar la tabla del plan; luego etapa 3 (firmware 100). No entregar zip ni simulador hasta que lo pida.

# PARTE — estado del trabajo (11-nov-2026, 14:30)

Proyecto: mascota virtual en un orbe ESP32-S3 (AMOLED redonda 466×466, LVGL 9.4). Responder siempre en español.
Versión en curso: **firmware 98** (partida **versión 41**). Hecho y probado en el simulador; **todavía no entregado** (ni zip ni simulador publicado).
Respaldo del código: `mascota_firmware_98.zip` (en las salidas de la sesión; mismo formato que el 97). Si la sesión nueva no lo tiene, pedirlo / subirlo.

---

## 1. Objetivo actual y cómo comprobarlo

**Objetivo (pedido del 11-nov, 07:13–07:56):**
- Que **el claro del Rey Peumo** y **el mirador** sean **zonas del mundo vistas desde arriba**, como el prado (con animales, visitas, clima, flechas, mapa), y no pantallas de frente aparte.
  - Claro: basado en la idea del prado: **una pequeña poza, la casa del rey a la que se entra al tocarla** (como la casita-tocón del prado, pero en **otra disposición**) y sus animales. **Dibujo nuevo**: NO usar pedazos de las imágenes del prado/bosque (el usuario lo rechazó explícitamente).
  - Mirador: **justo sobre la cueva**, vista desde arriba, que se entienda que es **la cima de una montaña** (el usuario rechazó una versión que «parecía una isla»).
- **Juegos entretenidos y repetibles** (los de la v97 eran «fomes»): los cuatro elegidos (Galletas de Peumo, Salto al cielo, Atrapa hojas, Canción de cuna + Piedra tibia), con puntaje, récord, 1–3 estrellas, premios, rondas cortas, dificultad creciente y versión infante.
  - Salto al cielo: **nubes más chiquitas** y **saltar hacia los lados** (no siempre recto).
  - Galletas: **el fuego se mantiene soplando** (micrófono; el fuelle queda de respaldo).

**Cómo comprobarlo:**
- Motor: `cd build && make test_core && ./test_core` → debe decir **1054/1054 comprobaciones OK**.
- Simulador (compilar): `export WASI_SDK=/home/claude/sdk/wasi-sdk-25.0-x86_64-linux LVGL_DIR=/home/claude/lvgl BINARYEN=/home/claude/sdk/binaryen-version_123 && sh sim/web/build.sh`
- Paneles: `node sim/web/pruebas/paneles.js` → **28 problemas** (es la línea base de siempre).
- Regresión: `node sim/web/pruebas/regresion.js` (la base ya se guardó con el firmware 98: 79 paneles).
- 180 días: `node sim/web/pruebas/soak.js` (~10 min) → «soak: 180 días sin caerse».
- Sintaxis de la UI: `gcc -fsyntax-only -std=gnu17 -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers -Wno-sign-compare -DLV_CONF_INCLUDE_SIMPLE -Isim/preview -I/home/claude/lvgl -Icore -Ifirmware/main firmware/main/ui.c` → sin avisos.
- A ojo en el simulador: botones «Novedades (v98)»: Claro del rey, Casa del rey, Mirador, Subir en la nube y los 5 juegos.
- En la placa: fase **28b** de `herramientas/prueba_total.py` (claro, casita, 5 juegos con `TRAMPALUGAR 1`, viaje en la nube, bajar por el sendero).

---

## 2. Decisiones tomadas, qué cambió y qué pruebas pasaron

**Decisiones (respuestas del usuario):**
- Lugares = «Escenas reales»: el claro y el mirador son zonas de verdad (ESC_CLARO=6, ESC_MIRADOR=7), partida v41 con conversión automática.
- Juegos = los cuatro propuestos + piedra tibia; todos repetibles; el secreto de cada cosa se descubre la primera vez.
- Rehacer todo el arte sin copiar el prado/bosque (dibujo procedural nuevo en pixel art); la casa del rey es un sprite propio (tocón de peumo) como la casita del prado, puesta en otra posición.
- No entregar zip ni simulador hasta que el usuario lo pida (ahorrar créditos). No dañar nada de lo existente. Probar todo antes de informar.

**Qué cambió (firmware 98):**
- **Motor (core/)**: `escena_t` agrega ESC_CLARO, ESC_MIRADOR (ESC_COUNT 8); `ESC_ES_SECRETA(e)`; `pet_scene_open` los abre desde `lugares` (bit PET_ZONA_CLARO=2 para el claro, bit 1 para el mirador); nombres, gustos, VIV_ZONA, ZONA, animales que viven ahí (máscaras Z_K/Z_M), animales ×2.5 más seguido en esas zonas; nunca va sola (IA y Pochi las excluyen); no trepa allá. **v41**: campos `jl_record[5]`, `jl_estrellas[5]`, `jl_premios[5]`, `jl_dia`, `jl_jugadas`; API `pet_place_game_result/record/stars/prize/seed` en `core/v41.inc`; premios: 2 rondas/día por juego (1★ ramita, 2★ +musgo, 3★ 2 ramitas+musgo, primera vez 3★ un regalito). Tests nuevos `test_v41`.
- **Fondos**: `herramientas/pixel_mundo.py` (pinceles) + `herramientas/dibujar_zonas_v98.py` → `fondos_zonas/claro_*.png` y `mirador_*.png` (4 estaciones, pixel 4×); empaquetados con `herramientas/hacer_fondos.py` (modo pixel, 128 colores) en `fondos_data.inc` (FD_6_x, FD_7_x; FD_TABLA[8][4]). ~330 KB.
- **Arte**: `ART_ESCENAS 9`, `ART_ESCENA_CASA 8` (antes 6), filas nuevas de ART_LUGAR_XY; íconos `ICO_ZONA_CLARO/MIRADOR` (medallones en iconos_hd2.inc). Quitadas las pantallas 6 y 7 viejas (en `fondos_pantallas/antes_v98/`, −117 KB) y los íconos 48 px repetidos (`herramientas/podar_iconos_hd.py`, −100 KB). Neto ≈ +140 KB.
- **UI**: `ui_zonas.inc` reescrito: la casita sigue como pantalla propia (X sale al claro por la puerta); en el mundo: cosas tocables del claro (casa→entrar, poza, hongos, tronco, huerto) y del mirador (piedra, corona, nube→juegos; cóndor, apacheta, grieta), luciérnagas, chucao, cóndor que planea, lo escondido de hoy; nube del cerro sube al mirador (hook `lw_nube_desde_colina`, llegada directa con `s_lw_directo`); sendero de luciérnagas del bosque; excursiones como antes. Tablas por zona ampliadas en ui.c (COPAS, ICO_ZONA, NOM del mapa, VECINO: bosque-oeste↔claro, mirador-sur→cerro, PUNTO_BORDE, ARBOL_TREPA, LUZ, textos, CACA_SITIOS, límites para caminar: casa/poza del claro y círculo de la cima), ui_historia (perchas), ui_pochi, ui_objx.
- **Juegos**: `ui_zjuegos.inc` reescrito (5 juegos, portada con récord/estrellas, HUD, barra de tiempo, final con estrellas, premios, OTRA VEZ y X; reloj 33 ms; soplido por micrófono en Galletas vía `zj_quiere_mic`/`zj_soplido`). Acción nueva `UI_ACCION_JUEGO_LUGAR 182` (main.c y web_main.c llaman a `pet_place_game_result`).
- **Pruebas/herramientas**: main.c (ENTRAR, JUEGOLUGAR, NUBE, LUGARINFO con `vista=`/`escena=`), `ui_escena_vista()`, prueba_total fase 28b reescrita, mascota_gui.py y plantilla.html con «Novedades del firmware 98», web_main.c `sim_zona` teletransporta, exports `sim_lw_dbg`, `sim_hit_dbg`. README sección «Firmware 98». Doc del proyecto `claude/pendientes-v89.md` actualizado.

**Pruebas que pasaron:** test_core 1054/1054; paneles 28 (base); regresión: 0 diferencias visibles contra la v97 (base regrabada); soak 180 días; suites inicio/libro/libro_flujo/final sin error; los 5 juegos terminan (adulto, trampa e infante); entrar/salir de la casa del rey; viaje en la nube cerro→mirador; luciérnagas bosque→claro; mapa con las zonas nuevas; animales en las zonas nuevas.

---

## 3. Qué falta y en qué orden exacto

1. **Esperar la respuesta del usuario** a «¿te armo el zip y el simulador?». Mostrar la vista previa (`vista_previa_v98.png`) si la pide de nuevo.
2. Cuando lo pida: **zip** `mascota_firmware_98.zip` (mismo contenido que el 97: todo `src/` sin `build/` ni `build_lv/`) y **republicar el simulador** en el artefacto existente slug `a282698c-8d8d-4547-aba0-6093fb884917` (título «Simulador Mascota v98»): `index.html` hecho desde `sim/web/plantilla.html` (quitar el bloque __MOTOR_JS__/__WASM_B64__, cargar `mascota.wasm` y, si falla, `motor.txt` = gzip+base64 de motor.js) + `mascota.wasm` + `motor.txt`, publicado desde /home/claude/web con `publish_files` (pedir `list_files`/`get_artifact` primero para base_version).
3. Con el informe de la placa: revisar que **el firmware quepa en la partición** (ota_1 = 0x7A0000), el **soplido real** en el horno, la fluidez de Salto al cielo y la fase 28b. Si no cabe: quitar la primavera propia del claro/mirador (alias al verano en FD_TABLA, −80 KB) o bajar colores.
4. Retoques que el usuario pida a los dibujos o juegos (los pendientes viejos siguen en `claude/pendientes-v89.md`: registro de la placa, prado, ideas Bluetooth, Año Nuevo).

---

## 4. Restricciones (no tocar / no romper)

- **No dañar nada de lo que ya existe**: tras cada cambio correr test_core, paneles (28), regresión y, si toca el motor, soak.
- **No usar pedazos del prado ni del bosque** en los fondos nuevos (dibujos propios en `pixel_mundo.py`).
- **No entregar zip ni simulador hasta que el usuario lo pida.** Informar solo cuando todo funcione.
- Los números de enum ya guardados en partidas no se reordenan (escenas 0–5, íconos se agregan al final de la lista X de art.h, acciones UI con ids nuevos libres).
- `PET_SAVE_VERSION` 41: cualquier campo nuevo va al final, antes de `crc`, con conversión en `pet_upgrade`.
- La pantalla de fondo en uso no se suelta (`art_pantalla_en_uso_fn` / `zona_usa_pantalla`); un juego del mundo no empieza encima de una zona.
- Taps en las pruebas: ≥4 cuadros apretado. Tamaños: PANT_MAX 16 (pantallas 8–12 = fondos de los juegos: 8 nubes, 9 viento, 10 piedra, 11 horno, 12 cama; 5 = casita).
- Textos en MAYÚSCULAS vía `gtxt`; nombres: el Rey es **Peumo** (antes Tilo); galletas de **piñón con miel de ulmo**.

---

## 5. Archivos clave y próximo paso concreto

**Archivos clave** (en `/home/claude/src`):
- `core/pet.h`, `core/pet.c`, `core/v41.inc`, `tests/test_core.c`
- `firmware/main/ui.c`, `ui_zonas.inc` (zonas, casita, cosas del mundo, nube, excursiones), `ui_zjuegos.inc` (los 5 juegos), `ui.h`, `main.c`, `art.c`, `art.h`, `fondos_data.inc`, `pantallas_data.inc`, `iconos_hd.inc`, `iconos_hd2.inc`
- `herramientas/pixel_mundo.py`, `dibujar_zonas_v98.py`, `hacer_fondos.py`, `hacer_pantallas.py`, `podar_iconos_hd.py`, `iconos_nuevos/iconos_chile2.py`, `prueba_total.py`, `mascota_gui.py`
- `sim/web/web_main.c`, `sim/web/plantilla.html`, `sim/web/pruebas/*.js` (ayuda de pruebas en `/tmp/prep.js`: `prep(edad)`)
- `README.md` (sección Firmware 98); doc del proyecto `claude/pendientes-v89.md`

**Próximo paso concreto:** preguntar/esperar si el usuario quiere el zip y el simulador del firmware 98; si dice que sí, hacer el paso 2 de la sección 3 (zip + republicar el artefacto `a282698c-…` como «Simulador Mascota v98») y enviarle ambos.
