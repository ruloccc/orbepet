# -*- coding: utf-8 -*-
"""Prueba de lo nuevo (firmware 1.0.16 a 1.0.23), en la placa de verdad.

Revisa lo que se hizo en estas versiones y que el simulador no puede probar:
  - la versión instalada y la llave de los informes
  - los fondos de los juegos en la memoria de recursos (pj0..pj5) y que se puedan armar
  - los 4 juegos de a dos y los 2 de la isla: abren, se juegan solos, terminan y se cierran
  - tocar el faro (de noche) y la piedra del buceo en la isla
  - el micrófono: que se abra solo cuando algo espera un soplido (niebla, olor) y, guiado, que oiga tu soplido
  - la velocidad de la pantalla con y sin «juntar áreas por redibujar» (1.0.22), con lluvia, nieve y de noche
  - la memoria interna libre

Se usa desde probar.py («nuevo» = guiada, «nuevo_auto» = sin manos) y como fase 31 de la PRUEBA TOTAL.
Necesita el firmware 1.0.23 o más nuevo (las órdenes de prueba de lo nuevo).
"""
import re
import time

# los fondos publicados (pj4, el faro, es la pantalla nueva de la 1.0.23): nombre -> bytes
FONDOS = {"pj0": 83230, "pj1": 128776, "pj2": 85508, "pj3": 154152, "pj4": 78145, "pj5": 53898}
JUEGOS = ((0, "ping pong"), (1, "tira y afloja"), (2, "memorice"), (3, "escondidas"), (4, "el faro"), (5, "buceo con marea"))


def _num(resp, clave, defecto=None):
    if not resp:
        return defecto
    m = re.search(r"\b" + re.escape(clave) + r"=(-?\d+)", resp)
    return int(m.group(1)) if m else defecto


def _crudo(p, orden, clave, seg=3.0):
    """Una orden de las de arriba (no PROBAR): MASCOTA <orden>, y la respuesta que empiece con clave."""
    try:
        p.s.reset_input_buffer()
        p.s.write(("MASCOTA " + orden + "\n").encode())
        for l in p.leer(seg):
            if l.startswith(clave):
                return l
    except Exception:
        pass
    return None


def correr(p, sal, pr, guiado=False, anota=None):
    """anota(texto, estado) viene de la prueba total; si no, se arma aquí. Devuelve (ok, avisos, fallas)."""
    cuenta = {"OK": 0, "AVISO": 0, "FALLA": 0}
    fallas = []

    def _anota(txt, estado="OK"):
        sal.append(f"[{estado:<5}] {txt}")
        print(sal[-1])
        cuenta[estado if estado in cuenta else "AVISO"] += 1
        if estado == "FALLA":
            fallas.append(txt)
    if anota is None:
        anota = _anota
    else:
        _ext = anota

        def anota(txt, estado="OK"):  # noqa: F811 (cuenta también aquí)
            cuenta[estado if estado in cuenta else "AVISO"] += 1
            _ext(txt, estado)

    def cmd(c, espera=1.0):
        return p.cmd(c, espera)

    def paso(t):
        pr.etapa("Lo nuevo — " + t)

    def ir(zona, seg=45):
        """lleva a la mascota a una zona y espera a que la pantalla ya la muestre (a la isla y de vuelta se va en bote)"""
        cmd(f"ZONA {zona}", 1.0)
        t0 = time.time()
        while time.time() - t0 < seg:
            if _num(cmd("LUGARINFO", 0.1), "vista", -1) == zona:
                p.leer(1.5)
                return True
            p.leer(1.0)
        anota(f"no llegó a la zona {zona} en {seg} s", "AVISO")
        return False

    # ------------------------------------------------------------ versión y llave
    paso("versión")
    v = _crudo(p, "VERSION", "MASCOTA VERSION")
    ver = v.split()[2] if v and len(v.split()) > 2 else "?"
    try:
        nums = [int(x) for x in ver.split(".")[:3]]
    except Exception:
        nums = [0, 0, 0]
    anota(f"versión instalada: {ver}" + ("" if nums >= [1, 0, 23] else "  ← hace falta la 1.0.23 o más nueva para esta prueba (BUSCAR ACTUALIZACIÓN)"),
          "OK" if nums >= [1, 0, 23] else "FALLA")
    if nums < [1, 0, 23]:
        return cuenta, fallas
    k = _crudo(p, "LLAVE ?", "MASCOTA LLAVE")
    anota("llave de los informes: " + ("cargada (ENVIAR INFORME funciona)" if k and "SI" in k else
                                        "NO cargada (usa «Cargar llave de informes…» en la pestaña Respaldo)"),
          "OK" if k and "SI" in k else "AVISO")
    cmd("CERRAR", 0.5)
    cmd("PANTALLA 1", 0.5)
    d0 = p.info() or {}
    anota(f"memoria al empezar: RAM interna {d0.get('ram')} B (bloque mayor {d0.get('mayor')}), PSRAM {d0.get('psram')} KB")

    # ------------------------------------------------------------ fondos de los juegos
    paso("fondos de los juegos")
    r = cmd("RECURSOS", 0.5)
    faltan = []
    for n, tam in FONDOS.items():
        t = _num(r, n, -1)
        if t != tam:
            faltan.append(f"{n} ({'falta' if t < 0 else f'{t} bytes, se esperaban {tam}'})")
    anota("los 6 fondos de los juegos están en la placa" if not faltan else
          "faltan fondos: " + ", ".join(faltan) + " → aprieta BUSCAR ACTUALIZACIÓN otra vez (ya al día, los baja)",
          "OK" if not faltan else "FALLA")
    for n in range(6):
        r = cmd(f"FONDODOS {n}", 1.5)
        anota(f"fondo pj{n} se arma ({_num(r, 'w', 0)}×{_num(r, 'h', 0)}, PSRAM libre {_num(r, 'psram')} KB, bloque mayor {_num(r, 'bloque')} KB)",
              "OK" if r and " OK " in r else "FALLA")

    # ------------------------------------------------------------ los juegos de a dos y de la isla
    cmd("TRAMPALUGAR 1", 0.2)   # en modo prueba juegan solos
    for n, nom in JUEGOS:
        paso(f"juego: {nom}")
        cmd("CERRAR", 0.4)
        r = cmd(f"ZDABRIR {n}", 1.5)
        e = _num(cmd("ZDINFO", 0.2), "estado", -1)
        if e is None or e < 0 or e // 1000 != 0:
            anota(f"juego {nom}: no abrió la portada (estado {e})", "FALLA")
            continue
        cmd("TOCARLUGAR 233 233", 0.6)   # la portada: empezar
        fin, t0, peor = False, time.time(), 0
        while time.time() - t0 < 150:
            p.leer(1.0)
            vel = cmd("VELOCIDAD", 0.1)
            peor = max(peor, _num(vel, "ms", 0) or 0)
            e = _num(cmd("ZDINFO", 0.1), "estado", -1)
            if e is not None and e >= 0 and e // 1000 >= 2:
                fin = True
                break
            if e is not None and e < 0:
                break
        dur = round(time.time() - t0)
        d = p.info() or {}
        anota(f"juego {nom}: " + (f"se jugó y terminó en {dur} s" if fin else f"NO terminó en {dur} s (estado {e})") +
              f" — dibujo hasta {peor} ms por cuadro, RAM interna {d.get('ram')} B",
              "OK" if fin and d.get("ram", 0) > 4000 else ("AVISO" if fin else "FALLA"))
        time.sleep(2.0)
        cmd("TOCARLUGAR 262 417", 0.8)   # la X del final
        e = _num(cmd("ZDINFO", 0.2), "estado", 0)
        if e is not None and e >= 0:
            cmd("TOCARLUGAR 233 417", 0.8)
            e = _num(cmd("ZDINFO", 0.2), "estado", 0)
        anota(f"juego {nom}: se cierra con la X", "OK" if e == -1 else "FALLA")
    cmd("TRAMPALUGAR 0", 0.2)

    # ------------------------------------------------------------ tocar el faro y la piedra en la isla
    paso("la isla se toca")
    ir(5)
    cmd("MOMENTO 22:30", 2.0)   # (de noche: en verano a las 19:30 todavía es de día)
    r = cmd("ISLATOQUE 392 130", 1.5)
    e = _num(cmd("ZDINFO", 0.2), "estado", -1)
    anota("isla: tocar el faro de noche abre su juego", "OK" if e is not None and e >= 0 else "FALLA")
    cmd("TOCARLUGAR 233 417", 0.8)
    cmd("MOMENTO 12:00", 2.0)
    r = cmd("ISLATOQUE 392 130", 1.5)
    e = _num(cmd("ZDINFO", 0.2), "estado", -1)
    anota("isla: tocar el faro de día NO abre el juego (te dice que vuelvas al atardecer)", "OK" if e == -1 and _num(r, "hizo", 0) == 1 else "FALLA")
    cmd("CERRAR", 0.5)
    r = cmd("ISLATOQUE 44 140", 1.5)
    e = _num(cmd("ZDINFO", 0.2), "estado", -1)
    anota("isla: tocar el agua sobre la piedra de la izquierda abre el buceo", "OK" if e is not None and e >= 0 else "FALLA")
    cmd("TOCARLUGAR 233 417", 0.8)
    r = cmd("ISLATOQUE 233 300", 1.0)
    anota("isla: tocar otra parte no abre nada", "OK" if _num(r, "hizo", 1) == 0 else "FALLA")
    cmd("MOMENTO REAL", 1.0)

    # ------------------------------------------------------------ el micrófono
    paso("micrófono")
    ir(0)
    cmd("CERRAR", 0.5)
    cmd("NIEBLA 0", 0.5); cmd("OLOR 0", 0.5)
    p.leer(1.5)
    s = cmd("SOPLOS", 0.3)
    anota(f"sin niebla ni olor el micrófono no espera nada (quiere={_num(s, 'quiere')})", "OK" if _num(s, "quiere") == 0 else "AVISO")
    cmd("NIEBLA 1", 1.0)
    p.leer(2.5)
    s = cmd("SOPLOS", 0.3)
    anota(f"con niebla el micrófono se enciende solo (quiere={_num(s, 'quiere')}, abierto={_num(s, 'abierto')}, memoria DMA libre {_num(s, 'dma')} B)",
          "OK" if _num(s, "quiere") == 1 and _num(s, "abierto") == 1 else "FALLA")
    if guiado:
        antes = _num(s, "soplos", 0)
        pr.guia_paso("Sopla la niebla", "Hay niebla en el prado. SOPLA fuerte hacia el orbe, de cerca, 1 o 2 segundos (puedes repetir).", 20, 1, 3)
        se_fue = False
        for _ in range(40):
            p.leer(0.5)
            s = cmd("SOPLOS", 0.1)
            if _num(s, "niebla", 1) == 0:
                se_fue = True
                break
        pr.guia_estado("¡La niebla se fue!" if se_fue else "La niebla no se fue", se_fue)
        anota(f"soplar la niebla: {'se fue' if se_fue else 'NO se fue en 20 s'} (soplidos oídos: {(_num(s, 'soplos', 0) or 0) - (antes or 0)})",
              "OK" if se_fue else "FALLA")
    else:
        anota("soplar la niebla: necesita tus manos (usa «Probar lo nuevo (guiada)»)", "AVISO")
    cmd("NIEBLA 0", 0.5)
    cmd("OLOR 6", 1.0)
    p.leer(2.5)
    s = cmd("SOPLOS", 0.3)
    anota(f"hedionda: el micrófono se enciende solo (quiere={_num(s, 'quiere')}, olor={_num(s, 'olor')})", "OK" if _num(s, "quiere") == 1 else "FALLA")
    if guiado:
        pr.guia_paso("Sopla el mal olor", "Ahora anda hedionda (con vahos verdes). SOPLA hacia el orbe para espantar los vahos. También puedes tocarlos.", 25, 2, 3)
        limpio = False
        for _ in range(50):
            p.leer(0.5)
            s = cmd("SOPLOS", 0.1)
            if _num(s, "olor", 1) == 0:
                limpio = True
                break
        pr.guia_estado("¡Quedó limpia!" if limpio else "Sigue hedionda", limpio)
        anota(f"soplar el olor: {'se fue' if limpio else 'NO se fue en 25 s'}", "OK" if limpio else "FALLA")
    cmd("OLOR 0", 0.5)
    if guiado:
        ir(2)
        cmd("BOTE 1", 2.0)
        p.leer(3.0)
        s = cmd("SOPLOS", 0.2)
        antes = _num(s, "soplos", 0) or 0
        pr.guia_paso("Sopla la vela del bote", "Van en el bote a la isla. SOPLA hacia el orbe para que el bote avance más rápido (2 o 3 soplidos).", 20, 3, 3)
        oyo = 0
        for _ in range(40):
            p.leer(0.5)
            s = cmd("SOPLOS", 0.1)
            oyo = (_num(s, "soplos", 0) or 0) - antes
            if oyo >= 2:
                break
        pr.guia_estado(f"Oyó {oyo} soplidos", oyo >= 1)
        pr.guia_cerrar()
        anota(f"en el bote: el micrófono oyó {oyo} soplidos (quiere={_num(s, 'quiere')})", "OK" if oyo >= 1 else "FALLA")
        p.leer(8.0)   # que termine de cruzar
    cmd("CERRAR", 0.5)

    # ------------------------------------------------------------ velocidad: juntar áreas (1.0.22) sí / no
    paso("velocidad de la pantalla")
    sal.append("   velocidad de la pantalla (cuadros en 3 s, ms por cuadro y miles de píxeles redibujados por cuadro), sin y con «juntar áreas»:")
    print(sal[-1])
    ir(0)
    for clima, mom, nom in ((0, "12:00", "prado de día"), (2, "12:00", "con lluvia"), (4, "12:00", "con nieve"), (5, "12:00", "con tormenta"), (0, "23:00", "de noche")):
        cmd(f"CLIMA {clima}", 1.0)
        cmd(f"MOMENTO {mom}", 1.0)
        res = []
        for f in (0, 1):
            cmd(f"FUSION {f}", 0.2)
            p.leer(4.0)   # una ventana de 3 s completa
            vel = cmd("VELOCIDAD", 0.2)
            res.append((_num(vel, "cuadros", 0), _num(vel, "ms", 0), _num(vel, "kpx", 0)))
        (c0, m0, k0), (c1, m1, k1) = res
        mejor = k0 and k1 is not None and k1 <= k0
        anota(f"{nom:<14} antes: {c0} cuadros, {m0} ms, {k0} kpx  →  ahora: {c1} cuadros, {m1} ms, {k1} kpx", "OK" if mejor or k0 == 0 else "AVISO")
    cmd("FUSION 1", 0.2)
    cmd("CLIMA 0", 1.0)
    cmd("MOMENTO REAL", 1.0)

    # ------------------------------------------------------------ memoria al final
    d = p.info() or {}
    anota(f"memoria al terminar: RAM interna {d.get('ram')} B (al empezar {d0.get('ram')}), PSRAM {d.get('psram')} KB (al empezar {d0.get('psram')})",
          "OK" if d.get("ram", 0) > 4000 else "AVISO")
    return cuenta, fallas
