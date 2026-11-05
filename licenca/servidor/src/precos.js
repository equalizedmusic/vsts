// Preço de cada pack à venda — fonte única: o servidor cobra por aqui e o site (site/dados.mjs) mostra daqui.
//   brl: reais no Mercado Pago (Pix, cartão até 12x, boleto); usd: dólares no PayPal (fora do Brasil).
//   null = sem preço ainda: o botão fica desligado no site e o servidor recusa a cobrança.
// ALIGN e VASSOURA são grátis (páginas próprias), por isso não estão aqui.

export const PRECOS = {
    mesa:      { brl: null, usd: null },
    criativo:  { brl: null, usd: null },
    mix:       { brl: null, usd: null },
    knob:      { brl: null, usd: null },
    quimera:   { brl: null, usd: null },
    limao:     { brl: null, usd: null },
    mercurio:  { brl: null, usd: null },
    resenha:   { brl: null, usd: null },
    groovador: { brl: null, usd: null },
};

export const NOMES = {
    mesa: 'MESA', criativo: 'SÉRIE CRIATIVO', mix: 'SÉRIE MIX', knob: 'SÉRIE KNOB', quimera: 'QUIMERA',
    limao: 'LIMÃO', mercurio: 'MERCÚRIO', resenha: 'RESENHA', groovador: 'GROOVADOR',
};
