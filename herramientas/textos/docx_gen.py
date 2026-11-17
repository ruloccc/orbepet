import json
from docx import Document
from docx.shared import Pt, Cm, RGBColor
from docx.enum.section import WD_ORIENT
from docx.enum.table import WD_TABLE_ALIGNMENT
from docx.oxml.ns import qn
from docx.oxml import OxmlElement
d=json.load(open('/tmp/textos/asignado.json'))
doc=Document()
sec=doc.sections[0]
sec.orientation=WD_ORIENT.LANDSCAPE; sec.page_width,sec.page_height=Cm(29.7),Cm(21.0)
for m in ('left_margin','right_margin','top_margin','bottom_margin'): setattr(sec,m,Cm(1.5))
st=doc.styles['Normal']; st.font.name='Arial'; st.font.size=Pt(9.5)
st.element.rPr.rFonts.set(qn('w:eastAsia'),'Arial')
for h,sz in (('Title',22),('Heading 1',16),('Heading 2',12.5)):
    s=doc.styles[h]; s.font.name='Arial'; s.font.size=Pt(sz); s.font.color.rgb=RGBColor(0x2E,0x4A,0x3A)
    s.element.rPr.rFonts.set(qn('w:asciiTheme'),'') if False else None
def shade(cell,hexc):
    tcPr=cell._tc.get_or_add_tcPr(); sh=OxmlElement('w:shd'); sh.set(qn('w:val'),'clear'); sh.set(qn('w:color'),'auto'); sh.set(qn('w:fill'),hexc); tcPr.append(sh)
def width(cell,cm):
    cell.width=Cm(cm)
W=[1.5,12.6,10.0,2.6]
doc.add_heading('Todos los textos del juego — firmware 98',0)
p=doc.add_paragraph('Revisión uno por uno. Cada texto tiene un número (columna Nº): escribe tu arreglo en la columna «Tu corrección» (o «QUITAR» para borrarlo) y devuélveme este archivo; con eso aplico los cambios. La columna «Dónde» es para mí (archivo y línea del código).')
doc.add_paragraph('Orden: por tipo (comienzo, la mascota, el mundo, zonas, animales, personajes, historia, juegos, objetos, cartas, fiestas, viajes, menús, diario) y, dentro de cada tipo, en el orden en que suelen aparecer al jugar. Los textos en minúscula se muestran igual en MAYÚSCULAS en la pantalla.')
doc.add_heading('Marcas que se reemplazan solas',2)
for a,b in [('%s, %d, %u','un nombre, una cosa o un número que pone el juego'),('{N}','el nombre de la mascota'),('{A}','otro amigo que conoce'),('{O} / {o}, {a}, {la}, {ella}, {lo}, {una}','cambian según sea niña o niño (ej.: «CANSAD{O}» → CANSADA / CANSADO)'),('{p}','«a» si la carta es de mamá, «o» si es de papá'),('{GUARDIANA}','GUARDIANA o GUARDIÁN')]:
    q=doc.add_paragraph(style='List Bullet'); r=q.add_run(a+': '); r.bold=True; q.add_run(b)
doc.add_heading('Contenido',2)
cnt={}
for s,sub,nota,lst in d['a']:
    u=len({x['t'] for x in lst if x['b']!='paises.h'})+(1 if any(x['b']=='paises.h' for x in lst) else 0)
    cnt[s]=cnt.get(s,0)+u
for s,n in cnt.items():
    q=doc.add_paragraph(style='List Bullet'); q.add_run(f'{s} — {n} textos')
n_total=0; resumen=[]
sec_actual=None; si=0
for s,sub,nota,lst in d['a']:
    if not lst: continue
    if s!=sec_actual:
        sec_actual=s; si=int(s.split('.')[0]); k=0
        doc.add_page_break(); doc.add_heading(s,1)
    doc.add_heading(sub,2)
    if nota: doc.add_paragraph(nota).runs[0].italic=True
    vistos=set(); filas=[]
    for x in lst:
        if x['t'] in vistos: continue
        vistos.add(x['t']); filas.append(x)
    if sub.startswith('Nombre, fecha') :
        pa=[x for x in filas if x['b']=='paises.h']; filas=[x for x in filas if x['b']!='paises.h']
    else: pa=[]
    t=doc.add_table(rows=1,cols=4); t.style='Table Grid'; t.alignment=WD_TABLE_ALIGNMENT.CENTER; t.autofit=False
    for i,col in enumerate(t.columns): col.width=Cm(W[i])
    for i,h in enumerate(['Nº','Texto','Tu corrección','Dónde']):
        c=t.rows[0].cells[i]; c.text=''; r=c.paragraphs[0].add_run(h); r.bold=True; shade(c,'DCE8DD'); width(c,W[i])
    tr=t.rows[0]._tr; trPr=tr.get_or_add_trPr(); e=OxmlElement('w:tblHeader'); e.set(qn('w:val'),'true'); trPr.append(e)
    def fila(num,txt,donde):
        cells=t.add_row().cells
        for i,v in enumerate([num,txt,'',donde]):
            width(cells[i],W[i]); par=cells[i].paragraphs[0]
            lines=v.split('\n')
            for j,l in enumerate(lines):
                rr=par.add_run(l)
                if j<len(lines)-1: rr.add_break()
                if i==3: rr.font.size=Pt(7); rr.font.color.rgb=RGBColor(0x88,0x88,0x88)
                if i==0: rr.font.size=Pt(8)
    for x in filas:
        k+=1; n_total+=1
        fila(f'{si}-{k:03d}',x['t'],f"{x['b']}:{x['l']}")
    if pa:
        k+=1; n_total+=1
        fila(f'{si}-{k:03d}','LISTA DE PAÍSES ('+str(len(pa))+'): '+', '.join(x['t'] for x in pa),f"paises.h:{pa[0]['l']}")
    resumen.append((s,sub,len(filas)+(1 if pa else 0)))
doc.save('/home/claude/textos_del_juego_v98.docx')
print(n_total)
