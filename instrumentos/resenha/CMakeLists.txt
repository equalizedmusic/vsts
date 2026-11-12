# versão: o primeiro "## x.y.z" de novidades/Resenha.md
vsts_versao(Resenha.md RESENHA_VERSAO)

juce_add_plugin(Resenha
    COMPANY_NAME                "InsideStudioSP"
    BUNDLE_ID                   com.insidestudiosp.resenha
    PLUGIN_MANUFACTURER_CODE    Insp
    PLUGIN_CODE                 Rsn1
    FORMATS                     ${VSTS_FORMATS}
    PRODUCT_NAME                "RESENHA"
    VERSION                     ${RESENHA_VERSAO}
    IS_SYNTH                    TRUE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    VST3_CATEGORIES             Instrument Synth
    AU_MAIN_TYPE                kAudioUnitType_MusicDevice)

target_sources(Resenha PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

# peças de hardware renderizadas (instrumentos/_hardware/tools/pecas.py): knob, lâmpada, LED, tecla
file(GLOB resenha_img CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Source/img/*")
juce_add_binary_data(ResenhaImg HEADER_NAME PluginImg.h NAMESPACE PluginImg SOURCES ${resenha_img})

isp_licenca(Resenha resenha ATUALIZAR)

target_compile_definitions(Resenha PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    # HUB: os slots PLUGIN hospedam instrumentos VST3 (Windows e Mac) e AU (Mac)
    JUCE_PLUGINHOST_VST3=1
    JUCE_PLUGINHOST_AU=1)

target_link_libraries(Resenha
    PRIVATE
        ResenhaImg
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags)

# ResenhaScan: lê cada plugin da busca do HUB FORA do DAW (plugin que trava ou cai não leva o DAW junto).
# Vai dentro do bundle do RESENHA (VST3 e AU), em Contents/Resources — o instalador copia o bundle inteiro.
juce_add_console_app(ResenhaScan PRODUCT_NAME "ResenhaScan")
target_sources(ResenhaScan PRIVATE Scanner/ScanMain.cpp)
target_compile_definitions(ResenhaScan PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_PLUGINHOST_VST3=1
    JUCE_PLUGINHOST_AU=1)
target_link_libraries(ResenhaScan
    PRIVATE
        juce::juce_audio_processors
        juce::juce_gui_basics
    PUBLIC
        juce::juce_recommended_config_flags)
# Cabeçalhos do SDK VST3 que vem dentro do JUCE: o ResenhaScan lê a categoria (instrumento/efeito) na fábrica do plugin.
set(RESENHA_VST3_SDK "")
foreach(m juce_audio_processors_headless juce_audio_processors)
    if(NOT RESENHA_VST3_SDK AND EXISTS "${juce_SOURCE_DIR}/modules/${m}/format_types/VST3_SDK/pluginterfaces/base/ipluginbase.h")
        set(RESENHA_VST3_SDK "${juce_SOURCE_DIR}/modules/${m}/format_types/VST3_SDK")
    endif()
endforeach()
if(NOT RESENHA_VST3_SDK)
    message(FATAL_ERROR "ResenhaScan: SDK VST3 do JUCE não encontrado em ${juce_SOURCE_DIR}")
endif()
target_include_directories(ResenhaScan PRIVATE "${RESENHA_VST3_SDK}")
if(WIN32)
    target_link_libraries(ResenhaScan PRIVATE shell32)
endif()
if(APPLE)
    # O host de AU usa AUGenericView (CoreAudioKit); o JUCE só liga esse framework em alvo de plugin, não em console app.
    target_link_libraries(ResenhaScan PRIVATE "-framework CoreAudioKit")
endif()

foreach(fmt VST3 AU)
    if(TARGET Resenha_${fmt})
        add_dependencies(Resenha_${fmt} ResenhaScan)
        add_custom_command(TARGET Resenha_${fmt} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "$<GENEX_EVAL:$<TARGET_PROPERTY:Resenha_${fmt},JUCE_PLUGIN_ARTEFACT_FILE>>/Contents/Resources"
            COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:ResenhaScan>" "$<GENEX_EVAL:$<TARGET_PROPERTY:Resenha_${fmt},JUCE_PLUGIN_ARTEFACT_FILE>>/Contents/Resources/"
            VERBATIM)
    endif()
endforeach()
