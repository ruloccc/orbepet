/* Textos de la versión 23: cartas nuevas (amigos, familia, viajeros, criaturas), historias de las
 * criaturas, fichas de los animales, leyendas de las constelaciones y pistas de los secretos.
 * Lo incluye solo pet.c.
 *
 * Marcas: {N} su nombre, {A} otro amigo que conoce, {P} quien la dejó (mamá o papá), {o} {a} {la} {ella}
 * {una} {lo} según sea niña o niño, {p} "a" si la carta es de mamá y "o" si es de papá. Las cartas de los viajeros te las escriben a ti (no saben si eres niña o
 * niño): se dicen sin género ("mi persona favorita", "nos quedábamos dormidos juntos"). */

/* --- treinta y una cartas más de los amigos del barrio: casi todas mágicas --- */
static const carta_txt_t CARTAS_MAS[] = {
    {CT_MAGICA, "Anoche una estrella bajó a tomar agua en mi charco. Me dijo que estaba cansada de brillar y se quedó dormida en mi ventana hasta que amaneció."},
    {CT_MAGICA, "¡No me vas a creer! Encontré una semilla que suena como cascabel. La planté y al otro día salió una flor que se ríe cuando le hacen cosquillas."},
    {CT_MAGICA, "Un caracol me contó que su concha es un mapa del bosque. Lo seguí toda la tarde y llegamos a un lugar donde los hongos cantan bajito."},
    {CT_MAGICA, "Hoy llovió al revés: las gotitas subían hacia las nubes. Mi abuela dice que pasa una vez cada cien años. ¡Me alegra que me haya tocado verlo!"},
    {CT_MAGICA, "Atrapé un pedacito de arcoíris en un frasco. De noche brilla de siete colores. Cuando vengas, lo prendemos juntos."},
    {CT_MAGICA, "El viento me trajo una hoja con un dibujo de ti. No sé quién la dibujó, pero te quedó muy linda la sonrisa."},
    {CT_MAGICA, "En el bosque hay un árbol que da sombreros en vez de frutas. Me probé uno de hongo y me quedó perfecto."},
    {CT_MAGICA, "Una luciérnaga se perdió y la acompañé a su casa. En agradecimiento, me dejó una lucecita que se prende cuando estoy feliz. Ahora está prendida porque te escribo."},
    {CT_MAGICA, "Mi sombra se fue a jugar sola esta mañana. Volvió a la hora de almuerzo con barro en los pies. ¡Ni me quiso contar dónde anduvo!"},
    {CT_MAGICA, "Hay un puente de telaraña entre dos pinos que solo aparece con el rocío. Lo crucé de puntitas y del otro lado olía a canela."},
    {CT_MAGICA, "Soñé que Nimba, la ballena de nubes, me llevaba a pasear. Cuando desperté, tenía polvo de estrellas en las orejas. ¿Será que no fue un sueño?"},
    {CT_MAGICA, "Encontré una piedra que guarda los sonidos. Le grité tu nombre y ahora, cuando la aprieto, dice \"{N}\" bajito."},
    {CT_MAGICA, "La luna estaba tan baja anoche que casi la toco con una rama. Le pedí que te cuidara mientras duermes."},
    {CT_MAGICA, "En mi jardín apareció una flor que cambia de color según el ánimo. Hoy está amarilla, porque me acordé de ti."},
    {CT_MAGICA, "Un hada chiquitita se metió en mi taza de té. La saqué con una cuchara y me regaló un estornudo de brillitos."},
    {CT_MAGICA, "El río me devolvió un barquito que hice el año pasado. Venía con un pasajero nuevo: un sapito con sombrero."},
    {CT_MAGICA, "Dicen que si silbas tres veces al pie del sauce, las hojas te cuentan un chiste. Lo intenté y me reí toda la tarde."},
    {CT_MAGICA, "Encontré huellas brillantes en la nieve. Las seguí y terminaban en el aire, como si alguien hubiera echado a volar."},
    {CT_MAGICA, "Mi tetera canta canciones cuando hierve el agua. Hoy cantó una que no conocía. Creo que la aprendió del viento."},
    {CT_MAGICA, "Encontré un caracol de mar en medio del bosque. Adentro se escuchan olas y alguien que se ríe."},
    {CT_MAGICA, "Las nubes de hoy tenían forma de animales. Vi un conejo, un barco y una que se parecía muchísimo a ti."},
    {CT_MAGICA, "Me encontré con un zorro que sabe hacer trucos de magia. Desapareció mi bufanda y la hizo aparecer en la rama más alta. Todavía la estoy bajando."},
    {CT_MAGICA, "Anoche las luciérnagas escribieron una palabra en el cielo. Decía \"hola\". Yo creo que era para ti."},
    {CT_MAGICA, "Hay un charco en mi camino que refleja otro cielo, uno con dos lunas. Me quedé mirándolo hasta que se hizo de noche de verdad."},
    {CT_MAGICA, "Planté una pluma por equivocación y creció un árbol con hojas que vuelan solas. Ahora tengo que barrer el cielo."},
    {CT_MAGICA, "Un chuncho me enseñó a contar estrellas. Llegamos hasta ciento tres y nos quedamos dormidos. Mañana seguimos."},
    {CT_MAGICA, "Encontré una llave chiquita en una bellota. No sé qué abre, pero la guardo por si algún día encontramos la puerta juntos."},
    {CT_MAGICA, "En el cerro hay una roca que, si la abrazas, se pone tibia. Me quedé ahí un rato y me acordé de tus abrazos."},
    {CT_VIDA, "Hoy ordené mi casa y encontré un dibujo que hicimos juntos. Lo colgué arriba de mi cama. ¡Te quedó muy bonita la nube!"},
    {CT_VIDA, "Aprendí a hacer pan de nueces. La primera vez quedó duro como piedra, la segunda, riquísimo. La tercera te la guardo a ti."},
    {CT_OTRO_AMIGO, "{A} y yo armamos una casita en el árbol. Tiene una ventana redonda y un columpio. Hay un lugar guardado para ti."},
};

/* --- treinta cartas de mamá o papá (solo si nació de un huevo de familia) --- */
static const char *const CARTAS_FAMILIA[] = {
    "Me acuerdo del día en que te dejé en tu huevito. Era tan redondo y tibio. Te canté bajito para que soñaras bonito.",
    "¿Comes bien? ¿Duermes tus siestas? Ya sé, ya sé, no me vas a contestar... pero una igual se preocupa un poquito.",
    "Hoy vi una flor igual a las que crecían en nuestro prado. Me la puse detrás de la oreja y pensé en ti todo el día.",
    "Cuando tengas miedo, respira hondo y piensa en algo calentito. Así hacía yo cuando era chiquit{p} como tú.",
    "Me contaron que estás creciendo mucho. Me pone tan orgullos{p}... ¡Sigue así, mi tesoro!",
    "Te mando un abrazo de esos que aprietan fuerte. Guárdalo debajo de la almohada para cuando lo necesites.",
    "Cuida bien a quien te cuida. Es la persona más buena del mundo: yo lo sé porque también me cuidó a mí.",
    "Cuando yo era pequeñ{p}, me escondía en el árbol hueco para que me buscaran. ¿Ya lo encontraste tú?",
    "Si alguna vez te sientes sol{o}, mira la luna. Yo también la miro, y así estamos juntos aunque estemos lejos.",
    "Aprendí que las cosas bonitas toman tiempo: las flores, los árboles y las amistades. Ten paciencia, que vale la pena.",
    "Hoy me acordé de tu primera risa. Fue tan chistosa que se rieron hasta los pajaritos.",
    "No importa si te equivocas. Todos nos equivocamos. Lo importante es volver a intentarlo con una sonrisa.",
    "Te dejé un secreto en el prado hace mucho tiempo. No te digo dónde... ¡pero a lo mejor ya lo encontraste!",
    "Los días de lluvia son para saltar en los charcos. Eso me lo enseñaron tus abuelos, y ahora te lo enseño yo.",
    "Me dijeron que tienes amigos nuevos. ¡Qué alegría! Los amigos son como las estrellas: no siempre los ves, pero siempre están.",
    "Cuando te portas con cariño, el bosque entero se pone contento. Lo noté desde aquí.",
    "Te extraño, pero estoy feliz, porque sé que estás bien cuidad{o}. Eso es lo que más quiere una familia.",
    "Te cuento un secreto de familia: a todos nos gusta que nos rasquen detrás de las orejas. Pídelo, que no da vergüenza.",
    "Hoy hice un pastel de frutillas y me acordé de que eran tus favoritas cuando eras un huevito... bueno, eso creo yo.",
    "Si te cuesta dormir, cuenta ovejitas de nube. A mí me funciona desde que era chiquitit{p}.",
    "Me encanta imaginarte corriendo por el prado. Corre mucho, juega mucho y ríete mucho más.",
    "No hace falta ser {la} más rápid{o} ni {la} más fuerte. Basta con ser como tú: eso ya es perfecto.",
    "Cuida tu jardín, que las plantas también sienten cariño. Háblales bonito y crecen más alegres.",
    "Hoy encontré una piedrita con forma de corazón. La guardé en mi bolsillo para dártela cuando nos veamos.",
    "Cuando seas grande, quizás tú también dejes un huevito. Y vas a entender cuánto te quiero.",
    "Te mando un beso en la frente, otro en la nariz y otro en la panza. Ese último hace cosquillas.",
    "El mundo es enorme y lleno de sorpresas. Explóralo sin apuro, que tienes todo el tiempo del mundo.",
    "Me preguntaron cómo es mi hij{o} y les dije: \"{la} más tiern{o} del bosque\". Nadie me contradijo.",
    "Acuérdate de dar las gracias, de compartir tu fruta y de abrazar mucho. Lo demás se aprende solo.",
    "Pase lo que pase, siempre vas a ser mi tesoro. Siempre, siempre, siempre.",
};

/* --- cuarenta y cinco cartas de las que se fueron de viaje: diez por cada camino, en orden, y cinco más --- */
static const char *const CARTAS_VIAJERO[] = {
    /* 0: la pluma gigante, por las auroras del cielo */
    "¡Ya estoy volando! La pluma es más calentita de lo que parece. Desde aquí arriba el prado se ve como un pañuelo verde.",
    "Llegué a las auroras: son cortinas de luz que bailan solas. Una se me enredó en la cola y quedé brillando de color verde.",
    "Conocí a unos pájaros de escarcha que viven en las nubes. Me enseñaron a dormir flotando. ¡Es comodísimo!",
    "Hoy la pluma y yo pasamos por un arcoíris tan bajito que lo pude tocar. Es tibio y suena como campanitas.",
    "En lo más alto del cielo hay un faro de estrellas que guía a las aves perdidas. El farero es un chuncho viejito muy simpático.",
    "Llovió polvo de estrellas toda la tarde. Junté un poquito en un bolsillo. Me hace cosquillas cuando me río.",
    "Las auroras de hoy tenían forma de ballena. Una me saludó con la cola. Creo que se acordaba de mí.",
    "Descubrí que las nubes tienen sabor. Las blancas saben a leche con miel; las grises, a sopa de lluvia.",
    "Me encontré con el viento del norte. Es gruñón, pero me regaló un silbido para cuando me pierda.",
    "Hoy vi la Tierra entera desde arriba y busqué nuestro bosque. Lo encontré: es el puntito que más brilla.",
    /* 1: el barquito de hoja, por el lago de estrellas */
    "¡Zarpamos! El barquito de hoja navega solito. El agua de la laguna de estrellas es tan clara que parece que flotara en el cielo.",
    "Debajo del barco nadan peces de luz. Cuando apago mi linterna, ellos me alumbran el camino.",
    "Paré en una isla tan chiquita que solo cabía una flor. Le conversé un rato para que no se sintiera sola.",
    "Me encontré con una tortuga que lleva una ciudad en su caparazón. Me invitaron a tomar té en la plaza.",
    "Esta noche la laguna reflejó dos lunas. Una era de verdad y la otra era tímida. No te digo cuál.",
    "Hoy el viento sopló al revés y el barquito retrocedió un día entero. ¡Me tocó ver otra vez el mismo atardecer!",
    "Una sirena de agua dulce me enseñó una canción para llamar a los delfines de estrellas. Vinieron siete.",
    "Llegué a la cascada que cae hacia arriba. El barquito subió por ella como por un tobogán al revés.",
    "En medio de la laguna hay un faro con una abuelita que teje mapas. Me regaló uno que se dibuja solo mientras navego.",
    "Hoy pesqué una estrella chiquita. La miré un ratito y la devolví al agua. Me guiñó un ojo al hundirse.",
    /* 2: el tren de luciérnagas, por el puente arcoíris */
    "¡Chu-chuuu! El tren de luciérnagas va lleno de pasajeros raros: un pichi con maleta, dos ranas y un señor caracol.",
    "Cruzamos el puente arcoíris. Cada color suena distinto bajo las ruedas: el rojo hace bom, el azul hace tilín.",
    "La primera estación se llama Merienda. Venden pasteles de nube y jugo de rocío. Me comí tres.",
    "Las luciérnagas que tiran el tren se cansan a medianoche. Entonces todos los pasajeros les cantamos para darles fuerzas.",
    "Pasamos por un túnel de flores. Olía tanto a jazmín que todos estornudamos al mismo tiempo.",
    "En la estación del Medio vive un reloj que nunca tiene apuro. Si te sientas a su lado, el tiempo pasa lentito.",
    "Hoy el tren se detuvo porque un dragón chiquitito dormía en la vía. Esperamos a que despertara. Tenía cara de bebé.",
    "Desde la ventana vi un pueblo hecho de galletas. Los techos eran de chocolate. No me dejaron bajar, ¡qué pena!",
    "El conductor del tren es un grillo con gorrita. Me dejó tocar el silbato. Sonó tan fuerte que salieron estrellas.",
    "Llegamos a la última estación del arcoíris: desde aquí se ven todos los colores juntos. Te guardé el más bonito.",
    /* 3: el diente de león, por las montañas que cantan y los bosques de cristal */
    "¡Voy volando, colgando de mi diente de león! Cuando sopla el viento, las semillas me llevan donde quieren.",
    "Llegué a las montañas que cantan. Cada montaña tiene su voz: la más alta canta grave y la chiquita, finito.",
    "Al atardecer, las montañas cantan todas juntas. Me quedé en una cumbre escuchando hasta que salió la luna.",
    "Una cabra de las montañas me enseñó a saltar de roca en roca. Me caí solo dos veces... bueno, cinco.",
    "Hoy aterricé en un bosque de cristal. Los árboles tintinean cuando los toca el viento y el sol hace arcoíris en el suelo.",
    "En el bosque de cristal todo se ve doble: dos lunas, dos caminos, dos yo. Mi otro yo me sacó la lengua.",
    "Las hojas de cristal no se caen en otoño: se ponen de colores y cantan una canción de despedida al verano.",
    "Encontré una laguna congelada donde se ven los sueños de los que pasan. Vi uno tuyo: estábamos jugando.",
    "Un zorro de cristal me acompañó un tramo. No hablaba, pero cuando sonreía brillaba entero.",
    "El viento me dejó en la cima de la montaña más alta. Desde aquí te mando un saludo con eco: ¡hola, hola, hola!",
    /* 4: cinco más (cualquier camino) */
    "Te extraño mucho, mi persona favorita. Me acuerdo de cómo nos quedábamos dormidos juntos después de jugar.",
    "A veces, cuando miro las estrellas, me parece que tú las estás mirando también. Así no me siento lejos.",
    "Guardé todos nuestros recuerdos en una cajita: la primera fruta, el primer baño, la primera vez que me reí contigo.",
    "No te pongas triste por mí: estoy viendo cosas maravillosas. Pero ninguna tan bonita como nuestro prado.",
    "Algún día, cuando el viento sople hacia tu casa, a lo mejor sientes un abrazo. Seré yo.",
};

#include "textos_historia.h" /* (versión 37) las criaturas, sus historias, cartas y los personajes nuevos */

/* --- los animales: cómo se llaman, lo que hacen y su historia para los Tesoros --- */
/* (las fichas 0 a 4 son la fauna de siempre; después, los animales en orden) */
static const char *const FICHA_NOMBRE[] = {
    "la mariposa", "la abeja", "el chincol", "el zorro", "la rana",
    "el pichi", "el monito del monte", "el conejo", "el caracol", "la tortuga", "el pato", "el chuncho", "el topo",
    "el picaflor", "el pájaro carpintero", "el martín pescador", "la golondrina", "el cisne de cuello negro", "la urraca", "el tordo", "la garza",
    "el quique", "el murciélago", "la nutria", "el castor", "el ratón de campo", "el oso", "el huemul", "la vizcacha", "el zorrillo",
    "la güiña", "el gato colocolo", "el puma",
    "la lagartija", "la ranita de Darwin", "el camaleón", "la culebrita",
    "la chinita", "el matapiojos", "el grillo", "las hormigas", "el escarabajo", "la polilla",
    "el pingüino", "el lobo marino", "el elefante marino"};
static const char *const FICHA_HISTORIA[] = {
    "Vuela de flor en flor sin apuro. Dicen que cada vez que una mariposa aletea, en algún lugar alguien sonríe.",
    "Trabaja todo el día juntando el polen de las flores. Si la persigues, se enoja; si la dejas tranquila, te deja miel.",
    "Canta temprano para despertar al sol. A veces deja semillas de regalo a quien le cae bien.",
    "Astuto y curioso. Le encantan las frutas ajenas, pero se asusta fácil si lo pillan.",
    "Salta entre los charcos después de la lluvia. Croa más fuerte cuando está contenta.",
    "Chiquito y con armadura, como un caballero de bolsillo. Tímido como nadie: si alguien corre cerca, se esconde en su caparazón. Pero si lo esperas quieto, se vuelve tu amigo.",
    "Chiquitito, de ojos enormes y cola enrollada, es primo lejano de los canguros. Travieso: esconde semillas por todas partes y a veces se olvida de dónde las dejó.",
    "Orejas largas, patitas rápidas. Le encanta jugar a perseguirse, pero los truenos lo asustan.",
    "Sale después de la lluvia y deja un caminito brillante. Lleva su casa a todas partes, sin apuro.",
    "La más sabia de la laguna. Si estás triste, sentarse a su lado ayuda a que se pase la pena.",
    "Curioso y conversador. Si le caes bien, te sigue en fila por la orilla.",
    "Una lechucita chica de ojos amarillos que vigila el bosque de noche. Dicen que su canto trae noticias; a ella le suena a canción de cuna.",
    "Un poco gruñón, vive bajo el cerro. Asoma la nariz para ver quién anda por ahí y se esconde al tiro.",
    "El pajarito más chico de todos. Mueve tan rápido las alas que zumba, y puede volar hacia atrás. Le encantan las flores del copihue.",
    "Toc, toc, toc: golpea los troncos con el pico para hacer su casa y buscar bichitos. Nunca le duele la cabeza.",
    "Un pájaro azul brillante que se lanza al agua como una flecha. Casi nunca falla un pez.",
    "Llega con la primavera y se va con el otoño. Vuela haciendo piruetas y hace su nido de barro.",
    "Blanco, con el cuello negro como de terciopelo. Nada sin hacer olas y lleva a sus pollitos en la espalda. Es el animal más tranquilo de la laguna.",
    "Blanca y negra, muy conversadora. Le encantan las cosas brillantes y a veces se lleva alguna.",
    "Negro brillante y muy conversador; anda en bandadas y silba bonito. Recuerda las caras de quienes son amables con él y a veces les trae regalitos.",
    "De patas larguísimas, pesca parada en el agua sin moverse. Tiene una paciencia enorme.",
    "Gris por arriba, negro por abajo y con una franja blanca en la frente: parece un ladroncito enmascarado, pero solo es curioso. Es rapidísimo y muy juguetón.",
    "Duerme de cabeza durante el día y sale a volar al atardecer. Ve con las orejas.",
    "Juguetona y nadadora. Flota de espaldas y a veces se toma de la mano con otras nutrias para no separarse.",
    "Constructor de la laguna: con ramas y barro hace su casa y sus diques. Golpea el agua con la cola para avisar.",
    "Chiquito y bigotudo, junta semillas para el invierno. Se asusta con su propia sombra.",
    "Grande, peludo y dormilón. Aparece en el otoño buscando moras antes de su larga siesta de invierno.",
    "Elegante y silencioso, camina entre los coihues al amanecer. Sus cuernos crecen como ramas. Está en el escudo de Chile, y hay muy pocos: hay que cuidarlo.",
    "Orejona y de cola larga, parece conejo pero no lo es. Toma sol en las rocas del cerro y, si se asusta, silba y se esconde.",
    "Blanco y negro, con una cola muy esponjosa. Es pacífico; solo hay que no asustarlo.",
    "La gatita salvaje más chica de América, con manchitas por todo el cuerpo. Vive escondida en el bosque y casi nunca se deja ver.",
    "Un gato salvaje con rayitas en las patas y ojos dorados. Es muy tímido: si te mira, es que confía en ti.",
    "El gran gato del cerro. Fuerte y silencioso, vigila desde las rocas sin molestar a nadie.",
    "Le encanta tomar sol en las piedras calientes. Si la asustas, sale corriendo rapidísimo.",
    "Chiquitita y con la nariz en punta, se hace la hoja seca para que no la vean. El papá guarda a los renacuajos en su boca hasta que crecen.",
    "Cambia de color según dónde esté y mueve cada ojo para un lado. Tiene una lengua larguísima.",
    "Verde como una hoja, se desliza entre el pasto sin hacer ruido. Es tímida y no hace daño.",
    "Roja con puntitos negros. Dicen que trae buena suerte a quien se le posa en la mano: hay que pedir un deseo.",
    "Alas de vidrio y colores brillantes. Vuela sobre el agua como un helicóptero chiquitito. Su nombre asusta, pero no le hace nada a nadie.",
    "Canta de noche frotando sus alas. Cuanto más calor hace, más rápido canta.",
    "Trabajan todas juntas, en fila. Pueden cargar cosas mucho más grandes que ellas.",
    "Con armadura brillante como una joya. Rueda bolitas y conoce todos los caminos del suelo.",
    "Vuela de noche buscando la luz de la luna. Sus alas tienen polvo suave como terciopelo.",
    "Camina como con frac y se desliza de panza sobre el hielo. Solo viene a la laguna cuando hace mucho frío, y siempre en compañía.",
    "Redondo, bigotudo y con melena, descansa sobre el hielo con los ojos entrecerrados. Le gusta que le canten bajito.",
    "Enorme y dormilón, con la nariz como una trompita. Solo viene cuando hace mucho frío, y ronca tan fuerte que tiembla el hielo.",
};
/* lo que hace cada animal nuevo (globito de ella, en mayúsculas en la interfaz) */
static const char *const ANIMAL_HACE[ANIMAL_COUNT - ANIMAL_V22] = {
    "¡El picaflor se quedó quieto en el aire frente a una flor!",
    "¡Toc, toc, toc! El carpintero está haciendo su casa",
    "¡El martín pescador se tiró al agua y sacó un pez!",
    "¡La golondrina hizo una pirueta en el aire!",
    "El cisne de cuello negro lleva a sus pollitos en la espalda",
    "¡La urraca se llevó algo brillante!",
    "¡El tordo me trajo una piedrita brillante!",
    "La garza se quedó en una pata. ¿Cómo no se cae?",
    "¡El quique pasó corriendo como un rayo!",
    "¡El murciélago pasó volando cerquita!",
    "¡La nutria está flotando de espaldas!",
    "¡El castor golpeó el agua con la cola!",
    "El ratón de campo está juntando semillas",
    "El oso está buscando moras... ¡qué grande es!",
    "El huemul me miró y movió las orejas",
    "La vizcacha está tomando sol en una roca",
    "Shhh... el zorrillo pasa tranquilo. Mejor no lo asustemos",
    "La güiña me miró desde lejos. ¡Qué chiquitita!",
    "¡Vi al gato colocolo! Tiene rayitas en las patas",
    "El puma vigila desde las rocas. Qué elegante",
    "La lagartija está tomando sol en la piedra",
    "La ranita de Darwin se hizo la hoja seca. ¡Casi no la veo!",
    "¡El camaleón cambió de color!",
    "La culebrita verde se escondió en el pasto",
    "¡Se me posó una chinita! Hay que pedir un deseo",
    "El matapiojos vuela sobre el agua como un helicóptero",
    "Cri-cri, cri-cri: el grillo está cantando",
    "Las hormigas van en fila llevando una miga gigante",
    "El escarabajo está rodando una bolita",
    "La polilla baila alrededor de la luz de la luna",
    "¡El pingüino se tiró de panza y se deslizó por el hielo!",
    "El lobo marino está tomando el sol sobre el hielo de la laguna",
    "El elefante marino infló la trompa y roncó. ¡Tembló el hielo!",
};

/* --- el reflejo encantado del lago: constelaciones que no existen en el cielo --- */
static const char *const CONSTELACION[PET_CONSTELACIONES] = {"La Tetera", "El Caracol Viajero", "La Niña del Farol", "El Pez Sonriente", "La Escoba", "El Abrazo"};
static const char *const CONSTELACION_LEYENDA[PET_CONSTELACIONES] = {
    "Una abuela del cielo hacía té para todas las estrellas que llegaban tarde. Un día se le cayó la tetera a la laguna, y desde entonces el agua refleja su forma, siempre humeante.",
    "Un caracol quiso conocer el cielo y subió por un tallo durante mil noches. Cuando llegó, las estrellas lo admiraron tanto por su paciencia que le hicieron un lugar entre ellas.",
    "Una niña buscaba a su gatito con un farol. Lo encontró dormido en la luna, y se quedó ahí para cuidarlo. Su farol es la estrella más brillante del reflejo.",
    "Un pez de la laguna saltó tan alto que se quedó pegado en el cielo. No se puso triste: desde arriba ve a todos sus amigos y les sonríe cada noche.",
    "Con esta escoba las hadas barren las nubes para que se vean las estrellas. Cuando está en el reflejo, al otro día seguro amanece despejado.",
    "Dos estrellas que se querían mucho estaban en lados opuestos del cielo. La laguna las juntó en su reflejo, y ahí se abrazan todas las noches.",
};

/* --- pistas de los secretos (las dice ella, de vez en cuando; nunca en las cartas) --- */
static const char *const PISTA_ANILLO[] = {
    "Anoche soñé que los hongos del bosque brillaban...",
    "Dicen que en el bosque hay un círculo de hongos donde bailan las hadas",
    "Cuando hay luna, los hongos del bosque se prenden. ¿Iremos a ver?",
};
static const char *const PISTA_REFLEJO[] = {
    "De noche la laguna se ve distinto. Como si tuviera su propio cielo",
    "Me pareció ver estrellas raras en el agua de la laguna",
    "Las estrellas de la laguna no son iguales a las del cielo... ¿las tocamos?",
};
static const char *const PISTA_COMETA[] = {
    "Cuando hay viento, en el cerro se podría elevar una cometa",
    "Vi una cometa enredada en el cerro. Un día de viento podríamos volarla",
    "Si inclinas el orbe, la cometa se mueve. ¡Hay que mantenerla arriba!",
};
/* el juego de los cristales (Simón dice) */
static const char *const PISTA_CRISTALES[] = {
    "En la cueva, los cristales quieren que alguien les siga la canción",
    "Si juegas con los cristales de la cueva, a lo mejor despiertan algo...",
    "Escuché algo grande respirando bajito al fondo de la cueva",
};

