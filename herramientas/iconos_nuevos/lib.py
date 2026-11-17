from PIL import Image, ImageDraw, ImageFilter, ImageChops
import math
import os
N=int(os.environ.get('ICN','48')); S=(8 if N==48 else 16); W=48*S
def hx(c): return tuple(int(c[i:i+2],16) for i in (1,3,5))
def mix(a,b,t): return tuple(int(a[i]*(1-t)+b[i]*t) for i in range(3))
class Ic:
    def __init__(s): s.im=Image.new('RGBA',(W,W),(0,0,0,0))
    def mask(s,fn):
        m=Image.new('L',(W,W),0); d=ImageDraw.Draw(m); fn(d,lambda *v:[x*S for x in v]); return m
    def fill(s,fn,top,bot=None,ang=0,bisel=True):
        m=s.mask(fn); bb=m.getbbox()
        if not bb: return m
        top=hx(top) if isinstance(top,str) else top; bot=top if bot is None else (hx(bot) if isinstance(bot,str) else bot)
        import numpy as np
        x0,y0,x1,y1=bb
        yy,xx=np.mgrid[0:W,0:W]
        t=((yy-y0)/max(1,y1-y0))*(1-ang)+((xx-x0)/max(1,x1-x0))*ang
        t=np.clip(t,0,1)[...,None]
        arr=np.zeros((W,W,4),np.float32); arr[...,:3]=np.array(top)*(1-t)+np.array(bot)*t; arr[...,3]=255
        g=Image.fromarray(arr.astype(np.uint8),'RGBA')
        s.im.paste(g,(0,0),m)
        if bisel:
            k=int(0.9*S*(48/N if False else 1))
            sh=lambda dx,dy:ImageChops.offset(m,dx,dy)
            luz=ImageChops.subtract(m,sh(k,k)); som=ImageChops.subtract(m,sh(-k,-k))
            luz=luz.point(lambda v:int(v*0.55)); som=som.point(lambda v:int(v*0.40))
            s.im.paste(Image.new('RGBA',(W,W),(255,255,255,255)),(0,0),luz)
            s.im.paste(Image.new('RGBA',(W,W),(20,10,40,255)),(0,0),som)
        return m
    def tex(s,fn,col,a=0.25):
        m=s.mask(fn); m=m.point(lambda v:int(v*a)); s.im.paste(Image.new('RGBA',(W,W),hx(col)+(255,)),(0,0),ImageChops.multiply(m,s.im.split()[3]))
    def light(s,fn,col,a=0.7,blur=0.6):
        m=s.mask(fn).filter(ImageFilter.GaussianBlur(blur*S))
        m=m.point(lambda v:int(v*a)); s.im.paste(Image.new('RGBA',(W,W),hx(col)+(255,)),(0,0),ImageChops.multiply(m,s.im.split()[3]))
    def done(s,rim=1.0,dark='#2A1F3A'):
        a=s.im.split()[3].point(lambda v:255 if v>100 else 0)
        big=a.filter(ImageFilter.MaxFilter(int(rim*S)*2+1))
        ring=ImageChops.subtract(big,a)
        # rim colour: darkened neighbour colour
        blur=s.im.filter(ImageFilter.GaussianBlur(S*1.2))
        col=Image.new('RGBA',(W,W))
        bp=blur.load(); cp=col.load()
        ys=ring.getbbox()
        if ys:
            rp=ring.load()
            for y in range(ys[1],ys[3]):
                for x in range(ys[0],ys[2]):
                    if rp[x,y]:
                        r,g,b,al=bp[x,y]
                        base=(r,g,b) if al>20 else hx(dark)
                        cp[x,y]=mix(base,hx(dark),0.62)+(255,)
        out=s.im.copy(); out.paste(col,(0,0),ring); return out.resize((N,N),Image.LANCZOS)
def quant(im,n=40):
    a=im.split()[3]; rgb=im.convert('RGB').quantize(n,method=Image.MEDIANCUT,dither=Image.NONE).convert('RGB')
    a=a.point(lambda v:255 if v>110 else 0); o=rgb.convert('RGBA'); o.putalpha(a); return o
