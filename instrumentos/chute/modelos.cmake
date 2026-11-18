# Modelos do CHUTE: Source/Modelos.h.cifrado (no git) -> <build>/chute_modelos/Modelos.h, aberto na configuração do
# CMake com a chave do ambiente CHUTE_MODELOS_KEY (segredo do repositório no GitHub). O Modelos.h em claro nunca vai
# para o git (.gitignore); na máquina de quem desenvolve ele pode existir em Source/ e tem prioridade (#include "").
# Uso: include(.../chute/modelos.cmake) e chute_modelos(<alvo>)
function(chute_modelos alvo)
    set(src "${CMAKE_CURRENT_FUNCTION_LIST_DIR}")
    set(dst "${CMAKE_CURRENT_BINARY_DIR}/chute_modelos")
    if(NOT EXISTS "${src}/Source/Modelos.h")
        find_package(Python3 COMPONENTS Interpreter REQUIRED)
        if("$ENV{CHUTE_MODELOS_KEY}" STREQUAL "")
            message(FATAL_ERROR "CHUTE: falta a chave CHUTE_MODELOS_KEY para abrir Source/Modelos.h.cifrado")
        endif()
        execute_process(
            COMMAND "${Python3_EXECUTABLE}" "${src}/tools/modelos_cifra.py" abre "${src}/Source/Modelos.h.cifrado" "${dst}/Modelos.h"
            RESULT_VARIABLE r)
        if(NOT r EQUAL 0)
            message(FATAL_ERROR "CHUTE: não deu para abrir os modelos (${r})")
        endif()
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${src}/Source/Modelos.h.cifrado")
    endif()
    target_include_directories(${alvo} PRIVATE "${dst}")
endfunction()
