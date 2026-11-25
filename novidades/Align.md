# ALIGN — novidades

Pacote do instalador: `InsideStudioSP-Align`. A versão é o primeiro título `## x.y.z` deste arquivo:
ela entra no plugin, no instalador e no release `align-vx.y.z` do GitHub (com o texto da seção como nota).
Para lançar uma versão nova, escreva a seção nova AQUI EM CIMA, no mesmo commit da mudança.

## 0.3.3 — 2026-11-25
- **Atualização automática:** o aviso de versão nova no pé da janela ganha o botão **ATUALIZAR**: o plugin baixa o
  instalador, confere a assinatura e o arquivo e instala sozinho. No Windows ele espera você fechar o DAW e roda o
  instalador (o Windows pede permissão uma vez); no Mac pede a senha de administrador e instala na hora. Depois é só
  abrir o DAW de novo. O **BAIXAR** pelo navegador continua lá para quem preferir.
- Vale a partir da próxima versão: esta ainda se instala pelo aviso de sempre.
- O som e as funções não mudam.

## 0.3.2 — 2026-11-24
- **Ativação automática:** se você já ativou outro plugin ou device InsideStudioSP neste computador, com o mesmo e-mail, este
  se ativa sozinho ao abrir, sem digitar o token. Se o seu e-mail não tem este pacote, a tela de ativação já abre com o
  e-mail preenchido e avisa. Desativar continua valendo: o plugin não se reativa sozinho depois.
- Aviso de autoria na tela da licença (© Rogerio Cardoso / InsideStudioSP).
- O som e as funções não mudam.

## 0.3.1 — 2026-11-18
- Versão oficial (20/11/2026), aprovada no teste junto com o CHUTE: chega como atualização para quem tem a 0.2.0.
- LINK mais firme (mesma correção do CHUTE 0.1.2): o kick recriado não some mais na volta do loop, num pulo da régua,
  com a faixa do kick um bloco na frente ou na exportação, e a ligação não cai mais sozinha (antes caía com 1,5 s
  sem áudio do CHUTE).
- Tela no mesmo tamanho do CHUTE e do BAREBASS (1000 × 752, topo e título iguais), com o osciloscópio maior.

## 0.3.0 — 2026-11-18
- TESTE (pré-lançamento: não chega como atualização para quem já tem a 0.2.0).
- LINK com o CHUTE: sem sidechain, o ALIGN acha o CHUTE do mesmo projeto sozinho e recria o corpo de cada batida na
  posição certa. Prioridade: sidechain ligado e com som > LINK > aviso "ligue o sidechain ou use o CHUTE". No alto
  aparece LINK · <nome da faixa do CHUTE>; com vários CHUTEs, clique para escolher qual seguir.
- DUCK (faixa DOSE): sidechain clássico com dosagem. O baixo inteiro abaixa junto com o envelope do kick, até a
  porcentagem da barra; a barra mostra quanto abaixou. Em 0 fica OFF (projeto antigo abre igual).
- Limites do LINK: efeitos depois do CHUTE (EQ, saturador) não entram na conta (para isso, sidechain); faixa
  congelada / passada para áudio ou DAW que isola cada plugin (Bitwig com sandbox) = sem LINK.

## 0.2.0 — 2026-11-11
- DOSE: o ALIGN segue sempre automático (lendo a nota e corrigindo em tempo real), e agora dá para dosar cada parte
  na faixa nova embaixo do osciloscópio:
  - PHASE liga/desliga a rotação de fase do grave;
  - POLARITY liga/desliga a inversão de polaridade (com as duas desligadas o grave passa direto);
  - LEVEL GUARD liga/desliga o controle de nível, e o GUARD DEPTH (0–100%) dosa quanto ele abaixa;
  - REACT (SLOW / MID / FAST) é a rapidez com que ele aprende cada nota e troca de correção;
  - X-OVER MAX (60–150 Hz) é o teto do grave corrigido; o X-OVER continua acompanhando a nota.
- Duplo clique nas barras volta ao padrão. Projeto antigo abre igual à 0.1.0 (tudo ligado, 100%, MID, 110 Hz).
- O painel CORRECTION mostra OFF na parte que estiver desligada.

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
