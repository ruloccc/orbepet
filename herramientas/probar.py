#!/usr/bin/env python3
"""Prueba la mascota por el cable USB, como en el simulador.

Uso (en la consola "ESP-IDF PowerShell"; cierra antes el monitor, usan el mismo puerto):
    python probar.py                      -> menú: instalar firmware nuevo, probar, ver estado
    python probar.py COM5                 -> todo: paneles, juegos, zonas, secretos y finales
    python probar.py COM5 paneles         -> solo los paneles
    python probar.py COM5 juegos          -> pelota, escondidas, frutas, hongos
    python probar.py COM5 zonas           -> abre cada zona
    python probar.py COM5 secretos        -> las escenas de los secretos
    python probar.py COM5 finales         -> final regular y final secreto
    python probar.py COM5 actividades     -> cada actividad de la mascota
    python probar.py COM5 cantar          -> modo cantar (voz y animación)
    python probar.py COM5 info            -> estado ahora
    python probar.py COM5 panel 12        -> un panel
    python probar.py COM5 comando "JUEGO 0"   -> cualquier comando PROBAR
    python probar.py COM5 total           -> PRUEBA TOTAL automática de todo (ver prueba_total.py); baja también el registro
    python probar.py COM5 restaurar       -> devuelve la partida a como estaba antes de una prueba
Antes de probar desbloquea todo (objetos, cartas, zonas, ropa, secretos...) guardando antes la partida;
al terminar te pregunta si la deja como estaba.
Deja el resultado en informe.txt (pégamelo).
"""
import sys, time, re, os, glob, hashlib, shutil, subprocess, zipfile
try:
    import serial
except ImportError:
    sys.exit("Falta pyserial: ejecuta  pip install pyserial")

LENTO_MS = 100      # más que esto por cuadro se marca LENTO
RAM_MIN = 4000      # menos RAM interna libre que esto se marca POCA RAM
ESPERA = 3.6        # segundos por prueba (la placa mide la velocidad en ventanas de 3 s)

REINICIAR = None  # función(puerto) -> bool que pone el programa con ventana: reinicia la placa por esptool (watchdog) cuando se colgó
ETAPA = None  # función(texto) que muestra qué se está haciendo
PROGRESO = None  # función(porcentaje) que pone el programa con ventana
EXTRAER = None  # función(porcentaje) mientras se extrae el zip
_total, _hecho = [1], [0]

def etapa(t):
    if ETAPA:
        ETAPA(t)

def avanza():
    _hecho[0] += 1
    if _hecho[0] >= _total[0] - 2:  # la cuenta se quedó corta (cada placa tiene distinta cantidad de paneles/sonidos): se alarga para no marcar 100 % antes de terminar
        _total[0] = _hecho[0] + max(8, _hecho[0] // 8)
    if PROGRESO:
        PROGRESO(min(99, 100 * _hecho[0] // max(1, _total[0])))  # el 100 % lo pone el programa al terminar

CANCELAR = False  # lo pone el programa con ventana para detener una prueba
GUIA = None  # el programa con ventana pone aquí algo con guia_paso/guia_estado/guia_cerrar: una sola ventana que guía la prueba con tus manos
AVISAR = None  # función(titulo, texto) que pone el programa con ventana: muestra lo que debes hacer y espera a que aprietes Aceptar
PREGUNTAR = None  # función(texto) -> bool que pone el programa con ventana; sin ella se pregunta en la consola

def carpeta_datos():
    """Carpeta que NO se borra al reinstalar: Documentos\\MascotaPruebas (progreso de la prueba, comparación con la anterior, informes y registros)."""
    base = os.path.join(os.path.expanduser("~"), "Documents")
    if not os.path.isdir(base):
        base = os.path.expanduser("~")
    ruta = os.path.join(base, "MascotaPruebas")
    try:
        os.makedirs(ruta, exist_ok=True)
        return ruta
    except Exception:
        return os.getcwd()

class Cancelado(Exception):
    pass

class PuertoPerdido(Exception):
    pass

class Placa:
    def __init__(self, puerto):
        self.s = serial.Serial()
        self.s.port = puerto
        self.s.baudrate = 115200
        self.s.timeout = 0.2
        self.s.dtr = False   # que abrir el puerto no reinicie la placa
        self.s.rts = False
        self.s.open()
        self.extra = []      # errores/avisos que salen por el camino
        self.ultimas = []    # las últimas líneas que dijo la placa (para ver por qué no contestó)

    def reabrir(self, seg=45):
        """Cierra el puerto (por ejemplo, antes de desconectar el cable) y lo vuelve a abrir cuando la placa reaparece (puede cambiar de COM)."""
        try: self.s.close()
        except Exception: pass
        fin = time.time() + seg
        while time.time() < fin:
            if CANCELAR:
                raise Cancelado()
            try:
                self.s.port = puerto_auto() or self.s.port
                self.s.open()
                return self.s.port
            except Exception:
                time.sleep(1)
        return None

    def leer(self, seg):
        """Lee lo que llegue durante 'seg' segundos; devuelve las líneas."""
        fin, lineas, resto = time.time() + seg, [], b""
        while time.time() < fin:
            if CANCELAR:
                raise Cancelado()
            try:
                d = self.s.read(4096)
            except (serial.SerialException, OSError):
                self._recuperar()
                continue
            if not d:
                continue
            resto += d
            while b"\n" in resto:
                l, resto = resto.split(b"\n", 1)
                l = l.decode("utf-8", "replace").strip()
                if l:
                    lineas.append(l)
                    self.ultimas = (self.ultimas + [l])[-40:]
                    self.nota(l)
        return lineas

    def _recuperar(self):
        """El puerto desapareció (la placa se reinició o se cayó el USB): intenta reabrirlo; si no, avisa."""
        self.reinicios = getattr(self, "reinicios", 0) + 1
        self.extra.append("E (probador) se perdió el puerto USB: la placa se reinició o se cayó. ÚLTIMO COMANDO ENVIADO: " + str(getattr(self, "ult_cmd", "?")))
        if self.reinicios > 4 or self.reabrir(60) is None:
            raise PuertoPerdido("se perdió el puerto USB y la placa no volvió")
        time.sleep(4)  # que termine de arrancar

    def nota(self, l):
        # volcado de un fallo grave: guarda las ~40 líneas siguientes (PC, dirección, registros, Backtrace)
        if "Guru Meditation" in l or "abort() was called" in l:
            self._vol = 40
            self.extra.append("== VOLCADO DEL FALLO (guarda esto: con el archivo .elf se ve la línea exacta) ==")
            self.extra.append(l)
            return
        if getattr(self, "_vol", 0) > 0 and not l.startswith("MASCOTA PROBAR"):
            self._vol -= 1
            if re.search(r"Backtrace|PC\s*:|EXCVADDR|EXCCAUSE|A\d+\s*:|Core\s+\d|ELF file|rst:|Rebooting|SP\s*:|Stack", l):
                self.extra.append(l)
            return
        if re.match(r"^\x1b?\[?[0-9;]*m?\s*[EW] \(", l) or " E (" in l or l.startswith("E (") or "Guru Meditation" in l or "abort()" in l:
            self.extra.append(l)

    def cmd(self, texto, espera=ESPERA, esperar_resp=True):
        self.ult_cmd = texto
        try:
            self.s.reset_input_buffer()
            self.s.write(("MASCOTA PROBAR " + texto + "\n").encode())
        except (serial.SerialException, OSError):
            self._recuperar()
            return None
        resp = None
        # (v97) se espera la respuesta DE ESTE comando («MASCOTA PROBAR <palabra>»), hasta 12 s: si la placa se demoraba
        # (p. ej. la flash limpiándose), antes se tomaba la respuesta atrasada del comando anterior y todo quedaba corrido
        clave = "MASCOTA PROBAR " + (texto.split()[0] if texto.split() else "")
        otra = None
        ini = time.time()
        fin = ini + 12
        while time.time() < fin and esperar_resp:
            for l in self.leer(0.3):
                if "MASCOTA PROBAR" in l:
                    r = l[l.index("MASCOTA PROBAR"):]
                    if r.startswith(clave): resp = r
                    elif otra is None and not r.startswith("MASCOTA PROBAR INFO"): otra = r
            if resp:
                break
            if otra and time.time() - ini > 5:   # contestó con otro nombre (comandos viejos): como antes
                resp = otra
                break
        if resp and time.time() - ini > 5:
            self.demoras = getattr(self, "demoras", 0) + 1
            print(f"   (la placa tardó {time.time() - ini:.0f} s en contestar «{texto[:40]}»)")
        self.leer(espera)
        if resp: self.mudas = 0
        elif esperar_resp: self._muda()
        return resp

    def info(self):
        self.s.write(b"MASCOTA PROBAR INFO\n")
        fin = time.time() + 10   # (v97) 10 s: una pausa larga de la flash no es un cuelgue
        while time.time() < fin:
            for l in self.leer(0.3):
                if "PROBAR INFO OK" in l:
                    self.mudas = 0
                    return parsear(l)
        self.explicar_silencio()
        self._muda()
        return None

    def _muda(self):
        """Si la placa no contesta 10 veces seguidas, está colgada: se corta la prueba (sigue guardada para «CONTINUAR») en vez de esperar horas."""
        self.mudas = getattr(self, "mudas", 0) + 1
        if self.mudas >= 10:
            raise PuertoPerdido("la placa dejó de contestar (10 comandos seguidos sin respuesta): está colgada. Mira su pantalla, sácale la batería, vuelve a ponerla, usa «Obtener log del dispositivo» y luego «CONTINUAR la prueba cortada»")

    def explicar_silencio(self):
        """La placa no contestó: muestra qué dijo al final (¿se reinició? ¿un error?)."""
        raras = [l for l in self.ultimas if re.search(r"rst:|ESP-ROM|Guru|abort\(\)|panic|Backtrace|ARRANQUE|boot:|E \(|W \(|heap|memoria|asign", l)]
        print("   ↳ SIN RESPUESTA. Lo último que dijo la placa:")
        for l in (raras[-8:] or self.ultimas[-6:] or ["(nada: se quedó muda)"]):
            print("     " + l[:200])
        if any(("rst:" in l or "ESP-ROM" in l or "ARRANQUE" in l) for l in self.ultimas):
            print("     → parece que la placa SE REINICIÓ")

def guia_paso(titulo, texto, seg=None, n=0, total=0):
    if GUIA:
        GUIA.guia_paso(titulo, texto, seg, n, total)
    else:
        print(f"   >>> [{n}/{total}] {titulo}: {texto}" + (f" ({seg} s)" if seg else ""))

def guia_estado(texto, bueno=True):
    if GUIA:
        GUIA.guia_estado(texto, bueno)
    else:
        print(f"       {'✔' if bueno else '·'} {texto}")

def guia_punto(x=None, y=None):
    """Dibuja en la ventana guía una pantalla chica con un punto donde hay que tocar (None lo quita)."""
    if GUIA:
        GUIA.guia_punto(x, y)

def guia_cerrar():
    if GUIA:
        GUIA.guia_cerrar()

def avisar(titulo, texto):
    """Pide algo al usuario en una ventana (programa con ventana) o en la consola, y espera a que lo confirme."""
    if AVISAR:
        AVISAR(titulo, texto)
        return
    try:
        input(f"\n*** {titulo} ***\n{texto}\n[Enter para empezar] ")
    except EOFError:
        pass

def preguntar_si(texto):
    if PREGUNTAR:
        return PREGUNTAR(texto)
    try:
        return input(texto + " [S/n]: ").strip().lower() not in ("n", "no")
    except EOFError:
        return True

def desbloquear_todo(p):
    """Desbloquea todo para probar con todo disponible. La placa guarda antes la partida (sobrevive a un reinicio)."""
    etapa("Desbloqueando todo para probar")
    r = p.cmd("DESBLOQUEAR TODO", 1)
    if r is None or "desconocido" in r:
        print("(Este firmware no sabe desbloquear todo: instala el nuevo. Pruebo con lo que tenga la partida.)")
        return False
    if "ERROR" in r:
        print("(No pude guardar la partida antes de desbloquear, así que NO desbloqueo nada: " + r + ")")
        return False
    print("Respaldo hecho: la partida de antes quedó guardada en la placa y se carga sola al terminar. Todo desbloqueado para la prueba.")
    return True

def terminar_desbloqueo(p, hubo):
    """Al terminar (o si se corta la prueba): la mascota vuelve SOLA a como estaba antes de empezar, sin lo desbloqueado ni lo que se probó.
    El respaldo lo hizo la placa al empezar y vive en su flash: sobrevive a reinicios y cuelgues."""
    if not hubo:
        return
    etapa("Devolviendo la mascota a como estaba antes de la prueba")
    r = p.cmd("RESTAURAR", 1.5)
    if r is None:  # a lo mejor se estaba reiniciando: un segundo intento
        p.leer(8)
        r = p.cmd("RESTAURAR", 1.5)
    ok = bool(r) and "ERROR" not in r
    if not ok and r and "no hay respaldo" in r:
        # (1.0.23) la PRUEBA TOTAL ya la devolvió por el camino (fases 22, 23, 25 y 26 restauran y borran el respaldo):
        # no queda nada que devolver; lo de las fases siguientes es poco (puntos de juegos, la hora ya volvió con MOMENTO REAL)
        print("La partida ya se había devuelto durante la prueba (no quedaba respaldo por usar). Está bien.")
        return True
    print("Partida devuelta a como estaba antes de la prueba." if ok else
          "No pude restaurar: " + str(r) + "  (el respaldo sigue en la placa: usa «Restaurar partida»)")
    return ok

def parsear(l):
    d = {k: int(v) for k, v in re.findall(r"(\w+)=(\d+)(?![/\d])", l)}
    m = re.search(r"vol=(\d+/\d+/\d+)", l)
    if m:
        d["vol"] = m.group(1)
    return d

def marcas(d):
    m = []
    if d is None:
        return ["SIN RESPUESTA"]
    if d.get("ms", 0) > LENTO_MS:
        m.append("LENTO")
    if d.get("ram", 1 << 30) < RAM_MIN:
        m.append("POCA RAM")
    return m

def linea(nombre, d, extra_marcas=()):
    if d is None:
        return f"{nombre:<26} SIN RESPUESTA"
    m = marcas(d) + list(extra_marcas)
    return f"{nombre:<26} {d.get('ms',0):>4} ms (máx {d.get('max',0)})  {d.get('cuadros',0):>3} cuad/3s  RAM {d.get('ram',0):>6} B  PSRAM {d.get('psram',0):>5} KB  {'  '.join(m) if m else 'ok'}"

def paneles(p, sal, uno=None):
    r = p.cmd("PANELES", 0.2)
    nombres = r.split()[4:] if r else []
    sal.append("== PANELES ==")
    etapa("Probando paneles")
    rango = [uno] if uno is not None else range(len(nombres))
    for k in rango:
        etapa(f"Probando panel {k}: {nombres[k] if k < len(nombres) else ''}")
        resp = p.cmd(f"PANEL {k}", ESPERA)
        d = p.info()
        nom = nombres[k] if k < len(nombres) else "?"
        sal.append(linea(f"{k:>2} {nom}", d, [] if resp and "ERROR" not in resp else ["no aplica (falta contenido: cartas, objetos…)"]))
        print(sal[-1]); avanza()
    p.cmd("CERRAR", 1)

NOM_JUEGO = ["pelota", "escondidas", "frutas", "hongos"]
def juegos(p, sal):
    sal.append("== JUEGOS ==")
    etapa("Probando juegos")
    for j in range(4):
        p.cmd("CERRAR", 1)
        p.cmd(f"JUEGO {j}", 3)
        d = p.info()
        ok = d is not None and d.get("juego", 0) != 0
        sal.append(linea(f"juego {j} {NOM_JUEGO[j]}", d, [] if ok else ["NO APARECIÓ"]))
        print(sal[-1]); avanza()
        p.cmd("CERRAR", 1)

def sonido(p, sal):
    sal.append("== SONIDO Y VOLUMEN ==")
    etapa("Probando sonido")
    d0 = p.info()
    vivo = d0 is not None and d0.get("sonido") == 1
    sal.append("tarea del sonido: " + ("VIVA" if vivo else "NO ARRANCÓ (sin memoria): por eso no suena ni cambia el volumen"))
    print(sal[-1])
    if d0 is None:
        return
    orig = d0.get("vol", "")
    for nombre, v in (("alto (100)", 100), ("medio (50)", 50), ("bajo (10)", 10)):
        p.cmd(f"VOLUMEN {v} {v} {v}", 0.3)
        p.cmd("SONIDO 0", 0.2)
        p.cmd("SONIDO 1", 1.5)
        d = p.info()
        ok = d is not None and d.get("vol") == f"{v}/{v}/{v}"
        sal.append(f"volumen {nombre:<12} escuchaste el pío y el efecto? el valor en la placa: {d and d.get('vol')}  {'ok' if ok else 'NO CAMBIÓ'}")
        print(sal[-1]); avanza()
    if isinstance(orig, str) and orig.count("/") == 2:
        p.cmd("VOLUMEN " + orig.replace("/", " "), 0.3)
        sal.append(f"(volumen devuelto a {orig})")


NOM_ACT = ["pasear", "olfatear", "cavar", "siesta", "explorar", "mirar", "jugar sola", "refugio", "sombra", "nieve", "esperar", "abrigarse", "perseguir",
           "pescar", "con su amigo", "nadar", "trepar", "juguete", "chapotear", "pelota", "probar ropa", "buscar", "aves", "charlar", "cantar", "bailar"]
ACT_CANTAR = 24

def actividades(p, sal):
    """Fuerza cada actividad de la mascota 3,6 s y mide la velocidad del dibujo."""
    sal.append("== ACTIVIDADES ==")
    etapa("Probando actividades")
    p.cmd("CERRAR", 1)
    for k, nom in enumerate(NOM_ACT):
        etapa(f"Actividad {k}: {nom}")
        resp = p.cmd(f"ACTIVIDAD {k}", ESPERA)
        d = p.info()
        ok = resp and "ERROR" not in resp
        sal.append(linea(f"act {k:>2} {nom}", d, [] if ok else ["no aplica"]))
        print(sal[-1]); avanza()
    p.cmd("ACTIVIDAD 0", 0.5)

def cantar(p, sal):
    """Modo cantar: la mascota canta 4 veces seguidas; se oye la voz y se mira que el dibujo no se trabe."""
    sal.append("== CANTAR ==")
    etapa("Probando el modo cantar")
    p.cmd("CERRAR", 1)
    for k in range(4):
        etapa(f"Cantando ({k + 1} de 4): escucha la voz y mira la boca y las notas")
        p.cmd(f"ACTIVIDAD {ACT_CANTAR}", 0.3)
        p.cmd("SONIDO 1", 0.3)
        p.cmd("SONAR 1", 5)
        d = p.info()
        sal.append(linea(f"cantar {k + 1}", d))
        print(sal[-1]); avanza()
    p.cmd("ACTIVIDAD 0", 0.5)

NOM_ZONA = ["prado", "bosque", "lago", "colina", "cueva", "isla"]
def zonas(p, sal):
    sal.append("== ZONAS ==")
    etapa("Probando zonas")
    p.cmd("DESBLOQUEAR", 0.5)
    for z in range(6):
        p.cmd(f"ZONA {z}", 3)
        d = p.info()
        ok = d is not None and d.get("zona") == z
        sal.append(linea(f"zona {z} {NOM_ZONA[z]}", d, [] if ok else ["NO CAMBIÓ"]))
        print(sal[-1]); avanza()
    p.cmd("ZONA 0", 2)

NOM_SEC = ["anillo", "reflejo", "cometa", "cristales"]
def secretos(p, sal):
    sal.append("== SECRETOS ==")
    etapa("Probando secretos")
    for k in range(4):
        p.cmd("CERRAR", 1)
        p.cmd(f"SECRETO {k}", 6)
        d = p.info()
        ok = d is not None and d.get("panel", 0) != 0
        sal.append(linea(f"secreto {k} {NOM_SEC[k]}", d, [] if ok else ["NO SE ABRIÓ"]))
        print(sal[-1]); avanza()
        p.cmd("CERRAR", 1)

def finales(p, sal):
    sal.append("== FINALES ==")
    etapa("Probando finales")
    for nombre in ("REGULAR", "SECRETO"):
        p.cmd("CERRAR", 1)
        p.cmd("FINAL " + nombre, 8)
        d = p.info()
        ok = d is not None and d.get("panel", 0) != 0
        sal.append(linea("final " + nombre.lower(), d, [] if ok else ["NO SE MOSTRÓ"]))
        print(sal[-1]); avanza()
        p.cmd("CERRAR", 1)
        p.cmd("FINAL BORRAR", 1)


NOM_EST = ["primavera", "verano", "otoño", "invierno"]
NOM_MOM = [("MANANA", "día"), ("TARDE", "tarde"), ("NOCHE", "noche")]
NOM_CLIMA = ["despejado", "nublado", "lluvia", "viento", "nieve", "tormenta", "frío"]
RENDIMIENTO_PASOS = 360  # aprox.: zonas×estaciones×horas, climas, efectos, paneles, juegos, sensores, micrófono, música, sonidos, historias, secretos, final, Pochi, giro

def _registro_final(p, n=8):
    """Las últimas líneas del registro que la placa guarda (motivo del reinicio, avisos y errores antes de caer)."""
    try:
        p.s.reset_input_buffer()
        p.s.write(b"MASCOTA REGISTRO\n")
        lineas, fin = [], time.time() + 20
        while time.time() < fin:
            for l in p.leer(0.5):
                if "MASCOTA REGISTRO FIN" in l: fin = 0
                elif "MASCOTA REGISTRO |" in l: lineas.append(l.split("MASCOTA REGISTRO |", 1)[1].strip())
        return lineas[-n:]
    except Cancelado:
        raise
    except Exception:
        return []

def _motivo_reinicio(p):
    """Mira lo último que dijo la placa y dice por qué se reinició (pánico, watchdog, poca energía...); '' si no hay señales."""
    t = "\n".join(p.ultimas + p.extra[-30:])
    if re.search(r"Guru Meditation|abort\(\)|panic|Backtrace|LoadProhibited|StoreProhibited|IllegalInstruction", t, re.I): m = "PÁNICO (fallo del programa)"
    elif re.search(r"watchdog|wdt", t, re.I): m = "SE TRABÓ (watchdog)"
    elif re.search(r"brownout", t, re.I): m = "POCA ENERGÍA (brownout)"
    elif re.search(r"rst:|ESP-ROM|ARRANQUE|boot:", t): m = "REINICIO"
    else: return ""
    esp = [l for l in (p.ultimas + p.extra[-30:]) if re.search(r"Guru|abort\(\)|panic|Backtrace|assert|heap|memoria|E \(", l)]
    return m + (" · " + esp[-1][:110] if esp else "")

def _caso(p, sal, res, nombre, d, extra=()):
    """Anota un caso del informe de rendimiento y lo guarda para el resumen."""
    sal.append(linea(nombre, d, extra)); print(sal[-1]); avanza()
    if d is None:
        # ¿se reinició? esperar a que arranque de nuevo y seguir
        etapa("La placa no contestó: esperando que arranque")
        p.leer(14)
        if p.info() is None and REINICIAR:  # sigue muda: colgada, no reiniciada. Se le pide un reinicio completo
            etapa("La placa sigue sin contestar: reiniciándola")
            try:
                p.s.close()
                REINICIAR(p.s.port)
                p.s.open()
                p.leer(3)
            except Cancelado:
                raise
            except Exception as e:
                sal.append(f"   (no pude reiniciarla sola: {e}; desconecta y vuelve a conectar la batería/cable)"); print(sal[-1])
        motivo = _motivo_reinicio(p)
        if motivo:
            sal.append(f"   ↳ {motivo} en: {nombre}"); print(sal[-1])
            for l in _registro_final(p):
                sal.append("      registro de la placa: " + l[:160]); print(sal[-1])
        res.append((nombre + (f" — {motivo}" if motivo else ""), 0, 0, 1 << 30, True))
        return False
    if d.get("cuadros", 1) == 0 and d.get("ms", 1) == 0 and d.get("max", 1) == 0:  # recién arrancó: se reinició y volvió antes de que la miráramos
        p.leer(4)
        motivo = _motivo_reinicio(p) or "REINICIO (0 cuadros: la placa recién arrancó)"
        sal.append(f"   ↳ {motivo} en: {nombre}"); print(sal[-1])
        for l in _registro_final(p):
            sal.append("      registro de la placa: " + l[:160]); print(sal[-1])
        res.append((nombre + " — " + motivo, 0, 0, 1 << 30, True))
        return False
    res.append((nombre, d.get("ms", 0), d.get("max", 0), d.get("ram", 1 << 30), False))
    return True

def _pedir(p, texto, clave, seg=6):
    """Manda un comando y devuelve (dict con los números, línea) de la respuesta que trae 'clave'; None si la placa no contesta."""
    p.s.reset_input_buffer()
    p.s.write(("MASCOTA PROBAR " + texto + "\n").encode())
    fin = time.time() + seg
    while time.time() < fin:
        for l in p.leer(0.3):
            if clave in l:
                return ({k: int(v) for k, v in re.findall(r"(\w+)=(-?\d+)", l)}, l)
    return None

def _problema(sal, res, nombre, texto):
    sal.append(f"{nombre:<30} PROBLEMA: {texto}"); print(sal[-1]); avanza()
    res.append((nombre + " — " + texto, 0, 0, 1 << 30, True))

def hardware(p, sal, res):
    """Sensores, micrófono, música, sonidos, historias, secretos, huevo, finales y giro: tiempo de cada cosa y cuadros por segundo mientras tanto."""
    # sensores
    etapa("sensores"); 
    r = _pedir(p, "SENSORES", "SENSORES OK")
    if not r:
        _problema(sal, res, "sensores", "la placa no contestó"); p.explicar_silencio(); return
    d, l = r
    sal.append("sensores: " + l[l.index("imu_us"):][:230]); print(sal[-1])
    chk = []
    if d.get("imu_us", 999) < 100: chk.append("acelerómetro: lectura sospechosamente rápida (¿no está conectado?)")
    if d.get("imu_us", 0) > 3000: chk.append(f"acelerómetro lento ({d['imu_us']} µs por lectura)")
    if d.get("tilt_x") == 0 and d.get("tilt_y") == 0 and d.get("tilt2_x") == 0 and d.get("tilt2_y") == 0: chk.append("inclinación siempre 0,0 (sin acelerómetro)")
    if d.get("bat", -1) < 0: sal.append("   (batería: sin lectura (-1): normal si el reloj está solo con el cable USB y sin batería)")
    elif d.get("bat", 100) < 15 and not d.get("carga"): chk.append(f"batería baja: {d['bat']} %")
    if d.get("bat_us", 0) > 5000: chk.append(f"batería lenta ({d['bat_us']} µs)")
    if d.get("rtc_us", 0) > 5000: chk.append(f"reloj lento ({d['rtc_us']} µs)")
    if d.get("pasos_us", 0) > 3000: chk.append(f"podómetro lento ({d['pasos_us']} µs)")
    if not d.get("res_ok"): chk.append("no pudo escribir un archivo de 4 KB en la partición de recursos")
    elif d.get("res_ms", 0) > 4000: chk.append(f"guardar 4 KB tardó {d['res_ms']} ms")
    elif d.get("res_ms", 0) > 800: sal.append(f"   (escribir 4 KB en los recursos tardó {d['res_ms']} ms: la memoria flash es lenta para borrar; solo pasa al guardar sonidos o íconos propios)")
    if not d.get("leer_ok"): chk.append("no pudo leer de vuelta el archivo")
    if not d.get("guardar_ok"): chk.append("no pudo guardar la partida")
    elif d.get("guardar_ms", 0) > 800: chk.append(f"guardar la partida tardó {d['guardar_ms']} ms (se nota un tirón)")
    for c in chk: _problema(sal, res, "sensores", c)
    if not chk: sal.append("sensores: todos responden y rápido ok"); print(sal[-1])
    _caso(p, sal, res, "después de los sensores", p.info())
    # micrófono: 2 s escuchando
    etapa("micrófono"); r = _pedir(p, "MIC 2000", "MIC OK", 10)
    if not r: _problema(sal, res, "micrófono", "la placa no contestó")
    else:
        d, l = r
        sal.append("micrófono 2 s: " + l[l.index("muestras"):][:100]); print(sal[-1])
        if d.get("muestras", 0) == 0: _problema(sal, res, "micrófono", "el micrófono no abrió (todas las lecturas fallaron; si la placa avisó «poca memoria DMA» es por la RAM interna, no por el micrófono)")
        elif d.get("fallas", 0) > 0: _problema(sal, res, "micrófono", f"{d['fallas']} lecturas fallaron")
        elif d.get("max", 0) == d.get("min", 0) == 0: sal.append("   (silencio total: normal en una pieza callada; sopla o haz ruido para ver cambios)")
        _caso(p, sal, res, "después del micrófono", p.info())
    # escuchar música: una tanda dura 6 s de cada 20
    etapa("escuchar música"); p.cmd("MUSICA 1", 0.5)
    for k, seg in enumerate((1.5, 6.5, 8.0, 6.0)):  # dentro de una tanda, fuera y otra vez dentro
        p.leer(seg); _caso(p, sal, res, f"escuchando música ({k+1}/4)", p.info())
    p.cmd("MUSICA 0", 0.5)
    # sonidos ambiente
    r = _pedir(p, "CUENTAS", "CUENTAS OK")
    cu = r[0] if r else {}
    for a in range(1, cu.get("ambientes", 0)):
        etapa(f"ambiente {a}"); p.cmd(f"AMBIENTE {a}", ESPERA); _caso(p, sal, res, f"sonido ambiente {a}", p.info())
    p.cmd("AMBIENTE -1", 0.5)
    # todos los sonidos y voces, uno tras otro
    n = cu.get("sonidos", 0)
    for a in range(1, n):
        etapa(f"sonido {a}/{n-1}"); p.cmd(f"SONAR {a}", 1.2)
        d = p.info()
        if a % 8 == 0 or d is None or marcas(d) or d.get("sonido") != 1: _caso(p, sal, res, f"sonido {a} (de {n-1})", d, [] if d and d.get("sonido") == 1 else ["TAREA DE SONIDO MUERTA"])
        else: res.append((f"sonido {a}", d.get("ms", 0), d.get("max", 0), d.get("ram", 1 << 30), False)); avanza()
    sal.append(f"sonidos probados: {max(n-1,0)} (se anotan los lentos y uno de cada 8)"); print(sal[-1])
    # historias de las criaturas, secretos, huevo, finales, Pochi, giro
    for c in range(cu.get("criaturas", 6)):
        for k in range(5):
            etapa(f"historia {c}.{k}"); p.cmd("CERRAR", 0.3); p.cmd(f"HISTORIA {c} {k}", 2.5)
            _caso(p, sal, res, f"historia {c}.{k}", p.info())
    p.cmd("CERRAR", 1)
    # ¿fuga de PSRAM? las mismas historias otra vez: si ya estaban en caché, la PSRAM libre no debería bajar más
    d1 = p.info() or {}
    for k in range(5):
        p.cmd("CERRAR", 0.3); p.cmd(f"HISTORIA 0 {k}", 2.5)
    p.cmd("CERRAR", 1); d2 = p.info() or {}
    if d1 and d2:
        bajo = d1.get("psram", 0) - d2.get("psram", 0)
        sal.append(f"repetir las historias 0.0-0.4: PSRAM libre {d1.get('psram')} KB → {d2.get('psram')} KB ({'-' if bajo > 0 else '+'}{abs(bajo)} KB)  " + ("POSIBLE FUGA" if bajo > 60 else "ok (sin fuga)")); print(sal[-1])
        if bajo > 60: res.append((f"posible fuga de PSRAM: -{bajo} KB al repetir historias", 0, 0, 1 << 30, True))
    for n_s in range(cu.get("secretos", 4)):
        etapa(f"secreto {n_s}"); p.cmd(f"SECRETO {n_s}", ESPERA + 2); _caso(p, sal, res, f"secreto {n_s}", p.info()); p.cmd("CERRAR", 1)
    etapa("el huevo"); p.cmd("HUEVO 0", ESPERA); _caso(p, sal, res, "huevo: viaje", p.info()); p.cmd("CERRAR", 1); p.cmd("HUEVO 2", 0.5)
    for f in ("REGULAR", "SECRETO"):
        etapa(f"final {f.lower()}"); p.cmd(f"FINAL {f}", ESPERA + 2); _caso(p, sal, res, f"final {f.lower()}", p.info()); p.cmd("FINAL BORRAR", 0.5); p.cmd("CERRAR", 1)
    for q in range(6):
        etapa(f"Pochi {q}"); p.cmd(f"POCHI {q}", ESPERA); _caso(p, sal, res, f"Pochi escena {q}", p.info())
    p.cmd("POCHI 3", 1); p.cmd("CERRAR", 1)
    for g in (1, 2, 3, 0):
        etapa(f"pantalla girada {g*90}°"); p.cmd(f"GIRO {g}", ESPERA); _caso(p, sal, res, f"pantalla girada {g*90}°", p.info())
    p.cmd("GIRO 0", 0.5)

def tamanos(p, sal):
    """Compara el tamaño de las criaturas (100, 120, 130 y 140 %) en el mundo, la casita, los juegos y los paneles: ms, picos y RAM de cada uno."""
    res = []
    sal.append("== TAMAÑO DE LAS CRIATURAS ==")
    p.cmd("DESBLOQUEAR", 0.5)
    base = p.info() or {}
    tam0 = base.get("tam", 100)
    r = p.cmd("PANELES", 0.2); nombres = r.split()[4:] if r else []
    for pct in (100, 120, 130, 140):
        sal.append(f"-- {pct} % --")
        p.cmd(f"TAMANO {pct}", 2)
        for z in (0, 1, 2, 3, 5):
            etapa(f"{pct} %: {NOM_ZONA[z]}"); p.cmd(f"ZONA {z}", ESPERA); _caso(p, sal, res, f"{pct}% {NOM_ZONA[z]}", p.info())
        p.cmd("ZONA 0", 2)
        etapa(f"{pct} %: casita"); p.cmd("CASITA 1", ESPERA); _caso(p, sal, res, f"{pct}% casita por dentro", p.info()); p.cmd("CASITA 2", ESPERA); _caso(p, sal, res, f"{pct}% saltando en la cama", p.info()); p.cmd("CASITA 0", 1.5)
        etapa(f"{pct} %: visita"); p.cmd("VISITA", ESPERA); _caso(p, sal, res, f"{pct}% con visita", p.info())
        etapa(f"{pct} %: Pochi"); p.cmd("POCHI 0", ESPERA); _caso(p, sal, res, f"{pct}% con Pochi", p.info()); p.cmd("POCHI 3", 1)
        for j in range(4):
            etapa(f"{pct} %: juego {NOM_JUEGO[j]}"); p.cmd("CERRAR", 1); p.cmd(f"JUEGO {j}", 3); _caso(p, sal, res, f"{pct}% juego {NOM_JUEGO[j]}", p.info()); p.cmd("CERRAR", 1)
        for k in range(len(nombres)):
            etapa(f"{pct} %: panel {nombres[k]}"); p.cmd(f"PANEL {k}", ESPERA); _caso(p, sal, res, f"{pct}% panel {nombres[k]}", p.info())
        p.cmd("CERRAR", 1)
    sal.append(""); sal.append("== RESUMEN DE TAMAÑOS ==")
    for pct in (100, 120, 130, 140):
        ok = [r for r in res if r[0].startswith(f"{pct}%") and not r[4]]
        if ok:
            sal.append(f"{pct:>3} %: promedio {sum(r[1] for r in ok)//len(ok)} ms · peor pico {max(r[2] for r in ok)} ms · RAM interna mínima {min(r[3] for r in ok)} B · casos lentos {sum(1 for r in ok if r[1] > LENTO_MS)}")
        mud = [r for r in res if r[0].startswith(f"{pct}%") and r[4]]
        if mud: sal.append(f"{pct:>3} %: SIN RESPUESTA en {len(mud)} casos: " + ", ".join(r[0] for r in mud[:6]))
    for l in sal[sal.index("== RESUMEN DE TAMAÑOS =="):]: print(l)
    p.cmd(f"TAMANO {tam0}", 1); p.cmd("ZONA 0", 1)
    sal.append(f"(el tamaño volvió a {tam0} %: elige con  comando \"TAMANO 130\"  el que prefieras)")

def reinicios(p, sal, veces=3):
    """Solo reinicia la placa varias veces (sin gastar memoria antes) y mide si vuelve: separa 'falla el reinicio por software' de 'falla por la memoria'."""
    sal.append("== REINICIOS SOLOS ==")
    for n in range(1, veces + 1):
        etapa(f"Reinicio {n} de {veces}")
        p.cmd("REINICIAR", 1.0)
        t0 = time.time()
        vuelve = False
        while time.time() - t0 < 40:
            p.leer(2)
            try:
                p.s.reset_input_buffer()
                if p.info():
                    vuelve = True
                    break
            except PuertoPerdido:
                raise
            except Exception:
                pass
        sal.append(f"[{'OK   ' if vuelve else 'FALLA'}] reinicio {n}: " + (f"volvió en {time.time() - t0:.0f} s" if vuelve else "NO volvió en 40 s (saca la batería, enchúfala y baja el registro: dice en qué paso del arranque se quedó)"))
        print(sal[-1])
        if not vuelve:
            break
        time.sleep(3)

def descarga(p, sal, largo):
    """Pone la placa en MODO PRUEBA DE DESCARGA: corre sola, sin cable, y anota todo en su registro."""
    sal.append("== PRUEBA DE DESCARGA (" + ("larga, hasta el 10 %" if largo else "corta, una vuelta de 3 etapas") + ") ==")
    r = p.cmd("DESCARGA 2" if largo else "DESCARGA 1", 1.5)
    if not r or "OK" not in r:
        sal.append("[FALLA] la placa no pudo empezar la prueba de descarga: " + (r or "no contestó") + " (la batería debe estar al 90 % o más: déjala cargar y repite)")
        print(sal[-1]); return
    sal.append("[OK   ] la placa entró en modo prueba de descarga")
    print(sal[-1])
    avisar("Prueba de descarga empezada",
           "La mascota quedó en MODO PRUEBA DE DESCARGA (pantalla negra con un círculo girando).\n\n"
           "1) Desenchufa el cable USB del computador. La prueba empieza cuando lo sacas.\n"
           "2) No uses ni toques la mascota. Déjala quieta y a temperatura normal.\n"
           "3) Duración: " + ("de 4 a 8 horas (hasta que la batería llegue al 10 %)." if largo else "1 hora aprox.") + "\n"
           "4) Para cancelarla: aprieta PWR dos veces seguidas.\n"
           "5) Cuando la pantalla vuelva a mostrar la mascota, enchúfala y usa «RESULTADO de la descarga».\n\n"
           "Si enchufas el cable a la mitad, la prueba se pausa y sigue sola cuando lo saques.")

def _kv(linea):
    return {k: int(v) for k, v in re.findall(r"(\w+)=(-?\d+)", linea)}

def descarga_resultado(p, sal):
    """Baja el registro de la placa y calcula, por etapa: consumo, autonomía, temperaturas y frecuencia."""
    sal.append("== RESULTADO DE LA PRUEBA DE DESCARGA ==")
    leer_registro(p)
    lineas = [l for l in ULTIMO_REGISTRO.split("\n") if "DESCARGA " in l]
    # solo la última prueba: desde el último «DESCARGA inicio»
    ini = [i for i, l in enumerate(lineas) if "DESCARGA inicio" in l]
    if not ini:
        sal.append("[AVISO] el registro de la placa no tiene ninguna prueba de descarga: ¿se hizo con el firmware nuevo?")
        print(sal[-1]); return
    lineas = lineas[ini[-1]:]
    for l in lineas:
        if "DESCARGA fin:" in l or "DESCARGA calor" in l or "DESCARGA:" in l or "DESCARGA inicio" in l:
            sal.append("   " + l.split("DESCARGA", 1)[1].strip().join(["DESCARGA ", ""]))
    med = [(_kv(l)) for l in lineas if re.search(r"DESCARGA (medida|etapa|fin) ", l) and "pct=" in l]
    if len(med) < 2:
        sal.append("[AVISO] hay muy pocas mediciones para calcular (la prueba se cortó muy pronto)")
        print(sal[-1]); return
    NOM = ["peor caso (brillo 100 %, CPU a tope, sensores y carga en los dos núcleos)", "uso normal (brillo 50 %, ahorro automático)", "dormida (pantalla apagada)"]
    for e in range(3):
        g = [m for m in med if m.get("etapa") == e]
        if len(g) < 2:
            continue
        dt_min = g[-1]["t"] - g[0]["t"]
        dp = g[0]["pct"] - g[-1]["pct"]
        por_h = dp * 60.0 / dt_min if dt_min > 0 else 0
        horas = 100.0 / por_h if por_h > 0 else None
        mhz = [m["mhz_medido"] for m in g if m.get("mhz_medido", -1) > 0]
        mn = [m["mhz_min"] for m in g if m.get("mhz_min", -1) > 0]
        c0 = [m["carga0"] for m in g if m.get("carga0", -1) >= 0]
        c1 = [m["carga1"] for m in g if m.get("carga1", -1) >= 0]
        tc = [m["chip"] for m in g if m.get("chip", -999) > -900]
        ti = [m["imu"] for m in g if m.get("imu", -999) > -900]
        sal.append(f"[OK   ] etapa {NOM[e]}: {dp} puntos de batería en {dt_min} min = {por_h:.1f} % por hora" + (f" → autonomía de 100 % a 0 %: {horas:.1f} h" if horas else ""))
        if mhz:
            sal.append(f"          frecuencia pedida {g[-1].get('mhz_pedido')} MHz · medida (promedio) {sum(mhz) // len(mhz)} MHz, la más baja {min(mn) if mn else '?'} MHz")
        if c0 and c1:
            sal.append(f"          carga de los núcleos: núcleo 0 {sum(c0) // len(c0)} %, núcleo 1 {sum(c1) // len(c1)} % (promedio)")
        if tc:
            sal.append(f"          temperatura: procesador máx {max(tc) / 10:.1f} °C" + (f", acelerómetro máx {max(ti) / 10:.1f} °C" if ti else ""))
        if e == 0 and mhz and sum(mhz) // len(mhz) < 230:
            sal.append("[AVISO] en el peor caso el procesador NO estuvo a 240 MHz: esta etapa no midió el peor caso real")
    for l in sal[-14:]:
        print(l)

def rendimiento(p, sal):
    """Todas las pruebas de velocidad y memoria, solas: cada zona × estación × hora, climas, niebla, casita, paneles y juegos.
    Al final hace un resumen con lo más lento, lo que más memoria gasta y si la placa se reinició."""
    res = []
    sal.append("== RENDIMIENTO ==")
    p.cmd("DESBLOQUEAR", 0.5)
    base = p.info()
    ram0 = base.get("ram", 0) if base else 0
    # 1) cada zona, en cada estación y a cada hora del día
    for z in range(6):
        for e in range(4):
            p.cmd(f"ESTACION {e}", 0.3)
            for q, nom in NOM_MOM:
                etapa(f"{NOM_ZONA[z]} · {NOM_EST[e]} · {nom}")
                p.cmd(f"MOMENTO {q}", 0.3)
                p.cmd(f"ZONA {z}", ESPERA)
                d = p.info()
                _caso(p, sal, res, f"{NOM_ZONA[z]} {NOM_EST[e][:4]} {nom}", d, [] if d and d.get("zona") == z else ["NO CAMBIÓ DE ZONA"])
    p.cmd("ESTACION 1", 0.3); p.cmd("MOMENTO MANANA", 0.3)
    # 2) el clima en el prado y en el lago
    for z in (0, 2):
        p.cmd(f"ZONA {z}", 2)
        for c in range(7):
            if z == 2 and c > 2: break
            etapa(f"clima {NOM_CLIMA[c]} en {NOM_ZONA[z]}")
            p.cmd(f"CLIMA {c}", ESPERA)
            _caso(p, sal, res, f"clima {NOM_CLIMA[c]} {NOM_ZONA[z]}", p.info())
    p.cmd("CLIMA 0", 1); p.cmd("ZONA 0", 2)
    # 3) niebla (antes estaba lentísima), la casita por dentro, la cama, la ballena y las visitas
    etapa("niebla"); p.cmd("NIEBLA 1", ESPERA); _caso(p, sal, res, "niebla de la mañana", p.info()); p.cmd("NIEBLA 0", 1)
    etapa("niebla de noche"); p.cmd("MOMENTO NOCHE", 0.3); p.cmd("NIEBLA 1", ESPERA); _caso(p, sal, res, "niebla de noche", p.info()); p.cmd("NIEBLA 0", 1); p.cmd("MOMENTO MANANA", 0.3)
    etapa("casita por dentro"); p.cmd("CASITA 1", ESPERA); d = p.info(); _caso(p, sal, res, "casita por dentro", d)
    p.cmd("CASITA 2", ESPERA); _caso(p, sal, res, "saltando en la cama", p.info())
    p.cmd("MOMENTO NOCHE", 0.3); _caso(p, sal, res, "casita de noche", p.info()); p.cmd("CASITA 0", 1.5); p.cmd("MOMENTO MANANA", 0.3)
    etapa("la ballena"); p.cmd("BALLENA", ESPERA); _caso(p, sal, res, "la ballena de nubes", p.info())
    etapa("una visita"); p.cmd("VISITA", ESPERA); _caso(p, sal, res, "llega una visita", p.info())
    etapa("Pochi"); p.cmd("POCHI 0", ESPERA); _caso(p, sal, res, "llega Pochi", p.info()); p.cmd("POCHI 3", 1)
    # 4) el lago helado (se camina por el hielo hasta la isla)
    p.cmd("ESTACION 3", 0.3); p.cmd("ZONA 2", ESPERA); _caso(p, sal, res, "lago helado", p.info()); p.cmd("ESTACION 1", 0.3); p.cmd("ZONA 0", 2)
    # 5) paneles y juegos
    r = p.cmd("PANELES", 0.2)
    nombres = r.split()[4:] if r else []
    for k in range(len(nombres)):
        etapa(f"panel {k}: {nombres[k]}")
        p.cmd(f"PANEL {k}", ESPERA)
        _caso(p, sal, res, f"panel {k} {nombres[k]}", p.info())
    p.cmd("CERRAR", 1)
    for j in range(4):
        etapa(f"juego {NOM_JUEGO[j]}"); p.cmd("CERRAR", 1); p.cmd(f"JUEGO {j}", 3)
        _caso(p, sal, res, f"juego {NOM_JUEGO[j]}", p.info()); p.cmd("CERRAR", 1)
    # 6) sensores, micrófono, música, sonidos, historias, secretos, huevo, finales, Pochi y giro
    hardware(p, sal, res)
    # resumen
    sal.append(""); sal.append("== RESUMEN DE RENDIMIENTO ==")
    ok = [r for r in res if not r[4]]
    mudas = [r for r in res if r[4]]  # (incluye los PROBLEMA de sensores, micrófono, etc.)
    paneos = [r for r in mudas if "PÁNICO" in r[0] or "watchdog" in r[0] or "brownout" in r[0] or "REINICIO" in r[0]]
    sal.append(f"pánicos / reinicios / watchdog: {len(paneos)}" + ("  ← " + "; ".join(r[0][:90] for r in paneos[:5]) if paneos else "  (ninguno)"))
    sal.append(f"casos probados: {len(res)}   sin respuesta (posible reinicio): {len(mudas)}   lentos (> {LENTO_MS} ms): {sum(1 for r in ok if r[1] > LENTO_MS)}   con poca RAM (< {RAM_MIN} B): {sum(1 for r in ok if r[3] < RAM_MIN)}")
    if ok:
        sal.append(f"RAM interna libre: al empezar {ram0} B · la menor de la prueba {min(r[3] for r in ok)} B")
        sal.append("los 10 más lentos (ms por cuadro, promedio):")
        for r in sorted(ok, key=lambda r: -r[1])[:10]:
            sal.append(f"   {r[1]:>4} ms (máx {r[2]:>4})  {r[0]}")
        sal.append("los 10 con picos más grandes (el cuadro más lento que se vio):")
        for r in sorted(ok, key=lambda r: -r[2])[:10]:
            sal.append(f"   máx {r[2]:>4} ms (prom {r[1]:>4})  {r[0]}")
        sal.append("los 10 que dejan menos RAM interna:")
        for r in sorted(ok, key=lambda r: r[3])[:10]:
            sal.append(f"   {r[3]:>6} B  {r[0]}")
    for r in mudas:
        sal.append(f"SIN RESPUESTA / PROBLEMA en: {r[0]}")
    for l in sal[sal.index("== RESUMEN DE RENDIMIENTO ==") :]:
        print(l)
    # dejar todo como estaba
    for orden in ("CASITA 0", "NIEBLA 0", "CERRAR", "ESTACION -1", "MOMENTO REAL", "CLIMA 0", "ZONA 0"):
        p.cmd(orden, 0.5)

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))  # la carpeta que tiene firmware\ y core\
FW = os.path.join(RAIZ, "firmware")

def puerto_auto():
    try:
        from serial.tools import list_ports
    except ImportError:
        return None
    cand = [p.device for p in list_ports.comports() if p.vid == 0x303A]  # Espressif
    if not cand:
        cand = [p.device for p in list_ports.comports()]
    return cand[0] if len(cand) == 1 else (cand[0] if cand else None)

def pedir_puerto():
    p = puerto_auto()
    r = input(f"Puerto de la placa [{p or 'COM5'}]: ").strip()
    return r or p or "COM5"

def buscar_zip():
    carpetas = [os.getcwd(), RAIZ, os.path.join(os.path.expanduser("~"), "Downloads"), os.path.join(os.path.expanduser("~"), "Descargas"),
                os.path.join(os.path.expanduser("~"), "Desktop")]
    hallados = []
    for c in carpetas:
        hallados += glob.glob(os.path.join(c, "mascota_firmware*.zip"))
    hallados = sorted(set(hallados), key=os.path.getmtime, reverse=True)
    return hallados[0] if hallados else None

def idf(args, cwd):
    print("\n> idf.py " + " ".join(args))
    try:
        return subprocess.call(["idf.py"] + args, cwd=cwd, shell=(os.name == "nt")) == 0
    except FileNotFoundError:
        print("No encuentro idf.py: abre este programa desde la consola 'ESP-IDF PowerShell'.")
        return False

def sha(ruta):
    try:
        return hashlib.sha256(open(ruta, "rb").read()).hexdigest()
    except OSError:
        return ""

def extraer_zip():
    z = buscar_zip()
    if not z:
        print("No encontré ningún mascota_firmware.zip (busqué en Descargas, Escritorio y esta carpeta).")
        return False
    print("Usando", z, time.strftime("(%d-%m %H:%M)", time.localtime(os.path.getmtime(z))))
    antes = sha(os.path.join(FW, "sdkconfig.defaults"))
    with zipfile.ZipFile(z) as zf:
        miembros = zf.infolist()
        total = max(1, sum(m.file_size for m in miembros))
        hecho = 0
        for m in miembros:  # archivo por archivo, para poder mostrar el avance (copiar 90 MB sin barra parecía colgado)
            if CANCELAR:
                raise Cancelado()
            zf.extract(m, RAIZ)
            hecho += m.file_size
            if EXTRAER:
                EXTRAER(100 * hecho // total)
    if sha(os.path.join(FW, "sdkconfig.defaults")) != antes:
        print("Cambió la configuración: se regenera (set-target).")
        for f in ("sdkconfig", "sdkconfig.old"):
            try:
                os.remove(os.path.join(FW, f))
            except OSError:
                pass
        shutil.rmtree(os.path.join(FW, "build"), ignore_errors=True)
    return True

def diagnosticar_memoria():
    """Si el programa no cabe en la RAM interna, muestra qué ocupa más (lee build\\mascota.map) y si la PSRAM está bien configurada."""
    import re
    print("\n--- DIAGNÓSTICO DE MEMORIA ---")
    try:
        for l in open(os.path.join(FW, "sdkconfig"), encoding="utf-8", errors="replace"):
            if re.match(r"#? ?CONFIG_(SPIRAM_ALLOW_BSS_SEG_EXTERNAL_MEMORY|SPIRAM_RODATA|SPIRAM)[ =]", l):
                print("sdkconfig:", l.strip())
    except OSError:
        print("(no encuentro firmware\\sdkconfig)")
    try:
        t = open(os.path.join(FW, "build", "mascota.map"), encoding="utf-8", errors="replace").read()
    except OSError:
        print("(no hay build\\mascota.map)")
        return
    # entradas de la RAM interna de datos (0x3fc8xxxx a 0x3fcfxxxx): .data y .bss
    pat = re.compile(r"^ (\.\S+)\s*\n?\s+(0x0*3fc[89a-f][0-9a-f]{4})\s+(0x[0-9a-f]+)\s+(\S+)", re.M)
    filas = []
    for m in pat.finditer(t):
        n = int(m.group(3), 16)
        if n >= 1500:
            filas.append((n, m.group(1), m.group(4)))
    filas.sort(reverse=True)
    print("Lo más grande en la RAM interna (bytes, nombre, archivo):")
    for n, nom, arc in filas[:30]:
        print("%8d  %s  %s" % (n, nom, arc.split("/")[-1]))
    print("Total de esas entradas:", sum(f[0] for f in filas))

def sdkconfig_al_dia():
    """¿El sdkconfig tiene lo esencial de sdkconfig.defaults (PSRAM, LVGL sin hilos)? Si no, estaba generado sin la configuración y la RAM no alcanza."""
    try:
        t = open(os.path.join(FW, "sdkconfig"), encoding="utf-8", errors="replace").read()
    except OSError:
        return False
    return all(("\n" + c + "\n") in t for c in ("CONFIG_SPIRAM=y", "CONFIG_SPIRAM_ALLOW_BSS_SEG_EXTERNAL_MEMORY=y", "CONFIG_LV_OS_NONE=y", "CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH=y"))

def respaldar_elf():
    """Después de compilar: guarda una copia de build\\mascota.elf en Documentos\\Respaldos mascota\\elf, con la fecha y los dígitos que la placa imprime en un fallo
    («ELF file SHA256: …»), para poder ubicar la línea exacta de un fallo viejo aunque ya se haya compilado otra versión."""
    try:
        elf = os.path.join(FW, "build", "mascota.elf")
        if not os.path.exists(elf):
            return
        sha = hashlib.sha256(open(elf, "rb").read()).hexdigest()[:9]   # son los mismos dígitos que la placa imprime en un fallo
        base = os.path.join(os.path.expanduser("~"), "Documents", "Respaldos mascota", "elf")
        os.makedirs(base, exist_ok=True)
        dest = os.path.join(base, "mascota_%s_sha%s.elf" % (time.strftime("%Y-%m-%d_%H-%M"), sha))
        if any(f.endswith("_sha%s.elf" % sha) for f in os.listdir(base)):
            print("(el .elf de esta compilación ya estaba respaldado: sha %s)" % sha)
            return
        shutil.copyfile(elf, dest)
        print("Copia del .elf guardada (para decodificar fallos de esta versión): " + dest)
    except Exception as e:
        print("(no pude respaldar el .elf:", e, ")")

def compilar():
    if os.path.exists(os.path.join(FW, "sdkconfig")) and not sdkconfig_al_dia():
        print("El sdkconfig no trae la configuración del proyecto (PSRAM, etc.): se regenera desde cero.")
        for f in ("sdkconfig", "sdkconfig.old"):
            try:
                os.remove(os.path.join(FW, f))
            except OSError:
                pass
        shutil.rmtree(os.path.join(FW, "build"), ignore_errors=True)
    if not os.path.exists(os.path.join(FW, "sdkconfig")) and not idf(["set-target", "esp32s3"], FW):
        return False
    if not sdkconfig_al_dia():
        print("AVISO: después de set-target el sdkconfig sigue sin CONFIG_SPIRAM=y; mira que firmware\\sdkconfig.defaults esté en la carpeta.")
    ok = idf(["build"], FW)
    if ok:
        respaldar_elf()
    if not ok:
        try:
            diagnosticar_memoria()
        except Exception as e:
            print("(no pude diagnosticar la memoria:", e, ")")
    return ok

def pedir_descarga(puerto):
    """Le pide a la placa (si está en marcha con el firmware nuevo) que se reinicie en modo descarga. True si lo aceptó."""
    try:
        s = serial.Serial()
        s.port, s.baudrate, s.timeout = puerto, 115200, 0.3
        s.dtr = False
        s.rts = False
        s.open()
    except Exception:
        return False
    ok = False
    try:
        s.write(b"MASCOTA DESCARGA\n")
        fin = time.time() + 3
        buf = b""
        while time.time() < fin and not ok:
            buf += s.read(200)
            ok = b"modo descarga" in buf
    except Exception:
        pass
    try:
        s.close()
    except Exception:
        pass
    if ok:
        time.sleep(3)  # reaparece el puerto, ahora en modo descarga
    return ok

def placa_responde(puerto, seg=12):
    """¿La placa arrancó y manda texto por el puerto?"""
    fin = time.time() + seg
    while time.time() < fin:
        try:
            s = serial.Serial()
            s.port, s.baudrate, s.timeout = puerto, 115200, 0.3
            s.dtr = False
            s.rts = False
            s.open()
        except Exception:
            time.sleep(0.5)
            continue
        try:
            t_fin = time.time() + 3
            while time.time() < t_fin:
                if s.read(100):
                    s.close()
                    return True
        except Exception:
            pass
        try:
            s.close()
        except Exception:
            pass
    return False

def enviar_recursos(puerto, seg=20):
    """(1.0.17) Copia a la memoria de recursos de la placa los archivos de firmware/recursos (los fondos de los juegos de a dos)
    que le falten o estén distintos. Solo manda lo que cambió (la placa dice qué tiene). Devuelve cuántos mandó, o -1 si falló."""
    import base64, glob, zlib
    archivos = sorted(glob.glob(os.path.join(FW, "recursos", "pj*.bin")))
    if not archivos:
        return 0
    fin = time.time() + seg
    s = None
    while time.time() < fin and s is None:
        try:
            s = serial.Serial()
            s.port, s.baudrate, s.timeout = puerto, 115200, 0.3
            s.dtr = False
            s.rts = False
            s.open()
        except Exception:
            s = None
            time.sleep(0.5)
    if s is None:
        print("No pude abrir el puerto para mandar los fondos.")
        return -1
    def pedir(linea, espera, t=6.0):
        s.reset_input_buffer()
        s.write((linea + "\n").encode())
        t_fin, buf = time.time() + t, b""
        while time.time() < t_fin:
            buf += s.read(400)
            for l in buf.decode("utf-8", "replace").splitlines():
                if l.startswith(espera) or l.startswith("MASCOTA ERROR"):
                    return l.strip()
        return ""
    mandados = 0
    try:
        for ruta in archivos:
            nom = os.path.basename(ruta)
            datos = open(ruta, "rb").read()
            crc = "%08x" % zlib.crc32(datos)
            r = pedir(f"MASCOTA ARCHIVO {nom} TIENE", f"MASCOTA ARCHIVO {nom}")
            if r.split()[3:5] == [str(len(datos)), crc]:
                continue
            print(f"Mandando {nom} ({len(datos) // 1024} KB) a la memoria de recursos…")
            if not pedir(f"MASCOTA ARCHIVO {nom} INICIO {len(datos)}", "MASCOTA OK").startswith("MASCOTA OK"):
                print(f"  la placa no aceptó {nom}: {r}")
                return -1
            for i in range(0, len(datos), 2400):
                b = base64.b64encode(datos[i:i + 2400]).decode()
                if not pedir(f"MASCOTA ARCHIVO {nom} DATOS {i} {b}", "MASCOTA OK").startswith("MASCOTA OK"):
                    print(f"  se cortó al mandar {nom}")
                    return -1
            if not pedir(f"MASCOTA ARCHIVO {nom} FIN {crc}", "MASCOTA OK", 15.0).startswith("MASCOTA OK"):
                print(f"  {nom} no llegó completo")
                return -1
            mandados += 1
    finally:
        s.close()
    print(f"Fondos de los juegos de a dos: {mandados} enviados" if mandados else "Fondos de los juegos de a dos: ya estaban")
    return mandados

def poner_hora(puerto, seg=15):
    """Le pone a la placa la hora y la fecha de este computador (con su zona horaria). Reintenta unos segundos: acaba de arrancar."""
    ahora = int(time.time())
    zona = time.localtime(ahora).tm_gmtoff or 0
    fin = time.time() + seg
    while time.time() < fin:
        try:
            s = serial.Serial()
            s.port, s.baudrate, s.timeout = puerto, 115200, 0.3
            s.dtr = False
            s.rts = False
            s.open()
        except Exception:
            time.sleep(0.5)
            continue
        try:
            s.write(f"MASCOTA HORA {int(time.time())} {zona}\n".encode())
            t_fin = time.time() + 2.5
            buf = b""
            while time.time() < t_fin:
                buf += s.read(200)
                if b"HORA OK" in buf:
                    s.close()
                    print(time.strftime("Hora y fecha puestas en la placa: %d-%m-%Y %H:%M"))
                    return True
        except Exception:
            pass
        try:
            s.close()
        except Exception:
            pass
    print("(No pude poner la hora en la placa: ¿arrancó ya? Se puede repetir con «Ver estado»)")
    return False

def contenido_lista(puerto, tipo):
    """Los nombres de un tipo de contenido (zonas, objetos...) tal como los conoce la placa; None si no contestó."""
    p = Placa(puerto)
    try:
        r = p.cmd("LISTA " + tipo, 0.1)
    finally:
        p.s.close()
    if not r or " OK " not in r:
        return None
    return r.split(" OK ", 1)[1].split("|")[1:]

def contenido_aplicar(puerto, tipo, idx, verbo="ABRIR"):
    """Abre un contenido (idx = -1: todos) o cambia la estación / el clima. Devuelve el texto de la placa."""
    orden = {"estacion": f"ESTACION {idx}", "clima": f"CLIMA {idx}"}.get(tipo, f"{verbo} {tipo} {idx}")
    p = Placa(puerto)
    try:
        r = p.cmd(orden, 0.3)
    finally:
        p.s.close()
    return r

def duracion(seg):
    seg = int(seg + 0.5)
    return f"{seg // 60} min {seg % 60:02d} s" if seg >= 60 else f"{seg} s"

puerto_global = [None]
def grabar(puerto):
    puerto_global[0] = puerto
    input("\nPON LA PLACA EN MODO DESCARGA: mantén apretado BOOT, conecta el USB, suelta BOOT a los 2 segundos. Luego Enter... ")
    ok = idf(["-p", puerto, "flash"], FW)
    if ok:
        input("\nGrabado. Ahora DESCONECTA el USB, espera 5 segundos, vuelve a conectarlo SIN tocar botones, y presiona Enter... ")
        time.sleep(3)
        poner_hora(puerto_global[0]) if puerto_global[0] else None
    return ok

def instalar(puerto):
    print("\n=== INSTALAR FIRMWARE NUEVO ===")
    if not extraer_zip() or not compilar():
        print("\nNo se pudo compilar. Copia lo de arriba y mándamelo.")
        return False
    return grabar(puerto)

def correr(puerto, que, extra=None):
    global sys
    argv = ["probar.py", puerto, que] + ([extra] if extra is not None else [])
    viejo, sys.argv = sys.argv, argv
    try:
        ejecutar()
    except SystemExit as e:
        if e.code:
            print(e.code)
    except Cancelado:
        print("Prueba detenida.")
    finally:
        sys.argv = viejo

def menu():
    puerto = pedir_puerto()
    while True:
        print("""
  1  Instalar firmware nuevo (usa el zip más reciente: extrae, compila y graba)
  2  Probar todo (paneles, juegos, zonas, secretos, finales)
  3  Probar solo juegos
  A  Probar actividades y modo cantar
  4  Probar solo zonas, secretos y finales
  5  Probar solo paneles
  6  Ver el estado de la placa
  G  PRUEBA TOTAL + guiada (además te pide tocar la pantalla y enchufar el cargador)
  T  PRUEBA TOTAL automática (pantalla, volumen, sensores, sonidos, objetos, animales, cueva, ahorro de energía, estrés... y baja el registro)
  9  RENDIMIENTO automático (zonas, estaciones, horas, climas, niebla, casita, paneles, juegos)
  N  Probar LO NUEVO 1.0.16-1.0.23 (guiada: te pide soplar)   M  lo mismo, sin manos
  7  Cambiar de puerto
  8  Restaurar la partida a como estaba antes de una prueba
  0  Salir""")
        o = input("Elige: ").strip()
        if o == "1": instalar(puerto)
        elif o == "2": correr(puerto, "todo")
        elif o == "3": correr(puerto, "juegos")
        elif o.upper() == "A":
            for q in ("actividades", "cantar"): correr(puerto, q)
        elif o == "4":
            for q in ("zonas", "secretos", "finales"): correr(puerto, q)
        elif o == "5": correr(puerto, "paneles")
        elif o == "6": correr(puerto, "info")
        elif o == "9": correr(puerto, "rendimiento")
        elif o.upper() == "N": correr(puerto, "nuevo")
        elif o.upper() == "M": correr(puerto, "nuevo_auto")
        elif o.upper() == "T": correr(puerto, "total")
        elif o.upper() == "G": correr(puerto, "total_guiado")
        elif o == "7": puerto = pedir_puerto()
        elif o == "8": correr(puerto, "restaurar")
        elif o == "0": return

ULTIMO_REGISTRO = ""


def leer_registro(p, borrar=False):
    """Pide a la placa su registro guardado (arranques, motivos de reinicio, avisos, latidos) y lo guarda en registro_placa.txt."""
    p.s.reset_input_buffer()
    if borrar:
        p.s.write(b"MASCOTA REGISTRO BORRAR\n")
        ok = [l for l in p.leer(3) if "MASCOTA OK" in l]
        print("Registro de la placa borrado." if ok else "La placa no contestó (¿firmware antiguo, sin registro?).")
        return
    p.s.write(b"MASCOTA REGISTRO\n")
    lineas, inicio, fin = [], False, time.time() + 20
    while time.time() < fin:
        for l in p.leer(0.5):
            if "MASCOTA REGISTRO INICIO" in l:
                inicio = True
            elif "MASCOTA REGISTRO FIN" in l:
                fin = 0
            elif "MASCOTA REGISTRO |" in l:
                lineas.append(l.split("MASCOTA REGISTRO |", 1)[1].strip())
    if not inicio:
        print("La placa no contestó al registro. ¿Tiene el firmware nuevo y está en marcha (sin modo descarga)?")
        return
    carpeta = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    ruta = os.path.join(carpeta, "registro_placa_" + time.strftime("%Y-%m-%d_%H-%M-%S") + ".txt")
    with open(ruta, "w", encoding="utf-8") as f:
        f.write("# registro pedido el " + time.strftime("%d-%m-%Y a las %H:%M:%S") + "\n")
        f.write("\n".join(lineas) + "\n")
    try: shutil.copy(ruta, carpeta_datos())  # copia a Documentos: sobrevive a borrar C:\mascota
    except Exception: pass
    global ULTIMO_REGISTRO
    ULTIMO_REGISTRO = "\n".join(lineas) + "\n"
    print(f"Registro de la placa: {len(lineas)} líneas (también guardado en {ruta}). Todo el registro:")
    for l in lineas:
        print("   " + l)
    arranques = [l for l in lineas if "ARRANQUE" in l]
    if arranques:
        print(f"Arranques anotados: {len(arranques)}. El último: {arranques[-1]}")


def ejecutar():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    p = Placa(sys.argv[1])
    que = sys.argv[2] if len(sys.argv) > 2 else "todo"
    sal = [time.strftime("Prueba %Y-%m-%d %H:%M:%S")]
    if que in ("registro", "registro_borrar"):
        leer_registro(p, borrar=(que == "registro_borrar"))
        return
    if que == "de_cero":
        p.s.reset_input_buffer()
        p.s.write(b"MASCOTA EMPEZAR DE CERO\n")
        ok = [l for l in p.leer(4) if "MASCOTA OK" in l]
        print("Partida borrada: la placa se reinicia y nace una mascota nueva." if ok else
              "La placa no contestó (¿firmware antiguo? instala el nuevo primero).")
        return
    if que == "escuchar":
        print("Escuchando a la placa 10 segundos (sale el texto que ella misma manda)...")
        oido = p.leer(10)
        for l in oido:
            print("   " + l[:220])
        print("(nada)" if not oido else f"({len(oido)} líneas)")
        return
    d = p.info()
    if d is None:
        print("La placa no responde a los comandos. Escuché esto durante 6 segundos:")
        oido = p.leer(6)
        for l in oido[-25:]:
            print("   " + l[:200])
        if not oido:
            print("   (nada: la placa no manda texto; puede estar en modo descarga, apagada o con otro programa usando el puerto)")
        print("Probando si la placa lee lo que le mando...")
        for orden in ("MASCOTA PING", "MASCOTA HOLA", "MASCOTA PROBAR INFO"):
            p.s.write((orden + "\n").encode())
            resp = [l for l in p.leer(3) if "MASCOTA" in l]
            print(f"   {orden!r} -> " + (resp[0][:160] if resp else "(sin respuesta)"))
            if orden == "MASCOTA PING" and not resp:
                print("   Puede que la placa no esté recibiendo nada por el cable, o que tenga un firmware anterior sin PING. Sigo probando...")
            if orden == "MASCOTA PING" and resp:
                print("   La placa SÍ recibe por el cable (contesta el PING).")
            if orden == "MASCOTA HOLA" and not resp:
                print("   La placa recibe pero su tarea principal no contesta (o no recibe nada).")
                break
            if orden == "MASCOTA PROBAR INFO" and resp and "desconocido" in resp[0]:
                print("   La placa recibe, pero tiene un firmware SIN los comandos de prueba: instala el firmware más nuevo.")
        print("Contadores de la placa después de mis comandos:")
        for l in p.leer(4):
            if "velocidad" in l:
                print("   " + l[:240])
        sys.exit("¿Está el juego en marcha (sin modo descarga), el monitor cerrado y el firmware nuevo instalado?")
    if que == "restaurar":
        r = p.cmd("RESTAURAR", 1)
        print("Partida devuelta a como estaba antes de la prueba." if r and "ERROR" not in r else "No hay nada que restaurar (" + str(r) + ").")
        return
    p.s.close(); poner_hora(sys.argv[1], 6); p.s.open()  # la hora del computador, por si la placa la perdió
    sal.append(linea("inicio", d)); print(sal[-1])
    _hecho[0] = 0
    r_pan = p.cmd("PANELES", 0.2)
    n_pan = max(1, len(r_pan.split()[4:])) if r_pan else 67  # los paneles que tiene ESTA placa
    n_act = len(NOM_ACT)
    _total[0] = {"todo": n_pan + 4 + 6 + 4 + 2 + n_act + 4, "paneles": n_pan, "juegos": 4, "zonas": 6, "secretos": 4, "finales": 2, "panel": 1, "sonido": 3,
                 "actividades": n_act, "cantar": 4, "rendimiento": RENDIMIENTO_PASOS, "tamano": 330, "total": 640 + n_pan, "total_guiado": 640 + n_pan, "total_continuar": 640 + n_pan, "nuevo": 60, "nuevo_auto": 60}.get(que, 1)
    hubo = False
    try:
        if que in ("todo", "paneles", "juegos", "zonas", "secretos", "finales", "panel", "actividades", "cantar", "rendimiento", "tamano", "total", "total_guiado", "total_continuar", "nuevo", "nuevo_auto"):
            hubo = desbloquear_todo(p)
        if que in ("todo", "paneles"): paneles(p, sal)
        if que in ("todo", "juegos"): juegos(p, sal)
        if que in ("todo", "zonas"): zonas(p, sal)
        if que in ("todo", "secretos"): secretos(p, sal)
        if que in ("todo", "finales"): finales(p, sal)
        if que in ("todo", "actividades"): actividades(p, sal)
        if que in ("todo", "cantar"): cantar(p, sal)
        if que == "rendimiento": rendimiento(p, sal)
        if que in ("nuevo", "nuevo_auto"):  # (1.0.23) lo de las versiones 1.0.16 a 1.0.23
            import prueba_nuevo
            p.cmd("MARCA PRUEBA LO NUEVO INICIO", 0.1)
            c, f = prueba_nuevo.correr(p, sal, sys.modules[__name__], guiado=(que == "nuevo"))
            sal.append(""); sal.append(f"== RESUMEN DE LO NUEVO: {c['OK']} OK, {c['AVISO']} AVISOS, {c['FALLA']} FALLAS ==")
            for x in f: sal.append("   FALLA: " + x)
            print(sal[-1 - len(f)])
            p.cmd("MARCA PRUEBA LO NUEVO FIN", 0.1)
            etapa("Bajando el registro de la placa")
            leer_registro(p)  # queda en registro_placa_*.txt: pásamelo junto con informe.txt
        if que == "descarga_corta": descarga(p, sal, False)
        if que == "descarga_larga": descarga(p, sal, True)
        if que == "descarga_resultado": descarga_resultado(p, sal)
        if que == "reinicios":
            reinicios(p, sal)
            etapa("Bajando el registro de la placa")
            leer_registro(p)
        if que == "tamano": tamanos(p, sal)
        if que in ("total", "total_guiado", "total_continuar"):
            import prueba_total
            p.cmd("MARCA PRUEBA TOTAL " + ("CONTINUA" if que == "total_continuar" else "INICIO"), 0.1)
            prueba_total.correr_total(p, sal, sys.modules[__name__], guiado=(que == "total_guiado"), reanudar=(que == "total_continuar"))
            etapa("Bajando el registro de la placa")
            leer_registro(p)  # queda en registro_placa_*.txt: pásamelo junto con informe.txt
        if que == "sonido": sonido(p, sal)
        if que == "panel": paneles(p, sal, int(sys.argv[3]))
        if que == "comando":
            print(p.cmd(sys.argv[3], 2))
        if que == "info": print(p.info())
    except Cancelado:
        global CANCELAR
        CANCELAR = False
        terminar_desbloqueo(p, hubo)
        raise
    except PuertoPerdido as e:
        sal.append("[FALLA] la prueba se cortó: " + str(e) + " (última RAM libre en el informe de arriba)")
        print(sal[-1])
    except Exception:
        try: terminar_desbloqueo(p, hubo)
        except Exception: pass
        raise
    if p.extra:
        sal.append("== ERRORES Y AVISOS DE LA PLACA ==")
        vistos = set()
        for l in p.extra:
            if l not in vistos:
                vistos.add(l); sal.append(l)
        print("\n== ERRORES Y AVISOS DE LA PLACA ==")
        for l in sal[sal.index("== ERRORES Y AVISOS DE LA PLACA ==") + 1:][:40]:
            print("   " + l[:200])
    else:
        sal.append("Sin errores ni avisos de la placa.")
    open("informe.txt", "w", encoding="utf-8").write("\n".join(sal) + "\n")
    try:  # copia a Documentos\\MascotaPruebas: la última y una con fecha
        d = carpeta_datos()
        open(os.path.join(d, "informe.txt"), "w", encoding="utf-8").write("\n".join(sal) + "\n")
        open(os.path.join(d, "informe_" + time.strftime("%Y-%m-%d_%H-%M-%S") + ".txt"), "w", encoding="utf-8").write("\n".join(sal) + "\n")
        print("Copia del informe en: " + d)
    except Exception:
        pass
    print("\nListo: informe.txt")
    try: terminar_desbloqueo(p, hubo)
    except PuertoPerdido: print("No pude devolver la mascota a su estado: se restaura sola al reiniciar con 'RESTAURAR'.")

if __name__ == '__main__':
    menu() if len(sys.argv) < 2 else ejecutar()
