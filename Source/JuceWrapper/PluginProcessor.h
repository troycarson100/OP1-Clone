#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "JuceEngineAdapter.h"
#include <vector>
#include <array>
#include <atomic>

// JUCE AudioProcessor - wrapper around portable Core engine
class Op1CloneAudioProcessor : public juce::AudioProcessor {
public:
    Op1CloneAudioProcessor();
    ~Op1CloneAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // Parameter access
    juce::AudioProcessorValueTreeState& getParameters() { return parameters; }
    
    // Test method to trigger a note (for UI testing)
    // Thread-safe: queues MIDI message for next audio block
    void triggerTestNote();
    
    // Load sample from file (called from UI thread)
    bool loadSampleFromFile(const juce::File& file);

private:
    JuceEngineAdapter adapter;
    juce::AudioProcessorValueTreeState parameters;
    
    // Thread-safe MIDI injection for test button (lock-free)
    juce::AbstractFifo midiFifo;
    std::array<juce::MidiMessage, 32> midiMessageBuffer;
    std::atomic<int> pendingMidiCount{0};
    
    // Sample loading helper
    void loadDefaultSample();
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Op1CloneAudioProcessor)
};

