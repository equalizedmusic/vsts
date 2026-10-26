// Roteiro dos stories de 60 s da LINHA DE MIX, lote B (video.html ?p=plugin&n=<NOME>).
// tour: 5 trechos; a/b = [centro x, centro y, largura visível] em fração da foto; t1 título curto, t2 legenda.
// Nomes de controles conferidos no PluginEditor.cpp atual (a foto em img/ ainda mostra os nomes antigos).
window.TOURS = Object.assign(window.TOURS || {}, {
  LIXA: { linha: 'mix', nome: 'LIXA', tag: 'De-esser', sub: 'De-esser de faixa ajustável', cor: '#ffb347',
    chips: ['Macro RANGE', 'Spectrum', 'Wideband', 'Só o corte'],
    tour: [
      { a: [.20, .20, .38], b: [.80, .20, .38], t1: 'Lixa', t2: 'De-esser · presets · latency', c: '#f4e9d4' },
      { a: [.28, .30, .50], b: [.74, .30, .50], t1: 'Spectrum', t2: 'Arraste as bordas da faixa', c: '#ffb347' },
      { a: [.12, .72, .42], b: [.22, .72, .42], t1: 'Faixa', t2: 'Low freq · high freq · wideband', c: '#ffb347' },
      { a: [.38, .72, .42], b: [.48, .72, .42], t1: 'Range', t2: 'Threshold · attack · release', c: '#ff6b4a' },
      { a: [.66, .72, .42], b: [.86, .72, .42], t1: 'Análise', t2: 'Gain reduction · output', c: '#6ec3ff' },
    ] },

  HORIZONTE: { linha: 'mix', nome: 'HORIZONTE', tag: 'Imagem estéreo', sub: 'Width M/S com latency zero', cor: '#6ec3ff',
    chips: ['Macro WIDTH', 'Crossover', 'Mono bass', 'M/S'],
    tour: [
      { a: [.20, .20, .38], b: [.80, .20, .38], t1: 'Horizonte', t2: 'Stereo image · zero latency', c: '#f4e9d4' },
      { a: [.16, .35, .50], b: [.16, .75, .50], t1: 'Vectorscope', t2: 'Correlation · balance L/R', c: '#6ec3ff' },
      { a: [.52, .26, .44], b: [.80, .26, .44], t1: 'Por banda', t2: 'Width por frequência ao vivo', c: '#c49bff' },
      { a: [.45, .70, .44], b: [.52, .70, .44], t1: 'Width', t2: 'Low · crossover · high', c: '#c49bff' },
      { a: [.72, .70, .44], b: [.84, .70, .44], t1: 'Mono bass', t2: 'Balance · rotation · output', c: '#8fd16a' },
    ] },

  SALA: { linha: 'mix', nome: 'SALA', tag: 'Reverb limpo', sub: 'Reverb de mix, limpo', cor: '#c49bff',
    chips: ['Pre-delay sync', 'Tail EQ', 'Duck', 'Só a sala'],
    tour: [
      { a: [.20, .20, .38], b: [.80, .20, .38], t1: 'Sala', t2: 'Pre-delay no tempo · presets', c: '#f4e9d4' },
      { a: [.28, .28, .50], b: [.74, .28, .50], t1: 'Resposta', t2: 'Early reflections e tail', c: '#c49bff' },
      { a: [.13, .685, .44], b: [.20, .685, .44], t1: 'Tail EQ', t2: 'Curva da cauda · width', c: '#ff6b4a' },
      { a: [.38, .685, .44], b: [.45, .685, .44], t1: 'Espaço', t2: 'Size · decay · pre-delay', c: '#ffb347' },
      { a: [.70, .685, .44], b: [.80, .685, .44], t1: 'Tail', t2: 'Damping · duck · dry/wet', c: '#8fd16a' },
    ] },

  MONITOR: { linha: 'mix', nome: 'MONITOR', tag: 'Medidor de pico e LUFS', sub: 'LUFS e true peak, zero latency', cor: '#8fd16a',
    chips: ['LUFS', 'True peak', 'Match target', 'Histórico 60 s'],
    tour: [
      { a: [.20, .20, .38], b: [.80, .20, .38], t1: 'Monitor', t2: 'Medidor LUFS e true peak', c: '#f4e9d4' },
      { a: [.20, .32, .42], b: [.55, .32, .42], t1: 'Loudness', t2: 'Integrated · short-term · LRA', c: '#8fd16a' },
      { a: [.22, .76, .42], b: [.52, .76, .42], t1: 'Spectrum', t2: 'Ao vivo, média e correlation', c: '#ffb347' },
      { a: [.74, .30, .42], b: [.84, .30, .42], t1: 'Target', t2: 'Target · TP ceil · LRA max', c: '#c49bff' },
      { a: [.68, .76, .42], b: [.84, .76, .42], t1: 'Checagem', t2: 'Level · mono · match target', c: '#6ec3ff' },
    ] },

  NAVALHA: { linha: 'mix', nome: 'NAVALHA', tag: 'Clipper ao vivo', sub: 'Clipper com 4× oversampling', cor: '#ff6b4a',
    chips: ['Macro DRIVE', 'Ceiling', 'Waveform', 'Só o corte'],
    tour: [
      { a: [.20, .20, .38], b: [.80, .20, .38], t1: 'Navalha', t2: 'Clipper · 4× oversampling', c: '#f4e9d4' },
      { a: [.28, .28, .50], b: [.74, .28, .50], t1: 'Waveform', t2: 'O que passa e o que corta', c: '#ff6b4a' },
      { a: [.13, .685, .44], b: [.20, .685, .44], t1: 'Curva', t2: 'Input × output ao vivo', c: '#ffb347' },
      { a: [.38, .685, .44], b: [.45, .685, .44], t1: 'Drive', t2: 'Ceiling · knee · asymmetry', c: '#ff6b4a' },
      { a: [.70, .685, .44], b: [.80, .685, .44], t1: 'Análise', t2: 'Peak ao vivo · gain · dry/wet', c: '#8fd16a' },
    ] },
});
