// (1.0.30) Las visitas de Pochi no son siempre iguales: la de siempre (papas y paseo), solo papas, solo a pasear,
// a jugar con la tapita (tú tocas donde cae y ella la trae) y solo a saludar con sus cachorros. Cada una termina
// despidiéndose y se va. Fotos en /tmp/pochi.
// Uso: node sim/web/pruebas/pochi_visitas.js
const { crear } = require('./arnes.js');
const DIR = process.argv[2] || '/tmp/pochi';
require('fs').mkdirSync(DIR, { recursive: true });
(async () => {
  const s = await crear({ seed: 33 }); const x = s.x;
  let malos = 0;
  const mal = (m) => { malos++; console.log('MAL: ' + m); };
  const calma = () => { for (let k = 0, q = 0; k < 200 && q < 6; k++) { s.frames(100); if (x.sim_panel_id()) { x.sim_close_panel(); s.tap(233, 300); q = 0; } else q++; } };
  s.listo(30); x.sim_close_panel(); s.frames(200); calma();
  const fase = () => Math.floor(x.sim_po_estado() / 1000);
  const PO_HAMBRE = 2, PO_INVITA = 4, PO_PASEO = 5, PO_DESPIDE = 6, PO_TAPITA = 17;
  const NOM = ['siempre', 'papas', 'paseo', 'tapita', 'saluda'];
  for (let tp = 0; tp < 5; tp++) {
    x.sim_pochi(120 + tp); s.frames(10);
    const vistas = new Set(); let tiro = 0, foto = 0;
    let k = 0;
    for (; k < 6000 && fase() !== 0; k++) {
      s.frames(1, 33); const f = fase(); vistas.add(f);
      if (f === PO_HAMBRE && k % 60 === 59) x.sim_pochi(1); // (le da las papas: como tocar ¡SÍ!)
      if (f === PO_INVITA && (tp === 0 || tp === 2) && k % 90 === 89) x.sim_pochi(2); // (acepta el paseo)
      if (f === PO_PASEO && k > 600) { x.sim_pochi(3); }
      if (f === PO_TAPITA && Math.floor(x.sim_po_estado() / 10) % 10 === 1 && k % 40 === 39) {
        if (tiro === 0) { s.png(`${DIR}/tapita_espera.png`); s.tap(100, 330); } else x.sim_pochi(125);
        tiro++;
      }
      if (f === PO_TAPITA && Math.floor(x.sim_po_estado() / 10) % 10 === 2 && !foto) { s.frames(8, 33); s.png(`${DIR}/tapita_vuela.png`); foto = 1; }
      if (f === PO_TAPITA && Math.floor(x.sim_po_estado() / 10) % 10 === 3 && foto === 1) { s.png(`${DIR}/tapita_cae.png`); foto = 2; }
      if (k === 300) s.png(`${DIR}/visita_${NOM[tp]}.png`);
    }
    const lista = [...vistas].sort((a, b) => a - b).join(',');
    console.log(`${NOM[tp]}: fases ${lista}; se fue a los ${(k * 33 / 1000).toFixed(0)} s` + (tp === 3 ? `; tiros ${tiro}` : ''));
    if (fase() !== 0) mal(`${NOM[tp]}: no se fue`);
    if (!vistas.has(PO_DESPIDE)) mal(`${NOM[tp]}: no se despidió`);
    if ((tp === 0 || tp === 1) && !vistas.has(PO_HAMBRE)) mal(`${NOM[tp]}: no pidió papas`);
    if ((tp >= 2) && vistas.has(PO_HAMBRE)) mal(`${NOM[tp]}: pidió papas y no venía a eso`);
    if (tp === 2 && !vistas.has(PO_PASEO)) mal('paseo: no paseó');
    if ((tp === 1 || tp >= 3) && vistas.has(PO_PASEO)) mal(`${NOM[tp]}: paseó y no venía a eso`);
    if (tp === 3 && (!vistas.has(PO_TAPITA) || tiro < 3)) mal('tapita: no se jugó tres veces');
    if (tp === 4 && vistas.has(PO_TAPITA)) mal('saluda: jugó a la tapita');
    if (tp === 0 && !vistas.has(PO_PASEO)) mal('siempre: no paseó');
    s.frames(100); calma();
  }
  // sin tirarla nunca: te la pide y al final la tira ella; igual termina
  x.sim_pochi(123); let k = 0;
  for (; k < 9000 && fase() !== 0; k++) s.frames(1, 33);
  if (fase() !== 0) mal('tapita sin tocar: no terminó'); else console.log(`tapita sin tocar: terminó a los ${(k * 33 / 1000).toFixed(0)} s`);
  console.log(malos ? `${malos} problemas` : 'visitas de Pochi: todo bien');
  process.exit(malos ? 1 : 0);
})();
