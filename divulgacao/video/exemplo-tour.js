// Roteiro dos stories de 60 s da LINHA DE MIX (video.html ?p=plugin&n=<NOME>).
// tour: 5 trechos; a/b = [centro x, centro y, largura visível] em fração da foto; t1 título curto, t2 legenda.
window.TOURS = Object.assign(window.TOURS || {}, {
  TORNO: { linha: 'mix', nome: 'TORNO', tag: 'Compressor', sub: 'Aperto de um knob · auto release', cor: '#ffb347',
    chips: ['Macro APERTO', 'Auto release', 'Sidechain HPF', 'Listen GR'],
    tour: [
      { a: [.20, .25, .40], b: [.45, .25, .40], t1: 'Aperto', t2: 'Um knob que comprime tudo', c: '#ffb347' },
      { a: [.55, .45, .45], b: [.80, .45, .45], t1: 'Threshold', t2: 'Ratio · knee · attack', c: '#ff6b4a' },
      { a: [.25, .65, .40], b: [.50, .65, .40], t1: 'Release', t2: 'Auto release que respira', c: '#c49bff' },
      { a: [.70, .70, .40], b: [.85, .75, .40], t1: 'Sidechain', t2: 'HPF no detector', c: '#6ec3ff' },
      { a: [.30, .85, .45], b: [.75, .85, .45], t1: 'Gain', t2: 'Makeup automático', c: '#8fd16a' },
    ] },
});
