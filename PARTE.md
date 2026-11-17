# ACTUALIZACIÓN (17-nov-2026, 15:10) — firmware 108: perfiles, Pochi, paleta y textos

## Lo que se hizo en el 108 (la partida NO cambia: sigue en la versión 42)
Seis pedidos del usuario (14:26); quedan cinco hechos y uno a medias (ver abajo).
1. **Cada mascota con su mundo.** `pet_resume` (core/pet.c) le pasaba a la que llegaba el mundo de la que se
   quedaba: por eso, al estrenar un perfil nuevo (mundo vacío) la antigua perdía casa, flores, jardín, ropa,
   juguetes, amigos... Ahora solo se pasa lo del orbe: hora, hemisferio, país, estación forzada, volumen del
   ambiente, clima y `vio` (los avisos que ya salieron). Prueba `test_fw108_mundos` en test_core.
   OJO: lo que ya se perdió en la placa no se recupera (salvo con un respaldo anterior).
2. **Pochi no se pasa de perfil**: `po_fuera()` en ui_pochi.inc, llamado al cambiar de mascota y al estrenar una.
3. **Pochi no interrumpe una actividad**: `po_ocupada()`; si le tocaba llegar queda `s_po_quiere` y llega apenas
   termina (se revisa cada 5 s).
4. **La paleta de colores**: `art_color_especie` usaba la paleta vieja y las criaturas se pintan con BEBE_PAL;
   ahora el botón muestra el color de verdad (`color_cuerpo_nuevo`, con el teñido y un toque de luz).
5. **Mensaje inicial**: reescritas las dos primeras páginas del Libro (ui_final.inc), en primera persona y con
   las recetas de galletas que pidió el usuario.
6. **«o la hormigas»**: PENDIENTE. Se revisó todo el código que arma frases con nombres de animales y comidas y
   no aparece ese texto; lo único claro que sí estaba mal y se arregló: «¡Wow, es las hormigas!» → «son» cuando
   el nombre es plural (ui_magia.inc, EV_ANIMAL_NUEVO). HAY QUE PEDIRLE AL USUARIO LA PANTALLA EXACTA (foto o en
   qué momento salió) para cazar la que vio.

## Estado
- test_core **1160/1160** (una prueba nueva); paneles 18 (la base); regresión: solo cambió el panel 28 «color»
  (línea base regrabada); inicio/libro/libro_flujo/final OK; soak 180 días; fondos por franjas 0 distintos.
- **Sin compilar con ESP-IDF** (lo compila el usuario).
- (de paso) `sim/web/pruebas/libro.js` cierra el panel antes de capturar: las capturas salían con la pantalla
  de elegir huevo encima.

# ACTUALIZACIÓN (17-nov-2026, 13:40) — firmware 107: el huevo se abre a la vista y el nombre no se pierde

## Lo que se hizo en el 107 (la partida NO cambia: sigue en la versión 42)
El usuario estrenó una partida nueva y: el huevo no apareció en el prado ni se abrió, y no alcanzó a ponerle nombre.
- **Causa del huevo**: en `ui_update`, al quedar puesto el reloj se mandaban `ASPECTO`, `GENERO` y `ECLOSIONAR`
  juntos: nacía en el mismo instante y el huevo nunca se dibujaba. Ahora ese paso solo manda ASPECTO y GENERO,
  prende `s_nace_desde` y durante **9 s** el huevo está en el prado meciéndose con las grietas creciendo
  (`NACE_MS`, globos «TU HUEVO LLEGÓ AL PRADO» y «¡ALGO SE MUEVE ADENTRO!»); al final va ECLOSIONAR + estrellitas +
  salto + «¡HOLA!», y el nombre espera 2,6 s más (`s_nace_fin`). Mientras dura, la rama nueva corta la cadena de
  else-if, así que ningún panel se abre encima.
- **Causa del nombre**: `ritmo_revisar()` abría «¿cuánto dura su día?» apenas la mascota dejaba de ser huevo, antes
  del nombre; y la pregunta del nombre solo salía con `etapa == STAGE_HUEVO`, así que una vez nacida ya no volvía.
  Ahora el ritmo espera a que no haya `s_inicio_cfg`, ni `s_nace_desde`, ni mascota sin nombre; y el nombre se
  pregunta a cualquier mascota sin nombre (huevo o no).
- Probado en el simulador los dos caminos: primer encendido y mascota nueva desde PERFILES. En los dos se ve el
  huevo en el prado y después sale el panel del nombre.

## Estado
- test_core **1149/1149**; paneles 18 (la base); regresión igual a la línea base; inicio/libro/libro_flujo/final OK;
  soak 180 días; fondos por franjas 1.260 casos / 0 distintos.
- **Sin compilar con ESP-IDF** (lo compila el usuario). El probador no se puede ejecutar aquí (no hay tkinter).

# ACTUALIZACIÓN (17-nov-2026, 12:40) — firmware 106: la pantalla de elegir huevo

## Lo que se hizo en el 106 (la partida NO cambia: sigue en la versión 42)
Pedido 12:00: «la selección de huevos parece sacada de otro juego»; más calidad en el prado, barra de texto
transparente, huevo y nido mejor dibujados, y descripciones acordes a lo que nace («¿cómo a un huevo de rayo le
asustan las tormentas?»).
- **Vivero nuevo**: `herramientas/dibujar_vivero_v106.py` dibuja `fondos_pantallas/0_huevos.png` con los pinceles
  de las zonas (pixel_mundo): prado de noche visto desde arriba, pasto de verdad, arbustos, piedras, hongos,
  luciérnagas, luz de luna y **ocho nidos tejidos** bajo cada huevo. Antes: degradado verde + elipses cafés.
- **Huevos al doble de detalle (92 x 90), con el MISMO dibujo**. OJO: el primer intento les cambió el diseño (cada
  cáscara con el color de su elemento) y el usuario lo rechazó («mantén el diseño original, los nuevos no se ven tan
  lindos»): NO volver a rediseñarlos, solo subir calidad.
  
  `herramientas/iconos_nuevos/huevos92.py` parte de `huevos_base.png` (el arte original rescatado del 104, porque
  `pack/gen_huevos.py` no viene en el zip), agranda con EPX/Scale2x e ilumina (rampa, brillo, luz rebotada, sombra,
  contorno propio). 96 colores; `huevos_data.inc` pasó de 60 KB a 219 KB. art.c dibuja el sprite a la mitad: ocupa
  lo mismo que antes en pantalla y a escala 5-6 sale pixel a pixel.
- **Barra de la pista transparente** (opacidad 150), más arriba y más baja de alto para que no la pise «¡ESTE!»; el
  huevo elegido lleva un **óvalo** de luz detrás que encierra al huevo Y a su nidito (12:28: «un círculo solo en el
  huevo se ve raro»). Las pistas quedaron de 2 líneas como máximo: más largas las tapaba el botón.
- **Textos**: ocho pistas nuevas de dos frases, acordes a quien nace (y sin spoiler). Se eliminó el arreglo GUSTO.

## Ojo con el tamaño
El programa creció ~200 KB (huevos 160 KB + el fondo del vivero 40 KB). Si al grabar NO cabe en ota_1 (0x7A0000):
alias de primavera en FD_TABLA (−80 KB), bajar colores de los huevos (CUANTOS en huevos92.py) o dejar las grietas
2 y 3 en el dibujo chico.

## Estado
- test_core **1149/1149**; paneles 18 (la base); regresión: solo cambió el panel 29 «aspecto» (línea base regrabada);
  inicio/libro/libro_flujo/final OK; soak 180 días; fondos por franjas 1.260 casos / 0 distintos.
- **Sin compilar con ESP-IDF** (lo compila el usuario). El probador no se pudo ejecutar aquí (no hay tkinter).

# ACTUALIZACIÓN (17-nov-2026, 08:10) — firmware 105: la casita ya no puede faltar + probador plegable

## Lo que se hizo en el 105 (la partida NO cambia: sigue en la versión 42)
- **La casita del prado que «sigue sin aparecer de vez en cuando»** (pedido 07:40). Causa: los adornos del prado (casita-tocón,
  farol, banco, flores, hongos, senderos) y el Corazón de la isla se dibujan encima de un lienzo de **876 KB de un tirón**; con
  la PSRAM picada no hay bloque así, y el 101b/104 dibujaban la zona PELADA y reintentaban cada 30 s pidiendo otra vez los
  mismos 876 KB (casi nunca llegaban: no volvía hasta reiniciar).
  Ahora, si no hay ese bloque, el fondo se arma **por franjas** (`fd_franjas` en art.c): 156 filas (292 KB) y, si no cabe, 78,
  36, 12 o 6 (11 KB). Cada adorno se dibuja en las franjas que toca, corrido, y la hora/clima/luces van con la fila de verdad.
  **Sale igual pixel a pixel**: `sim/web/pruebas/fondos_franjas.js` compara las dos maneras en 1.260 casos → 0 distintos.
  Para que cuadre: `luz_nocturna`, `resplandor`, `masa` y `sombra` redondean con `floorf` (antes `(int)`: con coordenadas
  negativas corría una fila), `pintar_senderos_off` recibe el corrimiento y las franjas son múltiplos de 6 (el tramado
  par/impar no cambia). La placa lo anota en el registro: «fondo por franjas de N filas…» con la PSRAM libre.
  Probar sin esperar: `SINLIENZO 1` / `0` / `R` por el cable, botón «Fondo por franjas» del simulador, cuadro «Fondos del
  mundo» del probador, fase **29c** de prueba_total.
- **El probador (mascota_gui.py) no tapa el registro** (pedido 07:40): 8 pestañas POR FUNCIÓN (Placa y registro · Pruebas ·
  Pantalla y hora · Mundo y lugares · Pochi y animales · Desbloquear contenido · Voces · Respaldo) y dentro, cuadros
  **plegables** (clase `Plegable`: se abren tocando el título; empiezan cerrados salvo el principal de cada pestaña), más
  «Plegar todo» / «Abrir todo». Cada pestaña mide lo que tiene abierto (hasta 360 px) y el registro pasó de 12 a 20 líneas.
  Ojo: aquí NO hay tkinter, así que el probador no se pudo ejecutar; está revisado a mano (sintaxis OK).

## Estado
- test_core **1149/1149** (sin cambios en el motor); ui.c y art.c compilan sin avisos nuevos; simulador: paneles 18 (la base),
  regresión igual, inicio/libro/libro_flujo/final OK, soak 180 días, fondos_franjas 1.260 casos / 0 distintos.
- **Sin compilar con ESP-IDF** (lo compila el usuario).

## Lo que sigue pendiente (lo importante)
1. **Nada del 102 al 105 se ha probado en la placa**, salvo lo que ya vio el usuario (el cachorro escondido y la casita).
   Pendiente ahí: Pochi hembra y los 4 cachorros, las escenas de familia, los toques a animales, el 104 entero (papelitos,
   hielo, paseos, terquedad) y ahora los fondos por franjas (fase 29c).
2. Confirmar que el micrófono ya no se come la RAM interna (registro con «escuchar música» varias horas).
3. Noche sin cable con sueño profundo (nunca probado), %/h con pantalla apagada, reposo del táctil (CST9217 en GPIO2).
4. Si al grabar no cabe en ota_1 (0x7A0000): alias de primavera en FD_TABLA (−80 KB) o bajar colores.

# ACTUALIZACIÓN (16-nov-2026, 21:30) — firmware 103 hecho y ENTREGADO (zip + parte, como pidió el usuario)

## Lo que se hizo en el 103
- **Tocar animales tiene consecuencias** (pedido 17:37, «vamos con todos de una vez»): paciencia por animal; toque 1 saluda,
  el siguiente avisa, insistir cuesta. Abeja pica (y le queda miedo), zorrillo rocía, erizo pincha, hormigas pican, los
  grandes rugen, los demás se van; caracol/tortuga/topo nunca se enojan. En modo infante se queda en el aviso.
  Motor: `core/toques.inc` (`pet_animal_touch`, `pet_fauna_touch`, `pet_animal_patience`, `pet_stinky`).
- **El olor del zorrillo NO es suciedad** (pedido explícito del usuario): `hediondo` + `hediondo_min` en bytes que eran
  relleno (la versión de partida sigue siendo 41). Dura ~3 h o hasta el baño. En pantalla: vahos verde claro que suben del
  cuerpo y se desvanecen + 2 moscas en órbita (`olor_actualizar` en ui.c). OJO: la primera versión usaba verde oscuro con
  opacidad 110 y era INVISIBLE sobre el pasto; ahora es 0xCBE86B con opacidad hasta 205 y borde. Si se vuelve a tocar, no
  bajar de ahí.
- **Fuera el «LO GUARDÉ EN EL LIBRO»** de cada mensaje de animal (pedido 21:02) y el «te lo dejo en el libro» del dragón.
- Pruebas nuevas: `TOQUE n` en la placa (1..7), botones en el simulador y en mascota_gui, fase **29** en prueba_total
  (Pochi, su familia y los toques), y `test_fw103_toques` en el motor.

## Estado
- test_core **1105/1105**; ui.c compila sin avisos; simulador: paneles 18 (la base), regresión igual, inicio/libro/
  libro_flujo/final OK, soak 180 días. **Sin compilar con ESP-IDF** (lo compila el usuario).
- Entregado: `mascota_firmware_103.zip`.

## Lo que sigue pendiente (lo importante)
1. **Nada del 102 ni del 103 se ha probado en la placa.** Pochi hembra, los 4 cachorros, las escenas de familia, los
   encuentros con animales, los 20 dibujos nuevos, la marca de enojo y ahora los toques.
2. Confirmar que el micrófono ya no se come la RAM interna (registro con «escuchar música» varias horas).
3. Noche sin cable con sueño profundo (nunca probado), %/h con pantalla apagada, reposo del táctil (CST9217 en GPIO2).
4. Si al grabar no cabe en ota_1 (0x7A0000): alias de primavera en FD_TABLA (−80 KB) o bajar colores.

## Ideas anotadas (de GitHub, 16-nov 16:30; el usuario preguntó «¿será necesario?» y se dejaron para después)
- Encadenar actividades con una matriz de transición estilo Ark-Pets (hoy `aplicar_caracter` no mira la actividad anterior).
- Umbrales de ánimo modulados por el cariño (VPet).
- Terquedad por personalidad: resistir el sueño si otra necesidad pesa más (Dosidicus).
Repos mirados: VPet 7.2k, Ark-Pets 1.17k, Dosidicus 334 (clonados en /home/claude/vpet, /home/claude/arkpets,
/home/claude/vicioussquid/dosidicus; se pierden al cerrar la sesión).

## Decisiones del usuario que NO hay que deshacer
- Pochi es **hembra** y mamá de **Chapi** y **Rony** (niños) y **Pasita** y **Morita** (niñas). Pasita es la MÁS clara.
  Rony **sin collar**; Chapi verde, Pasita rosado, Morita morado. Los cuatro son **igual de importantes**.
- Solo Pochi anda con la lengua afuera; los cachorros, de vez en cuando.
- El cachorro escondido **no se dibuja** mientras está escondido (nada de siluetas grises ni matas de color): se mueve el
  pasto y después asoma entero.
- Con dos o más cachorros NO hay encuentros con animales (esa visita es de familia).

# ACTUALIZACIÓN (16-nov-2026, 17:20) — fw 102: arreglado el cachorro escondido

- El usuario lo vio en la placa: «aparece y desaparece en color gris como fantasma» (se dibujaba con opa 160). Se probó una
  mata verde: la rechazó («parece un parche de color verde, no terreno de ocultamiento»).
- Ahora: invisible del todo; cada 2,6 s se mueve el pasto (ICO_HIERBA flotando + chispas verdes + quejidito) y DESPUÉS asoma
  entero un momento. Al encontrarlo, salta pasto. Sin matas ni siluetas.
- Probado en el simulador: ciclo a ojo y toque que lo encuentra. test_core 1090/1090, paneles 18, regresión igual.

# ACTUALIZACIÓN (16-nov-2026, 16:25) — fw 102: Pochi y los cachorros con los animales del lugar

- Pedido: que interactúen con los animales que haya («vamos con todo»), pero solo con Pochi sola o con un cachorro (no todos juntos).
- Hecho: tipos + tabla de reacciones, especiales (zorrillo, erizo, urraca/cuervo, búho, chucao), amigos por cachorro
  (`pochi_amigos`). Ver README. test_core 1090/1090. Zip 102 rearmado; el usuario ya recibió un 102 anterior (15:55): mandar el nuevo.

# ACTUALIZACIÓN (16-nov-2026, 15:55) — fw 102: familia de Pochi, todos igual de importantes, boca cerrada

- Pedidos: «familia entre ellos, y todos son igual de importantes (no tanto énfasis en Rony)»; «Morita y Pasita son niñas, Rony y
  Chapi niños»; «no todos tienen la lengua afuera: solo Pochi; los otros de vez en cuando»; «Pasita es la más clara».
- Hecho: elección pareja (`pet_pochi_pups_pick`, 1/2/4 cachorros), momentos de igual duración, juego propio para cada uno,
  despedidas una por cachorro; escenas de familia (pillada, ramita, siesta, se esconde, Pochi los lame), «¿y Morita?» al despedirse,
  el que no se quiere ir (colgando de la boca); bocas cerradas (cuadro «cola» con lengua). Ver README «Firmware 102».
- test_core 1088/1088; paneles 18; regresión igual; recorridos OK. Zip 102 rearmado, sin enviar.

# ACTUALIZACIÓN (16-nov-2026, 14:50) — firmware 102 hecho (Pochi mamá + cachorros + marca de enojo), SIN entregar

- Código en /home/claude/z100 (partió del zip 101 + 101b). Ver README «Firmware 102».
- Pedidos: Pochi hembra y mamá de Chapi (cochino amoroso), Pasita y Morita (tiernas) y Rony (el más loco, cosas chiquitas);
  «haz todo» (manías, reacciones, memoria, toques, cachorros, textos); se acuestan de espalda y se rascan el poto (sobre todo Pochi).
- Colores de los cachorros (pedido 14:17): Chapi y Rony color Pochi; Pasita más blanca; Morita un poco más clara; collares lisos
  (no perlas): Chapi verde, Pasita rosado, Morita morado, **Rony sin collar** (el usuario escribió «rony verde no tiene»: se
  interpretó sin collar; confirmar). Más chicos que Pochi (escala 4).
- Marca de enojo (ICO_ENOJO 48 px) redibujada: era un marco cuadrado (foto del usuario: «¡Y YO QUÉ!» con cuadrado rojo).
- (14:44) El nombre es **Pasita** (con s), no Pacita: cambiado en todo.
- test_core 1086/1086; paneles 18; regresión igual; soak OK. Zip `mascota_firmware_102.zip` armado, sin enviar.

# ACTUALIZACIÓN (16-nov-2026, 13:55) — 101b REHECHO desde el zip 101 (la sesión anterior se perdió) y ENTREGADO

- El usuario subió `mascota_firmware_101.zip` y dijo «Hazlo»: se rehízo todo el 101b (13:00 y 13:30–13:33 de abajo) sobre ese zip.
- Herramientas otra vez disponibles (GitHub responde): LVGL 9.4 en /home/claude/lvgl, wasi-sdk 25 y binaryen 123 en /home/claude/sdk.
- Probado: test_core 1077/1077; ui.c compila sin avisos; simulador armado: paneles 18 (base), regresión igual, inicio/libro/
  libro_flujo/final OK, soak 180 días. A ojo: hoja «Lo escondido de hoy» (una vez por lugar y día), mirador sin estrellitas,
  casa del rey sin estrellitas, pecera con el botón chico. Sin ESP-IDF (lo compila el usuario).
- Entregado: `mascota_firmware_101b.zip`. Pendiente igual que antes: noche sin cable, %/h con pantalla apagada, reposo del táctil.

# ACTUALIZACIÓN (16-nov-2026, 13:30) — pedidos nuevos, en el 101b

- Música de fábrica apagada y pecera 30 min: YA ERAN así (comprobado en pet_device_fix); solo se quitó que `DRAGON` encendiera
  la música. Botón del tiempo de la pecera más chico (44 px, anillo 56, letra escala 2). Sin estrellitas delatoras sobre los
  juegos del mirador ni las cosas de la casa del rey (la estrella de «lo escondido de hoy» se queda: es la pista del juego).
- Aviso del secreto del día (pedido 13:28): panel_lectura a pantalla completa al llegar a claro/casita/mirador con hallazgo,
  una vez al día por zona (`esc_anunciar` en ui_zonas.inc; no en infante ni sobre un panel/juego abierto). Aclarado al usuario:
  lo escondido dura todo el día; «desaparece» si ella lo recoge en su excursión (lo trae igual) o al cambiar el día.
- (13:33) Si ella encuentra el tesoro en su excursión, SE LO QUEDA (pet_excursion_back ya no llama a pet_zone_find_take: sin
  bolsa ni materiales; diario b 4-6 «me lo quedé», globo nuevo). El usuario también imaginó esconder pistas/historias/cartas de
  los personajes: le gustaron los tesoros, pero la idea queda anotada como posible mejora (mezclar de vez en cuando un capítulo
  o una carta como hallazgo).
- test_core 1077/1077. Zip armado: mascota_firmware_101b.zip (aún SIN entregar: el usuario no lo ha pedido desde entonces).

# ACTUALIZACIÓN (16-nov-2026, 13:00) — firmware 101b hecho, SIN entregar (esperar que lo pida)

- Código en /home/claude/z100. test_core 1075/1075. Sin compilar IDF ni simulador (sin LVGL, Bash sin red).
- Registro del 16-nov (fw 101 en placa): jugando 26,6 %/h; sueño ligero SÍ con pantalla apagada; RAM interna 37→5 KB en 6 h con
  «escuchar música» activada (micrófono ciclaba cada 20 s: fragmentó el DMA, bloque 3 KB, caída y reinicio #25).
- 101b arregla: micrófono queda abierto ≤5 min sin uso con pantalla encendida (cierra al apagarla, hal_sound_pantalla notifica);
  fondo sin adornos (casita del prado invisible hasta reiniciar) marcado con art_fondo_parcial() y reintentado cada 30 s por ui;
  dragón de la cueva: pet_prisma_ready() en core/historia.inc y ui_musica anuncia solo si hoy deja capítulo (test_v101b_prisma);
  línea «vivo» con bloque mayor de RAM interna.
- Pendiente: noche sin cable (sueño profundo sin probar), medición limpia %/h pantalla apagada en 101, reposo del táctil.

# ACTUALIZACIÓN (13-nov-2026, 10:30) — firmware 101 hecho, SIN entregar (el usuario pidió no mandar el zip aún)

- Código en /home/claude/z100 (si se pierde: partir del zip 100 y repetir lo del README «Firmware 101»).
- Pecera al tiempo elegido (también cargando; `ui_pecera_vencida`); reserva por 10 % o < 3,42 V; registro 12 KB (escritura
  anticipada máx. cada 2 min; `hal_log_read` sin pisar eventos); log «zona: A -> B»; vigilante de memoria sin botar la caché
  cada 1,5 s; micrófono sin candado en cada vuelta.
- «Siempre llegaba al lago»: era el FONDO que no se armaba (memoria) y quedaba el del lago. Índice de fondos fijo (217 KB, una vez),
  reintentos y prado sin adornos si no cabe el lienzo de 876 KB.
- Clima: decisión del usuario: con el clima que A ELLA no le gusta se va a su casita (se mantiene). Solo cambió: la tormenta ya no
  manda a las que la aman (gusto ≥ 20). `test_v101_clima_gusto`. Test del diario aflojado (la 1.ª página se puede ir si se llena).
- Sueño profundo de noche (etapa 4, pedido por el usuario): `BOARD_SUENO_NOCHE`, `hal_night_enter/boot_check/wake_reason`;
  despierta cada 5 s a mirar PWR (bit 0x49 leído antes de que hal_power_init lo borre) y el cable; BOOT al instante.
  sdkconfig.defaults: SPIRAM_MEMTEST=n, BOOTLOADER_SKIP_VALIDATE_IN_DEEP_SLEEP=y → borrar sdkconfig.
- test_core 1067/1067. NO se pudo compilar IDF ni correr simulador/paneles/regresión/soak (sin LVGL ni red).
- Sin hacer: reposo del táctil (el reset del CST9217 es GPIO2 según board.h: podría despertarlo, falta probar).

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
