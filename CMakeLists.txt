cmake_minimum_required(VERSION 3.22)

# macOS : binaire universel (Apple Silicon + Intel)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "")
if(APPLE AND NOT CMAKE_OSX_ARCHITECTURES)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
endif()

project(vck VERSION 1.2.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE : dossier local external/JUCE-8.0.12, sinon telechargement automatique
set(JUCE_PATH "" CACHE PATH "Dossier JUCE local (optionnel)")
if(NOT JUCE_PATH AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/JUCE-8.0.12/CMakeLists.txt")
    set(JUCE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/external/JUCE-8.0.12")
endif()
if(JUCE_PATH)
    add_subdirectory(${JUCE_PATH} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        URL https://github.com/juce-framework/JUCE/archive/refs/tags/8.0.12.zip
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

# Effet audio qui recoit du MIDI (la voix entre, les notes MIDI jouent la porteuse).
# AU : "music effect" (aumf) pour accepter le MIDI dans Logic / Ableton.
juce_add_plugin(vck
    COMPANY_NAME "vck"
    BUNDLE_ID "com.vck.vck"
    PLUGIN_MANUFACTURER_CODE Vckd
    PLUGIN_CODE Vckv
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "vck"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Fx Vocals
    AU_MAIN_TYPE kAudioUnitType_MusicEffect
    COPY_PLUGIN_AFTER_BUILD FALSE)

juce_add_binary_data(vckAssets SOURCES assets/raven.png assets/cover.jpg)

set(VCK_SOURCES
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Widgets.cpp
    Source/ClipEditor.cpp
    Source/Presets.cpp)

target_sources(vck PRIVATE ${VCK_SOURCES})

target_compile_definitions(vck PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

if(MSVC)
    target_compile_options(vck PRIVATE /utf-8)
endif()

target_link_libraries(vck
    PRIVATE
        vckAssets
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Programme de test hors DAW (rendu audio + capture de l'interface)
option(VCK_TESTS "Construire le test DSP" ON)
if(VCK_TESTS)
    juce_add_console_app(vckTest PRODUCT_NAME "vckTest")
    target_sources(vckTest PRIVATE Tests/DspTest.cpp ${VCK_SOURCES})
    target_compile_definitions(vckTest PRIVATE
        JucePlugin_Name="vck" JucePlugin_VersionString="${PROJECT_VERSION}"
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_MODAL_LOOPS_PERMITTED=1)
    if(MSVC)
        target_compile_options(vckTest PRIVATE /utf-8)
        target_link_options(vckTest PRIVATE /STACK:16777216)
    endif()
    target_link_libraries(vckTest PRIVATE
        vckAssets
        juce::juce_audio_utils juce::juce_dsp
        juce::juce_recommended_config_flags)
endif()
