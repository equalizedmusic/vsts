# RESENHA — novidades

Pacote do instalador: `InsideStudioSP-Resenha`. A versão é o primeiro título `## x.y.z` deste arquivo:
ela entra no plugin, no instalador e no release `resenha-vx.y.z` do GitHub (com o texto da seção como nota).
Para lançar uma versão nova, escreva a seção nova AQUI EM CIMA, no mesmo commit da mudança.

## 0.4.0 — 2026-11-08
- Oscilador novo: ENGINE VA por timbre (analógico virtual, no molde dos leads e arps de psy feitos em synth de subtração). Serra, quadrada e pulso 25%/12% com até 8 vozes desafinadas abertas no estéreo, 2º oscilador (uníssono, oitava ou quinta) e filtro de 24 dB (LP, BP ou HP) com drive e envelope curto no corte: a nota "fala" no ataque. O FM continua como opção (ENGINE = FM) para os timbres metálicos.
- No VA os knobs da 1ª fileira mudam de papel: RATIO = intervalo do OSC 2, FM = nível do OSC 2, FEEDBACK = DETUNE, FM DEC = tempo do envelope do filtro; UNISON = número de vozes.
- PAPO: o quanto a troca soa como conversa e não como sequência. Falas de tamanhos diferentes, respiro antes da resposta ou resposta que entra por cima da pergunta, resposta um pouco fora da grade, legato com glide nas notas vizinhas e a última nota da fala mais longa. 0% = grade dura, como até a 0.3.
- Os 120 presets de fábrica refeitos no VA (a CONVERSA FM continua no FM), com PAPO. O Init também abre no VA.
- O dado dos timbres sorteia VA na maioria das vezes; GERAR CONVERSA também sorteia o PAPO.
- Projetos e presets do usuário da 0.3 abrem como eram (ENGINE = FM, PAPO = 0%).

## 0.3.0 — 2026-10-29
- Timbre mais cheio: UNISON por timbre (3 cópias desafinadas abertas no estéreo).
- FILTER por timbre: LP, BP ou VOWEL. No VOWEL o lead "fala" (u-o-a-e-i): o CUTOFF escolhe a vogal e o F.ENV abre a boca no ataque.
- MOVE e RATE por timbre: movimento no tempo do DAW (1/16 a 2 compassos) no FM e no filtro/vogal.
- Conversa por tempo: TROCA A CADA ganhou ½ compasso e 1 compasso, e a resposta agora fecha na tônica (a pergunta fica no ar).
- Ritmo do gerador virou riff que se repete (células por compasso ou "rolando" 3+3+3+3+4), com virada a cada 2 compassos.
- CONTORNO novo: PEDAL (nota fixa alternando com uma linha que anda, o "0 5 0 6" do full on).
- Categoria nova LEAD FALANTE (12 presets) e os 108 presets de fábrica refeitos com UNISON, FILTER e MOVE. O plugin abre no "Boca Aberta".

## 0.2.0 — 2026-10-29
- BEND por timbre: a última nota de cada fala mergulha (ou sobe) até 24 semitons durante a nota, e a conversa desenha a cauda.
- CONTORNO DAS NOTAS no gerador: MOTIVO, CORRIDA (escada que sobe), QUEDA e ONDA.
- DUCK na saída: o som abaixa a cada tempo e volta, como o sidechain do bumbo.
- Categoria nova CONVERSA FM (12 presets): leads FM cheios de harmônico, escadas de 1/16 que fecham num mergulho e resposta em outra oitava. O plugin abre no "Última Palavra".
- GERAR CONVERSA também sorteia o contorno, e o dado dos timbres sorteia o BEND.

## 0.1.0 — 2026-10-29
- Primeira versão: call & response FM com instalador próprio.
- 4 timbres FM (A, B, C, D) que se revezam na frase.
- Gerador de frase na escala (notas, ritmo, gate), com travas e GERAR CONVERSA.
- Troca de timbre a cada N notas, com ordem e tipo de resposta (LIVRE, ECO, VARIA, INVERTE) e altura da resposta.
- Tocar COM NOTA (segure a nota no DAW e a frase vem transposta) ou SEMPRE (com o play).
- ARRASTAR MIDI: leva a frase para o DAW.
- Drive, delay ping-pong e reverb.
- Presets de fábrica, presets do usuário e bancos `.rsnbank` para importar.
- Ativação por licença: na primeira vez o plugin pede o e-mail e o token da licença. Sem ativar, o instrumento fica mudo.
