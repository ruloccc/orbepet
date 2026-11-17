# Catálogo de sonidos

Todos los sonidos se sintetizan en el reloj (`core/sfx.c`), sin archivos de audio. Se generan desde `PET_SONIDOS` en `core/pet.h`; esta lista y `sonidos.json` son la referencia para el editor de Windows, que podrá reemplazar cada uno por un sonido propio (mono, 16 bits, hasta 8.5 s). En `docs/sonidos/` hay un `.wav` de cada uno tal como suena hoy.

## Ánimo (cómo se siente)

| # | Sonido | Nombre | Qué significa | Cuándo suena | Duración | Archivo |
|---|---|---|---|---|---|---|
| 1 | `SND_ALEGRE` | Alegre | Dos notas que suben: está contenta | Le gustó lo que hiciste; despierta descansada; nace; va a una zona que le gusta | 0.35 s | `sonidos/alegre.wav` |
| 2 | `SND_RISA` | Risa | Risita juguetona | Jugar; sacudida que le gusta; saltar en las hojas; muchos toques si le hacen cosquillas | 0.37 s | `sonidos/risa.wav` |
| 3 | `SND_RONRONEO` | Ronroneo | Ronroneo grave: está a gusto | Caricia que le encanta; cepillarla; la bebida justa | 1.06 s | `sonidos/ronroneo.wav` |
| 4 | `SND_QUEJA` | Queja | Nota que baja: no le gustó | Comida que odia; toque que no le gusta; algo que no quería | 0.47 s | `sonidos/queja.wav` |
| 5 | `SND_LLANTO` | Llanto | Lloriqueo: está triste | Muy triste o te extraña | 0.58 s | `sonidos/llanto.wav` |
| 6 | `SND_GRUNIDO` | Gruñido | Gruñido: está enojada o celosa | No quiso hacer algo; la despertaste; mimaste a otro | 0.39 s | `sonidos/grunido.wav` |
| 13 | `SND_SORPRESA` | Sorpresa | Nota que sube rápido: algo nuevo | Visita; algo nuevo; se aparta porque todavía desconfía; zona nueva | 0.31 s | `sonidos/sorpresa.wav` |
| 14 | `SND_SUSPIRO_AMOR` | Suspiro de amor | Está enamorada | Se enamora de una visita | 0.57 s | `sonidos/suspiro_amor.wav` |
| 20 | `SND_SUSPIRO` | Suspiro | Suspiro aburrido o triste | Aburrida o triste | 0.44 s | `sonidos/suspiro.wav` |
| 22 | `SND_ABRAZO` | Abrazo | Sonido cálido y largo | La abrazas (mantener el dedo sobre ella) | 0.84 s | `sonidos/abrazo.wav` |
| 23 | `SND_MAREO` | Mareo | Sonido que da vueltas: la mareaste | Muchos toques seguidos | 0.95 s | `sonidos/mareo.wav` |

## Cuerpo

| # | Sonido | Nombre | Qué significa | Cuándo suena | Duración | Archivo |
|---|---|---|---|---|---|---|
| 7 | `SND_BOSTEZO` | Bostezo | Tiene sueño | Se duerme; está muy cansada para jugar | 0.76 s | `sonidos/bostezo.wav` |
| 8 | `SND_RONQUIDO` | Ronquido | Está durmiendo | La tocas mientras duerme | 0.89 s | `sonidos/ronquido.wav` |
| 9 | `SND_ESTORNUDO` | Estornudo | Se resfrió o le dio frío | Se enferma; una bebida fría cuando tiene frío | 0.31 s | `sonidos/estornudo.wav` |
| 10 | `SND_TOS` | Tos | Está enferma | Quieres jugar y está enferma | 0.18 s | `sonidos/tos.wav` |

## Lo que hace

| # | Sonido | Nombre | Qué significa | Cuándo suena | Duración | Archivo |
|---|---|---|---|---|---|---|
| 11 | `SND_COMER` | Comer | Ñam ñam | Come | 0.40 s | `sonidos/comer.wav` |
| 12 | `SND_CHAPOTEO` | Chapoteo | Agua | Baño; regar; charcos | 0.45 s | `sonidos/chapoteo.wav` |
| 15 | `SND_PELEA` | Pelea | Ruido de pelea | Pelea con una visita | 0.33 s | `sonidos/pelea.wav` |
| 16 | `SND_LLAMADO` | Llamado | Te llama: necesita algo | Pide comida, baño, juego, mimos o medicina | 0.35 s | `sonidos/llamado.wav` |
| 18 | `SND_OLFATEO` | Olfateo | Olfatea | Empieza a olfatear | 0.23 s | `sonidos/olfateo.wav` |
| 19 | `SND_CAVAR` | Cavar | Escarba la tierra | Cava; plantas una semilla | 0.46 s | `sonidos/cavar.wav` |
| 21 | `SND_TOQUE` | Palmadita | Un boop suave | Le das una palmadita (un toque) | 0.12 s | `sonidos/toque.wav` |
| 26 | `SND_PESCO` | ¡Pescó! | Notas alegres: sacó algo del agua | Termina de pescar y sacó algo | 0.39 s | `sonidos/pesco.wav` |
| 41 | `SND_CHAPUZON` | Chapuzón | Se tira al agua | Empieza a nadar | 0.27 s | `sonidos/chapuzon.wav` |
| 42 | `SND_CAIDA` | ¡Pum! | Golpecito suave al caer | Se cae del árbol mientras aprende a trepar | 0.23 s | `sonidos/caida.wav` |
| 43 | `SND_ROPA` | Ropa | Frufrú de tela | Le pones o le quitas ropa | 0.28 s | `sonidos/ropa.wav` |
| 48 | `SND_FOTO` | Foto | Clic de cámara | Le sacas una foto | 0.13 s | `sonidos/foto.wav` |
| 49 | `SND_BANO` | Baño | Agua, burbujas y un brillito: quedó limpia | La bañas | 1.40 s | `sonidos/bano.wav` |
| 50 | `SND_CEPILLO` | Cepillo | Cepillado suave y ronroneo | La cepillas | 1.15 s | `sonidos/cepillo.wav` |
| 51 | `SND_LIMPIO` | ¡Limpio! | Tres notitas brillantes | Quedó limpio (se fue la suciedad) | 0.34 s | `sonidos/limpio.wav` |
| 52 | `SND_SALPICON` | Salpicón | Un chapoteo chiquito | Cae en un charco al saltar | 0.20 s | `sonidos/salpicon.wav` |

## Juegos y trucos

| # | Sonido | Nombre | Qué significa | Cuándo suena | Duración | Archivo |
|---|---|---|---|---|---|---|
| 29 | `SND_LANZAR` | Lanzar | Silbido: la pelota sale volando | Tiras la pelota con el dedo | 0.25 s | `sonidos/lanzar.wav` |
| 30 | `SND_REBOTE` | Rebote | Boing de la pelota al rebotar | La pelota toca el suelo | 0.12 s | `sonidos/rebote.wav` |
| 31 | `SND_ATRAPA` | ¡Atrapada! | Pop alegre: algo atrapado | Atrapa la pelota; atrapas una fruta con la canasta | 0.29 s | `sonidos/atrapa.wav` |
| 32 | `SND_TRUCO` | Truco | Fanfarria corta: lo hizo bien | Hace un truco | 0.49 s | `sonidos/truco.wav` |
| 33 | `SND_CONTAR` | Contar | Tres pitidos: uno, dos, tres | Escondidas: cuentas mientras se esconde | 0.40 s | `sonidos/contar.wav` |
| 34 | `SND_ENCONTRADA` | ¡Te encontré! | Ta-dá: la encontraste | Escondidas: la encuentras | 0.45 s | `sonidos/encontrada.wav` |
| 35 | `SND_DESLIZA` | ¡Wiii! | Se resbala divertida | Inclinas el reloj y se desliza | 0.61 s | `sonidos/desliza.wav` |
| 39 | `SND_META` | Estampita | Ding de premio: cumplieron la meta | Cumplen la meta del día | 0.39 s | `sonidos/meta.wav` |
| 45 | `SND_PELUCHE` | Peluche | Chillido suave de juguete | Abraza su peluche | 0.25 s | `sonidos/peluche.wav` |
| 46 | `SND_TAMBOR` | Tambor | Tres golpes de tambor | Toca el tambor | 0.56 s | `sonidos/tambor.wav` |
| 47 | `SND_COLUMPIO` | Columpio | Chirrido suave que va y viene | Se columpia | 0.80 s | `sonidos/columpio.wav` |

## El mundo

| # | Sonido | Nombre | Qué significa | Cuándo suena | Duración | Archivo |
|---|---|---|---|---|---|---|
| 17 | `SND_TRUENO` | Trueno | Trueno de la tormenta | Tormenta | 1.15 s | `sonidos/trueno.wav` |
| 24 | `SND_HOLA_AMIGO` | Llega su amigo | Saludo de su amigo del barrio | Llega su amigo a visitarla | 0.39 s | `sonidos/hola_amigo.wav` |
| 25 | `SND_PLOP` | Plop | El anzuelo cae al agua | Empieza a pescar | 0.18 s | `sonidos/plop.wav` |
| 36 | `SND_TESORO` | Tesoro | Campanitas: desenterró un tesoro | Encuentra una pieza de su colección | 0.62 s | `sonidos/tesoro.wav` |
| 37 | `SND_HUELE` | Huele algo | Olfateo con un pitido curioso: aquí hay algo | Huele un tesoro escondido | 0.29 s | `sonidos/huele.wav` |
| 38 | `SND_RARO` | Algo mágico | Brillitos que suben: pasa algo raro y bonito | Arcoíris, estrella fugaz, ciervo, globo | 0.64 s | `sonidos/raro.wav` |
| 44 | `SND_NUEVO` | Algo nuevo | Cajita que se abre: hay algo nuevo en el baúl | Aparece ropa o un juguete nuevo | 0.45 s | `sonidos/nuevo.wav` |
| 53 | `SND_SEMILLA` | Semilla nueva | Tres notas que suben: encontró un tipo de semilla nuevo | Encuentra una semilla que todavía no tenía | 0.45 s | `sonidos/semilla.wav` |
| 60 | `SND_CAJA_MUSICA` | Caja de música | Una melodía chiquita de caja de música, a un pulso fijo para bailar | Usa la caja de música antigua: ella baila | 7.95 s | `sonidos/caja_musica.wav` |
| 67 | `SND_PRIMAVERA` | Melodía de primavera | Una melodía original, tocada con las notas de los cristales de la cueva | Suena el 21 de septiembre, el Día de la Primavera | 8.12 s | `sonidos/primavera.wav` |

## Música

| # | Sonido | Nombre | Qué significa | Cuándo suena | Duración | Archivo |
|---|---|---|---|---|---|---|
| 27 | `SND_BIENVENIDA` | Bienvenida | Melodía corta de bienvenida | La primera vez que se enciende (y al crear una mascota nueva) | 1.50 s | `sonidos/bienvenida.wav` |
| 28 | `SND_CUMPLEANOS` | Cumpleaños feliz | La melodía tradicional del cumpleaños (dominio público) | Tu cumpleaños; al tocar el pastel | 7.75 s | `sonidos/cumpleanos.wav` |
| 40 | `SND_FIESTA` | Fiesta | Cornetita de fiesta | Su cumpleaños; fechas especiales; colección completa | 1.05 s | `sonidos/fiesta.wav` |

## Alarmas

| # | Sonido | Nombre | Qué significa | Cuándo suena | Duración | Archivo |
|---|---|---|---|---|---|---|
| 54 | `SND_DESPERTADOR` | Despertador | Melodía alegre que se repite: ¡a despertar! | La alarma de despertar (se repite hasta que respondes) | 2.73 s | `sonidos/despertador.wav` |
| 55 | `SND_A_DORMIR` | Hora de dormir | Canción de cuna corta y suave | La alarma de irse a dormir | 5.14 s | `sonidos/a_dormir.wav` |

## El viaje de despedida

| # | Sonido | Nombre | Qué significa | Cuándo suena | Duración | Archivo |
|---|---|---|---|---|---|---|
| 56 | `SND_AVION` | Viento mágico | Un soplo de viento con campanitas que suben | Se va de viaje volando en una pluma gigante | 1.60 s | `sonidos/avion.wav` |
| 57 | `SND_BOCINA` | Campana del barco | Dos campanadas suaves sobre el agua | Se va de viaje en un barquito de hoja por el lago de estrellas | 2.10 s | `sonidos/bocina.wav` |
| 58 | `SND_SILBATO` | Silbato de cristal | Un chu-chuu de cristal y chispitas | Se va de viaje en el tren de luciérnagas | 2.60 s | `sonidos/silbato.wav` |
| 59 | `SND_GLOBO` | Diente de león | Un soplido suave y semillas que brillan | Se va de viaje colgada de un diente de león gigante | 1.55 s | `sonidos/globo.wav` |

## Sonido ambiente (bucles de fondo)

Suenan bajito de fondo mientras la pantalla está encendida (en Ajustes: apagado, bajito o normal). Cada uno es un bucle de 8 s que empalma sin cortes. El parlante del reloj es chico: se oyen bien la lluvia, los grillos y el viento, pero sin graves. Para reemplazarlos: mono, 16 bits, 16 000 Hz, y que el final empalme con el comienzo.

| # | Sonido | Nombre | Cuándo suena | Archivo |
|---|---|---|---|---|
| 1 | `AMB_PRADO` | Prado | De día en el prado: brisa y pajaritos | `sonidos/amb_prado.wav` |
| 2 | `AMB_BOSQUE` | Bosque | De día en el bosque: hojas y pájaros lejanos | `sonidos/amb_bosque.wav` |
| 3 | `AMB_LAGO` | Lago | De día en el lago: el agua contra la orilla | `sonidos/amb_lago.wav` |
| 4 | `AMB_CUEVA` | Cueva | Siempre en la cueva: gotas con eco | `sonidos/amb_cueva.wav` |
| 5 | `AMB_NOCHE` | Grillos | De noche (salvo en invierno o con frío) | `sonidos/amb_noche.wav` |
| 6 | `AMB_LLUVIA` | Lluvia | Cuando llueve | `sonidos/amb_lluvia.wav` |
| 7 | `AMB_TORMENTA` | Tormenta | Con tormenta: lluvia fuerte y viento (los truenos suenan aparte) | `sonidos/amb_tormenta.wav` |
| 8 | `AMB_VIENTO` | Viento | Con viento, y de día con frío o nieve | `sonidos/amb_viento.wav` |
| 9 | `AMB_MAR` | Mar | De día en la isla: olas que llegan y se retiran, y una gaviota | `sonidos/amb_mar.wav` |
