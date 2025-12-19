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
    
    // Enable or disable time-warp processing on all voices
    void setTimeWarpEnabled(bool enabled);
    
    // Handle MIDI events (called from wrapper) - DEPRECATED, use pushMidiEvent instead
    void handleMidi(const MidiEvent* events, int count);
    
    // Push MIDI event from UI/MIDI thread (non-blocking, lock-free)
    bool pushMidiEvent(const MidiEvent& event);
    
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
    void setTimeWarpSpeed(float speed);  // 0.5x to 2.0x (1.0x = normal speed)
    
    // Set playback mode (mono or poly)
    void setPlaybackMode(bool polyphonic);  // true = poly, false = mono
    
    // Set loop parameters
    void setLoopEnabled(bool enabled);
    void setLoopPoints(int startPoint, int endPoint);
    
    // Enable/disable filter and effects processing (for testing/debugging)
    void setFilterEffectsEnabled(bool enabled);
    
    // Get playhead position (for UI display)
    double getPlayheadPosition() const;
    
    // Get envelope value (for UI fade out)
    float getEnvelopeValue() const;
    
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
    float filterResonance;
    float filterEnvAmount;  // -1.0 to 1.0 (DEPRECATED - kept for future use)
    float filterDriveDb;    // Drive in dB (0.0 to 24.0)
    
    // Loop envelope parameters (DEPRECATED - kept for future use)
    float loopEnvAttackMs;
    float loopEnvReleaseMs;
    
    // Time-warp playback speed
    float timeWarpSpeed;  // 0.5x to 2.0x (1.0x = normal speed)
    
    // Playback mode
    bool isPolyphonic;  // true = poly, false = mono
    
    // Filter/effects processing enabled flag (for testing/debugging)
    bool filterEffectsEnabled;  // true = process filter/effects, false = bypass
    
    // Temporary buffer for processing (allocated in prepare)
    float* tempBuffer;
    
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
    
    void updateActiveVoiceCount();
    void updateLofiParameters();
};

} // namespace Core

