#include "SamplerEngine.h"
#include "LockFreeMidiQueue.h"
#include <algorithm>
#include <fstream>
#include <chrono>
#include <vector>
#include <atomic>  // For atomic_load_explicit/atomic_store_explicit on shared_ptr
#include <cmath>   // For std::isfinite

namespace Core {

SamplerEngine::SamplerEngine()
    : currentSampleRate(44100.0)
    , currentBlockSize(512)
    , currentNumChannels(2)
    , targetGain(1.0f)
    , filterCutoffHz(20000.0f)  // Start fully open (no filtering)
    , filterResonance(1.0f)
    , filterEnvAmount(0.0f)
    , filterDriveDb(0.0f)
    , loopEnvAttackMs(10.0f)
    , loopEnvReleaseMs(100.0f)
    , timeWarpSpeed(1.0f)  // Default to normal speed (1.0x)
    , isPolyphonic(true)
    , filterEffectsEnabled(true)  // Enabled by default
    , tempBuffer(nullptr)
    , activeVoiceCount(0)
    , currentSample_(nullptr)
{
}

SamplerEngine::~SamplerEngine() {
    delete[] tempBuffer;
}

void SamplerEngine::prepare(double sampleRate, int blockSize, int numChannels) {
    currentSampleRate = sampleRate;
    currentBlockSize = blockSize;
    currentNumChannels = numChannels;
    
    // Reset gain smoother with current block size
    gainSmoother.setTarget(targetGain, blockSize);
    
    // Initialize cutoff smoother
    cutoffSmoother.setValueImmediate(filterCutoffHz);
    
    // Prepare filter and effects
    filter.prepare(sampleRate);
    modEnv.prepare(sampleRate);  // DEPRECATED - kept for future use
    // lofi.prepare(sampleRate);  // DEPRECATED - lofi removed, replaced with speed knob
    
    // Set filter parameters (now that it's prepared)
    // Initialize filter to fully open (20kHz) so it doesn't cut signal
    filter.setCutoff(20000.0f);
    filter.setResonance(filterResonance);
    
    // Set envelope parameters (now that it's prepared) (DEPRECATED - kept for future use)
    modEnv.setAttack(loopEnvAttackMs);
    modEnv.setRelease(loopEnvReleaseMs);
    
    // Set drive (convert dB to linear gain, more aggressive mapping)
    float driveGain = 1.0f + (filterDriveDb / 24.0f) * 3.0f; // 1.0 to 4.0
    drive.setDrive(driveGain);
    
    // Set time-warp speed (default 1.0x = normal speed)
    voiceManager.setTimeWarpSpeed(timeWarpSpeed);
    
    // Allocate temporary buffer for processing (max block size)
    delete[] tempBuffer;
    tempBuffer = new float[static_cast<size_t>(blockSize)];
}

SampleDataPtr SamplerEngine::getSampleData() const noexcept {
    // Atomic load on plain shared_ptr (standard-safe, C++11 compatible)
    // Uses free function atomic_load_explicit with acquire semantics
    return std::atomic_load_explicit(&currentSample_, std::memory_order_acquire);
}

void SamplerEngine::setSampleData(SampleDataPtr sampleData) noexcept {
    // Atomic store on plain shared_ptr (standard-safe, C++11 compatible)
    // Uses free function atomic_store_explicit with release semantics
    // UI thread swaps in new sample, audio thread continues with old sample until next noteOn
    std::atomic_store_explicit(&currentSample_, std::move(sampleData), std::memory_order_release);
}

// DEPRECATED: setSample() removed - use setSampleData() instead
// This prevents raw pointer usage and ensures thread safety
// void SamplerEngine::setSample(const float* data, int length, double sourceSampleRate) {
//     // DELETED - do not accept raw pointers
//     // All sample loading must use setSampleData(SampleDataPtr) only
// }

void SamplerEngine::setRootNote(int rootNote) {
    voiceManager.setRootNote(rootNote);
}

void SamplerEngine::setTimeWarpEnabled(bool enabled) {
    voiceManager.setWarpEnabled(enabled);
}

bool SamplerEngine::pushMidiEvent(const MidiEvent& event) {
    // Push event to lock-free queue (UI/MIDI thread)
    return midiQueue.push(event);
}

void SamplerEngine::handleMidi(const MidiEvent* events, int count) {
    // DEPRECATED: Push events to queue instead of processing directly
    // This maintains backward compatibility but routes through queue
    for (int i = 0; i < count; ++i) {
        pushMidiEvent(events[i]);
    }
    
    // Legacy code kept for reference but now uses queue:
    int previousActiveCount = activeVoiceCount;
    
    // Update active voice count and trigger/release envelope
    updateActiveVoiceCount();
    
    // Trigger envelope on note-on (when voices become active)
    if (previousActiveCount == 0 && activeVoiceCount > 0) {
        modEnv.trigger();
    }
    
    // Release envelope when all voices stop
    if (previousActiveCount > 0 && activeVoiceCount == 0) {
        modEnv.release();
    }
}

void SamplerEngine::process(float** output, int numChannels, int numSamples) {
    // Safety check: ensure we have valid output buffers
    if (output == nullptr || numChannels <= 0 || numSamples <= 0) {
        return;
    }
    
    // Reset instrumentation for this block
    voicesStartedThisBlock.store(0, std::memory_order_relaxed);
    voicesStolenThisBlock.store(0, std::memory_order_relaxed);
    
    // Process MIDI events from lock-free queue (audio thread only)
    MidiEvent event;
    int voicesStarted = 0;
    int voicesStolen = 0;
    SampleDataPtr currentSample = getSampleData();
    
    while (midiQueue.pop(event)) {
        if (event.type == MidiEvent::NoteOn && currentSample && currentSample->length > 0) {
            bool wasStolen = false;
            // Pass 0 as startDelayOffset - VoiceManager will calculate the delay based on voicesStartedThisBlock
            bool started = voiceManager.noteOn(event.note, event.velocity, currentSample, wasStolen, 0);
            if (started) {
                voicesStarted++;
                if (wasStolen) {
                    voicesStolen++;
                }
            }
        } else if (event.type == MidiEvent::NoteOff) {
            voiceManager.noteOff(event.note);
        }
    }
    
    voicesStartedThisBlock.store(voicesStarted, std::memory_order_release);
    voicesStolenThisBlock.store(voicesStolen, std::memory_order_release);
    
    // Clear output buffer
    for (int ch = 0; ch < numChannels; ++ch) {
        if (output[ch] != nullptr) {
            for (int i = 0; i < numSamples; ++i) {
                output[ch][i] = 0.0f;
            }
        }
    }
    
    // Update gain - use current smoothed value
    // For simplicity, we set gain once per block (smoother transitions across blocks)
    float currentGain = gainSmoother.getCurrentValue();
    voiceManager.setGain(currentGain);
    
    // Advance smoother (for next block)
    for (int i = 0; i < numSamples; ++i) {
        gainSmoother.getNextValue();
    }
    
    // Process all voices
    // NOTE: No mutex lock here - voices store pointers that are only updated in setSample()
    // The mutex in setSample() ensures pointer updates are atomic, and once set, the pointers
    // remain valid until the next setSample() call. The audio thread can safely read from these
    // pointers without locking.
    voiceManager.process(output, numChannels, numSamples, currentSampleRate);
    
    // Update active voices count
    int activeVoices = voiceManager.getActiveVoiceCount();
    activeVoicesCount.store(activeVoices, std::memory_order_release);
    
    // First pass: detect peak and count clipped samples BEFORE any gain adjustment
    float peak = 0.0f;
    int clipped = 0;
    
    for (int ch = 0; ch < numChannels; ++ch) {
        if (output[ch] != nullptr) {
            for (int i = 0; i < numSamples; ++i) {
                float absSample = std::abs(output[ch][i]);
                if (absSample > 1.0f) {
                    clipped++;
                }
                if (absSample > peak) {
                    peak = absSample;
                }
            }
        }
    }
    
    // Update instrumentation
    blockPeak.store(peak, std::memory_order_release);
    clippedSamples.store(clipped, std::memory_order_release);
    
    // Second pass: apply very aggressive limiting with smooth attack/release to prevent clicks
    static float limiterGain = 1.0f;
    const float attackCoeff = 0.85f; // Very fast attack (15% per sample) for immediate response
    const float releaseCoeff = 0.995f; // Faster release (0.5% per sample) for better responsiveness
    
    // Very aggressive peak-based limiting - kick in earlier (above 0.6) to prevent any clipping
    if (peak > 0.6f) {
        float targetGain = 0.6f / peak; // Target 0.6 for maximum headroom and prevent clipping
        // Very fast attack for immediate response
        limiterGain = limiterGain * attackCoeff + targetGain * (1.0f - attackCoeff);
    } else {
        // Faster release - approach 1.0 more quickly
        limiterGain = limiterGain * releaseCoeff + (1.0f - releaseCoeff);
        limiterGain = std::min(1.0f, limiterGain);
    }
    
    for (int ch = 0; ch < numChannels; ++ch) {
        if (output[ch] != nullptr) {
            for (int i = 0; i < numSamples; ++i) {
                float sample = output[ch][i] * limiterGain;
                
                // Soft clipping to prevent hard clipping artifacts
                if (!std::isfinite(sample)) {
                    sample = 0.0f;
                }
                // Soft clip using tanh for smooth saturation
                if (std::abs(sample) > 0.9f) {
                    sample = std::tanh(sample * 0.7f) * 1.0f; // Soft clip above 0.9
                }
                // Final hard clamp as safety
                sample = std::max(-1.0f, std::min(1.0f, sample));
                output[ch][i] = sample;
            }
        }
    }
    
    // Apply filter and effects (global processing on mixed output)
    // Only process if engine is properly prepared and buffers are valid
    // Skip filter processing entirely if not prepared (safe fallback)
    // NOTE: We skip filter processing if tempBuffer is null (prepare() not called yet)
    // This is safe - audio will just pass through without filtering
    
    // #region agent log
    {
        std::ofstream log("/Users/troycarson/Documents/JUCE Projects/OP1-Clone/.cursor/debug.log", std::ios::app);
        if (log.is_open()) {
            log << "{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"H2\",\"location\":\"SamplerEngine.cpp:126\",\"message\":\"filter processing check\",\"data\":{\"numChannels\":" << numChannels << ",\"output0Null\":" << (output[0] == nullptr ? 1 : 0) << ",\"currentSampleRate\":" << currentSampleRate << ",\"tempBufferNull\":" << (tempBuffer == nullptr ? 1 : 0) << "},\"timestamp\":" << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() << "}\n";
        }
    }
    // #endregion
    
    // Apply filter and effects only if enabled
    if (filterEffectsEnabled && numChannels > 0 && output[0] != nullptr && currentSampleRate > 0.0 && tempBuffer != nullptr) {
        // Process first channel (mono filter for now, can be extended to stereo)
        float* channelData = output[0];
        
        // Ensure tempBuffer is large enough (shouldn't happen if prepare was called correctly)
        if (numSamples > currentBlockSize) {
            // Buffer too small - skip filter processing for this block
            // Just copy to other channels and return
            for (int ch = 1; ch < numChannels; ++ch) {
                if (output[ch] != nullptr) {
                    std::copy(channelData, channelData + numSamples, output[ch]);
                }
            }
            return;
        }
        
        // 1. Apply drive effect
        drive.processBlock(channelData, tempBuffer, numSamples);
        
        // 2. Process envelope (for modulation) - only if envelope is prepared
        float envValue = modEnv.getCurrentValue();
        
        // Only update filter if envelope amount is non-zero
        if (std::abs(filterEnvAmount) > 0.001f) {
            // Calculate base modulation range
            float modulationRange = filterCutoffHz * 0.5f; // Modulate up to 50% of cutoff
            
            // Process per-sample with envelope modulation
            // Update filter coefficients less frequently to prevent instability
            float currentFilterCutoff = filterCutoffHz;
            const int updateInterval = 8; // Update filter every 8 samples
            int samplesSinceUpdate = 0;
            
            for (int i = 0; i < numSamples; ++i) {
                // Calculate target modulated cutoff
                // Env amount: -1.0 to 1.0
                // Positive = envelope opens filter, negative = envelope closes filter
                float targetCutoff = filterCutoffHz + (filterEnvAmount * envValue * modulationRange);
                targetCutoff = std::max(20.0f, std::min(20000.0f, targetCutoff));
                
                // Update filter coefficients periodically (not every sample)
                // This prevents excessive recalculations and instability
                if (samplesSinceUpdate >= updateInterval) {
                    // Smooth the cutoff change
                    cutoffSmoother.setTarget(targetCutoff, updateInterval * 2);
                    currentFilterCutoff = cutoffSmoother.getNextValue();
                    filter.setCutoff(currentFilterCutoff);
                    samplesSinceUpdate = 0;
                } else {
                    // Advance smoother even when not updating filter
                    cutoffSmoother.getNextValue();
                    samplesSinceUpdate++;
                }
                
                // Process sample through filter (uses current filter coefficients)
                channelData[i] = filter.process(tempBuffer[i]);
                
                // Advance envelope
                envValue = modEnv.process();
            }
        } else {
            // No envelope modulation - process entire block at once
            filter.processBlock(tempBuffer, channelData, numSamples);
            
            // Still advance envelope (even if not used)
            for (int i = 0; i < numSamples; ++i) {
                envValue = modEnv.process();
            }
        }
        
        // 3. Lofi effect removed - replaced with speed knob
        // (lofi processing code kept commented for potential future use)
        
        // Copy processed signal to other channels (mono to stereo)
        for (int ch = 1; ch < numChannels; ++ch) {
            if (output[ch] != nullptr) {
                std::copy(channelData, channelData + numSamples, output[ch]);
            }
        }
    } else {
        // Filter/effects disabled - just copy first channel to other channels without processing
        if (numChannels > 0 && output[0] != nullptr) {
            for (int ch = 1; ch < numChannels; ++ch) {
                if (output[ch] != nullptr) {
                    std::copy(output[0], output[0] + numSamples, output[ch]);
                }
            }
        }
    }
}

void SamplerEngine::setGain(float gain) {
    targetGain = std::max(0.0f, std::min(1.0f, gain));
    gainSmoother.setTarget(targetGain, currentBlockSize);
}

void SamplerEngine::setADSR(float attackMs, float decayMs, float sustain, float releaseMs) {
    voiceManager.setADSR(attackMs, decayMs, sustain, releaseMs);
}

void SamplerEngine::setRepitch(float semitones) {
    voiceManager.setRepitch(semitones);
}

void SamplerEngine::setStartPoint(int sampleIndex) {
    voiceManager.setStartPoint(sampleIndex);
}

void SamplerEngine::setEndPoint(int sampleIndex) {
    voiceManager.setEndPoint(sampleIndex);
}

void SamplerEngine::setSampleGain(float gain) {
    voiceManager.setSampleGain(gain);
}

float SamplerEngine::getRepitch() const {
    return voiceManager.getRepitch();
}

int SamplerEngine::getStartPoint() const {
    return voiceManager.getStartPoint();
}

int SamplerEngine::getEndPoint() const {
    return voiceManager.getEndPoint();
}

float SamplerEngine::getSampleGain() const {
    return voiceManager.getSampleGain();
}

void SamplerEngine::getDebugInfo(int& actualInN, int& outN, int& primeRemaining, int& nonZeroCount) const {
    voiceManager.getDebugInfo(actualInN, outN, primeRemaining, nonZeroCount);
}

void SamplerEngine::updateActiveVoiceCount() {
    activeVoiceCount = voiceManager.getActiveVoiceCount();
}

double SamplerEngine::getPlayheadPosition() const {
    return voiceManager.getPlayheadPosition();
}

float SamplerEngine::getEnvelopeValue() const {
    return voiceManager.getEnvelopeValue();
}

void SamplerEngine::setLPFilterCutoff(float cutoffHz) {
    filterCutoffHz = std::max(20.0f, std::min(20000.0f, cutoffHz));
    // Only update filter if it's been prepared (sampleRate > 0)
    if (currentSampleRate > 0.0) {
        filter.setCutoff(filterCutoffHz);
        // Update smoother target (will smooth over next block)
        cutoffSmoother.setTarget(filterCutoffHz, currentBlockSize);
    }
}

void SamplerEngine::setLPFilterResonance(float resonance) {
    filterResonance = std::max(0.0f, std::min(4.0f, resonance));
    // Only update filter if it's been prepared (sampleRate > 0)
    if (currentSampleRate > 0.0) {
        filter.setResonance(filterResonance);
    }
}

void SamplerEngine::setLPFilterEnvAmount(float amount) {
    filterEnvAmount = std::max(-1.0f, std::min(1.0f, amount));
}

void SamplerEngine::setLPFilterDrive(float driveDb) {
    filterDriveDb = std::max(0.0f, std::min(24.0f, driveDb));
    // Convert dB to linear gain, but make it more aggressive
    // At 0 dB: drive = 1.0 (no effect)
    // At 24 dB: drive = 4.0 (strong saturation)
    float driveGain = 1.0f + (filterDriveDb / 24.0f) * 3.0f; // 1.0 to 4.0
    if (currentSampleRate > 0.0) {
        drive.setDrive(driveGain);
    }
}

void SamplerEngine::setTimeWarpSpeed(float speed) {
    timeWarpSpeed = std::max(0.5f, std::min(2.0f, speed));
    voiceManager.setTimeWarpSpeed(timeWarpSpeed);
}

void SamplerEngine::setPlaybackMode(bool polyphonic) {
    isPolyphonic = polyphonic;
    voiceManager.setPolyphonic(polyphonic);
}

void SamplerEngine::setLoopEnabled(bool enabled) {
    voiceManager.setLoopEnabled(enabled);
}

void SamplerEngine::setLoopPoints(int startPoint, int endPoint) {
    voiceManager.setLoopPoints(startPoint, endPoint);
}

void SamplerEngine::setFilterEffectsEnabled(bool enabled) {
    filterEffectsEnabled = enabled;
}

void SamplerEngine::updateLofiParameters() {
    // Map lofiAmount (0-1) to bit depth (16 bits to 1 bit) and sample rate reduction (1.0 to 0.1)
    // At 0.0: 16 bits, 1.0 sample rate (no lofi)
    // At 1.0: 1 bit, 0.1 sample rate (maximum lofi)
    // DEPRECATED: Lofi effect removed - replaced with speed knob
    // float bitDepth = 16.0f - (lofiAmount * 15.0f);  // 16 to 1 bits
    // float sampleRateReduction = 1.0f - (lofiAmount * 0.9f);  // 1.0 to 0.1
    // lofi.setBitDepth(bitDepth);
    // lofi.setSampleRateReduction(sampleRateReduction);
}

void SamplerEngine::setLoopEnvAttack(float attackMs) {
    loopEnvAttackMs = std::max(0.0f, attackMs);
    // Only update envelope if it's been prepared (sampleRate > 0)
    if (currentSampleRate > 0.0) {
        modEnv.setAttack(loopEnvAttackMs);
    }
}

void SamplerEngine::setLoopEnvRelease(float releaseMs) {
    loopEnvReleaseMs = std::max(0.0f, releaseMs);
    // Only update envelope if it's been prepared (sampleRate > 0)
    if (currentSampleRate > 0.0) {
        modEnv.setRelease(loopEnvReleaseMs);
    }
}

void SamplerEngine::setSineTestEnabled(bool enabled) {
    voiceManager.setSineTestEnabled(enabled);
}

} // namespace Core

