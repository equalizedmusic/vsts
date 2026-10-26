# alvo ConsoleIsp (nome genérico "Console" poderia colidir); o bundle e o zip saem como CONSOLE (PRODUCT_NAME)
juce_add_plugin(ConsoleIsp
    COMPANY_NAME                "InsideStudioSP"
    BUNDLE_ID                   com.insidestudiosp.console
    PLUGIN_MANUFACTURER_CODE    Insp
    PLUGIN_CODE                 Cns1
    FORMATS                     ${VSTS_FORMATS}
    PRODUCT_NAME                "CONSOLE"
    VERSION                     ${VSTS_VERSAO}
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    VST3_CATEGORIES             Fx Distortion
    AU_MAIN_TYPE                kAudioUnitType_Effect)

target_sources(ConsoleIsp PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

# peças de hardware renderizadas (instrumentos/_hardware/tools/pecas.py): knob, lâmpada, LED, tecla
file(GLOB console_img CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Source/img/*")
juce_add_binary_data(ConsoleImg HEADER_NAME PluginImg.h NAMESPACE PluginImg SOURCES ${console_img})

target_compile_definitions(ConsoleIsp PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(ConsoleIsp
    PRIVATE
        ConsoleImg
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags)
