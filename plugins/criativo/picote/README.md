# PICOTE — gate rítmico de 16 passos

Fatia o som no grid do host. Cada um dos 16 passos tem o seu **volume**, **pan** e **filtro**,
desenhados com o mouse na grade do topo. EMBARALHAR sorteia um padrão novo para a faixa que está
na tela, e PRENDER grava o próximo passo inteiro e repete até soltar. É o "trance gate" clássico,
mas serve para qualquer estilo.

## Controles

| Bloco | Controle | Faixa | O que faz |
|---|---|---|---|
| PASSOS | grade | 16 passos | Abas VOLUME · PAN · FILTRO. Clique e arraste para desenhar; duplo clique liga/desliga o volume, centraliza o pan e abre o filtro. O passo tocando fica aceso; o fundo de cada coluna é o que passou por ele na última volta |
| FATIA | visual | — | Envelope de dois passos (ataque, largura, soltura, chão da profundidade) e a roda com o padrão inteiro; aceso = tocando, coral = preso |
| 01 GRADE | **Macro PROFUNDIDADE** | 0–100% | Quanto o som fecha entre um golpe e outro. Em 0%, só o pan e o filtro trabalham |
| | Divisão | 1/8 · 1/8T · 1/16 · 1/16T · 1/32 | Tamanho de cada passo |
| | Passos | 1–16 | Quantos passos o padrão usa antes de voltar |
| | Swing | 0–100% | Empurra o passo par (até 3:1) |
| | Deslocar | +0 a +15 | Gira o padrão sem redesenhar |
| 02 FORMA | **Macro LARGURA** | 5–100% | Quanto de cada passo fica aberto |
| | Ataque | 0,5–50 ms | Subida no começo do passo |
| | Soltura | 1–200 ms | Queda depois da largura |
| | Filtro | Passa-baixa / Passa-alta / Passa-faixa | Tipo do filtro de cada passo (100% = aberto no passa-baixa/alta) |
| | Resson. | 0–100% | Ressonância do filtro |
| 03 SAÍDA | EMBARALHAR | golpe | Sorteia a faixa que está na tela (o volume sempre bate no passo 1) |
| | PRENDER | liga/desliga | Grava o próximo passo e repete em laço, no grid, até soltar |
| | Mix / Ganho | | Seco ↔ picotado (padrão 100%) e volume final |

**Sincronia:** o padrão segue a régua do host. Com o host parado, o cabeçalho mostra *AGUARDANDO PLAY*
e o padrão fica no passo do cursor; no play ele entra no grid. Sem régua nenhuma (standalone), conta sozinho.

## Presets

Vinte de fábrica (o padrão dos 16 passos faz parte do preset). No menu de presets: *Salvar como novo*;
com um preset do usuário aberto, também *Salvar alterações*, *Renomear* e *Apagar* (vai para a lixeira).
O **+** pede o nome antes de salvar. Trocar de preset não mexe no PRENDER.

- Mac: `~/Library/Audio/Presets/InsideStudioSP/PICOTE/`
- Windows: `%APPDATA%\InsideStudioSP\PICOTE\Presets\`

## Código

| Arquivo | Conteúdo |
|---|---|
| `Source/Params.h` | Parâmetros (12 + 3 faixas × 16 passos), textos e presets de fábrica escritos como texto (`x.xx.x…`) |
| `Source/GateEngine.h` | Posição no grid com swing, envelope do passo, filtro TPT por passo, pan, PRENDER |
| `Source/PluginProcessor.*` | Relógio do host, mix, estado, presets e EMBARALHAR |
| `Source/PluginEditor.*`, `Widgets.h`, `Theme.h` | Interface (1000×580); `Theme.h`/`Widgets.h` são o padrão da marca |
