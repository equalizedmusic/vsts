// Testa a edição da janela do DESENROLA (desenrola_view.js) sem o Max: partes (tipo, tamanho, ordem, + PART, dividir,
// tirar) e blocos das faixas (pintar, pincel, mover, começo/fim, apagar), com os blocos andando junto com as partes.
// DATA=<dict.json do sim-desenrola (DUMP)> node sim-desenrola-janela.js <pasta do build>
const fs = require('fs'), vm = require('vm'), path = require('path');
const B = process.argv[2] + '/';
const store = { desenrola_data: fs.readFileSync(process.env.DATA, 'utf8') };
const saidas = [];
const mg = new Proxy({ text_measure: (s) => [String(s).length * 6, 10] }, { get: (o, k) => (k in o ? o[k] : () => {}) });
const ctx = {
  mgraphics: mg, box: { rect: [0, 0, 1360, 840] }, outlets: 1,
  Dict: class { constructor(n) { this.n = n; } stringify() { return store[this.n] || '{}'; } parse(s) { store[this.n] = s; } },
  Task: class { constructor() {} schedule() {} repeat() {} cancel() {} },
  outlet: (...a) => saidas.push(a), arrayfromargs: (a) => Array.prototype.slice.call(a), post: () => {},
  max: { os: 'windows', version: '9.0.5', launchbrowser: () => {} },
  JSON, Math, Date, Number, String, Array, Object, parseFloat, parseInt, isNaN, Infinity, encodeURIComponent, decodeURIComponent, escape, unescape,
};
vm.createContext(ctx);
vm.runInContext(fs.readFileSync(B + 'desenrola_view.js', 'utf8'), ctx);
const ev = (c) => vm.runInContext(c, ctx);
ev('lic(' + JSON.stringify(JSON.stringify({ ok: 1, ocupado: 0, email: 'teste', token: '', msg: '', ate: 0 })) + ')');
ev('loadbang(); paint()');
let erros = 0;
const confere = (ok, txt) => { console.log((ok ? '  ok   ' : '  ERRO ') + txt); if (!ok) erros++; };
const D = () => ev('data');
const ini = () => { const o = []; let a = 0; for (const p of D().partes) { o.push(a); a += p.n; } return o; };
const tot = () => D().partes.reduce((s, p) => s + p.n, 0);
const xDe = (bar) => ev('xDe(' + bar + ')');
const yParte = () => ev('TL_Y') + 20;
const yFaixa = (t) => ev('TOP') + (t - ev('first')) * ev('lay.laneH') + ev('lay.laneH') / 2;
const T = (n) => D().tracks.findIndex((t) => t.name === n);
const blocos = (n) => JSON.stringify(D().tracks[T(n)].blocos.map((b) => [b.a, b.b]));
const valido = () => D().tracks.every((t) => t.blocos.every((b, i) => b.a >= 0 && b.b <= tot() + 1e-9 && b.b > b.a && (i == 0 || b.a >= t.blocos[i - 1].b - 1e-9)));
// mouse: clicar, arrastar e soltar (o Max manda ondrag com botão 0 no fim)
function arrasta(x0, y0, x1, y1, mods = {}) {
  ev(`onclick(${x0}, ${y0}, 1, 0, ${mods.shift ? 1 : 0}, 0, ${mods.alt ? 1 : 0}, 0); paint()`);
  if (x1 != null) { ev(`ondrag(${(x0 + x1) / 2}, ${y1}, 1, 0, ${mods.shift ? 1 : 0}, 0, ${mods.alt ? 1 : 0}); paint()`); ev(`ondrag(${x1}, ${y1}, 1, 0, ${mods.shift ? 1 : 0}, 0, ${mods.alt ? 1 : 0}); paint()`); }
  ev(`ondrag(${x1 == null ? x0 : x1}, ${y1 == null ? y0 : y1}, 0, 0, 0, 0, 0); paint()`);
}
const clica = (x, y, mods) => arrasta(x, y, null, null, mods);

const p0 = D().partes.map((p) => p.p + ' ' + p.n).join(' · ');
console.log('partes: ' + p0);
confere(valido(), 'blocos válidos ao abrir');
const kick0 = blocos('Kick');

// 1. borda da INTRO: +4 compassos ⇒ tudo depois anda 4; blocos da INTRO crescem
const i0 = ini();
arrasta(xDe(i0[1]), yParte(), xDe(i0[1] + 4), yParte());
confere(D().partes[0].n === 20 && tot() === 148, 'borda da INTRO: 16 → 20, total 148 (o resto anda junto)');
confere(blocos('Kick').startsWith('[[0,20],[20,52]'), 'Kick: bloco da INTRO cresceu e o do DROP 1 andou 4 ' + blocos('Kick'));
// 2. Alt + borda: a vizinha cede (total igual)
arrasta(xDe(20), yParte(), xDe(24), yParte(), { alt: 1 });
confere(D().partes[0].n === 24 && D().partes[1].n === 28 && tot() === 148, 'Alt+borda: INTRO 24, DROP 1 cede (28), total igual');
// 3. muda a parte de lugar: arrasta o BUILDUP (índice 3) para antes do BREAK (índice 2)
const idB = D().partes[3].id, ii = ini();
arrasta(xDe(ii[3] + 4), yParte(), xDe(ii[2] + 2), yParte());
confere(D().partes[2].id === idB && D().partes[2].p === 'BUILDUP', 'BUILDUP arrastado para antes do BREAK');
const riser = D().tracks[T('Riser')].blocos;
confere(riser.length === 1 && riser[0].a === ini()[2] && riser[0].b === ini()[2] + 8, 'Riser (FX, só no BUILDUP) andou junto com a parte ' + blocos('Riser'));
// 4. clique esquerdo só seleciona (não troca o tipo)
const iBr = D().partes.findIndex((p) => p.p === 'BREAK'), xb = xDe(ini()[iBr] + 2);
const antes4 = JSON.stringify(D().partes);
clica(xb, yParte());
confere(JSON.stringify(D().partes) === antes4 && ev('sel') === D().partes[iBr].id, 'clique esquerdo na parte só seleciona (tipo igual: ' + D().partes[iBr].p + ')');
// 4b. botão direito (ctrl = 1 no Max) abre o menu; escolher BUILDUP troca o tipo e as faixas vão para o padrão do tipo novo
const menuItem = (txt) => { ev('paint()'); const h = ev('hits').find((x) => x.tipo === 'menu' && ev('menu').itens[x.k].t === txt); return h; };
ev(`onclick(${xb}, ${yParte()}, 1, 0, 0, 0, 0, 1); paint()`);
confere(!!ev('menu') && JSON.stringify(D().partes) === antes4, 'botão direito abre o menu sem mexer na parte');
const itens = ev('menu.itens.map((i) => i.t || "-").join(" ")');
confere(itens === 'INTRO BREAK BUILDUP DROP FINAL - RENOMEAR… DIVIDIR TIRAR', 'menu: ' + itens);
let hm = menuItem('BUILDUP');
ev(`onclick(${hm.x + 10}, ${hm.y + 8}, 1, 0, 0, 0, 0, 0); paint()`);
confere(D().partes[iBr].p === 'BUILDUP' && !ev('menu'), 'menu → BUILDUP: a parte vira BUILDUP e o menu fecha');
confere(D().tracks[T('Riser')].blocos.some((b) => b.a === ini()[iBr]), 'Riser ganhou bloco na parte que virou BUILDUP');
// 4c. RENOMEAR: a janela pede o [dialog] (dlg_nome) e o nome volta como nome_parte
ev(`onclick(${xb}, ${yParte()}, 1, 0, 0, 0, 0, 1); paint()`);
hm = menuItem('RENOMEAR…');
saidas.length = 0;
ev(`onclick(${hm.x + 10}, ${hm.y + 8}, 1, 0, 0, 0, 0, 0); paint()`);
confere(saidas.some((s) => s[1] === 'dlg_nome'), 'RENOMEAR abre o diálogo (dlg_nome)');
const tipoRen = D().partes[iBr].p, blocosRen = JSON.stringify(D().tracks.map((t) => t.blocos));
ev('nome_parte("Drop", "Final"); paint()');
confere(D().partes[iBr].nome === 'Drop Final' && D().partes[iBr].p === tipoRen && ev('rotulos(data.partes)[' + iBr + ']') === 'Drop Final', 'nome próprio "Drop Final" (tipo continua ' + tipoRen + ')');
confere(JSON.stringify(D().tracks.map((t) => t.blocos)) === blocosRen, 'renomear não mexe nos blocos');
// escolher um tipo padrão no menu tira o nome próprio
ev(`onclick(${xb}, ${yParte()}, 1, 0, 0, 0, 0, 1); paint()`);
hm = menuItem(tipoRen);
ev(`onclick(${hm.x + 10}, ${hm.y + 8}, 1, 0, 0, 0, 0, 0); paint()`);
confere(!D().partes[iBr].nome && D().partes[iBr].p === tipoRen, 'tipo padrão no menu volta ao nome do tipo');
// clique fora do menu só fecha
ev(`onclick(${xb}, ${yParte()}, 1, 0, 0, 0, 0, 1); paint()`);
const antes4d = JSON.stringify(D());
ev('onclick(5, 5, 1, 0, 0, 0, 0, 0); paint()');
confere(!ev('menu') && JSON.stringify(D()) === antes4d, 'clique fora fecha o menu sem mudar nada');
// 5. duplo clique: a parte divide em duas, sem trocar o tipo
const nAntes = D().partes.length, tipoAntes = D().partes[iBr].p;
clica(xb, yParte()); clica(xb, yParte()); ev(`ondblclick(${xb}, ${yParte()}); paint()`);
confere(D().partes.length === nAntes + 1 && D().partes[iBr].p === tipoAntes && D().partes[iBr + 1].p === tipoAntes, 'duplo clique divide e não troca o tipo (' + tipoAntes + ')');
confere(valido(), 'blocos válidos depois de dividir');
// 6. + PART: 16 compassos no fim, padrão dos grupos
const t6 = tot();
ev('cmdLocal(["__mais"]); paint()');
const ult = D().partes[D().partes.length - 1];
confere(ult.n === 16 && tot() === t6 + 16, '+ PART põe uma parte de 16 no fim');
confere(D().tracks[T('Pad Long')].blocos.some((b) => b.a === t6 && b.b === t6 + 16), 'parte nova (BREAK) já com os blocos do padrão (Pad Long)');
// e dá para arrastar ela para o começo
ev('ajusta(); paint()');
arrasta(xDe(t6 + 8), yParte(), xDe(1), yParte());
confere(D().partes[0].id === ult.id, 'a parte nova arrastada para o começo');
confere(D().tracks[T('Pad Long')].blocos[0].a === 0 && D().tracks[T('Pad Long')].blocos[0].b === 16, 'Pad Long: o bloco foi junto para o começo');
// 7. Alt+clique tira a parte
const n7 = D().partes.length;
clica(xDe(4), yParte(), { alt: 1 });
confere(D().partes.length === n7 - 1 && tot() === t6, 'Alt+clique tira a parte');
confere(valido(), 'blocos válidos depois de tirar');

// blocos
ev('ajusta(); paint()');
const tv = T('Vox Chop'), yv = yFaixa(tv);
D().tracks[tv].blocos = []; ev('paint()');
// 8. pintar arrastando no vazio (grade de 4)
arrasta(xDe(9), yv, xDe(22), yv);
confere(blocos('Vox Chop') === '[[8,24]]', 'arrastar no vazio pinta 8–24 (grade de 4) ' + blocos('Vox Chop'));
// 9. pincel 8: clique no vazio pinta 8 compassos
ev('pincel = 8; paint()');
clica(xDe(41), yv);
confere(blocos('Vox Chop') === '[[8,24],[40,48]]', 'pincel 8: clique pinta 40–48 ' + blocos('Vox Chop'));
// 10. pincel PARTE: a parte inteira
ev('pincel = 0; paint()');
const pIdx = 3, ip = ini()[pIdx], np = D().partes[pIdx].n;
clica(xDe(ip + 1), yv);
confere(D().tracks[tv].blocos.some((b) => b.a === ip && b.b === ip + np), 'pincel PARTE: clique pinta a parte inteira');
// 11. mover o bloco 40–48 para 52–60
arrasta(xDe(44), yv, xDe(56), yv);
confere(D().tracks[tv].blocos.some((b) => b.a === 52 && b.b === 60), 'arrastar o bloco move de 40–48 para 52–60 ' + blocos('Vox Chop'));
// 12. começo e fim do bloco 8–24: começo para 12, fim para 30 (Shift = de 1 em 1)
arrasta(xDe(8) + 1, yv, xDe(12) + 1, yv);
arrasta(xDe(24) - 1, yv, xDe(30) - 1, yv, { shift: 1 });
confere(D().tracks[tv].blocos[0].a === 12 && D().tracks[tv].blocos[0].b === 30, 'borda do bloco: começo 8 → 12, fim 24 → 30 (Shift) ' + blocos('Vox Chop'));
// 13. Alt+clique apaga o bloco
const n13 = D().tracks[tv].blocos.length;
clica(xDe(20), yv, { alt: 1 });
confere(D().tracks[tv].blocos.length === n13 - 1 && !D().tracks[tv].blocos.some((b) => b.a === 12), 'Alt+clique apaga o bloco');
confere(D().tracks[tv].manual === 1, 'faixa marcada como editada à mão');
// 14. grupo: clique volta os blocos ao padrão do grupo novo
const ig = T('Riser');
ev(`onclick(${ev('XG') + 20}, ${yFaixa(ig)}, 1, 0, 0, 0, 0, 0); paint()`);
const dg = D(), gr = dg.tracks[ig].g;
confere(JSON.stringify(dg.tracks[ig].blocos) === JSON.stringify(ev('blocosDaRegra(' + gr + ', data.partes)')), 'troca de grupo: blocos = padrão do grupo ' + ev('GRUPOS[' + gr + ']'));
// 15. zoom e rolagem
ev('zoom(1 / 4); paint()');
confere(ev('vista.n') < tot() / 3, 'zoom + mostra menos compassos (' + Math.round(ev('vista.n')) + ')');
ev('onwheel(700, 500, 3, 0, 0, 0, 0, 0, 0); paint()');
confere(ev('vista.s') > 0 || ev('vista.n') >= tot(), 'roda horizontal anda na linha do tempo');
// 16. o motor recebe tudo
const salvo = JSON.parse(store.desenrola_data);
confere(JSON.stringify(salvo.partes) === JSON.stringify(D().partes) && saidas.some((s) => s[1] === 'marcas'), 'mudanças vão para o dict e o motor é avisado (marcas)');
confere(valido(), 'blocos válidos no fim');
console.log(erros ? erros + ' ERRO(S)' : 'TUDO OK');
process.exit(erros ? 1 : 0);
