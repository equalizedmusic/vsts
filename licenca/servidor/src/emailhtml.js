// HTML dos e-mails (vai junto com o texto puro: o Resend manda multipart, e o cliente sem HTML lê o texto).
//   textoParaHtml(texto): converte os e-mails de licença/download (texto no padrão do LEIA-ME) no layout da marca,
//     sem reescrever cada e-mail: 1ª linha = título, 2ª = subtítulo, "=== PORTUGUÊS ===" = divisor de idioma,
//     linha em MAIÚSCULAS sem recuo = seção, "  Token:  X" = caixa do token, "  Rótulo: valor" = tabela,
//     link de /baixar/ = botão, recuo de 8+ espaços = comando (monoespaçado).
//   moldura(titulo, sub, corpo): o mesmo layout para quem monta o corpo à mão (relatório do Instagram).
// Só estilo inline e tabelas (Gmail/Outlook ignoram <style> e CSS moderno).

const COR = { fundo: '#f3f1f8', cartao: '#ffffff', topo: '#0d0b14', marca: '#a78bfa', acento: '#6d28d9', texto: '#1f1b2e',
              suave: '#6b6880', linha: '#e7e4f0', claro: '#f5f3ff', aviso: '#fff7e6', avisoBorda: '#f5b942' };
const FONTE = "'Segoe UI', -apple-system, Helvetica, Arial, sans-serif";
const MONO = "Consolas, 'SF Mono', Menlo, monospace";

export const esc = (s) => String(s ?? '').replace(/[&<>"']/g, (x) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[x]));
const URL_RE = /https?:\/\/[^\s<>"')]+[^\s<>"').,;:]/g;
const linkar = (t) => esc(t).replace(/https?:\/\/[^\s<>&]+[^\s<>&.,;:)]/g, (u) => `<a href="${u}" style="color:${COR.acento};word-break:break-all">${u}</a>`);

export function botao(href, rotulo) {
    return `<table role="presentation" cellpadding="0" cellspacing="0" style="margin:6px 0 10px"><tr><td style="background:${COR.acento};border-radius:8px">` +
        `<a href="${esc(href)}" style="display:inline-block;padding:11px 22px;color:#fff;font:600 15px ${FONTE};text-decoration:none">${esc(rotulo)}</a></td></tr></table>`;
}

export function moldura(titulo, sub, corpo, { rodape = true } = {}) {
    return `<!doctype html><html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1"><meta name="color-scheme" content="light"></head>` +
        `<body style="margin:0;padding:0;background:${COR.fundo}">` +
        `<table role="presentation" width="100%" cellpadding="0" cellspacing="0" style="background:${COR.fundo}"><tr><td align="center" style="padding:24px 12px">` +
        `<table role="presentation" width="100%" cellpadding="0" cellspacing="0" style="max-width:640px;background:${COR.cartao};border-radius:14px;overflow:hidden;border:1px solid ${COR.linha}">` +
        `<tr><td style="background:${COR.topo};padding:22px 28px">` +
        `<div style="font:800 15px ${FONTE};letter-spacing:.6px;color:#eeecf6">Inside<span style="color:${COR.marca}">Studio</span>SP</div>` +
        `<div style="font:700 22px/1.3 ${FONTE};color:#ffffff;margin-top:14px">${esc(titulo)}</div>` +
        (sub ? `<div style="font:14px/1.5 ${FONTE};color:#b9b5cc;margin-top:4px">${esc(sub)}</div>` : '') +
        `</td></tr><tr><td style="padding:8px 28px 26px;font:15px/1.6 ${FONTE};color:${COR.texto}">${corpo}</td></tr>` +
        (rodape ? `<tr><td style="padding:18px 28px;background:${COR.claro};border-top:1px solid ${COR.linha}">${ASSINATURA_HTML}</td></tr>` : '') +
        `</table></td></tr></table></body></html>`;
}

// assinatura do Gmail do estúdio (a mesma de assinatura_insidestudiosp.html, 06/11): rodapé de todo e-mail
export const ASSINATURA_HTML =
    `<table role="presentation" cellpadding="0" cellspacing="0" border="0" style="border-collapse:collapse;font-family:'Segoe UI',Arial,Helvetica,sans-serif;color:#2b2838"><tr>` +
    `<td style="padding:0 14px 0 0;border-right:3px solid #a78bfa;vertical-align:middle">` +
    `<div style="font-size:15px;font-weight:700;color:#2b2838;line-height:1.3">Rogério</div>` +
    `<div style="font-size:18px;font-weight:800;letter-spacing:.5px;line-height:1.3;white-space:nowrap"><span style="color:#2b2838">Inside</span><span style="color:#7c5cf0">Studio</span><span style="color:#2b2838">SP</span></div></td>` +
    `<td style="padding:0 0 0 14px;vertical-align:middle;font-size:13px;line-height:1.7;white-space:nowrap">` +
    `<div><span style="color:#ff8a3d">&#9742;</span>&nbsp;<a href="tel:+5511956405215" style="color:#2b2838;text-decoration:none">(11) 95640-5215</a></div>` +
    `<div><span style="color:#ff8a3d">&#9993;</span>&nbsp;<a href="mailto:contato@insidestudiosp.com.br" style="color:#2b2838;text-decoration:none">contato@insidestudiosp.com.br</a></div>` +
    `<div><span style="color:#ff8a3d">&#127760;</span>&nbsp;<a href="https://insidestudiosp.com.br" style="color:#7c5cf0;text-decoration:none;font-weight:600">insidestudiosp.com.br</a></div>` +
    `</td></tr></table>`;
export const ASSINATURA_TXT = ['--', 'Rogério · InsideStudioSP', '(11) 95640-5215 · contato@insidestudiosp.com.br', 'https://insidestudiosp.com.br'];

// texto puro: troca a linha final "InsideStudioSP" pela assinatura (ou acrescenta no fim)
export function comAssinatura(texto) {
    const L = String(texto || '').replace(/\s+$/, '').split(/\r?\n/);
    if (L.length && L[L.length - 1].trim() === 'InsideStudioSP') L.pop();
    while (L.length && !L[L.length - 1].trim()) L.pop();
    return [...L, '', ...ASSINATURA_TXT].join('\r\n');
}

// resposta escrita à mão no painel: sem a moldura de marca (parece e-mail de gente); assinatura antes da citação, como no Gmail
const paraHtml = (t) => String(t || '').split(/\r?\n/).map((l) => linkar(l) || '&nbsp;').join('<br>');
export function respostaHtml(texto, citacao = '') {
    let cit = '';
    if (citacao) {
        const L = String(citacao).replace(/^\s+/, '').split(/\r?\n/);
        const cab = /^>/.test(L[0] || '') ? '' : L.shift();
        cit = `<div style="margin-top:22px;color:#666;font-size:13px">${esc(cab)}</div>` +
            `<blockquote style="margin:6px 0 0;padding:0 0 0 12px;border-left:3px solid #ddd;color:#666">${paraHtml(L.map((l) => l.replace(/^> ?/, '')).join('\n'))}</blockquote>`;
    }
    return `<!doctype html><html><head><meta charset="utf-8"></head><body style="font:15px/1.55 ${FONTE};color:#1f1b2e">` +
        `<div>${paraHtml(texto)}</div><div style="margin-top:18px">${ASSINATURA_HTML}</div>${cit}</body></html>`;
}

const ROTULOS = /^(E-mail|Windows|Mac|macOS|no Logic|in Logic)$/i;

const h3 =(t) => `<div style="font:700 13px ${FONTE};letter-spacing:.8px;color:${COR.acento};text-transform:uppercase;margin:24px 0 8px;padding-bottom:6px;border-bottom:2px solid ${COR.linha}">${esc(t)}</div>`;

export function textoParaHtml(texto) {
    const L = String(texto || '').split(/\r?\n/);
    let i = 0;
    const titulo = (L[i++] || 'InsideStudioSP').trim();
    let sub = '';
    if (L[i] && L[i].trim() && !/^\(english below\)$/i.test(L[i].trim()) && !/^=+/.test(L[i].trim())) sub = L[i++].trim();
    const out = [];
    let kv = [];   // linhas "Rótulo: valor" seguidas viram uma tabela
    const fecharKv = () => {
        if (!kv.length) return;
        out.push(`<table role="presentation" cellpadding="0" cellspacing="0" style="width:100%;margin:4px 0 8px;border-collapse:collapse">` +
            kv.map(([k, v]) => `<tr><td style="padding:5px 12px 5px 0;color:${COR.suave};font-size:14px;white-space:nowrap;vertical-align:top;width:1%">${esc(k)}</td>` +
                `<td style="padding:5px 0;font-size:15px;vertical-align:top">${v}</td></tr>`).join('') + '</table>');
        kv = [];
    };
    for (; i < L.length; i++) {
        const cru = L[i].replace(/\s+$/, ''), t = cru.trim();
        const recuo = cru.length - cru.trimStart().length;
        if (!t) { fecharKv(); out.push('<div style="height:6px"></div>'); continue; }
        if (/^\(english below\)$/i.test(t)) { out.push(`<div style="font:italic 13px ${FONTE};color:${COR.suave};margin-top:14px">English below ↓</div>`); continue; }
        let m = t.match(/^=+\s*(.+?)\s*=+$/);
        if (m) {
            fecharKv();
            out.push(`<div style="margin:30px 0 4px;text-align:center"><span style="display:inline-block;background:${COR.topo};color:#fff;font:700 12px ${FONTE};letter-spacing:1.5px;padding:6px 16px;border-radius:999px">${esc(m[1])}</span></div>`);
            continue;
        }
        if (!recuo && t.length <= 100 && /[A-ZÀ-Ú]/.test(t) && !/[a-zà-ÿ]/.test(t.replace(URL_RE, ''))) { fecharKv(); out.push(h3(t)); continue; }
        if (t === 'InsideStudioSP' && i >= L.length - 2) continue;   // assinatura: o rodapé já leva
        m = t.match(/^Token:\s+(\S{8,})$/);
        if (m) {
            fecharKv();
            out.push(`<div style="margin:8px 0 10px;padding:14px 16px;background:${COR.claro};border:1px dashed ${COR.marca};border-radius:10px">` +
                `<div style="font:12px ${FONTE};color:${COR.suave};letter-spacing:.5px">TOKEN</div>` +
                `<div style="font:700 20px ${MONO};color:${COR.texto};letter-spacing:1px;word-break:break-all;margin-top:2px">${esc(m[1])}</div></div>`);
            continue;
        }
        const links = t.match(URL_RE) || [];
        m = t.match(/^([A-Za-zÀ-ÿ][A-Za-zÀ-ÿ .\-º]{0,15}):\s+(.+)$/);
        // tabela só para dado (rótulo conhecido ou valor alinhado com 2+ espaços); "Outro erro: abra..." é frase
        if (m && recuo >= 2 && recuo <= 7 && (ROTULOS.test(m[1]) || /^\s*[^:]+:\s{2,}\S/.test(t))) {
            const v = m[2].trim();
            if (links.length === 1 && v === links[0] && /\/baixar\//.test(v)) { fecharKv(); out.push(botao(v, '⬇  ' + m[1])); continue; }
            kv.push([m[1], linkar(v)]);
            continue;
        }
        fecharKv();
        if (/^[A-Za-zÀ-ÿ ]{2,20}:$/.test(t)) { out.push(`<div style="font-weight:700;margin:10px 0 2px">${esc(t.slice(0, -1))}</div>`); continue; }
        if (links.length === 1 && /\/baixar\//.test(links[0]) && t.replace(links[0], '').replace(/[:\s]+$/, '').length < 40) {
            const resto = t.replace(links[0], '').replace(/[:\s]+$/, '');
            if (resto) out.push(`<div style="font-size:14px;color:${COR.suave};margin-top:6px">${esc(resto)}</div>`);
            out.push(botao(links[0], 'Baixar · Download'));
            continue;
        }
        if (recuo >= 8 && !/\s{2,}\S/.test(t) && /[\/\\~]|^xattr /.test(t))   // comando ou caminho sozinho na linha
            { out.push(`<div style="margin:4px 0 4px ${Math.min(recuo, 12) * 4}px;padding:8px 10px;background:#f4f4f7;border-radius:6px;font:13px ${MONO};word-break:break-all">${esc(t)}</div>`); continue; }
        const forte = /^[A-ZÀ-Ú]+( [A-ZÀ-Ú]+){3,}/.test(t);   // "ANTES DO PRIMEIRO APPLY..." (4+ palavras em maiúsculas)
        const estilo = forte ? `background:${COR.aviso};border-left:3px solid ${COR.avisoBorda};padding:8px 12px;border-radius:4px;font-weight:600;margin:6px 0` : `margin:2px 0 2px ${Math.max(0, recuo - 2) * 5}px`;
        out.push(`<div style="${estilo}">${linkar(t).replace(/ {2,}/g, (s) => '&nbsp;'.repeat(s.length - 1) + ' ')}</div>`);
    }
    fecharKv();
    return moldura(titulo, sub, out.join(''));
}

// ---- e-mails internos (só chegam para ele: formulário do site e cópia da caixa no Gmail) ----
// mesma moldura da marca, sem a assinatura no rodapé (quem lê é ele mesmo); campos = [[rótulo, valorHtml]]
export const mailto = (e) => `<a href="mailto:${esc(e)}" style="color:${COR.acento};text-decoration:none">${esc(e)}</a>`;
const camposHtml = (campos) => `<table role="presentation" cellpadding="0" cellspacing="0" style="width:100%;margin:14px 0 6px;border-collapse:collapse">` +
    campos.map(([k, v]) => `<tr><td style="padding:6px 14px 6px 0;color:${COR.suave};font-size:14px;white-space:nowrap;vertical-align:top;width:1%">${esc(k)}</td>` +
        `<td style="padding:6px 0;font-size:15px;vertical-align:top;border-bottom:1px solid ${COR.linha}">${v}</td></tr>`).join('') + '</table>';
const notaHtml = (t) => t ? `<div style="margin-top:18px;font:13px/1.5 ${FONTE};color:${COR.suave}">${esc(t)}</div>` : '';
const caixaTexto = (t) => `<div style="margin:12px 0 4px;padding:16px 18px;background:${COR.claro};border-left:3px solid ${COR.marca};border-radius:6px;font:15px/1.6 ${FONTE};color:${COR.texto}">${paraHtml(t)}</div>`;

export function internoHtml({ titulo, sub, campos = [], texto, nota }) {
    return moldura(titulo, sub, camposHtml(campos) + (texto != null ? caixaTexto(texto) : '') + notaHtml(nota), { rodape: false });
}

// cópia de e-mail recebido: com HTML original, a faixa entra logo depois do <body> e o e-mail segue como veio
// (mantém o <head>/<style> de quem mandou); sem HTML, o texto vai na moldura interna
export function copiaRecebidoHtml({ de, contato, para, anexos = 0, htmlOriginal, texto }) {
    const campos = [['De', esc(de)], ['Responder', mailto(contato)], ['Para', esc(para)]];
    if (anexos) campos.push(['Anexos', `${anexos} — abrir no painel, aba Caixa de entrada`]);
    if (!htmlOriginal) return internoHtml({ titulo: 'Recebido em contato@', sub: 'Cópia da Caixa de entrada do painel', campos, texto,
        nota: 'Responder este e-mail vai direto para quem escreveu.' });
    const faixa = `<table role="presentation" width="100%" cellpadding="0" cellspacing="0" style="border-collapse:collapse;margin:0 0 16px"><tr>` +
        `<td style="background:${COR.topo};padding:12px 18px;font:13px/1.6 ${FONTE};color:#d8d4e8">` +
        `<span style="font-weight:800;letter-spacing:.5px;color:#eeecf6">Inside<span style="color:${COR.marca}">Studio</span>SP</span>` +
        `&nbsp;&nbsp;·&nbsp;&nbsp;recebido em contato@&nbsp;&nbsp;·&nbsp;&nbsp;de <b style="color:#fff">${esc(de)}</b>` +
        (anexos ? `&nbsp;&nbsp;·&nbsp;&nbsp;📎 ${anexos} anexo(s) no painel` : '') + `</td></tr></table>`;
    const h = String(htmlOriginal);
    return /<body[^>]*>/i.test(h) ? h.replace(/<body[^>]*>/i, (b) => b + faixa) : faixa + h;
}
