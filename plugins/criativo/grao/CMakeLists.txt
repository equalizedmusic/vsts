juce_add_plugin(Grao
    COMPANY_NAME                "InsideStudioSP"
    BUNDLE_ID                   com.insidestudiosp.grao
    PLUGIN_MANUFACTURER_CODE    Insp
    PLUGIN_CODE                 Gra1
    FORMATS                     ${VSTS_FORMATS}
    PRODUCT_NAME                "GRAO"
    VERSION                     ${VSTS_VERSAO}
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    VST3_CATEGORIES             Fx Delay Reverb
    AU_MAIN_TYPE                kAudioUnitType_Effect)

target_sources(Grao PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

# peças renderizadas (instrumentos/_hardware/tools/pecas.py): knobs do pedal (SÉRIE KNOB), lâmpada, LED, tecla e a madeira da moldura
file(GLOB grao_img CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Source/img/*")
juce_add_binary_data(GraoImg HEADER_NAME PluginImg.h NAMESPACE PluginImg SOURCES ${grao_img})

target_compile_definitions(Grao PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(Grao
    PRIVATE
        GraoImg
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags)
