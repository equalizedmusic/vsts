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
const v8 = obj('v8 vassoura.js', 1, 3, [200, 260, 160, 22]);
const dev = box({ maxclass: 'newobj', text: 'live.thisdevice', numinlets: 1, numoutlets: 3,
  outlettype: ['bang', 'int', 'int'], patching_rect: [200, 20, 90, 22] });
const mInit = msg('init', [200, 50, 35, 22]);
line(dev, 0, mInit, 0);
line(mInit, 0, v8, 0);

const buf = obj('buffer~ vassoura_scan', 1, 2, [200, 320, 140, 22], { outlettype: ['float', 'bang'] });
const mLoaded = msg('loaded', [360, 350, 50, 22]);
line(v8, 0, buf, 0);
line(buf, 1, mLoaded, 0);
line(mLoaded, 0, v8, 0);

// controles (presentation)
box({ maxclass: 'comment', text: 'VASSOURA', fontname: 'Arial Bold', fontsize: 14, textcolor: FG,
  numinlets: 1, numoutlets: 0, patching_rect: [500, 20, 100, 22],
  presentation: 1, presentation_rect: [6, 4, 100, 22] });

const open = box({ maxclass: 'live.text', text: 'OPEN', texton: 'OPEN', mode: 0, varname: 'open',
  numinlets: 1, numoutlets: 2, outlettype: ['', ''], parameter_enable: 1,
  patching_rect: [500, 60, 60, 18], presentation: 1, presentation_rect: [150, 7, 54, 18],
  saved_attribute_attributes: { valueof: { parameter_longname: 'Open', parameter_shortname: 'Open',
    parameter_type: 2, parameter_enum: ['val1', 'val2'], parameter_mmax: 1, parameter_invisible: 1 } } });

const thr = box({ maxclass: 'live.dial', varname: 'threshold', numinlets: 1, numoutlets: 2,
  outlettype: ['', 'float'], parameter_enable: 1,
  patching_rect: [500, 100, 44, 48], presentation: 1, presentation_rect: [10, 36, 60, 48],
  saved_attribute_attributes: { valueof: { parameter_longname: 'Threshold', parameter_shortname: 'THRESHOLD',
    parameter_type: 0, parameter_mmin: -90.0, parameter_mmax: -20.0, parameter_initial_enable: 1,
    parameter_initial: [-60.0], parameter_unitstyle: 4 } } });
const pThr = obj('prepend threshold', 1, 1, [500, 160, 110, 22]);
line(thr, 0, pThr, 0);
line(pThr, 0, v8, 0);

const mlen = box({ maxclass: 'live.dial', varname: 'minlength', numinlets: 1, numoutlets: 2,
  outlettype: ['', 'float'], parameter_enable: 1,
  patching_rect: [640, 100, 44, 48], presentation: 1, presentation_rect: [80, 36, 60, 48],
  saved_attribute_attributes: { valueof: { parameter_longname: 'Min Length', parameter_shortname: 'MIN LENGTH',
    parameter_type: 0, parameter_mmin: 100.0, parameter_mmax: 10000.0, parameter_initial_enable: 1,
    parameter_initial: [1000.0], parameter_unitstyle: 2, parameter_exponent: 2.0 } } });
const pLen = obj('prepend minlength', 1, 1, [640, 160, 110, 22]);
line(mlen, 0, pLen, 0);
line(pLen, 0, v8, 0);

const an = box({ maxclass: 'live.text', text: 'ANALYZE', texton: 'ANALYZE', mode: 0, varname: 'analyze',
  numinlets: 1, numoutlets: 2, outlettype: ['', ''], parameter_enable: 1,
  patching_rect: [780, 100, 70, 18], presentation: 1, presentation_rect: [150, 44, 54, 30],
  saved_attribute_attributes: { valueof: { parameter_longname: 'Analyze', parameter_shortname: 'Analyze',
    parameter_type: 2, parameter_enum: ['val1', 'val2'], parameter_mmax: 1, parameter_invisible: 1 } } });
const tAn = obj('t b', 1, 1, [780, 130, 30, 22], { outlettype: ['bang'] });
const mAn = msg('analyze', [780, 160, 55, 22]);
line(an, 0, tAn, 0);
line(tAn, 0, mAn, 0);
line(mAn, 0, v8, 0);

const st = msg('aguardando o Live...', [200, 440, 300, 22], {
  ignoreclick: 1, bgcolor: [0.16, 0.16, 0.17, 1], textcolor: FG, fontsize: 9,
  presentation: 1, presentation_rect: [6, 120, 200, 22] });
line(v8, 1, st, 0);

const send = obj('s ---vassoura_ref', 1, 0, [370, 290, 110, 22]);
line(v8, 2, send, 0);

// janela flutuante
const sub = {
  fileversion: 1,
  appversion: { major: 9, minor: 0, revision: 0, architecture: 'x64', modernui: 1 },
  classnamespace: 'box',
  rect: [120, 120, 1220, 900],
  openinpresentation: 1,
  toolbarvisible: 0,
  boxes: [
    { box: { id: 'obj-1', maxclass: 'inlet', numinlets: 0, numoutlets: 1, outlettype: [''], patching_rect: [20, 20, 30, 30] } },
    { box: { id: 'obj-2', maxclass: 'newobj', text: 'thispatcher', numinlets: 1, numoutlets: 2, outlettype: ['', ''], patching_rect: [20, 70, 80, 22] } },
    { box: { id: 'obj-3', maxclass: 'newobj', text: 'r ---vassoura_ref', numinlets: 0, numoutlets: 1, outlettype: [''], patching_rect: [140, 20, 110, 22] } },
    { box: { id: 'obj-4', maxclass: 'jsui', filename: 'vassoura_view.js', numinlets: 1, numoutlets: 1, outlettype: [''],
      parameter_enable: 0, patching_rect: [20, 120, 1100, 780], presentation: 1, presentation_rect: [0, 0, 1100, 780] } },
  ],
  lines: [
    { patchline: { source: ['obj-1', 0], destination: ['obj-2', 0] } },
    { patchline: { source: ['obj-3', 0], destination: ['obj-4', 0] } },
  ],
};
const pj = box({ maxclass: 'newobj', text: 'p VASSOURA', numinlets: 1, numoutlets: 0,
  patching_rect: [500, 260, 80, 22], patcher: sub });
const tOpen = obj('t b', 1, 1, [500, 200, 30, 22], { outlettype: ['bang'] });
// janela do tamanho exato do jsui (1100x780), sem redimensionar, sempre por cima, título VASSOURA
const mOpen = msg('window flags float, window flags nogrow, window size 120 120 1220 900, window exec, title VASSOURA, front',
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
    devicewidth: 215.0,
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
