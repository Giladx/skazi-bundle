cmake_minimum_required(VERSION 3.22)

# macOS: universal binary (Apple Silicon + Intel). Must be set before project().
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "")

project(SKAZI VERSION 1.3.0)

if(APPLE)
    set(SKAZI_FORMATS VST3 AU Standalone)
else()
    set(SKAZI_FORMATS VST3 Standalone)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")


# Path to a JUCE 8 checkout (keep it short on Windows: MAX_PATH)
set(JUCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/JUCE" CACHE PATH "JUCE source folder")
add_subdirectory(${JUCE_DIR} ${CMAKE_BINARY_DIR}/JUCE)

juce_add_plugin(SKAZI
    COMPANY_NAME "SKAZI"
    PRODUCT_NAME "SKAZI"
    BUNDLE_ID "com.skazi.synth"
    PLUGIN_MANUFACTURER_CODE Skzi
    PLUGIN_CODE Skzs
    FORMATS ${SKAZI_FORMATS}
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument Synth
    AU_MAIN_TYPE kAudioUnitType_MusicDevice)

target_sources(SKAZI PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Presets.cpp
    Source/StandaloneApp.cpp)

target_compile_definitions(SKAZI PUBLIC
    JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1   # Source/StandaloneApp.cpp: MIDI inputs open by themselves
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

# The SKAZI logo (../Resources/skazi_logo.png) shown in the header
set(LOGO "${CMAKE_CURRENT_SOURCE_DIR}/../Resources/skazi_logo.png")
if(EXISTS "${LOGO}")
    juce_add_binary_data(SkaziLogo SOURCES "${LOGO}")
    target_link_libraries(SKAZI PRIVATE SkaziLogo)
    target_compile_definitions(SKAZI PRIVATE SKAZI_HAS_LOGO=1)
endif()

target_link_libraries(SKAZI
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Offline render + UI snapshot tool (not shipped)
option(SKAZI_BUILD_TESTS "Build the offline render test" OFF)
if(SKAZI_BUILD_TESTS)
    juce_add_console_app(RenderTest PRODUCT_NAME "RenderTest")
    target_sources(RenderTest PRIVATE Tools/RenderTest.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp Source/Presets.cpp)
    target_compile_definitions(RenderTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JucePlugin_Name="SKAZI")
    target_link_libraries(RenderTest PRIVATE juce::juce_audio_utils juce::juce_dsp
        PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

    # the sequencer must never play by itself in a DAW
    juce_add_console_app(SeqGateTest PRODUCT_NAME "SeqGateTest")
    target_sources(SeqGateTest PRIVATE Tools/SeqGateTest.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp Source/Presets.cpp)
    target_compile_definitions(SeqGateTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JucePlugin_Name="SKAZI")
    target_link_libraries(SeqGateTest PRIVATE juce::juce_audio_utils juce::juce_dsp
        PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
    # timing: latency reporting, note offsets, LFO and sequencer grid
    juce_add_console_app(TimingTest PRODUCT_NAME "TimingTest")
    target_sources(TimingTest PRIVATE Tools/TimingTest.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp Source/Presets.cpp)
    target_compile_definitions(TimingTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JucePlugin_Name="SKAZI")
    target_link_libraries(TimingTest PRIVATE juce::juce_audio_utils juce::juce_dsp
        PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

    # RANDOM covers every section, never silent or clipping; distortion types stay sane
    juce_add_console_app(RandomTest PRODUCT_NAME "RandomTest")
    target_sources(RandomTest PRIVATE Tools/RandomTest.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp Source/Presets.cpp)
    target_compile_definitions(RandomTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JucePlugin_Name="SKAZI")
    target_link_libraries(RandomTest PRIVATE juce::juce_audio_utils juce::juce_dsp
        PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

    # step locks, kick duck, A/B morph, macros, voice cards
    juce_add_console_app(FeatureTest PRODUCT_NAME "FeatureTest")
    target_sources(FeatureTest PRIVATE Tools/FeatureTest.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp Source/Presets.cpp)
    target_compile_definitions(FeatureTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JucePlugin_Name="SKAZI")
    target_link_libraries(FeatureTest PRIVATE juce::juce_audio_utils juce::juce_dsp
        PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
endif()
