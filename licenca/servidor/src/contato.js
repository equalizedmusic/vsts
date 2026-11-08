// Formulário de contato do site (insidestudiosp.com.br/contato/, página estática do site).
//   POST /contato/enviar  (form comum, sem JS: nome, email, assunto, mensagem, site = isca de robô)
// Manda um e-mail de contato@ para contato@ com reply_to = o visitante. O e-mail volta pelo recebimento do Resend
// (src/caixa.js) e cai na Caixa de entrada do painel na conversa do VISITANTE (o webhook usa o reply_to), então a
// resposta pelo painel vai direto para ele. Não grava cópia de saída (seria uma conversa "contato@" inútil).
// Limite por IP e por dia na tabela eventos (tipo 'contato'), sem tabela nova.
// Responde com redirecionamento 303 para as páginas do site: /contato/enviado/, /contato/erro/, /contato/limite/.

import { CONTATO, emailLigado, resendUrl } from './email.js';
import { internoHtml, mailto, esc } from './emailhtml.js';

const POR_IP_HORA = 5;
const TOTAL_DIA = 200;
const agora = () => Math.floor(Date.now() / 1000);
const ASSUNTOS = ['Plugins', 'Estúdio', 'Outro'];

export async function rotaContato(req, env, url) {
    const site = url.hostname.endsWith('insidestudiosp.com.br') ? url.origin : 'https://insidestudiosp.com.br';
    const ir = (p) => Response.redirect(`${site}/contato/${p}/`, 303);
    if (req.method !== 'POST') return Response.redirect(`${site}/contato/`, 302);

    let f;
    try { f = await req.formData(); } catch { return ir('erro'); }
    const campo = (n, max) => String(f.get(n) || '').replace(/\r\n?/g, '\n').trim().slice(0, max);
    if (campo('site', 200)) return ir('enviado');          // robô preencheu a isca: finge que foi
    const nome = campo('nome', 80).replace(/\s+/g, ' ');
    const email = campo('email', 120).toLowerCase();
    const assunto = ASSUNTOS.includes(campo('assunto', 20)) ? campo('assunto', 20) : 'Outro';
    const mensagem = campo('mensagem', 5000);
    if (!nome || mensagem.length < 5 || !/^[^@\s]+@[^@\s]+\.[^@\s]+$/.test(email)) return ir('erro');
    if (!emailLigado(env)) return ir('erro');

    const ip = req.headers.get('cf-connecting-ip') || '';
    const n = await env.DB.prepare("SELECT SUM(ip = ? AND quando > ?) AS ip, COUNT(*) AS total FROM eventos WHERE tipo = 'contato' AND quando > ?")
        .bind(ip, agora() - 3600, agora() - 86400).first();
    if ((n?.ip || 0) >= POR_IP_HORA || (n?.total || 0) >= TOTAL_DIA) return ir('limite');

    const texto = [
        'Mensagem enviada pelo formulário de contato do site (insidestudiosp.com.br/contato).',
        '',
        `Nome: ${nome}`,
        `E-mail: ${email}`,
        `Assunto: ${assunto}`,
        '',
        mensagem,
        '',
        '—',
        'Responder este e-mail (ou pela Caixa de entrada do painel) vai para o e-mail do visitante.',
    ].join('\n');
    let html;   // falha no HTML = vai só o texto
    try {
        html = internoHtml({ titulo: `Contato pelo site · ${assunto}`, sub: 'Formulário insidestudiosp.com.br/contato',
            campos: [['Nome', esc(nome)], ['E-mail', mailto(email)], ['Assunto', esc(assunto)]], texto: mensagem,
            nota: 'Responder este e-mail (ou pela Caixa de entrada do painel) vai para o e-mail do visitante.' });
    } catch (e) { console.error('contato: html', e); }
    const r = await fetch(resendUrl(env, '/emails'), {
        method: 'POST',
        headers: { authorization: 'Bearer ' + env.RESEND_API_KEY, 'content-type': 'application/json' },
        body: JSON.stringify({ from: `${nome.replace(/[<>"@]/g, '')} via site <${CONTATO}>`, to: [CONTATO], reply_to: email,
                               subject: `[Site · ${assunto}] ${nome}`, text: texto, ...(html ? { html } : {}) }),
    });
    if (!r.ok) { console.error('contato: resend', r.status, (await r.text()).slice(0, 300)); return ir('erro'); }
    await env.DB.prepare('INSERT INTO eventos (licenca_id, tipo, detalhe, ip, quando) VALUES (NULL, ?, ?, ?, ?)')
        .bind('contato', email, ip, agora()).run();
    return ir('enviado');
}
