# versão: o primeiro "## x.y.z" de novidades/Retalho.md
vsts_versao(Retalho.md RETALHO_VERSAO)

# RETALHO (nome provisório): stutter + groove de vocal, latência 0. EDIT (fill desenhado), COMPLEX (passos com DICE) e
# GROOVE (a voz em 8 chops tocando no contratempo). Pads de memória por MIDI (C3–F3), então recebe MIDI.
# É efeito, mas fica em instrumentos/ para ter build, instalador e release próprios, como o GOGÓ.
# 0.1.0 = TESTE, sem licença (a licença entra na versão oficial: isp_licenca(Retalho retalho)).
juce_add_plugin(Retalho
    COMPANY_NAME                "InsideStudioSP"
    BUNDLE_ID                   com.insidestudiosp.retalho
    PLUGIN_MANUFACTURER_CODE    Insp
    PLUGIN_CODE                 Rtl1
    FORMATS                     ${VSTS_FORMATS}
    PLUGIN_NAME                 "RETALHO"
    PRODUCT_NAME                "RETALHO"
    VERSION                     ${RETALHO_VERSAO}
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    VST3_CATEGORIES             Fx Delay
    AU_MAIN_TYPE                kAudioUnitType_MusicEffect)

target_sources(Retalho PRIVATE Source/PluginProcessor.cpp Source/PluginEditor.cpp)
target_compile_definitions(Retalho PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_USE_MP3AUDIOFORMAT=1)   # SAMPLER: o vocal pode vir em MP3
target_link_libraries(Retalho
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_formats
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags)
