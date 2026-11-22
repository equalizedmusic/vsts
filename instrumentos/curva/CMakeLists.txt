# versão: o primeiro "## x.y.z" de novidades/Curva.md
vsts_versao(Curva.md CURVA_VERSAO)

# CURVA: monitor de volume pela curva de mesma audibilidade (Fletcher-Munson / ISO 226). É efeito (vai por último no
# master, só para ouvir), mas fica em instrumentos/ para ter build, instalador e release próprios, como o ALIGN.
# Licença (0.2.0): pacote curva, grátis como o ALIGN (sem ativar, o som passa direto).
juce_add_plugin(Curva
    COMPANY_NAME                "InsideStudioSP"
    BUNDLE_ID                   com.insidestudiosp.curva
    PLUGIN_MANUFACTURER_CODE    Insp
    PLUGIN_CODE                 Crv1
    FORMATS                     ${VSTS_FORMATS}
    PLUGIN_NAME                 "CURVA"
    PRODUCT_NAME                "CURVA"
    VERSION                     ${CURVA_VERSAO}
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    VST3_CATEGORIES             Fx Analyzer Tools
    AU_MAIN_TYPE                kAudioUnitType_Effect)

target_sources(Curva PRIVATE Source/PluginProcessor.cpp Source/PluginEditor.cpp Source/EditorPaineis.cpp)
isp_licenca(Curva curva)   # pacote 'curva' registrado em licenca/cliente (licenca.cmake + IspLicenca.h)
target_compile_definitions(Curva PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(Curva
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags)
