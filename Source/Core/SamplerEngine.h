#pragma once

#include "VoiceManager.h"
#include "MidiEvent.h"
#include "LinearSmoother.h"

namespace Core {

// Main sampler engine - portable, no JUCE
// Owns voices and processes audio/MIDI
class SamplerEngine {
public:
    SamplerEngine();
    ~SamplerEngine();
    
    // Prepare engine for audio processing
    // Called once at startup or when sample rate/block size changes
    void prepare(double sampleRate, int blockSize, int numChannels);
    
    // Set the sample to play (called from wrapper after loading)
    void setSample(const float* data, int length, double sourceSampleRate);
    
    // Set root note (MIDI note that plays at original pitch, default 60)
    void setRootNote(int rootNote);
    
    // Enable or disable time-warp processing on all voices
    void setTimeWarpEnabled(bool enabled);
    
    // Handle MIDI events (called from wrapper)
    void handleMidi(const MidiEvent* events, int count);
    
    // Process audio block
    // output: non-interleaved buffer [channel][sample]
    void process(float** output, int numChannels, int numSamples);
    
    // Set gain parameter (0.0 to 1.0)
    void setGain(float gain);
    
    // Set ADSR envelope parameters (in milliseconds, except sustain which is 0.0-1.0)
    void setADSR(float attackMs, float decayMs, float sustain, float releaseMs);
    
    // Get debug info (for UI display)
    void getDebugInfo(int& actualInN, int& outN, int& primeRemaining, int& nonZeroCount) const;
    
private:
    VoiceManager voiceManager;
    LinearSmoother gainSmoother;
    double currentSampleRate;
    int currentBlockSize;
    int currentNumChannels;
    float targetGain;
};

} // namespace Core

