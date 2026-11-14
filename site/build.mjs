// Gera o site estático em site/dist: node site/build.mjs
//   /                 loja (VASSOURA e ALIGN em destaque + todos os packs)
//   /<pack>/          página do pack (plugins, galeria, compra)
//   /estudio/         o estúdio e o artista (fotos, vídeos, Spotify, YouTube)
//   /contato/         formulário de contato: POST /contato/enviar (servidor de licenças) ⇒ e-mail para contato@ ⇒ Caixa de entrada do painel
// Texto bilíngue no mesmo HTML: <span class="pt"> e <span class="en">; o site.js troca pelo atributo lang.

import { mkdirSync, rmSync, writeFileSync, copyFileSync, readdirSync } from 'node:fs';
import { join, dirname } from 'node:path';
import { fileURLToPath } from 'node:url';
import { SITE, PACKS, LIC, FAQ, OFERTA_FIM, OFERTA_ATIVA, SECOES } from './dados.mjs';
import { ICONES } from './icones.mjs';

const RAIZ = dirname(fileURLToPath(import.meta.url));
const DIST = join(RAIZ, 'dist');
const PREVIA = process.env.PREVIA === '1';   // site oficial desde 05/11/2026; PREVIA=1 gera a versão noindex com a faixa

const esc = (s) => String(s).replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;');
const t = (pt, en) => `<span class="pt">${esc(pt)}</span><span class="en" lang="en">${esc(en ?? pt)}</span>`;
const brl = (v) => v.toLocaleString('pt-BR', { style: 'currency', currency: 'BRL' });
const usd = (v) => 'US$ ' + v.toFixed(2);
const temBrl = (p) => typeof p.preco?.brl === 'number', temUsd = (p) => typeof p.preco?.usd === 'number';
// preço na etiqueta: reais em PT; em EN, dólares se houver (PayPal), senão reais
const valor = (p) => t(brl(p.preco.brl), temUsd(p) ? usd(p.preco.usd) : brl(p.preco.brl));
// oferta de lançamento: "de" riscado + desconto (só em reais; em dólar não há "de")
const off = (p) => Math.round((1 - p.preco.brl / p.de) * 100);
// requisitos de sistema (CMakeLists: macOS mínimo 11.0, universal arm64+x86_64; instalador Windows só x64)
const REQ = () => t('Requisitos: Windows 10 ou 11 (64 bits) · macOS 11 Big Sur ou mais novo (Apple Silicon e Intel)', 'Requirements: Windows 10 or 11 (64-bit) · macOS 11 Big Sur or newer (Apple Silicon and Intel)');
const ehVst = (p) => !p.direto;   // a VASSOURA é Max for Live: segue o requisito do Ableton, não o dos instaladores
const deRiscado = (p) => p.de && temBrl(p) ? `<s class="de so-oferta" data-fim="${p.fim}">${brl(p.de)}</s>` : '';
// contador regressivo (site.js atualiza a cada segundo; sem JS fica a data final)
const fimTxt = (fim) => new Date(fim).toLocaleDateString('pt-BR', { timeZone: 'America/Sao_Paulo', day: '2-digit', month: '2-digit' });
const contador = (cls = '', fim = OFERTA_FIM) => `<span class="contador ${cls}" data-fim="${fim}">${t('até ' + fimTxt(fim), 'until ' + fimTxt(fim))}</span>`;   // fim do pack (fimOferta) ou a oferta geral
const maiorOff = Math.max(0, ...PACKS.filter((p) => p.de && temBrl(p)).map((p) => off(p)));

const ICONE_CARRINHO = '<svg viewBox="0 0 24 24" width="18" height="18" aria-hidden="true"><path fill="currentColor" d="M7 18a2 2 0 1 0 0 4 2 2 0 0 0 0-4zm10 0a2 2 0 1 0 0 4 2 2 0 0 0 0-4zM5.2 4l.6 2H21l-2.4 8.4a2 2 0 0 1-1.9 1.6H8.4a2 2 0 0 1-1.9-1.4L3.7 4H1V2h4.2l1 2z"/></svg>';

function pagina({ titulo, descricao, corpo, raiz = '' }) {
    return `<!doctype html>
<html lang="pt-BR"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1">
<title>${esc(titulo)}</title>
<meta name="description" content="${esc(descricao)}">
${PREVIA ? '<meta name="robots" content="noindex, nofollow">' : ''}
<meta property="og:title" content="${esc(titulo)}"><meta property="og:description" content="${esc(descricao)}">
<meta name="theme-color" content="#0d0b14">
<link rel="icon" href="/favicon.svg" type="image/svg+xml">
<link rel="stylesheet" href="${raiz}/estilo.css">
<script src="${raiz}/site.js" defer></script>
</head><body>
<header class="topo"><div class="faixa">
  <a class="marca" href="${raiz}/">Inside<b>Studio</b>SP</a>
  <nav>
    <a href="${raiz}/">${t('Loja', 'Shop')}</a>
    <a href="${raiz}/estudio/">${t('Estúdio', 'Studio')}</a>
    <a href="${raiz}/contato/">${t('Contato', 'Contact')}</a>
    <a class="nav-carrinho" href="${raiz}/carrinho/" hidden aria-label="Carrinho / Cart">${ICONE_CARRINHO}<b class="carrinho-n">0</b></a>
    <button class="idioma" type="button" aria-label="Idioma / Language"><span class="pt">EN</span><span class="en">PT</span></button>
  </nav>
</div></header>
${OFERTA_ATIVA && maiorOff ? `<a class="faixa-oferta so-oferta" data-fim="${OFERTA_FIM}" href="${raiz}/#packs"><b>${t('OFERTA DE LANÇAMENTO', 'LAUNCH OFFER')}</b> ${t(`até ${maiorOff}% off nos packs`, `up to ${maiorOff}% off all packs`)} <span class="sep">·</span> ${t('termina em', 'ends in')} ${contador()}</a>` : ''}
${PREVIA ? `<div class="previa">${t('Prévia do site novo.', 'Preview of the new site.')}</div>` : ''}
<main>${corpo}</main>
<footer id="contato" class="rodape"><div class="faixa">
  <div>
    <div class="marca">Inside<b>Studio</b>SP</div>
    <p>${t('Plugins, mix e master em São Paulo.', 'Plugins, mixing and mastering from São Paulo, Brazil.')}</p>
  </div>
  <div class="contatos">
    <a href="mailto:${SITE.email}">${SITE.email}</a>
    <a href="${SITE.instagram}" rel="noopener" target="_blank">Instagram @equalizedmusic</a>
  </div>
  <small>© ${new Date().getFullYear()} InsideStudioSP · <a href="${raiz}/privacidade/">${t('Privacidade e cookies', 'Privacy and cookies')}</a></small>
</div></footer>
<a class="nav-carrinho carrinho-flutuante" href="${raiz}/carrinho/" hidden aria-label="Carrinho / Cart">${ICONE_CARRINHO}<b class="carrinho-n">0</b></a>
<dialog class="zoom"><button class="fechar" type="button" aria-label="Fechar / Close">✕</button><img alt=""></dialog>
</body></html>`;
}

function caixaCompra(p) {
    if (p.gratis) return `<div class="compra">
    <div class="preco"><span class="selo-gratis">${t('GRÁTIS', 'FREE')}</span></div>
    <a class="bt bt-principal" href="${p.gratis}">${t('Baixar grátis', 'Free download')}</a>
    <p class="nota">${p.direto
        ? t('Download direto, sem cadastro. Um zip só para Windows e Mac, com o LEIA-ME de instalação.', 'Direct download, no sign-up. One zip for Windows and Mac, with the install README.')
        : t('Cadastre o e-mail e receba o token de ativação e o download na hora.', 'Enter your e-mail and get the activation token and download right away.')}</p>
  </div>`;
    // formulário comum (sem JS): o servidor de licenças cria a cobrança e manda para o Mercado Pago ou o PayPal
    if (!temBrl(p) && !temUsd(p)) return `<div class="compra">
    <div class="preco"><span class="selo-breve">${t('EM BREVE', 'COMING SOON')}</span></div>
    <p class="nota">${t('Lançamento em breve. Siga o estúdio no Instagram para saber quando chegar.', 'Launching soon. Follow the studio on Instagram to know when it lands.')}</p>
    <a class="bt bt-sec" href="${SITE.instagram}" rel="noopener" target="_blank">Instagram @equalizedmusic</a>
  </div>`;
    // 13/11 (pedido dele): a página do pack só põe no carrinho; nome, e-mail e cupom ficam na página do carrinho.
    // O botão é link para /carrinho/?add=<id> (funciona mesmo se o site.js não carregar); com JS, adiciona sem sair da página.
    return `<div class="compra">
    ${p.de && temBrl(p) ? `<div class="oferta so-oferta" data-fim="${p.fim}">${p.bf ? t(`PREÇO PROMOCIONAL ATÉ A BLACK FRIDAY (${fimTxt(p.fim)})`, `PROMO PRICE UNTIL BLACK FRIDAY (${fimTxt(p.fim)})`) : t('OFERTA DE LANÇAMENTO', 'LAUNCH OFFER')} <b>-${off(p)}%</b></div>
    ${p.bf ? relogio(p.fim) : `<div class="tempo so-oferta" data-fim="${p.fim}">${t('Tempo limitado · termina em', 'Limited time · ends in')} ${contador('forte', p.fim)}</div>`}
    <div class="preco-de so-oferta" data-fim="${p.fim}">${t('de', 'from')} ${deRiscado(p)} ${t('por', 'now')}</div>` : ''}
    ${p.soma && temBrl(p) && p.soma > p.preco.brl ? `<div class="preco-de">${t('separados', 'separately')} <s class="de">${brl(p.soma)}</s> ${t('juntos por', 'together for')}</div>` : ''}
    <div class="preco">${temBrl(p) ? valor(p) : usd(p.preco.usd)}</div>
    <p class="req">${REQ()}</p>
    <a class="bt bt-principal bt-carrinho" href="/carrinho/?add=${p.id}" data-add="${p.id}">${t('+ Adicionar ao carrinho', '+ Add to cart')}</a>
    <div class="no-carrinho" hidden>
      <p class="no-carrinho-ok">${t('✓ ' + p.nome + ' está no seu carrinho', '✓ ' + p.nome + ' is in your cart')}</p>
      <a class="bt bt-principal" href="/carrinho/">${t('Finalizar compra →', 'Checkout →')}</a>
      <a class="bt bt-sec" href="/#packs">${t('Continuar comprando', 'Keep shopping')}</a>
    </div>
    ${avisoCompletaCombo(p)}
    <p class="nota">${t('Pix ou cartão em até 12x. Nome, e-mail e cupom de desconto você informa no carrinho, e pode juntar vários packs num pagamento só.',
                        'Pix or card. You enter your name, e-mail and discount coupon in the cart, and you can put several packs in one payment.')}</p>
    <p class="nota">${t('Pagamento aprovado: você recebe no e-mail o token de ativação, o link de download e as instruções. O token vale em 2 computadores.',
                        'Once the payment is approved you get the activation token, the download link and the instructions by e-mail. The token works on 2 computers.')}</p>
  </div>`;
}

// completar o combo (12/11): quem já comprou o outro plugin do combo paga só o que falta (servidor: loja.js creditosCombo)
function avisoCompletaCombo(p) {
    const c = PACKS.find((x) => x.combo?.includes(p.id) && temBrl(x));
    if (!c) return '';
    const outro = PACKS.find((x) => x.id === c.combo.find((y) => y !== p.id));
    if (!outro) return '';
    return `<p class="nota nota-combo">${t(`Já comprou o ${outro.nome}? No carrinho, use o mesmo e-mail da compra: o ${p.nome} sai só pelo que falta para fechar o combo dos dois (${brl(c.preco.brl)}). O valor com o abatimento aparece no Mercado Pago, antes de você pagar.`,
        `Already bought ${outro.nome}? In the cart, use the same e-mail:${p.nome} costs only what is left to reach the bundle price (${brl(c.preco.brl)}). The reduced amount shows on Mercado Pago before you pay.`)}</p>`;
}

// cupom de desconto (12/11): o site.js confere em /loja/cupom e mostra o total com desconto; quem cobra é o servidor.
// data-total = valor em reais sem desconto (no carrinho o site.js atualiza ao mudar os itens).
function campoCupom(total) {
    return `<div class="cupom" data-total="${typeof total === 'number' ? total : ''}">
      <label class="campo">${t('Cupom de desconto (opcional)', 'Discount coupon (optional)')}
        <span class="cupom-linha"><input name="cupom" maxlength="40" autocomplete="off" autocapitalize="characters" spellcheck="false" placeholder="#CUPOM"><button class="bt bt-sec cupom-aplicar" type="button" hidden>${t('Aplicar', 'Apply')}</button></span></label>
      <p class="cupom-msg" aria-live="polite"></p>
    </div>`;
}

// página do carrinho (12/11): a lista vive no navegador (localStorage, site.js); o formulário manda um "pacote" por item
// para o mesmo /loja/pagar, que cobra o total (com cupom) num pagamento só e cria uma licença por pack.
function carrinho() {
    const catalogo = Object.fromEntries(PACKS.filter((p) => temBrl(p)).map((p) => [p.id, {
        nome: p.nome, brl: p.preco.brl, de: p.de && OFERTA_ATIVA ? p.de : null, fim: p.fim || null, capa: p.capa, cor: p.cor,
        combo: p.combo || null, tipo: p.tipo }]));
    return pagina({
        titulo: 'Carrinho — InsideStudioSP',
        descricao: 'Seu carrinho na loja InsideStudioSP.',
        corpo: `
<section class="hero hero-curto"><div class="faixa">
  <p class="sobre">${t('CARRINHO', 'CART')}</p>
  <h1>${t('Seu carrinho', 'Your cart')}</h1>
  <p class="lead">${t('Junte os packs que quiser e pague tudo de uma vez, com Pix ou cartão em até 12x. Cada pack chega com a licença no seu e-mail.',
                      'Pick the packs you want and pay for everything at once. Each pack comes with its license in your e-mail.')}</p>
</div></section>
<section class="faixa carrinho-grade">
  <div class="carrinho-lista" id="carrinho-lista">
    <noscript><p>${t('O carrinho precisa de JavaScript ligado. Sem ele, compre cada pack pela página dele.', 'The cart needs JavaScript. Without it, buy each pack from its own page.')}</p></noscript>
  </div>
  <form class="compra carrinho-form" id="carrinho-form" method="post" action="${LIC}/loja/pagar" hidden>
    <input type="hidden" name="carrinho" value="1">
    <div class="carrinho-itens"></div>
    <dl class="carrinho-resumo">
      <dt>${t('Subtotal', 'Subtotal')}</dt><dd class="cr-sub"></dd>
      <dt class="cr-desc" hidden>${t('Cupom', 'Coupon')} <span class="cr-cod"></span></dt><dd class="cr-desc cr-desc-v" hidden></dd>
      <dt class="cr-total-t">${t('Total', 'Total')}</dt><dd class="cr-total"></dd>
    </dl>
    <label class="campo">${t('Nome completo', 'Full name')}
      <input name="nome" required minlength="3" maxlength="80" autocomplete="name" placeholder="Nome e sobrenome · Full name"></label>
    <label class="campo">${t('Seu e-mail (as licenças chegam nele)', 'Your e-mail (the licenses are sent to it)')}
      <input type="email" name="email" required maxlength="120" autocomplete="email" placeholder="voce@email.com"></label>
    ${campoCupom(null)}
    <button class="bt bt-principal" name="metodo" value="mp">${t('Pagar com Pix ou cartão em até 12x', 'Pay with Pix or card (Brazil)')}</button>
    <p class="nota">${t('Pagamento aprovado: chega no seu e-mail um token de ativação que vale para todos os packs, os links de download de cada um e as instruções. O token vale em 2 computadores.',
                        'Once the payment is approved you get one activation token for all the packs, the download links for each one and the instructions by e-mail. The token works on 2 computers.')}</p>
    <p class="nota">${t('Nome e e-mail são usados só para a licença e o suporte.', 'Your name and e-mail are used only for the license and support.')} <a href="/privacidade/">${t('Privacidade', 'Privacy')}</a></p>
  </form>
</section>
<script type="application/json" id="catalogo">${JSON.stringify(catalogo).replace(/</g, '\\u003c')}</script>`,
    });
}

function cartaoPack(p, grande = false) {
    const etiqueta = p.gratis ? `<span class="selo-gratis">${t('GRÁTIS', 'FREE')}</span>`
        : temBrl(p) || temUsd(p) ? `<span class="valor">${deRiscado(p)}${temBrl(p) ? valor(p) : usd(p.preco.usd)}</span>`
        : `<span class="selo-breve">${t('EM BREVE', 'SOON')}</span>`;
    return `<a class="pack${grande ? ' pack-grande' : ''}" href="/${p.id}/" style="--cor:${p.cor}">
    <div class="pack-img"><img src="/img/${p.capa}.webp" alt="${esc(p.nome)}" loading="lazy"></div>
    <div class="pack-txt">
      <div class="pack-topo"><h3>${esc(p.nome)}</h3>${etiqueta}</div>
      <p>${t(...p.frase)}</p>
      <small>${t(...p.tipo)}</small>
    </div>
  </a>`;
}

// contagem regressiva GRANDE da promoção (pedido dele, 10/11): blocos dias/horas/min/seg que o site.js atualiza a cada segundo
function relogio(fim) {
    const r = Math.max(0, Math.floor((Date.parse(fim) - Date.now()) / 1000)), d2 = (n) => String(n).padStart(2, '0');
    const bloco = (u, v, pt, en) => `<span class="rl-b"><b data-u="${u}">${d2(v)}</b><small>${t(pt, en)}</small></span>`;
    return `<div class="relogio so-oferta" data-fim="${fim}" role="timer" aria-label="A promoção acaba em / The promo ends in">
      <p class="rl-tit">${t('A promoção acaba em', 'The promo ends in')}</p>
      <div class="rl-blocos">${bloco('d', Math.floor(r / 86400), 'dias', 'days')}<i>:</i>${bloco('h', Math.floor(r % 86400 / 3600), 'horas', 'hours')}<i>:</i>${bloco('m', Math.floor(r % 3600 / 60), 'min', 'min')}<i>:</i>${bloco('s', r % 60, 'seg', 'sec')}</div>
    </div>`;
}

// popup de lançamento na entrada da loja (pedido dele, 09/11: o banner no topo repetia a "oferta de lançamento" da faixa e confundia).
// O pack com "lancamento" em dados.mjs (só se já tiver preço). "Quero" leva à página do pack; ✕ / "ver a loja" / Esc / clique fora fecham.
// site.js abre 1x por sessão do navegador (sessionStorage), para não reabrir a cada volta à home.
function bannerLancamento(p) {
    if (!p || !(temBrl(p) || temUsd(p))) return '';
    // o popup acaba junto com a oferta do pack (decisão dele, 10/11: MAESTRO sai em 28/11 00:00 sem perguntar).
    // O build depois da data já sai sem ele; o site.js tira o que ainda estiver em cache (data-ate).
    if (Date.now() >= Date.parse(p.fim)) return '';
    const L = p.lancamento;
    const bf = p.bf && p.de && temBrl(p);   // cara de Black Friday enquanto o preço promocional vale
    return `<dialog class="dl${bf ? ' dl-bf' : ''}" data-ate="${p.fim}" style="--cor:${p.cor}" aria-label="${esc(p.nome)}">
  <button class="dl-fechar" type="button" aria-label="Fechar / Close">✕</button>
  ${bf ? `<div class="dl-faixa-bf so-oferta" data-fim="${p.fim}"><b>BLACK FRIDAY</b><span>${t(`preço promocional até ${fimTxt(p.fim)}`, `promo price until ${fimTxt(p.fim)}`)}</span></div>` : ''}
  <div class="dl-grade">
  <div class="dl-txt">
    <p class="dl-selo"><span class="tag">${t('LANÇAMENTO', 'NEW RELEASE')}</span>${p.de && temBrl(p) ? ` <b class="so-oferta" data-fim="${p.fim}">-${off(p)}%</b>` : ''}</p>
    <h2 class="dl-nome">${esc(p.nome)}</h2>
    <p class="dl-tit">${t(...L.titulo)}</p>
    <p class="dl-lead">${t(...L.texto)}</p>
    <div class="dl-preco">
      ${p.de && temBrl(p) ? `<span class="dl-de so-oferta" data-fim="${p.fim}">${t('de', 'from')} ${deRiscado(p)} ${t('por', 'now')}</span>` : ''}
      <span class="dl-valor">${temBrl(p) ? valor(p) : usd(p.preco.usd)}</span>
      ${p.de && temBrl(p) ? `<span class="tempo so-oferta" data-fim="${p.fim}">${bf ? t(`Preço promocional até a Black Friday (${fimTxt(p.fim)})`, `Promo price until Black Friday (${fimTxt(p.fim)})`) : t('Oferta de lançamento · termina em', 'Launch offer · ends in')} ${bf ? '' : contador('forte', p.fim)}</span>` : ''}
      ${bf ? relogio(p.fim) : ''}
    </div>
    ${comboDoBanner(L)}
    <div class="dl-bts">
      <a class="bt bt-principal" href="/${p.id}/">${t('Quero o ' + p.nome, 'Get ' + p.nome)}</a>
      <button class="dl-depois" type="button">${t('Agora não, ver a loja', 'Not now, see the shop')}</button>
    </div>
    <p class="dl-tipo">${t(...p.tipo)} · ${t('Windows e Mac · Pix ou cartão em até 12x', 'Windows and Mac')}</p>
  </div>
  <a class="dl-img" href="/${p.id}/"><img src="/img/${p.capa}.webp" alt="${esc(p.nome)}"></a>
</div></dialog>`;
}

// caixa do combo no popup (11/11; 11/11 noite, pedido dele: no celular a linha ficava abaixo do "Agora não" e sumia —
// virou caixa em destaque ENTRE o preço e o botão de compra, visível sem rolar)
function comboDoBanner(L) {
    const c = L.combo && PACKS.find((x) => x.id === L.combo);
    if (!c || !temBrl(c)) return '';
    const nomes = c.combo.map((id) => PACKS.find((x) => x.id === id)).filter(Boolean).map((x) => x.nome).join(' + ');
    const soma = c.soma > c.preco.brl ? `<s>${brl(c.soma)}</s> ` : '';
    return `<a class="dl-combo" href="/${c.id}/"><span class="dl-combo-tag">COMBO</span><span class="dl-combo-txt"><b>${esc(nomes)}</b><span>${t('os dois por', 'both for')} ${soma}<strong>${brl(c.preco.brl)}</strong></span></span><span class="dl-combo-seta">→</span></a>`;
}

// ---- seções da loja (10/11) ----
const PACK = Object.fromEntries(PACKS.map((p) => [p.id, p]));
const itensSecao = (s) => s.itens.map((id) => PACK[id]).filter(Boolean);
const secaoDoPack = (p) => SECOES.find((s) => s.id !== 'gratis' && s.itens.includes(p.id)) || SECOES.find((s) => s.itens.includes(p.id));
const urlSecao = (s) => `/plugins/${s.id}/`;

// card de uma seção: leque com as capas dos produtos + nomes (cada nome leva direto ao produto) + resumo de preço.
// O card inteiro leva à página da seção (link "esticado" no título; os nomes ficam por cima dele).
function cartaoSecao(s, largo = false) {
    const it = itensSecao(s), n = it.length;
    const pagos = it.filter((p) => temBrl(p)), gratis = it.filter((p) => p.gratis);
    const menor = pagos.length ? Math.min(...pagos.map((p) => p.preco.brl)) : 0;
    const maxOff = Math.max(0, ...pagos.filter((p) => p.de).map((p) => off(p)));
    const fimOff = pagos.filter((p) => p.de).map((p) => p.fim).sort().pop();
    const resumo = s.id === 'gratis' ? `<span class="selo-gratis">${t('GRÁTIS', 'FREE')}</span>`
        : [menor ? `<span class="cat-preco">${t('a partir de ' + brl(menor), 'from ' + brl(menor))}</span>` : '',
           gratis.length ? `<span class="cat-tem-gratis">${t(`${gratis.length} grátis`, `${gratis.length} free`)}</span>` : '',
           maxOff ? `<span class="cat-off so-oferta" data-fim="${fimOff}">${t(`até -${maxOff}%`, `up to -${maxOff}%`)}</span>` : ''].join('');
    // o 1º da lista (o destaque, ex.: MAESTRO no Mix) fica no meio e na frente; os outros abrem para os lados
    const m = Math.min(n, 6), meio = (m - 1) / 2;
    const vagas = Array.from({ length: m }, (_, k) => k - meio).sort((a, b) => Math.abs(a) - Math.abs(b) || b - a);
    const leque = it.slice(0, 6).map((p, i) => [p, vagas[i]]).sort((a, b) => a[1] - b[1]).map(([p, d]) => {
        return `<img src="/img/${p.capa}.webp" alt="" loading="lazy" style="--i:${d.toFixed(1)};--z:${10 - Math.round(Math.abs(d) * 2)}">`;
    }).join('');
    return `<article class="cat${largo ? ' cat-largo' : ''}" style="--cor:${s.cor}">
    <div class="cat-leque" aria-hidden="true">${leque}</div>
    <div class="cat-txt">
      <div class="cat-topo"><h3><a class="cat-link" href="${urlSecao(s)}">${t(...s.nome)}</a></h3><span class="cat-n">${t(`${n} ${n === 1 ? 'produto' : 'produtos'}`, `${n} ${n === 1 ? 'product' : 'products'}`)}</span></div>
      <p>${t(...s.frase)}</p>
      <p class="cat-resumo">${resumo}</p>
      <ul class="cat-itens">${it.map((p) => `<li><a href="/${p.id}/" style="--cor:${p.cor}">${esc(p.nome)}${p.lancamento ? ` <i>${t('NOVO', 'NEW')}</i>` : ''}</a></li>`).join('')}</ul>
      <span class="cat-ver">${t('Ver a seção', 'See the section')} →</span>
    </div>
  </article>`;
}

function paginaSecao(s) {
    return pagina({
        titulo: `${s.nome[0]} — InsideStudioSP`,
        descricao: s.frase[0],
        corpo: `
<section class="hero hero-secao" style="--cor:${s.cor}"><div class="faixa">
  <a class="voltar" href="/#packs">← ${t('Todas as seções', 'All sections')}</a>
  <h1>${t(...s.nome)}</h1>
  <p class="lead">${t(...s.frase)}</p>
  <nav class="secoes-nav" aria-label="Seções">${SECOES.map((o) => `<a href="${urlSecao(o)}"${o.id === s.id ? ' class="atual" aria-current="page"' : ''}>${t(...o.nome)}</a>`).join('')}</nav>
</div></section>
<section class="faixa"><div class="grade">${itensSecao(s).map((p) => cartaoPack(p)).join('')}</div></section>`,
    });
}

function loja() {
    const [gratis, ...outras] = SECOES;
    return pagina({
        titulo: 'InsideStudioSP — Plugins',
        descricao: 'Plugins VST3/AU e Max for Live feitos em São Paulo: ALIGN, VASSOURA, MESA e mais.',
        corpo: `
${bannerLancamento(PACKS.find((p) => p.lancamento))}
<section class="hero"><div class="faixa">
  <p class="sobre">${t('PLUGINS · MAX FOR LIVE · ESTÚDIO', 'PLUGINS · MAX FOR LIVE · STUDIO')}</p>
  <h1>${t('Ferramentas feitas por quem mixa todo dia.', 'Tools made by someone who mixes every day.')}</h1>
  <p class="lead">${t('Plugins para Windows e Mac, pensados no estúdio para resolver problema de verdade, com a tela mostrando o que está acontecendo.',
                      'Plugins for Windows and Mac, built in the studio to solve real problems, with the screen showing what is going on.')}</p>
</div></section>
<section class="faixa">${cartaoSecao(gratis, true)}</section>
<section id="packs" class="faixa"><h2 class="secao">${t('Seções', 'Sections')}</h2>
  <div class="grade-cat">${outras.map((s) => cartaoSecao(s)).join('')}</div>
</section>
<section id="instalacao" class="faixa como">
  <p class="sobre">${t('INSTALAÇÃO', 'INSTALLATION')}</p>
  <h2 class="titulo-sec">${t('Do pagamento ao seu DAW', 'From checkout to your DAW')}</h2>
  <ol>
    <li><span class="passo">${t('PASSO', 'STEP')}</span>${t('Pague com Pix ou cartão em até 12x. Assim que o pagamento é aprovado, chegam no seu e-mail o token de ativação, o link de download e as instruções.',
        'Pay with Pix or card. As soon as the payment is approved you get the activation token, the download link and the instructions by e-mail.')}</li>
    <li><span class="passo">${t('PASSO', 'STEP')}</span>${t('Rode o instalador. No Windows, o .exe põe os plugins em', 'Run the installer. On Windows, the .exe puts the plugins in')} <code>C:\\Program Files\\Common Files\\VST3</code>. ${t('No Mac, abra o .dmg e rode o .pkg (VST3 e AU em', 'On Mac, open the .dmg and run the .pkg (VST3 and AU in')} <code>/Library/Audio/Plug-Ins</code>). ${t('Precisa de Windows 10 ou 11 (64 bits) ou macOS 11 Big Sur ou mais novo.', 'Requires Windows 10 or 11 (64-bit) or macOS 11 Big Sur or newer.')}</li>
    <li><span class="passo">${t('PASSO', 'STEP')}</span>${t('Reabra o DAW (no Ableton: Preferências › Plug-ins › Rescan), abra o plugin e ative com o e-mail e o token. Vale em 2 computadores.',
        'Reopen your DAW (in Ableton: Settings › Plug-ins › Rescan), open the plugin and activate it with your e-mail and token. Works on 2 computers.')}</li>
  </ol>
</section>
<section id="duvidas" class="faixa faq">
  <p class="sobre">${t('DÚVIDAS', 'FAQ')}</p>
  <h2 class="titulo-sec">${t('Perguntas frequentes', 'Frequently asked questions')}</h2>
  ${FAQ.map(([qp, qe, rp, re]) => `<details><summary>${t(qp, qe)}</summary><p>${t(rp, re)}</p></details>`).join('\n  ')}
</section>
<section class="faixa chamada-estudio">
  <div><h2>${t('Conheça o estúdio', 'Meet the studio')}</h2>
  <p>${t('Mix e master. Os plugins nascem aqui.', 'Mixing and mastering. The plugins are born here.')}</p></div>
  <a class="bt bt-sec" href="/estudio/">${t('Ver o estúdio', 'See the studio')}</a>
</section>`,
    });
}

// Manual em PDF de cada pack (arquivos em site/manuais/, copiados para /manuais/). Combo = os dois manuais.
const MANUAIS = {
    align: ['ALIGN'], barebass: ['BAREBASS'], vassoura: ['VASSOURA'], mesa: ['MESA'], stompbox: ['STOMPBOX'],
    maestro: ['MAESTRO'], magos: ['MAGOS'], maestromagos: ['MAESTRO', 'MAGOS'], criativo: ['SERIE-CRIATIVO'],
    mix: ['SERIE-MIX'], knob: ['SERIE-KNOB'], quimera: ['QUIMERA'], limao: ['LIMAO'], mercurio: ['MERCURIO'],
    resenha: ['RESENHA'], groovador: ['GROOVADOR'],
};
const NOME_MANUAL = { MAGOS: 'MASTER DOS MAGOS', LIMAO: 'LIMÃO', MERCURIO: 'MERCÚRIO', 'SERIE-CRIATIVO': 'SÉRIE CRIATIVO', 'SERIE-MIX': 'SÉRIE MIX', 'SERIE-KNOB': 'SÉRIE KNOB' };
const linkManual = (p) => {
    const m = MANUAIS[p.id];
    if (!m) return '';
    // um link por idioma: PT abre <NOME>-Manual.pdf, EN abre <NOME>-Manual-EN.pdf (a classe pt/en esconde o outro)
    const a = (f, en) => `<a class="${en ? 'en' : 'pt'}"${en ? ' lang="en"' : ''} href="/manuais/${f}-Manual${en ? '-EN' : ''}.pdf" target="_blank" rel="noopener">${en ? 'PDF manual' : 'Manual em PDF'}${m.length > 1 ? ` · ${esc(NOME_MANUAL[f] || f)}` : ''} ↗</a>`;
    return `<p class="manual">${m.map((f) => a(f, false) + a(f, true)).join('<span> · </span>')}</p>`;
};

function paginaPack(p) {
    const galeria = p.plugins.length > 1
        ? `<section class="faixa"><h2 class="secao">${t(`Os ${p.plugins.length} plugins`, `The ${p.plugins.length} plugins`)}</h2>
  <div class="grade-plugins">${p.plugins.map(([img, nome, pt, en]) => `<figure class="plugin">
    <button type="button" class="ampliar" data-img="/img/${img}.webp"><img src="/img/${img}.webp" alt="${esc(nome)}" loading="lazy"></button>
    <figcaption><b>${esc(nome)}</b>${pt ? `<span>${t(pt, en)}</span>` : ''}</figcaption></figure>`).join('')}</div></section>` : '';
    return pagina({
        titulo: `${p.nome} — InsideStudioSP`,
        descricao: p.frase[0],
        corpo: `
<section class="pack-hero" style="--cor:${p.cor}"><div class="faixa pack-hero-grade">
  <div>
    <a class="voltar" href="${urlSecao(secaoDoPack(p))}">← ${t(...secaoDoPack(p).nome)}</a>
    <h1>${esc(p.nome)}</h1>
    <p class="lead">${t(...p.frase)}</p>
    <p>${t(...p.texto)}</p>
    <p class="tipo">${t(...p.tipo)} · Windows · macOS</p>
    ${ehVst(p) ? `<p class="req">${REQ()}</p>` : ''}
    ${p.plugins.length === 1 && p.plugins[0][2] ? `<p class="dim">${t(p.plugins[0][2], p.plugins[0][3])}</p>` : ''}
    ${linkManual(p)}
  </div>
  ${caixaCompra(p)}
</div>
<div class="faixa"><button type="button" class="ampliar capa" data-img="/img/${p.capa}.webp"><img src="/img/${p.capa}.webp" alt="${esc(p.nome)}"></button></div>
</section>
${galeria}`,
    });
}

function estudio() {
    const spotify = SITE.spotify
        ? `<iframe src="${esc(SITE.spotify.replace('open.spotify.com/', 'open.spotify.com/embed/'))}" height="352" loading="lazy" allow="encrypted-media; fullscreen; picture-in-picture" title="Spotify"></iframe>`
        : `<div class="vaga">${t('Player do Spotify — falta o link do artista', 'Spotify player — artist link pending')}</div>`;
    const youtube = SITE.youtube
        ? `<iframe src="https://www.youtube-nocookie.com/embed/${esc(SITE.youtube)}" loading="lazy" allowfullscreen title="YouTube"></iframe>`
        : `<div class="vaga">${t('Vídeo do YouTube — falta o link', 'YouTube video — link pending')}</div>`;
    // Enquanto não houver texto, fotos e links, a página mostra só "em breve" (sem caixas vazias nem [texto]).
    // Quando chegarem: preencher SITE.bio/spotify/youtube em dados.mjs e as fotos em img/ — as seções aparecem sozinhas.
    const temConteudo = SITE.bio || SITE.spotify || SITE.youtube;
    if (!temConteudo) return pagina({
        titulo: 'Estúdio — InsideStudioSP',
        descricao: 'InsideStudioSP: estúdio de mix e master em São Paulo.',
        corpo: `
<section class="hero"><div class="faixa">
  <p class="sobre">${t('ESTÚDIO · SÃO PAULO', 'STUDIO · SÃO PAULO, BRAZIL')}</p>
  <h1>InsideStudioSP</h1>
  <p><span class="selo-breve">${t('EM BREVE', 'COMING SOON')}</span></p>
  <p class="lead">${t('A página do estúdio está chegando: fotos, vídeos, a história e os trabalhos de mix e master. Enquanto isso, acompanhe no Instagram.',
                      'The studio page is on its way: photos, videos, the story and the mixing and mastering work. Meanwhile, follow along on Instagram.')}</p>
  <p><a class="bt bt-sec" href="${SITE.instagram}" rel="noopener" target="_blank">Instagram @equalizedmusic</a>
     <a class="bt bt-sec" href="/contato/">${t('Fale com a gente', 'Get in touch')}</a></p>
</div></section>`,
    });
    const E = SITE.estudio, foto = (img, alt, cls = 'foto-img') => `<button type="button" class="ampliar ${cls}" data-img="/img/${img}.webp"><img src="/img/${img}.webp" alt="${esc(alt)}" loading="lazy"></button>`;
    return pagina({
        titulo: 'Estúdio — InsideStudioSP', raiz: '',
        descricao: 'InsideStudioSP: estúdio de mix e master em São Paulo, do DJ e produtor Equalized.',
        corpo: `
<section class="hero"><div class="faixa">
  <p class="sobre">${t('ESTÚDIO · SÃO PAULO', 'STUDIO · SÃO PAULO, BRAZIL')}</p>
  <h1>InsideStudioSP</h1>
  <p class="lead">${E ? t(...E.lead) : t('Mix e master em São Paulo. Ouça abaixo os lançamentos do Equalized.', 'Mixing and mastering in São Paulo, Brazil. Listen to Equalized releases below.')}</p>
</div></section>
${E ? `<section class="faixa duo">
  <div><h2 class="secao">${t('O estúdio', 'The studio')}</h2>
  ${E.texto.map((p) => `<p>${t(...p)}</p>`).join('\n  ')}
  <p><a class="bt bt-sec" href="/contato/">${t('Peça um orçamento de mix ou master', 'Ask for a mix or master quote')}</a></p></div>
  <div class="fotos-estudio">${(E.fotos || []).map(([img, pt, en]) => foto(img, pt)).join('')}</div>
</section>
${E.maisFotos?.length ? `<section class="faixa"><h2 class="secao">${t('Por dentro do estúdio', 'Inside the studio')}</h2>
  <div class="grade-fotos grade-estudio">${E.maisFotos.map(([img, pt]) => foto(img, pt)).join('')}</div></section>` : ''}` : ''}
${SITE.bio ? `<section class="faixa duo duo-artista">
  ${SITE.retrato ? `<div>${foto(SITE.retrato, 'Equalized', 'foto-img retrato')}</div>` : ''}
  <div><p class="sobre">${t('O ARTISTA', 'THE ARTIST')}</p>
  <h2 class="logo-artista"><img src="/img/equalized-logo.webp" alt="Equalized" width="900" height="213"></h2>
  ${SITE.bio.map((p) => `<p>${t(...p)}</p>`).join('\n  ')}</div>
</section>` : ''}
<section class="faixa artista">
  <div><h2 class="secao">${t('Ouça', 'Listen')}</h2>
  ${SITE.lancamento ? `<a class="lanc" href="${esc(SITE.lancamento.url)}" rel="noopener" target="_blank">
    <span class="lanc-capa"><img src="${esc(SITE.lancamento.capa)}" alt="${esc(SITE.lancamento.titulo)}" width="440" height="440" loading="lazy"><span class="lanc-play" aria-hidden="true"><svg viewBox="0 0 24 24"><path d="M8 5v14l11-7z"/></svg></span></span>
    <span class="lanc-txt"><span class="lanc-rot">${t('LANÇAMENTO · BLUE TUNES RECORDS', 'NEW RELEASE · BLUE TUNES RECORDS')}</span>
    <strong>${esc(SITE.lancamento.titulo)}</strong><span class="lanc-art">Equalized</span>
    <span class="lanc-cta">${t('Clique para ouvir', 'Click to listen')} →</span></span></a>` : ''}
  <p class="links-artista">${(SITE.links || []).map(([nome, url, ic]) => `<a class="ico ico-${ic}" href="${esc(url)}" rel="noopener" target="_blank" aria-label="${esc(nome)}" title="${esc(nome)}"><svg viewBox="0 0 24 24" aria-hidden="true"><path d="${ICONES[ic]}"/></svg></a>`).join('\n  ')}</p>
  <p><a class="bt bt-sec" href="/contato/">${t('Fale com a gente', 'Get in touch')}</a></p></div>
  ${SITE.spotify ? `<div>${spotify}</div>` : ''}
</section>
${SITE.galeria?.length ? `<section class="faixa"><h2 class="secao">${t('Fotos', 'Photos')}</h2>
  <div class="grade-fotos">${SITE.galeria.map((img) => foto(img, 'Equalized')).join('')}</div></section>` : ''}
${SITE.youtube ? `<section class="faixa"><h2 class="secao">${t('Vídeos', 'Videos')}</h2><div class="video">${youtube}</div></section>` : ''}`,
    });
}

function contato() {
    return pagina({
        titulo: 'Contato — InsideStudioSP',
        descricao: 'Fale com o InsideStudioSP: plugins, mix e master.',
        corpo: `
<section class="hero"><div class="faixa">
  <p class="sobre">${t('CONTATO', 'CONTACT')}</p>
  <h1>${t('Fale com a gente', 'Get in touch')}</h1>
  <p class="lead">${t('Dúvida sobre um plugin, licença, mix ou master: escreva aqui que a resposta chega no seu e-mail.',
                      'Questions about a plugin, a license, mixing or mastering: write here and the answer goes to your e-mail.')}</p>
</div></section>
<section class="faixa"><form class="compra form-contato" method="post" action="/contato/enviar">
  <label class="campo">${t('Seu nome', 'Your name')}<input name="nome" required maxlength="80" autocomplete="name"></label>
  <label class="campo">${t('Seu e-mail', 'Your e-mail')}<input type="email" name="email" required maxlength="120" autocomplete="email" placeholder="voce@email.com"></label>
  <label class="campo">${t('Assunto', 'Subject')}<select name="assunto">
    <option value="Plugins">${esc('Plugins / licença · Plugins / license')}</option>
    <option value="Estúdio">${esc('Estúdio: mix, master · Studio')}</option>
    <option value="Outro">${esc('Outro · Other')}</option></select></label>
  <label class="campo">${t('Mensagem', 'Message')}<textarea name="mensagem" required minlength="5" maxlength="5000" rows="7"></textarea></label>
  <label class="isca" aria-hidden="true">Site<input name="site" tabindex="-1" autocomplete="off"></label>
  <button class="bt bt-principal">${t('Enviar mensagem', 'Send message')}</button>
  <p class="nota">${t('Ou escreva direto para', 'Or write straight to')} <a href="mailto:${SITE.email}">${SITE.email}</a></p>
  <p class="nota">${t('Nome, e-mail e mensagem servem só para responder você.', 'Your name, e-mail and message are used only to reply to you.')} <a href="/privacidade/">${t('Privacidade', 'Privacy')}</a></p>
</form></section>`,
    });
}

// Política de privacidade e cookies (LGPD, Lei 13.709/2018). Escrita a partir do que o servidor guarda de fato
// (licenca/servidor: licencas, ativacoes, pedidos, mensagens, eventos). Mudou o que se guarda ⇒ mudar aqui e a data.
function privacidade() {
    const blocos = [
        ['Quem cuida dos seus dados', 'Who looks after your data',
         ['O InsideStudioSP é o estúdio e a loja de plugins de Rogério Cardoso (Equalized), em São Paulo. Ele é o responsável pelos dados tratados neste site. Para qualquer assunto sobre os seus dados, escreva para contato@insidestudiosp.com.br.',
          'InsideStudioSP is the studio and plugin shop of Rogério Cardoso (Equalized), in São Paulo, Brazil. He is responsible for the data handled on this site. For anything about your data, write to contato@insidestudiosp.com.br.']],
        ['Cookies', 'Cookies',
         ['Este site não usa cookies de propaganda, de rastreamento nem de estatística, e não tem Google Analytics, pixel do Facebook ou ferramenta parecida. Por isso não há aviso de cookies para aceitar.',
          'This site does not use advertising, tracking or analytics cookies, and has no Google Analytics, Facebook pixel or similar tool. That is why there is no cookie banner to accept.'],
         ['A exceção é o player do Spotify na página Estúdio. Ele é carregado do próprio Spotify, que pode gravar cookies dele no seu navegador, conforme a política de privacidade do Spotify.',
          'The exception is the Spotify player on the Studio page. It is loaded from Spotify itself, which may set its own cookies in your browser under Spotify\'s privacy policy.']],
        ['O que guardamos e para quê', 'What we keep and why',
         ['Compra: o nome completo, o e-mail, o pack, o valor, a forma de pagamento e a situação do pedido, para emitir e enviar a sua licença. O nome e o e-mail também vão ao Mercado Pago, para identificar a compra. Os dados do cartão e do Pix ficam com o Mercado Pago e não passam por aqui.',
          'Purchase: your full name, e-mail, the pack, the amount, the payment method and the order status, to issue and send your license. Your name and e-mail also go to Mercado Pago, to identify the purchase. Card and Pix details stay with Mercado Pago and never reach us.'],
         ['Licença: o e-mail, o token (guardado só em hash, não dá para ler de volta) e, para cada computador ativado, um código anônimo da máquina e o nome do computador, para controlar o limite de 2 computadores.',
          'License: your e-mail, the token (stored only as a hash, it cannot be read back) and, for each activated computer, an anonymous machine code and the computer name, to enforce the 2-computer limit.'],
         ['Downloads grátis (ALIGN e VASSOURA): o e-mail, para mandar o link e o token.', 'Free downloads (ALIGN and VASSOURA): your e-mail, to send the link and the token.'],
         ['Contato: nome, e-mail e a conversa, para responder você.', 'Contact: name, e-mail and the conversation, to reply to you.'],
         ['Segurança: o endereço IP de compras e de tentativas de ativação, para barrar abuso e fraude.',
          'Security: the IP address of purchases and activation attempts, to block abuse and fraud.']],
        ['Com quem os dados são compartilhados', 'Who the data is shared with',
         ['Só com os serviços que fazem o site funcionar: Mercado Pago (pagamento), Cloudflare (hospedagem e banco de dados) e Resend (envio dos e-mails). Alguns desses serviços ficam fora do Brasil. Nada é vendido nem repassado para propaganda.',
          'Only with the services that run the site: Mercado Pago (payments), Cloudflare (hosting and database) and Resend (sending e-mails). Some of them are outside Brazil. Nothing is sold or shared for advertising.']],
        ['Por quanto tempo', 'How long',
         ['Os dados da licença ficam enquanto a licença existir, porque sem eles o plugin não ativa. Os dados de pedidos ficam pelo prazo exigido pela lei. Mensagens de contato podem ser apagadas a qualquer momento, a seu pedido.',
          'License data is kept for as long as the license exists, because the plugin cannot activate without it. Order data is kept for as long as the law requires. Contact messages can be deleted at any time at your request.']],
        ['Seus direitos', 'Your rights',
         ['Pela LGPD você pode pedir para ver, corrigir ou apagar os seus dados, saber com quem foram compartilhados e retirar o consentimento. Escreva para contato@insidestudiosp.com.br com o e-mail cadastrado. Se você pedir para apagar a licença, o plugin deixa de ativar.',
          'Under Brazil\'s data protection law (LGPD) you can ask to see, correct or delete your data, learn who it was shared with and withdraw consent. Write to contato@insidestudiosp.com.br from the e-mail on file. If you ask us to delete your license, the plugin will stop activating.']],
    ];
    return pagina({
        titulo: 'Privacidade e cookies — InsideStudioSP',
        descricao: 'Como o InsideStudioSP trata os seus dados e cookies (LGPD).',
        corpo: `
<section class="hero"><div class="faixa">
  <p class="sobre">${t('PRIVACIDADE · LGPD', 'PRIVACY')}</p>
  <h1>${t('Privacidade e cookies', 'Privacy and cookies')}</h1>
  <p class="lead">${t('O mínimo de dados, só para vender, ativar o plugin e responder você.', 'As little data as possible, only to sell, activate the plugin and reply to you.')}</p>
</div></section>
<section class="faixa texto-legal">
  ${blocos.map(([hp, he, ...ps]) => `<h2>${t(hp, he)}</h2>\n  ${ps.map(([pp, pe]) => `<p>${t(pp, pe)}</p>`).join('\n  ')}`).join('\n  ')}
  <p class="dim">${t('Atualizada em 5 de novembro de 2026.', 'Last updated on November 5, 2026.')}</p>
</section>`,
    });
}

const paginaAviso = (titulo, pt, en) => pagina({ titulo: titulo + ' — InsideStudioSP', descricao: '',
    corpo: `<section class="hero"><div class="faixa"><h1>${titulo === 'Mensagem enviada' ? t('Mensagem enviada', 'Message sent') : t('Não deu para enviar', 'Could not send')}</h1>
<p class="lead">${t(pt, en)}</p><p><a class="bt bt-sec" href="${titulo === 'Mensagem enviada' ? '/' : '/contato/'}">${titulo === 'Mensagem enviada' ? t('Voltar para a loja', 'Back to the shop') : t('Voltar ao formulário', 'Back to the form')}</a></p></div></section>` });

rmSync(DIST, { recursive: true, force: true });
const grava = (rel, txt) => { const f = join(DIST, rel); mkdirSync(dirname(f), { recursive: true }); writeFileSync(f, txt); };
grava('index.html', loja());
for (const p of PACKS) grava(`${p.id}/index.html`, paginaPack(p));
for (const s of SECOES) grava(`plugins/${s.id}/index.html`, paginaSecao(s));
{ const sem = PACKS.filter((p) => !SECOES.some((s) => s.itens.includes(p.id))); if (sem.length) throw new Error('pack fora de seção: ' + sem.map((p) => p.id)); }
grava('estudio/index.html', estudio());
grava('contato/index.html', contato());
grava('carrinho/index.html', carrinho());
grava('privacidade/index.html', privacidade());
grava('contato/enviado/index.html', paginaAviso('Mensagem enviada', 'Recebemos sua mensagem. A resposta chega no e-mail que você informou.', 'We got your message. The reply will go to the e-mail you entered.'));
grava('contato/erro/index.html', paginaAviso('Erro', 'Confira o nome, o e-mail e a mensagem e tente de novo. Se continuar, escreva para contato@insidestudiosp.com.br.', 'Check your name, e-mail and message and try again. If it keeps failing, write to contato@insidestudiosp.com.br.'));
grava('contato/limite/index.html', paginaAviso('Erro', 'Muitas mensagens seguidas daqui. Tente de novo mais tarde ou escreva para contato@insidestudiosp.com.br.', 'Too many messages in a row. Try again later or write to contato@insidestudiosp.com.br.'));
grava('404.html', pagina({ titulo: 'InsideStudioSP', descricao: '', corpo: `<section class="hero"><div class="faixa"><h1>404</h1><p class="lead">${t('Página não encontrada.', 'Page not found.')} <a href="/">${t('Voltar para a loja', 'Back to the shop')}</a></p></div></section>` }));
mkdirSync(join(DIST, 'img'), { recursive: true });
for (const f of readdirSync(join(RAIZ, 'img'))) copyFileSync(join(RAIZ, 'img', f), join(DIST, 'img', f));
mkdirSync(join(DIST, 'manuais'), { recursive: true });
for (const f of readdirSync(join(RAIZ, 'manuais'))) copyFileSync(join(RAIZ, 'manuais', f), join(DIST, 'manuais', f));
{ const falta = Object.values(MANUAIS).flat().filter((f) => !readdirSync(join(RAIZ, 'manuais')).includes(`${f}-Manual.pdf`)); if (falta.length) throw new Error('manual faltando: ' + falta); }
for (const f of ['estilo.css', 'site.js']) copyFileSync(join(RAIZ, f), join(DIST, f));
grava('favicon.svg', '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64"><rect width="64" height="64" rx="14" fill="#0d0b14"/><path d="M10 34c6-16 10-16 14 0s8 16 14 0 10-16 16 0" fill="none" stroke="#a78bfa" stroke-width="6" stroke-linecap="round"/></svg>');
grava('robots.txt', PREVIA ? 'User-agent: *\nDisallow: /\n' : 'User-agent: *\nAllow: /\n');
// form-action: o Chrome confere também o REDIRECIONAMENTO do envio. O /loja/pagar responde 303 para o checkout do
// Mercado Pago, então o domínio dele tem que estar aqui (sem ele o botão de comprar não faz nada). PayPal: incluir
// https://www.paypal.com quando ele voltar. O domínio sem www cobre a volta da loja para a página vista pelo www.
grava('_headers', `/*
  X-Content-Type-Options: nosniff
  Referrer-Policy: strict-origin-when-cross-origin
  X-Frame-Options: DENY
  Content-Security-Policy: default-src 'self'; img-src 'self' data:; style-src 'self'; style-src-attr 'unsafe-inline'; script-src 'self' https://static.cloudflareinsights.com; frame-src https://open.spotify.com https://www.youtube-nocookie.com; connect-src 'self' https://cloudflareinsights.com; base-uri 'none'; form-action 'self' https://insidestudiosp.com.br https://www.mercadopago.com.br https://*.mercadopago.com.br; frame-ancestors 'none'
${PREVIA ? '  X-Robots-Tag: noindex, nofollow\n' : ''}/img/*
  Cache-Control: public, max-age=604800
/manuais/*
  ! Content-Security-Policy
  ! X-Frame-Options
  Cache-Control: public, max-age=86400
`);
console.log(`site gerado em ${DIST} (${PACKS.length} packs${PREVIA ? ', prévia noindex' : ''})`);
