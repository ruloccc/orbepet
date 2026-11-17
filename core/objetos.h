/*
 * Los objetos mágicos del bosque: 150, en cinco categorías. Los encuentra paseando, pescando o
 * explorando, y se guardan en la Bolsa. Cada uno tiene un efecto entre unos pocos que se reusan (comer,
 * ponérselo, decorar, el cielo, un acompañante...), un gesto para usarlo y una leyenda corta.
 * Sin tienda ni monedas: todo depende de lo que pasa.
 */
#ifndef OBJETOS_H
#define OBJETOS_H

#include <stdint.h>

#define PET_OBJETOS 150
#define PET_OBJ_MAX 9      /* de cada uno, como mucho */
#define PET_OBJ_PUESTOS 4  /* objetos decorando el prado */

typedef enum { OCAT_TIERRA = 0, OCAT_FLORA, OCAT_ESPIRITUS, OCAT_ANTIGUO, OCAT_ARTEFACTO, OCAT_COUNT } objeto_cat_t;

/* qué hace al usarlo */
typedef enum {
    OEF_MIRAR = 0, /* de colección: brilla y cuenta su leyenda */
    OEF_COMER,     /* se lo come (param: 0 normal, 1 copos, 2 humo, 3 arcoíris, 4 chispas, 5 corazones) */
    OEF_CALMA,     /* le quita la pena, el miedo o el enojo */
    OEF_ENERGIA,   /* despierta y con ganas de jugar */
    OEF_SUENO,     /* le da sueñito: se acuesta */
    OEF_CARINO,    /* busca tus caricias */
    OEF_CURAR,     /* la sana (como la hierba) */
    OEF_CABEZA,    /* se lo pone en la cabeza un rato */
    OEF_DECORAR,   /* lo deja en el prado (hasta 4) */
    OEF_JARDIN,    /* el jardín (param: 0 riega todo, 1 crece, 2 revive lo seco) */
    OEF_CIELO,     /* un rato de magia en el cielo (param: 0 aurora, 1 eclipse, 2 chispas, 3 estrellas fugaces,
                      4 fuegos, 5 purpurina, 6 luciérnagas, 7 constelaciones) */
    OEF_COMPA,     /* un acompañante que sigue tu dedo (param: 0 fuego fatuo, 1 luciérnaga, 2 espora, 3 sombra) */
    OEF_SUERTE,    /* un día con más hallazgos */
    OEF_PESCA,     /* un día de buena pesca */
    OEF_PROTEGE,   /* un día sin zorro ni abejas molestas */
    OEF_CLIMA,     /* cambia el clima (param: clima_t) */
    OEF_ESPECIAL,  /* hace algo propio (param: OBJX_*) */
    OEF_COUNT
} objeto_efecto_t;

/* los objetos con efecto propio (param de OEF_ESPECIAL) */
typedef enum {
    OBJX_NINGUNO = 0,
    OBJX_ESPEJO,     /* hoy todo lo que encuentra viene doble */
    OBJX_RUNA_TP,    /* la lleva a otra zona al instante */
    OBJX_INVISIBLE,  /* un rato casi transparente (y nadie la molesta) */
    OBJX_ENCOGE,     /* un rato chiquitita y más curiosa */
    OBJX_CRECE,      /* un rato grande y fuerte */
    OBJX_COLORES,    /* le cambia los colores un rato */
    OBJX_CANDADO,    /* cuida lo más valioso de la bolsa (no se gasta ni se ofrenda por error) */
    OBJX_TINTERO,    /* revela una nota escondida en su diario */
    OBJX_LENTES,     /* hoy aparecen más cosas raras */
    OBJX_IMAN,       /* hoy atrae objetos perdidos y antigüedades */
    OBJX_BRUJULA,    /* la aguja apunta hacia ella */
    OBJX_LLAVE,      /* abre el cofre bajo el árbol viejo (con el candado) */
    OBJX_CUERDA,     /* camina como robot de juguete */
    OBJX_PIPA        /* burbujas cuadradas que revientan al tocarlas */
} objeto_x_t;

/* el gesto para usarlo (en la Bolsa) */
/* soplar: al micrófono del reloj (o, si no hay, deslizando el dedo) */
typedef enum { OGE_TOCAR = 0, OGE_FROTAR, OGE_MANTENER, OGE_DESLIZAR, OGE_SOPLAR, OGE_COUNT } objeto_gesto_t;

/* familias de dibujo (el ícono se dibuja con la familia y dos colores) */
typedef enum {
    OFA_PIEDRA = 0, OFA_GEMA, OFA_CRISTAL, OFA_POLVO, OFA_FLOR, OFA_HONGO, OFA_SEMILLA, OFA_BAYA, OFA_HOJA, OFA_PLANTA,
    OFA_PLUMA, OFA_ESCAMA, OFA_HUEVO, OFA_HUESO, OFA_CONCHA, OFA_FRASCO, OFA_POCION, OFA_DISCO, OFA_LLAVE, OFA_LIBRO,
    OFA_PALO, OFA_RUNA, OFA_AMULETO, OFA_CAJA, OFA_BOTA, OFA_TAZA, OFA_LAMPARA, OFA_ORBE, OFA_HILO, OFA_COUNT
} objeto_familia_t;

/* dónde aparece: bits de escena_t (0 = en cualquier parte) */
#define OL_PRADO 1u
#define OL_BOSQUE 2u
#define OL_LAGO 4u
#define OL_COLINA 8u
#define OL_CUEVA 16u

typedef struct {
    const char *nombre;
    uint8_t cat, familia, efecto, param, gesto;
    uint8_t rareza;   /* 1 común, 2 raro, 3 muy raro, 0 no aparece al azar */
    uint8_t lugares;  /* OL_* (0 = cualquiera) */
    uint8_t noche;    /* 1: solo se encuentra de noche */
    uint8_t se_vuelve; /* 1 + objeto en que se convierte (frotándolo, o de noche si noche_cambia) */
    uint8_t noche_cambia; /* se transforma solo de noche, en la bolsa */
    uint32_t c1, c2;  /* colores del ícono */
    const char *leyenda;
} objeto_info_t;

const objeto_info_t *objeto_info(int id);   /* NULL si no existe */
const char *objeto_cat_nombre(int cat);
#define OBJ_CORAZON_BOSQUE 149                /* el último: no aparece al azar */
#define OBJ_BOTELLA 114                       /* la botella con mensaje (en la isla aparece más) */
#define OBJ_CANDADO_RUNICO 111                /* el candado: junto a la llave oxidada abre el cofre del árbol viejo */
#define OBJ_GAFAS_EXPLORADOR 99               /* con ellas puestas ve los nidos escondidos */
#define OBJ_LLAVE_OXIDADA 92                  /* también puede salir pescando */
#define OBJ_GALLETA_TILO 38                   /* (versión 37) la galleta quemada de Tilo: se encuentra en la casita escondida */
#define OBJ_CORONA_HOJAS 102                  /* (versión 37) la corona de hojas: en el mirador de las nubes */
const char *objeto_al_usar(int id);           /* lo que dice ella al usarlo (dice lo que hace) */
const char *objeto_leyenda_acto(int id, int acto); /* la leyenda según el acto (los objetos de la historia dan pistas) */
#define OBJ_SEDA_ARANA 67                     /* la seda de araña tejedora: con ella le tejen su bufanda */

#endif
