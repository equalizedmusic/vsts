// Caixa de entrada do contato@insidestudiosp.com.br (aba "Caixa de entrada" do painel).
//
// Recebimento: o MX do domínio aponta para o Resend (Receiving). A cada e-mail que chega, o Resend chama
//   POST /email/entrada   (webhook "email.received", assinado no padrão Svix com o segredo RESEND_WEBHOOK_SECRET)
// O aviso só traz o id; o conteúdo vem de GET /emails/receiving/<id> com a RESEND_API_KEY (chave "Full access":
// a "Sending access" não lê recebidos). Guarda só TEXTO: HTML recebido vira texto aqui e o painel mostra por
// textContent, então e-mail malicioso não roda nada no painel. Anexo fica no Resend; o painel pede o link na hora.
//
// Painel (sessão + x-painel, ver painel.js):
//   GET  /painel/caixa?f=recebidas|todas      conversas (uma por e-mail do outro lado) + não lidas
//   GET  /painel/caixa/conversa?c=<email>      mensagens da conversa (marca as recebidas como lidas)
//   GET  /painel/caixa/anexo?m=<id>&a=<anexo>  { url } link temporário do Resend
//   POST /painel/caixa/enviar   { para, assunto, texto, responder? }   responder = id da mensagem recebida
//   POST /painel/caixa/apagar   { contato }    apaga a conversa do painel (spam)
// Cada e-mail recebido também vai em cópia para insidestudiosp@gmail.com (copiaParaGmail, abaixo).
// Sem a RESEND_API_KEY a aba mostra que o e-mail está desligado; sem a tabela mensagens nada daqui quebra o resto.

import { CONTATO, emailLigado, enviarEmail, resendUrl } from './email.js';

const agora = () => Math.floor(Date.now() / 1000);
const TOLERANCIA_WEBHOOK = 300;      // s: aviso com horário mais velho que isso é recusado (reenvio forjado)

function json(obj, status = 200) {
    return new Response(JSON.stringify(obj), { status, headers: { 'content-type': 'application/json; charset=utf-8', 'cache-control': 'no-store' } });
}
const erro = (mensagem, status) => json({ ok: false, mensagem }, status);

function deB64(texto) {
    const s = atob(texto);
    const out = new Uint8Array(s.length);
    for (let i = 0; i < s.length; i++) out[i] = s.charCodeAt(i);
    return out;
}
function b64(bytes) {
    let s = '';
    for (const b of bytes) s += String.fromCharCode(b);
    return btoa(s);
}
function iguais(a, b) {
    if (a.length !== b.length) return false;
    let d = 0;
    for (let i = 0; i < a.length; i++) d |= a.charCodeAt(i) ^ b.charCodeAt(i);
    return d === 0;
}

// "Nome <a@b.com>" -> a@b.com
export function enderecoDe(s) {
    const t = String(s || '');
    const m = t.match(/<([^<>\s]+@[^<>\s]+)>/);
    return (m ? m[1] : t).trim().toLowerCase();
}

// HTML do e-mail recebido -> texto (só para leitura; nada disso volta a ser HTML)
export function htmlParaTexto(html) {
    let h = String(html || '');
    if (/^data:/i.test(h)) {                                    // html_format "data_uri"
        const i = h.indexOf(',');
        const cab = h.slice(5, i), dado = h.slice(i + 1);
        try { h = /;base64/i.test(cab) ? new TextDecoder().decode(deB64(dado)) : decodeURIComponent(dado); } catch { h = dado; }
    }
    return h
        .replace(/<(script|style|head)[\s\S]*?<\/\1>/gi, '')
        .replace(/<br\s*\/?>/gi, '\n')
        .replace(/<\/(p|div|tr|li|h[1-6]|blockquote)>/gi, '\n')
        .replace(/<li[^>]*>/gi, '• ')
        .replace(/<[^>]+>/g, '')
        .replace(/&nbsp;/gi, ' ').replace(/&lt;/gi, '<').replace(/&gt;/gi, '>').replace(/&quot;/gi, '"').replace(/&#39;/gi, "'")
        .replace(/&#(\d+);/g, (_, n) => String.fromCodePoint(Number(n))).replace(/&amp;/gi, '&')
        .replace(/[ \t]+\n/g, '\n').replace(/\n{3,}/g, '\n\n').trim();
}

// assinatura Svix: base64(HMAC-SHA256(segredo, "<svix-id>.<svix-timestamp>.<corpo>")); o cabeçalho traz "v1,<assinatura>" (uma ou mais)
async function webhookValido(req, corpo, segredo) {
    const id = req.headers.get('svix-id'), ts = req.headers.get('svix-timestamp'), assin = req.headers.get('svix-signature');
    if (!id || !ts || !assin || !/^\d+$/.test(ts) || Math.abs(agora() - Number(ts)) > TOLERANCIA_WEBHOOK) return false;
    const chave = await crypto.subtle.importKey('raw', deB64(segredo.replace(/^whsec_/, '')), { name: 'HMAC', hash: 'SHA-256' }, false, ['sign']);
    const esperado = b64(new Uint8Array(await crypto.subtle.sign('HMAC', chave, new TextEncoder().encode(`${id}.${ts}.${corpo}`))));
    return assin.split(' ').some((p) => { const [v, s] = p.split(','); return v === 'v1' && s && iguais(s, esperado); });
}

const resend = (env, caminho) => fetch(resendUrl(env, caminho), { headers: { authorization: 'Bearer ' + env.RESEND_API_KEY } });

// POST /email/entrada
export async function rotaWebhookEntrada(req, env) {
    if (!env.RESEND_WEBHOOK_SECRET || !emailLigado(env)) return erro('Não encontrado.', 404);
    if (req.method !== 'POST') return erro('Método não aceito.', 405);
    const corpo = await req.text();
    if (corpo.length > 100000 || !(await webhookValido(req, corpo, env.RESEND_WEBHOOK_SECRET))) return erro('Assinatura inválida.', 401);
    let ev;
    try { ev = JSON.parse(corpo); } catch { return erro('Pedido inválido.', 400); }
    if (ev.type !== 'email.received') return json({ ok: true, ignorado: ev.type });
    const id = String(ev.data?.email_id || '');
    if (!/^[\w-]{8,80}$/.test(id)) return erro('Pedido inválido.', 400);

    const ja = await env.DB.prepare("SELECT id FROM mensagens WHERE direcao = 'entrada' AND resend_id = ?").bind(id).first();
    if (ja) return json({ ok: true, repetido: true });
    const r = await resend(env, '/emails/receiving/' + id);
    if (!r.ok) {   // 5xx: o Resend tenta de novo depois
        console.error('caixa: receiving', r.status, (await r.text()).slice(0, 300));
        return erro('Não consegui ler o e-mail no Resend.', 502);
    }
    const m = await r.json();
    const responderPara = Array.isArray(m.reply_to) && m.reply_to[0] ? m.reply_to[0] : m.from;
    const contato = enderecoDe(responderPara);
    const texto = (m.text && String(m.text).trim()) || htmlParaTexto(m.html) || '(e-mail sem texto)';
    const anexos = (m.attachments || []).map((a) => ({ id: String(a.id), nome: String(a.filename || 'anexo'), tipo: String(a.content_type || ''), tamanho: Number(a.size) || 0 }));
    await env.DB.prepare('INSERT OR IGNORE INTO mensagens (direcao, resend_id, contato, de, para, assunto, texto, message_id, anexos, origem, lida, quando) ' +
        "VALUES ('entrada', ?, ?, ?, ?, ?, ?, ?, ?, 'recebido', 0, ?)")
        .bind(id, contato.slice(0, 200), String(m.headers?.from || m.from || '').slice(0, 300), (m.to || []).join(', ').slice(0, 500),
              String(m.subject || '(sem assunto)').slice(0, 300), texto.slice(0, 100000), String(m.message_id || '').slice(0, 300) || null,
              anexos.length ? JSON.stringify(anexos) : null, Math.floor(Date.parse(m.created_at) / 1000) || agora()).run();
    await copiaParaGmail(env, { de: String(m.headers?.from || m.from || contato), contato, assunto: String(m.subject || '(sem assunto)'), texto, anexos });
    return json({ ok: true });
}

// Cópia de cada e-mail recebido para o Gmail dele (o painel continua sendo a caixa oficial).
// Responder no Gmail vai direto para quem escreveu (reply_to). Não grava na tabela mensagens (senão viraria
// uma conversa "insidestudiosp@gmail.com" no painel). Falha aqui não derruba o recebimento.
// COPIA_PARA (variável opcional) troca o destino; COPIA_PARA = "off" desliga.
const COPIA_PARA = 'insidestudiosp@gmail.com';
async function copiaParaGmail(env, { de, contato, assunto, texto, anexos }) {
    const destino = env.COPIA_PARA || COPIA_PARA;
    if (destino === 'off' || !contato || contato === CONTATO || contato === destino.toLowerCase()) return;
    const nome = (de.match(/^\s*"?([^"<]+?)"?\s*</) || [])[1] || contato;
    const aviso = anexos.length ? `\n\n[${anexos.length} anexo(s): abrir no painel, aba Caixa de entrada]` : '';
    try {
        const r = await fetch(resendUrl(env, '/emails'), {
            method: 'POST',
            headers: { authorization: 'Bearer ' + env.RESEND_API_KEY, 'content-type': 'application/json' },
            body: JSON.stringify({
                from: `${nome.replace(/[<>"\r\n]/g, '').slice(0, 60)} via InsideStudioSP <${CONTATO}>`,
                to: [destino], reply_to: contato, subject: assunto.slice(0, 300),
                text: `De: ${de}\nPara: ${CONTATO}\n\n${texto.slice(0, 90000)}${aviso}`,
            }),
        });
        if (!r.ok) console.error('caixa: cópia gmail', r.status, (await r.text()).slice(0, 300));
    } catch (e) { console.error('caixa: cópia gmail', e); }
}

// /painel/caixa/* (a sessão já foi conferida em painel.js)
export async function rotaCaixa(caminho, req, env) {
    const url = new URL(req.url);
    const ligado = emailLigado(env), recebendo = ligado && !!env.RESEND_WEBHOOK_SECRET;

    if (req.method === 'GET' && caminho === '/painel/caixa') {
        const recebidas = url.searchParams.get('f') !== 'todas';
        const { results } = await env.DB.prepare(
            'SELECT m.id, m.contato, m.assunto, m.direcao, m.origem, m.quando, g.n, g.nao_lidas, g.recebidas FROM mensagens m JOIN (' +
            "  SELECT contato, MAX(id) AS ultima, COUNT(*) AS n, SUM(direcao = 'entrada' AND lida = 0) AS nao_lidas, SUM(direcao = 'entrada') AS recebidas" +
            '  FROM mensagens GROUP BY contato) g ON m.id = g.ultima ' +
            (recebidas ? 'WHERE g.recebidas > 0 ' : '') + 'ORDER BY m.id DESC LIMIT 300').all();
        const t = await env.DB.prepare("SELECT COUNT(*) AS n FROM mensagens WHERE direcao = 'entrada' AND lida = 0").first();
        return json({ ok: true, ligado, recebendo, contato: CONTATO, nao_lidas: t?.n || 0, conversas: results });
    }

    if (req.method === 'GET' && caminho === '/painel/caixa/conversa') {
        const c = String(url.searchParams.get('c') || '').toLowerCase();
        const { results } = await env.DB.prepare(
            'SELECT id, direcao, de, para, assunto, texto, anexos, origem, lida, quando FROM mensagens WHERE contato = ? ORDER BY id').bind(c).all();
        await env.DB.prepare("UPDATE mensagens SET lida = 1 WHERE contato = ? AND direcao = 'entrada' AND lida = 0").bind(c).run();
        return json({ ok: true, ligado, contato: c, mensagens: results.map((m) => ({ ...m, anexos: m.anexos ? JSON.parse(m.anexos) : [] })) });
    }

    if (req.method === 'GET' && caminho === '/painel/caixa/anexo') {
        if (!ligado) return erro('E-mail desligado.', 409);
        const m = await env.DB.prepare("SELECT resend_id, anexos FROM mensagens WHERE id = ? AND direcao = 'entrada'")
            .bind(Number(url.searchParams.get('m')) || 0).first();
        const a = String(url.searchParams.get('a') || '');
        if (!m || !(JSON.parse(m.anexos || '[]')).some((x) => x.id === a)) return erro('Anexo não encontrado.', 404);
        const r = await resend(env, `/emails/receiving/${m.resend_id}/attachments/${encodeURIComponent(a)}`);
        const j = r.ok ? await r.json() : null;
        if (!j?.download_url || !/^https:\/\//.test(j.download_url)) return erro('O Resend não entregou o anexo (ele guarda por tempo limitado).', 502);
        return json({ ok: true, url: j.download_url });
    }

    if (req.method !== 'POST') return erro('Não encontrado.', 404);
    let corpo;
    try { corpo = await req.json(); } catch { return erro('Pedido inválido.', 400); }

    if (caminho === '/painel/caixa/enviar') {
        if (!ligado) return erro('O envio de e-mail ainda não está ligado (falta a chave do Resend).', 409);
        const texto = String(corpo.texto || '').replace(/\r?\n/g, '\r\n').trim();
        if (!texto) return erro('Escreva a mensagem.', 400);
        if (texto.length > 50000) return erro('Mensagem grande demais.', 400);
        let para = enderecoDe(corpo.para), assunto = String(corpo.assunto || '').trim().slice(0, 200), cabecalhos, citacao = '';
        if (corpo.responder) {
            const o = await env.DB.prepare("SELECT * FROM mensagens WHERE id = ? AND direcao = 'entrada'").bind(Number(corpo.responder) || 0).first();
            if (!o) return erro('Mensagem original não encontrada.', 404);
            para = o.contato;
            assunto = assunto || (/^re:/i.test(o.assunto || '') ? o.assunto : 'Re: ' + (o.assunto || ''));
            if (o.message_id) cabecalhos = { 'In-Reply-To': o.message_id, References: o.message_id };
            const quando = new Date(o.quando * 1000).toLocaleString('pt-BR', { timeZone: 'America/Sao_Paulo', dateStyle: 'short', timeStyle: 'short' });
            citacao = `\r\n\r\nEm ${quando}, ${o.de || o.contato} escreveu:\r\n` +
                String(o.texto || '').slice(0, 6000).split(/\r?\n/).map((l) => '> ' + l).join('\r\n');
        }
        if (para.length > 120 || !/^[^@\s]+@[^@\s]+\.[^@\s]+$/.test(para)) return erro('Confira o e-mail do destinatário.', 400);
        if (!assunto) return erro('Escreva o assunto.', 400);
        const id = await enviarEmail(env, { para, assunto, texto: texto + citacao, cabecalhos, origem: 'painel' });
        if (!id) return erro('O Resend recusou o envio. Tente de novo em alguns minutos.', 502);
        return json({ ok: true, contato: para });
    }

    if (caminho === '/painel/caixa/apagar') {
        const c = String(corpo.contato || '').toLowerCase();
        const r = await env.DB.prepare('DELETE FROM mensagens WHERE contato = ?').bind(c).run();
        return json({ ok: true, apagadas: r.meta?.changes || 0 });
    }
    return erro('Não encontrado.', 404);
}
