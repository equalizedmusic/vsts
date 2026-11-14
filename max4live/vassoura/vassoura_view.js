// VASSOURA — janela flutuante (jsui). Desenha o dict "vassoura_data".
// Cor da faixa = clip com som (fica) · vermelho escuro tracejado = vazio (sai) · cinza = vazio marcado para FICAR.
// Clique no nome da faixa: deixa a faixa fora da limpeza. Clique no vermelho: mantém o trecho.
// Barra de cima: MODE / SNAP / SCOPE clicáveis, ANALYZE e APPLY (mandam para o motor pela saída do jsui).

mgraphics.init();
mgraphics.relative_coords = 0;
mgraphics.autofill = 0;
outlets = 1;

var DICT = "vassoura_data";
var VERSAO = "0.3.0";
var msgNova = "";     // versão nova disponível (vem do motor)
var NAME_W = 250;
var HEAD_H = 150;
var AC = [0.40, 0.78, 0.95];   // cor da marca da VASSOURA na janela (botões, licença)
var anim = null;      // APPLY: { total, feitos, frac, motorFim, fimT, t }
var pendente = 0;     // chegou dado novo durante a animação: carrega quando a vassoura terminar
var tAnim = new Task(passoAnim);
tAnim.interval = 33;
var arrastaSl = null; // slider sendo arrastado
var cfg = { threshold: -60, minlen: 1000, pre: 10, post: 100, snap: 0, mode: 0, scope: 0 };
var msgStatus = "";   // o que o motor está fazendo agora
var msgAviso = "";    // resultado do último APPLY (fica até o próximo)
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
	if (data && data.cfg) cfg = data.cfg;
	prefixo = data ? comum(arr(data.tracks)) : "";
}

function status() {
	msgStatus = arrayfromargs(arguments).join(" ");
	mgraphics.redraw();
}

function aviso() {
	msgAviso = arrayfromargs(arguments).join(" ");
	mgraphics.redraw();
}

function nova(v) {
	msgNova = String(v || "");
	mgraphics.redraw();
}

function save() {
	var d = new Dict(DICT);
	d.parse(JSON.stringify(data));
}

function bang() {
	if (anim) { pendente = 1; return; }
	load();
	mgraphics.redraw();
}

// valores atuais do motor (a janela pode abrir antes do 1º ANALYZE)
function valores(th, ml, pr, po, sn, mo, sc) {
	if (arrastaSl) return;
	cfg = { threshold: Number(th), minlen: Number(ml), pre: Number(pr), post: Number(po), snap: sn | 0, mode: mo | 0, scope: sc | 0 };
	if (!anim) mgraphics.redraw();
}

// APPLY: a vassoura atravessa as faixas acompanhando o motor (nunca na frente dele)
function varre(total) { msgAviso = ""; anim = { total: total, feitos: 0, frac: 0, motorFim: 0, fimT: 0, t: 0 }; tAnim.repeat(); }
function progresso(i, total) { if (anim) { anim.feitos = i; anim.total = total; } }
function fim() { if (anim) anim.motorFim = 1; }
function passoAnim() {
	if (!anim) { tAnim.cancel(); return; }
	anim.t += 0.033;
	if (anim.fimT > 0) {
		if (anim.t - anim.fimT > 2.0) {
			tAnim.cancel(); anim = null;
			if (pendente) load();
			pendente = 0;
		}
		mgraphics.redraw();
		return;
	}
	var alvo = anim.motorFim ? 1.08 : Math.min(1, anim.total ? anim.feitos / anim.total : 0);
	var falta = alvo - anim.frac;
	if (falta > 0) anim.frac = Math.min(alvo, anim.frac + Math.max(1 / 6, Math.min(1 / 1.6, falta * 3)) * 0.033);
	if (anim.motorFim && anim.frac >= 1.08 - 1e-9) anim.fimT = anim.t;
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

// barra de controles: grupos de opções clicáveis + valores dos dials do device + ANALYZE / APPLY
function botao(x, y, w, h, txt, fundo, frente, cmd) {
	caixa(x, y, w, h, fundo[0], fundo[1], fundo[2]);
	rgb(frente[0], frente[1], frente[2]);
	texto(txt, x + (w - largura(txt, 12, true)) / 2, y + h / 2 + 4, 12, true);
	if (cmd) hits.push({ x: x, y: y, w: w, h: h, tipo: "cmd", msg: cmd });
}

function grupo(x, y, h, rotulo, opcoes, val, cmd) {
	rgb(0.55, 0.55, 0.58); texto(rotulo, x, y + h / 2 + 4, 11, true);
	x += largura(rotulo, 11, true) + 8;
	for (var i = 0; i < opcoes.length; i++) {
		var w = largura(opcoes[i], 12, true) + 20;
		var on = i == val;
		botao(x, y, w, h, opcoes[i], on ? [0.86, 0.86, 0.88] : [0.19, 0.19, 0.21], on ? [0.08, 0.08, 0.09] : [0.70, 0.70, 0.73], [cmd, i]);
		x += w + 2;
	}
	return x + 20;
}

function barra(W) {
	var y = 70, h = 28;
	caixa(0, y - 6, W, h + 12 + 48, 0.12, 0.12, 0.13);
	var x = 16;
	x = grupo(x, y, h, "MODE", ["MUTE", "CUT"], cfg.mode, "ui_mode");
	x = grupo(x, y, h, "SNAP", ["OFF", "1/16", "BAR"], cfg.snap, "ui_snap");
	x = grupo(x, y, h, "SCOPE", ["ALL TRACKS", "SELECTED"], cfg.scope, "ui_scope");

	var wAp = 170, wAn = 120, xAp = W - 16 - wAp, xAn = xAp - 8 - wAn;
	var feito = anim && anim.fimT > 0, varrendo = anim && !feito;
	botao(xAn, y, wAn, h, "ANALYZE", [0.30, 0.30, 0.34], [0.95, 0.95, 0.95], anim ? null : ["analyze"]);
	var cut = cfg.mode == 1;
	var tAp = feito ? "APPLIED ✓" : varrendo ? "SWEEPING…" : "APPLY · " + (cut ? "CUT" : "MUTE");
	botao(xAp, y, wAp, h, tAp, feito ? [0.25, 0.60, 0.38] : cut ? [0.72, 0.20, 0.20] : [0.20, 0.42, 0.72], [1, 1, 1], anim ? null : ["apply"]);

	// 2ª linha: THRESHOLD, MIN LENGTH, PRE, POST (arrastar; valem na hora, sem reler os arquivos)
	var y2 = y + h + 12, ws = Math.floor((W - 32 - 3 * 24) / 4);
	slider(16, y2, ws, "THRESHOLD", cfg.threshold, -90, -20, 1, function (v) { return v.toFixed(1) + " dB"; }, "ui_threshold", "threshold");
	slider(16 + (ws + 24), y2, ws, "MIN LENGTH", cfg.minlen, 100, 10000, 2, function (v) { return (v / 1000).toFixed(2) + " s"; }, "ui_minlength", "minlen");
	slider(16 + 2 * (ws + 24), y2, ws, "PRE", cfg.pre, 0, 1000, 2, function (v) { return Math.round(v) + " ms"; }, "ui_pre", "pre");
	slider(16 + 3 * (ws + 24), y2, ws, "POST", cfg.post, 0, 2000, 2, function (v) { return Math.round(v) + " ms"; }, "ui_post", "post");
}

// slider horizontal: rótulo + valor em cima, trilho embaixo. exp > 1 = mais resolução no começo da faixa
function slider(x, y, w, rot, v, mn, mx, exp, fmt, cmd, chave) {
	rgb(0.55, 0.55, 0.58); texto(rot, x, y + 10, 11, true);
	var tv = fmt(v);
	rgb(AC[0], AC[1], AC[2]); texto(tv, x + w - largura(tv, 13, true), y + 11, 13, true);
	var ty = y + 22, pos = Math.pow(Math.max(0, Math.min(1, (v - mn) / (mx - mn))), 1 / exp);
	rgb(0.20, 0.21, 0.23); mgraphics.rectangle_rounded(x, ty, w, 6, 3, 3); mgraphics.fill();
	rgb(AC[0], AC[1], AC[2]); mgraphics.rectangle_rounded(x, ty, Math.max(6, pos * w), 6, 3, 3); mgraphics.fill();
	rgb(0.93, 0.93, 0.93); mgraphics.ellipse(x + pos * w - 7, ty - 4, 14, 14); mgraphics.fill();
	hits.push({ x: x - 8, y: y, w: w + 16, h: 36, tipo: "slider", x0: x, x1: x + w, mn: mn, mx: mx, exp: exp, cmd: cmd, chave: chave });
}
var ultimoEnvio = 0;
function mexeSlider(px, s, final) {
	var p = Math.max(0, Math.min(1, (px - s.x0) / (s.x1 - s.x0)));
	var v = s.mn + Math.pow(p, s.exp) * (s.mx - s.mn);
	v = s.chave == "threshold" ? Math.round(v * 10) / 10 : Math.round(v);
	if (v != cfg[s.chave]) { cfg[s.chave] = v; mgraphics.redraw(); }
	var t = new Date().getTime();
	if (final || t - ultimoEnvio > 80) { ultimoEnvio = t; outlet(0, s.cmd, v); }
}

// vassoura vista de cima: base com cerdas cobrindo as faixas, cabo para trás, poeira sendo empurrada na frente
function desenhaVassoura(x, top, bot, t) {
	var mid = (top + bot) / 2;
	var s = 53;
	function r2() { s = (s * 16807) % 2147483647; return s / 2147483647; }
	for (var k = 0; k < 90; k++) {
		var yy = top + 6 + r2() * (bot - top - 12), dx = 16 + r2() * 26 + Math.sin(t * 8 + k) * 2, rr = 0.8 + r2() * 2.2;
		rgb(0.66, 0.60, 0.52, 0.25 + r2() * 0.5); mgraphics.ellipse(x + dx - rr, yy - rr, rr * 2, rr * 2); mgraphics.fill();
	}
	// fiapos dos trechos vazios (o "lixo" vermelho) indo junto
	for (var q = 0; q < 26; q++) {
		var y2 = top + 10 + r2() * (bot - top - 20), x2 = x + 18 + r2() * 20;
		rgb(0.85, 0.30, 0.30, 0.55); mgraphics.set_line_width(1.5);
		mgraphics.move_to(x2, y2); mgraphics.line_to(x2 + 4 + r2() * 5, y2 + (r2() - 0.5) * 6); mgraphics.stroke();
	}
	var hx = x - 18, hy = mid, ex = x - 340, ey = mid - 150;
	try { mgraphics.set_line_cap("round"); } catch (e) { }
	rgb(0, 0, 0, 0.35); mgraphics.set_line_width(16); mgraphics.move_to(hx + 6, hy + 8); mgraphics.line_to(ex + 6, ey + 8); mgraphics.stroke();
	rgb(0.54, 0.35, 0.17); mgraphics.set_line_width(14); mgraphics.move_to(hx, hy); mgraphics.line_to(ex, ey); mgraphics.stroke();
	rgb(0.69, 0.48, 0.26); mgraphics.set_line_width(5); mgraphics.move_to(hx - 2, hy - 4); mgraphics.line_to(ex + 2, ey - 4); mgraphics.stroke();
	rgb(0.16, 0.17, 0.18); mgraphics.set_line_width(16); mgraphics.move_to(ex + 36, ey + 16); mgraphics.line_to(ex, ey); mgraphics.stroke();
	try { mgraphics.set_line_cap("butt"); } catch (e) { }
	// sombra
	rgb(0, 0, 0, 0.3); mgraphics.rectangle_rounded(x - 22, top + 8, 40, bot - top, 6, 6); mgraphics.fill();
	// base de madeira
	rgb(0.58, 0.39, 0.19); mgraphics.rectangle_rounded(x - 30, top, 18, bot - top, 5, 5); mgraphics.fill();
	rgb(0.72, 0.52, 0.28); mgraphics.rectangle(x - 27, top + 4, 3, bot - top - 8); mgraphics.fill();
	// encaixe do cabo
	rgb(AC[0], AC[1], AC[2]); mgraphics.rectangle_rounded(x - 40, mid - 24, 20, 48, 6, 6); mgraphics.fill();
	// cerdas
	mgraphics.set_line_width(1.2);
	for (var b = top + 2; b < bot - 2; b += 2.2) {
		var dobra = Math.sin(b * 0.7 + t * 14) * 1.5;
		rgb(0.93, 0.80, 0.42, 0.75 + 0.25 * Math.sin(b)); mgraphics.move_to(x - 12, b); mgraphics.line_to(x + 12 + dobra, b + 1.5); mgraphics.stroke();
	}
	mgraphics.save();
	mgraphics.translate(x - 21, mid);
	mgraphics.rotate(-Math.PI / 2);
	rgb(0.08, 0.08, 0.09); texto("VASSOURA", -largura("VASSOURA", 10, true) / 2, 4, 10, true);
	mgraphics.restore();
}

function paint() {
	var W = box.rect[2] - box.rect[0];
	var H = box.rect[3] - box.rect[1];
	hits = [];

	caixa(0, 0, W, H, 0.09, 0.09, 0.10);
	rgb(0.95, 0.95, 0.95); texto("VASSOURA", 16, 30, 22, true);
	var tx = 16 + largura("VASSOURA", 22, true) + 12;
	rgb(0.55, 0.55, 0.58); texto(VERSAO, tx, 29, 12, false);
	tx += largura(VERSAO, 12, false) + 18;
	var fimStatus = W - 20;
	if (licPronta()) {
		// LICENSE (informações / desativar) e, se houver, versão nova: BAIXAR abre o link fixo do zip
		var tL = "LICENSE", wL = largura(tL, 12, true) + 24;
		botao(W - 16 - wL, 12, wL, 26, tL, [0.19, 0.19, 0.21], [0.85, 0.85, 0.88], ["lic_info"]);
		fimStatus = W - 16 - wL - 8;
		if (msgNova) {
			var tNova = "NEW VERSION " + msgNova + " · DOWNLOAD", wNova = largura(tNova, 12, true) + 24;
			botao(fimStatus - wNova, 12, wNova, 26, tNova, [0.18, 0.55, 0.30], [1, 1, 1], ["baixar_nova"]);
			fimStatus -= wNova + 16;
		}
	}
	if (msgStatus) { rgb(0.98, 0.78, 0.35); texto(cabeFim(msgStatus, fimStatus - tx, 13), tx, 29, 13, true); }
	if (!licPronta() || licInfo) { licPinta(W, H, 44, "VASSOURA", AC, hits); return; }

	barra(W);

	if (!data) {
		rgb(0.7, 0.7, 0.72); texto("Clique ANALYZE para ler as faixas.", 16, HEAD_H + 40, 16, false);
		return;
	}

	var tracks = arr(data.tracks);
	var pct = data.clipBeats > 0 ? Math.round(100 * data.emptyBeats / data.clipBeats) : 0;
	rgb(0.92, 0.92, 0.92);
	texto(data.nEmpty + " VAZIOS EM " + tracks.length + " FAIXAS · " + pct + "% DO TEMPO DOS CLIPS", 16, 56, 15, true);
	if (msgAviso) {
		rgb(0.55, 0.85, 0.60);
		var ax = 16 + largura(data.nEmpty + " VAZIOS EM " + tracks.length + " FAIXAS · " + pct + "% DO TEMPO DOS CLIPS", 15, true) + 18;
		texto(cabeFim("ÚLTIMO APPLY: " + msgAviso, W - 420 - ax, 12), ax, 56, 12, false);
	}

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
	// posição da vassoura (APPLY): atrás dela os vazios já aparecem limpos
	var vx = anim ? (anim.fimT > 0 ? 1e9 : x0 - 40 + (x1 - x0 + 80) * anim.frac) : -1e9;

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
			else if (c.state == "muted") caixa(cx, by, cw, bh, 0.20, 0.20, 0.22, 1);  // já mutado (ex.: por um APPLY)
			else caixa(cx, by, cw, bh, 0.32, 0.32, 0.35, 0.9);
			if (c.state != "ok" && c.state != "muted") {
				var et = String(c.state).toUpperCase();
				if (largura(et, fs - 1, true) + 10 < cw) { rgb(0.95, 0.80, 0.35); texto(et, cx + 5, y + laneH / 2 + fs / 2 - 1, fs - 1, true); }
			}
			var em = arr(c.empties);
			for (var k = 0; k < em.length; k++) {
				var r = em[k];
				var rx = bx(r.s), rw = Math.max(1, bx(r.e) - rx);
				if (!r.keep && !off && rx + rw <= vx) {
					// já varrido: MUTE fica apagado, CUT some
					if (cfg.mode == 1) caixa(rx, by, rw, bh, 0.09, 0.09, 0.10); else caixa(rx, by, rw, bh, 0.20, 0.20, 0.22);
				} else if (r.keep || off) {
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

	// vassoura passando (APPLY)
	if (anim && anim.fimT == 0 && vx > x0 - 60) desenhaVassoura(vx, top - 8, fundo + 6, anim.t);

	// rodapé: faixa sob o mouse (nome inteiro) ou posição da rolagem
	caixa(0, H - FOOT_H, W, FOOT_H, 0.13, 0.13, 0.14);
	if (anim) {
		rgb(AC[0], AC[1], AC[2]);
		texto(anim.fimT > 0 ? "APPLIED ✓ · " + (cfg.mode == 1 ? "CUT" : "MUTE") + " FEITO" : "SWEEPING… " + Math.min(anim.feitos, anim.total) + " de " + anim.total + " trechos", 16, H - 10, 13, true);
	} else if (hover >= 0 && hover < tracks.length) {
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
	arrasto = -1; arrastaSl = null;
	for (var b = hits.length - 1; b >= 0; b--) {
		var hb = hits[b];
		if (hb.tipo == "slider" && !anim && x >= hb.x && x <= hb.x + hb.w && y >= hb.y && y <= hb.y + hb.h) { arrastaSl = hb; mexeSlider(x, hb, 0); return; }
		if (hb.tipo != "cmd" || x < hb.x || x > hb.x + hb.w || y < hb.y || y > hb.y + hb.h) continue;
		var m = hb.msg;
		if (licCliqueLocal(m)) return;
		if (m[0] == "ui_mode") cfg.mode = m[1];
		else if (m[0] == "ui_snap") cfg.snap = m[1];
		else if (m[0] == "ui_scope") cfg.scope = m[1];
		outlet(0, m);
		mgraphics.redraw();
		return;
	}
	if (!data || anim || !licPronta() || licInfo) return;
	if (lay && lay.maxFirst > 0 && x >= lay.W - SB_W - 4) {
		arrasto = y;
		rolar(lay.maxFirst * (y - lay.top) / Math.max(1, lay.listH));
		return;
	}
	for (var i = hits.length - 1; i >= 0; i--) {
		var h = hits[i];
		if ((h.tipo != "faixa" && h.tipo != "vazio") || x < h.x || x > h.x + h.w || y < h.y || y > h.y + h.h) continue;
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
		outlet(0, "marcas");  // motor reconta os totais
		return;
	}
}
onclick.local = 1;

function ondrag(x, y, but) {
	if (arrastaSl) {
		if (!but) { mexeSlider(x, arrastaSl, 1); arrastaSl = null; return; }
		mexeSlider(x, arrastaSl, 0);
		return;
	}
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
