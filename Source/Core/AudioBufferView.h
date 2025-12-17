#pragma once

namespace Core {

// Lightweight view over audio buffers - portable, no JUCE
// Assumes non-interleaved buffers (separate channel pointers)
struct AudioBufferView {
    float** channels;
    int numChannels;
    int numSamples;
    
    AudioBufferView() : channels(nullptr), numChannels(0), numSamples(0) {}
    AudioBufferView(float** ch, int chans, int samples)
        : channels(ch), numChannels(chans), numSamples(samples) {}
    
    // Safe accessor
    float* getChannel(int channel) const {
        if (channel >= 0 && channel < numChannels && channels != nullptr) {
            return channels[channel];
        }
        return nullptr;
    }
    
    // Clear all channels
    void clear() {
        if (channels != nullptr) {
            for (int ch = 0; ch < numChannels; ++ch) {
                if (channels[ch] != nullptr) {
                    for (int i = 0; i < numSamples; ++i) {
                        channels[ch][i] = 0.0f;
                    }
                }
            }
        }
    }
};

} // namespace Core

