#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "../Core/SamplerEngine.h"
#include "../Core/MidiEvent.h"
#include <vector>

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
    void setTimeWarpEnabled(bool enabled);
    
    // Get current gain
    float getGain() const;
    
    // Get sample data for visualization (thread-safe copy)
    void getSampleDataForVisualization(std::vector<float>& outData) const;
    
    // Get source sample rate (for time calculations)
    double getSourceSampleRate() const;
    
    // Get debug info (called from audio thread, safe to read from UI thread)
    void getDebugInfo(int& actualInN, int& outN, int& primeRemaining, int& nonZeroCount) const;
    
    // Set LP filter parameters
    void setLPFilterCutoff(float cutoffHz);
    void setLPFilterResonance(float resonance);
    void setLPFilterEnvAmount(float amount);  // -1.0 to 1.0 (DEPRECATED - kept for future use)
    void setLPFilterDrive(float driveDb);     // 0.0 to 24.0 dB
    
    // Set loop envelope parameters (for filter modulation) (DEPRECATED - kept for future use)
    void setLoopEnvAttack(float attackMs);
    void setLoopEnvRelease(float releaseMs);
    
    // Set lofi effect parameters
    void setLofiAmount(float amount);  // 0.0 to 1.0
    
    // Set playback mode (mono or poly)
    void setPlaybackMode(bool polyphonic);  // true = poly, false = mono
    
    // Set loop parameters
    void setLoopEnabled(bool enabled);
    void setLoopPoints(int startPoint, int endPoint);
    
private:
    Core::SamplerEngine engine;
    
    // Pre-allocated buffers for conversion (no allocation in audio thread)
    std::vector<float*> channelPointers;
    std::vector<Core::MidiEvent> midiEventBuffer;
    
    // Sample data storage (owned by adapter)
    std::vector<float> sampleData;
    
    // Source sample rate (stored when sample is loaded)
    double sourceSampleRate;
    
    // Helper: convert JUCE MIDI buffer to Core MidiEvent array
    void convertMidiBuffer(juce::MidiBuffer& midiMessages, int numSamples);
};

