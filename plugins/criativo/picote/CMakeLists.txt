juce_add_plugin(Picote
    COMPANY_NAME                "InsideStudioSP"
    BUNDLE_ID                   com.insidestudiosp.picote
    PLUGIN_MANUFACTURER_CODE    Insp
    PLUGIN_CODE                 Pic1
    FORMATS                     ${VSTS_FORMATS}
    PRODUCT_NAME                "PICOTE"
    VERSION                     ${VSTS_VERSAO}
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    VST3_CATEGORIES             Fx Dynamics
    AU_MAIN_TYPE                kAudioUnitType_Effect)

target_sources(Picote PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

# peças renderizadas (instrumentos/_hardware/tools/pecas.py): knobs do pedal (SÉRIE KNOB), lâmpada, LED, tecla e a madeira da moldura
file(GLOB picote_img CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Source/img/*")
juce_add_binary_data(PicoteImg HEADER_NAME PluginImg.h NAMESPACE PluginImg SOURCES ${picote_img})

isp_licenca(Picote criativo)

target_compile_definitions(Picote PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(Picote
    PRIVATE
        PicoteImg
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags)
