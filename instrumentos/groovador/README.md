# GROOVADOR

Percussão sintetizada (sem bumbo) com gerador de groove e variação de timbre, com cara de bateria eletrônica.
Toca no tempo do DAW (COM PLAY) ou pelas notas MIDI (PADS).

## Instrumentos

| Fileira | Som padrão | Nota no PADS |
|---|---|---|
| RIM | ANALÓGICO curto e estalado | C1 (36) |
| SNARE | ANALÓGICO com ruído | D1 (38) |
| CLAP | RUÍDO com 4 tapas | E1 (40) |
| HAT FECH. | METAL (6 osciladores) | F1 (41) |
| HAT ABERTO | METAL, cortado pelo hat fechado | G1 (43) |
| SHAKER | RUÍDO curto | A1 (45) |
| PERC 1 | FM (bongô/zap) | B1 (47) |
| PERC 2 | 808 (conga/tom) | C2 (48) |

Modelos: **ANALÓG.** (seno com pitch env + 2º modo de membrana no BODY + ruído no filtro), **808** (seno saturado;
BODY = saturação), **FM** (BODY = razão da moduladora; o índice cai junto com a nota), **METAL** (6 quadradas nas razões
do hat da 808; BODY espalha as razões), **RUÍDO** (BODY = de 1 a 4 tapas, o clap). TONE é o corte do filtro do ruído
(passa-alta nos hats e no shaker, passa-banda nos outros); SNAP é o estalo do ataque e a velocidade do pitch env.
A voz (`Source/Drum.h`) é C++ puro, para dar para medir o nível de cada som fora do plugin.

## Grade

Clique liga/desliga; arraste para cima/baixo numa batida para mudar a força; botão direito (ou Ctrl+clique) passa por
forte → ghost → roll ×2 → roll ×3 → talvez (50%). Tamanho de 1, 2 ou 4 compassos, RATE 1/16, 1/16T ou 1/8, compasso
4/4 ou 3/4 (no máximo 96 passos). Clique no nome do instrumento para ouvir e abrir o som dele no editor.

## Gerador

Cada ESTILO tem um peso (0 a 9) por semicolcheia para cada instrumento (`Source/Generator.h`). DENSIDADE abre o que tem
peso médio, GHOST põe batidas fracas nos vãos, ROLL põe 2 ou 3 batidas dentro do passo, SWING atrasa os passos ímpares
e HUMANO mexe na força e no tempo de cada batida. O compasso 1 é sorteado e os seguintes repetem com algumas trocas.
Os dados de RITMO, TIMBRES, FILLS e VELOCITY sorteiam só aquela parte; o cadeado faz o GERAR GROOVE pular a parte.

## Variação de timbre

Cada instrumento tem um timbre B sorteado (altura, decay, tone, snap, body e pitch env); o dado TIMBRES sorteia outro.
MUDA A CADA ½ a 8 compassos; **A ↔ B** alterna todos, **CONVERSA** deixa metade dos escolhidos em B enquanto a outra
metade fica em A e depois trocam, **DERIVA** vai mudando aos poucos. QUANTO = o tamanho da diferença. A faixa embaixo
da grade mostra quem está em B em cada trecho. FILL A CADA 4 ou 8 compassos troca a 2ª metade do último compasso do
ciclo por um fill (instrumento sem batida no fill segue o groove).

## Presets

72 de fábrica (12 por estilo): cada um é estilo + semente; o kit, os ajustes e o groove saem da semente, sempre
iguais. Presets do usuário guardam tudo (valores + grade). Travas, mudo/solo e o modo de tocar não mudam com o preset.
