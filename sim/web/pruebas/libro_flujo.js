// ¿Salen las páginas del Libro en el flujo real? Pasa el tiempo sin tocar nada (salvo cerrar lo que aparezca).
const { crear } = require('./arnes.js');
(async () => {
  const s = await crear({ seed: 77 });
  s.listo(30);
  s.tap(232, 115); s.frames(100);
  let ult = -1;
  for (let d = 1; d <= 60; d++) {
    for (let h = 0; h < 24; h++) { s.horas(1, 120); s.tap(233, 358); s.tap(233, 415); }
    const b = s.x.sim_libro_bits();
    if (b !== ult) { console.log('día', d, 'bits', b.toString(2).padStart(8, '0')); ult = b; }
  }
  console.log('final bits', s.x.sim_libro_bits().toString(2).padStart(8, '0'));
})().catch((e) => { console.error('FALLÓ', e); process.exit(1); });
