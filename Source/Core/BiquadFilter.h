#pragma once

namespace Core {

/**
 * Biquad low-pass filter
 * Portable C++ - no JUCE dependencies
 * 
 * Implements a standard biquad filter for low-pass filtering
 */
class BiquadFilter {
public:
    BiquadFilter();
    ~BiquadFilter();
    
    /**
     * Prepare filter with sample rate
     */
    void prepare(double sampleRate);
    
    /**
     * Set cutoff frequency (Hz)
     */
    void setCutoff(float cutoffHz);
    
    /**
     * Set resonance (Q factor, typically 0.1 to 10.0)
     */
    void setResonance(float resonance);
    
    /**
     * Process a single sample
     */
    float process(float input);
    
    /**
     * Process a buffer of samples
     */
    void processBlock(const float* input, float* output, int numSamples);
    
    /**
     * Reset filter state
     */
    void reset();

private:
    double sampleRate;
    float cutoffHz;
    float resonance;
    
    // Biquad coefficients
    float a0, a1, a2, b1, b2;
    
    // State variables (z^-1 delays)
    float x1, x2;  // Input history
    float y1, y2;  // Output history
    
    void updateCoefficients();
};

} // namespace Core





