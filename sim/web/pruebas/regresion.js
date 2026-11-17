// REGRESIÓN VISUAL Y DE TEXTOS (corre en el PC, con Node, sin la placa):
//   node regresion.js            compara cada panel con la línea base (regresion_base.json) y revisa sus textos
//   node regresion.js --base     guarda la línea base nueva con lo que dibuja ahora el simulador
// Para cada panel: hash de los píxeles (cambió el dibujo) + sim_check_panel (textos cortados, fuera del círculo, etc.).
const fs = require('fs'), path = require('path'), crypto = require('crypto');
const { crear } = require('./arnes.js');
const BASE = path.join(__dirname, 'regresion_base.json');
(async () => {
  const s = await crear();
  s.listo(30);
  const x = s.x;
  const fb = () => Buffer.from(new Uint8Array(x.memory.buffer, x.sim_fb(), 466 * 466 * 4));
  const hash = () => crypto.createHash('sha1').update(fb()).digest('hex').slice(0, 16);
  const guardar = process.argv.includes('--base');
  const base = !guardar && fs.existsSync(BASE) ? JSON.parse(fs.readFileSync(BASE, 'utf8')) : {};
  const ahora = {}, cambios = [], textos = [];
  for (let k = 0; k < 400; k++) {
    const nombre = x.sim_test_panel_name(k);
    if (!nombre) break;
    const n = s.str(nombre);
    if (n === '?') break;
    if (!x.sim_test_panel(k)) { textos.push(`${k} ${n}: no abrió`); continue; }
    s.frames(150);   // el dibujo de los sprites tarda en asentarse
    ahora[`${k} ${n}`] = hash();
    const malos = x.sim_check_panel();
    if (malos) textos.push(`${k} ${n}: ${s.str(x.sim_check_text()).replace(/\n/g, ' | ').slice(0, 200)}`);
    if (!guardar && base[`${k} ${n}`] && base[`${k} ${n}`] !== ahora[`${k} ${n}`]) cambios.push(`${k} ${n}`);
    if (!guardar && !base[`${k} ${n}`]) cambios.push(`${k} ${n} (nuevo, sin línea base)`);
    x.sim_close_panel(); s.frames(30);
  }
  if (guardar) { fs.writeFileSync(BASE, JSON.stringify(ahora, null, 1)); console.log(`línea base guardada: ${Object.keys(ahora).length} paneles`); }
  else {
    console.log(`paneles revisados: ${Object.keys(ahora).length}`);
    console.log(cambios.length ? 'DIBUJO CAMBIÓ en:\n  ' + cambios.join('\n  ') : 'dibujo: igual que la línea base');
  }
  console.log(textos.length ? 'AVISOS DE TEXTO (revisar a ojo; pueden ser falsos si el panel necesita un contexto especial):\n  ' + textos.join('\n  ') : 'textos: sin problemas');
  process.exit(cambios.length ? 1 : 0);
})();
