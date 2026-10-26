// Roteiro dos stories de 60 s da LINHA DE MIX — grupo A (PRISMA, TORNO, BARRAGEM, CONSOLE, SOCO).
// video.html ?p=plugin&n=<NOME>. tour: 5 trechos; a/b = [centro x, centro y, largura visível] em fração da foto.
// Ordem de leitura: 1) cabeçalho + tela de cima (pan esq→dir) · 2-4) painéis de baixo, da esquerda p/ a direita · 5) saída.
window.TOURS = Object.assign(window.TOURS || {}, {
  PRISMA: { linha: 'mix', nome: 'PRISMA', tag: 'Multibanda · 4 bandas', sub: 'Dinâmica + drive em 4 bandas', cor: '#c49bff',
    chips: ['4 bandas', 'Drive/banda', 'Solo · mute', 'Macro global'],
    tour: [
      { a: [.26, .30, .52], b: [.62, .30, .52], t1: 'Spectrum', t2: 'Arraste os cortes de banda', c: '#c49bff' },
      { a: [.21, .69, .44], b: [.22, .69, .41], t1: 'Bandas', t2: 'Solo, mute, threshold, ratio', c: '#ffb347' },
      { a: [.61, .69, .44], b: [.62, .69, .41], t1: 'Drive', t2: 'Saturação + attack/release', c: '#6ec3ff' },
      { a: [.90, .32, .36], b: [.90, .30, .33], t1: 'Macro', t2: 'Mais GR e drive em tudo', c: '#ff6b4a' },
      { a: [.90, .70, .36], b: [.90, .74, .34], t1: 'Output', t2: 'Gain, mix e medidor IN/OUT', c: '#f4e9d4' },
    ] },
  TORNO: { linha: 'mix', nome: 'TORNO', tag: 'Compressor', sub: 'Um knob de aperto · auto release', cor: '#ffb347',
    chips: ['Macro APERTO', 'Auto release', 'SC HPF', 'SC listen'],
    tour: [
      { a: [.25, .30, .50], b: [.75, .30, .50], t1: 'Histórico', t2: 'Input e GR dos últimos 4 s', c: '#ff6b4a' },
      { a: [.14, .59, .36], b: [.14, .78, .33], t1: 'Curva', t2: 'Threshold · ratio · knee', c: '#f4e9d4' },
      { a: [.41, .59, .36], b: [.41, .78, .33], t1: 'Aperto', t2: 'APERTO + ratio, knee, SC HPF', c: '#ff6b4a' },
      { a: [.68, .59, .36], b: [.68, .78, .33], t1: 'Envelope', t2: 'Attack, release e makeup', c: '#ffb347' },
      { a: [.90, .59, .36], b: [.90, .78, .33], t1: 'Output', t2: 'SC listen, gain, mix, medidor', c: '#6ec3ff' },
    ] },
  BARRAGEM: { linha: 'mix', nome: 'BARRAGEM', tag: 'Limiter true peak · LUFS', sub: 'Ceiling true peak + medidor LUFS', cor: '#6ec3ff',
    chips: ['True peak 4×', 'LUFS BS.1770', 'Lookahead', 'Delta'],
    tour: [
      { a: [.25, .30, .50], b: [.75, .30, .50], t1: 'Waveform', t2: 'O que sai e o que é segurado', c: '#6ec3ff' },
      { a: [.14, .59, .36], b: [.14, .78, .33], t1: 'Loudness', t2: 'Integrated, short-term, target', c: '#8fd16a' },
      { a: [.41, .59, .36], b: [.41, .78, .33], t1: 'Encher', t2: 'Macro, ceiling, lookahead', c: '#ff6b4a' },
      { a: [.68, .59, .36], b: [.68, .78, .33], t1: 'Análise', t2: 'GR agora, max e true peak', c: '#6ec3ff' },
      { a: [.90, .59, .36], b: [.90, .78, .33], t1: 'Output', t2: 'Delta, peak IN/OUT, medidor', c: '#ffb347' },
    ] },
  CONSOLE: { linha: 'mix', nome: 'CONSOLE', tag: 'Cristal · Ferro · Brasa', sub: 'Cor de canal com 4× oversampling', cor: '#ff6b4a',
    chips: ['Macro DRIVE', '3 modelos', 'OS 4×', 'THD ao vivo'],
    tour: [
      { a: [.25, .30, .50], b: [.75, .30, .50], t1: 'Mesa', t2: 'Sinal no canal e cor gerada', c: '#ff6b4a' },
      { a: [.14, .59, .36], b: [.14, .78, .33], t1: 'Curva', t2: 'A transferência do modelo', c: '#ffb347' },
      { a: [.41, .59, .36], b: [.41, .78, .33], t1: 'Drive', t2: 'Modelo · tone · crosstalk', c: '#ff6b4a' },
      { a: [.68, .59, .36], b: [.68, .78, .33], t1: 'Harmônicos', t2: 'THD, pares/ímpares, resposta', c: '#8fd16a' },
      { a: [.90, .59, .36], b: [.90, .78, .33], t1: 'Output', t2: 'Comparar, auto, OS 4×, gain', c: '#f4e9d4' },
    ] },
  SOCO: { linha: 'mix', nome: 'SOCO', tag: 'Modelador de transiente', sub: 'Attack e sustain sem threshold', cor: '#ff6b4a',
    chips: ['Macro ATTACK', 'Sustain', 'Detector', 'Clipper'],
    tour: [
      { a: [.25, .30, .50], b: [.75, .30, .50], t1: 'Waveform', t2: 'Antes e depois, ao vivo', c: '#ff6b4a' },
      { a: [.14, .59, .36], b: [.14, .78, .33], t1: 'Golpe', t2: 'Antes × depois do transiente', c: '#6ec3ff' },
      { a: [.41, .59, .36], b: [.41, .78, .33], t1: 'Attack', t2: 'Sustain, foco e medidores', c: '#ff6b4a' },
      { a: [.68, .59, .36], b: [.68, .78, .33], t1: 'Detector', t2: 'Faixa do detector e cutoff', c: '#c49bff' },
      { a: [.90, .59, .36], b: [.90, .78, .33], t1: 'Output', t2: 'Delta, clipper, gain, mix', c: '#ffb347' },
    ] },
});
