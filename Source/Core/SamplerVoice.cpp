#include "SamplerVoice.h"
#include <atomic>
#include <cmath>
#include "SignalsmithTimePitch.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <chrono>
#include <limits>

namespace Core {

SamplerVoice::SamplerVoice()
    : playhead(0.0)
    , active(false)
    , currentNote(60)
    , rootMidiNote(60)
    , currentVelocity(1.0f)
    , gain(1.0f)
    , attackTimeMs(2.0f)
    , decayTimeMs(0.0f)
    , sustainLevel(1.0f)
    , releaseTimeMs(20.0f)
    , envelopeValue(0.0f)
    , attackSamples(0)
    , attackCounter(0)
    , decaySamples(0)
    , decayCounter(0)
    , releaseSamples(0)
    , releaseCounter(0)
    , releaseStartValue(0.0f)
    , retriggerOldEnvelope(0.0f)
    , inRelease(false)
    , currentSampleRate(44100.0)
    , inputBuffer(nullptr)
    , outputBuffer(nullptr)
    , bufferSize(0)
    , sampleReadPos(0.0)
    , warpEnabled(true)
    , timeWarpSpeed(1.0f)  // Default to normal speed (1.0x)
    , sineTestEnabled(false)
    , sinePhase(0.0)
    , primeRemainingSamples(0)
    , loopEnabled(false)
    , loopStartPoint(0)
    , loopEndPoint(0)
    , voiceGain(1.0f)  // Full voice gain for better volume
    , rampGain(0.0f)
    , targetGain(0.0f)
    , rampIncrement(0.0f)
    , rampSamplesRemaining(0)
    , isRamping(false)
    , isBeingStolen(false)
    , fadeInSamples(512)
    , fadeOutSamples(4096)
    , fadeInCounter(0)
    , fadeOutCounter(0)
    , isFadingIn(false)
    , isFadingOut(false)
    , startDelaySamples(0)
    , startDelayCounter(0)
    , antiAliasState(0.0f)
    , antiAliasAlpha(0.0f)
    , dcBlockState(0.0f)
    , dcBlockAlpha(0.0f)
    , ditherSeed(1)
    , lastActualInN(0)
    , lastOutN(0)
    , lastPrimeRemaining(0)
    , lastNonZeroCount(0)
    , repitchSemitones(0.0f)
    , startPoint(0)
    , endPoint(0)
    , sampleGain(1.0f)
{
    // Create SignalsmithTimePitch instance
    timePitchProcessor = std::make_unique<SignalsmithTimePitch>();
}

SamplerVoice::~SamplerVoice() {
    // Sample data is owned by caller, we don't delete it
    delete[] inputBuffer;
    delete[] outputBuffer;
}

void SamplerVoice::setSampleData(SampleDataPtr sampleData) {
    // Capture sample data snapshot (thread-safe, immutable)
    sampleData_ = sampleData;
    
    if (sampleData_ && sampleData_->length > 0) {
    // Initialize start/end points to full sample
    startPoint = 0;
        endPoint = sampleData_->length;
    } else {
        startPoint = 0;
        endPoint = 0;
    }
}

// DEPRECATED: setSample() removed - voices now capture sample on noteOn only
// This prevents raw pointer usage and ensures thread safety
// void SamplerVoice::setSample(const float* data, int length, double sourceRate) {
//     // DELETED - do not accept raw pointers
//     // Voices must only receive SampleDataPtr via setSampleData() on noteOn
// }

void SamplerVoice::noteOn(int note, float velocity) {
    noteOn(note, velocity, 0);
}

void SamplerVoice::noteOn(int note, float velocity, int startDelayOffset) {
    // #region agent log
    {
        std::ofstream log("/Users/troycarson/Documents/JUCE Projects/OP1-Clone/.cursor/debug.log", std::ios::app);
        if (log.is_open()) {
            log << "{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"A\",\"location\":\"SamplerVoice.cpp:60\",\"message\":\"noteOn called\",\"data\":{\"note\":" << note << ",\"velocity\":" << velocity << ",\"wasActive\":" << (active ? 1 : 0) << ",\"warpEnabled\":" << (warpEnabled ? 1 : 0) << "},\"timestamp\":" << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() << "}\n";
        }
    }
    // #endregion
    
    currentNote = note;
    currentVelocity = clamp(velocity, 0.0f, 1.0f);
    
    bool wasActive = active;
    bool wasSameNote = (wasActive && currentNote == note);
    
    // ALWAYS reset playhead to start point for true retrigger behavior
    // This ensures rapid key presses restart the sample from the beginning
    playhead = static_cast<double>(startPoint); // Always start from start point
    sampleReadPos = static_cast<double>(startPoint); // Always start from start point
    
    active = (sampleData_ != nullptr && sampleData_->length > 0 && !sampleData_->mono.empty());
    
    // Start ramp gain: 0 -> 1 over 64 samples (very short for rapid triggers)
    // CRITICAL: If voice was being stolen, wait until fade-out completes
    if (active && currentSampleRate > 0.0) {
        // If voice was being stolen and still fading out, don't start new note yet
        if (isBeingStolen && rampGain > 0.0f) {
            // Voice is still fading out from steal - wait until it completes
            // The fade-out will continue, and we'll start the new note when rampGain reaches 0
            // For now, just clear the stealing flag and let fade-out complete
            // The new note will start in the next process() call when rampGain <= 0
            return; // Don't start new note yet
        }
        
        rampGain = 0.0f; // Always start from 0, even if voice was stolen
        targetGain = 1.0f;
        rampSamplesRemaining = 64; // Very short fade-in for rapid triggers
        rampIncrement = 1.0f / 64.0f;
        isRamping = true;
        isBeingStolen = false;
    }
    
    // Calculate pitch in semitones and set it (include repitch offset)
    if (warpEnabled && timePitchProcessor && active) {
        int semitones = currentNote - rootMidiNote;
        float totalSemitones = static_cast<float>(semitones) + repitchSemitones;
        
        timePitchProcessor->setPitchSemitones(totalSemitones);
        // Use timeWarpSpeed to control playback speed (0.5x to 2.0x)
        // 1.0x = normal speed, < 1.0x = slower, > 1.0x = faster
        timePitchProcessor->setTimeRatio(timeWarpSpeed);
        
        // Only reset/prime when voice transitions from inactive to active (new voice start)
        // Do NOT reset on every note press - this clears analysis history
        if (!wasActive) {
            timePitchProcessor->reset();
            // Set up latency priming only on voice start
            primeRemainingSamples = timePitchProcessor->getInputLatency();
            
            // #region agent log
            {
                std::ofstream log("/Users/troycarson/Documents/JUCE Projects/OP1-Clone/.cursor/debug.log", std::ios::app);
                if (log.is_open()) {
                    log << "{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"A\",\"location\":\"SamplerVoice.cpp:85\",\"message\":\"Voice reset and primed\",\"data\":{\"primeRemainingSamples\":" << primeRemainingSamples << ",\"inputLatency\":" << timePitchProcessor->getInputLatency() << "},\"timestamp\":" << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() << "}\n";
                }
            }
            // #endregion
        }
        // If wasActive, just update pitch - no reset, no re-priming
    }
    
    // Reset ADSR envelope - always start from 0.0 for true retrigger behavior
    // This ensures rapid key presses always restart the envelope from the beginning
    envelopeValue = 0.0f;
    retriggerOldEnvelope = 0.0f; // No old envelope for retrigger
    attackCounter = 0;
    decayCounter = 0;
    inRelease = false;
    releaseCounter = 0;
    releaseStartValue = 0.0f;
    
    // Reset debug counters
    oobGuardHits.store(0, std::memory_order_relaxed);
    numClippedSamples.store(0, std::memory_order_relaxed);
    peakOut.store(0.0f, std::memory_order_relaxed);
    
    // Calculate sample counts from ADSR parameters
    // Ensure minimum attack time for smoothness (even if user sets 0ms, use at least 1ms)
    float effectiveAttackMs = std::max(attackTimeMs, 1.0f);
    attackSamples = static_cast<int>(currentSampleRate * effectiveAttackMs / 1000.0);
    if (attackSamples < 1) attackSamples = 1;
    
    decaySamples = static_cast<int>(currentSampleRate * decayTimeMs / 1000.0);
    if (decaySamples < 1) decaySamples = 1;
    
    // Ensure minimum release time for smoothness (even if user sets 0ms, use at least 1ms)
    float effectiveReleaseMs = std::max(releaseTimeMs, 1.0f);
    releaseSamples = static_cast<int>(currentSampleRate * effectiveReleaseMs / 1000.0);
    if (releaseSamples < 1) releaseSamples = 1;
    
    // Reset micro fade state - CRITICAL for smooth retriggering
    fadeInCounter = 0;
    fadeOutCounter = 0;
    // CRITICAL: When retriggering same note, don't fade-in (playhead isn't reset, so no discontinuity)
    // Only fade-in for new voices or different notes
    isFadingIn = true;  // Always start fade-in for true retrigger behavior
    isFadingOut = false;
    
    // Calculate fade durations - longer for smoother transitions, especially on retrigger
    fadeInSamples = 128;  // ~2.9ms at 44.1kHz - longer ramp-in to prevent clicks on rapid triggers
    fadeOutSamples = 4096; // ~92.9ms at 44.1kHz - longer for smoother release
    
    // Set start delay for staggering voice starts within audio block
    startDelaySamples = startDelayOffset;
    startDelayCounter = 0;
    
    // CRITICAL: If retriggering an active voice, ensure smooth transition
    // Reset any ongoing fade-out to prevent clicks
    if (wasActive && isFadingOut) {
        isFadingOut = false;
        fadeOutCounter = 0;
    }
    
    // Reset anti-aliasing filter
    antiAliasState = 0.0f;
    
    // Reset DC blocking filter
    dcBlockState = 0.0f;
    // Calculate DC blocker coefficient (high-pass at ~10Hz)
    if (currentSampleRate > 0.0) {
        float cutoffHz = 10.0f;
        float rc = 1.0f / (2.0f * 3.14159265f * cutoffHz);
        float dt = 1.0f / static_cast<float>(currentSampleRate);
        dcBlockAlpha = dt / (rc + dt);
    } else {
        dcBlockAlpha = 0.0f;
    }
    
    // Initialize dither seed
    ditherSeed = static_cast<unsigned int>(std::chrono::steady_clock::now().time_since_epoch().count()) ^ (static_cast<unsigned int>(note) << 16);
    
    // Calculate pitch ratio for anti-aliasing
    if (warpEnabled && timePitchProcessor && active) {
        int semitones = currentNote - rootMidiNote;
        float totalSemitones = static_cast<float>(semitones) + repitchSemitones;
        float pitchRatio = std::pow(2.0f, totalSemitones / 12.0f);
        updateAntiAliasFilter(pitchRatio);
    } else {
        updateAntiAliasFilter(1.0f);
    }
}

void SamplerVoice::noteOff(int note) {
    // Only start release if this voice is playing the specified note
    if (currentNote == note && active && !inRelease) {
        // Start release phase instead of immediately stopping
        // Store current envelope value as starting point for release BEFORE setting inRelease
        releaseStartValue = envelopeValue;
        
        // Recalculate release samples in case release time parameter was changed
        // Ensure minimum release time for smoothness (even if user sets 0ms, use at least 1ms)
        float effectiveReleaseMs = std::max(releaseTimeMs, 1.0f);
        releaseSamples = static_cast<int>(currentSampleRate * effectiveReleaseMs / 1000.0);
        if (releaseSamples < 1) releaseSamples = 1;
        
        inRelease = true;
        releaseCounter = 0;
        // (envelopeValue will fade from releaseStartValue to 0)
        
        // Start fade-out ramp: current rampGain -> 0 over 512 samples
        if (currentSampleRate > 0.0) {
            targetGain = 0.0f;
            rampSamplesRemaining = 512;
            rampIncrement = -rampGain / 512.0f; // Negative increment to fade out
            isRamping = true;
        }
        
        // Start fade-out (legacy, for compatibility)
        // CRITICAL: Set fade-out duration to match release envelope duration
        // This ensures the fade-out doesn't override the ADSR release envelope
        fadeOutSamples = releaseSamples; // Match release envelope duration
        isFadingOut = true;
        fadeOutCounter = 0;
        isFadingIn = false;  // Stop fade-in if still active
        
        // CRITICAL: If we're in a loop range when release is triggered, 
        // immediately advance past loop end to prevent any further looping
        if (loopEnabled && loopEndPoint > loopStartPoint) {
            if (sampleReadPos >= static_cast<double>(loopStartPoint) && sampleReadPos < static_cast<double>(loopEndPoint)) {
                // We're in the loop range - jump to just past loop end to exit loop immediately
                sampleReadPos = static_cast<double>(loopEndPoint);
            }
            // Also check playhead for simple pitch path
            if (playhead >= static_cast<double>(loopStartPoint) && playhead < static_cast<double>(loopEndPoint)) {
                playhead = static_cast<double>(loopEndPoint);
            }
        }
    }
}

// Start voice steal fade-out (called when voice is being stolen)
void SamplerVoice::startStealFadeOut() {
    isBeingStolen = true;
    targetGain = 0.0f;
    rampSamplesRemaining = 256; // Short fade-out for stealing
    rampIncrement = -rampGain / 256.0f;
    isRamping = true;
}

void SamplerVoice::process(float** output, int numChannels, int numSamples, double sampleRate) {
    // #region agent log
    {
        std::ofstream log("/Users/troycarson/Documents/JUCE Projects/OP1-Clone/.cursor/debug.log", std::ios::app);
        if (log.is_open()) {
            log << "{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"H1,H4\",\"location\":\"SamplerVoice.cpp:189\",\"message\":\"process entry\",\"data\":{\"active\":" << (active ? 1 : 0) << ",\"hasSampleData_\":" << (sampleData_ != nullptr ? 1 : 0) << ",\"length\":" << (sampleData_ ? sampleData_->length : 0) << ",\"outputPtr\":" << (void*)output << "},\"timestamp\":" << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() << "}\n";
        }
    }
    // #endregion
    
    // Validate sample data - comprehensive safety checks
    if (!active || !sampleData_ || sampleData_->length <= 0 || 
        sampleData_->mono.empty() || sampleData_->mono.data() == nullptr || 
        output == nullptr) {
        // #region agent log
        {
            std::ofstream log("/Users/troycarson/Documents/JUCE Projects/OP1-Clone/.cursor/debug.log", std::ios::app);
            if (log.is_open()) {
                log << "{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"H4\",\"location\":\"SamplerVoice.cpp:198\",\"message\":\"process early return\",\"data\":{\"active\":" << (active ? 1 : 0) << ",\"hasSampleData_\":" << (sampleData_ != nullptr ? 1 : 0) << ",\"length\":" << (sampleData_ ? sampleData_->length : 0) << ",\"empty\":" << (sampleData_ && sampleData_->mono.empty() ? 1 : 0) << "},\"timestamp\":" << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() << "}\n";
            }
        }
        // #endregion
        return;
    }
    
    // Store local copies for safe access throughout the function
    // Vector won't reallocate, pointer stays valid for the duration of this call
    // Additional validation: ensure data pointer is valid
    const float* data = sampleData_->mono.data();
    const int len = sampleData_->length;
    const double sourceSampleRate = sampleData_->sourceSampleRate;
    
    // Final safety check: ensure data pointer is valid and length matches vector size
    if (!data || len <= 0 || len != static_cast<int>(sampleData_->mono.size())) {
        // #region agent log
        {
            std::ofstream log("/Users/troycarson/Documents/JUCE Projects/OP1-Clone/.cursor/debug.log", std::ios::app);
            if (log.is_open()) {
                log << "{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"H4\",\"location\":\"SamplerVoice.cpp:210\",\"message\":\"data validation failed\",\"data\":{\"dataNull\":" << (data == nullptr ? 1 : 0) << ",\"len\":" << len << ",\"vectorSize\":" << sampleData_->mono.size() << "},\"timestamp\":" << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() << "}\n";
            }
        }
        // #endregion
        return;
    }
    
    // Update sample rate if changed
    bool sampleRateChanged = (std::abs(currentSampleRate - sampleRate) > 0.1);
    currentSampleRate = sampleRate;
    
    // Prepare time-pitch processor if needed (first time or sample rate changed)
    // CRITICAL FIX: Use isPrepared() instead of static flag - each voice has its own processor
    if (warpEnabled && timePitchProcessor) {
        if (sampleRateChanged || !timePitchProcessor->isPrepared()) {
            TimePitchConfig config;
            config.channels = 1; // Mono
            config.sampleRate = sampleRate;
            config.maxBlockSize = numSamples;
            timePitchProcessor->prepare(config);
            // Get latency AFTER prepare (prepare() calls reset() which might clear it)
            int latency = timePitchProcessor->getInputLatency();
            // Reset priming when preparing - use actual latency
            primeRemainingSamples = latency;
            
            // #region agent log
            {
                std::ofstream log("/Users/troycarson/Documents/JUCE Projects/OP1-Clone/.cursor/debug.log", std::ios::app);
                if (log.is_open()) {
                    log << "{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"A\",\"location\":\"SamplerVoice.cpp:131\",\"message\":\"Processor prepared\",\"data\":{\"inputLatency\":" << latency << ",\"primeRemainingSamples\":" << primeRemainingSamples << ",\"sampleRate\":" << sampleRate << "},\"timestamp\":" << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() << "}\n";
                }
            }
            // #endregion
        }
    }
    
    // Base amplitude (velocity * gain) with scaling to prevent clipping
        // Scale per-voice more aggressively to prevent clipping when multiple voices play
        // Use 1/(MAX_VOICES * 1.2) for extra headroom: 1 voice = 0.833, 6 voices = 0.139 each
        // This ensures 6 voices sum to ~0.83, leaving headroom for processing
        const float amplitudeScale = 1.0f / (static_cast<float>(6) * 1.2f); // Scale based on MAX_VOICES (6) with extra headroom
    float baseAmplitude = currentVelocity * gain * amplitudeScale;
    
    if (warpEnabled) {
        // --- Time-warp path: read at original speed into buffer, then warp ---
        // Ensure buffers are allocated
        if (bufferSize < numSamples) {
            delete[] inputBuffer;
            delete[] outputBuffer;
            bufferSize = numSamples;
            inputBuffer = new float[static_cast<size_t>(bufferSize)];
            outputBuffer = new float[static_cast<size_t>(bufferSize)];
        }
        
        // For constant duration: read sample at 1x speed, use pitch-only
        // timeRatio is already set to 1.0 in noteOn()
        double speed = sourceSampleRate / sampleRate;
        
        // Guard against invalid speed
        if (!std::isfinite(speed) || speed <= 0.0) {
            speed = 1.0;
        }
        
        // Handle latency priming: pre-fill with actual sample data instead of zeros
        // This reduces perceived latency by giving the processor real audio from the start
        int samplesToPrime = 0;
        if (primeRemainingSamples > 0) {
            samplesToPrime = std::min(primeRemainingSamples, numSamples);
            primeRemainingSamples -= samplesToPrime;
        }
        
        // Read from sample at original speed (1x) - this ensures constant duration
        std::fill(inputBuffer, inputBuffer + numSamples, 0.0f);
        
        // Fill input buffer: during priming, read forward from startPoint
        // After priming, continue from sampleReadPos (which we'll advance after priming)
        for (int i = 0; i < numSamples; ++i) {
            // Check if voice should start outputting (staggered start)
            if (startDelayCounter < startDelaySamples) {
                startDelayCounter++;
                // Skip processing for this sample during delay period
                // We'll output silence by not adding to output buffer
                continue;
            }
            
            // Calculate the read position
            double readPos;
            if (i < samplesToPrime) {
                // Priming phase: read forward from startPoint
                // We'll advance sampleReadPos after priming to skip the primed portion
                readPos = static_cast<double>(startPoint) + static_cast<double>(i);
                // Clamp to valid range
                if (readPos >= static_cast<double>(len)) {
                    readPos = static_cast<double>(len - 1);
                }
            } else {
                // Normal phase: read from current sampleReadPos
                readPos = sampleReadPos;
            }
            
            if (i < samplesToPrime) {
                // Priming phase: feed zeros (processor needs to accumulate input before producing output)
                inputBuffer[i] = 0.0f;
                // Don't advance sampleReadPos during priming
            } else {
                // Normal phase: read from sample at 1x speed (respecting start/end points)
                // Bounds-safe interpolation
                if (sampleReadPos >= static_cast<double>(endPoint - 1)) {
                    // At or past last valid index - use last sample value
                    int lastIdx = std::max(startPoint, endPoint - 1);
                    if (lastIdx >= 0 && lastIdx < len) {
                        inputBuffer[i] = data[lastIdx] * sampleGain;
                    } else {
                    inputBuffer[i] = 0.0f;
                    }
                } else if (sampleReadPos < static_cast<double>(startPoint)) {
                    inputBuffer[i] = 0.0f;
                } else {
                    // Safe interpolation: idx1 is guaranteed < endPoint (which is <= len)
                    int idx0 = static_cast<int>(sampleReadPos);
                    int idx1 = idx0 + 1;
                    
                    // Clamp to valid range
                    if (idx0 < startPoint) idx0 = startPoint;
                    if (idx0 >= endPoint) idx0 = endPoint - 1;
                    if (idx1 < startPoint) idx1 = startPoint;
                    if (idx1 >= endPoint) idx1 = endPoint - 1;
                    
                    // Final bounds check (defensive)
                    if (idx0 < 0 || idx0 >= len || idx1 < 0 || idx1 >= len) {
                        inputBuffer[i] = 0.0f;
                    } else {
                    float frac = static_cast<float>(sampleReadPos - static_cast<double>(idx0));
                    frac = std::max(0.0f, std::min(1.0f, frac));
                        float s0 = data[idx0];
                        float s1 = data[idx1];
                        inputBuffer[i] = (s0 * (1.0f - frac) + s1 * frac) * sampleGain;
                    }
                }
                
                // CRITICAL: Check if we're in release BEFORE advancing - if so, don't loop
                // This ensures that when note is released, looping stops immediately
                bool shouldLoop = loopEnabled && !inRelease && loopEndPoint > loopStartPoint;
                
                sampleReadPos += speed;
                
                // Guard against NaN/Inf (already checked above, but double-check after increment)
                if (!std::isfinite(sampleReadPos)) {
                    oobGuardHits.fetch_add(1, std::memory_order_relaxed);
                    sampleReadPos = static_cast<double>(startPoint);
                    if (inRelease) {
                        envelopeValue = 0.0f;
                    }
                }
                
                // Handle looping: if loop is enabled and we've reached the end, loop back to start
                // CRITICAL: Never loop if in release phase - respect ADSR envelope
                // We check shouldLoop which already verified !inRelease
                if (shouldLoop && sampleReadPos >= static_cast<double>(loopEndPoint)) {
                    // Loop back to loop start point (only if not in release)
                    sampleReadPos = static_cast<double>(loopStartPoint);
                }
                // If inRelease is true, we explicitly do NOT loop - let playhead continue to end point
            }
        }
        
        // If we've hit the end point, start release (unless already in release)
        // When looping is enabled and note is released, stop looping and enter release
        if (sampleReadPos >= static_cast<double>(endPoint) && !inRelease) {
            // If loop is enabled, only start release if we've played through at least once
            // OR if the note was already released (inRelease will be set by noteOff)
            // This allows the sample to play once, then loop, then release when key is released
            if (!loopEnabled || sampleReadPos >= static_cast<double>(loopEndPoint)) {
            inRelease = true;
            releaseCounter = 0;
            releaseStartValue = envelopeValue;
            }
        }
        
        // If we're in release and looping, stop looping immediately
        // This ensures that when note is released, looping stops and envelope fades out
        if (inRelease && loopEnabled && sampleReadPos >= static_cast<double>(loopEndPoint) && loopEndPoint > loopStartPoint) {
            // Don't loop - let it continue past loop end to trigger release completion
            // The release will complete when we reach the end point
        }
        
        // Process through time-warp processor
        // For constant duration (timeRatio=1.0), pass numSamples input, request numSamples output
        std::fill(outputBuffer, outputBuffer + numSamples, 0.0f);
        int processedSamples = 0;
        
        if (timePitchProcessor) {
            // Pass numSamples input, request numSamples output (1:1 ratio for constant duration)
            // The processor may return less than numSamples if not enough accumulated yet
            processedSamples = timePitchProcessor->process(inputBuffer, numSamples, outputBuffer, numSamples);
            
            // Time-warp underrun protection: fill with zeros if underran
            if (processedSamples < numSamples * 0.9f) {
                std::fill(outputBuffer + processedSamples, outputBuffer + numSamples, 0.0f);
            }
            
            // #region agent log
            {
                std::ofstream log("/Users/troycarson/Documents/JUCE Projects/OP1-Clone/.cursor/debug.log", std::ios::app);
                if (log.is_open()) {
                    log << "{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"C\",\"location\":\"SamplerVoice.cpp:212\",\"message\":\"Stretcher process returned\",\"data\":{\"processedSamples\":" << processedSamples << ",\"requestedSamples\":" << numSamples << ",\"primeRemaining\":" << primeRemainingSamples << ",\"currentNote\":" << currentNote << "},\"timestamp\":" << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() << "}\n";
                }
            }
            // #endregion
        }
        
        // Count non-zero output samples for debugging
        int nonZeroCount = 0;
        for (int i = 0; i < processedSamples; ++i) {
            if (std::abs(outputBuffer[i]) > 1e-6f) {
                nonZeroCount++;
            }
        }
        
        // Store debug info (read by UI thread via adapter)
        lastActualInN = numSamples; // For constant duration, actualInN = outN
        lastOutN = processedSamples; // Actual output produced (may be less than numSamples)
        lastPrimeRemaining = primeRemainingSamples;
        lastNonZeroCount = nonZeroCount;
        
        // #region agent log
        {
            std::ofstream log("/Users/troycarson/Documents/JUCE Projects/OP1-Clone/.cursor/debug.log", std::ios::app);
            if (log.is_open()) {
                log << "{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"C\",\"location\":\"SamplerVoice.cpp:228\",\"message\":\"Voice process complete\",\"data\":{\"processedSamples\":" << processedSamples << ",\"nonZeroCount\":" << nonZeroCount << ",\"active\":" << (active ? 1 : 0) << ",\"currentNote\":" << currentNote << "},\"timestamp\":" << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() << "}\n";
            }
        }
        // #endregion
        
        // Apply envelope and mix
        // Only process samples that were actually produced by the processor
        // If processedSamples < numSamples, the rest of outputBuffer is already zero-filled
        // IMPORTANT: Even if processedSamples is 0 (still priming), keep voice active
        // and process the envelope so the voice doesn't get deactivated prematurely
        // Use processedSamples to avoid processing zeros that weren't produced yet
        int samplesToProcess = std::max(processedSamples, 0);
        for (int i = 0; i < numSamples; ++i) {
            // Check if voice should start outputting (staggered start)
            if (startDelayCounter < startDelaySamples) {
                startDelayCounter++;
                // Output silence during delay period
                for (int ch = 0; ch < numChannels; ++ch) {
                    if (output[ch] != nullptr) {
                        output[ch][i] += 0.0f;
                    }
                }
                continue; // Skip processing for this sample
            }
            
            // Envelope
            if (inRelease) {
                if (releaseCounter < releaseSamples) {
                    // Release phase: smooth exponential decay from releaseStartValue to 0.0
                    float releaseProgress = static_cast<float>(releaseCounter) / static_cast<float>(releaseSamples);
                    // Exponential decay: smooth, natural release curve
                    float releaseCurve = std::exp(-releaseProgress * 5.0f); // Smooth exponential decay (1.0 to ~0.0)
                    envelopeValue = releaseStartValue * releaseCurve; // Map from releaseStartValue to 0.0
                    // Ensure we reach exactly 0.0 at the end
                    if (releaseCounter >= releaseSamples - 1) {
                        envelopeValue = 0.0f;
                    }
                    releaseCounter++;
                } else {
                    // Release complete - envelope is at 0.0
                    envelopeValue = 0.0f;
                    // Don't deactivate immediately - let fade-out complete first
                    // active = false; // Commented out - let fade-out handle deactivation
                    // break; // Don't break - continue processing until fade-out completes
                }
            } else {
                // ADSR envelope phases - all with smooth cosine curves for click-free transitions
                if (attackCounter < attackSamples) {
                        // Attack phase: always ramp from 0.0 to 1.0 with ultra-smooth exponential curve
                        float attackProgress = static_cast<float>(attackCounter) / static_cast<float>(attackSamples);
                        // Use exponential curve for smoother, more natural attack
                        // Exponential: starts slow, accelerates, then levels off smoothly
                        // This prevents any sudden jumps that could cause clicks
                        float attackCurve = 1.0f - std::exp(-attackProgress * 5.0f); // Smooth exponential (0.0 to ~1.0)
                        // Ensure we reach exactly 1.0 at the end
                        if (attackCounter >= attackSamples - 1) {
                            attackCurve = 1.0f;
                        }
                        envelopeValue = attackCurve; // Ramp from 0.0 to 1.0
                    attackCounter++;
                } else if (decayCounter < decaySamples) {
                    // Decay phase: 1.0 to sustain level with smooth cosine curve
                    float decayProgress = static_cast<float>(decayCounter) / static_cast<float>(decaySamples);
                    // Smooth cosine decay: starts at 1.0, ends at sustainLevel
                    float decayCurve = 0.5f * (1.0f + std::cos(decayProgress * 3.14159265f)); // 1.0 to 0.0
                    envelopeValue = sustainLevel + (1.0f - sustainLevel) * decayCurve; // Map to 1.0 to sustainLevel
                    decayCounter++;
                } else {
                    // Sustain phase: hold at sustain level
                    envelopeValue = sustainLevel;
                }
            }
            
                // Use ADSR envelope (already smoothed in calculation above)
                float testEnvelopeValue = envelopeValue;
            
            float sample = 0.0f;
            
            // DEBUG: Sine test mode - output 220Hz sine instead of sample data
            if (sineTestEnabled) {
                // Generate 220Hz sine wave
                const double phaseIncrement = 220.0 * 2.0 * 3.14159265358979323846 / sampleRate;
                sinePhase += phaseIncrement;
                if (sinePhase > 2.0 * 3.14159265358979323846) {
                    sinePhase -= 2.0 * 3.14159265358979323846;
                }
                sample = static_cast<float>(std::sin(sinePhase));
            } else {
                // Normal sample playback
                sample = (i < processedSamples) ? outputBuffer[i] : 0.0f;
            }
            
            // DC blocking filter (high-pass at ~10Hz)
            dcBlockState += dcBlockAlpha * (sample - dcBlockState);
            sample = sample - dcBlockState;
            
            // TEMPORARY: Disable anti-aliasing filter to test if it's causing fuzziness
            // if (antiAliasAlpha < 1.0f) {
            //     antiAliasState += antiAliasAlpha * (sample - antiAliasState);
            //     sample = antiAliasState;
            // }
            
            // TEMPORARY: Disable ramp gain to test if it's causing clicks
            // Update ramp gain (linear ramp) - once per sample
            // if (isRamping && rampSamplesRemaining > 0) {
            //     rampGain += rampIncrement;
            //     rampSamplesRemaining--;
            //     if (rampSamplesRemaining <= 0) {
            //         rampGain = targetGain;
            //         isRamping = false;
            //         // If ramp reached 0 and we're being stolen, deactivate voice
            //         if (isBeingStolen && rampGain <= 0.0f) {
            //             active = false;
            //             isBeingStolen = false;
            //         }
            //     }
            // }
            // Update ramp gain smoothly (0 -> 1 over 128 samples)
            if (isRamping && rampSamplesRemaining > 0) {
                rampGain += rampIncrement;
                rampSamplesRemaining--;
                if (rampSamplesRemaining <= 0) {
                    rampGain = targetGain;
                    isRamping = false;
                }
            }
            
            // Apply fade-in/out to prevent clicks on attack and release
            // CRITICAL: Start fade-in at 0.0 to ensure first sample is silent
            float fadeGain = 0.0f;
            
            // Fade-in on note start (smooth cosine curve) - start from 0.0
            if (isFadingIn && fadeInCounter < fadeInSamples) {
                // CRITICAL: First sample (fadeInCounter == 0) must be exactly 0.0
                if (fadeInCounter == 0) {
                    fadeGain = 0.0f;
                } else {
                    // Calculate progress for subsequent samples
                    float progress = static_cast<float>(fadeInCounter) / static_cast<float>(fadeInSamples);
                    fadeGain = 0.5f * (1.0f - std::cos(progress * 3.14159265f)); // Smooth cosine fade-in (0.0 to 1.0)
                }
                fadeInCounter++;
            } else {
                isFadingIn = false;
                fadeGain = 1.0f; // Full gain after fade-in completes
            }
            
            // Fade-out on note release - but let ADSR envelope handle the release curve
            // Only use fade-out as a safety mechanism, not the primary release control
            // The ADSR envelope (envelopeValue) is the primary release control
            if (isFadingOut && fadeOutCounter < fadeOutSamples) {
                // Use a very gentle fade-out that doesn't interfere with ADSR release
                // Only apply minimal fade-out to prevent clicks, let envelopeValue do the work
                float progress = static_cast<float>(fadeOutCounter) / static_cast<float>(fadeOutSamples);
                // Very gentle fade - only reduces by 10% max to prevent clicks, envelopeValue handles the rest
                float fadeOutGain = 1.0f - (progress * 0.1f); // Only 10% reduction max
                fadeGain = std::min(fadeGain, fadeOutGain);
                fadeOutCounter++;
            } else if (isFadingOut && fadeOutCounter >= fadeOutSamples) {
                // Fade-out complete - but don't deactivate, let envelope handle it
                fadeGain = 0.9f; // Keep at 90% so envelopeValue can still control
            }
            
            // Simplified gain staging: combine all gains into single multiplication
            // TEMPORARY: Keep original gain staging to test if combined gain is causing issues
            // Simplified gain staging: use ONLY fade-in/out for attack/release, envelope for sustain
            // Increased per-voice gain for better volume
            sample *= voiceGain; // Apply voice gain (no additional reduction)
            // Apply both fade and envelope - fade prevents clicks, envelope shapes the sound
            sample *= fadeGain; // Apply fade to prevent clicks (handles attack/release)
            // Apply envelope - it works together with fade to shape the sound
            float amplitude = baseAmplitude * testEnvelopeValue;
            float outputSample = sample * amplitude;
            
            // Safety processing: Only NaN guard and hard clamp - no soft clip to reduce fuzziness
            if (!std::isfinite(outputSample)) {
                outputSample = 0.0f;
            }
            outputSample = std::max(-1.0f, std::min(1.0f, outputSample));
            
            // Peak measurement (atomic, lock-free)
            float absSample = std::abs(outputSample);
            float currentPeak = peakOut.load(std::memory_order_relaxed);
            if (absSample > currentPeak) {
                peakOut.store(absSample, std::memory_order_relaxed);
            }
            if (absSample > 1.0f) {
                numClippedSamples.fetch_add(1, std::memory_order_relaxed);
            }
            
            for (int ch = 0; ch < numChannels; ++ch) {
                if (output[ch] != nullptr) {
                    output[ch][i] += outputSample;
                }
            }
        }
        
        // Keep voice active even if no output yet (still priming/accumulating)
        // Only deactivate if we've hit the end point AND finished release
        
        if (sampleReadPos >= static_cast<double>(endPoint) && inRelease && releaseCounter >= releaseSamples) {
            // #region agent log
            {
                std::ofstream log("/Users/troycarson/Documents/JUCE Projects/OP1-Clone/.cursor/debug.log", std::ios::app);
                if (log.is_open()) {
                    log << "{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"A\",\"location\":\"SamplerVoice.cpp:268\",\"message\":\"Voice deactivated (end of sample)\",\"data\":{\"currentNote\":" << currentNote << "},\"timestamp\":" << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() << "}\n";
                }
            }
            // #endregion
            active = false;
        }
        
    } else {
        // --- Simple pitch path (no time-warp) ---
        // Calculate pitch ratio and playback speed (include repitch offset)
        int semitones = currentNote - rootMidiNote;
        float totalSemitones = static_cast<float>(semitones) + repitchSemitones;
        double pitchRatio = std::pow(2.0, totalSemitones / 12.0);
        double speed = (sourceSampleRate / sampleRate) * pitchRatio;
        
        // Guard against invalid speed
        if (!std::isfinite(speed) || speed <= 0.0) {
            speed = 1.0;
        }
        
        // Guard against invalid playhead
        if (!std::isfinite(playhead)) {
            playhead = static_cast<double>(startPoint);
        }
        
        for (int i = 0; i < numSamples; ++i) {
            // Check if voice should start outputting (staggered start)
            if (startDelayCounter < startDelaySamples) {
                startDelayCounter++;
                // Output silence during delay period
                for (int ch = 0; ch < numChannels; ++ch) {
                    if (output[ch] != nullptr) {
                        output[ch][i] += 0.0f;
                    }
                }
                continue; // Skip processing for this sample
            }
            
            // CRITICAL: Check if we're in release BEFORE checking loop - if so, don't loop
            // This ensures that when note is released, looping stops immediately
            bool shouldLoop = loopEnabled && !inRelease && loopEndPoint > loopStartPoint;
            
            // Handle looping: if loop is enabled and we've reached the end, loop back to start
            // CRITICAL: Never loop if in release phase - respect ADSR envelope
            // We check shouldLoop which already verified !inRelease
            if (shouldLoop && playhead >= static_cast<double>(loopEndPoint)) {
                // Loop back to loop start point (only if not in release)
                playhead = static_cast<double>(loopStartPoint);
            }
            // If inRelease is true, we explicitly do NOT loop - let playhead continue to end point
            
            // Bounds-safe sample reading
            if (playhead >= static_cast<double>(endPoint - 1)) {
                // At or past last valid index - use last sample value
                int lastIdx = std::max(startPoint, endPoint - 1);
                if (lastIdx >= 0 && lastIdx < len) {
                    float sample = data[lastIdx] * sampleGain;
                    
                    // Handle release if we hit the end
                    // When in release (note released), don't start release again
                    // When looping, only start release if we've reached loop end or note was released
                    if (!inRelease) {
                        if (!loopEnabled || playhead >= static_cast<double>(loopEndPoint)) {
                    inRelease = true;
                    releaseCounter = 0;
                    releaseStartValue = envelopeValue;
                }
            }
            
                    // Process envelope
                    if (releaseCounter < releaseSamples) {
                        float releaseProgress = static_cast<float>(releaseCounter) / static_cast<float>(releaseSamples);
                        // Use smooth exponential decay for release envelope to prevent crackling
                        // Exponential decay: smooth, natural release curve
                        float releaseCurve = std::exp(-releaseProgress * 5.0f); // Smooth exponential decay (1.0 to ~0.0)
                        envelopeValue = releaseStartValue * releaseCurve; // Map from releaseStartValue to 0.0
                        // Ensure we reach exactly 0.0 at the end
                        if (releaseCounter >= releaseSamples - 1) {
                            envelopeValue = 0.0f;
                        }
                        releaseCounter++;
                    } else {
                        envelopeValue = 0.0f;
                        active = false;
                        break;
                    }
                    
                    // DC blocking filter (high-pass at ~10Hz)
                    dcBlockState += dcBlockAlpha * (sample - dcBlockState);
                    sample = sample - dcBlockState;
                    
                    // TEMPORARY: Disable ramp gain to test if it's causing clicks
                    // Update ramp gain (linear ramp) - once per sample
                    // if (isRamping && rampSamplesRemaining > 0) {
                    //     rampGain += rampIncrement;
                    //     rampSamplesRemaining--;
                    //     if (rampSamplesRemaining <= 0) {
                    //         rampGain = targetGain;
                    //         isRamping = false;
                    //         if (isBeingStolen && rampGain <= 0.0f) {
                    //             active = false;
                    //             isBeingStolen = false;
                    //         }
                    //     }
                    // }
                    // Force ramp gain to 1.0 for testing
                    rampGain = 1.0f;
                    
                    // Apply fade-in/out to prevent clicks on attack and release
                    float fadeGain = 1.0f;
                    
                    // Fade-in on note start (smooth cosine curve) - start from 0.0
                    if (isFadingIn && fadeInCounter < fadeInSamples) {
                        float progress = static_cast<float>(fadeInCounter) / static_cast<float>(fadeInSamples);
                        fadeGain = 0.5f * (1.0f - std::cos(progress * 3.14159265f)); // Smooth cosine fade-in (0.0 to 1.0)
                        fadeInCounter++;
                    } else {
                        isFadingIn = false;
                        fadeGain = 1.0f; // Full gain after fade-in completes
                    }
                    
                    // Fade-out on note release - but let ADSR envelope handle the release curve
                    // Only use fade-out as a safety mechanism, not the primary release control
                    // The ADSR envelope (envelopeValue) is the primary release control
                    if (isFadingOut && fadeOutCounter < fadeOutSamples) {
                        // Use a very gentle fade-out that doesn't interfere with ADSR release
                        // Only apply minimal fade-out to prevent clicks, let envelopeValue do the work
                        float progress = static_cast<float>(fadeOutCounter) / static_cast<float>(fadeOutSamples);
                        // Very gentle fade - only reduces by 10% max to prevent clicks, envelopeValue handles the rest
                        float fadeOutGain = 1.0f - (progress * 0.1f); // Only 10% reduction max
                        fadeGain = std::min(fadeGain, fadeOutGain);
                        fadeOutCounter++;
                    } else if (isFadingOut && fadeOutCounter >= fadeOutSamples) {
                        // Fade-out complete - but don't deactivate, let envelope handle it
                        fadeGain = 0.9f; // Keep at 90% so envelopeValue can still control
                    }
                    
                    // Simplified gain staging: combine all gains into single multiplication
                    // TEMPORARY: Keep original gain staging to test if combined gain is causing issues
                    // Apply both fade and envelope - fade prevents clicks, envelope shapes the sound
                    sample *= voiceGain;
                    sample *= fadeGain; // Apply fade to prevent clicks (handles attack/release)
                    // Apply envelope - it works together with fade to shape the sound
                    float amplitude = baseAmplitude * envelopeValue;
                    float outputSample = sample * amplitude;
                    
                    // Safety processing: Only NaN guard and hard clamp - no soft clip
                    if (!std::isfinite(outputSample)) {
                        outputSample = 0.0f;
                    }
                    outputSample = std::max(-1.0f, std::min(1.0f, outputSample));
                    
                    // Peak measurement (atomic, lock-free)
                    float absSample = std::abs(outputSample);
                    float currentPeak = peakOut.load(std::memory_order_relaxed);
                    if (absSample > currentPeak) {
                        peakOut.store(absSample, std::memory_order_relaxed);
                    }
                    if (absSample > 1.0f) {
                        numClippedSamples.fetch_add(1, std::memory_order_relaxed);
                    }
                    
                    // Update oobGuardHits if we hit bounds (already counted above)
                    
                    for (int ch = 0; ch < numChannels; ++ch) {
                        if (output[ch] != nullptr) {
                            output[ch][i] += outputSample;
                        }
                    }
                } else {
                    // Invalid index - output silence
                    for (int ch = 0; ch < numChannels; ++ch) {
                        if (output[ch] != nullptr) {
                            output[ch][i] += 0.0f;
                        }
                    }
                }
            } else if (playhead < static_cast<double>(startPoint)) {
                // Before start point - output silence
                for (int ch = 0; ch < numChannels; ++ch) {
                    if (output[ch] != nullptr) {
                        output[ch][i] += 0.0f;
                    }
                }
            } else {
                // Safe interpolation: idx1 is guaranteed < endPoint (which is <= len)
                int index0 = static_cast<int>(playhead);
                int index1 = index0 + 1;
                
                // Clamp to valid range
                if (index0 < startPoint) index0 = startPoint;
                if (index0 >= endPoint) index0 = endPoint - 1;
                if (index1 < startPoint) index1 = startPoint;
                if (index1 >= endPoint) index1 = endPoint - 1;
                
                // Process envelope
            if (inRelease) {
                if (releaseCounter < releaseSamples) {
                    // Release phase: smooth exponential decay from releaseStartValue to 0.0
                    float releaseProgress = static_cast<float>(releaseCounter) / static_cast<float>(releaseSamples);
                    // Exponential decay: smooth, natural release curve
                    float releaseCurve = std::exp(-releaseProgress * 5.0f); // Smooth exponential decay (1.0 to ~0.0)
                    envelopeValue = releaseStartValue * releaseCurve; // Map from releaseStartValue to 0.0
                    // Ensure we reach exactly 0.0 at the end
                    if (releaseCounter >= releaseSamples - 1) {
                        envelopeValue = 0.0f;
                    }
                    releaseCounter++;
                } else {
                    // Release complete - envelope is at 0.0
                    envelopeValue = 0.0f;
                    // Don't deactivate immediately - let fade-out complete first
                    // active = false; // Commented out - let fade-out handle deactivation
                    // break; // Don't break - continue processing until fade-out completes
                }
            } else {
                // ADSR envelope phases - all with smooth cosine curves for click-free transitions
                if (attackCounter < attackSamples) {
                        // Attack phase: always ramp from 0.0 to 1.0 with ultra-smooth exponential curve
                        float attackProgress = static_cast<float>(attackCounter) / static_cast<float>(attackSamples);
                        // Use exponential curve for smoother, more natural attack
                        // Exponential: starts slow, accelerates, then levels off smoothly
                        // This prevents any sudden jumps that could cause clicks
                        float attackCurve = 1.0f - std::exp(-attackProgress * 5.0f); // Smooth exponential (0.0 to ~1.0)
                        // Ensure we reach exactly 1.0 at the end
                        if (attackCounter >= attackSamples - 1) {
                            attackCurve = 1.0f;
                        }
                        envelopeValue = attackCurve; // Ramp from 0.0 to 1.0
                    attackCounter++;
                } else if (decayCounter < decaySamples) {
                    // Decay phase: 1.0 to sustain level with smooth cosine curve
                    float decayProgress = static_cast<float>(decayCounter) / static_cast<float>(decaySamples);
                    // Smooth cosine decay: starts at 1.0, ends at sustainLevel
                    float decayCurve = 0.5f * (1.0f + std::cos(decayProgress * 3.14159265f)); // 1.0 to 0.0
                    envelopeValue = sustainLevel + (1.0f - sustainLevel) * decayCurve; // Map to 1.0 to sustainLevel
                    decayCounter++;
                } else {
                    // Sustain phase: hold at sustain level
                    envelopeValue = sustainLevel;
                }
            }
            
                // Use ADSR envelope (already smoothed in calculation above)
                float testEnvelopeValue = envelopeValue;
            
                // Final bounds check (defensive) - use cubic Hermite interpolation
            float sample = 0.0f;
                if (index0 >= 0 && index0 < len && index1 >= 0 && index1 < len) {
                float fraction = static_cast<float>(playhead - static_cast<double>(index0));
                fraction = std::max(0.0f, std::min(1.0f, fraction));
                    
                    // TEMPORARY: Use linear interpolation to test if cubic is causing crackling
                    // Cubic Hermite interpolation (4-point)
                    // int idx0 = index0 - 1;
                    // int idx1 = index0;
                    // int idx2 = index1;
                    // int idx3 = index1 + 1;
                    // 
                    // // Clamp indices to valid range
                    // idx0 = std::max(0, std::min(len - 1, idx0));
                    // idx1 = std::max(0, std::min(len - 1, idx1));
                    // idx2 = std::max(0, std::min(len - 1, idx2));
                    // idx3 = std::max(0, std::min(len - 1, idx3));
                    // 
                    // float s0 = data[idx0];
                    // float s1 = data[idx1];
                    // float s2 = data[idx2];
                    // float s3 = data[idx3];
                    // sample = cubicHermite(s0, s1, s2, s3, fraction) * sampleGain;
                    
                    // Linear interpolation (fallback)
                    // Ensure we're reading valid sample data
                    if (index0 >= 0 && index0 < len && index1 >= 0 && index1 < len) {
                        float s0 = data[index0];
                        float s1 = data[index1];
                        sample = (s0 * (1.0f - fraction) + s1 * fraction) * sampleGain;
                    } else {
                        sample = 0.0f; // Safety: output silence if indices are invalid
                    }
                }
                
                // Apply fade-in/out to prevent clicks on attack and release
                // CRITICAL: Start fade-in at 0.0 to ensure first sample is silent
                float fadeGain = 0.0f;
                
                // Fade-in on note start (smooth cosine curve) - start from 0.0
                if (isFadingIn && fadeInCounter < fadeInSamples) {
                    // Calculate progress: fadeInCounter starts at 0, so first sample has progress = 0, fadeGain = 0.0
                    // Use fadeInCounter + 1 to ensure first sample (counter=0) gets a small but non-zero progress
                    float progress = static_cast<float>(fadeInCounter + 1) / static_cast<float>(fadeInSamples);
                    fadeGain = 0.5f * (1.0f - std::cos(progress * 3.14159265f)); // Smooth cosine fade-in (0.0 to 1.0)
                    fadeInCounter++;
                } else {
                    isFadingIn = false;
                    fadeGain = 1.0f; // Full gain after fade-in completes
                    // retriggerOldEnvelope no longer used - removed for true retrigger behavior
                }
                
                // Fade-out on note release - but let ADSR envelope handle the release curve
                // Only use fade-out as a safety mechanism, not the primary release control
                // The ADSR envelope (envelopeValue) is the primary release control
                if (isFadingOut && fadeOutCounter < fadeOutSamples) {
                    // Use a very gentle fade-out that doesn't interfere with ADSR release
                    // Only apply minimal fade-out to prevent clicks, let envelopeValue do the work
                    float progress = static_cast<float>(fadeOutCounter) / static_cast<float>(fadeOutSamples);
                    // Very gentle fade - only reduces by 10% max to prevent clicks, envelopeValue handles the rest
                    float fadeOutGain = 1.0f - (progress * 0.1f); // Only 10% reduction max
                    fadeGain = std::min(fadeGain, fadeOutGain);
                    fadeOutCounter++;
                } else if (isFadingOut && fadeOutCounter >= fadeOutSamples) {
                    // Fade-out complete - but don't deactivate, let envelope handle it
                    fadeGain = 0.9f; // Keep at 90% so envelopeValue can still control
                }
                
                // Clean, simple processing chain - no extra filters or processing
                // Increased per-voice gain for better volume
                sample *= voiceGain; // Apply voice gain (no additional reduction)
                sample *= fadeGain; // Apply fade to prevent clicks (handles attack/release)
                
                // Apply envelope - when retriggering same note, envelope ramps smoothly from current value
                // No crossfade needed since playhead isn't reset, maintaining sample continuity
                float finalEnvelope = testEnvelopeValue;
                
                float outputSample = sample * baseAmplitude * finalEnvelope;
                
                // Safety: NaN guard and hard clamp
                if (!std::isfinite(outputSample)) {
                    outputSample = 0.0f;
                }
                outputSample = std::max(-1.0f, std::min(1.0f, outputSample));
                
                // Peak measurement (atomic, lock-free)
                float absSample = std::abs(outputSample);
                float currentPeak = peakOut.load(std::memory_order_relaxed);
                if (absSample > currentPeak) {
                    peakOut.store(absSample, std::memory_order_relaxed);
                }
                if (absSample > 1.0f) {
                    numClippedSamples.fetch_add(1, std::memory_order_relaxed);
                }
                
            for (int ch = 0; ch < numChannels; ++ch) {
                if (output[ch] != nullptr) {
                    output[ch][i] += outputSample;
                    }
                }
            }
            
            playhead += speed;
            
            // Guard against NaN/Inf
            if (!std::isfinite(playhead)) {
                playhead = static_cast<double>(startPoint);
            }
        }
        
        // Only deactivate after both release envelope AND fade-out complete
        if (playhead >= static_cast<double>(endPoint) && inRelease && releaseCounter >= releaseSamples && 
            (!isFadingOut || fadeOutCounter >= fadeOutSamples)) {
            active = false;
        }
    }
}

void SamplerVoice::setGain(float g) {
    gain = clamp(g, 0.0f, 1.0f);
}

float SamplerVoice::clamp(float value, float min, float max) {
    return std::max(min, std::min(max, value));
}

void SamplerVoice::getDebugInfo(int& actualInN, int& outN, int& primeRemaining, int& nonZeroCount) const {
    actualInN = lastActualInN;
    outN = lastOutN;
    primeRemaining = lastPrimeRemaining;
    nonZeroCount = lastNonZeroCount;
}

// Cubic Hermite interpolation (4-point)
float SamplerVoice::cubicHermite(float y0, float y1, float y2, float y3, float t) {
    // Clamp t to [0, 1]
    t = std::max(0.0f, std::min(1.0f, t));
    
    // Hermite basis functions
    float t2 = t * t;
    float t3 = t2 * t;
    
    float h00 = 2.0f * t3 - 3.0f * t2 + 1.0f;
    float h10 = t3 - 2.0f * t2 + t;
    float h01 = -2.0f * t3 + 3.0f * t2;
    float h11 = t3 - t2;
    
    // Tangents (finite differences)
    float m0 = 0.5f * (y2 - y0);
    float m1 = 0.5f * (y3 - y1);
    
    return h00 * y1 + h10 * m0 + h01 * y2 + h11 * m1;
}

void SamplerVoice::updateAntiAliasFilter(float pitchRatio) {
    // Only apply anti-aliasing when pitching up
    if (pitchRatio > 1.0f) {
        // Cutoff scaled down when pitching up
        float cutoff = 0.45f / pitchRatio; // relative to Nyquist
        cutoff = std::max(0.05f, std::min(0.45f, cutoff));
        
        // One-pole coefficient
        float x = std::exp(-2.0f * 3.14159265f * (cutoff * 0.5f));
        antiAliasAlpha = 1.0f - x;
    } else {
        // No filtering when pitching down or at unity
        antiAliasAlpha = 1.0f; // Pass through
    }
}

} // namespace Core

