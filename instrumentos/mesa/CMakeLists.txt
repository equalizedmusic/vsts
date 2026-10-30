# versão: o primeiro "## x.y.z" de novidades/Mesa.md
vsts_versao(Mesa.md MESA_VERSAO)

# MESA é efeito (uma cópia por canal do DAW), mas fica em instrumentos/ para ter build e instalador próprios
juce_add_plugin(Mesa
    COMPANY_NAME                "InsideStudioSP"
    BUNDLE_ID                   com.insidestudiosp.mesa
    PLUGIN_MANUFACTURER_CODE    Insp
    PLUGIN_CODE                 Msa1
    FORMATS                     ${VSTS_FORMATS}
    PRODUCT_NAME                "MESA"
    VERSION                     ${MESA_VERSAO}
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    VST3_CATEGORIES             Fx EQ Dynamics
    AU_MAIN_TYPE                kAudioUnitType_Effect)

target_sources(Mesa PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

# peças renderizadas no Blender (tools/render_mesa.py -> tools/recorta.py): chapas.jpg e pecas.png
file(GLOB mesa_img CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Source/img/*")
juce_add_binary_data(MesaImg HEADER_NAME PluginImg.h NAMESPACE PluginImg SOURCES ${mesa_img})

isp_licenca(Mesa mesa)

target_compile_definitions(Mesa PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(Mesa
    PRIVATE
        MesaImg
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags)
