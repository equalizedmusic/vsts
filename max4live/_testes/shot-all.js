// Renderiza (mgraphics -> Canvas2D) a faixa do device e a janela dos dois devices, a partir dos arquivos do BUILD.
// node shot-all.js <saida>
const fs = require('fs'), path = require('path');
const puppeteer = require(process.env.PUPPETEER || 'puppeteer');
const B = (process.env.M4L_BUILD || require('path').join(__dirname, '../../tmp/build-m4l')) + '/';
const out = process.argv[2] || require('os').tmpdir() + '/shots-m4l'; fs.mkdirSync(out, { recursive: true });
let seed = 11; const rnd = () => (seed = (seed * 16807) % 2147483647) / 2147483647;
const NOMES = [['01 KICK',0xff9a2e],['02 KICK SUB',0xff9a2e],['03 SNARE',0xf6d55c],['04 CLAP',0xf6d55c],['05 HH CLOSED',0xc6f24e],['06 HH OPEN',0xc6f24e],['07 PERC LOOP',0x4ee6a8],['08 TOM FILL',0x4ee6a8],['09 BASS FM',0xff6b8b],['10 BASS SUB',0xff6b8b],['11 PAD',0x7aa7ff],['12 CHORDS',0x7aa7ff],['13 ARP',0xb48cff],['14 LEAD',0xb48cff],['15 PLUCK',0xb48cff],['16 VOX LEAD',0xff8ad8],['17 VOX DOBRA',0xff8ad8],['18 VOX CHOP',0xff8ad8],['19 FX RISER',0x5fd3e8],['20 FX IMPACT',0x5fd3e8],['21 FOLEY',0xd9b38c],['22 ROOM',0x9a9a9a]];
// VASSOURA: clips com trechos vazios
const vt = []; let nEmpty = 0, clipBeats = 0, emptyBeats = 0;
NOMES.forEach(([name, color], i) => {
  const clips = []; let x = 64 + rnd() * 16;
  while (x < 560) {
    const e = Math.min(x + 60 + rnd() * 140, 576), em = [];
    let y = x + 8 + rnd() * 20;
    while (y < e - 12) { const l = 6 + rnd() * 26; if (rnd() < 0.55) { em.push({ s: y, e: Math.min(e, y + l), keep: rnd() < 0.08 ? 1 : 0 }); nEmpty++; emptyBeats += Math.min(e, y + l) - y; } y += l + 10 + rnd() * 30; }
    clips.push({ s: x, e, state: 'ok', empties: em }); clipBeats += e - x; x = e + 4 + rnd() * 30;
  }
  vt.push({ name, color, index: i, skip: i === 21 ? 1 : 0, clips });
});
const vdata = { tracks: vt, nEmpty, clipBeats, emptyBeats, maxBeat: 576, sig: 4, cfg: { threshold: -60, minlen: 1000, pre: 10, post: 100, snap: 0, mode: 0, scope: 0 } };
const vfila = nEmpty;
// RODO: dados no formato do motor
seed = 23;
const rt = [], renv = { tracks: [] }; let nClips = 0, dmin = 1e9, dmax = -1e9;
NOMES.forEach(([name, color], i) => {
  const base = -2 - rnd() * 24, clips = [], envs = []; let x = 80 + rnd() * 30;
  while (x < 560) {
    const b = Math.min(x + 24 + rnd() * 100, 590), pk = base - rnd() * 4, n = Math.max(4, Math.round(1100 * (b - x) / 510));
    let s = ''; for (let k = 0; k < n; k++) { const u = k / n, en = Math.min(1, u * 12) * Math.min(1, (1 - u) * 8), p = 0.45 + 0.55 * Math.abs(Math.sin(u * (6 + i % 5) * Math.PI)); s += String.fromCharCode(48 + Math.round(63 * Math.sqrt(en * p * (0.55 + 0.45 * rnd())))); }
    clips.push({ s: x, e: b, state: 'ok', pk, g0: 0, out: pk }); envs.push(s); x = b + 4 + rnd() * 40;
  }
  const mx = Math.max(...clips.map((c) => c.out));
  clips.forEach((c) => { c.g1 = Math.min(24, -6 - mx); c.falta = 0; c.delta = c.g1; nClips++; dmin = Math.min(dmin, c.delta); dmax = Math.max(dmax, c.delta); });
  rt.push({ name, color, index: i, skip: 0, clips }); renv.tracks.push(envs);
});
const rdata = { tracks: rt, nClips, dmin, dmax, nTeto: 0, minBeat: 80, maxBeat: 590, sig: 4, cfg: { target: -6, mode: 1, scope: 0 } };
const rfila = []; rt.forEach((t) => t.clips.forEach((c) => rfila.push(c.s))); rfila.sort((a, b) => a - b);

const LIC_NAO = JSON.stringify({ ok: 0, ocupado: 0, email: '', token: '', msg: '', ate: 0 });
const LIC_MEIO = JSON.stringify({ ok: 0, ocupado: 0, email: 'voce@email.com', token: 'ISP-7KQ2-M9XD-4TPA-H3WZ-8CNE', msg: '', ate: 0 });
const LIC_OK = JSON.stringify({ ok: 1, ocupado: 0, email: 'voce@email.com', token: '', msg: '', ate: 1768000000 });

(async () => {
  const br = await puppeteer.launch({ headless: 'new', args: ['--no-sandbox'], executablePath: 'C:/Users/rogerio.cardoso/.cache/puppeteer/chrome/win64-148.0.7778.97/chrome-win64/chrome.exe' });
  const p = await br.newPage();
  const errs = []; p.on('pageerror', (e) => errs.push(e.message));
  async function tela(arq, W, H, store, passos, nomePng, escala = 2) {
    await p.setViewport({ width: W, height: H, deviceScaleFactor: escala });
    await p.setContent(`<body style="margin:0;background:#000"><canvas id=c width=${W} height=${H}></canvas></body>`);
    await p.evaluate((src, W, H, store) => {
      const ctx = document.getElementById('c').getContext('2d');
      let font = { f: 'Arial', s: 10 };
      const mg = { init() {}, redraw() {}, relative_coords: 0, autofill: 0,
        set_source_rgba(r, g, b, a) { ctx.fillStyle = ctx.strokeStyle = `rgba(${r * 255},${g * 255},${b * 255},${a})`; },
        rectangle(x, y, w, h) { ctx.beginPath(); ctx.rect(x, y, w, h); },
        rectangle_rounded(x, y, w, h, rx) { ctx.beginPath(); ctx.roundRect(x, y, w, h, Math.max(0, Math.min(rx, w / 2, h / 2))); },
        ellipse(x, y, w, h) { ctx.beginPath(); ctx.ellipse(x + w / 2, y + h / 2, w / 2, h / 2, 0, 0, 7); },
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
  // faixas do device
  for (const [dev, pasta] of [['rodo', 'Rodo-0.2.0'], ['vassoura', 'Vassoura-0.3.0']]) {
    await tela(`${pasta}/${dev}_strip.js`, 420, 166, {}, [
      ['lic(1); status("PRONTO · clique ANALYZE")', `${dev}-device.png`],
      ['lic(0); status("ATIVE A LICENÇA · clique OPEN")', `${dev}-device-sem-licenca.png`],
      ['lic(1); hoverBt=1; status("APPLY FEITO · 139 clips ajustados")', `${dev}-device-hover.png`]]);
  }
  // RODO janela
  await tela('Rodo-0.2.0/rodo_view.js', 1360, 840, { rodo_data: JSON.stringify(rdata), rodo_env: JSON.stringify(renv) }, [
    [`loadbang(); lic(${JSON.stringify(LIC_NAO)}); status("ATIVE A LICENÇA · clique OPEN")`, 'rodo-ativar.png'],
    [`lic(${JSON.stringify(LIC_MEIO)})`, 'rodo-ativar-preenchido.png'],
    [`lic(${JSON.stringify(LIC_OK)}); status("${nClips} clips · TARGET -6.0 dB · TRACK")`, 'rodo-janela.png'],
    [`varre(${rfila.length}, 80, 590); progresso(${Math.floor(rfila.length * 0.55)}, ${rfila.length}, ${rfila[Math.floor(rfila.length * 0.55)]}); for (var i=0;i<60;i++) passoAnim()`, 'rodo-applying.png'],
    [`licInfo=1; anim=null`, 'rodo-licenca-info.png']], 1);
  // VASSOURA janela
  await tela('Vassoura-0.3.0/vassoura_view.js', 1360, 840, { vassoura_data: JSON.stringify(vdata) }, [
    [`loadbang(); lic(${JSON.stringify(LIC_NAO)}); status("ATIVE A LICENÇA · clique OPEN")`, 'vassoura-ativar.png'],
    [`lic(${JSON.stringify(LIC_OK)}); status("${nEmpty} vazios · MUTE")`, 'vassoura-janela.png'],
    [`varre(${vfila}); progresso(${Math.floor(vfila * 0.5)}, ${vfila}); for (var i=0;i<45;i++) passoAnim()`, 'vassoura-applying.png'],
    [`nova("0.3.1"); anim=null`, 'vassoura-nova-versao.png']], 1);
  console.log('ok', out, 'erros:', errs);
  await br.close();
})();
