// Loja: cobrança dos packs pelo Mercado Pago (Pix, cartão até 12x, boleto) e pelo PayPal (fora do Brasil).
//   POST /loja/pagar              formulário do site: pacote, nome, email, metodo=mp|paypal -> 303 para o checkout
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

import { precoAtual, NOMES, COMBOS, COMBO_DE, cupomValido, normCupom, licencasPedido, itensPedido, nomePedido } from './precos.js';
import { enviarEmail, emailLigado, CONTATO } from './email.js';

const MP_API = 'https://api.mercadopago.com';
const PAYPAL_API = 'https://api-m.paypal.com';
const SITE = 'https://insidestudiosp.com.br';
const PEDIDOS_POR_IP_HORA = 15;
const PRESO = 300;   // pedido em 'processando' há mais que isso (s) pode ser retomado (o Worker caiu no meio)
// prazo do pedido: 4 dias (dá para um boleto vencer e não ser pago). O link do Mercado Pago e o Pix/boleto gerado
// por ele vencem nesse prazo; a limpeza diária (vencerPedidos, cron do wrangler.toml) marca o pedido 'vencido'.
// Pagamento que mesmo assim chegar depois (boleto pago no prazo e compensado tarde) ainda libera a licença.
const PRAZO = 4 * 86400;
// data no formato do Mercado Pago, no horário de Brasília: 2026-11-09T15:00:00.000-03:00
const dataMp = (seg) => new Date((seg - 3 * 3600) * 1000).toISOString().replace('Z', '-03:00');

const siteUrl = (env) => (env.SITE_URL || SITE).replace(/\/+$/, '');
const mpUrl = (env, c) => (env.MP_URL || MP_API) + c;
const ppUrl = (env, c) => (env.PAYPAL_URL || PAYPAL_API) + c;
const mpLigado = (env) => !!env.MP_ACCESS_TOKEN;
const ppLigado = (env) => !!(env.PAYPAL_CLIENT_ID && env.PAYPAL_SECRET);
const reais = (c) => 'R$ ' + (c / 100).toFixed(2).replace('.', ',');
const dolares = (c) => 'US$ ' + (c / 100).toFixed(2);
const valorTxt = (p) => (p.moeda === 'BRL' ? reais(p.valor_centavos) : dolares(p.valor_centavos));
const descontoTxt = (p) => (p.moeda === 'BRL' ? reais(p.desconto_centavos || 0) : dolares(p.desconto_centavos || 0));
const MAX_ITENS = 20;   // carrinho: no máximo 20 packs num pedido
// 13/11: toda compra sai do carrinho (a página do pack só adiciona), então a volta é sempre o carrinho
const voltaPedido = (env) => `${siteUrl(env)}/carrinho/`;

// "uma vez por comprador": compara o e-mail sem pontos/"+apelido" no Gmail e sem "+apelido" nos outros,
// para joao.silva+1@gmail.com não contar como outra pessoa
function chaveEmail(e) {
    let [u, d] = String(e || '').toLowerCase().split('@');
    if (!d) return u;
    u = u.split('+')[0];
    if (d === 'gmail.com' || d === 'googlemail.com') { u = u.replace(/\./g, ''); d = 'gmail.com'; }
    return u + '@' + d;
}
async function cupomJaUsado(env, codigo, email) {
    const { results } = await env.DB.prepare("SELECT email FROM pedidos WHERE cupom = ? AND status IN ('pago', 'processando', 'devolvido')").bind(codigo).all();
    const k = chaveEmail(email);
    return results.some((r) => chaveEmail(r.email) === k);
}

// Completar o combo (precos.js COMBO_DE): quanto este e-mail já pagou por cada plugin de combo, em centavos.
// Só conta pedido PAGO com a licença ainda ativa. Plugin que veio dentro do próprio combo não gera crédito.
// Pedido com cupom ou carrinho: preço de tabela do plugin na data do pagamento × (pago ÷ total antes do cupom).
async function creditosCombo(env, email) {
    const { results } = await env.DB.prepare("SELECT pacote, valor_centavos, desconto_centavos, pago_em FROM pedidos WHERE email = ? AND status = 'pago'").bind(email).all();
    // licença de TESTE não é compra: não dá crédito nem bloqueia o combo
    const ativas = new Set((await env.DB.prepare("SELECT pacote FROM licencas WHERE email = ? AND status = 'ativa' AND origem <> 'teste'").bind(email).all()).results.map((r) => r.pacote));
    const cred = {};
    for (const p of results) {
        const fator = p.valor_centavos / (p.valor_centavos + (p.desconto_centavos || 0) || 1);
        for (const it of itensPedido(p.pacote)) {
            if (!COMBO_DE[it] || !ativas.has(it)) continue;
            const tabela = precoAtual(it, (p.pago_em || 0) * 1000)?.brl;
            if (typeof tabela === 'number') cred[it] = Math.max(cred[it] || 0, Math.round(tabela * 100 * fator));
        }
    }
    return { cred, ativas };
}

// GET /loja/cupom?c=CODIGO -> { ok, codigo, pct } (o site mostra o desconto antes; quem cobra é o /loja/pagar)
function consultaCupom(url) {
    const c = cupomValido(url.searchParams.get('c'));
    return new Response(JSON.stringify(c ? { ok: true, codigo: c.codigo.toUpperCase(), pct: c.pct, umaPorEmail: !!c.umaPorEmail } : { ok: false }),
        { headers: { 'content-type': 'application/json; charset=utf-8', 'cache-control': 'no-store' } });
}

export async function rotaLoja(caminho, req, env, url, h) {
    if (caminho === '/loja/cupom') return consultaCupom(url);
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
    // um pack (página do pack) ou vários (carrinho: um campo "pacote" por item). Repetido conta uma vez.
    const itens = [...new Set(f.getAll('pacote').map((x) => String(x || '').trim()).filter(Boolean))].slice(0, MAX_ITENS + 1);
    const pacote = itens.join('+');
    const carrinho = itens.length > 1 || f.get('carrinho') === '1';
    const metodo = f.get('metodo') === 'paypal' ? 'paypal' : 'mp';
    const email = h.normEmail(f.get('email'));
    // nome completo: vai ao Mercado Pago (payer.name/surname, ajuda na análise antifraude) e abre o e-mail ("Olá, Fulano")
    const nome = String(f.get('nome') || '').normalize('NFC').replace(/[\x00-\x1f\x7f<>]/g, '').replace(/\s+/g, ' ').trim().slice(0, 80);
    const volta = carrinho ? `${siteUrl(env)}/carrinho/` : `${siteUrl(env)}/${encodeURIComponent(pacote)}/`;
    if (!itens.length || itens.length > MAX_ITENS) return erro('Carrinho vazio ou grande demais.', 'Empty or oversized cart.', 400);
    const precos = itens.map((p) => precoAtual(p));   // MAESTRO sobe para 149,90 depois da Black Friday
    if (precos.some((p) => !p)) return erro('Pack desconhecido.', 'Unknown pack.', 404);
    // combo + o mesmo plugin avulso no carrinho = licença repetida: recusa (o site já evita)
    const lics = licencasPedido(pacote);
    if (new Set(lics).size !== lics.length)
        return erro('O carrinho tem um plugin repetido (por exemplo, o MAESTRO avulso e o combo MAESTRO + MASTER DOS MAGOS). Tire um deles e tente de novo.',
                    'Your cart has the same plugin twice (for example MAESTRO and the MAESTRO + MASTER DOS MAGOS bundle). Remove one and try again.');
    if (email.length > 120 || !/^[^@\s]+@[^@\s]+\.[^@\s]+$/.test(email))
        return erro('Confira o e-mail: é por ele que chegam o token e o download.', 'Check your e-mail: the token and the download are sent to it.');
    if (nome.length < 3 || !/\p{L}/u.test(nome))
        return erro('Preencha o nome completo.', 'Please fill in your full name.');
    const valores = precos.map((p) => (metodo === 'mp' ? p.brl : p.usd));
    if (valores.some((v) => typeof v !== 'number' || !(v > 0)) || !emailLigado(env) || !(metodo === 'mp' ? mpLigado(env) : ppLigado(env)))
        return erro('Este pagamento ainda não está disponível. Fale com a gente: ' + CONTATO,
                    'This payment option is not available yet. Contact us: ' + CONTATO, 503);

    // completar o combo (precos.js COMBO_DE): já tem um plugin do combo e leva o outro avulso = paga só o que falta
    // para o preço do combo. Combo com um dos plugins já no e-mail = recusa e aponta o avulso (sairia licença repetida).
    const completa = [];
    if (metodo === 'mp' && itens.some((p) => COMBO_DE[p] || COMBOS[p])) {
        const { cred, ativas } = await creditosCombo(env, email);
        for (const [i, it] of itens.entries()) {
            if (COMBOS[it]) {
                const tem = COMBOS[it].filter((x) => ativas.has(x)), falta = COMBOS[it].filter((x) => !ativas.has(x));
                if (!tem.length) continue;
                if (!falta.length) return erro(`Este e-mail já tem o ${nomePedido(it)}. Não precisa comprar de novo.`, `This e-mail already owns ${nomePedido(it)}.`, 409);
                return erro(`Este e-mail já tem o ${nomePedido(tem.join('+'))}. Compre só o ${nomePedido(falta.join('+'))} com o mesmo e-mail: o preço já sai abatido para fechar o valor do combo.`,
                            `This e-mail already owns ${nomePedido(tem.join('+'))}. Buy only ${nomePedido(falta.join('+'))} with the same e-mail: the price drops to complete the bundle price.`, 409);
            }
            const c = COMBO_DE[it];
            if (!c || itens.includes(c) || ativas.has(it)) continue;
            const outro = COMBOS[c].find((x) => x !== it);
            if (!cred[outro]) continue;
            const falta = Math.max(100, Math.round(precoAtual(c).brl * 100) - cred[outro]);   // nunca abaixo de R$ 1
            if (falta < Math.round(valores[i] * 100)) { valores[i] = falta / 100; completa.push(it); }
        }
    }

    // cupom: % sobre o total; cada e-mail usa uma vez (pedido pago, em processamento ou devolvido)
    const cupomTxt = normCupom(f.get('cupom'));
    const cupom = cupomTxt ? cupomValido(cupomTxt) : null;
    if (cupomTxt && !cupom) return erro(`O cupom "${cupomTxt.toUpperCase()}" não existe ou já venceu. Confira o código ou compre sem cupom.`,
                                        `Coupon "${cupomTxt.toUpperCase()}" does not exist or has expired. Check the code or buy without it.`);
    if (cupom?.umaPorEmail && await cupomJaUsado(env, cupom.codigo, email))
        return erro(`O cupom ${cupom.codigo.toUpperCase()} já foi usado com este e-mail. Ele vale uma vez por comprador. Você pode comprar sem o cupom.`,
                    `Coupon ${cupom.codigo.toUpperCase()} was already used with this e-mail. It works once per buyer. You can buy without it.`, 409);
    const bruto = valores.reduce((s, v) => s + Math.round(v * 100), 0);
    const desconto = cupom ? Math.round(bruto * cupom.pct / 100) : 0;

    const ip = req.headers.get('cf-connecting-ip') || '';
    const n = await env.DB.prepare('SELECT COUNT(*) AS n FROM pedidos WHERE ip = ? AND criado_em > ?').bind(ip, h.agora() - 3600).first();
    if ((n?.n || 0) >= PEDIDOS_POR_IP_HORA) return erro('Muitas tentativas seguidas. Espere uma hora ou fale com a gente.', 'Too many attempts. Wait an hour or contact us.', 429);

    const id = 'P' + [...crypto.getRandomValues(new Uint8Array(12))].map((b) => b.toString(16).padStart(2, '0')).join('').toUpperCase();
    const pedido = { id, pacote, nome, email, metodo, valor_centavos: bruto - desconto, moeda: metodo === 'mp' ? 'BRL' : 'USD', criado_em: h.agora(),
                     cupom: cupom ? cupom.codigo : null, desconto_centavos: desconto, completa };
    await env.DB.prepare('INSERT INTO pedidos (id, pacote, nome, email, metodo, valor_centavos, moeda, status, ip, criado_em, cupom, desconto_centavos) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)')
        .bind(id, pacote, nome, email, metodo, pedido.valor_centavos, pedido.moeda, 'criado', ip, pedido.criado_em, pedido.cupom, desconto).run();

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
            // carrinho e cupom: um item só com o total (o Mercado Pago não aceita item de desconto negativo)
            items: [{ id: p.pacote.slice(0, 250), title: `${nomePedido(p.pacote)} — InsideStudioSP (licença digital)${p.completa?.length ? ' · completa o combo' : ''}${p.cupom ? ` · cupom ${p.cupom.toUpperCase()}` : ''}`.slice(0, 250), category_id: 'others',
                      quantity: 1, currency_id: 'BRL', unit_price: p.valor_centavos / 100 }],
            payer: { email: p.email, ...partesNome(p.nome) },
            external_reference: p.id,
            notification_url: `${url.origin}/loja/mp/webhook`,
            back_urls: { success: volta, pending: volta, failure: volta },
            auto_return: 'approved',
            payment_methods: { installments: 12 },
            statement_descriptor: 'INSIDESTUDIOSP',
            expires: true, expiration_date_from: dataMp(p.criado_em), expiration_date_to: dataMp(p.criado_em + PRAZO),
            date_of_expiration: dataMp(p.criado_em + PRAZO),
        }),
    });
    if (!r.ok) { console.error('loja: mp preferência', r.status, (await r.text()).slice(0, 300)); return null; }
    const j = await r.json();
    return j.init_point ? { link: j.init_point, ref: String(j.id || '') } : null;
}

// "Maria da Silva Souza" -> { name: 'Maria', surname: 'da Silva Souza' } (o Mercado Pago pede os dois separados)
function partesNome(nome) {
    const [primeiro, ...resto] = String(nome || '').split(' ').filter(Boolean);
    return primeiro ? { name: primeiro, ...(resto.length ? { surname: resto.join(' ') } : {}) } : {};
}
const primeiroNome = (nome) => partesNome(nome).name || '';

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
            purchase_units: [{ reference_id: p.id, custom_id: p.id, description: `${nomePedido(p.pacote)} — InsideStudioSP (digital license)`.slice(0, 127),
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
        "WHERE id = ? AND (status IN ('criado', 'pendente', 'recusado', 'vencido') OR (status = 'processando' AND pago_em < ?))")
        .bind(conf.pagamento, conf.forma, h.agora(), pedido.id, h.agora() - PRESO).run();
    if (!pegou.meta.changes) return (await env.DB.prepare('SELECT status FROM pedidos WHERE id = ?').bind(pedido.id).first())?.status || 'pago';

    // combo (ex.: MAESTRO + MAGOS) e carrinho: uma licença por pacote, todas com o mesmo token do e-mail
    const pacotes = licencasPedido(pedido.pacote);
    const extra = (pacotes.length > 1 ? ' (' + nomePedido(pedido.pacote) + ')' : '') + (pedido.cupom ? ' cupom ' + pedido.cupom.toUpperCase() : '');
    let lic = null, token = null;
    const jaTinha = [];
    for (const pac of pacotes) {
        let l = await h.criarLicenca(env, { email: pedido.email, pacote: pac, origem: 'venda', pedido: pedido.id,
                                           nota: `${conf.forma} ${valorTxt(pedido)}${extra}`.slice(0, 300) });
        let ja = false;
        if (l.erro === 'ja_existe') { ja = true; jaTinha.push(nomePacote(pac)); l = { id: l.id, token: null, mesmo_token: true }; }
        else if (l.erro) throw new Error('loja: licença ' + pac + ' ' + l.erro);
        token = token || l.token;   // o 1º pacote novo gera o token (se o e-mail ainda não tinha); os outros usam o mesmo
        lic = lic || l;
        await h.evento(env, l.id, 'venda', `${pedido.id} ${pedido.metodo} ${conf.pagamento} ${valorTxt(pedido)}${pacotes.length > 1 ? ' ' + pedido.pacote : ''}${pedido.cupom ? ' cupom ' + pedido.cupom : ''}${ja ? ' (já tinha licença)' : ''}`, null);
    }
    lic = { ...lic, token };

    const enviado = await enviarEmail(env, {
        para: pedido.email, assunto: `Sua compra: ${nomePedido(pedido.pacote)} · Your purchase — InsideStudioSP`.slice(0, 200),
        texto: emailVenda({ url, pedido, conf, token: lic.token, jaTinha: jaTinha.length ? jaTinha.join(', ') : false }).join('\r\n'), segredos: [lic.token], origem: 'venda',
    });
    await env.DB.prepare("UPDATE pedidos SET status = 'pago', licenca_id = ?, email_enviado = ? WHERE id = ?")
        .bind(lic.id, enviado ? 1 : 0, pedido.id).run();
    if (!enviado) await h.evento(env, lic.id, 'venda_sem_email', pedido.id + ': mandar o token pelo painel (Novo token)', null);
    return 'pago';
}

// ---------------- pós-venda (12/11/2026, ideia dele) ----------------
// 3 dias depois do pagamento: e-mail perguntando se está tudo certo, PT -> EN, no layout da marca (enviarEmail gera o HTML).
// Roda no cron de 10 em 10 min, só entre 10h e 19h de Brasília (não chega de madrugada). Um por pedido (posvenda_em);
// pedido devolvido não recebe. Só pedidos pagos a partir de POSVENDA_DESDE (quem comprou antes não recebe atrasado).
const POSVENDA_DIAS = 3;
const POSVENDA_DESDE = Date.parse('2026-11-12T00:00:00-03:00') / 1000;
export async function posVenda(env, agora) {
    if (!emailLigado(env)) return 0;
    const hora = new Date((agora - 3 * 3600) * 1000).getUTCHours();
    if (hora < 10 || hora >= 19) return 0;
    const { results } = await env.DB.prepare("SELECT * FROM pedidos WHERE status = 'pago' AND posvenda_em IS NULL AND pago_em >= ? AND pago_em <= ? ORDER BY pago_em LIMIT 10")
        .bind(POSVENDA_DESDE, agora - POSVENDA_DIAS * 86400).all();
    let n = 0;
    for (const p of results) {
        // marca antes de enviar: dois crons juntos não mandam dobrado (se o envio falhar, fica marcado com -1 no evento)
        const pegou = await env.DB.prepare('UPDATE pedidos SET posvenda_em = ? WHERE id = ? AND posvenda_em IS NULL').bind(agora, p.id).run();
        if (!pegou.meta.changes) continue;
        const ok = await enviarEmail(env, { para: p.email, assunto: `Tudo certo com o ${nomePedido(p.pacote)}? · How is it going? — InsideStudioSP`.slice(0, 200),
                                            texto: textoPosVenda(p).join('\r\n'), origem: 'posvenda' });
        if (!ok) await env.DB.prepare('UPDATE pedidos SET posvenda_em = -1 WHERE id = ?').bind(p.id).run();
        else n++;
    }
    if (n) console.log('loja: pós-venda enviados', n);
    return n;
}

export function textoPosVenda(p) {
    const nome = nomePedido(p.pacote), ola = primeiroNome(p.nome), varios = licencasPedido(p.pacote).length > 1;
    return [
        `${nome} · InsideStudioSP`, '', '(English below)', '',
        ola ? `Olá, ${ola}!` : 'Olá!', '',
        `Faz alguns dias que você ${varios ? 'levou o ' + nome : 'comprou o ' + nome}, e eu queria saber: está tudo certo?`,
        `Conseguiu instalar e ativar ${varios ? 'os plugins' : 'o plugin'} no seu DAW? Ficou alguma dúvida sobre como usar?`, '',
        'Se precisar de qualquer coisa (instalação, ativação, trocar de computador ou uma dica de uso), é só responder este e-mail.',
        'Quem responde sou eu mesmo, e estou à disposição.', '',
        'E se puder, me conta o que achou. A sua opinião ajuda a melhorar os próximos plugins.', '',
        `Pedido: ${p.id}`, '', '',
        '------------------------------------------------------------', '',
        ola ? `Hi ${ola}!` : 'Hi!', '',
        `It has been a few days since you got ${nome}, and I wanted to check: is everything working?`,
        `Did you manage to install and activate ${varios ? 'the plugins' : 'the plugin'} in your DAW? Any questions about how to use ${varios ? 'them' : 'it'}?`, '',
        'If you need anything (install, activation, moving to another computer or a usage tip), just reply to this e-mail.',
        'I answer it myself and I am here to help.', '',
        'And if you can, tell me what you think. Your feedback helps shape the next plugins.', '',
        `Order: ${p.id}`,
    ];
}

// limpeza diária (cron): pedido aberto há mais de 4 dias sem pagamento vira 'vencido'. Não apaga nada.
export async function vencerPedidos(env, agora) {
    const r = await env.DB.prepare("UPDATE pedidos SET status = 'vencido' WHERE status IN ('criado', 'pendente') AND criado_em < ?")
        .bind(agora - PRAZO).run();
    if (r.meta.changes) console.log('loja: pedidos vencidos', r.meta.changes);
    return r.meta.changes || 0;
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
    const nome = nomePedido(pedido.pacote);
    // pago: a volta limpa o carrinho do navegador (/carrinho/?pago=1, site.js)
    if (status === 'pago') return paginaLoja(env, { titulo: 'Pagamento aprovado! · Payment approved!', link: siteUrl(env) + '/carrinho/?pago=1',
        pt: `Obrigado pela compra do ${nome}. Mandamos para ${mascara} o token de ativação, os links de download e as instruções. Não chegou em 10 minutos? Olhe o spam e a aba Promoções, ou escreva para ${CONTATO} com o pedido ${pedido.id}.`,
        en: `Thanks for buying ${nome}. We sent the activation token, the download links and the instructions to ${mascara}. Nothing after 10 minutes? Check spam/Promotions, or write to ${CONTATO} with order ${pedido.id}.` });
    if (status === 'vencido') return paginaLoja(env, { titulo: 'Pedido vencido · Order expired', link: voltaPedido(env, pedido),
        pt: 'Este pedido passou de 4 dias sem pagamento e venceu. Nada foi cobrado. Para comprar, faça um pedido novo na loja.',
        en: 'This order went 4 days without payment and expired. Nothing was charged. To buy, place a new order in the shop.' });
    if (status === 'recusado') return paginaLoja(env, { titulo: 'Pagamento não aprovado · Payment not approved', link: voltaPedido(env, pedido),
        pt: 'O pagamento não foi aprovado e nada foi cobrado. Você pode tentar de novo com outra forma de pagamento.',
        en: 'The payment was not approved and nothing was charged. You can try again with another payment method.' });
    return paginaLoja(env, { titulo: 'Aguardando o pagamento · Waiting for payment', atualizar: true, link: voltaPedido(env, pedido),
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

// ---------------- e-mail da licença (PT primeiro, depois EN) ----------------
// Usado na venda (com o bloco do pedido) e no painel ao adicionar pacote (src/avisos.js: um e-mail por pacote).
//   base: endereço dos links /baixar/<pacote>/<win|mac>; sistemas: só entram os links e passos dos marcados
//   trocado: o token novo substituiu o da página grátis (avisa que o anterior parou de valer)

const NOME_EXTRA = { align: 'ALIGN', barebass: 'BAREBASS', vassoura: 'VASSOURA', rodo: 'RODO', montamonta: 'MONTA-MONTA' };
// Max for Live (14/11/2026: VASSOURA e RODO pagos, com ativação): zip único Windows/Mac em /baixar/<id>/zip; arquivos da pasta
export const M4L = {
    vassoura: { amxd: 'Vassoura.amxd', arquivos: 'Vassoura.amxd, vassoura.js, vassoura_view.js e vassoura_strip.js',
                arquivosEn: 'Vassoura.amxd, vassoura.js, vassoura_view.js and vassoura_strip.js' },
    rodo: { amxd: 'Rodo.amxd', arquivos: 'Rodo.amxd, rodo.js, rodo_view.js e rodo_strip.js',
            arquivosEn: 'Rodo.amxd, rodo.js, rodo_view.js and rodo_strip.js' },
};
export const nomePacote = (p) => (String(p).includes('+') ? String(p).split('+').map(nomePacote).join(' + ')
    : NOMES[p] || NOME_EXTRA[p] || String(p).toUpperCase());

// Manual em PDF no site (site/manuais, publicado em /manuais/). Combo/carrinho = um link por produto.
const SITE_MANUAIS = 'https://insidestudiosp.com.br/manuais/';
const ARQ_MANUAL = { align: 'ALIGN', barebass: 'BAREBASS', vassoura: 'VASSOURA', mesa: 'MESA', stompbox: 'STOMPBOX', maestro: 'MAESTRO',
    magos: 'MAGOS', criativo: 'SERIE-CRIATIVO', mix: 'SERIE-MIX', knob: 'SERIE-KNOB', quimera: 'QUIMERA', limao: 'LIMAO', mercurio: 'MERCURIO',
    resenha: 'RESENHA', groovador: 'GROOVADOR', rodo: 'RODO' };
// en = true: versão em inglês (<NOME>-Manual-EN.pdf), usada no bloco ENGLISH do e-mail
export const linksManual = (pacote, en = false) => [...new Set(String(pacote).split('+').flatMap((p) => COMBOS[p] || [p]))]
    .filter((p) => ARQ_MANUAL[p]).map((p) => `  ${nomePacote(p)}: ${SITE_MANUAIS}${ARQ_MANUAL[p]}-Manual${en ? '-EN' : ''}.pdf`);

function emailVenda({ url, pedido, conf, token, jaTinha }) {
    return textoLicenca({ base: url.origin, pacote: pedido.pacote, email: pedido.email, cliente: pedido.nome, token, jaTinha, pedido, conf,
                         downloads: licencasPedido(pedido.pacote).length > 1 ? licencasPedido(pedido.pacote) : null });
}

export function textoLicenca({ base, pacote, email, cliente = '', token, jaTinha = false, pedido = null, conf = null, maquinas = 2,
                               sistemas = ['win', 'mac'], trocado = false, downloads = null, teste = 0 }) {
    const nome = nomePacote(pacote);
    const ola = primeiroNome(cliente);
    const W = sistemas.includes('win') || !sistemas.includes('mac'), M = sistemas.includes('mac') || !W;
    // combo: um link por plugin (MAESTRO, MASTER DOS MAGOS); pacote comum: um link só, como antes
    const itens = downloads || [pacote];
    const vsts = itens.filter((p) => !M4L[p]), m4ls = itens.filter((p) => M4L[p]);
    const V = vsts.length > 0, varios = itens.length > 1;
    const baixar = (so) => vsts.map((p) => (varios ? `  ${so === 'win' ? 'Windows' : 'Mac'} · ${nomePacote(p)}: ` : (so === 'win' ? '  Windows: ' : '  Mac:     ')) + `${base}/baixar/${p}/${so}`);
    const baixarM4l = (en) => m4ls.map((p) => `  ${en ? 'Max for Live (Windows and Mac)' : 'Max for Live (Windows e Mac)'} · ${nomePacote(p)}: ${base}/baixar/${p}/zip`);
    const nomesM4l = m4ls.map(nomePacote).join(' e '), nomesM4lEn = m4ls.map(nomePacote).join(' and ');
    const data = new Date().toLocaleString('pt-BR', { timeZone: 'America/Sao_Paulo' });
    const meio = pedido && (pedido.metodo === 'mp' ? 'Mercado Pago' : 'PayPal');
    const so = (cond, ...linhas) => (cond ? linhas : []);
    const manuais = linksManual(pacote);
    const maqPt = maquinas > 1 ? maquinas + ' computadores' : '1 computador', maqEn = maquinas > 1 ? maquinas + ' computers' : '1 computer';
    return [
        `${nome} · InsideStudioSP`, '', '(English below)', '',
        '==================================== PORTUGUÊS ====================================', '',
        ...(pedido ? [
            ola ? `Olá, ${ola}! Obrigado pela compra!` : 'Obrigado pela compra!', '',
            'DADOS DO PEDIDO',
            `  Pedido:     ${pedido.id}`,
            `  Produto:    ${nome} (licença digital para Windows e Mac)`,
            `  Valor:      ${valorTxt(pedido)} · ${conf.forma}`,
            ...so(pedido.cupom, `  Cupom:      ${String(pedido.cupom).toUpperCase()} (desconto de ${descontoTxt(pedido)} já aplicado)`),
            `  Pagamento:  ${meio} nº ${conf.pagamento}`,
            `  Data:       ${data}`, '',
        ] : teste ? [`Liberamos para você um TESTE de ${teste} dias do ${nome}. Abaixo estão os dados de ativação, o download e o passo a passo.`, '']
          : [`Acesso liberado: ${nome}. Abaixo estão os dados de ativação, o download e o passo a passo.`, '']),
        'SEUS DADOS DE ATIVAÇÃO',
        `  E-mail: ${email}`,
        token ? `  Token:  ${token}`
            : teste ? '  Token:  o mesmo token de TESTE que você já recebeu da InsideStudioSP (ele é diferente do token de compra).'
            : '  Token:  o mesmo que você já recebeu da InsideStudioSP para este e-mail (um token vale para todos os seus packs).',
        ...so(token && trocado, '  Este token substitui o que apareceu na página de cadastro grátis: o anterior não vale mais.'),
        ...so(jaTinha, '  Este e-mail já tinha o ' + (typeof jaTinha === 'string' ? jaTinha : nome) + '. Vamos conferir o pedido e te responder.'),
        ...(teste ? [
            `  Licença de TESTE para 1 computador. Os ${teste} dias contam a partir da ativação no seu computador.`,
            `  Terminado o teste, o plugin para de funcionar. Para continuar usando, compre o ${nome} em ${base}`,
            '  com este mesmo e-mail. Guarde este e-mail.', '',
        ] : [`  Licença para ${maqPt}. Guarde este e-mail.`, '']),
        '1. BAIXAR',
        ...(V && W ? baixar('win') : []),
        ...(V && M ? baixar('mac') : []),
        ...baixarM4l(false), '',
        '2. INSTALAR',
        ...so(m4ls.length,
            `  ${nomesM4l} (Max for Live): precisa do Ableton Live 12.2 ou mais novo (Suite, ou Standard com Max for Live).`,
            '  Extraia o zip e copie a pasta inteira para a User Library do Live:',
            '    Windows: Documentos\\Ableton\\User Library\\Presets\\Audio Effects\\Max Audio Effect\\',
            '    Mac:     Músicas/Ableton/User Library/Presets/Audio Effects/Max Audio Effect/',
            ...m4ls.map((p) => `  ${nomePacote(p)}: os arquivos ${M4L[p].arquivos} PRECISAM ficar juntos na mesma pasta.`),
            ...m4ls.map((p) => `  No Live, arraste o ${M4L[p].amxd} para a PRIMEIRA faixa de áudio do projeto (no MASTER ele não funciona).`)),
        ...so(V && W, '  Windows: rode o instalador .exe e siga as telas. Os plugins vão para C:\\Program Files\\Common Files\\VST3.'),
        ...so(V && M, '  Mac: abra o .dmg, dê dois cliques no instalador .pkg e siga as telas (VST3 e AU em /Library/Audio/Plug-Ins).'),
        ...so(V, '  Depois feche e abra de novo o seu DAW (se o plugin não aparecer, mande o DAW procurar plugins de novo' + (M ? ';' : ').')),
        ...so(V && M, '  no Logic: Logic Pro > Ajustes > Plug-in Manager > "Redefinir e verificar").'), '',
        '3. ATIVAR',
        ...so(V,
            '  Abra o plugin no DAW. Na tela de ativação, digite o e-mail e o token acima e clique em ATIVAR.',
            '  Precisa de internet só nessa hora. O mesmo token ativa todos os plugins do pack.',
            '  Vai trocar de computador? Menu ≡ do plugin > Licença... > Desativar este computador, e ative no novo.'),
        ...so(m4ls.length,
            `  ${nomesM4l}: no device, clique no botão OPEN. Na 1ª vez a janela pede o e-mail e o token acima: digite os dois`,
            '  e clique em ACTIVATE. Precisa de internet só nessa hora; depois funciona sem internet por até 45 dias.'),
        '',
        ...so(V, '4. SE APARECER AVISO DE SEGURANÇA'),
        ...so(V, '  Os instaladores ainda não têm assinatura digital paga, então ' + (W && M ? 'o Windows e o Mac podem avisar.' : W ? 'o Windows pode avisar.' : 'o Mac pode avisar.'),
            '  É o aviso padrão para programa novo e sem assinatura, não quer dizer que o arquivo tenha vírus.'),
        ...so(V && W,
            '  Windows:',
            '  a) Tela azul "O Windows protegeu o computador": clique em "Mais informações" > "Executar assim mesmo".',
            '  b) Chrome/Edge barrou o download: lista de downloads > "..." do arquivo > Manter > Manter assim mesmo.',
            '  c) O Defender apagou o arquivo: Segurança do Windows > Proteção contra vírus e ameaças > Histórico de proteção >',
            '     item do instalador > Ações > Restaurar. Outro antivírus: restaure da quarentena. Não precisa desligar o antivírus.'),
        ...so(V && M,
            '  Mac:',
            '  a) "A Apple não pode verificá-lo" / "desenvolvedor não identificado": OK > Ajustes do Sistema > Privacidade e',
            '     Segurança > "Abrir Mesmo Assim" e confirme com a senha. (macOS 14 ou anterior: Control + clique > Abrir.)',
            '  b) "Está danificado": no Terminal, rode  xattr -dr com.apple.quarantine ~/Downloads/<nome do arquivo>'), ...so(V, ''),
        ...so(manuais.length, 'MANUAL EM PDF (passo a passo de cada controle)', ...manuais, ''),
        `  Dúvidas ou dificuldade? Responda este e-mail ou escreva para ${CONTATO}. Instagram: @equalizedmusic`, '', '',
        '===================================== ENGLISH =====================================', '',
        ...(pedido ? [
            ola ? `Hi ${ola}, thank you for your purchase!` : 'Thank you for your purchase!', '',
            'ORDER DETAILS',
            `  Order:      ${pedido.id}`,
            `  Product:    ${nome} (digital license for Windows and Mac)`,
            `  Amount:     ${valorTxt(pedido)} · ${conf.forma}`,
            ...so(pedido.cupom, `  Coupon:     ${String(pedido.cupom).toUpperCase()} (${descontoTxt(pedido)} off, already applied)`),
            `  Payment:    ${meio} #${conf.pagamento}`,
            `  Date:       ${data} (Brasília time)`, '',
        ] : teste ? [`You have a ${teste}-day TRIAL of ${nome}. Below are your activation data, the download and the steps.`, '']
          : [`Access granted: ${nome}. Below are your activation data, the download and the steps.`, '']),
        'YOUR ACTIVATION DATA',
        `  E-mail: ${email}`,
        token ? `  Token:  ${token}`
            : teste ? '  Token:  the same TRIAL token you already received from InsideStudioSP (it is different from the purchase token).'
            : '  Token:  the same one you already received from InsideStudioSP for this e-mail (one token works for all your packs).',
        ...so(token && trocado, '  This token replaces the one shown on the free sign-up page: the old one no longer works.'),
        ...so(jaTinha, '  This e-mail already had ' + (typeof jaTinha === 'string' ? jaTinha : nome) + '. We will check the order and get back to you.'),
        ...(teste ? [
            `  TRIAL license for 1 computer. The ${teste} days start when you activate it on your computer.`,
            `  When the trial ends, the plugin stops working. To keep using it, buy ${nome} at ${base}`,
            '  with this same e-mail. Keep this e-mail.', '',
        ] : [`  License for ${maqEn}. Keep this e-mail.`, '']),
        '1. DOWNLOAD',
        ...(V && W ? baixar('win') : []),
        ...(V && M ? baixar('mac') : []),
        ...baixarM4l(true), '',
        '2. INSTALL',
        ...so(m4ls.length,
            `  ${nomesM4lEn} (Max for Live): needs Ableton Live 12.2 or newer (Suite, or Standard with Max for Live).`,
            '  Unzip it and copy the whole folder into the Live User Library:',
            '    Windows: Documents\\Ableton\\User Library\\Presets\\Audio Effects\\Max Audio Effect\\',
            '    Mac:     Music/Ableton/User Library/Presets/Audio Effects/Max Audio Effect/',
            ...m4ls.map((p) => `  ${nomePacote(p)}: the files ${M4L[p].arquivosEn} MUST stay together in the same folder.`),
            ...m4ls.map((p) => `  In Live, drag ${M4L[p].amxd} onto the FIRST audio track of the set (it does not work on the MASTER).`)),
        ...so(V && W, '  Windows: run the .exe installer and follow the steps. The plugins go to C:\\Program Files\\Common Files\\VST3.'),
        ...so(V && M, '  Mac: open the .dmg, double-click the .pkg installer and follow the steps (VST3 and AU in /Library/Audio/Plug-Ins).'),
        ...so(V, '  Then close and reopen your DAW (if the plugin does not show up, rescan your plugins' + (M ? ';' : ').')),
        ...so(V && M, '  in Logic: Logic Pro > Settings > Plug-in Manager > "Reset & Rescan Selection").'), '',
        '3. ACTIVATE',
        ...so(V,
            '  Open the plugin in your DAW. On the activation screen, type the e-mail and token above and click ATIVAR (activate).',
            '  Internet is needed only at this moment. The same token activates every plugin in the pack.',
            '  Changing computers? Plugin menu ≡ > Licença... > Desativar este computador (deactivate), then activate on the new one.'),
        ...so(m4ls.length,
            `  ${nomesM4lEn}: on the device, click OPEN. The first time, the window asks for the e-mail and token above: type both`,
            '  and click ACTIVATE. Internet is needed only at this moment; after that it works offline for up to 45 days.'),
        '',
        ...so(V, '4. IF YOU SEE A SECURITY WARNING',
            '  The installers do not have a paid digital signature yet, so ' + (W && M ? 'Windows and macOS may warn you.' : W ? 'Windows may warn you.' : 'macOS may warn you.'),
            '  This is the standard warning for new unsigned software, it does not mean the file has a virus.'),
        ...so(V && W,
            '  Windows:',
            '  a) Blue screen "Windows protected your PC": click "More info" > "Run anyway".',
            '  b) Chrome/Edge blocked the download: downloads list > "..." of the file > Keep > Keep anyway.',
            '  c) Defender deleted the file: Windows Security > Virus & threat protection > Protection history >',
            '     installer item > Actions > Restore. Other antivirus: restore it from quarantine. No need to turn your antivirus off.'),
        ...so(V && M,
            '  macOS:',
            '  a) "Apple cannot check it" / "unidentified developer": OK > System Settings > Privacy & Security >',
            '     "Open Anyway" and confirm with your password. (macOS 14 or older: Control-click > Open.)',
            '  b) "Is damaged": in Terminal, run  xattr -dr com.apple.quarantine ~/Downloads/<file name>'), ...so(V, ''),
        ...so(manuais.length, 'PDF MANUAL (step-by-step guide to every control)', ...linksManual(pacote, true), ''),
        `  Questions or trouble? Reply to this e-mail or write to ${CONTATO}. Instagram: @equalizedmusic`, '',
        'InsideStudioSP',
    ];
}
