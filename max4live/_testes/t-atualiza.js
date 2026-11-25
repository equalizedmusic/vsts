// atualização automática (25/11/2026): o motor guarda o instalador assinado do /v1/versao e o UPDATE manda ao
// [node.script]; o texto do botão acompanha o atu_estado; erro / Node mudo / sem dados assinados = navegador.
//   node t-atualiza.js <pasta do build com vassoura/ rodo/ espanador/ desenrola/>
// (o Node em si — assinatura, download, SHA-256, instalador — tem teste à parte imitando o max-api: ver licenca/README.md item 8)
const fs = require('fs'), vm = require('vm'), path = require('path');
const BUILD = process.argv[2] || (() => { throw new Error('passe a pasta do build'); })();
let erros = 0; const confere = (ok, t) => { console.log((ok ? '  ok   ' : '  ERRO ') + t); if (!ok) erros++; };

function abre(dev, resposta) {
	const outs = [], tasks = [], navegador = [];
	const ctx = {
		outlet: (n, ...a) => outs.push([n, ...a]), post: () => {},
		Task: class { constructor(f) { this.f = f; this.c = false; tasks.push(this); } schedule() {} repeat() {} cancel() { this.c = true; } },
		Dict: class { stringify() { return '{}'; } parse() {} }, LiveAPI: class {}, File: class { constructor() { this.isopen = false; } close() {} },
		max: { os: 'windows', launchbrowser: (u) => navegador.push(u) },
		Math, JSON, Date, Number, String, parseFloat, isNaN, Array, Infinity, isFinite, Float32Array, encodeURIComponent, decodeURIComponent,
		XMLHttpRequest: class {
			open(m, u) { this.u = u; } send() { this.readyState = 4; this.status = 200; this.responseText = /versao\?/.test(this.u) ? JSON.stringify(resposta) : '{}'; setTimeout(() => this.onreadystatechange && this.onreadystatechange(), 1); }
		},
		patcher: { filepath: 'C:/x/X.amxd' },
	};
	vm.createContext(ctx);
	vm.runInContext(fs.readFileSync(path.join(BUILD, dev, dev + '.js'), 'utf8'), ctx);
	const ult = (canal, nome) => [...outs].reverse().find((o) => o[0] === canal && o[1] === nome);
	return { m: ctx, outs, tasks, navegador, ult };
}
const espera = (ms) => new Promise((r) => setTimeout(r, ms));

(async () => {
	for (const dev of ['vassoura', 'rodo', 'espanador', 'desenrola']) {
		console.log(dev);
		const inst = { dados: 'RAADOS', assinatura: 'QVNT' };
		const a = abre(dev, { ok: true, atualizacao: { versao: '9.9.9', url: 'x', instalador: inst } });
		a.m.checaVersao(); await espera(20);
		confere(a.m.nova === '9.9.9' && a.m.novaInst && a.m.novaInst.dados === 'RAADOS', 'guardou versão nova e instalador assinado');
		a.m.baixar_nova();
		const pedido = a.ult(1, 'atualizar');
		confere(pedido && pedido[2] === dev && pedido[3] === a.m.VERSAO && pedido[4] === 'RAADOS' && pedido[5] === 'QVNT', 'UPDATE manda atualizar <pacote> <versão> <dados> <assinatura> ao Node');
		confere(a.ult(2, 'atu')[2] === 'UPDATING 9.9.9…', 'botão: UPDATING');
		confere(a.navegador.length === 0, 'não abriu o navegador');
		const n1 = a.outs.length; a.m.baixar_nova();
		confere(!a.outs.slice(n1).some((o) => o[1] === 'atualizar'), '2º clique durante a atualização: ignorado');
		a.m.atu_estado('baixando', '40%');
		confere(a.ult(2, 'atu')[2] === 'DOWNLOADING 9.9.9 · 40%…', 'botão: ' + a.ult(2, 'atu')[2]);
		confere(a.tasks.at(-1).c, 'Node respondeu: cancelou o prazo de 8 s');
		a.m.atu_estado('instalando', '');
		a.m.atu_estado('pronto', '9.9.9');
		confere(a.ult(2, 'atu')[2] === 'UPDATED TO 9.9.9 · RELOAD THE DEVICE', 'botão: ' + a.ult(2, 'atu')[2]);
		a.m.baixar_nova();
		confere(a.navegador.length === 0, 'depois de pronto, clique não faz nada');
		const n2 = a.outs.length; a.m.janela();
		confere(a.outs.slice(n2).some((o) => o[0] === 2 && o[1] === 'atu' && /UPDATED/.test(o[2])), 'reabrir a janela reenvia o andamento');

		const b = abre(dev, { ok: true, atualizacao: { versao: '9.9.9', url: 'x', instalador: inst } });
		b.m.checaVersao(); await espera(20); b.m.baixar_nova();
		b.m.atu_estado('erro', 'assinatura do servidor não conferiu');
		confere(b.navegador.length === 1 && /\/baixar\//.test(b.navegador[0]), 'erro: abre o navegador (' + b.navegador[0] + ')');
		confere(b.ult(2, 'atu')[2] === 'UPDATE 9.9.9 · DOWNLOAD', 'botão depois do erro: DOWNLOAD');

		const c = abre(dev, { ok: true, atualizacao: { versao: '9.9.9', url: 'x', instalador: inst } });
		c.m.checaVersao(); await espera(20); c.m.baixar_nova();
		c.tasks.at(-1).f();
		confere(c.navegador.length === 1, 'Node mudo por 8 s: abre o navegador');

		const d = abre(dev, { ok: true, atualizacao: { versao: '9.9.9', url: 'x' } });
		d.m.checaVersao(); await espera(20); d.m.baixar_nova();
		confere(d.navegador.length === 1 && !d.outs.some((o) => o[1] === 'atualizar'), 'sem instalador assinado: navegador direto');
	}
	console.log(erros ? `RESULTADO: ${erros} ERRO(S)` : 'RESULTADO: OK');
	process.exit(erros ? 1 : 0);
})();
