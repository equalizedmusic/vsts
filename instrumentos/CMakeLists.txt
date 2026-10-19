cmake_minimum_required(VERSION 3.22)

# Instrumentos (VSTi) da InsideStudioSP. Projeto separado dos efeitos de plugins/: tem o próprio build
# (workflow build-instrumentos), que só roda quando instrumentos/ muda.

# Mac: binário universal (Intel + Apple Silicon) a partir do macOS 11
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "macOS mínimo")
# Windows: runtime estático, o plugin não depende do VC++ Redistributable
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

project(VSTS_INSTRUMENTOS VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        master
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(JUCE)

set(VSTS_FORMATS VST3 AU)

# cada pasta em instrumentos/ com CMakeLists.txt é um instrumento
file(GLOB inst_dirs LIST_DIRECTORIES true "${CMAKE_CURRENT_SOURCE_DIR}/*")
foreach(dir ${inst_dirs})
    if(IS_DIRECTORY "${dir}" AND EXISTS "${dir}/CMakeLists.txt")
        add_subdirectory("${dir}")
    endif()
endforeach()
