// Peças comuns dos builds dos devices (build-rodo.js, build-vassoura.js):
//  - faixa do device = jsui <dev>_strip.js (arte + OPEN THE MAGIC); os controles do Live ficam escondidos (sem
//    presentation), só para o set guardar os valores; a janela manda e recebe tudo pelo motor.
//  - janela flutuante: jsui <dev>_view.js + [dialog] para digitar e-mail e token da licença.
//  - arquivos: motor = <dev>.js + isp_licenca.js · janela = <dev>_view.js + isp_tela_lic.js · faixa = DEV + isp_strip.js
const fs = require('fs');
const path = require('path');
const COMUM = __dirname;

function faixaEJanela(k) {
  const { box, obj, msg, line, v8, dev, NOME, boxes } = k;
  // tira da tela do device tudo o que estava lá (controles continuam existindo, escondidos)
  for (const b of boxes) { delete b.box.presentation; delete b.box.presentation_rect; }

  const strip = box({ maxclass: 'jsui', filename: `${dev}_strip.js`, numinlets: 1, numoutlets: 1, outlettype: [''],
    parameter_enable: 0, patching_rect: [700, 520, 420, 166], presentation: 1, presentation_rect: [0, 0, 420, 166] });
  line(v8, 1, strip, 0);

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
      { box: { id: 'obj-3', maxclass: 'newobj', text: `r ---${dev}_ref`, numinlets: 0, numoutlets: 1, outlettype: [''], patching_rect: [140, 20, 110, 22] } },
      { box: { id: 'obj-4', maxclass: 'jsui', filename: `${dev}_view.js`, numinlets: 1, numoutlets: 1, outlettype: [''],
        parameter_enable: 0, patching_rect: [20, 120, 1360, 840], presentation: 1, presentation_rect: [0, 0, 1360, 840] } },
      { box: { id: 'obj-5', maxclass: 'newobj', text: `s ---${dev}_cmd`, numinlets: 1, numoutlets: 0, patching_rect: [20, 1100, 110, 22] } },
      // dlg_email / dlg_token abrem o diálogo; o resto vai direto para o motor
      { box: { id: 'obj-6', maxclass: 'newobj', text: 'route dlg_email dlg_token', numinlets: 1, numoutlets: 3, outlettype: ['', '', ''], patching_rect: [20, 980, 160, 22] } },
      { box: { id: 'obj-7', maxclass: 'newobj', text: 'dialog E-mail da compra', numinlets: 2, numoutlets: 1, outlettype: [''], patching_rect: [200, 1010, 150, 22] } },
      { box: { id: 'obj-8', maxclass: 'newobj', text: 'dialog Token ISP-XXXX-XXXX-XXXX-XXXX-XXXX', numinlets: 2, numoutlets: 1, outlettype: [''], patching_rect: [380, 1010, 250, 22] } },
      { box: { id: 'obj-9', maxclass: 'newobj', text: 'prepend lic_email', numinlets: 1, numoutlets: 1, outlettype: [''], patching_rect: [200, 1050, 110, 22] } },
      { box: { id: 'obj-10', maxclass: 'newobj', text: 'prepend lic_token', numinlets: 1, numoutlets: 1, outlettype: [''], patching_rect: [380, 1050, 110, 22] } },
    ],
    lines: [
      { patchline: { source: ['obj-1', 0], destination: ['obj-2', 0] } },
      { patchline: { source: ['obj-3', 0], destination: ['obj-4', 0] } },
      { patchline: { source: ['obj-4', 0], destination: ['obj-6', 0] } },
      { patchline: { source: ['obj-6', 0], destination: ['obj-7', 0] } },
      { patchline: { source: ['obj-6', 1], destination: ['obj-8', 0] } },
      { patchline: { source: ['obj-6', 2], destination: ['obj-5', 0] } },
      { patchline: { source: ['obj-7', 0], destination: ['obj-9', 0] } },
      { patchline: { source: ['obj-8', 0], destination: ['obj-10', 0] } },
      { patchline: { source: ['obj-9', 0], destination: ['obj-5', 0] } },
      { patchline: { source: ['obj-10', 0], destination: ['obj-5', 0] } },
    ],
  };
  const pj = box({ maxclass: 'newobj', text: `p ${NOME}`, numinlets: 1, numoutlets: 0,
    patching_rect: [700, 760, 80, 22], patcher: sub });
  // OPEN: primeiro o motor manda licença e valores para a janela, depois a janela abre e relê o dict
  const tOpen = obj('t b b', 1, 2, [700, 700, 40, 22], { outlettype: ['bang', 'bang'] });
  const mJanela = msg('janela', [760, 730, 50, 22]);
  const mOpen = msg(`window flags float, window flags nogrow, window size 80 80 1440 920, window exec, title ${NOME}, front`,
    [700, 730, 520, 22]);
  const sRef = obj(`s ---${dev}_ref`, 1, 0, [1000, 760, 110, 22]);
  line(strip, 0, tOpen, 0);
  line(tOpen, 1, mJanela, 0);
  line(mJanela, 0, v8, 0);
  line(tOpen, 0, mOpen, 0);
  line(mOpen, 0, pj, 0);
  line(tOpen, 0, sRef, 0);
}

function escrever(out, dir, dev, Nome, patcher, DEVJS) {
  const json = Buffer.from(JSON.stringify(patcher, null, '\t') + '\n\0', 'utf8');
  const u32 = (v) => { const b = Buffer.alloc(4); b.writeUInt32LE(v); return b; };
  const amxd = Buffer.concat([
    Buffer.from('ampf'), u32(4), Buffer.from('aaaa'),
    Buffer.from('meta'), u32(4), u32(0),
    Buffer.from('ptch'), u32(json.length), json,
  ]);
  fs.writeFileSync(path.join(out, `${Nome}.amxd`), amxd);
  fs.writeFileSync(path.join(out, `${Nome}.maxpat`), JSON.stringify(patcher, null, '\t'));
  const ler = (p) => fs.readFileSync(p, 'utf8').replace(/\r\n/g, '\n');
  const motor = ler(path.join(dir, `${dev}.js`));
  const versao = (/var VERSAO = "([^"]+)"/.exec(motor) || [])[1] || '?';
  fs.writeFileSync(path.join(out, `${dev}.js`), motor + '\n' + ler(path.join(COMUM, 'isp_licenca.js')));
  fs.writeFileSync(path.join(out, `${dev}_view.js`), ler(path.join(dir, `${dev}_view.js`)) + '\n' + ler(path.join(COMUM, 'isp_tela_lic.js')));
  fs.writeFileSync(path.join(out, `${dev}_strip.js`),
    '// ' + Nome.toUpperCase() + ' — faixa do device (gerado pelo build a partir de _comum/isp_strip.js)\n' +
    'var DEV = ' + JSON.stringify(Object.assign({ versao }, DEVJS)) + ';\n' + ler(path.join(COMUM, 'isp_strip.js')));
  return { bytes: amxd.length, versao };
}

const dependencias = (dev) => [
  { name: `${dev}.js`, type: 'TEXT', implicit: 1 },
  { name: `${dev}_view.js`, type: 'TEXT', implicit: 1 },
  { name: `${dev}_strip.js`, type: 'TEXT', implicit: 1 },
];

module.exports = { faixaEJanela, escrever, dependencias };
