import time
#!/usr/bin/env python3
"""Programa con ventana para instalar el firmware de la mascota y probarla por el cable USB.
Se abre con "Abrir probador.bat" (en la carpeta C:\\mascota). No hace falta escribir nada."""
import os, re, sys, glob, queue, threading, subprocess, time, io, traceback
import tkinter as tk
from tkinter import ttk, messagebox, filedialog, scrolledtext

AQUI = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, AQUI)
try:
    import serial  # noqa
except ImportError:
    subprocess.call([sys.executable, "-m", "pip", "install", "pyserial"])
import probar  # el mismo motor de pruebas, ahora con botones

PROYECTO = os.path.dirname(AQUI)  # C:\\mascota
ORDEN = os.path.join(PROYECTO, "orden_claude.txt")        # Claude deja aquí las órdenes
RESULTADO = os.path.join(PROYECTO, "resultado_claude.txt")  # y aquí queda lo que salió
ESTADO = os.path.join(PROYECTO, "estado_claude.txt")        # señal de vida: «estoy atendiendo»

ANSI = re.compile(r"\x1b\[[0-9;?]*[A-Za-z]")

def buscar_idf():
    """Dónde está ESP-IDF (carpeta con export.ps1) y sus herramientas."""
    cands = []
    if os.environ.get("IDF_PATH"):
        cands.append(os.environ["IDF_PATH"])
    cands += sorted(glob.glob(r"C:\esp\*\esp-idf"), reverse=True)
    cands += sorted(glob.glob(r"C:\Espressif\frameworks\esp-idf-*"), reverse=True)
    cands += sorted(glob.glob(os.path.expanduser(r"~\esp\*\esp-idf")), reverse=True)
    idf = next((c for c in cands if os.path.exists(os.path.join(c, "export.ps1"))), None)
    tools = os.environ.get("IDF_TOOLS_PATH") or next((t for t in (r"C:\Espressif\tools", r"C:\Espressif") if os.path.isdir(t)), r"C:\Espressif\tools")
    return idf, tools

def buscar_activador():
    """El script que prepara ESP-IDF en 'ESP-IDF PowerShell' (el instalador lo deja en C:\\Espressif)."""
    raices = [r"C:\Espressif", os.path.expanduser(r"~\.espressif"), os.path.expanduser(r"~\Espressif")]
    patron = re.compile(r"(activate_idf.*|.*PowerShell_profile|Initialize-Idf.*)\.ps1$", re.I)
    saltar = {"python", "frameworks", "dist", "tools"}
    hallados = []
    for raiz in raices:
        if not os.path.isdir(raiz):
            continue
        for dirpath, dirs, files in os.walk(raiz):
            prof = dirpath[len(raiz):].count(os.sep)
            if prof >= 1:  # dentro de subcarpetas solo se mira «tools» y las de configuración
                dirs[:] = [d for d in dirs if d.lower() not in saltar - {"tools"} and prof < 2]
            for f in files:
                if patron.match(f):
                    ruta = os.path.join(dirpath, f)
                    try:
                        t = open(ruta, encoding="utf-8-sig", errors="replace").read()
                    except OSError:
                        continue
                    if "IDF_PATH" in t:
                        hallados.append(ruta)
    hallados.sort(key=lambda r: (0 if "activate_idf" in r.lower() else 1, -os.path.getmtime(r)))
    return hallados[0] if hallados else None

class BarraTexto(tk.Canvas):
    """Barra de progreso con el texto de lo que se hace escrito encima."""
    def __init__(self, padre, ancho=520, alto=26):
        super().__init__(padre, width=ancho, height=alto, highlightthickness=1, highlightbackground="#9a9a9a", bg="#e6e6e6")
        self.ancho, self.alto, self.valor, self.texto = ancho, alto, 0, ""
        self.bind("<Configure>", self.redibujar)
    def poner(self, valor=None, texto=None):
        if valor is not None:
            self.valor = max(0, min(100, valor))
        if texto is not None:
            self.texto = texto
        self.redibujar()
    def redibujar(self, e=None):
        if e is not None:
            self.ancho = e.width
        self.delete("all")
        w = int(self.ancho * self.valor / 100)
        self.create_rectangle(0, 0, w, self.alto, fill="#3fb950", width=0)
        t = f"{self.texto} · {int(self.valor)} %" if self.texto else f"{int(self.valor)} %"
        self.create_text(self.ancho // 2, self.alto // 2, text=t, fill="#111111", font=("Segoe UI", 10, "bold"))

class BarraBateria(tk.Canvas):
    """La batería de la placa en vivo, chiquita (cabe en la fila de arriba): barra, %, voltaje y si carga."""
    def __init__(self, padre, ancho=330, alto=24):
        super().__init__(padre, width=ancho, height=alto, highlightthickness=1, highlightbackground="#9a9a9a", bg="#e6e6e6")
        self.ancho, self.alto = ancho, alto
        self.poner(None)
    def poner(self, d, nota=""):
        self.delete("all")
        if not d or d.get("pct", -1) < 0:
            self.create_text(self.ancho // 2, self.alto // 2, text="Batería: " + (nota or "sin datos"), fill="#555555", font=("Segoe UI", 9))
            return
        pct = max(0, min(100, d["pct"]))
        if d.get("llena"): color, estado = "#2ea043", "CARGA COMPLETA"
        elif d.get("carga"): color, estado = "#d4a017", "cargando"
        elif d.get("vbus"): color, estado = "#d4a017", "enchufada"
        else: color, estado = ("#d1242f" if pct < 20 else "#3fb950"), "usando pila"
        self.create_rectangle(0, 0, int(self.ancho * pct / 100), self.alto, fill=color, width=0)
        txt = f"{pct} %  ·  {d.get('mv', 0)} mV  ·  {estado}" + (f"  ·  medidor {d['crudo']} %" if d.get("crudo", -1) >= 0 and d.get("crudo") != pct else "")
        self.create_text(self.ancho // 2, self.alto // 2, text=txt, fill="#111111", font=("Segoe UI", 9, "bold"))

class Plegable(ttk.Frame):
    """(105) Un cuadro de botones que se abre y se cierra tocando su título: así los botones no se comen el registro."""

    def __init__(self, padre, titulo, nota="", abierto=False, registro=None):
        super().__init__(padre)
        self.pack(fill="x", padx=6, pady=2)
        self.titulo, self._abierto = titulo, bool(abierto)
        self.cab = tk.Label(self, anchor="w", padx=6, pady=4, font=("Segoe UI", 9, "bold"),
                            bg="#DFE3EC", fg="#15181F", cursor="hand2")
        self.cab.pack(fill="x")
        self.cab.bind("<Button-1>", lambda e: self.alternar())
        self.cab.bind("<Enter>", lambda e: self.cab.config(bg="#CFD6E6"))
        self.cab.bind("<Leave>", lambda e: self.cab.config(bg="#DFE3EC"))
        self.cuerpo = ttk.Frame(self, padding=(10, 4, 6, 6))
        if nota:
            ttk.Label(self.cuerpo, text=nota, wraplength=820, justify="left", foreground="#44485A").pack(anchor="w", pady=(0, 4))
        if registro is not None:
            registro.append(self)
        self.pintar()

    def pintar(self):
        self.cab.config(text=("▾  " if self._abierto else "▸  ") + self.titulo)
        if self._abierto:
            self.cuerpo.pack(fill="x")
        else:
            self.cuerpo.pack_forget()

    def alternar(self, abrir=None):
        self._abierto = (not self._abierto) if abrir is None else bool(abrir)
        self.pintar()


class App(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("Mascota mágica · instalar y probar")
        self.geometry("1000x760")
        self.minsize(760, 560)
        self.cola = queue.Queue()
        self.hilo = None
        self.proc = None
        self.idf_path, self.idf_tools = buscar_idf()
        self.activador = buscar_activador()

        arriba = ttk.Frame(self, padding=8); arriba.pack(fill="x")
        ttk.Label(arriba, text="Puerto de la placa:").pack(side="left")
        self.puerto = tk.StringVar()
        self.combo = ttk.Combobox(arriba, textvariable=self.puerto, width=10)
        self.combo.pack(side="left", padx=6)
        ttk.Button(arriba, text="Buscar", command=self.buscar_puertos).pack(side="left")
        ttk.Button(arriba, text="Ver modo", command=self.ver_modo).pack(side="left")
        self.modo = ttk.Label(arriba, text="Modo: sin revisar", font=("Segoe UI", 9, "bold"))
        self.modo.pack(side="left", padx=6)
        ttk.Button(arriba, text="Sacar de modo boot", command=self.sacar_modo_boot).pack(side="left", padx=6)
        self.bateria = BarraBateria(arriba); self.bateria.pack(side="left", padx=10)  # la batería, siempre a la vista
        self.estado = ttk.Label(arriba, text="")
        self.estado.pack(side="right")
        self.var_remoto = tk.BooleanVar(value=True)
        ttk.Checkbutton(arriba, text="Atender a Claude", variable=self.var_remoto).pack(side="right", padx=8)

        # (105) las opciones van en pestañas POR FUNCIÓN y, dentro de cada una, en cuadros PLEGABLES: lo que se lee
        # es el registro de abajo, así que los botones ocupan solo lo que haga falta. Se abren tocando su título.
        self.plegables = []
        cab = ttk.Frame(self, padding=(8, 2, 8, 0)); cab.pack(fill="x")
        ttk.Label(cab, text="Toca el título de cada cuadro para abrirlo o cerrarlo:").pack(side="left")
        ttk.Button(cab, text="Plegar todo", command=lambda: self.plegar_todo(False)).pack(side="left", padx=6)
        ttk.Button(cab, text="Abrir todo", command=lambda: self.plegar_todo(True)).pack(side="left")
        self.nb = ttk.Notebook(self); self.nb.pack(fill="x", padx=8, pady=2)
        self.botones = []

        # ---------- 1. la placa
        t_placa = self.hoja("Placa y registro")
        g = self.plegable(t_placa, "1 · Instalar firmware", "Te pregunta qué zip instalar; después extrae, compila y graba. No toca la partida.", abierto=True)
        self.b_inst = ttk.Button(g, text="Instalar firmware nuevo", command=self.instalar); self.b_inst.pack(side="left")
        self.botones.append(self.b_inst)
        g = self.plegable(t_placa, "2 · Revisar que la placa está bien (rápido)", "Primero esto: ves el estado y compruebas que reinicia sin colgarse.")
        self.rejilla(g, (("Ver estado", "info"), ("Solo REINICIOS (3 veces)", "reinicios")), self.probar)
        g = self.plegable(t_placa, "3 · Registro y fallos", "Después de cualquier prueba: baja el registro de la placa (se copia solo) y pégalo en el chat.")
        self.rejilla(g, (("Obtener log del dispositivo", "registro"), ("Escuchar a la placa", "escuchar"), ("Borrar registro", "registro_borrar")), self.probar, por_fila=3)
        self.rejilla(g, (("Informe completo del fallo", self.informe_fallo), ("Decodificar último fallo", self.decodificar)), None, por_fila=2)
        g = self.plegable(t_placa, "4 · Dejar todo en orden", "Devuelve la mascota a como estaba o limpia. «Empezar de cero» BORRA la partida.")
        self.rejilla(g, (("Restaurar partida", "restaurar"), ("Empezar de cero", "de_cero")), self.probar)

        # ---------- 2. las pruebas
        t_prue = self.hoja("Pruebas")
        g = self.plegable(t_prue, "La prueba completa (la importante)",
                          "Usa UNA de las dos para empezar. La guiada es la recomendada: al comienzo te pide tocar, sacudir, girar y soplar; después sigue sola. "
                          "Si se corta (la placa se cae o se desenchufa), «CONTINUAR» sigue desde la fase donde quedó, sin repetir lo ya hecho.", abierto=True)
        self.rejilla(g, (("PRUEBA TOTAL + guiada (con tus manos)", "total_guiado"), ("PRUEBA TOTAL (todo automático)", "total"), ("CONTINUAR la prueba cortada", "total_continuar")), self.probar, por_fila=3)
        g = self.plegable(t_prue, "Lo nuevo (versiones 1.0.16 a 1.0.23)",
                          "Prueba en el orbe lo que se hizo en las últimas versiones: los juegos de a dos y de la isla, sus fondos, tocar el faro y la piedra, "
                          "el micrófono (niebla, mal olor, bote), la velocidad con lluvia y de noche, y la memoria. Necesita la versión 1.0.23 o más nueva.", abierto=True)
        self.rejilla(g, (("Probar lo nuevo (guiada: te pide soplar)", "nuevo"), ("Probar lo nuevo (sin manos)", "nuevo_auto")), self.probar, por_fila=2)
        g = self.plegable(t_prue, "Probar una sola parte", "Cada botón prueba una sola cosa, más rápido que la prueba completa.")
        self.rejilla(g, (("Paneles", "paneles"), ("Juegos", "juegos"), ("Zonas", "zonas"), ("Secretos", "secretos"), ("Finales", "finales"),
                         ("Actividades", "actividades"), ("Modo cantar", "cantar"), ("Sonido", "sonido"), ("Probar todo", "todo")), self.probar)
        g = self.plegable(t_prue, "Rendimiento y batería", "Miden cuánto gasta y cuánto aguanta. Las de descarga tardan (1 h o hasta el 10 %).")
        self.rejilla(g, (("RENDIMIENTO automático", "rendimiento"), ("Comparar TAMAÑOS 100/120/130/140", "tamano"), ("Prueba de DESCARGA corta (1 h)", "descarga_corta"),
                         ("Prueba de DESCARGA larga (hasta 10 %)", "descarga_larga"), ("RESULTADO de la descarga", "descarga_resultado")), self.probar, por_fila=2)

        # ---------- 3. la pantalla y la hora
        t_pant = self.hoja("Pantalla y hora")
        g = self.plegable(t_pant, "Giro de la pantalla", "Prueba sin el acelerómetro: gira la imagen y el táctil de la placa.")
        self.rejilla(g, (("Derecho", "GIRO 0"), ("Girar 90°", "GIRO 1"), ("Al revés (180°)", "GIRO 2"), ("Girar 270°", "GIRO 3"),
                         ("Volver a la calibración de fábrica", "GIRO CAL R")), self.probar_cmd)
        b = ttk.Button(g, text="Calibrar giro…", command=self.calibrar_giro); b.pack(anchor="w", padx=3, pady=2); self.botones.append(b)
        g = self.plegable(t_pant, "Hora del día de la mascota", "Solo de este perfil; no cambia el reloj de la placa.")
        self.rejilla(g, (("Mañana (8:00)", "MOMENTO MANANA"), ("Tarde (15:00)", "MOMENTO TARDE"), ("Noche (21:30)", "MOMENTO NOCHE"),
                         ("Madrugada (3:00)", "MOMENTO MADRUGADA"), ("Hora real", "MOMENTO REAL")), self.probar_cmd)
        fila = ttk.Frame(g); fila.pack(fill="x", pady=2)
        self.var_hora = tk.StringVar(value="12:00")
        ttk.Entry(fila, textvariable=self.var_hora, width=6).pack(side="left", padx=3)
        b = ttk.Button(fila, text="Poner esta hora (HH:MM)", command=lambda: self.probar_cmd(f"MOMENTO {self.var_hora.get().strip()}")); b.pack(side="left", padx=3); self.botones.append(b)
        b = ttk.Button(fila, text="Poner la hora REAL en el reloj de la placa", command=self.poner_hora_real); b.pack(side="left", padx=(14, 3)); self.botones.append(b)

        # ---------- 4. el mundo y los lugares
        t_mun = self.hoja("Mundo y lugares")
        g = self.plegable(t_mun, "Zonas y lugares secretos", "El claro del Rey Peumo y el mirador son zonas de verdad (fw 98); la casita del claro es una pantalla aparte.")
        self.rejilla(g, (("Abrir los lugares", "LUGARES"), ("Al claro del rey", "ENTRAR 1"), ("Casa del rey (adentro)", "ENTRAR 2"), ("Al mirador", "ENTRAR 3"),
                         ("Subir en la nube", "NUBE"), ("Arcoíris", "ARCOIRIS"), ("Esconder de nuevo", "ESCONDER"), ("Excursión al claro", "EXCURSION 0"),
                         ("Excursión al mirador", "EXCURSION 1"), ("Que vuelva", "EXCURSION -1")), self.probar_cmd, por_fila=5)
        g = self.plegable(t_mun, "Los cinco juegos de los lugares", "Con puntaje, récord y estrellas. «Salir» cierra el juego.")
        self.rejilla(g, (("Galletas de Peumo", "JUEGOLUGAR 1"), ("Canción de cuna", "JUEGOLUGAR 2"), ("Salto al cielo", "JUEGOLUGAR 3"),
                         ("Atrapa hojas", "JUEGOLUGAR 4"), ("Piedra tibia", "JUEGOLUGAR 5"), ("Salir", "SALIRLUGAR")), self.probar_cmd, por_fila=3)
        g = self.plegable(t_mun, "Visitas, huevo y ballena", "La mascota debe estar despierta, con el juego a la vista y sin otra visita.")
        self.rejilla(g, (("Que llegue una visita ahora", "VISITA"), ("Que pase la ballena", "BALLENA")), self.probar_cmd, por_fila=2)
        self.rejilla(g, (("Que ponga el huevo (pregunta de viaje)", "HUEVO 0"), ("Dejar el huevo en el nido", "HUEVO 1"), ("Quitar el huevo", "HUEVO 2")), self.probar_cmd, por_fila=3)
        b = ttk.Button(g, text="Sacar de la cueva y borrar su registro", command=self.sacar_cueva); b.pack(anchor="w", padx=3, pady=2); self.botones.append(b)
        g = self.plegable(t_mun, "Fondos del mundo (prueba de memoria, fw 105)",
                          "La casita del prado, el farol, el banco y los senderos se dibujan sobre el fondo de la zona. Si la PSRAM está muy picada, el fondo "
                          "se arma POR FRANJAS: con esto lo obligas a hacerlo así para comprobar que igual se ven todos los adornos.")
        self.rejilla(g, (("Fondo por franjas (como si faltara memoria)", "SINLIENZO 1"), ("Volver a lo normal", "SINLIENZO 0"),
                         ("Rehacer el fondo ahora", "SINLIENZO R")), self.probar_cmd, por_fila=3)

        # ---------- 5. Pochi y los animales
        t_esc = self.hoja("Pochi y animales")
        g = self.plegable(t_esc, "Pochi, la chihuahua visitante", "La mascota debe estar en el prado, despierta, con el juego a la vista.")
        self.rejilla(g, (("Que llegue", "POCHI 0"), ("Invitar a pasear", "POCHI 1"), ("Paseo", "POCHI 2"), ("Despedida", "POCHI 3"),
                         ("Que corra", "POCHI 4"), ("Barro", "POCHI 5")), self.probar_cmd)
        g = self.plegable(t_esc, "Sus cachorros y la familia (fw 102)", "Chapi y Rony (niños), Pasita y Morita (niñas). Los cuatro son igual de importantes.")
        self.rejilla(g, (("+ Chapi", "POCHI 10"), ("+ Pasita", "POCHI 11"), ("+ Morita", "POCHI 12"), ("+ Rony", "POCHI 13"),
                         ("+ Todos", "POCHI 14"), ("+ Dos", "POCHI 15"), ("Familia", "POCHI 16"), ("¿Y el cachorro?", "POCHI 17")), self.probar_cmd, por_fila=8)
        self.rejilla(g, (("Se rasca", "POCHI 20"), ("Panza arriba", "POCHI 21"), ("Mareada", "POCHI 22"), ("Ramita", "POCHI 30"),
                         ("Luciérnaga", "POCHI 31"), ("Cóndor", "POCHI 32"), ("Agua", "POCHI 33"), ("Consuela", "POCHI 40"),
                         ("Mojada", "POCHI 41"), ("Ofendida", "POCHI 42"), ("Acurrucada", "POCHI 43")), self.probar_cmd, por_fila=7)
        self.rejilla(g, (("Chapi y la rana", "POCHI 100"), ("Pasita y la mariposa", "POCHI 101"), ("Morita y el caracol", "POCHI 102"), ("Rony y la ardilla", "POCHI 103")), self.probar_cmd, por_fila=4)
        g = self.plegable(t_esc, "Pochi y los animales del lugar (fw 102)", "Arriba: Pochi sola. Abajo: Pochi con UN cachorro (con dos o más no hay encuentros).")
        self.rejilla(g, (("Bichito", "POCHI 60"), ("Pájaro", "POCHI 61"), ("Manso", "POCHI 62"), ("Lento", "POCHI 63"), ("De agua", "POCHI 64"),
                         ("Grande", "POCHI 65"), ("Zorrillo", "POCHI 66"), ("Erizo", "POCHI 67"), ("Urraca", "POCHI 68"), ("Búho", "POCHI 69"),
                         ("Chucao", "POCHI 70")), self.probar_cmd, por_fila=6)
        self.rejilla(g, (("Cach.+bichito", "POCHI 80"), ("Cach.+pájaro", "POCHI 81"), ("Cach.+manso", "POCHI 82"), ("Cach.+lento", "POCHI 83"),
                         ("Cach.+agua", "POCHI 84"), ("Cach.+grande", "POCHI 85"), ("Cach.+zorrillo", "POCHI 86"), ("Cach.+erizo", "POCHI 87"),
                         ("Cach.+urraca", "POCHI 88")), self.probar_cmd, por_fila=5)
        g = self.plegable(t_esc, "Tocar animales (fw 103)", "El primer toque saluda, el siguiente avisa y si insistes pasa lo suyo. En modo infante nunca pasa nada malo.")
        self.rejilla(g, (("Saluda", "TOQUE 1"), ("Se enoja", "TOQUE 2"), ("La pican", "TOQUE 3"), ("Púas", "TOQUE 4"),
                         ("Zorrillo (olor)", "TOQUE 5"), ("Ruge", "TOQUE 6"), ("Se va", "TOQUE 7")), self.probar_cmd)
        g = self.plegable(t_esc, "Novedades del firmware 104", "Papelitos del bosque, el trocito de hielo, los paseos de verdad y la terquedad.")
        self.rejilla(g, (("Papelito: lo encuentras", "PAPELITO 0"), ("Papelito: te lo trae", "PAPELITO 1"), ("Hielo: lo pesca", "HIELO 1"),
                         ("Hielo: se derrite", "HIELO 2"), ("Paseo corto", "PASEO 1200"), ("Paseo largo", "PASEO 3500"),
                         ("Muy terca", "TERCA 1"), ("Dócil", "TERCA 0")), self.probar_cmd, por_fila=4)

        # ---------- 6. desbloquear contenido
        t_des = self.hoja("Desbloquear contenido")
        fc = self.plegable(t_des, "Desbloquear contenido para probar", "Se guarda la partida antes; «Restaurar partida» (pestaña Placa) la devuelve.", abierto=True)
        izq = ttk.Frame(fc); izq.pack(side="left")
        cats = (("Zonas", "zonas"), ("Criaturas", "criaturas"), ("Historias", "historias"), ("Amigos", "amigos"), ("Animales", "animales"), ("Secretos", "secretos"),
                ("Objetos", "objetos"), ("Ropa", "ropa"), ("Juguetes", "juguetes"), ("Decoraciones", "decoraciones"), ("Comidas", "comidas"),
                ("Colecciones", "colecciones"), ("Leyendas", "leyendas"), ("Estación", "estacion"), ("Clima", "clima"))
        for i, (texto, q) in enumerate(cats):
            b = ttk.Button(izq, text=texto, width=12, command=lambda q=q, tx=texto: self.elegir_cat(q, tx))
            b.grid(row=i // 8, column=i % 8, padx=2, pady=2); self.botones.append(b)
        b = ttk.Button(izq, text="Todo lo anterior", width=12, command=lambda: self.probar_cmd("DESBLOQUEAR TODO"))
        b.grid(row=2, column=0, padx=2, pady=2); self.botones.append(b)
        der = ttk.Frame(fc); der.pack(side="left", padx=14)
        self.cat_lbl = ttk.Label(der, text="Elige un tipo a la izquierda y aquí aparece su lista"); self.cat_lbl.pack(anchor="w")
        fila = ttk.Frame(der); fila.pack(anchor="w", pady=2)
        self.cat_var = tk.StringVar()
        self.cat_combo = ttk.Combobox(fila, textvariable=self.cat_var, state="readonly", width=36); self.cat_combo.pack(side="left")
        self.b_aplicar = ttk.Button(fila, text="Desbloquear", command=self.aplicar_cat); self.b_aplicar.pack(side="left", padx=6)
        self.b_bloquear = ttk.Button(fila, text="Volver a bloquear", command=lambda: self.aplicar_cat("BLOQUEAR")); self.b_bloquear.pack(side="left")
        self.cat_q, self.cat_idx = None, []

        # ---------- 7. voces
        self.armar_voces(self.plegable(self.hoja("Voces"), "Voces de la mascota y de los personajes", "Suenan en la placa, con el volumen de efectos.", abierto=True))

        # ---------- 8. respaldo
        t_res = self.hoja("Respaldo")
        g = self.plegable(t_res, "Respaldo de la mascota", "Sirve con la placa en modo normal o en modo boot: ella sola entra al modo descarga y vuelve a arrancar.", abierto=True)
        self.var_fotos = tk.BooleanVar(value=True)
        fila = ttk.Frame(g); fila.pack(fill="x")
        b = ttk.Button(fila, text="Respaldar mascota", command=self.respaldar); b.pack(side="left"); self.botones.append(b)
        b = ttk.Button(fila, text="Restaurar un respaldo…", command=self.restaurar_respaldo); b.pack(side="left", padx=6); self.botones.append(b)
        ttk.Checkbutton(fila, text="incluir fotos (más lento)", variable=self.var_fotos).pack(side="left", padx=6)
        fd = ttk.Frame(g); fd.pack(fill="x", pady=2)
        ttk.Label(fd, text="Guardar los respaldos en:").pack(side="left")
        self.var_dir = tk.StringVar(value=self.carpeta_respaldos())
        ttk.Entry(fd, textvariable=self.var_dir, width=60).pack(side="left", padx=6)
        ttk.Button(fd, text="Elegir carpeta…", command=self.elegir_carpeta_respaldos).pack(side="left")
        ttk.Label(g, text="Cada respaldo queda en una subcarpeta con fecha. Elige un lugar FUERA de C:\\mascota para no perderlo al limpiar esa carpeta.",
                  wraplength=780, justify="left").pack(anchor="w", pady=(4, 0))

        # (1.0.20) la llave de los informes: se carga una vez y el orbe manda sus errores a GitHub (Ajustes → Versión → ENVIAR INFORME)
        g = self.plegable(t_res, "Llave de informes (para ENVIAR INFORME desde el orbe)", "Pega la llave de GitHub que creaste para orbepet-informes. Se guarda solo en la placa: no se muestra ni queda en el registro.", abierto=True)
        fila = ttk.Frame(g); fila.pack(fill="x")
        b = ttk.Button(fila, text="Cargar llave de informes…", command=self.cargar_llave); b.pack(side="left"); self.botones.append(b)
        b = ttk.Button(fila, text="¿Tiene llave?", command=lambda: self.orden_llave("?")); b.pack(side="left", padx=6); self.botones.append(b)
        b = ttk.Button(fila, text="Borrar llave", command=lambda: self.orden_llave("BORRAR")); b.pack(side="left"); self.botones.append(b)

        fa = ttk.Frame(self, padding=(8, 2)); fa.pack(fill="x")
        self.b_stop = ttk.Button(fa, text="Detener", command=self.detener, state="disabled"); self.b_stop.pack(side="left")
        ttk.Button(fa, text="Copiar informe", command=self.copiar_informe).pack(side="left", padx=6)
        ttk.Button(fa, text="Copiar todo lo de abajo", command=self.copiar_log).pack(side="left")
        self.etapa = ""
        self.t0 = None
        self.reloj = ttk.Label(fa, text="⏱ 00:00", width=9, anchor="e"); self.reloj.pack(side="right")
        self.barra = BarraTexto(fa); self.barra.pack(side="right", padx=6, fill="x", expand=True)

        self.log = scrolledtext.ScrolledText(self, wrap="word", font=("Consolas", 10), height=20)  # (105) el registro es lo que se lee: se queda con el espacio
        self.log.pack(fill="both", expand=True, padx=8, pady=8)
        self.escribir(f"Carpeta del proyecto: {probar.RAIZ}\n")
        if self.idf_path:
            self.escribir(f"ESP-IDF encontrado: {self.idf_path}\n")
            self.escribir(f"Script de activación: {self.activador or 'no encontré (uso export.ps1)'}\n")
        else:
            self.escribir("NO encuentro ESP-IDF (busqué en C:\\esp y C:\\Espressif). Instalar y probar sí funcionan si ya está compilado; compilar no.\n")
        self.buscar_puertos()
        self.after(100, self.vaciar)
        self.remoto_lineas, self.remoto_marca, self.remoto_ult = [], None, 0
        self.after(1500, self.remoto_tic)
        self._bat_ult, self._bat_ocupada = None, False
        self.after(2500, self.bateria_tic)

    # ---------- (105) pestañas por función y cuadros plegables
    def hoja(self, titulo, alto_max=360):
        """Una pestaña con barra para bajar; crece con lo que esté abierto, hasta un máximo."""
        hoja = ttk.Frame(self.nb)
        self.nb.add(hoja, text="  " + titulo + "  ")
        if not hasattr(self, "_altos"):
            self._altos = {}
            self.nb.bind("<<NotebookTabChanged>>", self.ajustar_nb)
        cv = tk.Canvas(hoja, highlightthickness=0, height=46)
        sb = ttk.Scrollbar(hoja, orient="vertical", command=cv.yview)
        cv.configure(yscrollcommand=sb.set)
        cv.pack(side="left", fill="both", expand=True)
        dentro = ttk.Frame(cv)
        wid = cv.create_window((0, 0), window=dentro, anchor="nw")

        ultimo = {"alto": None, "ancho": None}

        def medir(e=None):
            cv.configure(scrollregion=cv.bbox("all"))
            alto = min(max(dentro.winfo_reqheight(), 46), alto_max)
            if alto == ultimo["alto"]:
                return  # sin cambios: no se vuelve a configurar (evita dar vueltas con los eventos)
            ultimo["alto"] = alto
            cv.configure(height=alto)
            self._altos[str(hoja)] = alto + 8   # el cuaderno pide el alto de la pestaña que se está viendo
            self.ajustar_nb()
            if dentro.winfo_reqheight() > alto_max:
                sb.pack(side="right", fill="y")
            else:
                sb.pack_forget()
                cv.yview_moveto(0)
        dentro.bind("<Configure>", medir)

        def ancho(e):
            if e.width != ultimo["ancho"]:
                ultimo["ancho"] = e.width
                cv.itemconfigure(wid, width=e.width)
        cv.bind("<Configure>", ancho)

        def rueda(e):
            cv.yview_scroll(-1 if (getattr(e, "delta", 0) > 0 or getattr(e, "num", 0) == 4) else 1, "units")
        cv.bind("<Enter>", lambda e: cv.bind_all("<MouseWheel>", rueda))
        cv.bind("<Leave>", lambda e: cv.unbind_all("<MouseWheel>"))
        return dentro

    def ajustar_nb(self, e=None):
        """El cuaderno mide lo que pide la pestaña que se está viendo (si no, se queda con el alto de la más grande)."""
        try:
            sel = self.nb.select()
            if sel:
                self.nb.configure(height=self._altos.get(str(self.nb.nametowidget(sel)), 60))
        except tk.TclError:
            pass

    def plegable(self, padre, titulo, nota="", abierto=False):
        """Un cuadro plegable dentro de una pestaña; devuelve el marco donde van los botones."""
        return Plegable(padre, titulo, nota, abierto, self.plegables).cuerpo

    def plegar_todo(self, abrir):
        for p in self.plegables:
            p.alternar(abrir)

    def rejilla(self, padre, items, cmd, por_fila=4, ancho=0):
        """Botones en filas de «por_fila». Si cmd es None, cada valor es la función que se llama."""
        fila = None
        for k, (texto, v) in enumerate(items):
            if k % por_fila == 0:
                fila = ttk.Frame(padre)
                fila.pack(fill="x")
            b = ttk.Button(fila, text=texto, command=(lambda v=v: cmd(v)) if cmd else v)
            if ancho:
                b.config(width=ancho)
            b.pack(side="left", padx=3, pady=2)
            self.botones.append(b)

    def sacar_cueva(self):
        if messagebox.askyesno("Sacar de la cueva", "La lleva al prado y borra lo que quedó escrito de su primera visita a la cueva (la marca de «ya visitada», la nota del diario y la foto del álbum).\n\nEl resto de la partida no se toca.\n\n¿Seguro?"):
            self.probar_cmd("SACAR_ZONA 4")

    # ---------- batería en vivo (arriba): cada 3 s le pregunta a la placa si nadie más usa el puerto
    def bateria_tic(self):
        try:
            libre = not (self.hilo and self.hilo.is_alive()) and self.t0 is None and not getattr(self, "_viendo", False)
            puerto = self.puerto.get().strip()
            if libre and puerto and not self._bat_ocupada:
                self._bat_ocupada = True
                def f():
                    d, nota = None, "la placa no contesta"
                    try:
                        p = probar.Placa(puerto)
                        try:
                            r = p.cmd("BATERIA", 0.1)
                            if r and "BATERIA OK" in r:
                                d = probar.parsear(r)
                        finally:
                            p.s.close()
                    except Exception:
                        nota = "puerto ocupado o sin placa"
                    def mostrar():
                        self._bat_ocupada = False
                        if d:
                            self.bateria.poner(d)
                            if d.get("llena") and not getattr(self, "_bat_llena_avisada", False):
                                self._bat_llena_avisada = True
                                self.escribir(time.strftime("[%H:%M:%S]") + " BATERÍA: carga completa (100 %)\n")
                                self.bell()
                            if not d.get("llena"): self._bat_llena_avisada = False
                            try:  # un registro con hora para ver la curva de carga después
                                open(os.path.join(probar.RAIZ, "bateria_log.csv"), "a", encoding="utf-8").write(
                                    f"{time.strftime('%Y-%m-%d %H:%M:%S')},{d.get('pct')},{d.get('crudo')},{d.get('mv')},{d.get('vbus')},{d.get('carga')},{d.get('fase')},{d.get('llena')},{d.get('pantalla')}\n")
                            except OSError:
                                pass
                        else:
                            self.bateria.poner(None, nota)
                    self.cola.put(mostrar)
                threading.Thread(target=f, daemon=True).start()
        except Exception:
            self._bat_ocupada = False
        self.after(3000, self.bateria_tic)

    # ---------- modo Claude: él deja órdenes en un archivo y aquí se ejecutan; lo que sale queda en otro archivo
    def remoto_tic(self):
        try:
            self.remoto_paso()
        except Exception:
            self.escribir("[modo Claude] " + traceback.format_exc())
        self.after(700, self.remoto_tic)

    def remoto_paso(self):
        if time.time() - self.remoto_ult > 3:
            self.remoto_ult = time.time()
            try:
                texto = ("ATENDIENDO" if self.var_remoto.get() else "PAUSADO") + (" · ocupado" if self.t0 is not None else " · libre") + time.strftime(" · %H:%M:%S")
                open(ESTADO, "w", encoding="utf-8").write(texto + "\n")
            except OSError:
                pass
        if not self.var_remoto.get():
            return
        libre = not (self.hilo and self.hilo.is_alive()) and self.t0 is None
        if self.remoto_marca is None:
            if libre and os.path.exists(ORDEN):
                try:
                    lineas = [l.strip() for l in open(ORDEN, encoding="utf-8-sig").read().splitlines()]
                    os.replace(ORDEN, ORDEN + ".hecha")
                except OSError:
                    return
                self.remoto_lineas = [l for l in lineas if l and not l.startswith("#")]
                self.remoto_marca = self.log.index("end-1c")
                try:
                    if os.path.exists(RESULTADO):
                        os.remove(RESULTADO)
                except OSError:
                    pass
                self.escribir("\n[modo Claude] órdenes recibidas: " + " | ".join(self.remoto_lineas) + "\n")
            return
        if not libre:
            return
        if self.remoto_lineas:
            self.remoto_ejecutar(self.remoto_lineas.pop(0))
            return
        texto = self.log.get(self.remoto_marca, "end-1c")
        self.remoto_marca = None
        open(RESULTADO, "w", encoding="utf-8").write(texto + "\n=== FIN ===\n")

    def remoto_ejecutar(self, linea):
        cmd, _, arg = linea.partition(" ")
        cmd, arg = cmd.upper(), arg.strip()
        self.escribir(f"\n[modo Claude] >>> {linea}\n")
        if cmd == "CMD":          # una orden «MASCOTA PROBAR ...», p. ej.  CMD HUEVO 1
            self.probar_cmd(arg)
        elif cmd == "PROBAR":     # una de las pruebas de los botones: todo, juegos, zonas, info, escuchar, registro...
            self.probar(arg)
        elif cmd == "COREDUMP":     # el informe completo del último fallo (idf.py coredump-info)
            self.informe_fallo()
        elif cmd == "DECODIFICAR":  # traduce el último fallo guardado en el registro a archivo y línea
            self.decodificar()
        elif cmd == "LOG":        # el log de la placa
            self.probar("registro")
        elif cmd == "INSTALAR":   # extrae, compila y graba el zip (por defecto C:\mascota\mascota_firmware.zip)
            self.instalar(arg or os.path.join(PROYECTO, "mascota_firmware.zip"))
        elif cmd == "MODO":
            self.ver_modo()
        elif cmd == "ESPERAR":
            seg = float(arg or "1")
            self.tarea(lambda: time.sleep(seg))
        else:
            self.escribir(f"[modo Claude] orden desconocida: {cmd}\n")

    # ---------- utilidades
    def escribir(self, t):
        self.log.insert("end", t); self.log.see("end")

    def vaciar(self):
        try:
            while True:
                x = self.cola.get_nowait()
                if callable(x):
                    x()
                else:
                    self.escribir(x)
        except queue.Empty:
            pass
        self.after(100, self.vaciar)

    def tic(self):
        """El tiempo que lleva trabajando (se detiene al terminar y queda el total)."""
        if self.t0 is not None:
            s = int(time.time() - self.t0)
            self.ultimo = f"⏱ {s // 60:02d}:{s % 60:02d}"
            self.reloj.config(text=self.ultimo)
            self.after(1000, self.tic)

    def set_pct(self, p, texto=None):
        p = max(0, min(100, int(p)))
        self.barra.poner(p, texto if texto is not None else self.etapa)

    def poner_etapa(self, texto):  # desde el hilo de trabajo
        def f():
            self.etapa = texto
            self.barra.poner(None, texto)
        self.cola.put(f)

    _pmax = 0
    def poner_pct(self, p, texto=None):  # desde el hilo de trabajo
        if p <= 1:
            self._pmax = 0
        elif p < self._pmax:
            p = self._pmax   # la barra nunca retrocede
        self._pmax = max(self._pmax, p)
        self.cola.put(lambda: self.set_pct(p, texto))

    def puerto_real(self):
        """El puerto de la placa: si el escrito no existe y hay una placa conectada, se usa esa (así no falla por COM3 mal puesto)."""
        puerto = self.puerto.get().strip() or "COM5"
        try:
            from serial.tools import list_ports
            ps = list(list_ports.comports())
            nombres = [p.device for p in ps]
            esp = [p.device for p in ps if p.vid == 0x303A]
            if esp and puerto not in esp and (puerto not in nombres or len(esp) == 1):
                puerto = esp[0]
                self.puerto.set(puerto)
        except Exception:
            pass
        return puerto

    def buscar_puertos(self):
        try:
            from serial.tools import list_ports
            ps = list(list_ports.comports())
        except Exception:
            ps = []
        nombres = [p.device for p in ps]
        esp = [p.device for p in ps if p.vid == 0x303A]
        self.combo["values"] = nombres
        if esp:
            self.puerto.set(esp[0])
        elif nombres and not self.puerto.get():
            self.puerto.set(nombres[0])
        elif not nombres:
            self.puerto.set(self.puerto.get() or "COM5")
        self.estado.config(text="placa encontrada" if esp else "no veo la placa (¿está conectada?)")
        if esp and not (self.hilo and self.hilo.is_alive()):
            self.ver_modo()
        elif not esp:
            self.modo.config(text="Modo: sin placa", foreground="gray")

    def preguntar(self, titulo, texto):
        """Desde el hilo de trabajo: muestra un aviso y espera a que lo aprietes."""
        ev = threading.Event()
        def f():
            messagebox.showinfo(titulo, texto); ev.set()
        self.cola.put(f); ev.wait()

    # ---- la ventana que guía la prueba con tus manos (una sola, que va cambiando de paso) ----
    def guia_paso(self, titulo, texto, seg=None, n=0, total=0):
        """Desde el hilo de trabajo: muestra el paso (sin esperar). seg = cuenta regresiva en la ventana."""
        self.cola.put(lambda: self._guia_poner(titulo, texto, seg, n, total))

    def guia_estado(self, texto, bueno=True):
        self.cola.put(lambda: self._guia_estado(texto, bueno))

    def guia_cerrar(self):
        self.cola.put(self._guia_cerrar)

    def guia_punto(self, x, y):
        self.cola.put(lambda: self._guia_punto(x, y))

    def _guia_punto(self, x, y):
        w = self._guia_crear()
        cv = w.cv
        cv.delete("all")
        if x is None:
            cv.pack_forget()
            return
        cv.pack(before=w.l_cuenta, pady=(6, 0))
        cv.create_oval(4, 4, 156, 156, outline="#444", width=3)           # la pantalla redonda de la mascota
        cv.create_line(80, 4, 80, 156, fill="#ddd"); cv.create_line(4, 80, 156, 80, fill="#ddd")
        px, py = 4 + x * 152 / 466.0, 4 + y * 152 / 466.0
        cv.create_oval(px - 10, py - 10, px + 10, py + 10, fill="#e02030", outline="#600010", width=2)

    def _guia_crear(self):
        w = getattr(self, "_guia", None)
        if w is not None and w.winfo_exists():
            return w
        w = tk.Toplevel(self)
        w.title("Prueba guiada")
        w.geometry("640x600")
        w.attributes("-topmost", True)
        w.protocol("WM_DELETE_WINDOW", lambda: None)  # se cierra sola al terminar (para detener la prueba, usa el botón Detener)
        w.l_paso = tk.Label(w, text="", font=("Segoe UI", 11), fg="#666")
        w.l_paso.pack(pady=(14, 0))
        w.l_tit = tk.Label(w, text="", font=("Segoe UI", 24, "bold"), wraplength=600, justify="center")
        w.l_tit.pack(pady=(6, 4), padx=16)
        w.l_txt = tk.Label(w, text="", font=("Segoe UI", 14), wraplength=580, justify="center")
        w.l_txt.pack(pady=4, padx=16)
        w.cv = tk.Canvas(w, width=160, height=160, highlightthickness=0)   # (solo se muestra en los pasos del táctil)
        w.l_cuenta = tk.Label(w, text="", font=("Segoe UI", 44, "bold"), fg="#1b6ec2")
        w.l_cuenta.pack(pady=(10, 0))
        w.barra = ttk.Progressbar(w, length=520, maximum=100)
        w.barra.pack(pady=6)
        w.l_est = tk.Label(w, text="", font=("Segoe UI", 14, "bold"), wraplength=600)
        w.l_est.pack(pady=(8, 10))
        w.fin = 0.0
        w.dur = 0.0
        self._guia = w
        return w

    def _guia_poner(self, titulo, texto, seg, n, total):
        w = self._guia_crear()
        w.l_paso.config(text=(f"Paso {n} de {total}" if n and total else ""))
        w.l_tit.config(text=titulo)
        w.l_txt.config(text=texto)
        w.l_est.config(text="", fg="#000")
        if seg:
            w.dur, w.fin = float(seg), time.time() + float(seg)
            self._guia_tick(w, w.fin)
        else:
            w.dur = w.fin = 0.0
            w.l_cuenta.config(text="")
            w.barra["value"] = 0
        w.lift()

    def _guia_tick(self, w, fin):
        if not w.winfo_exists() or w.fin != fin or not fin:
            return
        resto = max(0.0, fin - time.time())
        w.l_cuenta.config(text=f"{resto:0.0f} s")
        w.barra["value"] = 100 * resto / w.dur if w.dur else 0
        if resto > 0:
            self.after(100, lambda: self._guia_tick(w, fin))

    def _guia_estado(self, texto, bueno):
        w = getattr(self, "_guia", None)
        if w is not None and w.winfo_exists():
            w.l_est.config(text=texto, fg="#1c8a3a" if bueno else "#b36b00")
            w.fin = 0.0

    def _guia_cerrar(self):
        w = getattr(self, "_guia", None)
        if w is not None and w.winfo_exists():
            w.destroy()
        self._guia = None

    def preguntar_si(self, texto):
        """Desde el hilo de trabajo: pregunta sí/no y espera la respuesta."""
        ev, r = threading.Event(), [True]
        def f():
            r[0] = messagebox.askyesno("Fin de la prueba", texto); ev.set()
        self.cola.put(f); ev.wait()
        return r[0]

    def ocupado(self, si):
        for b in self.botones:
            b.config(state="disabled" if si else "normal")
        self.b_stop.config(state="normal" if si else "disabled")
        if si:
            self.etapa = ""
            self.t0 = time.time()
            self._pmax = 0
            self.set_pct(0)
            self.tic()
        else:
            self.t0 = None

    # ---------- ejecutar tareas en otro hilo
    class Salida(io.TextIOBase):
        def __init__(s, cola): s.cola = cola
        def write(s, t):
            s.cola.put(ANSI.sub("", t).replace("\r", "")); return len(t)
        def flush(s): pass

    def tarea(self, f):
        if self.hilo and self.hilo.is_alive():
            return
        probar.CANCELAR = False
        self.ocupado(True)
        def run():
            viejo = sys.stdout
            sys.stdout = App.Salida(self.cola)
            estado = "Listo"
            try:
                for _ in range(100):  # espera a que el medidor de batería suelte el puerto (si no, COM choca)
                    if not getattr(self, "_bat_ocupada", False): break
                    time.sleep(0.1)
                if f() is False:
                    estado = "Terminó con problemas"
            except probar.Cancelado:
                print("Detenido.")
                estado = "Detenido"
            except Exception:
                print(traceback.format_exc())
                estado = "Terminó con error"
            try:
                if estado == "Listo":
                    if self._pmax < 100: self.poner_etapa(""); self.poner_pct(100, "Listo")  # (si la tarea ya cerró la barra con su propio texto, se respeta)
                else:  # fallo o detenido: la barra queda donde iba, con el motivo escrito encima
                    self.cola.put(lambda e=estado: self.barra.poner(None, e))
            except Exception:
                pass
            finally:
                sys.stdout = viejo
                self.cola.put(lambda: self.ocupado(False))
        self.hilo = threading.Thread(target=run, daemon=True)
        self.hilo.start()

    def detener(self):
        probar.CANCELAR = True
        if self.proc and self.proc.poll() is None:
            try:
                self.proc.terminate()
            except Exception:
                pass
        self.escribir("\nDeteniendo…\n")

    # ---------- ESP-IDF por dentro
    def idf(self, args, cwd):
        if not self.idf_path:
            print("No encuentro ESP-IDF en este computador.")
            return False
        # Se prepara el entorno igual que "ESP-IDF PowerShell": variables, el Python de ESP-IDF primero en el PATH y export.ps1.
        # El guion va en un archivo (sin líos de comillas).
        venv = os.environ.get("IDF_PYTHON_ENV_PATH") or next(iter(sorted(glob.glob(os.path.join(self.idf_tools, "python", "*", "venv")), reverse=True)), "")
        L = ["$ErrorActionPreference = 'Continue'",
             f"$env:IDF_TOOLS_PATH = '{self.idf_tools}'", f"$env:IDF_PATH = '{self.idf_path}'"]
        if venv:
            L += [f"$env:IDF_PYTHON_ENV_PATH = '{venv}'", f"$env:PATH = '{venv}\\Scripts;' + $env:PATH"]
        act = self.activador
        L += [f"Set-Location -LiteralPath '{cwd}'",
              f". '{act}'" if act else f". '{self.idf_path}\\export.ps1'",
              "Write-Output ('python: ' + (Get-Command python -ErrorAction SilentlyContinue).Source)",
              f"& python '{self.idf_path}\\tools\\idf.py' {' '.join(args)}",
              "exit $LASTEXITCODE"]
        guion = os.path.join(AQUI, "_idf.ps1")
        open(guion, "w", encoding="utf-8-sig").write("\r\n".join(L) + "\r\n")
        print("\n> idf.py " + " ".join(args))
        si = None
        if os.name == "nt":
            si = subprocess.STARTUPINFO()
            si.dwFlags |= subprocess.STARTF_USESHOWWINDOW
            si.wShowWindow = 0  # oculta
        try:
            self.proc = subprocess.Popen(["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", guion], cwd=cwd,
                                         stdout=subprocess.PIPE, stderr=subprocess.STDOUT, stdin=subprocess.DEVNULL, startupinfo=si)
        except FileNotFoundError:
            print("No encuentro PowerShell.")
            return False
        todas = []
        for linea in iter(self.proc.stdout.readline, b""):
            try:
                t = linea.decode("utf-8")
            except UnicodeDecodeError:
                t = linea.decode("cp850", "replace")  # la consola de Windows en español
            todas.append(t)
            sys.stdout.write(t)
            self.progreso_linea(t, args)
        codigo = self.proc.wait()
        if codigo:
            print(f"(idf.py terminó con código {codigo})")
            # el resumen de lo que falló va al final (lo de arriba puede ser larguísimo) y todo queda en un archivo
            try:
                ruta = os.path.join(cwd, "compilacion.txt")
                open(ruta, "w", encoding="utf-8").write("".join(todas))
                clave = re.compile(r"error|FAILED|undefined reference|multiple definition|CMake Error|fatal|Traceback", re.I)
                malas = [l for l in todas if clave.search(l) and "-Werror" not in l][:40]
                print("\n=== RESUMEN DEL ERROR (mándame esto) ===")
                for l in malas:
                    print(l.rstrip()[:400])
                print("--- últimas líneas ---")
                for l in todas[-25:]:
                    print(l.rstrip()[:400])
                print(f"(todo el texto de la compilación quedó en {ruta})")
            except OSError:
                pass
        return codigo == 0

    etapa_fase = None
    FASES = {"set-target": (5, 12), "build": (12, 75), "flash": (75, 98)}

    def progreso_linea(self, t, args):
        fase = "set-target" if args and args[0] == "set-target" else ("flash" if "flash" in args else "build")
        ini, fin = self.FASES[fase]
        if self.etapa_fase != fase:
            self.etapa_fase = fase
            self.poner_etapa({"set-target": "Preparando el proyecto", "build": "Compilando", "flash": "Instalando en el dispositivo"}[fase])
        if fase == "flash":
            # "flash" primero revisa la compilación (ninja) y luego esptool graba 3 imágenes, cada una de 0 a 100 %:
            # las pequeñas ocupan 75-80 y la aplicación (la grande) 80-98
            m = re.search(r"Compressed (\d+) bytes", t)
            if m:
                self.flash_grande = int(m.group(1)) > 200000
            m = re.search(r"\((\d+) ?%\)", t)
            if m:
                a, b = (80, 98) if getattr(self, "flash_grande", False) else (75, 80)
                self.poner_pct(a + (b - a) * int(m.group(1)) / 100, None)
            return
        m = re.search(r"\[(\d+)/(\d+)\]", t)            # ninja: [123/456]
        if m and int(m.group(2)):
            self.poner_pct(ini + (fin - ini) * int(m.group(1)) / int(m.group(2)), None)

    def python_idf(self):
        venv = os.environ.get("IDF_PYTHON_ENV_PATH") or next(iter(sorted(glob.glob(os.path.join(self.idf_tools, "python", "*", "venv")), reverse=True)), "")
        py = os.path.join(venv, "Scripts", "python.exe") if venv else ""
        return py if py and os.path.exists(py) else sys.executable

    def en_modo_descarga(self, puerto):
        """¿La placa ya está en modo descarga? (se le pregunta sin reiniciarla: en funcionamiento normal no contesta)"""
        flags = 0x08000000 if os.name == "nt" else 0
        try:
            r = subprocess.run([self.python_idf(), "-m", "esptool", "--chip", "esp32s3", "-p", puerto, "--before", "no_reset", "--after", "no_reset",
                                "--connect-attempts", "2", "chip_id"], capture_output=True, timeout=25, creationflags=flags, stdin=subprocess.DEVNULL)
        except Exception:
            return False
        txt = (r.stdout + r.stderr).decode("utf-8", "replace")
        return r.returncode == 0 and ("Chip is" in txt or "Chip type" in txt or "Detecting chip" in txt)

    def reiniciar_normal(self, puerto):
        """Saca a la placa del modo descarga y la hace arrancar el firmware sin desconectar el cable.
        En el USB nativo del ESP32-S3, el reinicio por RTS ("hard_reset") a veces la deja otra vez en modo descarga; el reinicio por
        watchdog reinicia todo el chip y arranca normal. Se prueba ese primero y luego los otros."""
        flags = 0x08000000 if os.name == "nt" else 0
        for despues in ("watchdog_reset", "hard_reset", "soft_reset"):
            for antes in ("no_reset", "usb_reset", "default_reset"):
                try:
                    r = subprocess.run([self.python_idf(), "-m", "esptool", "--chip", "esp32s3", "-p", puerto, "--before", antes,
                                        "--after", despues, "--connect-attempts", "3", "read_mac"],
                                       capture_output=True, timeout=40, creationflags=flags, stdin=subprocess.DEVNULL)
                except Exception as e:
                    print(f"({antes}/{despues}) no se pudo: {e}")
                    continue
                txt = (r.stdout + r.stderr).decode("utf-8", "replace")
                if r.returncode != 0 or not ("MAC" in txt or "esetting" in txt):
                    continue
                print(f"Reinicio pedido ({antes} + {despues}).")
                time.sleep(4)
                if probar.placa_responde(puerto, 8):
                    return True
                break  # contestó pero no arrancó: probar otra forma de reiniciar
        return False

    def poner_modo(self, texto, color):
        self.cola.put(lambda: self.modo.config(text=texto, foreground=color))

    def ver_modo(self):
        """Dice en qué modo está la placa: normal (corre el firmware), boot/carga (espera firmware) o sin respuesta."""
        if getattr(self, "_viendo", False) or (self.hilo and self.hilo.is_alive()):
            return
        self._viendo = True
        puerto = self.puerto.get().strip() or "COM5"
        self.modo.config(text="Modo: revisando…", foreground="gray")
        def poner(t, c):
            self.cola.put(lambda: self.modo.config(text=t, foreground=c))
        def f():
            try:
                if probar.placa_responde(puerto, 4):
                    poner("Modo: NORMAL (corriendo el firmware)", "#1a7f37")
                elif self.en_modo_descarga(puerto):
                    poner("Modo: BOOT / CARGA (espera firmware; usa «Sacar de modo boot»)", "#b35900")
                else:
                    poner("Modo: sin respuesta (apagada, sin firmware o puerto equivocado)", "#c62828")
            except Exception:
                poner("Modo: no se pudo revisar", "#c62828")
            finally:
                self._viendo = False
        threading.Thread(target=f, daemon=True).start()

    def poner_hora_real(self):
        """Le pone a la placa la fecha y la hora de este computador (con su zona horaria): arregla un reloj atrasado o en otra hora."""
        puerto = self.puerto.get().strip() or "COM5"
        def f():
            print(f"=== Poner la hora real (puerto {puerto}) ===")
            self.poner_etapa("Poniendo la hora y la fecha en la placa"); self.poner_pct(20)
            ok = probar.poner_hora(puerto, 8)
            if ok:
                print(time.strftime("Listo: el reloj de la placa quedó en %d-%m-%Y %H:%M (hora de este computador)."))
            else:
                print("No pude poner la hora: ¿el puerto está bien y el juego en marcha (no en modo boot)? Cierra otros programas que usen el puerto.")
            return ok
        self.tarea(f)

    def sacar_modo_boot(self):
        """La placa quedó en modo descarga (se encendió con BOOT apretado) y no arranca: se le ordena reiniciar y correr el firmware."""
        puerto = self.puerto.get().strip() or "COM5"
        def f():
            print(f"=== Sacar de modo boot (puerto {puerto}) ===")
            ok = self.reiniciar_normal(puerto)
            if not ok:
                print("No contestó. Con la placa apagada, SUELTA el botón BOOT y enciéndela normal (botón PWR, o sácale el cable USB y vuélvelo a conectar sin tocar BOOT).")
            else:
                print("Listo: la placa arrancó el juego.")
            self.cola.put(lambda: self.after(4000, self.ver_modo))
        self.tarea(f)

    # ---------- respaldo de la partida (partición nvs) y las fotos, leyendo la flash directo
    def esptool(self, args, intentos=("default_reset", "usb_reset", "no_reset"), timeout=300, despues="no_reset", progreso=None):
        """Corre esptool probando varias formas de entrar al bootloader. Devuelve (ok, texto).
        progreso(porcentaje 0-100): se llama mientras lee o escribe (esptool imprime «(NN %)»)."""
        flags = 0x08000000 if os.name == "nt" else 0
        ult = ""
        for antes in intentos:
            cmd = [self.python_idf(), "-m", "esptool", "--chip", "esp32s3", "-p", self.puerto.get().strip() or "COM5",
                   "--baud", "921600", "--before", antes, "--after", despues, "--connect-attempts", "3"] + args
            try:
                self.proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, creationflags=flags, stdin=subprocess.DEVNULL)
            except Exception as e:
                ult = str(e); continue
            fin, buf, texto = time.time() + timeout, b"", b""
            try:
                while True:
                    if probar.CANCELAR:
                        self.proc.terminate(); raise probar.Cancelado()
                    if time.time() > fin:
                        self.proc.terminate(); break
                    c = self.proc.stdout.read1(256) if hasattr(self.proc.stdout, "read1") else self.proc.stdout.read(1)
                    if not c:
                        break
                    texto += c; buf = (buf + c)[-80:]
                    if progreso:
                        m = re.findall(rb"\((\d+) ?%\)", buf)
                        if m:
                            progreso(int(m[-1]))
            finally:
                self.proc.wait()
            ult = texto.decode("utf-8", "replace")
            if self.proc.returncode == 0:
                return True, ult
        return False, ult

    CFG_RESP = os.path.join(os.path.expanduser("~"), ".mascota_respaldos.txt")

    def carpeta_respaldos(self):
        try:
            d = open(self.CFG_RESP, encoding="utf-8").read().strip()
            if d:
                return d
        except Exception:
            pass
        return os.path.join(os.path.expanduser("~"), "Documents", "Respaldos mascota")

    def elegir_carpeta_respaldos(self):
        from tkinter import filedialog
        d = filedialog.askdirectory(initialdir=self.var_dir.get() if os.path.isdir(self.var_dir.get()) else os.path.expanduser("~"), title="¿Dónde guardo los respaldos?")
        if d:
            d = os.path.normpath(d)
            self.var_dir.set(d)
            try:
                open(self.CFG_RESP, "w", encoding="utf-8").write(d)
            except Exception:
                pass

    def tabla_particiones(self, tmp):
        """Lee la tabla de particiones real de la placa: {nombre: (offset, tamaño)}."""
        ok, txt = self.esptool(["read_flash", "0x8000", "0xC00", tmp])
        if not ok:
            print(txt[-600:]); return None
        d = open(tmp, "rb").read()
        res = {}
        for i in range(0, len(d) - 31, 32):
            if d[i:i + 2] != b"\xaa\x50":
                break
            off, tam = int.from_bytes(d[i + 4:i + 8], "little"), int.from_bytes(d[i + 8:i + 12], "little")
            res[d[i + 12:i + 28].split(b"\0")[0].decode("ascii", "replace")] = (off, tam)
        return res

    def respaldar(self):
        import datetime, tempfile
        incluir = self.var_fotos.get()
        base = self.var_dir.get().strip() or self.carpeta_respaldos()
        def f():
            print("=== RESPALDAR MASCOTA ===")
            print("La placa entra sola al modo descarga (si no entra: BOOT apretado + RESET, suelta BOOT y repite). Al terminar vuelve a arrancar.")
            carpeta = os.path.join(base, datetime.datetime.now().strftime("%Y-%m-%d_%H-%M-%S"))
            os.makedirs(carpeta, exist_ok=True)
            self.poner_etapa("Conectando con la placa y leyendo su tabla"); self.poner_pct(3)
            tabla = self.tabla_particiones(os.path.join(carpeta, "_tabla.bin"))
            if not tabla or "nvs" not in tabla:
                print("No pude leer la placa. Ponla en modo descarga (BOOT + RESET) y vuelve a intentar."); return False
            self.poner_pct(10)
            hechos = []
            lista = ("nvs", "coredump", "fotos") if incluir else ("nvs", "coredump")  # coredump = el informe del último fallo
            pesos = {n: tabla[n][1] for n in lista if n in tabla}
            total_p = max(1, sum(pesos.values()))
            acum = 0
            for nombre in lista:
                if nombre not in tabla:
                    print(f"(la placa no tiene partición «{nombre}»)"); continue
                off, tam = tabla[nombre]
                print(f"Leyendo «{nombre}» (0x{off:X}, {tam // 1024} KB)…")
                ini = 10 + 88 * acum / total_p
                fin_p = 10 + 88 * (acum + tam) / total_p
                self.poner_etapa(f"Leyendo «{nombre}» de la placa")
                ok, txt = self.esptool(["read_flash", hex(off), hex(tam), os.path.join(carpeta, nombre + ".bin")], intentos=("no_reset", "default_reset"), timeout=900,
                                      despues="hard_reset" if nombre == lista[-1] else "no_reset",  # al terminar, la placa vuelve a arrancar
                                      progreso=lambda p, a=ini, b=fin_p: self.poner_pct(a + (b - a) * p / 100))
                acum += tam
                self.poner_pct(fin_p)
                if ok:
                    hechos.append(f"{nombre} 0x{off:X} {tam}")
                else:
                    print(txt[-500:]); print(f"Falló «{nombre}».")
            open(os.path.join(carpeta, "manifiesto.txt"), "w", encoding="utf-8").write("\n".join(hechos) + "\n")
            if hechos and hechos[0].startswith("nvs") and len(hechos) == len(lista):
                print(f"\nLISTO. Respaldo completo en: {carpeta}")
            elif hechos and hechos[0].startswith("nvs"):
                print(f"\nRESPALDO PARCIAL (la partida sí quedó; faltó alguna parte) en: {carpeta}")
                print("Para no perderla al actualizar: instala solo la app (el botón «Instalar» no toca la partida) y nunca uses «erase flash».")
            else:
                print("\nNO quedó respaldo de la partida. Revisa el puerto y el modo descarga.")
            self.cola.put(lambda: self.after(3000, self.ver_modo))
            return bool(hechos) and hechos[0].startswith("nvs") and len(hechos) == len(lista)
        self.tarea(f)

    def restaurar_respaldo(self):
        from tkinter import filedialog
        base = self.var_dir.get().strip()
        carpeta = filedialog.askdirectory(initialdir=base if os.path.isdir(base) else os.path.expanduser("~"), title="Carpeta del respaldo (la que tiene nvs.bin)")
        if not carpeta or not os.path.exists(os.path.join(carpeta, "manifiesto.txt")):
            return
        if not messagebox.askyesno("Restaurar", f"Esto reemplaza la partida de la mascota (y las fotos, si el respaldo las tiene) por las de:\n{carpeta}\n\n¿Seguro?"):
            return
        def f():
            print("=== RESTAURAR RESPALDO ===")
            partes = []
            for linea in open(os.path.join(carpeta, "manifiesto.txt"), encoding="utf-8").read().split("\n"):
                p = linea.split()
                if len(p) == 3:
                    partes.append((p[0], p[1], max(1, os.path.getsize(os.path.join(carpeta, p[0] + ".bin")) if os.path.exists(os.path.join(carpeta, p[0] + ".bin")) else 1)))
            total_p = max(1, sum(x[2] for x in partes))
            acum = 0
            self.poner_pct(3)
            for nombre, off, peso in partes:
                print(f"Escribiendo «{nombre}» en {off}…")
                ini = 5 + 93 * acum / total_p
                fin_p = 5 + 93 * (acum + peso) / total_p
                self.poner_etapa(f"Escribiendo «{nombre}» en la placa")
                ok, txt = self.esptool(["write_flash", off, os.path.join(carpeta, nombre + ".bin")], intentos=("no_reset", "default_reset"), timeout=900,
                                      progreso=lambda p, a=ini, b=fin_p: self.poner_pct(a + (b - a) * p / 100))
                acum += peso
                if not ok:
                    print(txt[-500:]); print(f"Falló «{nombre}»."); return False
                self.poner_pct(fin_p)
            self.poner_etapa("Reiniciando la placa"); self.poner_pct(96)
            puerto = self.puerto.get().strip() or "COM5"
            try:
                self.reiniciar_normal(puerto)  # que arranque sola con la partida restaurada
            except Exception:
                pass
            self.poner_etapa("Esperando que la placa arranque"); self.poner_pct(98)
            if probar.placa_responde(puerto, 25):
                self.poner_etapa("Poniendo la hora y la fecha en la placa")  # así no te la pregunta en pantalla
                probar.poner_hora(puerto, 15)
                print("LISTO. Partida restaurada y hora puesta (la de este computador).")
            else:
                print("LISTO, pero la placa no contestó: reiníciala (sácale el USB sin apretar BOOT, o usa «Sacar de modo boot») y aprieta «Poner la hora REAL».")
            self.cola.put(lambda: self.after(3000, self.ver_modo))
        self.tarea(f)

    # ---------- botones
    ULTIMO = os.path.join(AQUI, "_ultimo_zip.txt")

    def elegir_zip(self):
        """Pregunta cuál zip instalar (empieza siempre en la carpeta donde vive este programa)."""
        ini = AQUI
        z = filedialog.askopenfilename(title="Elige el firmware a instalar (mascota_firmware.zip)", initialdir=ini,
                                       filetypes=[("Firmware (zip)", "*.zip")])
        if z:
            try:
                open(self.ULTIMO, "w", encoding="utf-8").write(z)
            except OSError:
                pass
        return z or None

    def instalar(self, z=None):
        puerto = self.puerto_real()
        z = z or self.elegir_zip()
        if not z:
            return
        self.escribir(f"Firmware elegido: {z}  ({time.strftime('%d-%m %H:%M', time.localtime(os.path.getmtime(z)))})\n")
        def f():
            probar.idf = lambda args, cwd: self.idf(args, cwd)
            probar.buscar_zip = lambda: z
            print("=== INSTALAR FIRMWARE NUEVO ===")
            t_ini = time.time()
            self.poner_etapa("Extrayendo el firmware")
            self.poner_pct(1)
            probar.EXTRAER = lambda p: self.poner_pct(1 + 4 * p / 100, f"Extrayendo el firmware ({p} %)")
            if not probar.extraer_zip():
                return False
            self.poner_pct(5)
            if not probar.compilar():
                print("\nNo se pudo compilar. Usa «Copiar todo lo de abajo» y mándamelo.")
                return False
            t_comp = time.time()
            t_comp = time.time()
            self.etapa_fase = None
            self.flash_grande = False
            # 1) con el firmware nuevo corriendo, esptool reinicia la placa en modo descarga solo (USB nativo): se prueba directo
            self.poner_etapa("Instalando en el dispositivo")
            self.poner_modo("Modo: INSTALANDO (la placa pasa sola a BOOT / CARGA)", "#b35900")
            print("\nInstalando (la placa se pone sola en modo descarga si el firmware está en marcha)…")
            ok = probar.idf(["-p", puerto, "flash"], probar.FW)
            if not ok:
                # 2) se le pide por el puerto que se reinicie en modo descarga
                print("\nNo entró sola. Probando pedírselo por el puerto…")
                if probar.pedir_descarga(puerto):
                    print("Aceptó: reinicia en modo descarga.")
                    self.poner_modo("Modo: BOOT / CARGA (instalando)", "#b35900")
                    self.flash_grande = False
                    ok = probar.idf(["-p", puerto, "flash"], probar.FW)
            if not ok:
                # 3) a mano
                print("\nNo se pudo automático.")
                self.poner_etapa("Esperando que pongas la placa en modo descarga")
                self.poner_modo("Modo: esperando que la pongas en BOOT a mano", "#b35900")
                self.preguntar("Modo descarga", "Ahora pon la placa en modo descarga:\n\n1. Desconecta el USB.\n2. Mantén apretado el botón BOOT.\n"
                               "3. Conecta el USB sin soltar BOOT y suéltalo a los 2 segundos.\n\nLuego aprieta Aceptar.")
                self.flash_grande = False
                ok = probar.idf(["-p", puerto, "flash"], probar.FW)
            if not ok:
                print("\nNo se pudo grabar. Revisa el puerto (botón Buscar) y vuelve a intentar.")
                return False
            t_graba = time.time()
            self.poner_etapa("Esperando que la placa arranque")
            self.poner_modo("Modo: grabado; esperando que arranque", "gray")
            if not probar.placa_responde(puerto):
                print("La placa no arrancó sola: se le ordena reiniciar (sin desconectar el cable)…")
                self.poner_etapa("Reiniciando la placa")
                self.reiniciar_normal(puerto)
            if not probar.placa_responde(puerto):
                self.poner_etapa("Esperando que reconectes la placa")
                self.preguntar("Casi listo", "Grabado, pero la placa no arrancó sola.\n\nDesconecta el USB, espera 5 segundos y vuélvelo a conectar SIN tocar botones.\n"
                               "Cuando aparezca el juego en la pantalla, aprieta Aceptar.")
            self.poner_etapa("Poniendo la hora y la fecha en la placa")
            probar.poner_hora(puerto)
            self.poner_etapa("Copiando los fondos de los juegos de a dos")
            probar.enviar_recursos(puerto)
            t_fin = time.time()
            self.cola.put(self.buscar_puertos)
            self.poner_etapa("")
            self.poner_pct(100, "Listo")
            print(f"\nListo. Ya puedes probar.\nTiempo: compilar {probar.duracion(t_comp - t_ini)} · grabar {probar.duracion(t_graba - t_comp)} · total {probar.duracion(t_fin - t_ini)}")
            self.poner_pct(100, "Listo · " + probar.duracion(t_fin - t_ini))
        def g():
            try:
                return f()
            finally:
                self.cola.put(lambda: self.after(3000, self.ver_modo))
        self.tarea(g)

    # ---------- desbloquear contenido por tipo
    def elegir_cat(self, q, texto):
        puerto = self.puerto.get().strip() or "COM5"
        def f():
            self.poner_etapa(f"Pidiendo la lista de {texto.lower()}")
            nombres = probar.contenido_lista(puerto, q)
            if nombres is None:
                print(f"La placa no contestó la lista de {texto.lower()} (¿firmware nuevo instalado y juego en marcha?).")
                return
            if q == "estacion":
                idx, val = [-1] + list(range(len(nombres))), ["Según la fecha (automática)"] + [n.capitalize() for n in nombres]
            elif q == "clima":
                idx, val = list(range(len(nombres))), [n.capitalize() for n in nombres]
            else:
                idx, val = [-1] + list(range(len(nombres))), [f"TODOS ({len(nombres)})"] + [f"{i + 1}. {n}" for i, n in enumerate(nombres)]
            def poner():
                self.cat_q, self.cat_idx = q, idx
                self.cat_combo["values"] = val
                self.cat_combo.current(0)
                self.cat_lbl.config(text={"estacion": "Cambiar la estación a:", "clima": "Poner el clima:"}.get(q, f"Desbloquear {texto.lower()}:"))
            self.cola.put(poner)
            self.poner_etapa("")
        self.tarea(f)

    def aplicar_cat(self, verbo="ABRIR"):
        if not self.cat_q or self.cat_combo.current() < 0:
            messagebox.showinfo("Contenido", "Primero elige un tipo (Zonas, Objetos, Clima…) a la izquierda."); return
        q, idx = self.cat_q, self.cat_idx[self.cat_combo.current()]
        puerto, nombre = self.puerto.get().strip() or "COM5", self.cat_combo.get()
        def f():
            r = probar.contenido_aplicar(puerto, q, idx, verbo)
            print(f"{q}: {nombre} ({'bloqueado' if verbo == 'BLOQUEAR' else 'desbloqueado'}) -> " + ("listo" if r and "ERROR" not in r else f"no se pudo ({r})"))
        self.tarea(f)

    def calibrar_giro(self):
        """Mide cómo está montado el acelerómetro: dos posiciones con el orbe en la mano."""
        if not messagebox.showinfo("Calibrar giro (1 de 2)", "Sostén el orbe DERECHO, como lo usas normalmente (la pantalla hacia ti), y mantenlo quieto.\n\nPulsa Aceptar y no lo muevas un par de segundos."):
            pass
        self.probar_cmd("GIRO CAL 0")
        messagebox.showinfo("Calibrar giro (2 de 2)", "Ahora gíralo un cuarto de vuelta hacia la DERECHA (el lado derecho de la pantalla queda ABAJO) y mantenlo quieto.\n\nPulsa Aceptar.")
        self.probar_cmd("GIRO CAL 1")

    def armar_voces(self, padre):
        """Botones para oír las voces de la mascota y de los demás personajes (lee la lista de sonidos de core\\pet.h)."""
        try:
            t = open(os.path.join(PROYECTO, "core", "pet.h"), encoding="utf-8", errors="replace").read()
        except OSError:
            return
        ini = t.index("#define PET_SONIDOS(X)")
        fin = t.index("#define PET_SONIDO_ENUM")
        sonidos = re.findall(r'X\((SND_\w+),\s*"([^"]*)",\s*"([^"]*)"', t[ini:fin])  # (id, grupo, nombre); el número es su posición
        ttk.Label(padre, text=" Voces: toca para oír (suena en la placa, con el volumen de efectos). Desliza para ver más. ").pack(anchor="w", padx=8, pady=(4, 0))
        cont = ttk.Frame(padre); cont.pack(fill="x", padx=8, pady=4)
        lienzo = tk.Canvas(cont, height=170, highlightthickness=0)
        barra = ttk.Scrollbar(cont, orient="vertical", command=lienzo.yview)
        lienzo.configure(yscrollcommand=barra.set)
        barra.pack(side="right", fill="y"); lienzo.pack(side="left", fill="x", expand=True)
        marco = ttk.Frame(lienzo)
        lienzo.create_window((0, 0), window=marco, anchor="nw")
        marco.bind("<Configure>", lambda e: lienzo.configure(scrollregion=lienzo.bbox("all")))
        lienzo.bind("<Enter>", lambda e: lienzo.bind_all("<MouseWheel>", lambda ev: lienzo.yview_scroll(int(-ev.delta / 120), "units")))
        lienzo.bind("<Leave>", lambda e: lienzo.unbind_all("<MouseWheel>"))
        grupos = [("Mascota: ánimo", lambda g, i: g == "animo"), ("Mascota: cuerpo", lambda g, i: g == "cuerpo"),
                  ("Mascota: gestos", lambda g, i: i in ("SND_COMER", "SND_OLFATEO", "SND_HUELE", "SND_TOQUE", "SND_PELEA", "SND_LLAMADO", "SND_CAIDA", "SND_PELUCHE",
                                                          "SND_DESLIZA", "SND_TRUCO", "SND_ENCONTRADA", "SND_ATRAPA", "SND_META", "SND_CHAPUZON", "SND_CONTAR", "SND_HOLA_AMIGO")),
                  ("Pochi", lambda g, i: g == "pochi"), ("Canto", lambda g, i: g == "voz"), ("Otros personajes", lambda g, i: g == "personaje")]
        for titulo, filtro in grupos:
            lista = [(n, i, nom) for n, (i, g, nom) in enumerate(sonidos) if filtro(g, i)]
            if not lista:
                continue
            fila = ttk.LabelFrame(marco, text=" " + titulo + " ", padding=4)
            fila.pack(fill="x", pady=2)
            col = 0
            for n, i, nom in lista:
                b = ttk.Button(fila, text=nom, width=14, command=lambda n=n: self.probar_cmd(f"SONAR {n}"))
                b.grid(row=col // 6, column=col % 6, padx=2, pady=1)
                self.botones.append(b)
                col += 1

    def informe_fallo(self):
        """Lee el informe del último fallo (pánico) guardado en la flash de la placa y lo traduce con el programa compilado."""
        puerto = self.puerto.get().strip() or "COM5"
        def f():
            probar.idf = lambda args, cwd: self.idf(args, cwd)
            print("=== INFORME COMPLETO DEL FALLO (la placa pasa un momento a modo descarga) ===")
            ok = probar.idf(["-p", puerto, "coredump-info"], probar.FW)
            if not ok:
                print("No se pudo leer. Si dice que no hay informe, no hubo un fallo guardado desde que se instaló esta versión.")
            self.cola.put(lambda: self.after(4000, self.ver_modo))
        self.tarea(f)

    def decodificar(self):
        """Traduce las direcciones del último FALLO_BT del registro a archivo y línea del código (con el programa que se compiló)."""
        def f():
            import glob
            regs = sorted(glob.glob(os.path.join(PROYECTO, "registro_placa*.txt")) + glob.glob(os.path.join(probar.carpeta_datos(), "registro_placa*.txt")), key=os.path.getmtime)
            reg = regs[-1] if regs else os.path.join(PROYECTO, "registro_placa.txt")
            try:
                lineas = open(reg, encoding="utf-8", errors="replace").read().splitlines()
            except OSError:
                print("No encuentro ningún registro_placa*.txt: primero «Obtener log del dispositivo».")
                return
            fallo = [l for l in lineas if "FALLO:" in l]
            bts = [l for l in lineas if "FALLO_BT:" in l]
            if not bts:
                print("El registro no trae ningún FALLO_BT (no hubo un fallo guardado desde que se instaló esta versión).")
                return
            print(fallo[-1].strip() if fallo else "")
            dirs = re.findall(r"0x[0-9a-fA-F]{8}", bts[-1].split("FALLO_BT:", 1)[1])
            elf = os.path.join(PROYECTO, "firmware", "build", "mascota.elf")
            if not os.path.exists(elf):
                print("No encuentro", elf, "(hay que compilar primero)")
                return
            exe = None
            for base in {self.idf_tools, r"C:\Espressif", r"C:\Espressif\tools", os.path.expanduser(r"~\.espressif")}:
                for pat in ("tools", ""):
                    hall = glob.glob(os.path.join(base, pat, "xtensa-esp-elf", "*", "xtensa-esp-elf", "bin", "xtensa-esp32s3-elf-addr2line.exe"))
                    if hall:
                        exe = sorted(hall)[-1]
                        break
                if exe:
                    break
            if not exe:
                print("No encuentro xtensa-esp32s3-elf-addr2line.exe en", self.idf_tools)
                return
            r = subprocess.run([exe, "-pfiaC", "-e", elf] + dirs, capture_output=True, text=True)
            print("Dónde falló (de adentro hacia afuera):")
            print(r.stdout or r.stderr)
        self.tarea(f)

    def cargar_llave(self):
        from tkinter import simpledialog
        k = simpledialog.askstring("Llave de informes", "Pega la llave (empieza con github_pat_):", show="*", parent=self)
        if not k:
            return
        k = k.strip()
        if len(k) < 20 or any(c.isspace() for c in k):
            messagebox.showerror("Llave de informes", "Eso no parece una llave de GitHub.")
            return
        self.orden_llave(k, secreta=True)

    def orden_llave(self, arg, secreta=False):
        puerto = self.puerto.get().strip() or "COM5"
        def f():
            p = probar.Placa(puerto)
            try:
                p.s.reset_input_buffer()
                p.s.write(("MASCOTA LLAVE " + arg + "\n").encode())
                r = [l for l in p.leer(3) if l.startswith("MASCOTA OK") or l.startswith("MASCOTA ERROR") or l.startswith("MASCOTA LLAVE")]
            finally:
                p.s.close()
            que = "cargar la llave" if secreta else f"LLAVE {arg}"
            if not r:
                print(f"{que}: la placa no contestó (¿tiene la versión 1.0.20 o más nueva?)")
            elif r[0] == "MASCOTA LLAVE SI":
                print("La placa YA tiene la llave de informes.")
            elif r[0] == "MASCOTA LLAVE NO":
                print("La placa NO tiene llave de informes.")
            elif r[0].startswith("MASCOTA OK"):
                print(f"{que}: listo." + (" Ya puedes usar Ajustes → Versión → ENVIAR INFORME en el orbe." if secreta else ""))
            else:
                print(f"{que}: no se pudo ({r[0]})")
        self.tarea(f)

    def probar_cmd(self, orden):
        puerto = self.puerto.get().strip() or "COM5"
        def f():
            p = probar.Placa(puerto)
            try:
                r = p.cmd(orden, 0.5)
            finally:
                p.s.close()
            print(f"{orden} -> " + (f"listo: {r}" if r and "ERROR" not in r else f"no se pudo ({r})"))
        self.tarea(f)

    # qué hace cada botón de prueba: (nombre, qué hace, qué necesitas tú, cuánto dura)
    AYUDA_PRUEBAS = {
        "total": ("PRUEBA TOTAL (todo automático)", "Revisa todo sola: pantalla, sonido, cielo, zonas, acciones, ropa, bolsa, animales, ahorro de energía, paneles, historias, alarmas, reinicios y más. Al final baja el registro de la placa.", "Nada: no hace falta tocar la placa. Déjala enchufada.", "1 hora aprox."),
        "total_guiado": ("PRUEBA TOTAL + guiada (con tus manos)", "Igual que la prueba total, pero al comienzo una ventana te guía paso a paso para tocar la pantalla, sacudir, girar, apretar PWR, soplar y desenchufar el cable. Después sigue sola.", "Tener la placa en la mano al comienzo (unos 5 minutos). Luego te puedes olvidar.", "1 hora aprox."),
        "nuevo": ("Probar lo nuevo (guiada)", "Revisa lo de las versiones 1.0.16 a 1.0.23: los 6 fondos de los juegos, los juegos de a dos y de la isla (se juegan solos y se cierran), tocar el faro y la piedra en la isla, que el micrófono se encienda solo con niebla o mal olor, la velocidad con y sin la mejora de la 1.0.22 (lluvia, nieve, tormenta, noche) y la memoria. Al final baja el registro.", "Tener el orbe en la mano cuando te lo pida: SOPLAR la niebla, el mal olor y la vela del bote (3 veces, unos segundos cada una).", "10–15 min"),
        "nuevo_auto": ("Probar lo nuevo (sin manos)", "Lo mismo que la guiada, sin las pruebas de soplar (esas quedan como aviso). Al final baja el registro.", "Nada: déjala enchufada.", "10 min aprox."),
        "rendimiento": ("RENDIMIENTO automático", "Mide velocidad y memoria: cada zona, estación y hora, climas, niebla, casita, paneles y juegos. Resume lo más lento y lo que más memoria gasta.", "Nada.", "20–30 min"),
        "reinicios": ("Solo REINICIOS (3 veces)", "Reinicia la placa 3 veces seguidas y mide si vuelve a contestar. Sirve para ver si el reinicio por software la deja colgada. Baja el registro al final.", "Nada. Si la placa no vuelve: saca la batería, vuelve a ponerla y usa «Obtener log del dispositivo».", "2–3 min"),
        "total_continuar": ("CONTINUAR la prueba cortada", "Retoma la última prueba total que se cortó: salta las fases ya hechas y repite solo desde la fase donde quedó (esa la hace entera). El informe junta lo de antes con lo nuevo.", "Que la placa esté encendida y conectada. Si era la guiada, las pruebas con las manos no se repiten.", "lo que falte"),
        "descarga_corta": ("Prueba de DESCARGA corta (1 h)", "Pone la mascota en modo prueba: queda con una pantalla fija y corre sola 3 etapas de 20 min (peor caso con todo al máximo, uso normal y dormida). Anota batería, temperatura, frecuencia del procesador y carga de los núcleos cada 2 min.", "Tener la batería al 90 % o más. Desenchufar el cable USB cuando te lo pida y no tocar la mascota. Después, «RESULTADO de la descarga».", "1 hora aprox."),
        "descarga_larga": ("Prueba de DESCARGA larga (hasta 10 %)", "Igual que la corta, pero repite las etapas hasta que la batería llegue al 10 %. Da la autonomía real en cada modo.", "Batería al 90 % o más, cable desenchufado, mascota quieta y a temperatura normal. PWR dos veces la cancela.", "4 a 8 horas"),
        "descarga_resultado": ("RESULTADO de la descarga", "Baja el registro de la placa y calcula por etapa: % de batería por hora, horas de autonomía, temperaturas, frecuencia medida del procesador y carga de cada núcleo.", "Enchufar la mascota al computador cuando haya terminado la prueba.", "1–2 min"),
        "tamano": ("Comparar TAMAÑOS 100/120/130/140", "Prueba el tamaño de las criaturas en todas partes (100, 120, 130 y 140 %) y compara velocidad.", "Nada. Deja el tamaño que tenías guardado.", "10 min aprox."),
        "todo": ("Probar todo", "Corre las pruebas clásicas: paneles, juegos, zonas, secretos, finales, actividades y cantar.", "Nada.", "20 min aprox."),
        "juegos": ("Juegos", "Abre cada minijuego y comprueba que arranca y termina sin colgarse.", "Nada.", "5 min"),
        "zonas": ("Zonas", "Visita cada zona y comprueba que se dibuja y que la mascota se mueve.", "Nada.", "5 min"),
        "secretos": ("Secretos", "Abre los secretos del juego y comprueba que responden.", "Nada.", "5 min"),
        "finales": ("Finales", "Corre los finales (incluye el secreto) y comprueba que terminan bien.", "Nada.", "10 min"),
        "paneles": ("Paneles", "Abre uno por uno todos los paneles (menús, bolsa, diario…) y mide tiempo y memoria de cada uno.", "Nada.", "5 min"),
        "actividades": ("Actividades", "Hace las actividades de la mascota (comer, jugar, dormir…) y comprueba el resultado.", "Nada.", "5 min"),
        "cantar": ("Modo cantar", "Prueba el modo cantar con el micrófono.", "Que haya silencio; a veces te pide cantar o hacer ruido.", "3 min"),
        "sonido": ("Sonido", "Reproduce los sonidos y mide volumen y respuesta.", "Escuchar: debe sonar el parlante.", "3 min"),
        "info": ("Ver estado", "Muestra el estado actual de la placa (memoria, panel abierto, batería…). No cambia nada.", "Nada.", "segundos"),
        "escuchar": ("Escuchar a la placa", "Muestra en vivo lo que la placa va diciendo por el cable, hasta que lo detengas.", "Nada.", "hasta que lo cierres"),
        "registro": ("Obtener log del dispositivo", "Baja el registro guardado en la placa (arranques, motivos de reinicio, avisos) y lo copia para pegarlo en el chat.", "Nada.", "1–2 min"),
        "registro_borrar": ("Borrar registro", "Borra el registro guardado en la placa. La mascota no se toca.", "Nada.", "segundos"),
        "restaurar": ("Restaurar partida", "Devuelve la mascota a como estaba antes de una prueba (usa el respaldo que la placa hizo al empezar).", "Nada.", "segundos"),
        "de_cero": ("Empezar de cero", "BORRA la mascota, el mundo y los objetos de la placa y la reinicia.", "Estar segura de querer perder la partida.", "segundos"),
    }

    def probar(self, que):
        ay = self.AYUDA_PRUEBAS.get(que)
        if ay:
            txt = f"{ay[1]}\n\nQué necesitas hacer tú: {ay[2]}\nDuración: {ay[3]}\n\n¿Quieres hacerla ahora?"
            if not messagebox.askyesno(ay[0], txt):
                return
        if que == "de_cero" and not messagebox.askyesno("Empezar de cero", "Esto borra la mascota, el mundo y los objetos guardados en la placa y la reinicia.\n\n¿Seguro?"):
            return
        puerto = self.puerto.get().strip() or "COM5"
        def f():
            print(f"=== PROBAR: {que} (puerto {puerto}) ===")
            probar.PROGRESO = lambda p: self.poner_pct(p)
            probar.ETAPA = lambda t: self.poner_etapa(t)
            probar.REINICIAR = lambda pto: self.reiniciar_normal(pto)
            probar.PREGUNTAR = self.preguntar_si
            probar.AVISAR = self.preguntar
            probar.GUIA = self
            self.poner_etapa("Conectando con la placa")
            probar.correr(puerto, que)
            if que == "registro" and probar.ULTIMO_REGISTRO:  # queda copiado: solo hay que pegarlo en el chat
                texto_log = probar.ULTIMO_REGISTRO
                self.after(0, lambda: (self.clipboard_clear(), self.clipboard_append(texto_log), self.update()))
                print("\n(El log ya está copiado al portapapeles: pégalo en el chat. También está aquí abajo para copiarlo a mano.)")
            if que not in ("info", "escuchar", "registro", "registro_borrar", "de_cero", "restaurar"):
                self.poner_etapa("")
                self.poner_pct(100, "Listo")
            if que not in ("info", "escuchar", "registro", "registro_borrar", "de_cero", "restaurar"):
                print("\nTerminó. Aprieta «Copiar informe» y pégalo en el chat.")
        self.tarea(f)

    def copiar(self, texto, aviso):
        self.clipboard_clear(); self.clipboard_append(texto); self.update()
        messagebox.showinfo("Copiado", aviso)

    def copiar_informe(self):
        ruta = os.path.join(os.getcwd(), "informe.txt")
        if not os.path.exists(ruta):
            ruta = os.path.join(probar.carpeta_datos(), "informe.txt")
        if not os.path.exists(ruta):
            messagebox.showinfo("Informe", "Todavía no hay informe: haz una prueba primero."); return
        self.copiar(open(ruta, encoding="utf-8").read(), "Informe copiado. Pégalo en el chat.")

    def copiar_log(self):
        self.copiar(self.log.get("1.0", "end"), "Todo el texto copiado. Pégalo en el chat.")

if __name__ == "__main__":
    os.chdir(probar.RAIZ)  # el informe queda en la carpeta del proyecto
    App().mainloop()
