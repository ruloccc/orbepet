import json,collections,sys
sys.path.insert(0,'/tmp/textos')
from secciones import S
items=json.load(open('/tmp/textos/items.json'))
def match(x,sel):
    f,rest=sel.split(':',1)
    cases=None
    if '#' in rest: rest,c=rest.split('#'); cases=set(c.split(','))
    if x['b']!=f: return False
    if rest!='*' and x['fn']!=rest and x['tb']!=rest: return False
    if cases is not None and x['case'] not in cases: return False
    return True
asign=collections.OrderedDict()
for s,subs in S:
    for sub,nota,sels in subs: asign[(s,sub)]=(nota,[])
resto=[]
for x in items:
    if x['b']=='ui.c' and x['fn']=='ui_show_event' and x['t']!=x['t'].upper():
        asign[S[-1][0],S[-1][1][0][0]][1].append(x); continue
    done=False
    for s,subs in S:
        for sub,nota,sels in subs:
            hit=[i for i,q in enumerate(sels) if match(x,q)]
            if hit:
                x['ord']=hit[0]; asign[(s,sub)][1].append(x); done=True; break
        if done: break
    if not done: resto.append(x)
for k,v in asign.items(): v[1].sort(key=lambda x:(x.get('ord',99),x['f'],x['l']))
json.dump({'a':[[k[0],k[1],v[0],v[1]] for k,v in asign.items()],'resto':resto},open('/tmp/textos/asignado.json','w'),ensure_ascii=False)
for k,v in asign.items(): print(len(v[1]),k[1])
print('SIN CLASIFICAR',len(resto))
c=collections.Counter(x['b']+':'+x['fn']+('#'+x['case'] if x['case'] else '') for x in resto)
for k,n in c.most_common(): print(n,k)
