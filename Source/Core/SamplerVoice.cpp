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
    , currentVelocity(1.0f)
    , gain(1.0f)
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
}

void SamplerVoice::noteOn(int note, float velocity) {
    currentNote = note;
    currentVelocity = clamp(velocity, 0.0f, 1.0f);
    playhead = 0.0;
    active = (sampleData != nullptr && sampleLength > 0);
}

void SamplerVoice::noteOff() {
    active = false;
}

void SamplerVoice::process(float** output, int numChannels, int numSamples, double sampleRate) {
    if (!active || sampleData == nullptr || sampleLength == 0 || output == nullptr) {
        return;
    }
    
    // Calculate playback speed (ratio of source to current sample rate)
    double speed = sourceSampleRate / sampleRate;
    
    // Calculate output amplitude (velocity * gain)
    float amplitude = currentVelocity * gain;
    
    for (int i = 0; i < numSamples; ++i) {
        // Check if we've reached the end of the sample
        if (playhead >= static_cast<double>(sampleLength)) {
            active = false;
            break;
        }
        
        // Linear interpolation for fractional playhead
        int index0 = static_cast<int>(playhead);
        int index1 = std::min(index0 + 1, sampleLength - 1);
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

