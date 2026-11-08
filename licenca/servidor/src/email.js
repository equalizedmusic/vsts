// Envio de e-mail pelo Resend (https://resend.com), do domínio insidestudiosp.com.br.
// Sem o segredo RESEND_API_KEY ("npx wrangler secret put RESEND_API_KEY") o envio fica desligado e as páginas
// funcionam como antes (token só na tela). EMAIL_DE (variável opcional) troca o remetente.
// Todo e-mail que sai fica copiado na tabela mensagens (aba Caixa de entrada do painel), com o token trocado por
// ••••: o banco nunca guarda token em claro.

// sai de contato@ e a resposta volta para contato@ (caixa de entrada no painel, ver src/caixa.js)
import { textoParaHtml, comAssinatura } from './emailhtml.js';

export const CONTATO = 'contato@insidestudiosp.com.br';
const REMETENTE = `InsideStudioSP <${CONTATO}>`;
export const RESEND_API = 'https://api.resend.com';

export const emailLigado = (env) => !!env.RESEND_API_KEY;
export const resendUrl = (env, caminho) => (env.RESEND_URL || RESEND_API) + caminho;   // RESEND_URL: só para teste local

// vai em texto puro (o mesmo conteúdo do LEIA-ME) E em HTML com o layout da marca e a assinatura do Gmail
// (src/emailhtml.js): o Resend manda multipart e cada cliente mostra o que sabe ler.
//   html: omitido = gerado do texto (textoParaHtml); string = HTML pronto (o texto já deve vir assinado)
//   segredos: textos que não podem ir para a cópia no banco (token, link de troca)
//   cabecalhos: In-Reply-To/References da resposta pelo painel
// devolve o id do Resend (verdadeiro) ou false
export async function enviarEmail(env, { para, assunto, texto, html, segredos = [], cabecalhos, origem = 'automatico' }) {
    if (!emailLigado(env)) return false;
    if (html == null) {
        try { html = textoParaHtml(texto); } catch (e) { console.error('email: html', e); html = null; }   // sem HTML o texto segue sozinho
        texto = comAssinatura(texto);
    }
    let id;
    try {
        const corpo = { from: env.EMAIL_DE || REMETENTE, reply_to: CONTATO, to: [para], subject: assunto, text: texto };
        if (html) corpo.html = html;
        if (cabecalhos) corpo.headers = cabecalhos;
        const r = await fetch(resendUrl(env, '/emails'), {
            method: 'POST',
            headers: { authorization: 'Bearer ' + env.RESEND_API_KEY, 'content-type': 'application/json' },
            body: JSON.stringify(corpo),
        });
        if (!r.ok) { console.error('email: resend', r.status, (await r.text()).slice(0, 300)); return false; }
        id = String((await r.json().catch(() => ({}))).id || '') || true;
    } catch (e) {
        console.error('email:', e);
        return false;
    }
    // a cópia é extra: sem a tabela (migração não aplicada) o e-mail já saiu e o fluxo segue igual
    try {
        let copia = texto;
        for (const s of segredos) if (s) copia = copia.split(s).join('••••');
        await env.DB.prepare('INSERT OR IGNORE INTO mensagens (direcao, resend_id, contato, de, para, assunto, texto, origem, lida, quando) ' +
            "VALUES ('saida', ?, ?, ?, ?, ?, ?, ?, 1, ?)")
            .bind(id === true ? null : id, String(para).toLowerCase(), CONTATO, para, assunto, copia.slice(0, 100000), origem,
                  Math.floor(Date.now() / 1000)).run();
    } catch (e) { console.error('email: cópia na caixa', e); }
    return id;
}
