cmake_minimum_required(VERSION 3.22)

set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Universal binary (Apple Silicon + Intel)")

project(KICKASS VERSION 1.0.0)

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

# Manufacturer code Ashs (not Skzi): KICKASS shipped with it before joining the bundle,
# and changing it would stop existing projects from finding the plugin.
juce_add_plugin(Kickass
    COMPANY_NAME             "SKAZI"
    PLUGIN_MANUFACTURER_CODE Ashs
    PLUGIN_CODE              Kick
    BUNDLE_ID                com.asherswissa.kickass
    FORMATS                  ${FORMATS_LIST}
    PRODUCT_NAME             "KICKASS"
    IS_SYNTH                 TRUE
    NEEDS_MIDI_INPUT         TRUE
    NEEDS_MIDI_OUTPUT        FALSE
    IS_MIDI_EFFECT           FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD  FALSE
    VST3_CATEGORIES          Instrument Drum
    AU_MAIN_TYPE             kAudioUnitType_MusicDevice)

set(KICKASS_SOURCES
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/WaveformDisplay.cpp
    Source/KickLibrary.cpp)

target_sources(Kickass PRIVATE ${KICKASS_SOURCES})
target_include_directories(Kickass PRIVATE Source)

target_compile_definitions(Kickass PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(Kickass
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Preset / engine / Splice-library test, a host test that loads the built VST3, and a
# tool that renders the editor to a PNG.
option(KICKASS_TESTS "Build the KICKASS tests and tools" OFF)
if(KICKASS_TESTS)
    foreach(tool PresetTest SkinShot)
        juce_add_console_app(Kickass${tool} PRODUCT_NAME "Kickass${tool}")
        target_sources(Kickass${tool} PRIVATE Tests/${tool}.cpp ${KICKASS_SOURCES})
        target_include_directories(Kickass${tool} PRIVATE Source)
        target_compile_definitions(Kickass${tool} PRIVATE
            JucePlugin_Name="KICKASS" JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_MODAL_LOOPS_PERMITTED=1)
        target_link_libraries(Kickass${tool} PRIVATE juce::juce_audio_utils juce::juce_recommended_config_flags)
    endforeach()

    juce_add_console_app(KickassHostTest PRODUCT_NAME "KickassHostTest")
    target_sources(KickassHostTest PRIVATE Tests/HostTest.cpp)
    target_compile_definitions(KickassHostTest PRIVATE JUCE_PLUGINHOST_VST3=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(KickassHostTest PRIVATE juce::juce_audio_processors juce::juce_recommended_config_flags)
endif()
