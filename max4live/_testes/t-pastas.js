// 15/11: cenário do cliente (licença não ficava gravada; cada abertura = id de PC novo).
// Max falso com C:/Users/{Reginaldo,Public,Outro} e D:/; só Reginaldo e D: aceitam gravação.
const fs = require('fs'), vm = require('vm'), path = require('path');
const ARQ = process.argv[2];
const DEVN = /vassoura/.test(ARQ) ? 'vassoura' : 'rodo';
const RAIZ = require('os').tmpdir() + '/m4l-pastas-' + DEVN;
let erros = 0; const confere = (ok, t) => { console.log((ok ? '  ok   ' : '  ERRO ') + t); if (!ok) erros++; };
function montar(cenario) {
  fs.rmSync(RAIZ, { recursive: true, force: true });
  for (const u of ['Reginaldo', 'Public', 'Outro']) fs.mkdirSync(`${RAIZ}/C/Users/${u}/AppData/Roaming`, { recursive: true });
  fs.mkdirSync(`${RAIZ}/D/Ableton/User Library/Rodo`, { recursive: true });
  global.SO_LEITURA = cenario.soLeitura || [];
}
const mapa = (p) => p.replace(/^([A-Z]):/, (m, d) => RAIZ + '/' + d);
const podeGravar = (p) => !global.SO_LEITURA.some((r) => p.startsWith(r)) && fs.existsSync(path.dirname(mapa(p)));
function novo(filepath) {
  const base = require('./lic-mock.js')(RAIZ);
  class File {
    constructor(p, mode) {
      this.p = mapa(p); this.position = 0;
      if (mode === 'write') { this.isopen = podeGravar(p); if (this.isopen) fs.writeFileSync(this.p, ''); }
      else this.isopen = fs.existsSync(this.p);
    }
    get eof() { return fs.existsSync(this.p) ? fs.statSync(this.p).size : 0; } set eof(v) { if (this.isopen) fs.truncateSync(this.p, v); }
    writestring(x) { fs.appendFileSync(this.p, x); }
    readstring(n) { const b = fs.readFileSync(this.p, 'utf8').slice(this.position, this.position + n); this.position += Buffer.byteLength(b); return b; }
    close() {}
  }
  class Folder {
    constructor(p) { const q = mapa(p); this.l = fs.existsSync(q) ? fs.readdirSync(q, { withFileTypes: true }) : []; this.i = 0; }
    get end() { return this.i >= this.l.length; } get filename() { return this.l[this.i].name; }
    get filetype() { return this.l[this.i].isDirectory() ? 'fold' : 'TEXT'; } next() { this.i++; } close() {}
  }
  const logs = [];
  const ctx = { outlet: () => {}, post: (s) => logs.push(s), Task: class { constructor(f) { this.f = f; } schedule() {} repeat() {} cancel() {} },
    Dict: class { stringify() { return '{}'; } parse() {} }, LiveAPI: class {}, Math, JSON, Date, Number, String, parseFloat, isNaN, Array, Infinity, isFinite, Float32Array,
    patcher: { filepath }, ...base, File, Folder };
  ctx.max = Object.assign({}, base.max, { apppath: 'C:/ProgramData/Ableton/Live 12 Suite/Resources/Max/' });
  vm.createContext(ctx); vm.runInContext(fs.readFileSync(ARQ, 'utf8'), ctx);
  return { m: ctx, logs };
}
const espera = (ms) => new Promise((r) => setTimeout(r, ms));
const TOK = 'ISP-AAAA-BBBB-CCCC-DDDD-EEEE';
async function abreEAtiva(fp) { const a = novo(fp); a.m.init(); await espera(20); if (!a.m.licLiberada()) { a.m.lic_email('cliente@teste.com'); a.m.lic_token(TOK); a.m.lic_ativar(); await espera(40); } return a; }
(async () => {
  for (const [nome, fp, cen] of [
    ['filepath VAZIO (o caso do cliente)', '', {}],
    ['biblioteca no D: (sem /Users/ no caminho)', 'D:/Ableton/User Library/Rodo/Rodo.amxd', {}],
    ['biblioteca no D: e outros perfis sem permissão', 'D:/Ableton/User Library/Rodo/Rodo.amxd', { soLeitura: [] }],
    ['caminho normal (C:/Users/Reginaldo/Documents)', 'C:/Users/Reginaldo/Documents/Ableton/X/Rodo.amxd', {}],
  ]) {
    console.log('-- ' + nome);
    montar(Object.assign({ soLeitura: ['C:/Users/Public', 'C:/Users/Outro'] }, cen.soLeitura ? { soLeitura: ['C:/Users/Public', 'C:/Users/Outro'] } : {}));
    global.PEDIDOS = []; global.SERV_ATIVAS = {};
    const a = await abreEAtiva(fp);
    confere(a.m.licLiberada(), 'ativou');
    const maq1 = global.PEDIDOS.at(-1)?.c.maquina, nomeMaq = global.PEDIDOS.at(-1)?.c.nome_maquina;
    confere(/Reginaldo/.test(nomeMaq || ''), 'nome do PC traz o usuário: ' + nomeMaq);
    confere(fs.existsSync(`${RAIZ}/C/Users/Reginaldo/AppData/Roaming/InsideStudioSP-${DEVN}-licenca.txt`), 'licença gravada em C:/Users/Reginaldo/AppData/Roaming');
    confere(!fs.existsSync(`${RAIZ}/C/Users/Public/AppData/Roaming/InsideStudioSP-${DEVN}-licenca.txt`), 'nada gravado no perfil Public');
    global.PEDIDOS = [];
    const b = novo(fp); b.m.init(); await espera(20);
    confere(b.m.licLiberada() && global.PEDIDOS.length === 0, 'REABRIU: liberado direto do arquivo, sem pedir token');
    // mesmo PC com outro caminho do device (set salvo noutro lugar): mesmo id
    const c = await abreEAtiva('D:/Outra/Pasta/Rodo.amxd');
    confere(c.m.licLiberada() && (global.PEDIDOS.length === 0 || global.PEDIDOS.at(-1).c.maquina === maq1), 'device noutra pasta: segue liberado com o mesmo id de PC');
  }
  console.log('-- nada grava');
  montar({ soLeitura: ['C:/', 'D:/'] }); global.PEDIDOS = []; global.SERV_ATIVAS = {};
  const z = await abreEAtiva('');
  confere(z.m.licLiberada() && /não deixou gravar/.test(JSON.parse(z.m.licEstado()).msg || ''), 'ativa na sessão e AVISA que não gravou: ' + JSON.parse(z.m.licEstado()).msg);
  console.log('   log: ' + z.logs.filter((l) => /pastas da licença/.test(l)).join('').trim());
  console.log(erros ? 'RESULTADO: ' + erros + ' ERROS' : 'RESULTADO: OK');
})();
