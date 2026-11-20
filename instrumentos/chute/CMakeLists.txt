# versão: o primeiro "## x.y.z" de novidades/Chute.md
vsts_versao(Chute.md CHUTE_VERSAO)

# CHUTE: kick designer (synth de kick, uma nota = uma batida). Presets em bancos .chutebank: a FÁBRICA vai dentro do plugin (bancos.cmake) e os outros (bancos/*.chutebank) vão no instalador. ChuteLink.cpp: memória compartilhada que avisa cada batida ao ALIGN e ao BAREBASS (LINK, sem
# sidechain); os dois compilam o mesmo arquivo.
# 0.2.0 = oficial, paga: licença do pacote chute (sem ativação fica mudo e não publica no LINK).
juce_add_plugin(Chute
    COMPANY_NAME                "InsideStudioSP"
    BUNDLE_ID                   com.insidestudiosp.chute
    PLUGIN_MANUFACTURER_CODE    Insp
    PLUGIN_CODE                 Cht1
    FORMATS                     ${VSTS_FORMATS}
    PLUGIN_NAME                 "CHUTE"
    PRODUCT_NAME                "CHUTE"
    VERSION                     ${CHUTE_VERSAO}
    IS_SYNTH                    TRUE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    VST3_CATEGORIES             Instrument Drum
    AU_MAIN_TYPE                kAudioUnitType_MusicDevice)

isp_licenca(Chute chute)

target_sources(Chute PRIVATE Source/PluginProcessor.cpp Source/PluginEditor.cpp Source/ChuteLink.cpp)
include(${CMAKE_CURRENT_SOURCE_DIR}/bancos.cmake)
chute_bancos(Chute)
target_compile_definitions(Chute PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(Chute
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags)
