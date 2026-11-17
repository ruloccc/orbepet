// Compara la vista previa de criaturas del editor (renderCri) con lo que dibuja el reloj (art.c).
// Uso: ./build/mascota_criaturas_ref /tmp/ref.bin && node editor/pruebas/prueba_criaturas.js /tmp/ref.bin
const fs = require('fs'), path = require('path');
const html = fs.readFileSync(path.join(__dirname, '../plantilla.html'), 'utf8');
const a = html.indexOf('const C_OJO'), b = html.indexOf('/* px: tamaño de cada pixel en pantalla');
const code = html.slice(a, b);
const renderCri = new Function(code + '; return renderCri;')();
const buf = fs.readFileSync(process.argv[2] || 'criaturas_ref.bin');
let o = 0, total = 0, malos = 0, peor = '';
const EX = ['abiertos', 'feliz', 'triste', 'orgullo', 'cerrados'];
for (let e = 0; e < 8; e++) for (let et = 1; et <= 3; et++) {
  const ojo_x = buf.readInt8(o), ojo_y = buf.readInt8(o + 1), sep = buf.readInt8(o + 2), patitas = buf[o + 3]; o += 4;
  const px = new Uint8Array(3600);
  for (let i = 0; i < 900; i++) { const v = buf.readUInt32LE(o + i * 4); px.set([(v >> 16) & 255, (v >> 8) & 255, v & 255, v >>> 24], i * 4); }
  o += 3600;
  const cu = { px, ojo_x, ojo_y, sep, patitas };
  for (let x = 0; x < 5; x++) for (let esc = 3; esc <= 4; esc++) for (let fr = 0; fr < 2; fr++) {
    const R = esc === 3 ? 30 : 40, k = esc / 2;
    const js = renderCri(cu, { expr: EX[x], frame: fr, etapa: et, postura: x === 4 ? -2 : (x === 3 ? 1 : 0) }, k);
    let dif = 0;
    for (let i = 0; i < R * R; i++) {
      const v = buf.readUInt32LE(o + i * 4), c = (v >>> 24) ? (v & 0xFFFFFF) : -1, j = js[i];
      const near = (p, q) => p >= 0 && q >= 0 && Math.abs((p >> 16) - (q >> 16)) <= 2 && Math.abs(((p >> 8) & 255) - ((q >> 8) & 255)) <= 2 && Math.abs((p & 255) - (q & 255)) <= 2;
      if (!(c === j || near(c, j))) dif++;
    }
    o += R * R * 4; total++;
    if (dif) { malos++; if (!peor) peor = `e${e} et${et} ${EX[x]} esc${esc} fr${fr}: ${dif} pixeles`; }
  }
}
console.log((malos ? 'FALLA ' : 'OK   ') + `la vista previa del editor es igual al reloj en ${total - malos} de ${total} dibujos`, peor);
if (malos) process.exitCode = 1;
