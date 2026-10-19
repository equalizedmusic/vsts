# Fotos das interfaces: um executável por plugin que abre o editor de verdade, toca um loop de teste
# e grava a tela em PNG (200%). Projeto separado do build dos plugins; roda no workflow "shots".
cmake_minimum_required(VERSION 3.22)

set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "macOS mínimo")
project(VSTS_SHOTS VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        master
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(JUCE)

# efeitos (plugins/<linha>/<nome>) e instrumentos (instrumentos/<nome>, tocados por MIDI na foto)
file(GLOB plugin_dirs LIST_DIRECTORIES true "${CMAKE_CURRENT_SOURCE_DIR}/../plugins/*/*")
file(GLOB inst_dirs LIST_DIRECTORIES true "${CMAKE_CURRENT_SOURCE_DIR}/../instrumentos/*")
foreach(dir ${plugin_dirs} ${inst_dirs})
    if(EXISTS "${dir}/Source/PluginProcessor.cpp")
        get_filename_component(name "${dir}" NAME)
        string(TOUPPER "${name}" upper)
        set(t "shot_${name}")
        juce_add_console_app(${t} PRODUCT_NAME "${t}")
        file(GLOB srcs "${dir}/Source/*.cpp")
        target_sources(${t} PRIVATE ${srcs} ShotMain.cpp)
        target_include_directories(${t} PRIVATE "${dir}/Source")
        target_compile_definitions(${t} PRIVATE
            JUCE_WEB_BROWSER=0
            JUCE_USE_CURL=0
            JUCE_MODAL_LOOPS_PERMITTED=1
            SHOT_PLUGIN="${upper}")
        if(dir IN_LIST inst_dirs)
            # synth: sem entrada, toca acordes por MIDI; tela base 1200×740 (foto a 200%)
            target_compile_definitions(${t} PRIVATE SHOT_SYNTH=1 SHOT_W=2400 SHOT_H=1480)
        endif()
        target_link_libraries(${t}
            PRIVATE
                juce::juce_audio_utils
                juce::juce_dsp
            PUBLIC
                juce::juce_recommended_config_flags)
    endif()
endforeach()
