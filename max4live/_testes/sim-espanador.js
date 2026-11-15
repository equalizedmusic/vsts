// Simula o Live para testar o ESPANADOR (SCAN + AUTO + APPLY + UNDO) no Node.
// set MOTOR=<build>/espanador.js & node sim-espanador.js [formato da rota aceito pelo Live falso: 0..3]
const fs = require('fs'), vm = require('vm'), path = require('path'), os = require('os');
const FMT = +(process.argv[2] || 1);

let nextId = 10, nextObj = 5000;
const objs = {};   // clips e clip slots por id (arranjo: 1 objeto por clip; session: 2 slots, o 1º com clip)
const mk = (name, o) => {
  const t = Object.assign({ id: nextId++, name, color: 0x808080, is_foldable: 0, is_grouped: 0, has_midi_input: 0,
    out: 'Master', inp: 'Ext. In', mon: 1, devices: 0, clips: [[16, 48], [64, 96]] }, o || {});
  t.cl = t.clips.map(() => { const c = { kind: 'clipobj', id: nextObj++, color: 0x123456 }; objs[c.id] = c; return c; });
  t.slots = t.is_foldable ? [] : [0, 1].map((k) => {
    const sc = k === 0 && t.clips.length ? { kind: 'clipobj', id: nextObj++, color: 0x654321 } : null; if (sc) objs[sc.id] = sc;
    const sl = { kind: 'slot', id: nextObj++, clip: sc }; objs[sl.id] = sl; return sl;
  });
  return t;
};
let tracks = [
  mk('01 Kick'), mk('02 Kick Sub'), mk('03 Snare'), mk('04 Clap'), mk('05 HH Closed'),
  mk('Percs', { is_foldable: 1, clips: [] }), mk('06 Conga', { is_grouped: 1, out: 'Percs' }), mk('07 Shaker', { is_grouped: 1, out: 'Percs' }),
  mk('08 Bass FM'), mk('09 Lead Acid', { has_midi_input: 1 }), mk('10 Pad'), mk('11 Vox Lead'), mk('12 Riser'), mk('13 Coisa Sem Nome'),
  mk('DRUMS', { is_foldable: 1, clips: [] }),   // Group Track chamado DRUMS: o bus do DRUMS tem de virar "DRUMS BUS"
  mk('Toms', { is_foldable: 1, clips: [] }), mk('14 Tom Hi', { is_grouped: 1, out: 'Toms', grp: 'Toms' }), mk('15 Tom Lo', { is_grouped: 1, out: 'Toms', grp: 'Toms' }),
];
const MOLDE = !!process.env.MOLDE;   // MOLDE=1: set com faixa-molde MAESTRO (com o plugin) -> buses nascem como cópia dela
if (MOLDE) tracks.push(mk('Maestro', { devices: 1, clips: [], mon: 1, inp: 'Ext. In' }));
const REAIS = !!process.env.REAIS;   // REAIS=1: faixas de verdade chamadas "KICK" (áudio) e "BASS" (MIDI) -> são membros, não buses
if (REAIS) tracks.push(mk('KICK'), mk('BASS', { has_midi_input: 1 }));
const BK = REAIS ? 'KICK BUS' : 'KICK', BB = REAIS ? 'BASS BUS' : 'BASS';
const percs = tracks.find((t) => t.name === 'Percs'); tracks.forEach((t) => { if (t.is_grouped) t.pai = t.grp ? tracks.find((o) => o.name === t.grp).id : percs.id; });
const orig = JSON.parse(JSON.stringify(tracks));
const corClip = (n) => { const t = tracks.find((x) => x.name === n); return t.cl.map((c) => c.color).concat(t.slots.filter((s) => s.clip).map((s) => s.clip.color)); };

const byId = (id) => tracks.find((t) => t.id === id);
const allClips = () => tracks.flatMap((t) => t.cl.concat(t.slots.filter((s) => s.clip).map((s) => s.clip)));
const saidas = (t) => [{ display_name: 'Master', identifier: 1 }]
  .concat(t.is_grouped ? [{ display_name: t.grp || 'Percs', identifier: 2 }] : [])
  .concat(tracks.filter((o) => o !== t && !o.is_foldable).map((o, k) => ({ display_name: o.name, identifier: 100 + o.id })));
const entradas = () => [{ display_name: 'Ext. In', identifier: 1 }, { display_name: 'Resampling', identifier: 2 }, { display_name: 'No Input', identifier: 3 }];

let setsRota = 0;
function resolve(p) {
  const w = p.split(' ');
  if (w[0] === 'id') return objs[+w[1]] && (objs[+w[1]].kind === 'slot' || allClips().includes(objs[+w[1]])) ? objs[+w[1]] : { kind: byId(+w[1]) ? 'track' : 'none', id: +w[1] };
  if (p === 'live_set') return { kind: 'set' };
  if (w[1] === 'tracks' && w.length === 3) return { kind: 'track', id: tracks[+w[2]].id };
  if (w[1] === 'tracks' && w[3] === 'arrangement_clips') return { kind: 'clip', t: byId(tracks[+w[2]].id), j: +w[4] };
  throw new Error('path ' + p);
}
class LiveAPI {
  constructor(p) { this.r = resolve(p); }
  goto(p) { this.r = resolve(p); }
  get id() { return ['track', 'clipobj', 'slot'].includes(this.r.kind) ? this.r.id : 0; }
  getcount(k) {
    if (this.r.kind === 'set') return tracks.length;
    const t = byId(this.r.id);
    if (k === 'arrangement_clips') return t.clips.length;
    if (k === 'devices') return t.devices;
    throw new Error('getcount ' + k);
  }
  call(fn, a) {
    if (fn === 'create_audio_track') { tracks.splice(a, 0, mk(String(tracks.length + 1) + '-Audio', { clips: [] })); return; }
    if (fn === 'delete_track') { tracks.splice(a, 1); return; }
    if (fn === 'duplicate_track') { const o = tracks[a]; tracks.splice(a + 1, 0, mk(o.name, { color: o.color, out: o.out, inp: o.inp, mon: o.mon, devices: o.devices, clips: o.clips.slice(), semSessao: 1 })); return; }
    throw new Error('call ' + fn);
  }
  get(k) {
    if (this.r.kind === 'set') return [{ signature_numerator: 4 }[k]];
    if (this.r.kind === 'clip') return [this.r.t.clips[this.r.j][k === 'start_time' ? 0 : 1]];
    if (this.r.kind === 'clipobj') { if (k === 'color') return [this.r.color]; throw new Error('get clip ' + k); }
    if (this.r.kind === 'slot') { if (k === 'has_clip') return [this.r.clip ? 1 : 0]; if (k === 'clip') return ['id', this.r.clip ? this.r.clip.id : 0]; throw new Error('get slot ' + k); }
    const t = byId(this.r.id);
    if (k === 'output_routing_type') return [JSON.stringify({ output_routing_type: { display_name: t.out, identifier: 0 } })];
    if (k === 'input_routing_type') return [JSON.stringify({ input_routing_type: { display_name: t.inp, identifier: 0 } })];
    if (k === 'available_output_routing_types') return [JSON.stringify({ available_output_routing_types: saidas(t) })];
    if (k === 'available_input_routing_types') return [JSON.stringify({ available_input_routing_types: entradas() })];
    if (k === 'group_track') return ['id', t.pai || 0];
    if (k === 'arrangement_clips') return t.cl.flatMap((c) => ['id', c.id]);
    if (k === 'clip_slots') return t.slots.flatMap((c) => ['id', c.id]);
    const m = { name: t.name, color: t.color, is_foldable: t.is_foldable, is_grouped: t.is_grouped, has_midi_input: t.has_midi_input, current_monitoring_state: t.mon };
    if (k in m) return [m[k]];
    throw new Error('get ' + k);
  }
  set(k, ...v) {
    if (this.r.kind === 'clipobj') { if (k === 'color') { this.r.color = v[0]; return; } throw new Error('set clip ' + k); }
    const t = byId(this.r.id);
    if (k === 'name') { t.name = String(v[0]); return; }
    if (k === 'color') { t.color = v[0]; return; }
    if (k === 'current_monitoring_state') { t.mon = v[0]; return; }
    if (k === 'output_routing_type' || k === 'input_routing_type') {
      setsRota++;
      // o Live falso só entende UM formato (FMT): 0 objeto, 1 JSON {identifier}, 2 JSON {prop:{identifier}}, 3 "identifier", id
      let id = null;
      if (FMT === 0 && typeof v[0] === 'object') id = v[0].identifier;
      if (FMT === 1 && typeof v[0] === 'string' && v.length === 1) { try { const j = JSON.parse(v[0]); if (j.identifier != null) id = j.identifier; } catch (e) {} }
      if (FMT === 2 && typeof v[0] === 'string' && v.length === 1) { try { const j = JSON.parse(v[0]); if (j[k]) id = j[k].identifier; } catch (e) {} }
      if (FMT === 3 && v[0] === 'identifier') id = v[1];
      if (id == null) throw new Error('formato não aceito');
      const lst = k === 'output_routing_type' ? saidas(t) : entradas();
      const a = lst.find((x) => x.identifier === id);
      if (!a) throw new Error('identifier inválido');
      if (k === 'output_routing_type') t.out = a.display_name; else t.inp = a.display_name;
      return;
    }
    throw new Error('set ' + k);
  }
}

const LOGDIR = path.join(os.tmpdir(), 'espanador-sim'); fs.rmSync(LOGDIR, { recursive: true, force: true }); fs.mkdirSync(LOGDIR, { recursive: true });
class File {
  constructor(p, mode) { this.p = path.join(LOGDIR, path.basename(p)); if (mode === 'write') fs.writeFileSync(this.p, ''); this.isopen = true; this.position = 0; }
  get eof() { return fs.existsSync(this.p) ? fs.statSync(this.p).size : 0; } set eof(v) { fs.truncateSync(this.p, v); }
  writestring(x) { fs.appendFileSync(this.p, x); } readstring() { return ''; } close() {}
}
let dicts = {};
let motor;
const outs = [];
const ctx = {
  LiveAPI, File,
  Dict: class { constructor(n) { this.n = n; } stringify() { return dicts[this.n] || '{}'; } parse(s) { dicts[this.n] = s; } },
  Task: class { constructor(fn) { this.fn = fn; this.interval = 0; } schedule(ms) { this.h = setTimeout(this.fn, ms > 1000 ? ms : 0); }
    repeat() { this.h = setInterval(this.fn, this.interval); } cancel() { clearTimeout(this.h); clearInterval(this.h); } },
  outlet: (n, ...a) => { if (n === 1 && process.env.VERBOSO) console.log('STATUS:', a[1]); if (n === 2 && a[0] !== 'status') outs.push(a); if (n === 2 && a[0] === 'status') ctx._st = a[1]; },
  post: (s) => { if (process.env.VERBOSO || /erro|NÃO|não/i.test(s)) process.stdout.write(s); },
  patcher: { filepath: 'C:/Users/teste/Documents/Ableton/User Library/Presets/Espanador-0.1.0/Espanador.amxd' },
  max: { os: 'windows', version: '9.0.5', launchbrowser: () => {} }, arrayfromargs: (a) => Array.prototype.slice.call(a),
  BigInt, escape, unescape, encodeURIComponent, decodeURIComponent, parseInt, XMLHttpRequest: class {},
  Float32Array, Math, JSON, Date, Number, String, parseFloat, isNaN, Array, Infinity, isFinite,
};
vm.createContext(ctx);
vm.runInContext(fs.readFileSync(process.env.MOTOR || path.join(__dirname, '../../tmp/build-esp/Espanador-0.1.0/espanador.js'), 'utf8'), ctx);
motor = ctx;
const esperaLivre = (fn) => { const h = setInterval(() => { if (!vm.runInContext('busy', ctx)) { clearInterval(h); setTimeout(fn, 400); } }, 50); };
let erros = 0;
const confere = (ok, txt) => { console.log((ok ? '  ok   ' : '  ERRO ') + txt); if (!ok) erros++; };
const nome = (n) => tracks.find((t) => t.name === n);

motor.init();
confere(vm.runInContext('licLiberada()', ctx), 'TESTE abre sem licença');
motor.scan();
esperaLivre(() => {
  const d = JSON.parse(dicts.espanador_data);
  console.log('SCAN (AUTO pelo nome):');
  d.tracks.forEach((t) => console.log(`  ${t.name.padEnd(18)} ${t.fold ? 'GROUP' : '     '} ${t.member ? 'membro' : '      '} g=${t.g >= 0 ? d.grupos[t.g] : '-'}${t.auto ? ' (auto)' : ''} saída ${t.out} clips ${t.clips.length}`));
  const g = (n) => { const t = d.tracks.find((x) => x.name === n); return t.g >= 0 ? d.grupos[t.g] : '-'; };
  confere(g('01 Kick') === 'KICK' && g('08 Bass FM') === 'BASS' && g('05 HH Closed') === 'HATS' && g('11 Vox Lead') === 'VOCAL' && g('12 Riser') === 'FX' && g('10 Pad') === 'AMBIENCE' && g('09 Lead Acid') === 'SYNTHS', 'AUTO pelo nome = regras do MAESTRO');
  confere(g('13 Coisa Sem Nome') === '-' && g('Percs') === '-', 'sem nome conhecido / Group Track ficam sem grupo');
  confere(d.tracks[0].clips.length === 2 && d.tracks[0].clips[0][1] === 48, 'clips do arranjo no dict');
  // janela: o usuário põe "13 Coisa Sem Nome" no FX e tira o Pad do grupo
  d.tracks.find((x) => x.name === '13 Coisa Sem Nome').g = 7;
  d.tracks.find((x) => x.name === '10 Pad').g = -1;
  dicts.espanador_data = JSON.stringify(d);
  motor.marcas();
  motor.ui_prefix(1);
  motor.apply();
  esperaLivre(() => {
    console.log('APPLY (formato ' + FMT + '):');
    tracks.forEach((t) => console.log(`  ${t.name.padEnd(22)} cor ${t.color.toString(16).padStart(6, '0')} saída ${t.out.padEnd(10)} entrada ${t.inp} mon ${t.mon}`));
    const busD = nome('DRUMS BUS');
    confere(!!busD && busD.mon === 0 && busD.inp === 'No Input', 'bus DRUMS criado como "DRUMS BUS" (já existe Group Track DRUMS), Monitor IN, sem entrada');
    confere([BK, BB, 'HATS', 'SYNTHS', 'VOCAL', 'FX'].every((n) => nome(n) && nome(n).mon === 0), 'buses ' + BK + '/' + BB + '/HATS/SYNTHS/VOCAL/FX criados');
    if (REAIS) {
      confere(nome('KICK') && nome('KICK').out === 'KICK BUS' && nome('KICK').mon === 1 && nome('KICK').inp === 'Ext. In', 'REAIS: faixa "KICK" com clips é membro do KICK (saída no KICK BUS, Monitor/entrada intactos)');
      confere(nome('BASS') && nome('BASS').out === 'BASS BUS' && nome('BASS').mon === 1, 'REAIS: faixa MIDI "BASS" é membro do BASS');
    }
    confere(!nome('AMBIENCE'), 'grupo sem faixa não ganha bus (Pad tirado do grupo)');
    const ordem = tracks.slice(0, 7).map((t) => t.name).join(',');
    const ORD = [BK, BB, 'DRUMS BUS', 'HATS', 'SYNTHS', 'VOCAL', 'FX'];
    if (!MOLDE) confere(ordem === ORD.join(','), 'buses no topo na ordem do MAESTRO (' + ordem + ')');
    else {
      const im = tracks.findIndex((t) => t.name === BK), apos = tracks.slice(im, im + 7).map((t) => t.name).join(',');
      confere(apos === ORD.join(','), 'MOLDE: buses juntos, na ordem do MAESTRO (' + apos + ')');
      confere(ORD.every((n) => nome(n).devices === 1 && nome(n).inp === 'No Input'), 'MOLDE: todo bus novo veio com o plugin da faixa-molde');
      confere(!nome('Maestro'), 'MOLDE: faixa-molde apagada no fim do APPLY');
    }
    confere(nome('KICK 01 Kick') && nome('KICK 01 Kick').out === BK && nome('KICK 01 Kick').color === 0xff8a3d, 'Kick: prefixo, cor e saída no bus KICK');
    confere(nome('DRUMS 03 Snare').out === 'DRUMS BUS', 'Snare vai para o DRUMS BUS');
    confere(nome('DRUMS 06 Conga').out === 'Percs' && nome('DRUMS 06 Conga').color === nome('DRUMS 03 Snare').color, 'Conga (dentro do Group Percs) ganha cor/prefixo mas continua saindo no grupo');
    confere(nome('Percs').out === 'Master' && nome('HATS 07 Shaker').out === 'Percs', 'Group Track Percs (DRUMS + HATS misturados) não muda de saída');
    confere(nome('Toms').out === 'DRUMS BUS' && nome('Toms').color === nome('DRUMS 03 Snare').color && nome('DRUMS 14 Tom Hi').out === 'Toms', 'Group Track Toms (só DRUMS) vai inteiro para o DRUMS BUS, com a cor, sem renomear; os toms seguem no grupo');
    confere(nome('FX 13 Coisa Sem Nome').out === 'FX', 'marca feita na janela vale no APPLY');
    confere(nome('10 Pad').out === 'Master' && nome('10 Pad').color === 0x808080, 'faixa sem grupo não muda');
    confere(corClip('KICK 01 Kick').length === 3 && corClip('KICK 01 Kick').every((c) => c === 0xff8a3d), 'CLIPS ON: 2 clips do arranjo + 1 da session do Kick na cor do KICK');
    confere(corClip('DRUMS 06 Conga').every((c) => c === nome('DRUMS 03 Snare').color), 'CLIPS: faixa dentro de Group Track também tem os clips pintados');
    confere(corClip('10 Pad').every((c) => c === 0x123456 || c === 0x654321), 'CLIPS: faixa sem grupo não muda os clips');
    const prog = outs.filter((o) => o[0] === 'progresso').map((o) => o[3]);
    confere(prog.every((s, i) => i === 0 || s > prog[i - 1]), 'APPLY de cima para baixo (' + prog.join(',') + ')');
    const d2 = JSON.parse(dicts.espanador_data);
    confere(!d2.tracks.some((t) => t.name === 'Maestro' && t.g >= 0), 'faixa-molde nunca entra em grupo');
    confere(d2.tracks.filter((t) => t.bus >= 0).length === 7, 'SCAN depois do APPLY reconhece os 7 buses');
    // 2º APPLY: não cria bus de novo nem dobra o prefixo
    const n0 = tracks.length;
    motor.apply();
    esperaLivre(() => {
      confere(tracks.length === n0, '2º APPLY reaproveita os buses (' + tracks.length + ' faixas)');
      confere(!!nome('KICK 01 Kick') && !tracks.some((t) => /^KICK KICK/.test(t.name)), '2º APPLY não dobra o prefixo');
      // MAESTRO já no bus KICK: o UNDO não pode apagar
      nome(BK).devices = MOLDE ? 2 : 1;
      motor.undo();
      esperaLivre(() => {
        console.log('UNDO:');
        tracks.forEach((t) => console.log(`  ${t.name.padEnd(22)} saída ${t.out}`));
        const volta = orig.filter((o) => o.name !== 'Maestro').every((o) => { const t = byId(o.id); return t && t.name === o.name && t.color === o.color && t.out === o.out; });
        confere(volta, 'UNDO voltou nome, cor e saída de todas as faixas');
        const clipsVoltaram = orig.every((o) => { const t = byId(o.id); return !t || (t.cl.every((c, j) => c.color === o.cl[j].color) && t.slots.every((s, j) => !s.clip || s.clip.color === o.slots[j].clip.color)); });
        confere(clipsVoltaram, 'UNDO voltou a cor de todos os clips (arranjo e session)');
        confere(!!nome(BK) && !nome(BB) && !nome('DRUMS BUS'), 'UNDO apagou os buses vazios e manteve o ' + BK + ' (tem device)');
        if (MOLDE) { const mo = nome('Maestro'); confere(!!mo && mo.devices === 1 && mo.color === 0x808080 && mo.mon === 1 && mo.inp === 'Ext. In', 'MOLDE: UNDO devolveu a faixa-molde (com o plugin, cor, Monitor e entrada)'); }
        const lg = fs.readFileSync(path.join(LOGDIR, 'ESPANADOR-log.txt'), 'utf8');
        confere(lg.length > 200, 'log em arquivo (' + lg.split('\n').length + ' linhas)');
        console.log(erros ? '\nRESULTADO: ' + erros + ' ERROS' : '\nRESULTADO: OK');
        process.exit(erros ? 1 : 0);
      });
    });
  });
});
