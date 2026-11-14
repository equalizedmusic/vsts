// Conteúdo do site: packs, plugins e textos. Cada texto vem em [pt, en] (PT primeiro: o site abre em português).
// preco: vem de licenca/servidor/src/precos.js (o mesmo que o servidor cobra) — { brl, usd } ou null = selo "EM BREVE". gratis: link da página grátis
// (direto: true = o link já é o download, sem cadastro nem token — caso da VASSOURA, que não ativa).

import { PRECOS, OFERTA_FIM, DE_BRL, fimOferta, precoAtual } from '../licenca/servidor/src/precos.js';

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
    lancamento: { titulo: "Summer of '89", url: 'https://bluetunesrecords.lnk.to/summerof89', capa: '/img/summer89.webp' },
    // Página Estúdio (05/11): texto do estúdio + bio tirada do release dele (Google Docs). Cada parágrafo = [pt, en].
    estudio: {
        lead: ['Estúdio de mix e master em São Paulo, comandado pelo DJ e produtor Rogério Cardoso, o Equalized.',
               'Mixing and mastering studio in São Paulo, Brazil, run by DJ and producer Rogério Cardoso, aka Equalized.'],
        texto: [
            ['É aqui que nascem as faixas do Equalized e os plugins da loja. Sala tratada, monitores de referência, sintetizador analógico, controladores e CDJs ficam no mesmo ambiente: dá para compor, mixar e testar a faixa como na pista, sem sair da sala.',
             'This is where the Equalized tracks and the plugins in the shop are made. Treated room, reference monitors, analog synth, controllers and CDJs all in one place: you can write, mix and test the track the way it plays on the dancefloor, without leaving the room.'],
            ['Mixagem e masterização com setup híbrido: o digital dá a precisão, o analógico traz o punch e a textura que deixam a faixa viva na pista, do Psytrance ao Techno.',
             'Mixing and mastering on a hybrid setup: digital brings the precision, analog adds the punch and texture that make a track come alive on the dancefloor, from Psytrance to Techno.'],
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

// Perguntas frequentes da loja (05/11/2026, ideia tirada de rewindlabs.com.br). [pergunta pt, en, resposta pt, en].
// Fatos daqui vêm do servidor: 2 máquinas por licença, 45 dias sem internet (VALIDADE_DIAS), pastas do instalador (loja.js/gratis.js).
export const FAQ = [
    ['Funciona no Mac? Quais sistemas?', 'Does it work on Mac? Which systems?',
     'Sim. Os plugins vêm em VST3 e AU, com instalador para Windows e para Mac. Precisa de macOS 11 Big Sur ou mais novo (Apple Silicon ou Intel); versões anteriores, como o Catalina, não instalam. No Windows, precisa do Windows 10 ou 11 de 64 bits. A VASSOURA é Max for Live e roda onde o Ableton Live 12.2 ou mais novo roda.',
     'Yes. The plugins come as VST3 and AU, with installers for Windows and Mac. They need macOS 11 Big Sur or newer (Apple Silicon or Intel); older versions such as Catalina will not install. On Windows they need 64-bit Windows 10 or 11. VASSOURA is Max for Live and runs wherever Ableton Live 12.2 or newer runs.'],
    ['Funciona em outras DAWs além do Ableton?', 'Does it work in DAWs other than Ableton?',
     'Sim. Funciona em qualquer DAW que abra VST3 (Windows e Mac) ou AU (Mac), como FL Studio, Logic, Cubase, Bitwig, Reaper e Studio One. A exceção é a VASSOURA, que é Max for Live e só roda no Ableton Live.',
     'Yes. They work in any DAW that loads VST3 (Windows and Mac) or AU (Mac), such as FL Studio, Logic, Cubase, Bitwig, Reaper and Studio One. The exception is VASSOURA, which is Max for Live and only runs in Ableton Live.'],
    ['Quando recebo os plugins?', 'When do I get the plugins?',
     'Assim que o Mercado Pago aprova o pagamento, chegam no seu e-mail o token de ativação, o link de download e as instruções. Com Pix e cartão isso costuma ser na hora. Se não chegar em alguns minutos, olhe a caixa de spam e depois escreva para contato@insidestudiosp.com.br.',
     'As soon as the payment is approved you get the activation token, the download link and the instructions by e-mail. With Pix and card this is usually instant. If nothing arrives within a few minutes, check your spam folder and then write to contato@insidestudiosp.com.br.'],
    ['O navegador avisou que o arquivo é incomum ou suspeito. É normal?', 'My browser said the file is uncommon or suspicious. Is that normal?',
     'É. Os instaladores são novos e ainda foram baixados por pouca gente, e por isso o navegador, o Windows (tela azul do SmartScreen) ou o Mac avisam. Baixe sempre pelo link do site ou do e-mail. No Windows, clique em "Mais informações" e depois em "Executar assim mesmo". No Mac, abra Ajustes do Sistema > Privacidade e Segurança e clique em "Abrir Mesmo Assim". O passo a passo completo vai junto com o download.',
     'Yes. The installers are new and few people have downloaded them yet, so the browser, Windows (the blue SmartScreen screen) or the Mac show a warning. Always download from the link on the site or in the e-mail. On Windows, click "More info" and then "Run anyway". On Mac, open System Settings > Privacy & Security and click "Open Anyway". The full step-by-step comes with the download.'],
    ['Em quantos computadores posso usar?', 'How many computers can I use it on?',
     'Em 2. Para trocar de computador, desative a licença no antigo pelo menu da licença no próprio plugin e ative no novo.',
     'On 2. To move to another computer, deactivate the license on the old one from the license menu inside the plugin and activate it on the new one.'],
    ['Preciso de internet para usar?', 'Do I need internet to use it?',
     'Só para ativar. Depois o plugin funciona sem internet por até 45 dias e renova sozinho quando o computador fica on-line.',
     'Only to activate. After that the plugin works offline for up to 45 days and renews by itself when the computer is back online.'],
    ['Posso pedir reembolso?', 'Can I get a refund?',
     'Pode, em até 7 dias da compra, como garante o Código de Defesa do Consumidor (direito de arrependimento). Escreva para contato@insidestudiosp.com.br com o e-mail usado na compra. O valor é devolvido pelo Mercado Pago e a licença é cancelada.',
     'Yes, within 7 days of purchase, as guaranteed by Brazilian consumer law. Write to contato@insidestudiosp.com.br with the e-mail you used to buy. The amount is refunded through Mercado Pago and the license is cancelled.'],
];

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
        id: 'barebass', nome: 'BAREBASS', cor: '#34d399', tipo: ['Synth · VST3 · AU', 'Synth · VST3 · AU'], destaque: true,
        capa: 'barebass', preco: null, gratis: `${LIC}/barebass`,
        frase: ['Síntese de baixo sem complicação.', 'Bass synthesis, stripped down.'],
        texto: [
            'Dois osciladores e um SUB, filtro de 12/24 dB com envelope e o unHUNF: o sub bate no começo da nota e some, e o baixo fica seco sem embolar com o kick. Cada nota começa a onda do mesmo ponto, então todas batem igual. Junto com o ALIGN, kick e baixo ficam em fase sozinhos.',
            'Two oscillators and a SUB, a 12/24 dB filter with envelope and unHUNF: the sub hits at the start of the note and fades away, so the bass stays tight without clashing with the kick. Every note starts the wave from the same point, so they all hit the same. Paired with ALIGN, kick and bass stay in phase on their own.'],
        plugins: [['barebass', 'BAREBASS', 'A tela mostra ao vivo a onda, o filtro e o envelope de cada nota.', 'The screen shows the wave, filter and envelope of every note, live.']],
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
        id: 'stompbox', nome: 'STOMPBOX', cor: '#ef4444', tipo: ['Guitarra · VST3 · AU', 'Guitar · VST3 · AU'],
        capa: 'stompbox', preco: null,
        frase: ['O pedalboard de guitarra inteiro dentro do seu DAW.', 'A full guitar pedalboard inside your DAW.'],
        texto: [
            'Até 10 pedais em série no board, com emulação dos circuitos clássicos: compressor, overdrives, distorção, fuzz, wah, phaser, chorus, delay de BBD ou fita, reverb e pitch shifter. No fim, 5 amps e caixa com microfone. NOISE GATE e afinador na entrada. 24 presets inspirados em timbres de guitarristas famosos.',
            'Up to 10 pedals in series on the board, modeled on classic circuits: compressor, overdrives, distortion, fuzz, wah, phaser, chorus, BBD or tape delay, reverb and pitch shifter. Then 5 amps and a miked cab. NOISE GATE and tuner at the input. 24 presets inspired by famous guitar tones.'],
        plugins: [['stompbox', 'STOMPBOX', 'Arraste os pedais para mudar a ordem; cada amp muda de cor na tela.', 'Drag pedals to change their order; each amp has its own color on screen.']],
    },
    {
        id: 'maestro', nome: 'MAESTRO', cor: '#8b5cf6', tipo: ['Mixagem · VST3 · AU', 'Mixing · VST3 · AU'],
        capa: 'maestro', preco: null,
        // bf (pedido dele, 10/11): a oferta é "preço promocional até a Black Friday" — textos explícitos e popup com cara de Black Friday.
        // Some sozinho com a oferta (so-oferta + data-fim); o rebuild depois de 27/11 já sai sem nada disso (sem p.de).
        bf: true,
        // banner de lançamento no topo da loja (pedido dele, 09/11): tirar o "lancamento" quando passar a novidade
        lancamento: {
            combo: 'maestromagos',   // 11/11: o popup também oferece o combo com o MASTER DOS MAGOS
            titulo: ['Um clique e a mix se organiza.', 'One click and the mix sorts itself out.'],
            texto: ['Um MAESTRO em cada grupo: LEARN ouve o drop, LOCK acerta nível e tom, o GUARD abre espaço para kick e baixo e o FOCUS destaca quem você escolher. Um rascunho de mix equilibrado em segundos.',
                    'One MAESTRO on each group: LEARN listens to the drop, LOCK sets level and tone, GUARD makes room for kick and bass, and FOCUS brings forward whoever you pick. A balanced mix draft in seconds.'],
        },
        frase: ['Um MAESTRO em cada grupo, e a mix se organiza sozinha.', 'One MAESTRO on each group, and the mix sorts itself out.'],
        texto: [
            'Coloque o MAESTRO nos 8 grupos da sua mix (KICK, BASS, DRUMS, HATS, SYNTHS, AMBIENCE, VOCAL, FX). As cópias conversam entre si, sem sidechain nem roteamento. LEARN ouve o drop, LOCK acerta o nível e o tom de cada grupo pelo STYLE, o LOW-END GUARD abre espaço para o kick e o baixo e o FOCUS destaca o que você escolher com EQ dinâmica. Um rascunho de mix equilibrado em segundos, para você terminar do seu jeito. Latência zero.',
            'Put MAESTRO on the 8 groups of your mix (KICK, BASS, DRUMS, HATS, SYNTHS, AMBIENCE, VOCAL, FX). The copies talk to each other, no sidechain, no routing. LEARN listens to the drop, LOCK sets each group level and tone by STYLE, LOW-END GUARD makes room for kick and bass, and FOCUS brings forward what you pick with dynamic EQ. A balanced mix draft in seconds, for you to finish your way. Zero latency.'],
        plugins: [['maestro', 'MAESTRO', 'Qualquer janela é a console dos 8 grupos, com o mapa de conflito e a EQ dinâmica.', 'Any window is the console for all 8 groups, with the conflict map and the dynamic EQ.']],
    },
    {
        // MASTER DOS MAGOS (11/11/2026, aprovado no teste dele): par do MAESTRO, R$ 149,90 sem promoção
        id: 'magos', nome: 'MASTER DOS MAGOS', cor: '#d4a72c', tipo: ['Masterização · VST3 · AU', 'Mastering · VST3 · AU'],
        capa: 'magos', preco: null,
        frase: ['O master da sua faixa num clique, com o ouvido na referência.', 'Your track mastered in one click, with an ear on the reference.'],
        texto: [
            'Vai por último no master. Solte uma faixa de referência (ou use a curva do STYLE), aperte LISTEN no drop e depois CAST: o MASTER DOS MAGOS acerta o balanço tonal (TONE MATCH), ajusta o compressor multibanda de 4 bandas, liga a EQ dinâmica (corta embolado, aspereza e sibilância só nos picos e realça o punch do grave e o ar quando faltam) e leva o limiter true peak até o loudness alvo, com teto de −0,3 dBTP. Tudo em fase linear. A/B com volume igualado, simulador de plataformas e alto-falantes, e modo MANUAL para ajustar cada banda e cada ponto da EQ. Com o MAESTRO no projeto, ele mostra de qual grupo da mix vem cada problema.',
            'It goes last on the master. Drop a reference track (or use the STYLE curve), press LISTEN on the drop, then CAST: MASTER DOS MAGOS matches the tonal balance (TONE MATCH), sets the 4-band multiband compressor, turns on the dynamic EQ (cuts mud, harshness and sibilance only on the peaks, and lifts low punch and air when they are missing) and drives the true peak limiter to the target loudness, with a −0.3 dBTP ceiling. All in linear phase. Loudness-matched A/B, platform and speaker simulator, and a MANUAL mode to adjust every band and every EQ point. With MAESTRO in the project, it shows which group of the mix each problem comes from.'],
        plugins: [['magos', 'MASTER DOS MAGOS', 'MAESTRO LINKED: cada problema do master apontado no grupo da mix que causa.', 'MAESTRO LINKED: each master problem traced to the mix group that causes it.']],
    },
    {
        // combo (11/11/2026): MAESTRO + MASTER DOS MAGOS por R$ 179,90; o servidor cria as duas licenças (precos.js COMBOS)
        id: 'maestromagos', nome: 'MAESTRO + MASTER DOS MAGOS', cor: '#a78bfa', tipo: ['Combo · 2 plugins · VST3 · AU', 'Bundle · 2 plugins · VST3 · AU'],
        capa: 'maestromagos', preco: null, combo: ['maestro', 'magos'],
        frase: ['Do mix ao master: os dois juntos, por menos.', 'From mix to master: both together, for less.'],
        texto: [
            'O MAESTRO organiza a mix nos 8 grupos e o MASTER DOS MAGOS fecha o master. Juntos, eles conversam: o MAGOS lê os grupos do MAESTRO e diz em qual grupo corrigir cada problema, em vez de remendar no master. Uma compra, as duas licenças no mesmo token.',
            'MAESTRO organizes the mix across 8 groups and MASTER DOS MAGOS finishes the master. Together they talk: MAGOS reads the MAESTRO groups and tells you which group to fix each problem in, instead of patching it on the master. One purchase, both licenses on the same token.'],
        plugins: [['maestro', 'MAESTRO', 'A mix: console dos 8 grupos.', 'The mix: console for all 8 groups.'],
                  ['magos', 'MASTER DOS MAGOS', 'O master, ligado aos grupos do MAESTRO.', 'The master, linked to the MAESTRO groups.']],
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
        // 0.6.0 (12/11/2026): HUB — cada slot INTERNO ou PLUGIN (synth do cliente)
        // 0.7.0 (13/11/2026): PLAY em modo NOTAS (toca as notas do clipe, canal 1-4 = timbre A-D) e CONTORNO RETA
        frase: ['Call & response: os seus synths conversando.', 'Call & response: your own synths talking to each other.'],
        texto: [
            '4 slots que conversam: cada um usa o timbre INTERNO do RESENHA ou carrega um synth seu (VST3, e Audio Unit no Mac), com os presets dele: Serum num slot, Sylenth1 no outro. A busca lê só instrumentos e pula plugin que trava. Gerador de frase na escala, troca de timbre a cada N notas, OITAVA por slot, ARRASTAR MIDI para o DAW e 120 presets. No modo NOTAS ele toca as notas do seu clipe (o canal escolhe o timbre), e o CONTORNO RETA prende a frase na tônica.',
            '4 slots that talk to each other: each one uses RESENHA\'s INTERNAL sound or loads one of your synths (VST3, and Audio Unit on Mac) with its own presets: Serum in one slot, Sylenth1 in another. The scan reads instruments only and skips plugins that hang. In-scale phrase generator, voice switching every N notes, per-slot OCTAVE, DRAG MIDI into your DAW and 120 presets. In NOTAS (notes) mode it plays the notes from your clip (the MIDI channel picks the voice), and the RETA (flat) contour locks the phrase to the root.'],
        plugins: [['resenha', 'RESENHA', 'HUB: Serum, Sylenth1 ou o synth que você quiser em cada slot, conversando na escala.', 'HUB: Serum, Sylenth1 or any synth you like in each slot, talking in key.']],
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

// Seções da loja (pedido dele, 10/11: a home virou um card por seção, com o leque de produtos dentro, para não rolar demais).
// Cada seção tem a página /plugins/<id>/ com os cards dos produtos. Um produto pode estar em mais de uma seção
// (ALIGN e VASSOURA em Grátis e Mix, BAREBASS em Grátis e Synths). O "voltar" da página do pack vai para a 1ª seção paga em que ele aparece.
export const SECOES = [
    { id: 'gratis', nome: ['Grátis', 'Free'], cor: '#34d399', itens: ['align', 'barebass', 'vassoura'],
      frase: ['Baixe e use agora, sem pagar nada.', 'Download and use them now, for free.'] },
    { id: 'mix', nome: ['Mix', 'Mix'], cor: '#8b5cf6', itens: ['maestro', 'magos', 'maestromagos', 'mesa', 'align', 'vassoura'],
      frase: ['Console, mix automática, fase do grave e limpeza de stems.', 'Console, auto mix, low-end phase and stem cleanup.'] },
    { id: 'synths', nome: ['Synths', 'Synths'], cor: '#e879f9', itens: ['quimera', 'limao', 'mercurio', 'resenha', 'groovador', 'barebass'],
      frase: ['Wavetable, ácido, FM, percussão e baixo.', 'Wavetable, acid, FM, percussion and bass.'] },
    { id: 'pedais', nome: ['Pedais', 'Pedals'], cor: '#ef4444', itens: ['stompbox'],
      frase: ['Pedalboard de guitarra com amps e caixa.', 'Guitar pedalboard with amps and cab.'] },
    { id: 'packs', nome: ['Packs', 'Packs'], cor: '#f97316', itens: ['criativo', 'mix', 'knob'],
      frase: ['Séries de 10 plugins: Criativo, Mix e Knob.', '10-plugin series: Creative, Mix and Knob.'] },
];

// preço de cada pack = o do servidor (brl/usd null = em breve)
// (precoAtual: o MAESTRO sai a 149,90 em build feito depois da Black Friday)
for (const p of PACKS) p.preco = precoAtual(p.id) || null;
// combo: soma dos preços avulsos de hoje (mostrada como "separados" na página do combo)
for (const p of PACKS) if (p.combo) p.soma = p.combo.reduce((s, id) => s + (precoAtual(id)?.brl || 0), 0);

// "de" da oferta de lançamento: OFERTA_FIM e DE_BRL vêm de precos.js (a vitrine das páginas grátis usa os mesmos).
// Depois de OFERTA_FIM o site.js some com a oferta na hora e o próximo build já sai sem ela.
export { OFERTA_FIM };
export const OFERTA_ATIVA = Date.now() < Date.parse(OFERTA_FIM);
// cada pack tem o seu fim (fimOferta: MAESTRO termina na Black Friday, os demais em OFERTA_FIM)
for (const p of PACKS) {
    p.fim = fimOferta(p.id);
    if (Date.now() < Date.parse(p.fim) && p.preco && DE_BRL[p.preco.brl]) p.de = DE_BRL[p.preco.brl];
}
