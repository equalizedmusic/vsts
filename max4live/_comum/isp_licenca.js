// ---------------------------------------------------------------- LICENÇA (comum aos devices Max for Live)
// Colado no FIM do motor (<device>.js) pelo build-<device>.js. O motor define antes: LIC_PACOTE ("rodo"/"vassoura"),
// LIC_NOME ("RODO"), VERSAO, e as funções licMudou() (avisa janela/device) e log().
// Mesmo servidor e mesmo protocolo dos VSTs (licenca/cliente/IspLicenca.h): POST /v1/ativar|validar|desativar com
// { pacote, email, token, maquina, nome_maquina, versao } -> { ok, licenca: { dados, assinatura } } (Ed25519).
// Arquivo: Windows <home>/AppData/Roaming/InsideStudioSP-<pacote>.lic · Mac <home>/Library/Application Support/...
// (pasta que sempre existe: o File do Max não cria pasta). Sem home reconhecível no caminho do device: pasta do device.
// Id do computador: número aleatório gravado uma vez em InsideStudioSP-m4l.id (mesma pasta), mandado como hash.
// Vale 45 dias sem internet; com mais de 3 dias confirma sozinho em segundo plano.
// Isto segura o repasse casual (zip + pasta copiada para outro PC); o código do device é texto aberto.

var LIC_URL = "https://isp-licencas.insidestudiosp.workers.dev/v1/";
var LIC_CHAVE = "e318ad516db6cd3f9d6963fe4952aad30c17f33f1981c4005c283a0cfdef8978";
var lic = { ok: false, email: "", token: "", validaAte: 0, emitida: 0, msg: "", ocupado: false, verificou: false };
var licEmailDigitado = "", licTokenDigitado = "";
var licConfirmou = false;

function licLiberada() { return !!lic.ok; }

// ---- caminhos
function licHome() {
	var p = pastaDoDevice();
	var m = /^(.*?\/Users\/[^\/]+)\//i.exec(String(p).replace(/\\/g, "/"));
	return m ? m[1] : "";
}
function licMac() { try { return String(max.os).toLowerCase().indexOf("mac") >= 0; } catch (e) { return false; } }
function licPasta() {
	var h = licHome();
	if (h) return h + (licMac() ? "/Library/Application Support/" : "/AppData/Roaming/");
	return pastaDoDevice();
}
function licArquivo() { return licPasta() + "InsideStudioSP-" + LIC_PACOTE + ".lic"; }
function licArquivoId() { return licPasta() + "InsideStudioSP-m4l.id"; }

function licLe(caminho) {
	try {
		var f = new File(caminho, "read", "TEXT");
		if (!f.isopen) return "";
		var s = "", parte;
		f.position = 0;
		while (f.position < f.eof) {
			parte = f.readstring(Math.min(4096, f.eof - f.position));
			if (parte == null || !parte.length) break;
			s += parte;
		}
		f.close();
		return s;
	} catch (e) { return ""; }
}
function licGrava(caminho, texto) {
	try {
		var f = new File(caminho, "write", "TEXT");
		if (!f.isopen) return false;
		try { f.eof = 0; } catch (e) { }
		f.writestring(texto);
		f.close();
		return true;
	} catch (e) { return false; }
}
function licApaga(caminho) { licGrava(caminho, ""); }

// ---- id deste computador (hash; o número bruto não sai daqui)
var licMaqCache = "";
function licMaquina() {
	if (licMaqCache) return licMaqCache;
	var id = licLe(licArquivoId()).replace(/[^0-9a-f]/g, "");
	if (id.length < 32) {
		id = "";
		for (var i = 0; i < 40; i++) id += Math.floor(Math.random() * 16).toString(16);
		id += (new Date().getTime()).toString(16);
		licGrava(licArquivoId(), id);
	}
	licMaqCache = hexDe(sha512(utf8("InsideStudioSP|m4l|" + id)).slice(0, 32));
	return licMaqCache;
}
function licNomeMaquina() {
	var h = licHome();
	var u = h ? h.replace(/^.*\//, "") : "";
	return (licMac() ? "Mac" : "Windows") + (u ? " · " + u : "") + " · Max for Live";
}

// ---- carregar / conferir
function licCarrega() {
	var txt = licLe(licArquivo());
	var lido = null;
	try { lido = txt ? JSON.parse(txt) : null; } catch (e) { lido = null; }
	lic.ok = false;
	if (lido) { lic.email = String(lido.email || ""); lic.token = String(lido.token || ""); }
	var erro = lido ? licConfere(lido) : "";
	if (lido && !erro) {
		lic.ok = true;
		lic.msg = "";
		var agoraS = Math.floor(new Date().getTime() / 1000);
		if (!licConfirmou && agoraS - lic.emitida > 3 * 86400) { licConfirmou = true; licPede("validar", lic.email, lic.token, false); }
	} else if (lido && lic.token && !licConfirmou) {
		licConfirmou = true;
		lic.msg = erro;
		licPede("validar", lic.email, lic.token, false);
	} else if (lido) lic.msg = erro;
	licMudou();
}

function licConfere(lido) {
	var dados, ass;
	try { dados = deB64(String(lido.dados || "")); ass = deB64(String(lido.assinatura || "")); } catch (e) { return "Licença danificada. Ative de novo."; }
	if (ass.length != 64) return "Licença danificada. Ative de novo.";
	var conf = edVerifica(deHex(LIC_CHAVE), ass, dados);
	if (conf === false) return "Licença danificada. Ative de novo.";
	var d = null;
	try { d = JSON.parse(deUtf8(dados)); } catch (e) { return "Licença danificada. Ative de novo."; }
	if (!d || d.v != 1 || d.pacote != LIC_PACOTE) return "Licença de outro produto.";
	if (d.maquina != licMaquina()) return "A licença gravada é de outro computador. Ative neste.";
	lic.emitida = Number(d.emitida) || 0;
	lic.validaAte = Number(d.valida_ate) || 0;
	if (Math.floor(new Date().getTime() / 1000) > lic.validaAte) return "A licença precisa ser confirmada pela internet (a cada 45 dias). Conecte e clique ACTIVATE.";
	lic.email = String(d.email || lic.email);
	return "";
}

// ---- conversa com o servidor
var licXhr = null;
function licPede(acao, email, token, mostrarErro) {
	if (typeof XMLHttpRequest == "undefined") { lic.msg = "Este Max não tem XMLHttpRequest: precisa do Live 12.2 ou mais novo."; licMudou(); return; }
	lic.ocupado = true;
	licMudou();
	var corpo = JSON.stringify({ pacote: LIC_PACOTE, email: email, token: token, maquina: licMaquina(),
		nome_maquina: licNomeMaquina(), versao: LIC_NOME + " " + VERSAO });
	try {
		var x = new XMLHttpRequest(); licXhr = x;   // referência guardada (o GC não leva)
		x.open("POST", LIC_URL + acao);
		try { x.setRequestHeader("Content-Type", "application/json"); } catch (e) { }
		x.onreadystatechange = function () {
			if (x.readyState != 4) return;
			var r = null;
			try { r = JSON.parse(x.responseText); } catch (e) { r = null; }
			licResposta(acao, email, token, r, mostrarErro);
		};
		x.send(corpo);
	} catch (e) {
		lic.ocupado = false;
		lic.msg = "Não consegui falar com o servidor de licenças (" + e + ").";
		licMudou();
	}
}

function licResposta(acao, email, token, r, mostrarErro) {
	lic.ocupado = false;
	if (!r || typeof r != "object") {
		if (mostrarErro || !lic.ok) lic.msg = "Não consegui falar com o servidor de licenças. Confira a internet e tente de novo.";
		licMudou();
		return;
	}
	if (r.ok) {
		if (acao == "desativar") {
			licApaga(licArquivo());
			lic.ok = false; lic.email = ""; lic.token = "";
			lic.msg = "Este computador foi desativado. A vaga ficou livre para outro.";
			log(LIC_NOME + ": licença desativada neste computador\n");
			licMudou();
			return;
		}
		var lid = r.licenca || {};
		var obj = { email: email, token: token, dados: lid.dados, assinatura: lid.assinatura };
		var erro = licConfere(obj);
		if (erro) { lic.ok = false; lic.msg = acao == "ativar" ? "A resposta do servidor não conferiu: " + erro : erro; licMudou(); return; }
		if (!licGrava(licArquivo(), JSON.stringify(obj))) log(LIC_NOME + ": não consegui gravar " + licArquivo() + " (vale até fechar o set)\n");
		lic.ok = true; lic.email = email; lic.token = token;
		lic.msg = acao == "ativar" ? "Ativado. Bom trabalho!" : "";
		log(LIC_NOME + ": licença " + (acao == "ativar" ? "ativada" : "confirmada") + " (" + email + ")\n");
		licMudou();
		return;
	}
	var erroS = String(r.erro || "");
	lic.msg = String(r.mensagem || erroS || "Recusado pelo servidor.");
	if (acao == "validar" && (erroS == "revogada" || erroS == "nao_encontrada" || erroS == "maquina_nao_ativada" || erroS == "teste_expirado")) {
		licApaga(licArquivo());
		lic.ok = false;
	}
	licMudou();
}

// ---- vindos da janela
function lic_email() { licEmailDigitado = str(arrayfromargs(arguments)).replace(/\s+/g, "").toLowerCase(); licMudou(); }
function lic_token() { licTokenDigitado = str(arrayfromargs(arguments)).replace(/\s+/g, "").toUpperCase(); licMudou(); }
function lic_ativar() {
	if (lic.ocupado) return;
	var e = licEmailDigitado || lic.email, t = licTokenDigitado || lic.token;
	if (!/^[^@\s]+@[^@\s]+\.[^@\s]+$/.test(e)) { lic.msg = "Digite o e-mail da compra."; licMudou(); return; }
	if (t.replace(/[^A-Z0-9]/g, "").length < 20) { lic.msg = "Digite o token (ISP-XXXX-XXXX-XXXX-XXXX-XXXX) que veio no e-mail."; licMudou(); return; }
	lic.msg = "Ativando…";
	licPede("ativar", e, t, true);
}
function lic_desativar() { if (!lic.ocupado && lic.token) { lic.msg = "Desativando…"; licPede("desativar", lic.email, lic.token, true); } }
function lic_comprar() { try { max.launchbrowser("https://insidestudiosp.com.br/" + LIC_PACOTE + "/"); } catch (e) { } }

// estado para a janela: lic <ok 0|1> <ocupado 0|1> e depois os textos em JSON (e-mail digitado, token, msg, validade)
function licEstado() {
	return JSON.stringify({ ok: lic.ok ? 1 : 0, ocupado: lic.ocupado ? 1 : 0, email: licEmailDigitado || lic.email,
		token: licTokenDigitado || (lic.ok ? "" : lic.token), msg: lic.msg, ate: lic.validaAte });
}

// ---------------------------------------------------------------- versão nova (GET /v1/versao, mesma rota dos VSTs)
// O motor define URL_BAIXAR (zip fixo do site) e URL_NOVIDADES. Sem internet ou com erro: fica quieto.
var nova = "";
var versaoChecada = false;
var verXhr = null;
function checaVersao() {
	if (versaoChecada) return;
	versaoChecada = true;
	if (typeof XMLHttpRequest == "undefined") { log(LIC_NOME + ": sem XMLHttpRequest, não confere versão nova\n"); return; }
	try {
		var x = new XMLHttpRequest(); verXhr = x;
		x.open("GET", LIC_URL + "versao?pacote=" + LIC_PACOTE + "&versao=" + VERSAO + "&sistema=" + (licMac() ? "mac" : "win"));
		x.onreadystatechange = function () {
			if (x.readyState != 4) return;
			try {
				var r = JSON.parse(x.responseText);
				var v = r && r.ok && r.atualizacao ? String(r.atualizacao.versao || "") : "";
				if (!/^\d+\.\d+\.\d+$/.test(v) || comparaVersao(v, VERSAO) <= 0) return;
				nova = v;
				log(LIC_NOME + ": versão nova " + v + " disponível · " + URL_BAIXAR + "\n");
				outlet(2, "nova", v);
				outlet(1, "nova", v);
			} catch (e) { log(LIC_NOME + ": resposta de versão inválida\n"); }
		};
		x.send();
	} catch (e) { log(LIC_NOME + ": não conferiu versão nova (" + e + ")\n"); }
}
function comparaVersao(a, b) {
	var pa = String(a).split("."), pb = String(b).split(".");
	for (var i = 0; i < 3; i++) { var d = (parseInt(pa[i], 10) || 0) - (parseInt(pb[i], 10) || 0); if (d) return d; }
	return 0;
}
function baixar_nova() { if (nova) max.launchbrowser(URL_BAIXAR); }
function novidades() { max.launchbrowser(URL_NOVIDADES); }

// ---------------------------------------------------------------- utilidades de bytes
function utf8(s) {
	s = unescape(encodeURIComponent(String(s)));
	var b = new Array(s.length);
	for (var i = 0; i < s.length; i++) b[i] = s.charCodeAt(i);
	return b;
}
function deUtf8(b) {
	var s = "";
	for (var i = 0; i < b.length; i++) s += String.fromCharCode(b[i]);
	try { return decodeURIComponent(escape(s)); } catch (e) { return s; }
}
function hexDe(b) { var s = ""; for (var i = 0; i < b.length; i++) s += (b[i] < 16 ? "0" : "") + b[i].toString(16); return s; }
function deHex(h) { var b = []; for (var i = 0; i + 1 < h.length; i += 2) b.push(parseInt(h.substr(i, 2), 16)); return b; }
var B64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
function deB64(s) {
	s = s.replace(/[^A-Za-z0-9+\/]/g, "");
	var out = [], buf = 0, n = 0;
	for (var i = 0; i < s.length; i++) {
		buf = (buf << 6) | B64.indexOf(s.charAt(i)); n += 6;
		if (n >= 8) { n -= 8; out.push((buf >> n) & 255); }
	}
	return out;
}

// ---------------------------------------------------------------- SHA-512 e Ed25519 (só conferência), com BigInt
// Sem BigInt (Max muito antigo): edVerifica devolve null = "não deu para conferir" e a licença vale pelo resto.
var SHA_K = null;
function sha512(bytes) {
	var B = BigInt, M = (B(1) << B(64)) - B(1);
	if (!SHA_K) {
		var k = ["428a2f98d728ae22", "7137449123ef65cd", "b5c0fbcfec4d3b2f", "e9b5dba58189dbbc", "3956c25bf348b538", "59f111f1b605d019", "923f82a4af194f9b", "ab1c5ed5da6d8118",
			"d807aa98a3030242", "12835b0145706fbe", "243185be4ee4b28c", "550c7dc3d5ffb4e2", "72be5d74f27b896f", "80deb1fe3b1696b1", "9bdc06a725c71235", "c19bf174cf692694",
			"e49b69c19ef14ad2", "efbe4786384f25e3", "0fc19dc68b8cd5b5", "240ca1cc77ac9c65", "2de92c6f592b0275", "4a7484aa6ea6e483", "5cb0a9dcbd41fbd4", "76f988da831153b5",
			"983e5152ee66dfab", "a831c66d2db43210", "b00327c898fb213f", "bf597fc7beef0ee4", "c6e00bf33da88fc2", "d5a79147930aa725", "06ca6351e003826f", "142929670a0e6e70",
			"27b70a8546d22ffc", "2e1b21385c26c926", "4d2c6dfc5ac42aed", "53380d139d95b3df", "650a73548baf63de", "766a0abb3c77b2a8", "81c2c92e47edaee6", "92722c851482353b",
			"a2bfe8a14cf10364", "a81a664bbc423001", "c24b8b70d0f89791", "c76c51a30654be30", "d192e819d6ef5218", "d69906245565a910", "f40e35855771202a", "106aa07032bbd1b8",
			"19a4c116b8d2d0c8", "1e376c085141ab53", "2748774cdf8eeb99", "34b0bcb5e19b48a8", "391c0cb3c5c95a63", "4ed8aa4ae3418acb", "5b9cca4f7763e373", "682e6ff3d6b2b8a3",
			"748f82ee5defb2fc", "78a5636f43172f60", "84c87814a1f0ab72", "8cc702081a6439ec", "90befffa23631e28", "a4506cebde82bde9", "bef9a3f7b2c67915", "c67178f2e372532b",
			"ca273eceea26619c", "d186b8c721c0c207", "eada7dd6cde0eb1e", "f57d4f7fee6ed178", "06f067aa72176fba", "0a637dc5a2c898a6", "113f9804bef90dae", "1b710b35131c471b",
			"28db77f523047d84", "32caab7b40c72493", "3c9ebe0a15c9bebc", "431d67c49c100d4c", "4cc5d4becb3e42b6", "597f299cfc657e2a", "5fcb6fab3ad6faec", "6c44198c4a475817"];
		SHA_K = k.map(function (h) { return B("0x" + h); });
	}
	var H = ["6a09e667f3bcc908", "bb67ae8584caa73b", "3c6ef372fe94f82b", "a54ff53a5f1d36f1", "510e527fade682d1", "9b05688c2b3e6c1f", "1f83d9abfb41bd6b", "5be0cd19137e2179"]
		.map(function (h) { return B("0x" + h); });
	var msg = bytes.slice(), bits = B(bytes.length) * B(8);
	msg.push(0x80);
	while (msg.length % 128 != 112) msg.push(0);
	for (var z = 0; z < 8; z++) msg.push(0);
	for (var y = 7; y >= 0; y--) msg.push(Number((bits >> B(8 * y)) & B(255)));
	function rot(x, n) { return ((x >> B(n)) | (x << B(64 - n))) & M; }
	var w = new Array(80);
	for (var o = 0; o < msg.length; o += 128) {
		for (var t = 0; t < 16; t++) {
			var v = B(0);
			for (var q = 0; q < 8; q++) v = (v << B(8)) | B(msg[o + t * 8 + q]);
			w[t] = v;
		}
		for (t = 16; t < 80; t++) {
			var s0 = rot(w[t - 15], 1) ^ rot(w[t - 15], 8) ^ (w[t - 15] >> B(7));
			var s1 = rot(w[t - 2], 19) ^ rot(w[t - 2], 61) ^ (w[t - 2] >> B(6));
			w[t] = (w[t - 16] + s0 + w[t - 7] + s1) & M;
		}
		var a = H[0], b = H[1], c = H[2], d = H[3], e = H[4], f = H[5], g = H[6], h = H[7];
		for (t = 0; t < 80; t++) {
			var S1 = rot(e, 14) ^ rot(e, 18) ^ rot(e, 41);
			var ch = (e & f) ^ (~e & M & g);
			var t1 = (h + S1 + ch + SHA_K[t] + w[t]) & M;
			var S0 = rot(a, 28) ^ rot(a, 34) ^ rot(a, 39);
			var mj = (a & b) ^ (a & c) ^ (b & c);
			var t2 = (S0 + mj) & M;
			h = g; g = f; f = e; e = (d + t1) & M; d = c; c = b; b = a; a = (t1 + t2) & M;
		}
		H[0] = (H[0] + a) & M; H[1] = (H[1] + b) & M; H[2] = (H[2] + c) & M; H[3] = (H[3] + d) & M;
		H[4] = (H[4] + e) & M; H[5] = (H[5] + f) & M; H[6] = (H[6] + g) & M; H[7] = (H[7] + h) & M;
	}
	var out = [];
	for (var i = 0; i < 8; i++) for (var j = 7; j >= 0; j--) out.push(Number((H[i] >> B(8 * j)) & B(255)));
	return out;
}

// Ed25519: confere [S]B == R + [k]A, com k = SHA512(R || A || M) mod L. true/false; null = sem BigInt aqui.
function edVerifica(pk, sig, msg) {
	if (typeof BigInt == "undefined") return null;
	try {
		var B = BigInt;
		var P = (B(1) << B(255)) - B(19);
		var L = (B(1) << B(252)) + B("27742317777372353535851937790883648493");
		var D = B("37095705934669439343138083508754565189542113879843219016388785533085940283555");
		var I = B("19681161376707505956807079304988542015446066515923890162744021073123829784752");   // sqrt(-1)
		function md(a) { a %= P; return a < 0 ? a + P : a; }
		function pw(b, e) { var r = B(1); b = md(b); while (e > 0) { if (e & B(1)) r = r * b % P; b = b * b % P; e >>= B(1); } return r; }
		function inv(a) { return pw(a, P - B(2)); }
		function leLE(bs) { var n = B(0); for (var i = bs.length - 1; i >= 0; i--) n = (n << B(8)) | B(bs[i]); return n; }
		// pontos em coordenadas estendidas [X, Y, Z, T]
		function soma(p, q) {
			var A = md((p[1] - p[0]) * (q[1] - q[0])), Bb = md((p[1] + p[0]) * (q[1] + q[0]));
			var C = md(B(2) * p[3] * q[3] * D), Dd = md(B(2) * p[2] * q[2]);
			var E = Bb - A, F = Dd - C, G = Dd + C, H = Bb + A;
			return [md(E * F), md(G * H), md(F * G), md(E * H)];
		}
		function mult(s, p) { var q = [B(0), B(1), B(1), B(0)]; while (s > 0) { if (s & B(1)) q = soma(q, p); p = soma(p, p); s >>= B(1); } return q; }
		function descomprime(bs) {
			var b2 = bs.slice(); var sinal = b2[31] >> 7; b2[31] &= 127;
			var y = leLE(b2); if (y >= P) return null;
			var u = md(y * y - B(1)), v = md(D * y * y + B(1));
			var x2 = md(u * inv(v));
			var x = pw(x2, (P + B(3)) / B(8));
			if (md(x * x - x2) != B(0)) x = md(x * I);
			if (md(x * x - x2) != B(0)) return null;
			if (Number(x & B(1)) != sinal) x = P - x;
			if (x == B(0) && sinal) return null;
			return [x, y, B(1), md(x * y)];
		}
		function igual(p, q) { return md(p[0] * q[2] - q[0] * p[2]) == B(0) && md(p[1] * q[2] - q[1] * p[2]) == B(0); }
		var Gy = md(B(4) * inv(B(5)));
		var Gb = []; var gy = Gy; for (var i = 0; i < 32; i++) { Gb.push(Number(gy & B(255))); gy >>= B(8); }
		var G = descomprime(Gb);
		var A = descomprime(pk), R = descomprime(sig.slice(0, 32));
		if (!A || !R) return false;
		var S = leLE(sig.slice(32, 64));
		if (S >= L) return false;
		var k = leLE(sha512(sig.slice(0, 32).concat(pk, msg))) % L;
		return igual(mult(S, G), soma(R, mult(k, A)));
	} catch (e) { return null; }
}
