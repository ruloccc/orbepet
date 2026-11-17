// Capturas de las páginas del final secreto, varias por página para ver el movimiento.
const { crear } = require('./arnes.js');
(async () => {
  const s = await crear();
  s.listo(30); s.frames(100); s.tap(232, 115); s.tap(233, 360); s.frames(50); s.tap(233, 360); s.tap(232, 115); s.frames(50);
  s.x.sim_final_pend(2);
  s.frames(250);
  const tiempos = [40, 90, 160, 280]; // frames de 16 ms desde el inicio de cada página
  for (let p = 0; p < 7; p++) {
    let t = 0;
    tiempos.forEach((T, i) => { s.frames(T - t); t = T; s.png(`/tmp/fin_${p}_${i}.png`); });
    s.frames(60);
    s.tap(233, 358); /* SIGUE */
    s.frames(10);
  }
  console.log('final: capturas listas');
})();
