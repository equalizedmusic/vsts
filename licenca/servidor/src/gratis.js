// Páginas grátis: a pessoa põe o e-mail e recebe na tela o token e os links de download, sem passar pelo painel.
// É o link mandado no direct do Instagram para quem comenta a palavra do reel (QUERO, ALIGN…).
//
//   GET  /gratis/montamonta   MONTA-MONTA (2 computadores, links Windows e Mac)
//   GET  /gratis/vassoura     VASSOURA (só o e-mail: cadastro sem token, e-mail com o link do zip e o passo a passo)
//   GET  /gratis/align        ALIGN (1 computador; a pessoa escolhe Windows ou Mac e recebe só aquele link)
//   GET  /gratis/barebass     BAREBASS (igual ao ALIGN; config em VST_GRATIS)
//   POST /gratis/<pacote>     { email, sistema?, site }   (site = campo escondido; robô que preenche leva "ok" falso)
//     -> { ok, novo, token?, mesmo_token?, ja_tinha?, baixar: { win, mac } | { win } | { mac } }
//
// Regras:
//   - e-mail sem cadastro: cria a licença do pacote (origem "gratis") e mostra o token UMA vez;
//   - e-mail que já tem outra licença: entra o pacote com o mesmo token (o token não aparece: ninguém lê o token
//     de outro e-mail digitando o e-mail dele);
//   - e-mail que já tem o pacote: não muda nada, só lembra de usar o token que já recebeu.
//   - Como o token sai na tela sem conferir o e-mail, quem cadastra o e-mail de outra pessoa conhece o token dele.
//     Por isso, quando uma venda ou cortesia entra num e-mail que só tem licença "gratis", o servidor troca o token
//     (ver /admin/licencas em index.js) e o token da página deixa de valer para o pacote pago.
//   - Até 8 cadastros por IP em 24 h e 300 por dia no total (as páginas somam juntas).
//   - O sistema escolhido vai na nota da licença (aparece e é buscável no painel).

import { zipStream } from './zip.js';
import { emailLigado, enviarEmail, CONTATO } from './email.js';
import { PRECOS, OFERTA_FIM, DE_BRL, fimOferta, precoAtual } from './precos.js';
import { PACKS, SITE } from '../../../site/dados.mjs';   // nome, frase, texto, cor e capa: os mesmos da página de pack do site

const POR_IP_DIA = 8;
const TOTAL_DIA = 300;
const PAGINAS = {
    montamonta: { maquinas: 2, sistema: false, nota: 'página grátis' },
    align: { maquinas: 1, sistema: true, nota: 'página ALIGN' },
    barebass: { maquinas: 1, sistema: true, nota: 'página BAREBASS' },
    // VASSOURA (Max for Live) não ativa: o cadastro só guarda o e-mail (token_hash vazio, nenhum token vale nele)
    // e manda por e-mail o link do zip e o passo a passo daquele sistema (o zip é o mesmo; muda a pasta e os atalhos)
    vassoura: { maquinas: 0, sistema: true, nota: 'página VASSOURA', semToken: true },
};
export const PACOTES_GRATIS = Object.keys(PAGINAS);

// VSTs grátis com token (página no molde do site, zip com LEIA-ME, e-mail e "perdi meu token"): o que muda de um para o outro.
// pkg = nome do pacote do instalador (InsideStudioSP-<pkg>-win.exe / "Instalar InsideStudioSP <pkg>.pkg").
// usoPt/usoEn = passo 2 do LEIA-ME; prontoPt/prontoEn = último passo da ativação; pagUso = passo 2 da página (HTML).
export const VST_GRATIS = {
    align: {
        nome: 'ALIGN', pkg: 'Align', sub: 'Kick / bass phase aligner',
        usoTitPt: '2. COLOCAR NO BAIXO', usoTitEn: '2. PUT IT ON THE BASS',
        usoPt: ['  1) Coloque o ALIGN no canal do BAIXO.',
                '  2) No Sidechain do ALIGN, escolha a faixa do KICK.',
                '     No Ableton: abra o Sidechain do plugin, escolha a faixa do kick e deixe em Post FX.'],
        usoEn: ['  1) Put ALIGN on your BASS track.',
                '  2) In the ALIGN Sidechain, pick the KICK track.',
                '     In Ableton: open the plugin Sidechain, pick the kick track and set it to Post FX.'],
        prontoPt: '  2) Aperte o botão ALIGN. Pronto: ele corrige sozinho, batida por batida.',
        prontoEn: '  2) Press the ALIGN button. Done: it corrects by itself, hit by hit.',
        pagUsoTit: ['2. Coloque no canal do baixo', '2. Put it on your bass'],
        pagUso: [
            ['Abra o seu DAW e coloque o <b>ALIGN</b> no canal do <span class="b">BAIXO</span>.', 'Open your DAW and put <b>ALIGN</b> on the <span class="b">BASS</span> channel.'],
            ['No <b>Sidechain</b> do ALIGN, escolha a faixa do <span class="k">KICK</span> (no Ableton: abra o Sidechain do plugin, escolha a faixa do kick e deixe em Post FX).', 'In ALIGN\'s <b>Sidechain</b>, pick the <span class="k">KICK</span> track (in Ableton: open the plugin Sidechain, pick the kick track and leave it on Post FX).'],
        ],
        pagPronto: ['Aperte o botão <b>ALIGN</b>. Pronto: ele corrige sozinho, batida por batida.', 'Press the <b>ALIGN</b> button. That\'s it: it corrects by itself, hit by hit.'],
    },
    barebass: {
        nome: 'BAREBASS', pkg: 'Barebass', sub: 'Simple bass synth',
        usoTitPt: '2. ABRIR NO DAW', usoTitEn: '2. OPEN IT IN YOUR DAW',
        usoPt: ['  1) Crie uma faixa MIDI e coloque o BAREBASS nela (ele é instrumento: fica junto dos synths).',
                '  2) Toque notas graves. O PRESET (no alto) traz timbres prontos; o INIT é o ponto de partida.',
                '  3) Com o ALIGN (grátis também): coloque o ALIGN depois do BAREBASS, na mesma faixa,',
                '     e mande o KICK pelo Sidechain do ALIGN. Baixo e kick ficam em fase sozinhos.'],
        usoEn: ['  1) Create a MIDI track and put BAREBASS on it (it is an instrument: look among the synths).',
                '  2) Play low notes. PRESET (at the top) has ready-made sounds; INIT is the starting point.',
                '  3) With ALIGN (also free): put ALIGN after BAREBASS on the same track and send the KICK',
                '     to the ALIGN Sidechain. Bass and kick stay in phase by themselves.'],
        prontoPt: '  2) Pronto: o BAREBASS já toca. Sem ativação ele fica mudo.',
        prontoEn: '  2) Done: BAREBASS plays now. Without activation it stays silent.',
        pagUsoTit: ['2. Abra no seu DAW', '2. Open it in your DAW'],
        pagUso: [
            ['Crie uma faixa <b>MIDI</b> e coloque o <b>BAREBASS</b> nela (é instrumento: fica junto dos synths).', 'Create a <b>MIDI</b> track and put <b>BAREBASS</b> on it (it is an instrument: look among the synths).'],
            ['Toque notas graves. O <b>PRESET</b> traz timbres prontos; o <b>INIT</b> é o ponto de partida.', 'Play low notes. <b>PRESET</b> has ready-made sounds; <b>INIT</b> is the starting point.'],
            ['Tem o <a class="link" href="/align/">ALIGN</a>? Coloque depois do BAREBASS e mande o <span class="k">KICK</span> pelo Sidechain: baixo e kick ficam em fase sozinhos.', 'Got <a class="link" href="/align/">ALIGN</a>? Put it after BAREBASS and send the <span class="k">KICK</span> to its Sidechain: bass and kick stay in phase by themselves.'],
        ],
        pagPronto: ['Pronto: o BAREBASS já toca. Sem ativação ele fica mudo.', 'Done: BAREBASS plays now. Without activation it stays silent.'],
    },
};

function json(obj, status = 200) {
    return new Response(JSON.stringify(obj), {
        status, headers: { 'content-type': 'application/json; charset=utf-8', 'cache-control': 'no-store' },
    });
}

export async function rotaGratis(req, env, url, h, PACOTE = 'montamonta') {
    const cfg = PAGINAS[PACOTE];
    // form-action 'self': o botão de download do ALIGN é um formulário (POST /gratis/align/zip)
    if (req.method === 'GET') {
        const agora = Date.now();
        const site = !!VST_GRATIS[PACOTE] || PACOTE === 'vassoura';   // no molde do site (estilo.css e site.js do domínio do estúdio)
        const html = VST_GRATIS[PACOTE] ? paginaVst(PACOTE, agora, emailLigado(env)) : PACOTE === 'vassoura' ? paginaVassoura(agora) : PAGINA;
        return new Response(html, { headers: { 'content-type': 'text/html; charset=utf-8', 'cache-control': 'no-store',
            'content-security-policy': site ? `${CSP_SITE}; form-action ${VST_GRATIS[PACOTE] ? "'self'" : "'none'"}`
                : "default-src 'none'; style-src 'unsafe-inline'; script-src 'unsafe-inline'; connect-src 'self'; img-src data:; base-uri 'none'; form-action 'none'; frame-ancestors 'none'" } });
    }
    if (req.method !== 'POST') return json({ ok: false, mensagem: 'Método não aceito.' }, 405);

    const ip = req.headers.get('cf-connecting-ip') || '';
    let corpo;
    try { corpo = await req.json(); } catch { return json({ ok: false, mensagem: 'Pedido inválido.' }, 400); }
    const email = h.normEmail(corpo.email);
    const sistema = cfg.sistema ? String(corpo.sistema || '') : '';
    const baixar = {};
    if (PACOTE === 'vassoura') baixar.zip = `${url.origin}${VASSOURA_ZIP}`;
    else for (const s of ['win', 'mac']) if (!sistema || s === sistema) baixar[s] = `${url.origin}/baixar/${PACOTE}/${s}`;
    if (corpo.site) return json({ ok: true, novo: false, ja_tinha: true, baixar });   // robô
    if (email.length > 120 || !/^[^@\s]+@[^@\s]+\.[^@\s]+$/.test(email))
        return json({ ok: false, mensagem: 'Confira o e-mail.' }, 400);
    if (cfg.sistema && !['win', 'mac'].includes(sistema))
        return json({ ok: false, mensagem: 'Escolha Windows ou Mac.' }, 400);

    const desde = h.agora() - 86400;
    const n = await env.DB.prepare("SELECT SUM(ip = ?) AS ip, COUNT(*) AS total FROM eventos WHERE tipo = 'gratis' AND quando > ?")
        .bind(ip, desde).first();
    if ((n?.ip || 0) >= POR_IP_DIA || (n?.total || 0) >= TOTAL_DIA)
        return json({ ok: false, mensagem: 'Muitos pedidos agora. Tente de novo amanhã ou chame a gente no direct.' }, 429);

    // licença de TESTE (token próprio, ver index.js criarLicenca) não conta aqui: nem como pacote, nem como token do e-mail
    const doEmail = await env.DB.prepare("SELECT pacote, token_hash, status FROM licencas WHERE email = ? AND origem <> 'teste'").bind(email).all();
    const lista = doEmail.results || [];
    if (lista.some((l) => l.pacote === PACOTE)) {
        await h.evento(env, null, 'gratis_ja', email, ip);
        return json({ ok: true, novo: false, ja_tinha: true, baixar, reenvio: !!VST_GRATIS[PACOTE] && emailLigado(env) });
    }
    // cadastro da VASSOURA não tem token (token_hash vazio): não conta como "já tem token" para os outros pacotes
    const comToken = lista.filter((l) => l.token_hash);
    const token = cfg.semToken || comToken.length ? null : h.novoToken();
    const hash = cfg.semToken ? '' : comToken.length ? comToken[0].token_hash : await h.sha256Hex('ISP' + h.normToken(token));
    const nota = cfg.nota + (sistema ? ' · ' + (sistema === 'mac' ? 'Mac' : 'Windows') : '');
    const r = await env.DB.prepare(
        "INSERT INTO licencas (email, pacote, token_hash, max_maquinas, origem, nota, criada_em) VALUES (?, ?, ?, ?, 'gratis', ?, ?)")
        .bind(email, PACOTE, hash, cfg.maquinas, nota, h.agora()).run();
    await h.evento(env, r.meta.last_row_id, 'gratis', cfg.semToken ? 'cadastro' : comToken.length ? 'mesmo token' : 'novo', ip);
    if (PACOTE === 'vassoura') {
        const enviado = await emailVassoura(env, url, h, email, sistema);
        if (enviado) await h.evento(env, r.meta.last_row_id, 'email', 'cadastro', ip);
        return json({ ok: true, novo: true, baixar, email_enviado: !!enviado });
    }
    const V = VST_GRATIS[PACOTE];
    const enviado = !!V && await emailVst(env, url, h, { pacote: PACOTE, email, token, sistema, assunto: `Sua licença do ${V.nome} · Your ${V.nome} license` });
    if (enviado) await h.evento(env, r.meta.last_row_id, 'email', 'cadastro', ip);
    return json({ ok: true, novo: true, token, mesmo_token: !!comToken.length, baixar, email_enviado: !!enviado, reenvio: !!VST_GRATIS[PACOTE] && emailLigado(env) });
}

// e-mail com o mesmo conteúdo do LEIA-ME do zip + o link direto do instalador (o zip é POST com o token)
async function emailVst(env, url, h, { pacote, email, token, sistema, assunto, aviso = [] }) {
    if (!emailLigado(env)) return false;
    const V = VST_GRATIS[pacote];
    let inst = null;
    try { inst = await h.instalador(env, pacote, sistema); } catch (e) { console.error('email ' + pacote + ': instalador', e); }
    const link = `${url.origin}/baixar/${pacote}/${sistema}`;
    const texto = [
        ...aviso,
        ...leiaMe(pacote, { email, token, sistema, nome: inst?.nome || V.nome + (sistema === 'mac' ? '.dmg' : '.exe'),
                         versao: inst?.versao, sha256: inst?.sha256, porEmail: true, link }),
    ].join('\r\n');
    return enviarEmail(env, { para: email, assunto, texto, segredos: [token] });
}

// ---------------- VASSOURA: e-mail com o link do zip e o passo a passo (mesmo conteúdo do LEIA-ME do zip) ----------------
// o link do arquivo é /baixar/vassoura/zip; o /baixar/vassoura sozinho (link antigo, site, posts) abre a página de cadastro
export const VASSOURA_ZIP = '/baixar/vassoura/zip';

// sistema: 'win', 'mac' ou '' (painel com os dois marcados: o passo a passo traz os dois)
//   assunto/abertura: o painel troca o "Obrigado por baixar" por "você recebeu acesso"
export async function emailVassoura(env, url, h, email, sistema, { assunto, aberturaPt, aberturaEn, origem } = {}) {
    if (!emailLigado(env)) return false;
    let inst = null;
    try { inst = await h.instalador(env, 'vassoura'); } catch (e) { console.error('email vassoura: instalador', e); }
    const link = 'https://insidestudiosp.com.br' + VASSOURA_ZIP,   // sempre o domínio do estúdio (o cadastro pode vir do workers.dev)
          pasta = (inst?.nome || 'Vassoura.zip').replace(/\.zip$/, '');
    const mac = sistema === 'mac', ambos = !['win', 'mac'].includes(sistema);
    const S = (win, m) => ambos ? win + ' (Mac: ' + m + ')' : mac ? m : win;
    // zip igual nos dois; muda como extrai, a pasta da User Library e o atalho de desfazer
    const pW = 'Documentos\\Ableton\\User Library\\Presets\\Audio Effects\\Max Audio Effect\\',
          pM = 'Música/Ableton/User Library/Presets/Audio Effects/Max Audio Effect/   (no Finder: ~/Music/Ableton/...)',
          eW = 'Documents\\Ableton\\User Library\\Presets\\Audio Effects\\Max Audio Effect\\',
          eM = 'Music/Ableton/User Library/Presets/Audio Effects/Max Audio Effect/   (in Finder: ~/Music/Ableton/...)';
    const extrair = ambos ? 'Extraia o zip (Windows: botão direito > "Extrair tudo..."; Mac: dois cliques no arquivo baixado).'
                  : mac ? 'Extraia o zip (dois cliques no arquivo baixado).' : 'Extraia o zip (botão direito > "Extrair tudo...").',
          extract = ambos ? 'Extract the zip (Windows: right-click > "Extract All..."; Mac: double-click the downloaded file).'
                  : mac ? 'Extract the zip (double-click the downloaded file).' : 'Extract the zip (right-click > "Extract All...").',
          pastaPt = ambos ? 'Windows: ' + pW + '\r\n       Mac:     ' + pM : mac ? pM : pW,
          pastaEn = ambos ? 'Windows: ' + eW + '\r\n       Mac:     ' + eM : mac ? eM : eW,
          desfazer = S('Ctrl+Z', 'Cmd+Z'),
          salvar = 'File > Save Live Set As..., ' + S('Ctrl+Shift+S', 'Cmd+Shift+S'),
          prefs = S('Options > Settings', 'Live > Settings'),
          nomeSo = ambos ? 'Windows e Mac' : mac ? 'Mac' : 'Windows',
          nomeEn = ambos ? 'Windows and Mac' : nomeSo;
    const texto = [
        'VASSOURA · InsideStudioSP' + (inst?.versao ? ' · v' + inst.versao : '') + ' · ' + (ambos ? 'Windows / Mac' : nomeSo),
        'Limpa os trechos vazios dos clips de áudio no Ableton Live · Cleans the empty parts of audio clips in Ableton Live',
        '',
        '(English below)',
        '',
        '==================================== PORTUGUÊS ====================================',
        '',
        (aberturaPt || 'Obrigado por baixar a VASSOURA!') + ' É um device Max for Live para o Ableton Live 12 (Suite, ou Standard com Max for Live),',
        'no Windows e no Mac. Não precisa de ativação nem de token. Este passo a passo é para ' + nomeSo + '.',
        '',
        '1. BAIXAR',
        '  ' + link,
        '',
        '2. INSTALAR',
        '  1) ' + extrair,
        '  2) Copie a pasta inteira (' + pasta + ') para:',
        '       ' + pastaPt,
        '     Os 3 arquivos (Vassoura.amxd, vassoura.js e vassoura_view.js) PRECISAM ficar juntos na mesma pasta.',
        '  3) No Live, arraste o Vassoura.amxd para a PRIMEIRA faixa de áudio do projeto (no MASTER ele não funciona).',
        '',
        '  ANTES DO PRIMEIRO APPLY, SALVE UMA CÓPIA DO SET (' + salvar + ').',
        '  Cada trecho mutado ou apagado é um ' + desfazer + ' separado no Live.',
        '',
        '3. USAR',
        '  1) Abra um set com stems (clips de áudio no Arrangement, com WARP DESLIGADO).',
        '  2) Clique em ANALYZE e depois em OPEN para ver a janela.',
        '  3) Ajuste no device (a janela redesenha na hora):',
        '     THRESHOLD (padrão -60 dB): abaixo disso é silêncio',
        '     MIN LENGTH (padrão 1000 ms): silêncio mais curto que isso fica',
        '     PRE (padrão 10 ms): folga mantida antes de cada som (não corta o ataque)',
        '     POST (padrão 100 ms): cauda mantida depois de cada som (não corta reverb e release)',
        '     SNAP: OFF / 1/16 / BAR, os cortes caem na grade, sempre dentro do silêncio',
        '     SCOPE: ALL (todas as faixas) ou SELECTED (só a faixa selecionada)',
        '     MODE: MUTE (divide o clip e desliga os pedaços vazios, nada é apagado)',
        '           CUT (apaga os pedaços vazios; pede um 2º clique no APPLY em até 4 s)',
        '  4) Na janela: cor da faixa = som (fica) · vermelho tracejado = vazio (sai) · cinza = mantido.',
        '     Clique no vermelho para manter aquele trecho. Clique no nome da faixa para tirar a faixa da limpeza.',
        '  5) Clique em APPLY. Depois a VASSOURA relê o set e a janela mostra como ficou.',
        '',
        '  Limites: clips com WARP ligado ou em LOOP e faixas congeladas (Freeze) não são tocados.',
        '  Para não dar clique nos cortes, deixe ligado em ' + prefs + ' > Record, Warp & Launch > "Create Fades on Clip Edges".',
        '',
        '4. SE DER ERRO',
        '  "v8: can\'t find vassoura.js" = os 3 arquivos não estão na mesma pasta do Vassoura.amxd.',
        '  Outro erro: abra o console do Max (editar o device > Max > Window > Max Console) e mande um print',
        '  das linhas "VASSOURA:" respondendo este e-mail ou para ' + CONTATO + '.',
        '',
        '  Não pediu? Ignore este e-mail.',
        '',
        '',
        '===================================== ENGLISH =====================================',
        '',
        (aberturaEn || 'Thanks for downloading VASSOURA!') + ' It is a Max for Live device for Ableton Live 12 (Suite, or Standard with Max for Live),',
        'on Windows and Mac. No activation or token needed. These steps are for ' + nomeEn + '.',
        '',
        '1. DOWNLOAD',
        '  ' + link,
        '',
        '2. INSTALL',
        '  1) ' + extract,
        '  2) Copy the whole folder (' + pasta + ') to:',
        '       ' + pastaEn,
        '     The 3 files (Vassoura.amxd, vassoura.js and vassoura_view.js) MUST stay together in the same folder.',
        '  3) In Live, drag Vassoura.amxd onto the FIRST audio track of the project (it does not work on the MASTER).',
        '',
        '  BEFORE THE FIRST APPLY, SAVE A COPY OF YOUR SET (' + salvar + ').',
        '  Each muted or deleted part is a separate ' + desfazer + ' in Live.',
        '',
        '3. USE',
        '  1) Open a set with stems (audio clips in the Arrangement, WARP OFF).',
        '  2) Click ANALYZE, then OPEN to see the window.',
        '  3) Adjust on the device (the window redraws instantly):',
        '     THRESHOLD (default -60 dB): below this is silence',
        '     MIN LENGTH (default 1000 ms): silence shorter than this stays',
        '     PRE (default 10 ms): room kept before each sound (attacks are not cut)',
        '     POST (default 100 ms): tail kept after each sound (reverb and release are not cut)',
        '     SNAP: OFF / 1/16 / BAR, cuts land on the grid, always inside the silence',
        '     SCOPE: ALL (every track) or SELECTED (selected track only)',
        '     MODE: MUTE (splits the clip and turns off the empty parts, nothing is deleted)',
        '           CUT (deletes the empty parts; asks for a 2nd APPLY click within 4 s)',
        '  4) In the window: track color = sound (stays) · hatched red = empty (goes) · gray = kept.',
        '     Click the red to keep that part. Click the track name to leave the track out.',
        '  5) Click APPLY. VASSOURA then rereads the set and the window shows the result.',
        '',
        '  Limits: clips with WARP on or LOOP on and frozen tracks are not touched.',
        '  To avoid clicks at the cuts, keep ' + prefs + ' > Record, Warp & Launch > "Create Fades on Clip Edges" on.',
        '',
        '4. IF SOMETHING GOES WRONG',
        '  "v8: can\'t find vassoura.js" = the 3 files are not in the same folder as Vassoura.amxd.',
        '  Other errors: open the Max console (edit the device > Max > Window > Max Console) and send a screenshot',
        '  of the "VASSOURA:" lines by replying to this e-mail or writing to ' + CONTATO + '.',
        '',
        '  Did not ask for it? Just ignore this e-mail.',
        '',
        'InsideStudioSP',
    ].join('\r\n');
    return enviarEmail(env, { para: email, assunto: assunto || 'Seu download da VASSOURA · Your VASSOURA download', texto, origem });
}

// ---------------- "Não recebi / perdi meu token" (só com o e-mail ligado) ----------------
// O token fica guardado só como hash, então "reenviar" é gerar um token NOVO. Para ninguém trocar o token de outra
// pessoa digitando o e-mail dela, a troca só acontece pelo link que vai PARA o e-mail (vale 1 h, uma vez só), e o
// link abre uma página com botão (POST): leitor de link do provedor de e-mail que abre o GET não troca nada.
//   POST /gratis/align/reenviar  { email, site }  -> sempre a mesma resposta (não revela se o e-mail tem cadastro)
//   GET  /gratis/align/novo-token?c=…             -> página com o botão GERAR NOVO TOKEN
//   POST /gratis/align/novo-token  c=…            -> troca o token do e-mail (todos os pacotes), mostra e manda por e-mail

const REENVIO_VALIDADE = 3600;
const REENVIO_POR_EMAIL_DIA = 3, REENVIO_POR_IP_DIA = 10, REENVIO_INTERVALO = 120;

export async function rotaTokenVst(req, env, url, h, acao, pacote) {
    const V = VST_GRATIS[pacote];
    if (!emailLigado(env)) return h.paginaSimples('Indisponível', 'Chame a InsideStudioSP no direct do Instagram.', 404);
    const ip = req.headers.get('cf-connecting-ip') || '';
    if (acao === 'reenviar') {
        if (req.method !== 'POST') return json({ ok: false, mensagem: 'Método não aceito.' }, 405);
        let corpo;
        try { corpo = await req.json(); } catch { return json({ ok: false, mensagem: 'Pedido inválido.' }, 400); }
        const email = h.normEmail(corpo.email);
        if (email.length > 120 || !/^[^@\s]+@[^@\s]+\.[^@\s]+$/.test(email)) return json({ ok: false, mensagem: 'Confira o e-mail.' }, 400);
        const pronto = json({ ok: true, mensagem: 'Se este e-mail tiver o ' + V.nome + ', o link chega em alguns minutos (olhe também o spam e Promoções). O link vale por 1 hora.' });
        if (corpo.site) return pronto;
        const desde = h.agora() - 86400;
        const nIp = await env.DB.prepare("SELECT COUNT(*) AS n FROM eventos WHERE tipo = 'reenvio_pedido' AND ip = ? AND quando > ?").bind(ip, desde).first();
        if ((nIp?.n || 0) >= REENVIO_POR_IP_DIA) return json({ ok: false, mensagem: 'Muitos pedidos agora. Tente de novo amanhã ou chame a gente no direct.' }, 429);
        const lic = await env.DB.prepare("SELECT id FROM licencas WHERE email = ? AND pacote = ?").bind(email, pacote).first();
        if (!lic) { await h.evento(env, null, 'reenvio_pedido', 'sem ' + V.nome, ip); return pronto; }
        const ult = await env.DB.prepare("SELECT COUNT(*) AS n, MAX(quando) AS q FROM eventos WHERE tipo = 'reenvio_pedido' AND licenca_id = ? AND quando > ?")
            .bind(lic.id, desde).first();
        if ((ult?.n || 0) >= REENVIO_POR_EMAIL_DIA || (ult?.q && h.agora() - ult.q < REENVIO_INTERVALO)) return pronto;
        const c = [...crypto.getRandomValues(new Uint8Array(24))].map((b) => b.toString(16).padStart(2, '0')).join('');
        await h.evento(env, lic.id, 'reenvio_pedido', await h.sha256Hex('REENVIO' + c), ip);
        const link = `${url.origin}/gratis/${pacote}/novo-token?c=${c}`;
        await enviarEmail(env, { para: email, assunto: `Novo token do ${V.nome} · New ${V.nome} token`, texto: [
            `Alguém (provavelmente você) pediu um token novo do ${V.nome} para este e-mail.`,
            'Para gerar o token novo, abra o link abaixo e clique em GERAR NOVO TOKEN (vale por 1 hora, uma vez só):',
            '', link, '',
            'Atenção: o token antigo deixa de valer. Computador já ativado precisa ser ativado de novo com o token novo.',
            'Não pediu? Ignore este e-mail: nada muda.',
            'Dúvidas? Responda este e-mail ou escreva para ' + CONTATO + '.',
            '', '==================================== ENGLISH ====================================', '',
            `Someone (probably you) asked for a new ${V.nome} token for this e-mail.`,
            'To get the new token, open the link below and click GERAR NOVO TOKEN (valid for 1 hour, once):',
            '', link, '',
            'Note: the old token stops working. A computer already activated must be activated again with the new token.',
            'Did not ask for it? Ignore this e-mail: nothing changes.',
            'Questions? Reply to this e-mail or write to ' + CONTATO + '.',
            '', 'InsideStudioSP',
        ].join('\r\n'), segredos: [c] });
        return pronto;
    }

    // novo-token
    const c = req.method === 'POST' ? String((await req.formData().catch(() => null))?.get('c') || '') : String(url.searchParams.get('c') || '');
    const expirado = () => h.paginaSimples('Link vencido', 'Este link já foi usado ou passou de 1 hora. Volte à página do ' + V.nome + ' e peça de novo em "Perdi meu token". · This link was used or expired.', 410);
    if (!/^[0-9a-f]{48}$/.test(c)) return expirado();
    const hc = await h.sha256Hex('REENVIO' + c);
    const ped = await env.DB.prepare("SELECT licenca_id FROM eventos WHERE tipo = 'reenvio_pedido' AND detalhe = ? AND quando > ?")
        .bind(hc, h.agora() - REENVIO_VALIDADE).first();
    const usado = ped && await env.DB.prepare("SELECT id FROM eventos WHERE tipo = 'reenvio_usado' AND detalhe = ?").bind(hc).first();
    const lic = ped && !usado && await env.DB.prepare("SELECT id, email, nota FROM licencas WHERE id = ? AND pacote = ?").bind(ped.licenca_id, pacote).first();
    if (!lic) return expirado();
    if (req.method !== 'POST') return paginaToken({ c, pacote });

    await h.evento(env, lic.id, 'reenvio_usado', hc, ip);
    const token = h.novoToken();
    // licença de TESTE tem token próprio (index.js criarLicenca): fica de fora
    await env.DB.prepare("UPDATE licencas SET token_hash = ? WHERE email = ? AND origem <> 'teste'").bind(await h.sha256Hex('ISP' + h.normToken(token)), lic.email).run();
    await h.evento(env, lic.id, 'novo_token', 'pelo e-mail (perdi meu token)', ip);
    const sistema = /Mac$/.test(lic.nota || '') ? 'mac' : 'win';
    await emailVst(env, url, h, { pacote, email: lic.email, token, sistema, assunto: `Seu novo token do ${V.nome} · Your new ${V.nome} token`,
        aviso: ['O token antigo deixou de valer. · The old token no longer works.', ''] });
    return paginaToken({ email: lic.email, token, sistema, pacote });
}

function paginaToken({ c, email, token, sistema, pacote }) {
    const V = VST_GRATIS[pacote];
    const esc = (s) => String(s).replace(/[&<>"]/g, (x) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[x]));
    const corpo = c
        ? `<h2>Gerar novo token do ${V.nome}</h2>
<p>O token antigo deixa de valer. Computador já ativado precisa ser ativado de novo com o token novo.</p>
<p class="en">The old token stops working. A computer already activated must be activated again with the new token.</p>
<form method="post"><input type="hidden" name="c" value="${esc(c)}"><button>GERAR NOVO TOKEN</button></form>`
        : `<h2>Pronto! Seu novo token</h2>
<p>E-mail: <b>${esc(email)}</b></p><div class="token">${esc(token)}</div>
<p>Também mandamos para o seu e-mail. · We also sent it to your e-mail.</p>
<form method="post" action="/gratis/${pacote}/zip"><input type="hidden" name="email" value="${esc(email)}"><input type="hidden" name="token" value="${esc(token)}">
<input type="hidden" name="sistema" value="${sistema}"><button>BAIXAR ${V.nome} PARA ${sistema === 'mac' ? 'MAC' : 'WINDOWS'}</button></form>
<p class="en">O zip traz o instalador e o LEIA-ME com o token. · The zip has the installer and a READ ME with your token.</p>`;
    return new Response(`<!doctype html><html lang="pt-BR"><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1">
<title>${V.nome} · InsideStudioSP</title><style>
body { margin:0; background:#100e17; color:#eeecf6; font:16px/1.5 "Segoe UI", system-ui, -apple-system, Arial, sans-serif; }
main { max-width:520px; margin:0 auto; padding:36px 20px; } .en { color:#8d8aa3; font-size:14px; font-style:italic; }
.token { font:700 19px Consolas, "SF Mono", monospace; border:2px dashed #a78bfa; color:#a78bfa; border-radius:10px; padding:14px; text-align:center; word-break:break-all; user-select:all; }
button { width:100%; padding:15px; margin-top:16px; border:0; border-radius:12px; background:#34d399; color:#0b2316; font:900 17px "Segoe UI", system-ui, sans-serif; cursor:pointer; }
</style><main>${corpo}</main></html>`, {
        headers: { 'content-type': 'text/html; charset=utf-8', 'cache-control': 'no-store', 'referrer-policy': 'no-referrer',
                   'content-security-policy': "default-src 'none'; style-src 'unsafe-inline'; base-uri 'none'; form-action 'self'; frame-ancestors 'none'" },
    });
}

// ---------------- zip dos VSTs grátis: instalador + LEIA-ME gerado na hora ----------------
// POST /gratis/<align|barebass>/zip (formulário da página, depois do cadastro): email, token (vazio se o e-mail já tinha token),
// sistema. O token só entra no texto se conferir com a licença do e-mail (ninguém monta um leia-me com token alheio).
// Sem o CRC do instalador (release sem .crc32 e fora de CRC_CONHECIDOS) cai no download direto do instalador.

export async function rotaZipVst(req, env, h, pacote) {
    const V = VST_GRATIS[pacote];
    if (req.method !== 'POST') return h.paginaSimples('Link inválido', 'Abra a página do ' + V.nome + ' e gere a sua licença.', 405);
    let f;
    try { f = await req.formData(); } catch { return h.paginaSimples('Pedido inválido', 'Volte à página do ' + V.nome + ' e tente de novo.', 400); }
    const email = h.normEmail(f.get('email'));
    const sistema = f.get('sistema') === 'mac' ? 'mac' : 'win';
    const tokenTxt = String(f.get('token') || '').trim();
    const lic = await env.DB.prepare("SELECT id, token_hash FROM licencas WHERE email = ? AND pacote = ?").bind(email, pacote).first();
    if (!lic) return h.paginaSimples('E-mail sem ' + V.nome, 'Este e-mail não tem licença do ' + V.nome + '. Volte à página e gere a sua.', 404);
    let token = '';
    if (tokenTxt) {
        if (await h.sha256Hex('ISP' + h.normToken(tokenTxt)) !== lic.token_hash)
            return h.paginaSimples('Token não confere', 'Volte à página do ' + V.nome + ' e baixe pelo botão que apareceu junto com o seu token.', 403);
        token = tokenTxt.toUpperCase();
    }
    const inst = await h.instalador(env, pacote, sistema);
    if (!inst) return h.paginaSimples('Download indisponível', 'O instalador não está disponível agora. Tente de novo mais tarde.', 404);
    if (inst.crc == null || !inst.tamanho) {
        console.error('zip ' + pacote + ': sem crc', inst.nome, inst.sha256);
        return Response.redirect(inst.link, 302);
    }
    const r = await fetch(inst.link, { headers: { 'user-agent': 'isp-licencas' } });
    const len = Number(r.headers.get('content-length') || inst.tamanho);
    if (!r.ok || !r.body || len !== inst.tamanho) {
        console.error('zip ' + pacote + ': instalador', r.status, len, inst.tamanho);
        return Response.redirect(inst.link, 302);
    }
    const texto = new TextEncoder().encode('\uFEFF' + leiaMe(pacote, { email, token, sistema, ...inst, link: '' }).join('\r\n') + '\r\n');
    const zip = zipStream([
        { nome: 'LEIA-ME - READ ME.txt', bytes: texto },
        { nome: inst.nome, tamanho: inst.tamanho, crc: inst.crc, corpo: r.body },
    ]);
    await h.evento(env, lic.id, 'gratis_zip', sistema, req.headers.get('cf-connecting-ip') || '');
    return new Response(zip.stream, {
        headers: {
            'content-type': 'application/zip', 'content-length': String(zip.tamanho), 'cache-control': 'no-store',
            'content-disposition': `attachment; filename="${V.nome}-${inst.versao || 'InsideStudioSP'}-${sistema === 'mac' ? 'Mac' : 'Windows'}.zip"`,
        },
    });
}

// porEmail: o e-mail leva o link direto do instalador (sem zip), então os passos falam do arquivo e não do zip
function leiaMe(pacote, { email, token, sistema, nome, versao, sha256, porEmail = false, link = '' }) {
    const V = VST_GRATIS[pacote];
    const mac = sistema === 'mac';
    const pt = porEmail ? 'Guarde este e-mail' : 'Guarde este arquivo', en = porEmail ? 'Keep this e-mail' : 'Keep this file';
    const L = [
        V.nome + ' · InsideStudioSP' + (versao ? ' · v' + versao : '') + ' · ' + (mac ? 'Mac' : 'Windows'),
        V.sub,
        '',
        '(English below)',
        '',
        '==================================== PORTUGUÊS ====================================',
        '',
        'SEUS DADOS DE ATIVAÇÃO',
        '  E-mail: ' + email,
        token ? '  Token:  ' + token
              : '  Token:  use o token que você já recebeu da InsideStudioSP (o mesmo deste e-mail).',
        '  Licença para 1 computador. ' + pt + ': o token não aparece de novo.',
        '',
        '1. INSTALAR',
        ...(link ? ['  Baixe o instalador: ' + link] : []),
    ];
    if (mac) L.push(
        porEmail ? '  1) Dê dois cliques em ' + nome + ' (na pasta Downloads).'
                 : '  1) Abra o zip (o Safari já costuma abrir sozinho) e dê dois cliques em ' + nome + '.',
        '  2) Na janela que abrir, dê dois cliques em "Instalar InsideStudioSP ' + V.pkg + '.pkg" e siga o instalador.',
        '     Os plugins vão para as pastas padrão do Mac (VST3 e AU em /Library/Audio/Plug-Ins).',
        '  3) Feche e abra de novo o seu DAW. No Logic, se o ' + V.nome + ' não aparecer: Logic Pro > Ajustes >',
        '     Plug-in Manager > "Redefinir e verificar".',
    );
    else L.push(
        ...(porEmail ? [] : ['  1) Extraia o zip: botão direito no arquivo > "Extrair tudo..." > Extrair.',
                             '     Não rode o instalador de dentro do zip.']),
        '  ' + (porEmail ? 1 : 2) + ') Rode ' + nome + ' e siga o instalador. A pasta já vem certa',
        '     (C:\\Program Files\\Common Files\\VST3, onde o Ableton e os outros DAWs procuram).',
        '  ' + (porEmail ? 2 : 3) + ') Feche e abra de novo o seu DAW (se o ' + V.nome + ' não aparecer, mande o DAW procurar plugins de novo).',
    );
    L.push(
        '',
        V.usoTitPt,
        ...V.usoPt,
        '',
        '3. ATIVAR',
        '  1) Na tela de ativação do ' + V.nome + ', digite o e-mail e o token acima e clique em ATIVAR.',
        '     Precisa de internet só nessa hora.',
        V.prontoPt,
        '  3) Vai trocar de computador? Menu ≡ do plugin > Licença... > Desativar este computador, e ative no novo.',
        '',
        mac ? '4. SE O MAC DISSER QUE NÃO PODE ABRIR O INSTALADOR' : '4. SE O WINDOWS OU O ANTIVÍRUS DER AVISO DE SEGURANÇA',
        mac ? '  O ' + V.nome + ' ainda não é registrado na Apple, então o Mac avisa que não pode verificar o desenvolvedor.'
            : '  O ' + V.nome + ' ainda não tem assinatura digital paga, então o Windows não conhece o "editor" e pode avisar.',
        '  É o aviso padrão para programa novo e sem assinatura, não quer dizer que o arquivo tenha vírus.',
        '',
    );
    if (mac) L.push(
        '  a) "Não é possível abrir porque a Apple não pode verificá-lo" / "desenvolvedor não identificado":',
        '     clique em OK, abra Ajustes do Sistema > Privacidade e Segurança, role até o aviso do instalador,',
        '     clique em "Abrir Mesmo Assim" e confirme com a sua senha. Depois abra o instalador de novo.',
        '     (macOS 14 ou anterior: Control + clique no instalador > Abrir > Abrir.)',
        '  b) "Está danificado e não pode ser aberto": abra o Terminal e rode (troque o caminho se baixou em outro lugar):',
        '        xattr -dr com.apple.quarantine ~/Downloads/' + nome,
        '     Depois abra o ' + nome + ' de novo.',
    );
    else L.push(
        '  a) Tela azul "O Windows protegeu o computador" (SmartScreen):',
        '     clique em "Mais informações" e depois em "Executar assim mesmo".',
        porEmail ? '  b) Para o aviso nem aparecer: botão direito no ' + nome + ' > Propriedades >'
                 : '  b) Para o aviso nem aparecer: ANTES de extrair, botão direito no zip > Propriedades >',
        porEmail ? '     marque "Desbloquear" (embaixo, em Segurança) > OK. Depois rode o instalador.'
                 : '     marque "Desbloquear" (embaixo, em Segurança) > OK. Depois extraia e rode o instalador.',
        '  c) O navegador barrou o download (Chrome/Edge: "pode ser perigoso" / "não é baixado com frequência"):',
        '     abra a lista de downloads, clique nos "..." do arquivo > Manter > Manter assim mesmo.',
        '  d) O Microsoft Defender apagou o arquivo ou disse "Ameaça encontrada":',
        '     Iniciar > Segurança do Windows > Proteção contra vírus e ameaças > Histórico de proteção >',
        '     abra o item do ' + nome + ' > Ações > Restaurar (ou "Permitir no dispositivo"). Depois rode de novo.',
        '     Outro antivírus (Avast, AVG, Norton...): restaure o arquivo da quarentena / adicione como exceção.',
        '     Não precisa desligar o antivírus: libere só este arquivo.',
    );
    if (sha256) L.push(
        '',
        '  Quer conferir se o arquivo é o original? Ele tem que ter este SHA-256:',
        '     ' + sha256,
        mac ? '     No Terminal: shasum -a 256 ~/Downloads/' + nome
            : '     No PowerShell, na pasta do arquivo: Get-FileHash .\\' + nome,
    );
    L.push(
        '',
        '  Dúvidas ou dificuldade? ' + (porEmail ? 'Responda este e-mail ou escreva' : 'Escreva') + ' para ' + CONTATO + '.',
        ...(porEmail ? ['  Não pediu esta licença? Ignore este e-mail.'] : []),
        '',
        '',
        '===================================== ENGLISH =====================================',
        '',
        'YOUR ACTIVATION DATA',
        '  E-mail: ' + email,
        token ? '  Token:  ' + token
              : '  Token:  use the token you already received from InsideStudioSP (same one for this e-mail).',
        '  License for 1 computer. ' + en + ': the token is not shown again.',
        '',
        '1. INSTALL',
        ...(link ? ['  Download the installer: ' + link] : []),
    );
    if (mac) L.push(
        porEmail ? '  1) Double-click ' + nome + ' (in your Downloads folder).'
                 : '  1) Open the zip (Safari usually opens it by itself) and double-click ' + nome + '.',
        '  2) In the window that opens, double-click "Instalar InsideStudioSP ' + V.pkg + '.pkg" and follow the installer.',
        '     The plugins go to the standard Mac folders (VST3 and AU in /Library/Audio/Plug-Ins).',
        '  3) Close and reopen your DAW. In Logic, if ' + V.nome + ' does not show up: Logic Pro > Settings >',
        '     Plug-in Manager > "Reset & Rescan Selection".',
    );
    else L.push(
        ...(porEmail ? [] : ['  1) Extract the zip: right-click the file > "Extract All..." > Extract.',
                             '     Do not run the installer from inside the zip.']),
        '  ' + (porEmail ? 1 : 2) + ') Run ' + nome + ' and follow the installer. The folder is already right',
        '     (C:\\Program Files\\Common Files\\VST3, where Ableton and the other DAWs look).',
        '  ' + (porEmail ? 2 : 3) + ') Close and reopen your DAW (if ' + V.nome + ' does not show up, rescan your plugins).',
    );
    L.push(
        '',
        V.usoTitEn,
        ...V.usoEn,
        '',
        '3. ACTIVATE',
        '  1) On the ' + V.nome + ' activation screen, type the e-mail and token above and click ATIVAR (activate).',
        '     Internet is needed only at this moment.',
        V.prontoEn,
        '  3) Changing computers? Plugin menu ≡ > Licença... > Desativar este computador (deactivate), then activate on the new one.',
        '',
        mac ? '4. IF YOUR MAC SAYS IT CANNOT OPEN THE INSTALLER' : '4. IF WINDOWS OR YOUR ANTIVIRUS SHOWS A SECURITY WARNING',
        mac ? '  ' + V.nome + ' is not registered with Apple yet, so macOS says it cannot verify the developer.'
            : '  ' + V.nome + ' does not have a paid digital signature yet, so Windows does not know the "publisher" and may warn you.',
        '  This is the standard warning for new unsigned software, it does not mean the file has a virus.',
        '',
    );
    if (mac) L.push(
        '  a) "Cannot be opened because Apple cannot check it" / "unidentified developer":',
        '     click OK, open System Settings > Privacy & Security, scroll to the installer message,',
        '     click "Open Anyway" and confirm with your password. Then open the installer again.',
        '     (macOS 14 or older: Control-click the installer > Open > Open.)',
        '  b) "Is damaged and can\'t be opened": open Terminal and run (change the path if you saved it elsewhere):',
        '        xattr -dr com.apple.quarantine ~/Downloads/' + nome,
        '     Then open ' + nome + ' again.',
    );
    else L.push(
        '  a) Blue screen "Windows protected your PC" (SmartScreen):',
        '     click "More info" and then "Run anyway".',
        porEmail ? '  b) To avoid the warning: right-click ' + nome + ' > Properties >'
                 : '  b) To avoid the warning: BEFORE extracting, right-click the zip > Properties >',
        porEmail ? '     tick "Unblock" (bottom, under Security) > OK. Then run the installer.'
                 : '     tick "Unblock" (bottom, under Security) > OK. Then extract and run the installer.',
        '  c) The browser blocked the download (Chrome/Edge: "may be dangerous" / "not commonly downloaded"):',
        '     open the downloads list, click the "..." of the file > Keep > Keep anyway.',
        '  d) Microsoft Defender deleted the file or said "Threat found":',
        '     Start > Windows Security > Virus & threat protection > Protection history >',
        '     open the ' + nome + ' item > Actions > Restore (or "Allow on device"). Then run it again.',
        '     Other antivirus (Avast, AVG, Norton...): restore the file from quarantine / add it as an exception.',
        '     No need to turn your antivirus off: just allow this file.',
    );
    if (sha256) L.push(
        '',
        '  Want to check it is the original file? It must have this SHA-256:',
        '     ' + sha256,
        mac ? '     In Terminal: shasum -a 256 ~/Downloads/' + nome
            : '     In PowerShell, in the file folder: Get-FileHash .\\' + nome,
    );
    L.push('', '  Questions or trouble? ' + (porEmail ? 'Reply to this e-mail or write' : 'Write') + ' to ' + CONTATO + '.',
        ...(porEmail ? ['  Did not ask for this license? Just ignore this e-mail.'] : []), '');
    return L;
}

// topo e rodapé com os links do site (as páginas grátis podem abrir pelo workers.dev: links sempre absolutos no domínio do estúdio)
const SITE_URL = 'https://insidestudiosp.com.br';
const NAV = `<style>
  .navsite { display:flex; align-items:center; justify-content:space-between; gap:12px; flex-wrap:wrap; max-width:760px; margin:0 auto; padding:14px 20px;
             border-bottom:1px solid var(--line); font-size:14px; }
  .navsite a { color:var(--dim); text-decoration:none; font-weight:600; }
  .navsite a:hover, .rodsite a:hover { color:var(--ink); }
  .navsite .marca { color:var(--ink); font-weight:800; letter-spacing:.02em; }
  .navsite nav { display:flex; gap:18px; flex-wrap:wrap; }
  .rodsite { color:var(--dim); font-size:13px; text-align:center; margin-top:30px; line-height:1.9; }
  .rodsite a { color:var(--dim); }
  .rodsite .links a { margin:0 9px; white-space:nowrap; }
</style>
<header class="navsite">
  <a class="marca" href="${SITE_URL}/">Inside<b>Studio</b>SP</a>
  <nav><a href="${SITE_URL}/">Loja · Shop</a><a href="${SITE_URL}/estudio/">Estúdio · Studio</a><a href="${SITE_URL}/contato/">Contato · Contact</a></nav>
</header>`;
const RODAPE = `<footer class="rodsite">
    <div class="links"><a href="${SITE_URL}/">Loja</a><a href="${SITE_URL}/#packs">Packs</a><a href="${SITE_URL}/estudio/">Estúdio</a><a href="${SITE_URL}/contato/">Contato</a><a href="${SITE_URL}/privacidade/">Privacidade</a></div>
    <div>InsideStudioSP · <a href="mailto:${CONTATO}">${CONTATO}</a></div>
  </footer>`;

const PAGINA = `<!doctype html>
<html lang="pt-BR"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1">
<title>MONTA-MONTA grátis · InsideStudioSP</title>
<style>
  :root { --bg:#0e0f15; --card:#1b1d26; --line:#2c303d; --ink:#e9ecf5; --dim:#8d93a8; --vco:#ff8a2a; --vcf:#22d8ff; --amp:#5fe36a; --out:#ff4f8b; --gold:#ffd23a; }
  * { box-sizing:border-box; }
  body { margin:0; min-height:100vh; background:radial-gradient(700px 500px at 85% 0%, rgba(34,216,255,.12), transparent 70%),
         radial-gradient(700px 500px at 0% 100%, rgba(255,138,42,.10), transparent 70%), var(--bg);
         color:var(--ink); font:16px/1.5 "Segoe UI", system-ui, -apple-system, Arial, sans-serif; }
  main { max-width:520px; margin:0 auto; padding:36px 20px 60px; }
  .logo { display:flex; align-items:center; gap:14px; }
  .q { display:grid; grid-template-columns:22px 22px; gap:3px; }
  .q i { width:22px; height:22px; border-radius:5px; display:block; }
  h1 { margin:0; white-space:nowrap; font:900 clamp(26px, 8.4vw, 38px)/1 "Segoe UI Black", "Segoe UI", system-ui, sans-serif; letter-spacing:-.5px; }
  h1 .a { color:var(--vco); } h1 .b { color:var(--vcf); }
  .sub { color:var(--dim); font-size:13px; letter-spacing:.12em; font-weight:700; margin-top:6px; }
  .card { background:var(--card); border:1px solid var(--line); border-radius:18px; padding:24px; margin-top:26px; }
  .free { display:inline-block; background:var(--gold); color:#111318; font-weight:900; padding:2px 10px; border-radius:6px; }
  p { margin:.4em 0; } .en { color:var(--dim); font-size:14px; font-style:italic; }
  label { display:block; font-weight:700; margin:18px 0 6px; }
  input[type=email] { width:100%; padding:14px; border-radius:10px; border:2px solid var(--line); background:#0b0c11; color:var(--ink); font-size:17px; }
  input[type=email]:focus { outline:none; border-color:var(--vcf); }
  button, .bt { display:block; width:100%; text-align:center; padding:15px; margin-top:16px; border:0; border-radius:12px; cursor:pointer;
           font:900 17px "Segoe UI", system-ui, sans-serif; letter-spacing:.04em; text-decoration:none; }
  .principal { background:var(--amp); color:#0b0c11; }
  .principal:disabled { opacity:.6; cursor:wait; }
  .sec { background:#0b0c11; color:var(--ink); border:2px solid var(--line); }
  .sec:hover { border-color:var(--vcf); }
  .token { font:700 clamp(14px, 4.3vw, 19px) Consolas, "SF Mono", monospace; background:#0b0c11; border:2px dashed var(--gold); border-radius:10px; padding:14px; text-align:center;
           word-break:break-all; color:var(--gold); margin-top:8px; user-select:all; }
  .aviso { background:rgba(255,210,58,.08); border:1px solid rgba(255,210,58,.4); border-radius:10px; padding:10px 12px; font-size:14px; margin-top:12px; }
  .erro { color:var(--out); font-weight:700; min-height:1.5em; margin-top:10px; }
  ol { padding-left:20px; margin:8px 0 0; } li { margin:6px 0; }
  .dois { display:grid; grid-template-columns:1fr 1fr; gap:10px; } .dois .bt { margin-top:10px; }
  .esc { position:absolute; left:-5000px; }
  footer { color:var(--dim); font-size:12px; text-align:center; margin-top:26px; }
  [hidden] { display:none !important; }
</style></head>
<body>${NAV}<main>
  <div class="logo"><div class="q"><i style="background:#ff8a2a"></i><i style="background:#22d8ff"></i><i style="background:#5fe36a"></i><i style="background:#ff4f8b"></i></div>
    <div><h1><span class="a">MONTA</span>-<span class="b">MONTA</span></h1><div class="sub">INSIDE STUDIO SP · SYNTH DIDÁTICO</div></div></div>

  <section class="card" id="passo1">
    <p><span class="free">GRÁTIS</span> &nbsp;Monte um sintetizador peça por peça e aprenda o caminho do som. VST3 e AU, Windows e Mac.</p>
    <p class="en">Free learning synth: build it piece by piece. VST3 / AU, Windows and Mac.</p>
    <p class="en">Requisitos · Requirements: Windows 10 ou 11 (64 bits) · macOS 11 Big Sur ou mais novo (Apple Silicon e Intel).</p>
    <form id="f" novalidate>
      <label for="email">Seu e-mail <span class="en">· your e-mail</span></label>
      <input id="email" type="email" autocomplete="email" required placeholder="voce@email.com">
      <input class="esc" name="site" id="site" tabindex="-1" autocomplete="off" aria-hidden="true">
      <button class="principal" id="ir" type="submit">RECEBER MONTA-MONTA</button>
      <div class="erro" id="erro"></div>
    </form>
    <p class="en" style="margin-top:8px">O e-mail é só para a sua licença. · Your e-mail is used only for your license.</p>
  </section>

  <section class="card" id="passo2" hidden>
    <div id="comToken" hidden>
      <p><b>Pronto! Este é o seu token:</b> <span class="en">· your token</span></p>
      <div class="token" id="token"></div>
      <button class="sec" id="copiar" type="button">COPIAR TOKEN</button>
      <div class="aviso"><b>Guarde o token</b>: ele aparece só agora (tire um print). <span class="en">It is shown only once, save it.</span></div>
    </div>
    <div id="semToken" hidden>
      <p id="txtSem"></p>
    </div>
    <label>1. Baixe e instale <span class="en">· download and install</span></label>
    <div class="dois"><a class="bt principal" id="win" href="#">WINDOWS</a><a class="bt principal" id="mac" href="#">MAC</a></div>
    <label>2. Ative <span class="en">· activate</span></label>
    <ol>
      <li>Abra o MONTA-MONTA no seu DAW (Ableton, FL, Logic, Reaper…).</li>
      <li>Na tela de ativação, digite <b id="emailOk"></b> e o token, e clique em <b>ATIVAR</b>.</li>
      <li>Vale para 2 computadores. Para trocar de PC: menu do plugin → <b>Licença…</b> → <b>Desativar este computador</b>.</li>
    </ol>
    <p class="en">Open it in your DAW, type your e-mail and token, click ATIVAR. Works on 2 computers.</p>
  </section>
  ${RODAPE}
</main>
<script>
(function () {
  var $ = function (id) { return document.getElementById(id); };
  $('f').addEventListener('submit', function (ev) {
    ev.preventDefault();
    var email = $('email').value.trim();
    $('erro').textContent = '';
    if (!/^[^@\\s]+@[^@\\s]+\\.[^@\\s]+$/.test(email)) { $('erro').textContent = 'Confira o e-mail.'; return; }
    $('ir').disabled = true; $('ir').textContent = 'GERANDO…';
    fetch(location.pathname, { method: 'POST', headers: { 'content-type': 'application/json' },
      body: JSON.stringify({ email: email, site: $('site').value }) })
      .then(function (r) { return r.json(); })
      .then(function (j) {
        $('ir').disabled = false; $('ir').textContent = 'RECEBER MONTA-MONTA';
        if (!j.ok) { $('erro').textContent = j.mensagem || 'Não deu certo. Tente de novo.'; return; }
        $('win').href = j.baixar.win; $('mac').href = j.baixar.mac; $('emailOk').textContent = email.toLowerCase();
        if (j.token) { $('token').textContent = j.token; $('comToken').hidden = false; }
        else {
          $('txtSem').innerHTML = j.ja_tinha
            ? '<b>Este e-mail já recebeu o MONTA-MONTA.</b> Use o token que apareceu da primeira vez. Perdeu? Chame a gente no direct.'
            : '<b>Pronto! O MONTA-MONTA entrou no seu cadastro.</b> Use o mesmo token que você já recebeu da InsideStudioSP.';
          $('semToken').hidden = false;
        }
        $('passo1').hidden = true; $('passo2').hidden = false; window.scrollTo(0, 0);
      })
      .catch(function () { $('ir').disabled = false; $('ir').textContent = 'RECEBER MONTA-MONTA'; $('erro').textContent = 'Sem conexão. Tente de novo.'; });
  });
  $('copiar').addEventListener('click', function () {
    var t = $('token').textContent;
    (navigator.clipboard ? navigator.clipboard.writeText(t) : Promise.reject()).then(function () { $('copiar').textContent = 'COPIADO ✓'; },
      function () { var s = window.getSelection(), r = document.createRange(); r.selectNodeContents($('token')); s.removeAllRanges(); s.addRange(r); });
  });
})();
</script>
</body></html>`;

// ── ALIGN e VASSOURA no molde do site (pedido dele, 06/11/2026) ─────────────────────────────────────────────
// Mesmo topo, rodapé, folha de estilo e troca PT/EN do site (estilo.css e site.js vêm do domínio do estúdio); nome, frase,
// texto, cor e capa vêm de site/dados.mjs, o mesmo arquivo da página de pack. O cadastro fica na caixa lateral, no lugar
// da caixa de compra. Embaixo, a vitrine com os packs pagos (preço de precos.js; o "de" riscado só enquanto a oferta vale).
// O MONTA-MONTA (PAGINA, acima) continua na página antiga, de propósito.
const esc = (s) => String(s).replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;');
const t = (pt, en) => `<span class="pt">${esc(pt)}</span><span class="en" lang="en">${esc(en ?? pt)}</span>`;
const th = (pt, en) => `<span class="pt">${pt}</span><span class="en" lang="en">${en}</span>`;   // com HTML dentro (texto fixo daqui)
const brl = (v) => 'R$ ' + v.toFixed(2).replace('.', ',');
const REQ_VST = th('Requisitos: Windows 10 ou 11 (64 bits) · macOS 11 Big Sur ou mais novo (Apple Silicon e Intel)',
                   'Requirements: Windows 10 or 11 (64-bit) · macOS 11 Big Sur or newer (Apple Silicon and Intel)');
const CSP_SITE = `default-src 'none'; style-src 'unsafe-inline' 'self' ${SITE_URL}; script-src 'unsafe-inline' 'self' ${SITE_URL}; connect-src 'self'; `
    + `img-src data: 'self' ${SITE_URL}; base-uri 'none'; frame-ancestors 'none'`;

function oferta(agora) {
    const ativa = agora < Date.parse(OFERTA_FIM);
    const offs = Object.keys(PRECOS).map((id) => precoAtual(id, agora)?.brl).filter((v) => ativa && DE_BRL[v]).map((v) => Math.round((1 - v / DE_BRL[v]) * 100));
    return { ativa, maior: offs.length ? Math.max(...offs) : 0 };
}

function paginaSite({ titulo, corpo, agora }) {
    const of = oferta(agora);
    const fim = new Date(OFERTA_FIM).toLocaleDateString('pt-BR', { timeZone: 'America/Sao_Paulo', day: '2-digit', month: '2-digit' });
    return `<!doctype html>
<html lang="pt-BR"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1">
<title>${esc(titulo)}</title>
<meta name="theme-color" content="#0d0b14">
<link rel="icon" href="${SITE_URL}/favicon.svg" type="image/svg+xml">
<link rel="stylesheet" href="${SITE_URL}/estilo.css">
<script src="${SITE_URL}/site.js" defer></script>
<style>
  .compra .so { display:grid; grid-template-columns:1fr 1fr; gap:10px; }
  .compra .so input { position:absolute; opacity:0; pointer-events:none; }
  .compra .so label { padding:11px; text-align:center; border:1px solid var(--line); border-radius:10px; background:rgba(255,255,255,.04); cursor:pointer; font-weight:800; letter-spacing:.06em; font-size:14px; color:var(--ink); }
  .compra .so input:checked + label { border-color:var(--cor); background:color-mix(in srgb, var(--cor) 16%, transparent); }
  .compra .so input:focus-visible + label { outline:2px solid var(--cor); outline-offset:2px; }
  .compra .rot { font-size:13px; color:var(--dim); }
  .compra .erro { color:#f87171; font-weight:700; min-height:1.4em; font-size:14px; }
  .compra .token { font:700 17px Consolas, ui-monospace, monospace; background:#08070c; border:2px dashed var(--cor); border-radius:10px; padding:12px;
                   text-align:center; word-break:break-all; color:var(--cor); user-select:all; }
  .compra .aviso { background:color-mix(in srgb, var(--cor) 9%, transparent); border:1px solid color-mix(in srgb, var(--cor) 40%, transparent); border-radius:10px; padding:10px 12px; font-size:14px; }
  .compra .passo { font-weight:800; margin-top:8px; }
  .compra ol { padding-left:20px; margin:0; font-size:14px; color:#d6d2e4; } .compra li { margin:5px 0; }
  .compra code { font:12px Consolas, ui-monospace, monospace; background:#08070c; border:1px solid var(--line); border-radius:6px; padding:1px 5px; word-break:break-all; }
  .compra .bt { width:100%; } .compra form { display:flex; flex-direction:column; gap:10px; margin:0; }
  .compra a.link { color:var(--cor); }
  .compra.feito { position:static; }
  .k { color:var(--org); font-weight:800; } .b { color:var(--blu); font-weight:800; }
  [hidden] { display:none !important; }
</style>
</head><body>
<header class="topo"><div class="faixa">
  <a class="marca" href="${SITE_URL}/">Inside<b>Studio</b>SP</a>
  <nav>
    <a href="${SITE_URL}/">${t('Loja', 'Shop')}</a>
    <a href="${SITE_URL}/estudio/">${t('Estúdio', 'Studio')}</a>
    <a href="${SITE_URL}/contato/">${t('Contato', 'Contact')}</a>
    <button class="idioma" type="button" aria-label="Idioma / Language"><span class="pt">EN</span><span class="en">PT</span></button>
  </nav>
</div></header>
${of.ativa && of.maior ? `<a class="faixa-oferta so-oferta" href="${SITE_URL}/#packs"><b>${t('OFERTA DE LANÇAMENTO', 'LAUNCH OFFER')}</b> ${t(`até ${of.maior}% off nos packs`, `up to ${of.maior}% off all packs`)} <span class="sep">·</span> ${t('termina em', 'ends in')} <span class="contador" data-fim="${OFERTA_FIM}">${t('até ' + fim, 'until ' + fim)}</span></a>` : ''}
<main>${corpo}</main>
<footer id="contato" class="rodape"><div class="faixa">
  <div>
    <div class="marca">Inside<b>Studio</b>SP</div>
    <p>${t('Plugins, mix e master em São Paulo.', 'Plugins, mixing and mastering from São Paulo, Brazil.')}</p>
  </div>
  <div class="contatos">
    <a href="mailto:${SITE.email}">${SITE.email}</a>
    <a href="${SITE.instagram}" rel="noopener" target="_blank">Instagram @equalizedmusic</a>
  </div>
  <small>© ${new Date(agora).getFullYear()} InsideStudioSP · <a href="${SITE_URL}/privacidade/">${t('Privacidade e cookies', 'Privacy and cookies')}</a></small>
</div></footer>
<dialog class="zoom"><button class="fechar" type="button" aria-label="Fechar / Close">✕</button><img alt=""></dialog>
</body></html>`;
}

// vitrine: os packs pagos no cartão do site, com o preço que o servidor cobra
function vitrine(agora) {
    const cartoes = PACKS.filter((p) => typeof PRECOS[p.id]?.brl === 'number').map((p) => {
        const v = precoAtual(p.id, agora).brl, de = agora < Date.parse(fimOferta(p.id)) && DE_BRL[v];   // MAESTRO termina antes (Black Friday)
        return `<a class="pack" href="${SITE_URL}/${p.id}/" style="--cor:${p.cor}">
    <div class="pack-img"><img src="${SITE_URL}/img/${p.capa}.webp" alt="${esc(p.nome)}" loading="lazy"></div>
    <div class="pack-txt">
      <div class="pack-topo"><h3>${esc(p.nome)}</h3><span class="valor">${de ? `<s class="de so-oferta">${brl(de)}</s>` : ''}${brl(v)}</span></div>
      <p>${t(...p.frase)}</p>
      <small>${t(...p.tipo)}</small>
    </div>
  </a>`;
    }).join('\n  ');
    if (!cartoes) return '';
    return `<section class="faixa" id="loja">
  <h2 class="titulo-sec">${t('Gostou? Conheça os plugins da loja', 'Like it? Check out the shop')}</h2>
  <div class="grade">
  ${cartoes}
  </div>
  <p style="margin-top:22px"><a class="bt bt-sec" href="${SITE_URL}/#packs">${t('Ver a loja completa', 'See the whole shop')}</a></p>
</section>`;
}

// topo da página no molde do pack do site; a caixa lateral é o cadastro
function heroGratis(p, caixa, { vst }) {
    const [, , dPt, dEn] = p.plugins[0] || [];
    return `
<section class="pack-hero" style="--cor:${p.cor}"><div class="faixa pack-hero-grade">
  <div>
    <a class="voltar" href="${SITE_URL}/#packs">← ${t('Todos os packs', 'All packs')}</a>
    <h1>${esc(p.nome)}</h1>
    <p class="lead">${t(...p.frase)}</p>
    <p>${t(...p.texto)}</p>
    <p class="tipo">${t(...p.tipo)} · Windows · macOS</p>
    ${vst ? `<p class="req">${REQ_VST}</p>` : ''}
    ${dPt ? `<p class="dim">${t(dPt, dEn)}</p>` : ''}
  </div>
  ${caixa}
</div>
<div class="faixa"><button type="button" class="ampliar capa" data-img="${SITE_URL}/img/${p.capa}.webp"><img src="${SITE_URL}/img/${p.capa}.webp" alt="${esc(p.nome)}"></button></div>
</section>`;
}

// script comum: T() monta o texto nas duas línguas (o site.js mostra a do botão EN/PT)
const SCRIPT_BASE = `
  var $ = function (id) { return document.getElementById(id); };
  var T = function (pt, en) { return '<span class="pt">' + pt + '</span><span class="en" lang="en">' + en + '</span>'; };
  var mac = /Mac|iPhone|iPad/.test(navigator.platform || navigator.userAgent);
  $(mac ? 'soMac' : 'soWin').checked = true;
  var okEmail = function (e) { return /^[^@\\s]+@[^@\\s]+\\.[^@\\s]+$/.test(e); };
  var esc = function (s) { return String(s).replace(/[&<>"]/g, function (c) { return { '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[c]; }); };
  var erroServ = function (j) { return j && j.mensagem ? esc(j.mensagem) : T('Não deu certo. Tente de novo.', 'Something went wrong. Please try again.'); };
  var SEMREDE = T('Sem conexão. Tente de novo.', 'No connection. Please try again.');`;

const CAMPOS_EMAIL_SO = `
      <label class="campo">${t('Seu e-mail', 'Your e-mail')}
        <input id="email" type="email" autocomplete="email" required maxlength="120" placeholder="voce@email.com"></label>
      <span class="rot">${t('Seu sistema', 'Your system')}</span>
      <div class="so" role="radiogroup">
        <input type="radio" name="so" id="soWin" value="win"><label for="soWin">WINDOWS</label>
        <input type="radio" name="so" id="soMac" value="mac"><label for="soMac">MAC</label>
      </div>
      <input class="isca" name="site" id="site" tabindex="-1" autocomplete="off" aria-hidden="true">`;

function paginaVassoura(agora) {
    const p = PACKS.find((x) => x.id === 'vassoura');
    const caixa = `<div class="compra" id="caixa">
  <div id="passo1" style="display:contents">
    <div class="preco"><span class="selo-gratis">${t('GRÁTIS', 'FREE')}</span></div>
    <form id="f" novalidate>${CAMPOS_EMAIL_SO}
      <button class="bt bt-principal" id="ir" type="submit">${t('Receber a VASSOURA', 'Get VASSOURA')}</button>
      <div class="erro" id="erro"></div>
    </form>
    <p class="nota">${t('Mandamos o link e o passo a passo para o seu e-mail. Não precisa de ativação.', 'We e-mail you the link and the steps. No activation needed.')}</p>
  </div>
  <div id="passo2" hidden style="display:contents">
    <p id="txtOk"></p>
    <div class="aviso" id="mandado" hidden>${th('Também mandamos o link e o passo a passo para <b id="emailMandado"></b> (olhe o spam e Promoções).', 'We also e-mailed the link and the steps to <b id="emailMandado2"></b> (check spam).')}</div>
    <div class="passo">${t('1. Baixe', '1. Download')}</div>
    <a class="bt bt-principal" id="baixar" href="#">${t('Baixar VASSOURA', 'Download VASSOURA')}</a>
    <div class="passo">${t('2. Instale', '2. Install')}</div>
    <ol>
      <li><span id="extrair"></span> <span id="copie"></span> <code id="pasta"></code>. ${th('Os 3 arquivos (<b>Vassoura.amxd</b>, <b>vassoura.js</b> e <b>vassoura_view.js</b>) precisam ficar juntos.', 'The 3 files (<b>Vassoura.amxd</b>, <b>vassoura.js</b> and <b>vassoura_view.js</b>) must stay together.')}</li>
      <li>${th('No Live, arraste o <b>Vassoura.amxd</b> para a <b>PRIMEIRA faixa de áudio</b> do projeto. No <b>MASTER</b> ele não funciona: o ANALYZE não lê os clips.', 'In Live, drag <b>Vassoura.amxd</b> onto the <b>FIRST audio track</b> of the set. It does not work on the <b>MASTER</b>: ANALYZE cannot read the clips there.')}</li>
    </ol>
    <div class="aviso">${th('<b>Antes do primeiro APPLY, salve uma cópia do set</b> (File &gt; Save Live Set As…). Cada trecho mutado ou apagado é um <b class="desfazer"></b> separado.', '<b>Save a copy of your set before the first APPLY</b> (File &gt; Save Live Set As…). Each muted or deleted part is a separate <b class="desfazer"></b>.')}</div>
    <div class="passo">${t('3. Use', '3. Use')}</div>
    <ol>
      <li>${th('Abra um set com stems (clips de áudio no Arrangement, com <b>WARP desligado</b>).', 'Open a set with stems (audio clips in the Arrangement, with <b>WARP off</b>).')}</li>
      <li>${th('Clique em <b>ANALYZE</b> e em <b>OPEN</b>. Ajuste THRESHOLD, MIN LENGTH, PRE, POST e SNAP: a janela redesenha na hora.', 'Click <b>ANALYZE</b> and <b>OPEN</b>. Adjust THRESHOLD, MIN LENGTH, PRE, POST and SNAP: the window redraws instantly.')}</li>
      <li>${th('Escolha <b>MUTE</b> (desliga os pedaços vazios) ou <b>CUT</b> (apaga) e clique em <b>APPLY</b>.', 'Pick <b>MUTE</b> (switches the empty parts off) or <b>CUT</b> (deletes them) and click <b>APPLY</b>.')}</li>
    </ol>
  </div>
</div>`;
    const script = `<script>
(function () {${SCRIPT_BASE}
  var BOTAO = T('Receber a VASSOURA', 'Get VASSOURA');
  $('f').addEventListener('submit', function (ev) {
    ev.preventDefault();
    var email = $('email').value.trim();
    var so = $('soMac').checked ? 'mac' : $('soWin').checked ? 'win' : '';
    $('erro').innerHTML = '';
    if (!okEmail(email)) { $('erro').innerHTML = T('Confira o e-mail.', 'Check your e-mail.'); return; }
    if (!so) { $('erro').innerHTML = T('Escolha Windows ou Mac.', 'Choose Windows or Mac.'); return; }
    $('ir').disabled = true; $('ir').innerHTML = T('Enviando…', 'Sending…');
    fetch(location.pathname, { method: 'POST', headers: { 'content-type': 'application/json' },
      body: JSON.stringify({ email: email, sistema: so, site: $('site').value }) })
      .then(function (r) { return r.json(); })
      .then(function (j) {
        $('ir').disabled = false; $('ir').innerHTML = BOTAO;
        if (!j.ok) { $('erro').innerHTML = erroServ(j); return; }
        $('baixar').href = j.baixar.zip;
        $('extrair').innerHTML = so === 'mac' ? T('Extraia o zip (dois cliques no arquivo)', 'Unzip it (double-click the file)')
          : T('Extraia o zip (botão direito > "Extrair tudo...")', 'Unzip it (right-click > "Extract All...")');
        $('copie').innerHTML = T('e copie a pasta inteira para', 'and copy the whole folder to');
        var cam = 'Ableton/User Library/Presets/Audio Effects/Max Audio Effect/';
        if (so !== 'mac') cam = cam.split('/').join('\\\\');
        $('pasta').innerHTML = so === 'mac' ? T('Música/' + cam, 'Music/' + cam) : T('Documentos\\\\' + cam, 'Documents\\\\' + cam);
        document.querySelectorAll('.desfazer').forEach(function (b) { b.textContent = so === 'mac' ? 'Cmd+Z' : 'Ctrl+Z'; });
        $('txtOk').innerHTML = j.ja_tinha
          ? T('<b>Este e-mail já está cadastrado.</b> É só baixar de novo aqui embaixo.', '<b>This e-mail is already registered.</b> Just download it again below.')
          : T('<b>Pronto! Seu cadastro está feito.</b>', '<b>Done! You are registered.</b>');
        if (j.email_enviado) { $('emailMandado').textContent = $('emailMandado2').textContent = email.toLowerCase(); $('mandado').hidden = false; }
        $('passo1').hidden = true; $('passo2').hidden = false; $('caixa').classList.add('feito'); $('caixa').scrollIntoView({ block: 'start' });
      })
      .catch(function () { $('ir').disabled = false; $('ir').innerHTML = BOTAO; $('erro').innerHTML = SEMREDE; });
  });
})();
</script>`;
    return paginaSite({ titulo: 'VASSOURA grátis — InsideStudioSP', agora,
        corpo: heroGratis(p, caixa, { vst: false }) + '\n' + vitrine(agora) + '\n' + script });
}

function paginaVst(pacote, agora, reenvio) {
    const V = VST_GRATIS[pacote];
    const p = PACKS.find((x) => x.id === pacote);
    const caixa = `<div class="compra" id="caixa">
  <div id="passo1" style="display:contents">
    <div class="preco"><span class="selo-gratis">${t('GRÁTIS', 'FREE')}</span></div>
    <form id="f" novalidate>${CAMPOS_EMAIL_SO}
      <button class="bt bt-principal" id="ir" type="submit">${t('Gerar minha licença', 'Get my license')}</button>
      <div class="erro" id="erro"></div>
    </form>
    <p class="nota">${t('Licença para 1 computador. O e-mail é só para a sua licença.', 'License for 1 computer. Your e-mail is used only for your license.')}</p>
    ${reenvio ? `<p class="nota">${t('Já se cadastrou e não recebeu ou perdeu o token?', 'Already signed up and lost your token?')} <a href="#" id="perdi" class="link">${t('Receber um token novo no e-mail', 'Get a new token by e-mail')}</a></p>
    <p class="nota" id="msgPerdi"></p>` : ''}
  </div>
  <div id="passo2" hidden style="display:contents">
    <div id="comToken" hidden style="display:contents">
      <p style="margin:0"><b>${t('Pronto! Este é o seu token:', 'Done! This is your token:')}</b></p>
      <div class="token" id="token"></div>
      <button class="bt bt-sec" id="copiar" type="button">${t('Copiar token', 'Copy token')}</button>
      <div class="aviso">${th('<b>Guarde o token</b>: ele aparece só agora (tire um print).', '<b>Save your token</b>: it is shown only now (take a screenshot).')}</div>
    </div>
    <p id="txtSem" hidden></p>
    <div class="aviso" id="mandado" hidden>${th('Também mandamos o token e o passo a passo para <b id="emailMandado"></b> (olhe o spam e Promoções).', 'We also e-mailed the token and the steps to <b id="emailMandado2"></b> (check spam).')}</div>
    <div class="passo">${t('1. Baixe e instale', '1. Download and install')}</div>
    <form id="fz" method="post" action="/gratis/${pacote}/zip">
      <input type="hidden" name="email" id="zEmail"><input type="hidden" name="token" id="zToken"><input type="hidden" name="sistema" id="zSo">
      <button class="bt bt-principal" id="baixar" type="submit">${t('Baixar ' + V.nome, 'Download ' + V.nome)}</button>
    </form>
    <p class="nota">${th('Vem um <b>.zip</b> com o instalador e o <b>LEIA-ME</b> com o seu e-mail, o token, o passo a passo e o que fazer se aparecer aviso de segurança.', 'You get a <b>.zip</b> with the installer and a <b>READ ME</b> with your e-mail, token, steps and what to do about security warnings.')}</p>
    <p class="nota" id="dicaSo"></p>
    <p class="nota">${t('Não baixou?', 'Download failed?')} <a id="direto" href="#" class="link">${t('Baixar só o instalador', 'Get the installer only')}</a></p>
    <div class="passo">${t(...V.pagUsoTit)}</div>
    <ol>
      ${V.pagUso.map((x) => `<li>${th(...x)}</li>`).join('\n      ')}
    </ol>
    <div class="passo">${t('3. Ative', '3. Activate')}</div>
    <ol>
      <li>${th('Na tela de ativação do ' + V.nome + ', digite <b class="emailOk"></b> e o token, e clique em <b>ATIVAR</b>.', 'On the ' + V.nome + ' activation screen, type <b class="emailOk"></b> and the token, and click <b>ATIVAR</b>.')}</li>
      <li>${th('Vale para <b>1 computador</b>. Para trocar de PC: menu <b>≡</b> do plugin → <b>Licença…</b> → <b>Desativar este computador</b>, e ative no novo.', 'Works on <b>1 computer</b>. To switch: plugin menu <b>≡</b> → <b>Licença…</b> → <b>Desativar este computador</b>, then activate on the new one.')}</li>
      <li>${th(...V.pagPronto)}</li>
    </ol>
  </div>
</div>`;
    const script = `<script>
(function () {${SCRIPT_BASE}
  var BOTAO = T('Gerar minha licença', 'Get my license');
  var NOVO = T('Receber um token novo no e-mail', 'Get a new token by e-mail');
  $('f').addEventListener('submit', function (ev) {
    ev.preventDefault();
    var email = $('email').value.trim();
    var so = $('soMac').checked ? 'mac' : $('soWin').checked ? 'win' : '';
    $('erro').innerHTML = '';
    if (!okEmail(email)) { $('erro').innerHTML = T('Confira o e-mail.', 'Check your e-mail.'); return; }
    if (!so) { $('erro').innerHTML = T('Escolha Windows ou Mac.', 'Choose Windows or Mac.'); return; }
    $('ir').disabled = true; $('ir').innerHTML = T('Gerando…', 'Generating…');
    fetch(location.pathname, { method: 'POST', headers: { 'content-type': 'application/json' },
      body: JSON.stringify({ email: email, sistema: so, site: $('site').value }) })
      .then(function (r) { return r.json(); })
      .then(function (j) {
        $('ir').disabled = false; $('ir').innerHTML = BOTAO;
        if (!j.ok) { $('erro').innerHTML = erroServ(j); return; }
        $('direto').href = j.baixar[so] || '#';
        $('zEmail').value = email; $('zToken').value = j.token || ''; $('zSo').value = so;
        $('baixar').innerHTML = so === 'mac' ? T('Baixar ${V.nome} para Mac', 'Download ${V.nome} for Mac') : T('Baixar ${V.nome} para Windows', 'Download ${V.nome} for Windows');
        $('dicaSo').innerHTML = so === 'mac'
          ? T('Abra o .dmg que vem no zip e rode o instalador (VST3 e AU). Depois reabra o DAW.', 'Open the .dmg inside the zip and run the installer (VST3 and AU). Then restart your DAW.')
          : T('Extraia o zip e rode o instalador .exe (VST3). Se o Windows avisar, clique em "Mais informações" e depois em "Executar assim mesmo". Depois reabra o DAW.',
              'Unzip it and run the .exe installer (VST3). If Windows warns you, click "More info" and then "Run anyway". Then restart your DAW.');
        document.querySelectorAll('.emailOk').forEach(function (b) { b.textContent = email.toLowerCase(); });
        if (j.token) { $('token').textContent = j.token; $('comToken').hidden = false; }
        else {
          var perdeu = j.reenvio ? ' ' + T('Perdeu?', 'Lost it?') + ' <a href="#" id="perdi2" class="link">' + NOVO + '</a>.' : '';
          $('txtSem').innerHTML = j.ja_tinha
            ? T('<b>Este e-mail já recebeu o ${V.nome}.</b> Use o token que apareceu da primeira vez.', '<b>This e-mail already got ${V.nome}.</b> Use the token you were shown the first time.')
              + (j.reenvio ? perdeu : ' ' + T('Perdeu? Chame a gente no direct.', 'Lost it? Message us on Instagram.'))
            : T('<b>Pronto! O ${V.nome} entrou no seu cadastro.</b> Use o mesmo token que você já recebeu da InsideStudioSP.', '<b>Done! ${V.nome} was added to your account.</b> Use the same token you already got from InsideStudioSP.') + perdeu;
          $('txtSem').hidden = false;
          if ($('perdi2')) $('perdi2').addEventListener('click', function (ev) { ev.preventDefault(); reenviar(email, $('txtSem')); });
        }
        if (j.email_enviado) { $('emailMandado').textContent = $('emailMandado2').textContent = email.toLowerCase(); $('mandado').hidden = false; }
        $('passo1').hidden = true; $('passo2').hidden = false; $('caixa').classList.add('feito'); $('caixa').scrollIntoView({ block: 'start' });
      })
      .catch(function () { $('ir').disabled = false; $('ir').innerHTML = BOTAO; $('erro').innerHTML = SEMREDE; });
  });
  function reenviar(email, onde) {
    onde.innerHTML = T('Enviando…', 'Sending…');
    fetch('/gratis/${pacote}/reenviar', { method: 'POST', headers: { 'content-type': 'application/json' },
      body: JSON.stringify({ email: email, site: $('site').value }) })
      .then(function (r) { return r.json(); })
      .then(function (j) { onde.innerHTML = j.mensagem ? esc(j.mensagem) : erroServ(j); })
      .catch(function () { onde.innerHTML = SEMREDE; });
  }
  if ($('perdi')) $('perdi').addEventListener('click', function (ev) {
    ev.preventDefault();
    var email = $('email').value.trim();
    if (!okEmail(email)) { $('erro').innerHTML = T('Digite acima o e-mail que você cadastrou e clique de novo.', 'Type the e-mail you signed up with above and click again.'); $('email').focus(); return; }
    $('erro').innerHTML = ''; reenviar(email, $('msgPerdi'));
  });
  $('copiar').addEventListener('click', function () {
    var tk = $('token').textContent;
    (navigator.clipboard ? navigator.clipboard.writeText(tk) : Promise.reject()).then(function () { $('copiar').innerHTML = T('Copiado ✓', 'Copied ✓'); },
      function () { var s = window.getSelection(), r = document.createRange(); r.selectNodeContents($('token')); s.removeAllRanges(); s.addRange(r); });
  });
})();
</script>`;
    return paginaSite({ titulo: V.nome + ' grátis — InsideStudioSP', agora,
        corpo: heroGratis(p, caixa, { vst: true }) + '\n' + vitrine(agora) + '\n' + script });
}
