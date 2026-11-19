// Renderiza (mgraphics -> Canvas2D) a faixa e a janela do ESPANADOR a partir dos arquivos do BUILD.
// node shot-espanador.js <pasta do build Espanador-x.y.z> <saida>
const fs = require('fs'), path = require('path');
const puppeteer = require(process.env.PUPPETEER || 'puppeteer');
const B = (process.argv[2] || path.join(__dirname, '../../tmp/build-esp/Espanador-0.1.0')) + '/';
const out = process.argv[3] || require('os').tmpdir() + '/shots-esp'; fs.mkdirSync(out, { recursive: true });
let seed = 7; const rnd = () => (seed = (seed * 16807) % 2147483647) / 2147483647;
const T = [['01 KICK', -1, 0], ['02 KICK SUB', -1, 0], ['03 SNARE', -1, 2], ['04 CLAP', -1, 2], ['05 HH CLOSED', -1, 3], ['06 HH OPEN', -1, 3],
  ['PERCS', 1], ['07 CONGA', -1, 2, 1], ['08 SHAKER', -1, 3, 1], ['09 BASS FM', -1, 1], ['10 BASS SUB', -1, 1], ['11 PAD', -1, 5],
  ['12 CHORDS', -1, 4], ['13 ARP', -1, 4], ['14 LEAD ACID', -1, 4], ['15 VOX LEAD', -1, 6], ['16 VOX DOBRA', -1, 6], ['17 VOX CHOP', -1, -1],
  ['18 FX RISER', -1, 7], ['19 FX IMPACT', -1, 7], ['20 FOLEY', -1, -1], ['21 ROOM', -1, -1], ['22 GTR LIMPA', -1, -1], ['23 GTR SUJA', -1, -1]];
const cores = [0x9a9a9a, 0x7aa7ff, 0x5fd3e8, 0xb48cff, 0xd9b38c, 0x4ee6a8];
const tracks = T.map(([name, fold, g, member], i) => {
  const clips = []; let x = 64 + rnd() * 16;
  if (fold !== 1) while (x < 560) { const e = Math.min(x + 40 + rnd() * 140, 576); clips.push([x, e]); x = e + 4 + rnd() * 40; }
  return { id: 100 + i, index: i, name, color: cores[i % cores.length], fold: fold === 1 ? 1 : 0, member: member ? 1 : 0, g: g == null ? -1 : g,
    auto: g >= 0 && i % 3 === 0 ? 1 : 0, bus: -1, out: member ? 'PERCS' : 'Master', clips };
});
tracks[16].sel = 1; tracks[17].sel = 1; tracks[21].sel = 1; tracks[22].sel = 1;
const data = { versao: '0.1.0', sig: 4, minBeat: 64, maxBeat: 576, tracks, cfg: { prefix: 0, bus: 1, auto: 1, clips: 1 } };
const LIC_OK = JSON.stringify({ ok: 1, ocupado: 0, email: 'teste', token: '', msg: '', ate: 0 });

(async () => {
  const br = await puppeteer.launch({ headless: 'new', args: ['--no-sandbox'], executablePath: process.env.CHROME });
  const p = await br.newPage();
  const errs = []; p.on('pageerror', (e) => errs.push(e.message));
  async function tela(arq, W, H, store, passos, escala = 1) {
    await p.setViewport({ width: W, height: H, deviceScaleFactor: escala });
    await p.setContent(`<body style="margin:0;background:#000"><canvas id=c width=${W} height=${H}></canvas></body>`);
    await p.evaluate((src, W, H, store) => {
      const ctx = document.getElementById('c').getContext('2d');
      let font = { f: 'Arial', s: 10 };
      const mg = { init() {}, redraw() {}, relative_coords: 0, autofill: 0,
        set_source_rgba(r, g, b, a) { ctx.fillStyle = ctx.strokeStyle = `rgba(${r * 255},${g * 255},${b * 255},${a})`; },
        rectangle(x, y, w, h) { ctx.beginPath(); ctx.rect(x, y, w, h); },
        rectangle_rounded(x, y, w, h, rx) { ctx.beginPath(); ctx.roundRect(x, y, w, h, Math.max(0, Math.min(rx, w / 2, h / 2))); },
        ellipse(x, y, w, h) { ctx.beginPath(); ctx.ellipse(x + w / 2, y + h / 2, Math.abs(w / 2), Math.abs(h / 2), 0, 0, 7); },
        fill() { ctx.fill(); ctx.beginPath(); ctx._open = 0; }, stroke() { ctx.stroke(); ctx.beginPath(); ctx._open = 0; },
        move_to(x, y) { if (!ctx._open) { ctx.beginPath(); ctx._open = 1; } ctx.moveTo(x, y); this._x = x; this._y = y; }, line_to(x, y) { ctx.lineTo(x, y); },
        set_line_width(w) { ctx.lineWidth = w; }, set_line_cap(c) { ctx.lineCap = c; }, select_font_face(f) { font.f = f; }, set_font_size(s) { font.s = s; },
        save() { ctx.save(); }, restore() { ctx.restore(); }, translate(x, y) { ctx.translate(x, y); }, rotate(a) { ctx.rotate(a); },
        show_text(s) { ctx.font = (font.f.includes('Bold') ? 'bold ' : '') + font.s + 'px Arial'; ctx.fillText(s, this._x, this._y); ctx.beginPath(); ctx._open = 0; },
        text_measure(s) { ctx.font = (font.f.includes('Bold') ? 'bold ' : '') + font.s + 'px Arial'; return [ctx.measureText(s).width, font.s]; } };
      function Dict(n) { this.stringify = () => store[n] || '{}'; this.parse = (s) => (store[n] = s); }
      function Task(fn) { this.fn = fn; this.repeat = () => {}; this.cancel = () => {}; this.schedule = () => {}; }
      const box = { rect: [0, 0, W, H] };
      window.ev = new Function('mgraphics', 'Dict', 'box', 'Task', 'arrayfromargs', 'outlet', src + ';return function(c){return eval(c)}')(mg, Dict, box, Task, (a) => Array.from(a), () => {});
      window.pinta = () => { ctx.setTransform(1, 0, 0, 1, 0, 0); ctx.clearRect(0, 0, W, H); window.ev('paint()'); };
    }, fs.readFileSync(B + arq, 'utf8'), W, H, store);
    for (const [cmd, png] of passos) {
      await p.evaluate((c) => { window.ev(c); window.pinta(); }, cmd);
      if (png) await p.screenshot({ path: path.join(out, png) });
    }
  }
  await tela('espanador_strip.js', 420, 166, {}, [['lic(1); status("PRONTO · clique OPEN e depois SCAN")', 'espanador-device.png']], 2);
  await tela('espanador_view.js', 1360, 840, { espanador_data: JSON.stringify(data) }, [
    [`lic(${JSON.stringify(LIC_OK)}); loadbang(); data.tracks[16].sel=1;data.tracks[17].sel=1;data.tracks[21].sel=1;data.tracks[22].sel=1; status("24 faixas · 17 num grupo (6 pelo nome, confira) · nada muda até o APPLY")`, 'espanador-janela.png'],
    [`varre(17); progresso(9, 17, 12); for (var i=0;i<40;i++) passoAnim()`, 'espanador-applying.png']]);
  console.log('ok', out, 'erros:', errs);
  await br.close();
})();
