# versão: o primeiro "## x.y.z" de novidades/Stompbox.md
vsts_versao(Stompbox.md STOMPBOX_VERSAO)

# STOMPBOX: pedalboard de guitarra (10 pedais em série + AMP/CAB + NOISE GATE). É efeito, mas fica em instrumentos/
# para ter build, instalador e release próprios, como o ALIGN. Versão de TESTE: ainda sem isp_licenca.
juce_add_plugin(Stompbox
    COMPANY_NAME                "InsideStudioSP"
    BUNDLE_ID                   com.insidestudiosp.stompbox
    PLUGIN_MANUFACTURER_CODE    Insp
    PLUGIN_CODE                 Stb1
    FORMATS                     ${VSTS_FORMATS}
    PLUGIN_NAME                 "STOMPBOX"
    PRODUCT_NAME                "STOMPBOX"
    VERSION                     ${STOMPBOX_VERSAO}
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    VST3_CATEGORIES             Fx Distortion
    AU_MAIN_TYPE                kAudioUnitType_Effect)

target_sources(Stompbox PRIVATE Source/PluginProcessor.cpp Source/PluginEditor.cpp Source/EditorPedal.cpp
                                Source/EditorPainel.cpp Source/EditorMouse.cpp)
target_compile_definitions(Stompbox PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(Stompbox
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags)
