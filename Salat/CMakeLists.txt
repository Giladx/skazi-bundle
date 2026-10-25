cmake_minimum_required(VERSION 3.22)

set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Universal binary (Apple Silicon + Intel)")

project(SALAT VERSION 1.0.0)

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

juce_add_plugin(Salat
    COMPANY_NAME             "SKAZI"
    PLUGIN_MANUFACTURER_CODE Skzi
    PLUGIN_CODE              Slat
    FORMATS                  ${FORMATS_LIST}
    PRODUCT_NAME             "SALAT"
    IS_SYNTH                 FALSE
    NEEDS_MIDI_INPUT         FALSE
    NEEDS_MIDI_OUTPUT        FALSE
    IS_MIDI_EFFECT           FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD  FALSE
    VST3_CATEGORIES          Fx Distortion Delay
    AU_MAIN_TYPE             kAudioUnitType_Effect)

target_sources(Salat PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/SalatEngine.cpp
    Source/Presets.cpp)

# Save the SKAZI logo as ../Resources/skazi_logo.png to use it instead of the drawn stand-in
set(LOGO "${CMAKE_CURRENT_SOURCE_DIR}/../Resources/skazi_logo.png")
if(EXISTS "${LOGO}")
    juce_add_binary_data(SalatData SOURCES "${LOGO}")
    target_link_libraries(Salat PRIVATE SalatData)
    target_compile_definitions(Salat PRIVATE SHAK_HAS_LOGO=1)
endif()

target_compile_definitions(Salat PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

option(SALAT_TESTS "Build the engine test" OFF)
if(SALAT_TESTS)
    juce_add_console_app(SalatTest PRODUCT_NAME "SalatTest")
    target_sources(SalatTest PRIVATE Tests/SalatTest.cpp Source/SalatEngine.cpp Source/Presets.cpp)
    target_compile_definitions(SalatTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(SalatTest PRIVATE juce::juce_audio_processors juce::juce_audio_formats juce::juce_dsp juce::juce_recommended_config_flags)

    # Editor snapshot for checking the skin
    juce_add_console_app(SalatShot PRODUCT_NAME "SalatShot")
    target_sources(SalatShot PRIVATE Tests/SalatShot.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp Source/SalatEngine.cpp Source/Presets.cpp)
    target_compile_definitions(SalatShot PRIVATE JucePlugin_Name="SALAT" JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_MODAL_LOOPS_PERMITTED=1)
    if(TARGET SalatData)
        target_link_libraries(SalatShot PRIVATE SalatData)
        target_compile_definitions(SalatShot PRIVATE SHAK_HAS_LOGO=1)
    endif()
    target_link_libraries(SalatShot PRIVATE juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)
endif()

target_link_libraries(Salat
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
