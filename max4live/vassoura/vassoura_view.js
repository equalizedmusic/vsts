// VASSOURA — janela flutuante (jsui). Desenha o dict "vassoura_data".
// Verde = clip com som (fica) · vermelho = vazio (sai) · cinza = vazio marcado para FICAR.
// Clique no nome da faixa: deixa a faixa fora da limpeza. Clique no vermelho: mantém o trecho.

mgraphics.init();
mgraphics.relative_coords = 0;
mgraphics.autofill = 0;

var DICT = "vassoura_data";
var NAME_W = 210;
var HEAD_H = 54;
var FOOT_H = 20;
var SB_W = 10;
var data = null;
var hits = [];
var first = 0;     // primeira faixa visível (rolagem)
var lay = null;    // geometria da última pintura, para rolar com a barra
var arrasto = -1;  // y do clique na barra de rolagem (-1 = sem arrasto)

function arr(x) {
	if (!x) return [];
	return (x instanceof Array) ? x : [x];
}

function load() {
	var d = new Dict(DICT);
	try { data = JSON.parse(d.stringify()); } catch (e) { data = null; }
	if (data && !data.tracks) data = null;
}

function save() {
	var d = new Dict(DICT);
	d.parse(JSON.stringify(data));
}

function bang() {
	load();
	mgraphics.redraw();
}

function loadbang() { bang(); }

function rgb(r, g, b, a) { mgraphics.set_source_rgba(r, g, b, a == null ? 1 : a); }

function texto(s, x, y, size, bold) {
	mgraphics.select_font_face(bold ? "Arial Bold" : "Arial");
	mgraphics.set_font_size(size);
	mgraphics.move_to(x, y);
	mgraphics.show_text(String(s));
}

// Nomes dos stems costumam ter o mesmo começo ("mel rose - neural collapse ..."): se não couber, corta o MEIO
// e preserva o fim, que é o que diferencia uma faixa da outra.
function cabe(s, w, size) {
	s = String(s);
	mgraphics.select_font_face("Arial Bold");
	mgraphics.set_font_size(size);
	if (mgraphics.text_measure(s)[0] <= w) return s;
	var ini = s.substring(0, 4), fim = s.substring(4);
	while (fim.length > 1 && mgraphics.text_measure(ini + "…" + fim)[0] > w) fim = fim.substring(1);
	return ini + "…" + fim;
}

function rolar(n) {
	if (!lay) return;
	var novo = Math.max(0, Math.min(lay.maxFirst, Math.round(n)));
	if (novo != first) { first = novo; mgraphics.redraw(); }
}

function paint() {
	var W = box.rect[2] - box.rect[0];
	var H = box.rect[3] - box.rect[1];
	hits = [];

	rgb(0.10, 0.10, 0.11); mgraphics.rectangle(0, 0, W, H); mgraphics.fill();
	rgb(0.92, 0.92, 0.92); texto("VASSOURA", 14, 24, 16, true);
	rgb(0.55, 0.55, 0.58); texto("PROTÓTIPO 0.1.1 · SÓ DETECÇÃO (nada é alterado no set)", 112, 23, 10, false);

	if (!data) {
		rgb(0.6, 0.6, 0.6); texto("Clique ANALYZE no device para ler as faixas.", 14, 80, 12, false);
		return;
	}

	var pct = data.clipBeats > 0 ? Math.round(100 * data.emptyBeats / data.clipBeats) : 0;
	rgb(0.85, 0.85, 0.85);
	texto(data.nEmpty + " VAZIOS · " + pct + "% DO TEMPO DOS CLIPS · THRESHOLD " + data.threshold.toFixed(1) +
		" dB · MIN LENGTH " + Math.round(data.minlen) + " ms", 14, 44, 10, true);

	var tracks = arr(data.tracks);
	var maxBeat = data.maxBeat > 0 ? data.maxBeat : 1;
	var x0 = NAME_W, x1 = W - SB_W - 8;
	var bx = function (b) { return x0 + (x1 - x0) * b / maxBeat; };
	// faixas: encolhe a altura para caber todas; abaixo de 10 px rola (barra à direita / roda do mouse)
	var top = HEAD_H + 12, listH = H - top - FOOT_H;
	var laneH = Math.max(10, Math.min(28, Math.floor(listH / Math.max(1, tracks.length))));
	var vis = Math.max(1, Math.floor(listH / laneH));
	var maxFirst = Math.max(0, tracks.length - vis);
	if (first > maxFirst) first = maxFirst;
	var fs = Math.max(8, Math.min(10, laneH - 3));
	lay = { top: top, listH: listH, vis: vis, n: tracks.length, maxFirst: maxFirst, W: W };

	// régua em compassos
	var sig = data.sig || 4;
	var bars = maxBeat / sig;
	var step = Math.max(1, Math.pow(2, Math.ceil(Math.log(bars / 16) / Math.LN2)));
	for (var bar = 0; bar <= bars; bar += step) {
		var x = bx(bar * sig);
		rgb(0.25, 0.25, 0.27); mgraphics.move_to(x, HEAD_H + 8); mgraphics.line_to(x, top + listH); mgraphics.set_line_width(1); mgraphics.stroke();
		rgb(0.5, 0.5, 0.52); texto(bar + 1, x + 2, HEAD_H + 6, 9, false);
	}

	var last = Math.min(tracks.length, first + vis);
	for (var i = first; i < last; i++) {
		var tr = tracks[i];
		var y = top + (i - first) * laneH;
		var off = tr.skip ? 1 : 0;

		// caixinha + nome
		var cb = Math.min(10, laneH - 2);
		rgb(0.6, 0.6, 0.62); mgraphics.rectangle(10, y + laneH / 2 - cb / 2, cb, cb); mgraphics.set_line_width(1); mgraphics.stroke();
		if (!off) { rgb(0.35, 0.8, 0.45); mgraphics.rectangle(12, y + laneH / 2 - cb / 2 + 2, cb - 4, cb - 4); mgraphics.fill(); }
		rgb(off ? 0.45 : 0.88, off ? 0.45 : 0.88, off ? 0.45 : 0.88);
		texto(cabe(tr.name, NAME_W - 34, fs), 26, y + laneH / 2 + fs / 2 - 1, fs, true);
		hits.push({ x: 0, y: y, w: NAME_W, h: laneH, tipo: "faixa", t: i });

		var clips = arr(tr.clips);
		for (var j = 0; j < clips.length; j++) {
			var c = clips[j];
			var cx = bx(c.s), cw = Math.max(1, bx(c.e) - cx);
			if (c.state == "ok") rgb(0.22, 0.55, 0.32, off ? 0.35 : 0.9);
			else rgb(0.35, 0.35, 0.38, 0.8);
			mgraphics.rectangle(cx, y + 2, cw, laneH - 4); mgraphics.fill();
			if (c.state != "ok") {
				rgb(0.9, 0.75, 0.3); texto(String(c.state).toUpperCase(), cx + 4, y + laneH / 2 + fs / 2 - 1, fs, false);
			}
			var em = arr(c.empties);
			for (var k = 0; k < em.length; k++) {
				var r = em[k];
				var rx = bx(r.s), rw = Math.max(1, bx(r.e) - rx);
				if (r.keep || off) rgb(0.45, 0.45, 0.48, 0.9);
				else rgb(0.85, 0.22, 0.22, 0.85);
				mgraphics.rectangle(rx, y + 2, rw, laneH - 4); mgraphics.fill();
				// tracejado
				rgb(0, 0, 0, 0.25); mgraphics.set_line_width(1);
				for (var hx = rx - laneH; hx < rx + rw; hx += 6) {
					var a = Math.max(hx, rx), b = Math.min(hx + laneH - 4, rx + rw);
					if (b <= a) continue;
					mgraphics.move_to(a, y + laneH - 2 - (a - hx)); mgraphics.line_to(b, y + laneH - 2 - (b - hx)); mgraphics.stroke();
				}
				hits.push({ x: rx, y: y, w: rw, h: laneH, tipo: "vazio", t: i, c: j, k: k });
			}
		}
	}

	// barra de rolagem (só quando não cabem todas)
	var sx = W - SB_W - 2;
	if (maxFirst > 0) {
		rgb(0.18, 0.18, 0.2); mgraphics.rectangle(sx, top, SB_W, listH); mgraphics.fill();
		var th = Math.max(24, listH * vis / tracks.length);
		var ty = top + (listH - th) * first / maxFirst;
		rgb(0.55, 0.55, 0.58); mgraphics.rectangle(sx + 1, ty, SB_W - 2, th); mgraphics.fill();
	}

	rgb(0.55, 0.55, 0.58);
	texto("FAIXAS " + (tracks.length ? first + 1 : 0) + "–" + last + " DE " + tracks.length +
		(maxFirst > 0 ? " · role pela barra à direita ou pela roda do mouse" : ""), 14, H - 6, 9, false);
}

function onclick(x, y) {
	if (!data) return;
	arrasto = -1;
	if (lay && lay.maxFirst > 0 && x >= lay.W - SB_W - 4) {
		arrasto = y;
		rolar(lay.maxFirst * (y - lay.top) / Math.max(1, lay.listH));
		return;
	}
	for (var i = hits.length - 1; i >= 0; i--) {
		var h = hits[i];
		if (x < h.x || x > h.x + h.w || y < h.y || y > h.y + h.h) continue;
		var tracks = arr(data.tracks);
		data.tracks = tracks;
		var tr = tracks[h.t];
		if (h.tipo == "faixa") {
			tr.skip = tr.skip ? 0 : 1;
		} else {
			tr.clips = arr(tr.clips);
			var c = tr.clips[h.c];
			c.empties = arr(c.empties);
			c.empties[h.k].keep = c.empties[h.k].keep ? 0 : 1;
		}
		save();
		mgraphics.redraw();
		return;
	}
}
onclick.local = 1;

function ondrag(x, y, but) {
	if (arrasto < 0 || !lay) return;
	if (!but) { arrasto = -1; return; }
	rolar(lay.maxFirst * (y - lay.top) / Math.max(1, lay.listH));
}
ondrag.local = 1;

// roda do mouse / trackpad (jsui do Max 8.1+)
function onwheel(x, y, wx, wy) {
	if (!lay || lay.maxFirst <= 0) return;
	rolar(first - (wy > 0 ? 3 : wy < 0 ? -3 : 0));
}
onwheel.local = 1;

function onresize() { mgraphics.redraw(); }
