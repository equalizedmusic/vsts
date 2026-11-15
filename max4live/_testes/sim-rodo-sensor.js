// RODO 0.3.0 — testa SENSOR RMS/LUFS e PEAK GUARD com sinais de valor conhecido (seno 997 Hz a 48 kHz).
// node sim-rodo-sensor.js   (MOTOR=<rodo.js do build>)
const fs = require('fs'), vm = require('vm');
const TEMPO = 120, SPB = 60 / TEMPO, SR = 48000;
const dbDe = (v) => v <= 0 ? -Infinity : 24 + 40 * Math.log10(v);
const txtDe = (v) => { const d = dbDe(v); return d === -Infinity ? '-inf dB' : d.toFixed(2) + ' dB'; };
const vDe = (db) => Math.pow(10, (db - 24) / 40);
const sen = (A, f = 997) => (t) => A * Math.sin(2 * Math.PI * f * t);
// arquivos: { dur, nch, f(t, ch) }
const ARQ = {
  'C:/s0.wav': { dur: 6, nch: 2, f: sen(1) },                        // 0 dBFS estéreo: LUFS 0.0 · RMS −3.01
  'C:/s20.wav': { dur: 6, nch: 2, f: sen(0.1) },                     // −20: LUFS −20.0 · RMS −23.01
  'C:/mono.wav': { dur: 6, nch: 1, f: sen(0.1) },                    // mono −20: LUFS −20.0 (dual mono)
  'C:/meio.wav': { dur: 8, nch: 2, f: (t) => (t < 4 ? sen(0.1)(t) : 0) },   // metade silêncio: LUFS −20 (gate)
  // "bateria": pulsos fortes (pico −1) e corpo baixo => crest alto; RMS ~ −30
  'C:/bat.wav': { dur: 6, nch: 2, f: (t) => ((t % 0.5) < 0.004 ? 0.89 : 0.03) * Math.sin(2 * Math.PI * 200 * t) },
};
let atual = null;
let nextId = 100;
const nc = (file, s, o) => { const d = ARQ[file].dur; return Object.assign({ id: nextId++, muted: 0, warping: 0, looping: 0, sample_rate: SR, name: file, gain: vDe(0),
  file, start_time: s, end_time: s + d / SPB, start_marker: 0, end_marker: d, loop_start: 0, loop_end: d }, o || {}); };
const tracks = [
  { name: '1 ZERO', clips: [nc('C:/s0.wav', 0)] },
  { name: '2 MENOS20', clips: [nc('C:/s20.wav', 0), nc('C:/s20.wav', 16, { gain: vDe(-6) })] },
  { name: '3 MONO', clips: [nc('C:/mono.wav', 4)] },
  { name: '4 MEIO', clips: [nc('C:/meio.wav', 0)] },
  { name: '5 BATERIA', clips: [nc('C:/bat.wav', 0)] },
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
    if (k in c) return [c[k]];
    throw new Error('get ' + k);
  }
  set(k, v) { this.r.c.gain = Math.max(0, Math.min(1, v)); }
}
let dicts = {}, motor;
const outs = [];
const esperaLivre = (fn) => { const h = setInterval(() => { if (!motor.busy) { clearInterval(h); fn(); } }, 50); };
const LOGDIR = require('os').tmpdir() + '/rodo-sim3'; fs.rmSync(LOGDIR, { recursive: true, force: true }); fs.mkdirSync(LOGDIR);
const ctx = {
  LiveAPI,
  Dict: class { constructor(n) { this.n = n; } stringify() { return dicts[this.n] || '{}'; } parse(s) { dicts[this.n] = s; } },
  Buffer: class {
    framecount() { return ARQ[atual].dur * SR; } channelcount() { return ARQ[atual].nch; }
    peek(ch, f, n) { const a = new Array(n); const fn = ARQ[atual].f; for (let i = 0; i < n; i++) a[i] = fn((f + i) / SR, ch); return a; }
  },
  Task: class { constructor(fn) { this.fn = fn; this.interval = 0; } schedule(ms) { this.h = setTimeout(this.fn, ms > 1000 ? ms : 0); }
    repeat() { this.h = setInterval(this.fn, this.interval); } cancel() { clearTimeout(this.h); clearInterval(this.h); } },
  outlet: (n, ...a) => {
    if (n === 0 && a[0] === 'replace') { atual = a[1]; setTimeout(() => motor.loaded(), 0); }
    if (n === 2 && a[0] !== 'status') outs.push(a);
    if (n === 3) ecos.push(a);
  },
  post: (s) => { if (process.env.VERBOSO || /erro/i.test(s)) process.stdout.write(s); },
  patcher: { filepath: 'C:/Users/teste/Documents/Ableton/User Library/Presets/Rodo-0.3.0/Rodo.amxd' },
  ...require('./lic-mock.js')(LOGDIR, () => motor),
  Float32Array, Float64Array, Math, JSON, Date, Number, String, parseFloat, isNaN, Array, Infinity, isFinite,
};
const ecos = [];
vm.createContext(ctx);
vm.runInContext(fs.readFileSync((process.env.MOTOR || require('path').join(__dirname, '../rodo/rodo.js')), 'utf8'), ctx);
motor = ctx;
let erros = 0;
const confere = (ok, txt) => { console.log((ok ? '  ok   ' : '  ERRO ') + txt); if (!ok) erros++; };
const perto = (a, b, tol) => a != null && Math.abs(a - b) <= tol;
const dados = () => JSON.parse(dicts.rodo_data);
const clip = (d, t, c) => d.tracks[t].clips[c];
const espera = (fn) => esperaLivre(() => setTimeout(() => esperaLivre(fn), 400));

// coeficientes da ponderação K em 48 kHz = tabela da BS.1770
const K = motor.filtroK(48000);
confere(perto(K.b0, 1.53512485958697, 1e-9) && perto(K.b1, -2.69169618940638, 1e-9) && perto(K.b2, 1.19839281085285, 1e-9) &&
  perto(K.a1, -1.69065929318241, 1e-9) && perto(K.a2, 0.73248077421585, 1e-9) && perto(K.c1, -1.99004745483398, 1e-9) && perto(K.c2, 0.99007225036621, 1e-9),
  'filtro K em 48 kHz = coeficientes da BS.1770');

motor.init();
require('./lic-passos.js')(motor, outs, confere, () => {
  motor.mode(1);
  motor.analyze();
  esperaLivre(() => {
    let d = dados();
    console.log('MEDIDAS (arquivo):');
    d.tracks.forEach((t) => t.clips.forEach((c) => console.log(`  ${t.name.padEnd(12)} pk ${c.pk.toFixed(2)} rms ${c.rms == null ? '-' : c.rms.toFixed(2)} lufs ${c.lufs == null ? '-' : c.lufs.toFixed(2)}`)));
    confere(perto(clip(d, 0, 0).lufs, 0, 0.05) && perto(clip(d, 0, 0).rms, -3.01, 0.02), 'seno 997 Hz 0 dBFS estéreo: 0.0 LUFS e −3.01 dB RMS');
    confere(perto(clip(d, 1, 0).lufs, -20, 0.05) && perto(clip(d, 1, 0).rms, -23.01, 0.02), 'seno −20 dBFS: −20.0 LUFS e −23.01 dB RMS');
    confere(perto(clip(d, 2, 0).lufs, -20, 0.05), 'mono −20 dBFS: −20.0 LUFS (toca nos 2 lados)');
    confere(perto(clip(d, 3, 0).lufs, -20, 0.25) && perto(clip(d, 3, 0).rms, -23.01, 0.25), 'metade silêncio: o gate ignora o silêncio (só a borda conta: ' + clip(d, 3, 0).lufs.toFixed(2) + ' LUFS, ' + clip(d, 3, 0).rms.toFixed(2) + ' RMS; sem gate daria −23/−26)');

    // PEAK continua igual (padrão)
    confere(d.cfg.sensor === 0 && d.cfg.guard === 1, 'padrão: SENSOR PEAK, PEAK GUARD ligado');
    // LUFS · TRACK · −18: a faixa inteira vai a −18 LUFS; a dif. entre os 2 clips do MENOS20 fica
    motor.ui_sensor(2); motor.ui_target(-18);
    setTimeout(() => {
      d = dados();
      confere(ecos.some((e) => e[0] === 'sensor' && e[1] === 2) && ecos.some((e) => e[0] === 'tlufs' && e[1] === -18), 'janela -> controles do device: sensor 2, tlufs −18 (TARGET do PEAK intocado: ' + d.cfg.target + ')');
      const t2 = d.tracks[1];
      // faixa: clip A −20 LUFS, clip B −26 LUFS, mesma duração => integrada ~ −22.0 (com gate relativo de −10 LU os dois entram)
      confere(perto(t2.out, 10 * Math.log10((1e-2 + 10 ** -2.6) / 2), 0.1), 'LUFS da faixa MENOS20 (2 clips) medida junta: ' + t2.out.toFixed(2));
      confere(perto(t2.clips[0].delta, t2.clips[1].delta, 1e-9) && perto(t2.out1, -18, 0.05), 'TRACK: mesmo ajuste na faixa e ela sai em −18 LUFS (' + t2.out1.toFixed(2) + ')');
      const t1 = d.tracks[0];
      confere(t1.guardDb === 0 && perto(t1.out1, -18, 0.01), 'ZERO desce para −18 LUFS (o GUARD não atrapalha quem desce)');
      motor.ui_mode(0);
      setTimeout(() => {
        d = dados();
        confere(perto(d.tracks[1].clips[0].out1, -18, 0.01) && perto(d.tracks[1].clips[1].out1, -18, 0.01), 'CLIP: cada clip do MENOS20 em −18 LUFS');
        // RMS · ALL · −18: a faixa mais alta vai a −18 RMS e todas recebem o mesmo ajuste
        motor.ui_sensor(1); motor.ui_target(-18); motor.ui_mode(2);
        setTimeout(() => {
          d = dados();
          const ds = new Set(d.tracks.flatMap((t) => t.clips.map((c) => c.delta.toFixed(6))));
          const top = Math.max(...d.tracks.map((t) => t.out));
          confere(ds.size === 1, 'ALL: um ajuste só para o set inteiro (' + [...ds].map(Number).map((x) => x.toFixed(2)).join(',') + ' dB)');
          confere(perto(top + d.tracks[0].clips[0].delta, -18, 0.01), 'ALL: a faixa mais alta (ZERO, ' + top.toFixed(2) + ' RMS) vai a −18');
          // ALL + GUARD subindo: tira a ZERO (clique no nome) e pede −6 RMS: o set inteiro sobe só até o pico da BATERIA bater −0.3
          const dd = dados(); dd.tracks[0].skip = 1; dicts.rodo_data = JSON.stringify(dd); motor.marcas(); motor.ui_target(-6);
          setTimeout(() => {
          d = dados();
          const dl = new Set(d.tracks.slice(1).flatMap((t) => t.clips.map((c) => c.delta.toFixed(6))));
          confere(dl.size === 1 && perto(-1.01 + d.tracks[4].clips[0].delta, -0.3, 0.02) && d.nGuard > 0,
            'ALL + GUARD: queria +' + (-6 - d.tracks[1].out).toFixed(1) + ' dB, o set inteiro sobe só ' + [...dl][0].slice(0, 5) + ' (pico da BATERIA em −0.3)');
          const de = dados(); de.tracks[0].skip = 0; dicts.rodo_data = JSON.stringify(de); motor.marcas();
          // BATERIA: RMS · TRACK · −12 com e sem guard
          motor.ui_mode(1); motor.ui_target(-12);
          setTimeout(() => {
            d = dados();
            const b = d.tracks[4];
            confere(b.guardDb > 0.5 && perto(b.clips[0].pk + b.clips[0].g1, -0.3, 0.01), 'BATERIA (crest alto) a −12 RMS: GUARD segura ' + b.guardDb.toFixed(1) + ' dB, pico −0.3');
            motor.ui_guard(0);
            setTimeout(() => {
              d = dados();
              const b2 = d.tracks[4];
              confere(perto(b2.out1, -12, 0.05) && b2.clips[0].pk + b2.clips[0].g1 > 0, 'GUARD OFF: BATERIA vai a −12 RMS e o pico passa de 0 (' + (b2.clips[0].pk + b2.clips[0].g1).toFixed(1) + ' dB)');
              motor.ui_guard(1);
              // APPLY de verdade em LUFS TRACK −18 e confere no "Live"
              motor.ui_sensor(2); motor.ui_target(-18);
              setTimeout(() => {
                motor.apply();
                espera(() => {
                  const sai = (c, lu) => lu + dbDe(c.gain);
                  const m20 = tracks[1].clips;
                  // nível integrado da faixa no Live simulado
                  const pA = 10 ** (sai(m20[0], -20) / 10), pB = 10 ** (sai(m20[1], -20) / 10);
                  confere(perto(10 * Math.log10((pA + pB) / 2), -18, 0.08), 'APPLY LUFS TRACK: MENOS20 sai em −18 LUFS no Live (' + (10 * Math.log10((pA + pB) / 2)).toFixed(2) + ')');
                  confere(perto(sai(tracks[2].clips[0], -20), -18, 0.08), 'APPLY: MONO em −18 LUFS');
                  confere(perto(dbDe(tracks[0].clips[0].gain), -18, 0.08), 'APPLY: ZERO em −18 LUFS (' + dbDe(tracks[0].clips[0].gain).toFixed(2) + ')');
                  confere(-1.01 + dbDe(tracks[4].clips[0].gain) <= -0.25, 'APPLY: pico da BATERIA não passou de −0.3 (' + (-1.01 + dbDe(tracks[4].clips[0].gain)).toFixed(2) + ')');
                  const d3 = dados();
                  confere(d3.cfg.sensor === 2 && perto(d3.tracks[1].out, -18, 0.08), 'releitura depois do APPLY mostra −18 LUFS');
                  motor.reset();
                  espera(() => {
                    confere(tracks.flatMap((t) => t.clips).every((c) => Math.abs(c.gain - ganhoOrig[c.id]) < 1e-9), 'RESET voltou todos os ganhos');
                    console.log(erros ? '\nRESULTADO: ' + erros + ' ERROS' : '\nRESULTADO: OK');
                    process.exit(erros ? 1 : 0);
                  });
                });
              }, 300);
            }, 300);
          }, 300);
          }, 300);
        }, 300);
      }, 300);
    }, 300);
  });
});
