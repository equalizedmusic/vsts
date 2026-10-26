// Roteiro dos stories de 60 s (video.html ?p=plugin&n=<NOME>) — F1, RINGER, REPETECO, MIRAGEM, PICOTE, QUIMERA.
// tour: 5 trechos; a/b = [centro x, centro y, largura visível] em fração da foto; t1 título curto, t2 legenda.
// Linha criativa (2000×1160): cabeçalho+tela de cima → painel visual da esquerda → 01 → 02 → 03 OUTPUT.
window.TOURS = Object.assign(window.TOURS || {}, {
  F1: { linha: 'criativo', nome: 'F1', tag: 'Filtro ladder 4 polos', sub: 'Ladder clássico · envelope', cor: '#8fd16a',
    chips: ['4 polos', 'Envelope', 'LFO sync', 'Drive'],
    tour: [
      { a: [.20, .20, .42], b: [.80, .20, .42], t1: 'Resposta', t2: 'Curva do cutoff ao vivo', c: '#8fd16a' },
      { a: [.15, .56, .36], b: [.15, .77, .36], t1: 'Escada', t2: 'Os 4 polos do ladder', c: '#ffb347' },
      { a: [.41, .56, .36], b: [.41, .77, .36], t1: 'Cutoff', t2: 'Resonance · poles · drive', c: '#ff6b4a' },
      { a: [.68, .56, .36], b: [.68, .77, .36], t1: 'Envelope', t2: 'Wah automático com LFO', c: '#8fd16a' },
      { a: [.90, .55, .32], b: [.90, .78, .32], t1: 'Output', t2: 'ABRIR · FUMAÇA · mix · gain', c: '#f4e9d4' },
    ] },
  RINGER: { linha: 'criativo', nome: 'RINGER', tag: 'Ring modulator', sub: 'Sino, robô e sirene', cor: '#ff6b4a',
    chips: ['Carrier', 'LFO sync', 'Lissajous', 'Stereo'],
    tour: [
      { a: [.20, .20, .42], b: [.80, .20, .42], t1: 'Oscilloscope', t2: 'Input × carrier ao vivo', c: '#ff6b4a' },
      { a: [.15, .56, .36], b: [.15, .77, .36], t1: 'Lissajous', t2: 'O anel desenha o som', c: '#ffb347' },
      { a: [.41, .56, .36], b: [.41, .77, .36], t1: 'Carrier', t2: 'Frequency · range · stereo', c: '#ff6b4a' },
      { a: [.68, .56, .36], b: [.68, .77, .36], t1: 'LFO', t2: 'Rate · shape · sync · phase', c: '#c49bff' },
      { a: [.90, .55, .32], b: [.90, .78, .32], t1: 'Output', t2: 'DOBRAR · mix · gain', c: '#f4e9d4' },
    ] },
  REPETECO: { linha: 'criativo', nome: 'REPETECO', tag: 'Delay analógico BBD', sub: 'Eco escuro que se desfaz', cor: '#ffb347',
    chips: ['BBD 1024', 'Sync no grid', 'Ping-pong', 'Infinito'],
    tour: [
      { a: [.20, .20, .42], b: [.80, .20, .42], t1: 'Repetições', t2: 'Cada eco escurece na volta', c: '#ffb347' },
      { a: [.15, .56, .36], b: [.15, .77, .36], t1: 'Baldes', t2: '1024 estágios BBD', c: '#ff6b4a' },
      { a: [.41, .56, .36], b: [.41, .77, .36], t1: 'Time', t2: 'No grid · range · ping-pong', c: '#ffb347' },
      { a: [.68, .56, .36], b: [.68, .77, .36], t1: 'Feedback', t2: 'Passa de 100%: auto-oscila', c: '#ff6b4a' },
      { a: [.90, .55, .32], b: [.90, .78, .32], t1: 'Output', t2: 'INFINITO · MATAR · mix', c: '#f4e9d4' },
    ] },
  MIRAGEM: { linha: 'criativo', nome: 'MIRAGEM', tag: 'Reverb shimmer', sub: 'Cauda que se desfaz', cor: '#6ec3ff',
    chips: ['Shimmer', 'Congelar', 'Reverso', 'Duck'],
    tour: [
      { a: [.20, .20, .42], b: [.80, .20, .42], t1: 'Miragem', t2: 'A cauda vira paisagem', c: '#6ec3ff' },
      { a: [.15, .56, .36], b: [.15, .77, .36], t1: 'Tail', t2: 'Decay em dB × tempo · tone', c: '#ffb347' },
      { a: [.41, .56, .36], b: [.41, .77, .36], t1: 'Decay', t2: 'Size · pre-delay · damping', c: '#ffb347' },
      { a: [.68, .56, .36], b: [.68, .77, .36], t1: 'Shimmer', t2: 'Interval · modulation · duck', c: '#c49bff' },
      { a: [.90, .55, .32], b: [.90, .78, .32], t1: 'Output', t2: 'CONGELAR · REVERSO · mix', c: '#6ec3ff' },
    ] },
  PICOTE: { linha: 'criativo', nome: 'PICOTE', tag: 'Gate de 16 passos', sub: 'Volume, pan e filter no grid', cor: '#c49bff',
    chips: ['16 passos', 'Swing', 'Embaralhar', 'Prender'],
    tour: [
      { a: [.20, .20, .42], b: [.80, .20, .42], t1: 'Steps', t2: '16 passos · volume pan filter', c: '#c49bff' },
      { a: [.15, .56, .36], b: [.15, .77, .36], t1: 'Fatia', t2: 'O passo atual em detalhe', c: '#ffb347' },
      { a: [.41, .56, .36], b: [.41, .77, .36], t1: 'Depth', t2: 'Divisão · steps · swing', c: '#c49bff' },
      { a: [.68, .56, .36], b: [.68, .77, .36], t1: 'Width', t2: 'Attack · release · filter', c: '#ff6b4a' },
      { a: [.90, .55, .32], b: [.90, .78, .32], t1: 'Output', t2: 'EMBARALHAR · PRENDER · mix', c: '#f4e9d4' },
    ] },
  QUIMERA: { linha: 'synth', w: 2400, h: 1480, nome: 'QUIMERA', tag: 'Sintetizador wavetable', sub: 'Orgânico + metálico · MPE', cor: '#ffb347',
    chips: ['360 presets', '9 categorias', '8 vozes', 'MPE'],
    tour: [
      { a: [.22, .28, .46], b: [.50, .28, .46], t1: 'Wavetable', t2: '3 OSC · 360 presets no banco', c: '#ffb347' },
      { a: [.62, .27, .42], b: [.80, .27, .42], t1: 'Sub · noise', t2: 'Voice: glide · drift · bend', c: '#f4e9d4' },
      { a: [.20, .57, .34], b: [.82, .57, .34], t1: 'Filters', t2: 'Filter A e B · 4 macros', c: '#ff6b4a' },
      { a: [.18, .74, .34], b: [.84, .74, .34], t1: 'Envelopes', t2: '3 ENV · 2 LFO com sync', c: '#8fd16a' },
      { a: [.18, .90, .30], b: [.86, .90, .30], t1: 'MPE', t2: 'Teclado com expressão por nota', c: '#c49bff' },
    ] },
});
