juce_add_plugin(Grao
    COMPANY_NAME                "Equalized Music"
    BUNDLE_ID                   com.equalizedmusic.grao
    PLUGIN_MANUFACTURER_CODE    Eqzm
    PLUGIN_CODE                 Gra1
    FORMATS                     ${VSTS_FORMATS}
    PRODUCT_NAME                "GRAO"
    VERSION                     0.1.0
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

target_compile_definitions(Grao PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(Grao
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags)
