cmake_minimum_required(VERSION 3.22)

# MAPA: app STANDALONE (não é plugin). Lê uma música de referência, mostra a prévia do projeto com cara de DAW
# e monta o projeto na DAW do usuário (Ableton .als, Reaper .rpp, DAWproject, pasta MIDI). Build próprio:
# workflow build-mapa.yml (só roda quando apps/mapa/ ou novidades/Mapa.md mudam).

# Mac: binário universal (Intel + Apple Silicon) a partir do macOS 13.3
set(CMAKE_OSX_DEPLOYMENT_TARGET "13.3" CACHE STRING "macOS mínimo (o ONNX Runtime 1.20 pede 13.3)")
# Windows: runtime estático, o app não depende do VC++ Redistributable
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

project(MAPA VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
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

# ---------------------------------------------------------------------------------------------- IA (MAPA 0.2)
# Demucs (separação em stems) e Basic Pitch (notas) rodam no ONNX Runtime 1.20.1 (MIT), pacote oficial do NuGet
# (o repositório microsoft/onnxruntime saiu do GitHub). Eigen 3.4 (MPL2, só cabeçalhos) para o STFT do Demucs.
FetchContent_Declare(ort
    URL https://www.nuget.org/api/v2/package/Microsoft.ML.OnnxRuntime/1.20.1
    DOWNLOAD_NAME ort-1.20.1.zip
    URL_HASH SHA256=9c32108e9a1c78b090a95c2a219f244dd5f067d4caf68fb0f967103f3f20839c
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_Declare(eigen
    URL https://gitlab.com/libeigen/eigen/-/archive/3.4.0/eigen-3.4.0.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SOURCE_SUBDIR nao_compilar)   # só os cabeçalhos: não entra no CMake do Eigen
FetchContent_MakeAvailable(ort eigen)

set(_ort_inc "${ort_SOURCE_DIR}/build/native/include")
if(APPLE)
    # dylib universal (arm64 + x86_64) com install name @rpath, montada no configure
    set(MAPA_ORT_LIB "${CMAKE_BINARY_DIR}/ort/libonnxruntime.dylib")
    if(NOT EXISTS "${MAPA_ORT_LIB}")
        file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/ort")
        execute_process(COMMAND lipo -create
                            "${ort_SOURCE_DIR}/runtimes/osx-arm64/native/libonnxruntime.dylib"
                            "${ort_SOURCE_DIR}/runtimes/osx-x64/native/libonnxruntime.dylib"
                            -output "${MAPA_ORT_LIB}" COMMAND_ERROR_IS_FATAL ANY)
        execute_process(COMMAND install_name_tool -id "@rpath/libonnxruntime.dylib" "${MAPA_ORT_LIB}" COMMAND_ERROR_IS_FATAL ANY)
    endif()
elseif(WIN32)
    set(MAPA_ORT_LIB "${ort_SOURCE_DIR}/runtimes/win-x64/native/onnxruntime.lib")
    set(MAPA_ORT_DLL "${ort_SOURCE_DIR}/runtimes/win-x64/native/onnxruntime.dll")
else()
    set(MAPA_ORT_LIB "${ort_SOURCE_DIR}/runtimes/linux-x64/native/libonnxruntime.so")
endif()

# htdemucs.onnx (174 MB) não fica no git: vem do release "mapa-modelos" (workflow mapa-modelos.yml) para modelos/
set(MAPA_MODELO_DEMUCS "${CMAKE_CURRENT_SOURCE_DIR}/modelos/htdemucs.onnx" CACHE FILEPATH "htdemucs.onnx")
if(NOT EXISTS "${MAPA_MODELO_DEMUCS}")
    message(WARNING "MAPA: ${MAPA_MODELO_DEMUCS} não existe - o app sai SEM a IA (análise simples). "
                    "Baixe o asset htdemucs.onnx do release mapa-modelos para apps/mapa/modelos/.")
endif()

# motor de IA numa biblioteca à parte (sem as flags de aviso do JUCE no código de terceiros)
add_library(mapa_ia STATIC
    Source/IA.cpp
    terceiros/demucs_onnx/dsp.cpp
    terceiros/demucs_onnx/model_apply.cpp
    terceiros/demucs_onnx/model_inference.cpp)
target_include_directories(mapa_ia PUBLIC Source PRIVATE terceiros/demucs_onnx "${eigen_SOURCE_DIR}" "${_ort_inc}")
target_link_libraries(mapa_ia PUBLIC "${MAPA_ORT_LIB}")
target_compile_definitions(mapa_ia PRIVATE EIGEN_MPL2_ONLY _USE_MATH_DEFINES NOMINMAX)
target_compile_options(mapa_ia PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8 /bigobj /O2>)
set_target_properties(mapa_ia PROPERTIES POSITION_INDEPENDENT_CODE ON)

# teste do motor sem JUCE (CI: roda numa música sintética e mede o tempo)
option(MAPA_TESTES "compila tools/teste_v2 (motor + análise, sem JUCE)" OFF)
if(MAPA_TESTES)
    add_executable(mapa_teste tools/teste_v2.cpp Source/Analise.cpp)
    target_link_libraries(mapa_teste PRIVATE mapa_ia)
    target_compile_options(mapa_teste PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)
    if(APPLE)
        set_target_properties(mapa_teste PROPERTIES BUILD_RPATH "${CMAKE_BINARY_DIR}/ort")
    elseif(WIN32)
        add_custom_command(TARGET mapa_teste POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${MAPA_ORT_DLL}" "$<TARGET_FILE_DIR:mapa_teste>")
    endif()
endif()

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

# Basic Pitch (nmp.onnx, 230 KB) vai embutido no binário via Recursos/ (MapaDados::nmp_onnx)

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
        mapa_ia
        juce::juce_gui_extra
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# IA no pacote: Mac = MAPA.app/Contents/Frameworks/libonnxruntime.dylib + Contents/Resources/htdemucs.onnx;
# Windows = onnxruntime.dll ao lado do MAPA.exe + modelos/htdemucs.onnx (a DLL do app vem antes da do System32).
if(APPLE)
    set_target_properties(Mapa PROPERTIES INSTALL_RPATH "@executable_path/../Frameworks" BUILD_WITH_INSTALL_RPATH TRUE)
    add_custom_command(TARGET Mapa POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_BUNDLE_CONTENT_DIR:Mapa>/Frameworks"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${MAPA_ORT_LIB}" "$<TARGET_BUNDLE_CONTENT_DIR:Mapa>/Frameworks/libonnxruntime.dylib")
    if(EXISTS "${MAPA_MODELO_DEMUCS}")
        add_custom_command(TARGET Mapa POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_BUNDLE_CONTENT_DIR:Mapa>/Resources"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${MAPA_MODELO_DEMUCS}" "$<TARGET_BUNDLE_CONTENT_DIR:Mapa>/Resources/htdemucs.onnx")
    endif()
elseif(WIN32)
    add_custom_command(TARGET Mapa POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${MAPA_ORT_DLL}" "$<TARGET_FILE_DIR:Mapa>")
    if(EXISTS "${MAPA_MODELO_DEMUCS}")
        add_custom_command(TARGET Mapa POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Mapa>/modelos"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${MAPA_MODELO_DEMUCS}" "$<TARGET_FILE_DIR:Mapa>/modelos/htdemucs.onnx")
    endif()
endif()
