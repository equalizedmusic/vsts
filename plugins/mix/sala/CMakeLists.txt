juce_add_plugin(Sala
    COMPANY_NAME                "InsideStudioSP"
    BUNDLE_ID                   com.insidestudiosp.sala
    PLUGIN_MANUFACTURER_CODE    Insp
    PLUGIN_CODE                 Sal1
    FORMATS                     ${VSTS_FORMATS}
    PRODUCT_NAME                "SALA"
    VERSION                     ${VSTS_VERSAO}
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    VST3_CATEGORIES             Fx Reverb
    AU_MAIN_TYPE                kAudioUnitType_Effect)

target_sources(Sala PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

# peças renderizadas (instrumentos/_hardware/tools/pecas.py): knob, lâmpada, LED, tecla
file(GLOB sala_img CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Source/img/*")
juce_add_binary_data(SalaImg HEADER_NAME PluginImg.h NAMESPACE PluginImg SOURCES ${sala_img})

isp_licenca(Sala mix)

target_compile_definitions(Sala PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(Sala
    PRIVATE
        SalaImg
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags)
