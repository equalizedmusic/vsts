# Testes dos devices M4L (RODO / VASSOURA)

Simuladores em Node: um Max/Live falso roda o `.js` **gerado pelo build** e um servidor de licenças falso assina
com a chave real. Tudo que o device grava cai numa pasta temporária (`os.tmpdir()`), nada no disco de verdade.

Pré-requisitos:
- Node 18+.
- `ISP_ED25519` = caminho do `licenca-ed25519.json` (Bitwarden › pasta **Estúdio** › chave de assinatura das licenças).

Gerar os devices e testar:

```
node max4live/rodo/build-rodo.js          %TEMP%/m4l/rodo
node max4live/vassoura/build-vassoura.js  %TEMP%/m4l/vassoura
cd max4live/_testes
node t-pastas3.js %TEMP%/m4l/rodo/rodo.js         # licença: File/Dict/nada gravando -> ativa 1x, reabre sem token
node t-persist.js %TEMP%/m4l/rodo/rodo.js         # recarga offline, adulterada, outro PC, renovação, desativar
node t-antigo.js  <rodo.js 0.3.1> %TEMP%/m4l/rodo/rodo.js   # DEACTIVATE não volta pelo .lic antigo
set MOTOR=%TEMP%/m4l/rodo/rodo.js & node sim-rodo.js & node sim-rodo-sensor.js
set MOTOR=%TEMP%/m4l/vassoura/vassoura.js & node sim-vassoura.js MUTE & node sim-vassoura.js CUT
```

Os mesmos `t-*.js` servem para a VASSOURA (passe o `vassoura.js`). Cada um termina com `RESULTADO: OK` ou a lista de erros.
`t-lic.js` confere a chave pública embutida em `_comum/isp_licenca.js` contra a do `ISP_ED25519`.
`shot-all.js` desenha a faixa e a janela (precisa de `puppeteer`; `M4L_BUILD` = pasta com `Rodo-x.y.z/` e `Vassoura-x.y.z/`).
