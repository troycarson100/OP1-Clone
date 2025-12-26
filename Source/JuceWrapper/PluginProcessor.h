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
    
    // Set sample for a specific slot (0-4 for A-E)
    void setSampleForSlot(int slotIndex, juce::AudioBuffer<float>& buffer, double sourceSampleRate);
    
    // Set parameters for a specific slot (0-4 for A-E)
    void setSlotRepitch(int slotIndex, float semitones);
    void setSlotStartPoint(int slotIndex, int sampleIndex);
    void setSlotEndPoint(int slotIndex, int sampleIndex);
    void setSlotSampleGain(int slotIndex, float gain);
    void setSlotADSR(int slotIndex, float attackMs, float decayMs, float sustain, float releaseMs);
    
    // Set loop parameters for a specific slot (0-4 for A-E)
    void setSlotLoopEnabled(int slotIndex, bool enabled);
    void setSlotLoopPoints(int slotIndex, int startPoint, int endPoint);
    
    // Get parameters for a specific slot (0-4 for A-E)
    float getSlotRepitch(int slotIndex) const;
    int getSlotStartPoint(int slotIndex) const;
    int getSlotEndPoint(int slotIndex) const;
    float getSlotSampleGain(int slotIndex) const;
    
    // Get sample data for visualization (thread-safe)
    void getSampleDataForVisualization(std::vector<float>& outData) const;
    void getStereoSampleDataForVisualization(std::vector<float>& outLeft, std::vector<float>& outRight) const;
    void getSlotStereoSampleDataForVisualization(int slotIndex, std::vector<float>& outLeft, std::vector<float>& outRight) const;
    
    // Get source sample rate (for time calculations)
    double getSourceSampleRate() const;
    
    // Get source sample rate for a specific slot (for time calculations)
    double getSlotSourceSampleRate(int slotIndex) const;
    
    // Get active slots (slots that are currently playing)
    std::array<bool, 5> getActiveSlots() const;
    
    // Get active voice count (for UI updates)
    int getActiveVoiceCount() const;
    
    // Set LP filter parameters
    void setLPFilterCutoff(float cutoffHz);
    void setLPFilterResonance(float resonance);
    void setLPFilterEnvAmount(float amount);  // -1.0 to 1.0 (DEPRECATED - kept for future use)
    void setLPFilterDrive(float driveDb);     // 0.0 to 24.0 dB
    
    // Set loop envelope parameters (for filter modulation) (DEPRECATED - kept for future use)
    void setLoopEnvAttack(float attackMs);
    void setLoopEnvRelease(float releaseMs);
    
    // Set time-warp playback speed (only affects time-warped samples)
    // Time warp speed removed - fixed at 1.0 (constant duration)
    
    // Set playback mode (mono or poly)
    void setPlaybackMode(bool polyphonic);  // true = poly, false = mono
    
    // Set sample playback mode (0 = Stacked, 1 = Round Robin)
    void setPlaybackMode(int mode);  // 0 = Stacked, 1 = Round Robin
    
    // Set loop parameters
    void setLoopEnabled(bool enabled);
    void setLoopPoints(int startPoint, int endPoint);
    
    // Enable/disable time-warp processing
    void setWarpEnabled(bool enabled);
    
    // Set time ratio (1.0 = constant duration, != 1.0 = time stretching)
    void setTimeRatio(double ratio);
    
    // Enable/disable filter and effects processing
    void setFilterEffectsEnabled(bool enabled);
    
    // Get MIDI input handler (for device management and UI)
    MidiInputHandler& getMidiInputHandler() { return midiInputHandler; }
    
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
    
    // Get playhead position (for UI display)
    double getPlayheadPosition() const;
    
    // Get envelope value (for UI fade out)
    float getEnvelopeValue() const;
    
    // Get all active voice playhead positions and envelope values (for multi-voice visualization)
    void getAllActivePlayheads(std::vector<double>& positions, std::vector<float>& envelopeValues) const;
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
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Op1CloneAudioProcessor)
};

