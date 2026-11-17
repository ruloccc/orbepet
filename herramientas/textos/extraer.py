import re, sys, json
# Extrae literales de cadena C (uniendo los adyacentes) con archivo, línea, contexto (tabla o función) y llamada.
def strip_comments(src):
    out=[];i=0;n=len(src)
    while i<n:
        c=src[i]
        if src.startswith('//',i):
            j=src.find('\n',i); j=n if j<0 else j; out.append(' '*(j-i)); i=j
        elif src.startswith('/*',i):
            j=src.find('*/',i+2); j=n if j<0 else j+2
            out.append(re.sub(r'[^\n]',' ',src[i:j])); i=j
        elif c=='"':
            j=i+1
            while j<n and src[j]!='"':
                if src[j]=='\\': j+=1
                j+=1
            out.append(src[i:j+1]); i=j+1
        elif c=="'":
            j=i+1
            while j<n and src[j]!="'":
                if src[j]=='\\': j+=1
                j+=1
            out.append(src[i:j+1]); i=j+1
        else: out.append(c); i+=1
    return ''.join(out)
def unesc(s):
    return s.replace('\\"','"').replace('\\n','\n').replace('\\\\','\\')
res=[]
for path in sys.argv[1:]:
    src=open(path,encoding='utf-8',errors='replace').read()
    s=strip_comments(src)
    # máscara sin cadenas para contar llaves
    mask=re.sub(r'"(?:\\.|[^"\\])*"',lambda m:'"'+' '*(len(m.group(0))-2)+'"',s)
    mask=re.sub(r"'(?:\\.|[^'\\])*'",lambda m:"'"+' '*(len(m.group(0))-2)+"'",mask)
    # para cada { abierta guardamos etiqueta
    ctx_at={}  # posición de cadena -> (fn,tabla)
    stack=[]; 
    strpos=[m.start() for m in re.finditer(r'"',mask)]
    events=sorted([(m.start(),'{') for m in re.finditer(r'\{',mask)]+[(m.start(),'}') for m in re.finditer(r'\}',mask)])
    import bisect
    curfn=''
    labels=[]
    for pos,ch in events:
        if ch=='{':
            pre=mask[max(0,pos-300):pos]
            lab=''
            mt=re.search(r'\b(\w+)\s*(\[[^\]=;]*\])*\s*=\s*$',pre)
            mf=re.search(r'\b(\w+)\s*\([^;{}]*\)\s*$',pre)
            if not stack and mf and mf.group(1) not in ('if','for','while','switch'): lab='fn:'+mf.group(1)
            elif mt: lab='tabla:'+mt.group(1)
            stack.append((pos,lab))
        else:
            if stack:
                o,lab=stack.pop(); labels.append((o,pos,lab,len(stack)))
    while stack:
        o,lab=stack.pop(); labels.append((o,len(mask),lab,len(stack)))
    def ctx_of(pos):
        fn='';tb='';fs=0
        best=[l for l in labels if l[0]<pos<l[1] and l[2]]
        for o,e,lab,d in sorted(best,key=lambda l:l[3]):
            if lab.startswith('fn:') and not fn: fn=lab[3:]; fs=o
            if lab.startswith('tabla:'): tb=lab[6:]
        cs=re.findall(r'\bcase\s+(\w+)\s*:',mask[fs:pos]) if fn else []
        return fn,tb,(cs[-1] if cs else '')
    i=0;n=len(s)
    while i<n:
        if s[i]=='"':
            start=i; parts=[]
            while True:
                j=i+1
                while s[j]!='"':
                    if s[j]=='\\': j+=1
                    j+=1
                parts.append(s[i+1:j]); k=j+1
                while k<n and s[k] in ' \t\n\r': k+=1
                if k<n and s[k]=='"': i=k; continue
                # macros de cadena tipo "a" X "b" no se unen
                i=k; break
            txt=unesc(''.join(parts))
            line=src.count('\n',0,start)+1
            fn,tb,cs=ctx_of(start)
            ctx=(fn+('/' if fn and tb else '')+tb) or '-'
            # llamada que la contiene
            pre=s[max(0,start-200):start]
            m=re.findall(r'(\b[A-Za-z_]\w*)\s*\((?![^()]*\))',pre)
            call=m[-1] if m else ''
            res.append(dict(f=path,l=line,ctx=ctx,case=cs,call=call,t=txt))
        else: i+=1
json.dump(res,open('/tmp/textos/crudo.json','w'),ensure_ascii=False,indent=0)
print(len(res))
