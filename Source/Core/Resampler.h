#pragma once

namespace Core {

/**
 * High-quality resampler for pitch shifting
 * Portable C++ - no JUCE dependencies
 * 
 * Uses cubic interpolation (Catmull-Rom) for quality
 * Includes simple anti-aliasing lowpass for pitch up
 */
class Resampler {
public:
    Resampler();
    ~Resampler();
    
    /**
     * Prepare resampler
     * sampleRate: audio sample rate
     */
    void prepare(double sampleRate);
    
    /**
     * Set resampling ratio
     * ratio > 1.0: speed up / pitch up
     * ratio < 1.0: slow down / pitch down
     * ratio = 1.0: no change
     */
    void setRatio(float ratio);
    
    /**
     * Reset internal state
     */
    void reset();
    
    /**
     * Process audio
     * in: input samples
     * inCount: number of input samples available
     * out: output buffer
     * outCapacity: maximum output samples to produce
     * Returns: number of output samples actually produced
     */
    int process(const float* in, int inCount, float* out, int outCapacity);

private:
    static constexpr int INTERP_BUFFER_SIZE = 4;  // For cubic interpolation
    
    double sampleRate;
    float ratio;
    
    // Input ring buffer for interpolation
    float* inputBufferStorage;
    int inputBufferCapacity;
    int inputWritePos;
    int inputReadStart;  // Start position of valid data
    int inputSize;
    
    // Fractional read position (relative to inputReadStart)
    double readPos;
    
    // Lowpass filter state (for anti-aliasing when ratio > 1)
    float lowpassState;
    float lowpassAlpha;
    
    // Cubic interpolation helper
    static float cubicInterpolate(float y0, float y1, float y2, float y3, float frac);
    
    void allocateBuffers();
    void deallocateBuffers();
    void updateLowpass();
};

} // namespace Core

