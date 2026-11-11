# versão: o primeiro "## x.y.z" de novidades/Magos.md
vsts_versao(Magos.md MAGOS_VERSAO)

# MASTER DOS MAGOS — MASTERING WIZARD: masterizador "par do MAESTRO" (referência, TONE MATCH, DYNAMIC EQ, multibanda,
# limitador true peak). Vai no master. Lê os 8 grupos do MAESTRO pelo MAESTRO LINK (memória compartilhada do processo,
# Source/Link.cpp). É efeito, mas fica em instrumentos/ para ter build, instalador e release próprios, como o MAESTRO.
# Licença: pacote magos (pago, à venda no site; também no combo MAESTRO + MAGOS).
juce_add_plugin(Magos
    COMPANY_NAME                "InsideStudioSP"
    BUNDLE_ID                   com.insidestudiosp.masterdosmagos
    PLUGIN_MANUFACTURER_CODE    Insp
    PLUGIN_CODE                 Mgo1
    FORMATS                     ${VSTS_FORMATS}
    PLUGIN_NAME                 "MASTER DOS MAGOS"
    PRODUCT_NAME                "MASTER-DOS-MAGOS"
    VERSION                     ${MAGOS_VERSAO}
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    VST3_CATEGORIES             Fx Mastering Dynamics EQ
    AU_MAIN_TYPE                kAudioUnitType_Effect)

isp_licenca(Magos magos)

target_sources(Magos PRIVATE Source/PluginProcessor.cpp Source/PluginEditor.cpp Source/Link.cpp)
target_compile_definitions(Magos PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_USE_MP3AUDIOFORMAT=1
    JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(Magos
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_formats
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags)
