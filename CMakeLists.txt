cmake_minimum_required(VERSION 3.22)

# Mac: binário universal (Intel + Apple Silicon) a partir do macOS 11
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "macOS mínimo")
# Windows: runtime estático, o plugin não depende do VC++ Redistributable
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

project(VSTS VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        master
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(JUCE)

# Formatos gerados por todos os plugins. AU só existe no Mac (o JUCE ignora no Windows).
set(VSTS_FORMATS VST3 AU)

# VST2: a Steinberg não distribui mais o SDK. Se os headers estiverem em sdk/vst2, gera também.
set(VST2_SDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/sdk/vst2")
if(EXISTS "${VST2_SDK_DIR}/pluginterfaces/vst2.x/aeffect.h")
    juce_set_vst2_sdk_path("${VST2_SDK_DIR}")
    list(APPEND VSTS_FORMATS VST)
    message(STATUS "VST2 SDK encontrado: gerando VST2")
else()
    message(STATUS "VST2 SDK ausente (sdk/vst2): gerando só VST3/AU")
endif()

# versão de cada linha = o primeiro "## x.y.z" de novidades/<Linha>.md (o mesmo que o instalador e o release
# usam; ver instalador/versao.py). Cada plugin usa ${VSTS_VERSAO}, a versão da linha dele.
function(vsts_versao arquivo saida)
    set(md "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/novidades/${arquivo}")
    file(STRINGS "${md}" titulos REGEX "^## [0-9]+\\.[0-9]+\\.[0-9]+")
    if(NOT titulos)
        message(FATAL_ERROR "${md}: nenhum título '## x.y.z'")
    endif()
    list(GET titulos 0 primeiro)
    string(REGEX MATCH "[0-9]+\\.[0-9]+\\.[0-9]+" v "${primeiro}")
    set(${saida} "${v}" PARENT_SCOPE)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${md}")
endfunction()
vsts_versao(Criativo.md VSTS_VERSAO_criativo)
vsts_versao(Mix.md VSTS_VERSAO_mix)

# Cada pasta em plugins/<linha>/ com CMakeLists.txt é um plugin (linhas: criativo, mix)
file(GLOB plugin_dirs LIST_DIRECTORIES true "${CMAKE_CURRENT_SOURCE_DIR}/plugins/*/*")
foreach(dir ${plugin_dirs})
    if(EXISTS "${dir}/CMakeLists.txt")
        get_filename_component(linha_dir "${dir}" DIRECTORY)
        get_filename_component(linha "${linha_dir}" NAME)
        set(VSTS_VERSAO "${VSTS_VERSAO_${linha}}")
        if(NOT VSTS_VERSAO)
            message(FATAL_ERROR "linha '${linha}' sem arquivo em novidades/")
        endif()
        add_subdirectory("${dir}")
    endif()
endforeach()
