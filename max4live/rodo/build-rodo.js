// Gera Rodo.amxd (Max for Live Audio Effect, não congelado) + Rodo.maxpat + rodo.js / rodo_view.js / rodo_strip.js
// (com o código comum de max4live/_comum: licença, tela de ativação, faixa do device).
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

// controles do Live (ESCONDIDOS: sem presentation). Só guardam os valores no set; a janela mexe neles pelo eco do motor.
const dial = box({ maxclass: 'live.dial', varname: 'target', numinlets: 1, numoutlets: 2,
  outlettype: ['', 'float'], parameter_enable: 1,
  patching_rect: [500, 100, 44, 48],
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
function tab(varname, longname, opcoes, inicial, msgName, outRoute) {
  const t = box({ maxclass: 'live.tab', varname, num_lines_patching: 1, num_lines_presentation: 1,
    numinlets: 1, numoutlets: 3, outlettype: ['', '', 'float'], parameter_enable: 1,
    patching_rect: [px, 100 + ty, 140, 20],
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
tab('mode', 'Mode', ['CLIP', 'TRACK', 'ALL'], 1, 'mode', 1);
tab('scope', 'Scope', ['ALL', 'SELECTED'], 0, 'scope', 2);

const send = obj('s ---rodo_ref', 1, 0, [370, 290, 110, 22]);
line(v8, 2, send, 0);

// faixa do device (arte + OPEN THE MAGIC) e janela flutuante
const comum = require('../_comum/m4l_comum.js');
comum.faixaEJanela({ box, obj, msg, line, v8, dev: 'rodo', NOME: 'RODO', boxes });

const patcher = {
  patcher: {
    fileversion: 1,
    appversion: { major: 9, minor: 0, revision: 0, architecture: 'x64', modernui: 1 },
    classnamespace: 'box',
    rect: [80, 80, 1200, 800],
    openinpresentation: 1,
    devicewidth: 420.0,
    default_fontsize: 10,
    default_fontname: 'Arial Bold',
    gridsize: [8, 8],
    boxes,
    lines,
    dependency_cache: comum.dependencias('rodo'),
    latency: 0,
    is_mpe: 0,
    minimum_live_version: '',
    minimum_max_version: '',
    platform_compatibility: 0,
    autosave: 0,
  },
};

const r = comum.escrever(out, __dirname, 'rodo', 'Rodo', patcher,
  { nome: 'RODO', tipo: 'rodo', cor: [0.96, 0.65, 0.14], frase: 'normaliza o pico dos clips' });
console.log('ok', out, r.bytes, 'bytes', 'v' + r.versao);