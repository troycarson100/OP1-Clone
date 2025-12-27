#pragma once

#include "SampleData.h"
#include "PopDetector.h"
#include "DSP/IWarpProcessor.h"
#include <memory>
#include <atomic>
#include <cmath>

namespace Core {

// Single sampler voice - portable, no JUCE
// Plays back a loaded sample from memory with time-stretching pitch shifting
class SamplerVoice {
public:
    SamplerVoice();
    ~SamplerVoice();
    
    // DEPRECATED: setSample() removed - voices now capture sample on noteOn only
    // This method is disabled to prevent raw pointer usage
    // void setSample(const float* data, int length, double sourceSampleRate); // DELETED
    
    // Set sample data from shared_ptr (for noteOn snapshot)
    void setSampleData(SampleDataPtr sampleData);
    
    // Set root note (MIDI note that plays at original pitch)
    void setRootNote(int rootNote) { rootMidiNote = rootNote; }
    
    // Trigger note on
    void noteOn(int note, float velocity);
    
    // Trigger note on with start delay offset (for staggering voice starts)
    void noteOn(int note, float velocity, int startDelayOffset);
    
    // Trigger note off (only if playing this note)
    void noteOff(int note);
    
    // Start voice steal fade-out (called when voice is being stolen)
    void startStealFadeOut();
    
    // Check if voice is active (playing, not in release)
    bool isActive() const { return active && !inRelease; }
    
    // Check if voice is playing (including release phase)
    bool isPlaying() const { return active; }
    
    // Get the MIDI note this voice is currently playing
    int getCurrentNote() const { return currentNote; }
    
    // Process audio block - writes to output buffer
    // output: non-interleaved buffer [channel][sample]
    void process(float** output, int numChannels, int numSamples, double sampleRate);
    
    // Process with pop detection and slew limiting
    void processWithPopDetection(float** output, int numChannels, int numSamples, double sampleRate,
                                 PopEventRingBuffer& popBuffer, uint64_t globalFrameCounter,
                                 float popThreshold, float slewMaxStep);
    
    // Set gain (0.0 to 1.0)
    void setGain(float gain);
    
    // Set per-voice gain (for polyphonic scaling)
    void setVoiceGain(float gain);
    
    // Enable or disable time-warp processing
    void setWarpEnabled(bool enabled);
    
    // Set time ratio (1.0 = constant duration, != 1.0 = time stretching)
    void setTimeRatio(double ratio);
    
    // DEBUG: Enable sine test mode (outputs 220Hz sine instead of sample data)
    void setSineTestEnabled(bool enabled) { sineTestEnabled = enabled; }
    
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
    
    // Sample editing parameters
    void setRepitch(float semitones) { repitchSemitones = semitones; } // Pitch offset in semitones (-12 to +12)
    void setStartPoint(int sampleIndex) { startPoint = sampleIndex; } // Start playback from this sample
    void setEndPoint(int sampleIndex) { endPoint = sampleIndex; } // End playback at this sample
    void setSampleGain(float gain) { sampleGain = clamp(gain, 0.0f, 2.0f); } // Sample gain (0.0 to 2.0)
    
    // Loop parameters
    void setLoopEnabled(bool enabled) { loopEnabled = enabled; }
    void setLoopPoints(int start, int end) { loopStartPoint = start; loopEndPoint = end; }
    
    // Get sample editing parameters
    float getRepitch() const { return repitchSemitones; }
    int getStartPoint() const { return startPoint; }
    int getEndPoint() const { return endPoint; }
    float getSampleGain() const { return sampleGain; }
    
    // Get current playback position (for UI display)
    double getPlayhead() const { return playhead; }
    
    // Get envelope value (for UI fade out)
    float getEnvelopeValue() const { return envelopeValue; }
    
    // Check if in release phase (for UI fade out)
    bool isInRelease() const { return inRelease; }
    
private:
    // Sample data (immutable, shared ownership)
    // Captured on noteOn, remains valid until voice releases it
    SampleDataPtr sampleData_;
    
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
    float releaseStartValue; // Envelope value when release started (for smooth release)
    float retriggerOldEnvelope; // Old envelope value when retriggering (for smooth crossfade)
    bool inRelease;         // True when in release phase
    double currentSampleRate; // For calculating envelope times
    
    // Sample read position (advances at original speed)
    double sampleReadPos;
    
    // Time-stretching processor
    std::unique_ptr<IWarpProcessor> warpProcessor;
    bool warpEnabled;
    double timeRatio;  // 1.0 = constant duration, != 1.0 = time stretch
    
    // Buffers for warp processing (planar format)
    float** warpInputPlanar;   // [2][maxBlockSize] for feeding warp
    float** warpOutputPlanar;  // [2][maxBlockSize] for warp output
    int warpBufferSize;
    
    // Warp priming and crossfade state
    bool warpPriming;
    bool warpCrossfadeActive;
    float warpCrossfadePos;     // 0.0 = dry, 1.0 = warp
    float warpCrossfadeInc;     // Crossfade increment per sample
    int warpCrossfadeSamples;   // Total crossfade duration in samples
    int warpCrossfadeCounter;   // Current crossfade position
    
    // Gain matching state (warp vs dry reference)
    float dryRMS;           // Running RMS of dry path (smoothed)
    float warpRMS;          // Running RMS of warp path (smoothed)
    float gainMatch;        // Current gain match factor (dryRMS/warpRMS)
    float gainMatchTarget;  // Target gain match (smoothed)
    float gainMatchAttack;  // Attack coefficient (5ms)
    float gainMatchRelease; // Release coefficient (100ms)
    
    // DEBUG: Sine test mode (outputs 220Hz sine instead of sample data)
    bool sineTestEnabled;
    
    // Latency priming state
    int primeRemainingSamples;
    
    // Processor prepared state (per-voice, not shared)
    
    // Debug info (updated during process, read from UI thread)
    mutable int lastActualInN;
    mutable int lastOutN;
    mutable int lastPrimeRemaining;
    mutable int lastNonZeroCount;
    
    // Sample editing parameters
    float repitchSemitones;  // Pitch offset in semitones (default 0.0)
    int startPoint;          // Start playback from this sample (default 0)
    int endPoint;            // End playback at this sample (default sampleData_->length)
    float sampleGain;        // Sample gain multiplier (default 1.0)
    
    // Loop parameters
    bool loopEnabled;        // Loop on/off (default false)
    int loopStartPoint;      // Loop start point (default 0)
    int loopEndPoint;        // Loop end point (default 0)
    
    // Loop crossfade state (for smooth loop transitions)
    bool loopCrossfadeActive;  // True when crossfading at loop boundary
    int loopCrossfadeSamples; // Crossfade duration in samples (e.g., 512 samples ~11.6ms at 44.1k)
    int loopCrossfadeCounter; // Current position in crossfade (0 to loopCrossfadeSamples)
    float loopFadeOutGain;    // Fade-out gain (1.0 -> 0.0) for end of loop
    float loopFadeInGain;     // Fade-in gain (0.0 -> 1.0) for start of loop
    
    // Per-voice gain (for gain staging)
    float voiceGain;         // Per-voice gain (default 0.2 for polyphony)
    
    // De-click ramp gain (linear ramps for smooth transitions)
    float rampGain;          // Current ramp gain (0.0 to 1.0)
    float targetGain;        // Target ramp gain
    float rampIncrement;     // Ramp increment per sample
    int rampSamplesRemaining; // Samples remaining in ramp
    bool isRamping;           // True when ramping
    
    // Voice stealing state
    bool isBeingStolen;      // True when voice is being stolen (fading out)
    
    // PART 1: Per-voice safety ramp (separate from ADSR, always applied)
    // This is a dedicated anti-click ramp that multiplies the FINAL voice output
    enum class SafetyRampState {
        RampOff,   // Ramp complete (at 1.0 for RampIn, at 0.0 for RampOut)
        RampIn,    // Fading in (0.0 -> 1.0)
        RampOut    // Fading out (1.0 -> 0.0)
    };
    SafetyRampState safetyRampState;  // Current ramp state
    float safetyRampValue;            // Current ramp value (0.0 to 1.0)
    float safetyRampStep;             // Ramp step per sample
    int safetyRampSamples;            // Total ramp duration in samples (5-10ms)
    
    // Micro fade state (for click removal) - DEPRECATED, using rampGain instead
    int fadeInSamples;       // Fade-in duration (128 samples)
    int fadeOutSamples;      // Fade-out duration (512 samples)
    int fadeInCounter;       // Current fade-in position
    int fadeOutCounter;      // Current fade-out position
    bool isFadingIn;         // True during fade-in
    
    // Pop detection per voice
    float lastVoiceSampleL;  // Last sample produced (for discontinuity detection)
    float lastVoiceSampleR;
    float maxVoiceDelta;     // Max delta in current block
    int voiceId;             // Unique voice ID for pop events
    uint32_t voiceFlags;     // Flags for pop events (nanGuard, oobClamp, etc.)
    
    // Slew limiter per voice (click suppressor)
    float slewLastOutL;
    float slewLastOutR;
    bool isFadingOut;        // True during fade-out
    
    // Voice start delay (for staggering starts within audio block)
    int startDelaySamples;   // Delay before voice starts outputting (0-63 samples)
    int startDelayCounter;   // Current delay counter
    
    // Peak measurement (atomic, for UI display)
    mutable std::atomic<float> peakOut{0.0f};
    mutable std::atomic<int> numClippedSamples{0};
    mutable std::atomic<int> oobGuardHits{0}; // Out-of-bounds guard hits (debug counter)
    
    float lastLimiterGain;  // Last limiter gain for smoothing
    
    // Anti-aliasing lowpass (for pitch up)
    float antiAliasState;    // One-pole lowpass state
    float antiAliasAlpha;    // One-pole coefficient
    
    // DC blocking filter (high-pass at ~10Hz)
    float dcBlockState;      // DC blocker state
    float dcBlockAlpha;       // DC blocker coefficient
    
    // Dithering state
    unsigned int ditherSeed;  // PRNG seed for dithering
    
    // DEBUG: Sine test phase (for 220Hz sine generation)
    double sinePhase;
    
    // Cubic Hermite interpolation helper
    static float cubicHermite(float y0, float y1, float y2, float y3, float t);
    
    // Safety processing functions
    inline float softClip(float x) const {
        // Classic cubic soft clip
        if (x <= -1.0f) return -2.0f/3.0f;
        if (x >=  1.0f) return  2.0f/3.0f;
        return x - (x*x*x)/3.0f;
    }
    
    inline float safetyProcess(float x) const {
        // NaN/Inf guard
        if (!std::isfinite(x)) x = 0.0f;
        // Hard clamp to prevent explosion
        x = std::max(-2.0f, std::min(2.0f, x));
        // Soft clip
        return softClip(x);
    }
    
    void updateAntiAliasFilter(float pitchRatio);
    
    // Helper: clamp value
    static float clamp(float value, float min, float max);
};

} // namespace Core

