// Loja: cobrança dos packs pelo Mercado Pago (Pix, cartão até 12x, boleto) e pelo PayPal (fora do Brasil).
//   POST /loja/pagar              formulário do site: pacote, email, metodo=mp|paypal -> 303 para o checkout
//   POST /loja/mp/webhook         aviso do Mercado Pago (pagamento criado/atualizado)
//   GET  /loja/retorno?pedido=…   volta do checkout: confere o pagamento e mostra a situação do pedido
// Pagamento aprovado: cria a licença (origem 'venda', 2 computadores) e manda o e-mail PT -> EN com os dados do
// pedido, o token, os links e as instruções. Quem decide se está pago é sempre a API do Mercado Pago / PayPal,
// consultada pelo servidor; o aviso (webhook) e a volta do navegador só dizem "vá conferir".
//
// Segredos: MP_ACCESS_TOKEN e MP_WEBHOOK_SECRET (Mercado Pago > Suas integrações), PAYPAL_CLIENT_ID e PAYPAL_SECRET
// (developer.paypal.com > Apps & Credentials, modo Live). Sem os do Mercado Pago ou do PayPal, aquele botão recusa.
// Sem RESEND_API_KEY a loja inteira recusa (a licença só chega por e-mail).
// Variáveis opcionais: SITE_URL (volta para o site), MP_URL / PAYPAL_URL (teste: sandbox ou simulador local).

import { PRECOS, NOMES } from './precos.js';
import { enviarEmail, emailLigado, CONTATO } from './email.js';

const MP_API = 'https://api.mercadopago.com';
const PAYPAL_API = 'https://api-m.paypal.com';
const SITE = 'https://insidestudiosp-site.insidestudiosp.workers.dev';
const PEDIDOS_POR_IP_HORA = 15;
const PRESO = 300;   // pedido em 'processando' há mais que isso (s) pode ser retomado (o Worker caiu no meio)

const siteUrl = (env) => (env.SITE_URL || SITE).replace(/\/+$/, '');
const mpUrl = (env, c) => (env.MP_URL || MP_API) + c;
const ppUrl = (env, c) => (env.PAYPAL_URL || PAYPAL_API) + c;
const mpLigado = (env) => !!env.MP_ACCESS_TOKEN;
const ppLigado = (env) => !!(env.PAYPAL_CLIENT_ID && env.PAYPAL_SECRET);
const reais = (c) => 'R$ ' + (c / 100).toFixed(2).replace('.', ',');
const dolares = (c) => 'US$ ' + (c / 100).toFixed(2);
const valorTxt = (p) => (p.moeda === 'BRL' ? reais(p.valor_centavos) : dolares(p.valor_centavos));

export async function rotaLoja(caminho, req, env, url, h) {
    if (caminho === '/loja/pagar') return pagar(req, env, url, h);
    if (caminho === '/loja/mp/webhook') return webhookMp(req, env, url, h);
    if (caminho === '/loja/retorno') return retorno(req, env, url, h);
    return h.paginaSimples('Página não encontrada', 'Volte para a loja: ' + siteUrl(env), 404);
}

// ---------------- criar a cobrança ----------------

async function pagar(req, env, url, h) {
    const erro = (pt, en, status = 400) => paginaLoja(env, { titulo: 'Não deu para abrir o pagamento', pt, en, status });
    if (req.method !== 'POST') return Response.redirect(siteUrl(env) + '/', 302);
    let f;
    try { f = await req.formData(); } catch { return erro('Pedido inválido. Volte à loja e tente de novo.', 'Invalid request. Go back to the shop and try again.'); }
    const pacote = String(f.get('pacote') || '');
    const metodo = f.get('metodo') === 'paypal' ? 'paypal' : 'mp';
    const email = h.normEmail(f.get('email'));
    const preco = PRECOS[pacote];
    const volta = `${siteUrl(env)}/${encodeURIComponent(pacote)}/`;
    if (!preco) return erro('Pack desconhecido.', 'Unknown pack.', 404);
    if (email.length > 120 || !/^[^@\s]+@[^@\s]+\.[^@\s]+$/.test(email))
        return erro('Confira o e-mail: é por ele que chegam o token e o download.', 'Check your e-mail: the token and the download are sent to it.');
    const valor = metodo === 'mp' ? preco.brl : preco.usd;
    if (typeof valor !== 'number' || !(valor > 0) || !emailLigado(env) || !(metodo === 'mp' ? mpLigado(env) : ppLigado(env)))
        return erro('Este pagamento ainda não está disponível. Fale com a gente: ' + CONTATO,
                    'This payment option is not available yet. Contact us: ' + CONTATO, 503);

    const ip = req.headers.get('cf-connecting-ip') || '';
    const n = await env.DB.prepare('SELECT COUNT(*) AS n FROM pedidos WHERE ip = ? AND criado_em > ?').bind(ip, h.agora() - 3600).first();
    if ((n?.n || 0) >= PEDIDOS_POR_IP_HORA) return erro('Muitas tentativas seguidas. Espere uma hora ou fale com a gente.', 'Too many attempts. Wait an hour or contact us.', 429);

    const id = 'P' + [...crypto.getRandomValues(new Uint8Array(12))].map((b) => b.toString(16).padStart(2, '0')).join('').toUpperCase();
    const pedido = { id, pacote, email, metodo, valor_centavos: Math.round(valor * 100), moeda: metodo === 'mp' ? 'BRL' : 'USD' };
    await env.DB.prepare('INSERT INTO pedidos (id, pacote, email, metodo, valor_centavos, moeda, status, ip, criado_em) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)')
        .bind(id, pacote, email, metodo, pedido.valor_centavos, pedido.moeda, 'criado', ip, h.agora()).run();

    let destino = null;
    try {
        destino = metodo === 'mp' ? await cobrancaMp(env, url, pedido) : await cobrancaPaypal(env, url, pedido, volta);
    } catch (e) { console.error('loja: criar cobrança', metodo, e); }
    if (!destino) {
        await env.DB.prepare("UPDATE pedidos SET status = 'erro' WHERE id = ?").bind(id).run();
        return erro('O ' + (metodo === 'mp' ? 'Mercado Pago' : 'PayPal') + ' não respondeu agora. Tente de novo em alguns minutos.',
                    (metodo === 'mp' ? 'Mercado Pago' : 'PayPal') + ' did not answer. Try again in a few minutes.', 502);
    }
    await env.DB.prepare('UPDATE pedidos SET ref = ? WHERE id = ?').bind(destino.ref, id).run();
    return new Response(null, { status: 303, headers: { location: destino.link, 'cache-control': 'no-store' } });
}

async function cobrancaMp(env, url, p) {
    const volta = `${url.origin}/loja/retorno?pedido=${p.id}`;
    const r = await fetch(mpUrl(env, '/checkout/preferences'), {
        method: 'POST',
        headers: { authorization: 'Bearer ' + env.MP_ACCESS_TOKEN, 'content-type': 'application/json', 'x-idempotency-key': p.id },
        body: JSON.stringify({
            items: [{ id: p.pacote, title: `${NOMES[p.pacote]} — InsideStudioSP (licença digital)`, category_id: 'others',
                      quantity: 1, currency_id: 'BRL', unit_price: p.valor_centavos / 100 }],
            payer: { email: p.email },
            external_reference: p.id,
            notification_url: `${url.origin}/loja/mp/webhook`,
            back_urls: { success: volta, pending: volta, failure: volta },
            auto_return: 'approved',
            payment_methods: { installments: 12 },
            statement_descriptor: 'INSIDESTUDIOSP',
        }),
    });
    if (!r.ok) { console.error('loja: mp preferência', r.status, (await r.text()).slice(0, 300)); return null; }
    const j = await r.json();
    return j.init_point ? { link: j.init_point, ref: String(j.id || '') } : null;
}

async function tokenPaypal(env) {
    const r = await fetch(ppUrl(env, '/v1/oauth2/token'), {
        method: 'POST',
        headers: { authorization: 'Basic ' + btoa(env.PAYPAL_CLIENT_ID + ':' + env.PAYPAL_SECRET), 'content-type': 'application/x-www-form-urlencoded' },
        body: 'grant_type=client_credentials',
    });
    if (!r.ok) throw new Error('paypal token ' + r.status);
    return (await r.json()).access_token;
}

async function cobrancaPaypal(env, url, p, volta) {
    const r = await fetch(ppUrl(env, '/v2/checkout/orders'), {
        method: 'POST',
        headers: { authorization: 'Bearer ' + await tokenPaypal(env), 'content-type': 'application/json', 'paypal-request-id': p.id },
        body: JSON.stringify({
            intent: 'CAPTURE',
            purchase_units: [{ reference_id: p.id, custom_id: p.id, description: `${NOMES[p.pacote]} — InsideStudioSP (digital license)`,
                               amount: { currency_code: 'USD', value: (p.valor_centavos / 100).toFixed(2) } }],
            payment_source: { paypal: { experience_context: {
                brand_name: 'InsideStudioSP', user_action: 'PAY_NOW', shipping_preference: 'NO_SHIPPING',
                return_url: `${url.origin}/loja/retorno?pedido=${p.id}`, cancel_url: volta } } },
        }),
    });
    if (!r.ok) { console.error('loja: paypal pedido', r.status, (await r.text()).slice(0, 300)); return null; }
    const j = await r.json();
    const link = (j.links || []).find((l) => l.rel === 'payer-action' || l.rel === 'approve')?.href;
    return link && j.id ? { link, ref: j.id } : null;
}

// ---------------- conferir o pagamento ----------------

// Mercado Pago: lê o pagamento na API. Devolve { status: 'pago' | 'pendente' | 'recusado', pagamento } ou null
async function conferirMp(env, pedido, pagamentoId) {
    const r = await fetch(mpUrl(env, '/v1/payments/' + encodeURIComponent(pagamentoId)), { headers: { authorization: 'Bearer ' + env.MP_ACCESS_TOKEN } });
    if (!r.ok) { console.error('loja: mp pagamento', pagamentoId, r.status); return null; }
    const p = await r.json();
    if (String(p.external_reference || '') !== pedido.id) return null;
    if (p.status === 'approved' && p.currency_id === 'BRL' && Math.round(Number(p.transaction_amount) * 100) >= pedido.valor_centavos)
        return { status: 'pago', pagamento: String(p.id), forma: p.payment_type_id === 'bank_transfer' ? 'Pix' : p.payment_type_id === 'ticket' ? 'boleto'
                 : p.installments > 1 ? `cartão em ${p.installments}x` : 'cartão' };
    if (['rejected', 'cancelled', 'refunded', 'charged_back'].includes(p.status)) return { status: 'recusado', pagamento: String(p.id) };
    return { status: 'pendente', pagamento: String(p.id) };
}

// PayPal: captura o pedido aprovado pelo comprador (a captura é que cobra). Já capturado: lê o pedido.
async function conferirPaypal(env, pedido) {
    const tk = await tokenPaypal(env);
    const hd = { authorization: 'Bearer ' + tk, 'content-type': 'application/json', 'paypal-request-id': 'cap-' + pedido.id };
    let r = await fetch(ppUrl(env, `/v2/checkout/orders/${encodeURIComponent(pedido.ref)}/capture`), { method: 'POST', headers: hd, body: '{}' });
    let j = await r.json().catch(() => ({}));
    if (!r.ok && (j.details || []).some((d) => d.issue === 'ORDER_ALREADY_CAPTURED')) {
        r = await fetch(ppUrl(env, `/v2/checkout/orders/${encodeURIComponent(pedido.ref)}`), { headers: hd });
        j = await r.json().catch(() => ({}));
    }
    if (!r.ok) {
        console.error('loja: paypal captura', pedido.id, r.status, JSON.stringify(j).slice(0, 300));
        // comprador não aprovou (voltou sem pagar) = continua pendente; cartão recusado = recusado
        return { status: (j.details || []).some((d) => d.issue === 'INSTRUMENT_DECLINED') ? 'recusado' : 'pendente' };
    }
    const un = (j.purchase_units || [])[0] || {};
    const cap = (un.payments?.captures || [])[0];
    if (!cap || (cap.custom_id || un.custom_id) !== pedido.id) return { status: 'pendente' };
    if (cap.status === 'COMPLETED' && cap.amount?.currency_code === 'USD' && Math.round(Number(cap.amount.value) * 100) >= pedido.valor_centavos)
        return { status: 'pago', pagamento: String(cap.id), forma: 'PayPal' };
    if (['DECLINED', 'FAILED'].includes(cap.status)) return { status: 'recusado', pagamento: String(cap.id) };
    return { status: 'pendente', pagamento: String(cap.id) };
}

async function aplicar(env, url, h, pedido, conf) {
    if (!conf) return pedido.status;
    if (conf.status === 'pago') return concluir(env, url, h, pedido, conf);
    // pendente/recusado só muda pedido que ainda não foi pago
    await env.DB.prepare("UPDATE pedidos SET status = ?, pagamento_id = COALESCE(?, pagamento_id) WHERE id = ? AND status IN ('criado', 'pendente', 'recusado')")
        .bind(conf.status, conf.pagamento || null, pedido.id).run();
    return conf.status;
}

// pagamento aprovado: licença + e-mail, uma vez só (o webhook e a volta do navegador podem chegar juntos)
async function concluir(env, url, h, pedido, conf) {
    const pegou = await env.DB.prepare("UPDATE pedidos SET status = 'processando', pagamento_id = ?, forma = ?, pago_em = ? " +
        "WHERE id = ? AND (status IN ('criado', 'pendente', 'recusado') OR (status = 'processando' AND pago_em < ?))")
        .bind(conf.pagamento, conf.forma, h.agora(), pedido.id, h.agora() - PRESO).run();
    if (!pegou.meta.changes) return (await env.DB.prepare('SELECT status FROM pedidos WHERE id = ?').bind(pedido.id).first())?.status || 'pago';

    let lic = await h.criarLicenca(env, { email: pedido.email, pacote: pedido.pacote, origem: 'venda', pedido: pedido.id,
                                         nota: `${conf.forma} ${valorTxt(pedido)}` });
    let jaTinha = false;
    if (lic.erro === 'ja_existe') { jaTinha = true; lic = { id: lic.id, token: null, mesmo_token: true }; }
    else if (lic.erro) throw new Error('loja: licença ' + lic.erro);
    await h.evento(env, lic.id, 'venda', `${pedido.id} ${pedido.metodo} ${conf.pagamento} ${valorTxt(pedido)}${jaTinha ? ' (já tinha licença)' : ''}`, null);

    const enviado = await enviarEmail(env, {
        para: pedido.email, assunto: `Sua compra: ${NOMES[pedido.pacote]} · Your purchase — InsideStudioSP`,
        texto: emailVenda({ url, pedido, conf, token: lic.token, jaTinha }).join('\r\n'), segredos: [lic.token], origem: 'venda',
    });
    await env.DB.prepare("UPDATE pedidos SET status = 'pago', licenca_id = ?, email_enviado = ? WHERE id = ?")
        .bind(lic.id, enviado ? 1 : 0, pedido.id).run();
    if (!enviado) await h.evento(env, lic.id, 'venda_sem_email', pedido.id + ': mandar o token pelo painel (Novo token)', null);
    return 'pago';
}

// ---------------- avisos e volta do checkout ----------------

async function hmacHex(chave, texto) {
    const k = await crypto.subtle.importKey('raw', new TextEncoder().encode(chave), { name: 'HMAC', hash: 'SHA-256' }, false, ['sign']);
    return [...new Uint8Array(await crypto.subtle.sign('HMAC', k, new TextEncoder().encode(texto)))].map((b) => b.toString(16).padStart(2, '0')).join('');
}

async function webhookMp(req, env, url, h) {
    const ok = () => new Response('ok', { status: 200 });
    if (req.method !== 'POST' || !mpLigado(env)) return ok();
    const corpo = await req.json().catch(() => ({}));
    const tipo = url.searchParams.get('type') || url.searchParams.get('topic') || corpo.type || corpo.topic || '';
    const id = String(url.searchParams.get('data.id') || url.searchParams.get('id') || corpo.data?.id || '');
    if (tipo !== 'payment' || !/^\d{1,20}$/.test(id)) return ok();   // merchant_order e outros: o payment vem em seguida
    // assinatura x-signature (ts=…,v1=…): se veio, tem que conferir. O que vale mesmo é a consulta à API logo abaixo.
    const sig = req.headers.get('x-signature');
    if (sig && env.MP_WEBHOOK_SECRET) {
        const ts = (sig.match(/ts=([^,]+)/) || [])[1], v1 = (sig.match(/v1=([0-9a-f]+)/) || [])[1];
        const manifesto = `id:${id};request-id:${req.headers.get('x-request-id') || ''};ts:${ts};`;
        if (!ts || !v1 || await hmacHex(env.MP_WEBHOOK_SECRET, manifesto) !== v1) return new Response('assinatura', { status: 401 });
    }
    const r = await fetch(mpUrl(env, '/v1/payments/' + id), { headers: { authorization: 'Bearer ' + env.MP_ACCESS_TOKEN } });
    if (!r.ok) return new Response('consulta', { status: 502 });   // o Mercado Pago tenta de novo
    const ref = String((await r.json()).external_reference || '');
    const pedido = /^P[0-9A-F]{24}$/.test(ref) && await env.DB.prepare("SELECT * FROM pedidos WHERE id = ? AND metodo = 'mp'").bind(ref).first();
    if (!pedido) return ok();
    await aplicar(env, url, h, pedido, await conferirMp(env, pedido, id));   // erro aqui = 500 e o Mercado Pago repete
    return ok();
}

async function retorno(req, env, url, h) {
    const id = String(url.searchParams.get('pedido') || '');
    const pedido = /^P[0-9A-F]{24}$/.test(id) && await env.DB.prepare('SELECT * FROM pedidos WHERE id = ?').bind(id).first();
    if (!pedido) return paginaLoja(env, { titulo: 'Pedido não encontrado', pt: 'Confira o link ou fale com a gente.', en: 'Check the link or contact us.', status: 404 });
    let status = pedido.status;
    if (!['pago', 'processando'].includes(status)) {
        try {
            if (pedido.metodo === 'mp') {
                const pid = String(url.searchParams.get('payment_id') || url.searchParams.get('collection_id') || pedido.pagamento_id || '');
                if (/^\d{1,20}$/.test(pid)) status = await aplicar(env, url, h, pedido, await conferirMp(env, pedido, pid));
            } else if (pedido.ref && (url.searchParams.get('token') || '') === pedido.ref) {
                status = await aplicar(env, url, h, pedido, await conferirPaypal(env, pedido));
            }
        } catch (e) { console.error('loja: retorno', id, e); }
    }
    const mascara = pedido.email.replace(/^(.)(.*)(@.*)$/, (_, a, b, c) => a + '•'.repeat(Math.min(b.length, 6)) + c);
    const nome = NOMES[pedido.pacote];
    if (status === 'pago') return paginaLoja(env, { titulo: 'Pagamento aprovado! · Payment approved!',
        pt: `Obrigado pela compra do ${nome}. Mandamos para ${mascara} o token de ativação, os links de download e as instruções. Não chegou em 10 minutos? Olhe o spam e a aba Promoções, ou escreva para ${CONTATO} com o pedido ${pedido.id}.`,
        en: `Thanks for buying ${nome}. We sent the activation token, the download links and the instructions to ${mascara}. Nothing after 10 minutes? Check spam/Promotions, or write to ${CONTATO} with order ${pedido.id}.` });
    if (status === 'recusado') return paginaLoja(env, { titulo: 'Pagamento não aprovado · Payment not approved', link: `${siteUrl(env)}/${pedido.pacote}/`,
        pt: 'O pagamento não foi aprovado e nada foi cobrado. Você pode tentar de novo com outra forma de pagamento.',
        en: 'The payment was not approved and nothing was charged. You can try again with another payment method.' });
    return paginaLoja(env, { titulo: 'Aguardando o pagamento · Waiting for payment', atualizar: true, link: `${siteUrl(env)}/${pedido.pacote}/`,
        pt: `Assim que o ${pedido.metodo === 'mp' ? 'Mercado Pago' : 'PayPal'} confirmar (Pix costuma levar segundos; boleto, até 2 dias úteis), o e-mail com o token chega em ${mascara}. Esta página se atualiza sozinha.`,
        en: `As soon as ${pedido.metodo === 'mp' ? 'Mercado Pago' : 'PayPal'} confirms, the e-mail with your token arrives at ${mascara}. This page refreshes by itself.` });
}

function paginaLoja(env, { titulo, pt, en, status = 200, atualizar = false, link = '' }) {
    const esc = (s) => String(s).replace(/[&<>"]/g, (x) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[x]));
    return new Response(`<!doctype html><html lang="pt-BR"><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1">
${atualizar ? '<meta http-equiv="refresh" content="15">' : ''}<meta name="robots" content="noindex">
<title>InsideStudioSP</title><style>
body { margin:0; background:#0d0b14; color:#eeecf6; font:16px/1.55 "Segoe UI", system-ui, -apple-system, Arial, sans-serif; }
main { max-width:560px; margin:0 auto; padding:40px 20px; } .en { color:#8d8aa3; font-size:14px; font-style:italic; }
a { color:#a78bfa; } .marca { font-weight:800; letter-spacing:.5px; margin-bottom:28px; } .marca b { color:#a78bfa; }
</style><main><div class="marca">Inside<b>Studio</b>SP</div><h2>${esc(titulo)}</h2><p>${esc(pt)}</p><p class="en" lang="en">${esc(en)}</p>
<p><a href="${esc(link || siteUrl(env) + '/')}">← Voltar para a loja · Back to the shop</a></p></main></html>`, {
        status,
        headers: { 'content-type': 'text/html; charset=utf-8', 'cache-control': 'no-store', 'referrer-policy': 'no-referrer',
                   'content-security-policy': "default-src 'none'; style-src 'unsafe-inline'; base-uri 'none'; form-action 'none'; frame-ancestors 'none'" },
    });
}

// ---------------- e-mail da compra (PT primeiro, depois EN) ----------------

function emailVenda({ url, pedido, conf, token, jaTinha }) {
    const nome = NOMES[pedido.pacote];
    const win = `${url.origin}/baixar/${pedido.pacote}/win`, mac = `${url.origin}/baixar/${pedido.pacote}/mac`;
    const data = new Date().toLocaleString('pt-BR', { timeZone: 'America/Sao_Paulo' });
    const meio = pedido.metodo === 'mp' ? 'Mercado Pago' : 'PayPal';
    return [
        `${nome} · InsideStudioSP`, '', '(English below)', '',
        '==================================== PORTUGUÊS ====================================', '',
        'Obrigado pela compra!', '',
        'DADOS DO PEDIDO',
        `  Pedido:     ${pedido.id}`,
        `  Produto:    ${nome} (licença digital para Windows e Mac)`,
        `  Valor:      ${valorTxt(pedido)} · ${conf.forma}`,
        `  Pagamento:  ${meio} nº ${conf.pagamento}`,
        `  Data:       ${data}`, '',
        'SEUS DADOS DE ATIVAÇÃO',
        `  E-mail: ${pedido.email}`,
        token ? `  Token:  ${token}` : '  Token:  o mesmo que você já recebeu da InsideStudioSP para este e-mail (um token vale para todos os seus packs).',
        ...(jaTinha ? ['  Este e-mail já tinha o ' + nome + '. Vamos conferir o pedido e te responder.'] : []),
        '  Licença para 2 computadores. Guarde este e-mail.', '',
        '1. BAIXAR',
        '  Windows: ' + win,
        '  Mac:     ' + mac, '',
        '2. INSTALAR',
        '  Windows: rode o instalador .exe e siga as telas. Os plugins vão para C:\\Program Files\\Common Files\\VST3.',
        '  Mac: abra o .dmg, dê dois cliques no instalador .pkg e siga as telas (VST3 e AU em /Library/Audio/Plug-Ins).',
        '  Depois feche e abra de novo o seu DAW (se o plugin não aparecer, mande o DAW procurar plugins de novo;',
        '  no Logic: Logic Pro > Ajustes > Plug-in Manager > "Redefinir e verificar").', '',
        '3. ATIVAR',
        '  Abra o plugin no DAW. Na tela de ativação, digite o e-mail e o token acima e clique em ATIVAR.',
        '  Precisa de internet só nessa hora. O mesmo token ativa todos os plugins do pack.',
        '  Vai trocar de computador? Menu ≡ do plugin > Licença... > Desativar este computador, e ative no novo.', '',
        '4. SE APARECER AVISO DE SEGURANÇA',
        '  Os instaladores ainda não têm assinatura digital paga, então o Windows e o Mac podem avisar.',
        '  É o aviso padrão para programa novo e sem assinatura, não quer dizer que o arquivo tenha vírus.',
        '  Windows:',
        '  a) Tela azul "O Windows protegeu o computador": clique em "Mais informações" > "Executar assim mesmo".',
        '  b) Chrome/Edge barrou o download: lista de downloads > "..." do arquivo > Manter > Manter assim mesmo.',
        '  c) O Defender apagou o arquivo: Segurança do Windows > Proteção contra vírus e ameaças > Histórico de proteção >',
        '     item do instalador > Ações > Restaurar. Outro antivírus: restaure da quarentena. Não precisa desligar o antivírus.',
        '  Mac:',
        '  a) "A Apple não pode verificá-lo" / "desenvolvedor não identificado": OK > Ajustes do Sistema > Privacidade e',
        '     Segurança > "Abrir Mesmo Assim" e confirme com a senha. (macOS 14 ou anterior: Control + clique > Abrir.)',
        '  b) "Está danificado": no Terminal, rode  xattr -dr com.apple.quarantine ~/Downloads/<nome do arquivo>', '',
        `  Dúvidas ou dificuldade? Responda este e-mail ou escreva para ${CONTATO}. Instagram: @equalizedmusic`, '', '',
        '===================================== ENGLISH =====================================', '',
        'Thank you for your purchase!', '',
        'ORDER DETAILS',
        `  Order:      ${pedido.id}`,
        `  Product:    ${nome} (digital license for Windows and Mac)`,
        `  Amount:     ${valorTxt(pedido)} · ${conf.forma}`,
        `  Payment:    ${meio} #${conf.pagamento}`,
        `  Date:       ${data} (Brasília time)`, '',
        'YOUR ACTIVATION DATA',
        `  E-mail: ${pedido.email}`,
        token ? `  Token:  ${token}` : '  Token:  the same one you already received from InsideStudioSP for this e-mail (one token works for all your packs).',
        ...(jaTinha ? ['  This e-mail already had ' + nome + '. We will check the order and get back to you.'] : []),
        '  License for 2 computers. Keep this e-mail.', '',
        '1. DOWNLOAD',
        '  Windows: ' + win,
        '  Mac:     ' + mac, '',
        '2. INSTALL',
        '  Windows: run the .exe installer and follow the steps. The plugins go to C:\\Program Files\\Common Files\\VST3.',
        '  Mac: open the .dmg, double-click the .pkg installer and follow the steps (VST3 and AU in /Library/Audio/Plug-Ins).',
        '  Then close and reopen your DAW (if the plugin does not show up, rescan your plugins;',
        '  in Logic: Logic Pro > Settings > Plug-in Manager > "Reset & Rescan Selection").', '',
        '3. ACTIVATE',
        '  Open the plugin in your DAW. On the activation screen, type the e-mail and token above and click ATIVAR (activate).',
        '  Internet is needed only at this moment. The same token activates every plugin in the pack.',
        '  Changing computers? Plugin menu ≡ > Licença... > Desativar este computador (deactivate), then activate on the new one.', '',
        '4. IF YOU SEE A SECURITY WARNING',
        '  The installers do not have a paid digital signature yet, so Windows and macOS may warn you.',
        '  This is the standard warning for new unsigned software, it does not mean the file has a virus.',
        '  Windows:',
        '  a) Blue screen "Windows protected your PC": click "More info" > "Run anyway".',
        '  b) Chrome/Edge blocked the download: downloads list > "..." of the file > Keep > Keep anyway.',
        '  c) Defender deleted the file: Windows Security > Virus & threat protection > Protection history >',
        '     installer item > Actions > Restore. Other antivirus: restore it from quarantine. No need to turn your antivirus off.',
        '  macOS:',
        '  a) "Apple cannot check it" / "unidentified developer": OK > System Settings > Privacy & Security >',
        '     "Open Anyway" and confirm with your password. (macOS 14 or older: Control-click > Open.)',
        '  b) "Is damaged": in Terminal, run  xattr -dr com.apple.quarantine ~/Downloads/<file name>', '',
        `  Questions or trouble? Reply to this e-mail or write to ${CONTATO}. Instagram: @equalizedmusic`, '',
        'InsideStudioSP',
    ];
}
