#pragma once

namespace Core {

/**
 * Moog Ladder Filter (Stilson/Smith algorithm)
 * Portable C++ - no JUCE dependencies
 * 
 * 4-pole (24dB/octave) low-pass filter with classic analog sound
 * Based on the digital Moog ladder filter implementation
 */
class MoogLadderFilter {
public:
    MoogLadderFilter();
    ~MoogLadderFilter();
    
    /**
     * Prepare filter with sample rate
     */
    void prepare(double sampleRate);
    
    /**
     * Set cutoff frequency (Hz)
     * Range: 20-20000 Hz
     */
    void setCutoff(float cutoffHz);
    
    /**
     * Set resonance (Q factor)
     * Range: 0.0-4.0 (typical Moog range)
     * Higher values = more resonance, can self-oscillate at high values
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
     * Reset filter state (clears internal delays)
     */
    void reset();

private:
    double sampleRate;
    float cutoffHz;
    float resonance;
    
    // Filter state (4 stages for 4-pole filter)
    float stage[4];
    
    // Delay line for feedback
    float delay[4];
    
    // Pre-calculated coefficients
    float g;      // Cutoff coefficient
    float resonanceCoeff;  // Resonance coefficient
    
    // Flag to track if filter is prepared
    bool isPrepared;
    
    void updateCoefficients();
    float tanhApprox(float x) const;
};

} // namespace Core

