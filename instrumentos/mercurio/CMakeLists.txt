# versão: o primeiro "## x.y.z" de novidades/Mercurio.md
vsts_versao(Mercurio.md MERCURIO_VERSAO)

juce_add_plugin(Mercurio
    COMPANY_NAME                "InsideStudioSP"
    BUNDLE_ID                   com.insidestudiosp.mercurio
    PLUGIN_MANUFACTURER_CODE    Insp
    PLUGIN_CODE                 Mrc1
    FORMATS                     ${VSTS_FORMATS}
    PRODUCT_NAME                "MERCURIO"
    VERSION                     ${MERCURIO_VERSAO}
    IS_SYNTH                    TRUE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    VST3_CATEGORIES             Instrument Synth
    AU_MAIN_TYPE                kAudioUnitType_MusicDevice)

target_sources(Mercurio PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

# texturas renderizadas por tools/gerar-texturas.py (chapa, knob, tecla, parafuso)
file(GLOB mercurio_img CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Source/img/*")
juce_add_binary_data(MercurioImg HEADER_NAME PluginImg.h NAMESPACE PluginImg SOURCES ${mercurio_img})

target_compile_definitions(Mercurio PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(Mercurio
    PRIVATE
        MercurioImg
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags)
