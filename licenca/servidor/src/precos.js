// Preço de cada pack à venda — fonte única: o servidor cobra por aqui e o site (site/dados.mjs) mostra daqui.
//   brl: reais no Mercado Pago (Pix, cartão até 12x, boleto); usd: dólares no PayPal (fora do Brasil).
//   null = sem preço ainda: o botão fica desligado no site e o servidor recusa a cobrança.
// ALIGN e BAREBASS são grátis (páginas próprias), por isso não estão aqui. VASSOURA e RODO (Max for Live) são pagos desde 14/11/2026.

export const PRECOS = {
    // usd fica null até o PayPal ser configurado e testado: o site mostra o botão PayPal só por haver usd.
    // Combinado (05/11/2026): 79,90 -> US$ 14.90 e 24,90 -> US$ 4.90.
    mesa:      { brl: 79.90, usd: null },
    criativo:  { brl: 79.90, usd: null },
    mix:       { brl: 79.90, usd: null },
    knob:      { brl: 79.90, usd: null },
    quimera:   { brl: 24.90, usd: null },
    limao:     { brl: 24.90, usd: null },
    mercurio:  { brl: 24.90, usd: null },
    resenha:   { brl: 24.90, usd: null },
    groovador: { brl: 24.90, usd: null },
    // STOMPBOX (08/11/2026): ia para a faixa da MESA; ele pediu preço de promoção como os demais.
    stompbox:  { brl: 24.90, usd: null },
    // MAESTRO (09/11/2026): faixa da MESA, R$ 79,90 "de" 149,90 (fechado por ele).
    maestro:   { brl: 79.90, usd: null },
    // MASTER DOS MAGOS (11/11/2026, aprovado no teste dele): R$ 149,90 SEM promoção (decisão dele, 10/11).
    magos:     { brl: 149.90, usd: null },
    // COMBO MAESTRO + MASTER DOS MAGOS: R$ 179,90 (abaixo da soma). Uma compra cria as DUAS licenças (COMBOS abaixo).
    maestromagos: { brl: 179.90, usd: null },
    // VASSOURA (era grátis) e RODO (14/11/2026, decisão dele): Max for Live, R$ 24,90 cada na oferta (depois 49,90),
    // combo dos dois R$ 39,90 na oferta (depois 79,90). Quem já tinha a VASSOURA ganhou a licença (migração 0.3.0).
    vassoura:  { brl: 24.90, usd: null },
    rodo:      { brl: 24.90, usd: null },
    vassourarodo: { brl: 39.90, usd: null },
};

// combo = um pedido que libera vários pacotes (loja.js concluir() cria uma licença para cada um)
export const COMBOS = { maestromagos: ['maestro', 'magos'], vassourarodo: ['vassoura', 'rodo'] };
// Completar o combo (12/11/2026, decisão dele): quem já comprou UM plugin do combo e compra o outro avulso (mesmo e-mail)
// paga só o que falta para fechar o preço do combo. O que já pagou = preço de tabela daquele plugin na data do pagamento,
// com o cupom daquele pedido descontado na mesma proporção (loja.js creditosCombo). Vale enquanto o combo existir.
export const COMBO_DE = Object.fromEntries(Object.entries(COMBOS).flatMap(([c, ps]) => ps.map((p) => [p, c])));

// Cupom de desconto (12/11/2026, decisão dele): % sobre o TOTAL do pedido (carrinho ou pack avulso), vale também
// sobre o preço de oferta e o combo. umaPorEmail: cada comprador (e-mail) usa uma vez só — conta pedido pago,
// em processamento ou devolvido. O cliente digita com ou sem "#", maiúscula ou minúscula. ate: opcional (ISO).
export const CUPONS = {
    atwork10: { pct: 10, umaPorEmail: true },
};
export const normCupom = (c) => String(c || '').trim().toLowerCase().replace(/^#+/, '').replace(/\s+/g, '').slice(0, 40);
export function cupomValido(c, agora = Date.now()) {
    const codigo = normCupom(c), x = CUPONS[codigo];
    return x && !(x.ate && agora > Date.parse(x.ate)) ? { codigo, ...x } : null;
}

// pedido do carrinho: os pacotes vão juntos em pedidos.pacote, separados por "+" (ex.: "limao+resenha+maestromagos")
export const itensPedido = (pacote) => String(pacote || '').split('+').filter(Boolean);
// pacotes que viram licença (o combo abre nos dele)
export const licencasPedido = (pacote) => itensPedido(pacote).flatMap((p) => COMBOS[p] || [p]);
export const nomePedido = (pacote) => itensPedido(pacote).map((p) => NOMES[p] || p.toUpperCase()).join(' + ');

export const NOMES = {
    mesa: 'MESA', criativo: 'SÉRIE CRIATIVO', mix: 'SÉRIE MIX', knob: 'SÉRIE KNOB', quimera: 'QUIMERA',
    limao: 'LIMÃO', mercurio: 'MERCÚRIO', resenha: 'RESENHA', groovador: 'GROOVADOR', stompbox: 'STOMPBOX', maestro: 'MAESTRO',
    magos: 'MASTER DOS MAGOS', maestromagos: 'MAESTRO + MASTER DOS MAGOS',
    vassoura: 'VASSOURA', rodo: 'RODO', vassourarodo: 'VASSOURA + RODO',
};

// Oferta de lançamento (decisão dele, 05/11/2026): só vitrine, o servidor cobra o PRECOS.brl.
// Preço "de" por faixa de preço real; pack sem entrada aqui mostra só o preço. Termina em OFERTA_FIM (Brasília).
// Usado pelo site (site/dados.mjs) e pela vitrine das páginas grátis (gratis.js). Para prorrogar: mudar a data e publicar os dois.
// 12/11/2026 (decisão dele): TODAS as ofertas terminam no fim da Black Friday (27/11/2026, sexta, 23:59 de Brasília).
export const OFERTA_FIM = '2026-11-27T23:59:59-03:00';
export const DE_BRL = { 79.9: 149.9, 24.9: 49.9, 39.9: 79.9 };   // 39,90 = combo VASSOURA + RODO
// Pack com oferta que termina em outra data (vazio hoje: todos acabam em OFERTA_FIM).
export const OFERTA_FIM_PACK = {};
export const fimOferta = (id) => OFERTA_FIM_PACK[id] || OFERTA_FIM;

// Preço que muda sozinho na data (decisão dele): acabada a oferta, cada pack em promoção passa a custar o preço "de"
// (79,90 -> 149,90; 24,90 -> 49,90; combo VASSOURA + RODO 39,90 -> 79,90). MAGOS (sem promoção) e o combo MAESTRO + MAGOS (179,90) não mudam.
// Cobrança (loja.js) e vitrine das páginas grátis leem precoAtual() NA HORA; o site estático sai com o preço da hora
// do build (rebuild agendado no dOne para 28/11 00:02). Para adiar: mudar OFERTA_FIM e publicar servidor e site.
export const PRECO_DEPOIS = Object.fromEntries(Object.entries(PRECOS)
    .filter(([, p]) => DE_BRL[p.brl])
    .map(([id, p]) => [id, { apos: fimOferta(id), brl: DE_BRL[p.brl] }]));
export function precoAtual(id, agora = Date.now()) {
    const p = PRECOS[id], d = PRECO_DEPOIS[id];
    return p && d && agora >= Date.parse(d.apos) ? { ...p, brl: d.brl } : p;
}
