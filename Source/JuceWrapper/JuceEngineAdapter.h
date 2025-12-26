#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "../Core/SamplerEngine.h"
#include "../Core/MidiEvent.h"
#include <vector>
#include <array>
#include <atomic>

// Thin adapter layer between JUCE and portable Core engine
// Converts JUCE types to Core types
class JuceEngineAdapter {
public:
    JuceEngineAdapter();
    ~JuceEngineAdapter();
    
    // Prepare adapter (called from PluginProcessor::prepareToPlay)
    void prepare(double sampleRate, int blockSize, int numChannels);
    
    // Load sample from JUCE AudioBuffer (called after loading file)
    // Takes ownership of the buffer data
    void setSample(juce::AudioBuffer<float>& buffer, double sourceSampleRate);
    
    // Process audio block - converts JUCE buffer to Core format
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages);
    
    // Set gain parameter
    void setGain(float gain);
    
    // Set ADSR envelope parameters
    void setADSR(float attackMs, float decayMs, float sustain, float releaseMs);
    
    // Set sample editing parameters
    void setRepitch(float semitones);
    void setStartPoint(int sampleIndex);
    void setEndPoint(int sampleIndex);
    void setSampleGain(float gain);
    
    // Get sample editing parameters
    float getRepitch() const;
    int getStartPoint() const;
    int getEndPoint() const;
    float getSampleGain() const;
    
    // Enable or disable time-warp processing
    
    // Get current gain
    float getGain() const;
    
    // Get sample data for visualization (thread-safe copy) - DEPRECATED, use getStereoSampleDataForVisualization
    void getSampleDataForVisualization(std::vector<float>& outData) const;
    
    // Get stereo sample data for visualization (thread-safe copy)
    void getStereoSampleDataForVisualization(std::vector<float>& outLeft, std::vector<float>& outRight) const;
    
    // Get stereo sample data for a specific slot (thread-safe copy)
    void getSlotStereoSampleDataForVisualization(int slotIndex, std::vector<float>& outLeft, std::vector<float>& outRight) const;
    
    // Get source sample rate (for time calculations)
    double getSourceSampleRate() const;
    
    // Get source sample rate for a specific slot (for time calculations)
    double getSlotSourceSampleRate(int slotIndex) const;
    
    // Get active slots (slots that are currently playing) - thread-safe
    std::array<bool, 5> getActiveSlots() const;
    
    // Get debug info (called from audio thread, safe to read from UI thread)
    void getDebugInfo(int& actualInN, int& outN, int& primeRemaining, int& nonZeroCount) const;
    
    // Get active voice count (for UI updates)
    int getActiveVoiceCount() const;
    
    // Get playhead position (for UI display)
    double getPlayheadPosition() const;
    
    // Get envelope value (for UI fade out)
    float getEnvelopeValue() const;
    
    // Get all active voice playhead positions and envelope values (for multi-voice visualization)
    void getAllActivePlayheads(std::vector<double>& positions, std::vector<float>& envelopeValues) const;
    
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
    
    // Set loop parameters
    void setLoopEnabled(bool enabled);
    void setLoopPoints(int startPoint, int endPoint);
    
    // Enable/disable time-warp processing
    void setWarpEnabled(bool enabled);
    
    // Set time ratio (1.0 = constant duration, != 1.0 = time stretching)
    void setTimeRatio(double ratio);
    
    // Enable/disable filter and effects processing
    void setFilterEffectsEnabled(bool enabled);
    
    // Set sample for a specific slot (0-4 for A-E)
    void setSampleForSlot(int slotIndex, juce::AudioBuffer<float>& buffer, double sourceSampleRate);
    
    // Set playback mode (0 = Stacked, 1 = Round Robin)
    void setPlaybackMode(int mode);
    
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
    
private:
    Core::SamplerEngine engine;
    
    // Pre-allocated buffers for conversion (no allocation in audio thread)
    std::vector<float*> channelPointers;
    std::vector<Core::MidiEvent> midiEventBuffer;
    
    // Sample data storage (owned by adapter) - per slot
    struct SlotSampleData {
        std::vector<float> leftChannel;
        std::vector<float> rightChannel;
        double sourceSampleRate;
        bool hasSample;
        
        SlotSampleData() : sourceSampleRate(44100.0), hasSample(false) {}
    };
    std::array<SlotSampleData, 5> slotSamples;  // 5 slots A-E
    
    // Parameter storage per slot
    struct SlotParameters {
        float repitchSemitones;
        int startPoint;
        int endPoint;
        float sampleGain;
        float attackMs;
        float decayMs;
        float sustain;
        float releaseMs;
        bool loopEnabled;
        int loopStartPoint;
        int loopEndPoint;
        
        SlotParameters()
            : repitchSemitones(0.0f)
            , startPoint(0)
            , endPoint(0)
            , sampleGain(1.0f)
            , attackMs(800.0f)
            , decayMs(0.0f)
            , sustain(1.0f)
            , releaseMs(1000.0f)
            , loopEnabled(false)
            , loopStartPoint(0)
            , loopEndPoint(0)
        {}
    };
    std::array<SlotParameters, 5> slotParameters;  // 5 slots A-E
    
    // Track which slots are currently active (playing) - updated in processBlock
    mutable std::array<std::atomic<bool>, 5> activeSlots;  // Thread-safe tracking of active slots
    
    // Legacy sample data storage (for backward compatibility)
    std::vector<float> sampleData;        // Left channel (or mono)
    std::vector<float> rightChannelData;   // Right channel (empty if mono)
    
    // Source sample rate (stored when sample is loaded)
    double sourceSampleRate;
    
    // Playback mode (0 = Stacked, 1 = Round Robin)
    int playbackMode;
    int roundRobinIndex;  // Current index for round robin cycling
    
    // Helper: convert JUCE MIDI buffer to Core MidiEvent array
    void convertMidiBuffer(juce::MidiBuffer& midiMessages, int numSamples);
};

