# versão: o primeiro "## x.y.z" de novidades/Groovador.md
vsts_versao(Groovador.md GROOVADOR_VERSAO)

juce_add_plugin(Groovador
    COMPANY_NAME                "InsideStudioSP"
    BUNDLE_ID                   com.insidestudiosp.groovador
    PLUGIN_MANUFACTURER_CODE    Insp
    PLUGIN_CODE                 Grv1
    FORMATS                     ${VSTS_FORMATS}
    PRODUCT_NAME                "GROOVADOR"
    VERSION                     ${GROOVADOR_VERSAO}
    IS_SYNTH                    TRUE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    VST3_CATEGORIES             Instrument Drum
    AU_MAIN_TYPE                kAudioUnitType_MusicDevice)

target_sources(Groovador PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

isp_licenca(Groovador groovador)

target_compile_definitions(Groovador PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(Groovador
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags)
