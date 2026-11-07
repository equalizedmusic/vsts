# versão: o primeiro "## x.y.z" de novidades/Barebass.md
vsts_versao(Barebass.md BAREBASS_VERSAO)

# BAREBASS: synth de baixo simples (uma voz). Build, instalador e release próprios, como o ALIGN e o ONKEY.
# Licença: pacote barebass (grátis, como o ALIGN: página /barebass no servidor de licenças).
juce_add_plugin(Barebass
    COMPANY_NAME                "InsideStudioSP"
    BUNDLE_ID                   com.insidestudiosp.barebass
    PLUGIN_MANUFACTURER_CODE    Insp
    PLUGIN_CODE                 Brb1
    FORMATS                     ${VSTS_FORMATS}
    PLUGIN_NAME                 "BAREBASS"
    PRODUCT_NAME                "BAREBASS"
    VERSION                     ${BAREBASS_VERSAO}
    IS_SYNTH                    TRUE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    VST3_CATEGORIES             Instrument Synth
    AU_MAIN_TYPE                kAudioUnitType_MusicDevice)

target_sources(Barebass PRIVATE Source/PluginProcessor.cpp Source/PluginEditor.cpp)
isp_licenca(Barebass barebass)
target_compile_definitions(Barebass PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(Barebass
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags)
