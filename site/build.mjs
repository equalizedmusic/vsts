// Gera o site estático em site/dist: node site/build.mjs
//   /                 loja (VASSOURA e ALIGN em destaque + todos os packs)
//   /<pack>/          página do pack (plugins, galeria, compra)
//   /estudio/         o estúdio e o artista (fotos, vídeos, Spotify, YouTube)
//   /contato/         formulário de contato: POST /contato/enviar (servidor de licenças) ⇒ e-mail para contato@ ⇒ Caixa de entrada do painel
// Texto bilíngue no mesmo HTML: <span class="pt"> e <span class="en">; o site.js troca pelo atributo lang.

import { mkdirSync, rmSync, writeFileSync, copyFileSync, readdirSync } from 'node:fs';
import { join, dirname } from 'node:path';
import { fileURLToPath } from 'node:url';
import { SITE, PACKS, LIC } from './dados.mjs';

const RAIZ = dirname(fileURLToPath(import.meta.url));
const DIST = join(RAIZ, 'dist');
const PREVIA = process.env.PREVIA !== '0';   // prévia: noindex até virar o site oficial

const esc = (s) => String(s).replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;');
const t = (pt, en) => `<span class="pt">${esc(pt)}</span><span class="en" lang="en">${esc(en ?? pt)}</span>`;
const brl = (v) => v.toLocaleString('pt-BR', { style: 'currency', currency: 'BRL' });
const usd = (v) => 'US$ ' + v.toFixed(2);
const temBrl = (p) => typeof p.preco?.brl === 'number', temUsd = (p) => typeof p.preco?.usd === 'number';
// preço na etiqueta: reais em PT; em EN, dólares se houver (PayPal), senão reais
const valor = (p) => t(brl(p.preco.brl), temUsd(p) ? usd(p.preco.usd) : brl(p.preco.brl));

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
    <a href="${raiz}/#packs">${t('Loja', 'Shop')}</a>
    <a href="${raiz}/estudio/">${t('Estúdio', 'Studio')}</a>
    <a href="${raiz}/contato/">${t('Contato', 'Contact')}</a>
    <button class="idioma" type="button" aria-label="Idioma / Language"><span class="pt">EN</span><span class="en">PT</span></button>
  </nav>
</div></header>
${PREVIA ? `<div class="previa">${t('Prévia do site novo.', 'Preview of the new site.')}</div>` : ''}
<main>${corpo}</main>
<footer id="contato" class="rodape"><div class="faixa">
  <div>
    <div class="marca">Inside<b>Studio</b>SP</div>
    <p>${t('Plugins, produção e mix em São Paulo.', 'Plugins, production and mixing from São Paulo, Brazil.')}</p>
  </div>
  <div class="contatos">
    <a href="mailto:${SITE.email}">${SITE.email}</a>
    <a href="${SITE.instagram}" rel="noopener" target="_blank">Instagram @equalizedmusic</a>
  </div>
  <small>© ${new Date().getFullYear()} InsideStudioSP</small>
</div></footer>
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
    return `<form class="compra" method="post" action="${LIC}/loja/pagar">
    <div class="preco">${temBrl(p) ? valor(p) : usd(p.preco.usd)}</div>
    <input type="hidden" name="pacote" value="${p.id}">
    <label class="campo">${t('Seu e-mail (a licença chega nele)', 'Your e-mail (the license is sent to it)')}
      <input type="email" name="email" required maxlength="120" autocomplete="email" placeholder="voce@email.com"></label>
    ${temBrl(p) ? `<button class="bt bt-principal" name="metodo" value="mp">${t('Pix ou cartão em até 12x', 'Pix or card (Brazil)')}</button>` : ''}
    ${temUsd(p) ? `<button class="bt bt-sec" name="metodo" value="paypal">${t('PayPal (fora do Brasil)', 'PayPal (international)')}</button>` : ''}
    <p class="nota">${t('Pagamento aprovado: você recebe no e-mail o token de ativação, o link de download e as instruções. O token vale em 2 computadores.',
                        'Once the payment is approved you get the activation token, the download link and the instructions by e-mail. The token works on 2 computers.')}</p>
  </form>`;
}

function cartaoPack(p, grande = false) {
    const etiqueta = p.gratis ? `<span class="selo-gratis">${t('GRÁTIS', 'FREE')}</span>`
        : temBrl(p) || temUsd(p) ? `<span class="valor">${temBrl(p) ? valor(p) : usd(p.preco.usd)}</span>`
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

function loja() {
    const destaques = PACKS.filter((p) => p.destaque);
    const resto = PACKS.filter((p) => !p.destaque);
    return pagina({
        titulo: 'InsideStudioSP — Plugins',
        descricao: 'Plugins VST3/AU e Max for Live feitos em São Paulo: ALIGN, VASSOURA, MESA e mais.',
        corpo: `
<section class="hero"><div class="faixa">
  <p class="sobre">${t('PLUGINS · MAX FOR LIVE · ESTÚDIO', 'PLUGINS · MAX FOR LIVE · STUDIO')}</p>
  <h1>${t('Ferramentas feitas por quem mixa todo dia.', 'Tools made by someone who mixes every day.')}</h1>
  <p class="lead">${t('Plugins para Windows e Mac, pensados no estúdio para resolver problema de verdade, com a tela mostrando o que está acontecendo.',
                      'Plugins for Windows and Mac, built in the studio to solve real problems, with the screen showing what is going on.')}</p>
</div></section>
<section class="faixa"><h2 class="secao">${t('Destaques', 'Featured')}</h2>
  <div class="grade-destaque">${destaques.map((p) => cartaoPack(p, true)).join('')}</div>
</section>
<section id="packs" class="faixa"><h2 class="secao">${t('Todos os packs', 'All packs')}</h2>
  <div class="grade">${resto.map((p) => cartaoPack(p)).join('')}</div>
</section>
<section class="faixa como">
  <h2 class="secao">${t('Como funciona a compra', 'How buying works')}</h2>
  <ol>
    <li>${t('Escolha o pack e pague com Pix, cartão (até 12x) ou PayPal.', 'Pick a pack and pay with Pix, card or PayPal.')}</li>
    <li>${t('Pagamento aprovado, chega no seu e-mail o token, o link de download e as instruções.', 'Once approved, you get the token, the download link and the instructions by e-mail.')}</li>
    <li>${t('Instale, abra o plugin no DAW e ative com o e-mail e o token. Vale em 2 computadores.', 'Install, open the plugin in your DAW and activate it with your e-mail and token. Works on 2 computers.')}</li>
  </ol>
</section>
<section class="faixa chamada-estudio">
  <div><h2>${t('Conheça o estúdio', 'Meet the studio')}</h2>
  <p>${t('Produção, mix e master. Os plugins nascem aqui.', 'Production, mixing and mastering. The plugins are born here.')}</p></div>
  <a class="bt bt-sec" href="/estudio/">${t('Ver o estúdio', 'See the studio')}</a>
</section>`,
    });
}

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
    <a class="voltar" href="/#packs">← ${t('Todos os packs', 'All packs')}</a>
    <h1>${esc(p.nome)}</h1>
    <p class="lead">${t(...p.frase)}</p>
    <p>${t(...p.texto)}</p>
    <p class="tipo">${t(...p.tipo)} · Windows · macOS</p>
    ${p.plugins.length === 1 && p.plugins[0][2] ? `<p class="dim">${t(p.plugins[0][2], p.plugins[0][3])}</p>` : ''}
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
    const fotos = [1, 2, 3, 4, 5, 6].map((n) => `<div class="vaga foto">${t(`Foto ${n}`, `Photo ${n}`)}</div>`).join('');
    return pagina({
        titulo: 'Estúdio — InsideStudioSP', raiz: '',
        descricao: 'InsideStudioSP: estúdio de produção, mix e master em São Paulo.',
        corpo: `
<section class="hero"><div class="faixa">
  <p class="sobre">${t('ESTÚDIO · SÃO PAULO', 'STUDIO · SÃO PAULO, BRAZIL')}</p>
  <h1>InsideStudioSP</h1>
  <p class="lead">${t('[Texto sobre o estúdio: o que faz, para quem, o equipamento, o jeito de trabalhar.]',
                      '[About the studio: what it does, who it is for, the gear, the way of working.]')}</p>
</div></section>
<section class="faixa"><h2 class="secao">${t('Fotos', 'Photos')}</h2><div class="grade-fotos">${fotos}</div></section>
<section class="faixa artista">
  <div><h2 class="secao">${t('O artista', 'The artist')}</h2>
  <p class="lead">${t('[Bio do artista: trajetória, gênero, lançamentos, selos, shows.]', '[Artist bio: background, genre, releases, labels, gigs.]')}</p>
  <a class="bt bt-sec" href="${SITE.instagram}" rel="noopener" target="_blank">Instagram @equalizedmusic</a></div>
  <div>${spotify}</div>
</section>
<section class="faixa"><h2 class="secao">${t('Vídeos', 'Videos')}</h2><div class="video">${youtube}</div></section>`,
    });
}

function contato() {
    return pagina({
        titulo: 'Contato — InsideStudioSP',
        descricao: 'Fale com o InsideStudioSP: plugins, produção, mix e master.',
        corpo: `
<section class="hero"><div class="faixa">
  <p class="sobre">${t('CONTATO', 'CONTACT')}</p>
  <h1>${t('Fale com a gente', 'Get in touch')}</h1>
  <p class="lead">${t('Dúvida sobre um plugin, licença, produção ou mix: escreva aqui que a resposta chega no seu e-mail.',
                      'Questions about a plugin, a license, production or mixing: write here and the answer goes to your e-mail.')}</p>
</div></section>
<section class="faixa"><form class="compra form-contato" method="post" action="/contato/enviar">
  <label class="campo">${t('Seu nome', 'Your name')}<input name="nome" required maxlength="80" autocomplete="name"></label>
  <label class="campo">${t('Seu e-mail', 'Your e-mail')}<input type="email" name="email" required maxlength="120" autocomplete="email" placeholder="voce@email.com"></label>
  <label class="campo">${t('Assunto', 'Subject')}<select name="assunto">
    <option value="Plugins">${esc('Plugins / licença · Plugins / license')}</option>
    <option value="Estúdio">${esc('Estúdio: produção, mix, master · Studio')}</option>
    <option value="Outro">${esc('Outro · Other')}</option></select></label>
  <label class="campo">${t('Mensagem', 'Message')}<textarea name="mensagem" required minlength="5" maxlength="5000" rows="7"></textarea></label>
  <label class="isca" aria-hidden="true">Site<input name="site" tabindex="-1" autocomplete="off"></label>
  <button class="bt bt-principal">${t('Enviar mensagem', 'Send message')}</button>
  <p class="nota">${t('Ou escreva direto para', 'Or write straight to')} <a href="mailto:${SITE.email}">${SITE.email}</a></p>
</form></section>`,
    });
}

const paginaAviso = (titulo, pt, en) => pagina({ titulo: titulo + ' — InsideStudioSP', descricao: '',
    corpo: `<section class="hero"><div class="faixa"><h1>${titulo === 'Mensagem enviada' ? t('Mensagem enviada', 'Message sent') : t('Não deu para enviar', 'Could not send')}</h1>
<p class="lead">${t(pt, en)}</p><p><a class="bt bt-sec" href="${titulo === 'Mensagem enviada' ? '/' : '/contato/'}">${titulo === 'Mensagem enviada' ? t('Voltar para a loja', 'Back to the shop') : t('Voltar ao formulário', 'Back to the form')}</a></p></div></section>` });

rmSync(DIST, { recursive: true, force: true });
const grava = (rel, txt) => { const f = join(DIST, rel); mkdirSync(dirname(f), { recursive: true }); writeFileSync(f, txt); };
grava('index.html', loja());
for (const p of PACKS) grava(`${p.id}/index.html`, paginaPack(p));
grava('estudio/index.html', estudio());
grava('contato/index.html', contato());
grava('contato/enviado/index.html', paginaAviso('Mensagem enviada', 'Recebemos sua mensagem. A resposta chega no e-mail que você informou.', 'We got your message. The reply will go to the e-mail you entered.'));
grava('contato/erro/index.html', paginaAviso('Erro', 'Confira o nome, o e-mail e a mensagem e tente de novo. Se continuar, escreva para contato@insidestudiosp.com.br.', 'Check your name, e-mail and message and try again. If it keeps failing, write to contato@insidestudiosp.com.br.'));
grava('contato/limite/index.html', paginaAviso('Erro', 'Muitas mensagens seguidas daqui. Tente de novo mais tarde ou escreva para contato@insidestudiosp.com.br.', 'Too many messages in a row. Try again later or write to contato@insidestudiosp.com.br.'));
grava('404.html', pagina({ titulo: 'InsideStudioSP', descricao: '', corpo: `<section class="hero"><div class="faixa"><h1>404</h1><p class="lead">${t('Página não encontrada.', 'Page not found.')} <a href="/">${t('Voltar para a loja', 'Back to the shop')}</a></p></div></section>` }));
mkdirSync(join(DIST, 'img'), { recursive: true });
for (const f of readdirSync(join(RAIZ, 'img'))) copyFileSync(join(RAIZ, 'img', f), join(DIST, 'img', f));
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
  Content-Security-Policy: default-src 'self'; img-src 'self' data:; style-src 'self'; script-src 'self'; frame-src https://open.spotify.com https://www.youtube-nocookie.com; connect-src 'self'; base-uri 'none'; form-action 'self' https://insidestudiosp.com.br https://www.mercadopago.com.br https://*.mercadopago.com.br; frame-ancestors 'none'
${PREVIA ? '  X-Robots-Tag: noindex, nofollow\n' : ''}/img/*
  Cache-Control: public, max-age=604800
`);
console.log(`site gerado em ${DIST} (${PACKS.length} packs${PREVIA ? ', prévia noindex' : ''})`);
