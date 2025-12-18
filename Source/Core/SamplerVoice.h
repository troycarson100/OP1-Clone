#pragma once

#include "ITimePitch.h"
#include <memory>

namespace Core {

// Single sampler voice - portable, no JUCE
// Plays back a loaded sample from memory with time-stretching pitch shifting
class SamplerVoice {
public:
    SamplerVoice();
    ~SamplerVoice();
    
    // Set the sample data (called from wrapper after loading)
    void setSample(const float* data, int length, double sourceSampleRate);
    
    // Set root note (MIDI note that plays at original pitch)
    void setRootNote(int rootNote) { rootMidiNote = rootNote; }
    
    // Trigger note on
    void noteOn(int note, float velocity);
    
    // Trigger note off (only if playing this note)
    void noteOff(int note);
    
    // Check if voice is active (playing, not in release)
    bool isActive() const { return active && !inRelease; }
    
    // Check if voice is playing (including release phase)
    bool isPlaying() const { return active; }
    
    // Get the MIDI note this voice is currently playing
    int getCurrentNote() const { return currentNote; }
    
    // Process audio block - writes to output buffer
    // output: non-interleaved buffer [channel][sample]
    void process(float** output, int numChannels, int numSamples, double sampleRate);
    
    // Set gain (0.0 to 1.0)
    void setGain(float gain);
    
    // Enable or disable time-warp processing
    void setWarpEnabled(bool enabled) { warpEnabled = enabled; }
    
    // ADSR envelope parameters (in milliseconds, except sustain which is 0.0-1.0)
    void setAttackTime(float attackMs) { attackTimeMs = attackMs; }
    void setDecayTime(float decayMs) { decayTimeMs = decayMs; }
    void setSustainLevel(float sustain) { sustainLevel = sustain; } // 0.0 to 1.0
    void setReleaseTime(float releaseMs) { releaseTimeMs = releaseMs; }
    
    // Get ADSR parameters (for UI display)
    float getAttackTime() const { return attackTimeMs; }
    float getDecayTime() const { return decayTimeMs; }
    float getSustainLevel() const { return sustainLevel; }
    float getReleaseTime() const { return releaseTimeMs; }
    
    // Get debug info (for UI display)
    void getDebugInfo(int& actualInN, int& outN, int& primeRemaining, int& nonZeroCount) const;
    
private:
    const float* sampleData;
    int sampleLength;
    double sourceSampleRate;
    
    double playhead;        // Current playback position (samples, can be fractional)
    bool active;
    int currentNote;
    int rootMidiNote;       // MIDI note that plays at original pitch (default 60)
    float currentVelocity;
    float gain;
    
    // ADSR envelope parameters
    float attackTimeMs;     // Attack time in milliseconds (default 2.0)
    float decayTimeMs;      // Decay time in milliseconds (default 0.0)
    float sustainLevel;     // Sustain level (0.0 to 1.0, default 1.0)
    float releaseTimeMs;    // Release time in milliseconds (default 20.0)
    
    // Envelope state
    float envelopeValue;    // Current envelope value (0.0 to 1.0)
    int attackSamples;      // Number of samples for attack phase
    int attackCounter;      // Current position in attack phase
    int decaySamples;       // Number of samples for decay phase
    int decayCounter;       // Current position in decay phase
    int releaseSamples;     // Number of samples for release phase
    int releaseCounter;     // Current position in release phase
    bool inRelease;         // True when in release phase
    double currentSampleRate; // For calculating envelope times
    
    // Time-stretching pitch processor (abstract interface)
    std::unique_ptr<ITimePitch> timePitchProcessor;
    
    // Buffers for processing
    float* inputBuffer;     // Buffer for reading from sample
    float* outputBuffer;    // Buffer for processor output
    int bufferSize;
    
    // Sample read position (advances at original speed)
    double sampleReadPos;
    
    // Enable/disable time-warp processing (when false, use simple pitch path)
    bool warpEnabled;
    
    // Latency priming state
    int primeRemainingSamples;
    
    // Processor prepared state (per-voice, not shared)
    
    // Debug info (updated during process, read from UI thread)
    mutable int lastActualInN;
    mutable int lastOutN;
    mutable int lastPrimeRemaining;
    mutable int lastNonZeroCount;
    
    // Helper: clamp value
    static float clamp(float value, float min, float max);
};

} // namespace Core

