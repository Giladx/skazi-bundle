cmake_minimum_required(VERSION 3.22)

set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Universal binary (Apple Silicon + Intel)")

project(BULLY VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Pass -DFETCHCONTENT_SOURCE_DIR_JUCE=/path/to/JUCE to use a local JUCE checkout
include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(JUCE)

set(FORMATS_LIST VST3 Standalone)
if(APPLE)
    list(APPEND FORMATS_LIST AU)
endif()

juce_add_plugin(Bully
    COMPANY_NAME             "SKAZI"
    PLUGIN_MANUFACTURER_CODE Skzi
    PLUGIN_CODE              Buly
    FORMATS                  ${FORMATS_LIST}
    PRODUCT_NAME             "BULLY"
    IS_SYNTH                 TRUE
    NEEDS_MIDI_INPUT         TRUE
    NEEDS_MIDI_OUTPUT        FALSE
    IS_MIDI_EFFECT           FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD  FALSE
    VST3_CATEGORIES          Instrument Synth
    AU_MAIN_TYPE             kAudioUnitType_MusicDevice)

set(ENGINE_SOURCES Source/Engine.cpp Source/Wavetables.cpp Source/Presets.cpp)

target_sources(Bully PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    ${ENGINE_SOURCES})

# The SKAZI logo (../Resources/skazi_logo.png)
set(LOGO "${CMAKE_CURRENT_SOURCE_DIR}/../Resources/skazi_logo.png")
if(EXISTS "${LOGO}")
    juce_add_binary_data(BullyData SOURCES "${LOGO}")
    target_link_libraries(Bully PRIVATE BullyData)
    target_compile_definitions(Bully PRIVATE SKAZI_HAS_LOGO=1)
endif()

target_compile_definitions(Bully PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

option(BULLY_TESTS "Build the engine test" OFF)
if(BULLY_TESTS)
    juce_add_console_app(BullyTest PRODUCT_NAME "BullyTest")
    target_sources(BullyTest PRIVATE Tests/BullyTest.cpp ${ENGINE_SOURCES})
    target_compile_definitions(BullyTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(BullyTest PRIVATE juce::juce_audio_processors juce::juce_dsp juce::juce_recommended_config_flags)
endif()

target_link_libraries(Bully
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
