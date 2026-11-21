#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
static uint8_t F[128*1024]; static int borrados, escrituras;
static int AN_LEER(uint32_t d, void *b, uint32_t n){ if(d+n>sizeof F) return -1; memcpy(b,F+d,n); return 0; }
static int AN_ESCRIBIR(uint32_t d, const void *b, uint32_t n){ if(d+n>sizeof F) return -1; const uint8_t*s=b; for(uint32_t i=0;i<n;i++) F[d+i]&=s[i]; escrituras++; return 0; }
static int AN_BORRAR(uint32_t d){ if(d%4096) return -1; memset(F+d,0xFF,4096); borrados++; return 0; }
#include "../firmware/main/anillo.inc" /* gcc -std=gnu17 -O1 tests/test_anillo.c -o /tmp/ta && /tmp/ta */
static char modelo[1<<20]; static int mn;
static int fallas;
#define OK(c,...) do{ if(!(c)){ fallas++; printf("FALLA %d: ",__LINE__); printf(__VA_ARGS__); printf("\n"); } }while(0)
static void comprobar(anillo_t *a, int cap, const char *que){
  static char b[200000]; int v=an_leer_todo(a,b,cap);
  /* debe ser un sufijo del modelo, terminado igual, y largo >= cap - (largo de línea) cuando el modelo alcanza */
  OK(v<=cap,"%s: v %d > cap %d",que,v,cap);
  OK(v<=mn && memcmp(b,modelo+mn-v,v)==0,"%s: no es el final del modelo (v %d mn %d)",que,v,mn);
}
int main(void){
  memset(F,0xFF,sizeof F);
  anillo_t ev, lg;
  OK(an_abrir(&ev,0,6),"abrir ev"); OK(an_abrir(&lg,6*4096,26),"abrir lg");
  srand(3);
  /* 1) muchas líneas, con reaperturas (reinicios) por el camino */
  long cap_anillo=(26-1)*(4096-8);
  for(int r=0;r<4000;r++){
    char l[400]; int n=snprintf(l,sizeof l,"[%d s] linea %d %.*s\n",r,r,rand()%300,"xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    OK(an_agregar(&lg,l,n),"agregar");
    memcpy(modelo+mn,l,n); mn+=n;
    if(r%37==0){ anillo_t b; an_abrir(&b,6*4096,26); OK(b.cur==lg.cur&&b.off==lg.off&&b.seq==lg.seq,"reabrir r%d: cur %u/%u off %u/%u",r,b.cur,lg.cur,b.off,lg.off); lg=b; }
    if(r%500==0) comprobar(&lg,36000,"cap 36000");
  }
  comprobar(&lg,24576,"cap 24576"); comprobar(&lg,200000,"cap grande");
  { static char b[200000]; int v=an_leer_todo(&lg,b,200000); OK(v>=cap_anillo-400 && v<=cap_anillo+4096,"retiene %d (esperado ~%ld)",v,cap_anillo); OK(b[0]=='[',"empieza en línea entera"); }
  /* 2) no pisa la otra zona */
  { int ve=0; static char b[50000]; ve=an_leer_todo(&ev,b,50000); OK(ve==0,"eventos vacío: %d",ve); }
  OK(an_agregar(&ev,"evento uno\n",11),"ev"); { char b[100]; int v=an_leer_todo(&ev,b,100); OK(v==11&&!memcmp(b,"evento uno\n",11),"ev leer %d",v); }
  /* 3) 0xFF se cambia */
  { char x[3]={'a',(char)0xFF,'\n'}; an_agregar(&ev,x,3); char b[100]; int v=an_leer_todo(&ev,b,100); OK(v==14&&b[12]=='?',"ff"); }
  /* 4) vaciar */
  int bor=borrados; OK(an_vaciar(&lg),"vaciar"); OK(borrados==bor+1,"vaciar borra 1"); { char b[100]; OK(an_leer_todo(&lg,b,100)==0,"vacío tras vaciar"); }
  { anillo_t b; an_abrir(&b,6*4096,26); char bb[100]; OK(an_leer_todo(&b,bb,100)==0,"vacío tras vaciar y reabrir"); an_agregar(&b,"hola\n",5); OK(an_leer_todo(&b,bb,100)==5,"tras vaciar escribe"); }
  /* 5) corte de luz a mitad del borrado: el siguiente sector queda borrado sin cabecera */
  { anillo_t b; an_abrir(&b,6*4096,26); uint32_t sig=(b.cur+1)%b.n; memset(F+6*4096+sig*4096,0xFF,4096); anillo_t c; an_abrir(&c,6*4096,26); OK(c.cur==b.cur,"corte"); }
  printf("borrados %d escrituras %d fallas %d\n",borrados,escrituras,fallas);
  return fallas!=0;
}
