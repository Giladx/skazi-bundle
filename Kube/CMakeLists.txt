cmake_minimum_required(VERSION 3.22)

set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Universal binary (Apple Silicon + Intel)")

project(KUBE VERSION 1.0.0)

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

juce_add_plugin(Kube
    COMPANY_NAME             "SKAZI"
    PLUGIN_MANUFACTURER_CODE Skzi
    PLUGIN_CODE              Kube
    FORMATS                  ${FORMATS_LIST}
    PRODUCT_NAME             "KUBE"
    IS_SYNTH                 TRUE
    NEEDS_MIDI_INPUT         TRUE
    NEEDS_MIDI_OUTPUT        FALSE
    IS_MIDI_EFFECT           FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD  FALSE
    VST3_CATEGORIES          Instrument Synth
    AU_MAIN_TYPE             kAudioUnitType_MusicDevice)

set(KUBE_ENGINE Source/KubeEngine.cpp Source/Wavetables.cpp Source/Presets.cpp)

target_sources(Kube PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    ${KUBE_ENGINE})

target_compile_definitions(Kube PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

option(KUBE_TESTS "Build the engine test" OFF)
if(KUBE_TESTS)
    # Renders every preset to WAV and checks levels
    juce_add_console_app(KubeTest PRODUCT_NAME "KubeTest")
    target_sources(KubeTest PRIVATE Tests/KubeTest.cpp ${KUBE_ENGINE})
    target_compile_definitions(KubeTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(KubeTest PRIVATE juce::juce_audio_processors juce::juce_audio_formats juce::juce_dsp juce::juce_recommended_config_flags)

    # Editor snapshot for checking the skin
    juce_add_console_app(KubeShot PRODUCT_NAME "KubeShot")
    target_sources(KubeShot PRIVATE Tests/KubeShot.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp ${KUBE_ENGINE})
    target_compile_definitions(KubeShot PRIVATE JucePlugin_Name="KUBE" JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_MODAL_LOOPS_PERMITTED=1)
    target_link_libraries(KubeShot PRIVATE juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)

    # Loads the built KUBE.vst3 like a DAW
    juce_add_console_app(KubeHostTest PRODUCT_NAME "KubeHostTest")
    target_sources(KubeHostTest PRIVATE Tests/HostTest.cpp)
    target_compile_definitions(KubeHostTest PRIVATE JUCE_PLUGINHOST_VST3=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_MODAL_LOOPS_PERMITTED=1)
    target_link_libraries(KubeHostTest PRIVATE juce::juce_audio_processors juce::juce_recommended_config_flags)
endif()

target_link_libraries(Kube
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
