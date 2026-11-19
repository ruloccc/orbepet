// (1.0.32) La colección de la pesca: al sacar un pez (o un cangrejo, o uno raro) sale CUÁL es, con su dibujo, y si es
// nuevo dice «¡NUEVO EN TU COLECCIÓN!»; en el Cofre → PECES están los 30 (los que faltan, en sombra) y cada uno tiene su
// ficha. Fotos en /tmp/peces_sim.  Uso: node sim/web/pruebas/peces.js
const { crear } = require('./arnes.js');
const DIR = process.argv[2] || '/tmp/peces_sim';
require('fs').mkdirSync(DIR, { recursive: true });
(async () => {
  const s = await crear({ seed: 52 }); const x = s.x;
  let malos = 0;
  const mal = (m) => { malos++; console.log('MAL: ' + m); };
  const calma = () => { for (let k = 0, q = 0; k < 200 && q < 6; k++) { s.frames(100); if (x.sim_panel_id()) { x.sim_close_panel(); s.tap(233, 300); q = 0; } else q++; } };
  s.listo(30); x.sim_close_panel(); s.frames(200); calma();
  // pescar hasta que salga un animal (lo que pica es al azar: pez, bota, alga...); se fuerza el pez arcoíris
  let vio = false, fotos = 0;
  for (let intento = 0; intento < 25 && !vio; intento++) {
    x.sim_go(2); s.frames(60); calma();
    x.sim_pez_forzar(28);
    x.sim_pesca(2);
    for (let k = 0; k < 900; k++) {
      s.frames(1, 33);
      const f = x.sim_pesca_fase();
      if (f === 2 && k % 4 === 0) s.tap(233, 300); // PZ_PICO: tocar la pantalla varias veces
      if (f === 4 && !fotos) { s.frames(20, 33); s.png(`${DIR}/1_sale.png`); fotos = 1; }
      if (f === 4 && fotos === 1) { s.frames(40, 33); s.png(`${DIR}/2_celebracion.png`); fotos = 2; }
      if (f < 0) break;
      if (fotos === 2) { s.tap(233, 233); s.frames(30); break; }
    }
    if (x.sim_peces() > 0) vio = true; else { fotos = 0; s.frames(30); }
  }
  if (!vio) mal('pescando nunca salió un animal de la colección'); else console.log(`pescando: ${x.sim_peces()} en la colección`);
  // el cofre: los 30, algunos ya pescados
  calma();
  for (const k of [0, 3, 8, 15, 16, 21, 22, 26, 27, 29]) x.sim_pez_dar(k);
  x.sim_test_panel(90); s.frames(20); s.png(`${DIR}/3_cofre_peces.png`);
  if (x.sim_check_panel()) mal('los peces se salen del círculo: ' + s.str(x.sim_check_text()));
  if (x.sim_peces() < 10) mal('no quedaron anotados');
  s.tap(155, 160); s.frames(20); s.png(`${DIR}/4_ficha.png`);
  s.tap(233, 233); s.frames(10);
  x.sim_close_panel(); s.frames(10);
  x.sim_test_panel(87); s.frames(10); s.png(`${DIR}/5_cofre.png`);
  if (x.sim_check_panel()) mal('el cofre se sale del círculo: ' + s.str(x.sim_check_text()));
  x.sim_close_panel();
  console.log(malos ? `${malos} problemas` : 'colección de peces: todo bien');
  process.exit(malos ? 1 : 0);
})();
