const fs = require('fs'), crypto = require('crypto'), vm = require('vm');
const src = fs.readFileSync(require('path').join(__dirname, '../_comum/isp_licenca.js'), 'utf8');
const ctx = { BigInt, JSON, Math, Date, String, Number, encodeURIComponent, decodeURIComponent, escape, unescape, parseInt, console };
vm.createContext(ctx); vm.runInContext(src, ctx);
// sha512
for (const s of ['', 'abc', 'x'.repeat(300), 'InsideStudioSP|m4l|çã']) {
  const a = ctx.hexDe(ctx.sha512(ctx.utf8(s))), b = crypto.createHash('sha512').update(s, 'utf8').digest('hex');
  console.log('sha512', s.length, a === b);
}
// ed25519 par novo
const { publicKey, privateKey } = crypto.generateKeyPairSync('ed25519');
const pk = [...publicKey.export({ format: 'der', type: 'spki' }).slice(-32)];
const msg = Buffer.from(JSON.stringify({ v: 1, pacote: 'rodo', maquina: 'ab' }));
const sig = [...crypto.sign(null, msg, privateKey)];
let t = Date.now();
console.log('verifica ok', ctx.edVerifica(pk, sig, [...msg]), Date.now() - t, 'ms');
const m2 = Buffer.from(msg); m2[3] ^= 1;
console.log('msg alterada', ctx.edVerifica(pk, sig, [...m2]));
const s2 = sig.slice(); s2[40] ^= 1; console.log('sig alterada', ctx.edVerifica(pk, s2, [...msg]));
// chave real do servidor
const seg = JSON.parse(fs.readFileSync((process.env.ISP_ED25519 || (() => { throw new Error('defina ISP_ED25519'); })()), 'utf8'));
console.log('chaves do segredo:', Object.keys(seg));
const priv = crypto.createPrivateKey({ key: Buffer.from(seg.privada_pkcs8_b64 || seg.privada || seg.private || seg.sign_key || Object.values(seg).find(v => typeof v === 'string' && v.length > 60), 'base64'), format: 'der', type: 'pkcs8' });
const pub = [...crypto.createPublicKey(priv).export({ format: 'der', type: 'spki' }).slice(-32)];
console.log('pub do segredo == LIC_CHAVE', ctx.hexDe(pub) === ctx.LIC_CHAVE);
const dados = Buffer.from(JSON.stringify({ v: 1, pacote: 'rodo', email: 'a@b.c', maquina: 'x', emitida: 1, valida_ate: 2 }));
const sg = crypto.sign(null, dados, priv);
console.log('real', ctx.edVerifica(ctx.deHex(ctx.LIC_CHAVE), [...sg], [...dados]));
console.log('b64', ctx.hexDe(ctx.deB64(sg.toString('base64'))) === sg.toString('hex'));
