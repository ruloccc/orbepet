/* Catálogo de los 150 objetos mágicos (nombres de la propuesta del dueño; leyendas ajustadas a este mundo:
 * sin tienda ni monedas, el lago en vez de un río, el zorro en vez de mapaches). */
#include "objetos.h"

#include <stddef.h>

#define T OCAT_TIERRA
#define F OCAT_FLORA
#define E OCAT_ESPIRITUS
#define A OCAT_ANTIGUO
#define R OCAT_ARTEFACTO
/* O(nombre, categoría, familia, efecto, param, gesto, rareza, lugares, noche, se_vuelve, cambia_de_noche, c1, c2, leyenda) */
#define O(n, c, f, e, p, g, r, l, no, sv, nc, c1, c2, ley) {n, c, f, e, p, g, r, l, no, sv, nc, c1, c2, ley},

static const objeto_info_t OBJ[PET_OBJETOS] = {
    /* ---- 1. Tesoros de la tierra ---- */
    O("Piedra brillante", T, OFA_GEMA, OEF_CIELO, 7, OGE_TOCAR, 1, 0, 0, 0, 0, 0xFFE27A, 0xFFFFFF, "Guarda luz de luna. El Cuidador la usaba para contar estrellas cuando se le perdía la cuenta.")
    O("Geoda olvidada", T, OFA_PIEDRA, OEF_CIELO, 2, OGE_MANTENER, 2, OL_CUEVA | OL_COLINA, 0, 0, 0, 0x8A8498, 0xB48CFF, "Común por fuera. Cristalino dice que adentro guarda una nota en mi bemol. Apriétala: se abre.")
    O("Piedra pómez astral", T, OFA_PIEDRA, OEF_ENERGIA, 0, OGE_TOCAR, 2, OL_COLINA, 0, 0, 0, 0xD8D0C0, 0x9CF0FF, "Tan liviana que flota. Anemoi la usa para practicar vuelos sin caerse (casi nunca).")
    O("Pirita flotante", T, OFA_GEMA, OEF_COMPA, 1, OGE_TOCAR, 2, OL_CUEVA, 0, 0, 0, 0xE0B040, 0xFFF2A0, "Oro de mentira, cariño de verdad: una luciérnaga dorada siempre vuelve a ella.")
    O("Cuarzo ahumado", T, OFA_CRISTAL, OEF_CLIMA, 0, OGE_TOCAR, 2, OL_CUEVA, 0, 0, 0, 0x7A6A68, 0xC8B8B0, "Se traga la neblina. Lumbre dice que así el Cuidador veía el camino a casa.")
    O("Lágrima de laguna", T, OFA_GEMA, OEF_CALMA, 0, OGE_TOCAR, 1, OL_LAGO, 0, 0, 0, 0x6FB7FF, 0xD8F0FF, "Una lágrima de cuando Ondina era nube. Dice que ya no le duele: se volvió piedra bonita.")
    O("Piedra rúnica base", T, OFA_RUNA, OEF_MIRAR, 0, OGE_TOCAR, 1, OL_COLINA | OL_CUEVA, 0, 0, 0, 0x8A8A9A, 0x5A5A6A, "Una roca con una sola línea. Peumo marcaba así cada lugar que ya había cuidado ese día.")
    O("Ámbar antiguo", T, OFA_GEMA, OEF_MIRAR, 0, OGE_TOCAR, 3, OL_BOSQUE, 0, 0, 0, 0xF2A33A, 0x6A4A2A, "Guarda un mosquito de hace mil años y el olor del bosque cuando todavía era prado de piedras.")
    O("Fragmento de meteorito", T, OFA_PIEDRA, OEF_CIELO, 3, OGE_TOCAR, 3, OL_COLINA, 1, 0, 0, 0x4A3A48, 0xFF7A4A, "Todavía tibio. Lo trajo la cometa del Cuidador de muy, muy lejos.")
    O("Lodo magnético", T, OFA_PIEDRA, OEF_MIRAR, 0, OGE_FROTAR, 1, OL_LAGO, 0, 0, 0, 0x3A2A1A, 0x6A5A4A, "Barro que se mueve solo. Musguito dice que así era el prado antes de la primera semilla.")
    O("Guijarro cantarín", T, OFA_PIEDRA, OEF_DECORAR, 0, OGE_TOCAR, 1, OL_LAGO, 0, 0, 0, 0x9AA8B8, 0xFFFFFF, "Suena una notita cuando ella pasa. Cristalino lo afinó para que suene en sol.")
    O("Ágata de fuego", T, OFA_GEMA, OEF_CALMA, 0, OGE_FROTAR, 2, OL_COLINA, 0, 0, 0, 0xE8574F, 0xFFB04A, "Frotada se calienta. Marea la llevaba en el bote las noches frías del cruce.")
    O("Jade verde vivo", T, OFA_GEMA, OEF_ENERGIA, 0, OGE_TOCAR, 2, OL_BOSQUE, 0, 0, 0, 0x3E9E5E, 0x9CE8A8, "Del bosque más viejo: quita el cansancio. El Cuidador se la daba a todos. Nunca se quedó con una.")
    O("Obsidiana espejo", T, OFA_CRISTAL, OEF_MIRAR, 0, OGE_TOCAR, 2, OL_CUEVA, 0, 0, 0, 0x1A1626, 0x8AB8FF, "Una piedra negra que refleja el cielo de ahora. El Cuidador se peinaba en ella. No servía.")
    O("Mica reluciente", T, OFA_POLVO, OEF_CIELO, 5, OGE_TOCAR, 1, OL_COLINA, 0, 0, 0, 0xC8C8D8, 0xFFFFFF, "Se deshace en purpurina. Las hadas la usan para sus fiestas de media luna.")
    O("Piedra de trueno", T, OFA_PIEDRA, OEF_CLIMA, 5, OGE_TOCAR, 3, 0, 0, 0, 0, 0x4A4A6A, 0xFFE27A, "La dejó un rayo. Las tormentas también cantan: cada trueno es una nota.")
    O("Jaspe rojo", T, OFA_PIEDRA, OEF_CARINO, 0, OGE_MANTENER, 2, OL_COLINA, 0, 0, 0, 0xB83A3A, 0xFF8F8F, "Late despacito, como el corazón de quien te quiere. Dicen que al Cuidador le latía igual.")
    O("Mármol de hada", T, OFA_ORBE, OEF_ENERGIA, 0, OGE_TOCAR, 2, OL_BOSQUE, 0, 0, 0, 0xF0EEF8, 0xB48CFF, "Una bolita perfecta. Las hadas juegan a los bolos con ellas; el Cuidador siempre perdía.")
    O("Carbón fatuo", T, OFA_PIEDRA, OEF_COMPA, 0, OGE_TOCAR, 3, OL_CUEVA, 1, 0, 0, 0x2A2A3A, 0x5ED0FF, "Nunca se apaga. El cariño es luz, y este carbón no se olvida de quien lo encendió.")
    O("Escama de Prisma", T, OFA_CRISTAL, OEF_SUERTE, 0, OGE_TOCAR, 3, OL_CUEVA, 0, 0, 0, 0xB48CFF, 0xFFFFFF, "Una escama de Prisma, el dragón de cristal. Vibra cuando hay algo raro cerca.")
    O("Roca de musgo seco", T, OFA_PIEDRA, OEF_CLIMA, 0, OGE_TOCAR, 1, OL_BOSQUE, 0, 0, 0, 0x8A8A7A, 0x7FB86A, "Absorbe la lluvia. Ondina se enoja: dice que la lluvia también es bonita.")
    O("Pedernal de chispas azules", T, OFA_PIEDRA, OEF_CIELO, 2, OGE_FROTAR, 2, OL_COLINA, 0, 0, 0, 0x6A6A7A, 0x5ED0FF, "Frotado saca chispas cian. Anemoi lo usa para encender estrellas chiquitas.")
    O("Talco de estrellas", T, OFA_POLVO, OEF_CIELO, 2, OGE_TOCAR, 2, OL_CUEVA, 1, 0, 0, 0xE8E0FF, 0xFFE27A, "Polvo de estrellas que trae Nimba de sus paseos por el firmamento.")
    O("Gema de cueva profunda", T, OFA_GEMA, OEF_DECORAR, 0, OGE_TOCAR, 3, OL_CUEVA, 0, 0, 0, 0x2E4A9A, 0x9CF0FF, "Brilla más cuanto más oscuro está, como las luciérnagas de Lumbre.")
    O("Fósil de helecho", T, OFA_PIEDRA, OEF_JARDIN, 1, OGE_TOCAR, 2, OL_COLINA | OL_CUEVA, 0, 0, 0, 0xC8B89A, 0x7A9A5A, "Una hoja en la piedra, de cuando todo empezó chiquito.")
    O("Piedra de sal rosada", T, OFA_GEMA, OEF_PROTEGE, 0, OGE_TOCAR, 2, OL_CUEVA, 0, 0, 0, 0xF0A0B8, 0xFFE0E8, "Espanta lo molesto. Musguito la pone en la puerta cuando quiere estar tranquilo.")
    O("Grava movediza", T, OFA_POLVO, OEF_MIRAR, 0, OGE_TOCAR, 1, OL_COLINA, 0, 0, 0, 0x9A8A7A, 0x6A5A4A, "Cuando no la miras, cambia de lugar. Tilde jura que ella no fue.")
    O("Corazón de montaña", T, OFA_PIEDRA, OEF_CALMA, 0, OGE_MANTENER, 3, OL_COLINA, 0, 0, 0, 0x5A4A5A, 0xE0485E, "Pesadísimo. Dicen que es un pedacito del Señor Don Gigante del cerro.")
    O("Estalactita diminuta", T, OFA_CRISTAL, OEF_CLIMA, 6, OGE_TOCAR, 2, OL_CUEVA, 0, 0, 0, 0xC8E8FF, 0xFFFFFF, "Una gotita de piedra helada. ¡Ojo con ponerla en la frente de un dragón!")
    O("Ojo de tigre", T, OFA_GEMA, OEF_JARDIN, 2, OGE_TOCAR, 3, OL_COLINA, 0, 0, 0, 0xB8862E, 0xFFD45C, "Deja ver lo que les falta a las plantas, como miraba el Cuidador: con calma.")
    /* ---- 2. Flora mística ---- */
    O("Semilla ancestral", F, OFA_SEMILLA, OEF_JARDIN, 1, OGE_MANTENER, 3, OL_BOSQUE, 0, 0, 0, 0x8A5428, 0xB48CFF, "Hermana de la primera semilla que plantaron Musguito y el Cuidador.")
    O("Hongo luminoso", F, OFA_HONGO, OEF_CIELO, 6, OGE_TOCAR, 2, OL_BOSQUE | OL_CUEVA, 1, 0, 0, 0xFF8FE0, 0x7AF0FF, "Brilla rosa en la oscuridad, como los hongos del anillo de las hadas.")
    O("Diente de león gigante", F, OFA_FLOR, OEF_CLIMA, 0, OGE_SOPLAR, 1, OL_PRADO | OL_COLINA, 0, 0, 0, 0xF4F2FA, 0x7FB86A, "Sóplalo: lo que se suelta con amor vuelve distinto. Sus semillas se llevan la neblina.")
    O("Baya de luna", F, OFA_BAYA, OEF_COMER, 4, OGE_TOCAR, 2, 0, 1, 0, 0, 0xD8D8F0, 0xFFFFFF, "Plateada, brilla de noche. Lumbre dice que es la favorita de la luna.")
    O("Raíz caminante", F, OFA_PLANTA, OEF_MIRAR, 0, OGE_TOCAR, 2, OL_BOSQUE, 0, 0, 0, 0x8A6A4A, 0x5DB27A, "Da pasitos dentro de la bolsa. Quiere volver al bosque, pero no sabe por dónde.")
    O("Loto estelar", F, OFA_FLOR, OEF_CIELO, 7, OGE_TOCAR, 3, OL_LAGO, 0, 0, 0, 0xFFC8E8, 0xFFE27A, "Solo florece sobre la laguna, donde el Cuidador dibujó estrellas con el dedo.")
    O("Trébol de cuatro colores", F, OFA_PLANTA, OEF_SUERTE, 0, OGE_TOCAR, 3, OL_PRADO, 0, 0, 0, 0x5DB27A, 0xFF8FC8, "Cuatro hojas de cuatro colores. Tilde lleva años buscando uno. No le digas que lo tienes.")
    O("Musgo de terciopelo", F, OFA_PLANTA, OEF_SUENO, 0, OGE_TOCAR, 1, OL_BOSQUE, 0, 0, 0, 0x4FA86A, 0x8AE08E, "Una alfombrita suave. Así era la camita de musgo de la casita escondida.")
    O("Galleta quemada de Peumo", F, OFA_DISCO, OEF_COMER, 2, OGE_TOCAR, 2, OL_PRADO | OL_BOSQUE, 0, 0, 0, 0x5A3A22, 0x2A1A10, "Negra como el carbón. «Así venían», decía el Cuidador. Tiene cariño adentro, y humo.")
    O("Hojas de arce dorado", F, OFA_HOJA, OEF_DECORAR, 0, OGE_TOCAR, 1, OL_BOSQUE, 0, 0, 0, 0xFFC14D, 0xE86A3A, "Tintinean al caer. El Cuidador las juntaba para hacer coronas.")
    O("Orquídea susurrante", F, OFA_FLOR, OEF_CALMA, 0, OGE_MANTENER, 3, OL_BOSQUE, 0, 0, 0, 0xB48CFF, 0xFFE0FF, "Susurra la música del bosque al revés. Cristalino dice que así suena un verso sin terminar.")
    O("Savia de árbol sagrado", F, OFA_FRASCO, OEF_JARDIN, 2, OGE_TOCAR, 2, OL_BOSQUE, 0, 0, 0, 0xF2B33D, 0xFFE9A0, "Pegamento del árbol viejo: arregla lo roto. El árbol del hueco la regala a quien cuida.")
    O("Semilla helicóptero", F, OFA_SEMILLA, OEF_JARDIN, 1, OGE_TOCAR, 1, OL_BOSQUE | OL_COLINA, 0, 0, 0, 0xC89A5A, 0x8A6A3A, "Cae girando, como las cometas que bajan a descansar. Siempre aterriza en el centro.")
    O("Hongo sombrero de bruja", F, OFA_HONGO, OEF_CABEZA, 0, OGE_TOCAR, 2, OL_BOSQUE, 0, 0, 0, 0x7A4AB8, 0xF0E0FF, "Morado y puntiagudo. Las hadas dicen que al Cuidador le quedaba enorme.")
    O("Helecho de sombras", F, OFA_PLANTA, OEF_DECORAR, 0, OGE_TOCAR, 1, OL_BOSQUE, 0, 0, 0, 0x2E5A3E, 0x5DB27A, "Crece detrás de los árboles grandes, como el sendero que se mueve.")
    O("Baya invernal", F, OFA_BAYA, OEF_COMER, 1, OGE_TOCAR, 2, 0, 0, 0, 0, 0x8AC8FF, 0xFFFFFF, "Al comerla estornuda copitos. Marea dice que en la isla nieva así.")
    O("Corteza de hierro", F, OFA_PALO, OEF_PROTEGE, 0, OGE_TOCAR, 2, OL_BOSQUE, 0, 0, 0, 0x5A4A3A, 0x9A9AA8, "Madera durísima. El zorro dice que ya no roba, pero por si acaso.")
    O("Flor de niebla", F, OFA_FLOR, OEF_CLIMA, 1, OGE_TOCAR, 2, OL_LAGO, 0, 0, 0, 0xE8E8F0, 0xB8C8D8, "Suelta vapor blanco. Ondina la usa cuando quiere llorar sin que nadie la vea.")
    O("Enredadera elástica", F, OFA_PLANTA, OEF_ENERGIA, 0, OGE_DESLIZAR, 1, OL_BOSQUE, 0, 0, 0, 0x4FB86A, 0x2E8A4A, "Se estira y rebota. Anemoi dice que así se aprende a volar: rebotando.")
    O("Rosa de cristal", F, OFA_FLOR, OEF_DECORAR, 0, OGE_TOCAR, 3, OL_PRADO, 0, 0, 0, 0xFFC8E0, 0xFFFFFF, "Frágil y transparente. Cristalino la afinó en do: suena como campanita.")
    O("Semilla de calabaza linterna", F, OFA_SEMILLA, OEF_DECORAR, 0, OGE_TOCAR, 2, OL_PRADO, 0, 0, 0, 0xF2A33A, 0xFFE27A, "Crece una calabaza que se ilumina sola, para que nadie camine a oscuras.")
    O("Bulbo parpadeante", F, OFA_PLANTA, OEF_DECORAR, 0, OGE_TOCAR, 2, OL_LAGO, 1, 0, 0, 0x9CF0FF, 0x5DB27A, "Se prende y se apaga como el faro de Marea, en chiquitito.")
    O("Hierba gatera del bosque", F, OFA_PLANTA, OEF_CARINO, 0, OGE_TOCAR, 1, OL_BOSQUE, 0, 0, 0, 0x7FD38A, 0xB48CFF, "La pone mimosa. Tilde la usa para que la perdonen.")
    O("Brote de bambú veloz", F, OFA_PLANTA, OEF_JARDIN, 1, OGE_TOCAR, 2, OL_LAGO, 0, 0, 0, 0x8AE05A, 0x4FA84A, "Crece un poquito cada vez que lo miras. Todo empieza chiquito.")
    O("Pétalo de arcoíris", F, OFA_FLOR, OEF_COMER, 3, OGE_TOCAR, 3, OL_PRADO, 0, 0, 0, 0xFF6B6B, 0x6FB7FF, "Del primer arcoíris, cuando el sol y la lluvia hicieron las paces.")
    O("Cardo de espinas azules", F, OFA_FLOR, OEF_PROTEGE, 0, OGE_TOCAR, 1, OL_COLINA, 0, 0, 0, 0x5A7FD8, 0x7FB86A, "Cuida el jardín con sus espinas. Musguito le tiene un poquito de miedo.")
    O("Nuez de roble gigante", F, OFA_SEMILLA, OEF_ENERGIA, 0, OGE_TOCAR, 1, OL_BOSQUE, 0, 0, 0, 0x8A5A2A, 0xC89A5A, "Tan dura que sirve de pelota. El Cuidador la usaba para jugar con el conejo, y perdía.")
    O("Flor del sueño", F, OFA_FLOR, OEF_SUENO, 0, OGE_TOCAR, 2, OL_PRADO, 1, 0, 0, 0x9C7CFF, 0xFFE27A, "Su olor da tanto sueño que hasta el pino más viejo del bosque se duerme más rápido.")
    O("Liquen eléctrico", F, OFA_PLANTA, OEF_CLIMA, 5, OGE_TOCAR, 2, OL_COLINA, 0, 0, 0, 0x7FE7FF, 0x5A6A7A, "Parpadea con descargas cian. Las tormentas que cantan le enseñaron.")
    O("Fruta del tiempo", F, OFA_BAYA, OEF_COMER, 4, OGE_TOCAR, 3, 0, 0, 0, 0, 0xFFD45C, 0x9C7CFF, "La deja hiperactiva. Dicen que el Cuidador nunca la probó: ya corría bastante.")
    /* ---- 3. Restos de animales y espíritus ---- */
    O("Pluma de arrendajo", E, OFA_PLUMA, OEF_MIRAR, 0, OGE_TOCAR, 1, OL_BOSQUE, 0, 0, 0, 0x4A8AE8, 0xFFFFFF, "Azul brillante. Cae en zigzag, como Anemoi cuando todavía no sabía volar.")
    O("Escama de pez astral", E, OFA_ESCAMA, OEF_PESCA, 0, OGE_TOCAR, 2, OL_LAGO, 1, 0, 0, 0x5E8CFF, 0xFFE27A, "Destella como las estrellas de la laguna. El pez dorado la reconoce.")
    O("Huevo misterioso", E, OFA_HUEVO, OEF_CARINO, 0, OGE_FROTAR, 3, OL_BOSQUE, 0, 0, 0, 0xE8E0C8, 0x7FB86A, "Late despacito. Algo tibio y redondo… ¿será como el que vio la Abuela Brizna?")
    O("Fuego fatuo embotellado", E, OFA_FRASCO, OEF_COMPA, 0, OGE_TOCAR, 2, OL_BOSQUE | OL_LAGO, 1, 0, 0, 0x8A9AA8, 0x5ED0FF, "Al destapar el frasco sale un fuego fatuo. Lumbre le enseñó a seguir a quien se pierde.")
    O("Concha que repite", E, OFA_CONCHA, OEF_DECORAR, 0, OGE_TOCAR, 2, OL_LAGO, 0, 0, 0, 0xC89A5A, 0x9C7CFF, "Si la pones en tu oreja, repite con una vocecita: «ya va a llegar». Marea la cuidaba.")
    O("Pluma de chuncho sabio", E, OFA_PLUMA, OEF_MIRAR, 0, OGE_TOCAR, 2, OL_BOSQUE, 1, 0, 0, 0x8A6A4A, 0xF0E0C8, "Del chuncho sabio: ayuda a leer lo que está escrito en los árboles viejos.")
    O("Colmillo de zorro guía", E, OFA_HUESO, OEF_SUERTE, 0, OGE_TOCAR, 2, OL_PRADO, 0, 0, 0, 0xF6F2EA, 0xF2A33A, "El zorro la dejó de regalo el día que dejó de robar. Trae suerte.")
    O("Seda de araña tejedora", E, OFA_HILO, OEF_MIRAR, 0, OGE_TOCAR, 2, OL_BOSQUE, 0, 0, 0, 0xF0F0FF, 0xB8C8E8, "Hilo que brilla. Si todavía no tiene bufanda, alguien le teje una con él.")
    O("Cuernito de pudú de luz", E, OFA_HUESO, OEF_CIELO, 6, OGE_TOCAR, 3, OL_COLINA, 0, 0, 0, 0xFFF6D0, 0xFFFFFF, "Del pudú blanco, que mira la cima del cerro esperando a alguien. Brilla como faro.")
    O("Escama de serpiente esmeralda", E, OFA_ESCAMA, OEF_PROTEGE, 0, OGE_TOCAR, 2, OL_LAGO, 0, 0, 0, 0x2E9E5E, 0x9CE8A8, "De la culebrita de la laguna. Repele el barro, como los nenúfares de las ranas.")
    O("Huella dorada", E, OFA_POLVO, OEF_SUERTE, 0, OGE_TOCAR, 3, 0, 0, 0, 0, 0xFFD45C, 0xFFF2A0, "Flota en el aire y señala tesoros. Dicen que es una huella del Cuidador. Chiquita.")
    O("Lágrima de espíritu", E, OFA_FRASCO, OEF_JARDIN, 2, OGE_TOCAR, 3, OL_LAGO, 1, 0, 0, 0xC8E8FF, 0x9CF0FF, "Una lágrima que lloraron dos, juntos. Por eso revive lo que se secó.")
    O("Garra de oso guardián", E, OFA_HUESO, OEF_PROTEGE, 0, OGE_TOCAR, 2, OL_BOSQUE, 0, 0, 0, 0x5A4A3A, 0xF0E8D8, "Del oso dormilón. Con ella da un rugidito que espanta a cualquiera.")
    O("Capullo de mariposa prisma", E, OFA_HUEVO, OEF_CIELO, 2, OGE_TOCAR, 2, OL_PRADO, 0, 0, 0, 0xB48CFF, 0x7AF0C8, "Cambia de color según cómo lo mires, como el Cuidador según quién lo recuerde.")
    O("Pluma de fénix del bosque", E, OFA_PLUMA, OEF_CLIMA, 0, OGE_TOCAR, 3, OL_COLINA, 0, 0, 0, 0xFF7A3A, 0xFFE27A, "Tibia y naranja. El fénix dice que el sol se la prestó al Cuidador una vez.")
    O("Trocito del caparazón de Brizna", E, OFA_ESCAMA, OEF_PROTEGE, 0, OGE_TOCAR, 2, OL_LAGO, 0, 0, 0, 0x6E9A58, 0xC8B078, "Se le cayó a la Abuela Brizna al bostezar. Protege como abuela.")
    O("Aguijón de avispa mística", E, OFA_HUESO, OEF_MIRAR, 0, OGE_TOCAR, 2, OL_PRADO, 0, 0, 0, 0x2A2238, 0xFFD45C, "Sirve para tallar runas. Así marcaba el Cuidador los lugares que ya había cuidado.")
    O("Hueso de dinosaurio pixel", E, OFA_HUESO, OEF_ENERGIA, 0, OGE_TOCAR, 3, OL_COLINA, 0, 0, 0, 0xF6EEDC, 0xC8B89A, "Enorme. Tilde intentó llevárselo tres veces. Pesa demasiado.")
    O("Canto de sirena", E, OFA_ORBE, OEF_CALMA, 0, OGE_TOCAR, 3, OL_LAGO, 0, 0, 0, 0x7FC8F0, 0xFFFFFF, "Una burbuja con el sonido del agua. Marea la canta para que los barquitos no se pierdan.")
    O("Espora de hada voladora", E, OFA_POLVO, OEF_COMPA, 2, OGE_TOCAR, 2, OL_BOSQUE, 0, 0, 0, 0xFF9FC8, 0xFFFFFF, "Revolotea alrededor de su cabeza, como las hadas alrededor de la cuchara perdida.")
    O("Huevo de tordo nocturno", E, OFA_HUEVO, OEF_CABEZA, 0, OGE_TOCAR, 3, OL_BOSQUE, 1, 0, 0, 0x3A3A5A, 0x9C7CFF, "De noche se abre. El tordo guarda ahí las cosas elegantes que Tilde no encuentra.")
    O("Escama de dragón de agua", E, OFA_ESCAMA, OEF_PESCA, 0, OGE_TOCAR, 3, OL_LAGO, 0, 0, 0, 0x2E7AB8, 0x7FE7FF, "De un dragón de agua, primo de Prisma y Anemoi. La vuelve nadadora experta.")
    O("Sombra errante", E, OFA_ORBE, OEF_COMPA, 3, OGE_TOCAR, 3, OL_CUEVA, 1, 0, 0, 0x2A2238, 0xFFFFFF, "Una mancha con ojitos que imita todo. Dicen que era la sombra del Cuidador. Chiquita.")
    O("Cola de lagartija neón", E, OFA_HUESO, OEF_MIRAR, 0, OGE_TOCAR, 2, OL_COLINA, 0, 0, 0, 0x7FFF7A, 0x2E9E5E, "Si se cae, vuelve a crecer. Lo que se suelta con amor vuelve distinto.")
    O("Pluma de cisne de cuello negro", E, OFA_PLUMA, OEF_CABEZA, 0, OGE_TOCAR, 2, OL_LAGO, 0, 0, 0, 0xE8ECF8, 0xB8C0D8, "Con ella en la cabeza camina como bailarina. Las hadas la envidian.")
    O("Almeja perlada", E, OFA_CONCHA, OEF_MIRAR, 0, OGE_MANTENER, 2, OL_LAGO, 0, 0, 0, 0xE8D8F0, 0xFFFFFF, "Mantén el dedo y se abre: hay una perla. Ondina dice que la lloró ella. Exagera.")
    O("Pelo de lobo lunar", E, OFA_HILO, OEF_CIELO, 7, OGE_TOCAR, 3, OL_COLINA, 1, 0, 0, 0xD8E0F0, 0xFFFFFF, "Hebras plateadas que brillan con la luna llena, la noche del rito.")
    O("Duende travieso", E, OFA_ORBE, OEF_ENERGIA, 0, OGE_TOCAR, 2, OL_BOSQUE, 0, 0, 0, 0x7FFF7A, 0x2E9E5E, "Un puntito verde hiperactivo. Primo de Musguito. Muy, muy primo.")
    O("Diente de tiburón de laguna", E, OFA_HUESO, OEF_PESCA, 0, OGE_TOCAR, 2, OL_LAGO, 0, 0, 0, 0xF6F2EA, 0x8AB8D8, "Amuleto del pescador. El pez dorado no le tiene miedo: sabe que siempre lo sueltan.")
    O("Polvo de polilla lunar", E, OFA_POLVO, OEF_CIELO, 2, OGE_DESLIZAR, 2, 0, 1, 0, 0, 0xD8D8F0, 0xFFF2A0, "Desliza el dedo y deja luz. Las polillas bailan con la luna, como el Cuidador con las hadas.")
    /* ---- 4. Objetos perdidos y antigüedades ---- */
    O("Bota vieja", A, OFA_BOTA, OEF_DECORAR, 1, OGE_FROTAR, 1, OL_LAGO, 0, 0, 0, 0x6A5A3A, 0x3A2A1A, "Basura de la laguna. Frótala con cariño: limpia, las hadas la vuelven su casita en el prado.")
    O("Reloj de bolsillo parado", A, OFA_DISCO, OEF_SUENO, 0, OGE_TOCAR, 2, OL_PRADO, 0, 0, 0, 0xFFD45C, 0xF6F2EA, "Se paró la noche del último paseo. Al tocarlo, todo se queda quietito.")
    O("Llave oxidada", A, OFA_LLAVE, OEF_ESPECIAL, OBJX_LLAVE, OGE_TOCAR, 2, OL_PRADO | OL_LAGO, 0, 0, 0, 0xA8744A, 0x6A4A2A, "Abre algo que todavía no encuentras. Quizás bajo el árbol más viejo, con su candado.")
    O("Brújula loca", A, OFA_DISCO, OEF_ESPECIAL, OBJX_BRUJULA, OGE_TOCAR, 2, OL_COLINA, 0, 0, 0, 0xC8B89A, 0xE0485E, "La aguja siempre apunta hacia ella. Tilde dice que es porque ella brilla.")
    O("Tintero del escritor", A, OFA_FRASCO, OEF_ESPECIAL, OBJX_TINTERO, OGE_TOCAR, 2, OL_PRADO, 0, 0, 0, 0x2A2A5A, 0x5A5AA8, "La tinta del diario del Rey. Deja ver notas escondidas en su diario.")
    O("Moneda gastada", A, OFA_DISCO, OEF_MIRAR, 0, OGE_TOCAR, 1, OL_LAGO, 0, 0, 0, 0xE0B040, 0xA8803A, "Del reino del Rey, que nunca cobró nada. Tiene grabada una galleta. Quemada.")
    O("Farol de cobre viejo", A, OFA_LAMPARA, OEF_DECORAR, 0, OGE_TOCAR, 2, OL_PRADO, 0, 0, 0, 0xC87A3A, 0x5ED0FF, "Se enciende con un fuego fatuo. Así alumbraba el Cuidador el sendero que se mueve.")
    O("Herradura de la suerte", A, OFA_LLAVE, OEF_JARDIN, 0, OGE_TOCAR, 2, OL_PRADO, 0, 0, 0, 0x8A8A9A, 0x5A5A6A, "Cuida el jardín: con ella el Cuidador regaba todo en una sola vuelta.")
    O("Taza de porcelana rota", A, OFA_TAZA, OEF_JARDIN, 0, OGE_TOCAR, 1, OL_LAGO, 0, 0, 0, 0xF6F2EA, 0x6FB7FF, "Rota, pero sirve para traer agua. De la casita escondida: era la taza de Ondina.")
    O("Gafas del explorador", A, OFA_DISCO, OEF_CABEZA, 0, OGE_TOCAR, 2, OL_COLINA, 0, 0, 0, 0x6A4A2A, 0xBFE6FF, "Con ellas ve nidos escondidos en los árboles. Tilde tiene tres nidos. Uno es de cosas robadas.")
    O("Catalejo de latón", A, OFA_PALO, OEF_SUERTE, 0, OGE_TOCAR, 2, OL_COLINA, 0, 0, 0, 0xD8A83A, 0x6A4A2A, "Ve lejísimos. Desde el mirador se ve todo lo que el Cuidador cuidó.")
    O("Caja de música antigua", A, OFA_CAJA, OEF_CALMA, 0, OGE_TOCAR, 3, OL_PRADO, 0, 0, 0, 0x8A5A3A, 0xFFD45C, "Toca una melodía chiquita: el principio de la canción del Cuidador.")
    O("Corona de hojas", A, OFA_HOJA, OEF_CABEZA, 0, OGE_TOCAR, 2, OL_PRADO, 0, 0, 0, 0x8ACB5A, 0xD8A040, "Hojas secas trenzadas, chiquitas. Al Cuidador se le caía cada vez que se reía.")
    O("Frasco de perfume vacío", A, OFA_FRASCO, OEF_CALMA, 0, OGE_TOCAR, 2, OL_PRADO, 0, 0, 0, 0xFFC8E0, 0xB48CFF, "Guarda el olor de las flores mágicas. Doña Pétalo lo recomienda.")
    O("Dedal de plata", A, OFA_TAZA, OEF_MIRAR, 0, OGE_TOCAR, 1, OL_PRADO, 0, 0, 0, 0xC8C8D8, 0x8A8A9A, "Protege el dedo de las espinas. Del tamaño de un dedo… o de una mano chiquita.")
    O("Anillo de sello real", A, OFA_DISCO, OEF_MIRAR, 0, OGE_TOCAR, 3, OL_LAGO, 0, 0, 0, 0xFFD45C, 0xE0485E, "Tiene el sello del Rey: una cuchara. Nadie sabe por qué. (Ahora tú sí.)")
    O("Gato de la suerte", A, OFA_CAJA, OEF_SUERTE, 0, OGE_TOCAR, 2, OL_PRADO, 0, 0, 0, 0xF6EEDC, 0xE0485E, "Una figurita de madera que mueve la pata cuando algo bueno pasa.")
    O("Pipa de burbujas del druida", A, OFA_PALO, OEF_ESPECIAL, OBJX_PIPA, OGE_SOPLAR, 2, OL_BOSQUE, 0, 0, 0, 0x8A5A3A, 0xBFE6FF, "Hace burbujas cuadradas. El Cuidador las inventó para que el pino se riera.")
    O("Cuchara de palo", A, OFA_PALO, OEF_PESCA, 0, OGE_TOCAR, 1, OL_LAGO, 0, 0, 0, 0xC8955A, 0x8A5A30, "Como la del Cuidador: sirve para todo. Plantar, revolver, desenterrar lombrices.")
    O("Espejo de mano roto", A, OFA_DISCO, OEF_CARINO, 0, OGE_TOCAR, 2, OL_PRADO, 0, 0, 0, 0xBFE6FF, 0x8A5A3A, "Al mirarse, se ve como cuando era bebé. Ojalá supiera quién la cuidó desde antes.")
    O("Página suelta del diario", A, OFA_LIBRO, OEF_MIRAR, 0, OGE_TOCAR, 2, OL_PRADO, 0, 0, 0, 0xF4E8C8, 0x8A6A4A, "Una hoja del diario del Rey: una lista de cosas por cuidar.")
    O("Candado rúnico", A, OFA_LLAVE, OEF_ESPECIAL, OBJX_CANDADO, OGE_TOCAR, 2, OL_CUEVA, 0, 0, 0, 0x8A8A9A, 0x9C7CFF, "Cuida lo más valioso de la bolsa, como Marea cuida el faro.")
    O("Placa de reloj perdida", A, OFA_DISCO, OEF_MIRAR, 0, OGE_TOCAR, 3, OL_LAGO, 0, 0, 0, 0x2A2238, 0x7FE7FF, "Una pantallita redonda que muestra la hora. Le parece conocida.")
    O("Peine de carey", A, OFA_PALO, OEF_CARINO, 0, OGE_FROTAR, 2, OL_PRADO, 0, 0, 0, 0xA8643A, 0xF2B33D, "Para peinarla. Ondina peina a los peces con uno igual.")
    O("Botella con mensaje", A, OFA_FRASCO, OEF_MIRAR, 0, OGE_TOCAR, 2, OL_LAGO, 0, 0, 0, 0x7FC8A8, 0xF6F2EA, "Una carta de otra mascota que vive lejos. También la cuidan entre varios.")
    O("Broche que Tilde devolvió", A, OFA_AMULETO, OEF_CABEZA, 0, OGE_TOCAR, 2, OL_BOSQUE, 0, 0, 0, 0xC8C8D8, 0x5DB27A, "Tilde lo robó tres veces y lo devolvió tres veces. Ahora brilla de vergüenza.")
    O("Dado de hueso", A, OFA_CAJA, OEF_SUERTE, 0, OGE_TOCAR, 2, OL_COLINA, 0, 0, 0, 0xF6EEDC, 0x2A2238, "Tíralo: si sale seis, el día viene con suerte. Tilde lo tira hasta que sale seis.")
    O("Pluma de escribano real", A, OFA_PLUMA, OEF_MIRAR, 0, OGE_TOCAR, 3, OL_PRADO, 0, 0, 0, 0xF6F2EA, 0x2A2A5A, "Escribe nombres con letras elegantes. Le falta escribir uno muy corto, como de árbol.")
    O("Llave de cuerda", A, OFA_LLAVE, OEF_ESPECIAL, OBJX_CUERDA, OGE_FROTAR, 2, OL_PRADO, 0, 0, 0, 0xC8C8D8, 0xE0485E, "Dale cuerda y camina como robot. Prisma dice que así caminaba él recién despierto.")
    O("Medallón del eclipse", A, OFA_AMULETO, OEF_CIELO, 1, OGE_TOCAR, 3, OL_COLINA, 0, 0, 0, 0x2A2238, 0xFFD45C, "Tapa el sol un ratito. Lumbre dice que en la oscuridad se ve mejor la luz chiquita.")
    /* ---- 5. Artefactos y creaciones místicas ---- */
    O("Báculo rúnico de viento", R, OFA_PALO, OEF_CLIMA, 0, OGE_DESLIZAR, 3, OL_COLINA, 0, 0, 0, 0x8A6A4A, 0x7AF0C8, "Desliza el dedo rápido: una ráfaga, como las que sopla Anemoi.")
    O("Pócima de la alegría", R, OFA_POCION, OEF_CALMA, 0, OGE_TOCAR, 2, 0, 0, 0, 0, 0xFF6BB8, 0xFFE0F0, "Rosada y burbujeante. Ondina la toma después de llorar mucho.")
    O("Runa de teletransporte", R, OFA_RUNA, OEF_ESPECIAL, OBJX_RUNA_TP, OGE_TOCAR, 3, OL_CUEVA, 0, 0, 0, 0x6A6A8A, 0x7AF0FF, "Lleva de una zona a otra en un parpadeo, como el sendero que se mueve.")
    O("Lentes reveladores", R, OFA_DISCO, OEF_ESPECIAL, OBJX_LENTES, OGE_TOCAR, 3, OL_BOSQUE, 0, 0, 0, 0x9C7CFF, 0x7AF0FF, "Muestran el bosque mágico. Con ellos se ven los secretos que dejó el Cuidador.")
    O("Atrapasueños", R, OFA_AMULETO, OEF_SUENO, 0, OGE_TOCAR, 2, OL_BOSQUE, 1, 0, 0, 0xC89A5A, 0xB48CFF, "Atrapa las pesadillas. Nimba duerme con uno enredado en la cola.")
    O("Elixir del crecimiento", R, OFA_POCION, OEF_ESPECIAL, OBJX_CRECE, OGE_TOCAR, 3, 0, 0, 0, 0, 0x5DB27A, 0xFFE27A, "Un trago y se siente grande. El Cuidador nunca lo tomó: le gustaba su tamaño.")
    O("Runa de fuego fatuo", R, OFA_RUNA, OEF_COMPA, 0, OGE_TOCAR, 3, OL_CUEVA, 1, 0, 0, 0x5A5A7A, 0x5ED0FF, "Llama a un fuego fatuo que la cuida, para que nadie camine a oscuras.")
    O("Esfera del clima", R, OFA_ORBE, OEF_CLIMA, 2, OGE_TOCAR, 3, 0, 0, 0, 0, 0xBFE6FF, 0x6FB7FF, "Adentro llueve. Ondina la llenó con una lágrima de nube.")
    O("Cetro del rey del bosque", R, OFA_PALO, OEF_CIELO, 6, OGE_TOCAR, 3, OL_BOSQUE, 0, 0, 0, 0xE0B040, 0x5DB27A, "Le decían cetro, pero era la batuta de su orquesta de grillos. Los bichitos vienen a bailar.")
    O("Pócima de invisibilidad", R, OFA_POCION, OEF_ESPECIAL, OBJX_INVISIBLE, OGE_TOCAR, 3, OL_CUEVA, 0, 0, 0, 0xE8F0FF, 0xB8C8E8, "Casi transparente. Tilde la quiere para robar sin que la vean. Nadie se la presta.")
    O("Amuleto del tiempo", R, OFA_AMULETO, OEF_CIELO, 3, OGE_TOCAR, 3, OL_COLINA, 0, 0, 0, 0xFFD45C, 0x5A5AA8, "El cielo pasa rapidísimo, como pasan los años para la Abuela Brizna.")
    O("Campana de purificación", R, OFA_LAMPARA, OEF_CURAR, 0, OGE_TOCAR, 3, OL_BOSQUE, 0, 0, 0, 0xFFE27A, 0xFFFFFF, "Su sonido limpia todo, hasta los resfríos. El Cuidador la tocaba para los enfermos.")
    O("Espejo dimensional", R, OFA_DISCO, OEF_ESPECIAL, OBJX_ESPEJO, OGE_TOCAR, 3, OL_CUEVA, 0, 0, 0, 0x9C7CFF, 0x2A2238, "Un portalito: lo que se encuentra hoy, se encuentra doble.")
    O("Incienso del druida", R, OFA_FRASCO, OEF_CALMA, 0, OGE_TOCAR, 2, OL_BOSQUE, 0, 0, 0, 0x9C7CFF, 0xD8C8FF, "Humo morado. Lumbre medita con él en las noches sin luna.")
    O("Pócima de encogimiento", R, OFA_POCION, OEF_ESPECIAL, OBJX_ENCOGE, OGE_TOCAR, 3, 0, 0, 0, 0, 0x7AF0C8, 0x2E9E8E, "La deja chiquitita y curiosa, del tamaño exacto de alguien que conocemos.")
    O("Runa de cosecha", R, OFA_RUNA, OEF_JARDIN, 1, OGE_TOCAR, 3, OL_PRADO, 0, 0, 0, 0x7A6A5A, 0xFFD45C, "Todas las plantas maduran de golpe, como el bosque que apareció mientras la Abuela dormía.")
    O("Imán místico", R, OFA_LLAVE, OEF_ESPECIAL, OBJX_IMAN, OGE_TOCAR, 3, OL_CUEVA, 0, 0, 0, 0xE0485E, 0xC8C8D8, "Atrae las cosas perdidas. Tilde lo esconde cada vez que lo ve.")
    O("Cápsula de calma", R, OFA_ORBE, OEF_SUENO, 0, OGE_TOCAR, 3, 0, 0, 0, 0, 0xBFE6FF, 0x9C7CFF, "Para los días largos. Prisma dice que cien años de sueño es demasiado; esto es justo.")
    O("Flauta de los espíritus", R, OFA_PALO, OEF_CARINO, 0, OGE_SOPLAR, 3, OL_BOSQUE, 0, 0, 0, 0xC89A5A, 0x7AF0C8, "Toca una melodía que solo ustedes dos entienden. Las canciones son memoria.")
    O("Esencia de aurora", R, OFA_FRASCO, OEF_CIELO, 0, OGE_TOCAR, 3, OL_COLINA, 1, 0, 0, 0x7AF0C8, 0x9C7CFF, "Pinta el cielo de verde y violeta, como la noche en que Prisma despertó.")
    O("Pócima de sol", R, OFA_POCION, OEF_CLIMA, 0, OGE_TOCAR, 2, 0, 0, 0, 0, 0xFFD45C, 0xFFF6C8, "Tan brillante que despeja el cielo. El cariño es luz.")
    O("Runa del escudo", R, OFA_RUNA, OEF_PROTEGE, 0, OGE_TOCAR, 3, OL_COLINA, 0, 0, 0, 0x6A6A7A, 0x6FB7FF, "Hoy nada malo le pasa a su comida. Marea talló una igual para el faro.")
    O("Talismán del pescador", R, OFA_AMULETO, OEF_PESCA, 0, OGE_TOCAR, 3, OL_LAGO, 0, 0, 0, 0x2E7AB8, 0xFFD45C, "La caña se vuelve mágica. El pez dorado ya sabe y viene a saludar.")
    O("Polvo de hadas", R, OFA_POLVO, OEF_CIELO, 5, OGE_DESLIZAR, 2, OL_BOSQUE, 0, 0, 0, 0xFFB6F0, 0xFFFFFF, "Espárcelo con el dedo. Las hadas lo dejan donde bailaron.")
    O("Bebida helada del norte", R, OFA_POCION, OEF_CLIMA, 4, OGE_TOCAR, 3, OL_LAGO, 0, 0, 0, 0xBFE6FF, 0xFFFFFF, "Hace nevar. Marea dice que la laguna congelada también se puede cruzar, despacito.")
    O("Runa del despertar", R, OFA_RUNA, OEF_JARDIN, 1, OGE_TOCAR, 3, OL_BOSQUE, 0, 0, 0, 0x5A6A5A, 0x7FFF7A, "Las plantas despiertan con todo su poder, como el mundo cuando alguien lo cuida.")
    O("Tónico de energía", R, OFA_POCION, OEF_ENERGIA, 0, OGE_TOCAR, 2, 0, 0, 0, 0, 0xFF7A3A, 0xFFD45C, "Se le quita el sueño. Prisma pidió uno después de cien años. No funcionó.")
    O("Varita de destellos", R, OFA_PALO, OEF_CIELO, 4, OGE_TOCAR, 3, OL_PRADO, 0, 0, 0, 0x9C7CFF, 0xFFE27A, "Fuegos de colores. Para las fiestas que el Cuidador nunca se tomó.")
    O("Pócima de colores", R, OFA_POCION, OEF_ESPECIAL, OBJX_COLORES, OGE_TOCAR, 3, 0, 0, 0, 0, 0xFF6B6B, 0x7FD38A, "Le cambia los colores un rato. Como cada criatura recuerda al Cuidador de otro color.")
    O("Corazón del Mundo", R, OFA_ORBE, OEF_MIRAR, 0, OGE_MANTENER, 0, 0, 0, 0, 0, 0x7FFF7A, 0xFFD45C, "Medio corazón que late. Se siente como si buscara a alguien.")
};

/* lo que dice ella al usar cada objeto (versión 37): dice lo que hace de verdad */
static const char *const AL_USAR[PET_OBJETOS] = {
    "¡ME LLENA EL CIELO DE CONSTELACIONES!",
    "¡SE ABRE Y SALEN CHISPAS DE AMATISTA!",
    "¡ME DAN GANAS DE SALTAR Y JUGAR!",
    "¡UNA LUCIÉRNAGA DORADA ME SIGUE!",
    "¡SE TRAGA LAS NUBES! CIELO DESPEJADO",
    "CON ESTO SE ME PASAN TODAS LAS PENAS",
    "ES DE COLECCIÓN: LA GUARDO CON CARIÑO",
    "ES DE COLECCIÓN: HUELE A BOSQUE ANTIGUO",
    "¡AHORA CAEN ESTRELLAS FUGACES!",
    "ES DE COLECCIÓN: SE MUEVE SOLITO",
    "LO PONGO EN EL PRADO: ¡CANTA AL PASAR!",
    "ME ABRIGA Y ME CALMA EL CORAZÓN",
    "¡SE ME QUITA TODO EL CANSANCIO!",
    "ES DE COLECCIÓN: REFLEJA EL CIELO",
    "¡TODO SE LLENA DE DESTELLOS!",
    "¡LLAMO UNA TORMENTA (SIN PELIGRO)!",
    "ME DAN GANAS DE BUSCAR TUS CARICIAS",
    "¡QUIERO JUGAR CONTIGO!",
    "¡SALE UN FUEGO FATUO QUE SIGUE TU DEDO!",
    "¡HOY VOY A ENCONTRAR MÁS COSAS!",
    "¡SE SECA LA LLUVIA Y SALE EL SOL!",
    "¡SALEN CHISPAS DE COLORES!",
    "¡TODO DESTELLA UN RATO!",
    "LA PONGO EN EL PRADO: BRILLA DE NOCHE",
    "¡EL JARDÍN CRECE DE GOLPE!",
    "HOY LAS ABEJAS NO ME MOLESTAN",
    "ES DE COLECCIÓN: SE CAMBIA DE LUGAR",
    "SOSTENERLO ME DA UNA CALMA PROFUNDA",
    "¡TRAIGO EL FRÍO!",
    "¡REVIVO LAS PLANTAS SECAS!",
    "¡TODO EL JARDÍN CRECE DE GOLPE!",
    "¡LLAMO A LAS LUCIÉRNAGAS!",
    "¡SOPLO Y SE VA LA NEBLINA!",
    "¡ME LA COMO Y QUEDO FELIZ DE VERDAD!",
    "ES DE COLECCIÓN: DA PASITOS",
    "¡DIBUJO CONSTELACIONES EN EL CIELO!",
    "¡HOY TENGO MUCHÍSIMA SUERTE!",
    "ME DA SUEÑITO… ME VOY A ACOSTAR",
    "¡ME LA COMO! ¡ECHO HUMITO POR LAS OREJAS!",
    "LAS PONGO EN EL PRADO: ¡TINTINEAN!",
    "SU SUSURRO ME CALMA",
    "¡REVIVO LAS PLANTAS SECAS!",
    "¡EL JARDÍN CRECE!",
    "ME LO PONGO DE GORRITO",
    "LO PONGO EN EL PRADO, DETRÁS DE UN ÁRBOL",
    "¡ME LA COMO Y ESTORNUDO COPITOS!",
    "HOY EL ZORRO NO SE ATREVE A ROBAR",
    "¡EL DÍA SE NUBLA!",
    "¡ME DAN GANAS DE JUGAR!",
    "LA PONGO EN EL PRADO: ¡SUENA!",
    "LA PLANTO EN EL PRADO: ¡SE ILUMINA!",
    "LO PONGO EN EL PRADO: ¡PARPADEA!",
    "ME PONGO MIMOSA: ¡CARICIAS, POR FAVOR!",
    "¡EL JARDÍN CRECE!",
    "¡ME LO COMO Y QUEDO BRILLANDO!",
    "HOY NADIE VIENE A ROBAR FRUTA",
    "¡QUIERO JUGAR A LA PELOTA!",
    "¡QUÉ SUEÑO! ME DUERMO AL TIRO",
    "¡LLAMO UNA TORMENTA!",
    "¡ME LA COMO Y ME PONGO HIPERACTIVA!",
    "ES DE COLECCIÓN: CAE EN ZIGZAG",
    "¡HOY LA PESCA ES MÁGICA!",
    "LO ACARICIO… ¡ALGO SE MUEVE ADENTRO!",
    "¡UN FUEGO FATUO SIGUE TU DEDO!",
    "LA PONGO EN EL PRADO: «YA VA A LLEGAR»",
    "ES DE COLECCIÓN: LEE LOS ÁRBOLES",
    "HOY NO ME PIERDO Y TRAIGO MENOS BASURA",
    "¡A LO MEJOR ME TEJEN UNA BUFANDA!",
    "¡LLAMO A LAS LUCIÉRNAGAS!",
    "HOY NADA ME MOLESTA",
    "¡HOY ENCUENTRO MÁS TESOROS!",
    "¡REVIVO LAS PLANTAS SECAS!",
    "¡ROAR! HOY NADIE ME MOLESTA",
    "¡SALEN CHISPAS DE COLORES!",
    "¡SE EVAPORA LA LLUVIA Y SALE EL SOL!",
    "HOY LAS ABEJAS NO ME MOLESTAN",
    "ES DE COLECCIÓN: TALLA RUNAS",
    "¡ME DAN GANAS DE MORDERLO Y JUGAR!",
    "SU SONIDO ME CALMA",
    "¡UNA ESPORA DE HADA ME SIGUE!",
    "¡ME PONGO ALGO MUY ELEGANTE!",
    "¡HOY LA PESCA ES MEJOR!",
    "¡MI SOMBRA ME IMITA EN TODO!",
    "ES DE COLECCIÓN: VUELVE A CRECER",
    "ME LA PONGO Y CAMINO COMO BAILARINA",
    "ES DE COLECCIÓN: ¡TIENE UNA PERLA!",
    "¡DIBUJO CONSTELACIONES!",
    "¡QUIERO CORRER Y JUGAR!",
    "¡HOY PICA MÁS RÁPIDO!",
    "¡DEJO UN RASTRO DE LUZ!",
    "¡LAS HADAS LA VUELVEN SU CASITA!",
    "TODO SE QUEDA QUIETITO… QUÉ SUEÑO",
    "¡A VER SI ABRE EL COFRE DEL ÁRBOL VIEJO!",
    "¡LA AGUJA ME APUNTA A MÍ!",
    "¡APARECE UNA NOTA ESCONDIDA EN MI DIARIO!",
    "ES DE COLECCIÓN: UNA GALLETA GRABADA",
    "LO PONGO EN EL PRADO: ¡ALUMBRA!",
    "¡RIEGO TODAS LAS PLANTAS!",
    "¡RIEGO TODAS LAS PLANTAS!",
    "¡ME LAS PONGO Y VEO NIDOS ESCONDIDOS!",
    "¡HOY ENCUENTRO COSAS QUE NADIE VE!",
    "SU MELODÍA ME PONE CONTENTA",
    "ME PONGO LA CORONA DE HOJAS",
    "¡SU OLOR ME CAMBIA EL ÁNIMO!",
    "ES DE COLECCIÓN: ¡ME QUEDA DE GORRO!",
    "ES DE COLECCIÓN: EL SELLO DEL REY",
    "¡HOY ES UN DÍA DE SUERTE!",
    "¡BURBUJAS CUADRADAS! TÓCALAS",
    "¡DESENTIERRO LOMBRICES: HOY PESCO MEJOR!",
    "ME VEO DE BEBÉ… ¡QUIERO MIMOS!",
    "ES DE COLECCIÓN: UNA PÁGINA DEL DIARIO",
    "¡CUIDO LO MÁS VALIOSO DE LA BOLSA!",
    "ES DE COLECCIÓN: ¡SE PARECE A MI CASA!",
    "ME PEINO: ¡QUEDO SUAVECITA!",
    "ES DE COLECCIÓN: UNA CARTA DE LEJOS",
    "ME PONGO EL BROCHE: ¡SIEMPRE LIMPIO!",
    "¡HOY EL DÍA VIENE CON SUERTE!",
    "ES DE COLECCIÓN: ESCRIBE MI NOMBRE",
    "¡CAMINO COMO ROBOT DE JUGUETE!",
    "¡UN ECLIPSE DE MENTIRA!",
    "¡UNA RÁFAGA BARRE HOJAS Y NEBLINA!",
    "¡SE ME PASA CUALQUIER PENA!",
    "¡A VIAJAR! ME VOY A OTRA ZONA",
    "¡HOY APARECEN MÁS SECRETOS!",
    "DUERMO TRANQUILA, SIN PESADILLAS",
    "¡ME SIENTO GRANDE Y FUERTE!",
    "¡UN FUEGO FATUO ME CUIDA!",
    "¡AHORA LLUEVE AFUERA TAMBIÉN!",
    "¡LOS BICHITOS VIENEN A BAILAR!",
    "¡DESAPAREZCO UN RATO!",
    "¡ESTRELLAS FUGACES EN UN RATITO!",
    "¡ME CURO DE CUALQUIER RESFRÍO!",
    "¡HOY TODO LO QUE ENCUENTRE SERÁ DOBLE!",
    "MEDITO TRANQUILA",
    "¡ME VUELVO CHIQUITITA!",
    "¡TODAS LAS PLANTAS MADURAN!",
    "¡ATRAIGO LAS COSAS PERDIDAS!",
    "DUERMO PROFUNDO Y TRANQUILA",
    "TOCO NUESTRA MELODÍA: ¡TE QUIERO!",
    "¡UNA AURORA EN EL CIELO!",
    "¡SALE EL SOL!",
    "HOY NADIE TOCA MI COMIDA",
    "¡HOY PICA A CADA RATO!",
    "¡FLOTO UN POQUITO SOBRE EL SUELO!",
    "¡HAGO NEVAR Y SE CONGELA LA LAGUNA!",
    "¡LAS PLANTAS DESPIERTAN!",
    "¡SE ME QUITA EL SUEÑO! ¡A JUGAR!",
    "¡FUEGOS ARTIFICIALES EN EL CIELO!",
    "¡ME CAMBIAN LOS COLORES!",
    "LATE… ES PARA EL RITO, EN LA ISLA",
};
/* las leyendas de los objetos de la historia según el acto (2 y 3): pistas que no revelan el huevo */
static const struct { uint8_t id; const char *acto2, *acto3; } LEYENDA_ACTO[] = {
    {108, "Como la del Cuidador: sirve para todo. ¿Por qué el gancho de la casita escondida tenía una silueta tan chiquita?", "Como la que plantó la primera semilla. Él nunca dejaba que nadie lo ayudara a cargarla."},
    {128, "Batuta de grillos. Los grillos dicen que la canción del Rey tiene un final que nadie sabe tocar.", "La batuta del Rey. Dirigió hasta que se enfermó. Los grillos siguen tocando por él."},
    {105, "El sello del Rey: una cuchara. Del tamaño justo para una mano chiquita.", "El sello del Rey. Lo usaba para firmar sus listas de cosas por cuidar. Nunca firmó una para él."},
    {95, "Del reino del Rey. La galleta grabada es del tamaño de una moneda… ¿o de una mano?", "Del reino del Rey, que lo daba todo y nunca pidió nada de vuelta."},
    {94, "La tinta del diario del Rey. Las páginas que vuelven al Libro tienen esta misma tinta.", "La tinta del diario del Rey. Escribió «mañana lo intento» cien veces."},
    {110, "La cucharita de la esquina es su firma: no quería que supieran su nombre.", "Una página del diario del Rey. Al final, la letra tiembla: ya estaba enfermo."},
    {38, "«Así venían». El monito del monte todavía encuentra galletas como esta por todo el bosque.", "La última que horneó. Más quemada que nunca: ya estaba enfermo."},
    {19, "Una escama de Prisma. Se le cayó la noche que la gota lo durmió.", "Una escama de Prisma. Brilla un poco más desde que él se perdonó."},
    {75, "De la Abuela Brizna. Dice que un siglo empolló algo chiquitito. ¿O era una piedra?", "De la Abuela Brizna. Cuidó al que cuidaba a todos, aunque estuviera dormida."},
    {102, "Hojas chiquitas, trenzadas. La encontraste en el mirador de las nubes.", "Su corona. Se la quitó antes de subir al lomo de Nimba, la última noche."},
    {64, "«Ya va a llegar», repite con la voz del Cuidador. ¿A quién esperaba?", "«Ya va a llegar». Ahora suena distinta: como si ya hubiera llegado."},
    {149, "Medio corazón que late. Cuando ella respira hondo, late más fuerte. Qué raro.", "Medio corazón. Busca a alguien. Se calma cuando ella lo sostiene."},
    {62, "Algo tibio y redondo. La Abuela Brizna vio uno igual, envuelto en hojas, la última noche.", "Algo tibio y redondo. Tú también fuiste un huevo, ¿te acuerdas?"},
};
const char *objeto_al_usar(int id) { return id >= 0 && id < PET_OBJETOS ? AL_USAR[id] : ""; }
const char *objeto_leyenda_acto(int id, int acto)
{
    const objeto_info_t *o = objeto_info(id);
    if (!o) return "";
    for (unsigned k = 0; k < sizeof LEYENDA_ACTO / sizeof LEYENDA_ACTO[0]; k++)
        if (LEYENDA_ACTO[k].id == id) return acto >= 3 ? LEYENDA_ACTO[k].acto3 : (acto == 2 ? LEYENDA_ACTO[k].acto2 : o->leyenda);
    return o->leyenda;
}
const objeto_info_t *objeto_info(int id) { return id >= 0 && id < PET_OBJETOS ? &OBJ[id] : NULL; }
const char *objeto_cat_nombre(int cat)
{
    static const char *const N[OCAT_COUNT] = {"Tesoros de la tierra", "Flora mística", "Restos de animales y espíritus", "Objetos perdidos y antigüedades", "Artefactos místicos"};
    return cat >= 0 && cat < OCAT_COUNT ? N[cat] : "?";
}
