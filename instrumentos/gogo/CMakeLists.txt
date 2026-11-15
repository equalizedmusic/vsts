# versão: o primeiro "## x.y.z" de novidades/Gogo.md
vsts_versao(Gogo.md GOGO_VERSAO)

# GOGÓ: cadeia de voz automática (CLEAN, TUNE com o motor do ONKEY, RIDE, FET, OPTO, TONE, DE-ESS, WARMTH, SPACE).
# É efeito (vai no canal da voz), mas fica em instrumentos/ para ter build, instalador e release próprios, como o ONKEY.
# Lê o MAESTRO pelo MAESTRO LINK (Source/Link.cpp). Licença: pacote gogo (pago, à venda no site).
juce_add_plugin(Gogo
    COMPANY_NAME                "InsideStudioSP"
    BUNDLE_ID                   com.insidestudiosp.gogo
    PLUGIN_MANUFACTURER_CODE    Insp
    PLUGIN_CODE                 Ggo1
    FORMATS                     ${VSTS_FORMATS}
    PLUGIN_NAME                 "GOGO"
    PRODUCT_NAME                "GOGO"
    VERSION                     ${GOGO_VERSAO}
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    VST3_CATEGORIES             Fx Dynamics EQ "Pitch Shift" Reverb
    AU_MAIN_TYPE                kAudioUnitType_Effect)

isp_licenca(Gogo gogo)

target_sources(Gogo PRIVATE Source/PluginProcessor.cpp Source/PluginEditor.cpp Source/Link.cpp)
target_compile_definitions(Gogo PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(Gogo
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags)
