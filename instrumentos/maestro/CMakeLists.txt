# versão: o primeiro "## x.y.z" de novidades/Maestro.md
vsts_versao(Maestro.md MAESTRO_VERSAO)

# MAESTRO — MIX CONDUCTOR: uma instância em cada faixa; as cópias do mesmo binário se enxergam (maestro::Registry,
# memória estática do binário no processo do DAW). É efeito, mas fica em instrumentos/ para ter build, instalador e
# release próprios, como o ALIGN e o ONKEY. Versão de TESTE: ainda sem isp_licenca (o servidor não conhece o pacote).
juce_add_plugin(Maestro
    COMPANY_NAME                "InsideStudioSP"
    BUNDLE_ID                   com.insidestudiosp.maestro
    PLUGIN_MANUFACTURER_CODE    Insp
    PLUGIN_CODE                 Mae1
    FORMATS                     ${VSTS_FORMATS}
    PLUGIN_NAME                 "MAESTRO"
    PRODUCT_NAME                "MAESTRO"
    VERSION                     ${MAESTRO_VERSAO}
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    VST3_CATEGORIES             Fx EQ Dynamics
    AU_MAIN_TYPE                kAudioUnitType_Effect)

target_sources(Maestro PRIVATE Source/PluginProcessor.cpp Source/PluginEditor.cpp Source/Janela.cpp)
target_compile_definitions(Maestro PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(Maestro
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags)
