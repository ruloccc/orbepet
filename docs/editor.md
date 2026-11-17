# Lo que el editor de Windows puede leer y cambiar en el reloj

**El editor** está en `editor/`: `editor.html` es una sola página (con los 151 íconos, los 51 sonidos
y los 8 sonidos de fondo de fábrica adentro) que habla con el reloj por el cable usando Web Serial,
así que necesita **Microsoft Edge o Google Chrome**. `editor/windows/Instalar.cmd` la deja como
app de escritorio (ventana propia de Edge). Para rearmarla después de cambiar el firmware:

```
cd sim/preview && cmake --build build --target mascota_editor_datos && ./build/mascota_editor_datos ../../editor
cd ../../editor && python3 build.py
```

`editor/pruebas/prueba_editor.js` la prueba con un reloj de mentira (Playwright).

El reloj se conecta por el cable USB‑C (el USB nativo del ESP32‑S3, que aparece como puerto
serie en Windows). El editor manda **una línea de texto por comando** y lee las líneas que
empiezan con `MASCOTA`; las demás son mensajes de diagnóstico del reloj y se ignoran.

| Comando | Respuesta |
|---|---|
| `MASCOTA HOLA` | `MASCOTA OK <versión de partida> <nombre>` |
| `MASCOTA RESPALDO` | `MASCOTA RESPALDO <versión> <base64 de la partida>` |
| `MASCOTA RESTAURAR <base64>` | `MASCOTA ESPERANDO`; después `MASCOTA RESTAURADA` o `MASCOTA RECHAZADA` (se confirma en la pantalla del reloj) |
| `MASCOTA FIESTAS` | `MASCOTA FIESTAS <base64 de la tabla de celebraciones>` |
| `MASCOTA FIESTAS <base64>` | `MASCOTA OK` o `MASCOTA ERROR celebraciones no validas` |
| `MASCOTA HORA <unix utc> <zona en segundos>` | `MASCOTA HORA OK 1` si ajustó la hora o la zona, `MASCOTA HORA OK 0` si ya estaba bien. El editor la manda al conectar, con la hora y la zona del computador (la zona queda guardada: así sigue el horario de verano o de invierno) |
| `MASCOTA FIESTAS FABRICA` | `MASCOTA OK` (vuelven las de fábrica) |
| `MASCOTA FOTOS` | `MASCOTA FOTOS <n> <id>:<bytes> <id>:<bytes> ...` (de la más nueva a la más vieja) |
| `MASCOTA FOTO <id>` | varias líneas `MASCOTA FOTO <id> <total> <desde> <base64>` y al final `MASCOTA FOTO <id> FIN` |
| `MASCOTA BORRAR FOTO <id>` | `MASCOTA OK` o `MASCOTA ERROR` |
| `MASCOTA CAMBIOS` | `MASCOTA CAMBIOS I <ids> S <ids> A <ids> C <especie*10+etapa> N <especies>`: qué íconos, sonidos, sonidos de fondo, criaturas y nombres de especie están cambiados |
| `MASCOTA CRIATURA <especie> <etapa> <base64>` | `MASCOTA OK`: el cuerpo dibujado de una especie (0–7) en una etapa (1 bebé, 2 cría, 3 adulta); formato abajo |
| `MASCOTA CRIATURA <especie> <etapa> FABRICA` | `MASCOTA OK` (vuelve el dibujo de fábrica) |
| `MASCOTA ESPECIE <especie> NOMBRE <texto>` | `MASCOTA OK`: el nombre de la especie (hasta 15 bytes UTF‑8) |
| `MASCOTA ESPECIE <especie> FABRICA` | `MASCOTA OK` (vuelve el nombre de fábrica) |
| `MASCOTA ICONO <n> <base64>` | `MASCOTA OK`: 24 × 24 píxeles RGBA (2304 bytes); alfa < 128 es transparente. También acepta los de 12 × 12 (576 bytes) del editor anterior |
| `MASCOTA ICONO <n> FABRICA` | `MASCOTA OK` (vuelve el original) |
| `MASCOTA SONIDO <n> INICIO <bytes>` | `MASCOTA OK`. Después `DATOS <desde> <base64>` (responde `MASCOTA OK <recibidos>`) y `FIN <crc32 hex>` |
| `MASCOTA SONIDO <n> FABRICA` | `MASCOTA OK` |
| `MASCOTA AMBIENTE <n> …` | igual que `SONIDO`, para los sonidos de fondo |

Al conectar, Windows puede reiniciar el reloj (señales DTR/RTS); el editor las apaga apenas abre
el puerto. La mascota no se pierde: está guardada.

## Íconos y sonidos propios

Se guardan en la partición `recursos` (5 MB, SPIFFS, montada en `/res`) y se cargan al encender:
`/res/i<n>.bin` para los íconos (24 × 24 o 12 × 12 RGBA, según el largo), `/res/s<n>.pcm` y `/res/a<n>.pcm` para los sonidos. Los sonidos son
PCM de 16 bits con signo, mono, 16 kHz, little endian: hasta **8,5 s** los sonidos y **8,6 s** los de
fondo (el editor los deja en 8 s y funde el final con el comienzo para que el bucle no se note).
Se mandan en trozos de 2400 bytes; el reloj revisa el crc32 al final y, si está bien, guarda el
archivo y lo toca una vez. Si un archivo falta o está dañado, se usa el de fábrica.

## Criaturas propias

Cada especie se puede redibujar en cada etapa. Se dibuja **solo el cuerpo** (30 × 30 puntos, sin borde):
el reloj le pone encima los ojos, la boca y las caras de cada emoción, las mejillas, las lágrimas, las
patitas que caminan, la ropa, la postura (orgullosa, encogida, echada), el saltito al caminar y el borde
oscuro, igual que a las de fábrica. No hay que animar nada. El color que se elige al empezar una mascota
también tiñe el dibujo, y la forma radiante o gruñona y estar enferma lo cambian como a las de fábrica.

Formato (archivo `/res/c<especie><etapa>.bin` y lo que va en base64), 3612 bytes:

```
"CUER"        4 bytes
versión       1 byte (2)
ojo_x, ojo_y  1 byte cada uno: el ojo izquierdo (1–24, 1–25)
ojos_sep      1 byte: distancia al otro ojo (4–15)
patitas       1 byte: 1 = dibujar patitas que caminan debajo
relleno       3 bytes (0)
pixeles       30 × 30 × R, G, B, A (alfa < 128 es transparente)
```

La versión 1 (la del editor anterior: 20 × 20, 1612 bytes, ojos en 1–16 y 1–17, separación 3–10) se
sigue aceptando y se ve igual que antes. El reloj dibuja la cara en su grilla de 20 × 20 (cada punto de
ella son 1,5 puntos del dibujo), así que los ojos se acomodan a esa grilla.

Los nombres de especie van en `/res/n<especie>.txt`. Se ven al elegir una mascota nueva y en el panel Más.

## Paquete

El editor también guarda todo en un **paquete** `.mascotapack` (JSON, versión 3, con los íconos en
base64 RGBA de 24 × 24, las criaturas en el formato CUER, los nombres de especie, los sonidos ya procesados y las
celebraciones) para llevarlo a otro computador o a otro reloj. Abre también los paquetes del editor anterior (versión 2): sus
íconos y criaturas se agrandan a la grilla nueva.

El **simulador** también lo abre (Creatividad → *Traer lo del editor…*): así se ven las criaturas,
los íconos y los sonidos nuevos sin tener el reloj.

## Respaldo de la mascota

Un respaldo es un archivo de texto `.mascota`:

```
MASCOTA-RESPALDO 14
<base64 de la partida completa>
```

Contiene **todo**: el personaje (especie, etapa, forma, personalidad), sus necesidades y
emociones, el vínculo, la memoria afectiva, lo aprendido (trucos, nadar, trepar), el diario, el
álbum, las colecciones, el jardín, los amigos y los ajustes. La partida lleva su propio crc32,
así que un archivo dañado o cortado no se acepta. Traer una mascota **siempre se confirma en el
reloj** ("¿Traer a Luna? Reemplaza a Kiwi").

El mismo archivo sirve en el simulador web (botones *Descargar respaldo* y *Traer un respaldo*),
así que se puede pasar una mascota del simulador al reloj y al revés.

**Versiones**: desde la versión 14 lo nuevo de la partida se agrega siempre al final (la 15 sumó las alarmas y la 16 los
perfiles que siguen viviendo, el temporizador y los pasos), así que un respaldo de una versión anterior (desde la 13) se actualiza solo al traerlo, igual que la partida
guardada en el reloj o en el simulador después de actualizar el programa. Uno de una versión **más nueva**
que la del reloj no se puede traer.

## Fotos

Las fotos que le sacas en el reloj (Más → libro → cámara) son la pantalla tal cual (466 × 466, sin
botones), guardadas en la partición `fotos` como archivos **QOI** ("Quite OK Image", formato
público y sin pérdida: https://qoiformat.org). El editor junta los trozos en base64 de
`MASCOTA FOTO`, obtiene el archivo `.qoi` y lo convierte a PNG. En el simulador se bajan directo
como PNG.

## Celebraciones

Las fechas especiales son una tabla de hasta **16 celebraciones** guardada en el reloj, aparte
de la partida (si traes otra mascota, las celebraciones no cambian). Cada una usa **una de las
decoraciones que ya existen**: el editor no puede inventar decoraciones nuevas, solo elegir.
Con el editor de íconos se puede cambiar cómo se ve cada decoración, y el cambio aparece en
todas las celebraciones que la usan.

### Decoraciones disponibles

| id | decoración | se ve en el mundo |
|---|---|---|
| 1 | Halloween | calabazas |
| 2 | Navidad | arbolito con un regalo |
| 3 | Año Nuevo | fuegos artificiales de noche |
| 4 | Amor | corazones |
| 5 | Primavera | flores |
| 6 | Fiestas | banderines |

### Cómo se elige el día

| regla | id | campos |
|---|---|---|
| Día fijo | 0 | `mes` 1–12, `dia` 1–31 (el 29 de febrero cae el 28 en los años no bisiestos) |
| N‑ésimo día de la semana | 1 | `mes`, `dia` = 1–4 (o 5 = el último), `semana` = 0 domingo … 6 sábado. Ej.: Día de la Madre en Chile = 2º domingo de mayo |
| Primer día de primavera | 2 | sin campos: 22/9 en el hemisferio sur, 21/3 en el norte |

`dias` (1–7) es cuánto dura; puede cruzar el año (Año Nuevo: 31/12 por 2 días).

### Formato binario (lo que va en base64)

```
"FIES"            4 bytes
versión           1 byte  (1)
cantidad          1 byte  (0..16)
crc32             4 bytes, little endian, de la tabla
tabla             cantidad × 80 bytes:
  regla, mes, dia, semana, dias, deco    1 byte cada uno
  relleno                                2 bytes (0)
  nombre                                 32 bytes UTF‑8 con cero final (para el diario)
  saludo                                 40 bytes UTF‑8 con cero final (el globo de ese día)
```

El crc32 es el estándar (polinomio 0xEDB88320, como zip). El reloj revisa todo antes de
aceptarla: si una celebración no es válida, no cambia nada. Las de fábrica están en
`celebraciones.json`.
