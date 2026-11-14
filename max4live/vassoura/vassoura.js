// VASSOURA 0.2 — acha os trechos vazios dos clips de áudio do arranjo e, no APPLY, muta (MUTE) ou apaga (CUT).
// Roda no objeto [v8] (Max 9 / Live 12). ANALYZE lê o arquivo de cada clip no buffer~ e guarda o pico em blocos
// de 10 ms; THRESHOLD, MIN LENGTH, PRE, POST e SNAP recalculam na hora, sem reler os arquivos. O resultado vai
// para o dict "vassoura_data", que a janela (vassoura_view.js) desenha.
//
// Como o APPLY edita o arranjo (o LOM não tem "split"): para cada clip, uma CÓPIA de trabalho é posta depois do
// fim da música; para cada vazio, a cópia é aparada (markers) para mostrar só aquele trecho do arquivo, mutada,
// conferida (tamanho certo) e colada por cima do vazio — o Live divide o clip original em volta dela. No CUT,
// o pedaço colado é apagado em seguida. A cópia de trabalho é apagada no fim.

autowatch = 1;
inlets = 1;
outlets = 4; // 0: buffer~   1: status (device)   2: janela (bang / status)   3: eco dos controles (mode/snap/scope)

var BUF = "vassoura_scan";
var DICT = "vassoura_data";
var WIN_MS = 10;
var VERSAO = "0.3.0";
var BUILD = "";  // marca de pacote de teste no log ("teste2"...); vazio na versão publicada
var PAUSA_ARQ = 40;    // ms entre um arquivo e outro (Live respira, memória é liberada)
var PAUSA_APPLY = 15;  // ms entre um passo do APPLY e outro
var PAUSA_FAIXA = 10;  // ms entre uma faixa e outra no ANALYZE
var PATCHER = (typeof this != "undefined" && this && this.patcher) ? this.patcher : null;

var cfg = { threshold: -60, minlen: 1000, pre: 10, post: 100, snap: 0, mode: 0, scope: 0 };
var SNAP_TXT = ["OFF", "1/16", "BAR"];
var MODE_TXT = ["MUTE", "CUT"];
var SCOPE_TXT = ["ALL TRACKS", "SELECTED"];

var ready = false;
var busy = false;     // lendo arquivos ou aplicando
var jobs = [];
var cur = -1;
var result = null;
var peaks = {};       // "t/c" -> Float32Array (pico por bloco de 10 ms)
var arqs = {};        // arquivo -> { pk: Float32Array do arquivo INTEIRO, winSec } — cada arquivo é lido UMA vez
var fila_arq = [];    // arquivos a ler: [{ file, sr, jobs: [{t, c}] }]
var watchdog = null;
var recalcTask = null;
var armado = 0;       // CUT pede 2º clique em até 4 s

// ---------------------------------------------------------------- tarefas (Task) com referência guardada
// Task sem referência pode ser recolhida pelo coletor de lixo ANTES de rodar: a corrente para no meio, sem erro
// (0.2.3 no Live 12.4: ANALYZE parava na faixa 7). Por isso toda Task fica em T, e os passos de ANALYZE/APPLY passam
// por proximo(): ele marca a hora do último passo e o vigia retoma a corrente se ela ficar parada mais de 3 s.
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
		if (meu != seqPasso) return;  // o vigia já retomou por outra Task
		ultimoPasso = agora();
		fn();
	});
	T.passo.schedule(ms);
}
function esperaArquivo() { etapa = null; seqPasso++; }  // esperando o buffer~: quem cuida é o watchdog de 60 s
function vigiaLiga() {
	retomadas = 0;
	if (T.vigia) { try { T.vigia.cancel(); } catch (e) { } }
	T.vigia = new Task(function () {
		if (!busy) { T.vigia.cancel(); return; }
		if (etapa && agora() - ultimoPasso > 3000) {
			retomadas++;
			log("VASSOURA: corrente parada há " + Math.round((agora() - ultimoPasso) / 1000) + " s em [" + ondeEstou() + "], retomando (" + retomadas + ")\n");
			proximo(etapa, 0);
		}
	});
	T.vigia.interval = 1000;
	T.vigia.repeat();
}
function vigiaDesliga() { etapa = null; seqPasso++; if (T.vigia) { try { T.vigia.cancel(); } catch (e) { } } }
var onde = "";
function ondeEstou() { return onde; }

function init() {
	ready = true;
	licCarrega();
	status(licLiberada() ? "PRONTO · clique ANALYZE" : "ATIVE A LICENÇA · clique OPEN");
	enviaCfg();
	depois("versao", checaVersao, 4000);  // deixa o set terminar de abrir antes de ir à internet
}

// versão nova e licença: bloco LICENÇA no fim (colado pelo build-vassoura.js)
var LIC_PACOTE = "vassoura", LIC_NOME = "VASSOURA";
var URL_BAIXAR = "https://insidestudiosp.com.br/baixar/vassoura/zip";
var URL_NOVIDADES = "https://insidestudiosp.com.br/vassoura/";

// sem licença: nada lê nem mexe no set; a janela mostra a tela de ativação
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
// valores atuais para a janela (ela pode abrir antes do 1º ANALYZE)
function enviaCfg() { outlet(2, "valores", cfg.threshold, cfg.minlen, cfg.pre, cfg.post, cfg.snap, cfg.mode, cfg.scope); }
function janela() { enviaCfg(); outlet(2, "lic", licEstado()); if (nova) outlet(2, "nova", nova); }
// ---------------------------------------------------------------- controles
function threshold(v) { cfg.threshold = num(v); agendaRecalc(); }
function minlength(v) { cfg.minlen = num(v); agendaRecalc(); }
// (não pode chamar "post": é o nome da função do Max que escreve no console)
function prems(v) { cfg.pre = num(v); agendaRecalc(); }
function postms(v) { cfg.post = num(v); agendaRecalc(); }
function snap(v) { cfg.snap = Math.max(0, Math.min(2, num(v) | 0)); agendaRecalc(); }
function mode(v) { cfg.mode = num(v) ? 1 : 0; armado = 0; agendaRecalc(); }
function scope(v) { cfg.scope = num(v) ? 1 : 0; if (result) status("SCOPE mudou · clique ANALYZE de novo"); agendaRecalc(); }

// vindos da janela: mudam o valor e devolvem para o controle do device (sem eco de volta)
// (os controles do device ficaram escondidos, só para o Live guardar os valores no set)
function ui_mode(v) { mode(v); outlet(3, "mode", cfg.mode); enviaCfg(); }
function ui_snap(v) { snap(v); outlet(3, "snap", cfg.snap); enviaCfg(); }
function ui_scope(v) { scope(v); outlet(3, "scope", cfg.scope); enviaCfg(); }
function ui_threshold(v) { threshold(Math.max(-90, Math.min(-20, num(v)))); outlet(3, "threshold", cfg.threshold); }
function ui_minlength(v) { minlength(Math.max(100, Math.min(10000, num(v)))); outlet(3, "minlength", cfg.minlen); }
function ui_pre(v) { prems(Math.max(0, Math.min(1000, num(v)))); outlet(3, "pre", cfg.pre); }
function ui_post(v) { postms(Math.max(0, Math.min(2000, num(v)))); outlet(3, "post", cfg.post); }

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

// ---------------------------------------------------------------- LiveAPI reaproveitado
// Um LiveAPI só para varrer faixas e clips (criar um por clip, em set grande, derrubava o Live).
// Navega com goto(), o método documentado; se o goto não existir ou não pegar, cria um novo (e avisa no log, 1 vez).
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
		log("VASSOURA: goto do LiveAPI não funcionou aqui, usando um LiveAPI por consulta\n");
	}
	var a = new LiveAPI(path);
	if (!lapi) lapi = a;
	return a;
}

// ---------------------------------------------------------------- log em arquivo (VASSOURA-log.txt, ao lado do device)
// Se o Live fechar, a última linha do arquivo mostra onde parou. Qualquer falha ao gravar: segue só com o console.
var logPath = null;
var logOk = false;   // só liga depois que logNovo() CRIOU o arquivo (0.2.3 abria com "readwrite" antes de criar: nunca gravava)
var logMotivo = "";
// soArq = só no arquivo (detalhe por clip/arquivo não polui o console). Sem arquivo, vai tudo para o console.
function log(s, soArq) {
	if (!soArq || !logOk) post(s);
	if (!logOk || !s) return;
	try {
		var f = new File(logPath, "readwrite", "TEXT");
		if (!f.isopen) { logOk = false; post("VASSOURA: parou de gravar o log em " + logPath + "\n"); return; }
		f.position = f.eof;
		f.writestring(hora() + " " + s.replace(/\n$/, "") + "\r\n");
		f.close();
	} catch (e) { logOk = false; post("VASSOURA: parou de gravar o log (" + e + ")\n"); }
}
function pastaDoDevice() {
	var p = PATCHER;
	try { if (!p && typeof this != "undefined" && this && this.patcher) p = this.patcher; } catch (e) { }
	for (var n = 0; p && n < 5; n++) {
		var fp = "";
		try { fp = String(p.filepath || ""); } catch (e) { }
		if (fp) return fp.replace(/[^\/\\]*$/, "");
		try { p = p.parentpatcher; } catch (e) { p = null; }
	}
	return "";
}
// começa um log novo (cria/zera o arquivo) e diz no console onde ele está ou por que não deu
function logNovo() {
	logOk = false;
	try {
		if (logPath == null) {
			var pasta = pastaDoDevice();
			logPath = pasta ? pasta + "VASSOURA-log.txt" : "";
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
	post(logOk ? "VASSOURA: log em " + logPath + "\n" : "VASSOURA: sem log em arquivo (" + logMotivo + "), detalhes vão para este console\n");
}
function hora() {
	var d = new Date();
	function p(n) { return (n < 10 ? "0" : "") + n; }
	return p(d.getHours()) + ":" + p(d.getMinutes()) + ":" + p(d.getSeconds());
}
function versaoMax() {
	try { return String(max.version); } catch (e) { return "?"; }
}

function clipOf(job) {
	return result.tracks[job.t].clips[job.c];
}

// ---------------------------------------------------------------- ANALYZE
// ANALYZE do usuário relê os arquivos; o de depois do APPLY (reuso=1) aproveita o que já foi lido.
function analyze(reuso) {
	if (!ready) { status("aguarde o Live terminar de carregar"); return; }
	if (busy || travado()) return;
	busy = true;
	armado = 0;
	if (!reuso) arqs = {};

	var set = new LiveAPI("live_set");
	var tempo = num(set.get("tempo"));
	var den = num(set.get("signature_denominator")) || 4;
	result = {
		versao: VERSAO,
		tempo: tempo,
		sig: num(set.get("signature_numerator")) || 4,
		den: den,
		maxBeat: 0,
		nEmpty: 0,
		emptyBeats: 0,
		clipBeats: 0,
		tracks: []
	};
	jobs = [];
	peaks = {};

	var selId = -1;
	if (cfg.scope == 1) {
		var sel = new LiveAPI("live_set view selected_track");
		selId = Number(sel.id);
	}

	var nt = set.getcount("tracks");
	if (!reuso) logNovo();  // o ANALYZE automático depois do APPLY continua o mesmo log (não apaga o registro do APPLY)
	log("VASSOURA " + VERSAO + (BUILD ? " (" + BUILD + ")" : "") + ": ANALYZE · " + nt + " faixas no set · Live/Max " + versaoMax() + "\n");
	// uma faixa por vez, devolvendo o controle ao Live entre elas (antes eram todas de uma vez: travava o Live 12.3)
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
			log("VASSOURA erro no ANALYZE (faixa " + i + "): " + e + "\n");
			status("erro no ANALYZE na faixa " + i + " · ver console do Max");
		}
	}
	umaFaixa();
}

function leFaixa(i, selId, tempo) {
	var t = aponta("live_set tracks " + i);
	if (num(t.get("is_foldable"))) return;      // grupo
	if (!num(t.get("has_audio_input"))) return; // MIDI
	if (cfg.scope == 1 && Number(t.id) != selId) return;
	var tr = { name: str(t.get("name")), index: i, color: num(t.get("color")), skip: 0, clips: [] };
	var nc = t.getcount("arrangement_clips");
	status("LENDO FAIXAS " + (i + 1) + " · " + tr.name);
	log("  faixa " + i + " [" + tr.name + "]: " + nc + " clips\n", 1);
	{
		for (var j = 0; j < nc; j++) {
			var c = aponta("live_set tracks " + i + " arrangement_clips " + j);
			var info = {
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
				file: str(c.get("file_path")).replace(/\\/g, "/"),
				state: "pendente",
				empties: []
			};
			prepara(info, tempo, tr.name);
			if (info.e > result.maxBeat) result.maxBeat = info.e;
			tr.clips.push(info);
			if (info.state == "pendente") jobs.push({ t: result.tracks.length, c: tr.clips.length - 1 });
		}
		result.tracks.push(tr);
	}
}

function montaFila() {
	// agrupa por arquivo: vários clips do mesmo arquivo (ex.: depois de um CUT) = uma leitura só
	fila_arq = [];
	var porArq = {};
	for (var q = 0; q < jobs.length; q++) {
		var cj = clipOf(jobs[q]);
		if (arqs[cj.file]) { recorta(jobs[q]); continue; }
		if (!porArq[cj.file]) { porArq[cj.file] = { file: cj.file, sr: cj.sr, jobs: [] }; fila_arq.push(porArq[cj.file]); }
		porArq[cj.file].jobs.push(jobs[q]);
	}
	log("VASSOURA " + VERSAO + ": " + result.tracks.length + " faixas de áudio, " + jobs.length + " clips, " + fila_arq.length + " arquivos para ler, tempo " + result.tempo + "\n");
	cur = -1;
	next();
}

// Decide se o clip entra e onde começa a leitura no arquivo (segundos).
function prepara(c, tempo, trName) {
	var durSec = (c.e - c.s) * 60 / tempo;
	c.durSec = durSec;
	if (c.muted) { c.state = "muted"; return; }
	if (!c.file) { c.state = "sem arquivo"; return; }
	if (c.warping) { c.state = "warp (fora)"; return; }
	// Clip sem loop: o trecho que TOCA é loop_start..loop_end (LOM: "for unlooped clips: clip start/end").
	// Gravação longa aparada no arranjo deixa o end_marker no fim do arquivo — antes isso dava "(?)" e o APPLY errava.
	if (!c.looping && c.le > c.ls) { c.smOrig = c.sm; c.emOrig = c.em; c.sm = c.ls; c.em = c.le; }
	var dm = c.em - c.sm;
	// Sem warp o Live dá os markers em segundos; confere pela duração no arranjo.
	if (Math.abs(dm - durSec) <= Math.max(0.05, durSec * 0.01)) {
		c.units = "seg";
		c.fileStart = c.sm;
	} else if (Math.abs(dm - (c.e - c.s)) <= Math.max(0.05, (c.e - c.s) * 0.01)) {
		c.units = "beat";
		c.fileStart = c.sm * 60 / tempo;
	} else if (c.looping) {
		c.state = "loop (fora)";
		return;
	} else {
		c.units = "?";
		c.fileStart = c.sm;
	}
	if (c.units == "?") log("  [" + trName + "] " + c.name + " · pos " + c.s.toFixed(2) + "-" + c.e.toFixed(2) +
		" beats · markers " + (c.smOrig != null ? c.smOrig : c.sm).toFixed(3) + "-" + (c.emOrig != null ? c.emOrig : c.em).toFixed(3) +
		" · loop " + c.ls.toFixed(3) + "-" + c.le.toFixed(3) + " (" + c.units + ") · sr " + c.sr + "\n");
	else log("    clip " + c.name + " · " + c.units + " · " + c.durSec.toFixed(1) + " s\n", 1);
}

function next() {
	cur++;
	if (cur >= fila_arq.length) { finish(); return; }
	var fa = fila_arq[cur];
	status("LENDO " + (cur + 1) + "/" + fila_arq.length + " · " + result.tracks[fa.jobs[0].t].name);
	if (watchdog) watchdog.cancel();
	watchdog = new Task(function () {
		for (var k = 0; k < fa.jobs.length; k++) clipOf(fa.jobs[k]).state = "erro ao ler arquivo";
		log("VASSOURA: não carregou " + fa.file + "\n");
		proximoArquivo();
	});
	watchdog.schedule(60000);
	onde = "lendo arquivo " + (cur + 1) + "/" + fila_arq.length;
	log("  lendo " + (cur + 1) + "/" + fila_arq.length + ": " + fa.file + "\n", 1);
	esperaArquivo();
	outlet(0, "replace", fa.file);
}

function proximoArquivo() {
	proximo(next, PAUSA_ARQ);
}

// bang do buffer~ (arquivo carregado)
function loaded() {
	if (!busy || cur < 0 || cur >= fila_arq.length) return;
	if (watchdog) { watchdog.cancel(); watchdog = null; }
	var fa = fila_arq[cur], esse = cur;
	proximo(function () {
		if (!busy || cur != esse) return;  // o watchdog já desistiu deste arquivo
		try {
			scanArquivo(fa);
			for (var k = 0; k < fa.jobs.length; k++) recorta(fa.jobs[k]);
		}
		catch (e) {
			for (var k2 = 0; k2 < fa.jobs.length; k2++) clipOf(fa.jobs[k2]).state = "erro: " + e;
			log("VASSOURA erro: " + e + "\n");
		}
		proximoArquivo();
	}, 0);
}

// Pico de cada bloco de 10 ms do arquivo INTEIRO (fica guardado; os clips recortam dele).
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

// Pega do arquivo só o trecho que o clip toca. off = quanto o 1º bloco começa antes do clip.
function recorta(job) {
	var c = clipOf(job);
	var fa = arqs[c.file];
	var ws = fa.winSec;
	var w0 = Math.max(0, Math.floor(c.fileStart / ws));
	var off = Math.max(0, c.fileStart - w0 * ws);
	var w1 = Math.min(fa.pk.length, w0 + Math.ceil((c.durSec + off) / ws));
	peaks[job.t + "/" + job.c] = w1 > w0 ? fa.pk.subarray(w0, w1) : new Float32Array(0);
	c.winSec = ws;
	c.off = off;
	c.state = "ok";
}

function finish() {
	busy = false;
	vigiaDesliga();
	if (retomadas) log("VASSOURA: a corrente parou e foi retomada " + retomadas + " vez(es)\n");
	if (watchdog) { watchdog.cancel(); watchdog = null; }
	if (fila_arq.length) outlet(0, "sizeinsamps", 1);  // devolve a memória do último arquivo
	fila_arq = [];
	recalc(false);
}

// ---------------------------------------------------------------- vazios (THRESHOLD, MIN LENGTH, PRE, POST, SNAP)
function agendaRecalc() {
	if (!result || busy) return;
	if (recalcTask) recalcTask.cancel();
	recalcTask = new Task(function () { recalc(true); });
	recalcTask.schedule(120);
}

// Pega da janela o que o usuário marcou (faixa fora, trecho mantido).
function lerMarcas() {
	var d = new Dict(DICT), j = null;
	try { j = JSON.parse(d.stringify()); } catch (e) { j = null; }
	if (!j || !j.tracks) return;
	var tr = arr(j.tracks);
	for (var i = 0; i < tr.length && i < result.tracks.length; i++) {
		result.tracks[i].skip = tr[i].skip ? 1 : 0;
		var cl = arr(tr[i].clips);
		for (var k = 0; k < cl.length && k < result.tracks[i].clips.length; k++) {
			var mine = result.tracks[i].clips[k];
			mine.empties = arr(cl[k].empties);
		}
	}
}

function arr(x) {
	if (!x) return [];
	return (x instanceof Array) ? x : [x];
}

function grade() {
	if (cfg.snap == 1) return 0.25;                                // 1/16 = quarto de beat
	if (cfg.snap == 2) return result.sig * 4 / (result.den || 4);  // compasso
	return 0;
}

function recalc(manterMarcas) {
	if (!result) return;
	if (manterMarcas) lerMarcas();
	var spb = 60 / result.tempo;
	var thr = Math.pow(10, cfg.threshold / 20);
	var g = grade();
	var EPS = 1e-6;

	for (var i = 0; i < result.tracks.length; i++) {
		var tr = result.tracks[i];
		for (var j = 0; j < tr.clips.length; j++) {
			var c = tr.clips[j];
			if (c.state != "ok") { c.empties = []; continue; }
			var pk = peaks[i + "/" + j];
			if (!pk) continue;
			var mantidos = [];
			var old = arr(c.empties);
			for (var o = 0; o < old.length; o++) if (old[o].keep) mantidos.push(old[o]);

			var minWins = Math.ceil((cfg.minlen / 1000) / c.winSec);
			var regs = [];
			var run = -1;
			for (var w = 0; w <= pk.length; w++) {
				var vazio = w < pk.length && pk[w] < thr;
				if (vazio) { if (run < 0) run = w; continue; }
				if (run >= 0 && w - run >= minWins) {
					var off = c.off || 0;
					var rs = c.s + Math.max(0, run * c.winSec - off) / spb;
					var re = c.s + Math.min(w * c.winSec - off, c.durSec) / spb;
					if (re > c.e) re = c.e;
					var noIni = rs <= c.s + EPS, noFim = re >= c.e - EPS;
					// POST = cauda que fica depois do som; PRE = folga antes do próximo som. Borda do clip não precisa.
					var as = noIni ? c.s : rs + cfg.post / 1000 / spb;
					var ae = noFim ? c.e : re - cfg.pre / 1000 / spb;
					// SNAP: arredonda PARA DENTRO do vazio, nunca invade o som
					if (g > 0) {
						if (!noIni) as = Math.ceil(as / g - EPS) * g;
						if (!noFim) ae = Math.floor(ae / g + EPS) * g;
					}
					if (ae - as > 0.02 / spb) {
						var keep = 0;
						for (var m = 0; m < mantidos.length; m++)
							if (mantidos[m].s < ae && mantidos[m].e > as) { keep = 1; break; }
						regs.push({ s: as, e: ae, rs: rs, re: re, keep: keep });
					}
				}
				run = -1;
			}
			c.empties = regs;
		}
	}
	publica();
}

function publica() {
	var nEmpty = 0, eb = 0, cb = 0, nTr = 0;
	for (var i = 0; i < result.tracks.length; i++) {
		var tr = result.tracks[i], achou = false;
		for (var j = 0; j < tr.clips.length; j++) {
			var c = tr.clips[j];
			if (c.state != "ok") continue;
			cb += c.e - c.s;
			for (var k = 0; k < c.empties.length; k++) {
				if (tr.skip || c.empties[k].keep) continue;
				nEmpty++;
				eb += c.empties[k].e - c.empties[k].s;
				achou = true;
			}
		}
		if (achou) nTr++;
	}
	result.nEmpty = nEmpty;
	result.emptyBeats = eb;
	result.clipBeats = cb;
	result.cfg = cfg;
	var pct = cb > 0 ? Math.round(100 * eb / cb) : 0;

	var d = new Dict(DICT);
	d.parse(JSON.stringify(result));
	outlet(2, "bang");
	status(nEmpty + " vazios em " + nTr + " faixas · " + pct + "% do tempo · APPLY = " + MODE_TXT[cfg.mode]);
}

// janela pede de novo o resumo (clique em faixa/trecho mudou as contas)
function marcas() {
	if (!result || busy) return;
	lerMarcas();
	publica();
}

// ---------------------------------------------------------------- APPLY
var fila = [];
var ap = null;

function apply() {
	if (busy || travado()) return;
	if (!result) { status("clique ANALYZE antes do APPLY"); return; }
	lerMarcas();

	fila = [];
	var total = 0;
	for (var i = 0; i < result.tracks.length; i++) {
		var tr = result.tracks[i];
		if (tr.skip) continue;
		for (var j = 0; j < tr.clips.length; j++) {
			var c = tr.clips[j];
			if (c.state != "ok" || c.units == "?") continue;
			var em = [];
			var lst = arr(c.empties);
			for (var k = 0; k < lst.length; k++) if (!lst[k].keep) em.push(lst[k]);
			if (!em.length) continue;
			fila.push({ t: i, c: j, em: em });
			total += em.length;
		}
	}
	if (!fila.length) { status("nada para aplicar"); return; }

	if (cfg.mode == 1) {
		var agora = new Date().getTime();
		if (!armado || agora - armado > 4000) {
			armado = agora;
			status("CUT apaga " + total + " trechos · clique APPLY de novo em 4 s para confirmar");
			return;
		}
	}
	armado = 0;

	busy = true;
	var set = new LiveAPI("live_set");
	ap = {
		cx: null, i: 0, total: total, feitos: 0, falhas: 0, clipsPulados: 0,
		far: Math.ceil((Math.max(num(set.get("song_length")), result.maxBeat) + 64) / 4) * 4,
		spb: 60 / result.tempo
	};
	outlet(2, "varre", total);
	log("VASSOURA: APPLY " + MODE_TXT[cfg.mode] + " · " + total + " trechos em " + fila.length + " clips · cópia de trabalho no beat " + ap.far + "\n");
	vigiaLiga();
	passo();
}

// APPLY em passos pequenos: cada passo faz UMA coisa (preparar a cópia de trabalho de um clip, aplicar UM trecho,
// ou apagar a cópia) e devolve o controle ao Live por PAUSA_APPLY ms. Antes, um clip inteiro era feito de uma vez
// e cada busca criava um LiveAPI por clip da faixa — em set grande isso travava o Live.
function passo() {
	try {
		if (!ap.cx) {
			if (ap.i >= fila.length) { fimApply(); return; }
			var job = fila[ap.i++];
			onde = "APPLY clip " + ap.i + "/" + fila.length;
			status(MODE_TXT[cfg.mode] + " " + ap.i + "/" + fila.length + " · " + result.tracks[job.t].name);
			preparaClip(job);
		} else if (ap.cx.r < ap.cx.job.em.length) {
			aplicaTrecho(ap.cx, ap.cx.job.em[ap.cx.r++]);
		} else {
			fechaClip(ap.cx);
		}
	} catch (e) {
		var nome = ap.cx ? result.tracks[ap.cx.job.t].name : "";
		log("VASSOURA erro " + nome + ": " + e + "\n");
		if (ap.cx) {
			ap.falhas += ap.cx.job.em.length - ap.cx.r;
			ap.clipsPulados++;
			try { fechaClip(ap.cx); } catch (e2) { ap.cx = null; }
		}
	}
	if (busy) proximo(passo, PAUSA_APPLY);
}

function perto(a, b) { return Math.abs(a - b) < 0.002; }

function scratch(path) { return aponta(path); }
function porId(id) { return aponta("id " + Number(id)); }

// procura na faixa o clip que começa em s (e termina em e, se dado); devolve { id, s, e } ou null
function acha(ti, s, e) {
	var t = scratch("live_set tracks " + ti);
	var n = t.getcount("arrangement_clips");
	for (var j = 0; j < n; j++) {
		var c = scratch("live_set tracks " + ti + " arrangement_clips " + j);
		var cs = num(c.get("start_time"));
		if (!perto(cs, s)) continue;
		var ce = num(c.get("end_time"));
		if (e == null || perto(ce, e)) return { id: Number(c.id), s: cs, e: ce };
	}
	return null;
}

// chama função da faixa que recebe um clip; o LOM aceita o clip como "id N"
function chama(ti, fn, clipId, extra) {
	var t = scratch("live_set tracks " + ti);
	if (extra == null) t.call(fn, "id", Number(clipId));
	else t.call(fn, "id", Number(clipId), extra);
}

// apara a cópia de trabalho para tocar [a, b] do arquivo (unidade dos markers do clip)
function apara(c, a, b) {
	var sm = num(c.get("start_marker"));
	if (a >= sm) {  // início anda para a direita: fim primeiro (b > a >= início atual, sempre válido)
		c.set("end_marker", b); c.set("loop_end", b);
		c.set("start_marker", a); c.set("loop_start", a);
	} else {
		c.set("start_marker", a); c.set("loop_start", a);
		c.set("end_marker", b); c.set("loop_end", b);
	}
	// segunda passada: garante os quatro valores (a ordem pode ter barrado algum)
	c.set("loop_end", b); c.set("end_marker", b);
	c.set("loop_start", a); c.set("start_marker", a);
}

function preparaClip(job) {
	var tr = result.tracks[job.t];
	var c = tr.clips[job.c];
	var ti = tr.index;
	var tag = "[" + tr.name + "] " + c.name;

	// o clip ainda está onde estava no ANALYZE?
	var orig = acha(ti, c.s, c.e);
	if (orig) {
		var oa = porId(orig.id);
		if (str(oa.get("file_path")).replace(/\\/g, "/") != c.file) orig = null;
	}
	if (!orig) {
		ap.clipsPulados++; ap.falhas += job.em.length;
		log("  " + tag + ": mudou desde o ANALYZE, pulado\n");
		return;
	}

	// cópia de trabalho depois do fim da música
	chama(ti, "duplicate_clip_to_arrangement", orig.id, ap.far);
	var tmp = acha(ti, ap.far, null);
	if (!tmp) {
		ap.clipsPulados++; ap.falhas += job.em.length;
		log("  " + tag + ": o Live não criou a cópia de trabalho (faixa congelada?), pulado\n");
		return;
	}
	var api = new LiveAPI("id " + tmp.id);  // LiveAPI próprio: segue a cópia mesmo com clips novos na faixa
	api.set("muted", 1);
	ap.cx = { job: job, ti: ti, c: c, tag: tag, tmpId: tmp.id, api: api, r: 0,
		k: c.units == "seg" ? ap.spb : 1 };  // beats do arranjo -> unidade dos markers
}

function aplicaTrecho(cx, reg) {
	var c = cx.c, ti = cx.ti;
	var a = c.sm + (reg.s - c.s) * cx.k, b = c.sm + (reg.e - c.s) * cx.k;
	apara(cx.api, a, b);
	var ts = num(cx.api.get("start_time")), te = num(cx.api.get("end_time"));
	// trava de segurança: só cola se a cópia ficou exatamente do tamanho do vazio
	if (!perto(te - ts, reg.e - reg.s) || !perto(num(cx.api.get("loop_start")), a)) {
		ap.falhas++;
		log("  " + cx.tag + ": cópia não aparou certo em " + reg.s.toFixed(2) + " (" + (te - ts).toFixed(3) + " de " + (reg.e - reg.s).toFixed(3) + " beats), trecho pulado\n");
		return;
	}
	chama(ti, "duplicate_clip_to_arrangement", cx.tmpId, reg.s);
	outlet(2, "progresso", ap.feitos + ap.falhas + 1, ap.total);
	if (cfg.mode == 1) {
		var peca = acha(ti, reg.s, reg.e);
		var mutada = peca && num(porId(peca.id).get("muted"));
		if (mutada) chama(ti, "delete_clip", peca.id);
		else { ap.falhas++; log("  " + cx.tag + ": pedaço em " + reg.s.toFixed(2) + " ficou MUTADO (não achei para apagar)\n"); return; }
	}
	ap.feitos++;
}

function fechaClip(cx) {
	ap.cx = null;
	chama(cx.ti, "delete_clip", cx.tmpId);
	if (acha(cx.ti, ap.far, null)) log("  " + cx.tag + ": sobrou a cópia de trabalho no beat " + ap.far + " — apague à mão\n");
}

function fimApply() {
	busy = false;
	vigiaDesliga();
	if (retomadas) log("VASSOURA: a corrente parou e foi retomada " + retomadas + " vez(es)\n");
	var txt = MODE_TXT[cfg.mode] + " FEITO · " + ap.feitos + " de " + ap.total + " trechos" +
		(ap.falhas ? " · " + ap.falhas + " pulados (ver console do Max)" : "");
	log("VASSOURA: " + txt + "\n");
	status(txt);
	outlet(2, "fim");
	// relê o set para a janela mostrar como ficou — sem reler os arquivos (os picos já estão guardados)
	depois("reanalisa", function () { analyze(1); }, 300);
	depois("aviso", function () { outlet(2, "aviso", txt); }, 400);
}
