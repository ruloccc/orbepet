/* ======================================================================
 * (fw 103) Tocar a los animales: lo bueno y lo malo.
 * Incluido al final de pet.c (después de pochi.inc).
 *  - El primer toque siempre es un saludo. El segundo (o el tercero, según el bicho) es un AVISO claro.
 *    Si insistes, pasa lo que tenga que pasar, y ella aprende (de las abejas ya le quedaba miedo).
 *  - En modo infante nunca hay consecuencia mala: se quedan en el aviso.
 *  - Ser paciente también paga: el animal que tocas una vez y dejas tranquilo se queda más rato.
 * ==================================================================== */
#define HEDIONDO_PASOS 18 /* 18 pasos de 10 min = 3 horas de olor (o hasta que la bañes) */

bool pet_stinky(const pet_t *p) { return p && p->hediondo > 0; }

/* cuántos toques aguanta cada animal antes de la consecuencia (0: no se molesta nunca) */
int pet_animal_patience(int a)
{
    switch (a) {
    case ANIMAL_ZORRILLO: return 2;  /* avisa con la cola y rocía */
    case ANIMAL_ERIZO: return 2;     /* se hace bolita y pincha */
    case ANIMAL_HORMIGAS: return 2;
    case ANIMAL_CULEBRA: case ANIMAL_PUMA: case ANIMAL_OSO: case ANIMAL_OSO_POLAR:
    case ANIMAL_LINCE: case ANIMAL_GATO_MONTES: case ANIMAL_TEJON: return 2;
    case ANIMAL_CARACOL: case ANIMAL_TORTUGA: case ANIMAL_TOPO: return 0; /* paciencia infinita: se esconden y ya */
    default: return 3; /* los demás se cansan y se van */
    }
}
/* lo que hace de malo cada uno cuando ya te pasaste */
static int toque_castigo(int a)
{
    switch (a) {
    case ANIMAL_ZORRILLO: return PET_TOC_ROCIA;
    case ANIMAL_ERIZO: return PET_TOC_PUAS;
    case ANIMAL_HORMIGAS: return PET_TOC_PICA;
    case ANIMAL_CULEBRA: case ANIMAL_PUMA: case ANIMAL_OSO: case ANIMAL_OSO_POLAR:
    case ANIMAL_LINCE: case ANIMAL_GATO_MONTES: case ANIMAL_TEJON: return PET_TOC_RUGE;
    default: return PET_TOC_SE_VA;
    }
}
/* lo que pasa de verdad (sirve para los dos: los del mundo y los que pasan volando) */
static int toque_aplicar(pet_t *p, int res, uint32_t t, bool vivo)
{
    switch (res) {
    case PET_TOC_PICA:
        sumar(p, NEED_SALUD, -3);
        sumar(p, NEED_SEGURIDAD, -12);
        impulsar(p, EMO_MIEDO, 0.7f, CAUSA_ABEJA);
        susto_aqui(p, 0.3f);
        if (vivo) sonar(p, SND_LLANTO, t);
        break;
    case PET_TOC_PUAS:
        sumar(p, NEED_SALUD, -2);
        sumar(p, NEED_DIVERSION, -4);
        impulsar(p, EMO_TRISTEZA, 0.4f, CAUSA_TOQUE_FEO);
        if (vivo) sonar(p, SND_LLANTO, t);
        break;
    case PET_TOC_ROCIA:
        p->hediondo = HEDIONDO_PASOS; p->hediondo_min = 0;
        sumar(p, NEED_HIGIENE, -45);
        sumar(p, NEED_SEGURIDAD, -8);
        impulsar(p, EMO_ENOJO, 0.5f, CAUSA_TOQUE_FEO);
        impulsar(p, EMO_TRISTEZA, 0.3f, CAUSA_TOQUE_FEO);
        if (vivo) sonar(p, SND_SORPRESA, t);
        break;
    case PET_TOC_RUGE:
        sumar(p, NEED_SEGURIDAD, -25);
        impulsar(p, EMO_MIEDO, 1.0f, CAUSA_ZORRO);
        susto_aqui(p, 0.6f);
        if (vivo) sonar(p, SND_LLANTO, t);
        break;
    case PET_TOC_AVISA:
        sumar(p, NEED_SEGURIDAD, -3);
        impulsar(p, EMO_MIEDO, 0.2f, CAUSA_TOQUE_FEO);
        break;
    case PET_TOC_SALUDA:
        sumar(p, NEED_CURIOSIDAD, 4);
        impulsar(p, EMO_ALEGRIA, 0.2f, CAUSA_NOVEDAD);
        break;
    default: break;
    }
    return res;
}
int pet_animal_touch(pet_t *p, int a, int veces, uint32_t t)
{
    if (!p || (unsigned)a >= ANIMAL_COUNT || p->etapa == STAGE_HUEVO) return PET_TOC_NADA;
    if (!pet_animal_here(p, (animal_t)a, t)) return PET_TOC_NADA;
    p->ultima_interaccion = t;
    const int pac = pet_animal_patience(a);
    const bool nino = pet_mode(p) == MODO_INFANTE;
    /* los que nunca se molestan */
    if (pac == 0) {
        if (a == ANIMAL_TOPO) { animal_se_va(p, ANIMAL_TOPO, t, ANI_SE_ESCONDE); sonar(p, SND_GRUNIDO, t); return PET_TOC_ESCONDE; }
        sumar(p, NEED_CURIOSIDAD, 3);
        return veces <= 1 ? toque_aplicar(p, PET_TOC_SALUDA, t, true) : PET_TOC_ESCONDE;
    }
    if (veces <= 1) { /* el saludo de siempre */
        if (a == ANIMAL_ERIZO) { push_event(p, EV_ANIMAL, a, ANI_BOLITA, t); return toque_aplicar(p, PET_TOC_SALUDA, t, true); }
        if (a == ANIMAL_CONEJO && !p->dormida) { push_event(p, EV_ANIMAL, a, ANI_CORREN, t); sumar(p, NEED_DIVERSION, 8); }
        return toque_aplicar(p, PET_TOC_SALUDA, t, true);
    }
    if (veces <= pac || nino) return toque_aplicar(p, PET_TOC_AVISA, t, true); /* el aviso (en modo infante se queda aquí) */
    const int cast = toque_castigo(a);
    if (cast == PET_TOC_SE_VA) { /* se cansó: se va y hoy no vuelve */
        animal_se_va(p, (animal_t)a, t, ANI_SE_ASUSTA);
        sumar(p, NEED_DIVERSION, -4);
        impulsar(p, EMO_TRISTEZA, 0.3f, CAUSA_ESPANTASTE);
        return PET_TOC_SE_VA;
    }
    push_event(p, EV_ANIMAL, a, ANI_SE_ASUSTA, t);
    if (cast != PET_TOC_RUGE) animal_se_va(p, (animal_t)a, t, ANI_SE_VA);
    return toque_aplicar(p, cast, t, true);
}
int pet_fauna_touch(pet_t *p, int f, int veces, uint32_t t)
{
    if (!p || (unsigned)f >= FAUNA_COUNT || p->etapa == STAGE_HUEVO) return PET_TOC_NADA;
    if (!pet_fauna_here(p, (fauna_t)f, t)) return PET_TOC_NADA;
    p->ultima_interaccion = t;
    const bool nino = pet_mode(p) == MODO_INFANTE;
    if (f != FAUNA_ABEJA) { /* mariposa, pájaro, zorro, rana: se van, no hacen daño */
        if (veces <= 1) return toque_aplicar(p, PET_TOC_SALUDA, t, true);
        if (veces <= 3 || nino) return toque_aplicar(p, PET_TOC_AVISA, t, true);
        se_va(p, (fauna_t)f, t, true);
        return PET_TOC_SE_VA;
    }
    if (veces <= 1) return toque_aplicar(p, PET_TOC_SALUDA, t, true);
    if (veces <= 2 || nino || t < p->protege_hasta) return toque_aplicar(p, PET_TOC_AVISA, t, true);
    p->miedo_abejas = clampf(p->miedo_abejas + 0.55f, 0, 1); /* y aprende */
    push_event(p, EV_FAUNA, f, FSUC_PICO, t);
    se_va(p, (fauna_t)f, t, false);
    return toque_aplicar(p, PET_TOC_PICA, t, true);
}
