#pragma once

#include "SamplerVoice.h"
#include "MidiEvent.h"
#include "SampleData.h"
#include <array>

namespace Core {

// Manages multiple voices for polyphonic playback
// Simple voice allocation: find free voice or steal oldest
class VoiceManager {
public:
    static constexpr int MAX_VOICES = 16;
    
    VoiceManager();
    ~VoiceManager();
    
    // DEPRECATED: setSample() removed - voices now capture sample on noteOn only
    // This method is disabled to prevent raw pointer usage
    // void setSample(const float* data, int length, double sourceSampleRate); // DELETED
    
    // Set root note for all voices
    void setRootNote(int rootNote);
    
    // Enable or disable time-warp processing on all voices
    void setWarpEnabled(bool enabled);
    
    // Handle note on - allocates a voice
    void noteOn(int note, float velocity);
    
    // Handle note on with sample data snapshot (thread-safe)
    void noteOn(int note, float velocity, SampleDataPtr sampleData);
    
    // Handle note off - releases voice playing this note
    void noteOff(int note);
    
    // Process all active voices
    void process(float** output, int numChannels, int numSamples, double sampleRate);
    
    // Set gain for all voices
    void setGain(float gain);
    
    // Set ADSR envelope parameters for all voices
    void setADSR(float attackMs, float decayMs, float sustain, float releaseMs);
    
    // Set sample editing parameters for all voices
    void setRepitch(float semitones);
    void setStartPoint(int sampleIndex);
    void setEndPoint(int sampleIndex);
    void setSampleGain(float gain);
    
    // Set loop parameters for all voices
    void setLoopEnabled(bool enabled);
    void setLoopPoints(int startPoint, int endPoint);
    
    // Get sample editing parameters (from first voice)
    float getRepitch() const;
    int getStartPoint() const;
    int getEndPoint() const;
    float getSampleGain() const;
    
    // Get debug info from first active voice (for UI)
    void getDebugInfo(int& actualInN, int& outN, int& primeRemaining, int& nonZeroCount) const;
    
    // Get count of active voices (for envelope triggering)
    int getActiveVoiceCount() const;
    
    // Set playback mode (mono or poly)
    void setPolyphonic(bool polyphonic);  // true = poly, false = mono
    
    // Get playhead position from the most recently triggered voice (for UI display)
    // Returns -1 if no active voice
    double getPlayheadPosition() const;
    
    // Get envelope value from the most recently triggered voice (for UI fade out)
    // Returns 0.0 if no active voice
    float getEnvelopeValue() const;
    
private:
    std::array<SamplerVoice, MAX_VOICES> voices;
    int nextVoiceIndex; // For round-robin allocation
    bool isPolyphonicMode; // true = poly, false = mono
    
    // Find a free voice, or steal the oldest one
    int allocateVoice();
};

} // namespace Core

