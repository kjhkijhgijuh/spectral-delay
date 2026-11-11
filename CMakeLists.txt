cmake_minimum_required(VERSION 3.22)

set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")

project(SpectralDelay VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE: use a local checkout when JUCE_DIR is given, otherwise download a fixed release.
if(DEFINED JUCE_DIR)
    add_subdirectory(${JUCE_DIR} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.15
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

if(APPLE)
    set(SD_FORMATS AU VST3)
else()
    set(SD_FORMATS VST3)
endif()

juce_add_plugin(SpectralDelay
    COMPANY_NAME "Spectral Lab"
    BUNDLE_ID com.spectrallab.spectraldelay
    PLUGIN_MANUFACTURER_CODE Splb
    PLUGIN_CODE Spd1
    FORMATS ${SD_FORMATS}
    PRODUCT_NAME "Spectral Delay"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Delay Spectral
    AU_MAIN_TYPE kAudioUnitType_Effect)

target_sources(SpectralDelay PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(SpectralDelay PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(SpectralDelay
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# DSP core test (no JUCE)
add_executable(core_test tests/core_test.cpp)

# Host test: loads the built plugin like a DAW and checks it (optional).
option(SD_BUILD_HOST_TEST "Build the plugin host test" OFF)
if(SD_BUILD_HOST_TEST)
    juce_add_console_app(host_test PRODUCT_NAME "host_test")
    target_sources(host_test PRIVATE tests/host_test.cpp)
    target_compile_definitions(host_test PRIVATE
        JUCE_PLUGINHOST_VST3=1
        JUCE_PLUGINHOST_AU=$<IF:$<BOOL:${APPLE}>,1,0>
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0)
    target_link_libraries(host_test PRIVATE
        juce::juce_audio_utils
        juce::juce_recommended_config_flags)

    juce_add_console_app(ui_snapshot PRODUCT_NAME "ui_snapshot")
    target_sources(ui_snapshot PRIVATE tests/ui_snapshot.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
    target_compile_definitions(ui_snapshot PRIVATE
        JucePlugin_Name="Spectral Delay"
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0)
    target_link_libraries(ui_snapshot PRIVATE
        juce::juce_audio_utils
        juce::juce_recommended_config_flags)
endif()
