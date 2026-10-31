// VASSOURA — protótipo 0.1 (SÓ DETECÇÃO: lê e pinta, não altera o set)
// Roda no objeto [v8] (Max 9 / Live 12). Lê os clips de arranjo das faixas de
// áudio, carrega cada arquivo no buffer~ e acha trechos abaixo do THRESHOLD
// por mais que MIN LENGTH. Resultado vai para o dict "vassoura_data", que a
// janela (vassoura_view.js) desenha.

autowatch = 1;
inlets = 1;
outlets = 3; // 0: buffer~   1: status (message box)   2: refresh da janela

var BUF = "vassoura_scan";
var DICT = "vassoura_data";
var WIN_MS = 10;

var thresholdDb = -60;
var minLenMs = 1000;
var ready = false;
var busy = false;
var jobs = [];
var cur = -1;
var result = null;
var watchdog = null;

function init() {
	ready = true;
	status("PRONTO · clique ANALYZE");
}

function threshold(v) { thresholdDb = v; }
function minlength(v) { minLenMs = v; }

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
}

function clipOf(job) {
	return result.tracks[job.t].clips[job.c];
}

function analyze() {
	if (!ready) { status("aguarde o Live terminar de carregar"); return; }
	if (busy) return;
	busy = true;

	var set = new LiveAPI("live_set");
	var tempo = num(set.get("tempo"));
	result = {
		tempo: tempo,
		sig: num(set.get("signature_numerator")) || 4,
		maxBeat: 0,
		threshold: thresholdDb,
		minlen: minLenMs,
		nEmpty: 0,
		emptyBeats: 0,
		clipBeats: 0,
		tracks: []
	};
	jobs = [];

	var nt = set.getcount("tracks");
	for (var i = 0; i < nt; i++) {
		var t = new LiveAPI("live_set tracks " + i);
		if (num(t.get("is_foldable"))) continue;      // grupo
		if (!num(t.get("has_audio_input"))) continue; // MIDI
		var tr = { name: str(t.get("name")), index: i, skip: 0, clips: [] };
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

	post("VASSOURA: " + result.tracks.length + " faixas de áudio, " + jobs.length + " clips para ler, tempo " + tempo + "\n");
	cur = -1;
	next();
}

// Decide se o clip entra e onde começa a leitura no arquivo (segundos).
function prepara(c, tempo, trName) {
	var durSec = (c.e - c.s) * 60 / tempo;
	c.durSec = durSec;
	if (c.muted) { c.state = "muted"; return; }
	if (!c.file) { c.state = "sem arquivo"; return; }
	if (c.warping) { c.state = "warp (fora do protótipo)"; return; }
	var dm = c.em - c.sm;
	// Com warp desligado o Live pode reportar os markers em segundos ou em
	// beats; descobre pela duração no arranjo.
	if (Math.abs(dm - durSec) <= Math.max(0.05, durSec * 0.01)) {
		c.units = "seg";
		c.fileStart = c.sm;
	} else if (Math.abs(dm - (c.e - c.s)) <= Math.max(0.05, (c.e - c.s) * 0.01)) {
		c.units = "beat";
		c.fileStart = c.sm * 60 / tempo;
	} else if (c.looping) {
		c.state = "loop (fora do protótipo)";
		return;
	} else {
		c.units = "?";
		c.fileStart = c.sm;
	}
	post("  [" + trName + "] " + c.name + " · pos " + c.s.toFixed(2) + "-" + c.e.toFixed(2) +
		" beats · markers " + c.sm.toFixed(3) + "-" + c.em.toFixed(3) + " (" + c.units + ") · sr " + c.sr + "\n");
}

function next() {
	cur++;
	if (cur >= jobs.length) { finish(); return; }
	var c = clipOf(jobs[cur]);
	status("LENDO " + (cur + 1) + "/" + jobs.length + " · " + result.tracks[jobs[cur].t].name);
	if (watchdog) watchdog.cancel();
	watchdog = new Task(function () {
		c.state = "erro ao ler arquivo";
		post("VASSOURA: não carregou " + c.file + "\n");
		next();
	});
	watchdog.schedule(60000);
	outlet(0, "replace", c.file);
}

// bang do buffer~ (arquivo carregado)
function loaded() {
	if (!busy || cur < 0 || cur >= jobs.length) return;
	if (watchdog) { watchdog.cancel(); watchdog = null; }
	var t = new Task(function () {
		try { scan(clipOf(jobs[cur])); }
		catch (e) { clipOf(jobs[cur]).state = "erro: " + e; post("VASSOURA erro: " + e + "\n"); }
		next();
	});
	t.schedule(0);
}

function scan(c) {
	var buf = new Buffer(BUF);
	var frames = buf.framecount();
	var nch = buf.channelcount();
	var sr = c.sr > 0 ? c.sr : 44100;
	var win = Math.max(1, Math.round(sr * WIN_MS / 1000));
	var winSec = win / sr;
	var thr = Math.pow(10, thresholdDb / 20);
	var minWins = Math.ceil((minLenMs / 1000) / winSec);
	var startF = Math.max(0, Math.floor(c.fileStart * sr));
	var endF = Math.min(frames, Math.floor((c.fileStart + c.durSec) * sr));
	var chunk = win * 200;
	var spb = 60 / result.tempo; // segundos por beat

	var wIdx = 0, runStart = -1;
	var regions = [];

	function fecha(endW) {
		if (runStart >= 0 && endW - runStart >= minWins) {
			var a = runStart * winSec, b = Math.min(endW * winSec, c.durSec);
			regions.push({ s: c.s + a / spb, e: c.s + b / spb, keep: 0 });
		}
		runStart = -1;
	}

	for (var f = startF; f < endF; f += chunk) {
		var n = Math.min(chunk, endF - f);
		var nw = Math.ceil(n / win);
		var peaks = [];
		for (var k = 0; k < nw; k++) peaks.push(0);
		for (var ch = 1; ch <= nch; ch++) {
			var arr = buf.peek(ch, f, n);
			if (!(arr instanceof Array)) arr = [arr];
			for (var i = 0; i < arr.length; i++) {
				var v = arr[i] < 0 ? -arr[i] : arr[i];
				var w = (i / win) | 0;
				if (v > peaks[w]) peaks[w] = v;
			}
		}
		for (var k2 = 0; k2 < nw; k2++, wIdx++) {
			if (peaks[k2] < thr) { if (runStart < 0) runStart = wIdx; }
			else fecha(wIdx);
		}
	}
	fecha(wIdx);

	c.empties = regions;
	c.state = "ok";
	post("  -> " + c.name + ": " + regions.length + " vazios (" + frames + " frames, " + nch + " canais)\n");
}

function finish() {
	busy = false;
	if (watchdog) { watchdog.cancel(); watchdog = null; }
	var nEmpty = 0, eb = 0, cb = 0, nTr = 0;
	for (var i = 0; i < result.tracks.length; i++) {
		var tr = result.tracks[i], achou = false;
		for (var j = 0; j < tr.clips.length; j++) {
			var c = tr.clips[j];
			if (c.state != "ok") continue;
			cb += c.e - c.s;
			for (var k = 0; k < c.empties.length; k++) {
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
	var pct = cb > 0 ? Math.round(100 * eb / cb) : 0;

	var d = new Dict(DICT);
	d.parse(JSON.stringify(result));
	outlet(2, "bang");
	status(nEmpty + " vazios em " + nTr + " faixas · " + pct + "% do tempo");
	post("VASSOURA: fim · " + nEmpty + " vazios em " + nTr + " faixas · " + pct + "% do tempo dos clips\n");
}
