// © Rogerio Cardoso / InsideStudioSP — todos os direitos reservados. Proibida a cópia, redistribuição ou obra derivada.
// Internet para o motor quando o [v8] não tem XMLHttpRequest (Live 12.2/12.3 = Max 9.0.x). Roda no [js] CLÁSSICO
// (motor antigo do Max, ES5: sem let, sem arrow, sem BigInt), que tem XMLHttpRequest há muitas versões.
// Gerado pelo build como <dev>_http.js. Protocolo (ver licHttp em isp_licenca.js):
//   entra: req <id> <GET|POST> <url> t<corpo em encodeURIComponent>
//   sai:   http_resp <id> <status> t<texto em encodeURIComponent>     (status -1 = este [js] também não tem XMLHttpRequest)
autowatch = 0;
inlets = 1;
outlets = 1;

var pend = {};   // referência guardada: o GC não leva o pedido no meio

function responde(id, status, texto) {
	outlet(0, "http_resp", id, status, "t" + encodeURIComponent(texto == null ? "" : String(texto)));
}

function req(id, metodo, url, corpo) {
	if (typeof XMLHttpRequest == "undefined") { responde(id, -1, ""); return; }
	var c = corpo == null ? "" : String(corpo);
	try { c = c.charAt(0) == "t" ? decodeURIComponent(c.substr(1)) : ""; } catch (e) { c = ""; }
	try {
		var x = new XMLHttpRequest();
		pend[id] = x;
		x.open(String(metodo), String(url), true);
		// SEM setRequestHeader (derruba o Live no Max 9.1.5 do Mac; o servidor não precisa do cabeçalho)
		x.onreadystatechange = function () {
			if (x.readyState != 4) return;
			delete pend[id];
			var t = "";
			try { t = x.responseText; } catch (e) { t = ""; }
			var st = 0;
			try { st = Number(x.status) || 0; } catch (e) { st = 0; }
			responde(id, st, t);
		};
		if (String(metodo) == "GET") x.send(); else x.send(c);
	} catch (e) {
		delete pend[id];
		post("isp_http: falhou (" + e + ")\n");
		responde(id, 0, "");
	}
}

function anything() { }
