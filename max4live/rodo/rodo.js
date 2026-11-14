// RODO 0.1 — normaliza o PICO dos clips de áudio do arranjo pelo Clip Gain (par da VASSOURA).
// Roda no objeto [v8] (Max 9 / Live 12.2+). ANALYZE lê o arquivo de cada clip no buffer~ (uma vez por arquivo),
// guarda o pico em blocos de 10 ms e mede o pico do trecho que o clip toca. Com o Clip Gain atual de cada clip
// (lido em dB do próprio Live, gain_display_string), sabe o pico que SAI de cada clip. TARGET e MODE recalculam na
// hora, sem reler arquivo. APPLY ajusta o Clip Gain, clip a clip, da esquerda para a direita (a janela anima o rodo
// passando junto). RESET volta o ganho que cada clip tinha antes do primeiro APPLY desta sessão.
//
// O LOM dá o ganho do clip num valor 0..1 que NÃO é linear em dB e não é documentado. Por isso o RODO não usa
// fórmula: guarda todo par (valor, dB) que já viu (os ganhos atuais lidos no ANALYZE e cada ajuste feito) e acha o
// valor do dB pedido por interpolação + correção (secante), conferindo no texto que o Live mostra.
//
// MODE: CLIP = cada clip vai ao TARGET · TRACK = o clip mais alto da faixa vai ao TARGET e a faixa toda recebe o
// mesmo ajuste (mantém a dinâmica entre os clips) · ALL = o clip mais alto do set vai ao TARGET e tudo recebe o
// mesmo ajuste (mantém o balanço entre as faixas).

autowatch = 1;
inlets = 1;
outlets = 4; // 0: buffer~   1: status (device)   2: janela   3: eco dos controles (target/mode/scope)

var BUF = "rodo_scan";
var DICT = "rodo_data";   // leve: faixas, clips, picos e ganhos (republicado a cada mudança de TARGET/MODE)
var DICT_ENV = "rodo_env"; // pesado: contorno das ondas (só depois do ANALYZE)
var WIN_MS = 10;
var VERSAO = "0.2.0";
var BUILD = "";           // marca do pacote de teste no log ("teste"...); vazio na versão publicada
var LIC_PACOTE = "rodo", LIC_NOME = "RODO";   // licença: bloco LICENÇA no fim (colado pelo build-rodo.js)
var URL_BAIXAR = "https://insidestudiosp.com.br/baixar/rodo/zip";
var URL_NOVIDADES = "https://insidestudiosp.com.br/rodo/";
var PAUSA_ARQ = 40;
var PAUSA_FAIXA = 10;
var PAUSA_APPLY = 12;
var TOL_DB = 0.04;        // erro aceito no ajuste (o Live mostra o ganho com 1 ou 2 casas; com 1 casa fica o mais perto)
var MIN_DELTA = 0.1;      // ajuste menor que isso não mexe no clip (2º APPLY não fica mexendo à toa)
var TETO_PADRAO = 24;     // até medir o teto do Clip Gain (valor 1), a prévia supõe +24 dB
var ENV_PONTOS = 1100;    // pontos do contorno no set inteiro (a janela tem ~1000 px de largura útil)
var PATCHER = (typeof this != "undefined" && this && this.patcher) ? this.patcher : null;

var cfg = { target: -6, mode: 1, scope: 0 };
var MODE_TXT = ["CLIP", "TRACK", "ALL"];

var ready = false;
var busy = false;
var jobs = [];
var cur = -1;
var result = null;
var peaks = {};       // "t/c" -> Float32Array (pico por bloco de 10 ms do trecho que o clip toca)
var arqs = {};        // arquivo -> { pk, winSec }
var fila_arq = [];
var watchdog = null;
var orig = {};        // id do clip -> { v, db, t, nome } ganho antes do primeiro APPLY (para o RESET)
var nOrig = 0;
var calib = [];       // pares { v, db } vistos (valor 0..1 do LOM -> dB do Live), em ordem de v
var tetoDb = null;    // dB com valor 1 (teto do Clip Gain), quando já medido
var fora = {};        // índice da faixa no set -> 1 (tirada da normalização na janela)

// ---------------------------------------------------------------- tarefas com referência guardada (ver VASSOURA 0.2.3)
var T = {};
function depois(nome, fn, ms) {
	if (T[nome]) { try { T[nome].cancel(); } catch (e) { } }
	T[nome] = new Task(fn);
	T[nome].schedule(ms);
}
var seqPasso = 0, etapa = null, ultimoPasso = 0, retomadas = 0;
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
			log("RODO: corrente parada há " + Math.round((agora() - ultimoPasso) / 1000) + " s em [" + onde + "], retomando (" + retomadas + ")\n");
			proximo(etapa, 0);
		}
	});
	T.vigia.interval = 1000;
	T.vigia.repeat();
}
function vigiaDesliga() { etapa = null; seqPasso++; if (T.vigia) { try { T.vigia.cancel(); } catch (e) { } } }
var onde = "";

function init() {
	ready = true;
	licCarrega();
	status(licLiberada() ? "PRONTO · clique ANALYZE" : "ATIVE A LICENÇA · clique OPEN");
	enviaCfg();
	depois("versao", checaVersao, 4000);
}

// sem licença: nada lê nem mexe no set; a janela mostra a tela de ativação
function travado() {
	if (licLiberada()) return false;
	status("ATIVE A LICENÇA · clique OPEN");
	outlet(2, "lic", licEstado());
	return true;
}
// a licença mudou (ativou, desativou, venceu): janela e device ficam sabendo
function licMudou() {
	outlet(2, "lic", licEstado());
	outlet(1, "lic", lic.ok ? 1 : 0);
	if (!ready) return;
	if (lic.ok && !busy && !result) status("PRONTO · clique ANALYZE");
	else if (!lic.ok) status("ATIVE A LICENÇA · clique OPEN");
}
// valores atuais para a janela (ela pode abrir antes do 1º ANALYZE)
function enviaCfg() { outlet(2, "valores", cfg.target, cfg.mode, cfg.scope); }
function janela() { enviaCfg(); outlet(2, "lic", licEstado()); if (nova) outlet(2, "nova", nova); }

// ---------------------------------------------------------------- controles
function target(v) { cfg.target = Math.max(-12, Math.min(-0.3, num(v))); agendaRecalc(); }
function mode(v) { cfg.mode = Math.max(0, Math.min(2, num(v) | 0)); agendaRecalc(); }
function scope(v) { cfg.scope = num(v) ? 1 : 0; if (result) status("SCOPE mudou · clique ANALYZE de novo"); }

// vindos da janela: mudam o valor e devolvem para o controle do device (sem eco de volta)
function ui_target(v) { target(v); outlet(3, "target", cfg.target); }
function ui_mode(v) { mode(v); outlet(3, "mode", cfg.mode); enviaCfg(); }
function ui_scope(v) { scope(v); outlet(3, "scope", cfg.scope); enviaCfg(); }

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
function f1(v) { return (v > 0.05 ? "+" : v < -0.05 ? "-" : "") + Math.abs(v).toFixed(1); }

// texto de ganho do Live ("-6.02 dB", "0.0 dB", "-inf dB", "+3,5 dB") -> número (null = não entendi)
function dbDoTexto(s) {
	s = str(s).replace(/"/g, "");
	if (/-\s*inf/i.test(s)) return -Infinity;
	var m = /([-+]?\d+(?:[.,]\d+)?)/.exec(s);
	return m ? parseFloat(m[1].replace(",", ".")) : null;
}

// ---------------------------------------------------------------- LiveAPI reaproveitado (ver VASSOURA)
var lapi = null;
var gotoFalhou = false;
function aponta(path) {
	if (lapi && !gotoFalhou) {
		try {
			lapi.goto(path);
			var m = /^id (\d+)$/.exec(path);
			if (!m || Number(lapi.id) == Number(m[1])) return lapi;
		} catch (e) { }
		gotoFalhou = true;
		log("RODO: goto do LiveAPI não funcionou aqui, usando um LiveAPI por consulta\n");
	}
	var a = new LiveAPI(path);
	if (!lapi) lapi = a;
	return a;
}

// ---------------------------------------------------------------- log em arquivo (RODO-log.txt, ao lado do device)
var logPath = null, logOk = false, logMotivo = "";
function log(s, soArq) {
	if (!soArq || !logOk) post(s);
	if (!logOk || !s) return;
	try {
		var f = new File(logPath, "readwrite", "TEXT");
		if (!f.isopen) { logOk = false; post("RODO: parou de gravar o log em " + logPath + "\n"); return; }
		f.position = f.eof;
		f.writestring(hora() + " " + s.replace(/\n$/, "") + "\r\n");
		f.close();
	} catch (e) { logOk = false; post("RODO: parou de gravar o log (" + e + ")\n"); }
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
			logPath = pasta ? pasta + "RODO-log.txt" : "";
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
	post(logOk ? "RODO: log em " + logPath + "\n" : "RODO: sem log em arquivo (" + logMotivo + "), detalhes vão para este console\n");
}
function hora() {
	var d = new Date();
	function p(n) { return (n < 10 ? "0" : "") + n; }
	return p(d.getHours()) + ":" + p(d.getMinutes()) + ":" + p(d.getSeconds());
}
function versaoMax() { try { return String(max.version); } catch (e) { return "?"; } }

function clipOf(job) { return result.tracks[job.t].clips[job.c]; }

// ---------------------------------------------------------------- calibração valor 0..1 <-> dB
function anota(v, db) {
	if (db == null || !isFinite(db) || !(v > 0)) return;
	if (v >= 1 - 1e-7) tetoDb = db;
	for (var i = 0; i < calib.length; i++) {
		if (Math.abs(calib[i].v - v) < 1e-7) { calib[i].db = db; return; }
		if (calib[i].v > v) { calib.splice(i, 0, { v: v, db: db }); return; }
	}
	calib.push({ v: v, db: db });
	if (v >= 1 - 1e-7) tetoDb = db;
}
// melhor palpite do valor para um dB, a partir do que já foi visto
function palpite(db) {
	var n = calib.length;
	if (!n) return 0.5;
	if (n == 1) return lim(calib[0].v + (db - calib[0].db) * 0.012);  // inclinação chutada; a secante corrige
	for (var i = 0; i < n - 1; i++) {
		var a = calib[i], b = calib[i + 1];
		if (db >= a.db && db <= b.db) return b.db > a.db ? a.v + (b.v - a.v) * (db - a.db) / (b.db - a.db) : a.v;
	}
	var p, q;
	if (db < calib[0].db) { p = calib[0]; q = calib[1]; } else { p = calib[n - 2]; q = calib[n - 1]; }
	if (q.db - p.db < 1e-6) return lim(db < p.db ? p.v * 0.8 : q.v + 0.05);
	return lim(p.v + (q.v - p.v) * (db - p.db) / (q.db - p.db));
}
function lim(v) { return Math.max(0.0001, Math.min(1, v)); }

// leva o Clip Gain do clip ao dB pedido; devolve { db: o que ficou, falta: dB que faltou (teto) }
function ajustaGanho(api, querDb) {
	if (tetoDb != null && querDb > tetoDb + TOL_DB) {
		api.set("gain", 1);
		var dbT = dbDoTexto(api.get("gain_display_string"));
		anota(1, dbT);
		return { db: dbT, falta: querDb - dbT };
	}
	var melhor = null;
	for (var it = 0; it < 8; it++) {
		var v = palpite(querDb);
		api.set("gain", v);
		var vReal = num(api.get("gain"));
		var db = dbDoTexto(api.get("gain_display_string"));
		if (db == null) throw "não entendi o ganho que o Live mostrou: " + str(api.get("gain_display_string"));
		anota(vReal > 0 ? vReal : v, db);
		if (!melhor || Math.abs(db - querDb) < Math.abs(melhor.db - querDb)) melhor = { v: vReal, db: db };
		if (Math.abs(db - querDb) <= TOL_DB) return { db: db, falta: 0 };
		if (v >= 1 && db < querDb) { return { db: db, falta: querDb - db }; }  // bateu no teto
	}
	if (melhor && Math.abs(melhor.v - num(api.get("gain"))) > 1e-7) api.set("gain", melhor.v);
	return { db: melhor ? melhor.db : null, falta: 0, aprox: 1 };
}

// ---------------------------------------------------------------- ANALYZE
function analyze(reuso) {
	if (!ready) { status("aguarde o Live terminar de carregar"); return; }
	if (busy || travado()) return;
	busy = true;
	if (!reuso) arqs = {};
	// faixa tirada na janela continua fora depois do APPLY/RESET (releitura automática)
	fora = {};
	if (reuso && result) for (var q = 0; q < result.tracks.length; q++) if (result.tracks[q].skip) fora[result.tracks[q].index] = 1;

	var set = new LiveAPI("live_set");
	var tempo = num(set.get("tempo"));
	result = {
		versao: VERSAO, tempo: tempo,
		sig: num(set.get("signature_numerator")) || 4,
		den: num(set.get("signature_denominator")) || 4,
		minBeat: -1, maxBeat: 0, nClips: 0, tracks: []
	};
	jobs = [];
	peaks = {};

	var selId = -1;
	if (cfg.scope == 1) selId = Number(new LiveAPI("live_set view selected_track").id);

	var nt = set.getcount("tracks");
	if (!reuso) logNovo();
	log("RODO " + VERSAO + (BUILD ? " (" + BUILD + ")" : "") + ": ANALYZE · " + nt + " faixas no set · Live/Max " + versaoMax() + "\n");
	var i = 0;
	vigiaLiga();
	function umaFaixa() {
		try {
			if (i < nt) {
				var esta = i++;
				onde = "ANALYZE faixa " + (esta + 1) + "/" + nt;
				leFaixa(esta, selId, tempo);
				proximo(umaFaixa, PAUSA_FAIXA);
				return;
			}
			etapa = null;
			montaFila();
		} catch (e) {
			busy = false;
			vigiaDesliga();
			log("RODO erro no ANALYZE (faixa " + i + "): " + e + "\n");
			status("erro no ANALYZE na faixa " + i + " · ver console do Max");
		}
	}
	umaFaixa();
}

function leFaixa(i, selId, tempo) {
	var t = aponta("live_set tracks " + i);
	if (num(t.get("is_foldable"))) return;
	if (!num(t.get("has_audio_input"))) return;
	if (cfg.scope == 1 && Number(t.id) != selId) return;
	var tr = { name: str(t.get("name")), index: i, color: num(t.get("color")), skip: fora[i] ? 1 : 0, clips: [] };
	var nc = t.getcount("arrangement_clips");
	status("LENDO FAIXAS " + (i + 1) + " · " + tr.name);
	log("  faixa " + i + " [" + tr.name + "]: " + nc + " clips\n", 1);
	for (var j = 0; j < nc; j++) {
		var c = aponta("live_set tracks " + i + " arrangement_clips " + j);
		var info = {
			id: Number(c.id),
			s: num(c.get("start_time")),
			e: num(c.get("end_time")),
			name: str(c.get("name")),
			warping: num(c.get("warping")),
			looping: num(c.get("looping")),
			muted: num(c.get("muted")),
			sm: num(c.get("start_marker")),
			em: num(c.get("end_marker")),
			ls: num(c.get("loop_start")),
			le: num(c.get("loop_end")),
			sr: num(c.get("sample_rate")),
			gv: num(c.get("gain")),
			g0: dbDoTexto(c.get("gain_display_string")),
			file: str(c.get("file_path")).replace(/\\/g, "/"),
			state: "pendente"
		};
		anota(info.gv, info.g0);
		if (info.warping && !info.muted && info.file) info.wm = leWarp(c);
		prepara(info, tempo, tr.name);
		if (info.e > result.maxBeat) result.maxBeat = info.e;
		if (result.minBeat < 0 || info.s < result.minBeat) result.minBeat = info.s;
		tr.clips.push(info);
		if (info.state == "pendente") jobs.push({ t: result.tracks.length, c: tr.clips.length - 1 });
	}
	result.tracks.push(tr);
}

// warp markers do clip (Live 11+): [{ b: beat, s: segundos do arquivo }]; null se não der para ler
function leWarp(c) {
	try {
		var raw = str(c.get("warp_markers"));
		var k = raw.indexOf("{");
		if (k < 0) return null;
		var j = JSON.parse(raw.substring(k));
		var lst = j.warp_markers || j;
		if (!(lst instanceof Array)) lst = [lst];
		var out = [];
		for (var i = 0; i < lst.length; i++) {
			var b = parseFloat(lst[i].beat_time), s = parseFloat(lst[i].sample_time);
			if (!isNaN(b) && !isNaN(s)) out.push({ b: b, s: s });
		}
		out.sort(function (x, y) { return x.b - y.b; });
		return out.length >= 2 ? out : null;
	} catch (e) { return null; }
}
function beatParaSeg(wm, b) {
	var i = 0;
	while (i < wm.length - 2 && b > wm[i + 1].b) i++;
	var p = wm[i], q = wm[i + 1];
	return q.b > p.b ? p.s + (q.s - p.s) * (b - p.b) / (q.b - p.b) : p.s;
}

// Que trecho do arquivo (segundos) o clip toca: fs0..fs1. Clip sem loop toca loop_start..loop_end (LOM).
function prepara(c, tempo, trName) {
	var durSec = (c.e - c.s) * 60 / tempo;
	if (c.muted) { c.state = "muted"; return; }
	if (!c.file) { c.state = "sem arquivo"; return; }
	if (c.g0 == null) { c.state = "ganho ?"; log("  [" + trName + "] " + c.name + ": não entendi o ganho do Live\n"); return; }
	if (c.g0 == -Infinity) { c.state = "ganho -inf"; return; }
	var a = c.looping ? Math.min(c.sm, c.ls) : (c.le > c.ls ? c.ls : c.sm);
	var b = c.looping ? c.le : (c.le > c.ls ? c.le : c.em);
	if (c.warping) {
		if (c.wm) { c.fs0 = beatParaSeg(c.wm, a); c.fs1 = beatParaSeg(c.wm, b); c.units = "warp"; }
		else { c.fs0 = 0; c.fs1 = 1e9; c.units = "warp (arquivo inteiro)"; }
	} else {
		// sem warp o Live dá os markers em segundos; confere pela duração (clip sem loop) para pegar o caso "beats"
		var dm = b - a;
		if (!c.looping && Math.abs(dm - (c.e - c.s)) <= Math.max(0.05, (c.e - c.s) * 0.01) && Math.abs(dm - durSec) > Math.max(0.05, durSec * 0.01)) {
			c.fs0 = a * 60 / tempo; c.fs1 = b * 60 / tempo; c.units = "beat";
		} else { c.fs0 = a; c.fs1 = b; c.units = "seg"; }
	}
	if (!(c.fs1 > c.fs0)) { c.fs0 = 0; c.fs1 = 1e9; c.units += " (?)"; }
	log("    clip " + c.name + " · " + c.units + " · arquivo " + c.fs0.toFixed(2) + "-" + (c.fs1 < 1e8 ? c.fs1.toFixed(2) : "fim") +
		" s · ganho " + c.g0 + " dB\n", 1);
}

function montaFila() {
	fila_arq = [];
	var porArq = {};
	for (var q = 0; q < jobs.length; q++) {
		var cj = clipOf(jobs[q]);
		if (arqs[cj.file]) { recorta(jobs[q]); continue; }
		if (!porArq[cj.file]) { porArq[cj.file] = { file: cj.file, sr: cj.sr, jobs: [] }; fila_arq.push(porArq[cj.file]); }
		porArq[cj.file].jobs.push(jobs[q]);
	}
	log("RODO " + VERSAO + ": " + result.tracks.length + " faixas de áudio, " + jobs.length + " clips, " + fila_arq.length + " arquivos para ler, tempo " + result.tempo + "\n");
	cur = -1;
	next();
}

function next() {
	cur++;
	if (cur >= fila_arq.length) { finish(); return; }
	var fa = fila_arq[cur];
	status("LENDO " + (cur + 1) + "/" + fila_arq.length + " · " + result.tracks[fa.jobs[0].t].name);
	if (watchdog) watchdog.cancel();
	watchdog = new Task(function () {
		for (var k = 0; k < fa.jobs.length; k++) clipOf(fa.jobs[k]).state = "erro ao ler arquivo";
		log("RODO: não carregou " + fa.file + "\n");
		proximo(next, PAUSA_ARQ);
	});
	watchdog.schedule(60000);
	onde = "lendo arquivo " + (cur + 1) + "/" + fila_arq.length;
	log("  lendo " + (cur + 1) + "/" + fila_arq.length + ": " + fa.file + "\n", 1);
	esperaArquivo();
	outlet(0, "replace", fa.file);
}

// bang do buffer~ (arquivo carregado)
function loaded() {
	if (!busy || cur < 0 || cur >= fila_arq.length) return;
	if (watchdog) { watchdog.cancel(); watchdog = null; }
	var fa = fila_arq[cur], esse = cur;
	proximo(function () {
		if (!busy || cur != esse) return;
		try {
			scanArquivo(fa);
			for (var k = 0; k < fa.jobs.length; k++) recorta(fa.jobs[k]);
		} catch (e) {
			for (var k2 = 0; k2 < fa.jobs.length; k2++) clipOf(fa.jobs[k2]).state = "erro: " + e;
			log("RODO erro: " + e + "\n");
		}
		proximo(next, PAUSA_ARQ);
	}, 0);
}

function scanArquivo(fa) {
	var buf = new Buffer(BUF);
	var frames = buf.framecount();
	var nch = buf.channelcount();
	var sr = fa.sr > 0 ? fa.sr : 44100;
	var win = Math.max(1, Math.round(sr * WIN_MS / 1000));
	log("    " + frames + " amostras · " + nch + " canais · " + (frames / sr / 60).toFixed(1) + " min\n", 1);
	var pk = new Float32Array(Math.ceil(frames / win));
	var chunk = win * 100;
	for (var f = 0; f < frames; f += chunk) {
		var n = Math.min(chunk, frames - f);
		var w0 = (f / win) | 0;
		for (var ch = 1; ch <= nch; ch++) {
			var a = buf.peek(ch, f, n);
			if (typeof a == "number") a = [a];
			for (var i = 0; i < a.length; i++) {
				var v = a[i] < 0 ? -a[i] : a[i];
				var w = w0 + ((i / win) | 0);
				if (v > pk[w]) pk[w] = v;
			}
			a = null;
		}
	}
	arqs[fa.file] = { pk: pk, winSec: win / sr };
}

// pico (dB) do trecho que o clip toca
function recorta(job) {
	var c = clipOf(job);
	var fa = arqs[c.file];
	var ws = fa.winSec;
	var w0 = Math.max(0, Math.floor(c.fs0 / ws));
	var w1 = Math.min(fa.pk.length, Math.ceil(c.fs1 / ws));
	var sub = w1 > w0 ? fa.pk.subarray(w0, w1) : new Float32Array(0);
	peaks[job.t + "/" + job.c] = sub;
	var m = 0;
	for (var i = 0; i < sub.length; i++) if (sub[i] > m) m = sub[i];
	if (m <= 0.0000001) { c.state = "silêncio"; return; }
	c.pk = 20 * Math.log(m) / Math.LN10;
	c.state = "ok";
}

function finish() {
	busy = false;
	vigiaDesliga();
	if (retomadas) log("RODO: a corrente parou e foi retomada " + retomadas + " vez(es)\n");
	if (watchdog) { watchdog.cancel(); watchdog = null; }
	if (fila_arq.length) outlet(0, "sizeinsamps", 1);
	fila_arq = [];
	publicaEnv();
	recalc();
	if (posApply) { var txt = posApply; posApply = ""; status(txt); outlet(2, "aviso", txt); }
}

// contorno das ondas para a janela: por clip, uma string com 1 caractere por ponto (0..63 = raiz da amplitude
// relativa ao pico do clip). Raiz porque a janela desenha em escala de raiz da amplitude (dB achatava tudo).
function publicaEnv() {
	var span = Math.max(1, result.maxBeat - Math.max(0, result.minBeat));
	var env = { tracks: [] };
	for (var i = 0; i < result.tracks.length; i++) {
		var tr = result.tracks[i], lst = [];
		for (var j = 0; j < tr.clips.length; j++) {
			var c = tr.clips[j], pk = peaks[i + "/" + j];
			if (c.state != "ok" || !pk || !pk.length) { lst.push(""); continue; }
			var n = Math.max(4, Math.min(1200, Math.round(ENV_PONTOS * (c.e - c.s) / span)));
			var ref = Math.pow(10, c.pk / 20), s = "";
			for (var k = 0; k < n; k++) {
				var a0 = Math.floor(k * pk.length / n), a1 = Math.max(a0 + 1, Math.floor((k + 1) * pk.length / n)), mx = 0;
				for (var q = a0; q < a1 && q < pk.length; q++) if (pk[q] > mx) mx = pk[q];
				var r = Math.min(1, Math.sqrt(mx / ref));
				s += String.fromCharCode(48 + Math.round(r * 63));
			}
			lst.push(s);
		}
		env.tracks.push(lst);
	}
	var d = new Dict(DICT_ENV);
	d.parse(JSON.stringify(env));
	outlet(2, "ondas");
}

// ---------------------------------------------------------------- TARGET / MODE -> ganho de cada clip
var recalcTask = null;
function agendaRecalc() {
	if (!result || busy) return;
	if (recalcTask) recalcTask.cancel();
	recalcTask = new Task(function () { recalc(); });
	recalcTask.schedule(80);
}

function lerMarcas() {
	var d = new Dict(DICT), j = null;
	try { j = JSON.parse(d.stringify()); } catch (e) { j = null; }
	if (!j || !j.tracks) return;
	var tr = j.tracks instanceof Array ? j.tracks : [j.tracks];
	for (var i = 0; i < tr.length && i < result.tracks.length; i++) result.tracks[i].skip = tr[i].skip ? 1 : 0;
}

function recalc() {
	if (!result) return;
	var T0 = cfg.target, gmax = -1e9, i, j, c, tr;
	// pico que SAI de cada clip hoje = pico do arquivo + Clip Gain atual
	for (i = 0; i < result.tracks.length; i++) {
		tr = result.tracks[i]; tr.out = -1e9;
		for (j = 0; j < tr.clips.length; j++) {
			c = tr.clips[j];
			if (c.state != "ok") continue;
			c.out = c.pk + c.g0;
			if (c.out > tr.out) tr.out = c.out;
		}
		if (!tr.skip && tr.out > gmax) gmax = tr.out;
	}
	var nOk = 0, dmin = 1e9, dmax = -1e9, nTeto = 0;
	for (i = 0; i < result.tracks.length; i++) {
		tr = result.tracks[i]; tr.delta = null; tr.falta = 0;
		for (j = 0; j < tr.clips.length; j++) {
			c = tr.clips[j];
			if (c.state != "ok") continue;
			var d = cfg.mode == 0 ? T0 - c.out : cfg.mode == 1 ? T0 - tr.out : T0 - gmax;
			var quer = c.g0 + d;
			c.g1 = quer;
			c.falta = 0;
			var teto = tetoDb != null ? tetoDb : TETO_PADRAO;
			if (quer > teto) { c.g1 = teto; c.falta = quer - teto; }
			c.delta = c.g1 - c.g0;
			if (c.falta > tr.falta) tr.falta = c.falta;
			if (tr.skip) continue;
			nOk++;
			if (cfg.mode != 0 && tr.delta == null) tr.delta = d;
			if (c.delta < dmin) dmin = c.delta;
			if (c.delta > dmax) dmax = c.delta;
		}
		if (tr.falta > 0 && !tr.skip) nTeto++;
	}
	result.nClips = nOk;
	result.dmin = nOk ? dmin : 0;
	result.dmax = nOk ? dmax : 0;
	result.nTeto = nTeto;
	result.teto = tetoDb;
	result.cfg = cfg;
	result.nOrig = nOrig;
	var d2 = new Dict(DICT);
	d2.parse(JSON.stringify(result));
	outlet(2, "bang");
	if (!busy) status(nOk ? nOk + " clips · TARGET " + cfg.target.toFixed(1) + " dB · " + MODE_TXT[cfg.mode] + " · ajuste de " + f1(result.dmin) + " a " + f1(result.dmax) + " dB" + (nTeto ? " · " + nTeto + " no teto" : "")
		: "nenhum clip de áudio para normalizar (WARP/LOOP/MUTE ficam fora? ver console)");
}

// janela: clique no nome da faixa tira/põe a faixa
function marcas() {
	if (!result || busy) return;
	lerMarcas();
	recalc();
}

// ---------------------------------------------------------------- APPLY (da esquerda para a direita)
var fila = [];
var ap = null;
var posApply = "";

function apply() {
	if (busy || travado()) return;
	if (!result) { status("clique ANALYZE antes do APPLY"); return; }
	lerMarcas();
	recalc();
	fila = [];
	for (var i = 0; i < result.tracks.length; i++) {
		var tr = result.tracks[i];
		if (tr.skip) continue;
		for (var j = 0; j < tr.clips.length; j++) {
			var c = tr.clips[j];
			if (c.state != "ok") continue;
			fila.push({ t: i, c: j, s: c.s });
		}
	}
	if (!fila.length) { status("nada para aplicar"); return; }
	fila.sort(function (a, b) { return a.s - b.s || a.t - b.t; });  // ordem do rodo: da esquerda para a direita
	busy = true;
	ap = { i: 0, feitos: 0, iguais: 0, falhas: 0, teto: 0, aprox: 0, inicio: agora() };
	log("RODO: APPLY " + MODE_TXT[cfg.mode] + " · TARGET " + cfg.target + " dB · " + fila.length + " clips\n");
	outlet(2, "varre", fila.length, result.minBeat, result.maxBeat);
	vigiaLiga();
	passoApply();
}

// clip ainda é o mesmo do ANALYZE? (mesmo id, posição e arquivo)
function confere(tr, c) {
	var a = aponta("id " + c.id);
	try {
		if (Number(a.id) == c.id && Math.abs(num(a.get("start_time")) - c.s) < 0.002 &&
			str(a.get("file_path")).replace(/\\/g, "/") == c.file) return new LiveAPI("id " + c.id);
	} catch (e) { }
	return null;
}

function passoApply() {
	try {
		if (ap.i >= fila.length) { fimApply(); return; }
		var job = fila[ap.i++];
		var tr = result.tracks[job.t], c = tr.clips[job.c];
		onde = "APPLY clip " + ap.i + "/" + fila.length;
		status("APPLY " + ap.i + "/" + fila.length + " · " + tr.name);
		var tag = "[" + tr.name + "] " + c.name;
		if (Math.abs(c.g1 - c.g0) < MIN_DELTA && !(c.falta > 0 && tetoDb == null)) {
			ap.iguais++;
		} else {
			var api = confere(tr, c);
			if (!api) {
				ap.falhas++;
				log("  " + tag + ": mudou desde o ANALYZE, pulado\n");
			} else {
				var antes = { v: num(api.get("gain")), db: dbDoTexto(api.get("gain_display_string")) };
				if (!orig[c.id]) { orig[c.id] = { v: antes.v, db: antes.db, nome: tag }; nOrig++; }
				var r = ajustaGanho(api, c.g0 + (c.g1 - c.g0) + (c.falta || 0));
				if (r.falta > TOL_DB) { ap.teto++; log("  " + tag + ": teto do Clip Gain (" + r.db + " dB), faltou " + r.falta.toFixed(1) + " dB\n"); }
				else if (r.aprox) { ap.aprox++; log("  " + tag + ": ficou em " + r.db + " dB (pedido " + c.g1.toFixed(2) + ")\n"); }
				else log("    " + tag + ": " + antes.db + " -> " + r.db + " dB\n", 1);
				ap.feitos++;
			}
		}
		outlet(2, "progresso", ap.i, fila.length, c.s);
	} catch (e) {
		ap.falhas++;
		log("RODO erro no APPLY: " + e + "\n");
	}
	if (busy) proximo(passoApply, PAUSA_APPLY);
}

function fimApply() {
	busy = false;
	vigiaDesliga();
	if (retomadas) log("RODO: a corrente parou e foi retomada " + retomadas + " vez(es)\n");
	posApply = "APPLY FEITO · " + ap.feitos + " clips ajustados" + (ap.iguais ? " · " + ap.iguais + " já estavam no ponto" : "") +
		(ap.teto ? " · " + ap.teto + " no teto do Clip Gain" : "") + (ap.falhas ? " · " + ap.falhas + " pulados (ver console)" : "") +
		" · RESET volta como estava";
	log("RODO: " + posApply + " · " + ((agora() - ap.inicio) / 1000).toFixed(1) + " s\n");
	outlet(2, "fim");
	// relê o set (ganhos novos) sem reler os arquivos
	depois("reanalisa", function () { analyze(1); }, 200);
}

// ---------------------------------------------------------------- RESET (volta o ganho de antes do 1º APPLY)
var filaReset = [];
function reset() {
	if (busy || travado()) return;
	filaReset = [];
	for (var id in orig) if (orig.hasOwnProperty(id)) filaReset.push(id);
	if (!filaReset.length) { status("RESET: nada para voltar (nenhum APPLY nesta sessão do device)"); return; }
	busy = true;
	ap = { i: 0, feitos: 0, falhas: 0 };
	log("RODO: RESET · " + filaReset.length + " clips\n");
	vigiaLiga();
	passoReset();
}
function passoReset() {
	try {
		if (ap.i >= filaReset.length) {
			busy = false;
			vigiaDesliga();
			posApply = "RESET FEITO · " + ap.feitos + " clips de volta ao ganho original" + (ap.falhas ? " · " + ap.falhas + " não achados" : "");
			log("RODO: " + posApply + "\n");
			orig = {}; nOrig = 0;
			depois("reanalisa", function () { analyze(1); }, 200);
			return;
		}
		var id = filaReset[ap.i++], o = orig[id];
		onde = "RESET " + ap.i + "/" + filaReset.length;
		status("RESET " + ap.i + "/" + filaReset.length);
		var a = aponta("id " + Number(id)), ok = false;
		try { ok = Number(a.id) == Number(id); } catch (e) { ok = false; }
		if (!ok) { ap.falhas++; log("  " + o.nome + ": clip não existe mais\n"); }
		else { a.set("gain", o.v); ap.feitos++; }
	} catch (e) { ap.falhas++; log("RODO erro no RESET: " + e + "\n"); }
	if (busy) proximo(passoReset, PAUSA_APPLY);
}
