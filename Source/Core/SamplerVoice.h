#pragma once

namespace Core {

// Single sampler voice - portable, no JUCE
// Plays back a loaded sample from memory
class SamplerVoice {
public:
    SamplerVoice();
    ~SamplerVoice();
    
    // Set the sample data (called from wrapper after loading)
    void setSample(const float* data, int length, double sourceSampleRate);
    
    // Trigger note on
    void noteOn(int note, float velocity);
    
    // Trigger note off
    void noteOff();
    
    // Check if voice is active
    bool isActive() const { return active; }
    
    // Process audio block - writes to output buffer
    // output: non-interleaved buffer [channel][sample]
    void process(float** output, int numChannels, int numSamples, double sampleRate);
    
    // Set gain (0.0 to 1.0)
    void setGain(float gain);
    
private:
    const float* sampleData;
    int sampleLength;
    double sourceSampleRate;
    
    double playhead;        // Current playback position (samples, can be fractional)
    bool active;
    int currentNote;
    float currentVelocity;
    float gain;
    
    // Helper: clamp value
    static float clamp(float value, float min, float max);
};

} // namespace Core

