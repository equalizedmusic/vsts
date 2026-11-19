# CHUTE · BANCOS DE PRESETS — novidades

Pacote só de presets do CHUTE: `InsideStudioSP-ChuteBancos` (Windows .exe / Mac .dmg), SEM plugin. Leva todos os
`.chutebank` de `instrumentos/chute/bancos/` (a raiz; a fábrica em `bancos/fabrica` vai dentro do plugin) para a
pasta de bancos do CHUTE. O CHUTE mostra os bancos novos no menu na próxima vez que o DAW abrir.

Para lançar um banco novo:
1. `python instrumentos/chute/tools/banco.py gera <modelos.h> instrumentos/chute/bancos/<NOME>.chutebank --pacote <NOME>`
2. Escreva a seção `## x.y.z` nova AQUI EM CIMA (a versão do pacote), no mesmo commit do banco.
3. Push: o workflow `release-chute-bancos` monta os dois instaladores e publica o release `chute-bancos-vx.y.z`
   (sem recompilar o plugin). O instalador do CHUTE também passa a levar o banco no próximo build dele.

(Ainda nenhum banco extra: a primeira seção `## x.y.z` entra junto com o primeiro banco.)