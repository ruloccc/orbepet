// Seis meses seguidos con la interfaz real: pasa el tiempo, toca de vez en cuando y vigila que no se caiga.
const { crear } = require('./arnes.js');
(async () => {
  const s = await crear({ seed: 4242 });
  s.listo(30);
  s.tap(232, 115); s.frames(100);
  let dias = 0;
  for (let d = 0; d < 180; d++) {
    s.horas(24, 300);
    if (d % 3 === 0) { s.tap(233, 233); s.tap(233, 300); }
    dias++;
    if (d % 30 === 29) console.log('día', d + 1, 'ok');
  }
  console.log('páginas del Libro leídas (bits):', s.x.sim_libro_bits().toString(2));
  console.log('soak: 180 días sin caerse');
})().catch((e) => { console.error('FALLÓ', e); process.exit(1); });
