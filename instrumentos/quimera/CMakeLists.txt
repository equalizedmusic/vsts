juce_add_plugin(Quimera
    COMPANY_NAME                "InsideStudioSP"
    BUNDLE_ID                   com.insidestudiosp.quimera
    PLUGIN_MANUFACTURER_CODE    Insp
    PLUGIN_CODE                 Qmr1
    FORMATS                     ${VSTS_FORMATS}
    PRODUCT_NAME                "QUIMERA"
    VERSION                     0.1.0
    IS_SYNTH                    TRUE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    VST3_CATEGORIES             Instrument Synth
    AU_MAIN_TYPE                kAudioUnitType_MusicDevice)

target_sources(Quimera PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Engine.cpp
    Source/WaveBank.cpp
    Source/Fx.cpp
    Source/Presets.cpp)

target_compile_definitions(Quimera PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(Quimera
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags)
