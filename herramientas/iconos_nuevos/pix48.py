"""Pinta los dibujos de herramientas/iconos_nuevos como PIXEL ART a 48x48 (el estilo de los animales del mundo):
cada forma de un color plano, una franja de luz arriba-izquierda y una de sombra abajo-derecha, sin degradados ni
suavizado, y un borde oscuro de 1 pixel por fuera."""
import os,sys
os.environ['ICN']='48'
D='/home/claude/src/herramientas/iconos_nuevos'
sys.path.insert(0,D)
import numpy as np
from PIL import Image, ImageDraw, ImageFilter
import lib
S=lib.S; W=lib.W; G=48
DARK=np.array([38,22,36],float)
def hx(c): return np.array(lib.hx(c) if isinstance(c,str) else c,float)
def sat(c,k=1.35):
    m=c.mean(); return np.clip(m+(c-m)*k,0,255)
def down(m, thr=0.5):
    a=np.array(m,float)/255
    return a.reshape(G,S,G,S).mean((1,3))
def sh(m,dx,dy):
    o=np.zeros_like(m); H=m.shape[0]
    ys=slice(max(dy,0),H+min(dy,0)); yd=slice(max(-dy,0),H+min(-dy,0))
    xs=slice(max(dx,0),H+min(dx,0)); xd=slice(max(-dx,0),H+min(-dx,0))
    o[ys,xs]=m[yd,xd]; return o
class PixIc:
    def __init__(s):
        s.col=np.zeros((G,G,3)); s.a=np.zeros((G,G),bool); s.nf=0
    def mask(s,fn):
        m=Image.new('L',(W,W),0); d=ImageDraw.Draw(m); fn(d,lambda *v:[x*S for x in v]); return m
    def _paint(s,m,c,blend=1.0):
        s.col[m]=s.col[m]*(1-blend)+c*blend
    def fill(s,fn,top,bot=None,ang=0,bisel=True):
        mm=s.mask(fn); cov=down(mm); m=cov>0.5
        s.nf+=1
        if not m.any(): return mm
        ys_,xs_=np.where(m)
        if s.nf<=2 and ys_.min()>=33 and xs_.max()-xs_.min()>=26: return mm   # la base (roca, rama, hielo, hoja): fuera
        if ys_.min()>=34 and (str(top).upper() in ('#7ACC5A','#7ADA6A')): return mm   # hojitas y pasto a sus pies: fuera

        t=sat(hx(top)); b=sat(hx(bot)) if bot is not None else t
        base=t*0.55+b*0.45
        s._paint(m,base); s.a|=m
        ys=np.where(m.any(1))[0]
        if bisel and m.sum()>6:
            hl=m & ~sh(m,1,1); hl2=m & ~sh(m,0,1)
            sd=m & ~sh(m,-1,-1)
            s._paint(hl&~sd, np.clip(base*0.6+255*0.4,0,255))
            # mitad de abajo un tono más oscura (banda, sin degradado)
            if len(ys)>7:
                y0,y1=ys[0],ys[-1]; cut=y0+int((y1-y0)*0.62)
                low=m.copy(); low[:cut]=False; low&=~hl
                s._paint(low, base*0.86+b*0.0+ (b*0.14))
            s._paint(sd, base*0.55+DARK*0.45)
        return mm
    def tex(s,fn,col,a=0.25):
        m=(down(s.mask(fn))>0.4)&s.a
        s._paint(m,sat(hx(col)),min(1,a*1.4))
    def light(s,fn,col,a=0.7,blur=0.6):
        cov=down(s.mask(fn))
        c=sat(hx(col))
        if blur<=0.1 and a>=0.9:   # ojos, bocas, detalles finos: nítidos
            m=cov>0.38
            s._paint(m,c); s.a|=m
        else:
            m=(cov>0.55)&s.a
            s._paint(m,c,min(1,a*0.75))
    def done(s,rim=1.0,dark='#2A1F3A'):
        a=s.a; out=np.zeros((G,G,4),np.uint8)
        out[...,:3]=np.clip(s.col,0,255).astype(np.uint8); out[...,3]=np.where(a,255,0)
        # borde por fuera: oscuro del color vecino
        ring=(sh(a,1,0)|sh(a,-1,0)|sh(a,0,1)|sh(a,0,-1))&~a
        nb=np.zeros((G,G,3)); cnt=np.zeros((G,G,1))
        for dx,dy in ((1,0),(-1,0),(0,1),(0,-1)):
            mm=sh(a,dx,dy); nb+=np.roll(np.roll(s.col,dy,0),dx,1)*mm[...,None]; cnt+=mm[...,None]
        nbc=nb/np.maximum(cnt,1)
        rc=nbc*0.15+DARK*0.85
        out[ring,:3]=rc[ring].astype(np.uint8); out[ring,3]=255
        return Image.fromarray(out,'RGBA')
lib.Ic=PixIc
import iconos, iconos_chile, iconos_chile2
for mod in (iconos,iconos_chile,iconos_chile2): mod.Ic=PixIc
JUEGO=dict(iconos.JUEGO)

# (v99) el pingüino: antes era un dibujito chico de 12 px; ahora como los demás
def pinguino():
    from iconos_chile import ell, poly
    i=PixIc()
    i.fill(lambda d,p:ell(d,p,12,8,36,45),'#3A4462','#1E2236')
    i.fill(lambda d,p:ell(d,p,17,17,33,43),'#FFFFFF','#D8DEEC')
    i.fill(lambda d,p:poly(d,p,[(13,22),(6,36),(9,38),(16,30)]),'#2E3652','#1A1E30')
    i.fill(lambda d,p:poly(d,p,[(32,14),(41,16),(32,19)]),'#FFB040','#E07A20',bisel=False)
    i.light(lambda d,p:ell(d,p,25,11,31,17),'#FFFFFF',1,.05)
    i.light(lambda d,p:ell(d,p,27,12,30,16),'#22141E',1,.05)
    i.light(lambda d,p:ell(d,p,27.6,12.4,28.6,13.6),'#FFFFFF',1,.05)
    i.light(lambda d,p:ell(d,p,29,19,33,21.5),'#FF8FA8',.55,.5)
    i.fill(lambda d,p:(ell(d,p,15,42,23,47),ell(d,p,25,42,33,47)),'#FFB040','#E07A20',bisel=False)
    return i.done()
JUEGO['ICO_PINGUINO']=pinguino
