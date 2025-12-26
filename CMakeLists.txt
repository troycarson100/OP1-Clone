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

# Enable custom standalone app to auto-enable MIDI devices
target_compile_definitions(Op1Clone PRIVATE
    JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1
)

# Option to enable Signalsmith Stretch backend (MIT license)
# Set to ON to use Signalsmith Stretch, OFF to use passthrough backend
# NOTE: Signalsmith Stretch (phase vocoder) introduces artifacts for pitch-only use
# Use passthrough backend (simple resampling) for pitch-only (time ratio = 1.0)
option(USE_SIGNALSMITH "Enable Signalsmith Stretch for time-stretching" OFF)

# Core source files (portable C++)
target_sources(Op1Clone PRIVATE
    Source/Core/SamplerVoice.cpp
    Source/Core/SamplerEngine.cpp
    Source/Core/VoiceManager.cpp
    Source/Core/LockFreeMidiQueue.cpp
    Source/Core/SimplePitchShifter.cpp
    Source/Core/RingBufferF.cpp
    Source/Core/WSOLA.cpp
    Source/Core/Resampler.cpp
    Source/Core/TimePitchProcessor.cpp
    Source/Core/TimePitchError.cpp
    Source/Core/GranularTimeWarp.cpp
    Source/Core/SignalsmithTimePitch.cpp
    Source/Core/BiquadFilter.cpp
    Source/Core/MoogLadderFilter.cpp
    Source/Core/EnvelopeGenerator.cpp
    Source/Core/DriveEffect.cpp
    Source/Core/LofiEffect.cpp
    Source/Core/DSP/SignalsmithStretchWrapper.cpp
)


# JUCE wrapper source files
target_sources(Op1Clone PRIVATE
    Source/JuceWrapper/PluginProcessor.cpp
    Source/JuceWrapper/PluginProcessor.h
    Source/JuceWrapper/PluginEditor.cpp
    Source/JuceWrapper/PluginEditor.h
    Source/JuceWrapper/JuceEngineAdapter.cpp
    Source/JuceWrapper/JuceEngineAdapter.h
    Source/JuceWrapper/ScreenComponent.cpp
    Source/JuceWrapper/ScreenComponent.h
    Source/JuceWrapper/WaveformComponent.cpp
    Source/JuceWrapper/WaveformComponent.h
    Source/JuceWrapper/EncoderComponent.cpp
    Source/JuceWrapper/EncoderComponent.h
    # Source/JuceWrapper/ADSRVisualizationComponent.cpp  # COMMENTED OUT - replaced with ADSRPillComponent
    # Source/JuceWrapper/ADSRVisualizationComponent.h
    Source/JuceWrapper/ADSRPillComponent.cpp
    Source/JuceWrapper/ADSRPillComponent.h
    Source/JuceWrapper/JuceVisualizationRenderer.cpp
    Source/JuceWrapper/JuceVisualizationRenderer.h
    Source/JuceWrapper/MidiInputHandler.cpp
    Source/JuceWrapper/MidiInputHandler.h
    Source/JuceWrapper/MidiStatusComponent.cpp
    Source/JuceWrapper/MidiStatusComponent.h
    Source/JuceWrapper/EncoderSetupManager.cpp
    Source/JuceWrapper/EncoderSetupManager.h
    Source/JuceWrapper/EditorLayoutManager.cpp
    Source/JuceWrapper/EditorLayoutManager.h
    Source/JuceWrapper/EditorEventHandlers.cpp
    Source/JuceWrapper/EditorEventHandlers.h
    Source/JuceWrapper/EditorUpdateMethods.cpp
    Source/JuceWrapper/EditorUpdateMethods.h
    Source/JuceWrapper/EditorTimerCallback.cpp
    Source/JuceWrapper/EditorTimerCallback.h
    Source/JuceWrapper/InstrumentMenuComponent.cpp
    Source/JuceWrapper/InstrumentMenuComponent.h
    Source/JuceWrapper/PianoIconButton.cpp
    Source/JuceWrapper/PianoIconButton.h
    Source/JuceWrapper/SampleSlotComponent.cpp
    Source/JuceWrapper/SampleSlotComponent.h
)

# Custom standalone main file (only for standalone target)
target_sources(Op1Clone PRIVATE
    Source/JuceWrapper/StandaloneMain.cpp
)

# Include directories
target_include_directories(Op1Clone PUBLIC
    Source
    ThirdParty
    ThirdParty/signalsmith
    ThirdParty/signalsmith-linear
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

