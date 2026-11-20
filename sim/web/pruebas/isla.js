// (1.0.18) Los juegos de la isla: en Jugar salen primero los de la zona (el faro y el buceo), el faro solo de noche,
// cada uno se juega hasta el final (en modo prueba ella juega sola) y el motor lo anota. Fotos en /tmp/isla.
// Uso: node sim/web/pruebas/isla.js   (sale con error si algo falla)
const { crear } = require('./arnes.js');
const DIR = process.argv[2] || '/tmp/isla';
(async () => {
  const s = await crear({ seed: 21 }); const x = s.x;
  let malos = 0;
  const mal = (m) => { malos++; console.log('MAL: ' + m); };
  const calma = () => { for (let k = 0, q = 0; k < 200 && q < 6; k++) { s.frames(100); if (x.sim_panel_id()) { x.sim_close_panel(); s.tap(233, 300); q = 0; } else q++; } };
  s.listo(30); x.sim_close_panel(); s.frames(200);
  // la cueva: los cristales primero
  x.sim_go(4); s.frames(300); calma();
  x.sim_test_panel(1); s.frames(10); s.png(`${DIR}/menu_cueva.png`); x.sim_close_panel(); s.frames(10);
  // la isla, de día: el faro apagado
  x.sim_go(5); s.frames(300); calma();
  for (let k = 0; k < 300 && x.sim_periodo() !== 0; k++) { x.sim_set_now(x.sim_now() + 600); s.frames(2); } s.frames(60);
  for (let k = 0; k < 10 && x.sim_panel_id(); k++) { x.sim_close_panel(); s.frames(10); }
  x.sim_test_panel(1); s.frames(10); s.png(`${DIR}/menu_isla_dia.png`);
  if (x.sim_check_panel()) mal('el menú de la isla se sale del círculo: ' + s.str(x.sim_check_text()));
  x.sim_close_panel(); s.frames(10);
  // de noche
  x.sim_go(5); s.frames(200); calma();
  for (let k = 0; k < 300 && !(x.sim_periodo() === 1 && x.sim_hora_su() >= 18 && x.sim_hora_su() < 21); k++) { x.sim_set_now(x.sim_now() + 600); s.frames(2); } s.frames(60);
  for (let k = 0; k < 10 && x.sim_panel_id(); k++) { x.sim_close_panel(); s.frames(10); }
  console.log("periodo:", x.sim_periodo(), "hora de ella:", x.sim_hora_su());
  x.sim_test_panel(1); s.frames(10); s.png(`${DIR}/menu_isla_noche.png`);
  // el botón FARO (el primero, arriba a la izquierda)
  s.tap(233 - 98, 58 + 66 + 32); s.frames(20);
  if (x.sim_zd_estado() !== 0) mal('el botón del faro no abrió el juego (' + x.sim_zd_estado() + ')');
  s.tap(233, 417); s.frames(20); x.sim_close_panel(); s.frames(10);
  for (const [n, nom] of [[4, 'faro'], [5, 'buceo']]) {
    const antes = s.log().split('juego de la isla').length;
    x.sim_zj_trampa(0);
    x.sim_zd_abrir(n); s.frames(20);
    s.png(`${DIR}/${nom}_0portada.png`);
    s.tap(233, 233); s.frames(30);
    if (Math.floor(x.sim_zd_estado() / 1000) !== 1) mal(`${nom}: no empezó (${x.sim_zd_estado()})`);
    x.sim_zj_trampa(1);
    let t = 0;
    for (; t < 9000 && Math.floor(x.sim_zd_estado() / 1000) !== 2; t++) {
      s.frames(1, 33);
      if (t === 500) s.png(`${DIR}/${nom}_1juego.png`);
    }
    const e = x.sim_zd_estado();
    if (Math.floor(e / 1000) !== 2) mal(`${nom}: no terminó (${e})`);
    s.frames(60); s.png(`${DIR}/${nom}_2fin.png`);
    if (s.log().split('juego de la isla').length !== antes + 1) mal(`${nom}: el motor no lo anotó`);
    console.log(`${nom}: terminó en ${(t * 33 / 1000).toFixed(0)} s con ${Math.floor((e % 1000) / 10)} puntos`);
    x.sim_zj_trampa(0);
    s.tap(233 - 29, 417); s.frames(30);
    if (Math.floor(x.sim_zd_estado() / 1000) !== 0) mal(`${nom}: OTRA VEZ no volvió a la portada`);
    s.tap(233, 417); s.frames(30);
    if (x.sim_zd_estado() !== -1) mal(`${nom}: la X no cerró`);
  }
  // sin tocar nada también terminan
  for (const [n, nom] of [[4, 'faro'], [5, 'buceo']]) {
    x.sim_zd_abrir(n); s.frames(20); s.tap(233, 233);
    let t = 0;
    for (; t < 12000 && Math.floor(x.sim_zd_estado() / 1000) !== 2; t++) { s.frames(1, 33); if (t === 900 && n === 4) s.png(`${DIR}/faro_sin_luz.png`); }
    if (Math.floor(x.sim_zd_estado() / 1000) !== 2) mal(`${nom} sin tocar: no terminó (${x.sim_zd_estado()})`);
    else console.log(`${nom} sin tocar: terminó en ${(t * 33 / 1000).toFixed(0)} s con ${Math.floor((x.sim_zd_estado() % 1000) / 10)} puntos`);
    s.tap(233 + 29, 417); s.frames(30);
  }
  // (1.0.30) buceo: quieto mientras se leen las instrucciones; luego se maneja deslizando en cualquier parte
  x.sim_zd_abrir(5); s.frames(20); s.tap(233, 233); s.frames(45); s.png(`${DIR}/buceo_instrucciones.png`);
  x.sim_touch(330, 380, 1); s.frames(5, 33);
  for (let k = 0; k <= 30; k++) { x.sim_touch(330 - k * 3, 380 - k * 3, 1); s.frames(1, 33); }
  s.frames(20, 33); s.png(`${DIR}/buceo_deslizar.png`); x.sim_touch(240, 290, 0);
  s.tap(233, 417); s.frames(30); s.tap(233, 417); s.frames(30);
  // (1.0.34) el faro: quieto mientras están las instrucciones; el barquito que alumbras queda pegado a la luz
  x.sim_zd_abrir(4); s.frames(20); s.tap(233, 233); s.frames(30); s.png(`${DIR}/faro_instrucciones.png`);
  {
    // esperar el primer barquito y llevarle la luz encima; después, arrastrar la luz al muelle (206,330)
    let bx = -1, by = -1;
    for (let k = 0; k < 400 && bx < 0; k++) {
      s.frames(1, 33);
      for (let i = 0; i < 3; i++) { const v = x.sim_zj_dbg(100 + i); if (Math.floor(v / 1000000) === 1) { bx = Math.floor(v / 1000) % 1000; by = v % 1000; } }
    }
    if (bx < 0) mal('faro: no apareció ningún barquito');
    else {
      x.sim_touch(bx, by, 1); s.frames(3, 33);
      for (let k = 0; k < 6; k++) { for (let i = 0; i < 3; i++) { const v = x.sim_zj_dbg(100 + i); if (Math.floor(v / 1000000) === 1) { bx = Math.floor(v / 1000) % 1000; by = v % 1000; } } x.sim_touch(bx, by, 1); s.frames(2, 33); }
      const pts0 = Math.floor((x.sim_zd_estado() % 1000) / 10);
      // camino por el agua hasta el muelle (por abajo de la isla)
      const ruta = [[bx, by], [120, 380], [206, 360], [206, 330]];
      for (let r = 1; r < ruta.length; r++) {
        const [x0, y0] = ruta[r - 1], [x1, y1] = ruta[r];
        for (let k = 1; k <= 25; k++) { x.sim_touch(Math.round(x0 + (x1 - x0) * k / 25), Math.round(y0 + (y1 - y0) * k / 25), 1); s.frames(1, 33); }
      }
      for (let k = 0; k < 30; k++) { x.sim_touch(206, 330, 1); s.frames(1, 33); }
      s.png(`${DIR}/faro_pegado.png`);
      x.sim_touch(206, 330, 0);
      const pts1 = Math.floor((x.sim_zd_estado() % 1000) / 10);
      console.log(`faro con el barquito pegado: ${pts0} -> ${pts1} puntos`);
    }
  }
  s.tap(233, 417); s.frames(30);
  // con el dedo: arrastrar la luz hacia el muelle
  x.sim_zd_abrir(4); s.frames(20); s.tap(233, 233); s.frames(30);
  x.sim_touch(300, 300, 1); for (let k = 0; k < 400; k++) { x.sim_touch(300, 300, 1); s.frames(1, 33); } x.sim_touch(300, 300, 0);
  s.png(`${DIR}/faro_dedo.png`); s.tap(233, 417); s.frames(20);
  // (1.0.21) en la escena: tocar el faro (de noche) abre el faro; tocar el agua sobre la piedra de la izquierda, el buceo
  for (let k = 0; k < 6 && x.sim_zd_estado() !== -1; k++) { s.tap(233 + 29, 417); s.frames(30); s.tap(233, 417); s.frames(30); }
  x.sim_go(5); s.frames(200); calma();
  for (let k = 0; k < 300 && !(x.sim_periodo() === 1 && x.sim_hora_su() >= 18 && x.sim_hora_su() < 21); k++) { x.sim_set_now(x.sim_now() + 600); s.frames(2); } s.frames(60); calma();
  s.png(`${DIR}/escena_atardecer.png`);

  s.tap(392, 130); s.frames(20);
  if (x.sim_zd_estado() !== 0) mal('tocar el faro no abrió su juego (' + x.sim_zd_estado() + ')'); else console.log('tocar el faro: abre el juego');
  s.tap(233, 417); s.frames(30); calma();
  // (1.0.31) el buceo se abre con la piedra y con el agua de encima de ella (hasta la orilla); no con la arena ni más abajo
  for (const [tx, ty, debe] of [[39, 170], [60, 110, 1], [30, 140, 1], [85, 120, 1], [70, 95, 1], [130, 130, 0], [170, 200, 0]].map(([a, b, c]) => [a, b, c === undefined ? 1 : c])) {
    calma(); s.tap(tx, ty); s.frames(20);
    const abrio = x.sim_zd_estado() !== -1;
    if (abrio !== !!debe) mal(`tocar (${tx},${ty}) ${debe ? 'no abrió' : 'abrió'} el buceo`);
    if (abrio) { s.tap(233, 417); s.frames(30); }
  }
  s.tap(39, 170); s.frames(20);
  if (x.sim_zd_estado() !== 0) mal('tocar la piedra no abrió el buceo (' + x.sim_zd_estado() + ')'); else console.log('tocar la piedra: abre el buceo');
  s.png(`${DIR}/buceo_por_piedra.png`);
  s.tap(233, 417); s.frames(30);
  console.log(malos ? `${malos} problemas` : 'juegos de la isla: todo bien');
  process.exit(malos ? 1 : 0);
})();
