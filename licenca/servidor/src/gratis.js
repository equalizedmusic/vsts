// Páginas grátis: a pessoa põe o e-mail e recebe na tela o token e os links de download, sem passar pelo painel.
// É o link mandado no direct do Instagram para quem comenta a palavra do reel (QUERO, ALIGN…).
//
//   GET  /gratis/montamonta   MONTA-MONTA (2 computadores, links Windows e Mac)
//   GET  /gratis/align        ALIGN (1 computador; a pessoa escolhe Windows ou Mac e recebe só aquele link)
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

const POR_IP_DIA = 8;
const TOTAL_DIA = 300;
const PAGINAS = {
    montamonta: { maquinas: 2, sistema: false, nota: 'página grátis' },
    align: { maquinas: 1, sistema: true, nota: 'página ALIGN' },
};
export const PACOTES_GRATIS = Object.keys(PAGINAS);

function json(obj, status = 200) {
    return new Response(JSON.stringify(obj), {
        status, headers: { 'content-type': 'application/json; charset=utf-8', 'cache-control': 'no-store' },
    });
}

export async function rotaGratis(req, env, url, h, PACOTE = 'montamonta') {
    const cfg = PAGINAS[PACOTE];
    // form-action 'self': o botão de download do ALIGN é um formulário (POST /gratis/align/zip)
    if (req.method === 'GET') return new Response(PACOTE === 'align' ? PAGINA_ALIGN.replace('<!--REENVIO-->', emailLigado(env) ? REENVIO : '') : PAGINA, {
        headers: { 'content-type': 'text/html; charset=utf-8', 'cache-control': 'no-store',
                   'content-security-policy': `default-src 'none'; style-src 'unsafe-inline'; script-src 'unsafe-inline'; connect-src 'self'; img-src data:; base-uri 'none'; form-action ${PACOTE === 'align' ? "'self'" : "'none'"}; frame-ancestors 'none'` },
    });
    if (req.method !== 'POST') return json({ ok: false, mensagem: 'Método não aceito.' }, 405);

    const ip = req.headers.get('cf-connecting-ip') || '';
    let corpo;
    try { corpo = await req.json(); } catch { return json({ ok: false, mensagem: 'Pedido inválido.' }, 400); }
    const email = h.normEmail(corpo.email);
    const sistema = cfg.sistema ? String(corpo.sistema || '') : '';
    const baixar = {};
    for (const s of ['win', 'mac']) if (!sistema || s === sistema) baixar[s] = `${url.origin}/baixar/${PACOTE}/${s}`;
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

    const doEmail = await env.DB.prepare('SELECT pacote, token_hash, status FROM licencas WHERE email = ?').bind(email).all();
    const lista = doEmail.results || [];
    if (lista.some((l) => l.pacote === PACOTE)) {
        await h.evento(env, null, 'gratis_ja', email, ip);
        return json({ ok: true, novo: false, ja_tinha: true, baixar, reenvio: PACOTE === 'align' && emailLigado(env) });
    }
    const token = lista.length ? null : h.novoToken();
    const hash = lista.length ? lista[0].token_hash : await h.sha256Hex('ISP' + h.normToken(token));
    const nota = cfg.nota + (sistema ? ' · ' + (sistema === 'mac' ? 'Mac' : 'Windows') : '');
    const r = await env.DB.prepare(
        "INSERT INTO licencas (email, pacote, token_hash, max_maquinas, origem, nota, criada_em) VALUES (?, ?, ?, ?, 'gratis', ?, ?)")
        .bind(email, PACOTE, hash, cfg.maquinas, nota, h.agora()).run();
    await h.evento(env, r.meta.last_row_id, 'gratis', lista.length ? 'mesmo token' : 'novo', ip);
    const enviado = PACOTE === 'align' && await emailAlign(env, url, h, { email, token, sistema, assunto: 'Sua licença do ALIGN · Your ALIGN license' });
    if (enviado) await h.evento(env, r.meta.last_row_id, 'email', 'cadastro', ip);
    return json({ ok: true, novo: true, token, mesmo_token: !!lista.length, baixar, email_enviado: !!enviado, reenvio: PACOTE === 'align' && emailLigado(env) });
}

// e-mail com o mesmo conteúdo do LEIA-ME do zip + o link direto do instalador (o zip é POST com o token)
async function emailAlign(env, url, h, { email, token, sistema, assunto, aviso = [] }) {
    if (!emailLigado(env)) return false;
    let inst = null;
    try { inst = await h.instalador(env, 'align', sistema); } catch (e) { console.error('email align: instalador', e); }
    const link = `${url.origin}/baixar/align/${sistema}`;
    const texto = [
        ...aviso,
        ...leiaMeAlign({ email, token, sistema, nome: inst?.nome || (sistema === 'mac' ? 'ALIGN.dmg' : 'ALIGN.exe'),
                         versao: inst?.versao, sha256: inst?.sha256, porEmail: true, link }),
    ].join('\r\n');
    return enviarEmail(env, { para: email, assunto, texto, segredos: [token] });
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

export async function rotaTokenAlign(req, env, url, h, acao) {
    if (!emailLigado(env)) return h.paginaSimples('Indisponível', 'Chame a InsideStudioSP no direct do Instagram.', 404);
    const ip = req.headers.get('cf-connecting-ip') || '';
    if (acao === 'reenviar') {
        if (req.method !== 'POST') return json({ ok: false, mensagem: 'Método não aceito.' }, 405);
        let corpo;
        try { corpo = await req.json(); } catch { return json({ ok: false, mensagem: 'Pedido inválido.' }, 400); }
        const email = h.normEmail(corpo.email);
        if (email.length > 120 || !/^[^@\s]+@[^@\s]+\.[^@\s]+$/.test(email)) return json({ ok: false, mensagem: 'Confira o e-mail.' }, 400);
        const pronto = json({ ok: true, mensagem: 'Se este e-mail tiver o ALIGN, o link chega em alguns minutos (olhe também o spam e Promoções). O link vale por 1 hora.' });
        if (corpo.site) return pronto;
        const desde = h.agora() - 86400;
        const nIp = await env.DB.prepare("SELECT COUNT(*) AS n FROM eventos WHERE tipo = 'reenvio_pedido' AND ip = ? AND quando > ?").bind(ip, desde).first();
        if ((nIp?.n || 0) >= REENVIO_POR_IP_DIA) return json({ ok: false, mensagem: 'Muitos pedidos agora. Tente de novo amanhã ou chame a gente no direct.' }, 429);
        const lic = await env.DB.prepare("SELECT id FROM licencas WHERE email = ? AND pacote = 'align'").bind(email).first();
        if (!lic) { await h.evento(env, null, 'reenvio_pedido', 'sem ALIGN', ip); return pronto; }
        const ult = await env.DB.prepare("SELECT COUNT(*) AS n, MAX(quando) AS q FROM eventos WHERE tipo = 'reenvio_pedido' AND licenca_id = ? AND quando > ?")
            .bind(lic.id, desde).first();
        if ((ult?.n || 0) >= REENVIO_POR_EMAIL_DIA || (ult?.q && h.agora() - ult.q < REENVIO_INTERVALO)) return pronto;
        const c = [...crypto.getRandomValues(new Uint8Array(24))].map((b) => b.toString(16).padStart(2, '0')).join('');
        await h.evento(env, lic.id, 'reenvio_pedido', await h.sha256Hex('REENVIO' + c), ip);
        const link = `${url.origin}/gratis/align/novo-token?c=${c}`;
        await enviarEmail(env, { para: email, assunto: 'Novo token do ALIGN · New ALIGN token', texto: [
            'Alguém (provavelmente você) pediu um token novo do ALIGN para este e-mail.',
            'Para gerar o token novo, abra o link abaixo e clique em GERAR NOVO TOKEN (vale por 1 hora, uma vez só):',
            '', link, '',
            'Atenção: o token antigo deixa de valer. Computador já ativado precisa ser ativado de novo com o token novo.',
            'Não pediu? Ignore este e-mail: nada muda.',
            'Dúvidas? Responda este e-mail ou escreva para ' + CONTATO + '.',
            '', '==================================== ENGLISH ====================================', '',
            'Someone (probably you) asked for a new ALIGN token for this e-mail.',
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
    const expirado = () => h.paginaSimples('Link vencido', 'Este link já foi usado ou passou de 1 hora. Volte à página do ALIGN e peça de novo em "Perdi meu token". · This link was used or expired.', 410);
    if (!/^[0-9a-f]{48}$/.test(c)) return expirado();
    const hc = await h.sha256Hex('REENVIO' + c);
    const ped = await env.DB.prepare("SELECT licenca_id FROM eventos WHERE tipo = 'reenvio_pedido' AND detalhe = ? AND quando > ?")
        .bind(hc, h.agora() - REENVIO_VALIDADE).first();
    const usado = ped && await env.DB.prepare("SELECT id FROM eventos WHERE tipo = 'reenvio_usado' AND detalhe = ?").bind(hc).first();
    const lic = ped && !usado && await env.DB.prepare("SELECT id, email, nota FROM licencas WHERE id = ? AND pacote = 'align'").bind(ped.licenca_id).first();
    if (!lic) return expirado();
    if (req.method !== 'POST') return paginaToken({ c });

    await h.evento(env, lic.id, 'reenvio_usado', hc, ip);
    const token = h.novoToken();
    await env.DB.prepare('UPDATE licencas SET token_hash = ? WHERE email = ?').bind(await h.sha256Hex('ISP' + h.normToken(token)), lic.email).run();
    await h.evento(env, lic.id, 'novo_token', 'pelo e-mail (perdi meu token)', ip);
    const sistema = /Mac$/.test(lic.nota || '') ? 'mac' : 'win';
    await emailAlign(env, url, h, { email: lic.email, token, sistema, assunto: 'Seu novo token do ALIGN · Your new ALIGN token',
        aviso: ['O token antigo deixou de valer. · The old token no longer works.', ''] });
    return paginaToken({ email: lic.email, token, sistema });
}

function paginaToken({ c, email, token, sistema }) {
    const esc = (s) => String(s).replace(/[&<>"]/g, (x) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[x]));
    const corpo = c
        ? `<h2>Gerar novo token do ALIGN</h2>
<p>O token antigo deixa de valer. Computador já ativado precisa ser ativado de novo com o token novo.</p>
<p class="en">The old token stops working. A computer already activated must be activated again with the new token.</p>
<form method="post"><input type="hidden" name="c" value="${esc(c)}"><button>GERAR NOVO TOKEN</button></form>`
        : `<h2>Pronto! Seu novo token</h2>
<p>E-mail: <b>${esc(email)}</b></p><div class="token">${esc(token)}</div>
<p>Também mandamos para o seu e-mail. · We also sent it to your e-mail.</p>
<form method="post" action="/gratis/align/zip"><input type="hidden" name="email" value="${esc(email)}"><input type="hidden" name="token" value="${esc(token)}">
<input type="hidden" name="sistema" value="${sistema}"><button>BAIXAR ALIGN PARA ${sistema === 'mac' ? 'MAC' : 'WINDOWS'}</button></form>
<p class="en">O zip traz o instalador e o LEIA-ME com o token. · The zip has the installer and a READ ME with your token.</p>`;
    return new Response(`<!doctype html><html lang="pt-BR"><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1">
<title>ALIGN · InsideStudioSP</title><style>
body { margin:0; background:#100e17; color:#eeecf6; font:16px/1.5 "Segoe UI", system-ui, -apple-system, Arial, sans-serif; }
main { max-width:520px; margin:0 auto; padding:36px 20px; } .en { color:#8d8aa3; font-size:14px; font-style:italic; }
.token { font:700 19px Consolas, "SF Mono", monospace; border:2px dashed #a78bfa; color:#a78bfa; border-radius:10px; padding:14px; text-align:center; word-break:break-all; user-select:all; }
button { width:100%; padding:15px; margin-top:16px; border:0; border-radius:12px; background:#34d399; color:#0b2316; font:900 17px "Segoe UI", system-ui, sans-serif; cursor:pointer; }
</style><main>${corpo}</main></html>`, {
        headers: { 'content-type': 'text/html; charset=utf-8', 'cache-control': 'no-store', 'referrer-policy': 'no-referrer',
                   'content-security-policy': "default-src 'none'; style-src 'unsafe-inline'; base-uri 'none'; form-action 'self'; frame-ancestors 'none'" },
    });
}

// ---------------- zip do ALIGN: instalador + LEIA-ME gerado na hora ----------------
// POST /gratis/align/zip (formulário da página, depois do cadastro): email, token (vazio se o e-mail já tinha token),
// sistema. O token só entra no texto se conferir com a licença do e-mail (ninguém monta um leia-me com token alheio).
// Sem o CRC do instalador (release sem .crc32 e fora de CRC_CONHECIDOS) cai no download direto do instalador.

export async function rotaZipAlign(req, env, h) {
    if (req.method !== 'POST') return h.paginaSimples('Link inválido', 'Abra a página do ALIGN e gere a sua licença.', 405);
    let f;
    try { f = await req.formData(); } catch { return h.paginaSimples('Pedido inválido', 'Volte à página do ALIGN e tente de novo.', 400); }
    const email = h.normEmail(f.get('email'));
    const sistema = f.get('sistema') === 'mac' ? 'mac' : 'win';
    const tokenTxt = String(f.get('token') || '').trim();
    const lic = await env.DB.prepare("SELECT id, token_hash FROM licencas WHERE email = ? AND pacote = 'align'").bind(email).first();
    if (!lic) return h.paginaSimples('E-mail sem ALIGN', 'Este e-mail não tem licença do ALIGN. Volte à página e gere a sua.', 404);
    let token = '';
    if (tokenTxt) {
        if (await h.sha256Hex('ISP' + h.normToken(tokenTxt)) !== lic.token_hash)
            return h.paginaSimples('Token não confere', 'Volte à página do ALIGN e baixe pelo botão que apareceu junto com o seu token.', 403);
        token = tokenTxt.toUpperCase();
    }
    const inst = await h.instalador(env, 'align', sistema);
    if (!inst) return h.paginaSimples('Download indisponível', 'O instalador não está disponível agora. Tente de novo mais tarde.', 404);
    if (inst.crc == null || !inst.tamanho) {
        console.error('zip align: sem crc', inst.nome, inst.sha256);
        return Response.redirect(inst.link, 302);
    }
    const r = await fetch(inst.link, { headers: { 'user-agent': 'isp-licencas' } });
    const len = Number(r.headers.get('content-length') || inst.tamanho);
    if (!r.ok || !r.body || len !== inst.tamanho) {
        console.error('zip align: instalador', r.status, len, inst.tamanho);
        return Response.redirect(inst.link, 302);
    }
    const texto = new TextEncoder().encode('\uFEFF' + leiaMeAlign({ email, token, sistema, ...inst }).join('\r\n') + '\r\n');
    const zip = zipStream([
        { nome: 'LEIA-ME - READ ME.txt', bytes: texto },
        { nome: inst.nome, tamanho: inst.tamanho, crc: inst.crc, corpo: r.body },
    ]);
    await h.evento(env, lic.id, 'gratis_zip', sistema, req.headers.get('cf-connecting-ip') || '');
    return new Response(zip.stream, {
        headers: {
            'content-type': 'application/zip', 'content-length': String(zip.tamanho), 'cache-control': 'no-store',
            'content-disposition': `attachment; filename="ALIGN-${inst.versao || 'InsideStudioSP'}-${sistema === 'mac' ? 'Mac' : 'Windows'}.zip"`,
        },
    });
}

// porEmail: o e-mail leva o link direto do instalador (sem zip), então os passos falam do arquivo e não do zip
function leiaMeAlign({ email, token, sistema, nome, versao, sha256, porEmail = false, link = '' }) {
    const mac = sistema === 'mac';
    const pt = porEmail ? 'Guarde este e-mail' : 'Guarde este arquivo', en = porEmail ? 'Keep this e-mail' : 'Keep this file';
    const L = [
        'ALIGN · InsideStudioSP' + (versao ? ' · v' + versao : '') + ' · ' + (mac ? 'Mac' : 'Windows'),
        'Kick / bass phase aligner',
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
        '  2) Na janela que abrir, dê dois cliques em "Instalar InsideStudioSP Align.pkg" e siga o instalador.',
        '     Os plugins vão para as pastas padrão do Mac (VST3 e AU em /Library/Audio/Plug-Ins).',
        '  3) Feche e abra de novo o seu DAW. No Logic, se o ALIGN não aparecer: Logic Pro > Ajustes >',
        '     Plug-in Manager > "Redefinir e verificar".',
    );
    else L.push(
        ...(porEmail ? [] : ['  1) Extraia o zip: botão direito no arquivo > "Extrair tudo..." > Extrair.',
                             '     Não rode o instalador de dentro do zip.']),
        '  ' + (porEmail ? 1 : 2) + ') Rode ' + nome + ' e siga o instalador. A pasta já vem certa',
        '     (C:\\Program Files\\Common Files\\VST3, onde o Ableton e os outros DAWs procuram).',
        '  ' + (porEmail ? 2 : 3) + ') Feche e abra de novo o seu DAW (se o ALIGN não aparecer, mande o DAW procurar plugins de novo).',
    );
    L.push(
        '',
        '2. COLOCAR NO BAIXO',
        '  1) Coloque o ALIGN no canal do BAIXO.',
        '  2) No Sidechain do ALIGN, escolha a faixa do KICK.',
        '     No Ableton: abra o Sidechain do plugin, escolha a faixa do kick e deixe em Post FX.',
        '',
        '3. ATIVAR',
        '  1) Na tela de ativação do ALIGN, digite o e-mail e o token acima e clique em ATIVAR.',
        '     Precisa de internet só nessa hora.',
        '  2) Aperte o botão ALIGN. Pronto: ele corrige sozinho, batida por batida.',
        '  3) Vai trocar de computador? Menu ≡ do plugin > Licença... > Desativar este computador, e ative no novo.',
        '',
        mac ? '4. SE O MAC DISSER QUE NÃO PODE ABRIR O INSTALADOR' : '4. SE O WINDOWS OU O ANTIVÍRUS DER AVISO DE SEGURANÇA',
        mac ? '  O ALIGN ainda não é registrado na Apple, então o Mac avisa que não pode verificar o desenvolvedor.'
            : '  O ALIGN ainda não tem assinatura digital paga, então o Windows não conhece o "editor" e pode avisar.',
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
        '  2) In the window that opens, double-click "Instalar InsideStudioSP Align.pkg" and follow the installer.',
        '     The plugins go to the standard Mac folders (VST3 and AU in /Library/Audio/Plug-Ins).',
        '  3) Close and reopen your DAW. In Logic, if ALIGN does not show up: Logic Pro > Settings >',
        '     Plug-in Manager > "Reset & Rescan Selection".',
    );
    else L.push(
        ...(porEmail ? [] : ['  1) Extract the zip: right-click the file > "Extract All..." > Extract.',
                             '     Do not run the installer from inside the zip.']),
        '  ' + (porEmail ? 1 : 2) + ') Run ' + nome + ' and follow the installer. The folder is already right',
        '     (C:\\Program Files\\Common Files\\VST3, where Ableton and the other DAWs look).',
        '  ' + (porEmail ? 2 : 3) + ') Close and reopen your DAW (if ALIGN does not show up, rescan your plugins).',
    );
    L.push(
        '',
        '2. PUT IT ON THE BASS',
        '  1) Put ALIGN on your BASS track.',
        '  2) In the ALIGN Sidechain, pick the KICK track.',
        '     In Ableton: open the plugin Sidechain, pick the kick track and set it to Post FX.',
        '',
        '3. ACTIVATE',
        '  1) On the ALIGN activation screen, type the e-mail and token above and click ATIVAR (activate).',
        '     Internet is needed only at this moment.',
        '  2) Press the ALIGN button. Done: it corrects by itself, hit by hit.',
        '  3) Changing computers? Plugin menu ≡ > Licença... > Desativar este computador (deactivate), then activate on the new one.',
        '',
        mac ? '4. IF YOUR MAC SAYS IT CANNOT OPEN THE INSTALLER' : '4. IF WINDOWS OR YOUR ANTIVIRUS SHOWS A SECURITY WARNING',
        mac ? '  ALIGN is not registered with Apple yet, so macOS says it cannot verify the developer.'
            : '  ALIGN does not have a paid digital signature yet, so Windows does not know the "publisher" and may warn you.',
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
<body><main>
  <div class="logo"><div class="q"><i style="background:#ff8a2a"></i><i style="background:#22d8ff"></i><i style="background:#5fe36a"></i><i style="background:#ff4f8b"></i></div>
    <div><h1><span class="a">MONTA</span>-<span class="b">MONTA</span></h1><div class="sub">INSIDE STUDIO SP · SYNTH DIDÁTICO</div></div></div>

  <section class="card" id="passo1">
    <p><span class="free">GRÁTIS</span> &nbsp;Monte um sintetizador peça por peça e aprenda o caminho do som. VST3 e AU, Windows e Mac.</p>
    <p class="en">Free learning synth: build it piece by piece. VST3 / AU, Windows and Mac.</p>
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
  <footer>InsideStudioSP</footer>
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

// entra no lugar de <!--REENVIO--> da página do ALIGN só quando o e-mail está ligado
const REENVIO = `    <p style="margin-top:14px;font-size:14px">Já se cadastrou e não recebeu ou perdeu o token? <a href="#" id="perdi" style="color:var(--pur)">Receber um token novo no e-mail</a>
      <span class="en">· lost your token?</span></p>
    <p id="msgPerdi" style="font-size:14px"></p>`;

// ALIGN: escolhe o sistema, recebe o token e só o link daquele sistema; vale para 1 computador
const PAGINA_ALIGN = `<!doctype html>
<html lang="pt-BR"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1">
<title>ALIGN · InsideStudioSP</title>
<style>
  :root { --bg:#100e17; --card:#1b1926; --line:#33304a; --ink:#eeecf6; --dim:#8d8aa3; --pur:#a78bfa; --org:#ff8a3d; --blu:#3ba0ff; --grn:#34d399; --pink:#f43f5e; }
  * { box-sizing:border-box; }
  body { margin:0; min-height:100vh; background:radial-gradient(700px 500px at 85% 0%, rgba(167,139,250,.16), transparent 70%),
         radial-gradient(700px 500px at 0% 100%, rgba(255,138,61,.10), transparent 70%), var(--bg);
         color:var(--ink); font:16px/1.5 "Segoe UI", system-ui, -apple-system, Arial, sans-serif; }
  main { max-width:520px; margin:0 auto; padding:36px 20px 60px; }
  .logo { display:flex; align-items:center; gap:14px; }
  h1 { margin:0; font:800 clamp(34px, 11vw, 48px)/1 Bahnschrift, "Segoe UI", system-ui, sans-serif; letter-spacing:.22em; }
  h1 b { color:var(--pur); }
  .risco { height:4px; border-radius:2px; background:linear-gradient(90deg, var(--org), var(--blu)); margin-top:6px; }
  .sub { color:var(--dim); font-size:13px; letter-spacing:.16em; font-weight:700; margin-top:8px; }
  .card { background:var(--card); border:1px solid var(--line); border-radius:18px; padding:24px; margin-top:26px; }
  .free { display:inline-block; background:var(--grn); color:#0b2316; font-weight:900; padding:2px 10px; border-radius:6px; }
  p { margin:.4em 0; } .en { color:var(--dim); font-size:14px; font-style:italic; }
  label, .rot { display:block; font-weight:700; margin:18px 0 6px; }
  input[type=email] { width:100%; padding:14px; border-radius:10px; border:2px solid var(--line); background:#0b0a12; color:var(--ink); font-size:17px; }
  input[type=email]:focus { outline:none; border-color:var(--pur); }
  .so { display:grid; grid-template-columns:1fr 1fr; gap:10px; }
  .so input { position:absolute; opacity:0; pointer-events:none; }
  .so label { margin:0; padding:14px; text-align:center; border:2px solid var(--line); border-radius:10px; background:#0b0a12; cursor:pointer; font-weight:800; letter-spacing:.06em; }
  .so input:checked + label { border-color:var(--pur); background:rgba(167,139,250,.16); color:#fff; }
  .so input:focus-visible + label { outline:2px solid var(--pur); outline-offset:2px; }
  button, .bt { display:block; width:100%; text-align:center; padding:15px; margin-top:16px; border:0; border-radius:12px; cursor:pointer;
           font:900 17px "Segoe UI", system-ui, sans-serif; letter-spacing:.06em; text-decoration:none; }
  .principal { background:var(--grn); color:#0b2316; }
  .principal:disabled { opacity:.6; cursor:wait; }
  .sec { background:#0b0a12; color:var(--ink); border:2px solid var(--line); }
  .sec:hover { border-color:var(--pur); }
  .token { font:700 clamp(14px, 4.3vw, 19px) Consolas, "SF Mono", monospace; background:#0b0a12; border:2px dashed var(--pur); border-radius:10px; padding:14px; text-align:center;
           word-break:break-all; color:var(--pur); margin-top:8px; user-select:all; }
  .aviso { background:rgba(167,139,250,.08); border:1px solid rgba(167,139,250,.4); border-radius:10px; padding:10px 12px; font-size:14px; margin-top:12px; }
  .erro { color:var(--pink); font-weight:700; min-height:1.5em; margin-top:10px; }
  ol { padding-left:20px; margin:8px 0 0; } li { margin:7px 0; }
  .k { color:var(--org); font-weight:800; } .b { color:var(--blu); font-weight:800; }
  .esc { position:absolute; left:-5000px; }
  footer { color:var(--dim); font-size:12px; text-align:center; margin-top:26px; }
  [hidden] { display:none !important; }
</style></head>
<body><main>
  <div class="logo">
    <svg width="64" height="40" viewBox="0 0 64 40" aria-hidden="true"><path d="M2 20 C 12 2, 20 2, 32 20 S 52 38, 62 20" fill="none" stroke="#ff8a3d" stroke-width="5" stroke-linecap="round"/><path d="M2 22 C 12 4, 20 4, 32 22 S 52 40, 62 22" fill="none" stroke="#3ba0ff" stroke-width="5" stroke-linecap="round" opacity=".9"/></svg>
    <div><h1><b>A</b>LIGN</h1><div class="risco"></div><div class="sub">KICK / BASS PHASE</div></div></div>

  <section class="card" id="passo1">
    <p><span class="free">GRÁTIS</span> &nbsp;Alinha a fase do <span class="k">kick</span> com o <span class="b">baixo</span> sozinho, com um botão só. Latência zero. VST3 e AU.</p>
    <p class="en">Free kick/bass phase aligner: one button, zero latency. VST3 / AU.</p>
    <form id="f" novalidate>
      <label for="email">Seu e-mail <span class="en">· your e-mail</span></label>
      <input id="email" type="email" autocomplete="email" required placeholder="voce@email.com">
      <span class="rot">Seu sistema <span class="en">· your system</span></span>
      <div class="so" role="radiogroup">
        <input type="radio" name="so" id="soWin" value="win"><label for="soWin">WINDOWS</label>
        <input type="radio" name="so" id="soMac" value="mac"><label for="soMac">MAC</label>
      </div>
      <input class="esc" name="site" id="site" tabindex="-1" autocomplete="off" aria-hidden="true">
      <button class="principal" id="ir" type="submit">GERAR MINHA LICENÇA</button>
      <div class="erro" id="erro"></div>
    </form>
    <p class="en" style="margin-top:8px">Licença para 1 computador. O e-mail é só para a sua licença. · 1 computer. Your e-mail is used only for your license.</p>
<!--REENVIO-->
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
    <div class="aviso" id="mandado" hidden>Também mandamos o token e o passo a passo para <b id="emailMandado"></b> (olhe o spam e Promoções). <span class="en">We also e-mailed it to you (check spam).</span></div>
    <span class="rot">1. Baixe e instale <span class="en">· download and install</span></span>
    <form id="fz" method="post" action="/gratis/align/zip">
      <input type="hidden" name="email" id="zEmail"><input type="hidden" name="token" id="zToken"><input type="hidden" name="sistema" id="zSo">
      <button class="principal" id="baixar" type="submit">BAIXAR ALIGN</button>
    </form>
    <p style="margin-top:10px">Vem um <b>.zip</b> com o instalador e o <b>LEIA-ME</b> com o seu e-mail, o token, o passo a passo e o que fazer se aparecer aviso de segurança. <span class="en">A .zip with the installer and a READ ME with your e-mail, token, steps and how to handle security warnings.</span></p>
    <p id="dicaSo" style="margin-top:10px"></p>
    <p class="en" style="margin-top:6px">Não baixou? <a id="direto" href="#" style="color:var(--pur)">Baixar só o instalador</a> · installer only</p>
    <span class="rot">2. Coloque no canal do baixo <span class="en">· put it on your bass</span></span>
    <ol>
      <li>Abra o seu DAW e coloque o <b>ALIGN</b> no canal do <span class="b">BAIXO</span>.</li>
      <li>No <b>Sidechain</b> do ALIGN, escolha a faixa do <span class="k">KICK</span> (no Ableton: abra o Sidechain do plugin, escolha a faixa do kick e deixe em Post FX).</li>
    </ol>
    <span class="rot">3. Ative <span class="en">· activate</span></span>
    <ol>
      <li>Na tela de ativação do ALIGN, digite <b id="emailOk"></b> e o token, e clique em <b>ATIVAR</b>.</li>
      <li>Vale para <b>1 computador</b>. Para trocar de PC: menu <b>≡</b> do plugin → <b>Licença…</b> → <b>Desativar este computador</b>, e ative no novo.</li>
      <li>Aperte o botão <b>ALIGN</b>. Pronto: ele corrige sozinho, batida por batida.</li>
    </ol>
    <p class="en">Put ALIGN on the bass, route the kick to its sidechain, type your e-mail and token, click ATIVAR. Works on 1 computer.</p>
  </section>
  <footer>InsideStudioSP</footer>
</main>
<script>
(function () {
  var $ = function (id) { return document.getElementById(id); };
  var mac = /Mac|iPhone|iPad/.test(navigator.platform || navigator.userAgent);
  $(mac ? 'soMac' : 'soWin').checked = true;
  $('f').addEventListener('submit', function (ev) {
    ev.preventDefault();
    var email = $('email').value.trim();
    var so = $('soMac').checked ? 'mac' : $('soWin').checked ? 'win' : '';
    $('erro').textContent = '';
    if (!/^[^@\\s]+@[^@\\s]+\\.[^@\\s]+$/.test(email)) { $('erro').textContent = 'Confira o e-mail.'; return; }
    if (!so) { $('erro').textContent = 'Escolha Windows ou Mac.'; return; }
    $('ir').disabled = true; $('ir').textContent = 'GERANDO…';
    fetch(location.pathname, { method: 'POST', headers: { 'content-type': 'application/json' },
      body: JSON.stringify({ email: email, sistema: so, site: $('site').value }) })
      .then(function (r) { return r.json(); })
      .then(function (j) {
        $('ir').disabled = false; $('ir').textContent = 'GERAR MINHA LICENÇA';
        if (!j.ok) { $('erro').textContent = j.mensagem || 'Não deu certo. Tente de novo.'; return; }
        $('direto').href = j.baixar[so] || '#';
        $('zEmail').value = email; $('zToken').value = j.token || ''; $('zSo').value = so;
        $('baixar').textContent = 'BAIXAR ALIGN PARA ' + (so === 'mac' ? 'MAC' : 'WINDOWS');
        $('dicaSo').textContent = so === 'mac'
          ? 'Abra o .dmg que vem no zip e rode o instalador (VST3 e AU). Depois reabra o DAW.'
          : 'Extraia o zip e rode o instalador .exe (VST3). Se o Windows avisar, clique em "Mais informações" e depois em "Executar assim mesmo". Depois reabra o DAW.';
        $('emailOk').textContent = email.toLowerCase();
        if (j.token) { $('token').textContent = j.token; $('comToken').hidden = false; }
        else {
          $('txtSem').innerHTML = j.ja_tinha
            ? '<b>Este e-mail já recebeu o ALIGN.</b> Use o token que apareceu da primeira vez. ' + (j.reenvio
                ? 'Perdeu? <a href="#" id="perdi2" style="color:var(--pur)">Receber um token novo no e-mail</a>.' : 'Perdeu? Chame a gente no direct.')
            : '<b>Pronto! O ALIGN entrou no seu cadastro.</b> Use o mesmo token que você já recebeu da InsideStudioSP.' + (j.reenvio
                ? ' Perdeu? <a href="#" id="perdi2" style="color:var(--pur)">Receber um token novo no e-mail</a>.' : '');
          $('semToken').hidden = false;
          if ($('perdi2')) $('perdi2').addEventListener('click', function (ev) { ev.preventDefault(); reenviar(email, $('txtSem')); });
        }
        if (j.email_enviado) { $('emailMandado').textContent = email.toLowerCase(); $('mandado').hidden = false; }
        $('passo1').hidden = true; $('passo2').hidden = false; window.scrollTo(0, 0);
      })
      .catch(function () { $('ir').disabled = false; $('ir').textContent = 'GERAR MINHA LICENÇA'; $('erro').textContent = 'Sem conexão. Tente de novo.'; });
  });
  function reenviar(email, onde) {
    onde.textContent = 'Enviando…';
    fetch('/gratis/align/reenviar', { method: 'POST', headers: { 'content-type': 'application/json' },
      body: JSON.stringify({ email: email, site: $('site').value }) })
      .then(function (r) { return r.json(); })
      .then(function (j) { onde.textContent = j.mensagem || 'Não deu certo. Tente de novo.'; })
      .catch(function () { onde.textContent = 'Sem conexão. Tente de novo.'; });
  }
  if ($('perdi')) $('perdi').addEventListener('click', function (ev) {
    ev.preventDefault();
    var email = $('email').value.trim();
    if (!/^[^@\\s]+@[^@\\s]+\\.[^@\\s]+$/.test(email)) { $('erro').textContent = 'Digite acima o e-mail que você cadastrou e clique de novo.'; $('email').focus(); return; }
    $('erro').textContent = ''; reenviar(email, $('msgPerdi'));
  });
  $('copiar').addEventListener('click', function () {
    var t = $('token').textContent;
    (navigator.clipboard ? navigator.clipboard.writeText(t) : Promise.reject()).then(function () { $('copiar').textContent = 'COPIADO ✓'; },
      function () { var s = window.getSelection(), r = document.createRange(); r.selectNodeContents($('token')); s.removeAllRanges(); s.addRange(r); });
  });
})();
</script>
</body></html>`;
