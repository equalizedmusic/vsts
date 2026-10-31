# Ativação por licença (e-mail + token por pacote). No CMakeLists do plugin:
#     isp_licenca(<alvo> <pacote>)      pacote: criativo, mix, knob, quimera, limao, mercurio, resenha, montamonta, groovador, mesa, align
# Servidor: licenca/servidor (Cloudflare Workers). A chave abaixo é a PÚBLICA (só confere); a privada fica no servidor.

set(ISP_LICENCA_URL   "https://isp-licencas.insidestudiosp.workers.dev" CACHE STRING "servidor de licenças")
set(ISP_LICENCA_CHAVE "e318ad516db6cd3f9d6963fe4952aad30c17f33f1981c4005c283a0cfdef8978")
set(ISP_LICENCA_DIR   "${CMAKE_CURRENT_LIST_DIR}")
# só para testar o aviso de versão nova: o plugin diz ao servidor que é esta versão (vazio = a versão de verdade)
set(ISP_LICENCA_VERSAO_TESTE "" CACHE STRING "versão fingida no aviso de versão nova (teste)")

function(isp_licenca alvo pacote)
    if(NOT pacote MATCHES "^(criativo|mix|knob|quimera|limao|mercurio|resenha|montamonta|groovador|mesa|align)$")
        message(FATAL_ERROR "isp_licenca: pacote '${pacote}' desconhecido")
    endif()
    target_include_directories(${alvo} PRIVATE "${ISP_LICENCA_DIR}")
    target_compile_definitions(${alvo} PRIVATE
        ISP_PACOTE="${pacote}"
        ISP_LICENCA_URL="${ISP_LICENCA_URL}"
        ISP_LICENCA_CHAVE="${ISP_LICENCA_CHAVE}")
    if(ISP_LICENCA_VERSAO_TESTE)
        target_compile_definitions(${alvo} PRIVATE ISP_VERSAO_ATUAL="${ISP_LICENCA_VERSAO_TESTE}")
    endif()
endfunction()
