// Prueba el editor con un reloj de mentira en navigator.serial que habla el protocolo MASCOTA.
const { chromium } = require('playwright');
const path = require('path');
const salida = process.argv[2] || '.';
const FAKE = `
(() => {
  const st = { iconos: {}, sonidos: {}, amb: {}, cri: {}, nombres: {}, rx: null, fiestas: null, log: [] };
  window.__reloj = st;
  const enc = new TextEncoder(); let ctrl;
  const readable = new ReadableStream({ start(c) { ctrl = c; } });
  const decir = (l) => setTimeout(() => ctrl.enqueue(enc.encode(l + "\\r\\n")), 5);
  // una foto QOI de 4x4: todos rojos
  const qoi = new Uint8Array([113,111,105,102, 0,0,0,4, 0,0,0,4, 3,0, 0xFE,255,0,0, 0xC0|14, 0,0,0,0,0,0,0,1]);
  const b64 = (u) => btoa(String.fromCharCode(...u));
  function responder(l) {
    st.log.push(l.slice(0, 60));
    let m;
    if (l === "MASCOTA HOLA") return decir("MASCOTA OK 13 Kiwi");
    if ((m = l.match(/^MASCOTA HORA (\\d+) (-?\\d+)$/))) { st.hora = [+m[1], +m[2]]; return decir("MASCOTA HORA OK 1"); }
    if (l === "MASCOTA CAMBIOS") return decir("MASCOTA CAMBIOS I" + Object.keys(st.iconos).map(k=>" "+k).join("") + " S" + Object.keys(st.sonidos).map(k=>" "+k).join("") + " A" + Object.keys(st.amb).map(k=>" "+k).join("") + " C" + Object.keys(st.cri).map(k=>" "+k).join("") + " N" + Object.keys(st.nombres).map(k=>" "+k).join(""));
    if ((m = l.match(/^MASCOTA CRIATURA (\\d) (\\d) FABRICA$/))) { delete st.cri[m[1] + m[2]]; return decir("MASCOTA OK"); }
    if ((m = l.match(/^MASCOTA CRIATURA (\\d) (\\d) (\\S+)$/))) { const u = atob(m[3]); if (u.length !== 3612 || u.slice(0, 4) !== "CUER") return decir("MASCOTA ERROR criatura"); st.cri[m[1] + m[2]] = 1; return decir("MASCOTA OK"); }
    if ((m = l.match(/^MASCOTA ESPECIE (\\d) NOMBRE (.+)$/))) { st.nombres[m[1]] = m[2]; return decir("MASCOTA OK"); }
    if ((m = l.match(/^MASCOTA ESPECIE (\\d) FABRICA$/))) { delete st.nombres[m[1]]; return decir("MASCOTA OK"); }
    if ((m = l.match(/^MASCOTA ICONO (\\d+) FABRICA$/))) { delete st.iconos[m[1]]; return decir("MASCOTA OK"); }
    if ((m = l.match(/^MASCOTA ICONO (\\d+) (\\S+)$/))) { if (atob(m[2]).length !== 2304) return decir("MASCOTA ERROR icono"); st.iconos[m[1]] = 1; return decir("MASCOTA OK"); }
    if ((m = l.match(/^MASCOTA (SONIDO|AMBIENTE) (\\d+) (\\w+) ?(\\S*) ?(\\S*)$/))) {
      const dest = m[1] === "SONIDO" ? st.sonidos : st.amb;
      if (m[3] === "FABRICA") { delete dest[m[2]]; return decir("MASCOTA OK"); }
      if (m[3] === "INICIO") { st.rx = { n: +m[4], got: 0 }; return decir("MASCOTA OK"); }
      if (m[3] === "DATOS") { if (+m[4] !== st.rx.got) return decir("MASCOTA ERROR orden"); st.rx.got += atob(m[5]).length; return decir("MASCOTA OK " + st.rx.got); }
      if (m[3] === "FIN") { if (st.rx.got !== st.rx.n) return decir("MASCOTA ERROR no llego completo"); dest[m[2]] = m[4]; return decir("MASCOTA OK"); }
    }
    if (l === "MASCOTA FIESTAS FABRICA") { st.fiestas = null; return decir("MASCOTA OK"); }
    if (l === "MASCOTA FIESTAS") return decir("MASCOTA FIESTAS " + (st.fiestas || "RklFUwEAAAAAAA=="));
    if ((m = l.match(/^MASCOTA FIESTAS (\\S+)$/))) { const u = atob(m[1]); if (u.slice(0,4) !== "FIES") return decir("MASCOTA ERROR celebraciones no validas"); st.fiestas = m[1]; return decir("MASCOTA OK"); }
    if (l === "MASCOTA RESPALDO") return decir("MASCOTA RESPALDO 13 QUJDREVGRw==");
    if (l.startsWith("MASCOTA RESTAURAR ")) { decir("MASCOTA ESPERANDO"); setTimeout(() => decir("MASCOTA RESTAURADA"), 300); return; }
    if (l === "MASCOTA FOTOS") return decir("MASCOTA FOTOS 2 7:" + qoi.length + " 5:" + qoi.length);
    if ((m = l.match(/^MASCOTA FOTO (\\d+)$/))) { decir("algo de diagnostico I (123) main: hola"); decir("MASCOTA FOTO " + m[1] + " " + qoi.length + " 0 " + b64(qoi.subarray(0, 10))); decir("MASCOTA FOTO " + m[1] + " " + qoi.length + " 10 " + b64(qoi.subarray(10))); decir("MASCOTA FOTO " + m[1] + " FIN"); return; }
    if ((m = l.match(/^MASCOTA BORRAR FOTO (\\d+)$/))) return decir("MASCOTA OK");
    decir("MASCOTA ERROR comando desconocido");
  }
  let resto = ""; const dec = new TextDecoder();
  const writable = new WritableStream({ write(ch) { resto += dec.decode(ch); let k; while ((k = resto.indexOf("\\n")) >= 0) { const l = resto.slice(0, k); resto = resto.slice(k + 1); responder(l); } } });
  const port = { readable, writable, open: async () => {}, close: async () => {}, setSignals: async () => {} };
  Object.defineProperty(navigator, "serial", { value: Object.assign(new EventTarget(), { requestPort: async () => port }) });
})();`;
(async () => {
  const b = await chromium.launch({ args: ['--autoplay-policy=no-user-gesture-required'] });
  const ctx = await b.newContext({ viewport: { width: 1360, height: 900 }, acceptDownloads: true });
  await ctx.route(/fonts\.(googleapis|gstatic)/, r => r.abort());
  const pg = await ctx.newPage();
  const errores = []; pg.on('pageerror', e => errores.push(e.message)); pg.on('console', m => m.type() === 'error' && !/ERR_FAILED/.test(m.text()) && errores.push(m.text()));
  await pg.addInitScript(FAKE);
  await pg.goto('file://' + path.resolve(__dirname, '../editor.html'));
  const ok = (c, t) => { console.log((c ? 'OK   ' : 'FALLA ') + t); if (!c) process.exitCode = 1; };
  await pg.screenshot({ path: salida + '/ed_inicio_sin.png' });
  await pg.click('#conectar');
  await pg.waitForSelector('#estado.on', { timeout: 5000 });
  ok((await pg.textContent('#estado-t')).includes('Kiwi'), 'conecta y saluda');
  { const h = await pg.evaluate(() => __reloj.hora); ok(h && Math.abs(h[0] - Date.now() / 1000) < 30 && Number.isInteger(h[1]), 'al conectar le pasa la hora del computador'); }
  await pg.screenshot({ path: salida + '/ed_inicio.png' });
  // íconos: pinta y envía
  await pg.click('nav [data-s="iconos"]');
  await pg.click('.ico[aria-label="Pelota"]').catch(async () => { await pg.locator('.ico').nth(1).click(); });
  const box = await pg.locator('#lienzo').boundingBox();
  await pg.mouse.move(box.x + 10, box.y + 10); await pg.mouse.down(); await pg.mouse.move(box.x + box.width / 2, box.y + 10, { steps: 6 }); await pg.mouse.up();
  await pg.waitForTimeout(400);
  ok(await pg.locator('.ico .marca:not(.reloj)').count() === 1, 'marca el ícono cambiado');
  await pg.click('#i-enviar'); await pg.waitForTimeout(300);
  ok(Object.keys(await pg.evaluate(() => __reloj.iconos)).length === 1, 'el reloj recibió el ícono');
  await pg.click('#contorno'); await pg.click('#deshacer');
  await pg.screenshot({ path: salida + '/ed_iconos.png' });
  // criaturas: dibujar, mover los ojos, enviar, nombre
  await pg.click('nav [data-s="criaturas"]'); await pg.waitForTimeout(300);
  await pg.locator('.etapa').nth(4).click();
  const cb = await pg.locator('#lienzo-c').boundingBox(), cel = cb.width / 30;
  await pg.mouse.move(cb.x + cel * 5.5, cb.y + cel * 15.5); await pg.mouse.down(); await pg.mouse.move(cb.x + cel * 24.5, cb.y + cel * 15.5, { steps: 14 }); await pg.mouse.up();
  await pg.click('[data-hc="ojos"]'); await pg.mouse.click(cb.x + cel * 9.5, cb.y + cel * 14.5);
  await pg.click('[data-cara="triste"]'); await pg.waitForTimeout(600);
  ok(await pg.locator('.etapa .marca:not(.reloj)').count() === 1, 'marca la criatura dibujada');
  await pg.click('#k-enviar'); await pg.waitForTimeout(300);
  ok(JSON.stringify(await pg.evaluate(() => __reloj.cri)) === '{"12":1}', 'el reloj recibió la criatura (Chispa cría)');
  await pg.locator('.especie input').first().fill('Hojita'); await pg.keyboard.press('Tab'); await pg.waitForTimeout(300);
  ok((await pg.evaluate(() => __reloj.nombres))['0'] === 'Hojita', 'y el nombre de la especie');
  await pg.screenshot({ path: salida + '/ed_criaturas.png' });
  // grilla fina: punta gruesa (2x2), pintar parejo, dibujos antiguos que se agrandan y vista previa como el reloj
  const fino = await pg.evaluate(() => {
    const c = celdas(5, 15, 30, true, true).map((p) => p.join(",")).sort();
    const ico = b64aBytes(icoAlDia(bytesAb64(new Uint8Array(12 * 12 * 4)))).length;
    const cri = criAlDia({ px: bytesAb64(new Uint8Array(20 * 20 * 4).fill(255)), ojo_x: 6, ojo_y: 8, sep: 5, patitas: 1 });
    const cu = criDe(3, 2);
    return { c, ico, cri: [b64aBytes(cri.px).length, cri.ojo_x, cri.ojo_y, cri.sep], r3: renderCri(cu, {}, 1.5).length, r4: renderCri(cu, {}, 2).length,
      cuerpo: cuerpoBytes(cu).length, ver: cuerpoBytes(cu)[4], lado: CL, ilado: L };
  });
  ok(fino.c.length === 8 && fino.c.includes('4,14') && fino.c.includes('5,15') && fino.c.includes('25,14') && fino.c.includes('24,15'), 'la punta gruesa pinta 2x2 y "parejo" lo repite al otro lado');
  ok(fino.ico === 24 * 24 * 4 && fino.ilado === 24, 'los íconos son de 24x24 (y los de 12x12 se agrandan)');
  ok(fino.lado === 30 && fino.cri.join() === [3600, 9, 12, 8].join(), 'las criaturas son de 30x30 (y las de 20x20 se agrandan con los ojos en su lugar)');
  ok(fino.r3 === 900 && fino.r4 === 1600 && fino.cuerpo === 3612 && fino.ver === 2, 'vista previa a 30x30 y 40x40, formato CUER 2');
  // sonidos: genera un sonido de prueba como archivo WAV y lo sube
  await pg.click('nav [data-s="sonidos"]');
  const wav = (() => { const n = 16000, h = Buffer.alloc(44 + n * 2); h.write('RIFF', 0); h.writeUInt32LE(36 + n * 2, 4); h.write('WAVEfmt ', 8); h.writeUInt32LE(16, 16); h.writeUInt16LE(1, 20); h.writeUInt16LE(1, 22); h.writeUInt32LE(16000, 24); h.writeUInt32LE(32000, 28); h.writeUInt16LE(2, 32); h.writeUInt16LE(16, 34); h.write('data', 36); h.writeUInt32LE(n * 2, 40); for (let i = 0; i < n; i++) h.writeInt16LE(Math.round(Math.sin(i / 16000 * 2 * Math.PI * 880) * 12000), 44 + i * 2); return h; })();
  await pg.locator('#lista-sonidos .son').first().locator('input[type=file]').setInputFiles({ name: 'pio.wav', mimeType: 'audio/wav', buffer: wav });
  await pg.waitForSelector('#lista-sonidos .son.propio', { timeout: 5000 });
  await pg.locator('#lista-sonidos .son.propio [data-a="enviar"]').click();
  await pg.waitForFunction(() => Object.keys(__reloj.sonidos).length === 1, null, { timeout: 8000 });
  const crcOk = await pg.evaluate(() => Object.values(__reloj.sonidos)[0]);
  ok(/^[0-9a-f]+$/.test(crcOk), 'el sonido llegó en trozos con crc ' + crcOk);
  await pg.screenshot({ path: salida + '/ed_sonidos.png' });
  // ambiente
  await pg.click('nav [data-s="ambiente"]');
  await pg.locator('#lista-ambiente .son').nth(1).locator('input[type=file]').setInputFiles({ name: 'lluvia.wav', mimeType: 'audio/wav', buffer: wav });
  await pg.waitForSelector('#lista-ambiente .son.propio');
  await pg.screenshot({ path: salida + '/ed_ambiente.png' });
  // celebraciones
  await pg.click('nav [data-s="fiestas"]');
  const antes = await pg.locator('.cel').count();
  await pg.click('#c-agregar');
  ok(await pg.locator('.cel').count() === antes + 1, 'agrega una celebración');
  await pg.locator('.cel').last().locator('[data-k=nombre]').fill('el Día del Abuelo'); await pg.keyboard.press('Tab');
  await pg.locator('.cel').last().locator('[data-k=saludo]').fill('¡FELIZ DÍA, TATA!'); await pg.keyboard.press('Tab');
  await pg.click('#c-enviar'); await pg.waitForTimeout(300);
  const fies = await pg.evaluate(() => __reloj.fiestas);
  ok(fies && atob(fies).length === 10 + (antes + 1) * 80, 'manda la tabla FIES con ' + (antes + 1) + ' celebraciones');
  await pg.click('#c-traer'); await pg.waitForTimeout(300);
  ok(await pg.locator('.cel').count() === antes + 1, 'la trae de vuelta igual');
  await pg.screenshot({ path: salida + '/ed_fiestas.png', fullPage: true });
  // respaldos
  await pg.click('nav [data-s="respaldos"]');
  const [dl] = await Promise.all([pg.waitForEvent('download'), pg.click('#b-guardar')]);
  ok(/^kiwi-\d{4}-\d\d-\d\d\.mascota$/.test(dl.suggestedFilename()), 'baja ' + dl.suggestedFilename());
  await pg.setInputFiles('#f-respaldo', { name: 'k.mascota', mimeType: 'text/plain', buffer: Buffer.from('MASCOTA-RESPALDO 13\nQUJDREVGRw==\n') });
  await pg.waitForSelector('#b-aviso.bien', { timeout: 3000 });
  ok(true, 'trae un respaldo (confirmado en el reloj)');
  // fotos
  await pg.click('nav [data-s="fotos"]'); await pg.click('#f-ver');
  await pg.waitForFunction(() => document.querySelectorAll('.foto canvas').length === 2, null, { timeout: 5000 });
  const px = await pg.evaluate(() => Array.from(document.querySelector('.foto canvas').getContext('2d').getImageData(3, 3, 1, 1).data));
  ok(px[0] === 255 && px[1] === 0 && px[3] === 255, 'decodifica la foto QOI ' + px);
  // paquete
  const [pk] = await Promise.all([pg.waitForEvent('download'), pg.click('#paquete-guardar')]);
  ok(pk.suggestedFilename() === 'mis-cambios.mascotapack', 'guarda el paquete');
  await pg.click('nav [data-s="inicio"]'); await pg.click('#mandar-todo');
  await pg.waitForFunction(() => !document.querySelector('#mandar-todo').disabled && document.querySelector('#mandar-todo').textContent.startsWith('Mandar'), null, { timeout: 15000 });
  ok(Object.keys(await pg.evaluate(() => __reloj.amb)).length === 1, 'mandar todo incluye el sonido de fondo');
  // móvil / angosto
  await pg.setViewportSize({ width: 420, height: 860 }); await pg.click('nav [data-s="iconos"]');
  const ancho = await pg.evaluate(() => document.documentElement.scrollWidth);
  ok(ancho <= 420, 'sin scroll horizontal en angosto (' + ancho + ')');
  await pg.screenshot({ path: salida + '/ed_angosto.png' });
  // oscuro
  await pg.setViewportSize({ width: 1360, height: 900 }); await pg.emulateMedia({ colorScheme: 'dark' }); await pg.click('nav [data-s="iconos"]');
  await pg.screenshot({ path: salida + '/ed_oscuro.png' });
  ok(errores.length === 0, 'sin errores en la página ' + errores.join(' | '));
  await b.close();
})();
