// (1.0.22) Juntar áreas por redibujar (ui_inv_fusionar) no puede dejar restos: cada 10 cuadros se compara lo dibujado
// con un redibujo completo de la pantalla, con lluvia, nieve, tormenta, viento, de noche y en varias zonas.
// Uso: node sim/web/pruebas/redibujo.js [semilla]
const { crear } = require('./arnes.js');
(async () => {
  const s = await crear({ seed: +(process.argv[2] || 21) }); const x = s.x;
  let malos = 0;
  const calma = () => { for (let k = 0, q = 0; k < 200 && q < 6; k++) { s.frames(100); if (x.sim_panel_id()) { x.sim_close_panel(); s.tap(233, 300); q = 0; } else q++; } };
  s.listo(30); x.sim_close_panel(); s.frames(200);
  const revisar = (nom) => {
    let peor = 0;
    for (let k = 0; k < 30; k++) {
      s.frames(10, 33);
      x.sim_frame(0); /* (lo que quedó pedido al final del cuadro anterior se dibuja primero: no es un resto) */
      const fb = new Uint32Array(x.memory.buffer, x.sim_fb(), 466 * 466);
      const antes = Uint32Array.from(fb);
      x.sim_invalidar_todo(); x.sim_frame(0);
      const fb2 = new Uint32Array(x.memory.buffer, x.sim_fb(), 466 * 466);
      /* (solo cuenta un resto de verdad: más de 24 niveles de diferencia; 1-12 es redondeo de la mezcla al dibujar por partes) */
      let d = 0; for (let i = 0; i < antes.length; i++) if (antes[i] !== fb2[i]) { const a = antes[i], b = fb2[i]; if (Math.abs((a>>16&255)-(b>>16&255)) > 24 || Math.abs((a>>8&255)-(b>>8&255)) > 24 || Math.abs((a&255)-(b&255)) > 24) d++; }
      if (d > peor) peor = d;
      if (d) { let x1=999,y1=999,x2=-1,y2=-1; for (let i = 0; i < antes.length; i++) if (antes[i] !== fb2[i] && d) { const X=i%466,Y=(i/466)|0; x1=Math.min(x1,X);y1=Math.min(y1,Y);x2=Math.max(x2,X);y2=Math.max(y2,Y); } console.log("   ", nom, k, d, "px en", x1, y1, x2, y2); }
    }
    /* (unos pocos píxeles, por un cuadro, en el borde de una sombra semitransparente: el cuadro siguiente los corrige; un resto
     * de verdad, una gota o un animal que quedó pegado, son cientos de píxeles y se repite) */
    if (peor > 200) { malos++; console.log(`MAL: ${nom}: ${peor} píxeles quedaron sin redibujar`); s.png(`/tmp/redibujo_${nom}.png`); }
    else console.log(`${nom}: sin restos`);
  };
  x.sim_go(0); s.frames(300); calma();
  for (let k = 0; k < 300 && x.sim_periodo() !== 0; k++) { x.sim_set_now(x.sim_now() + 600); s.frames(2); }
  revisar('prado-dia');
  for (const [c, n] of [[2, 'lluvia'], [4, 'nieve'], [5, 'tormenta'], [3, 'viento'], [6, 'frio']]) { x.sim_set_weather(c); s.frames(120); calma(); revisar(n); }
  x.sim_set_weather(0); s.frames(60);
  for (let k = 0; k < 300 && x.sim_periodo() !== 2; k++) { x.sim_set_now(x.sim_now() + 600); s.frames(2); } s.frames(60); calma(); revisar('prado-noche');
  for (const [e, n] of [[1, 'bosque'], [2, 'lago'], [3, 'colina'], [5, 'isla']]) { x.sim_go(e); s.frames(300); calma(); revisar(n + '-noche'); }
  console.log(malos ? `${malos} problemas` : 'redibujo: todo bien');
  process.exit(malos ? 1 : 0);
})();
