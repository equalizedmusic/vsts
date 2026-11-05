// Envio de e-mail pelo Resend (https://resend.com), do domínio insidestudiosp.com.br.
// Sem o segredo RESEND_API_KEY ("npx wrangler secret put RESEND_API_KEY") o envio fica desligado e as páginas
// funcionam como antes (token só na tela). EMAIL_DE (variável opcional) troca o remetente.

// sai de contato@ e a resposta volta para contato@ (caixa de entrada no painel, quando o recebimento estiver ligado)
export const CONTATO = 'contato@insidestudiosp.com.br';
const REMETENTE = `InsideStudioSP <${CONTATO}>`;

export const emailLigado = (env) => !!env.RESEND_API_KEY;

// texto puro (sem HTML): cai menos em spam e é o mesmo conteúdo do LEIA-ME
export async function enviarEmail(env, { para, assunto, texto }) {
    if (!emailLigado(env)) return false;
    try {
        const r = await fetch(env.RESEND_URL || 'https://api.resend.com/emails', {   // RESEND_URL: só para teste local
            method: 'POST',
            headers: { authorization: 'Bearer ' + env.RESEND_API_KEY, 'content-type': 'application/json' },
            body: JSON.stringify({ from: env.EMAIL_DE || REMETENTE, reply_to: CONTATO, to: [para], subject: assunto, text: texto }),
        });
        if (!r.ok) { console.error('email: resend', r.status, (await r.text()).slice(0, 300)); return false; }
        return true;
    } catch (e) {
        console.error('email:', e);
        return false;
    }
}
