// Idioma (PT primeiro; quem tem o navegador em outra língua abre em inglês) e imagem ampliada.
(function () {
    var html = document.documentElement;
    var salvo = null;
    try { salvo = localStorage.getItem('isp-idioma'); } catch (e) { /* modo privado */ }
    var nav = (navigator.language || 'pt').toLowerCase();
    aplicar(salvo || (nav.indexOf('pt') === 0 ? 'pt' : 'en'));

    function aplicar(l) { html.lang = l === 'en' ? 'en' : 'pt-BR'; }

    document.addEventListener('DOMContentLoaded', function () {
        var bt = document.querySelector('.idioma');
        if (bt) bt.addEventListener('click', function () {
            var novo = html.lang === 'en' ? 'pt' : 'en';
            aplicar(novo);
            try { localStorage.setItem('isp-idioma', novo); } catch (e) { /* ignora */ }
        });

        var dlg = document.querySelector('.zoom');
        if (!dlg || !dlg.showModal) return;
        var img = dlg.querySelector('img');
        document.querySelectorAll('.ampliar').forEach(function (b) {
            b.addEventListener('click', function () { img.src = b.getAttribute('data-img'); dlg.showModal(); });
        });
        dlg.querySelector('.fechar').addEventListener('click', function () { dlg.close(); });
    });
})();
