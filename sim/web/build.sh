#!/bin/sh
# Compila el simulador web (motor + interfaz del firmware + LVGL) a WebAssembly.
#
#   WASI_SDK=/ruta/a/wasi-sdk LVGL_DIR=/ruta/a/lvgl-9.4 sh sim/web/build.sh
#
# Requiere wasi-sdk (https://github.com/WebAssembly/wasi-sdk/releases) y LVGL 9.4.
# Produce sim/web/mascota.wasm.
set -e
RAIZ=$(cd "$(dirname "$0")/../.." && pwd)
: "${WASI_SDK:?define WASI_SDK}"
: "${LVGL_DIR:?define LVGL_DIR}"

"$WASI_SDK/bin/clang" --target=wasm32-wasip1 --sysroot="$WASI_SDK/share/wasi-sysroot" \
  -O2 -mexec-model=reactor -Wno-unused-command-line-argument \
  -DLV_CONF_INCLUDE_SIMPLE \
  -I"$RAIZ/sim/preview" -I"$LVGL_DIR" -I"$RAIZ/core" -I"$RAIZ/firmware/main" \
  $(find "$LVGL_DIR/src" -name '*.c') \
  "$RAIZ/core/pet.c" "$RAIZ/core/sfx.c" "$RAIZ/core/foto.c" "$RAIZ/core/objetos.c" "$RAIZ/firmware/main/ui.c" "$RAIZ/firmware/main/art.c" "$RAIZ/sim/web/web_main.c" \
  -Wl,--wrap=lv_obj_set_local_style_prop -Wl,--wrap=lv_obj_invalidate -Wl,--wrap=lv_obj_move_to_index -Wl,--strip-all -Wl,-z,stack-size=262144 -Wl,--initial-memory=33554432 \
  -o "$RAIZ/sim/web/mascota.wasm"
ls -l "$RAIZ/sim/web/mascota.wasm"

# Respaldo en JavaScript puro (para navegadores o páginas que bloquean WebAssembly).
# Requiere binaryen (https://github.com/WebAssembly/binaryen/releases): BINARYEN=/ruta/a/binaryen
: "${BINARYEN:?define BINARYEN}"
"$BINARYEN/bin/wasm2js" --all-features -O2 "$RAIZ/sim/web/mascota.wasm" -o "$RAIZ/sim/web/motor.mjs"
python3 - "$RAIZ/sim/web" <<'PY'
import sys, pathlib, re
d = pathlib.Path(sys.argv[1])
s = (d / "motor.mjs").read_text(encoding="utf-8")
s = s.replace("import * as wasi_snapshot_preview1 from 'wasi_snapshot_preview1';", "")
s = re.sub(r"^export var .*$", "", s, flags=re.M)
llamada = 'var retasmFunc = asmFunc({\n  "wasi_snapshot_preview1": wasi_snapshot_preview1,\n});'
assert llamada in s, "cambio el formato de wasm2js"
s = s.replace(llamada, 'return asmFunc({ "wasi_snapshot_preview1": imports });')
assert "</script" not in s
(d / "motor.js").write_text("function crearMotorJS(imports) {\n" + s + "\n}\n", encoding="utf-8")
(d / "motor.mjs").unlink()
PY

# Página única con el .wasm incrustado (se abre directo en el navegador, sin servidor)
python3 - "$RAIZ/sim/web" <<'PY'
import base64, sys, pathlib
d = pathlib.Path(sys.argv[1])
b = base64.b64encode((d / "mascota.wasm").read_bytes()).decode()
motor = (d / "motor.js").read_text(encoding="utf-8")
html = (d / "plantilla.html").read_text(encoding="utf-8").replace("__WASM_B64__", b).replace("__MOTOR_JS__", motor)
(d / "simulador.html").write_text(html, encoding="utf-8")
print("simulador.html:", len(html) // 1024, "KB")
PY

# Versión "solo el reloj" para el celular: sin paneles de prueba, con botón de sonido y de bloqueo de pantalla
python3 - "$RAIZ/sim/web" <<'PY'
import sys, pathlib
d = pathlib.Path(sys.argv[1])
h = (d / "simulador.html").read_text(encoding="utf-8")
css = """<style>
body { margin: 0; padding: 0 !important; background: #000 !important; min-height: 100dvh; display: grid; place-items: center; }
header, .controls, .side, .clock, footer, .boot, .bootlabel, .strap, .main > .side, .hint, .offhint { display: none !important; }
.wrap { padding: 0 !important; max-width: none; gap: 0; }
.main { display: block; }
.bench { padding: 0; gap: 0; }
.watch { width: min(92vw, 100dvh - 110px) !important; max-width: none; margin: 8px auto 0 !important; }
html, body { overflow-x: hidden; background: #000 !important; }
.wrap, .main, .bench { width: 100vw; }
#barra-solo { display: flex; gap: 14px; justify-content: center; padding: 10px 0 18px; }
#barra-solo button { font: 600 16px system-ui, sans-serif; color: #fff; background: #2a2a3a; border: 0; border-radius: 22px; padding: 12px 20px; min-width: 132px; }
#barra-solo button[aria-pressed="true"] { background: #5b4bb0; }
#velo { position: absolute; inset: 0; z-index: 9; border-radius: 50%; touch-action: none; display: none; background: rgba(0,0,0,.28); }
#velo.on { display: grid; place-items: center; color: #fff; font: 600 15px system-ui, sans-serif; text-shadow: 0 1px 4px #000; }
</style>"""
barra = """<div id="barra-solo"><button id="b-sonido" aria-pressed="false">🔇 Sonido</button><button id="b-bloqueo" aria-pressed="false">🔓 Bloquear</button></div>
<script>
addEventListener("load", () => {
  const glass = document.getElementById("glass"), velo = document.createElement("div");
  velo.id = "velo"; glass.appendChild(velo);
  const bs = document.getElementById("b-sonido"), bb = document.getElementById("b-bloqueo"), real = document.getElementById("sonido");
  bs.addEventListener("click", () => { real.click(); const on = real.getAttribute("aria-pressed") === "true"; bs.setAttribute("aria-pressed", on); bs.textContent = on ? "🔊 Sonido" : "🔇 Sonido"; });
  let mantener = null;
  bb.addEventListener("click", async () => {
    const on = bb.getAttribute("aria-pressed") !== "true";
    bb.setAttribute("aria-pressed", on); bb.textContent = on ? "🔒 Desbloquear" : "🔓 Bloquear";
    velo.classList.toggle("on", on); velo.textContent = on ? "pantalla bloqueada" : "";
    try { if (on && navigator.wakeLock) mantener = await navigator.wakeLock.request("screen"); else if (mantener) { mantener.release(); mantener = null; } } catch (e) {}
  });
});
</script>"""
assert "</head>" in h or "</style>" in h
h = h.replace("</style>", "</style>" + css, 1) if "</head>" not in h else h.replace("</head>", css + "</head>", 1)
h = h.replace('<div class="wrap">', '<div class="wrap">', 1)
i = h.rfind("</body>")
h = (h[:i] + barra + h[i:]) if i >= 0 else h + barra
(d / "simulador_reloj.html").write_text(h, encoding="utf-8")
print("simulador_reloj.html:", len(h) // 1024, "KB")
PY
