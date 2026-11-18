// 18/11: internet do motor pelo [js] clássico quando o [v8] não tem XMLHttpRequest (Live 12.2/12.3, cliente Kontrakulture).
// node t-http.js          -> servidor falso (sem rede)
// node t-http.js real     -> o [js] fala com o servidor de licenças de verdade (versao + recuperar de um PC inexistente)
const fs = require('fs'), path = require('path'), vm = require('vm');
const REAL = process.argv[2] === 'real';
const LIC = fs.readFileSync(path.join(__dirname, '../_comum/isp_licenca.js'), 'utf8');
const HTTP = fs.readFileSync(path.join(__dirname, '../_comum/isp_http.js'), 'utf8');
let falhas = 0;
const ok = (c, txt) => { console.log((c ? 'OK   ' : 'FALHA') + ' ' + txt); if (!c) falhas++; };
const espera = (ms) => new Promise((r) => setTimeout(r, ms));

// servidor falso: guarda o corpo recebido e responde com acento (confere a volta em UTF-8)
let corpos = [];
class XhrFalso {
  open(m, url) { this.m = m; this.url = url; }
  send(corpo) {
    corpos.push({ url: this.url, corpo });
    setTimeout(() => {
      const r = /versao/.test(this.url) ? { ok: true, atualizacao: { versao: '9.9.9' } } : { ok: false, erro: 'nao_encontrada', mensagem: 'E-mail ou token não conferem; tente, de novo.' };
      this.readyState = 4; this.status = 200; this.responseText = JSON.stringify(r); this.onreadystatechange();
    }, 5);
  }
}
class XhrReal {
  open(m, url) { this.m = m; this.url = url; }
  send(corpo) {
    corpos.push({ url: this.url, corpo });
    fetch(this.url, { method: this.m, body: this.m === 'GET' ? undefined : corpo }).then(async (res) => {
      this.readyState = 4; this.status = res.status; this.responseText = await res.text(); this.onreadystatechange();
    }).catch(() => { this.readyState = 4; this.status = 0; this.responseText = ''; this.onreadystatechange(); });
  }
}

// [js] clássico (Max): outlet(0, ...) volta ao motor como mensagem; os átomos viram string/number como no Max
function novoJs(temXhr, motor) {
  const ctx = { post: () => {}, encodeURIComponent, decodeURIComponent, String, Number,
    outlet: (n, sel, ...a) => motor()[sel](...a) };
  if (temXhr) ctx.XMLHttpRequest = REAL ? XhrReal : XhrFalso;
  vm.createContext(ctx); vm.runInContext(HTTP, ctx);
  return ctx;
}
// motor [v8] com a licença; a saída 1 passa pelo [route http] do patcher
function novoMotor(v8TemXhr, jsTemXhr, semJs) {
  const saidas = [], tasks = [];
  let js;
  const ctx = { BigInt, JSON, Math, Date, String, Number, encodeURIComponent, decodeURIComponent, escape, unescape, parseInt,
    LIC_PACOTE: 'vassoura', LIC_NOME: 'VASSOURA', VERSAO: '0.3.6', URL_BAIXAR: 'x', URL_NOVIDADES: 'y',
    licMudou: () => {}, log: () => {}, arrayfromargs: (a) => Array.prototype.slice.call(a), str: (a) => a.join(' '), pastaDoDevice: () => '', File: undefined, Folder: undefined,
    max: { os: 'windows', apppath: 'C:/Program Files/Ableton/Live 12 Suite/Resources/Max/' },
    Task: class { constructor(fn) { this.fn = fn; tasks.push(this); } schedule(ms) { this.ms = ms; } cancel() { this.cancelada = 1; } },
    outlet: (n, ...a) => {
      saidas.push([n, ...a]);
      if (n === 1 && a[0] === 'http' && !semJs) js[a[1]](...a.slice(2));   // route http -> [js]
    } };
  if (v8TemXhr) ctx.XMLHttpRequest = REAL ? XhrReal : XhrFalso;
  vm.createContext(ctx); vm.runInContext(LIC, ctx);
  js = novoJs(jsTemXhr, () => ctx);
  return { ctx, saidas, tasks };
}

(async () => {
  const msgErro = REAL ? /./ : /não conferem; tente, de novo\./;

  // 1) Live 12.4: [v8] com XMLHttpRequest — caminho direto, nada passa pelo [js]
  corpos = [];
  let m = novoMotor(true, true);
  m.ctx.lic_email('cliente@teste.com'); m.ctx.lic_token('ISP-AAAA-BBBB-CCCC-DDDD-EEEE');
  m.ctx.lic_ativar(); await espera(REAL ? 4000 : 50);
  ok(!m.saidas.some((s) => s[1] === 'http'), '12.4: não usa o [js]');
  ok(!REAL ? msgErro.test(m.ctx.lic.msg) : m.ctx.lic.msg.length > 0, '12.4: resposta do servidor chegou -> "' + m.ctx.lic.msg + '"');
  ok(m.ctx.lic.ocupado === false, '12.4: tela destrava');
  ok(corpos[0] && !/ js"/.test(corpos[0].corpo), '12.4: versão sem marca " js"');

  // 2) Live 12.3: [v8] SEM XMLHttpRequest — vai pelo [js]
  corpos = [];
  m = novoMotor(false, true);
  m.ctx.lic_email('cliente@teste.com'); m.ctx.lic_token('ISP-AAAA-BBBB-CCCC-DDDD-EEEE');
  m.ctx.lic_ativar(); await espera(REAL ? 4000 : 50);
  ok(m.saidas.some((s) => s[0] === 1 && s[1] === 'http' && s[2] === 'req'), '12.3: pedido saiu pela saída 1 como "http req"');
  const c = corpos[0] && corpos[0].corpo;
  let j = null; try { j = JSON.parse(c); } catch (e) { }
  ok(j && j.email === 'cliente@teste.com' && j.token === 'ISP-AAAA-BBBB-CCCC-DDDD-EEEE' && j.pacote === 'vassoura', '12.3: servidor recebeu o JSON certo');
  ok(c && /^[\x00-\x7f]*$/.test(c) && /· Max for Live/.test(j.nome_maquina), '12.3: corpo em ASCII e "·" volta certo -> ' + (j && j.nome_maquina));
  ok(j && / js$/.test(j.versao), '12.3: versão marcada " js" -> ' + (j && j.versao));
  ok(!REAL ? msgErro.test(m.ctx.lic.msg) : m.ctx.lic.msg.length > 0, '12.3: resposta com acento/vírgula/; voltou -> "' + m.ctx.lic.msg + '"');
  ok(m.ctx.lic.ocupado === false, '12.3: tela destrava');
  ok(m.tasks.every((t) => t.cancelada), '12.3: relógio de 25 s cancelado');

  // 3) versão nova pelo [js]
  m = novoMotor(false, true);
  m.ctx.checaVersao(); await espera(REAL ? 4000 : 50);
  if (!REAL) ok(m.saidas.some((s) => s[0] === 2 && s[1] === 'nova' && s[2] === '9.9.9'), '12.3: aviso de versão nova chega pela internet do [js]');
  else ok(m.tasks.every((t) => t.cancelada), 'real: /versao respondeu pelo [js] (relógio cancelado)');

  // 4) recuperar (segundo plano) pelo [js]: sem erro na tela
  m = novoMotor(false, true);
  m.ctx.licPede('recuperar', '', '', false); await espera(REAL ? 4000 : 50);
  ok(m.ctx.lic.ok === false && m.tasks.every((t) => t.cancelada), '12.3: recuperar respondeu e não liberou');

  if (!REAL) {
    // 5) [js] também sem XMLHttpRequest
    m = novoMotor(false, false);
    m.ctx.lic_email('cliente@teste.com'); m.ctx.lic_token('ISP-AAAA-BBBB-CCCC-DDDD-EEEE');
    m.ctx.lic_ativar(); await espera(20);
    ok(/Atualize o Ableton Live/.test(m.ctx.lic.msg) && !m.ctx.lic.ocupado, 'sem internet no Max: mensagem clara -> "' + m.ctx.lic.msg + '"');

    // 6) [js] não responde (objeto ausente): relógio de 25 s libera a tela
    m = novoMotor(false, true, true);
    m.ctx.lic_email('cliente@teste.com'); m.ctx.lic_token('ISP-AAAA-BBBB-CCCC-DDDD-EEEE');
    m.ctx.lic_ativar();
    ok(m.ctx.lic.ocupado === true && m.tasks[0] && m.tasks[0].ms === 25000, 'sem [js]: fica ocupado e agenda 25 s');
    m.tasks[0].fn();
    ok(!m.ctx.lic.ocupado && /servidor de licenças/.test(m.ctx.lic.msg), 'sem [js]: após 25 s -> "' + m.ctx.lic.msg + '"');
  }
  console.log(falhas ? falhas + ' FALHA(S)' : 'TUDO OK');
  process.exit(falhas ? 1 : 0);
})();
