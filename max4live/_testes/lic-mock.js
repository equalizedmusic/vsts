// Max falso p/ a licença: File com caminho C:/Users/teste -> pasta temp, XMLHttpRequest -> servidor falso que assina com a chave real
const fs = require('fs'), path = require('path'), crypto = require('crypto');
module.exports = (LOGDIR, getMotor) => {
  const seg = JSON.parse(fs.readFileSync((process.env.ISP_ED25519 || (() => { throw new Error('defina ISP_ED25519 = caminho do licenca-ed25519.json (Bitwarden > Estúdio)'); })()), 'utf8'));
  const priv = crypto.createPrivateKey({ key: Buffer.from(seg.sign_key_pkcs8_b64, 'base64'), format: 'der', type: 'pkcs8' });
  // tudo cai no LOGDIR: C:/Users/teste -> u; outros perfis (Public...) e /Users (Mac) -> pastas próprias; nada grava fora
  const mapa = (p) => { const q = p.replace(/^C:\/Users\/teste/, LOGDIR + '/u').replace(/^[A-Za-z]:\/Users\//, LOGDIR + '/users/').replace(/^\/Users\//, LOGDIR + '/mac/').replace(/^([A-Za-z]):\//, (m, d) => LOGDIR + '/drv-' + d + '/'); fs.mkdirSync(path.dirname(q), { recursive: true }); return q; };
  const ativas = global.SERV_ATIVAS = global.SERV_ATIVAS || {};
  class File {
    constructor(p, mode) { this.p = mapa(p); if (mode === 'write') fs.writeFileSync(this.p, ''); this.isopen = mode === 'write' || fs.existsSync(this.p); this.position = 0; }
    get eof() { return fs.existsSync(this.p) ? fs.statSync(this.p).size : 0; } set eof(v) { fs.truncateSync(this.p, v); }
    writestring(x) { fs.appendFileSync(this.p, x); }
    readstring(n) { const b = fs.readFileSync(this.p, 'utf8').slice(this.position, this.position + n); this.position += Buffer.byteLength(b); return b; }
    close() {}
  }
  class XMLHttpRequest {
    open(m, url) { this.m = m; this.url = url; } setRequestHeader() {}
    send(corpo) {
      setTimeout(() => {
        let r;
        if (/versao/.test(this.url)) r = { ok: true, atualizacao: global.NOVA ? { versao: global.NOVA, url: 'https://x' } : null };
        else {
          const c = JSON.parse(corpo), acao = this.url.split('/').pop();
          global.PEDIDOS = (global.PEDIDOS || []).concat([{ acao, c }]);
          if (acao === 'recuperar') { if (!ativas[c.pacote + '|' + c.maquina]) r = { ok: false, erro: 'maquina_nao_ativada' }; else { c.email = 'cliente@teste.com'; const t = Math.floor(Date.now() / 1000); const dados = Buffer.from(JSON.stringify({ v: 1, pacote: c.pacote, email: c.email, maquina: c.maquina, emitida: t, valida_ate: t + 45 * 86400 })); r = { ok: true, licenca: { dados: dados.toString('base64'), assinatura: crypto.sign(null, dados, priv).toString('base64') } }; } }
          else if (acao === 'ativar' && global.LIMITE && !ativas[c.pacote + '|' + c.maquina] && Object.keys(ativas).length >= global.LIMITE) r = { ok: false, erro: 'limite_maquinas', mensagem: 'limite' };
          else if (c.email !== 'cliente@teste.com' || c.token !== 'ISP-AAAA-BBBB-CCCC-DDDD-EEEE') r = { ok: false, erro: 'nao_encontrada', mensagem: 'E-mail ou token não conferem.' };
          else if (global.SO_PACOTES && !global.SO_PACOTES.includes(c.pacote) && acao !== 'desativar') r = { ok: false, erro: 'pacote_nao_incluido', mensagem: 'sem este pacote' };
          else if (acao === 'desativar') { delete ativas[c.pacote + '|' + c.maquina]; r = { ok: true }; }
          else if (acao === 'validar' && !ativas[c.pacote + '|' + c.maquina]) r = { ok: false, erro: 'maquina_nao_ativada', mensagem: 'nao ativada' };
          else {
            ativas[c.pacote + '|' + c.maquina] = 1;
            const t = Math.floor(Date.now() / 1000) - (global.EMITIDA_ATRAS || 0);
            const dados = Buffer.from(JSON.stringify({ v: 1, pacote: c.pacote, email: c.email, maquina: c.maquina, emitida: t, valida_ate: t + 45 * 86400 }));
            r = { ok: true, licenca: { dados: dados.toString('base64'), assinatura: crypto.sign(null, dados, priv).toString('base64') } };
          }
        }
        this.readyState = 4; this.status = 200; this.responseText = JSON.stringify(r); this.onreadystatechange();
      }, 5);
    }
  }
  return { File, XMLHttpRequest, BigInt, escape, unescape, encodeURIComponent, decodeURIComponent, parseInt,
    max: { os: 'windows', version: '9.0.5', launchbrowser: (u) => console.log('  (abre navegador) ' + u) },
    arrayfromargs: (a) => Array.prototype.slice.call(a) };
};
