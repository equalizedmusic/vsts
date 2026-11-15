// 15/11: cliente que veio de <= 0.3.1 tem o .lic antigo gravado. DEACTIVATE (ou licença revogada) na versão nova
// tem que travar o device também depois de reabrir, sem cair no .lic antigo.
// node t-antigo.js <rodo.js ANTIGO (0.3.1)> <rodo.js NOVO>     (ISP_ED25519 = caminho do licenca-ed25519.json)
const fs = require('fs'), vm = require('vm');
const [ANTIGO, NOVO] = process.argv.slice(2);
const DIR = require('os').tmpdir() + '/m4l-antigo';
fs.rmSync(DIR, { recursive: true, force: true });
let erros = 0; const confere = (ok, t) => { console.log((ok ? '  ok   ' : '  ERRO ') + t); if (!ok) erros++; };
function novo(arq) {
  const ctx = { outlet: () => {}, post: () => {}, Task: class { constructor(f) { this.f = f; } schedule() {} repeat() {} cancel() {} },
    Dict: class { stringify() { return '{}'; } parse() {} }, LiveAPI: class {}, Math, JSON, Date, Number, String, parseFloat, isNaN, Array, Infinity, isFinite, Float32Array,
    patcher: { filepath: 'C:/Users/teste/Documents/Ableton/User Library/Presets/X/X.amxd' }, ...require('./lic-mock.js')(DIR) };
  vm.createContext(ctx); vm.runInContext(fs.readFileSync(arq, 'utf8'), ctx);
  return ctx;
}
(async () => {
  const espera = (ms) => new Promise((r) => setTimeout(r, ms));
  const a = novo(ANTIGO); a.init(); a.lic_email('cliente@teste.com'); a.lic_token('ISP-AAAA-BBBB-CCCC-DDDD-EEEE'); a.lic_ativar(); await espera(50);
  confere(a.licLiberada(), 'versão antiga: ativou');
  const antigos = fs.readdirSync(DIR, { recursive: true }).filter((f) => /\.lic$/.test(f));
  confere(antigos.length > 0, 'versão antiga gravou .lic (' + antigos.length + ')');
  let b = novo(NOVO); b.init(); await espera(50);
  confere(b.licLiberada(), 'versão nova: abre liberada com a licença antiga');
  b.lic_desativar(); await espera(50);
  confere(!b.licLiberada(), 'DEACTIVATE na versão nova');
  b = novo(NOVO); b.init(); await espera(80);
  confere(!b.licLiberada(), 'reabriu depois do DEACTIVATE: continua travado (não volta pelo .lic antigo)');
  console.log(erros ? 'RESULTADO: ' + erros + ' ERROS' : 'RESULTADO: OK');
})();
