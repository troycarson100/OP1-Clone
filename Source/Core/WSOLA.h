#pragma once

#include "RingBufferF.h"

namespace Core {

/**
 * WSOLA (Waveform Similarity Overlap-Add) time-scale modification
 * Portable C++ - no JUCE dependencies
 * 
 * Time-stretches audio by finding similar waveform segments and overlapping them
 * Hardware-friendly time-domain method
 */
class WSOLA {
public:
    WSOLA();
    ~WSOLA();
    
    /**
     * Prepare WSOLA processor
     * sampleRate: audio sample rate
     */
    void prepare(double sampleRate);
    
    /**
     * Set time scale ratio
     * scale > 1.0: stretch (make longer)
     * scale < 1.0: compress (make shorter)
     * scale = 1.0: no change
     */
    void setTimeScale(float scale);
    
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
    static constexpr int FRAME_SIZE = 512;  // Smaller frame for lower latency
    static constexpr int OVERLAP = 128;     // 25% overlap
    static constexpr int ANALYSIS_HOP = FRAME_SIZE - OVERLAP;  // 384
    static constexpr int SEEK_RANGE = 128;  // Search range for best match
    
    double sampleRate;
    float timeScale;
    
    // Ring buffer for input (needs frameSize + seekRange)
    float* inputBufferStorage;
    RingBufferF inputBuffer;
    int inputBufferCapacity;
    
    // Output overlap-add accumulator
    float* olaBuffer;
    
    // Previous frame tail for correlation
    float* prevTail;
    
    // Temporary frame buffer
    float* tempFrame;
    
    // Window function
    float* window;
    
    // Current position in processing
    int basePos;
    
    // Correlation function (normalized dot product)
    static float correlation(const float* a, const float* b, int n);
    
    void allocateBuffers();
    void deallocateBuffers();
};

} // namespace Core

