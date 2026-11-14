// Faixa do device no Live (jsui 420 × 166): arte do produto + botão OPEN THE MAGIC + estado (licença, status).
// Os controles ficam todos na janela. O build-<device>.js cola antes deste arquivo: var DEV = { nome, cor, tipo, frase, versao }.
// Entradas: status <texto> · lic <0|1> · nova <versão>. Saída: "open" (abre a janela).

mgraphics.init();
mgraphics.relative_coords = 0;
mgraphics.autofill = 0;
outlets = 1;

var msgStatus = "", licOk = -1, msgNova = "", hoverBt = 0, apertado = 0;
var bt = null;
var t0 = new Date().getTime();
var tBrilho = new Task(function () { if (hoverBt) mgraphics.redraw(); else tBrilho.cancel(); });
tBrilho.interval = 50;

function status() { msgStatus = arrayfromargs(arguments).join(" "); mgraphics.redraw(); }
function lic(v) { licOk = v ? 1 : 0; mgraphics.redraw(); }
function nova(v) { msgNova = String(v || ""); mgraphics.redraw(); }
function bang() { mgraphics.redraw(); }

function rgb(r, g, b, a) { mgraphics.set_source_rgba(r, g, b, a == null ? 1 : a); }
function rgbv(c, a) { rgb(c[0], c[1], c[2], a); }
function fonte(size, bold) { mgraphics.select_font_face(bold ? "Arial Bold" : "Arial"); mgraphics.set_font_size(size); }
function largura(s, size, bold) { fonte(size, bold); return mgraphics.text_measure(String(s))[0]; }
function texto(s, x, y, size, bold) { fonte(size, bold); mgraphics.move_to(x, y); mgraphics.show_text(String(s)); }
function textoC(s, xc, y, size, bold) { texto(s, xc - largura(s, size, bold) / 2, y, size, bold); }
function caixa(x, y, w, h, c, a) { rgbv(c, a); mgraphics.rectangle(x, y, w, h); mgraphics.fill(); }
function caixaR(x, y, w, h, r, c, a) { rgbv(c, a); mgraphics.rectangle_rounded(x, y, w, h, r, r); mgraphics.fill(); }
function cabeFim(s, w, size, bold) {
	s = String(s);
	if (largura(s, size, bold) <= w) return s;
	while (s.length > 1 && largura(s + "…", size, bold) > w) s = s.substring(0, s.length - 1);
	return s + "…";
}

// ondas estilizadas (fixas: semente própria) — "antes" altas e desiguais, "depois" niveladas/limpas
function ondas(x0, x1, cy, h, nivelado, cor, seed) {
	var s = seed;
	function r2() { s = (s * 16807) % 2147483647; return s / 2147483647; }
	mgraphics.set_line_width(2);
	for (var x = x0; x < x1; x += 4) {
		var bloco = Math.floor((x - x0) / 46);
		var vazio = DEV.tipo == "vassoura" && (bloco % 3 == 1);
		var a = r2();
		var amp = (0.35 + 0.65 * a) * (0.45 + 0.55 * Math.abs(Math.sin(bloco * 1.7 + seed)));
		if (nivelado) amp = DEV.tipo == "rodo" ? 0.55 + 0.25 * a : amp;
		if (vazio) { if (nivelado) continue; amp = 0.05 * a; }
		var hh = Math.max(1, amp * h);
		rgbv(cor, nivelado ? 0.95 : 0.45);
		mgraphics.move_to(x, cy - hh); mgraphics.line_to(x, cy + hh); mgraphics.stroke();
	}
}

function desenhaRodo(x, top, bot) {
	var mid = (top + bot) / 2;
	try { mgraphics.set_line_cap("round"); } catch (e) { }
	rgb(0, 0, 0, 0.35); mgraphics.set_line_width(9); mgraphics.move_to(x - 4, mid + 5); mgraphics.line_to(x - 124, mid - 13); mgraphics.stroke();
	rgb(0.54, 0.35, 0.17); mgraphics.set_line_width(8); mgraphics.move_to(x - 8, mid); mgraphics.line_to(x - 128, mid - 18); mgraphics.stroke();
	rgb(0.72, 0.50, 0.27); mgraphics.set_line_width(3); mgraphics.move_to(x - 9, mid - 3); mgraphics.line_to(x - 126, mid - 21); mgraphics.stroke();
	try { mgraphics.set_line_cap("butt"); } catch (e) { }
	caixaR(x - 20, mid - 14, 18, 28, 4, [0.85, 0.55, 0.07]);
	caixaR(x - 9, top, 10, bot - top, 3, DEV.cor);
	caixaR(x + 1, top + 2, 5, bot - top - 4, 2, [0.07, 0.07, 0.08]);
	for (var k = 0; k < 14; k++) { var yy = top + 6 + k * (bot - top - 12) / 13; rgb(0.61, 0.85, 1, 0.7); mgraphics.ellipse(x + 9 + (k % 3) * 3, yy, 3, 3); mgraphics.fill(); }
}

function desenhaVassoura(x, top, bot) {
	var mid = (top + bot) / 2;
	try { mgraphics.set_line_cap("round"); } catch (e) { }
	rgb(0, 0, 0, 0.35); mgraphics.set_line_width(9); mgraphics.move_to(x - 10, mid + 5); mgraphics.line_to(x - 130, mid - 13); mgraphics.stroke();
	rgb(0.54, 0.35, 0.17); mgraphics.set_line_width(8); mgraphics.move_to(x - 14, mid); mgraphics.line_to(x - 134, mid - 18); mgraphics.stroke();
	rgb(0.72, 0.50, 0.27); mgraphics.set_line_width(3); mgraphics.move_to(x - 15, mid - 3); mgraphics.line_to(x - 132, mid - 21); mgraphics.stroke();
	try { mgraphics.set_line_cap("butt"); } catch (e) { }
	// base (madeira) + cerdas
	caixaR(x - 18, top, 12, bot - top, 3, [0.62, 0.42, 0.20]);
	caixaR(x - 24, mid - 10, 10, 20, 3, DEV.cor);
	for (var k = 0; k <= (bot - top) / 2; k++) {
		var yy = top + 1 + k * 2;
		rgb(0.93, 0.80, 0.42, 0.9 - (k % 3) * 0.15); mgraphics.set_line_width(1);
		mgraphics.move_to(x - 6, yy); mgraphics.line_to(x + 8 + (k % 4), yy + 0.5); mgraphics.stroke();
	}
	for (var p = 0; p < 18; p++) { var py = top + 4 + ((p * 37) % (bot - top - 8)); rgb(0.62, 0.56, 0.50, 0.55); mgraphics.ellipse(x + 13 + (p % 4) * 3, py, 2.5, 2.5); mgraphics.fill(); }
}

function paint() {
	var W = box.rect[2] - box.rect[0], H = box.rect[3] - box.rect[1];
	var AC = DEV.cor;
	// fundo
	caixa(0, 0, W, H, [0.075, 0.078, 0.085]);
	for (var g = 0; g < 10; g++) caixa(0, g * H / 10, W, H / 10 + 1, AC, 0.012 * (10 - g) * 0.5);

	// palco: 3 "faixas" — à esquerda como era, à direita como fica; o rodo/vassoura no meio
	var sx0 = 8, sx1 = W - 8, sy = 46, lh = 17, xm = 168;
	caixaR(sx0, sy - 4, sx1 - sx0, lh * 3 + 8, 5, [0.11, 0.115, 0.125]);
	var cores = [AC, [0.48, 0.65, 1], [0.55, 0.85, 0.55]];
	for (var i = 0; i < 3; i++) {
		var cy = sy + i * lh + lh / 2;
		ondas(sx0 + 6, xm, cy, lh / 2 - 2, 0, cores[i], 11 + i * 7);
		ondas(xm + 14, sx1 - 6, cy, lh / 2 - 2, 1, cores[i], 11 + i * 7);
	}
	if (DEV.tipo == "rodo") {
		rgbv(AC, 0.5); mgraphics.set_line_width(1);
		for (var xd = xm + 14; xd < sx1 - 6; xd += 7) { mgraphics.move_to(xd, sy + 1.5); mgraphics.line_to(xd + 4, sy + 1.5); }
		mgraphics.stroke();
		desenhaRodo(xm, sy - 6, sy + lh * 3 + 4);
	} else desenhaVassoura(xm, sy - 6, sy + lh * 3 + 4);

	// nome + frase
	// selo de licença (primeiro: a frase ocupa o que sobrar)
	var selo = licOk == 1 ? "LICENSED" : licOk == 0 ? "NOT ACTIVATED" : "";
	if (msgNova) selo = "NEW " + msgNova;
	var xSelo = W - 8;
	if (selo) {
		var ws = largura(selo, 9, true) + 12, cs = msgNova ? [0.25, 0.65, 0.40] : licOk == 1 ? [0.30, 0.62, 0.40] : [0.85, 0.55, 0.15];
		xSelo = W - 8 - ws;
		caixaR(xSelo, 12, ws, 16, 8, cs, 0.25);
		rgbv(cs); textoC(selo, xSelo + ws / 2, 23, 9, true);
	}
	rgbv(AC); texto(DEV.nome, 10, 30, 24, true);
	var nx = 10 + largura(DEV.nome, 24, true) + 8;
	rgb(0.55, 0.55, 0.58); texto(DEV.versao, nx, 29, 10, false);
	var fx = nx + largura(DEV.versao, 10, false) + 10;
	rgb(0.78, 0.78, 0.80); texto(cabeFim(DEV.frase, xSelo - 10 - fx, 11, false), fx, 29, 11, false);

	// botão
	var bw = 240, bh = 30, bx = (W - bw) / 2, by = sy + lh * 3 + 12;
	bt = { x: bx, y: by, w: bw, h: bh };
	var pulso = hoverBt ? 0.5 + 0.5 * Math.sin((new Date().getTime() - t0) / 160) : 0;
	caixaR(bx - 5, by - 5, bw + 10, bh + 10, 10, AC, 0.10 + 0.12 * pulso);
	caixaR(bx - 2, by - 2, bw + 4, bh + 4, 8, AC, 0.25 + 0.15 * pulso);
	caixaR(bx, by + (apertado ? 1 : 0), bw, bh, 6, AC);
	caixaR(bx + 2, by + 2 + (apertado ? 1 : 0), bw - 4, bh / 2 - 2, 5, [1, 1, 1], 0.14);
	rgb(0.07, 0.07, 0.08); textoC("OPEN THE MAGIC  ✦", bx + bw / 2, by + bh / 2 + 5 + (apertado ? 1 : 0), 14, true);

	// status
	if (msgStatus) { rgb(0.62, 0.62, 0.65); textoC(cabeFim(msgStatus, W - 20, 10, false), W / 2, H - 6, 10, false); }
}

function dentro(x, y) { return bt && x >= bt.x && x <= bt.x + bt.w && y >= bt.y && y <= bt.y + bt.h; }
function onclick(x, y) { if (dentro(x, y)) { apertado = 1; mgraphics.redraw(); outlet(0, "open"); } }
onclick.local = 1;
function ondrag(x, y, but) { if (!but && apertado) { apertado = 0; mgraphics.redraw(); } }
ondrag.local = 1;
function onidle(x, y) {
	var h = dentro(x, y) ? 1 : 0;
	if (h != hoverBt) { hoverBt = h; mgraphics.redraw(); if (h) tBrilho.repeat(); }
	if (apertado) { apertado = 0; mgraphics.redraw(); }
}
onidle.local = 1;
function onidleout() { if (hoverBt || apertado) { hoverBt = 0; apertado = 0; mgraphics.redraw(); } }
onidleout.local = 1;
