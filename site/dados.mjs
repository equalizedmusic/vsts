// Conteúdo do site: packs, plugins e textos. Cada texto vem em [pt, en] (PT primeiro: o site abre em português).
// preco: vem de licenca/servidor/src/precos.js (o mesmo que o servidor cobra) — { brl, usd } ou null = selo "EM BREVE". gratis: link da página grátis
// (direto: true = o link já é o download, sem cadastro nem token — caso da VASSOURA, que não ativa).

import { PRECOS } from '../licenca/servidor/src/precos.js';

// servidor de licenças no domínio do estúdio: as rotas /loja/*, /align*, /baixar/* (licenca/servidor/wrangler.toml)
// valem com e sem www, então os links são relativos (o formulário de compra fica no mesmo endereço da página: form-action 'self')
export const LIC = '';

export const SITE = {
    nome: 'InsideStudioSP',
    instagram: 'https://instagram.com/equalizedmusic',
    email: 'contato@insidestudiosp.com.br',
    // Links do artista tirados do linktr.ee/equalizedmusic (05/11/2026), sem os parâmetros de rastreio.
    spotify: 'https://open.spotify.com/artist/5nIL632exR3FPgulSLr2IO',
    youtube: '',          // por ora fora, a pedido dele (05/11). Quando entrar: id de um vídeo para o player
    lancamento: { titulo: "Summer of '89", url: 'https://bluetunesrecords.lnk.to/summerof89' },
    // Página Estúdio (05/11): texto do estúdio + bio tirada do release dele (Google Docs). Cada parágrafo = [pt, en].
    estudio: {
        lead: ['Estúdio de produção, mix e master em São Paulo, comandado pelo DJ e produtor Rogério Cardoso, o Equalized.',
               'Production, mixing and mastering studio in São Paulo, Brazil, run by DJ and producer Rogério Cardoso, aka Equalized.'],
        texto: [
            ['É aqui que nascem as faixas do Equalized e os plugins da loja. Sala tratada, monitores de referência, sintetizador analógico, controladores e CDJs ficam no mesmo ambiente: dá para compor, mixar e testar a faixa como na pista, sem sair da sala.',
             'This is where the Equalized tracks and the plugins in the shop are made. Treated room, reference monitors, analog synth, controllers and CDJs all in one place: you can write, mix and test the track the way it plays on the dancefloor, without leaving the room.'],
            ['Produção, mix e master de música eletrônica, do Psytrance ao Techno, e mentoria para quem está construindo o próprio som.',
             'Production, mixing and mastering of electronic music, from Psytrance to Techno, plus mentoring for artists shaping their own sound.'],
        ],
        fotos: [['st-mesa', 'A mesa de produção', 'The production desk'], ['st-sala', 'A sala: produção de um lado, CDJs do outro', 'The room: production on one side, CDJs on the other']],
        // Galeria "Por dentro do estúdio", em largura cheia abaixo do texto.
        maisFotos: [['st-bancada', 'A bancada de produção', 'The production desk'], ['st-setup', 'Sintetizador analógico e monitores', 'Analog synth and monitors'],
                    ['st-mixer', 'A mesa de som', 'The mixing console'], ['st-mic', 'Microfone para gravação', 'Recording mic']],
    },
    bio: [
        ['Equalized é o projeto do DJ e produtor paulistano Rogério Cardoso, com mais de 20 anos dedicados à música eletrônica. Nome de destaque na cena Psytrance, seu som transita entre o Techno e o Full On, misturando melodias emocionais, grooves hipnóticos e uma forte identidade progressiva.',
         'Equalized is the project of São Paulo-born DJ and producer Rogério Cardoso, with more than 20 years devoted to electronic music. A standout name in the Psytrance scene, his sound moves freely between Techno and Full On, blending emotional melodies, hypnotic grooves and a strong progressive identity.'],
        ['Inspirado por artistas como Neelix, Astrix, Liquid Soul e Captain Hook, conquistou reconhecimento internacional, com destaque para o single “Mr. Rabbit”, que chegou ao Top 6 do chart de Psytrance da Beatport.',
         'Inspired by artists such as Neelix, Astrix, Liquid Soul and Captain Hook, he has earned international recognition, including the single “Mr. Rabbit”, which reached the Top 6 of the Beatport Psytrance chart.'],
        // Pedido dele (05/11): citar os lançamentos pela Blue Tunes Records com destaque para Summer of '89 (botão logo abaixo da bio).
        ['Seus lançamentos saem pela Blue Tunes Records, e o mais recente é “Summer of ’89”.',
         'His releases come out on Blue Tunes Records, and the latest one is “Summer of ’89”.'],
        ['Também é mentor de produção musical e contribuiu diretamente para o desenvolvimento de projetos como Tiago Lutz (Sonic Massala), João Penna (Penna), DJ Mel Rose e Guilherme Araújo (At Work), entre outros talentos em ascensão.',
         'He is also a music production mentor and has directly helped develop projects such as Tiago Lutz (Sonic Massala), João Penna (Penna), DJ Mel Rose and Guilherme Araújo (At Work), among other rising talents.'],
    ],
    retrato: 'eq-retrato',
    galeria: ['eq-mao', 'eq-palco', 'eq-synth', 'eq-praia', 'eq-sorriso', 'eq-holi1'],
    // [nome, url, ícone em icones.mjs]. Linktree saiu a pedido dele (05/11).
    links: [
        ['Instagram', 'https://instagram.com/equalizedmusic', 'instagram'],
        ['Spotify', 'https://open.spotify.com/artist/5nIL632exR3FPgulSLr2IO', 'spotify'],
        ['SoundCloud', 'https://soundcloud.com/equalizedmusic', 'soundcloud'],
        ['Beatport', 'https://www.beatport.com/artist/equalized/647307', 'beatport'],
    ],
};

export const PACKS = [
    {
        id: 'align', nome: 'ALIGN', cor: '#a78bfa', tipo: ['VST3 · AU', 'VST3 · AU'], destaque: true,
        capa: 'align', preco: null, gratis: `${LIC}/align`,
        frase: ['Fase do kick e do baixo alinhada com um botão só.', 'Kick and bass phase aligned with a single button.'],
        texto: [
            'Vai no canal do baixo, com o kick pelo sidechain. A cada batida, o corretor automático escolhe a polaridade e a rotação de fase do grave do baixo e guarda a correção por nota. O LEVEL GUARD alinha sem virar soma. Latência zero.',
            'Goes on the bass channel, with the kick on the sidechain. On every hit, the automatic corrector picks the polarity and phase rotation of the bass low end and remembers it per note. LEVEL GUARD aligns without building up. Zero latency.'],
        plugins: [['align', 'ALIGN', 'Osciloscópio de 2 tempos travado no kick, LOW END, CORRELATION e NOTE MEMORY.', '2-beat oscilloscope locked to the kick, LOW END, CORRELATION and NOTE MEMORY.']],
    },
    {
        id: 'vassoura', nome: 'VASSOURA', cor: '#fcd34d', tipo: ['Max for Live · Ableton Live 12.2+', 'Max for Live · Ableton Live 12.2+'], destaque: true,
        capa: 'vassoura', preco: null, gratis: `${LIC}/baixar/vassoura`, direto: true,
        frase: ['Pare de limpar stems na mão.', 'Stop cleaning stems by hand.'],
        texto: [
            'Lê todas as faixas do set, acha os trechos vazios dos clips de áudio e muta (MUTE) ou apaga (CUT) só o silêncio. PRE e POST protegem o ataque e a cauda, SNAP alinha os cortes na grade. Num set de 81 stems: 465 vazios em segundos.',
            'Reads every track in the set, finds the empty parts of the audio clips and mutes (MUTE) or deletes (CUT) only the silence. PRE and POST keep attacks and tails safe, SNAP puts the cuts on the grid. On an 81-stem set: 465 gaps in seconds.'],
        plugins: [['vassoura', 'VASSOURA', 'A janela mostra cada faixa: som na cor da faixa, vazio tracejado.', 'The window shows every track: sound in the track color, gaps hatched.']],
    },
    {
        id: 'mesa', nome: 'MESA', cor: '#f59e0b', tipo: ['VST3 · AU', 'VST3 · AU'],
        capa: 'mesa', preco: null,
        frase: ['A console analógica inteira dentro do seu DAW.', 'A full analog console inside your DAW.'],
        texto: [
            'Uma MESA CANAL no fim de cada faixa e a janela de qualquer uma mostra a console inteira: canais, 3 grupos e MASTER. Pré, EQ de 4 bandas e compressores com o timbre dos clássicos, visual de hardware renderizado.',
            'A MESA CANAL at the end of every track, and any of them opens the whole console: channels, 3 groups and MASTER. Preamp, 4-band EQ and compressors with the tone of the classics, rendered hardware look.'],
        plugins: [['mesa', 'MESA', 'A console no master.', 'The console on the master.'], ['mesa-canal', 'MESA CANAL', 'O canal em cada faixa.', 'The channel strip on every track.']],
    },
    {
        id: 'criativo', nome: 'SÉRIE CRIATIVO', cor: '#f97316', tipo: ['10 plugins · VST3 · AU', '10 plugins · VST3 · AU'],
        capa: 'chama', preco: null,
        frase: ['Dez efeitos criativos com cara de pedal boutique.', 'Ten creative effects that look like boutique pedals.'],
        texto: [
            'Granular, glitch, phaser infinito, fita, build-up, filtro, ring mod, delay BBD, reverb shimmer e gate rítmico. Tudo no tempo do projeto, com presets.',
            'Granular, glitch, endless phaser, tape, build-up, filter, ring mod, BBD delay, shimmer reverb and rhythmic gate. All synced to your project, with presets.'],
        plugins: [
            ['grao', 'GRÃO', 'Efeito granular de textura: fita de 4 s, grãos, feedback, cauda e XY.', 'Granular texture effect: 4 s tape, grains, feedback, tail and XY.'],
            ['caleido', 'CALEIDO', 'Glitch rítmico: mandala de 16 passos no BPM.', 'Rhythmic glitch: 16-step mandala synced to BPM.'],
            ['espiral', 'ESPIRAL', 'Phaser/flanger infinito "barber pole".', 'Endless "barber pole" phaser/flanger.'],
            ['fita', 'FITA', 'Gravador de fita: saturação, wow/flutter, hiss e TAPE STOP.', 'Tape machine: saturation, wow/flutter, hiss and TAPE STOP.'],
            ['chama', 'CHAMA', 'Build-up de um knob só, com fogo na tela.', 'One-knob build-up, with fire on screen.'],
            ['f1', 'F1', 'Filtro ladder de 4 degraus com envelope e LFO no grid.', '4-pole ladder filter with envelope and grid LFO.'],
            ['ringer', 'RINGER', 'Ring modulator: do tremolo ao sino metálico.', 'Ring modulator: from tremolo to metallic bell.'],
            ['repeteco', 'REPETECO', 'Delay analógico tipo BBD que escurece a cada volta.', 'BBD-style analog delay that darkens on every repeat.'],
            ['miragem', 'MIRAGEM', 'Reverb atmosférico com shimmer, CONGELAR e REVERSO.', 'Atmospheric shimmer reverb with FREEZE and REVERSE.'],
            ['picote', 'PICOTE', 'Gate rítmico de 16 passos com volume, pan e filtro por passo.', '16-step rhythmic gate with per-step volume, pan and filter.'],
        ],
    },
    {
        id: 'mix', nome: 'SÉRIE MIX', cor: '#38bdf8', tipo: ['10 plugins · VST3 · AU', '10 plugins · VST3 · AU'],
        capa: 'torno', preco: null,
        frase: ['As ferramentas de mix e master, em rack de 19".', 'Mix and mastering tools in a 19" rack.'],
        texto: [
            'Multibanda, compressor, limiter com LUFS, cor de console, transiente, de-esser, estéreo M/S, reverb, medidor de entrega e clipper. Cada um mostra na tela o que está fazendo.',
            'Multiband, compressor, LUFS limiter, console color, transient shaper, de-esser, M/S stereo, reverb, delivery meter and clipper. Each one shows on screen what it is doing.'],
        plugins: [
            ['prisma', 'PRISMA', 'Multibanda: 4 bandas com compressor e saturação.', 'Multiband: 4 bands with compression and saturation.'],
            ['torno', 'TORNO', 'Compressor que mostra o que está apertando.', 'A compressor that shows what it is squeezing.'],
            ['barragem', 'BARRAGEM', 'Limiter de master com true peak, LUFS e alvos de entrega.', 'Master limiter with true peak, LUFS and delivery targets.'],
            ['console', 'CONSOLE', 'Cor de canal analógico: Cristal, Ferro e Brasa.', 'Analog channel color: Cristal, Ferro and Brasa.'],
            ['soco', 'SOCO', 'Modelador de transiente com antes e depois na tela.', 'Transient shaper with before and after on screen.'],
            ['lixa', 'LIXA', 'De-esser que mostra no espectro o que tira.', 'De-esser that shows on the spectrum what it removes.'],
            ['horizonte', 'HORIZONTE', 'Imagem estéreo M/S sem latência, com mono bass.', 'Zero-latency M/S stereo imaging with mono bass.'],
            ['sala', 'SALA', 'Reverb limpo para pôr tudo no mesmo espaço.', 'Clean reverb to put everything in the same room.'],
            ['monitor', 'MONITOR', 'LUFS, LRA, true peak, espectro e correlação.', 'LUFS, LRA, true peak, spectrum and correlation.'],
            ['navalha', 'NAVALHA', 'Clipper 4× que mostra ao vivo o que corta.', '4× clipper that shows live what it cuts.'],
        ],
    },
    {
        id: 'knob', nome: 'SÉRIE KNOB', cor: '#a3e635', tipo: ['10 pedais · VST3 · AU', '10 pedals · VST3 · AU'],
        capa: 'fole', preco: null,
        frase: ['Dez pedais de um knob grandão, tudo no tempo.', 'Ten big-knob pedals, all in time.'],
        texto: [
            'Sidechain, trance gate, filtro, auto-pan, stutter, delay, reverso, tape stop, pitch e bitcrush. Em todos: TRIGGER pelo DAW, por MIDI ou por áudio, RATE, SHAPE e MIX.',
            'Sidechain, trance gate, filter, auto-pan, stutter, delay, reverse, tape stop, pitch and bitcrush. On all of them: TRIGGER from the DAW, MIDI or audio, RATE, SHAPE and MIX.'],
        plugins: [
            ['fole', 'FOLE', 'Sidechain sincronizado, 6 formas.', 'Synced sidechain pump, 6 shapes.'],
            ['picador', 'PICADOR', 'Trance gate de 16 passos.', '16-step trance gate.'],
            ['mare', 'MARÉ', 'Filtro que abre e fecha no tempo.', 'Filter that opens and closes in time.'],
            ['vaivem', 'VAIVÉM', 'Auto-pan no tempo.', 'Tempo-synced auto-pan.'],
            ['repique', 'REPIQUE', 'Stutter / beat repeat.', 'Stutter / beat repeat.'],
            ['eco', 'ECO', 'Delay de fita no tempo.', 'Tempo-synced tape delay.'],
            ['avesso', 'AVESSO', 'Fatias do compasso tocadas ao contrário.', 'Slices of the bar played in reverse.'],
            ['freio', 'FREIO', 'Tape stop / tape start.', 'Tape stop / tape start.'],
            ['escada', 'ESCADA', 'Sequenciador de pitch dentro da escala.', 'In-scale pitch sequencer.'],
            ['mastiga', 'MASTIGA', 'Bitcrush ritmado.', 'Rhythmic bitcrush.'],
        ],
    },
    {
        id: 'quimera', nome: 'QUIMERA', cor: '#e879f9', tipo: ['Instrumento · VST3 · AU', 'Instrument · VST3 · AU'],
        capa: 'quimera', preco: null,
        frase: ['Sintetizador wavetable de 3 osciladores com MPE.', '3-oscillator wavetable synth with MPE.'],
        texto: [
            'Filtro ladder, mod matrix de 16 slots, 8 efeitos, arpejador de 16 passos com SCALE/ROOT e 400 presets.',
            'Ladder filter, 16-slot mod matrix, 8 effects, 16-step arpeggiator with SCALE/ROOT and 400 presets.'],
        plugins: [['quimera', 'QUIMERA', '', '']],
    },
    {
        id: 'limao', nome: 'LIMÃO', cor: '#bef264', tipo: ['Instrumento · VST3 · AU', 'Instrument · VST3 · AU'],
        capa: 'limao', preco: null,
        frase: ['Bassline ácido com alma de 303.', 'Acid bassline with a 303 soul.'],
        texto: [
            'Filtro ladder de 18 dB/oit, ACCENT que acumula, SLIDE, macro ÁCIDO, drive, delay e reverb, sequenciador de 32 passos com SCALE/ROOT e ALEATORIZAR. 360 presets.',
            '18 dB/oct ladder filter, stacking ACCENT, SLIDE, ACID macro, drive, delay and reverb, 32-step sequencer with SCALE/ROOT and RANDOMIZE. 360 presets.'],
        plugins: [['limao', 'LIMÃO', '', '']],
    },
    {
        id: 'mercurio', nome: 'MERCÚRIO', cor: '#f472b6', tipo: ['Instrumento · VST3 · AU', 'Instrument · VST3 · AU'],
        capa: 'mercurio', preco: null,
        frase: ['FM de 4 operadores: plástico, metal e psicodelia.', '4-operator FM: plastic, metal and psychedelia.'],
        texto: [
            'Polifônico de 16 vozes, 8 algoritmos com o desenho na tela, razões inarmônicas, RING MOD e FOLD, dois filtros e 360 presets.',
            '16-voice polyphonic, 8 algorithms drawn on screen, inharmonic ratios, RING MOD and FOLD, two filters and 360 presets.'],
        plugins: [['mercurio', 'MERCÚRIO', '', '']],
    },
    {
        id: 'resenha', nome: 'RESENHA', cor: '#fb7185', tipo: ['Instrumento · VST3 · AU', 'Instrument · VST3 · AU'],
        capa: 'resenha', preco: null,
        frase: ['Call & response FM: quatro timbres que conversam.', 'FM call & response: four voices in conversation.'],
        texto: [
            'Gerador de frase na escala, troca de timbre a cada N notas, UNISON, filtro VOWEL (o lead "fala") e MOVE no tempo. 108 presets.',
            'In-scale phrase generator, voice switching every N notes, UNISON, VOWEL filter (the lead "talks") and tempo-synced MOVE. 108 presets.'],
        plugins: [['resenha', 'RESENHA', '', '']],
    },
    {
        id: 'groovador', nome: 'GROOVADOR', cor: '#2dd4bf', tipo: ['Instrumento · VST3 · AU', 'Instrument · VST3 · AU'],
        capa: 'groovador', preco: null,
        frase: ['Percussão sintetizada com gerador de groove.', 'Synthesized percussion with a groove generator.'],
        texto: [
            '8 instrumentos sem bumbo, 5 modelos cada, grade de até 4 compassos e gerador por estilo (psy, techno, house, tribal, break). Arraste o MIDI para o DAW.',
            '8 kick-less instruments, 5 models each, up to 4-bar grid and style-based generator (psy, techno, house, tribal, break). Drag the MIDI into your DAW.'],
        plugins: [['groovador', 'GROOVADOR', '', '']],
    },
];

// preço de cada pack = o do servidor (brl/usd null = em breve)
for (const p of PACKS) p.preco = PRECOS[p.id] || null;
