cmake_minimum_required(VERSION 3.22)
project(COLA_TUNE VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.6
    GIT_SHALLOW ON)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(ColaTune
    COMPANY_NAME "YoungCola"
    BUNDLE_ID com.youngcola.colatune
    PLUGIN_MANUFACTURER_CODE Ycol
    PLUGIN_CODE Ctun
    FORMATS VST3 Standalone
    PRODUCT_NAME "COLA-TUNE"
    VST3_CATEGORIES "Fx" "Pitch Shift"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE)

juce_add_binary_data(ColaAssets SOURCES youngcola.png)

target_sources(ColaTune PRIVATE
    PluginProcessor.cpp
    PluginEditor.cpp)

target_compile_definitions(ColaTune PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(ColaTune
    PRIVATE
        ColaAssets
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
