// Simula o Live para testar o RODO (ANALYZE + APPLY + RESET) no Node.
// node sim.js [CLIP|TRACK|ALL] [casas do display: 1|2]
const fs = require('fs'), vm = require('vm'), path = require('path');
const MODO = { CLIP: 0, TRACK: 1, ALL: 2 }[process.argv[2] || 'TRACK'];
const CASAS = +(process.argv[3] || 2);
const TARGET = +(process.env.TARGET || -6);
const TEMPO = 120, SPB = 60 / TEMPO, SR = 1000;

// curva de ganho "secreta" do Live simulado (não linear, teto +24 dB em v=1)
const dbDe = (v) => v <= 0 ? -Infinity : 24 + 40 * Math.log10(v);
const txtDe = (v) => { const d = dbDe(v); return d === -Infinity ? '-inf dB' : d.toFixed(CASAS) + ' dB'; };
const vDe = (db) => Math.pow(10, (db - 24) / 40);

// arquivos: amplitude por tempo (s)
const ARQ = {
  'C:/a.wav': { dur: 20, f: (t) => (t >= 10 && t < 10.01 ? 0.7 : 0.5) },          // -6.02 até 10 s; pico -3.1 em 10 s
  'C:/b.wav': { dur: 10, f: (t) => 0.1 },                                         // -20 dB
  'C:/c.wav': { dur: 10, f: (t) => 0.004 },                                       // -48 dB (vai bater no teto)
  'C:/d.wav': { dur: 30, f: (t) => (t < 15 ? 0.25 : 0.05) },                      // warp: -12 dB na 1ª metade, -26 na 2ª
};
let atual = null;

let nextId = 100;
const nc = (o) => Object.assign({ id: nextId++, muted: 0, warping: 0, looping: 0, sample_rate: SR, name: 'clip', gain: vDe(0) }, o);
const tracks = [
  { name: '1 KICK', clips: [
    nc({ file: 'C:/a.wav', start_time: 0, end_time: 8 / SPB, start_marker: 0, end_marker: 20, loop_start: 0, loop_end: 8 }),   // aparado: toca 0..8 s => -6.02
    nc({ file: 'C:/a.wav', start_time: 32, end_time: 32 + 20 / SPB, start_marker: 0, end_marker: 20, loop_start: 0, loop_end: 20, gain: vDe(-2) }), // inteiro => -3.1 + (-2)
  ] },
  { name: '2 PAD', clips: [nc({ file: 'C:/b.wav', start_time: 8, end_time: 8 + 10 / SPB, start_marker: 0, end_marker: 10, loop_start: 0, loop_end: 10 })] },
  { name: '3 ROOM DISTANTE', clips: [nc({ file: 'C:/c.wav', start_time: 4, end_time: 4 + 10 / SPB, start_marker: 0, end_marker: 10, loop_start: 0, loop_end: 10 })] },
  // warp: markers em beats; warp markers mapeiam beat 0 -> 0 s, beat 60 -> 30 s (arquivo esticado 2x); toca beats 0..20 => 0..10 s => -12 dB
  { name: '4 WARPADO', clips: [nc({ file: 'C:/d.wav', warping: 1, start_time: 16, end_time: 36, start_marker: 0, end_marker: 60, loop_start: 0, loop_end: 20,
      warp_markers: JSON.stringify({ warp_markers: [{ sample_time: 0, beat_time: 0 }, { sample_time: 30, beat_time: 60 }] }) })] },
  { name: '5 MUTADO', clips: [nc({ file: 'C:/b.wav', muted: 1, start_time: 0, end_time: 20, start_marker: 0, end_marker: 10, loop_start: 0, loop_end: 10 })] },
];
const ganhoOrig = {}; tracks.forEach((t) => t.clips.forEach((c) => (ganhoOrig[c.id] = c.gain)));

function byId(id) { for (const t of tracks) for (const c of t.clips) if (c.id === id) return c; return null; }
function resolve(p) {
  const w = p.split(' ');
  if (w[0] === 'id') { const c = byId(+w[1]); return { kind: c ? 'clip' : 'none', c }; }
  if (p === 'live_set') return { kind: 'set' };
  if (p === 'live_set view selected_track') return { kind: 'track', t: 0 };
  if (w[1] === 'tracks' && w.length === 3) return { kind: 'track', t: +w[2] };
  if (w[1] === 'tracks' && w[3] === 'arrangement_clips') return { kind: 'clip', c: tracks[+w[2]].clips[+w[4]] };
  throw new Error('path ' + p);
}
let sets = 0;
class LiveAPI {
  constructor(p) { this.r = resolve(p); }
  goto(p) { this.r = resolve(p); }
  get id() { return this.r.kind === 'clip' ? this.r.c.id : this.r.kind === 'track' ? 1 + this.r.t : 0; }
  getcount(k) { return k === 'tracks' ? tracks.length : tracks[this.r.t].clips.length; }
  get(k) {
    if (this.r.kind === 'set') return [{ tempo: TEMPO, signature_numerator: 4, signature_denominator: 4 }[k]];
    if (this.r.kind === 'track') { const t = tracks[this.r.t]; return [{ is_foldable: 0, has_audio_input: 1, name: t.name, color: 0xff8800 }[k]]; }
    const c = this.r.c;
    if (k === 'file_path') return [c.file];
    if (k === 'gain_display_string') return [txtDe(c.gain)];
    if (k === 'warp_markers') { if (!c.warp_markers) throw new Error('sem warp'); return [c.warp_markers]; }
    if (k in c) return [c[k]];
    throw new Error('get ' + k);
  }
  set(k, v) { if (k !== 'gain') throw new Error('set ' + k); sets++; this.r.c.gain = Math.max(0, Math.min(1, v)); }
}

let dicts = {};
let motor;
const outs = [];
const esperaLivre = (fn) => { const h = setInterval(() => { if (!motor.busy) { clearInterval(h); fn(); } }, 50); };
const LOGDIR = require('os').tmpdir() + '/rodo-sim2'; fs.rmSync(LOGDIR, { recursive: true, force: true }); fs.mkdirSync(LOGDIR);
const ctx = {
  LiveAPI,
  Dict: class { constructor(n) { this.n = n; } stringify() { return dicts[this.n] || '{}'; } parse(s) { dicts[this.n] = s; } },
  Buffer: class {
    framecount() { return ARQ[atual].dur * SR; } channelcount() { return 2; }
    peek(ch, f, n) { const a = []; for (let i = 0; i < n; i++) a.push((i % 2 ? -1 : 1) * ARQ[atual].f((f + i) / SR)); return a; }
  },
  Task: class { constructor(fn) { this.fn = fn; this.interval = 0; } schedule(ms) { this.h = setTimeout(this.fn, ms > 1000 ? ms : 0); }
    repeat() { this.h = setInterval(this.fn, this.interval); } cancel() { clearTimeout(this.h); clearInterval(this.h); } },
  outlet: (n, ...a) => {
    if (n === 0 && a[0] === 'replace') { atual = a[1]; setTimeout(() => motor.loaded(), 0); }
    if (n === 1 && process.env.VERBOSO) console.log('STATUS:', a[1]);
    if (n === 2 && a[0] !== 'status') outs.push(a);
  },
  post: (s) => { if (process.env.VERBOSO || /erro|pulado|teto|ficou/i.test(s)) process.stdout.write(s); },
  patcher: { filepath: 'C:/Users/teste/Documents/Ableton/User Library/Presets/Rodo-0.2.0/Rodo.amxd' },
  ...require('./lic-mock.js')(LOGDIR, () => motor),
  Float32Array, Math, JSON, Date, Number, String, parseFloat, isNaN, Array, Infinity, isFinite,
};
vm.createContext(ctx);
vm.runInContext(fs.readFileSync((process.env.MOTOR || require('path').join(__dirname, '../rodo/rodo.js')), 'utf8'), ctx);
motor = ctx;

// pico que sai de cada clip (dB) = pico do trecho tocado + ganho
const picoTrecho = { 100: -6.02, 101: -3.1, 102: -20, 103: -47.96, 104: -12.04 };
const sai = (c) => picoTrecho[c.id] + dbDe(c.gain);
let erros = 0;
const confere = (ok, txt) => { console.log((ok ? '  ok   ' : '  ERRO ') + txt); if (!ok) erros++; };

motor.init();
motor.target(TARGET); motor.mode(MODO);
require('./lic-passos.js')(motor, outs, confere, () => {
motor.analyze();
esperaLivre(() => {
  const d = JSON.parse(dicts.rodo_data);
  console.log('ANALYZE:');
  d.tracks.forEach((t) => t.clips.forEach((c) => console.log(`  ${t.name.padEnd(16)} ${c.state.padEnd(10)} pk ${c.pk != null ? c.pk.toFixed(2) : '-'} g0 ${c.g0} -> g1 ${c.g1 != null ? c.g1.toFixed(2) : '-'}`)));
  const env = JSON.parse(dicts.rodo_env);
  confere(env.tracks.length === 5 && env.tracks[0][0].length > 3, 'contorno das ondas publicado (' + env.tracks.map((t) => t.map((s) => s.length).join('+')).join(' ') + ' pontos)');
  confere(Math.abs(d.tracks[0].clips[0].pk - -6.02) < 0.05, 'clip aparado: pico só do trecho tocado (-6.02)');
  confere(Math.abs(d.tracks[3].clips[0].pk - -12.04) < 0.05, 'clip com WARP: pico pelo mapa dos warp markers (-12.04)');
  confere(d.tracks[4].clips[0].state === 'muted', 'clip mutado fica fora');
  motor.apply();
  esperaLivre(() => setTimeout(() => esperaLivre(() => {
    console.log('APPLY ' + ['CLIP', 'TRACK', 'ALL'][MODO] + ' (' + sets + ' sets de ganho):');
    tracks.forEach((t) => t.clips.forEach((c) => { if (!c.muted) console.log(`  ${t.name.padEnd(16)} ganho ${txtDe(c.gain).padStart(10)} · sai ${sai(c).toFixed(2)} dB`); }));
    const ok = tracks.slice(0, 4).flatMap((t) => t.clips);
    if (MODO === 0) ok.forEach((c) => c.id !== 103 && confere(Math.abs(sai(c) - TARGET) < 0.08, `CLIP: ${c.id} sai em ${TARGET}`));
    if (MODO === 1) {
      confere(Math.abs(Math.max(sai(ok[0]), sai(ok[1])) - TARGET) < 0.08, 'TRACK: clip mais alto do KICK no TARGET');
      confere(Math.abs((sai(ok[0]) - sai(ok[1])) - (-6.02 - -5.1)) < 0.1, 'TRACK: diferença entre os 2 clips do KICK mantida');
    }
    if (MODO === 2) {
      const mx = Math.max(...ok.map(sai)); confere(Math.abs(mx - TARGET) < 0.08, 'ALL: clip mais alto do set no TARGET');
      confere(Math.abs((sai(ok[2]) - sai(ok[0])) - (-20 - -6.02)) < 0.1, 'ALL: balanço entre faixas mantido');
    }
    const room = byId(103);
    if (MODO !== 2) confere(Math.abs(room.gain - 1) < 1e-9, 'ROOM DISTANTE bateu no teto (ganho máximo)');
    const d2 = JSON.parse(dicts.rodo_data);
    if (MODO !== 2) confere(d2.teto != null && Math.abs(d2.teto - 24) < 0.01, 'teto do Clip Gain medido: ' + d2.teto);
    const prog = outs.filter((o) => o[0] === 'progresso').map((o) => o[3]);
    confere(prog.every((s, i) => i === 0 || s >= prog[i - 1]), 'APPLY da esquerda para a direita (' + prog.join(', ') + ')');
    confere(outs.some((o) => o[0] === 'varre') && outs.some((o) => o[0] === 'fim'), 'janela recebeu varre/fim');
    // 2º APPLY: nada muda (já está no ponto)
    const s0 = sets; motor.apply();
    esperaLivre(() => setTimeout(() => esperaLivre(() => {
      confere(sets - s0 <= (MODO === 2 ? 0 : 1), '2º APPLY quase não mexe (' + (sets - s0) + ' sets)');
      motor.reset();
      esperaLivre(() => setTimeout(() => esperaLivre(() => {
        const volta = tracks.flatMap((t) => t.clips).every((c) => Math.abs(c.gain - ganhoOrig[c.id]) < 1e-9);
        confere(volta, 'RESET voltou todos os ganhos originais');
        const lg = fs.readFileSync(LOGDIR + '/u/Documents/Ableton/User Library/Presets/Rodo-0.2.0/RODO-log.txt', 'utf8');
        confere(lg.length > 100, 'log em arquivo (' + lg.split('\n').length + ' linhas)');
        console.log(erros ? '\nRESULTADO: ' + erros + ' ERROS' : '\nRESULTADO: OK');
        process.exit(erros ? 1 : 0);
      }), 400), 100);
    }), 400), 100);
  }), 400), 100);
});
});
