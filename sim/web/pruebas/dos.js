// (1.0.17) Los juegos de a dos: con una visita el menú JUGAR los muestra; cada uno se juega hasta el final (en modo
// prueba, ella juega sola y bien), el motor lo anota y al cerrar el mundo vuelve. Saca fotos en /tmp/dos.
// Uso: node sim/web/pruebas/dos.js   (sale con error si algo falla)
const { crear } = require('./arnes.js');
const DIR = process.argv[2] || '/tmp/dos';
(async () => {
  const s = await crear({ seed: 11 }); const x = s.x;
  let malos = 0;
  const mal = (m) => { malos++; console.log('MAL: ' + m); };
  s.listo(30); x.sim_close_panel(); s.frames(200);
  // 1) sin visita: el menú de siempre
  const jugar = 1; // (sim_test_panel: "jugar")
  x.sim_test_panel(jugar); s.frames(5); s.png(`${DIR}/menu_solo.png`); x.sim_close_panel(); s.frames(5);
  // 2) llega el amigo del barrio
  x.sim_neighbor_now();
  for (let i = 0; i < 400 && !x.sim_neighbor_here(); i++) s.frames(10, 100);
  if (!x.sim_neighbor_here()) mal('no llegó el amigo del barrio');
  for (let k = 0, quieto = 0; k < 200 && quieto < 6; k++) { s.frames(100); if (x.sim_panel_id()) { x.sim_close_panel(); s.tap(233, 300); quieto = 0; } else quieto++; } /* (lo que haya salido mientras: crecer, una carta...) */
  if (x.sim_panel_id()) mal('quedó un panel abierto: ' + x.sim_panel_id());
  x.sim_test_panel(jugar); s.frames(5); s.png(`${DIR}/menu_visita.png`);
  const n = x.sim_check_panel(); if (n) mal('el menú de a dos se sale del círculo: ' + s.str(x.sim_check_text()));
  // el botón de PING PONG en el menú abre el juego
  s.tap(233 - 62, 58 + 60 + 38); s.frames(10);
  if (x.sim_zd_estado() !== 0) mal('el botón PING PONG del menú no abrió el juego (' + x.sim_zd_estado() + ')');
  s.tap(233, 417); s.frames(20);
  x.sim_close_panel(); s.frames(5);
  // 3) cada juego hasta el final (modo prueba)
  const nombres = ['pingpong', 'cuerda', 'memorice', 'escondidas'];
  for (let j = 0; j < 4; j++) {
    const antes = x.sim_dos_jugadas();
    x.sim_zj_trampa(0);
    x.sim_zd_abrir(j); s.frames(20);
    if (x.sim_zd_estado() < 0) { mal(`${nombres[j]}: no se abrió`); continue; }
    s.png(`${DIR}/${nombres[j]}_0portada.png`);
    s.tap(233, 233); s.frames(60);
    if (Math.floor(x.sim_zd_estado() / 1000) !== 1) mal(`${nombres[j]}: no empezó (${x.sim_zd_estado()})`);
    x.sim_zj_trampa(1);
    let foto = false, t = 0;
    for (; t < 6000 && Math.floor(x.sim_zd_estado() / 1000) !== 2; t++) {
      s.frames(1, 33);
      if (!foto && t === (j === 1 ? 70 : (j === 3 ? 30 : 260))) { s.png(`${DIR}/${nombres[j]}_1juego.png`); foto = true; }
    }
    const e = x.sim_zd_estado();
    if (Math.floor(e / 1000) !== 2) mal(`${nombres[j]}: no terminó (${e})`);
    s.frames(60);
    s.png(`${DIR}/${nombres[j]}_2fin.png`);
    const despues = x.sim_dos_jugadas();
    if (Math.floor(despues / 1000) !== Math.floor(antes / 1000) + 1) mal(`${nombres[j]}: el motor no la anotó (${antes} -> ${despues})`);
    console.log(`${nombres[j]}: terminó en ${(t * 33 / 1000).toFixed(1)} s, marcador ${Math.floor(e % 1000 / 10)}-${e % 10}`);
    // OTRA VEZ (abajo a la izquierda) y luego la X
    x.sim_zj_trampa(0);
    s.tap(233 - 29, 417); s.frames(30);
    if (Math.floor(x.sim_zd_estado() / 1000) !== 0) mal(`${nombres[j]}: OTRA VEZ no volvió a la portada (${x.sim_zd_estado()})`);
    s.tap(233, 417); s.frames(30);
    if (x.sim_zd_estado() !== -1) mal(`${nombres[j]}: la X no cerró (${x.sim_zd_estado()})`);
  }
  // 4) sin tocar nada (sin modo prueba) el ping pong igual termina: la visita gana
  x.sim_zj_trampa(0);
  x.sim_zd_abrir(0); s.frames(20); s.tap(233, 233);
  let t = 0;
  for (; t < 9000 && Math.floor(x.sim_zd_estado() / 1000) !== 2; t++) s.frames(1, 33);
  const e = x.sim_zd_estado();
  if (Math.floor(e / 1000) !== 2 || e % 10 !== 5) mal(`ping pong sin jugar: esperaba que ganara la visita 0-5 (${e})`);
  else console.log(`ping pong sin tocar: la visita gana ${Math.floor(e % 1000 / 10)}-${e % 10} en ${(t * 33 / 1000).toFixed(0)} s`);
  s.tap(233 + 29, 417); s.frames(30);
  // 5) el diario tiene la partida
  const log = s.log();
  if (!/juego de a dos/.test(log)) mal('no quedó en el registro del simulador');
  s.png(`${DIR}/mundo_vuelve.png`);
  console.log(malos ? `${malos} problemas` : 'juegos de a dos: todo bien');
  process.exit(malos ? 1 : 0);
})();
