# Bancos de presets do CHUTE

- `fabrica/FABRICA.chutebank`: os 50 de fábrica (SUB, PUNCH, TIGHT, BOOM). Vai DENTRO do plugin (`../bancos.cmake`).
  Os números dos modelos (0 a 49) não podem mudar: projetos antigos guardam o número.
- `*.chutebank` aqui na raiz: bancos extras. Vão no instalador do CHUTE e no pacote só de presets
  (`InsideStudioSP-ChuteBancos`, workflow `release-chute-bancos`, notas em `novidades/ChuteBancos.md`).

Formato: `../Source/Bancos.h`. Ferramenta: `../tools/banco.py` (`gera` / `lista` / `abre`).
O arquivo é embaralhado (não abre num editor nem como áudio), mas NÃO é cifrado: quem tem o código lê.
No cliente: Windows `C:\ProgramData\InsideStudioSP\CHUTE\Bancos`, Mac `/Library/Application Support/InsideStudioSP/CHUTE/Bancos`
(e a mesma pasta no perfil do usuário). Banco com nome de preset repetido (banco + nome): fica o primeiro.