// Preço de cada pack à venda — fonte única: o servidor cobra por aqui e o site (site/dados.mjs) mostra daqui.
//   brl: reais no Mercado Pago (Pix, cartão até 12x, boleto); usd: dólares no PayPal (fora do Brasil).
//   null = sem preço ainda: o botão fica desligado no site e o servidor recusa a cobrança.
// ALIGN e VASSOURA são grátis (páginas próprias), por isso não estão aqui.

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
};

export const NOMES = {
    mesa: 'MESA', criativo: 'SÉRIE CRIATIVO', mix: 'SÉRIE MIX', knob: 'SÉRIE KNOB', quimera: 'QUIMERA',
    limao: 'LIMÃO', mercurio: 'MERCÚRIO', resenha: 'RESENHA', groovador: 'GROOVADOR', stompbox: 'STOMPBOX', maestro: 'MAESTRO',
};

// Oferta de lançamento (decisão dele, 05/11/2026): só vitrine, o servidor cobra o PRECOS.brl.
// Preço "de" por faixa de preço real; pack sem entrada aqui mostra só o preço. Termina em OFERTA_FIM (Brasília).
// Usado pelo site (site/dados.mjs) e pela vitrine das páginas grátis (gratis.js). Para prorrogar: mudar a data e publicar os dois.
export const OFERTA_FIM = '2026-12-31T23:59:59-03:00';
export const DE_BRL = { 79.9: 149.9, 24.9: 49.9 };
// Pack com oferta que termina antes da geral (decisão dele): MAESTRO só até a Black Friday (27/11/2026, sexta).
// Passada a data, some o "de" riscado do pack e fica só o preço do PRECOS.
export const OFERTA_FIM_PACK = { maestro: '2026-11-27T23:59:59-03:00' };
export const fimOferta = (id) => OFERTA_FIM_PACK[id] || OFERTA_FIM;
