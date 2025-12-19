#pragma once

namespace Core {

/**
 * Drive/Saturation Effect
 * Portable C++ - no JUCE dependencies
 * 
 * Soft saturation/clipping for analog-like distortion
 */
class DriveEffect {
public:
    DriveEffect();
    ~DriveEffect();
    
    /**
     * Set drive amount
     * Range: 0.0-2.0
     * 1.0 = no effect (pass through)
     * > 1.0 = saturation
     * < 1.0 = attenuation
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
     * Reset effect (no state to reset, but included for consistency)
     */
    void reset();

private:
    float drive;
    
    /**
     * Fast tanh approximation for saturation
     */
    float tanhApprox(float x) const;
};

} // namespace Core

