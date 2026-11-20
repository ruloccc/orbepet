# PARTE — estado del trabajo (19-nov-2026, 22:45)

> **1.0.35 «ALARMAS EN LA PECERA» (19-nov 22:45).** (a) Dock: el «+» (s_dock[3]) se ve dormida aunque s_interior o
> s_juego (mas_dormida), y se trae al frente en la casita. (b) Pecera: s_pecera_rit abre panel_alarmas (ICO_DESPERTADOR,
> pecera_alarmas_cb); alarmas_cb 21 (volver) con s_pecera cierra el panel y la pecera sigue. (c) ritmo_revisar no se
> abre con s_pecera. Regresión regrabada (version, mas-ajustes, rito-oro).


> **1.0.34 «EL LIBRO ORDENADO» (19-nov 21:20).** Pedido: tocar la tapa del Libro abre la hoja HISTORIAS/CANCIONES (antes
> abría el índice y la hoja estaba deslizando a la izquierda). ui_magia.inc: lib_abre_cb → panel_libro_hoja (con
> volver → tapa); libro_desliza_cb ya no cambia de hoja; sin lib_puntos. s_lib_origen (0 Libro, 1 Baúl) y s_can_origen
> (ui.c; 1 Libro, 2 Baúl, 0 cristales) para la flecha de volver del índice de narradores y de las canciones.
> Regresión regrabada (libro-narradores, libro-portada, libro-hoja2, version, mas-ajustes).
> Faro (pedido 21:18, «sigue injugable»): ui_isla.inc s_zf_pegado/ox/oy: al tocarlo el haz queda pegado (offset a la
> punta, ≤ 40 px y se va juntando), va a ax+ox a v = min(7·d, 240 px/s); pegado no encalla (se frena en la orilla, prueba
> por eje). Sin anillo s_zj_o[29]. s_zf_empieza/s_zf_solto como el buceo; s_zj_juego_t se reinicia al empezar. isla.js
> nueva parte: lleva la luz al primer barquito y lo arrastra al muelle (sale 1 punto).


> **1.0.33 «MEMORIA LIMPIA» (19-nov 20:10).** Idea del usuario: liberar cada cierto tiempo. En ui.c, en la vigilancia de
> memoria (cada 1,5 s, solo ESP_PLATFORM): cada 10 min y 30 s después de apagarse la pantalla, si está en calma (sin
> panel, zj, pesca, picnic ni juego), lv_image_cache_drop + iconos_soltar + pantallas_soltar + art_peces_soltar. NO
> art_fondos_soltar_otros (v88: un fondo en pantalla quedaba apuntando a memoria liberada). Evento «memoria: limpieza de
> rutina (…): soltados X KB; PSRAM a -> b KB, bloque mayor c -> d KB» solo si soltó > 128 KB o una vez por hora (la
> flash es lenta). Revisado con -DESP_PLATFORM y cabeceras de mentira (/tmp/espstub). Si con esto la PSRAM igual baja
> con las horas, lo que se pierde NO son dibujos guardados: buscar en otro lado (los eventos dirán cuánto suelta).
> Además (pedido 20:06 y 20:24): la NUBE de pensar() es un dibujo (nube_pintar/nube_dist en ui.c: caja con puntas
> redondas + bolitas arriba/abajo + una a cada lado, borde suave, se repinta al tamaño del contenido) en s_pens_fondo
> (hijo FLOATING en (-pad,-pad); s_pens transparente con pad NUBE_M+2). En adulto, sin la carita (s_pens_emo oculto).


> **1.0.32 «LA COLECCION DE PECES» (19-nov 19:40).** Pedido 18:45: colección de ~30 animales al pescar. core/peces.inc
> (incluido en pet.c): tabla PECES[30] (nombre, con artículo, descripción, tipo PZT_PEZ/CANG/RARO, estaciones, horas
> 6-11/11-18/18-21/21-6, clima, lago congelado sí/no/igual, peso). pez_elegir() en aplicar_pesca para PESCA_PEZ,
> PESCA_CANGREJO y PESCA_PEZ_DORADO (minijuego y pesca sola), ×1,6 a los que no tiene; el pez arcoíris (28) solo con
> p->raro == RARO_ARCOIRIS. Guardado en peces_a[2] y peces_b[2] (eran _res38 y _pad40: parten en cero, misma versión 42).
> EV_PESCA b = comió | (1+cuál)<<1 | nuevo<<6 (ojo: antes b era solo «comió»); DIA_PESCA b = 1+cuál. Dibujos 48 px:
> herramientas/iconos_nuevos/peces48.py → firmware/recursos/pj7.bin (ver OJO abajo), art_pez(k, escala, gris) con caché que se suelta
> en cerrar_panel y pez_cerrar (art_peces_soltar). UI: celebración de la pesca con el animal y «¡NUEVO EN TU COLECCIÓN!»;
> globo de la pesca sola; Cofre: 9.º botón PECES (s_cofre_vista 3, peces_cb → nota_mostrar_src con el dibujo); panel de
> prueba 90 «peces». Pruebas: test_core 1490 (bloque 1.0.32), pruebas/peces.js nueva (sim_pez_dar, sim_pez_forzar,
> sim_peces). Todo verde; regresión regrabada (version, mas-ajustes, baul y peces nuevo).
> **OJO, ESPACIO:** con los dibujos dentro del programa la compilación falló (mascota.bin 0x7da8a0, la partición es
> 0x7d0000: faltaban 43 KB). Se movieron a la memoria de recursos: pj7.bin (formato 'PZ', LZSS, ~25 KB;
> peces48.py lo escribe y hacer_fondos_dos.py lo mete en fondos_dos_sim.inc para el simulador). art_pez lo lee con
> dos_leer(7) al primer uso; si falta, dibuja ICO_PEZ/ICO_CANGREJO. Al programa le quedan ~30 KB libres: lo grande
> nuevo (dibujos, fondos) tiene que ir a recursos.


> **1.0.31 «EL TOBOGAN DEL ARCOIRIS» (19-nov 18:40).** (a) El usuario aclaró el buceo: se abre con la piedra izquierda Y
> con el agua de encima hasta la orilla (isla_toque: elipse de la piedra (45,170) 40×21 + agua y 84..160 con x < orilla,
> tabla ORILLA medida en la pantalla de la isla). (b) Juego nuevo ZJ_ARCOIRIS = 12 (ui_arcoiris.inc, prefijo zr_, en el
> marco zd: zd_es llega hasta 12; portada/fondo/empezar/animar/terminar con sus casos). Perspectiva: medio ancho
> ZR_HW0 + (y − ZR_HY)·ZR_HWK (34, 64, 0,24; igual que ARCO_* en hacer_fondos_juegos.py), ella en y 378, cosas en
> s_zj_o[2..13], rayitas 14..21, barrita 24..26, corazones 30..32. Se abre con arco_toque() en fondo_cb (radio 216-308
> alrededor de (CX,360), y<330, solo con el arcoíris visible) y con Jugar (botón 12; con 8 botones van 4 + 4). Fondo
> pj6.bin (19_arcoiris.png; `python herramientas/hacer_fondos_juegos.py arcoiris` y `hacer_fondos_dos.py`), PANT_MAX 23.
> Motor: PET_ISLA_ARCOIRIS = 2 en pet_island_game (diversión, curiosidad; sin concha) y texto del diario. Pruebas:
> sim_zd_abrir(6), sim_zr(q), pruebas/arcoiris.js nueva; isla.js prueba 7 puntos de la entrada al buceo. Todo verde
> (test_core 1227, paneles 18, regresión regrabada solo por «version», soak 180 días).
> (c) Fotos 19-nov 21:26: DOS canastas en el picnic = art_pantalla_clima(3) no cupo → art_pantalla(2) (tapete con la
> canasta PINTADA) + el ícono del botón. Ahora con la 2 se oculta s_pic_canasta_ico y el botón va a (50,270) sobre la
> pintada. Pesca celeste = art_cruce_fondo no cupo (428 + 212 KB). Nuevo hacer_lugar(para, kb) en ui.c (caché, íconos,
> pantallas_soltar, art_fondos_soltar_otros; evento «memoria: se hizo lugar para …») antes del picnic y de la pesca, y
> un 2.º intento si falla. Esto tapa el síntoma: la PSRAM que se pierde con las horas sigue pendiente. (d) La Bolsa: en
> obj_panel_cb «volver» sin venir de Tesoros llamaba panel_bolsa(); ahora panel_cofre con la categoría del objeto
> (OCAT → cofre_sub {0,1,3,2,4}); el botón 30 «ABRIR LA BOLSA» va a la cuadrícula de Tesoros.


> **1.0.30 «POCHI SORPRESA» (19-nov 17:45).** (a) Rayas oscuras con lluvia junto a los botones (foto del usuario):
> posible causa en `ui_inv_fusionar` (1.0.22): LVGL manda un INVALIDATE_AREA de prueba (get_max_row) con
> `rendering_in_progress`; la fusión lo juntaba con áreas pendientes. Ahora `if (d->rendering_in_progress) return;`.
> NO reproducido en el simulador (web_main.c ahora tiene modo franjas: sim_perfil copia las franjas a s_fb y
> sim_franjas_impares cuenta franjas mal alineadas o de más de 466×20; dio 0 con y sin el arreglo). Si siguen: pedir
> foto/informe. (b) Buceo (ui_isla.inc): control relativo (s_zb_tenia/dx0/px0: ella se mueve como el dedo ×1,4 desde
> donde está), espera s_zb_empieza hasta que vence el texto o un toque NUEVO (s_zb_solto: el toque de la portada no
> cuenta); al empezar reinicia s_zb_otra/s_zb_burbuja. (c) isla_toque: buceo solo en la piedra, círculo r26 en (39,165).
> (d) Pochi (ui_pochi.inc): s_po_tipo PT_COMPLETA/PAPAS/PASEO/TAPITA/SALUDA, elegido en po_empezar (28/20/18/18/16 %,
> no repite; primera visita o sentida → completa; saluda sin cachorros → pc_solo_uno). po_tras_llegar() reemplaza
> «po_saludo(); po_hambre();». Visitas sin paseo: PO_INVITA no invita, se queda 14 s y PO_DESPIDE. Fase nueva PO_TAPITA
> (po_tapita_paso, toque por fondo_cb → po_tapita_toque). Memoria al irse: ULT_COMIO solo si venía a pasear.
> Pruebas: ui_demo_pochi 120+tipo, 125 tira la tapita; sim_po_estado(); pruebas/pochi_visitas.js nueva; isla.js con
> buceo_instrucciones/buceo_deslizar y toque lejos de la piedra. Todo verde (test_core 1224, paneles 18, regresión
> regrabada: cambian version y mas-ajustes por el número, redibujo, franjas 1260/0, dos/isla/olor, soak 180 días).
> (El juego del arcoíris que se proponía aquí se hizo en la 1.0.31.) OJO: el punto (c) de arriba quedó mal entendido:
> el usuario quería la piedra Y el agua de encima; se corrigió en la 1.0.31.


> **1.0.29 «BATERIA QUE DURA» (19-nov 13:45).** Informe #5 de orbepet-informes (1.0.27, mandado desde el orbe): batería
> 58 % → 14 % en 75 min = 35 %/h, con la pantalla encendida todo el rato a brillo 100, 15 cuadros/s, CPU 240 MHz, sin
> toques en el registro. Causa: main.c, después de los eventos, `if (avisar …) lv_display_trigger_activity()` reiniciaba
> la inactividad con cada EV_PIDE/EV_FRASE/… : con ella pidiendo seguido (energía 20) la pantalla nunca se apagaba.
> Arreglo: el aviso solo despierta la pantalla APAGADA, a lo más cada 5 min (evento «energia: un aviso encendió la
> pantalla»). Otros datos del informe: «escritura lenta a flash: el registro tardó ~1,6-1,9 s» cada 5 min (punto B1 de
> 3b, sigue pendiente), un «memoria baja» a PSRAM 888 KB. Se puede leer los informes con
> `gh api repos/ruloccc/orbepet-informes/issues/N -q .body` (add_repo orbepet-informes funcionó en esta sesión).

> **1.0.28 «LO QUE PIENSA» (19-nov ~13:30): hechos 0000 (nube de pensamiento con texto) y 0001 (cuadrícula en Tesoros,
> Bolsa y Más).** pensar() en ui.c: texto en s_pens_txt dentro de la nube, sin cola; enc_rejilla() convierte una
> area_texto en cuadrícula; cofre_objetos y la Bolsa usan enc_celda. Pruebas: ui.c -fsyntax-only sin avisos,
> test_core 1224/1224, paneles 18, regresión (cambian mas-recuerdos, mas-ajustes, version) regrabada, dos/isla/olor/
> redibujo bien. Nuevo en el simulador: sim_pensar(e, c) y sim_llenar(). Publicación: igual que la 1.0.27 (falta acceso
> a orbepet-firmware o el secreto); se entregó zip para el cable.

> **1.0.27 «TODO EN SU LUGAR» (19-nov ~11:30): hecho TODO lo de la lista 000–000h** (ver README 1.0.27). Esta sesión SÍ
> tuvo GitHub (add_repo ruloccc/orbepet después de que el usuario pidió subir) y se bajaron LVGL, wasi-sdk y binaryen:
> ui.c pasa -fsyntax-only sin avisos, test_core 1224/1224, paneles 18 (la base), redibujo, fondos por franjas 1260/0,
> dos/isla/olor bien, regresión regrabada (cambios esperados: versión, fotos, verso-* de la 1.0.25c y 5 paneles nuevos
> 85-89: libro-portada, libro-hoja2, baul, encic-flora, diario). La 1.0.26 la compiló el usuario en su PC y anduvo.
> Los puntos 000–000h de abajo quedan como historia de lo pedido.

> **19-nov 10:00: la 1.0.26 COMPILÓ en el PC del usuario (idf.py, ESP-IDF 5.5.6) y arrancó.** Foto: huevo nuevo y al lado
> una criatura verde con flor = la visita (s_visita) de la mascota anterior, que solo se iba con EV_VISITA_SE_VA.
> Arreglado: `otra_mascota_limpiar()` (ui.c, junto a esconder_vecino) al crear/cambiar y si la mascota es huevo.
> Dato del registro: al volver de la isla al prado (24 s tras arrancar) la PSRAM quedó en **153 KB (bloque 38 KB)** y
> subió a 1944 KB recién al crear la mascota nueva: el paso isla → prado retiene ~1,8 MB. Mirar eso (fondos de la isla /
> pantallas guardadas) cuando se investigue la memoria.
>
> **19-nov 9:40, el usuario aclaró: la noche lenta fue SIN cable.** Causa encontrada: el sueño profundo de la noche pide
> `pet_is_bedtime` y una mascota nueva tiene `dormir.modo = DORMIR_NO` → nunca dormía profundo. Arreglado en la 1.0.26
> (`noche_profunda`: sin hora elegida, 22:00-7:00). En el próximo informe buscar «energia: SUEÑO PROFUNDO de noche».
>
> **Sesión del 19-nov mañana (zip 1.0.26 «NADIE SE PIERDE», sin subir a GitHub: la sesión no tuvo permiso para el repo
> ni para leer los informes).** Se perdió Tova de Perfiles tras una noche lenta. Arreglado (ver README 1.0.26):
> guardado seguro de m*.bin (`hal_res_save_seguro` + arreglo en `hal_res_init`), cambio de perfil anotado en
> `mcambio.bin` (`cambio_reparar` al arrancar), la que se va «DE VIAJE» queda en `adios.bin` y puede volver
> (UI_ACCION_VOLVER_VIAJE 192, `pet_traveller_back`), guardado en segundo plano numerado, respaldo del PC con «recursos»,
> reinicio de mantenimiento con PSRAM < 900 KB por 10 min. Además: faro con barquitos «guiados», huevo sin «NACERÁ…» y
> con meneo al tocarlo, gorrito de dormir oculto en el huevo, textos del Libro, colchoneta en la siesta, aviso de cómo
> despertarla. Probado aquí: test_core 1223/1223 y dos arneses en el PC (guardado seguro: 8 casos; cambio a medias: 4
> casos). NO se pudo compilar el firmware ni correr el simulador (el zip no trae `sim/` y no hay LVGL ni ESP-IDF): lo
> primero es la compilación de GitHub. Falta saber: qué se come la PSRAM de noche (pedir un informe tras una noche con
> cable) y si la mascota dormida sin colchoneta del 19-nov era la siesta (ahora con colchoneta) u otra cosa (pedir foto).

> **Sesión del 18-nov 19:10-19:45:** se revisaron los informes (prueba total 17:10, rendimiento 18:39 y 19:06,
> registro 18:30) y se arregló el probador (solo herramientas, sin firmware nuevo): **3a hecho** (respaldo en
> Documentos) y **3b-A hecho** (medir despierta). La sesión no tenía `git`: los archivos se subieron editándolos en
> la web de GitHub. Falta que el usuario corra «RENDIMIENTO automático» y «Probar lo nuevo» con el probador nuevo
> (ya tiene los 3 archivos). Siguiente firmware (1.0.24): la despedida lenta (punto 1 de la sección 3) y la flash.

Proyecto: mascota virtual en un orbe ESP32-S3 (AMOLED redonda 466×466, LVGL 9.4, ESP-IDF 5.5). **Responder siempre en
español.** El usuario casi no puede usar el PC: todo se publica para actualizar por Wi-Fi desde el orbe. «Deja de romper
cosas»: tras cada cambio, las pruebas de la sección 5. Nada puede quedar más lento.
Versión publicada por Wi-Fi: **1.0.24** (falló por Wi-Fi, ver 0a). En el orbe, por cable: 1.0.26. En GitHub (compilada,
sin publicar): **1.0.35** («ALARMAS EN LA PECERA»), que trae de la 1.0.27 a la 1.0.34. Partida **versión 42** (no cambia desde el 104).

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

000. **PRÓXIMA REVISIÓN — pedido del usuario 19-nov 10:16: crear mascota nueva entra en un bucle.** Perfiles → huevo
   nuevo → elige huevo → pone la fecha y su cumpleaños → vuelve a la pantalla inicial y a elegir huevo otra vez. Mirar el
   camino de UI_ACCION_NUEVA (main.c: pet_new_fresh + `edad_dueno = 0`, `dueno_dia/mes = 0`) y en ui.c la secuencia del
   inicio (vivero → hora → cumpleaños → país → `s_asp_listo` → ASPECTO/GENERO/HUEVO_EMPIEZA): qué condición devuelve al
   vivero (¿se pierde el aspecto elegido, `aspecto_ok`, al guardar la fecha? ¿UI_ACCION_HORA/poner_hora reinicia algo?).
   Pedir el registro de ese momento. Revisar también si lo provoca algo de la 1.0.26 (cambio_reparar, otra_mascota_limpiar).
000b. **PRÓXIMA REVISIÓN — el texto nuevo de la 2.ª página del Libro no cabe** (foto 19-nov 10:16: se corta en «AHORA TE
   TOCA DESCUBRIRME Y», el SIGUE tapa el resto). Son 198 letras y antes eran 184. Propuesta, sin aplicar (el usuario pidió
   esperar), de 176 letras: «GUARDÉ LISTAS DE COSAS POR CUIDAR, RECETAS DE GALLETAS, CHISTES E HISTORIAS.» / «MI DUEÑO
   ENFERMÓ Y QUEDÉ OLVIDADO EN UN RINCÓN, HASTA HOY.» / «AHORA DESCÚBREME Y COMPLÉTAME CON TU VIDA.» Mejor aún: que la
   página del Libro se ajuste sola (letra más chica o dos hojas si no cabe). Revisar también la 1.ª página (158 letras).
000c. **PRÓXIMA REVISIÓN — textos que llegan tarde (foto 19-nov 10:21: «¿QUÉ LE DAMOS?» sale DESPUÉS de darle la comida).**
   Causa vista en el código: picnic_abrir() (ui.c, ~8565) llama globo("¿QUÉ LE DAMOS?") con el picnic ya abierto;
   mundo_en_pausa() es true durante el picnic, así que globo() lo guarda en la cola s_gpend y seguir_globos() lo suelta
   cuando el picnic termina. Pasa con CUALQUIER globo dicho dentro de una actividad que pausa el mundo (juegos, bote,
   picnic, paneles) o mientras habla Pochi. El usuario: «no debería pasar en ningún lado». Arreglo propuesto:
   (a) cada globo en cola guarda cuándo se pidió y la escena/actividad; si espera más de ~6 s o cambió la actividad, se
   descarta (los avisos importantes que sí deben esperar, marcarlos aparte); (b) los textos propios de la actividad
   (como el del picnic) se muestran dentro de ella (s_po_bypass o un globo de la actividad), no en la cola del mundo.
   Buscar todos los globo() llamados justo después de abrir algo que pausa el mundo.
   **Decisión del usuario (10:22): en la comida NO va ninguna frase** → borrar el globo de picnic_abrir() (el título
   «¿QUÉ LE DAMOS?» de la tarjeta de comidas, ui.c ~1769, se queda). El arreglo general de la cola igual hace falta.
000d. **PRÓXIMA REVISIÓN — álbum de fotos (decisión del usuario 19-nov 10:34: «segunda»).** Hoy todas se ven iguales
   (dibujar_foto en ui.c ~9739: marco 120×104 con degradé, ícono de zona al 40 %, mini criatura y un iconito en la esquina,
   casi siempre el corazón) y repiten el diario. Se decidió: **pocas fotos, y cada una distinta.** Solo momentos grandes
   (DIA_NACIO, DIA_CRECIO, DIA_SU_CUMPLE, DIA_CUMPLE_DUENO, DIA_BALLENA, DIA_FIESTA, DIA_FINAL; ver si DIA_GUARDIANA,
   DIA_FAMILIA y DIA_ESTRENO también) con un dibujo propio grande en el centro (pastel, ballena, guirnaldas…) y los
   colores reales de la zona, en vez del corazón en la esquina. Los demás motivos dejan de sacar foto (siguen en el
   diario). En core/pet.c, `foto(p, DIA_…)` se llama en 25 lugares: filtrar ahí o en foto() con una lista de motivos;
   las fotos viejas que ya están guardadas se pueden dejar o filtrar al mostrarlas. Las fotos de la CÁMARA no se tocan.
   Ojo con la memoria: nada de fondos grandes por foto; dibujos de íconos del tamaño de los que ya hay.
000e. **PRÓXIMA REVISIÓN — la enciclopedia por dentro (pedido 19-nov 10:38).** El menú (cuadrícula de 4: SU DÍA A DÍA,
   PIEDRAS Y MINERALES, FLORA, FAUNA) está PERFECTO: no tocar. Lo malo es al entrar a una categoría (panel_cofre, ui.c
   ~10611, rama `s_cofre_vista == 2` con `s_cofre_sub >= 0`): arriba dice «ENCICLOPEDIA», debajo la fila de 4 botones
   (`cofre_cat_boton(10 + k…)`) con la elegida marcada, el nombre chico de la categoría (y 158) y abajo una lista que se
   desliza (area_texto 45,186 330×146). Lo que pide el usuario:
   - título = SOLO el nombre de la categoría (como ya hace Tesoros: ocultar «ENCICLOPEDIA», poner NM[s_cofre_sub] grande);
   - QUITAR la fila de categorías;
   - el contenido también en CUADRÍCULA (fichas como las del menú), usando todo el alto que queda libre;
   - si hay más, se desliza (scroll vertical de la cuadrícula).
   Hay que cambiar cofre_pieza(…, false) y tesoros_magia() para que dibujen celdas de cuadrícula en vez de filas (ojo: las
   mismas funciones las usa Tesoros con `true`; tesoros_magia crea solo las filas de la página por la RAM interna:
   ts_toca/s_ts_lo/s_ts_hi). Volver: sigue yendo al menú de la enciclopedia (cofre_hub_cb 21). Revisar también modo
   infante (sin textos). Probar con el simulador: paneles.js y una foto de cada categoría.
000f. **PRÓXIMA REVISIÓN — diario sin la pantalla grande (pedido 19-nov 10:40).** Al tocar un hito del diario se abre
   nota_mostrar() con lo mismo que ya dice la fila: QUITAR. En diario_nota_cb (ui.c ~9865) dejar solo los dos casos que
   abren otra cosa (DIA_HISTORIA → panel_historia, DIA_NUESTRA → panel_canciones) y no llamar nota_mostrar para el
   resto. Las filas normales dejan de ser tocables (sin LV_OBJ_FLAG_CLICKABLE ni el evento; mantener SCROLL_CHAIN_VER
   para deslizar). Revisar antes que la fila muestre el texto ENTERO (ancho 306, LV_SIZE_CONTENT) y no lo corte, porque
   la nota era lo único que mostraba el texto completo. Cambiar el comentario «cada una se lee entera al tocarla».
   (El álbum y los tesoros también usan nota_mostrar: esos NO se pidieron.)
000g. **PRÓXIMA REVISIÓN — los animales desaparecen de golpe (19-nov 10:41: el conejo se fue detrás de la mascota y
   desapareció).** Llegar sí: animar_animales (ui.c ~16000) los hace entrar desde un borde (izq/der/arriba/abajo, ANI_LLEGA).
   Irse NO: `if (!aqui && v->visible) v->visible = false;` (ui.c ~16033) los oculta en el mismo cuadro en que el motor
   deja de tenerlos (pet_animal_here = false: se acabó su animal_hasta, o animal_se_va / ANI_SE_VA). Regla del usuario:
   **un animal llega caminando o volando y se va caminando o volando**, siempre por un borde. Arreglo: un estado
   «saliendo» (como ya tiene la fauna, s_fauna[].saliendo, ~6747): al dejar de estar `aqui`, elegir el borde más cercano
   (o por arriba los que vuelan, nunca por el agua para los que caminan), caminar hasta salir de la pantalla y recién ahí
   ocultar. Excepciones con su propia animación que se deben conservar: el topo se mete en su hoyo (ANI_SE_ESCONDE) y el
   caracol/topo que salen despacito desde abajo. Si cambia la escena o se abre un panel/juego (mundo en pausa), ahí sí
   se pueden ocultar sin animación. Ojo: que el que va saliendo no se pueda tocar (ni cuente como presente para Pochi o
   los encuentros). Revisar también Pochi y el vecino por si algo se esfuma igual.
000h. **PRÓXIMA REVISIÓN — historias y canciones salen del diario y van al Baúl y al Libro (pedido 19-nov 10:43).**
   - Diario: NO mostrar las entradas DIA_HISTORIA ni DIA_NUESTRA en la lista de hitos (filtrar al dibujar en el panel del
     diario, ui.c ~9940; el motor las sigue anotando: anotar(p, DIA_HISTORIA…) en core/pet.c 1734/6011 y la estrellita
     lib_nuevo en 6741). Con esto, 000f queda: ninguna fila del diario se abre.
   - Baúl (cofre_hub, ui.c ~10547: ACCESORIOS, TESOROS, ENCICLOPEDIA, FOTOS, CÁMARA, CARTAS): dos entradas nuevas,
     HISTORIAS (abre las historias por narrador: panel_historia / lo que hoy abre el Libro) y CANCIONES (panel_canciones).
   - Libro (CONFIRMADO 10:46): el Libro se abre como siempre, deslizando de izquierda a derecha. ESTANDO en el Libro
     (panel_libro_hist, ui_magia.inc ~669), deslizar hacia la izquierda pasa a una hoja con dos botones grandes:
     HISTORIAS y CANCIONES (los mismos destinos que en el Baúl). Deslizar a la derecha vuelve al Libro. Usar el mismo
     mecanismo de hojas de los paneles (pag_ir / s_pag_n, pag_global_cb) para que se sienta igual que el resto.
     Propuesto al usuario: puntitos abajo (2 hojas) para que se note que hay otra hoja; con algo nuevo sin ver, la
     estrellita en el botón; en modo infante, solo los dos dibujos.
0000. **1.0.28 — NUBE DE PENSAMIENTO (aprobado por el usuario 19-nov 11:43).** Hoy pensar() (ui.c ~1088) en adulto llama
   globo() con texto_pensado(): se ve igual que cuando habla. Solo en modo infante usa la nube (s_pens, s_pens_emo,
   s_pens_causa, burbujas s_pens_b1/b2, estilo_nube). Hacer: lo que PIENSA va en la nube (forma de nube con dos burbujitas
   que salen de su cabeza) CON el texto adentro en adulto (en infante sigue solo con dibujos); lo que DICE sigue en el
   globo de siempre. Decidir qué cosas son pensamiento: pensar() (emoción + causa), los «sueños» dormida (sonar_algo), y
   quizás lo que opina de los animales cuando no se lo dicen a nadie. Reglas: la nube no espera en la cola de globos
   (si no hay lugar, no sale), se oculta en paneles/juegos (mundo_en_pausa) y no tapa el globo si los dos coinciden.
   Probar en el simulador (foto del prado con un pensamiento y con un globo a la vez) y en paneles/regresión.
   PUBLICAR: la 1.0.27 está compilada (rama «compilados», 3de5994) pero NO publicada: falta acceso a
   ruloccc/orbepet-firmware desde la sesión o el secreto ORBEPET_FIRMWARE_TOKEN en el repo de código.
0001. **1.0.28 — CUADRÍCULA EN TODOS LOS MENÚS menos el diario (pedido 19-nov 11:48).** Lo de la enciclopedia (1.0.27:
   enc_celda / enc_icono / enc_cabecera y s_enc_cuadro en ui.c, dos por fila 156×104, scroll vertical) se usa en todas
   las listas de cosas: Tesoros por dentro (panel_cofre vista 1, cofre_pieza(…, true) y cofre_objetos), la Bolsa
   (panel_bolsa), los tesoros/colecciones de Más (panel_mas) y donde haya filas de objetos. Las que son LISTAS DE TEXTO
   (cartas, títulos de historias de un narrador en panel_libro_indice, canciones del cuaderno): DECIDIDO 11:49, se quedan
   como LISTA. El diario también queda como lista. Las pantallas de LEER (carta abierta, historia, nota, resumen,
   ficha de un objeto) no son menús: no cambian. Ojo modo infante (ya usa cuadritos de 56×52 sin texto en tesoros_magia).
00. **1.0.26: compilar en GitHub y revisar** (main.c, services.c, ui.c, ui_isla.inc no se pudieron compilar aquí).
   En el orbe probar: Perfiles → traer a otra y volver; «DE VIAJE» y después «¿QUIERES QUE VUELVA?»; el faro (alumbrar un
   barquito y traer la luz de vuelta: tiene que seguirla); tocar el huevo nuevo. En el registro buscar «perfiles:»,
   «guardar seguro:» y «memoria: … reinicio de mantenimiento».
   Pendiente de fondo: **qué retiene la PSRAM con las horas** (ver «Problema general de memoria» en 0a). El reinicio de
   mantenimiento solo lo tapa.

0. **Esperar los informes del probador nuevo** (RENDIMIENTO automático y Probar lo nuevo, ya midiendo despierta).
   Revisar: la línea «RESPALDO: tus mascotas quedaron como antes…» al final del informe; si los 6 juegos, la niebla
   y el bote siguen fallando con la pantalla encendida (si siguen, ahí sí es el firmware); y la tabla de velocidad
   con kpx de verdad (FUSION 0/1).
0a. **18-nov noche, lo que se supo:**
   - La 1.0.24 bajada por Wi-Fi volvió sola a la 1.0.23 sin dejar ni una línea de registro (el arranque siguiente fue
     «encendido»: el usuario lo apagó). Instalada POR CABLE la misma 1.0.24 arrancó y corrió la prueba de RENDIMIENTO
     completa (informe 21:22): o sea el código arranca. La causa del fallo por Wi-Fi sigue sin saberse (¿el usuario lo
     apagó durante el primer arranque? el orbe dijo «error» en el modo Wi-Fi). Antes de publicar otra por Wi-Fi, mirar
     ota.c/E_BAJAR. OJO: el orbe no salta una versión que ya falló (ota.c no mira «fwmala»).
   - Con la 1.0.24 los sonidos quedaron bien: ambiente 5 1018 → 24 ms por cuadro; ningún sonido sobre 165 ms.
   - **«final regular» SIN RESPUESTA NO es la despedida (viaje_empezar)**: MASCOTA PROBAR FINAL REGULAR solo pone
     final_pend y la pantalla abre `panel_final(f, 0)` (ui_final.inc, final_revisar). El registro: «FINAL OK» y después
     nada (ni flush): no fue pánico, se quedó sin contestar y el probador la reinició (watchdog_reset). En las dos
     pruebas que se pegó la PSRAM estaba en ~300 KB y los eventos muestran «se soltaron 67 KB de íconos» cada 10 s
     (se rehacen y se vuelven a soltar). En la prueba total de las 17:10 (PSRAM ~1,3 MB) el final regular sí corrió
     (164 ms/cuadro). La 1.0.25 hace lugar antes de abrir el final (final_revisar) y anota «final: empieza…».
     Si sigue pesado: el Libro de escala 10 con fin_pop (transformación de imagen cada cuadro) y el fondo de 466×466
     con LV_RADIUS_CIRCLE + degradé (máscara redonda) son lo caro; se podría armar el fondo una vez como imagen.
   - **Prueba del 18-nov 22:59 (1.0.25 por cable): el final ya NO se pega** (PSRAM tras los paneles 1110 KB, bloque
     928 KB). Final regular 132 ms/cuadro (máx 261), secreto 10 ms. Los «826 kpx por cuadro» eran pedidos, no dibujo
     real (inv_evento sumaba cada pedido aunque cayera dentro de otro). Zip 1.0.25b (sin subir a GitHub):
     services.c cuenta en LV_EVENT_REFR_START las zonas ya juntadas (kpx reales); fin_fondo sin LV_RADIUS_CIRCLE
     (el borde dorado va en un aro aparte); el Libro flota de a 2 px; luciérnagas con bg_opa/border_opa en vez de
     style_opa (no capa aparte). «PROBAR EL ARREGLO» mide 2 veces VELOCIDAD en cada final.
     Si sigue lento: las hojas con shadow_width 18-22 que suben (páginas 1, 5, 6) y los aros de hasta 424 px.
   - **Prueba 23:16 (1.0.25b):** kpx reales del final regular = 54-66 por cuadro (¡no 826!), pero 118-135 ms → ~2 µs
     por pixel (el mundo normal ~0,5; el secreto ~1). Lo caro era `pagina_vieja`: hoja con transform_rotation + sombra +
     texto y cucharita girados adentro → LVGL la arma en una capa y la gira en CADA redibujo (los aros la cruzan).
     1.0.25c: pagina_vieja la arma en un marco con margen y la aplana UNA vez con lv_snapshot_take (ARGB8888, ~260 KB
     PSRAM, s_pag_plana[2], se suelta en cerrar_panel_dentro → pagina_plana_soltar). Si no hay memoria, queda girada.
   - **Problema general de memoria:** en RENDIMIENTO la PSRAM baja de ~1,8 MB a ~0,3 MB al recorrer los paneles
     (desde «panel 29 aspecto») y no vuelve. Algo retiene ~1,5 MB (¿pantallas art_pantalla_* guardadas? ¿caché?). Es
     lo próximo a investigar: con poca PSRAM todo se pone lento (íconos que se sueltan y rehacen).
   - Efecto lateral del respaldo en Documentos: devolver la partición nvs también devuelve los contadores guardados
     ahí («arranques», número de evento): tras una prueba los eventos pueden repetir número. No afecta al juego.
0b. **1.0.24 (por Wi-Fi falló; por cable bien) (18-nov noche)** — lo que hay que mirar en el próximo informe de RENDIMIENTO + registro:
   «final regular» ya no SIN RESPUESTA y el evento «despedida: empieza (viaje N), paisaje listo/NO CUPO; PSRAM …»;
   «sonido ambiente 5/6/7» y «sonido 68» sin picos de segundos. Los informes del PC se guardan con fecha y hora en
   `Documentos\MascotaPruebas\informe_AAAA-MM-DD_HH-MM-SS.txt` (y el último como `informe.txt`): pedir esos.
   Informe de rendimiento del 18-nov 19:45 (1.0.23, ya midiendo despierta): el dibujo normal va bien (zonas 3-24 ms,
   paneles 4-25 ms); lo malo eran el final regular (pegado: reinicio) y los sonidos (ambiente 5 = 1018 ms de promedio,
   máx 3909; ambiente 7 máx 1333; sonido 68 máx 1426; 61/62 637 ms). Lo que sigue en lo lento normal: clima en el prado
   (despejado 43, lluvia 38, nublado 34 ms), Pochi 44, isla de noche 39, lago helado 36, juegos 35-51 ms.
   OJO: en esta sesión NO se pudo compilar ni correr el simulador (sin red ni ESP-IDF): la 1.0.24 la validó solo la
   compilación de GitHub. Antes de tocar de nuevo la despedida, correr en el simulador `final.js` y `regresion.js`.
1. (hecho en la 1.0.24, ver 0b) **Pantalla de las despedidas lenta** (pedido del usuario 18-nov, 19:44): es el viaje de despedida = «final
   regular» (`ui.c` desde «El viaje de despedida», `viaje_tic`/`viaje_*`, VIAJE_MS 17800). En la prueba total dio
   164 ms por cuadro (máx 254) y en la de rendimiento de las 19:06 quedó SIN RESPUESTA (después la RAM sube: parece
   reinicio). Causa probable, vista en el código: `s_via_negro` es un velo de 466×466 y se le hace
   `lv_obj_set_style_opa(s_via_negro, oscuro * 235)` en CADA cuadro, también cuando vale 0 (antes de los 13,4 s) y
   cuando ya quedó fijo en 235. Cambiar un estilo invalida el objeto entero aunque el valor no cambie: **la pantalla
   completa se redibuja en cada cuadro durante toda la despedida**, y entre 13,4 y 15,2 s además con mezcla de
   transparencia sobre todo. Lo mismo, más chico, con `s_via_c1`/`s_via_c2` (opa 255 fija igual se vuelve a poner),
   las 16 estrellas y los corazones. Arreglo propuesto (seguro, no cambia cómo se ve):
   (a) un ayudante `opa_si_cambia(obj, v)` que guarda el último valor y solo llama a `lv_obj_set_style_opa` si
   cambió; usarlo en todo `viaje_*` (y de paso en `ui_final.inc`, el final secreto, que hace lo mismo);
   (b) `s_via_negro` oculto (`ocultar(…, true)`) mientras `oscuro == 0`;
   (c) las estrellas: titilar con 8 niveles de opacidad (no 255) y moverlas solo si cambian de píxel.
   Comprobar con el simulador: kpx por cuadro del final antes/después (`sim_test_panel` / la escena del final) y
   `regresion.js` igual; en el orbe, «final regular» en RENDIMIENTO (con medir despierta). Va en la 1.0.24 junto
   con las escrituras a flash (3b-B1).
2. (hecho el 18-nov) Respaldo de las pruebas en Documentos (3a) y medir despierta (3b-A).
3. **Rendimiento** (sección 3b, rehecha con los informes del 18-nov).
4. **Cuentapasos visible (pedido del usuario 18-nov, 22:19):** cuando sale a caminar con el orbe, tiene que poder VER
   los pasos que caminó. Hoy el acelerómetro ya cuenta (`pasos_total`/`pasos_premio` en pet.h; el paseo de verdad en
   main.c, «(fw 104) paseo de verdad»: ≥ 40 pasos/min, 8 min y 800 pasos = excursión juntos, EV_PASEO_JUNTOS /
   DIA_PASEO_JUNTOS) pero no se muestra en ninguna parte. Preguntarle al usuario dónde lo quiere ver (¿al encender la
   pantalla después de caminar, un aviso «¡CAMINAMOS 1.234 PASOS!», un panel con los de hoy y los de la semana, en el
   diario?). Ojo con los textos en MAYÚSCULAS vía gtxt y con el modo infante (sin textos: dibujo de huellitas).
2. **Pendiente del usuario «o la hormigas»** (sección 1): preguntarle dónde salió.
3. **Editor de personajes** (conversación del 17-nov): el arte nuevo es de 48 px (96 los grandes) y el editor dibuja
   íconos de 24×24. Acordado: primero abrir el dibujo de fábrica en 48×48 para retocarlo (`art_icono_lado_de_bytes`),
   después un editor por partes (colores, ropa, orejas, cola, expresión).
4. Batería: noche sin cable con sueño profundo (nunca probado); %/h con pantalla apagada.
5. Retoques que pida el usuario. Los pendientes viejos siguen en `claude/pendientes-v89.md`.

## 3a. Respaldo de las pruebas (pedido del 18-nov) — HECHO en el probador

Cómo quedó (`herramientas/mascota_gui.py`, bloque «(1.0.24) respaldo de las pruebas», y `probar.documentos()`):
todas las pruebas de `PRUEBAS_CON_RESPALDO` leen la partición **nvs** entera (todas las mascotas, perfiles y ajustes)
con esptool a `Documentos\Respaldos mascota\Antes de las pruebas\antes_de_<prueba>_<fecha>\nvs.bin` (+ manifiesto
y huella sha256), dejan `PENDIENTE_RESTAURAR.txt`, corren la prueba y en un `finally` graban ese nvs.bin (esptool
comprueba lo grabado), reinician, ponen la hora y borran la marca; el informe termina con «RESPALDO: …». Si queda
la marca (programa cerrado a mitad), al abrir el programa se restaura sola y avisa; una prueba nueva con la marca
puesta usa ESE respaldo (no toma uno nuevo de la partida ensuciada). Se guardan los 10 más nuevos. Botón nuevo en
Respaldo: «Restaurar el último respaldo de prueba». Si el respaldo falla, la prueba no empieza. Las fotos no se
respaldan aquí (la fase 17 borra su propia foto de prueba). Probado con una placa simulada (5 casos).
El RESPALDO/RESTAURAR de la placa sigue igual por dentro (las fases lo siguen usando); ya no importa que se gaste.

Lo que se escribió como plan (para historia):

Lo que pasa hoy: la PRUEBA TOTAL cambia la partida (aspecto y etapa en la fase 22, modo niño/infante en la 23,
«abrir todo» en la 25, viajes en el tiempo en la 19, perfiles…) y **no hay ningún paso final que la devuelva**:
el informe termina en «30. CIERRE» sin restaurar. Solo la fase 22 restaura lo suyo («aspectos: partida
restaurada»). La prueba de RENDIMIENTO (18:39) no respalda nada. Y si la placa se cuelga o reinicia a mitad
(pasó 6 veces el 17:10), lo que falta de la prueba ni siquiera corre.

Lo que pidió el usuario: **respaldo al empezar, en Mis documentos, y restaurar desde ese mismo archivo al terminar.**
Diseño (en `herramientas/mascota_gui.py` + un módulo común que usen `prueba_total.py` y `prueba_nuevo.py`, y la de
rendimiento):
1. Carpeta: la carpeta Documentos real del usuario. En Windows con `SHGetKnownFolderPath(FOLDERID_Documents)` por
   ctypes (sirve aunque OneDrive la haya movido); si falla, `~/Documents`. Dentro: `OrbePet\Respaldos de pruebas\`.
2. Al empezar CUALQUIER prueba: bajar el respaldo completo (todos los perfiles/mascotas, con el mismo formato que
   el botón «Respaldo» del programa) a `antes_de_prueba_AAAA-MM-DD_HH-MM-SS.<ext>`. Releerlo y comprobarlo (tamaño,
   CRC, cuántas mascotas y sus nombres). **Si el respaldo falla, la prueba no empieza.**
3. Dejar un archivo `PENDIENTE_RESTAURAR.txt` con la ruta del respaldo. Se borra solo cuando la restauración se
   comprobó.
4. Al terminar, en un `finally` (también si se cancela, si hay un error o si la placa dejó de responder): esperar a
   que la placa vuelva (hasta ~60 s, reconectando el puerto), subir el respaldo, reiniciar, y **comprobar** que
   cada mascota volvió (nombre, especie, etapa, perfil activo). Anotar el resultado en el informe:
   «RESPALDO: restauradas N mascotas desde …».
5. Si el programa se cerró a mitad de una prueba, al abrirlo de nuevo ve `PENDIENTE_RESTAURAR.txt` y restaura solo
   (el usuario casi no puede usar el PC: sin preguntas, solo avisar en grande que lo hizo).
6. Guardar los 10 respaldos de prueba más nuevos; borrar los más viejos de esa carpeta (solo de esa carpeta).
7. Botón en la pestaña Respaldo: «Restaurar el último respaldo de prueba».
Comprobar en el código antes de escribir: cómo baja/sube hoy el respaldo el programa (orden por cable) y si incluye
todos los perfiles; si no, ampliarlo. Ojo: la fase 27 reinicia la placa 5 veces y la 18 también: el respaldo NO se
toma de nuevo después de esos reinicios.

## 3b. Rendimiento: lo medido y lo que se propone (18-nov, rehecho con los informes de las 17:10, 18:39 y 19:06)

Lo de antes (1.0.19): dibujar ~95 ms por cuadro. **Eso ya no pasa:** con la 1.0.23, el prado con la pantalla
encendida y en calma dibuja **5 ms por cuadro y 15 kpx** («vivo» a los 785 s del registro). «Juntar áreas» (1.0.22)
funcionó. Ahora lo que se nota son los **tirones** (cuadros de 0,3 a 4 s) y algunas escenas puntuales.
No sirve cambiar de lenguaje (ya es C, y LVGL también).

**A. Primero, medir bien** — HECHO en el probador: `probar.medir_despierta(p)` (pantalla 1, ACCION 5 0 = despertar,
TOQUE) y mientras `p.despierta` cada orden manda antes un TOQUE si pasaron más de 5 s. Lo usan «RENDIMIENTO
automático» entero y «Probar lo nuevo» entero (no la prueba total: ahí las fases de ahorro deben ver el ahorro).
Lo que se vio:
- La fase 29d corrió con la pantalla apagada (registro: «pantalla off» a los 642-663 s) y la mascota dormida todo el
  rato (`dormida=1`). Por eso la tabla de velocidad dio 0-1 ms y 0 kpx: no se dibujó nada.
- Con la mascota dormida y 12 s sin toques, el procesador baja a 80 MHz **aunque la pantalla esté encendida**
  («energia: CPU 80 MHz (pantalla on, dormida 1, sin toques 12 s)»). En la prueba de rendimiento, desde el panel 22
  todo da 1 ms y ~35 cuadros: ya estaba en ahorro. Las cifras de las dos mitades no se pueden comparar.
- Arreglo en las pruebas: antes de cada medición, despertarla, encender la pantalla y fijar 240 MHz con una orden de
  prueba (que el ahorro no actúe mientras se mide); al final, devolver el ahorro normal.

**B. Los tirones (lo más importante para el usuario):**
1. **Escrituras a flash dentro del ciclo.** «escritura lenta a flash: el registro tardó 854-2309 ms» pasa también
   en uso normal (a los 13 s y 303 s de un arranque, sin pruebas). Cada una congela la pantalla. Además «escribir
   4 KB tardó 1703 ms» y «memoria flash: peor vuelta 1943 ms». Propuesta: (a) el registro va a una cola en RAM y lo
   escribe una tarea de baja prioridad cada ~60 s o al dormir; (b) revisar `CONFIG_SPI_FLASH_AUTO_SUSPEND` (el S3 lo
   permite con algunos chips: deja seguir trabajando mientras la flash borra); (c) 1,7 s para 4 KB es mucho: recursos y fotos
   son SPIFFS (partitions.csv); SPIFFS se pone muy lento al borrar cuando está lleno o fragmentado (mirar el espacio libre).
2. **Mensajes de error que se repiten y van a la flash:** «TP: Sleep mode not supported!» salió más de 100 veces
   (cada vez que se apaga la pantalla) y «i2s_channel_disable… not been enabled yet» en cada cierre del micrófono.
   Callarlos: no pedirle «dormir» al táctil (no lo admite) y desactivar el canal i2s solo si estaba activo. Menos
   escrituras y un registro más limpio.
3. **Sonidos que se cargan de golpe:** sonido 60/61 con un cuadro de 677 ms, 28/29 de 560 ms, 122 de 478 ms, 98/99
   de 373 ms. Probablemente se leen enteros de la flash al tocarlos. Cargarlos en la tarea de audio por trozos, o
   dejar listos los largos que suenan seguido.
4. **«1b editor USB» de 1,6 a 4,3 s** con pantalla encendida: las órdenes por cable hacen su trabajo dentro del
   ciclo del juego. Solo afecta a las pruebas, pero explica varios «la placa NO contestó» (la prueba se cansa de
   esperar). Además de mover el trabajo pesado fuera del ciclo, darle a la prueba más paciencia antes de dar FALLA.

**C. Redibujos de más (posible error de «juntar áreas»):** el registro tiene cuadros de **1585 kpx** y **445 kpx**
(a los 730 s y 743 s). La pantalla entera son 217 kpx: se dibujó la misma zona varias veces en un cuadro. Puede ser
que `ui_inv_fusionar` deje áreas encimadas. Propuesta: si lo fusionado ya pasa de ~60 % de la pantalla, invalidar la
pantalla una vez y listo; y no guardar un área que ya esté dentro de otra. Agregar a `redibujo.js` un aviso si la
suma de kpx de un cuadro pasa de 217.

**D. Memoria fragmentada → fondo por franjas (más lento):** con 2 MB de PSRAM libres el bloque mayor era de 608 KB,
y el lienzo pide 876 KB: el fondo salió por franjas 3 veces en el registro. Al arrancar sí hay un bloque de 2432 KB.
Propuesta: **reservar el lienzo una sola vez al arrancar y no soltarlo nunca.** Se acaba la fragmentación por ese
lado y el camino lento queda solo de respaldo. También se soltaron 468 KB de íconos guardados por «memoria baja»
(después hay que volver a cargarlos).

**E. Escenas que siguen pesadas** (prueba de rendimiento de las 18:39, aún con 240 MHz): clima en el prado
(viento 82 ms, nublado 70, lluvia 53, nieve 44, despejado 43 por cuadro), lago helado 47, ballena de nubes 36, y
«llamar al animal» de 87 a 144 ms (no debería: el animal es de 36×36). Mirarlas en el simulador con kpx: si el
cielo con nubes se redibuja entero, mover las nubes cada 2-3 cuadros y de a píxeles enteros.

**F. Lo que se proponía antes,** ahora en segundo plano (el dibujo en calma ya cuesta poco): franjas de dibujo de
30 filas, instrucciones vectoriales del S3 (`LV_DRAW_SW_ASM_CUSTOM`), sprites RGB565A8, adornos a menos cuadros.

**G. Fallas que no son de velocidad (prueba total 17:10):** la placa dejó de responder en «aspecto especie 2 color 0»,
«modo infante panel 3» y «acción 5», «historia narrador 2 n.º 2» y «con el olor del zorrillo encima»; en la de las
19:06, «final regular» (después de eso la RAM sube a 48 KB y la PSRAM a 2,2 MB: parece un reinicio que el resumen
no contó). El registro de eventos no muestra pánicos: pueden ser **esperas largas** (punto B4) y no cuelgues. Para
saberlo, la prueba debe anotar el contador de arranques antes y después de cada paso que falla. Otros avisos del
arranque: «hal: RTC no responde; se usa el reloj interno» (el reloj externo no contesta: la hora se puede perder sin
batería) y «i2c.master: Please check pull-up». Los 5 juegos del claro del rey no se cierran con estrellas en la
prueba y la nube no sube al mirador: revisar si es la prueba o el juego.

Lo anterior (para historia):

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
- **Una mascota de Perfiles nunca se pierde** (19-nov): guardado seguro, cambio anotado, «DE VIAJE» con vuelta atrás.
  Ojo: el respaldo de las PRUEBAS (Documentos\Respaldos mascota\Antes de las pruebas) sigue siendo solo nvs: las que
  descansan (recursos/m*.bin) no entran. El «Respaldar mascota» del programa sí (1.0.26).
- **Toda prueba por cable respalda al empezar (en Documentos\Respaldos mascota\Antes de las pruebas) y restaura desde ese
  mismo archivo al terminar, aunque falle o se cancele** (pedido del 18-nov).
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
cd build && cmake .. && make test_core && ./test_core          -> 1490/1490 comprobaciones OK
export WASI_SDK=/home/claude/sdk/wasi-sdk-25.0-x86_64-linux LVGL_DIR=/home/claude/lvgl BINARYEN=/home/claude/sdk/binaryen-version_123
sh sim/web/build.sh
node sim/web/pruebas/paneles.js          -> 18 problemas (la base de siempre)
node sim/web/pruebas/regresion.js        -> «dibujo: igual que la línea base» (--base para regrabarla)
node sim/web/pruebas/soak.js             -> soak: 180 días sin caerse   (~10 min)
node sim/web/pruebas/fondos_franjas.js   -> 1260 casos, 0 distintos
node sim/web/pruebas/inicio.js | libro.js | libro_flujo.js | final.js
node sim/web/pruebas/dos.js | isla.js | olor.js   -> juegos de a dos / de la isla / el olor: todo bien
node sim/web/pruebas/pochi_visitas.js            -> visitas de Pochi: todo bien (los 5 tipos de visita)
node sim/web/pruebas/arcoiris.js                 -> tobogán del arcoíris: todo bien
node sim/web/pruebas/peces.js                    -> colección de peces: todo bien
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

- Motor: `core/pet.h`, `core/pet.c`, `core/peces.inc` (1.0.32), `core/v41.inc`, `core/historia.inc`, `core/toques.inc`, `tests/test_core.c`
- Interfaz: `firmware/main/ui.c` + `ui_pochi.inc`, `ui_zonas.inc`, `ui_zjuegos.inc`, `ui_final.inc`, `ui_magia.inc`,
  `ui_historia.inc`, `ui_escenas.inc`, `ui_ceremonias.inc`
- Arte: `firmware/main/art.c`, `art.h`, `fondos_data.inc`, `pantallas_data.inc`, `huevos_data.inc`,
  `iconos_hd.inc`, `iconos_hd2.inc`, `personajes_data.inc`
- Dibujo (Python): `herramientas/pixel_mundo.py`, `dibujar_zonas_v98.py`, `dibujar_vivero_v106.py`,
  `hacer_fondos.py`, `hacer_pantallas.py`, `iconos_nuevos/huevos92.py` (+ `huevos_base.png`)
- Placa y pruebas: `firmware/main/main.c`, `herramientas/prueba_total.py`, `herramientas/mascota_gui.py`
- Simulador: `sim/web/web_main.c`, `sim/web/plantilla.html`, `sim/web/pruebas/*.js`
- Actualizar por Wi-Fi e informes: `firmware/main/ota.c`, `ota.h`, `version.h`, `.github/workflows/firmware.yml`
- Juegos de a dos y de la isla: `firmware/main/ui_dos.inc`, `ui_isla.inc`, `ui_arcoiris.inc` (1.0.31), `isla_mapa.inc` (generado),
  `herramientas/hacer_fondos_juegos.py` → `hacer_fondos_dos.py` (→ `firmware/recursos/pj0..pj6.bin`, `fondos_dos_sim.inc`)
- Documentación: `README.md` (una sección por firmware) y `claude/pendientes-v89.md`
