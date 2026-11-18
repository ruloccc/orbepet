/* Interfaz del reloj: mundo en pixel art con la mascota, para la pantalla redonda 466x466. */
#pragma once

#include "lvgl.h"
#include "pet.h"

/* Acciones que la interfaz entrega al bucle principal. Además de las action_t
 * del motor existen estas: */
#define UI_ACCION_DECORAR 100    /* arg = decor_t a poner en el prado */
#define UI_ACCION_NOMBRE 101     /* el nombre elegido está en ui_pending_name() */
#define UI_ACCION_INVITAR 102    /* arg = índice del amigo */
#define UI_ACCION_RESPONDER 103  /* arg = 1 acepta, 0 rechaza la invitación */
#define UI_ACCION_NUEVA 104      /* mascota nueva (vuelve a preguntar la fecha de nacimiento) */
#define UI_ACCION_EDAD 105       /* arg = edad del dueño (versión anterior; ya no se usa) */
#define UI_ACCION_HEMISFERIO 106 /* arg = 1 sur, 0 norte */
#define UI_ACCION_NACIMIENTO 107 /* fecha de nacimiento del dueño: arg = año * 10000 + mes * 100 + día */
#define UI_ACCION_SONAR 108      /* arg = sound_t que la interfaz quiere oír (bienvenida, cumpleaños) */
#define UI_ACCION_HORA 109       /* la fecha y hora de hoy: arg = segundos Unix en hora LOCAL (zona de la mascota) */
#define UI_ACCION_RESTAURAR 110  /* respuesta a ui_ask_restore: arg = 1 traerla, 0 no */
#define UI_ACCION_ASPECTO 111    /* especie, mezcla y color elegidos: arg = especie | (mezcla + 1) << 4 | color << 8 (mezcla -1 = sin) */
#define UI_ACCION_AMBIENTE 112   /* volumen del sonido de fondo (ajuste del reloj): 0..100 (| UI_SOLTADO al soltar: se guarda) */
#define UI_ACCION_DECOR_VER 113  /* guardar o volver a poner una decoración: arg = decor_t */

typedef void (*ui_action_cb_t)(int accion, int arg);

void ui_init(lv_display_t *disp, ui_action_cb_t on_action);

/* Refresca el mundo y la mascota. ahora_utc en segundos Unix (UTC).
 * Llamar con el lock de LVGL tomado. */
void ui_update(const pet_t *p, uint32_t ahora_utc);

/* Convierte un evento del motor en animación, globo o ícono. Con el lock tomado. */
void ui_show_event(const pet_t *p, const pet_event_t *e);

/* Nombre escrito en el teclado de bienvenida */
const char *ui_pending_name(void);
const uint8_t *ui_pending_song(int *n, const char **nombre); /* (v38) la canción por guardar (UI_ACCION_CANCION) */

/* Inclinación del reloj (-1..1 por eje): la mascota se desliza, la pelota rueda, la canasta se mueve */
void ui_set_tilt(float x, float y);

/* El reloj perdió la hora (sin batería): la interfaz la pregunta antes de seguir */
void ui_set_clock_doubtful(bool dudosa);

/* Llegó un respaldo (por USB o el simulador): pregunta en pantalla si traer esa mascota.
 * La respuesta llega como UI_ACCION_RESTAURAR. */
void ui_ask_restore(const pet_t *nueva);

/* Nivel de batería (-1 = sin batería / simulador) */
void ui_set_battery(int porcentaje, bool cargando);
bool ui_battery_aviso_activo(void);
void ui_demo_cofre(int v, int s);
void ui_demo_rapido(void);
void ui_battery_aviso(int porcentaje, int modo, uint32_t ms); /* modo 0: conectaron el cargador; 1: carga completa */

/* Fotos: la interfaz saca la foto (la pantalla en QOI, ver foto.h) y la guarda en el reloj. */
typedef struct {
    int (*guardar)(const uint8_t *qoi, int n);    /* id (>= 0) o -1 si no hay lugar */
    int (*listar)(uint16_t *ids, int max);        /* las más nuevas primero; devuelve cuántas */
    int (*leer)(int id, uint8_t *buf, int cap);   /* bytes leídos o -1 */
    int (*tamano)(int id);                        /* bytes o -1 */
    bool (*borrar)(int id);
} ui_fotos_t;
void ui_set_photo_store(const ui_fotos_t *f);
/* Mis mascotas: las que se quedan descansando en el reloj (hasta PET_GUARDADAS). */
typedef struct {
    bool (*leer)(int i, pet_t *out); /* false si ese lugar está libre */
} ui_mascotas_t;
void ui_set_pet_store(const ui_mascotas_t *m);
#define UI_ACCION_CAMBIAR 114 /* traer la mascota guardada en el lugar arg (la actual se queda descansando) */
#define UI_ACCION_SOLTAR 115  /* dejar ir a la mascota guardada en el lugar arg */

/* cierra la tarjeta abierta (para las pruebas automáticas del simulador) */
void ui_close_panel(void);
void ui_pecera(void); /* entra al modo pecera (el botón PWR dos veces) */
void ui_pecera_vencida(void); /* (v101) se acabó el tiempo de la pecera: sale de ella (la pantalla se apaga sola) */
/* muestra el viaje de despedida de m (tipo: art_viaje_t, -1 al azar), para las pruebas */
void ui_demo_trip(const pet_t *m, int tipo);

/* Modo pecera: sin botones, solo ella viviendo. Se entra desde Ajustes o solo, al cargar
 * la batería sin tocarlo un minuto. main deja la pantalla encendida (más tenue). */
bool ui_aquarium(void);
bool ui_silencio_ambiente(void);
int ui_escena_especial(void);
/* estilo del mundo: 0 el de siempre, 1 el nuevo */
void ui_set_style(int estilo);
/* un globo con texto sobre ella (pruebas) */
void ui_say(const char *texto);
void ui_splash(const char *l1, const char *l2, bool fallo, uint32_t ms); /* pantalla de bienvenida (fallo = rojo) */

/* Alarmas (ver pet_alarm_*): */
#define UI_ACCION_ALARMA 116      /* cambiar una alarma: arg = pet_alarm_pack(...) o i | PET_ALARMA_BORRAR */
#define UI_ACCION_ALARMA_RESP 117 /* respuesta a la alarma que suena: arg = alarma | 8 si pospone */
#define UI_ACCION_VOLUMEN 118     /* arg = 0..100 (| UI_SOLTADO al soltar la barra: se guarda y suena un pío) */
#define UI_ACCION_BRILLO 119      /* arg = PET_BRILLO_MIN..100 (en vivo; | UI_SOLTADO al soltar: se guarda) */
#define UI_ACCION_APAGADO 120     /* cuándo se apaga la pantalla: 0 = 10 s ... 3 = 1 min */
#define UI_ACCION_DORMIR 121      /* hora de dormir: arg = pet_bedtime_pack(...) */
#define UI_ACCION_TEMPORIZADOR 122 /* arg = segundos (0 lo cancela) */
#define UI_ACCION_PERFIL_VIVE 123  /* arg = lugar (15 = el perfil activo) | vive << 4 */
#define UI_ACCION_MODO 124         /* modo de juego a mano: -1 según la edad, 0 adulto, 1 niño, 2 infante */
#define UI_ACCION_OBJETO 125       /* la Bolsa: arg = objeto | op << 8 (0 usar, 1 devolverlo al bosque, 2 guardar el que
                                       decora el prado [objeto = lugar], 3 sacárselo de la cabeza, 4 ya viste la Bolsa,
                                       5 abrir el cofre del árbol viejo, 6 mirar el nido [objeto = nido 0..2]) */
#define UI_ACCION_NIEBLA 126       /* la despejaste (soplando o barriendo con el dedo) */
#define UI_ACCION_RESPUESTA 127    /* respondiste el mensaje del día (pet_phrase_hug) */
#define UI_ACCION_COLOR 128        /* el color, elegido al nacer: arg = 0..PET_COLORES-1 */
#define UI_ACCION_HABILIDAD 129    /* usar una de sus habilidades: arg = 0 (de cría) o 1 (de adulta) */
#define UI_ACCION_BALLENA 130      /* terminó el paseo por el firmamento: arg = estrellas que tocaste */
#define UI_ACCION_BAILE 131        /* terminó el baile con la caja de música: arg = veces que seguiste el ritmo */
#define UI_ACCION_FAMILIA 132      /* cuidar el huevo de familia: pasa a ser la mascota activa (ella queda en perfiles) */
#define UI_ACCION_VIAJE_NO 133     /* despedida natural: "todavía no" */
#define UI_ACCION_DESPEDIDA_NAT 134 /* activar (1) o no (0) que algún día pida irse de viaje */
#define UI_ACCION_VISITA_PERFIL 135 /* invitar a la mascota de otro perfil: arg = lugar */
#define UI_ACCION_FOTO 136         /* la cámara guardó una foto: arg = id; con 0x10000, arg & 0xFFFF = hasta dónde llegan las de antes */
#define UI_ACCION_FOTO_BORRADA 137 /* se borró una foto: arg = id */
#define UI_ACCION_GIRAR 138        /* la pantalla se endereza sola: arg = 1 sí, 0 no (ajuste del reloj) */
#define UI_ACCION_CARTA 139        /* abrió la carta que le llegó (rompió el sello) */
#define UI_ACCION_GENERO 140       /* niña (0) o niño (1), elegido al empezar */
/* la pantalla está encendida (los animales solo cuentan como vistos juntos con la pantalla encendida) */
void ui_screen_on(bool on);
void ui_boot_done(void);
bool ui_captura_sacudida(void);   /* v89: la pesca espera una sacudida (no va al motor) */
void ui_sacudida(void);
void ui_set_mic_ok(bool ok);       /* v90: el micrófono se pudo abrir (si no, la pesca no pide soplar) */
void ui_mic_nivel(int nivel);      /* v90: lo fuerte que suena ahora (para que se vea el soplido) */
void ui_demo_pesca(int accion);    /* (pruebas) 0 sacudir, 1 soplar, 2 tocar */          /* el arranque llegó al bucle (v88) */
extern volatile int g_ui_paso;    /* por dónde va ui_init (diagnóstico de arranque, v88) */
#define UI_ACCION_ANIMAL_VISTO 141 /* un animal estuvo unos segundos a la vista contigo: arg = ficha (pet_animal_seen) */
#define UI_ACCION_CRIATURA 142     /* tocaste a la criatura mágica (pet_creature_touch) */
#define UI_ACCION_CITA_FIN 143     /* terminó la cita con la criatura: arg = cita_t | puntos << 8 */
#define UI_ACCION_ESPECIAL 144     /* el secreto de la zona: arg = especial_t | resultado << 4 */
#define UI_ACCION_CRISTAL 145      /* tocaste un cristal de la cueva: arg = cuál (0 azul ... 5 blanco) */
#define UI_ACCION_PAIS 146         /* el país (pais_t): decide el hemisferio y las fiestas */
#define UI_ACCION_CRISTALES 147    /* terminó el juego de los cristales: arg = rondas que ganó (3: despertó al dragón) */
void ui_libro_pedir(int n); /* (pruebas) pide una página del Libro */
int ui_test_count(void);
int ui_panel_actual(void);
void ui_demo_juego(int j);
int ui_juego_actual(void); /* (pruebas) 0 si no hay juego en marcha */
bool ui_reposo_visual(void); /* (v99) nada que leer ni jugar: se puede atenuar y bajar los cuadros */
void ui_demo_historia(int c, int k);
void ui_demo_huevo(int f); /* (pruebas) 0 pregunta de viaje al poner el huevo, 1 tocar el huevo en el nido */
void ui_demo_pochi(int f); /* (pruebas) 0 llega, 1 invita, 2 paseo, 3 se despide, 4 corre, 5 barro; 10-17 cachorros, 20-22 trucos, 30-33 manías, 40-43 llegadas, 50-54 familia, 60-70 animales, 80-89 cachorro y animal, 100-103 amigos */
void ui_cine(int n); /* (pruebas) la escena de un secreto */
#define UI_ACCION_LIBRO 159        /* ya leyó una página del Libro: arg = 0..PET_LIBRO_PAGINAS-1 (pet_libro_mark) */
#define UI_ACCION_ARRULLO_DUR 158  /* cuánto dura el arrullo: arg = 0 (30 min), 1 (1 hora), 2 (2 horas) */
#define UI_ACCION_ARRULLO_VOL 162 /* volumen de los grillos del arrullo: 0..100 (| UI_SOLTADO al soltar) */
#define UI_ACCION_ARRULLO 157      /* modo arrullo: arg = 0 apagado, 3 grillos (ajuste del reloj) */
#define UI_ACCION_MUSICA 156       /* escuchar música con el micrófono: arg = 1 sí, 0 no (ajuste del reloj) */
#define UI_ACCION_FINAL 155       /* vio el final: arg = PET_FINAL_REGULAR o PET_FINAL_SECRETO (pet_final_done) */
#define UI_ACCION_FUENTE 161     /* tamaño de las letras de lectura: arg = 0 normal, 1 grande */
#define UI_ACCION_ECLOSIONAR 160 /* el huevo se abre ya (primer inicio, tras elegirlo y poner la hora) */
#define UI_ACCION_RITMO 154       /* el ritmo de su tiempo: arg = ritmo_t (pet_set_rhythm) */
#define UI_ACCION_ALARMAS 153     /* volumen de las alarmas: 45..100 (| UI_SOLTADO al soltar) */
#define UI_ACCION_EFECTOS 152     /* volumen de los efectos (fuegos, juegos, truenos): 0..100 (| UI_SOLTADO al soltar) */
#define UI_ACCION_FECHA_OTRA 151  /* ver otra fecha (solo este perfil; el reloj no cambia): arg = segundos Unix en hora LOCAL */
#define UI_ACCION_TILDE 166        /* (v37) tocaste a Tilde: arg 0 escuchaste el chisme, 1 leíste la nota (pet_tilde_touch) */
#define UI_ACCION_BRIZNA 167       /* (v37) la Abuela Brizna contó su recuerdo (pet_brizna_heard) */
#define UI_ACCION_PRISMA 168       /* (v37) tocaste al dragón de cristal (pet_prisma_touch) */
#define UI_ACCION_TILO 169         /* (v37) ya vio aparecer la página k del diario de Tilo (pet_tilo_page_seen) */
#define UI_ACCION_LUGAR 170        /* (v37) tocó algo en un lugar secreto: arg = lugar * 16 + cosa (pet_secret_find) */
#define UI_ACCION_PIEZA 171        /* (v38) terminó una pieza en el modo libre de los cristales: arg = notas | PIEZA_* << 8 (pet_crystal_piece) */
#define UI_ACCION_CANCION 172      /* (v38) guardar la canción que está en ui_pending_song(): arg = dónde (-1 la primera libre) (pet_song_save) */
#define UI_ACCION_CANCION_USO 173  /* (v38) arg = canción (15: la de siempre) | para << 4 (0 alarmas, 1 cuna) | 0x100 borrarla */
#define UI_ACCION_CALLAR 174       /* (v39) cerraron su globo: que se calle (corta el sonido que esté diciendo) */
#define UI_ACCION_ANIMAL_TOCAR 175 /* (v39) tocaste a un animal que ya conoce: arg = ficha (pet_animal_tell) */
#define UI_ACCION_POCHI_CHARLA 176 /* (v39) charlaron con Pochi: arg = cuál (pet_pochi_chat_done) */
#define UI_ACCION_ZONA_PISTA 177   /* (v39) ya le recordó cómo volver: arg 0 el claro, 1 el mirador (pet_zone_hint_done) */
#define UI_ACCION_EN_ZONA 179      /* (v39) arg 1: entraron al claro/casita/mirador; 0: salieron (pet_in_zone) */
#define UI_ACCION_ZONA_HALLAZGO 180 /* (v40) sacó lo escondido de hoy: arg 0 el claro, 1 la casita, 2 el mirador (pet_zone_find_take) */
#define UI_ACCION_JUEGO_LUGAR 182  /* (v98) terminó un juego de los lugares: arg = juego (PET_JL_*) | estrellas << 3 | puntos << 5 (pet_place_game_result) */
#define UI_ACCION_PECERA_DUR 183   /* (v99) cuánto dura la pecera con batería: 0 = 10 min, 1 = 30 min, 2 = 1 h, 3 = no se apaga */
#define UI_ACCION_EXCURSION 181    /* (v40) volvió de su excursión sola: arg 0 el claro, 1 el mirador (pet_excursion_back) */
#define UI_ACCION_TOCAR_ANIMAL 184 /* (fw 103) tocaste a un animal del mundo: arg = animal | veces << 8 (pet_animal_touch); devuelve pet_toque_t en ui_toque_res() */
#define UI_ACCION_INFORME 191      /* (1.0.20) Ajustes → Versión → Enviar informe: guarda y reinicia en el modo Wi-Fi (ota.c) */
#define UI_ACCION_AIREAR 190       /* (1.0.18) le soplaste o espantaste el olor (pet_air_out) */
#define UI_ACCION_JUEGO_ISLA 189   /* (1.0.18) terminó un juego de la isla: arg = PET_ISLA_* | puntos << 4 (pet_island_game) */
#define UI_ACCION_JUEGO_DOS 188    /* (1.0.17) terminó un juego de a dos con una visita: arg = PET_DOS_* | resultado << 4 (pet_duo_game) */
#define UI_ACCION_HUEVO_EMPIEZA 187 /* (1.0.10) el primer huevo llegó al prado: empieza su cuenta de 5 minutos (pet_egg_start) */
#define UI_ACCION_ACTUALIZAR 186   /* (1.0.09) Ajustes → Versión → Actualizar: guarda y reinicia en el modo actualizar por Wi-Fi (ota.c) */
#define UI_ACCION_TOCAR_FAUNA 185  /* (fw 103) igual, con los que pasan volando: arg = fauna | veces << 8 (pet_fauna_touch) */
#define UI_ACCION_LIBRO_VISTO 178  /* (v39) miró las historias de un narrador en el Libro: arg = narrador (pet_book_seen) */
void ui_demo_toque(int res); /* (pruebas) muestra la reacción a un toque (pet_toque_t) */
void ui_toque_resultado(int r); /* (fw 103) el motor contesta qué pasó al tocar al animal (pet_toque_t) */
void ui_demo_tilde(void);
void ui_demo_brizna(int k);
void ui_demo_pagina(int k);
void ui_demo_lugar(int l);
void ui_demo_ceremonia(int cual, int pag); /* (v39) 1 el rito, 2 el último verso; pag = desde qué página */
void ui_demo_zona(int z);                  /* (v39) 1 el claro, 2 la casita, 3 el mirador */
void ui_demo_zona_juego(int jg);           /* (v98) abre un juego en su pantalla: 1 galletas, 2 canción de cuna, 3 salto al cielo, 4 atrapa hojas, 5 piedra tibia */
int ui_zona(void);                         /* (v98) 0 ninguna, 1 en el claro del rey, 2 adentro de la casa del rey, 3 en el mirador */
void ui_demo_nube(void);
int ui_escena_vista(void);                 /* (v98) la zona que se ve en la pantalla (puede ir atrasada del motor durante un viaje) */
void ui_demo_zona_salir(void);             /* (v97) cierra el juego del lugar y sale de la zona secreta */
int ui_zj_estado(void);                    /* (v98) juego*100 + 50 si terminó + puntos (hasta 49) (0 sin juego) */
void ui_zj_trampa(int si);                 /* (v97, pruebas) cada toque acierta */
void ui_zj_tocar(int x, int y);            /* (v97, pruebas) un toque en el juego del lugar, sin el táctil */
void ui_demo_excursion(int z);             /* (v96) 0 al claro, 1 al mirador, -1 que vuelva */
int ui_excursion(void);
#define UI_ACCION_PESCA 165        /* v89: la pesca con minijuego: arg 0 empezó, 1 no picó, 2 se escapó, 3 lo sacó (pet_fish_game) */
#define UI_ACCION_LIBRO_AVISO 164  /* ya le enseñaron a abrir el libro de historias (pet_book_tutorial_done) */
#define UI_ACCION_SALTO_CAMA 163   /* saltó en la cama de su casita (pet_jump_bed) */
#define UI_ACCION_POCHI 150        /* Pochi, el chihuahua visitante: arg = 0 llegó, 1 comió, 2 un lugar del paseo, 3 se despidió (pet_pochi) */
#define UI_ACCION_EVOLUCION 149    /* le preguntaste si crece: arg = 1 crecer ahora, 0 todavía no (pet_evo_accept / pet_evo_defer) */
#define UI_ACCION_FIESTA 148       /* celebraron con todos: arg = la fiesta (pet_party_now) */
/* (UI_ACCION_SOLTAR y UI_ACCION_NUEVA llevan además el vehículo del viaje: arg | (vehículo + 1) << 8) */
#define UI_SOLTADO 0x1000
/* los ajustes del dispositivo (volumen, brillo, apagado): main los tiene y la interfaz los muestra */
void ui_set_device(const pet_device_t *d);
/* el micrófono: main lo prende solo cuando la interfaz espera un soplido, y le avisa cuando lo detecta */
bool ui_wants_mic(void);
void ui_mic_blow(void);
void ui_set_llave_informe(bool si);
void ui_inv_fusionar(lv_event_t *e); /* (1.0.22) junta áreas por redibujar antes de que LVGL se rinda y redibuje todo */
void ui_inv_fusion(bool si);
/* (1.0.23) pruebas por cable de lo nuevo */
bool ui_test_isla_toque(int x, int y); /* lo mismo que tocar (x, y) en la isla: true si abrió algo o contestó */
void ui_zd_abrir(int n);              /* 0 ping pong, 1 cuerda, 2 memorice, 3 escondidas, 4 faro, 5 buceo */
int ui_zd_estado(void);               /* -1 cerrado; si no, fase*1000 + puntos*10 + la visita */           /* (pruebas) apagarlo para comparar */ /* (1.0.20) ya se cargó la llave de los informes */
/* una alarma está sonando en pantalla: main mantiene la pantalla encendida */
bool ui_alarm_ringing(void);
/* suena la alarma "siempre" de otra mascota (no la activa) */
void ui_alarm_other(const pet_t *otra, int i);
/* (simulador) el paseo por el firmamento, para verlo */
void ui_demo_sky(const pet_t *m);
/* la pantalla quedó al revés (1) o derecha (0): para la interfaz es lo mismo, pero lo sabe */
void ui_set_upside_down(bool al_reves);
bool ui_upside_down(void);
/* el ajuste de girar sola (para mostrarlo en Pantalla) */
void ui_set_auto_rotate(bool si);

/* Casi sin batería: muestra que se va a dormir para cuidar la hora (main después la apaga) */
void ui_show_reserve(void);
/* pruebas: abre el panel k (0 si no hay) y revisa que nada quede fuera del círculo ni tape la X o la flecha */
int ui_test_panel(int k);
struct _lv_obj_t *ui_fondo_solo(int k); /* (pruebas) deja solo el fondo de una pantalla */
int ui_panel_actual(void); /* número del panel abierto (para el registro de velocidad) */
void ui_test_pos(int x, int y); /* (pruebas) */
int ui_test_fondo(int *esc, int *per, int *w, int *h); /* (pruebas) 1 si el fondo está cargado */
void ui_test_bote(int q);
void ui_test_alarma(int modo); /* 0 alarma, 1 dormir, 3 tiempo */
void ui_test_alarma_cerrar(void);
void ui_test_soltar_memoria(int modo); /* (pruebas) suelta la caché de imágenes (1) y las fotos de pantallas (2) */
void ui_test_descarga(const char *l0, const char *l1, const char *l2, const char *l3, bool animar); /* (pruebas) pantalla fija de la prueba de descarga; l0 NULL la quita */
void ui_test_punto(int x, int y); /* (pruebas) punto de mira en la pantalla; x < 0 lo quita */
int ui_test_cruce_fondo(void); /* -1 no hay cruce, 0 sin fondo, 1 con fondo */
void ui_demo_picnic(void); /* (pruebas) abre la pantalla del picnic: el camino real de «comer» */
void ui_touch_reset(void);
int ui_touch_get(int *x, int *y, int max); /* cuántos toques hubo desde el reset (y los primeros 16) */
void ui_test_casita(int modo); /* (pruebas) 0 sale de la casita, 1 entra a visitarla, 2 salta en la cama */
const char *ui_test_panel_name(int k);
void ui_test_huevo_elegido(int especie);
void ui_set_red_ota(const char *red); /* (1.0.11) la red propia del modo actualizar, para Ajustes → Versión */ /* (1.0.10, pruebas) el primer huevo ya elegido: va a llegar al prado */
int ui_check_panel(char *out, int n);
