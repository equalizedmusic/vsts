// passos de licença antes do fluxo normal do simulador
module.exports = (motor, outs, confere, seguir) => {
  const ultimaLic = () => JSON.parse([...outs].reverse().find((o) => o[0] === 'lic')[1]);
  motor.analyze();
  confere(!motor.busy && !motor.licLiberada(), 'sem licença: ANALYZE não roda');
  motor.lic_email('Cliente@Teste.com '); motor.lic_token('isp-aaaa-bbbb-cccc-dddd-ffff'); motor.lic_ativar();
  setTimeout(() => {
    confere(!motor.licLiberada() && /não conferem/.test(ultimaLic().msg), 'token errado: recusa com a mensagem do servidor');
    motor.lic_token('ISP-AAAA-BBBB-CCCC-DDDD-EEEE'); motor.lic_ativar();
    setTimeout(() => {
      const st = ultimaLic();
      confere(motor.licLiberada() && st.ok === 1 && /Ativado/.test(st.msg), 'ativou: ' + st.msg + ' · válida até ' + new Date(st.ate * 1000).toISOString().slice(0, 10));
      const p = global.PEDIDOS[global.PEDIDOS.length - 1].c;
      confere(p.pacote === motor.LIC_PACOTE && /^[0-9a-f]{64}$/.test(p.maquina) && p.email === 'cliente@teste.com', 'pedido ao servidor: pacote ' + p.pacote + ', máquina 64 hex, e-mail normalizado · ' + p.nome_maquina + ' · ' + p.versao);
      seguir();
    }, 60);
  }, 60);
};
