// Aba "Vendas" do painel: os pedidos da loja (src/loja.js) e o total do mês.
//
// Painel (sessão + x-painel, ver painel.js):
//   GET  /painel/vendas?m=AAAA-MM     pedidos do mês (pelo dia do pagamento; sem pagamento, pelo dia do pedido) + resumo + meses com pedido
//   GET  /painel/vendas/conta         e-mail/apelido da conta do Mercado Pago que recebe (GET /users/me com a chave)
//   POST /painel/vendas/devolvido { id }   marca a venda paga como devolvida e cancela a licença dela.
//                                          NÃO devolve o dinheiro: isso é feito no Mercado Pago (Atividade > Devolver dinheiro).
// O mês é o de São Paulo (UTC-3, sem horário de verão). 'devolvido' não volta a 'pago': as consultas da loja só mexem
// em pedido criado/pendente/recusado/vencido/processando.

import { NOMES } from './precos.js';

const agora = () => Math.floor(Date.now() / 1000);
const FUSO = 3 * 3600;   // São Paulo = UTC-3

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
            'SELECT id, pacote, nome, email, metodo, valor_centavos, moeda, status, forma, email_enviado, licenca_id, criado_em, pago_em ' +
            'FROM pedidos WHERE COALESCE(pago_em, criado_em) >= ? AND COALESCE(pago_em, criado_em) < ? ORDER BY COALESCE(pago_em, criado_em) DESC')
            .bind(ini, fim).all();
        const { results: meses } = await env.DB.prepare(
            "SELECT DISTINCT strftime('%Y-%m', COALESCE(pago_em, criado_em) - ?, 'unixepoch') AS mes FROM pedidos ORDER BY mes DESC")
            .bind(FUSO).all();
        const resumo = {};   // por moeda: { pagas, total, devolvidas, devolvido }
        let abertos = 0;
        for (const p of pedidos) {
            p.pacote_nome = NOMES[p.pacote] || p.pacote;
            const r = resumo[p.moeda] || (resumo[p.moeda] = { pagas: 0, total: 0, devolvidas: 0, devolvido: 0 });
            if (p.status === 'pago' || p.status === 'processando') { r.pagas++; r.total += p.valor_centavos; }
            else if (p.status === 'devolvido') { r.devolvidas++; r.devolvido += p.valor_centavos; }
            else abertos++;
        }
        const lista = meses.map((m) => m.mes);
        if (!lista.includes(mesAtual())) lista.unshift(mesAtual());
        if (!lista.includes(mes)) lista.push(mes);
        return json({ ok: true, mes, meses: lista, pedidos, resumo, abertos });
    }

    // de quem é a conta do Mercado Pago que recebe (a chave fica só no segredo do Worker)
    if (caminho === '/painel/vendas/conta' && req.method === 'GET') {
        if (!env.MP_ACCESS_TOKEN) return json({ ok: true, conta: null });
        const r = await fetch('https://api.mercadopago.com/users/me', { headers: { authorization: 'Bearer ' + env.MP_ACCESS_TOKEN } });
        if (!r.ok) return json({ ok: true, conta: null, erro: 'Mercado Pago respondeu ' + r.status });
        const u = await r.json();
        return json({ ok: true, conta: { email: u.email || null, apelido: u.nickname || null, nome: [u.first_name, u.last_name].filter(Boolean).join(' ') || null,
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
            const r = await env.DB.prepare("UPDATE licencas SET status = 'revogada', revogada_em = ? WHERE id = ? AND status = 'ativa'")
                .bind(agora(), p.licenca_id).run();
            cancelou = !!r.meta.changes;
        }
        await env.DB.prepare('INSERT INTO eventos (licenca_id, tipo, detalhe, ip, quando) VALUES (?, ?, ?, ?, ?)')
            .bind(p.licenca_id || null, 'devolucao', `${id} ${p.email} ${p.pacote}${cancelou ? ' (licença cancelada)' : ''}`,
                  req.headers.get('cf-connecting-ip') || null, agora()).run();
        return json({ ok: true, cancelou });
    }
    return erro('Não encontrado.', 404);
}
