// ESPANADOR 0.1 — organiza o set em grupos de stems (os 8 grupos do MAESTRO: KICK, BASS, DRUMS, HATS, SYNTHS,
// AMBIENCE, VOCAL, FX). Par do RODO e da VASSOURA, pensado para o pré-mix.
// Roda no objeto [v8] (Max 9 / Live 12.2+). SCAN lê as faixas do set (nome, cor, saída, posição dos clips).
// Na janela você marca faixas (clique, Shift+clique, arrastar) e escolhe o grupo; AUTO pré-marca pelo nome
// (mesmas palavras do MAESTRO). APPLY, faixa a faixa de cima para baixo:
//   - pinta a faixa com a cor do grupo;
//   - PREFIX (opcional) põe o nome do grupo na frente do nome ("DRUMS Snare Top");
//   - CLIPS (opcional) pinta os clips da faixa (arranjo e session) com a cor do grupo;
//   - BUS: cria (ou reaproveita) uma faixa de áudio com o nome do grupo no topo do set e manda a saída de cada
//     faixa do grupo para ela. O bus fica em Monitor IN e sem entrada externa: soa como um Group Track.
// O LOM não cria Group Track nem muda a ordem das faixas: por isso o bus. Bus = faixa de áudio VAZIA com o nome do grupo
// (uma faixa "KICK" com clips é o kick: vira membro e o bus é "KICK BUS"). Faixa-molde MAESTRO/MOLDE: o bus nasce como
// cópia dela e, no fim do APPLY, ela é apagada (o UNDO devolve numa das cópias).
// UNDO volta nome, cor e saída de cada faixa, a cor de cada clip pintado, e apaga os buses criados nesta sessão (só se continuarem vazios).

autowatch = 1;
inlets = 1;
outlets = 4; // 0: (livre)   1: status (device)   2: janela   3: eco dos controles (prefix/bus/auto)

var DICT = "espanador_data";
var VERSAO = "0.1.0";
var BUILD = "teste";      // marca do pacote de teste; vazio na versão publicada
var LIC_TESTE = true;     // 0.1 TESTE: sem licença (o pacote ainda não existe no servidor)
var LIC_PACOTE = "espanador", LIC_NOME = "ESPANADOR";
var URL_BAIXAR = "https://insidestudiosp.com.br/baixar/espanador/zip";
var URL_NOVIDADES = "https://insidestudiosp.com.br/espanador/";
var PAUSA = 15;
var PATCHER = (typeof this != "undefined" && this && this.patcher) ? this.patcher : null;

// os 8 grupos do MAESTRO, com as cores dele
var GRUPOS = ["KICK", "BASS", "DRUMS", "HATS", "SYNTHS", "AMBIENCE", "VOCAL", "FX"];
var CORES = [0xff8a3d, 0x45a6ff, 0xf5c451, 0xe6e1f5, 0xc9b0ff, 0x4fd1c5, 0xff7fb5, 0xc4e86b];

var cfg = { prefix: 0, bus: 1, auto: 1, clips: 1 };

var ready = false;
var busy = false;
var result = null;
var marcasAntes = {};   // id da faixa -> grupo marcado (sobrevive ao SCAN)
var orig = {};          // id -> { name, color, rota } antes do 1º APPLY (para o UNDO)
var nOrig = 0;
var origClip = {};      // id do clip -> cor antes do 1º APPLY (para o UNDO)
var criados = [];       // ids dos buses criados nesta sessão
var devCriado = {};     // id do bus criado -> nº de devices que ele já tinha ao nascer (cópia da faixa-molde)
var origBus = {};       // id do bus reaproveitado -> { mon, inp } antes do APPLY (para o UNDO)
var moldeApagado = null; // faixa-molde apagada no fim do APPLY: { name, color, mon, inp } (o UNDO devolve numa cópia)
var fmtRota = -1;       // jeito de gravar output_routing_type que funcionou neste Live

// ---------------------------------------------------------------- tarefas com referência guardada (ver VASSOURA 0.2.3)
var T = {};
function depois(nome, fn, ms) {
	if (T[nome]) { try { T[nome].cancel(); } catch (e) { } }
	T[nome] = new Task(fn);
	T[nome].schedule(ms);
}
var seqPasso = 0, etapa = null, ultimoPasso = 0, retomadas = 0, onde = "";
function agora() { return new Date().getTime(); }
function proximo(fn, ms) {
	var meu = ++seqPasso;
	etapa = fn;
	ultimoPasso = agora();
	T.passo = new Task(function () {
		if (meu != seqPasso) return;
		ultimoPasso = agora();
		fn();
	});
	T.passo.schedule(ms);
}
function vigiaLiga() {
	retomadas = 0;
	if (T.vigia) { try { T.vigia.cancel(); } catch (e) { } }
	T.vigia = new Task(function () {
		if (!busy) { T.vigia.cancel(); return; }
		if (etapa && agora() - ultimoPasso > 3000) {
			retomadas++;
			log("ESPANADOR: corrente parada em [" + onde + "], retomando (" + retomadas + ")\n");
			proximo(etapa, 0);
		}
	});
	T.vigia.interval = 1000;
	T.vigia.repeat();
}
function vigiaDesliga() { etapa = null; seqPasso++; if (T.vigia) { try { T.vigia.cancel(); } catch (e) { } } }

function init() {
	ready = true;
	if (LIC_TESTE) { lic.ok = true; lic.email = "teste"; lic.msg = ""; licMudou(); }
	else licCarrega();
	status(licLiberada() ? "PRONTO · clique OPEN e depois SCAN" : "ATIVE A LICENÇA · clique OPEN");
	enviaCfg();
	if (!LIC_TESTE) depois("versao", checaVersao, 4000);
}

function travado() {
	if (licLiberada()) return false;
	status("ATIVE A LICENÇA · clique OPEN");
	outlet(2, "lic", licEstado());
	return true;
}
function licMudou() {
	outlet(2, "lic", licEstado());
	outlet(1, "lic", lic.ok ? 1 : 0);
	if (!ready) return;
	if (lic.ok && !busy && !result) status("PRONTO · clique SCAN");
	else if (!lic.ok) status("ATIVE A LICENÇA · clique OPEN");
}
function enviaCfg() { outlet(2, "valores", cfg.prefix, cfg.bus, cfg.auto, cfg.clips); }
function janela() { enviaCfg(); outlet(2, "lic", licEstado()); if (nova) outlet(2, "nova", nova); }

// ---------------------------------------------------------------- controles
function prefix(v) { cfg.prefix = num(v) ? 1 : 0; publica(); }
function bus(v) { cfg.bus = num(v) ? 1 : 0; publica(); }
function auto(v) { cfg.auto = num(v) ? 1 : 0; }
function clips(v) { cfg.clips = num(v) ? 1 : 0; publica(); }
function ui_prefix(v) { prefix(v); outlet(3, "prefix", cfg.prefix); enviaCfg(); }
function ui_bus(v) { bus(v); outlet(3, "bus", cfg.bus); enviaCfg(); }
function ui_auto(v) { auto(v); outlet(3, "auto", cfg.auto); enviaCfg(); }
function ui_clips(v) { clips(v); outlet(3, "clips", cfg.clips); enviaCfg(); }

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
	outlet(1, "status", txt);
	outlet(2, "status", txt);
}

// ---------------------------------------------------------------- grupo pelo nome (cópia do roleDoNome do MAESTRO, Dsp.h)
function grupoDoNome(nome) {
	var s = String(nome || "").toLowerCase();
	if (!s) return -1;
	function tem(k) { return s.indexOf(k) >= 0; }
	if (tem("kick") || tem("kik") || tem("bd ") || s == "bd" || tem("bumbo")) return 0;
	if (tem("bass") || tem("sub") || tem("baixo") || tem("808")) return 1;
	if (tem("vox") || tem("vocal") || tem("voice") || tem("voc") || tem("speech") || tem("voz")) return 6;
	if (tem("hat") || tem("hh") || tem("ride") || tem("cymb") || tem("crash") || tem("prato") || tem("shaker")) return 3;
	if (tem("drum") || tem("snare") || tem("snr") || tem("clap") || tem("perc") || tem("tom") || tem("rim") ||
		tem("top") || tem("conga") || tem("bongo") || tem("caixa") || tem("bateria")) return 2;
	if (tem("fx") || tem("riser") || tem("sweep") || tem("impact") || tem("uplift") || tem("downlift") || tem("whoosh") ||
		tem("noise") || tem("fill") || tem("reverse") || tem("zap") || tem("efeito")) return 7;
	if (tem("amb") || tem("atmo") || tem("pad") || tem("drone") || tem("texture") || tem("space") || tem("choir")) return 5;
	if (tem("synth") || tem("lead") || tem("acid") || tem("303") || tem("arp") || tem("pluck") || tem("key") || tem("seq") ||
		tem("melod") || tem("stab") || tem("piano") || tem("guitar") || tem("chord") || tem("string") || tem("squelch")) return 4;
	return -1;
}
// nome de bus de grupo: "DRUMS" ou "DRUMS BUS" (exatos, sem diferença de maiúscula)
function grupoDoBus(nome) {
	var s = String(nome || "").replace(/^\s+|\s+$/g, "").toUpperCase();
	for (var g = 0; g < GRUPOS.length; g++) if (s == GRUPOS[g] || s == GRUPOS[g] + " BUS") return g;
	return -1;
}
// faixa-molde do bus: "MAESTRO" ou "MOLDE" (exatos, sem diferença de maiúscula)
function ehMolde(nome) {
	var s = String(nome || "").replace(/^\s+|\s+$/g, "").toUpperCase();
	return s == "MAESTRO" || s == "MOLDE";
}
// tira um prefixo de grupo que já esteja no nome ("DRUMS Snare" -> "Snare"), para o PREFIX não duplicar
function semPrefixo(nome) {
	var s = String(nome || "");
	for (var g = 0; g < GRUPOS.length; g++) {
		var p = GRUPOS[g] + " ";
		if (s.substring(0, p.length).toUpperCase() == p && s.length > p.length) return s.substring(p.length);
	}
	return s;
}

// ---------------------------------------------------------------- LiveAPI reaproveitado (ver VASSOURA)
var lapi = null;
var gotoFalhou = false;
function aponta(path) {
	if (lapi && !gotoFalhou) {
		try {
			lapi.goto(path);
			var m = /^id (\d+)$/.exec(path);
			if (!m || Number(lapi.id) == Number(m[1])) return lapi;
		} catch (e) { }
		gotoFalhou = true;
		log("ESPANADOR: goto do LiveAPI não funcionou aqui, usando um LiveAPI por consulta\n");
	}
	var a = new LiveAPI(path);
	if (!lapi) lapi = a;
	return a;
}

// ---------------------------------------------------------------- log em arquivo (ESPANADOR-log.txt, ao lado do device)
var logPath = null, logOk = false, logMotivo = "";
function log(s, soArq) {
	if (!soArq || !logOk) post(s);
	if (!logOk || !s) return;
	try {
		var f = new File(logPath, "readwrite", "TEXT");
		if (!f.isopen) { logOk = false; return; }
		f.position = f.eof;
		f.writestring(hora() + " " + s.replace(/\n$/, "") + "\r\n");
		f.close();
	} catch (e) { logOk = false; }
}
function pastaDoDevice() {
	var p = PATCHER;
	for (var n = 0; p && n < 5; n++) {
		var fp = "";
		try { fp = String(p.filepath || ""); } catch (e) { }
		if (fp) return fp.replace(/[^\/\\]*$/, "");
		try { p = p.parentpatcher; } catch (e) { p = null; }
	}
	return "";
}
function logNovo() {
	logOk = false;
	try {
		if (logPath == null) {
			var pasta = pastaDoDevice();
			logPath = pasta ? pasta + "ESPANADOR-log.txt" : "";
			if (!pasta) logMotivo = "não achei a pasta do device";
		}
		if (logPath) {
			var f = new File(logPath, "write", "TEXT");
			if (f.isopen) {
				try { f.eof = 0; } catch (e) { }
				f.writestring(hora() + " log novo\r\n");
				f.close();
				logOk = true;
			} else logMotivo = "o Max não abriu " + logPath + " para gravar";
		}
	} catch (e) { logMotivo = String(e); }
	post(logOk ? "ESPANADOR: log em " + logPath + "\n" : "ESPANADOR: sem log em arquivo (" + logMotivo + "), detalhes vão para este console\n");
}
function hora() {
	var d = new Date();
	function p(n) { return (n < 10 ? "0" : "") + n; }
	return p(d.getHours()) + ":" + p(d.getMinutes()) + ":" + p(d.getSeconds());
}
function versaoMax() { try { return String(max.version); } catch (e) { return "?"; } }

// ---------------------------------------------------------------- roteamento (output/input_routing_type são dicts no LOM)
// O Live devolve um texto JSON: {"output_routing_type": {"display_name": "Master", "identifier": 26}} e
// {"available_output_routing_types": [ ... ]}. Lê tolerante (texto, lista, objeto).
function leDict(api, prop) {
	var raw;
	try { raw = api.get(prop); } catch (e) { return null; }
	if (raw && typeof raw == "object" && !(raw instanceof Array)) return raw[prop] != null ? raw[prop] : raw;
	var s = str(raw), k = s.indexOf("{");
	if (k < 0) return null;
	try { var j = JSON.parse(s.substring(k)); return j[prop] != null ? j[prop] : j; } catch (e) { return null; }
}
function rotaAtual(api, lado) {
	var r = leDict(api, lado + "_routing_type");
	return r && r.display_name != null ? { nome: String(r.display_name), id: r.identifier } : { nome: "", id: null };
}
function rotasDisponiveis(api, lado) {
	var l = leDict(api, "available_" + lado + "_routing_types");
	if (!l) return [];
	if (!(l instanceof Array)) l = [l];
	var out = [];
	for (var i = 0; i < l.length; i++) if (l[i] && l[i].display_name != null) out.push({ nome: String(l[i].display_name), id: l[i].identifier });
	return out;
}
// grava a rota e confere lendo de volta; tenta os formatos que o LiveAPI do JS aceita (guarda o que funcionou)
function gravaRota(api, lado, alvo) {
	var prop = lado + "_routing_type";
	var fmts = [
		function () { api.set(prop, { identifier: alvo.id }); },
		function () { api.set(prop, JSON.stringify({ identifier: alvo.id })); },
		function () { var o = {}; o[prop] = { identifier: alvo.id }; api.set(prop, JSON.stringify(o)); },
		function () { api.set(prop, "identifier", alvo.id); }
	];
	var ordem = [];
	if (fmtRota >= 0) ordem.push(fmtRota);
	for (var k = 0; k < fmts.length; k++) if (k != fmtRota) ordem.push(k);
	for (var q = 0; q < ordem.length; q++) {
		try { fmts[ordem[q]](); } catch (e) { continue; }
		var agoraR = rotaAtual(api, lado);
		if (agoraR.nome == alvo.nome) {
			if (fmtRota != ordem[q]) { fmtRota = ordem[q]; log("ESPANADOR: rota gravada pelo formato " + fmtRota + "\n"); }
			return true;
		}
	}
	return false;
}
function achaRota(lista, nome) {
	var achou = null, n = 0;
	for (var i = 0; i < lista.length; i++) if (lista[i].nome == nome) { if (!achou) achou = lista[i]; n++; }
	if (!achou) for (var j = 0; j < lista.length; j++) if (lista[j].nome.toUpperCase() == String(nome).toUpperCase()) { achou = lista[j]; n++; break; }
	return achou ? { r: achou, n: n } : null;
}

// ---------------------------------------------------------------- SCAN
function scan() {
	if (!ready) { status("aguarde o Live terminar de carregar"); return; }
	if (busy || travado()) return;
	busy = true;
	if (result) lerMarcas();
	var set = new LiveAPI("live_set");
	var nt = set.getcount("tracks");
	result = {
		versao: VERSAO, sig: num(set.get("signature_numerator")) || 4,
		minBeat: -1, maxBeat: 0, tracks: [], grupos: GRUPOS, cores: CORES
	};
	logNovo();
	log("ESPANADOR " + VERSAO + (BUILD ? " (" + BUILD + ")" : "") + ": SCAN · " + nt + " faixas · Live/Max " + versaoMax() + "\n");
	var i = 0;
	vigiaLiga();
	function umaFaixa() {
		try {
			if (i < nt) {
				var esta = i++;
				onde = "SCAN faixa " + (esta + 1) + "/" + nt;
				leFaixa(esta);
				proximo(umaFaixa, 2);
				return;
			}
			fimScan();
		} catch (e) {
			busy = false;
			vigiaDesliga();
			log("ESPANADOR erro no SCAN (faixa " + i + "): " + e + "\n");
			status("erro no SCAN na faixa " + i + " · ver console do Max");
		}
	}
	umaFaixa();
}

function leFaixa(i) {
	var t = aponta("live_set tracks " + i);
	var tr = {
		id: Number(t.id), index: i, name: str(t.get("name")), color: num(t.get("color")),
		fold: num(t.get("is_foldable")), member: num(t.get("is_grouped")),
		midi: num(t.get("has_midi_input")), g: -1, auto: 0, bus: -1, clips: []
	};
	status("SCAN " + (i + 1) + " · " + tr.name);
	tr.pai = tr.member ? idDoPai(t) : 0;
	tr.out = rotaAtual(t, "output").nome;
	var nc = 0;
	try { nc = t.getcount("arrangement_clips"); } catch (e) { nc = 0; }
	// bus de grupo = faixa de áudio VAZIA com o nome do grupo. Uma faixa "KICK" com clips (ou MIDI) é o kick, não o bus
	var vazia = !tr.fold && !tr.midi && !nc && !nSessao(t);
	tr.bus = vazia ? grupoDoBus(tr.name) : -1;
	tr.molde = !tr.fold && ehMolde(tr.name) ? 1 : 0;
	for (var j = 0; j < nc; j++) {
		var c = aponta("live_set tracks " + i + " arrangement_clips " + j);
		var s = num(c.get("start_time")), e = num(c.get("end_time"));
		tr.clips.push([Math.round(s * 100) / 100, Math.round(e * 100) / 100]);
		if (e > result.maxBeat) result.maxBeat = e;
		if (result.minBeat < 0 || s < result.minBeat) result.minBeat = s;
	}
	result.tracks.push(tr);
	log("  " + i + " [" + tr.name + "] " + (tr.fold ? "GROUP " : "") + (tr.member ? "(no grupo) " : "") + (tr.bus >= 0 ? "BUS " + GRUPOS[tr.bus] + " " : "") +
		"saída " + tr.out + " · " + nc + " clips\n", 1);
}

// nº de clips da session (slots com clip)
function nSessao(api) {
	var slots = idsDe(api, "clip_slots"), n = 0;
	for (var s = 0; s < slots.length; s++) { try { if (num(new LiveAPI("id " + slots[s]).get("has_clip"))) n++; } catch (e) { } }
	return n;
}
// pode servir de bus: faixa de áudio sem clips no arranjo nem na session
function podeSerBus(api) {
	if (num(api.get("is_foldable")) || num(api.get("has_midi_input"))) return false;
	var nc = 0; try { nc = api.getcount("arrangement_clips"); } catch (e) { }
	return !nc && !nSessao(api);
}

// Group Track que contém a faixa (group_track volta "id N"; 0 = fora de grupo)
function idDoPai(api) {
	var r = null;
	try { r = api.get("group_track"); } catch (e) { return 0; }
	if (!r) return 0;
	var n = (r instanceof Array || typeof r == "object") ? (r.length >= 2 ? Number(r[1]) : Number(r[0])) : Number(r);
	return isNaN(n) ? 0 : n;
}
// Group Track mais de fora que contém a faixa (-1 = fora de grupo)
function grupoDeFora(tr) {
	var porId = {}, i;
	for (i = 0; i < result.tracks.length; i++) porId[result.tracks[i].id] = i;
	var idx = -1, cur = tr;
	for (var d = 0; d < 20 && cur.pai && porId.hasOwnProperty(cur.pai); d++) { idx = porId[cur.pai]; cur = result.tracks[idx]; }
	return idx;
}

function fimScan() {
	busy = false;
	vigiaDesliga();
	var nAuto = 0, nJa = 0, nomesBus = {};
	for (var b = 0; b < result.tracks.length; b++) if (result.tracks[b].bus >= 0) nomesBus[result.tracks[b].name] = result.tracks[b].bus;
	for (var i = 0; i < result.tracks.length; i++) {
		var tr = result.tracks[i];
		if (tr.bus >= 0 || tr.molde) { tr.g = -1; continue; }
		if (marcasAntes.hasOwnProperty(tr.id)) { tr.g = marcasAntes[tr.id].g; tr.auto = marcasAntes[tr.id].auto; continue; }
		// já mandada para um bus de grupo: é desse grupo
		if (!tr.fold && nomesBus.hasOwnProperty(tr.out)) { tr.g = nomesBus[tr.out]; nJa++; continue; }
		if (cfg.auto && !tr.fold) { var g = grupoDoNome(tr.name); if (g >= 0) { tr.g = g; tr.auto = 1; nAuto++; } }
	}
	publica();
	var n = result.tracks.length;
	status(n + " faixas · " + nMarcadas() + " num grupo" + (nAuto ? " (" + nAuto + " pelo nome, confira)" : "") + (nJa ? " · " + nJa + " já no bus" : "") + " · nada muda até o APPLY");
	if (posApply) { var txt = posApply; posApply = ""; status(txt); outlet(2, "aviso", txt); }
}
function nMarcadas() { var n = 0; for (var i = 0; i < result.tracks.length; i++) if (result.tracks[i].g >= 0) n++; return n; }

function publica() {
	if (!result) return;
	result.cfg = cfg;
	result.nOrig = nOrig;
	var d = new Dict(DICT);
	d.parse(JSON.stringify(result));
	outlet(2, "bang");
}

// marcas que a janela gravou no dict (grupo de cada faixa)
function lerMarcas() {
	var d = new Dict(DICT), j = null;
	try { j = JSON.parse(d.stringify()); } catch (e) { j = null; }
	if (!j || !j.tracks) return;
	var tr = j.tracks instanceof Array ? j.tracks : [j.tracks];
	marcasAntes = {};
	for (var i = 0; i < tr.length; i++) {
		var g = tr[i].g == null ? -1 : Number(tr[i].g);
		if (result && result.tracks[i] && result.tracks[i].id == tr[i].id) { result.tracks[i].g = g; result.tracks[i].auto = tr[i].auto ? 1 : 0; }
		marcasAntes[tr[i].id] = { g: g, auto: tr[i].auto ? 1 : 0 };
	}
}
// janela mudou marcas: relê e devolve a contagem
function marcas() {
	if (!result || busy) return;
	lerMarcas();
	status(result.tracks.length + " faixas · " + nMarcadas() + " num grupo · APPLY organiza");
}
// AUTO na hora (botão da janela): marca pelo nome só as faixas ainda sem grupo
function auto_agora() {
	if (!result || busy) { status("clique SCAN antes"); return; }
	lerMarcas();
	var n = 0;
	for (var i = 0; i < result.tracks.length; i++) {
		var tr = result.tracks[i];
		if (tr.bus >= 0 || tr.fold || tr.g >= 0) continue;
		var g = grupoDoNome(tr.name);
		if (g >= 0) { tr.g = g; tr.auto = 1; n++; }
	}
	publica();
	status(n ? "AUTO marcou " + n + " faixas pelo nome · confira antes do APPLY" : "AUTO: nenhum nome conhecido nas faixas sem grupo");
}

// ---------------------------------------------------------------- APPLY
var passos = [];
var ap = null;
var posApply = "";

function apply() {
	if (busy || travado()) return;
	if (!result) { status("clique SCAN antes do APPLY"); return; }
	lerMarcas();
	var membros = [], usados = {};
	for (var i = 0; i < result.tracks.length; i++) {
		var tr = result.tracks[i];
		if (tr.g >= 0 && tr.bus < 0 && !tr.molde) { membros.push(i); usados[tr.g] = 1; }
	}
	if (!membros.length) { status("nenhuma faixa num grupo · marque as faixas e escolha o grupo"); return; }
	busy = true;
	ap = { feitos: 0, falhas: 0, rotaFalhou: 0, buses: 0, busesNovos: 0, doMolde: 0, clips: 0, inicio: agora(), bus: {}, total: membros.length };
	passos = [];
	// 1) buses: reaproveita o que já existe com o nome do grupo; cria os que faltam no topo (ordem KICK..FX)
	if (cfg.bus) {
		var porNome = {};
		for (var q = 0; q < result.tracks.length; q++) porNome[result.tracks[q].name.toUpperCase()] = (porNome[result.tracks[q].name.toUpperCase()] || 0) + 1;
		for (var g = GRUPOS.length - 1; g >= 0; g--) {
			if (!usados[g]) continue;
			(function (g) { passos.push(function () { preparaBus(g, porNome); }); })(g);
		}
	}
	// 2) membros, de cima para baixo
	for (var m = 0; m < membros.length; m++) (function (idx, k) { passos.push(function () { umMembro(idx, k); }); })(membros[m], m);
	// 3) faixas que já estão num Group Track ficam no grupo; quem vai para o bus é o Group Track de fora,
	//    desde que todas as faixas marcadas dentro dele sejam do mesmo grupo
	if (cfg.bus) {
		var gt = {}, ordemGt = [];
		for (var mm = 0; mm < membros.length; mm++) {
			var tm = result.tracks[membros[mm]];
			if (!tm.member) continue;
			var fora = grupoDeFora(tm);
			if (fora < 0) continue;
			if (!gt[fora]) { gt[fora] = {}; ordemGt.push(fora); }
			gt[fora][tm.g] = 1;
		}
		for (var o = 0; o < ordemGt.length; o++) {
			var gs = []; for (var kk in gt[ordemGt[o]]) if (gt[ordemGt[o]].hasOwnProperty(kk)) gs.push(Number(kk));
			if (gs.length == 1) (function (idx, g) { passos.push(function () { umGroupTrack(idx, g); }); })(ordemGt[o], gs[0]);
			else log("  Group Track [" + result.tracks[ordemGt[o]].name + "]: tem faixas de " + gs.length + " grupos diferentes; saída dele não mudou\n");
		}
	}
	// 4) no fim, a faixa-molde sai do set (o MAESTRO dela, sem nome de grupo, ia ficar "BY SOUND" no AMBIENCE)
	if (cfg.bus) passos.push(apagaMolde);
	log("ESPANADOR: APPLY · " + membros.length + " faixas · BUS " + (cfg.bus ? "ON" : "OFF") + " · PREFIX " + (cfg.prefix ? "ON" : "OFF") + " · CLIPS " + (cfg.clips ? "ON" : "OFF") + "\n");
	outlet(2, "varre", membros.length);
	vigiaLiga();
	ap.i = 0;
	passoApply();
}
function passoApply() {
	if (ap.i >= passos.length) { fimApply(); return; }
	var fn = passos[ap.i++];
	try { fn(); } catch (e) { ap.falhas++; log("ESPANADOR erro no APPLY: " + e + "\n"); }
	if (busy) proximo(passoApply, PAUSA);
}

// acha o bus do grupo pelo id guardado ou pelo nome; senão cria um
function preparaBus(g, porNome) {
	onde = "bus " + GRUPOS[g];
	status("APPLY · bus " + GRUPOS[g]);
	var set = new LiveAPI("live_set");
	var nt = set.getcount("tracks"), achado = null;
	for (var i = 0; i < nt; i++) {
		var t = aponta("live_set tracks " + i);
		if (grupoDoBus(str(t.get("name"))) != g || !podeSerBus(t)) continue;
		achado = { id: Number(t.id), nome: str(t.get("name")) }; break;
	}
	if (achado) {
		var ab = new LiveAPI("id " + achado.id);
		if (!origBus[achado.id]) origBus[achado.id] = { mon: num(ab.get("current_monitoring_state")), inp: rotaAtual(ab, "input").nome };
		ap.bus[g] = achado; ap.buses++;
		log("  bus " + GRUPOS[g] + ": reaproveitado [" + achado.nome + "]\n");
		configuraBus(new LiveAPI("id " + achado.id), g, false);
		return;
	}
	// nome livre: "DRUMS"; se outra faixa (um Group Track, por ex.) já se chama DRUMS, o bus vira "DRUMS BUS"
	var nome = porNome[GRUPOS[g]] ? GRUPOS[g] + " BUS" : GRUPOS[g];
	var antes = {}, molde = -1;
	for (var k = 0; k < nt; k++) {
		var tk0 = aponta("live_set tracks " + k);
		antes[Number(tk0.id)] = 1;
		if (molde < 0 && !num(tk0.get("is_foldable")) && ehMolde(str(tk0.get("name")))) molde = k;
	}
	// faixa-molde ("MAESTRO" ou "MOLDE"): o bus nasce como cópia dela, com o MAESTRO (ou o que tiver) já dentro.
	// O LOM não insere plugin/M4L numa faixa (insert_device só aceita device nativo), mas duplica a faixa inteira.
	if (molde >= 0 && !ap.molde) {
		var am = new LiveAPI("live_set tracks " + molde);
		ap.molde = { id: Number(am.id), name: str(am.get("name")), color: num(am.get("color")), mon: num(am.get("current_monitoring_state")), inp: rotaAtual(am, "input").nome };
	}
	if (molde >= 0) set.call("duplicate_track", molde);
	else set.call("create_audio_track", 0);
	var novo = null;
	var nt2 = set.getcount("tracks");
	for (var k2 = 0; k2 < nt2; k2++) { var tk = aponta("live_set tracks " + k2); if (!antes[Number(tk.id)]) { novo = Number(tk.id); break; } }
	if (!novo) { ap.falhas++; log("  bus " + GRUPOS[g] + ": o Live não criou a faixa\n"); return; }
	criados.push(novo);
	var api = new LiveAPI("id " + novo);
	var nd0 = 0; try { nd0 = api.getcount("devices"); } catch (e) { }
	devCriado[novo] = nd0;
	api.set("name", nome);
	ap.bus[g] = { id: novo, nome: nome }; ap.buses++; ap.busesNovos++;
	if (molde >= 0) ap.doMolde++;
	log("  bus " + GRUPOS[g] + ": criado [" + nome + "] id " + novo + (molde >= 0 ? " (cópia da faixa-molde, " + nd0 + " devices)" : "") + "\n");
	configuraBus(api, g, true);
}
function configuraBus(api, g, novo) {
	try { api.set("color", CORES[g]); } catch (e) { }
	// Monitor IN (0) e sem entrada externa: só soa o que as faixas do grupo mandam
	try { api.set("current_monitoring_state", 0); } catch (e) { log("  bus: não mudou o Monitor (" + e + ")\n"); }
	var lst = rotasDisponiveis(api, "input"), sem = null;
	for (var i = 0; i < lst.length; i++) if (/^(no input|sem entrada|keine|aucune|ninguna)/i.test(lst[i].nome)) { sem = lst[i]; break; }
	var ent = rotaAtual(api, "input").nome;
	if (sem && ent != sem.nome) { if (!gravaRota(api, "input", sem)) log("  bus: não consegui pôr a entrada em \"" + sem.nome + "\" (está em \"" + ent + "\")\n"); }
	else if (!sem) log("  bus: não achei \"No Input\" nas entradas (" + lst.length + " opções); entrada ficou \"" + ent + "\"\n");
}

function umMembro(idx, k) {
	var tr = result.tracks[idx];
	onde = "APPLY faixa " + (k + 1) + "/" + ap.total;
	status("APPLY " + (k + 1) + "/" + ap.total + " · " + tr.name);
	var api = aponta("id " + tr.id), ok = false;
	try { ok = Number(api.id) == tr.id; } catch (e) { ok = false; }
	if (!ok) { ap.falhas++; log("  [" + tr.name + "]: faixa não existe mais\n"); outlet(2, "progresso", k + 1, ap.total, idx); return; }
	api = new LiveAPI("id " + tr.id);
	if (!orig[tr.id]) { orig[tr.id] = { name: str(api.get("name")), color: num(api.get("color")), rota: rotaAtual(api, "output").nome }; nOrig++; }
	var g = tr.g;
	try { api.set("color", CORES[g]); } catch (e) { log("  [" + tr.name + "]: cor não mudou (" + e + ")\n"); }
	if (cfg.prefix) {
		var base = semPrefixo(str(api.get("name")));
		var nn = base.replace(/^\s+|\s+$/g, "").toUpperCase() == GRUPOS[g] ? base : GRUPOS[g] + " " + base;   // faixa "KICK" não vira "KICK KICK"
		if (nn != str(api.get("name"))) api.set("name", nn);
	}
	if (cfg.clips) pintaClips(api, tr, CORES[g]);
	if (cfg.bus && ap.bus[g] && !tr.fold && tr.member) {
		log("  [" + tr.name + "]: está dentro de um Group Track; a saída fica no grupo\n", 1);
	} else if (cfg.bus && ap.bus[g] && !tr.fold) {
		var lst = rotasDisponiveis(api, "output"), a = achaRota(lst, ap.bus[g].nome);
		if (!a) { ap.rotaFalhou++; log("  [" + tr.name + "]: o bus \"" + ap.bus[g].nome + "\" não aparece nas saídas (" + lst.length + " opções)\n"); }
		else {
			if (a.n > 1) log("  [" + tr.name + "]: " + a.n + " saídas chamadas \"" + ap.bus[g].nome + "\", usei a primeira\n");
			if (rotaAtual(api, "output").nome != a.r.nome && !gravaRota(api, "output", a.r)) { ap.rotaFalhou++; log("  [" + tr.name + "]: não consegui mudar a saída para \"" + a.r.nome + "\"\n"); }
		}
	}
	ap.feitos++;
	log("    [" + tr.name + "] -> " + GRUPOS[g] + "\n", 1);
	outlet(2, "progresso", k + 1, ap.total, idx);
}

// ---------------------------------------------------------------- clips (arranjo e session) na cor do grupo
// get("arrangement_clips") / get("clip_slots") devolvem "id 12 id 13 ..."; o "clip" do slot devolve "id N" (0 = vazio)
function idsDe(api, prop) {
	var r;
	try { r = api.get(prop); } catch (e) { return []; }
	if (!(r instanceof Array)) r = str(r).split(" ");
	var out = [];
	for (var i = 0; i < r.length; i++) { var n = Number(r[i]); if (String(r[i]) != "id" && !isNaN(n) && n > 0) out.push(n); }
	return out;
}
function pintaClips(api, tr, cor) {
	var ids = idsDe(api, "arrangement_clips"), slots = idsDe(api, "clip_slots"), n = 0, falhou = 0;
	for (var s = 0; s < slots.length; s++) {
		try {
			var sl = new LiveAPI("id " + slots[s]);
			if (!num(sl.get("has_clip"))) continue;
			var c = idsDe(sl, "clip");
			if (c.length) ids.push(c[0]);
		} catch (e) { }
	}
	for (var i = 0; i < ids.length; i++) {
		try {
			var ca = new LiveAPI("id " + ids[i]);
			if (Number(ca.id) != ids[i]) continue;
			var antes = num(ca.get("color"));
			if (!origClip.hasOwnProperty(ids[i])) origClip[ids[i]] = antes;
			if (antes != cor) ca.set("color", cor);
			n++;
		} catch (e) { falhou++; }
	}
	ap.clips += n;
	if (falhou) log("  [" + tr.name + "]: " + falhou + " clips não mudaram de cor\n");
}

// Group Track existente: ganha a cor do grupo e sai no bus (o nome não muda)
function umGroupTrack(idx, g) {
	var tr = result.tracks[idx];
	onde = "APPLY Group Track " + tr.name;
	status("APPLY · Group Track " + tr.name);
	var api = aponta("id " + tr.id), ok = false;
	try { ok = Number(api.id) == tr.id; } catch (e) { ok = false; }
	if (!ok) { ap.falhas++; log("  Group Track [" + tr.name + "]: não existe mais\n"); return; }
	api = new LiveAPI("id " + tr.id);
	if (!orig[tr.id]) { orig[tr.id] = { name: str(api.get("name")), color: num(api.get("color")), rota: rotaAtual(api, "output").nome }; nOrig++; }
	try { api.set("color", CORES[g]); } catch (e) { }
	if (!ap.bus[g]) return;
	var a = achaRota(rotasDisponiveis(api, "output"), ap.bus[g].nome);
	if (!a) { ap.rotaFalhou++; log("  Group Track [" + tr.name + "]: o bus \"" + ap.bus[g].nome + "\" não aparece nas saídas\n"); return; }
	if (rotaAtual(api, "output").nome != a.r.nome && !gravaRota(api, "output", a.r)) { ap.rotaFalhou++; log("  Group Track [" + tr.name + "]: não consegui mudar a saída para \"" + a.r.nome + "\"\n"); return; }
	log("    Group Track [" + tr.name + "] -> " + GRUPOS[g] + "\n", 1);
}

// apaga a faixa-molde se algum bus nasceu dela neste APPLY (o UNDO a devolve numa das cópias)
function apagaMolde() {
	if (!ap.molde || !ap.doMolde) return;
	onde = "apaga molde";
	var set = new LiveAPI("live_set"), nt = set.getcount("tracks"), idx = -1;
	for (var k = 0; k < nt; k++) if (Number(aponta("live_set tracks " + k).id) == ap.molde.id) { idx = k; break; }
	if (idx < 0) { log("  faixa-molde: não achei mais\n"); return; }
	var am = new LiveAPI("id " + ap.molde.id);
	if (!podeSerBus(am)) { log("  faixa-molde [" + ap.molde.name + "] tem clips: não apaguei\n"); return; }
	set.call("delete_track", idx);
	moldeApagado = ap.molde;
	ap.moldeSaiu = 1;
	log("  faixa-molde [" + ap.molde.name + "] apagada (o UNDO devolve)\n");
}

function fimApply() {
	busy = false;
	vigiaDesliga();
	posApply = "APPLY FEITO · " + ap.feitos + " faixas organizadas" +
		(cfg.clips && ap.clips ? " · " + ap.clips + " clips pintados" : "") +
		(cfg.bus ? " · " + ap.buses + " bus" + (ap.buses > 1 ? "es" : "") + (ap.busesNovos ? " (" + ap.busesNovos + " novo" + (ap.busesNovos > 1 ? "s" : "") + (ap.doMolde ? ", cópia do molde" : "") + ")" : "") : "") +
		(ap.moldeSaiu ? " · faixa " + ap.molde.name + " apagada" : "") +
		(ap.rotaFalhou ? " · " + ap.rotaFalhou + " saídas NÃO mudaram (ver console)" : "") +
		(ap.falhas ? " · " + ap.falhas + " falhas (ver console)" : "") + " · UNDO volta como estava";
	log("ESPANADOR: " + posApply + " · " + ((agora() - ap.inicio) / 1000).toFixed(1) + " s\n");
	outlet(2, "fim");
	marcasAntes = {};
	for (var i = 0; i < result.tracks.length; i++) marcasAntes[result.tracks[i].id] = { g: result.tracks[i].g, auto: result.tracks[i].auto };
	depois("rescan", scan, 300);
}

// ---------------------------------------------------------------- UNDO (volta nome, cor e saída; apaga buses vazios criados aqui)
var filaUndo = [];
function voltaEntrada(api, nome) {
	if (!nome || rotaAtual(api, "input").nome == nome) return;
	var r = achaRota(rotasDisponiveis(api, "input"), nome);
	if (!r || !gravaRota(api, "input", r.r)) log("  [" + str(api.get("name")) + "]: entrada não voltou para \"" + nome + "\"\n");
}
function undo() {
	if (busy || travado()) return;
	filaUndo = [];
	for (var id in orig) if (orig.hasOwnProperty(id)) filaUndo.push({ tipo: "faixa", id: Number(id) });
	for (var cid in origClip) if (origClip.hasOwnProperty(cid)) filaUndo.push({ tipo: "clip", id: Number(cid) });
	for (var bid in origBus) if (origBus.hasOwnProperty(bid)) filaUndo.push({ tipo: "busvelho", id: Number(bid) });
	for (var c = criados.length - 1; c >= 0; c--) filaUndo.push({ tipo: "bus", id: criados[c] });
	if (!filaUndo.length) { status("UNDO: nada para voltar (nenhum APPLY nesta sessão do device)"); return; }
	busy = true;
	ap = { i: 0, feitos: 0, falhas: 0, apagados: 0, mantidos: 0, clips: 0, clipsSumiram: 0 };
	log("ESPANADOR: UNDO · " + filaUndo.length + " itens\n");
	vigiaLiga();
	passoUndo();
}
function passoUndo() {
	try {
		if (ap.i >= filaUndo.length) {
			busy = false;
			vigiaDesliga();
			posApply = "UNDO FEITO · " + ap.feitos + " faixas de volta" + (ap.clips ? " · " + ap.clips + " clips na cor de antes" : "") + (ap.apagados ? " · " + ap.apagados + " buses apagados" : "") +
				(ap.mantidos ? " · " + ap.mantidos + " buses mantidos (têm device ou clip)" : "") + (ap.moldeVoltou ? " · faixa-molde de volta" : "") + (ap.falhas ? " · " + ap.falhas + " não achadas" : "") + (ap.clipsSumiram ? " · " + ap.clipsSumiram + " clips já não existiam" : "");
			log("ESPANADOR: " + posApply + "\n");
			if (moldeApagado) log("  faixa-molde [" + moldeApagado.name + "] não voltou: todos os buses copiados ganharam device novo\n");
			orig = {}; nOrig = 0; origClip = {}; criados = []; devCriado = {}; origBus = {}; moldeApagado = null;
			marcasAntes = {};
			if (result) for (var i = 0; i < result.tracks.length; i++) marcasAntes[result.tracks[i].id] = { g: result.tracks[i].g, auto: result.tracks[i].auto };
			depois("rescan", scan, 300);
			return;
		}
		var it = filaUndo[ap.i++];
		onde = "UNDO " + ap.i + "/" + filaUndo.length;
		status("UNDO " + ap.i + "/" + filaUndo.length);
		var a = it.tipo == "clip" ? new LiveAPI("id " + it.id) : aponta("id " + it.id), ok = false;
		try { ok = Number(a.id) == it.id; } catch (e) { ok = false; }
		if (!ok) { if (it.tipo == "clip") ap.clipsSumiram++; else ap.falhas++; }
		else if (it.tipo == "clip") { a.set("color", origClip[it.id]); ap.clips++; }
		else if (it.tipo == "faixa") {
			var api = new LiveAPI("id " + it.id), o = orig[it.id];
			api.set("name", o.name);
			api.set("color", o.color);
			if (o.rota && rotaAtual(api, "output").nome != o.rota) {
				var r = achaRota(rotasDisponiveis(api, "output"), o.rota);
				if (!r || !gravaRota(api, "output", r.r)) log("  [" + o.name + "]: saída não voltou para \"" + o.rota + "\"\n");
			}
			ap.feitos++;
		} else if (it.tipo == "busvelho") {
			// bus que já existia: volta Monitor e entrada de antes
			var bv = new LiveAPI("id " + it.id), ob = origBus[it.id];
			try { bv.set("current_monitoring_state", ob.mon); } catch (e) { }
			voltaEntrada(bv, ob.inp);
		} else {
			var b = new LiveAPI("id " + it.id), nd = 0, ncl = 0;
			try { nd = b.getcount("devices"); } catch (e) { }
			try { ncl = b.getcount("arrangement_clips"); } catch (e) { }
			if (nd > (devCriado[it.id] || 0) || ncl) { ap.mantidos++; log("  bus [" + str(b.get("name")) + "] mantido: " + nd + " devices, " + ncl + " clips\n"); }
			else if (moldeApagado) {
				// a faixa-molde foi apagada no APPLY: esta cópia (igual a ela) volta a ser a faixa-molde
				var mo = moldeApagado; moldeApagado = null;
				b.set("name", mo.name);
				try { b.set("color", mo.color); } catch (e) { }
				try { b.set("current_monitoring_state", mo.mon); } catch (e) { }
				voltaEntrada(b, mo.inp);
				ap.moldeVoltou = 1;
				log("  faixa-molde [" + mo.name + "] de volta\n");
			} else {
				var set = new LiveAPI("live_set"), nt = set.getcount("tracks"), idx = -1;
				for (var k = 0; k < nt; k++) if (Number(aponta("live_set tracks " + k).id) == it.id) { idx = k; break; }
				if (idx >= 0) { set.call("delete_track", idx); ap.apagados++; } else ap.falhas++;
			}
		}
	} catch (e) { ap.falhas++; log("ESPANADOR erro no UNDO: " + e + "\n"); }
	if (busy) proximo(passoUndo, PAUSA);
}
