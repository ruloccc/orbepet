"""Antes de entregar un zip: comprueba que cada arreglo ya hecho siga en el código (que nada se pierda entre versiones).
Cada línea: (versión, qué es, archivo, texto que tiene que estar). Al hacer un arreglo nuevo, agregarlo aquí.
Uso: python herramientas/revisar_cambios.py   -> «todo está» o la lista de lo que falta (y sale con error)."""
import os, sys
R = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
C = [
    ("1.0.46", "fondo y «LLAMANDO A …» al traer otra mascota", "firmware/main/ui.c", "s_cambio_av_tapa"),
    ("1.0.47", "cartel único del Rincón (textos sin encimarse)", "firmware/main/ui.c", "static void decor_aviso("),
    ("1.0.48", "lluvia sin rayas: una sola baldosa", "firmware/main/services.c", "lv_display_set_tile_cnt("),
    ("1.0.49", "estrella de lo nuevo del baúl", "firmware/main/ui.c", "s_acc_nuevo"),
    ("1.0.50", "hedionda: soplar o bañar", "firmware/main/ui.c", "pet_stinky("),
    ("1.0.51", "registro de imágenes para medir", "firmware/main/art.c", "art_mayores"),
    ("1.0.52", "cada animal con su nombre al asustarse", "firmware/main/ui.c", "pet_animal_name("),
    ("1.0.53", "afinación de los cristales", "core/pet.h", "afinacion[8]"),
    ("1.0.53", "cristales de las orillas", "firmware/main/ui_magia.inc", "{58, 290, 4}"),
    ("1.0.54", "Bluetooth apagado", "firmware/sdkconfig.defaults", "CONFIG_BT_ENABLED=n"),
    ("1.0.54", "modo paseo", "firmware/main/services.c", "hal_walk_enter"),
    ("1.0.55", "luces de los hongos se sueltan", "firmware/main/ui.c", "hongos_luces("),
    ("1.0.55b", "CMake con el Bluetooth apagado", "firmware/CMakeLists.txt", "INTERFACE_LIBRARY"),
    ("1.0.56", "faro: encallar pierde punto", "firmware/main/ui_isla.inc", "PIERDES UN PUNTO"),
    ("1.0.56b", "instalador busca en otros puertos", "herramientas/mascota_gui.py", "def grabar_en_algun_puerto"),
    ("1.0.58", "cuentapasos", "firmware/main/ui.c", "static void panel_pasos("),
    ("1.0.59", "silencio con la pantalla apagada + almacenamiento", "firmware/main/services.c", "hal_res_informe"),
    ("1.0.60", "despertar sin luz blanca", "firmware/main/main.c", "ya_subio"),
    ("1.0.61", "efectos en su propia tarea (notas a tiempo)", "firmware/main/services.c", "tarea_sfx"),
    ("1.0.61", "botón afinar", "firmware/main/ui_magia.inc", "s_afin_btn"),
    ("1.0.61", "mantel quieto en la visita", "firmware/main/ui.c", "s_vec_mantel_puesto"),
    ("1.0.61", "cristalitos en la boca de la cueva", "firmware/main/ui_zonas.inc", "ICO_CRIS_0"),
    ("1.0.62", "lluvia de la regadera", "firmware/main/ui_magia.inc", "rg_gotas_animar"),
    ("1.0.63", "mantener la nota: todas a su tono", "firmware/main/ui_magia.inc", "afin_todas_cb"),
    ("1.0.64", "registro en su partición", "firmware/partitions.csv", "registro, data, 0x40"),
    ("1.0.64", "registro en anillo", "firmware/main/services.c", '#include "anillo.inc"'),
    ("1.0.64", "foto con menos memoria", "firmware/main/ui.c", 'hacer_lugar("la foto"'),
    ("1.0.65", "regadera nueva en la tabla HD", "firmware/main/iconos_hd.inc", "{ICO_REGADERA, HDI_REGADERA_PAL, 12,"),
]
falta = [c for c in C if not os.path.exists(os.path.join(R, c[2])) or c[3] not in open(os.path.join(R, c[2]), encoding="utf-8").read()]
for v, que, a, t in falta:
    print(f"FALTA {v}: {que} ({a}: «{t}»)")
print("todo está" if not falta else f"faltan {len(falta)} de {len(C)}")
sys.exit(1 if falta else 0)
