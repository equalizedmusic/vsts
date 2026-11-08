// Robô do Instagram do estúdio: quem comenta a palavra-chave (ex.: "align") num post/Reels recebe o link por DM.
// App da Meta "Instagram API (Unified Login)" — conta pessoal do Rogério, nada da Intermed.
//
//   GET  /instagram/webhook   verificação da Meta (hub.mode=subscribe, hub.verify_token = IG_VERIFY) -> hub.challenge
//   POST /instagram/webhook   aviso de comentário (campo "comments"); assinatura X-Hub-Signature-256 conferida
//                             com IG_APP_SECRET quando ele existir
//   GET  /instagram/diag      (Authorization: Bearer <ADMIN_KEY>) testa o token e mostra contas/páginas e os últimos envios
//
//   POST /instagram/inscrever (admin) inscreve o app nos comentários da conta (subscribed_apps)
//   POST /instagram/renovar   (admin) renova o token do login do Instagram na hora
//
// Dois tipos de token, detectados pelo prefixo:
//   EAA...  token do Facebook (usuário do sistema / página) -> graph.facebook.com; precisa da conta no portfólio
//   IGAA... token do "login do Instagram" (Add account na tela do app) -> graph.instagram.com; sem portfólio,
//           vale 60 dias e o cron diário renova (renovarTokenInstagram), guardando o novo em ig_config.
// Segredos: IG_TOKEN (token da Meta, EAA... ou IGAA...; semente), IG_VERIFY (texto que vai no campo "Verify token" da Meta),
// IG_APP_SECRET (opcional, "Chave secreta do app" em Configurações → Básico).
// Tabela ig_respostas (migracao-2026-11-08-instagram.sql): um comentário é respondido uma vez só
// (a Meta reenvia o aviso quando a resposta demora ou falha).

import { moldura, esc, comAssinatura } from './emailhtml.js';

const GRAPH_FB = 'https://graph.facebook.com/v23.0';
const GRAPH_IG = 'https://graph.instagram.com/v23.0';
const ehIg = (t) => typeof t === 'string' && t.startsWith('IG');

async function hash(t) {
    const d = new Uint8Array(await crypto.subtle.digest('SHA-256', new TextEncoder().encode(t || '')));
    return [...d].slice(0, 12).map((b) => b.toString(16).padStart(2, '0')).join('');
}

// Token em uso: o renovado (ig_config) se ele nasceu da semente atual; senão, o segredo IG_TOKEN.
async function tokenAtual(env) {
    const semente = env.IG_TOKEN || '';
    if (!ehIg(semente)) return semente;
    try {
        const r = await env.DB.prepare("SELECT valor FROM ig_config WHERE chave = 'token'").first();
        if (r) {
            const v = JSON.parse(r.valor);
            if (v.semente === (await hash(semente)) && v.token) return v.token;
        }
    } catch (e) { console.error('ig_config', e); }
    return semente;
}

// Cron diário: renova o token IGAA (a Meta só aceita com mais de 24h de vida; falha aqui não derruba nada).
export async function renovarTokenInstagram(env) {
    const atual = await tokenAtual(env);
    if (!ehIg(atual)) return { ok: false, motivo: 'token_nao_e_do_instagram' };
    const r = await fetch('https://graph.instagram.com/refresh_access_token?grant_type=ig_refresh_token&access_token=' + encodeURIComponent(atual));
    let c; try { c = await r.json(); } catch { c = null; }
    if (!r.ok || !c || !c.access_token) return { ok: false, status: r.status, erro: c?.error?.message };
    const valor = JSON.stringify({ token: c.access_token, semente: await hash(env.IG_TOKEN), expira_s: c.expires_in });
    await env.DB.prepare('INSERT INTO ig_config (chave, valor, quando) VALUES (?, ?, ?) ON CONFLICT(chave) DO UPDATE SET valor = excluded.valor, quando = excluded.quando')
        .bind('token', valor, new Date().toISOString()).run();
    return { ok: true, expira_s: c.expires_in };
}

// palavra-chave -> o que mandar. A palavra precisa aparecer sozinha no comentário ("ALIGN", "align!", "quero o align").
// curto: palavra comum do português ("mesa") só dispara em comentário de até 4 palavras ("mesa", "quero a mesa!").
const PUBLICO = ['Te mandei no direct! 🎧', 'Chegou no seu direct 👊', 'Mandei o link no direct! 🔥'];
const GATILHOS = [
    {
        palavra: 'align',
        dm: 'Opa! Aqui está o ALIGN, grátis 🎧\n\nBaixe aqui 👉 https://insidestudiosp.com.br/align\n\nÉ só colocar o seu e-mail na página que o token chega na hora.',
        publico: PUBLICO,
    },
    {
        palavra: 'barebass',
        dm: 'Opa! Aqui está o BAREBASS, o synth de baixo grátis 🎸\n\nBaixe aqui 👉 https://insidestudiosp.com.br/barebass\n\nÉ só colocar o seu e-mail na página que o token chega na hora.',
        publico: PUBLICO,
    },
    {
        palavra: 'vassoura',
        dm: 'Opa! Aqui está a VASSOURA, grátis 🧹\n\nLimpa os clips vazios do Ableton Live (12.2 ou mais novo).\n\nBaixe aqui 👉 https://insidestudiosp.com.br/vassoura',
        publico: PUBLICO,
    },
    {
        palavra: 'mesa', curto: true,
        dm: 'Opa! Aqui está a MESA, a console analógica dentro da sua DAW 🎚️\n\nVeja os detalhes e o preço de lançamento 👉 https://insidestudiosp.com.br/mesa/\n\nPix, cartão em até 12x ou boleto.',
        publico: PUBLICO,
    },
];

// Moderação: comentário de terceiro com cara de golpe/spam é OCULTADO (não apagado; dá para reexibir no app).
// Lista conservadora de propósito: na dúvida, não oculta.
const SPAM = [
    /https?:\/\//i, /\bwww\./i, /\bbit\.ly\b/i, /\bwa\.me\b/i, /\bt\.me\//i,
    /\bpromot(e|ion|ing)\b.*@/i, /\b(send|dm|message)\b.*\b(to|us)\b.*@/i, /\bcollab\b.*\b(dm|@)/i,
    /\b(ganh[ae]r?|compr[ae]r?|gr[aá]tis|free)\b.*\bseguidores\b/i, /\bseguidores\b.*\b(gr[aá]tis|reais|barato)\b/i,
    /\brenda extra\b/i, /\b(bitcoin|cripto|forex)\b/i, /\binvest\w*\b.*\b(garantid|lucro|retorno|profit)/i,
    /\bonlyfans\b/i, /\bprivacy\.com/i, /\b(contact|chama|fala)\b.*\btelegram\b/i, /\bsugar ?(daddy|baby)\b/i,
];
const ehSpam = (texto) => SPAM.some((r) => r.test(String(texto || '')));

function json(obj, status = 200) {
    return new Response(JSON.stringify(obj, null, 2), {
        status, headers: { 'content-type': 'application/json; charset=utf-8', 'cache-control': 'no-store' },
    });
}

function iguais(a, b) {
    if (typeof a !== 'string' || typeof b !== 'string' || a.length !== b.length) return false;
    let d = 0;
    for (let i = 0; i < a.length; i++) d |= a.charCodeAt(i) ^ b.charCodeAt(i);
    return d === 0;
}

async function assinaturaOk(env, corpo, cab) {
    if (!env.IG_APP_SECRET) return true;   // sem a chave do app ainda: aceita (o pior caso é responder um comentário real)
    if (!cab || !cab.startsWith('sha256=')) return false;
    const chave = await crypto.subtle.importKey('raw', new TextEncoder().encode(env.IG_APP_SECRET),
        { name: 'HMAC', hash: 'SHA-256' }, false, ['sign']);
    const sig = new Uint8Array(await crypto.subtle.sign('HMAC', chave, new TextEncoder().encode(corpo)));
    const hex = [...sig].map((b) => b.toString(16).padStart(2, '0')).join('');
    return iguais(cab.slice(7), hex);
}

function gatilhoDe(texto) {
    const t = String(texto || '').toLowerCase();
    const palavras = t.split(/\s+/).filter((p) => /[a-z0-9à-ú]/.test(p)).length;
    return GATILHOS.find((g) => new RegExp(`(^|[^a-z0-9])${g.palavra}([^a-z0-9]|$)`).test(t) && (!g.curto || palavras <= 4));
}
export const _teste = { gatilhoDe, ehSpam };

async function graph(caminho, token, opcoes = {}) {
    const sep = caminho.includes('?') ? '&' : '?';
    const r = await fetch((ehIg(token) ? GRAPH_IG : GRAPH_FB) + caminho + sep + 'access_token=' + encodeURIComponent(token), opcoes);
    let corpo; try { corpo = await r.json(); } catch { corpo = null; }
    return { ok: r.ok, status: r.status, corpo };
}

// Página do Facebook ligada ao Instagram (a DM privada sai pela página). Guardada na memória do isolate.
let paginaCache = null;
async function paginaDoInstagram(env, igId) {
    if (paginaCache && paginaCache.ig === igId) return paginaCache;
    const r = await graph('/me/accounts?fields=id,name,access_token,instagram_business_account{id,username}', env.IG_TOKEN);
    const lista = (r.ok && r.corpo && r.corpo.data) || [];
    const p = lista.find((x) => x.instagram_business_account && x.instagram_business_account.id === igId) || null;
    paginaCache = p ? { ig: igId, id: p.id, token: p.access_token } : null;
    return paginaCache;
}

// Resposta privada (DM) a um comentário: tenta pela página ligada; senão, direto pela conta do Instagram.
async function mandarDm(env, igId, comentarioId, texto) {
    const corpo = JSON.stringify({ recipient: { comment_id: comentarioId }, message: { text: texto } });
    const opc = { method: 'POST', headers: { 'content-type': 'application/json' }, body: corpo };
    const tentativas = [];
    const tk = await tokenAtual(env);
    if (ehIg(tk)) {   // login do Instagram: a DM sai direto pela conta, sem página
        const r = await graph('/me/messages', tk, opc);
        tentativas.push({ via: 'instagram_login', status: r.status, erro: r.ok ? null : r.corpo?.error?.message });
        return { ok: r.ok, tentativas };
    }
    const pag = await paginaDoInstagram(env, igId).catch(() => null);
    if (pag) {
        const r = await graph(`/${pag.id}/messages`, pag.token, opc);
        tentativas.push({ via: 'pagina', status: r.status, erro: r.ok ? null : r.corpo?.error?.message });
        if (r.ok) return { ok: true, tentativas };
    }
    const r2 = await graph(`/${igId}/messages`, env.IG_TOKEN, opc);
    tentativas.push({ via: 'instagram', status: r2.status, erro: r2.ok ? null : r2.corpo?.error?.message });
    return { ok: r2.ok, tentativas };
}

async function responderPublico(env, comentarioId, opcoes) {
    const msg = opcoes[Math.floor(Math.random() * opcoes.length)];
    const r = await graph(`/${comentarioId}/replies`, await tokenAtual(env), {
        method: 'POST', headers: { 'content-type': 'application/x-www-form-urlencoded' },
        body: 'message=' + encodeURIComponent(msg),
    });
    return { ok: r.ok, status: r.status, erro: r.ok ? null : r.corpo?.error?.message };
}

// Oculta o comentário (POST /{comentario}?hide=true) e registra em ig_respostas com palavra '[oculto]'.
async function ocultar(env, igId, v) {
    const ins = await env.DB.prepare(
        'INSERT OR IGNORE INTO ig_respostas (comentario_id, ig_id, midia_id, usuario, palavra, texto, quando) VALUES (?, ?, ?, ?, ?, ?, ?)')
        .bind(v.id, igId, v.media?.id ?? null, v.from?.username ?? null, '[oculto]',
            String(v.text || '').slice(0, 300), new Date().toISOString()).run();
    if (!ins.meta || ins.meta.changes !== 1) return;
    const r = await graph(`/${v.id}`, await tokenAtual(env), {
        method: 'POST', headers: { 'content-type': 'application/x-www-form-urlencoded' }, body: 'hide=true',
    });
    await env.DB.prepare('UPDATE ig_respostas SET dm_ok = ?, detalhe = ? WHERE comentario_id = ?')
        .bind(r.ok ? 1 : 0, JSON.stringify({ ocultar: { status: r.status, erro: r.ok ? null : r.corpo?.error?.message } }), v.id).run();
}

async function tratarComentario(env, igId, v) {
    const comentarioId = v && v.id;
    if (!comentarioId) return;
    if (v.from && v.from.id === igId) return;                 // comentário da própria conta (inclusive a resposta pública)
    if (ehSpam(v.text)) { await ocultar(env, igId, v); return; }   // vale também para resposta dentro de comentário
    if (v.parent_id) return;                                  // resposta dentro de um comentário: não dispara
    const g = gatilhoDe(v.text);
    if (!g) return;
    // reserva o comentário antes de responder: aviso repetido da Meta não manda a DM duas vezes
    const ins = await env.DB.prepare(
        'INSERT OR IGNORE INTO ig_respostas (comentario_id, ig_id, midia_id, usuario, palavra, texto, quando) VALUES (?, ?, ?, ?, ?, ?, ?)')
        .bind(comentarioId, igId, v.media?.id ?? null, v.from?.username ?? null, g.palavra,
            String(v.text || '').slice(0, 300), new Date().toISOString()).run();
    if (!ins.meta || ins.meta.changes !== 1) return;
    const dm = await mandarDm(env, igId, comentarioId, g.dm);
    const pub = dm.ok ? await responderPublico(env, comentarioId, g.publico) : null;
    await env.DB.prepare('UPDATE ig_respostas SET dm_ok = ?, publico_ok = ?, detalhe = ? WHERE comentario_id = ?')
        .bind(dm.ok ? 1 : 0, pub && pub.ok ? 1 : 0, JSON.stringify({ dm: dm.tentativas, publico: pub }).slice(0, 1500), comentarioId).run();
}

async function diag(env) {
    const out = { tem_token: !!env.IG_TOKEN, tem_verify: !!env.IG_VERIFY, tem_app_secret: !!env.IG_APP_SECRET };
    const tk = await tokenAtual(env);
    out.tipo_token = !tk ? null : ehIg(tk) ? 'instagram_login' : 'facebook';
    out.token_renovado = !!tk && tk !== env.IG_TOKEN;
    if (tk && ehIg(tk)) {
        const me = await graph('/me?fields=user_id,username,account_type', tk);
        out.me = me.ok ? me.corpo : { status: me.status, erro: me.corpo?.error };
        const sub = await graph('/me/subscribed_apps', tk);
        out.inscricao = sub.ok ? sub.corpo : { status: sub.status, erro: sub.corpo?.error };
    } else if (env.IG_TOKEN) {
        const me = await graph('/me?fields=id,name', env.IG_TOKEN);
        out.me = me.ok ? me.corpo : { status: me.status, erro: me.corpo?.error };
        const contas = await graph('/me/accounts?fields=id,name,instagram_business_account{id,username}', env.IG_TOKEN);
        out.paginas = contas.ok ? contas.corpo.data : { status: contas.status, erro: contas.corpo?.error };
        const perm = await graph('/me/permissions', env.IG_TOKEN);
        out.permissoes = perm.ok ? perm.corpo.data.filter((p) => p.status === 'granted').map((p) => p.permission) : { status: perm.status };
    }
    if (env.IG_APP_SECRET) {   // confere a chave do app com o token de app (id|secret); mostra o webhook assinado
        const appTk = '1453925772739380|' + env.IG_APP_SECRET;
        const app = await graph('/1453925772739380?fields=name', appTk);
        out.app_secret_valido = app.ok;
        if (!app.ok) out.app_secret_erro = app.corpo?.error?.message || app.status;
        const sub = await graph('/1453925772739380/subscriptions', appTk);
        out.webhooks = sub.ok ? (sub.corpo.data || []).map((d) => ({ objeto: d.object, ativo: d.active, url: d.callback_url, campos: (d.fields || []).map((f) => f.name) }))
            : { status: sub.status, erro: sub.corpo?.error?.message };
    }
    try {
        const r = await env.DB.prepare('SELECT * FROM ig_respostas ORDER BY quando DESC LIMIT 20').all();
        out.ultimos = r.results;
    } catch (e) { out.ultimos = 'tabela ig_respostas: ' + e.message; }
    try {
        const r = await env.DB.prepare('SELECT quando, corpo FROM ig_eventos ORDER BY id DESC LIMIT 10').all();
        out.eventos = r.results;
    } catch (e) { out.eventos = 'tabela ig_eventos: ' + e.message; }
    return out;
}

// ---------- Páginas públicas exigidas pela Meta para o app ir para Live ----------
function pagina(titulo, corpoHtml) {
    const html = `<!doctype html><html lang="pt-BR"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>${titulo} · InsideStudioSP</title><style>
body{margin:0;background:#0f0d14;color:#e7e3ef;font:16px/1.6 system-ui,-apple-system,Segoe UI,Roboto,sans-serif}
main{max-width:720px;margin:0 auto;padding:40px 20px 60px}h1{font-size:26px;margin:0 0 6px}h2{font-size:18px;margin:28px 0 6px;color:#c4b5fd}
a{color:#a78bfa}.sub{color:#9a93a8;font-size:14px}li{margin:4px 0}</style></head><body><main>${corpoHtml}
<p class="sub" style="margin-top:40px"><a href="https://insidestudiosp.com.br/">insidestudiosp.com.br</a></p></main></body></html>`;
    return new Response(html, { headers: { 'content-type': 'text/html; charset=utf-8', 'cache-control': 'public, max-age=3600' } });
}

const PRIVACIDADE = `<h1>Política de privacidade</h1><p class="sub">App "InsideStudio DM" · Instagram @equalizedmusic · atualizada em 08/11/2026</p>
<p>O InsideStudio DM é a automação do Instagram do InsideStudioSP. Quando alguém comenta uma palavra-chave num post
(por exemplo "align"), ele manda por mensagem direta o link do plugin pedido e responde o comentário.</p>
<h2>Que dados recebemos</h2><ul>
<li>Do Instagram, via API oficial da Meta: o nome de usuário e o identificador de quem comentou, o identificador e o texto do comentário e o post em que ele foi feito.</li>
<li>Não recebemos senha, e-mail, telefone, lista de seguidores nem mensagens que não sejam respostas ao nosso envio.</li></ul>
<h2>Para que usamos</h2><ul>
<li>Mandar a mensagem direta com o link pedido, uma vez só por comentário.</li>
<li>Ocultar comentários de spam ou golpe nos nossos posts.</li>
<li>Publicar e agendar posts da nossa própria conta.</li></ul>
<p>Não vendemos, não alugamos e não compartilhamos esses dados com ninguém, e não os usamos para anúncios.</p>
<h2>Por quanto tempo guardamos</h2>
<p>O registro de cada comentário respondido ou ocultado é apagado automaticamente depois de 90 dias.</p>
<h2>Onde ficam</h2><p>Num banco de dados da Cloudflare usado só por este serviço.</p>
<h2>Seus direitos (LGPD)</h2>
<p>Você pode pedir acesso, correção ou exclusão dos seus dados a qualquer momento. Veja <a href="/instagram/exclusao-dados">como pedir a exclusão</a>.</p>
<h2>Contato</h2><p>InsideStudioSP · <a href="mailto:contato@insidestudiosp.com.br">contato@insidestudiosp.com.br</a> · Instagram <a href="https://instagram.com/equalizedmusic">@equalizedmusic</a></p>`;

const EXCLUSAO = `<h1>Exclusão de dados</h1><p class="sub">App "InsideStudio DM" · Instagram @equalizedmusic</p>
<p>Para apagar os dados que o InsideStudio DM guardou sobre você (nome de usuário e comentários que ele respondeu ou ocultou):</p>
<ol><li>Mande um e-mail para <a href="mailto:contato@insidestudiosp.com.br?subject=Excluir%20meus%20dados%20do%20Instagram">contato@insidestudiosp.com.br</a> com o assunto <b>"Excluir meus dados do Instagram"</b>, ou mande uma mensagem direta para <a href="https://instagram.com/equalizedmusic">@equalizedmusic</a>.</li>
<li>Informe o seu nome de usuário do Instagram.</li>
<li>Apagamos tudo em até 30 dias e confirmamos pelo mesmo canal.</li></ol>
<p>Mesmo sem pedido, esses registros são apagados automaticamente depois de 90 dias.</p>
<p>Você também pode tirar o acesso do app pelo Instagram: Configurações → Apps e sites.</p>`;

// ---------- Agenda de posts (foto, carrossel, Reels) ----------
// Tabela ig_agenda (migracao-2026-11-08d-instagram-agenda.sql). O cron de 10 em 10 min chama publicarAgendados:
//   pendente (hora chegou) -> cria o(s) contêiner(es) na Meta -> processando -> (vídeo termina) -> publicado | erro
// As mídias precisam estar num endereço https público (a Meta baixa de lá).
const TIPOS = { imagem: [1, 1], reels: [1, 1], carrossel: [2, 10] };
const ehVideo = (u) => /\.(mp4|mov)(\?|$)/i.test(u);
const igAlvo = (env, tk) => (ehIg(tk) ? '/me' : '/' + (env.IG_ID || '17841400749768561'));
const form = (o) => ({
    method: 'POST', headers: { 'content-type': 'application/x-www-form-urlencoded' },
    body: Object.entries(o).filter(([, v]) => v !== undefined && v !== null).map(([k, v]) => k + '=' + encodeURIComponent(v)).join('&'),
});

function validarAgenda(d) {
    if (!d || !TIPOS[d.tipo]) return 'tipo deve ser imagem, reels ou carrossel';
    const [min, max] = TIPOS[d.tipo];
    if (!Array.isArray(d.midias) || d.midias.length < min || d.midias.length > max) return `midias: de ${min} a ${max} endereços`;
    if (d.midias.some((u) => typeof u !== 'string' || !/^https:\/\/\S+$/.test(u))) return 'midias: só endereços https';
    if (d.tipo === 'reels' && !ehVideo(d.midias[0])) return 'reels: a mídia tem que ser .mp4 ou .mov';
    if (d.tipo === 'imagem' && ehVideo(d.midias[0])) return 'imagem: para vídeo use reels';
    if (d.legenda != null && (typeof d.legenda !== 'string' || d.legenda.length > 2200)) return 'legenda: texto de até 2200 caracteres';
    const q = Date.parse(d.quando || '');
    if (!Number.isFinite(q)) return 'quando: data/hora ISO com fuso (ex.: 2026-11-10T19:00:00-03:00)';
    return null;
}

async function passoAgenda(env, tk, a) {
    const alvo = igAlvo(env, tk);
    const midias = JSON.parse(a.midias);
    const c = a.conteiner ? JSON.parse(a.conteiner) : {};
    const erro = (r) => r.corpo?.error?.message || ('status ' + r.status);
    if (a.estado === 'pendente') {
        if (a.tipo === 'carrossel') {
            c.filhos = [];
            for (const u of midias) {
                const r = await graph(alvo + '/media', tk, form(ehVideo(u)
                    ? { media_type: 'VIDEO', video_url: u, is_carousel_item: 'true' }
                    : { image_url: u, is_carousel_item: 'true' }));
                if (!r.ok || !r.corpo?.id) return { estado: 'erro', resultado: 'item ' + u + ': ' + erro(r) };
                c.filhos.push(r.corpo.id);
            }
        } else {
            const r = await graph(alvo + '/media', tk, form(a.tipo === 'reels'
                ? { media_type: 'REELS', video_url: midias[0], caption: a.legenda, share_to_feed: 'true' }
                : { image_url: midias[0], caption: a.legenda }));
            if (!r.ok || !r.corpo?.id) return { estado: 'erro', resultado: erro(r) };
            c.principal = r.corpo.id;
        }
        return { estado: 'processando', conteiner: c };
    }
    // processando
    const status = async (id) => {
        const r = await graph(`/${id}?fields=status_code,status`, tk);
        return r.ok ? r.corpo.status_code : 'FALHA_LEITURA';
    };
    if (!c.principal) {   // carrossel: espera os itens e cria o contêiner do carrossel
        const st = await Promise.all(c.filhos.map(status));
        if (st.some((s) => s === 'ERROR' || s === 'EXPIRED')) return { estado: 'erro', resultado: 'item com erro: ' + st.join(',') };
        if (!st.every((s) => s === 'FINISHED')) return { estado: 'processando', conteiner: c };
        const r = await graph(alvo + '/media', tk, form({ media_type: 'CAROUSEL', children: c.filhos.join(','), caption: a.legenda }));
        if (!r.ok || !r.corpo?.id) return { estado: 'erro', resultado: erro(r) };
        c.principal = r.corpo.id;
    }
    const s = await status(c.principal);
    if (s === 'ERROR' || s === 'EXPIRED') return { estado: 'erro', resultado: 'contêiner ' + s };
    if (s !== 'FINISHED') return { estado: 'processando', conteiner: c };
    const p = await graph(alvo + '/media_publish', tk, form({ creation_id: c.principal }));
    if (!p.ok || !p.corpo?.id) return { estado: 'erro', resultado: erro(p) };
    const link = await graph(`/${p.corpo.id}?fields=permalink`, tk);
    return { estado: 'publicado', conteiner: c, resultado: JSON.stringify({ midia: p.corpo.id, link: link.corpo?.permalink || null }) };
}

export async function publicarAgendados(env) {
    const tk = await tokenAtual(env);
    if (!tk) return { ok: false, motivo: 'sem_token' };
    const agora = new Date().toISOString();
    const r = await env.DB.prepare(
        "SELECT * FROM ig_agenda WHERE (estado = 'pendente' AND quando <= ?) OR estado = 'processando' ORDER BY quando LIMIT 5").bind(agora).all();
    const feitos = [];
    for (const a of r.results || []) {
        let x;
        try { x = await passoAgenda(env, tk, a); } catch (e) { x = { estado: 'erro', resultado: String(e.message || e) }; }
        const tentativas = (a.tentativas || 0) + 1;
        if (x.estado === 'processando' && tentativas > 36) x = { ...x, estado: 'erro', resultado: 'vídeo não terminou de processar em 6h' };
        await env.DB.prepare('UPDATE ig_agenda SET estado = ?, conteiner = ?, resultado = ?, tentativas = ?, atualizado = ? WHERE id = ?')
            .bind(x.estado, x.conteiner ? JSON.stringify(x.conteiner) : a.conteiner, x.resultado ?? a.resultado, tentativas, agora, a.id).run();
        feitos.push({ id: a.id, estado: x.estado, resultado: x.resultado ?? null });
    }
    return { ok: true, feitos };
}

// ---------- Relatório semanal (sem instagram_manage_insights) ----------
// Toda segunda no cron diário (09:00 UTC = 06:00 de Brasília) manda para o Gmail do estúdio:
// seguidores (e a variação desde o último relatório), posts da semana com curtidas/comentários, o que mais ganhou
// interação na semana, pedidos por palavra-chave, spams ocultados e a agenda. Alcance/impressões/salvamentos
// precisam do insights e ficam de fora. A "foto" da semana anterior fica em ig_config (chave 'relatorio').
//   POST /instagram/relatorio          (admin) prévia: devolve o texto, não manda nem grava a foto
//   POST /instagram/relatorio?enviar=1 (admin) manda agora e grava a foto (vira a base do próximo)
const RELATORIO_PARA = 'insidestudiosp@gmail.com';
const F = "'Segoe UI', -apple-system, Helvetica, Arial, sans-serif";
const dataBr = (iso) => new Date(iso).toLocaleString('pt-BR', { timeZone: 'America/Sao_Paulo', day: '2-digit', month: '2-digit', hour: '2-digit', minute: '2-digit' });
const diaBr = (ms) => new Date(ms).toLocaleDateString('pt-BR', { timeZone: 'America/Sao_Paulo', day: '2-digit', month: '2-digit' });
const sinal = (n) => (n > 0 ? '+' + n : String(n));
const resumoLegenda = (t) => { const s = String(t || '').replace(/\s+/g, ' ').trim(); return s ? (s.length > 60 ? s.slice(0, 57) + '...' : s) : '(sem legenda)'; };

async function montarRelatorio(env) {
    const tk = await tokenAtual(env);
    if (!tk) return { ok: false, motivo: 'sem_token' };
    const alvo = igAlvo(env, tk);
    const conta = await graph(alvo + '?fields=username,followers_count,follows_count,media_count', tk);
    if (!conta.ok) return { ok: false, motivo: 'conta: ' + (conta.corpo?.error?.message || conta.status) };
    const posts = [];
    let pag = await graph(alvo + '/media?fields=id,caption,media_type,media_product_type,timestamp,like_count,comments_count,permalink&limit=50', tk);
    for (let i = 0; i < 4 && pag.ok; i++) {   // até 200 posts (o suficiente para comparar a semana)
        posts.push(...(pag.corpo?.data || []));
        const prox = pag.corpo?.paging?.next;
        if (!prox) break;
        const x = await fetch(prox); let c; try { c = await x.json(); } catch { c = null; }
        pag = { ok: x.ok, corpo: c };
    }
    const agora = Date.now(), inicio = agora - 7 * 86400000;
    let antes = null;
    try { const r = await env.DB.prepare("SELECT valor FROM ig_config WHERE chave = 'relatorio'").first(); if (r) antes = JSON.parse(r.valor); } catch { /* sem base */ }
    const c = conta.corpo;
    const L = [];
    const H = [];   // o mesmo conteúdo em HTML (cartões e tabelas; src/emailhtml.js põe a moldura e a assinatura)
    const sec = (t) => H.push(`<div style="font:700 13px ${F};letter-spacing:.8px;color:#6d28d9;text-transform:uppercase;margin:26px 0 10px;padding-bottom:6px;border-bottom:2px solid #e7e4f0">${esc(t)}</div>`);
    const vazio = (t) => H.push(`<div style="color:#6b6880;font-style:italic;font-size:14px">${esc(t)}</div>`);
    const lnk = (u, t) => `<a href="${esc(u)}" style="color:#1f1b2e;text-decoration:none">${esc(t)}</a>`;
    const cartao = (rot, val, extra, cor) => `<td width="25%" style="padding:4px;vertical-align:top"><div style="background:#f5f3ff;border-radius:10px;padding:12px 8px;text-align:center">` +
        `<div style="font:800 24px ${F};color:#1f1b2e">${esc(val)}</div>` +
        `<div style="font:12px ${F};color:#6b6880;margin-top:2px">${esc(rot)}</div>` +
        (extra ? `<div style="font:700 12px ${F};color:${cor || '#6b6880'};margin-top:4px">${esc(extra)}</div>` : '') + '</div></td>';
    const tabela = (cab, linhas) => `<table role="presentation" width="100%" cellpadding="0" cellspacing="0" style="border-collapse:collapse;font:14px/1.45 ${F}">` +
        `<tr>${cab.map((x, i) => `<td style="padding:6px 8px;background:#f5f3ff;color:#6b6880;font-size:12px;font-weight:700;${i ? 'text-align:right;white-space:nowrap' : ''}">${x}</td>`).join('')}</tr>` +
        linhas.map((l) => `<tr>${l.map((x, i) => `<td style="padding:8px;border-bottom:1px solid #eeecf4;vertical-align:top;${i ? 'text-align:right;white-space:nowrap' : ''}">${x}</td>`).join('')}</tr>`).join('') + '</table>';
    const tipoDe = (p) => p.media_product_type === 'REELS' ? 'Reels' : p.media_type === 'CAROUSEL_ALBUM' ? 'Carrossel' : p.media_type === 'VIDEO' ? 'Vídeo' : 'Foto';
    const selo = (t) => `<span style="display:inline-block;background:#ede9fe;color:#6d28d9;font-size:11px;font-weight:700;padding:1px 7px;border-radius:999px">${esc(t)}</span>`;

    L.push(`Relatório semanal do Instagram @${c.username}`);
    L.push(`Semana de ${diaBr(inicio)} a ${diaBr(agora)}`, '');
    L.push('SEGUIDORES');
    const segAntes = antes?.seguidores;
    const varSeg = segAntes != null ? c.followers_count - segAntes : null;
    L.push(`  ${c.followers_count} seguidores` + (varSeg != null ? ` (${sinal(varSeg)} desde ${diaBr(Date.parse(antes.quando))})` : ' (primeiro relatório: a variação aparece a partir do próximo)'));
    L.push(`  ${c.media_count} posts no perfil · seguindo ${c.follows_count}`, '');

    const daSemana = posts.filter((p) => Date.parse(p.timestamp) >= inicio)
        .sort((a, b) => (b.like_count + b.comments_count) - (a.like_count + a.comments_count));

    // robô lido antes para entrar nos cartões do topo
    const ini = new Date(inicio).toISOString();
    let pedidos = [], ocultos = 0, erroRobo = '';
    try {
        const r = await env.DB.prepare('SELECT palavra, COUNT(*) n, SUM(dm_ok) ok FROM ig_respostas WHERE quando >= ? GROUP BY palavra ORDER BY n DESC').bind(ini).all();
        const linhas = r.results || [];
        pedidos = linhas.filter((x) => x.palavra !== '[oculto]');
        ocultos = linhas.find((x) => x.palavra === '[oculto]')?.n || 0;
    } catch (e) { erroRobo = e.message; }
    const totPedidos = pedidos.reduce((s, x) => s + x.n, 0);

    H.push('<table role="presentation" width="100%" cellpadding="0" cellspacing="0" style="margin-top:14px"><tr>' +
        cartao('seguidores', c.followers_count.toLocaleString('pt-BR'), varSeg == null ? 'base: 1º relatório' : sinal(varSeg) + ' desde ' + diaBr(Date.parse(antes.quando)),
               varSeg > 0 ? '#15803d' : varSeg < 0 ? '#b91c1c' : '#6b6880') +
        cartao('posts na semana', daSemana.length, c.media_count + ' no perfil') +
        cartao('pedidos de plugin', totPedidos, 'pelo comentário') +
        cartao('spams ocultados', ocultos, '') + '</tr></table>');

    L.push(`POSTS DA SEMANA (${daSemana.length})`);
    sec(`Posts da semana (${daSemana.length})`);
    if (!daSemana.length) { L.push('  Nenhum post nesta semana.'); vazio('Nenhum post nesta semana.'); }
    const linhasPost = [];
    for (const p of daSemana) {
        const tipo = tipoDe(p);
        L.push(`  ${dataBr(p.timestamp)} · ${tipo} · ${p.like_count ?? '?'} curtidas · ${p.comments_count ?? 0} comentários`);
        L.push(`    ${resumoLegenda(p.caption)}`, `    ${p.permalink}`);
        linhasPost.push([`${selo(tipo)} <span style="color:#6b6880;font-size:12px">${esc(dataBr(p.timestamp))}</span><br>${lnk(p.permalink, resumoLegenda(p.caption))}`,
            '❤ ' + (p.like_count ?? '?'), '💬 ' + (p.comments_count ?? 0)]);
    }
    if (linhasPost.length) H.push(tabela(['Post', 'Curtidas', 'Coment.'], linhasPost));
    L.push('');

    // interação ganha na semana (todos os posts, comparando com a foto anterior)
    if (antes?.posts) {
        const ganho = posts.map((p) => {
            const a = antes.posts[p.id];
            const base = a ? a[0] + a[1] : (Date.parse(p.timestamp) >= Date.parse(antes.quando) ? 0 : null);
            return base == null ? null : { p, g: (p.like_count || 0) + (p.comments_count || 0) - base };
        }).filter((x) => x && x.g > 0).sort((a, b) => b.g - a.g).slice(0, 5);
        L.push(`O QUE MAIS GANHOU INTERAÇÃO DESDE ${diaBr(Date.parse(antes.quando))} (curtidas + comentários novos)`);
        sec(`O que mais ganhou interação desde ${diaBr(Date.parse(antes.quando))}`);
        if (!ganho.length) { L.push('  Nenhum post ganhou interação desde o último relatório.'); vazio('Nenhum post ganhou interação desde o último relatório.'); }
        for (const { p, g } of ganho) L.push(`  +${g} · ${resumoLegenda(p.caption)} (${diaBr(Date.parse(p.timestamp))})`, `    ${p.permalink}`);
        if (ganho.length) H.push(tabela(['Post (curtidas + comentários novos)', 'Ganho'],
            ganho.map(({ p, g }) => [`${lnk(p.permalink, resumoLegenda(p.caption))} <span style="color:#6b6880;font-size:12px">· ${esc(diaBr(Date.parse(p.timestamp)))}</span>`,
                `<b style="color:#15803d">+${g}</b>`])));
        L.push('');
    }

    L.push('ROBÔ DE COMENTÁRIOS');
    sec('Robô de comentários');
    if (erroRobo) { L.push('  (não consegui ler: ' + erroRobo + ')', ''); vazio('Não consegui ler: ' + erroRobo); }
    else {
        if (!pedidos.length) { L.push('  Ninguém pediu plugin pelo comentário nesta semana.'); vazio('Ninguém pediu plugin pelo comentário nesta semana.'); }
        for (const x of pedidos) L.push(`  ${x.palavra.toUpperCase()}: ${x.n} pedido(s), ${x.ok || 0} DM(s) entregue(s)` + (x.n - (x.ok || 0) > 0 ? ` · ${x.n - (x.ok || 0)} falhou(aram)` : ''));
        if (pedidos.length) H.push(tabela(['Palavra', 'Pedidos', 'DM entregue', 'Falhou'], pedidos.map((x) => {
            const f = x.n - (x.ok || 0);
            return [selo(x.palavra.toUpperCase()), x.n, x.ok || 0, f > 0 ? `<b style="color:#b91c1c">${f}</b>` : '0'];
        })));
        L.push(`  Spam ocultado: ${ocultos}`, '');
        H.push(`<div style="font-size:14px;color:#6b6880;margin-top:8px">Spam ocultado na semana: <b style="color:#1f1b2e">${ocultos}</b></div>`);
    }

    try {
        const fim = new Date(agora + 7 * 86400000).toISOString();
        const r = await env.DB.prepare("SELECT tipo, quando, estado, legenda, resultado FROM ig_agenda WHERE (quando >= ? AND quando <= ?) OR estado IN ('pendente','processando','erro') AND quando >= ? ORDER BY quando").bind(ini, fim, ini).all();
        const ag = r.results || [];
        if (ag.length) {
            L.push('AGENDA');
            sec('Agenda');
            const nome = { pendente: 'agendado', processando: 'processando', publicado: 'publicado', erro: 'ERRO', cancelado: 'cancelado' };
            for (const a of ag) L.push(`  ${dataBr(a.quando)} · ${a.tipo} · ${nome[a.estado] || a.estado} · ${resumoLegenda(a.legenda)}` + (a.estado === 'erro' ? ` (${String(a.resultado || '').slice(0, 120)})` : ''));
            H.push(tabela(['Quando · post', 'Situação'], ag.map((a) => [`<span style="color:#6b6880;font-size:12px">${esc(dataBr(a.quando))} · ${esc(a.tipo)}</span><br>${esc(resumoLegenda(a.legenda))}` +
                (a.estado === 'erro' ? `<br><span style="color:#b91c1c;font-size:12px">${esc(String(a.resultado || '').slice(0, 120))}</span>` : ''),
                a.estado === 'erro' ? '<b style="color:#b91c1c">ERRO</b>' : esc(nome[a.estado] || a.estado)])));
            L.push('');
        }
    } catch { /* sem agenda */ }

    L.push('Alcance, impressões e salvamentos ficam de fora: precisam da permissão instagram_manage_insights.');
    H.push('<div style="margin-top:24px;font-size:12px;color:#6b6880">Alcance, impressões e salvamentos ficam de fora: precisam da permissão instagram_manage_insights.</div>');
    const foto = { quando: new Date(agora).toISOString(), seguidores: c.followers_count, posts: Object.fromEntries(posts.map((p) => [p.id, [p.like_count || 0, p.comments_count || 0]])) };
    const html = moldura(`Instagram @${c.username}`, `Relatório semanal · ${diaBr(inicio)} a ${diaBr(agora)}`, H.join(''));
    return { ok: true, assunto: `Instagram @${c.username}: semana de ${diaBr(inicio)} a ${diaBr(agora)}`, texto: L.join('\n'), html, foto };
}

export async function relatorioSemanal(env, { enviar = true } = {}) {
    const r = await montarRelatorio(env);
    if (!r.ok || !enviar) return r;
    if (!env.RESEND_API_KEY) return { ok: false, motivo: 'email_desligado', texto: r.texto };
    // direto no Resend (sem a cópia na tabela mensagens: não vira conversa no painel, como a cópia para o Gmail da caixa)
    const x = await fetch((env.RESEND_URL || 'https://api.resend.com') + '/emails', {
        method: 'POST',
        headers: { authorization: 'Bearer ' + env.RESEND_API_KEY, 'content-type': 'application/json' },
        body: JSON.stringify({ from: env.EMAIL_DE || 'InsideStudioSP <contato@insidestudiosp.com.br>', to: [RELATORIO_PARA], subject: r.assunto, text: comAssinatura(r.texto), html: r.html }),
    });
    if (!x.ok) return { ok: false, motivo: 'resend ' + x.status + ': ' + (await x.text()).slice(0, 200), texto: r.texto };
    await env.DB.prepare('INSERT INTO ig_config (chave, valor, quando) VALUES (?, ?, ?) ON CONFLICT(chave) DO UPDATE SET valor = excluded.valor, quando = excluded.quando')
        .bind('relatorio', JSON.stringify(r.foto), r.foto.quando).run();
    return { ok: true, enviado_para: RELATORIO_PARA, assunto: r.assunto, texto: r.texto };
}

// Cron diário: registros de comentário com mais de 90 dias somem (prometido na política de privacidade).
export async function limparInstagram(env) {
    const corte = new Date(Date.now() - 90 * 86400000).toISOString();
    await env.DB.prepare('DELETE FROM ig_respostas WHERE quando < ?').bind(corte).run();
}

export async function rotaInstagram(caminho, req, env, url, ctx) {
    if ((req.method === 'GET' || req.method === 'HEAD') && caminho === '/instagram/privacidade') return pagina('Política de privacidade', PRIVACIDADE);
    if ((req.method === 'GET' || req.method === 'HEAD') && caminho === '/instagram/exclusao-dados') return pagina('Exclusão de dados', EXCLUSAO);
    // mídia pública dos posts agendados (KV MIDIAS; o tipo vem do metadata ou da extensão do nome)
    if ((req.method === 'GET' || req.method === 'HEAD') && caminho.startsWith('/instagram/midia/')) {
        const nome = decodeURIComponent(caminho.slice('/instagram/midia/'.length));
        if (!env.MIDIAS || !/^[\w.-]{1,120}$/.test(nome)) return new Response('não encontrado', { status: 404 });
        const { value, metadata } = await env.MIDIAS.getWithMetadata(nome, { type: 'arrayBuffer' });
        if (!value) return new Response('não encontrado', { status: 404 });
        const ext = (nome.split('.').pop() || '').toLowerCase();
        const tipo = (metadata && metadata.tipo) || { jpg: 'image/jpeg', jpeg: 'image/jpeg', png: 'image/png', mp4: 'video/mp4', mov: 'video/quicktime' }[ext] || 'application/octet-stream';
        return new Response(req.method === 'HEAD' ? null : value, {
            headers: { 'content-type': tipo, 'content-length': String(value.byteLength), 'cache-control': 'public, max-age=86400', 'x-content-type-options': 'nosniff' },
        });
    }
    if (caminho === '/instagram/webhook' && req.method === 'GET') {
        const ok = url.searchParams.get('hub.mode') === 'subscribe' && !!env.IG_VERIFY
            && iguais(url.searchParams.get('hub.verify_token') || '', env.IG_VERIFY);
        return ok ? new Response(url.searchParams.get('hub.challenge') || '', { status: 200 }) : new Response('proibido', { status: 403 });
    }
    if (caminho === '/instagram/webhook' && req.method === 'POST') {
        const corpo = await req.text();
        if (!(await assinaturaOk(env, corpo, req.headers.get('x-hub-signature-256')))) return new Response('assinatura', { status: 401 });
        let dados; try { dados = JSON.parse(corpo); } catch { return new Response('ok'); }
        const trabalho = (async () => {
            // guarda o aviso cru (diagnóstico: saber se a Meta está mandando) e mantém só os 50 últimos
            try {
                await env.DB.prepare('INSERT INTO ig_eventos (quando, corpo) VALUES (?, ?)').bind(new Date().toISOString(), corpo.slice(0, 3000)).run();
                await env.DB.prepare('DELETE FROM ig_eventos WHERE id NOT IN (SELECT id FROM ig_eventos ORDER BY id DESC LIMIT 50)').run();
            } catch (err) { console.error('ig_eventos', err); }
            for (const e of dados.entry || []) {
                for (const c of e.changes || []) {
                    if (c.field !== 'comments' && c.field !== 'live_comments') continue;
                    try { await tratarComentario(env, String(e.id), c.value); } catch (err) { console.error('instagram', err); }
                }
            }
        })();
        if (ctx && ctx.waitUntil) ctx.waitUntil(trabalho); else await trabalho;
        return new Response('ok');   // a Meta quer 200 rápido
    }
    const admin = () => !!env.ADMIN_KEY && iguais(req.headers.get('authorization') || '', 'Bearer ' + env.ADMIN_KEY);
    if (caminho === '/instagram/inscrever' && req.method === 'POST') {
        if (!admin()) return json({ erro: 'nao_autorizado' }, 401);
        const tk = await tokenAtual(env);
        // IGAA: inscreve a própria conta (/me). EAA (usuário do sistema com a conta atribuída): inscreve pelo ID da conta.
        const igId = url.searchParams.get('ig') || env.IG_ID || '17841400749768561';
        if (!/^\d+$/.test(igId)) return json({ erro: 'ig_invalido' }, 400);
        const alvo = ehIg(tk) ? '/me' : '/' + igId;
        const r = await graph(alvo + '/subscribed_apps?subscribed_fields=comments', tk, { method: 'POST' });
        return json({ status: r.status, corpo: r.corpo });
    }
    // agenda: POST /instagram/agendar {tipo, midias[], legenda, quando}; GET /instagram/agenda;
    //         POST /instagram/agenda/cancelar?id=N (só pendente); POST /instagram/agenda/rodar (passo agora, sem esperar o cron)
    if (caminho === '/instagram/agendar' && req.method === 'POST') {
        if (!admin()) return json({ erro: 'nao_autorizado' }, 401);
        let d; try { d = await req.json(); } catch { return json({ erro: 'json_invalido' }, 400); }
        const e = validarAgenda(d);
        if (e) return json({ erro: e }, 400);
        const agora = new Date().toISOString();
        const r = await env.DB.prepare(
            "INSERT INTO ig_agenda (tipo, midias, legenda, quando, estado, tentativas, criado, atualizado) VALUES (?, ?, ?, ?, 'pendente', 0, ?, ?)")
            .bind(d.tipo, JSON.stringify(d.midias), d.legenda || '', new Date(Date.parse(d.quando)).toISOString(), agora, agora).run();
        return json({ ok: true, id: r.meta?.last_row_id });
    }
    if (caminho === '/instagram/agenda' && req.method === 'GET') {
        if (!admin()) return json({ erro: 'nao_autorizado' }, 401);
        const r = await env.DB.prepare('SELECT * FROM ig_agenda ORDER BY quando DESC LIMIT 30').all();
        return json(r.results);
    }
    if (caminho === '/instagram/agenda/cancelar' && req.method === 'POST') {
        if (!admin()) return json({ erro: 'nao_autorizado' }, 401);
        const r = await env.DB.prepare("UPDATE ig_agenda SET estado = 'cancelado', atualizado = ? WHERE id = ? AND estado = 'pendente'")
            .bind(new Date().toISOString(), Number(url.searchParams.get('id')) || 0).run();
        return json({ ok: r.meta?.changes === 1 });
    }
    if (caminho === '/instagram/agenda/rodar' && req.method === 'POST') {
        if (!admin()) return json({ erro: 'nao_autorizado' }, 401);
        return json(await publicarAgendados(env));
    }
    if (caminho === '/instagram/relatorio' && req.method === 'POST') {
        if (!admin()) return json({ erro: 'nao_autorizado' }, 401);
        const r = await relatorioSemanal(env, { enviar: url.searchParams.get('enviar') === '1' });
        delete r.foto;
        return json(r, r.ok ? 200 : 502);
    }
    if (caminho === '/instagram/renovar' && req.method === 'POST') {
        if (!admin()) return json({ erro: 'nao_autorizado' }, 401);
        return json(await renovarTokenInstagram(env));
    }
    if (caminho === '/instagram/diag' && req.method === 'GET') {
        if (!admin()) return json({ erro: 'nao_autorizado' }, 401);
        // ?q=/caminho?campos  -> GET de leitura na Graph API com o token (daqui da Intermed a Meta é bloqueada)
        const q = url.searchParams.get('q');
        if (q) {
            const tk = await tokenAtual(env);
            if (!q.startsWith('/') || !tk) return json({ erro: 'q_invalido' }, 400);
            // "/ig/..." vai para graph.instagram.com (token do login do Instagram); o resto, graph.facebook.com
            let r;
            if (q.startsWith('/ig/')) {
                const c = q.slice(3), sep = c.includes('?') ? '&' : '?';
                const x = await fetch('https://graph.instagram.com/v23.0' + c + sep + 'access_token=' + encodeURIComponent(tk));
                let corpo; try { corpo = await x.json(); } catch { corpo = null; }
                r = { status: x.status, corpo };
            } else r = await graph(q, tk);
            return json({ status: r.status, corpo: r.corpo });
        }
        return json(await diag(env));
    }
    return json({ erro: 'pedido_invalido' }, 404);
}
