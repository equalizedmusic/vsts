// Serviço de ativação dos plugins InsideStudioSP (Cloudflare Workers + D1).
//
// Rotas do plugin (sem senha; quem prova a compra é o par e-mail + token):
//   POST /v1/ativar     { pacote, email, token, maquina, nome_maquina, versao }
//   POST /v1/validar    { pacote, email, token, maquina, versao }      (o plugin chama de tempos em tempos)
//   POST /v1/desativar  { pacote, email, token, maquina }
//   GET  /v1/versao?pacote=mix&versao=1.0.0&sistema=win|mac   (o plugin já ativado pergunta ao abrir)
//        -> { ok: true, atualizacao: null | { versao, url, notas } }   url = instalador do sistema (ou a página do release)
//   GET  /baixar/<pacote>/<win|mac>   redireciona para o instalador (links da mensagem do painel)
//   GET/POST /gratis/montamonta   página grátis: e-mail -> token + download do MONTA-MONTA (ver src/gratis.js)
//   GET/POST /gratis/vassoura (= /vassoura)   só e-mail -> cadastro sem token + e-mail com o link /baixar/vassoura/zip
//   GET/POST /gratis/align (= /align)   e-mail + Windows/Mac -> token + download do ALIGN, 1 computador
//   POST /gratis/align/zip   { email, token?, sistema }   zip com o instalador + LEIA-ME com e-mail, token e instruções
//   POST /gratis/align/reenviar, GET/POST /gratis/align/novo-token   "perdi meu token" por e-mail (só com RESEND_API_KEY)
//   POST /contato/enviar  formulário de contato do site ⇒ e-mail para contato@ (ver src/contato.js)
//   POST /email/entrada   webhook do Resend: e-mail que chegou no contato@ (ver src/caixa.js)
//   POST /loja/pagar, POST /loja/mp/webhook, GET /loja/retorno   venda pelo Mercado Pago e PayPal (ver src/loja.js)
// Resposta: { ok: true, licenca: { dados, assinatura } }  ou  { ok: false, erro, mensagem }
//   dados = base64 do JSON { v, pacote, email, maquina, emitida, valida_ate }
//   assinatura = base64 da assinatura Ed25519 dos bytes de dados (chave pública embutida no plugin)
//
// Rotas de administração (cabeçalho Authorization: Bearer <ADMIN_KEY>): ver admin/isp-licenca.mjs
//   POST /admin/licencas          { email, pacote, origem?, pedido?, nota?, max_maquinas?, avisar?, sistemas? }  -> devolve o token (só aqui)
//                                 avisar: manda ao cliente o e-mail daquele pacote (src/avisos.js) -> email_enviado
//   GET  /admin/licencas?email=   lista com as máquinas
//   POST /admin/revogar           { id }
//   POST /admin/reativar          { id }
//   POST /admin/liberar-maquina   { id, maquina }
//   POST /admin/novo-token        { id }   gera outro token para a mesma licença (o antigo para de valer)
//   POST /admin/revogar-cliente   { email }   cancela todas as licenças do e-mail
//   POST /admin/excluir-cliente   { email }   apaga licenças e computadores do e-mail (só com tudo cancelado)
//
//   POST /admin/painel-senha      { usuario, senha }   (só com a ADMIN_KEY) cria/troca o login do painel
//
// Painel web (/painel, usuário e senha): ver src/painel.js. Um token por e-mail: a 2ª licença do mesmo e-mail usa
// o mesmo token, e "novo token" troca o do e-mail inteiro.
//
// Segredos (wrangler secret put): SIGN_KEY (Ed25519 privada, PKCS8 em base64), ADMIN_KEY, GITHUB_TOKEN (opcional, só leitura),
// RESEND_API_KEY e RESEND_WEBHOOK_SECRET (opcionais: e-mail e caixa de entrada; ver src/email.js e src/caixa.js),
// MP_ACCESS_TOKEN, MP_WEBHOOK_SECRET, PAYPAL_CLIENT_ID e PAYPAL_SECRET (opcionais: loja; ver src/loja.js).

import { rotaPainel, sessaoPainel, gravarSenhaPainel } from './painel.js';
import { rotaGratis, rotaZipVst, rotaTokenVst, PACOTES_GRATIS, VST_GRATIS } from './gratis.js';
import { rotaWebhookEntrada } from './caixa.js';
import { rotaLoja, vencerPedidos } from './loja.js';
import { avisarLicenca } from './avisos.js';
import { rotaContato } from './contato.js';

const PACOTES = ['criativo', 'mix', 'knob', 'quimera', 'limao', 'mercurio', 'resenha', 'montamonta', 'groovador', 'mesa', 'align', 'barebass', 'stompbox', 'maestro', 'vassoura'];
const VALIDADE_DIAS = 45;          // sem internet, o plugin segue liberado até aqui
const TENTATIVAS_POR_IP = 20;      // recusas em 15 min antes de bloquear o IP
const JANELA_TENTATIVAS = 15 * 60;
// versão mais nova de cada pacote = o release <pacote>-vX.Y.Z mais alto do GitHub (repo precisa estar público,
// ou o segredo GITHUB_TOKEN com leitura do repo). Se o GitHub não responder, ninguém recebe aviso e nada quebra.
const RELEASES_URL = 'https://api.github.com/repos/equalizedmusic/vsts/releases?per_page=100';
const CACHE_RELEASES = 30 * 60;    // segundos

const MENSAGENS = {
    pedido_invalido: 'Pedido inválido.',
    pacote_invalido: 'Pacote desconhecido.',
    nao_encontrada: 'E-mail ou token não conferem. Confira os dois como vieram no e-mail da compra.',
    pacote_nao_incluido: 'Este e-mail não tem licença deste pacote. Os pacotes ativados com este token são os da sua compra.',
    revogada: 'Esta licença foi cancelada. Fale com o suporte da InsideStudioSP.',
    limite_maquinas: 'Esta licença já está ativa no número máximo de computadores. Desative em um deles (menu da licença no plugin) e tente de novo.',
    maquina_nao_ativada: 'Este computador não está ativado nesta licença. Ative de novo com o e-mail e o token.',
    muitas_tentativas: 'Muitas tentativas seguidas. Espere 15 minutos e tente de novo.',
    nao_autorizado: 'Não autorizado.',
    ja_existe: 'Já existe licença deste pacote para este e-mail.',
    erro_interno: 'Erro no servidor de licenças. Tente de novo em alguns minutos.',
};

const agora = () => Math.floor(Date.now() / 1000);

function resposta(obj, status = 200) {
    return new Response(JSON.stringify(obj), {
        status,
        headers: { 'content-type': 'application/json; charset=utf-8', 'cache-control': 'no-store' },
    });
}
const falha = (erro, status = 400) => resposta({ ok: false, erro, mensagem: MENSAGENS[erro] || erro }, status);

const normEmail = (s) => String(s || '').trim().toLowerCase();
// token: ignora espaços, hífens, caixa e o prefixo ISP (o cliente pode colar "isp-abcd-..." de qualquer jeito)
const normToken = (s) => String(s || '').toUpperCase().replace(/[^0-9A-Z]/g, '').replace(/^ISP/, '');

async function sha256Hex(texto) {
    const h = await crypto.subtle.digest('SHA-256', new TextEncoder().encode(texto));
    return [...new Uint8Array(h)].map((b) => b.toString(16).padStart(2, '0')).join('');
}

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

// token no formato ISP-XXXX-XXXX-XXXX-XXXX-XXXX (alfabeto sem 0/O/1/I/L/U para não confundir ao digitar)
const ALFABETO = '23456789ABCDEFGHJKMNPQRSTVWXYZ';
function novoToken() {
    const bytes = crypto.getRandomValues(new Uint8Array(20));
    let s = '';
    for (const b of bytes) s += ALFABETO[b % ALFABETO.length];   // 20 × log2(30) ≈ 98 bits
    return 'ISP-' + s.match(/.{4}/g).join('-');
}

let chaveCache = null;
async function chaveAssinatura(env) {
    if (!chaveCache)
        chaveCache = await crypto.subtle.importKey('pkcs8', deB64(env.SIGN_KEY), { name: 'Ed25519' }, false, ['sign']);
    return chaveCache;
}

async function emitir(env, lic, maquina) {
    const t = agora();
    const dados = new TextEncoder().encode(JSON.stringify({
        v: 1, pacote: lic.pacote, email: lic.email, maquina, emitida: t, valida_ate: t + VALIDADE_DIAS * 86400,
    }));
    const assinatura = new Uint8Array(await crypto.subtle.sign({ name: 'Ed25519' }, await chaveAssinatura(env), dados));
    return { dados: b64(dados), assinatura: b64(assinatura) };
}

async function evento(env, licencaId, tipo, detalhe, ip) {
    await env.DB.prepare('INSERT INTO eventos (licenca_id, tipo, detalhe, ip, quando) VALUES (?, ?, ?, ?, ?)')
        .bind(licencaId ?? null, tipo, detalhe ?? null, ip ?? null, agora()).run();
}

async function ipBloqueado(env, ip) {
    const r = await env.DB.prepare("SELECT COUNT(*) AS n FROM eventos WHERE ip = ? AND tipo = 'recusou' AND quando > ?")
        .bind(ip, agora() - JANELA_TENTATIVAS).first();
    return (r?.n || 0) >= TENTATIVAS_POR_IP;
}

// ---------------- rotas do plugin ----------------

async function rotaPlugin(acao, req, env) {
    const ip = req.headers.get('cf-connecting-ip') || '';
    let corpo;
    try { corpo = await req.json(); } catch { return falha('pedido_invalido'); }

    const pacote = String(corpo.pacote || '');
    const email = normEmail(corpo.email);
    const token = normToken(corpo.token);
    const maquina = String(corpo.maquina || '');
    if (!PACOTES.includes(pacote)) return falha('pacote_invalido');
    if (!email || !token || !/^[0-9a-f]{32,128}$/.test(maquina)) return falha('pedido_invalido');

    if (await ipBloqueado(env, ip)) return falha('muitas_tentativas', 429);

    const recusa = async (erro, licId, status = 403) => {
        await evento(env, licId, 'recusou', `${acao}: ${erro} (${email}, ${pacote})`, ip);
        return falha(erro, status);
    };

    const hash = await sha256Hex('ISP' + token);   // o token guardado já normalizado (sem "-")
    const lic = await env.DB.prepare('SELECT * FROM licencas WHERE token_hash = ? AND email = ? AND pacote = ?')
        .bind(hash, email, pacote).first();
    if (!lic) {
        // e-mail e token certos, mas sem licença deste pacote: diz isso em vez de "não conferem"
        const outro = await env.DB.prepare('SELECT id FROM licencas WHERE token_hash = ? AND email = ? LIMIT 1').bind(hash, email).first();
        return recusa(outro ? 'pacote_nao_incluido' : 'nao_encontrada', outro?.id ?? null);
    }
    if (lic.status !== 'ativa') return recusa('revogada', lic.id);

    const atual = await env.DB.prepare('SELECT * FROM ativacoes WHERE licenca_id = ? AND maquina = ?')
        .bind(lic.id, maquina).first();

    if (acao === 'desativar') {
        if (atual) await env.DB.prepare('DELETE FROM ativacoes WHERE id = ?').bind(atual.id).run();
        await evento(env, lic.id, 'desativou', atual?.nome_maquina || maquina, ip);
        return resposta({ ok: true });
    }

    if (acao === 'validar') {
        if (!atual) return recusa('maquina_nao_ativada', lic.id);
        await env.DB.prepare('UPDATE ativacoes SET visto_em = ? WHERE id = ?').bind(agora(), atual.id).run();
        await evento(env, lic.id, 'validou', `${atual.nome_maquina || ''} ${corpo.versao || ''}`.trim(), ip);
        return resposta({ ok: true, licenca: await emitir(env, lic, maquina) });
    }

    // ativar
    const nome = String(corpo.nome_maquina || '').slice(0, 80);
    if (atual) {
        await env.DB.prepare('UPDATE ativacoes SET visto_em = ?, nome_maquina = ? WHERE id = ?').bind(agora(), nome, atual.id).run();
    } else {
        const n = await env.DB.prepare('SELECT COUNT(*) AS n FROM ativacoes WHERE licenca_id = ?').bind(lic.id).first();
        if ((n?.n || 0) >= lic.max_maquinas) {
            await evento(env, lic.id, 'limite', nome, ip);
            return falha('limite_maquinas', 403);
        }
        await env.DB.prepare('INSERT INTO ativacoes (licenca_id, maquina, nome_maquina, criada_em, visto_em) VALUES (?, ?, ?, ?, ?)')
            .bind(lic.id, maquina, nome, agora(), agora()).run();
    }
    await evento(env, lic.id, 'ativou', `${nome} ${corpo.versao || ''}`.trim(), ip);
    return resposta({ ok: true, licenca: await emitir(env, lic, maquina) });
}

// ---------------- aviso de versão nova ----------------

const partesVersao = (v) => (String(v || '').match(/(\d+)\.(\d+)\.(\d+)/) || []).slice(1).map(Number);
function compararVersao(a, b) {
    const x = partesVersao(a), y = partesVersao(b);
    if (x.length !== 3 || y.length !== 3) return 0;
    for (let i = 0; i < 3; i++) if (x[i] !== y[i]) return x[i] < y[i] ? -1 : 1;
    return 0;
}

// lista guardada na memória do Worker (o Cache API não funciona no workers.dev): 30 min, ou 5 min se o GitHub falhou
let releasesMem = null;   // { ate, lista }
async function releases(env, forcar = false) {
    if (releasesMem && agora() < releasesMem.ate && !(forcar && agora() - releasesMem.de > 60)) return releasesMem.lista;
    const h = { 'user-agent': 'isp-licencas', accept: 'application/vnd.github+json' };
    if (env.GITHUB_TOKEN) h.authorization = 'Bearer ' + env.GITHUB_TOKEN;
    let lista = null;
    try {
        const g = await fetch(RELEASES_URL, { headers: h });
        if (g.ok) lista = (await g.json()).filter((x) => !x.draft).map((x) => ({
            tag: x.tag_name, pre: !!x.prerelease, pagina: x.html_url, notas: String(x.body || '').slice(0, 800),
            arquivos: (x.assets || []).map((a) => a.browser_download_url),
            api: (x.assets || []).map((a) => a.url),
            tamanhos: (x.assets || []).map((a) => a.size),
            digests: (x.assets || []).map((a) => String(a.digest || '').replace(/^sha256:/, '')),
        }));
        else console.error('github releases', g.status);
    } catch (e) { console.error('github releases', e); }
    releasesMem = { de: agora(), ate: agora() + (lista ? CACHE_RELEASES : 300), lista: lista || releasesMem?.lista || [] };
    return releasesMem.lista;
}

async function rotaVersao(url, env) {
    const pacote = String(url.searchParams.get('pacote') || '');
    const versao = String(url.searchParams.get('versao') || '');
    const sistema = url.searchParams.get('sistema') === 'mac' ? 'mac' : 'win';
    if (!PACOTES.includes(pacote)) return falha('pacote_invalido');
    if (partesVersao(versao).length !== 3) return falha('pedido_invalido');

    const maior = releaseDoPacote(await releases(env), pacote);
    if (!maior || compararVersao(maior.versao, versao) <= 0) return resposta({ ok: true, atualizacao: null });
    const ext = sistema === 'mac' ? '.dmg' : '.exe';
    const instalador = maior.arquivos.find((a) => a.toLowerCase().endsWith(ext));
    // repo privado: o link do GitHub dá 404 para o cliente ⇒ sempre o /baixar do domínio (ele pede o link assinado com o token)
    const link = instalador ? `https://insidestudiosp.com.br/baixar/${pacote}/${sistema}` : 'https://insidestudiosp.com.br/';
    return resposta({ ok: true, atualizacao: { versao: maior.versao, url: link, notas: maior.notas } });
}

function releaseDoPacote(lista, pacote) {
    let maior = null;
    for (const rel of lista) {
        const m = !rel.pre && rel.tag.match(new RegExp(`^${pacote}-v(\\d+\\.\\d+\\.\\d+)$`));
        if (m && (!maior || compararVersao(m[1], maior.versao) > 0)) maior = { ...rel, versao: m[1] };
    }
    return maior;
}

// ---------------- download dos instaladores ----------------
// GET /baixar/<pacote>/<win|mac> -> redireciona para o instalador. Link aberto: sem licença o plugin não toca.
// Enquanto existir o pré-release "licenca-teste" (instaladores com ativação), ele vale para todos os pacotes; depois
// do merge, apagar o pré-release e o link passa a dar o release <pacote>-vX.Y.Z mais novo.
// a QUIMERA sai como "Quimera" (até o build de teste da ativação o pacote se chamava "Instrumentos")
const INSTALADOR = { criativo: ['Criativo'], mix: ['Mix'], knob: ['Knob'], quimera: ['Quimera', 'Instrumentos'], limao: ['Limao'], mercurio: ['Mercurio'], resenha: ['Resenha'], montamonta: ['MontaMonta'], groovador: ['Groovador'], mesa: ['Mesa'], align: ['Align'], barebass: ['Barebass'], stompbox: ['Stompbox'], maestro: ['Maestro'], vassoura: ['Vassoura'] };

function paginaSimples(titulo, texto, status) {
    const esc = (s) => s.replace(/[&<>]/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;' }[c]));
    return new Response(`<!doctype html><meta charset="utf-8"><meta name="viewport" content="width=device-width">
<title>InsideStudioSP</title><body style="font:16px system-ui;background:#16161a;color:#eee;padding:40px;max-width:560px;margin:auto">
<h2>${esc(titulo)}</h2><p>${esc(texto)}</p></body>`,
        { status, headers: { 'content-type': 'text/html; charset=utf-8', 'cache-control': 'no-store' } });
}

const nomeArquivo = (url) => decodeURIComponent(url.split('/').pop());

function acharInstalador(lista, pacote, sistema) {
    const fim = sistema === 'mac' ? '-mac.dmg' : '-win.exe';
    const rel = (pacote !== 'vassoura' && lista.find((r) => r.tag === 'licenca-teste')) || releaseDoPacote(lista, pacote);
    const i = rel ? rel.arquivos.findIndex((a) => {
        const nome = nomeArquivo(a);
        // VASSOURA (Max for Live): um zip só, igual no Windows e no Mac
        if (pacote === 'vassoura') return /^Vassoura-.*\.zip$/.test(nome);
        return INSTALADOR[pacote].some((n) => nome.startsWith(`InsideStudioSP-${n}-`)) && nome.endsWith(fim);
    }) : -1;
    return { rel, i };
}

// link para baixar o arquivo i do release; repo privado: o GitHub devolve um link temporário assinado
async function linkArquivo(env, rel, i) {
    if (!env.GITHUB_TOKEN) return rel.arquivos[i];
    return (await fetch(rel.api[i], {
        redirect: 'manual',
        headers: { 'user-agent': 'isp-licencas', accept: 'application/octet-stream', authorization: 'Bearer ' + env.GITHUB_TOKEN },
    })).headers.get('location');
}

// CRC dos instaladores publicados antes do .crc32 no release (chave = sha256 do GitHub)
const CRC_CONHECIDOS = {
    '7b5dac65240f6fb952779f73ca8b532d45da7856ad765c192b8d3a3ec6a50d3e': 0xde16e9a7,   // InsideStudioSP-Align-win.exe 0.1.0
    '765881b271527dba562c89129a52a3e0456edd766a6e0dfc55ca6af5c37d8363': 0xb276600d,   // InsideStudioSP-Align-mac.dmg 0.1.0
};
// instalador mais novo do pacote: { nome, tamanho, sha256, versao, link, crc } (crc = null se o release não tem o .crc32
// e o arquivo não está em CRC_CONHECIDOS) ou null. Usado pelo zip da página grátis (src/gratis.js).
async function instalador(env, pacote, sistema) {
    let { rel, i } = acharInstalador(await releases(env), pacote, sistema);
    if (i < 0) ({ rel, i } = acharInstalador(await releases(env, true), pacote, sistema));
    if (i < 0) return null;
    const nome = nomeArquivo(rel.arquivos[i]);
    let link = await linkArquivo(env, rel, i);
    if (!link) {   // o release da lista guardada foi apagado
        ({ rel, i } = acharInstalador(await releases(env, true), pacote, sistema));
        if (i < 0 || !(link = await linkArquivo(env, rel, i))) return null;
    }
    let crc = CRC_CONHECIDOS[rel.digests?.[i]] ?? null;
    const j = rel.arquivos.findIndex((a) => nomeArquivo(a) === nome + '.crc32');
    if (j >= 0) {
        try {
            const r = await fetch(await linkArquivo(env, rel, j), { headers: { 'user-agent': 'isp-licencas' } });
            const m = r.ok && (await r.text()).match(/^\s*([0-9a-f]{8})\b/i);
            if (m) crc = parseInt(m[1], 16);
        } catch (e) { console.error('crc32', e); }
    }
    return { nome, tamanho: rel.tamanhos?.[i], sha256: rel.digests?.[i] || '', versao: (rel.tag.match(/-v(\d+\.\d+\.\d+)$/) || [])[1] || '',
             link, crc };
}

async function rotaBaixar(caminho, env) {
    const [, , pacote, sistema] = caminho.split('/');
    // VASSOURA: /baixar/vassoura sozinho (link antigo do site, posts, painel) abre a página de cadastro;
    // o arquivo sai por /baixar/vassoura/zip (win/mac continuam valendo para os links já enviados)
    if (pacote === 'vassoura' && !sistema) return Response.redirect(VASSOURA_OFICIAL, 302);
    const unico = pacote === 'vassoura' && sistema === 'zip';
    if (!PACOTES.includes(pacote) || (!unico && !['win', 'mac'].includes(sistema)))
        return paginaSimples('Link inválido', 'Confira o link de download que veio no e-mail da licença.', 404);
    const indisponivel = () => paginaSimples('Download indisponível',
        'Este instalador não está disponível agora. Tente de novo mais tarde ou responda o e-mail da licença.', 404);
    let { rel, i } = acharInstalador(await releases(env), pacote, sistema);
    // a lista guardada pode ser de antes do release novo (ou de um release ainda subindo os arquivos)
    if (i < 0) ({ rel, i } = acharInstalador(await releases(env, true), pacote, sistema));
    if (i < 0) return indisponivel();
    // repo privado: link temporário assinado; o download não passa pelo Worker
    let destino = await linkArquivo(env, rel, i);
    // sem link: o release da lista guardada foi apagado (ex.: o pré-release licenca-teste) ⇒ lista nova e outra tentativa
    if (!destino) {
        ({ rel, i } = acharInstalador(await releases(env, true), pacote, sistema));
        if (i < 0) return indisponivel();
        destino = await linkArquivo(env, rel, i);
    }
    return Response.redirect(destino || rel.arquivos[i], 302);
}

// ---------------- administração ----------------

function iguais(a, b) {
    if (a.length !== b.length) return false;
    let d = 0;
    for (let i = 0; i < a.length; i++) d |= a.charCodeAt(i) ^ b.charCodeAt(i);
    return d === 0;
}

// cria a licença de um pacote para o e-mail (painel e loja). Devolve { id, email, pacote, token, mesmo_token,
// token_trocado } ou { erro }. Um token por e-mail: se o e-mail já tem licença de outro pacote, a nova usa o mesmo
// token (que não está guardado, então volta token null e o cliente segue com o que já recebeu)
async function criarLicenca(env, { email: emailBruto, pacote: pacoteBruto, origem, pedido, nota, max_maquinas }) {
    const email = normEmail(emailBruto);
    const pacote = String(pacoteBruto || '');
    if (!PACOTES.includes(pacote)) return { erro: 'pacote_invalido' };
    if (!/^[^@\s]+@[^@\s]+\.[^@\s]+$/.test(email)) return { erro: 'pedido_invalido' };
    const existe = await env.DB.prepare('SELECT id FROM licencas WHERE email = ? AND pacote = ?').bind(email, pacote).first();
    if (existe) return { erro: 'ja_existe', id: existe.id };
    // cadastro da página da VASSOURA não tem token (token_hash vazio): não serve de "mesmo token"
    let doEmail = await env.DB.prepare("SELECT token_hash FROM licencas WHERE email = ? AND token_hash <> '' LIMIT 1").bind(email).first();
    // e-mail que só tem licença da página grátis: aquele token saiu na tela sem conferir o e-mail (qualquer um pode
    // ter cadastrado), então licença dada por aqui troca o token do e-mail inteiro e o novo vai para o cliente
    const soGratis = doEmail && !(await env.DB.prepare("SELECT id FROM licencas WHERE email = ? AND origem <> 'gratis' LIMIT 1").bind(email).first());
    if (soGratis) doEmail = null;
    const token = doEmail ? null : novoToken();
    if (soGratis)
        await env.DB.prepare('UPDATE licencas SET token_hash = ? WHERE email = ?').bind(await sha256Hex('ISP' + normToken(token)), email).run();
    const r = await env.DB.prepare(
        'INSERT INTO licencas (email, pacote, token_hash, max_maquinas, origem, pedido, nota, criada_em) VALUES (?, ?, ?, ?, ?, ?, ?, ?)')
        .bind(email, pacote, doEmail ? doEmail.token_hash : await sha256Hex('ISP' + normToken(token)),
            Number(max_maquinas) || 2, origem === 'venda' ? 'venda' : 'cortesia',
            pedido || null, nota || null, agora())
        .run();
    const id = r.meta.last_row_id;
    await evento(env, id, 'criou', origem || 'cortesia', null);
    if (soGratis) await evento(env, id, 'novo_token', 'tinha só a licença grátis', null);
    return { id, email, pacote, token, mesmo_token: !!doEmail, token_trocado: !!soGratis };
}

async function rotaAdmin(caminho, req, env, url) {
    const auth = req.headers.get('authorization') || '';
    const porChave = !!env.ADMIN_KEY && iguais(auth, 'Bearer ' + env.ADMIN_KEY);
    // o painel entra com a sessão (cookie) + cabeçalho x-painel (outro site não consegue mandar esse cabeçalho)
    const porPainel = !porChave && req.headers.get('x-painel') === '1' && !!(await sessaoPainel(req, env));
    if (!porChave && !porPainel) return falha('nao_autorizado', 401);
    if (caminho === '/admin/painel-senha' && !porChave) return falha('nao_autorizado', 401);

    if (caminho === '/admin/licencas' && req.method === 'GET') {
        const email = normEmail(url.searchParams.get('email'));
        const lics = email
            ? await env.DB.prepare('SELECT * FROM licencas WHERE email = ? ORDER BY id').bind(email).all()
            : await env.DB.prepare('SELECT * FROM licencas ORDER BY id DESC LIMIT 5000').all();
        const ativ = await env.DB.prepare('SELECT * FROM ativacoes ORDER BY id').all();
        const porLic = {};
        for (const a of ativ.results) (porLic[a.licenca_id] ||= []).push(a);
        return resposta({
            ok: true,
            licencas: lics.results.map(({ token_hash, ...l }) => ({ ...l, maquinas: porLic[l.id] || [] })),
        });
    }

    let corpo;
    try { corpo = await req.json(); } catch { return falha('pedido_invalido'); }

    if (caminho === '/admin/painel-senha') {   // só com a ADMIN_KEY: cria/troca o usuário do painel
        const usuario = String(corpo.usuario || '').trim().toLowerCase();
        if (!/^[a-z0-9._-]{3,40}$/.test(usuario) || String(corpo.senha || '').length < 10) return falha('pedido_invalido');
        await gravarSenhaPainel(env, usuario, String(corpo.senha));
        return resposta({ ok: true, usuario });
    }

    if (caminho === '/admin/licencas' && req.method === 'POST') {
        const r = await criarLicenca(env, corpo);
        if (r.erro) return falha(r.erro, r.erro === 'ja_existe' ? 409 : 400);
        // avisar: um e-mail por pacote criado, com o passo a passo daquele plugin (src/avisos.js)
        let email_enviado = null;
        if (corpo.avisar) {
            email_enviado = !!(await avisarLicenca(env, url, { instalador }, r, { sistemas: corpo.sistemas, maquinas: corpo.max_maquinas }));
            await evento(env, r.id, email_enviado ? 'email' : 'email_falhou', 'painel', null);
        }
        return resposta({ ok: true, ...r, email_enviado });
    }

    // ações sobre o cliente inteiro (todas as licenças do e-mail)
    if (caminho === '/admin/revogar-cliente' || caminho === '/admin/excluir-cliente') {
        const email = normEmail(corpo.email);
        const r = await env.DB.prepare("SELECT COUNT(*) AS n, SUM(status = 'ativa') AS ativas FROM licencas WHERE email = ?")
            .bind(email).first();
        if (!email || !r?.n) return falha('nao_encontrada', 404);
        if (caminho === '/admin/revogar-cliente') {
            await env.DB.prepare("UPDATE licencas SET status = 'revogada', revogada_em = ? WHERE email = ? AND status = 'ativa'")
                .bind(agora(), email).run();
            await evento(env, null, 'revogou_cliente', email, null);
            return resposta({ ok: true, revogadas: r.ativas || 0 });
        }
        // excluir só depois de cancelar tudo: assim um clique errado não apaga cliente com licença valendo
        if (r.ativas) return falha('Cancele as licenças deste cliente antes de excluir.', 409);
        await env.DB.batch([
            env.DB.prepare('DELETE FROM ativacoes WHERE licenca_id IN (SELECT id FROM licencas WHERE email = ?)').bind(email),
            env.DB.prepare('DELETE FROM licencas WHERE email = ?').bind(email),
        ]);
        await evento(env, null, 'excluiu_cliente', email, null);   // o histórico (eventos) fica
        return resposta({ ok: true, excluidas: r.n });
    }

    const id = Number(corpo.id);
    const lic = id ? await env.DB.prepare('SELECT * FROM licencas WHERE id = ?').bind(id).first() : null;
    if (!lic) return falha('nao_encontrada', 404);

    if (caminho === '/admin/revogar' || caminho === '/admin/reativar') {
        const rev = caminho === '/admin/revogar';
        await env.DB.prepare('UPDATE licencas SET status = ?, revogada_em = ? WHERE id = ?')
            .bind(rev ? 'revogada' : 'ativa', rev ? agora() : null, id).run();
        await evento(env, id, rev ? 'revogou' : 'reativou', corpo.nota ?? null, null);
        return resposta({ ok: true });
    }
    if (caminho === '/admin/liberar-maquina') {
        await env.DB.prepare('DELETE FROM ativacoes WHERE licenca_id = ? AND (maquina = ? OR id = ?)')
            .bind(id, String(corpo.maquina || ''), Number(corpo.maquina) || -1).run();
        await evento(env, id, 'liberou_maquina', String(corpo.maquina || ''), null);
        return resposta({ ok: true });
    }
    if (caminho === '/admin/novo-token') {
        // troca o token do e-mail inteiro (todos os pacotes dele)
        const token = novoToken();
        await env.DB.prepare('UPDATE licencas SET token_hash = ? WHERE email = ?')
            .bind(await sha256Hex('ISP' + normToken(token)), lic.email).run();
        await evento(env, id, 'novo_token', lic.email, null);
        return resposta({ ok: true, id, email: lic.email, pacote: lic.pacote, token });
    }
    return falha('pedido_invalido', 404);
}

// a página do ALIGN mora no domínio do estúdio: as rotas de wrangler.toml entregam /align*,
// /gratis/align/* e /baixar/* (downloads de todos os pacotes) do insidestudiosp.com.br para este Worker
// (o resto do domínio é o site)
const ALIGN_OFICIAL = 'https://insidestudiosp.com.br/align/';
// a da VASSOURA também (rota /vassoura*): só e-mail, sem token
const VASSOURA_OFICIAL = 'https://insidestudiosp.com.br/vassoura/';
// e a do BAREBASS (rotas /barebass* e /gratis/barebass/*): igual à do ALIGN
const BAREBASS_OFICIAL = 'https://insidestudiosp.com.br/barebass/';
// o painel também (rotas /painel* e /admin/*). A sessão é um cookie do host, então fica num endereço só (sem www).
// A linha de comando (admin/isp-licenca.mjs) segue usando /admin/* no workers.dev com a ADMIN_KEY.
const PAINEL_OFICIAL = 'https://insidestudiosp.com.br/painel';

export default {
    async fetch(req, env) {
        const url = new URL(req.url);
        const caminho = url.pathname.replace(/\/+$/, '');
        try {
            // link antigo da página do ALIGN (workers.dev) ⇒ endereço oficial no domínio do estúdio.
            // Só GET/HEAD: os POST da página (cadastro, reenvio) continuam funcionando em qualquer host.
            if ((req.method === 'GET' || req.method === 'HEAD') && url.hostname.endsWith('.workers.dev')
                && (caminho === '/align' || caminho === '/gratis/align'))
                return Response.redirect(ALIGN_OFICIAL, 302);
            if ((req.method === 'GET' || req.method === 'HEAD') && url.hostname.endsWith('.workers.dev')
                && (caminho === '/vassoura' || caminho === '/gratis/vassoura'))
                return Response.redirect(VASSOURA_OFICIAL, 302);
            if ((req.method === 'GET' || req.method === 'HEAD') && url.hostname.endsWith('.workers.dev')
                && (caminho === '/barebass' || caminho === '/gratis/barebass'))
                return Response.redirect(BAREBASS_OFICIAL, 302);
            // painel aberto pelo endereço antigo (workers.dev) ou pelo www ⇒ endereço oficial. Só a página:
            // as chamadas de uma aba já aberta continuam respondendo onde estão.
            if ((req.method === 'GET' || req.method === 'HEAD') && caminho === '/painel'
                && (url.hostname.endsWith('.workers.dev') || url.hostname === 'www.insidestudiosp.com.br'))
                return Response.redirect(PAINEL_OFICIAL, 302);
            if (req.method === 'POST' && caminho === '/v1/ativar') return await rotaPlugin('ativar', req, env);
            if (req.method === 'POST' && caminho === '/v1/validar') return await rotaPlugin('validar', req, env);
            if (req.method === 'POST' && caminho === '/v1/desativar') return await rotaPlugin('desativar', req, env);
            if (req.method === 'GET' && caminho === '/v1/versao') return await rotaVersao(url, env);
            // HEAD também: prévia de link (WhatsApp, Instagram) consulta antes de baixar
            if ((req.method === 'GET' || req.method === 'HEAD') && caminho.startsWith('/baixar/')) return await rotaBaixar(caminho.replace(/\/+$/, ''), env);
            if (caminho.startsWith('/admin/')) return await rotaAdmin(caminho, req, env, url);
            if (caminho === '/contato/enviar') return await rotaContato(req, env, url);
            if (caminho === '/email/entrada') return await rotaWebhookEntrada(req, env);
            if (caminho.startsWith('/loja/'))
                return await rotaLoja(caminho, req, env, url, { normEmail, agora, evento, criarLicenca, paginaSimples });
            // VSTs grátis com token (ALIGN, BAREBASS): zip com LEIA-ME e "perdi meu token"
            const vst = caminho.match(/^\/gratis\/([a-z]+)\/(zip|reenviar|novo-token)$/);
            if (vst && VST_GRATIS[vst[1]]) {
                if (vst[2] === 'zip')
                    return await rotaZipVst(req, env, { normEmail, normToken, sha256Hex, evento, instalador, paginaSimples }, vst[1]);
                return await rotaTokenVst(req, env, url, { normEmail, normToken, novoToken, sha256Hex, agora, evento, paginaSimples, instalador },
                    vst[2], vst[1]);
            }
            const curto = caminho.slice(1);   // /align, /barebass, /vassoura = /gratis/<pacote>
            if (caminho === '/gratis' || (curto !== 'montamonta' && PACOTES_GRATIS.includes(curto)) || (caminho.startsWith('/gratis/') && PACOTES_GRATIS.includes(caminho.slice(8))))
                return await rotaGratis(req, env, url, { normEmail, normToken, novoToken, sha256Hex, agora, evento, instalador },
                    PACOTES_GRATIS.includes(curto) ? curto : caminho.slice(8) || 'montamonta');
            if (caminho === '/painel' || caminho.startsWith('/painel/')) return await rotaPainel(caminho, req, env);
            // navegador em endereço desconhecido (/Painel, /painel. colado com pontuação, raiz) ⇒ manda p/ o painel
            if (req.method === 'GET' && (req.headers.get('accept') || '').includes('text/html'))
                return Response.redirect(url.origin + '/painel', 302);
            if (caminho === '' || caminho === '/v1') return resposta({ ok: true, servico: 'licencas InsideStudioSP' });
            return falha('pedido_invalido', 404);
        } catch (e) {
            console.error(e);
            return falha('erro_interno', 500);
        }
    },
    // cron diário (wrangler.toml [triggers]): vence os pedidos da loja abertos há mais de 4 dias
    async scheduled(evento, env, ctx) {
        ctx.waitUntil(vencerPedidos(env, agora()));
    },
};
