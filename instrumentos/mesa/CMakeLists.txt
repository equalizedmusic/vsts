# versão: o primeiro "## x.y.z" de novidades/Mesa.md
vsts_versao(Mesa.md MESA_VERSAO)

# MESA são DOIS plugins do mesmo código (fica em instrumentos/ para ter build e instalador próprios):
#  - MESA (alvo Mesa): a console, no master do DAW. Mantém o código Msa1 da 0.1/0.2.
#  - MESA CANAL (alvo MesaCanal): vai no fim de cada faixa (tipo CANAL) ou no canal de grupo (tipo GRUPO).
# Os dois se acham pela Ponte (Source/Ponte.*): memória do processo, não do binário.
set(mesa_fontes Source/PluginProcessor.cpp Source/PluginEditor.cpp Source/Ponte.cpp)

function(mesa_plugin alvo nome produto codigo bundle console)
    juce_add_plugin(${alvo}
        COMPANY_NAME                "InsideStudioSP"
        BUNDLE_ID                   com.insidestudiosp.${bundle}
        PLUGIN_MANUFACTURER_CODE    Insp
        PLUGIN_CODE                 ${codigo}
        FORMATS                     ${VSTS_FORMATS}
        PLUGIN_NAME                 "${nome}"
        PRODUCT_NAME                "${produto}"
        VERSION                     ${MESA_VERSAO}
        IS_SYNTH                    FALSE
        NEEDS_MIDI_INPUT            FALSE
        NEEDS_MIDI_OUTPUT           FALSE
        IS_MIDI_EFFECT              FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        COPY_PLUGIN_AFTER_BUILD     FALSE
        VST3_CATEGORIES             Fx EQ Dynamics
        AU_MAIN_TYPE                kAudioUnitType_Effect)
    target_sources(${alvo} PRIVATE ${mesa_fontes})
    isp_licenca(${alvo} mesa)
    target_compile_definitions(${alvo} PUBLIC
        MESA_CONSOLE=${console}
        ISP_INDICE_FAIXA=1
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)
    target_link_libraries(${alvo}
        PRIVATE
            juce::juce_audio_utils
            juce::juce_dsp
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags)
endfunction()

mesa_plugin(Mesa      "MESA"       "MESA"       Msa1 mesa      1)
mesa_plugin(MesaCanal "MESA CANAL" "MESA-CANAL" Msa2 mesacanal 0)
