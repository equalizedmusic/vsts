// Gera Vassoura.amxd (Max for Live Audio Effect, não congelado) + Vassoura.maxpat + vassoura.js / vassoura_view.js /
// vassoura_strip.js (com o código comum de max4live/_comum: licença, tela de ativação, faixa do device).
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
// sem argumento = estéreo (canais 1 e 2). "plugin~ 2" é UM canal só (o 2): o Max apagava o fio do esquerdo.
const pin = obj('plugin~', 2, 2, [20, 20, 70, 22], { outlettype: ['signal', 'signal'] });
const pout = obj('plugout~', 2, 0, [20, 400, 70, 22]);
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

// controles do Live (ESCONDIDOS: sem presentation). Só guardam os valores no set e mandam ao motor ao carregar;
// a janela mexe neles pelo eco do motor (saída 3 -> route -> prepend set).
const rEco = obj('route mode snap scope threshold minlength pre post', 1, 8, [200, 300, 300, 22]);
line(v8, 3, rEco, 0);
let px = 500;
function eco(alvo, saida) {
  const s = obj('prepend set', 1, 1, [px + 60, 130, 80, 22]);
  line(rEco, saida, s, 0);
  line(s, 0, alvo, 0);
}
function dial(varname, longname, shortname, mmin, mmax, initial, unitstyle, msgName, exponent, saidaEco) {
  const d = box({ maxclass: 'live.dial', varname, numinlets: 1, numoutlets: 2,
    outlettype: ['', 'float'], parameter_enable: 1,
    patching_rect: [px, 100, 44, 48],
    saved_attribute_attributes: { valueof: Object.assign({ parameter_longname: longname, parameter_shortname: shortname,
      parameter_type: 0, parameter_mmin: mmin, parameter_mmax: mmax, parameter_initial_enable: 1,
      parameter_initial: [initial], parameter_unitstyle: unitstyle }, exponent ? { parameter_exponent: exponent } : {}) } });
  const p = obj('prepend ' + msgName, 1, 1, [px, 160, 110, 22]);
  line(d, 0, p, 0);
  line(p, 0, v8, 0);
  line(tDev, 1, d, 0);
  eco(d, saidaEco);
  px += 130;
}
dial('threshold', 'Threshold', 'THRESHOLD', -90.0, -20.0, -60.0, 4, 'threshold', 0, 3);
dial('minlength', 'Min Length', 'MIN LENGTH', 100.0, 10000.0, 1000.0, 2, 'minlength', 2.0, 4);
dial('pre', 'Pre', 'PRE', 0.0, 1000.0, 10.0, 2, 'prems', 2.0, 5);
dial('post', 'Post', 'POST', 0.0, 2000.0, 100.0, 2, 'postms', 2.0, 6);

let ty = 0;
function tab(varname, longname, opcoes, msgName, saidaEco) {
  const t = box({ maxclass: 'live.tab', varname, num_lines_patching: 1, num_lines_presentation: 1,
    numinlets: 1, numoutlets: 3, outlettype: ['', '', 'float'], parameter_enable: 1,
    patching_rect: [px, 100 + ty, 140, 20],
    saved_attribute_attributes: { valueof: { parameter_longname: longname, parameter_shortname: longname,
      parameter_type: 2, parameter_enum: opcoes, parameter_mmax: opcoes.length - 1,
      parameter_initial_enable: 1, parameter_initial: [0] } } });
  const p = obj('prepend ' + msgName, 1, 1, [px, 160 + ty, 110, 22]);
  line(t, 0, p, 0);
  line(p, 0, v8, 0);
  line(tDev, 1, t, 0);
  const s = obj('prepend set', 1, 1, [px + 150, 130 + ty, 80, 22]);
  line(rEco, saidaEco, s, 0);
  line(s, 0, t, 0);
  ty += 70;
}
tab('mode', 'Mode', ['MUTE', 'CUT'], 'mode', 0);
tab('snap', 'Snap', ['OFF', '1/16', 'BAR'], 'snap', 1);
tab('scope', 'Scope', ['ALL', 'SELECTED'], 'scope', 2);

const send = obj('s ---vassoura_ref', 1, 0, [370, 290, 110, 22]);
line(v8, 2, send, 0);

// faixa do device (arte + OPEN THE MAGIC) e janela flutuante
const comum = require('../_comum/m4l_comum.js');
comum.faixaEJanela({ box, obj, msg, line, v8, dev: 'vassoura', NOME: 'VASSOURA', boxes });

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
    dependency_cache: comum.dependencias('vassoura'),
    latency: 0,
    is_mpe: 0,
    minimum_live_version: '',
    minimum_max_version: '',
    platform_compatibility: 0,
    autosave: 0,
  },
};

const r = comum.escrever(out, __dirname, 'vassoura', 'Vassoura', patcher,
  { nome: 'VASSOURA', tipo: 'vassoura', cor: [0.40, 0.78, 0.95], frase: 'limpa os trechos vazios dos clips' });
console.log('ok', out, r.bytes, 'bytes', 'v' + r.versao);