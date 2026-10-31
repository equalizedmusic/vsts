// VASSOURA — janela flutuante (jsui). Desenha o dict "vassoura_data".
// Cor da faixa = clip com som (fica) · vermelho escuro tracejado = vazio (sai) · cinza = vazio marcado para FICAR.
// Clique no nome da faixa: deixa a faixa fora da limpeza. Clique no vermelho: mantém o trecho.

mgraphics.init();
mgraphics.relative_coords = 0;
mgraphics.autofill = 0;

var DICT = "vassoura_data";
var NAME_W = 250;
var HEAD_H = 74;
var RULER_H = 24;
var FOOT_H = 30;
var SB_W = 14;
var LANE_MIN = 20, LANE_MAX = 34;
var data = null;
var hits = [];
var first = 0;     // primeira faixa visível (rolagem)
var lay = null;    // geometria da última pintura, para rolar com a barra
var arrasto = -1;  // y do clique na barra de rolagem (-1 = sem arrasto)
var hover = -1;    // faixa sob o mouse
var prefixo = "";  // começo comum dos nomes, mostrado uma vez no cabeçalho

function arr(x) {
	if (!x) return [];
	return (x instanceof Array) ? x : [x];
}

function load() {
	var d = new Dict(DICT);
	try { data = JSON.parse(d.stringify()); } catch (e) { data = null; }
	if (data && !data.tracks) data = null;
	prefixo = data ? comum(arr(data.tracks)) : "";
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

// Stems do mesmo projeto costumam ter o mesmo começo ("1-mel rose - neural collapse inteira - 140 Fm 1 ref"):
// tira o número da frente e o trecho comum, que vai uma vez só para o cabeçalho.
function semNumero(s) { return String(s).replace(/^\d+\s*[-_.]\s*/, ""); }

function comum(tracks) {
	if (tracks.length < 2) return "";
	var p = semNumero(tracks[0].name);
	for (var i = 1; i < tracks.length && p.length; i++) {
		var s = semNumero(tracks[i].name), k = 0;
		while (k < p.length && k < s.length && p.charAt(k) == s.charAt(k)) k++;
		p = p.substring(0, k);
	}
	// corta no último separador para não partir palavra
	var m = Math.max(p.lastIndexOf(" "), p.lastIndexOf("-"), p.lastIndexOf("_"));
	p = m >= 0 ? p.substring(0, m + 1) : "";
	return p.length >= 6 ? p : "";
}

function curto(tr) {
	var s = semNumero(tr.name);
	if (prefixo && s.indexOf(prefixo) == 0) s = s.substring(prefixo.length);
	s = s.replace(/^\s+/, "");
	return s.length ? s : tr.name;
}

function rgb(r, g, b, a) { mgraphics.set_source_rgba(r, g, b, a == null ? 1 : a); }

function fonte(size, bold) {
	mgraphics.select_font_face(bold ? "Arial Bold" : "Arial");
	mgraphics.set_font_size(size);
}

function texto(s, x, y, size, bold) {
	fonte(size, bold);
	mgraphics.move_to(x, y);
	mgraphics.show_text(String(s));
}

function largura(s, size, bold) {
	fonte(size, bold);
	return mgraphics.text_measure(String(s))[0];
}

// se não couber, corta o MEIO e preserva o fim, que é o que diferencia uma faixa da outra
function cabe(s, w, size, bold) {
	s = String(s);
	if (largura(s, size, bold) <= w) return s;
	var ini = s.substring(0, 6), fim = s.substring(6);
	while (fim.length > 1 && largura(ini + "…" + fim, size, bold) > w) fim = fim.substring(1);
	return ini + "…" + fim;
}

// cor da faixa no Live (inteiro 0xRRGGBB); sem cor = verde
function corFaixa(tr) {
	var c = tr.color | 0;
	if (c <= 0) return [0.30, 0.72, 0.42];
	return [((c >> 16) & 255) / 255, ((c >> 8) & 255) / 255, (c & 255) / 255];
}

function tracejado(x, y, w, h, gap) {
	for (var hx = x - h; hx < x + w; hx += gap) {
		var a = Math.max(hx, x), b = Math.min(hx + h, x + w);
		if (b <= a) continue;
		mgraphics.move_to(a, y + h - (a - hx)); mgraphics.line_to(b, y + h - (b - hx)); mgraphics.stroke();
	}
}

function caixa(x, y, w, h, r, g, b, a) { rgb(r, g, b, a); mgraphics.rectangle(x, y, w, h); mgraphics.fill(); }

// corta o FIM (para o começo comum dos stems, que se lê da esquerda)
function cabeFim(s, w, size) {
	s = String(s);
	if (largura(s, size, false) <= w) return s;
	while (s.length > 1 && largura(s + "…", size, false) > w) s = s.substring(0, s.length - 1);
	return s + "…";
}

function rolar(n) {
	if (!lay) return;
	var novo = Math.max(0, Math.min(lay.maxFirst, Math.round(n)));
	if (novo != first) { first = novo; mgraphics.redraw(); }
}

function resumo(tr) {
	var tot = 0, vaz = 0, n = 0, clips = arr(tr.clips);
	for (var j = 0; j < clips.length; j++) {
		var c = clips[j];
		if (c.state != "ok") continue;
		tot += c.e - c.s;
		var em = arr(c.empties);
		for (var k = 0; k < em.length; k++) { vaz += em[k].e - em[k].s; n++; }
	}
	return { n: n, pct: tot > 0 ? Math.round(100 * vaz / tot) : 0 };
}

function paint() {
	var W = box.rect[2] - box.rect[0];
	var H = box.rect[3] - box.rect[1];
	hits = [];

	caixa(0, 0, W, H, 0.09, 0.09, 0.10);
	rgb(0.95, 0.95, 0.95); texto("VASSOURA", 16, 30, 22, true);
	rgb(0.55, 0.55, 0.58); texto("PROTÓTIPO 0.1.2 · SÓ DETECÇÃO — nada é alterado no set", 16 + largura("VASSOURA", 22, true) + 14, 29, 12, false);

	if (!data) {
		rgb(0.7, 0.7, 0.72); texto("Clique ANALYZE no device para ler as faixas.", 16, 110, 16, false);
		return;
	}

	var tracks = arr(data.tracks);
	var pct = data.clipBeats > 0 ? Math.round(100 * data.emptyBeats / data.clipBeats) : 0;
	rgb(0.92, 0.92, 0.92);
	texto(data.nEmpty + " VAZIOS EM " + tracks.length + " FAIXAS · " + pct + "% DO TEMPO DOS CLIPS", 16, 56, 15, true);
	rgb(0.60, 0.60, 0.63);
	var lx = 16 + largura(data.nEmpty + " VAZIOS EM " + tracks.length + " FAIXAS · " + pct + "% DO TEMPO DOS CLIPS", 15, true) + 18;
	texto("THRESHOLD " + data.threshold.toFixed(1) + " dB · MIN LENGTH " + (data.minlen / 1000).toFixed(2) + " s", lx, 56, 12, false);

	// legenda
	var gx = W - 400;
	caixa(gx, 45, 6, 14, 0.30, 0.72, 0.42); caixa(gx + 5, 45, 6, 14, 0.55, 0.45, 0.95); caixa(gx + 10, 45, 6, 14, 0.95, 0.60, 0.25);
	rgb(0.8, 0.8, 0.82); texto("SOM, cor da faixa (fica)", gx + 22, 57, 12, false);
	caixa(gx + 180, 45, 16, 14, 0.36, 0.09, 0.10); rgb(0.95, 0.35, 0.35, 0.8); mgraphics.set_line_width(1.5); tracejado(gx + 180, 45, 16, 14, 5);
	rgb(0.8, 0.8, 0.82); texto("VAZIO (sai)", gx + 202, 57, 12, false);
	caixa(gx + 295, 45, 16, 14, 0.30, 0.30, 0.33); rgb(0.8, 0.8, 0.82); texto("MANTIDO", gx + 317, 57, 12, false);

	// escala só no trecho onde há clips (do compasso do 1º clip até o fim do último)
	var sig = data.sig || 4;
	var minBeat = -1;
	for (var i = 0; i < tracks.length; i++) {
		var cl = arr(tracks[i].clips);
		for (var j = 0; j < cl.length; j++) if (minBeat < 0 || cl[j].s < minBeat) minBeat = cl[j].s;
	}
	if (minBeat < 0) minBeat = 0;
	minBeat = Math.floor(minBeat / sig) * sig;
	var maxBeat = data.maxBeat > minBeat ? data.maxBeat : minBeat + sig;
	var x0 = NAME_W + 8, x1 = W - SB_W - 10;
	var bx = function (b) { return x0 + (x1 - x0) * (b - minBeat) / (maxBeat - minBeat); };

	var top = HEAD_H + RULER_H + 4, listH = H - top - FOOT_H;
	var laneH = Math.max(LANE_MIN, Math.min(LANE_MAX, Math.floor(listH / Math.max(1, tracks.length))));
	var vis = Math.max(1, Math.floor(listH / laneH));
	var maxFirst = Math.max(0, tracks.length - vis);
	if (first > maxFirst) first = maxFirst;
	var fs = Math.max(11, Math.min(14, laneH - 8));
	lay = { top: top, listH: listH, vis: vis, n: tracks.length, maxFirst: maxFirst, W: W, laneH: laneH };

	// régua em compassos
	caixa(0, HEAD_H, W, RULER_H, 0.14, 0.14, 0.15);
	if (prefixo) { rgb(0.55, 0.55, 0.58); texto(cabeFim("STEMS: " + prefixo, NAME_W - 16, 11), 16, HEAD_H + 16, 11, false); }
	var bars = (maxBeat - minBeat) / sig;
	var step = Math.max(1, Math.pow(2, Math.ceil(Math.log(bars / 20) / Math.LN2)));
	var last = Math.min(tracks.length, first + vis);
	var fundo = top + (last - first) * laneH;
	for (var bar = 0; bar <= bars; bar += step) {
		var x = bx(minBeat + bar * sig);
		rgb(0.24, 0.24, 0.26); mgraphics.set_line_width(1); mgraphics.move_to(x, HEAD_H + RULER_H); mgraphics.line_to(x, fundo); mgraphics.stroke();
		rgb(0.75, 0.75, 0.78); texto(minBeat / sig + bar + 1, x + 3, HEAD_H + 17, 12, true);
	}

	for (var i = first; i < last; i++) {
		var tr = tracks[i];
		var y = top + (i - first) * laneH;
		var off = tr.skip ? 1 : 0;
		var cor = corFaixa(tr);
		var rs = resumo(tr);

		// fundo da linha (zebrado; destaque sob o mouse)
		if (i == hover) caixa(0, y, W - SB_W - 4, laneH, 0.20, 0.20, 0.23);
		else if (i % 2) caixa(0, y, W - SB_W - 4, laneH, 0.12, 0.12, 0.13);

		// tarja da cor da faixa + caixinha + nome + % vazio
		caixa(6, y + 2, 5, laneH - 4, cor[0], cor[1], cor[2], off ? 0.35 : 1);
		var cb = Math.min(14, laneH - 6), cy = y + (laneH - cb) / 2;
		rgb(0.65, 0.65, 0.68); mgraphics.rectangle(18, cy, cb, cb); mgraphics.set_line_width(1.2); mgraphics.stroke();
		if (!off) caixa(21, cy + 3, cb - 6, cb - 6, 0.40, 0.85, 0.50);
		var ptxt = rs.pct + "%";
		var pw = largura(ptxt, fs - 1, false);
		rgb(off ? 0.40 : 0.60, off ? 0.40 : 0.60, off ? 0.42 : 0.63);
		texto(ptxt, NAME_W - 6 - pw, y + laneH / 2 + (fs - 1) / 2 - 1, fs - 1, false);
		var cinza = off ? 0.42 : 0.93;
		rgb(cinza, cinza, cinza);
		texto(cabe(curto(tr), NAME_W - 18 - cb - 14 - pw, fs, true), 18 + cb + 8, y + laneH / 2 + fs / 2 - 1, fs, true);
		hits.push({ x: 0, y: y, w: NAME_W, h: laneH, tipo: "faixa", t: i });

		var clips = arr(tr.clips);
		var by = y + 2, bh = laneH - 4;
		for (var j = 0; j < clips.length; j++) {
			var c = clips[j];
			var cx = bx(c.s), cw = Math.max(1, bx(c.e) - cx);
			if (c.state == "ok") caixa(cx, by, cw, bh, cor[0], cor[1], cor[2], off ? 0.30 : 1);
			else caixa(cx, by, cw, bh, 0.32, 0.32, 0.35, 0.9);
			if (c.state != "ok") {
				rgb(0.95, 0.80, 0.35); texto(String(c.state).toUpperCase(), cx + 5, y + laneH / 2 + fs / 2 - 1, fs - 1, true);
			}
			var em = arr(c.empties);
			for (var k = 0; k < em.length; k++) {
				var r = em[k];
				var rx = bx(r.s), rw = Math.max(1, bx(r.e) - rx);
				if (r.keep || off) {
					caixa(rx, by, rw, bh, 0.30, 0.30, 0.33, off ? 0.6 : 1);
				} else {
					caixa(rx, by, rw, bh, 0.36, 0.09, 0.10);
					rgb(0.95, 0.35, 0.35, 0.55); mgraphics.set_line_width(1.2); tracejado(rx, by, rw, bh, 7);
				}
				hits.push({ x: rx, y: y, w: rw, h: laneH, tipo: "vazio", t: i, c: j, k: k });
			}
			// borda do clip, para ver onde cada clip começa e termina
			rgb(0, 0, 0, 0.6); mgraphics.set_line_width(1); mgraphics.rectangle(cx + 0.5, by + 0.5, cw - 1, bh - 1); mgraphics.stroke();
		}
	}

	// barra de rolagem (só quando não cabem todas)
	var sx = W - SB_W - 2;
	if (maxFirst > 0) {
		caixa(sx, top, SB_W, listH, 0.16, 0.16, 0.18);
		var th = Math.max(30, listH * vis / tracks.length);
		var ty = top + (listH - th) * first / maxFirst;
		caixa(sx + 2, ty, SB_W - 4, th, 0.55, 0.55, 0.58);
	}

	// rodapé: faixa sob o mouse (nome inteiro) ou posição da rolagem
	caixa(0, H - FOOT_H, W, FOOT_H, 0.13, 0.13, 0.14);
	if (hover >= 0 && hover < tracks.length) {
		var hr = resumo(tracks[hover]);
		rgb(0.92, 0.92, 0.92);
		texto(cabe(tracks[hover].name, W - 360, 13, true), 16, H - 10, 13, true);
		rgb(0.65, 0.65, 0.68);
		texto(hr.n + " vazios · " + hr.pct + "% vazio" + (tracks[hover].skip ? " · FORA DA LIMPEZA" : ""), W - 330, H - 10, 13, false);
	} else {
		rgb(0.65, 0.65, 0.68);
		texto("FAIXAS " + (tracks.length ? first + 1 : 0) + "–" + last + " DE " + tracks.length +
			(maxFirst > 0 ? " · role pela barra à direita ou pela roda do mouse" : "") +
			" · passe o mouse numa faixa para ver o nome inteiro", 16, H - 10, 12, false);
	}
}

function faixaEm(x, y) {
	if (!lay || y < lay.top || y >= lay.top + lay.listH || x > lay.W - SB_W - 4) return -1;
	var i = first + Math.floor((y - lay.top) / lay.laneH);
	return i < Math.min(lay.n, first + lay.vis) ? i : -1;
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

// mouse passando por cima (sem clicar): destaca a linha e mostra o nome inteiro no rodapé
function onidle(x, y) {
	var h = faixaEm(x, y);
	if (h != hover) { hover = h; mgraphics.redraw(); }
}
onidle.local = 1;

function onidleout() {
	if (hover != -1) { hover = -1; mgraphics.redraw(); }
}
onidleout.local = 1;

// roda do mouse / trackpad (jsui do Max 8.1+)
function onwheel(x, y, wx, wy) {
	if (!lay || lay.maxFirst <= 0) return;
	rolar(first - (wy > 0 ? 3 : wy < 0 ? -3 : 0));
	hover = faixaEm(x, y);
}
onwheel.local = 1;

function onresize() { mgraphics.redraw(); }
