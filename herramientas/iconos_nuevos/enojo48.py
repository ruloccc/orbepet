# (fw 102) La marca de enojo (ICO_ENOJO) en 48 px: cuatro trazos curvos como 💢. La versión anterior era un marco cuadrado.
# Escribe los índices en /home/claude/fotos/enojo_idx.txt; se copian a HDI_ENOJO_IDX en firmware/main/iconos_hd.inc.
import math,re,sys
S=48
def tipo(x,y):
    # supersampling 4x4: 0 nada, 2 relleno, 3 brillo
    best=0
    return best
idx=[0]*(S*S)
inside=[[0]*S for _ in range(S)]
for y in range(S):
    for x in range(S):
        n=0; hl=0
        for sy in range(4):
            for sx in range(4):
                px=x+(sx+.5)/4; py=y+(sy+.5)/4
                dx=px-24; dy=py-24
                if abs(dx)<2.8 or abs(dy)<2.8: continue
                qx=1 if dx>0 else -1; qy=1 if dy>0 else -1
                cx=24+qx*18; cy=24+qy*18
                d=math.hypot(px-cx,py-cy)
                if 11.5<=d<=17.5 and abs(dx)<17.5 and abs(dy)<17.5:
                    n+=1
                    if d<13.3: hl+=1
        if n>=8: inside[y][x]=3 if hl>=8 else 2
# contorno de 2 px
for y in range(S):
    for x in range(S):
        if inside[y][x]: idx[y*S+x]=inside[y][x]; continue
        near=any(0<=y+j<S and 0<=x+i<S and inside[y+j][x+i] for j in range(-2,3) for i in range(-2,3) if abs(i)+abs(j)<=3)
        if near: idx[y*S+x]=1
for y in range(S): print(''.join(' .#o'[v] for v in idx[y*S:(y+1)*S]))
open('/home/claude/fotos/enojo_idx.txt','w').write(','.join(map(str,idx)))
try:
    from PIL import Image
    pal={0:(0,0,0,0),1:(58,6,18,255),2:(230,50,80,255),3:(255,130,140,255)}
    im=Image.new('RGBA',(S,S))
    im.putdata([pal[v] for v in idx]); im.resize((240,240),Image.NEAREST).save('/home/claude/fotos/enojo.png')
except Exception as e: print(e)
