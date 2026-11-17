# Catálogo de íconos

Cada ícono se diseña en 12×12 y se pinta con más detalle: a tamaño de botón (escala 4) sale de 24×24 pixeles, cada uno de 2×2 en pantalla. En el editor se dibujan en 24×24. Se generan desde `ART_ICONOS` en `firmware/main/art.h`; esta lista y `iconos.json` las usa el editor.

## Botones

| # | Ícono | Nombre | Qué significa | Dónde aparece |
|---|---|---|---|---|
| 0 | `ICO_PLATO` | Comer | Darle de comer: abre la comida, la fruta y la miel | Barra de botones; globo cuando tiene hambre |
| 1 | `ICO_PELOTA` | Jugar | Jugar con ella; también: quiere jugar o jugaron | Barra de botones; globo y pensamiento |
| 2 | `ICO_DUCHA` | Bañar | Darle un baño; también: quiere bañarse; higiene | Barra de botones; globo; panel Más |
| 3 | `ICO_ESTADO` | Más | Abre cómo se siente, sus gustos y las demás acciones | Barra de botones |
| 4 | `ICO_BOMBILLA` | Luz | Apagar la luz para que duerma (o encenderla) | Panel Más |
| 5 | `ICO_JERINGA` | Curar | Curarla cuando se siente mal (usa una hierba medicinal) | Panel Más; globo cuando está enferma |
| 6 | `ICO_CEPILLO` | Cepillar | Cepillarla: la limpia un poco y le da cariño | Panel Más |
| 7 | `ICO_MACETA` | Rincón | Decorar su rincón con lo que encontró | Panel Más |
| 8 | `ICO_AMIGOS` | Amigos | Amigos cercanos (otros orbes); también: una visita | Panel Más; globo; pensamiento |
| 9 | `ICO_AJUSTES` | Ajustes | Hemisferio de las estaciones y mascota nueva | Panel Más |
| 10 | `ICO_SI` | Sí | Aceptar / listo | Invitaciones, tarjetas, rincón (decoración ya puesta) |
| 11 | `ICO_NO` | No | Rechazar; también: no le gusta o no quiere | Invitaciones; reacción; gustos (lo que odia) |
| 218 | `ICO_MAPA` | Mapa | Las zonas: adónde puede ir y cuáles faltan por descubrir | Panel Más |
| 221 | `ICO_LIBRO_MAGICO` | Libro mágico | El libro de los objetos mágicos que encontró | Más → recuerdos; la Bolsa |
| 222 | `ICO_ESPONJA` | Esponja | Bañarla: aparece cuando está sucia y se frota sobre ella | Barra de botones |
| 223 | `ICO_BOLSA` | La Bolsa | Los objetos mágicos que encontró: usarlos, devolverlos al bosque o mirar el libro | Panel Más; botón + cuando trae algo nuevo |
| 224 | `ICO_BOSQUE_DEV` | Al bosque | Devolver un objeto mágico al bosque | La Bolsa |
| 225 | `ICO_MANO_FROTA` | Frotar | Este objeto se usa frotándolo con el dedo | La Bolsa |
| 226 | `ICO_MANO_TIENE` | Mantener | Este objeto se usa manteniendo el dedo encima | La Bolsa |
| 227 | `ICO_MANO_DESLIZA` | Deslizar | Este objeto se usa deslizando el dedo | La Bolsa |
| 233 | `ICO_SOPLA` | Sopla | Sopla al orbe (el micrófono): despeja la niebla, llena la vela, apaga las velas | Niebla; el cruce en bote; tu cumpleaños |

## Comida y hallazgos

| # | Ícono | Nombre | Qué significa | Dónde aparece |
|---|---|---|---|---|
| 12 | `ICO_COMIDA` | Comida | La comida de siempre (nunca se acaba) | Panel Comer |
| 13 | `ICO_FRUTA` | Fruta | Fruta: sana; cae del árbol cada día o la encuentra | Panel Comer, Rincón, hallazgos |
| 14 | `ICO_MIEL` | Miel | Miel: dulce, la divierte pero no es muy sana | Panel Comer, Rincón, hallazgos |
| 15 | `ICO_HIERBA` | Hierba medicinal | Se usa al curarla | Panel Más (cantidad), Rincón, hallazgos |
| 16 | `ICO_SEMILLA` | Semilla | Sirve para decorar (flores, hongos...) | Rincón, hallazgos |
| 17 | `ICO_PIEDRA` | Piedra brillante | Sirve para decorar (farol, banco, casita) | Rincón, hallazgos; pensamiento: encontró algo |
| 18 | `ICO_TAZA` | Chocolate caliente | Bebida caliente: la abriga cuando tiene frío (con calor, quema) | Panel Comer; globo cuando tiene frío; pensamiento |
| 19 | `ICO_VASO` | Jugo con hielo | Bebida fría: la refresca cuando tiene calor (con frío, ¡brrr!) | Panel Comer; globo cuando tiene calor; pensamiento |
| 20 | `ICO_RAMITA` | Ramita | Algo que encontró y te regala con cariño (no sirve) | Hallazgos |
| 149 | `ICO_GALLETA` | Galletas | Las galletas que trae su amigo para compartir | Picnic con su amigo |
| 228 | `ICO_PINA` | Piña | Un regalo al volver: una piña con semillas | Cuando vuelves después de un rato |
| 229 | `ICO_SAMARA` | Semilla con alas | Un regalo al volver: la planta y crece un arbolito | Cuando vuelves después de un rato |
| 230 | `ICO_HUELLA` | Huella de barro | Trajo barro en las patas: un baño las limpia | El prado |

## Decoraciones

| # | Ícono | Nombre | Qué significa | Dónde aparece |
|---|---|---|---|---|
| 21 | `ICO_FLOR` | Flores | Decoración: flores en el prado | Rincón; prado |
| 22 | `ICO_HONGO` | Hongos | Decoración: hongos | Rincón; prado |
| 23 | `ICO_FAROL` | Farol | Decoración: ilumina de noche | Rincón; prado |
| 24 | `ICO_BANCO` | Banco | Decoración: banco | Rincón; prado |
| 25 | `ICO_CASITA` | Casita | Decoración: la protege de la lluvia; también: seguridad | Rincón; prado; panel Más |

## Clima

| # | Ícono | Nombre | Qué significa | Dónde aparece |
|---|---|---|---|---|
| 26 | `ICO_SOL` | Sol | Despejado; también: encender la luz | Arriba de la pantalla; globo; pensamiento |
| 27 | `ICO_NUBE` | Nublado | Día gris | Arriba; globo; pensamiento |
| 28 | `ICO_LLUVIA` | Lluvia | Llueve | Arriba; globo; pensamiento |
| 29 | `ICO_VIENTO` | Viento | Hay viento | Arriba; globo; pensamiento |
| 30 | `ICO_COPO` | Nieve | Nieva (solo en invierno) | Arriba; globo; pensamiento |
| 31 | `ICO_RAYO` | Tormenta | Tormenta con truenos (asusta a las tímidas) | Arriba; globo; pensamiento |
| 32 | `ICO_FRIO` | Frío | Hace mucho frío (a algunas les gusta, otras se abrigan) | Arriba; globo; pensamiento |
| 33 | `ICO_CALOR` | Calor | Tiene calor (verano, mediodía, sol) | Pensamiento; globo |
| 34 | `ICO_LUNA` | Noche / sueño | Tiene sueño o es hora de dormir; energía | Globo; pensamiento; panel Más |

## Emociones

| # | Ícono | Nombre | Qué significa | Dónde aparece |
|---|---|---|---|---|
| 35 | `ICO_CARA_FELIZ` | Alegría | Está feliz | Pensamiento; panel Más |
| 36 | `ICO_CARA_TRISTE` | Tristeza | Está triste | Pensamiento; panel Más |
| 37 | `ICO_CARA_ENOJADA` | Enojo | Está de mal humor | Pensamiento; panel Más |
| 38 | `ICO_CARA_MIEDO` | Miedo | Tiene miedo | Pensamiento; panel Más |
| 39 | `ICO_CARA_CURIOSA` | Curiosidad | Algo le llama la atención | Pensamiento; panel Más |
| 40 | `ICO_CARA_ABURRIDA` | Aburrimiento | Se aburre | Pensamiento; panel Más |
| 41 | `ICO_CARA_CANSADA` | Cansancio | Tiene sueño | Pensamiento; panel Más |
| 42 | `ICO_CARA_ORGULLOSA` | Orgullo | Está orgullosa de algo que logró | Pensamiento; panel Más |
| 43 | `ICO_CARA_CELOSA` | Celos | Siente celos | Pensamiento; panel Más |
| 44 | `ICO_CARA_CARINO` | Cariño | Te quiere mucho | Pensamiento; panel Más |

## Causas (por qué siente algo)

| # | Ícono | Nombre | Qué significa | Dónde aparece |
|---|---|---|---|---|
| 45 | `ICO_MANO` | Caricia | La acariciaste; también: quiere mimos | Pensamiento; globo; gustos (parte del cuerpo); marca donde la llamaste |
| 46 | `ICO_MANO_NO` | Toque que no le gusta | La tocaste donde no le gusta | Pensamiento |
| 47 | `ICO_PERSONA` | Tú | Te extraña / volviste | Pensamiento |
| 48 | `ICO_SACUDIDA` | Sacudida | Sacudiste el orbe | Pensamiento |
| 49 | `ICO_TERMOMETRO` | Enferma | Está enferma; también: salud | Pensamiento; globo; panel Más |
| 50 | `ICO_DESPERTADOR` | La despertaste | La despertaste antes de tiempo | Pensamiento |
| 51 | `ICO_ALERTA` | Lugar del susto | Aquí se asustó una vez | Pensamiento |
| 52 | `ICO_PUNTOS` | Nada que hacer | No pasa nada nuevo | Pensamiento |
| 53 | `ICO_LUPA` | Algo nuevo | Algo nuevo que explorar; también: curiosidad | Pensamiento; panel Más; amigos (buscando) |
| 54 | `ICO_GOTA` | Sucia | Está sucia (barro) | Pensamiento; reacción al no querer bañarse |
| 55 | `ICO_BURBUJA` | Baño | Por el baño | Pensamiento; burbujas al bañarla |
| 56 | `ICO_ESTRELLA` | Logro | Creció, evolucionó o logró algo | Pensamiento; efecto al crecer |

## Efectos

| # | Ícono | Nombre | Qué significa | Dónde aparece |
|---|---|---|---|---|
| 57 | `ICO_CORAZON` | Corazón | Le gustó / cariño / amor | Efecto flotante; gustos (lo que le encanta) |
| 58 | `ICO_NOTA` | Nota musical | Está jugando o contenta | Efecto flotante; amigos (invitando) |
| 59 | `ICO_ENOJO` | Marca de enojo | Se enojó | Efecto flotante; globo |
| 60 | `ICO_SUDOR` | Gotita de nervios | Se puso nerviosa o se asustó | Efecto flotante |
| 61 | `ICO_LAGRIMA` | Lágrima | Llora | Efecto flotante |
| 62 | `ICO_ZETA` | Zzz | Está durmiendo | Efecto flotante |
| 63 | `ICO_PREGUNTA` | Signo de pregunta | Olfatea o algo le da curiosidad | Efecto flotante |
| 64 | `ICO_CACA` | Caca | El prado está sucio: hay que bañarla | Prado |
| 65 | `ICO_HUEVO` | Huevo | Huevo / mascota nueva | Globo del huevo; ajustes |
| 150 | `ICO_MAREO` | Mareo | La mareaste con muchos toques seguidos | Efecto flotante |
| 151 | `ICO_ABRAZO` | Abrazo | Un abrazo (mantener el dedo sobre ella) | Efecto flotante; gustos |

## Animales

| # | Ícono | Nombre | Qué significa | Dónde aparece |
|---|---|---|---|---|
| 66 | `ICO_ARDILLA_B` | Ardilla (cuadro 2) | Segundo cuadro de la animación | Mundo |
| 67 | `ICO_CONEJO_B` | Conejo (cuadro 2) | Segundo cuadro de la animación | Mundo |
| 68 | `ICO_ZORRILLO_B` | Zorrillo (cuadro 2) | Segundo cuadro de la animación | Mundo |
| 69 | `ICO_MAPACHE_B` | Mapache (cuadro 2) | Segundo cuadro de la animación | Mundo |
| 70 | `ICO_NUTRIA_B` | Nutria (cuadro 2) | Segundo cuadro de la animación | Mundo |
| 71 | `ICO_CASTOR_B` | Castor (cuadro 2) | Segundo cuadro de la animación | Mundo |
| 72 | `ICO_RATON_B` | Raton (cuadro 2) | Segundo cuadro de la animación | Mundo |
| 73 | `ICO_OSO_B` | Oso (cuadro 2) | Segundo cuadro de la animación | Mundo |
| 74 | `ICO_CIERVO_CAFE_B` | Ciervo_Cafe (cuadro 2) | Segundo cuadro de la animación | Mundo |
| 75 | `ICO_TEJON_B` | Tejon (cuadro 2) | Segundo cuadro de la animación | Mundo |
| 76 | `ICO_GATO_MONTES_B` | Gato_Montes (cuadro 2) | Segundo cuadro de la animación | Mundo |
| 77 | `ICO_LINCE_B` | Lince (cuadro 2) | Segundo cuadro de la animación | Mundo |
| 78 | `ICO_PUMA_B` | Puma (cuadro 2) | Segundo cuadro de la animación | Mundo |
| 79 | `ICO_ERIZO_B` | Erizo (cuadro 2) | Segundo cuadro de la animación | Mundo |
| 80 | `ICO_TOPO_B` | Topo (cuadro 2) | Segundo cuadro de la animación | Mundo |
| 81 | `ICO_LAGARTIJA_B` | Lagartija (cuadro 2) | Segundo cuadro de la animación | Mundo |
| 82 | `ICO_SALAMANDRA_B` | Salamandra (cuadro 2) | Segundo cuadro de la animación | Mundo |
| 83 | `ICO_CAMALEON_B` | Camaleon (cuadro 2) | Segundo cuadro de la animación | Mundo |
| 84 | `ICO_CULEBRA_B` | Culebra (cuadro 2) | Segundo cuadro de la animación | Mundo |
| 85 | `ICO_TORTUGA_B` | Tortuga (cuadro 2) | Segundo cuadro de la animación | Mundo |
| 86 | `ICO_CARACOL_B` | Caracol (cuadro 2) | Segundo cuadro de la animación | Mundo |
| 87 | `ICO_COLIBRI_B` | Colibri (cuadro 2) | Segundo cuadro de la animación | Mundo |
| 88 | `ICO_CARPINTERO_B` | Carpintero (cuadro 2) | Segundo cuadro de la animación | Mundo |
| 89 | `ICO_MARTIN_B` | Martin (cuadro 2) | Segundo cuadro de la animación | Mundo |
| 90 | `ICO_GOLONDRINA_B` | Golondrina (cuadro 2) | Segundo cuadro de la animación | Mundo |
| 91 | `ICO_URRACA_B` | Urraca (cuadro 2) | Segundo cuadro de la animación | Mundo |
| 92 | `ICO_CUERVO_B` | Cuervo (cuadro 2) | Segundo cuadro de la animación | Mundo |
| 93 | `ICO_PATO_B` | Pato (cuadro 2) | Segundo cuadro de la animación | Mundo |
| 94 | `ICO_CISNE_B` | Cisne (cuadro 2) | Segundo cuadro de la animación | Mundo |
| 95 | `ICO_GARZA_B` | Garza (cuadro 2) | Segundo cuadro de la animación | Mundo |
| 96 | `ICO_BUHO_B` | Buho (cuadro 2) | Segundo cuadro de la animación | Mundo |
| 97 | `ICO_MURCIELAGO_B` | Murcielago (cuadro 2) | Segundo cuadro de la animación | Mundo |
| 98 | `ICO_MARIQUITA_B` | Mariquita (cuadro 2) | Segundo cuadro de la animación | Mundo |
| 99 | `ICO_LIBELULA_B` | Libelula (cuadro 2) | Segundo cuadro de la animación | Mundo |
| 100 | `ICO_GRILLO_B` | Grillo (cuadro 2) | Segundo cuadro de la animación | Mundo |
| 101 | `ICO_HORMIGAS_B` | Hormigas (cuadro 2) | Segundo cuadro de la animación | Mundo |
| 102 | `ICO_ESCARABAJO_B` | Escarabajo (cuadro 2) | Segundo cuadro de la animación | Mundo |
| 103 | `ICO_POLILLA_B` | Polilla (cuadro 2) | Segundo cuadro de la animación | Mundo |
| 121 | `ICO_MARIPOSA` | Mariposa | Una mariposa: le encanta perseguirlas; vienen más si hay flores | Mundo; pensamiento |
| 122 | `ICO_MARIPOSA_B` | Mariposa (aleteo) | Segundo cuadro del aleteo | Mundo |
| 123 | `ICO_ABEJA` | Abeja | Una abeja: vienen a las flores; si la persigue puede picarla | Mundo; pensamiento |
| 124 | `ICO_ABEJA_B` | Abeja (aleteo) | Segundo cuadro del aleteo | Mundo |
| 125 | `ICO_PAJARO` | Pájaro | El pájaro amigo: si lo dejas tranquilo a veces trae regalos | Mundo; pensamiento |
| 126 | `ICO_PAJARO_B` | Pájaro (aleteo) | Segundo cuadro del aleteo | Mundo |
| 127 | `ICO_ZORRO` | Zorro | Un zorro travieso: viene por la fruta y la miel; tócalo para espantarlo | Mundo; pensamiento |
| 128 | `ICO_RANA` | Rana | Una rana del lago | Mundo (lago); pensamiento |

## Jardín, charcos y hojas

| # | Ícono | Nombre | Qué significa | Dónde aparece |
|---|---|---|---|---|
| 129 | `ICO_BROTE` | Brote | Una semilla recién plantada que empieza a crecer | Jardín del prado |
| 130 | `ICO_FLOR_MUSTIA` | Flor mustia | Una planta con sed: hay que regarla (no se muere) | Jardín del prado; pensamiento |
| 131 | `ICO_ARBOLITO` | Arbolito | El árbol que creció de una semilla que ella enterró | Jardín del prado |
| 132 | `ICO_ARBOL_FRUTAL` | Árbol con frutos | Su árbol ya da fruta | Jardín del prado |
| 133 | `ICO_REGADERA` | Regar | Regar una planta; también: el jardín | Jardín; pensamiento |
| 134 | `ICO_HOJAS` | Montón de hojas | Hojas de otoño: tócalas y saltará encima | Prado en otoño; pensamiento |
| 135 | `ICO_CHARCO` | Charco | Charcos después de la lluvia: le encanta salpicar | Prado; pensamiento |
| 210 | `ICO_GIRASOL` | Girasol | Flor grande del jardín | Jardín; elegir semilla |
| 211 | `ICO_FRUTILLA` | Frutilla | Planta que da frutas | Jardín; elegir semilla |
| 217 | `ICO_PLANTA_SECA` | Planta seca | Pasó mucho sin agua: se secó (regándola todavía revive) | Jardines |

## Zonas del mundo

| # | Ícono | Nombre | Qué significa | Dónde aparece |
|---|---|---|---|---|
| 136 | `ICO_ZONA_PRADO` | Prado | Su casa: el prado con su rincón y su jardín | Flechas de zona; pensamiento |
| 137 | `ICO_ZONA_BOSQUE` | Bosque | El bosque: árboles, hongos y un arroyo | Flechas de zona; pensamiento |
| 138 | `ICO_ZONA_LAGO` | Lago | El lago: ranas, un muelle y un sauce | Flechas de zona; pensamiento |
| 139 | `ICO_ZONA_COLINA` | Colina | La colina: el árbol solitario y el mirador | Flechas de zona; pensamiento |
| 140 | `ICO_ZONA_CUEVA` | Cueva | La cueva secreta con cristales: un refugio | Flechas de zona; pensamiento |
| 141 | `ICO_FLECHA` | Ir a otra zona | Flecha para cambiar de zona (se espeja para la izquierda) | Bordes de la pantalla |
| 231 | `ICO_ZONA_ISLA` | Isla | La isla del lago: un árbol viejo, un círculo de piedras y un faro | Mapa; diario; pensamiento |

## Fiesta (tu cumpleaños)

| # | Ícono | Nombre | Qué significa | Dónde aparece |
|---|---|---|---|---|
| 142 | `ICO_PASTEL` | Pastel de cumpleaños | Tu cumpleaños: tócalo para cantar | Mundo, el día de tu cumpleaños; globo |
| 143 | `ICO_REGALO` | Regalo | Un regalo (de su amigo o por tu cumpleaños) | Globo; efecto flotante |
| 144 | `ICO_CALENDARIO` | Fecha de nacimiento | Tu fecha de nacimiento (calcula tu edad y tu cumpleaños) | Bienvenida; ajustes |
| 250 | `ICO_PASTEL_VELAS` | Pastel con velas | El pastel de tu cumpleaños, con las velas sin llama (las llamas se dibujan encima) | Tu cumpleaños |
| 297 | `ICO_EMPANADA` | Empanada | Las empanadas de las Fiestas Patrias | Fiestas Patrias (Chile) |
| 298 | `ICO_VOLANTIN` | Volantín | Volantín con los colores de Chile | Fiestas Patrias (Chile) |
| 299 | `ICO_GORRO_BRUJA` | Sombrero de bruja | Disfraz de Halloween | Halloween con todos |
| 300 | `ICO_FANTASMA` | Fantasmita | Disfraz de Halloween | Halloween con todos |
| 301 | `ICO_GUIRNALDA` | Guirnalda | Guirnalda navideña con lucecitas | Navidad con todos |
| 302 | `ICO_RAMO_AMARILLO` | Ramo de flores amarillas | El ramo que te trae el Día de las Flores Amarillas | 21 de septiembre (sur) o 21 de marzo (norte de Latinoamérica) |

## El lago (pesca)

| # | Ícono | Nombre | Qué significa | Dónde aparece |
|---|---|---|---|---|
| 145 | `ICO_BOTE` | Bote | El bote del lago: tócalo y sale a pescar | Lago |
| 146 | `ICO_PEZ` | Pez | Pescó un pez (se lo come o lo devuelve al agua) | Efecto flotante; pensamiento |
| 147 | `ICO_BOTA` | Bota vieja | Pescó una bota vieja (le da risa) | Efecto flotante |
| 148 | `ICO_CANA` | Caña de pescar | Está pescando; también: la pesca | Pensamiento; panel Más |
| 236 | `ICO_ALGA` | Alga | Pescó un alga: le da risa | Al pescar; diario |
| 237 | `ICO_CANGREJO` | Cangrejito | Pescó un cangrejito: le pellizca la nariz y lo devuelve al agua | Al pescar; diario |
| 238 | `ICO_NENUFAR` | Hoja de nenúfar | Pescó una hoja de nenúfar: se la pone de sombrero | Al pescar; en su cabeza |
| 239 | `ICO_PEZ_DORADO` | Pez dorado | Muy raro: un pez dorado que brilla; lo devuelve y queda en el diario | Al pescar; diario |

## Juegos y trucos

| # | Ícono | Nombre | Qué significa | Dónde aparece |
|---|---|---|---|---|
| 104 | `ICO_CRIS_0` | Cristal 1 | Uno de los seis cristales del juego de la cueva | Cristales |
| 105 | `ICO_CRIS_1` | Cristal 2 | Uno de los seis cristales del juego de la cueva | Cristales |
| 106 | `ICO_CRIS_2` | Cristal 3 | Uno de los seis cristales del juego de la cueva | Cristales |
| 107 | `ICO_CRIS_3` | Cristal 4 | Uno de los seis cristales del juego de la cueva | Cristales |
| 108 | `ICO_CRIS_4` | Cristal 5 | Uno de los seis cristales del juego de la cueva | Cristales |
| 109 | `ICO_CRIS_5` | Cristal 6 | Uno de los seis cristales del juego de la cueva | Cristales |
| 152 | `ICO_CANASTA` | Canasta | Atrapar frutas: la mueves con el dedo (o inclinando el orbe) | Menú Jugar; minijuego |
| 153 | `ICO_ESCONDIDAS` | Escondidas | Jugar a las escondidas: se esconde y la buscas | Menú Jugar |
| 154 | `ICO_PATITA` | Trucos | Enseñarle trucos | Menú Jugar |
| 155 | `ICO_SENTAR` | Sentarse | (Ya no se usa: era el truco de sentarse; ahora es hacerse la dormida, con la Zzz) | - |
| 156 | `ICO_VUELTA` | Dar una vuelta | El truco de darse vuelta como trompo | Panel Trucos |
| 157 | `ICO_SALUDO` | Saludar | El truco de saludar | Panel Trucos |
| 178 | `ICO_FRUTA_MANZANA` | Manzana | Cae en el juego de atrapar frutas | Juego de las frutas |
| 179 | `ICO_FRUTA_NARANJA` | Naranja | Cae en el juego de atrapar frutas | Juego de las frutas |
| 180 | `ICO_FRUTA_BANANA` | Banana | Cae en el juego de atrapar frutas | Juego de las frutas |
| 181 | `ICO_FRUTA_UVAS` | Uvas | Cae en el juego de atrapar frutas | Juego de las frutas |
| 182 | `ICO_FRUTA_CEREZAS` | Cerezas | Cae en el juego de atrapar frutas | Juego de las frutas |
| 183 | `ICO_FRUTA_PERA` | Pera | Cae en el juego de atrapar frutas | Juego de las frutas |
| 184 | `ICO_FRUTA_SANDIA` | Sandía | Cae en el juego de atrapar frutas | Juego de las frutas |

## Recuerdos y metas

| # | Ícono | Nombre | Qué significa | Dónde aparece |
|---|---|---|---|---|
| 158 | `ICO_LIBRO` | Diario | Su diario: lo importante de cada día, el álbum y sus colecciones | Panel Más |
| 159 | `ICO_FOTO` | Foto | Se guardó una foto en el álbum; también: el álbum | Diario; efecto al sacar una foto |
| 160 | `ICO_COFRE` | Colección | Sus colecciones de tesoros | Diario |
| 161 | `ICO_META` | Meta del día | La meta suave de hoy (sin castigo si no se cumple) | Diario; globo de la mañana |
| 162 | `ICO_ESTAMPITA` | Estampita | Premio por cumplir una meta | Diario; efecto |
| 163 | `ICO_GORRO` | Gorro de fiesta | Es su cumpleaños (lo lleva puesto todo el día) | Sobre su cabeza; álbum |
| 164 | `ICO_NARIZ` | ¡Huele algo! | Olió un tesoro escondido y va a cavar | Efecto; pensamiento |
| 190 | `ICO_NADAR` | Nadar | Está aprendiendo a nadar | Pensamiento; diario |
| 191 | `ICO_TREPAR` | Trepar | Está aprendiendo a trepar árboles | Pensamiento; diario |
| 232 | `ICO_CARTA` | Carta | Una carta de un amigo del barrio, con su sello de cera | Al llegar una carta; Recuerdos → Cartas |

## Tesoros y colecciones

| # | Ícono | Nombre | Qué significa | Dónde aparece |
|---|---|---|---|---|
| 165 | `ICO_CONCHA` | Concha | Colección de conchas del lago | Colección; hallazgo |
| 166 | `ICO_PLUMA` | Pluma | Colección de plumas (del bosque y del pájaro) | Colección; hallazgo |
| 167 | `ICO_HOJA` | Hoja | Colección de hojas: una por estación | Colección; hallazgo |
| 168 | `ICO_CRISTAL` | Cristal | Colección de cristales de la cueva | Colección; hallazgo |
| 169 | `ICO_CHAPITA` | Chapita | Una chapita de botella: "basura" que te trae con cariño | Regalo; hallazgo |
| 170 | `ICO_HOJA_SECA` | Hoja seca | Una hoja seca: otro regalo "basura" | Regalo; hallazgo |

## Cosas raras

| # | Ícono | Nombre | Qué significa | Dónde aparece |
|---|---|---|---|---|
| 171 | `ICO_ARCOIRIS` | Arcoíris | Salió un arcoíris (a veces después de la lluvia) | Mundo; álbum; pensamiento |
| 172 | `ICO_ESTRELLA_FUGAZ` | Estrella fugaz | Pasó una estrella fugaz: ¡pide un deseo! | Mundo de noche; álbum |
| 173 | `ICO_CIERVO` | Ciervo blanco | Un ciervo blanco que casi nadie ve | Mundo al amanecer o atardecer; álbum |
| 174 | `ICO_CIERVO_B` | Ciervo blanco (paso 1) | Cuadro de su caminata | Mundo |
| 175 | `ICO_CIERVO_C` | Ciervo blanco (paso 2) | Cuadro de su caminata | Mundo |
| 176 | `ICO_CIERVO_D` | Ciervo blanco (cabeza baja) | Cuadro de su caminata | Mundo |
| 185 | `ICO_GLOBO_AIRE` | Ballena de nubes | Una ballena hecha de nubes nada despacio por el cielo | Mundo; álbum |

## Fechas especiales

| # | Ícono | Nombre | Qué significa | Dónde aparece |
|---|---|---|---|---|
| 119 | `ICO_FAROL_PAPEL` | Farolito de papel | Decoración de Año Nuevo en el prado | Nochevieja |
| 120 | `ICO_GLOBO_FIESTA` | Globo de fiesta | Decoración de Año Nuevo en el prado | Nochevieja |
| 186 | `ICO_CALABAZA` | Calabaza | Decoración Halloween (de fábrica el 31 de octubre; el editor puede usarla en otras fiestas) | Mundo ese día; álbum |
| 187 | `ICO_ARBOL_NAVIDAD` | Árbol de Navidad | Decoración Navidad (de fábrica 24 y 25 de diciembre) | Mundo esos días; álbum |
| 188 | `ICO_FUEGOS` | Fuegos artificiales | Decoración Año Nuevo (de fábrica 31/12 y 1/1) | Mundo de noche; álbum |
| 189 | `ICO_BANDERINES` | Banderines | Decoración Fiestas (de fábrica Fiestas Patrias, 18 y 19 de septiembre; la fecha se cambia en el editor) | Arriba del mundo esos días; álbum |

## Ajustes, hora y respaldo

| # | Ícono | Nombre | Qué significa | Dónde aparece |
|---|---|---|---|---|
| 192 | `ICO_RELOJ` | Reloj | La hora de hoy (se pregunta al empezar o si el orbe la perdió) | Bienvenida; ajustes |
| 193 | `ICO_DADO` | Nombre al azar | Elige un nombre sugerido al azar | Elegir nombre |
| 194 | `ICO_RESPALDO` | Respaldo | Guardar la mascota en el computador o traerla de otro orbe | Ajustes; traer una mascota |
| 212 | `ICO_CAMARA` | Cámara | Sacarle una foto (se guarda en el orbe) | Libro: pestaña fotos |
| 213 | `ICO_PECERA` | Modo pecera | Verla vivir sin botones, como una pecera | Más |
| 214 | `ICO_PARLANTE` | Sonido ambiente | Lluvia, grillos, viento... de fondo | Ajustes |
| 215 | `ICO_PALETA` | Colores | Elegir sus colores | Elegir cómo será |
| 216 | `ICO_MEZCLA` | Mezclar | Mezclar dos especies | Elegir cómo será |
| 219 | `ICO_CANDADO` | Por descubrir | Una zona que todavía no encuentra | Mapa |
| 220 | `ICO_MALETA` | De viaje | Despedir a una mascota: se va de viaje a explorar el mundo | Mis mascotas |

## Ropa y accesorios

| # | Ícono | Nombre | Qué significa | Dónde aparece |
|---|---|---|---|---|
| 177 | `ICO_GORRO_DORMIR` | Gorro de dormir | Un gorrito azul con borla para dormir en su camita | Casita |
| 195 | `ICO_PERCHA` | Ropero | Su ropa y accesorios | Más; el ropero |
| 196 | `ICO_MONO` | Moño de pétalos | Un moño de pétalos que suelta brillitos | Ropero; puesto en la mascota |
| 197 | `ICO_PANUELO` | Pañuelo de hojas | Pañuelo de hojas: cambia de color con la estación | Ropero; puesto en la mascota |
| 198 | `ICO_GORRO_LANA` | Gorro de estrellas | Gorro de lana con estrellitas que titilan: abriga | Ropero; puesto en la mascota |
| 199 | `ICO_BUFANDA` | Bufanda de seda | Tejida con seda de araña tejedora: abriga y brilla | Ropero; puesto en la mascota |
| 200 | `ICO_SOMBRERO` | Sombrero con flor | Sombrero de paja con una flor que se abre al sol: da sombra | Ropero; puesto en la mascota |
| 201 | `ICO_LENTES` | Lentes de luna | Lentes de cristal de luna | Ropero; puesto en la mascota |
| 202 | `ICO_CORONA` | Corona de flores | Flores que brillan de noche; aparece cuando florece su jardín | Ropero; puesto en la mascota |
| 203 | `ICO_COLLAR` | Collar con perla | Conchas con una perla luminosa; aparece al completar las conchas | Ropero; puesto en la mascota |
| 322 | `ICO_PARAGUAS` | Paraguas de hoja | Un paraguas de hoja: lo sostiene abierto sobre su cabeza | Ropero; en su mano |
| 323 | `ICO_MOLINILLO` | Molinillo de viento | Un molinillo chiquito que gira con el viento | Ropero; en su mano |

## Juguetes

| # | Ícono | Nombre | Qué significa | Dónde aparece |
|---|---|---|---|---|
| 204 | `ICO_PELUCHE` | Peluche | Juguete: lo abraza (calma) | Rincón; en el prado |
| 205 | `ICO_PELOTON` | Pelotón | Juguete: una pelota grande para empujar | Rincón; en el prado |
| 206 | `ICO_CAJA` | Caja de cartón | Juguete: se mete adentro y se asoma | Rincón; en el prado |
| 207 | `ICO_COLUMPIO` | Columpio | Juguete: se columpia | Rincón; en el prado |
| 208 | `ICO_TAMBOR` | Tambor | Juguete: lo toca | Rincón; en el prado |
| 209 | `ICO_MOLINETE` | Molinete | Juguete: gira con el viento | Rincón; en el prado |

## Objetos mágicos: tesoros de la tierra

| # | Ícono | Nombre | Qué significa | Dónde aparece |
|---|---|---|---|---|
| 336 | `ICO_OBJ_001` | Piedra brillante | Guarda luz de luna. El Cuidador la usaba para contar estrellas cuando se le perdía la cuenta. | La Bolsa |
| 337 | `ICO_OBJ_002` | Geoda olvidada | Común por fuera. Cristalino dice que adentro guarda una nota en mi bemol. Apriétala: se abre. | La Bolsa |
| 338 | `ICO_OBJ_003` | Piedra pómez astral | Tan liviana que flota. Anemoi la usa para practicar vuelos sin caerse (casi nunca). | La Bolsa |
| 339 | `ICO_OBJ_004` | Pirita flotante | Oro de mentira, cariño de verdad: una luciérnaga dorada siempre vuelve a ella. | La Bolsa |
| 340 | `ICO_OBJ_005` | Cuarzo ahumado | Se traga la neblina. Lumbre dice que así el Cuidador veía el camino a casa. | La Bolsa |
| 341 | `ICO_OBJ_006` | Lágrima de lago | Una lágrima de cuando Ondina era nube. Dice que ya no le duele: se volvió piedra bonita. | La Bolsa |
| 342 | `ICO_OBJ_007` | Piedra rúnica base | Una roca con una sola línea. Tilo marcaba así cada lugar que ya había cuidado ese día. | La Bolsa |
| 343 | `ICO_OBJ_008` | Ámbar antiguo | Guarda un mosquito de hace mil años y el olor del bosque cuando todavía era prado de piedras. | La Bolsa |
| 344 | `ICO_OBJ_009` | Fragmento de meteorito | Todavía tibio. Lo trajo la cometa del Cuidador de muy, muy lejos. | La Bolsa |
| 345 | `ICO_OBJ_010` | Lodo magnético | Barro que se mueve solo. Musguito dice que así era el prado antes de la primera semilla. | La Bolsa |
| 346 | `ICO_OBJ_011` | Guijarro cantarín | Suena una notita cuando ella pasa. Cristalino lo afinó para que suene en sol. | La Bolsa |
| 347 | `ICO_OBJ_012` | Ágata de fuego | Frotada se calienta. Marea la llevaba en el bote las noches frías del cruce. | La Bolsa |
| 348 | `ICO_OBJ_013` | Jade verde vivo | Del bosque más viejo: quita el cansancio. Ojalá alguien se la hubiera dado al Cuidador. | La Bolsa |
| 349 | `ICO_OBJ_014` | Obsidiana espejo | Una piedra negra que refleja el cielo de ahora. El Cuidador se peinaba en ella. No servía. | La Bolsa |
| 350 | `ICO_OBJ_015` | Mica reluciente | Se deshace en purpurina. Las hadas la usan para sus fiestas de media luna. | La Bolsa |
| 351 | `ICO_OBJ_016` | Piedra de trueno | La dejó un rayo. Las tormentas también cantan: cada trueno es una nota. | La Bolsa |
| 352 | `ICO_OBJ_017` | Jaspe rojo | Late despacito, como el corazón de quien te quiere. Dicen que al Cuidador le latía igual. | La Bolsa |
| 353 | `ICO_OBJ_018` | Mármol de hada | Una bolita perfecta. Las hadas juegan a los bolos con ellas; el Cuidador siempre perdía. | La Bolsa |
| 354 | `ICO_OBJ_019` | Carbón fatuo | Nunca se apaga. El cariño es luz, y este carbón no se olvida de quien lo encendió. | La Bolsa |
| 355 | `ICO_OBJ_020` | Escama de Prisma | Una escama de Prisma, el dragón de cristal. Vibra cuando hay algo raro cerca. | La Bolsa |
| 356 | `ICO_OBJ_021` | Roca de musgo seco | Absorbe la lluvia. Ondina se enoja: dice que la lluvia también es bonita. | La Bolsa |
| 357 | `ICO_OBJ_022` | Pedernal de chispas azules | Frotado saca chispas cian. Anemoi lo usa para encender estrellas chiquitas. | La Bolsa |
| 358 | `ICO_OBJ_023` | Talco de estrellas | Polvo de estrellas que trae Nimba de sus paseos por el firmamento. | La Bolsa |
| 359 | `ICO_OBJ_024` | Gema de cueva profunda | Brilla más cuanto más oscuro está, como las luciérnagas de Lumbre. | La Bolsa |
| 360 | `ICO_OBJ_025` | Fósil de helecho | Una hoja en la piedra, de cuando todo empezó chiquito. | La Bolsa |
| 361 | `ICO_OBJ_026` | Piedra de sal rosada | Espanta lo molesto. Musguito la pone en la puerta cuando quiere estar tranquilo. | La Bolsa |
| 362 | `ICO_OBJ_027` | Grava movediza | Cuando no la miras, cambia de lugar. Tilde jura que ella no fue. | La Bolsa |
| 363 | `ICO_OBJ_028` | Corazón de montaña | Pesadísimo. Dicen que es un pedacito del Señor Don Gigante de la colina. | La Bolsa |
| 364 | `ICO_OBJ_029` | Estalactita diminuta | Una gotita de piedra helada. ¡Ojo con ponerla en la frente de un dragón! | La Bolsa |
| 365 | `ICO_OBJ_030` | Ojo de tigre | Deja ver lo que les falta a las plantas, como miraba el Cuidador: con calma. | La Bolsa |

## Objetos mágicos: flora mística

| # | Ícono | Nombre | Qué significa | Dónde aparece |
|---|---|---|---|---|
| 366 | `ICO_OBJ_031` | Semilla ancestral | Hermana de la primera semilla que plantaron Musguito y el Cuidador. | La Bolsa |
| 367 | `ICO_OBJ_032` | Hongo luminoso | Brilla rosa en la oscuridad, como los hongos del anillo de las hadas. | La Bolsa |
| 368 | `ICO_OBJ_033` | Diente de león gigante | Sóplalo: lo que se suelta con amor vuelve distinto. Sus semillas se llevan la neblina. | La Bolsa |
| 369 | `ICO_OBJ_034` | Baya de luna | Plateada, brilla de noche. Lumbre dice que es la favorita de la luna. | La Bolsa |
| 370 | `ICO_OBJ_035` | Raíz caminante | Da pasitos dentro de la bolsa. Quiere volver al bosque, pero no sabe por dónde. | La Bolsa |
| 371 | `ICO_OBJ_036` | Loto estelar | Solo florece sobre el lago, donde el Cuidador dibujó estrellas con el dedo. | La Bolsa |
| 372 | `ICO_OBJ_037` | Trébol de cuatro colores | Cuatro hojas de cuatro colores. Tilde lleva años buscando uno. No le digas que lo tienes. | La Bolsa |
| 373 | `ICO_OBJ_038` | Musgo de terciopelo | Una alfombrita suave. Así era la camita de musgo de la casita escondida. | La Bolsa |
| 374 | `ICO_OBJ_039` | Galleta quemada de Tilo | Negra como el carbón. «Así venían», decía el Cuidador. Tiene cariño adentro, y humo. | La Bolsa |
| 375 | `ICO_OBJ_040` | Hojas de arce dorado | Tintinean al caer. El Cuidador las juntaba para hacer coronas. | La Bolsa |
| 376 | `ICO_OBJ_041` | Orquídea susurrante | Susurra la música del bosque al revés. Cristalino dice que así suena un verso sin terminar. | La Bolsa |
| 377 | `ICO_OBJ_042` | Savia de árbol sagrado | Pegamento del árbol viejo: arregla lo roto. El árbol del hueco la regala a quien cuida. | La Bolsa |
| 378 | `ICO_OBJ_043` | Semilla helicóptero | Cae girando, como las cometas que bajan a descansar. Siempre aterriza en el centro. | La Bolsa |
| 379 | `ICO_OBJ_044` | Hongo sombrero de bruja | Morado y puntiagudo. Las hadas dicen que al Cuidador le quedaba enorme. | La Bolsa |
| 380 | `ICO_OBJ_045` | Helecho de sombras | Crece detrás de los árboles grandes, como el sendero que se mueve. | La Bolsa |
| 381 | `ICO_OBJ_046` | Baya invernal | Al comerla estornuda copitos. Marea dice que en la isla nieva así. | La Bolsa |
| 382 | `ICO_OBJ_047` | Corteza de hierro | Madera durísima. El zorro dice que ya no roba, pero por si acaso. | La Bolsa |
| 383 | `ICO_OBJ_048` | Flor de niebla | Suelta vapor blanco. Ondina la usa cuando quiere llorar sin que nadie la vea. | La Bolsa |
| 384 | `ICO_OBJ_049` | Enredadera elástica | Se estira y rebota. Anemoi dice que así se aprende a volar: rebotando. | La Bolsa |
| 385 | `ICO_OBJ_050` | Rosa de cristal | Frágil y transparente. Cristalino la afinó en do: suena como campanita. | La Bolsa |
| 386 | `ICO_OBJ_051` | Semilla de calabaza linterna | Crece una calabaza que se ilumina sola, para que nadie camine a oscuras. | La Bolsa |
| 387 | `ICO_OBJ_052` | Bulbo parpadeante | Se prende y se apaga como el faro de Marea, en chiquitito. | La Bolsa |
| 388 | `ICO_OBJ_053` | Hierba gatera del bosque | La pone mimosa. Tilde la usa para que la perdonen. | La Bolsa |
| 389 | `ICO_OBJ_054` | Brote de bambú veloz | Crece un poquito cada vez que lo miras. Todo empieza chiquito. | La Bolsa |
| 390 | `ICO_OBJ_055` | Pétalo de arcoíris | Del primer arcoíris, cuando el sol y la lluvia hicieron las paces. | La Bolsa |
| 391 | `ICO_OBJ_056` | Cardo de espinas azules | Cuida el jardín con sus espinas. Musguito le tiene un poquito de miedo. | La Bolsa |
| 392 | `ICO_OBJ_057` | Nuez de roble gigante | Tan dura que sirve de pelota. El Cuidador la usaba para jugar con el conejo, y perdía. | La Bolsa |
| 393 | `ICO_OBJ_058` | Flor del sueño | Su olor da tanto sueño que hasta el pino que ronca se duerme más rápido. | La Bolsa |
| 394 | `ICO_OBJ_059` | Liquen eléctrico | Parpadea con descargas cian. Las tormentas que cantan le enseñaron. | La Bolsa |
| 395 | `ICO_OBJ_060` | Fruta del tiempo | La deja hiperactiva. Dicen que el Cuidador nunca la probó: ya corría bastante. | La Bolsa |

## Objetos mágicos: restos de animales y espíritus

| # | Ícono | Nombre | Qué significa | Dónde aparece |
|---|---|---|---|---|
| 396 | `ICO_OBJ_061` | Pluma de arrendajo | Azul brillante. Cae en zigzag, como Anemoi cuando todavía no sabía volar. | La Bolsa |
| 397 | `ICO_OBJ_062` | Escama de pez astral | Destella como las estrellas del lago. El pez dorado la reconoce. | La Bolsa |
| 398 | `ICO_OBJ_063` | Huevo misterioso | Late despacito. Algo tibio y redondo… ¿será como el que vio la Abuela Brizna? | La Bolsa |
| 399 | `ICO_OBJ_064` | Fuego fatuo embotellado | Al destapar el frasco sale un fuego fatuo. Lumbre le enseñó a seguir a quien se pierde. | La Bolsa |
| 400 | `ICO_OBJ_065` | Concha que repite | Si la pones en tu oreja, repite con una vocecita: «ya va a llegar». Marea la cuidaba. | La Bolsa |
| 401 | `ICO_OBJ_066` | Pluma de búho sabio | Del búho sabio: ayuda a leer lo que está escrito en los árboles viejos. | La Bolsa |
| 402 | `ICO_OBJ_067` | Colmillo de zorro guía | El zorro la dejó de regalo el día que dejó de robar. Trae suerte. | La Bolsa |
| 403 | `ICO_OBJ_068` | Seda de araña tejedora | Hilo que brilla. Si todavía no tiene bufanda, alguien le teje una con él. | La Bolsa |
| 404 | `ICO_OBJ_069` | Asta de ciervo de luz | Del ciervo blanco, que mira la cima de la colina esperando a alguien. Brilla como faro. | La Bolsa |
| 405 | `ICO_OBJ_070` | Escama de serpiente esmeralda | De la culebrita del lago. Repele el barro, como los nenúfares de las ranas. | La Bolsa |
| 406 | `ICO_OBJ_071` | Huella dorada | Flota en el aire y señala tesoros. Dicen que es una huella del Cuidador. Chiquita. | La Bolsa |
| 407 | `ICO_OBJ_072` | Lágrima de espíritu | Una lágrima que lloraron dos, juntos. Por eso revive lo que se secó. | La Bolsa |
| 408 | `ICO_OBJ_073` | Garra de oso guardián | Del oso dormilón. Con ella da un rugidito que espanta a cualquiera. | La Bolsa |
| 409 | `ICO_OBJ_074` | Capullo de mariposa prisma | Cambia de color según cómo lo mires, como el Cuidador según quién lo recuerde. | La Bolsa |
| 410 | `ICO_OBJ_075` | Pluma de fénix del bosque | Tibia y naranja. El fénix dice que el sol se la prestó al Cuidador una vez. | La Bolsa |
| 411 | `ICO_OBJ_076` | Trocito del caparazón de Brizna | Se le cayó a la Abuela Brizna al bostezar. Protege como abuela. | La Bolsa |
| 412 | `ICO_OBJ_077` | Aguijón de avispa mística | Sirve para tallar runas. Así marcaba el Cuidador los lugares que ya había cuidado. | La Bolsa |
| 413 | `ICO_OBJ_078` | Hueso de dinosaurio pixel | Enorme. Tilde intentó llevárselo tres veces. Pesa demasiado. | La Bolsa |
| 414 | `ICO_OBJ_079` | Canto de sirena | Una burbuja con el sonido del agua. Marea la canta para que los barquitos no se pierdan. | La Bolsa |
| 415 | `ICO_OBJ_080` | Espora de hada voladora | Revolotea alrededor de su cabeza, como las hadas alrededor de la cuchara perdida. | La Bolsa |
| 416 | `ICO_OBJ_081` | Huevo de cuervo nocturno | De noche se abre. El cuervo guarda ahí las cosas elegantes que Tilde no encuentra. | La Bolsa |
| 417 | `ICO_OBJ_082` | Escama de dragón de agua | De un dragón de agua, primo lejano de Prisma y Anemoi. La vuelve nadadora experta. | La Bolsa |
| 418 | `ICO_OBJ_083` | Sombra errante | Una mancha con ojitos que imita todo. Dicen que era la sombra del Cuidador. Chiquita. | La Bolsa |
| 419 | `ICO_OBJ_084` | Cola de lagartija neón | Si se cae, vuelve a crecer. Lo que se suelta con amor vuelve distinto. | La Bolsa |
| 420 | `ICO_OBJ_085` | Pluma de cisne de plata | Con ella en la cabeza camina como bailarina. Las hadas la envidian. | La Bolsa |
| 421 | `ICO_OBJ_086` | Almeja perlada | Mantén el dedo y se abre: hay una perla. Ondina dice que la lloró ella. Exagera. | La Bolsa |
| 422 | `ICO_OBJ_087` | Pelo de lobo lunar | Hebras plateadas que brillan con la luna llena, la noche del rito. | La Bolsa |
| 423 | `ICO_OBJ_088` | Duende travieso | Un puntito verde hiperactivo. Primo de Musguito. Muy, muy primo. | La Bolsa |
| 424 | `ICO_OBJ_089` | Diente de tiburón de lago | Amuleto del pescador. El pez dorado no le tiene miedo: sabe que siempre lo sueltan. | La Bolsa |
| 425 | `ICO_OBJ_090` | Polvo de polilla lunar | Desliza el dedo y deja luz. Las polillas bailan con la luna, como el Cuidador con las hadas. | La Bolsa |

## Objetos mágicos: objetos perdidos y antigüedades

| # | Ícono | Nombre | Qué significa | Dónde aparece |
|---|---|---|---|---|
| 426 | `ICO_OBJ_091` | Bota vieja | Basura del lago. Frótala con cariño: limpia, las hadas la vuelven su casita en el prado. | La Bolsa |
| 427 | `ICO_OBJ_092` | Reloj de bolsillo parado | Se paró la noche del último paseo. Al tocarlo, todo se queda quietito. | La Bolsa |
| 428 | `ICO_OBJ_093` | Llave oxidada | Abre algo que todavía no encuentras. Quizás bajo el árbol más viejo, con su candado. | La Bolsa |
| 429 | `ICO_OBJ_094` | Brújula loca | La aguja siempre apunta hacia ella. Tilde dice que es porque ella brilla. | La Bolsa |
| 430 | `ICO_OBJ_095` | Tintero del escritor | La tinta del diario del Rey. Deja ver notas escondidas en su diario. | La Bolsa |
| 431 | `ICO_OBJ_096` | Moneda gastada | Del reino del Rey, que nunca cobró nada. Tiene grabada una galleta. Quemada. | La Bolsa |
| 432 | `ICO_OBJ_097` | Farol de cobre viejo | Se enciende con un fuego fatuo. Así alumbraba el Cuidador el sendero que se mueve. | La Bolsa |
| 433 | `ICO_OBJ_098` | Herradura de la suerte | Cuida el jardín: con ella el Cuidador regaba todo en una sola vuelta. | La Bolsa |
| 434 | `ICO_OBJ_099` | Taza de porcelana rota | Rota, pero sirve para traer agua. De la casita escondida: era la taza de Ondina. | La Bolsa |
| 435 | `ICO_OBJ_100` | Gafas del explorador | Con ellas ve nidos escondidos en los árboles. Tilde tiene tres nidos. Uno es de cosas robadas. | La Bolsa |
| 436 | `ICO_OBJ_101` | Catalejo de latón | Ve lejísimos. Desde el mirador se ve todo lo que el Cuidador cuidó. | La Bolsa |
| 437 | `ICO_OBJ_102` | Caja de música antigua | Toca una melodía chiquita: el principio de la canción del Cuidador. | La Bolsa |
| 438 | `ICO_OBJ_103` | Corona de hojas | Hojas secas trenzadas, chiquitas. Al Cuidador se le caía cada vez que se reía. | La Bolsa |
| 439 | `ICO_OBJ_104` | Frasco de perfume vacío | Guarda el olor de las flores mágicas. Doña Pétalo lo recomienda. | La Bolsa |
| 440 | `ICO_OBJ_105` | Dedal de plata | Protege el dedo de las espinas. Del tamaño de un dedo… o de una mano chiquita. | La Bolsa |
| 441 | `ICO_OBJ_106` | Anillo de sello real | Tiene el sello del Rey: una cuchara. Nadie sabe por qué. (Ahora tú sí.) | La Bolsa |
| 442 | `ICO_OBJ_107` | Gato de la suerte | Una figurita de madera que mueve la pata cuando algo bueno pasa. | La Bolsa |
| 443 | `ICO_OBJ_108` | Pipa de burbujas del druida | Hace burbujas cuadradas. El Cuidador las inventó para que el pino se riera. | La Bolsa |
| 444 | `ICO_OBJ_109` | Cuchara de palo | Como la del Cuidador: sirve para todo. Plantar, revolver, desenterrar lombrices. | La Bolsa |
| 445 | `ICO_OBJ_110` | Espejo de mano roto | Al mirarse, se ve como cuando era bebé. Ojalá supiera quién la cuidó desde antes. | La Bolsa |
| 446 | `ICO_OBJ_111` | Página suelta del diario | Letra chueca, una mancha de galleta y una cucharita dibujada en la esquina. | La Bolsa |
| 447 | `ICO_OBJ_112` | Candado rúnico | Cuida lo más valioso de la bolsa, como Marea cuida el faro. | La Bolsa |
| 448 | `ICO_OBJ_113` | Placa de reloj perdida | Una pantallita redonda que muestra la hora. Le parece conocida. | La Bolsa |
| 449 | `ICO_OBJ_114` | Peine de carey | Para peinarla. Ondina peina a los peces con uno igual. | La Bolsa |
| 450 | `ICO_OBJ_115` | Botella con mensaje | Una carta de otra mascota que vive lejos. También la cuidan entre varios. | La Bolsa |
| 451 | `ICO_OBJ_116` | Broche que Tilde devolvió | Tilde lo robó tres veces y lo devolvió tres veces. Ahora brilla de vergüenza. | La Bolsa |
| 452 | `ICO_OBJ_117` | Dado de hueso | Tíralo: si sale seis, el día viene con suerte. Tilde lo tira hasta que sale seis. | La Bolsa |
| 453 | `ICO_OBJ_118` | Pluma de escribano real | Escribe nombres con letras elegantes. Le falta escribir uno muy corto, como de árbol. | La Bolsa |
| 454 | `ICO_OBJ_119` | Llave de cuerda | Dale cuerda y camina como robot. Prisma dice que así caminaba él recién despierto. | La Bolsa |
| 455 | `ICO_OBJ_120` | Medallón del eclipse | Tapa el sol un ratito. Lumbre dice que en la oscuridad se ve mejor la luz chiquita. | La Bolsa |

## Objetos mágicos: artefactos místicos

| # | Ícono | Nombre | Qué significa | Dónde aparece |
|---|---|---|---|---|
| 456 | `ICO_OBJ_121` | Báculo rúnico de viento | Desliza el dedo rápido: una ráfaga, como las que sopla Anemoi. | La Bolsa |
| 457 | `ICO_OBJ_122` | Pócima de la alegría | Rosada y burbujeante. Ondina la toma después de llorar mucho. | La Bolsa |
| 458 | `ICO_OBJ_123` | Runa de teletransporte | Lleva de una zona a otra en un parpadeo, como el sendero que se mueve. | La Bolsa |
| 459 | `ICO_OBJ_124` | Lentes reveladores | Muestran el bosque mágico. Con ellos se ven los secretos que dejó el Cuidador. | La Bolsa |
| 460 | `ICO_OBJ_125` | Atrapasueños | Atrapa las pesadillas. Nimba duerme con uno enredado en la cola. | La Bolsa |
| 461 | `ICO_OBJ_126` | Elixir del crecimiento | Un trago y se siente grande. El Cuidador nunca lo tomó: le gustaba su tamaño. | La Bolsa |
| 462 | `ICO_OBJ_127` | Runa de fuego fatuo | Llama a un fuego fatuo que la cuida, para que nadie camine a oscuras. | La Bolsa |
| 463 | `ICO_OBJ_128` | Esfera del clima | Adentro llueve. Ondina la llenó con una lágrima de nube. | La Bolsa |
| 464 | `ICO_OBJ_129` | Cetro del rey del bosque | Le decían cetro, pero era la batuta de su orquesta de grillos. Los bichitos vienen a bailar. | La Bolsa |
| 465 | `ICO_OBJ_130` | Pócima de invisibilidad | Casi transparente. Tilde la quiere para robar sin que la vean. Nadie se la presta. | La Bolsa |
| 466 | `ICO_OBJ_131` | Amuleto del tiempo | El cielo pasa rapidísimo, como pasan los años para la Abuela Brizna. | La Bolsa |
| 467 | `ICO_OBJ_132` | Campana de purificación | Su sonido limpia todo, hasta los resfríos. El Cuidador la tocaba para los enfermos. | La Bolsa |
| 468 | `ICO_OBJ_133` | Espejo dimensional | Un portalito: lo que se encuentra hoy, se encuentra doble. | La Bolsa |
| 469 | `ICO_OBJ_134` | Incienso del druida | Humo morado. Lumbre medita con él en las noches sin luna. | La Bolsa |
| 470 | `ICO_OBJ_135` | Pócima de encogimiento | La deja chiquitita y curiosa, del tamaño exacto de alguien que conocemos. | La Bolsa |
| 471 | `ICO_OBJ_136` | Runa de cosecha | Todas las plantas maduran de golpe, como el bosque que apareció mientras la Abuela dormía. | La Bolsa |
| 472 | `ICO_OBJ_137` | Imán místico | Atrae las cosas perdidas. Tilde lo esconde cada vez que lo ve. | La Bolsa |
| 473 | `ICO_OBJ_138` | Cápsula de calma | Para los días largos. Prisma dice que cien años de sueño es demasiado; esto es justo. | La Bolsa |
| 474 | `ICO_OBJ_139` | Flauta de los espíritus | Toca una melodía que solo ustedes dos entienden. Las canciones son memoria. | La Bolsa |
| 475 | `ICO_OBJ_140` | Esencia de aurora | Pinta el cielo de verde y violeta, como la noche en que Prisma despertó. | La Bolsa |
| 476 | `ICO_OBJ_141` | Pócima de sol | Tan brillante que despeja el cielo. El cariño es luz. | La Bolsa |
| 477 | `ICO_OBJ_142` | Runa del escudo | Hoy nada malo le pasa a su comida. Marea talló una igual para el faro. | La Bolsa |
| 478 | `ICO_OBJ_143` | Talismán del pescador | La caña se vuelve mágica. El pez dorado ya sabe y viene a saludar. | La Bolsa |
| 479 | `ICO_OBJ_144` | Polvo de hadas | Espárcelo con el dedo. Las hadas lo dejan donde bailaron. | La Bolsa |
| 480 | `ICO_OBJ_145` | Bebida helada del norte | Hace nevar. Marea dice que el lago congelado también se puede cruzar, despacito. | La Bolsa |
| 481 | `ICO_OBJ_146` | Runa del despertar | Las plantas despiertan con todo su poder, como el mundo cuando alguien lo cuida. | La Bolsa |
| 482 | `ICO_OBJ_147` | Tónico de energía | Se le quita el sueño. Prisma pidió uno después de cien años. No funcionó. | La Bolsa |
| 483 | `ICO_OBJ_148` | Varita de destellos | Fuegos de colores. Para las fiestas que el Cuidador nunca se tomó. | La Bolsa |
| 484 | `ICO_OBJ_149` | Pócima de colores | Le cambia los colores un rato. Como cada criatura recuerda al Cuidador de otro color. | La Bolsa |
| 485 | `ICO_OBJ_150` | Corazón del Mundo | Medio corazón que late. Se siente como si buscara a alguien. | La Bolsa |

## Animales del bosque

| # | Ícono | Nombre | Qué significa | Dónde aparece |
|---|---|---|---|---|
| 240 | `ICO_ERIZO` | Erizo | Tímido: si ella espera quieta, se desenrolla y se hacen amigos | Prado y bosque, al atardecer y al amanecer |
| 241 | `ICO_ERIZO_BOLA` | Erizo en bolita | Se hizo bolita: ella corrió muy cerca | Prado y bosque |
| 242 | `ICO_ARDILLA` | Ardilla | Traviesa: le roba una semilla y a veces se la devuelve | Bosque y prado, de día |
| 243 | `ICO_CONEJO` | Conejo | Asustadizo y juguetón: juegan a perseguirse | Prado y colina |
| 244 | `ICO_CARACOL` | Caracol | Calmado: después de la lluvia deja un rastro brillante | Prado y bosque, con charcos |
| 245 | `ICO_TORTUGA` | Tortuga | Sabia y rara: a su lado se le pasa la pena | Lago e isla |
| 246 | `ICO_PATO` | Pato | Curioso: la sigue en fila por el lago | Lago, de día |
| 247 | `ICO_BUHO` | Búho | Sereno: ulula desde el árbol y, si está despierta, la arrulla | Prado y bosque, de noche |
| 248 | `ICO_TOPO` | Topo | Gruñón: asoma de un hoyo y se esconde si lo tocas | Colina, de día |
| 249 | `ICO_TOPO_HOYO` | Hoyo del topo | Un montoncito de tierra: el topo se escondió | Colina |
| 251 | `ICO_COLIBRI` | Colibrí | El más chiquito: se queda quieto en el aire frente a las flores | Prado, bosque y colina, con calor |
| 252 | `ICO_CARPINTERO` | Pájaro carpintero | Toc-toc: hace su casa en los troncos | Bosque y prado, de día |
| 253 | `ICO_MARTIN` | Martín pescador | Azul brillante: se tira al agua por un pez | Lago e isla |
| 254 | `ICO_GOLONDRINA` | Golondrina | Hace piruetas en el aire; llega con la primavera | Prado, lago y colina |
| 255 | `ICO_CISNE` | Cisne | Blanco y tranquilo: nada sin hacer olas | Lago e isla |
| 256 | `ICO_URRACA` | Urraca | Conversadora: le encantan las cosas brillantes | Prado y bosque |
| 257 | `ICO_CUERVO` | Cuervo | Listo y curioso: a veces le trae una piedrita brillante | Prado, bosque y colina |
| 258 | `ICO_GARZA` | Garza | Pesca parada en una pata | Lago |
| 259 | `ICO_MAPACHE` | Mapache | Con antifaz: lava la fruta antes de comer | Bosque y prado, de noche |
| 260 | `ICO_MURCIELAGO` | Murciélago | Duerme de cabeza; vuela al atardecer | Cueva; cruza el prado al atardecer |
| 261 | `ICO_NUTRIA` | Nutria | Juguetona: flota de espaldas | Lago |
| 262 | `ICO_CASTOR` | Castor | Constructor: golpea el agua con la cola | Lago |
| 263 | `ICO_RATON` | Ratón de campo | Chiquito y bigotudo: junta semillas | Prado y colina |
| 264 | `ICO_OSO` | Oso | Grande y dormilón: busca moras en otoño (raro) | Bosque, en otoño |
| 265 | `ICO_CIERVO_CAFE` | Ciervo | Elegante: camina entre los árboles al amanecer | Bosque, al amanecer y al atardecer |
| 266 | `ICO_TEJON` | Tejón | Rayado: cava túneles de noche | Bosque, de noche |
| 267 | `ICO_ZORRILLO` | Zorrillo | Blanco y negro, pacífico: mejor no asustarlo | Prado y colina, de noche |
| 268 | `ICO_GATO_MONTES` | Gato montés | Salvaje y silencioso (raro) | Prado, bosque y colina, al atardecer |
| 269 | `ICO_LINCE` | Lince | Orejas de pincel; muy tímido (raro) | Colina |
| 270 | `ICO_PUMA` | Puma | El gran gato de la colina (raro) | Colina, al atardecer |
| 271 | `ICO_LAGARTIJA` | Lagartija | Toma sol en las piedras | Colina y prado, con sol |
| 272 | `ICO_SALAMANDRA` | Salamandra | Naranja con puntitos: sale con el suelo mojado | Prado y bosque, con charcos |
| 273 | `ICO_CAMALEON` | Camaleón | Cambia de color | Colina |
| 274 | `ICO_CULEBRA` | Culebrita verde | Tímida: se desliza entre el pasto | Prado y colina, con sol |
| 275 | `ICO_MARIQUITA` | Mariquita | Roja con puntitos: trae buena suerte | Prado, bosque y colina |
| 276 | `ICO_LIBELULA` | Libélula | Vuela sobre el agua como un helicóptero | Lago y prado |
| 277 | `ICO_GRILLO` | Grillo | Canta de noche | Prado y colina, de noche |
| 278 | `ICO_HORMIGAS` | Hormigas | Trabajan en fila | Prado, bosque y colina |
| 279 | `ICO_ESCARABAJO` | Escarabajo | Con armadura brillante: rueda bolitas | Bosque y prado |
| 280 | `ICO_POLILLA` | Polilla | Baila alrededor de la luz de la luna | Prado y bosque, de noche |
| 303 | `ICO_PINGUINO` | Pingüino | Con frac de hielo: se desliza de panza | Lago, con frío o nieve |
| 304 | `ICO_FOCA` | Foca | Bigotuda y tranquila: toma sol sobre el hielo | Lago, con frío o nieve |
| 305 | `ICO_OSO_POLAR` | Osezno polar | Blanquito y curioso: se esconde en la nieve | Lago, con frío o nieve |

## Criaturas mágicas y secretos

| # | Ícono | Nombre | Qué significa | Dónde aparece |
|---|---|---|---|---|
| 281 | `ICO_MUSGUITO` | Musguito | El duende de musgo del prado | Prado; Tesoros |
| 282 | `ICO_LUMBRE` | Lumbre | El hada de luz del bosque | Bosque; Tesoros |
| 283 | `ICO_ONDINA` | Ondina | El espíritu del agua del lago | Lago; Tesoros |
| 284 | `ICO_VENTOLIN` | Anemoi | El dragón del viento de la colina | Colina; Tesoros |
| 285 | `ICO_CRISTALINO` | Cristalino | El gnomo de cristal de la cueva | Cueva; Tesoros |
| 286 | `ICO_MAREA` | Marea | La sirenita del faro de la isla | Isla; Tesoros |
| 291 | `ICO_MANTEL` | Mantel de picnic | El picnic con Musguito | Cita en el prado |
| 292 | `ICO_BARQUITO_HOJA` | Barquito de hoja | La carrera de barquitos con Ondina | Cita en el lago |
| 293 | `ICO_FARO` | Faro | El faro de la isla | Cita en la isla |
| 294 | `ICO_COMETA` | Cometa | La cometa de la colina, para los días de viento | Colina |
| 295 | `ICO_HONGO_LUZ` | Hongo que brilla | Los hongos del anillo de las hadas | Bosque, de noche |
| 296 | `ICO_HADA` | Hada | Las hadas del anillo | Bosque, de noche |

## Mensajeros de las cartas

| # | Ícono | Nombre | Qué significa | Dónde aparece |
|---|---|---|---|---|
| 287 | `ICO_PALOMA` | Paloma mensajera | Trae las cartas de los amigos | Llega una carta |
| 288 | `ICO_PALOMA_B` | Paloma (alas abajo) | Segundo cuadro del aleteo | Llega una carta |
| 289 | `ICO_GRULLA` | Grulla de papel | Trae las cartas de las que se fueron de viaje | Llega una carta |
| 290 | `ICO_GRULLA_ABRE` | Grulla desdoblándose | La grulla se abre y se vuelve carta | Llega una carta |

