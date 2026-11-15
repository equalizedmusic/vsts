// 15/11 (3): o Max do cliente não gravava+lia arquivo em pasta NENHUMA (diag [h0 s1 c0 d0 n0]).
// Três Max falsos, todos com o device aberto do set (caminho sem /Users/, muda a cada abertura) e limite de 2 PCs:
//   tipo   = File só abre arquivo .txt quando pede "TEXT" (hipótese do defeito)  -> tem que gravar em .txt
//   dict   = File nunca abre; Dict.export_json/import_json funcionam             -> tem que gravar pelo Dict
//   nada   = File nunca abre e Dict falha                                         -> id derivado + /v1/recuperar
// Em todos: ativa UMA vez e reabre 4x sem pedir token e sem estourar o limite.
const fs = require('fs'), vm = require('vm'), path = require('path');
const ARQ = process.argv[2];
const DEVN = /vassoura/.test(ARQ) ? 'vassoura' : 'rodo';
const RAIZ = require('os').tmpdir() + '/m4l-pastas3-' + DEVN;
let erros = 0; const confere = (ok, t) => { console.log((ok ? '  ok   ' : '  ERRO ') + t); if (!ok) erros++; };
const mapa = (p) => p.replace(/^([A-Z]):/, (m, d) => RAIZ + '/' + d);
function montar() {
  fs.rmSync(RAIZ, { recursive: true, force: true });
  for (const u of ['Pichau', 'Public']) fs.mkdirSync(`${RAIZ}/C/Users/${u}/AppData/Roaming`, { recursive: true });
  fs.mkdirSync(`${RAIZ}/C/Users/Public/Documents`, { recursive: true });
}
function novo(modo, filepath) {
  const base = require('./lic-mock.js')(RAIZ);
  class File {
    constructor(p, mode, tipos) {
      this.p = mapa(p); this.position = 0; this.isopen = false;
      if (modo !== 'tipo') return;                                    // dict/nada: File nunca abre
      if (tipos === 'TEXT' && !/\.txt$/i.test(p)) return;            // filtro de tipo: só .txt é TEXT
      if (mode === 'write' || mode === 'readwrite') { if (!fs.existsSync(path.dirname(this.p))) return; if (mode === 'write') fs.writeFileSync(this.p, ''); this.isopen = true; }
      else this.isopen = fs.existsSync(this.p);
    }
    get eof() { return fs.existsSync(this.p) ? fs.statSync(this.p).size : 0; } set eof(v) { if (this.isopen) fs.truncateSync(this.p, v); }
    writestring(x) { fs.appendFileSync(this.p, x); }
    readstring(n) { const b = fs.readFileSync(this.p, 'utf8').slice(this.position, this.position + n); this.position += Buffer.byteLength(b); return b; }
    close() {}
  }
  class Dict {
    constructor() { this.o = {}; } set(k, v) { this.o[k] = v; } get(k) { return this.o[k]; } freepeer() {}
    stringify() { return JSON.stringify(this.o); } parse(s) { this.o = JSON.parse(s); }
    export_json(p) { if (modo !== 'dict') throw new Error('dict falhou'); fs.writeFileSync(mapa(p), JSON.stringify(this.o)); }
    import_json(p) { if (modo !== 'dict') throw new Error('dict falhou'); const q = mapa(p); if (fs.existsSync(q)) this.o = JSON.parse(fs.readFileSync(q, 'utf8')); }
  }
  class Folder {
    constructor(p) { const q = mapa(p); this.l = fs.existsSync(q) ? fs.readdirSync(q, { withFileTypes: true }) : []; this.i = 0; }
    get end() { return this.i >= this.l.length; } get filename() { return this.l[this.i].name; }
    get filetype() { return this.l[this.i].isDirectory() ? 'fold' : 'TEXT'; } next() { this.i++; } close() {}
  }
  const ctx = { outlet: () => {}, post: () => {}, Task: class { constructor(f) { this.f = f; } schedule() {} repeat() {} cancel() {} },
    LiveAPI: class {}, Math, JSON, Date, Number, String, parseFloat, isNaN, Array, Infinity, isFinite, Float32Array,
    patcher: { filepath }, ...base, File, Folder, Dict };
  ctx.max = Object.assign({}, base.max, { apppath: 'C:/ProgramData/Ableton/Live 12 Suite/Resources/Max/' });
  vm.createContext(ctx); vm.runInContext(fs.readFileSync(ARQ, 'utf8'), ctx);
  return ctx;
}
const espera = (ms) => new Promise((r) => setTimeout(r, ms));
(async () => {
  let n = 0;
  const fp = () => 'D:/Projetos/Set ' + (n++) + '/Rodo.amxd';   // sem /Users/: igual ao cliente
  for (const modo of ['tipo', 'dict', 'nada']) {
    console.log('-- Max "' + modo + '"');
    montar(); global.PEDIDOS = []; global.SERV_ATIVAS = {}; global.LIMITE = 2;
    for (const k of Object.keys(require.cache)) if (/lic-mock/.test(k)) delete require.cache[k];
    const a = novo(modo, fp()); a.init(); await espera(30);
    confere(!a.licLiberada(), 'antes de ativar: pede token');
    a.lic_email('cliente@teste.com'); a.lic_token('ISP-AAAA-BBBB-CCCC-DDDD-EEEE'); a.lic_ativar(); await espera(40);
    const at = global.PEDIDOS.filter((p) => p.acao === 'ativar');
    confere(a.licLiberada(), 'ativou · ' + (at.at(-1) || { c: {} }).c.versao);
    for (let k = 1; k <= 4; k++) {
      global.PEDIDOS = [];
      const b = novo(modo, fp()); b.init(); await espera(40);
      const acoes = global.PEDIDOS.map((p) => p.acao).join(',') || 'nenhum';
      confere(b.licLiberada() && !global.PEDIDOS.some((p) => p.acao === 'ativar'), `reabertura ${k}: liberado sem pedir token (servidor: ${acoes}) e-mail=${b.lic.email}`);
    }
    confere(Object.keys(global.SERV_ATIVAS).length === 1, 'servidor: 1 só vaga usada (' + Object.keys(global.SERV_ATIVAS).length + ')');
  }
  console.log(erros ? 'RESULTADO: ' + erros + ' ERROS' : 'RESULTADO: OK');
})();
