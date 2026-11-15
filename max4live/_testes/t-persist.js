// licença gravada: recarrega sem internet, adulterada, outra máquina, renovação em segundo plano, desativar
const fs = require('fs'), vm = require('vm');
const LOGDIR = require('os').tmpdir() + '/rodo-sim2';
const ARQ = (process.argv[2] || require('path').join(__dirname, '../rodo/rodo.js'));
const DEVN = /vassoura/.test(ARQ) ? 'vassoura' : 'rodo';
let erros = 0; const confere = (ok, t) => { console.log((ok ? '  ok   ' : '  ERRO ') + t); if (!ok) erros++; };
function novo() {
  const outs = [];
  const ctx = { outlet: (n, ...a) => outs.push([n, ...a]), post: () => {}, Task: class { constructor(f) { this.f = f; } schedule() {} repeat() {} cancel() {} },
    Dict: class { stringify() { return '{}'; } parse() {} }, LiveAPI: class {}, Math, JSON, Date, Number, String, parseFloat, isNaN, Array, Infinity, isFinite, Float32Array,
    patcher: { filepath: 'C:/Users/teste/Documents/Ableton/User Library/Presets/X/X.amxd' }, ...require('./lic-mock.js')(LOGDIR) };
  vm.createContext(ctx); vm.runInContext(fs.readFileSync(ARQ, 'utf8'), ctx);
  return { m: ctx, outs };
}
const lic = `${LOGDIR}/u/AppData/Roaming/InsideStudioSP-${DEVN}-licenca.txt`, idf = `${LOGDIR}/u/AppData/Roaming/InsideStudioSP-m4l-id.txt`;
(async () => {
  const espera = (ms) => new Promise((r) => setTimeout(r, ms));
  if (DEVN === 'vassoura' || !fs.existsSync(lic)) {   // ativa primeiro
    const a = novo(); a.m.init(); a.m.lic_email('cliente@teste.com'); a.m.lic_token('ISP-AAAA-BBBB-CCCC-DDDD-EEEE'); a.m.lic_ativar(); await espera(50);
    confere(a.m.licLiberada(), DEVN + ': ativou');
  }
  global.PEDIDOS = [];
  let x = novo(); x.m.init(); await espera(30);
  confere(x.m.licLiberada() && global.PEDIDOS.length === 0, 'recarregou do arquivo sem falar com o servidor');
  const bom = fs.readFileSync(lic, 'utf8');
  const j = JSON.parse(bom); const d = JSON.parse(Buffer.from(j.dados, 'base64')); d.valida_ate += 999999; j.dados = Buffer.from(JSON.stringify(d)).toString('base64');
  fs.writeFileSync(lic, JSON.stringify(j));
  global.PEDIDOS = []; x = novo(); x.m.licCarrega = x.m.licCarrega; x.m.init(); await espera(30);
  confere(!x.m.licLiberada() || global.PEDIDOS.length > 0, 'adulterada: não vale pelo arquivo (tenta confirmar no servidor: ' + global.PEDIDOS.length + ')');
  await espera(30);
  confere(x.m.licLiberada() && fs.readFileSync(lic, 'utf8') !== JSON.stringify(j), '... e o servidor reemitiu (máquina ativada)');
  // outro computador: id diferente
  const idBom = fs.readFileSync(idf, 'utf8'); fs.writeFileSync(idf, 'f'.repeat(48));
  global.PEDIDOS = []; x = novo(); x.m.init(); await espera(50);
  confere(!x.m.licLiberada(), 'pasta copiada p/ outro PC (id diferente): trava e pede ativação');
  fs.writeFileSync(idf, idBom); fs.writeFileSync(lic, bom);
  // renovação: emitida há 4 dias
  global.EMITIDA_ATRAS = 4 * 86400; x = novo(); x.m.lic_email('cliente@teste.com'); x.m.lic_token('ISP-AAAA-BBBB-CCCC-DDDD-EEEE'); x.m.lic_ativar(); await espera(30); global.EMITIDA_ATRAS = 0;
  global.PEDIDOS = []; x = novo(); x.m.init(); await espera(40);
  confere(x.m.licLiberada() && global.PEDIDOS.some((p) => p.acao === 'validar'), 'com mais de 3 dias: confirma em segundo plano (validar) e segue liberada');
  // desativar
  x.m.lic_desativar(); await espera(30);
  confere(!x.m.licLiberada() && fs.readFileSync(lic, 'utf8') === '', 'DEACTIVATE: libera a vaga e apaga a licença daqui');
  x.m.init(); confere(!x.m.licLiberada(), 'depois de desativar: trava');
  // sem home reconhecível: grava na pasta do device
  console.log(erros ? 'RESULTADO: ' + erros + ' ERROS' : 'RESULTADO: OK');
})();
