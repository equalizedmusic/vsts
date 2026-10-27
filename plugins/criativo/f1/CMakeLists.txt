juce_add_plugin(F1
    COMPANY_NAME                "InsideStudioSP"
    BUNDLE_ID                   com.insidestudiosp.f1
    PLUGIN_MANUFACTURER_CODE    Insp
    PLUGIN_CODE                 InF1
    FORMATS                     ${VSTS_FORMATS}
    PRODUCT_NAME                "F1"
    VERSION                     ${VSTS_VERSAO}
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    VST3_CATEGORIES             Fx Filter
    AU_MAIN_TYPE                kAudioUnitType_Effect)

target_sources(F1 PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

# peças renderizadas (instrumentos/_hardware/tools/pecas.py): knobs do pedal (SÉRIE KNOB), lâmpada, LED, tecla e a madeira da moldura
file(GLOB f1_img CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Source/img/*")
juce_add_binary_data(F1Img HEADER_NAME PluginImg.h NAMESPACE PluginImg SOURCES ${f1_img})

isp_licenca(F1 criativo)

target_compile_definitions(F1 PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(F1
    PRIVATE
        F1Img
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags)
