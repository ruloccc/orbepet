"""(v97) Dibujos nuevos, más trabajados, para los animales chilenos que se veían más flojos: el cóndor, el quique,
el elefante marino, el cisne de cuello negro, el tordo, el huemul y la vizcacha. Mismos nombres de ícono y mismo
tamaño que los de iconos_chile.py: solo cambia el dibujo. Todos miran a la derecha; el cuadro 1 es la animación."""
from lib import *
import math
def ell(d,p,x0,y0,x1,y1): d.ellipse(p(x0,y0,x1,y1),fill=255)
def poly(d,p,pts): d.polygon(p(*[c for q in pts for c in q]),fill=255)
def ln(d,p,pts,w): d.line(p(*[c for q in pts for c in q]),fill=255,width=int(w*S),joint='curve')
def star(cx,cy,r): return lambda d,p:poly(d,p,[(cx,cy-r),(cx+r*.3,cy-r*.3),(cx+r,cy),(cx+r*.3,cy+r*.3),(cx,cy+r),(cx-r*.3,cy+r*.3),(cx-r,cy),(cx-r*.3,cy-r*.3)])
def ojo(i,x,y,r=2.0,iris='#22141E'):
    i.light(lambda d,p:d.ellipse(p(x-r,y-r*1.15,x+r,y+r*1.15),fill=255),iris,1,.05)
    i.light(lambda d,p:d.ellipse(p(x-r*.65,y-r*.95,x+r*.05,y-r*.2),fill=255),'#FFFFFF',1,.05)
    i.light(lambda d,p:d.ellipse(p(x+r*.2,y+r*.35,x+r*.6,y+r*.75),fill=255),'#FFFFFF',.8,.05)
def ojo_aro(i,x,y,r=2.0,aro='#F4F0E8'):  # ojo sobre fondo oscuro: con un aro claro para que se vea
    i.light(lambda d,p:d.ellipse(p(x-r-.8,y-r*1.15-.8,x+r+.8,y+r*1.15+.8),fill=255),aro,1,.05)
    ojo(i,x,y,r)
def cerrado(i,x,y,r=2.0,col='#22141E'):
    i.light(lambda d,p:d.arc(p(x-r,y-r,x+r,y+r),20,160,fill=255,width=int(.75*S)),col,1,.05)
def mejilla(i,x,y,r=2.2,col='#FF8FA8',a=.55):
    i.light(lambda d,p:ell(d,p,x-r,y-r*.7,x+r,y+r*.7),col,a,.5)
def roca(i,y0=38,x0=2,x1=46):
    i.fill(lambda d,p:(ell(d,p,x0,y0,x1,y0+13),),'#A8A2B4','#5E586C')
    i.tex(lambda d,p:[d.arc(p(x0+6,y0+2,x0+16,y0+8),200,330,fill=255,width=int(.5*S)),d.arc(p(x1-18,y0+3,x1-8,y0+9),200,330,fill=255,width=int(.5*S))],'#3A3444',.45)
    i.light(lambda d,p:ell(d,p,x0+6,y0+1,x0+20,y0+4),'#FFFFFF',.35,.8)

def condor(vuela=False):
    """El cóndor: negro azabache, la bufanda blanca de plumas, la cabeza pelada rosadita con su cresta oscura, el pico
    ganchudo marfil y la gran mancha blanca del ala. Parado en su roca, o planeando con los dedos de las alas abiertos."""
    i=Ic()
    if vuela:
        # las alas abiertas, con las plumas de la punta separadas como dedos
        def ala(d,p,s):
            cx=24
            pts=[(cx+s*3,18),(cx+s*12,13),(cx+s*21,12.5),(cx+s*24.5,14)]
            for k in range(4):  # los dedos
                y=15.5+k*2.4
                pts+=[(cx+s*(24.5-k*.4),y),(cx+s*(21.5-k*.8),y+1.2)]
            pts+=[(cx+s*17,25),(cx+s*8,26),(cx+s*3,25)]
            poly(d,p,pts)
        i.fill(lambda d,p:(ala(d,p,1),ala(d,p,-1)),'#34323E','#121016')
        i.fill(lambda d,p:(poly(d,p,[(28,18.5),(40,15.5),(41,18.5),(29,22)]),poly(d,p,[(20,18.5),(8,15.5),(7,18.5),(19,22)])),'#FFFFFF','#C8C8D4',bisel=False)
        i.tex(lambda d,p:[d.line(p(30+k*2.6,18.4-k*.6,30.6+k*2.6,21.4-k*.8),fill=255,width=int(.4*S)) for k in range(4)]+[d.line(p(18-k*2.6,18.4-k*.6,17.4-k*2.6,21.4-k*.8),fill=255,width=int(.4*S)) for k in range(4)],'#9A9AAE',.6)
        i.fill(lambda d,p:(ell(d,p,18.5,15,29.5,33),poly(d,p,[(20,30),(28,30),(26.5,38),(21.5,38)])),'#3A3846','#141218')
        i.fill(lambda d,p:[ell(d,p,24+math.cos(a)*5.6-1.8,15.5+math.sin(a)*2.2-1.6,24+math.cos(a)*5.6+1.8,15.5+math.sin(a)*2.2+1.6) for a in [k*math.pi/6 for k in range(12)]],'#FFFFFF','#D4D4E0',bisel=False)
        i.fill(lambda d,p:ell(d,p,20,5,28,15),'#E2A496','#A86252')
        i.fill(lambda d,p:(ell(d,p,21.5,2.5,26.5,7.5),),'#5A3438','#2A1418',bisel=False)
        i.fill(lambda d,p:poly(d,p,[(22.4,12),(25.6,12),(25.2,15.5),(24,17),(22.8,15.5)]),'#F4E8D2','#BCA27C',bisel=False)
        i.light(lambda d,p:ell(d,p,23.2,15.4,24.8,17),'#3A2A22',.8,.05)
        for x in (22,26): ojo(i,x,10,1.3)
        mejilla(i,21,12.5,1.2,'#FF7A8A',.4); mejilla(i,27,12.5,1.2,'#FF7A8A',.4)
        i.light(lambda d,p:ell(d,p,30,13.5,40,15.5),'#8A9AF8',.35,.6)
        return i.done()
    roca(i,37)
    # la cola y el cuerpo
    i.fill(lambda d,p:poly(d,p,[(13,30),(3,43),(9,44),(18,35)]),'#2E2C38','#0E0C12')
    i.fill(lambda d,p:ell(d,p,9,14,35,42),'#383644','#121016')
    # las patas grises sobre la roca
    i.fill(lambda d,p:(ln(d,p,[(20,38),(19.5,41.5)],1.8),ln(d,p,[(27,38),(28,41.5)],1.8)),'#9A94A4','#5A5464',bisel=False)
    i.fill(lambda d,p:(ell(d,p,16.5,40.5,22.5,43),ell(d,p,25.5,40.5,31.5,43)),'#8A8494','#4A4454',bisel=False)
    # el ala doblada, con la gran mancha blanca
    i.fill(lambda d,p:poly(d,p,[(12,22),(28,20),(31,30),(24,40),(8,40)]),'#2C2A36','#0C0A10')
    # la mancha blanca del ala: plumas redondeadas en fila, como escamitas
    i.fill(lambda d,p:[ell(d,p,13+k*3.4,22.6-k*.3,18+k*3.4,30.5-k*.5) for k in range(4)],'#FFFFFF','#C6C6D2',bisel=False)
    i.tex(lambda d,p:[d.arc(p(13+k*3.4,22.6-k*.3,18+k*3.4,30.5-k*.5),20,160,fill=255,width=int(.45*S)) for k in range(4)],'#8A8AA0',.7)
    i.tex(lambda d,p:[d.line(p(11+k*3.5,33,13+k*3.5,39.5),fill=255,width=int(.45*S)) for k in range(4)],'#5A5A72',.5)
    # la bufanda blanca y esponjosa
    i.fill(lambda d,p:[ell(d,p,29+math.cos(a)*6.5-2.2,15+math.sin(a)*3.2-2,29+math.cos(a)*6.5+2.2,15+math.sin(a)*3.2+2) for a in [k*math.pi/7 for k in range(14)]],'#FFFFFF','#CFCFDC',bisel=False)
    i.light(lambda d,p:ell(d,p,24,12,32,15),'#FFFFFF',.6,.4)
    # la cabeza pelada, la cresta y el pico ganchudo
    i.fill(lambda d,p:ell(d,p,28,3,40,15),'#E6AA9A','#AC6656')
    i.fill(lambda d,p:(ell(d,p,31,0.5,38,5.5),ell(d,p,36.5,3.5,40,7.5)),'#5E363C','#2A1418',bisel=False)
    i.fill(lambda d,p:poly(d,p,[(39,7),(44.5,7.5),(47.5,10.5),(46.5,13.5),(44.5,11.5),(39.5,12)]),'#F6EAD4','#BCA27C',bisel=False)
    i.light(lambda d,p:ell(d,p,45,10.5,47.5,13.5),'#3A2A22',.75,.05)
    i.light(lambda d,p:d.arc(p(30,7,36,12),200,320,fill=255,width=int(.45*S)),'#9A4E44',.55,.05)
    ojo(i,36,8.5,1.6)
    mejilla(i,37.5,11.5,1.4,'#FF6E80',.4)
    i.light(lambda d,p:ell(d,p,13,16,24,21),'#7A8AF0',.35,1)
    i.light(lambda d,p:ell(d,p,30,4.5,34,7),'#FFFFFF',.45,.4)
    return i.done()

def quique(f=0):
    """El quique: largo y bajito como un hurón, el lomo gris plata, la cara, el pecho y las patas negras,
    y la franja blanca que le va de la frente hasta los hombros. Orejitas redondas y cara de travieso."""
    i=Ic()
    sh=1.6 if f else 0
    i.fill(lambda d,p:ell(d,p,0,22,13,30),'#D2D2CA','#8A8A84')          # la cola esponjosa
    i.tex(lambda d,p:[d.line(p(2+k*2.5,23,3+k*2.5,29),fill=255,width=int(.4*S)) for k in range(4)],'#6A6A64',.45)
    i.fill(lambda d,p:(ell(d,p,9+sh,32,15+sh,43),ell(d,p,15-sh,33,21-sh,43.5),ell(d,p,26+sh,32,32+sh,43),ell(d,p,31-sh,33,37-sh,43.5)),'#2E2A36','#100E14')
    i.fill(lambda d,p:ell(d,p,6,21,38,39),'#34303E','#121016')          # el cuerpo (abajo negro)
    i.fill(lambda d,p:poly(d,p,[(7,28),(10,20),(22,17),(34,19),(37,25),(30,26),(18,27.5)]),'#E2E2DA','#9A9A92')  # el lomo plateado
    i.tex(lambda d,p:[ell(d,p,x-.55,y-.55,x+.55,y+.55) for x in range(10,35,3) for y in (20,23) if (x//3+y)%2],'#6A6A64',.5)
    i.light(lambda d,p:ell(d,p,11,19,26,22),'#FFFFFF',.55,.8)
    # la cabeza negra con la franja blanca en forma de casco
    i.fill(lambda d,p:ell(d,p,28,13,45,31),'#34303E','#121016')
    i.fill(lambda d,p:d.arc(p(27,10.5,46,33),196,320,fill=255,width=int(2.6*S)),'#FFFFFF','#D6D8E0',bisel=False)
    i.fill(lambda d,p:(ell(d,p,29,10.5,35.5,17),),'#3A3644','#141218')        # la orejita
    i.fill(lambda d,p:(ell(d,p,30.4,12,34,15.6),),'#E8E8EE','#A8A8B4',bisel=False)
    ojo_aro(i,40,22.5,1.7,'#4A4656')
    i.fill(lambda d,p:ell(d,p,44.4,24.5,47.6,27.6),'#1E1A22','#08060A',bisel=False)
    i.light(lambda d,p:ell(d,p,45,25,46.2,26),'#FFFFFF',.9,.05)
    i.light(lambda d,p:d.arc(p(40,25,45,30),30,150,fill=255,width=int(.45*S)),'#FFFFFF',.6,.05)
    mejilla(i,41,27.5,1.7,'#FF8FA8',.5)
    return i.done()

def elefante_marino(f=0):
    """El elefante marino: enorme, gordito y dormilón, tendido en la playa de hielo, con la nariz como trompita
    caída y los bigotes. En el segundo cuadro suspira (la trompita se infla) y sale un «z»."""
    i=Ic()
    i.fill(lambda d,p:d.rounded_rectangle(p(0,39,48,48),radius=3*S,fill=255),'#F2FAFF','#A8D2EE')
    i.light(lambda d,p:[d.line(p(6,42,14,42),fill=255,width=int(.5*S)),d.line(p(30,44,40,44),fill=255,width=int(.5*S))],'#FFFFFF',.8,.1)
    i.fill(lambda d,p:(poly(d,p,[(6,34),(0,28),(1.5,37)]),poly(d,p,[(6,36),(0,42),(8,41)])),'#7E706C','#4A3E3C')   # la cola de aletas
    i.fill(lambda d,p:ell(d,p,2,19,42,44),'#B2A098','#66574F')            # el cuerpo
    i.fill(lambda d,p:ell(d,p,12,31,40,44),'#D2C4BA','#A49488',bisel=False)  # la panza clarita
    i.tex(lambda d,p:[d.arc(p(8+k*5,21,20+k*5,37),205,325,fill=255,width=int(.5*S)) for k in range(4)],'#5A4C46',.4)
    i.fill(lambda d,p:ell(d,p,26,12,46,34),'#BCAAA2','#726058')           # la cabeza
    tr=1.6 if f else 0
    i.fill(lambda d,p:(ell(d,p,38,17-tr*.5,47.5,26+tr*.3),poly(d,p,[(41.5,23),(47.5,22),(47,32+tr),(42,31+tr)])),'#C8B6AC','#7E6C62')  # la trompita
    i.light(lambda d,p:[d.arc(p(41,21+k*3,48,25+k*3),200,340,fill=255,width=int(.45*S)) for k in range(3)],'#6A5852',.55,.05)
    i.fill(lambda d,p:(ell(d,p,42.5,30.5+tr,45.5,33+tr),),'#3A2E2C','#1A1210',bisel=False)
    cerrado(i,35,19,2.2,'#3A2A28')
    i.light(lambda d,p:[d.line(p(38,27,32,25.5),fill=255,width=int(.3*S)),d.line(p(38,28.5,32,29),fill=255,width=int(.3*S))],'#FFFFFF',.85,.05)
    mejilla(i,36,24,2.1)
    i.fill(lambda d,p:ell(d,p,22,33,34,42),'#8E7E78','#584A44')            # la aleta de adelante
    i.tex(lambda d,p:[d.line(p(25+k*2.5,38,26+k*2.5,41),fill=255,width=int(.4*S)) for k in range(3)],'#3A302C',.5)
    i.light(lambda d,p:ell(d,p,8,21,24,26),'#FFFFFF',.45,1)
    if f:  # el «z» del sueño (con trazo, no letra)
        i.fill(lambda d,p:ln(d,p,[(30,3),(35,3),(30,8.5),(35,8.5)],1.1),'#E8F2FF','#9AB8E8',bisel=False)
    return i.done()

def cisne(f=0):
    """El cisne de cuello negro: el cuerpo blanco y amplio con plumas en el ala, el cuello en S negro aterciopelado,
    la carúncula roja y el pico gris azulado. Lleva a su pollito gris en la espalda, como hacen ellos."""
    i=Ic()
    i.light(lambda d,p:(d.arc(p(0,39,22,46),200,340,fill=255,width=int(.8*S)),d.arc(p(22,40,46,47),200,340,fill=255,width=int(.8*S))),'#6AC8F0',.8,.2)
    # el cuello en S, negro aterciopelado (sale de detrás del pecho)
    nx=1.5 if f else 0
    i.fill(lambda d,p:ln(d,p,[(31,30),(35,24),(36,18.5),(34+nx,13.5),(35+nx,9)],5.0),'#463F56','#121018',bisel=False)
    i.light(lambda d,p:ln(d,p,[(34,24),(34.6,19),(33+nx,14)],1.1),'#8A82A8',.5,.3)
    i.fill(lambda d,p:(ell(d,p,2,22,38,43),poly(d,p,[(5,30),(0,21),(11,27)])),'#FFFFFF','#CCD8EA')   # el cuerpo y la colita alzada
    i.fill(lambda d,p:(ell(d,p,7,23,31,37),),'#FFFFFF','#BCCAE0')                                  # el ala
    i.tex(lambda d,p:[d.arc(p(8+k*4.5,25,18+k*4.5,35),205,335,fill=255,width=int(.5*S)) for k in range(4)],'#94A4C6',.55)
    i.tex(lambda d,p:[d.line(p(9+k*3,33,6+k*3,37),fill=255,width=int(.4*S)) for k in range(5)],'#94A4C6',.45)
    i.light(lambda d,p:ell(d,p,7,23,22,27),'#FFFFFF',.7,.8)
    # el pollito gris, acurrucado entre las alas
    i.fill(lambda d,p:(ell(d,p,13,15.5,23,24.5),ell(d,p,18.5,12,25.5,19)),'#ECEEF4','#A6AEC2')
    i.fill(lambda d,p:poly(d,p,[(25,15),(27.8,16),(25,17.2)]),'#7A8496','#4A5266',bisel=False)
    ojo(i,22.6,14.8,.95)
    mejilla(i,23.4,16.8,.9,'#FF9AB0',.5)
    i.fill(lambda d,p:ell(d,p,31+nx,3.5,42+nx,12.5),'#423C50','#121018')
    i.fill(lambda d,p:poly(d,p,[(40.5+nx,6.5),(47.5+nx,8.6),(47+nx,10.4),(40.5+nx,11)]),'#C8D2E0','#7A8496',bisel=False)
    i.fill(lambda d,p:ell(d,p,38.8+nx,4.6,42.6+nx,8.2),'#FF5A5A','#C82838',bisel=False)
    i.light(lambda d,p:d.line(p(33.8+nx,7.4,37.6+nx,6.4),fill=255,width=int(.7*S)),'#FFFFFF',.95,.05)
    ojo(i,37+nx,8.4,1.1)
    i.light(lambda d,p:ell(d,p,32.5+nx,4.5,36+nx,6.5),'#9A92C0',.5,.3)
    return i.done()

def pajaro_negro(f=0, tordo=True):
    """El tordo: negro brillante con reflejos violeta y azules, el pico negro y afilado, la cola larga en punta y
    los ojos oscuros y vivos. Parado en una ramita; en el segundo cuadro canta con el pico abierto."""
    i=Ic()
    i.fill(lambda d,p:ln(d,p,[(2,43),(24,40.5),(46,42)],2.4),'#9A6A42','#5A3A22',bisel=False)        # la ramita
    i.fill(lambda d,p:(ell(d,p,36,37.5,41,41),),'#7ACC5A','#3A8A3A',bisel=False)                    # una hojita
    i.fill(lambda d,p:poly(d,p,[(16,30),(2,40.5),(6,42.5),(21,35)]),'#2E2A3E','#0E0C14')             # la cola
    i.light(lambda d,p:ln(d,p,[(6,40),(17,32)],.7),'#6A6AE0',.45,.2)
    i.fill(lambda d,p:(ln(d,p,[(24,36),(23,41)],1.2),ln(d,p,[(29,36),(30,41)],1.2)),'#3A3442','#16121C',bisel=False)
    i.fill(lambda d,p:ell(d,p,13,17,37,38),'#3A3650','#100C18')                                         # el cuerpo
    i.fill(lambda d,p:poly(d,p,[(14,24),(29,21),(31,31),(20,36),(12,33)]),'#2C2840','#0C0A12')           # el ala
    i.tex(lambda d,p:[d.arc(p(14+k*3.5,23,24+k*3.5,35),210,330,fill=255,width=int(.45*S)) for k in range(3)],'#5A5AB0',.5)
    i.light(lambda d,p:ell(d,p,15,22,27,27),'#8A6AF0',.5,.8)                                           # reflejo violeta
    i.light(lambda d,p:ell(d,p,24,28,34,34),'#5A8AF0',.35,.8)                                          # reflejo azul
    i.fill(lambda d,p:ell(d,p,25,6,41,22),'#3E3A56','#141020')                                          # la cabeza
    if f:
        i.fill(lambda d,p:(poly(d,p,[(39.5,11.5),(48,9),(40.5,14)]),poly(d,p,[(39.5,15.5),(47.5,18),(40.5,14)])),'#2A2634','#0E0C12',bisel=False)
        i.light(lambda d,p:[d.arc(p(41,4,48,22),300,60,fill=255,width=int(.6*S)),d.arc(p(39,0,50,26),305,55,fill=255,width=int(.6*S))],'#FFE27A',1,.05)
    else:
        i.fill(lambda d,p:poly(d,p,[(39.5,11.5),(48,14),(39.5,16.5)]),'#2A2634','#0E0C12',bisel=False)
    ojo_aro(i,34.5,12.5,1.8,'#5A5670')
    i.light(lambda d,p:ell(d,p,28,7.5,35,11),'#A08AFF',.55,.5)
    mejilla(i,36,17,1.4,'#FF7A9A',.3)
    return i.done()

def huemul(f=0):
    """El huemul: robusto y de patas cortas, pardo con la panza clara, la Y oscura en la frente, las orejas grandes
    y los cuernos en horqueta. Tierno y firme, como en el escudo."""
    i=Ic()
    a,b=(2,-2) if f else (0,0)
    leg=lambda x0,x1:(lambda d,p:ln(d,p,[(x0,33),(x1,44.5)],2.6))
    i.fill(lambda d,p:(leg(11+a,10.5+a)(d,p),leg(16+b,16.5+b)(d,p),leg(28+b,27.5+b)(d,p),leg(32.5+a,33+a)(d,p)),'#8E7658','#4E3E2C',bisel=False)
    i.fill(lambda d,p:(ell(d,p,8.8+a,43.5,12.6+a,46.5),ell(d,p,14.6+b,43.5,18.4+b,46.5),ell(d,p,25.6+b,43.5,29.4+b,46.5),ell(d,p,31+a,43.5,34.8+a,46.5)),'#3A2E24','#1A140E',bisel=False)
    i.fill(lambda d,p:ell(d,p,4,17,37,37),'#AE946E','#6C573E')                   # el cuerpo redondito
    i.fill(lambda d,p:ell(d,p,10,28,32,37.5),'#E2D2B2','#B8A482',bisel=False)    # la panza
    i.fill(lambda d,p:ell(d,p,2.5,19,8.5,25),'#F2E8D8','#C8B8A0')               # la colita blanca
    i.light(lambda d,p:ell(d,p,9,19,24,23),'#FFFFFF',.4,1)
    i.fill(lambda d,p:poly(d,p,[(27,23),(31,11),(38.5,12.5),(36,29)]),'#AE946E','#6C573E')   # el cuello
    i.fill(lambda d,p:(ell(d,p,26.5,3.5,33.5,9.5),poly(d,p,[(28,5),(24.5,1.5),(30,6.5)])),'#A28A6A','#5E4C36')  # la oreja
    i.fill(lambda d,p:(ell(d,p,27.8,4.8,32.2,8.2),),'#E8C8B8','#B89484',bisel=False)
    # los cuernos en horqueta
    i.fill(lambda d,p:(ln(d,p,[(35,6),(34,1)],1.3),ln(d,p,[(34.4,3),(31,0.2)],1.1),ln(d,p,[(38,5.5),(40,0.5)],1.3),ln(d,p,[(39.4,2.6),(43,1)],1.1)),'#EEE0C2','#AC9672',bisel=False)
    i.fill(lambda d,p:ell(d,p,31,5,44,17),'#B69C76','#725C42')                   # la cabeza
    i.fill(lambda d,p:ell(d,p,38.5,9.5,47.5,16.5),'#D4C0A0','#94805E')           # el hocico
    i.tex(lambda d,p:(ln(d,p,[(37,5.6),(39.2,8.6)],.8),ln(d,p,[(41.6,5.4),(39.2,8.6)],.8),ln(d,p,[(39.2,8.6),(40.6,12.6)],.8)),'#4A3626',.45)  # la Y (suave, en la frente)
    ojo(i,36.2,11.2,1.9)
    i.fill(lambda d,p:ell(d,p,45,11.4,47.8,14),'#2A2028','#100C10',bisel=False)
    mejilla(i,40,14.6,1.6)
    return i.done()

def vizcacha(f=0):
    """La vizcacha: orejona, gris y de bigotes largos, sentada tomando sol en su roca, con la cola larga enroscada
    de punta negra y la panza clarita. En el segundo cuadro mueve las orejas."""
    i=Ic()
    roca(i,37,1,47)
    # la cola enroscada detrás
    i.fill(lambda d,p:d.arc(p(1,18,21,40),100,265,fill=255,width=int(3.4*S)),'#C8C0B2','#8A8274',bisel=False)
    i.fill(lambda d,p:ell(d,p,3,15.5,10,22.5),'#3E3436','#141012',bisel=False)
    i.fill(lambda d,p:ell(d,p,12,18,35,41),'#CEC6B8','#8A8274')                      # el cuerpo
    i.fill(lambda d,p:ell(d,p,18,25,31,40),'#F4EEE2','#D8D0C0',bisel=False)          # la panza
    i.fill(lambda d,p:(ell(d,p,15,36,22,41),ell(d,p,25,36,32,41)),'#BEB6A8','#7E7668')   # las patitas
    e=-1.8 if f else 0
    i.fill(lambda d,p:(poly(d,p,[(25,14),(22.5,-0.5+e),(28.5,-1+e),(30,13)]),poly(d,p,[(30.5,13),(32,-1),(37.5,0),(35,14)])),'#C8C0B2','#8A8274')
    i.fill(lambda d,p:(poly(d,p,[(26,12),(24.4,1+e),(27.8,.6+e),(28.8,12)]),poly(d,p,[(31.6,12),(32.8,.8),(36,1.6),(33.8,12)])),'#FFC8D2','#E8909E',bisel=False)
    i.fill(lambda d,p:ell(d,p,22.5,9,41.5,27),'#D8D0C2','#928A7C')                   # la cabeza
    i.fill(lambda d,p:ell(d,p,33,16.5,42.5,25),'#F4EEE2','#D2CAB8',bisel=False)      # el hocico
    i.tex(lambda d,p:ln(d,p,[(24,22),(29,25)],1.6),'#5A5050',.35)                    # la franjita oscura de la mejilla
    ojo(i,32.6,15.4,2.0)
    i.fill(lambda d,p:ell(d,p,39.6,17.4,42.6,20),'#FF9AB4','#D8607E',bisel=False)
    i.light(lambda d,p:[d.line(p(39,21,47.8,19),fill=255,width=int(.35*S)),d.line(p(39,22.2,47.8,23.6),fill=255,width=int(.35*S)),d.line(p(39,21.6,47.8,21.2),fill=255,width=int(.3*S))],'#4A4448',.8,.05)
    i.light(lambda d,p:d.arc(p(37,20,41,24),30,150,fill=255,width=int(.45*S)),'#5A3A3A',.8,.05)
    mejilla(i,35.6,21.2,2)
    i.light(lambda d,p:ell(d,p,25,11,33,15),'#FFFFFF',.5,.8)
    return i.done()

NUEVOS={
    'ICO_CONDOR':lambda:condor(False),'ICO_CONDOR_B':lambda:condor(True),
    'ICO_MAPACHE':lambda:quique(0),'ICO_MAPACHE_B':lambda:quique(1),
    'ICO_OSO_POLAR':lambda:elefante_marino(0),
    'ICO_CISNE':lambda:cisne(0),'ICO_CISNE_B':lambda:cisne(1),
    'ICO_CUERVO':lambda:pajaro_negro(0),'ICO_CUERVO_B':lambda:pajaro_negro(1),
    'ICO_CIERVO_CAFE':lambda:huemul(0),'ICO_CIERVO_CAFE_B':lambda:huemul(1),
    'ICO_TEJON':lambda:vizcacha(0),'ICO_TEJON_B':lambda:vizcacha(1),
}


# (v98) los íconos de las dos zonas nuevas: un medallón redondo sacado de su fondo (el mismo dibujo que se ve en el mundo)
def _zona_medallon(archivo, caja):
    import os
    from PIL import Image, ImageDraw
    raiz = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    im = Image.open(os.path.join(raiz, 'fondos_zonas', archivo)).convert('RGB').crop(caja).resize((84, 84), Image.LANCZOS)
    out = Image.new('RGBA', (96, 96), (0, 0, 0, 0))
    m = Image.new('L', (84, 84), 0); ImageDraw.Draw(m).ellipse((0, 0, 83, 83), fill=255)
    d = ImageDraw.Draw(out)
    d.ellipse((2, 2, 93, 93), fill=(58, 40, 30, 255))
    d.ellipse((4, 4, 91, 91), fill=(246, 230, 196, 255))
    out.paste(im, (6, 6), m)
    return out
ZONAS_V98 = [('ICO_ZONA_CLARO', lambda: _zona_medallon('claro_verano.png', (250, 40, 650, 440))),
          ('ICO_ZONA_MIRADOR', lambda: _zona_medallon('mirador_verano.png', (200, 120, 820, 740)))]
