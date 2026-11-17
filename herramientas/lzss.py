import numpy as np
def lz_encode(data):
    """LZSS: groups of 8 items led by a flag byte (bit=1 -> match). match = 2 bytes: offset(12 bits,1..4095) len-3 (4 bits => 3..18)."""
    n=len(data); out=bytearray(); i=0
    head={}                       # hash of 3 bytes -> list of recent positions
    items=[]                      # (is_match, bytes)
    def add(pos):
        if pos+2<n:
            k=(data[pos],data[pos+1],data[pos+2]); l=head.setdefault(k,[]); l.append(pos)
            if len(l)>48: del l[0]
    while i<n:
        best=0; boff=0
        if i+2<n:
            k=(data[i],data[i+1],data[i+2]); cand=head.get(k,())
            for p in reversed(cand):
                off=i-p
                if off>4095: break
                m=3; mx=min(18,n-i)
                while m<mx and data[p+m]==data[i+m]: m+=1
                if m>best: best,boff=m,off
                if best==18: break
        if best>=3:
            items.append((1,bytes([boff&255,((boff>>8)<<4)|(best-3)])))
            for j in range(best): add(i+j)
            i+=best
        else:
            items.append((0,bytes([data[i]]))); add(i); i+=1
    for g in range(0,len(items),8):
        grp=items[g:g+8]; flag=0
        for b,(m,_) in enumerate(grp):
            if m: flag|=1<<b
        out.append(flag)
        for _,by in grp: out+=by
    return bytes(out)
def lz_decode(buf,n):
    out=bytearray(); i=0
    while len(out)<n:
        flag=buf[i]; i+=1
        for b in range(8):
            if len(out)>=n: break
            if flag>>b&1:
                lo,hi=buf[i],buf[i+1]; i+=2; off=lo|((hi>>4)<<8); ln=(hi&15)+3
                for _ in range(ln): out.append(out[-off])
            else: out.append(buf[i]); i+=1
    return bytes(out)
