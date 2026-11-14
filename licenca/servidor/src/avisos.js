// E-mail automático ao adicionar pacote pelo painel: o painel cria uma licença por pacote marcado
// (POST /admin/licencas com avisar: true) e cada licença criada manda UM e-mail, com o passo a passo daquele plugin:
//   - VASSOURA e RODO (Max for Live, com ativação desde 14/11/2026): o e-mail padrão com token; o textoLicenca troca
//     instalador/ativação do VST pelo zip, a pasta do Ableton e o botão OPEN > ACTIVATE;
//   - os demais (VST3/AU com ativação): download dos sistemas marcados, instalação, ativação e avisos de segurança.
// O token só aparece no e-mail da licença que gerou o token novo (o primeiro pacote de um e-mail novo); os demais
// dizem "o mesmo token que você já recebeu". Sai pelo contato@ e fica copiado na Caixa de entrada do painel,
// com o token trocado por •••• (src/email.js).

import { enviarEmail, emailLigado } from './email.js';
import { textoLicenca, nomePacote } from './loja.js';

const DOMINIO = 'https://insidestudiosp.com.br';   // links sempre no domínio (o painel/CLI pode estar no workers.dev)

// lic = retorno do criarLicenca ({ id, email, pacote, token, token_trocado }); devolve o id do Resend ou false
export async function avisarLicenca(env, url, h, lic, { sistemas, maquinas } = {}) {
    if (!emailLigado(env)) return false;
    const sis = (Array.isArray(sistemas) ? sistemas : []).filter((s) => s === 'win' || s === 'mac');
    const nome = nomePacote(lic.pacote);
    // licença de TESTE (lic.teste_dias): texto e assunto próprios, 1 computador
    const teste = lic.teste ? Number(lic.teste_dias) || 7 : 0;
    const texto = textoLicenca({ base: DOMINIO, pacote: lic.pacote, email: lic.email, token: lic.token, trocado: !!lic.token_trocado,
                                 maquinas: teste ? 1 : Number(maquinas) || 2, sistemas: sis.length ? sis : ['win', 'mac'], teste });
    return enviarEmail(env, { para: lic.email, assunto: teste ? `Seu teste de ${teste} dias: ${nome} · Your ${teste}-day trial — InsideStudioSP`
                                                           : `Acesso liberado: ${nome} · Access granted — InsideStudioSP`,
                              texto: texto.join('\r\n'), segredos: [lic.token], origem: 'painel' });
}
