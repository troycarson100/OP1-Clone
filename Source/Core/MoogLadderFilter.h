#pragma once

#include <memory>

namespace Core {

// Forward declaration for pimpl pattern
struct MoogLadderFilterImpl;

/**
 * Moog Ladder Filter - Pure C++ implementation
 * 
 * 4-pole (24dB/octave) low-pass filter with classic analog sound
 * Based on Huovilainen method - portable, no JUCE dependencies
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
     * Set cutoff frequency without recalculating coefficients immediately
     * Used for smooth parameter changes - coefficients should be updated separately
     */
    void setCutoffNoUpdate(float cutoffHz);
    
    /**
     * Set resonance (Q factor)
     * Range: 0.0-4.0 (typical Moog range)
     * Higher values = more resonance, can self-oscillate at high values
     */
    void setResonance(float resonance);
    
    /**
     * Set drive amount (saturation)
     * Range: 0.0-1.0+ (0.0 = clean, 1.0+ = saturated with character)
     * Drive is integrated into the filter for authentic Moog behavior
     */
    void setDrive(float drive);
    
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
    // Pimpl pattern to hide implementation details
    std::unique_ptr<MoogLadderFilterImpl> pimpl;
};

} // namespace Core

