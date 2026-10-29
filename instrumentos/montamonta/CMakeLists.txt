# versão: o primeiro "## x.y.z" de novidades/MontaMonta.md
vsts_versao(MontaMonta.md MONTAMONTA_VERSAO)

juce_add_plugin(MontaMonta
    COMPANY_NAME                "InsideStudioSP"
    BUNDLE_ID                   com.insidestudiosp.montamonta
    PLUGIN_MANUFACTURER_CODE    Insp
    PLUGIN_CODE                 Mnt1
    FORMATS                     ${VSTS_FORMATS}
    PRODUCT_NAME                "MONTA-MONTA"
    VERSION                     ${MONTAMONTA_VERSAO}
    IS_SYNTH                    TRUE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    VST3_CATEGORIES             Instrument Synth
    AU_MAIN_TYPE                kAudioUnitType_MusicDevice)

target_sources(MontaMonta PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Panels.cpp)

isp_licenca(MontaMonta montamonta)

target_compile_definitions(MontaMonta PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(MontaMonta
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags)
