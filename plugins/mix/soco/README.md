# SOCO — modelador de transiente

> Parte da **SÉRIE MIX** · versão atual do pacote: **1.2.1** (08/11/2026) · à venda com licença no site [insidestudiosp.com.br](https://insidestudiosp.com.br) · notas em [`novidades/Mix.md`](../../../novidades/Mix.md)

![SOCO](https://insidestudiosp.com.br/img/soco.webp)

Mexe no **attack** e no **sustain** de cada golpe sem depender do volume: o detector compara um
envelope rápido com um lento (a diferença é o golpe) e dois envelopes com solturas diferentes (a
diferença é a cauda). Serve para bumbo, caixa, violão, baixo, loop… No alto, a forma de onda dos
últimos ~3 s mostra o ANTES (fantasma) e o DEPOIS sobrepostos, com o que o SOCO pôs a mais ou
tirou pintado na cor da parte: coral = attack, azul = sustain.

## Controles

| Bloco | Controle | Faixa | O que faz |
|---|---|---|---|
| WAVEFORM | visual | ~3 s | Pico de cada fatia de 6 ms: cinza = antes (com o traço fantasma), creme = depois (com o DRY/WET, antes do GAIN). O que subiu ou desceu fica coral (attack) ou azul (sustain). Embaixo, o GAIN APLICADO de cada parte (±15 dB) |
| GOLPE | visual | — | Um golpe de teste (estalo + corpo) passando pelo detector com os ajustes de agora, em dB × tempo (escala de raiz). A faixa creme no começo é a janela do FOCO |
| 01 SOCO | **Macro ATTACK** | −100% a +100% | Mais = o começo do golpe salta (até +15 dB); menos = o golpe amacia (até −15 dB) |
| | Sustain | −100% a +100% | Mais = a cauda cresce (sala, corpo; até +12 dB); menos = a cauda some (seca) |
| | Foco | 2–50 ms | Tamanho do ataque que conta como golpe: curto = só o clique; longo = o corpo do golpe |
| | visual | — | Ganho de attack e de sustain aplicado agora |
| 02 DETECTOR | visual | — | O que o detector ouve (resposta do filtro) |
| | Detector | Full · Low · High | Filtro só do detector (o som não é filtrado): Low = passa-baixa, High = passa-alta, 12 dB/oitava |
| | Cutoff | 40 Hz–8 kHz | Frequência do filtro do detector (em Full, não faz nada) |
| | visual | — | INPUT PEAK, OUTPUT PEAK e MAX OUTPUT (segura 3 s), em dBFS |
| 03 OUTPUT | DELTA | liga/desliga | Escuta só o que o SOCO acrescentou ou tirou (com o DRY/WET). Os presets não mexem nele |
| | Clipper de proteção | liga/desliga | Clipper suave no fim: passa igual até −2,3 dBFS e encosta em −0,3 dBFS |
| | Dry/Wet | 0–100% | Seco ↔ processado (paralelo) |
| | Gain | −36 a +12 dB | Volume final (antes do clipper) |

As cores dos knobs são as do desenho: ATTACK coral, SUSTAIN azul, FOCO creme (a janela do golpe de
teste), DETECTOR e CUTOFF violeta (a curva do detector).

**Latência:** zero (o detector não olha para a frente). O ganho é o mesmo nos dois canais, então a
imagem estéreo não mexe. IN/OUT em VU de ponteiro, lendo o pico.

## Presets

Vinte de fábrica. No menu de presets: *Salvar como novo*; com um preset do usuário aberto, também
*Salvar alterações*, *Renomear* e *Apagar* (vai para a lixeira). O **+** pede o nome antes de salvar.

- Mac: `~/Library/Audio/Presets/InsideStudioSP/SOCO/`
- Windows: `%APPDATA%\InsideStudioSP\SOCO\Presets\`

## Janela e licença

- Visual de rack de 19": orelhas parafusadas, frente pintada na cor do plugin, knobs no padrão Moog com a escala
  serigrafada e alavancas de metal nas chaves liga/desliga. IN/OUT em VU de ponteiro, que lê o pico: o fim da escala
  (+3) é 0 dBFS e o 0 VU, −3 dBFS.
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
| `Source/TransientEngine.h` | Detector (envelopes e filtro), ganho de attack/sustain, DRY/WET, DELTA, clipper, histórico da forma de onda |
| `Source/PluginProcessor.*` | Bypass, medidores, estado e presets |
| `Source/PluginEditor.*`, `Widgets.h`, `Theme.h` | Interface (frente de 1000×580 entre as orelhas do rack, janela de 1064×580 em 100%); `Theme.h`/`Widgets.h` são o padrão da marca |
| `Source/Hardware.h`, `Painel.h` | Peças renderizadas (knobs, lâmpadas) e o painel em volta (rack, alavancas, VU) |
