#pragma once

#include "VoiceManager.h"
#include "MidiEvent.h"
#include "LockFreeMidiQueue.h"
#include "LinearSmoother.h"
#include "MoogLadderFilter.h"
#include "EnvelopeGenerator.h"
#include "DriveEffect.h"
#include "LofiEffect.h"
#include "SampleData.h"
#include "PopDetector.h"
#include <memory>
#include <atomic>

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
    
    // DEPRECATED: setSample() removed - use setSampleData() instead
    // This method is disabled to prevent raw pointer usage
    // void setSample(const float* data, int length, double sourceSampleRate); // DELETED
    
    // Set sample data (thread-safe, lock-free)
    // UI thread: creates SampleData and atomically swaps it in
    void setSampleData(SampleDataPtr sampleData) noexcept;
    
    // Get current sample data (thread-safe, lock-free)
    // Audio thread: safely reads current sample without blocking
    SampleDataPtr getSampleData() const noexcept;
    
    // Set root note (MIDI note that plays at original pitch, default 60)
    void setRootNote(int rootNote);
    
    
    // Handle MIDI events (called from wrapper) - DEPRECATED, use pushMidiEvent instead
    void handleMidi(const MidiEvent* events, int count);
    
    // Push MIDI event from UI/MIDI thread (non-blocking, lock-free)
    bool pushMidiEvent(const MidiEvent& event);
    
    // Trigger note on immediately with specific sample data (for stacked playback)
    // This bypasses the queue and processes the note immediately with the given sample
    bool triggerNoteOnWithSample(int note, float velocity, SampleDataPtr sampleData);
    
    // Trigger note on with sample data and slot-specific parameters
    // Applies parameters to the allocated voice, not globally
    bool triggerNoteOnWithSample(int note, float velocity, SampleDataPtr sampleData,
                                 float repitchSemitones, int startPoint, int endPoint, float sampleGain,
                                 float attackMs, float decayMs, float sustain, float releaseMs,
                                 bool loopEnabled, int loopStartPoint, int loopEndPoint);
    
    // Process audio block
    // output: non-interleaved buffer [channel][sample]
    void process(float** output, int numChannels, int numSamples);
    
    // Set gain parameter (0.0 to 1.0)
    void setGain(float gain);
    
    // Set ADSR envelope parameters (in milliseconds, except sustain which is 0.0-1.0)
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
    
    // Get debug info (for UI display)
    void getDebugInfo(int& actualInN, int& outN, int& primeRemaining, int& nonZeroCount) const;
    
    // DEBUG: Enable/disable sine test mode on all voices
    void setSineTestEnabled(bool enabled);
    
    // Set LP filter parameters
    void setLPFilterCutoff(float cutoffHz);
    void setLPFilterResonance(float resonance);
    void setLPFilterEnvAmount(float amount);  // -1.0 to 1.0 (DEPRECATED - kept for future use)
    void setLPFilterDrive(float driveDb);    // Drive in dB (0.0 to 24.0 dB)
    
    // Set loop envelope parameters (for filter modulation) (DEPRECATED - kept for future use)
    void setLoopEnvAttack(float attackMs);
    void setLoopEnvRelease(float releaseMs);
    
    // Set time-warp playback speed (only affects time-warped samples)
    
    // Set playback mode (mono or poly)
    void setPlaybackMode(bool polyphonic);  // true = poly, false = mono
    
    // Set loop parameters
    void setLoopEnabled(bool enabled);
    void setLoopPoints(int startPoint, int endPoint);
    
    // Enable/disable time-warp processing
    void setWarpEnabled(bool enabled);
    
    // Set time ratio (1.0 = constant duration, != 1.0 = time stretching)
    void setTimeRatio(double ratio);
    
    // Enable/disable filter and effects processing (for testing/debugging)
    void setFilterEffectsEnabled(bool enabled);
    
    // Get playhead position (for UI display)
    double getPlayheadPosition() const;
    
    // Get envelope value (for UI fade out)
    float getEnvelopeValue() const;
    
    // Get all active voice playhead positions and envelope values (for multi-voice visualization)
    void getAllActivePlayheads(std::vector<double>& positions, std::vector<float>& envelopeValues) const;
    
    // Get instrumentation metrics (thread-safe, atomic reads)
    float getBlockPeak() const { return blockPeak.load(std::memory_order_acquire); }
    int getClippedSamples() const { return clippedSamples.load(std::memory_order_acquire); }
    int getActiveVoicesCount() const { return activeVoicesCount.load(std::memory_order_acquire); }
    int getVoicesStartedThisBlock() const { return voicesStartedThisBlock.load(std::memory_order_acquire); }
    int getVoicesStolenThisBlock() const { return voicesStolenThisBlock.load(std::memory_order_acquire); }
    bool getXrunsOrOverruns() const { return xrunsOrOverruns.load(std::memory_order_acquire); }
    
private:
    VoiceManager voiceManager;
    LinearSmoother gainSmoother;
    LinearSmoother cutoffSmoother;  // Smooth cutoff changes to prevent instability
    float lastAppliedFilterCutoff;  // Track last applied cutoff to avoid frequent updates
    double currentSampleRate;
    int currentBlockSize;
    int currentNumChannels;
    float targetGain;
    
    // Filter and effects
    MoogLadderFilter filter;
    EnvelopeGenerator modEnv;  // DEPRECATED - kept for future use
    DriveEffect drive;
    LofiEffect lofi;
    
    // Filter parameters
    float filterCutoffHz;
    float filterCutoffTarget;  // Track target for smoother (only update when changed)
    float filterResonance;
    float filterEnvAmount;  // -1.0 to 1.0 (DEPRECATED - kept for future use)
    float filterDriveDb;    // Drive in dB (0.0 to 24.0)
    
    // Loop envelope parameters (DEPRECATED - kept for future use)
    float loopEnvAttackMs;
    float loopEnvReleaseMs;
    
    // timeWarpSpeed removed - fixed at 1.0 (constant duration)
    
    // Playback mode
    bool isPolyphonic;  // true = poly, false = mono
    
    // Filter/effects processing enabled flag (for testing/debugging)
    bool filterEffectsEnabled;  // true = process filter/effects, false = bypass
    
    // Temporary buffer for processing (allocated in prepare)
    float* tempBuffer;
    
    // Limiter state (per-instance, not static)
    float limiterGain;
    
    // Track active voices for envelope triggering
    int activeVoiceCount;
    
    // Current sample data (plain shared_ptr with atomic operations)
    // UI thread writes via atomic_store_explicit, audio thread reads via atomic_load_explicit
    // This is the standard-safe way to atomically swap shared_ptr (C++11 compatible)
    mutable SampleDataPtr currentSample_;
    
    // Lock-free MIDI event queue (UI thread pushes, audio thread pops)
    LockFreeMidiQueue midiQueue;
    
    // Instrumentation metrics (atomic, updated in audio thread, read from UI thread)
    mutable std::atomic<float> blockPeak{0.0f};
    mutable std::atomic<int> clippedSamples{0};
    mutable std::atomic<int> activeVoicesCount{0};
    mutable std::atomic<int> voicesStartedThisBlock{0};
    mutable std::atomic<int> voicesStolenThisBlock{0};
    mutable std::atomic<bool> xrunsOrOverruns{false};
    
    // Pop detection
    PopDetector popDetector;
    PopEventRingBuffer popEventBuffer;
    
    // Slew limiter for final mix (click suppressor)
    SlewLimiter mixSlewLimiter;
    
    // Block boundary smoothing (prevents clicks between blocks)
    float lastBlockSampleL;
    float lastBlockSampleR;
    
    // Get pop events (UI thread)
    int getPopEvents(PopEvent* out, int maxCount) {
        return popEventBuffer.read(out, maxCount);
    }
    
    // Set pop detection threshold
    void setPopThreshold(float threshold) {
        popDetector.setThreshold(threshold);
    }
    
    // Set slew limiter max step
    void setSlewMaxStep(float maxStep) {
        mixSlewLimiter.setMaxStep(maxStep);
    }
    
    void updateActiveVoiceCount();
    void updateLofiParameters();
};

} // namespace Core

