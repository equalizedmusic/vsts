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
        var p = location.pathname, secao = p.indexOf('/estudio') === 0 ? 'estudio/' : p.indexOf('/contato') === 0 ? 'contato/' : p.indexOf('/privacidade') === 0 ? null : '';
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

        var dlg = document.querySelector('.zoom');
        if (!dlg || !dlg.showModal) return;
        var img = dlg.querySelector('img');
        document.querySelectorAll('.ampliar').forEach(function (b) {
            b.addEventListener('click', function () { img.src = b.getAttribute('data-img'); dlg.showModal(); });
        });
        dlg.querySelector('.fechar').addEventListener('click', function () { dlg.close(); });
    });
})();
