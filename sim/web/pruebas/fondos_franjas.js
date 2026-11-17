// (105) El fondo armado por franjas (cuando no hay memoria para el lienzo de 876 KB) tiene que salir
// EXACTAMENTE igual que el fondo entero: si no, la casita del prado u otro adorno se vería distinto.
// Uso: node sim/web/pruebas/fondos_franjas.js
const { crear } = require('./arnes.js');

(async () => {
  const s = await crear({});
  s.listo(30);
  const x = s.x;
  let fallas = 0, casos = 0;
  const ALTOS = [0, 78, 36, 12, 6]; // 0 = como en la placa (el alto que quepa, 156 primero)
  const DECOR_TODO = 0x1F; // flores, hongos, farol, banco y la casita-tocón
  for (const escena of [0, 5]) {
    for (const periodo of [0, 1, 2]) {           // día, atardecer, noche
      for (const estacion of [0, 1, 2, 3]) {     // primavera, verano, otoño, invierno
        for (const nublado of [0, 1, 2]) {       // despejado, gris, helada
          for (const decor of escena === 0 ? [DECOR_TODO, 1 << 4, 0] : [1, 0]) {
            for (const sendas of escena === 0 ? [0, 0x7FFF] : [0]) {
              if (escena === 0 && decor === 0 && sendas === 0) continue; // sin adornos no hace falta el lienzo
              for (const alto of ALTOS) {
                casos++;
                const malos = x.sim_fondo_cmp2 ? x.sim_fondo_cmp2(escena, periodo, estacion, nublado, decor, sendas, alto)
                                               : x.sim_fondo_cmp(escena, periodo, estacion, nublado, decor, sendas);
                if (malos !== 0) {
                  fallas++;
                  console.log(`DISTINTO: escena ${escena} hora ${periodo} estación ${estacion} clima ${nublado} adornos 0x${decor.toString(16)} sendas 0x${sendas.toString(16)} franjas ${alto || 'auto'} -> ${malos}`);
                }
              }
            }
          }
        }
      }
    }
  }
  console.log(`fondos por franjas: ${casos} casos, ${fallas} distintos`);
  process.exit(fallas ? 1 : 0);
})();
