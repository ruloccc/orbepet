// Arnés para manejar el simulador (mascota.wasm) desde node: tocar, avanzar, sacar fotos.
const fs = require('fs'), path = require('path'), zlib = require('zlib');
async function crear({ seed = 777, ahora = Date.UTC(2026, 9, 28, 15, 0) / 1000 } = {}) {
  const b = fs.readFileSync(path.join(__dirname, '../mascota.wasm'));
  const imp = { wasi_snapshot_preview1: { fd_close: () => 0, fd_seek: () => 0, fd_write: () => 0 } };
  const { instance } = await WebAssembly.instantiate(b, imp);
  const x = instance.exports; x._initialize && x._initialize();
  x.sim_init(ahora, seed, 0);
  x.sim_pocket && x.sim_pocket(0); /* las pruebas tocan la pantalla aunque se haya apagado */
  const mem = () => new Uint8Array(x.memory.buffer);
  const str = (p) => { const m = mem(); let e = p; while (m[e]) e++; return Buffer.from(m.slice(p, e)).toString(); };
  const s = {
    x, str,
    frames(n = 1, dt = 16) { for (let i = 0; i < n; i++) x.sim_frame(dt); },
    tap(px, py, veces = 1) {
      for (let v = 0; v < veces; v++) { x.sim_touch(px, py, 1); s.frames(4); x.sim_touch(px, py, 0); s.frames(veces > 1 ? 6 : 20); }
      s.frames(10);
    },
    log() { const l = Buffer.from(mem().slice(x.sim_log_ptr(), x.sim_log_ptr() + x.sim_log_len())).toString(); x.sim_log_clear(); return l; },
    horas(h, paso = 60) { x.sim_set_speed(paso); const n = Math.round(h * 3600 / paso / 0.1); for (let i = 0; i < n; i++) x.sim_frame(100); x.sim_set_speed(1); },
    /* salta la bienvenida: edad, nombre, nace, y contesta la fecha y la hora */
    listo(edad = 30) {
      x.sim_quick(edad); s.frames(100); s.tap(232, 366); s.frames(100);
      for (let k = 0; k < 4; k++) { s.tap(272, 366); s.frames(100); if (s.log().includes('pusiste la fecha')) break; }
      s.frames(200); s.log();
    },
    png(archivo) {
      const fb = new Uint32Array(x.memory.buffer, x.sim_fb(), 466 * 466);
      const raw = Buffer.alloc(466 * (466 * 3 + 1));
      for (let y = 0; y < 466; y++) { raw[y * (466 * 3 + 1)] = 0; for (let k = 0; k < 466; k++) { const v = fb[y * 466 + k], o = y * (466 * 3 + 1) + 1 + k * 3; raw[o] = v >> 16 & 255; raw[o + 1] = v >> 8 & 255; raw[o + 2] = v & 255; } }
      const crc = (buf) => { let c = ~0; for (const b of buf) { c ^= b; for (let k = 0; k < 8; k++) c = c & 1 ? 0xEDB88320 ^ (c >>> 1) : c >>> 1; } return (~c) >>> 0; };
      const chunk = (t, d) => { const l = Buffer.alloc(4); l.writeUInt32BE(d.length); const td = Buffer.concat([Buffer.from(t), d]); const c = Buffer.alloc(4); c.writeUInt32BE(crc(td)); return Buffer.concat([l, td, c]); };
      const ih = Buffer.alloc(13); ih.writeUInt32BE(466, 0); ih.writeUInt32BE(466, 4); ih[8] = 8; ih[9] = 2;
      fs.writeFileSync(archivo, Buffer.concat([Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]), chunk('IHDR', ih), chunk('IDAT', zlib.deflateSync(raw)), chunk('IEND', Buffer.alloc(0))]));
    },
  };
  return s;
}
module.exports = { crear };
