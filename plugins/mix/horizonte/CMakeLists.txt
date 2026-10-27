juce_add_plugin(Horizonte
    COMPANY_NAME                "InsideStudioSP"
    BUNDLE_ID                   com.insidestudiosp.horizonte
    PLUGIN_MANUFACTURER_CODE    Insp
    PLUGIN_CODE                 Hor1
    FORMATS                     ${VSTS_FORMATS}
    PRODUCT_NAME                "HORIZONTE"
    VERSION                     ${VSTS_VERSAO}
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    VST3_CATEGORIES             Fx Spatial
    AU_MAIN_TYPE                kAudioUnitType_Effect)

target_sources(Horizonte PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

# peças renderizadas (instrumentos/_hardware/tools/pecas.py): knob, lâmpada, LED, tecla
file(GLOB horizonte_img CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Source/img/*")
juce_add_binary_data(HorizonteImg HEADER_NAME PluginImg.h NAMESPACE PluginImg SOURCES ${horizonte_img})

isp_licenca(Horizonte mix)

target_compile_definitions(Horizonte PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(Horizonte
    PRIVATE
        HorizonteImg
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags)
