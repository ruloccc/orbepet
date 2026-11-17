// Revisa que en ningún panel quede algo fuera del círculo de la pantalla ni tapando la X o la flecha de volver.
// Uso: node sim/web/pruebas/paneles.js   (sale con error si encuentra algo)
const { crear } = require('./arnes.js');
(async () => {
  const s = await crear({ seed: 7 }); const x = s.x;
  s.listo(30); x.sim_close_panel(); s.frames(200);
  x.sim_letter_now(); s.frames(30); x.sim_close_panel();
  for (const id of [3, 40, 75]) x.sim_give_object(id);
  for (const f of [0, 5, 13, 24, 40]) x.sim_card_seen(f); /* animales y criaturas en los Tesoros */
  x.sim_creature_known(1); x.sim_creature_known(4);
  s.frames(30); x.sim_close_panel();
  let total = 0;
  for (const modo of ['adulto', 'infante']) {
    if (modo === 'infante') { x.sim_quick(5); s.frames(60); x.sim_close_panel(); s.frames(30); }
    for (let k = 0; k < 64; k++) {
      if (!x.sim_test_panel(k)) continue;
      s.frames(3);
      const n = x.sim_check_panel();
      if (n) { total += n; console.log(`${modo} ${s.str(x.sim_test_panel_name(k))}: ${n}\n` + s.str(x.sim_check_text())); }
    }
    x.sim_close_panel(); s.frames(5);
  }
  console.log(total ? `${total} problemas` : 'paneles: todo dentro del círculo');
  process.exit(total ? 1 : 0);
})();
