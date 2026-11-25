// © Rogerio Cardoso / InsideStudioSP — todos os direitos reservados. Proibida a cópia, redistribuição ou obra derivada.
// DESENROLA — janela flutuante (jsui). Desenha "desenrola_data": as partes lidas da referência (linha do tempo) e,
// faixa por faixa, os BLOCOS onde ela toca no rascunho (como um mini Arrangement).
// Partes: clique = seleciona · botão direito (ou Ctrl+clique no Mac) = menu: tipo padrão, RENOMEAR, DIVIDIR, TIRAR ·
//         arrastar = muda de lugar · arrastar a borda = tamanho (o resto anda junto; com Alt, a vizinha cede) ·
//         Alt+clique = tira · duplo clique = divide · + PART põe uma de 16 no fim. RENOMEAR abre o [dialog] da janela.
// Blocos: arrastar no vazio = pinta · clique no vazio = pinta do tamanho do PINCEL · arrastar o bloco = move ·
//         arrastar a borda = começo/fim · Alt+clique = apaga. Grade de 4 compassos (Shift = de 1 em 1).
// Grupo: clique = troca o grupo (os blocos da faixa voltam ao padrão do grupo).
// Zoom: − / + / ALL, roda do mouse em cima das partes; roda na horizontal (ou Shift+roda) anda na linha do tempo.
// ANALYZE, BUILD, UNDO, ESTRUTURA e APAGAR REF vão para o motor.

mgraphics.init();
mgraphics.relative_coords = 0;
mgraphics.autofill = 0;
outlets = 1;

var DICT = "desenrola_data";
var VERSAO = "0.3.1";
var msgNova = "";
var HEAD_H = 72, BAR_H = 86, TB_Y = 168, RUL_Y = 200, TL_Y = 216, TL_H = 46, TOP = 272, FOOT_H = 40, SB_W = 14, HS_H = 10;
var XN = 16, XG = 206, X0 = 300;
var LANE_MIN = 18, LANE_MAX = 26;
var AC = [0.98, 0.62, 0.20], OK = [0.37, 0.75, 0.48], WARN = [0.88, 0.33, 0.31], CINZA = [0.55, 0.55, 0.58];
var PARTES = ["INTRO", "BREAK", "BUILDUP", "DROP", "FINAL"];
var CORES_PARTE = [0x6fa8dc, 0x8e7cc3, 0xf6b26b, 0xe06666, 0x76a5af];
var GRUPOS = ["KICK", "BASS", "DRUMS", "HATS", "SYNTHS", "AMBIENCE", "VOCAL", "FX", "OTHER"];
var CORES = [0xff8a3d, 0x45a6ff, 0xf5c451, 0xe6e1f5, 0xc9b0ff, 0x4fd1c5, 0xff7fb5, 0xc4e86b, 0x9a9aa0];
var PINCEIS = [4, 8, 16, 32, 0];   // 0 = a parte inteira
var cfg = { est: 1, apaga: 1 };
var data = null;
var msgStatus = "", msgAviso = "";
var hits = [];
var first = 0, lay = null, arrasto = -1, hover = -1, hov = null;
var op = null;         // arrasto em andamento (parte, borda, bloco, pintura, barra de rolagem)
var prog = null;       // { feitos, total }
var vista = { s: 0, n: 64 };   // compassos visíveis: de s até s+n
var pincel = 0;
var sel = 0;           // id da parte selecionada
var menu = null;       // menu do botão direito: { x, y, id, itens, hov }
var renomeando = 0;    // id da parte que espera o nome do [dialog]
var MENU_W = 176, MENU_IH = 24;

function arr(x) { if (x == null) return []; return Array.isArray(x) ? x : [x]; }
function load() {
	var d = new Dict(DICT);
	try { data = JSON.parse(d.stringify()); } catch (e) { data = null; }
	if (data && !data.tracks) data = null;
	if (data) {
		data.tracks = arr(data.tracks);
		data.partes = arr(data.partes);
		if (data.ref) data.ref.partes = arr(data.ref.partes);
		if (data.modelo) data.modelo = arr(data.modelo);
		var maxId = 0;
		for (var p = 0; p < data.partes.length; p++) { if (!data.partes[p].id) data.partes[p].id = 1000 + p; maxId = Math.max(maxId, data.partes[p].id); }
		data.proxId = Math.max(Number(data.proxId) || 0, maxId + 1);
		for (var i = 0; i < data.tracks.length; i++) {
			var t = data.tracks[i];
			t.clips = arr(t.clips);
			t.g = t.g == null ? 8 : Number(t.g);
			if (t.blocos == null) t.blocos = data.livre ? [] : blocosDaRegra(t.g, data.partes);
			t.blocos = arr(t.blocos);
		}
		if (data.cfg) cfg = data.cfg;
	}
}
function save() { var d = new Dict(DICT); d.parse(JSON.stringify(data)); }
function mudou() { save(); outlet(0, "marcas"); mgraphics.redraw(); }
function ajusta() { vista.s = 0; vista.n = Math.max(8, total()); }

// mensagens do motor
function bang() { menu = null; load(); ajusta(); mgraphics.redraw(); }
function loadbang() { load(); ajusta(); mgraphics.redraw(); }
function status() { msgStatus = arrayfromargs(arguments).join(" "); mgraphics.redraw(); }
function aviso() { msgAviso = arrayfromargs(arguments).join(" "); prog = null; mgraphics.redraw(); }
function varre(total) { msgAviso = ""; prog = { feitos: 0, total: total }; mgraphics.redraw(); }
function progresso(i, total) { if (!prog) prog = {}; prog.feitos = i; prog.total = total; mgraphics.redraw(); }
function fim() { prog = null; mgraphics.redraw(); }
function valores(e, a) { cfg = { est: e | 0, apaga: a == null ? 1 : a | 0 }; mgraphics.redraw(); }
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
function corInt(c) { c = c | 0; if (c <= 0) return [0.48, 0.6500000000000000609796, 1]; return [((c >> 16) & 255) / 255, ((c >> 8) & 255) / 255, (c & 255) / 255]; }
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

// ---------------------------------------------------------------- partes e blocos (modelo)
var REGRAS = {
	INTRO:   [1, 0, 1, 1, 0, 1, 0, 0, 0],
	BREAK:   [0, 0, 0, 0, 1, 1, 1, 0, 1],
	BUILDUP: [0, 0, 1, 1, 1, 1, 1, 1, 0],
	DROP:    [1, 1, 1, 1, 1, 0, 1, 0, 1],
	FINAL:   [1, 0, 1, 1, 0, 1, 0, 0, 0]
};
function total() { var n = 0; if (data) for (var i = 0; i < data.partes.length; i++) n += data.partes[i].n; return n; }
function inicios(partes) { var o = [], pos = 0; for (var i = 0; i < partes.length; i++) { o.push(pos); pos += partes[i].n; } return o; }
function toca(g, tipo) { return REGRAS[tipo] && REGRAS[tipo][g < 0 ? 8 : g] ? 1 : 0; }
function blocosDaRegra(g, partes) {
	var out = [], pos = 0;
	for (var i = 0; i < partes.length; i++) { if (toca(g, partes[i].p)) out.push({ a: pos, b: pos + partes[i].n }); pos += partes[i].n; }
	return out;
}
// "DROP 1", "DROP 2"… (os outros tipos sem número); parte renomeada mostra o nome dela
function rotulos(partes) {
	var out = [], nd = 0, tot = 0, i;
	for (i = 0; i < partes.length; i++) if (partes[i].p == "DROP" && !partes[i].nome) tot++;
	for (i = 0; i < partes.length; i++) {
		if (partes[i].nome) out.push(String(partes[i].nome));
		else out.push(partes[i].p == "DROP" && tot > 1 ? "DROP " + (++nd) : partes[i].p);
	}
	return out;
}
// ordena e junta o que se sobrepõe (blocos que só encostam ficam separados: o loop recomeça no começo de cada bloco)
function arruma(bl) {
	var tot = total(), v = [];
	for (var i = 0; i < bl.length; i++) {
		var a = Math.max(0, bl[i].a), b = Math.min(tot, bl[i].b);
		if (b - a >= 0.25) v.push({ a: a, b: b });
	}
	v.sort(function (x, y) { return x.a - y.a || x.b - y.b; });
	var o = [];
	for (var j = 0; j < v.length; j++) {
		var u = o[o.length - 1];
		if (u && v[j].a < u.b - 1e-6) u.b = Math.max(u.b, v[j].b); else o.push(v[j]);
	}
	return o;
}
function copia(x) { return JSON.parse(JSON.stringify(x)); }
// as partes mudaram (tamanho, ordem, tipo, tiradas, novas): os blocos andam com a parte em que estão.
// Pedaço que ia até o fim da parte continua indo até o fim; parte nova ou que trocou de tipo recebe o padrão do grupo.
function remapeia(velhasP, velhosT, novasP) {
	var iniV = inicios(velhasP), iniN = inicios(novasP), porId = {};
	for (var i = 0; i < velhasP.length; i++) porId[velhasP[i].id] = { s: iniV[i], n: velhasP[i].n, p: velhasP[i].p };
	for (var t = 0; t < data.tracks.length; t++) {
		var tr = data.tracks[t], velhos = velhosT[t] || [], novos = [], comeco = {};
		for (var k = 0; k < novasP.length; k++) {
			var np = novasP[k], S = iniN[k], N = np.n, v = porId[np.id];
			if (!v || v.p != np.p) { if (toca(tr.g, np.p)) { novos.push({ a: S, b: S + N }); comeco[Math.round(S * 1000)] = 1; } continue; }
			for (var b = 0; b < velhos.length; b++) {
				var a0 = Math.max(velhos[b].a, v.s), b0 = Math.min(velhos[b].b, v.s + v.n);
				if (b0 <= a0 + 1e-6) continue;
				var oa = a0 - v.s, ob = b0 - v.s;
				var na = Math.min(oa, N), nb = Math.abs(ob - v.n) < 1e-6 ? N : Math.min(ob, N);
				if (nb <= na + 1e-6) continue;
				novos.push({ a: S + na, b: S + nb });
				if (Math.abs(a0 - velhos[b].a) < 1e-6) comeco[Math.round((S + na) * 1000)] = 1;   // aqui começava um bloco
			}
		}
		// pedaços colados de um bloco que atravessava partes voltam a ser um bloco só; começo de bloco fica separado
		var o = arruma(novos), r = [];
		for (var j = 0; j < o.length; j++) {
			var u = r[r.length - 1];
			if (u && Math.abs(o[j].a - u.b) < 1e-6 && !comeco[Math.round(o[j].a * 1000)]) u.b = o[j].b; else r.push(o[j]);
		}
		tr.blocos = r;
	}
}
function fotoT() { var o = []; for (var t = 0; t < data.tracks.length; t++) o.push(copia(data.tracks[t].blocos)); return o; }

// ---------------------------------------------------------------- vista (zoom / rolagem horizontal)
function x1() { return (lay ? lay.W : 1360) - SB_W - 10; }
function K() { return (x1() - X0) / Math.max(1, vista.n); }
function xDe(bar) { return X0 + (bar - vista.s) * K(); }
function barDe(x) { return vista.s + (x - X0) / K(); }
function limitaVista() {
	var tot = Math.max(8, total());
	vista.n = Math.max(4, Math.min(tot, vista.n));
	vista.s = Math.max(0, Math.min(tot - vista.n, vista.s));
}
function zoom(fator, xc) {
	var bc = xc == null ? vista.s + vista.n / 2 : barDe(xc);
	var n = vista.n * fator, f = xc == null ? 0.5 : (xc - X0) / (x1() - X0);
	vista.n = n; vista.s = bc - f * n;
	limitaVista();
	mgraphics.redraw();
}
// trecho [a, b) em compassos → [xa, xb] recortado na área visível (null se não aparece)
function faixaX(a, b) {
	var xa = Math.max(X0, xDe(a)), xb = Math.min(x1(), xDe(b));
	return xb > xa + 0.5 ? [xa, xb] : null;
}

// ---------------------------------------------------------------- pintura
function paint() {
	var W = box.rect[2] - box.rect[0], H = box.rect[3] - box.rect[1];
	hits = [];
	lay = lay || {};
	lay.W = W; lay.H = H;
	caixa(0, 0, W, H, 0.120000000000000000415119, 0.12, 0.13);
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
	limitaVista();
	ferramentas(W);
	linhaTempo(W);
	faixas(W, H);
	rodape(W, H);
	desenhaMenu(W, H);
}

// ---------------------------------------------------------------- menu do botão direito numa parte
function abreMenu(x, y, i) {
	var p = data.partes[i], itens = [];
	for (var k = 0; k < PARTES.length; k++) itens.push({ t: PARTES[k], tipo: PARTES[k], on: p.p == PARTES[k] && !p.nome });
	itens.push({ sep: 1 });
	itens.push({ t: "RENOMEAR…", acao: "ren" });
	itens.push({ t: "DIVIDIR", acao: "div" });
	if (data.partes.length > 1) itens.push({ t: "TIRAR", acao: "tira" });
	menu = { x: x, y: y, id: p.id, itens: itens, hov: -1 };
	sel = p.id;
	mgraphics.redraw();
}
function idxDaParte(id) { for (var i = 0; i < data.partes.length; i++) if (data.partes[i].id == id) return i; return -1; }
function desenhaMenu(W, H) {
	if (!menu || !data) return;
	var i = idxDaParte(menu.id);
	if (i < 0) { menu = null; return; }
	var TIT = 28, h = TIT + 6, k;
	for (k = 0; k < menu.itens.length; k++) h += menu.itens[k].sep ? 9 : MENU_IH;
	var x = Math.max(4, Math.min(W - MENU_W - 4, menu.x)), y = Math.max(4, Math.min(H - h - 4, menu.y));
	caixaR(x + 3, y + 4, MENU_W, h, 6, [0, 0, 0], 0.45);
	caixaR(x, y, MENU_W, h, 6, [0.17, 0.1700000000000000219294, 0.19]);
	contorno(x, y, MENU_W, h, 6, [0.36, 0.36, 0.4], 1);
	var cp = corP(data.partes[i].p);
	caixaR(x + 8, y + 9, 10, 10, 2, cp);
	rgb(0.91, 0.91, 0.91); texto(cabeFim(rotulos(data.partes)[i], MENU_W - 34, 12, true), x + 24, y + 19, 12, true);
	caixa(x + 6, y + TIT, MENU_W - 12, 1, 0.3, 0.3, 0.33);
	var yy = y + TIT + 3;
	for (k = 0; k < menu.itens.length; k++) {
		var it = menu.itens[k];
		if (it.sep) { caixa(x + 6, yy + 4, MENU_W - 12, 1, 0.3, 0.3, 0.33); yy += 9; continue; }
		if (menu.hov == k) caixaR(x + 4, yy, MENU_W - 8, MENU_IH, 4, AC, 0.9);
		if (it.tipo) { caixaR(x + 12, yy + 7, 10, 10, 2, corP(it.tipo)); }
		if (menu.hov == k) rgb(0.07, 0.07, 0.08); else if (it.acao == "tira") rgbv(WARN); else rgb(0.91, 0.91, 0.91);
		texto(it.t, x + (it.tipo ? 30 : 14), yy + 16, 12, true);
		if (it.on) { if (menu.hov == k) rgb(0.07, 0.07, 0.08); else rgbv(AC); textoD("✓", x + MENU_W - 12, yy + 16, 12, true); }
		hits.push({ x: x + 4, y: yy, w: MENU_W - 8, h: MENU_IH, tipo: "menu", k: k });
		yy += MENU_IH;
	}
	hits.push({ x: x, y: y, w: MENU_W, h: h, tipo: "menuFundo" });
}
function aplicaMenu(it) {
	var id = menu.id, i = idxDaParte(id);
	menu = null;
	if (i < 0) { mgraphics.redraw(); return; }
	var P = data.partes, p = P[i];
	if (it.tipo) { trocaTipo(i, it.tipo); return; }
	if (it.acao == "ren") { renomeando = id; outlet(0, "dlg_nome"); mgraphics.redraw(); return; }
	if (it.acao == "div") { divide(i); return; }
	if (it.acao == "tira" && P.length > 1) { var vP = copia(P), vT = fotoT(); P.splice(i, 1); remapeia(vP, vT, P); if (sel == id) sel = 0; mudou(); return; }
	mgraphics.redraw();
}
// tipo padrão escolhido no menu: tira o nome próprio; se o tipo mudou, as faixas dessa parte vão para o padrão do tipo novo
function trocaTipo(i, tipo) {
	var P = data.partes, p = P[i];
	if (p.p == tipo && !p.nome) { mgraphics.redraw(); return; }
	delete p.nome;
	if (p.p != tipo) {
		var vP = copia(P), vT = fotoT();
		p.p = tipo;
		p.id = data.proxId++;   // id novo = tipo novo: o remapeia dá o padrão do grupo só nessa parte
		sel = p.id;
		remapeia(vP, vT, P);
	}
	mudou();
}
// o [dialog] da janela devolve o nome digitado (vazio = volta ao nome do tipo)
function nome_parte() {
	// o [dialog] manda "symbol <texto>" (ou "list ..."): a primeira palavra é o tipo da mensagem do Max, não o nome
	var a = arrayfromargs(arguments);
	while (a.length && (String(a[0]) == "symbol" || String(a[0]) == "list")) a.shift();
	var s = a.join(" ").replace(/^\s+|\s+$/g, "").substring(0, 24);
	var id = renomeando;
	renomeando = 0;
	if (!data || !id) return;
	var i = idxDaParte(id);
	if (i < 0) return;
	if (s && s.toUpperCase() != data.partes[i].p) data.partes[i].nome = s; else delete data.partes[i].nome;
	mudou();
}
function divide(i) {
	var p = data.partes[i];
	if (p.n < 2) { mgraphics.redraw(); return; }
	var n1 = p.n >= 8 ? Math.round(p.n / 8) * 4 : Math.floor(p.n / 2);
	if (n1 < 1 || n1 >= p.n) n1 = Math.floor(p.n / 2);
	// dividir não mexe em nada no tempo: os blocos ficam onde estão
	data.partes.splice(i + 1, 0, { id: data.proxId++, p: p.p, n: p.n - n1 });
	p.n = n1; p.a = null; p.b = null;
	mudou();
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
	} else if (data && data.livre) {
		rgb(0.55, 0.55, 0.58); texto("REFERENCE", 16, y + 4, 11, true);
		rgbv(AC); var tl = "MODO LIVRE · sem faixa REF: monte as partes e pinte onde cada faixa toca";
		texto(tl, 100, y + 4, 12, true);
		var xl = 100 + largura(tl, 12, true) + 30;
		rgb(0.55, 0.55, 0.58); texto("LOOP", xl, y + 4, 11, true);
		rgb(0.91, 0.91, 0.91); texto(Math.round(data.loop.bars * 100) / 100 + " compassos · " + data.tracks.length + " faixas", xl + 40, y + 4, 12, true);
	} else { rgb(0.55, 0.55, 0.58); texto("REFERENCE  —", 16, y + 4, 11, true); }

	y = HEAD_H + 62;
	var x2 = 16;
	rgb(0.55, 0.55, 0.58); texto("ESTRUTURA", x2, y + 4, 11, true); x2 += 74;
	botao(x2, y - 13, 44, 26, "ON", cfg.est == 1, AC, ["ui_est", 1]); x2 += 46;
	botao(x2, y - 13, 44, 26, "OFF", cfg.est == 0, CINZA, ["ui_est", 0]); x2 += 64;
	rgb(0.55, 0.55, 0.58); texto("APAGAR REF", x2, y + 4, 11, true); x2 += 80;
	botao(x2, y - 13, 44, 26, "ON", cfg.apaga == 1, AC, ["ui_apaga", 1]); x2 += 46;
	botao(x2, y - 13, 44, 26, "OFF", cfg.apaga == 0, CINZA, ["ui_apaga", 0]); x2 += 64;
	var wU = 90, wB = 150, wA = 110, xU = W - 18 - wU, xB = xU - 8 - wB, xA = xB - 8 - wA;
	var feito = data && data.feito;
	botao(xA, y - 15, wA, 30, "ANALYZE", false, AC, prog ? null : ["analyze"]);
	botao(xB, y - 15, wB, 30, prog ? "DESENROLANDO…" : feito ? "BUILT ✓" : "BUILD", !!data, feito ? OK : AC, prog || !data ? null : ["build"], !!prog);
	botao(xU, y - 15, wU, 30, "UNDO", false, AC, prog ? null : ["undo"]);
}

function ferramentas(W) {
	var y = TB_Y, x = 16;
	rgb(0.91, 0.91, 0.91); texto(data.partes.length + " partes · " + total() + " compassos", x, y + 17, 13, true);
	if (data.inicio != null && data.bar) { rgb(0.55, 0.55, 0.58); texto("começa no compasso " + (Math.round(data.inicio / data.bar) + 1), x, y + 31, 10, false); }
	x = X0;
	if (!prog) { botaoP(x, y + 2, 76, 24, "+ PART", ["__mais"]); x += 84; botaoP(x, y + 2, 76, 24, "RESET", ["__reset"]); x += 100; }
	rgb(0.55, 0.55, 0.58); texto("PINCEL", x, y + 19, 11, true); x += 50;
	for (var i = 0; i < PINCEIS.length; i++) {
		var lb = PINCEIS[i] ? PINCEIS[i] + "" : "PARTE", w = PINCEIS[i] ? 34 : 56;
		botao(x, y + 2, w, 24, lb, pincel == PINCEIS[i], AC, ["__pincel", PINCEIS[i]]); x += w + 3;
	}
	x += 24;
	rgb(0.55, 0.55, 0.58); texto("ZOOM", x, y + 19, 11, true); x += 42;
	botao(x, y + 2, 30, 24, "−", false, AC, ["__zoom", 1.5]); x += 33;
	botao(x, y + 2, 30, 24, "+", false, AC, ["__zoom", 1 / 1.5]); x += 33;
	botao(x, y + 2, 44, 24, "ALL", false, AC, ["__zoom", 0]); x += 60;
	rgb(0.45, 0.45, 0.48); texto("compassos " + (Math.floor(vista.s) + 1) + "–" + Math.round(vista.s + vista.n), x, y + 19, 11, false);
}

function vazio(W, H) {
	var y = TB_Y + 20;
	rgb(0.91, 0.91, 0.91); texto("Como usar", 16, y, 18, true);
	var passos = [
		"1.  No Arrangement, deixe o seu loop com todas as faixas (kick, bass, drums, synths…) e marque o loop brace em volta dele.",
		"2.  Crie uma faixa de áudio chamada REF e arraste a música de referência para ela (fora do loop). Warp ligado ajuda no andamento.",
		"     Sem REF: o ANALYZE abre o MODO LIVRE, com um esqueleto de partes para você mudar e as faixas vazias para você pintar.",
		"3.  Clique ANALYZE: o DESENROLA lê as partes da referência (INTRO, BREAK, BUILDUP, DROP, FINAL) e as faixas do loop.",
		"4.  Monte a música na janela: botão direito numa parte troca o tipo ou renomeia; arraste partes e blocos. BUILD: o rascunho vem depois do loop,",
		"     com a faixa ESTRUTURA (um clip com nome e cor por parte). Com APAGAR REF ON, a faixa REF sai no fim.",
		"5.  Não gostou? UNDO apaga o rascunho e a faixa ESTRUTURA e traz a REF de volta. O loop original nunca é mexido."
	];
	for (var i = 0; i < passos.length; i++) { rgb(0.7, 0.7, 0.72); texto(passos[i], 16, y + 34 + i * 26, 14, false); }
}

function linhaTempo(W) {
	var X1 = x1(), y = TL_Y, ini = inicios(data.partes), rot = rotulos(data.partes), tot = total();
	// régua
	var step = 1; while (vista.n / step > 32) step *= 2;
	for (var b = Math.ceil(vista.s / step) * step; b <= vista.s + vista.n; b += step) {
		var xr = xDe(b);
		if (xr < X0 - 0.5 || xr > X1 + 0.5) continue;
		caixa(xr, RUL_Y + 6, 1, 8, 0.35, 0.35, 0.38);
		if (xr < X1 - 14) { rgb(0.5, 0.5, 0.53); texto(b + 1, xr + 2, RUL_Y + 12, 9, false); }
	}
	rgb(0.55, 0.55, 0.58); texto("PARTES", 16, y + 10, 11, true);
	rgb(0.45, 0.45, 0.48);
	texto("seta do canto / botão direito = menu", 16, y + 23, 10, false);
	texto("arraste = muda de lugar · borda = tamanho", 16, y + 34, 10, false);
	texto("Alt: vizinha cede · duplo clique = divide", 16, y + 45, 10, false);
	for (var i = 0; i < data.partes.length; i++) {
		var p = data.partes[i], c = corP(p.p), fx = faixaX(ini[i], ini[i] + p.n);
		var arrastando = op && op.tipo == "pMove" && op.id == p.id && op.mexeu;
		if (fx) {
			var w = fx[1] - fx[0], hv = hov && hov.tipo == "parte" && hov.i == i && !prog;
			caixaR(fx[0] + 1, y, Math.max(2, w - 2), TL_H, 4, c, arrastando ? 0.55 : hv ? 1 : 0.85);
			if (arrastando || p.id == sel) contorno(fx[0] + 1, y, Math.max(2, w - 2), TL_H, 4, [1, 1, 1], 0.9, 2);
			if (w > 26) {
				tinta(c);
				texto(cabeFim(rot[i], w > 44 && !arrastando ? w - 32 : w - 10, 12, true), fx[0] + 6, y + 17, 12, true);   // não passa por baixo da seta
				texto(cabeFim(p.n + (w > 90 ? " compassos" : ""), w - 10, 11, false), fx[0] + 6, y + 32, 11, false);
				if (p.a != null && p.b != null && w > 70) { rgb(0, 0, 0, 0.45); texto(cabeFim("ref " + (p.a + 1) + "–" + p.b, w - 10, 9, false), fx[0] + 6, y + 43, 9, false); }
			}
			hits.push({ x: fx[0], y: y, w: w, h: TL_H, tipo: "parte", i: i });
			// botão ▾ no canto da parte: abre o mesmo menu do botão direito (no Mac o botão direito nem sempre chega ao jsui)
			if (w > 44 && !arrastando) {
				var bx = fx[1] - 22, by = y + 4, hvm = hov && hov.tipo == "pmenu" && hov.i == i;
				caixaR(bx, by, 18, 16, 3, [0, 0, 0], hvm ? 0.55 : 0.30000000000000000339445);
				rgb(1, 1, 1, hvm ? 1 : 0.85);
				mgraphics.move_to(bx + 5, by + 6); mgraphics.line_to(bx + 13, by + 6); mgraphics.line_to(bx + 9, by + 11); mgraphics.line_to(bx + 5, by + 6); mgraphics.fill();
				hits.push({ x: bx - 2, y: by - 2, w: 22, h: 20, tipo: "pmenu", i: i });
			}
		}
		// borda direita (fim da parte i)
		var xb = xDe(ini[i] + p.n);
		if (xb >= X0 - 4 && xb <= X1 + 4) {
			hits.push({ x: xb - 5, y: y, w: 10, h: TL_H, tipo: "borda", i: i });
			if (hov && hov.tipo == "borda" && hov.i == i) caixa(xb - 2, y - 3, 4, TL_H + 6, 1, 1, 1, 0.9);
		}
	}
	// fim do rascunho (dá para puxar a última parte)
	var xf = xDe(tot);
	if (xf < X1 - 2) { rgb(0.45, 0.45, 0.48); texto("fim", xf + 6, y + 28, 10, false); }
}

function faixas(W, H) {
	var tracks = data.tracks, X1 = x1(), ini = inicios(data.partes);
	caixa(0, TOP - 6, W, 1, 0.22, 0.23, 0.25);
	var listH = H - TOP - FOOT_H - HS_H - 8;
	var laneH = Math.max(LANE_MIN, Math.min(LANE_MAX, Math.floor(listH / Math.max(1, tracks.length))));
	var vis = Math.max(1, Math.floor(listH / laneH));
	var maxFirst = Math.max(0, tracks.length - vis);
	if (first > maxFirst) first = maxFirst;
	var last = Math.min(tracks.length, first + vis);
	var fundo = TOP + (last - first) * laneH;
	lay.listH = listH; lay.vis = vis; lay.n = tracks.length; lay.maxFirst = maxFirst; lay.laneH = laneH;
	var fs = Math.max(11, Math.min(13, laneH - 7));

	// linhas das partes e do fim
	for (var q = 0; q <= data.partes.length; q++) {
		var xq = xDe(q < data.partes.length ? ini[q] : total());
		if (xq >= X0 && xq <= X1) caixa(xq, TOP, 1, fundo - TOP, 0.26, 0.26, 0.3);
	}
	for (var i = first; i < last; i++) {
		var tr = tracks[i];
		var yl = TOP + (i - first) * laneH, cy = yl + laneH / 2;
		if (i == hover && !prog) caixa(0, yl, W - SB_W - 4, laneH - 1, 0.17, 0.17, 0.20000000000000000383284);
		else caixa(0, yl, X0 - 8, laneH - 1, i % 2 ? 0.15 : 0.135, i % 2 ? 0.15 : 0.137, i % 2 ? 0.17 : 0.15);
		var ct = corInt(tr.color);
		caixa(6, yl + 3, 4, laneH - 7, ct[0], ct[1], ct[2]);
		rgb(0.91, 0.91, 0.91);
		texto(cabe((tr.midi ? "♪ " : "") + tr.name, XG - XN - 14, fs - 1, true), XN + 4, cy + fs / 2 - 2, fs - 1, true);
		var cg = corG(tr.g), wc = 82;
		caixaR(XG, cy - 9, wc, 18, 4, cg); tinta(cg);
		textoC(GRUPOS[tr.g], XG + wc / 2, cy + 4, 10, true);
		hits.push({ x: XG, y: cy - 9, w: wc, h: 18, tipo: "grupo", t: i });
		// trilha: vazio (pinta) + blocos
		if (!prog) hits.push({ x: X0, y: yl, w: X1 - X0, h: laneH, tipo: "trilha", t: i });
		var bl = tr.blocos;
		for (var b = 0; b < bl.length; b++) {
			var fx = faixaX(bl[b].a, bl[b].b);
			if (!fx) continue;
			var w = fx[1] - fx[0], hv = hov && (hov.tipo == "bloco" || hov.tipo == "bE" || hov.tipo == "bD") && hov.t == i && hov.j == b;
			caixa(fx[0] + 1, yl + 3, Math.max(1, w - 2), laneH - 7, ct[0], ct[1], ct[2], hv ? 0.85 : 0.6);
			caixa(fx[0] + 1, yl + 3, Math.max(1, w - 2), 2, ct[0], ct[1], ct[2], 1);
			if (hv && hov.tipo != "bloco") caixa(hov.tipo == "bE" ? fx[0] + 1 : fx[1] - 3, yl + 2, 2, laneH - 5, 1, 1, 1, 0.95);
			if (prog) continue;
			var e = Math.min(6, w / 3);
			hits.push({ x: fx[0], y: yl, w: w, h: laneH, tipo: "bloco", t: i, j: b });
			if (xDe(bl[b].a) >= X0 - 1) hits.push({ x: fx[0] - 2, y: yl, w: e + 2, h: laneH, tipo: "bE", t: i, j: b });
			if (xDe(bl[b].b) <= X1 + 1) hits.push({ x: fx[1] - e, y: yl, w: e + 2, h: laneH, tipo: "bD", t: i, j: b });
		}
	}
	if (prog && prog.total) {
		var xp = X0 + (X1 - X0) * Math.min(1, prog.feitos / prog.total);
		caixa(xp, TOP, X1 - xp, fundo - TOP, 0.12, 0.12, 0.13, 0.75);
		caixa(xp - 1, TOP - 4, 3, fundo - TOP + 8, AC[0], AC[1], AC[2], 1);
	}
	if (maxFirst > 0) {
		var sx = W - SB_W - 2;
		caixa(sx, TOP, SB_W, listH, 0.16, 0.16, 0.18);
		var tth = Math.max(30, listH * vis / tracks.length);
		caixa(sx + 2, TOP + (listH - tth) * first / maxFirst, SB_W - 4, tth, 0.55, 0.55, 0.58);
	}
	// barra de rolagem horizontal
	var tot = Math.max(8, total()), hy = H - FOOT_H - HS_H - 4;
	lay.hy = hy;
	caixa(X0, hy, X1 - X0, HS_H, 0.16, 0.16, 0.18);
	if (vista.n < tot - 1e-6) {
		var hw = Math.max(30, (X1 - X0) * vista.n / tot), hx = X0 + (X1 - X0 - hw) * vista.s / (tot - vista.n);
		caixa(hx, hy + 2, hw, HS_H - 4, 0.55, 0.55, 0.58000000000000000831769);
		hits.push({ x: X0, y: hy - 2, w: X1 - X0, h: HS_H + 4, tipo: "hs", hw: hw });
	}
}

function rodape(W, H) {
	caixa(0, H - FOOT_H, W, FOOT_H, 0.08, 0.09, 0.09);
	var y = H - 15;
	if (prog) { rgbv(AC); texto("BUILD… " + (prog.feitos || 0) + " de " + (prog.total || 0) + " cópias de clip", 16, y, 13, true); return; }
	if (msgAviso && hover < 0) { rgbv(OK); texto(cabeFim(msgAviso, W - 32, 13, true), 16, y, 13, true); return; }
	if (!data) return;
	if (hover >= 0 && hover < data.tracks.length) {
		var tr = data.tracks[hover], soma = 0;
		for (var b = 0; b < tr.blocos.length; b++) soma += tr.blocos[b].b - tr.blocos[b].a;
		rgb(0.91, 0.91, 0.91); texto(cabe(tr.name, W - 620, 13, true), 16, y, 13, true);
		rgb(0.62, 0.62, 0.65);
		textoD((tr.midi ? "MIDI · " : "áudio · ") + tr.clips.length + " clips no loop · grupo " + GRUPOS[tr.g] + (tr.auto ? " (pelo nome)" : "") + " · " +
			tr.blocos.length + " blocos, " + Math.round(soma) + " compassos" + (tr.manual ? " · editada à mão" : ""), W - 16, y, 12, false);
		return;
	}
	rgb(0.75, 0.75, 0.78);
	texto("faixas: arraste no vazio = pinta · clique no vazio = PINCEL · arraste o bloco = move · borda = começo/fim · Alt+clique = apaga · Shift = de 1 em 1 · grupo: clique troca", 16, y, 12, false);
	if (lay && lay.maxFirst > 0) { rgb(0.55, 0.5500000000000000957869, 0.58); textoD("FAIXAS " + (first + 1) + "–" + Math.min(data.tracks.length, first + lay.vis) + " DE " + data.tracks.length, W - 16, y, 12, false); }
}

// ---------------------------------------------------------------- comandos locais
function arred(n) { var r8 = Math.round(n / 8) * 8; if (r8 >= 8 && Math.abs(n - r8) <= 1) return r8; return Math.max(4, Math.round(n / 4) * 4); }
function cmdLocal(m) {
	if (m[0] == "__pincel") { pincel = Number(m[1]) || 0; mgraphics.redraw(); return true; }
	if (m[0] == "__zoom") { if (!Number(m[1])) { ajusta(); mgraphics.redraw(); } else zoom(Number(m[1])); return true; }
	if (m[0] == "__mais") {
		var vP = copia(data.partes), vT = fotoT();
		data.partes.push({ id: data.proxId++, p: "BREAK", n: 16 });
		remapeia(vP, vT, data.partes);
		var tot = total();
		if (tot > vista.s + vista.n) { if (vista.n >= tot - 16) vista.n = tot; vista.s = tot - vista.n; }
		mudou(); return true;
	}
	if (m[0] == "__reset") {
		if (!data.ref) {
			// modo livre: volta ao esqueleto e limpa os blocos
			if (!data.modelo) return true;
			data.partes = [];
			for (var i0 = 0; i0 < data.modelo.length; i0++) data.partes.push({ id: data.proxId++, p: data.modelo[i0].p, n: data.modelo[i0].n });
			for (var t0 = 0; t0 < data.tracks.length; t0++) { data.tracks[t0].blocos = []; data.tracks[t0].manual = 0; }
			ajusta();
			mudou(); return true;
		}
		data.partes = [];
		for (var i = 0; i < data.ref.partes.length; i++) { var r = data.ref.partes[i]; data.partes.push({ id: data.proxId++, p: r.p, n: arred(r.b - r.a), a: r.a, b: r.b }); }
		for (var t = 0; t < data.tracks.length; t++) { data.tracks[t].blocos = blocosDaRegra(data.tracks[t].g, data.partes); data.tracks[t].manual = 0; }
		ajusta();
		mudou(); return true;
	}
	return false;
}

// ---------------------------------------------------------------- mouse
function acha(x, y, tipos) {
	for (var b = hits.length - 1; b >= 0; b--) {
		var hb = hits[b];
		if (x < hb.x || x > hb.x + hb.w || y < hb.y || y > hb.y + hb.h) continue;
		if (!tipos || tipos.indexOf(hb.tipo) >= 0) return hb;
	}
	return null;
}
// prioridade: botões, bordas (parte e bloco), depois o corpo
function alvo(x, y) {
	return acha(x, y, ["cmd"]) || acha(x, y, ["pmenu"]) || acha(x, y, ["borda", "bE", "bD"]) || acha(x, y, ["parte", "bloco", "grupo", "hs", "trilha"]);
}
function grade(shift) { return shift ? 1 : 4; }
function parteEm(bar) {
	var ini = inicios(data.partes);
	for (var i = 0; i < data.partes.length; i++) if (bar >= ini[i] && bar < ini[i] + data.partes[i].n) return { i: i, s: ini[i], n: data.partes[i].n };
	return null;
}

// Max chama onclick(x, y, botão, cmd, shift, capslock, option, ctrl)
function onclick(x, y, but, cmd, shift, caps, opt, ctrl) {
	if (op) solta();
	arrasto = -1;
	if (menu) {
		// menu aberto: clique num item aplica; em qualquer outro lugar só fecha
		var hm = acha(x, y, ["menu"]);
		if (hm && !menu.itens[hm.k].sep) aplicaMenu(menu.itens[hm.k]);
		else { menu = null; mgraphics.redraw(); }
		return;
	}
	// botão direito: o Max manda ctrl = 1 (botão direito no Windows, Ctrl+clique/botão direito no Mac)
	var direito = !!ctrl || but > 1;
	var hb = alvo(x, y), apaga = !!(opt || ctrl);
	if (hb && hb.tipo == "cmd") {
		var m = hb.msg;
		if (licCliqueLocal(m)) return;
		if (data && cmdLocal(m)) return;
		if (m[0] == "ui_est") cfg.est = m[1];
		if (m[0] == "ui_apaga") cfg.apaga = m[1];
		if (m[0] == "build" || m[0] == "analyze" || m[0] == "undo") { msgAviso = ""; if (data) save(); }
		outlet(0, m);
		mgraphics.redraw();
		return;
	}
	if (!data || prog || !licPronta() || licInfo) return;
	if (lay && lay.maxFirst > 0 && x >= lay.W - SB_W - 4 && y >= TOP && y < TOP + lay.listH) {
		arrasto = y;
		rolar(lay.maxFirst * (y - TOP) / Math.max(1, lay.listH));
		return;
	}
	if (!hb) return;
	var P = data.partes;
	if (hb.tipo == "pmenu") { abreMenu(x, y, hb.i); return; }
	if (hb.tipo == "borda") {
		op = { tipo: "pBorda", i: hb.i, x0: x, alt: !!opt, vP: copia(P), vT: fotoT(), n0: P[hb.i].n, nViz: P[hb.i + 1] ? P[hb.i + 1].n : 0 };
		return;
	}
	if (hb.tipo == "parte") {
		if (direito) { abreMenu(x, y, hb.i); return; }
		if (opt) {
			if (P.length > 1) { var vP = copia(P), vT = fotoT(); P.splice(hb.i, 1); remapeia(vP, vT, P); mudou(); }
			return;
		}
		op = { tipo: "pMove", i: hb.i, id: P[hb.i].id, x0: x, mexeu: false, vP: copia(P), vT: fotoT() };
		return;
	}
	if (hb.tipo == "grupo") {
		var t = data.tracks[hb.t];
		t.g = (t.g + 1) % GRUPOS.length; t.auto = 0; t.manual = 0;
		t.blocos = blocosDaRegra(t.g, P);
		mudou(); return;
	}
	if (hb.tipo == "hs") { op = { tipo: "hs", x0: x, s0: vista.s, hw: hb.hw }; return; }
	if (hb.tipo == "bloco" || hb.tipo == "bE" || hb.tipo == "bD") {
		var tr = data.tracks[hb.t], bl = tr.blocos[hb.j];
		if (apaga) { tr.blocos.splice(hb.j, 1); tr.manual = 1; mudou(); return; }
		op = { tipo: hb.tipo, t: hb.t, j: hb.j, x0: x, a0: bl.a, b0: bl.b, mexeu: false };
		return;
	}
	if (hb.tipo == "trilha") {
		var bar = Math.max(0, Math.min(total(), barDe(x)));
		op = { tipo: "pinta", t: hb.t, x0: x, bar0: bar, mexeu: false, j: -1, shift: !!shift };
		return;
	}
}
onclick.local = 1;

function ondblclick(x, y) {
	if (op) solta();
	if (!data || prog || menu) return;
	var hb = acha(x, y, ["parte"]);
	if (!hb) return;
	divide(hb.i);
}
ondblclick.local = 1;

function ondrag(x, y, but, cmd, shift, caps, opt) {
	if (op && data) {
		// no "solto" o Max pode não mandar Shift/Alt: usa os do último arrasto
		if (but) { op.sh = shift; op.op = opt; arrastaOp(x, y, shift, opt); }
		else { if (x != op.ux) arrastaOp(x, y, op.sh, op.op); solta(); }
		if (op) op.ux = x;
		return;
	}
	if (arrasto >= 0 && lay) {
		if (!but) { arrasto = -1; return; }
		rolar(lay.maxFirst * (y - TOP) / Math.max(1, lay.listH));
	}
}
ondrag.local = 1;

function arrastaOp(x, y, shift, opt) {
	var P = data.partes, k = K(), d = (x - op.x0) / k, g = grade(shift);
	if (Math.abs(x - op.x0) > 3) op.mexeu = true;
	if (op.tipo == "hs") {
		var tot = Math.max(8, total()), livre = (x1() - X0) - op.hw;
		if (livre > 0) { vista.s = op.s0 + (x - op.x0) / livre * (tot - vista.n); limitaVista(); mgraphics.redraw(); }
		return;
	}
	if (op.tipo == "pBorda") {
		var P0 = op.vP, i = op.i, novas = copia(P0);
		var n = Math.max(1, Math.round((op.n0 + d) / g) * g);
		if (!shift && op.n0 % 4 && Math.abs(d) < 2) n = op.n0;
		if (op.alt && novas[i + 1]) {
			var soma = op.n0 + op.nViz;
			n = Math.min(soma - 1, n);
			novas[i].n = n; novas[i + 1].n = soma - n;
		} else novas[i].n = n;
		data.partes = novas;
		remapeia(P0, op.vT, novas);
		mgraphics.redraw();
		return;
	}
	if (op.tipo == "pMove") {
		if (!op.mexeu) return;
		// muda de lugar quando passa do meio da vizinha
		var P1 = copia(op.vP), de = op.i, item = P1.splice(de, 1)[0], ini = inicios(op.vP);
		var centro = ini[de] + item.n / 2 + d, para = 0, acum = 0;
		for (var q = 0; q < P1.length; q++) { if (centro > acum + P1[q].n / 2) para = q + 1; acum += P1[q].n; }
		P1.splice(para, 0, item);
		data.partes = P1;
		remapeia(op.vP, op.vT, P1);
		mgraphics.redraw();
		return;
	}
	var tot = total(), tr = op.t != null ? data.tracks[op.t] : null;
	if (op.tipo == "bloco") {
		if (!op.mexeu) return;
		var L = op.b0 - op.a0, a = Math.round((op.a0 + d) / g) * g;
		a = Math.max(0, Math.min(tot - L, a));
		tr.blocos[op.j] = { a: a, b: a + L };
		mgraphics.redraw();
		return;
	}
	if (op.tipo == "bE") {
		var na = Math.round((op.a0 + d) / g) * g;
		na = Math.max(0, Math.min(op.b0 - 1, na));
		tr.blocos[op.j] = { a: na, b: op.b0 };
		mgraphics.redraw();
		return;
	}
	if (op.tipo == "bD") {
		var nb = Math.round((op.b0 + d) / g) * g;
		nb = Math.min(tot, Math.max(op.a0 + 1, nb));
		tr.blocos[op.j] = { a: op.a0, b: nb };
		mgraphics.redraw();
		return;
	}
	if (op.tipo == "pinta") {
		if (!op.mexeu) return;
		var b1 = Math.max(0, Math.min(tot, barDe(x)));
		var lo = Math.min(op.bar0, b1), hi = Math.max(op.bar0, b1);
		var A = Math.floor(lo / g) * g, B = Math.ceil(hi / g) * g;
		if (B - A < g) B = A + g;
		B = Math.min(tot, B);
		var novo = { a: A, b: B };
		if (op.j < 0) { tr.blocos.push(novo); op.j = tr.blocos.length - 1; } else tr.blocos[op.j] = novo;
		mgraphics.redraw();
	}
}
// fim do arrasto (botão solto; ou o mouse já se mexeu sem botão e o Max não mandou o "solto")
function solta() {
	var o = op;
	op = null;
	if (!o || !data) return;
	var P = data.partes;
	if (o.tipo == "pMove" && !o.mexeu) {
		// clique simples numa parte: só seleciona (tipo e nome ficam no botão direito)
		sel = o.id;
		mgraphics.redraw();
		return;
	}
	if (o.tipo == "pMove") sel = o.id;
	if (o.tipo == "pinta" && !o.mexeu) {
		var tr = data.tracks[o.t], g = grade(o.shift), A, B;
		if (!pincel) { var pe = parteEm(o.bar0); if (!pe) return; A = pe.s; B = pe.s + pe.n; }
		else { A = Math.floor(o.bar0 / g) * g; B = Math.min(total(), A + pincel); }
		if (B > A) tr.blocos.push({ a: A, b: B });
	}
	if (o.tipo == "bloco" || o.tipo == "bE" || o.tipo == "bD" || o.tipo == "pinta") {
		var t2 = data.tracks[o.t];
		t2.blocos = arruma(t2.blocos);
		t2.manual = 1;
		mudou();
		return;
	}
	if (o.tipo == "pBorda" || o.tipo == "pMove") { mudou(); return; }
	mgraphics.redraw();
}

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
function mesmo(a, b) { return (!a && !b) || (a && b && a.tipo == b.tipo && a.i == b.i && a.t == b.t && a.j == b.j); }
function onidle(x, y) {
	if (op) { solta(); }
	if (prog) return;
	if (menu) {
		var hm = acha(x, y, ["menu"]), k = hm ? hm.k : -1;
		if (k != menu.hov) { menu.hov = k; mgraphics.redraw(); }
		return;
	}
	var h = faixaEm(x, y), hb = alvo(x, y);
	var hv = hb && hb.tipo != "cmd" && hb.tipo != "trilha" && hb.tipo != "hs" ? { tipo: hb.tipo, i: hb.i, t: hb.t, j: hb.j } : null;
	if (h != hover || !mesmo(hv, hov)) { hover = h; hov = hv; mgraphics.redraw(); }
}
onidle.local = 1;
function onidleout() { if (hover != -1 || hov) { hover = -1; hov = null; mgraphics.redraw(); } }
onidleout.local = 1;
// onwheel(x, y, wheel_x, wheel_y, cmd, shift, capslock, option, ctrl)
function onwheel(x, y, wx, wy, cmd, shift, caps, opt, ctrl) {
	if (!data) return;
	if (menu) { menu = null; mgraphics.redraw(); return; }
	var naLinha = y >= RUL_Y && y < TL_Y + TL_H + 4 && x >= X0;
	if (naLinha || cmd || ctrl) { if (wy) zoom(wy > 0 ? 1 / 1.25 : 1.25, x >= X0 ? x : null); return; }
	if (wx || shift) {
		var dv = wx ? -wx : -wy;
		vista.s += (dv > 0 ? 1 : -1) * Math.max(1, vista.n / 16);
		limitaVista(); mgraphics.redraw();
		return;
	}
	if (!lay || lay.maxFirst <= 0) return;
	rolar(first - (wy > 0 ? 3 : wy < 0 ? -3 : 0));
	hover = faixaEm(x, y);
}
onwheel.local = 1;
function onresize() { mgraphics.redraw(); }
