# Licenças (ativação dos pacotes)

Um **token por e-mail e por pacote** (criativo, mix, knob, quimera, limao, mercurio), válido em **até 2 computadores**.
Ativar um plugin libera todos os plugins do mesmo pacote naquele computador.

## Como funciona

1. O plugin abre com a tela **ATIVAR PACOTE …** por cima da janela. Sem ativação, o efeito deixa o som passar direto
   (instrumento fica mudo).
2. A pessoa digita o e-mail da compra e o token (`ISP-XXXX-XXXX-XXXX-XXXX-XXXX`). O plugin manda para o servidor junto
   com um hash do id do computador.
3. O servidor confere, registra a máquina e devolve a licença **assinada** (Ed25519). O plugin confere a assinatura com
   a chave pública embutida e grava em `%APPDATA%\InsideStudioSP\Licencas\<pacote>.lic` (Mac:
   `~/Library/Application Support/InsideStudioSP/Licencas`).
4. A licença assinada vale 45 dias sem internet. Com mais de 3 dias, o plugin confirma de novo em segundo plano; se a
   licença foi cancelada no servidor, o plugin trava de novo.
5. Menu de presets → **Licença…** mostra a licença e tem **Desativar este computador** (libera a vaga).
6. **Aviso de versão nova:** com a licença valendo, ao carregar o plugin ele pergunta uma vez ao servidor
   (`GET /v1/versao?pacote=&versao=&sistema=`). O servidor olha o release `<pacote>-vX.Y.Z` mais alto do GitHub (lista
   guardada 30 min) e, se for maior que a versão do plugin, aparece uma faixa no pé da janela com **BAIXAR** (abre o
   instalador do sistema no navegador) e **Agora não** (some em todos os plugins do pacote até o DAW carregar de novo).
   Repo privado ou GitHub fora = nenhum aviso, nada quebra (com repo privado dá para pôr um token só leitura no
   segredo `GITHUB_TOKEN` do Worker). Para testar, `-DISP_LICENCA_VERSAO_TESTE=1.0.0` faz o plugin fingir ser 1.0.0
   (o workflow do piloto usa isso).

Isso segura o repasse casual de instalador + token; não segura quem quiser craquear o binário.

## Pastas

- `servidor/` — Cloudflare Worker + banco D1 (`isp-licencas.insidestudiosp.workers.dev`, conta pessoal
  insidestudiosp). Rotas em `src/index.js`, tabelas em `schema.sql`. Teste local: `npm install`, `npx wrangler d1
  execute isp-licencas --local --file schema.sql`, `npx wrangler dev --local` (segredos em `.dev.vars`, fora do git).
- `cliente/` — o que vai no plugin: `IspLicenca.h` (estado, arquivo, conversa com o servidor), `IspTelaAtivacao.h`
  (tela), `IspEd25519.h` (conferência da assinatura, trecho do TweetNaCl), `licenca.cmake` (`isp_licenca(<alvo>
  <pacote>)`).
- `admin/isp-licenca.mjs` — criar token, listar, revogar, liberar máquina, gerar token novo.

## Segredos (fora do repositório)

- Chave **privada** de assinatura e chave de administração: `segredos/licenca-ed25519.json` (ao lado do clone) e nos
  segredos do Worker (`SIGN_KEY`, `ADMIN_KEY`). Perder a privada = trocar a chave pública e soltar versão nova de todos
  os plugins.
- `segredos/licenca-admin.json` = `{ url, admin_key }` usado pelo `admin/isp-licenca.mjs`.

## Ligar num plugin

```cmake
isp_licenca(Torno mix)
```
```cpp
// processador
juce::SharedResourcePointer<isp::Licenca> licenca;
if (! licenca->liberada()) return;            // efeito: som direto  (instrumento: buffer.clear(); return;)
// editor (membro por último) + no fim do construtor
isp::TelaAtivacao ativacao;
ativacao.anexar (*this);
// menu: item "Licença…" → ativacao.mostrarInfo();
```

Onde já está: os 33 plugins. Criativo e Mix pelo `CMakeLists` de cada plugin (menu de presets → **Licença…**);
SÉRIE KNOB uma vez só em `plugins/knob/_comum` (`serie_knob_pedal()`; sem menu de presets: **botão direito fora dos
controles** → Licença…); instrumentos pelo `instrumentos/CMakeLists.txt` + `isp_licenca()` de cada um.

Fotos (workflow `shots`): a tela é compilada como programa comum, sem `isp_licenca()`. O `shots/CMakeLists.txt` passa
`ISP_LICENCA_FOTO=1`: licença liberada, sem tela de ativação, sem arquivo e sem internet. Nunca usar isso no plugin.

Teste antes do merge: workflow `licenca-piloto.yml` (nome `licenca-teste`) compila todos os efeitos e instrumentos na
branch `licenca` e entrega os instaladores `*-licenca-teste-*` como artifact, sem release.
