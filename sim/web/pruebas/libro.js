// Saca capturas de las ocho páginas del Libro para revisar el diseño.
const { crear } = require('./arnes.js');
(async () => {
  const s = await crear();
  s.listo(30);
  s.x.sim_close_panel(); s.frames(60);   /* (fw 108) si no, las capturas salían con el panel del huevo encima */
  s.tap(232, 115); s.frames(200); /* ritmo tranquilo */
  for (let n = 0; n < 8; n++) {
    s.x.sim_libro(n);
    s.frames(700);
    s.png(`/tmp/libro_${n}.png`);
    s.tap(233, 358); s.frames(60); /* SIGUE */
  }
  console.log('libro: capturas listas');
})().catch((e) => { console.error('FALLÓ', e); process.exit(1); });
