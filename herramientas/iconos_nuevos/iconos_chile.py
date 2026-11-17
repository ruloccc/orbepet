"""(v40) Los animales chilenos: pichi, monito del monte, chuncho, cisne de cuello negro, tordo, quique, huemul, vizcacha, güiña,
gato colocolo, ranita de Darwin, lobo marino, elefante marino, pudú blanco, Tilde la tenca, el chucao y el cóndor.
Todos miran a la derecha (como los demás animales del mundo); el segundo cuadro (f=1) es el de la animación."""
from lib import *
def ell(d,p,x0,y0,x1,y1): d.ellipse(p(x0,y0,x1,y1),fill=255)
def poly(d,p,pts): d.polygon(p(*[c for q in pts for c in q]),fill=255)
def ln(d,p,pts,w): d.line(p(*[c for q in pts for c in q]),fill=255,width=int(w*S),joint='curve')
def star(cx,cy,r): return lambda d,p:poly(d,p,[(cx,cy-r),(cx+r*.3,cy-r*.3),(cx+r,cy),(cx+r*.3,cy+r*.3),(cx,cy+r),(cx-r*.3,cy+r*.3),(cx-r,cy),(cx-r*.3,cy-r*.3)])
def ojo(i,x,y,r=2.0,brillo=True):
    i.light(lambda d,p:d.ellipse(p(x-r,y-r*1.15,x+r,y+r*1.15),fill=255),'#22141E',1,.05)
    if brillo: i.light(lambda d,p:d.ellipse(p(x-r*.6,y-r*.9,x+r*.1,y-r*.15),fill=255),'#FFFFFF',1,.05)
def cerrado(i,x,y,r=2.0,col='#22141E'):
    i.light(lambda d,p:d.arc(p(x-r,y-r,x+r,y+r),20,160,fill=255,width=int(.7*S)),col,1,.05)
def mejilla(i,x,y,r=2.2,col='#FF8FA8',a=.55):
    i.light(lambda d,p:ell(d,p,x-r,y-r*.7,x+r,y+r*.7),col,a,.5)

def pichi(f=0, bola=False):
    """El pichi: un armadillo chiquito, color miel, con su caparazón de bandas y el hocico en punta."""
    i=Ic()
    if bola:
        i.fill(lambda d,p:ell(d,p,7,10,41,44),'#F2D29A','#A8804E')
        i.tex(lambda d,p:[d.arc(p(7+k*1.5,10,41-k*1.5,44),200+k*0,340,fill=255,width=int(.7*S)) for k in range(0,1)],'#7A5A34',.0)
        i.tex(lambda d,p:[d.line(p(x,12+abs(24-x)*.35,x,42-abs(24-x)*.35),fill=255,width=int(.8*S)) for x in (14,19,24,29,34)],'#8A6438',.55)
        i.fill(lambda d,p:ell(d,p,30,30,38,36),'#F6DEB8','#D0A878')  # el hocico que asoma
        i.fill(lambda d,p:ell(d,p,36,31.5,39,34),'#FF9AB4','#E06A8A',bisel=False)
        i.fill(lambda d,p:ell(d,p,26,8,31,14),'#E8C090','#B08858')  # la orejita
        i.light(lambda d,p:ell(d,p,12,13,24,21),'#FFFFFF',.55,.9)
        i.fill(star(42,8,2.4),'#FFF2A0','#FFC83A',bisel=False)
        return i.done()
    sh=1.2 if f else 0
    i.fill(lambda d,p:poly(d,p,[(9,33),(1,37),(2,39.5),(11,36.5)]),'#D8B07A','#9A7448')
    i.fill(lambda d,p:(ell(d,p,12+sh,35,18+sh,44),ell(d,p,28-sh,35,34-sh,44)),'#E6C49A','#A07A50')
    i.fill(lambda d,p:ell(d,p,8,15,38,40),'#F4D49C','#AE8450')
    i.tex(lambda d,p:[d.line(p(x,16+abs(23-x)*.25,x,38),fill=255,width=int(.8*S)) for x in (14,18.5,23,27.5,32)],'#8A6438',.55)
    i.tex(lambda d,p:[d.line(p(x,39,x-.5,41.5),fill=255,width=int(.35*S)) for x in range(10,37,3)],'#C8A070',.6)
    i.fill(lambda d,p:(ell(d,p,30,21,42,34),poly(d,p,[(38,24.5),(47.5,29),(38,32.5)])),'#F8DEB8','#D0A47A')
    i.fill(lambda d,p:ell(d,p,31,19,40,26),'#E8C48E','#B88E5A')
    i.fill(lambda d,p:ell(d,p,31,15,36,23),'#E8C090','#B08858')
    i.light(lambda d,p:ell(d,p,32.5,17,34.5,21),'#FF9AB4',.7,.2)
    ojo(i,37.5,27,1.7)
    i.fill(lambda d,p:ell(d,p,45.5,28,48,30.5),'#FF9AB4','#E06A8A',bisel=False)
    mejilla(i,39,31,2)
    i.light(lambda d,p:ell(d,p,13,17,26,24),'#FFFFFF',.5,1)
    return i.done()

def monito(f=0):
    """El monito del monte: chiquitito, de ojos enormes con antifaz oscuro, orejas redondas y la cola enrollada. Sostiene un piñón."""
    i=Ic()
    a=12 if f else 0
    i.fill(lambda d,p:d.arc(p(1,18,21,44),70+a,330+a,fill=255,width=int(3.4*S)),'#9A7A6A','#5E4638',bisel=False)
    i.fill(lambda d,p:ell(d,p,12,21,33,45),'#C4A694','#7E6252')
    i.fill(lambda d,p:ell(d,p,18,27,30,43),'#FAEEDF','#E2CDB6')
    i.fill(lambda d,p:(ell(d,p,13,3,23,13),ell(d,p,31,2,41,12)),'#C8AA98','#8A6E60')
    i.fill(lambda d,p:(ell(d,p,15.5,5.5,20.5,10.5),ell(d,p,33.5,4.5,38.5,9.5)),'#FFC2CE','#F08AA0',bisel=False)
    i.fill(lambda d,p:ell(d,p,15,6,40,29),'#D2B4A2','#8E7262')
    i.fill(lambda d,p:ell(d,p,18,19,37,29),'#FAEEDF','#E6D2BC',bisel=False)
    i.fill(lambda d,p:(ell(d,p,18.5,11.5,27.5,20.5),ell(d,p,28,11.5,37,20.5)),'#4A3436','#2A1C1E',bisel=False)
    for x in (23,32.5): ojo(i,x,16,2.9)
    i.fill(lambda d,p:ell(d,p,26.2,20,29.3,22.6),'#FF9AB4','#D8607E',bisel=False)
    i.light(lambda d,p:d.arc(p(25,21,30.5,25),20,160,fill=255,width=int(.5*S)),'#5A3A3A',1,.05)
    mejilla(i,20.5,23,2); mejilla(i,35,23,2)
    i.fill(lambda d,p:(ell(d,p,21,29,30,37),poly(d,p,[(21,33),(30,33),(25.5,39)])),'#B07A48','#6E4626')
    i.tex(lambda d,p:[d.line(p(22+k*2,31,24+k*2,37),fill=255,width=int(.4*S)) for k in range(3)],'#4A2E18',.5)
    i.fill(lambda d,p:(ell(d,p,18,30,23,35),ell(d,p,28,30,33,35)),'#D2B4A2','#9A7E6E')
    i.light(lambda d,p:ell(d,p,19,8,28,13),'#FFFFFF',.5,.8)
    return i.done()

def chuncho(f=0):
    """El chuncho: una lechucita chica, café con pintitas blancas, ojos amarillos y cejas blancas. En su rama."""
    i=Ic()
    i.fill(lambda d,p:d.rounded_rectangle(p(2,41,46,46),radius=2*S,fill=255),'#A06A3E','#5E3A1E')
    i.fill(lambda d,p:poly(d,p,[(18,40),(24,47),(30,40)]),'#8A5A34','#4A2E18')
    i.fill(lambda d,p:ell(d,p,9,8,39,45),'#B8865A','#6E4A2E')
    i.fill(lambda d,p:ell(d,p,15,24,33,44),'#F6E8D0','#DCC6A4')
    i.tex(lambda d,p:[d.line(p(x,27,x,41),fill=255,width=int(.9*S)) for x in (19,24,29)],'#9A6A40',.55)
    i.tex(lambda d,p:[ell(d,p,x-.9,y-.9,x+.9,y+.9) for (x,y) in ((14,12),(19,9.5),(24,9),(29,9.5),(34,12),(12,18),(36,18),(11,26),(37,26),(12,33),(36,33))],'#FFFFFF',.85)
    i.fill(lambda d,p:(ell(d,p,13,13,23.5,23.5),ell(d,p,24.5,13,35,23.5)),'#FFFFFF','#E8E2D2',bisel=False)
    if f:
        for x in (18.2,29.8): cerrado(i,x,18.5,3.2)
    else:
        i.fill(lambda d,p:(ell(d,p,14.5,14.5,22,22),ell(d,p,26,14.5,33.5,22)),'#FFE85A','#F2B020',bisel=False)
        for x in (18.2,29.8): ojo(i,x,18.3,2.2)
    i.fill(lambda d,p:poly(d,p,[(22,21.5),(26,21.5),(24,26.5)]),'#F2D27A','#B8902E',bisel=False)
    i.fill(lambda d,p:(ell(d,p,17,40,22,44),ell(d,p,26,40,31,44)),'#F2C84A','#B88A20',bisel=False)
    i.light(lambda d,p:ell(d,p,14,10,24,15),'#FFFFFF',.45,.9)
    return i.done()

def cisne(f=0):
    """El cisne de cuello negro: blanco, con el cuello y la cabeza negros como terciopelo y la carúncula roja. Con un pollito en la espalda."""
    i=Ic()
    i.light(lambda d,p:(d.arc(p(0,38,24,46),200,340,fill=255,width=int(.8*S)),d.arc(p(20,39,46,47),200,340,fill=255,width=int(.8*S))),'#6AC8F0',.8,.2)
    i.fill(lambda d,p:(ell(d,p,4,24,38,42),poly(d,p,[(6,30),(0,24),(10,28)])),'#FFFFFF','#D2DCEC')
    i.fill(lambda d,p:ell(d,p,9,25,30,36),'#FFFFFF','#C2CEE2')
    i.tex(lambda d,p:[d.arc(p(11+k*4,27,19+k*4,35),200,340,fill=255,width=int(.5*S)) for k in range(3)],'#9AA8C8',.5)
    # el pollito gris en la espalda
    i.fill(lambda d,p:(ell(d,p,13,18,22,26),ell(d,p,18,15,24,21)),'#E6E8F0','#A8B0C4')
    i.fill(lambda d,p:poly(d,p,[(23.5,17.5),(26,18.5),(23.5,19.5)]),'#7A8496','#4A5266',bisel=False)
    i.light(lambda d,p:ell(d,p,20.6,16.6,21.8,17.8),'#22141E',1,.05)
    nx=1.5 if f else 0
    i.fill(lambda d,p:ln(d,p,[(31,32),(35.5,25),(34+nx,17),(35.5+nx,10)],4.0),'#3A3448','#121018',bisel=False)
    i.fill(lambda d,p:ell(d,p,32+nx,5,42+nx,13),'#3A3448','#121018')
    i.fill(lambda d,p:poly(d,p,[(40.5+nx,7.5),(47.5+nx,9.5),(40.5+nx,11.5)]),'#B8C4D2','#7A8496',bisel=False)
    i.fill(lambda d,p:ell(d,p,38.5+nx,5.5,42.5+nx,9),'#FF5A5A','#C82838',bisel=False)
    i.light(lambda d,p:d.line(p(34.5+nx,8.5,37.5+nx,7.5),fill=255,width=int(.6*S)),'#FFFFFF',.9,.05)
    i.light(lambda d,p:ell(d,p,36+nx,8.5,38+nx,10.5),'#FFFFFF',1,.05)
    i.light(lambda d,p:ell(d,p,36.6+nx,9,37.6+nx,10),'#22141E',1,.05)
    i.light(lambda d,p:ell(d,p,8,26,22,30),'#FFFFFF',.6,1)
    return i.done()

def pajaro_negro(f=0, tordo=True):
    """El tordo: negro brillante con reflejos azules, pico negro afilado. En el segundo cuadro canta (con notita)."""
    i=Ic()
    i.fill(lambda d,p:poly(d,p,[(15,30),(3,40),(7,42.5),(19,34)]),'#3A3650','#141020')
    i.fill(lambda d,p:(d.line(p(23,37,21,45),fill=255,width=int(1.1*S)),d.line(p(28,37,29,45),fill=255,width=int(1.1*S))),'#4A4058','#2A2238',bisel=False)
    i.fill(lambda d,p:ell(d,p,12,18,36,39),'#3E3858','#16121E')
    i.fill(lambda d,p:ell(d,p,13,22,30,34),'#34304C','#100C18')
    i.light(lambda d,p:ell(d,p,14,24,26,30),'#6A7AE8',.55,.7)
    i.fill(lambda d,p:ell(d,p,25,8,41,24),'#3E3858','#16121E')
    if f:
        i.fill(lambda d,p:(poly(d,p,[(39,13),(47.5,11),(40,15.5)]),poly(d,p,[(39,17),(47,19.5),(40,16)])),'#4A4458','#1E1A28',bisel=False)
        i.fill(lambda d,p:(ell(d,p,41,1,45,5),d.line(p(44.5,3,44.5,-3),fill=255,width=int(.8*S))),'#FFE27A','#FFB43A',bisel=False)
    else:
        i.fill(lambda d,p:poly(d,p,[(39,13),(48,15.5),(39,18)]),'#4A4458','#1E1A28',bisel=False)
    ojo(i,34.5,13.8,1.8)
    i.light(lambda d,p:ell(d,p,32,13,37,14.8),'#000000',0,.05)
    i.light(lambda d,p:ell(d,p,28,9.5,34,13.5),'#8A9AF8',.55,.6)
    i.light(lambda d,p:ell(d,p,15,19,26,23),'#9AA8FF',.35,.9)
    return i.done()

def quique(f=0):
    """El quique: largo y bajito, lomo gris con pintitas, cara y patas negras y una franja blanca que le cruza la frente."""
    i=Ic()
    sh=1.5 if f else 0
    i.fill(lambda d,p:ell(d,p,0,24,12,31),'#C8C8C0','#7E7E78')
    i.fill(lambda d,p:(ell(d,p,9+sh,32,15+sh,42),ell(d,p,15-sh,33,21-sh,43),ell(d,p,26+sh,32,32+sh,42),ell(d,p,32-sh,33,38-sh,43)),'#3A3442','#18141E')
    i.fill(lambda d,p:ell(d,p,6,22,40,39),'#3E3848','#1A1620')
    i.fill(lambda d,p:ell(d,p,6,18,38,31),'#D8D8D0','#8E8E88')
    i.tex(lambda d,p:[ell(d,p,x-.6,y-.6,x+.6,y+.6) for x in range(9,36,3) for y in (21,24,27) if (x+y)%2],'#5A5A58',.55)
    i.fill(lambda d,p:ell(d,p,29,16,46,33),'#3E3848','#1A1620')
    i.fill(lambda d,p:d.arc(p(27,14,47,36),195,330,fill=255,width=int(2.4*S)),'#FFFFFF','#DADCE6',bisel=False)
    i.fill(lambda d,p:ell(d,p,30,12,40,19),'#D8D8D0','#9A9A94')
    i.fill(lambda d,p:ell(d,p,30.5,13,35.5,18),'#FFFFFF','#C8CAD4')
    i.light(lambda d,p:ell(d,p,37,22.5,42,27.5),'#5A5262',.9,.1)
    ojo(i,39.5,25,1.6)
    i.fill(lambda d,p:ell(d,p,44.5,26.5,47.5,29.5),'#2A2430','#100C14',bisel=False)
    i.light(lambda d,p:ell(d,p,45,27,46,28),'#FFFFFF',.9,.05)
    mejilla(i,41,30,1.8,'#FF8FA8',.45)
    i.light(lambda d,p:ell(d,p,10,19,26,23),'#FFFFFF',.45,1)
    return i.done()

def huemul(f=0):
    """El huemul: firme y pardo, con la Y oscura en la cara y los cuernos en horqueta. Como en el escudo."""
    i=Ic()
    a,b=(2,-2) if f else (0,0)
    i.fill(lambda d,p:(d.line(p(11+a,32,10+a,46),fill=255,width=int(2*S)),d.line(p(16+b,32,17+b,46),fill=255,width=int(2*S)),
                       d.line(p(28+b,32,27+b,46),fill=255,width=int(2*S)),d.line(p(32+a,32,33+a,46),fill=255,width=int(2*S))),'#8A7458','#4A3C2C',bisel=False)
    i.fill(lambda d,p:(ell(d,p,9.5+a,44,12.5+a,47),ell(d,p,15.5+b,44,18.5+b,47),ell(d,p,25.5+b,44,28.5+b,47),ell(d,p,31.5+a,44,34.5+a,47)),'#3A2E24','#1A140E',bisel=False)
    i.fill(lambda d,p:ell(d,p,5,18,37,36),'#A89070','#6A5640')
    i.fill(lambda d,p:ell(d,p,10,28,32,37),'#D8C8A8','#B09C7C',bisel=False)
    i.fill(lambda d,p:ell(d,p,3,19,9,26),'#F2E8D8','#C8B8A0')
    i.fill(lambda d,p:poly(d,p,[(28,22),(33,9),(40,12),(36,28)]),'#A89070','#6A5640')
    i.fill(lambda d,p:ell(d,p,32,6,45,17),'#B09878','#6E5A44')
    i.fill(lambda d,p:ell(d,p,40,10,47.5,16.5),'#CDB898','#8E7A5E')
    i.tex(lambda d,p:(d.line(p(36,8,40,12),fill=255,width=int(.9*S)),d.line(p(43,7.5,40,12),fill=255,width=int(.9*S)),d.line(p(40,12,42,15),fill=255,width=int(.9*S))),'#3A2A1E',.75)
    i.fill(lambda d,p:(ell(d,p,30,4,35,9),poly(d,p,[(31,5),(27,2),(32,8)])),'#A08868','#5A4834')
    i.fill(lambda d,p:(ln(d,p,[(36,6),(35,0.5)],1.2),ln(d,p,[(35.5,3.5),(31.5,0)],1.0),ln(d,p,[(38.5,6),(40.5,0.5)],1.2),ln(d,p,[(40,3),(43.5,1)],1.0)),'#E8D8B8','#A8946E',bisel=False)
    ojo(i,38.5,10.5,1.4)
    i.fill(lambda d,p:ell(d,p,45.5,11.5,48,14),'#2A2028','#100C10',bisel=False)
    i.light(lambda d,p:ell(d,p,9,20,24,25),'#FFFFFF',.4,1)
    return i.done()

def vizcacha(f=0):
    """La vizcacha: orejona y gris, con la cola larga y enroscada de punta negra, tomando sol en su roca."""
    i=Ic()
    i.fill(lambda d,p:ell(d,p,2,36,46,50),'#B4B4C4','#6E6E80')
    i.tex(lambda d,p:[d.line(p(10,40,16,41),fill=255,width=int(.5*S)),d.line(p(30,39,37,40),fill=255,width=int(.5*S))],'#4A4A5A',.5)
    i.fill(lambda d,p:d.arc(p(3,22,23,44),110,250,fill=255,width=int(3.4*S)),'#C8C0B2','#8A8274',bisel=False)
    i.fill(lambda d,p:d.arc(p(3,22,23,44),200,250,fill=255,width=int(3.6*S)),'#4A3E40','#1A1416',bisel=False)
    i.fill(lambda d,p:ell(d,p,13,18,35,41),'#D0C8BA','#8A8274')
    i.fill(lambda d,p:ell(d,p,18,25,31,40),'#F2ECE0','#D8D0C0',bisel=False)
    e=-2 if f else 0
    i.fill(lambda d,p:(poly(d,p,[(25,14),(23,-0.5+e),(28.5,-0.5+e),(30,13)]),poly(d,p,[(30,13),(31.5,-1),(36.5,0.5),(34,14)])),'#C8C0B2','#8A8274')
    i.fill(lambda d,p:(poly(d,p,[(26,12),(24.8,1+e),(27.6,1+e),(28.6,12)]),poly(d,p,[(31,12),(32.4,1),(35,2),(33,12)])),'#FFC8D2','#E8909E',bisel=False)
    i.fill(lambda d,p:ell(d,p,23,9,41,27),'#D8D0C2','#928A7C')
    i.fill(lambda d,p:ell(d,p,33,17,42,25),'#F2ECE0','#D2CAB8',bisel=False)
    ojo(i,33,15.5,1.9)
    i.fill(lambda d,p:ell(d,p,39.5,17.5,42.5,20),'#FF9AB4','#D8607E',bisel=False)
    i.light(lambda d,p:[d.line(p(39,21,47.5,19.5),fill=255,width=int(.35*S)),d.line(p(39,22,47.5,23.5),fill=255,width=int(.35*S))],'#4A4448',.8,.05)
    mejilla(i,36,21,2)
    i.light(lambda d,p:ell(d,p,25,11,33,15),'#FFFFFF',.5,.8)
    return i.done()

def gato_chico(f=0, colocolo=False):
    """La güiña (manchitas, la gatita más chica de América, con la mancha blanca detrás de la oreja)
    o el gato colocolo (rayas en las patas y en la cara, orejas en punta). Sentados, mirando un poco hacia ti."""
    i=Ic()
    base,osc=(('#D8C08A','#94784A') if colocolo else ('#D8B47A','#8E6A3C'))
    a=10 if f else 0
    i.fill(lambda d,p:d.arc(p(4,26,30,47),90+a,300+a,fill=255,width=int(3.2*S)),base,osc,bisel=False)
    if colocolo: i.tex(lambda d,p:[d.arc(p(4,26,30,47),k,k+8,fill=255,width=int(3.4*S)) for k in range(130,300,30)],'#6A4A2A',.7)
    else: i.tex(lambda d,p:[d.arc(p(4,26,30,47),k,k+10,fill=255,width=int(3.4*S)) for k in range(120,300,34)],'#2A1E18',.8)
    i.fill(lambda d,p:ell(d,p,12,22,34,46),base,osc)
    i.fill(lambda d,p:ell(d,p,17,28,29,45),'#FAF0DC','#E2D2B4',bisel=False)
    i.fill(lambda d,p:(ell(d,p,15,40,21,46),ell(d,p,25,40,31,46)),base,osc)
    if colocolo:
        i.tex(lambda d,p:[d.line(p(14,40+k*2,22,40+k*2),fill=255,width=int(.6*S)) for k in range(3)]+[d.line(p(24,40+k*2,32,40+k*2),fill=255,width=int(.6*S)) for k in range(3)],'#4A2E1A',.75)
        i.fill(lambda d,p:(poly(d,p,[(17,12),(17,-1),(26,8)]),poly(d,p,[(31,8),(40,-1),(40,12)])),base,osc)
    else:
        i.tex(lambda d,p:[ell(d,p,x-1,y-1,x+1,y+1) for (x,y) in ((15,27),(19,24),(29,24),(32,28),(14,33),(33,34),(18,31),(30,31))],'#2A1E18',.85)
        i.fill(lambda d,p:(poly(d,p,[(17,12),(16,2),(26,9)]),poly(d,p,[(31,9),(41,2),(40,12)])),base,osc)
    i.fill(lambda d,p:(poly(d,p,[(18.5,10),(18.3,3.5),(23.5,8.5)]),poly(d,p,[(33.5,8.5),(38.7,3.5),(38.5,10)])),'#FFC8D2','#E8909E',bisel=False)
    i.fill(lambda d,p:ell(d,p,15,6,42,28),base,osc)
    if not colocolo: i.light(lambda d,p:(ell(d,p,16.5,6.5,20,9.5),ell(d,p,37,6.5,40.5,9.5)),'#FFFFFF',.95,.1)
    i.fill(lambda d,p:ell(d,p,22,17,35,28),'#FAF0DC','#E6D6B8',bisel=False)
    if colocolo:
        i.tex(lambda d,p:[d.line(p(19,15,22,19),fill=255,width=int(.6*S)),d.line(p(38,15,35,19),fill=255,width=int(.6*S)),d.line(p(28.5,8,28.5,12),fill=255,width=int(.6*S))],'#6A3E1E',.8)
    else:
        i.tex(lambda d,p:[ell(d,p,x-.8,y-.8,x+.8,y+.8) for (x,y) in ((24,9.5),(28.5,8.5),(33,9.5),(19,13),(38,13))],'#2A1E18',.85)
    ojc='#C8E05A' if not colocolo else '#F2C83A'
    if f:
        for x in (23.5,33.5): cerrado(i,x,16.5,2.4)
    else:
        i.fill(lambda d,p:(ell(d,p,20.5,13.5,26.5,19.5),ell(d,p,30.5,13.5,36.5,19.5)),ojc,'#8AA02A' if not colocolo else '#C8902A',bisel=False)
        for x in (23.5,33.5):
            i.light(lambda d,p,x=x:ell(d,p,x-.9,13.8,x+.9,19.2),'#1A1014',1,.05)
            i.light(lambda d,p,x=x:ell(d,p,x-1.4,14.3,x-.3,15.6),'#FFFFFF',1,.05)
    i.fill(lambda d,p:poly(d,p,[(26.8,20.5),(30.2,20.5),(28.5,22.6)]),'#FF9AB4','#D8607E',bisel=False)
    i.light(lambda d,p:(d.arc(p(25.5,21.5,28.6,25),20,160,fill=255,width=int(.45*S)),d.arc(p(28.4,21.5,31.5,25),20,160,fill=255,width=int(.45*S))),'#5A3A3A',1,.05)
    i.light(lambda d,p:[d.line(p(22,22.5,14,21),fill=255,width=int(.3*S)),d.line(p(22,24,14,25),fill=255,width=int(.3*S)),d.line(p(35,22.5,43,21),fill=255,width=int(.3*S)),d.line(p(35,24,43,25),fill=255,width=int(.3*S))],'#FFFFFF',.9,.05)
    mejilla(i,21,23,2); mejilla(i,36,23,2)
    i.light(lambda d,p:ell(d,p,19,8,28,12),'#FFFFFF',.45,.8)
    return i.done()

def ranita(f=0):
    """La ranita de Darwin: chiquitita, verde, con la nariz en punta y la franja oscura del ojo. En una hoja.
    En el segundo cuadro infla el buche (ahí el papá guarda a los renacuajos)."""
    i=Ic()
    i.fill(lambda d,p:ell(d,p,1,34,47,47),'#7ACC5A','#3A8A3A')
    i.tex(lambda d,p:[d.line(p(4,40.5,44,40.5),fill=255,width=int(.6*S))]+[d.line(p(x,40.5,x+5,37),fill=255,width=int(.4*S)) for x in (10,20,30)],'#2E6A2E',.55)
    i.fill(lambda d,p:(ell(d,p,7,28,21,40),ell(d,p,4,36,14,40)),'#8ADA5A','#3E8E3A')
    if f: i.fill(lambda d,p:ell(d,p,26,25,38,36),'#F6F2C8','#D8D09A')
    i.fill(lambda d,p:(ell(d,p,10,16,36,38),poly(d,p,[(32,20),(46,26.5),(33,31)])),'#9AE268','#3E9A3E')
    i.fill(lambda d,p:ell(d,p,15,27,32,38),'#E8F6C8','#BEDC96',bisel=False)
    i.tex(lambda d,p:[ell(d,p,13,21,16,24),ell(d,p,19,18,21,20)],'#3E7A2E',.6)
    i.fill(lambda d,p:(ell(d,p,26,33,32,39),ell(d,p,31,34,37,39)),'#8ADA5A','#3E8E3A')
    i.light(lambda d,p:ln(d,p,[(46,26.5),(35,23),(24,24)],1.0),'#2A4A2A',.75,.1)
    i.fill(lambda d,p:ell(d,p,25.5,13,33.5,21),'#A6E878','#4AA040')
    ojo(i,29.5,17,2.1)
    i.light(lambda d,p:d.arc(p(36,26,42,30),20,160,fill=255,width=int(.5*S)),'#2A4A2A',1,.05)
    mejilla(i,35,28.5,1.6)
    i.light(lambda d,p:ell(d,p,14,18,24,22),'#FFFFFF',.55,.8)
    return i.done()

def lobo_marino(f=0):
    """El lobo marino: café, con su melena, el pecho en alto sobre el hielo y cara de buena siesta."""
    i=Ic()
    i.fill(lambda d,p:d.rounded_rectangle(p(1,40,47,48),radius=3*S,fill=255),'#F2FAFF','#A8D2EE')
    i.fill(lambda d,p:(poly(d,p,[(6,38),(0,33),(2,42)]),poly(d,p,[(6,40),(0,44),(9,43)])),'#7A5034','#4A2E1A')
    i.fill(lambda d,p:ell(d,p,3,28,34,44),'#B47A50','#6A4428')
    i.fill(lambda d,p:poly(d,p,[(18,36),(24,14),(38,16),(36,40)]),'#B47A50','#6A4428')
    i.fill(lambda d,p:ell(d,p,20,13,40,32),'#C88E5E','#7E5030')
    i.tex(lambda d,p:[d.arc(p(20+k*3,14,30+k*3,30),200,320,fill=255,width=int(.5*S)) for k in range(3)],'#5A3A22',.45)
    up=-2 if f else 0
    i.fill(lambda d,p:ell(d,p,27,4+up,43,19+up),'#BC8456','#704626')
    i.fill(lambda d,p:ell(d,p,36,10+up,47,18+up),'#E2B488','#A8784E')
    i.fill(lambda d,p:ell(d,p,29,6+up,32,9.5+up),'#7A5034','#4A2E1A',bisel=False)
    cerrado(i,36,10.5+up,1.8)
    i.fill(lambda d,p:ell(d,p,44.2,11.5+up,47.5,14.3+up),'#2A1E1E','#100A0A',bisel=False)
    i.light(lambda d,p:[d.line(p(42,15.5+up,48,14+up),fill=255,width=int(.3*S)),d.line(p(42,16.5+up,48,18+up),fill=255,width=int(.3*S)),d.line(p(41,16+up,36,18.5+up),fill=255,width=int(.3*S))],'#FFFFFF',.9,.05)
    mejilla(i,40,15.5+up,1.8)
    i.fill(lambda d,p:ell(d,p,26,35,38,44),'#8A5838','#54321C')
    if f: i.fill(lambda d,p:(ell(d,p,42,0,45.5,3.5),d.line(p(45,1.8,45,-4),fill=255,width=int(.8*S))),'#FFE27A','#FFB43A',bisel=False)
    i.light(lambda d,p:ell(d,p,8,30,22,34),'#FFFFFF',.4,1)
    return i.done()

def elefante_marino(f=0):
    """El elefante marino: enorme y dormilón, con la nariz como trompita, durmiendo sobre el hielo."""
    i=Ic()
    i.fill(lambda d,p:d.rounded_rectangle(p(0,40,48,48),radius=3*S,fill=255),'#F2FAFF','#A8D2EE')
    i.fill(lambda d,p:(poly(d,p,[(5,36),(0,30),(1,41)]),poly(d,p,[(5,38),(0,44),(8,43)])),'#7A6A66','#4A3E3C')
    i.fill(lambda d,p:ell(d,p,2,20,40,45),'#A8968E','#62544E')
    i.tex(lambda d,p:[d.arc(p(22+k*3,18,34+k*3,40),210,330,fill=255,width=int(.55*S)) for k in range(3)],'#4A3E3A',.45)
    i.fill(lambda d,p:ell(d,p,26,13,46,35),'#B4A29A','#6E5E58')
    i.fill(lambda d,p:ell(d,p,36,20,48,28),'#C2B0A8','#7E6E68')
    trompa=1.5 if f else 0
    i.fill(lambda d,p:(ell(d,p,39,18-trompa,47.5,27),poly(d,p,[(43,25),(47.5,24),(46.5,33+trompa),(42.5,32+trompa)])),'#BCA8A0','#7A6862')
    i.light(lambda d,p:[d.arc(p(41,22-trompa+k*3,48,26-trompa+k*3),200,340,fill=255,width=int(.45*S)) for k in range(3)],'#5A4A46',.6,.05)
    i.fill(lambda d,p:ell(d,p,43.5,31+trompa,46,33.2+trompa),'#3A2E2C','#1A1210',bisel=False)
    cerrado(i,36,20,2.2)
    mejilla(i,37,25,2)
    i.fill(lambda d,p:ell(d,p,24,34,36,43),'#8A7A74','#544844')
    if True: i.fill(lambda d,p:(poly(d,p,[(27,6),(32,6),(27.5,10)]),d.line(p(33,10,38,10),fill=255,width=int(.9*S))),'#B8D8FF','#6A9AE8',bisel=False)
    if f: i.fill(lambda d,p:poly(d,p,[(16,2),(20,2),(16.5,5)]),'#B8D8FF','#6A9AE8',bisel=False)
    i.light(lambda d,p:ell(d,p,8,23,24,28),'#FFFFFF',.4,1)
    return i.done()

def pudu(paso=0):
    """El pudú blanco: el ciervito más chico del mundo, blanquito y luminoso, con cuernitos de espiga y ojos enormes.
    paso: 0 parado, 1 y 2 caminando, 3 con la cabeza baja (comiendo)."""
    i=Ic()
    i.light(lambda d,p:ell(d,p,2,4,46,46),'#DFF2FF',.55,4)
    la={0:(0,0),1:(2,-2),2:(-2,2),3:(0,0)}[paso]
    i.fill(lambda d,p:(d.line(p(14+la[0],32,13+la[0],45),fill=255,width=int(2*S)),d.line(p(18+la[1],32,19+la[1],45),fill=255,width=int(2*S)),
                       d.line(p(29+la[1],32,28+la[1],45),fill=255,width=int(2*S)),d.line(p(33+la[0],32,34+la[0],45),fill=255,width=int(2*S))),'#F2F6FF','#B8C8E2',bisel=False)
    i.fill(lambda d,p:(ell(d,p,12+la[0],43.5,15+la[0],46.5),ell(d,p,17.5+la[1],43.5,20.5+la[1],46.5),ell(d,p,26.5+la[1],43.5,29.5+la[1],46.5),ell(d,p,32.5+la[0],43.5,35.5+la[0],46.5)),'#9AA8C8','#6A7898',bisel=False)
    i.fill(lambda d,p:ell(d,p,8,19,38,38),'#FFFFFF','#D2DEF0')
    i.fill(lambda d,p:ell(d,p,8,22,13,27),'#FFFFFF','#E2EAF8')
    if paso==3: hx_,hy=32,22
    else: hx_,hy=29,11
    i.fill(lambda d,p:poly(d,p,[(29,23),(hx_+1,hy+6),(hx_+8,hy+8),(36,30)]),'#FFFFFF','#D2DEF0')
    i.fill(lambda d,p:(ell(d,p,hx_-3,hy-1,hx_+3,hy+5),ell(d,p,hx_+7,hy-1,hx_+13,hy+5)),'#F2F6FF','#BCCAE2')
    i.fill(lambda d,p:(ell(d,p,hx_-1.8,hy+.5,hx_+1.8,hy+4),ell(d,p,hx_+8.2,hy+.5,hx_+11.8,hy+4)),'#FFD2DC','#F0A0B2',bisel=False)
    i.fill(lambda d,p:(ln(d,p,[(hx_+3.5,hy+1),(hx_+3,hy-3)],1.1),ln(d,p,[(hx_+7,hy+1),(hx_+7.5,hy-3)],1.1)),'#F2E8D2','#C8B48E',bisel=False)
    i.fill(lambda d,p:ell(d,p,hx_,hy,hx_+12,hy+12),'#FFFFFF','#D8E2F2')
    i.fill(lambda d,p:ell(d,p,hx_+7,hy+5,hx_+15,hy+11),'#FFFFFF','#DCE6F4')
    ojo(i,hx_+6.5,hy+5,2.0)
    i.fill(lambda d,p:ell(d,p,hx_+12.5,hy+6.5,hx_+15.5,hy+9),'#3A3448','#1A1424',bisel=False)
    mejilla(i,hx_+8.5,hy+8.5,1.8)
    if paso==3: i.fill(lambda d,p:(poly(d,p,[(hx_+13,hy+12),(hx_+15,hy+16),(hx_+12,hy+16)]),poly(d,p,[(hx_+15.5,hy+12),(hx_+17,hy+16),(hx_+14.5,hy+16)])),'#7ADA6A','#3A9A3A',bisel=False)
    i.light(lambda d,p:(ell(d,p,15,24,17,26),ell(d,p,20,22,22,24),ell(d,p,24,25,26,27)),'#E8EEFF',.9,.1)
    i.fill(star(6,8,2.6),'#FFFFFF','#B8E8FF',bisel=False)
    i.fill(star(42,30 if paso==3 else 4,2),'#FFF6C0','#FFD24A',bisel=False)
    return i.done()

def tenca(alas_arriba):
    """Tilde, la tenca: gris parda con rayitas, ceja blanca, cola larga oscura con puntas blancas, pico finito y curvo.
    Igual que antes: pañuelito rojo con puntos, ojito pícaro y un destello (le encanta lo brillante). Las tencas imitan todos los cantos."""
    i=Ic()
    i.fill(lambda d,p:poly(d,p,[(16,30),(1,43),(5,46),(20,34)]),'#6A5E56','#3A322E')
    i.fill(lambda d,p:(ell(d,p,0.5,41.5,4.5,45.5),),'#FFFFFF','#D2D2DA',bisel=False)
    i.fill(lambda d,p:(d.line(p(24,36,22,44),fill=255,width=int(1.1*S)),d.line(p(29,36,30,44),fill=255,width=int(1.1*S))),'#5A4E48','#2A2422',bisel=False)
    i.fill(lambda d,p:ell(d,p,13,18,37,38),'#9A8E84','#5E544C')
    i.fill(lambda d,p:ell(d,p,21,24,36,38),'#F2EADC','#D2C6B2')
    i.tex(lambda d,p:[d.line(p(24+k*3,27,23+k*3,34),fill=255,width=int(.5*S)) for k in range(4)],'#9A8A76',.55)
    if alas_arriba:
        i.fill(lambda d,p:poly(d,p,[(18,24),(6,4),(14,6),(26,22)]),'#8A7E74','#4A423C')
        i.tex(lambda d,p:[d.line(p(9+k*2.5,7+k*1,17+k*2,20),fill=255,width=int(.5*S)) for k in range(3)],'#FFFFFF',.55)
    else:
        i.fill(lambda d,p:ell(d,p,14,22,30,33),'#847870','#4A423C')
        i.tex(lambda d,p:[d.arc(p(15,23+k*3,29,31+k*3),200,340,fill=255,width=int(.5*S)) for k in range(2)],'#FFFFFF',.5)
    i.fill(lambda d,p:ell(d,p,26,8,42,24),'#A0948A','#635850')
    i.tex(lambda d,p:[d.line(p(28,10+k*2.5,34,10+k*2.5),fill=255,width=int(.4*S)) for k in range(2)],'#5A4E46',.5)
    i.fill(lambda d,p:poly(d,p,[(40,14.5),(47.5,17),(46,18.5),(40,18.5)]),'#4A403C','#1E1A18',bisel=False)
    i.fill(lambda d,p:(poly(d,p,[(25,20),(39,21),(33,28)]),ell(d,p,24,19,40,24)),'#FF6A6A','#C8283A')
    i.light(lambda d,p:(ell(d,p,29,21,31,23),ell(d,p,34,22,36,24)),'#FFFFFF',.9,.1)
    i.light(lambda d,p:d.line(p(30,11.2,40.5,12.6),fill=255,width=int(1.0*S)),'#FFFFFF',.95,.05)
    i.light(lambda d,p:d.line(p(32,15.8,39,15.8),fill=255,width=int(.8*S)),'#3A302C',.7,.05)
    ojo(i,35.5,14.4,2.0)
    i.fill(star(44,6,3),'#FFF2A0','#FFC83A',bisel=False)
    if alas_arriba: i.fill(lambda d,p:(ell(d,p,2,0,6,4),d.line(p(5.5,2,5.5,-4),fill=255,width=int(.8*S))),'#FFE27A','#FFB43A',bisel=False)
    return i.done()

def chucao(f=0):
    """El chucao: redondito, café oscuro, con el pecho y la cara color ladrillo, la panza a rayas y la cola parada.
    En el segundo cuadro canta fuerte ("¡chu-cao!"): así avisa cuando hay algo escondido."""
    i=Ic()
    i.fill(lambda d,p:poly(d,p,[(13,28),(5,9),(10.5,7.5),(18,25)]),'#6A4A36','#3A2618')
    i.fill(lambda d,p:(d.line(p(22,38,20,46),fill=255,width=int(1.3*S)),d.line(p(28,38,29,46),fill=255,width=int(1.3*S))),'#5A4A40','#2A2220',bisel=False)
    i.fill(lambda d,p:ell(d,p,10,15,37,41),'#7A563E','#3E2A1C')
    i.fill(lambda d,p:ell(d,p,15,30,33,41),'#FFFFFF','#E2DCD2')
    i.tex(lambda d,p:[d.line(p(16,32+k*2.4,33,32+k*2.4),fill=255,width=int(.7*S)) for k in range(4)],'#2A1E1A',.75)
    i.fill(lambda d,p:ell(d,p,20,15,38,33),'#F07E3E','#B84A1A')
    i.fill(lambda d,p:ell(d,p,24,5,41,22),'#7A563E','#3E2A1C')
    i.fill(lambda d,p:ell(d,p,30,10,41,22),'#F48C48','#C25A26')
    i.light(lambda d,p:d.ellipse(p(32.4,10.6,38.6,16.6),outline=255,width=int(.7*S)),'#FFFFFF',.8,.05)
    ojo(i,35.5,13.6,1.8)
    if f:
        i.fill(lambda d,p:(poly(d,p,[(40,15),(46.5,12.5),(40.5,17)]),poly(d,p,[(40,18),(46,20.5),(40.5,17)])),'#3A302C','#1A1412',bisel=False)
        i.light(lambda d,p:[d.arc(p(41,8,48,26),300,60,fill=255,width=int(.7*S)),d.arc(p(38,4,52,30),305,55,fill=255,width=int(.7*S))],'#FFE27A',1,.05)
    else:
        i.fill(lambda d,p:poly(d,p,[(40,15.5),(46,17),(40,18.5)]),'#3A302C','#1A1412',bisel=False)
    mejilla(i,36,19,1.7,'#FF6A6A',.4)
    i.light(lambda d,p:ell(d,p,26,7,34,11),'#FFFFFF',.45,.8)
    return i.done()

def condor(vuela=False):
    """El cóndor: negro, con la bufanda blanca de plumas, la cabeza pelada con su cresta y las alas con blanco.
    Parado en su roca del mirador, o volando con las alas bien abiertas."""
    i=Ic()
    if vuela:
        i.fill(lambda d,p:(poly(d,p,[(22,20),(0,12),(2,17),(-1,18),(3,21),(0,23),(6,25),(20,28)]),poly(d,p,[(26,20),(48,12),(46,17),(49,18),(45,21),(48,23),(42,25),(28,28)])),'#2E2C36','#121016')
        i.fill(lambda d,p:(poly(d,p,[(20,22),(8,17.5),(10,22),(20,25)]),poly(d,p,[(28,22),(40,17.5),(38,22),(28,25)])),'#F2F2F6','#BCBCC8',bisel=False)
        i.fill(lambda d,p:(ell(d,p,18,16,30,34),poly(d,p,[(20,32),(28,32),(24,40)])),'#2E2C36','#121016')
        i.fill(lambda d,p:ell(d,p,18.5,13,29.5,19),'#FFFFFF','#D8D8E2')
        i.fill(lambda d,p:ell(d,p,20,5,28,14),'#D89484','#9A5A4A')
        i.fill(lambda d,p:ell(d,p,21.5,3,26.5,7),'#8A2A2A','#5A1414',bisel=False)
        i.fill(lambda d,p:poly(d,p,[(22,12),(26,12),(24,15.5)]),'#F2E2C8','#B8A07A',bisel=False)
        for x in (22,26): ojo(i,x,9.5,1.0)
        return i.done()
    i.fill(lambda d,p:ell(d,p,1,38,47,52),'#A8A0B0','#5E5668')
    i.tex(lambda d,p:[d.line(p(8,42,15,43),fill=255,width=int(.5*S)),d.line(p(30,41,38,42),fill=255,width=int(.5*S))],'#3A3444',.5)
    i.fill(lambda d,p:poly(d,p,[(10,34),(2,44),(9,44),(16,38)]),'#2E2C36','#121016')
    i.fill(lambda d,p:(d.line(p(21,36,20,42),fill=255,width=int(1.6*S)),d.line(p(27,36,28,42),fill=255,width=int(1.6*S))),'#8A8494','#4A4454',bisel=False)
    i.fill(lambda d,p:ell(d,p,8,13,36,40),'#34323C','#121016')
    i.fill(lambda d,p:ell(d,p,10,18,32,36),'#2E2C36','#0E0C12')
    i.fill(lambda d,p:ell(d,p,13,21,28,29),'#F2F2F6','#BCBCC8')
    i.tex(lambda d,p:[d.arc(p(14+k*4,22,22+k*4,30),200,340,fill=255,width=int(.5*S)) for k in range(3)],'#5A5866',.5)
    i.fill(lambda d,p:(ell(d,p,22,10,40,19),ell(d,p,20,12,28,19)),'#FFFFFF','#D8D8E2')
    i.tex(lambda d,p:[ell(d,p,x-1,y-1,x+1,y+1) for (x,y) in ((24,15),(28,13),(32,16),(36,13))],'#C8C8D4',.8)
    i.fill(lambda d,p:ell(d,p,28,1,41,13),'#DE9C8A','#A0604E')
    i.fill(lambda d,p:(ell(d,p,31,-1,38,4),ell(d,p,36.5,1,40,4.5)),'#9A2E2E','#5E1616',bisel=False)
    i.fill(lambda d,p:(poly(d,p,[(39,5),(46,6.5),(46.5,10.5),(43.5,9),(39.5,10)])),'#F2E2C8','#B8A07A',bisel=False)
    i.fill(lambda d,p:ell(d,p,44.5,8.3,47,11),'#4A3A30','#2A1E18',bisel=False)
    ojo(i,36,6.8,1.5)
    i.tex(lambda d,p:[d.arc(p(30,6,36,11),200,320,fill=255,width=int(.4*S))],'#7A3A2E',.5)
    i.light(lambda d,p:ell(d,p,12,16,22,20),'#8A9AF8',.3,1)
    return i.done()

CHILE=[
    ('ICO_ERIZO',lambda:pichi(0)),('ICO_ERIZO_B',lambda:pichi(1)),('ICO_ERIZO_BOLA',lambda:pichi(0,True)),
    ('ICO_ARDILLA',lambda:monito(0)),('ICO_ARDILLA_B',lambda:monito(1)),
    ('ICO_BUHO',lambda:chuncho(0)),('ICO_BUHO_B',lambda:chuncho(1)),
    ('ICO_CISNE',lambda:cisne(0)),('ICO_CISNE_B',lambda:cisne(1)),
    ('ICO_CUERVO',lambda:pajaro_negro(0)),('ICO_CUERVO_B',lambda:pajaro_negro(1)),
    ('ICO_MAPACHE',lambda:quique(0)),('ICO_MAPACHE_B',lambda:quique(1)),
    ('ICO_CIERVO_CAFE',lambda:huemul(0)),('ICO_CIERVO_CAFE_B',lambda:huemul(1)),
    ('ICO_TEJON',lambda:vizcacha(0)),('ICO_TEJON_B',lambda:vizcacha(1)),
    ('ICO_GATO_MONTES',lambda:gato_chico(0)),('ICO_GATO_MONTES_B',lambda:gato_chico(1)),
    ('ICO_LINCE',lambda:gato_chico(0,True)),('ICO_LINCE_B',lambda:gato_chico(1,True)),
    ('ICO_SALAMANDRA',lambda:ranita(0)),('ICO_SALAMANDRA_B',lambda:ranita(1)),
    ('ICO_FOCA',lambda:lobo_marino(0)),('ICO_OSO_POLAR',lambda:elefante_marino(0)),
    ('ICO_CIERVO',lambda:pudu(0)),('ICO_CIERVO_B',lambda:pudu(1)),('ICO_CIERVO_C',lambda:pudu(2)),('ICO_CIERVO_D',lambda:pudu(3)),
    ('ICO_TILDE',lambda:tenca(False)),('ICO_TILDE_B',lambda:tenca(True)),
    ('ICO_CHUCAO',lambda:chucao(0)),('ICO_CHUCAO_B',lambda:chucao(1)),
    ('ICO_CONDOR',lambda:condor(False)),('ICO_CONDOR_B',lambda:condor(True)),
]
