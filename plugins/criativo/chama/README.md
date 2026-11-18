# CHAMA — build-up de um knob só

> Parte da **SÉRIE CRIATIVO** · versão atual do pacote: **1.2.1** (08/11/2026) · à venda com licença no site [insidestudiosp.com.br](https://insidestudiosp.com.br) · notas em [`novidades/Criativo.md`](../../../novidades/Criativo.md)

![CHAMA](https://insidestudiosp.com.br/img/chama.webp)

Um knob para a subida inteira: de 0 a 100%, a CHAMA vai tirando o grave com um passa-alta
ressonante, abre o estéreo, sobe um ruído filtrado junto, cresce um reverb e, nos últimos 40%,
entra um gate no tempo que acelera (1/8 → 1/16 → 1/32). Automatize a CHAMA no Ableton ao longo
dos compassos da subida e ligue o **DROP** na virada. Na tela, o fogo cresce na faixa de cima,
as lâmpadas em volta do knob acendem de 0 a 10 e o **FOGUINHO** dança no tempo.

## FOGUINHO

O espírito de fogo à direita do knob dança no tempo do projeto, contando 4/4 (os LEDs 1-2-3-4 na chapa,
embaixo dele, acendem o tempo atual; o 1 em coral). Quanto mais CHAMA, mais eufórico ele fica:
de boa, olho meio fechado (0%) → acompanha com a cabeça e sorri → braços para cima e pulos, com o
1 do compasso pesando mais → no talo (85%+) olhão de estrela, boca gritando e faíscas a cada tempo.
Com o DROP ligado ele agacha e prende o fôlego; com o host parado ele espera o play. O fogo também
dá um empurrão em cada tempo. Tudo é só visual: não mexe no som.

## Controles

| Bloco | Controle | Faixa | O que faz |
|---|---|---|---|
| TENSÃO | visual | — | Muro de fogo e o que está sendo aplicado agora: LOW CUT, RESONANCE, WIDTH, NOISE, ESPAÇO e GATE SYNC |
| CHAMA | **CHAMA** | 0–100% | Knob grandão, com as lâmpadas de 0 a 10 em volta. Tudo junto: passa-alta 20 Hz → 2 kHz (com pico), estéreo até 250%, ruído, reverb, gate no tempo e +4 dB de compensação |
| AJUSTES | DROP | liga/desliga | Apaga a chama em 20 ms (para a virada). Os presets não mexem nele |
| | NOISE | 0–100% | Quanto ruído sobe junto no máximo (faíscas azuis no desenho) |
| | ESPAÇO | 0–100% | Quanto reverb no máximo (fumaça violeta no desenho) |
| | GAIN | −36 a +12 dB | Volume final |

O gate segue o grid do Ableton (**SYNC**). Com o Ableton parado o gate espera o play
(**AGUARDANDO PLAY**); em host sem régua anda sozinho no BPM (**LIVRE**).

## Presets

Vinte de fábrica (Faísca, Brasa, Fogueira… Combustão). No menu de presets: *Salvar como novo*; com
um preset do usuário aberto, também *Salvar alterações*, *Renomear* e *Apagar* (vai para a lixeira).
O **+** pede o nome antes de salvar.

- Mac: `~/Library/Audio/Presets/InsideStudioSP/CHAMA/`
- Windows: `%APPDATA%\InsideStudioSP\CHAMA\Presets\`

## Janela e licença

- Visual de pedal: moldura de madeira, chapa esmaltada na cor do plugin, controles em chapa preta com os
  knobs da SÉRIE KNOB e alavancas de metal nas chaves liga/desliga. O knob CHAMA é o grandão do FOLE, com as lâmpadas
  de 0 a 10 em volta acendendo até a tensão de agora.
- A janela muda de tamanho pelo canto ou no menu de presets → *Tamanho da janela* (75% a 200%) e abre no
  tamanho salvo.
- Na primeira vez o plugin pede o e-mail e o token da licença do pacote; o mesmo token vale em até 2 computadores.
  Sem ativar, o som passa direto, sem processar. No menu de presets, *Licença…* mostra a licença e tem
  *Desativar este computador* (libera a vaga para outro).
- Com a licença ativa, uma faixa no pé da janela avisa quando sai versão nova do pacote (**BAIXAR** / **Agora não**).

## Código

| Arquivo | Conteúdo |
|---|---|
| `Source/Params.h` | Parâmetros, faixas, textos e presets de fábrica |
| `Source/FlameEngine.h` | Curvas da CHAMA, passa-alta e ruído (SVF), largura, reverb, gate no tempo |
| `Source/PluginProcessor.*` | Host/BPM e posição no compasso (para a animação), ganho, estado e presets |
| `Source/FireRender.h` | Fogo da tela: campo de ruído subindo, pintado em baixa resolução e ampliado; faíscas (NOISE) e fumaça (ESPAÇO) |
| `Source/Dancer.h` | FOGUINHO e o contador do compasso 4/4 |
| `Source/PluginEditor.*`, `Widgets.h`, `Theme.h` | Interface (1000×580 com a moldura de madeira); `Theme.h`/`Widgets.h` são o padrão da marca |
| `Source/Hardware.h`, `Painel.h` | Peças renderizadas (knobs, lâmpadas) e o painel em volta (moldura, chapa, alavancas) |
