cmake_minimum_required(VERSION 3.22)

project(Op1Clone VERSION 1.0.0)

# Add JUCE
add_subdirectory(JUCE)

# Create the plugin target
juce_add_plugin(Op1Clone
    COMPANY_NAME "YourCompany"
    PLUGIN_MANUFACTURER_CODE YcCm
    PLUGIN_CODE Op1C
    FORMATS AU VST3 Standalone
    PRODUCT_ID "Op1Clone"
    VERSION "1.0.0"
    DESCRIPTION "OP-1 Clone Sampler"
    PLUGIN_MANUFACTURER "YourCompany"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    PLUGIN_CODE_JucePlugin_IsSynth 1
)

# Core source files (portable C++)
target_sources(Op1Clone PRIVATE
    Source/Core/SamplerVoice.cpp
    Source/Core/SamplerEngine.cpp
)

# JUCE wrapper source files
target_sources(Op1Clone PRIVATE
    Source/JuceWrapper/PluginProcessor.cpp
    Source/JuceWrapper/PluginProcessor.h
    Source/JuceWrapper/PluginEditor.cpp
    Source/JuceWrapper/PluginEditor.h
    Source/JuceWrapper/JuceEngineAdapter.cpp
    Source/JuceWrapper/JuceEngineAdapter.h
)

# Include directories
target_include_directories(Op1Clone PUBLIC
    Source
)

# Link JUCE modules
target_link_libraries(Op1Clone
    PRIVATE
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_plugin_client
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_core
        juce::juce_data_structures
        juce::juce_dsp
        juce::juce_events
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

