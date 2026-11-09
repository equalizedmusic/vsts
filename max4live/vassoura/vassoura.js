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
var VERSAO = "0.2.2";
var PAUSA_ARQ = 40;    // ms entre um arquivo e outro (Live respira, memória é liberada)
var PAUSA_APPLY = 15;  // ms entre um passo do APPLY e outro

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

function init() {
	ready = true;
	status("PRONTO · clique ANALYZE");
	var t = new Task(checaVersao);
	t.schedule(4000);  // deixa o set terminar de abrir antes de ir à internet
}

// ---------------------------------------------------------------- versão nova
// Pergunta ao servidor de licenças (mesma rota dos VSTs) se há release mais novo. Sem internet, sem XMLHttpRequest
// ou com qualquer erro: fica quieto (só uma linha no console). O link de download é o fixo do site, não o da resposta.
var URL_VERSAO = "https://isp-licencas.insidestudiosp.workers.dev/v1/versao";
var URL_BAIXAR = "https://insidestudiosp.com.br/baixar/vassoura/zip";
var URL_NOVIDADES = "https://github.com/equalizedmusic/vsts/blob/main/novidades/Vassoura.md";
var nova = "";
var versaoChecada = false;

function checaVersao() {
	if (versaoChecada) return;
	versaoChecada = true;
	if (typeof XMLHttpRequest == "undefined") { post("VASSOURA: sem XMLHttpRequest, não confere versão nova\n"); return; }
	try {
		var so = (typeof max != "undefined" && String(max.os).indexOf("mac") >= 0) ? "mac" : "win";
		var x = new XMLHttpRequest();
		x.open("GET", URL_VERSAO + "?pacote=vassoura&versao=" + VERSAO + "&sistema=" + so);
		x.onreadystatechange = function () {
			if (x.readyState != 4) return;
			try {
				var r = JSON.parse(x.responseText);
				var v = r && r.ok && r.atualizacao ? String(r.atualizacao.versao || "") : "";
				if (!/^\d+\.\d+\.\d+$/.test(v) || comparaVersao(v, VERSAO) <= 0) return;
				nova = v;
				post("VASSOURA: versão nova " + v + " disponível · " + URL_BAIXAR + "\n");
				outlet(2, "nova", v);
				if (!busy && !result) outlet(1, "set", "NOVA VERSÃO " + v + " · abra a janela (OPEN)");  // só no device
			} catch (e) { post("VASSOURA: resposta de versão inválida\n"); }
		};
		x.send();
	} catch (e) { post("VASSOURA: não conferiu versão nova (" + e + ")\n"); }
}

function comparaVersao(a, b) {
	var pa = a.split("."), pb = b.split(".");
	for (var i = 0; i < 3; i++) { var d = (parseInt(pa[i], 10) || 0) - (parseInt(pb[i], 10) || 0); if (d) return d; }
	return 0;
}

// vindos da janela (botão da faixa de versão nova)
function baixar_nova() { if (nova) max.launchbrowser(URL_BAIXAR); }
function novidades() { max.launchbrowser(URL_NOVIDADES); }

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
function ui_mode(v) { mode(v); outlet(3, "mode", cfg.mode); }
function ui_snap(v) { snap(v); outlet(3, "snap", cfg.snap); }
function ui_scope(v) { scope(v); outlet(3, "scope", cfg.scope); }

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
	outlet(1, "set", txt);
	outlet(2, "status", txt);
}

function clipOf(job) {
	return result.tracks[job.t].clips[job.c];
}

// ---------------------------------------------------------------- ANALYZE
// ANALYZE do usuário relê os arquivos; o de depois do APPLY (reuso=1) aproveita o que já foi lido.
function analyze(reuso) {
	if (!ready) { status("aguarde o Live terminar de carregar"); return; }
	if (busy) return;
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
	for (var i = 0; i < nt; i++) {
		var t = new LiveAPI("live_set tracks " + i);
		if (num(t.get("is_foldable"))) continue;      // grupo
		if (!num(t.get("has_audio_input"))) continue; // MIDI
		if (cfg.scope == 1 && Number(t.id) != selId) continue;
		var tr = { name: str(t.get("name")), index: i, color: num(t.get("color")), skip: 0, clips: [] };
		var nc = t.getcount("arrangement_clips");
		for (var j = 0; j < nc; j++) {
			var c = new LiveAPI("live_set tracks " + i + " arrangement_clips " + j);
			var info = {
				s: num(c.get("start_time")),
				e: num(c.get("end_time")),
				name: str(c.get("name")),
				warping: num(c.get("warping")),
				looping: num(c.get("looping")),
				muted: num(c.get("muted")),
				sm: num(c.get("start_marker")),
				em: num(c.get("end_marker")),
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

	// agrupa por arquivo: vários clips do mesmo arquivo (ex.: depois de um CUT) = uma leitura só
	fila_arq = [];
	var porArq = {};
	for (var q = 0; q < jobs.length; q++) {
		var cj = clipOf(jobs[q]);
		if (arqs[cj.file]) { recorta(jobs[q]); continue; }
		if (!porArq[cj.file]) { porArq[cj.file] = { file: cj.file, sr: cj.sr, jobs: [] }; fila_arq.push(porArq[cj.file]); }
		porArq[cj.file].jobs.push(jobs[q]);
	}
	post("VASSOURA " + VERSAO + ": " + result.tracks.length + " faixas de áudio, " + jobs.length + " clips, " + fila_arq.length + " arquivos para ler, tempo " + tempo + "\n");
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
	if (c.units == "?") post("  [" + trName + "] " + c.name + " · pos " + c.s.toFixed(2) + "-" + c.e.toFixed(2) +
		" beats · markers " + c.sm.toFixed(3) + "-" + c.em.toFixed(3) + " (" + c.units + ") · sr " + c.sr + "\n");
}

function next() {
	cur++;
	if (cur >= fila_arq.length) { finish(); return; }
	var fa = fila_arq[cur];
	status("LENDO " + (cur + 1) + "/" + fila_arq.length + " · " + result.tracks[fa.jobs[0].t].name);
	if (watchdog) watchdog.cancel();
	watchdog = new Task(function () {
		for (var k = 0; k < fa.jobs.length; k++) clipOf(fa.jobs[k]).state = "erro ao ler arquivo";
		post("VASSOURA: não carregou " + fa.file + "\n");
		proximoArquivo();
	});
	watchdog.schedule(60000);
	outlet(0, "replace", fa.file);
}

function proximoArquivo() {
	var t = new Task(next);
	t.schedule(PAUSA_ARQ);
}

// bang do buffer~ (arquivo carregado)
function loaded() {
	if (!busy || cur < 0 || cur >= fila_arq.length) return;
	if (watchdog) { watchdog.cancel(); watchdog = null; }
	var fa = fila_arq[cur], esse = cur;
	var t = new Task(function () {
		if (!busy || cur != esse) return;  // o watchdog já desistiu deste arquivo
		try {
			scanArquivo(fa);
			for (var k = 0; k < fa.jobs.length; k++) recorta(fa.jobs[k]);
		}
		catch (e) {
			for (var k2 = 0; k2 < fa.jobs.length; k2++) clipOf(fa.jobs[k2]).state = "erro: " + e;
			post("VASSOURA erro: " + e + "\n");
		}
		proximoArquivo();
	});
	t.schedule(0);
}

// Pico de cada bloco de 10 ms do arquivo INTEIRO (fica guardado; os clips recortam dele).
function scanArquivo(fa) {
	var buf = new Buffer(BUF);
	var frames = buf.framecount();
	var nch = buf.channelcount();
	var sr = fa.sr > 0 ? fa.sr : 44100;
	var win = Math.max(1, Math.round(sr * WIN_MS / 1000));
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
	if (busy) return;
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
	post("VASSOURA: APPLY " + MODE_TXT[cfg.mode] + " · " + total + " trechos em " + fila.length + " clips · cópia de trabalho no beat " + ap.far + "\n");
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
			status(MODE_TXT[cfg.mode] + " " + ap.i + "/" + fila.length + " · " + result.tracks[job.t].name);
			preparaClip(job);
		} else if (ap.cx.r < ap.cx.job.em.length) {
			aplicaTrecho(ap.cx, ap.cx.job.em[ap.cx.r++]);
		} else {
			fechaClip(ap.cx);
		}
	} catch (e) {
		var nome = ap.cx ? result.tracks[ap.cx.job.t].name : "";
		post("VASSOURA erro " + nome + ": " + e + "\n");
		if (ap.cx) {
			ap.falhas += ap.cx.job.em.length - ap.cx.r;
			ap.clipsPulados++;
			try { fechaClip(ap.cx); } catch (e2) { ap.cx = null; }
		}
	}
	var t = new Task(passo);
	t.schedule(PAUSA_APPLY);
}

function perto(a, b) { return Math.abs(a - b) < 0.002; }

// um LiveAPI só, reaproveitado, para varrer os clips da faixa
var lapi = null;
function scratch(path) {
	if (!lapi) lapi = new LiveAPI(path);
	else lapi.path = path;
	return lapi;
}

function porId(id) {
	if (!lapi) lapi = new LiveAPI("id " + id);
	else lapi.id = Number(id);
	return lapi;
}

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
		post("  " + tag + ": mudou desde o ANALYZE, pulado\n");
		return;
	}

	// cópia de trabalho depois do fim da música
	chama(ti, "duplicate_clip_to_arrangement", orig.id, ap.far);
	var tmp = acha(ti, ap.far, null);
	if (!tmp) {
		ap.clipsPulados++; ap.falhas += job.em.length;
		post("  " + tag + ": o Live não criou a cópia de trabalho (faixa congelada?), pulado\n");
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
		post("  " + cx.tag + ": cópia não aparou certo em " + reg.s.toFixed(2) + " (" + (te - ts).toFixed(3) + " de " + (reg.e - reg.s).toFixed(3) + " beats), trecho pulado\n");
		return;
	}
	chama(ti, "duplicate_clip_to_arrangement", cx.tmpId, reg.s);
	if (cfg.mode == 1) {
		var peca = acha(ti, reg.s, reg.e);
		var mutada = peca && num(porId(peca.id).get("muted"));
		if (mutada) chama(ti, "delete_clip", peca.id);
		else { ap.falhas++; post("  " + cx.tag + ": pedaço em " + reg.s.toFixed(2) + " ficou MUTADO (não achei para apagar)\n"); return; }
	}
	ap.feitos++;
}

function fechaClip(cx) {
	ap.cx = null;
	chama(cx.ti, "delete_clip", cx.tmpId);
	if (acha(cx.ti, ap.far, null)) post("  " + cx.tag + ": sobrou a cópia de trabalho no beat " + ap.far + " — apague à mão\n");
}

function fimApply() {
	busy = false;
	var txt = MODE_TXT[cfg.mode] + " FEITO · " + ap.feitos + " de " + ap.total + " trechos" +
		(ap.falhas ? " · " + ap.falhas + " pulados (ver console do Max)" : "");
	post("VASSOURA: " + txt + "\n");
	status(txt);
	// relê o set para a janela mostrar como ficou — sem reler os arquivos (os picos já estão guardados)
	var t = new Task(function () { analyze(1); });
	t.schedule(300);
	var t2 = new Task(function () { outlet(2, "aviso", txt); });
	t2.schedule(400);
}
