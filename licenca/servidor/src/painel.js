// Painel web de administração das licenças: https://<worker>/painel  (usuário e senha).
//
//   GET  /painel              página (a tela de login ou o painel, decide o JS)
//   GET  /painel/app.js|.css
//   GET  /painel/sessao       { ok, usuario } ou 401
//   POST /painel/entrar       { usuario, senha }  -> cookie de sessão (12 h)
//   POST /painel/sair
//   POST /painel/senha        { atual, nova }
// As ações (criar, listar, revogar, liberar máquina, novo token) usam as mesmas rotas /admin/* da linha de comando,
// com a sessão no lugar da ADMIN_KEY (ver rotaAdmin em index.js).
//
// Senha em PBKDF2-SHA256 (tabela painel). Sessão = cookie HttpOnly/Secure/SameSite=Strict assinado com HMAC da
// ADMIN_KEY; leva o começo do hash da senha, então trocar a senha derruba as sessões abertas.
// 10 senhas erradas do mesmo IP em 15 min bloqueiam o login desse IP por 15 min.

const SESSAO_HORAS = 12;
const ITERACOES = 100000;          // máximo que o Workers aceita no PBKDF2
const ERROS_LOGIN = 10;
const JANELA_LOGIN = 15 * 60;
const COOKIE = 'isp_painel';

const agora = () => Math.floor(Date.now() / 1000);
const enc = (s) => new TextEncoder().encode(s);

function b64(bytes) {
    let s = '';
    for (const b of bytes) s += String.fromCharCode(b);
    return btoa(s);
}
function deB64(texto) {
    const s = atob(texto);
    const out = new Uint8Array(s.length);
    for (let i = 0; i < s.length; i++) out[i] = s.charCodeAt(i);
    return out;
}
const b64url = (bytes) => b64(bytes).replace(/\+/g, '-').replace(/\//g, '_').replace(/=+$/, '');
const deB64url = (s) => deB64(s.replace(/-/g, '+').replace(/_/g, '/') + '==='.slice((s.length + 3) % 4));

function iguais(a, b) {
    if (a.length !== b.length) return false;
    let d = 0;
    for (let i = 0; i < a.length; i++) d |= a.charCodeAt(i) ^ b.charCodeAt(i);
    return d === 0;
}

function json(obj, status = 200, extra = {}) {
    return new Response(JSON.stringify(obj), {
        status, headers: { 'content-type': 'application/json; charset=utf-8', 'cache-control': 'no-store', ...extra },
    });
}
const erro = (mensagem, status) => json({ ok: false, mensagem }, status);

async function pbkdf2(senha, sal, iteracoes) {
    const chave = await crypto.subtle.importKey('raw', enc(senha), 'PBKDF2', false, ['deriveBits']);
    const bits = await crypto.subtle.deriveBits({ name: 'PBKDF2', hash: 'SHA-256', salt: deB64(sal), iterations: iteracoes }, chave, 256);
    return b64(new Uint8Array(bits));
}

export async function gravarSenhaPainel(env, usuario, senha) {
    const sal = b64(crypto.getRandomValues(new Uint8Array(16)));
    await env.DB.prepare(
        'INSERT INTO painel (usuario, senha_hash, sal, iteracoes, trocada_em) VALUES (?, ?, ?, ?, ?) ' +
        'ON CONFLICT (usuario) DO UPDATE SET senha_hash = excluded.senha_hash, sal = excluded.sal, ' +
        'iteracoes = excluded.iteracoes, trocada_em = excluded.trocada_em')
        .bind(usuario, await pbkdf2(senha, sal, ITERACOES), sal, ITERACOES, agora()).run();
}

async function hmac(env, texto) {
    const chave = await crypto.subtle.importKey('raw', enc('painel:' + env.ADMIN_KEY), { name: 'HMAC', hash: 'SHA-256' }, false, ['sign']);
    return b64url(new Uint8Array(await crypto.subtle.sign('HMAC', chave, enc(texto))));
}

function lerCookie(req) {
    const m = (req.headers.get('cookie') || '').match(new RegExp('(?:^|;\\s*)' + COOKIE + '=([^;]+)'));
    return m ? m[1] : null;
}
const cookie = (valor, segundos) =>
    `${COOKIE}=${valor}; Path=/; HttpOnly; Secure; SameSite=Strict; Max-Age=${segundos}`;

// devolve o usuário logado ou null
export async function sessaoPainel(req, env) {
    if (!env.ADMIN_KEY) return null;
    const c = lerCookie(req);
    if (!c || !c.includes('.')) return null;
    const [dados, assinatura] = c.split('.');
    if (!iguais(assinatura, await hmac(env, dados))) return null;
    let s;
    try { s = JSON.parse(new TextDecoder().decode(deB64url(dados))); } catch { return null; }
    if (!s || !(s.exp > agora())) return null;
    const u = await env.DB.prepare('SELECT senha_hash FROM painel WHERE usuario = ?').bind(String(s.u)).first();
    if (!u || !iguais(String(s.h), u.senha_hash.slice(0, 16))) return null;
    return s.u;
}

async function abrirSessao(env, usuario, senhaHash) {
    const dados = b64url(enc(JSON.stringify({ u: usuario, h: senhaHash.slice(0, 16), exp: agora() + SESSAO_HORAS * 3600 })));
    return cookie(dados + '.' + (await hmac(env, dados)), SESSAO_HORAS * 3600);
}

async function conferirSenha(env, usuario, senha) {
    const u = await env.DB.prepare('SELECT * FROM painel WHERE usuario = ?').bind(usuario).first();
    // usuário que não existe também passa pelo PBKDF2, para o tempo de resposta não entregar quem existe
    const h = await pbkdf2(senha, u ? u.sal : 'AAAAAAAAAAAAAAAAAAAAAA==', u ? u.iteracoes : ITERACOES);
    return u && iguais(h, u.senha_hash) ? u : null;
}

async function registrar(env, tipo, detalhe, ip) {
    await env.DB.prepare('INSERT INTO eventos (licenca_id, tipo, detalhe, ip, quando) VALUES (NULL, ?, ?, ?, ?)')
        .bind(tipo, detalhe, ip, agora()).run();
}

const CABECALHOS_PAGINA = {
    'cache-control': 'no-store',
    'content-security-policy': "default-src 'self'; img-src 'self' data:; frame-ancestors 'none'; base-uri 'none'; form-action 'self'",
    'x-frame-options': 'DENY',
    'x-content-type-options': 'nosniff',
    'referrer-policy': 'no-referrer',
};
const pagina = (corpo, tipo) => new Response(corpo, { headers: { 'content-type': tipo, ...CABECALHOS_PAGINA } });

export async function rotaPainel(caminho, req, env) {
    const ip = req.headers.get('cf-connecting-ip') || '';

    if (req.method === 'GET') {
        if (caminho === '/painel') return pagina(HTML, 'text/html; charset=utf-8');
        if (caminho === '/painel/app.js') return pagina(JS, 'text/javascript; charset=utf-8');
        if (caminho === '/painel/app.css') return pagina(CSS, 'text/css; charset=utf-8');
        if (caminho === '/painel/sessao') {
            const u = await sessaoPainel(req, env);
            return u ? json({ ok: true, usuario: u }) : erro('Sessão encerrada.', 401);
        }
        return erro('Não encontrado.', 404);
    }
    if (req.method !== 'POST' || req.headers.get('x-painel') !== '1') return erro('Pedido inválido.', 400);

    let corpo;
    try { corpo = await req.json(); } catch { return erro('Pedido inválido.', 400); }

    if (caminho === '/painel/entrar') {
        const r = await env.DB.prepare("SELECT COUNT(*) AS n FROM eventos WHERE ip = ? AND tipo = 'painel_erro' AND quando > ?")
            .bind(ip, agora() - JANELA_LOGIN).first();
        if ((r?.n || 0) >= ERROS_LOGIN) return erro('Muitas tentativas. Espere 15 minutos.', 429);
        const usuario = String(corpo.usuario || '').trim().toLowerCase();
        const u = await conferirSenha(env, usuario, String(corpo.senha || ''));
        if (!u) {
            await registrar(env, 'painel_erro', usuario.slice(0, 40), ip);
            return erro('Usuário ou senha não conferem.', 401);
        }
        await registrar(env, 'painel_entrou', usuario, ip);
        return json({ ok: true, usuario }, 200, { 'set-cookie': await abrirSessao(env, usuario, u.senha_hash) });
    }

    if (caminho === '/painel/sair') return json({ ok: true }, 200, { 'set-cookie': cookie('', 0) });

    if (caminho === '/painel/senha') {
        const usuario = await sessaoPainel(req, env);
        if (!usuario) return erro('Sessão encerrada.', 401);
        if (!(await conferirSenha(env, usuario, String(corpo.atual || '')))) return erro('A senha atual não confere.', 403);
        const nova = String(corpo.nova || '');
        if (nova.length < 10) return erro('A senha nova precisa ter pelo menos 10 caracteres.', 400);
        await gravarSenhaPainel(env, usuario, nova);
        await registrar(env, 'painel_senha', usuario, ip);
        const u = await env.DB.prepare('SELECT senha_hash FROM painel WHERE usuario = ?').bind(usuario).first();
        return json({ ok: true }, 200, { 'set-cookie': await abrirSessao(env, usuario, u.senha_hash) });
    }
    return erro('Não encontrado.', 404);
}

// ---------------- página ----------------

const HTML = `<!doctype html>
<html lang="pt-BR">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<meta name="robots" content="noindex, nofollow">
<title>Licenças · InsideStudioSP</title>
<link rel="stylesheet" href="/painel/app.css">
<script src="/painel/app.js" defer></script>
</head>
<body>
<header>
  <div class="marca">INSIDE STUDIO SP <span>· licenças</span></div>
  <nav id="menu" hidden>
    <span id="quem"></span>
    <button type="button" id="btSenha" class="discreto">Trocar senha</button>
    <button type="button" id="btSair" class="discreto">Sair</button>
  </nav>
</header>

<main>
  <section id="telaLogin" hidden>
    <form id="fLogin" class="cartao estreito">
      <h1>Entrar</h1>
      <label>Usuário <input name="usuario" autocomplete="username" required></label>
      <label>Senha <input name="senha" type="password" autocomplete="current-password" required></label>
      <button type="submit">Entrar</button>
      <p class="msg" id="msgLogin"></p>
    </form>
  </section>

  <section id="telaPainel" hidden>
    <form id="fNova" class="cartao">
      <h2>Nova licença</h2>
      <label>E-mail do cliente <input name="email" type="email" required autocomplete="off"></label>
      <fieldset><legend>Pacotes</legend><div id="pacotes" class="pacotes"></div></fieldset>
      <div class="linha">
        <label>Origem
          <select name="origem"><option value="venda">Venda</option><option value="cortesia">Cortesia (teste grátis)</option></select>
        </label>
        <label>Pedido / Pix (opcional) <input name="pedido" autocomplete="off"></label>
        <label>Computadores <input name="maquinas" type="number" min="1" max="10" value="2"></label>
      </div>
      <label>Nota (opcional) <input name="nota" autocomplete="off"></label>
      <button type="submit">Gerar licença</button>
      <p class="msg" id="msgNova"></p>
    </form>

    <div id="resultado" class="cartao destaque" hidden>
      <h2>Licença gerada</h2>
      <p id="resResumo"></p>
      <pre id="resMensagem"></pre>
      <div class="linha">
        <button type="button" id="btCopiar">Copiar mensagem</button>
        <a id="btEmail" class="botao" href="#">Abrir no e-mail</a>
      </div>
    </div>

    <div class="cartao">
      <div class="linha topo">
        <h2>Clientes</h2>
        <form id="fBusca" class="busca"><input name="q" placeholder="Buscar por e-mail" autocomplete="off"><button type="submit" class="discreto">Buscar</button></form>
      </div>
      <div id="lista"></div>
    </div>
  </section>

  <dialog id="dSenha">
    <form id="fSenha" method="dialog" class="estreito">
      <h2>Trocar senha</h2>
      <label>Senha atual <input name="atual" type="password" autocomplete="current-password" required></label>
      <label>Senha nova (mín. 10) <input name="nova" type="password" autocomplete="new-password" minlength="10" required></label>
      <div class="linha">
        <button type="submit" value="ok">Salvar</button>
        <button type="button" id="btSenhaFechar" class="discreto">Cancelar</button>
      </div>
      <p class="msg" id="msgSenha"></p>
    </form>
  </dialog>
</main>
</body>
</html>`;

const CSS = `
:root { --fundo:#141416; --cartao:#1e1f23; --borda:#2f3036; --texto:#e8e6e1; --fraco:#9a988f; --acento:#e8a33d;
        --ok:#7cc576; --ruim:#e0685b; }
* { box-sizing:border-box; }
body { margin:0; background:var(--fundo); color:var(--texto); font:15px/1.45 system-ui, -apple-system, "Segoe UI", sans-serif; }
header { display:flex; align-items:center; justify-content:space-between; padding:14px 22px; border-bottom:1px solid var(--borda); }
.marca { font-weight:700; letter-spacing:.12em; } .marca span { color:var(--acento); letter-spacing:0; font-weight:500; }
nav { display:flex; gap:10px; align-items:center; } #quem { color:var(--fraco); }
main { max-width:980px; margin:0 auto; padding:22px; }
.cartao { background:var(--cartao); border:1px solid var(--borda); border-radius:10px; padding:18px 20px; margin-bottom:18px; }
.destaque { border-color:var(--acento); }
.estreito { max-width:380px; margin:40px auto; }
h1, h2 { margin:0 0 14px; font-size:18px; } h1 { font-size:22px; }
label { display:flex; flex-direction:column; gap:4px; margin-bottom:12px; color:var(--fraco); font-size:13px; flex:1; }
input, select { background:#111214; color:var(--texto); border:1px solid var(--borda); border-radius:6px; padding:9px 10px; font:inherit; }
input:focus, select:focus { outline:2px solid var(--acento); outline-offset:-1px; }
fieldset { border:1px solid var(--borda); border-radius:6px; margin:0 0 12px; padding:8px 12px; }
legend { color:var(--fraco); font-size:13px; padding:0 4px; }
.pacotes { display:flex; flex-wrap:wrap; gap:6px 18px; }
.pacotes label { flex-direction:row; align-items:center; gap:6px; margin:4px 0; color:var(--texto); font-size:15px; flex:none; }
.linha { display:flex; gap:12px; align-items:flex-end; flex-wrap:wrap; }
.linha.topo { justify-content:space-between; align-items:center; margin-bottom:10px; } .linha.topo h2 { margin:0; }
button, .botao { background:var(--acento); color:#1a1306; border:0; border-radius:6px; padding:9px 16px; font:inherit; font-weight:600;
                 cursor:pointer; text-decoration:none; display:inline-block; }
button.discreto { background:transparent; color:var(--texto); border:1px solid var(--borda); font-weight:500; }
button.pequeno { padding:4px 10px; font-size:13px; }
button.perigo { background:transparent; color:var(--ruim); border:1px solid var(--ruim); }
button:disabled { opacity:.5; cursor:default; }
.busca { display:flex; gap:8px; } .busca input { width:260px; }
.msg { min-height:1.2em; margin:10px 0 0; color:var(--ruim); } .msg.ok { color:var(--ok); }
pre { background:#111214; border:1px solid var(--borda); border-radius:6px; padding:12px; white-space:pre-wrap; font:13px/1.5 ui-monospace, Consolas, monospace; }
.cliente { border-top:1px solid var(--borda); padding:14px 0; } .cliente:first-child { border-top:0; }
.cliente-topo { display:flex; justify-content:space-between; align-items:center; gap:10px; flex-wrap:wrap; }
.email { font-weight:600; font-size:16px; word-break:break-all; }
.lic { display:grid; grid-template-columns:120px 1fr auto; gap:6px 14px; align-items:start; padding:8px 0 0 12px; }
.pac { font-weight:600; } .status-ativa { color:var(--ok); } .status-revogada { color:var(--ruim); }
.det { color:var(--fraco); font-size:13px; } .maq { display:flex; gap:8px; align-items:center; margin-top:3px; color:var(--texto); }
.vazio { color:var(--fraco); }
dialog { background:var(--cartao); color:var(--texto); border:1px solid var(--borda); border-radius:10px; padding:4px 20px; }
dialog::backdrop { background:rgba(0,0,0,.6); } dialog .estreito { margin:10px auto; }
@media (max-width:640px) { .lic { grid-template-columns:1fr; } .busca input { width:100%; } }
`;

// JS do navegador. Tudo que vem do banco (e-mail, nome da máquina, nota) entra por textContent, nunca por innerHTML.
const JS = `
'use strict';
var PACOTES = [['mix','Mix'],['criativo','Criativo'],['knob','Knob'],['quimera','Quimera'],['limao','Limão'],['mercurio','Mercúrio']];
var NOME = {}; PACOTES.forEach(function (p) { NOME[p[0]] = p[1]; });
var $ = function (id) { return document.getElementById(id); };

function el(tag, attrs, filhos) {
  var e = document.createElement(tag);
  if (attrs) Object.keys(attrs).forEach(function (k) {
    if (k === 'texto') e.textContent = attrs[k];
    else if (k === 'classe') e.className = attrs[k];
    else if (k.slice(0, 2) === 'on') e.addEventListener(k.slice(2), attrs[k]);
    else e.setAttribute(k, attrs[k]);
  });
  (filhos || []).forEach(function (f) { if (f) e.appendChild(typeof f === 'string' ? document.createTextNode(f) : f); });
  return e;
}

function api(metodo, caminho, corpo) {
  return fetch(caminho, {
    method: metodo, credentials: 'same-origin',
    headers: { 'x-painel': '1', 'content-type': 'application/json' },
    body: corpo ? JSON.stringify(corpo) : undefined
  }).then(function (r) {
    return r.json().catch(function () { return { ok: false, mensagem: 'Resposta inválida do servidor.' }; }).then(function (j) {
      if (r.status === 401 && caminho !== '/painel/entrar') { mostrarLogin(); }
      return j;
    });
  }).catch(function () { return { ok: false, mensagem: 'Sem conexão com o servidor.' }; });
}

function data(t) { return t ? new Date(t * 1000).toLocaleString('pt-BR', { dateStyle: 'short', timeStyle: 'short' }) : '-'; }
function msg(id, texto, ok) { var m = $(id); m.textContent = texto || ''; m.className = 'msg' + (ok ? ' ok' : ''); }

function mostrarLogin() {
  $('telaPainel').hidden = true; $('menu').hidden = true; $('telaLogin').hidden = false;
  $('fLogin').senha.value = ''; $('fLogin').usuario.focus();
}
function mostrarPainel(usuario) {
  $('telaLogin').hidden = true; $('telaPainel').hidden = false; $('menu').hidden = false;
  $('quem').textContent = usuario;
  carregar('');
}

function mensagemCliente(email, pacotes, token) {
  var nomes = pacotes.map(function (p) { return NOME[p]; }).join(', ');
  var linhas = [
    'Olá!',
    '',
    'Obrigado por escolher a InsideStudioSP. Seguem os dados para ativar seus plugins:',
    '',
    'Pacote' + (pacotes.length > 1 ? 's' : '') + ': ' + nomes,
    'E-mail: ' + email
  ];
  if (token) linhas.push('Token: ' + token);
  else linhas.push('Token: o mesmo que você já recebeu da InsideStudioSP (ele vale para todos os seus pacotes).');
  linhas.push('',
    'Como ativar:',
    '1. Instale o pacote e abra qualquer plugin dele no seu DAW.',
    '2. Na tela de ativação, digite o e-mail e o token acima e clique em ATIVAR.',
    '',
    'O token vale para 2 computadores. Para trocar de computador, abra o menu do plugin, entre em "Licença…" e clique em "Desativar este computador"; depois é só ativar no computador novo.',
    '',
    'Qualquer dúvida, é só responder este e-mail.',
    '',
    'InsideStudioSP');
  return linhas.join('\\n');
}

function mostrarResultado(email, pacotes, token, extra) {
  var texto = mensagemCliente(email, pacotes, token);
  $('resResumo').textContent = email + ' · ' + pacotes.map(function (p) { return NOME[p]; }).join(', ') +
    (token ? ' · token novo: ' + token : ' · mesmo token que este e-mail já tem') + (extra ? ' · ' + extra : '');
  $('resMensagem').textContent = texto;
  $('btEmail').href = 'mailto:' + encodeURIComponent(email) + '?subject=' + encodeURIComponent('Sua licença InsideStudioSP') +
    '&body=' + encodeURIComponent(texto);
  $('resultado').hidden = false;
  $('resultado').scrollIntoView({ behavior: 'smooth', block: 'start' });
}

function criar(ev) {
  ev.preventDefault();
  var f = ev.target;
  var pacotes = Array.prototype.slice.call(f.querySelectorAll('input[name=pacote]:checked')).map(function (c) { return c.value; });
  var email = f.email.value.trim().toLowerCase();
  if (!pacotes.length) { msg('msgNova', 'Marque pelo menos um pacote.'); return; }
  var bt = f.querySelector('button[type=submit]'); bt.disabled = true; msg('msgNova', 'Gerando…', true);
  var token = null, feitos = [], erros = [];
  var seq = Promise.resolve();
  pacotes.forEach(function (p) {
    seq = seq.then(function () {
      return api('POST', '/admin/licencas', {
        email: email, pacote: p, origem: f.origem.value, pedido: f.pedido.value.trim(), nota: f.nota.value.trim(),
        max_maquinas: Number(f.maquinas.value) || 2
      }).then(function (j) {
        if (j.ok) { feitos.push(p); if (j.token) token = j.token; }
        else erros.push(NOME[p] + ': ' + (j.mensagem || 'erro'));
      });
    });
  });
  seq.then(function () {
    bt.disabled = false;
    msg('msgNova', erros.join(' · '), false);
    if (feitos.length) {
      mostrarResultado(email, feitos, token, '');
      f.reset(); f.maquinas.value = 2;
      carregar(email);
    }
  });
}

function acao(caminho, corpo, pergunta) {
  if (pergunta && !confirm(pergunta)) return;
  api('POST', caminho, corpo).then(function (j) {
    if (!j.ok) { alert(j.mensagem || 'Erro.'); return; }
    if (j.token) {
      var pacs = (ultimaLista[j.email] || []).filter(function (l) { return l.status === 'ativa'; }).map(function (l) { return l.pacote; });
      mostrarResultado(j.email, pacs.length ? pacs : [j.pacote], j.token, 'o token antigo parou de valer');
    }
    carregar($('fBusca').q.value.trim());
  });
}

var ultimaLista = {};
function carregar(q) {
  api('GET', '/admin/licencas' + (q ? '?email=' + encodeURIComponent(q.toLowerCase()) : '')).then(function (j) {
    var lista = $('lista'); lista.textContent = '';
    if (!j.ok) { lista.appendChild(el('p', { classe: 'vazio', texto: j.mensagem || 'Erro ao carregar.' })); return; }
    var porEmail = {}, ordem = [];
    j.licencas.forEach(function (l) { if (!porEmail[l.email]) { porEmail[l.email] = []; ordem.push(l.email); } porEmail[l.email].push(l); });
    ultimaLista = porEmail;
    if (!ordem.length) { lista.appendChild(el('p', { classe: 'vazio', texto: q ? 'Nenhuma licença para este e-mail.' : 'Nenhuma licença ainda.' })); return; }
    ordem.forEach(function (email) {
      var lics = porEmail[email];
      var bloco = el('div', { classe: 'cliente' }, [
        el('div', { classe: 'cliente-topo' }, [
          el('span', { classe: 'email', texto: email }),
          el('button', { type: 'button', classe: 'discreto pequeno', texto: 'Gerar token novo',
            onclick: function () { acao('/admin/novo-token', { id: lics[0].id },
              'Gerar um token novo para ' + email + '?\\n\\nO token atual para de valer em todos os pacotes deste e-mail. Os computadores já ativados continuam até a próxima conferência com o servidor (em até 3 dias) e aí pedem o token novo.'); } })
        ])
      ]);
      lics.forEach(function (l) {
        var maquinas = el('div', { classe: 'det' }, [ l.maquinas.length + ' de ' + l.max_maquinas + ' computadores' ]);
        l.maquinas.forEach(function (m) {
          maquinas.appendChild(el('div', { classe: 'maq' }, [
            el('span', { texto: (m.nome_maquina || '(sem nome)') + ' · ativado ' + data(m.criada_em) + ' · visto ' + data(m.visto_em) }),
            el('button', { type: 'button', classe: 'discreto pequeno', texto: 'Liberar',
              onclick: function () { acao('/admin/liberar-maquina', { id: l.id, maquina: String(m.id) },
                'Liberar o computador "' + (m.nome_maquina || '') + '" desta licença? Ele para de funcionar na próxima conferência e a vaga fica livre.'); } })
          ]));
        });
        var info = [ l.origem === 'venda' ? 'venda' : 'cortesia', 'criada ' + data(l.criada_em) ];
        if (l.pedido) info.push('pedido ' + l.pedido);
        if (l.nota) info.push(l.nota);
        bloco.appendChild(el('div', { classe: 'lic' }, [
          el('div', {}, [ el('div', { classe: 'pac', texto: NOME[l.pacote] || l.pacote }),
                          el('div', { classe: 'status-' + l.status, texto: l.status }) ]),
          el('div', {}, [ el('div', { classe: 'det', texto: info.join(' · ') }), maquinas ]),
          l.status === 'ativa'
            ? el('button', { type: 'button', classe: 'perigo pequeno', texto: 'Cancelar',
                onclick: function () { acao('/admin/revogar', { id: l.id }, 'Cancelar a licença ' + (NOME[l.pacote] || l.pacote) + ' de ' + email + '? Os plugins desse pacote travam na próxima conferência.'); } })
            : el('button', { type: 'button', classe: 'discreto pequeno', texto: 'Reativar',
                onclick: function () { acao('/admin/reativar', { id: l.id }); } })
        ]));
      });
      lista.appendChild(bloco);
    });
  });
}

document.addEventListener('DOMContentLoaded', function () {
  var caixa = $('pacotes');
  PACOTES.forEach(function (p) {
    caixa.appendChild(el('label', {}, [ el('input', { type: 'checkbox', name: 'pacote', value: p[0] }), p[1] ]));
  });

  $('fLogin').addEventListener('submit', function (ev) {
    ev.preventDefault();
    var f = ev.target; msg('msgLogin', '');
    api('POST', '/painel/entrar', { usuario: f.usuario.value, senha: f.senha.value }).then(function (j) {
      if (j.ok) mostrarPainel(j.usuario); else msg('msgLogin', j.mensagem || 'Erro.');
    });
  });
  $('fNova').addEventListener('submit', criar);
  $('fBusca').addEventListener('submit', function (ev) { ev.preventDefault(); carregar(ev.target.q.value.trim()); });
  $('btCopiar').addEventListener('click', function () {
    navigator.clipboard.writeText($('resMensagem').textContent).then(function () { $('btCopiar').textContent = 'Copiado!';
      setTimeout(function () { $('btCopiar').textContent = 'Copiar mensagem'; }, 1500); });
  });
  $('btSair').addEventListener('click', function () { api('POST', '/painel/sair', {}).then(mostrarLogin); });
  $('btSenha').addEventListener('click', function () { $('fSenha').reset(); msg('msgSenha', ''); $('dSenha').showModal(); });
  $('btSenhaFechar').addEventListener('click', function () { $('dSenha').close(); });
  $('fSenha').addEventListener('submit', function (ev) {
    ev.preventDefault();
    var f = ev.target;
    api('POST', '/painel/senha', { atual: f.atual.value, nova: f.nova.value }).then(function (j) {
      if (j.ok) { msg('msgSenha', 'Senha trocada.', true); setTimeout(function () { $('dSenha').close(); }, 900); }
      else msg('msgSenha', j.mensagem || 'Erro.');
    });
  });

  api('GET', '/painel/sessao').then(function (j) { if (j.ok) mostrarPainel(j.usuario); else mostrarLogin(); });
});
`;
