#include "SamplerVoice.h"
#include "DSP/SignalsmithStretchWrapper.h"
#include <atomic>
#include <cmath>
#include <algorithm>
#include <cstring>
#include <fstream>
#include <chrono>
#include <limits>
#include <typeinfo>

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
    , sampleReadPos(0.0)
    , sineTestEnabled(false)
    , sinePhase(0.0)
    , primeRemainingSamples(0)
    , loopEnabled(false)
    , loopStartPoint(0)
    , loopEndPoint(0)
    , loopCrossfadeActive(false)
    , loopCrossfadeSamples(0)  // Will be set based on sample rate
    , loopCrossfadeCounter(0)
    , loopFadeOutGain(1.0f)
    , loopFadeInGain(0.0f)
    , voiceGain(1.0f)  // Full voice gain for better volume
    , rampGain(0.0f)
    , targetGain(0.0f)
    , rampIncrement(0.0f)
    , rampSamplesRemaining(0)
    , isRamping(false)
    , isBeingStolen(false)
    , fadeInSamples(128)  // microRamp duration (128 samples)
    , fadeOutSamples(4096)
    , fadeInCounter(0)
    , fadeOutCounter(0)
    , isFadingIn(false)
    , isFadingOut(false)
    , lastVoiceSampleL(0.0f)
    , lastVoiceSampleR(0.0f)
    , maxVoiceDelta(0.0f)
    , voiceId(0)
    , voiceFlags(0)
    , slewLastOutL(0.0f)
    , slewLastOutR(0.0f)
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
    , warpProcessor(nullptr)
    , warpEnabled(false)  // Disabled - fix simple path first
    , timeRatio(1.0)  // Normal speed (no time stretching)
    , warpInputPlanar(nullptr)
    , warpOutputPlanar(nullptr)
    , warpBufferSize(0)
    , warpPriming(false)
    , warpCrossfadeActive(false)
    , warpCrossfadePos(0.0f)
    , warpCrossfadeInc(0.0f)
    , warpCrossfadeSamples(0)
    , warpCrossfadeCounter(0)
    , dryRMS(0.0f)
    , warpRMS(0.0f)
    , gainMatch(1.0f)
    , gainMatchTarget(1.0f)
    , gainMatchAttack(0.0f)
    , gainMatchRelease(0.0f)
{
}

SamplerVoice::~SamplerVoice() {
    // Sample data is owned by caller, we don't delete it
    // Clean up warp buffers
    if (warpInputPlanar != nullptr) {
        delete[] warpInputPlanar[0];
        delete[] warpInputPlanar[1];
        delete[] warpInputPlanar;
    }
    if (warpOutputPlanar != nullptr) {
        delete[] warpOutputPlanar[0];
        delete[] warpOutputPlanar[1];
        delete[] warpOutputPlanar;
    }
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
                log << "{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"A\",\"location\":\"SamplerVoice.cpp:60\",\"message\":\"noteOn called\",\"data\":{\"note\":" << note << ",\"velocity\":" << velocity << ",\"wasActive\":" << (active ? 1 : 0) << "},\"timestamp\":" << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() << "}\n";
        }
    }
    // #endregion
    
    currentNote = note;
    currentVelocity = clamp(velocity, 0.0f, 1.0f);
    
    bool wasActive = active;
    bool wasSameNote = (wasActive && currentNote == note);
    
    // CRITICAL: Always reset playhead on noteOn to ensure clean retrigger
    // The rampGain (starts at 0.0) and envelope (starts at 0.0) will handle smooth fade-in
    // Always start at sample start, even for reverse loops
    // Reverse loop will activate when playhead reaches loopStartPoint
    playhead = static_cast<double>(startPoint);
    sampleReadPos = static_cast<double>(startPoint);
    
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
        
        // CRITICAL: microRamp MUST start at 0 and ramp to 1 over 128 samples
        rampGain = 0.0f; // Always start from 0
        targetGain = 1.0f;
        rampSamplesRemaining = 128; // microRamp duration (128 samples)
        rampIncrement = 1.0f / 128.0f;
        isRamping = true;
        isBeingStolen = false;
        
        // Reset pop detection
        lastVoiceSampleL = 0.0f;
        lastVoiceSampleR = 0.0f;
        maxVoiceDelta = 0.0f;
        
        // Reset warp processor if enabled
        if (warpEnabled && timeRatio != 1.0 && warpProcessor) {
            warpProcessor->reset();
            warpPriming = false;
            warpCrossfadeActive = false;
            warpCrossfadePos = 0.0f;
            warpCrossfadeCounter = 0;
        }
    }
    
    
    // CRITICAL: Always start envelope from 0.0 on noteOn to prevent jumps
    // This ensures attack always starts from 0.0, creating smooth transitions
    retriggerOldEnvelope = envelopeValue; // Store old value for reference, but don't use it
    
    // Always start attack from 0.0 (prevents discontinuities on retrigger)
    envelopeValue = 0.0f;
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
    // CRITICAL: Always use minimum attack time (even at 0ms) to prevent pops
    // Use at least 2ms (88 samples at 44.1k) for smooth attack
    float effectiveAttackMs = std::max(attackTimeMs, 2.0f);
    attackSamples = static_cast<int>(currentSampleRate * effectiveAttackMs / 1000.0);
    if (attackSamples < 1) attackSamples = 1;
    // Ensure minimum of 128 samples for smooth attack (prevents pops even at 0ms setting)
    attackSamples = std::max(attackSamples, 128);
    
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
        int semitones = currentNote - rootMidiNote;
        float totalSemitones = static_cast<float>(semitones) + repitchSemitones;
        float pitchRatio = std::pow(2.0f, totalSemitones / 12.0f);
        updateAntiAliasFilter(pitchRatio);
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
        
        // CRITICAL: noteOff must NOT deactivate voice immediately
        // ampEnv release must ramp from current value to 0 (no reset)
        // Do NOT start microRamp ramp-out for normal noteOff (only for stealing/kill)
        // The envelope release will handle the fade-out smoothly
        
        // Start fade-out (legacy, for compatibility) - but don't interfere with envelope
        fadeOutSamples = releaseSamples; // Match release envelope duration
        isFadingOut = true;
        fadeOutCounter = 0;
        
        // Do NOT ramp out microRamp on normal noteOff - let envelope handle it
        // Only ramp out microRamp for voice stealing or explicit kill
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
    // CRITICAL: Voice stealing must not overwrite state immediately
    // Mark as stealing and ramp it out (256 samples)
    // Only after it is silent may it be reused/reset
    isBeingStolen = true;
    targetGain = 0.0f;
    rampSamplesRemaining = 256; // Short fade-out for stealing
    rampIncrement = -rampGain / 256.0f;
    isRamping = true;
    
    // Also start envelope release if not already in release
    if (!inRelease) {
        releaseStartValue = envelopeValue;
        inRelease = true;
        releaseCounter = 0;
    }
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
    
    // Set loop crossfade duration based on sample rate (~50ms for smooth crossfade - longer to prevent pops)
    if (sampleRate > 0.0 && loopCrossfadeSamples == 0) {
        loopCrossfadeSamples = static_cast<int>(sampleRate * 0.15); // 150ms crossfade for much smoother transitions
        loopCrossfadeSamples = std::max(512, std::min(loopCrossfadeSamples, 8192)); // Clamp to reasonable range
    }
    
    // Prepare warp processor if needed
    // When warp is enabled, use Signalsmith Stretch to maintain constant duration (timeRatio = 1.0)
    // while allowing pitch to change via setTransposeSemitones()
    if (warpEnabled) {
        if (!warpProcessor) {
            warpProcessor = std::make_unique<SignalsmithStretchWrapper>();
        }
        if (sampleRateChanged || !warpProcessor->isPrepared()) {
            warpProcessor->prepare(sampleRate, 2, numSamples);
            // Initialize gain matching coefficients
            gainMatchAttack = 1.0f - std::exp(-1.0f / (0.005f * sampleRate)); // 5ms attack
            gainMatchRelease = 1.0f - std::exp(-1.0f / (0.1f * sampleRate));  // 100ms release
            gainMatch = 1.0f;
            gainMatchTarget = 1.0f;
            dryRMS = 0.0f;
            warpRMS = 0.0f;
        }
        // Set time ratio to 1.0 for constant duration (all notes same length)
        warpProcessor->setTimeRatio(1.0);
        // Set pitch based on note (this changes pitch while keeping duration constant)
        int semitones = currentNote - rootMidiNote;
        float totalSemitones = static_cast<float>(semitones) + repitchSemitones;
        warpProcessor->setPitchSemitones(totalSemitones);
    }
    
    // Base amplitude (velocity * gain)
    const float amplitudeScale = 1.0f;
    float baseAmplitude = currentVelocity * gain * amplitudeScale;
    
    // Decision logic: use warp path when enabled AND processor is ready
    // When warp is disabled OR processor not ready, use simple resampling
    bool useWarpPath = false;
    if (warpEnabled) {
        // Only use warp if processor exists and is prepared
        if (warpProcessor && warpProcessor->isPrepared()) {
            useWarpPath = true;
        }
    }
    
    if (useWarpPath) {
        // --- Time Stretch Path (Signalsmith) ---
        // Allocate buffers if needed
        if (warpBufferSize < numSamples) {
            if (warpInputPlanar != nullptr) {
                delete[] warpInputPlanar[0];
                delete[] warpInputPlanar[1];
                delete[] warpInputPlanar;
            }
            if (warpOutputPlanar != nullptr) {
                delete[] warpOutputPlanar[0];
                delete[] warpOutputPlanar[1];
                delete[] warpOutputPlanar;
            }
            warpBufferSize = numSamples;
            warpInputPlanar = new float*[2];
            warpOutputPlanar = new float*[2];
            warpInputPlanar[0] = new float[static_cast<size_t>(warpBufferSize)];
            warpInputPlanar[1] = new float[static_cast<size_t>(warpBufferSize)];
            warpOutputPlanar[0] = new float[static_cast<size_t>(warpBufferSize)];
            warpOutputPlanar[1] = new float[static_cast<size_t>(warpBufferSize)];
        }
        
        // Read sample at original speed (no pitch adjustment - warp handles pitch)
        // For constant duration (timeRatio = 1.0), input frames = output frames
        int inFramesNeeded = numSamples;  // 1:1 ratio for constant duration
        inFramesNeeded = std::max(1, std::min(inFramesNeeded, warpBufferSize));
        
        // Read sample data at original speed into warpInputPlanar
        double originalSpeed = sourceSampleRate / sampleRate; // No pitch adjustment
        for (int i = 0; i < inFramesNeeded; ++i) {
            // Handle looping
            bool shouldLoop = loopEnabled && !inRelease && loopEndPoint > loopStartPoint;
            if (shouldLoop && playhead >= static_cast<double>(loopEndPoint)) {
                playhead = static_cast<double>(loopStartPoint);
            }
            
            // Read sample with linear interpolation
            float sample = 0.0f;
            if (playhead >= static_cast<double>(endPoint - 1)) {
                    int lastIdx = std::max(startPoint, endPoint - 1);
                    if (lastIdx >= 0 && lastIdx < len) {
                    sample = data[lastIdx] * sampleGain;
                }
                if (!inRelease && (!loopEnabled || playhead >= static_cast<double>(loopEndPoint))) {
                    inRelease = true;
                    releaseCounter = 0;
                    releaseStartValue = envelopeValue;
                }
            } else if (playhead >= static_cast<double>(startPoint)) {
                int index0 = static_cast<int>(playhead);
                int index1 = index0 + 1;
                index0 = std::max(startPoint, std::min(endPoint - 1, index0));
                index1 = std::max(startPoint, std::min(endPoint - 1, index1));
                if (index0 >= 0 && index0 < len && index1 >= 0 && index1 < len) {
                    float fraction = static_cast<float>(playhead - static_cast<double>(index0));
                    float s0 = data[index0];
                    float s1 = data[index1];
                    sample = (s0 * (1.0f - fraction) + s1 * fraction) * sampleGain;
                }
            }
            
            // Write to planar buffer (stereo - duplicate mono)
            warpInputPlanar[0][i] = sample;
            warpInputPlanar[1][i] = sample;
            
            playhead += originalSpeed;
            if (!std::isfinite(playhead)) {
                playhead = static_cast<double>(startPoint);
            }
        }
        
        // Process through warp processor
        int outFrames = warpProcessor->process(
            const_cast<const float* const*>(warpInputPlanar), inFramesNeeded,
            warpOutputPlanar, numSamples
        );
        
        // Update crossfade state
        if (!warpPriming && warpProcessor->getOutputRingFill() >= warpProcessor->getLatencyFrames()) {
            warpPriming = true;
            warpCrossfadeActive = true;
            warpCrossfadePos = 0.0f;
            warpCrossfadeSamples = static_cast<int>(std::round(0.01 * sampleRate)); // 10ms
            warpCrossfadeInc = 1.0f / static_cast<float>(warpCrossfadeSamples);
            warpCrossfadeCounter = 0;
        }
        
        // Update crossfade position
        if (warpCrossfadeActive && warpCrossfadeCounter < warpCrossfadeSamples) {
            warpCrossfadePos += warpCrossfadeInc;
            warpCrossfadeCounter++;
            if (warpCrossfadePos >= 1.0f) {
                warpCrossfadePos = 1.0f;
                warpCrossfadeActive = false;
            }
        }
        
        // Compute RMS for gain matching
        float blockDryRMS = 0.0f;
        float blockWarpRMS = 0.0f;
        for (int i = 0; i < outFrames; ++i) {
            float warpL = warpOutputPlanar[0][i];
            float warpR = warpOutputPlanar[1][i];
            blockWarpRMS += warpL * warpL + warpR * warpR;
        }
        blockWarpRMS = std::sqrt(blockWarpRMS / static_cast<float>(outFrames * 2));
        
        // Smooth RMS values
        float rmsAlpha = (blockWarpRMS > warpRMS) ? gainMatchAttack : gainMatchRelease;
        warpRMS = warpRMS + rmsAlpha * (blockWarpRMS - warpRMS);
        
        // Compute gain match (dry reference would be computed from simple resampling)
        // For now, use a fixed reference or compute from dry path
        if (dryRMS < 1e-4f) {
            dryRMS = blockWarpRMS; // Initialize from warp if dry not available
        }
        
        float newGainMatch = (dryRMS > 1e-6f) ? (dryRMS / std::max(warpRMS, 1e-6f)) : 1.0f;
        newGainMatch = std::max(0.5f, std::min(2.0f, newGainMatch)); // Clamp to [0.5, 2.0]
        
        // Smooth gain match
        float matchAlpha = (newGainMatch > gainMatchTarget) ? gainMatchAttack : gainMatchRelease;
        gainMatchTarget = gainMatchTarget + matchAlpha * (newGainMatch - gainMatchTarget);
        gainMatch = gainMatchTarget;
        
        // Apply gain matching and process output
        float limiterGain = 1.0f;
        float blockPeak = 0.0f;
        
        for (int i = 0; i < outFrames; ++i) {
            // Apply gain matching
            float warpL = warpOutputPlanar[0][i] * gainMatch;
            float warpR = warpOutputPlanar[1][i] * gainMatch;
            
            // Safety limiter
            float peak = std::max(std::abs(warpL), std::abs(warpR));
            if (peak > blockPeak) blockPeak = peak;
            if (peak > 0.98f) {
                float atten = 0.98f / peak;
                limiterGain = std::min(limiterGain, atten);
            }
        }
        
        // Apply limiter gain smoothly
        float limiterAlpha = (limiterGain < lastLimiterGain) ? 0.1f : 0.01f;
        lastLimiterGain = lastLimiterGain + limiterAlpha * (limiterGain - lastLimiterGain);
        
        // Process envelope and mix output
        for (int i = 0; i < outFrames; ++i) {
            // Process envelope (same as simple path)
            if (inRelease) {
                if (releaseCounter < releaseSamples) {
                    float releaseProgress = static_cast<float>(releaseCounter) / static_cast<float>(releaseSamples);
                    float releaseCurve = std::exp(-releaseProgress * 5.0f);
                    envelopeValue = releaseStartValue * releaseCurve;
                    if (releaseCounter >= releaseSamples - 1) {
                        envelopeValue = 0.0f;
                    }
                    releaseCounter++;
                } else {
                    envelopeValue = 0.0f;
                }
            } else {
                if (attackCounter < attackSamples) {
                        float attackProgress = static_cast<float>(attackCounter) / static_cast<float>(attackSamples);
                    float attackCurve = 1.0f - std::exp(-attackProgress * 5.0f);
                        if (attackCounter >= attackSamples - 1) {
                            attackCurve = 1.0f;
                        }
                    envelopeValue = attackCurve;
                    attackCounter++;
                } else if (decayCounter < decaySamples) {
                    float decayProgress = static_cast<float>(decayCounter) / static_cast<float>(decaySamples);
                    float decayCurve = 0.5f * (1.0f + std::cos(decayProgress * 3.14159265f));
                    envelopeValue = sustainLevel + (1.0f - sustainLevel) * decayCurve;
                    decayCounter++;
                } else {
                    envelopeValue = sustainLevel;
                }
            }
            
            // Update ramp gain
            if (isRamping && rampSamplesRemaining > 0) {
                rampGain += rampIncrement;
                rampSamplesRemaining--;
                if (rampSamplesRemaining <= 0) {
                    rampGain = targetGain;
                    isRamping = false;
                }
            }
            
            // Get warp output with gain matching and limiter
            float warpL = warpOutputPlanar[0][i] * gainMatch * lastLimiterGain;
            float warpR = warpOutputPlanar[1][i] * gainMatch * lastLimiterGain;
            
            // Apply crossfade (if priming, fade in from silence)
            float crossfade = warpPriming ? warpCrossfadePos : 1.0f;
            warpL *= crossfade;
            warpR *= crossfade;
            
            // Apply envelope, ramp, and voice gain
            float amplitude = baseAmplitude * envelopeValue;
            float voiceOutL = warpL * voiceGain * rampGain * amplitude;
            float voiceOutR = warpR * voiceGain * rampGain * amplitude;
            
            // Safety processing
            if (!std::isfinite(voiceOutL)) voiceOutL = 0.0f;
            if (!std::isfinite(voiceOutR)) voiceOutR = 0.0f;
            voiceOutL = std::max(-1.0f, std::min(1.0f, voiceOutL));
            voiceOutR = std::max(-1.0f, std::min(1.0f, voiceOutR));
            
            // Slew limiting
            float deltaL = voiceOutL - slewLastOutL;
            float deltaR = voiceOutR - slewLastOutR;
            const float maxStep = 0.02f;
            if (std::abs(deltaL) > maxStep) {
                voiceOutL = slewLastOutL + (deltaL > 0.0f ? maxStep : -maxStep);
            }
            if (std::abs(deltaR) > maxStep) {
                voiceOutR = slewLastOutR + (deltaR > 0.0f ? maxStep : -maxStep);
            }
            slewLastOutL = voiceOutL;
            slewLastOutR = voiceOutR;
            
            // Mix to output
            if (output[0] != nullptr) output[0][i] += voiceOutL;
            if (output[1] != nullptr && numChannels > 1) output[1][i] += voiceOutR;
        }
        
        // Voice reuse safety
        bool canDeactivate = (playhead >= static_cast<double>(endPoint) && inRelease && 
                             releaseCounter >= releaseSamples && 
                             rampGain <= 0.001f);
        if (canDeactivate) {
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
            // Support reverse playback: if loopStartPoint > loopEndPoint, play in reverse
            // Reverse loop only activates when playhead reaches loopStartPoint
            bool isReverseLoop = loopEnabled && loopStartPoint > loopEndPoint;
            // For reverse loops, we're in the loop region when playhead is between loopEndPoint and loopStartPoint
            // (going backwards from loopStartPoint to loopEndPoint)
            bool inReverseLoopRegion = isReverseLoop && playhead >= static_cast<double>(loopEndPoint) && playhead <= static_cast<double>(loopStartPoint);
            bool shouldLoop = loopEnabled && !inRelease && 
                             ((loopEndPoint > loopStartPoint && playhead >= static_cast<double>(loopStartPoint) && playhead < static_cast<double>(loopEndPoint)) ||
                              (isReverseLoop && inReverseLoopRegion));
            
            // Calculate distance from loop end point (for crossfade)
            // For reverse loops, we need to check distance from loop end point (when going backwards)
            double distanceFromLoopEnd;
            if (isReverseLoop && inReverseLoopRegion) {
                // Reverse loop: check distance from loop end point (we're going backwards)
                distanceFromLoopEnd = playhead - static_cast<double>(loopEndPoint);
            } else if (!isReverseLoop && shouldLoop) {
                // Forward loop: check distance from loop end point
                distanceFromLoopEnd = static_cast<double>(loopEndPoint) - playhead;
            } else {
                distanceFromLoopEnd = -1.0; // Not in loop region
            }
            bool inLoopCrossfadeRegion = shouldLoop && distanceFromLoopEnd >= 0.0 && 
                                         distanceFromLoopEnd <= static_cast<double>(loopCrossfadeSamples);
            
            // Start loop crossfade when entering the crossfade region
            if (inLoopCrossfadeRegion && !loopCrossfadeActive) {
                loopCrossfadeActive = true;
                loopCrossfadeCounter = 0;
                // Calculate fade-out gain (1.0 -> 0.0) and fade-in gain (0.0 -> 1.0)
                int samplesUntilLoopEnd = static_cast<int>(distanceFromLoopEnd);
                loopCrossfadeCounter = loopCrossfadeSamples - samplesUntilLoopEnd;
                loopFadeOutGain = 1.0f - (static_cast<float>(loopCrossfadeCounter) / static_cast<float>(loopCrossfadeSamples));
                loopFadeInGain = static_cast<float>(loopCrossfadeCounter) / static_cast<float>(loopCrossfadeSamples);
            }
            
            // Update loop crossfade gains
            if (loopCrossfadeActive) {
                loopCrossfadeCounter++;
                if (loopCrossfadeCounter < loopCrossfadeSamples) {
                    // Update fade gains
                    float fadeProgress = static_cast<float>(loopCrossfadeCounter) / static_cast<float>(loopCrossfadeSamples);
                    loopFadeOutGain = 1.0f - fadeProgress;  // Fade out from loop end
                    loopFadeInGain = fadeProgress;          // Fade in from loop start
                } else {
                    // Crossfade complete
                    loopCrossfadeActive = false;
                    loopFadeOutGain = 0.0f;
                    loopFadeInGain = 1.0f;
                }
            }
            
            // Handle looping: if loop is enabled and we've reached the end, loop back to start
            // CRITICAL: Never loop if in release phase - respect ADSR envelope
            // We check shouldLoop which already verified !inRelease
            // Support reverse playback: when loopStartPoint > loopEndPoint, play in reverse
            // Reverse loop is handled in the playhead update section (after decrement)
            // Forward loop: jump back to loop start point when reaching loop end
            // Check forward loop BEFORE playhead update to catch it at the right time
            if (!isReverseLoop && shouldLoop && playhead >= static_cast<double>(loopEndPoint)) {
                // Forward loop: jump back to loop start point
                double overshoot = playhead - static_cast<double>(loopEndPoint);
                playhead = static_cast<double>(loopStartPoint) + overshoot;
                
                // If crossfade is still active, continue it
                if (!loopCrossfadeActive && shouldLoop) {
                    // Start crossfade from loop start
                    loopCrossfadeActive = true;
                    loopCrossfadeCounter = 0;
                    loopFadeOutGain = 0.0f;
                    loopFadeInGain = 0.0f;
                }
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
                        // CRITICAL: Do NOT hard deactivate - let ramp handle fade-out
                        // Only deactivate after rampGain reaches 0
                        // active = false; // REMOVED - let ramp handle deactivation
                        // break; // REMOVED - continue processing until ramp done
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
                    // Update ramp gain smoothly (0 -> 1 over 128 samples)
                    if (isRamping && rampSamplesRemaining > 0) {
                        rampGain += rampIncrement;
                        rampSamplesRemaining--;
                        if (rampSamplesRemaining <= 0) {
                            rampGain = targetGain;
                            isRamping = false;
                            // CRITICAL: Only deactivate after rampGain reaches 0 AND we're being stolen
                            if (isBeingStolen && rampGain <= 0.001f) {
                            active = false;
                                isBeingStolen = false;
                            }
                        }
                    }
                    
                    // CRITICAL: Apply microRamp (rampGain) to prevent clicks on voice start
                    // rampGain starts at 0.0 and ramps to 1.0 over 128 samples
                    // Always compute output smoothly - gains will naturally be 0.0 when inactive
                    // This eliminates hard discontinuities that cause clicks
                    float amplitude = baseAmplitude * envelopeValue;
                    float outputSample = sample * voiceGain * rampGain * amplitude;
                    
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
                    
                    // Apply per-voice slew limiter (click suppressor)
                    float voiceOutL = outputSample;
                    float voiceOutR = outputSample;
                    
                    // Slew limit per sample
                    float deltaL = voiceOutL - slewLastOutL;
                    float deltaR = voiceOutR - slewLastOutR;
                    const float maxStep = 0.02f;  // Slew max step (tunable)
                    
                    if (std::abs(deltaL) > maxStep) {
                        voiceOutL = slewLastOutL + (deltaL > 0.0f ? maxStep : -maxStep);
                    }
                    if (std::abs(deltaR) > maxStep) {
                        voiceOutR = slewLastOutR + (deltaR > 0.0f ? maxStep : -maxStep);
                    }
                    
                    slewLastOutL = voiceOutL;
                    slewLastOutR = voiceOutR;
                    
                    // Track per-voice pop detection (after slew limiting)
                    float deltaL_raw = std::abs(voiceOutL - lastVoiceSampleL);
                    float deltaR_raw = (numChannels > 1) ? std::abs(voiceOutR - lastVoiceSampleR) : deltaL_raw;
                    maxVoiceDelta = std::max(maxVoiceDelta, std::max(deltaL_raw, deltaR_raw));
                    
                    // Store last sample for next iteration
                    lastVoiceSampleL = voiceOutL;
                    lastVoiceSampleR = voiceOutR;
                    
                    for (int ch = 0; ch < numChannels; ++ch) {
                        if (output[ch] != nullptr) {
                            output[ch][i] += (ch == 0) ? voiceOutL : voiceOutR;
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
                
                // Read sample from current position (loop end region)
                float sampleEnd = 0.0f;
                if (index0 >= 0 && index0 < len && index1 >= 0 && index1 < len) {
                    float fraction = static_cast<float>(playhead - static_cast<double>(index0));
                    fraction = std::max(0.0f, std::min(1.0f, fraction));
                    float s0 = data[index0];
                    float s1 = data[index1];
                    sampleEnd = (s0 * (1.0f - fraction) + s1 * fraction) * sampleGain;
                }
                
                // If in loop crossfade region, also read from loop start and crossfade
                float sample = sampleEnd;
                if (loopCrossfadeActive && shouldLoop && loopStartPoint >= 0 && loopStartPoint < len) {
                    // Read sample from loop start position
                    // For reverse loops, calculate position differently
                    double loopStartPlayhead;
                    if (isReverseLoop) {
                        // Reverse: calculate position from loop start going backwards
                        loopStartPlayhead = static_cast<double>(loopStartPoint) - (static_cast<double>(loopEndPoint) - playhead);
                    } else {
                        // Forward: calculate position from loop start going forwards
                        loopStartPlayhead = static_cast<double>(loopStartPoint) + (playhead - static_cast<double>(loopEndPoint));
                    }
                    int loopIndex0 = static_cast<int>(loopStartPlayhead);
                    int loopIndex1 = loopIndex0 + 1;
                    
                    // Clamp loop indices to valid range (for reverse, start > end)
                    if (isReverseLoop) {
                        // Reverse loop: clamp between loopEndPoint and loopStartPoint
                        if (loopIndex0 > loopStartPoint) loopIndex0 = loopStartPoint;
                        if (loopIndex0 <= loopEndPoint) loopIndex0 = loopEndPoint + 1;
                        if (loopIndex1 > loopStartPoint) loopIndex1 = loopStartPoint;
                        if (loopIndex1 <= loopEndPoint) loopIndex1 = loopEndPoint + 1;
                    } else {
                        // Forward loop: clamp between loopStartPoint and loopEndPoint
                        if (loopIndex0 < loopStartPoint) loopIndex0 = loopStartPoint;
                        if (loopIndex0 >= loopEndPoint) loopIndex0 = loopEndPoint - 1;
                        if (loopIndex1 < loopStartPoint) loopIndex1 = loopStartPoint;
                        if (loopIndex1 >= loopEndPoint) loopIndex1 = loopEndPoint - 1;
                    }
                    
                    float sampleStart = 0.0f;
                    if (loopIndex0 >= 0 && loopIndex0 < len && loopIndex1 >= 0 && loopIndex1 < len) {
                        float loopFraction = static_cast<float>(loopStartPlayhead - static_cast<double>(loopIndex0));
                        loopFraction = std::max(0.0f, std::min(1.0f, loopFraction));
                        float ls0 = data[loopIndex0];
                        float ls1 = data[loopIndex1];
                        sampleStart = (ls0 * (1.0f - loopFraction) + ls1 * loopFraction) * sampleGain;
                    }
                    
                    // Crossfade: fade out from end, fade in from start
                    sample = sampleEnd * loopFadeOutGain + sampleStart * loopFadeInGain;
                }
                
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
                        // Map from releaseStartValue to 1.0 (smooth transition from current value)
                        // Always attack from 0.0 to 1.0 (releaseStartValue is always 0.0 on noteOn)
                        envelopeValue = attackCurve;
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
            
                // Read sample from current position (already computed above with crossfade if needed)
                // sample is already set from the crossfade code above
                
                // Update ramp gain smoothly (0 -> 1 over 128 samples)
                if (isRamping && rampSamplesRemaining > 0) {
                    rampGain += rampIncrement;
                    rampSamplesRemaining--;
                    if (rampSamplesRemaining <= 0) {
                        rampGain = targetGain;
                        isRamping = false;
                    }
                }
                
                // CRITICAL: Apply microRamp (rampGain) to prevent clicks on voice start
                // rampGain starts at 0.0 and ramps to 1.0 over 128 samples
                // Always compute output smoothly - gains will naturally be 0.0 when inactive
                // This eliminates hard discontinuities that cause clicks
                float finalEnvelope = testEnvelopeValue;
                float outputSample = sample * voiceGain * rampGain * baseAmplitude * finalEnvelope;
                
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
                
            // Apply per-voice slew limiter (click suppressor)
            float voiceOutL = outputSample;
            float voiceOutR = outputSample;
            
            // Slew limit per sample
            float deltaL = voiceOutL - slewLastOutL;
            float deltaR = voiceOutR - slewLastOutR;
            const float maxStep = 0.02f;  // Slew max step (tunable)
            
            if (std::abs(deltaL) > maxStep) {
                voiceOutL = slewLastOutL + (deltaL > 0.0f ? maxStep : -maxStep);
            }
            if (std::abs(deltaR) > maxStep) {
                voiceOutR = slewLastOutR + (deltaR > 0.0f ? maxStep : -maxStep);
            }
            
            slewLastOutL = voiceOutL;
            slewLastOutR = voiceOutR;
            
            // Track per-voice pop detection (after slew limiting)
            float deltaL_raw = std::abs(voiceOutL - lastVoiceSampleL);
            float deltaR_raw = (numChannels > 1) ? std::abs(voiceOutR - lastVoiceSampleR) : deltaL_raw;
            maxVoiceDelta = std::max(maxVoiceDelta, std::max(deltaL_raw, deltaR_raw));
            
            // Store last sample for next iteration
            lastVoiceSampleL = voiceOutL;
            lastVoiceSampleR = voiceOutR;
                
            for (int ch = 0; ch < numChannels; ++ch) {
                if (output[ch] != nullptr) {
                    output[ch][i] += (ch == 0) ? voiceOutL : voiceOutR;
                    }
                }
            }
            
            // Handle reverse loop: if loopStartPoint > loopEndPoint, play in reverse
            // Use the isReverseLoop variable already declared earlier in the function
            // Only reverse when we're in the reverse loop region (between loopEndPoint and loopStartPoint)
            if (isReverseLoop && inReverseLoopRegion && !inRelease) {
                // Reverse playback: decrement playhead
                playhead -= speed;
                
                // Check if we've gone past loopEndPoint and need to loop back
                if (playhead < static_cast<double>(loopEndPoint)) {
                    // Jump back to loopStartPoint to continue the reverse loop
                    playhead = static_cast<double>(loopStartPoint);
                }
            } else {
                // Forward playback: increment playhead
                playhead += speed;
                
                // Check forward loop AFTER playhead update to catch when it goes past loopEndPoint
                if (!isReverseLoop && shouldLoop && playhead >= static_cast<double>(loopEndPoint)) {
                    // Forward loop: jump back to loop start point
                    double overshoot = playhead - static_cast<double>(loopEndPoint);
                    playhead = static_cast<double>(loopStartPoint) + overshoot;
                }
            }
            
            // Guard against NaN/Inf
            if (!std::isfinite(playhead)) {
                playhead = static_cast<double>(startPoint);
            }
        }
        
        // Voice reuse safety - only deactivate after envelope ~0 AND ramp complete
        bool canDeactivate = (playhead >= static_cast<double>(endPoint) && inRelease && 
                             releaseCounter >= releaseSamples && 
                             (!isFadingOut || fadeOutCounter >= fadeOutSamples) && 
                             rampGain <= 0.001f);
        
        if (canDeactivate) {
            active = false;
        }
    }
}

void SamplerVoice::setGain(float g) {
    gain = clamp(g, 0.0f, 1.0f);
}


void SamplerVoice::setVoiceGain(float g) {
    voiceGain = clamp(g, 0.0f, 1.0f);
}

void SamplerVoice::setWarpEnabled(bool enabled) {
    warpEnabled = enabled;
    if (!enabled && warpProcessor) {
        // Reset processor when disabling
        warpProcessor.reset();
        warpPriming = false;
        warpCrossfadeActive = false;
        warpCrossfadePos = 0.0f;
    }
}

void SamplerVoice::setTimeRatio(double ratio) {
    timeRatio = std::max(0.25, std::min(4.0, ratio));
    if (warpProcessor && warpProcessor->isPrepared()) {
        warpProcessor->setTimeRatio(timeRatio);
    }
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

// Process with pop detection and slew limiting
// NOTE: Slew limiting is already integrated in process(), this is for future per-voice pop detection
void SamplerVoice::processWithPopDetection(float** output, int numChannels, int numSamples, double sampleRate,
                                           PopEventRingBuffer& popBuffer, uint64_t globalFrameCounter,
                                           float popThreshold, float slewMaxStep) {
    // For now, just call regular process (slew limiting is already integrated in process())
    // Full per-voice pop detection would require refactoring process() to expose intermediate values
    process(output, numChannels, numSamples, sampleRate);
}

} // namespace Core

