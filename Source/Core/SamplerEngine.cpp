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
    , filterCutoffHz(20000.0f)  // Start fully open (20kHz = no filtering) so it doesn't reduce volume
    , filterCutoffTarget(20000.0f)  // Track target for smoother
    , filterResonance(1.0f)
    , lastAppliedFilterCutoff(20000.0f)  // Initialize to match filterCutoffHz
    , filterEnvAmount(0.0f)
    , filterDriveDb(0.0f)
    , loopEnvAttackMs(10.0f)
    , loopEnvReleaseMs(100.0f)
    // timeWarpSpeed removed - fixed at 1.0 (constant duration)
    , isPolyphonic(true)
    , filterEffectsEnabled(true)  // Enabled by default
    , tempBuffer(nullptr)
    , limiterGain(1.0f)
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
    
    // Configure slew limiter based on sample rate
    // maxStep = 0.02f at 44.1k (tunable, scales with sample rate)
    float slewMaxStep = 0.02f * (static_cast<float>(sampleRate) / 44100.0f);
    mixSlewLimiter.setMaxStep(slewMaxStep);
    mixSlewLimiter.reset();
    
    // Reset gain smoother with current block size
    gainSmoother.setTarget(targetGain, blockSize);
    
    // Initialize cutoff smoother
    cutoffSmoother.setValueImmediate(filterCutoffHz);
    
    // Prepare filter and effects
    filter.prepare(sampleRate);
    modEnv.prepare(sampleRate);  // DEPRECATED - kept for future use
    // lofi.prepare(sampleRate);  // DEPRECATED - lofi removed, replaced with speed knob
    
    // Set filter parameters (now that it's prepared)
    // Initialize filter to 20kHz (fully open) so it doesn't reduce volume initially
    filter.setCutoff(20000.0f);
    filter.setResonance(filterResonance);
    
    // Set envelope parameters (now that it's prepared) (DEPRECATED - kept for future use)
    modEnv.setAttack(loopEnvAttackMs);
    modEnv.setRelease(loopEnvReleaseMs);
    
    // Set filter drive (integrated into filter, not separate effect)
    float driveAmount = filterDriveDb / 24.0f;  // 0.0 to 1.0 for 0-24dB range
    driveAmount = std::max(0.0f, std::min(1.0f, driveAmount));
    filter.setDrive(driveAmount);
    
    // Time warp speed removed - fixed at 1.0 (constant duration)
    
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
    
    // Calculate polyphonic gain scaling to prevent overdrive
    // With N voices, scale each voice so N voices sum to ~0.5x total (prevents overdrive)
    // Formula: each voice = 0.5 / N, so sum = 0.5x regardless of voice count
    int activeVoiceCount = voiceManager.getActiveVoiceCount();
    float polyphonicVoiceGain = 0.5f / std::max(1.0f, static_cast<float>(activeVoiceCount));
    voiceManager.setVoiceGain(polyphonicVoiceGain);
    
    // Advance smoother (for next block)
    for (int i = 0; i < numSamples; ++i) {
        gainSmoother.getNextValue();
    }
    
    // CRITICAL: Clear output buffers at start of block
    // Mix voices by accumulation only; do not overwrite unintentionally
    for (int ch = 0; ch < numChannels; ++ch) {
        if (output[ch] != nullptr) {
            std::fill(output[ch], output[ch] + numSamples, 0.0f);
        }
    }
    
    // Process all voices
    // NOTE: No mutex lock here - voices store pointers that are only updated in setSample()
    // The mutex in setSample() ensures pointer updates are atomic, and once set, the pointers
    // remain valid until the next setSample() call. The audio thread can safely read from these
    // pointers without locking.
    voiceManager.process(output, numChannels, numSamples, currentSampleRate);
    
    // Apply slew limiter to final mix (click suppressor)
    for (int i = 0; i < numSamples; ++i) {
        float mixL = (output[0] != nullptr) ? output[0][i] : 0.0f;
        float mixR = (numChannels > 1 && output[1] != nullptr) ? output[1][i] : mixL;
        
        mixSlewLimiter.process(mixL, mixR);
        
        if (output[0] != nullptr) {
            output[0][i] = mixL;
        }
        if (numChannels > 1 && output[1] != nullptr) {
            output[1][i] = mixR;
        }
    }
    
    // Run pop detector on output (after slew limiting)
    popDetector.processBlock(output, numChannels, numSamples, popEventBuffer);
    
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
    
    // Second pass: apply soft clipping first, then limiting
    // This prevents harsh artifacts from hard clipping
    const float softClipThreshold = 0.85f; // Start soft clipping at 85%
    const float softClipRatio = 0.8f; // Tanh scaling factor
    
    for (int ch = 0; ch < numChannels; ++ch) {
        if (output[ch] != nullptr) {
            for (int i = 0; i < numSamples; ++i) {
                float sample = output[ch][i];
                
                // Safety: NaN/Inf guard
                if (!std::isfinite(sample)) {
                    sample = 0.0f;
                }
                
                // Apply soft clipping before limiting for smoother sound
                float absSample = std::abs(sample);
                if (absSample > softClipThreshold) {
                    // Smooth tanh soft clipping - starts gentle, gets more aggressive
                    float excess = absSample - softClipThreshold;
                    float softClipAmount = std::tanh(excess * softClipRatio * 3.0f);
                    float newAbs = softClipThreshold + (1.0f - softClipThreshold) * softClipAmount;
                    sample = (sample > 0.0f ? 1.0f : -1.0f) * newAbs;
                }
                
                // Store processed sample
                output[ch][i] = sample;
            }
        }
    }
    
    // Re-detect peak after soft clipping
    float peakAfterSoftClip = 0.0f;
    for (int ch = 0; ch < numChannels; ++ch) {
        if (output[ch] != nullptr) {
            for (int i = 0; i < numSamples; ++i) {
                float absSample = std::abs(output[ch][i]);
                if (absSample > peakAfterSoftClip) {
                    peakAfterSoftClip = absSample;
                }
            }
        }
    }
    
    // Apply look-ahead style limiting with smooth attack/release
    const float attackCoeff = 0.80f; // Fast attack (20% per sample)
    const float releaseCoeff = 0.998f; // Slower release for smoother recovery
    const float limitThreshold = 0.95f; // Limit to 95% for safety margin
    
    if (peakAfterSoftClip > limitThreshold) {
        float targetGain = limitThreshold / peakAfterSoftClip;
        // Fast attack for immediate response
        limiterGain = limiterGain * attackCoeff + targetGain * (1.0f - attackCoeff);
    } else {
        // Smooth release - approach 1.0 gradually
        limiterGain = limiterGain * releaseCoeff + 1.0f * (1.0f - releaseCoeff);
        limiterGain = std::min(1.0f, limiterGain);
    }
    
    // Apply limiter gain
    for (int ch = 0; ch < numChannels; ++ch) {
        if (output[ch] != nullptr) {
            for (int i = 0; i < numSamples; ++i) {
                output[ch][i] *= limiterGain;
                // Final hard clamp as absolute safety
                output[ch][i] = std::max(-1.0f, std::min(1.0f, output[ch][i]));
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
        // CRITICAL: Update filter parameters before processing to ensure they're current
        // The filter cutoff/resonance might have been changed from UI thread
        // Only update smoother target if cutoff actually changed (prevents resetting ramp every block)
        if (std::abs(filterCutoffHz - filterCutoffTarget) > 0.1f) {
            filterCutoffTarget = filterCutoffHz;
            // Use longer time-based smoothing (50ms) for smooth filter changes during rapid knob turns
            const float filterSmoothTimeMs = 50.0f; // 50ms smoothing time (increased from 20ms)
            int filterSmoothSamples = static_cast<int>(currentSampleRate * filterSmoothTimeMs / 1000.0);
            filterSmoothSamples = std::max(1, std::min(filterSmoothSamples, numSamples * 4)); // Clamp to reasonable range
            cutoffSmoother.setTarget(filterCutoffHz, filterSmoothSamples);
        }
        // Smooth cutoff per-sample to prevent clicks
        // Update filter coefficients less frequently (every block) to reduce clicks
        float currentSmoothedCutoff = cutoffSmoother.getNextValue();
        
        // Only update filter if cutoff changed significantly (prevents crackling from frequent updates)
        // Reduced threshold from 10.0 Hz to 5.0 Hz for smoother response, but still prevents excessive updates
        if (std::abs(currentSmoothedCutoff - lastAppliedFilterCutoff) > 5.0f) {
            filter.setCutoff(currentSmoothedCutoff);
            lastAppliedFilterCutoff = currentSmoothedCutoff;
        }
        filter.setResonance(filterResonance);
        
        // Set filter drive (integrated into filter, not separate processing)
        // Convert drive from dB to linear drive amount (0.0 = clean, 1.0+ = saturated)
        float driveAmount = filterDriveDb / 24.0f;  // 0.0 to 1.0 for 0-24dB range
        driveAmount = std::max(0.0f, std::min(1.0f, driveAmount));
        filter.setDrive(driveAmount);
        
        // No separate drive processing - drive is now integrated into the filter
        
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
                
                // Process sample through filter (drive is integrated, process directly from channelData)
                channelData[i] = filter.process(channelData[i]);
                
                // Advance envelope
                envValue = modEnv.process();
            }
        } else {
            // No envelope modulation - process entire block at once
            // CRITICAL: Ensure filter cutoff is updated before processing
            // The cutoff might have been changed from UI, so update it now
            // Only update smoother target if cutoff actually changed (prevents resetting ramp every block)
            if (std::abs(filterCutoffHz - filterCutoffTarget) > 0.1f) {
                filterCutoffTarget = filterCutoffHz;
                // Use longer time-based smoothing (50ms) for smooth filter changes during rapid knob turns
                const float filterSmoothTimeMs = 50.0f; // 50ms smoothing time (increased from 20ms)
                int filterSmoothSamples = static_cast<int>(currentSampleRate * filterSmoothTimeMs / 1000.0);
                filterSmoothSamples = std::max(1, std::min(filterSmoothSamples, numSamples * 4)); // Clamp to reasonable range
                cutoffSmoother.setTarget(filterCutoffHz, filterSmoothSamples);
            }
            float smoothedCutoff = cutoffSmoother.getNextValue();
            
            // Set filter drive (integrated into filter, not separate processing)
            float driveAmount = filterDriveDb / 24.0f;  // 0.0 to 1.0 for 0-24dB range
            driveAmount = std::max(0.0f, std::min(1.0f, driveAmount));
            filter.setDrive(driveAmount);
            
            // Process block - update filter cutoff only if changed significantly
            // Don't update per-sample to avoid crackling from frequent coefficient changes
            if (std::abs(smoothedCutoff - lastAppliedFilterCutoff) > 1.0f) {
                filter.setCutoff(smoothedCutoff);
                lastAppliedFilterCutoff = smoothedCutoff;
            }
            // Process directly from channelData (drive is integrated into filter)
            // Filter will handle bypass internally at high cutoffs
            filter.processBlock(channelData, channelData, numSamples);
            
            // Advance smoother for next block (but don't use the values for this block)
            for (int i = 1; i < numSamples; ++i) {
                cutoffSmoother.getNextValue();
            }
            
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
        
        // CRITICAL: Apply final limiting after filter/drive to prevent clipping
        // Filter and drive can boost the signal, so we need to limit again
        float finalPeak = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch) {
            if (output[ch] != nullptr) {
                for (int i = 0; i < numSamples; ++i) {
                    float absSample = std::abs(output[ch][i]);
                    if (absSample > finalPeak) {
                        finalPeak = absSample;
                    }
                }
            }
        }
        
        // Apply fast limiter if needed
        if (finalPeak > 0.95f) {
            float finalLimiterGain = 0.95f / finalPeak;
            for (int ch = 0; ch < numChannels; ++ch) {
                if (output[ch] != nullptr) {
                    for (int i = 0; i < numSamples; ++i) {
                        output[ch][i] *= finalLimiterGain;
                        output[ch][i] = std::max(-1.0f, std::min(1.0f, output[ch][i]));
                    }
                }
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
    float newCutoff = std::max(20.0f, std::min(20000.0f, cutoffHz));
        // Only update if cutoff actually changed (prevents unnecessary smoother resets)
        if (std::abs(newCutoff - filterCutoffHz) > 0.1f) {
            filterCutoffHz = newCutoff;
            // Only update filter if it's been prepared (sampleRate > 0)
            if (currentSampleRate > 0.0) {
                // Use longer time-based smoothing (50ms) for smooth filter changes during rapid knob turns
                const float filterSmoothTimeMs = 50.0f; // 50ms smoothing time (increased from 20ms)
                int filterSmoothSamples = static_cast<int>(currentSampleRate * filterSmoothTimeMs / 1000.0);
                filterSmoothSamples = std::max(1, std::min(filterSmoothSamples, currentBlockSize * 4)); // Clamp to reasonable range
                cutoffSmoother.setTarget(filterCutoffHz, filterSmoothSamples);
            }
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
    
    // Update filter drive (integrated into filter, not separate effect)
    if (currentSampleRate > 0.0) {
        // Convert dB to linear drive amount (0.0 = clean, 1.0 = saturated at 24dB)
        float driveAmount = filterDriveDb / 24.0f;
        driveAmount = std::max(0.0f, std::min(1.0f, driveAmount));
        filter.setDrive(driveAmount);
    }
}

// Time warp speed removed - fixed at 1.0 (constant duration)

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

void SamplerEngine::setWarpEnabled(bool enabled) {
    voiceManager.setWarpEnabled(enabled);
}

void SamplerEngine::setTimeRatio(double ratio) {
    voiceManager.setTimeRatio(ratio);
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

