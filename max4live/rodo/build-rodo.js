// Gera Rodo.amxd (Max for Live Audio Effect, não congelado) + Rodo.maxpat. Molde: build-vassoura.js.
// node build-rodo.js <pasta-saida>
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
function line(a, ao, b, bi) { lines.push({ patchline: { source: [a, ao], destination: [b, bi] } }); }
const obj = (text, ni, no, rect, extra) =>
  box(Object.assign({ maxclass: 'newobj', text, numinlets: ni, numoutlets: no,
    outlettype: Array(no).fill(''), patching_rect: rect }, extra || {}));
const msg = (text, rect, extra) =>
  box(Object.assign({ maxclass: 'message', text, numinlets: 2, numoutlets: 1,
    outlettype: [''], patching_rect: rect }, extra || {}));

const FG = [0.92, 0.92, 0.92, 1];
const LARANJA = [0.96, 0.65, 0.14, 1];

// áudio passa direto (sem argumento = estéreo; "plugin~ 2" seria um canal só)
const pin = obj('plugin~', 2, 2, [20, 20, 70, 22], { outlettype: ['signal', 'signal'] });
const pout = obj('plugout~', 2, 0, [20, 400, 70, 22]);
line(pin, 0, pout, 0);
line(pin, 1, pout, 1);

// motor
const v8 = obj('v8 rodo.js', 1, 4, [200, 260, 160, 22]);
const dev = box({ maxclass: 'newobj', text: 'live.thisdevice', numinlets: 1, numoutlets: 3,
  outlettype: ['bang', 'int', 'int'], patching_rect: [200, 20, 90, 22] });
const tDev = obj('t b b', 1, 2, [200, 50, 40, 22], { outlettype: ['bang', 'bang'] });
const mInit = msg('init', [200, 80, 35, 22]);
line(dev, 0, tDev, 0);
line(tDev, 0, mInit, 0);
line(mInit, 0, v8, 0);
const rCmd = obj('r ---rodo_cmd', 0, 1, [60, 230, 110, 22]);
line(rCmd, 0, v8, 0);

const buf = obj('buffer~ rodo_scan', 1, 2, [200, 320, 140, 22], { outlettype: ['float', 'bang'] });
const mLoaded = msg('loaded', [360, 350, 50, 22]);
line(v8, 0, buf, 0);
line(buf, 1, mLoaded, 0);
line(mLoaded, 0, v8, 0);

// eco da janela para os controles do device
const rEco = obj('route target mode scope', 1, 4, [200, 300, 150, 22]);
line(v8, 3, rEco, 0);

// controles (presentation) — device 420 px de largura, como a VASSOURA
box({ maxclass: 'comment', text: 'RODO', fontname: 'Arial Bold', fontsize: 16, textcolor: LARANJA,
  numinlets: 1, numoutlets: 0, patching_rect: [500, 20, 100, 22],
  presentation: 1, presentation_rect: [6, 4, 100, 22] });

const open = box({ maxclass: 'live.text', text: 'OPEN', texton: 'OPEN', mode: 0, varname: 'open',
  numinlets: 1, numoutlets: 2, outlettype: ['', ''], parameter_enable: 1,
  patching_rect: [500, 60, 60, 18], presentation: 1, presentation_rect: [356, 6, 58, 18],
  saved_attribute_attributes: { valueof: { parameter_longname: 'Open', parameter_shortname: 'Open',
    parameter_type: 2, parameter_enum: ['val1', 'val2'], parameter_mmax: 1, parameter_invisible: 1 } } });

// TARGET: dial -> "prepend target" -> motor; "set" vindo da janela não sai de novo
const dial = box({ maxclass: 'live.dial', varname: 'target', numinlets: 1, numoutlets: 2,
  outlettype: ['', 'float'], parameter_enable: 1,
  patching_rect: [500, 100, 44, 48], presentation: 1, presentation_rect: [10, 30, 70, 52],
  saved_attribute_attributes: { valueof: { parameter_longname: 'Target', parameter_shortname: 'TARGET',
    parameter_type: 0, parameter_mmin: -12.0, parameter_mmax: -0.3, parameter_initial_enable: 1,
    parameter_initial: [-6.0], parameter_unitstyle: 4 } } });
const pTarget = obj('prepend target', 1, 1, [500, 160, 110, 22]);
line(dial, 0, pTarget, 0);
line(pTarget, 0, v8, 0);
line(tDev, 1, dial, 0);
const sTarget = obj('prepend set', 1, 1, [560, 130, 80, 22]);
line(rEco, 0, sTarget, 0);
line(sTarget, 0, dial, 0);

let px = 650, ty = 0;
function tab(varname, longname, opcoes, inicial, rect, msgName, outRoute, labelRect) {
  box({ maxclass: 'comment', text: longname.toUpperCase(), fontname: 'Arial Bold', fontsize: 9, textcolor: [0.7, 0.7, 0.72, 1],
    numinlets: 1, numoutlets: 0, patching_rect: [px, 60 + ty, 50, 18], presentation: 1, presentation_rect: labelRect });
  const t = box({ maxclass: 'live.tab', varname, num_lines_patching: 1, num_lines_presentation: 1,
    numinlets: 1, numoutlets: 3, outlettype: ['', '', 'float'], parameter_enable: 1,
    patching_rect: [px, 100 + ty, 140, 20], presentation: 1, presentation_rect: rect,
    saved_attribute_attributes: { valueof: { parameter_longname: longname, parameter_shortname: longname,
      parameter_type: 2, parameter_enum: opcoes, parameter_mmax: opcoes.length - 1,
      parameter_initial_enable: 1, parameter_initial: [inicial] } } });
  const p = obj('prepend ' + msgName, 1, 1, [px, 160 + ty, 110, 22]);
  line(t, 0, p, 0);
  line(p, 0, v8, 0);
  line(tDev, 1, t, 0);
  const s = obj('prepend set', 1, 1, [px + 150, 130 + ty, 80, 22]);
  line(rEco, outRoute, s, 0);
  line(s, 0, t, 0);
  ty += 70;
}
tab('mode', 'Mode', ['CLIP', 'TRACK', 'ALL'], 1, [140, 34, 150, 18], 'mode', 1, [96, 35, 40, 18]);
tab('scope', 'Scope', ['ALL', 'SELECTED'], 0, [140, 58, 150, 18], 'scope', 2, [96, 59, 40, 18]);

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
botaoDev('analyze', 'ANALYZE', [300, 32, 114, 20], 'analyze', 780);
botaoDev('apply', 'APPLY', [300, 56, 114, 24], 'apply', 880);
botaoDev('reset', 'RESET', [300, 86, 114, 20], 'reset', 980);

const st = msg('aguardando o Live...', [200, 500, 300, 22], {
  ignoreclick: 1, bgcolor: [0.16, 0.16, 0.17, 1], textcolor: FG, fontsize: 11,
  presentation: 1, presentation_rect: [6, 118, 408, 22] });
line(v8, 1, st, 0);

const send = obj('s ---rodo_ref', 1, 0, [370, 290, 110, 22]);
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
    { box: { id: 'obj-3', maxclass: 'newobj', text: 'r ---rodo_ref', numinlets: 0, numoutlets: 1, outlettype: [''], patching_rect: [140, 20, 110, 22] } },
    { box: { id: 'obj-5', maxclass: 'newobj', text: 's ---rodo_cmd', numinlets: 1, numoutlets: 0, patching_rect: [20, 980, 110, 22] } },
    { box: { id: 'obj-4', maxclass: 'jsui', filename: 'rodo_view.js', numinlets: 1, numoutlets: 1, outlettype: [''],
      parameter_enable: 0, patching_rect: [20, 120, 1360, 840], presentation: 1, presentation_rect: [0, 0, 1360, 840] } },
  ],
  lines: [
    { patchline: { source: ['obj-1', 0], destination: ['obj-2', 0] } },
    { patchline: { source: ['obj-3', 0], destination: ['obj-4', 0] } },
    { patchline: { source: ['obj-4', 0], destination: ['obj-5', 0] } },
  ],
};
const pj = box({ maxclass: 'newobj', text: 'p RODO', numinlets: 1, numoutlets: 0,
  patching_rect: [500, 260, 80, 22], patcher: sub });
const tOpen = obj('t b', 1, 1, [500, 200, 30, 22], { outlettype: ['bang'] });
const mOpen = msg('window flags float, window flags nogrow, window size 80 80 1440 920, window exec, title RODO, front',
  [500, 230, 520, 22]);
line(open, 0, tOpen, 0);
line(tOpen, 0, mOpen, 0);
line(mOpen, 0, pj, 0);
line(tOpen, 0, send, 0);  // janela recarrega o dict ao abrir

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
      { name: 'rodo.js', type: 'TEXT', implicit: 1 },
      { name: 'rodo_view.js', type: 'TEXT', implicit: 1 },
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
fs.writeFileSync(path.join(out, 'Rodo.amxd'), amxd);
fs.writeFileSync(path.join(out, 'Rodo.maxpat'), JSON.stringify(patcher, null, '\t'));
for (const f of ['rodo.js', 'rodo_view.js']) fs.copyFileSync(path.join(__dirname, f), path.join(out, f));
console.log('ok', out, amxd.length, 'bytes');
