"""(v98) Quita de firmware/main/iconos_hd.inc los íconos de 48x48 que también están en iconos_hd2.inc (96x96, "tiernos"):
el firmware siempre usa primero los de iconos_hd2, así que los otros nunca se veían y solo ocupaban flash (~100 KB).
Correr después de regenerar iconos_hd.inc o iconos_hd2.inc.   Uso: python herramientas/podar_iconos_hd.py"""
import os, re
RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
HD = os.path.join(RAIZ, "firmware", "main", "iconos_hd.inc")
HD2 = os.path.join(RAIZ, "firmware", "main", "iconos_hd2.inc")
a = open(HD, encoding="utf-8").read()
b = open(HD2, encoding="utf-8").read()
en2 = set(re.findall(r"\{(ICO_[A-Z0-9_]+), HD2_", b))
quitar = []
for ico, pal, idx in re.findall(r"    \{(ICO_[A-Z0-9_]+), (HDI_[A-Z0-9_]+_PAL), \d+, (HDI_[A-Z0-9_]+_IDX)\},\n", a):
    if ico in en2:
        quitar.append((ico, pal, idx))
antes = len(a)
for ico, pal, idx in quitar:
    a = re.sub(r"    \{%s, %s, \d+, %s\},\n" % (ico, pal, idx), "", a)
    a = re.sub(r"static const uint8_t %s\[\d+\] = \{[^}]*\};\n" % pal, "", a)
    a = re.sub(r"static const uint8_t %s\[\d+\] = \{[^}]*\};\n" % idx, "", a)
open(HD, "w", encoding="utf-8").write(a)
print("quitados %d íconos repetidos (%d KB de texto menos)" % (len(quitar), (antes - len(a)) // 1024))
