
// ---------------------------------------------------------------- tela de licença (comum às janelas; colada pelo build)
// O motor manda "lic <json>" ({ ok, ocupado, email, token, msg, ate }). Sem licença a janela mostra só esta tela.
// E-mail e token são digitados num [dialog] do Max (o jsui não recebe teclado): clicar no campo manda dlg_email /
// dlg_token, o patcher da janela abre o diálogo e devolve lic_email / lic_token para o motor.
var licSt = null;      // null = ainda não ouviu o motor
var licInfo = 0;       // painel de informações da licença aberto (com a licença valendo)

function lic() {
	var s = arrayfromargs(arguments).join(" ");
	try { licSt = JSON.parse(s); } catch (e) { }
	if (licSt && !licSt.ok) licInfo = 0;
	mgraphics.redraw();
}
function licPronta() { return licSt && licSt.ok; }

function L_rgb(c, a) { mgraphics.set_source_rgba(c[0], c[1], c[2], a == null ? 1 : a); }
function L_fonte(size, bold) { mgraphics.select_font_face(bold ? "Arial Bold" : "Arial"); mgraphics.set_font_size(size); }
function L_larg(s, size, bold) { L_fonte(size, bold); return mgraphics.text_measure(String(s))[0]; }
function L_txt(s, x, y, size, bold) { L_fonte(size, bold); mgraphics.move_to(x, y); mgraphics.show_text(String(s)); }
function L_txtC(s, xc, y, size, bold) { L_txt(s, xc - L_larg(s, size, bold) / 2, y, size, bold); }
function L_ret(x, y, w, h, r, c, a) { L_rgb(c, a); mgraphics.rectangle_rounded(x, y, w, h, r, r); mgraphics.fill(); }
function L_quebra(s, w, size, bold) {
	var pal = String(s).split(" "), linhas = [], l = "";
	for (var i = 0; i < pal.length; i++) {
		var t = l ? l + " " + pal[i] : pal[i];
		if (L_larg(t, size, bold) > w && l) { linhas.push(l); l = pal[i]; } else l = t;
	}
	if (l) linhas.push(l);
	return linhas;
}

// desenha a tela de ativação (ou o painel de informações) por cima de tudo, a partir de y0
function licPinta(W, H, y0, nome, cor, hits) {
	var cinza = [0.55, 0.55, 0.58], branco = [0.93, 0.93, 0.93];
	L_rgb([0.07, 0.075, 0.08], 0.96); mgraphics.rectangle(0, y0, W, H - y0); mgraphics.fill();
	var pw = 560, ph = licInfo ? 300 : 400, px = (W - pw) / 2, py = y0 + Math.max(30, (H - y0 - ph) / 2 - 20);
	L_ret(px - 3, py - 3, pw + 6, ph + 6, 14, cor, 0.25);
	L_ret(px, py, pw, ph, 12, [0.13, 0.135, 0.145]);
	var x = px + 36, y = py + 52;

	if (licInfo && licSt && licSt.ok) {
		L_rgb(cor); L_txt("LICENSE · " + nome, x, y, 22, true);
		y += 44;
		L_rgb(cinza); L_txt("E-MAIL", x, y, 11, true); L_rgb(branco); L_txt(licSt.email || "—", x + 90, y, 14, true);
		y += 30;
		var ate = licSt.ate ? new Date(licSt.ate * 1000) : null;
		var dd = function (n) { return (n < 10 ? "0" : "") + n; };
		L_rgb(cinza); L_txt("VALID UNTIL", x, y, 11, true);
		L_rgb(branco); L_txt(ate ? dd(ate.getDate()) + "/" + dd(ate.getMonth() + 1) + "/" + ate.getFullYear() + " (renova sozinha com internet)" : "—", x + 90, y, 13, false);
		y += 30;
		if (licSt.msg) { L_rgb([0.98, 0.78, 0.35]); L_txt(licSt.msg, x, y, 12, false); }
		y = py + ph - 64;
		L_ret(x, y, 220, 36, 6, [0.45, 0.17, 0.17]);
		L_rgb(branco); L_txtC(licSt.ocupado ? "…" : "DEACTIVATE THIS COMPUTER", x + 110, y + 23, 12, true);
		hits.push({ x: x, y: y, w: 220, h: 36, tipo: "cmd", msg: ["lic_desativar"] });
		L_ret(px + pw - 36 - 120, y, 120, 36, 6, cor);
		L_rgb([0.07, 0.07, 0.08]); L_txtC("CLOSE", px + pw - 36 - 60, y + 23, 13, true);
		hits.push({ x: px + pw - 156, y: y, w: 120, h: 36, tipo: "cmd", msg: ["lic_fecha"] });
		return;
	}

	L_rgb(cor); L_txt("ACTIVATE " + nome, x, y, 24, true);
	y += 26;
	L_rgb(cinza); L_txt("Digite o e-mail da compra e o token que chegou no e-mail. Vale em até 2 computadores.", x, y, 12, false);
	y += 36;
	var campo = function (rot, val, ph2, cmd) {
		L_rgb(cinza); L_txt(rot, x, y, 11, true);
		L_ret(x, y + 8, pw - 72, 38, 6, [0.08, 0.085, 0.09]);
		L_rgb(cor, 0.5); mgraphics.set_line_width(1); mgraphics.rectangle_rounded(x + 0.5, y + 8.5, pw - 73, 37, 6, 6); mgraphics.stroke();
		if (val) { L_rgb(branco); L_txt(val, x + 14, y + 33, 15, true); }
		else { L_rgb([0.42, 0.42, 0.45]); L_txt(ph2, x + 14, y + 33, 14, false); }
		hits.push({ x: x, y: y + 8, w: pw - 72, h: 38, tipo: "cmd", msg: [cmd] });
		y += 66;
	};
	campo("E-MAIL", licSt ? licSt.email : "", "clique para digitar o e-mail", "dlg_email");
	campo("TOKEN", licSt ? licSt.token : "", "clique para colar o token  ISP-XXXX-XXXX-XXXX-XXXX-XXXX", "dlg_token");
	var ocup = licSt && licSt.ocupado;
	L_ret(x - 3, y - 3, 206, 46, 9, cor, 0.25);
	L_ret(x, y, 200, 40, 7, cor);
	L_rgb([0.07, 0.07, 0.08]); L_txtC(ocup ? "ACTIVATING…" : "ACTIVATE", x + 100, y + 26, 15, true);
	if (!ocup) hits.push({ x: x, y: y, w: 200, h: 40, tipo: "cmd", msg: ["lic_ativar"] });
	L_rgb(cinza); L_txt("Ainda não tem?", x + 226, y + 18, 12, false);
	L_rgb(cor); L_txt("COMPRAR " + nome + " ↗", x + 226, y + 34, 13, true);
	hits.push({ x: x + 220, y: y, w: 220, h: 40, tipo: "cmd", msg: ["lic_comprar"] });
	y += 66;
	var m = licSt == null ? "Conferindo a licença…" : licSt.msg;
	if (m) {
		var ok = /Ativado/.test(m);
		L_rgb(ok ? [0.45, 0.85, 0.55] : [0.98, 0.78, 0.35]);
		var ls = L_quebra(m, pw - 72, 12, true);
		for (var i = 0; i < ls.length && i < 3; i++) L_txt(ls[i], x, y + i * 17, 12, true);
	}
}

// cliques que a própria janela resolve (sem mandar ao motor)
function licCliqueLocal(m) {
	if (m[0] == "lic_info") { licInfo = 1; mgraphics.redraw(); return true; }
	if (m[0] == "lic_fecha") { licInfo = 0; mgraphics.redraw(); return true; }
	return false;
}
