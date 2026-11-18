/*
 * Arte en pixel art, generado por código (sin archivos de imagen).
 *
 * Todo se dibuja a baja resolución y se amplía por un factor entero, así se
 * mantiene el aspecto de pixel art en la pantalla de 466x466. En pantalla cada
 * pixel mide 2x2 (las criaturas a escala 3 se dibujan en 30x30, los íconos a
 * escala 4 en 24x24 y el mundo en 234x234). Las criaturas, el mundo y los
 * íconos son diseños originales de este proyecto.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"

#define ART_PANTALLA 466
#define ART_MUNDO 156   /* el mundo se diseña en 156x156 (y se pinta en 234x234, ver art.c) */
#define ART_ESCALA_MUNDO 3

/* Imagen ARGB8888 con búfer propio, reutilizable */
typedef struct {
    lv_image_dsc_t dsc;
    uint32_t *px;
    uint32_t capacidad; /* en pixeles */
} art_img_t;

typedef enum {
    PERIODO_DIA = 0,
    PERIODO_ATARDECER,
    PERIODO_NOCHE,
    PERIODO_COUNT
} art_periodo_t;

typedef enum {
    EXP_ABIERTOS = 0,
    EXP_CERRADOS, /* durmiendo */
    EXP_FELIZ,    /* ojos ^ ^ */
    EXP_TRISTE,   /* con lágrima */
    EXP_ENOJADO,  /* cejas */
    EXP_MAREADO,  /* enferma */
    EXP_CURIOSO,  /* ojos grandes, mira arriba */
    EXP_MIEDO,    /* pupila chica, sudor */
    EXP_ORGULLO,  /* párpados satisfechos, destellos */
    EXP_CELOS,    /* mira de reojo, cejas */
    EXP_ABURRIDO, /* párpado caído */
    EXP_ENFERMA,      /* enferma: ojos — —, boca ⌒ que tiembla, mejillas de fiebre, sudor y burbuja de mocos */
    EXP_ENFERMA_LEVE, /* lo mismo para los más chicos: sin sudor ni mocos */
    EXP_COUNT
} art_expresion_t;

/* ---------------------------------------------------------------------------
 * Catálogo de íconos: una sola lista con el nombre, qué significa y dónde se usa.
 * De aquí salen el enum, la hoja de muestra y docs/iconos.md / iconos.json, que
 * usará el editor de personalizaciones para saber qué representa cada uno.
 *   X(ID, grupo, nombre, significado, dónde aparece)
 * ------------------------------------------------------------------------- */
#define ART_ICONOS(X) \
    /* botones */ \
    X(ICO_PLATO,       "boton",    "Comer",            "Darle de comer: abre la comida, la fruta y la miel", "Barra de botones; globo cuando tiene hambre") \
    X(ICO_PELOTA,      "boton",    "Jugar",            "Jugar con ella; también: quiere jugar o jugaron", "Barra de botones; globo y pensamiento") \
    X(ICO_DUCHA,       "boton",    "Bañar",            "Darle un baño; también: quiere bañarse; higiene", "Barra de botones; globo; panel Más") \
    X(ICO_ESTADO,      "boton",    "Más",              "Abre cómo se siente, sus gustos y las demás acciones", "Barra de botones") \
    X(ICO_BOMBILLA,    "boton",    "Luz",              "Apagar la luz para que duerma (o encenderla)", "Panel Más") \
    X(ICO_JERINGA,     "boton",    "Curar",            "Curarla cuando se siente mal (usa una hierba medicinal)", "Panel Más; globo cuando está enferma") \
    X(ICO_CEPILLO,     "boton",    "Cepillar",         "Cepillarla: la limpia un poco y le da cariño", "Panel Más") \
    X(ICO_MACETA,      "boton",    "Rincón",           "Decorar su rincón con lo que encontró", "Panel Más") \
    X(ICO_AMIGOS,      "boton",    "Amigos",           "Amigos cercanos (otros orbes); también: una visita", "Panel Más; globo; pensamiento") \
    X(ICO_AJUSTES,     "boton",    "Ajustes",          "Hemisferio de las estaciones y mascota nueva", "Panel Más") \
    X(ICO_SI,          "boton",    "Sí",               "Aceptar / listo", "Invitaciones, tarjetas, rincón (decoración ya puesta)") \
    X(ICO_NO,          "boton",    "No",               "Rechazar; también: no le gusta o no quiere", "Invitaciones; reacción; gustos (lo que odia)") \
    /* comida y cosas que encuentra */ \
    X(ICO_COMIDA,      "objeto",   "Comida",           "La comida de siempre (nunca se acaba)", "Panel Comer") \
    X(ICO_FRUTA,       "objeto",   "Fruta",            "Fruta: sana; cae del árbol cada día o la encuentra", "Panel Comer, Rincón, hallazgos") \
    X(ICO_MIEL,        "objeto",   "Miel",             "Miel: dulce, la divierte pero no es muy sana", "Panel Comer, Rincón, hallazgos") \
    X(ICO_HIERBA,      "objeto",   "Hierba medicinal", "Se usa al curarla", "Panel Más (cantidad), Rincón, hallazgos") \
    X(ICO_SEMILLA,     "objeto",   "Semilla",          "Sirve para decorar (flores, hongos...)", "Rincón, hallazgos") \
    X(ICO_PIEDRA,      "objeto",   "Piedra brillante", "Sirve para decorar (farol, banco, casita)", "Rincón, hallazgos; pensamiento: encontró algo") \
    X(ICO_TAZA,        "objeto",   "Chocolate caliente", "Bebida caliente: la abriga cuando tiene frío (con calor, quema)", "Panel Comer; globo cuando tiene frío; pensamiento") \
    X(ICO_VASO,        "objeto",   "Jugo con hielo",   "Bebida fría: la refresca cuando tiene calor (con frío, ¡brrr!)", "Panel Comer; globo cuando tiene calor; pensamiento") \
    X(ICO_RAMITA,      "objeto",   "Ramita",           "Algo que encontró y te regala con cariño (no sirve)", "Hallazgos") \
    /* decoraciones */ \
    X(ICO_FLOR,        "decor",    "Flores",           "Decoración: flores en el prado", "Rincón; prado") \
    X(ICO_HONGO,       "decor",    "Hongos",           "Decoración: hongos", "Rincón; prado") \
    X(ICO_FAROL,       "decor",    "Farol",            "Decoración: ilumina de noche", "Rincón; prado") \
    X(ICO_BANCO,       "decor",    "Banco",            "Decoración: banco", "Rincón; prado") \
    X(ICO_CASITA,      "decor",    "Casita",           "Decoración: la protege de la lluvia; también: seguridad", "Rincón; prado; panel Más") \
    /* clima */ \
    X(ICO_SOL,         "clima",    "Sol",              "Despejado; también: encender la luz", "Arriba de la pantalla; globo; pensamiento") \
    X(ICO_NUBE,        "clima",    "Nublado",          "Día gris", "Arriba; globo; pensamiento") \
    X(ICO_LLUVIA,      "clima",    "Lluvia",           "Llueve", "Arriba; globo; pensamiento") \
    X(ICO_VIENTO,      "clima",    "Viento",           "Hay viento", "Arriba; globo; pensamiento") \
    X(ICO_COPO,        "clima",    "Nieve",            "Nieva (solo en invierno)", "Arriba; globo; pensamiento") \
    X(ICO_RAYO,        "clima",    "Tormenta",         "Tormenta con truenos (asusta a las tímidas)", "Arriba; globo; pensamiento") \
    X(ICO_FRIO,        "clima",    "Frío",             "Hace mucho frío (a algunas les gusta, otras se abrigan)", "Arriba; globo; pensamiento") \
    X(ICO_CALOR,       "clima",    "Calor",            "Tiene calor (verano, mediodía, sol)", "Pensamiento; globo") \
    X(ICO_LUNA,        "clima",    "Noche / sueño",    "Tiene sueño o es hora de dormir; energía", "Globo; pensamiento; panel Más") \
    /* emociones (caritas) */ \
    X(ICO_CARA_FELIZ,     "emocion", "Alegría",        "Está feliz", "Pensamiento; panel Más") \
    X(ICO_CARA_TRISTE,    "emocion", "Tristeza",       "Está triste", "Pensamiento; panel Más") \
    X(ICO_CARA_ENOJADA,   "emocion", "Enojo",          "Está de mal humor", "Pensamiento; panel Más") \
    X(ICO_CARA_MIEDO,     "emocion", "Miedo",          "Tiene miedo", "Pensamiento; panel Más") \
    X(ICO_CARA_CURIOSA,   "emocion", "Curiosidad",     "Algo le llama la atención", "Pensamiento; panel Más") \
    X(ICO_CARA_ABURRIDA,  "emocion", "Aburrimiento",   "Se aburre", "Pensamiento; panel Más") \
    X(ICO_CARA_CANSADA,   "emocion", "Cansancio",      "Tiene sueño", "Pensamiento; panel Más") \
    X(ICO_CARA_ORGULLOSA, "emocion", "Orgullo",        "Está orgullosa de algo que logró", "Pensamiento; panel Más") \
    X(ICO_CARA_CELOSA,    "emocion", "Celos",          "Siente celos", "Pensamiento; panel Más") \
    X(ICO_CARA_CARINO,    "emocion", "Cariño",         "Te quiere mucho", "Pensamiento; panel Más") \
    /* causas (por qué siente lo que siente) */ \
    X(ICO_MANO,        "causa",    "Caricia",          "La acariciaste; también: quiere mimos", "Pensamiento; globo; gustos (parte del cuerpo); marca donde la llamaste") \
    X(ICO_MANO_NO,     "causa",    "Toque que no le gusta", "La tocaste donde no le gusta", "Pensamiento") \
    X(ICO_PERSONA,     "causa",    "Tú",               "Te extraña / volviste", "Pensamiento") \
    X(ICO_SACUDIDA,    "causa",    "Sacudida",         "Sacudiste el orbe", "Pensamiento") \
    X(ICO_TERMOMETRO,  "causa",    "Enferma",          "Está enferma; también: salud", "Pensamiento; globo; panel Más") \
    X(ICO_DESPERTADOR, "causa",    "La despertaste",   "La despertaste antes de tiempo", "Pensamiento") \
    X(ICO_ALERTA,      "causa",    "Lugar del susto",  "Aquí se asustó una vez", "Pensamiento") \
    X(ICO_PUNTOS,      "causa",    "Nada que hacer",   "No pasa nada nuevo", "Pensamiento") \
    X(ICO_LUPA,        "causa",    "Algo nuevo",       "Algo nuevo que explorar; también: curiosidad", "Pensamiento; panel Más; amigos (buscando)") \
    X(ICO_GOTA,        "causa",    "Sucia",            "Está sucia (barro)", "Pensamiento; reacción al no querer bañarse") \
    X(ICO_BURBUJA,     "causa",    "Baño",             "Por el baño", "Pensamiento; burbujas al bañarla") \
    X(ICO_ESTRELLA,    "causa",    "Logro",            "Creció, evolucionó o logró algo", "Pensamiento; efecto al crecer") \
    /* efectos que flotan sobre ella */ \
    X(ICO_CORAZON,     "efecto",   "Corazón",          "Le gustó / cariño / amor", "Efecto flotante; gustos (lo que le encanta)") \
    X(ICO_NOTA,        "efecto",   "Nota musical",     "Está jugando o contenta", "Efecto flotante; amigos (invitando)") \
    X(ICO_ENOJO,       "efecto",   "Marca de enojo",   "Se enojó", "Efecto flotante; globo") \
    X(ICO_SUDOR,       "efecto",   "Gotita de nervios", "Se puso nerviosa o se asustó", "Efecto flotante") \
    X(ICO_LAGRIMA,     "efecto",   "Lágrima",          "Llora", "Efecto flotante") \
    X(ICO_ZETA,        "efecto",   "Zzz",              "Está durmiendo", "Efecto flotante") \
    X(ICO_PREGUNTA,    "efecto",   "Signo de pregunta", "Olfatea o algo le da curiosidad", "Efecto flotante") \
    X(ICO_CACA,        "efecto",   "Caca",             "El prado está sucio: hay que bañarla", "Prado") \
    X(ICO_HUEVO,       "efecto",   "Huevo",            "Huevo / mascota nueva", "Globo del huevo; ajustes") \
    X(ICO_ARDILLA_B,  "fauna",    "Monito del monte (cuadro 2)", "Segundo cuadro de la animación", "Mundo") \
    X(ICO_CONEJO_B,  "fauna",    "Conejo (cuadro 2)", "Segundo cuadro de la animación", "Mundo") \
    X(ICO_ZORRILLO_B,  "fauna",    "Zorrillo (cuadro 2)", "Segundo cuadro de la animación", "Mundo") \
    X(ICO_MAPACHE_B,  "fauna",    "Quique (cuadro 2)", "Segundo cuadro de la animación", "Mundo") \
    X(ICO_NUTRIA_B,  "fauna",    "Nutria (cuadro 2)", "Segundo cuadro de la animación", "Mundo") \
    X(ICO_CASTOR_B,  "fauna",    "Castor (cuadro 2)", "Segundo cuadro de la animación", "Mundo") \
    X(ICO_RATON_B,  "fauna",    "Raton (cuadro 2)", "Segundo cuadro de la animación", "Mundo") \
    X(ICO_OSO_B,  "fauna",    "Oso (cuadro 2)", "Segundo cuadro de la animación", "Mundo") \
    X(ICO_CIERVO_CAFE_B,  "fauna",    "Huemul (cuadro 2)", "Segundo cuadro de la animación", "Mundo") \
    X(ICO_TEJON_B,  "fauna",    "Vizcacha (cuadro 2)", "Segundo cuadro de la animación", "Mundo") \
    X(ICO_GATO_MONTES_B,  "fauna",    "Güiña (cuadro 2)", "Segundo cuadro de la animación", "Mundo") \
    X(ICO_LINCE_B,  "fauna",    "Gato colocolo (cuadro 2)", "Segundo cuadro de la animación", "Mundo") \
    X(ICO_PUMA_B,  "fauna",    "Puma (cuadro 2)", "Segundo cuadro de la animación", "Mundo") \
    X(ICO_ERIZO_B,  "fauna",    "Pichi (cuadro 2)", "Segundo cuadro de la animación", "Mundo") \
    X(ICO_TOPO_B,  "fauna",    "Topo (cuadro 2)", "Segundo cuadro de la animación", "Mundo") \
    X(ICO_LAGARTIJA_B,  "fauna",    "Lagartija (cuadro 2)", "Segundo cuadro de la animación", "Mundo") \
    X(ICO_SALAMANDRA_B,  "fauna",    "Ranita de Darwin (cuadro 2)", "Segundo cuadro de la animación", "Mundo") \
    X(ICO_CAMALEON_B,  "fauna",    "Camaleon (cuadro 2)", "Segundo cuadro de la animación", "Mundo") \
    X(ICO_CULEBRA_B,  "fauna",    "Culebra (cuadro 2)", "Segundo cuadro de la animación", "Mundo") \
    X(ICO_TORTUGA_B,  "fauna",    "Tortuga (cuadro 2)", "Segundo cuadro de la animación", "Mundo") \
    X(ICO_CARACOL_B,  "fauna",    "Caracol (cuadro 2)", "Segundo cuadro de la animación", "Mundo") \
    X(ICO_COLIBRI_B,  "fauna",    "Picaflor (cuadro 2)", "Segundo cuadro de la animación", "Mundo") \
    X(ICO_CARPINTERO_B,  "fauna",    "Carpintero (cuadro 2)", "Segundo cuadro de la animación", "Mundo") \
    X(ICO_MARTIN_B,  "fauna",    "Martin (cuadro 2)", "Segundo cuadro de la animación", "Mundo") \
    X(ICO_GOLONDRINA_B,  "fauna",    "Golondrina (cuadro 2)", "Segundo cuadro de la animación", "Mundo") \
    X(ICO_URRACA_B,  "fauna",    "Urraca (cuadro 2)", "Segundo cuadro de la animación", "Mundo") \
    X(ICO_CUERVO_B,  "fauna",    "Tordo (cuadro 2)", "Segundo cuadro de la animación", "Mundo") \
    X(ICO_PATO_B,  "fauna",    "Pato (cuadro 2)", "Segundo cuadro de la animación", "Mundo") \
    X(ICO_CISNE_B,  "fauna",    "Cisne de cuello negro (cuadro 2)", "Segundo cuadro de la animación", "Mundo") \
    X(ICO_GARZA_B,  "fauna",    "Garza (cuadro 2)", "Segundo cuadro de la animación", "Mundo") \
    X(ICO_BUHO_B,  "fauna",    "Chuncho (cuadro 2)", "Segundo cuadro de la animación", "Mundo") \
    X(ICO_MURCIELAGO_B,  "fauna",    "Murcielago (cuadro 2)", "Segundo cuadro de la animación", "Mundo") \
    X(ICO_MARIQUITA_B,  "fauna",    "Chinita (cuadro 2)", "Segundo cuadro de la animación", "Mundo") \
    X(ICO_LIBELULA_B,  "fauna",    "Matapiojos (cuadro 2)", "Segundo cuadro de la animación", "Mundo") \
    X(ICO_GRILLO_B,  "fauna",    "Grillo (cuadro 2)", "Segundo cuadro de la animación", "Mundo") \
    X(ICO_HORMIGAS_B,  "fauna",    "Hormigas (cuadro 2)", "Segundo cuadro de la animación", "Mundo") \
    X(ICO_ESCARABAJO_B,  "fauna",    "Escarabajo (cuadro 2)", "Segundo cuadro de la animación", "Mundo") \
    X(ICO_POLILLA_B,  "fauna",    "Polilla (cuadro 2)", "Segundo cuadro de la animación", "Mundo") \
    X(ICO_CRIS_0,  "juego",    "Cristal 1", "Uno de los seis cristales del juego de la cueva", "Cristales") \
    X(ICO_CRIS_1,  "juego",    "Cristal 2", "Uno de los seis cristales del juego de la cueva", "Cristales") \
    X(ICO_CRIS_2,  "juego",    "Cristal 3", "Uno de los seis cristales del juego de la cueva", "Cristales") \
    X(ICO_CRIS_3,  "juego",    "Cristal 4", "Uno de los seis cristales del juego de la cueva", "Cristales") \
    X(ICO_CRIS_4,  "juego",    "Cristal 5", "Uno de los seis cristales del juego de la cueva", "Cristales") \
    X(ICO_CRIS_5,  "juego",    "Cristal 6", "Uno de los seis cristales del juego de la cueva", "Cristales") \
    X(ICO_POCHI,  "visita",    "Pochi", "La chihuahua visitante (la mamá de Chapi, Pasita, Morita y Rony), de pie", "Pochi") \
    X(ICO_POCHI_B,  "visita",    "Pochi (cola)", "Segundo cuadro: mueve la cola", "Pochi") \
    X(ICO_POCHI_C,  "visita",    "Pochi (trota 1)", "Trota: patas de un lado", "Pochi") \
    X(ICO_POCHI_D,  "visita",    "Pochi (trota 2)", "Trota: patas del otro", "Pochi") \
    X(ICO_POCHI_COME,  "visita",    "Pochi comiendo", "Con la cabeza en el platito", "Pochi") \
    X(ICO_POCHI_SENT,  "visita",    "Pochi sentada", "Sentada esperando", "Pochi") \
    X(ICO_POCHI_PLATO,  "visita",    "Platito de Pochi", "Su platito de papas fritas", "Pochi") \
    X(ICO_ARBOL_OFRENDAS, "mundo",   "Árbol de las ofrendas", "El árbol viejo del prado, con raíces grandes", "Prado") \
    X(ICO_CUIDADOR,    "mundo",      "El Cuidador", "De frente, con su barba blanca, en el final", "Final") \
    X(ICO_FAROL_PAPEL,  "fecha",    "Farolito de papel", "Decoración de Año Nuevo en el prado", "Nochevieja") \
    X(ICO_GLOBO_FIESTA, "fecha",    "Globo de fiesta", "Decoración de Año Nuevo en el prado", "Nochevieja") \
    /* fauna (las que tienen _B son el segundo cuadro de la animación) */ \
    X(ICO_MARIPOSA,    "fauna",    "Mariposa",         "Una mariposa: le encanta perseguirlas; vienen más si hay flores", "Mundo; pensamiento") \
    X(ICO_MARIPOSA_B,  "fauna",    "Mariposa (aleteo)", "Segundo cuadro del aleteo", "Mundo") \
    X(ICO_ABEJA,       "fauna",    "Abeja",            "Una abeja: vienen a las flores; si la persigue puede picarla", "Mundo; pensamiento") \
    X(ICO_ABEJA_B,     "fauna",    "Abeja (aleteo)",   "Segundo cuadro del aleteo", "Mundo") \
    X(ICO_PAJARO,      "fauna",    "Pájaro",           "El pájaro amigo: si lo dejas tranquilo a veces trae regalos", "Mundo; pensamiento") \
    X(ICO_PAJARO_B,    "fauna",    "Pájaro (aleteo)",  "Segundo cuadro del aleteo", "Mundo") \
    X(ICO_ZORRO,       "fauna",    "Zorro",            "Un zorro travieso: viene por la fruta y la miel; tócalo para espantarlo", "Mundo; pensamiento") \
    X(ICO_RANA,        "fauna",    "Rana",             "Una rana de la laguna", "Mundo (laguna); pensamiento") \
    /* jardín */ \
    X(ICO_BROTE,       "jardin",   "Brote",            "Una semilla recién plantada que empieza a crecer", "Jardín del prado") \
    X(ICO_FLOR_MUSTIA, "jardin",   "Flor mustia",      "Una planta con sed: hay que regarla (no se muere)", "Jardín del prado; pensamiento") \
    X(ICO_ARBOLITO,    "jardin",   "Arbolito",         "El árbol que creció de una semilla que ella enterró", "Jardín del prado") \
    X(ICO_ARBOL_FRUTAL, "jardin",  "Árbol con frutos", "Su árbol ya da fruta", "Jardín del prado") \
    X(ICO_REGADERA,    "jardin",   "Regar",            "Regar una planta; también: el jardín", "Jardín; pensamiento") \
    X(ICO_HOJAS,       "jardin",   "Montón de hojas",  "Hojas de otoño: tócalas y saltará encima", "Prado en otoño; pensamiento") \
    X(ICO_CHARCO,      "jardin",   "Charco",           "Charcos después de la lluvia: le encanta salpicar", "Prado; pensamiento") \
    /* zonas del mundo (flechas para cambiar de lugar) */ \
    X(ICO_ZONA_PRADO,  "zona",     "Prado",            "Su casa: el prado con su rincón y su jardín", "Flechas de zona; pensamiento") \
    X(ICO_ZONA_BOSQUE, "zona",     "Bosque",           "El bosque: árboles, hongos y un arroyo", "Flechas de zona; pensamiento") \
    X(ICO_ZONA_LAGO,   "zona",     "Laguna",             "La laguna: ranas, un muelle y un sauce", "Flechas de zona; pensamiento") \
    X(ICO_ZONA_COLINA, "zona",     "Cerro",           "El cerro: el árbol solitario y el mirador", "Flechas de zona; pensamiento") \
    X(ICO_ZONA_CUEVA,  "zona",     "Cueva",            "La cueva secreta con cristales: un refugio", "Flechas de zona; pensamiento") \
    X(ICO_FLECHA,      "zona",     "Ir a otra zona",   "Flecha para cambiar de zona (se espeja para la izquierda)", "Bordes de la pantalla") \
    /* fase 3: fiesta, lago, amigo y gestos */ \
    X(ICO_PASTEL,      "fiesta",   "Pastel de cumpleaños", "Tu cumpleaños: tócalo para cantar", "Mundo, el día de tu cumpleaños; globo") \
    X(ICO_REGALO,      "fiesta",   "Regalo",           "Un regalo (de su amigo o por tu cumpleaños)", "Globo; efecto flotante") \
    X(ICO_CALENDARIO,  "fiesta",   "Fecha de nacimiento", "Tu fecha de nacimiento (calcula tu edad y tu cumpleaños)", "Bienvenida; ajustes") \
    X(ICO_BOTE,        "laguna",     "Bote",             "El bote de la laguna: tócalo y sale a pescar", "Laguna") \
    X(ICO_PEZ,         "laguna",     "Pez",              "Pescó un pez (se lo come o lo devuelve al agua)", "Efecto flotante; pensamiento") \
    X(ICO_BOTA,        "laguna",     "Bota vieja",       "Pescó una bota vieja (le da risa)", "Efecto flotante") \
    X(ICO_CANA,        "laguna",     "Caña de pescar",   "Está pescando; también: la pesca", "Pensamiento; panel Más") \
    X(ICO_GALLETA,     "objeto",   "Galletas",         "Las galletas que trae su amigo para compartir", "Picnic con su amigo") \
    X(ICO_MAREO,       "efecto",   "Mareo",            "La mareaste con muchos toques seguidos", "Efecto flotante") \
    X(ICO_ABRAZO,      "efecto",   "Abrazo",           "Un abrazo (mantener el dedo sobre ella)", "Efecto flotante; gustos") \
    /* juegos y trucos */ \
    X(ICO_CANASTA,     "juego",    "Canasta",          "Atrapar frutas: la mueves con el dedo (o inclinando el orbe)", "Menú Jugar; minijuego") \
    X(ICO_ESCONDIDAS,  "juego",    "Escondidas",       "Jugar a las escondidas: se esconde y la buscas", "Menú Jugar") \
    X(ICO_PATITA,      "juego",    "Trucos",           "Enseñarle trucos", "Menú Jugar") \
    X(ICO_SENTAR,      "juego",    "Sentarse",         "(Ya no se usa: era el truco de sentarse; ahora es hacerse la dormida, con la Zzz)", "-") \
    X(ICO_VUELTA,      "juego",    "Dar una vuelta",   "El truco de darse vuelta como trompo", "Panel Trucos") \
    X(ICO_SALUDO,      "juego",    "Saludar",          "El truco de saludar", "Panel Trucos") \
    /* recuerdos y sorpresas (fase 4) */ \
    X(ICO_LIBRO,       "recuerdo", "Diario",           "Su diario: lo importante de cada día, el álbum y sus colecciones", "Panel Más") \
    X(ICO_FOTO,        "recuerdo", "Foto",             "Se guardó una foto en el álbum; también: el álbum", "Diario; efecto al sacar una foto") \
    X(ICO_COFRE,       "recuerdo", "Colección",        "Sus colecciones de tesoros", "Diario") \
    X(ICO_META,        "recuerdo", "Meta del día",     "La meta suave de hoy (sin castigo si no se cumple)", "Diario; globo de la mañana") \
    X(ICO_ESTAMPITA,   "recuerdo", "Estampita",        "Premio por cumplir una meta", "Diario; efecto") \
    X(ICO_GORRO,       "recuerdo", "Gorro de fiesta",  "Es su cumpleaños (lo lleva puesto todo el día)", "Sobre su cabeza; álbum") \
    X(ICO_NARIZ,       "recuerdo", "¡Huele algo!",     "Olió un tesoro escondido y va a cavar", "Efecto; pensamiento") \
    X(ICO_CONCHA,      "tesoro",   "Concha",           "Colección de conchas de la laguna", "Colección; hallazgo") \
    X(ICO_PLUMA,       "tesoro",   "Pluma",            "Colección de plumas (del bosque y del pájaro)", "Colección; hallazgo") \
    X(ICO_HOJA,        "tesoro",   "Hoja",             "Colección de hojas: una por estación", "Colección; hallazgo") \
    X(ICO_CRISTAL,     "tesoro",   "Cristal",          "Colección de cristales de la cueva", "Colección; hallazgo") \
    X(ICO_CHAPITA,     "tesoro",   "Chapita",          "Una chapita de botella: \"basura\" que te trae con cariño", "Regalo; hallazgo") \
    X(ICO_HOJA_SECA,   "tesoro",   "Hoja seca",        "Una hoja seca: otro regalo \"basura\"", "Regalo; hallazgo") \
    X(ICO_ARCOIRIS,    "raro",     "Arcoíris",         "Salió un arcoíris (a veces después de la lluvia)", "Mundo; álbum; pensamiento") \
    X(ICO_ESTRELLA_FUGAZ, "raro",  "Estrella fugaz",   "Pasó una estrella fugaz: ¡pide un deseo!", "Mundo de noche; álbum") \
    X(ICO_CIERVO,      "raro",     "Pudú blanco",      "Un pudú blanco que casi nadie ve", "Mundo al amanecer o atardecer; álbum") \
    X(ICO_CIERVO_B,    "raro",     "Pudú blanco (paso 1)", "Cuadro de su caminata", "Mundo") \
    X(ICO_CIERVO_C,    "raro",     "Pudú blanco (paso 2)", "Cuadro de su caminata", "Mundo") \
    X(ICO_CIERVO_D,    "raro",     "Pudú blanco (cabeza baja)", "Cuadro de su caminata", "Mundo") \
    X(ICO_GORRO_DORMIR, "ropa",     "Gorro de dormir", "Un gorrito azul con borla para dormir en su camita", "Casita") \
    X(ICO_FRUTA_MANZANA, "juego",    "Manzana", "Cae en el juego de atrapar frutas", "Juego de las frutas") \
    X(ICO_FRUTA_NARANJA, "juego",    "Naranja", "Cae en el juego de atrapar frutas", "Juego de las frutas") \
    X(ICO_FRUTA_BANANA, "juego",    "Banana", "Cae en el juego de atrapar frutas", "Juego de las frutas") \
    X(ICO_FRUTA_UVAS, "juego",    "Uvas", "Cae en el juego de atrapar frutas", "Juego de las frutas") \
    X(ICO_FRUTA_CEREZAS, "juego",    "Cerezas", "Cae en el juego de atrapar frutas", "Juego de las frutas") \
    X(ICO_FRUTA_PERA, "juego",    "Pera", "Cae en el juego de atrapar frutas", "Juego de las frutas") \
    X(ICO_FRUTA_SANDIA, "juego",    "Sandía", "Cae en el juego de atrapar frutas", "Juego de las frutas") \
    X(ICO_GLOBO_AIRE,  "raro",     "Ballena de nubes", "Una ballena hecha de nubes nada despacio por el cielo", "Mundo; álbum") \
    X(ICO_CALABAZA,    "fecha",    "Calabaza",         "Decoración Halloween (de fábrica el 31 de octubre; el editor puede usarla en otras fiestas)", "Mundo ese día; álbum") \
    X(ICO_ARBOL_NAVIDAD, "fecha",  "Árbol de Navidad", "Decoración Navidad (de fábrica 24 y 25 de diciembre)", "Mundo esos días; álbum") \
    X(ICO_FUEGOS,      "fecha",    "Fuegos artificiales", "Decoración Año Nuevo (de fábrica 31/12 y 1/1)", "Mundo de noche; álbum") \
    X(ICO_BANDERINES,  "fecha",    "Banderines",       "Decoración Fiestas (de fábrica Fiestas Patrias, 18 y 19 de septiembre; la fecha se cambia en el editor)", "Arriba del mundo esos días; álbum") \
    X(ICO_NADAR,       "recuerdo", "Nadar",            "Está aprendiendo a nadar", "Pensamiento; diario") \
    X(ICO_TREPAR,      "recuerdo", "Trepar",           "Está aprendiendo a trepar árboles", "Pensamiento; diario") \
    X(ICO_RELOJ,       "ajustes",  "Reloj",            "La hora de hoy (se pregunta al empezar o si el orbe la perdió)", "Bienvenida; ajustes") \
    X(ICO_DADO,        "ajustes",  "Nombre al azar",   "Elige un nombre sugerido al azar", "Elegir nombre") \
    X(ICO_RESPALDO,    "ajustes",  "Respaldo",         "Guardar la mascota en el computador o traerla de otro orbe", "Ajustes; traer una mascota") \
    X(ICO_PERCHA,      "ropa",     "Ropero",           "Su ropa y accesorios", "Más; el ropero") \
    X(ICO_MONO,        "ropa",     "Moño de pétalos",  "Un moño de pétalos que suelta brillitos", "Ropero; puesto en la mascota") \
    X(ICO_PANUELO,     "ropa",     "Pañuelo de hojas", "Pañuelo de hojas: cambia de color con la estación", "Ropero; puesto en la mascota") \
    X(ICO_GORRO_LANA,  "ropa",     "Gorro de estrellas","Gorro de lana con estrellitas que titilan: abriga", "Ropero; puesto en la mascota") \
    X(ICO_BUFANDA,     "ropa",     "Bufanda de seda",  "Tejida con seda de araña tejedora: abriga y brilla", "Ropero; puesto en la mascota") \
    X(ICO_SOMBRERO,    "ropa",     "Sombrero con flor","Sombrero de paja con una flor que se abre al sol: da sombra", "Ropero; puesto en la mascota") \
    X(ICO_LENTES,      "ropa",     "Lentes de luna",   "Lentes de cristal de luna", "Ropero; puesto en la mascota") \
    X(ICO_CORONA,      "ropa",     "Corona de flores", "Flores que brillan de noche; aparece cuando florece su jardín", "Ropero; puesto en la mascota") \
    X(ICO_COLLAR,      "ropa",     "Collar con perla", "Conchas con una perla luminosa; aparece al completar las conchas", "Ropero; puesto en la mascota") \
    X(ICO_PELUCHE,     "juguete",  "Peluche",          "Juguete: lo abraza (calma)", "Rincón; en el prado") \
    X(ICO_PELOTON,     "juguete",  "Pelotón",          "Juguete: una pelota grande para empujar", "Rincón; en el prado") \
    X(ICO_CAJA,        "juguete",  "Caja de cartón",   "Juguete: se mete adentro y se asoma", "Rincón; en el prado") \
    X(ICO_COLUMPIO,    "juguete",  "Columpio",         "Juguete: se columpia", "Rincón; en el prado") \
    X(ICO_TAMBOR,      "juguete",  "Tambor",           "Juguete: lo toca", "Rincón; en el prado") \
    X(ICO_MOLINETE,    "juguete",  "Molinete",         "Juguete: gira con el viento", "Rincón; en el prado") \
    X(ICO_GIRASOL,     "jardin",   "Girasol",          "Flor grande del jardín", "Jardín; elegir semilla") \
    X(ICO_FRUTILLA,    "jardin",   "Frutilla",         "Planta que da frutas", "Jardín; elegir semilla") \
    X(ICO_CAMARA,      "ajustes",  "Cámara",           "Sacarle una foto (se guarda en el orbe)", "Libro: pestaña fotos") \
    X(ICO_PECERA,      "ajustes",  "Modo pecera",      "Verla vivir sin botones, como una pecera", "Más") \
    X(ICO_PARLANTE,    "ajustes",  "Sonido ambiente",  "Lluvia, grillos, viento... de fondo", "Ajustes") \
    X(ICO_PALETA,      "ajustes",  "Colores",          "Elegir sus colores", "Elegir cómo será") \
    X(ICO_MEZCLA,      "ajustes",  "Mezclar",          "Mezclar dos especies", "Elegir cómo será") \
    X(ICO_PLANTA_SECA, "jardin",   "Planta seca",      "Pasó mucho sin agua: se secó (regándola todavía revive)", "Jardines") \
    X(ICO_MAPA,        "boton",    "Mapa",             "Las zonas: adónde puede ir y cuáles faltan por descubrir", "Panel Más") \
    X(ICO_CANDADO,     "ajustes",  "Por descubrir",    "Una zona que todavía no encuentra", "Mapa") \
    X(ICO_MALETA,      "ajustes",  "De viaje",         "Despedir a una mascota: se va de viaje a explorar el mundo", "Mis mascotas") \
    X(ICO_LIBRO_MAGICO,"boton",    "Libro mágico",     "El libro de los objetos mágicos que encontró", "Más → recuerdos; la Bolsa") \
    X(ICO_ESPONJA,     "boton",    "Esponja",          "Bañarla: aparece cuando está sucia y se frota sobre ella", "Barra de botones") \
    X(ICO_BOLSA,       "boton",    "La Bolsa",         "Los objetos mágicos que encontró: usarlos, devolverlos al bosque o mirar el libro", "Panel Más; botón + cuando trae algo nuevo") \
    X(ICO_BOSQUE_DEV,  "boton",    "Al bosque",        "Devolver un objeto mágico al bosque", "La Bolsa") \
    X(ICO_MANO_FROTA,  "boton",    "Frotar",           "Este objeto se usa frotándolo con el dedo", "La Bolsa") \
    X(ICO_MANO_TIENE,  "boton",    "Mantener",         "Este objeto se usa manteniendo el dedo encima", "La Bolsa") \
    X(ICO_MANO_DESLIZA,"boton",    "Deslizar",         "Este objeto se usa deslizando el dedo", "La Bolsa") \
    X(ICO_PINA,        "objeto",   "Piña",             "Un regalo al volver: una piña con semillas", "Cuando vuelves después de un rato") \
    X(ICO_SAMARA,      "objeto",   "Semilla con alas", "Un regalo al volver: la planta y crece un arbolito", "Cuando vuelves después de un rato") \
    X(ICO_HUELLA,      "objeto",   "Huella de barro",  "Trajo barro en las patas: un baño las limpia", "El prado") \
    X(ICO_ZONA_ISLA,   "zona",     "Isla",             "La isla de la laguna: un árbol viejo, un círculo de piedras y un faro", "Mapa; diario; pensamiento") \
    X(ICO_CARTA,       "recuerdo", "Carta",            "Una carta de un amigo del barrio, con su sello de cera", "Al llegar una carta; Recuerdos → Cartas") \
    X(ICO_SOPLA,       "boton",    "Sopla",            "Sopla al orbe (el micrófono): despeja la niebla, llena la vela, apaga las velas", "Niebla; el cruce en bote; tu cumpleaños") \
    X(ICO_VIRUS,       "estado",   "Resfrío",          "Un bichito de resfrío con pinchos (simpático): está enferma", "Sobre ella, parpadeando, cuando está enferma") \
    X(ICO_MALESTAR,    "estado",   "Malestar",         "Líneas onduladas moradas: se siente mal", "Sobre su cabeza cuando está enferma") \
    X(ICO_ALGA,        "laguna",     "Alga",             "Pescó un alga: le da risa", "Al pescar; diario") \
    X(ICO_CANGREJO,    "laguna",     "Cangrejito",       "Pescó un cangrejito: le pellizca la nariz y lo devuelve al agua", "Al pescar; diario") \
    X(ICO_NENUFAR,     "laguna",     "Hoja de nenúfar",  "Pescó una hoja de nenúfar: se la pone de sombrero", "Al pescar; en su cabeza") \
    X(ICO_PEZ_DORADO,  "laguna",     "Pez dorado",       "Muy raro: un pez dorado que brilla; lo devuelve y queda en el diario", "Al pescar; diario") \
    X(ICO_ERIZO,       "animal",   "Pichi",            "Tímido: si ella espera quieta, sale de su caparazón y se hacen amigos", "Prado y bosque, al atardecer y al amanecer") \
    X(ICO_ERIZO_BOLA,  "animal",   "Pichi en bolita",  "Se escondió en su caparazón: ella corrió muy cerca", "Prado y bosque") \
    X(ICO_ARDILLA,     "animal",   "Monito del monte", "Travieso: le roba una semilla y a veces se la devuelve", "Bosque y prado, de día") \
    X(ICO_CONEJO,      "animal",   "Conejo",           "Asustadizo y juguetón: juegan a perseguirse", "Prado y cerro") \
    X(ICO_CARACOL,     "animal",   "Caracol",          "Calmado: después de la lluvia deja un rastro brillante", "Prado y bosque, con charcos") \
    X(ICO_TORTUGA,     "animal",   "Tortuga",          "Sabia y rara: a su lado se le pasa la pena", "Laguna e isla") \
    X(ICO_PATO,        "animal",   "Pato",             "Curioso: la sigue en fila por la laguna", "Laguna, de día") \
    X(ICO_BUHO,        "animal",   "Chuncho",          "Sereno: canta desde el árbol y, si está despierta, la arrulla", "Prado y bosque, de noche") \
    X(ICO_TOPO,        "animal",   "Topo",             "Gruñón: asoma de un hoyo y se esconde si lo tocas", "Cerro, de día") \
    X(ICO_TOPO_HOYO,   "animal",   "Hoyo del topo",    "Un montoncito de tierra: el topo se escondió", "Cerro") \
    X(ICO_PASTEL_VELAS,"fiesta",   "Pastel con velas", "El pastel de tu cumpleaños, con las velas sin llama (las llamas se dibujan encima)", "Tu cumpleaños") \
    X(ICO_COLIBRI,       "animal",     "Picaflor", "El más chiquito: se queda quieto en el aire frente a las flores", "Prado, bosque y cerro, con calor") \
    X(ICO_CARPINTERO,    "animal",     "Pájaro carpintero", "Toc-toc: hace su casa en los troncos", "Bosque y prado, de día") \
    X(ICO_MARTIN,        "animal",     "Martín pescador", "Azul brillante: se tira al agua por un pez", "Laguna e isla") \
    X(ICO_GOLONDRINA,    "animal",     "Golondrina", "Hace piruetas en el aire; llega con la primavera", "Prado, laguna y cerro") \
    X(ICO_CISNE,         "animal",     "Cisne de cuello negro", "Blanco con el cuello negro: lleva a sus pollitos en la espalda", "Laguna e isla") \
    X(ICO_URRACA,        "animal",     "Urraca", "Conversadora: le encantan las cosas brillantes", "Prado y bosque") \
    X(ICO_CUERVO,        "animal",     "Tordo", "Negro brillante y cantor: a veces le trae una piedrita brillante", "Prado, bosque y cerro") \
    X(ICO_GARZA,         "animal",     "Garza", "Pesca parada en una pata", "Laguna") \
    X(ICO_MAPACHE,       "animal",     "Quique", "Gris con la frente blanca: rapidísimo y juguetón", "Bosque y prado, de noche") \
    X(ICO_MURCIELAGO,    "animal",     "Murciélago", "Duerme de cabeza; vuela al atardecer", "Cueva; cruza el prado al atardecer") \
    X(ICO_NUTRIA,        "animal",     "Nutria", "Juguetona: flota de espaldas", "Laguna") \
    X(ICO_CASTOR,        "animal",     "Castor", "Constructor: golpea el agua con la cola", "Laguna") \
    X(ICO_RATON,         "animal",     "Ratón de campo", "Chiquito y bigotudo: junta semillas", "Prado y cerro") \
    X(ICO_OSO,           "animal",     "Oso", "Grande y dormilón: busca moras en otoño (raro)", "Bosque, en otoño") \
    X(ICO_CIERVO_CAFE,   "animal",     "Huemul", "El del escudo: camina entre los coihues al amanecer", "Bosque, al amanecer y al atardecer") \
    X(ICO_TEJON,         "animal",     "Vizcacha", "Orejona: toma sol en las rocas y silba si se asusta", "Bosque y cerro") \
    X(ICO_ZORRILLO,      "animal",     "Zorrillo", "Blanco y negro, pacífico: mejor no asustarlo", "Prado y cerro, de noche") \
    X(ICO_GATO_MONTES,   "animal",     "Güiña", "La gatita salvaje más chica, con manchitas (rara)", "Prado, bosque y cerro, al atardecer") \
    X(ICO_LINCE,         "animal",     "Gato colocolo", "Rayitas en las patas; muy tímido (raro)", "Cerro") \
    X(ICO_PUMA,          "animal",     "Puma", "El gran gato del cerro (raro)", "Cerro, al atardecer") \
    X(ICO_LAGARTIJA,     "animal",     "Lagartija", "Toma sol en las piedras", "Cerro y prado, con sol") \
    X(ICO_SALAMANDRA,    "animal",     "Ranita de Darwin", "Nariz en punta: se hace la hoja seca", "Prado y bosque, con charcos") \
    X(ICO_CAMALEON,      "animal",     "Camaleón", "Cambia de color", "Cerro") \
    X(ICO_CULEBRA,       "animal",     "Culebrita verde", "Tímida: se desliza entre el pasto", "Prado y cerro, con sol") \
    X(ICO_MARIQUITA,     "animal",     "Chinita", "Roja con puntitos: trae buena suerte", "Prado, bosque y cerro") \
    X(ICO_LIBELULA,      "animal",     "Matapiojos", "Vuela sobre el agua como un helicóptero", "Laguna y prado") \
    X(ICO_GRILLO,        "animal",     "Grillo", "Canta de noche", "Prado y cerro, de noche") \
    X(ICO_HORMIGAS,      "animal",     "Hormigas", "Trabajan en fila", "Prado, bosque y cerro") \
    X(ICO_ESCARABAJO,    "animal",     "Escarabajo", "Con armadura brillante: rueda bolitas", "Bosque y prado") \
    X(ICO_POLILLA,       "animal",     "Polilla", "Baila alrededor de la luz de la luna", "Prado y bosque, de noche") \
    X(ICO_MUSGUITO,      "magia",      "Musguito", "El duende de musgo del prado", "Prado; Tesoros") \
    X(ICO_LUMBRE,        "magia",      "Lumbre", "El hada de luz del bosque", "Bosque; Tesoros") \
    X(ICO_ONDINA,        "magia",      "Ondina", "El espíritu del agua de la laguna", "Laguna; Tesoros") \
    X(ICO_VENTOLIN,      "magia",      "Anemoi", "El dragón del viento del cerro", "Cerro; Tesoros") \
    X(ICO_CRISTALINO,    "magia",      "Cristalino", "El gnomo de cristal de la cueva", "Cueva; Tesoros") \
    X(ICO_MAREA,         "magia",      "Marea", "La sirenita del faro de la isla", "Isla; Tesoros") \
    X(ICO_PALOMA,        "mensajero",  "Paloma mensajera", "Trae las cartas de los amigos", "Llega una carta") \
    X(ICO_PALOMA_B,      "mensajero",  "Paloma (alas abajo)", "Segundo cuadro del aleteo", "Llega una carta") \
    X(ICO_GRULLA,        "mensajero",  "Grulla de papel", "Trae las cartas de las que se fueron de viaje", "Llega una carta") \
    X(ICO_GRULLA_ABRE,   "mensajero",  "Grulla desdoblándose", "La grulla se abre y se vuelve carta", "Llega una carta") \
    X(ICO_MANTEL,        "magia",      "Mantel de picnic", "El picnic con Musguito", "Cita en el prado") \
    X(ICO_BARQUITO_HOJA, "magia",      "Barquito de hoja", "La carrera de barquitos con Ondina", "Cita en la laguna") \
    X(ICO_FARO,          "magia",      "Faro", "El faro de la isla", "Cita en la isla") \
    X(ICO_COMETA,        "magia",      "Cometa", "La cometa del cerro, para los días de viento", "Cerro") \
    X(ICO_HONGO_LUZ,     "magia",      "Hongo que brilla", "Los hongos del anillo de las hadas", "Bosque, de noche") \
    X(ICO_HADA,          "magia",      "Hada", "Las hadas del anillo", "Bosque, de noche") \
    X(ICO_EMPANADA,      "fiesta",     "Empanada", "Las empanadas de las Fiestas Patrias", "Fiestas Patrias (Chile)") \
    X(ICO_VOLANTIN,      "fiesta",     "Volantín", "Volantín con los colores de Chile", "Fiestas Patrias (Chile)") \
    X(ICO_GORRO_BRUJA,   "fiesta",     "Sombrero de bruja", "Disfraz de Halloween", "Halloween con todos") \
    X(ICO_FANTASMA,      "fiesta",     "Fantasmita", "Disfraz de Halloween", "Halloween con todos") \
    X(ICO_GUIRNALDA,     "fiesta",     "Guirnalda", "Guirnalda navideña con lucecitas", "Navidad con todos") \
    X(ICO_RAMO_AMARILLO, "fiesta",     "Ramo de flores amarillas", "El ramo que te trae el Día de las Flores Amarillas", "21 de septiembre (sur) o 21 de marzo (norte de Latinoamérica)") \
    X(ICO_PINGUINO,      "animal",     "Pingüino", "Con frac de hielo: se desliza de panza", "Laguna, con frío o nieve") \
    X(ICO_FOCA,          "animal",     "Lobo marino", "Bigotudo y con melena: toma sol sobre el hielo", "Laguna, con frío o nieve") \
    X(ICO_OSO_POLAR,     "animal",     "Elefante marino", "Enorme y dormilón, con trompita: ronca sobre el hielo", "Laguna, con frío o nieve") \
    X(ICO_CE_FRUTILLA, "comida", "Frutillas", "Comida de primavera", "Comer → De temporada; se recoge en: Prado y cerro") \
    X(ICO_CE_BROTES, "comida", "Brotes tiernos", "Comida de primavera", "Comer → De temporada; se recoge en: Bosque y prado") \
    X(ICO_CE_PETALOS, "comida", "Pétalos dulces", "Comida de primavera", "Comer → De temporada; se recoge en: Cerro") \
    X(ICO_CE_BERROS, "comida", "Berros", "Comida de primavera", "Comer → De temporada; se recoge en: Laguna y bosque") \
    X(ICO_CE_SANDIA, "comida", "Sandía", "Comida de verano", "Comer → De temporada; se recoge en: Prado y laguna") \
    X(ICO_CE_DURAZNO, "comida", "Duraznos", "Comida de verano", "Comer → De temporada; se recoge en: Bosque y prado") \
    X(ICO_CE_ARANDANOS, "comida", "Arándanos", "Comida de verano", "Comer → De temporada; se recoge en: Cerro y bosque") \
    X(ICO_CE_CHOCLO, "comida", "Choclo", "Comida de verano", "Comer → De temporada; se recoge en: Prado") \
    X(ICO_CE_CALABAZA2, "comida", "Calabaza", "Comida de otoño", "Comer → De temporada; se recoge en: Prado y cerro") \
    X(ICO_CE_CASTANAS, "comida", "Castañas", "Comida de otoño", "Comer → De temporada; se recoge en: Bosque") \
    X(ICO_CE_HONGOS, "comida", "Hongos", "Comida de otoño", "Comer → De temporada; se recoge en: Bosque y cueva") \
    X(ICO_CE_HIGOS, "comida", "Higos", "Comida de otoño", "Comer → De temporada; se recoge en: Cerro y prado") \
    X(ICO_CE_PINONES, "comida", "Piñones", "Comida de invierno", "Comer → De temporada; se recoge en: Bosque") \
    X(ICO_CE_CAMOTE, "comida", "Camote asado", "Comida de invierno", "Comer → De temporada; se recoge en: Prado y cerro") \
    X(ICO_CE_BAYAS, "comida", "Bayas de escarcha", "Comida de invierno", "Comer → De temporada; se recoge en: Cerro y laguna") \
    X(ICO_NIMBA,       "personaje", "Nimba",           "Nimba, la ballena beluga de nubes", "Libro de historias; paseo") \
    X(ICO_PARAGUAS,    "ropa",     "Paraguas de hoja", "Un paraguas de hoja: lo sostiene abierto sobre su cabeza", "Ropero; en su mano") \
    X(ICO_MOLINILLO,   "ropa",     "Molinillo de viento","Un molinillo chiquito que gira con el viento", "Ropero; en su mano") \
    X(ICO_CE_MIEL_CRIST, "comida", "Miel cristalizada", "Comida de invierno", "Comer → De temporada; se recoge en: Cueva") \
    /* versión 37: la historia del mundo */ \
    X(ICO_TILDE,        "personaje", "Tilde, la tenca", "Chismosa, imita todos los cantos y colecciona cosas brillantes; posada", "Visitas de Tilde; Libro de historias") \
    X(ICO_TILDE_B,      "personaje", "Tilde (vuela)", "Tilde con las alas abiertas, llegando o yéndose", "Visitas de Tilde") \
    X(ICO_BRIZNA,       "personaje", "Abuela Brizna", "La tortuga dormida bajo la isla, despierta", "Isla; Libro de historias") \
    X(ICO_BRIZNA_B,     "personaje", "Abuela Brizna (duerme)", "Con los ojos cerrados", "Isla") \
    X(ICO_BRIZNA_C,     "personaje", "Abuela Brizna (bosteza)", "Bostezando", "Isla") \
    X(ICO_TILO,         "personaje", "Peumo", "El Cuidador de verdad: un abuelito chiquito, con capucha de hojas, barba blanca y su cuchara", "Final secreto; páginas del diario") \
    X(ICO_HUEVO_ORO,    "mundo",     "Huevo dorado", "El huevo donde Peumo guardó la mitad del Corazón", "Final secreto") \
    X(ICO_MEDIO_CORAZON,"mundo",     "Mitad del Corazón", "La mitad que brilla dentro de ella", "Rito de la Guardiana; final") \
    X(ICO_PAGINA_TILO,  "mundo",     "Página del diario", "Una página suelta del diario de Peumo", "Libro de historias") \
    X(ICO_PRISMA,       "personaje", "Prisma", "El dragón de cristal, para el Libro de historias", "Libro de historias") \
    X(ICO_CUCHARA_PALO, "mundo",     "Cuchara de palo", "La cuchara de Peumo: sirve para todo", "Páginas del diario; lugares secretos") \
    /* versión 40: los vecinos de los lugares secretos */ \
    X(ICO_CHUCAO,       "animal",    "Chucao", "Redondito y de pecho ladrillo: canta fuerte cuando hay algo escondido", "El claro") \
    X(ICO_CHUCAO_B,     "animal",    "Chucao (canta)", "Segundo cuadro: ¡chu-cao!", "El claro") \
    X(ICO_CONDOR,       "animal",    "Cóndor", "El gran cóndor del mirador, en su roca", "El mirador") \
    X(ICO_CONDOR_B,     "animal",    "Cóndor (vuela)", "Con las alas bien abiertas, planeando", "El mirador") \
    X(ICO_ZONA_CLARO,   "zona",      "Claro de Tilo", "El claro de las luciérnagas: la casita de Tilo y su poza", "Mapa; flechas; diario") \
    X(ICO_ZONA_MIRADOR, "zona",      "Mirador", "La cima del cerro, sobre la cueva: se ve todo el valle", "Mapa; flechas; diario") \
    X(ICO_CACH_CHAPI, "visita", "Chapi (de pie)", "Cachorro de Pochi (niño), el cochino amoroso: del color de su mamá, collar verde", "Pochi") \
    X(ICO_CACH_CHAPI_B, "visita", "Chapi (mueve la cola)", "Cachorro de Pochi (niño), el cochino amoroso: del color de su mamá, collar verde", "Pochi") \
    X(ICO_CACH_CHAPI_C, "visita", "Chapi (trota 1)", "Cachorro de Pochi (niño), el cochino amoroso: del color de su mamá, collar verde", "Pochi") \
    X(ICO_CACH_CHAPI_D, "visita", "Chapi (trota 2)", "Cachorro de Pochi (niño), el cochino amoroso: del color de su mamá, collar verde", "Pochi") \
    X(ICO_CACH_CHAPI_S, "visita", "Chapi (sentado)", "Cachorro de Pochi (niño), el cochino amoroso: del color de su mamá, collar verde", "Pochi") \
    X(ICO_CACH_PASITA, "visita", "Pasita (de pie)", "Cachorrita de Pochi (niña), tierna: la más blanca, collar rosado", "Pochi") \
    X(ICO_CACH_PASITA_B, "visita", "Pasita (mueve la cola)", "Cachorrita de Pochi (niña), tierna: la más blanca, collar rosado", "Pochi") \
    X(ICO_CACH_PASITA_C, "visita", "Pasita (trota 1)", "Cachorrita de Pochi (niña), tierna: la más blanca, collar rosado", "Pochi") \
    X(ICO_CACH_PASITA_D, "visita", "Pasita (trota 2)", "Cachorrita de Pochi (niña), tierna: la más blanca, collar rosado", "Pochi") \
    X(ICO_CACH_PASITA_S, "visita", "Pasita (sentada)", "Cachorrita de Pochi (niña), tierna: la más blanca, collar rosado", "Pochi") \
    X(ICO_CACH_MORITA, "visita", "Morita (de pie)", "Cachorrita de Pochi (niña), tierna y tímida: un poco más clara que su mamá, collar morado", "Pochi") \
    X(ICO_CACH_MORITA_B, "visita", "Morita (mueve la cola)", "Cachorrita de Pochi (niña), tierna y tímida: un poco más clara que su mamá, collar morado", "Pochi") \
    X(ICO_CACH_MORITA_C, "visita", "Morita (trota 1)", "Cachorrita de Pochi (niña), tierna y tímida: un poco más clara que su mamá, collar morado", "Pochi") \
    X(ICO_CACH_MORITA_D, "visita", "Morita (trota 2)", "Cachorrita de Pochi (niña), tierna y tímida: un poco más clara que su mamá, collar morado", "Pochi") \
    X(ICO_CACH_MORITA_S, "visita", "Morita (sentada)", "Cachorrita de Pochi (niña), tierna y tímida: un poco más clara que su mamá, collar morado", "Pochi") \
    X(ICO_CACH_RONY, "visita", "Rony (de pie)", "Cachorro de Pochi (niño), el más loco, juega con cosas chiquitas: del color de su mamá, sin collar", "Pochi") \
    X(ICO_CACH_RONY_B, "visita", "Rony (mueve la cola)", "Cachorro de Pochi (niño), el más loco, juega con cosas chiquitas: del color de su mamá, sin collar", "Pochi") \
    X(ICO_CACH_RONY_C, "visita", "Rony (trota 1)", "Cachorro de Pochi (niño), el más loco, juega con cosas chiquitas: del color de su mamá, sin collar", "Pochi") \
    X(ICO_CACH_RONY_D, "visita", "Rony (trota 2)", "Cachorro de Pochi (niño), el más loco, juega con cosas chiquitas: del color de su mamá, sin collar", "Pochi") \
    X(ICO_CACH_RONY_S, "visita", "Rony (sentado)", "Cachorro de Pochi (niño), el más loco, juega con cosas chiquitas: del color de su mamá, sin collar", "Pochi") \
    X(ICO_HIELO, "tesoro", "Trocito de hielo", "Un trocito de hielo que pescó en el lago congelado: es de su colección de cristales y fuera del invierno se derrite", "Pesca en invierno; colección; diario") \
    X(ICO_PAPELITO, "historia", "Papelito del bosque", "Un papel doblado con letra antigua: una carta del Rey o una pista, escondido como tesoro del día", "Lo escondido de hoy; Libro; diario")

#define ART_ICONO_ENUM(id, g, n, s, u) id,
/* después de los íconos con nombre vienen los 150 objetos mágicos (ICO_OBJ0 + id, ver objetos.h): se dibujan
 * por familia con sus dos colores y también se pueden cambiar en el editor */
#define ART_OBJETOS 150
typedef enum { ART_ICONOS(ART_ICONO_ENUM) ICO_OBJ0, ICO_COUNT = ICO_OBJ0 + ART_OBJETOS } art_icono_t;
#define ICO_OBJETO(id) ((art_icono_t)(ICO_OBJ0 + (id)))

typedef struct {
    const char *id, *grupo, *nombre, *significado, *uso;
} art_icono_info_t;
const art_icono_info_t *art_icono_info(art_icono_t ico);

/* mismas estaciones que estacion_t del motor */
typedef enum {
    ESTACION_ART_PRIMAVERA = 0,
    ESTACION_ART_VERANO,
    ESTACION_ART_OTONO,
    ESTACION_ART_INVIERNO,
    ESTACION_ART_COUNT
} art_estacion_t;

void art_img_free(art_img_t *img);
/* un ícono dibujado en una imagen propia (para listas largas: no pasa por el caché compartido) */
void art_icono_en(art_img_t *img, art_icono_t ico, int escala);
/* estilo del mundo: 0 el de siempre, 1 el nuevo (verdes profundos, más tonos, sombras, pinos); se redibuja solo */
void art_set_estilo(int estilo);
int art_estilo(void);

/* Fondo del mundo (RGB565, 466x466) para un período del día, las decoraciones
 * compradas (bits de decor_t), la estación y si el día está gris (nublado o lluvia).
 * Se genera al cambiar algo y queda en caché. */
/* escena: 0 prado, 1 bosque, 2 lago, 3 colina, 4 cueva (escena_t). senderos: caminos marcados del prado
 * (bit por par de lugares, como pet_trail_mask). tinte: 0 normal, 1 gris, 2 helada. */
const lv_image_dsc_t *art_fondo(art_periodo_t periodo, uint32_t decoraciones, art_estacion_t estacion, int tinte,
                                int escena, uint32_t senderos);
void art_fondo_rapido(int on); /* (pruebas) 1: graduar por paleta; 0: pixel por pixel */
#define ART_ESCENAS 9
#define ART_ESCENA_CASA 8 /* adentro de la casita (su camita); las otras 8 son escena_t (5 la isla, 6 el claro de Tilo, 7 el mirador) */
/* dónde queda cada lugar (lugar_t) en cada zona, en coordenadas de pantalla (pies de la mascota) */
extern const int16_t ART_LUGAR_XY[ART_ESCENAS][6][2];

/* Dibuja una criatura en dst (ARGB8888). especie puede traer todo el aspecto (pet_look: mezcla y color).
 * etapa: 1 bebé, 2 cría, 3 adulta (stage_t).
 * frame 0/1 para animar; espejo mira a la izquierda. Tamaño: 20*escala x 20*escala. */
void art_criatura(art_img_t *dst, int especie, int etapa, int forma, int escala, art_expresion_t e, bool boca_abierta,
                  int frame, bool espejo, bool enferma);
/* Lenguaje corporal: además de la cara, orejas/crestas, cola y postura.
 * orejas: -1 caídas, 0 normales, 1 alertas. cola: -1 baja, 0 quieta, 1 se mueve.
 * postura: -2 echada, -1 encogida, 0 normal, 1 erguida. */
typedef struct {
    int especie, etapa, forma, escala;
    art_expresion_t expr;
    bool boca;
    int frame;
    bool espejo, enferma;
    int orejas, cola, postura;
    int ropa[4]; /* lo que lleva puesto: ropa_t + 1 en cabeza, cara, cuello y mano (0 = nada) */
    int sucio;   /* 0 limpia; 1..3 manchas de barro en el cuerpo (3: muy sucia, opaca) */
    int capa;    /* 1: la capa blanca élfica de la Guardiana del Mundo */
} art_pose_t;
void art_criatura_pose(art_img_t *dst, const art_pose_t *pose);
/* tamaño de las criaturas en todas partes: 100 (normal), 120, 130 o 140 %. Se aplica al dibujar cada pose (una vez; después queda en caché). */
void art_tamano_set(int pct);
int art_tamano_get(void);
/* la ropa mágica cambia con el ambiente: pañuelo según la estación (0..3), corona que brilla de noche, flor que se abre al sol */
void art_set_ambiente(int estacion, bool noche, bool sol);
void art_set_casa(bool luz, int cuadro, int icono); /* adentro de la casita: luz encendida y cuadro de la pared (0-5 zona, 6 ballena de nubes, 7 animal amigo con su ícono) */
uint32_t art_ambiente_clave(void);

/* ---------------------------------------------------------------------------
 * Criaturas propias (desde el editor): el cuerpo de una especie en una etapa, dibujado a mano en
 * 30x30 pixeles (los del editor antiguo, de 20x20, siguen sirviendo y se ven como antes). La cara
 * (ojos, boca, mejillas), las patitas, la ropa, la postura y el saltito al caminar se le ponen igual
 * que a las de fábrica: no hay que animar nada.
 * ------------------------------------------------------------------------- */
#define ART_CRIATURA_LADO_C 30  /* la grilla del editor */
#define ART_CRIATURA_LADO_V1 20 /* la del editor antiguo */
typedef struct {
    uint32_t px[ART_CRIATURA_LADO_C * ART_CRIATURA_LADO_C]; /* ARGB, lado x lado; alfa < 128 es transparente; sin contorno */
    int8_t ojo_x, ojo_y; /* ojo izquierdo (el de la izquierda en el dibujo), en pixeles del dibujo */
    int8_t ojos_sep;     /* distancia al otro ojo (de 4 a 15 en 30x30; de 3 a 10 en 20x20) */
    uint8_t patitas;     /* 1: dibujar patitas que caminan debajo */
    uint8_t lado;        /* 30, o 20 si viene del editor antiguo */
} art_cuerpo_t;
/* NULL vuelve al de fábrica. etapa: 1 bebé, 2 cría, 3 adulta */
void art_criatura_propia(int especie, int etapa, const art_cuerpo_t *c);
bool art_criatura_es_propia(int especie, int etapa);
/* El cuerpo de fábrica (sin cara, patitas ni contorno) y dónde van sus ojos: para empezar a editar */
void art_criatura_fabrica(int especie, int etapa, art_cuerpo_t *c);
/* Formato del editor (archivo y protocolo): "CUER", versión 2, ojo_x, ojo_y, ojos_sep, patitas, 3 ceros y
 * 30x30 pixeles R, G, B, A (3612 bytes). También acepta la versión 1 (20x20, 1612 bytes). false si no sirve. */
#define ART_CUERPO_BYTES (12 + ART_CRIATURA_LADO_C * ART_CRIATURA_LADO_C * 4)
#define ART_CUERPO_BYTES_V1 (12 + ART_CRIATURA_LADO_V1 * ART_CRIATURA_LADO_V1 * 4)
bool art_cuerpo_desde_bytes(const uint8_t *b, int n, art_cuerpo_t *c);
/* Cambia cada vez que se pone o se quita una criatura propia (para redibujar) */
uint32_t art_criaturas_version(void);

/* Huevo con manchas de la especie; grietas 0..3; inclinacion -1, 0 o 1 (se mece) */
void art_huevo(art_img_t *dst, int especie, int escala, int grietas, int inclinacion);
/* personajes nuevos (en prueba): el bebé de cada especie y su huevo gris con una pista */
void art_bebe_nuevo(art_img_t *dst, int especie, int escala, art_expresion_t e, int frame);
void art_huevo_nuevo(art_img_t *dst, int especie, int escala);
/* Piedrita de recuerdo con una estrella, cuando la mascota muere */
void art_recuerdo(art_img_t *dst, int especie, int escala);
#define ART_CRIATURA_LADO 20

/* Íconos (12x12 pixeles por escala), en caché */
const lv_image_dsc_t *art_icono(art_icono_t ico, int escala);
uint32_t art_iconos_soltar(bool (*en_uso)(const void *src)); /* (v96) suelta los íconos guardados que no se usan; bytes liberados */
const lv_image_dsc_t *art_icono_espejo(art_icono_t ico, int escala); /* mirando a la izquierda */
/* Íconos propios (desde el editor de Windows): 24x24 pixeles ARGB8888, ya con contorno (los del editor
 * antiguo, de 12x12, también sirven). px = NULL vuelve al dibujo de fábrica. Lo que está en pantalla se
 * actualiza solo. */
#define ART_ICONO_LADO 24
#define ART_ICONO_LADO_V1 12
void art_icono_propio(art_icono_t ico, const uint32_t *px, int lado);
/* 24 o 12 según el largo de un ícono en RGBA (0 si no es ninguno) */
int art_icono_lado_de_bytes(int n);
bool art_icono_es_propio(art_icono_t ico);
void art_icono_pixeles(art_icono_t ico, uint32_t px[ART_ICONO_LADO * ART_ICONO_LADO]); /* el que se usa */
void art_icono_fabrica(art_icono_t ico, uint32_t px[ART_ICONO_LADO * ART_ICONO_LADO]); /* el original */

/* Texto en fuente pixel propia (mayúsculas, números, tildes, ñ, ¡¿).
 * Corta líneas en ancho_max_px (0 = sin límite). */
void art_texto(art_img_t *dst, const char *utf8, uint32_t color, int escala, int ancho_max_px);
void art_texto_centrado(art_img_t *dst, const char *utf8, uint32_t color, int escala, int ancho_max_px); /* cada renglón centrado */
int art_texto_ancho(const char *utf8, int escala);
void art_texto_minusculas(bool si); /* (1.0.13) los textos siguientes muestran las minúsculas tal cual (claves de Wi-Fi) */
void art_texto_a8(bool si); /* (v97) los textos siguientes salen en A8 (el color lo pone image_recolor de la imagen) */
void art_texto_factor(float f); /* (v96) cuánto más grande se pinta la letra (1.05 = un 5 %); 1 = como antes, pixel exacto */

/* ---------------------------------------------------------------------------
 * El viaje de despedida: se va a explorar el mundo en avión, barco, tren o globo.
 * La mascota se dibuja ANTES que el vehículo y asoma por arriba (cabina, cubierta, vagón, canasto).
 * ------------------------------------------------------------------------- */
typedef enum { VIAJE_AVION = 0, VIAJE_BARCO, VIAJE_TREN, VIAJE_GLOBO, VIAJE_COUNT } art_viaje_t;
/* frame 0/1: hélice, humo, ruedas o fuego. Mira a la derecha. */
void art_vehiculo(art_img_t *dst, art_viaje_t v, int frame, int escala);
/* dónde van los pies de la mascota (centro), en pixeles de la imagen del vehículo */
void art_vehiculo_asiento(art_viaje_t v, int escala, int *x, int *y);
/* el paisaje (cielo, mar, vía o campo), 468x468: ponerlo en (-1, -1). Pesa: liberarlo al terminar. */
void art_viaje_fondo(art_img_t *dst, art_viaje_t v);
/* la pantalla de bienvenida: fondo (466x466), borde del nido (con transparencia) y huevo; false si no se pudo */
bool art_noche(art_img_t *fondo);
void art_cristales_fondo(art_img_t *dst);
void art_dragon_pintado(art_img_t *dst, int fase, int esc); /* esc 1: 96 px, 2: 192 px */
bool art_vivero(art_img_t *fondo, art_img_t *frente);
bool art_bienvenida(art_img_t *fondo, art_img_t *frente, art_img_t *huevo);
/* el cruce a la isla: el lago visto de lado, con la orilla a la izquierda y la isla a la derecha (466x466) */
void art_cruce_estacion(int e); /* estación (0..3) del próximo fondo del cruce; 3 = hielo */
void art_cruce_fondo(art_img_t *dst, int periodo);
/* fondos de pantalla como foto (0 elegir huevo, 1 juego de la fruta, 2 picnic: capa con transparencia). NULL si no hay: se pinta con formas. */
const lv_image_dsc_t *art_pantalla(int id, int *x, int *y);
void art_pantalla_liberar(int id);
/* (1.0.17) los fondos de los juegos de a dos viven en la MEMORIA DE RECURSOS (/res/pjN.bin), no en el programa.
 * art_lector: quién lee esos archivos (main: hal_res_read). art_fondo_dos(n): el fondo n (0..3) o NULL si falta. */
#define ART_PANT_DOS 16 /* las pantallas 16..19 son los fondos de los juegos de a dos */
void art_lector(int (*leer)(const char *nombre, void *buf, int cap), int (*tam)(const char *nombre));
const lv_image_dsc_t *art_fondo_dos(int n, int *x, int *y);
bool art_fondo_dos_hay(int n);
void art_pantalla_en_uso_fn(bool (*fn)(int id)); /* (v97) la que está en pantalla no se suelta al hacer lugar */
void art_fondos_soltar_otros(void); /* suelta los fondos de las otras horas del día (poca PSRAM) */
bool art_fondo_parcial(void); /* (101b) el fondo de ahora quedó sin adornos (sin memoria): devuelve true y lo deja para rehacer */
void art_fondo_sin_lienzo(int on); /* (105) de prueba: hace como si no hubiera memoria para el lienzo entero (fondos por franjas) */
int art_fondo_sin_lienzo_ver(void);
void art_fondo_rehacer(void); /* (105) rehace el fondo de la zona de ahora */
const lv_image_dsc_t *art_pantalla_clima(int id, int periodo, int estacion, int tinte, int *x, int *y);
const lv_image_dsc_t *art_cristales_fondo565(void);
void art_cristales_fondo565_liberar(void);
/* el bote con su velita, de lado: inflada 0 (floja) o 1 (llena de viento). Tamaño 40x30 por escala;
 * asiento: dónde van los pies de la mascota (en pixeles de la imagen) */
void art_bote_vela(art_img_t *dst, int inflada, int escala);
void art_bote_vela_asiento(int escala, int *x, int *y);
#define ART_CRUCE_AGUA_Y 246
/* el paseo por el firmamento: el cielo de fondo (466x466) y lo que pasa: 0 planeta con anillos, 1 la luna,
 * 2 un cometa, 3 un planetita rojo, 4 una nube (para subir y bajar). Tamaño 40x40 por escala */
void art_firmamento_fondo(art_img_t *dst);
void art_astro(art_img_t *dst, int tipo, int escala); /* 0 anillos, 1 luna, 2 cometa, 3 rojo, 4 nube, 5 gigante rayado, 6 luciérnaga */
void art_ballena(art_img_t *dst, int cuadro);
#define ART_BALLENA_ASIENTO_X 116
#define ART_BALLENA_ASIENTO_Y 46
/* tu cumpleaños: el claro del bosque de noche y el Árbol de los años (una flor por cumpleaños, la última brilla) */
void art_claro_fondo(art_img_t *dst);
void art_arbol_anios(art_img_t *dst, int flores, int escala);
/* la carta de un amigo: 0 cerrada con su sello de cera, 1 el sello partido, 2 abierta, 3..6 la hoja saliendo */
void art_carta(art_img_t *dst, int fase, int escala);
void art_carta_tipo(int t); /* el color del sello: 0 amigo, 1 familia, 2 viajero, 3 criatura */
/* el dragón de cristal (el secreto de la cueva): fase 0 dormido, 1 despierto, 2 con las alas abiertas. 44x40 por escala */
void art_dragon_cristal(art_img_t *dst, int fase, int escala);
/* altura (en pantalla) del agua o de los rieles en el paisaje */
#define ART_VIAJE_AGUA_Y 300
#define ART_VIAJE_RIEL_Y 356

/* Color principal de cada especie (para detalles de la interfaz) */
uint32_t art_color_especie(int especie);
