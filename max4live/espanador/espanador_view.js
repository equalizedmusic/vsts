// ESPANADOR — janela flutuante (jsui). Desenha "espanador_data" (faixas, cores, saídas, clips, grupo de cada faixa).
// Seleção: clique na faixa marca/desmarca, Shift+clique marca o intervalo, arrastar marca várias.
// Botões de grupo (os 8 do MAESTRO): põem as faixas marcadas no grupo. NONE tira do grupo.
// AUTO BY NAME marca pelo nome as faixas sem grupo. PREFIX / BUS / AUTO ON SCAN, SCAN, APPLY, UNDO vão para o motor.
// APPLY: o motor organiza de cima para baixo e avisa cada faixa ("progresso"); o espanador desce atrás da última
// faixa feita e, atrás dele, as faixas já aparecem com a cor do grupo.

mgraphics.init();
mgraphics.relative_coords = 0;
mgraphics.autofill = 0;
outlets = 1;

var DICT = "espanador_data";
var VERSAO = "0.1.0";
var msgNova = "";
var HEAD_H = 72, BAR_H = 86, RULER_Y = 160, TOP = 188, FOOT_H = 40, SB_W = 14;
var X0 = 540;
var LANE_MIN = 18, LANE_MAX = 28;
var AC = [0.93, 0.36, 0.55], OK = [0.37, 0.75, 0.48], WARN = [0.88, 0.33, 0.31], CINZA = [0.55, 0.55, 0.58];
var GRUPOS = ["KICK", "BASS", "DRUMS", "HATS", "SYNTHS", "AMBIENCE", "VOCAL", "FX"];
var CORES = [0xff8a3d, 0x45a6ff, 0xf5c451, 0xe6e1f5, 0xc9b0ff, 0x4fd1c5, 0xff7fb5, 0xc4e86b];
var cfg = { prefix: 0, bus: 1, auto: 1 };
var data = null;
var msgStatus = "", msgAviso = "";
var hits = [];
var first = 0, lay = null, arrasto = -1, hover = -1;
var ancora = -1, arrastaSel = null;   // seleção: última faixa clicada (Shift) e o estado que o arrasto pinta
var pendente = 0;
var anim = null;    // { total, feitos, alvo (linha), y (linha, contínua), motorFim, fimT, t }
var tAnim = new Task(passoAnim);
tAnim.interval = 33;

function arr(x) { if (!x) return []; return (x instanceof Array) ? x : [x]; }
function load() {
	var d = new Dict(DICT), sel = {};
	if (data) for (var s = 0; s < data.tracks.length; s++) if (data.tracks[s].sel) sel[data.tracks[s].id] = 1;
	try { data = JSON.parse(d.stringify()); } catch (e) { data = null; }
	if (data && !data.tracks) data = null;
	if (data) {
		data.tracks = arr(data.tracks);
		for (var i = 0; i < data.tracks.length; i++) {
			var t = data.tracks[i];
			t.clips = arr(t.clips);
			if (t.clips.length && !(t.clips[0] instanceof Array)) t.clips = [t.clips];   // um clip só: o Dict achata
			t.g = t.g == null ? -1 : Number(t.g);
			t.bus = t.bus == null ? -1 : Number(t.bus);
			t.sel = sel[t.id] ? 1 : 0;
		}
		if (data.cfg) cfg = data.cfg;
	}
}
function save() { var d = new Dict(DICT); d.parse(JSON.stringify(data)); }

// mensagens do motor
function bang() { if (anim) { pendente = 1; return; } load(); mgraphics.redraw(); }
function loadbang() { load(); mgraphics.redraw(); }
function status() { msgStatus = arrayfromargs(arguments).join(" "); if (!anim) mgraphics.redraw(); }
function aviso() { msgAviso = arrayfromargs(arguments).join(" "); if (!anim) mgraphics.redraw(); }
function varre(total) {
	msgAviso = "";
	if (data) for (var i = 0; i < data.tracks.length; i++) data.tracks[i].sel = 0;
	anim = { total: total, feitos: 0, alvo: 0, y: null, motorFim: 0, fimT: 0, t: 0 };
	tAnim.repeat();
}
function progresso(i, total, linha) { if (!anim) return; anim.feitos = i; anim.total = total; if (linha + 1 > anim.alvo) anim.alvo = linha + 1; }
function fim() { if (anim) anim.motorFim = 1; }
function valores(p, b, a) { cfg = { prefix: p | 0, bus: b | 0, auto: a | 0 }; if (!anim) mgraphics.redraw(); }
function nova(v) { msgNova = String(v || ""); if (!anim) mgraphics.redraw(); }

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
function corG(g) { return corInt(CORES[g]); }
function escura(c) { return 0.299 * c[0] + 0.587 * c[1] + 0.114 * c[2] > 0.6; }

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

function contaGrupo(g) { var n = 0; if (data) for (var i = 0; i < data.tracks.length; i++) if (data.tracks[i].g == g && data.tracks[i].bus < 0) n++; return n; }
function nSel() { var n = 0; if (data) for (var i = 0; i < data.tracks.length; i++) if (data.tracks[i].sel) n++; return n; }

// ---------------------------------------------------------------- pintura
function paint() {
	var W = box.rect[2] - box.rect[0], H = box.rect[3] - box.rect[1];
	hits = [];
	var tracks = data ? data.tracks : [];
	var feito = anim && anim.fimT > 0, varrendo = anim && !feito;

	caixa(0, 0, W, H, 0.12, 0.12, 0.13);
	caixa(0, 0, W, HEAD_H, 0.08, 0.09, 0.09);
	rgb(0.91, 0.91, 0.91); texto("ESPANADOR", 16, 33, 26, true);
	var tx = 16 + largura("ESPANADOR", 26, true) + 10;
	rgb(0.55, 0.55, 0.58); texto(VERSAO, tx, 31, 12, false);
	tx += largura(VERSAO, 12, false) + 18;
	var fimSt = W - 20;
	if (licPronta()) {
		var wl = largura("LICENSE", 11, true) + 22;
		botaoP(W - 16 - wl, 14, wl, 24, "LICENSE", ["lic_info"]);
		fimSt = W - 16 - wl - 10;
		if (msgNova) {
			var tn = "NEW VERSION " + msgNova + " · DOWNLOAD", wn = largura(tn, 11, true) + 22;
			botaoP(fimSt - wn, 14, wn, 24, tn, ["baixar_nova"], OK);
			fimSt -= wn + 10;
		}
	}
	if (msgStatus) { rgbv(AC); texto(cabeFim(msgStatus, fimSt - tx, 13, true), tx, 31, 13, true); }
	if (!licPronta() || licInfo) { licPinta(W, H, HEAD_H, "ESPANADOR", AC, hits); return; }

	if (data) {
		var nG = 0, usados = 0;
		for (var g0 = 0; g0 < 8; g0++) { var c0 = contaGrupo(g0); nG += c0; if (c0) usados++; }
		var resumo = tracks.length + " FAIXAS · " + nG + " EM " + usados + " GRUPO" + (usados == 1 ? "" : "S");
		rgb(0.91, 0.91, 0.91); texto(resumo, 16, 58, 14, true);
		rgb(0.55, 0.55, 0.58);
		texto("BUS " + (cfg.bus ? "ON: cada grupo vai para uma faixa de áudio com o nome dele" : "OFF: só cor" + (cfg.prefix ? " e nome" : "")) + (cfg.prefix ? " · PREFIX ON" : ""),
			16 + largura(resumo, 14, true) + 16, 58, 12, false);
	}

	barra(W, varrendo, feito);

	if (!data) {
		rgb(0.7, 0.7, 0.72); texto("Clique SCAN para ler as faixas do set.", 16, TOP + 40, 16, false);
		rgb(0.55, 0.55, 0.58); texto("Depois marque as faixas (clique, Shift+clique ou arrastando) e escolha o grupo nos botões de cima.", 16, TOP + 66, 13, false);
		rodape(W, H, tracks, varrendo, feito);
		return;
	}

	var sig = data.sig || 4;
	var minBeat = Math.floor(Math.max(0, data.minBeat) / sig) * sig;
	var maxBeat = data.maxBeat > minBeat ? data.maxBeat : minBeat + sig;
	var x1 = W - SB_W - 8;
	var bx = function (b) { return X0 + (x1 - X0) * (b - minBeat) / (maxBeat - minBeat); };

	var listH = H - TOP - FOOT_H;
	var laneH = Math.max(LANE_MIN, Math.min(LANE_MAX, Math.floor(listH / Math.max(1, tracks.length))));
	var vis = Math.max(1, Math.floor(listH / laneH));
	var maxFirst = Math.max(0, tracks.length - vis);
	// na animação a lista acompanha o espanador
	if (varrendo && anim.y != null) first = Math.max(0, Math.min(maxFirst, Math.round(anim.y - vis * 0.6)));
	if (first > maxFirst) first = maxFirst;
	var last = Math.min(tracks.length, first + vis);
	var fundo = TOP + (last - first) * laneH;
	lay = { listH: listH, vis: vis, n: tracks.length, maxFirst: maxFirst, W: W, laneH: laneH };
	var fs = Math.max(11, Math.min(13, laneH - 7));

	// linha (contínua) até onde o espanador já passou
	var passou = -1;
	if (anim && anim.y != null) passou = anim.y;
	if (feito) passou = 1e9;

	// régua
	rgb(0.55, 0.55, 0.58);
	texto("TRACK", 36, RULER_Y + 18, 10, true); texto("GROUP", 262, RULER_Y + 18, 10, true); texto("OUTPUT", 380, RULER_Y + 18, 10, true);
	if (data.maxBeat > 0) {
		var bars = (maxBeat - minBeat) / sig;
		var step = Math.max(1, Math.pow(2, Math.ceil(Math.log(Math.max(1, bars) / 16) / Math.LN2)));
		for (var bar = 0; bar <= bars; bar += step) {
			var xr = bx(minBeat + bar * sig);
			caixa(xr, RULER_Y + 6, 1, 16, 0.22, 0.23, 0.25);
			rgb(0.55, 0.55, 0.58); texto(minBeat / sig + bar + 1, xr + 4, RULER_Y + 18, 10, true);
			caixa(xr, TOP, 1, fundo - TOP, 0.17, 0.17, 0.19);
		}
	} else { rgb(0.45, 0.45, 0.48); texto("(sem clips no arranjo)", X0, RULER_Y + 18, 10, false); }

	for (var i = first; i < last; i++) {
		var tr = tracks[i];
		var y = TOP + (i - first) * laneH, cy = y + laneH / 2;
		var limpo = i + 1 <= passou;               // o espanador já passou por esta faixa
		var g = tr.bus >= 0 ? tr.bus : tr.g;
		var corAgora = (limpo && tr.g >= 0) ? corG(tr.g) : corInt(tr.color);
		if (tr.sel) caixa(0, y, W - SB_W - 4, laneH - 1, AC[0], AC[1], AC[2], 0.20);
		else if (i == hover && !anim) caixa(0, y, W - SB_W - 4, laneH - 1, 0.19, 0.19, 0.22);
		else caixa(0, y, W - SB_W - 4, laneH - 1, i % 2 ? 0.15 : 0.135, i % 2 ? 0.15 : 0.137, i % 2 ? 0.17 : 0.15);

		caixa(6, y + 3, 4, laneH - 7, corAgora[0], corAgora[1], corAgora[2]);
		// caixinha de seleção
		var cb = Math.min(12, laneH - 8), cby = y + (laneH - cb) / 2;
		if (tr.bus < 0) {
			if (tr.sel) caixaR(16, cby, cb, cb, 2, AC);
			else { rgb(0.42, 0.42, 0.45); mgraphics.set_line_width(1.2); mgraphics.rectangle(16.5, cby + 0.5, cb - 1, cb - 1); mgraphics.stroke(); }
		}
		// nome (Group Track com ▾, faixa dentro de grupo recuada, bus com selo)
		var nx = 36 + (tr.member ? 12 : 0);
		var nome = (tr.fold ? "▾ " : "") + tr.name;
		if (tr.bus >= 0) {
			var cbus = corG(tr.bus);
			caixaR(nx, cy - 8, 30, 16, 3, cbus, 0.9); rgb(0.07, 0.07, 0.08); textoC("BUS", nx + 15, cy + 4, 9, true);
			nx += 36;
		}
		rgb(0.91, 0.91, 0.91);
		texto(cabe(nome, 250 - nx, fs - 1, true), nx, cy + fs / 2 - 2, fs - 1, true);
		hits.push({ x: 0, y: y, w: W - SB_W - 4, h: laneH, tipo: "faixa", t: i });

		// grupo
		if (tr.g >= 0 && tr.bus < 0) {
			var cgc = corG(tr.g), wc = 100;
			if (tr.auto && !limpo) { contorno(262, cy - 9, wc, 18, 4, cgc, 0.9, 1.2); rgbv(cgc); textoC(GRUPOS[tr.g] + " ?", 262 + wc / 2, cy + 4, 11, true); }
			else { caixaR(262, cy - 9, wc, 18, 4, cgc); if (escura(cgc)) rgb(0.07, 0.07, 0.08); else rgb(0.98, 0.98, 0.98); textoC(GRUPOS[tr.g], 262 + wc / 2, cy + 4, 11, true); }
		} else if (tr.bus < 0) { rgb(0.32, 0.32, 0.34); textoC("—", 312, cy + 4, 11, true); }

		// saída: a de agora e, se o APPLY vai mudar, a nova
		var out = String(tr.out || "");
		var vaiBus = cfg.bus && tr.g >= 0 && tr.bus < 0 && !tr.fold && out.toUpperCase() != GRUPOS[tr.g] && out.toUpperCase() != GRUPOS[tr.g] + " BUS";
		if (vaiBus && !limpo) {
			rgb(0.55, 0.55, 0.58); var to = cabeFim(out, 70, 11, false); texto(to, 380, cy + 4, 11, false);
			var xo = 380 + largura(to, 11, false) + 6; rgbv(corG(tr.g)); texto("→ " + GRUPOS[tr.g], xo, cy + 4, 11, true);
		} else if (vaiBus && limpo) { rgbv(corG(tr.g)); texto(GRUPOS[tr.g], 380, cy + 4, 11, true); }
		else { if (tr.bus < 0 && g >= 0 && /bus|^[A-Z]+$/.test(out) && out.toUpperCase().indexOf(GRUPOS[g]) == 0) rgbv(corG(g)); else rgb(0.55, 0.55, 0.58); texto(cabeFim(out, 150, 11, false), 380, cy + 4, 11, false); }

		// clips no arranjo (blocos na cor que a faixa tem / vai ter)
		var cc = tr.clips;
		for (var j = 0; j < cc.length; j++) {
			var xa = bx(cc[j][0]), xb = Math.max(xa + 1, bx(cc[j][1]));
			caixa(xa, y + 3, xb - xa, laneH - 7, corAgora[0], corAgora[1], corAgora[2], 0.55);
			caixa(xa, y + 3, xb - xa, 2, corAgora[0], corAgora[1], corAgora[2], 1);
		}
	}
	caixa(X0 - 8, TOP, 1, fundo - TOP, 0.22, 0.23, 0.25);

	if (maxFirst > 0) {
		var sx = W - SB_W - 2;
		caixa(sx, TOP, SB_W, listH, 0.16, 0.16, 0.18);
		var tth = Math.max(30, listH * vis / tracks.length);
		caixa(sx + 2, TOP + (listH - tth) * first / maxFirst, SB_W - 4, tth, 0.55, 0.55, 0.58);
	}

	// espanador descendo (linha contínua anim.y)
	if (varrendo && anim.y != null) {
		var ey = TOP + (anim.y - first) * laneH;
		if (ey > TOP - 40 && ey < fundo + 40) desenhaEspanador(ey, W - SB_W - 4, anim.t);
	}

	rodape(W, H, tracks, varrendo, feito);
}

function barra(W, varrendo, feito) {
	caixa(0, HEAD_H, W, BAR_H, 0.10, 0.11, 0.12);
	caixa(0, HEAD_H + BAR_H - 1, W, 1, 0.22, 0.23, 0.25);
	// linha 1: grupos
	var y = HEAD_H + 24;
	rgb(0.55, 0.55, 0.58); texto("GROUP", 16, y + 4, 11, true);
	var x = 72, ns = nSel();
	for (var g = 0; g < 8; g++) {
		var c = corG(g), n = contaGrupo(g), w = 104;
		caixaR(x, y - 13, w, 26, 3, c, ns ? 1 : 0.85);
		if (escura(c)) rgb(0.07, 0.07, 0.08); else rgb(0.98, 0.98, 0.98);
		textoC(GRUPOS[g] + (n ? "  " + n : ""), x + w / 2, y + 4, 12, true);
		hits.push({ x: x, y: y - 13, w: w, h: 26, tipo: "grupo", g: g });
		x += w + 4;
	}
	x += 6;
	botao(x, y - 13, 62, 26, "NONE", false, CINZA, null);
	hits.push({ x: x, y: y - 13, w: 62, h: 26, tipo: "grupo", g: -1 });
	x += 74;
	rgbv(ns ? AC : CINZA); texto(ns ? ns + " SELECTED" : "select tracks, then a group", x, y + 4, 12, true);
	if (ns) { var xs = x + largura(ns + " SELECTED", 12, true) + 12; botaoP(xs, y - 11, 64, 22, "CLEAR", ["__limpa_sel"]); }

	// linha 2: AUTO · PREFIX · BUS | SCAN APPLY UNDO
	y = HEAD_H + 62;
	x = 16;
	rgb(0.55, 0.55, 0.58); texto("AUTO", x, y + 4, 11, true); x = 72;
	botao(x, y - 13, 104, 26, "BY NAME", false, AC, anim ? null : ["auto_agora"]); x += 110;
	rgb(0.55, 0.55, 0.58); texto("ON SCAN", x, y + 4, 10, true); x += 54;
	botao(x, y - 13, 44, 26, "ON", cfg.auto == 1, AC, ["ui_auto", 1]); x += 46;
	botao(x, y - 13, 44, 26, "OFF", cfg.auto == 0, CINZA, ["ui_auto", 0]); x += 64;
	rgb(0.55, 0.55, 0.58); texto("PREFIX", x, y + 4, 11, true); x += 50;
	botao(x, y - 13, 44, 26, "ON", cfg.prefix == 1, AC, ["ui_prefix", 1]); x += 46;
	botao(x, y - 13, 44, 26, "OFF", cfg.prefix == 0, CINZA, ["ui_prefix", 0]); x += 64;
	rgb(0.55, 0.55, 0.58); texto("BUS", x, y + 4, 11, true); x += 34;
	botao(x, y - 13, 44, 26, "ON", cfg.bus == 1, AC, ["ui_bus", 1]); x += 46;
	botao(x, y - 13, 44, 26, "OFF", cfg.bus == 0, CINZA, ["ui_bus", 0]);

	var wR = 90, wAp = 128, wAn = 90, xR = W - 18 - wR, xAp = xR - 8 - wAp, xAn = xAp - 8 - wAn;
	botao(xAn, y - 15, wAn, 30, "SCAN", false, AC, anim ? null : ["scan"]);
	botao(xAp, y - 15, wAp, 30, feito ? "DONE ✓" : varrendo ? "DUSTING…" : "APPLY", true, feito ? OK : AC, anim ? null : ["apply"], varrendo || feito);
	botao(xR, y - 15, wR, 30, "UNDO", false, AC, anim ? null : ["undo"]);
}

function rodape(W, H, tracks, varrendo, feito) {
	caixa(0, H - FOOT_H, W, FOOT_H, 0.08, 0.09, 0.09);
	var y = H - 15;
	if (varrendo) { rgbv(AC); texto("APPLY… " + Math.min(anim.feitos, anim.total) + " de " + anim.total + " faixas", 16, y, 13, true); return; }
	if (feito || (msgAviso && hover < 0)) { rgbv(OK); texto(cabeFim(msgAviso || "APPLY FEITO", W - 32, 13, true), 16, y, 13, true); return; }
	if (!data) return;
	if (hover >= 0 && hover < tracks.length) {
		var tr = tracks[hover];
		rgb(0.91, 0.91, 0.91); texto(cabe(tr.name, W - 520, 13, true), 16, y, 13, true);
		var info = (tr.fold ? "GROUP TRACK · " : "") + (tr.member ? "dentro de um grupo · " : "") + (tr.bus >= 0 ? "BUS do grupo " + GRUPOS[tr.bus] + " · " : "") +
			"saída: " + (tr.out || "?") + " · " + tr.clips.length + " clips" + (tr.g >= 0 && tr.bus < 0 ? " · grupo " + GRUPOS[tr.g] + (tr.auto ? " (pelo nome, confira)" : "") : "");
		rgb(0.62, 0.62, 0.65); textoD(info, W - 16, y, 12, false);
		return;
	}
	rgb(0.91, 0.91, 0.91);
	texto("clique = marca · Shift+clique = intervalo · arrastar = várias · depois o botão do grupo", 16, y, 13, true);
	var nMemb = 0; for (var i = 0; i < tracks.length; i++) if (tracks[i].member && tracks[i].g >= 0 && tracks[i].bus < 0) nMemb++;
	if (cfg.bus && nMemb) { rgbv(WARN); textoD(nMemb + (nMemb > 1 ? " faixas estão" : " faixa está") + " dentro de Group Track: com BUS ON a saída sai do grupo", W - 16, y, 12, true); }
	else if (lay && lay.maxFirst > 0) { rgb(0.55, 0.55, 0.58); textoD("FAIXAS " + (first + 1) + "–" + Math.min(tracks.length, first + lay.vis) + " DE " + tracks.length, W - 16, y, 12, false); }
}

// espanador visto de cima, deitado: a pluma de penas (cores dos grupos) cobre a largura das faixas e desce;
// o cabo sai para trás (para cima, à direita); o pó vai sendo empurrado na frente (embaixo)
function desenhaEspanador(y, W, t) {
	var s = 31;
	function r2() { s = (s * 16807) % 2147483647; return s / 2147483647; }
	// pó na frente
	for (var k = 0; k < 90; k++) {
		var xx = 10 + r2() * (W - 20), dy = 10 + r2() * 22 + Math.sin(t * 7 + k) * 1.5, rr = 0.8 + r2() * 2;
		rgb(0.72, 0.68, 0.62, 0.25 + r2() * 0.4); mgraphics.ellipse(xx - rr, y + dy - rr, rr * 2, rr * 2); mgraphics.fill();
	}
	// sombra da pluma
	caixaR(6, y - 9, W - 8, 22, 11, [0, 0, 0], 0.30);
	// penas: elipses sobrepostas, cores dos grupos, balançando
	for (var p = 0; p < 150; p++) {
		var px = 4 + (p / 149) * (W - 8) + (r2() - 0.5) * 10;
		var py = y + (r2() - 0.5) * 12 + Math.sin(t * 6 + p * 0.7) * 1.2;
		var c = corG(Math.floor(r2() * 8)), lw = 10 + r2() * 14, lh = 6 + r2() * 6;
		rgbv(c, 0.75); mgraphics.ellipse(px - lw / 2, py - lh / 2, lw, lh); mgraphics.fill();
		rgb(1, 1, 1, 0.18); mgraphics.ellipse(px - lw / 4, py - lh / 2, lw / 2, lh / 3); mgraphics.fill();
	}
	// cabo: do meio da pluma para trás (cima, à direita)
	var hx = W * 0.62, hy = y - 4, ex = hx + 300, ey = y - 170;
	try { mgraphics.set_line_cap("round"); } catch (e) { }
	rgb(0, 0, 0, 0.35); mgraphics.set_line_width(14); mgraphics.move_to(hx + 6, hy + 8); mgraphics.line_to(ex + 6, ey + 8); mgraphics.stroke();
	rgb(0.54, 0.35, 0.17); mgraphics.set_line_width(12); mgraphics.move_to(hx, hy); mgraphics.line_to(ex, ey); mgraphics.stroke();
	rgb(0.69, 0.48, 0.26); mgraphics.set_line_width(4); mgraphics.move_to(hx - 1, hy - 4); mgraphics.line_to(ex + 1, ey - 4); mgraphics.stroke();
	rgbv(AC); mgraphics.set_line_width(14); mgraphics.move_to(hx + 4, hy - 2); mgraphics.line_to(hx + 34, hy - 19); mgraphics.stroke();
	rgb(0.16, 0.17, 0.18); mgraphics.set_line_width(14); mgraphics.move_to(ex - 30, ey + 17); mgraphics.line_to(ex, ey); mgraphics.stroke();
	try { mgraphics.set_line_cap("butt"); } catch (e) { }
}

// ---------------------------------------------------------------- animação (33 ms)
function passoAnim() {
	if (!anim || !data) { tAnim.cancel(); anim = null; return; }
	var dt = 0.033;
	anim.t += dt;
	var n = data.tracks.length;
	if (anim.fimT > 0) {
		if (anim.t - anim.fimT > 2.0) {
			tAnim.cancel(); anim = null;
			if (pendente) load();
			pendente = 0;
		}
		mgraphics.redraw();
		return;
	}
	if (anim.y == null) anim.y = -1;
	var alvo = anim.motorFim ? n + 1.5 : anim.alvo;
	var falta = alvo - anim.y;
	if (falta > 0) {
		var v = Math.max(n / 6, Math.min(n / 1.4, falta * 3));
		anim.y = Math.min(alvo, anim.y + v * dt);
	}
	if (anim.motorFim && anim.y >= n + 1.5 - 1e-9) anim.fimT = anim.t;
	mgraphics.redraw();
}

// ---------------------------------------------------------------- mouse
function faixaEm(x, y) {
	if (!lay || y < TOP || y >= TOP + lay.listH || x > lay.W - SB_W - 4) return -1;
	var i = first + Math.floor((y - TOP) / lay.laneH);
	return i < Math.min(lay.n, first + lay.vis) ? i : -1;
}
function rolar(n) {
	if (!lay) return;
	var novo = Math.max(0, Math.min(lay.maxFirst, Math.round(n)));
	if (novo != first) { first = novo; mgraphics.redraw(); }
}
function poeGrupo(g) {
	var n = 0;
	for (var i = 0; i < data.tracks.length; i++) {
		var t = data.tracks[i];
		if (!t.sel) continue;
		t.g = g; t.auto = 0; t.sel = 0; n++;
	}
	if (!n) { msgStatus = "marque as faixas antes (clique, Shift+clique ou arrastando) e depois escolha o grupo"; mgraphics.redraw(); return; }
	ancora = -1;
	save();
	outlet(0, "marcas");
	mgraphics.redraw();
}

// Max chama onclick(x, y, botão, cmd, shift, capslock, option, ctrl)
function onclick(x, y, but, cmd, shift) {
	arrasto = -1; arrastaSel = null;
	for (var b = hits.length - 1; b >= 0; b--) {
		var hb = hits[b];
		if (x < hb.x || x > hb.x + hb.w || y < hb.y || y > hb.y + hb.h) continue;
		if (hb.tipo == "grupo") { if (!data || anim) return; poeGrupo(hb.g); return; }
		if (hb.tipo != "cmd") continue;
		var m = hb.msg;
		if (licCliqueLocal(m)) return;
		if (m[0] == "__limpa_sel") { for (var s = 0; s < data.tracks.length; s++) data.tracks[s].sel = 0; ancora = -1; mgraphics.redraw(); return; }
		if (m[0] == "ui_prefix") cfg.prefix = m[1];
		else if (m[0] == "ui_bus") cfg.bus = m[1];
		else if (m[0] == "ui_auto") cfg.auto = m[1];
		if (m[0] == "apply" || m[0] == "scan" || m[0] == "undo") msgAviso = "";
		if (m[0] == "apply" || m[0] == "auto_agora") save();
		outlet(0, m);
		mgraphics.redraw();
		return;
	}
	if (!data || anim || !licPronta() || licInfo) return;
	if (lay && lay.maxFirst > 0 && x >= lay.W - SB_W - 4 && y >= TOP) {
		arrasto = y;
		rolar(lay.maxFirst * (y - TOP) / Math.max(1, lay.listH));
		return;
	}
	var i = faixaEm(x, y);
	if (i < 0) return;
	var tr = data.tracks[i];
	if (tr.bus >= 0) return;
	if (shift && ancora >= 0) {
		var a = Math.min(ancora, i), z = Math.max(ancora, i);
		for (var k = a; k <= z; k++) if (data.tracks[k].bus < 0) data.tracks[k].sel = 1;
	} else {
		tr.sel = tr.sel ? 0 : 1;
		arrastaSel = tr.sel;
	}
	ancora = i;
	mgraphics.redraw();
}
onclick.local = 1;

function ondrag(x, y, but) {
	if (arrasto >= 0 && lay) {
		if (!but) { arrasto = -1; return; }
		rolar(lay.maxFirst * (y - TOP) / Math.max(1, lay.listH));
		return;
	}
	if (arrastaSel == null || !data) return;
	if (!but) { arrastaSel = null; return; }
	var i = faixaEm(Math.min(x, lay.W - SB_W - 6), y);
	if (i < 0 || data.tracks[i].bus >= 0) return;
	// arrasta do clique até aqui: tudo no estado do primeiro
	var a = Math.min(ancora, i), z = Math.max(ancora, i), mudou = 0;
	for (var k = a; k <= z; k++) if (data.tracks[k].bus < 0 && data.tracks[k].sel != arrastaSel) { data.tracks[k].sel = arrastaSel; mudou = 1; }
	if (mudou) mgraphics.redraw();
}
ondrag.local = 1;

function onidle(x, y) {
	if (anim) return;
	var h = faixaEm(x, y);
	if (h != hover) { hover = h; mgraphics.redraw(); }
}
onidle.local = 1;
function onidleout() { if (hover != -1) { hover = -1; mgraphics.redraw(); } }
onidleout.local = 1;
function onwheel(x, y, wx, wy) {
	if (!lay || lay.maxFirst <= 0) return;
	rolar(first - (wy > 0 ? 3 : wy < 0 ? -3 : 0));
	hover = faixaEm(x, y);
}
onwheel.local = 1;
function onresize() { mgraphics.redraw(); }
