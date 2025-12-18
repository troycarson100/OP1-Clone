#pragma once

#include "SamplerVoice.h"
#include "MidiEvent.h"
#include <array>

namespace Core {

// Manages multiple voices for polyphonic playback
// Simple voice allocation: find free voice or steal oldest
class VoiceManager {
public:
    static constexpr int MAX_VOICES = 16;
    
    VoiceManager();
    ~VoiceManager();
    
    // Set sample for all voices
    void setSample(const float* data, int length, double sourceSampleRate);
    
    // Set root note for all voices
    void setRootNote(int rootNote);
    
    // Enable or disable time-warp processing on all voices
    void setWarpEnabled(bool enabled);
    
    // Handle note on - allocates a voice
    void noteOn(int note, float velocity);
    
    // Handle note off - releases voice playing this note
    void noteOff(int note);
    
    // Process all active voices
    void process(float** output, int numChannels, int numSamples, double sampleRate);
    
    // Set gain for all voices
    void setGain(float gain);
    
    // Set ADSR envelope parameters for all voices
    void setADSR(float attackMs, float decayMs, float sustain, float releaseMs);
    
    // Get debug info from first active voice (for UI)
    void getDebugInfo(int& actualInN, int& outN, int& primeRemaining, int& nonZeroCount) const;
    
private:
    std::array<SamplerVoice, MAX_VOICES> voices;
    int nextVoiceIndex; // For round-robin allocation
    
    // Find a free voice, or steal the oldest one
    int allocateVoice();
};

} // namespace Core

