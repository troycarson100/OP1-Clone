#include "SamplerVoice.h"
#include "SignalsmithTimePitch.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <chrono>

namespace Core {

SamplerVoice::SamplerVoice()
    : sampleData(nullptr)
    , sampleLength(0)
    , sourceSampleRate(44100.0)
    , playhead(0.0)
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

void SamplerVoice::setSample(const float* data, int length, double sourceRate) {
    sampleData = data;
    sampleLength = length;
    sourceSampleRate = sourceRate;
    playhead = 0.0;
    sampleReadPos = 0.0;
    active = false;
    rootMidiNote = 60; // Default root note is C4
    
    // Initialize start/end points to full sample
    startPoint = 0;
    endPoint = length;
    
    // TimePitchProcessor will be prepared when we know the output sample rate
    // (done in process() or prepareToPlay equivalent)
}

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
    active = (sampleData != nullptr && sampleLength > 0);
    
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
    if (!active || sampleData == nullptr || sampleLength == 0 || output == nullptr) {
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
    // Scale down to prevent clipping when multiple voices play or velocity is high
    const float amplitudeScale = 0.5f; // Reduce overall amplitude
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
                if (sampleReadPos >= static_cast<double>(endPoint)) {
                    inputBuffer[i] = 0.0f;
                } else if (sampleReadPos < static_cast<double>(startPoint)) {
                    inputBuffer[i] = 0.0f;
                } else {
                    int idx0 = static_cast<int>(sampleReadPos);
                    int idx1 = idx0 + 1;
                    if (idx0 < startPoint) idx0 = startPoint;
                    if (idx0 >= endPoint) idx0 = endPoint - 1;
                    if (idx1 < startPoint) idx1 = startPoint;
                    if (idx1 >= endPoint) idx1 = endPoint - 1;
                    float frac = static_cast<float>(sampleReadPos - static_cast<double>(idx0));
                    frac = std::max(0.0f, std::min(1.0f, frac));
                    float s0 = sampleData[idx0];
                    float s1 = sampleData[idx1];
                    inputBuffer[i] = (s0 * (1.0f - frac) + s1 * frac) * sampleGain; // Apply sample gain
                }
                sampleReadPos += speed;
            }
        }
        
        // If we've hit the end point, start release
        if (sampleReadPos >= static_cast<double>(endPoint) && !inRelease) {
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
        
        for (int i = 0; i < numSamples; ++i) {
            int index0 = static_cast<int>(playhead);
            
            if (index0 >= endPoint - 1 || index0 < startPoint) {
                if (!inRelease && index0 >= endPoint - 1) {
                    inRelease = true;
                    releaseCounter = 0;
                    releaseStartValue = envelopeValue;
                }
            }
            
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
            
            float sample = 0.0f;
            if (index0 >= startPoint && index0 < endPoint - 1) {
                int index1 = index0 + 1;
                if (index1 >= endPoint) index1 = endPoint - 1;
                float fraction = static_cast<float>(playhead - static_cast<double>(index0));
                fraction = std::max(0.0f, std::min(1.0f, fraction));
                float s0 = sampleData[index0];
                float s1 = sampleData[index1];
                sample = (s0 * (1.0f - fraction) + s1 * fraction) * sampleGain; // Apply sample gain
            }
            
            float outputSample = sample * amplitude;
            for (int ch = 0; ch < numChannels; ++ch) {
                if (output[ch] != nullptr) {
                    output[ch][i] += outputSample;
                }
            }
            
            playhead += speed;
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

