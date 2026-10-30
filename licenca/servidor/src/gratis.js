// Página grátis do MONTA-MONTA: a pessoa põe o e-mail e recebe na tela o token e os links de download, sem passar
// pelo painel. É o link mandado no direct do Instagram para quem comenta QUERO.
//
//   GET  /gratis/montamonta   a página
//   POST /gratis/montamonta   { email, site }   (site = campo escondido; robô que preenche leva "ok" falso)
//     -> { ok, novo, token?, mesmo_token?, ja_tinha?, baixar: { win, mac } }
//
// Regras:
//   - e-mail sem cadastro: cria a licença montamonta (origem "gratis") e mostra o token UMA vez;
//   - e-mail que já tem outra licença: entra o montamonta com o mesmo token (o token não aparece: ninguém lê o token
//     de outro e-mail digitando o e-mail dele);
//   - e-mail que já tem montamonta: não muda nada, só lembra de usar o token que já recebeu.
//   - Como o token sai na tela sem conferir o e-mail, quem cadastra o e-mail de outra pessoa conhece o token dele.
//     Por isso, quando uma venda ou cortesia entra num e-mail que só tem licença "gratis", o servidor troca o token
//     (ver /admin/licencas em index.js) e o token da página deixa de valer para o pacote pago.
//   - Até 8 cadastros por IP em 24 h e 300 por dia no total.

const PACOTE = 'montamonta';
const POR_IP_DIA = 8;
const TOTAL_DIA = 300;

function json(obj, status = 200) {
    return new Response(JSON.stringify(obj), {
        status, headers: { 'content-type': 'application/json; charset=utf-8', 'cache-control': 'no-store' },
    });
}

export async function rotaGratis(req, env, url, h) {
    if (req.method === 'GET') return new Response(PAGINA, {
        headers: { 'content-type': 'text/html; charset=utf-8', 'cache-control': 'no-store',
                   'content-security-policy': "default-src 'none'; style-src 'unsafe-inline'; script-src 'unsafe-inline'; connect-src 'self'; img-src data:; base-uri 'none'; form-action 'none'; frame-ancestors 'none'" },
    });
    if (req.method !== 'POST') return json({ ok: false, mensagem: 'Método não aceito.' }, 405);

    const ip = req.headers.get('cf-connecting-ip') || '';
    let corpo;
    try { corpo = await req.json(); } catch { return json({ ok: false, mensagem: 'Pedido inválido.' }, 400); }
    const email = h.normEmail(corpo.email);
    const baixar = { win: url.origin + '/baixar/montamonta/win', mac: url.origin + '/baixar/montamonta/mac' };
    if (corpo.site) return json({ ok: true, novo: false, ja_tinha: true, baixar });   // robô
    if (email.length > 120 || !/^[^@\s]+@[^@\s]+\.[^@\s]+$/.test(email))
        return json({ ok: false, mensagem: 'Confira o e-mail.' }, 400);

    const desde = h.agora() - 86400;
    const n = await env.DB.prepare("SELECT SUM(ip = ?) AS ip, COUNT(*) AS total FROM eventos WHERE tipo = 'gratis' AND quando > ?")
        .bind(ip, desde).first();
    if ((n?.ip || 0) >= POR_IP_DIA || (n?.total || 0) >= TOTAL_DIA)
        return json({ ok: false, mensagem: 'Muitos pedidos agora. Tente de novo amanhã ou chame a gente no direct.' }, 429);

    const doEmail = await env.DB.prepare('SELECT pacote, token_hash, status FROM licencas WHERE email = ?').bind(email).all();
    const lista = doEmail.results || [];
    if (lista.some((l) => l.pacote === PACOTE)) {
        await h.evento(env, null, 'gratis_ja', email, ip);
        return json({ ok: true, novo: false, ja_tinha: true, baixar });
    }
    const token = lista.length ? null : h.novoToken();
    const hash = lista.length ? lista[0].token_hash : await h.sha256Hex('ISP' + h.normToken(token));
    const r = await env.DB.prepare(
        "INSERT INTO licencas (email, pacote, token_hash, max_maquinas, origem, nota, criada_em) VALUES (?, ?, ?, 2, 'gratis', 'página grátis', ?)")
        .bind(email, PACOTE, hash, h.agora()).run();
    await h.evento(env, r.meta.last_row_id, 'gratis', lista.length ? 'mesmo token' : 'novo', ip);
    return json({ ok: true, novo: true, token, mesmo_token: !!lista.length, baixar });
}

const PAGINA = `<!doctype html>
<html lang="pt-BR"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1">
<title>MONTA-MONTA grátis · InsideStudioSP</title>
<style>
  :root { --bg:#0e0f15; --card:#1b1d26; --line:#2c303d; --ink:#e9ecf5; --dim:#8d93a8; --vco:#ff8a2a; --vcf:#22d8ff; --amp:#5fe36a; --out:#ff4f8b; --gold:#ffd23a; }
  * { box-sizing:border-box; }
  body { margin:0; min-height:100vh; background:radial-gradient(700px 500px at 85% 0%, rgba(34,216,255,.12), transparent 70%),
         radial-gradient(700px 500px at 0% 100%, rgba(255,138,42,.10), transparent 70%), var(--bg);
         color:var(--ink); font:16px/1.5 "Segoe UI", system-ui, -apple-system, Arial, sans-serif; }
  main { max-width:520px; margin:0 auto; padding:36px 20px 60px; }
  .logo { display:flex; align-items:center; gap:14px; }
  .q { display:grid; grid-template-columns:22px 22px; gap:3px; }
  .q i { width:22px; height:22px; border-radius:5px; display:block; }
  h1 { margin:0; white-space:nowrap; font:900 clamp(26px, 8.4vw, 38px)/1 "Segoe UI Black", "Segoe UI", system-ui, sans-serif; letter-spacing:-.5px; }
  h1 .a { color:var(--vco); } h1 .b { color:var(--vcf); }
  .sub { color:var(--dim); font-size:13px; letter-spacing:.12em; font-weight:700; margin-top:6px; }
  .card { background:var(--card); border:1px solid var(--line); border-radius:18px; padding:24px; margin-top:26px; }
  .free { display:inline-block; background:var(--gold); color:#111318; font-weight:900; padding:2px 10px; border-radius:6px; }
  p { margin:.4em 0; } .en { color:var(--dim); font-size:14px; font-style:italic; }
  label { display:block; font-weight:700; margin:18px 0 6px; }
  input[type=email] { width:100%; padding:14px; border-radius:10px; border:2px solid var(--line); background:#0b0c11; color:var(--ink); font-size:17px; }
  input[type=email]:focus { outline:none; border-color:var(--vcf); }
  button, .bt { display:block; width:100%; text-align:center; padding:15px; margin-top:16px; border:0; border-radius:12px; cursor:pointer;
           font:900 17px "Segoe UI", system-ui, sans-serif; letter-spacing:.04em; text-decoration:none; }
  .principal { background:var(--amp); color:#0b0c11; }
  .principal:disabled { opacity:.6; cursor:wait; }
  .sec { background:#0b0c11; color:var(--ink); border:2px solid var(--line); }
  .sec:hover { border-color:var(--vcf); }
  .token { font:700 clamp(14px, 4.3vw, 19px) Consolas, "SF Mono", monospace; background:#0b0c11; border:2px dashed var(--gold); border-radius:10px; padding:14px; text-align:center;
           word-break:break-all; color:var(--gold); margin-top:8px; user-select:all; }
  .aviso { background:rgba(255,210,58,.08); border:1px solid rgba(255,210,58,.4); border-radius:10px; padding:10px 12px; font-size:14px; margin-top:12px; }
  .erro { color:var(--out); font-weight:700; min-height:1.5em; margin-top:10px; }
  ol { padding-left:20px; margin:8px 0 0; } li { margin:6px 0; }
  .dois { display:grid; grid-template-columns:1fr 1fr; gap:10px; } .dois .bt { margin-top:10px; }
  .esc { position:absolute; left:-5000px; }
  footer { color:var(--dim); font-size:12px; text-align:center; margin-top:26px; }
  [hidden] { display:none !important; }
</style></head>
<body><main>
  <div class="logo"><div class="q"><i style="background:#ff8a2a"></i><i style="background:#22d8ff"></i><i style="background:#5fe36a"></i><i style="background:#ff4f8b"></i></div>
    <div><h1><span class="a">MONTA</span>-<span class="b">MONTA</span></h1><div class="sub">INSIDE STUDIO SP · SYNTH DIDÁTICO</div></div></div>

  <section class="card" id="passo1">
    <p><span class="free">GRÁTIS</span> &nbsp;Monte um sintetizador peça por peça e aprenda o caminho do som. VST3 e AU, Windows e Mac.</p>
    <p class="en">Free learning synth: build it piece by piece. VST3 / AU, Windows and Mac.</p>
    <form id="f" novalidate>
      <label for="email">Seu e-mail <span class="en">· your e-mail</span></label>
      <input id="email" type="email" autocomplete="email" required placeholder="voce@email.com">
      <input class="esc" name="site" id="site" tabindex="-1" autocomplete="off" aria-hidden="true">
      <button class="principal" id="ir" type="submit">RECEBER MONTA-MONTA</button>
      <div class="erro" id="erro"></div>
    </form>
    <p class="en" style="margin-top:8px">O e-mail é só para a sua licença. · Your e-mail is used only for your license.</p>
  </section>

  <section class="card" id="passo2" hidden>
    <div id="comToken" hidden>
      <p><b>Pronto! Este é o seu token:</b> <span class="en">· your token</span></p>
      <div class="token" id="token"></div>
      <button class="sec" id="copiar" type="button">COPIAR TOKEN</button>
      <div class="aviso"><b>Guarde o token</b>: ele aparece só agora (tire um print). <span class="en">It is shown only once, save it.</span></div>
    </div>
    <div id="semToken" hidden>
      <p id="txtSem"></p>
    </div>
    <label>1. Baixe e instale <span class="en">· download and install</span></label>
    <div class="dois"><a class="bt principal" id="win" href="#">WINDOWS</a><a class="bt principal" id="mac" href="#">MAC</a></div>
    <label>2. Ative <span class="en">· activate</span></label>
    <ol>
      <li>Abra o MONTA-MONTA no seu DAW (Ableton, FL, Logic, Reaper…).</li>
      <li>Na tela de ativação, digite <b id="emailOk"></b> e o token, e clique em <b>ATIVAR</b>.</li>
      <li>Vale para 2 computadores. Para trocar de PC: menu do plugin → <b>Licença…</b> → <b>Desativar este computador</b>.</li>
    </ol>
    <p class="en">Open it in your DAW, type your e-mail and token, click ATIVAR. Works on 2 computers.</p>
  </section>
  <footer>InsideStudioSP</footer>
</main>
<script>
(function () {
  var $ = function (id) { return document.getElementById(id); };
  $('f').addEventListener('submit', function (ev) {
    ev.preventDefault();
    var email = $('email').value.trim();
    $('erro').textContent = '';
    if (!/^[^@\\s]+@[^@\\s]+\\.[^@\\s]+$/.test(email)) { $('erro').textContent = 'Confira o e-mail.'; return; }
    $('ir').disabled = true; $('ir').textContent = 'GERANDO…';
    fetch(location.pathname, { method: 'POST', headers: { 'content-type': 'application/json' },
      body: JSON.stringify({ email: email, site: $('site').value }) })
      .then(function (r) { return r.json(); })
      .then(function (j) {
        $('ir').disabled = false; $('ir').textContent = 'RECEBER MONTA-MONTA';
        if (!j.ok) { $('erro').textContent = j.mensagem || 'Não deu certo. Tente de novo.'; return; }
        $('win').href = j.baixar.win; $('mac').href = j.baixar.mac; $('emailOk').textContent = email.toLowerCase();
        if (j.token) { $('token').textContent = j.token; $('comToken').hidden = false; }
        else {
          $('txtSem').innerHTML = j.ja_tinha
            ? '<b>Este e-mail já recebeu o MONTA-MONTA.</b> Use o token que apareceu da primeira vez. Perdeu? Chame a gente no direct.'
            : '<b>Pronto! O MONTA-MONTA entrou no seu cadastro.</b> Use o mesmo token que você já recebeu da InsideStudioSP.';
          $('semToken').hidden = false;
        }
        $('passo1').hidden = true; $('passo2').hidden = false; window.scrollTo(0, 0);
      })
      .catch(function () { $('ir').disabled = false; $('ir').textContent = 'RECEBER MONTA-MONTA'; $('erro').textContent = 'Sem conexão. Tente de novo.'; });
  });
  $('copiar').addEventListener('click', function () {
    var t = $('token').textContent;
    (navigator.clipboard ? navigator.clipboard.writeText(t) : Promise.reject()).then(function () { $('copiar').textContent = 'COPIADO ✓'; },
      function () { var s = window.getSelection(), r = document.createRange(); r.selectNodeContents($('token')); s.removeAllRanges(); s.addRange(r); });
  });
})();
</script>
</body></html>`;
