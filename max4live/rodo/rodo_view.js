// RODO — janela flutuante (jsui). Desenha "rodo_data" (faixas, picos, ganhos) e "rodo_env" (contorno das ondas).
// Escala das ondas = raiz da amplitude (0 dBFS = linha cheia); linha tracejada laranja = TARGET.
// Clique no nome da faixa: tira/põe a faixa na normalização. Barra de cima (2 linhas): SENSOR (PEAK/RMS/LUFS),
// TARGET do SENSOR (arrastar), PEAK GUARD; MODE, SCOPE; ANALYZE, APPLY, RESET (mandam para o motor pela saída do jsui).
// APPLY: o motor ajusta os clips da esquerda para a direita e avisa cada um ("progresso"); o rodo anda atrás
// do último clip feito (nunca na frente do motor) e, atrás dele, as ondas já aparecem no nível novo.

mgraphics.init();
mgraphics.relative_coords = 0;
mgraphics.autofill = 0;
outlets = 1;

var DICT = "rodo_data", DICT_ENV = "rodo_env";
var VERSAO = "0.3.0";
var msgNova = "";
var W0 = 1360, H0 = 840;
var HEAD_H = 72, BAR_H = 86, RULER_Y = 160, TOP = 188, FOOT_H = 40, SB_W = 14;
var X0 = 340;
var LANE_MIN = 18, LANE_MAX = 30;
var AC = [0.96, 0.65, 0.14], OK = [0.37, 0.75, 0.48], WARN = [0.88, 0.33, 0.31], WATER = [0.61, 0.85, 1], GUARDC = [0.95, 0.80, 0.35];
var SENS = ["PEAK", "RMS", "LUFS"], UNID = ["dB", "dB RMS", "LUFS"], FAIXA = [[-12, -0.3], [-36, -6], [-36, -6]];
var GUARD_DB = -0.3;
var cfg = { target: -6, mode: 1, scope: 0, sensor: 0, guard: 1, trms: -18, tlufs: -18 };
function alvo() { return cfg.sensor == 1 ? cfg.trms : cfg.sensor == 2 ? cfg.tlufs : cfg.target; }
function poeAlvo(v) { if (cfg.sensor == 1) cfg.trms = v; else if (cfg.sensor == 2) cfg.tlufs = v; else cfg.target = v; }
function cfgOk(c) { c.sensor = c.sensor | 0; if (c.guard == null) c.guard = 1; if (c.trms == null) c.trms = -18; if (c.tlufs == null) c.tlufs = -18; return c; }
var data = null, env = null;
var msgStatus = "", msgAviso = "";
var hits = [];
var first = 0, lay = null, arrasto = -1, hover = -1, arrastaTarget = 0;
var pendente = 0;   // chegou dado novo durante a animação: carrega quando o rodo terminar
var anim = null;    // { total, feitos, alvo, x, motorFim, fimT, t }
var tAnim = new Task(passoAnim);
tAnim.interval = 33;

function arr(x) { if (!x) return []; return (x instanceof Array) ? x : [x]; }

function load() {
	var d = new Dict(DICT);
	try { data = JSON.parse(d.stringify()); } catch (e) { data = null; }
	if (data && !data.tracks) data = null;
	if (data) { data.tracks = arr(data.tracks); for (var i = 0; i < data.tracks.length; i++) data.tracks[i].clips = arr(data.tracks[i].clips); }
	if (data && data.cfg && !arrastaTarget) cfg = cfgOk(data.cfg);
}
function loadEnv() {
	var d = new Dict(DICT_ENV);
	try { env = JSON.parse(d.stringify()); } catch (e) { env = null; }
	if (env) { env.tracks = arr(env.tracks); for (var i = 0; i < env.tracks.length; i++) env.tracks[i] = arr(env.tracks[i]); }
}
function save() { var d = new Dict(DICT); d.parse(JSON.stringify(data)); }

// mensagens do motor
function bang() { if (anim) { pendente |= 1; return; } load(); mgraphics.redraw(); }
function loadbang() { load(); loadEnv(); mgraphics.redraw(); }
function status() { msgStatus = arrayfromargs(arguments).join(" "); if (!anim) mgraphics.redraw(); }
function aviso() { msgAviso = arrayfromargs(arguments).join(" "); if (!anim) mgraphics.redraw(); }
function ondas() { if (anim) pendente |= 2; else { loadEnv(); mgraphics.redraw(); } }
function varre(total, b0, b1) {
	msgAviso = "";
	anim = { total: total, feitos: 0, alvo: b0, x: null, motorFim: 0, fimT: 0, t: 0 };
	tAnim.repeat();
}
function progresso(i, total, s) { if (!anim) return; anim.feitos = i; anim.total = total; if (s > anim.alvo) anim.alvo = s; }
function fim() { if (anim) anim.motorFim = 1; }
// valores atuais do motor (a janela pode abrir antes do 1º ANALYZE)
function valores(t, m, s, sen, g, trms, tlufs) {
	if (arrastaTarget) return;
	cfg = cfgOk({ target: Number(t), mode: m | 0, scope: s | 0, sensor: sen, guard: g, trms: trms == null ? null : Number(trms), tlufs: tlufs == null ? null : Number(tlufs) });
	if (!anim) mgraphics.redraw();
}
function nova(v) { msgNova = String(v || ""); if (!anim) mgraphics.redraw(); }

// ---------------------------------------------------------------- desenho (helpers)
function rgb(r, g, b, a) { mgraphics.set_source_rgba(r, g, b, a == null ? 1 : a); }
function rgbv(c, a) { rgb(c[0], c[1], c[2], a); }
function fonte(size, bold) { mgraphics.select_font_face(bold ? "Arial Bold" : "Arial"); mgraphics.set_font_size(size); }
function largura(s, size, bold) { fonte(size, bold); return mgraphics.text_measure(String(s))[0]; }
function texto(s, x, y, size, bold) { fonte(size, bold); mgraphics.move_to(x, y); mgraphics.show_text(String(s)); }
function textoD(s, xr, y, size, bold) { texto(s, xr - largura(s, size, bold), y, size, bold); }  // alinhado à direita
function textoC(s, xc, y, size, bold) { texto(s, xc - largura(s, size, bold) / 2, y, size, bold); }
function caixa(x, y, w, h, r, g, b, a) { rgb(r, g, b, a); mgraphics.rectangle(x, y, w, h); mgraphics.fill(); }
function caixaR(x, y, w, h, raio, c, a) { rgbv(c, a); mgraphics.rectangle_rounded(x, y, w, h, raio, raio); mgraphics.fill(); }
function f1(v) { return (v > 0.05 ? "+" : v < -0.05 ? "−" : "") + Math.abs(v).toFixed(1); }
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
function corFaixa(tr) {
	var c = tr.color | 0;
	if (c <= 0) return [0.48, 0.65, 1];
	return [((c >> 16) & 255) / 255, ((c >> 8) & 255) / 255, (c & 255) / 255];
}
function altura(db) { return Math.min(1, Math.sqrt(Math.pow(10, db / 20))); }  // raiz da amplitude

function botao(x, y, w, h, txt, on, cor, cmd, brilho) {
	if (brilho) { caixaR(x - 4, y - 4, w + 8, h + 8, 7, cor, 0.18); caixaR(x - 2, y - 2, w + 4, h + 4, 5, cor, 0.25); }
	caixaR(x, y, w, h, 3, on ? cor : [0.23, 0.23, 0.25]);
	if (on) rgb(0.07, 0.07, 0.08); else rgb(0.91, 0.91, 0.91);
	textoC(txt, x + w / 2, y + h / 2 + 4, 12, true);
	if (cmd) hits.push({ x: x, y: y, w: w, h: h, tipo: "cmd", msg: cmd });
}

// ---------------------------------------------------------------- pintura
function paint() {
	var W = box.rect[2] - box.rect[0], H = box.rect[3] - box.rect[1];
	hits = [];
	var tracks = data ? data.tracks : [];
	var feito = anim && anim.fimT > 0, varrendo = anim && !feito;

	caixa(0, 0, W, H, 0.12, 0.12, 0.13);
	// cabeçalho
	caixa(0, 0, W, HEAD_H, 0.08, 0.09, 0.09);
	rgb(0.91, 0.91, 0.91); texto("RODO", 16, 33, 26, true);
	var tx = 16 + largura("RODO", 26, true) + 10;
	rgb(0.55, 0.55, 0.58); texto(VERSAO, tx, 31, 12, false);
	tx += largura(VERSAO, 12, false) + 18;
	// canto direito: LICENSE (informações / desativar) e aviso de versão nova
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
	if (!licPronta() || licInfo) { licPinta(W, H, HEAD_H, "RODO", AC, hits); return; }

	if (data) {
		var resumo = tracks.length + " FAIXAS · " + data.nClips + " CLIPS · " + ["PICO", "RMS", "LOUDNESS"][cfg.sensor] + " VAI A " + f1(alvo()) + " " + UNID[cfg.sensor];
		rgb(0.91, 0.91, 0.91); texto(resumo, 16, 58, 14, true);
		var rx = 16 + largura(resumo, 14, true) + 16;
		rgb(0.55, 0.55, 0.58);
		texto("SENSOR " + SENS[cfg.sensor] + (cfg.sensor && cfg.guard ? " + PEAK GUARD" : "") + " · MODE " + ["CLIP", "TRACK", "ALL"][cfg.mode] + " · SCOPE " + ["ALL", "SELECTED"][cfg.scope] +
			(data.nClips ? " · ajuste de " + f1(data.dmin) + " a " + f1(data.dmax) + " dB" : ""), rx, 58, 12, false);
	}
	// legenda
	var gx = W - 340;
	rgbv(AC); mgraphics.set_line_width(1.5);
	for (var dx = 0; dx < 26; dx += 7) { mgraphics.move_to(gx + dx, 54); mgraphics.line_to(gx + Math.min(26, dx + 4), 54); }
	mgraphics.stroke();
	rgb(0.55, 0.55, 0.58); texto(cfg.sensor ? "−0.3 PK" : "TARGET", gx + 32, 58, 12, false);
	caixa(gx + 100, 47, 14, 14, 0.48, 0.65, 1); rgb(0.55, 0.55, 0.58); texto("CLIP, cor da faixa", gx + 120, 58, 12, false);
	caixa(gx + 242, 47, 14, 14, WARN[0], WARN[1], WARN[2]); rgb(0.55, 0.55, 0.58); texto("TETO", gx + 262, 58, 12, false);

	barra(W, varrendo, feito);

	if (!data) {
		rgb(0.7, 0.7, 0.72); texto("Clique ANALYZE para ler as faixas.", 16, TOP + 40, 16, false);
		rodape(W, H, tracks, varrendo, feito);
		return;
	}

	// escala do tempo: do compasso do 1º clip ao fim do último
	var sig = data.sig || 4;
	var minBeat = Math.floor(Math.max(0, data.minBeat) / sig) * sig;
	var maxBeat = data.maxBeat > minBeat ? data.maxBeat : minBeat + sig;
	var x1 = W - SB_W - 8;
	var bx = function (b) { return X0 + (x1 - X0) * (b - minBeat) / (maxBeat - minBeat); };

	var listH = H - TOP - FOOT_H;
	var laneH = Math.max(LANE_MIN, Math.min(LANE_MAX, Math.floor(listH / Math.max(1, tracks.length))));
	var vis = Math.max(1, Math.floor(listH / laneH));
	var maxFirst = Math.max(0, tracks.length - vis);
	if (first > maxFirst) first = maxFirst;
	var last = Math.min(tracks.length, first + vis);
	var fundo = TOP + (last - first) * laneH;
	lay = { listH: listH, vis: vis, n: tracks.length, maxFirst: maxFirst, W: W, laneH: laneH };
	var fs = Math.max(11, Math.min(13, laneH - 8));

	// posição do rodo (em x da tela)
	var rodoX = -1e9;
	if (anim && anim.x != null) rodoX = bx(anim.x);
	if (feito) rodoX = 1e9;

	// régua
	rgb(0.55, 0.55, 0.58);
	texto("TRACK", 40, RULER_Y + 18, 10, true); textoD(SENS[cfg.sensor], 238, RULER_Y + 18, 10, true); textoD("GAIN", 318, RULER_Y + 18, 10, true);
	var bars = (maxBeat - minBeat) / sig;
	var step = Math.max(1, Math.pow(2, Math.ceil(Math.log(Math.max(1, bars) / 16) / Math.LN2)));
	for (var bar = 0; bar <= bars; bar += step) {
		var xr = bx(minBeat + bar * sig);
		caixa(xr, RULER_Y + 6, 1, 16, 0.22, 0.23, 0.25);
		rgb(0.55, 0.55, 0.58); texto(minBeat / sig + bar + 1, xr + 4, RULER_Y + 18, 10, true);
		caixa(xr, TOP, 1, fundo - TOP, 0.17, 0.17, 0.19);
	}

	// linha tracejada: PEAK = TARGET; RMS/LUFS = o teto do PEAK GUARD (as ondas são desenhadas pelo pico)
	var thT = altura(cfg.sensor ? GUARD_DB : cfg.target), linhaT = !cfg.sensor || cfg.guard;
	for (var i = first; i < last; i++) {
		var tr = tracks[i];
		var y = TOP + (i - first) * laneH, cy = y + laneH / 2, hh = laneH / 2 - 2;
		var off = tr.skip ? 1 : 0;
		var cor = corFaixa(tr);
		var clips = tr.clips;
		if (i == hover && !anim) caixa(0, y, W - SB_W - 4, laneH - 1, 0.19, 0.19, 0.22);
		else caixa(0, y, W - SB_W - 4, laneH - 1, i % 2 ? 0.15 : 0.135, i % 2 ? 0.15 : 0.137, i % 2 ? 0.17 : 0.15);

		// pico e ganho da faixa
		var pkAntes = -1e9, pkDepois = -1e9, dmin = 1e9, dmax = -1e9, falta = 0, gdb = 0, fimFaixa = -1e9, nOk = 0;
		for (var j = 0; j < clips.length; j++) {
			var c = clips[j];
			if (c.state != "ok") continue;
			nOk++;
			if (c.out > pkAntes) pkAntes = c.out;
			if (c.pk + c.g1 > pkDepois) pkDepois = c.pk + c.g1;
			if (c.delta < dmin) dmin = c.delta;
			if (c.delta > dmax) dmax = c.delta;
			if (c.falta > falta) falta = c.falta;
			if (c.guardDb > gdb) gdb = c.guardDb;
			if (c.e > fimFaixa) fimFaixa = c.e;
		}
		var pronta = !off && nOk && rodoX > bx(fimFaixa);

		caixa(6, y + 3, 4, laneH - 7, cor[0], cor[1], cor[2], off ? 0.35 : 1);
		var cb = Math.min(12, laneH - 8), cby = y + (laneH - cb) / 2;
		if (off) { rgb(0.55, 0.55, 0.58); mgraphics.set_line_width(1.2); mgraphics.rectangle(16.5, cby + 0.5, cb - 1, cb - 1); mgraphics.stroke(); }
		else caixaR(16, cby, cb, cb, 2, AC);
		var cinza = off ? 0.42 : 0.91;
		rgb(cinza, cinza, cinza);
		texto(cabe(tr.name, 150, fs - 1, true), 36, cy + fs / 2 - 2, fs - 1, true);
		hits.push({ x: 0, y: y, w: 170, h: laneH, tipo: "faixa", t: i });
		// RMS/LUFS: o motor manda o nível da faixa inteira (antes = out, depois = out1)
		if (cfg.sensor) { pkAntes = tr.out == null ? -1e9 : tr.out; pkDepois = tr.out1 == null ? -1e9 : tr.out1; }
		if (nOk) {
			var pk = pronta ? pkDepois : pkAntes;
			if (pronta) rgbv(falta > 0.06 ? WARN : OK); else rgb(0.55, 0.55, 0.58);
			textoD(pk > -1e8 ? f1(pk) : "—", 238, cy + 4, 12, true);
			var gt;
			if (off) { gt = "—"; rgb(0.42, 0.42, 0.44); }
			else if (falta > 0.06 && pronta) { gt = "faltou " + falta.toFixed(1); rgbv(WARN); }
			else if (gdb > 0.06 && pronta) { gt = "guard −" + gdb.toFixed(1); rgbv(GUARDC); }
			else {
				gt = Math.abs(dmax - dmin) < 0.05 ? f1(dmax) : f1(dmin) + "…" + f1(dmax);
				if (falta > 0.06) { gt += " !"; rgbv(WARN); } else if (gdb > 0.06) { gt += " ▲"; rgbv(GUARDC); } else if (pronta) rgb(0.55, 0.55, 0.58); else rgbv(AC);
			}
			textoD(cabeFim(gt, 92, 12, true), 318, cy + 4, 12, true);
		}

		// TARGET tracejado
		if (!off && linhaT) {
			var th = thT * hh;
			rgbv(cfg.sensor ? GUARDC : AC, 0.35); mgraphics.set_line_width(1);
			for (var xd = X0; xd < x1; xd += 7) {
				mgraphics.move_to(xd, cy - th + 0.5); mgraphics.line_to(Math.min(x1, xd + 4), cy - th + 0.5);
				mgraphics.move_to(xd, cy + th - 0.5); mgraphics.line_to(Math.min(x1, xd + 4), cy + th - 0.5);
			}
			mgraphics.stroke();
		}

		// clips
		var envT = env && env.tracks[i] ? env.tracks[i] : [];
		for (var j2 = 0; j2 < clips.length; j2++) {
			var c2 = clips[j2];
			var xa = bx(c2.s), xb = Math.max(xa + 1, bx(c2.e)), w = xb - xa;
			if (c2.state != "ok") {
				caixa(xa, y + 2, w, laneH - 5, 0.30, 0.30, 0.33, 0.8);
				var et = String(c2.state).toUpperCase();
				if (largura(et, 10, true) + 8 < w) { rgb(0.95, 0.80, 0.35); texto(et, xa + 4, cy + 4, 10, true); }
				continue;
			}
			caixa(xa, y + 1, w, laneH - 3, cor[0], cor[1], cor[2], off ? 0.06 : 0.13);
			rgbv(cor, off ? 0.2 : 0.4); mgraphics.set_line_width(1); mgraphics.rectangle(xa + 0.5, y + 1.5, w - 1, laneH - 4); mgraphics.stroke();
			var s = envT[j2] || "";
			var n = s.length;
			if (n) {
				var hA = altura(c2.pk + c2.g0) * hh, hD = altura(c2.pk + c2.g1) * hh;
				var passo = w / n;
				// lado já varrido (nível novo) e lado por varrer (nível de antes): um traço por ponto
				rgbv(cor, off ? 0.35 : 1);
				mgraphics.set_line_width(Math.max(1, Math.min(2, passo * 0.9)));
				for (var k = 0; k < n; k++) {
					var q = (s.charCodeAt(k) - 48) / 63;
					if (q <= 0.01) continue;
					var xk = xa + (k + 0.5) * passo;
					var hk = (!off && xk < rodoX ? hD : hA) * q;
					if (hk < 0.5) hk = 0.5;
					mgraphics.move_to(xk, cy - hk); mgraphics.line_to(xk, cy + hk);
				}
				mgraphics.stroke();
			}
			if (c2.falta > 0.06 && !off && xa < rodoX) { rgbv(WARN); mgraphics.set_line_width(1.2); mgraphics.rectangle(xa + 0.5, y + 1.5, w - 1, laneH - 4); mgraphics.stroke(); }
		}
	}
	caixa(X0 - 1, TOP, 1, fundo - TOP, 0.22, 0.23, 0.25);

	// barra de rolagem
	if (maxFirst > 0) {
		var sx = W - SB_W - 2;
		caixa(sx, TOP, SB_W, listH, 0.16, 0.16, 0.18);
		var tth = Math.max(30, listH * vis / tracks.length);
		caixa(sx + 2, TOP + (listH - tth) * first / maxFirst, SB_W - 4, tth, 0.55, 0.55, 0.58);
	}

	// chão molhado + rodo
	if (varrendo && anim.x != null && rodoX > X0 - 40) {
		for (var gk = 0; gk < 16; gk++) {
			var gx0 = rodoX - 160 + gk * 10;
			if (gx0 + 10 < X0) continue;
			caixa(Math.max(X0, gx0), TOP, 10 - Math.max(0, X0 - gx0), fundo - TOP, WATER[0], WATER[1], WATER[2], 0.012 * (gk + 1));
		}
		desenhaRodo(rodoX, TOP - 10, fundo + 6, anim.t);
	}

	rodape(W, H, tracks, varrendo, feito);
}

// botão pequeno do cabeçalho (contorno)
function botaoP(x, y, w, h, txt, cmd, cor) {
	cor = cor || [0.55, 0.55, 0.58];
	rgbv(cor, 0.9); mgraphics.set_line_width(1); mgraphics.rectangle_rounded(x + 0.5, y + 0.5, w - 1, h - 1, 5, 5); mgraphics.stroke();
	rgbv(cor); textoC(txt, x + w / 2, y + h / 2 + 4, 11, true);
	hits.push({ x: x, y: y, w: w, h: h, tipo: "cmd", msg: cmd });
}

function barra(W, varrendo, feito) {
	caixa(0, HEAD_H, W, BAR_H, 0.10, 0.11, 0.12);
	caixa(0, HEAD_H + BAR_H - 1, W, 1, 0.22, 0.23, 0.25);
	// linha 1: SENSOR · TARGET (do SENSOR) · PEAK GUARD
	var y = HEAD_H + 24;
	rgb(0.55, 0.55, 0.58); texto("SENSOR", 16, y + 4, 11, true);
	var x = 72;
	for (var k = 0; k < 3; k++) { botao(x, y - 13, 58, 26, SENS[k], k == cfg.sensor, AC, ["ui_sensor", k]); x += 60; }
	x += 22;
	rgb(0.55, 0.55, 0.58); texto("TARGET", x, y + 4, 11, true);
	var fx = FAIXA[cfg.sensor], sx0 = x + 56, sx1 = sx0 + 258, sv = (alvo() - fx[0]) / (fx[1] - fx[0]), skx = sx0 + sv * (sx1 - sx0);
	caixaR(sx0, y - 4, sx1 - sx0, 8, 4, [0.20, 0.21, 0.23]);
	caixaR(sx0, y - 4, Math.max(8, skx - sx0), 8, 4, AC);
	rgb(0.93, 0.93, 0.93); mgraphics.ellipse(skx - 8, y - 8, 16, 16); mgraphics.fill();
	rgb(0.55, 0.55, 0.58); texto(f1(fx[0]), sx0, y + 19, 9, false); textoD(f1(fx[1]), sx1, y + 19, 9, false);
	hits.push({ x: sx0 - 10, y: y - 14, w: sx1 - sx0 + 20, h: 28, tipo: "target", x0: sx0, x1: sx1, v0: fx[0], v1: fx[1] });
	rgbv(AC); texto(f1(alvo()) + " " + UNID[cfg.sensor], sx1 + 14, y + 6, 16, true);
	x = sx1 + 140;
	// PEAK GUARD só vale em RMS/LUFS (no PEAK o TARGET já é o pico)
	var gOn = cfg.sensor != 0;
	rgb(gOn ? 0.55 : 0.32, gOn ? 0.55 : 0.32, gOn ? 0.58 : 0.34); texto("PEAK GUARD", x, y + 4, 11, true); x += 80;
	if (gOn) {
		botao(x, y - 13, 50, 26, "ON", cfg.guard == 1, GUARDC, ["ui_guard", 1]); x += 52;
		botao(x, y - 13, 50, 26, "OFF", cfg.guard == 0, [0.55, 0.55, 0.58], ["ui_guard", 0]); x += 60;
		rgb(0.55, 0.55, 0.58); texto(cfg.guard ? "o ganho para quando o pico chega a −0.3 dB" : "sem trava: o pico pode passar de 0 dB", x, y + 4, 11, false);
	} else { rgb(0.32, 0.32, 0.34); texto("só em RMS e LUFS", x, y + 4, 11, false); }

	// linha 2: MODE · SCOPE
	y = HEAD_H + 62;
	x = 16;
	rgb(0.55, 0.55, 0.58); texto("MODE", x, y + 4, 11, true); x = 72;
	var modos = ["CLIP", "TRACK", "ALL"];
	for (var m = 0; m < 3; m++) { botao(x, y - 13, 58, 26, modos[m], m == cfg.mode, AC, ["ui_mode", m]); x += 60; }
	x += 22;
	rgb(0.55, 0.55, 0.58); texto("SCOPE", x, y + 4, 11, true); x += 56;
	botao(x, y - 13, 78, 26, "ALL", cfg.scope == 0, AC, ["ui_scope", 0]); x += 82;
	botao(x, y - 13, 86, 26, "SELECTED", cfg.scope == 1, AC, ["ui_scope", 1]);

	// ações: à direita da linha 2
	var yb = y;
	var wR = 98, wAp = 128, wAn = 100, xR = W - 18 - wR, xAp = xR - 8 - wAp, xAn = xAp - 8 - wAn;
	botao(xAn, yb - 15, wAn, 30, "ANALYZE", false, AC, anim ? null : ["analyze"]);
	botao(xAp, yb - 15, wAp, 30, feito ? "APPLIED ✓" : varrendo ? "APPLYING…" : "APPLY", true, feito ? OK : AC, anim ? null : ["apply"], varrendo || feito);
	botao(xR, yb - 15, wR, 30, "RESET", false, AC, anim ? null : ["reset"]);
}

function rodape(W, H, tracks, varrendo, feito) {
	caixa(0, H - FOOT_H, W, FOOT_H, 0.08, 0.09, 0.09);
	var y = H - 15;
	if (varrendo) {
		rgbv(AC); texto("APPLY… " + Math.min(anim.feitos, anim.total) + " de " + anim.total + " clips", 16, y, 13, true);
		return;
	}
	if (feito || (msgAviso && hover < 0)) {
		rgbv(OK); texto(cabeFim(msgAviso || "APPLY FEITO", W - 32, 13, true), 16, y, 13, true);
		return;
	}
	if (!data) return;
	if (hover >= 0 && hover < tracks.length) {
		var tr = tracks[hover];
		rgb(0.91, 0.91, 0.91); texto(cabe(tr.name, W - 420, 13, true), 16, y, 13, true);
		var n = 0, est = {};
		for (var j = 0; j < tr.clips.length; j++) { if (tr.clips[j].state == "ok") n++; else est[tr.clips[j].state] = (est[tr.clips[j].state] || 0) + 1; }
		var fora = []; for (var k in est) if (est.hasOwnProperty(k)) fora.push(est[k] + " " + k);
		rgb(0.62, 0.62, 0.65);
		var lim = 0; for (var j3 = 0; j3 < tr.clips.length; j3++) if (tr.clips[j3].state == "ok" && tr.clips[j3].guardDb > lim) lim = tr.clips[j3].guardDb;
		textoD(n + " clips" + (lim > 0.06 && cfg.sensor && cfg.guard ? " · PEAK GUARD segurou " + lim.toFixed(1) + " dB" : "") + (fora.length ? " · fora: " + fora.join(", ") : "") + (tr.skip ? " · FORA DA NORMALIZAÇÃO" : ""), W - 16, y, 12, false);
		return;
	}
	rgb(0.91, 0.91, 0.91);
	var t = data.nClips ? "ANALYZE OK · ajuste de " + f1(data.dmin) + " a " + f1(data.dmax) + " dB · nada muda no set até o APPLY" : "nenhum clip para normalizar";
	texto(t, 16, y, 13, true);
	if (data.nTeto) { rgbv(WARN); textoD(data.nTeto + (data.nTeto > 1 ? " faixas vão" : " faixa vai") + " bater no teto do Clip Gain", W - 16, y, 12, true); }
	else if (data.nGuard && cfg.sensor && cfg.guard) { rgbv(GUARDC); textoD(data.nGuard + (data.nGuard > 1 ? " faixas param" : " faixa para") + " antes do TARGET: PEAK GUARD (pico em −0.3 dB)", W - 16, y, 12, true); }
	else if (lay && lay.maxFirst > 0) { rgb(0.55, 0.55, 0.58); textoD("FAIXAS " + (first + 1) + "–" + Math.min(tracks.length, first + lay.vis) + " DE " + tracks.length + " · role pela barra ou pela roda do mouse", W - 16, y, 12, false); }
}

// rodo visto de cima: lâmina vertical cobrindo as faixas, cabo para trás (esquerda), gotas sendo empurradas na frente
function desenhaRodo(x, top, bot, t) {
	var mid = (top + bot) / 2;
	var s = 97;
	function r2() { s = (s * 16807) % 2147483647; return s / 2147483647; }
	for (var k = 0; k < 70; k++) {
		var yy = top + 8 + r2() * (bot - top - 16), ddx = 12 + r2() * 16 + Math.sin(t * 9 + k) * 1.5, rr = 1.2 + r2() * 2.4;
		rgbv(WATER, 0.35 + r2() * 0.45); mgraphics.ellipse(x + ddx - rr, yy - rr, rr * 2, rr * 2); mgraphics.fill();
	}
	var hx = x - 14, hy = mid, ex = x - 330, ey = mid - 150;
	try { mgraphics.set_line_cap("round"); } catch (e) { }
	// sombra do cabo
	rgb(0, 0, 0, 0.35); mgraphics.set_line_width(16); mgraphics.move_to(hx + 6, hy + 8); mgraphics.line_to(ex + 6, ey + 8); mgraphics.stroke();
	rgb(0.54, 0.35, 0.17); mgraphics.set_line_width(14); mgraphics.move_to(hx, hy); mgraphics.line_to(ex, ey); mgraphics.stroke();
	rgb(0.69, 0.48, 0.26); mgraphics.set_line_width(5); mgraphics.move_to(hx - 2, hy - 4); mgraphics.line_to(ex + 2, ey - 4); mgraphics.stroke();
	rgb(0.16, 0.17, 0.18); mgraphics.set_line_width(16); mgraphics.move_to(ex + 36, ey + 16); mgraphics.line_to(ex, ey); mgraphics.stroke();
	try { mgraphics.set_line_cap("butt"); } catch (e) { }
	// sombra do corpo
	caixaR(x - 28, mid - 18, 30, 52, 6, [0, 0, 0], 0.35);
	caixaR(x - 8, top + 8, 16, bot - top, 4, [0, 0, 0], 0.3);
	// suporte (encaixe do cabo)
	caixaR(x - 34, mid - 26, 30, 52, 6, [0.85, 0.55, 0.07]);
	// barra da lâmina
	caixaR(x - 14, top, 16, bot - top, 4, AC);
	caixa(x - 11, top + 4, 3, bot - top - 8, 1, 1, 1, 0.35);
	// borracha
	caixaR(x + 2, top + 2, 7, bot - top - 4, 2, [0.07, 0.07, 0.08]);
	caixa(x + 3, top + 4, 1, bot - top - 8, 1, 1, 1, 0.12);
	// marca no suporte
	mgraphics.save();
	mgraphics.translate(x - 19, mid);
	mgraphics.rotate(-Math.PI / 2);
	rgb(0.07, 0.07, 0.08); textoC("RODO", 0, 4, 11, true);
	mgraphics.restore();
}

// ---------------------------------------------------------------- animação (33 ms)
function passoAnim() {
	if (!anim || !data) { tAnim.cancel(); anim = null; return; }
	var dt = 0.033;
	anim.t += dt;
	var sig = data.sig || 4;
	var minBeat = Math.floor(Math.max(0, data.minBeat) / sig) * sig;
	var maxBeat = data.maxBeat > minBeat ? data.maxBeat : minBeat + sig;
	var span = maxBeat - minBeat, margem = span * 0.06;
	if (anim.fimT > 0) {
		if (anim.t - anim.fimT > 2.2) {
			tAnim.cancel(); anim = null;
			if (pendente & 1) load();
			if (pendente & 2) loadEnv();
			pendente = 0;
		}
		mgraphics.redraw();
		return;
	}
	if (anim.x == null) anim.x = minBeat - margem;
	// o rodo vai até o último clip que o motor já fez; com o motor no fim, sai pela direita
	var alvo = anim.motorFim ? maxBeat + margem : anim.alvo;
	var falta = alvo - anim.x;
	if (falta > 0) {
		// no máximo a janela inteira em ~1.6 s, no mínimo em ~6 s (anda suave mesmo com o motor rápido)
		var v = Math.max(span / 6, Math.min(span / 1.6, falta * 3));
		anim.x = Math.min(alvo, anim.x + v * dt);
	}
	if (anim.motorFim && anim.x >= maxBeat + margem - 1e-9) anim.fimT = anim.t;
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
var ultimoEnvio = 0;
function mexeTarget(x, h, final) {
	var v = h.v0 + (h.v1 - h.v0) * Math.max(0, Math.min(1, (x - h.x0) / (h.x1 - h.x0)));
	v = Math.round(v * 10) / 10;
	if (v != alvo()) { poeAlvo(v); mgraphics.redraw(); }
	var t = new Date().getTime();
	if (final || t - ultimoEnvio > 60) { ultimoEnvio = t; outlet(0, "ui_target", alvo()); }
}

function onclick(x, y) {
	arrasto = -1; arrastaTarget = 0;
	for (var b = hits.length - 1; b >= 0; b--) {
		var hb = hits[b];
		if (x < hb.x || x > hb.x + hb.w || y < hb.y || y > hb.y + hb.h) continue;
		if (hb.tipo == "target") { if (anim) return; arrastaTarget = hb; mexeTarget(x, hb, 0); return; }
		if (hb.tipo != "cmd") continue;
		var m = hb.msg;
		if (licCliqueLocal(m)) return;
		if (m[0] == "ui_mode") cfg.mode = m[1];
		else if (m[0] == "ui_scope") cfg.scope = m[1];
		else if (m[0] == "ui_sensor") cfg.sensor = m[1];
		else if (m[0] == "ui_guard") cfg.guard = m[1];
		if (m[0] == "apply" || m[0] == "analyze" || m[0] == "reset") msgAviso = "";
		outlet(0, m);
		mgraphics.redraw();
		return;
	}
	if (!data || anim || !licPronta() || licInfo) return;
	if (lay && lay.maxFirst > 0 && x >= lay.W - SB_W - 4) {
		arrasto = y;
		rolar(lay.maxFirst * (y - TOP) / Math.max(1, lay.listH));
		return;
	}
	for (var i = hits.length - 1; i >= 0; i--) {
		var h = hits[i];
		if (h.tipo != "faixa" || x < h.x || x > h.x + h.w || y < h.y || y > h.y + h.h) continue;
		var tr = data.tracks[h.t];
		tr.skip = tr.skip ? 0 : 1;
		save();
		mgraphics.redraw();
		outlet(0, "marcas");
		return;
	}
}
onclick.local = 1;

function ondrag(x, y, but) {
	if (arrastaTarget) {
		if (!but) { mexeTarget(x, arrastaTarget, 1); arrastaTarget = 0; return; }
		mexeTarget(x, arrastaTarget, 0);
		return;
	}
	if (arrasto < 0 || !lay) return;
	if (!but) { arrasto = -1; return; }
	rolar(lay.maxFirst * (y - TOP) / Math.max(1, lay.listH));
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
