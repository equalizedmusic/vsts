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

// 15/11: token novo pelo painel (por plugin). pacotes = onde o token novo vale; os outros plugins do e-mail não mudam.
export async function avisarNovoToken(env, { email, token, pacotes, teste }) {
    if (!emailLigado(env)) return false;
    const nomes = pacotes.map(nomePacote);
    const lista = nomes.join(', ');
    const m4l = pacotes.filter((p) => p === 'vassoura' || p === 'rodo'), vst = pacotes.filter((p) => p !== 'vassoura' && p !== 'rodo');
    const texto = [
        `Seu token novo · InsideStudioSP`, '', '(English below)', '',
        '==================================== PORTUGUÊS ====================================', '',
        `Geramos um token novo para: ${lista}.`, '',
        'SEUS DADOS DE ATIVAÇÃO',
        `  E-mail: ${email}`,
        `  Token:  ${token}`,
        `  Vale para: ${lista}${teste ? ' (licença de TESTE)' : ''}`, '',
        `  O token anterior destes plugins parou de valer. Se você tem outros plugins da InsideStudioSP, eles continuam com o token de antes.`, '',
        'COMO ATIVAR',
        ...(vst.length ? ['  Plugins VST/AU: se o plugin pedir ativação, digite o e-mail e o token acima e clique em ATIVAR.'] : []),
        ...(m4l.length ? [`  ${m4l.map(nomePacote).join(' e ')} (Max for Live): clique em OPEN THE MAGIC; se a janela pedir, digite o e-mail e o token acima e clique em ACTIVATE.`] : []),
        '  Guarde este e-mail.', '',
        `  Dúvidas? Responda este e-mail. Instagram: @equalizedmusic`, '', '',
        '===================================== ENGLISH =====================================', '',
        `We generated a new token for: ${lista}.`, '',
        'YOUR ACTIVATION DATA',
        `  E-mail: ${email}`,
        `  Token:  ${token}`,
        `  Works for: ${lista}${teste ? ' (TRIAL license)' : ''}`, '',
        '  The previous token of these plugins no longer works. Other InsideStudioSP plugins you own keep their current token.', '',
        'HOW TO ACTIVATE',
        ...(vst.length ? ['  VST/AU plugins: if the plugin asks for activation, type the e-mail and token above and click ATIVAR.'] : []),
        ...(m4l.length ? [`  ${m4l.map(nomePacote).join(' and ')} (Max for Live): click OPEN THE MAGIC; if the window asks, type the e-mail and token above and click ACTIVATE.`] : []),
        '  Keep this e-mail.', '',
        '  Questions? Just reply to this e-mail.',
    ];
    return enviarEmail(env, { para: email, assunto: `Seu token novo: ${lista} · Your new token — InsideStudioSP`,
                              texto: texto.join('\r\n'), segredos: [token], origem: 'painel' });
}

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
