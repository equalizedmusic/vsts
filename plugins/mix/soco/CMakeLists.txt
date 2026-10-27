juce_add_plugin(Soco
    COMPANY_NAME                "InsideStudioSP"
    BUNDLE_ID                   com.insidestudiosp.soco
    PLUGIN_MANUFACTURER_CODE    Insp
    PLUGIN_CODE                 Soc1
    FORMATS                     ${VSTS_FORMATS}
    PRODUCT_NAME                "SOCO"
    VERSION                     ${VSTS_VERSAO}
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    VST3_CATEGORIES             Fx Dynamics
    AU_MAIN_TYPE                kAudioUnitType_Effect)

target_sources(Soco PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

# peças renderizadas (instrumentos/_hardware/tools/pecas.py): knob, lâmpada, LED, tecla
file(GLOB soco_img CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Source/img/*")
juce_add_binary_data(SocoImg HEADER_NAME PluginImg.h NAMESPACE PluginImg SOURCES ${soco_img})

isp_licenca(Soco mix)

target_compile_definitions(Soco PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(Soco
    PRIVATE
        SocoImg
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags)
