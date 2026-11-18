// (1.0.18) Hedionda: se ven los vahos y las moscas; soplarle (tres soplidos) le quita el olor; tocar los vahos también.
// Y el mundo en pausa: con un menú abierto no aparece el olor encima ni hablan. Fotos en /tmp/olor.
const { crear } = require('./arnes.js');
const DIR = process.argv[2] || '/tmp/olor';
require('fs').mkdirSync(DIR, { recursive: true });
(async () => {
  const s = await crear({ seed: 31 }); const x = s.x;
  let malos = 0; const mal = (m) => { malos++; console.log('MAL: ' + m); };
  s.listo(30); x.sim_close_panel(); s.frames(200);
  for (let k = 0, q = 0; k < 200 && q < 6; k++) { s.frames(100); if (x.sim_panel_id()) { x.sim_close_panel(); s.tap(233, 300); q = 0; } else q++; }
  x.sim_hedionda(6); s.frames(60); s.png(`${DIR}/1_hedionda.png`);
  for (let k = 0; k < 3; k++) { x.sim_blow(); s.frames(20); }
  s.frames(30);
  if (x.sim_olor() !== 0) mal('tres soplidos no le quitaron el olor (' + x.sim_olor() + ')');
  s.frames(40); s.png(`${DIR}/2_soplada.png`);
  // tocando los vahos
  x.sim_hedionda(6); s.frames(60);
  let toques = 0;
  for (let k = 0; k < 120 && x.sim_olor(); k++) {
    // buscar un vaho: se tocan puntos alrededor de la cabeza
    for (const [dx, dy] of [[-14, -40], [14, -50], [-20, -60], [20, -70], [0, -80]]) { x.sim_tap_pet_rel ? x.sim_tap_pet_rel(dx, dy) : 0; }
    s.frames(5); toques++;
    if (!x.sim_tap_pet_rel) break;
  }
  // con un menú abierto no se ve el olor
  x.sim_hedionda(6); s.frames(30);
  x.sim_test_panel(3); s.frames(20); s.png(`${DIR}/3_menu.png`); x.sim_close_panel(); s.frames(30);
  console.log(malos ? `${malos} problemas` : 'olor: todo bien');
  process.exit(malos ? 1 : 0);
})();
