/* Lo que dice la ballena de nubes en el paseo por el firmamento.
 * Modo infante: sin textos (solo dibujos). Modo niño: frases cortas. Modo adulto: la versión larga. */
#pragma once

typedef struct { const char *nino, *adulto; } cielo_txt_t;

/* los astros que pasan: Luna, el planeta de los anillos, el cometa, el planeta rojo, el gigante rayado y la luciérnaga dormida */
static const cielo_txt_t CIELO_ASTRO[6] = {
    {"¡Mira, la Luna! Es la lámpara de las noches.",
     "Esa es la Luna, la lámpara de todas las noches. Por la mañana la apaga con cuidado y se echa a dormir detrás de las nubes."},
    {"¡Ese es Anillado! Sus anillos son pulseras de hielo.",
     "¡Mira! Ese es Anillado, el planeta de las pulseras. Dicen que cada anillo es un deseo que alguien dejó caer, y el hielo lo guardó para que no se perdiera."},
    {"¡Un cometa cartero! Lleva cartas a las estrellas.",
     "Es el cometa Cartero. Lleva cartas a las estrellas lejanas, pero siempre se le olvidan las direcciones, y por eso tarda tanto en llegar."},
    {"Ese es Rojito. Siempre tiene calorcito.",
     "Ese es Rojito, el planeta de las siestas tibias. Tiene una lunita chica que le tararea hasta que se queda dormido."},
    {"¡Mira! Ese es Trueno, el hogar de las tormentas que cantan.",
     "¡Mira! Ese es Trueno, el hogar de las tormentas que cantan. Cada rayo es una nota, y por eso los truenos suenan a canción de cuna."},
    {"Esa estrellita es una luciérnaga dormida.",
     "Esa estrellita no es una estrella: es una luciérnaga que subió tan alto que se quedó dormida. Si le hablas bajito, parpadea."},
};

/* al juntar cada estrella (la 1.ª, la 2.ª...) */
static const cielo_txt_t CIELO_ESTRELLA[7] = {
    {"¡Una! Brilla como tus ojos.", "¡La primera! Mira cómo brilla: se parece a la luz que tienes cuando sonríes."},
    {"¡Dos! Hacen buena pareja.", "Ya son dos. Las estrellas nunca van solas: siempre buscan a alguien con quien brillar."},
    {"¡Tres! Ya se ve un dibujo.", "Con tres estrellas ya se adivina un dibujo. Sigue juntando y descubrimos qué es."},
    {"¡Cuatro! El cielo te quiere.", "Cuatro. El cielo es generoso con quien se detiene a mirarlo."},
    {"¡Cinco! Tengo polvo de estrellas para ti.", "¡Cinco! Con cinco ya puedo bajar un poquito de polvo de estrellas para ustedes."},
    {"¡Seis! Casi está.", "Seis. Falta solo una para que el dibujo quede completo."},
    {"¡Siete! El dibujo quedó listo.", "¡Siete! El dibujo está completo. Es hora de ponerle nombre."},
};

/* las constelaciones que se forman según cuántas estrellas junte (3 a 7) */
static const char *const CIELO_NOMBRE[5] = {"La Hojita", "El Barquito de Hoja", "El Faro de Piedritas", "La Ballena Chiquita", "La Compañera Dormida"};

/* final A: la ballena le pone nombre */
static const cielo_txt_t CIELO_NOMBRAR[5] = {
    {"Formaron La Hojita. Ya vive en el cielo.",
     "Mira lo que formaron: La Hojita. Desde esta noche las hojas que sueñan con volar la usan para saber que se puede."},
    {"Formaron El Barquito de Hoja. ¡Buen viaje!",
     "Formaron El Barquito de Hoja. Los navegantes de las nubes lo usan para no perderse y, de vez en cuando, uno baja a saludar a la isla."},
    {"Formaron El Faro de Piedritas. Alumbra las noches.",
     "Formaron El Faro de Piedritas. Es igualito al de la isla: cada estrella es una piedrita que alguien llevó para que nadie se perdiera."},
    {"Formaron La Ballena Chiquita. ¡Soy yo, de bebé!",
     "Formaron La Ballena Chiquita. Es mi retrato de cuando era bebé y flotaba entre las nubes sin saber nadar. Qué bien se ve."},
    {"Formaron La Compañera Dormida. Eres tú.",
     "Formaron La Compañera Dormida. Es el dibujo de alguien que cuida a quien quiere hasta cuando duerme. Me recuerda mucho a ustedes."},
};

/* final B: la ballena cuenta un cuentito */
static const cielo_txt_t CIELO_CUENTO[5] = {
    {"Una hojita soñaba con volar. El viento la subió, y ahora brilla para que otras hojas también se atrevan.",
     "Había una hojita que soñaba con volar. Una noche el viento la subió tan alto que no pudo bajar... y se quedó brillando, para que otras hojas supieran que atreverse vale la pena."},
    {"Un barquito de hoja navegó tan lejos que llegó al cielo. Allí las estrellas le dieron un puerto.",
     "Un barquito de hoja navegó tan lejos que se salió del lago y llegó al cielo. Las estrellas lo recibieron, le dieron un puerto y desde entonces guía a los que navegan de noche."},
    {"Las piedritas del faro eran tan lindas que el cielo quiso una. Hoy hay un faro arriba y otro en la isla.",
     "Las piedritas del faro eran tan lindas que el cielo quiso tener una. Los barquitos de hoja le llevaron la más brillante, y hoy hay un faro abajo en la isla y otro arriba, guiñándose."},
    {"Una ballenita se perdió entre las nubes. Las estrellas se juntaron y le hicieron un camino de luz hasta casa.",
     "Una ballenita se perdió entre las nubes y lloró tanto que se le cayeron estrellas. Las estrellas se juntaron, le hicieron un camino de luz y la llevaron de vuelta a casa."},
    {"Alguien se durmió cuidando a su amiga. Las estrellas se quedaron despiertas para cuidarlas a las dos.",
     "Alguien se durmió cuidando a su mejor amiga. Las estrellas, para no dejarlas solas, se quedaron despiertas toda la noche y dibujaron en el cielo cómo se querían."},
};

/* cuando juntó menos de tres estrellas */
static const cielo_txt_t CIELO_POCAS = {
    "Qué lindo paseo. La próxima juntamos más estrellas.",
    "Qué lindo paseo. No juntamos estrellas para un dibujo, pero el cielo igual nos conoció. La próxima volvemos por más."};
