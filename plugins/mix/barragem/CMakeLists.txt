juce_add_plugin(Barragem
    COMPANY_NAME                "InsideStudioSP"
    BUNDLE_ID                   com.insidestudiosp.barragem
    PLUGIN_MANUFACTURER_CODE    Insp
    PLUGIN_CODE                 Bar1
    FORMATS                     ${VSTS_FORMATS}
    PRODUCT_NAME                "BARRAGEM"
    VERSION                     ${VSTS_VERSAO}
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    VST3_CATEGORIES             Fx Dynamics Mastering
    AU_MAIN_TYPE                kAudioUnitType_Effect)

target_sources(Barragem PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

# peças renderizadas (instrumentos/_hardware/tools/pecas.py): knob, lâmpada, LED, tecla
file(GLOB barragem_img CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Source/img/*")
juce_add_binary_data(BarragemImg HEADER_NAME PluginImg.h NAMESPACE PluginImg SOURCES ${barragem_img})

isp_licenca(Barragem mix)

target_compile_definitions(Barragem PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(Barragem
    PRIVATE
        BarragemImg
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags)
