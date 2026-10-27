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
