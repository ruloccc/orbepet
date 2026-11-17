from lib import *
def E(d,p,*v): pass
def ell(d,p,x0,y0,x1,y1,**k): d.ellipse(p(x0,y0,x1,y1),fill=255)
def rr(d,p,x0,y0,x1,y1,r): d.rounded_rectangle(p(x0,y0,x1,y1),radius=r*S,fill=255)
def poly(d,p,pts): d.polygon(p(*[c for q in pts for c in q]),fill=255)

def corazon():
    i=Ic()
    def f(d,p):
        ell(d,p,6,7,25,27); ell(d,p,23,7,42,27); poly(d,p,[(7,21),(41,21),(24,43)])
    i.fill(f,'#FF8FA8','#D8284E')
    i.light(lambda d,p:ell(d,p,10,10,19,17),'#FFFFFF',.85,.8)
    i.light(lambda d,p:ell(d,p,32,28,40,34),'#8E1030',.35,1.2)
    return i.done()
def comer():
    i=Ic()
    i.fill(lambda d,p:ell(d,p,8,18,24,34),'#FF7A6B','#C8283A'); i.light(lambda d,p:ell(d,p,11,20,16,24),'#FFFFFF',.8,.6)
    i.fill(lambda d,p:ell(d,p,22,15,39,32),'#FFC24A','#E8801A'); i.light(lambda d,p:ell(d,p,26,17,31,22),'#FFFFFF',.7,.6)
    i.fill(lambda d,p:ell(d,p,15,9,32,26),'#9BE070','#3FA04A'); i.light(lambda d,p:ell(d,p,19,11,24,16),'#FFFFFF',.7,.6)
    def bowl(d,p): d.pieslice(p(4,12,44,46),0,180,fill=255)
    i.fill(bowl,'#C98B52','#7A4A26'); i.fill(lambda d,p:rr(d,p,3,28,45,33,2.5),'#E6B176','#B87A42')
    i.light(lambda d,p:rr(d,p,8,29,30,31,1),'#FFFFFF',.55,.5)
    i.light(lambda d,p:ell(d,p,11,35,15,41),'#FFFFFF',.25,.9)
    return i.done()
def jugar():
    i=Ic(); cols=['#FF5A6E','#FFFFFF','#FFD24A','#FFFFFF','#3E9BFF','#FFFFFF']
    m=i.mask(lambda d,p:ell(d,p,5,5,43,43))
    for k,c in enumerate(cols):
        t=Image.new('L',(W,W),0); d=ImageDraw.Draw(t)
        d.pieslice((5*S,5*S,43*S,43*S),k*60-90,k*60-30,fill=255)
        t=ImageChops.multiply(t,m)
        g=Image.new('RGBA',(W,W),hx(c)+(255,)); i.im.paste(g,(0,0),t)
    i.light(lambda d,p:ell(d,p,24-6,24-6,24+6,24+6),'#FFFFFF',.9,.2)
    i.light(lambda d,p:ell(d,p,10,8,21,17),'#FFFFFF',.9,.8)
    i.light(lambda d,p:d.arc(p(5,5,43,43),20,70,fill=255,width=int(1.2*S)),'#FFFFFF',.6,.3)
    i.light(lambda d,p:d.ellipse(p(14,22,48,56),fill=255),'#2A1F6A',.10,2.0)
    return i.done()
def luz():
    i=Ic()
    for a in range(0,360,45):
        if a in (90,): continue
        x=24+math.cos(math.radians(a))*21; y=18+math.sin(math.radians(a))*21; x2=24+math.cos(math.radians(a))*17; y2=18+math.sin(math.radians(a))*17
        i.fill(lambda d,p,x=x,y=y,x2=x2,y2=y2:d.line(p(x,y,x2,y2),fill=255,width=int(1.8*S)),'#FFE27A','#FFC93C')
    i.fill(lambda d,p:ell(d,p,12,5,36,31),'#FFF3A8','#FFC93C')
    i.fill(lambda d,p:poly(d,p,[(17,26),(31,26),(29,34),(19,34)]),'#FFE27A','#F2B01E')
    i.light(lambda d,p:(d.line(p(19,18,21,24,24,19,27,24,29,18),fill=255,width=int(1.2*S))),'#B8741C',.9,.15)
    i.fill(lambda d,p:rr(d,p,18,33,30,37,1.5),'#B9BDCF','#7C8098'); i.fill(lambda d,p:rr(d,p,19,37,29,41,1.5),'#9DA2B8','#666A82'); i.fill(lambda d,p:ell(d,p,21,40,27,44),'#7C8098','#555A72')
    i.light(lambda d,p:ell(d,p,15,9,21,18),'#FFFFFF',.9,.9)
    return i.done()
def curar():
    i=Ic()
    def rot(fn,ang):
        pass
    L=Image.new('RGBA',(W,W),(0,0,0,0))
    j=Ic()
    j.fill(lambda d,p:rr(d,p,18,14,30,36,3),'#EAF6FF','#9CCBEA')
    j.fill(lambda d,p:rr(d,p,19,24,29,35,2),'#5EE0A0','#25A86A')
    for y in (18,21,24): j.fill(lambda d,p,y=y:d.rectangle(p(18,y,22,y+.8),fill=255),'#5A7A9A')
    j.fill(lambda d,p:rr(d,p,14,11,34,15,2),'#C9D2E8','#7F8AAA')
    j.fill(lambda d,p:rr(d,p,21,3,27,12,2),'#9DA6C4','#5E678A')
    j.fill(lambda d,p:rr(d,p,22.5,35,25.5,45,1),'#E8EEF8','#9AA6C0')
    j.light(lambda d,p:rr(d,p,20,16,22,28,1),'#FFFFFF',.85,.4)
    i.im=j.im.rotate(-40,resample=Image.BICUBIC,center=(W//2,W//2)); return i.done()
def cofre():
    i=Ic()
    i.fill(lambda d,p:rr(d,p,5,22,43,43,3),'#B7773E','#6B3E1E')
    def tapa(d,p): d.pieslice(p(5,7,43,37),180,360,fill=255); d.rectangle(p(5,22,43,26),fill=255)
    i.fill(tapa,'#D99A55','#8A5328')
    for x in (13,32): i.fill(lambda d,p,x=x:d.rectangle(p(x,10,x+3.5,43),fill=255),'#E8D7A0','#B99A3C')
    i.fill(lambda d,p:rr(d,p,5,22,43,26,1),'#E8D7A0','#B99A3C')
    i.tex(lambda d,p:[d.line(p(6,y,42,y+.6),fill=255,width=int(.5*S)) for y in (14,17.5,31,34.5,38,41)],'#3A1E0C',.55)
    i.tex(lambda d,p:[d.line(p(x,y,x+7,y-.8),fill=255,width=int(.4*S)) for x,y in ((15,12),(24,15),(33,12),(7,33),(24,36))],'#F2C98A',.5)
    for x in (13,32):
        for y in (12,24,39): i.fill(lambda d,p,x=x,y=y:ell(d,p,x+.4,y,x+3.1,y+2.7),'#FFF2B8','#B8901E',bisel=False)
    i.fill(lambda d,p:rr(d,p,20,22,28,32,2.5),'#FFE27A','#E0A31C')
    i.light(lambda d,p:ell(d,p,22.5,26,25.5,29),'#6B4A10',.9,.15)
    i.light(lambda d,p:rr(d,p,8,11,22,14,1.5),'#FFFFFF',.55,.7)
    i.light(lambda d,p:rr(d,p,7,29,18,31,1),'#FFFFFF',.25,.6)
    # destello
    i.fill(lambda d,p:poly(d,p,[(40,2),(41.5,6),(46,7.5),(41.5,9),(40,13),(38.5,9),(34,7.5),(38.5,6)]),'#FFFFFF','#FFE98A')
    return i.done()
def libro():
    i=Ic()
    i.fill(lambda d,p:rr(d,p,8,6,40,43,3),'#7B5CD6','#3B2A8A')
    i.fill(lambda d,p:rr(d,p,35,8,41,42,1.5),'#F4ECD2','#C9BC96')
    for y in (9,13,17,21,25,29,33,37,41): i.tex(lambda d,p,y=y:d.line(p(35.5,y,40.5,y),fill=255,width=int(.4*S)),'#8A7A56',.8)
    i.fill(lambda d,p:rr(d,p,8,6,13,43,2),'#5A3FB8','#2A1C68')
    i.tex(lambda d,p:[d.line(p(9,y,12,y),fill=255,width=int(.5*S)) for y in (12,20,28,36)],'#FFD84A',.8)
    i.tex(lambda d,p:[ell(d,p,x,y,x+1.1,y+1.1) for x in range(16,34,4) for y in (9.5,40)],'#FFD84A',.7)
    i.fill(lambda d,p:poly(d,p,[(25,12),(27.2,18),(33.5,18.4),(28.5,22.4),(30.3,28.6),(25,25),(19.7,28.6),(21.5,22.4),(16.5,18.4),(22.8,18)]),'#FFF0A0','#F0B020')
    i.fill(lambda d,p:rr(d,p,15,33,34,35.5,1),'#FFD84A','#D8A018')
    i.fill(lambda d,p:rr(d,p,15,37,30,39,1),'#FFD84A','#D8A018')
    i.light(lambda d,p:rr(d,p,15,8,17,40,1),'#FFFFFF',.4,.5)
    i.fill(lambda d,p:poly(d,p,[(41,2),(42.3,5.5),(46,6.8),(42.3,8),(41,11.5),(39.7,8),(36,6.8),(39.7,5.5)]),'#FFFFFF','#BFE8FF')
    return i.done()
def mapa():
    i=Ic()
    i.fill(lambda d,p:poly(d,p,[(4,10),(17,6),(31,10),(44,6),(44,38),(31,42),(17,38),(4,42)]),'#F5E3B0','#D4B472')
    i.fill(lambda d,p:poly(d,p,[(17,6),(31,10),(31,42),(17,38)]),'#FBEFC6','#E0C688')
    i.light(lambda d,p:(d.line(p(17,6,17,38),fill=255,width=int(.7*S)),d.line(p(31,10,31,42),fill=255,width=int(.7*S))),'#8A6A30',.6,.1)
    i.tex(lambda d,p:[d.line(p(x,y,x+3,y+.8),fill=255,width=int(.35*S)) for x,y in ((6,14),(20,20),(36,16),(8,36),(22,34),(35,38))],'#8A6A30',.5)
    i.fill(lambda d,p:ell(d,p,7,22,14,29),'#6CC070','#3A8A48'); i.fill(lambda d,p:ell(d,p,34,12,41,19),'#6CC070','#3A8A48')
    def ruta(d,p):
        for a,b in [((10,32),(18,26)),((18,26),(26,31)),((26,31),(33,24)),((33,24),(37,28))]:
            [d.ellipse(p(a[0]+(b[0]-a[0])*t-.7,a[1]+(b[1]-a[1])*t-.7,a[0]+(b[0]-a[0])*t+.7,a[1]+(b[1]-a[1])*t+.7),fill=255) for t in (0,.25,.5,.75,1)]
    i.light(ruta,'#B0482A',.95,.1)
    i.fill(lambda d,p:(d.line(p(34,29,42,37),fill=255,width=int(2.4*S)),d.line(p(42,29,34,37),fill=255,width=int(2.4*S))),'#F04A4A','#C02A2A')
    return i.done()
def amigos():
    i=Ic()
    i.fill(lambda d,p:ell(d,p,3,11,28,36),'#FFD76A','#F0A428')
    i.fill(lambda d,p:ell(d,p,20,15,45,40),'#7FC8FF','#3E8EE0')
    i.light(lambda d,p:(ell(d,p,9,21,12,26),ell(d,p,17,21,20,26)),'#4A2A10',.95,.1)
    i.light(lambda d,p:(ell(d,p,27,25,30,30),ell(d,p,35,25,38,30)),'#1A2E6A',.95,.1)
    i.light(lambda d,p:(d.arc(p(9,23,20,32),20,160,fill=255,width=int(1*S))),'#4A2A10',.95,.1)
    i.light(lambda d,p:(d.arc(p(27,27,38,36),20,160,fill=255,width=int(1*S))),'#1A2E6A',.95,.1)
    i.light(lambda d,p:(ell(d,p,6,26,10,29),ell(d,p,19,26,23,29)),'#FF7A8A',.6,.5)
    i.light(lambda d,p:ell(d,p,7,14,14,19),'#FFFFFF',.7,.8)
    i.light(lambda d,p:ell(d,p,25,18,32,23),'#FFFFFF',.6,.8)
    return i.done()
def sol():
    i=Ic()
    for k in range(12):
        a=math.radians(k*30); r0,r1=15.5,22.5
        cx,cy=24+math.cos(a)*19,24+math.sin(a)*19
        def f(d,p,a=a,cx=cx,cy=cy):
            pts=[(cx+math.cos(a)*3.6,cy+math.sin(a)*3.6),(cx+math.cos(a+1.57)*1.7,cy+math.sin(a+1.57)*1.7),(cx-math.cos(a)*3.6,cy-math.sin(a)*3.6),(cx+math.cos(a-1.57)*1.7,cy+math.sin(a-1.57)*1.7)]
            poly(d,p,pts)
        i.fill(f,'#FFB02E','#FF8A1E')
    i.fill(lambda d,p:ell(d,p,10,10,38,38),'#FFF08A','#FFB52E')
    i.light(lambda d,p:ell(d,p,14,13,24,21),'#FFFFFF',.85,1.0)
    return i.done()
def luna():
    i=Ic()
    def f(d,p):
        d.ellipse(p(7,5,43,41),fill=255); d.ellipse(p(17,1,52,34),fill=0)
    i.fill(f,'#FFE9A0','#F0B020')
    i.light(lambda d,p:ell(d,p,10,22,20,34),'#C98A12',.4,1.0)
    i.light(lambda d,p:ell(d,p,12,14,18,22),'#FFFFFF',.7,.8)
    for (x,y,r) in [(33,10,2.2),(40,20,1.5),(30,24,1.2)]:
        i.fill(lambda d,p,x=x,y=y,r=r:poly(d,p,[(x,y-r*2),(x+r*.5,y-r*.5),(x+r*2,y),(x+r*.5,y+r*.5),(x,y+r*2),(x-r*.5,y+r*.5),(x-r*2,y),(x-r*.5,y-r*.5)]),'#FFFFFF','#FFF3B0')
    return i.done()
def fruta():
    i=Ic()
    i.fill(lambda d,p:(ell(d,p,6,14,26,42),ell(d,p,22,14,42,42),d.rectangle(p(14,16,34,38),fill=255)),'#FF7C86','#C41E3C')
    i.fill(lambda d,p:rr(d,p,23,5,26,16,1.4),'#8B5E3C','#5A3A1E')
    i.fill(lambda d,p:poly(d,p,[(26,12),(31,5),(38,6),(36,12),(31,15)]),'#8FE070','#3FA04A')
    i.light(lambda d,p:ell(d,p,10,19,17,29),'#FFFFFF',.8,1.0)
    i.light(lambda d,p:ell(d,p,28,32,38,40),'#7A0E24',.35,1.4)
    return i.done()
def nube():
    i=Ic()
    i.fill(lambda d,p:(ell(d,p,5,22,25,40),ell(d,p,13,10,35,32),ell(d,p,24,18,44,40),d.rectangle(p(14,28,34,40),fill=255)),'#FFFFFF','#B9C6E0')
    i.light(lambda d,p:ell(d,p,16,13,26,20),'#FFFFFF',.9,1.0)
    i.light(lambda d,p:ell(d,p,22,32,40,42),'#7C8CB8',.35,1.6)
    return i.done()
ALL=[('COMER',comer),('JUGAR',jugar),('LUZ',luz),('CURAR',curar),('COFRE',cofre),('LIBRO MÁGICO',libro),('MAPA',mapa),('AMIGOS',amigos),('CORAZÓN',corazon),('FRUTA',fruta),('SOL',sol),('LUNA',luna),('NUBE',nube)]

# ---------- versión tierna: caritas, mejillas y formas regordetas ----------
def cara(i,cx,cy,t=1.0,tipo='feliz',tinta='#3A1E2A',mej='#FF7A9A'):
    ex=4.2*t; ey=cy-0.6*t; r=1.5*t
    if tipo=='dormida':
        for s_ in (-1,1):
            i.light(lambda d,p,s_=s_:d.arc(p(cx+s_*ex-r*1.3,ey-r,cx+s_*ex+r*1.3,ey+r*1.2),20,160,fill=255,width=max(1,int(.55*S*t))),tinta,1,.05)
    else:
        for s_ in (-1,1):
            i.light(lambda d,p,s_=s_:d.ellipse(p(cx+s_*ex-r*.9,ey-r*1.25,cx+s_*ex+r*.9,ey+r*1.25),fill=255),tinta,1,.05)
            i.light(lambda d,p,s_=s_:d.ellipse(p(cx+s_*ex-r*.55,ey-r*.95,cx+s_*ex+r*.05,ey-r*.25),fill=255),'#FFFFFF',1,.05)
    for s_ in (-1,1):
        i.light(lambda d,p,s_=s_:d.ellipse(p(cx+s_*(ex+2.2*t)-1.5*t,cy+1.2*t,cx+s_*(ex+2.2*t)+1.5*t,cy+2.8*t),fill=255),mej,.75,.35)
    i.light(lambda d,p:d.arc(p(cx-1.4*t,cy+.2*t,cx+1.4*t,cy+2.6*t),15,165,fill=255,width=max(1,int(.5*S*t))),tinta,1,.05)

def t_corazon():
    i=Ic()
    def f(d,p): ell(d,p,4,6,26,30); ell(d,p,22,6,44,30); poly(d,p,[(5.5,21),(42.5,21),(24,44.5)])
    i.fill(f,'#FFA6BE','#EC5C86')
    i.light(lambda d,p:ell(d,p,9,9,19,17),'#FFFFFF',.9,.8)
    cara(i,24,26,1.0); return i.done()
def t_fruta():
    i=Ic()
    i.fill(lambda d,p:(ell(d,p,4,12,27,42),ell(d,p,21,12,44,42),ell(d,p,10,14,38,42)),'#FF8E94','#E83A56')
    i.fill(lambda d,p:rr(d,p,22.5,4,25.5,15,1.4),'#9A6A42','#6A4426')
    i.fill(lambda d,p:poly(d,p,[(26,12),(31,4),(40,6),(37,13),(31,16)]),'#A6EE7E','#4DB85A')
    i.light(lambda d,p:ell(d,p,9,16,17,25),'#FFFFFF',.85,1.0)
    cara(i,24,30,1.0,mej='#FF4F73'); return i.done()
def t_sol():
    i=Ic()
    for k in range(10):
        a=math.radians(k*36); cx,cy=24+math.cos(a)*19.5,24+math.sin(a)*19.5
        i.fill(lambda d,p,cx=cx,cy=cy:ell(d,p,cx-3.4,cy-3.4,cx+3.4,cy+3.4),'#FFC857','#FF9A2E')
    i.fill(lambda d,p:ell(d,p,7,7,41,41),'#FFF2A0','#FFC03A')
    i.light(lambda d,p:ell(d,p,12,11,23,19),'#FFFFFF',.8,1.0)
    return i.done()
def t_luna():
    i=Ic()
    def f(d,p): d.ellipse(p(6,4,44,42),fill=255); d.ellipse(p(19,-1,54,34),fill=0)
    i.fill(f,'#FFEEA8','#F5BC3A')
    i.light(lambda d,p:ell(d,p,10,16,16,24),'#FFFFFF',.7,.8)
    cara(i,19,28,.95,'dormida',mej='#FF8FA0')
    for (x,y,r) in [(35,8,2.2),(41,19,1.5)]:
        i.fill(lambda d,p,x=x,y=y,r=r:poly(d,p,[(x,y-r*2),(x+r*.5,y-r*.5),(x+r*2,y),(x+r*.5,y+r*.5),(x,y+r*2),(x-r*.5,y+r*.5),(x-r*2,y),(x-r*.5,y-r*.5)]),'#FFFFFF','#FFF3B0')
    return i.done()
def t_nube():
    i=Ic()
    i.fill(lambda d,p:(ell(d,p,3,22,25,43),ell(d,p,11,7,36,33),ell(d,p,23,16,45,43),d.rectangle(p(13,28,35,43),fill=255)),'#FFFFFF','#C4D4F0')
    i.light(lambda d,p:ell(d,p,15,10,26,18),'#FFFFFF',.9,1.0)
    cara(i,24,29,1.15,mej='#FFA0B8'); return i.done()
def t_luz():
    i=Ic()
    for a in (-150,-120,-90,-60,-30,0,180,150):
        x=24+math.cos(math.radians(a))*22; y=18+math.sin(math.radians(a))*21
        i.fill(lambda d,p,x=x,y=y:ell(d,p,x-1.5,y-1.5,x+1.5,y+1.5),'#FFE27A','#FFC93C',bisel=False)
    i.fill(lambda d,p:ell(d,p,8,3,40,35),'#FFF6B8','#FFC93C')
    i.fill(lambda d,p:rr(d,p,16,30,32,36,2),'#FFE27A','#F2B01E')
    i.fill(lambda d,p:rr(d,p,16,35,32,39,1.6),'#C5C9DB','#868AA2'); i.fill(lambda d,p:rr(d,p,18,39,30,43,1.6),'#A9AEC4','#6C7090')
    i.light(lambda d,p:ell(d,p,13,8,22,17),'#FFFFFF',.9,.9)
    cara(i,24,20,1.15,mej='#FFA070'); return i.done()
def t_pelota():
    i=Ic(); cols=['#FF6F86','#FFFFFF','#FFD85A','#FFFFFF','#5AAEFF','#FFFFFF']
    m=i.mask(lambda d,p:ell(d,p,4,4,44,44))
    for k,c in enumerate(cols):
        t=Image.new('L',(W,W),0); d=ImageDraw.Draw(t); d.pieslice((4*S,4*S,44*S,44*S),k*60-90,k*60-30,fill=255)
        i.im.paste(Image.new('RGBA',(W,W),hx(c)+(255,)),(0,0),ImageChops.multiply(t,m))
    i.light(lambda d,p:ell(d,p,10,8,22,17),'#FFFFFF',.9,.9)
    i.light(lambda d,p:ell(d,p,18,18,30,30),'#FFFFFF',.0,.1)
    cara(i,24,26,1.05,mej='#FF7A9A'); return i.done()
def t_comer():
    i=Ic()
    i.fill(lambda d,p:ell(d,p,7,15,25,33),'#FF9096','#E03A56'); i.light(lambda d,p:ell(d,p,10,18,15,22),'#FFFFFF',.85,.6)
    i.fill(lambda d,p:ell(d,p,23,14,41,32),'#FFCB5A','#F08A22'); i.light(lambda d,p:ell(d,p,27,17,32,21),'#FFFFFF',.8,.6)
    i.fill(lambda d,p:ell(d,p,14,6,34,26),'#B2F086','#48B054'); i.light(lambda d,p:ell(d,p,18,9,24,14),'#FFFFFF',.8,.6)
    i.fill(lambda d,p:d.pieslice(p(3,10,45,46),0,180,fill=255),'#E0A164','#9A6234')
    i.fill(lambda d,p:rr(d,p,2,27,46,33,3),'#F2C48A','#C98B52')
    cara(i,24,38,.9,mej='#FF8A8A',tinta='#4A2A14'); return i.done()
def t_curar():
    j=Ic()
    j.fill(lambda d,p:rr(d,p,16,12,32,38,5),'#F2FAFF','#A8D4F0')
    j.fill(lambda d,p:rr(d,p,18,24,30,37,3.5),'#7CF0B4','#2FB872')
    j.fill(lambda d,p:rr(d,p,12,9,36,14,2.5),'#D6DDF0','#8C98BA')
    j.fill(lambda d,p:rr(d,p,20.5,1,27.5,11,3),'#B0B8D4','#6A759A')
    j.fill(lambda d,p:rr(d,p,22.5,37,25.5,46,1.4),'#E8EEF8','#9AA6C0')
    j.light(lambda d,p:rr(d,p,18.5,15,21,29,1.2),'#FFFFFF',.9,.4)
    cara(j,24,29,.8,mej='#FF8FA8',tinta='#1F5A3A')
    i=Ic(); i.im=j.im.rotate(-25,resample=Image.BICUBIC,center=(W//2,W//2)); return i.done()
def t_cofre():
    i=Ic()
    i.fill(lambda d,p:rr(d,p,4,22,44,44,5),'#C98448','#7A4624')
    def tapa(d,p): d.pieslice(p(4,5,44,39),180,360,fill=255); d.rectangle(p(4,22,44,26),fill=255)
    i.fill(tapa,'#E6A862','#9A5E2E')
    for x in (12,32.5): i.fill(lambda d,p,x=x:d.rectangle(p(x,8,x+3.5,44),fill=255),'#F4E3A8','#C8A444')
    i.fill(lambda d,p:rr(d,p,4,22,44,26.5,1.2),'#F4E3A8','#C8A444')
    i.tex(lambda d,p:[d.line(p(6,y,42,y+.6),fill=255,width=int(.45*S)) for y in (14,17.5,31,34.5,39,42)],'#3A1E0C',.4)
    i.fill(lambda d,p:ell(d,p,19,20,29,32),'#FFE98A','#E8AA24')
    i.light(lambda d,p:rr(d,p,23,25,25.2,29,.6),'#6B4A10',.9,.1)
    i.light(lambda d,p:rr(d,p,8,10,22,13,1.5),'#FFFFFF',.55,.7)
    cara(i,24,36.5,.85,mej='#FFB08A',tinta='#4A2A14')
    i.fill(lambda d,p:poly(d,p,[(40,1),(41.5,5.5),(46,7),(41.5,8.5),(40,13),(38.5,8.5),(34,7),(38.5,5.5)]),'#FFFFFF','#FFF0A0')
    return i.done()
def t_libro():
    i=Ic()
    i.fill(lambda d,p:rr(d,p,7,5,41,44,5),'#8F72E8','#4A36A8')
    i.fill(lambda d,p:rr(d,p,36,7,42,43,2),'#FBF3DA','#D4C8A0')
    for y in (10,14,18,22,26,30,34,38): i.tex(lambda d,p,y=y:d.line(p(36.5,y,41.5,y),fill=255,width=int(.4*S)),'#8A7A56',.8)
    i.fill(lambda d,p:rr(d,p,7,5,12.5,44,2.5),'#6A4CCC','#34248A')
    i.fill(lambda d,p:ell(d,p,15,9,35,29),'#FFF6B0','#F6BE3A')
    i.light(lambda d,p:ell(d,p,18,11,25,17),'#FFFFFF',.8,.8)
    cara(i,25,20,.85,mej='#FF9A7A',tinta='#5A3A10')
    i.fill(lambda d,p:rr(d,p,15,33,34,35.5,1),'#FFD84A','#D8A018'); i.fill(lambda d,p:rr(d,p,15,37.5,29,39.8,1),'#FFD84A','#D8A018')
    i.fill(lambda d,p:poly(d,p,[(41,1),(42.3,5),(46,6.4),(42.3,7.8),(41,12),(39.7,7.8),(36,6.4),(39.7,5)]),'#FFFFFF','#BFE8FF')
    return i.done()
def t_mapa():
    i=Ic()
    i.fill(lambda d,p:poly(d,p,[(3,10),(17,5),(31,10),(45,5),(45,39),(31,44),(17,39),(3,44)]),'#FBEBB8','#DDB874')
    i.fill(lambda d,p:poly(d,p,[(17,5),(31,10),(31,44),(17,39)]),'#FFF4D2','#E8CE90')
    i.fill(lambda d,p:ell(d,p,6,22,13,29),'#7CD580','#3FA050'); i.fill(lambda d,p:ell(d,p,35,10,42,17),'#7CD580','#3FA050')
    def ruta(d,p):
        for a,b in [((10,33),(18,27)),((18,27),(26,32)),((26,32),(33,25)),((33,25),(37,29))]:
            [d.ellipse(p(a[0]+(b[0]-a[0])*t-.8,a[1]+(b[1]-a[1])*t-.8,a[0]+(b[0]-a[0])*t+.8,a[1]+(b[1]-a[1])*t+.8),fill=255) for t in (0,.25,.5,.75,1)]
    i.light(ruta,'#C0502E',.95,.1)
    i.fill(lambda d,p:(d.line(p(34,30,42,38),fill=255,width=int(2.6*S)),d.line(p(42,30,34,38),fill=255,width=int(2.6*S))),'#FF6A6A','#D03A3A')
    cara(i,24,18,.8,mej='#FF9A8A',tinta='#5A3A14'); return i.done()
def t_amigos():
    i=Ic()
    i.fill(lambda d,p:ell(d,p,2,10,29,37),'#FFE07A','#F6AE30'); i.fill(lambda d,p:ell(d,p,19,13,46,40),'#8AD0FF','#4A98EA')
    i.light(lambda d,p:ell(d,p,6,13,14,19),'#FFFFFF',.85,.8); i.light(lambda d,p:ell(d,p,24,16,32,22),'#FFFFFF',.75,.8)
    cara(i,14,25,.95,mej='#FF7A8A',tinta='#4A2A10'); cara(i,32,28,.95,mej='#FF8FB0',tinta='#1A2E6A')
    i.fill(lambda d,p:poly(d,p,[(24,2),(26.8,6),(31,6.4),(27.8,9.6),(28.6,14),(24,11.8),(19.4,14),(20.2,9.6),(17,6.4),(21.2,6)]),'#FFB0C8','#EE5C8A') if False else None
    return i.done()
TIERNOS=[('COMER',t_comer),('JUGAR',t_pelota),('LUZ',t_luz),('CURAR',t_curar),('COFRE',t_cofre),('LIBRO MÁGICO',t_libro),('MAPA',t_mapa),('AMIGOS',t_amigos),('CORAZÓN',t_corazon),('FRUTA',t_fruta),('SOL',t_sol),('LUNA',t_luna),('NUBE',t_nube)]

# ---------- v3: caritas solo en personajes; objetos redondos, con corazoncitos y brillos ----------
def heart_at(i,cx,cy,r,top='#FF9CB8',bot='#E8446E',hl=True):
    i.fill(lambda d,p:(ell(d,p,cx-r,cy-r*.9,cx,cy+r*.3),ell(d,p,cx,cy-r*.9,cx+r,cy+r*.3),poly(d,p,[(cx-r*.97,cy-r*.05),(cx+r*.97,cy-r*.05),(cx,cy+r*1.05)])),top,bot,bisel=False)
    if hl: i.light(lambda d,p:ell(d,p,cx-r*.7,cy-r*.7,cx-r*.2,cy-r*.3),'#FFFFFF',.9,.3)
def v_comer():
    i=Ic()
    i.fill(lambda d,p:ell(d,p,6,14,25,34),'#FF9AA0','#E8405C'); i.light(lambda d,p:ell(d,p,9,17,15,22),'#FFFFFF',.9,.6)
    i.fill(lambda d,p:ell(d,p,23,13,42,33),'#FFD066','#F59A2A'); i.light(lambda d,p:ell(d,p,27,16,33,21),'#FFFFFF',.85,.6)
    i.fill(lambda d,p:ell(d,p,14,4,34,25),'#BDF592','#4FBC5A'); i.light(lambda d,p:ell(d,p,18,7,24,13),'#FFFFFF',.85,.6)
    i.fill(lambda d,p:poly(d,p,[(24,5),(27,2),(31,3),(28,7)]),'#7FD86A','#3FA04A',bisel=False)
    i.fill(lambda d,p:d.pieslice(p(2,9,46,47),0,180,fill=255),'#F0B070','#B87038')
    i.fill(lambda d,p:rr(d,p,1,26,47,33,3.5),'#FFD49A','#D8984E')
    i.tex(lambda d,p:[ell(d,p,x,36.5,x+2,38.5) for x in (12,22,32)],'#FFF0D8',.7)
    i.light(lambda d,p:ell(d,p,9,28,19,30.5),'#FFFFFF',.6,.5)
    return i.done()
def v_pelota():
    i=Ic(); cols=['#FF8CA2','#FFFFFF','#FFE070','#FFFFFF','#7CC2FF','#FFFFFF']
    m=i.mask(lambda d,p:ell(d,p,4,4,44,44))
    for k,c in enumerate(cols):
        t=Image.new('L',(W,W),0); d=ImageDraw.Draw(t); d.pieslice((4*S,4*S,44*S,44*S),k*60-90,k*60-30,fill=255)
        i.im.paste(Image.new('RGBA',(W,W),hx(c)+(255,)),(0,0),ImageChops.multiply(t,m))
    i.fill(lambda d,p:ell(d,p,19,19,29,29),'#FFFFFF','#FFD0DC',bisel=False)
    heart_at(i,24,24,3.6,hl=False)
    i.light(lambda d,p:ell(d,p,9,8,22,17),'#FFFFFF',.95,.9)
    i.light(lambda d,p:d.ellipse(p(14,24,54,60),fill=255),'#5A4AA0',.14,2.4)
    return i.done()
def v_curar():
    j=Ic()
    j.fill(lambda d,p:rr(d,p,15,11,33,39,7),'#F6FCFF','#B0D8F4')
    j.fill(lambda d,p:rr(d,p,17,25,31,38,5),'#8CF4C0','#34BC7A')
    heart_at(j,24,31,3.4,'#FFFFFF','#FFD0DC',hl=False)
    j.fill(lambda d,p:rr(d,p,11,8,37,14,3),'#E0E6F8','#929EC4')
    j.fill(lambda d,p:rr(d,p,19.5,0,28.5,10.5,4),'#BCC4E0','#7480A6')
    j.fill(lambda d,p:rr(d,p,22.3,38,25.7,46.5,1.6),'#F0F4FC','#A6B2CC')
    for y in (17,20.5): j.tex(lambda d,p,y=y:d.rectangle(p(15.5,y,19,y+.9),fill=255),'#5A7A9A',.7)
    j.light(lambda d,p:rr(d,p,17.5,14,20.5,26,1.6),'#FFFFFF',.95,.4)
    i=Ic(); i.im=j.im.rotate(-28,resample=Image.BICUBIC,center=(W//2,W//2)); return i.done()
def v_cofre():
    i=Ic()
    i.fill(lambda d,p:rr(d,p,4,22,44,44,7),'#D08A4E','#80482A')
    def tapa(d,p): d.pieslice(p(4,4,44,40),180,360,fill=255); d.rectangle(p(4,22,44,26),fill=255)
    i.fill(tapa,'#EEB06A','#A2642E')
    for x in (12,32.5): i.fill(lambda d,p,x=x:d.rectangle(p(x,7,x+3.6,44),fill=255),'#FFEDB0','#D4AE4C')
    i.fill(lambda d,p:rr(d,p,4,22,44,26.8,1.4),'#FFEDB0','#D4AE4C')
    i.tex(lambda d,p:[d.line(p(6,y,42,y+.6),fill=255,width=int(.45*S)) for y in (13,16.5,31,35,39.5,42)],'#3A1E0C',.35)
    for x in (13.8,34.3):
        for y in (10,24,40): i.fill(lambda d,p,x=x,y=y:ell(d,p,x-1.1,y-1.1,x+1.1,y+1.1),'#FFFBE0','#C8A030',bisel=False)
    i.fill(lambda d,p:ell(d,p,18.5,20.5,29.5,33),'#FFEE9A','#EBB02A')
    heart_at(i,24,26.6,2.6,'#FF9CB8','#D8345E',hl=False)
    i.light(lambda d,p:rr(d,p,8,9,21,12.5,1.7),'#FFFFFF',.65,.7)
    for (x,y,r) in [(40,2,1),(6,5,.7)]:
        i.fill(lambda d,p,x=x,y=y,r=r:poly(d,p,[(x,y-r*5),(x+r*1.3,y-r*1.3),(x+r*5,y),(x+r*1.3,y+r*1.3),(x,y+r*5),(x-r*1.3,y+r*1.3),(x-r*5,y),(x-r*1.3,y-r*1.3)]),'#FFFFFF','#FFF0A0',bisel=False)
    return i.done()
def v_libro():
    i=Ic()
    i.fill(lambda d,p:rr(d,p,6,5,42,44,7),'#9A7EF0','#5640B8')
    i.fill(lambda d,p:rr(d,p,37,8,43,42,2.5),'#FFF6E0','#DCCFA8')
    for y in (11,15,19,23,27,31,35,39): i.tex(lambda d,p,y=y:d.line(p(37.5,y,42.5,y),fill=255,width=int(.4*S)),'#9A8A66',.8)
    i.fill(lambda d,p:rr(d,p,6,5,12.5,44,3),'#7A5CD8','#3C2A98')
    i.tex(lambda d,p:[ell(d,p,8.6,y,10,y+1.4) for y in (11,19,27,35)],'#FFE27A',.9)
    i.fill(lambda d,p:ell(d,p,14,8,36,30),'#FFF8C0','#FFCB44')
    i.fill(lambda d,p:poly(d,p,[(25,11),(27.2,17),(33.4,17.5),(28.6,21.6),(30.2,27.6),(25,24.2),(19.8,27.6),(21.4,21.6),(16.6,17.5),(22.8,17)]),'#FFE9A0','#F5A524',bisel=False)
    i.light(lambda d,p:ell(d,p,17,10,24,15),'#FFFFFF',.8,.8)
    heart_at(i,24,36,2.8,'#FFD36A','#E89A1E',hl=False)
    i.fill(lambda d,p:poly(d,p,[(31,38),(36,38),(36,47),(33.5,44.5),(31,47)]),'#FF8CA8','#D8345E',bisel=False)
    i.fill(lambda d,p:poly(d,p,[(41,0),(42.4,4.6),(47,6),(42.4,7.4),(41,12),(39.6,7.4),(35,6),(39.6,4.6)]),'#FFFFFF','#C4EAFF',bisel=False)
    return i.done()
def v_mapa():
    i=Ic()
    i.fill(lambda d,p:poly(d,p,[(3,11),(17,6),(31,11),(45,6),(45,39),(31,44),(17,39),(3,44)]),'#FFEFC4','#E8C27C')
    i.fill(lambda d,p:poly(d,p,[(17,6),(31,11),(31,44),(17,39)]),'#FFF8DC','#F0D698')
    i.tex(lambda d,p:[d.line(p(x,y,x+3,y+.8),fill=255,width=int(.35*S)) for x,y in ((6,14),(20,20),(37,16),(8,36),(22,34),(35,40))],'#8A6A30',.4)
    i.fill(lambda d,p:ell(d,p,5,21,13.5,30),'#8EE08C','#44AE5C'); i.fill(lambda d,p:ell(d,p,34,11,42,19),'#8EE08C','#44AE5C'); i.fill(lambda d,p:ell(d,p,20,13,26,19),'#9AD8FF','#4A9AE8')
    def ruta(d,p):
        for a,b in [((10,34),(18,28)),((18,28),(26,33)),((26,33),(33,27))]:
            [d.ellipse(p(a[0]+(b[0]-a[0])*t-.8,a[1]+(b[1]-a[1])*t-.8,a[0]+(b[0]-a[0])*t+.8,a[1]+(b[1]-a[1])*t+.8),fill=255) for t in (0,.25,.5,.75,1)]
    i.light(ruta,'#D05A7A',.95,.1)
    heart_at(i,37,29,4.2,'#FF7C9C','#D8305A')
    return i.done()
VERSION3=[('COMER',v_comer),('JUGAR',v_pelota),('LUZ',t_luz),('CURAR',v_curar),('COFRE',v_cofre),('LIBRO MÁGICO',v_libro),('MAPA',v_mapa),('AMIGOS',t_amigos),('CORAZÓN',t_corazon),('FRUTA',t_fruta),('SOL',t_sol),('LUNA',t_luna),('NUBE',t_nube)]

def v_corazon():
    i=Ic()
    def f(d,p): ell(d,p,4,6,26,30); ell(d,p,22,6,44,30); poly(d,p,[(5.5,21),(42.5,21),(24,44.5)])
    i.fill(f,'#FFA6BE','#EC5C86')
    i.light(lambda d,p:ell(d,p,9,9,19,17),'#FFFFFF',.9,.8)
    i.light(lambda d,p:ell(d,p,31,26,40,33),'#9A1038',.25,1.2)
    return i.done()

def v_banco():
    i=Ic()
    rr=lambda d,p,x0,y0,x1,y1,r=1.6:d.rounded_rectangle(p(x0,y0,x1,y1),radius=int(r*S),fill=255)
    # patas y postes del respaldo
    i.fill(lambda d,p:(rr(d,p,6,34,12,45,1.2),rr(d,p,36,34,42,45,1.2)),'#A8704A','#6E4529')
    i.fill(lambda d,p:(rr(d,p,7,8,12,32,1.4),rr(d,p,36,8,41,32,1.4)),'#B9804F','#7A4C2E')
    # tablas del respaldo
    i.fill(lambda d,p:rr(d,p,4,10,44,17.5,2.2),'#F0BC80','#CE9358')
    i.fill(lambda d,p:rr(d,p,4,19.5,44,27,2.2),'#EBB475','#C48A50')
    # el asiento
    i.fill(lambda d,p:rr(d,p,2,28.5,46,37,2.6),'#F6C58A','#D49A5E')
    i.light(lambda d,p:ell(d,p,8,11,24,14.5),'#FFFFFF',.55,.8)
    i.light(lambda d,p:ell(d,p,8,30,26,33),'#FFFFFF',.5,.8)
    i.tex(lambda d,p:[d.line(p(x,y,x+5,y+.6),fill=255,width=int(.3*S)) for x,y in ((10,13),(26,15),(18,22),(30,24),(12,32),(30,34))],'#8A5A30',.35)
    # una hojita y una florcita sobre el asiento
    i.fill(lambda d,p:ell(d,p,34,24,41,29.5),'#8EE08C','#44AE5C')
    i.fill(lambda d,p:ell(d,p,38,22,43,26),'#FF9FBC','#E0507A')
    return i.done()
def v_nimba():
    """Nimba: beluga de nubes mirando a la derecha: cuerpo alargado con cola de dos lóbulos, frente abultada, aleta redonda, panza de nubes con tinte de atardecer, estrellitas y chorrito."""
    i=Ic()
    star=lambda cx,cy,r:(lambda d,p:poly(d,p,[(cx,cy-r),(cx+r*.3,cy-r*.3),(cx+r,cy),(cx+r*.3,cy+r*.3),(cx,cy+r),(cx-r*.3,cy+r*.3),(cx-r,cy),(cx-r*.3,cy-r*.3)]))
    # chorrito de agua/nubecitas desde el espiráculo
    for (cx,cy,r,c0,c1) in ((29,8,2.2,'#FFFFFF','#BCD8FF'),(33,5.5,1.6,'#FFFFFF','#BCD8FF'),(26,5,1.3,'#FFFFFF','#CFE4FF')):
        i.fill(lambda d,p,cx=cx,cy=cy,r=r:ell(d,p,cx-r,cy-r,cx+r,cy+r),c0,c1,bisel=False)
    i.fill(lambda d,p:poly(d,p,[(29,10),(31,10),(30.4,14),(29.6,14)]),'#E4F0FF','#9CC0F0',bisel=False)
    # cola: pedúnculo y dos lóbulos de aleta (con tinte rosado en las puntas)
    i.fill(lambda d,p:(poly(d,p,[(15,30),(7,24),(6,29),(17,35)]),ell(d,p,1,12,10,23),ell(d,p,7,8,17,18),poly(d,p,[(6,20),(11,15),(15,27),(9,30)])),'#F6F9FF','#B8C8F0')
    i.light(lambda d,p:(ell(d,p,1.5,13,5.5,19),ell(d,p,8,9,12,13)),'#FFB0D0',.55,.8)
    i.tex(lambda d,p:[d.arc(p(3,14,9,21),200,340,fill=255,width=int(.35*S)),d.arc(p(9,10,15,16),200,340,fill=255,width=int(.35*S))],'#7E92C8',.55)
    # cuerpo alargado + melón de la frente (bulto redondo, típico de la beluga)
    i.fill(lambda d,p:(ell(d,p,9,24,44,38),ell(d,p,26,15,46,33),ell(d,p,14,25,30,38)),'#FFFFFF','#B4C6EE')
    # panza con tinte lavanda/rosa de atardecer
    i.light(lambda d,p:ell(d,p,12,31,40,40),'#C9B4F0',.55,1.4)
    i.light(lambda d,p:ell(d,p,18,33,34,40),'#FFC0DC',.45,1.2)
    # nubes del lomo y de la panza, con sus remolinitos
    i.fill(lambda d,p:(ell(d,p,14,18,25,26),ell(d,p,21,14,30,23),ell(d,p,9,22,17,28)),'#FFFFFF','#D4E2F8')
    i.fill(lambda d,p:(ell(d,p,10,33,20,41),ell(d,p,18,34,30,42),ell(d,p,28,33,38,40)),'#F8FBFF','#B0C4EC')
    i.tex(lambda d,p:[d.arc(p(15,19,23,25),190,350,fill=255,width=int(.4*S)),d.arc(p(22,15,29,21),190,350,fill=255,width=int(.4*S)),d.arc(p(12,35,20,42),20,170,fill=255,width=int(.4*S)),d.arc(p(21,37,30,44),20,170,fill=255,width=int(.4*S)),d.arc(p(30,35,39,42),20,170,fill=255,width=int(.4*S))],'#7E92C8',.5)
    # línea de la boca de la frente a la barbilla y el pliegue del cuello
    i.tex(lambda d,p:d.arc(p(29,20,44,40),100,170,fill=255,width=int(.35*S)),'#7E92C8',.5)
    # aleta lateral redondita con deditos
    i.fill(lambda d,p:ell(d,p,22,29,30,38),'#EEF3FF','#A4B8E6')
    i.tex(lambda d,p:[d.line(p(24,31,24.6,38),fill=255,width=int(.3*S)),d.line(p(27,31,27,38),fill=255,width=int(.3*S))],'#7E92C8',.5)
    # brillos
    i.light(lambda d,p:ell(d,p,31,18,40,24),'#FFFFFF',.95,.9)
    i.light(lambda d,p:ell(d,p,15,20,22,24),'#FFFFFF',.9,.8)
    # mejilla rosada
    i.light(lambda d,p:ell(d,p,33.5,27.2,40.5,32),'#FF8FB4',.9,.6)
    # ojo grande brillante con pestañas
    i.light(lambda d,p:d.ellipse(p(35.8,21.2,40,26.2),fill=255),'#2E1A3A',1,.05)
    i.light(lambda d,p:d.ellipse(p(36.6,21.7,38.4,23.5),fill=255),'#FFFFFF',1,.05)
    i.light(lambda d,p:d.ellipse(p(38.2,24,39.2,25),fill=255),'#9FD4FF',.9,.05)
    i.light(lambda d,p:(d.line(p(39.6,21.8,41.2,20.6),fill=255,width=int(.35*S)),d.line(p(39.9,22.8,41.6,22.2),fill=255,width=int(.35*S))),'#2E1A3A',1,.05)
    # sonrisa y naricita
    i.light(lambda d,p:d.arc(p(35.5,27.5,45,34),20,150,fill=255,width=max(1,int(.5*S))),'#3A1E2A',1,.05)
    i.light(lambda d,p:d.ellipse(p(44.2,26.6,45.2,27.6),fill=255),'#8A9CD0',.8,.05)
    # estrellitas: una en la frente y varias alrededor
    i.fill(star(33,17.5,1.7),'#FFF6B0','#FFD04A',bisel=False)
    i.fill(star(41,6,3.2),'#FFF2A0','#FFC83A',bisel=False)
    i.fill(star(6,36,2.2),'#FFF2A0','#FFC83A',bisel=False)
    i.fill(star(44,40,1.8),'#E6F2FF','#9CC8FF',bisel=False)
    i.light(lambda d,p:(ell(d,p,3,3,6,6),ell(d,p,44,14,46,16)),'#FFFFFF',.9,.3)
    return i.done()
def v_paraguas():
    """Paraguas de hoja abierto, verde tierno con festón, mango curvo y gotitas."""
    i=Ic()
    # palo y mango en gancho
    i.fill(lambda d,p:(rr(d,p,23,18,25.4,40,1),d.arc(p(19,36,25.4,45),0,180,fill=255,width=int(1.6*S))),'#C58C5A','#8A5A32',bisel=True)
    # copa abierta con festón
    i.fill(lambda d,p:(d.pieslice(p(3,6,45,38),180,360,fill=255),ell(d,p,3,19,13.5,25),ell(d,p,12.5,19,23,25),ell(d,p,23,19,33.5,25),ell(d,p,32.5,19,45,25),d.rectangle(p(6,19,42,22),fill=255)),'#B6F0A0','#4DB65C')
    # gajos
    i.tex(lambda d,p:[d.line(p(24,6,x,22),fill=255,width=int(.55*S)) for x in (8,16,24,32,40)],'#2E8A4A',.55)
    i.light(lambda d,p:ell(d,p,9,9,20,14),'#FFFFFF',.9,.8)
    # puntita de arriba y cuenta de adorno
    i.fill(lambda d,p:ell(d,p,22.3,3,25.7,8),'#FFE27A','#FFB830',bisel=False)
    # gotitas de lluvia
    for (x,y) in ((8,31),(40,29),(34,39)):
        i.fill(lambda d,p,x=x,y=y:(poly(d,p,[(x,y-3),(x+1.5,y),(x-1.5,y)]),ell(d,p,x-1.5,y-0.8,x+1.5,y+2)),'#CFF0FF','#69B8F0',bisel=False)
    # florcita en el festón
    i.fill(lambda d,p:ell(d,p,31,12,36,17),'#FFB6D2','#F06A9E'); i.fill(lambda d,p:ell(d,p,32.6,13.6,34.4,15.4),'#FFE066','#FFC83A',bisel=False)
    return i.done()
def v_molinillo():
    """Molinillo de viento: cuatro aspas de colores, alfiler brillante, palito y estrellita."""
    i=Ic()
    # palito
    i.fill(lambda d,p:rr(d,p,22.4,22,25.6,46,1.2),'#D9A877','#A8703E')
    cx,cy=24,19
    cols=[('#FF9CC2','#E8447E'),('#FFE58A','#F5B523'),('#9FE0FF','#3BA4E8'),('#FFFFFF','#B9C6EE')]
    # cuatro aspas (triángulos curvos) en cruz
    dirs=[(0,-1),(1,0),(0,1),(-1,0)]
    for (dx,dy),(a,b) in zip(dirs,cols):
        px,py=-dy,dx
        pts=[(cx,cy),(cx+dx*15,cy+dy*15),(cx+dx*15+px*14,cy+dy*15+py*14)]
        i.fill(lambda d,p,pts=pts:poly(d,p,pts),a,b)
    # brillos
    i.light(lambda d,p:ell(d,p,24,8,30,13),'#FFFFFF',.7,.6)
    # alfiler central
    i.fill(lambda d,p:ell(d,p,21,16,27,22),'#FF5A78','#C81E44'); i.light(lambda d,p:ell(d,p,22,16.6,24,18.6),'#FFFFFF',.95,.3)
    # estrellitas
    star=lambda cx_,cy_,r:(lambda d,p:poly(d,p,[(cx_,cy_-r),(cx_+r*.3,cy_-r*.3),(cx_+r,cy_),(cx_+r*.3,cy_+r*.3),(cx_,cy_+r),(cx_-r*.3,cy_+r*.3),(cx_-r,cy_),(cx_-r*.3,cy_-r*.3)]))
    i.fill(star(41,6,3),'#FFF2A0','#FFC83A',bisel=False); i.fill(star(6,40,2.2),'#E6F2FF','#9CC8FF',bisel=False)
    return i.done()
JUEGO=[('ICO_PLATO',v_comer),('ICO_PELOTA',v_pelota),('ICO_BOMBILLA',t_luz),('ICO_JERINGA',v_curar),('ICO_COFRE',v_cofre),('ICO_LIBRO_MAGICO',v_libro),('ICO_MAPA',v_mapa),('ICO_AMIGOS',t_amigos),('ICO_CORAZON',v_corazon),('ICO_FRUTA',t_fruta),('ICO_SOL',t_sol),('ICO_BANCO',v_banco),('ICO_LUNA',t_luna),('ICO_NUBE',t_nube),('ICO_NIMBA',v_nimba),('ICO_PARAGUAS',v_paraguas),('ICO_MOLINILLO',v_molinillo)]

# ---------------- versión 37: los personajes y cosas de la historia ----------------
def _star(cx,cy,r): return lambda d,p:poly(d,p,[(cx,cy-r),(cx+r*.3,cy-r*.3),(cx+r,cy),(cx+r*.3,cy+r*.3),(cx,cy+r),(cx-r*.3,cy+r*.3),(cx-r,cy),(cx-r*.3,cy-r*.3)])
def _ojo(i,x,y,r=2.0):
    i.light(lambda d,p:d.ellipse(p(x-r,y-r*1.15,x+r,y+r*1.15),fill=255),'#2A1A30',1,.05)
    i.light(lambda d,p:d.ellipse(p(x-r*.55,y-r*.85,x+r*.1,y-r*.15),fill=255),'#FFFFFF',1,.05)
def _urraca(alas_arriba):
    """Tilde: urraca blanca y negra, cola larga azul tornasol, pañuelito rojo y ojito pícaro."""
    i=Ic()
    # cola larga
    i.fill(lambda d,p:poly(d,p,[(16,30),(2,44),(6,46),(20,34)]),'#5A7AD8','#2A3A8A')
    i.light(lambda d,p:poly(d,p,[(14,33),(5,42),(7,43)]),'#9FE0FF',.6,.4)
    # patitas
    i.fill(lambda d,p:(d.line(p(24,36,22,44),fill=255,width=int(1.1*S)),d.line(p(29,36,30,44),fill=255,width=int(1.1*S))),'#4A4058','#2A2238',bisel=False)
    # cuerpo: lomo negro, panza blanca
    i.fill(lambda d,p:ell(d,p,13,18,37,38),'#3A3250','#1A1428')
    i.fill(lambda d,p:ell(d,p,21,24,36,38),'#FFFFFF','#D6DCEE')
    # ala
    if alas_arriba:
        i.fill(lambda d,p:poly(d,p,[(18,24),(6,4),(14,6),(26,22)]),'#3A3250','#1A1428')
        i.fill(lambda d,p:poly(d,p,[(12,10),(8,5),(13,6),(18,14)]),'#FFFFFF','#C8D0E8')
        i.light(lambda d,p:poly(d,p,[(16,20),(9,8),(12,8)]),'#7A9AF0',.7,.4)
    else:
        i.fill(lambda d,p:ell(d,p,14,22,30,33),'#2E2844','#141020')
        i.fill(lambda d,p:ell(d,p,17,24,25,29),'#FFFFFF','#C8D0E8')
        i.light(lambda d,p:ell(d,p,15,27,24,32),'#6A8AE8',.6,.6)
    # cabeza
    i.fill(lambda d,p:ell(d,p,26,8,42,24),'#3A3250','#1A1428')
    # pico
    i.fill(lambda d,p:poly(d,p,[(40,14),(47,17),(40,19)]),'#5A5068','#2A2238')
    # pañuelito rojo con puntos
    i.fill(lambda d,p:(poly(d,p,[(25,20),(39,21),(33,28)]),ell(d,p,24,19,40,24)),'#FF6A6A','#C8283A')
    i.light(lambda d,p:(ell(d,p,29,21,31,23),ell(d,p,34,22,36,24)),'#FFFFFF',.9,.1)
    # ojo pícaro con ceja
    _ojo(i,35,14,2.1)
    i.light(lambda d,p:d.line(p(32,10.5,38,11.5),fill=255,width=int(.6*S)),'#FFFFFF',.8,.05)
    i.light(lambda d,p:ell(d,p,29,10,34,14),'#8A9AE8',.5,.6)
    # destello: le encanta lo brillante
    i.fill(_star(44,6,3),'#FFF2A0','#FFC83A',bisel=False)
    return i.done()
def v_tilde(): return _urraca(False)
def v_tilde_vuela(): return _urraca(True)
def _tortuga(modo):
    """La Abuela Brizna: cabezota de tortuga anciana asomando, con musgo, florcita, arrugas y anteojitos. modo: 0 ojos abiertos, 1 dormida, 2 bostezo."""
    i=Ic()
    # cuello que sale del agua
    i.fill(lambda d,p:rr(d,p,14,26,34,48,6),'#8CC07A','#4A7A4A')
    i.tex(lambda d,p:[d.arc(p(15,30+k*5,33,38+k*5),200,340,fill=255,width=int(.5*S)) for k in range(3)],'#3A6A3A',.5)
    # cabeza
    i.fill(lambda d,p:ell(d,p,6,6,42,34),'#A8D890','#5A9A5A')
    # musgo y florcita en la cabeza
    i.fill(lambda d,p:(ell(d,p,12,3,24,11),ell(d,p,20,2,32,10),ell(d,p,28,4,38,11)),'#6AB85A','#2E7A3A')
    i.fill(lambda d,p:ell(d,p,29,0,35,6),'#FFB6D2','#F06A9E'); i.fill(lambda d,p:ell(d,p,31,2,33,4),'#FFE066','#FFC83A',bisel=False)
    # manchitas y arrugas
    i.tex(lambda d,p:[ell(d,p,9,14,13,18),ell(d,p,36,13,40,17)],'#4A8A4A',.5)
    i.tex(lambda d,p:[d.arc(p(10,24,20,30),20,160,fill=255,width=int(.45*S)),d.arc(p(28,24,38,30),20,160,fill=255,width=int(.45*S))],'#3A6A3A',.5)
    # mejillas
    i.light(lambda d,p:(ell(d,p,8,19,15,24),ell(d,p,33,19,40,24)),'#FF9AB4',.7,.6)
    # ojos
    if modo==1:
        for x in (16,32): i.light(lambda d,p,x=x:d.arc(p(x-3.5,14,x+3.5,20),20,160,fill=255,width=int(.7*S)),'#2A1A30',1,.05)
    else:
        for x in (16,32):
            i.fill(lambda d,p,x=x:ell(d,p,x-4,12,x+4,20),'#FFFFFF','#E6EEDC',bisel=False)
            _ojo(i,x,16.5,2.2)
            i.light(lambda d,p,x=x:d.arc(p(x-4.5,10.5,x+4.5,17),200,340,fill=255,width=int(.8*S)),'#4A7A4A',1,.05)
    # anteojitos redondos
    i.light(lambda d,p:(d.ellipse(p(11,11,21,21),outline=255,width=int(.6*S)),d.ellipse(p(27,11,37,21),outline=255,width=int(.6*S)),d.line(p(21,15,27,15),fill=255,width=int(.6*S))),'#C8A040',.9,.05)
    # boca
    if modo==2:
        i.fill(lambda d,p:ell(d,p,18,23,30,32),'#7A2A3A','#4A1424',bisel=False)
        i.light(lambda d,p:ell(d,p,21,27,27,31),'#FF8FA8',.8,.3)
    else:
        i.light(lambda d,p:d.arc(p(19,21,29,28),20,160,fill=255,width=int(.6*S)),'#2A1A30',1,.05)
    i.light(lambda d,p:ell(d,p,12,8,22,13),'#FFFFFF',.55,.8)
    if modo==1: i.fill(_star(42,8,2.2),'#E6F2FF','#9CC8FF',bisel=False)
    return i.done()
def v_brizna(): return _tortuga(0)
def v_brizna_duerme(): return _tortuga(1)
def v_brizna_bosteza(): return _tortuga(2)
def v_tilo():
    """(v39) Tilo, el Cuidador de verdad: el mismo de los recuerdos (capucha verde de hojas, barba blanca, cara de abuelito),
    pero chiquito y redondito, del tamaño de ella. En vez del bastón, su cuchara de palo."""
    i=Ic()
    # la cuchara de palo (en la mano, detrás)
    i.fill(lambda d,p:(d.line(p(40,45,38,18),fill=255,width=int(2.2*S)),ell(d,p,34,7,43,19)),'#D8A468','#9A6A38')
    i.light(lambda d,p:ell(d,p,36,9,39,13),'#FFFFFF',.6,.4)
    # la capa verde, redondita (el cuerpo)
    i.fill(lambda d,p:(ell(d,p,7,20,39,47),d.rectangle(p(9,33,37,45),fill=255)),'#5EC46A','#2E7A44')
    i.tex(lambda d,p:(d.line(p(23,30,23,46),fill=255,width=int(1.0*S))),'#1E5A34',.35)
    # piecitos
    i.fill(lambda d,p:(ell(d,p,12,42,20,47),ell(d,p,26,42,34,47)),'#8A5A34','#5A3A22')
    # la capucha de hojas
    i.fill(lambda d,p:ell(d,p,8,6,38,34),'#7AD27A','#3E9A54')
    for (x,y) in ((10,10),(16,5),(23,3),(30,5),(36,10)):
        i.fill(lambda d,p,x=x,y=y:poly(d,p,[(x,y-4),(x+4,y+2),(x,y+5),(x-4,y+2)]),'#A8EC8A','#4EAA5A')
    i.fill(_star(23,1.5,2),'#FFF2A0','#FFC83A',bisel=False)
    # la cara de abuelito
    i.fill(lambda d,p:ell(d,p,13,12,33,32),'#FBE2C8','#E8B898')
    # cejas blancas, tupidas
    i.fill(lambda d,p:(ell(d,p,14.5,15.5,21,18.5),ell(d,p,25,15.5,31.5,18.5)),'#FFFFFF','#DDE2EE',bisel=False)
    # ojitos cerrados de risa
    i.fill(lambda d,p:(d.arc(p(16,18,21,22),200,340,fill=255,width=int(1.1*S)),d.arc(p(25,18,30,22),200,340,fill=255,width=int(1.1*S))),'#3A1E2A',bisel=False)
    # mejillas
    i.light(lambda d,p:(ell(d,p,14,21,19,25),ell(d,p,27,21,32,25)),'#FF8A9A',.55,.6)
    # la barba blanca, larga y esponjosa (le tapa la pancita)
    i.fill(lambda d,p:(ell(d,p,14,22,32,40),ell(d,p,12,24,22,36),ell(d,p,24,24,34,36),poly(d,p,[(17,36),(29,36),(23,44)])),'#FFFFFF','#D6DCEC')
    i.tex(lambda d,p:(d.line(p(20,30,21,39),fill=255,width=int(.8*S)),d.line(p(26,30,25,39),fill=255,width=int(.8*S))),'#B8C0D8',.5)
    # la nariz, chiquita
    i.fill(lambda d,p:ell(d,p,21,21,25,25),'#F4B098','#D88A78',bisel=False)
    # la manito que sostiene la cuchara
    i.fill(lambda d,p:ell(d,p,34,28,41,34),'#FBE2C8','#E0B090')
    i.light(lambda d,p:ell(d,p,11,9,20,15),'#FFFFFF',.5,.8)
    return i.done()
def v_huevo_oro():
    """El huevo dorado del recuerdo: brilla igual que el de la mascota, con la mitad del Corazón latiendo adentro."""
    i=Ic()
    i.light(lambda d,p:ell(d,p,4,6,44,46),'#FFE27A',.55,3)
    i.fill(lambda d,p:ell(d,p,11,6,37,44),'#FFF6D0','#E8B850')
    i.tex(lambda d,p:[ell(d,p,15,14,21,19),ell(d,p,28,30,33,35)],'#FFFFFF',.6)
    i.light(lambda d,p:(ell(d,p,19,22,25,30),ell(d,p,23,22,29,30),poly(d,p,[(19.5,27),(28.5,27),(24,34)])),'#7AFF8A',.8,.8)
    i.light(lambda d,p:ell(d,p,15,10,22,18),'#FFFFFF',.8,.8)
    for (x,y,r) in ((6,10,2.5),(42,14,2),(40,40,2.2),(7,38,1.8)): i.fill(_star(x,y,r),'#FFF6B0','#FFC83A',bisel=False)
    return i.done()
def v_medio_corazon():
    """La mitad del Corazón del Mundo que brilla dentro de ella (verde y dorada, con el borde partido)."""
    i=Ic()
    i.light(lambda d,p:ell(d,p,4,4,44,44),'#B8FF8A',.5,3)
    i.fill(lambda d,p:(ell(d,p,7,9,26,28),poly(d,p,[(8,22),(24,22),(24,42)]),poly(d,p,[(20,9),(26,9),(26,42),(24,42)])),'#B8FF9A','#3EBA5A')
    i.fill(lambda d,p:poly(d,p,[(24,10),(28,16),(24,20),(28,26),(24,31),(27,36),(24,42)]),'#FFE27A','#D8A030',bisel=False)
    i.light(lambda d,p:ell(d,p,11,12,18,18),'#FFFFFF',.9,.6)
    return i.done()
def v_pagina():
    """Una página suelta del diario de Tilo: papel viejo, letra chueca, una cucharita dibujada y una mancha de galleta."""
    i=Ic()
    i.fill(lambda d,p:poly(d,p,[(8,5),(36,3),(41,42),(10,45)]),'#FBF0D2','#E2C890')
    i.fill(lambda d,p:poly(d,p,[(31,3.5),(36,3),(37,9)]),'#D8B878','#B8925A',bisel=False)
    i.tex(lambda d,p:[d.line(p(12,11+k*5,34-(k%2)*4,10+k*5),fill=255,width=int(.7*S)) for k in range(5)],'#6A4A3A',.7)
    i.fill(lambda d,p:(d.line(p(16,41,24,33),fill=255,width=int(1.2*S)),ell(d,p,22,29,28,35)),'#C8955A','#8A5A30',bisel=False)
    i.light(lambda d,p:ell(d,p,29,34,36,40),'#7A4A2A',.45,.6)
    i.light(lambda d,p:ell(d,p,10,6,22,14),'#FFFFFF',.6,1)
    i.fill(_star(42,5,3),'#FFF2A0','#FFC83A',bisel=False)
    return i.done()
def v_prisma():
    """Prisma, el dragón de cristal: cabeza facetada en violeta y celeste, cuernitos, ojo sereno y un destello."""
    i=Ic()
    i.fill(lambda d,p:(poly(d,p,[(12,8),(16,0),(19,9)]),poly(d,p,[(24,7),(30,0),(30,10)])),'#E6D8FF','#9C7CF0')
    i.fill(lambda d,p:poly(d,p,[(6,14),(20,6),(34,10),(46,22),(42,30),(26,32),(12,30),(4,24)]),'#D8C8FF','#7A5AD8')
    i.fill(lambda d,p:poly(d,p,[(10,30),(26,32),(30,46),(14,46)]),'#C8B4FF','#6A48C8')
    i.tex(lambda d,p:[d.line(p(20,6,22,30),fill=255,width=int(.5*S)),d.line(p(6,14,34,22),fill=255,width=int(.5*S)),d.line(p(34,10,30,28),fill=255,width=int(.5*S))],'#FFFFFF',.5)
    i.light(lambda d,p:poly(d,p,[(9,15),(19,9),(22,18),(12,22)]),'#9FE8FF',.7,.5)
    i.light(lambda d,p:d.arc(p(22,13,30,20),200,340,fill=255,width=int(.8*S)),'#2A1A40',1,.05)
    i.light(lambda d,p:(ell(d,p,40,23,42,25),ell(d,p,43,24,45,26)),'#4A3A80',.8,.05)
    i.fill(_star(42,8,3),'#FFFFFF','#B8E8FF',bisel=False)
    return i.done()
def v_cuchara():
    """La cuchara de palo de Tilo: sirve para todo."""
    i=Ic()
    i.fill(lambda d,p:(d.line(p(10,42,30,16),fill=255,width=int(3.2*S)),ell(d,p,26,4,42,20)),'#E2B07A','#9A6A38')
    i.light(lambda d,p:ell(d,p,29,7,36,13),'#FFFFFF',.6,.6)
    i.tex(lambda d,p:d.line(p(14,38,26,22),fill=255,width=int(.5*S)),'#7A4A28',.5)
    return i.done()
JUEGO+=[('ICO_TILDE',v_tilde),('ICO_TILDE_B',v_tilde_vuela),('ICO_BRIZNA',v_brizna),('ICO_BRIZNA_B',v_brizna_duerme),('ICO_BRIZNA_C',v_brizna_bosteza),
        ('ICO_TILO',v_tilo),('ICO_HUEVO_ORO',v_huevo_oro),('ICO_MEDIO_CORAZON',v_medio_corazon),('ICO_PAGINA_TILO',v_pagina),('ICO_PRISMA',v_prisma),('ICO_CUCHARA_PALO',v_cuchara)]

# (v40) los animales chilenos, Tilde la tenca, el chucao y el cóndor: reemplazan a los dibujos viejos
from iconos_chile import CHILE as _CHILE
_nom={n for n,_ in _CHILE}
JUEGO=[x for x in JUEGO if x[0] not in _nom]+_CHILE

# (v97) dibujos más trabajados para el cóndor, el quique, el elefante marino, el cisne, el tordo, el huemul y la vizcacha
from iconos_chile2 import NUEVOS as _NUEVOS
JUEGO=[(n, _NUEVOS.get(n, f)) for n, f in JUEGO]

# (v98) los íconos del claro de Tilo y del mirador (medallones sacados de sus fondos)
from iconos_chile2 import ZONAS_V98 as _ZONAS_V98
JUEGO += _ZONAS_V98
