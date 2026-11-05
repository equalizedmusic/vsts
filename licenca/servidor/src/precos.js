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
};

export const NOMES = {
    mesa: 'MESA', criativo: 'SÉRIE CRIATIVO', mix: 'SÉRIE MIX', knob: 'SÉRIE KNOB', quimera: 'QUIMERA',
    limao: 'LIMÃO', mercurio: 'MERCÚRIO', resenha: 'RESENHA', groovador: 'GROOVADOR',
};
