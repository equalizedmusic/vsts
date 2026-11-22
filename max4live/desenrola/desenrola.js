// DESENROLA 0.1 — tira a música do loop. Você tem um loop no Arrangement (todas as faixas, 8/16/32 compassos, marcado
// com o loop brace) e uma faixa REF com a música de referência. ANALYZE:
//   1. lê a referência (o arquivo do clip da faixa REF) e acha as partes: INTRO, GROOVE, BUILD, DROP, BREAK, OUTRO;
//   2. lê as faixas que têm clip dentro do loop e põe cada uma num grupo pelo nome (os 8 do MAESTRO).
// Na janela você ajusta as partes (tipo e tamanho) e o que toca em cada uma (faixa × parte). BUILD monta o rascunho
// DEPOIS do loop, na mesma linha do tempo: cada parte recebe cópias dos clips das faixas ligadas nela
// (duplicate_clip_to_arrangement), repetidas até encher a parte. Nada é criado do zero, nenhuma nota é escrita.
// O loop original não é mexido. LOCATORS ON põe um locator com o nome de cada parte. UNDO apaga o rascunho.
// Roda no [v8] (Max 9 / Live 12.2+).

autowatch = 1;
inlets = 1;
outlets = 4; // 0: buffer~   1: status (device)   2: janela   3: eco dos controles

var BUF = "desenrola_ref";
var DICT = "desenrola_data";
var VERSAO = "0.1.0";
var BUILD = "teste";      // marca do pacote de teste; vazio na versão publicada
var LIC_TESTE = true;     // grátis: sem licença
var LIC_PACOTE = "desenrola", LIC_NOME = "DESENROLA";
var URL_BAIXAR = "https://insidestudiosp.com.br/baixar/desenrola/zip";
var URL_NOVIDADES = "https://insidestudiosp.com.br/desenrola/";
var PAUSA = 8;            // ms entre um passo e outro (o Live respira)
var POR_PASSO = 3;        // cópias de clip por passo no BUILD
var PATCHER = (typeof this != "undefined" && this && this.patcher) ? this.patcher : null;

var GRUPOS = ["KICK", "BASS", "DRUMS", "HATS", "SYNTHS", "AMBIENCE", "VOCAL", "FX", "OTHER"];
var CORES = [0xff8a3d, 0x45a6ff, 0xf5c451, 0xe6e1f5, 0xc9b0ff, 0x4fd1c5, 0xff7fb5, 0xc4e86b, 0x9a9aa0];
var CORES_PARTE = [0x6fa8dc, 0x93c47d, 0xf6b26b, 0xe06666, 0x8e7cc3, 0x76a5af];
// o que toca em cada parte, por grupo (KICK BASS DRUMS HATS SYNTHS AMBIENCE VOCAL FX OTHER)
var REGRAS = {
	INTRO:  [1, 0, 1, 1, 0, 1, 0, 0, 0],
	GROOVE: [1, 0, 1, 1, 1, 0, 1, 0, 1],
	BUILD:  [0, 0, 1, 1, 1, 1, 1, 1, 0],
	DROP:   [1, 1, 1, 1, 1, 0, 1, 0, 1],
	BREAK:  [0, 0, 0, 0, 1, 1, 1, 0, 1],
	OUTRO:  [1, 0, 1, 1, 0, 1, 0, 0, 0]
};

var cfg = { loc: 1 };
var ready = false, busy = false;
var result = null;
var refCache = null;     // { chave, ref } — mesma referência não é relida
var criado = null;       // último BUILD: { faixas: [ids], ini, fim, cues: [tempos] }

// ---------------------------------------------------------------- tarefas com referência guardada (ver VASSOURA 0.2.3)
var T = {};
function depois(nome, fn, ms) {
	if (T[nome]) { try { T[nome].cancel(); } catch (e) { } }
	T[nome] = new Task(fn);
	T[nome].schedule(ms);
}
var seqPasso = 0, etapa = null, ultimoPasso = 0, retomadas = 0, onde = "";
function agora() { return new Date().getTime(); }
function proximo(fn, ms) {
	var meu = ++seqPasso;
	etapa = fn;
	ultimoPasso = agora();
	T.passo = new Task(function () {
		if (meu != seqPasso) return;
		ultimoPasso = agora();
		fn();
	});
	T.passo.schedule(ms);
}
function esperaArquivo() { etapa = null; seqPasso++; }
function vigiaLiga() {
	retomadas = 0;
	if (T.vigia) { try { T.vigia.cancel(); } catch (e) { } }
	T.vigia = new Task(function () {
		if (!busy) { T.vigia.cancel(); return; }
		if (etapa && agora() - ultimoPasso > 3000) {
			retomadas++;
			log("DESENROLA: corrente parada em [" + onde + "], retomando (" + retomadas + ")\n");
			proximo(etapa, 0);
		}
	});
	T.vigia.interval = 1000;
	T.vigia.repeat();
}
function vigiaDesliga() { etapa = null; seqPasso++; if (T.vigia) { try { T.vigia.cancel(); } catch (e) { } } }

function init() {
	ready = true;
	if (LIC_TESTE) { lic.ok = true; lic.email = "teste"; lic.msg = ""; licMudou(); }
	else licCarrega();
	status(licLiberada() ? "PRONTO · clique OPEN e depois ANALYZE" : "ATIVE A LICENÇA · clique OPEN");
	enviaCfg();
	if (!LIC_TESTE) depois("versao", checaVersao, 4000);
}
function travado() {
	if (licLiberada()) return false;
	status("ATIVE A LICENÇA · clique OPEN");
	outlet(2, "lic", licEstado());
	return true;
}
function licMudou() {
	outlet(2, "lic", licEstado());
	outlet(1, "lic", lic.ok ? 1 : 0);
	if (!ready) return;
	if (lic.ok && !busy && !result) status("PRONTO · clique ANALYZE");
	else if (!lic.ok) status("ATIVE A LICENÇA · clique OPEN");
}
function enviaCfg() { outlet(2, "valores", cfg.loc); }
function janela() { enviaCfg(); outlet(2, "lic", licEstado()); if (nova) outlet(2, "nova", nova); }

function loc(v) { cfg.loc = num(v) ? 1 : 0; if (result) { result.cfg = cfg; } }
function ui_loc(v) { loc(v); outlet(3, "loc", cfg.loc); enviaCfg(); }

function num(v) {
	if (v instanceof Array) v = v[0];
	var n = parseFloat(v);
	return isNaN(n) ? 0 : n;
}
function str(v) {
	if (v instanceof Array) return v.join(" ");
	return v == null ? "" : String(v);
}
function status(txt) {
	outlet(1, "status", txt);
	outlet(2, "status", txt);
}

// ---------------------------------------------------------------- grupo pelo nome (roleDoNome do MAESTRO, Dsp.h)
function grupoDoNome(nome) {
	var s = String(nome || "").toLowerCase();
	if (!s) return -1;
	function tem(k) { return s.indexOf(k) >= 0; }
	if (tem("kick") || tem("kik") || tem("bd ") || s == "bd" || tem("bumbo")) return 0;
	if (tem("bass") || tem("sub") || tem("baixo") || tem("808")) return 1;
	if (tem("vox") || tem("vocal") || tem("voice") || tem("voc") || tem("speech") || tem("voz")) return 6;
	if (tem("hat") || tem("hh") || tem("ride") || tem("cymb") || tem("crash") || tem("prato") || tem("shaker")) return 3;
	if (tem("drum") || tem("snare") || tem("snr") || tem("clap") || tem("perc") || tem("tom") || tem("rim") ||
		tem("top") || tem("conga") || tem("bongo") || tem("caixa") || tem("bateria")) return 2;
	if (tem("fx") || tem("riser") || tem("sweep") || tem("impact") || tem("uplift") || tem("downlift") || tem("whoosh") ||
		tem("noise") || tem("fill") || tem("reverse") || tem("zap") || tem("efeito")) return 7;
	if (tem("amb") || tem("atmo") || tem("pad") || tem("drone") || tem("texture") || tem("space") || tem("choir")) return 5;
	if (tem("synth") || tem("lead") || tem("acid") || tem("303") || tem("arp") || tem("pluck") || tem("key") || tem("seq") ||
		tem("melod") || tem("stab") || tem("piano") || tem("guitar") || tem("chord") || tem("string") || tem("squelch")) return 4;
	return -1;
}
function ehRef(nome) {
	var s = String(nome || "").replace(/^\s+|\s+$/g, "").toUpperCase();
	return /^(REF|REFS|REFERENCE|REFERÊNCIA|REFERENCIA)(\b|\s|$)/.test(s) || /\bREF\b/.test(s);
}
function onDaRegra(g) {
	var on = [];
	for (var p = 0; p < PARTES.length; p++) on.push(REGRAS[PARTES[p]][g < 0 ? 8 : g]);
	return on;
}

// ---------------------------------------------------------------- LiveAPI reaproveitado (ver VASSOURA)
var lapi = null, gotoFalhou = false;
function aponta(path) {
	if (lapi && !gotoFalhou) {
		try {
			lapi.goto(path);
			var m = /^id (\d+)$/.exec(path);
			if (!m || Number(lapi.id) == Number(m[1])) return lapi;
		} catch (e) { }
		gotoFalhou = true;
		log("DESENROLA: goto do LiveAPI não funcionou aqui, usando um LiveAPI por consulta\n");
	}
	var a = new LiveAPI(path);
	if (!lapi) lapi = a;
	return a;
}
// get("arrangement_clips") devolve "id 12 id 13 ..."
function idsDe(api, prop) {
	var r;
	try { r = api.get(prop); } catch (e) { return []; }
	if (!(r instanceof Array)) r = str(r).split(" ");
	var out = [];
	for (var i = 0; i < r.length; i++) { var n = Number(r[i]); if (String(r[i]) != "id" && !isNaN(n) && n > 0) out.push(n); }
	return out;
}
// lê uma propriedade-dicionário do LOM (warp_markers): texto JSON, lista ou objeto
function leDict(api, prop) {
	var raw;
	try { raw = api.get(prop); } catch (e) { return null; }
	if (raw && typeof raw == "object" && !(raw instanceof Array)) return raw[prop] != null ? raw[prop] : raw;
	var s = str(raw), k = s.indexOf("{");
	if (k < 0) return null;
	try { var j = JSON.parse(s.substring(k)); return j[prop] != null ? j[prop] : j; } catch (e) { return null; }
}

// ---------------------------------------------------------------- log em arquivo (DESENROLA-log.txt, ao lado do device)
var logPath = null, logOk = false, logMotivo = "";
function log(s, soArq) {
	if (!soArq || !logOk) post(s);
	if (!logOk || !s) return;
	try {
		var f = new File(logPath, "readwrite", "TEXT");
		if (!f.isopen) { logOk = false; return; }
		f.position = f.eof;
		f.writestring(hora() + " " + s.replace(/\n$/, "") + "\r\n");
		f.close();
	} catch (e) { logOk = false; }
}
function pastaDoDevice() {
	var p = PATCHER;
	for (var n = 0; p && n < 5; n++) {
		var fp = "";
		try { fp = String(p.filepath || ""); } catch (e) { }
		if (fp) return fp.replace(/[^\/\\]*$/, "");
		try { p = p.parentpatcher; } catch (e) { p = null; }
	}
	return "";
}
function logNovo() {
	logOk = false;
	try {
		if (logPath == null) {
			var pasta = pastaDoDevice();
			logPath = pasta ? pasta + "DESENROLA-log.txt" : "";
			if (!pasta) logMotivo = "não achei a pasta do device";
		}
		if (logPath) {
			var f = new File(logPath, "write", "TEXT");
			if (f.isopen) {
				try { f.eof = 0; } catch (e) { }
				f.writestring(hora() + " log novo\r\n");
				f.close();
				logOk = true;
			} else logMotivo = "o Max não abriu " + logPath + " para gravar";
		}
	} catch (e) { logMotivo = String(e); }
	post(logOk ? "DESENROLA: log em " + logPath + "\n" : "DESENROLA: sem log em arquivo (" + logMotivo + "), detalhes vão para este console\n");
}
function hora() {
	var d = new Date();
	function p(n) { return (n < 10 ? "0" : "") + n; }
	return p(d.getHours()) + ":" + p(d.getMinutes()) + ":" + p(d.getSeconds());
}
function versaoMax() { try { return String(max.version); } catch (e) { return "?"; } }

// ---------------------------------------------------------------- LEITURA DA REFERÊNCIA (estrutura)
// Passo 1 (em pedaços, para não travar o Live): o áudio vira 3 curvas de 10 ms: grave (até ~110 Hz), agudo (acima de
// ~5 kHz) e cheio. Passo 2: andamento e batida (marcas de warp do Live, se houver; senão pela pulsação do grave).
// Passo 3: cada compasso ganha kick (pulso do grave na batida), energia e brilho; os blocos de 8 compassos viram
// INTRO / GROOVE / BUILD / DROP / BREAK / OUTRO pela presença de kick e pela energia (o que o MAPA acertava).
var PARTES = ["INTRO", "GROOVE", "BUILD", "DROP", "BREAK", "OUTRO"];

function refNovo(sr, nFrames) {
	var hop = Math.max(1, Math.round(sr * 0.01));
	var n = Math.ceil(nFrames / hop);
	return {
		sr: sr, hop: hop, n: n, pos: 0,
		aL: 1 - Math.exp(-2 * Math.PI * 110 / sr), aH: 1 - Math.exp(-2 * Math.PI * 5000 / sr),
		l1: 0, l2: 0, h1: 0, sL: 0, sH: 0, sF: 0, k: 0, f: 0,
		low: new Float32Array(n), high: new Float32Array(n), full: new Float32Array(n)
	};
}
// soma um pedaço de áudio mono (array de amostras) nas curvas
function refBloco(R, a) {
	var aL = R.aL, aH = R.aH, l1 = R.l1, l2 = R.l2, h1 = R.h1, sL = R.sL, sH = R.sH, sF = R.sF, k = R.k, f = R.f, hop = R.hop;
	for (var i = 0; i < a.length; i++) {
		var x = a[i];
		l1 += aL * (x - l1); l2 += aL * (l1 - l2);
		h1 += aH * (x - h1);
		var hp = x - h1;
		sL += l2 * l2; sH += hp * hp; sF += x * x;
		if (++k >= hop) {
			if (f < R.n) { R.low[f] = Math.sqrt(sL / hop); R.high[f] = Math.sqrt(sH / hop); R.full[f] = Math.sqrt(sF / hop); }
			f++; k = 0; sL = 0; sH = 0; sF = 0;
		}
	}
	R.l1 = l1; R.l2 = l2; R.h1 = h1; R.sL = sL; R.sH = sH; R.sF = sF; R.k = k; R.f = f;
}

function db(v) { return v > 1e-7 ? 20 * Math.log(v) / Math.LN10 : -140; }

// pulsação do grave: subida da energia do grave (onde o kick bate)
function refNovidade(R) {
	var n = R.f < R.n ? R.f : R.n, nov = new Float32Array(n), prev = 0;
	for (var i = 0; i < n; i++) {
		var v = R.low[i];
		var d = v - prev;
		nov[i] = d > 0 ? d : 0;
		prev = v;
	}
	return nov;
}
function amostra(arr, t) {  // valor interpolado na posição t (em quadros de 10 ms)
	var i = Math.floor(t), fr = t - i;
	if (i < 0 || i + 1 >= arr.length) return 0;
	return arr[i] * (1 - fr) + arr[i + 1] * fr;
}
// maior valor perto de t (±2 quadros): a batida do kick não cai exato no quadro
function picoPerto(arr, t) {
	var i = Math.round(t), m = 0;
	for (var d = -2; d <= 2; d++) { var j = i + d; if (j >= 0 && j < arr.length && arr[j] > m) m = arr[j]; }
	return m;
}
// andamento pela pulsação: nota de um andamento + fase = média do pulso do grave nas batidas
function notaAnd(nov, fps, bpm, fase) {
	var per = fps * 60 / bpm, s = 0, c = 0;
	for (var t = fase; t < nov.length - 2; t += per) { s += picoPerto(nov, t); c++; }
	return c ? s / c : 0;
}
// lista de testes: 100..180 BPM de 0,5 em 0,5 (todas as fases); depois o fino em volta do melhor
function andNovo(fps) { return { fps: fps, fase: 1, b: 100, melhor: { bpm: 0, fase: 0, nota: -1 }, fim: 0 }; }
// avança até 'quantos' andamentos; devolve true quando terminou
function andPasso(A, nov, quantos) {
	for (var q = 0; q < quantos && !A.fim; q++) {
		var b0 = A.fase == 1 ? 100 : A.c - 0.6, b1 = A.fase == 1 ? 180 : A.c + 0.6, passo = A.fase == 1 ? 0.5 : 0.02, fp = A.fase == 1 ? 2 : 0.5;
		var per = A.fps * 60 / A.b;
		for (var fa = 0; fa < per; fa += fp) {
			var v = notaAnd(nov, A.fps, A.b, fa);
			if (v > A.melhor.nota) A.melhor = { bpm: A.b, fase: fa, nota: v };
		}
		A.b += passo;
		if (A.b > b1 + 1e-9) {
			if (A.fase == 1) { A.fase = 2; A.c = A.melhor.bpm; A.b = A.c - 0.6; }
			else A.fim = 1;
		}
	}
	if (A.fim) {
		var bpm = A.melhor.bpm;
		if (Math.abs(bpm - Math.round(bpm)) < 0.1) bpm = Math.round(bpm);
		A.bpm = bpm; A.fase0 = A.melhor.fase;
	}
	return A.fim;
}
// qual das 4 batidas é o 1 do compasso: onde a energia mais muda (as partes trocam no 1)
function refUm(R, bpm, fase) {
	var fps = R.sr / R.hop, per = fps * 60 / bpm, n = R.f < R.n ? R.f : R.n, E = [];
	for (var t = fase; t + per <= n; t += per) {
		var s = 0, c = 0;
		for (var q = Math.floor(t); q < Math.floor(t + per); q++) { s += R.full[q] * R.full[q]; c++; }
		E.push(db(Math.sqrt(s / Math.max(1, c))));
	}
	var melhor = 0, mv = -1;
	for (var o = 0; o < 4; o++) {
		var v = 0;
		for (var k = o + 4; k < E.length; k += 4) v += Math.abs(E[k] - E[k - 1]);
		if (v > mv) { mv = v; melhor = o; }
	}
	return fase + melhor * per;
}

// por compasso: kick (pulso do grave nas 4 batidas, comparado com o meio dos tempos), grave, agudo, energia
function refCompassos(R, nov, bpm, fase0) {
	var fps = R.sr / R.hop, per = fps * 60 / bpm, barra = per * 4;
	var n = R.f < R.n ? R.f : R.n;
	var bars = [];
	for (var t = fase0; t + barra <= n; t += barra) {
		var kOn = 0, kOff = 0, lo = 0, hi = 0, fu = 0, c = 0;
		for (var bt = 0; bt < 4; bt++) {
			kOn += picoPerto(nov, t + bt * per);
			kOff += picoPerto(nov, t + bt * per + per / 2);
		}
		for (var q = Math.floor(t); q < Math.floor(t + barra) && q < n; q++) { lo += R.low[q] * R.low[q]; hi += R.high[q] * R.high[q]; fu += R.full[q] * R.full[q]; c++; }
		bars.push({ t: t, kOn: kOn / 4, kOff: kOff / 4, lo: db(Math.sqrt(lo / c)), hi: db(Math.sqrt(hi / c)), fu: db(Math.sqrt(fu / c)) });
	}
	return bars;
}
function quantil(lista, q) {
	var a = lista.slice().sort(function (x, y) { return x - y; });
	if (!a.length) return 0;
	return a[Math.min(a.length - 1, Math.max(0, Math.floor(q * (a.length - 1))))];
}


// rótulo de cada compasso e as partes (trechos seguidos do mesmo tipo)
//  kick  = pulso do grave na batida (>= 25% do kick forte da música e mais forte que o meio dos tempos)
//  baixo = grave ENTRE as batidas (baixo rolante/offbeat do psy, house, techno)
//  DROP = kick + baixo · GROOVE = kick sem baixo (ou fraco) · BREAK = sem kick
//  antes do 1º DROP = INTRO · depois do último DROP = OUTRO · o fim de um BREAK que desemboca no DROP = BUILD
function refPartes(bars) {
	var nb = bars.length, i;
	var kOnL = [], fuL = [];
	for (i = 0; i < nb; i++) { kOnL.push(bars[i].kOn); fuL.push(bars[i].fu); }
	var kRef = quantil(kOnL, 0.9), fuTop = quantil(fuL, 0.9);
	var cls = [];
	for (i = 0; i < nb; i++) {
		var b = bars[i];
		b.kick = kRef > 0 && b.kOn > 0.25 * kRef && b.kOn > 0.6 * b.kOff ? 1 : 0;
		b.baixo = kRef > 0 && b.kOff > 0.3 * kRef ? 1 : 0;
		b.e = b.fu - fuTop;
		cls.push(!b.kick ? "B" : (b.baixo && b.e > -6 ? "D" : "K"));
	}
	// compasso solto (virada, fill) segue os vizinhos
	for (i = 1; i < nb - 1; i++) if (cls[i - 1] == cls[i + 1] && cls[i] != cls[i - 1]) cls[i] = cls[i - 1];
	// trechos
	var tr = [];
	for (i = 0; i < nb; i++) {
		var u = tr.length ? tr[tr.length - 1] : null;
		if (u && u.c == cls[i]) u.b = i + 1; else tr.push({ c: cls[i], a: i, b: i + 1 });
	}
	// trecho de 1 compasso (no começo/fim, ou entre dois diferentes) entra no vizinho maior
	for (var it = 0; it < 3; it++) {
		for (var k = 0; k < tr.length; k++) {
			if (tr.length < 2 || tr[k].b - tr[k].a > 1) continue;
			var viz = k == 0 ? 1 : k == tr.length - 1 ? k - 1 : ((tr[k - 1].b - tr[k - 1].a) >= (tr[k + 1].b - tr[k + 1].a) ? k - 1 : k + 1);
			if (viz < k) tr[viz].b = tr[k].b; else tr[viz].a = tr[k].a;
			tr.splice(k, 1); k--;
		}
		for (var m = 1; m < tr.length; m++) if (tr[m].c == tr[m - 1].c) { tr[m - 1].b = tr[m].b; tr.splice(m, 1); m--; }
	}
	var nomes = { D: "DROP", K: "GROOVE", B: "BREAK" };
	for (var q = 0; q < tr.length; q++) tr[q].p = nomes[tr[q].c];
	var prim = -1, ult = -1;
	for (var w = 0; w < tr.length; w++) if (tr[w].p == "DROP") { if (prim < 0) prim = w; ult = w; }
	for (w = 0; w < tr.length; w++) {
		if (prim >= 0 && w < prim) tr[w].p = "INTRO";
		if (ult >= 0 && w > ult) tr[w].p = "OUTRO";
	}
	if (prim < 0 && tr.length) { tr[0].p = "INTRO"; if (tr.length > 1) tr[tr.length - 1].p = "OUTRO"; }
	// BUILD: o trecho logo antes de cada DROP (menos o 1º, que vem da INTRO): BREAK de 12+ compassos perde os
	// últimos 8 para o BUILD; BREAK curto (até 8) depois de outro BREAK/GROOVE vira BUILD inteiro;
	// GROOVE (kick sem baixo) entre um BREAK e o DROP vira BUILD
	var out = [];
	for (w = 0; w < tr.length; w++) {
		var t = tr[w], prox = tr[w + 1];
		if (prox && prox.p == "DROP" && t.p == "BREAK") {
			var tam = t.b - t.a;
			if (tam >= 12) { out.push({ p: "BREAK", a: t.a, b: t.b - 8 }); out.push({ p: "BUILD", a: t.b - 8, b: t.b }); continue; }
		}
		if (prox && prox.p == "DROP" && t.p == "GROOVE" && w > 0 && tr[w - 1].p == "BREAK") { out.push({ p: "BUILD", a: t.a, b: t.b }); continue; }
		out.push({ p: t.p, a: t.a, b: t.b });
	}
	for (w = 1; w < out.length; w++) if (out[w].p == out[w - 1].p) { out[w - 1].b = out[w].b; out.splice(w, 1); w--; }
	return { partes: out, cls: cls.join("") };
}

// tamanho da parte no rascunho: múltiplo de 8 se estiver a 1 compasso dele, senão de 4 (mínimo 4)
function arredonda(n) {
	var r8 = Math.round(n / 8) * 8;
	if (r8 >= 8 && Math.abs(n - r8) <= 1) return r8;
	return Math.max(4, Math.round(n / 4) * 4);
}

// ---------------------------------------------------------------- ANALYZE
var an = null;   // { set, nt, i, refT, loop, tracks, ... }
function analyze() {
	if (!ready) { status("aguarde o Live terminar de carregar"); return; }
	if (busy || travado()) return;
	if (result) lerMarcas();
	busy = true;
	logNovo();
	var set = new LiveAPI("live_set");
	var ls = num(set.get("loop_start")), ll = num(set.get("loop_length"));
	an = {
		nt: set.getcount("tracks"), i: 0, sig: num(set.get("signature_numerator")) || 4, den: num(set.get("signature_denominator")) || 4,
		tempo: num(set.get("tempo")), loop: { s: ls, e: ls + ll }, tracks: [], refs: [], fora: 0, selId: -1, inicio: 0
	};
	try { an.selId = Number(new LiveAPI("live_set view selected_track").id); } catch (e) { }
	log("DESENROLA " + VERSAO + (BUILD ? " (" + BUILD + ")" : "") + ": ANALYZE · " + an.nt + " faixas · loop " + ls + "-" + (ls + ll) + " beats · Live/Max " + versaoMax() + "\n");
	if (ll < an.sig) { busy = false; status("marque o loop no Arrangement (loop brace em volta do seu loop) e clique ANALYZE"); return; }
	vigiaLiga();
	proximo(umaFaixa, 0);
}
function umaFaixa() {
	try {
		if (an.i < an.nt) { var i = an.i++; onde = "ANALYZE faixa " + an.i + "/" + an.nt; leFaixa(i); proximo(umaFaixa, 2); return; }
		fimFaixas();
	} catch (e) { falhou("erro lendo a faixa " + an.i + ": " + e); }
}
function falhou(txt) {
	busy = false;
	vigiaDesliga();
	log("DESENROLA: " + txt + "\n");
	status(txt);
}
function leFaixa(i) {
	var t = aponta("live_set tracks " + i);
	var tr = { id: Number(t.id), index: i, name: str(t.get("name")), color: num(t.get("color")), fold: num(t.get("is_foldable")),
		midi: num(t.get("has_midi_input")), clips: [] };
	status("ANALYZE · faixa " + (i + 1) + " · " + tr.name);
	if (tr.fold) return;
	var ids = idsDe(t, "arrangement_clips"), EPS = 0.001, fim = 0, dentro = 0, maior = null;
	for (var j = 0; j < ids.length; j++) {
		var c = new LiveAPI("id " + ids[j]);
		var s = num(c.get("start_time")), e = num(c.get("end_time"));
		if (e > fim) fim = e;
		if (s >= an.loop.s - EPS && e <= an.loop.e + EPS) tr.clips.push({ id: ids[j], s: s, e: e });
		else if (s < an.loop.e - EPS && e > an.loop.s + EPS) an.fora++;   // cruza a borda do loop: fica de fora
		if (!tr.midi && (!maior || e - s > maior.e - maior.s)) maior = { id: ids[j], s: s, e: e };
	}
	tr.fim = fim;
	if (!tr.midi && maior && (ehRef(tr.name) || tr.id == an.selId)) an.refs.push({ tr: tr, clip: maior, porNome: ehRef(tr.name) ? 1 : 0 });
	if (!ehRef(tr.name)) an.tracks.push(tr);
	log("  " + i + " [" + tr.name + "] " + ids.length + " clips, " + tr.clips.length + " no loop\n", 1);
}
function fimFaixas() {
	// faixa da referência: a que se chama REF; senão a selecionada no Live (se o clip dela estiver fora do loop)
	var ref = null;
	for (var r = 0; r < an.refs.length; r++) if (an.refs[r].porNome) { ref = an.refs[r]; break; }
	if (!ref) for (var r2 = 0; r2 < an.refs.length; r2++) { var cr = an.refs[r2].clip; if (cr.s >= an.loop.e || cr.e <= an.loop.s) { ref = an.refs[r2]; break; } }
	var faixas = [], fimSet = an.loop.e;
	for (var k = 0; k < an.tracks.length; k++) {
		var tr = an.tracks[k];
		if (ref && tr.id == ref.tr.id) continue;
		if (tr.fim > fimSet) fimSet = tr.fim;
		if (!tr.clips.length) continue;
		var g = grupoDoNome(tr.name);
		tr.g = g < 0 ? 8 : g; tr.auto = g >= 0 ? 1 : 0;
		tr.on = onDaRegra(tr.g);
		faixas.push(tr);
	}
	if (!faixas.length) { falhou("nenhuma faixa com clip dentro do loop · marque o loop brace em volta do seu loop"); return; }
	if (!ref) { falhou("não achei a referência · crie uma faixa de áudio chamada REF com a música (ou selecione a faixa dela) e clique ANALYZE"); return; }
	// início do rascunho: depois do loop e de tudo o que já existe nas faixas do loop, no compasso, + 4 compassos
	var bar = an.sig * 4 / an.den;
	an.inicio = Math.ceil(fimSet / bar - 1e-6) * bar + 4 * bar;
	an.faixas = faixas;
	an.ref = ref;
	an.bar = bar;
	// marcas da janela que valem de novo (mesma faixa): grupo e liga/desliga
	if (result && result.tracks) {
		var velho = {};
		for (var v = 0; v < result.tracks.length; v++) velho[result.tracks[v].id] = result.tracks[v];
		for (var f = 0; f < faixas.length; f++) { var o = velho[faixas[f].id]; if (o) { faixas[f].g = o.g; faixas[f].auto = o.auto; faixas[f].on = o.on; } }
	}
	log("DESENROLA: " + faixas.length + " faixas no loop · referência [" + ref.tr.name + "] · rascunho começa no beat " + an.inicio + (an.fora ? " · " + an.fora + " clip(s) cruzam a borda do loop (ficam de fora)" : "") + "\n");
	leRef();
}

// referência: o arquivo do maior clip da faixa REF
var leit = null;   // leitura em andamento
function leRef() {
	var c = new LiveAPI("id " + an.ref.clip.id);
	var info = { file: str(c.get("file_path")).replace(/\\/g, "/"), sr: num(c.get("sample_rate")), warping: num(c.get("warping")), name: str(c.get("name")) };
	var wm = null;
	if (info.warping) { wm = leDict(c, "warp_markers"); if (wm && !(wm instanceof Array)) wm = [wm]; }
	info.wm = wm;
	if (!info.file) { falhou("o clip da referência não tem arquivo de áudio"); return; }
	var chave = info.file + "|" + (wm ? JSON.stringify(wm) : "");
	if (refCache && refCache.chave == chave) { log("DESENROLA: referência já lida, usando a mesma\n"); fimRef(refCache.ref); return; }
	leit = { info: info, chave: chave };
	status("ANALYZE · lendo a referência " + (info.name || info.file));
	log("  referência: " + info.file + (info.warping ? " (warp, " + (wm ? wm.length : 0) + " marcas)" : " (sem warp)") + "\n");
	onde = "carregando a referência";
	esperaArquivo();
	depois("cao", function () { if (leit && !leit.R) falhou("o Max não carregou o arquivo da referência em 60 s"); }, 60000);
	outlet(0, "replace", info.file);
}
// bang do buffer~
function loaded() {
	if (!busy || !leit || leit.R) return;
	if (T.cao) { try { T.cao.cancel(); } catch (e) { } }
	var buf = new Buffer(BUF);
	leit.buf = buf;
	leit.frames = buf.framecount();
	leit.nch = Math.max(1, Math.min(2, buf.channelcount()));
	var sr = leit.info.sr > 0 ? leit.info.sr : 44100;
	leit.R = refNovo(sr, leit.frames);
	leit.pos = 0;
	log("  " + leit.frames + " amostras · " + leit.nch + " canais · " + (leit.frames / sr / 60).toFixed(1) + " min\n", 1);
	proximo(passoLeitura, 0);
}
function passoLeitura() {
	try {
		var L = leit, R = L.R, CH = R.sr * 4;
		if (L.pos < L.frames) {
			var n = Math.min(CH, L.frames - L.pos);
			var a = L.buf.peek(1, L.pos, n);
			if (typeof a == "number") a = [a];
			if (L.nch > 1) {
				var b = L.buf.peek(2, L.pos, n);
				if (typeof b == "number") b = [b];
				for (var i = 0; i < a.length; i++) a[i] = 0.5 * (a[i] + b[i]);
			}
			refBloco(R, a);
			L.pos += n;
			onde = "lendo a referência";
			status("ANALYZE · lendo a referência " + Math.round(100 * L.pos / L.frames) + "%");
			proximo(passoLeitura, 1);
			return;
		}
		outlet(0, "sizeinsamps", 1);   // devolve a memória
		L.nov = refNovidade(R);
		var fps = R.sr / R.hop;
		var wmA = andDoWarp(L.info.wm, R.sr);
		if (wmA) { L.and = wmA; log("  andamento pelas marcas de warp: " + wmA.bpm.toFixed(2) + " BPM\n"); proximo(passoPartes, 0); return; }
		L.A = andNovo(fps);
		proximo(passoAndamento, 0);
	} catch (e) { falhou("erro lendo a referência: " + e); }
}
// warp do Live: sample_time (s) × beat_time. Andamento pela reta das marcas; o 1 do compasso = beat_time múltiplo de 4
function andDoWarp(wm, sr) {
	if (!wm || wm.length < 2) return null;
	var pts = [];
	for (var i = 0; i < wm.length; i++) if (wm[i] && wm[i].sample_time != null && wm[i].beat_time != null) pts.push([Number(wm[i].sample_time), Number(wm[i].beat_time)]);
	if (pts.length < 2) return null;
	var a = pts[0], z = pts[pts.length - 1];
	if (z[0] - a[0] < 5) return null;
	var bpm = 60 * (z[1] - a[1]) / (z[0] - a[0]);
	if (bpm < 60 || bpm > 200) return null;
	if (Math.abs(bpm - Math.round(bpm)) < 0.05) bpm = Math.round(bpm);
	var spb = 60 / bpm;
	var t0 = a[0] - a[1] * spb;                 // segundo do beat 0 (1.1.1 do clip)
	var barS = 4 * spb;
	t0 = t0 - Math.floor(t0 / barS) * barS;     // primeiro 1 do compasso dentro do arquivo
	return { bpm: bpm, fase: t0 * 100, warp: 1 };  // fase em quadros de 10 ms
}
function passoAndamento() {
	try {
		onde = "andamento";
		status("ANALYZE · achando o andamento da referência");
		if (!andPasso(leit.A, leit.nov, 6)) { proximo(passoAndamento, 1); return; }
		leit.and = { bpm: leit.A.bpm, fase: refUm(leit.R, leit.A.bpm, leit.A.fase0), warp: 0 };
		log("  andamento pelo áudio: " + leit.and.bpm + " BPM\n");
		proximo(passoPartes, 0);
	} catch (e) { falhou("erro no andamento: " + e); }
}
function passoPartes() {
	try {
		var L = leit, R = L.R;
		var fps = R.sr / R.hop;
		var fase = L.and.fase * (fps / 100);
		var bars = refCompassos(R, L.nov, L.and.bpm, fase);
		if (bars.length < 8) { falhou("a referência é curta demais (" + bars.length + " compassos)"); return; }
		var rp = refPartes(bars);
		var ref = { nome: an.ref.tr.name, clip: L.info.name, bpm: L.and.bpm, warp: L.and.warp, compassos: bars.length, cls: rp.cls,
			partes: rp.partes };
		refCache = { chave: L.chave, ref: ref };
		leit = null;
		fimRef(ref);
	} catch (e) { falhou("erro lendo as partes: " + e); }
}
function fimRef(ref) {
	busy = false;
	vigiaDesliga();
	var partes = [];
	for (var i = 0; i < ref.partes.length; i++) { var p = ref.partes[i]; partes.push({ p: p.p, n: arredonda(p.b - p.a), a: p.a, b: p.b }); }
	result = {
		versao: VERSAO, sig: an.sig, bar: an.bar, tempo: an.tempo, loop: { s: an.loop.s, e: an.loop.e, bars: (an.loop.e - an.loop.s) / an.bar },
		inicio: an.inicio, ref: ref, partes: partes, tracks: an.faixas, partesNomes: PARTES, grupos: GRUPOS, cores: CORES, coresParte: CORES_PARTE,
		regras: REGRAS, cfg: cfg, fora: an.fora, feito: criado ? 1 : 0
	};
	log("DESENROLA: referência " + ref.bpm + " BPM" + (ref.warp ? " (warp)" : "") + ", " + ref.compassos + " compassos · " + descrevePartes(partes) + "\n");
	publica();
	status(partes.length + " partes · " + totalBars(partes) + " compassos · " + an.faixas.length + " faixas · confira e clique BUILD");
}
function descrevePartes(partes) {
	var s = [];
	for (var i = 0; i < partes.length; i++) s.push(partes[i].p + " " + partes[i].n);
	return s.join(" · ");
}
function totalBars(partes) { var n = 0; for (var i = 0; i < partes.length; i++) n += partes[i].n; return n; }

function publica() {
	if (!result) return;
	result.cfg = cfg;
	result.feito = criado ? 1 : 0;
	var d = new Dict(DICT);
	d.parse(JSON.stringify(result));
	outlet(2, "bang");
}
function arr(x) { if (x == null) return []; return (x instanceof Array) ? x : [x]; }
// o que a janela mudou: partes (tipo/tamanho), grupo e liga/desliga de cada faixa
function lerMarcas() {
	var d = new Dict(DICT), j = null;
	try { j = JSON.parse(d.stringify()); } catch (e) { j = null; }
	if (!j || !result) return;
	if (j.partes) {
		var ps = arr(j.partes), novas = [];
		for (var i = 0; i < ps.length; i++) {
			var tipo = String(ps[i].p), n = Math.max(1, Math.round(Number(ps[i].n) || 0));
			if (REGRAS[tipo]) novas.push({ p: tipo, n: n, a: ps[i].a, b: ps[i].b });
		}
		if (novas.length) result.partes = novas;
	}
	if (j.tracks) {
		var tr = arr(j.tracks), porId = {};
		for (var k = 0; k < tr.length; k++) porId[tr[k].id] = tr[k];
		for (var q = 0; q < result.tracks.length; q++) {
			var o = porId[result.tracks[q].id];
			if (!o) continue;
			result.tracks[q].g = o.g == null ? result.tracks[q].g : Number(o.g);
			result.tracks[q].auto = o.auto ? 1 : 0;
			var on = arr(o.on);
			if (on.length == PARTES.length) { var oo = []; for (var z = 0; z < on.length; z++) oo.push(Number(on[z]) ? 1 : 0); result.tracks[q].on = oo; }
		}
	}
}
function marcas() {
	if (!result || busy) return;
	lerMarcas();
	status(result.partes.length + " partes · " + totalBars(result.partes) + " compassos · BUILD monta o rascunho");
}

// ---------------------------------------------------------------- BUILD
var bd = null;
function build() {
	if (busy || travado()) return;
	if (!result) { status("clique ANALYZE antes do BUILD"); return; }
	if (criado) { status("já existe um rascunho · clique UNDO antes de montar outro"); return; }
	lerMarcas();
	var R = result.loop.e - result.loop.s, bar = result.bar;
	var ops = [], pos = result.inicio, cues = [];
	for (var i = 0; i < result.partes.length; i++) {
		var pt = result.partes[i], L = pt.n * bar, ip = PARTES.indexOf(pt.p), prox = result.partes[i + 1];
		var ipProx = prox ? PARTES.indexOf(prox.p) : -1;
		cues.push({ t: pos, nome: pt.p });
		for (var f = 0; f < result.tracks.length; f++) {
			var tr = result.tracks[f];
			if (!tr.on[ip]) continue;
			for (var k = 0; pos + k * R < pos + L - 0.001; k++) {
				for (var c = 0; c < tr.clips.length; c++) {
					var cl = tr.clips[c], dest = pos + k * R + (cl.s - result.loop.s);
					if (dest >= pos + L - 0.001) continue;
					ops.push({ tid: tr.id, nome: tr.name, cid: cl.id, dest: dest, len: cl.e - cl.s, fimParte: pos + L, segue: ipProx >= 0 && tr.on[ipProx] ? 1 : 0 });
				}
			}
		}
		pos += L;
	}
	if (!ops.length) { status("nenhuma faixa ligada em nenhuma parte"); return; }
	busy = true;
	bd = { ops: ops, i: 0, feitos: 0, falhas: 0, aparados: 0, sobra: 0, apagados: 0, cues: cfg.loc ? cues : [], ic: 0, cuesFeitos: [], inicio: agora(), ini: result.inicio, fim: pos, faixas: {} };
	for (var t = 0; t < result.tracks.length; t++) bd.faixas[result.tracks[t].id] = 1;
	log("DESENROLA: BUILD · " + result.partes.length + " partes · " + ops.length + " cópias · beats " + result.inicio + "-" + pos + " · " + descrevePartes(result.partes) + "\n");
	outlet(2, "varre", ops.length);
	vigiaLiga();
	proximo(passoBuild, 0);
}
function passoBuild() {
	try {
		for (var q = 0; q < POR_PASSO && bd.i < bd.ops.length; q++) umaCopia(bd.ops[bd.i++]);
		if (bd.i < bd.ops.length) {
			onde = "BUILD " + bd.i + "/" + bd.ops.length;
			status("BUILD " + bd.i + "/" + bd.ops.length);
			outlet(2, "progresso", bd.i, bd.ops.length);
			proximo(passoBuild, PAUSA);
			return;
		}
		if (bd.ic < bd.cues.length) { umLocator(bd.cues[bd.ic++]); onde = "locators"; proximo(passoBuild, PAUSA); return; }
		fimBuild();
	} catch (e) { bd.falhas++; log("DESENROLA erro no BUILD: " + e + "\n"); proximo(passoBuild, PAUSA); }
}
// clip da faixa que começa em s (o mais novo se houver mais de um)
function achaClip(tid, s) {
	var t = new LiveAPI("id " + tid), ids = idsDe(t, "arrangement_clips"), achou = null;
	for (var j = 0; j < ids.length; j++) {
		var c = aponta("id " + ids[j]);
		if (Math.abs(num(c.get("start_time")) - s) < 0.002) achou = { id: ids[j], s: s, e: num(c.get("end_time")) };
	}
	return achou;
}
function umaCopia(op) {
	var t = new LiveAPI("id " + op.tid);
	if (Number(t.id) != op.tid) { bd.falhas++; log("  [" + op.nome + "]: a faixa não existe mais\n"); return; }
	t.call("duplicate_clip_to_arrangement", "id", op.cid, op.dest);
	if (op.dest + op.len <= op.fimParte + 0.002) { bd.feitos++; return; }
	// passou do fim da parte: apara a cópia; se não der e a faixa segue na próxima parte, a próxima cobre por cima
	var nova = achaClip(op.tid, op.dest);
	if (!nova) { bd.falhas++; log("  [" + op.nome + "]: o Live não criou a cópia no beat " + op.dest + "\n"); return; }
	bd.feitos++;
	if (apara(nova.id, op.fimParte - op.dest)) { bd.aparados++; return; }
	if (op.segue) { bd.sobra++; log("  [" + op.nome + "]: cópia no beat " + op.dest + " passou da parte; a próxima parte cobre por cima\n", 1); return; }
	t.call("delete_clip", "id", nova.id);
	bd.apagados++;
	log("  [" + op.nome + "]: clip de " + op.len + " beats não coube no fim da parte (beat " + op.dest + "), tirei\n");
}
// encurta o clip para 'len' beats no arranjo (fim dos markers) e confere; volta como estava se não deu
var fmtApara = -1;
function apara(id, len) {
	var c = new LiveAPI("id " + id);
	var s = num(c.get("start_time")), e0 = num(c.get("end_time"));
	var looping = num(c.get("looping")), warping = 1, audio = 0;
	try { audio = num(c.get("is_audio_clip")); } catch (e) { }
	if (audio) { try { warping = num(c.get("warping")); } catch (e) { } }
	var k = audio && !warping ? 60 / result.tempo : 1;        // sem warp os markers são segundos
	var sm = num(c.get("start_marker")), em = num(c.get("end_marker")), lsC = num(c.get("loop_start")), leC = num(c.get("loop_end"));
	function confere() { return Math.abs(num(c.get("end_time")) - (s + len)) < 0.01; }
	var jeitos = [
		function () { c.set("end_marker", sm + len * k); },
		function () { c.set("loop_end", lsC + len * k); c.set("end_marker", sm + len * k); },
		function () { c.set("looping", 0); c.set("end_marker", sm + len * k); }
	];
	var ordem = fmtApara >= 0 ? [fmtApara, 0, 1, 2] : [0, 1, 2];
	for (var q = 0; q < ordem.length; q++) {
		try { jeitos[ordem[q]](); } catch (e) { }
		if (confere()) { if (fmtApara != ordem[q]) { fmtApara = ordem[q]; log("DESENROLA: aparar clip funcionou pelo jeito " + fmtApara + "\n"); } return true; }
	}
	// não deu: volta como estava
	try { c.set("looping", looping); } catch (e) { }
	try { c.set("loop_end", leC); c.set("end_marker", em); } catch (e) { }
	if (Math.abs(num(c.get("end_time")) - e0) > 0.01) log("  clip " + id + ": não voltou ao tamanho de antes\n");
	return false;
}
// locator com o nome da parte (o LOM cria o locator na posição de play)
function umLocator(cue) {
	var set = new LiveAPI("live_set");
	var antes = num(set.get("current_song_time"));
	var ids = idsDe(set, "cue_points");
	for (var i = 0; i < ids.length; i++) if (Math.abs(num(aponta("id " + ids[i]).get("time")) - cue.t) < 0.01) { log("  locator: já existe um no beat " + cue.t + "\n", 1); return; }
	try {
		set.set("current_song_time", cue.t);
		set.call("set_or_delete_cue");
		var ids2 = idsDe(set, "cue_points");
		for (var j = 0; j < ids2.length; j++) {
			var cp = new LiveAPI("id " + ids2[j]);
			if (Math.abs(num(cp.get("time")) - cue.t) < 0.01) { cp.set("name", cue.nome); bd.cuesFeitos.push(cue.t); break; }
		}
	} catch (e) { log("  locator " + cue.nome + ": " + e + "\n"); }
	try { set.set("current_song_time", antes); } catch (e) { }
}
function fimBuild() {
	busy = false;
	vigiaDesliga();
	criado = { faixas: bd.faixas, ini: bd.ini, fim: bd.fim, cues: bd.cuesFeitos };
	var bar = result.bar;
	var txt = "BUILD FEITO · " + result.partes.length + " partes, " + Math.round((bd.fim - bd.ini) / bar) + " compassos a partir do compasso " + (Math.round(bd.ini / bar) + 1) +
		" · " + bd.feitos + " clips" + (bd.aparados ? " (" + bd.aparados + " aparados)" : "") + (bd.cuesFeitos.length ? " · " + bd.cuesFeitos.length + " locators" : "") +
		(bd.apagados ? " · " + bd.apagados + " não couberam" : "") + (bd.falhas ? " · " + bd.falhas + " falhas (ver console)" : "") + " · UNDO apaga o rascunho";
	log("DESENROLA: " + txt + " · " + ((agora() - bd.inicio) / 1000).toFixed(1) + " s\n");
	status(txt);
	publica();
	outlet(2, "fim");
	depois("aviso", function () { outlet(2, "aviso", txt); }, 300);
}

// ---------------------------------------------------------------- UNDO: apaga os clips do rascunho e os locators criados
var ud = null;
function undo() {
	if (busy || travado()) return;
	if (!criado) { status("UNDO: nada para apagar (nenhum BUILD nesta sessão do device)"); return; }
	var lista = [];
	for (var tid in criado.faixas) if (criado.faixas.hasOwnProperty(tid)) {
		var t = new LiveAPI("id " + tid);
		if (Number(t.id) != Number(tid)) continue;
		var ids = idsDe(t, "arrangement_clips");
		for (var j = 0; j < ids.length; j++) {
			var c = aponta("id " + ids[j]), s = num(c.get("start_time"));
			if (s >= criado.ini - 0.001 && s < criado.fim - 0.001) lista.push({ tid: Number(tid), cid: ids[j] });
		}
	}
	busy = true;
	ud = { lista: lista, i: 0, apagados: 0, cues: criado.cues.slice(), ic: 0, cuesApagados: 0 };
	log("DESENROLA: UNDO · " + lista.length + " clips · " + ud.cues.length + " locators\n");
	vigiaLiga();
	proximo(passoUndo, 0);
}
function passoUndo() {
	try {
		for (var q = 0; q < 6 && ud.i < ud.lista.length; q++) {
			var it = ud.lista[ud.i++];
			try { new LiveAPI("id " + it.tid).call("delete_clip", "id", it.cid); ud.apagados++; } catch (e) { }
		}
		if (ud.i < ud.lista.length) { onde = "UNDO"; status("UNDO " + ud.i + "/" + ud.lista.length); proximo(passoUndo, PAUSA); return; }
		if (ud.ic < ud.cues.length) {
			var t = ud.cues[ud.ic++], set = new LiveAPI("live_set"), antes = num(set.get("current_song_time"));
			var ids = idsDe(set, "cue_points");
			for (var i = 0; i < ids.length; i++) if (Math.abs(num(new LiveAPI("id " + ids[i]).get("time")) - t) < 0.01) {
				set.set("current_song_time", t); set.call("set_or_delete_cue"); ud.cuesApagados++; break;
			}
			try { set.set("current_song_time", antes); } catch (e) { }
			proximo(passoUndo, PAUSA);
			return;
		}
		busy = false;
		vigiaDesliga();
		criado = null;
		var txt = "UNDO FEITO · " + ud.apagados + " clips apagados" + (ud.cuesApagados ? " · " + ud.cuesApagados + " locators" : "") + " · o loop ficou como estava";
		log("DESENROLA: " + txt + "\n");
		status(txt);
		publica();
		outlet(2, "aviso", txt);
	} catch (e) { busy = false; vigiaDesliga(); log("DESENROLA erro no UNDO: " + e + "\n"); status("erro no UNDO · ver console do Max"); }
}
