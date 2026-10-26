// Roteiro dos stories de 60 s da LINHA CRIATIVA (parte C): GRÃO, CALEIDO, ESPIRAL, FITA, CHAMA.
// tour: 5 trechos; a/b = [centro x, centro y, largura visível] em fração da foto; t1 título curto, t2 legenda.
// Ordem de leitura: cabeçalho + tela de cima, depois os painéis de baixo da esquerda para a direita.
window.TOURS = Object.assign(window.TOURS || {}, {
  GRAO: { linha: 'criativo', nome: 'GRÃO', tag: 'Granular de textura', sub: 'Areia, nuvem e eco em grãos', cor: '#ffb347',
    chips: ['Campo XY', 'Fita de 4 s', 'CONGELAR', 'REVERSO'],
    tour: [
      { a: [.22, .12, .42], b: [.78, .12, .42], t1: 'Fita 4 s', t2: 'Preset e grãos ao vivo na fita', c: '#ffb347' },
      { a: [.14, .68, .50], b: [.14, .68, .44], t1: 'Campo XY', t2: 'Um ponto mexe GRÃO e ESPAÇO', c: '#ff6b4a' },
      { a: [.41, .68, .50], b: [.41, .68, .44], t1: 'Grão', t2: 'Size · density · spray · pitch', c: '#ffb347' },
      { a: [.68, .68, .50], b: [.68, .68, .44], t1: 'Espaço', t2: 'Stereo, feedback, decay, tone', c: '#c49bff' },
      { a: [.90, .68, .50], b: [.90, .68, .44], t1: 'Congelar', t2: 'REVERSO, dry/wet, gain, IN/OUT', c: '#6ec3ff' },
    ] },
  CALEIDO: { linha: 'criativo', nome: 'CALEIDO', tag: 'Glitch rítmico', sub: 'Corta, repete e vira o loop', cor: '#c49bff',
    chips: ['Mandala', 'BPM sync', 'TRANSMUTAR', 'SEGURAR'],
    tour: [
      { a: [.22, .12, .42], b: [.78, .12, .42], t1: 'Padrão', t2: 'BPM sync e os cortes no loop', c: '#c49bff' },
      { a: [.14, .68, .50], b: [.14, .68, .44], t1: 'Mandala', t2: 'Gira no step e GERAR semente', c: '#8fd16a' },
      { a: [.41, .68, .50], b: [.41, .68, .44], t1: 'Caos', t2: 'Repete, embaralha e inverte', c: '#ffb347' },
      { a: [.68, .68, .50], b: [.68, .68, .44], t1: 'Viagem', t2: 'FITA, filter, crush e GIRO', c: '#6ec3ff' },
      { a: [.90, .68, .50], b: [.90, .68, .44], t1: 'Segurar', t2: 'TRANSMUTAR, dry/wet e gain', c: '#ff6b4a' },
    ] },
  ESPIRAL: { linha: 'criativo', nome: 'ESPIRAL', tag: 'Phaser · flanger infinito', sub: 'A subida que nunca termina', cor: '#6ec3ff',
    chips: ['Phaser/flanger', 'BPM sync', 'CONGELAR', 'DOBRAR'],
    tour: [
      { a: [.22, .12, .42], b: [.78, .12, .42], t1: 'Resposta', t2: 'BPM sync e a curva ao vivo', c: '#6ec3ff' },
      { a: [.14, .68, .50], b: [.14, .68, .44], t1: 'Espiral', t2: '4 voices girando sem parar', c: '#c49bff' },
      { a: [.41, .68, .50], b: [.41, .68, .44], t1: 'Range', t2: 'CICLO, DIREÇÃO, center, stereo', c: '#ffb347' },
      { a: [.68, .68, .50], b: [.68, .68, .44], t1: 'Resonance', t2: 'MODO, stages, drive e tone', c: '#ff6b4a' },
      { a: [.90, .68, .50], b: [.90, .68, .44], t1: 'Congelar', t2: 'DOBRAR, dry/wet, gain, IN/OUT', c: '#8fd16a' },
    ] },
  FITA: { linha: 'criativo', nome: 'FITA', tag: 'Gravador de fita', sub: 'Saturation, wow e tape stop', cor: '#ff6b4a',
    chips: ['VU L/R', 'Head bump', 'TAPE STOP', 'BPM sync'],
    tour: [
      { a: [.22, .12, .42], b: [.78, .12, .42], t1: 'Máquina', t2: 'VU, head bump e tape speed', c: '#ff6b4a' },
      { a: [.14, .68, .50], b: [.14, .68, .44], t1: 'Rolos', t2: 'Rolos girando e o contador', c: '#ffb347' },
      { a: [.41, .68, .50], b: [.41, .68, .44], t1: 'Saturation', t2: 'MODELO, speed, head bump, wear', c: '#8fd16a' },
      { a: [.68, .68, .50], b: [.68, .68, .44], t1: 'Wow', t2: 'Flutter, hiss, FREIO, PARTIDA', c: '#6ec3ff' },
      { a: [.90, .68, .50], b: [.90, .68, .44], t1: 'Tape stop', t2: 'Freia a fita; dry/wet e gain', c: '#c49bff' },
    ] },
  CHAMA: { linha: 'criativo', nome: 'CHAMA', tag: 'Build-up de um knob', sub: 'Um knob para a subida inteira', cor: '#ffb347',
    chips: ['Um knob', 'FOGUINHO', 'DROP', 'Gate sync'],
    tour: [
      { a: [.20, .06, .34], b: [.80, .06, .34], t1: 'Chama', t2: 'Preset e BPM sync no topo', c: '#ffb347' },
      { a: [.25, .24, .45], b: [.75, .24, .45], t1: 'Tensão', t2: 'Do low cut ao gate sync, ao vivo', c: '#ff6b4a' },
      { a: [.35, .66, .50], b: [.35, .66, .42], t1: 'Um knob', t2: 'Gira e o fogo cresce junto', c: '#ffb347' },
      { a: [.52, .78, .40], b: [.58, .82, .36], t1: 'Foguinho', t2: 'Dança no tempo com 4 LEDs', c: '#ff6b4a' },
      { a: [.85, .55, .40], b: [.85, .76, .40], t1: 'Drop', t2: 'Noise, ESPAÇO, gain e IN/OUT', c: '#6ec3ff' },
    ] },
});
