// Partida nueva sin atajos: cae el huevo, ¿sale la página del Libro y después la hora?
const { crear } = require('./arnes.js');
(async () => {
  const s = await crear();
  for (let i = 0; i < 16; i++) { s.frames(250); s.png(`/tmp/inicio_${String(i).padStart(2, '0')}.png`); if (i === 8) s.tap(233, 233); }
  console.log('inicio: capturas listas');
})().catch((e) => { console.error('FALLÓ', e); process.exit(1); });
