#!/usr/bin/env python3
"""Arma editor.html: mete datos.js (íconos, sonidos y celebraciones de fábrica) dentro de plantilla.html.
Uso: primero  ./build/mascota_editor_datos ../editor  (desde sim/preview), después  python3 build.py"""
import pathlib, sys
aqui = pathlib.Path(__file__).resolve().parent
plantilla = (aqui / "plantilla.html").read_text(encoding="utf-8")
datos = (aqui / "datos.js").read_text(encoding="utf-8")
if "__DATOS_JS__" not in plantilla:
    sys.exit("plantilla.html no tiene __DATOS_JS__")
salida = aqui / "editor.html"
salida.write_text(plantilla.replace("__DATOS_JS__", datos.replace("</script", "<\\/script")), encoding="utf-8")
print(f"{salida.name}: {salida.stat().st_size // 1024} KB")
