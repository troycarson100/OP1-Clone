#include "SamplerEngine.h"
#include <algorithm>
#include <fstream>
#include <chrono>
#include <vector>
#include <atomic>  // For atomic_load_explicit/atomic_store_explicit on shared_ptr

namespace Core {

SamplerEngine::SamplerEngine()
    : currentSampleRate(44100.0)
    , currentBlockSize(512)
    , currentNumChannels(2)
    , targetGain(1.0f)
    , filterCutoffHz(1000.0f)
    , filterResonance(1.0f)
    , filterEnvAmount(0.0f)
    , filterDriveDb(0.0f)
    , loopEnvAttackMs(10.0f)
    , loopEnvReleaseMs(100.0f)
    , lofiAmount(0.0f)
    , isPolyphonic(true)
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
    lofi.prepare(sampleRate);
    
    // Set filter parameters (now that it's prepared)
    filter.setCutoff(filterCutoffHz);
    filter.setResonance(filterResonance);
    
    // Set envelope parameters (now that it's prepared) (DEPRECATED - kept for future use)
    modEnv.setAttack(loopEnvAttackMs);
    modEnv.setRelease(loopEnvReleaseMs);
    
    // Set drive (convert dB to linear gain, more aggressive mapping)
    float driveGain = 1.0f + (filterDriveDb / 24.0f) * 3.0f; // 1.0 to 4.0
    drive.setDrive(driveGain);
    
    // Set lofi
    updateLofiParameters();
    
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

void SamplerEngine::handleMidi(const MidiEvent* events, int count) {
    int previousActiveCount = activeVoiceCount;
    
    // Get current sample data snapshot (atomic, lock-free)
    SampleDataPtr currentSample = getSampleData();
    
    // Safety check: only process MIDI if we have valid sample data
    // If no sample is loaded, voices will remain inactive
    if (!currentSample || currentSample->length <= 0 || currentSample->mono.empty()) {
        // No valid sample - skip MIDI processing (voices will remain inactive)
        return;
    }
    
    for (int i = 0; i < count; ++i) {
        const MidiEvent& event = events[i];
        
        if (event.type == MidiEvent::NoteOn) {
            // Pass sample data snapshot to voice on noteOn
            voiceManager.noteOn(event.note, event.velocity, currentSample);
        } else if (event.type == MidiEvent::NoteOff) {
            voiceManager.noteOff(event.note);
        }
    }
    
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
    
    // Soft limit the mixed output to prevent crackling from clipping
    // This prevents hard clipping that causes crackling when multiple voices play
    // Use a more aggressive soft limiter with tanh for smooth limiting
    // Also apply dynamic gain reduction based on number of active voices
    int activeVoices = voiceManager.getActiveVoiceCount();
    float voiceGainReduction = 1.0f;
    if (activeVoices > 4) {
        // Reduce gain when more than 4 voices are active
        // At 8 voices: 0.7x, at 16 voices: 0.5x
        voiceGainReduction = 1.0f - ((activeVoices - 4) * 0.05f);
        voiceGainReduction = std::max(0.5f, voiceGainReduction);
    }
    
    for (int ch = 0; ch < numChannels; ++ch) {
        if (output[ch] != nullptr) {
            for (int i = 0; i < numSamples; ++i) {
                float sample = output[ch][i] * voiceGainReduction;
                // Use tanh for smooth, natural-sounding soft limiting
                // This prevents hard clipping while maintaining dynamics
                // More aggressive limiting: scale input more to prevent overflow
                sample = std::tanh(sample * 0.6f) * 0.9f; // More aggressive scaling (was 0.8f * 0.95f)
                // Hard safety clamp to prevent any sample from exceeding ±1.0 (prevents glitches)
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
    
    if (numChannels > 0 && output[0] != nullptr && currentSampleRate > 0.0 && tempBuffer != nullptr) {
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
        
        // 3. Apply lofi effect (after filter, before copying to other channels)
        if (lofiAmount > 0.001f) {
            lofi.processBlock(channelData, channelData, numSamples);
        }
        
        // Copy processed signal to other channels (mono to stereo)
        for (int ch = 1; ch < numChannels; ++ch) {
            if (output[ch] != nullptr) {
                std::copy(channelData, channelData + numSamples, output[ch]);
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

void SamplerEngine::setLofiAmount(float amount) {
    lofiAmount = std::max(0.0f, std::min(1.0f, amount));
    if (currentSampleRate > 0.0) {
        updateLofiParameters();
    }
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

void SamplerEngine::updateLofiParameters() {
    // Map lofiAmount (0-1) to bit depth (16 bits to 1 bit) and sample rate reduction (1.0 to 0.1)
    // At 0.0: 16 bits, 1.0 sample rate (no lofi)
    // At 1.0: 1 bit, 0.1 sample rate (maximum lofi)
    float bitDepth = 16.0f - (lofiAmount * 15.0f);  // 16 to 1 bits
    float sampleRateReduction = 1.0f - (lofiAmount * 0.9f);  // 1.0 to 0.1
    
    lofi.setBitDepth(bitDepth);
    lofi.setSampleRateReduction(sampleRateReduction);
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

} // namespace Core

