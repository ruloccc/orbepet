# Plan de batería — mascota virtual (escrito el 11-nov-2026)

Estado: **solo plan, nada implementado.** Se haría sobre el firmware 98 (partida v41) como firmware 99.
Para empezar hace falta `mascota_firmware_98.zip` en la sesión.

## Medición real y etapa 3 (12-nov-2026) — firmware 100

- **Línea base medida (firmware 99, registro del 12-nov):** pantalla apagada, 100 % → 9 % en 536 min = **10,1 %/h** (~9 h).
  CPU a 80 MHz sin dormir nunca, vuelta cada 141 ms, panel 6 abierto, ella despierta, flash escrita cada minuto.
- **Falla:** a 8 % entró a reserva (sueño profundo) y no salió al enchufar: revisaba cada 20 min y exigía «cargando».
  Arreglado: cada 20 s, y basta el cable (VBUS).
- **Etapa 3 hecha (sin probar en placa):** `dormitar()` en main.c: LVGL en pausa + candado NO_LIGHT_SLEEP suelto con la pantalla
  apagada, sin cable ni sonido; tickless idle en sdkconfig.defaults (borrar `firmware/sdkconfig`); despierta BOOT por GPIO.
  Interrupciones del acelerómetro/AXP: no se usaron (pines sin confirmar): se sigue consultando 4 veces por segundo.
- **Siguiente:** medir %/h con pantalla apagada en el firmware 100 (`ENERGIA` → `ligero=1 dormitadas>0`). Si sigue alto:
  cerrar el panel al apagar, apagar el códec/amplificador del todo, pausa de 500 ms, y etapa 4 (sueño profundo de noche).

## Estado (11-nov-2026, 19:00) — firmware 99

**Hecho:**
- P0 en `tests/test_core.c` (`test_p0_salto`): 1 h, 8 h, 8 h cruzando la hora de dormir, 24 h y 3 días; 30 mascotas; promedio de
  la diferencia < 5 puntos por necesidad, y de un salto también le da hambre. test_core 1063/1063.
- **P0 encontró un error y se corrigió**: `azar_animal` cambia una vez por minuto, pero la llegada de animales, lo que hacen con
  ella y las criaturas que aparecen solas se sorteaban en cada vuelta con la probabilidad de la vuelta (mismo número muchas
  veces). Con latidos de 5 s la seguridad de noche quedaba ~10 puntos más baja que de un salto; en la placa (varias vueltas por
  segundo) los animales venían mucho menos. Ahora `prob_minutos()` sortea una vez por minuto cruzado.

**Verificado en el código:**
- El motor avanza por tiempo transcurrido (`pet_update`: pasos de 60 s, de 300 s si faltan más de 6 h; tope 14 días). OK.
- Ya existe ajuste de frecuencia: `CONFIG_PM_ENABLE=y`, `esp_pm_configure` 240/80 MHz **sin** sueño ligero, candado «rapido»;
  baja a 80 MHz con la pantalla apagada, sin cargar, sin sonido ni alarma, o encendida pero ella dormida y quieta > 8 s.
- Ciclo principal: pausa de 50 ms (encendida), 90 ms (dormida y quieta), 140 ms (pantalla apagada), 250 ms en «profundo»
  (pantalla apagada y ella dormida): ahí el motor se actualiza cada 3 s.
- Pantalla apagada: hoy solo brillo 0 + `ui_screen_on(false)` + táctil ignorado; **no** se mandan los comandos de reposo del panel.
- Ya hay registro: «actividad» cada 10 min (batería, pantalla, brillo, CPU, Bluetooth, micrófono, sonido, giro…), «bateria:
  ritmo %/hora» al cambiar el porcentaje, el comando `PROBAR ENERGIA` y la prueba de DESCARGA por etapas (peor caso / uso
  normal / dormida). La etapa 0 en la placa puede hacerse ya con eso.
- Bluetooth: baliza cada 2 s, siempre (NimBLE, solo anunciar y escuchar).
- Pines: board.h solo nombra BOOT (GPIO0) y la interrupción del táctil (GPIO11). Faltan por confirmar en el esquema de Waveshare
  las del QMI8658, AXP2101 y PCF85063.

**Etapas 1 y 2 hechas en el firmware 99 (falta probarlas en la placa):** reposo de verdad del panel y del táctil con la
pantalla apagada (`BOARD_PANEL_REPOSO`); atenuar a los 8 s sin tocar si no hay nada que leer ni jugar (40 % del brillo, LVGL
cada 66 ms, CPU 80 MHz; un toque la devuelve). LVGL 9.4 ya está en /home/claude/lvgl (se baja con
`git clone --depth 1 -b v9.4.0 https://github.com/lvgl/lvgl.git`) y binaryen 123 en /home/claude/sdk.
**Siguiente:** que el usuario corra en la placa las pruebas de las etapas 1 y 2 y la medición (registro «actividad» y
«bateria: ritmo», prueba de DESCARGA) y llene la tabla; con eso, etapa 3 (sueño ligero) en el firmware 100.

Placa: Waveshare ESP32-S3-Touch-AMOLED-1.75C (ESP32-S3, 8 MB PSRAM, AMOLED CO5300 por QSPI, AXP2101, QMI8658, PCF85063), ESP-IDF 5.5, LVGL 9.4, esp_lvgl_adapter.

## Regla de oro

**La mascota no se congela.** Dormir el procesador no detiene su vida: al despertar, el motor avanza el tiempo que pasó y ella tiene las necesidades acumuladas. Ninguna etapa se da por buena si cambia su comportamiento mientras no la usan (prueba P0).

Además, como siempre: no dañar lo existente (test_core, paneles = 28, regresión, soak), no entregar zip ni simulador hasta que se pida.

## Lo que hay que verificar en el código antes de empezar

Este plan se escribió sin el código a la vista (solo con el README). Confirmar primero:

- Que el motor avanza por tiempo transcurrido (reloj) y no por cantidad de latidos. Si alguna parte cuenta latidos, corregirla antes de todo.
- Qué hace hoy el ciclo con la pantalla apagada (el README menciona una pausa de 250 ms) y qué decide la frecuencia del procesador (el registro «Energía»).
- Qué pines de interrupción están cableados al ESP32: botón BOOT, QMI8658 (INT1/INT2), táctil, AXP2101, PCF85063.
- Si el AXP2101 entrega corriente de batería. Suposición: no; solo voltaje y porcentaje.
- Cómo se enciende hoy el Bluetooth (NimBLE) y cada cuánto anuncia.

---

## Etapa 0 — Medir (riesgo: ninguno)

**Qué hacer**
- Agregar al registro de la placa una línea de energía cada 5 minutos: hora, porcentaje y voltaje de batería, estado (encendida / pecera / apagada / cargando), brillo, frecuencia del procesador, Bluetooth sí/no.
- Comando de prueba por el cable (por ejemplo `ENERGIA`) que imprima el resumen: porcentaje por hora en cada estado.

**Pruebas**
- P0 (automática, en el computador, segundos): la misma mascota vivida 8 h latido a latido y 8 h de un solo salto termina con las mismas necesidades (margen chico). Repetir con 1 h, 24 h y 3 días, y cruzando la hora de dormir. Queda en `tests/test_core.c` para siempre.
- En la placa (1–2 h por estado, sin mirarla): pantalla encendida con brillo normal, pecera, pantalla apagada. Anotar el porcentaje por hora de cada uno: es la línea base contra la que se compara todo lo demás.

**Decisión con los datos:** si el gasto grande está con la pantalla apagada, priorizar etapas 1, 3 y 4; si está con la pantalla encendida, priorizar la etapa 2.

---

## Etapa 1 — Apagar de verdad lo que queda encendido (riesgo: bajo)

**Qué hacer**
- Panel AMOLED: al apagar la pantalla, mandar los comandos de reposo estándar (`0x28` pantalla apagada, `0x10` entrar en reposo) con `esp_lcd_panel_io_tx_param()`; al encender, `0x11`, esperar ~120 ms, `0x29`. Hoy queda en brillo 0.
- Táctil: ponerlo en reposo con la pantalla apagada (ya se ignora).
- Audio: apagar el amplificador y el códec cuando no suena nada; el micrófono solo mientras un juego pide soplar.
- Bluetooth: anunciar con intervalo largo, o solo con la pantalla encendida (decidir según para qué se usa hoy).

**Pruebas**
- Encender y apagar la pantalla 50 veces seguidas (botón y comando): siempre vuelve con imagen, sin parpadeos ni colores raros.
- Con la pantalla apagada: suena la alarma, suena el temporizador, ella hace su pedido y la pantalla se enciende.
- Sonido: el primer sonido después de un silencio largo no sale cortado ni con chasquido.
- Soplar sigue funcionando (niebla, velas, horno de galletas).
- Dos orbes se siguen encontrando por Bluetooth (si se usa).
- Medición: porcentaje por hora con la pantalla apagada, contra la línea base.

---

## Etapa 2 — Gastar menos con la pantalla encendida (riesgo: bajo; se ve en el simulador)

**Qué hacer**
- Atenuar a los 5–8 s sin tocar, antes del apagado elegido en Ajustes; cualquier toque devuelve el brillo.
- Mundo a 12–15 cuadros por segundo cuando nadie toca hace unos segundos; vuelve a 30 al primer toque y durante los juegos.
- Bajar la frecuencia del procesador en reposo (`esp_pm_configure` con máximo 240 y mínimo 80 MHz), manteniéndola alta en juegos y animaciones pesadas.
- Pecera y noche: más negro puro en el fondo (en AMOLED el negro no gasta) y menos cuadros por segundo.

**Pruebas**
- Simulador: paneles (28), regresión sin diferencias no buscadas, soak 180 días.
- A ojo: la atenuación no molesta al leer un globo de texto ni durante una historia; los juegos nunca bajan de ritmo.
- En la placa: el primer toque después del reposo responde al instante (sin el primer toque «perdido»); ms por cuadro en el registro.
- Medición: porcentaje por hora con la pantalla encendida y en pecera, contra la línea base.

---

## Etapa 3 — Sueño ligero con la pantalla apagada (riesgo: medio)

**Qué hacer**
- Activar `CONFIG_PM_ENABLE` y `CONFIG_FREERTOS_USE_TICKLESS_IDLE`, y `light_sleep_enable` en `esp_pm_configure()`.
- Despertar por interrupción: botón BOOT, acelerómetro, AXP2101 (enchufar el cargador) y temporizador.
- Antes de dormir, calcular el próximo momento en que hay que hacer algo (pedido de ella, mensaje del día de las 9/12/15/18/21 h, alarma, temporizador, guardado) y programar el despertador para entonces.
- Acelerómetro: que el QMI8658 avise por interrupción (movimiento / sacarlo del bolsillo) en vez de consultarlo cada 250 ms. Los pasos los sigue contando el chip.
- Con el cable USB conectado no dormir (el USB se corta en sueño ligero y el editor dejaría de verlo).
- Bluetooth: usar su modo de ahorro o apagarlo con la pantalla apagada.
- Tomar `esp_pm_lock` mientras se guarda la partida, suena algo o hay una transferencia del editor.

**Pruebas**
- P0 otra vez (no debe cambiar).
- Botón BOOT: 50 despertares seguidos, todos encienden en menos de medio segundo.
- Sacarlo del bolsillo enciende la pantalla como antes; caminando con él en el bolsillo no se enciende solo.
- Alarma, temporizador, mensaje del día y pedido de ella llegan a su hora (±1 min) después de 1 h dormido.
- Enchufar el cargador dormido: se alegra y entra a pecera como antes.
- Con cable: el editor y `prueba_total.py` funcionan igual; fase 28b completa.
- Pasos: caminar ~200 pasos con la pantalla apagada y comparar con antes.
- Una noche entera (8 h) con pantalla apagada: a la mañana, hora correcta, necesidades acumuladas, sin reinicios en el registro (revisar motivo de arranque).
- Medición: porcentaje por hora con la pantalla apagada. Aquí se espera la mayor mejora.

---

## Etapa 4 — Sueño profundo de noche (riesgo: medio-alto; cambia la experiencia)

**Qué hacer**
- Durante la hora de dormir, tras N minutos con la pantalla apagada: guardar y entrar en sueño profundo (mismo mecanismo del modo reserva). Despertar con el botón, al enchufarlo o un minuto antes de la próxima alarma.
- Al despertar, el motor avanza las horas transcurridas leyendo el PCF85063.
- Costo: 1–2 s de arranque al despertarlo de noche. Decidir con el usuario si vale la pena después de ver los números de la etapa 3.

**Pruebas**
- P0 con saltos de 6–10 h cruzando la medianoche.
- Alarma de la mañana suena a su hora tras una noche en sueño profundo.
- Despertarla a mitad de la noche: aparece dormida, en modo noche, sin pantalla de arranque molesta.
- No se pierde nada: partida, pasos del día, fotos, hora.
- Medición: porcentaje perdido en una noche de 8 h, contra la etapa 3.

---

## Tabla de resultados (llenar con la placa)

| Estado | Base (%/h) | Etapa 1 | Etapa 2 | Etapa 3 | Etapa 4 |
|---|---|---|---|---|---|
| Pantalla encendida, brillo normal | | | | | |
| Pecera | | | | | |
| Pantalla apagada (día) | | | | | |
| Noche de 8 h (% total) | | | | | |

## Orden y entregas

1. Etapa 0 + Etapa 1 + Etapa 2 en un firmware 99 (riesgo bajo).
2. Medir en la placa y llenar la tabla.
3. Etapa 3 en un firmware 100; medir.
4. Etapa 4 solo si los números de la etapa 3 no alcanzan.

En cada firmware: test_core completo (con P0), paneles = 28, regresión, soak, y la sección nueva en el README.

## Fuera de este plan

Paneles translúcidos: se dejó de lado a favor de la batería (se queda el «vidrio» actual).
