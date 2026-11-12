// Idioma (PT primeiro; quem tem o navegador em outra língua abre em inglês) e imagem ampliada.
(function () {
    var html = document.documentElement;
    var salvo = null;
    try { salvo = localStorage.getItem('isp-idioma'); } catch (e) { /* modo privado */ }
    var nav = (navigator.language || 'pt').toLowerCase();
    aplicar(salvo || (nav.indexOf('pt') === 0 ? 'pt' : 'en'));

    function aplicar(l) { html.lang = l === 'en' ? 'en' : 'pt-BR'; }

    document.addEventListener('DOMContentLoaded', function () {
        // menu: marca a seção aberta (página de pack conta como Loja)
        var p = location.pathname, secao = p.indexOf('/estudio') === 0 ? 'estudio/' : p.indexOf('/contato') === 0 ? 'contato/' : p.indexOf('/carrinho') === 0 ? 'carrinho/' : p.indexOf('/privacidade') === 0 ? null : '';
        document.querySelectorAll('nav a').forEach(function (a) {
            var h = a.getAttribute('href').replace(/^\.*\//, '').replace(/^(\.\.\/)+/, '');
            if (h === secao) { a.classList.add('atual'); a.setAttribute('aria-current', 'page'); }
        });

        var bt = document.querySelector('.idioma');
        if (bt) bt.addEventListener('click', function () {
            var novo = html.lang === 'en' ? 'pt' : 'en';
            aplicar(novo);
            try { localStorage.setItem('isp-idioma', novo); } catch (e) { /* ignora */ }
        });

        // oferta por tempo limitado: cada contador segue a sua data (data-fim: o pack pode terminar antes da oferta geral);
        // ao zerar, some com selo, "de" riscado e faixa daquela data. .so-oferta sem data-fim (páginas do servidor) segue o 1º contador.
        var conts = document.querySelectorAll('.contador[data-fim], .relogio[data-fim]');
        if (conts.length) {
            var padrao = conts[0].getAttribute('data-fim');
            var d2 = function (n) { return (n < 10 ? '0' : '') + n; };
            var tique = function () {
                var vivos = 0;
                document.querySelectorAll('.contador[data-fim]').forEach(function (c) {
                    var f = c.getAttribute('data-fim'), r = Math.floor((Date.parse(f) - Date.now()) / 1000);
                    if (r > 0) {
                        vivos++;
                        c.textContent = (r >= 86400 ? Math.floor(r / 86400) + 'd ' : '') + d2(Math.floor(r % 86400 / 3600)) + 'h '
                                      + d2(Math.floor(r % 3600 / 60)) + 'm ' + d2(r % 60) + 's';
                    }
                });
                // relógio grande da Black Friday (MAESTRO): dias / horas / min / seg em blocos
                document.querySelectorAll('.relogio[data-fim]').forEach(function (c) {
                    var r = Math.max(0, Math.floor((Date.parse(c.getAttribute('data-fim')) - Date.now()) / 1000));
                    if (r > 0) vivos++;
                    var v = { d: Math.floor(r / 86400), h: Math.floor(r % 86400 / 3600), m: Math.floor(r % 3600 / 60), s: r % 60 };
                    c.querySelectorAll('[data-u]').forEach(function (b) { b.textContent = d2(v[b.getAttribute('data-u')]); });
                });
                document.querySelectorAll('.so-oferta').forEach(function (el) {
                    if (Date.parse(el.getAttribute('data-fim') || padrao) <= Date.now()) el.remove();
                });
                return vivos > 0;
            };
            if (tique()) var iv = setInterval(function () { if (!tique()) clearInterval(iv); }, 1000);
        }

        // popup de lançamento na loja: abre de novo a cada 12 h (no celular a "sessão" da aba dura dias, por isso não é sessionStorage);
        // "Quero" segue o link, o resto só fecha (✕, "agora não", Esc, clique fora). ?popup na URL força abrir.
        var lanc = document.querySelector('dialog.dl');
        // passada a data da oferta (data-ate), o popup não abre mais nem com ?popup
        if (lanc && Date.parse(lanc.getAttribute('data-ate') || '') <= Date.now()) { lanc.remove(); lanc = null; }
        if (lanc) {
            var visto = 0, JANELA = 12 * 3600 * 1000;
            try { visto = parseInt(localStorage.getItem('isp-lanc-ts'), 10) || 0; } catch (e) { /* modo privado */ }
            if (location.search.indexOf('popup') >= 0 || Date.now() - visto > JANELA) setTimeout(function () {
                if (lanc.showModal) lanc.showModal(); else lanc.setAttribute('open', ''); // navegador antigo sem showModal
                // o showModal foca o 1º botão e o iPhone rolava a caixa até ele, escondendo a faixa e o ✕: volta para o topo
                var x = lanc.querySelector('.dl-fechar');
                try { x.focus({ preventScroll: true }); } catch (e) { /* ignora */ }
                lanc.scrollTop = 0;
                requestAnimationFrame(function () { lanc.scrollTop = 0; });
                try { localStorage.setItem('isp-lanc-ts', String(Date.now())); } catch (e) { /* ignora */ }
            }, 500);
            var fechaLanc = function () { if (lanc.close) lanc.close(); else lanc.removeAttribute('open'); };
            lanc.querySelector('.dl-fechar').addEventListener('click', fechaLanc);
            lanc.querySelector('.dl-depois').addEventListener('click', fechaLanc);
            lanc.addEventListener('click', function (ev) { // só fora da caixa (o padding do dialog também tem target === lanc)
                var r = lanc.getBoundingClientRect();
                if (ev.target === lanc && (ev.clientX < r.left || ev.clientX > r.right || ev.clientY < r.top || ev.clientY > r.bottom)) fechaLanc();
            });
        }

        carrinhoLoja();

        var dlg = document.querySelector('.zoom');
        if (!dlg || !dlg.showModal) return;
        var img = dlg.querySelector('img');
        document.querySelectorAll('.ampliar').forEach(function (b) {
            b.addEventListener('click', function () { img.src = b.getAttribute('data-img'); dlg.showModal(); });
        });
        dlg.querySelector('.fechar').addEventListener('click', function () { dlg.close(); });
    });

    // ---------------- carrinho + cupom (12/11/2026) ----------------
    // A lista fica no navegador (localStorage 'isp-carrinho' = ids dos packs). O preço mostrado é o do site; quem
    // cobra (e confere o cupom e o "uma vez por e-mail") é o servidor em /loja/pagar.
    var CHAVE = 'isp-carrinho';
    function lerCarrinho() {
        try { var v = JSON.parse(localStorage.getItem(CHAVE) || '[]'); return Array.isArray(v) ? v.filter(function (x) { return typeof x === 'string'; }) : []; }
        catch (e) { return []; }
    }
    function gravarCarrinho(l) { try { localStorage.setItem(CHAVE, JSON.stringify(l)); } catch (e) { /* modo privado */ } }
    function esc(s) { return String(s).replace(/[&<>"]/g, function (x) { return { '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[x]; }); }
    function tt(pt, en) { return '<span class="pt">' + esc(pt) + '</span><span class="en" lang="en">' + esc(en) + '</span>'; }
    function reais(v) { return 'R$ ' + v.toFixed(2).replace('.', ','); }
    function centavos(v) { return Math.round(v * 100); }

    function atualizarContador() {
        var n = lerCarrinho().length;
        document.querySelectorAll('.nav-carrinho').forEach(function (a) {
            a.hidden = n === 0 && location.pathname.indexOf('/carrinho') !== 0;
            var b = a.querySelector('.carrinho-n'); if (b) b.textContent = String(n);
        });
    }

    // cupom: confere no servidor e mostra o total com desconto embaixo do campo
    var cupomAtual = null;   // { codigo, pct }
    function ligarCupom(box, aoMudar) {
        var inp = box.querySelector('input[name="cupom"]'), bt = box.querySelector('.cupom-aplicar'), msg = box.querySelector('.cupom-msg');
        if (!inp) return;
        bt.hidden = false;
        var seq = 0;
        var conferir = function () {
            var c = inp.value.trim().replace(/^#+/, '');
            var minha = ++seq;
            if (!c) { box.cupom = null; msg.innerHTML = ''; msg.className = 'cupom-msg'; aoMudar(); return; }
            msg.innerHTML = tt('Conferindo o cupom...', 'Checking the coupon...'); msg.className = 'cupom-msg';
            fetch('/loja/cupom?c=' + encodeURIComponent(c), { cache: 'no-store' }).then(function (r) { return r.json(); }).then(function (j) {
                if (minha !== seq) return;
                if (j && j.ok) {
                    box.cupom = { codigo: j.codigo, pct: j.pct };
                    msg.className = 'cupom-msg ok';
                    msg.innerHTML = tt('Cupom ' + j.codigo + ': ' + j.pct + '% de desconto no total' + (j.umaPorEmail ? ' (vale uma vez por comprador)' : '') + '.',
                                       'Coupon ' + j.codigo + ': ' + j.pct + '% off the total' + (j.umaPorEmail ? ' (once per buyer)' : '') + '.');
                } else {
                    box.cupom = null;
                    msg.className = 'cupom-msg ruim';
                    msg.innerHTML = tt('Cupom não encontrado. Confira o código.', 'Coupon not found. Check the code.');
                }
                aoMudar();
            }).catch(function () { if (minha === seq) { box.cupom = null; msg.innerHTML = ''; aoMudar(); } });
        };
        bt.addEventListener('click', conferir);
        inp.addEventListener('change', conferir);
        inp.addEventListener('keydown', function (ev) { if (ev.key === 'Enter') { ev.preventDefault(); conferir(); } });
    }
    // linha "Total com o cupom" na caixa de compra do pack (o total do carrinho é tratado em montarCarrinho)
    function totalComCupom(box) {
        var out = box.querySelector('.cupom-total');
        var total = parseFloat(box.getAttribute('data-total'));
        if (!box.cupom || !(total > 0)) { if (out) out.remove(); return; }
        if (!out) { out = document.createElement('p'); out.className = 'cupom-total'; box.appendChild(out); }
        var c = centavos(total), d = Math.round(c * box.cupom.pct / 100);
        out.innerHTML = tt('Total com o cupom: ', 'Total with the coupon: ') + '<s>' + reais(c / 100) + '</s> <b>' + reais((c - d) / 100) + '</b>';
    }

    function carrinhoLoja() {
        atualizarContador();
        window.addEventListener('storage', function (ev) { if (ev.key === CHAVE) atualizarContador(); });

        // botão "adicionar ao carrinho" na página do pack
        document.querySelectorAll('.bt-carrinho[data-add]').forEach(function (bt) {
            var id = bt.getAttribute('data-add'), ver = bt.parentNode.querySelector('.bt-ver-carrinho');
            var mostrar = function () { var tem = lerCarrinho().indexOf(id) >= 0; bt.hidden = tem; if (ver) ver.hidden = !tem; };
            mostrar();
            bt.addEventListener('click', function () {
                var l = lerCarrinho();
                if (l.indexOf(id) < 0) l.push(id);
                gravarCarrinho(l); mostrar(); atualizarContador();
            });
        });
        // cupom na caixa de compra do pack
        document.querySelectorAll('form.compra:not(.carrinho-form) .cupom').forEach(function (box) {
            ligarCupom(box, function () { totalComCupom(box); });
        });

        var lista = document.getElementById('carrinho-lista');
        if (!lista) return;
        var cat = {};
        try { cat = JSON.parse(document.getElementById('catalogo').textContent); } catch (e) { /* sem catálogo */ }
        var form = document.getElementById('carrinho-form');
        var box = form.querySelector('.cupom');
        if (/[?&]pago=1/.test(location.search)) { gravarCarrinho([]); history.replaceState(null, '', location.pathname); }

        var aviso = '';
        // combo + avulso do mesmo plugin: o combo fica e o avulso sai (o servidor recusa licença repetida)
        function arrumar(l) {
            var limpa = [], usados = {};
            l.forEach(function (id) { if (cat[id] && limpa.indexOf(id) < 0) limpa.push(id); });
            limpa.filter(function (id) { return cat[id].combo; }).forEach(function (c) { cat[c].combo.forEach(function (x) { usados[x] = c; }); });
            return limpa.filter(function (id) {
                if (!cat[id].combo && usados[id]) { aviso = tt(cat[id].nome + ' saiu: ele já vem no combo ' + cat[usados[id]].nome + '.', cat[id].nome + ' was removed: it is already in the ' + cat[usados[id]].nome + ' bundle.'); return false; }
                return true;
            });
        }

        function montar() {
            var l = arrumar(lerCarrinho());
            gravarCarrinho(l); atualizarContador();
            if (!l.length) {
                form.hidden = true;
                lista.innerHTML = '<div class="carrinho-vazio">' + tt('Seu carrinho está vazio.', 'Your cart is empty.') +
                    '<p><a class="bt bt-sec" href="/#packs">' + tt('Ver os plugins', 'See the plugins') + '</a></p></div>';
                return;
            }
            var sub = 0;
            lista.innerHTML = (aviso ? '<p class="carrinho-aviso">' + aviso + '</p>' : '') + l.map(function (id) {
                var p = cat[id]; sub += centavos(p.brl);
                var de = p.de && (!p.fim || Date.parse(p.fim) > Date.now()) ? '<s>' + reais(p.de) + '</s> ' : '';
                return '<div class="carrinho-item" style="--cor:' + esc(p.cor || '#a78bfa') + '">' +
                    '<a class="ci-img" href="/' + esc(id) + '/"><img src="/img/' + esc(p.capa) + '.webp" alt=""></a>' +
                    '<div class="ci-txt"><a href="/' + esc(id) + '/"><b>' + esc(p.nome) + '</b></a><small>' + tt(p.tipo[0], p.tipo[1]) + '</small></div>' +
                    '<div class="ci-preco">' + de + '<b>' + reais(p.brl) + '</b></div>' +
                    '<button type="button" class="ci-tirar" data-tirar="' + esc(id) + '" aria-label="Tirar do carrinho / Remove">✕</button></div>';
            }).join('') + '<p class="carrinho-mais"><a href="/#packs">' + tt('+ Adicionar mais plugins', '+ Add more plugins') + '</a></p>';
            aviso = '';
            form.querySelector('.carrinho-itens').innerHTML = l.map(function (id) { return '<input type="hidden" name="pacote" value="' + esc(id) + '">'; }).join('');
            form.hidden = false;
            box.setAttribute('data-total', (sub / 100).toFixed(2));
            resumo(sub);
        }
        function resumo(sub) {
            if (sub == null) sub = centavos(parseFloat(box.getAttribute('data-total')) || 0);
            var d = box.cupom ? Math.round(sub * box.cupom.pct / 100) : 0;
            form.querySelector('.cr-sub').textContent = reais(sub / 100);
            form.querySelectorAll('.cr-desc').forEach(function (e) { e.hidden = !d; });
            form.querySelector('.cr-cod').textContent = box.cupom ? box.cupom.codigo + ' (-' + box.cupom.pct + '%)' : '';
            form.querySelector('.cr-desc-v').textContent = '- ' + reais(d / 100);
            form.querySelector('.cr-total').textContent = reais((sub - d) / 100);
        }
        lista.addEventListener('click', function (ev) {
            var b = ev.target.closest('[data-tirar]');
            if (!b) return;
            var id = b.getAttribute('data-tirar');
            gravarCarrinho(lerCarrinho().filter(function (x) { return x !== id; }));
            montar();
        });
        ligarCupom(box, function () { resumo(); });
        window.addEventListener('storage', function (ev) { if (ev.key === CHAVE) montar(); });
        montar();
    }
})();
