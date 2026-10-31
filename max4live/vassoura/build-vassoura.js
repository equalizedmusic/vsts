// Gera Vassoura.amxd (Max for Live Audio Effect, não congelado) + Vassoura.maxpat.
// node build-vassoura.js <pasta-saida>
const fs = require('fs');
const path = require('path');

const out = process.argv[2] || path.join(__dirname, 'dist');
fs.mkdirSync(out, { recursive: true });

let n = 0;
const boxes = [];
const lines = [];
function box(o) {
  const id = 'obj-' + (++n);
  boxes.push({ box: Object.assign({ id }, o) });
  return id;
}
function line(a, ao, b, bi) {
  lines.push({ patchline: { source: [a, ao], destination: [b, bi] } });
}
const obj = (text, ni, no, rect, extra) =>
  box(Object.assign({ maxclass: 'newobj', text, numinlets: ni, numoutlets: no,
    outlettype: Array(no).fill(''), patching_rect: rect }, extra || {}));
const msg = (text, rect, extra) =>
  box(Object.assign({ maxclass: 'message', text, numinlets: 2, numoutlets: 1,
    outlettype: [''], patching_rect: rect }, extra || {}));

const FG = [0.92, 0.92, 0.92, 1];

// áudio passa direto
const pin = obj('plugin~ 2', 2, 2, [20, 20, 70, 22], { outlettype: ['signal', 'signal'] });
const pout = obj('plugout~ 2', 2, 0, [20, 400, 70, 22]);
line(pin, 0, pout, 0);
line(pin, 1, pout, 1);

// motor
const v8 = obj('v8 vassoura.js', 1, 4, [200, 260, 160, 22]);
const dev = box({ maxclass: 'newobj', text: 'live.thisdevice', numinlets: 1, numoutlets: 3,
  outlettype: ['bang', 'int', 'int'], patching_rect: [200, 20, 90, 22] });
// ao carregar: primeiro os controles mandam o valor salvo (saída direita), depois init
const tDev = obj('t b b', 1, 2, [200, 50, 40, 22], { outlettype: ['bang', 'bang'] });
const mInit = msg('init', [200, 80, 35, 22]);
line(dev, 0, tDev, 0);
line(tDev, 0, mInit, 0);
line(mInit, 0, v8, 0);
// comandos da janela (jsui) chegam por aqui
const rCmd = obj('r ---vassoura_cmd', 0, 1, [60, 230, 110, 22]);
line(rCmd, 0, v8, 0);

const buf = obj('buffer~ vassoura_scan', 1, 2, [200, 320, 140, 22], { outlettype: ['float', 'bang'] });
const mLoaded = msg('loaded', [360, 350, 50, 22]);
line(v8, 0, buf, 0);
line(buf, 1, mLoaded, 0);
line(mLoaded, 0, v8, 0);

// controles (presentation)
box({ maxclass: 'comment', text: 'VASSOURA', fontname: 'Arial Bold', fontsize: 16, textcolor: FG,
  numinlets: 1, numoutlets: 0, patching_rect: [500, 20, 100, 22],
  presentation: 1, presentation_rect: [6, 4, 100, 22] });

const open = box({ maxclass: 'live.text', text: 'OPEN', texton: 'OPEN', mode: 0, varname: 'open',
  numinlets: 1, numoutlets: 2, outlettype: ['', ''], parameter_enable: 1,
  patching_rect: [500, 60, 60, 18], presentation: 1, presentation_rect: [356, 6, 58, 18],
  saved_attribute_attributes: { valueof: { parameter_longname: 'Open', parameter_shortname: 'Open',
    parameter_type: 2, parameter_enum: ['val1', 'val2'], parameter_mmax: 1, parameter_invisible: 1 } } });

// dial -> "prepend <msg>" -> motor; recebe bang no load para mandar o valor salvo
let px = 500;
function dial(varname, longname, shortname, mmin, mmax, initial, unitstyle, rect, msgName, exponent) {
  const d = box({ maxclass: 'live.dial', varname, numinlets: 1, numoutlets: 2,
    outlettype: ['', 'float'], parameter_enable: 1,
    patching_rect: [px, 100, 44, 48], presentation: 1, presentation_rect: rect,
    saved_attribute_attributes: { valueof: Object.assign({ parameter_longname: longname, parameter_shortname: shortname,
      parameter_type: 0, parameter_mmin: mmin, parameter_mmax: mmax, parameter_initial_enable: 1,
      parameter_initial: [initial], parameter_unitstyle: unitstyle }, exponent ? { parameter_exponent: exponent } : {}) } });
  const p = obj('prepend ' + msgName, 1, 1, [px, 160, 110, 22]);
  line(d, 0, p, 0);
  line(p, 0, v8, 0);
  line(tDev, 1, d, 0);
  px += 130;
  return d;
}
dial('threshold', 'Threshold', 'THRESHOLD', -90.0, -20.0, -60.0, 4, [6, 30, 58, 48], 'threshold');
dial('minlength', 'Min Length', 'MIN LENGTH', 100.0, 10000.0, 1000.0, 2, [66, 30, 58, 48], 'minlength', 2.0);
dial('pre', 'Pre', 'PRE', 0.0, 1000.0, 10.0, 2, [126, 30, 58, 48], 'prems', 2.0);
dial('post', 'Post', 'POST', 0.0, 2000.0, 100.0, 2, [186, 30, 58, 48], 'postms', 2.0);

// live.tab (MODE / SNAP / SCOPE): saída -> motor; o motor devolve "set" quando a escolha vem da janela
const rEco = obj('route mode snap scope', 1, 4, [200, 300, 140, 22]);
line(v8, 3, rEco, 0);
let ty = 0;
function tab(varname, longname, opcoes, rect, msgName, outRoute, labelRect) {
  box({ maxclass: 'comment', text: longname.toUpperCase(), fontname: 'Arial Bold', fontsize: 9, textcolor: [0.7, 0.7, 0.72, 1],
    numinlets: 1, numoutlets: 0, patching_rect: [px, 60 + ty, 50, 18], presentation: 1, presentation_rect: labelRect });
  const t = box({ maxclass: 'live.tab', varname, num_lines_patching: 1, num_lines_presentation: 1,
    numinlets: 1, numoutlets: 3, outlettype: ['', '', 'float'], parameter_enable: 1,
    patching_rect: [px, 100 + ty, 140, 20], presentation: 1, presentation_rect: rect,
    saved_attribute_attributes: { valueof: { parameter_longname: longname, parameter_shortname: longname,
      parameter_type: 2, parameter_enum: opcoes, parameter_mmax: opcoes.length - 1,
      parameter_initial_enable: 1, parameter_initial: [0] } } });
  const p = obj('prepend ' + msgName, 1, 1, [px, 160 + ty, 110, 22]);
  line(t, 0, p, 0);
  line(p, 0, v8, 0);
  line(tDev, 1, t, 0);
  const s = obj('prepend set', 1, 1, [px + 150, 130 + ty, 80, 22]);
  line(rEco, outRoute, s, 0);
  line(s, 0, t, 0);
  ty += 70;
  return t;
}
tab('mode', 'Mode', ['MUTE', 'CUT'], [292, 32, 122, 18], 'mode', 0, [250, 33, 40, 18]);
tab('snap', 'Snap', ['OFF', '1/16', 'BAR'], [292, 54, 122, 18], 'snap', 1, [250, 55, 40, 18]);
tab('scope', 'Scope', ['ALL', 'SELECTED'], [292, 76, 122, 18], 'scope', 2, [250, 77, 40, 18]);

function botaoDev(varname, label, rect, msgText, x) {
  const b = box({ maxclass: 'live.text', text: label, texton: label, mode: 0, varname,
    numinlets: 1, numoutlets: 2, outlettype: ['', ''], parameter_enable: 1,
    patching_rect: [x, 400, 70, 18], presentation: 1, presentation_rect: rect,
    saved_attribute_attributes: { valueof: { parameter_longname: label, parameter_shortname: label,
      parameter_type: 2, parameter_enum: ['val1', 'val2'], parameter_mmax: 1, parameter_invisible: 1 } } });
  const t = obj('t b', 1, 1, [x, 430, 30, 22], { outlettype: ['bang'] });
  const m = msg(msgText, [x, 460, 55, 22]);
  line(b, 0, t, 0);
  line(t, 0, m, 0);
  line(m, 0, v8, 0);
}
botaoDev('analyze', 'ANALYZE', [6, 88, 116, 24], 'analyze', 780);
botaoDev('apply', 'APPLY', [128, 88, 116, 24], 'apply', 880);

const st = msg('aguardando o Live...', [200, 500, 300, 22], {
  ignoreclick: 1, bgcolor: [0.16, 0.16, 0.17, 1], textcolor: FG, fontsize: 11,
  presentation: 1, presentation_rect: [6, 118, 408, 22] });
line(v8, 1, st, 0);

const send = obj('s ---vassoura_ref', 1, 0, [370, 290, 110, 22]);
line(v8, 2, send, 0);

// janela flutuante
const sub = {
  fileversion: 1,
  appversion: { major: 9, minor: 0, revision: 0, architecture: 'x64', modernui: 1 },
  classnamespace: 'box',
  rect: [80, 80, 1440, 920],
  openinpresentation: 1,
  toolbarvisible: 0,
  boxes: [
    { box: { id: 'obj-1', maxclass: 'inlet', numinlets: 0, numoutlets: 1, outlettype: [''], patching_rect: [20, 20, 30, 30] } },
    { box: { id: 'obj-2', maxclass: 'newobj', text: 'thispatcher', numinlets: 1, numoutlets: 2, outlettype: ['', ''], patching_rect: [20, 70, 80, 22] } },
    { box: { id: 'obj-3', maxclass: 'newobj', text: 'r ---vassoura_ref', numinlets: 0, numoutlets: 1, outlettype: [''], patching_rect: [140, 20, 110, 22] } },
    { box: { id: 'obj-5', maxclass: 'newobj', text: 's ---vassoura_cmd', numinlets: 1, numoutlets: 0, patching_rect: [20, 980, 110, 22] } },
    { box: { id: 'obj-4', maxclass: 'jsui', filename: 'vassoura_view.js', numinlets: 1, numoutlets: 1, outlettype: [''],
      parameter_enable: 0, patching_rect: [20, 120, 1360, 840], presentation: 1, presentation_rect: [0, 0, 1360, 840] } },
  ],
  lines: [
    { patchline: { source: ['obj-1', 0], destination: ['obj-2', 0] } },
    { patchline: { source: ['obj-3', 0], destination: ['obj-4', 0] } },
    { patchline: { source: ['obj-4', 0], destination: ['obj-5', 0] } },
  ],
};
const pj = box({ maxclass: 'newobj', text: 'p VASSOURA', numinlets: 1, numoutlets: 0,
  patching_rect: [500, 260, 80, 22], patcher: sub });
const tOpen = obj('t b', 1, 1, [500, 200, 30, 22], { outlettype: ['bang'] });
// janela do tamanho exato do jsui (1360x840), sem redimensionar, sempre por cima, título VASSOURA
const mOpen = msg('window flags float, window flags nogrow, window size 80 80 1440 920, window exec, title VASSOURA, front',
  [500, 230, 520, 22]);
line(open, 0, tOpen, 0);
line(tOpen, 0, mOpen, 0);
line(mOpen, 0, pj, 0);
line(tOpen, 0, send, 0);

const patcher = {
  patcher: {
    fileversion: 1,
    appversion: { major: 9, minor: 0, revision: 0, architecture: 'x64', modernui: 1 },
    classnamespace: 'box',
    rect: [80, 80, 1000, 560],
    openinpresentation: 1,
    devicewidth: 420.0,
    default_fontsize: 10,
    default_fontname: 'Arial Bold',
    gridsize: [8, 8],
    boxes,
    lines,
    dependency_cache: [
      { name: 'vassoura.js', type: 'TEXT', implicit: 1 },
      { name: 'vassoura_view.js', type: 'TEXT', implicit: 1 },
    ],
    latency: 0,
    is_mpe: 0,
    minimum_live_version: '',
    minimum_max_version: '',
    platform_compatibility: 0,
    autosave: 0,
  },
};

const json = Buffer.from(JSON.stringify(patcher, null, '\t') + '\n\0', 'utf8');
const u32 = (v) => { const b = Buffer.alloc(4); b.writeUInt32LE(v); return b; };
const amxd = Buffer.concat([
  Buffer.from('ampf'), u32(4), Buffer.from('aaaa'),
  Buffer.from('meta'), u32(4), u32(0),
  Buffer.from('ptch'), u32(json.length), json,
]);
fs.writeFileSync(path.join(out, 'Vassoura.amxd'), amxd);
fs.writeFileSync(path.join(out, 'Vassoura.maxpat'), JSON.stringify(patcher, null, '\t'));
for (const f of ['vassoura.js', 'vassoura_view.js']) fs.copyFileSync(path.join(__dirname, f), path.join(out, f));
console.log('ok', out, amxd.length, 'bytes');
