# Licenças (ativação dos pacotes)

> **Desde 15/11/2026 o servidor de licenças, o painel admin e o site ficam no repositório PRIVADO `equalizedmusic/site`.**
> Aqui no `vsts` fica só `licenca/cliente` (o que vai dentro do plugin). As menções a `licenca/servidor` abaixo apontam para lá.

Um **token por e-mail e por pacote** (criativo, mix, knob, quimera, limao, mercurio, resenha, montamonta, groovador, mesa, align, barebass, stompbox, maestro, magos, gogo, vassoura, rodo, espanador), válido em **até 2 computadores**.
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
7. **Atualização automática** (plugins com `isp_licenca(<alvo> <pacote> ATUALIZAR)`: TODOS desde 25/11/2026; antes só LIMÃO e RESENHA): a
   resposta do `/v1/versao` traz `instalador: { dados, assinatura }` — `{ v, tipo:"instalador", pacote, sistema,
   versao, sha256, tamanho, url }` assinado com a MESMA chave Ed25519 das licenças (sha256 = `digest` do asset no
   GitHub; url = `/baixar/<pacote>/<sistema>?tag=<pacote>-vX.Y.Z`, a versão exata). O plugin confere a assinatura,
   mostra **ATUALIZAR**, baixa para a pasta temporária e confere o SHA-256 (`IspAtualizacao.h`). Windows: um script
   PowerShell espera o processo do DAW terminar e roda o Inno com `/VERYSILENT /NOCLOSEAPPLICATIONS` (UAC uma vez;
   log em `%TEMP%\InsideStudioSP-atualizacao\`). Mac: `osascript … with administrator privileges` monta o .dmg e
   roda o .pkg com `installer -target /`. Falhou = mensagem na faixa e o BAIXAR continua. `ISP_CANAL_TESTE=1` manda
   `teste=1` e o servidor passa a considerar os pré-lançamentos do pacote.
8. **Atualização automática dos devices Max for Live** (25/11/2026): o `[v8]`/`[js]` do Max não roda instalador, então
   cada device tem um `[node.script <dev>_atualiza.js]` (fonte `max4live/_comum/isp_atualiza.js`). O motor guarda o
   `instalador` do `/v1/versao` e, no clique em **UPDATE**, manda `atualizar <pacote> <versão> <dados> <assinatura>`;
   o Node confere a assinatura Ed25519 e os campos (pacote, sistema, versão maior), baixa, confere o SHA-256 e roda o
   instalador do device sem janela: Windows `InsideStudioSP-<Nome>-win.exe /VERYSILENT /NOCLOSEAPPLICATIONS` (sem
   admin; usa a pasta da instalação anterior), Mac `hdiutil attach` + `installer -pkg … -target CurrentUserHomeDirectory`
   (sem senha). Responde `atu_estado <baixando|conferindo|instalando|pronto|erro>`; o texto do botão vem do motor
   (`atu`). Erro ou Node mudo por 8 s = abre `/baixar/<device>/zip` (que entrega o instalador) no navegador.
   O device aberto continua o antigo até ser carregado de novo. Teste fora do Live: imitar `max-api` e usar
   `ISP_ATUALIZA_DIR=<pasta>` (instala lá em vez da User Library).

Isso segura o repasse casual de instalador + token; não segura quem quiser craquear o binário.

## Ativação automática entre produtos (24/11/2026)

O token é um só por e-mail e vale para todos os pacotes da pessoa. Pacote sem licença no computador procura o e-mail+token
gravado por OUTRO produto InsideStudioSP e tenta ativar sozinho, com `"auto": true` no POST /v1/ativar:

- VST/AU (`licenca/cliente/IspLicenca.h`, `tentarAutomatica`): lê os `*.lic` da pasta Licencas e as licenças dos devices
  M4L (`InsideStudioSP-*-licenca.*` em C:/Users/Public/Documents ou /Users/Shared e em AppData/Roaming ou Application Support).
- Max for Live (`max4live/_comum/isp_licenca.js`, `licAutoInicia`): depois do `recuperar` sem sucesso, lê as licenças dos
  outros devices nas mesmas pastas e os `.lic` dos VST em `<home>/AppData/Roaming/InsideStudioSP/Licencas/`.
- No máximo 3 candidatos (os mais recentes). Cada conjunto é tentado UMA vez: o hash fica em `<pacote>.auto` (VST) ou
  `InsideStudioSP-<pacote>-auto` (M4L). DESATIVAR grava o hash, então o produto não se reativa sozinho na próxima abertura.
- Servidor (`licenca/servidor/src/index.js`): com `auto: true`, e-mail+token CERTOS sem o pacote respondem
  `pacote_nao_incluido` SEM contar como recusa (evento `auto_sem_pacote`), para abrir o DAW com vários produtos não bloquear
  o IP. Token errado continua contando. A ativação que deu certo fica no evento `ativou` com "(automática)".
- Sem o pacote: a tela abre com o e-mail preenchido e o aviso "Seu e-mail já tem outros … mas não o <PACOTE>".
- Teste: `max4live/_testes/t-auto.js` (5 cenários). O cliente VST segue a mesma lógica (não há como rodar JUCE aqui).

## Ativação sem internet (26/11/2026) — VST3/AU

Para o PC de estúdio sem rede (cliente com Cubase sem placa de rede). Só VST/AU; Max for Live e licença de teste ficam fora.
- Plugin: tela de ativação → **SEM INTERNET?** mostra o código do computador (= `maquina()`) e **SALVAR CÓDIGO** grava
  `InsideStudioSP-codigo-<PC>.txt`. **CARREGAR LICENÇA** lê o `.isplic` (`{ tipo: "isp-offline", licencas: [{ pacote,
  dados, assinatura }] }`), confere assinatura + máquina de cada uma e grava o `.lic` de TODOS os pacotes do arquivo
  (token vazio). Cada plugin aberto percebe pelo timer; os outros ao abrir.
- Licença: `dados` ganha `offline: true` e `valida_ate` = 31/12/2099. Com `offline`, o plugin não reconfirma a cada 3
  dias e a tela de info mostra "Licença SEM INTERNET" com só **Fechar** (sem token não há DESATIVAR). Plugin antigo
  também aceita o `.lic` (só olha v/pacote/maquina/valida_ate), mas não tem a tela para carregar.
- Servidor (repo `site`): página `GET /offline` e `POST /offline/gerar { email, token, codigo, nome }` — licenças do
  e-mail com aquele token; cada pacote ocupa a vaga da máquina como no ATIVAR (nome "<PC> (sem internet)", evento
  `ativou`); sem vaga = fica de fora com motivo `limite`. Liberar a máquina no painel solta a vaga, mas o `.lic` no PC
  sem internet segue valendo (não há como revogar sem rede).

## Licença de teste (7 dias) — sem divulgar, só pelo painel

Painel → Novo cliente / Adicionar pacote → Origem **Teste de 7 dias**. Regras (servidor, `index.js` `criarLicenca` e `rotaPlugin`):
- só pacote **pago** (`precos.js` PRECOS); **1 computador**; origem `teste`.
- **token próprio**, diferente do token de compra do e-mail (todos os testes do e-mail dividem o mesmo token de teste).
  "Gerar token novo" e "perdi meu token" trocam só o lado da compra.
- os 7 dias contam da **1ª ativação** (evento `ativou` mais antigo da licença): desativar e ativar noutro PC não zera.
- **o plugin não mudou**: a licença assinada sai com `valida_ate` = fim do teste, então o plugin trava sozinho nessa
  hora, mesmo offline. Depois disso `/v1/ativar` e `/v1/validar` respondem `teste_expirado` (evento `teste_vencido`, não
  conta como recusa para o bloqueio por IP). Quem atrasar o relógio do PC engana a trava local; ao confirmar pela internet
  o servidor recusa.
- comprou o pacote (loja ou painel): a licença de teste é **apagada** e entra a normal, com o token de compra
  (evento `teste_substituido`). Teste não dá crédito de "completar o combo" nem bloqueia o combo.
- e-mail próprio ("Seu teste de 7 dias: …", `textoLicenca({ teste })`).

## Pastas

- Página grátis do MONTA-MONTA: `/gratis/montamonta` (e-mail → token na tela + download, origem `gratis`; ver `servidor/src/gratis.js`). Venda/cortesia num e-mail que só tem licença grátis troca o token.
- Página grátis do ALIGN: `/align` (= `/gratis/align`): e-mail + Windows/Mac → token na tela, link de download só
  do sistema escolhido e o passo a passo (baixo + sidechain do kick + ativar). Licença de **1 computador**, origem
  `gratis`, nota `página ALIGN · Mac|Windows` (aparece no painel). Mesmos limites de pedidos da página do MONTA-MONTA.
  O botão **BAIXAR ALIGN** entrega um **zip montado na hora** (`POST /gratis/align/zip`, `src/zip.js`): o instalador
  do sistema + `LEIA-ME - READ ME.txt` (PT e EN) com o e-mail, o token, instalação, sidechain, ativação, o que fazer
  com SmartScreen/Defender/Gatekeeper e o SHA-256 do instalador. O token só entra no texto se conferir com a licença.
  Zip sem compressão e em streaming: o CRC32 do instalador vem pronto (`<instalador>.crc32` no release, gerado pelo
  `release-versoes.sh`; os da 0.1.0 estão em `CRC_CONHECIDOS` no `index.js`), porque no plano grátis o Worker tem 10 ms
  de CPU e o CRC de 5–9 MB em JS leva 11–22 ms. Sem CRC, o botão cai no download direto do instalador. Link
  "Baixar só o instalador" na página como reserva.
- `servidor/` — Cloudflare Worker + banco D1 (`isp-licencas.insidestudiosp.workers.dev`, conta pessoal
  insidestudiosp). Rotas em `src/index.js`, tabelas em `schema.sql`. Teste local: `npm install`, `npx wrangler d1
  execute isp-licencas --local --file schema.sql`, `npx wrangler dev --local` (segredos em `.dev.vars`, fora do git).
- `cliente/` — o que vai no plugin: `IspLicenca.h` (estado, arquivo, conversa com o servidor), `IspTelaAtivacao.h`
  (tela), `IspEd25519.h` (conferência da assinatura, trecho do TweetNaCl), `licenca.cmake` (`isp_licenca(<alvo>
  <pacote>)`).
- `admin/isp-licenca.mjs` — criar token, listar, revogar, liberar máquina, gerar token novo, `senha-painel <usuário>`.
- **Painel web**: `https://insidestudiosp.com.br/painel` (o endereço antigo `…workers.dev/painel` e o `www`
  redirecionam para ele; rotas `/painel*` e `/admin/*` do domínio em `wrangler.toml`) (usuário e senha; código em
  `servidor/src/painel.js`). A página é só a lista de clientes (uma linha por e-mail, filtro na hora).
  **+ Novo cliente** abre um modal com e-mail + pacotes + sistema + origem; se o e-mail já existir, avisa, trava os
  pacotes que ele já tem e oferece abrir o cliente. Clicar num cliente abre o modal dele: cancela/reativa pacote,
  cancela todas, exclui o cliente (só com tudo cancelado; apaga licenças e computadores, o histórico fica), libera
  computador, gera token novo e **+ Adicionar pacote** (área no próprio modal; os pacotes que ele já tem aparecem
  travados, a mensagem sai só com os pacotes novos e diz que o token é o mesmo). A mensagem para o cliente (copiar
  ou abrir no e-mail; links só de Windows, só de Mac ou dos dois) abre num modal próprio. Troca a senha.

**Um token por e-mail**: a 1ª licença de um e-mail gera o token; as seguintes (outros pacotes) usam o mesmo, e "token
novo" troca o do e-mail inteiro. Cada pacote ativa em até 2 computadores. Mudança aplicada em 27/10/2026 com
`servidor/migracao-2026-10-27-token-por-email.sql` (banco novo: só o `schema.sql`).

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
