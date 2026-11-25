// ativação AUTOMÁTICA (24/11/2026): um produto ativado no PC ativa os outros do mesmo e-mail sem pedir o token
//   node t-auto.js <pasta do build com rodo/ vassoura/ espanador/ desenrola/>
// Cenários: M4L ativa M4L · .lic de VST ativa M4L · e-mail sem o pacote (não ativa, e-mail preenchido, não repete)
//           · DEACTIVATE não volta sozinho na próxima abertura · sem candidato = fluxo de sempre (só "recuperar")
const fs = require('fs'), vm = require('vm'), path = require('path'), os = require('os');
const BUILD = process.argv[2] || (() => { throw new Error('passe a pasta do build'); })();
let erros = 0; const confere = (ok, t) => { console.log((ok ? '  ok   ' : '  ERRO ') + t); if (!ok) erros++; };
const espera = (ms) => new Promise((r) => setTimeout(r, ms));
let LOGDIR = '';
function pasta(nome) { LOGDIR = path.join(os.tmpdir(), 'isp-auto-' + nome); /* com \ no Windows: o lic-mock só remapeia caminhos com / */ fs.rmSync(LOGDIR, { recursive: true, force: true }); fs.mkdirSync(LOGDIR, { recursive: true }); global.SERV_ATIVAS = {}; global.PEDIDOS = []; global.SO_PACOTES = null; }
function abre(dev) {
  const outs = [];
  const ctx = { outlet: (n, ...a) => outs.push([n, ...a]), post: () => {}, Task: class { constructor(f) { this.f = f; } schedule() {} repeat() {} cancel() {} },
    Dict: class { stringify() { return '{}'; } parse() {} }, LiveAPI: class {}, Math, JSON, Date, Number, String, parseFloat, isNaN, Array, Infinity, isFinite, Float32Array,
    patcher: { filepath: 'C:/Users/teste/Documents/Ableton/User Library/Presets/X/X.amxd' }, ...require('./lic-mock.js')(LOGDIR) };
  vm.createContext(ctx); vm.runInContext(fs.readFileSync(path.join(BUILD, dev, dev + '.js'), 'utf8'), ctx);
  const ultima = () => { const o = [...outs].reverse().find((x) => x[0] === 'lic' || (x[1] === 'lic')); return o; };
  return { m: ctx, outs, ultima };
}
const autos = () => global.PEDIDOS.filter((p) => p.acao === 'ativar' && p.c.auto === true);
const ativaMao = async (dev) => { const a = abre(dev); a.m.init(); await espera(40); a.m.lic_email('cliente@teste.com'); a.m.lic_token('ISP-AAAA-BBBB-CCCC-DDDD-EEEE'); a.m.lic_ativar(); await espera(60); return a; };

(async () => {
  console.log('1) VASSOURA ativada à mão; RODO abre pela 1ª vez');
  pasta('1');
  const v = await ativaMao('vassoura');
  confere(v.m.licLiberada(), 'vassoura ativou com e-mail+token');
  global.PEDIDOS = [];
  const r = abre('rodo'); r.m.init(); await espera(120);
  confere(r.m.licLiberada(), 'rodo liberou SEM digitar nada');
  confere(autos().length === 1 && autos()[0].c.pacote === 'rodo' && autos()[0].c.email === 'cliente@teste.com', 'pedido ao servidor: ativar com auto=true, pacote rodo, e-mail da vassoura');
  confere(/sozinho/.test(r.m.lic.msg), 'mensagem: ' + r.m.lic.msg);
  global.PEDIDOS = [];
  const r2 = abre('rodo'); r2.m.init(); await espera(60);
  confere(r2.m.licLiberada() && global.PEDIDOS.length === 0, 'reabrir o rodo: lê a licença própria, sem falar com o servidor');

  console.log('2) só um VST/AU ativado (curva.lic); DESENROLA abre pela 1ª vez');
  pasta('2');
  const pv = LOGDIR + '/u/AppData/Roaming/InsideStudioSP/Licencas';
  fs.mkdirSync(pv, { recursive: true });
  fs.writeFileSync(pv + '/curva.lic', JSON.stringify({ email: 'cliente@teste.com', token: 'ISP-AAAA-BBBB-CCCC-DDDD-EEEE', dados: 'x', assinatura: 'y' }));
  const d = abre('desenrola'); d.m.init(); await espera(120);
  confere(d.m.licLiberada() && autos().length === 1, 'desenrola ativou sozinho com o token do VST');
  if (!d.m.licLiberada()) console.log('       pedidos:', JSON.stringify(global.PEDIDOS.map((p) => [p.acao, p.c.pacote, !!p.c.auto])), d.m.licEstado(), 'home=' + d.m.licHome());

  console.log('3) e-mail SEM o pacote (só tem a VASSOURA); ESPANADOR abre');
  pasta('3');
  global.SO_PACOTES = ['vassoura'];
  await ativaMao('vassoura');
  global.PEDIDOS = [];
  const e = abre('espanador'); e.m.init(); await espera(120);
  confere(!e.m.licLiberada(), 'espanador continua pedindo ativação');
  confere(autos().length === 1, 'tentou 1 vez (auto)');
  const st = JSON.parse(e.m.licEstado());
  confere(st.email === 'cliente@teste.com' && /não o ESPANADOR/.test(st.msg), 'tela com o e-mail preenchido e o aviso: ' + st.msg);
  global.PEDIDOS = [];
  const e2 = abre('espanador'); e2.m.init(); await espera(120);
  confere(autos().length === 0, 'reabrir: NÃO tenta de novo com o mesmo token (marcador)');

  console.log('4) RODO ativado sozinho e depois DEACTIVATE');
  pasta('4');
  await ativaMao('vassoura');
  const a = abre('rodo'); a.m.init(); await espera(120);
  confere(a.m.licLiberada(), 'rodo ativou sozinho');
  a.m.lic_desativar(); await espera(60);
  confere(!a.m.licLiberada(), 'DEACTIVATE travou');
  global.PEDIDOS = [];
  const b = abre('rodo'); b.m.init(); await espera(120);
  confere(!b.m.licLiberada() && autos().length === 0, 'reabrir depois de DEACTIVATE: não ativa sozinho de novo');

  console.log('5) PC sem nenhum produto InsideStudioSP');
  pasta('5');
  const n = abre('rodo'); n.m.init(); await espera(80);
  confere(!n.m.licLiberada() && autos().length === 0 && global.PEDIDOS.every((p) => p.acao === 'recuperar'), 'só o "recuperar" de sempre, nenhuma ativação automática');

  console.log(erros ? 'RESULTADO: ' + erros + ' ERROS' : 'RESULTADO: OK');
  process.exit(erros ? 1 : 0);
})();
