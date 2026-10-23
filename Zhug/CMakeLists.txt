cmake_minimum_required(VERSION 3.22)

set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Universal binary (Apple Silicon + Intel)")

project(ZHUG VERSION 1.0.0)

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

juce_add_plugin(Zhug
    COMPANY_NAME             "SKAZI"
    PLUGIN_MANUFACTURER_CODE Skzi
    PLUGIN_CODE              Zhug
    FORMATS                  ${FORMATS_LIST}
    PRODUCT_NAME             "ZHUG"
    IS_SYNTH                 FALSE
    NEEDS_MIDI_INPUT         FALSE
    NEEDS_MIDI_OUTPUT        FALSE
    IS_MIDI_EFFECT           FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD  FALSE
    VST3_CATEGORIES          Fx Distortion
    AU_MAIN_TYPE             kAudioUnitType_Effect)

target_sources(Zhug PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/ZhugEngine.cpp
    Source/Presets.cpp)

# Save the SKAZI logo as ../Resources/skazi_logo.png to use it instead of the drawn stand-in
set(LOGO "${CMAKE_CURRENT_SOURCE_DIR}/../Resources/skazi_logo.png")
if(EXISTS "${LOGO}")
    juce_add_binary_data(ZhugData SOURCES "${LOGO}")
    target_link_libraries(Zhug PRIVATE ZhugData)
    target_compile_definitions(Zhug PRIVATE SHAK_HAS_LOGO=1)
endif()

target_compile_definitions(Zhug PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

option(ZHUG_TESTS "Build the engine test" OFF)
if(ZHUG_TESTS)
    juce_add_console_app(ZhugTest PRODUCT_NAME "ZhugTest")
    target_sources(ZhugTest PRIVATE Tests/ZhugTest.cpp Source/ZhugEngine.cpp Source/Presets.cpp)
    target_compile_definitions(ZhugTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(ZhugTest PRIVATE juce::juce_audio_processors juce::juce_dsp juce::juce_recommended_config_flags)
endif()

target_link_libraries(Zhug
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
