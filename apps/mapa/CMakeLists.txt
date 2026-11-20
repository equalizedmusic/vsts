cmake_minimum_required(VERSION 3.22)

# MAPA: app STANDALONE (não é plugin). Lê uma música de referência, mostra a prévia do projeto com cara de DAW
# e monta o projeto na DAW do usuário (Ableton .als, Reaper .rpp, DAWproject, pasta MIDI). Build próprio:
# workflow build-mapa.yml (só roda quando apps/mapa/ ou novidades/Mapa.md mudam).

# Mac: binário universal (Intel + Apple Silicon) a partir do macOS 11
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "macOS mínimo")
# Windows: runtime estático, o app não depende do VC++ Redistributable
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

project(MAPA VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        master
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(JUCE)

# versão = o primeiro "## x.y.z" de novidades/Mapa.md
set(_md "${CMAKE_CURRENT_SOURCE_DIR}/../../novidades/Mapa.md")
file(STRINGS "${_md}" _titulos REGEX "^## [0-9]+\\.[0-9]+\\.[0-9]+")
list(GET _titulos 0 _primeiro)
string(REGEX MATCH "[0-9]+\\.[0-9]+\\.[0-9]+" MAPA_VERSAO "${_primeiro}")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_md}")
message(STATUS "MAPA ${MAPA_VERSAO}")

# MAPA_TESTE=1: build de teste (as notas MIDI do PRO vão para a DAW sem trava)
set(MAPA_TESTE 1 CACHE STRING "1 = build de teste (PRO liberado)")

juce_add_gui_app(Mapa
    PRODUCT_NAME        "MAPA"
    COMPANY_NAME        "InsideStudioSP"
    BUNDLE_ID           com.insidestudiosp.mapa
    VERSION             ${MAPA_VERSAO}
    DOCUMENT_EXTENSIONS wav aif aiff mp3 flac
    MICROPHONE_PERMISSION_ENABLED FALSE)

juce_generate_juce_header(Mapa)

file(GLOB _recursos CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Recursos/*")
juce_add_binary_data(MapaDados NAMESPACE MapaDados HEADER_NAME BinaryData.h SOURCES ${_recursos})
set_target_properties(MapaDados PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_sources(Mapa PRIVATE
    Source/Main.cpp
    Source/Analise.cpp
    Source/Exportar.cpp
    Source/Daws.cpp
    Source/Tela.cpp
    Source/Tocador.cpp)

target_compile_definitions(Mapa PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_USE_MP3AUDIOFORMAT=1
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:Mapa,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:Mapa,JUCE_VERSION>"
    MAPA_VERSAO="${MAPA_VERSAO}"
    MAPA_TESTE=${MAPA_TESTE})

# os textos em PT-BR estão em UTF-8 no fonte
target_compile_options(Mapa PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)
target_compile_options(MapaDados PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)

target_link_libraries(Mapa
    PRIVATE
        MapaDados
        juce::juce_gui_extra
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
