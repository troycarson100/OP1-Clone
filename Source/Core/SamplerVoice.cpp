#include "SamplerVoice.h"
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
    , inRelease(false)
    , currentSampleRate(44100.0)
    , inputBuffer(nullptr)
    , outputBuffer(nullptr)
    , bufferSize(0)
    , sampleReadPos(0.0)
    , warpEnabled(true)
    , primeRemainingSamples(0)
    , loopEnabled(false)
    , loopStartPoint(0)
    , loopEndPoint(0)
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
    playhead = static_cast<double>(startPoint); // Start from start point
    sampleReadPos = static_cast<double>(startPoint); // Start from start point
    
    bool wasActive = active;
    active = (sampleData_ != nullptr && sampleData_->length > 0 && !sampleData_->mono.empty());
    
    // Calculate pitch in semitones and set it (include repitch offset)
    if (warpEnabled && timePitchProcessor && active) {
        int semitones = currentNote - rootMidiNote;
        float totalSemitones = static_cast<float>(semitones) + repitchSemitones;
        
        timePitchProcessor->setPitchSemitones(totalSemitones);
        // For constant duration: use timeRatio = 1.0 (pitch-only)
        // Reading sample at 1x speed + pitch shift = constant duration automatically
        timePitchProcessor->setTimeRatio(1.0f);
        
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
    
    // Reset ADSR envelope
    envelopeValue = 0.0f;
    attackCounter = 0;
    decayCounter = 0;
    inRelease = false;
    releaseCounter = 0;
    
    // Calculate sample counts from ADSR parameters
    attackSamples = static_cast<int>(currentSampleRate * attackTimeMs / 1000.0);
    if (attackSamples < 1) attackSamples = 1;
    
    decaySamples = static_cast<int>(currentSampleRate * decayTimeMs / 1000.0);
    if (decaySamples < 1) decaySamples = 1;
    
    releaseSamples = static_cast<int>(currentSampleRate * releaseTimeMs / 1000.0);
    if (releaseSamples < 1) releaseSamples = 1;
}

void SamplerVoice::noteOff(int note) {
    // Only start release if this voice is playing the specified note
    if (currentNote == note && active && !inRelease) {
        // Start release phase instead of immediately stopping
        inRelease = true;
        releaseCounter = 0;
        // Store current envelope value as starting point for release
        // (envelopeValue will fade from current value to 0)
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
        // Scale down significantly to prevent clipping when multiple voices play
        // With up to 16 voices, we need much more headroom
        const float amplitudeScale = 0.15f; // Reduce overall amplitude (was 0.25f, now 0.15f for better polyphony)
        float baseAmplitude = currentVelocity * gain * amplitudeScale;
    
    // Starting envelope value for release (captured when release starts)
    float releaseStartValue = 1.0f;
    
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
        
        // Handle latency priming: feed zeros until primeRemainingSamples is consumed
        int samplesToPrime = 0;
        if (primeRemainingSamples > 0) {
            samplesToPrime = std::min(primeRemainingSamples, numSamples);
            primeRemainingSamples -= samplesToPrime;
        }
        
        // Read from sample at original speed (1x) - this ensures constant duration
        std::fill(inputBuffer, inputBuffer + numSamples, 0.0f);
        
        // Fill input buffer: zeros for priming, then actual sample data
        for (int i = 0; i < numSamples; ++i) {
            if (i < samplesToPrime) {
                // Priming phase: feed zeros
                inputBuffer[i] = 0.0f;
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
                
                sampleReadPos += speed;
                
                // Guard against NaN/Inf
                if (!std::isfinite(sampleReadPos)) {
                    sampleReadPos = static_cast<double>(startPoint);
                }
                
                // Handle looping: if loop is enabled and we've reached the end, loop back to start
                if (loopEnabled && sampleReadPos >= static_cast<double>(loopEndPoint) && loopEndPoint > loopStartPoint) {
                    // Loop back to loop start point
                    sampleReadPos = static_cast<double>(loopStartPoint);
                }
            }
        }
        
        // If we've hit the end point (and not looping), start release
        if (sampleReadPos >= static_cast<double>(endPoint) && !inRelease && !loopEnabled) {
            inRelease = true;
            releaseCounter = 0;
            releaseStartValue = envelopeValue;
        }
        
        // Process through time-warp processor
        // For constant duration (timeRatio=1.0), pass numSamples input, request numSamples output
        std::fill(outputBuffer, outputBuffer + numSamples, 0.0f);
        int processedSamples = 0;
        
        if (timePitchProcessor) {
            // Pass numSamples input, request numSamples output (1:1 ratio for constant duration)
            // The processor may return less than numSamples if not enough accumulated yet
            processedSamples = timePitchProcessor->process(inputBuffer, numSamples, outputBuffer, numSamples);
            
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
        // Process all samples we got, even if less than numSamples
        // The rest of the buffer is already zero-filled
        // IMPORTANT: Even if processedSamples is 0 (still priming), keep voice active
        // and process the envelope so the voice doesn't get deactivated prematurely
        for (int i = 0; i < numSamples; ++i) {
            // Envelope
            if (inRelease) {
                if (releaseCounter == 0) {
                    releaseStartValue = envelopeValue;
                }
                if (releaseCounter < releaseSamples) {
                    float releaseProgress = static_cast<float>(releaseCounter) / static_cast<float>(releaseSamples);
                    envelopeValue = releaseStartValue * (1.0f - releaseProgress);
                    releaseCounter++;
                } else {
                    envelopeValue = 0.0f;
                    active = false;
                    break;
                }
            } else {
                // ADSR envelope phases
                if (attackCounter < attackSamples) {
                    // Attack phase: 0 to 1.0
                    envelopeValue = static_cast<float>(attackCounter) / static_cast<float>(attackSamples);
                    attackCounter++;
                } else if (decayCounter < decaySamples) {
                    // Decay phase: 1.0 to sustain level
                    float decayProgress = static_cast<float>(decayCounter) / static_cast<float>(decaySamples);
                    envelopeValue = 1.0f - (1.0f - sustainLevel) * decayProgress;
                    decayCounter++;
                } else {
                    // Sustain phase: hold at sustain level
                    envelopeValue = sustainLevel;
                }
            }
            
            float amplitude = baseAmplitude * envelopeValue;
            float sample = (i < processedSamples) ? outputBuffer[i] : 0.0f;
            float outputSample = sample * amplitude;
            
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
            // Bounds-safe sample reading
            if (playhead >= static_cast<double>(endPoint - 1)) {
                // At or past last valid index - use last sample value
                int lastIdx = std::max(startPoint, endPoint - 1);
                if (lastIdx >= 0 && lastIdx < len) {
                    float sample = data[lastIdx] * sampleGain;
                    
                    // Handle release if we hit the end
                    if (!inRelease) {
                        inRelease = true;
                        releaseCounter = 0;
                        releaseStartValue = envelopeValue;
                    }
                    
                    // Process envelope
                    if (releaseCounter < releaseSamples) {
                        float releaseProgress = static_cast<float>(releaseCounter) / static_cast<float>(releaseSamples);
                        envelopeValue = releaseStartValue * (1.0f - releaseProgress);
                        releaseCounter++;
                    } else {
                        envelopeValue = 0.0f;
                        active = false;
                        break;
                    }
                    
                    float amplitude = baseAmplitude * envelopeValue;
                    float outputSample = sample * amplitude;
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
                    if (releaseCounter == 0) {
                        releaseStartValue = envelopeValue;
                    }
                    if (releaseCounter < releaseSamples) {
                        float releaseProgress = static_cast<float>(releaseCounter) / static_cast<float>(releaseSamples);
                        envelopeValue = releaseStartValue * (1.0f - releaseProgress);
                        releaseCounter++;
                    } else {
                        envelopeValue = 0.0f;
                        active = false;
                        break;
                    }
                } else {
                    // ADSR envelope phases
                    if (attackCounter < attackSamples) {
                        envelopeValue = static_cast<float>(attackCounter) / static_cast<float>(attackSamples);
                        attackCounter++;
                    } else if (decayCounter < decaySamples) {
                        float decayProgress = static_cast<float>(decayCounter) / static_cast<float>(decaySamples);
                        envelopeValue = 1.0f - (1.0f - sustainLevel) * decayProgress;
                        decayCounter++;
                    } else {
                        envelopeValue = sustainLevel;
                    }
                }
                
                float amplitude = baseAmplitude * envelopeValue;
                
                // Final bounds check (defensive)
                float sample = 0.0f;
                if (index0 >= 0 && index0 < len && index1 >= 0 && index1 < len) {
                    float fraction = static_cast<float>(playhead - static_cast<double>(index0));
                    fraction = std::max(0.0f, std::min(1.0f, fraction));
                    float s0 = data[index0];
                    float s1 = data[index1];
                    sample = (s0 * (1.0f - fraction) + s1 * fraction) * sampleGain;
                }
                
                float outputSample = sample * amplitude;
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
        
        if (playhead >= static_cast<double>(endPoint) && inRelease && releaseCounter >= releaseSamples) {
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

} // namespace Core

