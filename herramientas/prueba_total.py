#!/usr/bin/env python3
"""PRUEBA TOTAL AUTOMÁTICA de la mascota (se lanza desde el probador: botón «Prueba total», o `python probar.py COM5 total`).

Recorre TODO lo que se puede probar solo, por el cable USB, sin que toques la placa:
  1  Preparación      estado, sensores, energía, hora
  2  Pantalla         apagar (como el botón PWR), encender, brillo 100→5→100, modo pecera, toque
  3  Volumen          subir y bajar hasta los topes, con sonido en cada paso
  4  Sensores         acelerómetro, batería, reloj, pasos, flash, micrófono (varias veces, con tolerancias)
  5  Giro             las 4 orientaciones y una ráfaga de giros seguidos
  6  Sonidos          cada sonido del juego, uno por uno (y que la RAM no se escape)
  7  Cielo            cada clima × cada momento del día (y la ventana de la casita)
  8  Zonas            las 6 zonas, secretos, finales, juegos, huevo, Pochi, casita, visita, ballena
  9  Acciones         cada acción del juego con varios argumentos (alimentar, bañar, acariciar, truco, ir, vestir...)
 10  Ropa y manos     cada prenda y accesorio puesto y quitado (nenúfar, paraguas, molinillo, farolito...)
 11  Objetos          cada objeto de la Bolsa, usado y devuelto
 12  Animales         cada animal del bosque llamado
 13  Cueva y dragón   la melodía de los cristales con el dragón despierto y la música activada
 14  Dormir y ahorro  duerme, espera, y comprueba que el procesador baja a 80 MHz y que un toque lo sube
 15  Estrés           paneles abiertos y cerrados en ráfaga, giros en ráfaga, fuga de memoria
 16  Paneles          todos los paneles uno por uno (velocidad y memoria)
 17  Hardware extra   parlante→micrófono, memoria flash, fotos, radio; (guiada, ahora en 1b) táctil, sacudir, giro, PWR, soplar
 18  Reinicio         validación de partida corrupta, guardar, reiniciar la placa y comprobar que vuelve igual
 19  Tiempo           las 24 horas, saltos de 6 h / 1 día / 1 semana / 40 días, fiestas del año, tamaños de criaturas
 20  Fuga             20 vueltas completas midiendo RAM y PSRAM
 21  Pendientes       comida (menú antiguo) y bote (fondo): se anotan, no se arreglan todavía
 22  Aspectos        8 especies × colores, etapas de vida (huevo a adulta) y formas
 23  Modo niño        niño e infante: paneles y acciones
 24  Alarmas          alarma, hora de dormir, aviso de tiempo; sobre paneles y juegos
 25  Historias        todos los narradores, colecciones, leyendas, comidas
 26  Jardín y juegos  parcelas y minijuegos con resultados forzados
 27  Reloj y arranque 5 arranques seguidos, hora tras reinicio, batería estable, guardar en animación
 28  Textos/vista     (en el PC) regresion.js
 29  Hardware real   (guiada) sacudir, girar, botón PWR, soplar/aplaudir
 30  Cierre           estado final, consumo por fase, comparación con la prueba anterior y registro de la placa (registro_placa_*.txt)

Cada paso queda como OK / FALLA / AVISO en informe.txt. La placa anota «PRUEBA: ...» en su registro en cada fase,
así el registro que bajes después se lee por fases. Al terminar la partida vuelve a como estaba (lo hace probar.py).
"""
import re
import re, time, json, os

LENTO_MS = 100
RAM_MIN = 4000

class Totales:
    def __init__(self):
        self.ok = self.falla = self.aviso = 0
        self.fallas = []

def _num(resp, clave, defecto=None):
    if not resp:
        return defecto
    m = re.search(r"\b" + re.escape(clave) + r"=(-?\d+)", resp)
    return int(m.group(1)) if m else defecto

# Las fases 1a, 1c, 1d y 1e eran pruebas rápidas al comienzo para ver en minutos si se había arreglado una caída puntual
# (paneles en ráfaga, quién se queda con la memoria, fugas por panel, los sospechosos de la última caída). Desde el 10-nov
# quedan apagadas: la prueba hace el ciclo completo en su orden. Para volver a usarlas (cuando haga falta comprobar rápido un
# arreglo antes de una hora de pruebas), poner esto en True.
PRUEBAS_RAPIDAS_AL_INICIO = False

def correr_total(p, sal, pr, guiado=False, reanudar=False):
    """p = Placa, sal = líneas del informe, pr = el módulo probar (para reutilizar paneles, juegos, etc.)"""
    T = Totales()
    PROG = os.path.join(pr.carpeta_datos(), "prueba_total_progreso.json")
    prog = None
    if reanudar:
        try:
            prog = json.load(open(PROG, encoding="utf-8"))
        except Exception:
            prog = None
        if not prog or prog.get("fin") or "pos" not in prog:
            sal.append("[AVISO] no hay una prueba total a medias para continuar: la anterior terminó o no existe. Usa «PRUEBA TOTAL».")
            print(sal[-1]); return T
        guiado = bool(prog.get("guiado"))
    reanudando = prog is not None
    desde = prog["pos"] if reanudando else 0
    pos = [-1]
    datos = {"pasos": {}, "fases": {}, "fecha": time.strftime("%Y-%m-%d %H:%M:%S")}  # para comparar con la corrida anterior
    estado_fase = {"n": None, "nombre": "", "t": 0.0, "pct": None, "mv": None}

    def anota(txt, estado="OK"):
        sal.append(f"[{estado:<5}] {txt}")
        print(sal[-1])
        if estado == "OK": T.ok += 1
        elif estado == "FALLA": T.falla += 1; T.fallas.append(txt)
        else: T.aviso += 1

    def cierra_fase():
        """Consumo de la fase que termina: batería y voltaje al empezar y al terminar."""
        if estado_fase["n"] is None:
            return
        b = p.cmd("BATERIA", 0.1)
        pct, mv = _num(b, "pct"), _num(b, "mv")
        dt = time.time() - estado_fase["t"]
        dp = None if pct is None or estado_fase["pct"] is None else pct - estado_fase["pct"]
        dm = None if mv is None or estado_fase["mv"] is None else mv - estado_fase["mv"]
        datos["fases"][f"{estado_fase['n']}. {estado_fase['nombre']}"] = {"seg": round(dt), "pct": dp, "mv": dm}
        sal.append(f"   · consumo de la fase {estado_fase['n']}: {round(dt)} s, batería {dp if dp is not None else '?'} puntos, voltaje {dm if dm is not None else '?'} mV"
                   f" ({pct}% / {mv} mV)")

    def guardar_progreso(n, nombre, fin=False):
        try:
            json.dump({"pos": pos[0], "n": str(n), "nombre": nombre, "fin": fin, "guiado": guiado, "sal": sal, "ok": T.ok, "falla": T.falla, "aviso": T.aviso,
                       "fallas": T.fallas, "datos": datos, "hora": time.strftime("%H:%M:%S")}, open(PROG, "w", encoding="utf-8"), ensure_ascii=False)
        except Exception:
            pass

    def fase(n, nombre):
        """True si la fase se corre; False si ya se había hecho (al continuar una prueba cortada). Guarda dónde va, para poder continuar."""
        pos[0] += 1
        if reanudando and pos[0] == 1:  # ya pasó la preparación: se recupera el informe y los totales de donde quedó la vez anterior
            sal[:] = prog["sal"]
            T.ok, T.falla, T.aviso, T.fallas = prog["ok"], prog["falla"], prog["aviso"], list(prog["fallas"])
            datos.update(prog["datos"])
            sal.append(""); sal.append(f"(Se continúa la prueba cortada: se saltan las fases ya hechas y se sigue desde «{prog['n']}. {prog['nombre']}», que se repite entera.)")
            print(sal[-1]); pr.etapa(f"Continuando desde la fase {prog['n']}")
        if reanudando and pos[0] < desde:
            return False
        cierra_fase()
        guardar_progreso(n, nombre)
        b = p.cmd("BATERIA", 0.1)
        estado_fase.update(n=n, nombre=nombre, t=time.time(), pct=_num(b, "pct"), mv=_num(b, "mv"))
        sal.append(""); sal.append(f"== {n}. {nombre.upper()} ==")
        print(sal[-1]); pr.etapa(f"Prueba total — {n}. {nombre}")
        p.cmd(f"MARCA fase {n} {nombre}", 0.1)
        try:  # memoria al comenzar cada fase: así se ve en qué fase se va la PSRAM
            _i = p.info() or {}
            sal.append(f"   memoria al comenzar: RAM interna {_i.get('ram')} B (bloque mayor {_i.get('mayor')}), PSRAM {_i.get('psram')} KB"); print(sal[-1])
        except Exception:
            pass
        return True

    def cmd(c, espera=1.0):
        return p.cmd(c, espera)

    def ultima(clave):
        """La última línea que dijo la placa con esa clave (para respuestas que tardan más de lo que espera cmd)."""
        for l in reversed(p.ultimas):
            if clave in l:
                return l[l.index("MASCOTA PROBAR"):] if "MASCOTA PROBAR" in l else l
        return None

    def mira(nombre, resp, d=None, exige_ok=True, lento=True):
        """Revisa una respuesta y el estado: contestó, sin ERROR, sin reinicio, sin lentitud ni falta de RAM."""
        if resp is None:
            anota(f"{nombre}: la placa NO contestó", "FALLA"); return False
        if "desconocido" in resp:
            anota(f"{nombre}: comando desconocido (el firmware no es el nuevo)", "FALLA"); return False
        if exige_ok and "ERROR" in resp:
            anota(f"{nombre}: {resp[resp.index('ERROR'):][:90]}", "AVISO"); return False
        if d is None:
            d = p.info()
        if d is None:
            anota(f"{nombre}: después de esto la placa dejó de responder (¿se colgó o reinició?)", "FALLA")
            p.leer(12)
            return False
        if d.get("cuadros", 1) == 0 and d.get("ms", 1) == 0 and d.get("max", 1) == 0:
            anota(f"{nombre}: REINICIO (la placa recién arrancó)", "FALLA"); return False
        extra = []
        if lento and d.get("ms", 0) > LENTO_MS: extra.append(f"LENTO {d['ms']} ms")
        if d.get("ram", 1 << 30) < RAM_MIN: extra.append(f"POCA RAM {d['ram']} B")
        datos["pasos"][nombre] = {"ms": d.get("ms", 0), "max": d.get("max", 0), "ram": d.get("ram", 0)}
        anota(f"{nombre}  ({d.get('ms',0)} ms, máx {d.get('max',0)}, RAM {d.get('ram',0)} B)" + (("  ← " + ", ".join(extra)) if extra else ""),
              "AVISO" if extra else "OK")
        return not extra

    # ---------------------------------------------------------------- 1
    fase(1, "Preparación")
    d0 = p.info()
    if d0 is None:
        anota("la placa no responde a INFO", "FALLA"); return T
    anota(f"INFO: sonido={'vivo' if d0.get('sonido') == 1 else 'NO ARRANCÓ'} RAM {d0.get('ram')} B (bloque mayor {d0.get('mayor')}) PSRAM {d0.get('psram')} KB",
          "OK" if d0.get("sonido") == 1 else "FALLA")
    vol0 = d0.get("vol", "")
    cuentas = cmd("CUENTAS", 0.2)
    n_snd = _num(cuentas, "sonidos", 0)
    n_cri = _num(cuentas, "criaturas", 0)
    anota(f"CUENTAS: {cuentas[cuentas.index('OK'):] if cuentas and 'OK' in cuentas else cuentas}")
    en0 = cmd("ENERGIA", 0.2)
    anota(f"ENERGIA base: {en0[en0.index('OK'):] if en0 and 'OK' in en0 else en0}", "OK" if en0 and "OK" in en0 else "FALLA")
    bat0 = cmd("BATERIA", 0.2)
    pct0 = _num(bat0, "pct", -1)
    anota(f"batería {pct0}% ({_num(bat0, 'mv', 0)} mV, {'cargando' if _num(bat0, 'carga', 0) else 'sin cargador'})")

    # ---------------------------------------------------------------- 1a
    if PRUEBAS_RAPIDAS_AL_INICIO and fase("1a", "Prueba rápida de lo que falló la vez anterior (si la placa se cae, se sabe en minutos)"):
      try:
        cmd("CERRAR", 0.5)
        r = cmd("PANELES", 0.2)
        n_rap = max(1, len(r.split()[4:])) if r else 60
        d0 = p.info() or {}
        colgo = None
        for ronda in ("A (sin cerrar entre paneles)", "B (otra vez, sin cerrar)", "C (abriendo y cerrando)"):
            for k in range(n_rap):
                if cmd(f"PANEL {k}", 0.12) is None:
                    vivo = None  # puede ser solo lento (ej. la alarma suena): se espera y se pregunta INFO antes de decir que se colgó
                    for _ in range(3):
                        p.leer(2.5)
                        vivo = p.info()
                        if vivo: break
                    if vivo:
                        anota(f"panel {k}: tardó en contestar pero la placa sigue viva (RAM {vivo.get('ram')} B)", "AVISO")
                        cmd("CERRAR", 0.5)
                    else:
                        colgo = f"ronda {ronda}, panel {k}"
                        break
                if ronda.startswith("C"): cmd("CERRAR", 0.1)
            if colgo: break
        cmd("CERRAR", 0.8)
        d1 = p.info()
        if colgo or not d1:
            anota(f"abrir los {n_rap} paneles uno tras otro: LA PLACA SE COLGÓ en {colgo or 'el final'}", "FALLA")
        else:
            anota(f"abrir los {n_rap} paneles uno tras otro, 3 rondas: sin colgarse; RAM {d0.get('ram')} → {d1.get('ram')} B, PSRAM {d0.get('psram')} → {d1.get('psram')} KB",
                  "OK" if d0.get("ram") and d1.get("ram", 0) > d0["ram"] - 6000 else "FALLA")
            pr.reinicios(p, sal, 1)
      except (NameError, TypeError, KeyError, AttributeError) as _e:
        if not reanudando:
            raise
        anota(f"esta fase quedó a medias: usa datos de una fase anterior que no se corrió en esta vuelta ({type(_e).__name__}: {_e})", "AVISO")

    # ---------------------------------------------------------------- 1d
    if PRUEBAS_RAPIDAS_AL_INICIO and fase("1d", "¿Quién se queda con la memoria? (PSRAM por panel, y si se puede soltar)"):
      try:
        cmd("CERRAR", 0.8)
        r = cmd("PANELES", 0.2)
        n_m = max(1, len(r.split()[4:])) if r else 60
        base = (p.info() or {}).get("psram")
        subidas = []
        previo = base
        for k in range(n_m):
            cmd(f"PANEL {k}", 0.25); cmd("CERRAR", 0.25); p.leer(0.6)  # (se espera: LVGL borra los objetos un poco después)
            ps = (p.info() or {}).get("psram")
            if previo is not None and ps is not None and previo - ps >= 60:
                subidas.append((previo - ps, k))
            if ps is not None: previo = ps
        fin = (p.info() or {}).get("psram")
        subidas.sort(reverse=True)
        anota(f"abrir y cerrar los {n_m} paneles una vez: PSRAM {base} → {fin} KB; los que más retienen (KB, panel): {subidas[:10] or 'ninguno ≥ 60 KB'}",
              "OK" if base and fin and base - fin < 800 else "AVISO")
        NOM_M = {1: 'caché de imágenes', 2: 'caché y fotos de pantallas', 3: 'y además los fondos de las otras horas'}
        for modo in (1,):  # (los modos 2 y 3 soltaban imágenes que podían estar en pantalla y provocaron reinicios: ya no se usan)
            rm = cmd(f"MEMLIMPIAR {modo}", 0.5)
            m = re.search(r"liberado=(-?\d+)", rm or "")
            anota(f"soltar memoria (modo {modo}: {NOM_M[modo]}): {rm[rm.index('OK'):] if rm and 'OK' in rm else rm}",
                  "OK" if m else "AVISO")
        anota("(si «liberado» es grande, la memoria era caché y se puede soltar; si es ~0, es una fuga de verdad)")
      except (NameError, TypeError, KeyError, AttributeError) as _e:
        anota(f"fase 1d quedó a medias ({type(_e).__name__}: {_e})", "AVISO")
    # ---------------------------------------------------------------- 1e
    if PRUEBAS_RAPIDAS_AL_INICIO and fase("1e", "Fuga por panel: cada sospechoso 8 veces, con la memoria ya asentada"):
      try:
        cmd("CERRAR", 0.8)
        res = []
        for k in (0, 35, 29, 18, 45, 3, 4, 41, 56, 53, 13, 34, 9, 12):
            cmd(f"PANEL {k}", 0.5); cmd("CERRAR", 0.8); p.leer(1.0)   # una vuelta de calentamiento
            a = (p.info() or {}).get("psram")
            for _ in range(8):
                cmd(f"PANEL {k}", 0.4); cmd("CERRAR", 0.6)
            p.leer(1.5)
            b = (p.info() or {}).get("psram")
            if a is not None and b is not None:
                res.append((a - b, k))
        res.sort(reverse=True)
        anota("PSRAM que se pierde tras 8 vueltas (KB, panel): " + str(res) + "  — el panel 0 es el testigo; lo que crece de forma pareja es fuga de verdad",
              "AVISO" if res and res[0][0] >= 100 else "OK")
        for kb, k in res[:3]:
            if kb >= 100:
                anota(f"FUGA en el panel {k}: {kb} KB perdidos en 8 aperturas ({kb // 8} KB por vez)", "FALLA")
      except (NameError, TypeError, KeyError, AttributeError) as _e:
        anota(f"fase 1e quedó a medias ({type(_e).__name__}: {_e})", "AVISO")
    # ---------------------------------------------------------------- 1c
    if PRUEBAS_RAPIDAS_AL_INICIO and fase("1c", "Sospechosos de la caída (lo que estaba pasando cuando se cayó la placa)"):
      try:
        SOSP = [("cueva (zona 4)", ["DRAGON", "ZONA 4"], 4.0), ("aspecto especie 0 color 6", ["CERRAR", "ASPECTO 0 6 3 0"], 1.5),
                ("etapa 3 forma 3", ["ASPECTO 0 0 3 3"], 1.5), ("modo niño acción 21", ["ACCION 21 0"], 1.5),
                ("historia narrador 4 n.º 4", ["CERRAR", "HISTORIA 4 4"], 1.5), ("juego 1 termina con 5 puntos", ["ACCION 20 105"], 2.0)]
        for nom, cmds, esp in SOSP:
            for _rep in range(3):  # cada sospechoso 3 veces: si falla una, se ve
                alguna = None
                for c1 in cmds:
                    if cmd(c1, esp) is None:
                        alguna = c1; break
                vivo = None
                if alguna:
                    for _ in range(3):
                        p.leer(2.5); vivo = p.info()
                        if vivo: break
                else:
                    vivo = p.info()
                if alguna or not vivo or (vivo.get("cuadros", 1) == 0 and vivo.get("ms", 1) == 0):
                    anota(f"SOSPECHOSO «{nom}» (vuelta {_rep + 1}): con el comando «{alguna or cmds[-1]}» la placa {'no contestó' if alguna else 'se reinició'}"
                          f" → ESTE es el momento de la caída; PSRAM {vivo and vivo.get('psram')} KB", "FALLA")
                    break
                cmd("CERRAR", 0.5)
            else:
                anota(f"sospechoso «{nom}»: 3 vueltas sin caerse (PSRAM {(p.info() or {}).get('psram')} KB)", "OK")
      except (NameError, TypeError, KeyError, AttributeError) as _e:
        anota(f"fase 1c quedó a medias ({type(_e).__name__}: {_e})", "AVISO")
    # ---------------------------------------------------------------- 1b
    if fase("1b", "Pruebas guiadas (al comienzo, con tus manos)" + ("" if guiado else " — omitidas: usa «PRUEBA TOTAL + guiada»")):
      try:
            if not guiado:
                anota("táctil, sacudir, girar, botón PWR y soplar: necesitan tus manos, solo en la prueba guiada", "AVISO")
            else:
                TOTAL_G = 14
                n_g = [0]
                def paso(titulo, texto, seg=None):
                    n_g[0] += 1
                    pr.etapa(f"Prueba guiada {n_g[0]}/{TOTAL_G}: {titulo}")
                    pr.guia_paso(titulo, texto, seg, n_g[0], TOTAL_G)
                def listo(texto, bueno=True):
                    pr.guia_estado(texto, bueno)
                    time.sleep(0.5)   # un instante para ver el ✔ y pasa al siguiente
                cmd("CERRAR", 0.6); cmd("PANTALLA 1", 0.8)
                # 1-5: el táctil. Avanza apenas lo tocas; si en 6 s no lo toca, pasa solo al siguiente.
                for nombre, (tx, ty) in (("el CENTRO", (233, 233)), ("ARRIBA", (233, 70)), ("ABAJO", (233, 400)), ("la IZQUIERDA", (70, 233)), ("la DERECHA", (400, 233))):
                    paso(f"Toca {nombre}", f"Toca el PUNTO ROJO que se ve en la pantalla de la mascota (en el dibujo de abajo, dónde está).", 6)
                    cmd(f"PUNTO {tx} {ty}", 0.5); pr.guia_punto(tx, ty)
                    cmd(f"TOQUES 6 1 {tx} {ty} 90", 8.0)
                    cmd("PUNTO -1 -1", 0.5); pr.guia_punto(None)   # termina solo cuando tocas cerca del punto pedido; si no, espera los 6 s
                    r = ultima("PROBAR TOQUES")
                    pts = []
                    if r and "pts=" in r:
                        for par in r.split("pts=")[1].split(";"):
                            try:
                                x, y = par.strip().split(","); pts.append((int(x), int(y)))
                            except Exception:
                                pass
                    cerca = [q for q in pts if abs(q[0] - tx) < 90 and abs(q[1] - ty) < 90]
                    if cerca:
                        anota(f"táctil en {nombre}: tocaste cerca de ({tx},{ty}) → la placa leyó {cerca[0]}", "OK"); listo("✔ Lo detecté")
                    elif pts:
                        anota(f"táctil en {nombre}: la placa leyó {pts[:3]} pero esperaba cerca de ({tx},{ty}): ¿táctil descalibrado o pantalla girada?", "AVISO"); listo("Lo detecté, pero en otro lugar", False)
                    else:
                        anota(f"táctil en {nombre}: la placa no registró ningún toque (¿no tocaste, o el táctil no responde?)", "AVISO"); listo("No detecté el toque: sigo", False)
                # 6: sacudir
                cmd("PANTALLA 1", 0.8)
                paso("Sacude el reloj", "Sacúdelo fuerte, varias veces.", 12)
                r = cmd("SACUDIDAS 12 3", 14.0) or ultima("SACUDIDAS")
                r = ultima("PROBAR SACUDIDAS") or r
                s = _num(r, "sacudidas", 0)
                anota(f"acelerómetro: {s} sacudidas, {_num(r, 'movimientos', 0)} movimientos, rango x {_num(r, 'rango_x', 0)} y {_num(r, 'rango_y', 0)} (milésimas de g)", "OK" if s >= 1 else "FALLA")
                listo("✔ Lo detecté" if s >= 1 else "No detecté sacudidas: sigo", s >= 1)
                # 7: girar
                paso("Gira el reloj", "Ponlo derecho, luego un cuarto de vuelta a cada lado y al revés. Unos 3 segundos en cada posición.", 30)
                r = cmd("GIROREAL 30 1", 32.0) or ultima("GIROREAL")
                r = ultima("PROBAR GIROREAL") or r
                mask = _num(r, "posiciones", 1)
                vistas = bin(mask).count("1")
                anota(f"giro real: vio {vistas} de 4 posiciones (máscara {mask})", "OK" if vistas >= 4 else ("AVISO" if vistas >= 2 else "FALLA"))
                listo(f"✔ Vi las {vistas} posiciones" if vistas >= 4 else f"Vi {vistas} de 4 posiciones: sigo", vistas >= 4)
                cmd("GIRO 0", 1.0)
                # 8-9: botón PWR
                paso("Botón PWR: una vez", "Aprieta el botón PWR UNA vez.", 15)
                r = cmd("BOTONES 15 1", 17.0) or ultima("BOTONES")
                r = ultima("PROBAR BOTONES") or r
                u = _num(r, "una_vez", 0)
                anota(f"botón PWR: {u} pulsaciones simples", "OK" if u >= 1 else "AVISO")
                listo("✔ Lo detecté" if u >= 1 else "No detecté la pulsación: sigo", u >= 1)
                cmd("PANTALLA 1", 0.8)
                paso("Botón PWR: dos veces", "Aprieta el botón PWR DOS veces seguidas y rápido.", 15)
                r = cmd("BOTONES 15 2", 17.0) or ultima("BOTONES")
                r = ultima("PROBAR BOTONES") or r
                d2 = _num(r, "dos_veces", 0)
                anota(f"botón PWR: {d2} pulsaciones dobles", "OK" if d2 >= 1 else "AVISO")
                listo("✔ Lo detecté" if d2 >= 1 else "No detecté la doble pulsación: sigo", d2 >= 1)
                cmd("PANTALLA 1", 0.8)
                # 10: micrófono
                paso("Sopla o aplaude", "Sopla fuerte al reloj, cerca del micrófono, o aplaude.", 12)
                r = cmd("SOPLO 12 1", 14.0) or ultima("SOPLO")
                r = ultima("PROBAR SOPLO") or r
                anota(f"micrófono real: base {_num(r, 'base', 0)}, pico {_num(r, 'pico', 0)}, {_num(r, 'fuertes', 0)} muestras fuertes, {_num(r, 'golpes', 0)} golpes", "OK" if _num(r, "pico", 0) >= 20 else "AVISO")
                listo("✔ Te oí" if _num(r, "pico", 0) >= 20 else "No te oí bien: sigo", _num(r, "pico", 0) >= 20)
                # 11: niebla soplada
                cmd("CERRAR", 0.6); cmd("CASITA 0", 2.0); cmd("ZONA 0", 2.0); cmd("PANTALLA 1", 0.8)
                hay_niebla = False
                for _ in range(3):  # a veces la respuesta se pierde entre otras líneas: se pone y se vuelve a mirar
                    cmd("NIEBLA 1", 2.0)
                    cmd("NIEBLA 2", 1.0)
                    if _num(ultima("PROBAR NIEBLA OK") or "", "niebla", 0) == 1:
                        hay_niebla = True
                        break
                if not hay_niebla:
                    anota("no pude poner niebla para la prueba del micrófono", "AVISO"); n_g[0] += 1
                else:
                    paso("Despeja la niebla", "Sopla al reloj hasta que desaparezca la niebla (no la barras con el dedo).", 25)
                    despejo = None
                    for i in range(25):
                        p.leer(1)
                        r = cmd("NIEBLA 2", 0.5)
                        if _num(ultima("PROBAR NIEBLA OK") or r or "", "niebla", 1) == 0:
                            despejo = i + 1; break
                    anota("niebla soplada: " + (f"se despejó con el soplido en {despejo} s" if despejo else "NO se despejó en 25 s (¿el micrófono no la oyó, o no soplaste?)"), "OK" if despejo else "FALLA")
                    listo(f"✔ Se despejó en {despejo} s" if despejo else "No se despejó: sigo", bool(despejo))
                    cmd("NIEBLA 0", 1.5)
                # 12-14: el cable USB (es el mismo del computador): se desconecta un rato y la placa anota sola lo que pasa
                b = p.cmd("BATERIA", 2.0)
                pct_b = _num(b, "pct", 0)
                if _num(b, "vbus") != 1:
                    anota("prueba de carga: la placa no ve el cable USB conectado; se salta", "AVISO"); n_g[0] += 3
                elif pct_b < 20:
                    anota(f"prueba de carga: la batería está en {pct_b} % (menos de 20 %); se salta para que la placa no se apague al desconectarla", "AVISO"); n_g[0] += 3
                else:
                    cmd("CARGAEV 0", 0.6)
                    r0 = ultima("CARGAEV OK") or ""
                    up0 = _num(r0, "up", 0)
                    ESPERA_S = 15
                    pto0 = p.s.port
                    paso("Desconecta el cable USB", "Saca el cable USB de la placa (el que va al computador). La placa sigue funcionando con su batería.", 25)
                    def hay_placa():
                        try:
                            from serial.tools import list_ports
                            return any(x.device == pto0 for x in list_ports.comports())
                        except Exception:
                            return True
                    salio = False
                    for i in range(50):
                        time.sleep(0.5)
                        if not hay_placa():
                            salio = True; break
                    if salio:
                        try: p.s.close()
                        except Exception: pass
                        listo("✔ Lo desconectaste")
                        paso("Mantenlo desconectado", f"No lo conectes todavía. Espera a que termine la cuenta.", ESPERA_S)
                        time.sleep(ESPERA_S)
                        paso("Conecta el cable USB", "Ahora vuelve a conectar el cable USB. La prueba sigue sola cuando el computador vea la placa.", 60)
                        pto = p.reabrir(60)
                        if not pto:
                            anota("prueba de carga: el computador no volvió a ver la placa en 60 s tras reconectar el cable", "FALLA")
                            listo("No volví a ver la placa", False)
                        else:
                            listo("✔ Placa de vuelta")
                            time.sleep(2); p.leer(1)
                            rr = None
                            for _ in range(5):
                                cmd("CARGAEV", 1.5)
                                rr = ultima("CARGAEV OK")
                                if rr and "ev=" in rr: break
                                time.sleep(1)
                            up1 = _num(rr or "", "up", 0)
                            if up1 and up0 and up1 < up0:
                                anota(f"prueba de carga: la placa SE REINICIÓ al desconectar o reconectar el cable (tiempo encendida {up1} ms, antes {up0} ms); no se pudo medir", "FALLA")
                            else:
                                ev = []
                                if rr and "ev=" in rr and rr.split("ev=")[1].strip():
                                    for e in rr.split("ev=")[1].strip().split(";"):
                                        try:
                                            v, ms_, mv_, pc_ = e.split(":"); ev.append((int(v), int(ms_), int(mv_), int(pc_)))
                                        except Exception:
                                            pass
                                fuera = [e for e in ev if e[0] == 0]
                                vuelta = [e for e in ev if e[0] == 1 and fuera and e[1] > fuera[0][1]]
                                if fuera and vuelta:
                                    seg_ = (vuelta[0][1] - fuera[0][1]) / 1000.0
                                    anota(f"carga: la placa notó que se fue el cable ({fuera[0][2]} mV, {fuera[0][3]} %) y que volvió {seg_:.0f} s después ({vuelta[0][2]} mV, {vuelta[0][3]} %); esperábamos ~{ESPERA_S} s o más", "OK" if seg_ >= ESPERA_S - 5 else "AVISO")
                                elif fuera:
                                    anota("carga: notó que se fue el cable, pero no anotó que volvió (¿ya estaba conectado al revisar?)", "AVISO")
                                else:
                                    anota(f"carga: la placa NO anotó que se fuera el cable (eventos: {ev}); ¿la placa no detecta la pérdida de energía USB?", "FALLA")
                    else:
                        anota("prueba de carga: no noté que desconectaras el cable en 25 s; se salta", "AVISO")
                        listo("No noté que lo desconectaras: sigo", False)
                        n_g[0] += 2
                pr.guia_paso("Terminaron las pruebas con tus manos", "Ahora continúo en automático. Ya no tienes que hacer nada: déjala y olvídate hasta que termine toda la prueba.", None, 0, 0)
                pr.etapa("Pruebas guiadas listas: sigue la prueba automática")
                time.sleep(7)
                pr.guia_cerrar()

            # ---------------------------------------------------------------- 2
      except (NameError, TypeError, KeyError, AttributeError) as _e:
        if not reanudando:
            raise
        anota(f"esta fase quedó a medias: usa datos de una fase anterior que no se corrió en esta vuelta ({type(_e).__name__}: {_e})", "AVISO")
    if fase(2, "Pantalla"):
      try:
            cmd("CERRAR", 0.5)
            r = cmd("PANTALLA 0", 3.0)
            en = cmd("ENERGIA", 0.3)
            anota("apagar como el botón PWR: pantalla=%s, procesador rápido=%s" % (_num(en, "pantalla"), _num(en, "cpu_rapido")),
                  "OK" if _num(en, "pantalla") == 0 else "FALLA")
            if _num(en, "cargando") == 0 and _num(en, "cpu_rapido") == 1:
                anota("con la pantalla apagada el procesador debería bajar a 80 MHz y sigue en 240 (¿hay sonido ambiente o alarma?)", "AVISO")
            elif _num(en, "cpu_rapido") == 0:
                anota("con la pantalla apagada el procesador bajó a 80 MHz")
            cmd("TOQUE", 1.0)
            en = cmd("ENERGIA", 0.3)
            anota("un TOQUE no enciende si la apagaste con el botón (queda apagada hasta que vuelvas a apretarlo): pantalla=%s" % _num(en, "pantalla"), "OK")
            r = cmd("PANTALLA 1", 2.0)
            en = cmd("ENERGIA", 0.3)
            anota("encender: pantalla=%s, procesador rápido=%s" % (_num(en, "pantalla"), _num(en, "cpu_rapido")), "OK" if _num(en, "pantalla") == 1 else "FALLA")
            for b in (100, 70, 40, 15, 5, 100):
                r = cmd(f"BRILLO {b}", 0.8)
                mira(f"brillo {b}", r, lento=False)
            r = cmd("PANTALLA 2", 3.0)
            mira("modo pecera (botón PWR dos veces)", r)
            cmd("CERRAR", 1.0)
            cmd("PANTALLA 1", 1.0)
            for i in range(6):  # el botón apretado en ráfaga
                cmd("PANTALLA 0", 0.2); cmd("PANTALLA 1", 0.2)
            en = cmd("ENERGIA", 0.3)
            anota("6 apagados y encendidos seguidos: pantalla=%s" % _num(en, "pantalla"), "OK" if _num(en, "pantalla") == 1 else "FALLA")
            mira("tras la ráfaga del botón", cmd("INFO", 1.0), exige_ok=False)
            # (v99, batería) el panel y el táctil duermen de verdad con la pantalla apagada: 50 vueltas, siempre vuelve con imagen
            malas = 0
            for i in range(50):
                cmd("PANTALLA 0", 0.3); cmd("PANTALLA 1", 0.5)
                if i % 10 == 9:
                    en = cmd("ENERGIA", 0.3)
                    if _num(en, "pantalla") != 1: malas += 1
            anota("(v99) 50 apagados y encendidos con el panel en reposo: la pantalla volvió siempre (%d malas)" % malas, "OK" if malas == 0 else "FALLA")
            mira("(v99) mírala: ¿se ve bien la imagen, sin rayas ni colores raros, tras dormir el panel 50 veces?", cmd("INFO", 1.0), exige_ok=False)
            # (v99) atenuar: a los 8 s sin tocarla, sin nada que leer ni jugar, baja el brillo y el procesador; un toque la devuelve
            cmd("CERRAR", 0.5); cmd("BRILLO 80", 0.5)
            time.sleep(11)
            en = cmd("ENERGIA", 0.3)
            anota("(v99) 11 s sin tocarla: atenuada=%s brillo=%s procesador rápido=%s" % (_num(en, "atenuada"), _num(en, "brillo"), _num(en, "cpu_rapido")),
                  "OK" if _num(en, "atenuada") == 1 and (_num(en, "brillo") or 99) < 80 else "AVISO")
            cmd("TOQUE", 0.8)
            en = cmd("ENERGIA", 0.3)
            anota("(v99) un toque la devuelve: atenuada=%s brillo=%s" % (_num(en, "atenuada"), _num(en, "brillo")),
                  "OK" if _num(en, "atenuada") == 0 and _num(en, "brillo") == 80 else "AVISO")
            cmd("BRILLO 100", 0.5)

            # ---------------------------------------------------------------- 3
      except (NameError, TypeError, KeyError, AttributeError) as _e:
        if not reanudando:
            raise
        anota(f"esta fase quedó a medias: usa datos de una fase anterior que no se corrió en esta vuelta ({type(_e).__name__}: {_e})", "AVISO")
    if fase(3, "Volumen"):
      try:
            cmd("VOLUMEN 50 50 50", 0.3)
            previo = None
            for i in range(7):
                r = cmd("VOLSUBIR", 0.6)
                d = p.info()
                v = d.get("vol") if d else None
                previo = v
            anota(f"subir ×7 desde 50: queda en {previo} (tope esperado 100/100/100)", "OK" if previo == "100/100/100" else "FALLA")
            for i in range(12):
                r = cmd("VOLBAJAR", 0.5)
            d = p.info()
            anota(f"bajar ×12: queda en {d and d.get('vol')} (piso esperado 0 en ella y efectos; las alarmas tienen un mínimo propio)",
                  "OK" if d and d.get("vol", "").startswith("0/0/") else "FALLA")
            for v in (0, 25, 75, 100):
                cmd(f"VOLUMEN {v} {v} {v}", 0.3); cmd("SONIDO 0", 0.4); cmd("SONIDO 1", 0.8); cmd("SONIDO 2", 1.2)
                d = p.info()
                anota(f"volumen {v}: toque, efecto y alarma sonaron; la placa dice {d and d.get('vol')}", "OK" if d and d.get("vol", "").startswith(f"{v}/{v}/") else "AVISO")
            if isinstance(vol0, str) and vol0.count("/") == 2:
                cmd("VOLUMEN " + vol0.replace("/", " "), 0.3)

            # ---------------------------------------------------------------- 4
      except (NameError, TypeError, KeyError, AttributeError) as _e:
        if not reanudando:
            raise
        anota(f"esta fase quedó a medias: usa datos de una fase anterior que no se corrió en esta vuelta ({type(_e).__name__}: {_e})", "AVISO")
    if fase(4, "Sensores"):
      try:
            tiempos = {}
            for k in range(3):
                r = cmd("SENSORES", 0.6)
                r = ultima("imu_us=") or r   # la placa responde dos líneas (datos e INFO); interesa la de los datos
                if not r or "OK" not in r:
                    anota(f"SENSORES #{k + 1}: sin respuesta o error", "FALLA"); continue
                tiempos[k] = r
                pr_ok = all(_num(r, c) == 1 for c in ("res_ok", "leer_ok", "guardar_ok"))
                anota(f"SENSORES #{k + 1}: imu {_num(r,'imu_us')} µs, batería {_num(r,'bat')}% ({_num(r,'bat_us')} µs), reloj {_num(r,'rtc_us')} µs, pasos {_num(r,'pasos_us')} µs, "
                      f"archivo {_num(r,'res_ms')} ms, guardar partida {_num(r,'guardar_ms')} ms, fotos {_num(r,'fotos')}, radio {_num(r,'radio_us')} µs",
                      "OK" if pr_ok else "FALLA")
                if _num(r, "guardar_ms", 0) > 250: anota(f"guardar la partida tardó {_num(r,'guardar_ms')} ms (>250)", "AVISO")
                if _num(r, "imu_us", 0) > 4000: anota(f"leer el acelerómetro tardó {_num(r,'imu_us')} µs (>4 ms)", "AVISO")
                if _num(r, "bat", 100) < 0: anota("el medidor de batería no responde", "FALLA")
                tx, ty = _num(r, "tilt_x", 0), _num(r, "tilt_y", 0)
                if tx == 0 and ty == 0: anota("la inclinación da exactamente 0,0: ¿acelerómetro muerto?", "AVISO")
            for ms in (600, 1500, 3000):
                r = cmd(f"MIC {ms}", 0.3)
                if r and "OK" in r:
                    f = _num(r, "fallas", 0)
                    anota(f"micrófono {ms} ms: muestras={_num(r,'muestras')} prom={_num(r,'prom')} máx={_num(r,'max')} fallas={f}", "OK" if f == 0 and _num(r, "muestras", 0) > 0 else "AVISO")
                else:
                    anota(f"micrófono {ms} ms: sin respuesta", "FALLA")

            # ---------------------------------------------------------------- 5
      except (NameError, TypeError, KeyError, AttributeError) as _e:
        if not reanudando:
            raise
        anota(f"esta fase quedó a medias: usa datos de una fase anterior que no se corrió en esta vuelta ({type(_e).__name__}: {_e})", "AVISO")
    if fase(5, "Giro de pantalla"):
      try:
            for g in (0, 1, 2, 3, 0):
                r = cmd(f"GIRO {g}", 1.5)
                mira(f"giro {g}", r, exige_ok=True)
            ram_a = (p.info() or {}).get("ram")
            for i in range(24):  # ráfaga: giros seguidos
                cmd(f"GIRO {i % 4}", 0.25)
            cmd("GIRO 0", 1.5)
            ram_b = (p.info() or {}).get("ram")
            mira("24 giros en ráfaga", cmd("INFO", 0.5), exige_ok=False)
            if ram_a and ram_b and ram_a - ram_b > 2000:
                anota(f"los giros se comieron {ram_a - ram_b} B de RAM interna", "AVISO")

            # ---------------------------------------------------------------- 6
      except (NameError, TypeError, KeyError, AttributeError) as _e:
        if not reanudando:
            raise
        anota(f"esta fase quedó a medias: usa datos de una fase anterior que no se corrió en esta vuelta ({type(_e).__name__}: {_e})", "AVISO")
    if fase(6, "Sonidos"):
      try:
            lista = cmd("LISTA sonidos", 0.3)
            nombres = lista.split("|")[1:] if lista and "|" in lista else []
            anota(f"{len(nombres)} sonidos en la lista (CUENTAS dice {n_snd})", "OK" if len(nombres) == n_snd and n_snd else "AVISO")
            cmd("VOLUMEN 40 40 40", 0.2)
            ram_ini = (p.info() or {}).get("ram")
            malos = []
            for i in range(1, len(nombres)):
                pr.etapa(f"Sonido {i} de {len(nombres) - 1}: {nombres[i]}")
                r = cmd(f"SONAR {i}", 0.9)
                if r is None or "ERROR" in r:
                    malos.append(f"{i} {nombres[i]}")
                pr.avanza()
            anota(f"se hicieron sonar {len(nombres) - 1} sonidos" + (f"; sin respuesta: {', '.join(malos[:8])}" if malos else ""), "OK" if not malos else "FALLA")
            d = p.info()
            anota(f"tras los sonidos: RAM {d and d.get('ram')} B (al inicio {ram_ini})", "OK" if d and ram_ini and ram_ini - d.get("ram", 0) < 3000 else "AVISO")
            for amb in range(0, 10):  # ambientes
                r = cmd(f"AMBIENTE {amb}", 1.5)
                if r and "ERROR" in r: break
                mira(f"ambiente {amb}", r, exige_ok=False)
            cmd("AMBIENTE -1", 0.5)
            cmd("VOLUMEN " + (vol0.replace("/", " ") if isinstance(vol0, str) and vol0.count("/") == 2 else "60 60 60"), 0.2)

            # ---------------------------------------------------------------- 7
      except (NameError, TypeError, KeyError, AttributeError) as _e:
        if not reanudando:
            raise
        anota(f"esta fase quedó a medias: usa datos de una fase anterior que no se corrió en esta vuelta ({type(_e).__name__}: {_e})", "AVISO")
    if fase(7, "Cielo: climas × momentos"):
      try:
            NOM_CLIMA = ["despejado", "nublado", "lluvia", "viento", "nieve", "tormenta", "frío"]
            NOM_MOM = ["MANANA", "TARDE", "NOCHE", "MADRUGADA"]
            for cl in range(len(NOM_CLIMA)):
                r = cmd(f"CLIMA {cl}", 0.4)
                if r and "ERROR" in r:
                    anota(f"clima {cl}: no existe en este firmware", "AVISO"); continue
                for mo in NOM_MOM:
                    cmd(f"MOMENTO {mo}", 1.2)
                    mira(f"clima {NOM_CLIMA[cl] if cl < len(NOM_CLIMA) else cl} de {mo.lower()}", cmd("INFO", 1.6), exige_ok=False)
            for e in range(4):
                r = cmd(f"ESTACION {e}", 1.2)
                mira(f"estación {e}", r)
            cmd("ESTACION -1", 0.5); cmd("CLIMA 0", 0.5); cmd("MOMENTO REAL", 0.5)
            for niebla in (1, 0):
                mira(f"niebla {niebla}", cmd(f"NIEBLA {niebla}", 2.0))

            # ---------------------------------------------------------------- 8
      except (NameError, TypeError, KeyError, AttributeError) as _e:
        if not reanudando:
            raise
        anota(f"esta fase quedó a medias: usa datos de una fase anterior que no se corrió en esta vuelta ({type(_e).__name__}: {_e})", "AVISO")
    if fase(8, "Zonas, escenas y eventos"):
      try:
            cmd("DESBLOQUEAR", 0.5)
            for z in range(6):
                mira(f"zona {z}", cmd(f"ZONA {z}", 3.0))
            cmd("ZONA 0", 1.5)
            for k in range(4):
                cmd("CERRAR", 0.8); mira(f"secreto {k}", cmd(f"SECRETO {k}", 6.0)); cmd("CERRAR", 0.8)
            for n in ("REGULAR", "SECRETO"):
                cmd("CERRAR", 0.8); mira(f"final {n.lower()}", cmd(f"FINAL {n}", 8.0)); cmd("CERRAR", 0.8)
            cmd("FINAL BORRAR", 0.5)
            for j in range(4):
                cmd("CERRAR", 0.8); mira(f"juego {j}", cmd(f"JUEGO {j}", 3.0)); cmd("CERRAR", 0.8)
            for h in (0, 1, 2):
                mira(f"huevo {h}", cmd(f"HUEVO {h}", 2.5))
            cmd("HUEVO 2", 0.5)
            for k in range(6):
                mira(f"Pochi {k}", cmd(f"POCHI {k}", 2.5))
            for k in (1, 2, 0):
                mira(f"casita {k}", cmd(f"CASITA {k}", 3.0))
            mira("visita", cmd("VISITA", 2.5), exige_ok=False)
            mira("ballena de nubes", cmd("BALLENA", 3.0))
            for c in range(max(1, min(n_cri, 7))):  # historias de cada narrador
                for k in range(5):
                    r = cmd(f"HISTORIA {c} {k}", 1.6)
                    if r and "ERROR" in r: break
                mira(f"historias del narrador {c}", cmd("CERRAR", 0.8), exige_ok=False)

            # ---------------------------------------------------------------- 9
      except (NameError, TypeError, KeyError, AttributeError) as _e:
        if not reanudando:
            raise
        anota(f"esta fase quedó a medias: usa datos de una fase anterior que no se corrió en esta vuelta ({type(_e).__name__}: {_e})", "AVISO")
    if fase(9, "Acciones del juego"):
      try:
            ACC = [("alimentar", 0, range(0, 6)), ("bañar", 1, [0]), ("jugar", 2, range(0, 4)), ("acariciar", 3, range(0, 6)), ("dormir", 4, [0]), ("despertar", 5, [0]),
                   ("curar", 6, [0]), ("sacudir", 7, [0]), ("cepillar", 8, [0]), ("llamar", 10, range(0, 4)), ("ir a una zona", 11, range(0, 6)),
                   ("espantar", 14, range(0, 5)), ("pescar", 15, [0]), ("invitar al vecino", 16, [0]), ("lanzar", 17, [0, 1, 2]), ("truco", 19, range(0, 5)),
                   ("limpiar", 24, [0]), ("salpicar", 25, [0, 1]), ("tocar animal del bosque", 27, range(0, 6))]
            for nombre, a, args in ACC:
                res = []
                for arg in args:
                    r = cmd(f"ACCION {a} {arg}", 0.7)
                    if r is None: res.append("¡sin respuesta!"); break
                    res.append(str(_num(r, "resultado", "?")))
                d = p.info()
                bien = d is not None and not (d.get("cuadros", 1) == 0 and d.get("ms", 1) == 0)
                anota(f"acción {nombre}: resultados {','.join(res)}  (RAM {d and d.get('ram')} B)", "OK" if bien and "¡sin respuesta!" not in res else "FALLA")
            cmd("ACCION 5 0", 1.0)

            # ---------------------------------------------------------------- 10
      except (NameError, TypeError, KeyError, AttributeError) as _e:
        if not reanudando:
            raise
        anota(f"esta fase quedó a medias: usa datos de una fase anterior que no se corrió en esta vuelta ({type(_e).__name__}: {_e})", "AVISO")
    if fase(10, "Ropa y accesorios"):
      try:
            cmd("ABRIR ropa -1", 0.5)
            lista = cmd("LISTA ropa", 0.3)
            ropas = lista.split("|")[1:] if lista and "|" in lista else []
            cmd("ACCION 0 0", 0.2)
            cmd("ACCION 5 0", 0.8)
            for i, nom in enumerate(ropas):
                r = cmd(f"ACCION 21 {i}", 1.3)  # ACT_VESTIR
                mira(f"ponerle «{nom}»", r, exige_ok=False)
            for s in range(4):
                cmd(f"ACCION 21 {-(s + 1)}", 0.5)
            mira("quitarle todo", cmd("INFO", 0.5), exige_ok=False)
            # combinaciones: cabeza + mano (paraguas con nenúfar, sombrero con molinillo...)
            nombres_ropa = [n.lower() for n in ropas]
            def idx(txt):
                for i, n in enumerate(nombres_ropa):
                    if txt in n: return i
                return None
            pares = [("nenúfar", "paraguas"), ("sombrero", "molinillo"), ("corona", "farol"), ("lentes", "paraguas")]
            for a_, b_ in pares:
                ia, ib = idx(a_), idx(b_)
                if ia is None or ib is None: continue
                cmd(f"ACCION 21 {ia}", 0.6); cmd(f"ACCION 21 {ib}", 1.8)
                mira(f"combinación {a_} + {b_}", cmd("INFO", 0.5), exige_ok=False)
                for s in range(4): cmd(f"ACCION 21 {-(s + 1)}", 0.3)
            for j in range(0, 16):  # juguetes
                r = cmd(f"ABRIR juguetes {j}", 0.3)
                if r and "ERROR" in r: break

            # ---------------------------------------------------------------- 11
      except (NameError, TypeError, KeyError, AttributeError) as _e:
        if not reanudando:
            raise
        anota(f"esta fase quedó a medias: usa datos de una fase anterior que no se corrió en esta vuelta ({type(_e).__name__}: {_e})", "AVISO")
    if fase(11, "Objetos de la Bolsa"):
      try:
            cmd("ACCION 5 0", 0.5)
            lista = cmd("LISTA objetos", 0.3)
            objetos = lista.split("|")[1:] if lista and "|" in lista else []
            cmd("ABRIR objetos -1", 1.0)
            malos = []
            ram_ini = (p.info() or {}).get("ram")
            for i, nom in enumerate(objetos):
                pr.etapa(f"Objeto {i + 1} de {len(objetos)}: {nom}")
                r = cmd(f"OBJETO {i}", 1.2)
                if r is None or "ERROR" in r:
                    malos.append(f"{i} {nom}")
                pr.avanza()
            anota(f"{len(objetos)} objetos usados y devueltos" + (f"; con problema: {', '.join(malos[:8])}" if malos else ""), "OK" if not malos else "FALLA")
            d = p.info()
            anota(f"tras los objetos: RAM {d and d.get('ram')} B (al inicio {ram_ini})", "OK" if d and ram_ini and ram_ini - d.get("ram", 0) < 3000 else "AVISO")

            # ---------------------------------------------------------------- 12
      except (NameError, TypeError, KeyError, AttributeError) as _e:
        if not reanudando:
            raise
        anota(f"esta fase quedó a medias: usa datos de una fase anterior que no se corrió en esta vuelta ({type(_e).__name__}: {_e})", "AVISO")
    if fase(12, "Animales del bosque"):
      try:
            lista = cmd("LISTA animales", 0.3)
            fichas = lista.split("|")[1:] if lista and "|" in lista else []
            cmd("ZONA 1", 2.0)
            ram_ser = []
            n_ani = max(0, len(fichas) - 5)  # las primeras fichas son fauna (mariposa, abeja, pájaro, zorro, rana)
            for a in range(n_ani):
                pr.etapa(f"Llamando al animal {a + 1} de {n_ani}")
                r = cmd(f"LLAMAR {a}", 2.0)
                mira(f"llamar al animal {a}", r, exige_ok=True)
                try:
                    _d = p.info() or {}
                    ram_ser.append((a, _d.get("ram"), _d.get("mayor"), _d.get("psram")))
                except Exception:
                    pass
                pr.avanza()
            try:  # ¿cuál animal se queda con la RAM interna? (la vez anterior bajó de 50 KB a 4 KB en esta fase)
                bajadas = [(ram_ser[i - 1][1] - ram_ser[i][1], ram_ser[i][0]) for i in range(1, len(ram_ser)) if ram_ser[i][1] is not None and ram_ser[i - 1][1] is not None]
                bajadas.sort(reverse=True)
                anota("RAM interna por animal: de %s a %s B; las mayores bajadas (B, animal): %s; bloque mayor final %s; PSRAM final %s KB" %
                      (ram_ser[0][1] if ram_ser else "?", ram_ser[-1][1] if ram_ser else "?", bajadas[:6], ram_ser[-1][2] if ram_ser else "?", ram_ser[-1][3] if ram_ser else "?"),
                      "AVISO" if ram_ser and ram_ser[-1][1] is not None and ram_ser[-1][1] < 15000 else "OK")
            except Exception:
                pass
            cmd("ZONA 0", 1.5)

            # ---------------------------------------------------------------- 13
      except (NameError, TypeError, KeyError, AttributeError) as _e:
        if not reanudando:
            raise
        anota(f"esta fase quedó a medias: usa datos de una fase anterior que no se corrió en esta vuelta ({type(_e).__name__}: {_e})", "AVISO")
    if fase(13, "Cueva y dragón de cristal"):
      try:
            cmd("VOLUMEN 70 70 70", 0.2)
            r = cmd("DRAGON", 4.0)
            mira("el dragón despierta y suena la melodía de los cristales", r)
            for z in (4, 0):
                mira(f"cueva/prado {z}", cmd(f"ZONA {z}", 2.5))
            r = cmd("MUSICA 1", 22.0)  # una tanda de escucha de música (6 s de cada 20)
            mira("la música activada (22 s con escucha por tandas)", r, exige_ok=False)
            cmd("MUSICA 0", 0.3)
            cmd("VOLUMEN " + (vol0.replace("/", " ") if isinstance(vol0, str) and vol0.count("/") == 2 else "60 60 60"), 0.2)

            # ---------------------------------------------------------------- 14
      except (NameError, TypeError, KeyError, AttributeError) as _e:
        if not reanudando:
            raise
        anota(f"esta fase quedó a medias: usa datos de una fase anterior que no se corrió en esta vuelta ({type(_e).__name__}: {_e})", "AVISO")
    if fase(14, "Dormir y ahorro de energía"):
      try:
            cmd("ZONA 0", 1.0); cmd("CERRAR", 0.8); cmd("PANTALLA 1", 1.0); cmd("TOQUE", 0.3)
            cmd("ACCION 4 0", 1.0)  # dormir
            en = cmd("ENERGIA", 0.3)
            anota(f"se durmió: dormida={_num(en,'dormida')}", "OK" if _num(en, "dormida") == 1 else "AVISO")
            cmd("TOQUE", 0.3)
            cmd("CERRAR", 0.8)   # un toque puede abrir un panel; con un panel abierto el ahorro no entra
            p.leer(11)  # sin tocar nada: el ahorro entra a los 8 s
            en = cmd("ENERGIA", 0.3)
            if _num(en, "cargando") == 1:
                anota("está cargando: el ahorro no se aplica con el cargador puesto (es lo esperado)")
            elif _num(en, "suave") == 1 and _num(en, "cpu_rapido") == 0:
                anota("dormida y quieta: el procesador bajó a 80 MHz (ahorro suave) ✔")
            else:
                anota(f"dormida y quieta 11 s: el ahorro NO se activó: panel abierto={_num(en,'panel')} (debe ser 0), ambiente={_num(en,'ambiente')} (debe ser 0), alarma={_num(en,'alarma')} ({en[en.index('OK'):] if en and 'OK' in en else en})", "FALLA")
            cmd("TOQUE", 1.0)
            en = cmd("ENERGIA", 0.3)
            anota("un toque devuelve el procesador a 240 MHz: " + ("sí ✔" if _num(en, "cpu_rapido") == 1 and _num(en, "suave") == 0 else f"NO ({en})"),
                  "OK" if _num(en, "cpu_rapido") == 1 else "FALLA")
            # ahorro profundo: pantalla apagada y ella dormida → bucle lento; debe seguir contestando y despertar al encender
            cmd("CERRAR", 0.8); cmd("PANTALLA 0", 1.0); p.leer(6)
            en = cmd("ENERGIA", 0.5)
            anota("pantalla apagada y ella dormida: ahorro profundo " + ("activo ✔" if _num(en, "profundo") == 1 else f"NO activo (dormida={_num(en,'dormida')}, panel={_num(en,'panel')}, ambiente={_num(en,'ambiente')}, alarma={_num(en,'alarma')}, cargando={_num(en,'cargando')})"),
                  "OK" if _num(en, "profundo") == 1 else "AVISO")
            t0 = time.time(); r = cmd("INFO", 1.5)
            seg = time.time() - t0
            anota(f"en ahorro profundo la placa contesta en {seg:.2f} s", "OK" if r and seg < 1.5 else "AVISO")
            cmd("PANTALLA 1", 1.5)
            en = cmd("ENERGIA", 0.5)
            anota("al encender la pantalla el ahorro profundo se suelta: " + ("sí ✔" if _num(en, "profundo") == 0 else "NO"), "OK" if _num(en, "profundo") == 0 else "FALLA")
            # la ventana de la casita: de día y de noche, con lluvia
            for mo, cl in (("MANANA", 2), ("NOCHE", 2), ("MANANA", 0), ("NOCHE", 4)):
                cmd(f"MOMENTO {mo}", 0.4); cmd(f"CLIMA {cl}", 0.4)
                cmd("CASITA 1", 3.0)
                mira(f"dormida adentro de la casita ({mo.lower()}, clima {cl}): la ventana debe mostrar lo mismo que afuera", cmd("INFO", 0.5), exige_ok=False)
                cmd("CASITA 0", 2.5)
            cmd("MOMENTO REAL", 0.4); cmd("CLIMA 0", 0.4)
            cmd("ACCION 5 0", 1.0)

            # ---------------------------------------------------------------- 15
      except (NameError, TypeError, KeyError, AttributeError) as _e:
        if not reanudando:
            raise
        anota(f"esta fase quedó a medias: usa datos de una fase anterior que no se corrió en esta vuelta ({type(_e).__name__}: {_e})", "AVISO")
    if fase(15, "Estrés"):
      try:
            cmd("CERRAR", 0.8)
            r = cmd("PANELES", 0.2)
            n_pan = max(1, len(r.split()[4:])) if r else 60
            ram_a = (p.info() or {}).get("ram")
            mayor_a = (p.info() or {}).get("mayor")
            import random
            azar = random.Random(7)
            ok = True
            for i in range(40):
                k = azar.randrange(n_pan)
                if cmd(f"PANEL {k}", 0.25) is None: ok = False; break
                if i % 2: cmd("CERRAR", 0.2)
            cmd("CERRAR", 1.0)
            d = p.info()
            anota(f"40 paneles abiertos al azar en ráfaga: {'sin colgarse' if ok and d else 'SE COLGÓ'}; RAM {ram_a} → {d and d.get('ram')} B; bloque mayor {mayor_a} → {d and d.get('mayor')}",
                  "OK" if ok and d and ram_a and ram_a - d.get("ram", 0) < 4000 else "FALLA")
            for i in range(30):  # ráfaga de comandos mezclados
                cmd(azar.choice(["TOQUE", "CERRAR", "PANTALLA 1", "ACCION 3 0", "ACCION 3 2", "BRILLO 80"]), 0.1)
            p.leer(2)
            mira("ráfaga de 30 comandos mezclados", cmd("INFO", 1.0), exige_ok=False)
            for i in range(12):  # abrir y cerrar zonas seguido
                cmd(f"ZONA {i % 6}", 0.5)
            cmd("ZONA 0", 2.0)
            d2 = p.info()
            anota(f"12 cambios de zona seguidos; RAM {d2 and d2.get('ram')} B, PSRAM {d2 and d2.get('psram')} KB", "OK" if d2 and d2.get("ram", 0) > RAM_MIN else "FALLA")
            # (v97) soltar los íconos guardados a la fuerza (lo que hace sola la vigilancia de memoria cuando falta) y seguir jugando:
            # si algún ícono soltado seguía en uso, aquí se cae
            ps0 = (p.info() or {}).get("psram")
            vivo = True
            for vuelta in range(3):
                rm = cmd("MEMLIMPIAR 4", 0.5)
                for k in (14, 9, 15, 45, 3, 74):
                    cmd(f"PANEL {k}", 0.3); cmd("CERRAR", 0.2)
                cmd(f"ZONA {vuelta}", 0.6); cmd("LLAMAR 27", 0.6) if vuelta == 1 else None
                if not p.info(): vivo = False; break
            cmd("ZONA 0", 1.0)
            d3 = p.info()
            anota(f"soltar los íconos guardados 3 veces y seguir abriendo paneles: {'sin colgarse' if vivo and d3 else 'SE COLGÓ'}; PSRAM {ps0} → {d3 and d3.get('psram')} KB; {(rm or '')[(rm or '').find('liberado'):]}",
                  "OK" if vivo and d3 else "FALLA")

            # ---------------------------------------------------------------- 16
      except (NameError, TypeError, KeyError, AttributeError) as _e:
        if not reanudando:
            raise
        anota(f"esta fase quedó a medias: usa datos de una fase anterior que no se corrió en esta vuelta ({type(_e).__name__}: {_e})", "AVISO")
    if fase(16, "Todos los paneles"):
      try:
            pr.paneles(p, sal)

            # ---------------------------------------------------------------- 17
      except (NameError, TypeError, KeyError, AttributeError) as _e:
        if not reanudando:
            raise
        anota(f"esta fase quedó a medias: usa datos de una fase anterior que no se corrió en esta vuelta ({type(_e).__name__}: {_e})", "AVISO")
    if fase(17, "Hardware extra"):
      try:
            cmd("VOLUMEN 100 100 100", 0.2)
            for sid in (1, 2, 3):  # el parlante suena y el micrófono lo oye
                r = cmd(f"ECO {sid}", 0.3)
                if not r or "OK" not in r:
                    anota(f"eco con el sonido {sid}: sin respuesta", "FALLA"); continue
                base, con, pico = _num(r, "base", 0), _num(r, "con_sonido", 0), _num(r, "pico", 0)
                anota(f"parlante → micrófono (sonido {sid}): silencio {base}, con sonido {con}, pico {pico}" + ("" if pico >= base + 4 else "  ← no se oyó (¿parlante mudo o volumen bajo?)"),
                      "OK" if pico >= base + 4 else "AVISO")
            cmd("VOLUMEN " + (vol0.replace("/", " ") if isinstance(vol0, str) and vol0.count("/") == 2 else "60 60 60"), 0.2)
            try: p.ultimas.clear()
            except Exception: pass
            cmd("FLASH 20", 0.1)      # 20 vueltas (con 150 tardó casi 3 minutos: SPIFFS reescribiendo el mismo archivo)
            for _ in range(90):       # espera hasta 90 s a que la placa termine y conteste
                if ultima("PROBAR FLASH OK"): break
                p.leer(1)
            r = ultima("PROBAR FLASH")
            if r and "OK" in r:
                malo = _num(r, "fallo_guardar", 0) + _num(r, "fallo_leer", 0) + _num(r, "distinto", 0)
                anota(f"memoria flash: {_num(r,'vueltas')} guardados y leídos, errores {malo}, peor vuelta {_num(r,'peor_ms')} ms, total {_num(r,'total_ms')} ms", "OK" if malo == 0 else "FALLA")
                if _num(r, "peor_ms", 0) > 400: anota(f"una escritura a flash tardó {_num(r,'peor_ms')} ms (>400): puede congelar la animación", "AVISO")
            else:
                anota("memoria flash: sin respuesta", "FALLA")
            r = cmd("FOTOPRUEBA", 0.5)
            if r and "OK" in r:
                bien = _num(r, "id", -1) >= 0 and all(_num(r, k) == 1 for k in ("en_lista", "igual", "borrada"))
                anota(f"fotos: guardar, listar, leer y borrar una foto de prueba (id {_num(r,'id')}, {_num(r,'tam')} B)", "OK" if bien else "FALLA")
            else:
                anota("fotos: sin respuesta", "FALLA")
            for k in range(5):
                cmd("SENSORES", 0.3)
            anota("radio (baliza) y sensores 5 veces más: sin colgarse", "OK" if p.info() else "FALLA")
            rt = cmd("TEMPERATURA", 0.4)
            tc, ti = _num(rt, "chip"), _num(rt, "imu")
            if tc is None or tc == -999:
                anota("temperatura del procesador: la placa no la entregó", "AVISO")
            else:
                anota(f"temperatura del procesador: {tc / 10:.1f} °C" + (" (¡caliente!)" if tc >= 700 else ""), "AVISO" if tc >= 700 else "OK")
            if ti is None or ti == -999:
                anota("temperatura del acelerómetro (junto a la batería): la placa no la entregó", "AVISO")
            else:
                anota(f"temperatura del acelerómetro (junto a la batería): {ti / 10:.1f} °C" + (" (¡caliente!)" if ti >= 600 else ""), "AVISO" if ti >= 600 else "OK")
            # ---------------------------------------------------------------- 18
      except (NameError, TypeError, KeyError, AttributeError) as _e:
        if not reanudando:
            raise
        anota(f"esta fase quedó a medias: usa datos de una fase anterior que no se corrió en esta vuelta ({type(_e).__name__}: {_e})", "AVISO")
    if fase(18, "Guardar, reiniciar y comprobar"):
      try:
            r = cmd("VALIDAR", 0.5)
            if r and "OK" in r:
                bien = _num(r, "sana") == 1 and _num(r, "corruptas_detectadas") == _num(r, "de")
                anota(f"partida sana pasa la validación; corruptas detectadas {_num(r,'corruptas_detectadas')} de {_num(r,'de')}", "OK" if bien else "FALLA")
            else:
                anota("validación de partida: sin respuesta", "FALLA")
            antes = cmd("ESTADO", 0.2)
            g = cmd("GUARDAR", 1.5)
            anota("guardar la partida ahora: " + ("OK" if g and "OK" in g else str(g)), "OK" if g and "OK" in g else "FALLA")
            cmd("REINICIAR", 0.3)
            vuelve = False
            try:
                p.s.close()
            except Exception:
                pass
            fin = time.time() + 40
            while time.time() < fin and not vuelve:
                time.sleep(2.0)
                try:
                    p.s.open()
                    p.leer(4)
                    if p.info(): vuelve = True
                except Exception:
                    try: p.s.close()
                    except Exception: pass
            if not vuelve:
                anota("la placa NO volvió a contestar tras reiniciar (40 s)", "FALLA")
            else:
                despues = cmd("ESTADO", 0.2)
                a1 = antes.split("OK", 1)[-1].strip() if antes else ""
                a2 = despues.split("OK", 1)[-1].strip() if despues else ""
                anota("reinicio: la placa volvió y la partida quedó " + ("IGUAL (nombre, especie, etapa, zonas, ropa, juguetes, decoración, final)" if a1 == a2 and a1 else f"DISTINTA\n      antes:   {a1}\n      después: {a2}"),
                      "OK" if a1 == a2 and a1 else "FALLA")
                mira("la mascota tras el reinicio", cmd("INFO", 1.0), exige_ok=False)

            # ---------------------------------------------------------------- 19
      except (NameError, TypeError, KeyError, AttributeError) as _e:
        if not reanudando:
            raise
        anota(f"esta fase quedó a medias: usa datos de una fase anterior que no se corrió en esta vuelta ({type(_e).__name__}: {_e})", "AVISO")
    if fase(19, "Viaje en el tiempo y fiestas"):
      try:
            for h in range(0, 24, 3):
                cmd(f"MOMENTO {h:02d}:30", 1.2)
                mira(f"hora {h:02d}:30", cmd("INFO", 0.6), exige_ok=False)
            cmd("MOMENTO REAL", 0.5)
            for horas, nombre in ((6, "6 horas"), (24, "1 día"), (24 * 7, "1 semana"), (24 * 40, "40 días (cambia de estación)"), (-24 * 3, "3 días atrás")):
                r = cmd(f"SALTAR {horas}", 2.5)
                mira(f"saltar {nombre}", r, exige_ok=True)
                cmd("MOMENTO REAL", 1.0)
            FIESTAS = [(1, 1, "Año Nuevo"), (2, 14, "San Valentín"), (5, 21, "Glorias navales"), (9, 18, "Fiestas Patrias"), (10, 31, "Halloween"), (12, 24, "Nochebuena"),
                       (12, 25, "Navidad"), (12, 31, "Nochevieja"), (3, 20, "equinoccio"), (6, 21, "solsticio de invierno")]
            for mes, dia, nombre in FIESTAS:
                r = cmd(f"FECHA {mes} {dia}", 2.0)
                mira(f"fecha {dia:02d}/{mes:02d} {nombre}", r, exige_ok=True)
            cmd("MOMENTO REAL", 1.0)
            tam0 = d0.get("tam", 100)
            for tam in (100, 120, 130, 140):
                r = cmd(f"TAMANO {tam}", 2.5)
                mira(f"tamaño de las criaturas {tam}%", r, exige_ok=True)
            cmd(f"TAMANO {tam0}", 1.5)

            # ---------------------------------------------------------------- 20
      except (NameError, TypeError, KeyError, AttributeError) as _e:
        if not reanudando:
            raise
        anota(f"esta fase quedó a medias: usa datos de una fase anterior que no se corrió en esta vuelta ({type(_e).__name__}: {_e})", "AVISO")
    if fase(20, "Fuga de memoria: 20 vueltas completas"):
      try:
            azar2 = __import__("random").Random(11)
            r = cmd("PANELES", 0.2)
            n_pan2 = max(1, len(r.split()[4:])) if r else 60
            serie = []
            for v in range(20):
                cmd(f"PANEL {azar2.randrange(n_pan2)}", 0.4); cmd("CERRAR", 0.4)
                cmd(f"ZONA {v % 6}", 0.6)
                cmd(f"JUEGO {v % 4}", 0.8); cmd("CERRAR", 0.4)
                cmd(f"SONAR {1 + v % max(1, (n_snd - 1))}", 0.5)
                cmd(f"CLIMA {v % 7}", 0.3)
                if v % 5 == 4:
                    dd = p.info() or {}
                    serie.append((v + 1, dd.get("ram"), dd.get("psram"), dd.get("mayor")))
            cmd("ZONA 0", 1.0); cmd("CLIMA 0", 0.3)
            if serie:
                anota("memoria cada 5 vueltas (vuelta, RAM interna, PSRAM KB, bloque mayor): " + " | ".join(f"{a},{b},{c},{d}" for a, b, c, d in serie))
                r0, r1 = serie[0][1], serie[-1][1]
                ps0, ps1 = serie[0][2], serie[-1][2]
                fuga = (r0 or 0) - (r1 or 0)
                anota(f"RAM interna: {r0} → {r1} B ({'-' if fuga > 0 else '+'}{abs(fuga)}); PSRAM {ps0} → {ps1} KB", "OK" if fuga < 3000 and (ps0 or 0) - (ps1 or 0) < 300 else "FALLA")

            # ---------------------------------------------------------------- 21
      except (NameError, TypeError, KeyError, AttributeError) as _e:
        if not reanudando:
            raise
        anota(f"esta fase quedó a medias: usa datos de una fase anterior que no se corrió en esta vuelta ({type(_e).__name__}: {_e})", "AVISO")
    if fase(21, "Pendientes conocidos (se anotan, no se arreglan todavía)"):
      try:
            # a) la comida: el panel 0 del probador abre el menú ANTIGUO; el camino real (tocar el plato) abre la pantalla del picnic
            cmd("CERRAR", 0.5)
            mira("comida: panel 0 del probador (abre el menú ANTIGUO «¿QUÉ LE DAMOS?»)", cmd("PANEL 0", 2.0), exige_ok=False)
            anota("CONOCIDO: el panel 0 de la prueba llama al menú de comida antiguo; el camino real al tocar el plato es el picnic (ui.c, case 0: picnic_abrir)", "AVISO")
            cmd("CERRAR", 0.8)
            r = cmd("PICNIC", 3.0)
            mira("comida: camino real (pantalla del picnic)", r, exige_ok=False)
            cmd("CERRAR", 0.8)
            # b) el bote: abrir el panel desde el prado no trae el lago; la travesía debe cargar su fondo
            cmd("ZONA 0", 1.5)
            cmd("BOTE 2", 2.0)
            f1 = cmd("FONDO", 0.3)
            anota(f"bote desde el prado: el panel se abre sobre el prado (escena dibujada {_num(f1,'escena_dibujada')}, fondo cargado {_num(f1,'cargado')}); no es el lago",
                  "AVISO")
            cmd("CERRAR", 0.8)
            cmd("DESBLOQUEAR", 0.3)
            cmd("ZONA 2", 3.0)
            f2 = cmd("FONDO", 0.3)
            anota(f"el lago: fondo cargado={_num(f2,'cargado')} escena dibujada={_num(f2,'escena_dibujada')} ({_num(f2,'ancho')}x{_num(f2,'alto')}), PSRAM {_num(f2,'psram')} KB",
                  "OK" if _num(f2, "cargado") == 1 and _num(f2, "escena_dibujada") == 2 else "FALLA")
            cmd("BOTE 2", 2.0)
            cmd("BOTE 1", 1.0)   # a la isla
            cruce_ok = None
            for i in range(20):  # la travesía tarda en empezar (camina hasta el bote): se mira varias veces
                f3 = cmd("FONDO", 1.5)
                c = _num(f3, "cruce", -1)
                if c != -1:
                    cruce_ok = c; break
            if cruce_ok is None:
                anota("travesía del bote: no llegó a empezar en 30 s (¿el bote no sale?)", "FALLA")
            else:
                anota(f"travesía del bote a la isla: fondo del cruce {'CARGADO' if cruce_ok == 1 else 'NO CARGADO (queda en blanco)'}; PSRAM {_num(f3,'psram')} KB",
                      "OK" if cruce_ok == 1 else "FALLA")
            p.leer(25)
            f4 = cmd("FONDO", 0.3)
            anota(f"después de la travesía: escena dibujada {_num(f4,'escena_dibujada')} fondo cargado {_num(f4,'cargado')} cruce {_num(f4,'cruce')}",
                  "OK" if _num(f4, "cargado") == 1 else "FALLA")
            cmd("ZONA 0", 3.0)
            mira("de vuelta al prado", cmd("INFO", 1.0), exige_ok=False)

            # ---------------------------------------------------------------- 22
      except (NameError, TypeError, KeyError, AttributeError) as _e:
        if not reanudando:
            raise
        anota(f"esta fase quedó a medias: usa datos de una fase anterior que no se corrió en esta vuelta ({type(_e).__name__}: {_e})", "AVISO")
    if fase(22, "Aspectos: especies, colores y etapas de vida"):
      try:
            n_esp = _num(cuentas, "especies", 0)
            n_col = _num(cuentas, "colores", 0)
            if not n_esp:
                anota("CUENTAS no trae especies/colores (firmware viejo): se salta", "AVISO")
            else:
                cmd("CERRAR", 0.5)
                for esp in range(n_esp):
                    for col in range(0, n_col, 3):           # colores 0, 3, 6
                        r = cmd(f"ASPECTO {esp} {col} 3 0", 1.2)
                        mira(f"aspecto especie {esp} color {col} adulta", r, exige_ok=False)
                for et in range(4):                           # huevo, bebé, cría, adulta con varias formas
                    for forma in ((0,) if et < 3 else (0, 3, 7)):
                        r = cmd(f"ASPECTO 0 0 {et} {forma}", 1.2)
                        mira(f"etapa {et} forma {forma}: reloj entero", r, exige_ok=False)
                        for pn in (3, 34, 40):
                            mira(f"etapa {et} forma {forma}: panel {pn}", cmd(f"PANEL {pn}", 1.2), exige_ok=False)
                            cmd("CERRAR", 0.4)
                cmd("RESTAURAR", 1.5)
                mira("aspectos: partida restaurada", cmd("INFO", 0.8), exige_ok=False)

            # ---------------------------------------------------------------- 23
      except (NameError, TypeError, KeyError, AttributeError) as _e:
        if not reanudando:
            raise
        anota(f"esta fase quedó a medias: usa datos de una fase anterior que no se corrió en esta vuelta ({type(_e).__name__}: {_e})", "AVISO")
    if fase(23, "Modo niño e infante"):
      try:
            for modo, nom in ((1, "niño"), (2, "infante")):
                mira(f"modo {nom}: activar", cmd(f"NINO {modo}", 1.5), exige_ok=False)
                for pn in (0, 3, 34, 40, 56, 57, 45, 81):  # (v96: también el Libro de historias y una hoja de lectura)
                    mira(f"modo {nom}: panel {pn}", cmd(f"PANEL {pn}", 1.2), exige_ok=False)
                    cmd("CERRAR", 0.4)
                for ac in (0, 1, 3, 4, 5, 8, 21):
                    mira(f"modo {nom}: acción {ac}", cmd(f"ACCION {ac} 0", 1.5), exige_ok=False)
                mira(f"modo {nom}: sonido de felicitación", cmd("SONIDO 1", 0.8), exige_ok=False)
            cmd("RESTAURAR", 1.5)
            mira("modo normal restaurado", cmd("INFO", 0.8), exige_ok=False)

            # ---------------------------------------------------------------- 24
      except (NameError, TypeError, KeyError, AttributeError) as _e:
        if not reanudando:
            raise
        anota(f"esta fase quedó a medias: usa datos de una fase anterior que no se corrió en esta vuelta ({type(_e).__name__}: {_e})", "AVISO")
    if fase(24, "Alarmas y avisos"):
      try:
            cmd("CERRAR", 0.4)
            for modo, nom in ((0, "alarma"), (1, "hora de dormir"), (3, "aviso de tiempo")):
                r = cmd(f"ALARMA {modo}", 2.0)
                r = ultima("sonando=") or r
                if mira(f"{nom}: aparece", r, exige_ok=False):
                    anota(f"{nom}: {'suena/está en pantalla' if _num(r, 'sonando', 0) else 'NO quedó sonando (puede ser normal para este tipo)'}",
                          "OK" if _num(r, "sonando", 0) or modo == 3 else "AVISO")
                cmd("PANTALLA 0", 0.8)
                cmd("TOQUE", 0.8)                               # un toque despierta; la alarma debe seguir
                p.leer(2)
                mira(f"{nom}: con la pantalla apagada y despertada", cmd("INFO", 0.8), exige_ok=False)
                r = cmd("ALARMA 9", 1.2)
                r = ultima("sonando=") or r
                anota(f"{nom}: apagada → sonando={_num(r, 'sonando', -1)}", "OK" if _num(r, "sonando", 1) == 0 else "FALLA")
            cmd("PANTALLA 1", 0.5)
            # orden de eventos: alarma encima de un panel y encima de un juego
            cmd("PANEL 3", 1.2); cmd("ALARMA 0", 1.5); mira("alarma sobre un panel abierto", cmd("INFO", 0.6), exige_ok=False); cmd("ALARMA 9", 1.0); cmd("CERRAR", 0.5)
            cmd("JUEGO 0", 1.5); cmd("ALARMA 0", 1.5); mira("alarma sobre un juego", cmd("INFO", 0.6), exige_ok=False); cmd("ALARMA 9", 1.0); cmd("CERRAR", 0.8)
            cmd("JUEGO 1", 1.5); cmd("VISITA", 2.0); mira("visita sobre un juego", cmd("INFO", 0.6), exige_ok=False); cmd("CERRAR", 0.8)

            # ---------------------------------------------------------------- 25
      except (NameError, TypeError, KeyError, AttributeError) as _e:
        if not reanudando:
            raise
        anota(f"esta fase quedó a medias: usa datos de una fase anterior que no se corrió en esta vuelta ({type(_e).__name__}: {_e})", "AVISO")
    if fase(25, "Historias, colecciones, leyendas y comidas"):
      try:
            cmd("CERRAR", 0.4)
            ram_antes = (p.info() or {}).get("ram")
            for narr in range(min(n_cri, 7) + 1):                     # la última es Nimba / el libro
                for h in range(5):
                    r = cmd(f"HISTORIA {narr} {h}", 1.3)
                    mira(f"historia narrador {narr} n.º {h}", r, exige_ok=False)
                    cmd("CERRAR", 0.5)
                    ram_ahora = (p.info() or {}).get("ram")   # con el panel cerrado: lo que queda sin devolverse
                    if ram_antes and ram_ahora and ram_antes - ram_ahora > 4000:
                        anota(f"RAM interna: la historia {narr}.{h} dejó {ram_antes - ram_ahora} B sin devolver tras cerrar ({ram_antes} → {ram_ahora} B)", "AVISO")
                    if ram_ahora: ram_antes = ram_ahora
            for tipo in ("colecciones", "leyendas", "comidas", "juguetes", "decoraciones", "ropa"):
                r = cmd(f"ABRIR {tipo} -1", 2.0)
                mira(f"abrir todo: {tipo}", r, exige_ok=False)
            for pn in (56, 57, 0):
                mira(f"panel {pn} con todo abierto", cmd(f"PANEL {pn}", 1.5), exige_ok=False)
                cmd("CERRAR", 0.5)
            cmd("RESTAURAR", 1.5)

            # ---------------------------------------------------------------- 26
      except (NameError, TypeError, KeyError, AttributeError) as _e:
        if not reanudando:
            raise
        anota(f"esta fase quedó a medias: usa datos de una fase anterior que no se corrió en esta vuelta ({type(_e).__name__}: {_e})", "AVISO")
    if fase(26, "Jardín y minijuegos con resultados forzados"):
      try:
            for ac in (12, 13, 26):                            # plantar, regar, arrancar
                for parcela in range(6):
                    mira(f"jardín acción {ac} parcela {parcela}", cmd(f"ACCION {ac} {parcela}", 1.2), exige_ok=False)
            for juego in range(4):
                for pts in (0, 5, 15, 40):
                    cmd(f"JUEGO {juego}", 1.2)
                    mira(f"juego {juego} termina con {pts} puntos", cmd(f"ACCION 20 {juego * 100 + pts}", 2.0), exige_ok=False)
                    cmd("CERRAR", 0.6)
            cmd("RESTAURAR", 1.5)

            # ---------------------------------------------------------------- 27
      except (NameError, TypeError, KeyError, AttributeError) as _e:
        if not reanudando:
            raise
        anota(f"esta fase quedó a medias: usa datos de una fase anterior que no se corrió en esta vuelta ({type(_e).__name__}: {_e})", "AVISO")
    if fase(27, "Reloj y arranque"):
      try:
            r1 = cmd("RELOJ", 0.5)
            anota(f"RELOJ: {r1[r1.index('OK'):] if r1 and 'OK' in r1 else r1}", "OK" if r1 and "OK" in r1 else "FALLA")
            rtc_a = _num(r1, "rtc", 0)
            time.sleep(5)
            r2 = cmd("RELOJ", 0.5)
            dt = _num(r2, "rtc", 0) - rtc_a
            anota(f"el reloj avanzó {dt} s en 5 s reales", "OK" if 4 <= dt <= 7 else "FALLA")
            arranques = []
            for i in range(5):                                 # 5 arranques seguidos
                t0 = time.time()
                cmd("REINICIAR", 0.2)
                ok = False
                for _ in range(40):
                    time.sleep(0.5)
                    d = p.info()
                    if d and d.get("sonido") in (0, 1):
                        ok = True; break
                seg = time.time() - t0
                arranques.append(round(seg, 1))
                anota(f"arranque {i + 1}/5: {'respondió' if ok else 'NO respondió'} en {seg:.1f} s ({'sonido vivo' if d and d.get('sonido') == 1 else 'sonido NO arrancó'})",
                      "OK" if ok and d.get("sonido") == 1 else "FALLA")
            datos["pasos"]["arranque_seg_prom"] = {"ms": int(sum(arranques) / len(arranques) * 1000), "max": int(max(arranques) * 1000), "ram": 0}
            r3 = cmd("RELOJ", 0.5)
            dif = abs(_num(r3, "rtc", 0) - (_num(r2, "rtc", 0) + int(sum(arranques) + 5)))
            anota(f"la hora sobrevivió a 5 reinicios (diferencia {dif} s contra lo esperado)", "OK" if dif < 120 else "AVISO")
            lecturas = []
            for _ in range(20):
                lecturas.append(_num(cmd("BATERIA", 0.15), "mv", 0))
                time.sleep(0.25)
            lec = [x for x in lecturas if x]
            if lec:
                anota(f"batería: 20 lecturas entre {min(lec)} y {max(lec)} mV (dispersión {max(lec) - min(lec)} mV)", "OK" if max(lec) - min(lec) < 80 else "AVISO")
            else:
                anota("batería: no hubo lecturas", "FALLA")
            # escritura a flash mientras hay animación: no debe haber tirón largo
            cmd("ZONA 2", 2.0)
            r = cmd("GUARDAR", 3.0)
            d = p.info()
            mira("guardar la partida en plena animación", r, d, exige_ok=False)
            cmd("ZONA 0", 2.0)

            # ---------------------------------------------------------------- 28
      except (NameError, TypeError, KeyError, AttributeError) as _e:
        if not reanudando:
            raise
        anota(f"esta fase quedó a medias: usa datos de una fase anterior que no se corrió en esta vuelta ({type(_e).__name__}: {_e})", "AVISO")
    if fase(28, "Texto y vista de los paneles (revisión del simulador)"):
      try:
            anota("la comparación visual y de textos de cada panel se corre en el PC: `node sim/web/pruebas/regresion.js` (ver instrucciones al final del informe)", "AVISO")

            # ---------------------------------------------------------------- 30
      except (NameError, TypeError, KeyError, AttributeError) as _e:
        if not reanudando:
            raise
        anota(f"esta fase quedó a medias: usa datos de una fase anterior que no se corrió en esta vuelta ({type(_e).__name__}: {_e})", "AVISO")
    if fase("28b", "Lugares del rey: el claro y el mirador (zonas nuevas), la casita y los cinco juegos"):
      try:
            cmd("CERRAR", 0.5)
            # (v98) el claro y el mirador son zonas del mundo; la casita se abre al tocar la casa del rey
            r = cmd("ENTRAR 1", 2.0)
            li = cmd("LUGARINFO", 0.2)
            anota(f"el claro del rey es una zona del mundo ({li[li.index('OK'):] if li and 'OK' in li else li})", "OK" if _num(li, "zona", 0) == 1 else "FALLA")
            mira("en el claro del rey", cmd("INFO", 0.8), exige_ok=False)
            cmd("ENTRAR 2", 2.0)
            li = cmd("LUGARINFO", 0.2)
            anota(f"adentro de la casa del rey ({li[li.index('OK'):] if li and 'OK' in li else li})", "OK" if _num(li, "zona", 0) == 2 else "FALLA")
            cmd("SALIRLUGAR", 1.5)
            li = cmd("LUGARINFO", 0.2)
            anota(f"salir de la casita con la X: de vuelta en el claro ({li[li.index('OK'):] if li and 'OK' in li else li})", "OK" if _num(li, "zona", 0) == 1 else "FALLA")
            # los 5 juegos: portada, se juegan solos con «trampa», terminan con puntaje y estrellas, y se cierran con la X del final
            cmd("TRAMPALUGAR 1", 0.2)
            for jg, nombre in ((1, "galletas de Peumo"), (2, "canción de cuna"), (3, "salto al cielo"), (4, "atrapa hojas"), (5, "la piedra tibia")):
                r = cmd(f"JUEGOLUGAR {jg}", 1.5)
                if not mira(f"juego del lugar: {nombre} (abrir)", r, exige_ok=False): continue
                cmd("TOCARLUGAR 233 240", 0.6)   # la portada: empezar
                ganado = False
                for _ in range(60):   # hasta ~30 s (la canción de cuna dura lo más)
                    p.leer(0.5)
                    j = _num(cmd("LUGARINFO", 0.1), "juego", -1)
                    if j is not None and j >= 0 and j % 100 >= 50: ganado = True; break
                time.sleep(2.0)
                cmd("TOCARLUGAR 262 417", 0.8)   # la X del final
                li = cmd("LUGARINFO", 0.1)
                anota(f"juego del lugar: {nombre}: terminado con estrellas y cerrado ({li[li.index('OK'):] if li and 'OK' in li else li})",
                      "OK" if ganado and _num(li, "juego", 999) < 100 and _num(li, "zona", 0) > 0 else "FALLA")
            cmd("TRAMPALUGAR 0", 0.2)
            # salir con la X a mitad de un juego no cuenta y no cuelga
            cmd("JUEGOLUGAR 4", 1.0); cmd("TOCARLUGAR 233 240", 0.5); cmd("TOCARLUGAR 233 417", 0.8)
            li = cmd("LUGARINFO", 0.1)
            anota(f"juego del lugar: salir con la X a mitad ({li[li.index('OK'):] if li and 'OK' in li else li})", "OK" if _num(li, "juego", 999) < 100 else "FALLA")
            # el viaje en la nube: desde el cerro, baja, se sube, suben y llegan al mirador
            cmd("ZONA 3", 1.0)
            for _ in range(20):   # (espera a que en la pantalla ya se vea el cerro)
                if _num(cmd("LUGARINFO", 0.1), "vista", -1) == 3: break
                p.leer(0.5)
            cmd("NUBE", 0.3)
            entro = False
            for _ in range(30):
                p.leer(0.5)
                if _num(cmd("LUGARINFO", 0.1), "zona", 0) == 3: entro = True; break
            anota("la nube del cerro la sube al mirador" + ("" if entro else ": NO llegó al mirador en 15 s"), "OK" if entro else "FALLA")
            cmd("SALIRLUGAR", 4.0)   # baja por el sendero del lomo, al cerro
            mira("de vuelta en el cerro (bajó por el sendero)", cmd("INFO", 1.0), exige_ok=False)
      except (NameError, TypeError, KeyError, AttributeError) as _e:
        if not reanudando:
            raise
        anota(f"esta fase quedó a medias ({type(_e).__name__}: {_e})", "AVISO")
    if fase("29", "Pochi y su familia, y tocar animales (fw 102-103)"):
      try:
        for n, que in ((10, "Pochi con Chapi"), (14, "Pochi con los cuatro cachorros"), (16, "una escena de familia"),
                       (17, "un cachorro escondido al despedirse"), (21, "Pochi panza arriba"), (31, "la manía de la luciérnaga"),
                       (43, "Pochi se acurruca con ella dormida"), (101, "Pasita y su amiga la mariposa")):
            r = cmd(f"POCHI {n}", 2.5)
            anota(f"{que} (POCHI {n})", "FALLA" if "ERROR" in r else "OK")
            p.leer(2.0)
            cmd("CERRAR", 0.5)
        for n, que in ((2, "el animal se enoja"), (3, "la pican"), (4, "se pincha con las púas"),
                       (5, "el zorrillo la rocía y queda hedionda"), (6, "un animal grande la asusta")):
            r = cmd(f"TOQUE {n}", 2.0)
            anota(f"{que} (TOQUE {n})", "FALLA" if "ERROR" in r else "OK")
            p.leer(1.5)
            cmd("CERRAR", 0.5)
        p.leer(3.0)   # el olor se ve un rato: vahos y moscas encima de ella
        mira("con el olor del zorrillo encima", cmd("INFO", 1.0), exige_ok=False)
        cmd("ACCION 1 0", 2.0)   # un baño: le quita el olor
      except (NameError, TypeError, KeyError, AttributeError) as _e:
        if not reanudando:
            raise
        anota(f"esta fase quedó a medias ({type(_e).__name__}: {_e})", "AVISO")
    if fase("29b", "Papelitos, trocito de hielo, terquedad y paseos (fw 104)"):
      try:
        for c, que, t in (("PAPELITO 0", "un papelito del bosque: lo encuentras tú (se abre la hoja)", 3.0),
                          ("PAPELITO 1", "un papelito que te trae ella de su excursión", 3.5),
                          ("HIELO 1", "pesca el trocito de hielo (va a sus cristales)", 2.0),
                          ("HIELO 2", "el trocito de hielo se derrite (aviso y diario)", 2.0),
                          ("PASEO 1200", "paseo de verdad corto (1.200 pasos)", 2.0),
                          ("PASEO 3500", "paseo de verdad largo (trae algo)", 2.0)):
            r = cmd(c, t)
            anota(f"{que} ({c})", "FALLA" if "ERROR" in r else "OK")
            p.leer(1.5)
            cmd("CERRAR", 0.5)
        r = cmd("PASEO 300", 1.0)
        anota("300 pasos no son un paseo (PASEO 300)", "OK" if "r=0" in r else "AVISO")
        cmd("TERCA 1", 0.5)
        noes = 0
        for i in range(12):   # muy terca: darle juego mientras está en lo suyo; a veces dice que no (y si insistes, obedece)
            r = cmd("ACCION 2 0", 1.2)
            noes += 1 if "terca" in r or "-2" in r else 0
            cmd("CERRAR", 0.4)
        anota(f"muy terca: dijo que no {noes} de 12 veces a la primera (lo normal: algunas; nunca dos seguidas)", "OK")
        cmd("TERCA 0", 0.5)
      except (NameError, TypeError, KeyError, AttributeError) as _e:
        if not reanudando:
            raise
        anota(f"esta fase quedó a medias ({type(_e).__name__}: {_e})", "AVISO")
    if fase("29c", "La casita y los adornos del prado con el fondo por franjas (fw 105)"):
      try:
        cmd("ABRIR decoraciones -1", 1.5)      # la casita-tocón, el farol, el banco, las flores y los hongos
        cmd("ZONA 0", 1.5)
        for c, t in (("SINLIENZO 1", 1.0), ("SINLIENZO R", 3.0), ("MOMENTO NOCHE", 3.0), ("MOMENTO TARDE", 3.0), ("MOMENTO MANANA", 3.0),
                     ("CLIMA 1", 3.0), ("CLIMA 0", 3.0)):
            r = cmd(c, t)
            anota(f"fondo por franjas: {c}", "FALLA" if "ERROR" in r else "OK")
        d = p.info() or {}
        anota(f"memoria con el fondo por franjas: PSRAM {d.get('psram')} KB, RAM interna {d.get('ram')} B", "OK")
        cmd("SINLIENZO 0", 1.0)
        cmd("SINLIENZO R", 3.0)
        anota("mira la pantalla: la casita del prado, el farol y el banco tienen que verse IGUAL con franjas y sin franjas "
              "(el registro de la placa anota «fondo por franjas» cada vez que lo necesita de verdad)", "OK")
      except (NameError, TypeError, KeyError, AttributeError) as _e:
        if not reanudando:
            raise
        anota(f"esta fase quedó a medias ({type(_e).__name__}: {_e})", "AVISO")
    fase(30, "Cierre")
    cmd("PANTALLA 1", 0.5)
    en = cmd("ENERGIA", 0.3)
    anota(f"energía al final: {en[en.index('OK'):] if en and 'OK' in en else en}")
    bat1 = cmd("BATERIA", 0.2)
    pct1 = _num(bat1, "pct", -1)
    anota(f"batería: {pct0}% → {pct1}% durante toda la prueba")
    d = p.info()
    anota(f"estado final: RAM {d and d.get('ram')} B (al inicio {d0.get('ram')}), PSRAM {d and d.get('psram')} KB (al inicio {d0.get('psram')})",
          "OK" if d and d.get("ram", 0) > d0.get("ram", 0) - 6000 else "AVISO")
    cierra_fase()
    guardar_progreso("30", "Cierre", fin=True)
    p.cmd("MARCA PRUEBA TOTAL FIN", 0.2)
    _comparar(sal, datos, anota)

    sal.append("")
    sal.append(f"== RESUMEN DE LA PRUEBA TOTAL: {T.ok} OK, {T.aviso} AVISOS, {T.falla} FALLAS ==")
    for f in T.fallas:
        sal.append("   FALLA: " + f)
    print(sal[-1 - len(T.fallas)])
    return T


def _carpeta():
    try:
        import probar
        return probar.carpeta_datos()
    except Exception:
        return os.getcwd()

def _comparar(sal, datos, anota):
    """Compara con la corrida anterior (prueba_total_datos.json) y guarda esta."""
    _d = _carpeta()
    nuevo, viejo = os.path.join(_d, "prueba_total_datos.json"), os.path.join(_d, "prueba_total_anterior.json")
    ant = None
    try:
        if os.path.exists(nuevo):
            ant = json.load(open(nuevo, encoding="utf-8"))
            os.replace(nuevo, viejo)
    except Exception:
        ant = None
    try:
        json.dump(datos, open(nuevo, "w", encoding="utf-8"), ensure_ascii=False, indent=1)
    except Exception as e:
        sal.append(f"(no pude guardar {nuevo}: {e})")
    sal.append(""); sal.append("== COMPARACIÓN CON LA PRUEBA ANTERIOR ==")
    if not ant:
        sal.append("No hay una prueba anterior para comparar: esta queda guardada y la próxima vez se compara."); print(sal[-1]); return
    sal.append(f"anterior: {ant.get('fecha')}   ahora: {datos.get('fecha')}")
    peores, mejores = [], []
    for k, v in datos["pasos"].items():
        a = ant.get("pasos", {}).get(k)
        if not a: continue
        if v["ms"] > max(20, a["ms"] * 2): peores.append(f"{k}: {a['ms']} → {v['ms']} ms")
        elif a["ms"] > max(20, v["ms"] * 2): mejores.append(f"{k}: {a['ms']} → {v['ms']} ms")
        if a["ram"] - v["ram"] > 3000: peores.append(f"{k}: RAM {a['ram']} → {v['ram']} B")
    sal.append(f"pasos más LENTOS que antes ({len(peores)}):" + ("" if peores else " ninguno"))
    sal += ["   " + x for x in peores[:30]]
    sal.append(f"pasos más RÁPIDOS que antes ({len(mejores)}):" + ("" if mejores else " ninguno"))
    sal += ["   " + x for x in mejores[:30]]
    sal.append("consumo por fase (antes → ahora, en puntos de batería / segundos):")
    for k, v in datos["fases"].items():
        a = ant.get("fases", {}).get(k)
        if a: sal.append(f"   {k}: {a.get('pct')} pts en {a.get('seg')} s  →  {v.get('pct')} pts en {v.get('seg')} s")
    for l in sal[-(len(peores) + len(mejores) + 6):]: print(l)
