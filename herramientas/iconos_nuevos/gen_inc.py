import sys,os
os.environ['ICN']='96'
sys.path.insert(0,os.path.dirname(os.path.abspath(__file__)))
from iconos import *
import numpy as np
out=['/* GENERADO por herramientas/iconos_nuevos (gen_inc.py): íconos tiernos pintados a 96x96 (paleta ARGB + índices; 0 = transparente). No editar. */']
tab=[]
for nombre,f in JUEGO:
    im=f().convert('RGBA'); a=np.array(im)[...,3]
    rgb=im.convert('RGB').quantize(120,method=Image.MEDIANCUT,dither=Image.NONE)
    pal=rgb.getpalette()[:120*3]; idx=np.array(rgb)
    idx=np.where(a>110,idx+1,0).astype(np.uint8)
    used=int(idx.max())
    palb=[]
    for k in range(used):
        palb+= [255,pal[k*3],pal[k*3+1],pal[k*3+2]]
    base=nombre[4:]
    out.append(f'static const uint8_t HD2_{base}_PAL[{len(palb)}] = {{'+','.join(map(str,palb))+'};')
    out.append(f'static const uint8_t HD2_{base}_IDX[{96*96}] = {{'+','.join(map(str,idx.flatten().tolist()))+'};')
    tab.append(f'    {{{nombre}, HD2_{base}_PAL, {used}, HD2_{base}_IDX}},')
out.append('#define HD2_LADO 96')
out.append('static const struct { int ico; const uint8_t *pal; int n; const uint8_t *idx; } HD2[] = {')
out+=tab; out.append('};')
open(sys.argv[1],'w').write('\n'.join(out)+'\n')
print('listo',len(JUEGO))
