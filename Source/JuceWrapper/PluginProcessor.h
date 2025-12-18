#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "JuceEngineAdapter.h"
#include "MidiInputHandler.h"
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
    
    // Send MIDI note from keyboard (thread-safe)
    void sendMidiNote(int note, float velocity, bool noteOn);
    
    // Load sample from file (called from UI thread)
    bool loadSampleFromFile(const juce::File& file);
    
    // Get sample data for visualization (thread-safe)
    void getSampleDataForVisualization(std::vector<float>& outData) const;
    
    // Get MIDI input handler (for device management and UI)
    MidiInputHandler& getMidiInputHandler() { return midiInputHandler; }
    
    // Enable or disable time-warp processing (called from UI)
    void setTimeWarpEnabled(bool enabled);
    
    // Set ADSR envelope parameters (called from UI)
    void setADSR(float attackMs, float decayMs, float sustain, float releaseMs);
    
    // Set sample editing parameters (called from UI)
    void setRepitch(float semitones);
    void setStartPoint(int sampleIndex);
    void setEndPoint(int sampleIndex);
    void setSampleGain(float gain);
    
    // Get sample editing parameters (for UI display)
    float getRepitch() const;
    int getStartPoint() const;
    int getEndPoint() const;
    float getSampleGain() const;
    
    // Debug info (updated from audio thread, read from UI thread)
    std::atomic<int> debugLastActualInN{0};
    std::atomic<int> debugLastOutN{0};
    std::atomic<int> debugLastPrimeRemaining{0};
    std::atomic<int> debugLastNonZeroOutCount{0};

private:
    JuceEngineAdapter adapter;
    juce::AudioProcessorValueTreeState parameters;
    
    // MIDI input handler for standalone app (lock-free FIFO)
    MidiInputHandler midiInputHandler;
    
    // Thread-safe MIDI injection for test button (lock-free)
    juce::AbstractFifo midiFifo;
    std::array<juce::MidiMessage, 32> midiMessageBuffer;
    std::atomic<int> pendingMidiCount{0};
    
    // Sample loading helper
    void loadDefaultSample();
    
    // Time-warp enable flag (atomic, read on audio thread)
    std::atomic<bool> timeWarpEnabled { true };
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Op1CloneAudioProcessor)
};

