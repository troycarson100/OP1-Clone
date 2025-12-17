#include "SamplerVoice.h"
#include <algorithm>
#include <cmath>

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
    , envelopeValue(0.0f)
    , attackSamples(0)
    , attackCounter(0)
    , releaseSamples(0)
    , releaseCounter(0)
    , inRelease(false)
    , currentSampleRate(44100.0)
{
}

SamplerVoice::~SamplerVoice() {
    // Sample data is owned by caller, we don't delete it
}

void SamplerVoice::setSample(const float* data, int length, double sourceRate) {
    sampleData = data;
    sampleLength = length;
    sourceSampleRate = sourceRate;
    playhead = 0.0;
    active = false;
    rootMidiNote = 60; // Default root note is C4
}

void SamplerVoice::noteOn(int note, float velocity) {
    currentNote = note;
    currentVelocity = clamp(velocity, 0.0f, 1.0f);
    playhead = 0.0;
    active = (sampleData != nullptr && sampleLength > 0);
    
    // Reset attack envelope (ramp from 0 to 1 over ~2ms to prevent clicks)
    envelopeValue = 0.0f;
    attackCounter = 0;
    inRelease = false;
    releaseCounter = 0;
    const double attackTimeMs = 2.0; // 2 milliseconds attack
    attackSamples = static_cast<int>(currentSampleRate * attackTimeMs / 1000.0);
    if (attackSamples < 1) attackSamples = 1;
    
    // Calculate release time (20ms for smooth fade-out, longer to prevent clicks)
    const double releaseTimeMs = 20.0; // 20 milliseconds release
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
    currentSampleRate = sampleRate;
    
    // Calculate pitch ratio based on MIDI note difference from root
    // Each semitone = 2^(1/12) = ~1.059463
    int semitones = currentNote - rootMidiNote;
    double pitchRatio = std::pow(2.0, semitones / 12.0);
    
    // Calculate playback speed (ratio of source to current sample rate, adjusted for pitch)
    double speed = (sourceSampleRate / sampleRate) * pitchRatio;
    
    // Base amplitude (velocity * gain)
    float baseAmplitude = currentVelocity * gain;
    
    // Starting envelope value for release (captured when release starts)
    float releaseStartValue = 1.0f;
    
    for (int i = 0; i < numSamples; ++i) {
        // Calculate the sample index we're about to read
        int index0 = static_cast<int>(playhead);
        
        // Stop EARLIER to prevent any possibility of looping
        // Stop when we would read from the second-to-last sample or later
        // This gives us margin and prevents any reads from the last sample
        // For lower notes (slower speed), this prevents the playhead from getting stuck
        if (!inRelease && index0 >= sampleLength - 2) {
            active = false;
            break;
        }
        
        // Also check playhead value directly for extra safety
        if (!inRelease && playhead >= static_cast<double>(sampleLength - 2)) {
            active = false;
            break;
        }
        
        // Check if sample has ended during release - continue release smoothly
        if (inRelease && index0 >= sampleLength - 1) {
            // Sample ended during release - continue release fade-out with silence
            if (releaseCounter < releaseSamples) {
                float releaseProgress = static_cast<float>(releaseCounter) / static_cast<float>(releaseSamples);
                envelopeValue = releaseStartValue * (1.0f - releaseProgress);
                releaseCounter++;
                
                // Output silence with release envelope
                float amplitude = baseAmplitude * envelopeValue;
                float outputSample = 0.0f * amplitude; // Silence
                
                for (int ch = 0; ch < numChannels; ++ch) {
                    if (output[ch] != nullptr) {
                        output[ch][i] += outputSample;
                    }
                }
                continue; // Skip sample reading, just continue release
            } else {
                // Release complete
                active = false;
                break;
            }
        }
        
        // Update envelope based on current phase
        if (inRelease) {
            // Release phase: fade from current value to 0
            if (releaseCounter == 0) {
                releaseStartValue = envelopeValue; // Capture current envelope value
            }
            
            if (releaseCounter < releaseSamples) {
                float releaseProgress = static_cast<float>(releaseCounter) / static_cast<float>(releaseSamples);
                envelopeValue = releaseStartValue * (1.0f - releaseProgress); // Fade from start to 0
                releaseCounter++;
            } else {
                // Release complete - voice is done
                envelopeValue = 0.0f;
                active = false;
                break;
            }
        } else {
            // Attack phase: ramp from 0 to 1
            if (attackCounter < attackSamples) {
                envelopeValue = static_cast<float>(attackCounter) / static_cast<float>(attackSamples);
                attackCounter++;
            } else {
                envelopeValue = 1.0f;
            }
        }
        
        // Apply envelope to amplitude
        float amplitude = baseAmplitude * envelopeValue;
        
        // Linear interpolation for fractional playhead
        // index0 was already calculated and validated above (index0 < sampleLength - 2)
        int index1 = index0 + 1;
        
        // Safety check: ensure both indices are within bounds
        if (index0 >= sampleLength || index1 >= sampleLength) {
            active = false;
            break;
        }
        
        float fraction = static_cast<float>(playhead - static_cast<double>(index0));
        
        float sample = sampleData[index0] * (1.0f - fraction) + 
                       sampleData[index1] * fraction;
        
        // Apply amplitude
        float outputSample = sample * amplitude;
        
        // Write to all output channels (mono to stereo duplication)
        for (int ch = 0; ch < numChannels; ++ch) {
            if (output[ch] != nullptr) {
                output[ch][i] += outputSample;
            }
        }
        
        // Advance playhead
        playhead += speed;
    }
}

void SamplerVoice::setGain(float g) {
    gain = clamp(g, 0.0f, 1.0f);
}

float SamplerVoice::clamp(float value, float min, float max) {
    return std::max(min, std::min(max, value));
}

} // namespace Core

