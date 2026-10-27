// Serviço de ativação dos plugins InsideStudioSP (Cloudflare Workers + D1).
//
// Rotas do plugin (sem senha; quem prova a compra é o par e-mail + token):
//   POST /v1/ativar     { pacote, email, token, maquina, nome_maquina, versao }
//   POST /v1/validar    { pacote, email, token, maquina, versao }      (o plugin chama de tempos em tempos)
//   POST /v1/desativar  { pacote, email, token, maquina }
//   GET  /v1/versao?pacote=mix&versao=1.0.0&sistema=win|mac   (o plugin já ativado pergunta ao abrir)
//        -> { ok: true, atualizacao: null | { versao, url, notas } }   url = instalador do sistema (ou a página do release)
// Resposta: { ok: true, licenca: { dados, assinatura } }  ou  { ok: false, erro, mensagem }
//   dados = base64 do JSON { v, pacote, email, maquina, emitida, valida_ate }
//   assinatura = base64 da assinatura Ed25519 dos bytes de dados (chave pública embutida no plugin)
//
// Rotas de administração (cabeçalho Authorization: Bearer <ADMIN_KEY>): ver admin/isp-licenca.mjs
//   POST /admin/licencas          { email, pacote, origem?, pedido?, nota?, max_maquinas? }  -> devolve o token (só aqui)
//   GET  /admin/licencas?email=   lista com as máquinas
//   POST /admin/revogar           { id }
//   POST /admin/reativar          { id }
//   POST /admin/liberar-maquina   { id, maquina }
//   POST /admin/novo-token        { id }   gera outro token para a mesma licença (o antigo para de valer)
//
// Segredos (wrangler secret put): SIGN_KEY (Ed25519 privada, PKCS8 em base64), ADMIN_KEY, GITHUB_TOKEN (opcional, só leitura).

const PACOTES = ['criativo', 'mix', 'knob', 'quimera', 'limao', 'mercurio'];
const VALIDADE_DIAS = 45;          // sem internet, o plugin segue liberado até aqui
const TENTATIVAS_POR_IP = 20;      // recusas em 15 min antes de bloquear o IP
const JANELA_TENTATIVAS = 15 * 60;
// versão mais nova de cada pacote = o release <pacote>-vX.Y.Z mais alto do GitHub (repo precisa estar público,
// ou o segredo GITHUB_TOKEN com leitura do repo). Se o GitHub não responder, ninguém recebe aviso e nada quebra.
const RELEASES_URL = 'https://api.github.com/repos/equalizedmusic/vsts/releases?per_page=100';
const CACHE_RELEASES = 30 * 60;    // segundos

const MENSAGENS = {
    pedido_invalido: 'Pedido inválido.',
    pacote_invalido: 'Pacote desconhecido.',
    nao_encontrada: 'E-mail ou token não conferem. Confira os dois como vieram no e-mail da compra.',
    revogada: 'Esta licença foi cancelada. Fale com o suporte da InsideStudioSP.',
    limite_maquinas: 'Esta licença já está ativa no número máximo de computadores. Desative em um deles (menu da licença no plugin) e tente de novo.',
    maquina_nao_ativada: 'Este computador não está ativado nesta licença. Ative de novo com o e-mail e o token.',
    muitas_tentativas: 'Muitas tentativas seguidas. Espere 15 minutos e tente de novo.',
    nao_autorizado: 'Não autorizado.',
    ja_existe: 'Já existe licença deste pacote para este e-mail.',
    erro_interno: 'Erro no servidor de licenças. Tente de novo em alguns minutos.',
};

const agora = () => Math.floor(Date.now() / 1000);

function resposta(obj, status = 200) {
    return new Response(JSON.stringify(obj), {
        status,
        headers: { 'content-type': 'application/json; charset=utf-8', 'cache-control': 'no-store' },
    });
}
const falha = (erro, status = 400) => resposta({ ok: false, erro, mensagem: MENSAGENS[erro] || erro }, status);

const normEmail = (s) => String(s || '').trim().toLowerCase();
// token: ignora espaços, hífens, caixa e o prefixo ISP (o cliente pode colar "isp-abcd-..." de qualquer jeito)
const normToken = (s) => String(s || '').toUpperCase().replace(/[^0-9A-Z]/g, '').replace(/^ISP/, '');

async function sha256Hex(texto) {
    const h = await crypto.subtle.digest('SHA-256', new TextEncoder().encode(texto));
    return [...new Uint8Array(h)].map((b) => b.toString(16).padStart(2, '0')).join('');
}

function b64(bytes) {
    let s = '';
    for (const b of bytes) s += String.fromCharCode(b);
    return btoa(s);
}
function deB64(texto) {
    const s = atob(texto);
    const out = new Uint8Array(s.length);
    for (let i = 0; i < s.length; i++) out[i] = s.charCodeAt(i);
    return out;
}

// token no formato ISP-XXXX-XXXX-XXXX-XXXX-XXXX (alfabeto sem 0/O/1/I/L/U para não confundir ao digitar)
const ALFABETO = '23456789ABCDEFGHJKMNPQRSTVWXYZ';
function novoToken() {
    const bytes = crypto.getRandomValues(new Uint8Array(20));
    let s = '';
    for (const b of bytes) s += ALFABETO[b % ALFABETO.length];   // 20 × log2(30) ≈ 98 bits
    return 'ISP-' + s.match(/.{4}/g).join('-');
}

let chaveCache = null;
async function chaveAssinatura(env) {
    if (!chaveCache)
        chaveCache = await crypto.subtle.importKey('pkcs8', deB64(env.SIGN_KEY), { name: 'Ed25519' }, false, ['sign']);
    return chaveCache;
}

async function emitir(env, lic, maquina) {
    const t = agora();
    const dados = new TextEncoder().encode(JSON.stringify({
        v: 1, pacote: lic.pacote, email: lic.email, maquina, emitida: t, valida_ate: t + VALIDADE_DIAS * 86400,
    }));
    const assinatura = new Uint8Array(await crypto.subtle.sign({ name: 'Ed25519' }, await chaveAssinatura(env), dados));
    return { dados: b64(dados), assinatura: b64(assinatura) };
}

async function evento(env, licencaId, tipo, detalhe, ip) {
    await env.DB.prepare('INSERT INTO eventos (licenca_id, tipo, detalhe, ip, quando) VALUES (?, ?, ?, ?, ?)')
        .bind(licencaId ?? null, tipo, detalhe ?? null, ip ?? null, agora()).run();
}

async function ipBloqueado(env, ip) {
    const r = await env.DB.prepare("SELECT COUNT(*) AS n FROM eventos WHERE ip = ? AND tipo = 'recusou' AND quando > ?")
        .bind(ip, agora() - JANELA_TENTATIVAS).first();
    return (r?.n || 0) >= TENTATIVAS_POR_IP;
}

// ---------------- rotas do plugin ----------------

async function rotaPlugin(acao, req, env) {
    const ip = req.headers.get('cf-connecting-ip') || '';
    let corpo;
    try { corpo = await req.json(); } catch { return falha('pedido_invalido'); }

    const pacote = String(corpo.pacote || '');
    const email = normEmail(corpo.email);
    const token = normToken(corpo.token);
    const maquina = String(corpo.maquina || '');
    if (!PACOTES.includes(pacote)) return falha('pacote_invalido');
    if (!email || !token || !/^[0-9a-f]{32,128}$/.test(maquina)) return falha('pedido_invalido');

    if (await ipBloqueado(env, ip)) return falha('muitas_tentativas', 429);

    const recusa = async (erro, licId, status = 403) => {
        await evento(env, licId, 'recusou', `${acao}: ${erro} (${email}, ${pacote})`, ip);
        return falha(erro, status);
    };

    const hash = await sha256Hex('ISP' + token);   // o token guardado já normalizado (sem "-")
    const lic = await env.DB.prepare('SELECT * FROM licencas WHERE token_hash = ? AND email = ? AND pacote = ?')
        .bind(hash, email, pacote).first();
    if (!lic) return recusa('nao_encontrada', null);
    if (lic.status !== 'ativa') return recusa('revogada', lic.id);

    const atual = await env.DB.prepare('SELECT * FROM ativacoes WHERE licenca_id = ? AND maquina = ?')
        .bind(lic.id, maquina).first();

    if (acao === 'desativar') {
        if (atual) await env.DB.prepare('DELETE FROM ativacoes WHERE id = ?').bind(atual.id).run();
        await evento(env, lic.id, 'desativou', atual?.nome_maquina || maquina, ip);
        return resposta({ ok: true });
    }

    if (acao === 'validar') {
        if (!atual) return recusa('maquina_nao_ativada', lic.id);
        await env.DB.prepare('UPDATE ativacoes SET visto_em = ? WHERE id = ?').bind(agora(), atual.id).run();
        await evento(env, lic.id, 'validou', `${atual.nome_maquina || ''} ${corpo.versao || ''}`.trim(), ip);
        return resposta({ ok: true, licenca: await emitir(env, lic, maquina) });
    }

    // ativar
    const nome = String(corpo.nome_maquina || '').slice(0, 80);
    if (atual) {
        await env.DB.prepare('UPDATE ativacoes SET visto_em = ?, nome_maquina = ? WHERE id = ?').bind(agora(), nome, atual.id).run();
    } else {
        const n = await env.DB.prepare('SELECT COUNT(*) AS n FROM ativacoes WHERE licenca_id = ?').bind(lic.id).first();
        if ((n?.n || 0) >= lic.max_maquinas) {
            await evento(env, lic.id, 'limite', nome, ip);
            return falha('limite_maquinas', 403);
        }
        await env.DB.prepare('INSERT INTO ativacoes (licenca_id, maquina, nome_maquina, criada_em, visto_em) VALUES (?, ?, ?, ?, ?)')
            .bind(lic.id, maquina, nome, agora(), agora()).run();
    }
    await evento(env, lic.id, 'ativou', `${nome} ${corpo.versao || ''}`.trim(), ip);
    return resposta({ ok: true, licenca: await emitir(env, lic, maquina) });
}

// ---------------- aviso de versão nova ----------------

const partesVersao = (v) => (String(v || '').match(/(\d+)\.(\d+)\.(\d+)/) || []).slice(1).map(Number);
function compararVersao(a, b) {
    const x = partesVersao(a), y = partesVersao(b);
    if (x.length !== 3 || y.length !== 3) return 0;
    for (let i = 0; i < 3; i++) if (x[i] !== y[i]) return x[i] < y[i] ? -1 : 1;
    return 0;
}

// lista guardada na memória do Worker (o Cache API não funciona no workers.dev): 30 min, ou 5 min se o GitHub falhou
let releasesMem = null;   // { ate, lista }
async function releases(env) {
    if (releasesMem && agora() < releasesMem.ate) return releasesMem.lista;
    const h = { 'user-agent': 'isp-licencas', accept: 'application/vnd.github+json' };
    if (env.GITHUB_TOKEN) h.authorization = 'Bearer ' + env.GITHUB_TOKEN;
    let lista = null;
    try {
        const g = await fetch(RELEASES_URL, { headers: h });
        if (g.ok) lista = (await g.json()).filter((x) => !x.draft && !x.prerelease).map((x) => ({
            tag: x.tag_name, pagina: x.html_url, notas: String(x.body || '').slice(0, 800),
            arquivos: (x.assets || []).map((a) => a.browser_download_url),
        }));
        else console.error('github releases', g.status);
    } catch (e) { console.error('github releases', e); }
    releasesMem = { ate: agora() + (lista ? CACHE_RELEASES : 300), lista: lista || releasesMem?.lista || [] };
    return releasesMem.lista;
}

async function rotaVersao(url, env) {
    const pacote = String(url.searchParams.get('pacote') || '');
    const versao = String(url.searchParams.get('versao') || '');
    const sistema = url.searchParams.get('sistema') === 'mac' ? 'mac' : 'win';
    if (!PACOTES.includes(pacote)) return falha('pacote_invalido');
    if (partesVersao(versao).length !== 3) return falha('pedido_invalido');

    let maior = null;
    for (const rel of await releases(env)) {
        const m = rel.tag.match(new RegExp(`^${pacote}-v(\\d+\\.\\d+\\.\\d+)$`));
        if (m && (!maior || compararVersao(m[1], maior.versao) > 0)) maior = { ...rel, versao: m[1] };
    }
    if (!maior || compararVersao(maior.versao, versao) <= 0) return resposta({ ok: true, atualizacao: null });
    const ext = sistema === 'mac' ? '.dmg' : '.exe';
    const instalador = maior.arquivos.find((a) => a.toLowerCase().endsWith(ext));
    return resposta({ ok: true, atualizacao: { versao: maior.versao, url: instalador || maior.pagina, notas: maior.notas } });
}

// ---------------- administração ----------------

function iguais(a, b) {
    if (a.length !== b.length) return false;
    let d = 0;
    for (let i = 0; i < a.length; i++) d |= a.charCodeAt(i) ^ b.charCodeAt(i);
    return d === 0;
}

async function rotaAdmin(caminho, req, env, url) {
    const auth = req.headers.get('authorization') || '';
    if (!env.ADMIN_KEY || !iguais(auth, 'Bearer ' + env.ADMIN_KEY)) return falha('nao_autorizado', 401);

    if (caminho === '/admin/licencas' && req.method === 'GET') {
        const email = normEmail(url.searchParams.get('email'));
        const lics = email
            ? await env.DB.prepare('SELECT * FROM licencas WHERE email = ? ORDER BY id').bind(email).all()
            : await env.DB.prepare('SELECT * FROM licencas ORDER BY id DESC LIMIT 500').all();
        const ativ = await env.DB.prepare('SELECT * FROM ativacoes ORDER BY id').all();
        const porLic = {};
        for (const a of ativ.results) (porLic[a.licenca_id] ||= []).push(a);
        return resposta({
            ok: true,
            licencas: lics.results.map(({ token_hash, ...l }) => ({ ...l, maquinas: porLic[l.id] || [] })),
        });
    }

    let corpo;
    try { corpo = await req.json(); } catch { return falha('pedido_invalido'); }

    if (caminho === '/admin/licencas' && req.method === 'POST') {
        const email = normEmail(corpo.email);
        const pacote = String(corpo.pacote || '');
        if (!PACOTES.includes(pacote)) return falha('pacote_invalido');
        if (!/^[^@\s]+@[^@\s]+\.[^@\s]+$/.test(email)) return falha('pedido_invalido');
        const existe = await env.DB.prepare('SELECT id FROM licencas WHERE email = ? AND pacote = ?').bind(email, pacote).first();
        if (existe) return falha('ja_existe', 409);
        const token = novoToken();
        const r = await env.DB.prepare(
            'INSERT INTO licencas (email, pacote, token_hash, max_maquinas, origem, pedido, nota, criada_em) VALUES (?, ?, ?, ?, ?, ?, ?, ?)')
            .bind(email, pacote, await sha256Hex('ISP' + normToken(token)), Number(corpo.max_maquinas) || 2,
                corpo.origem === 'venda' ? 'venda' : 'cortesia', corpo.pedido ?? null, corpo.nota ?? null, agora())
            .run();
        const id = r.meta.last_row_id;
        await evento(env, id, 'criou', corpo.origem || 'cortesia', null);
        return resposta({ ok: true, id, email, pacote, token });
    }

    const id = Number(corpo.id);
    const lic = id ? await env.DB.prepare('SELECT * FROM licencas WHERE id = ?').bind(id).first() : null;
    if (!lic) return falha('nao_encontrada', 404);

    if (caminho === '/admin/revogar' || caminho === '/admin/reativar') {
        const rev = caminho === '/admin/revogar';
        await env.DB.prepare('UPDATE licencas SET status = ?, revogada_em = ? WHERE id = ?')
            .bind(rev ? 'revogada' : 'ativa', rev ? agora() : null, id).run();
        await evento(env, id, rev ? 'revogou' : 'reativou', corpo.nota ?? null, null);
        return resposta({ ok: true });
    }
    if (caminho === '/admin/liberar-maquina') {
        await env.DB.prepare('DELETE FROM ativacoes WHERE licenca_id = ? AND (maquina = ? OR id = ?)')
            .bind(id, String(corpo.maquina || ''), Number(corpo.maquina) || -1).run();
        await evento(env, id, 'liberou_maquina', String(corpo.maquina || ''), null);
        return resposta({ ok: true });
    }
    if (caminho === '/admin/novo-token') {
        const token = novoToken();
        await env.DB.prepare('UPDATE licencas SET token_hash = ? WHERE id = ?').bind(await sha256Hex('ISP' + normToken(token)), id).run();
        await evento(env, id, 'novo_token', null, null);
        return resposta({ ok: true, id, email: lic.email, pacote: lic.pacote, token });
    }
    return falha('pedido_invalido', 404);
}

export default {
    async fetch(req, env) {
        const url = new URL(req.url);
        const caminho = url.pathname.replace(/\/+$/, '');
        try {
            if (req.method === 'POST' && caminho === '/v1/ativar') return await rotaPlugin('ativar', req, env);
            if (req.method === 'POST' && caminho === '/v1/validar') return await rotaPlugin('validar', req, env);
            if (req.method === 'POST' && caminho === '/v1/desativar') return await rotaPlugin('desativar', req, env);
            if (req.method === 'GET' && caminho === '/v1/versao') return await rotaVersao(url, env);
            if (caminho.startsWith('/admin/')) return await rotaAdmin(caminho, req, env, url);
            if (caminho === '' || caminho === '/v1') return resposta({ ok: true, servico: 'licencas InsideStudioSP' });
            return falha('pedido_invalido', 404);
        } catch (e) {
            console.error(e);
            return falha('erro_interno', 500);
        }
    },
};
