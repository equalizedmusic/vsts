# versão: o primeiro "## x.y.z" de novidades/Limao.md
vsts_versao(Limao.md LIMAO_VERSAO)

juce_add_plugin(Limao
    COMPANY_NAME                "InsideStudioSP"
    BUNDLE_ID                   com.insidestudiosp.limao
    PLUGIN_MANUFACTURER_CODE    Insp
    PLUGIN_CODE                 Lmo1
    FORMATS                     ${VSTS_FORMATS}
    PRODUCT_NAME                "LIMAO"
    VERSION                     ${LIMAO_VERSAO}
    IS_SYNTH                    TRUE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    VST3_CATEGORIES             Instrument Synth
    AU_MAIN_TYPE                kAudioUnitType_MusicDevice)

target_sources(Limao PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

# peças de hardware renderizadas (instrumentos/_hardware/tools/pecas.py): knob, lâmpada, LED, tecla, madeira
file(GLOB limao_img CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Source/img/*")
juce_add_binary_data(LimaoImg HEADER_NAME PluginImg.h NAMESPACE PluginImg SOURCES ${limao_img})

target_compile_definitions(Limao PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(Limao
    PRIVATE
        LimaoImg
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags)
