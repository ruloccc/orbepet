# (1.0.53) YA APLICADO a firmware/main/cristales_data.inc: mueve los dos cristales de las orillas de la cueva (borra el
# cristal y su halo rellenando con el fondo de alrededor, y los vuelve a pegar más cerca con un halo nuevo).
# NO volver a correrlo con «grabar»: los movería otra vez. Uso para mirar: python3 mover_cristales.py 9 33 -9 39
import re,sys,math
sys.path.insert(0,'/home/claude/w/herramientas')
from lzss import lz_decode, lz_encode
from PIL import Image
SRC='/home/claude/w/firmware/main/cristales_data.inc'
s=open(SRC).read()
def arr(name):
    m=re.search(r'static const uint8_t '+name+r'\[\d+\] = \{([^}]*)\}',s); return bytes(int(x) for x in m.group(1).split(','))
pal=arr('CJ_FONDO_PAL'); lz=arr('CJ_FONDO_LZ'); N=233
ix=list(lz_decode(lz,N*N)); orig=ix[:]
cols=[(16,20,24)]+[(pal[1+i*3],pal[2+i*3],pal[3+i*3]) for i in range(pal[0])]
def lum(c): return 0.3*c[0]+0.59*c[1]+0.11*c[2]
def cerca(rgb):
    best=None;bd=1e9
    for i,c in enumerate(cols):
        if i==0: continue
        d=(c[0]-rgb[0])**2+(c[1]-rgb[1])**2+(c[2]-rgb[2])**2
        if d<bd: bd,best=d,i
    return best
def pieza(x0,x1,y0,y1):
    return [(x,y,orig[y*N+x]) for y in range(y0,y1) for x in range(x0,x1) if lum(cols[orig[y*N+x]])>70]
L=pieza(8,32,84,113); R=pieza(203,226,78,107)
def rellenar(cx,cy,r):
    mask=set((x,y) for y in range(int(cy-r),int(cy+r)+1) for x in range(int(cx-r),int(cx+r)+1) if 0<=x<N and 0<=y<N and math.hypot(x-cx,y-cy)<=r)
    val={}
    for (x,y) in mask: val[(x,y)]=list(cols[orig[y*N+x]])
    # inicio: promedio del anillo
    def c0(x,y): return list(cols[orig[y*N+x]])
    for it in range(1500):
        nv={}
        for (x,y) in mask:
            acc=[0,0,0];n=0
            for dx,dy in ((1,0),(-1,0),(0,1),(0,-1)):
                q=(x+dx,y+dy)
                if not(0<=q[0]<N and 0<=q[1]<N): continue
                c=val[q] if q in mask else c0(*q)
                acc=[acc[k]+c[k] for k in range(3)];n+=1
            nv[(x,y)]=[a/n for a in acc]
        val=nv
    B=[[0,2],[3,1]]
    for (x,y),c in val.items():
        t=(B[y%2][x%2]-1.5)*3.0  # un poquito de tramado
        ix[y*N+x]=cerca(tuple(max(0,min(255,c[k]+t)) for k in range(3)))
rellenar(20,98,26)
rellenar(214,92,26)
def poner(p, dx, dy, glow, cx, cy, R0=20):
    for y in range(int(cy-R0),int(cy+R0)+1):
        for x in range(int(cx-R0),int(cx+R0)+1):
            if not(0<=x<N and 0<=y<N): continue
            d=math.hypot(x+.5-cx,(y+.5-cy)*1.1)/R0
            if d>=1: continue
            f=(1-d)**1.4*0.55
            c=cols[ix[y*N+x]]
            rgb=tuple(min(255,int(c[k]+(glow[k]-c[k])*f)) for k in range(3))
            ix[y*N+x]=cerca(rgb)
    for (x,y,v) in p: ix[(y+dy)*N+(x+dx)]=v
DXL,DYL,DXR,DYR=[int(v) for v in sys.argv[1:5]] if len(sys.argv)>4 else (10,17,-11,20)
poner(L,DXL,DYL,(0x8F,0xF0,0xF0),20+DXL,99+DYL)
poner(R,DXR,DYR,(0xC8,0x8A,0xF8),214+DXR,93+DYR)
im=Image.new('RGB',(N,N)); im.putdata([cols[v] for v in ix]); im.resize((466,466),Image.NEAREST).save('/tmp/cr/nueva.png')
print('base izq pantalla', 2*(20+DXL), 2*(112+DYL), 'der', 2*(214+DXR), 2*(106+DYR))
if 'grabar' in sys.argv:
    nl=lz_encode(bytes(ix)); assert lz_decode(nl,N*N)==bytes(ix)
    s2=re.sub(r'static const uint8_t CJ_FONDO_LZ\[\d+\] = \{[^}]*\}', lambda m:'static const uint8_t CJ_FONDO_LZ[%d] = {%s}'%(len(nl),','.join(str(b) for b in nl)), s)
    open(SRC,'w').write(s2); print('grabado', len(lz), '->', len(nl))
