// Simula o Live (arranjo com sobreposiÃ§Ã£o/corte de clips) para testar o APPLY da VASSOURA no Node.
// node sim.js <MUTE|CUT>
const fs = require('fs');
const vm = require('vm');
const path = require('path');

const MODO = process.argv[2] === 'CUT' ? 1 : 0;
const TEMPO = 120, SPB = 60 / TEMPO, SR = 1000; // sr baixo sÃ³ para o teste
// "arquivo": 20 s; som em [2,5) e [9,10) e [15,20); resto silÃªncio
const SOM = [[2, 5], [9, 10], [15, 20]];
const temSom = (t) => SOM.some(([a, b]) => t >= a && t < b);

let nextId = 100;
const tracks = [
  { name: '1-x - kick', clips: [] },
  { name: '2-x - snare', clips: [] },
  { name: '3-x - aparado', clips: [] },
];
function novoClip(o) { return Object.assign({ id: nextId++, muted: 0, warping: 0, looping: 0, sample_rate: SR, file: 'C:\\a.wav', name: 'a' }, o); }
// clip 1: arquivo inteiro no beat 8 (=4 s); clip 2 na outra faixa: arquivo de 1 s a 19 s, no beat 0; + um clip vizinho depois
tracks[0].clips.push(novoClip({ start_time: 8, end_time: 8 + 20 / SPB, start_marker: 0, end_marker: 20, loop_start: 0, loop_end: 20 }));
tracks[1].clips.push(novoClip({ start_time: 0, end_time: 18 / SPB, start_marker: 1, end_marker: 19, loop_start: 1, loop_end: 19 }));
tracks[1].clips.push(novoClip({ start_time: 18 / SPB, end_time: 18 / SPB + 4, start_marker: 0, end_marker: 2, loop_start: 0, loop_end: 2, file: 'C:\\b.wav', name: 'vizinho' }));

// clip 3: gravacao aparada no arranjo - end_marker no fim do arquivo, toca so loop 2..18 s (caso do cliente 10/11)
tracks[2].clips.push(novoClip({ start_time: 4, end_time: 4 + 16 / SPB, start_marker: 0.5, end_marker: 20, loop_start: 2, loop_end: 18 }));

const sortClips = (t) => t.clips.sort((a, b) => a.start_time - b.start_time);
function byId(id) { for (const t of tracks) for (const c of t.clips) if (c.id === id) return [t, c]; return [null, null]; }

function setProp(c, k, v) {
  const len = () => (c.loop_end - c.loop_start);
  if (k === 'muted') { c.muted = v; return; }
  if (k === 'start_marker') { if (v >= c.end_marker) return console.log('   LIVE: start_marker recusado'); c.start_marker = v; return; }
  if (k === 'end_marker') { if (v <= c.start_marker) return console.log('   LIVE: end_marker recusado'); c.end_marker = v; return; }
  if (k === 'loop_start') { if (v >= c.loop_end) return console.log('   LIVE: loop_start recusado'); c.loop_start = v; c.end_time = c.start_time + len() / SPB; return; }
  if (k === 'loop_end') { if (v <= c.loop_start || v > 20 + 1e-9) return console.log('   LIVE: loop_end recusado'); c.loop_end = v; c.end_time = c.start_time + len() / SPB; return; }
  throw new Error('set ' + k);
}

// colar sobrepondo: o que estiver embaixo Ã© cortado/dividido, preservando o conteÃºdo
function colar(t, nc) {
  const p = nc.start_time, q = nc.end_time, out = [];
  for (const c of t.clips) {
    if (c.end_time <= p + 1e-9 || c.start_time >= q - 1e-9) { out.push(c); continue; }
    if (c.start_time < p - 1e-9) out.push(Object.assign({}, c, { id: nextId++, end_time: p, loop_end: c.loop_start + (p - c.start_time) * SPB, end_marker: c.loop_start + (p - c.start_time) * SPB }));
    if (c.end_time > q + 1e-9) { const ls = c.loop_start + (q - c.start_time) * SPB; out.push(Object.assign({}, c, { id: nextId++, start_time: q, loop_start: ls, start_marker: ls })); }
  }
  out.push(nc);
  t.clips = out; sortClips(t);
}

function resolve(p) {
  const w = p.split(' ');
  if (w[0] === 'id') { const [t, c] = byId(+w[1]); return { kind: 'clip', c }; }
  if (p === 'live_set') return { kind: 'set' };
  if (p === 'live_set view selected_track') return { kind: 'track', t: 0 };
  if (w[0] === 'live_set' && w[1] === 'tracks' && w.length === 3) return { kind: 'track', t: +w[2] };
  if (w[1] === 'tracks' && w[3] === 'arrangement_clips') return { kind: 'clip', c: tracks[+w[2]].clips[+w[4]] };
  throw new Error('path ' + p);
}

let novosApi = 0, leituras = 0;
class LiveAPI {
  constructor(p) { novosApi++; this.r = resolve(p); }
  set path(p) { this.r = resolve(p); }
  goto(p) { this.r = resolve(p); }
  set id(n) { this.r = resolve('id ' + n); }
  get id() { return this.r.kind === 'clip' ? this.r.c.id : this.r.kind === 'track' ? 1 + this.r.t : 0; }
  getcount(k) { return k === 'tracks' ? tracks.length : tracks[this.r.t].clips.length; }
  get(k) {
    if (this.r.kind === 'set') return [{ tempo: TEMPO, signature_numerator: 4, signature_denominator: 4, song_length: 60 }[k]];
    if (this.r.kind === 'track') { const t = tracks[this.r.t]; return [{ is_foldable: 0, has_audio_input: 1, name: t.name, color: 0xff8800 }[k]]; }
    const c = this.r.c;
    if (k === 'file_path') return [c.file];
    if (k in c) return [c[k]];
    throw new Error('get ' + k);
  }
  set(k, v) { setProp(this.r.c, k, v); }
  call(fn, idw, id, extra) {
    const t = tracks[this.r.t];
    const [, src] = byId(id);
    if (!src) return console.log('   LIVE: id invÃ¡lido', id);
    if (fn === 'delete_clip') { t.clips = t.clips.filter((c) => c.id !== id); return; }
    if (fn === 'duplicate_clip_to_arrangement') {
      const nc = Object.assign({}, src, { id: nextId++, start_time: extra, end_time: extra + (src.end_time - src.start_time) });
      colar(t, nc); return;
    }
    throw new Error('call ' + fn);
  }
}

let dict = '{}';
let motor;
const PERDE = +(process.env.PERDE || 0); let nTask = 0, perdidas = 0;
const LOGDIR = require('os').tmpdir() + '/vassoura-sim5'; fs.rmSync(LOGDIR, { recursive: true, force: true }); fs.mkdirSync(LOGDIR);
const esperaLivre = (fn) => { const h = setInterval(() => { if (!motor.busy) { clearInterval(h); fn(); } }, 50); };
const ctx = {
  LiveAPI,
  Dict: class { stringify() { return dict; } parse(s) { dict = s; } },
  Buffer: class {
    framecount() { return 20 * SR; } channelcount() { return 2; }
    peek(ch, f, n) { const a = []; for (let i = 0; i < n; i++) a.push(temSom((f + i) / SR) ? 0.5 : 0.0001); return a; }
  },
  Task: class { constructor(fn) { this.fn = fn; this.interval = 0; }
    schedule(ms) { nTask++; if (PERDE && nTask % PERDE === 0) { perdidas++; return; } this.h = setTimeout(this.fn, ms > 1000 ? ms : 0); }
    repeat() { this.h = setInterval(this.fn, this.interval); } cancel() { clearTimeout(this.h); clearInterval(this.h); } },
  outlet: (n, ...a) => {
    if (n === 0 && a[0] === 'replace') leituras++, setTimeout(() => motor.loaded(), 0);
    if (n === 1 && a[0] === 'status' && process.env.VERBOSO) console.log('STATUS:', a[1]);
    if (n === 2) global.JAN = (global.JAN || []).concat([a]);
  },
  post: (s) => process.stdout.write(s),
  patcher: { filepath: 'C:/Users/teste/Documents/Vassoura-0.3.0/Vassoura.amxd' },
  Float32Array, Math, ...require('./lic-mock.js')(LOGDIR), JSON, Date, Number, String, parseFloat, isNaN, Array,
};
ctx.arrayfromargs = (a) => Array.prototype.slice.call(a);
vm.createContext(ctx);
vm.runInContext(fs.readFileSync((process.env.MOTOR || require('path').join(__dirname, '../vassoura/vassoura.js')), 'utf8'), ctx);
motor = ctx;

function dump(titulo) {
  console.log('\n== ' + titulo);
  tracks.forEach((t, i) => {
    console.log(' faixa ' + i + ' ' + t.name);
    for (const c of t.clips) console.log(`   [${c.start_time.toFixed(3)} â€“ ${c.end_time.toFixed(3)}] ${c.muted ? 'MUTED' : 'som  '} arquivo ${c.loop_start.toFixed(3)}â€“${c.loop_end.toFixed(3)} s  ${c.name}`);
  });
}

// confere: em qualquer instante do arranjo, o arquivo tocado Ã© o mesmo de antes (conteÃºdo nÃ£o deslocou)
const antes = JSON.parse(JSON.stringify(tracks));
function arquivoEm(lst, beat) {
  for (const c of lst) if (beat >= c.start_time - 1e-9 && beat < c.end_time - 1e-9) return { t: c.loop_start + (beat - c.start_time) * SPB, m: c.muted, f: c.file };
  return null;
}

motor.init();
motor.threshold(-60); motor.minlength(500); motor.prems(100); motor.postms(200); motor.snap(+(process.env.SNAP||0)); motor.mode(MODO);
dump('ANTES');
motor.analyze();
console.log('sem licença, busy=' + motor.busy);
motor.lic_email('cliente@teste.com'); motor.lic_token('ISP-AAAA-BBBB-CCCC-DDDD-EEEE'); motor.lic_ativar();
setTimeout(() => { console.log('licença: ' + motor.licLiberada()); motor.analyze();
setTimeout(() => esperaLivre(() => {
  const d = JSON.parse(dict);
  d.tracks.forEach((t) => t.clips.forEach((c) => console.log('vazios', t.name, c.state, c.empties.map((r) => r.s.toFixed(2) + '-' + r.e.toFixed(2)).join(' '))));
  motor.apply();
  if (MODO === 1) motor.apply(); // 2Âº clique confirma o CUT
  setTimeout(() => esperaLivre(() => setTimeout(() => {
    dump('DEPOIS (' + (MODO ? 'CUT' : 'MUTE') + ')');
    let erros = 0, nSom = 0, nVazio = 0;
    for (let ti = 0; ti < tracks.length; ti++) {
      for (let b = 0; b < 60; b += 0.01) {
        const a = arquivoEm(antes[ti].clips, b), d = arquivoEm(tracks[ti].clips, b);
        if (!a) { if (d) { erros++; } continue; }
        const som = a.f.endsWith('a.wav') && temSom(a.t);
        if (d && (Math.abs(d.t - a.t) > 1e-6 || d.f !== a.f)) { erros++; if (erros < 5) console.log('DESLOCOU no beat', b.toFixed(2)); }
        const tocando = d && !d.m;
        if (som && !tocando) { erros++; if (erros < 5) console.log('SOM PERDIDO no beat', b.toFixed(2)); }
        if (som) nSom++; else if (!tocando) nVazio++;
      }
    }
    if (tracks.some((t) => t.clips.some((c) => c.start_time >= 60))) { erros++; console.log('SOBROU CÃ“PIA DE TRABALHO'); }
    console.log(`\nRESULTADO ${MODO ? 'CUT' : 'MUTE'}: ${erros ? erros + ' ERROS' : 'OK'} Â· amostras de som intactas ${nSom} Â· amostras silenciadas ${nVazio}`);
    const lg = fs.existsSync(LOGDIR + '/u/Documents/Vassoura-0.3.0/VASSOURA-log.txt') ? fs.readFileSync(LOGDIR + '/u/Documents/Vassoura-0.3.0/VASSOURA-log.txt', 'utf8') : '';
    console.log('Tasks perdidas de propósito: ' + perdidas + ' · log em arquivo: ' + lg.split(String.fromCharCode(10)).length + ' linhas');
    if (!lg) { erros++; console.log('LOG NAO GRAVADO'); }
    if (process.env.MOSTRALOG) console.log(lg);
    const ev = (global.JAN || []).map((a) => a[0]); console.log('janela: varre ' + ev.filter((e) => e === 'varre').length + ' · progresso ' + ev.filter((e) => e === 'progresso').length + ' · fim ' + ev.filter((e) => e === 'fim').length + ' · valores ' + ev.filter((e) => e === 'valores').length);
    process.exit(erros ? 1 : 0);
  }, 800)), 100);
}), 100);
}, 80);
