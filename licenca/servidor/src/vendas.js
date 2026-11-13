// Aba "Vendas" do painel: os pedidos da loja (src/loja.js) e o total do mês.
//
// Painel (sessão + x-painel, ver painel.js):
//   GET  /painel/vendas?m=AAAA-MM     pedidos do mês (pelo dia do pagamento; sem pagamento, pelo dia do pedido) + resumo + meses com pedido
//   GET  /painel/vendas/conta         e-mail/apelido da conta do Mercado Pago que recebe (GET /users/me com a chave)
//   POST /painel/vendas/devolvido { id }   marca a venda paga como devolvida e cancela a licença dela.
//                                          NÃO devolve o dinheiro: isso é feito no Mercado Pago (Atividade > Devolver dinheiro).
// O mês é o de São Paulo (UTC-3, sem horário de verão). 'devolvido' não volta a 'pago': as consultas da loja só mexem
// em pedido criado/pendente/recusado/vencido/processando.

import { nomePedido, itensPedido } from './precos.js';

const agora = () => Math.floor(Date.now() / 1000);
const FUSO = 3 * 3600;   // São Paulo = UTC-3
const MAX_CONSULTAS = 40;          // consultas ao Mercado Pago por abertura da aba (limite de subrequests do Worker)
const CACHE_PAGAMENTO = 6 * 3600;  // taxa/líquido de um pagamento quase não muda: guarda 6 h no cache do Worker

// taxa e líquido de um pagamento do Mercado Pago (GET /v1/payments/{id}); null se não deu para ler
async function pagamentoMp(env, id) {
    if (!env.MP_ACCESS_TOKEN || !/^\d{1,20}$/.test(String(id))) return null;
    const chave = new Request('https://cache.interno/mp-pagamento/' + id);
    const cache = typeof caches !== 'undefined' ? caches.default : null;
    if (cache) { const c = await cache.match(chave); if (c) return c.json(); }
    let d = null;
    try {
        const r = await fetch('https://api.mercadopago.com/v1/payments/' + id, { headers: { authorization: 'Bearer ' + env.MP_ACCESS_TOKEN } });
        if (!r.ok) return null;
        const p = await r.json();
        const td = p.transaction_details || {};
        const bruto = Math.round(Number(p.transaction_amount || 0) * 100);
        const liquido = td.net_received_amount != null ? Math.round(Number(td.net_received_amount) * 100) : null;
        if (liquido == null) return null;
        d = { liquido, taxa: Math.max(0, bruto - liquido), libera_em: p.money_release_date ? Math.floor(Date.parse(p.money_release_date) / 1000) : null };
    } catch { return null; }
    if (cache) await cache.put(chave, new Response(JSON.stringify(d), { headers: { 'cache-control': 'max-age=' + CACHE_PAGAMENTO } }));
    return d;
}

// saldo da conta do Mercado Pago (disponível / a liberar); null se a API não liberar
async function saldoMp(env, userId) {
    try {
        const r = await fetch(`https://api.mercadopago.com/users/${userId}/mercadopago_account/balance`, { headers: { authorization: 'Bearer ' + env.MP_ACCESS_TOKEN } });
        if (!r.ok) return { erro: 'Mercado Pago respondeu ' + r.status };
        const s = await r.json();
        const c = (v) => (v == null ? null : Math.round(Number(v) * 100));
        return { disponivel: c(s.available_balance), a_liberar: c(s.unavailable_balance), total: c(s.total_amount), moeda: s.currency_id || 'BRL' };
    } catch (e) { return { erro: String(e.message || e) }; }
}

function json(obj, status = 200) {
    return new Response(JSON.stringify(obj), { status, headers: { 'content-type': 'application/json; charset=utf-8', 'cache-control': 'no-store' } });
}
const erro = (mensagem, status) => json({ ok: false, mensagem }, status);

function mesAtual() {
    const d = new Date((agora() - FUSO) * 1000);
    return `${d.getUTCFullYear()}-${String(d.getUTCMonth() + 1).padStart(2, '0')}`;
}
// "2026-11" -> [início, fim) em segundos UTC
function intervalo(mes) {
    const [a, m] = mes.split('-').map(Number);
    return [Date.UTC(a, m - 1, 1) / 1000 + FUSO, Date.UTC(a, m, 1) / 1000 + FUSO];
}

export async function rotaVendas(caminho, req, env) {
    if (caminho === '/painel/vendas' && req.method === 'GET') {
        const url = new URL(req.url);
        let mes = url.searchParams.get('m') || mesAtual();
        if (!/^\d{4}-(0[1-9]|1[0-2])$/.test(mes)) mes = mesAtual();
        const [ini, fim] = intervalo(mes);
        const { results: pedidos } = await env.DB.prepare(
            'SELECT id, pacote, nome, email, metodo, valor_centavos, moeda, status, forma, cupom, desconto_centavos, email_enviado, licenca_id, criado_em, pago_em, pagamento_id ' +
            'FROM pedidos WHERE COALESCE(pago_em, criado_em) >= ? AND COALESCE(pago_em, criado_em) < ? ORDER BY COALESCE(pago_em, criado_em) DESC')
            .bind(ini, fim).all();
        const { results: meses } = await env.DB.prepare(
            "SELECT DISTINCT strftime('%Y-%m', COALESCE(pago_em, criado_em) - ?, 'unixepoch') AS mes FROM pedidos ORDER BY mes DESC")
            .bind(FUSO).all();
        // taxa e líquido de cada venda paga no Mercado Pago (o que de fato cai na conta)
        await Promise.all(pedidos.filter((p) => p.metodo === 'mp' && p.pagamento_id && (p.status === 'pago' || p.status === 'processando'))
            .slice(0, MAX_CONSULTAS).map(async (p) => {
                const d = await pagamentoMp(env, p.pagamento_id);
                if (d) { p.liquido_centavos = d.liquido; p.taxa_centavos = d.taxa; p.libera_em = d.libera_em; }
            }));
        const resumo = {};   // por moeda: { pagas, total, liquido, taxas, sem_liquido, devolvidas, devolvido, cupons, desconto, carrinhos }
        let abertos = 0;
        for (const p of pedidos) {
            p.itens = itensPedido(p.pacote).length;
            p.pacote_nome = nomePedido(p.pacote);
            delete p.pagamento_id;
            const r = resumo[p.moeda] || (resumo[p.moeda] = { pagas: 0, total: 0, liquido: 0, taxas: 0, sem_liquido: 0, devolvidas: 0, devolvido: 0,
                                                                cupons: 0, desconto: 0, carrinhos: 0 });
            if (p.status === 'pago' || p.status === 'processando') {
                r.pagas++; r.total += p.valor_centavos;
                if (p.liquido_centavos != null) { r.liquido += p.liquido_centavos; r.taxas += p.taxa_centavos; } else r.sem_liquido++;
                if (p.cupom) { r.cupons++; r.desconto += p.desconto_centavos || 0; }
                if (p.itens > 1) r.carrinhos++;
            }
            else if (p.status === 'devolvido') { r.devolvidas++; r.devolvido += p.valor_centavos; }
            else abertos++;
        }
        // desde a primeira venda (todas as datas)
        const { results: geral } = await env.DB.prepare(
            "SELECT moeda, COUNT(*) AS pagas, SUM(valor_centavos) AS total FROM pedidos WHERE status IN ('pago', 'processando') GROUP BY moeda").all();
        const lista = meses.map((m) => m.mes);
        if (!lista.includes(mesAtual())) lista.unshift(mesAtual());
        if (!lista.includes(mes)) lista.push(mes);
        return json({ ok: true, mes, meses: lista, pedidos, resumo, abertos, geral });
    }

    // de quem é a conta do Mercado Pago que recebe (a chave fica só no segredo do Worker)
    if (caminho === '/painel/vendas/conta' && req.method === 'GET') {
        if (!env.MP_ACCESS_TOKEN) return json({ ok: true, conta: null });
        const r = await fetch('https://api.mercadopago.com/users/me', { headers: { authorization: 'Bearer ' + env.MP_ACCESS_TOKEN } });
        if (!r.ok) return json({ ok: true, conta: null, erro: 'Mercado Pago respondeu ' + r.status });
        const u = await r.json();
        let saldo = u.id ? await saldoMp(env, u.id) : null;
        if (!saldo || saldo.erro) {
            // a API de saldo é fechada para esta conta (403): soma o líquido das vendas da loja, liberado ou a liberar.
            // Não é o saldo da conta: não conta saques, devoluções feitas lá nem dinheiro de fora da loja.
            const { results: pagos } = await env.DB.prepare(
                "SELECT pagamento_id FROM pedidos WHERE metodo = 'mp' AND status IN ('pago', 'processando') AND pagamento_id IS NOT NULL ORDER BY pago_em DESC LIMIT ?")
                .bind(MAX_CONSULTAS).all();
            let liberado = 0, aLiberar = 0, proxima = null, lidos = 0;
            for (const d of await Promise.all(pagos.map((p) => pagamentoMp(env, p.pagamento_id)))) {
                if (!d) continue;
                lidos++;
                if (d.libera_em && d.libera_em > agora()) { aLiberar += d.liquido; if (!proxima || d.libera_em < proxima) proxima = d.libera_em; }
                else liberado += d.liquido;
            }
            saldo = { estimado: true, liberado, a_liberar: aLiberar, proxima, lidos, total_vendas: pagos.length, moeda: 'BRL', erro_api: saldo && saldo.erro };
        }
        return json({ ok: true, saldo, conta: { email: u.email || null, apelido: u.nickname || null, nome: [u.first_name, u.last_name].filter(Boolean).join(' ') || null,
                                         id: u.id || null, teste: String(env.MP_ACCESS_TOKEN).startsWith('TEST-') } });
    }

    if (caminho === '/painel/vendas/devolvido' && req.method === 'POST') {
        let corpo;
        try { corpo = await req.json(); } catch { return erro('Pedido inválido.', 400); }
        const id = String(corpo.id || '');
        if (!/^P[0-9A-F]{24}$/.test(id)) return erro('Pedido inválido.', 400);
        const p = await env.DB.prepare('SELECT id, status, licenca_id, email, pacote FROM pedidos WHERE id = ?').bind(id).first();
        if (!p) return erro('Pedido não encontrado.', 404);
        if (p.status !== 'pago') return erro('Só dá para marcar como devolvida uma venda paga.', 409);
        await env.DB.prepare("UPDATE pedidos SET status = 'devolvido' WHERE id = ? AND status = 'pago'").bind(id).run();
        let cancelou = false;
        if (p.licenca_id) {
            // carrinho/combo: cancela todas as licenças criadas por este pedido (licencas.pedido), não só a 1ª
            const r = await env.DB.prepare("UPDATE licencas SET status = 'revogada', revogada_em = ? WHERE (id = ? OR pedido = ?) AND status = 'ativa'")
                .bind(agora(), p.licenca_id, id).run();
            cancelou = !!r.meta.changes;
        }
        await env.DB.prepare('INSERT INTO eventos (licenca_id, tipo, detalhe, ip, quando) VALUES (?, ?, ?, ?, ?)')
            .bind(p.licenca_id || null, 'devolucao', `${id} ${p.email} ${p.pacote}${cancelou ? ' (licença cancelada)' : ''}`,
                  req.headers.get('cf-connecting-ip') || null, agora()).run();
        return json({ ok: true, cancelou });
    }
    return erro('Não encontrado.', 404);
}
