"""(v99) Los personajes y animales que estaban pintados «tiernos» a 96x96 (Nimba, Brizna, Prisma, Tilde, el chucao,
el cóndor y los animales chilenos del bosque, el claro y el mirador) se veían de otro juego. Este script los vuelve a
pintar como PIXEL ART a 48x48, igual que los demás animales del mundo (pix48.py), los saca de iconos_hd2.inc y los
agrega a iconos_hd.inc. Peumo usa el dibujo del Cuidador (es él). Uso: python3 gen_pixel48.py <carpeta firmware/main>"""
import os,sys,re
sys.path.insert(0,os.path.dirname(os.path.abspath(__file__)))
from pix48 import JUEGO
import numpy as np
from PIL import Image
LISTA=['ICO_NIMBA','ICO_BRIZNA','ICO_BRIZNA_B','ICO_BRIZNA_C','ICO_PRISMA','ICO_ERIZO','ICO_ERIZO_B','ICO_ERIZO_BOLA',
 'ICO_ARDILLA','ICO_ARDILLA_B','ICO_BUHO','ICO_BUHO_B','ICO_CISNE','ICO_CISNE_B','ICO_CUERVO','ICO_CUERVO_B','ICO_MAPACHE',
 'ICO_MAPACHE_B','ICO_CIERVO_CAFE','ICO_CIERVO_CAFE_B','ICO_TEJON','ICO_TEJON_B','ICO_GATO_MONTES','ICO_GATO_MONTES_B',
 'ICO_LINCE','ICO_LINCE_B','ICO_SALAMANDRA','ICO_SALAMANDRA_B','ICO_FOCA','ICO_OSO_POLAR','ICO_CIERVO','ICO_CIERVO_B',
 'ICO_CIERVO_C','ICO_CIERVO_D','ICO_TILDE','ICO_TILDE_B','ICO_CHUCAO','ICO_CHUCAO_B','ICO_CONDOR','ICO_CONDOR_B','ICO_PINGUINO']
SACAR=LISTA+['ICO_TILO']
def a_inc(nombre,im):
    a=np.array(im.convert('RGBA')); m=a[...,3]>0
    cols={}; idx=np.zeros((48,48),np.uint8)
    for y in range(48):
        for x in range(48):
            if m[y,x]:
                c=tuple(int(v) for v in a[y,x,:3])
                if c not in cols: cols[c]=len(cols)+1
                idx[y,x]=cols[c]
    assert len(cols)<=255,nombre
    pal=[]
    for c in sorted(cols,key=cols.get): pal+=[255,*c]
    b=nombre[4:]
    arr=(f'static const uint8_t HPX_{b}_PAL[{len(pal)}] = {{'+','.join(map(str,pal))+'};\n'
         f'static const uint8_t HPX_{b}_IDX[2304] = {{'+','.join(map(str,idx.flatten().tolist()))+'};\n')
    return arr, f'    {{{nombre}, HPX_{b}_PAL, {len(cols)}, HPX_{b}_IDX}},'
def main(carpeta):
    p1=os.path.join(carpeta,'iconos_hd.inc'); p2=os.path.join(carpeta,'iconos_hd2.inc')
    hd=open(p1).read(); hd2=open(p2).read()
    if 'HPX_' in hd:   # rehacer: sacar lo que puso la vez anterior
        hd=re.sub(r'/\* \(v99\) personajes.*?\*/\n(static const uint8_t HPX_[^\n]*\n)*','',hd,flags=re.S)
        hd=re.sub(r'    \{ICO_\w+, HPX_[^\n]*\n','',hd)
        hd=re.sub(r'    \{ICO_TILO, HDI_CUIDADOR_PAL[^\n]*\n','',hd)
    # 1) fuera de HD2: sus filas de la tabla y sus arreglos
    for n in SACAR:
        b=n[4:]
        hd2=re.sub(r'    \{'+n+r', HD2_'+b+r'_PAL, \d+, HD2_'+b+r'_IDX\},\n','',hd2)
        hd2=re.sub(r'static const uint8_t HD2_'+b+r'_(PAL|IDX)\[\d+\] = \{[^}]*\};\n','',hd2)
    # 2) adentro de HDI
    arrs=[]; filas=[]
    for n in LISTA:
        a,f=a_inc(n,JUEGO[n]()); arrs.append(a); filas.append(f)
    filas.append('    {ICO_TILO, HDI_CUIDADOR_PAL, %s, HDI_CUIDADOR_IDX}, /* Peumo es el Cuidador: el mismo dibujo */'
                 % re.search(r'\{ICO_CUIDADOR, HDI_CUIDADOR_PAL, (\d+),',hd).group(1))
    hd=hd.replace('#define HDI_LADO 48','/* (v99) personajes y animales pasados a pixel art: herramientas/iconos_nuevos/gen_pixel48.py */\n'+''.join(arrs)+'#define HDI_LADO 48')
    k=hd.rstrip().rfind('};'); hd=hd[:k]+'\n'.join(filas)+'\n};\n'
    open(p1,'w').write(hd); open(p2,'w').write(hd2)
    print('listo:',len(LISTA),'a pixel art; Peumo = el Cuidador')
if __name__=='__main__': main(sys.argv[1])
