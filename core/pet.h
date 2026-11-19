/*
 * Motor de la mascota virtual — API pública.
 *
 * C99 portable, sin memoria dinámica, sin dependencias del hardware.
 * El mismo código corre en el reloj (ESP32-S3) y en el simulador de PC.
 *
 * Modelo de tiempo: el motor recibe la hora absoluta (segundos Unix) y
 * avanza la simulación desde la última actualización. Así funciona igual
 * si el chip estuvo despierto o durmió varias horas.
 *
 * Modelo emocional (versión 5):
 *   necesidades (8)  ->  emociones mezcladas (10), cada una con su causa
 *                    ->  estado de ánimo visible (expresión, postura, sonido)
 *   + impulsos de lo que pasa (jugar, un trueno, un hallazgo) que se desvanecen
 *   + personalidad que cambia según cómo la tratan
 *   + memoria afectiva que se olvida si no se refuerza (lugares, comidas,
 *     partes del cuerpo, clima, el vínculo con el dueño)
 * La mascota no muere: si la abandonan se pone triste, te extraña y te recibe
 * con alegría al volver.
 */
#ifndef PET_H
#define PET_H

#include <stdbool.h>
#include <stdint.h>
#include "objetos.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- Necesidades (0 = crítico, 100 = satisfecho) ---------- */
typedef enum {
    NEED_SACIEDAD = 0, /* hambre invertida: 100 = llena                    */
    NEED_ENERGIA,      /* sueño invertido: 100 = descansada                 */
    NEED_HIGIENE,      /* 100 = limpia                                      */
    NEED_DIVERSION,    /* 100 = entretenida                                 */
    NEED_CARINO,       /* afecto: 100 = se siente querida                   */
    NEED_SALUD,        /* 100 = sana                                        */
    NEED_CURIOSIDAD,   /* 100 = vio cosas nuevas; baja con la rutina        */
    NEED_SEGURIDAD,    /* 100 = tranquila; baja con truenos, sustos, extraños */
    NEED_COUNT
} need_t;

/* ---------- Emociones (0..1, pueden convivir varias) ---------- */
typedef enum {
    EMO_ALEGRIA = 0,
    EMO_TRISTEZA,
    EMO_ENOJO,
    EMO_MIEDO,
    EMO_CURIOSIDAD,
    EMO_ABURRIMIENTO,
    EMO_CANSANCIO,
    EMO_ORGULLO,
    EMO_CELOS,
    EMO_CARINO,
    EMO_COUNT
} emo_t;

/* ---------- Por qué siente lo que siente ---------- */
typedef enum {
    CAUSA_NINGUNA = 0,
    CAUSA_JUGAMOS,
    CAUSA_NO_JUGASTE,
    CAUSA_CARICIAS,
    CAUSA_TE_EXTRANA,
    CAUSA_HAMBRE,
    CAUSA_COMIDA_RICA,
    CAUSA_COMIDA_FEA,
    CAUSA_SUENO,
    CAUSA_SUCIA,
    CAUSA_ENFERMA,
    CAUSA_LLUVIA,
    CAUSA_SOL,
    CAUSA_NIEVE,
    CAUSA_VIENTO,
    CAUSA_TORMENTA,
    CAUSA_NUBLADO,
    CAUSA_VISITA,
    CAUSA_AMIGO,
    CAUSA_TOQUE_FEO,      /* la tocaste como no le gusta */
    CAUSA_SACUDIDA,
    CAUSA_HALLAZGO,
    CAUSA_CRECIO,
    CAUSA_MIMASTE_OTRO,   /* celos */
    CAUSA_NOVEDAD,
    CAUSA_LUGAR_FEO,      /* se asustó una vez en este lugar */
    CAUSA_VOLVISTE,
    CAUSA_NADA_QUE_HACER,
    CAUSA_LA_DESPERTASTE,
    CAUSA_BANO,
    CAUSA_EXPLORAR,
    CAUSA_FRIO,
    CAUSA_CALOR,
    CAUSA_BEBIDA,         /* la bebida caliente o fresca que le diste */
    CAUSA_MARIPOSA,
    CAUSA_ABEJA,          /* la picó una abeja */
    CAUSA_PAJARO,
    CAUSA_ZORRO,          /* el zorro le robó */
    CAUSA_ESPANTASTE,     /* espantaste al zorro */
    CAUSA_JARDIN,         /* sus flores o su arbolito */
    CAUSA_LUGAR,          /* le gusta (o no) la zona donde está */
    CAUSA_CHARCO,
    CAUSA_HOJAS,
    CAUSA_VECINO,         /* vino su amigo del barrio */
    CAUSA_PESCA,
    CAUSA_CUMPLE,         /* es tu cumpleaños */
    CAUSA_TRUCO,          /* aprendió o hizo un truco */
    CAUSA_INCLINAR,       /* la hiciste deslizar inclinando el reloj */
    CAUSA_TESORO,         /* encontró un tesoro de su colección */
    CAUSA_RARO,           /* vio algo raro y bonito (arcoíris, estrella fugaz...) */
    CAUSA_META,           /* cumplieron la meta del día */
    CAUSA_SU_CUMPLE,      /* es su cumpleaños */
    CAUSA_FECHA,          /* una fecha especial */
    CAUSA_HABILIDAD,      /* aprendió a nadar o a trepar */
    CAUSA_ROPA,           /* lo que le pusiste (le encanta o no) */
    CAUSA_JUGUETE,        /* jugó con uno de sus juguetes */
    CAUSA_FALTA_CARINO,   /* hace rato que no le haces cariño (estando tú) */
    CAUSA_CARTA,          /* le llegó una carta de un amigo */
    CAUSA_CRIATURA,       /* jugó con una criatura mágica */
    CAUSA_ESPECIAL,       /* descubrió el secreto de una zona */
    CAUSA_COUNT
} causa_t;

/* ---------- Estado de ánimo visible (resumen para la cara y el cuerpo) ---------- */
typedef enum {
    MOOD_FELIZ = 0,
    MOOD_TRANQUILA,
    MOOD_HAMBRIENTA,
    MOOD_SOMNOLIENTA,
    MOOD_SUCIA,
    MOOD_ABURRIDA,
    MOOD_TRISTE,
    MOOD_ENOJADA,
    MOOD_ENFERMA,
    MOOD_ENAMORADA, /* cariñosa */
    MOOD_DORMIDA,
    MOOD_CURIOSA,
    MOOD_ASUSTADA,
    MOOD_ORGULLOSA,
    MOOD_CELOSA,
    MOOD_COUNT
} mood_t;

/* ---------- Personalidad (0..1; nace con ella y cambia según el trato) ---------- */
typedef enum {
    TRAIT_GLOTONA = 0, /* le da hambre más rápido, ama la comida             */
    TRAIT_ENERGICA,    /* juguetona: gasta energía y se aburre más rápido      */
    TRAIT_CARINOSA,    /* necesita y da más cariño; más celosa                 */
    TRAIT_TIMIDA,      /* se asusta con facilidad (0 = valiente)               */
    TRAIT_PULCRA,      /* le gusta el baño y le molesta estar sucia            */
    TRAIT_CURIOSA,     /* explora, olfatea, encuentra cosas                    */
    TRAIT_TRAVIESA,    /* cava, a veces no obedece, trae cosas raras           */
    TRAIT_COUNT
} trait_t;

/* ---------- Acciones del dueño ---------- */
typedef enum {
    ACT_ALIMENTAR = 0, /* arg: food_t                                   */
    ACT_BANAR,
    ACT_JUGAR,
    ACT_ACARICIAR,     /* arg: gesto_t (cómo la tocaste)                 */
    ACT_DORMIR,        /* apagar la luz                                  */
    ACT_DESPERTAR,
    ACT_CURAR,         /* darle una hierba medicinal                     */
    ACT_SACUDIR,       /* gesto detectado por la IMU                     */
    ACT_CEPILLAR,
    ACT_MIMAR_VISITA,  /* acariciar a la visita (¡celos!)                */
    ACT_LLAMAR,        /* tocar dos veces el prado: arg = lugar_t más cercano */
    ACT_IR,            /* llevarla a otra zona: arg = escena_t        */
    ACT_PLANTAR,       /* plantar una semilla: arg = parcela del jardín */
    ACT_REGAR,         /* regar: arg = parcela del jardín             */
    ACT_ESPANTAR,      /* tocar a un animal: arg = fauna_t (espanta al zorro) */
    ACT_PESCAR,        /* tocar el bote en el lago: sale a pescar     */
    ACT_INVITAR_VECINO,/* llamar a su amigo del barrio                */
    ACT_LANZAR,        /* le tiras la pelota: +1 la trae, 0 va pero la deja, 2 se la queda (traviesa), -1 no va */
    ACT_INCLINAR,      /* inclinaste el reloj y se deslizó                 */
    ACT_TRUCO,         /* le pides un truco: arg = truco_t; +1 lo hizo, 0 no entendió */
    ACT_FIN_JUEGO,     /* terminó un minijuego: arg = juego_t * 100 + puntaje */
    ACT_VESTIR,        /* ponerle ropa: arg = ropa_t; quitarla: arg = -(ranura_t + 1) */
    ACT_PONER_JUGUETE, /* poner un juguete en el prado: arg = juguete_t | (x/2) << 8 | (y/2) << 16; x/2 = 255: guardarlo */
    ACT_USAR_JUGUETE,  /* tocas un juguete del prado: la invitas a jugar con él (arg = juguete_t) */
    ACT_LIMPIAR,       /* recoges una caca del prado */
    ACT_SALPICAR,      /* tocas un charco: la invitas a saltar en él (arg = charco 0 o 1) */
    ACT_ARRANCAR,      /* sacas una planta del jardín (arg = parcela) */
    ACT_ANIMAL,        /* tocas a uno de los animales del bosque (arg = animal_t) */
    ACT_COUNT
} action_t;

/* ---------- Fase 5: creatividad ---------- */
/* Ropa y accesorios: uno por ranura. A cada mascota algunos le encantan y otros no. */
typedef enum { RANURA_CABEZA = 0, RANURA_CARA, RANURA_CUELLO, RANURA_MANO, RANURA_COUNT } ranura_t; /* la mano (paraguas, molinillo...) se guarda aparte: ropa_mano */
#define RANURAS_VIEJAS 3 /* las que están en ropa_puesta[] */
typedef enum {
    ROPA_MONO = 0,     /* cabeza */
    ROPA_PANUELO,      /* cuello */
    ROPA_GORRO_LANA,   /* cabeza: abriga */
    ROPA_BUFANDA,      /* cuello: abriga */
    ROPA_SOMBRERO,     /* cabeza: sombra con calor */
    ROPA_LENTES,       /* cara */
    ROPA_CORONA,       /* cabeza: corona de flores */
    ROPA_COLLAR,       /* cuello: collar de conchas */
    ROPA_NENUFAR,      /* cabeza: un nenúfar con su florcita (cuando ya conoce el lago) */
    ROPA_PARAGUAS,     /* mano: lo sostiene abierto sobre su cabeza (con lluvia la divierte) */
    ROPA_MOLINILLO,    /* mano: un molinillo chiquito que gira con el viento */
    ROPA_FAROLITO,     /* mano: un farolito de papel que brilla de noche */
    ROPA_COUNT
} ropa_t;
#define ROPA_VIEJAS 8 /* las que tienen su gusto guardado (gusto_ropa[]); las demás se calculan */
/* Juguetes: se ponen en el prado donde quieras (hasta PET_JUGUETES_PUESTOS) y los usa sola */
typedef enum { JUG_PELUCHE = 0, JUG_PELOTON, JUG_CAJA, JUG_COLUMPIO, JUG_TAMBOR, JUG_MOLINETE, JUG_COUNT } juguete_t;
#define PET_JUGUETES_PUESTOS 3
#define PET_JUGUETE_AFUERA_S (10u * 60u) /* cuánto tiempo se queda un juguete en el prado */
/* Qué se planta: cada semilla es de un tipo, y los tipos se van encontrando con el tiempo */
typedef enum { CULTIVO_FLOR = 0, CULTIVO_GIRASOL, CULTIVO_FRUTILLA, CULTIVO_HIERBA, CULTIVO_COUNT } cultivo_t;
/* Sonido ambiente según la zona, la hora y el clima */
typedef enum {
    AMB_NADA = 0, AMB_PRADO, AMB_BOSQUE, AMB_LAGO, AMB_CUEVA, AMB_NOCHE, AMB_LLUVIA, AMB_TORMENTA, AMB_VIENTO, AMB_MAR, AMB_COUNT
} ambiente_t;
#define PET_COLORES 8 /* 0 = el de su especie; 1..7 tintes */

/* ---------- Fase 4: vínculo, habilidades, recuerdos y sorpresas ---------- */
typedef enum { VINCULO_DESCONFIA = 0, VINCULO_TE_CONOCE, VINCULO_AMIGOS, VINCULO_MEJORES, VINCULO_INSEPARABLES, VINCULO_COUNT } vinculo_nivel_t;
typedef enum { HAB_NADAR = 0, HAB_TREPAR, HAB_COUNT } habilidad_t;

/* Colecciones de tesoros que solo ella huele (cada pieza es un bit) */
typedef enum { COL_PIEDRAS = 0, COL_CONCHAS, COL_PLUMAS, COL_HOJAS, COL_CRISTALES, COL_COUNT } coleccion_t;
#define PET_PIEZAS_MAX 5
int pet_collection_size(coleccion_t c);           /* piezas de esa colección */
const char *pet_piece_name(coleccion_t c, int pieza);

/* Cosas raras que pasan muy de vez en cuando */
typedef enum { RARO_ARCOIRIS = 0, RARO_ESTRELLA_FUGAZ, RARO_CIERVO, RARO_GLOBO, RARO_COUNT } raro_t;

/* Metas suaves: una por día, sin castigo; al cumplirla, una estampita */
typedef enum { META_PELOTA = 0, META_FRUTA, META_REGAR, META_TRUCO, META_CARICIAS, META_PASEO, META_PESCAR, META_BANO, META_COUNT } meta_t;

/* Decoraciones de fiesta: las que existen en el arte. Cada celebración usa una de ellas
 * (calabazas, arbolito con regalo, fuegos artificiales, corazones, flores o banderines). */
typedef enum { FECHA_NINGUNA = 0, FECHA_HALLOWEEN, FECHA_NAVIDAD, FECHA_ANIO_NUEVO, FECHA_SAN_VALENTIN, FECHA_PRIMAVERA,
               FECHA_FIESTAS_PATRIAS, FECHA_COUNT } fecha_t;

/* Celebraciones configurables (el editor puede cambiar fechas y agregar otras, siempre con una
 * decoración que ya existe). No son parte de la partida: son ajustes del reloj. */
typedef enum {
    CEL_FIJA = 0,   /* un día del año: dia/mes (si dura varios días, puede cruzar el año) */
    CEL_NESIMO,     /* el n-ésimo día de la semana del mes: dia = 1..4 (5 = el último), semana = 0 domingo..6 sábado */
    CEL_PRIMAVERA,  /* el Día de la Primavera (21/9 en el sur, 21/3 en el norte) */
    CEL_REGLAS
} cel_regla_t;
#define PET_CELEBRACIONES 16
typedef struct {
    uint8_t regla;   /* cel_regla_t */
    uint8_t mes;     /* 1..12 */
    uint8_t dia;
    uint8_t semana;
    uint8_t dias;    /* cuántos días dura: 1..7 */
    uint8_t deco;    /* fecha_t (no FECHA_NINGUNA) */
    uint8_t _pad[2];
    char nombre[32]; /* para el diario: "Fiestas Patrias" */
    char saludo[40]; /* el globo de ese día: "¡FELICES FIESTAS PATRIAS!" */
} celebracion_t;

/* Diario: lo importante de cada día */
typedef enum {
    DIA_NACIO = 0, DIA_CRECIO, DIA_ZONA, DIA_TRUCO, DIA_HABILIDAD, DIA_VECINO, DIA_PESCA, DIA_TESORO, DIA_COLECCION,
    DIA_RARO, DIA_META, DIA_CUMPLE_DUENO, DIA_SU_CUMPLE, DIA_FECHA, DIA_VINCULO, DIA_REGALO, DIA_AMIGO,
    DIA_NUEVO, DIA_ESTRENO, DIA_ROPA, DIA_SEMILLA, DIA_OBJETO, DIA_REGALO_VUELTA,
    DIA_BALLENA, DIA_GUARDIANA, DIA_RITUAL, DIA_SEMANA, DIA_FAMILIA, DIA_BAILE, DIA_CARTA, DIA_ANIMAL,
    DIA_ANIMAL_NUEVO, /* a = ficha (pet_animal_card) */
    DIA_CRIATURA,     /* a = magica_t, b = cita_t que hicieron (-1: solo se conocieron) */
    DIA_HISTORIA,     /* a = magica_t, b = cuál de sus historias: se relee tocándola */
    DIA_ESPECIAL,     /* a = especial_t: descubrió el secreto de la zona */
    DIA_FIESTA,       /* a = fecha_t (o FIESTA_SU_CUMPLE), b = cuántos amigos vinieron */
    DIA_FINAL,        /* a = 0 el final regular, 1 el secreto */
    DIA_MINI,         /* a = mini_t, b = detalle (cuál equinoccio/solsticio) */
    DIA_ESTACION,     /* a = estacion_t, b = 1 su favorita, 2 la que menos le gusta */
    DIA_CANCION,      /* b = tempo (0 lenta, 1 media, 2 rápida): encontró su canción favorita */
    DIA_GUSTO_SEM,    /* a = actividad que más hizo la semana pasada, b = cuántas veces (máx. 127) */
    DIA_MELODIA,      /* compuso su propia melodía con los cristales de la cueva */
    DIA_GUSTO,        /* a = actividad (actividad_t) que descubrió que le encanta; b = cuál de las frases */
    DIA_NOTA_TINTA,   /* a = cuál de las notas escondidas reveló con el tintero */
    DIA_COFRE,        /* abrió el cofre del árbol viejo */
    DIA_TILDE,        /* Tilde: a = 0 se llevó, 1 devolvió; b = objeto ((uint8_t)) */
    DIA_LUGAR,        /* encontró un lugar secreto: a = 0 la casita de Tilo, 1 el mirador */
    DIA_NUESTRA,      /* (v38) compusieron una canción: a = cuál (0 nuestra canción, 1..4 el cuaderno) */
    DIA_EXCURSION,    /* (v40) fue sola de paseo: a = 0 el claro, 1 el mirador; b = lo que trajo (PET_ZH_*; 101b: 4..6 = PET_ZH_* + 3, se lo quedó) */
    DIA_PASEO_JUNTOS, /* (fw 104) salieron a caminar de verdad: a = pasos / 100 (tope 255) */
    DIA_HIELO,        /* (fw 104) a = 1 el trocito de hielo se derritió (encontrarlo queda en DIA_TESORO) */
    DIA_JUEGO_DOS,    /* (1.0.17) jugó con una visita: a = PET_DOS_* (el juego), b = 0 perdió, 1 empate, 2 ganó */
    DIA_JUEGO_ISLA,   /* (1.0.18) un juego de la isla: a = PET_ISLA_* (faro o buceo), b = puntos (hasta 127) */
    DIA_COUNT
} diario_t;
#define PET_DIARIO 32
#define PET_ALBUM 16
typedef struct {
    uint32_t t;
    uint8_t tipo; /* diario_t */
    int8_t a, b;
    uint8_t _pad;
} entrada_diario_t;
/* Una foto del álbum: no es una imagen, es la escena para volver a dibujarla */
typedef struct {
    uint32_t t;
    uint8_t motivo;            /* diario_t que la causó */
    int8_t a;                  /* detalle (zona, truco, colección, fecha...) */
    uint8_t escena, hora, estacion, especie, etapa, forma;
    uint8_t emocion;           /* mood_t */
    uint8_t vecino_especie;    /* especie | color << 3 del amigo que estaba (0xFF = ninguno) */
    uint8_t _pad[2];
} foto_t;

/* Lo vivido desde que despertó (para soñar con eso) */
typedef enum {
    VIV_PELOTA = 0, VIV_PESCA, VIV_VECINO, VIV_MARIPOSA, VIV_ZORRO, VIV_LLUVIA, VIV_NIEVE, VIV_TRUCO, VIV_NADAR, VIV_TREPAR,
    VIV_TESORO, VIV_FRUTA, VIV_CARICIAS, VIV_BOSQUE, VIV_LAGO, VIV_COLINA, VIV_CUEVA, VIV_ARCOIRIS, VIV_CUMPLE, VIV_FLORES,
    VIV_ESCONDIDAS, VIV_JUEGO_FRUTAS, VIV_PAJARO, VIV_JUGUETE, VIV_ROPA, VIV_COUNT
} vivido_t;

/* Juegos (el botón Jugar abre el menú; ACT_JUGAR lleva el juego en arg) */
typedef enum { JUEGO_PELOTA = 0, JUEGO_ESCONDIDAS, JUEGO_FRUTAS, JUEGO_HONGOS, JUEGO_COUNT } juego_t;

/* Trucos que se aprenden practicando (y se olvidan si no se practican) */
/* (en partidas viejas los dos primeros eran sentarse y la patita: lo practicado se conserva) */
typedef enum { TRUCO_DORMIDA = 0, TRUCO_VOLTERETA, TRUCO_VUELTA, TRUCO_SALUDO, TRUCO_COUNT } truco_t;
#define PET_TRUCO_APRENDIDO 60 /* desde aquí "lo sabe" y a veces lo hace sola */

/* Cómo la tocas (la pantalla es chica: se reconocen gestos, no partes del cuerpo).
 * A cada una le gustan distinto; casi a ninguna le gustan muchos toques seguidos. */
typedef enum {
    GESTO_TOQUE = 0, /* una palmadita: un toque corto                  */
    GESTO_CARICIA,   /* deslizar el dedo sobre ella                     */
    GESTO_MUCHOS,    /* muchos toques rápidos seguidos (la marea)       */
    GESTO_ABRAZO,    /* mantener el dedo encima un momento              */
    GESTO_COUNT
} gesto_t;

typedef enum {
    FOOD_COMIDA = 0, /* nutritiva y siempre disponible  */
    FOOD_FRUTA,      /* sana; cae de los árboles         */
    FOOD_MIEL,       /* dulce: divierte, poco sana       */
    FOOD_BEBIDA_CALIENTE, /* chocolate caliente: abriga con frío */
    FOOD_BEBIDA_FRIA,     /* jugo con hielo: refresca con calor  */
    FOOD_COUNT
} food_t;

/* ---------- Lo que pide al dueño ---------- */
typedef enum {
    WANT_NADA = 0,
    WANT_COMER,
    WANT_DORMIR,
    WANT_BANO,
    WANT_JUGAR,
    WANT_CARINO,
    WANT_MEDICINA,
    WANT_BEBIDA_CALIENTE, /* tiene frío */
    WANT_BEBIDA_FRIA,     /* tiene calor */
    WANT_COUNT
} want_t;

/* Mini eventos de temporada: pasan en fechas reales o con el clima de su estación, y los vive con ella */
typedef enum { MINI_ESTRELLAS = 0, MINI_PRIMERA_LLUVIA, MINI_PRIMERA_NIEVE, MINI_LUCIERNAGAS, MINI_SOL, MINI_LUNA, MINI_ECLIPSE_LUNAR, MINI_ECLIPSE_SOLAR, MINI_COUNT } mini_t;

/* ---------- Lo que hace por su cuenta (autonomía) ---------- */
typedef enum {
    ACTV_PASEAR = 0,
    ACTV_OLFATEAR,
    ACTV_CAVAR,
    ACTV_SIESTA,
    ACTV_EXPLORAR,
    ACTV_MIRAR,       /* contempla la lluvia, las nubes, el estanque */
    ACTV_JUGAR_SOLA,
    ACTV_REFUGIO,     /* se esconde porque tiene miedo              */
    ACTV_SOMBRA,      /* busca sombra en verano                     */
    ACTV_NIEVE,       /* juega en la nieve                          */
    ACTV_ESPERAR,     /* te espera donde sueles acariciarla         */
    ACTV_ABRIGARSE,   /* se acurruca para no pasar frío             */
    ACTV_PERSEGUIR,   /* persigue una mariposa, una abeja o al pájaro */
    ACTV_PESCAR,      /* en el bote del lago, con su caña             */
    ACTV_JUNTOS,      /* con su amigo del barrio (ver vecino_t)       */
    ACTV_NADAR,       /* nada en el lago (aprende con la práctica)    */
    ACTV_TREPAR,      /* trepa un árbol (aprende con la práctica)     */
    ACTV_JUGUETE,     /* juega con un juguete del prado (juguete_uso)  */
    ACTV_CHAPOTEAR,   /* chapotea en charcos, el estanque o la orilla  */
    ACTV_PELOTA,      /* patea y persigue una pelota                   */
    ACTV_PROBAR,      /* se prueba una ropa o un objeto que tiene (probando) */
    ACTV_BUSCAR,      /* husmea arbustos y árboles buscando comida     */
    ACTV_AVES,        /* mira los pájaros con atención                 */
    ACTV_CHARLAR,     /* conversa con un animal de la zona (probando = animal_t + 1) */
    ACTV_CANTAR,      /* canta por su cuenta                           */
    ACTV_BAILAR,      /* baila (sola, o porque suena música cerca)     */
    ACTV_COUNT
} actividad_t;

/* ---------- Carácter, planes y trato (la capa de intención sobre lo que hace por su cuenta) ---------- */
/* El humor con que amanece cada día: inclina lo que le apetece hacer, sin mandar sobre lo urgente */
typedef enum { HUMOR_NORMAL = 0, HUMOR_AVENTURERA, HUMOR_PEREZOSA, HUMOR_SOCIAL, HUMOR_JUGUETONA, HUMOR_SONADORA, HUMOR_COUNT } humor_t;
/* Planes de tres pasos con sentido: ir a un lugar y hacer cosas que se siguen una a otra */
typedef enum {
    PLAN_NINGUNO = 0,
    PLAN_LAGO,           /* al lago: pescar, chapotear y la siesta */
    PLAN_BOSQUE_BUSCAR,  /* al bosque: busca, olfatea y cava */
    PLAN_BOSQUE_TREPAR,  /* al bosque: trepa, mira los pájaros y descansa */
    PLAN_COLINA_TESORO,  /* a la colina: explora, olfatea y cava un tesoro */
    PLAN_ESTRELLAS,      /* de noche: mira el cielo, canta y se duerme */
    PLAN_DESFILE,        /* se prueba algo, baila y espera que la veas */
    PLAN_JUEGO,          /* pelota, juega sola y descansa */
    PLAN_TERTULIA,       /* charla con un animal, mira aves y contempla */
    PLAN_EXTRANA,        /* te extraña: pasea, mira a lo lejos y te espera */
    PLAN_COUNT
} plan_t;
/* ---------- Lugares del prado (la interfaz los ubica en la pantalla) ---------- */
typedef enum {
    LUGAR_CLARO = 0,
    LUGAR_ESTANQUE,
    LUGAR_ARBOL,
    LUGAR_ROCAS,
    LUGAR_SENDERO,
    LUGAR_CASITA, /* junto a la casita (o los arbustos si no hay casita) */
    LUGAR_COUNT
} lugar_t;

/* ---------- Zonas del mundo: se abren con el tiempo y la confianza ---------- */
/* la isla del lago se agregó después: se llega en el bote (con el dueño) y no por las flechas */
typedef enum { ESC_PRADO = 0, ESC_BOSQUE, ESC_LAGO, ESC_COLINA, ESC_CUEVA, ESC_ISLA, ESC_CLARO, ESC_MIRADOR, ESC_COUNT } escena_t;
/* (v41) ESC_CLARO: el claro de las luciérnagas con la casita de Tilo (se abre con el lugar secreto PET_ZONA_CLARO);
 * ESC_MIRADOR: la cima del cerro, justo sobre la cueva (se abre con el lugar secreto 1). Solo se va contigo (o de excursión). */
#define ESC_ES_SECRETA(e) ((e) == ESC_CLARO || (e) == ESC_MIRADOR)
#define ESC_GUSTOS 5 /* zonas con gusto guardado (las cinco primeras); el de la isla se calcula */

/* ---------- Fauna ---------- */
typedef enum { FAUNA_MARIPOSA = 0, FAUNA_ABEJA, FAUNA_PAJARO, FAUNA_ZORRO, FAUNA_RANA, FAUNA_COUNT } fauna_t;
/* Más animales del bosque (versión 22), cada uno con su manera de ser */
#define ANIMAL_V22 8 /* los de la versión 22 (los demás guardan su hora en animal_hasta2) */
/* de noche, cuánto más seguido vienen los animales nocturnos */
#define PET_NOCTURNOS_X 2.5f
#define PET_MELODIA_NOTAS 8   /* cuántas notas tiene su melodía */
#define PET_MELODIA_ENSAYOS 4 /* cuántos días ensaya en la cueva antes de decidirla */
typedef enum {
    ANIMAL_ERIZO = 0, /* tímido: se hace bolita si ella corre; si ella espera quieta, se desenrolla y se hacen amigos */
    ANIMAL_ARDILLA,   /* traviesa: le roba una semilla y la esconde; a veces la desentierra y se la devuelve */
    ANIMAL_CONEJO,    /* asustadizo y juguetón: juegan a perseguirse; se espanta con la tormenta */
    ANIMAL_CARACOL,   /* calmado: sale después de la lluvia y deja un rastro brillante que ella sigue */
    ANIMAL_TORTUGA,   /* sabia (rara): si ella se sienta a su lado, se le pasa la pena */
    ANIMAL_PATO,      /* curioso: la sigue en fila por el lago */
    ANIMAL_BUHO,      /* sereno (de noche): ulula desde el árbol; si ella está despierta, la arrulla */
    ANIMAL_TOPO,      /* gruñón (en la colina): asoma de un hoyo y se esconde si lo tocas */
    /* versión 23: treinta más */
    ANIMAL_COLIBRI, ANIMAL_CARPINTERO, ANIMAL_MARTIN, ANIMAL_GOLONDRINA, ANIMAL_CISNE, ANIMAL_URRACA, ANIMAL_CUERVO, ANIMAL_GARZA,
    ANIMAL_MAPACHE, ANIMAL_MURCIELAGO, ANIMAL_NUTRIA, ANIMAL_CASTOR, ANIMAL_RATON, ANIMAL_OSO, ANIMAL_CIERVO, ANIMAL_TEJON, ANIMAL_ZORRILLO,
    ANIMAL_GATO_MONTES, ANIMAL_LINCE, ANIMAL_PUMA,
    ANIMAL_LAGARTIJA, ANIMAL_SALAMANDRA, ANIMAL_CAMALEON, ANIMAL_CULEBRA,
    ANIMAL_MARIQUITA, ANIMAL_LIBELULA, ANIMAL_GRILLO, ANIMAL_HORMIGAS, ANIMAL_ESCARABAJO, ANIMAL_POLILLA,
    ANIMAL_PINGUINO, ANIMAL_FOCA, ANIMAL_OSO_POLAR, /* los del frío: solo vienen al lago con invierno, nieve o frío */
    ANIMAL_COUNT
} animal_t;
typedef enum {
    ANI_LLEGA = 0, ANI_SE_VA, ANI_BOLITA, ANI_AMIGOS, ANI_ROBA, ANI_DEVUELVE, ANI_CORREN, ANI_SE_ASUSTA, ANI_RASTRO,
    ANI_CONSUELA, ANI_EN_FILA, ANI_ULULA, ANI_ARRULLA, ANI_SE_ESCONDE,
    ANI_HACE, /* hace lo suyo (los de la versión 23): el carpintero toc-toc, el cuervo trae algo brillante... */
    ANI_SUCESO_COUNT
} animal_suceso_t;
typedef enum {
    FSUC_LLEGA = 0,
    FSUC_SE_VA,
    FSUC_PERSIGUE,    /* ella la persigue y se divierte          */
    FSUC_PICO,        /* la abeja la picó                         */
    FSUC_EVITA,       /* aprendió: ya no persigue abejas          */
    FSUC_ROBO,        /* el zorro se llevó una fruta              */
    FSUC_ESPANTADO,   /* tú espantaste al zorro                   */
    FSUC_LO_ECHO,     /* ella misma echó al zorro                 */
    FSUC_REGALO,      /* el pájaro le trajo una semilla           */
    FSUC_COUNT
} fauna_suceso_t;

/* ---------- Jardín del prado ---------- */
#define PET_PARCELAS 6
typedef enum { PLANTA_NADA = 0, PLANTA_FLOR, PLANTA_ARBOL, PLANTA_GIRASOL, PLANTA_FRUTILLA, PLANTA_HIERBA } planta_tipo_t;
typedef enum { ETAPA_SEMILLA = 0, ETAPA_BROTE, ETAPA_GRANDE, ETAPA_FRUTOS } planta_etapa_t;
typedef enum { JAR_PLANTO = 0, JAR_CRECIO, JAR_FLORECIO, JAR_MUSTIA, JAR_REVIVIO, JAR_FRUTO, JAR_ENTERRO,
               JAR_SECA,     /* pasó mucho sin agua: se secó (regándola todavía revive) */
               JAR_PERDIDA,  /* se secó del todo y ya no está (en modo niño e infante no pasa) */
               JAR_ARRANCO,  /* la sacaste */
               JAR_VIVA,     /* quisiste sacarla, pero está viva: solo se sacan las que se secaron */
               JAR_VIEJA,    /* (versión 25) cumplió su vida: se marchitó sola y ya no revive; se puede sacar */
} jardin_suceso_t;
typedef struct {
    uint8_t tipo;    /* planta_tipo_t  */
    uint8_t etapa;   /* planta_etapa_t */
    uint8_t mustia;  /* le falta agua: no crece hasta regarla */
    uint8_t color;   /* color de la flor */
    float agua;      /* 100 recién regada; 0 mustia; bajo PET_AGUA_SECA seca; en PET_AGUA_PERDIDA se pierde */
    float crece;     /* horas creciendo con agua */
} planta_t;
#define PET_AGUA_SECA (-40.0f)
#define PET_VIDA_PLANTA_H (14.0f * 24.0f) /* (versión 25) ya grande, una flor vive unas dos semanas */
#define PET_VIDA_ARBOL_H (90.0f * 24.0f)  /* y un árbol, una estación */
#define PET_AGUA_PERDIDA (-100.0f)
/* Jardines: 6 parcelas en el prado y 3 en el bosque, el lago y la colina */
#define PET_PARCELAS_ZONA 3
#define PET_PARCELAS_TOTAL (PET_PARCELAS + 3 * PET_PARCELAS_ZONA)

/* ---------- Cosas que encuentra ---------- */
typedef enum {
    HALL_FRUTA = 0,
    HALL_MIEL,
    HALL_HIERBA,
    HALL_SEMILLA,
    HALL_PIEDRA,
    HALL_RAMITA, /* una ramita: (v38) con el musgo se hace la casita; si ya la tiene, te la trae igual */
    HALL_CHAPITA,   /* otra "basura": una chapita de botella */
    HALL_HOJA_SECA, /* y una hoja seca */
    HALL_MUSGO,     /* (v38) un puñado de musgo: con las ramitas se hace la casita */
    HALL_COUNT
} hallazgo_t;

/* ---------- Etapas de vida ---------- */
typedef enum {
    STAGE_HUEVO = 0, /* antes de nacer  */
    STAGE_BEBE,      /* días 0 a 3       */
    STAGE_CRIA,      /* días 3 a 4       */
    STAGE_ADULTA,
    STAGE_COUNT
} stage_t;

/* ---------- Estaciones y clima ---------- */
typedef enum { ESTACION_PRIMAVERA = 0, ESTACION_VERANO, ESTACION_OTONO, ESTACION_INVIERNO, ESTACION_COUNT } estacion_t;
typedef enum { CLIMA_DESPEJADO = 0, CLIMA_NUBLADO, CLIMA_LLUVIA, CLIMA_VIENTO, CLIMA_NIEVE, CLIMA_TORMENTA, CLIMA_FRIO, CLIMA_COUNT } clima_t;

#define PET_HUEVO_SEGUNDOS 300   /* el huevo eclosiona a los 5 minutos */
#define PET_HUEVO_CALOR_S 15     /* cada caricia al huevo lo adelanta 15 s */
#define PET_NOMBRE_MAX 10        /* letras del nombre */
#define PET_NAME_LEN 16          /* bytes del nombre (UTF-8 y el cero final) */
#define PET_EDAD_INFANTE 7       /* menores de esta edad: modo infante (todavía no leen) */
#define PET_EDAD_NINO 12         /* menores de esta edad: modo niño (de 7 a 11) */

/* Modo de juego según la edad del dueño */
typedef enum {
    MODO_ADULTO = 0,  /* 13 años o más: todo completo */
    MODO_NINO = 1,    /* 7 a 12: textos cortos, consecuencias suaves */
    MODO_INFANTE = 2, /* 3 a 6 (o menos): sin textos, sin enfermedades ni "perder" */
} modo_juego_t;

/* ---------- Sonidos ----------
 * Catálogo único: id, grupo, nombre, qué significa y cuándo suena. De aquí salen el enum,
 * los nombres del simulador y docs/sonidos.md / .json (para el editor de Windows, que podrá
 * reemplazar cada sonido por uno propio). sfx.c los sintetiza. */
#define PET_SONIDOS(X) \
    X(SND_NINGUNO,      "-",       "Ninguno",          "Silencio", "-") \
    X(SND_ALEGRE,       "animo",   "Alegre",           "Dos notas que suben: está contenta", "Le gustó lo que hiciste; despierta descansada; nace; va a una zona que le gusta") \
    X(SND_RISA,         "animo",   "Risa",             "Risita juguetona", "Jugar; sacudida que le gusta; saltar en las hojas; muchos toques si le hacen cosquillas") \
    X(SND_RONRONEO,     "animo",   "Ronroneo",         "Ronroneo grave: está a gusto", "Caricia que le encanta; cepillarla; la bebida justa") \
    X(SND_QUEJA,        "animo",   "Queja",            "Nota que baja: no le gustó", "Comida que odia; toque que no le gusta; algo que no quería") \
    X(SND_LLANTO,       "animo",   "Llanto",           "Lloriqueo: está triste", "Muy triste o te extraña") \
    X(SND_GRUNIDO,      "animo",   "Gruñido",          "Gruñido: está enojada o celosa", "No quiso hacer algo; la despertaste; mimaste a otro") \
    X(SND_BOSTEZO,      "cuerpo",  "Bostezo",          "Tiene sueño", "Se duerme; está muy cansada para jugar") \
    X(SND_RONQUIDO,     "cuerpo",  "Ronquido",         "Está durmiendo", "La tocas mientras duerme") \
    X(SND_ESTORNUDO,    "cuerpo",  "Estornudo",        "Se resfrió o le dio frío", "Se enferma; una bebida fría cuando tiene frío") \
    X(SND_TOS,          "cuerpo",  "Tos",              "Está enferma", "Quieres jugar y está enferma") \
    X(SND_COMER,        "accion",  "Comer",            "Ñam ñam", "Come") \
    X(SND_CHAPOTEO,     "accion",  "Chapoteo",         "Agua", "Baño; regar; charcos") \
    X(SND_SORPRESA,     "animo",   "Sorpresa",         "Nota que sube rápido: algo nuevo", "Visita; algo nuevo; se aparta porque todavía desconfía; zona nueva") \
    X(SND_SUSPIRO_AMOR, "animo",   "Suspiro de amor",  "Está enamorada", "Se enamora de una visita") \
    X(SND_PELEA,        "accion",  "Pelea",            "Ruido de pelea", "Pelea con una visita") \
    X(SND_LLAMADO,      "accion",  "Llamado",          "Te llama: necesita algo", "Pide comida, baño, juego, mimos o medicina") \
    X(SND_TRUENO,       "mundo",   "Trueno",           "Trueno de la tormenta", "Tormenta") \
    X(SND_OLFATEO,      "accion",  "Olfateo",          "Olfatea", "Empieza a olfatear") \
    X(SND_CAVAR,        "accion",  "Cavar",            "Escarba la tierra", "Cava; plantas una semilla") \
    X(SND_SUSPIRO,      "animo",   "Suspiro",          "Suspiro aburrido o triste", "Aburrida o triste") \
    X(SND_TOQUE,        "accion",  "Palmadita",        "Un boop suave", "Le das una palmadita (un toque)") \
    X(SND_ABRAZO,       "animo",   "Abrazo",           "Sonido cálido y largo", "La abrazas (mantener el dedo sobre ella)") \
    X(SND_MAREO,        "animo",   "Mareo",            "Sonido que da vueltas: la mareaste", "Muchos toques seguidos") \
    X(SND_HOLA_AMIGO,   "mundo",   "Llega su amigo",   "Saludo de su amigo del barrio", "Llega su amigo a visitarla") \
    X(SND_PLOP,         "mundo",   "Plop",             "El anzuelo cae al agua", "Empieza a pescar") \
    X(SND_PESCO,        "accion",  "¡Pescó!",          "Notas alegres: sacó algo del agua", "Termina de pescar y sacó algo") \
    X(SND_BIENVENIDA,   "musica",  "Bienvenida",       "Melodía corta de bienvenida", "La primera vez que se enciende (y al crear una mascota nueva)") \
    X(SND_CUMPLEANOS,   "musica",  "Cumpleaños feliz", "La melodía tradicional del cumpleaños (dominio público)", "Tu cumpleaños; al tocar el pastel") \
    X(SND_LANZAR,       "juego",   "Lanzar",           "Silbido: la pelota sale volando", "Tiras la pelota con el dedo") \
    X(SND_REBOTE,       "juego",   "Rebote",           "Boing de la pelota al rebotar", "La pelota toca el suelo") \
    X(SND_ATRAPA,       "juego",   "¡Atrapada!",       "Pop alegre: algo atrapado", "Atrapa la pelota; atrapas una fruta con la canasta") \
    X(SND_TRUCO,        "juego",   "Truco",            "Fanfarria corta: lo hizo bien", "Hace un truco") \
    X(SND_CONTAR,       "juego",   "Contar",           "Tres pitidos: uno, dos, tres", "Escondidas: cuentas mientras se esconde") \
    X(SND_ENCONTRADA,   "juego",   "¡Te encontré!",    "Ta-dá: la encontraste", "Escondidas: la encuentras") \
    X(SND_DESLIZA,      "juego",   "¡Wiii!",           "Se resbala divertida", "Inclinas el reloj y se desliza") \
    X(SND_TESORO,       "mundo",   "Tesoro",           "Campanitas: desenterró un tesoro", "Encuentra una pieza de su colección") \
    X(SND_HUELE,        "mundo",   "Huele algo",       "Olfateo con un pitido curioso: aquí hay algo", "Huele un tesoro escondido") \
    X(SND_RARO,         "mundo",   "Algo mágico",      "Brillitos que suben: pasa algo raro y bonito", "Arcoíris, estrella fugaz, pudú, globo") \
    X(SND_META,         "juego",   "Estampita",        "Ding de premio: cumplieron la meta", "Cumplen la meta del día") \
    X(SND_FIESTA,       "musica",  "Fiesta",           "Cornetita de fiesta", "Su cumpleaños; fechas especiales; colección completa") \
    X(SND_CHAPUZON,     "accion",  "Chapuzón",         "Se tira al agua", "Empieza a nadar") \
    X(SND_CAIDA,        "accion",  "¡Pum!",            "Golpecito suave al caer", "Se cae del árbol mientras aprende a trepar") \
    X(SND_ROPA,         "accion",  "Ropa",             "Frufrú de tela", "Le pones o le quitas ropa") \
    X(SND_NUEVO,        "mundo",   "Algo nuevo",       "Cajita que se abre: hay algo nuevo en el baúl", "Aparece ropa o un juguete nuevo") \
    X(SND_PELUCHE,      "juego",   "Peluche",          "Chillido suave de juguete", "Abraza su peluche") \
    X(SND_TAMBOR,       "juego",   "Tambor",           "Tres golpes de tambor", "Toca el tambor") \
    X(SND_COLUMPIO,     "juego",   "Columpio",         "Chirrido suave que va y viene", "Se columpia") \
    X(SND_FOTO,         "accion",  "Foto",             "Clic de cámara", "Le sacas una foto") \
    X(SND_BANO,         "accion",  "Baño",             "Agua, burbujas y un brillito: quedó limpia", "La bañas") \
    X(SND_CEPILLO,      "accion",  "Cepillo",          "Cepillado suave y ronroneo", "La cepillas") \
    X(SND_LIMPIO,       "accion",  "¡Limpio!",         "Tres notitas brillantes", "Quedó limpio (se fue la suciedad)") \
    X(SND_SALPICON,     "accion",  "Salpicón",         "Un chapoteo chiquito", "Cae en un charco al saltar") \
    X(SND_SEMILLA,      "mundo",   "Semilla nueva",    "Tres notas que suben: encontró un tipo de semilla nuevo", "Encuentra una semilla que todavía no tenía") \
    X(SND_DESPERTADOR,  "alarma",  "Despertador",      "Melodía alegre que se repite: ¡a despertar!", "La alarma de despertar (se repite hasta que respondes)") \
    X(SND_A_DORMIR,     "alarma",  "Hora de dormir",   "Canción de cuna corta y suave", "La alarma de irse a dormir") \
    X(SND_AVION,        "viaje",   "Viento mágico",    "Un soplo de viento con campanitas que suben", "Se va de viaje volando en una pluma gigante") \
    X(SND_BOCINA,       "viaje",   "Campana del barco", "Dos campanadas suaves sobre el agua", "Se va de viaje en un barquito de hoja por la laguna de estrellas") \
    X(SND_SILBATO,      "viaje",   "Silbato de cristal", "Un chu-chuu de cristal y chispitas", "Se va de viaje en el tren de luciérnagas") \
    X(SND_GLOBO,        "viaje",   "Diente de león",   "Un soplido suave y semillas que brillan", "Se va de viaje colgada de un diente de león gigante") \
    X(SND_CAJA_MUSICA,  "mundo",   "Caja de música",   "Una melodía chiquita de caja de música, a un pulso fijo para bailar", "Usa la caja de música antigua: ella baila") \
    X(SND_CRISTAL_AZUL, "cueva",   "Cristal azul",     "Una nota de cristal (do)", "Tocas el cristal azul de la cueva") \
    X(SND_CRISTAL_VERDE,"cueva",   "Cristal verde",    "Una nota de cristal (mi)", "Tocas el cristal verde de la cueva") \
    X(SND_CRISTAL_ROSA, "cueva",   "Cristal rosado",   "Una nota de cristal (sol)", "Tocas el cristal rosado de la cueva") \
    X(SND_CRISTAL_ORO,  "cueva",   "Cristal dorado",   "Una nota de cristal (do alto)", "Tocas el cristal dorado de la cueva") \
    X(SND_CRISTAL_LILA, "cueva",   "Cristal violeta",  "Una nota de cristal (re)", "Tocas el cristal violeta de la cueva") \
    X(SND_CRISTAL_BLANCO,"cueva",  "Cristal blanco",   "Una nota de cristal (la)", "Tocas el cristal blanco de la cueva") \
    X(SND_PRIMAVERA,    "mundo",   "Melodía de primavera", "Una melodía original, tocada con las notas de los cristales de la cueva", "Suena el 21 de septiembre, el Día de la Primavera") \
    X(SND_HADAS,        "magia",   "Hadas",            "Campanitas que giran: algo mágico de verdad", "Las hadas del anillo; la luz escondida de la cueva; una criatura mágica") \
    X(SND_ALETEO,       "magia",   "Aleteo",           "Alas que se acercan: llega un mensajero", "La paloma, la mariposa o la grulla traen una carta") \
    X(SND_COHETE_SILBA, "fuegos",  "Cohete: silbido",  "El cohete sube silbando", "Un fuego artificial despega") \
    X(SND_COHETE_BUM_P, "fuegos",  "Cohete chico",     "Un estallido pequeño y seco", "Estalla un fuego artificial chico") \
    X(SND_COHETE_BUM_M, "fuegos",  "Cohete mediano",   "Un estallido hondo", "Estalla un fuego artificial mediano") \
    X(SND_COHETE_BUM_G, "fuegos",  "Cohete grande",    "Un estallido profundo que retumba", "Estalla un fuego artificial grande") \
    X(SND_COHETE_CHISPAS,"fuegos", "Cohete: chispas",  "Crepitar final de las chispas que caen", "Las chispas de un fuego artificial caen") \
    X(SND_COHETE_LEJOS, "fuegos",  "Cohete lejano",    "Un estallido suave, de muy lejos", "Fuegos artificiales de otro lugar") \
    X(SND_VOZ_0,       "voz",     "Canto 1",        "Sílabas inventadas cantadas con voz tierna (tonadita 1 de la especie 1)", "Ella canta") \
    X(SND_VOZ_1,       "voz",     "Canto 2",        "Sílabas inventadas cantadas con voz tierna (tonadita 2 de la especie 1)", "Ella canta") \
    X(SND_VOZ_2,       "voz",     "Canto 3",        "Sílabas inventadas cantadas con voz tierna (tonadita 3 de la especie 1)", "Ella canta") \
    X(SND_VOZ_3,       "voz",     "Canto 4",        "Sílabas inventadas cantadas con voz tierna (tonadita 1 de la especie 2)", "Ella canta") \
    X(SND_VOZ_4,       "voz",     "Canto 5",        "Sílabas inventadas cantadas con voz tierna (tonadita 2 de la especie 2)", "Ella canta") \
    X(SND_VOZ_5,       "voz",     "Canto 6",        "Sílabas inventadas cantadas con voz tierna (tonadita 3 de la especie 2)", "Ella canta") \
    X(SND_VOZ_6,       "voz",     "Canto 7",        "Sílabas inventadas cantadas con voz tierna (tonadita 1 de la especie 3)", "Ella canta") \
    X(SND_VOZ_7,       "voz",     "Canto 8",        "Sílabas inventadas cantadas con voz tierna (tonadita 2 de la especie 3)", "Ella canta") \
    X(SND_VOZ_8,       "voz",     "Canto 9",        "Sílabas inventadas cantadas con voz tierna (tonadita 3 de la especie 3)", "Ella canta") \
    X(SND_VOZ_9,       "voz",     "Canto 10",        "Sílabas inventadas cantadas con voz tierna (tonadita 1 de la especie 4)", "Ella canta") \
    X(SND_VOZ_10,       "voz",     "Canto 11",        "Sílabas inventadas cantadas con voz tierna (tonadita 2 de la especie 4)", "Ella canta") \
    X(SND_VOZ_11,       "voz",     "Canto 12",        "Sílabas inventadas cantadas con voz tierna (tonadita 3 de la especie 4)", "Ella canta") \
    X(SND_VOZ_12,       "voz",     "Canto 13",        "Sílabas inventadas cantadas con voz tierna (tonadita 1 de la especie 5)", "Ella canta") \
    X(SND_VOZ_13,       "voz",     "Canto 14",        "Sílabas inventadas cantadas con voz tierna (tonadita 2 de la especie 5)", "Ella canta") \
    X(SND_VOZ_14,       "voz",     "Canto 15",        "Sílabas inventadas cantadas con voz tierna (tonadita 3 de la especie 5)", "Ella canta") \
    X(SND_VOZ_15,       "voz",     "Canto 16",        "Sílabas inventadas cantadas con voz tierna (tonadita 1 de la especie 6)", "Ella canta") \
    X(SND_VOZ_16,       "voz",     "Canto 17",        "Sílabas inventadas cantadas con voz tierna (tonadita 2 de la especie 6)", "Ella canta") \
    X(SND_VOZ_17,       "voz",     "Canto 18",        "Sílabas inventadas cantadas con voz tierna (tonadita 3 de la especie 6)", "Ella canta") \
    X(SND_VOZ_18,       "voz",     "Canto 19",        "Sílabas inventadas cantadas con voz tierna (tonadita 1 de la especie 7)", "Ella canta") \
    X(SND_VOZ_19,       "voz",     "Canto 20",        "Sílabas inventadas cantadas con voz tierna (tonadita 2 de la especie 7)", "Ella canta") \
    X(SND_VOZ_20,       "voz",     "Canto 21",        "Sílabas inventadas cantadas con voz tierna (tonadita 3 de la especie 7)", "Ella canta") \
    X(SND_VOZ_21,       "voz",     "Canto 22",        "Sílabas inventadas cantadas con voz tierna (tonadita 1 de la especie 8)", "Ella canta") \
    X(SND_VOZ_22,       "voz",     "Canto 23",        "Sílabas inventadas cantadas con voz tierna (tonadita 2 de la especie 8)", "Ella canta") \
    X(SND_VOZ_23,       "voz",     "Canto 24",        "Sílabas inventadas cantadas con voz tierna (tonadita 3 de la especie 8)", "Ella canta") \
    X(SND_TARAREO_0,   "voz",     "Tarareo 1",      "Tarareo con la boca cerrada (especie 1)", "Ella canta, en la segunda parte de su canción") \
    X(SND_TARAREO_1,   "voz",     "Tarareo 2",      "Tarareo con la boca cerrada (especie 2)", "Ella canta, en la segunda parte de su canción") \
    X(SND_TARAREO_2,   "voz",     "Tarareo 3",      "Tarareo con la boca cerrada (especie 3)", "Ella canta, en la segunda parte de su canción") \
    X(SND_TARAREO_3,   "voz",     "Tarareo 4",      "Tarareo con la boca cerrada (especie 4)", "Ella canta, en la segunda parte de su canción") \
    X(SND_TARAREO_4,   "voz",     "Tarareo 5",      "Tarareo con la boca cerrada (especie 5)", "Ella canta, en la segunda parte de su canción") \
    X(SND_TARAREO_5,   "voz",     "Tarareo 6",      "Tarareo con la boca cerrada (especie 6)", "Ella canta, en la segunda parte de su canción") \
    X(SND_TARAREO_6,   "voz",     "Tarareo 7",      "Tarareo con la boca cerrada (especie 7)", "Ella canta, en la segunda parte de su canción") \
    X(SND_TARAREO_7,   "voz",     "Tarareo 8",      "Tarareo con la boca cerrada (especie 8)", "Ella canta, en la segunda parte de su canción") \
    X(SND_POCHI_LADRA,   "pochi",   "Pochi ladra",      "Ladridos cortos y agudos: «ya-ya-yi»", "Pochi llega, saluda o la tocas") \
    X(SND_POCHI_PIDE,    "pochi",   "Pochi pide",       "Quejidito que sube: «iii-ya-yaa»", "Pochi espera sus papas") \
    X(SND_POCHI_COME,    "pochi",   "Pochi come",       "«ñam-ñam-ñam» con un ladridito", "Pochi come sus papas") \
    X(SND_POCHI_FELIZ,   "pochi",   "Pochi feliz",      "Ladridos rápidos y contentos: «ya-ya-ya-yiii»", "Pochi corre feliz o invita a pasear") \
    X(SND_POCHI_ADIOS,   "pochi",   "Pochi se despide", "Lloriqueo suave que baja: «iiu-uu»", "Pochi se despide") \
    X(SND_LA_0,          "voz",     "La (cristal azul)",    "Ella canta «la» en la nota del cristal azul (do)", "Toca contigo en los cristales; canta nuestra canción") \
    X(SND_LA_1,          "voz",     "La (cristal verde)",   "Ella canta «la» en la nota del cristal verde (mi)", "Toca contigo en los cristales; canta nuestra canción") \
    X(SND_LA_2,          "voz",     "La (cristal rosado)",  "Ella canta «la» en la nota del cristal rosado (sol)", "Toca contigo en los cristales; canta nuestra canción") \
    X(SND_LA_3,          "voz",     "La (cristal dorado)",  "Ella canta «la» en la nota del cristal dorado (do alto)", "Toca contigo en los cristales; canta nuestra canción") \
    X(SND_LA_4,          "voz",     "La (cristal violeta)", "Ella canta «la» en la nota del cristal violeta (re)", "Toca contigo en los cristales; canta nuestra canción") \
    X(SND_LA_5,          "voz",     "La (cristal blanco)",  "Ella canta «la» en la nota del cristal blanco (la)", "Toca contigo en los cristales; canta nuestra canción") \
    X(SND_DRAGON_TARAREA, "cueva",  "El dragón tararea",    "Un zumbido grave y redondo, como un ronroneo con música", "El dragón de cristal escucha lo que tocaste") \
    X(SND_CRISTAL_CELESTE, "cueva", "Cristal celeste",      "Una nota grave y redonda, como un bajo (do grave)", "Tocas el cristal celeste de la orilla izquierda de la cueva") \
    X(SND_CRISTAL_MORADO,  "cueva", "Cristal morado",       "Una nota aguda con brillo, como una campanita (re alto)", "Tocas el cristal morado de la orilla derecha de la cueva") \
    X(SND_CHUCAO,          "animal", "El chucao",            "«¡Chu-cao!»: dos notas fuertes, la segunda más grave, de un pajarito chiquito", "El chucao del claro avisa que hay algo escondido") \
    X(SND_CONDOR,          "animal", "El cóndor",            "Un soplo de viento largo bajo las alas y un graznido ronco, bajito", "El cóndor del mirador sale a planear")
#define PET_SONIDO_ENUM(id, g, n, s, c) id,
typedef enum { PET_SONIDOS(PET_SONIDO_ENUM) SND_COUNT } sound_t;
typedef struct {
    const char *id, *grupo, *nombre, *significado, *cuando;
} pet_sonido_info_t;
const pet_sonido_info_t *pet_sound_info(sound_t s);

/* ---------- Eventos para la interfaz (cola circular) ---------- */
typedef enum {
    EV_NINGUNO = 0,
    EV_EMOCION,        /* a = mood_t nuevo, b = emoción principal          */
    EV_PIDE,           /* a = want_t (la mascota pide algo)               */
    EV_SE_DURMIO,
    EV_DESPERTO,       /* a = 1 si la despertaste                          */
    EV_ENFERMO,
    EV_SANO,
    EV_REACCION,       /* a = action_t, b = 1 le gustó / 0 / -1 no / -2 no quiso hacerlo */
    EV_VISITA_LLEGA,   /* a = índice de visitante                         */
    EV_ENCUENTRO,      /* a = índice de visitante, b = encounter_t        */
    EV_VISITA_SE_VA,   /* a = índice de visitante                         */
    EV_CRECIO,         /* a = nueva etapa, b = forma                       */
    EV_SONIDO,         /* a = sound_t                                     */
    EV_REGALO,         /* cayó fruta del árbol: b = 0 la de cada día, 1 porque sacudiste el reloj */
    EV_SIN_ITEM,       /* quiso usar algo que no tiene; a = hallazgo_t    */
    EV_DECORO,         /* a = decor_t puesta en el prado                   */
    EV_NACIO,          /* salió del huevo                                 */
    EV_CLIMA,          /* a = clima_t nuevo                               */
    EV_FRASE,          /* mensaje del día: a = frase, b = mood_t actual   */
    EV_AMIGO_CERCA,    /* a = índice de amigo que entró en rango          */
    EV_INVITACION,     /* a = índice de amigo que nos invita a jugar      */
    EV_AMIGO_VISITA,   /* a = índice de amigo, b = amistad_t (resultado)  */
    EV_PENSAMIENTO,    /* a = emo_t principal, b = causa_t (globo de pensamiento) */
    EV_ACTIVIDAD,      /* a = actividad_t, b = lugar_t                     */
    EV_HALLAZGO,       /* a = hallazgo_t, b = 1 si te lo trae de regalo    */
    EV_REENCUENTRO,    /* volviste tras mucho tiempo; a = horas (máx. 127) */
    EV_TRUENO,
    EV_DESCUBRE,       /* mostró un gusto fuerte: a = gusto_tipo_t, b = id */
    EV_RASGO,          /* su personalidad cambió: a = trait_t, b = +1 / -1  */
    EV_ESCENA,         /* cambió de zona: a = escena_t, b = 1 si fue sola  */
    EV_ESCENA_NUEVA,   /* se abrió una zona nueva: a = escena_t            */
    EV_FAUNA,          /* a = fauna_t, b = fauna_suceso_t                  */
    EV_JARDIN,         /* a = parcela, b = jardin_suceso_t                 */
    EV_SALPICA,        /* salta en un charco: a = charco (0 o 1; +2 si la invitaste), b = 1 le encantó, -1 no */
    EV_HOJAS,          /* saltó en el montón de hojas                       */
    EV_VECINO,         /* su amigo del barrio: a = vecino_act_t, b = item_t del regalo */
    EV_PESCA,          /* terminó de pescar: a = pesca_t, b = 1 se lo comió */
    EV_CUMPLE,         /* tu cumpleaños: a = los años que cumples          */
    EV_TRUCO,          /* a = truco_t; b = 1 lo hizo sola, 2 ¡lo aprendió! */
    EV_VINCULO,        /* subió el vínculo: a = vinculo_nivel_t             */
    EV_HABILIDAD,      /* a = habilidad_t; b = -1 se cayó, 1 primer intento, 2 aprendió, 3 experta, 0 practicó */
    EV_TESORO,         /* a = 0 huele algo / 1 lo desenterró; b = coleccion_t * 8 + pieza (o -1 repetido) */
    EV_COLECCION,      /* completó una colección: a = coleccion_t           */
    EV_RARO,           /* a = raro_t; b = 1 empieza, 0 termina              */
    EV_META,           /* a = meta_t; b = 0 la meta de hoy, 1 ¡cumplida!, 2 avanzó */
    EV_SU_CUMPLE,      /* su cumpleaños: a = días (7) o meses; b = 1 si son años */
    EV_FECHA,          /* fecha especial: a = fecha_t                       */
    EV_FOTO,           /* se guardó una foto en el álbum: a = índice        */
    EV_ROPA,           /* a = ropa_t; b = 2 le encanta, 1 le gusta, 0 le da igual, -1 no le gusta,
                          -2 se la sacó, -3 se la sacó por el calor, 4 se la quitaste */
    EV_JUGUETE,        /* a = juguete_t; b = 0 juega sola, 1 lo estrena, 2 aceptó tu invitación,
                          -1 no quiso, 3 lo pusiste en el prado, 4 lo guardaste */
    EV_NUEVO,          /* algo nuevo en el baúl: a = 0 ropa / 1 juguete, b = ropa_t o juguete_t */
    EV_SEMILLA,        /* encontró una semilla: a = cultivo_t, b = 1 si es de un tipo nuevo */
    EV_ALARMA,         /* suena una alarma del dueño: a = cuál (0..PET_ALARMAS-1), b = 1 si es la pospuesta */
    EV_HORA_DORMIR,    /* a = 0 hora de acostarse, 1 hora de levantarse */
    EV_TEMPORIZADOR,   /* se acabó el tiempo del temporizador */
    EV_PASEO,          /* caminaron harto: a = hallazgo_t que trajo (-1: trajo un objeto mágico, viene en EV_OBJETO) */
    EV_OBJETO,         /* encontró un objeto mágico: objeto = a | (b & 1) << 7; b & 2 = es nuevo, b & 4 = te lo trae,
                          b >> 3 = de dónde (objeto_fuente_t) */
    EV_REGALO_VUELTA,  /* volviste y te trae algo de afuera: a = regalo_t */
    EV_NIEBLA,         /* a = 1 amanece con niebla, 0 se despejó (b = 1 si la despejaste tú) */
    EV_OFRENDA,        /* llevó un objeto al árbol hueco: objeto = a | (b & 1) << 7; b & 2 = el bosque lo bendecirá mañana */
    EV_HABILIDAD_USO, /* usó una de sus habilidades: a = especie de la habilidad, b = cuál (0 o 1) */
    EV_BENDICION,      /* el bosque agradece la ofrenda: a = 0 un día de sol, 1 luciérnagas de noche */
    EV_OBJETO_USO,     /* usó un objeto de la Bolsa: objeto = a | (b & 1) << 7; b >> 1 = resultado (pet_object_use) */
    EV_BOTELLA,        /* en la orilla de la isla llegó una botella con un mensaje: a = la frase (como EV_FRASE) */
    EV_GUARDIANA,      /* hizo el rito: ahora es la Guardiana del Mundo */
    EV_TE_ESPERABA,    /* llegaste a tu hora de siempre: "¡sabía que vendrías!" (a = hábito: 0 comer, 1 jugar, 2 mimos) */
    EV_RITUAL,         /* a = 0 buenos días, 1 buenas noches; b = 1 lo acaba de aprender, 0 lo repite */
    EV_ANECDOTA,       /* se acordó de algo que vivieron: a = índice en el diario */
    EV_FAMILIA,        /* dejó un huevo de familia (a = 0) o lo cuidaste en otro perfil (a = 1) */
    EV_PIDE_VIAJE,     /* despedida natural (opcional): siente ganas de explorar el mundo; te pregunta */
    EV_BAILE,          /* bailó con la caja de música: a = cuántas veces seguiste el ritmo */
    EV_CARTA,          /* le llegó una carta de un amigo del barrio: a = quién (índice del nombre) */
    EV_ANIMAL,         /* un animal del bosque: a = animal_t, b = animal_suceso_t */
    EV_LUNA_LLENA,     /* hoy en la noche hay luna llena (lo avisa una vez): a = 1 si tiene el Corazón y es adulta (pista) */
    EV_ANIMAL_NUEVO,   /* conoció contigo a un animal nuevo: a = ficha */
    EV_CRIATURA,       /* una criatura mágica: a = magica_t, b = criatura_suceso_t */
    EV_ESPECIAL,       /* el secreto de una zona: a = especial_t, b = especial_suceso_t */
    EV_PISTA,          /* comenta una pista del secreto de la zona: a = especial_t, b = cuál pista */
    EV_FIESTA,         /* celebraron con todos: a = fiesta */
    EV_EVO_LISTA,      /* ya puede crecer y espera tu decisión: a = la etapa a la que crecería */
    EV_MINI,           /* mini evento de temporada: a = mini_t, b = detalle */
    EV_ESTACION,       /* cambió la estación: a = estacion_t, b = 0 normal, 1 es su favorita, 2 es la que menos le gusta */
    EV_CANCION,        /* reconoció su canción: a = 0 la primera vez, 1 otra vez; b = tempo */
    EV_PIDE_CANCION,   /* te pide que pongas su canción */
    EV_ENSAYA_MELODIA, /* en la cueva prueba los cristales: a = en qué ensayo va (1..), b = 0..5 la primera nota que toca (el resto sale de la nota) */
    EV_MELODIA,        /* decidió su melodía: queda guardada para siempre (pet_melody) */
    EV_GUSTO,          /* descubrió su actividad favorita: a = actividad_t */
    EV_MUSICA,         /* suena música cerca del reloj y le encanta: a = tempo * 3 + cuál frase (0..8); b = tempo (0 lenta, 1 media, 2 rápida) */
    EV_COMIDA_EST,     /* encontró una comida de temporada: a = cuál (comida_est_t), b = 1 si te la trae */
    EV_PLAN,           /* un plan suyo: a = plan_t, b = 0 empieza, 1 lo cumplió, 2 lo dejó */
    EV_HUMOR,          /* amaneció con un humor: a = humor_t */
    EV_ANTOJO,         /* reacciona a tu trato: a = 0 pide más mimos, 1 te invita a jugar, 2 se enfurruña, 3 ya te perdonó */
    EV_HIST_NIMBA,     /* Nimba le contó una historia en el paseo: a = cuál (0..4) */
    EV_TILDE,          /* Tilde, la urraca: a = 0 llega, 1 se va (b = objeto que se lleva, -1 ninguno) */
    EV_BRIZNA,         /* la Abuela Brizna despierta en la isla: a = recuerdo (-1 solo saluda); b = 0 despierta, 1 se duerme */
    EV_PRISMA,         /* Prisma le contó un capítulo: a = cuál */
    EV_TILO,           /* una página del diario de Tilo volvió al Libro: a = cuál */
    EV_ANIMAL_DICE,    /* un animal que le tiene confianza le contó algo: a = ficha, b = cuál frase */
    EV_LUGAR,          /* un lugar secreto: a = cuál (0 la casita de Tilo, 1 el mirador); b = -1 se abrió, k = encontró lo k */
    EV_PIEZA,          /* (v38) terminó una pieza en los cristales: a = PIEZA_* (lo que pasa), b = objeto regalado + 1 (0 nada) */
    EV_NUESTRA,        /* (v38) guardaron una canción: a = cuál */
    EV_PAGINA,         /* (v39) la página suelta se desdobló y muestra una página ya leída: a = cuál (-1: borrosa, hoy no) */
    EV_CARTA_BOSQUE,   /* (fw 104) encontró un papelito del bosque: a = cuál; b = 1 lo trajo ella de su excursión */
    EV_PASEO_JUNTOS,   /* (fw 104) salieron a caminar de verdad: a = pasos / 100 (tope 127) */
    EV_NIEGA,          /* (fw 104) terca: no quiso; a = action_t (ACT_DORMIR = no se quiere dormir todavía) */
    EV_HIELO,          /* (fw 104) a = 1 su trocito de hielo se derritió */
    EV_HEDIONDA,       /* (1.0.18) quedó hedionda sola (no el zorrillo): a = por qué (0..5, ver ui); b = 0 empieza, 1 se le quitó soplándole o espantando el olor */
    EV_COUNT
} event_type_t;

typedef struct {
    uint8_t type; /* event_type_t */
    int8_t a;
    int8_t b;
    uint8_t _pad;
    uint32_t t;   /* hora del evento (Unix) */
} pet_event_t;

/* ---------- Objetos (lo que encuentra) y decoraciones ---------- */
typedef enum { ITEM_FRUTA = 0, ITEM_MIEL, ITEM_HIERBA, ITEM_SEMILLA, ITEM_PIEDRA, ITEM_COUNT } item_t;

typedef enum {
    DECOR_FLORES = 0,
    DECOR_HONGOS,
    DECOR_FAROL,
    DECOR_BANCO,
    DECOR_CASITA,
    DECOR_COUNT
} decor_t;

/* Formas al evolucionar (al llegar a adulta), según cómo fue cuidada */
typedef enum { FORMA_NORMAL = 0, FORMA_RADIANTE, FORMA_GRUNONA, FORMA_COUNT } forma_t;

/* Gustos que se descubren observándola */
typedef enum { GUSTO_COMIDA = 0, GUSTO_CLIMA, GUSTO_LUGAR, GUSTO_GESTO, GUSTO_TIPO_COUNT } gusto_tipo_t;

/* ---------- Su amigo del barrio (vive en el mismo reloj, no viene por radio) ---------- */
typedef enum {
    VEC_LLEGA = 0,
    VEC_SE_VA,
    VEC_JUGAR,   /* se tiran la pelota          */
    VEC_COMER,   /* comen juntos (picnic)       */
    VEC_CORRER,  /* se persiguen                */
    VEC_CHARLAR, /* se cuentan cosas            */
    VEC_SIESTA,  /* duermen una siesta juntos   */
    VEC_REGALO,  /* le trae algo                */
    VEC_COUNT,
    VEC_DORMIDA = VEC_COUNT /* pasó de visita y la encontró durmiendo (no cuenta como actividad) */
} vecino_act_t;
typedef struct {
    char nombre[PET_NAME_LEN];
    uint8_t especie, presente, actividad;
    uint8_t color;            /* 0..PET_COLORES-1 (como el aspecto de la mascota) */
    float amistad;            /* 0..100: con los amigos del barrio en general */
    uint16_t visitas;
    uint8_t regalo_hecho;
    uint8_t nombre_i;         /* índice en la lista de nombres (para el diario) */
    /* Por ahora cada visita es un amigo distinto, al azar. Más adelante cada uno podría tener
     * su personalidad y sus recuerdos: este es el lugar para guardarlos. */
    uint32_t proxima;         /* cuándo vuelve a visitarla */
    uint32_t hasta;           /* hasta cuándo se queda */
    uint32_t actividad_hasta;
    uint32_t se_fue;
} vecino_t;

/* ---------- Pesca en el lago ---------- */
typedef enum {
    PESCA_NADA = 0, PESCA_PEZ, PESCA_BOTA, PESCA_PIEDRA,
    PESCA_CONCHA,     /* va a su colección */
    PESCA_ALGA,       /* le da risa */
    PESCA_CANGREJO,   /* le pellizca la nariz y lo devuelve al agua */
    PESCA_NENUFAR,    /* una hoja de nenúfar: se la pone de sombrero */
    PESCA_BOTELLA,    /* una botella con un mensaje */
    PESCA_LLAVE,      /* una llave oxidada (objeto mágico) */
    PESCA_PEZ_DORADO, /* muy raro: lo mira, lo devuelve y queda en el diario */
    PESCA_HIELO,      /* (fw 104) un trocito de hielo: solo en invierno, pescando en el hielo; va a la colección de cristales */
    PESCA_COUNT
} pesca_t;
#define PESCA_DUR_MIN 40   /* segundos que dura salir a pescar (remar, tirar la caña, esperar y sacar algo) */
#define PESCA_DUR_MAX 90
#define PESCA_SOLA_DIA 2   /* sola pesca como mucho dos veces al día */

/* ---------- Visitantes ---------- */
typedef enum {
    ENC_JUGARON = 0,
    ENC_PELEARON,
    ENC_SE_ENAMORARON,
    ENC_SE_ASUSTO,
    ENC_SE_IGNORARON,
    ENC_COUNT
} encounter_t;

#define PET_MAX_VISITORS 8
#define PET_EVENT_QUEUE 64

typedef struct {
    uint8_t especie;            /* índice en la tabla de especies          */
    uint8_t conocido;           /* 0 = espacio libre                       */
    int8_t relacion;            /* -100 enemigos .. +100 inseparables      */
    uint8_t enamorados;         /* 1 si se enamoraron                      */
    uint8_t rasgos[TRAIT_COUNT];/* personalidad del visitante, 0..255      */
    uint8_t _pad0;
    uint16_t encuentros;
    uint16_t _pad;
} visitor_t;

/* ---------- Amigos cercanos (otros relojes) ----------
 * Cada reloj emite por radio una "baliza" corta con su mascota. Si dos relojes
 * se ven, aparecen como amigos cercanos; uno invita, el otro acepta, y ambos
 * calculan la misma visita a partir de un número compartido (token). */
#define PET_MAX_AMIGOS 6
#define PET_BALIZA_BYTES 23
#define PET_BALIZA_MAGIC 0xA7

typedef enum { AMI_JUGARON = 0, AMI_PICNIC, AMI_REGALO, AMI_BAILARON, AMI_ENAMORARON, AMI_COUNT } amistad_t;

typedef struct {
    uint32_t id;
    char nombre[PET_NOMBRE_MAX + 2];
    uint8_t especie, etapa, forma, animo;
    uint8_t amistad;      /* 0..100, se olvida de a poco si no se ven */
    uint8_t _pad;
    uint16_t visitas;
    uint32_t visto;       /* última vez que se recibió su baliza */
    uint32_t invito;      /* última invitación recibida de este amigo */
} amigo_t;

typedef struct {
    uint32_t id;
    uint32_t invita_a;    /* 0 = no invita a nadie */
    uint16_t token;
    uint8_t acepta;       /* 1 = responde "sí" a una invitación de invita_a */
    uint8_t especie, etapa, forma, animo;
    char nombre[10];      /* hasta 9 bytes UTF-8 */
} pet_baliza_t;

/* ---------- Hábitos del dueño ---------- */
#define HABIT_KINDS 4 /* alimentar, jugar, acariciar, bañar */

typedef struct {
    uint8_t prob[HABIT_KINDS][24]; /* 0..255 ~ probabilidad por hora */
    uint8_t gusto[FOOD_COUNT];     /* cuánto le gusta cada comida 0..255 (128 = indiferente) */
} habits_t;

/* ---------- Memoria afectiva (todo se desvanece si no se refuerza) ---------- */
typedef struct {
    float agrado[LUGAR_COUNT];   /* 0..1: la acariciaron o jugaron aquí */
    float miedo[LUGAR_COUNT];    /* 0..1: se asustó aquí */
    uint16_t visitas[LUGAR_COUNT];
    int8_t gesto[GESTO_COUNT];   /* -100..100: cómo le gusta que la toquen */
    int8_t clima[CLIMA_COUNT];   /* -100..100: qué clima le gusta */
    uint8_t descubierto[GUSTO_TIPO_COUNT]; /* bits: gustos que ya mostró al dueño */
    uint8_t _pad;
} memoria_t;

/* ---------- Estado completo (POD: se guarda tal cual en flash) ---------- */
#define PET_SAVE_MAGIC 0x4D415343u /* "MASC" */
/* Amigos cercanos por Bluetooth: apagado en el prototipo (el código está en firmware/main/radio.c, listo para
 * encenderlo con 1). Los amigos del barrio no dependen de esto: viven dentro del reloj. */
#ifndef MASCOTA_BLUETOOTH
#define MASCOTA_BLUETOOTH 0
#endif
/* 1: aprende qué actividades te gustan (gusto[]); 0: la versión sin aprendizaje (todo lo demás igual) */
#ifndef PET_APRENDE
#define PET_APRENDE 1
#endif
/* (versión 38) una canción compuesta con los cristales: cada nota es cristal (bits 0..2, 0..5) y duración en pasos (bits 3..5: 1..8 pasos) */
#define PET_CANCIONES 5
#define PET_CANCION_NOTAS 16
#define PET_CANCION_NOMBRE 21
typedef struct {
    uint8_t n;                                /* cuántas notas (0: no hay canción) */
    uint8_t nota[PET_CANCION_NOTAS];
    char nombre[PET_CANCION_NOMBRE];
    uint8_t _r[2];
} cancion_t;
#define PET_CANCION_CRISTAL(b) ((b) & 7)
#define PET_CANCION_PASOS(b) ((((b) >> 3) & 7) + 1)
#define PET_CANCION_NOTA(cristal, pasos) ((uint8_t)(((cristal) & 7) | ((((pasos) < 1 ? 1 : ((pasos) > 8 ? 8 : (pasos))) - 1) << 3)))
#define PET_PASO_MS 180 /* un paso de la canción, en milisegundos (dos pasos = un pulso) */
#define PET_SAVE_VERSION 42
#define PET_CARTAS 8 /* cartas guardadas para releer */
/* versión 23 */
#define PET_VIAJEROS 6    /* las que se fueron de viaje y te escriben */
#define PET_FICHAS_MAX 48 /* animales que se pueden conocer (fauna + animales) */
typedef struct {
    char nombre[PET_NAME_LEN];
    uint8_t especie, genero, vehiculo; /* vehiculo: 0 pluma, 1 barquito de hoja, 2 tren de luciérnagas, 3 diente de león */
    uint8_t cartas;                    /* cuántas te ha escrito (para que cuente su viaje en orden) */
} viajero_t;
/* las criaturas mágicas: una en cada zona */
typedef enum { CRI_MUSGUITO = 0, CRI_LUMBRE, CRI_ONDINA, CRI_VENTOLIN, CRI_CRISTALINO, CRI_MAREA, CRI_COUNT } magica_t;
#define CRI_NIMBA CRI_COUNT /* Nimba, la ballena: cuenta sus historias en los paseos; solo existe como narradora del libro (no es una criatura de visita) */
/* lo que te invitan a hacer */
typedef enum {
    CITA_PICNIC = 0, CITA_PLANTAR, CITA_ESCONDIDAS, CITA_LUCIERNAGAS, CITA_PESCAR, CITA_BARQUITOS, CITA_COMETA, CITA_VOLAR_HOJAS,
    CITA_MUSICA, CITA_TESORO, CITA_ESTRELLAS, CITA_FARO, CITA_COUNT
} cita_t;
typedef enum {
    CRS_ASOMA = 0,  /* se asomó un ratito (sin invitación) */
    CRS_ESPERA,     /* llegaste a la cita: los está esperando */
    CRS_JUGARON,    /* hicieron lo de la cita (b del evento) */
    CRS_SE_VA,
    CRS_HISTORIA,   /* le contó una historia (la última de pet->historias) */
    CRS_HOLA,       /* la tocaste: saluda */
    CRS_SIN_HISTORIAS, /* no le quedan historias que contar: bosteza y se despide (b>>4 = 1 si ya contó las cinco) */
    CRS_COUNT
} criatura_suceso_t;
/* el secreto de cada zona */
typedef enum { ESP_ANILLO = 0, ESP_REFLEJO, ESP_COMETA, ESP_CRISTALES, ESP_COUNT } especial_t;
typedef enum { ESS_DESCUBRE = 0, ESS_OTRA_VEZ, ESS_LEYENDA, ESS_NOTA_BIEN, ESS_NOTA_MAL, ESS_COMETA_CAE, ESS_REGALO } especial_suceso_t;
#define PET_FOTOS_PERFIL 24 /* fotos de la cámara por mascota: al pasarse, se borra la más vieja de ella */

/* Alarmas del dueño (hasta PET_ALARMAS, de cada perfil: viajan en el respaldo). dias: bit 0 lunes ...
 * bit 6 domingo; 0 = suena una sola vez. siempre: suena aunque su perfil no sea el activo. */
#define PET_ALARMAS 5
typedef struct {
    uint8_t activa, hora, minuto, dias;
    uint8_t sonido;  /* 0..ALARMA_SONIDOS-1 (ver pet_alarm_sound) */
    uint8_t siempre; /* suena aunque esté activo otro perfil */
    uint8_t usada;   /* 1 si existe (se puede borrar) */
    uint8_t _pad;
} alarma_t;
#define ALARMA_SONIDOS 4
#define ALARMA_LUN_VIE 0x1Fu
#define ALARMA_TODOS 0x7Fu
#define ALARMA_FINDE 0x60u
#define ALARMA_POSPONER_S 300u /* "5 minutos más" */
/* Hora de dormir: a la hora de acostarse la pantalla pasa a modo noche, se callan los sonidos y ella se
 * acuesta (en su casita o en su tapete); a la de despertar, se levanta. */
typedef enum { DORMIR_NO = 0, DORMIR_FIJA, DORMIR_ADAPTARSE, DORMIR_COUNT } dormir_modo_t;
typedef struct {
    uint8_t modo; /* dormir_modo_t (adaptarse: por ahora usa la hora fija, hasta que aprenda tus hábitos) */
    uint8_t acostarse_h, acostarse_m, despertar_h, despertar_m;
    uint8_t _pad[3];
    uint32_t aviso[2]; /* cuándo avisó por última vez (acostarse, despertar) */
} hora_dormir_t;

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t size;

    char nombre[PET_NAME_LEN];
    uint8_t especie;
    uint8_t etapa;           /* stage_t */
    uint8_t emocion;         /* mood_t visible */
    uint8_t deseo;           /* want_t: lo que pide */

    float necesidad[NEED_COUNT];
    float rasgo[TRAIT_COUNT];
    float emo[EMO_COUNT];        /* intensidad actual de cada emoción */
    float impulso[EMO_COUNT];    /* empuje por lo que pasó; se desvanece */
    uint8_t causa[EMO_COUNT];    /* causa_t de cada emoción */
    uint8_t impulso_causa[EMO_COUNT];

    uint8_t dormida;
    uint8_t enferma;
    uint8_t luz_apagada;     /* el dueño apagó la luz */
    uint8_t dosis;           /* hierbas recibidas en la enfermedad actual */

    uint32_t nacimiento;     /* Unix */
    int32_t zona_horaria;    /* segundos respecto de UTC (Chile: -3*3600) */
    uint32_t ultima_actualizacion;
    uint32_t ultima_interaccion;
    uint32_t ultimo_deseo_avisado;
    uint32_t enfermo_desde;
    float digestion;         /* comida en proceso 0..1; al llenarse ensucia */
    float enamoramiento;     /* 0..1, decae lentamente */
    float dulces;            /* miel reciente, sube riesgo de enfermar */
    float vinculo;           /* 0..100: confianza con el dueño; nace desconfiada */

    /* visita en curso */
    int8_t visita_actual;    /* -1 = ninguna */
    int8_t hora_actual;      /* hora local de la última actualización 0..23 */
    uint8_t hora_flags;      /* interacciones hechas en la hora actual */
    uint8_t ultimo_pedido;   /* último want_t anunciado al dueño */
    uint32_t visita_hasta;

    visitor_t visitantes[PET_MAX_VISITORS];
    habits_t habitos;
    memoria_t memoria;

    uint32_t rng;            /* generador pseudoaleatorio (determinista) */

    /* cola de eventos */
    pet_event_t eventos[PET_EVENT_QUEUE];
    uint8_t ev_ini, ev_fin;

    /* modo de juego según la edad del dueño */
    uint8_t edad_dueno;      /* 0 = todavía no se preguntó */
    uint8_t modo_nino;       /* modo_juego_t: distinto de 0 = modo suave (niño o infante) */

    /* autonomía */
    uint8_t actividad;       /* actividad_t */
    uint8_t lugar;           /* lugar_t donde está */
    uint8_t ultimo_no;       /* action_t de la última vez que no obedeció (0xFF = ninguna) */
    uint8_t pensamiento_emo; /* último pensamiento mostrado */
    uint32_t actividad_hasta;
    uint32_t ultimo_hallazgo;
    uint32_t ultimo_pensamiento;
    uint32_t celos_hasta;
    uint32_t ultimo_juego;
    uint32_t ultima_caricia;
    uint32_t bebida_hasta;   /* hasta cuándo le dura el efecto de la bebida */
    uint8_t bebida_tipo;     /* food_t de la última bebida */
    uint8_t _pad5[3];

    /* el mundo (versión 8) */
    uint8_t escena;          /* escena_t donde está */
    uint8_t escenas_abiertas;/* bits: zonas que ya se pueden visitar */
    uint8_t escenas_vistas;  /* bits: zonas que ya visitó */
    uint8_t persigue;        /* fauna_t que persigue (con ACTV_PERSEGUIR) */
    int8_t gusto_escena[ESC_GUSTOS]; /* usar pet_scene_liking (la isla no tiene lugar aquí) */
    uint8_t hojas;           /* montón de hojas en otoño: 0..3 */
    uint8_t lugar_antes;     /* para marcar caminos */
    int32_t dia_jardin;      /* último día en que el arbolito dio fruta */
    uint32_t escena_desde;   /* desde cuándo está en esta zona */
    uint8_t fauna_perseguida;/* bits: animales que ya persiguió en esta visita */
    uint8_t _pad6[3];
    uint32_t fauna_hasta[FAUNA_COUNT]; /* hasta cuándo está cada animal en su zona */
    uint32_t zorro_roba;     /* si no lo espantas antes de esto, se lleva una fruta */
    uint32_t charcos_hasta;
    uint32_t ultimas_hojas;
    float pajaro_amistad;    /* 0..1 */
    float miedo_abejas;      /* 0..1: lo que aprendió de las picaduras (se olvida lento) */
    float senda[LUGAR_COUNT * (LUGAR_COUNT - 1) / 2]; /* caminos que se marcan */
    planta_t jardin[PET_PARCELAS];

    /* fase 3: tu cumpleaños, su amigo del barrio (versión 9) */
    uint8_t dueno_dia, dueno_mes;  /* 0 = todavía no se preguntó */
    uint16_t dueno_anio;
    int16_t cumple_anio;           /* último año en que se celebró */
    uint8_t _pad7[2];
    vecino_t vecino;

    /* interacción directa (versión 10) */
    float truco[TRUCO_COUNT];      /* 0..100: cuánto sabe cada truco */
    uint8_t racha_trucos;          /* intentos seguidos (se cansa) */
    uint8_t frutas_sacudidas;      /* frutas que cayeron hoy al sacudir */
    uint8_t trucos_sabidos;        /* bit por truco ya celebrado (se avisa una sola vez) */
    uint8_t _pad8;
    int32_t dia_sacudidas;
    uint32_t ultimo_truco;
    uint32_t ultimo_inclinar;
    uint32_t ultimo_lanzar;

    /* fase 4: vínculo, recuerdos y sorpresas (versión 11) */
    uint8_t vinculo_nivel;         /* el más alto alcanzado (vinculo_nivel_t) */
    uint8_t tesoro_pendiente;      /* 1 = olió algo y va a cavar */
    uint8_t tesoros_dia;
    uint8_t raro;                  /* raro_t en curso (0xFF = nada) */
    float habilidad[HAB_COUNT];    /* 0..100 */
    uint8_t hab_hito[HAB_COUNT];   /* último hito avisado */
    uint8_t coleccion[COL_COUNT];  /* bits: piezas encontradas */
    uint8_t colecciones_completas; /* bits */
    uint32_t raro_hasta, ultimo_raro;
    int32_t dia_tesoros;
    uint32_t vivido;               /* bits vivido_t desde que despertó */
    uint8_t meta, meta_objetivo, meta_progreso, meta_hecha;
    int32_t meta_dia;
    uint16_t estampitas;
    uint16_t su_cumple_visto;      /* último "cumplemes" celebrado (en meses; 0xFFFF la semana) */
    int32_t dia_fecha;             /* último día con fecha especial celebrada */
    uint8_t hab_hoy[HAB_COUNT];    /* veces que practicó hoy (aprende de a poco) */
    uint8_t ritmo;                 /* ritmo_t: qué tan rápido corre su propio tiempo (0 = como el tuyo: las partidas viejas no cambian) */
    uint8_t ritmo_visto;           /* 1 = ya elegiste el ritmo al empezar */
    int32_t hab_dia;
    uint8_t diario_n, diario_ini, album_n, album_ini;
    entrada_diario_t diario[PET_DIARIO];
    foto_t album[PET_ALBUM];

    /* fase 5: aspecto, ropa, juguetes y ambiente */
    uint8_t mezcla;          /* 0 = sin mezcla; si no, la otra especie + 1 (sus rasgos: orejas, cola, crestas) */
    uint8_t color;           /* 0..PET_COLORES-1: 0 = el de su especie */
    uint8_t aspecto_ok;      /* ya se eligieron especie y colores (con el huevo) */
    uint8_t ambiente_vol;    /* (ya no se usa: el sonido de fondo es un ajuste del reloj, pet_device_t.ambiente) */
    uint8_t ropa_puesta[RANURAS_VIEJAS]; /* ropa_t + 1 en cada ranura (cabeza, cara, cuello), 0 = nada; la mano va en ropa_mano */
    uint8_t juguete_uso;     /* juguete_t con ACTV_JUGUETE */
    uint16_t ropa_tiene;     /* bit por ropa_t que hay en el baúl */
    uint16_t ropa_amada;     /* bit por ropa_t que ya le encantó (para el diario) */
    uint8_t juguetes_tiene;  /* bit por juguete_t */
    uint8_t juguetes_puestos;/* bit por juguete_t que está en el prado */
    uint8_t juguetes_usados; /* bit por juguete_t que ya estrenó */
    uint8_t juguete_xy[JUG_COUNT][2]; /* dónde está cada uno (pantalla / 2) */
    int8_t gusto_ropa[ROPA_VIEJAS];
    int8_t gusto_juguete[JUG_COUNT];
    uint8_t flores_hechas;   /* flores que florecieron en su jardín */
    uint8_t vio;             /* bits: 1 frío, 2 calor, 4 viento, 8 Navidad (para lo que aparece en el baúl) */
    uint8_t decor_ocultas;   /* bit por decor_t que guardaste (la tiene, pero no se ve) */
    uint8_t _pad10;
    uint32_t ropa_revisada;  /* última vez que pensó si se deja la ropa puesta */

    /* objetos, decoraciones y progreso */
    uint8_t inventario[ITEM_COUNT];
    uint8_t forma;           /* forma_t, se fija al llegar a adulta */
    uint8_t hallazgos_dia;   /* cosas encontradas hoy (con tope) */
    uint8_t pensamiento_causa;
    uint32_t decoraciones;   /* bit por decor_t puesta */
    int32_t ultimo_regalo;   /* día local de la última fruta caída */
    int32_t dia_hallazgos;
    float cuidado;           /* promedio móvil del bienestar: decide la evolución */
    float rasgo_base[TRAIT_COUNT]; /* rasgos al nacer (para saber cuánto cambió) */

    /* ciclo de vida y ambiente */
    uint32_t eclosion;       /* hora en que sale del huevo */
    uint32_t clima_hasta;    /* hasta cuándo dura el clima actual */
    uint8_t clima;           /* clima_t */
    uint8_t hemisferio_sur;  /* 1: estaciones de Chile; 0: hemisferio norte */
    uint8_t estacion_fija;   /* 0xFF = según la fecha; otro = estacion_t forzada (pruebas) */
    uint8_t _pad4;

    /* mensajes y amigos */
    uint32_t id;             /* identificador de esta mascota para la radio */
    int32_t ultima_frase_slot;
    uint8_t ultima_frase;
    uint8_t frase_respondida; /* ya le respondiste el mensaje de ahora (antes era relleno: las partidas viejas traen 0) */
    uint16_t token;          /* token de la invitación en curso */
    uint32_t invitando_a;    /* id del amigo invitado (0 = nadie) */
    uint32_t aceptando_a;    /* id del amigo cuya invitación aceptamos */
    uint32_t invitacion_de;  /* id del amigo que nos invitó y espera respuesta */
    uint32_t invitacion_hasta;
    uint32_t envio_hasta;    /* hasta cuándo se emite la invitación o la respuesta */
    amigo_t amigos[PET_MAX_AMIGOS];

    /* versión 14 (lo nuevo va siempre al final, antes del crc: así las partidas viejas se actualizan) */
    uint8_t semillas[CULTIVO_COUNT]; /* semillas de cada tipo (inventario[ITEM_SEMILLA] es la suma) */
    uint8_t semillas_vistas;         /* bits: tipos de semilla que ya encontró */
    uint8_t _pad11[3];
    planta_t jardin_zona[3 * PET_PARCELAS_ZONA]; /* bosque, lago y colina (parcelas 6 a 14) */
    /* versión 15 */
    alarma_t alarma[PET_ALARMAS];
    uint32_t alarma_sono[PET_ALARMAS];      /* cuándo sonó por última vez (UTC), para no repetir */
    uint32_t alarma_pospuesta[PET_ALARMAS]; /* vuelve a sonar a esta hora (UTC; 0 = no) */
    hora_dormir_t dormir;
    uint32_t ultimo_respaldo;               /* cuándo se guardó en el computador (para el aviso suave) */
    /* versión 16 */
    uint8_t perfil_vive;  /* 1: sigue viviendo cuando no es el perfil activo (0: descansa) */
    uint8_t modo_manual;  /* 0: el modo sale de tu edad; 1 + modo_juego_t: elegido a mano en Ajustes */
    uint8_t _pad16[2];
    uint32_t temporizador; /* cuándo suena el temporizador (UTC; 0 = no hay) */
    uint32_t pasos_total;  /* pasos que contó el acelerómetro mientras era la activa */
    uint32_t pasos_premio; /* pasos_total la última vez que trajo algo por caminar */
    /* versión 17: la Bolsa de objetos mágicos */
    uint8_t bolsa[PET_OBJETOS];               /* cuántos tiene de cada uno (máx. PET_OBJ_MAX) */
    uint8_t obj_visto[(PET_OBJETOS + 7) / 8]; /* bits: los que encontró alguna vez (el libro) */
    uint8_t obj_dia[PET_OBJETOS];             /* los que no se gastan funcionan una vez al día: 1 + día local & 0xFF */
    uint8_t obj_puesto[PET_OBJ_PUESTOS];      /* 1 + objeto decorando el prado (0 = libre) */
    uint8_t obj_cabeza;                       /* 1 + objeto que lleva en la cabeza */
    uint8_t cielo;                            /* 1 + param del efecto del cielo activo */
    uint8_t compa;                            /* 1 + param del acompañante */
    uint8_t bolsa_nueva;                      /* 1 + último objeto que trajo y todavía no ves en la Bolsa */
    uint8_t plan_tipo, plan_paso, plan_fallos;  /* versión 33+: el plan en marcha (plan_t), en qué paso va y cuántas veces no pudo (antes relleno: las partidas viejas traen 0) */
    uint32_t cabeza_hasta, cielo_hasta, compa_hasta;
    uint32_t suerte_hasta, pesca_hasta, protege_hasta;
    uint32_t ultimo_objeto;                   /* cuándo encontró el último (para espaciarlos) */
    /* versión 18: lo que descubriste de cada objeto, los regalos al volver y la niebla */
    uint8_t obj_sabe[(PET_OBJETOS + 7) / 8];  /* bits: ya sabes qué hace (lo usaste una vez) */
    uint8_t regalo_vuelta;                    /* el último regalo al volver (1 + regalo_t) */
    uint8_t barro;                            /* 1: trajo barro en las botas (huellas hasta el baño) */
    uint8_t hediondo;                         /* (fw 103, era relleno: parte en cero) el zorrillo la roció: pasos de 10 min que le quedan de olor */
    uint8_t hediondo_min;                     /* minutos sueltos hacia el próximo paso (así el olor no se reinicia con la placa) */
    uint8_t _pad18[1];
    uint32_t hoja_hasta;                      /* lleva la hoja gigante de sombrero hasta esta hora */
    uint32_t palo_hasta;                      /* el palo perfecto está esperando que se lo tires */
    uint32_t niebla_hasta;                    /* hay niebla hasta esta hora (0 = no) */
    int32_t dia_niebla;                       /* el último día en que se revisó si amanecía con niebla */
    int32_t dia_regalo_vuelta;                /* el último día en que te trajo un regalo al volver */
    /* versión 19: la ofrenda al árbol hueco y la bendición del bosque al día siguiente */
    int32_t dia_ofrenda;                      /* el día de la última ofrenda que todavía espera su bendición (0 = no) */
    int32_t dia_luciernagas;                  /* esa noche el bosque manda luciérnagas */
    /* versión 20: el linaje (lo que la va moldeando mientras crece) y sus habilidades */
    uint16_t linaje[8];                       /* puntos hacia la forma adulta de cada especie */
    uint32_t linaje_hora;                     /* la última hora que se contó (zona y clima) */
    uint8_t hab_usada[2];                     /* sus dos habilidades: 1 + día local & 0xFF en que se usaron */
    uint8_t dia_botella;                      /* 1 + día local & 0xFF de la última botella con mensaje en la isla */
    int8_t trato;                             /* cómo la has tratado últimamente (-100..100); se va apagando con las horas (antes relleno) */
    /* versión 21: el paseo en la ballena, la Guardiana, se adapta a ti (rituales), anécdotas, la semana y la familia */
    uint8_t dia_ballena;                      /* 1 + día & 0xFF del último paseo por el firmamento (uno al día) */
    uint8_t guardiana;                        /* 1: hizo el rito con el Corazón del Mundo */
    uint8_t dia_te_esperaba;                  /* 1 + día & 0xFF: ya te recibió a tu hora de siempre */
    uint8_t dia_anecdota;                     /* 1 + día & 0xFF: ya contó una anécdota hoy */
    uint8_t rit[2];                           /* días seguidos de buenos días (0) y buenas noches (1); 0x80 = ritual aprendido */
    uint8_t dia_rit[2];                       /* 1 + día & 0xFF del último que se contó */
    int32_t dia_guardiana;                    /* día local del rito */
    int32_t semana;                           /* la semana de los contadores (día local / 7, empezando el lunes) */
    uint8_t sem[4];                           /* esta semana: juegos, mimos, cosas mágicas, paseos a otras zonas */
    uint8_t hijos;                            /* huevos de familia que dejó */
    uint8_t generacion;                       /* 0: la primera; sus hijos 1, sus nietos 2... */
    uint8_t huevo_familia;                    /* 1: hay un huevo de familia esperando que lo cuides */
    uint8_t despedida_natural;                /* 1: dejas que algún día pida irse de viaje (siempre con tu permiso) */
    char padre[PET_NAME_LEN];                 /* quién la dejó (vacío: nadie, fue la primera) */
    int32_t dia_familia;                      /* cuándo dejó el último huevo */
    int32_t dia_pide_viaje;                   /* la última vez que pidió irse de viaje */
    uint16_t mis_fotos[PET_FOTOS_PERFIL];     /* las fotos de la cámara que son de esta mascota (las más viejas primero) */
    uint8_t mis_fotos_n;
    uint8_t fotos_legado_ok;                  /* 1: ya se marcó hasta dónde llegan las fotos de antes (compartidas) */
    uint16_t fotos_legado;                    /* las fotos con id hasta aquí son de antes de la versión 21: las ven todas */
    /* versión 22: cartas de los amigos del barrio y si es niña o niño */
    uint32_t vecinos_vistos;                  /* bit i: ya conoce al amigo del barrio i (los que le pueden escribir) */
    int32_t carta_dia;                        /* día local en que le toca la próxima carta (0: todavía no) */
    uint8_t carta_hora;                       /* a qué hora de ese día */
    uint8_t carta_pend;                       /* 1: la última carta llegó y todavía no la abres */
    uint8_t cartas_n;                         /* cuántas cartas guardadas (hasta PET_CARTAS) */
    uint8_t genero;                           /* 0: niña ("ella"), 1: niño ("él") */
    uint8_t cartas[PET_CARTAS][2];            /* las cartas, la más nueva primero: quién la escribió y cuál es */
    uint8_t pesca_dia;                        /* 1 + día & 0xFF de las pescas que hizo sola */
    uint8_t pescas_solas;                     /* cuántas veces pescó sola ese día */
    uint8_t hoja_nenufar;                     /* 1: el sombrero de hoja (hoja_hasta) es una hoja de nenúfar */
    uint8_t erizo_confianza;                  /* veces que esperó quieta junto al erizo (3: amigos) */
    uint32_t animal_hasta[ANIMAL_V22];        /* hasta cuándo está cada animal del bosque (los ocho primeros) */
    uint32_t ardilla_vuelve;                  /* cuándo la ardilla desentierra la semilla que se llevó */
    uint8_t ardilla_semilla;                  /* 1 + cultivo_t que se llevó (0: ninguna) */
    uint8_t dia_tortuga;                      /* 1 + día & 0xFF: la tortuga ya la consoló hoy */
    uint8_t dia_luna_aviso;                   /* 1 + día & 0xFF: ya avisó que esta noche hay luna llena */
    uint8_t anillos;                          /* el Árbol de los años: cuántos cumpleaños tuyos celebraron */
    /* versión 23: más cartas (de la familia, de los viajeros y de las criaturas), animales, criaturas y secretos */
    uint8_t cartas_var[PET_CARTAS];           /* saludo y firma (bits 0-1) y si trae P.D. (0x80) de cada carta */
    viajero_t viajeros[PET_VIAJEROS];         /* las que se fueron de viaje (la más nueva primero): te escriben */
    uint8_t viajeros_n;
    uint8_t padre_genero;                     /* 0: la dejó su mamá, 1: su papá */
    uint8_t fichas_n;                         /* animales que conoció contigo */
    uint8_t criaturas_vistas;                 /* bit c: ya conoce a la criatura c */
    uint8_t fichas[PET_FICHAS_MAX];           /* los animales que conoció contigo, en orden */
    uint32_t animal_hasta2[ANIMAL_COUNT - ANIMAL_V22];
    uint8_t historias[CRI_COUNT];             /* bit k: ya le contó su historia k */
    uint8_t cita_criatura;                    /* 1 + magica_t que te invitó (0: nadie) */
    uint8_t cita_que;                         /* cita_t de la invitación */
    uint8_t cita_dia;                         /* 1 + día & 0xFF en que los espera (0: ya pasó) */
    uint8_t cita_estado;                      /* 0 carta sin leer, 1 la leíste: los espera, 2 ya jugaron */
    int32_t criatura_prox;                    /* día local de la próxima invitación */
    uint32_t criatura_hasta;                  /* hasta cuándo está la criatura en su zona */
    uint8_t criatura_cual;                    /* cuál está (si criatura_hasta) */
    uint8_t criatura_espera;                  /* 1: está esperando para jugar (la cita) */
    uint8_t especiales;                       /* bit e: ya descubrió el secreto e */
    uint8_t pista_ultima;                     /* la última pista que comentó (1 + especial * 8 + cuál) */
    uint32_t pista_prox;                      /* cuándo puede comentar otra pista */
    uint8_t pistas_dadas[ESP_COUNT];          /* cuántas pistas dio de cada secreto (la melodía se va contando de a poco) */
    uint8_t especial_dia[ESP_COUNT];          /* 1 + día & 0xFF: ya pasó hoy (el anillo, la cometa...) */
    uint8_t leyendas;                         /* bit k: ya le contaron la leyenda de la constelación k */
    uint8_t melodia_pos;                      /* (versión 23; ya no se usa: ahora es el juego de los cristales) */
    uint8_t _rel23[2];
    /* versión 24: el país, el juego de los cristales y las fiestas con todos */
    uint8_t pais;                             /* pais_t (0: todavía no lo eligen) */
    uint8_t cristales_rondas;                 /* hasta qué ronda llegó en el juego de los cristales (3: despertó al dragón) */
    uint8_t fiestas_hechas;                   /* bit fecha_t (bit 7: su cumpleaños): ya celebraron con todos este año */
    uint8_t _rel24;
    int16_t fiestas_anio;                     /* el año de fiestas_hechas */
    uint8_t _rel24b[2];
    /* versión 25: los nombres de sus amigos, inventados para ella */
    uint32_t semilla_amigos;                  /* de aquí salen los nombres de sus hasta 32 amigos (0: los 20 de antes) */
    /* versión 26: antes de crecer te pregunta (ves cómo será y decides) */
    uint8_t evo_pregunta;                     /* 1: antes de cada crecimiento te pregunta (mascotas nuevas); 0: crece sola (partidas de antes) */
    uint8_t evo_pend;                         /* 0: nada; si no, la etapa a la que ya puede crecer y espera tu decisión */
    uint8_t evo_decidido;                     /* 1: dijiste «todavía no»: se sigue pudiendo desde Ajustes, pero ya no te lo pregunta */
    uint8_t _evo;

    /* versión 27: viaje en el tiempo (solo de este perfil; no cambia el reloj del dispositivo) */
    int32_t desfase_s;                        /* lo que se suma a la hora real del reloj para esta mascota (0 = hora real) */

    /* versión 28: bolsas sin repetición. Lo que sale al azar (frases, cartas, visitas, criaturas, animales) no se repite hasta
     * que salió todo lo que podía salir; entonces empieza otra vuelta. Un bit por cosa. */
    uint64_t bolsa_frase, bolsa_carta, bolsa_familia, bolsa_viajero, bolsa_invitacion, bolsa_animal;
    uint32_t bolsa_amigo;                     /* visitantes del barrio (hasta 32) */
    uint8_t bolsa_criatura;                   /* criaturas que ya invitaron en esta vuelta */
    uint8_t final_estado;                     /* bit0: ya vivió el final regular; bit1: el secreto */
    uint8_t final_pend;                       /* 0 nada, 1 final regular esperando, 2 final secreto esperando */
    uint8_t _b28;

    /* versión 29: comidas de temporada */
    uint8_t comida_est[16];                   /* cuántas tiene de cada una (hasta 9) */
    uint8_t gusto_est[16];                    /* cuánto le gusta cada una (0 = todavía no la probó: sale de su manera de ser) */
    uint16_t comida_vista;                    /* bits: las que ya encontró alguna vez */
    uint8_t comida_ult[4];                    /* las últimas que comió (para la variedad) */
    uint8_t humor, humor_dia;                 /* humor_t de hoy y 1 + el día en que se eligió (antes relleno: las partidas viejas traen 0) */
    uint8_t sem_act[32];                      /* esta semana: cuántas veces eligió cada actividad (para la página del lunes) */
    uint8_t dia_foto_gusto;                   /* el día de la última foto haciendo su actividad favorita (una al día) */
    uint32_t gusto_aviso;                     /* la última vez que dijo cuál es su actividad favorita (no lo repite seguido) */
    uint16_t mini_dia[8];                     /* el día (de su calendario) en que vio cada mini evento por última vez */
    uint8_t mini_ciclo, mini_flags;           /* la estación de este año y qué "primeras" ya vio en ella (lluvia, nieve) */
    uint8_t melodia[PET_MELODIA_NOTAS];       /* su propia melodía: cada nota es un cristal de la cueva (0 = azul, 1 = verde, 2 = rosado, 3 = dorado, 4 = violeta, 5 = blanco) */
    uint8_t melodia_n, melodia_ens;           /* cuántas notas tiene (0 = todavía no la crea) y cuántos ensayos lleva */
    uint16_t melodia_dia;                     /* el día del último ensayo (uno por día como mucho) */
    uint8_t escucha_musica;                   /* 1 si el reloj tiene encendida la escucha de música (lo pone main cada vuelta; sin eso no pide su canción) */
    uint8_t est_ult, est_mask;                /* la última estación que notó (+1) y cuáles ya vivió (un bit cada una) */
    uint32_t cancion_t, cancion_pide;         /* su canción: la última vez que sonó y la última vez que la pidió */
    uint8_t cancion_bpm, cancion_en, cancion_n; /* su huella (golpes por minuto, energía 0..3) y en cuántas ocasiones distintas la oyó */
    uint32_t musica_ult;                      /* la última vez que le encantó una música (para no repetirlo) */
    uint8_t caca_zona[8];                     /* cuántas cacas hay en cada zona (donde estaba cuando las hizo) */
    uint8_t obj_oro[(PET_OBJETOS + 7) / 8];   /* bits: ya encontró la versión dorada de ese objeto (juego+) */
    int8_t gusto[32];                         /* cuánto le gusta cada actividad opcional, aprendido de tus caricias y juegos (-60..60) */
    uint8_t probando;                         /* ACTV_PROBAR: ropa_t + 1 (1..31), o 32 + objeto (0 = nada) */

    /* versión 33: lo que de verdad hacen los objetos más raros */
    uint32_t doble_hasta;                     /* espejo dimensional: hasta esta hora todo lo que encuentra viene doble */
    uint32_t invisible_hasta;                 /* pócima de invisibilidad */
    uint32_t talla_hasta;                     /* encogimiento o crecimiento (talla): hasta esta hora */
    uint32_t colores_hasta;                   /* pócima de colores */
    uint32_t lentes_hasta;                    /* lentes reveladores: más cosas raras */
    uint32_t iman_hasta;                      /* imán místico: atrae objetos perdidos y antigüedades */
    uint16_t tinta;                           /* bits: las notas escondidas del diario que ya reveló */
    uint8_t talla;                            /* 0 normal, 1 chiquitita, 2 grande */
    uint8_t cofre;                            /* bit0: ya lo vio, bit1: ya lo abrió (cofre bajo el árbol viejo) */
    int32_t dia_nidos;                        /* día en que busca nidos (con las gafas del explorador) */
    uint8_t nidos;                            /* bits: los nidos que ya miró hoy */
    uint8_t libro;                            /* bits: las páginas del Libro (el cuento del Libro I) que ya le leyeron, ver pet_libro_seen */
    uint8_t hist_dia;                         /* día local (& 0xFF) en que cuenta las historias de hoy */
    uint8_t hist_hoy;                         /* cuántas historias de criaturas le contaron ese día (máx. PET_HISTORIAS_DIA) */

    uint8_t historias_nimba;                  /* bit k: ya le contó Nimba su historia k (una por paseo) */
    uint8_t libro_aviso;                      /* 0 aún no le enseñaron a abrir el libro, 1 debe enseñárselo la criatura que acaba de contar, 2 ya lo sabe */
    uint8_t libro_aviso_de;                   /* quién se lo enseña (magica_t) */
    uint8_t _res_libro;

    uint8_t ropa_mano;                        /* ropa_t + 1 de lo que lleva en la mano (0 = nada) */
    uint8_t _res_mano[3];

    uint32_t juguete_hasta[JUG_COUNT];        /* hasta cuándo está afuera cada juguete del prado: unos minutos y se guarda solo en el baúl (0 = sin hora) */

    /* versión 37: la historia del mundo (biblia v2): avanza por días jugados */
    int32_t dia_jugado;                       /* último día local en que hubo juego */
    uint32_t tilde_hasta;                     /* Tilde, la urraca, está de visita hasta (0 = no está) */
    uint32_t brizna_hasta;                    /* la Abuela Brizna está despierta hasta (0 = dormida) */
    uint32_t animal_frases[2];                /* bits: lo que ya le contaron los animales (pet_animal_phrase) */
    uint16_t dias_jugados;                    /* días en que hubo al menos un rato de juego */
    uint16_t tilde_chismes;                   /* bits: los chismes de Tilde que ya escuchó */
    uint16_t tilde_prox;                      /* en qué día jugado vuelve Tilde */
    uint16_t brizna_ultimo;                   /* en qué día jugado despertó por última vez la Abuela Brizna */
    uint8_t tilde_chisme;                     /* el chisme de esta visita (0xFF ninguno; | 0x80 = repetido) */
    uint8_t tilde_lleva;                      /* 1 + objeto que se llevó y todavía tiene ella */
    uint8_t tilde_trae;                       /* 1 + objeto que devuelve en esta visita (con una nota) */
    uint8_t tilde_toma;                       /* 1 + objeto que se lleva en esta visita */
    uint8_t tilde_escena;                     /* dónde está de visita */
    uint8_t tilde_visitas;
    uint8_t tilde_nota;                       /* cuántas notas dejó (elige la siguiente) */
    uint8_t tilde_oido;                       /* bit0: oyó el chisme de esta visita, bit1: leyó la nota */
    uint8_t prisma_caps;                      /* bits: los capítulos que le contó Prisma, el dragón de cristal */
    uint8_t prisma_dia;                       /* 1 + día & 0xFF: hoy ya contó uno */
    uint8_t brizna;                           /* bits: los recuerdos de la Abuela Brizna */
    uint8_t brizna_k;                         /* el recuerdo de este despertar (0xFE = solo saluda) */
    uint8_t brizna_saludo;                    /* 1 + día & 0xFF del último saludo (después del final) */
    uint8_t tilo_paginas;                     /* bits: las páginas del diario de Tilo que volvieron al Libro */
    uint8_t tilo_vistas;                      /* bits: las que ya vio aparecer */
    uint8_t animal_dia;                       /* 1 + día & 0xFF: hoy ya le contó algo un animal */
    uint8_t lugares;                          /* bits: los lugares secretos abiertos (0 la casita de Tilo, 1 el mirador) */
    uint8_t lugar_hallado[2];                 /* bits: lo que ya encontró en cada lugar secreto */
    uint8_t _res_historia;

    /* versión 38: la música libre de los cristales, nuestra canción, la casita de ramitas y musgo, la pesca en el hielo */
    cancion_t canciones[PET_CANCIONES];      /* 0 = "nuestra canción" (la principal); 1..4 el cuaderno (n = 0: vacía) */
    uint32_t lago_helado_hasta;               /* la bebida helada del norte congela el lago (fuera del invierno) */
    uint32_t limpio_hasta;                    /* el broche que Tilde devolvió: no se ensucia */
    uint32_t flota_hasta;                     /* el polvo de hadas: flota un poquito sobre el suelo */
    uint16_t cris_piezas;                     /* piezas tocadas en el modo libre de los cristales */
    uint8_t cris_dias;                        /* días distintos en que tocaron juntos */
    uint8_t cris_dia;                         /* 1 + día & 0xFF del último día que tocaron */
    uint8_t cris_flags;                       /* bit0: ya le dieron la escama; bit1: el dragón ya propuso la canción; bit2: ella la propuso */
    uint8_t cris_propuso_dia;                 /* 1 + día & 0xFF: hoy ya propusieron hacer una canción */
    uint8_t cancion_alarma;                   /* 0: las alarmas suenan con su sonido; k + 1: con la canción k */
    uint8_t cancion_cuna;                     /* 0: la canción de cuna de siempre; k + 1: la canción k */
    uint8_t ramitas;                          /* materiales de la casita */
    uint8_t musgo;
    uint8_t peces_a[2];                       /* (1.0.32, era relleno: parte en cero) bits: peces 0-15 de la colección de la pesca */

    /* versión 39: Pochi en la historia, la página suelta, las zonas del claro y el mirador, el segundo final */
    uint8_t pochi_charla;                     /* bits 0-1, 2-3, 4-5: charlas con Pochi en cada acto; bit6: ya le contó el nombre */
    uint8_t zona_pista;                       /* bits: ya le recordó cómo volver (0 el claro, 1 el mirador) */
    uint8_t tilo_ultima;                      /* bit0: la última página del diario (la carta del segundo final); bit1: ya la vio */
    uint8_t animal_hoy;                       /* frases de animales contadas hoy (con animal_dia) */
    uint8_t tilde_dia;                        /* 1 + día & 0xFF de la última visita de Tilde (una por día) */
    uint8_t lib_nuevo[2];                     /* bits por narrador: tiene algo nuevo sin mirar en el Libro de historias (la estrellita) */
    uint8_t en_zona;                          /* (v39) 1: están en el claro, la casita o el mirador (no se va sola a otra zona) */

    /* versión 40: las cosas escondidas en los lugares secretos y las excursiones sola */
    uint8_t zona_hallazgo[3];                 /* 1 + día & 0x7F en que ya sacó lo escondido de hoy (0 el claro, 1 la casita, 2 el mirador) */
    uint8_t excursion_dia;                    /* 1 + día & 0x7F de la última excursión sola (con excursiones: cuántas ese día) */
    uint8_t excursiones;
    uint8_t pochi_amigos;                     /* (fw 102, era relleno: parte en cero) bits 0-3: el cachorro k ya se hizo amigo de su animal */
    uint8_t peces_b[2];                       /* (1.0.32, era relleno) bits: peces 16-29 */

    /* versión 41: el claro y el mirador son zonas de verdad; los juegos de los lugares tienen récord y estrellas */
    uint16_t jl_record[5];                    /* el mejor puntaje de cada juego de los lugares (PET_JL_*) */
    uint8_t jl_estrellas[5];                  /* las mejores estrellas (0..3) */
    uint8_t jl_premios[5];                    /* rondas con premio hoy (con jl_dia) */
    uint8_t jl_dia;                           /* 1 + día & 0x7F de jl_premios */
    uint8_t jl_jugadas;                       /* rondas jugadas en total (hasta 255) */
    /* (fw 102, en los 2 bytes de relleno de la versión 41: parten en cero, no cambia la versión) Pochi se acuerda de ti */
    uint8_t pochi_visitas;                    /* cuántas veces vino (hasta 255) */
    uint8_t pochi_mem;                        /* bits 0-1: cómo terminó la última visita (PET_POCHI_ULT_*); bits 2-5: cachorros que ya conociste */
    /* versión 42 (fw 104): los papelitos del bosque, el trocito de hielo, la terquedad y los paseos de verdad */
    uint16_t papelitos;                       /* bits: papelito k ya encontrado (queda en el Libro) */
    uint8_t carta_sem;                        /* 1 + (semana & 0x7F) de carta_sem_n */
    uint8_t carta_sem_n;                      /* papelitos aparecidos esta semana (2-3 por semana, no más) */
    uint8_t papel_dia;                        /* 1 + día & 0x7F en que salió el último papelito (se espacian) */
    uint8_t hielo_fuera;                      /* días seguidos sin invierno con el trocito de hielo (se derrite) */
    uint8_t paseo_dia;                        /* 1 + día & 0x7F de paseo_n */
    uint8_t paseo_n;                          /* paseos de verdad premiados hoy (máx. 2) */
    uint8_t terca_act;                        /* 1 + la acción que acaba de negar (a la segunda obedece) */
    /* (1.0.17, en los 3 bytes de reserva104: parten en cero, no cambia la versión) los juegos de a dos con las visitas */
    uint8_t dos_jugadas;                      /* partidas de a dos (hasta 255) */
    uint8_t dos_ganadas;                      /* las que ganó ella (hasta 255) */
    uint8_t dos_dia;                          /* 1 + día & 0x7F de la última que quedó en el diario (una por día) */
    uint32_t terca_t;                         /* cuándo la negó */

    uint32_t crc;            /* siempre el último campo */
} pet_t;


/* ---------- Cartas de los amigos del barrio (versión 22) ----------
 * Cada 3 a 5 días le escribe un amigo que ya conoce (solo en modo niño y adulto): cómo le va, que la
 * extraña, algo que vivieron juntos o cómo está el bosque en esta estación. Llega cerrada; al abrirla se
 * pone contenta y queda guardada (las últimas PET_CARTAS) para releerla. */
bool pet_letter_pending(const pet_t *p);
int pet_letter_count(const pet_t *p);
/* la carta i (0 = la más nueva): quién la escribió y el texto completo (saludo, cuerpo y firma). */
bool pet_letter_get(const pet_t *p, int i, char *de, int nde, char *txt, int ntxt);
void pet_letter_open(pet_t *p, uint32_t ahora);
/* para probar: que le llegue una carta ahora (si conoce a alguien). Devuelve false si no pudo */
bool pet_letter_now(pet_t *p, uint32_t ahora);
int pet_letter_bodies(void);              /* cuántos textos distintos hay (para las pruebas) */
/* versión 23: de quién es la carta i y quién la trae */
typedef enum { CARTA_AMIGO = 0, CARTA_FAMILIA, CARTA_VIAJERO, CARTA_CRIATURA } carta_de_t;
typedef enum { MENS_PALOMA = 0, MENS_BUHO, MENS_MARIPOSA, MENS_GRULLA, MENS_HOJA, MENS_CHISPA, MENS_BURBUJA, MENS_VIENTO, MENS_CRISTAL, MENS_CARACOLA } mensajero_t;
carta_de_t pet_letter_from(const pet_t *p, int i);
mensajero_t pet_letter_messenger(const pet_t *p, int i, uint32_t ahora); /* quién la trae (de noche, el búho) */
int pet_letter_bodies_of(carta_de_t de);  /* cuántos textos hay de cada tipo */
/* para probar: que llegue una carta de este tipo (false si no puede: no hay familia, viajeros...) */
bool pet_letter_now_from(pet_t *p, carta_de_t de, uint32_t ahora);

/* ---------- Las que se fueron de viaje (versión 23) ----------
 * Al despedirla (en Mascotas o la despedida natural) queda anotada: desde su viaje te escribe y saluda a la
 * de ahora. El vehículo decide por dónde anda (la pluma por la aurora, el barquito por el lago de estrellas...) */
void pet_add_traveller(pet_t *p, const pet_t *viajera, int vehiculo, uint32_t ahora);
int pet_travellers(const pet_t *p);
bool pet_photo_moment(int motivo); /* (1.0.27) ¿este momento va al álbum? (solo los grandes) */
bool pet_traveller_back(pet_t *p, const pet_t *viajera); /* (1.0.26) la que se fue volvió (Perfiles): deja de ser viajera */

/* ---------- Animales que conoció contigo (versión 23) ----------
 * Una ficha por animal (los cinco de siempre y los del bosque). Solo cuenta si lo vieron juntos: la interfaz
 * avisa cuando estuvo unos segundos a la vista con la pantalla encendida */
#define PET_FICHAS (FAUNA_COUNT + ANIMAL_COUNT)
int pet_card_of_fauna(fauna_t f);
int pet_card_of_animal(animal_t a);
bool pet_animal_seen(pet_t *p, int ficha, uint32_t ahora); /* true si es nuevo */
int pet_cards_known(const pet_t *p);                      /* cuántos conoce */
int pet_card_get(const pet_t *p, int i);                  /* el i-ésimo que conoció (0 el primero) */
const char *pet_card_name(int ficha);                     /* "el picaflor" */
const char *pet_card_name_a(int ficha, char *buf, int n);  /* "al picaflor", "a la rana" */
const char *pet_card_story(int ficha);                    /* su historia para los Tesoros */
const char *pet_animal_doing(animal_t a);                 /* lo que hace (ANI_HACE), para el globito */

/* ---------- (fw 103) Tocar a un animal: lo bueno y lo malo ----------
 * Cada animal tiene su paciencia. El primer toque es un saludo; al siguiente avisa que se está molestando;
 * si insistes, hay consecuencia (y ella aprende). Los que no se molestan nunca devuelven siempre lo mismo.
 * `veces` son los toques seguidos a ESE animal (1 = el primero). En modo infante nunca pasa nada malo. */
typedef enum {
    PET_TOC_NADA = 0,  /* ahí no había nada */
    PET_TOC_SALUDA,    /* le gustó: lo saluda, lo huele, juegan */
    PET_TOC_AVISA,     /* se molestó: a la próxima duele */
    PET_TOC_PICA,      /* la picó (abeja, hormigas, avispa) */
    PET_TOC_PUAS,      /* se pinchó con las púas (erizo) */
    PET_TOC_ROCIA,     /* el zorrillo la roció: queda hedionda */
    PET_TOC_RUGE,      /* uno grande la asustó */
    PET_TOC_SE_VA,     /* se cansó y se fue */
    PET_TOC_ESCONDE,   /* se escondió (topo, caracol, tortuga) */
    PET_TOC_COUNT
} pet_toque_t;
int pet_animal_touch(pet_t *p, int animal, int veces, uint32_t ahora); /* animal_t */
int pet_fauna_touch(pet_t *p, int fauna, int veces, uint32_t ahora);   /* fauna_t */
int pet_animal_patience(int animal); /* cuántos toques aguanta antes de la consecuencia (0: infinita) */
bool pet_stinky(const pet_t *p);     /* la roció el zorrillo (o se puso hedionda sola) y todavía huele */
/* (1.0.18) le soplaste o espantaste el olor: se le quita (el olor «suelto», no el del zorrillo, que es más fuerte y
 * pide baño; ese solo baja a la mitad). Devuelve 1 si se le quitó, 2 si solo bajó, 0 si no estaba hedionda. */
int pet_air_out(pet_t *p, uint32_t ahora);

/* ---------- Criaturas mágicas (versión 23) ----------
 * Una en cada zona. A veces te escriben para invitarte a hacer algo juntos: si después de leer la carta van a
 * esa zona ese mismo día, la criatura los espera y juegan. Muy de vez en cuando se asoman solas. Cada vez que
 * se encuentran le cuentan una historia de su lugar (queda en el diario para releerla) */
const char *pet_creature_name(magica_t c);
escena_t pet_creature_zone(magica_t c);
const char *pet_creature_about(magica_t c);             /* quién es, para los Tesoros */
bool pet_creature_here(const pet_t *p, uint32_t ahora, magica_t *c); /* hay una en la zona en que está */
bool pet_creature_waiting(const pet_t *p, uint32_t ahora);             /* es la de la cita y los espera */
cita_t pet_creature_invite(const pet_t *p);                            /* qué los invitó a hacer */
const char *pet_cita_name(cita_t c);
void pet_creature_touch(pet_t *p, uint32_t ahora);       /* tocas a la criatura: saluda, o empieza la cita */
void pet_creature_done(pet_t *p, cita_t que, int puntos, uint32_t ahora); /* terminaron la cita */
int pet_creature_stories(magica_t c);
int pet_creature_stories_known(const pet_t *p, magica_t c);
const char *pet_story_title(magica_t c, int k);
const char *pet_story_text(magica_t c, int k);
/* ---------- La historia del mundo (versión 37, biblia v2) ----------
 * Todo avanza por días jugados (días con al menos un rato de juego), con un tope de PET_HISTORIAS_DIA momentos de historia por día
 * (historias de criaturas y de Nimba, capítulos de Prisma, chismes de Tilde, recuerdos de Brizna). Si no se juega, la historia espera.
 * Los narradores del Libro de historias: 0..5 las criaturas, CRI_NIMBA y los de aquí. */
#define NARR_PRISMA (CRI_NIMBA + 1)   /* el dragón de cristal: sus capítulos (al tocarlo en la cueva, uno por día) */
#define NARR_TILDE (CRI_NIMBA + 2)    /* Tilde, la urraca: sus chismes */
#define NARR_BRIZNA (CRI_NIMBA + 3)   /* la Abuela Brizna, la tortuga bajo la isla: sus recuerdos */
#define NARR_TILO (CRI_NIMBA + 4)     /* las páginas sueltas del diario de Tilo */
#define NARR_ANIMALES (CRI_NIMBA + 5) /* lo que dicen los animales */
#define NARR_PAPELITOS (CRI_NIMBA + 6) /* (fw 104) los papelitos del bosque: cartas y pistas escondidas como tesoro del día */
#define NARR_COUNT (CRI_NIMBA + 7)
#define PET_PRISMA_CAPS 8
#define PET_TILDE_CHISMES 12
#define PET_BRIZNA_RECUERDOS 5
#define PET_TILO_PAGINAS 9 /* (v39) la 8 es la última: la carta del segundo final */
#define PET_ANIMAL_FRASES 33
int pet_days_played(const pet_t *p);
int pet_story_act(const pet_t *p);                      /* 1, 2 o 3: en qué acto va la historia */
int pet_story_moments_left(const pet_t *p, uint32_t ahora); /* cuántos momentos de historia quedan hoy */
int pet_narr_count(int narr);                           /* cuántas tiene cada narrador */
const char *pet_narr_name(int narr);                    /* "Musguito", "Prisma", "Tilde"... */
const char *pet_story_short(int c, int k);              /* la versión corta (modo niño) */
/* Prisma: lo tocaste en la cueva. Devuelve el capítulo que te cuenta ahora (-1 si hoy no) */
int pet_prisma_touch(pet_t *p, uint32_t ahora);
bool pet_prisma_ready(const pet_t *p, uint32_t ahora);
/* (fw 104) los papelitos del bosque: de vez en cuando (2-3 por semana) el escondite del día guarda una carta o una pista */
#define PET_PAPELITOS 12
/* (fw 104) terquedad: cuánto le cuesta hacer caso cuando no quiere (de sus rasgos: traviesa y valiente = más terca) */
float pet_stubbornness(const pet_t *p);
/* (fw 104) salieron a caminar de verdad (pasos sostenidos con la pantalla apagada): 0 no contó, 1 contó, 2 contó y trajo algo */
int pet_walk_together(pet_t *p, uint32_t pasos, uint32_t ahora);
void pet_walk_diary(pet_t *p, uint32_t pasos, uint32_t ahora); /* (1.0.25) anota en el diario un paseo que no contó como excursión (300 pasos o más) */
/* (fw 104, pruebas en la placa y el simulador) */
int pet_papelito_test(pet_t *p, int lo_trajo_ella, uint32_t ahora); /* el próximo papelito ya; -1 si están todos */
void pet_ice_test(pet_t *p, int que, uint32_t ahora);                 /* 1 pesca el hielo, 2 se derrite */ /* (101b) tocarlo ahora le dejaría un capítulo (no cambia nada) */
/* Tilde, la urraca: de visita cada 3–4 días jugados, de día */
bool pet_tilde_here(const pet_t *p, uint32_t ahora);
int pet_tilde_gossip(const pet_t *p);                   /* el chisme de esta visita (-1 ninguno) */
int pet_tilde_takes(const pet_t *p);                    /* el objeto que se lleva en esta visita (-1 ninguno) */
int pet_tilde_returns(const pet_t *p);                  /* el objeto que devuelve en esta visita (-1 ninguno) */
void pet_tilde_touch(pet_t *p, int que, uint32_t ahora); /* que 0: escuchaste el chisme, 1: leíste la nota */
const char *pet_tilde_note(const pet_t *p, char *buf, int n); /* la nota con que devuelve lo que se llevó */
/* la Abuela Brizna: despierta 5 veces, más o menos una por semana de juego, cuando ella está en la isla */
bool pet_brizna_awake(const pet_t *p, uint32_t ahora, int *recuerdo); /* recuerdo -1 = solo saluda */
void pet_brizna_heard(pet_t *p, uint32_t ahora);
const char *pet_brizna_hello(int i);                    /* los saludos de después del final */
/* las páginas del diario de Tilo: vuelven al Libro al descubrir los secretos (y en dos momentos más) */
bool pet_tilo_page(const pet_t *p, int k);
int pet_tilo_page_new(const pet_t *p);                  /* una página que todavía no vio aparecer (-1 ninguna) */
void pet_tilo_page_seen(pet_t *p, int k);
/* lo que dicen los animales: cuál animal (ficha) lo dice y el texto */
int pet_animal_phrase_card(int i);
bool pet_animal_tell(pet_t *p, int ficha, uint32_t ahora); /* (v39) lo tocaste (o se quedó con ella): cuenta su frase si le toca hoy */
const char *pet_animal_phrase(int i, bool corto);
bool pet_animal_phrase_known(const pet_t *p, int i);
/* los lugares secretos: la casita escondida de Tilo (bosque, semana 2–3) y el mirador del último paseo (cima de la colina, semana 4) */
#define PET_LUGARES 2
#define PET_CASITA_COSAS 4  /* el horno, la camita, el gancho de la cuchara, la mesa con el diario */
#define PET_MIRADOR_COSAS 3 /* la piedra donde se sentaba, la corona de hojas, la nube del último paseo */
#define PET_ZONA_CLARO 2    /* (v39) el claro de las luciérnagas: pet_secret_open(p, PET_ZONA_CLARO); adentro está la entrada a la casita */
bool pet_secret_open(const pet_t *p, int lugar);
void pet_zone_hint_done(pet_t *p, int zona);         /* (v39) ya le recordó cómo volver (0 el claro, 1 el mirador) */
bool pet_zone_hint_pending(const pet_t *p, int zona);
int pet_secret_things(int lugar);
bool pet_secret_found(const pet_t *p, int lugar, int cosa);
bool pet_secret_find(pet_t *p, int lugar, int cosa, uint32_t ahora); /* la tocó: true si es la primera vez */
const char *pet_secret_place_name(int lugar);
const char *pet_secret_text(int lugar, int cosa, bool corto);
const char *pet_secret_title(int lugar, int cosa);
/* las leyendas de los objetos según el acto, y lo que ella dice al usarlo */
const char *pet_object_legend(const pet_t *p, int id);
const char *pet_object_says(int id, bool corto);
/* el saludo de una criatura después del final secreto */
const char *pet_creature_after(magica_t c);
int pet_story_count(const pet_t *p, int c);   /* historias que ya conoce de c (0..CRI_COUNT = Nimba) */
bool pet_story_known(const pet_t *p, int c, int k);
bool pet_book_tutorial_pending(const pet_t *p, int *quien);
void pet_book_tutorial_done(pet_t *p);
void pet_creature_call(pet_t *p, magica_t c, uint32_t ahora); /* para probar: que se asome ahora */
bool pet_creature_invite_now(pet_t *p, magica_t c, uint32_t ahora); /* para probar: que llegue su invitación */

/* ---------- El país (versión 24): decide las estaciones y si hay Fiestas Patrias (por ahora, solo Chile) ---------- */
typedef enum {
    PAIS_NINGUNO = 0, PAIS_CHILE, PAIS_ARGENTINA, PAIS_URUGUAY, PAIS_PARAGUAY, PAIS_BOLIVIA, PAIS_PERU, PAIS_ECUADOR,
    PAIS_COLOMBIA, PAIS_VENEZUELA, PAIS_MEXICO, PAIS_ESPANA, PAIS_EEUU, PAIS_OTRO_SUR, PAIS_OTRO_NORTE, PAIS_COUNT
} pais_t;
/* (versión 25) hay unos 200 países (los de arriba son los primeros, en el mismo orden) */
int pet_countries(void);
const char *pet_country_name(int c);
void pet_set_country(pet_t *p, int c); /* también fija el hemisferio */
/* busca países que calcen con lo que escribiste (sin importar tildes ni mayúsculas; al principio de una palabra);
 * devuelve cuántos encontró (hasta max) */
int pet_country_find(const char *texto, int *res, int max);
/* un país que no está en la lista (o inventado): la estación que hay ahora decide el hemisferio */
void pet_set_country_by_season(pet_t *p, estacion_t ahora_es, uint32_t ahora);
bool pet_yellow_flowers_day(const pet_t *p, uint32_t ahora); /* hoy es el Día de las Flores Amarillas (donde se celebra) */

/* ---------- Las fiestas con todos (versión 24) ----------
 * En Navidad, Año Nuevo, Halloween, Fiestas Patrias (solo en Chile) y su cumpleaños, sus amigos del barrio, las
 * criaturas que conoció y algún animal amigo vienen al prado a celebrar con ella, una vez por fiesta */
#define FIESTA_SU_CUMPLE 7 /* (además de los fecha_t) */
#define FIESTA_FLORES 8    /* (versión 25) el Día de las Flores Amarillas */
int pet_party_now(const pet_t *p, uint32_t ahora);        /* fecha_t o FIESTA_SU_CUMPLE si toca celebrar ahora; -1 si no */
int pet_party_guests(const pet_t *p, int *amigos, int max); /* los amigos del barrio que conoce (índices de nombre) */
const char *pet_friend_name(const pet_t *p, int i);    /* el nombre de su amigo i (inventado la primera vez, siempre el mismo) */
int pet_friends_max(const pet_t *p);                      /* cuántos amigos puede llegar a tener (32; 20 en partidas viejas) */
/* un nombre tierno inventado con sílabas (Lumi, Tavo, Pelusa...): siempre el mismo para la misma semilla */
const char *pet_make_name(uint32_t semilla, char *out, int n);
int pet_friend_look(int i);                                /* su aspecto (especie y color), siempre el mismo */
void pet_party_done(pet_t *p, int fiesta, uint32_t ahora); /* celebraron: regalos, alegría y el diario */

/* ---------- El juego de los cristales (versión 24): Simón dice en la cueva ----------
 * Tres rondas; al ganar la tercera despierta el dragón de cristal (el secreto de la cueva) */
void pet_crystal_game(pet_t *p, int rondas, uint32_t ahora);
/* ---------- (versión 38) El modo libre de los cristales y "nuestra canción" ----------
 * Tocan libre (sin rondas). Al terminar una pieza (3 s sin tocar, después de 6+ notas) la interfaz avisa con estilo = PIEZA_*
 * (rápida, lenta, larga, repetida). El motor responde con EV_PIEZA: a = lo que pasa (bits PIEZA_*), b = objeto que le dieron + 1. */
#define PIEZA_RAPIDA 1
#define PIEZA_LENTA 2
#define PIEZA_LARGA 4     /* 20 notas o más */
#define PIEZA_REPETIDA 8  /* repitió la misma frase 2-3 veces */
#define PIEZA_ESCAMA 16   /* (respuesta) varios días tocando: el dragón le regala una escama de cristal */
#define PIEZA_CANCION 32  /* (respuesta) proponen hacer una canción (bit 64: lo propone el dragón) */
#define PIEZA_DRAGON 64
#define PIEZA_REGALO 128  /* (respuesta) una pieza larga: a veces un regalo */
#define PET_PIEZA_MIN 6   /* menos notas que esto no es una pieza (nadie reacciona) */
int pet_crystal_piece(pet_t *p, int notas, int estilo, uint32_t ahora);
/* El ritmo: de los toques (cristal y milisegundos desde el toque anterior) a notas con pasos parejos */
int pet_song_from_taps(const uint8_t *cristal, const uint16_t *ms, int n, uint8_t *out, int max);
/* Ella propone cómo sigue una frase (en el mismo estilo): devuelve cuántas notas dejó en out (variante = otra propuesta) */
int pet_song_continue(const uint8_t *frase, int n, uint8_t *out, int max, int variante);
int pet_song_save(pet_t *p, int cual, const uint8_t *notas, int n, const char *nombre, uint32_t ahora); /* cual -1: la primera libre (la principal si no hay); devuelve dónde quedó o -1 */
const cancion_t *pet_song(const pet_t *p, int k);  /* NULL si no hay canción ahí */
int pet_song_count(const pet_t *p);
void pet_song_delete(pet_t *p, int k);
void pet_song_use(pet_t *p, int k, int para);      /* para 0: alarmas, 1: canción de cuna; k -1 = la de siempre */
int pet_song_for(const pet_t *p, int para);        /* la canción elegida para eso, o -1 */
bool pet_song_can_compose(const pet_t *p);         /* (no en modo infante) */
/* ---------- (versión 38) la pesca en el hielo y otros efectos de objetos ---------- */
bool pet_lake_frozen(const pet_t *p, uint32_t ahora); /* invierno, o la bebida helada del norte */
bool pet_floating(const pet_t *p, uint32_t ahora);    /* el polvo de hadas: flota un poquito */
bool pet_always_clean(const pet_t *p, uint32_t ahora); /* el broche: no se ensucia */
/* ---------- (versión 38) la casita se hace con ramitas y musgo ---------- */
#define PET_CASITA_RAMITAS 3
#define PET_CASITA_MUSGO 3
int pet_materials(const pet_t *p, int *ramitas, int *musgo);
extern float pet_visita_escala; /* 1 = normal; 0 = sin visitas (pruebas) */
bool pet_visit_now(pet_t *p, uint32_t ahora); /* probador: llega una visita ya */
void pet_jump_bed(pet_t *p, uint32_t ahora); /* saltó en la cama de su casita: se divierte */
void pet_pochi(pet_t *p, int fase, uint32_t ahora); /* la chihuahua visitante (ver PET_POCHI_F_*) */
/* ---------- (fw 102) Pochi es la mamá de cuatro cachorros y se acuerda de ti ----------
 * fases de pet_pochi: 0 llega, 1 comió, 2 un lugar del paseo, 3 se despide, 4 te salpicó de barro, y además: */
enum {
    PET_POCHI_F_CONSUELA = 5,   /* estabas triste o enferma: te lamió la cara / te trajo algo */
    PET_POCHI_F_ACURRUCA = 6,   /* dormías: llegó en puntillas y se acurrucó a tu lado */
    PET_POCHI_F_PANZA = 7,      /* la tocaste varias veces: panza arriba para que la rasques */
    PET_POCHI_F_MANIA = 8,      /* una de sus manías (la ramita robada, las luciérnagas...) */
    PET_POCHI_F_MOJADA = 9,     /* llegó mojada por la lluvia y se sacudió encima */
    PET_POCHI_F_FIN = 10,       /* 10 + PET_POCHI_ULT_*: cómo terminó la visita (se acuerda la próxima vez) */
    PET_POCHI_F_ANIMAL = 14,    /* ella o un cachorro se encontraron con un animal del lugar */
    PET_POCHI_F_AMIGO = 30,     /* 30 + k: el cachorro k estuvo con su animal amigo (la primera vez se hacen amigos) */
    PET_POCHI_F_CACHORRO = 20,  /* 20 + k: el cachorro k jugó contigo (y ya lo conoces) */
};
enum { PET_POCHI_ULT_NADA = 0, PET_POCHI_ULT_PASEO, PET_POCHI_ULT_COMIO, PET_POCHI_ULT_HOYNO }; /* paseó / comió sin pasear / le dijiste «hoy no» */
#define PET_POCHI_CACHORROS 4
enum { POCHI_CHAPI = 0, POCHI_PASITA, POCHI_MORITA, POCHI_RONY };
const char *pet_pochi_pup_name(int k);                   /* "Chapi", "Pasita", "Morita", "Rony" */
int pet_pochi_visits(const pet_t *p);                    /* cuántas veces vino */
int pet_pochi_last(const pet_t *p);                      /* cómo terminó la visita anterior (PET_POCHI_ULT_*) */
bool pet_pochi_pup_met(const pet_t *p, int k);
bool pet_pochi_pup_friend(const pet_t *p, int k);        /* ya se hizo amigo de su animal (Chapi: la rana; Pasita: la mariposa; Morita: el caracol; Rony: la ardilla) */
int pet_pochi_pup_pick(const pet_t *p, uint32_t ahora);  /* el primero de los que la acompañan en esta visita (-1 ninguno) */
int pet_pochi_pups_pick(const pet_t *p, uint32_t ahora, int out[PET_POCHI_CACHORROS]); /* los cachorros de esta visita: 0, 1, 2 o los 4 */
/* ---------- (versión 39) Pochi en la historia, la página suelta y el segundo final ----------
 * Pochi: en cada visita, una charla corta (3 por acto) en la que pregunta y ella responde con lo que ya sabe.
 * Después del segundo final, una charla especial: ella le cuenta el nombre. No gastan momentos de historia. */
#define PET_POCHI_CHARLAS 9
#define PET_POCHI_NOMBRE 9
int pet_pochi_chat_next(const pet_t *p);                  /* la charla de esta visita (-1 ninguna) */
void pet_pochi_chat_done(pet_t *p, int k, uint32_t ahora);
bool pet_pochi_knows_name(const pet_t *p);
const char *pet_pochi_chat(int k, int linea, bool corto); /* linea 0: Pochi pregunta; 1: ella responde; 2: Pochi reacciona */
/* la página suelta del diario (objeto 111): devuelve la página que se ve (nueva si *nueva), o -1 */
int pet_loose_page(pet_t *p, uint32_t ahora, bool *nueva);
/* el segundo final, «El último verso»: la canción de Tilo (su verso con nuestra canción, terminada) */
int pet_tilo_song(const pet_t *p, uint8_t *out, int max);  /* notas como en cancion_t (0 si todavía no la terminaron) */
int pet_tilo_verse(uint8_t *out, int max);                  /* su verso desafinado, sin terminar */
bool pet_final2_ready(const pet_t *p, uint32_t ahora);       /* (pruebas) ya puede empezar */
bool pet_book_new(const pet_t *p, int narr);                 /* el narrador tiene algo nuevo sin mirar (la estrellita del Libro) */
void pet_book_seen(pet_t *p, int narr);
void pet_in_zone(pet_t *p, bool si);                          /* (v39) entraron o salieron del claro/casita/mirador */
/* ---------- (versión 40) lo escondido en los lugares secretos y las excursiones sola ----------
 * En el claro, la casita y el mirador hay, una vez al día cada uno, algo escondido: musgo, ramitas o un regalito (un objeto
 * mágico). El chucao lo canta en el claro; en la casita y el mirador brilla un poquito. Por el día, si está tranquila en el
 * bosque o en el cerro, a veces se va sola al claro o al mirador y vuelve a los minutos (a veces con lo escondido de hoy).
 * De noche nunca va sola al mirador; al claro, tampoco después de las 20 (vuelve al prado a dormir). */
#define PET_ZONAS_SECRETAS 3
enum { PET_ZH_NADA = 0, PET_ZH_MUSGO, PET_ZH_RAMITAS, PET_ZH_REGALO, PET_ZH_CARTA /* (fw 104) un papelito del bosque */ };
int pet_zone_find(const pet_t *p, int zona, uint32_t ahora);       /* qué hay escondido hoy (PET_ZH_*; NADA si ya lo sacó) */
int pet_zone_find_spot(const pet_t *p, int zona, uint32_t ahora);  /* en cuál de los tres escondites está hoy (0..2) */
int pet_zone_find_take(pet_t *p, int zona, uint32_t ahora);        /* lo sacó: lo guarda (el regalito: id del objeto en *_objeto) */
int pet_zone_find_object(const pet_t *p);                           /* el objeto del último regalito (-1: ninguno) */
int pet_excursion_wants(const pet_t *p, int escena, uint32_t ahora); /* ¿quiere irse sola de paseo? -1 no; 0 al claro, 1 al mirador */
int pet_excursion_back(pet_t *p, int zona, uint32_t ahora);        /* volvió: anota en el diario; devuelve lo que encontró (PET_ZH_*; 101b: se lo queda ella) */

/* ---------- (versión 41) Los juegos de los lugares secretos ----------
 * Cinco juegos que se repiten: puntaje, récord, de 1 a 3 estrellas y premios (las dos primeras rondas de cada juego al día):
 * 1 estrella una ramita, 2 una ramita y musgo, 3 dos ramitas y musgo; la primera vez que saca 3 estrellas en un juego, un
 * regalito (un objeto mágico) si queda alguno. */
#define PET_JUEGOS_LUGAR 5
enum { PET_JL_GALLETAS = 0, PET_JL_CUNA, PET_JL_SALTO, PET_JL_HOJAS, PET_JL_PIEDRA };
#define PET_JL_PREMIOS_DIA 2
enum { PET_JLR_RECORD = 1, PET_JLR_ESTRELLAS = 2, PET_JLR_PREMIO = 4, PET_JLR_REGALO = 8 };
int pet_place_game_record(const pet_t *p, int jg);
int pet_place_game_stars(const pet_t *p, int jg);
int pet_place_game_result(pet_t *p, int jg, int puntos, int estrellas, uint32_t ahora); /* bits PET_JLR_* */
/* (1.0.17) los juegos de a dos con una visita (el amigo del barrio o un visitante): ping pong, tira y afloja, memorice
 * y escondidas. resultado: 0 perdió ella, 1 empate, 2 ganó ella. Divierte (más si gana, pero perder con un amigo
 * también es lindo), cansa un poco y suma amistad con el amigo del barrio si está. Una vez al día queda en el diario.
 * Devuelve 1 si contó (0: huevo, dormida o juego que no existe). */
enum { PET_DOS_PING = 0, PET_DOS_CUERDA, PET_DOS_MEMO, PET_DOS_ESCONDE, PET_DOS_COUNT };
int pet_duo_game(pet_t *p, int juego, int resultado, uint32_t ahora);
/* (1.0.18) los juegos de la isla: el FARO (guiar barquitos de noche hasta el muelle) y el BUCEO con Marea (perlas y
 * conchas). puntos: barquitos a salvo o cosas encontradas. Divierte, cansa un poco; con buen puntaje encuentra una pieza
 * de la colección de conchas (si le falta alguna). Queda en el diario (una vez por juego cada vez que se enciende el orbe).
 * Devuelve 1 si contó, 2 si además encontró una concha nueva para su colección. */
enum { PET_ISLA_FARO = 0, PET_ISLA_BUCEO, PET_ISLA_ARCOIRIS, PET_ISLA_COUNT }; /* (1.0.31: el tobogán del arcoíris, aunque no es de la isla) */
int pet_island_game(pet_t *p, int juego, int puntos, uint32_t ahora);
void pet_place_game_prize(int *ramitas, int *musgo, int *objeto);                      /* lo que ganó en la última ronda */
int pet_place_game_seed(const pet_t *p, int jg, uint32_t ahora);                       /* cambia cada día (la piedra tibia) */

/* ---------- El secreto de cada zona (versión 23) ---------- */
bool pet_special_known(const pet_t *p, especial_t e);
/* ¿se puede ahora? el anillo de hadas (bosque, de noche con luna), el reflejo (lago, de noche), la cometa (colina, con viento) */
bool pet_special_ready(const pet_t *p, especial_t e, uint32_t ahora);
void pet_special_do(pet_t *p, especial_t e, int resultado, uint32_t ahora); /* lo hizo: resultado según cada uno */
#define PET_MELODIA_LARGO 5
int pet_crystal_note(pet_t *p, int cristal, uint32_t ahora); /* tocas un cristal: 1 bien, 0 mal, 2 completó la melodía */
const char *pet_constellation_name(int k);
const char *pet_constellation_legend(int k);
#define PET_CONSTELACIONES 6
const char *pet_hint_text(especial_t e, int k);

/* "a" para ella, "o" para él, y así: la palabra según si es niña o niño */
const char *pet_g(const pet_t *p, const char *nina, const char *nino);
/* cambia las marcas {a} {la} {ella} {una} {guardiana} (y en MAYÚSCULAS) según sea niña o niño */
const char *pet_gender_text(const pet_t *p, const char *src, char *out, int n);

/* ---------- La luna, la ballena, la Guardiana, rituales, la familia (versión 21) ---------- */
/* fase de la luna de verdad (0 nueva, 0.5 llena), calculada de la fecha */
float pet_moon_phase(uint32_t ahora);
bool pet_full_moon(uint32_t ahora);            /* la noche de luna llena (un día antes y uno después) */
int pet_nights_to_full_moon(uint32_t ahora);   /* 0 si es esta noche */
/* el paseo por el firmamento en la ballena de nubes: ¿se puede ahora? y al terminar, cuántas estrellas tocaste.
 * Con cinco o más trae polvo de estrellas (Talco de estrellas) a la Bolsa. Devuelve 1 si trajo algo */
bool pet_whale_can_ride(const pet_t *p, uint32_t ahora);
int pet_whale_ride(pet_t *p, int estrellas, uint32_t ahora);
/* el rito de la Guardiana: adulta, con el Corazón del Mundo, de noche en la isla y con luna llena */
bool pet_guardian_ready(const pet_t *p, uint32_t ahora);
/* bailó con la caja de música: cuántas veces seguiste el ritmo */
void pet_dance(pet_t *p, int aciertos, uint32_t ahora);
/* ¿aprendió su ritual? 0 buenos días, 1 buenas noches */
bool pet_ritual(const pet_t *p, int cual);
/* la familia: el hijo nace del huevo que dejó (en otro perfil): su forma, su color, algo de su manera de
 * ser y lo que ella sabía del libro mágico. La madre queda sin el huevo pendiente. */
void pet_family_child(pet_t *madre, pet_t *hijo, uint32_t ahora, uint32_t seed);
/* la despedida natural: tú dices que todavía no (vuelve a preguntar en un mes) */
void pet_trip_not_yet(pet_t *p, uint32_t ahora);
/* visitas entre perfiles del mismo reloj: la otra viene a jugar (las dos lo anotan) */
bool pet_profile_visit(pet_t *p, pet_t *otra, uint32_t ahora);
/* las alarmas "suena siempre" de una mascota que no está activa: devuelve la que suena ahora (o -1) */
int pet_alarm_resting(pet_t *p, uint32_t desde, uint32_t ahora);
/* fotos de la cámara de esta mascota: anotar una nueva (si ya tiene el máximo, olvida la más vieja y la
 * devuelve para borrarla; si no, 0); y si una foto es suya (o de antes, compartida) */
int pet_photo_add(pet_t *p, int id);
bool pet_photo_mine(const pet_t *p, int id);
void pet_photo_forget(pet_t *p, int id);
void pet_photo_legacy(pet_t *p, int hasta);

/* ---------- Ciclo de vida ---------- */

/* Crea una mascota nueva (un huevo). seed define su personalidad y gustos. */
void pet_init(pet_t *p, const char *nombre, uint8_t especie, uint32_t ahora, uint32_t seed);

/* El huevo nace a los PET_HUEVO_SEGUNDOS; acariciarlo lo adelanta.
 * pet_hatch lo hace nacer ya (pruebas). */
void pet_hatch(pet_t *p, uint32_t ahora);
/* (1.0.10) el primer huevo llega al prado: desde AHORA cuenta sus PET_HUEVO_SEGUNDOS (5 min) para nacer
 * (antes de poner el reloj la hora no sirve para contar) */
void pet_egg_start(pet_t *p, uint32_t ahora);
bool pet_is_egg(const pet_t *p);
uint32_t pet_hatch_remaining(const pet_t *p, uint32_t ahora);
void pet_set_name(pet_t *p, const char *nombre);

/* Edad del dueño: menores de PET_EDAD_INFANTE juegan en modo infante (sin textos), de ahí a
 * PET_EDAD_NINO en modo niño (textos cortos); en ambos sin castigos y muy expresiva. */
void pet_set_owner_age(pet_t *p, int edad);
bool pet_child_mode(const pet_t *p); /* niño o infante */
modo_juego_t pet_mode(const pet_t *p);
const char *pet_mode_name(modo_juego_t m);
/* Fecha de nacimiento del dueño (se pregunta al empezar): calcula la edad y el modo, y
 * cada año celebra el cumpleaños (EV_CUMPLE). Devuelve false si la fecha no es válida. */
bool pet_set_owner_birthdate(pet_t *p, int dia, int mes, int anio, uint32_t ahora);
int pet_owner_age(const pet_t *p, uint32_t ahora);       /* 0 si no se sabe */
bool pet_owner_birthday(const pet_t *p, uint32_t ahora); /* hoy es tu cumpleaños */
/* Fecha local (para la interfaz) */
void pet_local_date(const pet_t *p, uint32_t ahora, int *anio, int *mes, int *dia);

/* Fase 4 */
vinculo_nivel_t pet_bond_level(const pet_t *p);
fecha_t pet_special_date(const pet_t *p, uint32_t ahora);   /* hoy (o FECHA_NINGUNA) */
bool pet_her_birthday(const pet_t *p, uint32_t ahora);       /* hoy cumple semanas/meses/años */
int pet_goal_target(meta_t m);                               /* cuántas veces hay que hacerla */
/* Diario y álbum: i = 0 es lo más reciente */
/* Los finales: el de siempre es el rito de la Guardiana del Mundo (con el Corazón, el último objeto).
 * El secreto, "El libro se escribe solo", llega después, cuando además usó una vez cada uno de los 150 objetos. */
#define PET_FINAL_REGULAR 1 /* (el rito de la Guardiana) */
#define PET_FINAL_SECRETO 2
int pet_objects_used(const pet_t *p);                /* cuántos objetos ya usó una vez (sabe qué hacen) */
int pet_final_pending(const pet_t *p);               /* 0 nada; PET_FINAL_SECRETO: la pantalla debe mostrarlo ahora */
void pet_final_done(pet_t *p, int cual, uint32_t t); /* ya lo vieron: lo deja guardado */
/* Las páginas del Libro: el Libro cuenta el primer libro del cuento en ocho momentos, una sola vez cada uno
 * (0 prólogo, 1 nace, 2 la isla, 3 la primera carta, 4 el primer secreto, 5 Prisma, 6 su melodía, 7 el Corazón). */
#define PET_LIBRO_PAGINAS 8
#define PET_HISTORIAS_DIA 2 /* las criaturas cuentan a lo más dos historias por día: así no se agotan tan rápido */
bool pet_libro_seen(const pet_t *p, int n);
void pet_libro_mark(pet_t *p, int n);
bool pet_final_seen(const pet_t *p, int cual);
/* Comidas de temporada: cada estación tiene las suyas y se recogen en ciertas zonas (explorando, olfateando, paseando) */
#define PET_COMIDAS_EST 16
#define PET_COMIDA_EST_BASE 8 /* ACT_ALIMENTAR: arg = 8 + comida_est_t */
typedef struct {
    const char *nombre;    /* "las castañas" */
    const char *corto;     /* "castañas" */
    uint8_t estacion;      /* estacion_t en la que se encuentra */
    uint8_t zonas;         /* bits de escena_t donde se recoge */
    uint8_t saciedad, salud, diversion, abrigo;
} comida_est_info_t;
const comida_est_info_t *pet_seasonal_food_info(int i);
int pet_seasonal_food_count(const pet_t *p, int i);
bool pet_seasonal_food_seen(const pet_t *p, int i);
int pet_seasonal_food_kinds(const pet_t *p);       /* cuántas distintas tiene ahora */
int pet_seasonal_food_taste(const pet_t *p, int i); /* 0 no le gusta, 1 normal, 2 le encanta */
void pet_seasonal_food_give(pet_t *p, int i, int n); /* (pruebas) */
bool pet_juego_mas(const pet_t *p);            /* tras el final secreto: los objetos pueden venir en versión dorada */
bool pet_gold_has(const pet_t *p, int id);
/* --- los objetos con efecto propio (versión 33) --- */
int pet_guarded_object(const pet_t *p);               /* el objeto que cuida el candado rúnico (-1 = ninguno) */
bool pet_chest_visible(const pet_t *p);               /* el cofre bajo el árbol viejo (tras el final secreto, mientras no se abra) */
int pet_chest_status(const pet_t *p);                 /* 0 no hay, 1 hay y faltan llave y candado, 2 falta la llave, 3 falta el candado, 4 se puede abrir */
int pet_chest_open(pet_t *p, uint32_t ahora);         /* lo abre con la llave y el candado: cuántos dorados salieron (<=0: no se pudo) */
bool pet_nests_visible(const pet_t *p, uint32_t ahora); /* con las gafas del explorador puestas, en el prado y el bosque */
int pet_nest_look(pet_t *p, int i, uint32_t ahora);   /* mira el nido i (0..2): objeto que había (id), -1 solo una plumita, -2 ya estaba vacío */
bool pet_nest_empty(const pet_t *p, int i, uint32_t ahora);
int pet_size_mode(const pet_t *p, uint32_t ahora);    /* 0 normal, 1 chiquitita, 2 grande */
bool pet_invisible(const pet_t *p, uint32_t ahora);
bool pet_colors_on(const pet_t *p, uint32_t ahora);
int pet_ink_notes(const pet_t *p);                    /* cuántas notas escondidas ya reveló */
const char *pet_ink_note(int i);                      /* el texto de la nota i */
#define PET_TINTA_NOTAS 12
#define PET_NIDOS 3
#define PET_COFRE_DORADOS 6
int pet_gold_count(const pet_t *p);
void pet_music_heard(pet_t *p, uint32_t ahora, int tempo, int bpm, int energia); /* tempo: 0 lenta, 1 media, 2 rápida; bpm: golpes por minuto; energia 0..3 (volumen) */
bool pet_has_song(const pet_t *p);
/* Su melodía propia: la compone sola con los cristales de la cueva (varios días de ensayo) y, una vez decidida, no cambia. Devuelve cuántas notas hay (0 si aún no)
 * y las deja en notas[] como cristales 0..5 (se tocan con SND_CRISTAL_AZUL + nota; las notas, de grave a agudo, son los cristales 0, 4, 1, 2, 5, 3). */
int pet_melody(const pet_t *p, uint8_t *notas);
void pet_inject_event(pet_t *p, int tipo, int a, int b, uint32_t ahora); /* solo para pruebas y el simulador: hace como si hubiera pasado */ /* ya tiene "su canción" (la reconoció en 3 ocasiones distintas) */ /* suena música cerca: baila y, la primera vez, dice que le encanta */
const char *pet_activity_verb(int actividad); /* "bailar", "jugar con la pelota"… (vacío si no es de las que aprende) */
int pet_taste_low(const pet_t *p); /* la actividad de la que más se aburrió (gusto bajo) o -1 */
int pet_taste_top(const pet_t *p); /* la actividad que más le gusta (si ya aprendió algo claro) o -1 */
int pet_taste(const pet_t *p, int actividad); /* lo aprendido: -60 (la evita) .. 60 (la busca) */
int pet_poops_here(const pet_t *p); /* cacas para recoger en la zona donde está */
int pet_diary_count(const pet_t *p);
const entrada_diario_t *pet_diary_get(const pet_t *p, int i);
int pet_album_count(const pet_t *p);
const foto_t *pet_album_get(const pet_t *p, int i);

/* Su amigo del barrio: ¿está de visita? */
bool pet_neighbor_here(const pet_t *p);
/* ¿Se puede pescar ahora? (en el lago, despierta y sin hielo) */
bool pet_can_fish(const pet_t *p, uint32_t ahora);
/* v89: pesca con minijuego. arg 0: empezó (la pesca no se termina sola); 1: no picó; 2: se escapó; 3: lo sacó (lo que sale depende de la estación y la hora) */
void pet_fish_game(pet_t *p, int arg, uint32_t ahora);
/* (1.0.32) La colección de la pesca: 30 animales del agua (peces, cangrejos y algunos raros), cada uno en su estación,
 * su hora y su clima. Cuando sale un pez, un cangrejo o el pez dorado, EV_PESCA trae en b: bit0 se lo comió,
 * bits 1-5 = 1 + cuál (0: ninguno), bit6 = es nuevo en la colección. El diario guarda en b 1 + cuál. */
#define PET_PECES 30
bool pet_fish_species_known(const pet_t *p, int k);
int pet_fish_species_found(const pet_t *p);
const char *pet_fish_species_name(int k);          /* «Trucha arcoíris» */
const char *pet_fish_species_with_article(int k);  /* «una trucha arcoíris» */
const char *pet_fish_species_desc(int k);
void pet_fish_species_when(int k, char *buf, int n); /* «En primavera, de mañana.» */
void pet_fish_species_force(int k);                /* (pruebas) que el próximo sea este (-1: al azar) */

/* Varias mascotas en el mismo reloj: la que no está se queda "descansando" guardada (el tiempo no
 * corre para ella). Al volver a traerla, toma del reloj lo que es de todos (tus datos, la hora y el
 * clima, el jardín, las decoraciones, el baúl, las semillas y los amigos) y sigue donde quedó. La edad y el
 * cumpleaños del dueño quedan con cada mascota (puede ser de otra persona de la casa). */
#define PET_GUARDADAS 3
void pet_resume(pet_t *p, const pet_t *mundo, uint32_t ahora);

/* Mascota nueva: conserva amigos, decoraciones y la edad del dueño. */
void pet_rebirth(pet_t *p, uint8_t especie, uint32_t ahora, uint32_t seed);
/* como pet_rebirth, pero sin heredar nada del mundo anterior (mascota nueva elegida por la persona) */
void pet_new_fresh(pet_t *p, uint8_t especie, uint32_t ahora, uint32_t seed);

/* Estación según la fecha local y el hemisferio; clima actual */
estacion_t pet_season(const pet_t *p, uint32_t ahora);
clima_t pet_weather(const pet_t *p);

/* carácter, planes y trato */
int pet_humor(const pet_t *p);                   /* humor_t de hoy */
int pet_plan(const pet_t *p);                    /* plan_t en marcha (PLAN_NINGUNO si no hay) */
int pet_plan_step(const pet_t *p);               /* en qué paso va (0..2) */
const char *pet_humor_name(int humor);           /* "aventurera" */
const char *pet_plan_name(int plan);             /* "ir a la laguna" */
int pet_plan_activity(int plan, int paso);       /* actividad_t de ese paso (-1 si no tiene) */
int pet_plan_scene(int plan);                    /* escena_t adonde va (-1 si no sale de donde está) */
int pet_treat(const pet_t *p, uint32_t ahora);   /* cómo la has tratado, con el tiempo ya aplicado (-100..100) */
/* sus manías, que son de ella y no cambian: la actividad que más le gusta y la que menos, y una que hace a cierta hora */
void pet_quirks(const pet_t *p, int *favorita, int *esquiva, int *hora_actividad, int *hora);

void pet_force_weather(pet_t *p, clima_t c, uint32_t ahora, uint32_t segundos);

/* Zona horaria local en segundos respecto de UTC (afecta día y noche). */
void pet_set_timezone(pet_t *p, int32_t offset_s);
/* La hora del reloj quedó antes de la última vez que se usó (se quedó sin batería y perdió
 * la hora): hay que preguntarla antes de seguir. */
bool pet_clock_behind(const pet_t *p, uint32_t ahora);
/* viaje en el tiempo: mueve todas sus horas guardadas (delta puede ser negativo) para que el mundo siga igual en otra fecha */
void pet_time_shift(pet_t *p, int32_t delta);

/* Avanza la simulación hasta "ahora". Maneja saltos largos (horas o días). */
void pet_update(pet_t *p, uint32_t ahora);

/* Acción del dueño. arg: food_t para alimentar, gesto_t para acariciar.
 * Devuelve +1 si le gustó, 0 neutral, -1 si no le gustó, -2 si no quiso. */
int pet_action(pet_t *p, action_t a, int arg, uint32_t ahora);

/* Decoraciones: se ponen con semillas y piedras brillantes que ella encuentra.
 * 1 = puesta, 0 = faltan cosas, -1 = ya está o inválida. */
void pet_decor_cost(decor_t d, int *semillas, int *piedras); /* (v38: la casita no cuesta semillas ni piedras sino ramitas y musgo, ver PET_CASITA_*) */
int pet_place_decor(pet_t *p, decor_t d, uint32_t ahora);
bool pet_has_decor(const pet_t *p, decor_t d);
/* guardar o volver a poner una decoración que ya tiene (sin volver a pagarla) */
void pet_toggle_decor(pet_t *p, decor_t d);
bool pet_decor_visible(const pet_t *p, decor_t d);
uint32_t pet_decor_mask(const pet_t *p); /* las que se ven (para el fondo) */

/* ---------- Fase 5: creatividad ---------- */
/* Especie, mezcla y colores: se eligen con el huevo, antes del nombre. mezcla = -1 sin mezcla.
 * Vuelve a empezar el huevo (conserva tus datos, el jardín y el baúl). false si ya nació. */
bool pet_choose_look(pet_t *p, int especie, int mezcla, int color, uint32_t ahora, uint32_t seed);
/* Todo el aspecto en un número para el arte: especie | (mezcla) << 3 | color << 7 */
int pet_look(const pet_t *p);
ranura_t pet_clothes_slot(ropa_t r);
bool pet_has_clothes(const pet_t *p, ropa_t r);
int pet_wearing(const pet_t *p, ranura_t s); /* ropa_t o -1 */
static inline int pet_worn_raw(const pet_t *p, int s) { return s == RANURA_MANO ? p->ropa_mano : (unsigned)s < RANURAS_VIEJAS ? p->ropa_puesta[s] : 0; } /* ropa_t + 1 (0 = nada) */
bool pet_has_toy(const pet_t *p, juguete_t j);
bool pet_toy_placed(const pet_t *p, juguete_t j, int *x, int *y); /* posición en pantalla */
/* ¿le gusta? -100..100 (en modo infante nunca es negativo a la vista) */
int pet_clothes_liking(const pet_t *p, ropa_t r);
int pet_toy_liking(const pet_t *p, juguete_t j);
/* qué se escucha ahora de fondo */
ambiente_t pet_ambient(const pet_t *p, uint32_t ahora);
const char *pet_clothes_name(ropa_t r);
const char *pet_toy_name(juguete_t j);
const char *pet_crop_name(cultivo_t c);
const char *pet_ambient_name(ambiente_t a);
const char *pet_color_name(int c);

/* Mensajes del día (9, 12, 15, 18 y 21 h) y cómo se siente en palabras. */
int pet_phrase_count(void);
const char *pet_phrase(int i);
/* la respuesta del botón de cada mensaje */
typedef enum { RESP_ABRAZO = 0, RESP_LO_HARE, RESP_YO_TAMBIEN, RESP_VOY_A_MIRAR, RESP_CON_CARINO, RESP_GRACIAS, RESP_COUNT } respuesta_t;
/* cada mensaje del día tiene un tipo fijo, y el tipo decide el botón para responder */
typedef enum {
    FRASE_ANIMO = 0,  /* te da ánimo o te dice algo bonito -> "¡Gracias!"     */
    FRASE_TE_QUIERE,  /* te dice que te quiere o te extrañó -> "¡Yo también!" */
    FRASE_CUIDATE,    /* te pide que te cuides -> "¡Lo haré!"                  */
    FRASE_MIRA,       /* te invita a mirar algo -> "¡Voy a mirar!"              */
    FRASE_AGRADECE,   /* te da las gracias -> "¡Con cariño!"                    */
    FRASE_ABRAZO,     /* te consuela o te acompaña -> "¡Un abrazo!"             */
    FRASE_TIPOS
} frase_tipo_t;
frase_tipo_t pet_phrase_type(int i);
respuesta_t pet_phrase_reply(int i);
const char *pet_reply_text(respuesta_t r);
/* respondiste el mensaje: un poco de cariño, una vez por mensaje (true si contó) */
bool pet_phrase_hug(pet_t *p, uint32_t ahora);
void pet_feeling(const pet_t *p, char *buf, int n);
/* lo mismo, corto: solo lo que más siente y por qué (para lugares chicos) */
void pet_feeling_short(const pet_t *p, char *buf, int n);

/* Sensación térmica: -1 tiene frío, 0 bien, +1 tiene calor (ya descontado el efecto
 * de una bebida caliente o fresca). Con frío conviene una bebida caliente; con calor, una fría. */
int pet_temperature_feel(const pet_t *p, uint32_t ahora);

/* Mundo: zonas abiertas, animales presentes (en la zona actual), caminos marcados. */
bool pet_scene_open(const pet_t *p, escena_t e);
/* cuánto le gusta una zona (-100..100); el de la isla sale del lago y la colina, con un poquito más */
int pet_scene_liking(const pet_t *p, escena_t e);
/* Qué falta para abrir una zona: 0 ya está abierta, 1 faltan días (dias = cuántos), 2 falta confianza,
 * 3 es un secreto (la cueva: la encuentra olfateando en el bosque o la colina cuando confía mucho) */
int pet_scene_requirement(const pet_t *p, escena_t e, uint32_t ahora, float *dias);
/* ¿visitó ya esa zona? (para avisar de las nuevas) */
bool pet_scene_seen(const pet_t *p, escena_t e);
int pet_scene_forget(pet_t *p, escena_t e, uint32_t ahora); /* la saca de esa zona (al prado) y borra el registro de su primera visita; -1 si no se puede */
/* Jardines: parcela 0..PET_PARCELAS_TOTAL-1; en qué zona está cada una */
const planta_t *pet_plant(const pet_t *p, int parcela);
escena_t pet_plant_scene(int parcela);
int pet_plant_first(escena_t e, int *cuantas); /* primera parcela de la zona (-1 si no tiene) */
/* Semillas: cuántas de cada tipo y si ya conoce ese tipo */
int pet_seeds(const pet_t *p, cultivo_t c);
bool pet_seed_known(const pet_t *p, cultivo_t c);
bool pet_fauna_here(const pet_t *p, fauna_t f, uint32_t ahora);
bool pet_animal_here(const pet_t *p, animal_t a, uint32_t ahora);
const char *pet_animal_name(animal_t a);
void pet_animal_call(pet_t *p, animal_t a, uint32_t ahora); /* para probar: que aparezca ahora */
uint32_t pet_trail_mask(const pet_t *p); /* bit por par de lugares con camino marcado */
int pet_trail_pair(int a, int b);        /* índice del par (a,b) en senda[] y en la máscara */
bool pet_puddles(const pet_t *p, uint32_t ahora);

/* Emociones: intensidad 0..1, su causa y las dos más fuertes. */
float pet_emotion(const pet_t *p, emo_t e);
causa_t pet_emotion_cause(const pet_t *p, emo_t e);
int pet_top_emotions(const pet_t *p, emo_t *primera, emo_t *segunda); /* devuelve cuántas superan el umbral */
/* Gustos ya mostrados: favoritos y odiados (para el panel del adulto) */
int pet_favorite(const pet_t *p, gusto_tipo_t tipo, bool odiado); /* id o -1 */

/* Amigos: la capa de radio llama pet_make_baliza cada pocos segundos para
 * emitir, y pet_on_baliza con cada baliza recibida. */
void pet_make_baliza(const pet_t *p, uint32_t ahora, pet_baliza_t *b);
void pet_on_baliza(pet_t *p, const pet_baliza_t *b, uint32_t ahora);
bool pet_baliza_encode(const pet_baliza_t *b, uint8_t out[PET_BALIZA_BYTES]);
bool pet_baliza_decode(const uint8_t *in, int n, pet_baliza_t *b);
bool pet_friend_nearby(const pet_t *p, int i, uint32_t ahora);
void pet_invite(pet_t *p, int i, uint32_t ahora);
void pet_answer_invite(pet_t *p, bool acepta, uint32_t ahora);
int pet_pending_invite(const pet_t *p, uint32_t ahora); /* índice del amigo o -1 */

/* Saca el siguiente evento para la interfaz. false si no hay. */
bool pet_poll_event(pet_t *p, pet_event_t *out);

/* ---------- Consultas ---------- */
float pet_need(const pet_t *p, need_t n);
mood_t pet_mood(const pet_t *p);
want_t pet_want(const pet_t *p);
sound_t pet_mood_sound(const pet_t *p);
float pet_age_days(const pet_t *p, uint32_t ahora);
uint32_t pet_next_wakeup(const pet_t *p, uint32_t ahora);

/* ---------- Guardado ---------- */
void pet_seal(pet_t *p);
bool pet_validate(const pet_t *p);
/* Una partida de una versión anterior (con los campos nuevos al final): la completa y la sella.
 * p debe tener sizeof(pet_t) bytes, con la partida vieja al comienzo. */
bool pet_upgrade(pet_t *p);

/* ---------- Ajustes del dispositivo ----------
 * Son de la casa, no de cada mascota ni del respaldo: main los guarda aparte (NVS "disp"). */
#define PET_DEVICE_VERSION 11
#define PET_BRILLO_MIN 8           /* nunca la pantalla negra */
#define PET_ALARMA_VOLUMEN_MIN 45  /* la alarma suena aunque el volumen esté en 0 */
#define PET_ALARMA_VOLUMEN 85      /* las alarmas tienen su propio volumen: ninguna barra las calla */
typedef struct {
    uint8_t version;
    uint8_t volumen; /* 0 (silencio) .. 100 */
    uint8_t brillo;  /* PET_BRILLO_MIN .. 100 */
    uint8_t apagado; /* cuándo se apaga la pantalla: 0 = 10 s, 1 = 20 s, 2 = 30 s, 3 = 1 min, 4 = 2 min */
    uint8_t ambiente; /* volumen del sonido de fondo: 0 (apagado) a 100 (versión 2) */
    uint8_t girar;    /* 1: la pantalla se endereza sola si das vuelta el reloj (los de antes quedan en 0) */
    uint8_t efectos;  /* volumen de los efectos (fuegos artificiales, juegos, truenos): 0..100 (versión 3) */
    uint8_t arrullo;  /* modo arrullo: 0 apagado; 3 = grillos mientras ella duerme (apagado de fábrica; versión 5; los 1 y 2, lluvia y mar, ya no existen) */
    uint8_t arrullo_dur; /* cuánto dura el arrullo: 0 = 30 min, 1 = 1 hora, 2 = 2 horas (versión 6) */
    uint8_t musica;   /* 1: escucha con el micrófono si suena música cerca (apagado de fábrica; versión 4) */
    uint8_t fuente;   /* tamaño de las letras de lectura: 0 normal, 1 grande (versión 7) */
    uint8_t alarmas;  /* volumen de las alarmas: PET_ALARMA_VOLUMEN_MIN..100 (en las partidas de antes vale 0: se usa el de siempre, 85) */
    uint8_t arrullo_vol; /* volumen de los grillos del arrullo: 0..100 (versión 9) */
    uint8_t pecera;   /* (versión 11) cuánto dura encendida la pecera con batería: 0 = 10 min, 1 = 30 min, 2 = 1 hora, 3 = no se apaga */
} pet_device_t;
void pet_device_defaults(pet_device_t *d);
void pet_device_fix(pet_device_t *d); /* corrige valores fuera de rango (o de otra versión) */
uint32_t pet_device_screen_ms(const pet_device_t *d);
uint32_t pet_device_aquarium_ms(const pet_device_t *d); /* (v99) la pecera con batería: UINT32_MAX = no se apaga sola */
/* brillo a usar: el elegido, multiplicado en modo pecera (0,55) y de noche u hora de dormir (0,3) */
int pet_device_brightness(const pet_device_t *d, bool pecera, bool noche);
int pet_device_alarm_volume(const pet_device_t *d);
/* tres volúmenes: 0 = ella (sus sonidos), 1 = efectos (fuegos, juegos, truenos), 2 = alarmas (aparte: no las callan las barras) */
int pet_sound_group(sound_t s);
int pet_device_sound_volume(const pet_device_t *d, sound_t s);

/* ---------- Alarmas del dueño y hora de dormir ----------
 * Se revisan en pet_update (suenan con EV_ALARMA, solo si la hora llegó hace menos de 2 minutos). Sin
 * hora segura pet_update no corre: las alarmas quedan en pausa. */
void pet_alarm_set(pet_t *p, int i, bool activa, int hora, int minuto, uint8_t dias, int sonido, bool siempre);
void pet_alarm_delete(pet_t *p, int i);
int pet_alarm_count(const pet_t *p); /* las que existen */
sound_t pet_alarm_sound(int sonido);
const char *pet_alarm_sound_name(int sonido);
/* La respuesta en la pantalla: posponer (vuelve en 5 minutos) o "¡ya desperté!" (prende la luz y, si ya
 * durmió lo suficiente, se despierta contigo). */
void pet_alarm_answer(pet_t *p, int i, bool posponer, uint32_t ahora);
/* Próxima vez que sonará (UTC), o 0 si está apagada */
uint32_t pet_alarm_next(const pet_t *p, int i, uint32_t ahora);
/* Empaquetado para la interfaz: i | activa << 3 | hora << 4 | minuto << 9 | dias << 15 | sonido << 22 |
 * siempre << 24 | borrar << 25 */
int pet_alarm_pack(int i, bool activa, int hora, int minuto, uint8_t dias, int sonido, bool siempre);
#define PET_ALARMA_BORRAR (1 << 25)
void pet_alarm_set_packed(pet_t *p, int arg);
/* hora de dormir: modo | acostarse_h << 2 | acostarse_m << 7 | despertar_h << 13 | despertar_m << 18 */
void pet_bedtime_set(pet_t *p, int modo, int ah, int am, int dh, int dm);
int pet_bedtime_pack(int modo, int ah, int am, int dh, int dm);
void pet_bedtime_set_packed(pet_t *p, int arg);
bool pet_is_bedtime(const pet_t *p, uint32_t ahora); /* entre la hora de acostarse y la de levantarse */
/* temporizador (reusa lo de las alarmas): segundos 0 lo cancela */
void pet_timer_set(pet_t *p, uint32_t segundos, uint32_t ahora);
uint32_t pet_timer_left(const pet_t *p, uint32_t ahora);
/* pasos del acelerómetro: sin metas ni números; cada PET_PASOS_PREMIO pasos trae algo del paseo */
#define PET_PASOS_PREMIO 2000u
void pet_steps_add(pet_t *p, uint32_t pasos, uint32_t ahora);
/* perfiles: si sigue viviendo cuando no es la activa (al volver se pone al día como si te hubieras ido) */
void pet_set_profile_lives(pet_t *p, bool vive);
/* el modo de juego elegido a mano (-1 = según la edad, como siempre) */
void pet_set_mode_manual(pet_t *p, int modo);
/* El ritmo del tiempo de ella. El reloj de la pantalla y las alarmas siguen en TU hora; solo su día interno (cuándo
 * duerme y despierta) y qué tan rápido le dan hambre, sueño y ganas de jugar corren más rápido. Las etapas, los
 * cumpleaños y todo lo importante se siguen midiendo en días reales: así el juego no se acaba antes. */
typedef enum { RITMO_TRANQUILO = 0, RITMO_EQUILIBRADO, RITMO_RAPIDO, RITMO_COUNT } ritmo_t;
void pet_set_rhythm(pet_t *p, int ritmo);   /* ritmo_t; marca que ya lo elegiste */
int pet_rhythm(const pet_t *p);
float pet_rhythm_factor(const pet_t *p);    /* días de ella por día real: 1 / 1,5 / 3 */
const char *pet_rhythm_name(int ritmo);
int pet_pet_hour(const pet_t *p, uint32_t ahora); /* la hora de SU reloj interno (0..23) */
/* ---------- La Bolsa de objetos mágicos (ver objetos.h) ----------
 * Los encuentra explorando, pescando o paseando contigo; muy de vez en cuando. Se usan con un gesto; los
 * que se gastan (flores, frutos, pociones, polvos...) desaparecen; los demás funcionan una vez al día. */
/* lo que te trae cuando vuelves después de un rato (una vez al día) */
typedef enum { REG_PINA = 0, REG_HOJA, REG_PALO, REG_PLUMA, REG_SAMARA, REG_PIEDRA, REG_BOTAS, REG_COUNT } regalo_t;
#define PET_REGALO_AUSENCIA (3u * 3600u) /* hay que haberse ido al menos esto */
bool pet_object_discovered(const pet_t *p, int id); /* ya sabes qué hace */
bool pet_fog(const pet_t *p, uint32_t ahora);
void pet_fog_clear(pet_t *p, uint32_t ahora);      /* la despejaste (soplando o con el dedo) */
const char *pet_gift_name(regalo_t r);
void pet_gift_now(pet_t *p, regalo_t r, uint32_t ahora);             /* (simulador y pruebas) */
void pet_fog_start(pet_t *p, uint32_t ahora, uint32_t segundos);   /* (simulador y pruebas) */
typedef enum { OFU_EXPLORAR = 0, OFU_PESCA, OFU_PASEO, OFU_REGALO, OFU_COUNT } objeto_fuente_t;
int pet_object_count(const pet_t *p, int id);
bool pet_object_known(const pet_t *p, int id); /* lo encontró alguna vez */
int pet_objects_known(const pet_t *p);
bool pet_object_gives(pet_t *p, int id, objeto_fuente_t f, uint32_t ahora); /* se lo da (false si ya tiene muchos) */
int pet_object_find(pet_t *p, objeto_fuente_t f, uint32_t ahora);           /* busca uno al azar: id o -1 */
/* 1 lo usó; 0 ahora no quiere (o está durmiendo); -1 no lo tiene; -2 ya lo usó hoy */
int pet_object_use(pet_t *p, int id, uint32_t ahora);
bool pet_object_ready(const pet_t *p, int id, uint32_t ahora); /* se puede usar ahora (no lo usó hoy) */
bool pet_object_spent(int id);                                /* se gasta al usarlo */
/* ---------- Linaje y habilidades ----------
 * Mientras es bebé y cría, lo que vive la va moldeando: los objetos que usa, las zonas donde pasa el tiempo y
 * el clima. Al crecer, si algo la marcó mucho más que su propia especie, toma la forma adulta de otra (con sus
 * propios colores: pet_look la lleva en mezcla). */
int pet_lineage_lead(const pet_t *p);   /* hacia dónde va (su especie si nada la marca) */
int pet_lineage_shape(const pet_t *p);  /* la forma de su cuerpo de adulta */
/* Crecer con permiso: cuando le toca crecer te lo avisa, ves cómo será y decides (una vez hecho no hay vuelta atrás).
 * pet_evo_pending: 0 o la etapa a la que ya puede crecer; pet_evo_preview: cómo se vería (etapa, forma y mezcla);
 * pet_evo_accept: crece ahora; pet_evo_defer: «todavía no» (se puede hacer después desde Ajustes). */
int pet_evo_pending(const pet_t *p);
void pet_evo_preview(const pet_t *p, uint32_t ahora, int *etapa, int *forma, int *mezcla);
bool pet_evo_accept(pet_t *p, uint32_t ahora);
void pet_evo_defer(pet_t *p);

/* dos habilidades por especie: la primera de cría, la segunda de adulta; una vez al día cada una.
 * 1 la usó; 0 ahora no puede (o todavía no la tiene); -2 ya la usó hoy */
#define PET_HABILIDADES 2
int pet_ability_use(pet_t *p, int cual, uint32_t ahora);
bool pet_ability_has(const pet_t *p, int cual);
bool pet_ability_ready(const pet_t *p, int cual, uint32_t ahora);
const char *pet_ability_name(int especie, int cual);
int pet_ability_block(const pet_t *p, int cual, uint32_t ahora); /* 0 = se puede; si no, el motivo */
const char *pet_ability_block_text(int code);
/* el color se elige al nacer (mientras es bebé) */
bool pet_set_color(pet_t *p, int color);
/* lo lleva de ofrenda al árbol hueco: la primera del día trae al otro día una bendición (sol y luciérnagas) */
bool pet_object_return(pet_t *p, int id, uint32_t ahora);
bool pet_object_unplace(pet_t *p, int slot);                  /* lo saca del prado y vuelve a la Bolsa */
void pet_object_head_off(pet_t *p);
void pet_bag_seen(pet_t *p);                                  /* abriste la Bolsa: se apaga el aviso */
/* aviso suave de respaldo: modo adulto, más de 4 semanas sin guardarla en el computador */
void pet_mark_backup(pet_t *p, uint32_t ahora);
bool pet_backup_due(const pet_t *p, uint32_t ahora);
#define PET_SAVE_VERSION_MIN 13 /* la más vieja que se puede actualizar */

/* ---------- Nombres (para depuración y el simulador) ---------- */
const char *pet_need_name(need_t n);
const char *pet_mood_name(mood_t m);
const char *pet_emotion_name(emo_t e);
const char *pet_cause_name(causa_t c);
const char *pet_want_name(want_t w);
const char *pet_action_name(action_t a);
const char *pet_activity_name(actividad_t a);
const char *pet_place_name(lugar_t l);
const char *pet_find_name(hallazgo_t h);
const char *pet_trait_name(trait_t t);
const char *pet_gesture_name(gesto_t g);
const char *pet_neighbor_activity_name(vecino_act_t a);
const char *pet_catch_name(pesca_t c);
const char *pet_trick_name(truco_t t);
const char *pet_skill_name(habilidad_t h);
const char *pet_collection_name(coleccion_t c);
const char *pet_rare_name(raro_t r);
const char *pet_goal_name(meta_t m);
const char *pet_date_name(fecha_t f);

/* ---------- Celebraciones ---------- */
void pet_celebrations_default(void);
int pet_celebration_count(void);
const celebracion_t *pet_celebration_get(int i);
/* Reemplaza la tabla (valida todo: si algo no sirve, no cambia nada y devuelve false) */
bool pet_celebrations_set(const celebracion_t *c, int n);
int pet_celebration_today(const pet_t *p, uint32_t ahora); /* índice o -1 */
/* Formato binario para guardarlas y para el editor: "FIES", versión, cantidad, crc32, tabla */
int pet_celebrations_pack(uint8_t *out, int cap);
bool pet_celebrations_unpack(const uint8_t *in, int n);
const char *pet_rule_name(cel_regla_t r);

/* ---------- Respaldo ----------
 * La mascota entera (personaje, recuerdos, diario, álbum, colecciones, ajustes) en texto:
 * la partida sellada en base64. Sirve para guardarla en el computador y pasarla a otro reloj
 * (o al simulador) con la misma versión de partida. */
#define PET_RESPALDO_MAX ((int)(4 * ((sizeof(pet_t) + 2) / 3) + 64))
#define PET_RESPALDO_CABECERA "MASCOTA-RESPALDO"
int pet_backup_export(pet_t *p, char *out, int cap);     /* largo escrito o -1 */
bool pet_backup_import(const char *txt, pet_t *out);     /* acepta la cabecera, espacios y saltos */
int pet_base64_encode(const uint8_t *in, int n, char *out, int cap);
int pet_base64_decode(const char *in, uint8_t *out, int cap); /* bytes o -1 */
const char *pet_bond_name(vinculo_nivel_t v);
const char *pet_diary_text(const pet_t *p, const entrada_diario_t *e, char *buf, int n); /* frase del diario */
const char *pet_game_name(juego_t j);
const char *pet_food_name(food_t f);
const char *pet_stage_name(stage_t s);
const char *pet_sound_name(sound_t s);
const char *pet_encounter_name(encounter_t e);
const char *pet_item_name(item_t i);
const char *pet_decor_name(decor_t d);
const char *pet_form_name(forma_t f);
const char *pet_season_name(estacion_t e);
int pet_season_like(const pet_t *p, int cual); /* cuánto le gusta una estación (-1..1 aprox.): sale de su personalidad y de lo que aprendió */
int pet_season_fav(const pet_t *p, bool peor);   /* su estación favorita (o la que menos le gusta) o -1 si todavía no vivió ninguna / no destaca */
const char *pet_weather_name(clima_t c);
const char *pet_scene_name(escena_t e);
const char *pet_scene_el(escena_t e);     /* (v40) «el prado», «la laguna» */
const char *pet_scene_al(escena_t e);     /* (v40) «al prado», «a la laguna» */
const char *pet_scene_proper(escena_t e); /* (v40) «Laguna de los Cisnes», «Cerro del Viento»... */
const char *pet_fauna_name(fauna_t f);
const char *pet_friendship_name(amistad_t a);
const char *pet_species_name(uint8_t especie);
uint8_t pet_species_count(void);
/* Nombre propio para una especie (desde el editor); NULL o "" vuelve al de fábrica */
void pet_species_set_name(uint8_t especie, const char *nombre);
const char *pet_species_factory_name(uint8_t especie);

#ifdef __cplusplus
}
#endif
#endif /* PET_H */
