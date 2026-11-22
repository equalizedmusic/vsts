// Renderiza (mgraphics -> Canvas2D) a faixa e a janela do DESENROLA a partir dos arquivos do BUILD.
// DATA=<dict.json> node shot-desenrola.js <pasta do build Desenrola-x.y.z> <saida>
const fs = require('fs'), path = require('path');
const puppeteer = require(process.env.PUPPETEER || 'puppeteer');
const B = (process.argv[2] || path.join(__dirname, '../../tmp/build-esp/Espanador-0.1.0')) + '/';
const out = process.argv[3] || require('os').tmpdir() + '/shots-des'; fs.mkdirSync(out, { recursive: true });
// partes e faixas: DATA=<json do dict desenrola_data> (o sim-desenrola grava com DUMP=arquivo); faixas a mais para encher a tela
const data = JSON.parse(fs.readFileSync(process.env.DATA, 'utf8'));
const extra = [['Kick Top', 0], ['Sub Bass', 1], ['Snare Roll', 2], ['Perc Loop', 2], ['HH Open', 3], ['Ride', 3], ['Chords', 4], ['Arp 16th', 4], ['Pluck', 4],
  ['Atmo Rain', 5], ['Vox Main', 6], ['Vox Dobra', 6], ['FX Impact', 7], ['Downlifter', 7], ['Foley', 8], ['Guitar', 4]];
const REG = data.regras;
extra.forEach(([n, g], i) => data.tracks.push({ id: 900 + i, name: n, color: [0x9a9a9a, 0x7aa7ff, 0x5fd3e8, 0xb48cff, 0xd9b38c, 0x4ee6a8][i % 6], g, auto: i % 4 ? 1 : 0, midi: i % 3 == 0 ? 1 : 0,
  clips: [{ s: 16, e: 48 }], blocos: (() => { const o = []; let pos = 0; for (const p of data.partes) { if (REG[p.p][g]) o.push({ a: pos, b: pos + p.n }); pos += p.n; } return o; })() }));
// uma faixa editada à mão (blocos soltos) para a foto mostrar
if (data.tracks[7]) data.tracks[7].blocos = [{ a: 4, b: 12 }, { a: 24, b: 40 }, { a: 76, b: 84 }], data.tracks[7].manual = 1;
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
  await tela('desenrola_strip.js', 420, 166, {}, [['lic(1); status("PRONTO · clique OPEN e depois ANALYZE")', 'desenrola-device.png']], 2);
  await tela('desenrola_view.js', 1360, 840, { desenrola_data: JSON.stringify(data) }, [
    [`lic(${JSON.stringify(LIC_OK)}); loadbang(); status('8 partes · 144 compassos · 24 faixas · monte a música na janela e clique BUILD')`, 'desenrola-janela.png'],
    ['varre(900); progresso(380, 900)', 'desenrola-build.png'],
    ['fim(); data=null; status("PRONTO · clique ANALYZE")', 'desenrola-vazio.png']]);
  console.log('ok', out, 'erros:', errs);
  await br.close();
})();
