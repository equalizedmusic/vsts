# ALIGN — novidades

Pacote do instalador: `InsideStudioSP-Align`. A versão é o primeiro título `## x.y.z` deste arquivo:
ela entra no plugin, no instalador e no release `align-vx.y.z` do GitHub (com o texto da seção como nota).
Para lançar uma versão nova, escreva a seção nova AQUI EM CIMA, no mesmo commit da mudança.

## 0.1.0 — 2026-10-31
- Primeira versão do ALIGN: alinha a fase do kick com o baixo, sozinho, com um botão só.
- Vai no canal do BAIXO; o KICK entra pelo sidechain (no Ableton: Sidechain do ALIGN → faixa do kick).
- ALIGN liga o corretor automático contínuo: a cada batida ele mede como a cauda do kick e a nota do baixo se somam
  no grave, escolhe a polaridade e a rotação de fase que somam sem cancelar e guarda a correção por NOTA. A troca é
  suave (crossfade de 10 ms, enquanto o kick soa) e só acontece se melhorar mais de 1 dB.
- LEVEL GUARD no mesmo botão: alinhar não pode virar soma. Onde kick e baixo se cruzam, o grave do baixo abaixa só o
  que passaria da soma de potência (nem buraco, nem embolar), até −6 dB. A relação de volume kick/baixo se mantém.
- A correção é só no grave do baixo, com X-OVER (crossover Linkwitz-Riley) que acompanha a nota (~1,6 × a fundamental,
  60 a 110 Hz). O resto do baixo passa sem mudar.
- Latência zero. Desligado, o som passa sem nenhuma alteração (serve de A/B), e o ALIGN continua ouvindo e aprendendo.
- Tela: osciloscópio de 2 tempos travado no kick (FULL ou só o grave), zoom na passagem kick → b com a soma, o alvo e a
  soma "sem LEVEL GUARD"; LOW END (dB perdidos ou somados), CORRELATION, CORRECTION (AUTO) e NOTE MEMORY. O
  osciloscópio desenha a 4x (interpolado), sem mexer no áudio.
