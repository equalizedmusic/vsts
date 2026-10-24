cmake_minimum_required(VERSION 3.22)

# Instrumentos (VSTi) da InsideStudioSP. Projeto separado dos efeitos de plugins/: tem o próprio build
# (workflow build-instrumentos), que só roda quando instrumentos/ muda.

# Mac: binário universal (Intel + Apple Silicon) a partir do macOS 11
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "macOS mínimo")
# Windows: runtime estático, o plugin não depende do VC++ Redistributable
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

project(VSTS_INSTRUMENTOS VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        master
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(JUCE)

set(VSTS_FORMATS VST3 AU)

# versão de cada instrumento = o primeiro "## x.y.z" de novidades/<Arquivo>.md (o mesmo que o instalador e o
# release usam; ver instalador/versao.py). Ex.: vsts_versao(Limao.md LIMAO_VERSAO)
function(vsts_versao arquivo saida)
    set(md "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../novidades/${arquivo}")
    file(STRINGS "${md}" titulos REGEX "^## [0-9]+\\.[0-9]+\\.[0-9]+")
    if(NOT titulos)
        message(FATAL_ERROR "${md}: nenhum título '## x.y.z'")
    endif()
    list(GET titulos 0 primeiro)
    string(REGEX MATCH "[0-9]+\\.[0-9]+\\.[0-9]+" v "${primeiro}")
    set(${saida} "${v}" PARENT_SCOPE)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${md}")
endfunction()

# cada pasta em instrumentos/ com CMakeLists.txt é um instrumento
file(GLOB inst_dirs LIST_DIRECTORIES true "${CMAKE_CURRENT_SOURCE_DIR}/*")
foreach(dir ${inst_dirs})
    if(IS_DIRECTORY "${dir}" AND EXISTS "${dir}/CMakeLists.txt")
        add_subdirectory("${dir}")
    endif()
endforeach()
