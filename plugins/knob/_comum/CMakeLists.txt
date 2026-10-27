# SÉRIE KNOB — o que os 10 pedais compartilham: processor, interface e as imagens renderizadas no Blender.
# Esta pasta entra antes dos pedais (o glob da raiz é em ordem alfabética e "_" vem antes das letras).
# Cada pedal só chama serie_knob_pedal(...) e traz o seu Source/Pedal.cpp (Info + motor de som).

file(GLOB SERIE_KNOB_IMAGENS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/assets/*.png" "${CMAKE_CURRENT_SOURCE_DIR}/assets/*.jpg")
juce_add_binary_data(SerieKnobData
    HEADER_NAME BinaryData_SerieKnob.h
    NAMESPACE   SerieKnobData
    SOURCES     ${SERIE_KNOB_IMAGENS})
set_target_properties(SerieKnobData PROPERTIES POSITION_INDEPENDENT_CODE ON)

set(SERIE_KNOB_COMUM "${CMAKE_CURRENT_SOURCE_DIR}" CACHE INTERNAL "pasta comum da SÉRIE KNOB")

function(serie_knob_pedal alvo codigo produto bundle)
    juce_add_plugin(${alvo}
        COMPANY_NAME                "InsideStudioSP"
        BUNDLE_ID                   com.insidestudiosp.${bundle}
        PLUGIN_MANUFACTURER_CODE    Insp
        PLUGIN_CODE                 ${codigo}
        FORMATS                     ${VSTS_FORMATS}
        PRODUCT_NAME                "${produto}"
        VERSION                     ${VSTS_VERSAO}
        IS_SYNTH                    FALSE
        NEEDS_MIDI_INPUT            TRUE
        NEEDS_MIDI_OUTPUT           FALSE
        IS_MIDI_EFFECT              FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        COPY_PLUGIN_AFTER_BUILD     FALSE
        VST3_CATEGORIES             Fx
        AU_MAIN_TYPE                kAudioUnitType_MusicEffect)

    target_sources(${alvo} PRIVATE
        ${SERIE_KNOB_COMUM}/Source/KnobProcessor.cpp
        ${SERIE_KNOB_COMUM}/Source/KnobEditor.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Source/Pedal.cpp)
    target_include_directories(${alvo} PRIVATE ${SERIE_KNOB_COMUM}/Source ${CMAKE_CURRENT_SOURCE_DIR}/Source)

    isp_licenca(${alvo} knob)

    target_compile_definitions(${alvo} PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)
    if(MSVC)
        target_compile_options(${alvo} PRIVATE /utf-8)
    endif()

    target_link_libraries(${alvo}
        PRIVATE
            SerieKnobData
            juce::juce_audio_utils
            juce::juce_dsp
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags)
endfunction()
