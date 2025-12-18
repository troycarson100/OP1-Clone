#include "SamplerEngine.h"
#include <algorithm>

namespace Core {

SamplerEngine::SamplerEngine()
    : currentSampleRate(44100.0)
    , currentBlockSize(512)
    , currentNumChannels(2)
    , targetGain(1.0f)
{
}

SamplerEngine::~SamplerEngine() {
}

void SamplerEngine::prepare(double sampleRate, int blockSize, int numChannels) {
    currentSampleRate = sampleRate;
    currentBlockSize = blockSize;
    currentNumChannels = numChannels;
    
    // Reset gain smoother with current block size
    gainSmoother.setTarget(targetGain, blockSize);
}

void SamplerEngine::setSample(const float* data, int length, double sourceSampleRate) {
    voiceManager.setSample(data, length, sourceSampleRate);
}

void SamplerEngine::setRootNote(int rootNote) {
    voiceManager.setRootNote(rootNote);
}

void SamplerEngine::setTimeWarpEnabled(bool enabled) {
    voiceManager.setWarpEnabled(enabled);
}

void SamplerEngine::handleMidi(const MidiEvent* events, int count) {
    for (int i = 0; i < count; ++i) {
        const MidiEvent& event = events[i];
        
        if (event.type == MidiEvent::NoteOn) {
            voiceManager.noteOn(event.note, event.velocity);
        } else if (event.type == MidiEvent::NoteOff) {
            voiceManager.noteOff(event.note);
        }
    }
}

void SamplerEngine::process(float** output, int numChannels, int numSamples) {
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
    voiceManager.process(output, numChannels, numSamples, currentSampleRate);
}

void SamplerEngine::setGain(float gain) {
    targetGain = std::max(0.0f, std::min(1.0f, gain));
    gainSmoother.setTarget(targetGain, currentBlockSize);
}

void SamplerEngine::getDebugInfo(int& actualInN, int& outN, int& primeRemaining, int& nonZeroCount) const {
    voiceManager.getDebugInfo(actualInN, outN, primeRemaining, nonZeroCount);
}

} // namespace Core

