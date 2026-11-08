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

        // oferta por tempo limitado: contador regressivo; ao zerar, some com selo, "de" riscado e faixa
        var conts = document.querySelectorAll('.contador[data-fim]');
        if (conts.length) {
            var fim = Date.parse(conts[0].getAttribute('data-fim'));
            var d2 = function (n) { return (n < 10 ? '0' : '') + n; };
            var tique = function () {
                var r = Math.floor((fim - Date.now()) / 1000);
                if (r <= 0) {
                    document.querySelectorAll('.so-oferta').forEach(function (el) { el.remove(); });
                    return false;
                }
                var txt = (r >= 86400 ? Math.floor(r / 86400) + 'd ' : '') + d2(Math.floor(r % 86400 / 3600)) + 'h '
                        + d2(Math.floor(r % 3600 / 60)) + 'm ' + d2(r % 60) + 's';
                conts.forEach(function (c) { c.textContent = txt; });
                return true;
            };
            if (tique()) var iv = setInterval(function () { if (!tique()) clearInterval(iv); }, 1000);
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
