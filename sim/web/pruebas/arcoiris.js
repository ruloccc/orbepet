// (1.0.31) El tobogán del arcoíris: se abre tocando el arcoíris del cielo (solo cuando hay uno) y desde Jugar; espera las
// instrucciones; el dedo la mueve a los lados como él se mueve; junta estrellitas; las nubes y pájaros quitan corazones;
// termina al llegar a la punta (o al quedarse sin corazones) y el motor lo anota en el diario. Fotos en /tmp/arcoiris.
// Uso: node sim/web/pruebas/arcoiris.js
const { crear } = require('./arnes.js');
const DIR = process.argv[2] || '/tmp/arcoiris';
require('fs').mkdirSync(DIR, { recursive: true });
(async () => {
  const s = await crear({ seed: 44 }); const x = s.x;
  let malos = 0;
  const mal = (m) => { malos++; console.log('MAL: ' + m); };
  const calma = () => { for (let k = 0, q = 0; k < 200 && q < 6; k++) { s.frames(100); if (x.sim_panel_id()) { x.sim_close_panel(); s.tap(233, 300); q = 0; } else q++; } };
  const fase = () => Math.floor(x.sim_zd_estado() / 1000);
  s.listo(30); x.sim_close_panel(); s.frames(200); calma();
  // sin arcoíris, tocar el cielo no abre nada
  s.tap(233, 75); s.frames(20);
  if (x.sim_zd_estado() !== -1) { mal('sin arcoíris se abrió el tobogán'); s.tap(233, 417); s.frames(30); }
  // con arcoíris: tocarlo lo abre
  x.sim_rare(0); s.frames(150); calma(); s.png(`${DIR}/0_cielo_con_arcoiris.png`);
  s.tap(233, 75); s.frames(20);
  if (fase() !== 0) mal('tocar el arcoíris no abrió el tobogán (' + x.sim_zd_estado() + ')'); else console.log('tocar el arcoíris: abre el tobogán');
  s.png(`${DIR}/1_portada.png`);
  // empieza: quieto mientras están las instrucciones
  s.tap(233, 233); s.frames(30, 33); s.png(`${DIR}/2_instrucciones.png`);
  if (x.sim_zr(1) !== 0) mal('avanzó mientras estaban las instrucciones');
  // jugar con el dedo: deslizar hacia la derecha la mueve a la derecha
  const x0 = x.sim_zr(2);
  x.sim_touch(150, 300, 1); s.frames(3, 33);
  for (let k = 0; k <= 20; k++) { x.sim_touch(150 + k * 4, 300, 1); s.frames(1, 33); }
  s.frames(15, 33);
  const x1 = x.sim_zr(2);
  if (!(x1 > x0 + 40)) mal(`el dedo no la movió a la derecha (${x0} -> ${x1})`); else console.log(`el dedo la mueve: x ${x0} -> ${x1}`);
  for (let k = 0; k < 120; k++) { x.sim_touch(230 + 50 * Math.sin(k / 15), 300, 1); s.frames(1, 33); if (k === 90) s.png(`${DIR}/3_jugando.png`); }
  x.sim_touch(230, 300, 0);
  // se deja: termina sola (al llegar o sin corazones)
  let k = 0;
  for (; k < 4000 && fase() === 1; k++) { s.frames(1, 33); if (k === 500) s.png(`${DIR}/3b_mas_rapido.png`); }
  for (let j = 0; j < 60 && fase() !== 2; j++) s.frames(1, 33);
  if (fase() !== 2) mal('no terminó (' + x.sim_zd_estado() + ')'); else console.log(`terminó a los ${((k + 150) * 33 / 1000).toFixed(0)} s; vidas ${x.sim_zr(0)}`);
  s.frames(40); s.png(`${DIR}/4_final.png`);
  // OTRA VEZ en modo prueba (juega sola): llega a la punta con estrellitas
  x.sim_zj_trampa(1);
  s.tap(233 - 29, 417); s.frames(30); s.tap(233, 233);
  for (k = 0; k < 4000 && fase() !== 2; k++) s.frames(1, 33);
  const pts = Math.floor((x.sim_zd_estado() % 1000) / 10);
  if (fase() !== 2 || pts < 5) mal(`jugando sola: no terminó bien (${x.sim_zd_estado()})`); else console.log(`jugando sola: ${pts} estrellitas, vidas ${x.sim_zr(0)}`);
  s.frames(40); s.png(`${DIR}/5_final_bien.png`);
  x.sim_zj_trampa(0);
  s.tap(233 + 29, 417); s.frames(40);
  if (x.sim_zd_estado() !== -1) mal('la X no cerró');
  // en Jugar: el botón ARCOÍRIS sale mientras hay arcoíris
  calma(); x.sim_test_panel(1); s.frames(10); s.png(`${DIR}/6_menu_jugar.png`);
  if (x.sim_check_panel()) mal('el menú Jugar se sale del círculo: ' + s.str(x.sim_check_text()));
  x.sim_close_panel(); s.frames(10);
  console.log(malos ? `${malos} problemas` : 'tobogán del arcoíris: todo bien');
  process.exit(malos ? 1 : 0);
})();
