# versão: o primeiro "## x.y.z" de novidades/Align.md
vsts_versao(Align.md ALIGN_VERSAO)

# ALIGN: fase kick/baixo. É efeito (vai no canal do baixo, kick pelo sidechain), mas fica em instrumentos/ para ter
# build, instalador e licença próprios, como a MESA.
juce_add_plugin(Align
    COMPANY_NAME                "InsideStudioSP"
    BUNDLE_ID                   com.insidestudiosp.align
    PLUGIN_MANUFACTURER_CODE    Insp
    PLUGIN_CODE                 Aln1
    FORMATS                     ${VSTS_FORMATS}
    PLUGIN_NAME                 "ALIGN"
    PRODUCT_NAME                "ALIGN"
    VERSION                     ${ALIGN_VERSAO}
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    VST3_CATEGORIES             Fx Tools
    AU_MAIN_TYPE                kAudioUnitType_Effect)

target_sources(Align PRIVATE Source/PluginProcessor.cpp Source/PluginEditor.cpp)
isp_licenca(Align align)
target_compile_definitions(Align PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(Align
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags)
