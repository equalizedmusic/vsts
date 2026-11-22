// Simula o Live para testar o DESENROLA (ANALYZE + BUILD + UNDO) no Node.
// set MOTOR=<build>/desenrola.js & set REF_F32=<música de teste f32 estéreo 44,1 kHz> & node sim-desenrola.js [warp]
// A música de teste sai de gera-ref-teste.py (gabarito 0.2: INTRO 16 · DROP 32 · BREAK 16 · BUILDUP 8 · DROP 32 · BREAK 8 · DROP 16 · FINAL 16;
// o GROOVE entre dois DROPs vira BREAK).
// Live falso: arranjo com sobrescrita (cópia por cima corta/divide o que estava lá, como no Live real),
// clip em loop só encurta desligando o loop (o pior caso para o aparar).
const fs = require('fs'), vm = require('vm'), path = require('path'), os = require('os');
const WARP = process.argv[2] === 'warp';
const TEMPO = 128;
let nextId = 10;
const objs = {};
const mkClip = (o) => {
  const c = Object.assign({ kind: 'clip', looping: 1, warping: 1, audio: 1, color: 0x445566, name: 'c', file: '', sr: 44100 }, o, { id: nextId++ });
  if (c.sm == null) { c.sm = 0; c.em = c.e - c.s; c.ls = 0; c.le = c.e - c.s; }
  objs[c.id] = c; return c;
};
const mkTrack = (name, o, clips) => {
  const t = Object.assign({ kind: 'track', id: nextId++, name, color: 0x808080, is_foldable: 0, has_midi_input: 0 }, o || {});
  t.clips = (clips || []).map((c) => mkClip(Object.assign({ audio: t.has_midi_input ? 0 : 1 }, c))); objs[t.id] = t; return t;
};
// loop de 8 compassos (32 beats) em 16..48
const L0 = 16, L1 = 48;
let tracks = [
  mkTrack('Kick', {}, [{ s: L0, e: L1 }]),
  mkTrack('Bass FM', { has_midi_input: 1 }, [{ s: L0, e: L0 + 16 }, { s: L0 + 16, e: L1 }]),
  mkTrack('Hats', {}, [0, 4, 8, 12, 16, 20, 24, 28].map((x) => ({ s: L0 + x, e: L0 + x + 4 }))),
  mkTrack('Clap', {}, [{ s: L0, e: L1, looping: 0 }]),
  mkTrack('Pad Long', {}, [{ s: L0, e: L1 }]),
  mkTrack('Lead Acid', { has_midi_input: 1 }, [{ s: L0, e: L1 }]),
  mkTrack('Riser', {}, [{ s: L0 + 16, e: L1 }]),
  mkTrack('Vox Chop', {}, [{ s: L0 + 8, e: L0 + 24, looping: 0, warping: 0, sm: 1.0, em: 1.0 + 16 * 60 / TEMPO, ls: 1.0, le: 1.0 + 16 * 60 / TEMPO }]),
  mkTrack('Coisa', {}, [{ s: L0 - 4, e: L0 + 8 }]),       // cruza a borda: fica de fora
  mkTrack('Drums', { is_foldable: 1 }),
  mkTrack('Sem clip no loop', {}, [{ s: 200, e: 232 }]),
  mkTrack('REF', {}, [{ s: 400, e: 400 + 600, looping: 0, warping: WARP ? 1 : 0, file: 'C:/musicas/ref teste.wav', name: 'House Teste' }]),
];
const refClip = tracks[tracks.length - 1].clips[0];
const LEAD = +(process.env.LEAD || 2.2), BPM_REF = 124;
if (WARP) refClip.wm = [{ sample_time: LEAD, beat_time: 0 }, { sample_time: LEAD + 400 * 60 / BPM_REF, beat_time: 400 }];
const cues = [];
const DEV_TRACK = () => tracks[0];   // o DESENROLA está na 1ª faixa (Kick)
let songTime = 0;
const byId = (id) => objs[id];

function poe(t, nova) {   // cópia por cima: corta/divide o que estava lá
  const a = nova.s, b = nova.e, fica = [];
  for (const c of t.clips) {
    if (c.e <= a + 1e-9 || c.s >= b - 1e-9) { fica.push(c); continue; }
    if (c.s < a) {
      if (c.e > b) fica.push(mkClip(Object.assign({}, c, { s: b, e: c.e })));
      c.e = a; fica.push(c); continue;
    }
    if (c.e > b) { c.s = b; fica.push(c); continue; }
    delete objs[c.id];
  }
  fica.push(nova);
  fica.sort((x, y) => x.s - y.s);
  t.clips = fica;
}
function resolve(p) {
  const w = p.split(' ');
  if (w[0] === 'id') return byId(+w[1]) || { kind: 'none', id: 0 };
  if (p === 'live_set') return { kind: 'set' };
  if (p === 'live_set view selected_track') return tracks.find((t) => t.name === 'Kick');
  if (p === 'this_device canonical_parent') return DEV_TRACK();
  if (w[1] === 'tracks' && w.length === 3) return tracks[+w[2]];
  throw new Error('path ' + p);
}
class LiveAPI {
  constructor(p) { this.r = resolve(p); }
  goto(p) { this.r = resolve(p); }
  get id() { return this.r && this.r.id ? this.r.id : 0; }
  getcount(k) {
    if (this.r.kind === 'set' && k === 'tracks') return tracks.length;
    if (this.r.kind === 'track' && k === 'arrangement_clips') return this.r.clips.length;
    throw new Error('getcount ' + k);
  }
  call(fn, ...a) {
    if (this.r.kind === 'set') {
      if (fn === 'set_or_delete_cue') {
        const k = cues.findIndex((c) => Math.abs(c.time - songTime) < 1e-6);
        if (k >= 0) { delete objs[cues[k].id]; cues.splice(k, 1); } else { const c = { kind: 'cue', id: nextId++, time: songTime, name: '' }; objs[c.id] = c; cues.push(c); }
        return;
      }
      if (fn === 'create_midi_track') { const t = mkTrack('MIDI', { has_midi_input: 1 }); tracks.splice(a[0], 0, t); return; }
      if (fn === 'delete_track') { const t = tracks[a[0]]; if (!t) throw new Error('delete_track sem faixa ' + a[0]); t.clips.forEach((c) => delete objs[c.id]); delete objs[t.id]; tracks.splice(a[0], 1); return; }
      throw new Error('call set ' + fn);
    }
    const t = this.r;
    if (fn === 'create_midi_clip') { if (!t.has_midi_input) throw new Error('create_midi_clip em faixa de áudio'); poe(t, mkClip({ s: a[0], e: a[0] + a[1], audio: 0, name: '' })); return; }
    if (a[0] !== 'id') throw new Error('call sem "id": ' + a.join(' '));
    const c = byId(a[1]);
    if (!c || c.kind !== 'clip') throw new Error('clip inválido ' + a[1]);
    if (fn === 'duplicate_clip_to_arrangement') { const len = c.e - c.s; poe(t, mkClip(Object.assign({}, c, { s: a[2], e: a[2] + len }))); return; }
    if (fn === 'delete_clip') { t.clips = t.clips.filter((x) => x.id !== c.id); delete objs[c.id]; return; }
    throw new Error('call ' + fn);
  }
  get(k) {
    const r = this.r;
    if (r.kind === 'set') {
      const m = { loop_start: L0, loop_length: L1 - L0, signature_numerator: 4, signature_denominator: 4, tempo: TEMPO, current_song_time: songTime };
      if (k in m) return [m[k]];
      if (k === 'cue_points') return cues.flatMap((c) => ['id', c.id]);
      throw new Error('get set ' + k);
    }
    if (r.kind === 'cue') { if (k === 'time') return [r.time]; if (k === 'name') return [r.name]; }
    if (r.kind === 'track') {
      if (k === 'arrangement_clips') return r.clips.flatMap((c) => ['id', c.id]);
      const m = { name: r.name, color: r.color, is_foldable: r.is_foldable, has_midi_input: r.has_midi_input };
      if (k in m) return [m[k]];
      throw new Error('get track ' + k);
    }
    if (r.kind === 'clip') {
      const m = { start_time: r.s, end_time: r.e, looping: r.looping, warping: r.warping, is_audio_clip: r.audio, start_marker: r.sm, end_marker: r.em,
        loop_start: r.ls, loop_end: r.le, file_path: r.file, sample_rate: r.sr, name: r.name };
      if (k in m) return [m[k]];
      if (k === 'warp_markers') return [JSON.stringify({ warp_markers: r.wm || [] })];
      throw new Error('get clip ' + k);
    }
    throw new Error('get ' + r.kind + ' ' + k);
  }
  set(k, v) {
    const r = this.r;
    if (r.kind === 'set') { if (k === 'current_song_time') { songTime = v; return; } throw new Error('set set ' + k); }
    if (r.kind === 'cue') { if (k === 'name') { r.name = String(v); return; } }
    if (r.kind === 'track') { if (k === 'name') { r.name = String(v); return; } if (k === 'color') { r.color = v; return; } throw new Error('set track ' + k); }
    if (r.kind === 'clip') {
      if (k === 'name') { r.name = String(v); return; }
      if (k === 'color') { r.color = v; return; }
      const k2 = { looping: 'looping', end_marker: 'em', loop_end: 'le', start_marker: 'sm', loop_start: 'ls' }[k];
      if (!k2) throw new Error('set clip ' + k);
      r[k2] = v;
      // arranjo: só clip SEM loop muda de tamanho pelos markers (warp = beats, sem warp = segundos)
      if (!r.looping && (k === 'end_marker' || k === 'looping')) r.e = r.s + (r.em - r.sm) * (r.audio && !r.warping ? TEMPO / 60 : 1);
      return;
    }
    throw new Error('set ' + r.kind + ' ' + k);
  }
}
// buffer~ falso: lê a música de teste (f32 estéreo)
const raw = fs.readFileSync(process.env.REF_F32);
const st = new Float32Array(raw.buffer, raw.byteOffset, raw.length / 4);
class Buffer {
  framecount() { return st.length / 2; }
  channelcount() { return 2; }
  peek(ch, f, n) { const o = new Array(n); for (let i = 0; i < n; i++) o[i] = st[2 * (f + i) + ch - 1]; return o; }
}
const LOGDIR = path.join(os.tmpdir(), 'desenrola-sim'); fs.rmSync(LOGDIR, { recursive: true, force: true }); fs.mkdirSync(LOGDIR, { recursive: true });
class File {
  constructor(p, mode) { this.p = path.join(LOGDIR, path.basename(p)); if (mode === 'write') fs.writeFileSync(this.p, ''); this.isopen = true; this.position = 0; }
  get eof() { return fs.existsSync(this.p) ? fs.statSync(this.p).size : 0; } set eof(v) { fs.truncateSync(this.p, v); }
  writestring(x) { fs.appendFileSync(this.p, x); } readstring() { return ''; } close() {}
}
const dicts = {};
let ctx;
ctx = {
  LiveAPI, File, Buffer,
  Dict: class { constructor(n) { this.n = n; } stringify() { return dicts[this.n] || '{}'; } parse(s) { dicts[this.n] = s; } },
  Task: class { constructor(fn) { this.fn = fn; this.interval = 0; } schedule(ms) { this.h = setTimeout(this.fn, ms > 1000 ? ms : 0); }
    repeat() { this.h = setInterval(this.fn, this.interval); } cancel() { clearTimeout(this.h); clearInterval(this.h); } },
  outlet: (n, ...a) => {
    if (n === 0 && a[0] === 'replace') { if (a[1] !== refClip.file) throw new Error('arquivo errado ' + a[1]); setTimeout(() => ctx.loaded(), 5); }
    if (n === 1 && process.env.VERBOSO) console.log('STATUS:', a[1]);
    if (n === 2 && a[0] === 'status') ctx._st = a[1];
  },
  post: (s) => { if (process.env.VERBOSO || /erro|NÃO|não/i.test(s)) process.stdout.write(s); },
  patcher: { filepath: 'C:/Users/teste/Documents/Ableton/User Library/Presets/Desenrola-0.1.0/Desenrola.amxd' },
  max: { os: 'windows', version: '9.0.5', launchbrowser: () => {} }, arrayfromargs: (a) => Array.prototype.slice.call(a),
  BigInt, escape, unescape, encodeURIComponent, decodeURIComponent, parseInt, XMLHttpRequest: class {},
  Float32Array, Math, JSON, Date, Number, String, parseFloat, isNaN, Array, Infinity, isFinite,
};
vm.createContext(ctx);
vm.runInContext(fs.readFileSync(process.env.MOTOR, 'utf8'), ctx);
const esperaLivre = (fn) => { const h = setInterval(() => { if (!vm.runInContext('busy', ctx)) { clearInterval(h); setTimeout(fn, 50); } }, 20); };
let erros = 0;
const confere = (ok, txt) => { console.log((ok ? '  ok   ' : '  ERRO ') + txt); if (!ok) erros++; };
const tr = (n) => tracks.find((t) => t.name === n);
const intervalos = (t, a, b) => t.clips.filter((c) => c.s < b - 1e-6 && c.e > a + 1e-6).map((c) => [Math.max(a, c.s), Math.min(b, c.e)]);
const junta = (iv) => {
  iv = iv.slice().sort((x, y) => x[0] - y[0]); const o = [];
  for (const v of iv) { const u = o[o.length - 1]; if (u && v[0] <= u[1] + 1e-6) u[1] = Math.max(u[1], v[1]); else o.push(v.slice()); }
  return o;
};
const fotoLoop = () => JSON.stringify(tracks.filter((t) => !/^(REF|ESTRUTURA)$/.test(t.name)).map((t) => t.clips.filter((c) => c.s < L1 + 1).map((c) => [c.s, c.e])));
const antes = fotoLoop();

ctx.init();
const t0 = Date.now();
ctx.analyze();
const regraBlocos = (d, g, partes) => { const o = []; let pos = 0; for (const p of partes) { if (d.regras[p.p][g]) o.push({ a: pos, b: pos + p.n }); pos += p.n; } return o; };
esperaLivre(() => {
  const d = JSON.parse(dicts.desenrola_data || '{}');
  console.log('ANALYZE (' + (WARP ? 'warp' : 'sem warp') + ', ' + (Date.now() - t0) + ' ms): ' + ctx._st);
  confere(d.ref && Math.abs(d.ref.bpm - BPM_REF) < 0.05, 'andamento da referência ' + (d.ref && d.ref.bpm) + ' BPM');
  const partes = (d.partes || []).map((p) => p.p + ' ' + p.n).join(' · ');
  console.log('  partes: ' + partes);
  confere(partes === 'INTRO 16 · DROP 32 · BREAK 16 · BUILDUP 8 · DROP 32 · BREAK 8 · DROP 16 · FINAL 16', 'partes = gabarito da música de teste (nomes 0.2)');
  confere(d.partes.every((p, i) => p.id === i + 1), 'cada parte com id');
  if (!d.tracks) { console.log('sem faixas'); process.exit(1); }
  console.log('  faixas: ' + d.tracks.map((t) => t.name + ':' + d.grupos[t.g]).join(' '));
  confere(d.tracks.length === 8 && !d.tracks.some((t) => /REF|Coisa|Drums|Sem clip/.test(t.name)), 'faixas do loop: 8 (sem REF, sem a que cruza a borda, sem grupo, sem a sem clip no loop)');
  confere(d.tracks.every((t) => JSON.stringify(t.blocos) === JSON.stringify(regraBlocos(d, t.g, d.partes))), 'blocos de cada faixa = padrão do grupo, um por parte');
  confere(d.fora === 1, 'clip que cruza a borda do loop contado como fora');
  confere(d.inicio === 232 + 16, 'rascunho começa depois de tudo nas faixas do loop + 4 compassos (beat ' + d.inicio + ')');
  if (process.env.DUMP) fs.writeFileSync(process.env.DUMP, dicts.desenrola_data);
  // janela: INTRO com 12 (não fecha o loop de 8); Pad Long também nos DROPs; Vox Chop num bloco solto no meio da INTRO;
  // Kick sem o 2º DROP; Lead Acid num bloco só que atravessa DROP 1 + BREAK
  d.partes[0].n = 12;
  const ini = []; let acc = 0; for (const p of d.partes) { ini.push(acc); acc += p.n; }
  for (const t of d.tracks) t.blocos = regraBlocos(d, t.g, d.partes);
  const T = (n) => d.tracks.find((t) => t.name === n);
  d.partes.forEach((p, i) => { if (p.p === 'DROP') T('Pad Long').blocos.push({ a: ini[i], b: ini[i] + p.n }); });
  T('Pad Long').blocos.sort((x, y) => x.a - y.a);
  T('Vox Chop').blocos = [{ a: 4, b: 10 }];
  T('Kick').blocos = T('Kick').blocos.filter((b) => b.a !== ini[4]);
  T('Lead Acid').blocos = [{ a: ini[1], b: ini[3] }];
  T('Pad Long').manual = 1; T('Vox Chop').manual = 1; T('Kick').manual = 1; T('Lead Acid').manual = 1;
  dicts.desenrola_data = JSON.stringify(d);
  ctx.marcas();
  const t1 = Date.now();
  ctx.build();
  esperaLivre(() => {
    console.log('BUILD (' + (Date.now() - t1) + ' ms): ' + ctx._st);
    const d2 = JSON.parse(dicts.desenrola_data);
    const bar = 4, I = d2.inicio, fim = I + d2.partes.reduce((s, p) => s + p.n, 0) * bar;
    let ok = true;
    const det = [];
    for (const t of d2.tracks) {
      const loopC = t.clips.map((c) => [c.s - L0, c.e - L0]);
      const real = junta(intervalos(tr(t.name), I, fim + 1000));
      let esp = [];
      for (const b of t.blocos) {
        const A = I + b.a * bar, B = I + b.b * bar;
        for (let x = 0; A + x < B; x += 32) for (const c of loopC) if (A + x + c[0] < B) esp.push([A + x + c[0], Math.min(B, A + x + c[1])]);
      }
      esp = junta(esp);
      if (JSON.stringify(real) !== JSON.stringify(esp)) { ok = false; det.push(t.name + ' real ' + JSON.stringify(real) + ' esperado ' + JSON.stringify(esp)); }
    }
    det.slice(0, 6).forEach((x) => console.log('     ' + x));
    confere(ok, 'cada faixa toca exatamente nos seus blocos, o loop recomeçando no começo de cada bloco e aparado no fim (bloco solto, atravessando partes, INTRO de 12)');
    confere(tracks.every((t) => t.clips.every((c) => c.e <= Math.max(fim, 1000) + 1e-6)), 'nada passa do fim do rascunho (beat ' + fim + ')');
    confere(fotoLoop() === antes, 'loop original intacto');
    const est = tracks.find((t) => t.name === 'ESTRUTURA');
    confere(!!est && tracks[0] === est && est.has_midi_input, 'faixa MIDI ESTRUTURA criada no topo');
    const nomes = est ? est.clips.map((c) => c.name + '@' + (c.s - I) / bar + '+' + (c.e - c.s) / bar).join(' ') : '';
    console.log('  estrutura: ' + nomes);
    let esperado = [], pos = 0;
    const rot = { INTRO: 'Intro', BREAK: 'Break', BUILDUP: 'Buildup', FINAL: 'Final' };
    let nd = 0;
    for (const p of d2.partes) { esperado.push((p.p === 'DROP' ? 'Drop ' + (++nd) : rot[p.p]) + '@' + pos + '+' + p.n); pos += p.n; }
    confere(nomes === esperado.join(' '), 'um clip por parte com o nome (Drop 1, 2, 3…) no lugar certo');
    confere(est && est.clips.every((c) => c.color === d2.coresParte[d2.partesNomes.indexOf(d2.partes[est.clips.indexOf(c)].p)]), 'clip da estrutura com a cor da parte');
    confere(!tracks.some((t) => t.name === 'REF'), 'faixa REF apagada no fim (APAGAR REF ON)');
    confere(cues.length === 0, 'nenhum locator criado');
    confere(songTime === 0, 'posição de play não mexida');
    ctx.build();
    confere(/UNDO antes/.test(ctx._st), 'BUILD de novo pede UNDO antes');
    ctx.undo();
    esperaLivre(() => {
      console.log('UNDO: ' + ctx._st);
      confere(tracks.every((t) => t.clips.every((c) => c.s < d2.inicio - 1e-6)), 'UNDO apagou todo o rascunho');
      confere(!tracks.some((t) => t.name === 'ESTRUTURA'), 'UNDO apagou a faixa ESTRUTURA');
      confere(fotoLoop() === antes, 'loop original intacto depois do UNDO');
      console.log(erros ? erros + ' ERRO(S)' : 'TUDO OK');
      process.exit(erros ? 1 : 0);
    });
  });
});
