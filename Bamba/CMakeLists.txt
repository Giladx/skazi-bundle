cmake_minimum_required(VERSION 3.22)

set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Universal binary (Apple Silicon + Intel)")

project(BAMBA VERSION 1.1.0)

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

juce_add_plugin(Bamba
    COMPANY_NAME             "SKAZI"
    PLUGIN_MANUFACTURER_CODE Skzi
    PLUGIN_CODE              Bamb
    FORMATS                  ${FORMATS_LIST}
    PRODUCT_NAME             "BAMBA"
    IS_SYNTH                 TRUE
    NEEDS_MIDI_INPUT         TRUE
    NEEDS_MIDI_OUTPUT        FALSE
    IS_MIDI_EFFECT           FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD  FALSE
    VST3_CATEGORIES          Instrument Synth
    AU_MAIN_TYPE             kAudioUnitType_MusicDevice)

set(BAMBA_SOURCES
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/BambaEngine.cpp
    Source/Presets.cpp
    Source/RiserLibrary.cpp)
target_sources(Bamba PRIVATE ${BAMBA_SOURCES})

# Save the SKAZI logo as ../Resources/skazi_logo.png to use it instead of the drawn stand-in
set(LOGO "${CMAKE_CURRENT_SOURCE_DIR}/../Resources/skazi_logo.png")
if(EXISTS "${LOGO}")
    juce_add_binary_data(BambaData SOURCES "${LOGO}")
    target_link_libraries(Bamba PRIVATE BambaData)
    target_compile_definitions(Bamba PRIVATE SHAK_HAS_LOGO=1)
endif()

target_compile_definitions(Bamba PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

# Engine / preset / Splice-library test, and a tool that renders the editor to a PNG
option(BAMBA_TESTS "Build the BAMBA tests and tools" OFF)
if(BAMBA_TESTS)
    foreach(tool Test SkinShot)
        juce_add_console_app(Bamba${tool} PRODUCT_NAME "Bamba${tool}")
        target_sources(Bamba${tool} PRIVATE Tests/Bamba${tool}.cpp ${BAMBA_SOURCES})
        target_include_directories(Bamba${tool} PRIVATE Source)
        target_compile_definitions(Bamba${tool} PRIVATE
            JucePlugin_Name="BAMBA" JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_MODAL_LOOPS_PERMITTED=1)
        target_link_libraries(Bamba${tool} PRIVATE juce::juce_audio_utils juce::juce_recommended_config_flags)
        if(TARGET BambaData)
            target_link_libraries(Bamba${tool} PRIVATE BambaData)
            target_compile_definitions(Bamba${tool} PRIVATE SHAK_HAS_LOGO=1)
        endif()
    endforeach()
endif()

target_link_libraries(Bamba
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
