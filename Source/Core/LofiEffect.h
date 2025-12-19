#pragma once

namespace Core {

// Lofi effect - bitcrushing and sample rate reduction
// Portable C++, no JUCE dependencies
class LofiEffect {
public:
    LofiEffect();
    ~LofiEffect();
    
    // Prepare effect for processing
    void prepare(double sampleRate);
    
    // Set bit depth (1.0 = 1 bit, 16.0 = 16 bit, etc.)
    // Lower values = more bitcrushing
    void setBitDepth(float bits);
    
    // Set sample rate reduction factor (1.0 = no reduction, 0.1 = 10% of original rate)
    // Lower values = more lofi
    void setSampleRateReduction(float factor);
    
    // Process single sample
    float process(float input);
    
    // Process block of samples
    void processBlock(const float* input, float* output, int numSamples);
    
    // Reset internal state
    void reset();
    
private:
    double sampleRate;
    float bitDepth;
    float sampleRateReduction;
    
    // Sample rate reduction state
    float holdSample;
    float holdCounter;
    float holdInterval;
    
    // Calculate quantization step based on bit depth
    float calculateQuantizationStep() const;
    
    // Quantize sample to bit depth
    float quantize(float sample) const;
    
    // Update hold interval based on sample rate reduction
    void updateHoldInterval();
};

} // namespace Core

