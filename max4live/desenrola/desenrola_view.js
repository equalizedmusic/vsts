// DESENROLA — janela flutuante (jsui). Desenha "desenrola_data": as partes lidas da referência (linha do tempo) e as
// faixas do loop com o que cada uma toca em cada tipo de parte (grade) + a prévia do rascunho alinhada às partes.
// Partes: clique = troca o tipo · arrastar a borda direita = tamanho (de 4 em 4; Shift = de 1 em 1) ·
//         Shift+clique = tira a parte · duplo clique = divide em duas · + PART põe uma no fim.
// Faixas: clique no grupo = troca o grupo (volta ao padrão do grupo) · clique na célula = liga/desliga a faixa naquele
//         tipo de parte · clique na prévia = liga/desliga naquele tipo também.
// ANALYZE, BUILD, UNDO e LOCATORS vão para o motor.

mgraphics.init();
mgraphics.relative_coords = 0;
mgraphics.autofill = 0;
outlets = 1;

var DICT = "desenrola_data";
var VERSAO = "0.1.0";
var msgNova = "";
var HEAD_H = 72, BAR_H = 86, TL_Y = 172, TL_H = 64, MAT_Y = 290, TOP = 318, FOOT_H = 40, SB_W = 14;
var XN = 16, XG = 236, XC = 330, CW = 50, X0 = 650;
var LANE_MIN = 18, LANE_MAX = 26;
var AC = [0.98, 0.62, 0.20], OK = [0.37, 0.75, 0.48], WARN = [0.88, 0.33, 0.31], CINZA = [0.55, 0.55, 0.58];
var PARTES = ["INTRO", "GROOVE", "BUILD", "DROP", "BREAK", "OUTRO"];
var CORES_PARTE = [0x6fa8dc, 0x93c47d, 0xf6b26b, 0xe06666, 0x8e7cc3, 0x76a5af];
var GRUPOS = ["KICK", "BASS", "DRUMS", "HATS", "SYNTHS", "AMBIENCE", "VOCAL", "FX", "OTHER"];
var CORES = [0xff8a3d, 0x45a6ff, 0xf5c451, 0xe6e1f5, 0xc9b0ff, 0x4fd1c5, 0xff7fb5, 0xc4e86b, 0x9a9aa0];
var cfg = { loc: 1 };
var data = null;
var msgStatus = "", msgAviso = "";
var hits = [];
var first = 0, lay = null, arrasto = -1, hover = -1, hoverParte = -1;
var bordaArr = null;   // { i, x0, n0, shift } arrastando a borda de uma parte
var prog = null;       // { feitos, total, fim }

function arr(x) { if (x == null) return []; return (x instanceof Array) ? x : [x]; }
function load() {
	var d = new Dict(DICT);
	try { data = JSON.parse(d.stringify()); } catch (e) { data = null; }
	if (data && !data.tracks) data = null;
	if (data) {
		data.tracks = arr(data.tracks);
		data.partes = arr(data.partes);
		if (data.ref) data.ref.partes = arr(data.ref.partes);
		for (var i = 0; i < data.tracks.length; i++) {
			var t = data.tracks[i];
			t.clips = arr(t.clips);
			t.on = arr(t.on);
			t.g = t.g == null ? 8 : Number(t.g);
		}
		if (data.cfg) cfg = data.cfg;
	}
}
function save() { var d = new Dict(DICT); d.parse(JSON.stringify(data)); }
function mudou() { save(); outlet(0, "marcas"); mgraphics.redraw(); }

// mensagens do motor
function bang() { load(); mgraphics.redraw(); }
function loadbang() { load(); mgraphics.redraw(); }
function status() { msgStatus = arrayfromargs(arguments).join(" "); mgraphics.redraw(); }
function aviso() { msgAviso = arrayfromargs(arguments).join(" "); prog = null; mgraphics.redraw(); }
function varre(total) { msgAviso = ""; prog = { feitos: 0, total: total }; mgraphics.redraw(); }
function progresso(i, total) { if (!prog) prog = {}; prog.feitos = i; prog.total = total; mgraphics.redraw(); }
function fim() { prog = null; mgraphics.redraw(); }
function valores(l) { cfg = { loc: l | 0 }; mgraphics.redraw(); }
function nova(v) { msgNova = String(v || ""); mgraphics.redraw(); }

// ---------------------------------------------------------------- desenho (helpers)
function rgb(r, g, b, a) { mgraphics.set_source_rgba(r, g, b, a == null ? 1 : a); }
function rgbv(c, a) { rgb(c[0], c[1], c[2], a); }
function fonte(size, bold) { mgraphics.select_font_face(bold ? "Arial Bold" : "Arial"); mgraphics.set_font_size(size); }
function largura(s, size, bold) { fonte(size, bold); return mgraphics.text_measure(String(s))[0]; }
function texto(s, x, y, size, bold) { fonte(size, bold); mgraphics.move_to(x, y); mgraphics.show_text(String(s)); }
function textoD(s, xr, y, size, bold) { texto(s, xr - largura(s, size, bold), y, size, bold); }
function textoC(s, xc, y, size, bold) { texto(s, xc - largura(s, size, bold) / 2, y, size, bold); }
function caixa(x, y, w, h, r, g, b, a) { rgb(r, g, b, a); mgraphics.rectangle(x, y, w, h); mgraphics.fill(); }
function caixaR(x, y, w, h, raio, c, a) { rgbv(c, a); mgraphics.rectangle_rounded(x, y, w, h, raio, raio); mgraphics.fill(); }
function contorno(x, y, w, h, raio, c, a, lw) { rgbv(c, a); mgraphics.set_line_width(lw || 1); mgraphics.rectangle_rounded(x + 0.5, y + 0.5, w - 1, h - 1, raio, raio); mgraphics.stroke(); }
function cabe(s, w, size, bold) {
	s = String(s);
	if (largura(s, size, bold) <= w) return s;
	var ini = s.substring(0, 6), fim = s.substring(6);
	while (fim.length > 1 && largura(ini + "…" + fim, size, bold) > w) fim = fim.substring(1);
	return ini + "…" + fim;
}
function cabeFim(s, w, size, bold) {
	s = String(s);
	if (largura(s, size, bold) <= w) return s;
	while (s.length > 1 && largura(s + "…", size, bold) > w) s = s.substring(0, s.length - 1);
	return s + "…";
}
function corInt(c) { c = c | 0; if (c <= 0) return [0.48, 0.65, 1]; return [((c >> 16) & 255) / 255, ((c >> 8) & 255) / 255, (c & 255) / 255]; }
function corG(g) { return corInt(CORES[g < 0 ? 8 : g]); }
function corP(p) { var i = PARTES.indexOf(p); return corInt(CORES_PARTE[i < 0 ? 0 : i]); }
function escura(c) { return 0.299 * c[0] + 0.587 * c[1] + 0.114 * c[2] > 0.6; }
function tinta(c) { if (escura(c)) rgb(0.07, 0.07, 0.08); else rgb(0.98, 0.98, 0.98); }

function botao(x, y, w, h, txt, on, cor, cmd, brilho) {
	if (brilho) { caixaR(x - 4, y - 4, w + 8, h + 8, 7, cor, 0.18); caixaR(x - 2, y - 2, w + 4, h + 4, 5, cor, 0.25); }
	caixaR(x, y, w, h, 3, on ? cor : [0.23, 0.23, 0.25]);
	if (on) rgb(0.07, 0.07, 0.08); else rgb(0.91, 0.91, 0.91);
	textoC(txt, x + w / 2, y + h / 2 + 4, 12, true);
	if (cmd) hits.push({ x: x, y: y, w: w, h: h, tipo: "cmd", msg: cmd });
}
function botaoP(x, y, w, h, txt, cmd, cor) {
	cor = cor || CINZA;
	contorno(x, y, w, h, 5, cor, 0.9);
	rgbv(cor); textoC(txt, x + w / 2, y + h / 2 + 4, 11, true);
	hits.push({ x: x, y: y, w: w, h: h, tipo: "cmd", msg: cmd });
}

function total() { var n = 0; if (data) for (var i = 0; i < data.partes.length; i++) n += data.partes[i].n; return n; }
// posição x de um compasso do rascunho (0 = início do rascunho)
function geo(W) {
	var x1 = W - SB_W - 10, tot = Math.max(1, total());
	return { x1: x1, k: (x1 - X0) / tot };
}

// ---------------------------------------------------------------- pintura
function paint() {
	var W = box.rect[2] - box.rect[0], H = box.rect[3] - box.rect[1];
	hits = [];
	caixa(0, 0, W, H, 0.12, 0.12, 0.13);
	caixa(0, 0, W, HEAD_H, 0.08, 0.09, 0.09);
	rgbv(AC); texto("DESENROLA", 16, 33, 26, true);
	var tx = 16 + largura("DESENROLA", 26, true) + 10;
	rgb(0.55, 0.55, 0.58); texto(VERSAO, tx, 31, 12, false);
	tx += largura(VERSAO, 12, false) + 18;
	var fimSt = W - 20;
	if (msgNova) {
		var tn = "NEW VERSION " + msgNova + " · DOWNLOAD", wn = largura(tn, 11, true) + 22;
		botaoP(fimSt - wn, 14, wn, 24, tn, ["baixar_nova"], OK);
		fimSt -= wn + 10;
	}
	if (msgStatus) { rgb(0.91, 0.91, 0.91); texto(cabeFim(msgStatus, fimSt - tx, 13, true), tx, 31, 13, true); }
	rgb(0.55, 0.55, 0.58); texto("loop → música: lê a estrutura da referência e espalha os clips do seu loop pelo Arrangement", 16, 58, 12, false);
	if (!licPronta() || licInfo) { licPinta(W, H, HEAD_H, "DESENROLA", AC, hits); return; }

	barra(W);
	if (!data) { vazio(W, H); rodape(W, H); return; }
	linhaTempo(W);
	matriz(W, H);
	rodape(W, H);
}

function barra(W) {
	caixa(0, HEAD_H, W, BAR_H, 0.10, 0.11, 0.12);
	caixa(0, HEAD_H + BAR_H - 1, W, 1, 0.22, 0.23, 0.25);
	var y = HEAD_H + 26;
	if (data && data.ref) {
		var r = data.ref;
		rgb(0.55, 0.55, 0.58); texto("REFERENCE", 16, y + 4, 11, true);
		rgb(0.91, 0.91, 0.91); var tr = cabeFim(r.clip || r.nome, 300, 13, true); texto(tr, 100, y + 4, 13, true);
		var x = 100 + largura(tr, 13, true) + 14;
		rgbv(AC); var tb = (Math.round(r.bpm * 100) / 100) + " BPM" + (r.warp ? " (warp)" : " (pelo áudio)") + " · " + r.compassos + " compassos";
		texto(tb, x, y + 4, 12, true);
		x += largura(tb, 12, true) + 30;
		rgb(0.55, 0.55, 0.58); texto("LOOP", x, y + 4, 11, true);
		rgb(0.91, 0.91, 0.91); texto(Math.round(data.loop.bars * 100) / 100 + " compassos · " + data.tracks.length + " faixas" + (data.fora ? " · " + data.fora + (data.fora > 1 ? " clips cruzam" : " clip cruza") + " a borda (fica de fora)" : ""), x + 40, y + 4, 12, true);
	} else { rgb(0.55, 0.55, 0.58); texto("REFERENCE  —", 16, y + 4, 11, true); }

	y = HEAD_H + 62;
	var x2 = 16;
	rgb(0.55, 0.55, 0.58); texto("LOCATORS", x2, y + 4, 11, true); x2 += 70;
	botao(x2, y - 13, 44, 26, "ON", cfg.loc == 1, AC, ["ui_loc", 1]); x2 += 46;
	botao(x2, y - 13, 44, 26, "OFF", cfg.loc == 0, CINZA, ["ui_loc", 0]); x2 += 64;
	if (data && !prog) { botaoP(x2, y - 12, 84, 24, "+ PART", ["__mais"]); x2 += 96; botaoP(x2, y - 12, 110, 24, "RESET PARTS", ["__reset"]); }
	var wU = 90, wB = 150, wA = 110, xU = W - 18 - wU, xB = xU - 8 - wB, xA = xB - 8 - wA;
	var feito = data && data.feito;
	botao(xA, y - 15, wA, 30, "ANALYZE", false, AC, prog ? null : ["analyze"]);
	botao(xB, y - 15, wB, 30, prog ? "DESENROLANDO…" : feito ? "BUILT ✓" : "BUILD", !!data, feito ? OK : AC, prog || !data ? null : ["build"], !!prog);
	botao(xU, y - 15, wU, 30, "UNDO", false, AC, prog ? null : ["undo"]);
}

function vazio(W, H) {
	var y = TL_Y + 10;
	rgb(0.91, 0.91, 0.91); texto("Como usar", 16, y, 18, true);
	var passos = [
		"1.  No Arrangement, deixe o seu loop com todas as faixas (kick, bass, drums, synths…) e marque o loop brace em volta dele.",
		"2.  Crie uma faixa de áudio chamada REF e arraste a música de referência para ela (fora do loop). Warp ligado ajuda no andamento.",
		"3.  Clique ANALYZE: o DESENROLA lê as partes da referência (INTRO, BUILD, DROP, BREAK…) e as faixas do loop.",
		"4.  Confira as partes e o que toca em cada uma. Clique BUILD: o rascunho aparece depois do loop, com locators.",
		"5.  Não gostou? UNDO apaga o rascunho. O loop original nunca é mexido."
	];
	for (var i = 0; i < passos.length; i++) { rgb(0.7, 0.7, 0.72); texto(passos[i], 16, y + 34 + i * 26, 14, false); }
}

function linhaTempo(W) {
	var G = geo(W), x = X0, y = TL_Y;
	rgb(0.55, 0.55, 0.58); texto("PARTES DO RASCUNHO", 16, y + 18, 11, true);
	rgb(0.45, 0.45, 0.48);
	texto("clique = tipo · arraste a borda = tamanho", 16, y + 36, 11, false);
	texto("Shift+clique = tira · duplo clique = divide", 16, y + 52, 11, false);
	var resumo = data.partes.length + " partes · " + total() + " compassos";
	rgb(0.91, 0.91, 0.91); texto(resumo, 330, y + 18, 13, true);
	if (data.inicio != null && data.bar) { rgb(0.55, 0.55, 0.58); texto("começa no compasso " + (Math.round(data.inicio / data.bar) + 1) + ", depois do loop", 330, y + 36, 11, false); }
	var acum = 0;
	for (var i = 0; i < data.partes.length; i++) {
		var p = data.partes[i], w = p.n * G.k, c = corP(p.p);
		var hov = i == hoverParte && !prog;
		caixaR(x + 1, y, Math.max(2, w - 2), TL_H, 4, c, hov ? 1 : 0.85);
		if (w > 30) {
			tinta(c);
			texto(cabeFim(p.p, w - 10, 12, true), x + 6, y + 18, 12, true);
			texto(p.n + "", x + 6, y + 36, 11, false);
			if (p.a != null && p.b != null && w > 60) { rgb(0, 0, 0, 0.45); texto(cabeFim("ref " + (p.a + 1) + "–" + p.b, w - 10, 9, false), x + 6, y + 54, 9, false); }
		}
		hits.push({ x: x, y: y, w: w, h: TL_H, tipo: "parte", i: i });
		// borda direita (arrastar = tamanho)
		hits.push({ x: x + w - 5, y: y, w: 10, h: TL_H, tipo: "borda", i: i, xb: x + w });
		if (hov) caixa(x + w - 3, y + 8, 2, TL_H - 16, 1, 1, 1, 0.8);
		x += w; acum += p.n;
	}
	// régua de compassos
	var step = 8; while (total() / step > 24) step *= 2;
	for (var b = 0; b <= total(); b += step) {
		var xr = X0 + b * G.k;
		caixa(xr, y + TL_H + 2, 1, 6, 0.35, 0.35, 0.38);
		rgb(0.5, 0.5, 0.53); texto(b + 1, xr + 2, y + TL_H + 16, 9, false);
	}
	lay = lay || {};
	lay.G = G;
}

function matriz(W, H) {
	var G = geo(W), tracks = data.tracks;
	var y = MAT_Y;
	caixa(0, y - 8, W, 1, 0.22, 0.23, 0.25);
	rgb(0.55, 0.55, 0.58); texto("TRACK", XN + 14, y + 14, 10, true); texto("GROUP", XG, y + 14, 10, true);
	for (var p = 0; p < PARTES.length; p++) {
		var cp = corP(PARTES[p]);
		caixaR(XC + p * CW + 2, y, CW - 4, 20, 3, cp, 0.9);
		tinta(cp); textoC(PARTES[p].substring(0, 5), XC + p * CW + CW / 2, y + 14, 9, true);
	}
	rgb(0.55, 0.55, 0.58); texto("PRÉVIA DO RASCUNHO", X0, y + 14, 10, true);

	var listH = H - TOP - FOOT_H;
	var laneH = Math.max(LANE_MIN, Math.min(LANE_MAX, Math.floor(listH / Math.max(1, tracks.length))));
	var vis = Math.max(1, Math.floor(listH / laneH));
	var maxFirst = Math.max(0, tracks.length - vis);
	if (first > maxFirst) first = maxFirst;
	var last = Math.min(tracks.length, first + vis);
	var fundo = TOP + (last - first) * laneH;
	lay = { listH: listH, vis: vis, n: tracks.length, maxFirst: maxFirst, W: W, laneH: laneH, G: G };
	var fs = Math.max(11, Math.min(13, laneH - 7));

	// colunas das partes na prévia
	var xx = X0;
	for (var q = 0; q < data.partes.length; q++) { var wq = data.partes[q].n * G.k; caixa(xx, TOP, 1, fundo - TOP, 0.22, 0.22, 0.25); xx += wq; }
	// progresso do BUILD: cortina na prévia
	for (var i = first; i < last; i++) {
		var tr = tracks[i];
		var yl = TOP + (i - first) * laneH, cy = yl + laneH / 2;
		if (i == hover && !prog) caixa(0, yl, W - SB_W - 4, laneH - 1, 0.19, 0.19, 0.22);
		else caixa(0, yl, X0 - 8, laneH - 1, i % 2 ? 0.15 : 0.135, i % 2 ? 0.15 : 0.137, i % 2 ? 0.17 : 0.15);
		var ct = corInt(tr.color);
		caixa(6, yl + 3, 4, laneH - 7, ct[0], ct[1], ct[2]);
		rgb(0.91, 0.91, 0.91);
		texto(cabe((tr.midi ? "♪ " : "") + tr.name, XG - XN - 24, fs - 1, true), XN + 4, cy + fs / 2 - 2, fs - 1, true);
		// grupo
		var cg = corG(tr.g), wc = 82;
		caixaR(XG, cy - 9, wc, 18, 4, cg); tinta(cg);
		textoC(GRUPOS[tr.g], XG + wc / 2, cy + 4, 10, true);
		hits.push({ x: XG, y: cy - 9, w: wc, h: 18, tipo: "grupo", t: i });
		// células
		for (var k = 0; k < PARTES.length; k++) {
			var on = tr.on[k] == 1, cx = XC + k * CW + 8, cw = CW - 16, cpk = corP(PARTES[k]);
			if (on) caixaR(cx, cy - 7, cw, 14, 3, cpk);
			else contorno(cx, cy - 7, cw, 14, 3, [0.4, 0.4, 0.43], 1, 1);
			hits.push({ x: XC + k * CW, y: yl, w: CW, h: laneH, tipo: "cel", t: i, k: k });
		}
		// prévia: blocos onde a faixa toca
		var x0 = X0;
		for (var m = 0; m < data.partes.length; m++) {
			var pm = data.partes[m], wm = pm.n * G.k, kk = PARTES.indexOf(pm.p);
			if (tr.on[kk] == 1) {
				caixa(x0 + 1, yl + 3, Math.max(1, wm - 2), laneH - 7, ct[0], ct[1], ct[2], 0.55);
				caixa(x0 + 1, yl + 3, Math.max(1, wm - 2), 2, ct[0], ct[1], ct[2], 1);
			}
			hits.push({ x: x0, y: yl, w: wm, h: laneH, tipo: "cel", t: i, k: kk });
			x0 += wm;
		}
	}
	if (prog && prog.total) {
		var xp = X0 + (G.x1 - X0) * Math.min(1, prog.feitos / prog.total);
		caixa(xp, TOP, G.x1 - xp, fundo - TOP, 0.12, 0.12, 0.13, 0.75);
		caixa(xp - 1, TOP - 4, 3, fundo - TOP + 8, AC[0], AC[1], AC[2], 1);
	}
	if (maxFirst > 0) {
		var sx = W - SB_W - 2;
		caixa(sx, TOP, SB_W, listH, 0.16, 0.16, 0.18);
		var tth = Math.max(30, listH * vis / tracks.length);
		caixa(sx + 2, TOP + (listH - tth) * first / maxFirst, SB_W - 4, tth, 0.55, 0.55, 0.58);
	}
}

function rodape(W, H) {
	caixa(0, H - FOOT_H, W, FOOT_H, 0.08, 0.09, 0.09);
	var y = H - 15;
	if (prog) { rgbv(AC); texto("BUILD… " + (prog.feitos || 0) + " de " + (prog.total || 0) + " cópias de clip", 16, y, 13, true); return; }
	if (msgAviso && hover < 0) { rgbv(OK); texto(cabeFim(msgAviso, W - 32, 13, true), 16, y, 13, true); return; }
	if (!data) return;
	if (hover >= 0 && hover < data.tracks.length) {
		var tr = data.tracks[hover];
		rgb(0.91, 0.91, 0.91); texto(cabe(tr.name, W - 560, 13, true), 16, y, 13, true);
		var toca = [];
		for (var k = 0; k < PARTES.length; k++) if (tr.on[k] == 1) toca.push(PARTES[k]);
		rgb(0.62, 0.62, 0.65);
		textoD((tr.midi ? "MIDI · " : "áudio · ") + tr.clips.length + " clips no loop · grupo " + GRUPOS[tr.g] + (tr.auto ? " (pelo nome)" : "") + " · toca em: " + (toca.length ? toca.join(", ") : "nenhuma"), W - 16, y, 12, false);
		return;
	}
	rgb(0.75, 0.75, 0.78);
	texto("grupo: clique troca (o que toca volta ao padrão do grupo) · célula ou prévia: liga/desliga a faixa naquele tipo de parte", 16, y, 12, false);
	if (lay && lay.maxFirst > 0) { rgb(0.55, 0.55, 0.58); textoD("FAIXAS " + (first + 1) + "–" + Math.min(data.tracks.length, first + lay.vis) + " DE " + data.tracks.length, W - 16, y, 12, false); }
}

// ---------------------------------------------------------------- edição das partes
var REGRAS = {
	INTRO:  [1, 0, 1, 1, 0, 1, 0, 0, 0],
	GROOVE: [1, 0, 1, 1, 1, 0, 1, 0, 1],
	BUILD:  [0, 0, 1, 1, 1, 1, 1, 1, 0],
	DROP:   [1, 1, 1, 1, 1, 0, 1, 0, 1],
	BREAK:  [0, 0, 0, 0, 1, 1, 1, 0, 1],
	OUTRO:  [1, 0, 1, 1, 0, 1, 0, 0, 0]
};
function arred(n) { var r8 = Math.round(n / 8) * 8; if (r8 >= 8 && Math.abs(n - r8) <= 1) return r8; return Math.max(4, Math.round(n / 4) * 4); }
function cmdLocal(m) {
	if (m == "__mais") { data.partes.push({ p: "DROP", n: 8 }); mudou(); return true; }
	if (m == "__reset") {
		if (!data.ref) return true;
		data.partes = [];
		for (var i = 0; i < data.ref.partes.length; i++) { var r = data.ref.partes[i]; data.partes.push({ p: r.p, n: arred(r.b - r.a), a: r.a, b: r.b }); }
		mudou(); return true;
	}
	return false;
}

// Max chama onclick(x, y, botão, cmd, shift, capslock, option, ctrl)
function onclick(x, y, but, cmd, shift) {
	arrasto = -1; bordaArr = null;
	for (var b = hits.length - 1; b >= 0; b--) {
		var hb = hits[b];
		if (x < hb.x || x > hb.x + hb.w || y < hb.y || y > hb.y + hb.h) continue;
		if (hb.tipo == "cmd") {
			var m = hb.msg;
			if (licCliqueLocal(m)) return;
			if (cmdLocal(m[0])) return;
			if (m[0] == "ui_loc") cfg.loc = m[1];
			if (m[0] == "build" || m[0] == "analyze" || m[0] == "undo") { msgAviso = ""; if (data) save(); }
			outlet(0, m);
			mgraphics.redraw();
			return;
		}
		if (!data || prog) return;
		if (hb.tipo == "borda") { bordaArr = { i: hb.i, x0: x, n0: data.partes[hb.i].n }; return; }
		if (hb.tipo == "parte") {
			if (shift) { if (data.partes.length > 1) data.partes.splice(hb.i, 1); }
			else { var p = data.partes[hb.i], k = PARTES.indexOf(p.p); p.p = PARTES[(k + 1) % PARTES.length]; }
			mudou(); return;
		}
		if (hb.tipo == "grupo") {
			var t = data.tracks[hb.t];
			t.g = (t.g + 1) % GRUPOS.length; t.auto = 0;
			t.on = []; for (var q = 0; q < PARTES.length; q++) t.on.push(REGRAS[PARTES[q]][t.g]);
			mudou(); return;
		}
		if (hb.tipo == "cel") {
			var tc = data.tracks[hb.t];
			tc.on[hb.k] = tc.on[hb.k] == 1 ? 0 : 1;
			mudou(); return;
		}
	}
	if (!data || prog || !licPronta() || licInfo) return;
	if (lay && lay.maxFirst > 0 && x >= lay.W - SB_W - 4 && y >= TOP) {
		arrasto = y;
		rolar(lay.maxFirst * (y - TOP) / Math.max(1, lay.listH));
	}
}
onclick.local = 1;

function ondblclick(x, y) {
	if (!data || prog) return;
	for (var b = hits.length - 1; b >= 0; b--) {
		var hb = hits[b];
		if (hb.tipo != "parte" || x < hb.x || x > hb.x + hb.w || y < hb.y || y > hb.y + hb.h) continue;
		var p = data.partes[hb.i];
		if (p.n < 2) return;
		var n1 = p.n >= 8 ? Math.round(p.n / 8) * 4 : Math.floor(p.n / 2);
		if (n1 < 1 || n1 >= p.n) n1 = Math.floor(p.n / 2);
		data.partes.splice(hb.i + 1, 0, { p: p.p, n: p.n - n1 });
		p.n = n1; p.a = null; p.b = null;
		mudou();
		return;
	}
}
ondblclick.local = 1;

function ondrag(x, y, but, cmd, shift) {
	if (bordaArr && data) {
		var G = geo(lay ? lay.W : 1360);
		var passo = shift ? 1 : 4;
		var d = (x - bordaArr.x0) / G.k;
		var n = Math.max(passo, Math.round((bordaArr.n0 + d) / passo) * passo);
		if (!shift && bordaArr.n0 % 4 && Math.abs(d) < 2) n = bordaArr.n0;
		var p = data.partes[bordaArr.i];
		if (n != p.n) { p.n = n; mgraphics.redraw(); }
		if (!but) { bordaArr = null; mudou(); }
		return;
	}
	if (arrasto >= 0 && lay) {
		if (!but) { arrasto = -1; return; }
		rolar(lay.maxFirst * (y - TOP) / Math.max(1, lay.listH));
	}
}
ondrag.local = 1;

function rolar(n) {
	if (!lay) return;
	var novo = Math.max(0, Math.min(lay.maxFirst, Math.round(n)));
	if (novo != first) { first = novo; mgraphics.redraw(); }
}
function faixaEm(x, y) {
	if (!lay || !lay.laneH || y < TOP || y >= TOP + lay.listH || x > lay.W - SB_W - 4) return -1;
	var i = first + Math.floor((y - TOP) / lay.laneH);
	return i < Math.min(lay.n, first + lay.vis) ? i : -1;
}
function onidle(x, y) {
	if (prog) return;
	var h = faixaEm(x, y), hp = -1;
	for (var b = 0; b < hits.length; b++) { var hb = hits[b]; if (hb.tipo == "parte" && x >= hb.x && x <= hb.x + hb.w && y >= hb.y && y <= hb.y + hb.h) { hp = hb.i; break; } }
	if (h != hover || hp != hoverParte) { hover = h; hoverParte = hp; mgraphics.redraw(); }
}
onidle.local = 1;
function onidleout() { if (hover != -1 || hoverParte != -1) { hover = -1; hoverParte = -1; mgraphics.redraw(); } }
onidleout.local = 1;
function onwheel(x, y, wx, wy) {
	if (!lay || lay.maxFirst <= 0) return;
	rolar(first - (wy > 0 ? 3 : wy < 0 ? -3 : 0));
	hover = faixaEm(x, y);
}
onwheel.local = 1;
function onresize() { mgraphics.redraw(); }
